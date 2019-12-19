#ifndef _CPUFREQ_STATS_H
#define _CPUFREQ_STATS_H

struct cpufreq_stats {
	spinlock_t cpufreq_stats_lock;
	unsigned int total_trans;
	unsigned long long last_time;
	unsigned int max_state;
	unsigned int state_num;
	unsigned int last_index;
	u64 *time_in_state;
	unsigned int *freq_table;
	struct attribute_group *stats_attr_group;
#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
	unsigned int *trans_table;
#endif
};
#endif
