/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6879_H__
#define __GPUFREQ_MT6879_H__

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
#define GPUFREQ_HWAPM_ENABLE            (0)
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
#define MFGPLL_STACK_CON1               (g_mfg_pll_base + 0x03C)

/**************************************************
 * Shader Present Setting
 **************************************************/
#define MFG2_SHADER_STACK0              (T0C0)        /* MC0 */
#define MFG3_SHADER_STACK1              (T1C0 | T1C1) /* MC1, 2 */
#define MFG4_SHADER_STACK2              (T2C0 | T2C1) /* MC3, 4 */
#define MFG5_SHADER_STACK5              (T5C0 | T5C1) /* MC5, 6 */
#define MFG6_SHADER_STACK6              (T6C0 | T6C1) /* MC7, 8 */

#define GPU_SHADER_PRESENT_1 \
	(MFG2_SHADER_STACK0)
#define GPU_SHADER_PRESENT_2 \
	(MFG3_SHADER_STACK1)
#define GPU_SHADER_PRESENT_3 \
	(MFG2_SHADER_STACK0 | MFG3_SHADER_STACK1)
#define GPU_SHADER_PRESENT_4 \
	(MFG3_SHADER_STACK1 | MFG4_SHADER_STACK2)
#define GPU_SHADER_PRESENT_5 \
	(MFG2_SHADER_STACK0 | MFG3_SHADER_STACK1 | MFG4_SHADER_STACK2)
#define GPU_SHADER_PRESENT_6 \
	(MFG3_SHADER_STACK1 | MFG4_SHADER_STACK2 | MFG5_SHADER_STACK5)
#define GPU_SHADER_PRESENT_7 \
	(MFG2_SHADER_STACK0 | MFG3_SHADER_STACK1 | MFG4_SHADER_STACK2 | \
	MFG5_SHADER_STACK5)
#define GPU_SHADER_PRESENT_8 \
	(MFG3_SHADER_STACK1 | MFG4_SHADER_STACK2 | MFG5_SHADER_STACK5 | \
	MFG6_SHADER_STACK6)
#define GPU_SHADER_PRESENT_9 \
	(MFG2_SHADER_STACK0 | MFG3_SHADER_STACK1 | MFG4_SHADER_STACK2 | \
	MFG5_SHADER_STACK5 | MFG6_SHADER_STACK6)

/**************************************************
 * Reference Power Setting
 **************************************************/
#define GPU_ACT_REF_POWER               (1571)          /* mW  */
#define GPU_ACT_REF_FREQ                (1000000)       /* KHz */
#define GPU_ACT_REF_VOLT                (80000)         /* mV x 100 */
#define GPU_LEAKAGE_POWER               (130)

/**************************************************
 * PMIC Setting
 **************************************************/
/*
 * PMIC hardware range:
 * vgpu      0.3 ~ 1.19375 V
 * vsram     0.5 ~ 1.29375 V
 */
#define VGPU_MAX_VOLT                   (119375)        /* mV x 100 */
#define VGPU_MIN_VOLT                   (30000)         /* mV x 100 */
#define VSRAM_MAX_VOLT                  (129375)        /* mV x 100 */
#define VSRAM_MIN_VOLT                  (50000)         /* mV x 100 */
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
#define SRAM_PARK_VOLT                  (75000)

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
#define GPUFREQ_AGING_KEEP_FREQ         (385000)
#define GPUFREQ_AGING_KEEP_VOLT         (65000)
#define GPUFREQ_AGING_GAP_MIN           (-3)
#define GPUFREQ_AGING_GAP_1             (2)
#define GPUFREQ_AGING_GAP_2             (4)
#define GPUFREQ_AGING_GAP_3             (6)
#define GPUFREQ_AGING_MOST_AGRRESIVE_IDX (0)

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_segment {
	MT6879_SEGMENT = 1,
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
	struct regulator *reg_vsram;
};

struct gpufreq_clk_info {
	struct clk *clk_mux;
	struct clk *clk_main_parent;
	struct clk *clk_sub_parent;
	struct clk *subsys_mfg_cg;
};

struct gpufreq_mtcmos_info {
	struct platform_device *mfg1_pdev;
	struct platform_device *mfg2_pdev;
	struct platform_device *mfg3_pdev;
	struct platform_device *mfg4_pdev;
	struct platform_device *mfg5_pdev;
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
	GPUOP(1000000, 80000, VSRAM_LEVEL_1, POSDIV_POWER_2, 0, 0), /*  0 sign off */
	GPUOP(986000,  79375, VSRAM_LEVEL_1, POSDIV_POWER_2, 0, 0), /*  1 */
	GPUOP(972000,  78750, VSRAM_LEVEL_1, POSDIV_POWER_2, 0, 0), /*  2 */
	GPUOP(958000,  78125, VSRAM_LEVEL_1, POSDIV_POWER_2, 0, 0), /*  3 */
	GPUOP(945000,  77500, VSRAM_LEVEL_1, POSDIV_POWER_4, 0, 0), /*  4 */
	GPUOP(931000,  76875, VSRAM_LEVEL_1, POSDIV_POWER_4, 0, 0), /*  5 */
	GPUOP(917000,  76250, VSRAM_LEVEL_1, POSDIV_POWER_4, 0, 0), /*  6 */
	GPUOP(903000,  75625, VSRAM_LEVEL_1, POSDIV_POWER_4, 0, 0), /*  7 */
	GPUOP(890000,  75000, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /*  8 sign off */
	GPUOP(876000,  74375, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /*  9 */
	GPUOP(862000,  73750, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 10 */
	GPUOP(848000,  73125, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 11 */
	GPUOP(835000,  72500, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 12 */
	GPUOP(821000,  71875, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 13 */
	GPUOP(807000,  71250, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 14 */
	GPUOP(793000,  70625, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 15 */
	GPUOP(780000,  70000, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 16 */
	GPUOP(766000,  69375, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 17 */
	GPUOP(752000,  68750, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 18 */
	GPUOP(738000,  68125, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 19 */
	GPUOP(725000,  67500, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 20 */
	GPUOP(711000,  66875, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 21 */
	GPUOP(697000,  66250, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 22 */
	GPUOP(683000,  65625, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 23 */
	GPUOP(670000,  65000, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 24 sign off */
	GPUOP(652000,  64375, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 25 */
	GPUOP(634000,  63750, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 26 */
	GPUOP(616000,  63125, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 27 */
	GPUOP(598000,  62500, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 28 */
	GPUOP(580000,  61875, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0),  /* 29 */
	GPUOP(563000,  61250, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0),  /* 30 */
	GPUOP(545000,  60625, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0),  /* 31 */
	GPUOP(527000,  60000, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0),  /* 32 */
	GPUOP(509000,  59375, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0),  /* 33 */
	GPUOP(491000,  58750, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0),  /* 34 */
	GPUOP(474000,  58125, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0),  /* 35 */
	GPUOP(456000,  57500, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0),  /* 36 */
	GPUOP(438000,  56875, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0),  /* 37 */
	GPUOP(420000,  56250, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0),  /* 38 */
	GPUOP(402000,  55625, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0),  /* 39 */
	GPUOP(385000,  55000, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0),  /* 40 sign off */
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

#define SEGMENT_ADJ_GPU_1_NUM         ARRAY_SIZE(g_segment_adj_gpu_1)
struct gpufreq_adj_info g_segment_adj_gpu_1[] = {
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
		1880, 1880, 1880, 1880, 1880, 1880, 1880, 1880, 1880, 1880, /* OPP 0~9   */
		1880, 1880, 1880, 1880, 1880, 1250, 1250, 1250, 1250, 1250, /* OPP 10~19 */
		1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 625,  /* OPP 20~29 */
		625,  625,  625,  625,  625,  625,  625,  625,  625,  625,  /* OPP 30~39 */
		625,                                                        /* OPP 40    */
	},
	{ /* aging table 1 */
		1880, 1880, 1880, 1880, 1880, 1880, 1880, 1880, 1880, 1880, /* OPP 0~9   */
		1880, 1880, 1880, 1880, 1880, 1250, 1250, 1250, 1250, 1250, /* OPP 10~19 */
		1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 625,  /* OPP 20~29 */
		625,  625,  625,  625,  625,  625,  625,  625,  625,  625,  /* OPP 30~39 */
		625,                                                        /* OPP 40    */
	},
	{ /* aging table 2 */
		1880, 1880, 1880, 1880, 1880, 1880, 1880, 1880, 1880, 1880, /* OPP 0~9   */
		1880, 1880, 1880, 1880, 1880, 1250, 1250, 1250, 1250, 1250, /* OPP 10~19 */
		1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 625,  /* OPP 20~29 */
		625,  625,  625,  625,  625,  625,  625,  625,  625,  625,  /* OPP 30~39 */
		625,                                                        /* OPP 40    */
	},
	{ /* aging table 3 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 0~9   */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 10~19 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 20~29 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 30~39 */
		0,                            /* OPP 40    */
	},
};

#endif /* __GPUFREQ_MT6879_H__ */
