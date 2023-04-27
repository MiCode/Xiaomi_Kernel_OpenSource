// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
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

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
/* label_comment */
#include "mtk_gpufreq.h"
/* #include "mtk_misc.h" */
#endif

/*
 * operation of EEM detectors
 */
/* legacy ptp need to define other hook functions */
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


#define GPU_SWITCH 1	//label_macro

/* import some functions about GPU */
/* label_new_strat */
/* PTPOD for legacy chip*/
/* extern unsigned int mt_gpufreq_update_volt(unsigned int pmic_volt[], unsigned int array_size);
extern void mt_gpufreq_enable_by_ptpod(void);
extern void mt_gpufreq_disable_by_ptpod(void);
extern void mt_gpufreq_restore_default_volt(void);
extern unsigned int mt_gpufreq_get_cur_volt(void);
extern unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_ori_opp_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_volt_by_real_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_freq_by_real_idx(unsigned int idx); */

/* label_new_end */





#ifndef EARLY_PORTING_CPU
static unsigned int detid_to_dvfsid(struct eem_det *det)
{
	unsigned int cpudvfsindex = 0;
	enum eem_det_id detid = det_to_id(det);

//	if (detid == EEM_DET_L)
//		cpudvfsindex = MT_CPU_DVFS_L;
	if (detid == EEM_DET_2L)
		cpudvfsindex = MT_CPU_DVFS_LL;
#if ENABLE_LOO
//	else if (detid == EEM_DET_L_HI)
//		cpudvfsindex = MT_CPU_DVFS_L;
	else if (detid == EEM_DET_2L_HI)
		cpudvfsindex = MT_CPU_DVFS_LL;
#endif
	return cpudvfsindex;
}
#endif

/* Will return 10uV */
int get_volt_cpu(struct eem_det *det)
{
	unsigned int value = 0;
	enum eem_det_id cpudvfsindex;
	eem_debug("[Add_EEM] Start:	get_volt_cpu() !\n");
	// WARN_ON(true);

	FUNC_ENTER(FUNC_LV_HELP);
	/* unit mv * 100 = 10uv */
	eem_debug("[Add_EEM] Start:	detid_to_dvfsid() !\n");
	cpudvfsindex = detid_to_dvfsid(det);
	eem_debug("[Add_EEM] End:	detid_to_dvfsid() !,cpudvfsindex=0x%08X \n", cpudvfsindex);
	/* label_comment */
	eem_debug("[Add_EEM] Start:	mt_cpufreq_get_cur_volt() !\n");
	value = mt_cpufreq_get_cur_volt(cpudvfsindex);
	eem_debug("[Add_EEM] End:	mt_cpufreq_get_cur_volt() ,value=0x%08X !\n", value);
	/* value = 0; */
	FUNC_EXIT(FUNC_LV_HELP);
	eem_debug("[Add_EEM] End:	get_volt_cpu() ,value=0x%08X !\n", value);
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
	/* label_comment */
	value = mt_cpufreq_update_volt(cpudvfsindex, record_tbl_locked,
		det->num_freq_tbl);
/* 	value = 0; */
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
	case EEM_DET_2L:
	/* label_comment */
		value = mt_cpufreq_update_volt(MT_CPU_DVFS_LL,
			det->volt_tbl_orig,
			det->num_freq_tbl);
			/* value = 0; */
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
		/* label_comment */
		det->freq_tbl[i] =
			PERCENT(mt_cpufreq_get_freq_by_idx(cpudvfsindex, i),
			det->max_freq_khz);
		/* det->freq_tbl[i] = 0; */

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
		/* label_comment */
		volt = mt_cpufreq_get_volt_by_idx(cpudvfsindex, i);
		/* volt=0; */

		det->volt_tbl_orig[i] = det->ops->volt_2_pmic(det, volt);

		eem_debug("[%s]volt_tbl_orig[%d] = %d(0x%x)\n",
			det->name+8,
			i,
			volt,
			det->volt_tbl_orig[i]);
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


#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
	/* eem_debug("get_volt_gpu=%d\n",mt_gpufreq_get_cur_volt()); */
#if GPU_SWITCH
	return gpufreq_get_cur_volt(TARGET_GPU); /* unit  mv * 100 = 10uv */

#else
#endif
	return 0;
#endif

	FUNC_EXIT(FUNC_LV_HELP);
}

int set_volt_gpu(struct eem_det *det)
{

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
#if GPU_SWITCH
	int i;
	unsigned int output[NR_FREQ_GPU];

	for (i = 0; i < det->num_freq_tbl; i++)
		output[i] = det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]);

	return mt_gpufreq_update_volt(output, det->num_freq_tbl);
#else
#endif
	return 0;
#endif

}

void restore_default_volt_gpu(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

#if GPU_SWITCH
#if  IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
	mt_gpufreq_restore_default_volt();
#endif
#endif

	FUNC_EXIT(FUNC_LV_HELP);
}

void get_freq_table_gpu(struct eem_det *det)
{
#if  IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
	int i = 0;

	memset(det->freq_tbl, 0, sizeof(det->freq_tbl));

	FUNC_ENTER(FUNC_LV_HELP);

#if GPU_SWITCH
	for (i = 0; i < NR_FREQ_GPU; i++) {
		/* label_comment */

		det->freq_tbl[i] =
		PERCENT(mt_gpufreq_get_freq_by_idx(i), det->max_freq_khz);
		eem_debug("freq_tbl_gpu[%d]=%d, (%d)\n",
			i,
			det->freq_tbl[i],
			mt_gpufreq_get_freq_by_idx(i));


		if (det->freq_tbl[i] == 0)
			break;
	}
#endif

	det->num_freq_tbl = i;
	eem_debug("[%s] freq_num:%d, max_freq=%d\n",
		det->name+8, det->num_freq_tbl, det->max_freq_khz);
#endif

	FUNC_EXIT(FUNC_LV_HELP);
}

void get_orig_volt_table_gpu(struct eem_det *det)
{
#if  IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
#if SET_PMIC_VOLT_TO_DVFS
#if GPU_SWITCH
	int i = 0, volt = 0;
#endif
	FUNC_ENTER(FUNC_LV_HELP);

#if GPU_SWITCH
	for (i = 0; i < det->num_freq_tbl; i++) {

		/* label_comment */
		volt = mt_gpufreq_get_volt_by_idx(i);
		det->volt_tbl_orig[i] = det->ops->volt_2_pmic(det, volt);

	}
#endif

#if ENABLE_LOO
		/* Use signoff volt */
	memcpy
	(det->volt_tbl, det->volt_tbl_orig, sizeof(det->volt_tbl));
	memcpy
	(det->volt_tbl_init2, det->volt_tbl_orig, sizeof(det->volt_tbl));
#endif

	FUNC_EXIT(FUNC_LV_HELP);
#endif
#endif
}


/************************************************
 * common det operations for legacy and sspm ptp
 ************************************************
 */
int base_ops_volt_2_pmic(struct eem_det *det, int volt)
{
	return (((volt) - det->pmic_base + det->pmic_step - 1) /
		det->pmic_step);
}

int base_ops_volt_2_eem(struct eem_det *det, int volt)
{
	return (((volt) - det->eem_v_base + det->eem_step - 1) /
		det->eem_step);
}

int base_ops_pmic_2_volt(struct eem_det *det, int pmic_val)
{
	return (((pmic_val) * det->pmic_step) + det->pmic_base);
}

int base_ops_eem_2_pmic(struct eem_det *det, int eem_val)
{
	return ((((eem_val) * det->eem_step) + det->eem_v_base -
			det->pmic_base + det->pmic_step - 1) / det->pmic_step);
}
#undef __MTK_EEM_PLATFORM_C__
