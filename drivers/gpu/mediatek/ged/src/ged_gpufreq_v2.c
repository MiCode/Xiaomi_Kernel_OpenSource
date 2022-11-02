// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <gpufreq_v2.h>
#include <ged_base.h>
#include <ged_dcs.h>
#include <ged_dvfs.h>
#include <ged_global.h>
#include <ged_eb.h>
#include <mt-plat/mtk_gpu_utility.h>

static int g_min_count;

static int g_max_core_num;             /* core_num */
static int g_avail_mask_num;           /* mask_num */

static int g_working_oppnum;           /* opp_num */
static int g_virtual_oppnum;           /* opp_num + mask_num - 1 */
static int g_max_working_oppidx;       /* 0 */
static int g_min_working_oppidx;       /* opp_num - 1 */
static int g_min_virtual_oppidx;       /* opp_num - 1 + mask_num -1 */

static int g_max_freq_in_mhz;         /* max freq in opp table */

struct gpufreq_core_mask_info *g_mask_table;
struct gpufreq_opp_info *g_working_table;
struct gpufreq_opp_info *g_virtual_table;

GED_ERROR ged_gpufreq_init(void)
{
	int i, j = 0;
	int min_freq, freq_scale = 0;
	unsigned int core_num = 0;
	const struct gpufreq_opp_info *opp_table;
	struct gpufreq_core_mask_info *core_mask_table;

	if (gpufreq_bringup())
		return GED_OK;

	GED_LOGI("%s: start to init GPU Freq\n", __func__);

	/* init gpu opp table */
	g_working_oppnum = gpufreq_get_opp_num(TARGET_DEFAULT);
	g_min_working_oppidx = g_working_oppnum - 1;
	g_max_working_oppidx = 0;
	g_max_freq_in_mhz = gpufreq_get_freq_by_idx(TARGET_DEFAULT, 0) / 1000;

	opp_table  = gpufreq_get_working_table(TARGET_DEFAULT);
	g_working_table = kcalloc(g_working_oppnum,
						sizeof(struct gpufreq_opp_info), GFP_KERNEL);

	if (!g_working_table || !opp_table) {
		GED_LOGE("%s: Failed to init opp table");
		return GED_ERROR_FAIL;
	}

	for (i = 0; i < g_working_oppnum; i++)
		*(g_working_table + i) = *(opp_table + i);

#ifdef GED_KPI_DEBUG
	for (i = 0; i < g_working_oppnum; i++) {
		GED_LOGI("[%02d*] Freq: %d, Volt: %d, Vsram: %d, Vaging: %d",
			i, g_working_table[i].freq, g_working_table[i].volt,
			g_working_table[i].vsram, g_working_table[i].vaging);
	}
#endif /* GED_KPI_DEBUG */

	/* init core mask table if support DCS policy*/
	core_mask_table = dcs_get_avail_mask_table();
	g_max_core_num = dcs_get_max_core_num();
	g_avail_mask_num = dcs_get_avail_mask_num();

	g_mask_table = kcalloc(g_avail_mask_num,
					sizeof(struct gpufreq_core_mask_info), GFP_KERNEL);

	for (i = 0; i < g_avail_mask_num; i++)
		*(g_mask_table + i) = *(core_mask_table + i);

	if (!core_mask_table || !g_mask_table) {
		GED_LOGE("%s: Failed to init core mask table");
		return GED_OK;
	}

#ifdef GED_KPI_DEBUG
	for (i = 0; i < g_avail_mask_num; i++) {
		GED_LOGE("[%02d*] MC0%d : 0x%llX",
			i, g_mask_table[i].num, g_mask_table[i].mask);
	}
#endif /* GED_KPI_DEBUG */

	/* init virtual opp table by core mask table */
	g_virtual_oppnum = g_working_oppnum + g_avail_mask_num - 1;
	g_min_virtual_oppidx = g_min_working_oppidx + g_avail_mask_num - 1;

	g_virtual_table = kcalloc(g_virtual_oppnum,
						sizeof(struct gpufreq_opp_info), GFP_KERNEL);

	if (!g_mask_table || !g_virtual_table) {
		GED_LOGE("%s: Failed to init virtual opp table");
		return GED_ERROR_FAIL;
	}
	for (i = 0; i < g_working_oppnum; i++)
		*(g_virtual_table + i) = *(opp_table + i);


	min_freq = gpufreq_get_freq_by_idx(TARGET_DEFAULT, g_min_working_oppidx);

	/* construct virtual opp from real freq and core num */
	for (i = g_working_oppnum ; i < g_virtual_oppnum; i++) {
		j = i - g_virtual_oppnum + g_avail_mask_num;

		freq_scale = min_freq * g_mask_table[j].num / g_max_core_num;

		g_virtual_table[i].freq =  freq_scale;
		g_virtual_table[i].volt = 0;
		g_virtual_table[i].vsram = 0;
		g_virtual_table[i].vaging = 0;
	}

#ifdef GED_KPI_DEBUG
	for (i = 0; i < g_virtual_oppnum; i++) {
		GED_LOGI("[%02d*] Freq: %d, Volt: %d, Vsram: %d, Vaging: %d",
			i, g_virtual_table[i].freq, g_virtual_table[i].volt,
			g_virtual_table[i].vsram, g_virtual_table[i].vaging);
	}
#endif /* GED_KPI_DEBUG */
	return GED_OK;
}

void ged_gpufreq_exit(void)
{
	kfree(g_working_table);
	kfree(g_virtual_table);
	kfree(g_mask_table);
}

unsigned int ged_get_cur_freq(void)
{
	unsigned int freq = 0;
	unsigned int core_num = 0;

	freq = gpufreq_get_cur_freq(TARGET_DEFAULT);

	if (!is_dcs_enable())
		return freq;

	core_num = dcs_get_cur_core_num();

	if (core_num < g_max_core_num)
		freq = freq * core_num / g_max_core_num;

	return freq;
}

unsigned int ged_get_cur_volt(void)
{
	return gpufreq_get_cur_volt(TARGET_DEFAULT);
}

int ged_get_cur_oppidx(void)
{
	unsigned int core_num = 0;
	int i = 0;
	int oppidx = 0;

	oppidx = gpufreq_get_cur_oppidx(TARGET_DEFAULT);

	if (!is_dcs_enable())
		return oppidx;

	core_num = dcs_get_cur_core_num();

	if (core_num == g_max_core_num)
		return oppidx;

	for (i = 0; i < g_avail_mask_num; i++) {
		if (g_mask_table[i].num == core_num)
			break;
	}

	oppidx = g_min_working_oppidx + i;

	return oppidx;
}

int ged_get_max_freq_in_opp(void)
{
	return g_max_freq_in_mhz;
}

int ged_get_max_oppidx(void)
{
	return g_max_working_oppidx;
}

int ged_get_min_oppidx(void)
{
	if (is_dcs_enable() && g_min_virtual_oppidx)
		return g_min_virtual_oppidx;

	if (g_min_working_oppidx)
		return g_min_working_oppidx;
	else
		return gpufreq_get_opp_num(TARGET_DEFAULT) - 1;
}

int ged_get_min_oppidx_real(void)
{
	if (g_min_working_oppidx)
		return g_min_working_oppidx;
	else
		return gpufreq_get_opp_num(TARGET_DEFAULT) - 1;
}

unsigned int ged_get_opp_num(void)
{
	if (is_dcs_enable() && g_virtual_oppnum)
		return g_virtual_oppnum;

	if (g_working_oppnum)
		return g_working_oppnum;
	else
		return gpufreq_get_opp_num(TARGET_DEFAULT);
}

unsigned int ged_get_opp_num_real(void)
{
	if (g_working_oppnum)
		return g_working_oppnum;
	else
		return gpufreq_get_opp_num(TARGET_DEFAULT);
}

unsigned int ged_get_freq_by_idx(int oppidx)
{
	if (is_dcs_enable() && g_virtual_table)
		return g_virtual_table[oppidx].freq;

	if (g_working_table == NULL)
		return gpufreq_get_freq_by_idx(TARGET_DEFAULT, oppidx);

	if (unlikely(oppidx > g_min_working_oppidx))
		oppidx = g_min_working_oppidx;

	return g_working_table[oppidx].freq;
}

unsigned int ged_get_power_by_idx(int oppidx)
{
	if (oppidx < 0 || oppidx >= g_working_oppnum)
		return 0;

	if (g_working_table == NULL)
		return gpufreq_get_power_by_idx(TARGET_DEFAULT, oppidx);

	if (unlikely(oppidx > g_min_working_oppidx))
		oppidx = g_min_working_oppidx;

	return g_working_table[oppidx].power;
}

int ged_get_oppidx_by_freq(unsigned int freq)
{
	int i = 0;

	if (g_working_table == NULL)
		return gpufreq_get_oppidx_by_freq(TARGET_DEFAULT, freq);

	if (is_dcs_enable() && g_virtual_table) {
		for (i = g_min_virtual_oppidx; i >= g_max_working_oppidx; i--) {
			if (g_virtual_table[i].freq >= freq)
				break;
		}
		return (i > g_max_working_oppidx) ? i : g_max_working_oppidx;
	}

	for (i = g_min_working_oppidx; i >= g_max_working_oppidx; i--) {
		if (g_working_table[i].freq >= freq)
			break;
	}
	return (i > g_max_working_oppidx) ? i : g_max_working_oppidx;
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
	int cur_floor = 0;

	cur_floor =  gpufreq_get_cur_limit_idx(TARGET_DEFAULT, GPUPPM_FLOOR);

	if (is_dcs_enable() && cur_floor == g_min_working_oppidx)
		cur_floor = g_min_virtual_oppidx;

	return cur_floor;
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
	if (ceil > g_min_working_oppidx)
		ceil = g_min_working_oppidx;

	if (limiter)
		return gpufreq_set_limit(TARGET_DEFAULT,
			LIMIT_APIBOOST, ceil, GPUPPM_KEEP_IDX);
	else
		return gpufreq_set_limit(TARGET_DEFAULT,
			LIMIT_FPSGO, ceil, GPUPPM_KEEP_IDX);
}

int ged_set_limit_floor(int limiter, int floor)
{
	if (floor > g_min_working_oppidx)
		floor = g_min_working_oppidx;

	if (limiter)
		return gpufreq_set_limit(TARGET_DEFAULT,
			LIMIT_APIBOOST, GPUPPM_KEEP_IDX, floor);
	else
		return gpufreq_set_limit(TARGET_DEFAULT,
			LIMIT_FPSGO, GPUPPM_KEEP_IDX, floor);
}

int ged_gpufreq_commit(int oppidx, int commit_type, int *bCommited)
{
	int ret = GED_OK;
	int oppidx_tar = 0;
	int mask_idx = 0;
	unsigned int freq = 0, core_mask_tar = 0, core_num_tar = 0;

	int dvfs_state = 0;

	/* DCS policy enabled */
	if (is_dcs_enable()) {

		/* check constraint for lowest virtual opp */
		if (oppidx == g_min_virtual_oppidx)
			g_min_count = (g_min_count < DCS_MIN_OPP_CNT) ?
				g_min_count + 1 : DCS_MIN_OPP_CNT;
		else
			g_min_count = 0;

		if (g_min_count > 0 && g_min_count < 4)
			oppidx -= 1;

		/* check FB fallback timer commit */
		if (commit_type == GED_DVFS_FB_FALLBACK_COMMIT)
			if (oppidx > g_min_working_oppidx)
				oppidx = g_min_working_oppidx;

		/* convert virtual opp to working opp with corresponding core mask */
		if (oppidx > g_min_working_oppidx) {
			mask_idx = oppidx - g_virtual_oppnum + g_avail_mask_num;
			oppidx_tar = g_min_working_oppidx;
		} else {
			mask_idx = 0;
			oppidx_tar = oppidx;
		}

		/* scaling cores to max if freq. is fixed */
		dvfs_state = gpufreq_get_dvfs_state();

		if (dvfs_state == DVFS_DEBUG_KEEP) {
			mask_idx = 0;
			oppidx_tar = oppidx;
		}

		core_mask_tar = g_mask_table[mask_idx].mask;
		core_num_tar = g_mask_table[mask_idx].num;

		/* scaling freq first than scaling shader cores*/
		if (ged_is_fdvfs_support())
			mtk_gpueb_dvfs_dcs_commit(oppidx_tar, commit_type,
				 g_virtual_table[oppidx].freq);
		else
			ged_dvfs_gpu_freq_commit_fp(TARGET_DEFAULT, oppidx_tar, bCommited);

		dcs_set_core_mask(core_mask_tar, core_num_tar);
	}
	/* DCS policy disabled */
	else {
		freq = (g_working_table == NULL) ?
			gpufreq_get_freq_by_idx(TARGET_DEFAULT, oppidx)
			: g_working_table[oppidx].freq;

		if (ged_is_fdvfs_support())
			mtk_gpueb_dvfs_dcs_commit(oppidx, commit_type, freq);
		else
			ged_dvfs_gpu_freq_commit_fp(oppidx, commit_type, bCommited);
	}

	/* TODO: return value handling */
	return ret;
}

unsigned int ged_gpufreq_bringup(void)
{
	return gpufreq_bringup();
}

void ged_gpufreq_print_tables(void)
{
	int i = 0;

	if (g_mask_table == NULL || g_virtual_table == NULL)
		GED_LOGE("Failed to print core mask table");

	for (i = 0; i < g_avail_mask_num; i++)
		GED_LOGE("[%02d*] MC0%d : 0x%llX",
				i, g_mask_table[i].num, g_mask_table[i].mask);

	for (i = 0; i < g_virtual_oppnum; i++) {
		GED_LOGI("[%02d*] Freq: %d, Volt: %d, Vsram: %d, Vaging: %d",
				i, g_virtual_table[i].freq, g_virtual_table[i].volt,
				g_virtual_table[i].vsram, g_virtual_table[i].vaging);
	}
}

unsigned int ged_gpufreq_get_power_state(void)
{
	return gpufreq_get_power_state();
}

