// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/cpufreq.h>
#include "mtk_ppm_api.h"
#include "mtk_ppm_internal.h"

void mt_ppm_set_dvfs_table(unsigned int cpu,
	struct cpufreq_frequency_table *tbl,
	unsigned int num, enum dvfs_table_type type)
{
}

void mt_ppm_register_client(enum ppm_client client,
	void (*limit)(struct ppm_client_req req))
{
}


/* SYS boost policy */
void mt_ppm_sysboost_core(enum ppm_sysboost_user user, unsigned int core_num)
{
}

void mt_ppm_sysboost_freq(enum ppm_sysboost_user user, unsigned int freq)
{
}

void mt_ppm_sysboost_set_core_limit(enum ppm_sysboost_user user,
	unsigned int cluster, int min_core, int max_core)
{
}

void mt_ppm_sysboost_set_freq_limit(enum ppm_sysboost_user user,
	unsigned int cluster, int min_freq, int max_freq)
{
}

/* DLPT policy */
void mt_ppm_dlpt_set_limit_by_pbm(unsigned int limited_power)
{
}

void mt_ppm_dlpt_kick_PBM(struct ppm_cluster_status *cluster_status,
	unsigned int cluster_num)
{
}

void ppm_cobra_update_core_limit(unsigned int cluster, int limit)
{
}
void ppm_cobra_dump_tbl(struct seq_file *m)
{
}
void ppm_cobra_update_freq_limit(unsigned int cluster, int limit)
{
}
/* Thermal policy */
void mt_ppm_cpu_thermal_protect(unsigned int limited_power)
{
}

unsigned int mt_ppm_thermal_get_min_power(void)
{
	return 0;
}

unsigned int mt_ppm_thermal_get_max_power(void)
{
	return 0;
}
unsigned int ppm_calc_total_power(struct ppm_cluster_status *cluster_status,
			 unsigned int cluster_num, unsigned int percentage)
{
	return 0;
}
unsigned int mt_ppm_thermal_get_cur_power(void)
{
	return 0;
}
int ppm_get_max_pwr_idx(void)
{
	return 0;
}
int ppm_get_min_pwr_idx(void)
{
	return 0;
}
void ppm_update_req_by_pwr(struct ppm_policy_req *req)
{
}

extern void ppm_cobra_lookup_get_result(struct seq_file *m, enum ppm_cobra_lookup_type type)
{
}
int ppm_find_pwr_idx(struct ppm_cluster_status *cluster_status)
{
	return 0;
}
struct ppm_cobra_lookup cobra_lookup_data;

/* User limit policy */
unsigned int mt_ppm_userlimit_cpu_core(unsigned int cluster_num,
	struct ppm_limit_data *data)
{
	return 0;
}

unsigned int mt_ppm_userlimit_cpu_freq(unsigned int cluster_num,
	struct ppm_limit_data *data)
{
	return 0;
}

unsigned int mt_ppm_userlimit_freq_limit_by_others(unsigned int cluster)
{
	return 0;
}

void ppm_game_mode_change_cb(int is_game_mode)
{
}

/* Force limit policy */
unsigned int mt_ppm_forcelimit_cpu_core(unsigned int cluster_num,
	struct ppm_limit_data *data)
{
	return 0;
}

/* PTPOD policy */
void mt_ppm_ptpod_policy_activate(void)
{
}

void mt_ppm_ptpod_policy_deactivate(void)
{
}

unsigned int mt_ppm_get_leakage_mw(enum ppm_cluster_lkg cluster)
{
	return 0;
}

/* CPI */
unsigned int ppm_get_cluster_cpi(unsigned int cluster)
{
	return 0;
}

MODULE_DESCRIPTION("Dummy Driver for MediaTek PPM");
MODULE_LICENSE("GPL");
