/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __SWPM_CPU_V6886_H__
#define __SWPM_CPU_V6886_H__

#define NR_CPU_CORE				(8)
#define NR_CPU_LKG				(9)

enum cpu_cmd_action {
	CPU_GET_ADDR,
	CPU_SET_PMU_MS,
};

enum cpu_type {
	CPU_TYPE_L,
	CPU_TYPE_B,

	NR_CPU_TYPE
};

enum cpu_lkg_type {
	CPU_L_LKG,
	CPU_B_LKG,
	DSU_LKG,

	NR_CPU_LKG_TYPE
};

enum dsu_pmu_idx {
	DSU_PMU_IDX_CYCLES,

	MAX_DSU_PMU_CNT
};

enum pmu_idx {
	PMU_IDX_L3DC,
	PMU_IDX_INST_SPEC,
	PMU_IDX_CYCLES,
	PMU_IDX_NON_WFX_COUNTER, /* put non-WFX counter here */

	MAX_PMU_CNT
};

enum cpu_core_power_state {
	CPU_CORE_ACTIVE,
	CPU_CORE_IDLE,
	CPU_CORE_POWER_OFF,

	NR_CPU_CORE_POWER_STATE
};

enum cpu_cluster_power_state {
	CPU_CLUSTER_ACTIVE,
	CPU_CLUSTER_IDLE,
	CPU_CLUSTER_DORMANT,
	CPU_CLUSTER_POWER_OFF,

	NR_CPU_CLUSTER_POWER_STATE
};

enum mcusys_power_state {
	MCUSYS_ACTIVE,
	MCUSYS_IDLE,
	MCUSYS_POWER_OFF,

	NR_MCUSYS_POWER_STATE
};

enum cpu_pwr_type {
	CPU_PWR_TYPE_L,
	CPU_PWR_TYPE_B,
	CPU_PWR_TYPE_DSU,
	CPU_PWR_TYPE_MCUSYS,

	NR_CPU_PWR_TYPE
};

/* cpu voltage/freq index */
struct cpu_swpm_vf_index {
	unsigned int cpu_volt_mv[NR_CPU_TYPE];
	unsigned int cpu_freq_mhz[NR_CPU_TYPE];
	unsigned int cpu_opp[NR_CPU_TYPE];
	unsigned int cci_volt_mv;
	unsigned int cci_freq_mhz;
	unsigned int cci_opp;
};

/* cpu power index structure */
struct cpu_swpm_index {
	unsigned int core_state_ratio[NR_CPU_CORE_POWER_STATE][NR_CPU_CORE];
	unsigned int cpu_stall_ratio[NR_CPU_CORE];
	unsigned int cluster_state_ratio[NR_CPU_CLUSTER_POWER_STATE];
	unsigned int mcusys_state_ratio[NR_MCUSYS_POWER_STATE];
	unsigned int pmu_val[MAX_PMU_CNT][NR_CPU_CORE];
	unsigned int dsu_pmu_val[MAX_DSU_PMU_CNT];
	unsigned int l3_bw;
	unsigned int cpu_emi_bw;
	struct cpu_swpm_vf_index vf;
	unsigned int cpu_lkg[NR_CPU_LKG];
	unsigned int cpu_pwr[NR_CPU_PWR_TYPE];
};

struct cpu_swpm_data {
	unsigned int cpu_temp[NR_CPU_CORE];
};

extern int swpm_cpu_v6886_init(void);
extern void swpm_cpu_v6886_exit(void);

#endif

