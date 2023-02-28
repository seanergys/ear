#define SHOW_DEBUGS 1

#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <common/system/time.h>
#include <common/output/debug.h>
#include <common/hardware/cpuid.h>
#include <common/hardware/bithack.h>
#include <common/environment_common.h>
#include <metrics/common/msr.h>
#include <management/cpufreq/cpufreq_base.h>
#include <management/cpufreq/archs/amd17.h>

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
    // If the difference is 1 KHz, base/nominal is f1
    base->frequency = ((f0 - f1) == 1000) ? f1:f0;
    close(fd);
    debug("detected frequency %llu", base->frequency);
    return 1;
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
    return 1;
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

static int basefreq_spec(topology_t *tp, basefreq_t *base)
{
    if (tp->vendor != VENDOR_ARM) {
        return 0; 
    }
    base->frequency = 2000000;
    #if __ARCH_ARM
    // 40 movs * 10.000 * 10.000 = 4.0 GHz
    // Getting the timestamp we can approximate the CPU frequency.
    // The ADD, CMP and BNE are not considered because the CPU seems
    // to perform a speculative execution, not consuming even one cycle.
    timestamp_t ts1;
    #define DO_40(X) \
        X X X X X X X X X X X X X X X X X X X X \
        X X X X X X X X X X X X X X X X X X X X
    timestamp_get(&ts1);
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
    double  ns = (double) timestamp_diffnow(&ts1, TIME_NSECS);
    double chz = (4100000000.0 / ns) * 10.0; // Passing 3.0 GHz to 30.0 CHz (invent)
    ullong khz = ((ullong) round(chz)) * 100000LU;
    debug("ns : %0.2lf", ns);
    debug("hz : %0.2lf", 4100000000.0);
    debug("div: %0.6lf", 4100000000.0 / ns);
    // Finally, if is under 4 GHz and over 1 GHz the speculation could
    // be valid. If not, we set 2 GHz which is a common frequency.
    if (khz > 4000000 || khz < 1000000) {
        khz = 1000000;
    }
    base->frequency = khz;
    #endif
    debug("detected frequency %llu", base->frequency);
    return (base->frequency != 0);
}

void basefreq_init(topology_t *tp, basefreq_t *base)
{
    if (basefreq_intel_haswell(tp, base)) return;
    if (basefreq_amd_zen(tp, base)) return;
    if (basefreq_file_saf(tp, base)) return;
    if (basefreq_file_cpuinfo(tp, base)) return;
    if (basefreq_environment(tp, base)) return;
    if (basefreq_spec(tp, base)) return;
}
