/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6835_H__
#define __GPUFREQ_MT6835_H__

/**************************************************
 * GPUFREQ Config
 **************************************************/
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
/* MFGSYS Feature */
#define GPUFREQ_HWDCM_ENABLE            (1)
#define GPUFREQ_VCORE_DVFS_ENABLE       (1)
#define GPUFREQ_MERGER_ENABLE           (1)
#define GPUFREQ_AVS_ENABLE              (1)
#define GPUFREQ_ASENSOR_ENABLE          (0)
#define GPUFREQ_SELF_CTRL_MTCMOS        (1)
#define GPUFREQ_SHARED_STATUS_REG       (0)
#define GPUFREQ_TEMPER_COMP_ENABLE      (1)

/**************************************************
 * Clock Setting
 **************************************************/
#define POSDIV_2_MAX_FREQ               (1900000)         /* KHz */
#define POSDIV_2_MIN_FREQ               (750000)          /* KHz */
#define POSDIV_4_MAX_FREQ               (950000)          /* KHz */
#define POSDIV_4_MIN_FREQ               (375000)          /* KHz */
#define POSDIV_8_MAX_FREQ               (475000)          /* KHz */
#define POSDIV_8_MIN_FREQ               (187500)          /* KHz */
#define POSDIV_16_MAX_FREQ              (237500)          /* KHz */
#define POSDIV_16_MIN_FREQ              (125000)          /* KHz */
#define POSDIV_SHIFT                    (24)              /* bit */
#define DDS_SHIFT                       (14)              /* bit */
#define TO_MHZ_HEAD                     (100)
#define TO_MHZ_TAIL                     (10)
#define ROUNDING_VALUE                  (5)
#define MFGPLL_FIN                      (26)              /* MHz */
#define MFG_PLL_SEL_MASK                (BIT(16))         /* [16] */
#define MFG_REF_SEL_MASK                (GENMASK(17, 16)) /* [17:16] */

/**************************************************
 * Frequency Hopping Setting
 **************************************************/
#define GPUFREQ_FHCTL_ENABLE            (1)
#define MFG_PLL_NAME                    "mfgpll"

/**************************************************
 * Power Domain Setting
 **************************************************/
#define GPUFREQ_CHECK_MTCMOS_PWR_STATUS (0)
#define MFG_0_1_PWR_MASK                (0x6)           /* 0000 0110 */
#define MFG_0_3_PWR_MASK                (0x1E)          /* 0001 1110 */
#define MFG_1_3_PWR_MASK                (0x1C)          /* 0001 1100 */

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
 * Reference Power Setting
 **************************************************/
#define GPU_ACT_REF_POWER               (977)           /* mW  */
#define GPU_ACT_REF_FREQ                (1100000)       /* KHz */
#define GPU_ACT_REF_VOLT                (85000)         /* mV x 100 */
#define GPU_LEAKAGE_POWER               (30)

/**************************************************
 * PMIC Setting
 **************************************************/
/*
 * PMIC hardware range:
 * VCORE     0.4 ~ 1.19375 V (MT6363 VBUCK2)
 * VGPU      0.4 ~ 1.19375 V (MT6363 VBUCK5)
 * VSRAM     0.4 ~ 1.19375 V (MT6363 VBUCK4)
 */
#define VGPU_MAX_VOLT                   (119375)        /* mV x 100 */
#define VGPU_MIN_VOLT                   (40000)         /* mV x 100 */
#define VSRAM_MAX_VOLT                  (119375)        /* mV x 100 */
#define VSRAM_MIN_VOLT                  (40000)         /* mV x 100 */
#define PMIC_STEP                       (625)           /* mV x 100 */
#define VGPU_LEVEL_0                    (55000)         /* mV x 100 */
#define VOLT_NORMALIZATION(volt) \
	((volt % 625) ? (volt - (volt % 625) + 625) : volt)

/**************************************************
 * SRAM Setting
 **************************************************/
#define VSRAM_LEVEL_0                   (75000)
#define VSRAM_LEVEL_1                   (80000)
#define VSRAM_LEVEL_2                   (85000)
#define SRAM_PARK_VOLT                  (75000)

/**************************************************
 * DVFSRC Setting
 **************************************************/
#define MAX_VCORE_LEVEL                 (3)
#define VCORE_LEVEL_0                   (55000)
#define VCORE_LEVEL_1                   (60000)
#define VCORE_LEVEL_2                   (65000)
#define VCORE_LEVEL_3                   (72500)

/**************************************************
 * Power Throttling Setting
 **************************************************/
#define GPUFREQ_BATT_OC_ENABLE          (1)
#define GPUFREQ_LOW_BATT_ENABLE         (1)
#define GPUFREQ_BATT_OC_FREQ            (467000)
#define GPUFREQ_LOW_BATT_FREQ           (467000)

/**************************************************
 * Aging Sensor Setting
 **************************************************/
#define GPUFREQ_AGING_KEEP_FGPU         (660000)
#define GPUFREQ_AGING_KEEP_VGPU         (65000)
#define GPUFREQ_AGING_LKG_VGPU          (70000)
#define GPUFREQ_AGING_KEEP_VSRAM        (65000)
#define GPUFREQ_AGING_LKG_VGPU          (70000)
#define GPUFREQ_AGING_GAP_MIN           (-3)
#define GPUFREQ_AGING_GAP_1             (2)
#define GPUFREQ_AGING_GAP_2             (4)
#define GPUFREQ_AGING_GAP_3             (6)
#define GPUFREQ_AGING_MAX_TABLE_IDX     (1)
#define GPUFREQ_AGING_MOST_AGRRESIVE    (0)

/**************************************************
 * Temperature Compensation Setting
 **************************************************/
#define GPU_MAX_SIGNOFF_VOLT            (90000)
#define TEMPERATURE_DEFAULT             (-274)          /* 'C */
#define TEMPER_COMP_DEFAULT_VOLT        (0)
#define TEMPER_COMP_10_25_VOLT          (2500)          /* mV * 100 */
#define TEMPER_COMP_10_VOLT             (4375)          /* mV * 100 */

/**************************************************
 * SPM MTCMOS Setting
 **************************************************/
/* bus protect control mask */
#define MFG0_PROT_STEP0_0_MASK          (BIT(4))
#define MFG0_PROT_STEP0_0_ACK_MASK      (BIT(4))
#define MFG0_PROT_STEP1_0_MASK          (BIT(9))
#define MFG0_PROT_STEP1_0_ACK_MASK      (BIT(9))
#define MFG1_PROT_STEP0_0_MASK          (BIT(0))
#define MFG1_PROT_STEP0_0_ACK_MASK      (BIT(0))
#define MFG1_PROT_STEP1_0_MASK          (BIT(20))
#define MFG1_PROT_STEP1_0_ACK_MASK      (BIT(20))
#define MFG1_PROT_STEP2_0_MASK          (BIT(1))
#define MFG1_PROT_STEP2_0_ACK_MASK      (BIT(1))
#define MFG1_PROT_STEP3_0_MASK          (BIT(18) | BIT(19))
#define MFG1_PROT_STEP3_0_ACK_MASK      (BIT(18) | BIT(19))
#define MFG1_PROT_STEP4_0_MASK          (BIT(2))
#define MFG1_PROT_STEP4_0_ACK_MASK      (BIT(2))
#define MFG1_PROT_STEP5_0_MASK          (BIT(3))
#define MFG1_PROT_STEP5_0_ACK_MASK      (BIT(3))
/* power control bit mapping */
#define PWR_RST_B                       BIT(0)
#define PWR_ISO                         BIT(1)
#define PWR_ON                          BIT(2)
#define PWR_ON_2ND                      BIT(3)
#define PWR_CLK_DIS                     BIT(4)
#define SRAM_CKISO                      BIT(5)
#define SRAM_ISOINT_B                   BIT(6)
#define SRAM_PDN                        BIT(8)
#define SRAM_SLP_B                      BIT(9)
#define SRAM_PDN_ACK                    BIT(12)
#define SRAM_SLP_B_ACK                  BIT(13)
/* power status bit mapping */
#define MFG0_PWR_STA_MASK               BIT(1)
#define MFG1_PWR_STA_MASK               BIT(2)
#define MFG2_PWR_STA_MASK               BIT(3)
#define MFG3_PWR_STA_MASK               BIT(4)

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_segment {
	ENG_SEGMENT = 0,
	MT6835_SEGMENT = 1,
	MT6835M_SEGMENT = 2,
	MT6835T_SEGMENT = 3,
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
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram;
};

struct gpufreq_clk_info {
	struct clk *clk_mux;
	struct clk *clk_ref_mux;
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
	unsigned int cur_vcore;
};

/**************************************************
 * GPU Platform OPP Table Definition
 **************************************************/
#define GPU_SIGNED_OPP_0				(0)
#define GPU_SIGNED_OPP_1				(32)
#define GPU_SIGNED_OPP_2				(44)
#define NUM_GPU_SIGNED_IDX				ARRAY_SIZE(g_gpu_signed_idx)
#define NUM_GPU_SIGNED_OPP				ARRAY_SIZE(g_gpu_default_opp_table)
static const int g_gpu_signed_idx[] = {
	GPU_SIGNED_OPP_0,
	GPU_SIGNED_OPP_1,
	GPU_SIGNED_OPP_2,
};

static struct gpufreq_opp_info g_gpu_default_opp_table[] = {
	GPUOP(1100000, 90000, 90000, POSDIV_POWER_2, 0, 0), /*  0 sign off */
	GPUOP(1086000, 89375, 89375, POSDIV_POWER_2, 0, 0), /*  1 */
	GPUOP(1072000, 88750, 88750, POSDIV_POWER_2, 0, 0), /*  2 */
	GPUOP(1058000, 88125, 88125, POSDIV_POWER_2, 0, 0), /*  3 */
	GPUOP(1045000, 87500, 87500, POSDIV_POWER_2, 0, 0), /*  4 */
	GPUOP(1031000, 86875, 86875, POSDIV_POWER_2, 0, 0), /*  5 */
	GPUOP(1017000, 86250, 86250, POSDIV_POWER_2, 0, 0), /*  6 */
	GPUOP(1003000, 85625, 85625, POSDIV_POWER_2, 0, 0), /*  7 */
	GPUOP(990000,  85000, 85000, POSDIV_POWER_2, 0, 0), /*  8 */
	GPUOP(976000,  84375, 84375, POSDIV_POWER_2, 0, 0), /*  9 */
	GPUOP(962000,  83125, 83125, POSDIV_POWER_2, 0, 0), /* 10 */
	GPUOP(948000,  82500, 82500, POSDIV_POWER_4, 0, 0), /* 11 */
	GPUOP(935000,  81875, 81875, POSDIV_POWER_4, 0, 0), /* 12 */
	GPUOP(921000,  81250, 81250, POSDIV_POWER_4, 0, 0), /* 13 */
	GPUOP(907000,  80625, 80625, POSDIV_POWER_4, 0, 0), /* 14 */
	GPUOP(893000,  80000, 80000, POSDIV_POWER_4, 0, 0), /* 15 */
	GPUOP(880000,  79375, 79375, POSDIV_POWER_4, 0, 0), /* 16 */
	GPUOP(868000,  78750, 78750, POSDIV_POWER_4, 0, 0), /* 17 */
	GPUOP(857000,  78125, 78125, POSDIV_POWER_4, 0, 0), /* 18 */
	GPUOP(846000,  77500, 77500, POSDIV_POWER_4, 0, 0), /* 19 */
	GPUOP(835000,  76875, 76875, POSDIV_POWER_4, 0, 0), /* 20 */
	GPUOP(823000,  76250, 76250, POSDIV_POWER_4, 0, 0), /* 21 */
	GPUOP(812000,  75625, 75625, POSDIV_POWER_4, 0, 0), /* 22 */
	GPUOP(801000,  75625, 75625, POSDIV_POWER_4, 0, 0), /* 23 */
	GPUOP(790000,  75000, 75000, POSDIV_POWER_4, 0, 0), /* 24 */
	GPUOP(778000,  74375, 75000, POSDIV_POWER_4, 0, 0), /* 25 */
	GPUOP(767000,  73750, 75000, POSDIV_POWER_4, 0, 0), /* 26 */
	GPUOP(756000,  73125, 75000, POSDIV_POWER_4, 0, 0), /* 27 */
	GPUOP(745000,  72500, 75000, POSDIV_POWER_4, 0, 0), /* 28 */
	GPUOP(733000,  71875, 75000, POSDIV_POWER_4, 0, 0), /* 29 */
	GPUOP(722000,  71250, 75000, POSDIV_POWER_4, 0, 0), /* 30 */
	GPUOP(711000,  70625, 75000, POSDIV_POWER_4, 0, 0), /* 31 */
	GPUOP(700000,  70000, 75000, POSDIV_POWER_4, 0, 0), /* 32 sign off*/
	GPUOP(674000,  70000, 75000, POSDIV_POWER_4, 0, 0), /* 33 */
	GPUOP(648000,  70000, 75000, POSDIV_POWER_4, 0, 0), /* 34 */
	GPUOP(622000,  69375, 75000, POSDIV_POWER_4, 0, 0), /* 35 */
	GPUOP(596000,  69375, 75000, POSDIV_POWER_4, 0, 0), /* 36 */
	GPUOP(570000,  69375, 75000, POSDIV_POWER_4, 0, 0), /* 37 */
	GPUOP(545000,  68750, 75000, POSDIV_POWER_4, 0, 0), /* 38 */
	GPUOP(519000,  68750, 75000, POSDIV_POWER_4, 0, 0), /* 39 */
	GPUOP(493000,  68750, 75000, POSDIV_POWER_4, 0, 0), /* 40 */
	GPUOP(467000,  68125, 75000, POSDIV_POWER_4, 0, 0), /* 41 */
	GPUOP(441000,  68125, 75000, POSDIV_POWER_4, 0, 0), /* 42 */
	GPUOP(415000,  68125, 75000, POSDIV_POWER_4, 0, 0), /* 43 */
	GPUOP(390000,  67500, 75000, POSDIV_POWER_4, 0, 0), /* 44 sign off*/
};

/**************************************************
 * OPP Adjustment
 **************************************************/
static struct gpufreq_adj_info g_avs_table[NUM_GPU_SIGNED_IDX] = {
	ADJOP(GPU_SIGNED_OPP_0, 0, 0, 0),
	ADJOP(GPU_SIGNED_OPP_1, 0, 0, 0),
	ADJOP(GPU_SIGNED_OPP_2, 0, 0, 0),
};

static struct gpufreq_adj_info g_gpu_aging_table[][NUM_GPU_SIGNED_IDX] = {
	{ /* aging table 0 */
		ADJOP(GPU_SIGNED_OPP_0, 0, 625, 0),
		ADJOP(GPU_SIGNED_OPP_1, 0, 625, 0),
		ADJOP(GPU_SIGNED_OPP_2, 0, 625, 0),
	},
	{ /* aging table 1 */
		ADJOP(GPU_SIGNED_OPP_0, 0, 0, 0),
		ADJOP(GPU_SIGNED_OPP_1, 0, 0, 0),
		ADJOP(GPU_SIGNED_OPP_2, 0, 0, 0),
	},
	/* aging table 2: remove for code size */
	/* aging table 3: remove for code size */
};

#endif /* __GPUFREQ_MT6835_H__ */
