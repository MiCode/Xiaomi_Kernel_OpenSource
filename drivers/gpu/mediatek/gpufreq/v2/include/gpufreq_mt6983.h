/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6983_H__
#define __GPUFREQ_MT6983_H__

/**************************************************
 * GPUFREQ Local Config
 **************************************************/
#define GPUFREQ_BRINGUP                 (1)
#define GPUFREQ_BUCK_ALWAYS_ON          (0)
#define GPUFREQ_MTCMOS_ALWAYS_ON        (0)
#define GPUFREQ_CG_ALWAYS_ON            (0)
/*
 *  0 -> all on when mtk probe init (Freq/Vgpu/Vsram_gpu)
 *       disable DDK power on/off callback
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
/* feature control */
#define GPUFREQ_PDCv2_ENABLE            (0)
#define GPUFREQ_VCORE_DVFS_ENABLE       (0)
#define GPUFREQ_CHECK_MTCMOS_PWR_STATUS (1)

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
#define MFGPLL_GPU_CON1                 (g_mfg_pll_base + 0x00C)
#define MFGPLL_STACK_CON1               (g_mfg_scpll_base + 0x00C)
#define PWR_STATUS_OFS                  (0xF3C)
#define PWR_STATUS_2ND_OFS              (0xF40)
#define MFG_0_1_PWR_MASK                (0x6)           /* 0000 0000 0000 0000 0110 */
#define MFG_0_18_PWR_MASK               (0xFFFFE)       /* 1111 1111 1111 1111 1110 */
#define MFG_1_18_PWR_MASK               (0xFFFFC)       /* 1111 1111 1111 1111 1100 */

/**************************************************
 * Shader Present Setting
 **************************************************/
#define MFG3_SHADER_STACK0              (T0C0 | T0C1)   /* MFG3, MFG9, MFG12 */
#define MFG4_SHADER_STACK1              (T1C0)          /* MFG4, MFG10 */
#define MFG5_SHADER_STACK2              (T2C0)          /* MFG5, MFG11 */
#define MFG6_SHADER_STACK4              (T4C0 | T4C1)   /* MFG6, MFG13, MFG16 */
#define MFG7_SHADER_STACK5              (T5C0 | T5C1)   /* MFG7, MFG14, MFG17 */
#define MFG8_SHADER_STACK6              (T6C0 | T6C1)   /* MFG8, MFG15, MFG18 */

#define GPU_SHADER_PRESENT_10 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG6_SHADER_STACK4 | MFG7_SHADER_STACK5 | MFG8_SHADER_STACK6)

/**************************************************
 * Reference Power Setting
 **************************************************/
#define GPU_ACT_REF_POWER               (0)             /* mW  */
#define GPU_ACT_REF_FREQ                (0)             /* KHz */
#define GPU_ACT_REF_VOLT                (0)             /* mV x 100 */
#define GPU_LEAKAGE_POWER               (0)
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

#define GPUFREQ_BATT_OC_IDX             (34)
#define GPUFREQ_BATT_PERCENT_IDX        (34)
#define GPUFREQ_LOW_BATT_IDX            (34)

/**************************************************
 * Adaptive Volt Scaling (AVS) Setting
 **************************************************/
#define GPUFREQ_AVS_ENABLE              (0)

/**************************************************
 * Aging Sensor Setting
 **************************************************/
#define GPUFREQ_ASENSOR_ENABLE          (0)
#define GPUFREQ_AGING_KEEP_FREQ         (350000)
#define GPUFREQ_AGING_KEEP_VOLT         (65000)
#define GPUFREQ_AGING_GAP_MIN           (-3)
#define GPUFREQ_AGING_GAP_1             (2)
#define GPUFREQ_AGING_GAP_2             (4)
#define GPUFREQ_AGING_GAP_3             (6)
#define GPUFREQ_AGING_MOST_AGRRESIVE_IDX (0)

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
#define DVFS_TIMING_PARK_VOLT           (65000)
/* DELSEL ULV SRAM access */
#define DELSEL_ULV_PARK_VOLT            (55000)

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_segment {
	MT6983_SEGMENT = 1,
	MT6981_SEGMENT,
};

enum gpufreq_clk_src {
	CLOCK_MAIN = 0,
	CLOCK_SUB,
};

/**************************************************
 * Structure
 **************************************************/
struct gpufreq_pmic_info {
	struct regulator *reg_vgpu;
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
	struct platform_device *mfg1_pdev;  /* MPU, CM7 */
#if !GPUFREQ_PDCv2_ENABLE
	struct platform_device *mfg2_pdev;  /* L2 Cache */
	struct platform_device *mfg3_pdev;  /* ST0      */
	struct platform_device *mfg4_pdev;  /* ST1      */
	struct platform_device *mfg5_pdev;  /* ST2      */
	struct platform_device *mfg6_pdev;  /* ST4      */
	struct platform_device *mfg7_pdev;  /* ST5      */
	struct platform_device *mfg8_pdev;  /* ST6      */
	struct platform_device *mfg9_pdev;  /* ST0T0    */
	struct platform_device *mfg10_pdev; /* ST1T0    */
	struct platform_device *mfg11_pdev; /* ST2T0    */
	struct platform_device *mfg12_pdev; /* ST0T1    */
	struct platform_device *mfg13_pdev; /* ST4T0    */
	struct platform_device *mfg14_pdev; /* ST5T0    */
	struct platform_device *mfg15_pdev; /* ST6T0    */
	struct platform_device *mfg16_pdev; /* ST4T1    */
	struct platform_device *mfg17_pdev; /* ST5T1    */
	struct platform_device *mfg18_pdev; /* ST6T1    */
#endif /* GPUFREQ_PDCv2_ENABLE */
};

struct gpufreq_mfg_fp {
	int (*probe)(struct platform_device *pdev);
	int (*remove)(struct platform_device *pdev);
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
#define GPUOP(_freq, _volt, _vsram, _postdiv, _vaging, _power) \
	{                                  \
		.freq = _freq,                 \
		.volt = _volt,                 \
		.vsram = _vsram,               \
		.postdiv = _postdiv,           \
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

#define SEGMENT_ADJ_STACK_1_NUM         ARRAY_SIZE(g_segment_adj_stack_1)
struct gpufreq_adj_info g_segment_adj_stack_1[] = {
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
/*
 * todo: Need to check correct table for applying aging
 */
unsigned int g_aging_table[][SIGNED_OPP_STACK_NUM] = {
	{ /* aging table 0 */
		1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, /* OPP 0~9   */
		1875, 1875, 1875, 1875, 1875, 1250, 1250, 1250, 1250, 1250, /* OPP 10~19 */
		1250, 1250, 1250, 1250, 1250, 1250, 625,  625,  625,  625,  /* OPP 20~29 */
		625,  625,  625,  625,  625,  625,  625,  625,  625,  625,  /* OPP 30~39 */
		625,                                                        /* OPP 40    */
	},
	{ /* aging table 1 */
		1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, /* OPP 0~9   */
		1250, 1250, 1250, 1250, 1250, 625,  625,  625,  625,  625,  /* OPP 10~19 */
		625,  625,  625,  625,  625,  625,  0,    0,    0,    0,    /* OPP 20~29 */
		0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    /* OPP 30~39 */
		0,                                                          /* OPP 40    */
	},
	{
		625, 625, 625, 625, 625, 625, 625, 625, 625, 625,           /* OPP 0~9   */
		625, 625, 625, 625, 625, 0,   0,   0,   0,   0,             /* OPP 10~19 */
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,             /* OPP 20~29 */
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,             /* OPP 30~39 */
		0,                                                          /* OPP 40    */
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
