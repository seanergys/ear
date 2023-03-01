/*
*
* This program is part of the EAR software.
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* Copyright © 2017-present BSC-Lenovo
* BSC Contact   mailto:ear-support@bsc.es
* Lenovo contact  mailto:hpchelp@lenovo.com
*
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

//#define SHOW_DEBUGS 1

#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <common/system/time.h>
#include <common/output/debug.h>
#include <common/utils/stress.h>
#include <common/hardware/cpuid.h>
#include <common/hardware/bithack.h>
#include <common/hardware/defines.h>
#include <common/environment_common.h>
#include <management/cpufreq/cpufreq_base.h>
#include <management/cpufreq/archs/amd17.h>
#include <metrics/common/msr.h>

static basefreq_t static_base;

static int basefreq_intel_haswell(topology_t *tp, basefreq_t *base)
{
    if (tp->vendor != VENDOR_INTEL || tp->model < MODEL_HASWELL_X) {
        return 0; 
    }
    cpuid_regs_t regs;
    // Getting base frequency (CPUID 16H)
    CPUID(regs, 0x16, 0);
    // Computing base frequency in KHz
    base->frequency = (regs.eax & 0x0000FFFF) * 1000LU;
    // Getting boost status (CPUID 6H)
    CPUID(regs, 0x06, 0);
    //
    base->boost_enabled = (ulong) cpuid_getbits(regs.eax, 1, 1);
    debug("detected frequency %llu", base->frequency);
    return 1;
}

static int basefreq_amd_zen(topology_t *tp, basefreq_t *base)
{
    ullong aux;
    ullong did;
    ullong fid;

    // This method is requires permissions to, at least, read the MSR files.
    // If doesn't have enough privileges or the MSR driver is not present,
    // it will fail. In the future, maybe we can build an MSR class to avoid
    // the use of MSR driver.
    if (tp->vendor != VENDOR_AMD || tp->model < FAMILY_ZEN) {
        return 0; 
    }
    if (state_fail(msr_open(0, MSR_RD))) {
        return 0;
    }
    if (state_fail(msr_read(0, (char *) &aux, sizeof(ullong), REG_P0))) {
        return 0;
    }
    if (state_fail(msr_read(0, (char *) &aux, sizeof(ullong), REG_HWCONF))) {
        return 0;
    }
    fid = getbits64(aux,  7, 0);
    did = getbits64(aux, 13, 8);
    if (did > 0) {
        base->frequency = (ulong) (((fid * 200LLU) / did) * 1000);
    }
    base->frequency = (uint) !getbits64(aux, 25, 25);
    debug("detected frequency %llu", base->frequency);
    return 1;
}

static ullong file_read_value(int fd)
{
    char buffer[8];
    int i = 0;
    char c;

    while((read(fd, &c, sizeof(char)) > 0) && ((c >= '0') && (c <= '9'))) {
        buffer[i] = c;
        i++;
    }
    buffer[i] = '\0';
    return (ullong) atoi(buffer);
}

static int basefreq_file_saf(topology_t *tp, basefreq_t *base)
{
    ullong f0, f1;
    int fd;

    if ((fd = open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies", O_RDONLY)) < 0) {
        return 0;
    }
    f0 = file_read_value(fd);
    f1 = file_read_value(fd);
    close(fd);
    // If the difference is 1 KHz, base/nominal is f1
    base->frequency = ((f0 - f1) == 1000) ? f1:f0;
    debug("detected frequency %llu", base->frequency);
    return (base->frequency != 0);
}

static int basefreq_file_cmf(topology_t *tp, basefreq_t *base)
{
    int fd;
    if ((fd = open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", O_RDONLY)) < 0) {
        return 0;
    }
    base->frequency = file_read_value(fd);
    close(fd);
    debug("detected frequency %llu", base->frequency);
    return (base->frequency != 0);
}

static int basefreq_file_cpuinfo(topology_t *tp, basefreq_t *base)
{
    char *line = NULL;
    char *col  = NULL; //column
    char *aux  = NULL;
    size_t len = 0;
    FILE *fd   = NULL;
    
    if ((fd = fopen("/proc/cpuinfo", "r")) == NULL) {
        return 0;
    }
    while(getline(&line, &len, fd) != -1) {
        if (strncmp(line,"model name", 10) == 0){
            if ((col = strchr(line, '@')) != NULL) {
                col++;
                while (*col == ' ') col++;
                aux = col;
                while (*col != 'G') col++;
                *col = '\0';
                // Converted to KHz
                base->frequency = atof(aux)*1000000;
            }
        }
    }
    fclose(fd);
    free(line);
    debug("detected frequency %llu", base->frequency);
    return (base->frequency != 0);
}

static int basefreq_environment(topology_t *tp, basefreq_t *base)
{
    char *freq          = ear_getenv("EAR_BASE_FREQ");
    char *boost         = ear_getenv("EAR_BASE_BOOST");
    base->frequency     = (freq  != NULL) ? (ullong) atoi(freq ) : 0;
    base->boost_enabled = (boost != NULL) ? (ullong) atoi(boost) : 0;
    debug("detected frequency %llu", base->frequency);
    return (base->frequency != 0);
}

static int basefreq_spec_valid(basefreq_t *base) 
{
    // Finally, if is under 5 GHz and over 1 GHz we assume that
    // the speculation could be valid.
    if (base->frequency > 5000000 || base->frequency < 1000000) {
        base->frequency = 0;
    }
    debug("detected frequency %llu (speculative)", base->frequency);
    return (base->frequency != 0);
}

static int basefreq_spec_tsc(topology_t *tp, basefreq_t *base)
{
    ullong l1, h1, l2, h2, khz;
    double chz;
    // The ARM TSC doesn't work at base frequency. CNTFRQ_EL0 has the
    // TSC (CNTVCT_EL0 or CNTPCT_EL0) frequency. From ARMv8.6 is 1GHz.
    if (tp->vendor == VENDOR_ARM) {
        return 0;
    }
    #if __ARCH_X86
    asm volatile("rdtsc" : "=a"(l1), "=d"(h1));
    #endif
    stress_spin(1000);
    #if __ARCH_X86
    asm volatile("rdtsc" : "=a"(l2), "=d"(h2));
    #endif
    h1 = h1 << 32 | l1;
    h2 = h2 << 32 | l2;
    // CHz is an invented unit. Is thousands of MHz and is
    // used to round the counting cycles.
    chz = ((double) (h2-h1)) / 100000000.0;
    debug("Cycles TSC: %llu, CHz: %0.2lf", h2-h1, chz);
    // Passing to frequency and checking 
    base->frequency = ((ullong) round(chz)) * 100000LU;
    return basefreq_spec_valid(base);
}

static int basefreq_spec_asm(topology_t *tp, basefreq_t *base)
{
    timestamp_t ts1;
    double ns, chz;
    // Around 4.0 GHz of ASM instructions. This method is turbo boost
    // sensitive, so can not find the correct frequency in these nodes
    // whose boost technology is enabled. But is better than nothing.
    #define DO_40(X) \
        X X X X X X X X X X X X X X X X X X X X \
        X X X X X X X X X X X X X X X X X X X X
    timestamp_get(&ts1);
    #if __ARCH_ARM
    // Getting the timestamp we can approximate the CPU frequency.
    // The ADD, CMP and BNE are not considered because the CPU seems
    // to perform a speculative execution, not consuming even one cycle.
    asm volatile
    (
        "mov x0, #0;"
        "mov x1, #10000;"
        "mov x2, #10000;"
        "mul x1, x1, x2;"
        "Loop:"
        DO_40("mov x8, x8;") // 40 cycles
        "add x0, x0, #1;"    // 1 cycle
        "cmp x0, x1;"        // 1 cycle
        "bne Loop;"          // 2 cycles
    );
    #else
    // On the contrary, MOV and NOP seems to not work well on X86,
    // so we use the traditional ADD instruction.
    asm volatile (
        "mov $0, %eax;"
        "mov $10000, %ebx;"
        "imul %ebx, %ebx;"
        "Loop:"
        "add $1, %eax;"        // 1 cycle
        DO_40("add $1, %ecx;") // 40 cycles
        "cmp %ebx, %eax;"      // 1 cycle
        "jl Loop;"             // X cycles
    );
    #endif
    ns  = (double) timestamp_diffnow(&ts1, TIME_NSECS);
    chz = (4100000000.0 / ns) * 10.0; // Hundreds of MHz
    debug("Time nS: %0.2lf, CHz: %0.2lf", ns, chz);
    // Passing to frequency and checking 
    base->frequency = ((ullong) round(chz)) * 100000LLU;
    return basefreq_spec_valid(base);
}

static int basefreq_spec_hardcode(topology_t *tp, basefreq_t *base)
{
    base->frequency = 2000000; // 2GHz
    return basefreq_spec_valid(base);
}

void basefreq_init_static(topology_t *tp, basefreq_t *base)
{
    if (basefreq_intel_haswell(tp, base)) return;
    if (basefreq_amd_zen(tp, base)) return;
    if (basefreq_file_saf(tp, base)) return;
    if (basefreq_file_cpuinfo(tp, base)) return;
    if (basefreq_environment(tp, base)) return;
    if (basefreq_spec_tsc(tp, base)) return;
    if (basefreq_spec_asm(tp, base)) return;
    if (basefreq_spec_hardcode(tp, base)) return;
}

void basefreq_init(topology_t *tp, basefreq_t *base)
{
    if (static_base.frequency == 0) {
        basefreq_init_static(tp, &static_base);
    }
    memcpy(base, &static_base, sizeof(basefreq_t));
}
