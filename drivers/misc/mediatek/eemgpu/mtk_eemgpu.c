// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/**
 * @file	mtk_eem.
 * @brief   Driver for EEM
 *
 */

#define __MTK_EEMG_C__
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
#include <linux/thermal.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_platform.h>
#include <linux/suspend.h>
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <mt-plat/aee.h>
#include "mtk_eemgpu_config.h"
#include "mtk_eemgpu.h"
#include "mtk_defeemgpu.h"
#include "mtk_eemgpu_internal_ap.h"
#include "mtk_eemgpu_internal.h"
#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
#include "mtk_gpufreq.h"
#endif
#include <regulator/consumer.h>

#define INFRA_EEMG_RST          (infra_base_gpu + 0x150)
#define INFRA_EEMG_CLR          (infra_base_gpu + 0x154)
#define LVTS_COEFF_A_X_1000                     (-250460)
#define LVTS_COEFF_B_X_1000                      (250460)
#define DEFAULT_EFUSE_GOLDEN_TEMP                (50)
#define WAIT_TIME       (2500000)

enum thermal_bank_name {
	THERMAL_BANK0 = 0,
	THERMAL_BANK1,
	THERMAL_BANK2,
	THERMAL_BANK3,
	THERMAL_BANK4,
	THERMAL_BANK5,
	THERMAL_BANK_NUM
};

struct TS_PTPOD {
	unsigned int ts_MTS;
	unsigned int ts_BTS;
};

static unsigned int ctrl_EEMG_Enable = 1;
static unsigned int gpu_2line;
static unsigned long long eemg_pTime_us, eemg_cTime_us, eemg_diff_us;
static unsigned int final_init01_flag;
unsigned int gpu_final_init02_flag;
unsigned int gpu_cur_init02_flag;
static unsigned int golden_temp;
static struct eemg_devinfo eemg_devinfo;
static struct hrtimer eemg_log_timer;
static DEFINE_SPINLOCK(eemg_spinlock);
static int eemg_log_en;
static unsigned int eemg_checkEfuse = 1;
void __iomem *eemg_base;
void __iomem *infra_base_gpu;
static u32 eemg_irq_number;

unsigned int gpu_vb;
unsigned int gpu_vb_volt;
unsigned int gpu_vb_turn_pt;

DEFINE_MUTEX(gpu_mutex_g);

static int create_procfs(struct platform_device *pdev);
static void eemg_set_eemg_volt(struct eemg_det *det);
static void eemg_restore_eemg_volt(struct eemg_det *det);
static unsigned int interpolate(unsigned int y1, unsigned int y0,
	unsigned int x1, unsigned int x0, unsigned int ym);
static void eemg_fill_freq_table(struct eemg_det *det);
static void read_volt_from_VOP(struct eemg_det *det);

static struct eemg_det *id_to_eemg_det(enum eemg_det_id id)
{
	if (likely(id < NR_EEMG_DET))
		return &eemg_detectors[id];
	else
		return NULL;
}

static int get_devinfo(struct platform_device *pdev)
{
	int ret = 1, i = 0;
	int *val;
	unsigned int safeEfuse = 0;
	struct nvmem_device *nvmem_dev;
	//struct device_node *node = pdev->dev.of_node;
	struct device_node *node;
	int devinfo = 0, efuse = 0, efuse_mask = 0;
	int count = 0;
	int mcl50 = 0;

	FUNC_ENTER(FUNC_LV_HELP);
	val = (int *)&eemg_devinfo;

	nvmem_dev = nvmem_device_get(&pdev->dev, "mtk_efuse");
	if (IS_ERR(nvmem_dev))
		return -ENODEV;

	node = of_find_compatible_node(NULL, NULL, "mediatek,cpufreq-hw");
	if (node) {
		ret = of_property_read_u32(node, "mcl50_load", &mcl50);
		if (!ret && mcl50)
			mcl50 = 1;
	}

	node = pdev->dev.of_node;
	ret = of_property_read_u32(node, "gpu-vb", &efuse);
	if (efuse == 1) {
		gpu_vb = 1;
		of_property_read_u32_index(node, "gpu-vb-efuse", 0, &efuse);
		of_property_read_u32_index(node, "gpu-vb-efuse", 1,
			&efuse_mask);
		nvmem_device_read(nvmem_dev, efuse, sizeof(__u32), &devinfo);
		of_property_read_u32_index(node, "gpu-vb-opp0",
			(devinfo & efuse_mask) >> find_first_bit(
			(unsigned long *)&efuse_mask, 32), &gpu_vb_volt);
		if (mcl50) {
			ret = of_property_read_u32(node, "gpu-vb-opp0-mcl50",
				&gpu_vb_volt);
			eemg_error("mc50 load setting\n");
		}
	} else
		gpu_vb = 0;

	count = of_property_count_u32_elems(node, "devinfo-idx");

	ret = of_property_read_u32(node, "devinfo-fake-ignore", &efuse_mask);

	for (i = 0; i < count ; i++) {
		of_property_read_u32_index(node, "devinfo-idx", i, &efuse);
		nvmem_device_read(nvmem_dev, efuse, sizeof(__u32), &devinfo);
		val[i] = devinfo;
		if (efuse_mask & BIT(i))
			continue;
		else if (val[i] == 0) {
			eemg_error("No EFUSE (val[%d]), use safe efuse\n", i);
			safeEfuse = 1;
		}
	}

	if (safeEfuse) {
		for (i = 0; i < count ; i++) {
			of_property_read_u32_index(node, "devinfo", i, &efuse);
			val[i] = efuse;
			eemg_error("[PTP_DUMP] RES%d: 0x%X\n", i, val[i]);
		}
	} else {
		for (i = 0; i < count ; i++)
			eemg_error("[PTP_DUMP] RES%d: 0x%X\n", i, val[i]);
	}


	if (mcl50) {
		for (i = 0; i < count ; i++) {
			of_property_read_u32_index(node, "devinfo-mcl50", i, &efuse);
			val[i] = efuse;
			eemg_error("[PTP_DUMP] RES%d: 0x%X\n", i, val[i]);
		}

	}

	gpu_2line = 1;

	ret = of_property_read_u32(node, "golden-temp", &efuse);

	nvmem_device_read(nvmem_dev, efuse, sizeof(__u32), &devinfo);

	ret = of_property_read_u32(node, "golden-temp-mask", &efuse);

	golden_temp = ((devinfo & efuse)) >> find_first_bit((unsigned long *)&efuse, 32);

	if (golden_temp != 0) {
		ret = of_property_read_u32(node, "golden-temp-default", &efuse);
		golden_temp = efuse;
	}
	ret = 1;

	FUNC_EXIT(FUNC_LV_HELP);
	return ret;
}

void get_lvts_slope_intercept(struct TS_PTPOD *ts_info)
{
	struct TS_PTPOD ts_ptpod;
	int temp;

	/* chip dependent */
	temp = (0 - LVTS_COEFF_A_X_1000) * 2;
	temp /= 1000;
	ts_ptpod.ts_MTS = temp;
	temp = 500 * golden_temp + LVTS_COEFF_B_X_1000;
	temp /= 1000;
	ts_ptpod.ts_BTS = (temp - 25) * 4;
	ts_info->ts_MTS = ts_ptpod.ts_MTS;
	ts_info->ts_BTS = ts_ptpod.ts_BTS;
}

/*============================================================
 * function declarations of EEM detectors
 *============================================================
 */
static void mt_ptpgpu_lock(unsigned long *flags);
static void mt_ptpgpu_unlock(unsigned long *flags);

/*=============================================================
 * Local function definition
 *=============================================================
 */
#ifdef CONFIG_EEMG_AEE_RR_REC
static void _mt_eemg_aee_init(void)
{
	aee_rr_rec_ptp_vboot(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_gpu_volt(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_gpu_volt_1(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_gpu_volt_2(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_gpu_volt_3(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_temp(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_status(0xFF);
}
#endif

#ifdef CONFIG_THERMAL
/* common part in thermal */
int tscpu_get_temp_by_bank(enum thermal_bank_name ts_bank)
{
	struct thermal_zone_device *zone;
	char cell_name[12];
	int i, ret, temperature = 0, total = 0;

	switch (ts_bank) {
	case THERMAL_BANK0:
		for (i = 0; i < 4; i++) {
			memset(cell_name, '\0', 12);
			ret = snprintf(cell_name, 9, "cpu_big%d", i + 1);
			if (ret <= 0)
				return EEM_LOW_T;
			zone = thermal_zone_get_zone_by_name(cell_name);
			if (IS_ERR(zone))
				return EEM_LOW_T;
			ret = thermal_zone_get_temp(zone, &temperature);
			if (ret != 0)
				return EEM_LOW_T;
			total += temperature;
		}
		total /= 4;
		break;
	case THERMAL_BANK1:
		return EEM_LOW_T;
	case THERMAL_BANK2:
		for (i = 0; i < 2; i++) {
			memset(cell_name, '\0', 12);
			ret = snprintf(cell_name, 12, "cpu_little%d", i + 1);
			if (ret <= 0)
				return EEM_LOW_T;
			zone = thermal_zone_get_zone_by_name(cell_name);
			if (IS_ERR(zone))
				return EEM_LOW_T;
			ret = thermal_zone_get_temp(zone, &temperature);
			if (ret != 0)
				return EEM_LOW_T;
			total += temperature;
		}
		total /= 2;
		break;
	case THERMAL_BANK3:
		break;
	case THERMAL_BANK4:
		for (i = 0; i < 2; i++) {
			memset(cell_name, '\0', 12);
			ret = snprintf(cell_name, 5, "gpu%d", i + 1);
			if (ret <= 0)
				return EEM_LOW_T;
			zone = thermal_zone_get_zone_by_name(cell_name);
			if (IS_ERR(zone))
				return EEM_LOW_T;
			ret = thermal_zone_get_temp(zone, &temperature);
			if (ret != 0)
				return EEM_LOW_T;
			total += temperature;
		}
		total /= 2;
		break;
	default:
		/* un-handled cases */
		break;
	}
	return total;
}
#endif


static struct eemg_ctrl *id_to_eemg_ctrl(enum eemg_ctrl_id id)
{
	if (likely(id < NR_EEMG_CTRL))
		return &eemg_ctrls[id];
	else
		return NULL;
}

void base_ops_enable_gpu(struct eemg_det *det, int reason)
{
	/* FIXME: UNDER CONSTRUCTION */
	FUNC_ENTER(FUNC_LV_HELP);
	det->disabled &= ~reason;
	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_switch_bank_gpu(struct eemg_det *det, enum eemg_phase phase)
{
	unsigned int coresel;

	FUNC_ENTER(FUNC_LV_HELP);

	coresel = (eemg_read(EEMGCORESEL) & ~BITMASK(2:0))
		| BITS(2:0, det->ctrl_id);

	/* 803f0000 + det->ctrl_id = enable ctrl's swcg clock */
	/* 003f0000 + det->ctrl_id = disable ctrl's swcg clock */
	/* bug: when system resume, need to restore coresel value */
	if (phase == EEMG_PHASE_INIT01) {
		coresel |= CORESEL_VAL;
	} else {
		coresel |= CORESEL_INIT2_VAL;
#if defined(CFG_THERM_LVTS) && CFG_LVTS_DOMINATOR
		coresel &= 0x0fffffff;
#else
		coresel &= 0x0ffffeff;  /* get temp from AUXADC */
#endif
	}
	eemg_write(EEMGCORESEL, coresel);

	if ((eemg_read(EEMGCORESEL) & 0x7) != (coresel & 0x7)) {
#ifdef CONFIG_EEMG_AEE_RR_REC
		aee_kernel_warning("mt_eem",
		"@%s():%d, EEMGCORESEL %x != %x\n",
		__func__,
		__LINE__,
		eemg_read(EEMGCORESEL),
		coresel);
#endif
		WARN_ON(eemg_read(EEMGCORESEL) != coresel);
	}

	eemg_debug("[%s] 0x1100bf00=0x%x\n",
			((char *)(det->name) + 8), eemg_read(EEMGCORESEL));

	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_disable_locked_gpu(struct eemg_det *det, int reason)
{
	FUNC_ENTER(FUNC_LV_HELP);

	switch (reason) {
	case BY_MON_ERROR: /* 4 */
	case BY_INIT_ERROR: /* 2 */
		/* disable EEM */
		eemg_write(EEMGEN, 0x0 | SEC_MOD_SEL);

		/* Clear EEM interrupt EEMGINTSTS */
		eemg_write(EEMGINTSTS, 0x00ffffff);

	case BY_PROCFS: /* 1 */
		det->disabled |= reason;
		eemg_restore_eemg_volt(det);
		break;

	default:
		eemg_debug("det->disabled=%x\n", det->disabled);
		det->disabled &= ~BY_PROCFS;
		eemg_debug("det->disabled=%x\n", det->disabled);
		eemg_set_eemg_volt(det);
		break;
	}

	eemg_debug("Disable EEM[%s] done. reason=[%d]\n",
			det->name, det->disabled);

	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_disable_gpu(struct eemg_det *det, int reason)
{
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	mt_ptpgpu_lock(&flags);
	det->ops->switch_bank_gpu(det, NR_EEMG_PHASE);
	det->ops->disable_locked_gpu(det, reason);
	mt_ptpgpu_unlock(&flags);

	FUNC_EXIT(FUNC_LV_HELP);
}

int base_ops_init01_gpu(struct eemg_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	if (unlikely(!HAS_FEATURE(det, FEA_INIT01))) {
		eemg_debug("det %s has no INIT01\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	det->ops->set_phase_gpu(det, EEMG_PHASE_INIT01);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

int base_ops_init02_gpu(struct eemg_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	if (unlikely(!HAS_FEATURE(det, FEA_INIT02))) {
		eemg_debug("det %s has no INIT02\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	if (det->disabled & BY_INIT_ERROR) {
		eemg_error("[%s] Disabled by INIT_ERROR\n",
				((char *)(det->name) + 8));
		det->ops->dump_status_gpu(det);
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}
	/* eemg_debug("DCV = 0x%08X, AGEV = 0x%08X\n",
	 * det->DCVOFFSETIN, det->AGEVOFFSETIN);
	 */

	/* det->ops->dump_status(det); */
	det->ops->set_phase_gpu(det, EEMG_PHASE_INIT02);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

int base_ops_mon_mode_gpu(struct eemg_det *det)
{
	struct TS_PTPOD ts_info;

	FUNC_ENTER(FUNC_LV_HELP);
	if (!HAS_FEATURE(det, FEA_MON)) {
		eemg_debug("det %s has no MON mode\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	if (det->disabled & BY_INIT_ERROR) {
		eemg_error("[%s] Disabled BY_INIT_ERROR\n",
			((char *)(det->name) + 8));
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}
	det->MTS = MTS_VAL;
	det->BTS = BTS_VAL;
	get_lvts_slope_intercept(&ts_info);
	det->MTS = ts_info.ts_MTS;
	det->BTS = ts_info.ts_BTS;
	/*
	 * eemg_debug("[base_ops_mon_mode_gpu] Bk = %d,
	 * MTS = 0x%08X, BTS = 0x%08X\n",
	 * det->ctrl_id, det->MTS, det->BTS);
	 */
	/* det->ops->dump_status(det); */
	det->ops->set_phase_gpu(det, EEMG_PHASE_MON);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

int base_ops_get_status_gpu(struct eemg_det *det)
{
	int status;
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	mt_ptpgpu_lock(&flags);
	det->ops->switch_bank_gpu(det, NR_EEMG_PHASE);
	status = (eemg_read(EEMGEN) != 0) ? 1 : 0;
	mt_ptpgpu_unlock(&flags);

	FUNC_EXIT(FUNC_LV_HELP);

	return status;
}

void base_ops_dump_status_gpu(struct eemg_det *det)
{
	unsigned int i;

	FUNC_ENTER(FUNC_LV_HELP);

	eemg_isr_info("[%s]\n",			det->name);

	eemg_isr_info("EEMINITEN = 0x%08X\n",	det->EEMINITEN);
	eemg_isr_info("EEMMONEN = 0x%08X\n",	det->EEMMONEN);
	eemg_isr_info("MDES = 0x%08X\n",		det->MDES);
	eemg_isr_info("BDES = 0x%08X\n",		det->BDES);
	eemg_isr_info("DCMDET = 0x%08X\n",	det->DCMDET);

	eemg_isr_info("DCCONFIG = 0x%08X\n",	det->DCCONFIG);
	eemg_isr_info("DCBDET = 0x%08X\n",	det->DCBDET);

	eemg_isr_info("AGECONFIG = 0x%08X\n",	det->AGECONFIG);
	eemg_isr_info("AGEM = 0x%08X\n",		det->AGEM);

	eemg_isr_info("AGEDELTA = 0x%08X\n",	det->AGEDELTA);
	eemg_isr_info("DVTFIXED = 0x%08X\n",	det->DVTFIXED);
	eemg_isr_info("MTDES = 0x%08X\n",	det->MTDES);
	eemg_isr_info("VCO = 0x%08X\n",		det->VCO);

	eemg_isr_info("DETWINDOW = 0x%08X\n",	det->DETWINDOW);
	eemg_isr_info("VMAX = 0x%08X\n",		det->VMAX);
	eemg_isr_info("VMIN = 0x%08X\n",		det->VMIN);
	eemg_isr_info("DTHI = 0x%08X\n",		det->DTHI);
	eemg_isr_info("DTLO = 0x%08X\n",		det->DTLO);
	eemg_isr_info("VBOOT = 0x%08X\n",	det->VBOOT);
	eemg_isr_info("DETMAX = 0x%08X\n",	det->DETMAX);

	eemg_isr_info("DCVOFFSETIN = 0x%08X\n",	det->DCVOFFSETIN);
	eemg_isr_info("AGEVOFFSETIN = 0x%08X\n",	det->AGEVOFFSETIN);

	eemg_isr_info("MTS = 0x%08X\n",		det->MTS);
	eemg_isr_info("BTS = 0x%08X\n",		det->BTS);

	eemg_isr_info("num_freq_tbl = %d\n", det->num_freq_tbl);

	for (i = 0; i < det->num_freq_tbl; i++)
		eemg_isr_info("freq_tbl[%d] = %d\n",
				i, det->freq_tbl[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		eemg_isr_info("volt_tbl[%d] = %d\n",
				i, det->volt_tbl[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		eemg_isr_info("volt_tbl_init2[%d] = %d\n",
				i, det->volt_tbl_init2[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		eemg_isr_info("volt_tbl_pmic[%d] = %d\n", i,
				det->volt_tbl_pmic[i]);

	FUNC_EXIT(FUNC_LV_HELP);
}


void dump_register_gpu(void)
{

	eemg_debug("EEMG_DESCHAR = 0x%x\n", eemg_read(EEMG_DESCHAR));
	eemg_debug("EEMG_TEMPCHAR = 0x%x\n", eemg_read(EEMG_TEMPCHAR));
	eemg_debug("EEMG_DETCHAR = 0x%x\n", eemg_read(EEMG_DETCHAR));
	eemg_debug("EEMG_AGECHAR = 0x%x\n", eemg_read(EEMG_AGECHAR));
	eemg_debug("EEMG_DCCONFIG = 0x%x\n", eemg_read(EEMG_DCCONFIG));
	eemg_debug("EEMG_AGECONFIG = 0x%x\n", eemg_read(EEMG_AGECONFIG));
	eemg_debug("EEMG_FREQPCT30 = 0x%x\n", eemg_read(EEMG_FREQPCT30));
	eemg_debug("EEMG_FREQPCT74 = 0x%x\n", eemg_read(EEMG_FREQPCT74));
	eemg_debug("EEMG_LIMITVALS = 0x%x\n", eemg_read(EEMG_LIMITVALS));
	eemg_debug("EEMG_VBOOT = 0x%x\n", eemg_read(EEMG_VBOOT));
	eemg_debug("EEMG_DETWINDOW = 0x%x\n", eemg_read(EEMG_DETWINDOW));
	eemg_debug("EEMGCONFIG = 0x%x\n", eemg_read(EEMGCONFIG));
	eemg_debug("EEMG_TSCALCS = 0x%x\n", eemg_read(EEMG_TSCALCS));
	eemg_debug("EEMG_RUNCONFIG = 0x%x\n", eemg_read(EEMG_RUNCONFIG));
	eemg_debug("EEMGEN = 0x%x\n", eemg_read(EEMGEN));
	eemg_debug("EEMG_INIT2VALS = 0x%x\n", eemg_read(EEMG_INIT2VALS));
	eemg_debug("EEMG_DCVALUES = 0x%x\n", eemg_read(EEMG_DCVALUES));
	eemg_debug("EEMG_AGEVALUES = 0x%x\n", eemg_read(EEMG_AGEVALUES));
	eemg_debug("EEMG_VOP30 = 0x%x\n", eemg_read(EEMG_VOP30));
	eemg_debug("EEMG_VOP74 = 0x%x\n", eemg_read(EEMG_VOP74));
	eemg_debug("TEMP = 0x%x\n", eemg_read(TEMPG));
	eemg_debug("EEMGINTSTS = 0x%x\n", eemg_read(EEMGINTSTS));
	eemg_debug("EEMGINTSTSRAW = 0x%x\n", eemg_read(EEMGINTSTSRAW));
	eemg_debug("EEMGINTEN = 0x%x\n", eemg_read(EEMGINTEN));
	eemg_debug("EEMG_CHKSHIFT = 0x%x\n", eemg_read(EEMG_CHKSHIFT));
	eemg_debug("EEMG_VDESIGN30 = 0x%x\n", eemg_read(EEMG_VDESIGN30));
	eemg_debug("EEMG_VDESIGN74 = 0x%x\n", eemg_read(EEMG_VDESIGN74));
	eemg_debug("EEMG_AGECOUNT = 0x%x\n", eemg_read(EEMG_AGECOUNT));
	eemg_debug("EEMG_SMSTATE0 = 0x%x\n", eemg_read(EEMG_SMSTATE0));
	eemg_debug("EEMG_SMSTATE1 = 0x%x\n", eemg_read(EEMG_SMSTATE1));
	eemg_debug("EEMG_CTL0 = 0x%x\n", eemg_read(EEMG_CTL0));
	eemg_debug("EEMGCORESEL = 0x%x\n", eemg_read(EEMGCORESEL));
	eemg_debug("EEMG_THERMINTST = 0x%x\n", eemg_read(EEMG_THERMINTST));
	eemg_debug("EEMGODINTST = 0x%x\n", eemg_read(EEMGODINTST));
	eemg_debug("EEMG_THSTAGE0ST = 0x%x\n", eemg_read(EEMG_THSTAGE0ST));
	eemg_debug("EEMG_THSTAGE1ST = 0x%x\n", eemg_read(EEMG_THSTAGE1ST));
	eemg_debug("EEMG_THSTAGE2ST = 0x%x\n", eemg_read(EEMG_THSTAGE2ST));
	eemg_debug("EEMG_THAHBST0 = 0x%x\n", eemg_read(EEMG_THAHBST0));
	eemg_debug("EEMG_THAHBST1 = 0x%x\n", eemg_read(EEMG_THAHBST1));
	eemg_debug("EEMGSPARE0 = 0x%x\n", eemg_read(EEMGSPARE0));
	eemg_debug("EEMGSPARE1 = 0x%x\n", eemg_read(EEMGSPARE1));
	eemg_debug("EEMGSPARE2 = 0x%x\n", eemg_read(EEMGSPARE2));
	eemg_debug("EEMGSPARE3 = 0x%x\n", eemg_read(EEMGSPARE3));
	eemg_debug("EEMG_THSLPEVEB = 0x%x\n", eemg_read(EEMG_THSLPEVEB));
	eemg_debug("EEMG_TEMP = 0x%x\n", eemg_read(TEMPG));
	eemg_debug("EEMG_THERMAL = 0x%x\n", eemg_read(EEMG_THERMAL));

}

void base_ops_set_phase_gpu(struct eemg_det *det, enum eemg_phase phase)
{
	unsigned int i, filter, val;

	FUNC_ENTER(FUNC_LV_HELP);
	det->ops->switch_bank_gpu(det, phase);

	/* config EEM register */
	eemg_write(EEMG_DESCHAR,
		  ((det->BDES << 8) & 0xff00) | (det->MDES & 0xff));
	eemg_write(EEMG_TEMPCHAR,
		  (((det->VCO << 16) & 0xff0000) |
		   ((det->MTDES << 8) & 0xff00) | (det->DVTFIXED & 0xff)));
	eemg_write(EEMG_DETCHAR,
		  ((det->DCBDET << 8) & 0xff00) | (det->DCMDET & 0xff));

	eemg_write(EEMG_DCCONFIG, det->DCCONFIG);
	eemg_write(EEMG_AGECONFIG, det->AGECONFIG);

	if (phase == EEMG_PHASE_MON)
		eemg_write(EEMG_TSCALCS,
			  ((det->BTS << 12) & 0xfff000) | (det->MTS & 0xfff));

	if (det->AGEM == 0x0)
		eemg_write(EEMG_RUNCONFIG, 0x80000000);
	else {
		val = 0x0;

		for (i = 0; i < 24; i += 2) {
			filter = 0x3 << i;

			if (((det->AGECONFIG) & filter) == 0x0)
				val |= (0x1 << i);
			else
				val |= ((det->AGECONFIG) & filter);
		}
		eemg_write(EEMG_RUNCONFIG, val);
	}

	eemg_fill_freq_table(det);

	eemg_write(EEMG_LIMITVALS,
		  ((det->VMAX << 24) & 0xff000000)	|
		  ((det->VMIN << 16) & 0xff0000)	|
		  ((det->DTHI << 8) & 0xff00)		|
		  (det->DTLO & 0xff));
	/* eemg_write(EEMG_LIMITVALS, 0xFF0001FE); */
	eemg_write(EEMG_VBOOT, (((det->VBOOT) & 0xff)));
	eemg_write(EEMG_DETWINDOW, (((det->DETWINDOW) & 0xffff)));
	eemg_write(EEMGCONFIG, (((det->DETMAX) & 0xffff)));


	eemg_write(EEMG_CHKSHIFT, 0x87);
	if (eemg_read(EEMG_CHKSHIFT) != 0x87) {
#ifdef CONFIG_EEMG_AEE_RR_REC
		aee_kernel_warning("mt_eem",
		"@%s():%d, EEMG_CHKSHIFT %x\n",
		__func__,
		__LINE__,
		eemg_read(EEMG_CHKSHIFT));
#endif
		WARN_ON(eemg_read(EEMG_CHKSHIFT) != 0x87);
	}

	/* eem ctrl choose thermal sensors */
	eemg_write(EEMG_CTL0, det->EEMCTL0);
	/* clear all pending EEM interrupt & config EEMGINTEN */
	eemg_write(EEMGINTSTS, 0xffffffff);

	/* work around set thermal register
	 * eemg_write(EEMG_THERMAL, 0x200);
	 */

	eemg_debug(" %s set phase = %d\n", ((char *)(det->name) + 8), phase);
	switch (phase) {
	case EEMG_PHASE_INIT01:
		eemg_debug("EEMG_SET_PHASE01\n ");
		eemg_write(EEMGINTEN, 0x00005f01);
		/* enable EEM INIT measurement */
		eemg_write(EEMGEN, 0x00000001 | SEC_MOD_SEL);
		if (eemg_read(EEMGEN) != (0x00000001 | SEC_MOD_SEL)) {
#ifdef CONFIG_EEMG_AEE_RR_REC
			aee_kernel_warning("mt_eem",
			"@%s():%d, EEMGEN %x\n",
			__func__,
			__LINE__,
			eemg_read(EEMGEN));
#endif
			WARN_ON(eemg_read(EEMGEN) !=
				(0x00000001 | SEC_MOD_SEL));
		}
		eemg_debug("EEMGEN = 0x%x, EEMGINTEN = 0x%x\n",
				eemg_read(EEMGEN), eemg_read(EEMGINTEN));
		//dump_register_gpu();
		udelay(250); /* all banks' phase cannot be set without delay */
		break;

	case EEMG_PHASE_INIT02:
		/* check if DCVALUES is minus and set DCVOFFSETIN to zero */
//		if ((det->DCVOFFSETIN & 0x8000) || (eemg_devinfo.FT_PGM == 0))
			det->DCVOFFSETIN = 0;
		eemg_debug("EEMG_SET_PHASE02\n ");
		eemg_write(EEMGINTEN, 0x00005f01);

		eemg_write(EEMG_INIT2VALS,
			  ((det->AGEVOFFSETIN << 16) & 0xffff0000) |
			  (det->DCVOFFSETIN & 0xffff));

		/* enable EEM INIT measurement */
		eemg_write(EEMGEN, 0x00000005 | SEC_MOD_SEL);
		if (eemg_read(EEMGEN) != (0x00000005 | SEC_MOD_SEL)) {
#ifdef CONFIG_EEMG_AEE_RR_REC
			aee_kernel_warning("mt_eem",
			"@%s():%d, EEMGEN %x\n",
			__func__,
			__LINE__,
			eemg_read(EEMGEN));
#endif
			WARN_ON(eemg_read(EEMGEN) !=
				(0x00000005 | SEC_MOD_SEL));
		}

		//dump_register_gpu();
		udelay(200); /* all banks' phase cannot be set without delay */
		break;

	case EEMG_PHASE_MON:
		eemg_debug("EEMG_SET_PHASE_MON\n ");
		eemg_write(EEMGINTEN, 0x00FF0000);
		/* enable EEM monitor mode */
		eemg_write(EEMGEN, 0x00000002 | SEC_MOD_SEL);
		/* dump_register_gpu(); */
		break;

	default:
		WARN_ON(1); /*BUG()*/
		break;
	}
	/* mt_ptpgpu_unlock(&flags); */

	FUNC_EXIT(FUNC_LV_HELP);
}

int base_ops_get_temp_gpu(struct eemg_det *det)
{
#ifdef CONFIG_THERMAL
	enum thermal_bank_name ts_bank;

	if (det_to_id(det) == EEMG_DET_GPU)
		ts_bank = THERMAL_BANK4;
	else if (det_to_id(det) == EEMG_DET_GPU_HI)
		ts_bank = THERMAL_BANK4;
	else
		ts_bank = THERMAL_BANK4;
	return tscpu_get_temp_by_bank(ts_bank);
#else
	return 0;
#endif
}

int base_ops_get_volt_gpu(struct eemg_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	eemg_debug("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

int base_ops_set_volt_gpu(struct eemg_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	eemg_debug("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

void base_ops_restore_default_volt_gpu(struct eemg_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	eemg_debug("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_get_freq_table_gpu(struct eemg_det *det, unsigned int gpu_freq_base,
	unsigned int gpu_m_freq_base)
{
	FUNC_ENTER(FUNC_LV_HELP);

	det->freq_tbl[0] = 100;
	det->num_freq_tbl = 1;

	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_get_orig_volt_table_gpu(struct eemg_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);
}

static long long eemg_get_current_time_us(void)
{
	return ktime_to_us(ktime_get());
}

/*=============================================================
 * Global function definition
 *=============================================================
 */
static void mt_ptpgpu_lock(unsigned long *flags)
{
	spin_lock_irqsave(&eemg_spinlock, *flags);
	eemg_pTime_us = eemg_get_current_time_us();

}

static void mt_ptpgpu_unlock(unsigned long *flags)
{
	eemg_cTime_us = eemg_get_current_time_us();
	EEMG_IS_TOO_LONG();
	spin_unlock_irqrestore(&eemg_spinlock, *flags);
}

/*
 * timer for log
 */
static enum hrtimer_restart eemg_log_timer_func(struct hrtimer *timer)
{
	struct eemg_det *det;

	FUNC_ENTER(FUNC_LV_HELP);

	for_each_det(det) {
		/* get rid of redundent banks */
		if (det->features == 0)
			continue;
		eemg_debug("Timer Bk=%d (%d)(%d, %d, %d, %d, %d, %d, %d, %d)(0x%x)\n",
			det->ctrl_id,
			det->ops->get_temp_gpu(det),
			det->ops->pmic_2_volt_gpu(det, det->volt_tbl_pmic[0]),
			det->ops->pmic_2_volt_gpu(det, det->volt_tbl_pmic[1]),
			det->ops->pmic_2_volt_gpu(det, det->volt_tbl_pmic[2]),
			det->ops->pmic_2_volt_gpu(det, det->volt_tbl_pmic[3]),
			det->ops->pmic_2_volt_gpu(det, det->volt_tbl_pmic[4]),
			det->ops->pmic_2_volt_gpu(det, det->volt_tbl_pmic[5]),
			det->ops->pmic_2_volt_gpu(det, det->volt_tbl_pmic[6]),
			det->ops->pmic_2_volt_gpu(det, det->volt_tbl_pmic[7]),
			det->t250);
	}

	hrtimer_forward_now(timer, ns_to_ktime(LOG_INTERVAL));
	FUNC_EXIT(FUNC_LV_HELP);

	return HRTIMER_RESTART;
}

static void eemg_calculate_aging_margin(struct eemg_det *det,
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

static void eemg_save_final_volt_aee(struct eemg_det *ndet)
{
#ifdef CONFIG_EEMG_AEE_RR_REC
	int i;

	if (ndet == NULL)
		return;

	for (i = 0; i < NR_FREQ; i++) {
		switch (ndet->ctrl_id) {
		case EEMG_CTRL_GPU:
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
		default:
			break;
		}
	}
#endif
}

static void eemg_interpolate_mid_opp(struct eemg_det *ndet)
{
	int i;

	for (i = 1; i < gpu_vb_turn_pt; i++) {
		ndet->volt_tbl_pmic[i] = (unsigned int)(interpolate(
			ndet->freq_tbl[0], ndet->freq_tbl[gpu_vb_turn_pt],
			ndet->volt_tbl_pmic[0], ndet->volt_tbl_pmic[
			gpu_vb_turn_pt], ndet->freq_tbl[i]));
	}
}

static void get_volt_table_in_thread(struct eemg_det *det)
{
	unsigned int init2chk = 0;
	struct eemg_det *highdet;
	struct eemg_det *ndet = det;
	unsigned int i, verr = 0;
	int low_temp_offset = 0, rm_dvtfix_offset = 0;
	unsigned int t_clamp = 0;

	mutex_lock(det->loo_mutex);
	ndet = (det->loo_role == HIGH_BANK) ?
		id_to_eemg_det(det->loo_couple) : det;
	eemg_debug("@@! In %s\n", __func__);
	read_volt_from_VOP(det);
	/* Copy volt to volt_tbl_init2 */
	if (det->init2_done == 0) {
		gpu_cur_init02_flag |= BIT(det->ctrl_id);
		/* Receive init2 isr and update init2 volt_table */
		if (det->loo_role == HIGH_BANK) {
			memcpy(ndet->volt_tbl_init2, det->volt_tbl,
				sizeof(int) * det->turn_pt);
			memcpy(det->volt_tbl_init2, det->volt_tbl,
				sizeof(int) * det->turn_pt);
		} else if (det->loo_role == LOW_BANK)
			memcpy(&(det->volt_tbl_init2[det->turn_pt]),
			&(det->volt_tbl[det->turn_pt]), sizeof(int) *
			(det->num_freq_tbl - det->turn_pt));
		det->init2_done = 1;
	}

	/* Copy high bank volt to volt_tbl
	 * remap to L/B bank for update dvfs table, also copy
	 * high opp volt table
	 * Band HIGHL will update its volt table (opp0~7) to bank L
	 */
	if (det->loo_role == HIGH_BANK)
		memcpy(ndet->volt_tbl, det->volt_tbl, sizeof(int) *
		       det->turn_pt);

	for (i = 0; i < NR_FREQ; i++) {
#ifdef CONFIG_EEMG_AEE_RR_REC
		switch (ndet->ctrl_id) {
		case EEMG_CTRL_GPU:
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
		default:
			break;
		}
#endif

		if ((i > 0) && ((i % 4) != 0) &&
			(i != det->turn_pt) &&
			(ndet->volt_tbl[i] > ndet->volt_tbl[i-1])) {

			verr = 1;
#ifdef CONFIG_EEMG_AEE_RR_REC
			aee_kernel_warning("mt_eem",
				"@%s():%d; (%s) [%d] = [%x] > [%d] = [%x]\n",
				__func__, __LINE__, ((char *)(ndet->name) + 8),
				i, ndet->volt_tbl[i], i-1, ndet->volt_tbl[i-1]);

			aee_kernel_warning("mt_eem",
			"@%s():%d; (%s) V30_[0x%x], V74_[0x%x],",
				__func__, __LINE__, ((char *)(det->name) + 8),
				ndet->mon_vop30, ndet->mon_vop74);
#endif
			WARN_ON(ndet->volt_tbl[i] > ndet->volt_tbl[i-1]);
		}

		/*
		 *eemg_debug("mon_[%s].volt_tbl[%d] = 0x%X (%d)\n",
		 *	det->name, i, det->volt_tbl[i],
		 *	det->ops->pmic_2_volt_gpu(det, det->volt_tbl[i]));

		 *if (NR_FREQ > 8) {
		 *	eemg_isr_info("mon_[%s].volt_tbl[%d] = 0x%X (%d)\n",
		 *	det->name, i+1, det->volt_tbl[i+1],
		 *	det->ops->pmic_2_volt_gpu(det, det->volt_tbl[i+1]));
		 *}
		 */
	}

	if (verr == 1)
		memcpy(ndet->volt_tbl, ndet->volt_tbl_init2,
				sizeof(ndet->volt_tbl));

	/* Check Temperature */
	ndet->temp = ndet->ops->get_temp_gpu(det);


	if (ndet->temp <= EXTRA_LOW_TEMP_VAL)
		ndet->isTempInv = EEM_EXTRALOW_T;
	else if (ndet->temp <= LOW_TEMP_VAL)
		ndet->isTempInv = EEM_LOW_T;
	else if (ndet->temp >= HIGH_TEMP_VAL)
		ndet->isTempInv = EEM_HIGH_T;
	else
		ndet->isTempInv = 0;

	if (ndet->ctrl_id == EEMG_CTRL_GPU) {
		if ((ndet->isTempInv == EEM_EXTRALOW_T) ||
			(ndet->isTempInv == EEM_LOW_T)) {
			ndet->low_temp_off =
			(ndet->isTempInv == EEM_EXTRALOW_T) ?
			EXTRA_LOW_TEMP_OFF_GPU : LOW_TEMP_OFF_GPU;
			t_clamp = ndet->low_temp_off;
		} else
			t_clamp = 0;
	}

	/* scale of det->volt_offset must equal 10uV */
	/* if has record table, min with record table of each cpu */
	for (i = 0; i < ndet->num_freq_tbl; i++) {
		if (ndet->volt_policy) {
			if (i < det->turn_pt) {
				highdet = id_to_eemg_det(ndet->loo_couple);
				rm_dvtfix_offset = 0 - highdet->DVTFIXED;
			} else
				rm_dvtfix_offset = 0 - ndet->DVTFIXED;
		} else {
			if (i < det->turn_pt) {
				highdet = id_to_eemg_det(ndet->loo_couple);
				rm_dvtfix_offset = min(
				((highdet->DVTFIXED * highdet->freq_tbl[i]
				+ (100 - 1)) / 100),
				highdet->DVTFIXED) - highdet->DVTFIXED;
			} else
				rm_dvtfix_offset = min(
				((ndet->DVTFIXED * ndet->freq_tbl[i]
				+ (100 - 1)) / 100),
				ndet->DVTFIXED) - ndet->DVTFIXED;
		}

		/* Add low temp offset for each bank if temp inverse */
		if (ndet->isTempInv) {
			low_temp_offset =
			(ndet->isTempInv == EEM_HIGH_T) ?
			ndet->high_temp_off : ndet->low_temp_off;
		}

		switch (ndet->ctrl_id) {
		case EEMG_CTRL_GPU:
			if ((i == 0) && (gpu_vb != 0 && gpu_vb_turn_pt != 0))
				ndet->volt_tbl_pmic[i] = min((unsigned int)(
					clamp(ndet->ops->eemg_2_pmic(ndet,
					(ndet->ops->volt_2_eemg(ndet,
					gpu_vb_volt))),
					ndet->ops->eemg_2_pmic(ndet,
					ndet->VMIN), ndet->ops->eemg_2_pmic(
					ndet, VMAX_VAL_GPU)) + low_temp_offset
					), ndet->volt_tbl_orig[i] +
					ndet->volt_clamp + t_clamp);
			else
				ndet->volt_tbl_pmic[i] = min((unsigned int)(clamp(
					ndet->ops->eemg_2_pmic(ndet,
					(ndet->volt_tbl[i] + ndet->volt_offset +
					ndet->volt_aging[i]) + rm_dvtfix_offset),
					ndet->ops->eemg_2_pmic(ndet, ndet->VMIN),
					ndet->ops->eemg_2_pmic(ndet, VMAX_VAL_GPU)
					) + low_temp_offset), ndet->volt_tbl_orig[i] +
					ndet->volt_clamp + t_clamp);

			break;
		default:
			eemg_error("[eemg_set_eemg_volt] incorrect det :%s!!",
					ndet->name);
			break;
		}

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
	}

	if ((ndet->ctrl_id == EEMG_CTRL_GPU) && (gpu_vb != 0 && gpu_vb_turn_pt != 0))
		eemg_interpolate_mid_opp(ndet);

	eemg_save_final_volt_aee(ndet);

	if (ndet->set_volt_to_upower == 0) {
		if (((ndet->ctrl_id == EEMG_CTRL_GPU) &&
			(gpu_cur_init02_flag != gpu_final_init02_flag)))
			init2chk = 0;
		else
			init2chk = 1;
		eemg_error("cur_flag:0x%x, g_flag:0x%x\n",
			gpu_cur_init02_flag, gpu_final_init02_flag);
		/* only when set_volt_to_upower == 0,
		 * the volt will be apply to upower
		 */
		if (init2chk)
			ndet->set_volt_to_upower = 1;
	}

	if (0 == (ndet->disabled % 2) && ndet->set_volt_to_upower)
		ndet->ops->set_volt_gpu(ndet);
	mutex_unlock(ndet->loo_mutex);
}
/*
 * Thread for voltage setting
 */
static int eemg_volt_thread_handler(void *data)
{
	struct eemg_ctrl *ctrl = (struct eemg_ctrl *)data;
	struct eemg_det *det = id_to_eemg_det(ctrl->det_id);
#ifdef CONFIG_EEMG_AEE_RR_REC
	int temp = -1;
#endif

	FUNC_ENTER(FUNC_LV_HELP);
	do {
		eemg_debug("In thread handler\n");
		wait_event_interruptible(ctrl->wq, ctrl->volt_update);
		if ((ctrl->volt_update & EEMG_VOLT_UPDATE) &&
				det->ops->set_volt_gpu) {
			get_volt_table_in_thread(det);
#ifdef CONFIG_EEMG_AEE_RR_REC
			/* update set volt status for this bank */
			switch (det->ctrl_id) {
			case EEMG_CTRL_GPU:
				aee_rr_rec_ptp_status
					(aee_rr_curr_ptp_status() |
					(1 << EEMG_GPU_IS_SET_VOLT));
				temp = EEMG_GPU_IS_SET_VOLT;
				break;
			case EEMG_CTRL_GPU_HI:
				aee_rr_rec_ptp_status
					(aee_rr_curr_ptp_status() |
					(1 << EEMG_GPU_HI_IS_SET_VOLT));
				temp = EEMG_GPU_HI_IS_SET_VOLT;
				break;
			default:
				eemg_error
				("eemg_volt_handler :incorrect det id %d\n",
				 det->ctrl_id);
				break;
			}
#endif
		}
		if ((ctrl->volt_update & EEMG_VOLT_RESTORE) &&
			det->ops->restore_default_volt_gpu) {
			det->ops->restore_default_volt_gpu(det);
			ctrl->volt_update = EEMG_VOLT_NONE;
		}
		if (det->vop_check)
			ctrl->volt_update = EEMG_VOLT_NONE;
	} while (!kthread_should_stop());
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static void inherit_base_det(struct eemg_det *det)
{
	/*
	 * Inherit ops from eemg_det_base_ops if ops in det is NULL
	 */
	FUNC_ENTER(FUNC_LV_HELP);

	#define INIT_OP(ops, func)					\
		do {							\
			if (ops->func == NULL)				\
				ops->func = eemg_det_base_ops.func;	\
		} while (0)

	INIT_OP(det->ops, disable_gpu);
	INIT_OP(det->ops, disable_locked_gpu);
	INIT_OP(det->ops, switch_bank_gpu);
	INIT_OP(det->ops, init01_gpu);
	INIT_OP(det->ops, init02_gpu);
	INIT_OP(det->ops, mon_mode_gpu);
	INIT_OP(det->ops, get_status_gpu);
	INIT_OP(det->ops, dump_status_gpu);
	INIT_OP(det->ops, set_phase_gpu);
	INIT_OP(det->ops, get_temp_gpu);
	INIT_OP(det->ops, get_volt_gpu);
	INIT_OP(det->ops, set_volt_gpu);
	INIT_OP(det->ops, restore_default_volt_gpu);
	INIT_OP(det->ops, get_freq_table_gpu);
	INIT_OP(det->ops, volt_2_pmic_gpu);
	INIT_OP(det->ops, volt_2_eemg);
	INIT_OP(det->ops, pmic_2_volt_gpu);
	INIT_OP(det->ops, eemg_2_pmic);

	FUNC_EXIT(FUNC_LV_HELP);
}

static void eemg_init_ctrl(struct eemg_ctrl *ctrl)
{
	FUNC_ENTER(FUNC_LV_HELP);

	eemg_debug("In %s\n", __func__);
	if (1) {
		init_waitqueue_head(&ctrl->wq);
		ctrl->thread = kthread_run(eemg_volt_thread_handler,
				ctrl, ctrl->name);

		if (IS_ERR(ctrl->thread))
			eemg_error("Create %s thread failed: %ld\n",
					ctrl->name, PTR_ERR(ctrl->thread));
	}

	eemg_debug("End %s\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);
}

static void eemg_init_det(struct eemg_det *det, struct eemg_devinfo *devinfo,
	struct platform_device *pdev)
{
	enum eemg_det_id det_id = det_to_id(det);
	struct nvmem_device *nvmem_dev;
	struct device_node *node = pdev->dev.of_node, *np;
	const char *domain;
	int *val;
	int ret, efuse_offset = 0, efuse_mask = 0, efuse = 0;
	struct eemg_det *h_det, *l_det;

	FUNC_ENTER(FUNC_LV_HELP);
	inherit_base_det(det);
	eemg_debug("IN init_det %s\n", det->name);

	nvmem_dev = nvmem_device_get(&pdev->dev, "mtk_efuse");

	val = (int *)&eemg_devinfo;

	ret = of_property_read_string_index(node, "eemg_detectors", det_id,
		&domain);
	np = of_find_node_by_name(node, domain);
	if (np) {
		of_property_read_u32_index(np, "MDES", 0, &efuse_offset);
		of_property_read_u32_index(np, "MDES", 1, &efuse_mask);
		det->MDES      = (*(val + efuse_offset) & efuse_mask) >>
			find_first_bit((unsigned long *)&efuse_mask, 32);
		of_property_read_u32_index(np, "BDES", 0, &efuse_offset);
		of_property_read_u32_index(np, "BDES", 1, &efuse_mask);
		det->BDES       = (*(val + efuse_offset) & efuse_mask) >>
			find_first_bit((unsigned long *)&efuse_mask, 32);
		of_property_read_u32_index(np, "DCMDET", 0, &efuse_offset);
		of_property_read_u32_index(np, "DCMDET", 1, &efuse_mask);
		det->DCMDET       = (*(val + efuse_offset) & efuse_mask) >>
			find_first_bit((unsigned long *)&efuse_mask, 32);
		of_property_read_u32_index(np, "DCBDET", 0, &efuse_offset);
		of_property_read_u32_index(np, "DCBDET", 1, &efuse_mask);
		det->DCBDET      = (*(val + efuse_offset) & efuse_mask) >>
			find_first_bit((unsigned long *)&efuse_mask, 32);
		of_property_read_u32_index(np, "EEMINITEN", 0, &efuse_offset);
		of_property_read_u32_index(np, "EEMINITEN", 1, &efuse_mask);
		det->EEMINITEN = (*(val + efuse_offset) & efuse_mask) >>
			find_first_bit((unsigned long *)&efuse_mask, 32);
		of_property_read_u32_index(np, "EEMMONEN", 0, &efuse_offset);
		of_property_read_u32_index(np, "EEMMONEN", 1, &efuse_mask);
		det->EEMMONEN      = (*(val + efuse_offset) & efuse_mask) >>
			find_first_bit((unsigned long *)&efuse_mask, 32);
		of_property_read_u32_index(np, "MTDES", 0, &efuse_offset);
		of_property_read_u32_index(np, "MTDES", 1, &efuse_mask);
		det->MTDES      = (*(val + efuse_offset) & efuse_mask) >>
			find_first_bit((unsigned long *)&efuse_mask, 32);
		of_property_read_u32_index(np, "SPEC", 0, &efuse_offset);
		of_property_read_u32_index(np, "SPEC", 1, &efuse_mask);
		det->SPEC       = (*(val + efuse_offset) & efuse_mask) >>
			find_first_bit((unsigned long *)&efuse_mask, 32);
		ret = of_property_read_u32(np, "max_freq_khz",
			&det->max_freq_khz);
		ret = of_property_read_u32(np, "loo_role", &det->loo_role);
		ret = of_property_read_u32(np, "DVTFIXED", &det->DVTFIXED);
		of_property_read_u32_index(np, "VMIN", 0, &efuse_offset);
		of_property_read_u32_index(np, "VMIN", 1, &efuse_mask);
		nvmem_device_read(nvmem_dev, efuse_offset, sizeof(__u32),
			&efuse);
		of_property_read_u32_index(np, "VMIN-idx", (efuse  & efuse_mask)
			>> find_first_bit((unsigned long *)&efuse_mask, 32),
			&det->VMIN);
		ret = of_property_read_u32(np, "VMAX", &det->VMAX);
		det->VMAX	+= det->DVTFIXED;
	}

	switch (det_id) {
	case EEMG_DET_GPU:
		gpu_final_init02_flag |= BIT(det_id);
		break;
	case EEMG_DET_GPU_HI:
		break;
	default:
		eemg_debug("[%s]: Unknown det_id %d\n", __func__, det_id);
		break;
	}

	/* get DVFS frequency table */
	if (det->ops->get_freq_table_gpu) {
		h_det = (det->loo_role == HIGH_BANK) ? det:id_to_eemg_det(det->loo_couple);
		l_det = (det->loo_role == LOW_BANK) ? det:id_to_eemg_det(det->loo_couple);
		det->ops->get_freq_table_gpu(det, h_det->max_freq_khz, l_det->max_freq_khz);
	}

	if (gpu_2line && det_id == EEMG_DET_GPU_HI) {
		if (det->turn_pt != 0)
			gpu_final_init02_flag |= BIT(det_id);
		else
			det->features = 0;
	}

	eemg_debug("END init_det %s, turn_pt:%d\n",
		det->name, det->turn_pt);
	FUNC_EXIT(FUNC_LV_HELP);
}

static void eemg_set_eemg_volt(struct eemg_det *det)
{
#if SET_PMIC_VOLT
	struct eemg_ctrl *ctrl = id_to_eemg_ctrl(det->ctrl_id);

	FUNC_ENTER(FUNC_LV_HELP);
	ctrl->volt_update |= EEMG_VOLT_UPDATE;
	dsb(sy);
	eemg_debug("@@!In %s\n", __func__);
	wake_up_interruptible(&ctrl->wq);
#endif

	FUNC_EXIT(FUNC_LV_HELP);
}

static void eemg_restore_eemg_volt(struct eemg_det *det)
{
#if SET_PMIC_VOLT
	struct eemg_ctrl *ctrl = id_to_eemg_ctrl(det->ctrl_id);

	if (ctrl == NULL)
		return;

	ctrl->volt_update |= EEMG_VOLT_RESTORE;
	dsb(sy);
	wake_up_interruptible(&ctrl->wq);
#endif
}

static inline void handle_init01_isr(struct eemg_det *det)
{
	FUNC_ENTER(FUNC_LV_LOCAL);

	eemg_debug("mode = init1 %s-isr\n", ((char *)(det->name) + 8));

	det->dcvalues[EEMG_PHASE_INIT01] = eemg_read(EEMG_DCVALUES);
	det->freqpct30[EEMG_PHASE_INIT01] = eemg_read(EEMG_FREQPCT30);
	det->eemg_26c[EEMG_PHASE_INIT01] = eemg_read(EEMG_VDESIGN30);
	det->vop30[EEMG_PHASE_INIT01] = eemg_read(EEMG_VOP30);
	det->eemg_eemEn[EEMG_PHASE_INIT01] = eemg_read(EEMGEN);

#if DUMP_DATA_TO_DE
	{
		unsigned int i;

		for (i = 0; i < ARRAY_SIZE(reg_gpu_addr_off); i++) {
			det->reg_dump_data[i][EEMG_PHASE_INIT01] =
				eemg_read(EEMG_BASEADDR + reg_gpu_addr_off[i]);
			eemg_isr_info("0x%lx = 0x%08x\n",
			(unsigned long)EEMG_BASEADDR + reg_gpu_addr_off[i],
			det->reg_dump_data[i][EEMG_PHASE_INIT01]
			);
		}
	}
#endif
	/*
	 * Read & store 16 bit values EEMG_DCVALUES.DCVOFFSET and
	 * EEMG_AGEVALUES.AGEVOFFSET for later use in INIT2 procedure
	 */
	 /* hw bug, workaround */
	det->DCVOFFSETIN = ~(eemg_read(EEMG_DCVALUES) & 0xffff) + 1;

	det->AGEVOFFSETIN = eemg_read(EEMG_AGEVALUES) & 0xffff;

	/*
	 * Set EEMGEN.EEMINITEN/EEMGEN.EEMINIT2EN = 0x0 &
	 * Clear EEM INIT interrupt EEMGINTSTS = 0x00000001
	 */
	eemg_write(EEMGEN, 0x0 | SEC_MOD_SEL);
	eemg_write(EEMGINTSTS, 0x1);
	/* det->ops->init02_gpu(det); */

	eemg_debug("@@ END mode = init1 %s-isr\n", ((char *)(det->name) + 8));
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
		 *eemg_debug("y1(%d), y0(%d), x1(%d), x0(%d), ym(%d), ratio(%d),
		 *rtn(%d)\n",
		 *	y1, y0, x1, x0, ym, ratio, result);
		 */
	}
	return result;
}

static void eemg_fill_freq_table(struct eemg_det *det)
{
	int tmpfreq30 = 0, tmpfreq74 = 0;
	int i, j, shift_bits;
	int *setfreq;

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
			eemg_error("Fill 32 opp/n");
			tmpfreq30 = det->freq_tbl[0] & 0xFF;
			/* prepare last 7 freq, fill to FREQPCT31, FREQPCT74 */
			for (i = (det->num_freq_tbl - 7), j = 1;
				i < det->num_freq_tbl; i++, j++) {
				shift_bits = (j % 4) * 8;
				setfreq = (j < 4) ? &tmpfreq30 : &tmpfreq74;
				*setfreq |= (det->freq_tbl[i] << shift_bits);
			}
		}
	}
	eemg_write(EEMG_FREQPCT30, tmpfreq30);
	eemg_write(EEMG_FREQPCT74, tmpfreq74);
}

static void read_volt_from_VOP(struct eemg_det *det)
{
	int temp30, temp74, i, j;
	unsigned int step = NR_FREQ / 8;
	unsigned int read_init2 = 0;
	int ref_idx = ((det->num_freq_tbl + (step - 1)) / step) - 1;
	int readvop, shift_bits;
	struct eemg_det *couple_det;

	/* Check both high/low bank's voltage are ready */
	if (det->loo_role != 0) {
		couple_det = id_to_eemg_det(det->loo_couple);
		if ((couple_det->init2_done == 0) ||
			(couple_det->mon_vop30 == 0) ||
			(couple_det->mon_vop74 == 0))
			read_init2 = 1;
	}
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
	if (det->loo_role == HIGH_BANK) {
		if (det->turn_pt < 8) {
			for (i = 0; i < det->turn_pt; i++) {
				shift_bits = (i % 4) * 8;
				readvop = (i < 4) ? temp30 : temp74;
				det->volt_tbl[i] = (readvop >> shift_bits) & 0xff;
			}
		} else {
			det->volt_tbl[0] = (temp30 & 0xff);
			/* prepare last 7 opp */
			for (i = (det->turn_pt - 7), j = 1; i < det->turn_pt; i++, j++) {
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
			for (i = (det->num_freq_tbl - 7), j = 1; i < det->num_freq_tbl; i++, j++) {
				shift_bits = (j % 4) * 8;
				readvop = (j < 4) ? temp30 : temp74;
				det->volt_tbl[i] =
					(readvop >> shift_bits) & 0xff;
			}
			/* prepare middle opp */
			for (i = (det->turn_pt + 1); i < det->num_freq_tbl - 7; i++) {
				det->volt_tbl[i] = interpolate(
					det->freq_tbl[det->turn_pt],
					det->freq_tbl[det->num_freq_tbl - 7],
					det->volt_tbl[det->turn_pt],
					det->volt_tbl[det->num_freq_tbl - 7],
					det->freq_tbl[i]
				);
			}
		}
	} else {
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
	}
}

static inline void handle_init02_isr(struct eemg_det *det)
{
	unsigned int i;

	FUNC_ENTER(FUNC_LV_LOCAL);

	eemg_debug("mode = init2 %s-isr\n", ((char *)(det->name) + 8));

	det->dcvalues[EEMG_PHASE_INIT02] = eemg_read(EEMG_DCVALUES);
	det->freqpct30[EEMG_PHASE_INIT02] = eemg_read(EEMG_FREQPCT30);
	det->eemg_26c[EEMG_PHASE_INIT02] = eemg_read(EEMG_VDESIGN30);
	det->vop30[EEMG_PHASE_INIT02] = eemg_read(EEMG_VOP30);
	det->eemg_eemEn[EEMG_PHASE_INIT02] = eemg_read(EEMGEN);

#if DUMP_DATA_TO_DE
	for (i = 0; i < ARRAY_SIZE(reg_gpu_addr_off); i++) {
		det->reg_dump_data[i][EEMG_PHASE_INIT02] =
			eemg_read(EEMG_BASEADDR + reg_gpu_addr_off[i]);
		eemg_isr_info("0x%lx = 0x%08x\n",
			(unsigned long)EEMG_BASEADDR + reg_gpu_addr_off[i],
			det->reg_dump_data[i][EEMG_PHASE_INIT02]
		);
	}
#endif

	det->init2_vop30 = eemg_read(EEMG_VOP30);
	det->init2_vop74 = eemg_read(EEMG_VOP74);

	det->vop_check = 0;
	eemg_set_eemg_volt(det);
	/*
	 * Set EEMGEN.EEMINITEN/EEMGEN.EEMINIT2EN = 0x0 &
	 * Clear EEM INIT interrupt EEMGINTSTS = 0x00000001
	 */
	eemg_write(EEMGEN, 0x0 | SEC_MOD_SEL);
	eemg_write(EEMGINTSTS, 0x1);

	det->ops->mon_mode_gpu(det);
	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_init_err_isr(struct eemg_det *det)
{
	int i;
	/* int *val = (int *)&eemg_devinfo; */

	FUNC_ENTER(FUNC_LV_LOCAL);
	eemg_error("====================================================\n");
	eemg_error("BD/MD/DCB/DCM/MTD=0x%X,%X,%X,%X,%X\n",
		 det->BDES, det->MDES, det->DCBDET, det->DCMDET,
		 det->MTDES);
	eemg_error("DETWINDOW/VMIN/VMAX/DTHI/DTLO=0x%X,%X,%X,%X,%X\n",
		 det->DETWINDOW, det->VMIN, det->VMAX, det->DTHI,
		 det->DTLO);
	eemg_error("DETMAX/DVTFIXED/VCO/DCCONFIG/=0x%X,%X,%X,%X\n",
		 det->DETMAX, det->DVTFIXED, det->VCO,
		 det->DCCONFIG);
	eemg_error("DCVOFFSETIN/dcvalues0/1/turn_pt=0x%X,%X,%X,%X\n",
		 det->DCVOFFSETIN, det->dcvalues[0], det->dcvalues[1],
		 det->turn_pt);

	/* Depend on EFUSE location */
	for (i = 0; i < sizeof(struct eemg_devinfo) / sizeof(unsigned int);
		i++)
		eemg_error("M_HW_RES%d= 0x%08X\n", i, val[i]);

	eemg_error("EEM init err: EEMGEN(%p) = 0x%X, EEMGINTSTS(%p) = 0x%X\n",
			 EEMGEN, eemg_read(EEMGEN),
			 EEMGINTSTS, eemg_read(EEMGINTSTS));
	eemg_error("EEMG_SMSTATE0 (%p) = 0x%X\n",
			 EEMG_SMSTATE0, eemg_read(EEMG_SMSTATE0));
	eemg_error("EEMG_SMSTATE1 (%p) = 0x%X\n",
			 EEMG_SMSTATE1, eemg_read(EEMG_SMSTATE1));
	eemg_error("EEMG_DESCHAR (%p) = 0x%X,0x%X,0x%X\n",
		 EEMG_DESCHAR, eemg_read(EEMG_DESCHAR),
		 eemg_read(EEMG_TEMPCHAR), eemg_read(EEMG_DETCHAR));
	eemg_error("EEMG_DCCONFIG/LIMIT/CORESEL (%p) = 0x%X,0x%X,0x%X\n",
		 EEMG_DCCONFIG, eemg_read(EEMG_DCCONFIG),
		 eemg_read(EEMG_LIMITVALS), eemg_read(EEMGCORESEL));
	eemg_error("EEMG_DETWINDOW/INIT2/DC (%p) = 0x%X,0x%X,0x%X\n",
		 EEMG_DETWINDOW, eemg_read(EEMG_DETWINDOW),
		 eemg_read(EEMG_INIT2VALS), eemg_read(EEMG_DCVALUES));
	eemg_error("EEMG_CHKSHIFT/VDESIGN/VOP(%p)=0x%X,0x%X,0x%X,0x%X,0x%X\n",
		EEMG_CHKSHIFT, eemg_read(EEMG_CHKSHIFT),
		eemg_read(EEMG_VDESIGN30), eemg_read(EEMG_VDESIGN74),
		eemg_read(EEMG_VOP30), eemg_read(EEMG_VOP74));
	eemg_error("EEMG_FREQPCT/74/CE0/CE4 (%p) = 0x%X,0x%X,0x%X,0x%X\n",
		EEMG_FREQPCT30, eemg_read(EEMG_FREQPCT30),
		eemg_read(EEMG_FREQPCT74), eemg_read(EEMG_BASEADDR + 0xCE0),
		eemg_read(EEMG_BASEADDR + 0xCE4));


	eemg_error("====================================================\n");
#ifdef CONFIG_EEMG_AEE_RR_REC
	aee_kernel_warning("mt_eem", "@%s():%d, get_volt_gpu(%s) = 0x%08X\n",
		__func__,
		__LINE__,
		det->name,
		det->VBOOT);
#endif
	det->ops->disable_locked_gpu(det, BY_INIT_ERROR);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_mon_mode_isr(struct eemg_det *det)
{
	unsigned int i;
#ifdef CONFIG_THERMAL
#ifdef CONFIG_EEMG_AEE_RR_REC
	unsigned long long temp_long;
	unsigned long long temp_cur =
		(unsigned long long)aee_rr_curr_ptp_temp();
#endif
#endif

	FUNC_ENTER(FUNC_LV_LOCAL);

	eemg_debug("mode = mon %s-isr\n", ((char *)(det->name) + 8));

#ifdef CONFIG_EEMG_AEE_RR_REC
	switch (det->ctrl_id) {
	case EEMG_CTRL_GPU:
#ifdef CONFIG_THERMAL
#if defined(__LP64__) || defined(_LP64)
		temp_long = 0;
#else
		temp_long = 0;
#endif
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(temp_long <<
				(8 * EEMG_GPU_IS_SET_VOLT) |
			(temp_cur & ~((0xFFULL)
			<< (8 * EEMG_GPU_IS_SET_VOLT))));
		}
#endif
		break;
	case EEMG_CTRL_GPU_HI:
#ifdef CONFIG_THERMAL
#if defined(__LP64__) || defined(_LP64)
		temp_long = 0;
#else
		temp_long = 0;
#endif
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(temp_long <<
				(8 * EEMG_GPU_HI_IS_SET_VOLT) |
			(temp_cur & ~((0xFFULL)
			<< (8 * EEMG_GPU_HI_IS_SET_VOLT))));
		}
#endif
		break;
	default:
		break;
	}
#endif

	det->dcvalues[EEMG_PHASE_MON]	= eemg_read(EEMG_DCVALUES);
	det->freqpct30[EEMG_PHASE_MON]	= eemg_read(EEMG_FREQPCT30);
	det->eemg_26c[EEMG_PHASE_MON]	= eemg_read(EEMGINTEN + 0x10);
	det->vop30[EEMG_PHASE_MON]	= eemg_read(EEMG_VOP30);
	det->eemg_eemEn[EEMG_PHASE_MON]	= eemg_read(EEMGEN);

#if DUMP_DATA_TO_DE
	for (i = 0; i < ARRAY_SIZE(reg_gpu_addr_off); i++) {
		det->reg_dump_data[i][EEMG_PHASE_MON] =
			eemg_read(EEMG_BASEADDR + reg_gpu_addr_off[i]);
		eemg_isr_info("0x%lx = 0x%08x\n",
			(unsigned long)EEMG_BASEADDR + reg_gpu_addr_off[i],
			det->reg_dump_data[i][EEMG_PHASE_MON]
			);
	}
#endif

	det->mon_vop30	= eemg_read(EEMG_VOP30);
	det->mon_vop74	= eemg_read(EEMG_VOP74);
	det->vop_check = 0;

	/* check if thermal sensor init completed? */
	det->t250 = eemg_read(TEMPG);

	/*
	 * 0x64 mappint to 100 + 25 = 125C,
	 *   0xB2 mapping to 178 - 128 = 50, -50 + 25 = -25C
	 */
	if (((det->t250 & 0xff) > 0x64) && ((det->t250  & 0xff) < 0xB2)) {
		eemg_error("Bank %s Temperature > 125C or < -25C 0x%x\n",
				det->name, det->t250);
		goto out;
	}

	eemg_set_eemg_volt(det);

out:
	/* Clear EEM INIT interrupt EEMGINTSTS = 0x00ff0000 */
	eemg_write(EEMGINTSTS, 0x00ff0000);
	eemg_debug("finish mon isr\n");

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_mon_err_isr(struct eemg_det *det)
{
#if DUMP_DATA_TO_DE
	unsigned int i;
#endif

	FUNC_ENTER(FUNC_LV_LOCAL);

	/* EEM Monitor mode error handler */
	eemg_error("====================================================\n");
	eemg_error
	("EEM mon err: EEMGCORESEL(%p) = 0x%08X,EEMG_THERMINTST(%p) = 0x%08X,",
			EEMGCORESEL, eemg_read(EEMGCORESEL),
			EEMG_THERMINTST, eemg_read(EEMG_THERMINTST));
	eemg_error("EEM0DINTST(%p) = 0x%08X",
			EEMGODINTST, eemg_read(EEMGODINTST));
	eemg_error("EEMGINTSTSRAW(%p) = 0x%08X, EEMGINTEN(%p) = 0x%08X\n",
			EEMGINTSTSRAW, eemg_read(EEMGINTSTSRAW),
			EEMGINTEN, eemg_read(EEMGINTEN));
	eemg_error("====================================================\n");
	eemg_error("EEM mon err:EEMGEN(%p)=0x%08X, EEMGINTSTS(%p)=0x%08X\n",
			EEMGEN, eemg_read(EEMGEN),
			EEMGINTSTS, eemg_read(EEMGINTSTS));
	eemg_error("EEMG_SMSTATE0 (%p) = 0x%08X\n",
			EEMG_SMSTATE0, eemg_read(EEMG_SMSTATE0));
	eemg_error("EEMG_SMSTATE1 (%p) = 0x%08X\n",
			EEMG_SMSTATE1, eemg_read(EEMG_SMSTATE1));
	eemg_error("TEMP (%p) = 0x%08X\n",
			TEMPG, eemg_read(TEMPG));
	eemg_error("EEMG_TEMPMSR0 (%p) = 0x%08X\n",
			EEMG_TEMPMSR0, eemg_read(EEMG_TEMPMSR0));
	eemg_error("EEMG_TEMPMSR1 (%p) = 0x%08X\n",
			EEMG_TEMPMSR1, eemg_read(EEMG_TEMPMSR1));
	eemg_error("EEMG_TEMPMSR2 (%p) = 0x%08X\n",
			EEMG_TEMPMSR2, eemg_read(EEMG_TEMPMSR2));
	eemg_error("EEMG_TEMPMONCTL0 (%p) = 0x%08X\n",
			EEMG_TEMPMONCTL0, eemg_read(EEMG_TEMPMONCTL0));
	eemg_error("EEMG_TEMPMSRCTL1 (%p) = 0x%08X\n",
			EEMG_TEMPMSRCTL1, eemg_read(EEMG_TEMPMSRCTL1));
	eemg_error("====================================================\n");

#if DUMP_DATA_TO_DE
	for (i = 0; i < ARRAY_SIZE(reg_gpu_addr_off); i++) {
		det->reg_dump_data[i][EEMG_PHASE_MON] =
			eemg_read(EEMG_BASEADDR + reg_gpu_addr_off[i]);
		eemg_error("0x%lx = 0x%08x\n",
		(unsigned long)EEMG_BASEADDR + reg_gpu_addr_off[i],
		det->reg_dump_data[i][EEMG_PHASE_MON]
		);
	}
#endif

	eemg_error("====================================================\n");
	eemg_error("EEM mon err: EEMGCORESEL(%p) = 0x%08X,",
			 EEMGCORESEL, eemg_read(EEMGCORESEL));
	eemg_error(" EEMG_THERMINTST(%p) = 0x%08X, EEMGODINTST(%p) = 0x%08X",
			 EEMG_THERMINTST, eemg_read(EEMG_THERMINTST),
			 EEMGODINTST, eemg_read(EEMGODINTST));

	eemg_error(" EEMGINTSTSRAW(%p) = 0x%08X, EEMGINTEN(%p) = 0x%08X\n",
			 EEMGINTSTSRAW, eemg_read(EEMGINTSTSRAW),
			 EEMGINTEN, eemg_read(EEMGINTEN));
	eemg_error("====================================================\n");
#ifdef CONFIG_EEMG_AEE_RR_REC
	aee_kernel_warning("mt_eem",
	"@%s():%d,get_volt_gpu(%s)=0x%08X,EEMGEN(%p)=0x%08X,EEMGINTSTS(%p)=0x%08X\n",
		__func__, __LINE__,
		det->name, det->VBOOT,
		EEMGEN, eemg_read(EEMGEN),
		EEMGINTSTS, eemg_read(EEMGINTSTS));
#endif
	det->ops->disable_locked_gpu(det, BY_MON_ERROR);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void eemg_isr_handler(struct eemg_det *det)
{
	unsigned int eemintsts, eemen;

	FUNC_ENTER(FUNC_LV_LOCAL);

	eemintsts = eemg_read(EEMGINTSTS);
	eemen = eemg_read(EEMGEN);

	eemg_debug("Bk_# = %d %s-isr, 0x%X, 0x%X\n",
		det->ctrl_id, ((char *)(det->name) + 8), eemintsts, eemen);

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

static irqreturn_t eemg_isr(int irq, void *dev_id)
{
	unsigned long flags;
	struct eemg_det *det = NULL;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	/* mt_ptpgpu_lock(&flags); */
	for (i = 0; i < NR_EEMG_CTRL; i++) {
		mt_ptpgpu_lock(&flags);
		/* TODO: FIXME, it is better to link i @ struct eemg_det */
		if ((BIT(i) & eemg_read(EEMGODINTST))) {
			mt_ptpgpu_unlock(&flags);
			continue;
		}

		det = &eemg_detectors[i];
		det->ops->switch_bank_gpu(det, NR_EEMG_PHASE);

		/*mt_eemg_reg_dump_locked(); */

		eemg_isr_handler(det);
		mt_ptpgpu_unlock(&flags);
	}

	/* mt_ptpgpu_unlock(&flags); */

	FUNC_EXIT(FUNC_LV_MODULE);

	return IRQ_HANDLED;
}

void eemg_init02_gpu(const char *str)
{
	struct eemg_det *det;
	struct eemg_ctrl *ctrl;

	FUNC_ENTER(FUNC_LV_LOCAL);
	eemg_debug("%s called by [%s]\n", __func__, str);

	for_each_det_ctrl(det, ctrl) {
		if (HAS_FEATURE(det, FEA_INIT02)) {
			unsigned long flag;

			mt_ptpgpu_lock(&flag);
			det->init2_done = 0;
			det->ops->init02_gpu(det);
			mt_ptpgpu_unlock(&flag);
		}
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

void eemg_init01_gpu(void)
{
	FUNC_ENTER(FUNC_LV_LOCAL);
	eemg_init02_gpu(__func__);
	FUNC_EXIT(FUNC_LV_LOCAL);
}

#if EN_EEMGPU
#if SUPPORT_DCONFIG
static void eemg_dconfig_set_det(struct eemg_det *det, struct device_node *node)
{
	enum eemg_det_id det_id = det_to_id(det);
	struct eemg_det *highdet;
	int doe_initmon = 0xF, doe_clamp = 0;
	int doe_offset = 0xFF;
	int rc1 = 0, rc2 = 0, rc3 = 0;

	if (det_id > EEMG_DET_GPU)
		return;

	switch (det_id) {
	case EEMG_DET_GPU:
		rc1 = of_property_read_u32(node, "eemg-initmon-gpu",
			&doe_initmon);
		rc2 = of_property_read_u32(node, "eemg-clamp-gpu",
			&doe_clamp);
		rc3 = of_property_read_u32(node, "eemg-offset-gpu",
			&doe_offset);
		break;
	default:
		eemg_debug("[%s]: Unknown det_id %d\n", __func__, det_id);
		break;
	}

	if (!rc1) {
		if ((((doe_initmon >= 0x0) && (doe_initmon <= 0x3)) ||
			((doe_initmon >= 0x6) && (doe_initmon <= 0x7))) &&
			(det->features != doe_initmon)) {
			det->features = doe_initmon;
			eemg_error("[DCONFIG] feature modified by DT(0x%x)\n",
				doe_initmon);

			if (HAS_FEATURE(det, FEA_INIT01) == 0)
				final_init01_flag &= ~BIT(det_id);
			else
				final_init01_flag |= BIT(det_id);


			switch (det_id) {
			case EEMG_DET_GPU:
				/* Change GPU Low_bank & High_bank */
				highdet = id_to_eemg_det(EEMG_DET_GPU_HI);
				highdet->features = doe_initmon & 0x6;
				break;
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

static int eemg_probe(struct platform_device *pdev)
{
	int ret;
	const phandle *ph;
	struct eemg_det *det;
	struct eemg_ctrl *ctrl;
	struct device_node *node, *node_infra;

#if SUPPORT_DCONFIG
	int doe_status;
#endif

	FUNC_ENTER(FUNC_LV_MODULE);

	ret = get_devinfo(pdev);
	if (!ret)
		return ret;

	node = pdev->dev.of_node;
	if (!node) {
		eemg_error("get eemg device node err\n");
		return -ENODEV;
	}

#if SUPPORT_DCONFIG
	if (of_property_read_u32(node, "eemg-status",
		&doe_status) < 0) {
		eemg_debug("[DCONFIG] eemg-status read error!\n");
	} else {
		eemg_debug("[DCONFIG] success-> status:%d, EEMG_Enable:%d\n",
			doe_status, ctrl_EEMG_Enable);
		if (((doe_status == 1) || (doe_status == 0)) &&
			(ctrl_EEMG_Enable != doe_status)) {
			ctrl_EEMG_Enable = doe_status;
			eemg_error("[DCONFIG] status modified by DT(0x%x).\n",
				doe_status);
		}
	}
#endif

	if (ctrl_EEMG_Enable == 0) {
		eemg_error("ctrl_EEMG_Enable = 0\n");
		FUNC_EXIT(FUNC_LV_MODULE);
		return 0;
	}

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
	if (mt_gpufreq_not_ready()) {
		eemg_error("Check gpu status for EEMGPU\n");
		return EPROBE_DEFER;
	}
#endif
	/* Setup IO addresses */
	eemg_base = of_iomap(node, 0);
	eemg_debug("[EEMG] eemg_base = 0x%p\n", eemg_base);
	eemg_irq_number = irq_of_parse_and_map(node, 0);
	eemg_debug("[THERM_CTRL] eemg_irq_number=%d\n", eemg_irq_number);
	if (!eemg_irq_number) {
		eemg_error("[EEMG] get irqnr failed=0x%x\n", eemg_irq_number);
		return 0;
	}

	ph = of_get_property(node, "infracfg", NULL);
	if (!ph) {
		eemg_error("infracfg failed\n");
		return -ENODEV;
	}

	node_infra = of_find_node_by_phandle(be32_to_cpup(ph));
	if (!node_infra) {
		eemg_debug("infracfg Mapping Failed\n");
		return -ENODEV;
	}

	infra_base_gpu = of_iomap(node_infra, 0);
	if (!infra_base_gpu) {
		eemg_debug("infra_ao Map Failed\n");
		return -ENODEV;
	}

	create_procfs(pdev);

	/* set EEM IRQ */
	ret = request_irq(eemg_irq_number, eemg_isr,
			IRQF_TRIGGER_HIGH, "eemg", NULL);
	if (ret) {
		eemg_error("EEMG IRQ register failed (%d)\n", ret);
		WARN_ON(1);
	}
	eemg_debug("Set EEMG IRQ OK.\n");

#ifdef CONFIG_EEMG_AEE_RR_REC
	_mt_eemg_aee_init();
#endif

	for_each_ctrl(ctrl)
		eemg_init_ctrl(ctrl);

	eemg_debug("finish eemg_init_ctrl\n");

	/* for slow idle */
	for_each_det(det)
		eemg_init_det(det, &eemg_devinfo, pdev);


	final_init01_flag = EEMG_INIT01_FLAG;

	/* get original volt from cpu dvfs before init01*/
	for_each_det(det) {
		if (det->ops->get_orig_volt_table_gpu)
			det->ops->get_orig_volt_table_gpu(det);
	}

#if SUPPORT_DCONFIG
	for_each_det(det)
		eemg_dconfig_set_det(det, node);
#endif

	eemg_init01_gpu();

	eemg_debug("%s ok\n", __func__);
	FUNC_EXIT(FUNC_LV_MODULE);
	return 0;
}

static int eemg_suspend(void)
{
	struct eemg_det *det;
	unsigned long flag;

	if (ctrl_EEMG_Enable) {
		eemg_error("Start EEM suspend\n");
		mt_ptpgpu_lock(&flag);
		for_each_det(det) {
			det->ops->switch_bank_gpu(det, NR_EEMG_PHASE);
			eemg_write(EEMGEN, 0x0 | SEC_MOD_SEL);
			/* clear all pending EEM int & config EEMGINTEN */
			eemg_write(EEMGINTSTS, 0xffffffff);
		}
		mt_ptpgpu_unlock(&flag);
	}

	return 0;
}

static int eemg_resume(void)
{
	if (ctrl_EEMG_Enable) {
		eemg_error("Start EEM resume\n");
		/* Reset EEM */
		eemg_write(INFRA_EEMG_RST, (1 << 5));
		eemg_write(INFRA_EEMG_CLR, (1 << 5));
		eemg_init02_gpu(__func__);
	}
	return 0;
}

static const struct of_device_id mt_eemg_of_match[] = {
	{ .compatible = "mediatek,eemgpu_fsm", },
	{},
};

static struct platform_driver eemg_driver = {
	.remove	 = NULL,
	.shutdown   = NULL,
	.probe	  = eemg_probe,
	.suspend	= NULL,
	.resume	 = NULL,
	.driver	 = {
		.name   = "mt-eemgpu",
		.of_match_table = mt_eemg_of_match,
	},
};

int mt_eemg_opp_num(enum eemg_det_id id)
{
	struct eemg_det *det = id_to_eemg_det(id);

	FUNC_ENTER(FUNC_LV_API);
	if (det == NULL)
		return 0;

	FUNC_EXIT(FUNC_LV_API);

	return det->num_freq_tbl;
}
EXPORT_SYMBOL(mt_eemg_opp_num);

void mt_eemg_opp_freq(enum eemg_det_id id, unsigned int *freq)
{
	struct eemg_det *det = id_to_eemg_det(id);
	int i = 0;

	FUNC_ENTER(FUNC_LV_API);

	if (det == NULL)
		return;

	for (i = 0; i < det->num_freq_tbl; i++)
		freq[i] = det->freq_tbl[i];

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_eemg_opp_freq);

void mt_eemg_opp_status(enum eemg_det_id id, unsigned int *temp,
	unsigned int *volt)
{
	struct eemg_det *det = id_to_eemg_det(id);
	int i = 0;

	FUNC_ENTER(FUNC_LV_API);

#ifdef CONFIG_THERMAL
	if (id == EEMG_DET_GPU)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK4);
	else if (id == EEMG_DET_GPU_HI)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK4);
	else
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK4);
#else
		*temp = 0;
#endif

	if (det == NULL)
		return;

	for (i = 0; i < det->num_freq_tbl; i++)
		volt[i] = det->ops->pmic_2_volt_gpu(det, det->volt_tbl_pmic[i]);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_eemg_opp_status);

/***************************
 * return current EEM stauts
 ***************************
 */
int mt_eemg_status(enum eemg_det_id id)
{
	struct eemg_det *det = id_to_eemg_det(id);

	FUNC_ENTER(FUNC_LV_API);
	if (det == NULL)
		return 0;
	else if (det->ops == NULL)
		return 0;
	else if (det->ops->get_status_gpu == NULL)
		return 0;

	FUNC_EXIT(FUNC_LV_API);

	return det->ops->get_status_gpu(det);
}

/**
 * ===============================================
 * PROCFS interface for debugging
 * ===============================================
 */

/*
 * show current EEM stauts
 */
static int eemg_debug_proc_show(struct seq_file *m, void *v)
{
	struct eemg_det *det = (struct eemg_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	/* FIXME: EEMGEN sometimes is disabled temp */
	seq_printf(m, "[%s] %s (%d)\n",
		   ((char *)(det->name) + 8),
		   det->disabled ? "disabled" : "enable",
		   det->ops->get_status_gpu(det)
		   );

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * set EEM status by procfs interface
 */
static ssize_t eemg_debug_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	int enabled = 0;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eemg_det *det = (struct eemg_det *)PDE_DATA(file_inode(file));

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
	eemg_debug("in eem debug proc write 2~~~~~~~~\n");

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &enabled)) {
		ret = 0;

		eemg_debug("in eem debug proc write 3~~~~~~~~\n");
		if (enabled == 0)
			/* det->ops->enable(det, BY_PROCFS); */
			det->ops->disable_gpu(det, 0);
		else if (enabled == 1)
			det->ops->disable_gpu(det, BY_PROCFS);
	} else
		ret = -EINVAL;

out:
	eemg_debug("in eem debug proc write 4~~~~~~~~\n");
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

/*
 * show current aging margin
 */
static int eemg_setmargin_proc_show(struct seq_file *m, void *v)
{
	struct eemg_det *det = (struct eemg_det *)m->private;
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	/* FIXME: EEMGEN sometimes is disabled temp */
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
static ssize_t eemg_setmargin_proc_write(struct file *file,
			const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	int aging_val[2];
	int i = 0;
	int start_oft, end_oft;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eemg_det *det = (struct eemg_det *)PDE_DATA(file_inode(file));
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
			eemg_error("number of arguments > 3!\n");
			goto out;
		}

		if (kstrtoint(tok, 10, &aging_val[i])) {
			eemg_error("Invalid input: %s\n", tok);
			goto out;
		} else
			i++;
	}

	if (!strncmp(cmd_str, "aging", sizeof("aging"))) {
		start_oft = aging_val[0];
		end_oft = aging_val[1];
		eemg_calculate_aging_margin(det, start_oft, end_oft);

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

	eemg_set_eemg_volt(det);

out:
	eemg_debug("in eem debug proc write 4~~~~~~~~\n");
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
}

/*
 * show current EEM data
 */
void eemg_dump_reg_by_det(struct eemg_det *det, struct seq_file *m)
{
	unsigned int i, k;
#if DUMP_DATA_TO_DE
	unsigned int j;
#endif

	for (i = EEMG_PHASE_INIT01; i <= EEMG_PHASE_MON; i++) {
		seq_printf(m, "Bank_number = %d\n", det->ctrl_id);
		if (i < EEMG_PHASE_MON)
			seq_printf(m, "mode = init%d\n", i+1);
		else
			seq_puts(m, "mode = mon\n");
		if (eemg_log_en) {
			seq_printf(m, "0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
				det->dcvalues[i],
				det->freqpct30[i],
				det->eemg_26c[i],
				det->vop30[i],
				det->eemg_eemEn[i]
			);

			if (det->eemg_eemEn[i] == (0x5 | SEC_MOD_SEL)) {
				seq_printf(m, "EEMG_LOG: Bank_number = [%d] (%d) - (",
				det->ctrl_id, det->ops->get_temp_gpu(det));

				for (k = 0; k < det->num_freq_tbl - 1; k++)
					seq_printf(m, "%d, ",
					det->ops->pmic_2_volt_gpu(det,
					det->volt_tbl_pmic[k]));
				seq_printf(m, "%d) - (",
						det->ops->pmic_2_volt_gpu(det,
						det->volt_tbl_pmic[k]));

				for (k = 0; k < det->num_freq_tbl - 1; k++)
					seq_printf(m, "%d, ", det->freq_tbl[k]);
				seq_printf(m, "%d)\n", det->freq_tbl[k]);
			}
		} /* if (eemg_log_en) */
#if DUMP_DATA_TO_DE
		for (j = 0; j < ARRAY_SIZE(reg_gpu_addr_off); j++)
			seq_printf(m, "0x%08lx = 0x%08x\n",
			(unsigned long)EEMG_BASEADDR + reg_gpu_addr_off[j],
			det->reg_dump_data[j][i]
			);
#endif
	}
}

static int eemg_dump_proc_show(struct seq_file *m, void *v)
{
	int *val = (int *)&eemg_devinfo;
	struct eemg_det *det;
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	/* Depend on EFUSE location */
	for (i = 0; i < sizeof(struct eemg_devinfo) / sizeof(unsigned int);
		i++)
		seq_printf(m, "M_HW_RES%d\t= 0x%08X\n", i, val[i]);

	for_each_det(det) {
		eemg_dump_reg_by_det(det, m);
	}

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}

/*
 * show current voltage
 */
static int eemg_cur_volt_proc_show(struct seq_file *m, void *v)
{
	struct eemg_det *det = (struct eemg_det *)m->private;
	u32 rdata = 0, i;

	FUNC_ENTER(FUNC_LV_HELP);

	rdata = det->ops->get_volt_gpu(det);

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
			det->ops->pmic_2_volt_gpu(det, det->volt_tbl_pmic[i]));

		seq_printf(m, "policy:%d, isTempInv:%d\n",
		det->volt_policy, det->isTempInv);
	}
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * show current EEM status
 */
static int eemg_status_proc_show(struct seq_file *m, void *v)
{
	int i;
	struct eemg_det *det = (struct eemg_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	seq_printf(m, "bank = %d, feature:%d, T(%d) - (",
		   det->ctrl_id, det->features, det->ops->get_temp_gpu(det));
	for (i = 0; i < det->num_freq_tbl - 1; i++)
		seq_printf(m, "%d, ", det->ops->pmic_2_volt_gpu(det,
					det->volt_tbl_pmic[i]));
	seq_printf(m, "%d) - (",
			det->ops->pmic_2_volt_gpu(det, det->volt_tbl_pmic[i]));

	for (i = 0; i < det->num_freq_tbl - 1; i++)
		seq_printf(m, "%d, ", det->freq_tbl[i]);
	seq_printf(m, "%d)\n", det->freq_tbl[i]);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}
/*
 * set EEM log enable by procfs interface
 */

static int eemg_log_en_proc_show(struct seq_file *m, void *v)
{
	FUNC_ENTER(FUNC_LV_HELP);
	seq_printf(m, "%d\n", eemg_log_en);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static ssize_t eemg_log_en_proc_write(struct file *file,
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

	if (kstrtoint(buf, 10, &eemg_log_en)) {
		eemg_debug("bad argument!! Should be \"0\" or \"1\"\n");
		goto out;
	}

	ret = 0;

	switch (eemg_log_en) {
	case 0:
		eemg_debug("eem log disabled.\n");
		hrtimer_cancel(&eemg_log_timer);
		break;

	case 1:
		eemg_debug("eem log enabled.\n");
		hrtimer_start(&eemg_log_timer,
			ns_to_ktime(LOG_INTERVAL), HRTIMER_MODE_REL);
		break;

	default:
		eemg_debug("bad argument!! Should be \"0\" or \"1\"\n");
		ret = -EINVAL;
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

/*
 * show EEM offset
 */
static int eemg_offset_proc_show(struct seq_file *m, void *v)
{
	struct eemg_det *det = (struct eemg_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	seq_printf(m, "%d\n", det->volt_offset);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * set EEM offset by procfs
 */
static ssize_t eemg_offset_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	int offset = 0;
	struct eemg_det *det = (struct eemg_det *)PDE_DATA(file_inode(file));
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
		mt_ptpgpu_lock(&flags);
		eemg_error("[%s]\n", __func__);
		eemg_set_eemg_volt(det);
		mt_ptpgpu_unlock(&flags);
	} else {
		ret = -EINVAL;
		eemg_debug("bad argument_1!! argument should be \"0\"\n");
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
	static const struct proc_ops name ## _proc_fops = {	\
		.proc_open		= name ## _proc_open,			\
		.proc_read		= seq_read,				\
		.proc_lseek		= seq_lseek,				\
		.proc_release	= single_release,			\
		.proc_write		= name ## _proc_write,			\
	}

#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct proc_ops name ## _proc_fops = {	\
		.proc_open		= name ## _proc_open,			\
		.proc_read		= seq_read,				\
		.proc_lseek		= seq_lseek,				\
		.proc_release	= single_release,			\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(eemg_debug);
PROC_FOPS_RO(eemg_status);
PROC_FOPS_RO(eemg_cur_volt);
PROC_FOPS_RW(eemg_offset);
PROC_FOPS_RO(eemg_dump);
PROC_FOPS_RW(eemg_log_en);
PROC_FOPS_RW(eemg_setmargin);

//static int create_procfs(void)
static int create_procfs(struct platform_device *pdev)
{
	struct proc_dir_entry *eemg_dir = NULL;
	struct proc_dir_entry *det_dir = NULL;
	int i;
	struct eemg_det *det;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	struct pentry det_entries[] = {
		PROC_ENTRY(eemg_debug),
		PROC_ENTRY(eemg_status),
		PROC_ENTRY(eemg_cur_volt),
		PROC_ENTRY(eemg_offset),
		PROC_ENTRY(eemg_setmargin),
	};

	struct pentry eemg_entries[] = {
		PROC_ENTRY(eemg_dump),
		PROC_ENTRY(eemg_log_en),
	};

	FUNC_ENTER(FUNC_LV_HELP);

	/* create procfs root /proc/eem */
	eemg_dir = proc_mkdir("eemg", NULL);

	if (!eemg_dir) {
		eemg_error("[%s]: mkdir /proc/eemg failed\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	/* if ctrl_EEMG_Enable =1, and has efuse value,
	 * create other banks procfs
	 */
	if (ctrl_EEMG_Enable != 0 && eemg_checkEfuse == 1) {
		for (i = 0; i < ARRAY_SIZE(eemg_entries); i++) {
			if (!proc_create(eemg_entries[i].name, 0664,
					eemg_dir, eemg_entries[i].fops)) {
				eemg_error("[%s]: create /proc/eem/%s failed\n",
						__func__,
						eemg_entries[i].name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -3;
			}
		}

		for_each_det(det) {
			if (det->features == 0)
				continue;

			det_dir = proc_mkdir(det->name, eemg_dir);

			if (!det_dir) {
				eemg_debug("[%s]: mkdir /proc/eemg/%s failed\n"
						, __func__, det->name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -2;
			}

			for (i = 0; i < ARRAY_SIZE(det_entries); i++) {
				if (!proc_create_data(det_entries[i].name,
					0664,
					det_dir,
					det_entries[i].fops, det)) {
					eemg_debug
			("[%s]: create /proc/eemg/%s/%s failed\n", __func__,
			det->name, det_entries[i].name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -3;
				}
			}
		}

	} /* if (ctrl_EEMG_Enable != 0) */

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}

void eemg_set_pi_efuse(enum eemg_det_id id,
			unsigned int pi_efuse,
			unsigned int loo_enabled)
{
	struct eemg_det *det = id_to_eemg_det(id);

	if (!det)
		return;

	det->pi_loo_enabled = loo_enabled;
	det->pi_efuse = pi_efuse;
}

void eemg_set_pi_dvtfixed(enum eemg_det_id id, unsigned int pi_dvtfixed)
{
	struct eemg_det *det = id_to_eemg_det(id);

	if (!det)
		return;

	det->pi_dvtfixed = pi_dvtfixed;
}

static int eemg_pm_event(struct notifier_block *notifier,
	unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		eemg_suspend();
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		eemg_resume();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block eemg_pm_notifier_func = {
	.notifier_call = eemg_pm_event,
	.priority = 0,
};

/*
 * Module driver
 */
static int __init eemg_init(void)
{
	int err = 0;

	eemg_debug("[EEM] ctrl_EEMG_Enable=%d\n", ctrl_EEMG_Enable);

	/* move to eemg_probe */
	/* create_procfs(); */
//	FIX ME
//        if (eemg_checkEfuse == 1) {

	if (eemg_checkEfuse == 0) {
		eemg_error("eemg_checkEfuse = 0\n");
		FUNC_EXIT(FUNC_LV_MODULE);
		return 0;
	}

	/* init timer for log / volt */
	hrtimer_init(&eemg_log_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	eemg_log_timer.function = eemg_log_timer_func;

	/*
	 * reg platform device driver
	 */
	err = platform_driver_register(&eemg_driver);

	if (err) {
		eemg_debug("EEM driver callback register failed..\n");
		FUNC_EXIT(FUNC_LV_MODULE);
		return err;
	}

	err = register_pm_notifier(&eemg_pm_notifier_func);
	if (err) {
		eemg_error("Failed to register PM notifier.\n");
		return err;
	}

	return 0;
}

static void __exit eemg_exit(void)
{
	FUNC_ENTER(FUNC_LV_MODULE);
	eemg_error("eem de-initialization\n");
	FUNC_EXIT(FUNC_LV_MODULE);
}

//late_initcall(eemg_init); /* late_initcall */
module_init(eemg_init);
module_exit(eemg_exit);
#endif /* EN_EEM */

MODULE_DESCRIPTION("MediaTek EEM Driver v0.3");
MODULE_LICENSE("GPL");

#undef __MTK_EEMG_C__
