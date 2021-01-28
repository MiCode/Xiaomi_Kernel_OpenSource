
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

#include "mtk_eem.h"
#include "mtk_eem_config.h"
#if !(EEM_ENABLE_TINYSYS_SSPM)
	#include "mtk_eem_internal_ap.h"
#else
	#include "mtk_eem_internal_sspm.h"
#endif
#include "mtk_eem_internal.h"
#ifndef EARLY_PORTING_CPU
#include "mtk_cpufreq_config.h"
#include "mtk_cpufreq_api.h"
#endif
#ifndef EARLY_PORTING_GPU
#include "mtk_gpufreq.h"
#endif
#if defined(CONFIG_MTK_PMIC_CHIP_MT6335)
	#include "include/pmic_regulator.h"
	#include "mtk_pmic_regulator.h"
#endif


/*
 * operation of EEM detectors
 */

#if (EEM_ENABLE_TINYSYS_SSPM)
/* sspm ptp only have pmic transfer related hook functions */
struct eem_det_ops big_det_ops;
struct eem_det_ops gpu_det_ops;
struct eem_det_ops soc_det_ops;
struct eem_det_ops little_det_ops;
struct eem_det_ops dual_little_det_ops;
struct eem_det_ops cci_det_ops;
#else
/* legacy ptp need to define other hook functions */
struct eem_det_ops big_det_ops = {
	.get_volt		= get_volt_cpu,
	.set_volt		= set_volt_cpu,
	.restore_default_volt	= restore_default_volt_cpu,
	.get_freq_table		= get_freq_table_cpu,
	.get_orig_volt_table = get_orig_volt_table_cpu,
};

struct eem_det_ops gpu_det_ops = {
	.get_volt		= get_volt_gpu,
	.set_volt		= set_volt_gpu,
	.restore_default_volt	= restore_default_volt_gpu,
	.get_freq_table		= get_freq_table_gpu,
	.get_orig_volt_table	= get_orig_volt_table_gpu,
};

struct eem_det_ops soc_det_ops = {
#if EEM_BANK_SOC
	.get_freq_table		= get_freq_table_vcore,
	.get_orig_volt_table = NULL,
	.get_volt		= get_volt_vcore,
	.set_volt		= set_volt_vcore,
	.restore_default_volt	= restore_volt_vcore,
#endif
};

struct eem_det_ops little_det_ops = {
	.get_volt		= get_volt_cpu,
	.set_volt		= set_volt_cpu,
	.restore_default_volt	= restore_default_volt_cpu,
	.get_freq_table		= get_freq_table_cpu,
	.get_orig_volt_table = get_orig_volt_table_cpu,
};

struct eem_det_ops dual_little_det_ops = {
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

#if 0
static struct eem_det_ops lte_det_ops = {
	.get_volt		= get_volt_lte,
	.set_volt		= set_volt_lte,
	.restore_default_volt	= restore_default_volt_lte,
	.get_freq_table		= NULL,
};
#endif

/* Will return 10uV */
int get_volt_cpu(struct eem_det *det)
{
	unsigned int value = 0;

	FUNC_ENTER(FUNC_LV_HELP);
#ifndef EARLY_PORTING_CPU
	/* unit mv * 100 = 10uv */  /* I-Chang */
	switch (det_to_id(det)) {
	case EEM_DET_2L:
		value = mt_cpufreq_get_cur_volt(MT_CPU_DVFS_LL);
		break;

#ifndef CONFIG_MACH_MT6739
	case EEM_DET_L:
		value = mt_cpufreq_get_cur_volt(MT_CPU_DVFS_L);
		break;

	case EEM_DET_CCI:
		value = mt_cpufreq_get_cur_volt(MT_CPU_DVFS_CCI);
		break;
#endif

	default:
		value = 0;
		break;
	}
#endif

	FUNC_EXIT(FUNC_LV_HELP);
	return value;
}

/* volt_tbl_pmic is convert from 10uV */
int set_volt_cpu(struct eem_det *det)
{
	int value = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	/* eem_debug("init02_vop_30 = 0x%x\n", det->vop30[EEM_PHASE_INIT02]); */
	#if SET_PMIC_VOLT_TO_CPU_DVFS
	#ifdef __KERNEL__
	mutex_lock(&record_mutex);
	#endif
	for (value = 0; value < NR_FREQ; value++)
		record_tbl_locked[value] = det->volt_tbl_pmic[value];

	switch (det_to_id(det)) {
	case EEM_DET_2L:
		value = mt_cpufreq_update_volt(MT_CPU_DVFS_LL, record_tbl_locked, det->num_freq_tbl);
		break;
#ifndef CONFIG_MACH_MT6739
	case EEM_DET_L:
		value = mt_cpufreq_update_volt(MT_CPU_DVFS_L, record_tbl_locked, det->num_freq_tbl);
		break;

	case EEM_DET_CCI:
		value = mt_cpufreq_update_volt(MT_CPU_DVFS_CCI, record_tbl_locked, det->num_freq_tbl);
		break;
#endif
	default:
		value = 0;
		break;
	}
	#ifdef __KERNEL__
	mutex_unlock(&record_mutex);
	#endif
	FUNC_EXIT(FUNC_LV_HELP);
	#endif /*if SET_PMIC_VOLT_TO_CPU_DVFS*/
	return value;

}

void restore_default_volt_cpu(struct eem_det *det)
{
	#if SET_PMIC_VOLT_TO_CPU_DVFS
	int value = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	switch (det_to_id(det)) {
	case EEM_DET_2L:
		value = mt_cpufreq_update_volt(MT_CPU_DVFS_LL, det->volt_tbl_orig, det->num_freq_tbl);
		break;
#ifndef CONFIG_MACH_MT6739
	case EEM_DET_L:
		value = mt_cpufreq_update_volt(MT_CPU_DVFS_L, det->volt_tbl_orig, det->num_freq_tbl);
		break;

	case EEM_DET_CCI:
		value = mt_cpufreq_update_volt(MT_CPU_DVFS_CCI, det->volt_tbl_orig, det->num_freq_tbl);
		break;
#endif
	}

	FUNC_EXIT(FUNC_LV_HELP);
	#endif /*if SET_PMIC_VOLT_TO_CPU_DVFS*/
}

void get_freq_table_cpu(struct eem_det *det)
{

	int i = 0;
	#if !(DVT)
	/* unsigned int binLevel = 0;*/
	/* Frequency gethering stopped from DVFS */
	enum mt_cpu_dvfs_id cpu_id;
#ifndef CONFIG_MACH_MT6739
	enum eem_det_id det_id;
#endif
	#endif

	FUNC_ENTER(FUNC_LV_HELP);

	#if DVT
	for (i = 0; i < NR_FREQ; i++) {
		det->freq_tbl[i] = freq[i];
		if (det->freq_tbl[i] == 0)
			break;
	}
	#else

#ifdef CONFIG_MACH_MT6739
	cpu_id = MT_CPU_DVFS_LL;
#else
	det_id = det_to_id(det);
	cpu_id = (det_id == EEM_DET_CCI) ? MT_CPU_DVFS_CCI :
			(det_id == EEM_DET_2L) ? MT_CPU_DVFS_LL :
			MT_CPU_DVFS_L;
#endif

	for (i = 0; i < NR_FREQ_CPU; i++) {
		det->freq_tbl[i] = PERCENT(mt_cpufreq_get_freq_by_idx(cpu_id, i), det->max_freq_khz);
		/* eem_debug("freq_tbl[%d]=%d 0x%0x\n", i, det->freq_tbl[i], det->freq_tbl[i]); */
		if (det->freq_tbl[i] == 0)
			break;
	}
	#endif

	det->num_freq_tbl = i;

	/*
	*eem_debug("[%s] freq_num:%d, max_freq=%d\n", det->name+8, det->num_freq_tbl, det->max_freq_khz);
	*for (i = 0; i < NR_FREQ; i++)
	*	eem_debug("%d\n", det->freq_tbl[i]);
	*/

	FUNC_EXIT(FUNC_LV_HELP);
}

/* get original volt from cpu dvfs, and apply this table to dvfs
*   when ptp need to restore volt
*/
void get_orig_volt_table_cpu(struct eem_det *det)
{
#if SET_PMIC_VOLT_TO_CPU_DVFS
	int i = 0, volt = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	for (i = 0; i < det->num_freq_tbl; i++) {
#ifdef CONFIG_MACH_MT6739
		volt = mt_cpufreq_get_volt_by_idx(MT_CPU_DVFS_LL, i);
#else
		unsigned int det_id = det_to_id(det);

		volt = ((det_id == EEM_DET_2L) ? mt_cpufreq_get_volt_by_idx(MT_CPU_DVFS_LL, i) :
			(det_id == EEM_DET_L) ?  mt_cpufreq_get_volt_by_idx(MT_CPU_DVFS_L, i) :
			mt_cpufreq_get_volt_by_idx(MT_CPU_DVFS_CCI, i));
#endif
		det->volt_tbl_orig[i] = det->ops->volt_2_pmic(det, volt);

		#if 1
		eem_debug("[%s]volt_tbl_orig[%d] = %d(0x%x)\n",
			det->name+8,
			i,
			volt,
			det->volt_tbl_orig[i]);
		#endif
	}
	FUNC_EXIT(FUNC_LV_HELP);
#endif /* SET_PMIC_VOLT_TO_CPU_DVFS */
}

int get_volt_gpu(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	/* eem_debug("get_volt_gpu=%d\n",mt_gpufreq_get_cur_volt()); */
	#ifdef EARLY_PORTING_GPU
	return 0;
	#else
		#if defined(__MTK_SLT_)
			return gpu_dvfs_get_cur_volt();
		#else
			return mt_gpufreq_get_cur_volt(); /* unit  mv * 100 = 10uv */
		#endif
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

	#if !defined(EARLY_PORTING_GPU) && (SET_PMIC_VOLT_TO_GPU_DVFS)
		return mt_gpufreq_update_volt(output, det->num_freq_tbl);
	#else
		return 0;
	#endif
}

void restore_default_volt_gpu(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	#if !defined(EARLY_PORTING_GPU) && (SET_PMIC_VOLT_TO_GPU_DVFS)
	mt_gpufreq_restore_default_volt();
	#endif
	FUNC_EXIT(FUNC_LV_HELP);
}

void get_freq_table_gpu(struct eem_det *det)
{
	int i = 0;

	memset(det->freq_tbl, 0, sizeof(det->freq_tbl));

	FUNC_ENTER(FUNC_LV_HELP);

	#if DVT
	for (i = 0; i < NR_FREQ; i++) {
		det->freq_tbl[i] = freq[i];
		if (det->freq_tbl[i] == 0)
			break;
	}
	#else
		#ifndef EARLY_PORTING_GPU
		for (i = 0; i < NR_FREQ_GPU; i++) {
			det->freq_tbl[i] = PERCENT(mt_gpufreq_get_freq_by_idx(i), det->max_freq_khz);
			#if 0
			eem_debug("freq_tbl_gpu[%d]=%d, (%d)\n",
				i,
				det->freq_tbl[i],
				mt_gpufreq_get_freq_by_idx(i));
			#endif
			if (det->freq_tbl[i] == 0)
				break;
		}
		#endif
	#endif /* if DVT */

	det->num_freq_tbl = i;
	eem_debug("[%s] freq_num:%d, max_freq=%d\n", det->name+8, det->num_freq_tbl, det->max_freq_khz);

	FUNC_EXIT(FUNC_LV_HELP);
}

void get_orig_volt_table_gpu(struct eem_det *det)
{
#if SET_PMIC_VOLT_TO_GPU_DVFS
	int i = 0, volt = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	for (i = 0; i < det->num_freq_tbl; i++) {
		volt = mt_gpufreq_get_volt_by_idx(i);
		det->volt_tbl_orig[i] = det->ops->volt_2_pmic(det, volt);

		#if 0
		eem_debug("[%s]volt_tbl_orig[%d] = %d(0x%x)\n",
			det->name+8,
			i,
			volt,
			det->volt_tbl_orig[i]);
		#endif
	}
	FUNC_EXIT(FUNC_LV_HELP);
#endif
}

/* for DVT */
#if EEM_BANK_SOC
int set_volt_vcore(struct eem_det *det)
{
	int i = 0;

	for (i = 0; i < VCORE_NR_FREQ; i++) {
		eem_vcore[i] = det->volt_tbl_pmic[i];
		/* eem_debug("eem_vcore = 0x%x\n", eem_vcore[i]);*/
	}
	return 0;
}

void restore_volt_vcore(struct eem_det *det)
{
	int i = 0;

	for (i = 0; i < VCORE_NR_FREQ; i++)
		eem_vcore[i] = det->ops->volt_2_pmic(det, (*vcore_opp[i]+eem_vcore_index[i]));
	for (i = VCORE_NR_FREQ - 2; i >= 0; i--)
		eem_vcore[i] = (eem_vcore[i] < eem_vcore[i+1]) ? eem_vcore[i+1] : eem_vcore[i];

	eem_error("got vcore volt(pmic): 0x%x, 0x%x, 0x%x, 0x%x\n",
				eem_vcore[0], eem_vcore[1], eem_vcore[2], eem_vcore[3]);
}

void get_freq_table_vcore(struct eem_det *det)
{
	int i = 0;

	FUNC_ENTER(FUNC_LV_HELP);
	#if DVT
	for (i = 0; i < NR_FREQ; i++) {
		det->freq_tbl[i] = freq[i];
		if (det->freq_tbl[i] == 0)
			break;
	}
	#else
	for (i = 0; i < VCORE_NR_FREQ; i++) {
		det->freq_tbl[i] = vcore_freq[i];
		/* eem_debug("freq_tbl[%d]=%d 0x%0x\n", i, det->freq_tbl[i], det->freq_tbl[i]); */
		if (det->freq_tbl[i] == 0)
			break;
	}
	#endif

	/* eem_debug("NR_FREQ=%d\n", i); */
	det->num_freq_tbl = i;
	FUNC_EXIT(FUNC_LV_HELP);

}

int get_volt_vcore(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);

	#if DVT
	return 0x30;
	#else
	/* eem_debug("get_volt_vcore=%d\n", buck_get_voltage(VCORE)/10); */
	return buck_get_voltage(VCORE)/10; /* unit = 10 uv */
	#endif
}

#endif /* #if EEM_BANK_SOC*/

#if 0 /* non use now */
static int set_volt_lte(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
#ifdef __KERNEL__
	return mt_cpufreq_set_lte_volt(det->volt_tbl_init2[0]);
#else
	return 0;
	/* return dvfs_set_vlte(det->volt_tbl_bin[0]); */
	/* return dvfs_set_vlte(det->volt_tbl_init2[0]); */
#endif
	FUNC_EXIT(FUNC_LV_HELP);
}

static int get_volt_lte(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	return mt_get_cur_volt_lte(); /* unit mv * 100 = 10uv */
	FUNC_EXIT(FUNC_LV_HELP);
}

static void restore_default_volt_lte(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
#ifdef __KERNEL__
	if (mt_get_chip_hw_code() == 0x337)
		mt_cpufreq_set_lte_volt(det->ops->volt_2_pmic(det, 105000));
#else
	/* dvfs_set_vlte(det->ops->volt_2_pmic(det, 105000)); */
#endif
	FUNC_EXIT(FUNC_LV_HELP);
}

static void switch_to_vcore_ao(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	eem_write_field(PERI_VCORE_PTPOD_CON0, VCORE_PTPODSEL, SEL_VCORE_AO);
	eem_write_field(EEMCORESEL, APBSEL, det->ctrl_id);
	eem_ctrls[PTP_CTRL_VCORE].det_id = det_to_id(det);

	FUNC_EXIT(FUNC_LV_HELP);
}

static void switch_to_vcore_pdn(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	eem_write_field(PERI_VCORE_PTPOD_CON0, VCORE_PTPODSEL, SEL_VCORE_PDN);
	eem_write_field(EEMCORESEL, APBSEL, det->ctrl_id);
	eem_ctrls[EEM_CTRL_SOC].det_id = det_to_id(det);

	FUNC_EXIT(FUNC_LV_HELP);
}

#ifndef __KERNEL__
static int set_volt_vcore(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);
	#if defined(__MTK_SLT_)
		#if defined(SLT_VMAX)
			det->volt_tbl_init2[0] = det->ops->volt_2_pmic(det, 131000);
			eem_debug("HV VCORE voltage EEM to 0x%X\n", det->volt_tbl_init2[0]);
		#elif defined(SLT_VMIN)
			det->volt_tbl_init2[0] = clamp(det->volt_tbl_init2[0] - 0xB,
				det->ops->eem_2_pmic(det, det->VMIN), det->ops->eem_2_pmic(det, det->VMAX));
			eem_debug("LV VCORE voltage EEM to 0x%X\n", det->volt_tbl_init2[0]);
		#else
			eem_debug("NV VCORE voltage EEM to 0x%x\n", det->volt_tbl_init2[0]);
		#endif
	#else
		eem_debug("VCORE EEM voltage to 0x%x\n", det->volt_tbl_init2[0]);
	#endif

	/* return mt_set_cur_volt_vcore_pdn(det->volt_tbl_pmic[0]); */  /* unit = 10 uv */
	#ifdef EARLY_PORTING
		return 0;
	#else
		return dvfs_set_vcore(det->volt_tbl_init2[0]);
	#endif
}
#endif
#endif /* #if 0*/
#endif /*#if !(EEM_ENABLE_TINYSYS_SSPM)*/

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
	return (((volt) - det->pmic_base + det->pmic_step - 1) / det->pmic_step);
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
		((((eem_val) * det->eem_step) + det->eem_v_base - det->pmic_base + det->pmic_step - 1) / det->pmic_step)
		);
	#endif
	return ((((eem_val) * det->eem_step) + det->eem_v_base -
			det->pmic_base + det->pmic_step - 1) / det->pmic_step);
}
#undef __MTK_EEM_PLATFORM_C__
