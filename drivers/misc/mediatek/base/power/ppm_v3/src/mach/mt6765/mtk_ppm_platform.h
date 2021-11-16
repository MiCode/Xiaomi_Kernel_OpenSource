/*
 * Copyright (C) 2016 MediaTek Inc.
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


#ifndef __MT_PPM_PLATFORM_H__
#define __MT_PPM_PLATFORM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mtk_ppm_api.h"
#include "mach/mtk_cpufreq_api.h"

/*==============================================================*/
/* Macros                                                       */
/*==============================================================*/
/* TODO: remove these workaround for k49 migration */
#define NO_MTK_TRACE		(1)
#define NO_SCHEDULE_API		(1)

#define DYNAMIC_TABLE2REAL_PERCENTAGE	(58)

/* for COBRA algo */
#define PPM_COBRA_USE_CORE_LIMIT	(1)
#define PPM_COBRA_RUNTIME_CALC_DELTA	(0)
#ifdef PPM_COBRA_USE_CORE_LIMIT
#define PPM_COBRA_MAX_FREQ_IDX	(COBRA_OPP_NUM)
#else
#define PPM_COBRA_MAX_FREQ_IDX	(COBRA_OPP_NUM-1)
#endif
#define COBRA_OPP_NUM	(DVFS_OPP_NUM)
#define TOTAL_CORE_NUM	(CORE_LL+CORE_L)
#define CORE_LL	(4)
#define CORE_L	(4)

#ifdef PPM_SSPM_SUPPORT
#define PPM_COBRA_TBL_SRAM_ADDR	(0x0011B800)
#define PPM_COBRA_TBL_SRAM_SIZE \
	(sizeof(struct ppm_cobra_basic_pwr_data) \
	* TOTAL_CORE_NUM * COBRA_OPP_NUM)
/* online core to SSPM */
#define PPM_ONLINE_CORE_SRAM_ADDR \
	(PPM_COBRA_TBL_SRAM_ADDR + PPM_COBRA_TBL_SRAM_SIZE)
#endif

/* other policy settings */
#define PTPOD_FREQ_IDX		(8)
#define PTPOD_FREQ_IDX_LY	(10)
#define PWRTHRO_BAT_PER_MW	(600)
#define PWRTHRO_BAT_OC_MW	(600)
#define PWRTHRO_LOW_BAT_LV1_MW	(600)
#define PWRTHRO_LOW_BAT_LV2_MW	(600)

#define DVFS_OPP_NUM		(16)
#define get_cluster_cpu_core(id)	\
		(id ? CORE_LL : CORE_L)
/*==============================================================*/
/* Enum                                                         */
/*==============================================================*/
enum ppm_cluster {
	PPM_CLUSTER_L = 0,
	PPM_CLUSTER_LL,

	NR_PPM_CLUSTERS,
};

enum ppm_cobra_lookup_type {
	LOOKUP_BY_BUDGET,
	LOOKUP_BY_LIMIT,

	NR_PPM_COBRA_LOOKUP_TYPE,
};
/*==============================================================*/
/* Data Structures                                              */
/*==============================================================*/
struct ppm_cobra_basic_pwr_data {
	unsigned short perf_idx;
	unsigned short power_idx;
};

struct ppm_cobra_delta {
	unsigned int delta_pwr;
};

struct ppm_cobra_data {
	struct ppm_cobra_basic_pwr_data ptbl[TOTAL_CORE_NUM][DVFS_OPP_NUM];
#if !PPM_COBRA_RUNTIME_CALC_DELTA
	struct ppm_cobra_delta delta_tbl[CORE_L+1][CORE_LL+1][COBRA_OPP_NUM];
#endif
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
extern struct ppm_cobra_data cobra_tbl;
extern struct ppm_cobra_lookup cobra_lookup_data;

/*==============================================================*/
/* APIs								*/
/*==============================================================*/
extern unsigned int ppm_calc_total_power(
	struct ppm_cluster_status *cluster_status,
	unsigned int cluster_num, unsigned int percentage);
extern int ppm_platform_init(void);

/* COBRA algo */
extern void ppm_cobra_update_core_limit(unsigned int cluster, int limit);
extern void ppm_cobra_update_freq_limit(unsigned int cluster, int limit);
extern void ppm_cobra_update_limit(void *user_req);
extern void ppm_cobra_init(void);
extern void ppm_cobra_dump_tbl(struct seq_file *m);
extern void ppm_cobra_lookup_get_result(struct seq_file *m,
	enum ppm_cobra_lookup_type type);

unsigned int __attribute__((weak)) mt_cpufreq_get_cur_phy_freq_no_lock(
	unsigned int id)
{
	return 0;
}

unsigned int get_cluster_ptpod_fix_freq_idx(unsigned int id);
static inline void ppm_get_cl_cpus(struct cpumask *cpu_mask, unsigned int cid)
{
	if (cid == 0) {
		cpumask_setall(cpu_mask);
		cpumask_clear_cpu(4, cpu_mask);
		cpumask_clear_cpu(5, cpu_mask);
		cpumask_clear_cpu(6, cpu_mask);
		cpumask_clear_cpu(7, cpu_mask);
	} else if (cid == 1) {
		cpumask_clear(cpu_mask);
		cpumask_set_cpu(4, cpu_mask);
		cpumask_set_cpu(5, cpu_mask);
		cpumask_set_cpu(6, cpu_mask);
		cpumask_set_cpu(7, cpu_mask);
	}
}
#ifdef __cplusplus
}
#endif

#endif
