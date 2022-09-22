/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6768_H__
#define __GPUFREQ_MT6768_H__

/**************************************************
 * GPUFREQ Local Config
 **************************************************/
#define GPUFREQ_BRINGUP                 (0)
/*
 * 0 -> power on once then never off and disable DDK power on/off callback
 */
#define GPUFREQ_POWER_CTRL_ENABLE       (1)
/*
 * (DVFS_ENABLE, CUST_INIT)
 * (1, 1) -> DVFS enable and init to CUST_INIT_OPPIDX
 * (1, 0) -> DVFS enable
 * (0, 1) -> DVFS disable but init to CUST_INIT_OPPIDX (do DVFS only onces)
 * (0, 0) -> DVFS disable
 */
#define GPUFREQ_DVFS_ENABLE             (1)
#define GPUFREQ_CUST_INIT_ENABLE        (0)
#define GPUFREQ_CUST_INIT_OPPIDX        (0)

#define PBM_RAEDY                       (0)
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
#define MFGPLL_FH_PLL                   (6)
#define MFGPLL_CON1				        (g_apmixed_base + 0x24C)
//TODO:GKI
#if 0
#define CLK_MUX_OFS                     (0x1034)
#define CKMUX_SEL_REF_CORE              (4)
#define CKMUX_SEL_REF_PARK              (5)
#define CKMUX_SEL_REF_STACK             (7)
#define CKMUX_SEL_REF_STACK_PARK        (8)
#define GPUPLL_ID                       (0x0)
#define STACKPLL_ID                     (0x3)
#endif

/**************************************************
 * Frequency Hopping Setting
 **************************************************/
//TODO:GKI enable when FHCTL ready
#define GPUFREQ_FHCTL_ENABLE            (0)
#define MFG_PLL_NAME                    "mfgpll"

/**************************************************
 * Power Domain Setting
 **************************************************/
#define GPUFREQ_CHECK_MTCMOS_PWR_STATUS (0)
//TODO:GKI
#if 0
#define PWR_STATUS_OFS                  (0xF3C)
#define PWR_STATUS_2ND_OFS              (0xF40)
#define MFG_0_1_PWR_MASK                (0x6)           /* 0000 0110 */
#define MFG_0_3_PWR_MASK                (0x1E)          /* 0001 1110 */
#define MFG_0_4_PWR_MASK                (0x3E)          /* 0011 1110 */
#define MFG_0_5_PWR_MASK                (0x7E)          /* 0111 1110 */
#define MFG_1_5_PWR_MASK                (0x7C)          /* 0111 1100 */
#define MFG_2_5_PWR_MASK                (0x78)          /* 0111 1000 */
#endif

/**************************************************
 * Shader Core Setting
 **************************************************/
#define SHADER_CORE_NUM                 (2)

#define MFG2_SHADER_STACK0              (T0C0)          /* MFG0 */
#define MFG3_SHADER_STACK1              (T1C0)          /* MFG1 */

#define GPU_SHADER_PRESENT_1 \
	(MFG2_SHADER_STACK0)
#define GPU_SHADER_PRESENT_2 \
	(MFG2_SHADER_STACK0 | MFG3_SHADER_STACK1)

struct gpufreq_core_mask_info g_core_mask_table_6768[] = {
	{2, GPU_SHADER_PRESENT_2},
	{1, GPU_SHADER_PRESENT_1},
};

/**************************************************
 * Reference Power Setting MT6768 TBD
 **************************************************/
// TODO:GKI
#define GPU_ACT_REF_POWER			(1285)		/* mW  */
#define GPU_ACT_REF_FREQ			(900000)	/* KHz */
#define GPU_ACT_REF_VOLT			(90000)		/* mV x 100 */
#define GPU_DVFS_PTPOD_DISABLE_VOLT	(80000)		/* mV x 100 */
#define GPU_DVFS_PTPOD_DISABLE_VSRAM_VOLT (90000)
#define GPU_LEAKAGE_POWER               (71)

/**************************************************
 * PMIC Setting MT6768
 **************************************************/
#define VGPU_MAX_VOLT		(95000)         /* mV x 100 */
#define VGPU_MIN_VOLT       (61250)         /* mV x 100 */
#define VSRAM_MAX_VOLT      (105000)        /* mV x 100 */
#define VSRAM_MIN_VOLT      (85000)         /* mV x 100 */
#define DELAY_FACTOR		(625)
/*
 * (0)mv <= (VSRAM - VGPU) <= (250)mV
 */
#define MAX_BUCK_DIFF                   (25000)         /* mV x 100 */
#define MIN_BUCK_DIFF                   (10000)        /* mV x 100 */

/*
 * (Vgpu > THRESH): Vsram = Vgpu + DIFF
 * (Vgpu <= THRESH): Vsram = FIXED_VOLT
 */
#define VSRAM_FIXED_THRESHOLD           (75000)
#define VSRAM_FIXED_VOLT                (85000)
#define VSRAM_FIXED_DIFF                (10000)
#define VOLT_NORMALIZATION(volt) \
	((volt % 625) ? (volt - (volt % 625) + 625) : volt)


/*
 * PMIC hardware range:
 * vgpu      0.4 ~ 1.19300 V (MT6368)
 * vsram     0.5 ~ 1.29300 V (MT6363)
 */
#define PMIC_STEP                       (625)           /* mV x 100 */


/**************************************************
 * SRAMRC Setting
 **************************************************/
#define GPUFREQ_SAFE_VLOGIC             (60000)
//TODO:GKI
#if 0
#define VSRAM_LEVEL_0                   (75000)
#define VSRAM_LEVEL_1                   (80000)
#define SRAM_PARK_VOLT                  (75000)
#endif

/**************************************************
 * Power Throttling Setting
 **************************************************/
//TODO:GKI
#define GPUFREQ_BATT_OC_ENABLE          (0)
#define GPUFREQ_BATT_PERCENT_ENABLE     (0)
#define GPUFREQ_LOW_BATT_ENABLE         (0)
#define GPUFREQ_BATT_OC_FREQ            (474000)
#define GPUFREQ_BATT_PERCENT_IDX        (0)
#define GPUFREQ_LOW_BATT_FREQ           (474000)

/**************************************************
 * AGING Setting
 **************************************************/
#define GPUFREQ_AGING_MOST_AGRRESIVE    (0)

/**************************************************
 * Enumeration MT6768
 **************************************************/
enum gpufreq_segment {
	MT6767_SEGMENT = 1,
	MT6768_SEGMENT,
	MT6769_SEGMENT,
	MT6769T_SEGMENT,
	MT6769Z_SEGMENT,
};

enum gpufreq_clk_src {
	CLOCK_MAIN = 0,
	CLOCK_SUB,
};

/**************************************************
 * Structure MT6768
 **************************************************/
struct g_clk_info {
	struct clk *clk_mux;		/* main clock for mfg setting */
	struct clk *clk_main_parent;	/* substitution clock for mfg transient mux setting */
	struct clk *clk_sub_parent;	/* substitution clock for mfg transient parent setting */
	struct clk *subsys_mfg_cg;	/* clock gating */
};
struct g_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram_gpu;
};

struct gpufreq_mtcmos_info {
	struct device *pd_mfg;
	struct device *pd_mfg_async;
	struct device *pd_mfg_core0;
	struct device *pd_mfg_core1;
};

struct gpufreq_adj_info {
	int oppidx;
	unsigned int freq;
	unsigned int volt;
	unsigned int vsram;
	unsigned int vaging;
};

struct gpufreq_status {
	struct gpufreq_opp_info *signed_table;
	struct gpufreq_opp_info *working_table;
	struct gpufreq_sb_info *sb_table;
	int buck_count;
	int mtcmos_count;
	int cg_count;
	int power_count;
	unsigned int segment_id;
	int signed_opp_num;
	int segment_upbound;
	int segment_lowbound;
	int opp_num;
	int max_oppidx;
	int min_oppidx;
	int cur_oppidx;
	unsigned int cur_freq;
	unsigned int cur_volt;
	unsigned int cur_vsram;
};

/**************************************************
 * GPU Platform OPP Table Definition
 **************************************************/
#define GPUOP(_freq, _volt, _vsram, _posdiv, _vaging, _power) \
	{                                  \
		.freq = _freq,                 \
		.volt = _volt,                 \
		.vsram = _vsram,               \
		.posdiv = _posdiv,             \
		.vaging = _vaging,             \
		.power = _power                \
	}

#define SIGNED_OPP_GPU_NUM              ARRAY_SIZE(g_default_gpu)
struct gpufreq_opp_info g_default_gpu[] = {
	GPUOP(1000000, 95000, 105000, POSDIV_POWER_2, 1875, 0), /*  0 sign off */
	GPUOP(975000, 92500, 102500, POSDIV_POWER_2, 1875, 0), /*  1 */
	GPUOP(950000, 90000, 100000, POSDIV_POWER_2, 1875, 0), /*  2 */
	GPUOP(925000, 87500, 97500, POSDIV_POWER_2, 1875, 0), /*  3 */
	GPUOP(900000, 85000, 95000, POSDIV_POWER_2, 1875, 0), /*  4 */
	GPUOP(875000, 82500, 92500, POSDIV_POWER_2, 1875, 0), /*  5 */
	GPUOP(850000, 80000, 90000, POSDIV_POWER_2, 1875, 0), /*  6 */
	GPUOP(823000, 79375, 89375, POSDIV_POWER_2, 1875, 0), /*  7 */
	GPUOP(796000, 78125, 88125, POSDIV_POWER_2, 1875, 0), /*  8 sign off */
	GPUOP(769000,  76875, 86875, POSDIV_POWER_2, 1875, 0), /*  9 */
	GPUOP(743000,  75625, 85625, POSDIV_POWER_2, 1250, 0), /* 10 */
	GPUOP(716000,  75000, 85000, POSDIV_POWER_2, 1250, 0), /* 11 */
	GPUOP(690000,  73750, 85000, POSDIV_POWER_4, 1250, 0), /* 12 */
	GPUOP(663000,  72500, 85000, POSDIV_POWER_4, 1250, 0), /* 13 */
	GPUOP(637000,  71250, 85000, POSDIV_POWER_4, 1250, 0), /* 14 */
	GPUOP(611000,  70625, 85000, POSDIV_POWER_4, 1250, 0), /* 15 */
	GPUOP(586000,  70000, 85000, POSDIV_POWER_4, 1250, 0), /* 16 sign off*/
	GPUOP(560000,  69375, 85000, POSDIV_POWER_4, 625, 0), /* 17 */
	GPUOP(535000,  68750, 85000, POSDIV_POWER_4, 625, 0), /* 18 */
	GPUOP(509000,  68125, 85000, POSDIV_POWER_4, 625, 0), /* 19 */
	GPUOP(484000,  66875, 85000, POSDIV_POWER_4, 625, 0), /* 20 */
	GPUOP(467000,  66875, 85000, POSDIV_POWER_4, 625, 0), /* 21 */
	GPUOP(450000,  66250, 85000, POSDIV_POWER_4, 625, 0), /* 22 */
	GPUOP(434000,  65625, 85000, POSDIV_POWER_4, 625, 0), /* 23 */
	GPUOP(417000,  65000, 85000, POSDIV_POWER_4, 625, 0), /* 24 */
	GPUOP(400000,  64375, 85000, POSDIV_POWER_4, 625, 0), /* 25 */
	GPUOP(383000,  64375, 85000, POSDIV_POWER_4, 625, 0), /* 26 */
	GPUOP(366000,  63750, 85000, POSDIV_POWER_4, 625, 0), /* 27 */
	GPUOP(349000,  63125, 85000, POSDIV_POWER_4, 625, 0), /* 28 */
	GPUOP(332000,  62500, 85000, POSDIV_POWER_4, 625, 0), /* 29 */
	GPUOP(315000,  61875, 85000, POSDIV_POWER_4, 625, 0), /* 30 */
	GPUOP(299000,  61250, 85000, POSDIV_POWER_4, 625, 0), /* 31 */
};


/**************************************************
 * Segment Adjustment
 **************************************************/
#define ADJOP(_oppidx, _freq, _volt, _vsram, _vaging) \
	{                                  \
		.oppidx = _oppidx,             \
		.freq = _freq,                 \
		.volt = _volt,                 \
		.vsram = _vsram,               \
		.vaging = _vaging,             \
	}

/**************************************************
 * Aging Adjustment
 **************************************************/
unsigned int g_aging_table[][SIGNED_OPP_GPU_NUM] = {
	{ /* aging table 0 */
		1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, /* OPP 0~9   */
		1250, 1250, 1250, 1250, 1250, 1250, 1250, 625, 625, 625,    /* OPP 10~19 */
		 625,  625,  625,  625,  625,  625,  625, 625, 625, 625,    /* OPP 20~29 */
		 625,  625,                                                 /* OPP 30~31 */
	},
};

/**************************************************
 * PTPOD Adjustment
 **************************************************/
#define PTPOD_OPP_GPU_NUM              ARRAY_SIZE(g_ptpod_opp_idx_table)
unsigned int g_ptpod_opp_idx_table[] = {
	0, 2, 4, 6,
	8, 10, 12, 14,
	16, 18, 20, 23,
	25, 27, 29, 31
};

#endif /* __GPUFREQ_MT6879_H__ */
