// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <ged_base.h>
#include <ged_dcs.h>
#include <ged_log.h>

#if defined(CONFIG_MTK_GPUFREQ_V2)
#include <ged_gpufreq_v2.h>
#include <gpufreq_v2.h>
#else
#include <ged_gpufreq_v1.h>
#endif /* CONFIG_MTK_GPUFREQ_V2 */

static unsigned int g_dcs_enable;
static unsigned int g_dcs_support;
static unsigned int g_dcs_opp_setting;
static struct mutex g_DCS_lock;

int g_cur_core_num;
int g_max_core_num;
int g_avail_mask_num;
int g_virtual_opp_num;

struct gpufreq_core_mask_info *g_core_mask_table;
struct gpufreq_core_mask_info *g_avail_mask_table;

/* Function Pointer hooked by DDK to scale cores */
int (*ged_dvfs_set_gpu_core_mask_fp)(u64 core_mask) = NULL;
EXPORT_SYMBOL(ged_dvfs_set_gpu_core_mask_fp);

static void _dcs_init_core_mask_table(void)
{
	int i = 0;
	struct gpufreq_core_mask_info *mask_table;

	/* init core mask table */
	g_max_core_num = gpufreq_get_core_num();
	g_cur_core_num = g_max_core_num;
	mask_table = gpufreq_get_core_mask_table();
	g_core_mask_table = kcalloc(g_max_core_num,
		sizeof(struct gpufreq_core_mask_info), GFP_KERNEL);

	if (!g_core_mask_table || !mask_table) {
		GED_LOGE("Failed to query core mask from gpufreq");
		g_dcs_enable = 0;
		return;
	}

	for (i = 0; i < g_max_core_num; i++)
		*(g_core_mask_table + i) = *(mask_table + i);

#ifdef GED_KPI_DEBUG
	for (i = 0; i < g_max_core_num; i++) {
		GED_LOGI("[%02d*] MC0%d : 0x%llX",
			i, g_core_mask_table[i].num, g_core_mask_table[i].mask);
	}
#endif /* GED_KPI_DEBUG */

	// return mask_table;
}

GED_ERROR ged_dcs_init_platform_info(void)
{
	struct device_node *dcs_node = NULL;
	int opp_setting = 0;
	int ret = GED_OK;

	mutex_init(&g_DCS_lock);

	dcs_node = of_find_compatible_node(NULL, NULL, "mediatek,gpu_dcs");
	if (unlikely(!dcs_node)) {
		GED_LOGE("Failed to find gpu_dcs node");
		return ret;
	}

	of_property_read_u32(dcs_node, "dcs-policy-support", &g_dcs_support);
	of_property_read_u32(dcs_node, "virtual-opp-support", &g_dcs_opp_setting);

	opp_setting = g_dcs_opp_setting;

	if (!g_dcs_support) {
		GED_LOGE("DCS policy not support");
		return ret;
	}

	while (opp_setting) {
		g_avail_mask_num += opp_setting & 1;
		opp_setting >>= 1;
	}
	g_dcs_enable = 1;

	GED_LOGI("g_dcs_enable: %u,  g_dcs_opp_setting: 0x%X",
			g_dcs_enable, g_dcs_opp_setting);

	_dcs_init_core_mask_table();

	return ret;
}

void ged_dcs_exit(void)
{
	mutex_destroy(&g_DCS_lock);

	kfree(g_core_mask_table);
	kfree(g_avail_mask_table);
}

struct gpufreq_core_mask_info *dcs_get_avail_mask_table(void)
{
	int i, j = 0;
	u32 iter = 0;

	if (!g_dcs_opp_setting)
		return g_core_mask_table;

	if (g_avail_mask_table)
		return g_avail_mask_table;

	/* mapping selected core mask */
	g_avail_mask_table = kcalloc(g_avail_mask_num,
		sizeof(struct gpufreq_core_mask_info), GFP_KERNEL);

	iter = 1 << (g_max_core_num - 1);

	for (i = 0; i < g_max_core_num; i++) {
		if (g_dcs_opp_setting & iter) {
			*(g_avail_mask_table + j) = *(g_core_mask_table + i);
			j++;
		}
		iter >>= 1;
	}

#ifdef GED_KPI_DEBUG
	for (i = 0; i < g_avail_mask_num; i++) {
		GED_LOGI("[%02d*] MC0%d : 0x%llX",
			i, g_avail_mask_table[i].num, g_avail_mask_table[i].mask);
	}
#endif /* GED_KPI_DEBUG */

	return g_avail_mask_table;
}

int dcs_get_dcs_opp_setting(void)
{
	return g_dcs_opp_setting;
}

int dcs_get_cur_core_num(void)
{
	return g_cur_core_num;
}

int dcs_get_max_core_num(void)
{
	return g_max_core_num;
}

int dcs_get_avail_mask_num(void)
{
	return g_avail_mask_num;
}

int dcs_set_core_mask(unsigned int core_mask, unsigned int core_num)
{
	int ret = GED_OK;

	mutex_lock(&g_DCS_lock);


	if (!g_dcs_enable || g_cur_core_num == core_num)
		goto done_unlock;

	if (!g_core_mask_table) {
		ret = GED_ERROR_FAIL;
		GED_LOGE("null core mask table");
		goto done_unlock;
	}

	ged_dvfs_set_gpu_core_mask_fp(core_mask);
	g_cur_core_num = core_num;
	Policy__DCS(g_max_core_num, g_cur_core_num);
	Policy__DCS__Detail(core_mask);
	/* TODO: set return error */
	if (ret) {
		GED_LOGE("Failed to set core_mask: 0x%llX, core_num: %u", core_mask, core_num);
		goto done_unlock;
	}

done_unlock:
	mutex_unlock(&g_DCS_lock);
	return ret;
}

int dcs_restore_max_core_mask(void)
{
	int ret = GED_OK;

	mutex_lock(&g_DCS_lock);

	if (!g_dcs_enable || g_cur_core_num == g_max_core_num)
		goto done_unlock;

	if (g_core_mask_table == NULL) {
		ret = GED_ERROR_FAIL;
		GED_LOGE("null core mask table");
		goto done_unlock;
	}

	ged_dvfs_set_gpu_core_mask_fp(g_core_mask_table[0].mask);
	g_cur_core_num = g_max_core_num;
	Policy__DCS(g_max_core_num, g_cur_core_num);
	Policy__DCS__Detail(g_core_mask_table[0].mask);

done_unlock:
	mutex_unlock(&g_DCS_lock);
	return ret;
}

int is_dcs_enable(void)
{
	return g_dcs_enable;
}

void dcs_enable(int enable)
{
	if (g_core_mask_table == NULL)
		return;

	mutex_lock(&g_DCS_lock);

	if (enable)
		g_dcs_enable = enable;
	else {
		ged_dvfs_set_gpu_core_mask_fp(g_core_mask_table[0].mask);
		g_cur_core_num = g_max_core_num;
		g_dcs_enable = 0;
		Policy__DCS(g_max_core_num, g_cur_core_num);
		Policy__DCS__Detail(g_core_mask_table[0].mask);
	}
	mutex_unlock(&g_DCS_lock);
}
EXPORT_SYMBOL(dcs_enable);
