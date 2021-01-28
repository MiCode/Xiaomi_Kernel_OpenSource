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
	unsigned long long prev_clock;
	struct perf_event *events[NR_MAX_PMU];
	u64 prev_counters[NR_MAX_PMU];
	u64 counters[NR_MAX_PMU];
};

DECLARE_PER_CPU(struct ca_pmu_stats, ca_pmu_stats);

#define GROUP_ROOT 1
#define GROUP_FG   2
#define GROUP_BG   3
#define GROUP_TA   4

#define CORTEX_A76 76
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

enum CA55_PMU_EVENT {
	// L2_PF_LD,
	L2_PF_ST,
	// L2_PF,
	L2_PF_UNUSED,
	// L2_PF_REFILL,
	NR_PMU_COUNTERS,
	FRONTEND_STALL,
	BACKEND_STALL,
	CPU_CYCLES,
	LL_CACHE_MISS_RD,
	INST_RETIRED,
};
extern int ca55_register[];
extern int ca75_register[];
extern int ca76_register[];

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
extern struct default_setting set_mode[NR_TYPES];

extern int set_pftch_qos_control(const char *buf,
		const struct kernel_param *kp);

struct pftch_environment {
	int is_congested;
	int is_enabled;
};
extern struct pftch_environment pftch_env;


#define IS_BIG_CORE(cpu) (cpu == 4 || cpu == 5 || cpu == 6 || cpu == 7)

extern struct notifier_block nb;
extern int sysctl_perf_event_paranoid;
extern void pftch_qos_tick(int cpu);

#define PFTCH_ENABLE (0x3333)
#define PFTCH_DISABLE (0x22223333)
#endif
