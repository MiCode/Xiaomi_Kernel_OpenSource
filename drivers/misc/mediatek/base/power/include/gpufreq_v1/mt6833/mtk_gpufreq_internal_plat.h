/*
 * Copyright (C) 2020 MediaTek Inc.
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
#define MT_GPUFREQ_CUST_INIT_OPP        (g_opp_table_segment_1[16].gpufreq_khz)

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
 * vgpu      0.4 ~ 1.19300 V
 * vsram_gpu 0.5 ~ 1.29300 V
 */
#define VGPU_MAX_VOLT                   (119300)        /* mV x 100 */
#define VGPU_MIN_VOLT                   (40000)         /* mV x 100 */
#define VSRAM_GPU_MAX_VOLT              (129300)        /* mV x 100 */
#define VSRAM_GPU_MIN_VOLT              (50000)         /* mV x 100 */
#define PMIC_STEP                       (625)           /* mV x 100 */
/*
 * (0)mv <= (VSRAM - VGPU) <= (200)mV
 */
#define BUCK_DIFF_MAX                   (20000)         /* mV x 100 */
#define BUCK_DIFF_MIN                   (0)             /* mV x 100 */

/**************************************************
 * Clock Setting
 **************************************************/
#define POSDIV_2_MAX_FREQ               (1900000)       /* KHz */
#define POSDIV_2_MIN_FREQ               (750000)        /* KHz */
#define POSDIV_4_MAX_FREQ               (950000)        /* KHz */
#define POSDIV_4_MIN_FREQ               (375000)        /* KHz */
#define POSDIV_8_MAX_FREQ               (475000)        /* KHz */
#define POSDIV_8_MIN_FREQ               (187500)        /* KHz */
#define POSDIV_16_MAX_FREQ              (237500)        /* KHz */
#define POSDIV_16_MIN_FREQ              (125000)        /* KHz */
#define POSDIV_SHIFT                    (24)            /* bit */
#define DDS_SHIFT                       (14)            /* bit */
#define TO_MHZ_HEAD                     (100)
#define TO_MHZ_TAIL                     (10)
#define ROUNDING_VALUE                  (5)
#define MFGPLL_FIN                      (26)            /* MHz */
#define MFGPLL_FH_PLL                   FH_PLL6
#define MFGPLL_CON0                     (g_apmixed_base + 0x268)
#define MFGPLL_CON1                     (g_apmixed_base + 0x26C)
#define MFGPLL_CON2                     (g_apmixed_base + 0x270)
#define MFGPLL_CON3                     (g_apmixed_base + 0x274)

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
 * GPM (GIMP: GPU Idle To Max Protector)
 **************************************************/
#define MT_GPUFREQ_GPM_ENABLE 0

/**************************************************
 * PTPOD Calibration
 **************************************************/
#define MT_GPUFREQ_PTPOD_CALIBRATION_ENABLE 0

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
	MT6833_SEGMENT = 1,
	MT6833M_SEGMENT,
	MT6833T_SEGMENT,		//Reserve Segment
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
	struct clk *clk_mux;
	struct clk *clk_main_parent;
	struct clk *clk_sub_parent;
	struct clk *subsys_bg3d;
	struct clk *mtcmos_mfg0;
	struct clk *mtcmos_mfg1;
	struct clk *mtcmos_mfg2;
	struct clk *mtcmos_mfg3;
};
struct g_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram_gpu;
};

/**************************************************
 * External functions declaration
 **************************************************/
extern bool mtk_get_gpu_loading(unsigned int *pLoading);
extern unsigned int mt_get_abist_freq(unsigned int idx);

/**************************************************
 * global value definition
 **************************************************/
struct opp_table_info *g_opp_table;

/**************************************************
 * PTPOD definition
 **************************************************/
unsigned int g_ptpod_opp_idx_table_segment[] = {
	0, 4, 8, 12,
	16, 20, 23, 26,
	29, 32, 34, 36,
	38, 40, 42, 44
};

/**************************************************
 * GPU OPP table definition
 **************************************************/
struct opp_table_info g_opp_table_segment_1[] = {
	GPUOP(1068000, 85000, 85000, POSDIV_POWER_2, 1875), /* 0 sign off */
	GPUOP(1059000, 84375, 84375, POSDIV_POWER_2, 1875), /* 1 */
	GPUOP(1051000, 83750, 83750, POSDIV_POWER_2, 1875), /* 2 */
	GPUOP(1042000, 83125, 83125, POSDIV_POWER_2, 1875), /* 3 */
	GPUOP(1034000, 82500, 82500, POSDIV_POWER_2, 1875), /* 4 */
	GPUOP(1025000, 81875, 81875, POSDIV_POWER_2, 1875), /* 5 */
	GPUOP(1017000, 81250, 81250, POSDIV_POWER_2, 1875), /* 6 */
	GPUOP(1008000, 80625, 80625, POSDIV_POWER_2, 1875), /* 7 */
	GPUOP(1000000, 80000, 80000, POSDIV_POWER_2, 1875), /* 8 sign off */
	GPUOP(985000,  79375, 79375, POSDIV_POWER_2, 1875), /* 9 */
	GPUOP(970000,  78750, 78750, POSDIV_POWER_2, 1875), /*10 */
	GPUOP(955000,  78125, 78125, POSDIV_POWER_2, 1875), /*11 */
	GPUOP(940000,  77500, 77500, POSDIV_POWER_4, 1875), /*12 */
	GPUOP(925000,  76875, 76875, POSDIV_POWER_4, 1875), /*13 */
	GPUOP(910000,  76250, 76250, POSDIV_POWER_4, 1875), /*14 */
	GPUOP(895000,  75625, 75625, POSDIV_POWER_4, 1875), /*15 */
	GPUOP(880000,  75000, 75000, POSDIV_POWER_4, 1875), /*16 sign off */
	GPUOP(868000,  74375, 75000, POSDIV_POWER_4, 1875), /*17 */
	GPUOP(857000,  73750, 75000, POSDIV_POWER_4, 1875), /*18 */
	GPUOP(846000,  73125, 75000, POSDIV_POWER_4, 1875), /*19 */
	GPUOP(835000,  72500, 75000, POSDIV_POWER_4, 1250), /*20 */
	GPUOP(823000,  71875, 75000, POSDIV_POWER_4, 1250), /*21 */
	GPUOP(812000,  71250, 75000, POSDIV_POWER_4, 1250), /*22 */
	GPUOP(801000,  70625, 75000, POSDIV_POWER_4, 1250), /*23 */
	GPUOP(790000,  70000, 75000, POSDIV_POWER_4, 1250), /*24 */
	GPUOP(778000,  69375, 75000, POSDIV_POWER_4, 1250), /*25 */
	GPUOP(767000,  68750, 75000, POSDIV_POWER_4, 1250), /*26 */
	GPUOP(756000,  68125, 75000, POSDIV_POWER_4, 1250), /*27 */
	GPUOP(745000,  67500, 75000, POSDIV_POWER_4, 1250), /*28 */
	GPUOP(733000,  66875, 75000, POSDIV_POWER_4, 1250), /*29 */
	GPUOP(722000,  66250, 75000, POSDIV_POWER_4, 1250), /*30 */
	GPUOP(711000,  65625, 75000, POSDIV_POWER_4, 1250), /*31 */
	GPUOP(700000,  65000, 75000, POSDIV_POWER_4, 1250), /*32 sign off */
	GPUOP(674000,  64375, 75000, POSDIV_POWER_4, 1250), /*33 */
	GPUOP(648000,  63750, 75000, POSDIV_POWER_4, 1250), /*34 */
	GPUOP(622000,  63125, 75000, POSDIV_POWER_4, 1250), /*35 */
	GPUOP(596000,  62500, 75000, POSDIV_POWER_4,  625), /*36 */
	GPUOP(570000,  61875, 75000, POSDIV_POWER_4,  625), /*37 */
	GPUOP(545000,  61250, 75000, POSDIV_POWER_4,  625), /*38 */
	GPUOP(519000,  60625, 75000, POSDIV_POWER_4,  625), /*39 */
	GPUOP(493000,  60000, 75000, POSDIV_POWER_4,  625), /*40 */
	GPUOP(467000,  59375, 75000, POSDIV_POWER_4,  625), /*41 */
	GPUOP(441000,  58750, 75000, POSDIV_POWER_4,  625), /*42 */
	GPUOP(415000,  58125, 75000, POSDIV_POWER_4,  625), /*43 */
	GPUOP(390000,  57500, 75000, POSDIV_POWER_4,  625), /*44 sign off */
};

struct opp_table_info g_opp_table_segment_2[] = {
	GPUOP(1068000, 85000, 85000, POSDIV_POWER_2, 1875), /* 0 sign off */
	GPUOP(1059000, 84375, 84375, POSDIV_POWER_2, 1875), /* 1 */
	GPUOP(1051000, 83750, 83750, POSDIV_POWER_2, 1875), /* 2 */
	GPUOP(1042000, 83125, 83125, POSDIV_POWER_2, 1875), /* 3 */
	GPUOP(1034000, 82500, 82500, POSDIV_POWER_2, 1875), /* 4 */
	GPUOP(1025000, 81875, 81875, POSDIV_POWER_2, 1875), /* 5 */
	GPUOP(1017000, 81250, 81250, POSDIV_POWER_2, 1875), /* 6 */
	GPUOP(1008000, 80625, 80625, POSDIV_POWER_2, 1875), /* 7 */
	GPUOP(1000000, 80000, 80000, POSDIV_POWER_2, 1875), /* 8 sign off */
	GPUOP(985000,  79375, 79375, POSDIV_POWER_2, 1875), /* 9 */
	GPUOP(970000,  78750, 78750, POSDIV_POWER_2, 1875), /*10 */
	GPUOP(955000,  78125, 78125, POSDIV_POWER_2, 1875), /*11 */
	GPUOP(940000,  77500, 77500, POSDIV_POWER_4, 1875), /*12 */
	GPUOP(925000,  76875, 76875, POSDIV_POWER_4, 1875), /*13 */
	GPUOP(910000,  76250, 76250, POSDIV_POWER_4, 1875), /*14 */
	GPUOP(895000,  75625, 75625, POSDIV_POWER_4, 1875), /*15 */
	GPUOP(880000,  75000, 75000, POSDIV_POWER_4, 1875), /*16 sign off */
	GPUOP(868000,  74375, 75000, POSDIV_POWER_4, 1875), /*17 */
	GPUOP(857000,  73750, 75000, POSDIV_POWER_4, 1875), /*18 */
	GPUOP(846000,  73125, 75000, POSDIV_POWER_4, 1875), /*19 */
	GPUOP(835000,  72500, 75000, POSDIV_POWER_4, 1250), /*20 */
	GPUOP(823000,  71875, 75000, POSDIV_POWER_4, 1250), /*21 */
	GPUOP(812000,  71250, 75000, POSDIV_POWER_4, 1250), /*22 */
	GPUOP(801000,  70625, 75000, POSDIV_POWER_4, 1250), /*23 */
	GPUOP(790000,  70000, 75000, POSDIV_POWER_4, 1250), /*24 */
	GPUOP(778000,  69375, 75000, POSDIV_POWER_4, 1250), /*25 */
	GPUOP(767000,  68750, 75000, POSDIV_POWER_4, 1250), /*26 */
	GPUOP(756000,  68125, 75000, POSDIV_POWER_4, 1250), /*27 */
	GPUOP(745000,  67500, 75000, POSDIV_POWER_4, 1250), /*28 */
	GPUOP(733000,  66875, 75000, POSDIV_POWER_4, 1250), /*29 */
	GPUOP(722000,  66250, 75000, POSDIV_POWER_4, 1250), /*30 */
	GPUOP(711000,  65625, 75000, POSDIV_POWER_4, 1250), /*31 */
	GPUOP(700000,  65000, 75000, POSDIV_POWER_4, 1250), /*32 sign off */
	GPUOP(674000,  65000, 75000, POSDIV_POWER_4, 1250), /*33 */
	GPUOP(648000,  64375, 75000, POSDIV_POWER_4, 1250), /*34 */
	GPUOP(622000,  63750, 75000, POSDIV_POWER_4, 1250), /*35 */
	GPUOP(596000,  63750, 75000, POSDIV_POWER_4,  625), /*36 */
	GPUOP(570000,  63125, 75000, POSDIV_POWER_4,  625), /*37 */
	GPUOP(545000,  62500, 75000, POSDIV_POWER_4,  625), /*38 */
	GPUOP(519000,  62500, 75000, POSDIV_POWER_4,  625), /*39 */
	GPUOP(493000,  61875, 75000, POSDIV_POWER_4,  625), /*40 */
	GPUOP(467000,  61250, 75000, POSDIV_POWER_4,  625), /*41 */
	GPUOP(441000,  61250, 75000, POSDIV_POWER_4,  625), /*42 */
	GPUOP(415000,  60625, 75000, POSDIV_POWER_4,  625), /*43 */
	GPUOP(390000,  60000, 75000, POSDIV_POWER_4,  625), /*44 sign off */
};

struct opp_table_info g_opp_table_segment_3[] = {
	GPUOP(1068000, 85000, 85000, POSDIV_POWER_2, 1875), /* 0 sign off */
	GPUOP(1059000, 84375, 84375, POSDIV_POWER_2, 1875), /* 1 */
	GPUOP(1051000, 83750, 83750, POSDIV_POWER_2, 1875), /* 2 */
	GPUOP(1042000, 83125, 83125, POSDIV_POWER_2, 1875), /* 3 */
	GPUOP(1034000, 82500, 82500, POSDIV_POWER_2, 1875), /* 4 */
	GPUOP(1025000, 81875, 81875, POSDIV_POWER_2, 1875), /* 5 */
	GPUOP(1017000, 81250, 81250, POSDIV_POWER_2, 1875), /* 6 */
	GPUOP(1008000, 80625, 80625, POSDIV_POWER_2, 1875), /* 7 */
	GPUOP(1000000, 80000, 80000, POSDIV_POWER_2, 1875), /* 8 sign off */
	GPUOP(985000,  79375, 79375, POSDIV_POWER_2, 1875), /* 9 */
	GPUOP(970000,  78750, 78750, POSDIV_POWER_2, 1875), /*10 */
	GPUOP(955000,  78125, 78125, POSDIV_POWER_2, 1875), /*11 */
	GPUOP(940000,  77500, 77500, POSDIV_POWER_4, 1875), /*12 */
	GPUOP(925000,  76875, 76875, POSDIV_POWER_4, 1875), /*13 */
	GPUOP(910000,  76250, 76250, POSDIV_POWER_4, 1875), /*14 */
	GPUOP(895000,  75625, 75625, POSDIV_POWER_4, 1875), /*15 */
	GPUOP(880000,  75000, 75000, POSDIV_POWER_4, 1875), /*16 sign off */
	GPUOP(868000,  74375, 75000, POSDIV_POWER_4, 1875), /*17 */
	GPUOP(857000,  73750, 75000, POSDIV_POWER_4, 1875), /*18 */
	GPUOP(846000,  73125, 75000, POSDIV_POWER_4, 1875), /*19 */
	GPUOP(835000,  72500, 75000, POSDIV_POWER_4, 1250), /*20 */
	GPUOP(823000,  71875, 75000, POSDIV_POWER_4, 1250), /*21 */
	GPUOP(812000,  71250, 75000, POSDIV_POWER_4, 1250), /*22 */
	GPUOP(801000,  70625, 75000, POSDIV_POWER_4, 1250), /*23 */
	GPUOP(790000,  70000, 75000, POSDIV_POWER_4, 1250), /*24 */
	GPUOP(778000,  69375, 75000, POSDIV_POWER_4, 1250), /*25 */
	GPUOP(767000,  68750, 75000, POSDIV_POWER_4, 1250), /*26 */
	GPUOP(756000,  68125, 75000, POSDIV_POWER_4, 1250), /*27 */
	GPUOP(745000,  67500, 75000, POSDIV_POWER_4, 1250), /*28 */
	GPUOP(733000,  66875, 75000, POSDIV_POWER_4, 1250), /*29 */
	GPUOP(722000,  66250, 75000, POSDIV_POWER_4, 1250), /*30 */
	GPUOP(711000,  65625, 75000, POSDIV_POWER_4, 1250), /*31 */
	GPUOP(700000,  65000, 75000, POSDIV_POWER_4, 1250), /*32 sign off */
	GPUOP(674000,  65000, 75000, POSDIV_POWER_4, 1250), /*33 */
	GPUOP(648000,  65000, 75000, POSDIV_POWER_4, 1250), /*34 */
	GPUOP(622000,  64375, 75000, POSDIV_POWER_4, 1250), /*35 */
	GPUOP(596000,  64375, 75000, POSDIV_POWER_4,  625), /*36 */
	GPUOP(570000,  64375, 75000, POSDIV_POWER_4,  625), /*37 */
	GPUOP(545000,  63750, 75000, POSDIV_POWER_4,  625), /*38 */
	GPUOP(519000,  63750, 75000, POSDIV_POWER_4,  625), /*39 */
	GPUOP(493000,  63750, 75000, POSDIV_POWER_4,  625), /*40 */
	GPUOP(467000,  63125, 75000, POSDIV_POWER_4,  625), /*41 */
	GPUOP(441000,  63125, 75000, POSDIV_POWER_4,  625), /*42 */
	GPUOP(415000,  63125, 75000, POSDIV_POWER_4,  625), /*43 */
	GPUOP(390000,  62500, 75000, POSDIV_POWER_4,  625), /*44 sign off */
};

#endif /* ___MT_GPUFREQ_INTERNAL_PLAT_H___ */
