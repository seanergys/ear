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

#define _GNU_SOURCE
#include <sched.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <common/output/debug.h>
#include <common/utils/keeper.h>
#include <management/cpufreq/drivers/intel_pstate.h>

#define PATH_SMF   "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq" // line break
#define PATH_SGV   "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor" // line break
#define PATH_CMF0  "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq"  // Just read
#define PATH_SMF0  "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq"
#define PATH_SGV0  "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"

static uint    cpu_count;
static ullong *current_list;
static uint    avail_list_count;
static ullong  avail_list[128]; // From 10 GHz to 1 GHz there are 90 items, enough
static uint    boost_enabled;
static ullong  freq_max; // KHz
static ullong  freq_max0; // Initial max freq
static ullong  freq_base; // KHz
static ullong  freq_batman; // MAX to BASE if boost enabled
static uint    governor0;
static int    *fds_smf; // FD scaling max freq
static int    *fds_sgv;

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

// Writes a word in a list of file descriptors.
static state_t write_multi(int *fds, char *word, int line_break)
{
    state_t s = EAR_SUCCESS;
    int len = strlen(word);
    int cpu;
    debug("Multiple writing a word '%s'", word);
    // Adding a line break
    if (line_break) {
        word[len]   = '\n';
        word[len+1] = '0';
        len++;
    }
    for (cpu = 0; cpu < cpu_count && state_ok(s); ++cpu) {
        s = write_word(fds[cpu], word, len, 0);
    }
    if (state_fail(s)) {
        return s;
    }
    return EAR_SUCCESS;
}

int filemagic_can_write(char *path)
{
    return access(path, W_OK) == 0;
}

int filemagic_can_read(char *path)
{
    return access(path, R_OK) == 0;
}

void mgt_intel_pstate_load(topology_t *tp, mgt_ps_driver_ops_t *ops)
{
    char buffer[SZ_PATH];
    int set_frequency = 1;
    int set_governor  = 1;
    int fd_cmf;
    int perms;
    int i;

    if (!filemagic_can_read(PATH_CMF0)) return;
    if (!filemagic_can_read(PATH_SGV0)) return;
    if (!filemagic_can_read(PATH_SMF0)) return;
    // Opening cpuinfo_max_freq
    if ((fd_cmf = open(PATH_CMF0, O_RDONLY)) < 0) {
        debug("Opening '%s' failed: %d (%s)", PATH_CMF0, fd_cmf, strerror(errno));
        return_msg(, strerror(errno));
    }
    // Reading the content of cpuinfo_max_freq
    if (state_fail(read_word(fd_cmf, buffer, 0))) {
        debug("Opening '%s' failed: %d (%s)", PATH_CMF0, fd_cmf, strerror(errno));
        return_msg(, strerror(errno));
    }
    // Saving some required data
    cpu_count     = tp->cpu_count;
    boost_enabled = tp->boost_enabled;
    freq_base     = (ullong) tp->base_freq;
    freq_max      = (ullong) atoi(buffer);
    freq_max0     = freq_max;
    current_list  = calloc(cpu_count, sizeof(ullong));
    fds_smf       = calloc(cpu_count, sizeof(int));
    fds_sgv       = calloc(cpu_count, sizeof(int));
    // Saving scaling_max_freq file descriptors
    for (i = 0, perms = O_RDWR; i < cpu_count; ++i) {
        sprintf(buffer, PATH_SMF, i);
        if ((fds_smf[i] = open(buffer, perms)) < 0) {
            debug("Failed while opening '%s' failed: %s", buffer, strerror(errno));
            set_frequency = 0;
            perms = O_RDONLY;
            i = -1;
        }
    }
    // Saving governor file descriptors
    for (i = 0, perms = O_RDWR; i < cpu_count; ++i) {
        sprintf(buffer, PATH_SGV, i);
        if ((fds_sgv[i] = open(buffer, perms)) < 0) {
            debug("Failed while opening '%s' failed: %s", buffer, strerror(errno));
            set_governor = 0;
            perms = O_RDONLY;
            i = -1;
        }
    }
    mgt_intel_pstate_governor_get(NULL, &governor0);
    // Saved initial values
    keeper_macro(uint32, "IntelPstateDefaultGovernor", governor0);
    keeper_macro(uint64, "IntelPstateDefaultFrequency", freq_max0);
    debug("IntelPstateDefaultGovernor: %u", governor0);
    debug("IntelPstateDefaultFrequency: %llu", freq_max0);
	// Driver references
    apis_put(ops->init                 , mgt_intel_pstate_init                                );
    apis_put(ops->dispose              , mgt_intel_pstate_dispose                             );
    apis_put(ops->get_available_list   , mgt_intel_pstate_get_available_list                  );
    apis_put(ops->get_current_list     , mgt_intel_pstate_get_current_list                    );
    apis_put(ops->get_boost            , mgt_intel_pstate_get_boost                           );
    apis_put(ops->get_governor         , mgt_intel_pstate_governor_get                        );
    apis_put(ops->get_governor_list    , mgt_intel_pstate_governor_get_list                   );
    apis_put(ops->is_governor_available, mgt_intel_pstate_governor_is_available               );
    apis_pin(ops->set_current_list     , mgt_intel_pstate_set_current_list     , set_frequency);
    apis_pin(ops->set_current          , mgt_intel_pstate_set_current          , set_frequency);
    apis_pin(ops->set_governor         , mgt_intel_pstate_governor_set         , set_governor );
    apis_pin(ops->set_governor_mask    , mgt_intel_pstate_governor_set_mask    , set_governor );
    apis_pin(ops->set_governor_list    , mgt_intel_pstate_governor_set_list    , set_governor );

    debug("Loaded INTEL_PSTATE");
}

state_t mgt_intel_pstate_init(ctx_t *c)
{
    ullong m = 0;
    uint i = 0;

    debug("cpuinfo_max_freq: %llu", freq_max);
    debug("topo->base_freq : %llu", freq_base);
    // Discovering if boost is enabled
    if (!boost_enabled) {
        boost_enabled = (freq_max > freq_base);
        // A disguised frequency
        freq_batman = freq_max;
    }
    if (boost_enabled) {
        // Boost is marked as base clock + 1MHz
        freq_batman = freq_base + 1000;
        avail_list[0] = freq_batman;
        debug("avail_list%u: %llu (B)", 0, freq_batman);
        i = 1;
    }
    // While greater than 1GHz
    while((avail_list[i] = freq_base - (100000LLU*m)) >= 1000000) {
        debug("avail_list%u: %llu", i, avail_list[i]);
        ++m;
        ++i;
    }
    avail_list_count = i;
	return EAR_SUCCESS;
}

state_t mgt_intel_pstate_dispose(ctx_t *c)
{
    return EAR_SUCCESS;
}

state_t mgt_intel_pstate_reset()
{
    return EAR_SUCCESS;
}

/* Getters */
state_t mgt_intel_pstate_get_available_list(ctx_t *c, const ullong **list, uint *list_count)
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

state_t mgt_intel_pstate_get_current_list(ctx_t *c, const ullong **p)
{
    state_t s1 = EAR_SUCCESS;
    state_t s2 = EAR_SUCCESS;
    char data[64];
    int cpu;

    for (cpu = 0; cpu < cpu_count; ++cpu) {
        if (state_fail(s1 = read_word(fds_smf[cpu], data, 1))) {
            current_list[cpu] = 0LLU;
            s2 = s1;
        } else {
            current_list[cpu] = (ullong) atoi(data);
            if (boost_enabled && current_list[cpu] == freq_max) {
                current_list[cpu] = freq_batman;
            }
        }
        debug("current_list%d: %llu", cpu, current_list[cpu]);
    }
    *p = current_list;
    return s2;
}

state_t mgt_intel_pstate_get_boost(ctx_t *c, uint *boost_enabled)
{
    *boost_enabled = 0;
	return EAR_SUCCESS;
}

/** Setters */
state_t mgt_intel_pstate_set_current_list(ctx_t *c, uint *freqs_index)
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

state_t mgt_intel_pstate_set_current(ctx_t *c, uint freq_index, int cpu)
{
    char data[SZ_NAME_SHORT];
    // Correcting invalid values
    if (freq_index >= avail_list_count) {
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
}

static state_t static_get_governor(int cpu, uint *governor)
{
    char buffer[64];
    if (state_fail(read_word(fds_sgv[cpu], buffer, 1))) {
        buffer[0] = '\0';
    }
    return mgt_governor_toint(buffer, governor);
}

state_t mgt_intel_pstate_governor_get(ctx_t *c, uint *governor)
{
    return static_get_governor(0, governor);
}

state_t mgt_intel_pstate_governor_get_list(ctx_t *c, uint *governors)
{
    state_t s;
    int i;
    for (i = 0; i < cpu_count; ++i) {
        if (state_fail(s = static_get_governor(i, governors))) {
            return s;
        }
    }
    return EAR_SUCCESS;
}

state_t mgt_intel_pstate_governor_set(ctx_t *c, uint governor)
{
    char buffer[64];
    state_t s;
    if (!mgt_intel_pstate_governor_is_available(c, governor)) {
        return_msg(EAR_ERROR, "Governor not available");
    }
    if (state_fail(s = mgt_governor_tostr(governor, buffer))) {
        return s;
    }
    return write_multi(fds_sgv, buffer, 1);
}

state_t mgt_intel_pstate_governor_set_mask(ctx_t *c, uint governor, cpu_set_t mask)
{
    char buffer[64];
    state_t s;
    int i;

    if (!mgt_intel_pstate_governor_is_available(c, governor)) {
        return_msg(EAR_ERROR, "Governor not available");
    }
    if (state_fail(s = mgt_governor_tostr(governor, buffer))) {
        return s;
    }
    for (i = 0; i < cpu_count; ++i) {
        if (CPU_ISSET(i, &mask)) {
            if (state_fail(s = write_word(fds_sgv[i], buffer, strlen(buffer), 1))) {
                return s;
            }
        }
    }
    return EAR_SUCCESS;
}

state_t mgt_intel_pstate_governor_set_list(ctx_t *c, uint *governors)
{
    char buffer[64];
    state_t s;
    int i;

    for (i = 0; i < cpu_count; ++i) {
        if (!mgt_intel_pstate_governor_is_available(c, governors[i])) {
            return_msg(EAR_ERROR, "Governor not available");
        }
        if (state_fail(s = mgt_governor_tostr(governors[i], buffer))) {
            return s;
        }
        if (state_fail(s = write_word(fds_sgv[i], buffer, strlen(buffer), 1))) {
            return s;
        }
    }
    return EAR_SUCCESS;
}

int mgt_intel_pstate_governor_is_available(ctx_t *c, uint governor)
{
    if (governor == Governor.performance) {
        return 1;
    } else if (governor == Governor.powersave) {
        return 1;
    }
    return 0;
}

#if TEST
int main(int argc, char *argv[])
{
    mgt_ps_driver_ops_t ops;
    const ullong *freqs;
    topology_t tp;

    topology_init(&tp);

    #if 0
    mgt_intel_pstate_load(&tp, &ops);
    mgt_intel_pstate_init(no_ctx);
    mgt_intel_pstate_get_current_list(no_ctx, &freqs);

    if (argc == 1) {
        return 0;
    }
    if (atoi(argv[1]) == 1) {
        if (state_fail(mgt_intel_pstate_governor_set(no_ctx, Governor.performance))) {
            debug("mgt_intel_pstate_governor_set: %s", state_msg);
        }
    }
    if (atoi(argv[1]) == 2) {
        if (state_fail(mgt_intel_pstate_governor_set(no_ctx, Governor.powersave))) {
            debug("mgt_intel_pstate_governor_set: %s", state_msg);
        }
    }
    if (atoi(argv[1]) == 3) {
        if (state_fail(mgt_intel_pstate_set_current(no_ctx, 0, all_cpus))) {
            debug("mgt_intel_pstate_governor_set: %s", state_msg);
        }
    }
    if (atoi(argv[1]) == 4) {
        if (state_fail(mgt_intel_pstate_set_current(no_ctx, 1, all_cpus))) {
            debug("mgt_intel_pstate_governor_set: %s", state_msg);
        }
    }
    #else
    mgt_cpufreq_load(&tp, NO_EARD);
    mgt_cpufreq_init(no_ctx);
    #endif
    return 0;
}
#endif