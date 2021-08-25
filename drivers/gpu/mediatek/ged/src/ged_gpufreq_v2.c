// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <gpufreq_v2.h>
#include <ged_base.h>

static int g_working_oppnum;
static int g_min_working_oppidx;   // opp_num-1
static int g_max_working_oppidx;   // 0
struct gpufreq_opp_info *g_working_table;

GED_ERROR ged_gpufreq_init(void)
{
	int i = 0;
	const struct gpufreq_opp_info *opp_table;

	g_working_oppnum = gpufreq_get_opp_num(TARGET_DEFAULT);
	g_min_working_oppidx = g_working_oppnum - 1;
	g_max_working_oppidx = 0;

	opp_table  = gpufreq_get_working_table(TARGET_DEFAULT);
	g_working_table = kcalloc(g_working_oppnum,
		sizeof(struct gpufreq_opp_info), GFP_KERNEL);

	if (!g_working_table || !opp_table)
		return GED_ERROR_FAIL;

	for (i = 0; i < g_working_oppnum; i++)
		*(g_working_table + i) = *(opp_table + i);

#ifdef GED_KPI_DEBUG
	for (i = 0; i < g_working_oppnum; i++) {
		GED_LOGI("[%02d*] Freq: %d, Volt: %d, Vsram: %d, Vaging: %d",
			i, g_working_table[i].freq, g_working_table[i].volt,
			g_working_table[i].vsram, g_working_table[i].vaging);
	}
#endif /* GED_KPI_DEBUG */

	return GED_OK;
}

void ged_gpufreq_exit(void)
{
	kfree(g_working_table);
}

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
	int oppidx = -1;

	oppidx = gpufreq_get_cur_oppidx(TARGET_DEFAULT);

	if (oppidx >= 0 && oppidx < gpufreq_get_opp_num(TARGET_DEFAULT))
		return oppidx;
	else
		return -1;
}

int ged_get_max_oppidx(void)
{
	return g_max_working_oppidx;
}

int ged_get_min_oppidx(void)
{
	if (g_working_oppnum)
		return g_working_oppnum - 1;
	else
		return gpufreq_get_opp_num(TARGET_DEFAULT) - 1;
}

unsigned int ged_get_opp_num(void)
{
	if (g_working_oppnum)
		return g_working_oppnum;
	else
		return gpufreq_get_opp_num(TARGET_DEFAULT);
}

unsigned int ged_get_freq_by_idx(int oppidx)
{
	if (oppidx <= 0 || oppidx >= g_working_oppnum)
		return 0;

	if (g_working_table == NULL)
		return gpufreq_get_freq_by_idx(TARGET_DEFAULT, oppidx);
	else
		return g_working_table[oppidx].freq;
}

unsigned int ged_get_power_by_idx(int oppidx)
{
	if (oppidx <= 0 || oppidx >= g_working_oppnum)
		return 0;

	if (g_working_table == NULL)
		return gpufreq_get_power_by_idx(TARGET_DEFAULT, oppidx);
	else
		return g_working_table[oppidx].power;
}

int ged_get_oppidx_by_freq(unsigned int freq)
{
	int oppidx = -1;
	int i = 0;

	if (g_working_table == NULL)
		return gpufreq_get_oppidx_by_freq(TARGET_DEFAULT, freq);

	for (i = g_min_working_oppidx; i >= g_max_working_oppidx; i--) {
		if (g_working_table[i].freq >= freq)
			break;
	}
	oppidx = (i > g_max_working_oppidx) ? i : g_max_working_oppidx;

	return oppidx;
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

int ged_set_limit_ceil(int limiter, int ceil)
{
	if (limiter)
		return gpufreq_set_limit(TARGET_DEFAULT,
			LIMIT_APIBOOST, ceil, GPUPPM_KEEP_IDX);
	else
		return gpufreq_set_limit(TARGET_DEFAULT,
			LIMIT_FPSGO, ceil, GPUPPM_KEEP_IDX);
}

int ged_set_limit_floor(int limiter, int floor)
{
	if (limiter)
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

unsigned int ged_gpufreq_bringup(void)
{
	return gpufreq_bringup();
}
