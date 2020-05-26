// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef ___MT_GPUFREQ_INTERNAL_PLAT_H___
#define ___MT_GPUFREQ_INTERNAL_PLAT_H___

/**************************************************
 *  0:     all on when mtk probe init (freq/ Vgpu/ Vsram_gpu)
 *         disable DDK power on/off callback
 **************************************************/
#define MT_GPUFREQ_POWER_CTL_ENABLE	1

/**************************************************
 * (DVFS_ENABLE, CUST_CONFIG)
 * (1, 1) -> DVFS enable and init to CUST_INIT_OPP
 * (1, 0) -> DVFS enable
 * (0, 1) -> DVFS disable but init to CUST_INIT_OPP (do DVFS only onces)
 * (0, 0) -> DVFS disable
 **************************************************/
#define MT_GPUFREQ_DVFS_ENABLE          1
#define MT_GPUFREQ_CUST_CONFIG          0
#define MT_GPUFREQ_CUST_INIT_OPP        (836000)

/**************************************************
 * DVFS Setting
 **************************************************/
#define NUM_OF_OPP_IDX (sizeof(g_opp_table_segment_1) / \
			sizeof(g_opp_table_segment_1[0]))

/* On opp table, low vgpu will use the same vsram.
 * And hgih vgpu will have the same diff with vsram.
 *
 * if (vgpu <= FIXED_VSRAM_VOLT_THSRESHOLD) {
 *     vsram = FIXED_VSRAM_VOLT;
 * } else {
 *     vsram = vgpu + FIXED_VSRAM_VOLT_DIFF;
 * }
 */
#define FIXED_VSRAM_VOLT                (75000)
#define FIXED_VSRAM_VOLT_THSRESHOLD     (75000)
#define FIXED_VSRAM_VOLT_DIFF           (0)

/**************************************************
 * PMIC Setting
 **************************************************/
/* PMIC hardware range:
 * vgpu      0.3 ~ 1.19375 V
 * vsram_gpu 0.5 ~ 1.29375 V
 */
#define VGPU_MAX_VOLT                   (119375)        /* mV x 100 */
#define VGPU_MIN_VOLT                   (30000)         /* mV x 100 */
#define VSRAM_GPU_MAX_VOLT              (129375)        /* mV x 100 */
#define VSRAM_GPU_MIN_VOLT              (50000)         /* mV x 100 */
#define PMIC_STEP                       (625)           /* mV x 100 */
/*
 * (-100)mv <= (VSRAM - VGPU) <= (300)mV
 */
#define BUCK_DIFF_MAX                   (30000)         /* mV x 100 */
#define BUCK_DIFF_MIN                   (-10000)        /* mV x 100 */

/**************************************************
 * Clock Setting
 **************************************************/
#define POSDIV_4_MAX_FREQ               (950000)        /* KHz */
#define POSDIV_4_MIN_FREQ               (375000)        /* KHz */
#define POSDIV_8_MAX_FREQ               (475000)        /* KHz */
#define POSDIV_8_MIN_FREQ               (187500)        /* KHz */
#define POSDIV_SHIFT                    (24)            /* bit */
#define DDS_SHIFT                       (14)            /* bit */
#define TO_MHZ_HEAD                     (100)
#define TO_MHZ_TAIL                     (10)
#define ROUNDING_VALUE                  (5)
#define MFGPLL_FIN                      (26)            /* MHz */
#define MFGPLL_FH_PLL                   FH_PLL6
#define MFGPLL_CON1                     (g_apmixed_base + 0x026C)

/**************************************************
 * Reference Power Setting
 **************************************************/
#define GPU_ACT_REF_POWER               (1285)                /* mW  */
#define GPU_ACT_REF_FREQ                (900000)              /* KHz */
#define GPU_ACT_REF_VOLT                (90000)               /* mV x 100 */
#define PTPOD_DISABLE_VOLT              (75000)

/**************************************************
 * Battery Over Current Protect
 **************************************************/
#define MT_GPUFREQ_BATT_OC_PROTECT              1
#define MT_GPUFREQ_BATT_OC_LIMIT_FREQ           (485000)        /* KHz */

/**************************************************
 * Battery Percentage Protect
 **************************************************/
#define MT_GPUFREQ_BATT_PERCENT_PROTECT         0
#define MT_GPUFREQ_BATT_PERCENT_LIMIT_FREQ      (485000)        /* KHz */

/**************************************************
 * Low Battery Volume Protect
 **************************************************/
#define MT_GPUFREQ_LOW_BATT_VOLT_PROTECT        1
#define MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ     (485000)        /* KHz */

/**************************************************
 * DFD Dump
 **************************************************/
#define MT_GPUFREQ_DFD_ENABLE 1
#define MT_GPUFREQ_DFD_DEBUG 0

/**************************************************
 * Register Manipulations
 **************************************************/
#define READ_REGISTER_UINT32(reg)	\
	(*(unsigned int * const)(reg))
#define WRITE_REGISTER_UINT32(reg, val)	\
	((*(unsigned int * const)(reg)) = (val))
#define INREG32(x)	\
	READ_REGISTER_UINT32((unsigned int *)((void *)(x)))
#define OUTREG32(x, y)	\
	WRITE_REGISTER_UINT32((unsigned int *)((void *)(x)), (unsigned int)(y))
#define SETREG32(x, y)	\
	OUTREG32(x, INREG32(x)|(y))
#define CLRREG32(x, y)	\
	OUTREG32(x, INREG32(x)&~(y))
#define MASKREG32(x, y, z)	\
	OUTREG32(x, (INREG32(x)&~(y))|(z))
#define DRV_Reg32(addr)				INREG32(addr)
#define DRV_WriteReg32(addr, data)	OUTREG32(addr, data)
#define DRV_SetReg32(addr, data)	SETREG32(addr, data)
#define DRV_ClrReg32(addr, data)	CLRREG32(addr, data)

/**************************************************
 * Proc Node Definition
 **************************************************/
#ifdef CONFIG_PROC_FS
#define PROC_FOPS_RW(name)	\
	static int mt_ ## name ## _proc_open(	\
			struct inode *inode,	\
			struct file *file)	\
	{	\
		return single_open(	\
				file,	\
				mt_ ## name ## _proc_show,	\
				PDE_DATA(inode));	\
	}	\
	static const struct file_operations mt_ ## name ## _proc_fops =	\
	{	\
		.owner = THIS_MODULE,	\
		.open = mt_ ## name ## _proc_open,	\
		.read = seq_read,	\
		.llseek = seq_lseek,	\
		.release = single_release,	\
		.write = mt_ ## name ## _proc_write,	\
	}
#define PROC_FOPS_RO(name)	\
	static int mt_ ## name ## _proc_open(	\
			struct inode *inode,	\
			struct file *file)	\
	{	\
		return single_open(	\
				file,	\
				mt_ ## name ## _proc_show,	\
				PDE_DATA(inode));	\
	}	\
	static const struct file_operations mt_ ## name ## _proc_fops =	\
	{	\
		.owner = THIS_MODULE,	\
		.open = mt_ ## name ## _proc_open,	\
		.read = seq_read,	\
		.llseek = seq_lseek,	\
		.release = single_release,	\
	}
#define PROC_ENTRY(name) \
	{__stringify(name), &mt_ ## name ## _proc_fops}
#endif

/**************************************************
 * Operation Definition
 **************************************************/
#define VOLT_NORMALIZATION(volt)	\
	((volt % 625) ? (volt - (volt % 625) + 625) : volt)
#ifndef MIN
#define MIN(x, y)	(((x) < (y)) ? (x) : (y))
#endif

#define GPUOP(khz, vgpu, vsram, post_divider, aging_margin)	\
	{							\
		.gpufreq_khz = khz,				\
		.gpufreq_vgpu = vgpu,				\
		.gpufreq_vsram = vsram,				\
		.gpufreq_post_divider = post_divider,		\
		.gpufreq_aging_margin = aging_margin,		\
	}

/**************************************************
 * Enumerations
 **************************************************/
enum g_segment_id_enum {
	MT6883_SEGMENT = 1,
	MT6885_SEGMENT,
	MT6885T_SEGMENT,
	MT6885T_PLUS_SEGMENT,
};

enum g_posdiv_power_enum  {
	POSDIV_POWER_1 = 0,
	POSDIV_POWER_2,
	POSDIV_POWER_4,
	POSDIV_POWER_8,
	POSDIV_POWER_16,
};
enum g_clock_source_enum  {
	CLOCK_MAIN = 0,
	CLOCK_SUB,
};

enum g_limit_enable_enum  {
	LIMIT_DISABLE = 0,
	LIMIT_ENABLE,
};

enum {
	GPUFREQ_LIMIT_PRIO_NONE,	/* the lowest priority */
	GPUFREQ_LIMIT_PRIO_1,
	GPUFREQ_LIMIT_PRIO_2,
	GPUFREQ_LIMIT_PRIO_3,
	GPUFREQ_LIMIT_PRIO_4,
	GPUFREQ_LIMIT_PRIO_5,
	GPUFREQ_LIMIT_PRIO_6,
	GPUFREQ_LIMIT_PRIO_7,
	GPUFREQ_LIMIT_PRIO_8		/* the highest priority */
};

struct gpudvfs_limit {
	unsigned int kicker;
	char *name;
	unsigned int prio;
	unsigned int upper_idx;
	unsigned int upper_enable;
	unsigned int lower_idx;
	unsigned int lower_enable;
};

#define LIMIT_IDX_DEFAULT -1

struct gpudvfs_limit limit_table[] = {
	{KIR_STRESS,		"STRESS",	GPUFREQ_LIMIT_PRIO_8,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_PROC,			"PROC",		GPUFREQ_LIMIT_PRIO_7,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_PTPOD,			"PTPOD",	GPUFREQ_LIMIT_PRIO_6,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_THERMAL,		"THERMAL",	GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_BATT_OC,		"BATT_OC",	GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_BATT_LOW,		"BATT_LOW",	GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_BATT_PERCENT,	"BATT_PERCENT",	GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_PBM,			"PBM",		GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_POLICY,		"POLICY",	GPUFREQ_LIMIT_PRIO_4,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
};

/**************************************************
 * Structures
 **************************************************/
struct opp_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_vgpu;
	unsigned int gpufreq_vsram;
	enum g_posdiv_power_enum gpufreq_post_divider;
	unsigned int gpufreq_aging_margin;
};
struct g_clk_info {
	/* main clock for mfg setting */
	struct clk *clk_mux;
	/* substitution clock for mfg transient mux setting */
	struct clk *clk_main_parent;
	/* substitution clock for mfg transient parent setting */
	struct clk *clk_sub_parent;
	/* clock gate, which has only two state with ON or OFF */
	struct clk *subsys_mfg_cg;
	struct clk *mtcmos_mfg_async;
	/* mtcmos_mfg dependent on mtcmos_mfg_async */
	struct clk *mtcmos_mfg;
	/* mtcmos_mfg_core0 dependent on mtcmos_mfg0 */
	struct clk *mtcmos_mfg_core0;
	/* mtcmos_mfg_core1_2 dependent on mtcmos_mfg1/0 */
	struct clk *mtcmos_mfg_core1_2;
	/* mtcmos_mfg_core3_4 dependent on mtcmos_mfg1 */
	struct clk *mtcmos_mfg_core3_4;
	/* mtcmos_mfg_core5_6 dependent on mtcmos_mfg1 */
	struct clk *mtcmos_mfg_core5_6;
	/* mtcmos_mfg_core7_8 dependent on mtcmos_mfg1 */
	struct clk *mtcmos_mfg_core7_8;
};
struct g_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram_gpu;
};

/**************************************************
 * External functions declaration
 **************************************************/
extern bool mtk_get_gpu_loading(unsigned int *pLoading);
extern unsigned int mt_get_ckgen_freq(unsigned int idx);

/**************************************************
 * global value definition
 **************************************************/
struct opp_table_info *g_opp_table;

/**************************************************
 * PTPOD definition
 **************************************************/
unsigned int g_ptpod_opp_idx_table_segment[] = {
	0, 3, 6, 8,
	10, 12, 14, 16,
	18, 20, 22, 24,
	26, 28, 30, 32
};

/**************************************************
 * GPU OPP table definition
 **************************************************/
struct opp_table_info g_opp_table_segment_1[] = {
	GPUOP(836000, 75000, 75000, POSDIV_POWER_4, 1875), /* 0 sign off */
	GPUOP(825000, 74375, 75000, POSDIV_POWER_4, 1875), /* 1 */
	GPUOP(815000, 73750, 75000, POSDIV_POWER_4, 1875), /* 2 */
	GPUOP(805000, 73125, 75000, POSDIV_POWER_4, 1875), /* 3 */
	GPUOP(795000, 72500, 75000, POSDIV_POWER_4, 1875), /* 4 */
	GPUOP(785000, 71875, 75000, POSDIV_POWER_4, 1875), /* 5 */
	GPUOP(775000, 71250, 75000, POSDIV_POWER_4, 1875), /* 6 */
	GPUOP(765000, 70625, 75000, POSDIV_POWER_4, 1875), /* 7 */
	GPUOP(755000, 70000, 75000, POSDIV_POWER_4, 1875), /* 8 */
	GPUOP(745000, 69375, 75000, POSDIV_POWER_4, 1875), /* 9 */
	GPUOP(735000, 68750, 75000, POSDIV_POWER_4, 1875), /*10 */
	GPUOP(725000, 68125, 75000, POSDIV_POWER_4, 1875), /*11 */
	GPUOP(715000, 67500, 75000, POSDIV_POWER_4, 1875), /*12 */
	GPUOP(705000, 66875, 75000, POSDIV_POWER_4, 1875), /*13 */
	GPUOP(695000, 66250, 75000, POSDIV_POWER_4, 1875), /*14 */
	GPUOP(685000, 65625, 75000, POSDIV_POWER_4, 1875), /*15 */
	GPUOP(675000, 65000, 75000, POSDIV_POWER_4, 1875), /*16 sign off */
	GPUOP(654000, 65000, 75000, POSDIV_POWER_4, 1250), /*17 */
	GPUOP(634000, 64375, 75000, POSDIV_POWER_4, 1250), /*18 */
	GPUOP(614000, 63750, 75000, POSDIV_POWER_4, 1250), /*19 */
	GPUOP(593000, 63125, 75000, POSDIV_POWER_4, 1250), /*20 */
	GPUOP(573000, 62500, 75000, POSDIV_POWER_4, 1250), /*21 */
	GPUOP(553000, 62500, 75000, POSDIV_POWER_4, 1250), /*22 */
	GPUOP(532000, 61875, 75000, POSDIV_POWER_4, 1250), /*23 */
	GPUOP(512000, 61250, 75000, POSDIV_POWER_4,  625), /*24 */
	GPUOP(492000, 60625, 75000, POSDIV_POWER_4,  625), /*25 */
	GPUOP(471000, 60000, 75000, POSDIV_POWER_4,  625), /*26 */
	GPUOP(451000, 60000, 75000, POSDIV_POWER_4,  625), /*27 */
	GPUOP(431000, 59375, 75000, POSDIV_POWER_4,  625), /*28 */
	GPUOP(410000, 58750, 75000, POSDIV_POWER_4,  625), /*29 */
	GPUOP(390000, 58125, 75000, POSDIV_POWER_4,  625), /*30 */
	GPUOP(370000, 57500, 75000, POSDIV_POWER_8,  625), /*31 */
	GPUOP(350000, 56875, 75000, POSDIV_POWER_8,  625), /*32 sign off */
};

struct opp_table_info g_opp_table_segment_2[] = {
	GPUOP(836000, 75000, 75000, POSDIV_POWER_4, 1875), /* 0 sign off */
	GPUOP(825000, 74375, 75000, POSDIV_POWER_4, 1875), /* 1 */
	GPUOP(815000, 73750, 75000, POSDIV_POWER_4, 1875), /* 2 */
	GPUOP(805000, 73125, 75000, POSDIV_POWER_4, 1875), /* 3 */
	GPUOP(795000, 72500, 75000, POSDIV_POWER_4, 1875), /* 4 */
	GPUOP(785000, 71875, 75000, POSDIV_POWER_4, 1875), /* 5 */
	GPUOP(775000, 71250, 75000, POSDIV_POWER_4, 1875), /* 6 */
	GPUOP(765000, 70625, 75000, POSDIV_POWER_4, 1875), /* 7 */
	GPUOP(755000, 70000, 75000, POSDIV_POWER_4, 1875), /* 8 */
	GPUOP(745000, 69375, 75000, POSDIV_POWER_4, 1875), /* 9 */
	GPUOP(735000, 68750, 75000, POSDIV_POWER_4, 1875), /*10 */
	GPUOP(725000, 68125, 75000, POSDIV_POWER_4, 1875), /*11 */
	GPUOP(715000, 67500, 75000, POSDIV_POWER_4, 1875), /*12 */
	GPUOP(705000, 66875, 75000, POSDIV_POWER_4, 1875), /*13 */
	GPUOP(695000, 66250, 75000, POSDIV_POWER_4, 1875), /*14 */
	GPUOP(685000, 65625, 75000, POSDIV_POWER_4, 1875), /*15 */
	GPUOP(675000, 65000, 75000, POSDIV_POWER_4, 1875), /*16 sign off */
	GPUOP(654000, 65000, 75000, POSDIV_POWER_4, 1250), /*17 */
	GPUOP(634000, 65000, 75000, POSDIV_POWER_4, 1250), /*18 */
	GPUOP(614000, 65000, 75000, POSDIV_POWER_4, 1250), /*19 */
	GPUOP(593000, 64375, 75000, POSDIV_POWER_4, 1250), /*20 */
	GPUOP(573000, 64375, 75000, POSDIV_POWER_4, 1250), /*21 */
	GPUOP(553000, 64375, 75000, POSDIV_POWER_4, 1250), /*22 */
	GPUOP(532000, 63750, 75000, POSDIV_POWER_4, 1250), /*23 */
	GPUOP(512000, 63750, 75000, POSDIV_POWER_4,  625), /*24 */
	GPUOP(492000, 63750, 75000, POSDIV_POWER_4,  625), /*25 */
	GPUOP(471000, 63125, 75000, POSDIV_POWER_4,  625), /*26 */
	GPUOP(451000, 63125, 75000, POSDIV_POWER_4,  625), /*27 */
	GPUOP(431000, 63125, 75000, POSDIV_POWER_4,  625), /*28 */
	GPUOP(410000, 62500, 75000, POSDIV_POWER_4,  625), /*29 */
	GPUOP(390000, 62500, 75000, POSDIV_POWER_4,  625), /*30 */
	GPUOP(370000, 62500, 75000, POSDIV_POWER_8,  625), /*31 */
	GPUOP(350000, 61875, 75000, POSDIV_POWER_8,  625), /*32 sign off */
};

#endif /* ___MT_GPUFREQ_INTERNAL_PLAT_H___ */
