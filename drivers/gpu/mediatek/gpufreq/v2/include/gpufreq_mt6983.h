/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6983_H__
#define __GPUFREQ_MT6983_H__

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
#define GPUFREQ_VCORE_DVFS_ENABLE       (1)
#define GPUFREQ_CG_CONTROL_ENABLE       (0)

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
#define MFG_PLL_CON1                    (g_mfg_pll_base + 0x00C)
#define MFGSC_PLL_CON1                  (g_mfgsc_pll_base + 0x00C)
#define MFGPLL_FQMTR_CON0               (g_mfg_pll_base + 0x040)
#define MFGPLL_FQMTR_CON1               (g_mfg_pll_base + 0x044)
#define MFGSCPLL_FQMTR_CON0             (g_mfgsc_pll_base + 0x040)
#define MFGSCPLL_FQMTR_CON1             (g_mfgsc_pll_base + 0x044)
#define MFG_SEL_0_MASK                  (0x10000)       /* [16] */
#define MFG_SEL_1_MASK                  (0x20000)       /* [17] */
#define MFG_REF_SEL_MASK                (0x3000000)     /* [25:24] */
#define MFGSC_REF_SEL_MASK              (0x3)           /* [1:0] */

/**************************************************
 * Frequency Hopping Setting
 **************************************************/
#define GPUFREQ_FHCTL_ENABLE            (1)
#define MFG_PLL_NAME                    "mfgpll"
#define MFGSC_PLL_NAME                  "mfgscpll"

/**************************************************
 * Power Domain Setting
 **************************************************/
#define GPUFREQ_PDCv2_ENABLE            (1)
#define GPUFREQ_CHECK_MTCMOS_PWR_STATUS (0)
#define PWR_STATUS_OFS                  (0xF3C)
#define PWR_STATUS_2ND_OFS              (0xF40)
#define MFG_0_1_PWR_MASK                (0x6)           /* 0000 0000 0000 0000 0110 */
#define MFG_0_18_PWR_MASK               (0xFFFFE)       /* 1111 1111 1111 1111 1110 */
#define MFG_1_18_PWR_MASK               (0xFFFFC)       /* 1111 1111 1111 1111 1100 */

/**************************************************
 * Shader Core Setting
 **************************************************/
#define MFG3_SHADER_STACK0              (T0C0 | T0C1)   /* MFG9, MFG12 */
#define MFG4_SHADER_STACK1              (T1C0)          /* MFG10 */
#define MFG5_SHADER_STACK2              (T2C0)          /* MFG11 */
#define MFG6_SHADER_STACK4              (T4C0 | T4C1)   /* MFG13, MFG16 */
#define MFG7_SHADER_STACK5              (T5C0 | T5C1)   /* MFG14, MFG17 */
#define MFG8_SHADER_STACK6              (T6C0 | T6C1)   /* MFG15, MFG18 */

#define GPU_SHADER_PRESENT_1 \
	(T0C0)
#define GPU_SHADER_PRESENT_2 \
	(MFG3_SHADER_STACK0)
#define GPU_SHADER_PRESENT_3 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1)
#define GPU_SHADER_PRESENT_4 \
	(MFG3_SHADER_STACK0 | MFG6_SHADER_STACK4)
#define GPU_SHADER_PRESENT_5 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG6_SHADER_STACK4)
#define GPU_SHADER_PRESENT_6 \
	(MFG3_SHADER_STACK0 | MFG6_SHADER_STACK4 | MFG7_SHADER_STACK5)
#define GPU_SHADER_PRESENT_7 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG6_SHADER_STACK4 | \
	MFG7_SHADER_STACK5)
#define GPU_SHADER_PRESENT_8 \
	(MFG3_SHADER_STACK0 | MFG6_SHADER_STACK4 | MFG7_SHADER_STACK5 | \
	 MFG8_SHADER_STACK6)
#define GPU_SHADER_PRESENT_9 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG6_SHADER_STACK4 | \
	 MFG7_SHADER_STACK5 | MFG8_SHADER_STACK6)
#define GPU_SHADER_PRESENT_10 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG6_SHADER_STACK4 | MFG7_SHADER_STACK5 | MFG8_SHADER_STACK6)

#define SHADER_CORE_NUM                 (10)
struct gpufreq_core_mask_info g_core_mask_table[] = {
	{10, GPU_SHADER_PRESENT_10},
	{9, GPU_SHADER_PRESENT_9},
	{8, GPU_SHADER_PRESENT_8},
	{7, GPU_SHADER_PRESENT_7},
	{6, GPU_SHADER_PRESENT_6},
	{5, GPU_SHADER_PRESENT_5},
	{4, GPU_SHADER_PRESENT_4},
	{3, GPU_SHADER_PRESENT_3},
	{2, GPU_SHADER_PRESENT_2},
	{1, GPU_SHADER_PRESENT_1},
};

/**************************************************
 * Reference Power Setting
 **************************************************/
#define STACK_ACT_REF_POWER             (2671)          /* mW  */
#define STACK_ACT_REF_FREQ              (848000)        /* KHz */
#define STACK_ACT_REF_VOLT              (75000)         /* mV x 100 */
#define STACK_LEAKAGE_POWER             (130)

/**************************************************
 * PMIC Setting
 **************************************************/
/*
 * PMIC hardware range:
 * vgpu      0.3 ~ 1.19375 V (MT6363)
 * vstack    0.3 ~ 1.19375 V (MT6373)
 * vsram     0.4 ~ 1.19375 V (MT6363)
 */
#define VGPU_MAX_VOLT                   (119375)        /* mV x 100 */
#define VGPU_MIN_VOLT                   (30000)         /* mV x 100 */
#define VSTACK_MAX_VOLT                 (119375)        /* mV x 100 */
#define VSTACK_MIN_VOLT                 (30000)         /* mV x 100 */
#define VSRAM_MAX_VOLT                  (119375)        /* mV x 100 */
#define VSRAM_MIN_VOLT                  (40000)         /* mV x 100 */
#define PMIC_STEP                       (625)           /* mV x 100 */
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
#define GPUFREQ_SAFE_VLOGIC             (55000)
#define VSRAM_LEVEL_0                   (75000)
#define VSRAM_LEVEL_1                   (80000)

/**************************************************
 * DVFSRC Setting
 **************************************************/
#define MAX_VCORE_LEVEL                 (4)
#define VCORE_LEVEL_0                   (57500)
#define VCORE_LEVEL_1                   (60000)
#define VCORE_LEVEL_2                   (65000)
#define VCORE_LEVEL_3                   (72500)
#define VCORE_LEVEL_4                   (75000)

/**************************************************
 * Power Throttling Setting
 **************************************************/
#define GPUFREQ_BATT_OC_ENABLE          (1)
#define GPUFREQ_BATT_PERCENT_ENABLE     (0)
#define GPUFREQ_LOW_BATT_ENABLE         (1)
#define GPUFREQ_BATT_OC_FREQ            (484000)
#define GPUFREQ_BATT_PERCENT_IDX        (0)
#define GPUFREQ_LOW_BATT_FREQ           (484000)

/**************************************************
 * Adaptive Volt Scaling (AVS) Setting
 **************************************************/
#define GPUFREQ_AVS_ENABLE              (0)

/**************************************************
 * Aging Sensor Setting
 **************************************************/
#define GPUFREQ_ASENSOR_ENABLE          (0)
#define GPUFREQ_AGING_KEEP_FGPU         (800000)
#define GPUFREQ_AGING_KEEP_VGPU         (VCORE_LEVEL_3)
#define GPUFREQ_AGING_KEEP_FSTACK       (586000)
#define GPUFREQ_AGING_KEEP_VSTACK       (65000)
#define GPUFREQ_AGING_GAP_MIN           (-3)
#define GPUFREQ_AGING_GAP_1             (2)
#define GPUFREQ_AGING_GAP_2             (4)
#define GPUFREQ_AGING_GAP_3             (6)
#define GPUFREQ_AGING_MOST_AGRRESIVE    (0)

/**************************************************
 * GPU DVFS HW Constraint Setting
 **************************************************/
/*
 * Constraint Coefficient = CONSTRAINT_COEF/BASE_COEF
 * Fgpu = Fstack * 1.1
 */
#define GPUFREQ_CONSTRAINT_COEF         (11)
#define GPUFREQ_BASE_COEF               (10)
/* DVFS Timing issue */
#define DVFS_TIMING_PARK_FREQ           (586000)
/* DELSEL ULV SRAM access */
#define DELSEL_ULV_PARK_FREQ            (316000)

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_segment {
	MT6983_SEGMENT = 1,
	MT6981_SEGMENT,
};

enum gpufreq_clk_src {
	CLOCK_SUB = 0,
	CLOCK_MAIN,
};

/**************************************************
 * Structure
 **************************************************/
struct gpufreq_pmic_info {
	struct regulator *reg_vcore;
	struct regulator *reg_dvfsrc;
	struct regulator *reg_vstack;
	struct regulator *reg_vsram;
};

struct gpufreq_clk_info {
	struct clk *clk_mux;
	struct clk *clk_main_parent;
	struct clk *clk_sub_parent;
	struct clk *clk_sc_mux;
	struct clk *clk_sc_main_parent;
	struct clk *clk_sc_sub_parent;
	struct clk *subsys_mfg_cg;
};

struct gpufreq_mtcmos_info {
	struct device *mfg1_dev;  /* MPU, CM7 */
#if !GPUFREQ_PDCv2_ENABLE
	struct device *mfg2_dev;  /* L2 Cache */
	struct device *mfg3_dev;  /* ST0      */
	struct device *mfg4_dev;  /* ST1      */
	struct device *mfg5_dev;  /* ST2      */
	struct device *mfg6_dev;  /* ST4      */
	struct device *mfg7_dev;  /* ST5      */
	struct device *mfg8_dev;  /* ST6      */
	struct device *mfg9_dev;  /* ST0T0    */
	struct device *mfg10_dev; /* ST1T0    */
	struct device *mfg11_dev; /* ST2T0    */
	struct device *mfg12_dev; /* ST0T1    */
	struct device *mfg13_dev; /* ST4T0    */
	struct device *mfg14_dev; /* ST5T0    */
	struct device *mfg15_dev; /* ST6T0    */
	struct device *mfg16_dev; /* ST4T1    */
	struct device *mfg17_dev; /* ST5T1    */
	struct device *mfg18_dev; /* ST6T1    */
#endif /* GPUFREQ_PDCv2_ENABLE */
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
	GPUOP(880000, VCORE_LEVEL_4, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 0 */
	GPUOP(800000, VCORE_LEVEL_3, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 1 */
	GPUOP(610000, VCORE_LEVEL_2, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 2 */
	GPUOP(430000, VCORE_LEVEL_1, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 3 */
	GPUOP(350000, VCORE_LEVEL_0, VSRAM_LEVEL_0, POSDIV_POWER_8, 0, 0), /* 4 */
};

#define SIGNED_OPP_STACK_NUM            ARRAY_SIZE(g_default_stack)
struct gpufreq_opp_info g_default_stack[] = {
	GPUOP(848000, 75000, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /*  0 sign off */
	GPUOP(841000, 74375, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /*  1 */
	GPUOP(835000, 73750, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /*  2 */
	GPUOP(828000, 73125, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /*  3 */
	GPUOP(822000, 72500, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /*  4 */
	GPUOP(816000, 71875, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /*  5 */
	GPUOP(809000, 71250, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /*  6 */
	GPUOP(803000, 70625, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /*  7 */
	GPUOP(797000, 70000, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /*  8 sign off */
	GPUOP(770000, 69375, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /*  9 */
	GPUOP(744000, 68750, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 10 */
	GPUOP(717000, 68125, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 11 */
	GPUOP(691000, 67500, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 12 */
	GPUOP(665000, 66875, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 13 */
	GPUOP(638000, 66250, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 14 */
	GPUOP(612000, 65625, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 15 */
	GPUOP(586000, 65000, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 16 sign off */
	GPUOP(569000, 64375, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 17 */
	GPUOP(552000, 63750, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 18 */
	GPUOP(535000, 63125, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 19 */
	GPUOP(518000, 62500, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 20 */
	GPUOP(501000, 61875, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 21 */
	GPUOP(484000, 61250, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 22 */
	GPUOP(467000, 60625, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 23 */
	GPUOP(451000, 60000, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 24 */
	GPUOP(434000, 59375, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 25 */
	GPUOP(417000, 58750, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 26 */
	GPUOP(400000, 58125, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 27 */
	GPUOP(383000, 57500, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 28 */
	GPUOP(366000, 56875, VSRAM_LEVEL_0, POSDIV_POWER_8, 0, 0), /* 29 */
	GPUOP(349000, 56250, VSRAM_LEVEL_0, POSDIV_POWER_8, 0, 0), /* 30 */
	GPUOP(332000, 55625, VSRAM_LEVEL_0, POSDIV_POWER_8, 0, 0), /* 31 */
	GPUOP(316000, 55000, VSRAM_LEVEL_0, POSDIV_POWER_8, 0, 0), /* 32 sign off */
	GPUOP(303000, 54375, VSRAM_LEVEL_0, POSDIV_POWER_8, 0, 0), /* 33 */
	GPUOP(290000, 53750, VSRAM_LEVEL_0, POSDIV_POWER_8, 0, 0), /* 34 */
	GPUOP(277000, 53125, VSRAM_LEVEL_0, POSDIV_POWER_8, 0, 0), /* 35 */
	GPUOP(265000, 52500, VSRAM_LEVEL_0, POSDIV_POWER_8, 0, 0), /* 36 */
	GPUOP(252000, 51875, VSRAM_LEVEL_0, POSDIV_POWER_8, 0, 0), /* 37 */
	GPUOP(239000, 51250, VSRAM_LEVEL_0, POSDIV_POWER_8, 0, 0), /* 38 */
	GPUOP(226000, 50625, VSRAM_LEVEL_0, POSDIV_POWER_8, 0, 0), /* 39 */
	GPUOP(214000, 50000, VSRAM_LEVEL_0, POSDIV_POWER_8, 0, 0), /* 40 sign off */
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
	ADJOP(0,  0, 66875, 0, 0), /*  0 sign off */
	ADJOP(1,  0, 66875, 0, 0),
	ADJOP(2,  0, 66250, 0, 0),
	ADJOP(3,  0, 66250, 0, 0),
	ADJOP(4,  0, 65625, 0, 0),
	ADJOP(5,  0, 65625, 0, 0),
	ADJOP(6,  0, 65000, 0, 0),
	ADJOP(7,  0, 65000, 0, 0),
	ADJOP(8,  0, 64375, 0, 0), /*  8 sign off */
	ADJOP(9,  0, 64375, 0, 0),
	ADJOP(10, 0, 63750, 0, 0),
	ADJOP(11, 0, 63125, 0, 0),
	ADJOP(12, 0, 62500, 0, 0),
	ADJOP(13, 0, 61875, 0, 0),
	ADJOP(14, 0, 61250, 0, 0),
	ADJOP(15, 0, 60625, 0, 0),
	ADJOP(16, 0, 60000, 0, 0), /* 16 sign off */
	ADJOP(17, 0, 60000, 0, 0),
	ADJOP(18, 0, 59375, 0, 0),
	ADJOP(19, 0, 58750, 0, 0),
	ADJOP(20, 0, 58125, 0, 0),
	ADJOP(21, 0, 57500, 0, 0),
	ADJOP(22, 0, 56875, 0, 0),
	ADJOP(23, 0, 56250, 0, 0),
	ADJOP(24, 0, 55625, 0, 0),
	ADJOP(25, 0, 55000, 0, 0),
	ADJOP(26, 0, 54375, 0, 0),
	ADJOP(27, 0, 53750, 0, 0),
	ADJOP(28, 0, 53125, 0, 0),
	ADJOP(29, 0, 52500, 0, 0),
	ADJOP(30, 0, 51875, 0, 0),
	ADJOP(31, 0, 51250, 0, 0),
	ADJOP(32, 0, 50625, 0, 0), /* 32 sign off */
	ADJOP(33, 0, 50625, 0, 0),
	ADJOP(34, 0, 50000, 0, 0),
	ADJOP(35, 0, 50000, 0, 0),
	ADJOP(36, 0, 49375, 0, 0),
	ADJOP(37, 0, 49375, 0, 0),
	ADJOP(38, 0, 48750, 0, 0),
	ADJOP(39, 0, 48750, 0, 0),
	ADJOP(40, 0, 48125, 0, 0), /* 40 sign off */
};

/**************************************************
 * AVS Adjustment
 **************************************************/
#define AVS_ADJ_NUM                     ARRAY_SIZE(g_avs_adj)
struct gpufreq_adj_info g_avs_adj[] = {
	ADJOP(0,  0, 0, 0, 0),
	ADJOP(8,  0, 0, 0, 0),
	ADJOP(16, 0, 0, 0, 0),
	ADJOP(32, 0, 0, 0, 0),
	ADJOP(40, 0, 0, 0, 0),
};

/**************************************************
 * Aging Adjustment
 **************************************************/
unsigned int g_aging_table[][SIGNED_OPP_STACK_NUM] = {
	{ /* aging table 0 */
		625, 625, 625, 625, 625, 625, 625, 625, 625, 625,  /* OPP 0~9   */
		625, 625, 625, 625, 625, 625, 625, 625, 625, 625,  /* OPP 10~19 */
		625, 625, 625, 625, 625, 625, 625, 625, 625, 625,  /* OPP 20~29 */
		625, 625, 625, 625, 625, 625, 625, 625, 625, 625,  /* OPP 30~39 */
		625,                                               /* OPP 40    */
	},
	{ /* aging table 1 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 0~9   */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 10~19 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 20~29 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 30~39 */
		0,                            /* OPP 40    */
	},
	{ /* aging table 2 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 0~9   */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 10~19 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 20~29 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 30~39 */
		0,                            /* OPP 40    */
	},
	{ /* aging table 3 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 0~9   */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 10~19 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 20~29 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 30~39 */
		0,                            /* OPP 40    */
	},
};

#endif /* __GPUFREQ_MT6983_H__ */
