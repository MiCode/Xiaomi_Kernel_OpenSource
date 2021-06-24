// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/types.h>
#include <gpufreq_v2.h>


unsigned int ged_get_cur_freq(void)
{
    return gpufreq_get_cur_freq(TARGET_DEFAULT);
}

unsigned int ged_get_cur_volt(void)
{
    return gpufreq_get_cur_volt(TARGET_DEFAULT);
}

int ged_get_cur_oppidx(void)
{
    return gpufreq_get_cur_oppidx(TARGET_DEFAULT);
}

int ged_get_max_oppidx(void)
{
    return gpufreq_get_max_oppidx(TARGET_DEFAULT);
}

int ged_get_min_oppidx(void)
{
    return gpufreq_get_min_oppidx(TARGET_DEFAULT);
}

unsigned int ged_get_opp_num(void)
{
    return gpufreq_get_opp_num(TARGET_DEFAULT);
}

unsigned int ged_get_freq_by_idx(int oppidx)
{
    return gpufreq_get_freq_by_idx(TARGET_DEFAULT, oppidx);
}

unsigned int ged_get_volt_by_idx(int oppidx)
{
    return gpufreq_get_volt_by_idx(TARGET_DEFAULT, oppidx);
}

unsigned int ged_get_power_by_idx(int oppidx)
{
    return gpufreq_get_power_by_idx(TARGET_DEFAULT, oppidx);
}

int ged_get_oppidx_by_freq(unsigned int freq)
{
    return gpufreq_get_oppidx_by_freq(TARGET_DEFAULT, freq);
}
int ged_get_oppidx_by_power(unsigned int power)
{
    return gpufreq_get_oppidx_by_power(TARGET_DEFAULT, power);
}
unsigned int ged_get_leakage_power(unsigned int volt)
{
    return gpufreq_get_leakage_power(TARGET_DEFAULT, volt);
}

unsigned int ged_get_dynamic_power(unsigned int freq, unsigned int volt)
{
    return gpufreq_get_dynamic_power(TARGET_DEFAULT, freq, volt);
}

int ged_get_cur_limit_idx_ceil(void)
{
    return gpufreq_get_cur_limit_idx(TARGET_DEFAULT, GPUPPM_CEILING);
}

int ged_get_cur_limit_idx_floor(void)
{
    return gpufreq_get_cur_limit_idx(TARGET_DEFAULT, GPUPPM_FLOOR);
}

unsigned int ged_get_cur_limiter_ceil(void)
{
    return gpufreq_get_cur_limiter(TARGET_DEFAULT, GPUPPM_CEILING);
}

unsigned int ged_get_cur_limiter_floor(void)
{
    return gpufreq_get_cur_limiter(TARGET_DEFAULT, GPUPPM_FLOOR);
}

int ged_set_limit_ceil(int limiter ,int ceil)
{
    if(limiter)
        return gpufreq_set_limit(TARGET_DEFAULT,
                LIMIT_APIBOOST, ceil, GPUPPM_KEEP_IDX);
    else
        return gpufreq_set_limit(TARGET_DEFAULT,
                LIMIT_FPSGO, ceil, GPUPPM_KEEP_IDX);
}

int ged_set_limit_floor(int limiter, int floor)
{
    if(limiter)
        return gpufreq_set_limit(TARGET_DEFAULT,
                LIMIT_APIBOOST, GPUPPM_KEEP_IDX, floor);
    else
        return gpufreq_set_limit(TARGET_DEFAULT,
                LIMIT_FPSGO, GPUPPM_KEEP_IDX, floor);
}

int ged_gpufreq_commit(int oppidx)
{
    return gpufreq_commit(TARGET_DEFAULT, oppidx);
}

