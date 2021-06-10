
/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/**
 * @file	mtk_eem_platform.c
 * @brief   Driver for EEM
 *
 */
#define __MTK_EEM_PLATFORM_C__

#include <linux/kernel.h>
#include "mtk_eem_config.h"
#include "mtk_eem.h"
#include "mtk_eem_internal_ap.h"
#include "mtk_eem_internal.h"
#include "mtk_cpufreq_api.h"
#ifdef CORN_LOAD
#include "vpu_dvfs.h"
#include "apu_dvfs.h"
#endif
#ifdef CONFIG_MTK_GPU_SUPPORT
#include "mtk_gpufreq.h"
#endif
/*
 * operation of EEM detectors
 */
/* legacy ptp need to define other hook functions */
unsigned int dvtfreq[NR_FREQ] = {0x64, 0x60, 0x59, 0x53, 0x4d, 0x45, 0x3d, 0x39,
		0x35, 0x30, 0x2c, 0x26, 0x1e, 0x18, 0x12, 0x0e};
struct eem_det_ops gpu_det_ops = {
	.get_volt		= get_volt_gpu,
	.set_volt		= set_volt_gpu,
	.restore_default_volt	= restore_default_volt_gpu,
	.get_freq_table		= get_freq_table_gpu,
	.get_orig_volt_table	= get_orig_volt_table_gpu,
};

struct eem_det_ops cpu_det_ops = {
	.get_volt		= get_volt_cpu,
	.set_volt		= set_volt_cpu,
	.restore_default_volt	= restore_default_volt_cpu,
	.get_freq_table		= get_freq_table_cpu,
	.get_orig_volt_table = get_orig_volt_table_cpu,
};

struct eem_det_ops cci_det_ops = {
	.get_volt		= get_volt_cpu,
	.set_volt		= set_volt_cpu,
	.restore_default_volt	= restore_default_volt_cpu,
	.get_freq_table		= get_freq_table_cpu,
	.get_orig_volt_table = get_orig_volt_table_cpu,
};
#if ENABLE_VPU
struct eem_det_ops vpu_det_ops = {
	.get_volt		= get_volt_vpu,
};
#endif

#if ENABLE_MDLA
struct eem_det_ops mdla_det_ops = {
	.get_volt		= get_volt_mdla,
};
#endif

#ifndef EARLY_PORTING_CPU
static unsigned int detid_to_dvfsid(struct eem_det *det)
{
	unsigned int cpudvfsindex;
	enum eem_det_id detid = det_to_id(det);

	if (detid == EEM_DET_L)
		cpudvfsindex = MT_CPU_DVFS_LL;
	else if (detid == EEM_DET_B)
		cpudvfsindex = MT_CPU_DVFS_L;
#if ENABLE_LOO_B
	else if (detid == EEM_DET_B_HI)
		cpudvfsindex = MT_CPU_DVFS_L;
#endif
	else
		cpudvfsindex = MT_CPU_DVFS_CCI;

#if 1
	eem_debug("[%s] id:%d, cpudvfsindex:%d\n", __func__,
		det->ctrl_id, cpudvfsindex);
#endif

	return cpudvfsindex;
}
#endif

/* Will return 10uV */
int get_volt_cpu(struct eem_det *det)
{
	unsigned int value = 0;
	enum eem_det_id cpudvfsindex;

	FUNC_ENTER(FUNC_LV_HELP);
	/* unit mv * 100 = 10uv */
	cpudvfsindex = detid_to_dvfsid(det);
	value = mt_cpufreq_get_cur_volt(cpudvfsindex);

	FUNC_EXIT(FUNC_LV_HELP);
	eem_debug("proc voltage = %d~~~\n", value);
	return value;
}

/* volt_tbl_pmic is convert from 10uV */
int set_volt_cpu(struct eem_det *det)
{
	int value = 0;
	int errcheck = 0;
	enum eem_det_id cpudvfsindex;

	FUNC_ENTER(FUNC_LV_HELP);

	mutex_lock(&record_mutex);

	for (value = 0; value < det->num_freq_tbl; value++) {
		record_tbl_locked[value] =
			det->volt_tbl_pmic[value];
	}

	if (record_tbl_locked[0] < det->volt_tbl_orig[15])
		errcheck = 1;

	if (errcheck == 0) {
		cpudvfsindex = detid_to_dvfsid(det);
		value = mt_cpufreq_update_volt(cpudvfsindex,
				record_tbl_locked, det->num_freq_tbl);
	} else
		WARN_ON(errcheck);
#if 0
	cpudvfsindex = detid_to_dvfsid(det);
	value = mt_cpufreq_update_volt(cpudvfsindex,
			record_tbl_locked, det->num_freq_tbl);
#endif
#if 0
	/*
	 *eem_debug("[set_volt_cpu %s].volt_tbl[0] = 0x%X
	 *		----- Ori[0x%x] volt_tbl_pmic[0] = 0x%X (%d)\n",
	 *	det->name,
	 *	det->volt_tbl[0], det->volt_tbl_orig[0],
	 *	det->volt_tbl_pmic[0], det->ops->pmic_2_volt(det,
	 *		det->volt_tbl_pmic[0]));
	 * eem_debug("[set_volt_cpu %s].volt_tbl[7] = 0x%X
	 *	----- Ori[0x%x] volt_tbl_pmic[7] = 0x%X (%d)\n",
	 *	det->name,
	 *	det->volt_tbl[7], det->volt_tbl_orig[7],
	 *	det->volt_tbl_pmic[7], det->ops->pmic_2_volt(det,
	 *		det->volt_tbl_pmic[7]));
	 *eem_debug("[set_volt_cpu %s].volt_tbl[8] = 0x%X
	 *		----- Ori[0x%x] volt_tbl_pmic[8] = 0x%X (%d)\n",
	 *	det->name,
	 *	det->volt_tbl[8], det->volt_tbl_orig[8],
	 *	det->volt_tbl_pmic[8], det->ops->pmic_2_volt(det,
	 *		det->volt_tbl_pmic[8]));
	 */
#endif

	mutex_unlock(&record_mutex);

	FUNC_EXIT(FUNC_LV_HELP);

	return value;

}

void restore_default_volt_cpu(struct eem_det *det)
{
#if SET_PMIC_VOLT_TO_DVFS
	int value = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	switch (det_to_id(det)) {
	case EEM_DET_L:
		value = mt_cpufreq_update_volt(MT_CPU_DVFS_LL,
				det->volt_tbl_orig, det->num_freq_tbl);
		break;

	case EEM_DET_B:
		value = mt_cpufreq_update_volt(MT_CPU_DVFS_L,
				det->volt_tbl_orig, det->num_freq_tbl);
		break;

	case EEM_DET_CCI:
		value = mt_cpufreq_update_volt(MT_CPU_DVFS_CCI,
				det->volt_tbl_orig, det->num_freq_tbl);
		break;
	}

	FUNC_EXIT(FUNC_LV_HELP);
#endif /*if SET_PMIC_VOLT_TO_DVFS*/
}

void get_freq_table_cpu(struct eem_det *det)
{
	int i = 0, curfreq = 0;
	enum mt_cpu_dvfs_id cpudvfsindex;


	FUNC_ENTER(FUNC_LV_HELP);

	cpudvfsindex = detid_to_dvfsid(det);

	for (i = 0; i < NR_FREQ_CPU; i++) {

#if DVT
		det->freq_tbl[i] = dvtfreq[i];
#else
		curfreq = mt_cpufreq_get_freq_by_idx
			(cpudvfsindex, i);

		det->freq_tbl[i] = PERCENT(curfreq, det->max_freq_khz);
#endif
#if 1
		eem_debug("@@ %s freq_tbl[%d]=%d 0x%0x\n", det->name, i,
			det->freq_tbl[i], det->freq_tbl[i]);
#endif
		if (det->freq_tbl[i] == 0)
			break;
	}

	det->num_freq_tbl = i;

#if ENABLE_LOO_B
	/* Find 2line turn point */
	if (cpudvfsindex == MT_CPU_DVFS_L) {
		for (i = 0; i < det->num_freq_tbl; i++) {
			curfreq = mt_cpufreq_get_freq_by_idx
			(cpudvfsindex, i);
			if (curfreq <= B_M_FREQ_BASE) {
				det->turn_pt = i;
				break;
			}
		}
	}
#endif
	eem_debug("[%s] freq_num:%d, max_freq=%d, turn_pt:%d\n",
			det->name+8, det->num_freq_tbl, det->max_freq_khz,
			det->turn_pt);

	FUNC_EXIT(FUNC_LV_HELP);
}


/* get original volt from cpu dvfs, and apply this table to dvfs
 *   when ptp need to restore volt
 */
void get_orig_volt_table_cpu(struct eem_det *det)
{
#if SET_PMIC_VOLT_TO_DVFS
	int i = 0, volt = 0;
	enum mt_cpu_dvfs_id cpudvfsindex;

	FUNC_ENTER(FUNC_LV_HELP);
	cpudvfsindex = detid_to_dvfsid(det);

	for (i = 0; i < det->num_freq_tbl; i++) {
		volt = mt_cpufreq_get_volt_by_idx(cpudvfsindex, i);

		det->volt_tbl_orig[i] = det->ops->volt_2_pmic(det, volt);

#if 0
		eem_debug("[%s]@@volt_tbl_orig[%d] = %d(0x%x)\n",
			det->name+8,
			i,
			volt,
			det->volt_tbl_orig[i]);
#endif
	}

#if ENABLE_LOO
	/* Use signoff volt */
	memcpy(det->volt_tbl, det->volt_tbl_orig, sizeof(det->volt_tbl));
	memcpy(det->volt_tbl_init2, det->volt_tbl_orig, sizeof(det->volt_tbl));
#endif
	FUNC_EXIT(FUNC_LV_HELP);
#endif
}

int get_volt_gpu(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

#ifdef CONFIG_MTK_GPU_SUPPORT
	/* eem_debug("get_volt_gpu=%d\n",mt_gpufreq_get_cur_volt()); */

	return mt_gpufreq_get_cur_volt(); /* unit  mv * 100 = 10uv */

#else
	return 0;
#endif
	FUNC_EXIT(FUNC_LV_HELP);
}

int set_volt_gpu(struct eem_det *det)
{
	int i;
	unsigned int output[NR_FREQ_GPU];

	for (i = 0; i < det->num_freq_tbl; i++) {
		output[i] = det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]);
#if 0
		eem_error("set_volt_[%s]=0x%x(%d), ",
		det->name,
		det->volt_tbl_pmic[i],
		det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));
#endif
	}

	return mt_gpufreq_update_volt(output, det->num_freq_tbl);
}

void restore_default_volt_gpu(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

#ifdef CONFIG_MTK_GPU_SUPPORT
	mt_gpufreq_restore_default_volt();
#endif

	FUNC_EXIT(FUNC_LV_HELP);
}

void get_freq_table_gpu(struct eem_det *det)
{
#ifdef CONFIG_MTK_GPU_SUPPORT
	int i = 0, curfreq = 0;

	memset(det->freq_tbl, 0, sizeof(det->freq_tbl));

	FUNC_ENTER(FUNC_LV_HELP);
	eem_debug("In gpu freq\n");

	for (i = 0; i < NR_FREQ_GPU; i++) {
#if DVT
		det->freq_tbl[i] = dvtfreq[i];
#else
		curfreq = mt_gpufreq_get_freq_by_real_idx
				(mt_gpufreq_get_ori_opp_idx(i));
		det->freq_tbl[i] = PERCENT(curfreq,
					det->max_freq_khz);
#endif
#if 0
		eem_error("@@freq_tbl_gpu[%d]=%d, (%d) gpu_map[%d] = %d\n",
		i,
		det->freq_tbl[i],
		mt_gpufreq_get_freq_by_idx(i),
		mt_gpufreq_get_ori_opp_idx(i),
		mt_gpufreq_get_freq_by_real_idx(mt_gpufreq_get_ori_opp_idx(i)));
#endif
		if (det->freq_tbl[i] == 0)
			break;
	}

	det->num_freq_tbl = i;

	gpu_vb_turn_pt = 0;

	for (i = 0; i < det->num_freq_tbl; i++) {
		curfreq = mt_gpufreq_get_freq_by_real_idx
			(mt_gpufreq_get_ori_opp_idx(i));
		if (curfreq <= GPU_FREQ_BASE) {
			gpu_vb_turn_pt = i;
			break;
		}
	}

#if ENABLE_LOO_G
	/* Find 2line turn point */
	for (i = 0; i < det->num_freq_tbl; i++) {
		curfreq = mt_gpufreq_get_freq_by_real_idx
			(mt_gpufreq_get_ori_opp_idx(i));
		if (curfreq <= GPU_M_FREQ_BASE) {
			det->turn_pt = i;
			break;
		}
	}
#endif

	eem_debug("[%s] freq_num:%d, max_freq=%d, turn_pt:%d\n",
		det->name+8, det->num_freq_tbl,
		det->max_freq_khz, det->turn_pt);
#endif

	FUNC_EXIT(FUNC_LV_HELP);
}

void get_orig_volt_table_gpu(struct eem_det *det)
{
#ifdef CONFIG_MTK_GPU_SUPPORT
#if SET_PMIC_VOLT_TO_DVFS
	int i = 0, volt = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	for (i = 0; i < det->num_freq_tbl; i++) {
		volt = mt_gpufreq_get_volt_by_real_idx
			(mt_gpufreq_get_ori_opp_idx(i));
		det->volt_tbl_orig[i] = det->ops->volt_2_pmic(det, volt);

#if 0
		eem_debug("@@[%s]volt_tbl_orig[%d] = %d(0x%x)\n",
			det->name+8,
			i,
			volt,
			det->volt_tbl_orig[i]);
#endif
	}

#if ENABLE_LOO_G
		/* Use signoff volt */
		memcpy(det->volt_tbl, det->volt_tbl_orig,
				sizeof(det->volt_tbl));
		memcpy(det->volt_tbl_init2, det->volt_tbl_orig,
				sizeof(det->volt_tbl));
#endif

	FUNC_EXIT(FUNC_LV_HELP);
#endif
#endif
}


int get_volt_vpu(struct eem_det *det)
{
#if ENABLE_VPU
	FUNC_ENTER(FUNC_LV_HELP);

#ifdef EARLY_PORTING_VPU
	return 0;
#else
	return vvpu_get_cur_volt();
#endif
	FUNC_EXIT(FUNC_LV_HELP);
#endif
	return 0;
}

int get_volt_mdla(struct eem_det *det)
{
#if ENABLE_VPU
	FUNC_ENTER(FUNC_LV_HELP);

#ifdef EARLY_PORTING_VPU
	return 0;
#else
	return vmdla_get_cur_volt();
#endif
	FUNC_EXIT(FUNC_LV_HELP);
#endif
	return 0;
}

/************************************************
 * common det operations for legacy and sspm ptp
 ************************************************
 */
int base_ops_volt_2_pmic(struct eem_det *det, int volt)
{
#if 0
	eem_debug("[%s][%s] volt = %d, pmic = %x\n",
		__func__,
		((char *)(det->name) + 8),
		volt,
		(((volt) - det->pmic_base +
			det->pmic_step - 1) / det->pmic_step)
		);
#endif
	return (((volt) - det->pmic_base
			+ det->pmic_step - 1) / det->pmic_step);
}

int base_ops_volt_2_eem(struct eem_det *det, int volt)
{
#if 0
	eem_debug("[%s][%s] volt = %d, eem = %x\n",
		__func__,
		((char *)(det->name) + 8),
		volt,
		(((volt) - det->eem_v_base + det->eem_step - 1) / det->eem_step)
		);
#endif
	return (((volt) - det->eem_v_base + det->eem_step - 1) / det->eem_step);

}

int base_ops_pmic_2_volt(struct eem_det *det, int pmic_val)
{
#if 0
	eem_debug("[%s][%s] pmic = %x, volt = %d\n",
		__func__,
		((char *)(det->name) + 8),
		pmic_valKERN_ERR,
		(((pmic_val) * det->pmic_step) + det->pmic_base)
		);
#endif
	return (((pmic_val) * det->pmic_step) + det->pmic_base);
}

int base_ops_eem_2_pmic(struct eem_det *det, int eem_val)
{
#if 0
	eem_debug("[%s][%s] eem_val = 0x%x, base = %d, pmic = %x\n",
		__func__,
		((char *)(det->name) + 8),
		eem_val,
		det->pmic_base,
		((((eem_val) * det->eem_step) + det->eem_v_base -
			det->pmic_base + det->pmic_step - 1) / det->pmic_step)
		);
#endif
	return ((((eem_val) * det->eem_step) + det->eem_v_base -
			det->pmic_base + det->pmic_step - 1) / det->pmic_step);
}
#undef __MTK_EEM_PLATFORM_C__
