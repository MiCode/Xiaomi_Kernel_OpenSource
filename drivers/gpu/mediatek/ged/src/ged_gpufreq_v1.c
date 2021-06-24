// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <mtk_gpufreq_v1.h>


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

int ged_get_max_oppidx(void)
{
    return 0;
}

int ged_get_min_oppidx(void)
{
    return mt_gpufreq_get_dvfs_table_num() - 1;
}

int ged_get_opp_num(void)
{
    return mt_gpufreq_get_dvfs_table_num();
}

unsigned int ged_get_freq_by_idx(int oppidx)
{
    return mt_gpufreq_get_freq_by_idx(oppidx);
}

unsigned int ged_get_volt_by_idx(int oppidx)
{
    return mt_gpufreq_get_volt_by_real_idx(oppidx);
}

unsigned int ged_get_power_by_idx(int oppidx)
{
    return mt_gpufreq_get_power_by_idx(oppidx);
}

int ged_get_oppidx_by_freq(unsigned int freq)
{
    return mt_gpufreq_get_opp_idx_by_freq(freq);
}
int ged_get_oppidx_by_power(unsigned int power)
{
    return -1;
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

int ged_set_limit_ceil(int limiter ,int ceil)
{
    return 0;
}

int ged_set_limit_floor(int limiter, int floor)
{
    return 0;
}

int ged_gpufreq_commit(int oppidx)
{
    return mt_gpufreq_target(KIR_POLICY, oppidx);
}
