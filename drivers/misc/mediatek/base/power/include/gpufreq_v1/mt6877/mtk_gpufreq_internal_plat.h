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
#define MT_GPUFREQ_POWER_CTL_ENABLE     1

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
#define VGPU_MAX_VOLT                   (119375)        /* mV x 100 */
#define VGPU_MIN_VOLT                   (40000)         /* mV x 100 */
#define VSRAM_GPU_MAX_VOLT              (129375)        /* mV x 100 */
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
#define MFGPLL1_CON0_OFS                (0x08)
#define MFGPLL1_CON1_OFS                (0x0C)
#define MFGPLL1_CON2_OFS                (0x10)
#define MFGPLL1_CON3_OFS                (0x14)
#define MFGPLL4_CON0_OFS                (0x38)
#define MFGPLL4_CON1_OFS                (0x3C)
#define MFGPLL4_CON2_OFS                (0x40)
#define MFGPLL4_CON3_OFS                (0x44)
#define MFGPLL1_CON0                    (g_gpu_pll_ctrl + MFGPLL1_CON0_OFS)
#define MFGPLL1_CON1                    (g_gpu_pll_ctrl + MFGPLL1_CON1_OFS)
#define MFGPLL1_CON2                    (g_gpu_pll_ctrl + MFGPLL1_CON2_OFS)
#define MFGPLL1_CON3                    (g_gpu_pll_ctrl + MFGPLL1_CON3_OFS)
#define MFGPLL4_CON0                    (g_gpu_pll_ctrl + MFGPLL4_CON0_OFS)
#define MFGPLL4_CON1                    (g_gpu_pll_ctrl + MFGPLL4_CON1_OFS)
#define MFGPLL4_CON2                    (g_gpu_pll_ctrl + MFGPLL4_CON2_OFS)
#define MFGPLL4_CON3                    (g_gpu_pll_ctrl + MFGPLL4_CON3_OFS)
#define PLL4H_FQMTR_CON0_OFS            (0x200)
#define PLL4H_FQMTR_CON1_OFS            (0x204)
#define PWR_STATUS_OFS                  (0xEF8)
#define PWR_STATUS_2ND_OFS              (0xEFC)

/**************************************************
 * Reference Power Setting
 **************************************************/
#define GPU_ACT_REF_POWER               (1223)                /* mW  */
#define GPU_ACT_REF_FREQ                (950000)              /* KHz */
#define GPU_ACT_REF_VOLT                (78125)               /* mV x 100 */

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
#define MT_GPUFREQ_DFD_DEBUG  0

/**************************************************
 * GPM (GIMP: GPU Idle To Max Protector)
 **************************************************/
#define MT_GPUFREQ_GPM_ENABLE 0

/**************************************************
 * ASENSOR (Aging Sensor) = Sensor network0
 **************************************************/
#define MT_GPUFREQ_ASENSOR_ENABLE		1
#define ASENSOR_DISABLE_VOLT            (55000)
#define MT_GPUFREQ_AGING_GAP0			(-3)
#define MT_GPUFREQ_AGING_GAP1			(2)
#define MT_GPUFREQ_AGING_GAP2			(4)
#define MT_GPUFREQ_AGING_GAP3			(6)

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
	((volt % PMIC_STEP) ? (volt - (volt % PMIC_STEP) + PMIC_STEP) : volt)

#ifndef MAX
#define MAX(x, y)	(((x) < (y)) ? (y) : (x))
#endif

#ifndef MIN
#define MIN(x, y)	(((x) < (y)) ? (x) : (y))
#endif

static int ceil(float a)
{
	if (a == (int)(a))
		return a;
	else
		return (int)(a)+1;
}

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
	MT6877_SEGMENT = 1,
	MT6877T_SEGMENT,
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
	CLOCK_SUB2,
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
	{KIR_ASENSOR,			"ASENSOR",	GPUFREQ_LIMIT_PRIO_6,
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
	struct clk *clk_pll4;
	struct clk *subsys_bg3d;
	struct clk *mtcmos_mfg0;
	struct clk *mtcmos_mfg1;
	struct clk *mtcmos_mfg2;
	struct clk *mtcmos_mfg3;
	struct clk *mtcmos_mfg4;
	struct clk *mtcmos_mfg5;
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
	 0,  3,  5,  9,
	12, 15, 18, 21,
	23, 25, 27, 29,
	31, 33, 35, 37
};

/**************************************************
 * GPU OPP table definition
 **************************************************/
struct opp_table_info g_opp_table_segment_1[] = {
	GPUOP(950000, 78125, 78125, POSDIV_POWER_4, 1875), /* 0 sign off */
	GPUOP(938000, 77500, 77500, POSDIV_POWER_4, 1875), /* 1 */
	GPUOP(926000, 76875, 76875, POSDIV_POWER_4, 1875), /* 2 */
	GPUOP(914000, 76250, 76250, POSDIV_POWER_4, 1875), /* 3 */
	GPUOP(902000, 75625, 75625, POSDIV_POWER_4, 1875), /* 4 */
	GPUOP(890000, 75000, 75000, POSDIV_POWER_4, 1875), /* 5 sign off */
	GPUOP(876000, 74375, 75000, POSDIV_POWER_4, 1875), /* 6 */
	GPUOP(862000, 73750, 75000, POSDIV_POWER_4, 1875), /* 7 */
	GPUOP(848000, 73125, 75000, POSDIV_POWER_4, 1875), /* 8 */
	GPUOP(835000, 72500, 75000, POSDIV_POWER_4, 1875), /* 9 */
	GPUOP(821000, 71875, 75000, POSDIV_POWER_4, 1875), /*10 */
	GPUOP(807000, 71250, 75000, POSDIV_POWER_4, 1875), /*11 */
	GPUOP(793000, 70625, 75000, POSDIV_POWER_4, 1875), /*12 */
	GPUOP(780000, 70000, 75000, POSDIV_POWER_4, 1875), /*13 */
	GPUOP(766000, 69375, 75000, POSDIV_POWER_4, 1875), /*14 */
	GPUOP(752000, 68750, 75000, POSDIV_POWER_4, 1250), /*15 */
	GPUOP(738000, 68125, 75000, POSDIV_POWER_4, 1250), /*16 */
	GPUOP(725000, 67500, 75000, POSDIV_POWER_4, 1250), /*17 */
	GPUOP(711000, 66875, 75000, POSDIV_POWER_4, 1250), /*18 */
	GPUOP(697000, 66250, 75000, POSDIV_POWER_4, 1250), /*19 */
	GPUOP(683000, 65625, 75000, POSDIV_POWER_4, 1250), /*20 */
	GPUOP(670000, 65000, 75000, POSDIV_POWER_4, 1250), /*21 sign off */
	GPUOP(652000, 64375, 75000, POSDIV_POWER_4, 1250), /*22 */
	GPUOP(634000, 63750, 75000, POSDIV_POWER_4, 1250), /*23 */
	GPUOP(616000, 63125, 75000, POSDIV_POWER_4, 1250), /*24 */
	GPUOP(598000, 62500, 75000, POSDIV_POWER_4, 1250), /*25 */
	GPUOP(580000, 61875, 75000, POSDIV_POWER_4, 1250), /*26 */
	GPUOP(563000, 61250, 75000, POSDIV_POWER_4,  625), /*27 */
	GPUOP(545000, 60625, 75000, POSDIV_POWER_4,  625), /*28 */
	GPUOP(527000, 60000, 75000, POSDIV_POWER_4,  625), /*29 */
	GPUOP(509000, 59375, 75000, POSDIV_POWER_4,  625), /*30 */
	GPUOP(491000, 58750, 75000, POSDIV_POWER_4,  625), /*31 */
	GPUOP(474000, 58125, 75000, POSDIV_POWER_4,  625), /*32 */
	GPUOP(456000, 57500, 75000, POSDIV_POWER_4,  625), /*33 */
	GPUOP(438000, 56875, 75000, POSDIV_POWER_4,  625), /*34 */
	GPUOP(420000, 56250, 75000, POSDIV_POWER_4,  625), /*35 */
	GPUOP(402000, 55625, 75000, POSDIV_POWER_4,  625), /*36 */
	GPUOP(385000, 55000, 75000, POSDIV_POWER_4,  625), /*37 sign off */
};

struct opp_table_info g_opp_table_segment_2[] = {
	GPUOP(950000, 78125, 78125, POSDIV_POWER_4, 1875), /* 0 sign off */
	GPUOP(938000, 77500, 77500, POSDIV_POWER_4, 1875), /* 1 */
	GPUOP(926000, 76875, 76875, POSDIV_POWER_4, 1875), /* 2 */
	GPUOP(914000, 76250, 76250, POSDIV_POWER_4, 1875), /* 3 */
	GPUOP(902000, 75625, 75625, POSDIV_POWER_4, 1875), /* 4 */
	GPUOP(890000, 75000, 75000, POSDIV_POWER_4, 1875), /* 5 sign off */
	GPUOP(876000, 74375, 75000, POSDIV_POWER_4, 1875), /* 6 */
	GPUOP(862000, 73750, 75000, POSDIV_POWER_4, 1875), /* 7 */
	GPUOP(848000, 73125, 75000, POSDIV_POWER_4, 1875), /* 8 */
	GPUOP(835000, 72500, 75000, POSDIV_POWER_4, 1875), /* 9 */
	GPUOP(821000, 71875, 75000, POSDIV_POWER_4, 1875), /*10 */
	GPUOP(807000, 71250, 75000, POSDIV_POWER_4, 1875), /*11 */
	GPUOP(793000, 70625, 75000, POSDIV_POWER_4, 1875), /*12 */
	GPUOP(780000, 70000, 75000, POSDIV_POWER_4, 1875), /*13 */
	GPUOP(766000, 69375, 75000, POSDIV_POWER_4, 1875), /*14 */
	GPUOP(752000, 68750, 75000, POSDIV_POWER_4, 1250), /*15 */
	GPUOP(738000, 68125, 75000, POSDIV_POWER_4, 1250), /*16 */
	GPUOP(725000, 67500, 75000, POSDIV_POWER_4, 1250), /*17 */
	GPUOP(711000, 66875, 75000, POSDIV_POWER_4, 1250), /*18 */
	GPUOP(697000, 66250, 75000, POSDIV_POWER_4, 1250), /*19 */
	GPUOP(683000, 65625, 75000, POSDIV_POWER_4, 1250), /*20 */
	GPUOP(670000, 65000, 75000, POSDIV_POWER_4, 1250), /*21 sign off */
	GPUOP(652000, 65000, 75000, POSDIV_POWER_4, 1250), /*22 */
	GPUOP(634000, 64375, 75000, POSDIV_POWER_4, 1250), /*23 */
	GPUOP(616000, 63750, 75000, POSDIV_POWER_4, 1250), /*24 */
	GPUOP(598000, 63125, 75000, POSDIV_POWER_4, 1250), /*25 */
	GPUOP(580000, 63125, 75000, POSDIV_POWER_4, 1250), /*26 */
	GPUOP(563000, 62500, 75000, POSDIV_POWER_4,  625), /*27 */
	GPUOP(545000, 61875, 75000, POSDIV_POWER_4,  625), /*28 */
	GPUOP(527000, 61250, 75000, POSDIV_POWER_4,  625), /*29 */
	GPUOP(509000, 61250, 75000, POSDIV_POWER_4,  625), /*30 */
	GPUOP(491000, 60625, 75000, POSDIV_POWER_4,  625), /*31 */
	GPUOP(474000, 60000, 75000, POSDIV_POWER_4,  625), /*32 */
	GPUOP(456000, 59375, 75000, POSDIV_POWER_4,  625), /*33 */
	GPUOP(438000, 59375, 75000, POSDIV_POWER_4,  625), /*34 */
	GPUOP(420000, 58750, 75000, POSDIV_POWER_4,  625), /*35 */
	GPUOP(402000, 58125, 75000, POSDIV_POWER_4,  625), /*36 */
	GPUOP(385000, 57500, 75000, POSDIV_POWER_4,  625), /*37 sign off */
};

struct opp_table_info g_opp_table_segment_3[] = {
	GPUOP(950000, 78125, 78125, POSDIV_POWER_4, 1875), /* 0 sign off */
	GPUOP(938000, 77500, 77500, POSDIV_POWER_4, 1875), /* 1 */
	GPUOP(926000, 76875, 76875, POSDIV_POWER_4, 1875), /* 2 */
	GPUOP(914000, 76250, 76250, POSDIV_POWER_4, 1875), /* 3 */
	GPUOP(902000, 75625, 75625, POSDIV_POWER_4, 1875), /* 4 */
	GPUOP(890000, 75000, 75000, POSDIV_POWER_4, 1875), /* 5 sign off */
	GPUOP(876000, 74375, 75000, POSDIV_POWER_4, 1875), /* 6 */
	GPUOP(862000, 73750, 75000, POSDIV_POWER_4, 1875), /* 7 */
	GPUOP(848000, 73125, 75000, POSDIV_POWER_4, 1875), /* 8 */
	GPUOP(835000, 72500, 75000, POSDIV_POWER_4, 1875), /* 9 */
	GPUOP(821000, 71875, 75000, POSDIV_POWER_4, 1875), /*10 */
	GPUOP(807000, 71250, 75000, POSDIV_POWER_4, 1875), /*11 */
	GPUOP(793000, 70625, 75000, POSDIV_POWER_4, 1875), /*12 */
	GPUOP(780000, 70000, 75000, POSDIV_POWER_4, 1875), /*13 */
	GPUOP(766000, 69375, 75000, POSDIV_POWER_4, 1875), /*14 */
	GPUOP(752000, 68750, 75000, POSDIV_POWER_4, 1250), /*15 */
	GPUOP(738000, 68125, 75000, POSDIV_POWER_4, 1250), /*16 */
	GPUOP(725000, 67500, 75000, POSDIV_POWER_4, 1250), /*17 */
	GPUOP(711000, 66875, 75000, POSDIV_POWER_4, 1250), /*18 */
	GPUOP(697000, 66250, 75000, POSDIV_POWER_4, 1250), /*19 */
	GPUOP(683000, 65625, 75000, POSDIV_POWER_4, 1250), /*20 */
	GPUOP(670000, 65000, 75000, POSDIV_POWER_4, 1250), /*21 sign off */
	GPUOP(652000, 65000, 75000, POSDIV_POWER_4, 1250), /*22 */
	GPUOP(634000, 64375, 75000, POSDIV_POWER_4, 1250), /*23 */
	GPUOP(616000, 64375, 75000, POSDIV_POWER_4, 1250), /*24 */
	GPUOP(598000, 63750, 75000, POSDIV_POWER_4, 1250), /*25 */
	GPUOP(580000, 63750, 75000, POSDIV_POWER_4, 1250), /*26 */
	GPUOP(563000, 63125, 75000, POSDIV_POWER_4,  625), /*27 */
	GPUOP(545000, 63125, 75000, POSDIV_POWER_4,  625), /*28 */
	GPUOP(527000, 62500, 75000, POSDIV_POWER_4,  625), /*29 */
	GPUOP(509000, 62500, 75000, POSDIV_POWER_4,  625), /*30 */
	GPUOP(491000, 61875, 75000, POSDIV_POWER_4,  625), /*31 */
	GPUOP(474000, 61875, 75000, POSDIV_POWER_4,  625), /*32 */
	GPUOP(456000, 61250, 75000, POSDIV_POWER_4,  625), /*33 */
	GPUOP(438000, 61250, 75000, POSDIV_POWER_4,  625), /*34 */
	GPUOP(420000, 60625, 75000, POSDIV_POWER_4,  625), /*35 */
	GPUOP(402000, 60625, 75000, POSDIV_POWER_4,  625), /*36 */
	GPUOP(385000, 60000, 75000, POSDIV_POWER_4,  625), /*37 sign off */
};

/**************************************************
 * Aging table
 **************************************************/
struct g_asensor_info {
	u32 efuse_val1, efuse_val2;
	u32 a_t0_lvt_rt, a_t0_ulvt_rt;
	u32 a_tn_lvt_cnt, a_tn_ulvt_cnt;
	int tj1, tj2;
	int adiff1, adiff2;
	int leak_power;
};
unsigned int g_aging_table[][NUM_OF_OPP_IDX] = {
	/* Aging Table 0 */
	{1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875,
	 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1250,
	 1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250,
	 1250, 1250, 1250,  625,  625,  625,  625,  625,
	  625,  625,  625,  625,  625,  625},
	/* Aging Table 1 */
	{1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250,
	 1250, 1250, 1250, 1250, 1250, 1250, 1250,  625,
	  625,  625,  625,  625,  625,  625,  625,  625,
	  625,  625,  625,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0},
	/* Aging Table 2 */
	{ 625,  625,  625,  625,  625,  625,  625,  625,
	  625,  625,  625,  625,  625,  625,  625,    0,
	    0,    0,    0,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0},
	/* Aging Table 3 */
	{   0,    0,    0,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0},
};

#endif /* ___MT_GPUFREQ_INTERNAL_PLAT_H___ */
