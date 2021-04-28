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


#ifndef __MT_PPM_PLATFORM_H__
#define __MT_PPM_PLATFORM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mtk_ppm_api.h"
#include "mach/mtk_cpufreq_api.h"

#if 0 /* No PPM in SSPM @ 6885 */
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#define PPM_SSPM_SUPPORT        (1)
#endif
#endif /* No PPM in SSPM @ 6885 */

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
#define TOTAL_CORE_NUM	(CORE_NUM_L+CORE_NUM_B+CORE_NUM_BB)
#define CORE_NUM_L	(4)
#define CORE_NUM_B	(3)
#define CORE_NUM_BB	(1)

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

/*==============================================================*/
/* Enum								*/
/*==============================================================*/
enum ppm_cluster {
	PPM_CLUSTER_L = 0,
	PPM_CLUSTER_B,
	PPM_CLUSTER_BB,
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
extern unsigned int volt_bb[COBRA_OPP_NUM];
extern unsigned int volt_bl[COBRA_OPP_NUM];

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
extern unsigned int get_sb_pwr(unsigned int tbl_pwr, unsigned int tbl_volt, unsigned int sb_volt);

unsigned int __attribute__((weak))
	mt_cpufreq_get_cur_phy_freq_no_lock(unsigned int id)
{
	return 0;
}

unsigned int __attribute__((weak))
	mt_cpufreq_find_Vboot_idx(unsigned int id)
{
	return 0;
}

static inline int ppm_get_nr_clusters(void) { return NR_PPM_CLUSTERS; }
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
	} else if (cid == 2) {
		cpumask_clear(cpu_mask);
		cpumask_set_cpu(7, cpu_mask);
	}
}

static inline unsigned int get_cluster_cpu_core(unsigned int id)
{
	if (id == 0)
		return CORE_NUM_L;
	else if (id == 1)
		return CORE_NUM_B;
	else if (id == 2)
		return CORE_NUM_BB;

	return 0;
}

static inline unsigned int get_cl_by_core(unsigned int core)
{
	if (core < CORE_NUM_L)
		return 0;
	else if (core < (CORE_NUM_L + CORE_NUM_B))
		return 1;
	else if (core == (CORE_NUM_L + CORE_NUM_B))
		return 2;

	return 0;
}

static inline unsigned int get_cl_cid(unsigned int core)
{
	if (core < CORE_NUM_L)
		return core;
	else if (core < (CORE_NUM_L + CORE_NUM_B))
		return core - 4;
	else
		return 0;

}

static inline unsigned int get_cl_first_core_id(unsigned int cl)
{
	if (cl == 0)
		return 0;
	else if (cl == 1)
		return CORE_NUM_L;
	else if (cl == 2)
		return CORE_NUM_B + CORE_NUM_L;

	return 0;
}


#ifdef __cplusplus
}
#endif

#endif
