
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
/*
 * operation of EEM detectors
 */
/* legacy ptp need to define other hook functions */
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

#ifndef EARLY_PORTING_CPU
static unsigned int detid_to_dvfsid(struct eem_det *det)
{
	unsigned int cpudvfsindex;
	enum eem_det_id detid = det_to_id(det);

	if (detid == EEM_DET_L)
		cpudvfsindex = MT_CPU_DVFS_L;
	else if (detid == EEM_DET_2L)
		cpudvfsindex = MT_CPU_DVFS_LL;
#if ENABLE_LOO
	else if (detid == EEM_DET_L_HI)
		cpudvfsindex = MT_CPU_DVFS_L;
	else if (detid == EEM_DET_2L_HI)
		cpudvfsindex = MT_CPU_DVFS_LL;
#endif
	else
		cpudvfsindex = MT_CPU_DVFS_CCI;

#if 0
	eem_debug("[detid_to_dvfsid] id:%d, cpudvfsindex:%d\n",
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
	return value;
}

/* volt_tbl_pmic is convert from 10uV */
int set_volt_cpu(struct eem_det *det)
{
	int value = 0;
	enum eem_det_id cpudvfsindex;
#ifdef DRCC_SUPPORT
	unsigned long flags;
#endif

	FUNC_ENTER(FUNC_LV_HELP);

#if 0
	eem_debug("init02_vop_30 = 0x%x\n", det->vop30[EEM_PHASE_INIT02]);
#endif

#ifdef DRCC_SUPPORT
	mt_record_lock(&flags);

	for (value = 0; value < NR_FREQ; value++)
		record_tbl_locked[value] = min(
		(unsigned int)(det->volt_tbl_pmic[value] +
			det->volt_offset_drcc[value]),
		det->volt_tbl_orig[value]);
#else
	mutex_lock(&record_mutex);

	for (value = 0; value < NR_FREQ; value++)
		record_tbl_locked[value] = det->volt_tbl_pmic[value];
#endif

	cpudvfsindex = detid_to_dvfsid(det);
	value = mt_cpufreq_update_volt(cpudvfsindex, record_tbl_locked,
		det->num_freq_tbl);
#if 0
	eem_debug(
"[%s].volt_tbl[0]=0x%X - Ori[0x%x] volt_tbl_pmic[0]=0x%X (%d)\n",
det->name,
det->volt_tbl[0], det->volt_tbl_orig[0],
det->volt_tbl_pmic[0], det->ops->pmic_2_volt(det, det->volt_tbl_pmic[0]));
	eem_debug(
"[%s].volt_tbl[7]=0x%X - Ori[0x%x] volt_tbl_pmic[7]=0x%X (%d)\n",
det->name,
det->volt_tbl[7], det->volt_tbl_orig[7],
det->volt_tbl_pmic[7], det->ops->pmic_2_volt(det, det->volt_tbl_pmic[7]));
	eem_debug(
"[%s].volt_tbl[8]=0x%X - Ori[0x%x] volt_tbl_pmic[8]=0x%X (%d)\n",
det->name,
det->volt_tbl[8], det->volt_tbl_orig[8],
det->volt_tbl_pmic[8], det->ops->pmic_2_volt(det, det->volt_tbl_pmic[8]));
#endif

#ifdef DRCC_SUPPORT
	mt_record_unlock(&flags);
#else
	mutex_unlock(&record_mutex);
#endif

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
		value = mt_cpufreq_update_volt(MT_CPU_DVFS_L,
			det->volt_tbl_orig,
			det->num_freq_tbl);
		break;

	case EEM_DET_2L:
		value = mt_cpufreq_update_volt(MT_CPU_DVFS_LL,
			det->volt_tbl_orig,
			det->num_freq_tbl);
		break;

	case EEM_DET_CCI:
		value = mt_cpufreq_update_volt(MT_CPU_DVFS_CCI,
			det->volt_tbl_orig,
			det->num_freq_tbl);
		break;
	}

	FUNC_EXIT(FUNC_LV_HELP);
#endif /*if SET_PMIC_VOLT_TO_DVFS*/
}

void get_freq_table_cpu(struct eem_det *det)
{
	int i = 0;
	enum mt_cpu_dvfs_id cpudvfsindex;


	FUNC_ENTER(FUNC_LV_HELP);

	cpudvfsindex = detid_to_dvfsid(det);

	for (i = 0; i < NR_FREQ_CPU; i++) {
		det->freq_tbl[i] =
			PERCENT(mt_cpufreq_get_freq_by_idx(cpudvfsindex, i),
			det->max_freq_khz);
		#if 0
		eem_debug("freq_tbl[%d]=%d 0x%0x\n", i, det->freq_tbl[i],
			det->freq_tbl[i]);
		#endif
		if (det->freq_tbl[i] == 0)
			break;
	}

	det->num_freq_tbl = i;

	/*
	 * eem_debug("[%s] freq_num:%d, max_freq=%d\n", det->name+8,
	 * det->num_freq_tbl, det->max_freq_khz);
	 * for (i = 0; i < NR_FREQ; i++)
	 *	eem_debug("%d\n", det->freq_tbl[i]);
	 */
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
		eem_debug("[%s]volt_tbl_orig[%d] = %d(0x%x)\n",
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
	(((volt) - det->pmic_base + det->pmic_step - 1) / det->pmic_step)
	);
#endif
	return (((volt) - det->pmic_base + det->pmic_step - 1) /
		det->pmic_step);
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
	return (((volt) - det->eem_v_base + det->eem_step - 1) /
		det->eem_step);
}

int base_ops_pmic_2_volt(struct eem_det *det, int pmic_val)
{
#if 0
	eem_debug("[%s][%s] pmic = %x, volt = %d\n",
		__func__,
		((char *)(det->name) + 8),
		pmic_val,
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
