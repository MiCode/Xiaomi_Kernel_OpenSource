// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/**
 * @file	mtk_eemg_platform.c
 * @brief   Driver for EEM
 *
 */
#define __MTK_EEMG_PLATFORM_C__

#include <linux/kernel.h>
#include "mtk_eemgpu_config.h"
#include "mtk_eemgpu.h"
#include "mtk_eemgpu_internal_ap.h"
#include "mtk_eemgpu_internal.h"
#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
#include "mtk_gpufreq.h"
#endif

/*
 * operation of EEM detectors
 */

/* legacy ptp need to define other hook functions */
unsigned int dvtgpufreq[NR_FREQ] = {
	0x64, 0x60, 0x59, 0x53, 0x4d, 0x45, 0x3d, 0x39,
	0x35, 0x30, 0x2c, 0x26, 0x1e, 0x18, 0x12, 0x0e
};

struct eemg_det_ops gpu_det_ops = {
	.get_volt_gpu		= get_volt_gpu,
	.set_volt_gpu		= set_volt_gpu,
	.restore_default_volt_gpu	= restore_default_volt_gpu,
	.get_freq_table_gpu		= get_freq_table_gpu,
	.get_orig_volt_table_gpu	= get_orig_volt_table_gpu,
};

int get_volt_gpu(struct eemg_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
	/* eemg_debug("get_volt_gpu=%d\n",mt_gpufreq_get_cur_volt()); */

	return mt_gpufreq_get_cur_volt(); /* unit  mv * 100 = 10uv */

#else
	return 0;
#endif
	FUNC_EXIT(FUNC_LV_HELP);
}

int set_volt_gpu(struct eemg_det *det)
{
	int i;
	unsigned int output[NR_FREQ_GPU];

	for (i = 0; i < det->num_freq_tbl; i++)
		output[i] = det->ops->pmic_2_volt_gpu(det, det->volt_tbl_pmic[i]);

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
	return mt_gpufreq_update_volt(output, det->num_freq_tbl);
#else
	return 0;
#endif
}

void restore_default_volt_gpu(struct eemg_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
	mt_gpufreq_restore_default_volt();
#endif

	FUNC_EXIT(FUNC_LV_HELP);
}

void get_freq_table_gpu(struct eemg_det *det, unsigned int gpu_freq_base,
	unsigned int gpu_m_freq_base)
{
#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
	int i = 0, curfreq = 0;

	memset(det->freq_tbl, 0, sizeof(det->freq_tbl));
	FUNC_ENTER(FUNC_LV_HELP);
	eemg_debug("In gpu freq\n");

	for (i = 0; i < NR_FREQ_GPU; i++) {
		curfreq = mt_gpufreq_get_freq_by_real_idx
				(mt_gpufreq_get_ori_opp_idx(i));
		det->freq_tbl[i] = PERCENT(curfreq,
					det->max_freq_khz);
		if (det->freq_tbl[i] == 0)
			break;
	}

	det->num_freq_tbl = i;
	if (gpu_vb != 0) {
		gpu_vb_turn_pt = 0;
		for (i = 0; i < det->num_freq_tbl; i++) {
			curfreq = mt_gpufreq_get_freq_by_real_idx
			(mt_gpufreq_get_ori_opp_idx(i));
			if (curfreq <= gpu_freq_base) {
				gpu_vb_turn_pt = i;
				break;
			}
		}
	}
	/* Find 2line turn point */
	for (i = 0; i < det->num_freq_tbl; i++) {
		curfreq = mt_gpufreq_get_freq_by_real_idx
			(mt_gpufreq_get_ori_opp_idx(i));
		if (curfreq <= gpu_m_freq_base) {
			det->turn_pt = i;
			break;
		}
	}

	eemg_debug("[%s] freq_num:%d, max_freq=%d, turn_pt:%d\n",
		det->name+8, det->num_freq_tbl,
		det->max_freq_khz, det->turn_pt);
#endif

	FUNC_EXIT(FUNC_LV_HELP);
}

void get_orig_volt_table_gpu(struct eemg_det *det)
{
#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
#if SET_PMIC_VOLT_TO_DVFS
	int i = 0, volt = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	for (i = 0; i < det->num_freq_tbl; i++) {
		volt = mt_gpufreq_get_volt_by_real_idx
			(mt_gpufreq_get_ori_opp_idx(i));
		det->volt_tbl_orig[i] = det->ops->volt_2_pmic_gpu(det, volt);
	}

	/* Use signoff volt */
	memcpy(det->volt_tbl, det->volt_tbl_orig,
		sizeof(det->volt_tbl));
	memcpy(det->volt_tbl_init2, det->volt_tbl_orig,
		sizeof(det->volt_tbl));

	FUNC_EXIT(FUNC_LV_HELP);
#endif
#endif
}

/************************************************
 * common det operations for legacy and sspm ptp
 ************************************************
 */
int base_ops_volt_2_pmic_gpu(struct eemg_det *det, int volt)
{
	return (((volt) - det->pmic_base
			+ det->pmic_step - 1) / det->pmic_step);
}

int base_ops_volt_2_eemg(struct eemg_det *det, int volt)
{
	return (((volt) - det->eemg_v_base + det->eemg_step - 1) /
		det->eemg_step);

}

int base_ops_pmic_2_volt_gpu(struct eemg_det *det, int pmic_val)
{
	return (((pmic_val) * det->pmic_step) + det->pmic_base);
}

int base_ops_eemg_2_pmic(struct eemg_det *det, int eemg_val)
{
	return ((((eemg_val) * det->eemg_step) + det->eemg_v_base -
			det->pmic_base + det->pmic_step - 1) / det->pmic_step);
}
#undef __MTK_EEMG_PLATFORM_C__
