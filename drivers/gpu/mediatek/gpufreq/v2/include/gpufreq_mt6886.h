/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6886_H__
#define __GPUFREQ_MT6886_H__

/**************************************************
 * GPUFREQ Local Config
 **************************************************/
/* 0 -> power on once then never off and disable DDK power on/off callback */
#define GPUFREQ_POWER_CTRL_ENABLE       (1)
/* 0 -> disable DDK runtime active-idle callback */
#define GPUFREQ_ACTIVE_IDLE_CTRL_ENABLE (0)
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
#define GPUFREQ_ACP_ENABLE              (1)
#define GPUFREQ_PDCA_ENABLE             (1)
#define GPUFREQ_GPM1_ENABLE             (1)
#define GPUFREQ_MERGER_ENABLE           (1)
#define GPUFREQ_DFD_ENABLE              (1)
#define GPUFREQ_AVS_ENABLE              (0)
#define GPUFREQ_ASENSOR_ENABLE          (0)
#define GPUFREQ_SELF_CTRL_MTCMOS        (1)
#define GPUFREQ_SHARED_STATUS_REG       (0)

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
#define MFGPLL_FIN                      (26)            /* MHz */
#define CKMUX_SEL_CORE                  (4)
#define CKMUX_SEL_PARK                  (5)
#define CKMUX_SEL_STACK                 (7)
#define CKMUX_SEL_STACK_PARK            (8)
#define GPUPLL_ID                       (0x0)
#define STACKPLL_ID                     (0x3)
#define FREQ_ROUNDUP_TO_10(freq)        ((freq % 10) ? (freq - (freq % 10) + 10) : freq)

/**************************************************
 * Frequency Hopping Setting
 **************************************************/
#define GPUFREQ_FHCTL_ENABLE            (0)
#define MFG_PLL_NAME                    "mfgpll"
#define MFGSC_PLL_NAME                  "mfgscpll"

/**************************************************
 * MTCMOS Setting
 **************************************************/
#define GPUFREQ_CHECK_MFG_PWR_STATUS    (0)
#define MFG_0_1_PWR_MASK                (GENMASK(1, 0))
#define MFG_0_12_PWR_MASK               (GENMASK(6, 0))
#define MFG_1_12_PWR_MASK               (GENMASK(6, 1))
#define MFG_0_1_PWR_STATUS \
	(((readl(SPM_MFG0_PWR_CON) & BIT(30)) >> 30) | \
	(((readl(MFG_RPC_MFG1_PWR_CON) & BIT(30)) >> 30) << 1))
#define MFG_0_12_PWR_STATUS \
	(((readl(SPM_MFG0_PWR_CON) & BIT(30)) >> 30) | \
	(((readl(MFG_RPC_MFG1_PWR_CON) & BIT(30)) >> 30) << 1) | \
	(((readl(MFG_RPC_MFG2_PWR_CON) & BIT(30)) >> 30) << 2) | \
	(((readl(MFG_RPC_MFG9_PWR_CON) & BIT(30)) >> 30) << 3) | \
	(((readl(MFG_RPC_MFG10_PWR_CON) & BIT(30)) >> 30) << 4) | \
	(((readl(MFG_RPC_MFG11_PWR_CON) & BIT(30)) >> 30) << 5) | \
	(((readl(MFG_RPC_MFG12_PWR_CON) & BIT(30)) >> 30) << 6))

/**************************************************
 * Shader Core Setting
 **************************************************/
#define MFG9_SHADER_STACK0               (T0C0)          /* MFG9 */
#define MFG10_SHADER_STACK2              (T2C0)          /* MFG10 */
#define MFG11_SHADER_STACK4              (T4C0)          /* MFG11 */
#define MFG12_SHADER_STACK6              (T6C0)          /* MFG12 */

#define GPU_SHADER_PRESENT_1 \
	(MFG9_SHADER_STACK0)
#define GPU_SHADER_PRESENT_2 \
	(MFG9_SHADER_STACK0 | MFG10_SHADER_STACK2)
#define GPU_SHADER_PRESENT_3 \
	(MFG9_SHADER_STACK0 | MFG10_SHADER_STACK2 | MFG11_SHADER_STACK4)
#define GPU_SHADER_PRESENT_4 \
	(MFG9_SHADER_STACK0 | MFG10_SHADER_STACK2 | MFG11_SHADER_STACK4 | MFG12_SHADER_STACK6)

#define SHADER_CORE_NUM                 (4)
struct gpufreq_core_mask_info g_core_mask_table[] = {
	{4, GPU_SHADER_PRESENT_4},
	{3, GPU_SHADER_PRESENT_3},
	{2, GPU_SHADER_PRESENT_2},
	{1, GPU_SHADER_PRESENT_1},
};

/**************************************************
 * Reference Power Setting MT6789 TBD
 **************************************************/
#define GPU_ACT_REF_POWER               (977)           /* mW  */
#define GPU_ACT_REF_FREQ                (1100000)       /* KHz */
#define GPU_ACT_REF_VOLT                (85000)         /* mV x 100 */
#define GPU_LEAKAGE_POWER               (71)

/**************************************************
 * PMIC Setting MT6886
 **************************************************/
/*
 * PMIC hardware range:
 * vgpu      0.4  ~ 1.19375 V (MT6368)
 * vsram     0.4  ~ 1.19375 V (MT6363)
 */
#define VGPU_MAX_VOLT                   (119375)        /* mV x 100 */
#define VGPU_MIN_VOLT                   (40000)         /* mV x 100 */
#define VSRAM_MAX_VOLT                  (119375)        /* mV x 100 */
#define VSRAM_MIN_VOLT                  (40000)         /* mV x 100 */
#define VSRAM_THRESH                    (75000)
#define PMIC_STEP                       (625)           /* mV x 100 */
/*
 * (0)mv <= (VSRAM - VGPU) <= (200)mV
 */
#define VSRAM_VLOGIC_DIFF              (20000)         /* mV x 100 */
#define VOLT_NORMALIZATION(volt)       ((volt % 625) ? (volt - (volt % 625) + 625) : volt)

/**************************************************
 * Power Throttling Setting
 **************************************************/
#define GPUFREQ_BATT_OC_ENABLE          (0)
#define GPUFREQ_BATT_PERCENT_ENABLE     (0)
#define GPUFREQ_LOW_BATT_ENABLE         (0)
#define GPUFREQ_BATT_OC_FREQ            (485000)
#define GPUFREQ_BATT_PERCENT_IDX        (0)
#define GPUFREQ_LOW_BATT_FREQ           (485000)

/**************************************************
 * Aging Sensor Setting
 **************************************************/
#define GPUFREQ_AGING_KEEP_FGPU         (670000)
#define GPUFREQ_AGING_KEEP_VGPU         (65000)
#define GPUFREQ_AGING_KEEP_VSRAM        (75000)
#define GPUFREQ_AGING_LKG_VGPU          (70000)
#define GPUFREQ_AGING_GAP_MIN           (-3)
#define GPUFREQ_AGING_GAP_1             (2)
#define GPUFREQ_AGING_GAP_2             (4)
#define GPUFREQ_AGING_MAX_TABLE_IDX     (1)
#define GPUFREQ_AGING_MOST_AGRRESIVE    (0)

/**************************************************
 * DFD Setting
 **************************************************/
#define MFG_DEBUGMON_CON_00_ENABLE   (0xFFFFFFFF)
#define MFG_DFD_CON_0_ENABLE         (0x0F101100)
#define MFG_DFD_CON_1_ENABLE         (0x00008C20)
#define MFG_DFD_CON_2_ENABLE         (0x0000DB9E)
#define MFG_DFD_CON_3_ENABLE         (0x0021547C)
#define MFG_DFD_CON_4_ENABLE         (0x00000000)
#define MFG_DFD_CON_5_ENABLE         (0x00000000)
#define MFG_DFD_CON_6_ENABLE         (0x00000000)
#define MFG_DFD_CON_7_ENABLE         (0x00000000)
#define MFG_DFD_CON_8_ENABLE         (0x00000000)
#define MFG_DFD_CON_9_ENABLE         (0x00000000)
#define MFG_DFD_CON_10_ENABLE        (0x00000000)
#define MFG_DFD_CON_11_ENABLE        (0x00000000)

/**************************************************
 * Enumeration MT6886
 **************************************************/
enum gpufreq_segment {
	MT6886_SEGMENT = 1,
};

enum gpufreq_clk_src {
	CLOCK_SUB = 0,
	CLOCK_MAIN,
};

enum gpufreq_opp_direct {
	SCALE_DOWN = 0,
	SCALE_UP,
	SCALE_STAY,
};

/**************************************************
 * Structure MT6886
 **************************************************/
struct gpufreq_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram;
};

struct gpufreq_clk_info {
	struct clk *clk_main_parent;
	struct clk *clk_sc_main_parent;
};

struct gpufreq_mtcmos_info {
	struct device *mfg1_dev;  /* CM7, PDCA      */
#if !GPUFREQ_PDCA_ENABLE
	struct device *mfg2_dev;  /* L2, MMU, Tiler */
	struct device *mfg9_dev;  /* ST0 */
	struct device *mfg10_dev; /* ST2 */
	struct device *mfg11_dev; /* ST4 */
	struct device *mfg12_dev; /* ST6 */
#endif
};

struct gpufreq_status {
	struct gpufreq_opp_info *signed_table;
	struct gpufreq_opp_info *working_table;
	int buck_count;
	int mtcmos_count;
	int cg_count;
	int power_count;
	int active_count;
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
#define GPU_BINNING_OPP_0               (0)
#define GPU_BINNING_OPP_1               (28)
#define GPU_BINNING_OPP_2               (44)
#define NUM_GPU_BINNING_IDX             ARRAY_SIZE(g_gpu_binning_idx)
#define NUM_GPU_SIGNED_OPP              ARRAY_SIZE(g_gpu_default_opp_table)
static const int g_gpu_binning_idx[] = {
	GPU_BINNING_OPP_0,
	GPU_BINNING_OPP_1,
	GPU_BINNING_OPP_2,
};
struct gpufreq_opp_info g_gpu_default_opp_table[] = {
	GPUOP(1130000, 85000, 85000, POSDIV_POWER_2, 0, 0), /*  0 sign off */
	GPUOP(1115000, 85000, 85000, POSDIV_POWER_2, 0, 0), /*  1 */
	GPUOP(1100000, 84375, 84375, POSDIV_POWER_2, 0, 0), /*  2 */
	GPUOP(1085000, 83750, 83750, POSDIV_POWER_2, 0, 0), /*  3 */
	GPUOP(1070000, 83125, 83125, POSDIV_POWER_2, 0, 0), /*  4 */
	GPUOP(1055000, 82500, 82500, POSDIV_POWER_2, 0, 0), /*  5 */
	GPUOP(1040000, 81875, 81875, POSDIV_POWER_2, 0, 0), /*  6 */
	GPUOP(1025000, 81250, 81250, POSDIV_POWER_2, 0, 0), /*  7 */
	GPUOP(1010000, 80625, 80625, POSDIV_POWER_2, 0, 0), /*  8 */
	GPUOP(995000,  80000, 80000, POSDIV_POWER_2, 0, 0), /*  9 */
	GPUOP(980000,  79375, 79375, POSDIV_POWER_2, 0, 0), /* 10 */
	GPUOP(965000,  78750, 78750, POSDIV_POWER_2, 0, 0), /* 11 */
	GPUOP(950000,  78125, 78125, POSDIV_POWER_4, 0, 0), /* 12 */
	GPUOP(935000,  77500, 77500, POSDIV_POWER_4, 0, 0), /* 13 */
	GPUOP(920000,  77500, 77500, POSDIV_POWER_4, 0, 0), /* 14 */
	GPUOP(905000,  76875, 76875, POSDIV_POWER_4, 0, 0), /* 15 */
	GPUOP(890000,  76250, 76250, POSDIV_POWER_4, 0, 0), /* 16 */
	GPUOP(872000,  75625, 75625, POSDIV_POWER_4, 0, 0), /* 17 */
	GPUOP(854000,  75000, 75000, POSDIV_POWER_4, 0, 0), /* 18 sign off */
	GPUOP(836000,  73750, 75000, POSDIV_POWER_4, 0, 0), /* 19 */
	GPUOP(818000,  73125, 75000, POSDIV_POWER_4, 0, 0), /* 20 */
	GPUOP(800000,  72500, 75000, POSDIV_POWER_4, 0, 0), /* 21 */
	GPUOP(782000,  71875, 75000, POSDIV_POWER_4, 0, 0), /* 22 */
	GPUOP(764000,  71250, 75000, POSDIV_POWER_4, 0, 0), /* 23 */
	GPUOP(746000,  70625, 75000, POSDIV_POWER_4, 0, 0), /* 24 */
	GPUOP(728000,  70000, 75000, POSDIV_POWER_4, 0, 0), /* 25 */
	GPUOP(710000,  69375, 75000, POSDIV_POWER_4, 0, 0), /* 26 */
	GPUOP(692000,  68750, 75000, POSDIV_POWER_4, 0, 0), /* 27 */
	GPUOP(675000,  67500, 75000, POSDIV_POWER_4, 0, 0), /* 28 sign off*/
	GPUOP(655000,  66875, 75000, POSDIV_POWER_4, 0, 0), /* 29 */
	GPUOP(635000,  66250, 75000, POSDIV_POWER_4, 0, 0), /* 30 */
	GPUOP(615000,  65625, 75000, POSDIV_POWER_4, 0, 0), /* 31 */
	GPUOP(596000,  65000, 75000, POSDIV_POWER_4, 0, 0), /* 32 */
	GPUOP(576000,  64375, 75000, POSDIV_POWER_4, 0, 0), /* 33 */
	GPUOP(556000,  63750, 75000, POSDIV_POWER_4, 0, 0), /* 34 */
	GPUOP(537000,  63125, 75000, POSDIV_POWER_4, 0, 0), /* 35 */
	GPUOP(517000,  62500, 75000, POSDIV_POWER_4, 0, 0), /* 36 */
	GPUOP(497000,  61875, 75000, POSDIV_POWER_4, 0, 0), /* 37 */
	GPUOP(478000,  61250, 75000, POSDIV_POWER_4, 0, 0), /* 38 */
	GPUOP(458000,  60625, 75000, POSDIV_POWER_8, 0, 0), /* 39 */
	GPUOP(438000,  60000, 75000, POSDIV_POWER_8, 0, 0), /* 40 */
	GPUOP(419000,  59375, 75000, POSDIV_POWER_8, 0, 0), /* 41 */
	GPUOP(399000,  58750, 75000, POSDIV_POWER_8, 0, 0), /* 42 */
	GPUOP(379000,  58125, 75000, POSDIV_POWER_8, 0, 0), /* 43 */
	GPUOP(360000,  57500, 75000, POSDIV_POWER_8, 0, 0), /* 44 sign off*/
};

/**************************************************
 * OPP Adjustment
 **************************************************/
static struct gpufreq_adj_info g_avs_table[NUM_GPU_BINNING_IDX] = {
	ADJOP(GPU_BINNING_OPP_0, 0, 0, 0),
	ADJOP(GPU_BINNING_OPP_1, 0, 0, 0),
	ADJOP(GPU_BINNING_OPP_2, 0, 0, 0),
};

static struct gpufreq_adj_info g_aging_table[][NUM_GPU_BINNING_IDX] = {
	{ /* aging table 0 */
		ADJOP(GPU_BINNING_OPP_0, 0, 625, 0),
		ADJOP(GPU_BINNING_OPP_1, 0, 625, 0),
		ADJOP(GPU_BINNING_OPP_2, 0, 625, 0),
	},
	{ /* aging table 1 */
		ADJOP(GPU_BINNING_OPP_0, 0, 0, 0),
		ADJOP(GPU_BINNING_OPP_1, 0, 0, 0),
		ADJOP(GPU_BINNING_OPP_2, 0, 0, 0),
	},
	/* aging table 2: remove for code size */
	/* aging table 3: remove for code size */
};

#endif /* __GPUFREQ_MT6886_H__ */
