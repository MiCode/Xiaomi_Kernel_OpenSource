/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MT_PPM_PLATFORM_H__
#define __MT_PPM_PLATFORM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mtk_ppm_api.h"
#include "mtk_cpufreq_api.h"

/*==============================================================*/
/* Macros							*/
/*==============================================================*/
/* TODO: remove these workaround for k49 migration */
#define NO_MTK_TRACE		(1)

#define DYNAMIC_TABLE2REAL_PERCENTAGE	(58)

/* for COBRA algo */
#define PPM_COBRA_USE_CORE_LIMIT	(1)
#define PPM_COBRA_RUNTIME_CALC_DELTA	(1)
#ifdef PPM_COBRA_USE_CORE_LIMIT
#define PPM_COBRA_MAX_FREQ_IDX	(COBRA_OPP_NUM)
#else
#define PPM_COBRA_MAX_FREQ_IDX	(COBRA_OPP_NUM-1)
#endif
#define COBRA_OPP_NUM	(DVFS_OPP_NUM)
#define TOTAL_CORE_NUM	(CORE_NUM_L+CORE_NUM_B)
#define CORE_NUM_L	(6)
#define CORE_NUM_B	(2)

#define PPM_COBRA_TBL_SRAM_ADDR	(0x0011B800)
#define PPM_COBRA_TBL_SRAM_SIZE \
	(sizeof(struct ppm_cobra_basic_pwr_data) \
	*TOTAL_CORE_NUM*COBRA_OPP_NUM)
#ifdef PPM_SSPM_SUPPORT
/* online core to SSPM */
#define PPM_ONLINE_CORE_SRAM_ADDR \
	(PPM_COBRA_TBL_SRAM_ADDR + PPM_COBRA_TBL_SRAM_SIZE)
#endif

/* other policy settings */
#define PTPOD_FREQ_IDX		(7)
#define PWRTHRO_BAT_PER_MW	(600)
#define PWRTHRO_BAT_OC_MW	(600)
#define PWRTHRO_LOW_BAT_LV1_MW	(600)
#define PWRTHRO_LOW_BAT_LV2_MW	(600)

#define DVFS_OPP_NUM		(16)
#define get_cluster_ptpod_fix_freq_idx(id)	(mt_cpufreq_find_Vboot_idx(id))
#define get_cluster_cpu_core(id)	\
		(id ? CORE_NUM_B : CORE_NUM_L)

/*==============================================================*/
/* Enum								*/
/*==============================================================*/
enum ppm_cluster {
	PPM_CLUSTER_L = 0,
	PPM_CLUSTER_B,

	NR_PPM_CLUSTERS,
};

enum ppm_cobra_lookup_type {
	LOOKUP_BY_BUDGET,
	LOOKUP_BY_LIMIT,

	NR_PPM_COBRA_LOOKUP_TYPE,
};
/*==============================================================*/
/* Data Structures						*/
/*==============================================================*/
struct ppm_cobra_basic_pwr_data {
	unsigned short perf_idx;
	unsigned short power_idx;
};

struct ppm_cobra_delta_data {
	unsigned int delta_pwr;
};

struct ppm_cobra_data {
	struct ppm_cobra_basic_pwr_data
		basic_pwr_tbl[TOTAL_CORE_NUM][DVFS_OPP_NUM];
};

struct ppm_cobra_lookup {
	unsigned int budget;
	struct {
		unsigned int opp;
		unsigned int core;
	} limit[NR_PPM_CLUSTERS];
};

/*==============================================================*/
/* Global Variables						*/
/*==============================================================*/
extern struct ppm_cobra_data *cobra_tbl;
extern struct ppm_cobra_lookup cobra_lookup_data;
extern int cobra_init_done;

/*==============================================================*/
/* APIs								*/
/*==============================================================*/
extern unsigned int ppm_calc_total_power(
			struct ppm_cluster_status *cluster_status,
			unsigned int cluster_num,
			unsigned int percentage);
extern int ppm_platform_init(void);

/* COBRA algo */
extern void ppm_cobra_update_core_limit(unsigned int cluster, int limit);
extern void ppm_cobra_update_freq_limit(unsigned int cluster, int limit);
extern void ppm_cobra_update_limit(void *user_req);
extern void ppm_cobra_init(void);
extern void ppm_cobra_dump_tbl(struct seq_file *m);
extern void ppm_cobra_lookup_get_result(
		struct seq_file *m, enum ppm_cobra_lookup_type type);

unsigned int __attribute__((weak))
	mt_cpufreq_get_cur_phy_freq_no_lock(unsigned int id)
{
	return 0;
}

static inline int ppm_get_nr_clusters(void) { return NR_PPM_CLUSTERS; }
static inline void ppm_get_cl_cpus(struct cpumask *cpu_mask, unsigned int cid)
{
	if (cid == 0) {
		cpumask_setall(cpu_mask);
		cpumask_clear_cpu(6, cpu_mask);
		cpumask_clear_cpu(7, cpu_mask);
	} else if (cid == 1) {
		cpumask_clear(cpu_mask);
		cpumask_set_cpu(6, cpu_mask);
		cpumask_set_cpu(7, cpu_mask);
	}
}
#ifdef __cplusplus
}
#endif

#endif
