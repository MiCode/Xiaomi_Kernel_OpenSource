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
#include <linux/string.h>
#include <linux/math64.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/rtc.h>

#ifdef CONFIG_OF
	#include <linux/cpu.h>
	#include <linux/of.h>
	#include <linux/of_irq.h>
	#include <linux/of_address.h>
	#include <linux/of_fdt.h>
	#include <mt-plat/aee.h>
#endif

#include <mt-plat/mtk_chip.h>
/* #include <mt-plat/mtk_gpio.h> */
#include "upmu_common.h"
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
#ifdef CONFIG_MTK_GPU_SUPPORT
#include "mtk_gpufreq.h"
#endif
#include <mt-plat/mtk_devinfo.h>
#include <regulator/consumer.h>
#include "pmic_regulator.h"
#include "pmic_api_buck.h"

#if UPDATE_TO_UPOWER
#include "mtk_upower.h"
#endif

#include "mtk_mcdi_api.h"
#ifdef CORN_LOAD
#include "vpu_dvfs.h"
#include "apu_dvfs.h"
#endif

/****************************************
 * define variables for legacy and eem
 ****************************************
 */
#if ENABLE_INIT1_STRESS
static int eem_init1stress_en, testCnt;
wait_queue_head_t wqStress;
struct task_struct *threadStress;
#endif
static unsigned int eem_aging_enable, segCode, is_warn_on;
static unsigned int eem_clamp_val, pmic_clamp_val;

static unsigned int ctrl_EEM_Enable = 1;
static unsigned int big_2line;
#if ENABLE_GPU
static unsigned int gpu_2line;
#endif

/* Get time stmp to known the time period */
static unsigned long long eem_pTime_us, eem_cTime_us, eem_diff_us;

/* for setting pmic pwm mode and auto mode */
struct regulator *eem_regulator_vproc1;
struct regulator *eem_regulator_vproc2;

#if EN_EEM_THERM_CLK
#if !defined(CONFIG_MTK_CLKMGR)
struct clk *eem_therm_main;		/* main clock for Thermal */
#endif
#endif

static int create_procfs(void);
static void eem_set_eem_volt(struct eem_det *det);
static void eem_restore_eem_volt(struct eem_det *det);
#if !EARLY_PORTING
static void eem_buck_set_mode(unsigned int mode);
#endif
static unsigned int interpolate(unsigned int y1, unsigned int y0,
	unsigned int x1, unsigned int x0, unsigned int ym);
static void eem_fill_freq_table(struct eem_det *det);
static void read_volt_from_VOP(struct eem_det *det);
#if UPDATE_TO_UPOWER
static void eem_update_init2_volt_to_upower
	(struct eem_det *det, unsigned int *pmic_volt);
static enum upower_bank transfer_ptp_to_upower_bank(unsigned int det_id);
#endif
#ifdef CORN_LOAD
static void eem_corner(int testcnt);
static unsigned int corn_init;
static unsigned int vpu_detcount[CORN_SIZE];
static unsigned int mdla_detcount[CORN_SIZE];
static unsigned int arrtype[CORN_SIZE] = {0, DCCONFIG_VAL, 0xaaaaaa, 0xffffff};
unsigned int final_corner_flag;
#endif
/* table used to apply to dvfs at final */
unsigned int record_tbl_locked[NR_FREQ];
unsigned int final_init01_flag;
#if ENABLE_LOO_B
unsigned int bcpu_final_init02_flag;
#endif
unsigned int cur_init02_flag;

DEFINE_MUTEX(record_mutex);
#if ENABLE_LOO_B
DEFINE_MUTEX(bcpu_mutex);
#endif
#if ENABLE_LOO_G
DEFINE_MUTEX(gpu_mutex);
#endif

static struct eem_devinfo eem_devinfo;
static struct hrtimer eem_log_timer;
static DEFINE_SPINLOCK(eem_spinlock);
DEFINE_SPINLOCK(record_spinlock);

#define WAIT_TIME	(2500000)
#define FALL_NUM        (3)
#if SUPPORT_PICACHU
#define PICACHU_SIG					(0xA5)
#define PICACHU_SIGNATURE_SHIFT_BIT	(24)
#endif
/******************************************
 * common variables for legacy ptp
 *******************************************
 */
static int eem_log_en;
static unsigned int eem_checkEfuse = 1;
static unsigned int informEEMisReady;
static int eem_aging;

#if ENABLE_GPU
static int time_val = 1;
#endif

/* Global variable for slow idle*/
unsigned int ptp_data[3] = {0, 0, 0};
static char *cpu_name[3] = {
	"L",
	"BIG",
	"CCI"
};

#ifdef CONFIG_OF
void __iomem *eem_base;
void __iomem *infra_base;
void __iomem *mcucfg_base;
static u32 eem_irq_number;
#endif
#define INFRA_AO_NODE		"mediatek,infracfg_ao"
#define INFRA_EEM_RST		(infra_base + 0x730)
#define INFRA_EEM_CLR		(infra_base + 0x734)

#define MCUCFG_NODE			"mediatek,mcucfg"
#define MCUCFG_AGING		(mcucfg_base + 0xFFEC)

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

#ifndef MC50_LOAD
#if SUPPORT_PICACHU
static void get_picachu_efuse(void)
{
	int *val;
	phys_addr_t picachu_mem_base_phys;
	phys_addr_t picachu_mem_size;
	phys_addr_t picachu_mem_base_virt = 0;
	unsigned int i, cnt, sig;
	void __iomem *addr_ptr;

	val = (int *)&eem_devinfo;

	picachu_mem_size = 0x80000;
	picachu_mem_base_phys = eem_read(EEMSPARE0);
	if ((void __iomem *)picachu_mem_base_phys != NULL)
		picachu_mem_base_virt =
			(phys_addr_t)(uintptr_t)ioremap_wc(
			picachu_mem_base_phys,
			picachu_mem_size);

#if 0
	eem_error("phys:0x%llx, size:0x%llx, virt:0x%llx\n",
		(unsigned long long)picachu_mem_base_phys,
		(unsigned long long)picachu_mem_size,
		(unsigned long long)picachu_mem_base_virt);
#endif
	if ((void __iomem *)(picachu_mem_base_virt) != NULL) {
		/* 0x60000 was reserved for eem efuse using */
		addr_ptr = (void __iomem *)(picachu_mem_base_virt
			+ 0x60000);

		/* check signature */
		sig = (eem_read(addr_ptr) >>
			PICACHU_SIGNATURE_SHIFT_BIT) & 0xff;

		if (sig == PICACHU_SIG) {
			cnt = eem_read(addr_ptr) & 0xff;
			if (cnt > NR_HW_RES_FOR_BANK)
				cnt = NR_HW_RES_FOR_BANK;
			addr_ptr += 4;

			/* check efuse data */
			for (i = 1; i < cnt; i++) {
				if ((i == 1) || (i == 2) ||
					(i == 5) || (i == 6) ||
					(i == 11) || (i == 12) || (i == 15))
					continue;
				else if (eem_read(addr_ptr + i * 4) == 0) {
					eem_error("Wrong PI-OD%d: 0x%x\n",
						i, eem_read(addr_ptr + i * 4));
					return;
				}
			}

			for (i = 1; i < cnt; i++)
				val[i] = eem_read(addr_ptr + i * 4);

		}
	}
}
#endif
#endif

static int get_devinfo(void)
{

#if 0
	struct eem_det *det;
	unsigned int tmp;
	int err;
	struct rtc_device *rtc_dev;
	int efuse_val, year, mon, real_year, real_mon;
	struct rtc_time tm;
#endif
	int ret = 0, i = 0;
	int *val;
	unsigned int safeEfuse = 0;

	FUNC_ENTER(FUNC_LV_HELP);

#if 0
	memset(&tm, 0, sizeof(struct rtc_time));
	rtc_dev = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc_dev) {
		err = rtc_read_time(rtc_dev, &tm);
		if (err < 0)
			pr_info("fail to read time\n");
	} else
		pr_info("[systimer] rtc_class_open rtc_dev fail\n");
#endif

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
	val[12] = get_devinfo_with_index(DEVINFO_IDX_12);
	val[13] = get_devinfo_with_index(DEVINFO_IDX_13);
	val[14] = get_devinfo_with_index(DEVINFO_IDX_14);
	val[15] = get_devinfo_with_index(DEVINFO_IDX_15);
	val[16] = get_devinfo_with_index(DEVINFO_IDX_16);
	val[17] = get_devinfo_with_index(DEVINFO_IDX_17);

	segCode = get_devinfo_with_index(DEVINFO_SEG_IDX)
		& 0xFF;
#if 0
	efuse_val = get_devinfo_with_index(DEVINFO_TIME_IDX);
	year = ((efuse_val >> 4) & 0xf) + 2018;
	mon = efuse_val  & 0xf;
	real_year = tm.tm_year + 1900;
	real_mon = tm.tm_mon + 1;

	if (((real_year - year == 1) && (real_mon > mon)) ||
			(real_year - year > 1))
		time_val = 0;
#endif

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
	val[12] = DEVINFO_12;
	val[13] = DEVINFO_13;
	val[14] = DEVINFO_14;
	val[15] = DEVINFO_15;
	val[16] = DEVINFO_16;
	val[17] = DEVINFO_17;
#endif

	for (i = 0; i < NR_HW_RES_FOR_BANK; i++)
		eem_debug("[PTP_DUMP] RES%d: 0x%X\n",
			i, val[i]);

#ifdef CONFIG_EEM_AEE_RR_REC
	aee_rr_rec_ptp_e0((unsigned int)val[0]);
	aee_rr_rec_ptp_e1((unsigned int)val[1]);
	aee_rr_rec_ptp_e2((unsigned int)val[2]);
	aee_rr_rec_ptp_e3((unsigned int)val[3]);
	aee_rr_rec_ptp_e4((unsigned int)val[4]);
	aee_rr_rec_ptp_e5((unsigned int)val[5]);
	aee_rr_rec_ptp_e6((unsigned int)val[6]);
	aee_rr_rec_ptp_e7((unsigned int)val[7]);
	aee_rr_rec_ptp_e8((unsigned int)val[8]);
	aee_rr_rec_ptp_e9((unsigned int)val[9]);
	aee_rr_rec_ptp_e10((unsigned int)val[10]);
	aee_rr_rec_ptp_e11((unsigned int)val[11]);
	aee_rr_rec_ptp_devinfo_0((unsigned int)val[12]);
	aee_rr_rec_ptp_devinfo_1((unsigned int)val[13]);
	aee_rr_rec_ptp_devinfo_2((unsigned int)val[14]);
	aee_rr_rec_ptp_devinfo_3((unsigned int)val[15]);
	aee_rr_rec_ptp_devinfo_4((unsigned int)val[16]);
	aee_rr_rec_ptp_devinfo_5((unsigned int)val[17]);
#endif
#if ENABLE_LOO_B
	big_2line = 1;
#else
	big_2line = 0;
#endif
#if ENABLE_GPU
#if ENABLE_LOO_G
	gpu_2line = 1;
#else
	gpu_2line = 0;
#endif
#endif

	for (i = 1; i < NR_HW_RES_FOR_BANK; i++) {
		if ((i == 1) || (i == 2) ||
			(i == 5) || (i == 6) ||
			(i == 11) || (i == 12) || (i == 15))
			continue;
		else if (val[i] == 0) {
			ret = 1;
			safeEfuse = 1;
			eem_error("No EFUSE (val[%d]), use safe efuse\n", i);
			break;
		}
	}

#if (EEM_FAKE_EFUSE)
	eem_checkEfuse = 1;
#endif

#ifdef MC50_LOAD
	safeEfuse = 1;
#endif
	if (safeEfuse) {
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
		val[12] = DEVINFO_12;
		val[13] = DEVINFO_13;
		val[14] = DEVINFO_14;
		val[15] = DEVINFO_15;
		val[16] = DEVINFO_16;
		val[17] = DEVINFO_17;
	}


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

	coresel = (eem_read(EEMCORESEL) & ~BITMASK(2:0))
		| BITS(2:0, det->ctrl_id);

	/* 803f0000 + det->ctrl_id = enable ctrl's swcg clock */
	/* 003f0000 + det->ctrl_id = disable ctrl's swcg clock */
	/* bug: when system resume, need to restore coresel value */
	if (phase == EEM_PHASE_INIT01) {
		coresel |= CORESEL_VAL;
	} else {
		coresel |= CORESEL_INIT2_VAL;
#if defined(CFG_THERM_LVTS) && CFG_LVTS_DOMINATOR
		coresel &= 0x0fffffff;
#else
		coresel &= 0x0ffffeff;  /* get temp from AUXADC */
#endif
	}
	eem_write(EEMCORESEL, coresel);

	if ((eem_read(EEMCORESEL) & 0x7) != (coresel & 0x7)) {
		aee_kernel_warning("mt_eem",
		"@%s():%d, EEMCORESEL %x != %x\n",
		__func__,
		__LINE__,
		eem_read(EEMCORESEL),
		coresel);
		WARN_ON(eem_read(EEMCORESEL) != coresel);
	}

	eem_debug("[%s] 0x1100bf00=0x%x\n",
			((char *)(det->name) + 8), eem_read(EEMCORESEL));

	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_disable_locked(struct eem_det *det, int reason)
{
	FUNC_ENTER(FUNC_LV_HELP);

	switch (reason) {
	case BY_MON_ERROR: /* 4 */
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
		eem_debug("det->disabled=%x\n", det->disabled);
		eem_set_eem_volt(det);
		break;
	}

	eem_debug("Disable EEM[%s] done. reason=[%d]\n",
			det->name, det->disabled);

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
	/* eem_debug("DCV = 0x%08X, AGEV = 0x%08X\n",
	 * det->DCVOFFSETIN, det->AGEVOFFSETIN);
	 */

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
		eem_debug("det %s has no MON mode\n", det->name);
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
		ts_bank = THERMAL_BANK2;
	else if (det_to_id(det) == EEM_DET_B)
		ts_bank = THERMAL_BANK0;
	else if (det_to_id(det) == EEM_DET_CCI)
		ts_bank = THERMAL_BANK2;
#if ENABLE_GPU
	else if (det_to_id(det) == EEM_DET_GPU)
		ts_bank = THERMAL_BANK4;
#endif
#if ENABLE_VPU
	else if (det_to_id(det) == EEM_DET_VPU)
		ts_bank = THERMAL_BANK3;
#endif
#if ENABLE_MDLA
	else if (det_to_id(det) == EEM_DET_MDLA)
		ts_bank = THERMAL_BANK3;
#endif
#if ENABLE_GPU
#if ENABLE_LOO_G
	else if (det_to_id(det) == EEM_DET_GPU_HI)
		ts_bank = THERMAL_BANK4;
#endif
#endif
#if ENABLE_LOO_B
	else if (det_to_id(det) == EEM_DET_B_HI)
		ts_bank = THERMAL_BANK0;
#endif
	else
		ts_bank = THERMAL_BANK0;

#if defined(CFG_THERM_LVTS) && CFG_LVTS_DOMINATOR
	get_lvts_slope_intercept(&ts_info, ts_bank);
#else
	get_thermal_slope_intercept(&ts_info, ts_bank);
#endif
	det->MTS = ts_info.ts_MTS;
	det->BTS = ts_info.ts_BTS;
#endif

	/*
	 * eem_debug("[base_ops_mon_mode] Bk = %d,
	 * MTS = 0x%08X, BTS = 0x%08X\n",
	 * det->ctrl_id, det->MTS, det->BTS);
	 */
#if 0
	if ((det->EEMINITEN == 0x0) || (det->EEMMONEN == 0x0)) {
		eem_debug("EEMINITEN = 0x%08X, EEMMONEN = 0x%08X\n",
				det->EEMINITEN, det->EEMMONEN);
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
		eem_isr_info("freq_tbl[%d] = %d\n",
				i, det->freq_tbl[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		eem_isr_info("volt_tbl[%d] = %d\n",
				i, det->volt_tbl[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		eem_isr_info("volt_tbl_init2[%d] = %d\n",
				i, det->volt_tbl_init2[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		eem_isr_info("volt_tbl_pmic[%d] = %d\n", i,
				det->volt_tbl_pmic[i]);

	FUNC_EXIT(FUNC_LV_HELP);
}


void dump_register(void)
{

	eem_debug("EEM_DESCHAR = 0x%x\n", eem_read(EEM_DESCHAR));
	eem_debug("EEM_TEMPCHAR = 0x%x\n", eem_read(EEM_TEMPCHAR));
	eem_debug("EEM_DETCHAR = 0x%x\n", eem_read(EEM_DETCHAR));
	eem_debug("EEM_AGECHAR = 0x%x\n", eem_read(EEM_AGECHAR));
	eem_debug("EEM_DCCONFIG = 0x%x\n", eem_read(EEM_DCCONFIG));
	eem_debug("EEM_AGECONFIG = 0x%x\n", eem_read(EEM_AGECONFIG));
	eem_debug("EEM_FREQPCT30 = 0x%x\n", eem_read(EEM_FREQPCT30));
	eem_debug("EEM_FREQPCT74 = 0x%x\n", eem_read(EEM_FREQPCT74));
	eem_debug("EEM_LIMITVALS = 0x%x\n", eem_read(EEM_LIMITVALS));
	eem_debug("EEM_VBOOT = 0x%x\n", eem_read(EEM_VBOOT));
	eem_debug("EEM_DETWINDOW = 0x%x\n", eem_read(EEM_DETWINDOW));
	eem_debug("EEMCONFIG = 0x%x\n", eem_read(EEMCONFIG));
	eem_debug("EEM_TSCALCS = 0x%x\n", eem_read(EEM_TSCALCS));
	eem_debug("EEM_RUNCONFIG = 0x%x\n", eem_read(EEM_RUNCONFIG));
	eem_debug("EEMEN = 0x%x\n", eem_read(EEMEN));
	eem_debug("EEM_INIT2VALS = 0x%x\n", eem_read(EEM_INIT2VALS));
	eem_debug("EEM_DCVALUES = 0x%x\n", eem_read(EEM_DCVALUES));
	eem_debug("EEM_AGEVALUES = 0x%x\n", eem_read(EEM_AGEVALUES));
	eem_debug("EEM_VOP30 = 0x%x\n", eem_read(EEM_VOP30));
	eem_debug("EEM_VOP74 = 0x%x\n", eem_read(EEM_VOP74));
	eem_debug("TEMP = 0x%x\n", eem_read(TEMP));
	eem_debug("EEMINTSTS = 0x%x\n", eem_read(EEMINTSTS));
	eem_debug("EEMINTSTSRAW = 0x%x\n", eem_read(EEMINTSTSRAW));
	eem_debug("EEMINTEN = 0x%x\n", eem_read(EEMINTEN));
	eem_debug("EEM_CHKSHIFT = 0x%x\n", eem_read(EEM_CHKSHIFT));
	eem_debug("EEM_VDESIGN30 = 0x%x\n", eem_read(EEM_VDESIGN30));
	eem_debug("EEM_VDESIGN74 = 0x%x\n", eem_read(EEM_VDESIGN74));
	eem_debug("EEM_AGECOUNT = 0x%x\n", eem_read(EEM_AGECOUNT));
	eem_debug("EEM_SMSTATE0 = 0x%x\n", eem_read(EEM_SMSTATE0));
	eem_debug("EEM_SMSTATE1 = 0x%x\n", eem_read(EEM_SMSTATE1));
	eem_debug("EEM_CTL0 = 0x%x\n", eem_read(EEM_CTL0));
	eem_debug("EEMCORESEL = 0x%x\n", eem_read(EEMCORESEL));
	eem_debug("EEM_THERMINTST = 0x%x\n", eem_read(EEM_THERMINTST));
	eem_debug("EEMODINTST = 0x%x\n", eem_read(EEMODINTST));
	eem_debug("EEM_THSTAGE0ST = 0x%x\n", eem_read(EEM_THSTAGE0ST));
	eem_debug("EEM_THSTAGE1ST = 0x%x\n", eem_read(EEM_THSTAGE1ST));
	eem_debug("EEM_THSTAGE2ST = 0x%x\n", eem_read(EEM_THSTAGE2ST));
	eem_debug("EEM_THAHBST0 = 0x%x\n", eem_read(EEM_THAHBST0));
	eem_debug("EEM_THAHBST1 = 0x%x\n", eem_read(EEM_THAHBST1));
	eem_debug("EEMSPARE0 = 0x%x\n", eem_read(EEMSPARE0));
	eem_debug("EEMSPARE1 = 0x%x\n", eem_read(EEMSPARE1));
	eem_debug("EEMSPARE2 = 0x%x\n", eem_read(EEMSPARE2));
	eem_debug("EEMSPARE3 = 0x%x\n", eem_read(EEMSPARE3));
	eem_debug("EEM_THSLPEVEB = 0x%x\n", eem_read(EEM_THSLPEVEB));
	eem_debug("EEM_TEMP = 0x%x\n", eem_read(TEMP));
	eem_debug("EEM_THERMAL = 0x%x\n", eem_read(EEM_THERMAL));

}

void base_ops_set_phase(struct eem_det *det, enum eem_phase phase)
{
	unsigned int i, filter, val;

	FUNC_ENTER(FUNC_LV_HELP);
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

	eem_fill_freq_table(det);

	eem_write(EEM_LIMITVALS,
		  ((det->VMAX << 24) & 0xff000000)	|
		  ((det->VMIN << 16) & 0xff0000)	|
		  ((det->DTHI << 8) & 0xff00)		|
		  (det->DTLO & 0xff));
	/* eem_write(EEM_LIMITVALS, 0xFF0001FE); */
	eem_write(EEM_VBOOT, (((det->VBOOT) & 0xff)));
	eem_write(EEM_DETWINDOW, (((det->DETWINDOW) & 0xffff)));
	eem_write(EEMCONFIG, (((det->DETMAX) & 0xffff)));


	eem_write(EEM_CHKSHIFT, 0x87);
	if (eem_read(EEM_CHKSHIFT) != 0x87) {
		aee_kernel_warning("mt_eem",
		"@%s():%d, EEM_CHKSHIFT %x\n",
		__func__,
		__LINE__,
		eem_read(EEM_CHKSHIFT));
		WARN_ON(eem_read(EEM_CHKSHIFT) != 0x87);
	}

	/* eem ctrl choose thermal sensors */
	eem_write(EEM_CTL0, det->EEMCTL0);
	/* clear all pending EEM interrupt & config EEMINTEN */
	eem_write(EEMINTSTS, 0xffffffff);

	/* work around set thermal register
	 * eem_write(EEM_THERMAL, 0x200);
	 */

	eem_debug(" %s set phase = %d\n", ((char *)(det->name) + 8), phase);
	switch (phase) {
	case EEM_PHASE_INIT01:
		eem_debug("EEM_SET_PHASE01\n ");
		eem_write(EEMINTEN, 0x00005f01);
		/* enable EEM INIT measurement */
		eem_write(EEMEN, 0x00000001 | SEC_MOD_SEL);
		if (eem_read(EEMEN) != (0x00000001 | SEC_MOD_SEL)) {
			aee_kernel_warning("mt_eem",
			"@%s():%d, EEMEN %x\n",
			__func__,
			__LINE__,
			eem_read(EEMEN));
			WARN_ON(eem_read(EEMEN) != (0x00000001 | SEC_MOD_SEL));
		}
		eem_debug("EEMEN = 0x%x, EEMINTEN = 0x%x\n",
				eem_read(EEMEN), eem_read(EEMINTEN));
		dump_register();
		udelay(250); /* all banks' phase cannot be set without delay */
		break;

	case EEM_PHASE_INIT02:
		/* check if DCVALUES is minus and set DCVOFFSETIN to zero */
		if ((det->DCVOFFSETIN & 0x8000) || (eem_devinfo.FT_PGM == 0))
			det->DCVOFFSETIN = 0;

#if ENABLE_MINIHQA
		det->DCVOFFSETIN = 0;
#endif

		eem_debug("EEM_SET_PHASE02\n ");
		eem_write(EEMINTEN, 0x00005f01);

		eem_write(EEM_INIT2VALS,
			  ((det->AGEVOFFSETIN << 16) & 0xffff0000) |
			  (det->DCVOFFSETIN & 0xffff));

		/* enable EEM INIT measurement */
		eem_write(EEMEN, 0x00000005 | SEC_MOD_SEL);
		if (eem_read(EEMEN) != (0x00000005 | SEC_MOD_SEL)) {
			aee_kernel_warning("mt_eem",
			"@%s():%d, EEMEN %x\n",
			__func__,
			__LINE__,
			eem_read(EEMEN));
			WARN_ON(eem_read(EEMEN) != (0x00000005 | SEC_MOD_SEL));
		}

		dump_register();
		udelay(200); /* all banks' phase cannot be set without delay */
		break;

	case EEM_PHASE_MON:
		eem_debug("EEM_SET_PHASE_MON\n ");
		eem_write(EEMINTEN, 0x00FF0000);
		/* enable EEM monitor mode */
		eem_write(EEMEN, 0x00000002 | SEC_MOD_SEL);
		/* dump_register(); */
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
		ts_bank = THERMAL_BANK2;
	else if (det_to_id(det) == EEM_DET_B)
		ts_bank = THERMAL_BANK0;
	else if (det_to_id(det) == EEM_DET_CCI)
		ts_bank = THERMAL_BANK2;
#if ENABLE_GPU
	else if (det_to_id(det) == EEM_DET_GPU)
		ts_bank = THERMAL_BANK4;

#if ENABLE_LOO_G
	else if (det_to_id(det) == EEM_DET_GPU_HI)
		ts_bank = THERMAL_BANK4;
#endif
#endif
#if ENABLE_MDLA
	else if (det_to_id(det) == EEM_DET_MDLA)
		ts_bank = THERMAL_BANK3;
#endif
#if ENABLE_VPU
	else if (det_to_id(det) == EEM_DET_VPU)
		ts_bank = THERMAL_BANK3;
#endif
#if ENABLE_LOO_B
	else if (det_to_id(det) == EEM_DET_B_HI)
		ts_bank = THERMAL_BANK0;
#endif
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

		eem_debug("Timer Bk=%d (%d)(%d, %d, %d, %d, %d, %d, %d, %d)(0x%x)\n",
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

static void eem_calculate_aging_margin(struct eem_det *det,
	int start_oft, int end_oft)
{

	int num_bank_freq, offset, i = 0;


	num_bank_freq = det->num_freq_tbl;

	offset = start_oft - end_oft;
	for (i = 0; i < det->num_freq_tbl; i++) {
		if (i == 0)
			det->volt_aging[i] = start_oft;
		else
			det->volt_aging[i] = start_oft -
			((offset * i) / det->num_freq_tbl);
	}

}

static void eem_save_final_volt_aee(struct eem_det *ndet)
{
#ifdef CONFIG_EEM_AEE_RR_REC
	int i;

	if (ndet == NULL)
		return;

	for (i = 0; i < NR_FREQ; i++) {
		switch (ndet->ctrl_id) {
		case EEM_CTRL_L:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_little_volt_2(
				((unsigned long long)(ndet->volt_tbl_pmic[i])
				<< (8 * i)) |
				(aee_rr_curr_ptp_cpu_little_volt_2() & ~
				((unsigned long long)(0xFF) << (8 * i)))
				);
			} else {
				aee_rr_rec_ptp_cpu_little_volt_3(
				((unsigned long long)(ndet->volt_tbl_pmic[i])
				 << (8 * (i - 8))) |
				(aee_rr_curr_ptp_cpu_little_volt_3() & ~
					((unsigned long long)(0xFF)
					<< (8 * (i - 8))))
				);
			}
			break;

		case EEM_CTRL_B:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_big_volt_2(
					((unsigned long long)
					(ndet->volt_tbl_pmic[i])
					 << (8 * i)) |
					(aee_rr_curr_ptp_cpu_big_volt_2() & ~
						((unsigned long long)(0xFF)
						 << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_big_volt_3(
					((unsigned long long)
					(ndet->volt_tbl_pmic[i])
					 << (8 * (i - 8))) |
					(aee_rr_curr_ptp_cpu_big_volt_3() & ~
						((unsigned long long)(0xFF)
						 << (8 * (i - 8)))
					)
				);
			}
			break;
		case EEM_CTRL_CCI:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_cci_volt_2(
					((unsigned long long)
					(ndet->volt_tbl_pmic[i])
					 << (8 * i)) |
					(aee_rr_curr_ptp_cpu_cci_volt_2() & ~
						((unsigned long long)(0xFF)
						 << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_cci_volt_3(
					((unsigned long long)
					(ndet->volt_tbl_pmic[i])
					 << (8 * (i - 8))) |
					(aee_rr_curr_ptp_cpu_cci_volt_3() & ~
						((unsigned long long)(0xFF)
						 << (8 * (i - 8)))
					)
				);
			}
			break;
#if ENABLE_GPU
		case EEM_CTRL_GPU:
			if (i < 8) {
				aee_rr_rec_ptp_gpu_volt_2(
					((unsigned long long)
					(ndet->volt_tbl_pmic[i])
					 << (8 * i)) |
					(aee_rr_curr_ptp_gpu_volt_2() & ~
						((unsigned long long)(0xFF)
						 << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_gpu_volt_3(
				((unsigned long long)(ndet->volt_tbl_pmic[i]) <<
				(8 * (i - 8))) |
				(aee_rr_curr_ptp_gpu_volt_3() & ~
				((unsigned long long)(0xFF) <<
				(8 * (i - 8)))
				)
				);
			}
			break;
#endif
		default:
			break;
		}
	}
#endif
}

static void get_volt_table_in_thread(struct eem_det *det)
{
#if ENABLE_LOO
#if UPDATE_TO_UPOWER
	unsigned int init2chk = 0;
#endif
	struct eem_det *highdet;
#endif
	struct eem_det *ndet = det;
	unsigned int i, verr = 0;
	int low_temp_offset = 0, rm_dvtfix_offset = 0;
	unsigned int t_clamp = 0, final_clamp_val, bcpu_opp0_eemv;
#if ENABLE_GPU
	int extra_aging = 0;
#endif

	if (det == NULL)
		return;

#if ENABLE_LOO
	if (det->loo_role != NO_LOO_BANK)
		mutex_lock(det->loo_mutex);

	ndet = (det->loo_role == HIGH_BANK) ?
		id_to_eem_det(det->loo_couple) : det;
#endif
	eem_debug("@@! In %s\n", __func__);
	read_volt_from_VOP(det);

	/* Copy volt to volt_tbl_init2 */
	if (det->init2_done == 0) {
		/* To check BCPU banks init2 isr */
		if (det->ctrl_id == EEM_CTRL_B)
			cur_init02_flag |= BIT(det->ctrl_id);
#if ENABLE_LOO_B
		else if (det->ctrl_id == EEM_CTRL_B_HI)
			cur_init02_flag |= BIT(det->ctrl_id);
#endif

#if ENABLE_LOO
		/* Receive init2 isr and update init2 volt_table */
		if (det->loo_role == HIGH_BANK) {
			memcpy(ndet->volt_tbl_init2, det->volt_tbl,
				sizeof(int) * det->turn_pt);
			memcpy(det->volt_tbl_init2, det->volt_tbl,
				sizeof(int) * det->turn_pt);
		} else if (det->loo_role == LOW_BANK) {
			memcpy(&(det->volt_tbl_init2[det->turn_pt]),
				&(det->volt_tbl[det->turn_pt]),
				sizeof(int) *
				(det->num_freq_tbl - det->turn_pt));
		} else
			memcpy(det->volt_tbl_init2, det->volt_tbl,
				sizeof(det->volt_tbl_init2));
#else
		/* backup to volt_tbl_init2 */
		memcpy(det->volt_tbl_init2, det->volt_tbl,
			sizeof(det->volt_tbl_init2));
#endif
		det->init2_done = 1;
	}

#if ENABLE_LOO
	/* Copy high bank volt to volt_tbl
	 * remap to L/B bank for update dvfs table, also copy
	 * high opp volt table
	 * Band HIGHL will update its volt table (opp0~7) to bank L
	 */
	if (det->loo_role == HIGH_BANK) {
		memcpy(ndet->volt_tbl, det->volt_tbl,
				sizeof(int) * det->turn_pt);
	}
#endif

	for (i = 0; i < NR_FREQ; i++) {
#ifdef CONFIG_EEM_AEE_RR_REC
		switch (ndet->ctrl_id) {
		case EEM_CTRL_L:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_little_volt(
				((unsigned long long)(ndet->volt_tbl[i])
				<< (8 * i)) |
				(aee_rr_curr_ptp_cpu_little_volt() & ~
				((unsigned long long)(0xFF) << (8 * i)))
				);
			} else {
				aee_rr_rec_ptp_cpu_little_volt_1(
				((unsigned long long)(ndet->volt_tbl[i])
				 << (8 * (i - 8))) |
				(aee_rr_curr_ptp_cpu_little_volt_1() & ~
					((unsigned long long)(0xFF)
					<< (8 * (i - 8))))
				);
			}
			break;

		case EEM_CTRL_B:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_big_volt(
					((unsigned long long)(ndet->volt_tbl[i])
					 << (8 * i)) |
					(aee_rr_curr_ptp_cpu_big_volt() & ~
						((unsigned long long)(0xFF)
						 << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_big_volt_1(
					((unsigned long long)(ndet->volt_tbl[i])
					 << (8 * (i - 8))) |
					(aee_rr_curr_ptp_cpu_big_volt_1() & ~
						((unsigned long long)(0xFF)
						 << (8 * (i - 8)))
					)
				);
			}
			break;
		case EEM_CTRL_CCI:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_cci_volt(
					((unsigned long long)(ndet->volt_tbl[i])
					 << (8 * i)) |
					(aee_rr_curr_ptp_cpu_cci_volt() & ~
						((unsigned long long)(0xFF)
						 << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_cci_volt_1(
					((unsigned long long)(ndet->volt_tbl[i])
					 << (8 * (i - 8))) |
					(aee_rr_curr_ptp_cpu_cci_volt_1() & ~
						((unsigned long long)(0xFF)
						 << (8 * (i - 8)))
					)
				);
			}
			break;
#if ENABLE_GPU
		case EEM_CTRL_GPU:
			if (i < 8) {
				aee_rr_rec_ptp_gpu_volt(
					((unsigned long long)(ndet->volt_tbl[i])
					 << (8 * i)) |
					(aee_rr_curr_ptp_gpu_volt() & ~
						((unsigned long long)(0xFF)
						 << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_gpu_volt_1(
				((unsigned long long)(ndet->volt_tbl[i]) <<
				(8 * (i - 8))) |
				(aee_rr_curr_ptp_gpu_volt_1() & ~
				((unsigned long long)(0xFF) <<
				(8 * (i - 8)))
				)
				);
			}
			break;
#endif
		default:
			break;
		}
#endif


		if ((i > 0) && ((i % 4) != 0) &&
#if ENABLE_LOO
			(i != det->turn_pt) &&
#endif
			(ndet->volt_tbl[i] > ndet->volt_tbl[i-1])) {

			verr = 1;
			aee_kernel_warning("mt_eem",
				"@%s():%d; (%s) [%d] = [%x] > [%d] = [%x]\n",
				__func__, __LINE__, ((char *)(ndet->name) + 8),
				i, ndet->volt_tbl[i], i-1, ndet->volt_tbl[i-1]);

			aee_kernel_warning("mt_eem",
			"@%s():%d; (%s) V30_[0x%x], V74_[0x%x],",
				__func__, __LINE__, ((char *)(det->name) + 8),
				ndet->mon_vop30, ndet->mon_vop74);

			WARN_ON(ndet->volt_tbl[i] > ndet->volt_tbl[i-1]);
		}

		/*
		 *eem_debug("mon_[%s].volt_tbl[%d] = 0x%X (%d)\n",
		 *	det->name, i, det->volt_tbl[i],
		 *	det->ops->pmic_2_volt(det, det->volt_tbl[i]));

		 *if (NR_FREQ > 8) {
		 *	eem_isr_info("mon_[%s].volt_tbl[%d] = 0x%X (%d)\n",
		 *	det->name, i+1, det->volt_tbl[i+1],
		 *	det->ops->pmic_2_volt(det, det->volt_tbl[i+1]));
		 *}
		 */
	}

	if (verr == 1)
		memcpy(ndet->volt_tbl, ndet->volt_tbl_init2,
				sizeof(ndet->volt_tbl));

	/* Check Temperature */
	ndet->temp = ndet->ops->get_temp(det);

#if UPDATE_TO_UPOWER
	upower_update_degree_by_eem
		(transfer_ptp_to_upower_bank(det_to_id(ndet)),
		ndet->temp/1000);
#endif

	if (ndet->temp <= EXTRA_LOW_TEMP_VAL)
		ndet->isTempInv = EEM_EXTRALOW_T;
	else if (ndet->temp <= LOW_TEMP_VAL)
		ndet->isTempInv = EEM_LOW_T;
	else if (ndet->temp >= HIGH_TEMP_VAL)
		ndet->isTempInv = EEM_HIGH_T;
	else
		ndet->isTempInv = 0;

#ifdef CONFIG_THERMAL
	if (!tscpu_is_temp_valid())
		ndet->isTempInv = EEM_LOW_T;
#endif

#if ENABLE_GPU
	if (ndet->ctrl_id == EEM_CTRL_GPU) {
		if (ndet->isTempInv) {
			ndet->low_temp_off =
			(ndet->temp <= INVERT_LOW_TEMP_VAL) ?
			EXTRA_LOW_TEMP_OFF_GPU : EXTRA_TEMP_OFF_GPU;
			t_clamp = ndet->low_temp_off;
		} else
			t_clamp = 0;
	}
#endif

	/* scale of det->volt_offset must equal 10uV */
	/* if has record table, min with record table of each cpu */
	for (i = 0; i < ndet->num_freq_tbl; i++) {
#if !ENABLE_MINIHQA
		if (ndet->volt_policy) {
			if ((ndet->loo_role != NO_LOO_BANK) &&
				(i < det->turn_pt)) {
				highdet = id_to_eem_det(ndet->loo_couple);
				rm_dvtfix_offset = 0 - highdet->DVTFIXED;
			} else
				rm_dvtfix_offset = 0 - ndet->DVTFIXED;
		} else {
#if ENABLE_LOO
			if ((ndet->loo_role != NO_LOO_BANK) &&
				(i < det->turn_pt)) {
				highdet = id_to_eem_det(ndet->loo_couple);
				rm_dvtfix_offset = min(
				((highdet->DVTFIXED * highdet->freq_tbl[i]
				+ (100 - 1)) / 100),
				highdet->DVTFIXED) - highdet->DVTFIXED;
			} else
#endif
				rm_dvtfix_offset = min(
				((ndet->DVTFIXED * ndet->freq_tbl[i]
				+ (100 - 1)) / 100),
				ndet->DVTFIXED) - ndet->DVTFIXED;
		}
#endif

		/* Add low temp offset for each bank if temp inverse */
		if (ndet->isTempInv) {
#if ENABLE_LOO
			if ((ndet->loo_role != NO_LOO_BANK) &&
				(i < det->turn_pt)) {
				highdet = id_to_eem_det(ndet->loo_couple);
				low_temp_offset =
				(highdet->isTempInv == EEM_HIGH_T) ?
				highdet->high_temp_off : highdet->low_temp_off;
			} else
#endif
				low_temp_offset =
				(ndet->isTempInv == EEM_HIGH_T) ?
				ndet->high_temp_off : ndet->low_temp_off;

			if (ndet->isTempInv != EEM_HIGH_T)
				t_clamp = low_temp_offset;
		}

		/* For low temp, re-calculate max clamp value */
		if (t_clamp) {
			final_clamp_val = ndet->volt_tbl_orig[i] +
				ndet->volt_clamp + t_clamp;
			if (final_clamp_val > CPU_PMIC_VMAX_CLAMP)
				final_clamp_val = CPU_PMIC_VMAX_CLAMP;
		} else
			final_clamp_val = ndet->volt_tbl_orig[i] +
			ndet->volt_clamp;

		if ((ndet->ctrl_id == EEM_CTRL_B) && (segCode == 0x10))
			final_clamp_val = min(pmic_clamp_val, final_clamp_val);

#if ENABLE_GPU
		if (time_val && (i < FALL_NUM))
			extra_aging = 1;
		else
			extra_aging = 0;
#endif
		switch (ndet->ctrl_id) {
		case EEM_CTRL_L:
			ndet->volt_tbl_pmic[i] = min(
			(unsigned int)(clamp(
			ndet->ops->eem_2_pmic(ndet,
			(ndet->volt_tbl[i] + ndet->volt_offset +
			low_temp_offset + ndet->volt_aging[i]) +
			rm_dvtfix_offset + eem_aging),
			ndet->ops->eem_2_pmic(ndet, ndet->VMIN),
			ndet->ops->eem_2_pmic(ndet, ndet->VMAX))),
			final_clamp_val);
			break;

		case EEM_CTRL_B:
			if ((i == 0) && (det->loo_role == HIGH_BANK) &&
				(segCode == 0x10) && (is_warn_on == 0) &&
				(eem_devinfo.CPU_L_HI_SPEC == 0)) {
				bcpu_opp0_eemv =
					det->volt_tbl[0] + rm_dvtfix_offset;
				if (bcpu_opp0_eemv > eem_clamp_val) {
					aee_rr_rec_ptp_devinfo_7(
					(unsigned int)det->ops->pmic_2_volt(det,
					det->ops->eem_2_pmic(det,
					bcpu_opp0_eemv)));
					is_warn_on = 1;
				}
			}

			ndet->volt_tbl_pmic[i] = min(
			(unsigned int)(clamp(
				ndet->ops->eem_2_pmic(ndet,
				(ndet->volt_tbl[i] + ndet->volt_offset +
				low_temp_offset + ndet->volt_aging[i]) +
				rm_dvtfix_offset + eem_aging),
				ndet->ops->eem_2_pmic(ndet, ndet->VMIN),
				ndet->ops->eem_2_pmic(ndet, ndet->VMAX))),
				final_clamp_val);

			break;

		case EEM_CTRL_CCI:
			ndet->volt_tbl_pmic[i] = min(
			(unsigned int)(clamp(
				ndet->ops->eem_2_pmic(ndet,
				(ndet->volt_tbl[i] + ndet->volt_offset +
				low_temp_offset + ndet->volt_aging[i]) +
				rm_dvtfix_offset + eem_aging),
				ndet->ops->eem_2_pmic(ndet, ndet->VMIN),
				ndet->ops->eem_2_pmic(ndet, ndet->VMAX))),
				final_clamp_val);
			break;
#if ENABLE_GPU
		case EEM_CTRL_GPU:
			ndet->volt_tbl_pmic[i] = min(
			(unsigned int)(clamp(
			ndet->ops->eem_2_pmic(ndet,
			(ndet->volt_tbl[i] + ndet->volt_offset +
			ndet->volt_aging[i]) +
			rm_dvtfix_offset),
			ndet->ops->eem_2_pmic(ndet, ndet->VMIN),
			ndet->ops->eem_2_pmic(ndet, VMAX_VAL_GPU)) +
			low_temp_offset - extra_aging),
			ndet->volt_tbl_orig[i] + ndet->volt_clamp +
			t_clamp);
			break;
#endif
#if ENABLE_VPU
		case EEM_CTRL_VPU:
			ndet->volt_tbl_pmic[i] = min(
			(unsigned int)(clamp(
				ndet->ops->eem_2_pmic(ndet,
				(ndet->volt_tbl[i] + ndet->volt_offset +
				low_temp_offset + ndet->volt_aging[i]) +
				rm_dvtfix_offset),
				ndet->ops->eem_2_pmic(ndet, ndet->VMIN),
				ndet->ops->eem_2_pmic(ndet, ndet->VMAX))),
				ndet->volt_tbl_orig[i] + ndet->volt_clamp);
			break;
#endif
		default:
			eem_error("[eem_set_eem_volt] incorrect det :%s!!",
					ndet->name);
			break;
		}
#if 0
		eem_error("[%s].volt[%d]=0x%X, Ori[0x%x], pmic[%d]=0x%x(%d)\n",
			det->name,
			i, det->volt_tbl[i], det->volt_tbl_orig[i],
			i, det->volt_tbl_pmic[i], det->ops->pmic_2_volt
			(det, det->volt_tbl_pmic[i]));
		eem_error("low_temp_offset:%d, rm_dvtfix:%d, 'det'->id:%d\n",
			low_temp_offset, rm_dvtfix_offset, det->ctrl_id);
#endif
#if ENABLE_LOO
		if ((i > 0) && (ndet->volt_tbl_pmic[i] >
			ndet->volt_tbl_pmic[i-1]) &&
			(ndet->set_volt_to_upower)) {
				/*                 _ /            */
				/*                /_/             */
				/*               /                */
			if (det->loo_role == HIGH_BANK)
				/* Receive high bank isr but low opp */
				/* still using higher volt */
				/* overwrite low bank opp voltage */
				/*                  /             */
				/*                _/              */
				/*               /                */
				ndet->volt_tbl_pmic[i] =
					ndet->volt_tbl_pmic[i-1];
			else {
				/* Receive low bank isr but high opp */
				/* still using lower volt */
				/* overwrite high bank opp voltage */
				/*                 _/             */
				/*                /               */
				/*               /                */
				ndet->volt_tbl_pmic[i-1] =
					ndet->volt_tbl_pmic[i];

				if ((i > 1) && (ndet->volt_tbl_pmic[i] >
						ndet->volt_tbl_pmic[i-2]))
					ndet->volt_tbl_pmic[i-2] =
						ndet->volt_tbl_pmic[i];
			}
		}
#endif

	}

	eem_save_final_volt_aee(ndet);

#if UPDATE_TO_UPOWER
#if ENABLE_LOO
	if ((ndet->set_volt_to_upower == 0) &&
#if ENABLE_GPU
		(ndet->ctrl_id < EEM_CTRL_GPU)) {
#else
		(ndet->ctrl_id <= EEM_CTRL_CCI)) {
#endif
		if (((ndet->ctrl_id == EEM_CTRL_B) &&
			(cur_init02_flag !=
			bcpu_final_init02_flag)))
			init2chk = 0;
		else
			init2chk = 1;
		eem_error("cur_flag:0x%x, b_flag:0x%x\n",
			cur_init02_flag, bcpu_final_init02_flag);
		/* only when set_volt_to_upower == 0,
		 * the volt will be apply to upower
		 */
		if (init2chk) {
			eem_update_init2_volt_to_upower
				(ndet, ndet->volt_tbl_pmic);
			ndet->set_volt_to_upower = 1;
		}
	}
#else
	/* only when set_volt_to_upower == 0,
	 * the volt will be apply to upower
	 */
	if (ndet->set_volt_to_upower == 0) {
		eem_update_init2_volt_to_upower(ndet, ndet->volt_tbl_pmic);
		ndet->set_volt_to_upower = 1;
	}
#endif
#endif
	if (0 == (ndet->disabled % 2))
		ndet->ops->set_volt(ndet);
#if 0
skip_update:
#endif
#if ENABLE_LOO
	if (det->loo_role != NO_LOO_BANK)
		mutex_unlock(ndet->loo_mutex);
#endif
}
/*
 * Thread for voltage setting
 */
static int eem_volt_thread_handler(void *data)
{
	struct eem_ctrl *ctrl = (struct eem_ctrl *)data;
	struct eem_det *det;
#ifdef CONFIG_EEM_AEE_RR_REC
	int temp = -1;
#endif
#if ENABLE_LOO
	/* struct eem_ctrl *new_ctrl; */
	/* struct eem_det *new_det; */
	/* unsigned int init2chk = 0; */
#endif

	FUNC_ENTER(FUNC_LV_HELP);
	if (ctrl == NULL)
		return 0;

	det = id_to_eem_det(ctrl->det_id);
	if (det == NULL)
		return 0;
	do {
		eem_debug("In thread handler\n");
		wait_event_interruptible(ctrl->wq, ctrl->volt_update);
		if ((ctrl->volt_update & EEM_VOLT_UPDATE) &&
				det->ops->set_volt) {
			get_volt_table_in_thread(det);
#ifdef CONFIG_EEM_AEE_RR_REC
			/* update set volt status for this bank */
			switch (det->ctrl_id) {
			case EEM_CTRL_L:
				aee_rr_rec_ptp_status
					(aee_rr_curr_ptp_status() |
					(1 << EEM_CPU_2_LITTLE_IS_SET_VOLT));
				temp = EEM_CPU_2_LITTLE_IS_SET_VOLT;
				break;
			case EEM_CTRL_B:
				aee_rr_rec_ptp_status
					(aee_rr_curr_ptp_status() |
					(1 << EEM_CPU_LITTLE_IS_SET_VOLT));
				temp = EEM_CPU_LITTLE_IS_SET_VOLT;
				break;
			case EEM_CTRL_CCI:
				aee_rr_rec_ptp_status
					(aee_rr_curr_ptp_status() |
					(1 << EEM_CPU_CCI_IS_SET_VOLT));
				temp = EEM_CPU_CCI_IS_SET_VOLT;
				break;
#if ENABLE_GPU
			case EEM_CTRL_GPU:
				aee_rr_rec_ptp_status
					(aee_rr_curr_ptp_status() |
					(1 << EEM_GPU_IS_SET_VOLT));
				temp = EEM_GPU_IS_SET_VOLT;
				break;
			case EEM_CTRL_GPU_HI:
#if ENABLE_LOO_G
				aee_rr_rec_ptp_status
					(aee_rr_curr_ptp_status() |
					(1 << EEM_GPU_HI_IS_SET_VOLT));
				temp = EEM_GPU_HI_IS_SET_VOLT;
#endif
				break;
#endif
#if ENABLE_LOO_B
			case EEM_CTRL_B_HI:
				aee_rr_rec_ptp_status
					(aee_rr_curr_ptp_status() |
					(1 << EEM_CPU_BIG_HI_IS_SET_VOLT));
				temp = EEM_CPU_BIG_HI_IS_SET_VOLT;
				break;
#endif
			default:
				eem_error
				("eem_volt_handler :incorrect det id %d\n",
				 det->ctrl_id);
				break;
			}
#endif
			/* clear out set volt status for this bank */
#ifdef CONFIG_EEM_AEE_RR_REC
			if (temp >= EEM_CPU_2_LITTLE_IS_SET_VOLT)
				aee_rr_rec_ptp_status
				(aee_rr_curr_ptp_status() & ~(1 << temp));
#endif
		}
		if ((ctrl->volt_update & EEM_VOLT_RESTORE) &&
				det->ops->restore_default_volt){
			det->ops->restore_default_volt(det);
			ctrl->volt_update = EEM_VOLT_NONE;
		}
		if (det->vop_check)
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
#ifdef CORN_LOAD
	ktime_t ktime, start;
	unsigned int curtime = 0;
	unsigned int i;
	unsigned int err_flag = 0;

	start = ktime_get();
	while (1) {
		eem_debug("wait vpu time %d\n", curtime);
		if (get_vpu_init_done() && get_ready_for_ptpod_check())
			break;
		ktime = ktime_sub(ktime_get(), start);
		curtime = ktime_to_us(ktime);
		if (curtime > WAIT_TIME) {
			eem_error("%s wait vpu init timeout = %d\n", __func__,
					curtime);
			err_flag = 1;
			break;
		}
	}

	if (!err_flag) {
		eem_debug("@@~finish vpu hw init %d %d\n", get_vpu_init_done(),
			get_ready_for_ptpod_check());
		vpu_enable_mtcmos();
		for (i = 0; i < CORN_SIZE; i++)
			eem_corner(i);
		vpu_enable_by_ptpod();
		mdla_enable_by_ptpod();
		vpu_disable_mtcmos();
		vvpu_update_ptp_count(vpu_detcount, CORN_SIZE);
		vmdla_update_ptp_count(mdla_detcount, CORN_SIZE);
		ktime = ktime_sub(ktime_get(), ktime);
		eem_error("@@corner cost time = %d\n", ktime_to_us(ktime));
	}
#endif

	do {
		wait_event_interruptible(wqStress, eem_init1stress_en);
		eem_error("eem init1stress start\n");
		testCnt = 0;
		/* Disable GPU test due to MTCMOS can't force enable */
#if ENABLE_GPU
		det = id_to_eem_det(EEM_DET_GPU);
		det->features = 0;
		final_init01_flag &= ~(BIT(EEM_DET_GPU));
#endif
		/* CPU/GPU pre-process (need to fix )*/
		mt_cpufreq_ctrl_cci_volt(VBOOT_PMIC_VAL);
		mcdi_pause(MCDI_PAUSE_BY_EEM, true);
		mt_ppm_ptpod_policy_activate();

		eem_debug("vpu_disable_by_ptpod 2\n");
#if ENABLE_VPU
		/* vpu_disable_by_ptpod(); */
#endif
#if !EARLY_PORTING
		eem_buck_set_mode(1);
#endif
		while (eem_init1stress_en) {
			/* Start to clear previour ptp init status */
			mt_ptp_lock(&flag);

			for_each_det(det) {
				det->ops->switch_bank(det, NR_EEM_PHASE);
				eem_write(EEMEN, 0x0 | SEC_MOD_SEL);
				/* Clear interrupt EEMINTSTS = 0x00ff0000 */
				eem_write(EEMINTSTS, 0x00ff0000);
				det->eem_eemEn[EEM_PHASE_INIT01] = 0;

				if (HAS_FEATURE(det, FEA_INIT01))
					det->features = FEA_INIT01;

			}
			mt_ptp_unlock(&flag);

			if (testCnt++ % 200 == 0)
				eem_error("eem_init1stress,cnt:%d\n", testCnt);

			for_each_det_ctrl(det, ctrl) {
				if (HAS_FEATURE(det, FEA_INIT01)) {
#ifdef CORN_LOAD
					if (det->ctrl_id == EEM_CTRL_MDLA ||
						det->ctrl_id == EEM_CTRL_VPU)
						continue;
#endif
					mt_ptp_lock(&flag); /* <-XXX */
					det->ops->init01(det);
					mt_ptp_unlock(&flag); /* <-XXX */
				}
			}

			/* This patch is waiting for whole bank finish
			 * the init01 then go
			 * next. Due to LL/L use same bulk PMIC,
			 * LL voltage table change
			 * will impact L to process init01 stage,
			 * because L require a
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
					eem_error("init01 wait %d,0x%x[0x%x]\n",
					timeout, out, final_init01_flag);
			}
			msleep(100);
		}
#if !EARLY_PORTING
		/* CPU/GPU post-process */
		eem_buck_set_mode(0);
#endif

#if ENABLE_VPU
		eem_error("vpu_enable_by_ptpod 2\n");
		/* vpu_enable_by_ptpod(); */
#endif

		/* need to fix */
		mt_cpufreq_ctrl_cci_volt(VBOOT_PMIC_CLR);

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

	eem_debug("In %s\n", __func__);
	if (1) {
		init_waitqueue_head(&ctrl->wq);
		ctrl->thread = kthread_run(eem_volt_thread_handler,
				ctrl, ctrl->name);

		if (IS_ERR(ctrl->thread))
			eem_error("Create %s thread failed: %ld\n",
					ctrl->name, PTR_ERR(ctrl->thread));
	}

	eem_debug("End %s\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);
}

static void eem_init_det(struct eem_det *det, struct eem_devinfo *devinfo)
{
	enum eem_det_id det_id = det_to_id(det);

	FUNC_ENTER(FUNC_LV_HELP);
	inherit_base_det(det);
	eem_debug("IN init_det %s\n", det->name);
	switch (det_id) {
	case EEM_DET_L:
		det->MDES	= devinfo->CPU_L_MDES;
		det->BDES	= devinfo->CPU_L_BDES;
		det->DCMDET	= devinfo->CPU_L_DCMDET;
		det->DCBDET	= devinfo->CPU_L_DCBDET;
		det->EEMINITEN	= devinfo->CPU_L_INITEN;
		det->EEMMONEN	= devinfo->CPU_L_MONEN;
		det->MTDES	= devinfo->CPU_L_MTDES;
		det->SPEC	= devinfo->CPU_L_SPEC;
		det->VMAX += det->DVTFIXED;
		if (devinfo->CPU_L_DVFS_LOW == 4)
			det->VMIN = VMIN_VAL_D4;
		else if (devinfo->CPU_L_DVFS_LOW == 5)
			det->VMIN = VMIN_VAL_D5;
		break;
	case EEM_DET_B:
#if ENABLE_LOO_B
		bcpu_final_init02_flag |= BIT(det_id);
		if (big_2line) {
			det->MDES	= devinfo->CPU_B_LO_MDES;
			det->BDES	= devinfo->CPU_B_LO_BDES;
			det->DCMDET	= devinfo->CPU_B_LO_DCMDET;
			det->DCBDET	= devinfo->CPU_B_LO_DCBDET;
			det->EEMINITEN	= devinfo->CPU_B_LO_INITEN;
			det->EEMMONEN	= devinfo->CPU_B_LO_MONEN;
			det->MTDES	= devinfo->CPU_B_LO_MTDES;
			det->SPEC	= devinfo->CPU_B_LO_SPEC;

			det->DVTFIXED = DVTFIXED_VAL_BL;
			det->max_freq_khz = B_M_FREQ_BASE;
			det->loo_role	= LOW_BANK;
			if (devinfo->CPU_B_LO_DVFS_LOW == 4)
				det->VMIN = VMIN_VAL_B_D4;
			else if (devinfo->CPU_B_LO_DVFS_LOW == 5)
				det->VMIN = VMIN_VAL_B_D5;
		} else {
#endif
			det->MDES	= devinfo->CPU_B_MDES;
			det->BDES	= devinfo->CPU_B_BDES;
			det->DCMDET	= devinfo->CPU_B_DCMDET;
			det->DCBDET	= devinfo->CPU_B_DCBDET;
			det->EEMINITEN	= devinfo->CPU_B_INITEN;
			det->EEMMONEN	= devinfo->CPU_B_MONEN;
			det->MTDES	= devinfo->CPU_B_MTDES;
			det->SPEC	= devinfo->CPU_B_SPEC;
			det->max_freq_khz = B_FREQ_BASE;
			if (devinfo->CPU_B_DVFS_LOW == 4)
				det->VMIN = VMIN_VAL_B_D4;
			else if (devinfo->CPU_B_DVFS_LOW == 5)
				det->VMIN = VMIN_VAL_B_D5;
#if ENABLE_LOO_B
			det->loo_role = NO_LOO_BANK;
		}
#endif
		if (segCode == 0x10)
			det->VMAX = VMAX_VAL_B_T;

		det->VMAX += det->DVTFIXED;
		break;
	case EEM_DET_CCI:
		det->MDES	= devinfo->CCI_MDES;
		det->BDES	= devinfo->CCI_BDES;
		det->DCMDET	= devinfo->CCI_DCMDET;
		det->DCBDET	= devinfo->CCI_DCBDET;
		det->EEMINITEN	= devinfo->CCI_INITEN;
		det->EEMMONEN	= devinfo->CCI_MONEN;
		det->MTDES	= devinfo->CCI_MTDES;
		det->SPEC       = devinfo->CCI_SPEC;
		det->VMAX += det->DVTFIXED;
		if (devinfo->CCI_DVFS_LOW == 4)
			det->VMIN = VMIN_VAL_D4;
		else if (devinfo->CCI_DVFS_LOW == 5)
			det->VMIN = VMIN_VAL_D5;
		break;
#if ENABLE_GPU
	case EEM_DET_GPU:
#if ENABLE_LOO_G
		if (gpu_2line) {
			det->MDES	= devinfo->GPU_LO_MDES;
			det->BDES	= devinfo->GPU_LO_BDES;
			det->DCMDET	= devinfo->GPU_LO_DCMDET;
			det->DCBDET	= devinfo->GPU_LO_DCBDET;
			det->EEMINITEN	= devinfo->GPU_LO_INITEN;
			det->EEMMONEN	= devinfo->GPU_LO_MONEN;
			det->MTDES	= devinfo->GPU_LO_MTDES;
			det->SPEC       = devinfo->GPU_LO_SPEC;
			det->VMAX		= VMAX_VAL_GL;
			det->max_freq_khz = GPU_M_FREQ_BASE;
			det->loo_role = LOW_BANK;
		} else {
#endif
			det->MDES	= devinfo->GPU_MDES;
			det->BDES	= devinfo->GPU_BDES;
			det->DCMDET	= devinfo->GPU_DCMDET;
			det->DCBDET	= devinfo->GPU_DCBDET;
			det->EEMINITEN	= devinfo->GPU_INITEN;
			det->EEMMONEN	= devinfo->GPU_MONEN;
			det->MTDES	= devinfo->GPU_MTDES;
			det->SPEC       = devinfo->GPU_SPEC;
			det->VMAX		= VMAX_VAL_GPU;
			det->max_freq_khz = GPU_FREQ_BASE;
#if ENABLE_LOO_G
			det->loo_role = NO_LOO_BANK;
		}
#endif
		det->VMAX += det->DVTFIXED;
		break;
#endif
#if ENABLE_VPU
	case EEM_DET_VPU:
		det->MDES	= devinfo->GPU_MDES;
		det->BDES	= devinfo->GPU_BDES;
		det->DCMDET	= devinfo->GPU_DCMDET;
		det->DCBDET	= devinfo->GPU_DCBDET;
		det->EEMINITEN	= devinfo->GPU_INITEN;
		det->EEMMONEN	= devinfo->GPU_MONEN;
		det->MTDES	= devinfo->GPU_MTDES;
		break;
#endif
#if ENABLE_MDLA
	case EEM_DET_MDLA:
		det->MDES	= devinfo->GPU_MDES;
		det->BDES	= devinfo->GPU_BDES;
		det->DCMDET	= devinfo->GPU_DCMDET;
		det->DCBDET	= devinfo->GPU_DCBDET;
		det->EEMINITEN	= devinfo->GPU_INITEN;
		det->EEMMONEN	= devinfo->GPU_MONEN;
		det->MTDES	= devinfo->GPU_MTDES;
		break;
#endif
#if ENABLE_GPU
	case EEM_DET_GPU_HI:
#if ENABLE_LOO_G
		det->MDES	= devinfo->GPU_HI_MDES;
		det->BDES	= devinfo->GPU_HI_BDES;
		det->DCMDET	= devinfo->GPU_HI_DCMDET;
		det->DCBDET	= devinfo->GPU_HI_DCBDET;
		det->EEMINITEN	= devinfo->GPU_HI_INITEN;
		det->EEMMONEN	= devinfo->GPU_HI_MONEN;
		det->MTDES	= devinfo->GPU_HI_MTDES;
		det->SPEC       = devinfo->GPU_HI_SPEC;
		det->VMAX += det->DVTFIXED;
		if (!gpu_2line) {
			det->features = 0;
			det->loo_role = NO_LOO_BANK;
		}
#endif
		break;
#endif
	case EEM_DET_B_HI:
#if ENABLE_LOO_B
		if ((segCode == 0x10) &&
			(devinfo->CPU_L_HI_SPEC == 2)) {
			det->MDES	= devinfo->CPU_L_HI_MDES;
			det->BDES	= devinfo->CPU_L_HI_BDES;
			det->DCMDET = devinfo->CPU_B_HI_DCMDET;
			det->DCBDET = devinfo->CPU_B_HI_DCBDET;
			det->EEMINITEN	= devinfo->CPU_L_HI_INITEN;
			det->EEMMONEN	= devinfo->CPU_L_HI_MONEN;
			det->MTDES	= devinfo->CPU_L_HI_MTDES;
			det->SPEC	= devinfo->CPU_L_HI_SPEC;
			det->max_freq_khz = B_FREQ26_BASE;
		} else {
			det->MDES	= devinfo->CPU_B_HI_MDES;
			det->BDES	= devinfo->CPU_B_HI_BDES;
			det->DCMDET = devinfo->CPU_B_HI_DCMDET;
			det->DCBDET = devinfo->CPU_B_HI_DCBDET;
			det->EEMINITEN	= devinfo->CPU_B_HI_INITEN;
			det->EEMMONEN	= devinfo->CPU_B_HI_MONEN;
			det->MTDES	= devinfo->CPU_B_HI_MTDES;
			det->SPEC	= devinfo->CPU_B_HI_SPEC;
		}

		if (segCode == 0x10)
			det->VMAX = VMAX_VAL_B_T;

		det->VMAX += det->DVTFIXED;
		if (!big_2line) {
			det->features = 0;
			det->loo_role = NO_LOO_BANK;
		}
#endif
		break;
	default:
		eem_debug("[%s]: Unknown det_id %d\n", __func__, det_id);
		break;
	}
#if 0
#if DVT
	det->VBOOT = 0x30;
	det->VMAX = 0xFF;
	det->VMIN = 0x0;
	det->VCO = 0x10;
	det->DVTFIXED = 0x07;
#if (SEC_MOD_SEL == 0xF0)
	det->MDES	= 0x3C;
	det->BDES	= 0x1B;
	det->DCMDET = 0x10;
	det->DCBDET = 0xBD;
	det->MTDES	= 0x55;
#else
#if 0
	det->MDES	= SEC_MDES;
	det->BDES	= SEC_BDES;
	det->DCMDET = SEC_DCMDET;
	det->DCBDET = SEC_DCBDET;
	det->MTDES	= SEC_MTDES;
#endif
#endif
#endif
#endif
	/* get DVFS frequency table */
	if (det->ops->get_freq_table)
		det->ops->get_freq_table(det);

#if ENABLE_LOO_B
	if ((big_2line) &&
		(det_id == EEM_DET_B_HI)) {
		if (det->turn_pt != 0)
			bcpu_final_init02_flag |= BIT(det_id);
		else
			det->features = 0;
	}
#endif
	eem_debug("END init_det %s, turn_pt:%d\n",
		det->name, det->turn_pt);
	FUNC_EXIT(FUNC_LV_HELP);
}

#if UPDATE_TO_UPOWER
static enum upower_bank transfer_ptp_to_upower_bank(unsigned int det_id)
{
	enum upower_bank bank;

	switch (det_id) {
	case EEM_DET_L:
		bank = UPOWER_BANK_LL;
		break;
	case EEM_DET_B:
		bank = UPOWER_BANK_L;
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

static void eem_update_init2_volt_to_upower
	(struct eem_det *det, unsigned int *pmic_volt)
{
	unsigned int volt_tbl[NR_FREQ_CPU];
	enum upower_bank bank;
	int i;

	for (i = 0; i < det->num_freq_tbl; i++)
		volt_tbl[i] = det->ops->pmic_2_volt(det, pmic_volt[i]);

	bank = transfer_ptp_to_upower_bank(det_to_id(det));
#if 1
	if (bank < NR_UPOWER_BANK) {
		upower_update_volt_by_eem(bank, volt_tbl, det->num_freq_tbl);
		/* eem_debug
		 * ("update init2 volt to upower
		 * (eem bank %ld upower bank %d)\n",
		 * det_to_id(det), bank);
		 */
	}
#endif

}
#endif

static void eem_set_eem_volt(struct eem_det *det)
{
#if SET_PMIC_VOLT
	struct eem_ctrl *ctrl = id_to_eem_ctrl(det->ctrl_id);

	FUNC_ENTER(FUNC_LV_HELP);
	if (ctrl == NULL)
		return;

	ctrl->volt_update |= EEM_VOLT_UPDATE;
	dsb(sy);
	eem_debug("@@!In %s\n", __func__);
	wake_up_interruptible(&ctrl->wq);
#endif

	FUNC_EXIT(FUNC_LV_HELP);
}

static void eem_restore_eem_volt(struct eem_det *det)
{
#if SET_PMIC_VOLT
	struct eem_ctrl *ctrl = id_to_eem_ctrl(det->ctrl_id);

	if (ctrl == NULL)
		return;

	ctrl->volt_update |= EEM_VOLT_RESTORE;
	dsb(sy);
	wake_up_interruptible(&ctrl->wq);
#endif
}

#if 0
static void mt_eem_reg_dump_locked(void)
{
	unsigned long addr;

	for (addr = (unsigned long)EEM_DESCHAR;
			addr <= (unsigned long)EEM_SMSTATE1; addr += 4)
		eem_isr_info("0x %lu = 0x %lu\n", addr, *(unsigned long *)addr);

	addr = (unsigned long)EEMCORESEL;
	eem_isr_info("0x %lu = 0x %lu\n", addr, *(unsigned long *)addr);
}
#endif

static inline void handle_init01_isr(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_LOCAL);

	eem_debug("mode = init1 %s-isr\n", ((char *)(det->name) + 8));

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
			(unsigned long)EEM_BASEADDR + reg_dump_addr_off[i],
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

#ifdef CORN_LOAD
	if (corn_init)
		det->detcount = (eem_read(EEM_DCVALUES) >> 16) & 0xffff;
#endif
	det->AGEVOFFSETIN = eem_read(EEM_AGEVALUES) & 0xffff;

	/*
	 * Set EEMEN.EEMINITEN/EEMEN.EEMINIT2EN = 0x0 &
	 * Clear EEM INIT interrupt EEMINTSTS = 0x00000001
	 */
	eem_write(EEMEN, 0x0 | SEC_MOD_SEL);
	eem_write(EEMINTSTS, 0x1);
	/* det->ops->init02(det); */

	eem_debug("@@ END mode = init1 %s-isr\n", ((char *)(det->name) + 8));
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
		/*
		 *eem_debug("y1(%d), y0(%d), x1(%d), x0(%d), ym(%d), ratio(%d),
		 *rtn(%d)\n",
		 *	y1, y0, x1, x0, ym, ratio, result);
		 */
	}
	return result;
}

static void eem_fill_freq_table(struct eem_det *det)
{
	int tmpfreq30 = 0, tmpfreq74 = 0;
	int i, j, shift_bits;
	int *setfreq;
#if ENABLE_LOO

	if (det->loo_role == HIGH_BANK) {
		/* From opp0 ~ opp[turn_pt] */
		if (det->turn_pt < 8) {
			for (i = 0; i < det->turn_pt; i++) {
				shift_bits = (i % 4) * 8;
				setfreq = (i < 4) ? &tmpfreq30 : &tmpfreq74;
				*setfreq |= (det->freq_tbl[i] << shift_bits);
			}
		} else {
			tmpfreq30 = det->freq_tbl[0] & 0xFF;

			/* prepare last 7 freq, fill to FREQPCT31, FREQPCT74 */
			for (i = (det->turn_pt - 7), j = 1;
				i < det->turn_pt; i++, j++) {
				shift_bits = (j % 4) * 8;
				setfreq = (j < 4) ? &tmpfreq30 : &tmpfreq74;
				*setfreq |= (det->freq_tbl[i] << shift_bits);
			}
		}
	} else if (det->loo_role == LOW_BANK) {
		/* From opp[turn_pt] to final opp */
		/* prepare 2nd line's first freq */
		tmpfreq30 = det->freq_tbl[det->turn_pt] & 0xFF;
		if ((det->num_freq_tbl - det->turn_pt) <= 8) {
			/* For opp9~15 */
			for (i = (det->turn_pt + 1), j = 1;
				i < det->num_freq_tbl; i++, j++) {
				shift_bits = (j % 4) * 8;
				setfreq = (j < 4) ? &tmpfreq30 : &tmpfreq74;
				*setfreq |= (det->freq_tbl[i] << shift_bits);
			}
		} else {
			/* prepare last 7 freq, fill to FREQPCT31, FREQPCT74 */
			for (i = (det->num_freq_tbl - 7), j = 1;
				i < det->num_freq_tbl; i++, j++) {
				shift_bits = (j % 4) * 8;
				setfreq = (j < 4) ? &tmpfreq30 : &tmpfreq74;
				*setfreq |= (det->freq_tbl[i] << shift_bits);
			}
		}
	} else {
#endif
		if (det->num_freq_tbl <= 16) {
			tmpfreq30 =
			((det->freq_tbl[3 * ((det->num_freq_tbl + 7) / 8)]
			<< 24) & 0xff000000)	|
			((det->freq_tbl[2 * ((det->num_freq_tbl + 7) / 8)]
			<< 16) & 0xff0000) |
			((det->freq_tbl[1 * ((det->num_freq_tbl + 7) / 8)]
			<< 8) & 0xff00) |
			(det->freq_tbl[0]
			& 0xff);

			tmpfreq74 =
			((det->freq_tbl[7 * ((det->num_freq_tbl + 7) / 8)]
			<< 24) & 0xff000000)	|
			((det->freq_tbl[6 * ((det->num_freq_tbl + 7) / 8)]
			<< 16) & 0xff0000) |
			((det->freq_tbl[5 * ((det->num_freq_tbl + 7) / 8)]
			<< 8) & 0xff00) |
			((det->freq_tbl[4 * ((det->num_freq_tbl + 7) / 8)])
			& 0xff);
		} else {
			/* For 32 opp */
			eem_error("Fill 32 opp/n");
			tmpfreq30 = det->freq_tbl[0] & 0xFF;
			/* prepare last 7 freq, fill to FREQPCT31, FREQPCT74 */
			for (i = (det->num_freq_tbl - 7), j = 1;
				i < det->num_freq_tbl; i++, j++) {
				shift_bits = (j % 4) * 8;
				setfreq = (j < 4) ? &tmpfreq30 : &tmpfreq74;
				*setfreq |= (det->freq_tbl[i] << shift_bits);
			}
		}
#if ENABLE_LOO
	}
#endif

	eem_write(EEM_FREQPCT30, tmpfreq30);
	eem_write(EEM_FREQPCT74, tmpfreq74);

}

static void read_volt_from_VOP(struct eem_det *det)
{
	int temp30, temp74, i, j;
	unsigned int step = NR_FREQ / 8;
	unsigned int read_init2 = 0;
	int ref_idx = ((det->num_freq_tbl + (step - 1)) / step) - 1;
#if ENABLE_LOO
	int readvop, shift_bits;
	struct eem_det *couple_det;

	/* Check both high/low bank's voltage are ready */
	if (det->loo_role != 0) {
		couple_det = id_to_eem_det(det->loo_couple);
		if ((couple_det->init2_done == 0) ||
			(couple_det->mon_vop30 == 0) ||
			(couple_det->mon_vop74 == 0))
			read_init2 = 1;
	}
#endif
#if ENABLE_HT_FT
		read_init2 = 1;
#endif

	if ((det->init2_done == 0) || (det->mon_vop30 == 0) ||
		(det->mon_vop74 == 0))
		read_init2 = 1;

	if (read_init2) {
		temp30 = det->init2_vop30;
		temp74 = det->init2_vop74;
	} else {
		temp30 = det->mon_vop30;
		temp74 = det->mon_vop74;
	}
	det->vop_check = 1;

#if ENABLE_LOO
	if (det->loo_role == HIGH_BANK) {
		if (det->turn_pt < 8) {
			for (i = 0; i < det->turn_pt; i++) {
				shift_bits = (i % 4) * 8;
				readvop = (i < 4) ? temp30 : temp74;
				det->volt_tbl[i] =
					(readvop >> shift_bits) & 0xff;
			}
		} else {
			det->volt_tbl[0] = (temp30 & 0xff);

			/* prepare last 7 opp */
			for (i = (det->turn_pt - 7), j = 1;
			i < det->turn_pt; i++, j++) {
				shift_bits = (j % 4) * 8;
				readvop = (j < 4) ? temp30 : temp74;
				det->volt_tbl[i] =
					(readvop >> shift_bits) & 0xff;
			}

			/* prepare middle opp */
			for (i = 1; i < det->turn_pt - 7; i++) {
				det->volt_tbl[i] =
				interpolate(
				det->freq_tbl[0],
				det->freq_tbl[det->turn_pt - 7],
				det->volt_tbl[0],
				det->volt_tbl[det->turn_pt - 7],
				det->freq_tbl[i]
				);
			}
		}
	} else if (det->loo_role == LOW_BANK) {
		/* prepare first opp */
		det->volt_tbl[det->turn_pt] = (temp30 & 0xff);

		if ((det->num_freq_tbl - det->turn_pt) <= 8) {
			/* For opp8~15 */
			for (i = det->turn_pt + 1, j = 1;
				i < det->num_freq_tbl; i++, j++) {
				shift_bits = (j % 4) * 8;
				readvop = (j < 4) ? temp30 : temp74;
				det->volt_tbl[i] =
					(readvop >> shift_bits) & 0xff;
			}
		} else {
			/* prepare last 7 opp, for opp9~15 */
			for (i = (det->num_freq_tbl - 7), j = 1;
				i < det->num_freq_tbl; i++, j++) {
				shift_bits = (j % 4) * 8;
				readvop = (j < 4) ? temp30 : temp74;
				det->volt_tbl[i] =
					(readvop >> shift_bits) & 0xff;
			}

			/* prepare middle opp */
			for (i = (det->turn_pt + 1);
				i < det->num_freq_tbl - 7; i++) {
				det->volt_tbl[i] =
				interpolate(
				det->freq_tbl[det->turn_pt],
				det->freq_tbl[det->num_freq_tbl - 7],
				det->volt_tbl[det->turn_pt],
				det->volt_tbl[det->num_freq_tbl - 7],
				det->freq_tbl[i]
				);
			}
		}
	} else {
#endif

		/* for 16 opp */
		det->volt_tbl[0] = (temp30 & 0xff);
		det->volt_tbl[1 * ((det->num_freq_tbl + 7) / 8)] =
			(temp30 >> 8)  & 0xff;
		det->volt_tbl[2 * ((det->num_freq_tbl + 7) / 8)] =
			(temp30 >> 16) & 0xff;
		det->volt_tbl[3 * ((det->num_freq_tbl + 7) / 8)] =
			(temp30 >> 24) & 0xff;

		det->volt_tbl[4 * ((det->num_freq_tbl + 7) / 8)] =
			(temp74 & 0xff);
		det->volt_tbl[5 * ((det->num_freq_tbl + 7) / 8)] =
			(temp74 >> 8)  & 0xff;
		det->volt_tbl[6 * ((det->num_freq_tbl + 7) / 8)] =
			(temp74 >> 16) & 0xff;
		det->volt_tbl[7 * ((det->num_freq_tbl + 7) / 8)] =
			(temp74 >> 24) & 0xff;

		if ((det->num_freq_tbl > 8) && (ref_idx > 0)) {
			for (i = 0; i <= ref_idx; i++) { /* i < 8 */
				for (j = 1; j < step; j++) {
					if (i < ref_idx) {
						det->volt_tbl[(i * step) + j] =
						interpolate(
						det->freq_tbl[(i * step)],
						det->freq_tbl[((1 + i) * step)],
						det->volt_tbl[(i * step)],
						det->volt_tbl[((1 + i) * step)],
						det->freq_tbl[(i * step) + j]
						);
					} else {
						det->volt_tbl[(i * step) + j] =
						clamp(
						interpolate(
						det->freq_tbl[((i - 1) * step)],
						det->freq_tbl[((i) * step)],
						det->volt_tbl[((i - 1) * step)],
						det->volt_tbl[((i) * step)],
						det->freq_tbl[(i * step) + j]
						),
						det->VMIN,
						det->VMAX
						);
					}
				}
			}
		} /* if (det->num_freq_tbl > 8)*/

#if ENABLE_LOO
	}
#endif
}

static inline void handle_init02_isr(struct eem_det *det)
{
	unsigned int i;

	FUNC_ENTER(FUNC_LV_LOCAL);

	eem_debug("mode = init2 %s-isr\n", ((char *)(det->name) + 8));

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

#if ENABLE_MINIHQA
	if ((det->ctrl_id != EEM_CTRL_GPU) ||
		(det->ctrl_id != EEM_CTRL_GPU_HI)) {
		det->init2_vop30 = eem_read(EEM_VDESIGN30);
		det->init2_vop74 = eem_read(EEM_VDESIGN74);
	} else {
		det->init2_vop30 = eem_read(EEM_VOP30);
		det->init2_vop74 = eem_read(EEM_VOP74);
	}
#else
	det->init2_vop30 = eem_read(EEM_VOP30);
	det->init2_vop74 = eem_read(EEM_VOP74);
#endif

	det->vop_check = 0;
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
	int i;
	int *val = (int *)&eem_devinfo;

	FUNC_ENTER(FUNC_LV_LOCAL);
	eem_error("====================================================\n");
	eem_error("BD/MD/DCB/DCM/MTD=0x%X,%X,%X,%X,%X\n",
		 det->BDES, det->MDES, det->DCBDET, det->DCMDET,
		 det->MTDES);
	eem_error("DETWINDOW/VMIN/VMAX/DTHI/DTLO=0x%X,%X,%X,%X,%X\n",
		 det->DETWINDOW, det->VMIN, det->VMAX, det->DTHI,
		 det->DTLO);
	eem_error("DETMAX/DVTFIXED/VCO/DCCONFIG/=0x%X,%X,%X,%X\n",
		 det->DETMAX, det->DVTFIXED, det->VCO,
		 det->DCCONFIG);
	eem_error("DCVOFFSETIN/dcvalues0/1/turn_pt=0x%X,%X,%X,%X\n",
		 det->DCVOFFSETIN, det->dcvalues[0], det->dcvalues[1],
		 det->turn_pt);


	/* Depend on EFUSE location */
	for (i = 0; i < sizeof(struct eem_devinfo) / sizeof(unsigned int);
		i++)
		eem_error("M_HW_RES%d= 0x%08X\n", i, val[i]);

	eem_error("EEM init err: EEMEN(%p) = 0x%X, EEMINTSTS(%p) = 0x%X\n",
			 EEMEN, eem_read(EEMEN),
			 EEMINTSTS, eem_read(EEMINTSTS));
	eem_error("EEM_SMSTATE0 (%p) = 0x%X\n",
			 EEM_SMSTATE0, eem_read(EEM_SMSTATE0));
	eem_error("EEM_SMSTATE1 (%p) = 0x%X\n",
			 EEM_SMSTATE1, eem_read(EEM_SMSTATE1));
	eem_error("EEM_DESCHAR (%p) = 0x%X,0x%X,0x%X\n",
		 EEM_DESCHAR, eem_read(EEM_DESCHAR),
		 eem_read(EEM_TEMPCHAR), eem_read(EEM_DETCHAR));
	eem_error("EEM_DCCONFIG/LIMIT/CORESEL (%p) = 0x%X,0x%X,0x%X\n",
		 EEM_DCCONFIG, eem_read(EEM_DCCONFIG),
		 eem_read(EEM_LIMITVALS), eem_read(EEMCORESEL));
	eem_error("EEM_DETWINDOW/INIT2/DC (%p) = 0x%X,0x%X,0x%X\n",
		 EEM_DETWINDOW, eem_read(EEM_DETWINDOW),
		 eem_read(EEM_INIT2VALS), eem_read(EEM_DCVALUES));
	eem_error("EEM_CHKSHIFT/VDESIGN/VOP (%p) = 0x%X,0x%X,0x%X,0x%X,0x%X\n",
		EEM_CHKSHIFT, eem_read(EEM_CHKSHIFT),
		eem_read(EEM_VDESIGN30), eem_read(EEM_VDESIGN74),
		eem_read(EEM_VOP30), eem_read(EEM_VOP74));
	eem_error("EEM_FREQPCT/74/CE0/CE4 (%p) = 0x%X,0x%X,0x%X,0x%X\n",
		EEM_FREQPCT30, eem_read(EEM_FREQPCT30),
		eem_read(EEM_FREQPCT74), eem_read(EEM_BASEADDR + 0xCE0),
		eem_read(EEM_BASEADDR + 0xCE4));


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
	unsigned int i;
#ifdef CONFIG_THERMAL
#ifdef CONFIG_EEM_AEE_RR_REC
	unsigned long long temp_long;
	unsigned long long temp_cur =
		(unsigned long long)aee_rr_curr_ptp_temp();
#endif
#endif
	FUNC_ENTER(FUNC_LV_LOCAL);

	eem_debug("mode = mon %s-isr\n", ((char *)(det->name) + 8));

#ifdef CONFIG_THERMAL
	eem_debug("LL_temp=%d, L_temp=%d, CCI_temp=%d, GPU_temp=%d\n",
		tscpu_get_temp_by_bank(THERMAL_BANK0),
		tscpu_get_temp_by_bank(THERMAL_BANK1),
		tscpu_get_temp_by_bank(THERMAL_BANK2),
		tscpu_get_temp_by_bank(THERMAL_BANK3)
		);
#endif

#ifdef CONFIG_EEM_AEE_RR_REC
	switch (det->ctrl_id) {
	case EEM_CTRL_L:
#ifdef CONFIG_THERMAL
#if defined(__LP64__) || defined(_LP64)
		temp_long = (unsigned long long)
			tscpu_get_temp_by_bank(THERMAL_BANK2)/1000;
#else
		temp_long = div_u64((unsigned long long)
			tscpu_get_temp_by_bank(THERMAL_BANK2), 1000);
#endif
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(temp_long <<
					(8 * EEM_CPU_2_LITTLE_IS_SET_VOLT) |
			(temp_cur & ~((0xFFULL) <<
					(8 * EEM_CPU_2_LITTLE_IS_SET_VOLT))));
		}
#endif
		break;

	case EEM_CTRL_B:
#ifdef CONFIG_THERMAL
#if defined(__LP64__) || defined(_LP64)
		temp_long =
		(unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK0)/1000;
#else
		temp_long = div_u64
		((unsigned long long)
		 tscpu_get_temp_by_bank(THERMAL_BANK0), 1000);
#endif
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(temp_long <<
					(8 * EEM_CPU_LITTLE_IS_SET_VOLT) |
			(temp_cur & ~((0xFFULL) <<
					(8 * EEM_CPU_LITTLE_IS_SET_VOLT))));
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
		div_u64
		((unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK2),
		1000);
#endif
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(temp_long <<
					(8 * EEM_CPU_CCI_IS_SET_VOLT)|
			(temp_cur & ~((0xFFULL) <<
					(8 * EEM_CPU_CCI_IS_SET_VOLT))));
		}
#endif
		break;
#if ENABLE_GPU
	case EEM_CTRL_GPU:
#ifdef CONFIG_THERMAL
#if defined(__LP64__) || defined(_LP64)
		temp_long =
		(unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK4)/1000;
#else
		temp_long = div_u64
		((unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK4),
		 1000);
#endif
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(temp_long <<
				(8 * EEM_GPU_IS_SET_VOLT) |
			(temp_cur & ~((0xFFULL)
			<< (8 * EEM_GPU_IS_SET_VOLT))));
		}
#endif
		break;
#if ENABLE_LOO_G
	case EEM_CTRL_GPU_HI:
#ifdef CONFIG_THERMAL
#if defined(__LP64__) || defined(_LP64)
		temp_long =
		(unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK4)/1000;
#else
		temp_long = div_u64
		((unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK4),
		 1000);
#endif
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(temp_long <<
				(8 * EEM_GPU_HI_IS_SET_VOLT) |
			(temp_cur & ~((0xFFULL)
			<< (8 * EEM_GPU_HI_IS_SET_VOLT))));
		}
#endif
		break;
#endif
#endif
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

	det->mon_vop30	= eem_read(EEM_VOP30);
	det->mon_vop74	= eem_read(EEM_VOP74);
	det->vop_check = 0;

	/* check if thermal sensor init completed? */
	det->t250 = eem_read(TEMP);

	/* 0x64 mappint to 100 + 25 = 125C,
	 *   0xB2 mapping to 178 - 128 = 50, -50 + 25 = -25C
	 */
	if (((det->t250 & 0xff) > 0x64) && ((det->t250  & 0xff) < 0xB2)) {
		eem_error("Bank %s Temperature > 125C or < -25C 0x%x\n",
				det->name, det->t250);
		goto out;
	}

	eem_set_eem_volt(det);

out:
	/* Clear EEM INIT interrupt EEMINTSTS = 0x00ff0000 */
	eem_write(EEMINTSTS, 0x00ff0000);
	eem_debug("finish mon isr\n");

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
	eem_error
	("EEM mon err: EEMCORESEL(%p) = 0x%08X,EEM_THERMINTST(%p) = 0x%08X,",
			EEMCORESEL, eem_read(EEMCORESEL),
			EEM_THERMINTST, eem_read(EEM_THERMINTST));
	eem_error(" EEM0DINTST(%p) = 0x%08X", EEMODINTST, eem_read(EEMODINTST));
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
				eem_read(EEM_BASEADDR + reg_dump_addr_off[i]);
			eem_error("0x%lx = 0x%08x\n",
			(unsigned long)EEM_BASEADDR + reg_dump_addr_off[i],
			det->reg_dump_data[i][EEM_PHASE_MON]
			);
		}
#endif

	eem_error("====================================================\n");
	eem_error("EEM mon err: EEMCORESEL(%p) = 0x%08X,",
			 EEMCORESEL, eem_read(EEMCORESEL));
	eem_error(" EEM_THERMINTST(%p) = 0x%08X, EEMODINTST(%p) = 0x%08X",
			 EEM_THERMINTST, eem_read(EEM_THERMINTST),
			 EEMODINTST, eem_read(EEMODINTST));

	eem_error(" EEMINTSTSRAW(%p) = 0x%08X, EEMINTEN(%p) = 0x%08X\n",
			 EEMINTSTSRAW, eem_read(EEMINTSTSRAW),
			 EEMINTEN, eem_read(EEMINTEN));
	eem_error("====================================================\n");

	aee_kernel_warning("mt_eem",
	"@%s():%d,get_volt(%s)=0x%08X,EEMEN(%p)=0x%08X,EEMINTSTS(%p)=0x%08X\n",
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

#if 1
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
	unsigned int mcucfg_aging;

	FUNC_ENTER(FUNC_LV_LOCAL);
	eem_debug("%s called by [%s]\n", __func__, str);

	mcucfg_aging = eem_read(MCUCFG_AGING);
	if ((mcucfg_aging >> 31) & 0x1)
		eem_aging = (mcucfg_aging >> 4) & 0xf;

	for_each_det_ctrl(det, ctrl) {
		if (HAS_FEATURE(det, FEA_INIT02)) {
			unsigned long flag;

			mt_ptp_lock(&flag);
			det->init2_done = 0;
			det->ops->init02(det);
			mt_ptp_unlock(&flag);
		}
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

#if !EARLY_PORTING
/* get regulator reference */
static int eem_buck_get(struct platform_device *pdev)
{
	int ret = 0;

	eem_regulator_vproc1 = regulator_get(&pdev->dev, "vproc1");
	if (!eem_regulator_vproc1) {
		eem_error("eem_regulator_vproc1 error\n");
		return -EINVAL;
	}

	eem_regulator_vproc2 = regulator_get(&pdev->dev, "vproc2");
	if (!eem_regulator_vproc2) {
		eem_error("eem_regulator_vproc2 error\n");
		return -EINVAL;
	}

	return ret;
}

static void eem_buck_set_mode(unsigned int mode)
{
	/* set pwm mode for each buck */
	eem_debug("pmic set mode (%d)\n", mode);
	if (mode) {
		regulator_set_mode(eem_regulator_vproc1, REGULATOR_MODE_FAST);
		regulator_set_mode(eem_regulator_vproc2, REGULATOR_MODE_FAST);
	} else {
		regulator_set_mode(eem_regulator_vproc1, REGULATOR_MODE_NORMAL);
		regulator_set_mode(eem_regulator_vproc2, REGULATOR_MODE_NORMAL);
	}
}
#endif
#ifdef CORN_LOAD
void eem_corner(int testcnt)
{
	struct eem_det *det;
	struct eem_ctrl *ctrl;
	unsigned int out = 0, timeout = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	corn_init = 1;
	for_each_det_ctrl(det, ctrl) {
		unsigned long flag;

		if (HAS_FEATURE(det, FEA_CORN)) {
#if 1
			if (det->ops->get_volt != NULL) {
				det->real_vboot = det->ops->volt_2_eem
				(det, det->ops->get_volt(det));

			}
#endif

#if 1
			while (det->real_vboot != det->VBOOT) {
				det->real_vboot = det->ops->volt_2_eem(det,
					det->ops->get_volt(det));
#if 1
				if (timeout++ % 300 == 0)
					eem_debug
("@%s():%d, get_volt(%s) = 0x%08X, VBOOT = 0x%08X vpu_return = %d\n",
__func__, __LINE__, det->name, det->real_vboot, det->VBOOT,
det->ops->get_volt(det));
#endif
			}
			/* BUG_ON(det->real_vboot != det->VBOOT); */
			WARN_ON(det->real_vboot != det->VBOOT);
#endif
			det->eem_eemEn[EEM_PHASE_INIT01] = 0;
			det->DCVOFFSETIN = 0;
			det->DCCONFIG = arrtype[testcnt];
			mt_ptp_lock(&flag); /* <-XXX */
			det->ops->init01(det);
			mt_ptp_unlock(&flag); /* <-XXX */
		}
	}

	while (1) {
		for_each_det(det) {
			eem_debug("@@!corner out is 0x%x, bankmask:0x%x\n",
					out, final_corner_flag);
			if (((out & BIT(det->ctrl_id)) == 0) &&
					(det->eem_eemEn[EEM_PHASE_INIT01] ==
					(1 | SEC_MOD_SEL)) &&
					(HAS_FEATURE(det, FEA_CORN))) {
				out |= BIT(det->ctrl_id);
				eem_debug("@@# temp out = 0x%x ctrl_id = %d\n",
						out, det->ctrl_id);
			}
		}


		if (out == final_corner_flag) {
			eem_debug("@@!ct finish time is %d, bankmask:0x%x\n",
					timeout, final_corner_flag);
			break;
		}
		udelay(100);
		timeout++;

		if (timeout % 300 == 0)
			eem_debug
			("@@#corner wait time is %d, bankmask:0x%x[/0x%x]\n",
			timeout, out, final_corner_flag);
	}
	for_each_det(det) {
		if (det->ctrl_id == EEM_CTRL_VPU) {
			vpu_detcount[testcnt] = det->detcount;
			eem_debug("@@! %s detcount[%d] = 0x%x\n",
				det->name, testcnt, vpu_detcount[testcnt]);
		}
		if (det->ctrl_id == EEM_CTRL_MDLA) {
			mdla_detcount[testcnt] = det->detcount;
			eem_debug("@@! %s detcount[%d] = 0x%x\n",
				det->name, testcnt, mdla_detcount[testcnt]);
		}
	}

	FUNC_EXIT(FUNC_LV_LOCAL);


}
#endif
void eem_init01(void)
{
	struct eem_det *det;
	struct eem_ctrl *ctrl;
	unsigned int out = 0, timeout = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	for_each_det_ctrl(det, ctrl) {
		unsigned long flag;

		if (HAS_FEATURE(det, FEA_INIT01)) {
#ifdef CORN_LOAD
			if (HAS_FEATURE(det, FEA_CORN))
				continue;
#endif
			if (det->ops->get_volt != NULL) {
				det->real_vboot = det->ops->volt_2_eem
				(det, det->ops->get_volt(det));
#ifdef CONFIG_EEM_AEE_RR_REC
			aee_rr_rec_ptp_vboot(
			((unsigned long long)(det->real_vboot) <<
			 (8 * det->ctrl_id)) |
			(aee_rr_curr_ptp_vboot() & ~
			((unsigned long long)(0xFF) << (8 * det->ctrl_id)))
			);
#endif
			}
			timeout = 0;

			while (det->real_vboot != det->VBOOT) {
				eem_debug
			("@%s():%d, get_volt(%s) = 0x%08X, VBOOT = 0x%08X\n",
			__func__, __LINE__, det->name,
			det->real_vboot, det->VBOOT);

				det->real_vboot = det->ops->volt_2_eem(det,
					det->ops->get_volt(det));
				if (timeout++ % 1000 == 0)
					eem_error
("@%s():%d, get_volt(%s) = 0x%08X, VBOOT = 0x%08X\n",
__func__, __LINE__, det->name, det->real_vboot, det->VBOOT);
			}
			/* BUG_ON(det->real_vboot != det->VBOOT); */
			WARN_ON(det->real_vboot != det->VBOOT);

			eem_debug
			("@@!%s():%d, get_volt(%s) = 0x%08X, VBOOT = 0x%08X\n",
			__func__, __LINE__, det->name, det->real_vboot,
			det->VBOOT);
			mt_ptp_lock(&flag); /* <-XXX */
			det->ops->init01(det);
			mt_ptp_unlock(&flag); /* <-XXX */
		}
	}
#if !EARLY_PORTING

	/* CPU/GPU post-process */
	eem_buck_set_mode(0);
#if ENABLE_GPU
#ifdef CONFIG_MTK_GPU_SUPPORT
	mt_gpufreq_enable_by_ptpod(); /* enable gpu DVFS */
#endif
#endif
/* @@ */
#if ENABLE_VPU
	eem_debug("vpu_enable_by_ptpod 1\n");
#endif

	/* need to fix */
	mt_cpufreq_ctrl_cci_volt(VBOOT_PMIC_CLR);

	mt_ppm_ptpod_policy_deactivate();

	mcdi_pause(MCDI_PAUSE_BY_EEM, false);
#endif
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
#if ENABLE_GPU
#if ENABLE_LOO_G
	/* save CPU L/B init01 info to HIGHL/HIGHB */
	eem_detectors[EEM_DET_GPU_HI].DCVOFFSETIN =
		eem_detectors[EEM_DET_GPU].DCVOFFSETIN;
	eem_detectors[EEM_DET_GPU_HI].AGEVOFFSETIN =
		eem_detectors[EEM_DET_GPU].AGEVOFFSETIN;
#endif
#endif
#if ENABLE_LOO_B
	eem_detectors[EEM_DET_B_HI].DCVOFFSETIN =
		eem_detectors[EEM_DET_B].DCVOFFSETIN;
	eem_detectors[EEM_DET_B_HI].AGEVOFFSETIN =
		eem_detectors[EEM_DET_B].AGEVOFFSETIN;
#endif
	eem_init02(__func__);
	FUNC_EXIT(FUNC_LV_LOCAL);
}

#if EN_EEM
#if SUPPORT_DCONFIG
static void eem_dconfig_set_det(struct eem_det *det, struct device_node *node)
{
	enum eem_det_id det_id = det_to_id(det);
#if ENABLE_LOO
	struct eem_det *highdet;
#endif
	int doe_initmon = 0xF, doe_clamp = 0;
	int doe_offset = 0xFF;
	int rc1 = 0, rc2 = 0, rc3 = 0;

#if ENABLE_GPU
	if (det_id > EEM_DET_GPU)
		return;
#endif

	switch (det_id) {
	case EEM_DET_L:
		rc1 = of_property_read_u32(node, "eem-initmon-little",
			&doe_initmon);
		rc2 = of_property_read_u32(node, "eem-clamp-little",
			&doe_clamp);
		rc3 = of_property_read_u32(node, "eem-offset-little",
			&doe_offset);
		break;
	case EEM_DET_B:
		rc1 = of_property_read_u32(node, "eem-initmon-big",
			&doe_initmon);
		rc2 = of_property_read_u32(node, "eem-clamp-big",
			&doe_clamp);
		rc3 = of_property_read_u32(node, "eem-offset-big",
			&doe_offset);
		break;
	case EEM_DET_CCI:
		rc1 = of_property_read_u32(node, "eem-initmon-cci",
			&doe_initmon);
		rc2 = of_property_read_u32(node, "eem-clamp-cci",
			&doe_clamp);
		rc3 = of_property_read_u32(node, "eem-offset-cci",
			&doe_offset);
		break;
#if ENABLE_GPU
	case EEM_DET_GPU:
		rc1 = of_property_read_u32(node, "eem-initmon-gpu",
			&doe_initmon);
		rc2 = of_property_read_u32(node, "eem-clamp-gpu",
			&doe_clamp);
		rc3 = of_property_read_u32(node, "eem-offset-gpu",
			&doe_offset);
		break;
#endif
	default:
		eem_debug("[%s]: Unknown det_id %d\n", __func__, det_id);
		break;
	}
#if 0
	eem_error("[DCONFIG] det_id:%d, feature modified by DT(0x%x)\n",
			det_id, doe_initmon);
	eem_error("[DCONFIG] doe_offset:%x, doe_clamp:%x\n",
			doe_offset, doe_clamp);
#endif
	if (!rc1) {
		if ((((doe_initmon >= 0x0) && (doe_initmon <= 0x3)) ||
			((doe_initmon >= 0x6) && (doe_initmon <= 0x7))) &&
			(det->features != doe_initmon)) {
			det->features = doe_initmon;
			eem_error("[DCONFIG] feature modified by DT(0x%x)\n",
				doe_initmon);

			if (HAS_FEATURE(det, FEA_INIT01) == 0)
				final_init01_flag &= ~BIT(det_id);
			else
				final_init01_flag |= BIT(det_id);


			switch (det_id) {
			case EEM_DET_L:
			case EEM_DET_CCI:
#if UPDATE_TO_UPOWER
				if (HAS_FEATURE(det, FEA_INIT02) == 0)
					eem_update_init2_volt_to_upower
					(det, det->volt_tbl_orig);
#endif
				break;
			case EEM_DET_B:
#if UPDATE_TO_UPOWER
				if (HAS_FEATURE(det, FEA_INIT02) == 0) {
					eem_update_init2_volt_to_upower
					(det, det->volt_tbl_orig);
#if ENABLE_LOO_B
					bcpu_final_init02_flag = 0;
#endif
				}
#endif

#if ENABLE_LOO_B
				if (big_2line) {
					/* Change BCPU Low_bank & High_bank */
					highdet = id_to_eem_det(EEM_DET_B_HI);
					highdet->features = doe_initmon & 0x6;
				}
#endif
				break;
#if ENABLE_GPU
			case EEM_DET_GPU:
#if ENABLE_LOO_G
				if (gpu_2line) {
					/* Change GPU Low_bank & High_bank */
					highdet = id_to_eem_det(EEM_DET_GPU_HI);
					highdet->features = doe_initmon & 0x6;
				}
#endif
				break;
#endif
			default:
				break;
			}
		}
	}

	if (!rc2)
		det->volt_clamp = doe_clamp;

	if ((!rc3) && (doe_offset != 0xFF)) {
		if (doe_offset < 1000)
			det->volt_offset = doe_offset;
		else
			det->volt_offset = 0 - (doe_offset - 1000);
	}

}
#endif

#if EN_EEM_THERM_CLK
int eem_thermal_clock_on(void)
{
	int ret = -1;

	ret = clk_prepare_enable(eem_therm_main);
	if (ret)
		eem_error("Cannot enable thermal clock.\n");

	return ret;
}
#endif

static int eem_probe(struct platform_device *pdev)
{
	int ret;
	struct eem_det *det;
	struct eem_ctrl *ctrl;
#ifdef CONFIG_OF
	struct device_node *node = NULL;
	struct device_node *node_infra = NULL;
	struct device_node *node_mcucfg = NULL;
#endif

#if SUPPORT_DCONFIG
	int doe_status;
#endif

	FUNC_ENTER(FUNC_LV_MODULE);

#ifdef CONFIG_OF
	node = pdev->dev.of_node;
	if (!node) {
		eem_error("get eem device node err\n");
		return -ENODEV;
	}

#if SUPPORT_DCONFIG
	if (of_property_read_u32(node, "eem-status",
		&doe_status) < 0) {
		eem_debug("[DCONFIG] eem-status read error!\n");
	} else {
		eem_debug("[DCONFIG] success-> status:%d, EEM_Enable:%d\n",
			doe_status, ctrl_EEM_Enable);
		if (((doe_status == 1) || (doe_status == 0)) &&
			(ctrl_EEM_Enable != doe_status)) {
			ctrl_EEM_Enable = doe_status;
			eem_debug("[DCONFIG] status modified by DT(0x%x).\n",
				doe_status);
		}
	}
#endif

	if (ctrl_EEM_Enable == 0) {
		eem_error("ctrl_EEM_Enable = 0\n");
		FUNC_EXIT(FUNC_LV_MODULE);
		return 0;
	}

	/* Setup IO addresses */
	eem_base = of_iomap(node, 0);
	eem_debug("[EEM] eem_base = 0x%p\n", eem_base);
	eem_irq_number = irq_of_parse_and_map(node, 0);
	eem_debug("[THERM_CTRL] eem_irq_number=%d\n", eem_irq_number);
	if (!eem_irq_number) {
		eem_error("[EEM] get irqnr failed=0x%x\n", eem_irq_number);
		return 0;
	}

	/* infra_ao */
	node_infra = of_find_compatible_node(NULL, NULL, INFRA_AO_NODE);
	if (!node_infra) {
		eem_debug("INFRA_AO_NODE Not Found\n");
		return 0;
	}

	infra_base = of_iomap(node_infra, 0);
	if (!infra_base) {
		eem_debug("infra_ao Map Failed\n");
		return 0;
	}

	/* node_mcucfg */
	node_mcucfg = of_find_compatible_node(NULL, NULL, MCUCFG_NODE);
	if (!node_mcucfg) {
		eem_debug("MCUCFG_NODE Not Found\n");
		return 0;
	}

	mcucfg_base = of_iomap(node_mcucfg, 0);
	if (!mcucfg_base) {
		eem_debug("mcucfg Map Failed\n");
		return 0;
	}

#endif

#if EN_EEM_THERM_CLK
#if !defined(CONFIG_MTK_CLKMGR)
	eem_therm_main = devm_clk_get(&pdev->dev, "therm-main");
	if (IS_ERR(eem_therm_main)) {
		eem_error("cannot get thermal clock.\n");
		return 0;
	}
	eem_debug("eem_therm_main Ptr=%p", eem_therm_main);
	eem_thermal_clock_on();
#endif
#endif

	create_procfs();
	informEEMisReady = 1;

	/* set EEM IRQ */
	ret = request_irq(eem_irq_number, eem_isr,
			IRQF_TRIGGER_HIGH, "eem", NULL);
	if (ret) {
		eem_error("EEM IRQ register failed (%d)\n", ret);
		WARN_ON(1);
	}
	eem_debug("Set EEM IRQ OK.\n");

#ifndef MC50_LOAD
#if SUPPORT_PICACHU
	get_picachu_efuse();
#endif
#endif

#ifdef CONFIG_EEM_AEE_RR_REC
		_mt_eem_aee_init();
#endif

#if defined(AGING_LOAD)
	eem_debug("@%s: AGING flavor name: %s\n",
		__func__, PROJECT_DTB_NAMES);
	eem_aging_enable = 1;
#endif

	if (eem_aging_enable) {
		eem_clamp_val = CLAMP_VMAX_VAL_B_T_AGING;
		pmic_clamp_val = CLAMP_VMAX_PMIC_VAL_B_T_AGING;
	} else {
		eem_clamp_val = CLAMP_VMAX_VAL_B_T;
		pmic_clamp_val = CLAMP_VMAX_PMIC_VAL_B_T;
	}


	for_each_ctrl(ctrl)
		eem_init_ctrl(ctrl);

	eem_debug("finish eem_init_ctrl\n");
#if !EARLY_PORTING
	/* CPU/GPU pre-process(need to fix) */
	mt_cpufreq_ctrl_cci_volt(VBOOT_PMIC_VAL);

	mcdi_pause(MCDI_PAUSE_BY_EEM, true);

	mt_ppm_ptpod_policy_activate();

#if ENABLE_GPU
#ifdef CONFIG_MTK_GPU_SUPPORT
	mt_gpufreq_disable_by_ptpod();
#endif
#endif
/* @@ */
#if ENABLE_VPU
	eem_debug("vpu_disable_by_ptpod 1\n");
	/* vpu_disable_by_ptpod(); */
	/* vpu_enable_mtcmos(); */
#endif
	ret = eem_buck_get(pdev);
	if (ret != 0)
		eem_error("eem_buck_get failed\n");

	eem_buck_set_mode(1);
#endif

	/* for slow idle */
	ptp_data[0] = 0xffffffff;
	for_each_det(det)
		eem_init_det(det, &eem_devinfo);


	final_init01_flag = EEM_INIT01_FLAG;
#ifdef CORN_LOAD
	final_corner_flag = EEM_CORNER_FLAG;
#endif

	/* get original volt from cpu dvfs before init01*/
	for_each_det(det) {
		if (det->ops->get_orig_volt_table)
			det->ops->get_orig_volt_table(det);
	}

#if SUPPORT_DCONFIG
	for_each_det(det)
		eem_dconfig_set_det(det, node);
#endif


	eem_init01();

	ptp_data[0] = 0;

#if ENABLE_INIT1_STRESS
	init_waitqueue_head(&wqStress);
	threadStress = kthread_run(eem_init1stress_thread_handler,
			0, "Init_1_Stress");

	if (IS_ERR(threadStress))
		eem_error("Create %s thread failed: %ld\n",
				"Init_1_Stress", PTR_ERR(threadStress));
#endif

	eem_debug("%s ok\n", __func__);
	FUNC_EXIT(FUNC_LV_MODULE);
	return 0;
}

static int eem_suspend(void)
{
	struct eem_det *det;
	unsigned long flag;

	if (ctrl_EEM_Enable) {
		eem_error("Start EEM suspend\n");
		mt_ptp_lock(&flag);
		for_each_det(det) {
			det->ops->switch_bank(det, NR_EEM_PHASE);
			eem_write(EEMEN, 0x0 | SEC_MOD_SEL);
			/* clear all pending EEM int & config EEMINTEN */
			eem_write(EEMINTSTS, 0xffffffff);
		}
		mt_ptp_unlock(&flag);
	}

	return 0;
}

static int eem_resume(void)
{
	if (ctrl_EEM_Enable) {
		eem_error("Start EEM resume\n");
		/* Reset EEM */
		eem_write(INFRA_EEM_RST, (1 << 9));
		eem_write(INFRA_EEM_CLR, (1 << 9));
		eem_init02(__func__);
	}
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_eem_of_match[] = {
	{ .compatible = "mediatek,eem_fsm", },
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
	if (det == NULL)
		return 0;

	FUNC_EXIT(FUNC_LV_API);

	return det->num_freq_tbl;
}
EXPORT_SYMBOL(mt_eem_opp_num);

void mt_eem_opp_freq(enum eem_det_id id, unsigned int *freq)
{
	struct eem_det *det = id_to_eem_det(id);
	int i = 0;

	FUNC_ENTER(FUNC_LV_API);

	if (det == NULL)
		return;

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
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK2);
	else if (id == EEM_DET_B)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK0);
	else if (id == EEM_DET_CCI)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK2);
#if ENABLE_GPU
	else if (id == EEM_DET_GPU)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK4);
#if ENABLE_LOO_G
	else if (id == EEM_DET_GPU_HI)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK4);
#endif
#endif
#if ENABLE_MDLA
	else if (id == EEM_DET_MDLA)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK3);
#endif
#if ENABLE_VPU
	else if (id == EEM_DET_VPU)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK3);
#endif
#if ENABLE_LOO_B
	else if (id == EEM_DET_B_HI)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK0);
#endif

	else
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK0);
#else
	*temp = 0;
#endif

	if (det == NULL)
		return;

	for (i = 0; i < det->num_freq_tbl; i++)
		volt[i] = det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_eem_opp_status);

/***************************
 * return current EEM stauts
 ***************************
 */
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
	eem_debug("in eem debug proc write 2~~~~~~~~\n");

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &enabled)) {
		ret = 0;

		eem_debug("in eem debug proc write 3~~~~~~~~\n");
		if (enabled == 0)
			/* det->ops->enable(det, BY_PROCFS); */
			det->ops->disable(det, 0);
		else if (enabled == 1)
			det->ops->disable(det, BY_PROCFS);
	} else
		ret = -EINVAL;

out:
	eem_debug("in eem debug proc write 4~~~~~~~~\n");
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

/*
 * show current aging margin
 */
static int eem_setmargin_proc_show(struct seq_file *m, void *v)
{
	struct eem_det *det = (struct eem_det *)m->private;
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	/* FIXME: EEMEN sometimes is disabled temp */
	seq_printf(m, "[%s] volt clamp:%d\n",
		   ((char *)(det->name) + 8),
		   det->volt_clamp);

	for (i = 0; i < det->num_freq_tbl; i++) {
		seq_printf(m, "[Opp%d] aging margin:%d\n",
	   i, det->volt_aging[i]);
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * remove aging margin
 */
static ssize_t eem_setmargin_proc_write(struct file *file,
			const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	int aging_val[2];
	int i = 0;
	int start_oft, end_oft;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eem_det *det = (struct eem_det *)PDE_DATA(file_inode(file));
	char *tok;
	char *cmd_str = NULL;

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

	cmd_str = strsep(&buf, " ");
	if (cmd_str == NULL)
		ret = -EINVAL;

	while ((tok = strsep(&buf, " ")) != NULL) {
		if (i == 3) {
			eem_error("number of arguments > 3!\n");
			goto out;
		}

		if (kstrtoint(tok, 10, &aging_val[i])) {
			eem_error("Invalid input: %s\n", tok);
			goto out;
		} else
			i++;
	}

	if (!strncmp(cmd_str, "aging", sizeof("aging"))) {
		start_oft = aging_val[0];
		end_oft = aging_val[1];
		eem_calculate_aging_margin(det, start_oft, end_oft);

		ret = count;
	} else if (!strncmp(cmd_str, "clamp", sizeof("clamp"))) {
		if (aging_val[0] < 20)
			det->volt_clamp = aging_val[0];

		ret = count;
	} else if (!strncmp(cmd_str, "setopp", sizeof("setopp"))) {
		if ((aging_val[0] >= 0) && (aging_val[0] < det->num_freq_tbl))
			det->volt_aging[aging_val[0]] = aging_val[1];

		ret = count;
	} else {
		ret = -EINVAL;
		goto out;
	}

	eem_set_eem_volt(det);

out:
	eem_debug("in eem debug proc write 4~~~~~~~~\n");
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
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

	for (i = EEM_PHASE_INIT01; i <= EEM_PHASE_MON; i++) {
		seq_printf(m, "Bank_number = %d\n", det->ctrl_id);
		if (i < EEM_PHASE_MON)
			seq_printf(m, "mode = init%d\n", i+1);
		else
			seq_puts(m, "mode = mon\n");
		if (eem_log_en) {
			seq_printf(m, "0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
				det->dcvalues[i],
				det->freqpct30[i],
				det->eem_26c[i],
				det->vop30[i],
				det->eem_eemEn[i]
			);

			if (det->eem_eemEn[i] == (0x5 | SEC_MOD_SEL)) {
				seq_printf(m, "EEM_LOG: Bank_number = [%d] (%d) - (",
				det->ctrl_id, det->ops->get_temp(det));

				for (k = 0; k < det->num_freq_tbl - 1; k++)
					seq_printf(m, "%d, ",
					det->ops->pmic_2_volt(det,
					det->volt_tbl_pmic[k]));
				seq_printf(m, "%d) - (",
						det->ops->pmic_2_volt(det,
						det->volt_tbl_pmic[k]));

				for (k = 0; k < det->num_freq_tbl - 1; k++)
					seq_printf(m, "%d, ", det->freq_tbl[k]);
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
	int *val = (int *)&eem_devinfo;
	struct eem_det *det;
	unsigned int i;

	FUNC_ENTER(FUNC_LV_HELP);

	/* Depend on EFUSE location */
	for (i = 0; i < sizeof(struct eem_devinfo) / sizeof(unsigned int);
		i++)
		seq_printf(m, "M_HW_RES%d\t= 0x%08X\n", i, val[i]);

	for_each_det(det) {
		eem_dump_reg_by_det(det, m);
	}

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}

static int eem_hrid_proc_show(struct seq_file *m, void *v)
{
	unsigned int i;

	FUNC_ENTER(FUNC_LV_HELP);
	for (i = 0; i < 4; i++)
		seq_printf(m, "%s[HRID][%d]: 0x%08X\n", EEM_TAG, i,
			get_devinfo_with_index(DEVINFO_HRID_0 + i));

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}

static int eem_efuse_proc_show(struct seq_file *m, void *v)
{
	int *val = (int *)&eem_devinfo;
	unsigned int i;

	FUNC_ENTER(FUNC_LV_HELP);
	for (i = 0; i < 24; i++)
		seq_printf(m, "%s[PTP_DUMP] ORIG_RES%d: 0x%08X\n", EEM_TAG, i,
			get_devinfo_with_index(DEVINFO_IDX_0 + i));

	/* Depend on EFUSE location */
	for (i = 0; i < sizeof(struct eem_devinfo) / sizeof(unsigned int);
		i++)
		seq_printf(m, "%s[PTP_DUMP] RES%d: 0x%08X\n", EEM_TAG, i,
			val[i]);

	for (i = NR_HW_RES_FOR_BANK; i <= 24; i++)
		seq_printf(m, "%s[PTP_DUMP] RES%d: 0x%08X\n", EEM_TAG, i,
			get_devinfo_with_index(DEVINFO_IDX_0 + i));

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}

static int eem_freq_proc_show(struct seq_file *m, void *v)
{
	struct eem_det *det;
	unsigned int i;
	enum mt_cpu_dvfs_id cpudvfsindex;

	FUNC_ENTER(FUNC_LV_HELP);
	for_each_det(det) {
		cpudvfsindex = detid_to_dvfsid(det);
		for (i = 0; i < NR_FREQ_CPU; i++) {
			if (det->ctrl_id <= EEM_CTRL_CCI) {
				seq_printf(m,
					"%s[DVFS][CPU_%s][OPP%d] volt:%d, freq:%d\n",
					EEM_TAG, cpu_name[cpudvfsindex], i,
					det->ops->pmic_2_volt(det,
					det->volt_tbl_orig[i]) * 10,
#if SET_PMIC_VOLT_TO_DVFS
					mt_cpufreq_get_freq_by_idx(cpudvfsindex,
									i)
					/ 1000
#else
					0
#endif
					);
			}
		}
	}

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}

static int eem_mar_proc_show(struct seq_file *m, void *v)
{
	FUNC_ENTER(FUNC_LV_HELP);

	seq_printf(m, "%s[CPU_BIG][HIGH] 1:%d, 2:%d, 3:%d, 5:%d\n",
			EEM_TAG, LOW_TEMP_OFF_B, 0,
			HIGH_TEMP_OFF_B, AGING_VAL_CPU_B);

	seq_printf(m, "%s[CPU_BIG][MID] 1:%d, 2:%d, 3:%d, 5:%d\n",
			EEM_TAG, LOW_TEMP_OFF_B, 0,
			HIGH_TEMP_OFF_B, AGING_VAL_CPU_B);

	seq_printf(m, "%s[CPU_L][HIGH] 1:%d, 2:%d, 3:%d, 5:%d\n",
			EEM_TAG, LOW_TEMP_OFF_L, 0,
			HIGH_TEMP_OFF_L, AGING_VAL_CPU);

	seq_printf(m, "%s[CPU_CCI][HIGH] 1:%d, 2:%d, 3:%d, 5:%d\n",
			EEM_TAG, LOW_TEMP_OFF_L, 0,
			HIGH_TEMP_OFF_L, AGING_VAL_CPU);

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
		seq_printf(m, "EEM[%s] read current voltage fail\n", det->name);

	if (det->features != 0) {
		for (i = 0; i < det->num_freq_tbl; i++)
			seq_printf(m, "[%d],eem = [%x], pmic = [%x], volt = [%d]\n",
			i,
			det->volt_tbl[i],
			det->volt_tbl_pmic[i],
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));

		seq_printf(m, "policy:%d, isTempInv:%d, eem_aging:%d, warn_on:%d\n",
		det->volt_policy, det->isTempInv, eem_aging, is_warn_on);
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

	seq_printf(m, "bank = %d, feature:%d, T(%d) - (",
		   det->ctrl_id, det->features, det->ops->get_temp(det));
	for (i = 0; i < det->num_freq_tbl - 1; i++)
		seq_printf(m, "%d, ", det->ops->pmic_2_volt(det,
					det->volt_tbl_pmic[i]));
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
		hrtimer_start(&eem_log_timer,
			ns_to_ktime(LOG_INTERVAL), HRTIMER_MODE_REL);
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
		eem_error("[%s]\n", __func__);
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
		.owner		= THIS_MODULE,				\
		.open		= name ## _proc_open,			\
		.read		= seq_read,				\
		.llseek		= seq_lseek,				\
		.release	= single_release,			\
		.write		= name ## _proc_write,			\
	}

#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		= THIS_MODULE,				\
		.open		= name ## _proc_open,			\
		.read		= seq_read,				\
		.llseek		= seq_lseek,				\
		.release	= single_release,			\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(eem_debug);
PROC_FOPS_RO(eem_status);
PROC_FOPS_RO(eem_cur_volt);
PROC_FOPS_RW(eem_offset);
PROC_FOPS_RO(eem_dump);
PROC_FOPS_RO(eem_hrid);
PROC_FOPS_RO(eem_efuse);
PROC_FOPS_RO(eem_freq);
PROC_FOPS_RO(eem_mar);
PROC_FOPS_RW(eem_log_en);
PROC_FOPS_RW(eem_setmargin);
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
		PROC_ENTRY(eem_setmargin),
	};

	struct pentry eem_entries[] = {
		PROC_ENTRY(eem_dump),
		PROC_ENTRY(eem_hrid),
		PROC_ENTRY(eem_efuse),
		PROC_ENTRY(eem_freq),
		PROC_ENTRY(eem_mar),
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

	/* if ctrl_EEM_Enable =1, and has efuse value,
	 * create other banks procfs
	 */
	if (ctrl_EEM_Enable != 0 && eem_checkEfuse == 1) {
		for (i = 0; i < ARRAY_SIZE(eem_entries); i++) {
			if (!proc_create(eem_entries[i].name, 0664,
						eem_dir, eem_entries[i].fops)) {
				eem_error("[%s]: create /proc/eem/%s failed\n",
						__func__,
						eem_entries[i].name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -3;
			}
		}

		for_each_det(det) {
			if (det->features == 0)
				continue;

			det_dir = proc_mkdir(det->name, eem_dir);

			if (!det_dir) {
				eem_debug("[%s]: mkdir /proc/eem/%s failed\n"
						, __func__, det->name);
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

void eem_set_pi_efuse(enum eem_det_id id,
			unsigned int pi_efuse,
			unsigned int loo_enabled)
{
	struct eem_det *det = id_to_eem_det(id);

	if (!det)
		return;

	det->pi_loo_enabled = loo_enabled;
	det->pi_efuse = pi_efuse;
}

void eem_set_pi_dvtfixed(enum eem_det_id id, unsigned int pi_dvtfixed)
{
	struct eem_det *det = id_to_eem_det(id);

	if (!det)
		return;

	det->pi_dvtfixed = pi_dvtfixed;
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
#ifdef EEM_NOT_READY
	return 0;
#endif

	eem_debug("[EEM] ctrl_EEM_Enable=%d\n", ctrl_EEM_Enable);

	get_devinfo();

	/* move to eem_probe */
	/* create_procfs(); */

	if (eem_checkEfuse == 0) {
		eem_error("eem_checkEfuse = 0\n");
		FUNC_EXIT(FUNC_LV_MODULE);
		return 0;
	}

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
