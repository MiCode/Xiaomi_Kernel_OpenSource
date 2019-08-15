// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include "mtk_gpufreq.h"
#include "thermal_budget_platform.h"
#include "thermal_budget.h"

void eara_thrm_update_gpu_info(int *input_opp_num, int *in_max_opp_idx,
			struct mt_gpufreq_power_table_info **gpu_tbl,
			struct thrm_pb_ratio **opp_ratio)
{
	/* should be locked unless init */

	struct mt_gpufreq_power_table_info *tbl = NULL;
	int opp_num = *input_opp_num;

	if (!(*gpu_tbl)) {
		/*
		 * Bind @thr_gpu_tbl, @g_opp_ratio and @g_gpu_opp_num together.
		 * JUST check one of them to know if initialized or not.
		 */

		opp_num = mt_gpufreq_get_dvfs_table_num();
		if (!opp_num) {
			*input_opp_num = 0;
			return;
		}

		*gpu_tbl = kcalloc(opp_num,
				sizeof(struct mt_gpufreq_power_table_info),
				GFP_KERNEL);
		*opp_ratio = kcalloc(opp_num,
				sizeof(struct thrm_pb_ratio), GFP_KERNEL);
		if (!(*gpu_tbl) || !(*opp_ratio)) {
			kfree(*gpu_tbl);
			*gpu_tbl = NULL;
			kfree(*opp_ratio);
			*opp_ratio = NULL;
			*input_opp_num = 0;
			return;
		}

		*input_opp_num = opp_num;
	}

	tbl = pass_gpu_table_to_eara();
	if (!tbl)
		return;

	memcpy((*gpu_tbl), tbl, opp_num * sizeof(*tbl));
}

int eara_thrm_get_vpu_core_num(void)
{
#ifdef CONFIG_MTK_VPU_SUPPORT
	return 2;
#else
	return 0;
#endif
}

int eara_thrm_get_mdla_core_num(void)
{
#ifdef CONFIG_MTK_MDLA_SUPPORT
	return 1;
#else
	return 0;
#endif
}

int eara_thrm_get_nr_clusters(void)
{
	return 2;
}

unsigned int eara_thrm_get_freq_by_idx(int cluster, int opp)
{
	unsigned int freq = 0;

	if (cluster == 0) {
		switch (opp) {
		case 0:
			freq = 2000000;
			break;
		case 1:
			freq = 1933000;
			break;
		case 2:
			freq = 1866000;
			break;
		case 3:
			freq = 1800000;
			break;
		case 4:
			freq = 1733000;
			break;
		case 5:
			freq = 1666000;
			break;
		case 6:
			freq = 1548000;
			break;
		case 7:
			freq = 1475000;
			break;
		case 8:
			freq = 1375000;
			break;
		case 9:
			freq = 1275000;
			break;
		case 10:
			freq = 1175000;
			break;
		case 11:
			freq = 1075000;
			break;
		case 12:
			freq = 999000;
			break;
		case 13:
			freq = 925000;
			break;
		case 14:
			freq = 850000;
			break;
		case 15:
			freq = 774000;
			break;
		}
	} else if (cluster == 1) {
		switch (opp) {
		case 0:
			freq = 2200000;
			break;
		case 1:
			freq = 2133000;
			break;
		case 2:
			freq = 2066000;
			break;
		case 3:
			freq = 2000000;
			break;
		case 4:
			freq = 1933000;
			break;
		case 5:
			freq = 1866000;
			break;
		case 6:
			freq = 1800000;
			break;
		case 7:
			freq = 1651000;
			break;
		case 8:
			freq = 1503000;
			break;
		case 9:
			freq = 1414000;
			break;
		case 10:
			freq = 1295000;
			break;
		case 11:
			freq = 1176000;
			break;
		case 12:
			freq = 1087000;
			break;
		case 13:
			freq = 998000;
			break;
		case 14:
			freq = 909000;
			break;
		case 15:
			freq = 850000;
			break;
		}
	}

	return freq;
}

