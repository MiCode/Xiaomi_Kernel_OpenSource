/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6893_H__
#define __GPUFREQ_MT6893_H__

/**************************************************
 * GPUFREQ Local Config
 **************************************************/
#define GPUFREQ_BRINGUP                 (0)
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
/* external module control */
#define GPUFREQ_THERMAL_ENABLE          (0)
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
#define MFGPLL_CON1                     (g_apmixed_base + 0x026C)

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
#define GPU_ACT_REF_POWER               (3352)          /* mW  */
#define GPU_ACT_REF_FREQ                (886000)        /* KHz */
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
 * (-100)mv <= (VSRAM - VGPU) <= (300)mV
 */
#define MAX_BUCK_DIFF                   (30000)         /* mV x 100 */
#define MIN_BUCK_DIFF                   (-10000)        /* mV x 100 */
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
#define GPUFREQ_AVS_KEEP_FREQ           (836000)
#define GPUFREQ_AVS_KEEP_VOLT           (75000)
#define AVS_NUM                         ARRAY_SIZE(g_avs_to_opp)
int g_avs_to_opp[] = {
	0, 8, 11, 14,
	17, 20, 22, 24,
	26, 28, 30, 32,
	34, 36, 38, 40
};

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_segment {
	MT6891_SEGMENT = 1,
	MT6893_SEGMENT,
};

enum gpufreq_clk_src {
	CLOCK_SUB = 0,
	CLOCK_MAIN,
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
	struct device *mfg0_dev;
	struct device *mfg1_dev;
	struct device *mfg2_dev;
	struct device *mfg3_dev;
	struct device *mfg4_dev;
	struct device *mfg5_dev;
	struct device *mfg6_dev;
};

struct gpufreq_adj_info {
	int oppidx;
	unsigned int freq;
	unsigned int volt;
	unsigned int vsram;
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

#define SIGNED_OPP_NUM_GPU              ARRAY_SIZE(g_default_gpu_segment)
struct gpufreq_opp_info g_default_gpu_segment[] = {
	GPUOP(886000, 80000, 80000, POSDIV_POWER_4, 1875, 0), /*  0 sign off */
	GPUOP(879000, 79375, 79375, POSDIV_POWER_4, 1875, 0), /*  1 */
	GPUOP(873000, 78750, 78750, POSDIV_POWER_4, 1875, 0), /*  2 */
	GPUOP(867000, 78125, 78125, POSDIV_POWER_4, 1875, 0), /*  3 */
	GPUOP(861000, 77500, 77500, POSDIV_POWER_4, 1875, 0), /*  4 */
	GPUOP(854000, 76875, 76875, POSDIV_POWER_4, 1875, 0), /*  5 */
	GPUOP(848000, 76250, 76250, POSDIV_POWER_4, 1875, 0), /*  6 */
	GPUOP(842000, 75625, 75625, POSDIV_POWER_4, 1875, 0), /*  7 */
	GPUOP(836000, 75000, 75000, POSDIV_POWER_4, 1875, 0), /*  8 sign off */
	GPUOP(825000, 74375, 75000, POSDIV_POWER_4, 1875, 0), /*  9 */
	GPUOP(815000, 73750, 75000, POSDIV_POWER_4, 1875, 0), /* 10 */
	GPUOP(805000, 73125, 75000, POSDIV_POWER_4, 1875, 0), /* 11 */
	GPUOP(795000, 72500, 75000, POSDIV_POWER_4, 1875, 0), /* 12 */
	GPUOP(785000, 71875, 75000, POSDIV_POWER_4, 1875, 0), /* 13 */
	GPUOP(775000, 71250, 75000, POSDIV_POWER_4, 1875, 0), /* 14 */
	GPUOP(765000, 70625, 75000, POSDIV_POWER_4, 1875, 0), /* 15 */
	GPUOP(755000, 70000, 75000, POSDIV_POWER_4, 1875, 0), /* 16 */
	GPUOP(745000, 69375, 75000, POSDIV_POWER_4, 1875, 0), /* 17 */
	GPUOP(735000, 68750, 75000, POSDIV_POWER_4, 1875, 0), /* 18 */
	GPUOP(725000, 68125, 75000, POSDIV_POWER_4, 1875, 0), /* 19 */
	GPUOP(715000, 67500, 75000, POSDIV_POWER_4, 1875, 0), /* 20 */
	GPUOP(705000, 66875, 75000, POSDIV_POWER_4, 1250, 0), /* 21 */
	GPUOP(695000, 66250, 75000, POSDIV_POWER_4, 1250, 0), /* 22 */
	GPUOP(685000, 65625, 75000, POSDIV_POWER_4, 1250, 0), /* 23 */
	GPUOP(675000, 65000, 75000, POSDIV_POWER_4, 1250, 0), /* 24 sign off */
	GPUOP(654000, 64375, 75000, POSDIV_POWER_4, 1250, 0), /* 25 */
	GPUOP(634000, 63750, 75000, POSDIV_POWER_4, 1250, 0), /* 26 */
	GPUOP(614000, 63125, 75000, POSDIV_POWER_4, 1250, 0), /* 27 */
	GPUOP(593000, 62500, 75000, POSDIV_POWER_4, 1250, 0), /* 28 */
	GPUOP(573000, 61875, 75000, POSDIV_POWER_4, 1250, 0), /* 29 */
	GPUOP(553000, 61250, 75000, POSDIV_POWER_4, 1250, 0), /* 30 */
	GPUOP(532000, 60625, 75000, POSDIV_POWER_4, 1250, 0), /* 31 */
	GPUOP(512000, 60000, 75000, POSDIV_POWER_4, 625, 0),  /* 32 */
	GPUOP(492000, 59375, 75000, POSDIV_POWER_4, 625, 0),  /* 33 */
	GPUOP(471000, 58750, 75000, POSDIV_POWER_4, 625, 0),  /* 34 */
	GPUOP(451000, 58125, 75000, POSDIV_POWER_4, 625, 0),  /* 35 */
	GPUOP(431000, 57500, 75000, POSDIV_POWER_4, 625, 0),  /* 36 */
	GPUOP(410000, 56875, 75000, POSDIV_POWER_4, 625, 0),  /* 37 */
	GPUOP(390000, 56250, 75000, POSDIV_POWER_4, 625, 0),  /* 38 */
	GPUOP(370000, 55625, 75000, POSDIV_POWER_8, 625, 0),  /* 39 */
	GPUOP(350000, 55000, 75000, POSDIV_POWER_8, 625, 0),  /* 40 sign off */
};

#define ADJOP(_oppidx, _freq, _volt, _vsram) \
	{                                  \
		.oppidx = _oppidx,             \
		.freq = _freq,                 \
		.volt = _volt,                 \
		.vsram = _vsram,               \
	}

#define ADJ_GPU_SEGMENT_1_NUM           ARRAY_SIZE(g_adj_gpu_segment_1)
struct gpufreq_adj_info g_adj_gpu_segment_1[] = {
	ADJOP(25, 0, 65000, 0),
	ADJOP(26, 0, 64375, 0),
	ADJOP(27, 0, 64375, 0),
	ADJOP(28, 0, 63750, 0),
	ADJOP(29, 0, 63750, 0),
	ADJOP(30, 0, 63125, 0),
	ADJOP(31, 0, 63125, 0),
	ADJOP(32, 0, 62500, 0),
	ADJOP(33, 0, 62500, 0),
	ADJOP(34, 0, 61875, 0),
	ADJOP(35, 0, 61875, 0),
	ADJOP(36, 0, 61250, 0),
	ADJOP(37, 0, 61250, 0),
	ADJOP(38, 0, 60625, 0),
	ADJOP(39, 0, 60625, 0),
	ADJOP(40, 0, 60000, 0), /* sign off */
};

/* for mcl50 swrgo */
#define ADJ_GPU_SEGMENT_2_NUM           ARRAY_SIZE(g_adj_gpu_segment_2)
struct gpufreq_adj_info g_adj_gpu_segment_2[] = {
	ADJOP(0,  0, 70000, 75000), /* sign off */
	ADJOP(1,  0, 70000, 75000),
	ADJOP(2,  0, 69375, 75000),
	ADJOP(3,  0, 69375, 75000),
	ADJOP(4,  0, 68750, 75000),
	ADJOP(5,  0, 68125, 75000),
	ADJOP(6,  0, 68125, 75000),
	ADJOP(7,  0, 67500, 75000),
	ADJOP(8,  0, 66875, 0), /* sign off */
	ADJOP(9,  0, 66875, 0),
	ADJOP(10, 0, 66875, 0),
	ADJOP(11, 0, 66250, 0),
	ADJOP(12, 0, 65625, 0),
	ADJOP(13, 0, 65000, 0),
	ADJOP(14, 0, 64375, 0),
	ADJOP(15, 0, 64375, 0),
	ADJOP(16, 0, 64375, 0),
	ADJOP(17, 0, 63750, 0),
	ADJOP(18, 0, 63125, 0),
	ADJOP(19, 0, 62500, 0),
	ADJOP(20, 0, 61875, 0),
	ADJOP(21, 0, 61875, 0),
	ADJOP(22, 0, 61250, 0),
	ADJOP(23, 0, 61250, 0),
	ADJOP(24, 0, 60625, 0), /* sign off */
	ADJOP(25, 0, 60625, 0),
	ADJOP(26, 0, 60000, 0),
	ADJOP(27, 0, 60000, 0),
	ADJOP(28, 0, 59375, 0),
	ADJOP(29, 0, 59375, 0),
	ADJOP(30, 0, 58750, 0),
	ADJOP(31, 0, 58750, 0),
	ADJOP(32, 0, 58125, 0),
	ADJOP(33, 0, 58125, 0),
	ADJOP(34, 0, 57500, 0),
	ADJOP(35, 0, 57500, 0),
	ADJOP(36, 0, 56875, 0),
	ADJOP(37, 0, 56250, 0),
	ADJOP(38, 0, 55625, 0),
};

#define ADJ_GPU_CUSTOM_NUM              ARRAY_SIZE(g_adj_gpu_custom)
struct gpufreq_adj_info g_adj_gpu_custom[] = {
	ADJOP(0, 0, 0, 0),
};

#endif /* __GPUFREQ_MT6893_H__ */
