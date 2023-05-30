// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <mtk_gpufreq_v1.h>
#include <ged_base.h>

GED_ERROR ged_gpufreq_init(void)
{
	return GED_OK;
}

void ged_gpufreq_exit(void)
{
	/* Do nothing */
}

unsigned int ged_get_cur_freq(void)
{
	return mt_gpufreq_get_cur_freq();
}

unsigned int ged_get_cur_volt(void)
{
return mt_gpufreq_get_cur_volt();
}

int ged_get_cur_oppidx(void)
{
	return mt_gpufreq_get_cur_freq_index();
}

int ged_get_max_freq_in_opp(void)
{
	return mt_gpufreq_get_freq_by_idx(0);
}

int ged_get_max_oppidx(void)
{
	return 0;
}

int ged_get_min_oppidx(void)
{
	return mt_gpufreq_get_dvfs_table_num() - 1;
}

int ged_get_min_oppidx_real(void)
{
	return mt_gpufreq_get_dvfs_table_num() - 1;
}

int ged_get_opp_num(void)
{
	return mt_gpufreq_get_dvfs_table_num();
}

int ged_get_opp_num_real(void)
{
	return mt_gpufreq_get_dvfs_table_num();
}

unsigned int ged_get_freq_by_idx(int oppidx)
{
	return mt_gpufreq_get_freq_by_idx(oppidx);
}

unsigned int ged_get_power_by_idx(int oppidx)
{
	return mt_gpufreq_get_power_by_idx(oppidx);
}

int ged_get_oppidx_by_freq(unsigned int freq)
{
	return mt_gpufreq_get_opp_idx_by_freq(freq);
}

unsigned int ged_get_leakage_power(unsigned int volt)
{
	return mt_gpufreq_get_leakage_mw();
}

unsigned int ged_get_dynamic_power(unsigned int freq, unsigned int volt)
{
	return 0;
}

int ged_get_cur_limit_idx_ceil(void)
{
	return mt_gpufreq_get_thermal_limit_index();
}

int ged_get_cur_limit_idx_floor(void)
{
	return -1;
}

unsigned int ged_get_cur_limiter_ceil(void)
{
	return mt_gpufreq_get_limit_user(1);
}

unsigned int ged_get_cur_limiter_floor(void)
{
	return mt_gpufreq_get_limit_user(0);
}

int ged_set_limit_ceil(int limiter, int ceil)
{
	return 0;
}

int ged_set_limit_floor(int limiter, int floor)
{
	return 0;
}

int ged_gpufreq_commit(int oppidx, int commit_type, int *bCommited)
{
	return mt_gpufreq_target(KIR_POLICY, oppidx);
}

unsigned int ged_gpufreq_bringup(void)
{
	return  mt_gpufreq_bringup();
}

unsigned int ged_gpufreq_get_power_state(void)
{
	// For v1 usage, always consider as POWER ON
	return 1;
}

