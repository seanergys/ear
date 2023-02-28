#ifndef MANAGEMENT_CPUFREQ_BASE_H
#define MANAGEMENT_CPUFREQ_BASE_H

typedef struct basefreq_s {
    ullong frequency; // In KHZ
    uint   boost_enabled;
} basefreq_t;

void basefreq_init(topology_t *tp, basefreq_t *base);

#endif //MANAGEMENT_CPUFREQ_BASE_H
