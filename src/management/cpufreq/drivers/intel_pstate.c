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

#define SHOW_DEBUGS 1

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <common/output/debug.h>
#include <management/cpufreq/drivers/intel_pstate.h>

#define PATH_SMF "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq" // line break
#define PATH_CMF "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq"  // Just read

static uint    cpu_count;
static ullong *current_list;
static uint    avail_list_count;
static ullong  avail_list[128]; // From 10 GHz to 1 GHz there are 90 items, enough
static ullong  freq_max; // KHz
static int    *fds_smf; // FD scaling max freq

static state_t read_word(int fd, char *word, int reset_position)
{
    int i = 0;
    char c;
    if (reset_position) {
        if(lseek(fd, 0, SEEK_SET) < 0) {
            return_print(EAR_ERROR, "while trying to read cpufreq file (%s)", strerror(errno));
        }
    }
    // Does not read ' ' nor '\n'
    while((read(fd, &c, sizeof(char)) > 0) && !((c == (char) 10) || (c == (char) 32))) {
        word[i] = c;
        i++;
    }
    word[i] = '\0';
//    debug("read the word '%s'", word);
    if (i == 0) {
        return_print(EAR_ERROR, "while reading '%s' in cpufreq file (%s)",
                     word, strerror(errno));
    }
    return EAR_SUCCESS;
}

static state_t write_word(int fd, char *word, int s, int line_break)
{
    int i, r, p;
    // Adding a line break
    if (line_break) {
        s += 1;
        word[s-1] = '\n';
        word[s] = '\0';
    }
    for (i = 0, r = 1, p = s; i < s && r > 0;) {
        r = pwrite(fd, (void*) &word[i], p, i);
        i = i + r;
        p = s - i;
    }
    if (r == -1) {
        return_print(EAR_ERROR, "while writing '%s' in cpufreq file (%s)",
                     word, strerror(errno));
    }
    return EAR_SUCCESS;
}

void mgt_intel_pstate_linux_load(topology_t *tp, mgt_ps_driver_ops_t *ops)
{
    char data[SZ_PATH];
    int set_frequency = 1;
    int set_governor  = 1;
    int fd_cmf;
    state_t s;
    int i;

    // Opening cpuinfo_max_freq
    if ((fd_cmf = open(PATH_CMF, O_RDONLY)) < 0) {
        debug("Opening '%s' failed: %d (%s)", PATH_CMF, fd_cmf, strerror(errno));
        return_msg(, strerror(errno));
    }
    // Reading the content of cpuinfo_max_freq
    if (state_fail(read_word(fd_cmf, data, 0))) {
        debug("Opening '%s' failed: %d (%s)", PATH_CMF, fd_cmf, strerror(errno));
        return_msg(, strerror(errno));
    }
    // Saving some required data
    cpu_count    = tp->cpu_count;
    current_list = calloc(cpu_count, sizeof(ullong));
    fds_smf      = calloc(cpu_count, sizeof(int));
    freq_max     = (ullong) atoi(data);
    // Saving scaling_max_freq file descriptors
    for (i = 0; i < cpu_count; ++i) {
        sprintf(data, PATH_SMF, i);
        if ((fds_smf[i] = open(data, O_RDWR)) < 0) {
            debug("Opening '%s' failed: %d (%s)", data, fds_smf[i], strerror(errno));
            return_msg(, strerror(errno));
        }
    }
	// Driver references
    apis_put(ops->init              , mgt_intel_pstate_linux_init                            );
    apis_put(ops->dispose           , mgt_intel_pstate_linux_dispose                         );
    apis_put(ops->get_available_list, mgt_intel_pstate_linux_get_available_list              );
    apis_put(ops->get_current_list  , mgt_intel_pstate_linux_get_current_list                );
    apis_put(ops->get_boost         , mgt_intel_pstate_linux_get_boost                       );
    apis_put(ops->get_governor      , mgt_intel_pstate_linux_governor_get                    );
    apis_put(ops->get_governor_list , mgt_intel_pstate_linux_governor_get_list               );
    apis_pin(ops->set_current_list  , mgt_intel_pstate_linux_set_current_list , set_frequency);
    apis_pin(ops->set_current       , mgt_intel_pstate_linux_set_current      , set_frequency);
    apis_pin(ops->set_governor      , mgt_intel_pstate_linux_governor_set     , set_governor );
    apis_pin(ops->set_governor_mask , mgt_intel_pstate_linux_governor_set_mask, set_governor );
    apis_pin(ops->set_governor_list , mgt_intel_pstate_linux_governor_set_list, set_governor );
}

state_t mgt_intel_pstate_linux_init(ctx_t *c)
{
    char data[64];
    ullong i = 0;

    debug("cpuinfo_max_freq: %llu", freq_max)
    // While greater than 1GHz
    while((avail_list[i] = freq_max - (100000LLU*i)) >= 1000000) {
        debug("avail_list%llu: %llu", i, avail_list[i]);
        ++i;
    }
    avail_list_count = i;
	return EAR_SUCCESS;
}

state_t mgt_intel_pstate_linux_dispose(ctx_t *c)
{
    return EAR_SUCCESS;
}

/* Getters */
state_t mgt_intel_pstate_linux_get_available_list(ctx_t *c, const ullong **list, uint *list_count)
{
    static ullong shared_list[128];
    memcpy(shared_list, avail_list, sizeof(ullong)*128);
    if (list != NULL) {
        *list = (const ullong *) shared_list;
    }
    if (list_count != NULL) {
        *list_count = avail_list_count;
    }
	return EAR_SUCCESS;
}

state_t mgt_intel_pstate_linux_get_current_list(ctx_t *c, const ullong **p)
{
    state_t s1 = EAR_SUCCESS;
    state_t s2 = EAR_SUCCESS;
    char data[64];
    int cpu;

    for (cpu = 0; cpu < cpu_count; ++cpu) {
        if (state_fail(s1 = read_word(fds_smf[cpu], data, 1))) {
            avail_list[cpu] = 0LLU;
            s2 = s1;
        } else {
            avail_list[cpu] = (ullong) atoi(data);
        }
        debug("avail_list%d: %llu", cpu, avail_list[cpu]);
    }
    *p = avail_list;
    return s2;
}

state_t mgt_intel_pstate_linux_get_boost(ctx_t *c, uint *boost_enabled)
{
    *boost_enabled = 0;
	return EAR_SUCCESS;
}

/** Setters */
state_t mgt_intel_pstate_linux_set_current_list(ctx_t *c, uint *freqs_index)
{
    char data[SZ_NAME_SHORT];
    state_t s2 = EAR_SUCCESS;
    state_t s1 = EAR_SUCCESS;
    int cpu;

    for (cpu = 0; cpu < cpu_count; ++cpu) {
        if (freqs_index[cpu] == ps_nothing) {
            continue;
        }
        if (freqs_index[cpu] > avail_list_count) {
            freqs_index[cpu] = avail_list_count-1;
        }
        sprintf(data, "set_list%d: %llu", cpu, avail_list[freqs_index[cpu]]);
        s1 = write_word(fds_smf[cpu], data, strlen(data), 1);
        if (state_fail(s1)) {
            s2 = s1;
        }
    }
    return s2;
}

state_t mgt_intel_pstate_linux_set_current(ctx_t *c, uint freq_index, int cpu)
{
#if 0
    char data[SZ_NAME_SHORT];
    // Correcting invalid values
    if (freq_index > avail_list_count) {
        freq_index = avail_list_count-1;
    }
    // If is one P_STATE for all CPUs
    if (cpu == all_cpus) {
        // Converting frequency to text
        sprintf(data, "%llu", avail_list[freq_index]);
        return write_multi(fds_smf, data, 1);
    }
    // If it is for a specified CPU
    if (cpu >= 0 && cpu < cpu_count) {
        sprintf(data, "%llu", avail_list[freq_index]);
        debug("writing a word '%s'", data);
        return write_word(fds_smf[cpu], data, strlen(data), 1);
    }
    return_msg(EAR_ERROR, Generr.cpu_invalid);
#endif
}

state_t mgt_intel_pstate_linux_governor_get(ctx_t *c, uint *governor)
{
    return EAR_SUCCESS;
}

state_t mgt_intel_pstate_linux_governor_get_list(ctx_t *c, uint *governors)
{
    return EAR_SUCCESS;
}

state_t mgt_intel_pstate_linux_governor_set(ctx_t *c, uint governor)
{
    return EAR_SUCCESS;
}

state_t mgt_intel_pstate_linux_governor_set_mask(ctx_t *c, uint governor, cpu_set_t mask)
{
    return EAR_SUCCESS;
}

state_t mgt_intel_pstate_linux_governor_set_list(ctx_t *c, uint *governors)
{
    return EAR_SUCCESS;
}

int main(int argc, char *argv[])
{
    const ullong *freqs;
    mgt_ps_driver_ops_t ops;
    topology_t tp;
    topology_init(&tp);
    mgt_intel_pstate_linux_load(&tp, &ops);
    mgt_intel_pstate_linux_init(no_ctx);
    mgt_intel_pstate_linux_get_current_list(no_ctx, &freqs);
    ullong freq_base;
    cpufreq_get_base(&tp, &freq_base);
    printf("FREQ BASE %llu\n", freq_base);
    return 0;
}