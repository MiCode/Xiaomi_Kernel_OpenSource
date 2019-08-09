/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef CA_INTERNAL_H
#define CA_INTERNAL_H


#define NR_MAX_PMU 6

struct ca_pmu_stats {
	int *config;
	unsigned long long prev_clock_task;
	int in_aging_control;
	int in_partition_control;
	struct perf_event *events[NR_MAX_PMU];
	u64 prev_counters[NR_MAX_PMU];
};


extern int ca_notifier(struct notifier_block *self,
			    unsigned long action, void *hcpu);
static struct notifier_block ca_nb = {
	.notifier_call	= ca_notifier,
	.priority	= 0,
};

#define STALL_RATIO_CA55 (100)

#define GROUP_ROOT 1
#define GROUP_FG   2
#define GROUP_BG   3
#define GROUP_TA   4

#define CORTEX_A75 75
#define CORTEX_A55 55
struct cpu_config_node {
	const char *compatible;
	int *config;
};

void release_cache_control(int cpu);
void disable_cache_control(void);
void apply_cache_control(int cpu);
extern int __init init_cache_priority(void);
void config_partition(int enable);

enum setting_type {
	DISABLE_TYPE,
	DEFAULT_TYPE,
	BENCHMARK_TYPE,
	NR_TYPES
};
struct default_setting {
	bool partition_enable;
	unsigned int penalty_shift;
	unsigned long stall_ratio;
	unsigned long background_badness;
};
struct default_setting set_mode[NR_TYPES] = {
	[DEFAULT_TYPE] = {
		.partition_enable = true,
		.penalty_shift = 14,
		.stall_ratio   = 300,
		.background_badness = 10
	},
	[BENCHMARK_TYPE] = {
		.partition_enable = true,
		.penalty_shift = 17,
		.stall_ratio   = 100,
		.background_badness = 10
	},
};

extern int sysctl_perf_event_paranoid;
#endif
