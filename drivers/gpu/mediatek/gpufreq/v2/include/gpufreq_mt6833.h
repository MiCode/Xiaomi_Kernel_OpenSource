/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6833_H__
#define __GPUFREQ_MT6833_H__

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
/* misc setting control */
#define GPUFREQ_MFG1_CONTROL_ENABLE     (1)

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
#define MFGPLL_CON0                     (g_apmixed_base + 0x268)
#define MFGPLL_CON1                     (g_apmixed_base + 0x26C)
#define MFGPLL_CON2                     (g_apmixed_base + 0x270)
#define MFGPLL_CON3                     (g_apmixed_base + 0x274)
#define CLK_MUX_OFS                     (0x1034)
#define CKMUX_SEL_REF_CORE              (4)
#define CKMUX_SEL_REF_PARK              (5)
#define CKMUX_SEL_REF_STACK             (7)
#define CKMUX_SEL_REF_STACK_PARK        (8)
#define GPUPLL_ID                       (0x0)
#define STACKPLL_ID                     (0x3)

/**************************************************
 * Frequency Hopping Setting
 **************************************************/
#define GPUFREQ_FHCTL_ENABLE            (1)
#define MFG_PLL_NAME                    "mfgpll"

/**************************************************
 * Power Domain Setting
 **************************************************/
#define GPUFREQ_PDCv2_ENABLE            (0)
#define GPUFREQ_CHECK_MTCMOS_PWR_STATUS (0)
#define PWR_STATUS_OFS                  (0xF3C)
#define PWR_STATUS_2ND_OFS              (0xF40)
#define MFG_0_1_PWR_MASK                (0x6)           /* 0000 0110 */
#define MFG_0_3_PWR_MASK                (0x1E)          /* 0001 1110 */
#define MFG_0_4_PWR_MASK                (0x3E)          /* 0011 1110 */
#define MFG_0_5_PWR_MASK                (0x7E)          /* 0111 1110 */
#define MFG_1_5_PWR_MASK                (0x7C)          /* 0111 1100 */
#define MFG_2_5_PWR_MASK                (0x78)          /* 0111 1000 */

/**************************************************
 * Shader Core Setting
 **************************************************/
#define MFG2_SHADER_STACK0              (T0C0)          /* MFG2 */
#define MFG3_SHADER_STACK2              (T2C0)          /* MFG3 */

#define GPU_SHADER_PRESENT_1 \
	(MFG2_SHADER_STACK0)
#define GPU_SHADER_PRESENT_2 \
	(MFG2_SHADER_STACK0 | MFG3_SHADER_STACK2)

#define SHADER_CORE_NUM                 (2)
struct gpufreq_core_mask_info g_core_mask_table[] = {
	{2, GPU_SHADER_PRESENT_2},
	{1, GPU_SHADER_PRESENT_1},
};

/**************************************************
 * Reference Power Setting MT6833 TBD
 **************************************************/
#define GPU_ACT_REF_POWER               (1285)          /* mW  */
#define GPU_ACT_REF_FREQ                (900000)       /* KHz */
#define GPU_ACT_REF_VOLT                (90000)         /* mV x 100 */
#define GPU_LEAKAGE_POWER               (71)

/**************************************************
 * PMIC Setting MT6833
 **************************************************/
/*
 * PMIC hardware range:
 * vgpu      0.4 ~ 1.19300 V (MT6368)
 * vsram     0.5 ~ 1.29300 V (MT6363)
 */
#define VGPU_MAX_VOLT                   (119300)        /* mV x 100 */
#define VGPU_MIN_VOLT                   (40000)         /* mV x 100 */
#define VSRAM_MAX_VOLT                  (129300)        /* mV x 100 */
#define VSRAM_MIN_VOLT                  (50000)         /* mV x 100 */
#define PMIC_STEP                       (625)           /* mV x 100 */
/*
 * (0)mv <= (VSRAM - VGPU) <= (200)mV
 */
#define MAX_BUCK_DIFF                   (20000)         /* mV x 100 */
#define MIN_BUCK_DIFF                   (0)        /* mV x 100 */
/*
 * (Vgpu > THRESH): Vsram = Vgpu + DIFF
 * (Vgpu <= THRESH): Vsram = FIXED_VOLT
 */
#define VSRAM_FIXED_THRESHOLD           (75000)
#define VSRAM_FIXED_VOLT                (75000)
#define VSRAM_FIXED_DIFF                (0)
#define VOLT_NORMALIZATION(volt) \
	((volt % 625) ? (volt - (volt % 625) + 625) : volt)

/**************************************************
 * SRAMRC Setting
 **************************************************/
#define GPUFREQ_SAFE_VLOGIC             (60000)
#define VSRAM_LEVEL_0                   (75000)
#define VSRAM_LEVEL_1                   (80000)
#define SRAM_PARK_VOLT                  (75000)

/**************************************************
 * Power Throttling Setting
 **************************************************/
#define GPUFREQ_BATT_OC_ENABLE          (0)
#define GPUFREQ_BATT_PERCENT_ENABLE     (0)
#define GPUFREQ_LOW_BATT_ENABLE         (0)
#define GPUFREQ_BATT_OC_FREQ            (474000)
#define GPUFREQ_BATT_PERCENT_IDX        (0)
#define GPUFREQ_LOW_BATT_FREQ           (474000)

/**************************************************
 * Adaptive Volt Scaling (AVS) Setting
 **************************************************/
#define GPUFREQ_AVS_ENABLE              (0)

/**************************************************
 * Aging Sensor Setting
 **************************************************/
#define GPUFREQ_ASENSOR_ENABLE          (0)
#define GPUFREQ_AGING_KEEP_FGPU         (670000)
#define GPUFREQ_AGING_KEEP_VGPU         (65000)
#define GPUFREQ_AGING_GAP_MIN           (-3)
#define GPUFREQ_AGING_GAP_1             (2)
#define GPUFREQ_AGING_GAP_2             (4)
#define GPUFREQ_AGING_GAP_3             (6)
#define GPUFREQ_AGING_MOST_AGRRESIVE    (0)

/**************************************************
 * Enumeration MT6833
 **************************************************/
enum gpufreq_segment {
	MT6833_SEGMENT = 1,
};

enum gpufreq_clk_src {
	CLOCK_MAIN = 0,
	CLOCK_SUB,
};

/**************************************************
 * Structure MT6833
 **************************************************/
struct gpufreq_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram_gpu;
};

struct gpufreq_clk_info {
	struct clk *clk_mux;
	struct clk *clk_main_parent;
	struct clk *clk_sub_parent;
	struct clk *subsys_bg3d;
};

struct gpufreq_mtcmos_info {
	struct device *mfg0_dev;
	struct device *mfg1_dev;
	struct device *mfg2_dev;
	struct device *mfg3_dev;
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
	GPUOP(1068000, 85000, 85000, POSDIV_POWER_2, 1875, 0), /*  0 sign off */
	GPUOP(1059000, 84375, 84375, POSDIV_POWER_2, 1875, 0), /*  1 */
	GPUOP(1051000, 83750, 83750, POSDIV_POWER_2, 1875, 0), /*  2 */
	GPUOP(1042000, 83125, 83125, POSDIV_POWER_2, 1875, 0), /*  3 */
	GPUOP(1034000, 82500, 82500, POSDIV_POWER_2, 1875, 0), /*  4 */
	GPUOP(1025000, 81875, 81875, POSDIV_POWER_2, 1875, 0), /*  5 */
	GPUOP(1017000, 81250, 81250, POSDIV_POWER_2, 1875, 0), /*  6 */
	GPUOP(1008000, 80625, 80625, POSDIV_POWER_2, 1875, 0), /*  7 */
	GPUOP(1000000, 80000, 80000, POSDIV_POWER_2, 1875, 0), /*  8 sign off */
	GPUOP(985000,  79375, 79375, POSDIV_POWER_2, 1875, 0), /*  9 */
	GPUOP(970000,  78750, 78750, POSDIV_POWER_2, 1875, 0), /* 10 */
	GPUOP(955000,  78125, 78125, POSDIV_POWER_2, 1875, 0), /* 11 */
	GPUOP(940000,  77500, 77500, POSDIV_POWER_4, 1875, 0), /* 12 */
	GPUOP(925000,  76875, 76875, POSDIV_POWER_4, 1875, 0), /* 13 */
	GPUOP(910000,  76250, 76250, POSDIV_POWER_4, 1875, 0), /* 14 */
	GPUOP(895000,  75625, 75625, POSDIV_POWER_4, 1875, 0), /* 15 */
	GPUOP(880000,  75000, 75000, POSDIV_POWER_4, 1875, 0), /* 16 sign off*/
	GPUOP(868000,  74375, 75000, POSDIV_POWER_4, 1875, 0), /* 17 */
	GPUOP(857000,  73750, 75000, POSDIV_POWER_4, 1875, 0), /* 18 */
	GPUOP(846000,  73125, 75000, POSDIV_POWER_4, 1875, 0), /* 19 */
	GPUOP(835000,  72500, 75000, POSDIV_POWER_4, 1250, 0), /* 20 */
	GPUOP(823000,  71875, 75000, POSDIV_POWER_4, 1250, 0), /* 21 */
	GPUOP(812000,  71250, 75000, POSDIV_POWER_4, 1250, 0), /* 22 */
	GPUOP(801000,  70625, 75000, POSDIV_POWER_4, 1250, 0), /* 23 */
	GPUOP(790000,  70000, 75000, POSDIV_POWER_4, 1250, 0), /* 24 */
	GPUOP(778000,  69375, 75000, POSDIV_POWER_4, 1250, 0), /* 25 */
	GPUOP(767000,  68750, 75000, POSDIV_POWER_4, 1250, 0), /* 26 */
	GPUOP(756000,  68125, 75000, POSDIV_POWER_4, 1250, 0), /* 27 */
	GPUOP(745000,  67500, 75000, POSDIV_POWER_4, 1250, 0), /* 28 */
	GPUOP(733000,  66875, 75000, POSDIV_POWER_4, 1250, 0), /* 29 */
	GPUOP(722000,  66250, 75000, POSDIV_POWER_4, 1250, 0), /* 30 */
	GPUOP(711000,  65625, 75000, POSDIV_POWER_4, 1250, 0), /* 31 */
	GPUOP(700000,  65000, 75000, POSDIV_POWER_4, 1250, 0), /* 32 sign off*/
	GPUOP(674000,  64375, 75000, POSDIV_POWER_4, 1250, 0), /* 33 */
	GPUOP(648000,  63750, 75000, POSDIV_POWER_4, 1250, 0), /* 34 */
	GPUOP(622000,  63125, 75000, POSDIV_POWER_4, 1250, 0), /* 35 */
	GPUOP(596000,  62500, 75000, POSDIV_POWER_4,  625, 0), /* 36 */
	GPUOP(570000,  61875, 75000, POSDIV_POWER_4,  625, 0), /* 37 */
	GPUOP(545000,  61250, 75000, POSDIV_POWER_4,  625, 0), /* 38 */
	GPUOP(519000,  60625, 75000, POSDIV_POWER_4,  625, 0), /* 39 */
	GPUOP(493000,  60000, 75000, POSDIV_POWER_4,  625, 0), /* 40 */
	GPUOP(467000,  59375, 75000, POSDIV_POWER_4,  625, 0), /* 41 */
	GPUOP(441000,  58750, 75000, POSDIV_POWER_4,  625, 0), /* 42 */
	GPUOP(415000,  58125, 75000, POSDIV_POWER_4,  625, 0), /* 43 */
	GPUOP(390000,  57500, 75000, POSDIV_POWER_4,  625, 0), /* 44 sign off*/
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

#define SEGMENT_ADJ_NUM                 ARRAY_SIZE(g_segment_adj)
struct gpufreq_adj_info g_segment_adj[] = {
	ADJOP(25, 0, 65000, 0, 0), /* sign off */
	ADJOP(26, 0, 64375, 0, 0),
	ADJOP(27, 0, 64375, 0, 0),
	ADJOP(28, 0, 63750, 0, 0),
	ADJOP(29, 0, 63750, 0, 0),
	ADJOP(30, 0, 63125, 0, 0),
	ADJOP(31, 0, 63125, 0, 0),
	ADJOP(32, 0, 62500, 0, 0),
	ADJOP(33, 0, 62500, 0, 0),
	ADJOP(34, 0, 61875, 0, 0),
	ADJOP(35, 0, 61875, 0, 0),
	ADJOP(36, 0, 61250, 0, 0),
	ADJOP(37, 0, 61250, 0, 0),
	ADJOP(38, 0, 60625, 0, 0),
	ADJOP(39, 0, 60625, 0, 0),
	ADJOP(40, 0, 60000, 0, 0), /* sign off */
};

/* MCL50 flavor load */
#define MCL50_ADJ_NUM                   ARRAY_SIZE(g_mcl50_adj)
struct gpufreq_adj_info g_mcl50_adj[] = {
	ADJOP(0,  0, 68750, VSRAM_LEVEL_0, 0), /*  0 sign off */
	ADJOP(1,  0, 68750, VSRAM_LEVEL_0, 0),
	ADJOP(2,  0, 68125, VSRAM_LEVEL_0, 0),
	ADJOP(3,  0, 67500, VSRAM_LEVEL_0, 0),
	ADJOP(4,  0, 66875, VSRAM_LEVEL_0, 0),
	ADJOP(5,  0, 66250, VSRAM_LEVEL_0, 0),
	ADJOP(6,  0, 65625, VSRAM_LEVEL_0, 0),
	ADJOP(7,  0, 65000, VSRAM_LEVEL_0, 0),
	ADJOP(8,  0, 64375, 0,             0), /*  8 sign off */
	ADJOP(9,  0, 64375, 0,             0),
	ADJOP(10, 0, 63750, 0,             0),
	ADJOP(11, 0, 63125, 0,             0),
	ADJOP(12, 0, 62500, 0,             0),
	ADJOP(13, 0, 62500, 0,             0),
	ADJOP(14, 0, 61875, 0,             0),
	ADJOP(15, 0, 61250, 0,             0),
	ADJOP(16, 0, 60625, 0,             0), /* 16 sign off */
	ADJOP(17, 0, 60625, 0,             0),
	ADJOP(18, 0, 60000, 0,             0),
	ADJOP(19, 0, 59375, 0,             0),
	ADJOP(20, 0, 58750, 0,             0),
	ADJOP(21, 0, 58750, 0,             0),
	ADJOP(22, 0, 58125, 0,             0),
	ADJOP(23, 0, 57500, 0,             0),
	ADJOP(24, 0, 56875, 0,             0),
	ADJOP(25, 0, 56875, 0,             0),
	ADJOP(26, 0, 56875, 0,             0),
	ADJOP(27, 0, 56875, 0,             0),
	ADJOP(28, 0, 56250, 0,             0),
	ADJOP(29, 0, 56250, 0,             0),
	ADJOP(30, 0, 56250, 0,             0),
	ADJOP(31, 0, 56250, 0,             0),
	ADJOP(32, 0, 55625, 0,             0), /* 32 sign off */
	ADJOP(33, 0, 55625, 0,             0),
	ADJOP(34, 0, 55625, 0,             0),
	ADJOP(35, 0, 55625, 0,             0),
	ADJOP(36, 0, 55000, 0,             0),
	ADJOP(37, 0, 55000, 0,             0),
	ADJOP(38, 0, 55000, 0,             0),
	ADJOP(39, 0, 55000, 0,             0),
	ADJOP(40, 0, 54375, 0,             0), /* 40 sign off */
};

/**************************************************
 * AVS Adjustment
 **************************************************/
#define AVS_ADJ_NUM                   ARRAY_SIZE(g_avs_adj)
struct gpufreq_adj_info g_avs_adj[] = {
	ADJOP(0,  0, 0, 0, 0),
	ADJOP(8,  0, 0, 0, 0),
	ADJOP(24, 0, 0, 0, 0),
	ADJOP(40, 0, 0, 0, 0),
};

/**************************************************
 * Aging Adjustment
 **************************************************/
/*
 * todo: Need to check correct table for applying aging
 */
unsigned int g_aging_table[][SIGNED_OPP_GPU_NUM] = {
	{ /* aging table 0 */
		1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, /* OPP 0~9   */
		1875, 1875, 1875, 1875, 1875, 1250, 1250, 1250, 1250, 1250, /* OPP 10~19 */
		1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 625,  /* OPP 20~29 */
		625,  625,  625,  625,  625,  625,  625,  625,  625,  625,  /* OPP 30~39 */
		625,  625,  625,  625,  625,                                /* OPP 40~44 */
	},
	{ /* aging table 1 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 0~9   */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 10~19 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 20~29 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 30~39 */
		0, 0, 0, 0, 0,                /* OPP 40~44 */
	},
	{ /* aging table 2 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 0~9   */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 10~19 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 20~29 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 30~39 */
		0, 0, 0, 0, 0,                /* OPP 40~44 */
	},
	{ /* aging table 3 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 0~9   */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 10~19 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 20~29 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 30~39 */
		0, 0, 0, 0, 0,                /* OPP 40~44 */
	},
};

#endif /* __GPUFREQ_MT6879_H__ */
