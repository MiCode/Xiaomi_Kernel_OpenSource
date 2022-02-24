/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include "mtk_gpufreq.h"
#include "thermal_internal.h"

#if defined(CONFIG_MTK_VPU_SUPPORT)
#include "vpu_dvfs.h"
#endif

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
			*in_max_opp_idx = 0;
			return;
		}

		*in_max_opp_idx = mt_gpufreq_get_seg_max_opp_index();
		if (*in_max_opp_idx + opp_num
			!= mt_gpufreq_get_real_dvfs_table_num()) {
			*input_opp_num = 0;
			*in_max_opp_idx = 0;
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
			*in_max_opp_idx = 0;
			return;
		}

		*input_opp_num = opp_num;
	}

	tbl = pass_gpu_table_to_eara();
	if (!tbl)
		return;

	memcpy((*gpu_tbl), &tbl[*in_max_opp_idx], opp_num * sizeof(*tbl));
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
	return 0;
}

int eara_thrm_vpu_opp_to_freq(int opp)
{
#if defined(CONFIG_MTK_VPU_SUPPORT)
	return get_vpu_opp_to_freq(opp);
#else
	return 100;
#endif
}

int eara_thrm_mdla_opp_to_freq(int opp)
{
	return 100;
}

int eara_thrm_apu_ready(void)
{
	return 1;
}

int eara_thrm_vpu_onoff(void)
{
	return 0;
}

int eara_thrm_mdla_onoff(void)
{
	return 0;
}

int eara_thrm_keep_little_core(void)
{
	return 1;
}

