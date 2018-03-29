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


#include <linux/cpufreq.h>
#include "mach/mt_ppm_api.h"

void mt_ppm_set_dvfs_table(unsigned int cpu, struct cpufreq_frequency_table *tbl,
	unsigned int num, enum dvfs_table_type type)
{
}

void mt_ppm_register_client(enum ppm_client client, void (*limit)(struct ppm_client_req req))
{
}

void mt_ppm_set_5A_limit_throttle(bool enable)
{
}

void mt_ppm_limit_freq_when_ke(void)
{
}

/* SYS boost policy */
void mt_ppm_sysboost_core(enum ppm_sysboost_user user, unsigned int core_num)
{
}

void mt_ppm_sysboost_freq(enum ppm_sysboost_user user, unsigned int freq)
{
}

void mt_ppm_sysboost_set_core_limit(enum ppm_sysboost_user user, unsigned int cluster,
					int min_core, int max_core)
{
}

void mt_ppm_sysboost_set_freq_limit(enum ppm_sysboost_user user, unsigned int cluster,
					int min_freq, int max_freq)
{
}

/* DLPT policy */
void mt_ppm_dlpt_set_limit_by_pbm(unsigned int limited_power)
{
}

void mt_ppm_dlpt_kick_PBM(struct ppm_cluster_status *cluster_status, unsigned int cluster_num)
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

unsigned int mt_ppm_thermal_get_cur_power(void)
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

/* HICA policy */
void mt_ppm_hica_update_algo_data(unsigned int cur_loads,
				unsigned int cur_nr_heavy_task, unsigned int cur_tlp)
{
}

int mt_ppm_main(void)
{
	return 0;
}

/* MET */
void mt_set_ppm_state_registerCB(met_set_ppm_state_funcMET pCB)
{
}

