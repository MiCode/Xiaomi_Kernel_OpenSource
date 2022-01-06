/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6855_H__
#define __GPUFREQ_MT6855_H__

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
#define CLK_MUX_OFS                     (0x1034)
#define CKMUX_SEL_REF_CORE              (4)
#define CKMUX_SEL_REF_CORE_MASK         (0x10)
#define CKMUX_SEL_REF_PARK              (5)
#define CKMUX_SEL_REF_PARK_MASK         (0x20)
#define PLL4H_FQMTR_CON0                (g_mfg_pll_base + 0x200)
#define PLL4H_FQMTR_CON1                (g_mfg_pll_base + 0x204)
#define FQMTR_PLL1_ID                   (0x0)
#define FQMTR_PLL4_ID                   (0x3)

/**************************************************
 * Frequency Hopping Setting
 **************************************************/
#define GPUFREQ_FHCTL_ENABLE            (0)
/* todo: fhctl pll name */
#define MFG_PLL_NAME                    "mfgpll0"

/**************************************************
 * Power Domain Setting
 **************************************************/
#define GPUFREQ_HWAPM_ENABLE            (1)
#define GPUFREQ_CHECK_MTCMOS_PWR_STATUS (1)
#define PWR_STATUS_OFS                  (0xF3C)
#define PWR_STATUS_2ND_OFS              (0xF40)
#define MFG_0_1_PWR_MASK                (0x6)           /* 0000 0110 */
#define MFG_1_2_PWR_MASK                (0xC)           /* 0000 1100 */
#define MFG_0_2_PWR_MASK                (0xE)           /* 0000 1110 */

/**************************************************
 * Reference Power Setting
 **************************************************/
#define GPU_ACT_REF_POWER               (1331)          /* mW  */
#define GPU_ACT_REF_FREQ                (950000)        /* KHz */
#define GPU_ACT_REF_VOLT                (80000)         /* mV x 100 */
#define GPU_LEAKAGE_POWER               (30)

/**************************************************
 * PMIC Setting
 **************************************************/
/*
 * PMIC hardware range:
 * VGPU      0.4 ~ 1.19375 V (MT6363 VBUCK5)
 * VSRAM     0.4 ~ 1.19375 V (MT6369 VSRAM CORE)
 */
#define VGPU_MAX_VOLT                   (119375)        /* mV x 100 */
#define VGPU_MIN_VOLT                   (40000)         /* mV x 100 */
#define VSRAM_MAX_VOLT                  (119375)        /* mV x 100 */
#define VSRAM_MIN_VOLT                  (40000)         /* mV x 100 */
#define PMIC_STEP                       (625)           /* mV x 100 */
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
#define GPUFREQ_AGING_KEEP_FGPU         (660000)
#define GPUFREQ_AGING_KEEP_VGPU         (65000)
#define GPUFREQ_AGING_LKG_VGPU          (70000)
#define GPUFREQ_AGING_GAP_MIN           (-3)
#define GPUFREQ_AGING_GAP_1             (2)
#define GPUFREQ_AGING_GAP_2             (4)
#define GPUFREQ_AGING_GAP_3             (6)
#define GPUFREQ_AGING_MAX_TABLE_IDX     (1)
#define GPUFREQ_AGING_MOST_AGRRESIVE    (0)

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_segment {
	MT6855_SEGMENT = 1,
};

enum gpufreq_clk_src {
	CLOCK_MAIN = 0,
	CLOCK_SUB,
};

enum gpufreq_transaction_mode {
	MERGER_OFF = 0,
	MERGER_LIGHT,
};

/**************************************************
 * Structure
 **************************************************/
struct gpufreq_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram;
};

struct gpufreq_clk_info {
	struct clk *clk_main_parent;
	struct clk *clk_sub_parent;
};

struct gpufreq_mtcmos_info {
	struct device *mfg1_dev;
#if !GPUFREQ_HWAPM_ENABLE
	struct device *mfg2_dev;
#endif /* GPUFREQ_HWAPM_ENABLE */
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
	GPUOP(950000, 80000, VSRAM_LEVEL_1, POSDIV_POWER_4, 0, 0), /*  0 sign off */
	GPUOP(943000, 79375, VSRAM_LEVEL_1, POSDIV_POWER_4, 0, 0), /*  1 */
	GPUOP(937000, 78750, VSRAM_LEVEL_1, POSDIV_POWER_4, 0, 0), /*  2 */
	GPUOP(931000, 78125, VSRAM_LEVEL_1, POSDIV_POWER_4, 0, 0), /*  3 */
	GPUOP(925000, 77500, VSRAM_LEVEL_1, POSDIV_POWER_4, 0, 0), /*  4 */
	GPUOP(918000, 76875, VSRAM_LEVEL_1, POSDIV_POWER_4, 0, 0), /*  5 */
	GPUOP(912000, 76250, VSRAM_LEVEL_1, POSDIV_POWER_4, 0, 0), /*  6 */
	GPUOP(906000, 75625, VSRAM_LEVEL_1, POSDIV_POWER_4, 0, 0), /*  7 */
	GPUOP(900000, 75000, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /*  8 sign off */
	GPUOP(885000, 74375, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /*  9 */
	GPUOP(870000, 73750, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 10 */
	GPUOP(855000, 73125, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 11 */
	GPUOP(840000, 72500, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 12 */
	GPUOP(825000, 71875, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 13 */
	GPUOP(810000, 71250, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 14 */
	GPUOP(795000, 70625, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 15 */
	GPUOP(780000, 70000, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 16 */
	GPUOP(765000, 69375, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 17 */
	GPUOP(750000, 68750, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 18 */
	GPUOP(735000, 68125, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 19 */
	GPUOP(720000, 67500, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 20 */
	GPUOP(705000, 66875, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 21 */
	GPUOP(690000, 66250, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 22 */
	GPUOP(675000, 65625, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 23 */
	GPUOP(660000, 65000, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 24 sign off */
	GPUOP(637000, 64375, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 25 */
	GPUOP(615000, 63750, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 26 */
	GPUOP(592000, 63125, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 27 */
	GPUOP(570000, 62500, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 28 */
	GPUOP(547000, 61875, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 29 */
	GPUOP(525000, 61250, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 30 */
	GPUOP(502000, 60625, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 31 */
	GPUOP(480000, 60000, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 32 */
	GPUOP(457000, 59375, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 33 */
	GPUOP(435000, 58750, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 34 */
	GPUOP(412000, 58125, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 35 */
	GPUOP(390000, 57500, VSRAM_LEVEL_0, POSDIV_POWER_4, 0, 0), /* 36 sign off */
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
	ADJOP(16, 0, 60625, 0,             0),
	ADJOP(17, 0, 60625, 0,             0),
	ADJOP(18, 0, 60000, 0,             0),
	ADJOP(19, 0, 59375, 0,             0),
	ADJOP(20, 0, 58750, 0,             0),
	ADJOP(21, 0, 58750, 0,             0),
	ADJOP(22, 0, 58125, 0,             0),
	ADJOP(23, 0, 57500, 0,             0),
	ADJOP(24, 0, 56875, 0,             0), /* 24 sign off */
	ADJOP(25, 0, 56875, 0,             0),
	ADJOP(26, 0, 56875, 0,             0),
	ADJOP(27, 0, 56875, 0,             0),
	ADJOP(28, 0, 56250, 0,             0),
	ADJOP(29, 0, 56250, 0,             0),
	ADJOP(30, 0, 56250, 0,             0),
	ADJOP(31, 0, 56250, 0,             0),
	ADJOP(32, 0, 55625, 0,             0),
	ADJOP(33, 0, 55625, 0,             0),
	ADJOP(34, 0, 55625, 0,             0),
	ADJOP(35, 0, 55625, 0,             0),
	ADJOP(36, 0, 55000, 0,             0), /* 36 sign off */
};

/**************************************************
 * AVS Adjustment
 **************************************************/
#define AVS_ADJ_NUM                   ARRAY_SIZE(g_avs_adj)
struct gpufreq_adj_info g_avs_adj[] = {
	ADJOP(0,  0, 0, 0, 0),
	ADJOP(8,  0, 0, 0, 0),
	ADJOP(24, 0, 0, 0, 0),
	ADJOP(36, 0, 0, 0, 0),
};

/**************************************************
 * Aging Adjustment
 **************************************************/
unsigned int g_aging_table[][SIGNED_OPP_GPU_NUM] = {
	{ /* aging table 0 */
		625, 625, 625, 625, 625, 625, 625, 625, 625, 625,  /* OPP 0-9   */
		625, 625, 625, 625, 625, 625, 625, 625, 625, 625,  /* OPP 10-19 */
		625, 625, 625, 625, 625, 625, 625, 625, 625, 625,  /* OPP 20-29 */
		625, 625, 625, 625, 625, 625, 625,                 /* OPP 30-36 */
	},
#if GPUFREQ_ASENSOR_ENABLE
	{ /* aging table 1 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 0-9   */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 10-19 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 20-29 */
		0, 0, 0, 0, 0, 0, 0,          /* OPP 30-36 */
	},
	/* aging table 2: remove for code size */
	/* aging table 3: remove for code size */
#endif /* GPUFREQ_ASENSOR_ENABLE */
};

#endif /* __GPUFREQ_MT6855_H__ */
