/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __GED_GPUFREQ_V2_H__
#define __GED_GPUFREQ_V2_H__

#include "ged_type.h"

GED_ERROR ged_gpufreq_init(void);
void ged_gpufreq_exit(void);

unsigned int ged_get_cur_freq(void);
unsigned int ged_get_cur_volt(void);
int ged_get_cur_oppidx(void);
int ged_get_max_oppidx(void);
int ged_get_min_oppidx(void);
int ged_get_min_oppidx_real(void);
int ged_get_opp_num(void);
int ged_get_opp_num_real(void);
unsigned int ged_get_freq_by_idx(int oppidx);
unsigned int ged_get_power_by_idx(int oppidx);
int ged_get_oppidx_by_freq(unsigned int freq);
unsigned int ged_get_leakage_power(unsigned int volt);
unsigned int ged_get_dynamic_power(unsigned int freq, unsigned int volt);

int ged_get_cur_limit_idx_ceil(void);
int ged_get_cur_limit_idx_floor(void);
unsigned int ged_get_cur_limiter_ceil(void);
unsigned int ged_get_cur_limiter_floor(void);
int ged_set_limit_ceil(int limiter, int ceil);
int ged_set_limit_floor(int limiter, int floor);

int ged_gpufreq_commit(int oppidx, int commit_type, int *bCommited);

unsigned int ged_gpufreq_bringup(void);
void ged_gpufreq_print_tables(void);

unsigned int ged_gpufreq_get_power_state(void);
int ged_get_max_freq_in_opp(void);

#endif /* __GED_GPUFREQ_V2_H__ */
