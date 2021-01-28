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
 * @file	mtk_eem.
 * @brief   Driver for EEM
 *
 */

#define __MTK_EEM_C__
/*=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/math64.h>
#include <linux/uaccess.h>

#ifdef CONFIG_OF
	#include <linux/cpu.h>
	#include <linux/of.h>
	#include <linux/of_irq.h>
	#include <linux/of_address.h>
	#include <linux/of_fdt.h>
	#include <mt-plat/aee.h>
#endif

#include <mt-plat/mtk_chip.h>
#include "upmu_common.h"
#ifdef CONFIG_MTK_FREQ_HOPPING
#include "mt_freqhopping.h"
#endif
#ifdef CONFIG_THERMAL
#include "mtk_thermal.h"
#endif
#include "mtk_ppm_api.h"
#include "mtk_cpufreq_api.h"
#include "mtk_eem_config.h"
#include "mtk_eem.h"
#include "mtk_defeem.h"
#include "mtk_eem_internal_ap.h"

#include "mtk_eem_internal.h"
#include <mt-plat/mtk_devinfo.h>
#include <regulator/consumer.h>
#include "pmic_regulator.h"
#include "mtk_pmic_regulator.h"
#include "pmic_api_buck.h"


#if UPDATE_TO_UPOWER
#include "mtk_upower.h"
#endif

#include "mtk_mcdi_api.h"

/****************************************
 * define variables for legacy and eem
 ****************************************
 */
#if ENABLE_INIT1_STRESS
static int eem_init1stress_en, testCnt;
wait_queue_head_t wqStress;
struct task_struct *threadStress;
#endif

static unsigned int ctrl_EEM_Enable = 1;
/* Get time stmp to known the time period */
static unsigned long long eem_pTime_us, eem_cTime_us, eem_diff_us;

/* for setting pmic pwm mode and auto mode */
struct regulator *eem_regulator_vproc;

static void eem_set_eem_volt(struct eem_det *det);
static void eem_restore_eem_volt(struct eem_det *det);
static void eem_buck_set_mode(unsigned int mode);

/* table used to apply to dvfs at final */
unsigned int record_tbl_locked[NR_FREQ];
unsigned int final_init01_flag;
#if ENABLE_LOO
unsigned int final_init02_flag;
#endif

DEFINE_MUTEX(record_mutex);
static struct eem_devinfo eem_devinfo;
#if defined(CONFIG_CPU_FORCE_TO_BIN2)
static struct eem_devinfo_extra ex_eem_devinfo;
#endif
static struct hrtimer eem_log_timer;
static DEFINE_SPINLOCK(eem_spinlock);
DEFINE_SPINLOCK(record_spinlock);

/******************************************
 * common variables for legacy ptp
 *******************************************
 */
static int eem_log_en;
static unsigned int eem_checkEfuse = 1;
#if defined(CONFIG_CPU_FORCE_TO_BIN2)
static unsigned int eem_chkExEfuse = 1;
#endif
static unsigned int informEEMisReady;

/* The EMM controller list managed by Picachu. */
static unsigned int pi_eem_ctrl_id[] = {
#if ENABLE_LOO
	EEM_CTRL_2L, EEM_CTRL_L, EEM_CTRL_CCI, EEM_CTRL_L_HI, EEM_CTRL_2L_HI
#else
	EEM_CTRL_2L, EEM_CTRL_L, EEM_CTRL_CCI
#endif
};

#define PI_MDES_BDES_MASK	(0xFFFF)
#define PI_MTDES_MASK		(0xFF)

struct pi_efuse_index {
	unsigned int mdes_bdes_index : 8;
	unsigned int mdes_bdes_shift : 8;

	unsigned int mtdes_index : 8;
	unsigned int mtdes_shift : 8;
};

static struct pi_efuse_index pi_efuse_idx[] = {
#if ENABLE_LOO
	{3, 0, 2, 0},		/* EEM_CTRL_2L */
	{1, 0, 2, 16},		/* EEM_CTRL_L */
	{7, 0, 8, 16},		/* EEM_CTRL_CCI */
	{4, 0, 5, 16},		/* EEM_CTRL_L_HI */
	{6, 0, 5, 0},		/* EEM_CTRL_2L_HI */
#else
	{11, 0, 10, 0},		/* EEM_CTRL_2L */
	{9, 0, 10, 16},		/* EEM_CTRL_L */
	{7, 0, 8, 16},		/* EEM_CTRL_CCI */
#endif
};

/* Global variable for slow idle*/
unsigned int ptp_data[3] = {0, 0, 0};
#ifdef CONFIG_OF
void __iomem *eem_base;
static u32 eem_irq_number;
#endif

/*=============================================================
 * weak function
 *=============================================================
 */
void __attribute__ ((weak))
mt_ppm_ptpod_policy_activate(void)
{
	pr_info("%s not ready\n", __func__);
}

void __attribute__ ((weak))
mt_ppm_ptpod_policy_deactivate(void)
{
	pr_info("%s not ready\n", __func__);
}

/*=============================================================
 * common functions for both ap and eem
 *=============================================================
 */
unsigned int mt_eem_is_enabled(void)
{
	return informEEMisReady;
}

static struct eem_det *id_to_eem_det(enum eem_det_id id)
{
	if (likely(id < NR_EEM_DET))
		return &eem_detectors[id];
	else
		return NULL;
}

static int get_devinfo(void)
{
	struct pi_efuse_index *p;
	unsigned int n, tmp, idx;
	struct eem_det *det;
	int ret = 0;
	int i = 0;
	int *val;

	val = (int *)&eem_devinfo;

	/* FTPGM */
	val[0] = get_devinfo_with_index(DEVINFO_IDX_0);
	val[1] = get_devinfo_with_index(DEVINFO_IDX_1);
	val[2] = get_devinfo_with_index(DEVINFO_IDX_2);
	val[3] = get_devinfo_with_index(DEVINFO_IDX_3);
	val[4] = get_devinfo_with_index(DEVINFO_IDX_4);
	val[5] = get_devinfo_with_index(DEVINFO_IDX_5);
	val[6] = get_devinfo_with_index(DEVINFO_IDX_6);
	val[7] = get_devinfo_with_index(DEVINFO_IDX_7);
	val[8] = get_devinfo_with_index(DEVINFO_IDX_8);
	val[9] = get_devinfo_with_index(DEVINFO_IDX_9);
	val[10] = get_devinfo_with_index(DEVINFO_IDX_10);
	val[11] = get_devinfo_with_index(DEVINFO_IDX_11);

	for (i = 1; i < NR_HW_RES_FOR_BANK; i++) {
		if (val[i] == 0) {
			ret = 1;
			eem_checkEfuse = 0;
			eem_debug("No EEM EFUSE available,will apply safe EFUSE for EEM (val[%d])!!\n",
				i);
			break;
		}
	}

#if EEM_FAKE_EFUSE
	/* for verification */
	val[0] = DEVINFO_0;
	val[1] = DEVINFO_1;
	val[2] = DEVINFO_2;
	val[3] = DEVINFO_3;
	val[4] = DEVINFO_4;
	val[5] = DEVINFO_5;
	val[6] = DEVINFO_6;
	val[7] = DEVINFO_7;
	val[8] = DEVINFO_8;
	val[9] = DEVINFO_9;
	val[10] = DEVINFO_10;
	val[11] = DEVINFO_11;
#endif

#ifdef CONFIG_EEM_AEE_RR_REC
	aee_rr_rec_ptp_devinfo_0((unsigned int)val[0]);
	aee_rr_rec_ptp_e0((unsigned int)val[1]);
	aee_rr_rec_ptp_e1((unsigned int)val[2]);
	aee_rr_rec_ptp_e2((unsigned int)val[3]);
	aee_rr_rec_ptp_e3((unsigned int)val[4]);
	aee_rr_rec_ptp_e4((unsigned int)val[5]);
	aee_rr_rec_ptp_e5((unsigned int)val[6]);
	aee_rr_rec_ptp_e6((unsigned int)val[7]);
	aee_rr_rec_ptp_e7((unsigned int)val[8]);
	aee_rr_rec_ptp_e8((unsigned int)val[9]);
	aee_rr_rec_ptp_e9((unsigned int)val[10]);
	aee_rr_rec_ptp_e10((unsigned int)val[11]);

#endif

	n = sizeof(pi_eem_ctrl_id) / sizeof(unsigned int);

	/* Update MTDES/BDES/MDES if they are modified by PICACHU. */
	for (i = 0; i < n; i++) {
		det = id_to_eem_det(pi_eem_ctrl_id[i]);

		idx = i % det->pi_efuse_count;

		if (!det->pi_efuse[idx])
			continue;

		p = &pi_efuse_idx[i];

		/* Get mdes/bdes efuse data from Picachu */
		tmp = (det->pi_efuse[idx] >> 8) & PI_MDES_BDES_MASK;

		/* Update mdes/bdes */
		val[p->mdes_bdes_index] &=
				~(PI_MDES_BDES_MASK << p->mdes_bdes_shift);

		val[p->mdes_bdes_index] |= (tmp << p->mdes_bdes_shift);

		/* Get mtdes efuse data from Picachu */
		tmp = det->pi_efuse[idx] & PI_MTDES_MASK;

		/* Update mtdes */
		val[p->mtdes_index] &= ~(PI_MTDES_MASK << p->mtdes_shift);
		val[p->mtdes_index] |= (tmp << p->mtdes_shift);
	}


	eem_debug("M_HW_RES0 = 0x%08X\n", val[0]);
	eem_debug("M_HW_RES1 = 0x%08X\n", val[1]);
	eem_debug("M_HW_RES2 = 0x%08X\n", val[2]);
	eem_debug("M_HW_RES3 = 0x%08X\n", val[3]);
	eem_debug("M_HW_RES4 = 0x%08X\n", val[4]);
	eem_debug("M_HW_RES5 = 0x%08X\n", val[5]);
	eem_debug("M_HW_RES6 = 0x%08X\n", val[6]);
	eem_debug("M_HW_RES7 = 0x%08X\n", val[7]);
	eem_debug("M_HW_RES8 = 0x%08X\n", val[8]);
	eem_debug("M_HW_RES9 = 0x%08X\n", val[9]);
	eem_debug("M_HW_RES10= 0x%08X\n", val[10]);
	eem_debug("M_HW_RES11= 0x%08X\n", val[11]);


	FUNC_ENTER(FUNC_LV_HELP);

	/* NR_HW_RES_FOR_BANK =  10 for 5 banks efuse */
	for (i = 1; i < NR_HW_RES_FOR_BANK; i++) {
		if (val[i] == 0) {
			ret = 1;
			eem_checkEfuse = 0;
			eem_error
("No EEM EFUSE available, will turn off EEM (val[%d]) !!\n", i);
			for_each_det(det)
				det->disabled = 1;
			break;
		}
	}

#if defined(CONFIG_CPU_FORCE_TO_BIN2)
	val = (int *)&ex_eem_devinfo;

	val[0] = get_devinfo_with_index(EX_DEV_IDX_0);
	val[1] = get_devinfo_with_index(EX_DEV_IDX_1);
	val[2] = get_devinfo_with_index(EX_DEV_IDX_2);
	val[3] = get_devinfo_with_index(EX_DEV_IDX_3);

	aee_rr_rec_ptp_devinfo_1((unsigned int)val[0]);
	aee_rr_rec_ptp_devinfo_2((unsigned int)val[1]);
	aee_rr_rec_ptp_devinfo_3((unsigned int)val[2]);
	aee_rr_rec_ptp_devinfo_4((unsigned int)val[3]);
	if ((ex_eem_devinfo.EXTRA_BIN & 0x1) != 0x1)
		eem_chkExEfuse = 0;

	for (i = 1; i < 4; i++) {
		if (val[i] == 0) {
			eem_chkExEfuse = 0;
			break;
		}
	}

#endif

#if (EEM_FAKE_EFUSE)
	eem_checkEfuse = 1;
#endif

	FUNC_EXIT(FUNC_LV_HELP);
	return ret;
}

/*============================================================
 * function declarations of EEM detectors
 *============================================================
 */
static void mt_ptp_lock(unsigned long *flags);
static void mt_ptp_unlock(unsigned long *flags);

/*=============================================================
 * Local function definition
 *=============================================================
 */
#ifdef CONFIG_EEM_AEE_RR_REC
static void _mt_eem_aee_init(void)
{
	aee_rr_rec_ptp_vboot(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_big_volt(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_big_volt_1(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_big_volt_2(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_big_volt_3(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_gpu_volt(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_gpu_volt_1(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_gpu_volt_2(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_gpu_volt_3(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_little_volt(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_little_volt_1(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_little_volt_2(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_little_volt_3(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_2_little_volt(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_2_little_volt_1(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_2_little_volt_2(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_2_little_volt_3(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_cci_volt(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_cci_volt_1(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_cci_volt_2(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_cci_volt_3(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_temp(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_status(0xFF);
}
#endif

#ifdef CONFIG_THERMAL
/* common part in thermal */
int __attribute__((weak))
tscpu_get_temp_by_bank(enum thermal_bank_name ts_bank)
{
	eem_error("cannot find %s (thermal has not ready yet!)\n", __func__);
	return 0;
}

int __attribute__((weak))
tscpu_is_temp_valid(void)
{
	eem_error("cannot find %s (thermal has not ready yet!)\n", __func__);
	return 0;
}
#endif

static struct eem_ctrl *id_to_eem_ctrl(enum eem_ctrl_id id)
{
	if (likely(id < NR_EEM_CTRL))
		return &eem_ctrls[id];
	else
		return NULL;
}

void base_ops_enable(struct eem_det *det, int reason)
{
	/* FIXME: UNDER CONSTRUCTION */
	FUNC_ENTER(FUNC_LV_HELP);
	det->disabled &= ~reason;
	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_switch_bank(struct eem_det *det, enum eem_phase phase)
{
	unsigned int coresel;

	FUNC_ENTER(FUNC_LV_HELP);

	coresel =
	(eem_read(EEMCORESEL) & ~BITMASK(2:0)) | BITS(2:0, det->ctrl_id);

	/* 803f0000 + det->ctrl_id = enable ctrl's swcg clock */
	/* 003f0000 + det->ctrl_id = disable ctrl's swcg clock */
	/* bug: when system resume, need to restore coresel value */
	if (phase == EEM_PHASE_INIT01) {
		coresel |= CORESEL_VAL;
	} else {
		coresel |= CORESEL_INIT2_VAL;
		coresel &= 0x0fffffff;
	}

	eem_write(EEMCORESEL, coresel);
#if 0
	eem_debug("[%s] 0x1100bf00=0x%x\n", ((char *)(det->name) + 8),
		eem_read(EEMCORESEL));
#endif

	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_disable_locked(struct eem_det *det, int reason)
{
	FUNC_ENTER(FUNC_LV_HELP);

	switch (reason) {
	case BY_MON_ERROR: /* 4 */
		/* disable EEM */
		eem_write(EEMEN, 0x0 | SEC_MOD_SEL);

		/* Clear EEM interrupt EEMINTSTS */
		eem_write(EEMINTSTS, 0x00ffffff);
		/* fall through */

	case BY_PROCFS_INIT2: /* 8 */
		/* set init2 value to DVFS table (PMIC) */
		memcpy(det->volt_tbl, det->volt_tbl_init2,
		sizeof(det->volt_tbl_init2));
		#if UPDATE_TO_UPOWER
		det->set_volt_to_upower = 0;
		#endif
		eem_set_eem_volt(det);
		det->disabled |= reason;
		eem_debug("det->disabled=%x", det->disabled);
		break;

	case BY_INIT_ERROR: /* 2 */
		/* disable EEM */
		eem_write(EEMEN, 0x0 | SEC_MOD_SEL);

		/* Clear EEM interrupt EEMINTSTS */
		eem_write(EEMINTSTS, 0x00ffffff);
		/* fall through */

	case BY_PROCFS: /* 1 */
		det->disabled |= reason;
		eem_debug("det->disabled=%x", det->disabled);
		/* restore default DVFS table (PMIC) */
		eem_restore_eem_volt(det);
		break;

	default:
		eem_debug("det->disabled=%x\n", det->disabled);
		det->disabled &= ~BY_PROCFS;
		det->disabled &= ~BY_PROCFS_INIT2;
		eem_debug("det->disabled=%x\n", det->disabled);
		eem_set_eem_volt(det);
		break;
	}

	eem_debug("Disable EEM[%s] done. reason=[%d]\n", det->name,
		det->disabled);

	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_disable(struct eem_det *det, int reason)
{
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	mt_ptp_lock(&flags);
	det->ops->switch_bank(det, NR_EEM_PHASE);
	det->ops->disable_locked(det, reason);
	mt_ptp_unlock(&flags);

	FUNC_EXIT(FUNC_LV_HELP);
}

int base_ops_init01(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	if (unlikely(!HAS_FEATURE(det, FEA_INIT01))) {
		eem_debug("det %s has no INIT01\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	det->ops->set_phase(det, EEM_PHASE_INIT01);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

int base_ops_init02(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	if (unlikely(!HAS_FEATURE(det, FEA_INIT02))) {
		eem_debug("det %s has no INIT02\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	if (det->disabled & BY_INIT_ERROR) {
		eem_error("[%s] Disabled by INIT_ERROR\n",
			((char *)(det->name) + 8));
		det->ops->dump_status(det);
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}
#if 0
	eem_debug("DCV = 0x%08X, AGEV = 0x%08X\n", det->DCVOFFSETIN,
		det->AGEVOFFSETIN);
#endif
	/* det->ops->dump_status(det); */
	det->ops->set_phase(det, EEM_PHASE_INIT02);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

int base_ops_mon_mode(struct eem_det *det)
{
#ifdef CONFIG_THERMAL
	struct TS_PTPOD ts_info;
	enum thermal_bank_name ts_bank;
#endif

	FUNC_ENTER(FUNC_LV_HELP);

	if (!HAS_FEATURE(det, FEA_MON)) {
		eem_error("det %s has no MON mode\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	if (det->disabled & BY_INIT_ERROR) {
		eem_error("[%s] Disabled BY_INIT_ERROR\n",
			((char *)(det->name) + 8));
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}

#ifndef CONFIG_THERMAL
	det->MTS = MTS_VAL;
	det->BTS = BTS_VAL;
#else
	if (det_to_id(det) == EEM_DET_L)
		ts_bank = THERMAL_BANK0;
	else if (det_to_id(det) == EEM_DET_2L)
		ts_bank = THERMAL_BANK1;
	else if (det_to_id(det) == EEM_DET_CCI)
		ts_bank = THERMAL_BANK2;
#if ENABLE_LOO
	else if (det_to_id(det) == EEM_DET_L_HI)
		ts_bank = THERMAL_BANK0;
	else if (det_to_id(det) == EEM_DET_2L_HI)
		ts_bank = THERMAL_BANK1;
#endif
	else
		ts_bank = THERMAL_BANK0;

	get_thermal_slope_intercept(&ts_info, ts_bank);
	det->MTS = ts_info.ts_MTS;
	det->BTS = ts_info.ts_BTS;
#endif

/*
 * eem_debug("[base_ops_mon_mode] Bk = %d, MTS = 0x%08X, BTS = 0x%08X\n",
 *			det->ctrl_id, det->MTS, det->BTS);
 */
#if 0
	if ((det->EEMINITEN == 0x0) || (det->EEMMONEN == 0x0)) {
		eem_debug("EEMINITEN = 0x%08X, EEMMONEN = 0x%08X\n",
			det->EEMINITEN,	det->EEMMONEN);
		FUNC_EXIT(FUNC_LV_HELP);
		return 1;
	}
#endif
	/* det->ops->dump_status(det); */
	det->ops->set_phase(det, EEM_PHASE_MON);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

int base_ops_get_status(struct eem_det *det)
{
	int status;
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	mt_ptp_lock(&flags);
	det->ops->switch_bank(det, NR_EEM_PHASE);
	status = (eem_read(EEMEN) != 0) ? 1 : 0;
	mt_ptp_unlock(&flags);

	FUNC_EXIT(FUNC_LV_HELP);

	return status;
}

void base_ops_dump_status(struct eem_det *det)
{
	unsigned int i;

	FUNC_ENTER(FUNC_LV_HELP);

	eem_isr_info("[%s]\n",			det->name);

	eem_isr_info("EEMINITEN = 0x%08X\n",	det->EEMINITEN);
	eem_isr_info("EEMMONEN = 0x%08X\n",	det->EEMMONEN);
	eem_isr_info("MDES = 0x%08X\n",		det->MDES);
	eem_isr_info("BDES = 0x%08X\n",		det->BDES);
	eem_isr_info("DCMDET = 0x%08X\n",	det->DCMDET);

	eem_isr_info("DCCONFIG = 0x%08X\n",	det->DCCONFIG);
	eem_isr_info("DCBDET = 0x%08X\n",	det->DCBDET);

	eem_isr_info("AGECONFIG = 0x%08X\n",	det->AGECONFIG);
	eem_isr_info("AGEM = 0x%08X\n",		det->AGEM);

	eem_isr_info("AGEDELTA = 0x%08X\n",	det->AGEDELTA);
	eem_isr_info("DVTFIXED = 0x%08X\n",	det->DVTFIXED);
	eem_isr_info("MTDES = 0x%08X\n",	det->MTDES);
	eem_isr_info("VCO = 0x%08X\n",		det->VCO);

	eem_isr_info("DETWINDOW = 0x%08X\n",	det->DETWINDOW);
	eem_isr_info("VMAX = 0x%08X\n",		det->VMAX);
	eem_isr_info("VMIN = 0x%08X\n",		det->VMIN);
	eem_isr_info("DTHI = 0x%08X\n",		det->DTHI);
	eem_isr_info("DTLO = 0x%08X\n",		det->DTLO);
	eem_isr_info("VBOOT = 0x%08X\n",	det->VBOOT);
	eem_isr_info("DETMAX = 0x%08X\n",	det->DETMAX);

	eem_isr_info("DCVOFFSETIN = 0x%08X\n",	det->DCVOFFSETIN);
	eem_isr_info("AGEVOFFSETIN = 0x%08X\n",	det->AGEVOFFSETIN);

	eem_isr_info("MTS = 0x%08X\n",		det->MTS);
	eem_isr_info("BTS = 0x%08X\n",		det->BTS);

	eem_isr_info("num_freq_tbl = %d\n", det->num_freq_tbl);

	for (i = 0; i < det->num_freq_tbl; i++)
		eem_isr_info("freq_tbl[%d] = %d\n", i, det->freq_tbl[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		eem_isr_info("volt_tbl[%d] = %d\n", i, det->volt_tbl[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		eem_isr_info("volt_tbl_init2[%d] = %d\n",
			i, det->volt_tbl_init2[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		eem_isr_info("volt_tbl_pmic[%d] = %d\n",
			i, det->volt_tbl_pmic[i]);

	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_set_phase(struct eem_det *det, enum eem_phase phase)
{
	unsigned int i, filter, val;
#if ENABLE_LOO
	enum eem_det_id detid = det_to_id(det);
#endif
	/* unsigned long flags; */

	FUNC_ENTER(FUNC_LV_HELP);

	/* mt_ptp_lock(&flags); */

	det->ops->switch_bank(det, phase);

	/* config EEM register */
	eem_write(EEM_DESCHAR,
		  ((det->BDES << 8) & 0xff00) | (det->MDES & 0xff));
	eem_write(EEM_TEMPCHAR,
		  (((det->VCO << 16) & 0xff0000) |
		   ((det->MTDES << 8) & 0xff00) | (det->DVTFIXED & 0xff)));
	eem_write(EEM_DETCHAR,
		  ((det->DCBDET << 8) & 0xff00) | (det->DCMDET & 0xff));

	eem_write(EEM_DCCONFIG, det->DCCONFIG);
	eem_write(EEM_AGECONFIG, det->AGECONFIG);

	if (phase == EEM_PHASE_MON)
		eem_write(EEM_TSCALCS,
			  ((det->BTS << 12) & 0xfff000) | (det->MTS & 0xfff));

	if (det->AGEM == 0x0)
		eem_write(EEM_RUNCONFIG, 0x80000000);
	else {
		val = 0x0;

		for (i = 0; i < 24; i += 2) {
			filter = 0x3 << i;

			if (((det->AGECONFIG) & filter) == 0x0)
				val |= (0x1 << i);
			else
				val |= ((det->AGECONFIG) & filter);
		}
		eem_write(EEM_RUNCONFIG, val);
	}

#if ENABLE_LOO
	if ((detid == EEM_DET_L) || (detid == EEM_DET_2L)) {
		eem_write(EEM_FREQPCT30,
			  ((det->freq_tbl[11] << 24) & 0xff000000)	|
			  ((det->freq_tbl[10] << 16) & 0xff0000) |
			  ((det->freq_tbl[9] << 8) & 0xff00)	|
			  (det->freq_tbl[8] & 0xff));
		eem_write(EEM_FREQPCT74,
			  ((det->freq_tbl[15] << 24) & 0xff000000)	|
			  ((det->freq_tbl[14] << 16) & 0xff0000) |
			  ((det->freq_tbl[13] << 8) & 0xff00)	|
			  ((det->freq_tbl[12]) & 0xff));
	} else if ((detid == EEM_DET_L_HI) || (detid == EEM_DET_2L_HI)) {
		eem_write(EEM_FREQPCT30,
			  ((det->freq_tbl[3] << 24) & 0xff000000)	|
			  ((det->freq_tbl[2] << 16) & 0xff0000) |
			  ((det->freq_tbl[1] << 8) & 0xff00)	|
			  (det->freq_tbl[0] & 0xff));
		eem_write(EEM_FREQPCT74,
			  ((det->freq_tbl[7] << 24) & 0xff000000)	|
			  ((det->freq_tbl[6] << 16) & 0xff0000) |
			  ((det->freq_tbl[5] << 8) & 0xff00)	|
			  ((det->freq_tbl[4]) & 0xff));
	} else {
		eem_write(EEM_FREQPCT30,
((det->freq_tbl[3 * ((det->num_freq_tbl + 7) / 8)] << 24) & 0xff000000)	|
((det->freq_tbl[2 * ((det->num_freq_tbl + 7) / 8)] << 16) & 0xff0000) |
((det->freq_tbl[1 * ((det->num_freq_tbl + 7) / 8)] << 8) & 0xff00)	|
(det->freq_tbl[0] & 0xff));
		eem_write(EEM_FREQPCT74,
((det->freq_tbl[7 * ((det->num_freq_tbl + 7) / 8)] << 24) & 0xff000000)	|
((det->freq_tbl[6 * ((det->num_freq_tbl + 7) / 8)] << 16) & 0xff0000) |
((det->freq_tbl[5 * ((det->num_freq_tbl + 7) / 8)] << 8) & 0xff00)	|
((det->freq_tbl[4 * ((det->num_freq_tbl + 7) / 8)]) & 0xff));
	}
#else
	eem_write(EEM_FREQPCT30,
((det->freq_tbl[3 * ((det->num_freq_tbl + 7) / 8)] << 24) & 0xff000000) |
((det->freq_tbl[2 * ((det->num_freq_tbl + 7) / 8)] << 16) & 0xff0000) |
((det->freq_tbl[1 * ((det->num_freq_tbl + 7) / 8)] << 8) & 0xff00) |
(det->freq_tbl[0] & 0xff));
	eem_write(EEM_FREQPCT74,
((det->freq_tbl[7 * ((det->num_freq_tbl + 7) / 8)] << 24) & 0xff000000)	|
((det->freq_tbl[6 * ((det->num_freq_tbl + 7) / 8)] << 16) & 0xff0000)	|
((det->freq_tbl[5 * ((det->num_freq_tbl + 7) / 8)] << 8) & 0xff00)	|
((det->freq_tbl[4 * ((det->num_freq_tbl + 7) / 8)]) & 0xff));
#endif

	eem_write(EEM_LIMITVALS,
		  ((det->VMAX << 24) & 0xff000000)	|
		  ((det->VMIN << 16) & 0xff0000)	|
		  ((det->DTHI << 8) & 0xff00)		|
		  (det->DTLO & 0xff));
	/* eem_write(EEM_LIMITVALS, 0xFF0001FE); */
	eem_write(EEM_VBOOT, (((det->VBOOT) & 0xff)));
	eem_write(EEM_DETWINDOW, (((det->DETWINDOW) & 0xffff)));
	eem_write(EEMCONFIG, (((det->DETMAX) & 0xffff)));
	/* for two line */
	eem_write(EEM_CHKSHIFT, (0x77 & 0xff));

#if ENABLE_EEMCTL0
	/* eem ctrl choose thermal sensors */
	eem_write(EEM_CTL0, det->EEMCTL0);
#endif
	/* clear all pending EEM interrupt & config EEMINTEN */
	eem_write(EEMINTSTS, 0xffffffff);

#if 0
	eem_debug(" %s set phase = %d\n", ((char *)(det->name) + 8), phase);
#endif
	switch (phase) {
	case EEM_PHASE_INIT01:
		eem_write(EEMINTEN, 0x00005f01);
		/* enable EEM INIT measurement */
		eem_write(EEMEN, 0x00000001 | SEC_MOD_SEL);
		udelay(250); /* all banks' phase cannot be set without delay */
		break;

	case EEM_PHASE_INIT02:
		eem_write(EEMINTEN, 0x00005f01);

		eem_write(EEM_INIT2VALS,
		((det->AGEVOFFSETIN << 16) & 0xffff0000) |
		((eem_devinfo.FT_PGM == 0) ? 0 : det->DCVOFFSETIN & 0xffff));

		/* enable EEM INIT measurement */
		eem_write(EEMEN, 0x00000005 | SEC_MOD_SEL);
		udelay(200); /* all banks' phase cannot be set without delay */
		break;

	case EEM_PHASE_MON:
		eem_write(EEMINTEN, 0x00FF0000);
		/* enable EEM monitor mode */
		eem_write(EEMEN, 0x00000002 | SEC_MOD_SEL);
		break;

	default:
		WARN_ON(1); /*BUG()*/
		break;
	}
	/* mt_ptp_unlock(&flags); */

	FUNC_EXIT(FUNC_LV_HELP);
}

int base_ops_get_temp(struct eem_det *det)
{
#ifdef CONFIG_THERMAL
	enum thermal_bank_name ts_bank;

	if (det_to_id(det) == EEM_DET_L)
		ts_bank = THERMAL_BANK0;
	else if (det_to_id(det) == EEM_DET_2L)
		ts_bank = THERMAL_BANK1;
	else if (det_to_id(det) == EEM_DET_CCI)
		ts_bank = THERMAL_BANK2;
	else
		ts_bank = THERMAL_BANK0;

	return tscpu_get_temp_by_bank(ts_bank);
#else
	return 0;
#endif
}

int base_ops_get_volt(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	eem_debug("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

int base_ops_set_volt(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	eem_debug("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

void base_ops_restore_default_volt(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	eem_debug("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_get_freq_table(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	det->freq_tbl[0] = 100;
	det->num_freq_tbl = 1;

	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_get_orig_volt_table(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);
}

static long long eem_get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec);
}

/*=============================================================
 * Global function definition
 *=============================================================
 */
static void mt_ptp_lock(unsigned long *flags)
{
	spin_lock_irqsave(&eem_spinlock, *flags);
	eem_pTime_us = eem_get_current_time_us();

}
EXPORT_SYMBOL(mt_ptp_lock);

static void mt_ptp_unlock(unsigned long *flags)
{
	eem_cTime_us = eem_get_current_time_us();
	EEM_IS_TOO_LONG();
	spin_unlock_irqrestore(&eem_spinlock, *flags);
}
EXPORT_SYMBOL(mt_ptp_unlock);

void mt_record_lock(unsigned long *flags)
{
	spin_lock_irqsave(&record_spinlock, *flags);
}
EXPORT_SYMBOL(mt_record_lock);

void mt_record_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&record_spinlock, *flags);
}
EXPORT_SYMBOL(mt_record_unlock);

/*
 * timer for log
 */
static enum hrtimer_restart eem_log_timer_func(struct hrtimer *timer)
{
	struct eem_det *det;

	FUNC_ENTER(FUNC_LV_HELP);

	for_each_det(det) {
		/* get rid of redundent banks */
		if (det->features == 0)
			continue;

		eem_debug
		("Timer Bk=%d (%d)(%d, %d, %d, %d, %d, %d, %d, %d)(0x%x)\n",
			det->ctrl_id,
			det->ops->get_temp(det),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[0]),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[1]),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[2]),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[3]),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[4]),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[5]),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[6]),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[7]),
			det->t250);

#if 0
		det->freq_tbl[0],
		det->freq_tbl[1],
		det->freq_tbl[2],
		det->freq_tbl[3],
		det->freq_tbl[4],
		det->freq_tbl[5],
		det->freq_tbl[6],
		det->freq_tbl[7],
		det->dcvalues[3],
		det->freqpct30[3],
		det->eem_26c[3],
		det->vop30[3]
#endif
	}

	hrtimer_forward_now(timer, ns_to_ktime(LOG_INTERVAL));
	FUNC_EXIT(FUNC_LV_HELP);

	return HRTIMER_RESTART;
}

/*
 * Thread for voltage setting
 */
static int eem_volt_thread_handler(void *data)
{
	struct eem_ctrl *ctrl = (struct eem_ctrl *)data;
	struct eem_det *det = id_to_eem_det(ctrl->det_id);

	FUNC_ENTER(FUNC_LV_HELP);

	do {
		wait_event_interruptible(ctrl->wq, ctrl->volt_update);

		if ((ctrl->volt_update & EEM_VOLT_UPDATE) &&
			det->ops->set_volt) {

#ifdef CONFIG_EEM_AEE_RR_REC
			/* update set volt status for this bank */
			int temp = -1;

			switch (det->ctrl_id) {
			case EEM_CTRL_L:
				aee_rr_rec_ptp_status(
					aee_rr_curr_ptp_status() |
					(1 << EEM_CPU_LITTLE_IS_SET_VOLT));
				temp = EEM_CPU_LITTLE_IS_SET_VOLT;
				break;

			case EEM_CTRL_2L:
				aee_rr_rec_ptp_status(
					aee_rr_curr_ptp_status() |
					(1 << EEM_CPU_2_LITTLE_IS_SET_VOLT));
				temp = EEM_CPU_2_LITTLE_IS_SET_VOLT;
				break;

			case EEM_CTRL_CCI:
				aee_rr_rec_ptp_status(
					aee_rr_curr_ptp_status() |
					(1 << EEM_CPU_CCI_IS_SET_VOLT));
				temp = EEM_CPU_CCI_IS_SET_VOLT;
				break;

			default:
				eem_error
("eem_volt_thread_handler : incorrect det id %d\n", det->ctrl_id);
				break;
			}
#endif

		det->ops->set_volt(det);
#if 0
		eem_debug("B=%d,T=%d,DC=%x,V30=%x,F30=%x,sts=%x,250=%x\n",
		det->ctrl_id,
		det->ops->get_temp(det),
		det->dcvalues[EEM_PHASE_INIT01],
		det->vop30[EEM_PHASE_MON],
		det->freqpct30[EEM_PHASE_MON],
		det->ops->get_status(det),
		det->t250);
#endif

		/* clear out set volt status for this bank */
#ifdef CONFIG_EEM_AEE_RR_REC
			if (temp >= EEM_CPU_LITTLE_IS_SET_VOLT)
				aee_rr_rec_ptp_status(
				aee_rr_curr_ptp_status() & ~(1 << temp));
#endif
		}
		if ((ctrl->volt_update & EEM_VOLT_RESTORE) &&
			det->ops->restore_default_volt)
			det->ops->restore_default_volt(det);

		ctrl->volt_update = EEM_VOLT_NONE;

	} while (!kthread_should_stop());

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

#if ENABLE_INIT1_STRESS
static int eem_init1stress_thread_handler(void *data)
{
	struct eem_det *det;
	struct eem_ctrl *ctrl;
	unsigned long flag;
	unsigned int out = 0, timeout = 0;

	do {
		wait_event_interruptible(wqStress, eem_init1stress_en);
		eem_error("eem init1stress start\n");
		testCnt = 0;

		/* CPU pre-process */
		mcdi_pause(MCDI_PAUSE_BY_EEM, true);

		mt_ppm_ptpod_policy_activate();

		eem_buck_set_mode(1);

		while (eem_init1stress_en) {
			/* Start to clear previour ptp init status */
			mt_ptp_lock(&flag);

			for_each_det(det) {
				det->ops->switch_bank(det, NR_EEM_PHASE);
				eem_write(EEMEN, 0x0 | SEC_MOD_SEL);
			/* Clear EEM INIT interrupt EEMINTSTS = 0x00ff0000 */
				eem_write(EEMINTSTS, 0x00ff0000);
				det->eem_eemEn[EEM_PHASE_INIT01] = 0;
				det->features &= FEA_INIT01;
			}
			mt_ptp_unlock(&flag);

			if (testCnt++ % 200 == 0)
				eem_error
("eem_init1stress_thread_handler, test counter:%d\n", testCnt);

			for_each_det_ctrl(det, ctrl) {
				if (HAS_FEATURE(det, FEA_INIT01)) {

					mt_ptp_lock(&flag); /* <-XXX */
					det->ops->init01(det);
					mt_ptp_unlock(&flag); /* <-XXX */
				}
			}

	/* This patch is waiting for whole bank finish the init01 then go
	 * next. Due to LL/L use same bulk PMIC, LL voltage table change
	 * will impact L to process init01 stage, because L require a
	 * stable 1V for init01.
	 */
			while (1) {
				for_each_det(det) {
					if (((out & BIT(det->ctrl_id)) == 0) &&
					(det->eem_eemEn[EEM_PHASE_INIT01] ==
					(1 | SEC_MOD_SEL)))
						out |= BIT(det->ctrl_id);
				}

				if (out == final_init01_flag) {
					timeout = 0;
					break;
				}
				udelay(100);
				timeout++;

				if (timeout % 300 == 0)
					eem_error
("init01 wait time is %d, bankmask:0x%x[/0x%x]\n",
timeout, out, final_init01_flag);
			}
			msleep(100);
		}

		/* CPU post-process */
		eem_buck_set_mode(0);

		mt_ppm_ptpod_policy_deactivate();

		mcdi_pause(MCDI_PAUSE_BY_EEM, false);

		eem_error("eem init1stress end, total test counter:%d\n",
			testCnt);
	} while (!kthread_should_stop());

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}
#endif

static void inherit_base_det(struct eem_det *det)
{
	/*
	 * Inherit ops from eem_det_base_ops if ops in det is NULL
	 */
	FUNC_ENTER(FUNC_LV_HELP);

	#define INIT_OP(ops, func)					\
		do {							\
			if (ops->func == NULL)				\
				ops->func = eem_det_base_ops.func;	\
		} while (0)

	INIT_OP(det->ops, disable);
	INIT_OP(det->ops, disable_locked);
	INIT_OP(det->ops, switch_bank);
	INIT_OP(det->ops, init01);
	INIT_OP(det->ops, init02);
	INIT_OP(det->ops, mon_mode);
	INIT_OP(det->ops, get_status);
	INIT_OP(det->ops, dump_status);
	INIT_OP(det->ops, set_phase);
	INIT_OP(det->ops, get_temp);
	INIT_OP(det->ops, get_volt);
	INIT_OP(det->ops, set_volt);
	INIT_OP(det->ops, restore_default_volt);
	INIT_OP(det->ops, get_freq_table);
	INIT_OP(det->ops, volt_2_pmic);
	INIT_OP(det->ops, volt_2_eem);
	INIT_OP(det->ops, pmic_2_volt);
	INIT_OP(det->ops, eem_2_pmic);

	FUNC_EXIT(FUNC_LV_HELP);
}

static void eem_init_ctrl(struct eem_ctrl *ctrl)
{
	FUNC_ENTER(FUNC_LV_HELP);

	if (1) {
		init_waitqueue_head(&ctrl->wq);
		ctrl->thread = kthread_run(eem_volt_thread_handler,
			ctrl, ctrl->name);

		if (IS_ERR(ctrl->thread))
			eem_error("Create %s thread failed: %ld\n", ctrl->name,
						PTR_ERR(ctrl->thread));
	}

	FUNC_EXIT(FUNC_LV_HELP);
}

static void eem_init_det(struct eem_det *det, struct eem_devinfo *devinfo)
{
	enum eem_det_id det_id = det_to_id(det);
	/* unsigned int binLevel; */

	FUNC_ENTER(FUNC_LV_HELP);

	inherit_base_det(det);

	switch (det_id) {
#if ENABLE_LOO
	case EEM_DET_L:
		det->MDES	= devinfo->CPU_L_LO_MDES;
		det->BDES	= devinfo->CPU_L_LO_BDES;
		det->DCMDET	= devinfo->CPU_L_LO_DCMDET;
		det->DCBDET	= devinfo->CPU_L_LO_DCBDET;
		det->EEMINITEN	= devinfo->CPU_L_LO_INITEN;
		det->EEMMONEN	= devinfo->CPU_L_LO_MONEN;
		det->MTDES	= devinfo->CPU_L_LO_MTDES;
		det->SPEC	= devinfo->CPU_L_LO_SPEC;
		det->DVTFIXED = DVTFIXED_M_VAL;

#if defined(CONFIG_CPU_FORCE_TO_BIN2)
		if (eem_chkExEfuse) {
			det->MDES	= ex_eem_devinfo.EX_L_LO_MDES;
			det->BDES	= ex_eem_devinfo.EX_L_LO_BDES;
			det->MTDES	= ex_eem_devinfo.EX_L_LO_MTDES;
		}
#endif
		break;
	case EEM_DET_2L:
		det->MDES	= devinfo->CPU_2L_LO_MDES;
		det->BDES	= devinfo->CPU_2L_LO_BDES;
		det->DCMDET	= devinfo->CPU_2L_LO_DCMDET;
		det->DCBDET	= devinfo->CPU_2L_LO_DCBDET;
		det->EEMINITEN	= devinfo->CPU_2L_LO_INITEN;
		det->EEMMONEN	= devinfo->CPU_2L_LO_MONEN;
		det->MTDES	= devinfo->CPU_2L_LO_MTDES;
		det->SPEC	= devinfo->CPU_2L_LO_SPEC;
		det->DVTFIXED = DVTFIXED_M_VAL;
		break;
#if defined(CONFIG_CPU_FORCE_TO_BIN2)
		if (eem_chkExEfuse) {
			det->MDES	= ex_eem_devinfo.EX_2L_LO_MDES;
			det->BDES	= ex_eem_devinfo.EX_2L_LO_BDES;
			det->MTDES	= ex_eem_devinfo.EX_2L_LO_MTDES;
		}
#endif
#else
	case EEM_DET_L:
		det->MDES	= devinfo->CPU_L_MDES;
		det->BDES	= devinfo->CPU_L_BDES;
		det->DCMDET	= devinfo->CPU_L_DCMDET;
		det->DCBDET	= devinfo->CPU_L_DCBDET;
		det->EEMINITEN	= devinfo->CPU_L_INITEN;
		det->EEMMONEN	= devinfo->CPU_L_MONEN;
		det->MTDES	= devinfo->CPU_L_MTDES;
		det->SPEC	= devinfo->CPU_L_SPEC;
		break;

	case EEM_DET_2L:
		det->MDES	= devinfo->CPU_2L_MDES;
		det->BDES	= devinfo->CPU_2L_BDES;
		det->DCMDET	= devinfo->CPU_2L_DCMDET;
		det->DCBDET	= devinfo->CPU_2L_DCBDET;
		det->EEMINITEN	= devinfo->CPU_2L_INITEN;
		det->EEMMONEN	= devinfo->CPU_2L_MONEN;
		det->MTDES	= devinfo->CPU_2L_MTDES;
		det->SPEC	= devinfo->CPU_2L_SPEC;
		break;
#endif
	case EEM_DET_CCI:
		det->MDES	= devinfo->CCI_MDES;
		det->BDES	= devinfo->CCI_BDES;
		det->DCMDET	= devinfo->CCI_DCMDET;
		det->DCBDET	= devinfo->CCI_DCBDET;
		det->EEMINITEN	= devinfo->CCI_INITEN;
		det->EEMMONEN	= devinfo->CCI_MONEN;
		det->MTDES	= devinfo->CCI_MTDES;
		det->SPEC       = devinfo->CCI_SPEC;
#if defined(CONFIG_CPU_FORCE_TO_BIN2)
		if (eem_chkExEfuse) {
			det->MDES	= ex_eem_devinfo.EX_CCI_MDES;
			det->BDES	= ex_eem_devinfo.EX_CCI_BDES;
			det->MTDES	= ex_eem_devinfo.EX_CCI_MTDES;
		}
#endif
		break;
#if ENABLE_LOO
	case EEM_DET_L_HI:
		det->MDES	= devinfo->CPU_L_HI_MDES;
		det->BDES	= devinfo->CPU_L_HI_BDES;
		det->DCMDET	= devinfo->CPU_L_HI_DCMDET;
		det->DCBDET	= devinfo->CPU_L_HI_DCBDET;
		det->EEMINITEN	= devinfo->CPU_L_HI_INITEN;
		det->EEMMONEN	= devinfo->CPU_L_HI_MONEN;
		det->MTDES	= devinfo->CPU_L_HI_MTDES;
		det->SPEC	= devinfo->CPU_L_HI_SPEC;
#if defined(CONFIG_CPU_FORCE_TO_BIN2)
		if (eem_chkExEfuse) {
			det->MDES	= ex_eem_devinfo.EX_L_HI_MDES;
			det->BDES	= ex_eem_devinfo.EX_L_HI_BDES;
			det->MTDES	= ex_eem_devinfo.EX_L_HI_MTDES;
		}
#endif
	break;
	case EEM_DET_2L_HI:
		/* TODO: config real B_HI efuse here */
		det->MDES	= devinfo->CPU_2L_HI_MDES;
		det->BDES	= devinfo->CPU_2L_HI_BDES;
		det->DCMDET	= devinfo->CPU_2L_HI_DCMDET;
		det->DCBDET	= devinfo->CPU_2L_HI_DCBDET;
		det->EEMINITEN	= devinfo->CPU_2L_HI_INITEN;
		det->EEMMONEN	= devinfo->CPU_2L_HI_MONEN;
		det->MTDES	= devinfo->CPU_2L_HI_MTDES;
		det->SPEC	= devinfo->CPU_2L_HI_SPEC;
#if defined(CONFIG_CPU_FORCE_TO_BIN2)
		if (eem_chkExEfuse) {
			det->MDES	= ex_eem_devinfo.EX_2L_HI_MDES;
			det->BDES	= ex_eem_devinfo.EX_2L_HI_BDES;
			det->MTDES	= ex_eem_devinfo.EX_2L_HI_MTDES;
		}
#endif
	break;

#endif
	default:
		eem_debug("[%s]: Unknown det_id %d\n", __func__, det_id);
		break;
	}

	/* get DVFS frequency table */
	if (det->ops->get_freq_table)
		det->ops->get_freq_table(det);

	FUNC_EXIT(FUNC_LV_HELP);
}

#if UPDATE_TO_UPOWER
static enum upower_bank transfer_ptp_to_upower_bank(unsigned int det_id)
{
	enum upower_bank bank;

	switch (det_id) {
	case EEM_DET_L:
		bank = UPOWER_BANK_L;
		break;
	case EEM_DET_2L:
		bank = UPOWER_BANK_LL;
		break;
	case EEM_DET_CCI:
		bank = UPOWER_BANK_CCI;
		break;
	default:
		bank = NR_UPOWER_BANK;
		break;
	}
	return bank;
}

static void eem_update_init2_volt_to_upower(struct eem_det *det,
	unsigned int *pmic_volt)
{
	unsigned int volt_tbl[NR_FREQ_CPU];
	enum upower_bank bank;
	int i;

	for (i = 0; i < det->num_freq_tbl; i++)
		volt_tbl[i] = det->ops->pmic_2_volt(det, pmic_volt[i]);

	bank = transfer_ptp_to_upower_bank(det_to_id(det));
	if (bank < NR_UPOWER_BANK) {
		upower_update_volt_by_eem(bank, volt_tbl, det->num_freq_tbl);
#if 0
		eem_debug
("update init2 volt to upower (eem bank %ld upower bank %d)\n",
det_to_id(det), bank);
#endif
	}

}
#endif

#define MARGIN_ADD_OFF				(10) /* Add 62.5mv */
static void eem_set_eem_volt(struct eem_det *det)
{
#if SET_PMIC_VOLT
	unsigned int i;
	int low_temp_offset = 0;
	struct eem_ctrl *ctrl = id_to_eem_ctrl(det->ctrl_id);
#if ENABLE_LOO
	struct eem_det *org_det = det;
	unsigned int init2chk = 0;
#endif

	FUNC_ENTER(FUNC_LV_HELP);

#if ENABLE_LOO
	/* remap to L/B bank for update dvfs table,
	 * also copy high opp volt table
	 */
	/* Band HIGHL will update its volt table (opp0~7) to bank L */
	if (det->ctrl_id == EEM_CTRL_L_HI) {
		det = id_to_eem_det(EEM_DET_L);
		memcpy(det->volt_tbl, org_det->volt_tbl,
			sizeof(det->volt_tbl)/2);
		ctrl = id_to_eem_ctrl(det->ctrl_id);
	} else if (det->ctrl_id == EEM_CTRL_2L_HI) {
		det = id_to_eem_det(EEM_DET_2L);
		memcpy(det->volt_tbl, org_det->volt_tbl,
			sizeof(det->volt_tbl)/2);
		ctrl = id_to_eem_ctrl(det->ctrl_id);
	}
#endif
	det->temp = det->ops->get_temp(det);

#if UPDATE_TO_UPOWER
	upower_update_degree_by_eem(
		transfer_ptp_to_upower_bank(det_to_id(det)), det->temp/1000);
#endif

#ifdef CONFIG_THERMAL
#if 0
	eem_debug("eem_set_eem_volt cur_temp = %d, valid = %d\n",
		det->temp, tscpu_is_temp_valid());
#endif
	/* 6250 * 10uV = 62.5mv */
	if (det->temp <= INVERT_TEMP_VAL || !tscpu_is_temp_valid())
#else
	if (det->temp <= INVERT_TEMP_VAL)
#endif
		det->isTempInv = 1;
	else if ((det->isTempInv) && (det->temp > OVER_INV_TEM_VAL))
		det->isTempInv = 0;

	if (det->isTempInv) {
		memcpy(det->volt_tbl, det->volt_tbl_init2,
			sizeof(det->volt_tbl));

		/* Add low temp offset for each bank if temp inverse */
		low_temp_offset = det->low_temp_off;
	}

	ctrl->volt_update |= EEM_VOLT_UPDATE;

	/* for debugging */
#if 0
	eem_debug("volt_offset, low_temp_offset= %d, %d\n",
		det->volt_offset, low_temp_offset);
	eem_debug("det->vmin = %d\n", det->VMIN);
	eem_debug("det->vmax = %d\n", det->VMAX);
#endif

	/* scale of det->volt_offset must equal 10uV */
	/* if has record table, min with record table of each cpu */
	for (i = 0; i < det->num_freq_tbl; i++) {
		switch (det->ctrl_id) {
		case EEM_CTRL_L:
				det->volt_tbl_pmic[i] = min(
				(unsigned int)clamp(
				det->ops->eem_2_pmic(det,
					det->volt_tbl[i] +
					det->volt_offset +
					low_temp_offset),
				det->ops->eem_2_pmic(det, det->VMIN),
				det->ops->eem_2_pmic(det, det->VMAX)),
				clamp(
				det->volt_tbl_orig[i] + MARGIN_ADD_OFF,
				(unsigned int)det->ops->eem_2_pmic(det,
					det->VMIN),
				(unsigned int)det->ops->eem_2_pmic(det,
					det->VMAX))
				);
			break;

		case EEM_CTRL_2L:
				det->volt_tbl_pmic[i] = min(
				(unsigned int)clamp(
				det->ops->eem_2_pmic(det,
					det->volt_tbl[i] +
					det->volt_offset +
					low_temp_offset),
				det->ops->eem_2_pmic(det, det->VMIN),
				det->ops->eem_2_pmic(det, det->VMAX)),
				clamp(
				det->volt_tbl_orig[i] + MARGIN_ADD_OFF,
				(unsigned int)det->ops->eem_2_pmic(det,
					det->VMIN),
				(unsigned int)det->ops->eem_2_pmic(det,
					det->VMAX))
				);
			break;

		case EEM_CTRL_CCI:
			det->volt_tbl_pmic[i] = min(
			(unsigned int)(clamp(
				det->ops->eem_2_pmic(det,
		(det->volt_tbl[i] + det->volt_offset + low_temp_offset)),
				det->ops->eem_2_pmic(det, det->VMIN),
				det->ops->eem_2_pmic(det, det->VMAX))),
				det->volt_tbl_orig[i]);
			break;

		default:
			eem_error("[eem_set_eem_volt] incorrect det :%s!!",
				det->name);
			break;
		}
#if 0
	eem_error
("[%s].volt_tbl[%d]=0x%X - Ori[0x%x] volt_tbl_pmic[%d]=0x%X (%d)\n",
det->name,
i, det->volt_tbl[i], det->volt_tbl_orig[i],
i, det->volt_tbl_pmic[i], det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));
#endif
#if ENABLE_LOO
		if ((i > 0) &&
			(det->volt_tbl_pmic[i] > det->volt_tbl_pmic[i-1]) &&
			(det->set_volt_to_upower)) {
			if ((org_det->ctrl_id == EEM_CTRL_L_HI) ||
				(org_det->ctrl_id == EEM_CTRL_2L_HI))
				/* Receive high bank isr but low opp
				 * still using higher volt overwrite
				 * low bank opp voltage
				 */
				det->volt_tbl_pmic[i] = det->volt_tbl_pmic[i-1];
			else {
				/* Receive low bank isr but high opp
				 * still using lower volt overwrite
				 * high bank opp voltage
				 */
				det->volt_tbl_pmic[i-1] = det->volt_tbl_pmic[i];
				if ((i > 1) &&
					(det->volt_tbl_pmic[i] >
						det->volt_tbl_pmic[i-2]))
					det->volt_tbl_pmic[i-2] =
					det->volt_tbl_pmic[i];
			}
		}
#endif
	}
	dsb(sy);

#if UPDATE_TO_UPOWER
#if ENABLE_LOO
	if (det->set_volt_to_upower == 0) {
		if (((det->ctrl_id == EEM_CTRL_L) &&
			((final_init02_flag & EEM_L_INIT02_FLAG) ==
				EEM_L_INIT02_FLAG)) ||
			((det->ctrl_id == EEM_CTRL_2L) &&
			((final_init02_flag & EEM_2L_INIT02_FLAG) ==
				EEM_2L_INIT02_FLAG)) ||
			(det->ctrl_id == EEM_CTRL_CCI))
			init2chk = 1;

		/* only when set_volt_to_upower == 0,
		 * the volt will be apply to upower
		 */
		if (init2chk) {
			eem_update_init2_volt_to_upower(det,
				det->volt_tbl_pmic);
			det->set_volt_to_upower = 1;
		}
	}
#else
	/* only when set_volt_to_upower == 0, */
	/* the volt will be apply to upower */
	if (det->set_volt_to_upower == 0) {
		eem_update_init2_volt_to_upower(det, det->volt_tbl_pmic);
		det->set_volt_to_upower = 1;
	}
#endif
#endif

	if ((0 == (det->disabled % 2)) &&
		(0 == (det->disabled & BY_PROCFS_INIT2)))
		wake_up_interruptible(&ctrl->wq);
	else
		eem_error("Disabled by [%d]\n", det->disabled);

#endif

	FUNC_EXIT(FUNC_LV_HELP);
}

static void eem_restore_eem_volt(struct eem_det *det)
{
#if SET_PMIC_VOLT
	struct eem_ctrl *ctrl = id_to_eem_ctrl(det->ctrl_id);

	ctrl->volt_update |= EEM_VOLT_RESTORE;
	wake_up_interruptible(&ctrl->wq);
#endif
}

#if 0
static void mt_eem_reg_dump_locked(void)
{
	unsigned long addr;

	for (addr = (unsigned long)EEM_DESCHAR;
		addr <= (unsigned long)EEM_SMSTATE1; addr += 4)
		eem_isr_info("0x %lu = 0x %lu\n", addr,
			*(unsigned long *)addr);

	addr = (unsigned long)EEMCORESEL;
	eem_isr_info("0x %lu = 0x %lu\n", addr, *(unsigned long *)addr);
}
#endif

static inline void handle_init01_isr(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_LOCAL);

	/* eem_debug("mode = init1 %s-isr\n", ((char *)(det->name) + 8)); */

	det->dcvalues[EEM_PHASE_INIT01]		= eem_read(EEM_DCVALUES);
	det->freqpct30[EEM_PHASE_INIT01]	= eem_read(EEM_FREQPCT30);
	det->eem_26c[EEM_PHASE_INIT01]		= eem_read(EEMINTEN + 0x10);
	det->vop30[EEM_PHASE_INIT01]		= eem_read(EEM_VOP30);
	det->eem_eemEn[EEM_PHASE_INIT01]	= eem_read(EEMEN);

#if DUMP_DATA_TO_DE
	{
		unsigned int i;

		for (i = 0; i < ARRAY_SIZE(reg_dump_addr_off); i++) {
			det->reg_dump_data[i][EEM_PHASE_INIT01] =
				eem_read(EEM_BASEADDR + reg_dump_addr_off[i]);
			eem_isr_info("0x%lx = 0x%08x\n",
				(unsigned long)EEM_BASEADDR +
				reg_dump_addr_off[i],
				det->reg_dump_data[i][EEM_PHASE_INIT01]
				);
		}
	}
#endif
	/*
	 * Read & store 16 bit values EEM_DCVALUES.DCVOFFSET and
	 * EEM_AGEVALUES.AGEVOFFSET for later use in INIT2 procedure
	 */
	/* hw bug, workaround */
	det->DCVOFFSETIN = ~(eem_read(EEM_DCVALUES) & 0xffff) + 1;
	/* check if DCVALUES is minus and set DCVOFFSETIN to zero */

	if (det->DCVOFFSETIN & 0x8000)
		det->DCVOFFSETIN = 0;

	det->AGEVOFFSETIN = eem_read(EEM_AGEVALUES) & 0xffff;

	/*
	 * Set EEMEN.EEMINITEN/EEMEN.EEMINIT2EN = 0x0 &
	 * Clear EEM INIT interrupt EEMINTSTS = 0x00000001
	 */
	eem_write(EEMEN, 0x0 | SEC_MOD_SEL);
	eem_write(EEMINTSTS, 0x1);
	/* det->ops->init02(det); */

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static unsigned int interpolate(unsigned int y1, unsigned int y0,
	unsigned int x1, unsigned int x0, unsigned int ym)
{
	unsigned int ratio, result;

	if (x1 == x0) {
		result =  x1;
	} else {
		ratio = (((y1 - y0) * 100) + (x1 - x0 - 1)) / (x1 - x0);
		result =
		(x1 - ((((y1 - ym) * 10000) + ratio - 1) / ratio) / 100);
		#if 0
		eem_debug("y1(%d), y0(%d), x1(%d), x0(%d), ym(%d), ratio(%d), rtn(%d)\n",
			y1, y0, x1, x0, ym, ratio, result);
		#endif
	}
	return result;
}

static void read_volt_from_VOP(struct eem_det *det)
{
	int temp, i, j;
	unsigned int step = NR_FREQ / 8;
	int ref_idx = ((det->num_freq_tbl + (step - 1)) / step) - 1;
#if ENABLE_LOO
	enum eem_det_id detid = det_to_id(det);

	if ((detid == EEM_DET_L) || (detid == EEM_DET_2L)) {
		temp = eem_read(EEM_VOP30);
		/* eem_debug("(%s_VOP30) = 0x%X\n",
		 * ((char *)(det->name) + 8), temp);
		 */
		/* EEM_VOP30=>pmic value */
		det->volt_tbl[8] = (temp & 0xff);
		det->volt_tbl[9] = (temp >> 8)	& 0xff;
		det->volt_tbl[10] = (temp >> 16) & 0xff;
		det->volt_tbl[11] = (temp >> 24) & 0xff;

		temp = eem_read(EEM_VOP74);
		/* eem_debug("read(EEM_VOP74) = 0x%08X\n", temp); */
		/* EEM_VOP74=>pmic value */
		det->volt_tbl[12] = (temp & 0xff);
		det->volt_tbl[13] = (temp >> 8) & 0xff;
		det->volt_tbl[14] = (temp >> 16) & 0xff;
		det->volt_tbl[15] = (temp >> 24) & 0xff;
	} else if ((detid == EEM_DET_L_HI) || (detid == EEM_DET_2L_HI)) {
		temp = eem_read(EEM_VOP30);
		/* eem_debug("(%s_VOP30) = 0x%X\n",
		 * ((char *)(det->name) + 8), temp);
		 */
		/* EEM_VOP30=>pmic value */
		det->volt_tbl[0] = (temp & 0xff);
		det->volt_tbl[1] = (temp >> 8)	& 0xff;
		det->volt_tbl[2] = (temp >> 16) & 0xff;
		det->volt_tbl[3] = (temp >> 24) & 0xff;

		temp = eem_read(EEM_VOP74);
		/* eem_debug("read(EEM_VOP74) = 0x%08X\n", temp); */
		/* EEM_VOP74=>pmic value */
		det->volt_tbl[4] = (temp & 0xff);
		det->volt_tbl[5] = (temp >> 8)	& 0xff;
		det->volt_tbl[6] = (temp >> 16) & 0xff;
		det->volt_tbl[7] = (temp >> 24) & 0xff;
	} else {
		temp = eem_read(EEM_VOP30);
		/* eem_debug("(%s_VOP30) = 0x%X\n",
		 * ((char *)(det->name) + 8), temp);
		 */
		/* EEM_VOP30=>pmic value */
		det->volt_tbl[0] = (temp & 0xff);
		det->volt_tbl[1 * ((det->num_freq_tbl + 7) / 8)] =
			(temp >> 8)  & 0xff;
		det->volt_tbl[2 * ((det->num_freq_tbl + 7) / 8)] =
			(temp >> 16) & 0xff;
		det->volt_tbl[3 * ((det->num_freq_tbl + 7) / 8)] =
			(temp >> 24) & 0xff;

		temp = eem_read(EEM_VOP74);
		/* eem_debug("read(EEM_VOP74) = 0x%08X\n", temp); */
		/* EEM_VOP74=>pmic value */
		det->volt_tbl[4 * ((det->num_freq_tbl + 7) / 8)] =
		(temp & 0xff);
		det->volt_tbl[5 * ((det->num_freq_tbl + 7) / 8)] =
			(temp >> 8)  & 0xff;
		det->volt_tbl[6 * ((det->num_freq_tbl + 7) / 8)] =
			(temp >> 16) & 0xff;
		det->volt_tbl[7 * ((det->num_freq_tbl + 7) / 8)] =
			(temp >> 24) & 0xff;

		if ((det->num_freq_tbl > 8) && (ref_idx > 0)) {
			for (i = 0; i <= ref_idx; i++) { /* i < 8 */
				for (j = 1; j < step; j++) {
					if (i < ref_idx) {
						det->volt_tbl
							[((i + 0) * step) + j] =
							interpolate(
						det->freq_tbl[((i + 0) * step)],
						det->freq_tbl[((i + 1) * step)],
						det->volt_tbl[((i + 0) * step)],
						det->volt_tbl[((i + 1) * step)],
						det->freq_tbl
						[((i + 0) * step) + j]);
					} else {
						det->volt_tbl
							[((i + 0) * step) + j] =
						clamp(
							interpolate(
						det->freq_tbl[((i - 1) * step)],
						det->freq_tbl[((i + 0) * step)],
						det->volt_tbl[((i - 1) * step)],
						det->volt_tbl[((i + 0) * step)],
						det->freq_tbl
						[((i + 0) * step) + j]),
							det->VMIN,
							det->VMAX
						);
					}
				}
			}
		} /* if (NR_FREQ > 8)*/

	}
#else
	temp = eem_read(EEM_VOP30);
	/* eem_debug("read(EEM_VOP30) = 0x%08X\n", temp); */
	/* EEM_VOP30=>pmic value */
	det->volt_tbl[0] = (temp & 0xff);
	det->volt_tbl[1 * ((det->num_freq_tbl + 7) / 8)] = (temp >> 8)	& 0xff;
	det->volt_tbl[2 * ((det->num_freq_tbl + 7) / 8)] = (temp >> 16) & 0xff;
	det->volt_tbl[3 * ((det->num_freq_tbl + 7) / 8)] = (temp >> 24) & 0xff;

	temp = eem_read(EEM_VOP74);
	/* eem_debug("read(EEM_VOP74) = 0x%08X\n", temp); */
	/* EEM_VOP74=>pmic value */
	det->volt_tbl[4 * ((det->num_freq_tbl + 7) / 8)] = (temp & 0xff);
	det->volt_tbl[5 * ((det->num_freq_tbl + 7) / 8)] = (temp >> 8)	& 0xff;
	det->volt_tbl[6 * ((det->num_freq_tbl + 7) / 8)] = (temp >> 16) & 0xff;
	det->volt_tbl[7 * ((det->num_freq_tbl + 7) / 8)] = (temp >> 24) & 0xff;

	if ((det->num_freq_tbl > 8) && (ref_idx > 0)) {
		for (i = 0; i <= ref_idx; i++) { /* i < 8 */
			for (j = 1; j < step; j++) {
				if (i < ref_idx) {
					det->volt_tbl
						[((i + 0) * step) + j] =
						interpolate(
					det->freq_tbl[((i + 0) * step)],
					det->freq_tbl[((i + 1) * step)],
					det->volt_tbl[((i + 0) * step)],
					det->volt_tbl[((i + 1) * step)],
					det->freq_tbl[((i + 0) * step) + j]
						);
				} else {
					det->volt_tbl
						[((i + 0) * step) + j] =
					clamp(
						interpolate(
					det->freq_tbl[((i - 1) * step)],
					det->freq_tbl[((i + 0) * step)],
					det->volt_tbl[((i - 1) * step)],
					det->volt_tbl[((i + 0) * step)],
					det->freq_tbl[((i + 0) * step) + j]
						),
						det->VMIN,
						det->VMAX
					);
				}
			}
		}
	} /* if (NR_FREQ > 8)*/
#endif
}

static inline void handle_init02_isr(struct eem_det *det)
{
	unsigned int i;
	/* struct eem_ctrl *ctrl = id_to_eem_ctrl(det->ctrl_id); */
#if ENABLE_LOO
	enum eem_det_id detid = det_to_id(det);
	struct eem_det *backupdet;
#endif
	FUNC_ENTER(FUNC_LV_LOCAL);

	/* eem_debug("mode = init2 %s-isr\n", ((char *)(det->name) + 8)); */

	det->dcvalues[EEM_PHASE_INIT02]		= eem_read(EEM_DCVALUES);
	det->freqpct30[EEM_PHASE_INIT02]	= eem_read(EEM_FREQPCT30);
	det->eem_26c[EEM_PHASE_INIT02]		= eem_read(EEMINTEN + 0x10);
	det->vop30[EEM_PHASE_INIT02]	= eem_read(EEM_VOP30);
	det->eem_eemEn[EEM_PHASE_INIT02]	= eem_read(EEMEN);

#if DUMP_DATA_TO_DE
	for (i = 0; i < ARRAY_SIZE(reg_dump_addr_off); i++) {
		det->reg_dump_data[i][EEM_PHASE_INIT02] =
				eem_read(EEM_BASEADDR + reg_dump_addr_off[i]);
		eem_isr_info("0x%lx = 0x%08x\n",
			(unsigned long)EEM_BASEADDR + reg_dump_addr_off[i],
			det->reg_dump_data[i][EEM_PHASE_INIT02]
			);
	}
#endif

	read_volt_from_VOP(det);

#if ENABLE_LOO
	if ((detid == EEM_DET_L_HI) || (detid == EEM_DET_2L_HI)) {
		backupdet = (detid == EEM_DET_L_HI) ?
			id_to_eem_det(EEM_DET_L) : id_to_eem_det(EEM_DET_2L);
		memcpy(backupdet->volt_tbl_init2, det->volt_tbl,
			sizeof(det->volt_tbl_init2)/2);
		memcpy(det->volt_tbl_init2, det->volt_tbl,
			sizeof(det->volt_tbl_init2)/2);
		final_init02_flag |= BIT(detid);
	} else if ((detid == EEM_DET_L) || (detid == EEM_DET_2L)) {
		memcpy(&(det->volt_tbl_init2[NR_FREQ/2]),
			&(det->volt_tbl[NR_FREQ/2]),
			sizeof(det->volt_tbl_init2)/2);
		final_init02_flag |= BIT(detid);
	} else
		memcpy(det->volt_tbl_init2, det->volt_tbl,
			sizeof(det->volt_tbl_init2));
#else
	/* backup to volt_tbl_init2 */
	memcpy(det->volt_tbl_init2, det->volt_tbl, sizeof(det->volt_tbl_init2));
#endif

	for (i = 0; i < NR_FREQ; i++) {
#ifdef CONFIG_EEM_AEE_RR_REC
		switch (det->ctrl_id) {
#if ENABLE_LOO
		case EEM_CTRL_L_HI:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_little_volt(
		((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
		(aee_rr_curr_ptp_cpu_little_volt() & ~
		((unsigned long long)(0xFF) << (8 * i))
					)
				);
			}
			break;
		case EEM_CTRL_L:
			if (i  >= 8) {
				aee_rr_rec_ptp_cpu_little_volt_1(
		((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
		(aee_rr_curr_ptp_cpu_little_volt_1() & ~
		((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;
		case EEM_CTRL_2L_HI:
			if (det->ctrl_id == EEM_CTRL_2L_HI) {
				if (i < 8) {
					aee_rr_rec_ptp_cpu_2_little_volt(
		((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
		(aee_rr_curr_ptp_cpu_2_little_volt() & ~
		((unsigned long long)(0xFF) << (8 * i))
						)
					);
				}
			}
			break;
		case EEM_CTRL_2L:
			if (i  >= 8) {
				aee_rr_rec_ptp_cpu_2_little_volt_1(
		((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
		(aee_rr_curr_ptp_cpu_2_little_volt_1() & ~
		((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;
#else
		case EEM_CTRL_L:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_little_volt(
		((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
		(aee_rr_curr_ptp_cpu_little_volt() & ~
		((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_little_volt_1(
		((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
		(aee_rr_curr_ptp_cpu_little_volt_1() & ~
		((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;

		case EEM_CTRL_2L:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_2_little_volt(
		((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
		(aee_rr_curr_ptp_cpu_2_little_volt() & ~
		((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_2_little_volt_1(
		((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
		(aee_rr_curr_ptp_cpu_2_little_volt_1() & ~
		((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;
#endif
		case EEM_CTRL_CCI:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_cci_volt(
		((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
		(aee_rr_curr_ptp_cpu_cci_volt() & ~
		((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_cci_volt_1(
		((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
		(aee_rr_curr_ptp_cpu_cci_volt_1() & ~
		((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;

		default:
			break;
		}
#endif

#if 0
		eem_debug("init02_[%s].volt_tbl[%d] = 0x%X (%d)\n",
			det->name, i, det->volt_tbl[i],
			det->ops->pmic_2_volt(det, det->volt_tbl[i]));

		if (NR_FREQ > 8) {
			eem_debug("init02_[%s].volt_tbl[%d] = 0x%X (%d)\n",
			det->name, i+1, det->volt_tbl[i+1],
			det->ops->pmic_2_volt(det, det->volt_tbl[i+1]));
		}
#endif
	}

	/* Fix me */
	eem_set_eem_volt(det);

	/*
	 * Set EEMEN.EEMINITEN/EEMEN.EEMINIT2EN = 0x0 &
	 * Clear EEM INIT interrupt EEMINTSTS = 0x00000001
	 */
	eem_write(EEMEN, 0x0 | SEC_MOD_SEL);
	eem_write(EEMINTSTS, 0x1);

	det->ops->mon_mode(det);
	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_init_err_isr(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_LOCAL);
	eem_error("====================================================\n");
	eem_error("EEM init err: EEMEN(%p) = 0x%X, EEMINTSTS(%p) = 0x%X\n",
			 EEMEN, eem_read(EEMEN),
			 EEMINTSTS, eem_read(EEMINTSTS));
	eem_error("EEM_SMSTATE0 (%p) = 0x%X\n",
			 EEM_SMSTATE0, eem_read(EEM_SMSTATE0));
	eem_error("EEM_SMSTATE1 (%p) = 0x%X\n",
			 EEM_SMSTATE1, eem_read(EEM_SMSTATE1));
	eem_error("====================================================\n");

	aee_kernel_warning("mt_eem", "@%s():%d, get_volt(%s) = 0x%08X\n",
		__func__,
		__LINE__,
		det->name,
		det->VBOOT);

	det->ops->disable_locked(det, BY_INIT_ERROR);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_mon_mode_isr(struct eem_det *det)
{
	unsigned int i, verr = 0;
#ifdef CONFIG_THERMAL
#ifdef CONFIG_EEM_AEE_RR_REC
	unsigned long long temp_long;
	unsigned long long temp_cur =
		(unsigned long long)aee_rr_curr_ptp_temp();
#endif
#endif
#if ENABLE_LOO
	enum eem_det_id detid = det_to_id(det);
	struct eem_det *backupdet;
#endif
	FUNC_ENTER(FUNC_LV_LOCAL);

	/* eem_debug("mode = mon %s-isr\n", ((char *)(det->name) + 8)); */

#ifdef CONFIG_THERMAL
	eem_debug("LL_temp=%d, L_temp=%d, CCI_temp=%d\n",
		tscpu_get_temp_by_bank(THERMAL_BANK0),
		tscpu_get_temp_by_bank(THERMAL_BANK1),
		tscpu_get_temp_by_bank(THERMAL_BANK2)
		);
#endif

#ifdef CONFIG_EEM_AEE_RR_REC
	switch (det->ctrl_id) {
	case EEM_CTRL_L:
#ifdef CONFIG_THERMAL
#if defined(__LP64__) || defined(_LP64)
		temp_long =
(unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK0)/1000;
#else
		temp_long =
div_u64((unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK0), 1000);
#endif
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(
			temp_long << (8 * EEM_CPU_LITTLE_IS_SET_VOLT) |
			(temp_cur & ~(0xFF <<
			(8 * EEM_CPU_LITTLE_IS_SET_VOLT))));
		}
#endif
		break;

	case EEM_CTRL_2L:
#ifdef CONFIG_THERMAL
#if defined(__LP64__) || defined(_LP64)
		temp_long =
(unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK1)/1000;
#else
		temp_long =
div_u64((unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK1), 1000);
#endif
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(
		temp_long << (8 * EEM_CPU_2_LITTLE_IS_SET_VOLT) |
			(temp_cur & ~(0xFF <<
			(8 * EEM_CPU_2_LITTLE_IS_SET_VOLT))));
		}
#endif
		break;

	case EEM_CTRL_CCI:
#ifdef CONFIG_THERMAL
#if defined(__LP64__) || defined(_LP64)
		temp_long =
(unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK2)/1000;
#else
		temp_long =
div_u64((unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK2), 1000);
#endif
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(
			temp_long << (8 * EEM_CPU_CCI_IS_SET_VOLT)|
			(temp_cur & ~(0xFF <<
			(8 * EEM_CPU_CCI_IS_SET_VOLT))));
		}
#endif
		break;

	default:
		break;
	}
#endif

	det->dcvalues[EEM_PHASE_MON]	= eem_read(EEM_DCVALUES);
	det->freqpct30[EEM_PHASE_MON]	= eem_read(EEM_FREQPCT30);
	det->eem_26c[EEM_PHASE_MON]	= eem_read(EEMINTEN + 0x10);
	det->vop30[EEM_PHASE_MON]	= eem_read(EEM_VOP30);
	det->eem_eemEn[EEM_PHASE_MON]	= eem_read(EEMEN);

#if DUMP_DATA_TO_DE
	for (i = 0; i < ARRAY_SIZE(reg_dump_addr_off); i++) {
		det->reg_dump_data[i][EEM_PHASE_MON] =
			eem_read(EEM_BASEADDR + reg_dump_addr_off[i]);
		eem_isr_info("0x%lx = 0x%08x\n",
			(unsigned long)EEM_BASEADDR + reg_dump_addr_off[i],
			det->reg_dump_data[i][EEM_PHASE_MON]
			);
	}
#endif

	/* check if thermal sensor init completed? */
	det->t250 = eem_read(TEMP);

	/* 0x64 mappint to 100 + 25 = 125C,
	 * 0xB2 mapping to 178 - 128 = 50, -50 + 25 = -25C
	 */
	if (((det->t250 & 0xff) > 0x64) && ((det->t250  & 0xff) < 0xB2)) {
		eem_error("Temperature > 125C or < -25C !!\n");
		goto out;
	}

	read_volt_from_VOP(det);

#if ENABLE_LOO
	/* Read low bank volt table due to low bank isr may not happen */
	if ((detid == EEM_DET_L_HI) &&
		((final_init02_flag & EEM_L_INIT02_FLAG) ==
			EEM_L_INIT02_FLAG)) {
		/* Make sure aleast init02 isr already received */
		backupdet = id_to_eem_det(EEM_DET_L);
		if (backupdet->isTempInv == 1) {
			det->ops->switch_bank(backupdet, NR_EEM_PHASE);
			read_volt_from_VOP(backupdet);
			det->ops->switch_bank(det, NR_EEM_PHASE);
		}
	} else if ((detid == EEM_DET_2L_HI) &&
		((final_init02_flag & EEM_2L_INIT02_FLAG) ==
			EEM_2L_INIT02_FLAG)) {
		backupdet = id_to_eem_det(EEM_DET_2L);
		if (backupdet->isTempInv == 1) {
			det->ops->switch_bank(backupdet, NR_EEM_PHASE);
			read_volt_from_VOP(backupdet);
			det->ops->switch_bank(det, NR_EEM_PHASE);
		}
	}

#endif
	for (i = 0; i < NR_FREQ; i++) {
#ifdef CONFIG_EEM_AEE_RR_REC
		switch (det->ctrl_id) {
#if ENABLE_LOO
		case EEM_CTRL_L_HI:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_little_volt(
		((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
		(aee_rr_curr_ptp_cpu_little_volt() & ~
		((unsigned long long)(0xFF) << (8 * i))
					)
				);
			}
			break;
		case EEM_CTRL_L:
			if (i  >= 8) {
				aee_rr_rec_ptp_cpu_little_volt_1(
		((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
		(aee_rr_curr_ptp_cpu_little_volt_1() & ~
		((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;
		case EEM_CTRL_2L_HI:
			if (det->ctrl_id == EEM_CTRL_2L_HI) {
				if (i < 8) {
					aee_rr_rec_ptp_cpu_2_little_volt(
		((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
		(aee_rr_curr_ptp_cpu_2_little_volt() & ~
		((unsigned long long)(0xFF) << (8 * i))
						)
					);
				}
			}
			break;
		case EEM_CTRL_2L:
			if (i  >= 8) {
				aee_rr_rec_ptp_cpu_2_little_volt_1(
		((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
		(aee_rr_curr_ptp_cpu_2_little_volt_1() & ~
		((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;
#else

		case EEM_CTRL_L:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_little_volt(
		((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
		(aee_rr_curr_ptp_cpu_little_volt() & ~
		((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_little_volt_1(
		((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
		(aee_rr_curr_ptp_cpu_little_volt_1() & ~
		((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;

		case EEM_CTRL_2L:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_2_little_volt(
		((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
		(aee_rr_curr_ptp_cpu_2_little_volt() & ~
		((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_2_little_volt_1(
		((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
		(aee_rr_curr_ptp_cpu_2_little_volt_1() & ~
		((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;
#endif
		case EEM_CTRL_CCI:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_cci_volt(
		((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
		(aee_rr_curr_ptp_cpu_cci_volt() & ~
		((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_cci_volt_1(
		((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
		(aee_rr_curr_ptp_cpu_cci_volt_1() & ~
		((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;

		default:
			break;
		}
#endif


		if ((i > 0) && (det->volt_tbl[i] > det->volt_tbl[i-1])) {
#if ENABLE_LOO
			if (((detid == EEM_DET_L) || (detid == EEM_DET_2L))
				&& (i == 8))
				continue;
			else if (((detid == EEM_DET_L_HI) ||
				(detid == EEM_DET_2L_HI))
				&& (i >= 8))
				continue;
#endif
			verr = 1;
			aee_kernel_warning("mt_eem",
				"@%s():%d; (%s) [%d] = [%x] > [%d] = [%x]\n",
				__func__, __LINE__, ((char *)(det->name) + 8),
				i, det->volt_tbl[i], i-1, det->volt_tbl[i-1]);

			aee_kernel_warning("mt_eem",
	"@%s():%d; (%s) V30_[0x%x], V74_[0x%x], VD30_[0x%x], VD74_[0x%x]\n",
			__func__, __LINE__, ((char *)(det->name) + 8),
			eem_read(EEM_VOP30), eem_read(EEM_VOP74),
			eem_read(EEM_VDESIGN30), eem_read(EEM_VDESIGN74));

			WARN_ON(det->volt_tbl[i] > det->volt_tbl[i-1]);
		}

		/*
		 * eem_debug("mon_[%s].volt_tbl[%d] = 0x%X (%d)\n",
		 *	det->name, i, det->volt_tbl[i],
		 *	det->ops->pmic_2_volt(det, det->volt_tbl[i]));
		 * if (NR_FREQ > 8) {
		 *	eem_isr_info("mon_[%s].volt_tbl[%d] = 0x%X (%d)\n",
		 *	det->name, i+1, det->volt_tbl[i+1],
		 *	det->ops->pmic_2_volt(det, det->volt_tbl[i+1]));
		 *}
		 */
	}
#if 0
	eem_isr_info("ISR : EEM_TEMPSPARE1 = 0x%08X\n",
		eem_read(EEM_TEMPSPARE1));
#endif
	if (verr == 1) {
#if ENABLE_LOO
		if ((detid == EEM_DET_L_HI) ||
			(detid == EEM_DET_2L_HI)) {
			backupdet = (detid == EEM_DET_L_HI) ?
				id_to_eem_det(EEM_DET_L) :
				id_to_eem_det(EEM_DET_2L);
			memcpy(backupdet->volt_tbl, backupdet->volt_tbl_init2,
				sizeof(det->volt_tbl));
		}
#endif
		memcpy(det->volt_tbl, det->volt_tbl_init2,
			sizeof(det->volt_tbl));
	}

	/* Fix me */
	eem_set_eem_volt(det);

out:
	/* Clear EEM INIT interrupt EEMINTSTS = 0x00ff0000 */
	eem_write(EEMINTSTS, 0x00ff0000);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_mon_err_isr(struct eem_det *det)
{
#if DUMP_DATA_TO_DE
	unsigned int i;
#endif

	FUNC_ENTER(FUNC_LV_LOCAL);

	/* EEM Monitor mode error handler */
	eem_error("====================================================\n");
	eem_error("EEM mon err: EEMCORESEL, EEM_THERMINTST, EEMODINTST");
	eem_error("EEM mon err: (%p) = 0x%08X, (%p) = 0x%08X, (%p) = 0x%08X",
			 EEMCORESEL, eem_read(EEMCORESEL),
			 EEM_THERMINTST, eem_read(EEM_THERMINTST),
			 EEMODINTST, eem_read(EEMODINTST));
	eem_error(" EEMINTSTSRAW(%p) = 0x%08X, EEMINTEN(%p) = 0x%08X\n",
			 EEMINTSTSRAW, eem_read(EEMINTSTSRAW),
			 EEMINTEN, eem_read(EEMINTEN));
	eem_error("====================================================\n");
	eem_error("EEM mon err: EEMEN(%p) = 0x%08X, EEMINTSTS(%p) = 0x%08X\n",
			 EEMEN, eem_read(EEMEN),
			 EEMINTSTS, eem_read(EEMINTSTS));
	eem_error("EEM_SMSTATE0 (%p) = 0x%08X\n",
			 EEM_SMSTATE0, eem_read(EEM_SMSTATE0));
	eem_error("EEM_SMSTATE1 (%p) = 0x%08X\n",
			 EEM_SMSTATE1, eem_read(EEM_SMSTATE1));
	eem_error("TEMP (%p) = 0x%08X\n",
			 TEMP, eem_read(TEMP));
	eem_error("EEM_TEMPMSR0 (%p) = 0x%08X\n",
			 EEM_TEMPMSR0, eem_read(EEM_TEMPMSR0));
	eem_error("EEM_TEMPMSR1 (%p) = 0x%08X\n",
			 EEM_TEMPMSR1, eem_read(EEM_TEMPMSR1));
	eem_error("EEM_TEMPMSR2 (%p) = 0x%08X\n",
			 EEM_TEMPMSR2, eem_read(EEM_TEMPMSR2));
	eem_error("EEM_TEMPMONCTL0 (%p) = 0x%08X\n",
			 EEM_TEMPMONCTL0, eem_read(EEM_TEMPMONCTL0));
	eem_error("EEM_TEMPMSRCTL1 (%p) = 0x%08X\n",
			 EEM_TEMPMSRCTL1, eem_read(EEM_TEMPMSRCTL1));
	eem_error("====================================================\n");

#if DUMP_DATA_TO_DE
		for (i = 0; i < ARRAY_SIZE(reg_dump_addr_off); i++) {
			det->reg_dump_data[i][EEM_PHASE_MON] =
			eem_read(EEM_BASEADDR +	reg_dump_addr_off[i]);
			eem_error("0x%lx = 0x%08x\n",
			(unsigned long)EEM_BASEADDR + reg_dump_addr_off[i],
				det->reg_dump_data[i][EEM_PHASE_MON]
				);
		}
#endif

	eem_error("====================================================\n");
	eem_error("EEM mon err: EEMCORESEL, EEM_THERMINTST, EEMODINTST");
	eem_error("EEM mon err: (%p) = 0x%08X, (%p) = 0x%08X, (%p) = 0x%08X",
			 EEMCORESEL, eem_read(EEMCORESEL),
			 EEM_THERMINTST, eem_read(EEM_THERMINTST),
			 EEMODINTST, eem_read(EEMODINTST));
	eem_error(" EEMINTSTSRAW(%p) = 0x%08X, EEMINTEN(%p) = 0x%08X\n",
			 EEMINTSTSRAW, eem_read(EEMINTSTSRAW),
			 EEMINTEN, eem_read(EEMINTEN));
	eem_error("====================================================\n");

	aee_kernel_warning("mt_eem",
"@%s():%d, get_volt(%s)=0x%08X, EEMEN(%p)=0x%08X, EEMINTSTS(%p)=0x%08X\n",
		__func__, __LINE__,
		det->name, det->VBOOT,
		EEMEN, eem_read(EEMEN),
		EEMINTSTS, eem_read(EEMINTSTS));

	det->ops->disable_locked(det, BY_MON_ERROR);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void eem_isr_handler(struct eem_det *det)
{
	unsigned int eemintsts, eemen;

	FUNC_ENTER(FUNC_LV_LOCAL);

	eemintsts = eem_read(EEMINTSTS);
	eemen = eem_read(EEMEN);

#if 0
	eem_debug("Bk_# = %d %s-isr, 0x%X, 0x%X\n",
		det->ctrl_id, ((char *)(det->name) + 8), eemintsts, eemen);
#endif

	if (eemintsts == 0x1) { /* EEM init1 or init2 */
		if ((eemen & 0x7) == 0x1) /* EEM init1 */
			handle_init01_isr(det);
		else if ((eemen & 0x7) == 0x5) /* EEM init2 */
			handle_init02_isr(det);
		else {
			/* error : init1 or init2 */
			handle_init_err_isr(det);
		}
	} else if ((eemintsts & 0x00ff0000) != 0x0)
		handle_mon_mode_isr(det);
	else { /* EEM error handler */
		/* init 1  || init 2 error handler */
		if (((eemen & 0x7) == 0x1) || ((eemen & 0x7) == 0x5))
			handle_init_err_isr(det);
		else /* EEM Monitor mode error handler */
			handle_mon_err_isr(det);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static irqreturn_t eem_isr(int irq, void *dev_id)
{
	unsigned long flags;
	struct eem_det *det = NULL;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	/* mt_ptp_lock(&flags); */

	for (i = 0; i < NR_EEM_CTRL; i++) {
		mt_ptp_lock(&flags);
		/* TODO: FIXME, it is better to link i @ struct eem_det */
		if ((BIT(i) & eem_read(EEMODINTST))) {
			mt_ptp_unlock(&flags);
			continue;
		}

		det = &eem_detectors[i];

		det->ops->switch_bank(det, NR_EEM_PHASE);

		/*mt_eem_reg_dump_locked(); */

		eem_isr_handler(det);
		mt_ptp_unlock(&flags);
	}

	/* mt_ptp_unlock(&flags); */

	FUNC_EXIT(FUNC_LV_MODULE);

	return IRQ_HANDLED;
}

void eem_init02(const char *str)
{
	struct eem_det *det;
	struct eem_ctrl *ctrl;

	FUNC_ENTER(FUNC_LV_LOCAL);
	eem_debug("eem_init02 called by [%s]\n", str);

	for_each_det_ctrl(det, ctrl) {
		if (HAS_FEATURE(det, FEA_INIT02)) {
			unsigned long flag;

			mt_ptp_lock(&flag);
			det->ops->init02(det);
			mt_ptp_unlock(&flag);
		}
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

/* get regulator reference */
static int eem_buck_get(struct platform_device *pdev)
{
	int ret = 0;

	eem_regulator_vproc = regulator_get(&pdev->dev, "vproc");
	if (!eem_regulator_vproc) {
		eem_error("eem_regulator_vproc error\n");
		return -EINVAL;
	}

	return ret;
}

static void eem_buck_set_mode(unsigned int mode)
{
	/* set pwm mode for each buck */
	eem_debug("pmic set mode (%d)\n", mode);
	if (mode)
		regulator_set_mode(eem_regulator_vproc, REGULATOR_MODE_FAST);
	else
		regulator_set_mode(eem_regulator_vproc, REGULATOR_MODE_NORMAL);
}

void eem_init01(void)
{
	struct eem_det *det;
	struct eem_ctrl *ctrl;
	unsigned int out = 0, timeout = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	for_each_det_ctrl(det, ctrl) {
		unsigned long flag;

		if (HAS_FEATURE(det, FEA_INIT01)) {
			if (det->ops->get_volt != NULL) {
				det->real_vboot = det->ops->volt_2_eem(det,
					det->ops->get_volt(det));

#ifdef CONFIG_EEM_AEE_RR_REC
			aee_rr_rec_ptp_vboot(
				((unsigned long long)(det->real_vboot) <<
				(8 * det->ctrl_id)) |
				(aee_rr_curr_ptp_vboot() & ~
				((unsigned long long)(0xFF) <<
				(8 * det->ctrl_id))
				)
			);
#endif
			}
			timeout = 0;
			while (det->real_vboot != det->VBOOT) {
				det->real_vboot = det->ops->volt_2_eem(det,
					det->ops->get_volt(det));
				if (timeout++ % 300 == 0)
					eem_error
("@%s():%d, get_volt(%s) = 0x%08X, VBOOT = 0x%08X\n",
__func__, __LINE__, det->name, det->real_vboot, det->VBOOT);
			}
			/* BUG_ON(det->real_vboot != det->VBOOT); */
			WARN_ON(det->real_vboot != det->VBOOT);

			mt_ptp_lock(&flag); /* <-XXX */
			det->ops->init01(det);
			mt_ptp_unlock(&flag); /* <-XXX */
		}
	}

	/* CPU post-process */
	eem_buck_set_mode(0);

	mt_ppm_ptpod_policy_deactivate();

	mcdi_pause(MCDI_PAUSE_BY_EEM, false);

	/* This patch is waiting for whole bank finish the init01 then go
	 * next. Due to LL/L use same bulk PMIC, LL voltage table change
	 * will impact L to process init01 stage, because L require a
	 * stable 1V for init01.
	 */
	timeout = 0;
	while (1) {
		for_each_det(det) {
			if (((out & BIT(det->ctrl_id)) == 0) &&
				(det->eem_eemEn[EEM_PHASE_INIT01] ==
				(1 | SEC_MOD_SEL)))
				out |= BIT(det->ctrl_id);
		}


		if (out == final_init01_flag) {
			eem_debug("init01 finish time is %d, bankmask:0x%x\n",
			timeout, out);
			break;
		}
		udelay(100);
		timeout++;

		if (timeout % 300 == 0)
			eem_error
			("init01 wait time is %d, bankmask:0x%x[/0x%x]\n",
			timeout, out, final_init01_flag);
	}

#if ENABLE_LOO
	/* save CPU L/B init01 info to HIGHL/HIGHB */
	eem_detectors[EEM_DET_L_HI].DCVOFFSETIN =
		eem_detectors[EEM_DET_L].DCVOFFSETIN;
	eem_detectors[EEM_DET_L_HI].AGEVOFFSETIN =
		eem_detectors[EEM_DET_L].AGEVOFFSETIN;
	eem_detectors[EEM_DET_2L_HI].DCVOFFSETIN =
		eem_detectors[EEM_DET_2L].DCVOFFSETIN;
	eem_detectors[EEM_DET_2L_HI].AGEVOFFSETIN =
		eem_detectors[EEM_DET_2L].AGEVOFFSETIN;
#endif
	eem_init02(__func__);
	FUNC_EXIT(FUNC_LV_LOCAL);
}

#if EN_EEM
static int eem_probe(struct platform_device *pdev)
{
	int ret;
	struct eem_det *det;
	struct eem_ctrl *ctrl;
#ifdef CONFIG_OF
	struct device_node *node = NULL;
#endif

	FUNC_ENTER(FUNC_LV_MODULE);

#ifdef CONFIG_OF
	node = pdev->dev.of_node;
	if (!node) {
		eem_error("get eem device node err\n");
		return -ENODEV;
	}
	/* Setup IO addresses */
	eem_base = of_iomap(node, 0);
	eem_debug("[EEM] eem_base = 0x%p\n", eem_base);
	eem_irq_number = irq_of_parse_and_map(node, 0);
	eem_debug("[THERM_CTRL] eem_irq_number=%d\n", eem_irq_number);
	if (!eem_irq_number) {
		eem_debug("[EEM] get irqnr failed=0x%x\n", eem_irq_number);
		return 0;
	}
#endif

	/* set EEM IRQ */
	ret = request_irq(eem_irq_number, eem_isr, IRQF_TRIGGER_LOW, "eem",
		NULL);
	if (ret) {
		eem_error("EEM IRQ register failed (%d)\n", ret);
		WARN_ON(1);
	}
	eem_debug("Set EEM IRQ OK.\n");

#ifdef CONFIG_EEM_AEE_RR_REC
		_mt_eem_aee_init();
#endif

	for_each_ctrl(ctrl)
		eem_init_ctrl(ctrl);

	/* CPU pre-process */
	mcdi_pause(MCDI_PAUSE_BY_EEM, true);

	mt_ppm_ptpod_policy_activate();

	ret = eem_buck_get(pdev);
	if (ret != 0)
		eem_error("eem_buck_get failed\n");

	eem_buck_set_mode(1);

	/* for slow idle */
	ptp_data[0] = 0xffffffff;

	for_each_det(det)
		eem_init_det(det, &eem_devinfo);

	/* get original volt from cpu dvfs before init01*/
	for_each_det(det) {
		if (det->ops->get_orig_volt_table)
			det->ops->get_orig_volt_table(det);
	}

	final_init01_flag = EEM_INIT01_FLAG;

	eem_init01();
	ptp_data[0] = 0;

#if ENABLE_INIT1_STRESS
	init_waitqueue_head(&wqStress);
	threadStress = kthread_run(eem_init1stress_thread_handler, 0,
		"Init_1_Stress");

	if (IS_ERR(threadStress))
		eem_error("Create %s thread failed: %ld\n", "Init_1_Stress",
			PTR_ERR(threadStress));
#endif

	eem_debug("eem_probe ok\n");
	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int eem_suspend(void)
{
	struct eem_det *det;

	eem_error("Start EEM suspend\n");

	for_each_det(det) {
		if (HAS_FEATURE(det, FEA_INIT02) &&
			det->ctrl_id <= EEM_CTRL_CCI) {
			unsigned long flag;

			mt_ptp_lock(&flag);
			memcpy(det->volt_tbl, det->volt_tbl_init2,
			sizeof(det->volt_tbl_init2));
			eem_set_eem_volt(det);
			mt_ptp_unlock(&flag);
		}
	}

	return 0;
}

static int eem_resume(void)
{
	eem_error("Start EEM resume\n");
	eem_init02(__func__);

	return 0;
}

#ifdef CONFIG_OF
#define EEM_DT_NODE "mediatek,eem_fsm"
static const struct of_device_id mt_eem_of_match[] = {
	{ .compatible = EEM_DT_NODE, },
	{},
};
#endif

static struct platform_driver eem_driver = {
	.remove	 = NULL,
	.shutdown   = NULL,
	.probe	  = eem_probe,
	.suspend	= NULL,
	.resume	 = NULL,
	.driver	 = {
	.name   = "mt-eem",
#ifdef CONFIG_OF
	.of_match_table = mt_eem_of_match,
#endif
	},
};

#ifdef CONFIG_PROC_FS
int mt_eem_opp_num(enum eem_det_id id)
{
	struct eem_det *det = id_to_eem_det(id);

	FUNC_ENTER(FUNC_LV_API);
	FUNC_EXIT(FUNC_LV_API);

	return det->num_freq_tbl;
}
EXPORT_SYMBOL(mt_eem_opp_num);

void mt_eem_opp_freq(enum eem_det_id id, unsigned int *freq)
{
	struct eem_det *det = id_to_eem_det(id);
	int i = 0;

	FUNC_ENTER(FUNC_LV_API);

	for (i = 0; i < det->num_freq_tbl; i++)
		freq[i] = det->freq_tbl[i];

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_eem_opp_freq);

void mt_eem_opp_status(enum eem_det_id id, unsigned int *temp,
	unsigned int *volt)
{
	struct eem_det *det = id_to_eem_det(id);
	int i = 0;

	FUNC_ENTER(FUNC_LV_API);

#ifdef CONFIG_THERMAL
	if (id == EEM_DET_L)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK0);
	else if (id == EEM_DET_2L)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK1);
	else if (id == EEM_DET_CCI)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK2);
#if ENABLE_LOO
	else if (id == EEM_DET_L_HI)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK0);
	else if (id == EEM_DET_2L_HI)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK1);
#endif
	else
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK0);
#else
	*temp = 0;
#endif

	for (i = 0; i < det->num_freq_tbl; i++)
		volt[i] = det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_eem_opp_status);

/***************************
 * return current EEM stauts
 ****************************/
int mt_eem_status(enum eem_det_id id)
{
	struct eem_det *det = id_to_eem_det(id);

	FUNC_ENTER(FUNC_LV_API);
	if (det == NULL)
		return 0;
	else if (det->ops == NULL)
		return 0;
	else if (det->ops->get_status == NULL)
		return 0;

	FUNC_EXIT(FUNC_LV_API);

	return det->ops->get_status(det);
}

/**
 * ===============================================
 * PROCFS interface for debugging
 * ===============================================
 */

/*
 * show current EEM stauts
 */
static int eem_debug_proc_show(struct seq_file *m, void *v)
{
	struct eem_det *det = (struct eem_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	/* FIXME: EEMEN sometimes is disabled temp */
	seq_printf(m, "[%s] %s (%d)\n",
		   ((char *)(det->name) + 8),
		   det->disabled ? "disabled" : "enable",
		   det->ops->get_status(det)
		   );

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * set EEM status by procfs interface
 */
static ssize_t eem_debug_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	int enabled = 0;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eem_det *det = (struct eem_det *)PDE_DATA(file_inode(file));

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &enabled)) {
		ret = 0;

		if (enabled == 0)
			/* det->ops->enable(det, BY_PROCFS); */
			det->ops->disable(det, 0);
		else if (enabled == 1)
			det->ops->disable(det, BY_PROCFS);
		else if (enabled == 2)
			det->ops->disable(det, BY_PROCFS_INIT2);

	} else
		ret = -EINVAL;

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

/*
 * show current EEM data
 */
void eem_dump_reg_by_det(struct eem_det *det, struct seq_file *m)
{
	unsigned int i, k;
#if DUMP_DATA_TO_DE
	unsigned int j;
#endif

	for (i = EEM_PHASE_INIT01; i < NR_EEM_PHASE; i++) {
		seq_printf(m, "Bank_number = %d\n", det->ctrl_id);
		if (i < EEM_PHASE_MON)
			seq_printf(m, "mode = init%d\n", i+1);
		else
			seq_puts(m, "mode = mon\n");
		if (eem_log_en) {
			seq_printf(m,
				"0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
				det->dcvalues[i],
				det->freqpct30[i],
				det->eem_26c[i],
				det->vop30[i],
				det->eem_eemEn[i]
			);

			if (det->eem_eemEn[i] == (0x5 | SEC_MOD_SEL)) {
				seq_printf(m,
				"EEM_LOG: Bank_number = [%d] (%d) - (",
				det->ctrl_id, det->ops->get_temp(det));

				for (k = 0; k < det->num_freq_tbl - 1; k++)
					seq_printf(m, "%d, ",
					det->ops->pmic_2_volt(det,
					det->volt_tbl_pmic[k]));
				seq_printf(m, "%d) - (",
					det->ops->pmic_2_volt(det,
					det->volt_tbl_pmic[k]));

				for (k = 0; k < det->num_freq_tbl - 1; k++)
					seq_printf(m, "%d, ",
						det->freq_tbl[k]);
				seq_printf(m, "%d)\n", det->freq_tbl[k]);
			}
		} /* if (eem_log_en) */
#if DUMP_DATA_TO_DE
		for (j = 0; j < ARRAY_SIZE(reg_dump_addr_off); j++)
			seq_printf(m, "0x%08lx = 0x%08x\n",
			(unsigned long)EEM_BASEADDR + reg_dump_addr_off[j],
				det->reg_dump_data[j][i]
				);
#endif
	}
}

static int eem_dump_proc_show(struct seq_file *m, void *v)
{
	struct eem_det *det;
	int *val = (int *)&eem_devinfo;
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	for (i = 0; i < sizeof(struct eem_devinfo) / sizeof(unsigned int);
		i++) {
		/* Depend on EFUSE location */
		if (i < 9)
			seq_printf(m, "M_HW_RES%d\t= 0x%08X\n", i, val[i]);
		else
			seq_printf(m, "M_HW_RES%d\t= 0x%08X\n", i + 2, val[i]);
	}

#if defined(CONFIG_CPU_FORCE_TO_BIN2)
	val = (int *)&ex_eem_devinfo;
	for (i = 0; i < sizeof(struct eem_devinfo_extra) / sizeof(unsigned int);
		i++) {
		/* Depend on EFUSE location */

		seq_printf(m, "Extra M_HW_RES%d\t= 0x%08X\n", i, val[i]);
	}
#endif
	for_each_det(det) {
		eem_dump_reg_by_det(det, m);
	}

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}

/*
 * show current voltage
 */
static int eem_cur_volt_proc_show(struct seq_file *m, void *v)
{
	struct eem_det *det = (struct eem_det *)m->private;
	u32 rdata = 0, i;

	FUNC_ENTER(FUNC_LV_HELP);

	rdata = det->ops->get_volt(det);

	if (rdata != 0)
		seq_printf(m, "%d\n", rdata);
	else
		seq_printf(m, "EEM[%s] read current voltage fail\n",
			det->name);

	if (det->features != 0) {
		for (i = 0; i < det->num_freq_tbl; i++)
			seq_printf(m,
			"[%d],eem = [%x], pmic = [%x], volt = [%d]\n",
			i,
			det->volt_tbl[i],
			det->volt_tbl_pmic[i],
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));
	}
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * show current EEM status
 */
static int eem_status_proc_show(struct seq_file *m, void *v)
{
	int i;
	struct eem_det *det = (struct eem_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	seq_printf(m, "bank = %d, (%d) - (",
		   det->ctrl_id, det->ops->get_temp(det));
	for (i = 0; i < det->num_freq_tbl - 1; i++)
		seq_printf(m, "%d, ",
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));
	seq_printf(m, "%d) - (",
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));

	for (i = 0; i < det->num_freq_tbl - 1; i++)
		seq_printf(m, "%d, ", det->freq_tbl[i]);
	seq_printf(m, "%d)\n", det->freq_tbl[i]);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}
/*
 * set EEM log enable by procfs interface
 */

static int eem_log_en_proc_show(struct seq_file *m, void *v)
{
	FUNC_ENTER(FUNC_LV_HELP);
	seq_printf(m, "%d\n", eem_log_en);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static ssize_t eem_log_en_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	ret = -EINVAL;

	if (kstrtoint(buf, 10, &eem_log_en)) {
		eem_debug("bad argument!! Should be \"0\" or \"1\"\n");
		goto out;
	}

	ret = 0;

	switch (eem_log_en) {
	case 0:
		eem_debug("eem log disabled.\n");
		hrtimer_cancel(&eem_log_timer);
		break;

	case 1:
		eem_debug("eem log enabled.\n");
		hrtimer_start(&eem_log_timer, ns_to_ktime(LOG_INTERVAL),
			HRTIMER_MODE_REL);
		break;

	default:
		eem_debug("bad argument!! Should be \"0\" or \"1\"\n");
		ret = -EINVAL;
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

#if ENABLE_INIT1_STRESS
static int eem_init1stress_en_proc_show(struct seq_file *m, void *v)
{
	FUNC_ENTER(FUNC_LV_HELP);
	seq_printf(m, "%d\n", eem_init1stress_en);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static ssize_t eem_init1stress_en_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	ret = -EINVAL;

	if (kstrtoint(buf, 10, &eem_init1stress_en)) {
		eem_debug("bad argument!! Should be \"0\" or \"1\"\n");
		goto out;
	}

	ret = 0;

	switch (eem_init1stress_en) {
	case 0:
		eem_debug("eem stress init1 disabled.\n");
		break;

	case 1:
		eem_debug("eem stress init1 enabled.\n");
		wake_up_interruptible(&wqStress);
		break;

	default:
		eem_debug("bad argument!! Should be \"0\" or \"1\"\n");
		ret = -EINVAL;
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}
#endif

/*
 * show EEM offset
 */
static int eem_offset_proc_show(struct seq_file *m, void *v)
{
	struct eem_det *det = (struct eem_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	seq_printf(m, "%d\n", det->volt_offset);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * set EEM offset by procfs
 */
static ssize_t eem_offset_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	int offset = 0;
	struct eem_det *det = (struct eem_det *)PDE_DATA(file_inode(file));
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &offset)) {
		ret = 0;
		det->volt_offset = offset;
		mt_ptp_lock(&flags);
		eem_set_eem_volt(det);
		mt_ptp_unlock(&flags);
	} else {
		ret = -EINVAL;
		eem_debug("bad argument_1!! argument should be \"0\"\n");
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

#define PROC_FOPS_RW(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
		.write		  = name ## _proc_write,		\
	}

#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(eem_debug);
PROC_FOPS_RO(eem_status);
PROC_FOPS_RO(eem_cur_volt);
PROC_FOPS_RW(eem_offset);
PROC_FOPS_RO(eem_dump);
PROC_FOPS_RW(eem_log_en);
#if ENABLE_INIT1_STRESS
PROC_FOPS_RW(eem_init1stress_en);
#endif

static int create_procfs(void)
{
	struct proc_dir_entry *eem_dir = NULL;
	struct proc_dir_entry *det_dir = NULL;
	int i;
	struct eem_det *det;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry det_entries[] = {
		PROC_ENTRY(eem_debug),
		PROC_ENTRY(eem_status),
		PROC_ENTRY(eem_cur_volt),
		PROC_ENTRY(eem_offset),
	};

	struct pentry eem_entries[] = {
		PROC_ENTRY(eem_dump),
		PROC_ENTRY(eem_log_en),
#if ENABLE_INIT1_STRESS
		PROC_ENTRY(eem_init1stress_en),
#endif
	};

	FUNC_ENTER(FUNC_LV_HELP);

	/* create procfs root /proc/eem */
	eem_dir = proc_mkdir("eem", NULL);

	if (!eem_dir) {
		eem_error("[%s]: mkdir /proc/eem failed\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

/* if ctrl_EEM_Enable =1, and has efuse value, create other banks procfs */
	if (ctrl_EEM_Enable != 0) {
		for (i = 0; i < ARRAY_SIZE(eem_entries); i++) {
			if (!proc_create(
			eem_entries[i].name, 0664,
			eem_dir, eem_entries[i].fops)) {
				eem_error("[%s]: create /proc/eem/%s failed\n",
					__func__, eem_entries[i].name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -3;
			}
		}

		for_each_det(det) {
			if (det->features == 0)
				continue;

			det_dir = proc_mkdir(det->name, eem_dir);

			if (!det_dir) {
				eem_debug("[%s]: mkdir /proc/eem/%s failed\n",
					__func__, det->name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -2;
			}

			for (i = 0; i < ARRAY_SIZE(det_entries); i++) {
				if (!proc_create_data(det_entries[i].name,
					0664,
					det_dir,
					det_entries[i].fops, det)) {
					eem_debug
("[%s]: create /proc/eem/%s/%s failed\n", __func__,
det->name, det_entries[i].name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -3;
				}
			}
		}
	} /* if (ctrl_EEM_Enable != 0) */

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}
#endif /* CONFIG_PROC_FS */

void eem_set_pi_efuse(enum eem_det_id id, unsigned int pi_efuse)
{
	struct eem_det *det = id_to_eem_det(id);

	if (det->pi_efuse_count >= NR_PI_SHARED_CTRL)
		return;

	det->pi_efuse[det->pi_efuse_count++] = pi_efuse;
}

unsigned int get_efuse_status(void)
{
	return eem_checkEfuse;
}

#ifdef CONFIG_PM
static int eem_pm_event(struct notifier_block *notifier, unsigned long pm_event,
			void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		eem_suspend();
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		eem_resume();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block eem_pm_notifier_func = {
	.notifier_call = eem_pm_event,
	.priority = 0,
};
#endif /* CONFIG_PM */

/*
 * Module driver
 */
static int __init eem_init(void)
{
	int err = 0;
#ifdef DRCC_SUPPORT
	spinlock_t record_lock;
#endif

#ifdef EEM_NOT_READY
	return 0;
#endif

	eem_debug("[EEM] ctrl_EEM_Enable=%d\n", ctrl_EEM_Enable);

#ifdef DRCC_SUPPORT
	spin_lock_init(&record_lock);
#endif

	get_devinfo();
	create_procfs();

	if (ctrl_EEM_Enable == 0) {
		eem_error("ctrl_EEM_Enable = 0x%X\n", ctrl_EEM_Enable);
		FUNC_EXIT(FUNC_LV_MODULE);
		return 0;
	}
	informEEMisReady = 1;

	/* init timer for log / volt */
	hrtimer_init(&eem_log_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	eem_log_timer.function = eem_log_timer_func;

	/*
	 * reg platform device driver
	 */
	err = platform_driver_register(&eem_driver);

	if (err) {
		eem_debug("EEM driver callback register failed..\n");
		FUNC_EXIT(FUNC_LV_MODULE);
		return err;
	}
#ifdef CONFIG_PM
	err = register_pm_notifier(&eem_pm_notifier_func);
	if (err) {
		eem_debug("Failed to register PM notifier.\n");
		return err;
	}
#endif /* CONFIG_PM */

	return 0;
}

static void __exit eem_exit(void)
{
	FUNC_ENTER(FUNC_LV_MODULE);
	eem_debug("eem de-initialization\n");
	FUNC_EXIT(FUNC_LV_MODULE);
}

late_initcall(eem_init); /* late_initcall */
#endif /* EN_EEM */

MODULE_DESCRIPTION("MediaTek EEM Driver v0.3");
MODULE_LICENSE("GPL");

#undef __MTK_EEM_C__
