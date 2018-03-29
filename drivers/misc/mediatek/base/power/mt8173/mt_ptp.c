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

/* #define DEVINFO 1 */
struct devinfo {
	int sn;
	int M_HW_RES4;
	int M_HW_RES5;
	int M_HW_RES3;
	int M_HW_RES0;
	int M_HW_RES1;
	int M_HW_RES7;
	int M_HW_RES8;
	int M_HW_RES9;
	int M_HW_RES6;
	int core;
	int gpu;
	int sram2;
	int sram1;
#ifdef DEVINFO
} devinfo[] = {
	{
		1, 0x00000003, 0x00000000, 0x00000000, 0x0F0F0F0F,
		0xBABAB9B6, 0x32271E37, 0x252B1912, 0x434C854B,
	},
#endif
};
/**
 * @file    mt_ptp.c
 * @brief   Driver for PTP
 *
 */

/*=============================================================
 * Include files
 *=============================================================*/

/* system includes */
#include <linux/delay.h>
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
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clk-private.h>
#include <linux/clkdev.h>
#include <linux/uaccess.h>

#include "mt_cpufreq.h"
#include "mt_gpufreq.h"
#include <mach/mt_ptp.h>
#include <mach/mt_thermal.h>
#include <mach/mt_freqhopping.h>
#include <mt-plat/mt_pmic_wrap.h>
#include <mt-plat/aee.h>
#include <mt-plat/mt_chip.h>

/* TODO: FIXME #include "devinfo.h"*/

/* local includes */
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/regulator/consumer.h>

#if HAS_PTPOD_VCORE
struct regulator *reg_vcore;
#endif

/* Global variable */
volatile unsigned int ptp_data[3] = { 0, 0, 0 };

struct ptp_det;
struct ptp_ctrl;
static int ptp_log_en;
static int is_CA15L_DCBDET_overflow;
#if HAS_PTPOD_VCORE
static int vcore_original;
#endif
static unsigned long therm_ctrl_clk;
static unsigned long axi_sel_clk;


static void ptp_set_ptp_volt(struct ptp_det *det);
static void ptp_restore_ptp_volt(struct ptp_det *det);
static void ptp_init01_prepare(struct ptp_det *det);
static void ptp_init01_finish(struct ptp_det *det);

/*=============================================================
 * Macro definition
 *=============================================================*/
#define __MT_PTP_C__

/*
 * CONFIG (SW related)
 */
#define CONFIG_PTP_SHOWLOG	1
#define EN_ISR_LOG		(1)
#define PTP_GET_REAL_VAL	(1)	/* get val from efuse */
#define SET_PMIC_VOLT		(1)	/* apply PMIC voltage */
#define LOG_INTERVAL		(2LL * NSEC_PER_SEC)
#define NR_FREQ			8
/*
 * 100 us, This is the PTP Detector sampling time as represented in
 * cycles of bclk_ck during INIT. 52 MHz
 */
#define DETWINDOW_VAL		0xa28

/*
 * mili Volt to config value. voltage = 700mV + val * 6.25mV
 * val = (voltage - 700) / 6.25
 * @mV:	mili volt
 */
#define MV_TO_VAL(MV)		((((MV) - 700) * 100 + 625 - 1) / 625)
/* TODO: FIXME, refer to VOLT_TO_PMIC_VAL() */
#define VAL_TO_MV(VAL)		(((VAL) * 625) / 100 + 700)
/* TODO: FIXME, refer to PMIC_VAL_TO_VOLT() */

#define VMAX_VAL		MV_TO_VAL(1125)
#define VMIN_VAL		MV_TO_VAL(800)
#define VMIN_VAL_GPU		MV_TO_VAL(931)
#define VMIN_VAL_VCORE		MV_TO_VAL(931)
#define DTHI_VAL		0x01	/* positive */
#define DTLO_VAL		0xfe	/* negative (2's compliment) */
/* This timeout value is in cycles of bclk_ck. */
#define DETMAX_VAL		0xffff
#define AGECONFIG_VAL		0x555555	/* FIXME */
#define AGEM_VAL		0x0	/* FIXME */
#define DVTFIXED_VAL		0x6	/* FIXME */
#define VCO_VAL			0x10	/* FIXME */
#define DCCONFIG_VAL		0x555555	/* FIXME */

/*
 * bit operation
 */
#undef  BIT
#define BIT(bit)	(1U << (bit))

#define MSB(range)	(1 ? range)
#define LSB(range)	(0 ? range)
/**
 * Genearte a mask wher MSB to LSB are all 0b1
 * @r:	Range in the form of MSB:LSB
 */
#define BITMASK(r)	\
	(((unsigned) -1 >> (31 - MSB(r))) & ~((1U << LSB(r)) - 1))

/**
 * Set value at MSB:LSB. For example, BITS(7:3, 0x5A)
 * will return a value where bit 3 to bit 7 is 0x5A
 * @r:	Range in the form of MSB:LSB
 */
/* BITS(MSB:LSB, value) => Set value at MSB:LSB  */
#define BITS(r, val)	((val << LSB(r)) & BITMASK(r))

/*
 * LOG
 */
#define ptp_error(fmt, args...)		pr_err("[PTP] " fmt, ##args)
#define ptp_warning(fmt, args...)	pr_warn("[PTP] " fmt, ##args)
#define ptp_notice(fmt, args...)	pr_debug("[PTP] " fmt, ##args)
#define ptp_info(fmt, args...)		pr_debug("[PTP] " fmt, ##args)
#define ptp_debug(fmt, args...)		pr_debug("[PTP] " fmt, ##args)

#if EN_ISR_LOG
#define ptp_isr_info(fmt, args...)	ptp_notice(fmt, ##args)
#else
#define ptp_isr_info(fmt, args...)	ptp_debug(fmt, ##args)
#endif
/* module, platform driver interface */
#define FUNC_LV_MODULE			BIT(0)
/* cpufreq driver interface          */
#define FUNC_LV_CPUFREQ			BIT(1)
/* mt_cpufreq driver global function */
#define FUNC_LV_API			BIT(2)
/* mt_cpufreq driver lcaol function  */
#define FUNC_LV_LOCAL			BIT(3)
/* mt_cpufreq driver help function   */
#define FUNC_LV_HELP			BIT(4)

static unsigned int func_lv_mask;
/*
FUNC_LV_MODULE | FUNC_LV_CPUFREQ | FUNC_LV_API | FUNC_LV_LOCAL | FUNC_LV_HELP)
*/
#if defined(CONFIG_PTP_SHOWLOG)
#define FUNC_ENTER(lv) \
	do { \
		if ((lv) & func_lv_mask)\
			ptp_debug(">> %s()\n", __func__); \
	} while (0)
#define FUNC_EXIT(lv) \
	do { \
		if ((lv) & func_lv_mask) \
			ptp_debug("<< %s():%d\n", __func__, __LINE__); \
	} while (0)
#else
#define FUNC_ENTER(lv)
#define FUNC_EXIT(lv)
#endif

/*
 * REG ACCESS
 */

#define ptp_read(addr)		__raw_readl(addr)
#define ptp_read_field(addr, range)	\
	((ptp_read(addr) & BITMASK(range)) >> LSB(range))

#define ptp_write(addr, val)	mt_reg_sync_writel(val, addr)

/**
 * Write a field of a register.
 * @addr:	Address of the register
 * @range:	The field bit range in the form of MSB:LSB
 * @val:	The value to be written to the field
 */
#define ptp_write_field(addr, range, val)	\
	ptp_write(addr, (ptp_read(addr) & ~BITMASK(range)) | BITS(range, val))

/**
 * Helper macros
 */

/* PTP detector is disabled by who */
enum {
	BY_PROCFS = BIT(0),
	BY_INIT_ERROR = BIT(1),
	BY_MON_ERROR = BIT(2),
};

void __iomem *ptpod_base;
u32 ptpod_irq_number = 0;
int ptpod_phy_base;
/**
 * iterate over list of detectors
 * @det:	the detector * to use as a loop cursor.
 */
#define for_each_det(det) \
		for (det = ptp_detectors; \
		det < (ptp_detectors + ARRAY_SIZE(ptp_detectors)); \
		det++)

/**
 * iterate over list of detectors and its controller
 * @det:	the detector * to use as a loop cursor.
 * @ctrl:	the ptp_ctrl * to use as ctrl pointer of current det.
 */
#define for_each_det_ctrl(det, ctrl)				\
	for (det = ptp_detectors,				\
		ctrl = id_to_ptp_ctrl(det->ctrl_id);		\
		det < (ptp_detectors + ARRAY_SIZE(ptp_detectors)); \
		det++,						\
		ctrl = id_to_ptp_ctrl(det->ctrl_id))

/**
 * iterate over list of controllers
 * @pos:	the ptp_ctrl * to use as a loop cursor.
 */
#define for_each_ctrl(ctrl) \
		for (ctrl = ptp_ctrls; \
		ctrl < (ptp_ctrls + ARRAY_SIZE(ptp_ctrls)); \
		ctrl++)

/**
 * Given a ptp_det * in ptp_detectors. Return the id.
 * @det:	pointer to a ptp_det in ptp_detectors
 */
#define det_to_id(det)	((det) - &ptp_detectors[0])

/**
 * Given a ptp_ctrl * in ptp_ctrls. Return the id.
 * @det:	pointer to a ptp_ctrl in ptp_ctrls
 */
#define ctrl_to_id(ctrl)	((ctrl) - &ptp_ctrls[0])

/**
 * Check if a detector has a feature
 * @det:	pointer to a ptp_det to be check
 * @feature:	enum ptp_features to be checked
 */
#define HAS_FEATURE(det, feature)	((det)->features & feature)

#define PERCENT(numerator, denominator)	\
	(unsigned char)(((numerator) * 100 + (denominator) - 1) / (denominator))

/*=============================================================
 * Local type definition
 *=============================================================*/

/*
 * CONFIG (CHIP related)
 * PTPCORESEL.APBSEL
 */

typedef enum {
	PTP_PHASE_INIT01,
	PTP_PHASE_INIT02,
	PTP_PHASE_MON,

	NR_PTP_PHASE,
} ptp_phase;

enum {
	PTP_VOLT_NONE = 0,
	PTP_VOLT_UPDATE = BIT(0),
	PTP_VOLT_RESTORE = BIT(1),
};

struct ptp_ctrl {
	const char *name;
	ptp_det_id det_id;
	struct completion init_done;
	atomic_t in_init;
	/* for voltage setting thread */
	wait_queue_head_t wq;
	int volt_update;
	struct task_struct *thread;
};

struct ptp_det_ops {
	/* interface to PTP-OD */
	void (*enable)(struct ptp_det *det, int reason);
	void (*disable)(struct ptp_det *det, int reason);
	void (*disable_locked)(struct ptp_det *det, int reason);
	void (*switch_bank)(struct ptp_det *det);

	int (*init01)(struct ptp_det *det);
	int (*init02)(struct ptp_det *det);
	int (*mon_mode)(struct ptp_det *det);

	int (*get_status)(struct ptp_det *det);
	void (*dump_status)(struct ptp_det *det);

	void (*set_phase)(struct ptp_det *det, ptp_phase phase);

	/* interface to thermal */
	int (*get_temp)(struct ptp_det *det);

	/* interface to DVFS */
	int (*get_volt)(struct ptp_det *det);
	int (*set_volt)(struct ptp_det *det);
	void (*restore_default_volt)(struct ptp_det *det);
	void (*get_freq_table)(struct ptp_det *det);
};

enum ptp_features {
	FEA_INIT01 = BIT(PTP_PHASE_INIT01),
	FEA_INIT02 = BIT(PTP_PHASE_INIT02),
	FEA_MON = BIT(PTP_PHASE_MON),
};

struct ptp_det {
	const char *name;
	struct ptp_det_ops *ops;
	int status;		/* TODO: enable/disable */
	int features;		/* enum ptp_features */
	ptp_ctrl_id ctrl_id;

	/* devinfo */
	unsigned int PTPINITEN;
	unsigned int PTPMONEN;
	unsigned int MDES;
	unsigned int BDES;
	unsigned int DCMDET;
	unsigned int DCBDET;
	unsigned int AGEDELTA;
	unsigned int MTDES;

	/* constant */
	unsigned int DETWINDOW;
	unsigned int VMAX;
	unsigned int VMIN;
	unsigned int DTHI;
	unsigned int DTLO;
	unsigned int VBOOT;
	unsigned int DETMAX;
	unsigned int AGECONFIG;
	unsigned int AGEM;
	unsigned int DVTFIXED;
	unsigned int VCO;
	unsigned int DCCONFIG;

	/* Generated by PTP init01. Used in PTP init02 */
	unsigned int DCVOFFSETIN;
	unsigned int AGEVOFFSETIN;

	/* for debug */
	unsigned int dcvalues[NR_PTP_PHASE];

	unsigned int ptp_freqpct30[NR_PTP_PHASE];
	unsigned int ptp_26c[NR_PTP_PHASE];
	unsigned int ptp_vop30[NR_PTP_PHASE];
	unsigned int ptp_ptpen[NR_PTP_PHASE];

	/* slope */
	unsigned int MTS;
	unsigned int BTS;

	/* dvfs */
	/* could be got @ the same time with freq_tbl[] */
	unsigned int num_freq_tbl;
	/* maximum frequency used to calculate percentage */
	unsigned int max_freq_khz;
	/* percentage to maximum freq */
	unsigned char freq_tbl[NR_FREQ];

	unsigned int volt_tbl[NR_FREQ];
	unsigned int volt_tbl_init2[NR_FREQ];
	unsigned int volt_tbl_pmic[NR_FREQ];
	int volt_offset;
	/* Disabled by error or sysfs */
	int disabled;
};

struct ptp_devinfo {
	/* M_HW_RES0 */
	unsigned int CA15L_DCMDET:8;
	unsigned int CA7_DCMDET:8;
	unsigned int GPU_DCMDET:8;
	unsigned int SOC_DCMDET:8;
	/* M_HW_RES1 */
	unsigned int CA15L_DCBDET:8;
	unsigned int CA7_DCBDET:8;
	unsigned int GPU_DCBDET:8;
	unsigned int SOC_DCBDET:8;
	/* M_HW_RES2 */
	unsigned int CA15L_AGEDELTA:8;
	unsigned int CA7_AGEDELTA:8;
	unsigned int GPU_AGEDELTA:8;
	unsigned int SOC_AGEDELTA:8;
	/* M_HW_RES3 */
	unsigned int SOC_AO_DCBDET:8;
	unsigned int SOC_AO_DCMDET:8;
	unsigned int SOC_AO_BDES:8;
	unsigned int SOC_AO_MDES:8;
	/* M_HW_RES4 */
	unsigned int PTPINITEN:1;
	unsigned int PTPMONEN:1;
	unsigned int Bodybias:1;
	unsigned int PTPOD_T:1;
	unsigned int EPS:1;
	unsigned int M_HW_RES4_OTHERS:27;
	/* M_HW_RES5 */
	unsigned int M_HW_RES5:32;
	/* M_HW_RES6 */
	unsigned int M_HW_RES6:32;
	/* M_HW_RES7 */
	unsigned int CA15L_MDES:8;
	unsigned int CA7_MDES:8;
	unsigned int GPU_MDES:8;
	unsigned int SOC_MDES:8;
	/* M_HW_RES8 */
	unsigned int CA15L_BDES:8;
	unsigned int CA7_BDES:8;
	unsigned int GPU_BDES:8;
	unsigned int SOC_BDES:8;
	/* M_HW_RES9 */
	unsigned int CA15L_MTDES:8;
	unsigned int CA7_MTDES:8;
	unsigned int GPU_MTDES:8;
	unsigned int SOC_MTDES:8;
};

/*=============================================================
 *Local variable definition
 *=============================================================*/

/*
 * lock
 */
static DEFINE_SPINLOCK(ptp_spinlock);

/**
 * PTP controllers
 */
struct ptp_ctrl ptp_ctrls[NR_PTP_CTRL] = {
	[PTP_CTRL_LITTLE] = {
			.name = __stringify(PTP_CTRL_LITTLE),
			.det_id = PTP_DET_LITTLE,
			},

	[PTP_CTRL_BIG] = {
			.name = __stringify(PTP_CTRL_BIG),
			.det_id = PTP_DET_BIG,
			},

	[PTP_CTRL_GPU] = {
			.name = __stringify(PTP_CTRL_GPU),
			.det_id = PTP_DET_GPU,
			},
#if HAS_PTPOD_VCORE
	[PTP_CTRL_VCORE] = {
			.name = __stringify(PTP_CTRL_VCORE),
			.det_id = PTP_DET_VCORE_PDN,
			},
#endif
};

/*
 * PTP detectors
 */
static void base_ops_enable(struct ptp_det *det, int reason);
static void base_ops_disable(struct ptp_det *det, int reason);
static void base_ops_disable_locked(struct ptp_det *det, int reason);
static void base_ops_switch_bank(struct ptp_det *det);

static int base_ops_init01(struct ptp_det *det);
static int base_ops_init02(struct ptp_det *det);
static int base_ops_mon_mode(struct ptp_det *det);

static int base_ops_get_status(struct ptp_det *det);
static void base_ops_dump_status(struct ptp_det *det);

static void base_ops_set_phase(struct ptp_det *det, ptp_phase phase);
static int base_ops_get_temp(struct ptp_det *det);
static int base_ops_get_volt(struct ptp_det *det);
static int base_ops_set_volt(struct ptp_det *det);
static void base_ops_restore_default_volt(struct ptp_det *det);
static void base_ops_get_freq_table(struct ptp_det *det);

static int get_volt_cpu(struct ptp_det *det);
static int set_volt_cpu(struct ptp_det *det);
static void restore_default_volt_cpu(struct ptp_det *det);
static void get_freq_table_cpu(struct ptp_det *det);
#if HAS_PTPOD_VCORE
static void switch_to_vcore_pdn(struct ptp_det *det);
#endif
/* static int set_volt_vcore(struct ptp_det *det); */

static int get_volt_gpu(struct ptp_det *det);
static int set_volt_gpu(struct ptp_det *det);
static void restore_default_volt_gpu(struct ptp_det *det);
static void get_freq_table_gpu(struct ptp_det *det);

#if HAS_PTPOD_VCORE
static int get_volt_vcore(struct ptp_det *det);
#endif

#define BASE_OP(fn)	.fn = base_ops_ ## fn
static struct ptp_det_ops ptp_det_base_ops = {
	BASE_OP(enable),
	BASE_OP(disable),
	BASE_OP(disable_locked),
	BASE_OP(switch_bank),

	BASE_OP(init01),
	BASE_OP(init02),
	BASE_OP(mon_mode),

	BASE_OP(get_status),
	BASE_OP(dump_status),

	BASE_OP(set_phase),

	BASE_OP(get_temp),

	BASE_OP(get_volt),
	BASE_OP(set_volt),
	BASE_OP(restore_default_volt),
	BASE_OP(get_freq_table),
};

#if HAS_PTPOD_VCORE
static struct ptp_det_ops vcore_pdn_det_ops = {
	.get_volt = get_volt_vcore,
	/* FIXME: set_volt API */
	.switch_bank = switch_to_vcore_pdn,
	.set_volt = NULL, /* set_volt_vcore */
};
#endif

static struct ptp_det_ops big_det_ops = {
	.get_volt = get_volt_cpu,
	.set_volt = set_volt_cpu,
	.restore_default_volt = restore_default_volt_cpu,
	.get_freq_table = get_freq_table_cpu,
};

static struct ptp_det_ops little_det_ops = {
	.get_volt = get_volt_cpu,
	.set_volt = set_volt_cpu,
	.restore_default_volt = restore_default_volt_cpu,
	.get_freq_table = get_freq_table_cpu,
};

static struct ptp_det_ops gpu_det_ops = {
	.get_volt = get_volt_gpu,
	.set_volt = set_volt_gpu,	/* <-@@@ */
	.restore_default_volt = restore_default_volt_gpu,
	.get_freq_table = get_freq_table_gpu,
};

static struct ptp_det ptp_detectors[NR_PTP_DET] = {
	[PTP_DET_LITTLE] = {
		.name = __stringify(PTP_DET_LITTLE),
		.ops = &little_det_ops,
		.ctrl_id = PTP_CTRL_LITTLE,
		.features = FEA_INIT01 | FEA_INIT02 | FEA_MON,
		.max_freq_khz = 1600000,	/* TODO: FIXME */
		.VBOOT = MV_TO_VAL(1000),	/* 1.0v: 0x30 */
		/* .volt_offset  = 12, // <-@@@ */
		},

	[PTP_DET_BIG] = {
		.name = __stringify(PTP_DET_BIG),
		.ops = &big_det_ops,
		.ctrl_id = PTP_CTRL_BIG,
		.features = FEA_INIT01 | FEA_INIT02 | FEA_MON,
		.max_freq_khz = 2000000,	/* TODO: FIXME */
		.VBOOT = MV_TO_VAL(1000),	/* 1.0v: 0x30 */
		/* .volt_offset  = 12, // <-@@@ */
		},

	[PTP_DET_GPU] = {
		.name = __stringify(PTP_DET_GPU),
		.ops = &gpu_det_ops,
		.ctrl_id = PTP_CTRL_GPU,
		.features = FEA_INIT01 | FEA_INIT02 | FEA_MON,
		.max_freq_khz = 600000,	/* TODO: FIXME */
		.VBOOT = MV_TO_VAL(1000),	/* 1.0v: 0x30 */
		/* .volt_offset  = 12, // <-@@@ */
		},
#if HAS_PTPOD_VCORE
	[PTP_DET_VCORE_PDN] = {
		.name = __stringify(PTP_DET_VCORE_PDN),
		.ops = &vcore_pdn_det_ops,
		.ctrl_id = PTP_CTRL_VCORE,
		.features = FEA_INIT01 | FEA_INIT02,
		.VBOOT = MV_TO_VAL(1125),	/* 1.125v: 0x44 */
		},
#endif
};

static struct ptp_devinfo ptp_devinfo;

static unsigned int ptp_level;	/* debug info */

/**
 * timer for log
 */
static struct hrtimer ptp_log_timer;

/*=============================================================
 * Local function definition
 *=============================================================*/

static struct ptp_det *id_to_ptp_det(ptp_det_id id)
{
	if (likely(id < NR_PTP_DET))
		return &ptp_detectors[id];
	else
		return NULL;
}

static struct ptp_ctrl *id_to_ptp_ctrl(ptp_ctrl_id id)
{
	if (likely(id < NR_PTP_CTRL))
		return &ptp_ctrls[id];
	else
		return NULL;
}

static void base_ops_enable(struct ptp_det *det, int reason)
{
	/* FIXME: UNDER CONSTRUCTION */
	FUNC_ENTER(FUNC_LV_HELP);
	det->disabled &= ~reason;
	FUNC_EXIT(FUNC_LV_HELP);
}

static void base_ops_switch_bank(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	/* 3:0 = APBSEL*/
	ptp_write_field(PTP_PTPCORESEL, 3:0, det->ctrl_id);
	FUNC_EXIT(FUNC_LV_HELP);
}

static void base_ops_disable_locked(struct ptp_det *det, int reason)
{
	FUNC_ENTER(FUNC_LV_HELP);

	/* disable PTP */
	ptp_write(PTP_PTPEN, 0x0);

	/* Clear PTP interrupt PTPINTSTS */
	ptp_write(PTP_PTPINTSTS, 0x00ffffff);

	switch (reason) {
	case BY_MON_ERROR:
		/* set init2 value to DVFS table (PMIC) */
		memcpy(det->volt_tbl,
				det->volt_tbl_init2,
				sizeof(det->volt_tbl_init2));
		ptp_set_ptp_volt(det);
		break;

	case BY_INIT_ERROR:
	case BY_PROCFS:
	default:
		/* restore default DVFS table (PMIC) */
		ptp_restore_ptp_volt(det);
		break;
	}

	ptp_notice("Disable PTP-OD[%s] done.\n", det->name);
	det->disabled |= reason;

	FUNC_EXIT(FUNC_LV_HELP);
}

static void base_ops_disable(struct ptp_det *det, int reason)
{
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	mt_ptp_lock(&flags);
	det->ops->switch_bank(det);
	det->ops->disable_locked(det, reason);
	mt_ptp_unlock(&flags);

	FUNC_EXIT(FUNC_LV_HELP);
}

static int base_ops_init01(struct ptp_det *det)
{
	struct ptp_ctrl *ctrl = id_to_ptp_ctrl(det->ctrl_id);

	FUNC_ENTER(FUNC_LV_HELP);

	if (unlikely(!HAS_FEATURE(det, FEA_INIT01))) {
		ptp_notice("det %s has no INIT01\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	if (det->disabled & BY_PROCFS) {
		ptp_notice("[%s] Disabled by PROCFS\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}
	if (ptp_log_en)
		ptp_notice("%s(%s) start (ptp_level = 0x%08X).\n"
			, __func__, det->name, ptp_level);

	atomic_inc(&ctrl->in_init);
	ptp_init01_prepare(det);
	/* det->ops->dump_status(det); // <-@@@ */
	det->ops->set_phase(det, PTP_PHASE_INIT01);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static int base_ops_init02(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	if (unlikely(!HAS_FEATURE(det, FEA_INIT02))) {
		ptp_notice("det %s has no INIT02\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	if (det->disabled & BY_PROCFS) {
		ptp_notice("[%s] Disabled by PROCFS\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}

	if (ptp_log_en) {
		ptp_notice("%s(%s) start (ptp_level = 0x%08X).\n"
			, __func__, det->name, ptp_level);
		ptp_notice("DCVOFFSETIN = 0x%08X\n", det->DCVOFFSETIN);
		ptp_notice("AGEVOFFSETIN = 0x%08X\n", det->AGEVOFFSETIN);
	}

	/* det->ops->dump_status(det); // <-@@@ */
	det->ops->set_phase(det, PTP_PHASE_INIT02);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static int base_ops_mon_mode(struct ptp_det *det)
{
	struct TS_PTPOD ts_info;
	thermal_TS_name ts_name;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!HAS_FEATURE(det, FEA_MON)) {
		ptp_notice("det %s has no MON mode\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	if (det->disabled & BY_PROCFS) {
		ptp_notice("[%s] Disabled by PROCFS\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}

	if (ptp_log_en) {
		ptp_notice("%s(%s) start (ptp_level = 0x%08X).\n"
			, __func__, det->name, ptp_level);
		ptp_notice("PTPINITEN = 0x%08X, PTPMONEN = 0x%08X\n"
			, det->PTPINITEN, det->PTPMONEN);
	}

	ts_name = det->ctrl_id;	/* TODO: FIXME */
	get_thermal_slope_intercept(&ts_info, ts_name);
	det->MTS = ts_info.ts_MTS;
	det->BTS = ts_info.ts_BTS;

	if ((det->PTPINITEN == 0x0) || (det->PTPMONEN == 0x0)) {
		FUNC_EXIT(FUNC_LV_HELP);
		return 1;
	}

	/* det->ops->dump_status(det); */
	det->ops->set_phase(det, PTP_PHASE_MON);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static int base_ops_get_status(struct ptp_det *det)
{
	int status;
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	mt_ptp_lock(&flags);
	det->ops->switch_bank(det);
	status = (ptp_read(PTP_PTPEN) != 0) ? 1 : 0;
	mt_ptp_unlock(&flags);

	FUNC_EXIT(FUNC_LV_HELP);

	return status;
}

static void base_ops_dump_status(struct ptp_det *det)
{
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	ptp_isr_info("[%s]\n", det->name);

	ptp_isr_info("PTPINITEN = 0x%08X\n", det->PTPINITEN);
	ptp_isr_info("PTPMONEN = 0x%08X\n", det->PTPMONEN);
	ptp_isr_info("MDES = 0x%08X\n", det->MDES);
	ptp_isr_info("BDES = 0x%08X\n", det->BDES);
	ptp_isr_info("DCMDET = 0x%08X\n", det->DCMDET);

	ptp_isr_info("DCCONFIG = 0x%08X\n", det->DCCONFIG);
	ptp_isr_info("DCBDET = 0x%08X\n", det->DCBDET);

	ptp_isr_info("AGECONFIG = 0x%08X\n", det->AGECONFIG);
	ptp_isr_info("AGEM = 0x%08X\n", det->AGEM);

	ptp_isr_info("AGEDELTA = 0x%08X\n", det->AGEDELTA);
	ptp_isr_info("DVTFIXED = 0x%08X\n", det->DVTFIXED);
	ptp_isr_info("MTDES = 0x%08X\n", det->MTDES);
	ptp_isr_info("VCO = 0x%08X\n", det->VCO);

	ptp_isr_info("DETWINDOW = 0x%08X\n", det->DETWINDOW);
	ptp_isr_info("VMAX = 0x%08X\n", det->VMAX);
	ptp_isr_info("VMIN = 0x%08X\n", det->VMIN);
	ptp_isr_info("DTHI = 0x%08X\n", det->DTHI);
	ptp_isr_info("DTLO = 0x%08X\n", det->DTLO);
	ptp_isr_info("VBOOT = 0x%08X\n", det->VBOOT);
	ptp_isr_info("DETMAX = 0x%08X\n", det->DETMAX);

	ptp_isr_info("DCVOFFSETIN = 0x%08X\n", det->DCVOFFSETIN);
	ptp_isr_info("AGEVOFFSETIN = 0x%08X\n", det->AGEVOFFSETIN);

	ptp_isr_info("MTS = 0x%08X\n", det->MTS);
	ptp_isr_info("BTS = 0x%08X\n", det->BTS);

	ptp_isr_info("num_freq_tbl = %d\n", det->num_freq_tbl);

	for (i = 0; i < det->num_freq_tbl; i++)
		ptp_isr_info("freq_tbl[%d] = %d\n", i, det->freq_tbl[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		ptp_isr_info("volt_tbl[%d] = %d\n", i, det->volt_tbl[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		ptp_isr_info("volt_tbl_init2[%d] = %d\n",
					i, det->volt_tbl_init2[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		ptp_isr_info("volt_tbl_pmic[%d] = %d\n",
					i, det->volt_tbl_pmic[i]);

	FUNC_EXIT(FUNC_LV_HELP);
}

static void base_ops_set_phase(struct ptp_det *det, ptp_phase phase)
{
	unsigned int i, filter, val;
	/* unsigned long flags; // <-XXX */

	FUNC_ENTER(FUNC_LV_HELP);

	/* mt_ptp_lock(&flags); // <-XXX */

	det->ops->switch_bank(det);
	/* config PTP register */
	ptp_write(PTP_DESCHAR, ((det->BDES << 8) & 0xff00) |
			(det->MDES & 0xff));
	ptp_write(PTP_TEMPCHAR,
			(((det->VCO << 16) & 0xff0000) |
			((det->MTDES << 8) & 0xff00) |
			(det->DVTFIXED & 0xff)));
	ptp_write(PTP_DETCHAR, ((det->DCBDET << 8) & 0xff00) |
				(det->DCMDET & 0xff));
	ptp_write(PTP_AGECHAR, ((det->AGEDELTA << 8) & 0xff00) |
				(det->AGEM & 0xff));
	ptp_write(PTP_DCCONFIG, det->DCCONFIG);
	ptp_write(PTP_AGECONFIG, det->AGECONFIG);

	if (PTP_PHASE_MON == phase)
		ptp_write(PTP_TSCALCS, ((det->BTS << 12) & 0xfff000) |
					(det->MTS & 0xfff));

	if (det->AGEM == 0x0)
		ptp_write(PTP_RUNCONFIG, 0x80000000);
	else {
		val = 0x0;

		for (i = 0; i < 24; i += 2) {
			filter = 0x3 << i;

			if (((det->AGECONFIG) & filter) == 0x0)
				val |= (0x1 << i);
			else
				val |= ((det->AGECONFIG) & filter);
		}

		ptp_write(PTP_RUNCONFIG, val);
	}

	ptp_write(PTP_FREQPCT30,
			((det->freq_tbl[3] << 24) & 0xff000000) |
			((det->freq_tbl[2] << 16) & 0xff0000) |
			((det->freq_tbl[1] << 8) & 0xff00) |
			(det->freq_tbl[0] & 0xff));
	ptp_write(PTP_FREQPCT74,
			((det->freq_tbl[7] << 24) & 0xff000000) |
			((det->freq_tbl[6] << 16) & 0xff0000) |
			((det->freq_tbl[5] << 8) & 0xff00) |
			((det->freq_tbl[4]) & 0xff));
	ptp_write(PTP_LIMITVALS,
			((det->VMAX << 24) & 0xff000000) |
			((det->VMIN << 16) & 0xff0000) |
			((det->DTHI << 8) & 0xff00) | (det->DTLO & 0xff));
	/* ((det->VMIN << 16) & 0xff0000) | */
	ptp_write(PTP_VBOOT, (((det->VBOOT) & 0xff)));
	ptp_write(PTP_DETWINDOW, (((det->DETWINDOW) & 0xffff)));
	ptp_write(PTP_PTPCONFIG, (((det->DETMAX) & 0xffff)));

	/* clear all pending PTP interrupt & config PTPINTEN */
	ptp_write(PTP_PTPINTSTS, 0xffffffff);

	/* fix CA15L_DCBDET overflow */
	if (det_to_id(det) == PTP_DET_BIG && is_CA15L_DCBDET_overflow)
		ptp_write(PTP_CHKSHIFT, (ptp_read(PTP_CHKSHIFT) & ~0x0F) | 0x07); /* 0x07 = DCBDETOFF */

	switch (phase) {
	case PTP_PHASE_INIT01:
		ptp_write(PTP_PTPINTEN, 0x00005f01);
		/* enable PTP INIT measurement */
		ptp_write(PTP_PTPEN, 0x00000001);
		break;

	case PTP_PHASE_INIT02:
		ptp_write(PTP_PTPINTEN, 0x00005f01);
		ptp_write(PTP_INIT2VALS,
				((det->AGEVOFFSETIN << 16) & 0xffff0000) |
				(det->DCVOFFSETIN & 0xffff));
		/* enable PTP INIT measurement */
		ptp_write(PTP_PTPEN, 0x00000005);
		break;

	case PTP_PHASE_MON:
		ptp_write(PTP_PTPINTEN, 0x00FF0000);
		/* enable PTP monitor mode */
		ptp_write(PTP_PTPEN, 0x00000002);
		break;

	default:
		BUG();
		break;
	}

	/* mt_ptp_unlock(&flags); // <-XXX */

	FUNC_EXIT(FUNC_LV_HELP);
}

static int base_ops_get_temp(struct ptp_det *det)
{
	thermal_TS_name ts_name;

	FUNC_ENTER(FUNC_LV_HELP);

	ts_name = (det_to_id(det) == PTP_DET_LITTLE) ? THERMAL_CA7 :
			(det_to_id(det) == PTP_DET_BIG) ? THERMAL_CA15 :
			(det_to_id(det) == PTP_DET_GPU) ? THERMAL_GPU :
			THERMAL_CORE;

	FUNC_EXIT(FUNC_LV_HELP);

	return tscpu_get_bL_temp(ts_name);
}

static int base_ops_get_volt(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	ptp_notice("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static int base_ops_set_volt(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	ptp_notice("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static void base_ops_restore_default_volt(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	ptp_notice("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);
}

static void base_ops_get_freq_table(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	det->freq_tbl[0] = 100;
	det->num_freq_tbl = 1;

	FUNC_EXIT(FUNC_LV_HELP);
}

static int get_volt_cpu(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);

	return mt_cpufreq_cur_vproc(
			(det_to_id(det) == PTP_DET_LITTLE) ?
			MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG);
}

static int set_volt_cpu(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);

	return mt_cpufreq_voltage_set_by_ptpod(
					(det_to_id(det) == PTP_DET_LITTLE) ?
					MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG,
					det->volt_tbl_pmic, det->num_freq_tbl);
}

#if 0
static int set_volt_vcore(struct ptp_det *det)
{
	static int set_volt_vcore_times = 1;
	int mon_mode_vcore;

	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);

	/* TODO: Add global variable to get ptp-od phase. Then, you don't this to know it is init02 or monitor mode. */

	/* init02 mode */
	if (set_volt_vcore_times == 1) {
		ptp_error("Vcore voltage won't be tuned down except first time in monitor mode.\n");
	} else if (set_volt_vcore_times == 2) { /* monitor mode */
		mon_mode_vcore = VAL_TO_MV(det->volt_tbl_pmic[0]) * 1000;
		ptp_error("Tune Vcore down to %duV\n", mon_mode_vcore);
		regulator_set_voltage(reg_vcore, mon_mode_vcore, mon_mode_vcore);
		ptp_error("Vcore voltage = %dmV", det->ops->get_volt(det));

		det->ops->switch_bank(det);
		/* disable PTP VCORE */
		ptp_write(PTP_PTPEN, 0x0);
		/* Clear PTP VCORE interrupt PTPINTSTS */
		ptp_write(PTP_PTPINTSTS, 0x00ffffff);
	}

	set_volt_vcore_times++;
	return 0;
}
#endif

static void restore_default_volt_cpu(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	mt_cpufreq_return_default_DVS_by_ptpod(
		(det_to_id(det) == PTP_DET_LITTLE) ?
		MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG);

	FUNC_EXIT(FUNC_LV_HELP);
}

static void get_freq_table_cpu(struct ptp_det *det)
{
	int i;
	enum mt_cpu_dvfs_id cpu;

	FUNC_ENTER(FUNC_LV_HELP);

	cpu = (det_to_id(det) == PTP_DET_LITTLE) ?
			MT_CPU_DVFS_LITTLE : MT_CPU_DVFS_BIG;

#if 0
	if (cpu != 0)
		return;		/* TODO: FIXME, just for E1 */
#endif

	/* det->max_freq_khz = mt_cpufreq_max_frequency_by_DVS(cpu, 0);
	// XXX: defined @ ptp_detectors[] */

	for (i = 0; i < NR_FREQ; i++) {
		det->freq_tbl[i] = 100 - i*5;
		det->freq_tbl[i] =
			PERCENT(mt_cpufreq_max_frequency_by_DVS(cpu, i),
					det->max_freq_khz);
		if (0 == det->freq_tbl[i])
			break;
	}

	det->num_freq_tbl = i;

	FUNC_EXIT(FUNC_LV_HELP);
}

#if HAS_PTPOD_VCORE
static void switch_to_vcore_pdn(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	/* 3:0 = APBSEL*/
	ptp_write_field(PTP_PTPCORESEL, 3:0, det->ctrl_id);
	ptp_ctrls[PTP_CTRL_VCORE].det_id = det_to_id(det);

	FUNC_EXIT(FUNC_LV_HELP);
}
#endif
static int get_volt_gpu(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);

	return mt_gpufreq_get_cur_volt();	/* TODO: FIXME, unit = 10 uv */
}

static int set_volt_gpu(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);

	return mt_gpufreq_voltage_set_by_ptpod(det->volt_tbl_pmic, det->num_freq_tbl);
}

static void restore_default_volt_gpu(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	mt_gpufreq_return_default_DVS_by_ptpod();
	FUNC_EXIT(FUNC_LV_HELP);
}

static void get_freq_table_gpu(struct ptp_det *det)
{
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	for (i = 0; i < NR_FREQ; i++) {
		/* TODO: FIXME */
		det->freq_tbl[i] = PERCENT(mt_gpufreq_get_freq_by_idx(i), det->max_freq_khz);
		if (0 == det->freq_tbl[i])
			break;
	}

	FUNC_EXIT(FUNC_LV_HELP);

	det->num_freq_tbl = i;
}

#if HAS_PTPOD_VCORE
static int get_volt_vcore(struct ptp_det *det)
{
	return regulator_get_voltage(reg_vcore) / 1000;
}
#endif

/*=============================================================
 * Global function definition
 *=============================================================*/

unsigned int mt_ptp_get_level(void)
{
	unsigned int spd_bin_resv = 0, ret = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	spd_bin_resv = (get_devinfo_with_index(15) >> 28) & 0x7;

	switch (spd_bin_resv) {
	case 1:
		ret = 1;	/* 2.0G */
		break;

	case 2:
		ret = 2;	/* 1.3G */
		break;

	case 4:
		ret = 2;	/* 1.3G */
		break;

	default:
		ret = 0;	/* 1.7G */
		break;
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
}

#if 0				/* TODO: FIXME, remove it latter (unused) */
static unsigned int ptp_trasnfer_to_volt(unsigned int value)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);

	/* (700mv + n * 6.25mv) */
	return VAL_TO_MV(value);
}
#endif				/* TODO: FIXME, remove it latter (unused) */

void mt_ptp_lock(unsigned long *flags)
{
	/* FUNC_ENTER(FUNC_LV_HELP); */
	/* FIXME: lock with MD32 */
	/* get_md32_semaphore(SEMAPHORE_PTP); */
	spin_lock_irqsave(&ptp_spinlock, *flags);
	/* FUNC_EXIT(FUNC_LV_HELP); */
}
EXPORT_SYMBOL(mt_ptp_lock);

void mt_ptp_unlock(unsigned long *flags)
{
	/* FUNC_ENTER(FUNC_LV_HELP); */
	spin_unlock_irqrestore(&ptp_spinlock, *flags);
	/* FIXME: lock with MD32 */
	/* release_md32_semaphore(SEMAPHORE_PTP); */
	/* FUNC_EXIT(FUNC_LV_HELP); */
}
EXPORT_SYMBOL(mt_ptp_unlock);

int mt_ptp_idle_can_enter(void)
{
	struct ptp_ctrl *ctrl;

	FUNC_ENTER(FUNC_LV_HELP);

	for_each_ctrl(ctrl) {
		if (atomic_read(&ctrl->in_init)) {
			FUNC_EXIT(FUNC_LV_HELP);
			return 0;
		}
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return 1;
}
EXPORT_SYMBOL(mt_ptp_idle_can_enter);

/*
 * timer for log
 */
static enum hrtimer_restart ptp_log_timer_func(struct hrtimer *timer)
{
	struct ptp_det *det;

	FUNC_ENTER(FUNC_LV_HELP);

	for_each_det(det) {
		pr_notice(
		"PTP_LOG: [%s] (%d) - (%d, %d, %d, %d, %d, %d, %d, %d)-",
			det->name, det->ops->get_temp(det),
			VAL_TO_MV(det->volt_tbl_pmic[0]),
			VAL_TO_MV(det->volt_tbl_pmic[1]),
			VAL_TO_MV(det->volt_tbl_pmic[2]),
			VAL_TO_MV(det->volt_tbl_pmic[3]),
			VAL_TO_MV(det->volt_tbl_pmic[4]),
			VAL_TO_MV(det->volt_tbl_pmic[5]),
			VAL_TO_MV(det->volt_tbl_pmic[6]),
			VAL_TO_MV(det->volt_tbl_pmic[7]));
		pr_notice("(%d, %d, %d, %d, %d, %d, %d, %d)\n",
			det->freq_tbl[0], det->freq_tbl[1],
			det->freq_tbl[2], det->freq_tbl[3],
			det->freq_tbl[4], det->freq_tbl[5],
			det->freq_tbl[6], det->freq_tbl[7]);
	}

	hrtimer_forward_now(timer, ns_to_ktime(LOG_INTERVAL));
	FUNC_EXIT(FUNC_LV_HELP);

	return HRTIMER_RESTART;
}

/*
 * Thread for voltage setting
 */
static int ptp_volt_thread_handler(void *data)
{
	struct ptp_ctrl *ctrl = (struct ptp_ctrl *)data;
	struct ptp_det *det = id_to_ptp_det(ctrl->det_id);

	FUNC_ENTER(FUNC_LV_HELP);

	do {
		wait_event_interruptible(ctrl->wq, ctrl->volt_update);
#if 1
		if ((ctrl->volt_update & PTP_VOLT_UPDATE) && det->ops->set_volt)
			det->ops->set_volt(det);

		if ((ctrl->volt_update & PTP_VOLT_RESTORE) &&
			det->ops->restore_default_volt)
			det->ops->restore_default_volt(det);

		ctrl->volt_update = PTP_VOLT_NONE;
#endif
	} while (!kthread_should_stop());

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static void inherit_base_det(struct ptp_det *det)
{
	/*
	 * Inherit ops from ptp_det_base_ops if ops in det is NULL
	 */
	FUNC_ENTER(FUNC_LV_HELP);

#define INIT_OP(ops, func)					\
		do {							\
			if (ops->func == NULL)				\
				ops->func = ptp_det_base_ops.func;	\
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

	FUNC_EXIT(FUNC_LV_HELP);
}

static void ptp_init_ctrl(struct ptp_ctrl *ctrl)
{
	FUNC_ENTER(FUNC_LV_HELP);

	init_completion(&ctrl->init_done);
	atomic_set(&ctrl->in_init, 0);

	if (1) {
		/* HAS_FEATURE(id_to_ptp_det(ctrl->det_id), FEA_MON)
		// TODO: FIXME, why doesn't work <-XXX */
		init_waitqueue_head(&ctrl->wq);
		ctrl->thread = kthread_run(ptp_volt_thread_handler, ctrl,
						ctrl->name);

		if (IS_ERR(ctrl->thread))
			ptp_error("Create %s thread failed: %ld\n", ctrl->name,
			PTR_ERR(ctrl->thread));
	}

	FUNC_EXIT(FUNC_LV_HELP);
}

static void ptp_init_det(struct ptp_det *det, struct ptp_devinfo *devinfo)
{
	ptp_det_id det_id = det_to_id(det);

	FUNC_ENTER(FUNC_LV_HELP);

	inherit_base_det(det);

	/* init with devinfo */
	det->PTPINITEN = devinfo->PTPINITEN;
	det->PTPMONEN = devinfo->PTPMONEN;

	/* init with constant */
	det->DETWINDOW = DETWINDOW_VAL;
	det->VMAX = VMAX_VAL;
	det->VMIN = VMIN_VAL;

	det->DTHI = DTHI_VAL;
	det->DTLO = DTLO_VAL;
	det->DETMAX = DETMAX_VAL;

	det->AGECONFIG = AGECONFIG_VAL;
	det->AGEM = AGEM_VAL;
	det->DVTFIXED = DVTFIXED_VAL;
	det->VCO = VCO_VAL;
	det->DCCONFIG = DCCONFIG_VAL;

	if (NULL != det->ops->get_volt) {
		det->VBOOT = MV_TO_VAL(det->ops->get_volt(det));
		ptp_notice("[%s][%s]: det->VBOOT %d, %d\n",
			__func__, det->name, det->ops->get_volt(det),
			MV_TO_VAL(det->ops->get_volt(det)));
	}

	switch (det_id) {
	case PTP_DET_LITTLE:
		det->MDES = devinfo->CA7_MDES;
		det->BDES = devinfo->CA7_BDES;
		det->DCMDET = devinfo->CA7_DCMDET;
		det->DCBDET = devinfo->CA7_DCBDET;
		det->VMAX = MV_TO_VAL(1125);	/* override default setting */
		break;

	case PTP_DET_BIG:
		det->MDES = devinfo->CA15L_MDES;
		det->BDES = devinfo->CA15L_BDES;
		det->DCMDET = devinfo->CA15L_DCMDET;
		det->DCBDET = devinfo->CA15L_DCBDET;
		break;

	case PTP_DET_GPU:
		det->MDES = devinfo->GPU_MDES;
		det->BDES = devinfo->GPU_BDES;
		det->DCMDET = devinfo->GPU_DCMDET;
		det->DCBDET = devinfo->GPU_DCBDET;
		det->VMIN = VMIN_VAL_GPU;
		break;
#if HAS_PTPOD_VCORE
	case PTP_DET_VCORE_PDN:
		det->MDES = devinfo->SOC_MDES;
		det->BDES = devinfo->SOC_BDES;
		det->DCMDET = devinfo->SOC_DCMDET;
		det->DCBDET = devinfo->SOC_DCBDET;
		det->VMIN = VMIN_VAL_VCORE;
		/* det->VBOOT = XXX // TODO: FIXME */
#endif
		break;

	default:
		ptp_error("[%s]: Unknown det_id %d\n", __func__, det_id);
		break;
	}

	switch (det->ctrl_id) {
	case PTP_CTRL_LITTLE:
		det->AGEDELTA = devinfo->CA7_AGEDELTA;
		det->MTDES = devinfo->CA7_MTDES;
		break;

	case PTP_CTRL_BIG:
		det->AGEDELTA = devinfo->CA15L_AGEDELTA;
		det->MTDES = devinfo->CA15L_MTDES;
		break;

	case PTP_CTRL_GPU:
		det->AGEDELTA = devinfo->GPU_AGEDELTA;
		det->MTDES = devinfo->GPU_MTDES;
		break;
#if HAS_PTPOD_VCORE
	case PTP_CTRL_VCORE:
		det->AGEDELTA = devinfo->SOC_AGEDELTA;
		det->MTDES = devinfo->SOC_MTDES;
		break;
#endif
	default:
		ptp_error("[%s]: Unknown ctrl_id %d\n", __func__, det->ctrl_id);
		break;
	}

	/* get DVFS frequency table */
	det->ops->get_freq_table(det);

	FUNC_EXIT(FUNC_LV_HELP);
}

#define E1_SLOPE 0

static void ptp_set_ptp_volt(struct ptp_det *det)
{
#if SET_PMIC_VOLT
	int i, cur_temp, low_temp_offset;
	struct ptp_ctrl *ctrl = id_to_ptp_ctrl(det->ctrl_id);

#if E1_SLOPE
	unsigned int gpu_freq;

	for (i = 0; i < det->num_freq_tbl; i++)
		ptp_notice("before det %d %d\n", det->ctrl_id,
					det->volt_tbl[i]);

	i = det->ctrl_id;

	switch (i) {
	case PTP_CTRL_LITTLE:
	case PTP_CTRL_BIG:
		for (i = 1; i < det->num_freq_tbl; i++)
			det->volt_tbl[i] += (i + 1);
		break;
	case PTP_CTRL_GPU:
		for (i = 0; i < det->num_freq_tbl; i++) {

			gpu_freq = mt_gpufreq_get_freq_by_idx(i);

			switch (gpu_freq) {
			case GPU_DVFS_FREQ1:	/* 695500 */
#if 0
			case GPU_DVFS_FREQ1_1:	/* 650000 */
#endif
				break;
			case GPU_DVFS_FREQ2:	/* 598000 */
#if 0
			case GPU_DVFS_FREQ2_1:	/* 549250 */
#endif
				det->volt_tbl[i] += 2;
				break;
			case GPU_DVFS_FREQ3:	/* 494000 */
#if 0
			case GPU_DVFS_FREQ3_1:	/* 455000 */
#endif
				det->volt_tbl[i] += 6;
				break;
			case GPU_DVFS_FREQ4:	/* 396500 */
				det->volt_tbl[i] += 8;
				break;
			case GPU_DVFS_FREQ5:	/* 299000 */
				det->volt_tbl[i] += 8;
				break;
			case GPU_DVFS_FREQ6:	/* 253500 */
				det->volt_tbl[i] += 10;
				break;
			}
		}
		break;
	}

	for (i = 0; i < det->num_freq_tbl; i++)
		ptp_notice("after det %d %d\n", det->ctrl_id, det->volt_tbl[i]);

#endif
	cur_temp = det->ops->get_temp(det);

	if (ptp_log_en)
		ptp_isr_info("ptp_set_ptp_volt cur_temp = %d\n", cur_temp);

	if (cur_temp <= 33000)
		low_temp_offset = 10;
	else
		low_temp_offset = 0;

	for (i = 0; i < det->num_freq_tbl; i++) {
		det->volt_tbl_pmic[i] =
		clamp(det->volt_tbl[i] + det->volt_offset + low_temp_offset,
		det->VMIN, det->VMAX);
	}

	ctrl->volt_update |= PTP_VOLT_UPDATE;
	wake_up_interruptible(&ctrl->wq);
#endif

	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);
}

static void ptp_restore_ptp_volt(struct ptp_det *det)
{
#if SET_PMIC_VOLT
	struct ptp_ctrl *ctrl = id_to_ptp_ctrl(det->ctrl_id);

	ctrl->volt_update |= PTP_VOLT_RESTORE;
	wake_up_interruptible(&ctrl->wq);
#endif

	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);
}

static void mt_ptp_reg_dump(void)
{
	struct ptp_det *det;
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	ptp_isr_info("PTP_TEMPSPARE3(PTP_REVISIONID)	= 0x%08X\n", ptp_read(PTP_TEMPSPARE3));
	ptp_isr_info("PTP_TEMPMONCTL0	= 0x%08X\n", ptp_read(PTP_TEMPMONCTL0));
	ptp_isr_info("PTP_TEMPMONCTL1	= 0x%08X\n", ptp_read(PTP_TEMPMONCTL1));
	ptp_isr_info("PTP_TEMPMONCTL2	= 0x%08X\n", ptp_read(PTP_TEMPMONCTL2));
	ptp_isr_info("PTP_TEMPMONINT	= 0x%08X\n", ptp_read(PTP_TEMPMONINT));
	ptp_isr_info("PTP_TEMPMONINTSTS	= 0x%08X\n",
					ptp_read(PTP_TEMPMONINTSTS));
	ptp_isr_info("PTP_TEMPMONIDET0	= 0x%08X\n",
					ptp_read(PTP_TEMPMONIDET0));
	ptp_isr_info("PTP_TEMPMONIDET1	= 0x%08X\n",
					ptp_read(PTP_TEMPMONIDET1));
	ptp_isr_info("PTP_TEMPMONIDET2	= 0x%08X\n",
					ptp_read(PTP_TEMPMONIDET2));
	ptp_isr_info("PTP_TEMPH2NTHRE	= 0x%08X\n", ptp_read(PTP_TEMPH2NTHRE));
	ptp_isr_info("PTP_TEMPHTHRE	= 0x%08X\n", ptp_read(PTP_TEMPHTHRE));
	ptp_isr_info("PTP_TEMPCTHRE	= 0x%08X\n", ptp_read(PTP_TEMPCTHRE));
	ptp_isr_info("PTP_TEMPOFFSETH	= 0x%08X\n", ptp_read(PTP_TEMPOFFSETH));
	ptp_isr_info("PTP_TEMPOFFSETL	= 0x%08X\n", ptp_read(PTP_TEMPOFFSETL));
	ptp_isr_info("PTP_TEMPMSRCTL0	= 0x%08X\n", ptp_read(PTP_TEMPMSRCTL0));
	ptp_isr_info("PTP_TEMPMSRCTL1	= 0x%08X\n", ptp_read(PTP_TEMPMSRCTL1));
	ptp_isr_info("PTP_TEMPAHBPOLL	= 0x%08X\n", ptp_read(PTP_TEMPAHBPOLL));
	ptp_isr_info("PTP_TEMPAHBTO	= 0x%08X\n", ptp_read(PTP_TEMPAHBTO));
	ptp_isr_info("PTP_TEMPADCPNP0	= 0x%08X\n", ptp_read(PTP_TEMPADCPNP0));
	ptp_isr_info("PTP_TEMPADCPNP1	= 0x%08X\n", ptp_read(PTP_TEMPADCPNP1));
	ptp_isr_info("PTP_TEMPADCPNP2	= 0x%08X\n", ptp_read(PTP_TEMPADCPNP2));
	ptp_isr_info("PTP_TEMPADCMUX	= 0x%08X\n", ptp_read(PTP_TEMPADCMUX));
	ptp_isr_info("PTP_TEMPADCEXT	= 0x%08X\n", ptp_read(PTP_TEMPADCEXT));
	ptp_isr_info("PTP_TEMPADCEXT1	= 0x%08X\n", ptp_read(PTP_TEMPADCEXT1));
	ptp_isr_info("PTP_TEMPADCEN	= 0x%08X\n", ptp_read(PTP_TEMPADCEN));
	ptp_isr_info("PTP_TEMPPNPMUXADDR	= 0x%08X\n",
				ptp_read(PTP_TEMPPNPMUXADDR));
	ptp_isr_info("PTP_TEMPADCMUXADDR	= 0x%08X\n",
				ptp_read(PTP_TEMPADCMUXADDR));
	ptp_isr_info("PTP_TEMPADCEXTADDR	= 0x%08X\n",
				ptp_read(PTP_TEMPADCEXTADDR));
	ptp_isr_info("PTP_TEMPADCEXT1ADDR	= 0x%08X\n",
				ptp_read(PTP_TEMPADCEXT1ADDR));
	ptp_isr_info("PTP_TEMPADCENADDR	= 0x%08X\n",
				ptp_read(PTP_TEMPADCENADDR));
	ptp_isr_info("PTP_TEMPADCVALIDADDR	= 0x%08X\n",
				ptp_read(PTP_TEMPADCVALIDADDR));
	ptp_isr_info("PTP_TEMPADCVOLTADDR	= 0x%08X\n",
				ptp_read(PTP_TEMPADCVOLTADDR));
	ptp_isr_info("PTP_TEMPRDCTRL	= 0x%08X\n", ptp_read(PTP_TEMPRDCTRL));
	ptp_isr_info("PTP_TEMPADCVALIDMASK	= 0x%08X\n",
				ptp_read(PTP_TEMPADCVALIDMASK));
	ptp_isr_info("PTP_TEMPADCVOLTAGESHIFT	= 0x%08X\n",
				ptp_read(PTP_TEMPADCVOLTAGESHIFT));
	ptp_isr_info("PTP_TEMPADCWRITECTRL	= 0x%08X\n",
				ptp_read(PTP_TEMPADCWRITECTRL));
	ptp_isr_info("PTP_TEMPMSR0	= 0x%08X\n", ptp_read(PTP_TEMPMSR0));
	ptp_isr_info("PTP_TEMPMSR1	= 0x%08X\n", ptp_read(PTP_TEMPMSR1));
	ptp_isr_info("PTP_TEMPMSR2	= 0x%08X\n", ptp_read(PTP_TEMPMSR2));
	ptp_isr_info("PTP_TEMPIMMD0	= 0x%08X\n", ptp_read(PTP_TEMPIMMD0));
	ptp_isr_info("PTP_TEMPIMMD1	= 0x%08X\n", ptp_read(PTP_TEMPIMMD1));
	ptp_isr_info("PTP_TEMPIMMD2	= 0x%08X\n", ptp_read(PTP_TEMPIMMD2));
	ptp_isr_info("PTP_TEMPMONIDET3	= 0x%08X\n", ptp_read(PTP_TEMPMONIDET3));
	ptp_isr_info("PTP_TEMPADCPNP3	= 0x%08X\n", ptp_read(PTP_TEMPADCPNP3));
	ptp_isr_info("PTP_TEMPMSR3	= 0x%08X\n", ptp_read(PTP_TEMPMSR3));
	ptp_isr_info("PTP_TEMPIMMD3	= 0x%08X\n", ptp_read(PTP_TEMPIMMD3));
	ptp_isr_info("PTP_TEMPPROTCTL	= 0x%08X\n", ptp_read(PTP_TEMPPROTCTL));
	ptp_isr_info("PTP_TEMPPROTTA	= 0x%08X\n", ptp_read(PTP_TEMPPROTTA));
	ptp_isr_info("PTP_TEMPPROTTB	= 0x%08X\n", ptp_read(PTP_TEMPPROTTB));
	ptp_isr_info("PTP_TEMPPROTTC	= 0x%08X\n", ptp_read(PTP_TEMPPROTTC));
	ptp_isr_info("PTP_TEMPSPARE0	= 0x%08X\n", ptp_read(PTP_TEMPSPARE0));
	ptp_isr_info("PTP_TEMPSPARE1	= 0x%08X\n", ptp_read(PTP_TEMPSPARE1));
	ptp_isr_info("PTP_TEMPSPARE2	= 0x%08X\n", ptp_read(PTP_TEMPSPARE2));

	for_each_det(det) {
		mt_ptp_lock(&flags);
		det->ops->switch_bank(det);

		ptp_isr_info("PTP_DESCHAR[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_DESCHAR));
		ptp_isr_info("PTP_TEMPCHAR[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_TEMPCHAR));
		ptp_isr_info("PTP_DETCHAR[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_DETCHAR));
		ptp_isr_info("PTP_AGECHAR[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_AGECHAR));
		ptp_isr_info("PTP_DCCONFIG[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_DCCONFIG));
		ptp_isr_info("PTP_AGECONFIG[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_AGECONFIG));
		ptp_isr_info("PTP_FREQPCT30[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_FREQPCT30));
		ptp_isr_info("PTP_FREQPCT74[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_FREQPCT74));
		ptp_isr_info("PTP_LIMITVALS[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_LIMITVALS));
		ptp_isr_info("PTP_VBOOT[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_VBOOT));
		ptp_isr_info("PTP_DETWINDOW[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_DETWINDOW));
		ptp_isr_info("PTP_PTPCONFIG[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_PTPCONFIG));
		ptp_isr_info("PTP_TSCALCS[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_TSCALCS));
		ptp_isr_info("PTP_RUNCONFIG[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_RUNCONFIG));
		ptp_isr_info("PTP_PTPEN[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_PTPEN));
		ptp_isr_info("PTP_INIT2VALS[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_INIT2VALS));
		ptp_isr_info("PTP_DCVALUES[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_DCVALUES));
		ptp_isr_info("PTP_AGEVALUES[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_AGEVALUES));
		ptp_isr_info("PTP_CHKSHIFT[%s] = 0x%08X\n",
					det->name, ptp_read(PTP_CHKSHIFT));
		ptp_isr_info("PTP_VOP30[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_VOP30));
		ptp_isr_info("PTP_VOP74[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_VOP74));
		ptp_isr_info("PTP_TEMP[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_TEMP));
		ptp_isr_info("PTP_PTPINTSTS[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_PTPINTSTS));
		ptp_isr_info("PTP_PTPINTSTSRAW[%s]	= 0x%08X\n", det->name,
					ptp_read(PTP_PTPINTSTSRAW));
		ptp_isr_info("PTP_PTPINTEN[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_PTPINTEN));
		ptp_isr_info("PTP_SMSTATE0[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_SMSTATE0));
		ptp_isr_info("PTP_SMSTATE1[%s]	= 0x%08X\n",
					det->name, ptp_read(PTP_SMSTATE1));

		mt_ptp_unlock(&flags);
	}

	ptp_isr_info("PTP_PTPCORESEL	= 0x%08X\n", ptp_read(PTP_PTPCORESEL));
	ptp_isr_info("PTP_THERMINTST	= 0x%08X\n", ptp_read(PTP_THERMINTST));
	ptp_isr_info("PTP_PTPODINTST	= 0x%08X\n", ptp_read(PTP_PTPODINTST));
	ptp_isr_info("PTP_THSTAGE0ST	= 0x%08X\n", ptp_read(PTP_THSTAGE0ST));
	ptp_isr_info("PTP_THSTAGE1ST	= 0x%08X\n", ptp_read(PTP_THSTAGE1ST));
	ptp_isr_info("PTP_THSTAGE2ST	= 0x%08X\n", ptp_read(PTP_THSTAGE2ST));
	ptp_isr_info("PTP_THAHBST0	= 0x%08X\n", ptp_read(PTP_THAHBST0));
	ptp_isr_info("PTP_THAHBST1	= 0x%08X\n", ptp_read(PTP_THAHBST1));
	ptp_isr_info("PTP_PTPSPARE0	= 0x%08X\n", ptp_read(PTP_PTPSPARE0));
	ptp_isr_info("PTP_PTPSPARE1	= 0x%08X\n", ptp_read(PTP_PTPSPARE1));
	ptp_isr_info("PTP_PTPSPARE2	= 0x%08X\n", ptp_read(PTP_PTPSPARE2));
	ptp_isr_info("PTP_PTPSPARE3	= 0x%08X\n", ptp_read(PTP_PTPSPARE3));
	ptp_isr_info("PTP_THSLPEVEB	= 0x%08X\n", ptp_read(PTP_THSLPEVEB));

	FUNC_EXIT(FUNC_LV_HELP);
}

static inline void handle_init01_isr(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_LOCAL);
	if (ptp_log_en) {
		ptp_isr_info("@ %s(%s)\n", __func__, det->name);
		ptp_isr_info("ca7_temp=%d, c17_temp=%d gpu_temp=%d\n",
			tscpu_get_bL_temp(THERMAL_CA7),
			tscpu_get_bL_temp(THERMAL_CA15),
			tscpu_get_bL_temp(THERMAL_GPU));
	}

	det->dcvalues[PTP_PHASE_INIT01] = ptp_read(PTP_DCVALUES);
	det->ptp_freqpct30[PTP_PHASE_INIT01] = ptp_read(PTP_FREQPCT30);
	det->ptp_26c[PTP_PHASE_INIT01] = ptp_read(PTP_PTPINTEN + 0x10);
	det->ptp_vop30[PTP_PHASE_INIT01] = ptp_read(PTP_VOP30);
	det->ptp_ptpen[PTP_PHASE_INIT01] = ptp_read(PTP_PTPEN);

	/*
	 * Read & store 16 bit values DCVALUES.DCVOFFSET and
	 * AGEVALUES.AGEVOFFSET for later use in INIT2 procedure
	 */
	/* hw bug, workaround */
	det->DCVOFFSETIN = ~(ptp_read(PTP_DCVALUES) & 0xffff) + 1;
	det->AGEVOFFSETIN = ptp_read(PTP_AGEVALUES);

	/*
	 * Set PTPEN.PTPINITEN/PTPEN.PTPINIT2EN = 0x0 &
	 * Clear PTP INIT interrupt PTPINTSTS = 0x00000001
	 */
	ptp_write(PTP_PTPEN, 0x0);
	ptp_write(PTP_PTPINTSTS, 0x1);
	ptp_init01_finish(det);
	det->ops->init02(det);

	if (ptp_log_en)
		ptp_isr_info("[%s]\n", det->name);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_init02_isr(struct ptp_det *det)
{
	unsigned int temp;
	int i;
	struct ptp_ctrl *ctrl = id_to_ptp_ctrl(det->ctrl_id);

	FUNC_ENTER(FUNC_LV_LOCAL);
	if (ptp_log_en) {
		ptp_isr_info("@ %s(%s)\n", __func__, det->name);
		ptp_isr_info("ca7_temp=%d, c17_temp=%d gpu_temp=%d\n",
			tscpu_get_bL_temp(THERMAL_CA7),
			tscpu_get_bL_temp(THERMAL_CA15),
			tscpu_get_bL_temp(THERMAL_GPU));
	}

	det->dcvalues[PTP_PHASE_INIT02] = ptp_read(PTP_DCVALUES);
	det->ptp_freqpct30[PTP_PHASE_INIT02] = ptp_read(PTP_FREQPCT30);
	det->ptp_26c[PTP_PHASE_INIT02] = ptp_read(PTP_PTPINTEN + 0x10);
	det->ptp_vop30[PTP_PHASE_INIT02] = ptp_read(PTP_VOP30);
	det->ptp_ptpen[PTP_PHASE_INIT02] = ptp_read(PTP_PTPEN);

	temp = ptp_read(PTP_VOP30);
	det->volt_tbl[0] = temp & 0xff;
	det->volt_tbl[1] = (temp >> 8) & 0xff;
	det->volt_tbl[2] = (temp >> 16) & 0xff;
	det->volt_tbl[3] = (temp >> 24) & 0xff;

	temp = ptp_read(PTP_VOP74);
	det->volt_tbl[4] = temp & 0xff;
	det->volt_tbl[5] = (temp >> 8) & 0xff;
	det->volt_tbl[6] = (temp >> 16) & 0xff;
	det->volt_tbl[7] = (temp >> 24) & 0xff;

	memcpy(det->volt_tbl_init2, det->volt_tbl, sizeof(det->volt_tbl_init2));
	if (ptp_log_en)
		for (i = 0; i < NR_FREQ; i++)
			ptp_isr_info("ptp_detectors[%s].volt_tbl[%d] = 0x%08X\n",
					det->name, i, det->volt_tbl[i]);
	if (ptp_log_en)
		ptp_isr_info("ptp_level = 0x%08X\n", ptp_level);

	ptp_set_ptp_volt(det);

	/*
	 * Set PTPEN.PTPINITEN/PTPEN.PTPINIT2EN = 0x0 &
	 * Clear PTP INIT interrupt PTPINTSTS = 0x00000001
	 */
	ptp_write(PTP_PTPEN, 0x0);
	ptp_write(PTP_PTPINTSTS, 0x1);

	atomic_dec(&ctrl->in_init);
	complete(&ctrl->init_done);
	det->ops->mon_mode(det);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_init_err_isr(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_LOCAL);

	ptp_error("================== PTP init err: ===================\n");
	ptp_error("[%s]\n", det->name);
	ptp_error("PTPEN = 0x%08X, PTPINTSTS = 0x%08X\n",
				ptp_read(PTP_PTPEN), ptp_read(PTP_PTPINTSTS));
	ptp_error("PTP_SMSTATE0 = 0x%08X\n", ptp_read(PTP_SMSTATE0));
	ptp_error("PTP_SMSTATE1 = 0x%08X\n", ptp_read(PTP_SMSTATE1));
	ptp_error("================== PTP init err: ===================\n");

	/* TODO: FIXME */
	{
		struct ptp_ctrl *ctrl = id_to_ptp_ctrl(det->ctrl_id);

		atomic_dec(&ctrl->in_init);
		complete(&ctrl->init_done);
	}
	/* TODO: FIXME */

	det->ops->disable_locked(det, BY_INIT_ERROR);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_mon_mode_isr(struct ptp_det *det)
{
	unsigned int temp;
	int i;

	FUNC_ENTER(FUNC_LV_LOCAL);

	if (ptp_log_en) {
		ptp_isr_info("@ %s(%s)\n", __func__, det->name);
		ptp_isr_info("ca7_temp=%d, c17_temp=%d gpu_temp=%d\n",
					tscpu_get_bL_temp(THERMAL_CA7),
					tscpu_get_bL_temp(THERMAL_CA15),
					tscpu_get_bL_temp(THERMAL_GPU));
	}
	det->dcvalues[PTP_PHASE_MON] = ptp_read(PTP_DCVALUES);
	det->ptp_freqpct30[PTP_PHASE_MON] = ptp_read(PTP_FREQPCT30);
	det->ptp_26c[PTP_PHASE_MON] = ptp_read(PTP_PTPINTEN + 0x10);
	det->ptp_vop30[PTP_PHASE_MON] = ptp_read(PTP_VOP30);
	det->ptp_ptpen[PTP_PHASE_MON] = ptp_read(PTP_PTPEN);

	/* check if thermal sensor init completed? */
	temp = (ptp_read(PTP_TEMP) & 0xff);
	if (ptp_log_en)
		if ((temp > 0x4b) && (temp < 0xd3)) {
			ptp_isr_info("thermal init has not been completed. ");
			ptp_isr_info("(temp = 0x%08X)\n", temp);
			goto out;
		}

	temp = ptp_read(PTP_VOP30);
	det->volt_tbl[0] = temp & 0xff;
	det->volt_tbl[1] = (temp >> 8) & 0xff;
	det->volt_tbl[2] = (temp >> 16) & 0xff;
	det->volt_tbl[3] = (temp >> 24) & 0xff;

	temp = ptp_read(PTP_VOP74);
	det->volt_tbl[4] = temp & 0xff;
	det->volt_tbl[5] = (temp >> 8) & 0xff;
	det->volt_tbl[6] = (temp >> 16) & 0xff;
	det->volt_tbl[7] = (temp >> 24) & 0xff;
	if (ptp_log_en) {
		for (i = 0; i < NR_FREQ; i++)
			ptp_isr_info("ptp_detectors[%s].volt_tbl[%d] = 0x%08X\n",
							det->name, i, det->volt_tbl[i]);

		ptp_isr_info("ptp_level = 0x%08X\n", ptp_level);
		/* ptp_isr_info("ISR : TEMPSPARE1 = 0x%08X\n", ptp_read(TEMPSPARE1)); */
	}
	ptp_set_ptp_volt(det);

 out:
	/* Clear PTP INIT interrupt PTPINTSTS = 0x00ff0000 */
	ptp_write(PTP_PTPINTSTS, 0x00ff0000);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_mon_err_isr(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_LOCAL);

	/* PTP Monitor mode error handler */
	ptp_error("================== PTP mon err: ==================\n");
	ptp_error("[%s]\n", det->name);
	ptp_error("PTPEN = 0x%08X, PTPINTSTS = 0x%08X\n",
				ptp_read(PTP_PTPEN), ptp_read(PTP_PTPINTSTS));
	ptp_error("PTP_SMSTATE0 = 0x%08X\n", ptp_read(PTP_SMSTATE0));
	ptp_error("PTP_SMSTATE1 = 0x%08X\n", ptp_read(PTP_SMSTATE1));
	ptp_error("PTP_TEMP = 0x%08X\n", ptp_read(PTP_TEMP));
	ptp_error("PTP_TEMPMSR0 = 0x%08X\n", ptp_read(PTP_TEMPMSR0));
	ptp_error("PTP_TEMPMSR1 = 0x%08X\n", ptp_read(PTP_TEMPMSR1));
	ptp_error("PTP_TEMPMSR2 = 0x%08X\n", ptp_read(PTP_TEMPMSR2));
	ptp_error("PTP_TEMPMONCTL0 = 0x%08X\n", ptp_read(PTP_TEMPMONCTL0));
	ptp_error("PTP_TEMPMSRCTL1 = 0x%08X\n", ptp_read(PTP_TEMPMSRCTL1));
	ptp_error("================= PTP mon err: ===================\n");

	det->ops->disable_locked(det, BY_MON_ERROR);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void ptp_isr_handler(struct ptp_det *det)
{
	unsigned int PTPINTSTS, PTPEN;

	FUNC_ENTER(FUNC_LV_LOCAL);

	PTPINTSTS = ptp_read(PTP_PTPINTSTS);
	PTPEN = ptp_read(PTP_PTPEN);

	if (ptp_log_en) {
		ptp_isr_info("func = %s\n", __func__);
		ptp_isr_info("[%s]\n", det->name);
		ptp_isr_info("PTPINTSTS = 0x%08X\n", PTPINTSTS);
		ptp_isr_info("PTP_PTPEN = 0x%08X\n", PTPEN);
		ptp_isr_info("PTP_PTPODINTST = 0x%X\n", ptp_read(PTP_PTPODINTST));
		ptp_isr_info("PTP_PTPCORESEL = 0x%X\n", ptp_read(PTP_PTPCORESEL));
		ptp_isr_info("PTP_DCVALUES = 0x%08X\n", ptp_read(PTP_DCVALUES));
		ptp_isr_info("PTP_AGECOUNT = 0x%08X\n", ptp_read(PTP_AGECOUNT));

		ptp_isr_info("Source :\n");
		ptp_isr_info("PTP_DESCHAR = 0x%x\n", ptp_read(PTP_DESCHAR));
		ptp_isr_info("PTP_DCCONFIG = 0x%x\n", ptp_read(PTP_DCCONFIG));
		ptp_isr_info("PTP_DETCHAR = 0x%x\n", ptp_read(PTP_DETCHAR));
		ptp_isr_info("PTP_AGECONFIG = 0x%x\n",
						ptp_read(PTP_AGECONFIG));
		ptp_isr_info("PTP_AGECHAR = 0x%x\n", ptp_read(PTP_AGECHAR));
		ptp_isr_info("PTP_TEMPCHAR = 0x%x\n", ptp_read(PTP_TEMPCHAR));
		ptp_isr_info("PTP_RUNCONFIG = 0x%x\n",
						ptp_read(PTP_RUNCONFIG));
		ptp_isr_info("PTP_FREQPCT30 = 0x%x\n",
						ptp_read(PTP_FREQPCT30));
		ptp_isr_info("PTP_FREQPCT74 = 0x%x\n",
						ptp_read(PTP_FREQPCT74));
		ptp_isr_info("PTP_LIMITVALS = 0x%x\n",
						ptp_read(PTP_LIMITVALS));
		ptp_isr_info("PTP_VBOOT = 0x%x\n", ptp_read(PTP_VBOOT));
		ptp_isr_info("PTP_DETWINDOW = 0x%x\n",
						ptp_read(PTP_DETWINDOW));
		ptp_isr_info("PTP_PTPCONFIG = 0x%x\n",
						ptp_read(PTP_PTPCONFIG));
		ptp_isr_info("PTP_TSCALCS = 0x%x\n", ptp_read(PTP_TSCALCS));

		ptp_isr_info("Output check :\n");
		ptp_isr_info("PTP_PTPINTSTS = 0x%x\n",
						ptp_read(PTP_PTPINTSTS));
		ptp_isr_info("PTP_DCVALUES = 0x%x\n", ptp_read(PTP_DCVALUES));
		ptp_isr_info("PTP_AGECOUNT = 0x%x\n", ptp_read(PTP_AGECOUNT));
		ptp_isr_info("PTP_INIT2VALS = 0x%x\n",
						ptp_read(PTP_INIT2VALS));
		ptp_isr_info("PTP_VDESIGN30 = 0x%x\n",
						ptp_read(PTP_VDESIGN30));
		ptp_isr_info("PTP_VDESIGN74 = 0x%x\n",
						ptp_read(PTP_VDESIGN74));
		ptp_isr_info("PTP_TEMP = 0x%x\n", ptp_read(PTP_TEMP));
		ptp_isr_info("PTP_DVT30 = 0x%x\n", ptp_read(PTP_DVT30));
		ptp_isr_info("PTP_DVT74 = 0x%x\n", ptp_read(PTP_DVT74));
		ptp_isr_info("PTP_VOP30 = 0x%x\n", ptp_read(PTP_VOP30));
		ptp_isr_info("PTP_VOP74 = 0x%x\n", ptp_read(PTP_VOP74));
		ptp_isr_info("PTP_AGEVALUES = 0x%x\n", ptp_read(PTP_AGEVALUES));
		ptp_isr_info("PTP_CHKSHIFT = 0x%x\n", ptp_read(PTP_CHKSHIFT));
		ptp_isr_info("TEMPMSR0  = 0x%x\n", ptp_read(TEMPMSR0));
		ptp_isr_info("TEMPMSR1 = 0x%x\n", ptp_read(TEMPMSR1));
		ptp_isr_info("TEMPMSR2 = 0x%x\n", ptp_read(TEMPMSR2));
		ptp_isr_info("TEMPMSR3 = 0x%x\n", ptp_read(TEMPMSR3));
	}
	if (PTPINTSTS == 0x1) {	/* PTP init1 or init2 */
		if ((PTPEN & 0x7) == 0x1)	/* PTP init1 */
			handle_init01_isr(det);
		else if ((PTPEN & 0x7) == 0x5)	/* PTP init2 */
			handle_init02_isr(det);
		else {
			/*
			 * error : init1 or init2,
			 * but enable setting is wrong.
			 */
			handle_init_err_isr(det);
		}
	} else if ((PTPINTSTS & 0x00ff0000) != 0x0)
		handle_mon_mode_isr(det);
	else {			/* PTP error handler */
		/* init 1  || init 2 error handler */
		if (((PTPEN & 0x7) == 0x1) || ((PTPEN & 0x7) == 0x5))
			handle_init_err_isr(det);
		else		/* PTP Monitor mode error handler */
			handle_mon_err_isr(det);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static irqreturn_t ptp_isr(int irq, void *dev_id)
{
	unsigned long flags;
	struct ptp_det *det = NULL;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	if (ptp_log_en) {
		ptp_isr_info("func = %s\n", __func__);
		ptp_isr_info("PTP_PTPEN = 0x%08X\n", ptp_read(PTP_PTPEN));
		ptp_isr_info("PTP_PTPODINTST = 0x%X\n", ptp_read(PTP_PTPODINTST));
		ptp_isr_info("PTP_PTPCORESEL = 0x%X\n", ptp_read(PTP_PTPCORESEL));
	}

	/* mt_ptp_reg_dump(); // TODO: FIXME, for temp reg dump <-XXX */

	mt_ptp_lock(&flags);
	i = ptp_read(PTP_PTPODINTST);

	for (i = 0; i < NR_PTP_CTRL; i++) {
		/* TODO: FIXME, it is better to link i @ struct ptp_det */
		if ((BIT(i) & ptp_read(PTP_PTPODINTST)))
			continue;

		det = &ptp_detectors[i];
		if (likely(det)) {
			det->ops->switch_bank(det);
			/* mt_ptp_reg_dump_locked(); */
			/* TODO: FIXME, for temp reg dump <-XXX */
			ptp_isr_handler(det);
		}
	}

	mt_ptp_unlock(&flags);

	FUNC_EXIT(FUNC_LV_MODULE);

	if (ptp_log_en)
		ptp_isr_info("%s done.\n", __func__);

	return IRQ_HANDLED;
}

static atomic_t ptp_init01_cnt;
static void ptp_init01_prepare(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_LOCAL);

	atomic_inc(&ptp_init01_cnt);

	if (atomic_read(&ptp_init01_cnt) == 1) {
		enum mt_cpu_dvfs_id cpu;

		switch (det_to_id(det)) {
		case PTP_DET_LITTLE:
			cpu = MT_CPU_DVFS_LITTLE;
			break;

		case PTP_DET_BIG:
			cpu = MT_CPU_DVFS_BIG;
			break;

		default:
			return;
		}

#if 0

		if (0 == cpu) {	/* TODO: FIXME, for E1 */
			/* disable frequency hopping (main PLL) */
			mt_fh_popod_save();
			/* disable DVFS and set vproc = 1.15v (1 GHz) */
			mt_cpufreq_disable_by_ptpod(cpu);
		}
#endif				/* TODO: move to ptp_init01() */
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static void ptp_init01_finish(struct ptp_det *det)
{
	atomic_dec(&ptp_init01_cnt);

	FUNC_ENTER(FUNC_LV_LOCAL);

	if (atomic_read(&ptp_init01_cnt) < 0)
		BUG();

	if (atomic_read(&ptp_init01_cnt) == 0) {
		enum mt_cpu_dvfs_id cpu;

		switch (det_to_id(det)) {
		case PTP_DET_LITTLE:
			cpu = MT_CPU_DVFS_LITTLE;
			break;

		case PTP_DET_BIG:
			cpu = MT_CPU_DVFS_BIG;
			break;

		default:
			return;
		}

#if 0				/* TODO: move to ptp_init01() */

		if (0 == cpu) {	/* TODO: FIXME, for E1 */
			/* enable DVFS */
			mt_cpufreq_enable_by_ptpod(cpu);
			/* enable frequency hopping (main PLL) */
			mt_fh_popod_restore();
		}
#endif				/* TODO: move to ptp_init01() */
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

void ptp_init01(void)
{
	struct ptp_det *det;
	struct ptp_ctrl *ctrl;

	FUNC_ENTER(FUNC_LV_LOCAL);

	for_each_det_ctrl(det, ctrl) {
		{
			unsigned long flag;	/* <-XXX */
			unsigned int vboot;

			vboot = MV_TO_VAL(det->ops->get_volt(det));

			if (vboot != det->VBOOT) {
				ptp_error("@%s: get_volt(%s)",
				__func__, det->name);
				ptp_error("=0x%08d, VBOOT=0x%08X\n", vboot,
				det->VBOOT);
			}
			mt_ptp_lock(&flag);	/* <-XXX */
			det->ops->init01(det);
			mt_ptp_unlock(&flag);	/* <-XXX */
		}

		/*
		 * VCORE_AO and VCORE_PDN use the same controller.
		 * Wait until VCORE_AO init01 and init02 done
		 */
		if (atomic_read(&ctrl->in_init)) {
			/* TODO: Use workqueue to avoid blocking */
			ptp_notice("@%s():%d, wait_for_completion(%s) in\n",
						__func__, __LINE__, det->name);
			wait_for_completion(&ctrl->init_done);
			ptp_notice("@%s():%d, wait_for_completion(%s) out\n",
						__func__, __LINE__, det->name);
		}
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

void ptp_init02(void)
{
	struct ptp_det *det;
	struct ptp_ctrl *ctrl;

	FUNC_ENTER(FUNC_LV_LOCAL);

	for_each_det_ctrl(det, ctrl) {
		if (HAS_FEATURE(det, FEA_MON)) {
			unsigned long flag;

			mt_ptp_lock(&flag);
			det->ops->init02(det);
			mt_ptp_unlock(&flag);
		}
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

#if EN_PTP_OD

/* leakage */
unsigned int leakage_core;
unsigned int leakage_gpu;
unsigned int leakage_sram2;
unsigned int leakage_sram1;

void get_devinfo(struct ptp_devinfo *p)
{
	int *val = (int *)p;

	FUNC_ENTER(FUNC_LV_HELP);

	/* TODO: Read devinfo from file system? */
#if 1				/* FIXME: ned get_devinfo_with_index() ready */
	{
		int i;

		val[4] = get_devinfo_with_index(15);	/* M_HW_RES4 */
		val[5] = get_devinfo_with_index(16);	/* M_HW_RES5 */

		/* search table (temp) */
		#ifdef DEVINFO
		for (i = 0; i < ARRAY_SIZE(devinfo); i++) {
			if (val[4] == devinfo[i].M_HW_RES4 &&
				val[5] == devinfo[i].M_HW_RES5) {
				ptp_notice("get array\n");
				val[0] = devinfo[i].M_HW_RES0;
				val[1] = devinfo[i].M_HW_RES1;
				val[2] = get_devinfo_with_index(9);
				val[3] = devinfo[i].M_HW_RES3;
				val[6] = devinfo[i].M_HW_RES6;
				val[7] = devinfo[i].M_HW_RES7;
				val[8] = devinfo[i].M_HW_RES8;
				val[9] = devinfo[i].M_HW_RES9;
				leakage_core = devinfo[i].core;
				leakage_gpu = devinfo[i].gpu;
				leakage_sram2 = devinfo[i].sram2;
				leakage_sram1 = devinfo[i].sram1;

				p->PTPINITEN = 1;
				p->PTPMONEN = 1;

				break;
			}
		}
		/* get efuse */
		if (i >= ARRAY_SIZE(devinfo)) {
			ptp_notice("get efuse\n");
			val[0] = get_devinfo_with_index(7);
			val[1] = get_devinfo_with_index(8);
			val[2] = get_devinfo_with_index(9);
			val[3] = get_devinfo_with_index(14);
			val[4] = get_devinfo_with_index(15);
			val[5] = get_devinfo_with_index(16);
			val[6] = get_devinfo_with_index(17);
			val[7] = get_devinfo_with_index(18);
			val[8] = get_devinfo_with_index(19);
			val[9] = get_devinfo_with_index(21);
		}
		#else
		ptp_notice("get efuse\n");
		val[0] = get_devinfo_with_index(7);
		val[1] = get_devinfo_with_index(8);
		val[2] = get_devinfo_with_index(9);
		val[3] = get_devinfo_with_index(14);
		val[4] = get_devinfo_with_index(15);
		val[5] = get_devinfo_with_index(16);
		val[6] = get_devinfo_with_index(17);
		val[7] = get_devinfo_with_index(18);
		val[8] = get_devinfo_with_index(19);
		val[9] = get_devinfo_with_index(21);
		#endif
		#ifdef DEVINFO
		if (0 == ptp_devinfo.PTPINITEN) {
			ptp_error("hard code\n");
			val[0] = 0x0F0F0F0F;
			val[1] = 0xBABAB9B6;
			val[2] = 0x00000000;
			val[3] = 0x00000000;
			val[4] = 0x00000003;
			val[5] = 0x00000000;
			val[6] = 0x00000000;
			val[7] = 0x32271E37;
			val[8] = 0x252B1912;
			val[9] = 0x434C854B;
		}
		#endif
		for (i = 0; i < sizeof(struct ptp_devinfo) / sizeof(unsigned int); i++)
			ptp_notice("M_HW_RES%d\t= 0x%08X\n", i, val[i]);
	}
#else
	val[0] = get_devinfo_with_index(7);
	val[1] = get_devinfo_with_index(8);
	val[2] = get_devinfo_with_index(9);
	val[3] = get_devinfo_with_index(14);
	val[4] = get_devinfo_with_index(15);
	val[5] = get_devinfo_with_index(16);
	val[6] = get_devinfo_with_index(17);
	val[7] = get_devinfo_with_index(18);
	val[8] = get_devinfo_with_index(19);
	val[9] = get_devinfo_with_index(21);
#endif
	FUNC_EXIT(FUNC_LV_HELP);
}

static const struct of_device_id ptp_of_match[] = {
	{.compatible = "mediatek,mt8173-ptp_od",},
	{},
};

MODULE_DEVICE_TABLE(of, ptp_of_match);

static int ptp_probe(struct platform_device *pdev)
{
	int ret;
	struct ptp_det *det;
	struct ptp_ctrl *ctrl;
	struct clk *clk_therm;
	struct clk *clk_axi_sel = NULL;
	struct clk *univpll2_d2 = NULL;
	struct clk *syspll1_d2 = NULL;
	struct clk *axi_sel_parent;
	int ptpod_id;

	FUNC_ENTER(FUNC_LV_MODULE);

	/* wait for Vgpu power */
	if (!mt_gpucore_ready()) {
		ptp_error("ptp_probe: GPU core is not ready\n");
		return -EPROBE_DEFER;
	}

	ptpod_id = (ptp_devinfo.M_HW_RES5 >> PTPOD_ID_SHIFT) & PTPOD_ID_MASK;
	dev_dbg(&pdev->dev, "ptpod_id: %d\n", ptpod_id);

#if HAS_PTPOD_VCORE
	reg_vcore = devm_regulator_get(&pdev->dev, "reg-vcore");
	if (IS_ERR(reg_vcore)) {
		ret = PTR_ERR(reg_vcore);
		dev_err(&pdev->dev, "Failed to request reg-vcore: %d\n", ret);
		return ret;
	}
#endif

	clk_therm = devm_clk_get(&pdev->dev, "ptp_peri_therm");
	BUG_ON(IS_ERR(clk_therm));

	clk_axi_sel = devm_clk_get(&pdev->dev, "axi_sel");
	BUG_ON(IS_ERR(clk_axi_sel));
	axi_sel_parent = clk_get_parent(clk_axi_sel);

	/* therm_ctrl & ptp_ctrl share same clock*/
	/* Therefore, we only need to enable thermal clock */
	clk_prepare_enable(clk_therm);

	ret = request_irq(ptpod_irq_number, ptp_isr, IRQF_TRIGGER_LOW, "ptp", NULL);
	if (ret) {
		ptp_warning("PTP IRQ register failed (%d)\n", ret);
		WARN_ON(1);
	}

	ptp_debug("Set PTP IRQ OK.\n");
	ptp_level = mt_ptp_get_level();
	atomic_set(&ptp_init01_cnt, 0);

	/* Create threads for updating CPU/GPU 8's dc level. */
	for_each_ctrl(ctrl) {
		ptp_init_ctrl(ctrl);
	}

	/* If chip ic is E2, Vcore voltage isn't needed to be tuned down by PTPOD. */
	/* if (mt_get_chip_sw_ver() == CHIP_SW_VER_02) {
		TODO: If PTPOD needs to control Vcore voltage, we need to take care of this case.
	} */

	ptp_notice("PTPOD starts to initialize\n");

	/* disable frequency hopping (main PLL) */
	mt_fh_popod_save();

	/* disable DVFS and set vproc = 1.00v */
	mt_cpufreq_disable_by_ptpod(MT_CPU_DVFS_LITTLE);
	mt_cpufreq_disable_by_ptpod(MT_CPU_DVFS_BIG);

#if HAS_PTPOD_VCORE
	/* don't use get_volt_vcore() because it focibly devide 1000 and it will truncate some data */
	vcore_original = regulator_get_voltage(reg_vcore);
#endif

	mt_gpufreq_disable_by_ptpod();

	/* This case is for certain IC-efuse whose axi_sel clock is initialized as 208MHz.
	   So, set topckgen's axi_sel to 208MHz by univpll2_d2. */
	if (ptpod_id <= 1) {
		univpll2_d2 = devm_clk_get(&pdev->dev, "univpll2_d2");
		BUG_ON(IS_ERR(univpll2_d2));
		ret = clk_set_parent(clk_axi_sel, univpll2_d2);
		if (ret) {
			ptp_warning("clk_axi_sel should be 208MHz. Now, clk_axi_sel = %luHz\n",
									__clk_get_rate(clk_axi_sel));
			WARN_ON(1);
		}
	} else {
		/* This case is for the rest IC-efuse whose axi_sel clock is initialized as 273MHz. */
		syspll1_d2 = devm_clk_get(&pdev->dev, "syspll1_d2");
		BUG_ON(IS_ERR(syspll1_d2));

#if HAS_PTPOD_VCORE
		ret = regulator_set_voltage(reg_vcore, 1125000, 1125000);
		if (ret) {
			ptp_error("Vcore should be 1.125V. Now, Vcore = %duV\n",
							regulator_get_voltage(reg_vcore));
			WARN_ON(1);
		}
#endif

		ret = clk_set_parent(clk_axi_sel, syspll1_d2);
		if (ret) {
			ptp_error("clk_axi_sel should be 273MHz. Now, clk_axi_sel = %luHz\n",
									__clk_get_rate(clk_axi_sel));
			WARN_ON(1);
		}
	}

	ptp_notice("clk_axi_sel = %luHz\n", __clk_get_rate(clk_axi_sel));
	ptp_notice("clk_therm = %luHz\n", __clk_get_rate(clk_therm));

#if HAS_PTPOD_VCORE
	ptp_notice("Vcore = %duV\n", regulator_get_voltage(reg_vcore));
#endif

	/* disable slow idle */
	ptp_data[0] = 0xffffffff;

	for_each_det(det) {
		ptp_init_det(det, &ptp_devinfo);
	}

	ptp_init01();

	ptp_notice("PTPOD initiailization done\n");

	/* enable slow idle */
	ptp_data[0] = 0;

	/* Set topckgen's axi back to its original parent clock */
	ret = clk_set_parent(clk_axi_sel, axi_sel_parent);
	if (ret) {
		ptp_error("Fail to set clk_axi_sel to its origianl parent clock. Now, clk_axi_sel = %luHz\n",
											__clk_get_rate(clk_axi_sel));
		WARN_ON(1);
	}

#if HAS_PTPOD_VCORE
	ret = regulator_set_voltage(reg_vcore, vcore_original, vcore_original);
	if (ret) {
		ptp_error("Fail to set vcore back. Now, Vcore = %duV\n",
								regulator_get_voltage(reg_vcore));
		WARN_ON(1);
	}
#endif

	mt_gpufreq_enable_by_ptpod();
	mt_cpufreq_enable_by_ptpod(MT_CPU_DVFS_LITTLE);
	mt_cpufreq_enable_by_ptpod(MT_CPU_DVFS_BIG);

	axi_sel_clk = __clk_get_rate(clk_axi_sel);
	therm_ctrl_clk = __clk_get_rate(clk_therm);
#if HAS_PTPOD_VCORE
	vcore_original = regulator_get_voltage(reg_vcore);
#endif

	ptp_notice("clk_axi_sel = %luHz\n", axi_sel_clk);
	ptp_notice("clk_therm = %luHz\n", therm_ctrl_clk);

#if HAS_PTPOD_VCORE
	ptp_notice("Vcore = %duV\n", vcore_original);
	regulator_put(reg_vcore);
#endif

	/* enable frequency hopping (main PLL) */
	mt_fh_popod_restore();

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int ptp_suspend(struct platform_device *pdev, pm_message_t state)
{
	/*
	   kthread_stop(ptp_volt_thread);
	 */
	FUNC_ENTER(FUNC_LV_MODULE);
	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int ptp_resume(struct platform_device *pdev)
{
	/*
	ptp_volt_thread = kthread_run(ptp_volt_thread_handler, 0, "ptp volt");
	if (IS_ERR(ptp_volt_thread))
	{
		printk("[%s]: failed to create ptp volt thread\n", __func__);
	}
	*/
	FUNC_ENTER(FUNC_LV_MODULE);
	ptp_init02();
	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static struct platform_driver ptp_driver = {
	.remove = NULL,
	.shutdown = NULL,
	.probe = ptp_probe,
	.suspend = ptp_suspend,
	.resume = ptp_resume,
	.driver = {
			.name = "mt-ptp",
			.of_match_table = of_match_ptr(ptp_of_match),
	},
};

int mt_ptp_opp_num(ptp_det_id id)
{
	struct ptp_det *det = id_to_ptp_det(id);

	FUNC_ENTER(FUNC_LV_API);
	FUNC_EXIT(FUNC_LV_API);

	return det->num_freq_tbl;
}
EXPORT_SYMBOL(mt_ptp_opp_num);

void mt_ptp_opp_freq(ptp_det_id id, unsigned int *freq)
{
	struct ptp_det *det = id_to_ptp_det(id);
	int i = 0;

	FUNC_ENTER(FUNC_LV_API);

	for (i = 0; i < det->num_freq_tbl; i++)
		freq[i] = det->freq_tbl[i];

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_ptp_opp_freq);

void mt_ptp_opp_status(ptp_det_id id, unsigned int *temp,
						unsigned int *volt)
{
	struct ptp_det *det = id_to_ptp_det(id);
	int i = 0;

	FUNC_ENTER(FUNC_LV_API);

	/* TODO: FIXME */
	*temp = tscpu_get_bL_temp((id == PTP_DET_LITTLE) ? THERMAL_BANK0 : THERMAL_BANK1);

	for (i = 0; i < det->num_freq_tbl; i++)
		volt[i] = VAL_TO_MV(det->volt_tbl_pmic[i]);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_ptp_opp_status);

/***************************
* return current PTP stauts
****************************/
int mt_ptp_status(ptp_det_id id)
{
	struct ptp_det *det = id_to_ptp_det(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(!det);
	BUG_ON(!det->ops);
	BUG_ON(!det->ops->get_status);

	FUNC_EXIT(FUNC_LV_API);

	return det->ops->get_status(det);
}

/**
 * ===============================================
 * PROCFS interface for debugging
 * ===============================================
 */

/*
 * show current PTP stauts
 */
static int ptp_debug_proc_show(struct seq_file *m, void *v)
{
	struct ptp_det *det = (struct ptp_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	/* FIXME: PTPEN sometimes is disabled temp */
	seq_printf(m, "PTPOD[%s] %s (ptp_level = 0x%08X)\n",
				det->name, det->ops->get_status(det) ?
				"enabled" : "disable", ptp_level);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * set PTP status by procfs interface
 */
static ssize_t ptp_debug_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	int enabled = 0;
	char *buf = (char *)__get_free_page(GFP_USER);
	struct ptp_det *det = (struct ptp_det *)PDE_DATA(file_inode(file));

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
		/* det->ops->enable(det, BY_PROCFS); */
		/* TODO: FIXME, kernel panic when enabled */
		if (1 == enabled)
			;
		else
			det->ops->disable(det, BY_PROCFS);
	} else
		ret = -EINVAL;

 out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

/*
 * show current PTP data
 */
static int ptp_dump_proc_show(struct seq_file *m, void *v)
{
	struct ptp_det *det;
	int *val = (int *)&ptp_devinfo;
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	/* ptp_detectors[PTP_DET_LITTLE].ops->dump_status(
				&ptp_detectors[PTP_DET_LITTLE]); */
	/* ptp_detectors[PTP_DET_BIG].ops->dump_status(
				&ptp_detectors[PTP_DET_BIG]); */

	mt_ptp_reg_dump();

	for (i = 0; i < sizeof(struct ptp_devinfo) / sizeof(unsigned int); i++)
		seq_printf(m, "M_HW_RES%d\t= 0x%08X\n", i, val[i]);

	seq_puts(m, "Clocks & Vcore set by PTPOD\n");
	seq_printf(m, "clk_axi_sel\t= %luHz\n", axi_sel_clk);
	seq_printf(m, "clk_therm\t= %luHz\n", therm_ctrl_clk);
#if HAS_PTPOD_VCORE
	seq_printf(m, "Vcore\t\t= %duV\n", vcore_original);
#endif
	seq_printf(m, "leakage_core\t= %d\n"
			"leakage_gpu\t= %d\n"
			"leakage_little\t= %d\n"
			"leakage_big\t= %d\n",
			leakage_core, leakage_gpu, leakage_sram2,
			leakage_sram1);

	for_each_det(det) {
		seq_printf(m, "PTP_DCVALUES[%s]\t= 0x%08X\n", det->name,
					det->VBOOT);

		for (i = PTP_PHASE_INIT01; i < NR_PTP_PHASE; i++)
			seq_printf(m, "0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
					det->dcvalues[i],
					det->ptp_freqpct30[i],
					det->ptp_26c[i], det->ptp_vop30[i],
					det->ptp_ptpen[i]);
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * show current voltage
 */
static int ptp_cur_volt_proc_show(struct seq_file *m, void *v)
{
	struct ptp_det *det = (struct ptp_det *)m->private;
	u32 rdata = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	rdata = det->ops->get_volt(det);

	if (rdata != 0)
		seq_printf(m, "%d\n", rdata);
	else
		seq_printf(m, "PTPOD[%s] read current voltage fail\n",
					det->name);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * show current PTP status
 */
static int ptp_status_proc_show(struct seq_file *m, void *v)
{
	struct ptp_det *det = (struct ptp_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	seq_printf(m, "PTP_LOG:PTPOD[%s](%d)-(%d, %d, %d, %d, %d, %d, %d, %d)",
			det->name, det->ops->get_temp(det),
			VAL_TO_MV(det->volt_tbl_pmic[0]),
			VAL_TO_MV(det->volt_tbl_pmic[1]),
			VAL_TO_MV(det->volt_tbl_pmic[2]),
			VAL_TO_MV(det->volt_tbl_pmic[3]),
			VAL_TO_MV(det->volt_tbl_pmic[4]),
			VAL_TO_MV(det->volt_tbl_pmic[5]),
			VAL_TO_MV(det->volt_tbl_pmic[6]),
			VAL_TO_MV(det->volt_tbl_pmic[7])),
	seq_printf(m, "(%d, %d, %d, %d, %d, %d, %d, %d)\n",
			det->freq_tbl[0],
			det->freq_tbl[1],
			det->freq_tbl[2],
			det->freq_tbl[3],
			det->freq_tbl[4],
			det->freq_tbl[5],
			det->freq_tbl[6],
			det->freq_tbl[7]);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * set PTP log enable by procfs interface
 */

static int ptp_log_en_proc_show(struct seq_file *m, void *v)
{
	FUNC_ENTER(FUNC_LV_HELP);
	seq_printf(m, "%d\n", ptp_log_en);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static ssize_t ptp_log_en_proc_write(
struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *)__get_free_page(GFP_USER);

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

	ret = 0;

	if (kstrtoint(buf, 10, &ptp_log_en))
		ptp_notice("kstrtoint(xx, 10, xx) cannot parse your input.");

	switch (ptp_log_en) {
	case 0:
		ptp_notice("ptp log disabled, ptp_log_en = %d\n", ptp_log_en);
		hrtimer_cancel(&ptp_log_timer);
		break;

	case 1:
		ptp_notice("ptp log enabled, ptp_log_en = %d\n", ptp_log_en);
		hrtimer_start(&ptp_log_timer, ns_to_ktime(LOG_INTERVAL), HRTIMER_MODE_REL);
		break;

	default:
		ptp_error("bad argument!! Should be \"0\" or \"1\"\n");
		ret = -EINVAL;
		break;
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}


/*
 * show PTP offset
 */
static int ptp_offset_proc_show(struct seq_file *m, void *v)
{
	struct ptp_det *det = (struct ptp_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	seq_printf(m, "%d\n", det->volt_offset);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * set PTP offset by procfs
 */
static ssize_t ptp_offset_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *)__get_free_page(GFP_USER);
	int offset = 0;
	struct ptp_det *det = (struct ptp_det *)PDE_DATA(file_inode(file));

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
		ptp_set_ptp_volt(det);
	} else {
		ret = -EINVAL;
		ptp_notice("bad argument_1!! argument should be positive integer\n");
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
		.owner	= THIS_MODULE,				\
		.open	= name ## _proc_open,			\
		.read	= seq_read,				\
		.llseek	= seq_lseek,				\
		.release	= single_release,			\
		.write	= name ## _proc_write,			\
	}

#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner	= THIS_MODULE,				\
		.open	= name ## _proc_open,			\
		.read	= seq_read,				\
		.llseek	= seq_lseek,				\
		.release	= single_release,			\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(ptp_debug);
PROC_FOPS_RO(ptp_dump);
PROC_FOPS_RW(ptp_log_en);
PROC_FOPS_RO(ptp_status);
PROC_FOPS_RO(ptp_cur_volt);
PROC_FOPS_RW(ptp_offset);

static int create_procfs(void)
{
	struct proc_dir_entry *ptp_dir = NULL;
	struct proc_dir_entry *det_dir = NULL;
	int i;
	struct ptp_det *det;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry det_entries[] = {
		PROC_ENTRY(ptp_debug),
		PROC_ENTRY(ptp_status),
		PROC_ENTRY(ptp_cur_volt),
		PROC_ENTRY(ptp_offset),
	};

	struct pentry ptp_entries[] = {
		PROC_ENTRY(ptp_dump),
		PROC_ENTRY(ptp_log_en),
	};

	FUNC_ENTER(FUNC_LV_HELP);

	ptp_dir = proc_mkdir("ptp", NULL);

	if (!ptp_dir) {
		ptp_error("[%s]: mkdir /proc/ptp failed\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(ptp_entries); i++) {
		if (!proc_create(ptp_entries[i].name,
						S_IRUGO | S_IWUSR | S_IWGRP,
						ptp_dir, ptp_entries[i].fops)) {
			ptp_error("[%s]: create /proc/ptp/%s failed\n",
						__func__, ptp_entries[i].name);
			FUNC_EXIT(FUNC_LV_HELP);
			return -3;
		}
	}

	for_each_det(det) {
		det_dir = proc_mkdir(det->name, ptp_dir);

		if (!det_dir) {
			ptp_error("[%s]: mkdir /proc/ptp/%s failed\n", __func__,
						det->name);
			FUNC_EXIT(FUNC_LV_HELP);
			return -2;
		}

		for (i = 0; i < ARRAY_SIZE(det_entries); i++) {
			if (!proc_create_data(det_entries[i].name,
				S_IRUGO | S_IWUSR | S_IWGRP,
				det_dir, det_entries[i].fops, det)) {
				ptp_error("create /proc/ptp/%s/%s failed\n",
						det->name, det_entries[i].name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -3;
			}
		}
	}

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}

void ptp_efuse_calibration(struct ptp_devinfo *ptp_dev)
{
	int temp;

	ptp_notice("%s\n", __func__);

	is_CA15L_DCBDET_overflow = 0;

	if (mt_get_chip_sw_ver() != CHIP_SW_VER_04) {
		ptp_notice("Not E4 IC, No need to do ptp-efuse-calibration\n");
		return;
	}

	if (ptp_dev->CA15L_DCBDET >= 128) {
		ptp_notice("CA15L_DCBDET = %d which is correct\n", ptp_dev->CA15L_DCBDET);
		return;
	}

	ptp_notice("origianl CA15L_DCBDET = %d\n", ptp_dev->CA15L_DCBDET);

	/* fix CA15L_DCBDET overflow */
	is_CA15L_DCBDET_overflow = 1;
	temp = ptp_dev->CA15L_DCBDET;
	ptp_dev->CA15L_DCBDET = (unsigned char)((temp - 256) / 2);
	ptp_warning("re-calculate CA15L_DCBDET = %d\n", ptp_dev->CA15L_DCBDET);
}

/*
 * Module driver
 */
static int __init ptp_init(void)
{
	int err = 0;
	struct device_node *node;

	FUNC_ENTER(FUNC_LV_MODULE);

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-ptp_od");
	BUG_ON(node == 0);
	if (node) {
		/* Setup IO addresses */
		ptpod_base = of_iomap(node, 0);
		if (!ptpod_base) {
			ptp_error("ptpod_base of_iomap error\n");
			return -ENOMEM;
		}
	}

	/*get ptpod irq num */
	ptpod_irq_number = irq_of_parse_and_map(node, 0);
	if (!ptpod_irq_number) {
		ptp_error("get irqnr failed = 0x%x\n", ptpod_irq_number);
		return 0;
	}

	ptp_notice("ptpod_irq_number = 0x%x\n", ptpod_irq_number);
	ptp_notice("ptpod_base = 0x%16p\n", (void *)ptpod_base);
	get_devinfo(&ptp_devinfo);

	if (0 == ptp_devinfo.PTPINITEN) {
		ptp_error("PTPINITEN = 0x%08X\n", ptp_devinfo.PTPINITEN);
		FUNC_EXIT(FUNC_LV_MODULE);
		return 0;
	}

	ptp_efuse_calibration(&ptp_devinfo);

	/*
	 * init timer for log / volt
	 */
	hrtimer_init(&ptp_log_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ptp_log_timer.function = ptp_log_timer_func;	/* <-XXX */

	create_procfs();

	/*
	 * reg platform device driver
	 */
	err = platform_driver_register(&ptp_driver);

	if (err) {
		ptp_notice("PTP driver callback register failed..\n");
		FUNC_EXIT(FUNC_LV_MODULE);
		return err;
	}

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static void __exit ptp_exit(void)
{
	FUNC_ENTER(FUNC_LV_MODULE);
	ptp_notice("PTP de-initialization\n");
	FUNC_EXIT(FUNC_LV_MODULE);
}

/* TODO: FIXME, disable for bring up */
late_initcall(ptp_init);
#endif

MODULE_DESCRIPTION("MediaTek PTPOD Driver v0.3");
MODULE_LICENSE("GPL");
MODULE_VERSION("2.0");

#undef __MT_PTP_C__
