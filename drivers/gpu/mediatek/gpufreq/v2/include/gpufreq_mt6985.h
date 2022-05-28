/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6985_H__
#define __GPUFREQ_MT6985_H__

/**************************************************
 * GPUFREQ Config
 **************************************************/
/* 0 -> power on once then never off and disable DDK power on/off callback */
#define GPUFREQ_POWER_CTRL_ENABLE       (0)
/* 0 -> disable DDK runtime active-idle callback */
#define GPUFREQ_ACTIVE_IDLE_CTRL_ENABLE (0)
/*
 * (DVFS_ENABLE, CUST_INIT)
 * (1, 1) -> DVFS enable and init to CUST_INIT_OPPIDX
 * (1, 0) -> DVFS enable
 * (0, 1) -> DVFS disable but init to CUST_INIT_OPPIDX (do DVFS only onces)
 * (0, 0) -> DVFS disable
 */
#define GPUFREQ_DVFS_ENABLE             (0)
#define GPUFREQ_CUST_INIT_ENABLE        (0)
#define GPUFREQ_CUST_INIT_OPPIDX        (0)
/* MFGSYS Feature */
#define GPUFREQ_HWDCM_ENABLE            (0)
#define GPUFREQ_ACP_ENABLE              (0)
#define GPUFREQ_PDCA_ENABLE             (0)
#define GPUFREQ_GPM1_ENABLE             (0)
#define GPUFREQ_GPM3_ENABLE             (0)
#define GPUFREQ_MERGER_ENABLE           (0)
#define GPUFREQ_DFD_ENABLE              (0)
#define GPUFREQ_AVS_ENABLE              (0)
#define GPUFREQ_ASENSOR_ENABLE          (0)
#define GPUFREQ_SELF_CTRL_MTCMOS        (0)
#define GPUFREQ_SHARED_STATUS_REG       (0)

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
#define MFG_INT0_SEL_MASK               (BIT(16))         /* [16] */
#define MFGSC_INT1_SEL_MASK             (BIT(17))         /* [17] */
#define MFG_REF_SEL_MASK                (GENMASK(17, 16)) /* [17:16] */
#define MFGSC_REF_SEL_MASK              (GENMASK(25, 24)) /* [25:24] */

/**************************************************
 * Frequency Hopping Setting
 **************************************************/
#define GPUFREQ_FHCTL_ENABLE            (0)
#define MFG_PLL_NAME                    "mfg_ao_mfgpll"
#define MFGSC_PLL_NAME                  "mfgsc_ao_mfgscpll"

/**************************************************
 * MTCMOS Setting
 **************************************************/
#define GPUFREQ_CHECK_MFG_PWR_STATUS    (0)
#define MFG_0_1_PWR_MASK                (GENMASK(1, 0))
#define MFG_0_19_PWR_MASK               (GENMASK(19, 0))
#define MFG_1_19_PWR_MASK               (GENMASK(19, 1))
#define MFG_0_1_PWR_STATUS \
	(((readl(SPM_XPU_PWR_STATUS) & BIT(1)) >> 1) | \
	(((readl(MFG_RPC_MFG1_PWR_CON) & BIT(30)) >> 30) << 1))
#define MFG_0_19_PWR_STATUS \
	(((readl(SPM_XPU_PWR_STATUS) & BIT(1)) >> 1) | \
	(((readl(MFG_RPC_MFG1_PWR_CON) & BIT(30)) >> 30) << 1) | \
	(((readl(MFG_RPC_MFG2_PWR_CON) & BIT(30)) >> 30) << 2) | \
	(((readl(MFG_RPC_MFG3_PWR_CON) & BIT(30)) >> 30) << 3) | \
	(((readl(MFG_RPC_MFG4_PWR_CON) & BIT(30)) >> 30) << 4) | \
	(((readl(MFG_RPC_MFG5_PWR_CON) & BIT(30)) >> 30) << 5) | \
	(((readl(MFG_RPC_MFG6_PWR_CON) & BIT(30)) >> 30) << 6) | \
	(((readl(MFG_RPC_MFG7_PWR_CON) & BIT(30)) >> 30) << 7) | \
	(((readl(MFG_RPC_MFG8_PWR_CON) & BIT(30)) >> 30) << 8) | \
	(((readl(MFG_RPC_MFG9_PWR_CON) & BIT(30)) >> 30) << 9) | \
	(((readl(MFG_RPC_MFG10_PWR_CON) & BIT(30)) >> 30) << 10) | \
	(((readl(MFG_RPC_MFG11_PWR_CON) & BIT(30)) >> 30) << 11) | \
	(((readl(MFG_RPC_MFG12_PWR_CON) & BIT(30)) >> 30) << 12) | \
	(((readl(MFG_RPC_MFG13_PWR_CON) & BIT(30)) >> 30) << 13) | \
	(((readl(MFG_RPC_MFG14_PWR_CON) & BIT(30)) >> 30) << 14) | \
	(((readl(MFG_RPC_MFG15_PWR_CON) & BIT(30)) >> 30) << 15) | \
	(((readl(MFG_RPC_MFG16_PWR_CON) & BIT(30)) >> 30) << 16) | \
	(((readl(MFG_RPC_MFG17_PWR_CON) & BIT(30)) >> 30) << 17) | \
	(((readl(MFG_RPC_MFG18_PWR_CON) & BIT(30)) >> 30) << 18) | \
	(((readl(MFG_RPC_MFG19_PWR_CON) & BIT(30)) >> 30) << 19))

/**************************************************
 * Shader Core Setting
 **************************************************/
#define MFG3_SHADER_STACK0              (T0C0 | T0C1)   /* MFG9,  MFG12 */
#define MFG4_SHADER_STACK1              (T1C0 | T1C1)   /* MFG10, MFG13 */
#define MFG5_SHADER_STACK2              (T2C0 | T2C1)   /* MFG11, MFG14 */
#define MFG6_SHADER_STACK4              (T4C0)          /* MFG15        */
#define MFG7_SHADER_STACK5              (T5C0 | T5C1)   /* MFG16, MFG18 */
#define MFG8_SHADER_STACK6              (T6C0 | T6C1)   /* MFG17, MFG19 */

#define GPU_SHADER_PRESENT_1 \
	(MFG6_SHADER_STACK4)
#define GPU_SHADER_PRESENT_2 \
	(MFG3_SHADER_STACK0)
#define GPU_SHADER_PRESENT_3 \
	(MFG3_SHADER_STACK0 | MFG6_SHADER_STACK4)
#define GPU_SHADER_PRESENT_4 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1)
#define GPU_SHADER_PRESENT_5 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG6_SHADER_STACK4)
#define GPU_SHADER_PRESENT_6 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2)
#define GPU_SHADER_PRESENT_7 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG6_SHADER_STACK4)
#define GPU_SHADER_PRESENT_8 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG7_SHADER_STACK5)
#define GPU_SHADER_PRESENT_9 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG6_SHADER_STACK4 | MFG7_SHADER_STACK5)
#define GPU_SHADER_PRESENT_10 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG7_SHADER_STACK5 | MFG8_SHADER_STACK6)
#define GPU_SHADER_PRESENT_11 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG6_SHADER_STACK4 | MFG7_SHADER_STACK5 | MFG8_SHADER_STACK6)

#define SHADER_CORE_NUM                 (11)
struct gpufreq_core_mask_info g_core_mask_table[] = {
	{11, GPU_SHADER_PRESENT_11},
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
 * Dynamic Power Setting
 **************************************************/
#define GPU_DYN_REF_POWER               (826)           /* mW  */
#define GPU_DYN_REF_FREQ                (1000000)       /* KHz */
#define GPU_DYN_REF_VOLT                (90000)         /* mV x 100 */
#define STACK_DYN_REF_POWER             (4329)          /* mW  */
#define STACK_DYN_REF_FREQ              (981000)        /* KHz */
#define STACK_DYN_REF_VOLT              (80000)         /* mV x 100 */

/**************************************************
 * PMIC Setting
 **************************************************/
/*
 * PMIC hardware range:
 * VGPU      0.3 - 1.19375 V (MT6373_VBUCK4)
 * VSTACK    0.3 - 1.19375 V (MT6373_VBUCK2)
 * VSRAM     0.3 - 1.19375 V (MT6373_VSRAM_DIGRF_AIF)
 */
#define VGPU_MAX_VOLT                   (119375)        /* mV x 100 */
#define VGPU_MIN_VOLT                   (30000)         /* mV x 100 */
#define VSTACK_MAX_VOLT                 (119375)        /* mV x 100 */
#define VSTACK_MIN_VOLT                 (30000)         /* mV x 100 */
#define VSRAM_MAX_VOLT                  (119375)        /* mV x 100 */
#define VSRAM_MIN_VOLT                  (30000)         /* mV x 100 */
#define VSRAM_THRESH                    (75000)         /* mV x 100 */
#define PMIC_STEP                       (625)           /* mV x 100 */
#define VOLT_NORMALIZATION(volt) \
	((volt % 625) ? (volt - (volt % 625) + 625) : volt)

/**************************************************
 * Power Throttling Setting
 **************************************************/
#define GPUFREQ_BATT_OC_ENABLE          (0)
#define GPUFREQ_BATT_PERCENT_ENABLE     (0)
#define GPUFREQ_LOW_BATT_ENABLE         (0)
#define GPUFREQ_BATT_OC_FREQ            (484000)
#define GPUFREQ_BATT_PERCENT_IDX        (0)
#define GPUFREQ_LOW_BATT_FREQ           (484000)

/**************************************************
 * Aging Sensor Setting
 **************************************************/
#define GPUFREQ_AGING_KEEP_FGPU         (945000)
#define GPUFREQ_AGING_KEEP_VGPU         (82500)
#define GPUFREQ_AGING_KEEP_FSTACK       (600000)
#define GPUFREQ_AGING_KEEP_VSTACK       (65000)
#define GPUFREQ_AGING_KEEP_VSRAM        (82500)
#define GPUFREQ_AGING_LKG_VSTACK        (70000)
#define GPUFREQ_AGING_GAP_MIN           (-3)
#define GPUFREQ_AGING_GAP_1             (2)
#define GPUFREQ_AGING_GAP_2             (4)
#define GPUFREQ_AGING_GAP_3             (6)
#define GPUFREQ_AGING_MAX_TABLE_IDX     (1)
#define GPUFREQ_AGING_MOST_AGRRESIVE    (0)

/**************************************************
 * DVFS Constraint Setting
 **************************************************/
#define STACK_SEL_OPP                   (20)
#define SRAM_DEL_SEL_OPP                (36)
#define CONSTRAINT_OPP_0                (0)
#define CONSTRAINT_OPP_1                (20)
#define CONSTRAINT_OPP_2                (36)
#define CONSTRAINT_OPP_3                (48)
#define CONSTRAINT_VSRAM_PARK           (-1)
#define NUM_CONSTRAINT_IDX              ARRAY_SIZE(g_constraint_idx)
static const int g_constraint_idx[] = {
	CONSTRAINT_OPP_0,
	CONSTRAINT_OPP_1,
	CONSTRAINT_VSRAM_PARK,
	CONSTRAINT_OPP_2,
	CONSTRAINT_OPP_3,
};

/**************************************************
 * DFD Setting
 **************************************************/
#define MFG_DEBUGMON_CON_00_ENABLE      (0xFFFFFFFF)
#define MFG_DFD_CON_0_ENABLE            (0x0F101100)
#define MFG_DFD_CON_1_ENABLE            (0x00000100)
#define MFG_DFD_CON_2_ENABLE            (0x00000000)
#define MFG_DFD_CON_3_ENABLE            (0x0010001F)
#define MFG_DFD_CON_4_ENABLE            (0x00000000)
#define MFG_DFD_CON_5_ENABLE            (0x00000000)
#define MFG_DFD_CON_6_ENABLE            (0x00000000)
#define MFG_DFD_CON_7_ENABLE            (0x00000000)
#define MFG_DFD_CON_8_ENABLE            (0x00000000)
#define MFG_DFD_CON_9_ENABLE            (0x00000000)
#define MFG_DFD_CON_10_ENABLE           (0x00000000)
#define MFG_DFD_CON_11_ENABLE           (0x00000000)

/**************************************************
 * Leakage Power Setting
 **************************************************/
#define GPU_LKG_POWER                   (30)
#define STACK_LKG_POWER                 (30)

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_segment {
	MT6985_SEGMENT = 0,
};

enum gpufreq_clk_src {
	CLOCK_SUB = 0,
	CLOCK_MAIN,
};

enum gpufreq_sema_op {
	SEMA_RELEASE = 0,
	SEMA_ACQUIRE,
};

enum gpufreq_opp_direct {
	SCALE_DOWN = 0,
	SCALE_UP,
	SCALE_STAY,
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
};

struct gpufreq_mtcmos_info {
	struct device *mfg1_dev;  /* CM7, PDCA      */
#if !GPUFREQ_PDCA_ENABLE
	struct device *mfg2_dev;  /* L2, MMU, Tiler */
	struct device *mfg3_dev;  /* ST0 */
	struct device *mfg4_dev;  /* ST1 */
	struct device *mfg5_dev;  /* ST2 */
	struct device *mfg6_dev;  /* ST4 */
	struct device *mfg7_dev;  /* ST5 */
	struct device *mfg8_dev;  /* ST6 */
	struct device *mfg9_dev;  /* ST0T0 */
	struct device *mfg10_dev; /* ST1T0 */
	struct device *mfg11_dev; /* ST2T0 */
	struct device *mfg12_dev; /* ST0T1 */
	struct device *mfg13_dev; /* ST1T1 */
	struct device *mfg14_dev; /* ST2T1 */
	struct device *mfg15_dev; /* ST4T0 */
	struct device *mfg16_dev; /* ST5T0 */
	struct device *mfg17_dev; /* ST6T0 */
	struct device *mfg18_dev; /* ST5T1 */
	struct device *mfg19_dev; /* ST6T1 */
#endif /* GPUFREQ_PDCA_ENABLE */
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
	unsigned int lkg_rt_info;
	unsigned int lkg_ht_info;
	unsigned int lkg_rt_info_sram;
	unsigned int lkg_ht_info_sram;
};

struct gpufreq_volt_sb {
	unsigned int oppidx;
	unsigned int vgpu;
	unsigned int vstack;
	unsigned int vgpu_up;
	unsigned int vgpu_down;
	unsigned int vstack_up;
	unsigned int vstack_down;
};

/**************************************************
 * GPU Platform OPP Table Definition
 **************************************************/
#define GPU_SIGNED_OPP_0                (0)
#define GPU_SIGNED_OPP_1                (28)
#define GPU_SIGNED_OPP_2                (36)
#define GPU_SIGNED_OPP_3                (48)
#define NUM_GPU_SIGNED_IDX              ARRAY_SIZE(g_gpu_signed_idx)
#define NUM_GPU_SIGNED_OPP              ARRAY_SIZE(g_gpu_default_opp_table)
static const int g_gpu_signed_idx[] = {
	GPU_SIGNED_OPP_0,
	GPU_SIGNED_OPP_1,
	GPU_SIGNED_OPP_2,
	GPU_SIGNED_OPP_3,
};
static struct gpufreq_opp_info g_gpu_default_opp_table[] = {
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /*  0 sign off */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /*  1 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /*  2 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /*  3 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /*  4 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /*  5 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /*  6 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /*  7 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /*  8 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /*  9 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /* 10 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /* 11 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /* 12 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /* 13 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /* 14 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /* 15 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /* 16 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /* 17 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /* 18 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /* 19 */
	GPUOP(1000000, 90000, 90000, POSDIV_POWER_2, 0, 0), /* 20 */
	GPUOP(986250,  88125, 88125, POSDIV_POWER_2, 0, 0), /* 21 */
	GPUOP(972500,  86250, 86250, POSDIV_POWER_2, 0, 0), /* 22 */
	GPUOP(958750,  84375, 84375, POSDIV_POWER_2, 0, 0), /* 23 */
	GPUOP(945000,  82500, 82500, POSDIV_POWER_4, 0, 0), /* 24 */
	GPUOP(931250,  80625, 80625, POSDIV_POWER_4, 0, 0), /* 25 */
	GPUOP(917500,  78750, 78750, POSDIV_POWER_4, 0, 0), /* 26 */
	GPUOP(903750,  76875, 76875, POSDIV_POWER_4, 0, 0), /* 27 */
	GPUOP(890000,  75000, 75000, POSDIV_POWER_4, 0, 0), /* 28 sign off */
	GPUOP(866250,  74375, 75000, POSDIV_POWER_4, 0, 0), /* 29 */
	GPUOP(842500,  73125, 75000, POSDIV_POWER_4, 0, 0), /* 30 */
	GPUOP(818750,  72500, 75000, POSDIV_POWER_4, 0, 0), /* 31 */
	GPUOP(795000,  71250, 75000, POSDIV_POWER_4, 0, 0), /* 32 */
	GPUOP(771250,  70625, 75000, POSDIV_POWER_4, 0, 0), /* 33 */
	GPUOP(747500,  69375, 75000, POSDIV_POWER_4, 0, 0), /* 34 */
	GPUOP(723750,  68750, 75000, POSDIV_POWER_4, 0, 0), /* 35 */
	GPUOP(700000,  67500, 75000, POSDIV_POWER_4, 0, 0), /* 36 sign off */
	GPUOP(673750,  66875, 75000, POSDIV_POWER_4, 0, 0), /* 37 */
	GPUOP(647500,  66250, 75000, POSDIV_POWER_4, 0, 0), /* 38 */
	GPUOP(621250,  65000, 75000, POSDIV_POWER_4, 0, 0), /* 39 */
	GPUOP(595000,  64375, 75000, POSDIV_POWER_4, 0, 0), /* 40 */
	GPUOP(568750,  63750, 75000, POSDIV_POWER_4, 0, 0), /* 41 */
	GPUOP(542500,  62500, 75000, POSDIV_POWER_4, 0, 0), /* 42 */
	GPUOP(516250,  61875, 75000, POSDIV_POWER_4, 0, 0), /* 43 */
	GPUOP(490000,  61250, 75000, POSDIV_POWER_4, 0, 0), /* 44 */
	GPUOP(463750,  60000, 75000, POSDIV_POWER_4, 0, 0), /* 45 */
	GPUOP(437500,  59375, 75000, POSDIV_POWER_4, 0, 0), /* 46 */
	GPUOP(411250,  58750, 75000, POSDIV_POWER_4, 0, 0), /* 47 */
	GPUOP(385000,  57500, 75000, POSDIV_POWER_4, 0, 0), /* 48 sign off */
};

#define STACK_SIGNED_OPP_0              (0)
#define STACK_SIGNED_OPP_1              (20)
#define STACK_SIGNED_OPP_2              (36)
#define STACK_SIGNED_OPP_3              (48)
#define NUM_STACK_SIGNED_IDX            ARRAY_SIZE(g_stack_signed_idx)
#define NUM_STACK_SIGNED_OPP            ARRAY_SIZE(g_stack_default_opp_table)
static const int g_stack_signed_idx[] = {
	STACK_SIGNED_OPP_0,
	STACK_SIGNED_OPP_1,
	STACK_SIGNED_OPP_2,
	STACK_SIGNED_OPP_3,
};
static struct gpufreq_opp_info g_stack_default_opp_table[] = {
	GPUOP(981000, 80000, 80000, POSDIV_POWER_2, 0, 0), /*  0 sign off */
	GPUOP(965000, 79375, 79375, POSDIV_POWER_2, 0, 0), /*  1 */
	GPUOP(949000, 78750, 78750, POSDIV_POWER_4, 0, 0), /*  2 */
	GPUOP(934000, 78125, 78125, POSDIV_POWER_4, 0, 0), /*  3 */
	GPUOP(918000, 77500, 77500, POSDIV_POWER_4, 0, 0), /*  4 */
	GPUOP(903000, 76875, 76875, POSDIV_POWER_4, 0, 0), /*  5 */
	GPUOP(887000, 76250, 76250, POSDIV_POWER_4, 0, 0), /*  6 */
	GPUOP(872000, 75625, 75625, POSDIV_POWER_4, 0, 0), /*  7 */
	GPUOP(856000, 75000, 75000, POSDIV_POWER_4, 0, 0), /*  8 */
	GPUOP(841000, 74375, 75000, POSDIV_POWER_4, 0, 0), /*  9 */
	GPUOP(825000, 73750, 75000, POSDIV_POWER_4, 0, 0), /* 10 */
	GPUOP(809000, 73125, 75000, POSDIV_POWER_4, 0, 0), /* 11 */
	GPUOP(794000, 72500, 75000, POSDIV_POWER_4, 0, 0), /* 12 */
	GPUOP(778000, 71875, 75000, POSDIV_POWER_4, 0, 0), /* 13 */
	GPUOP(763000, 71250, 75000, POSDIV_POWER_4, 0, 0), /* 14 */
	GPUOP(747000, 70625, 75000, POSDIV_POWER_4, 0, 0), /* 15 */
	GPUOP(732000, 70000, 75000, POSDIV_POWER_4, 0, 0), /* 16 */
	GPUOP(716000, 69375, 75000, POSDIV_POWER_4, 0, 0), /* 17 */
	GPUOP(701000, 68750, 75000, POSDIV_POWER_4, 0, 0), /* 18 */
	GPUOP(685000, 68125, 75000, POSDIV_POWER_4, 0, 0), /* 19 */
	GPUOP(670000, 67500, 75000, POSDIV_POWER_4, 0, 0), /* 20 sign off */
	GPUOP(652000, 66875, 75000, POSDIV_POWER_4, 0, 0), /* 21 */
	GPUOP(635000, 66250, 75000, POSDIV_POWER_4, 0, 0), /* 22 */
	GPUOP(617000, 65625, 75000, POSDIV_POWER_4, 0, 0), /* 23 */
	GPUOP(600000, 65000, 75000, POSDIV_POWER_4, 0, 0), /* 24 */
	GPUOP(582000, 64375, 75000, POSDIV_POWER_4, 0, 0), /* 25 */
	GPUOP(565000, 63750, 75000, POSDIV_POWER_4, 0, 0), /* 26 */
	GPUOP(547000, 63125, 75000, POSDIV_POWER_4, 0, 0), /* 27 */
	GPUOP(530000, 62500, 75000, POSDIV_POWER_4, 0, 0), /* 28 */
	GPUOP(512000, 61875, 75000, POSDIV_POWER_4, 0, 0), /* 29 */
	GPUOP(495000, 61250, 75000, POSDIV_POWER_4, 0, 0), /* 30 */
	GPUOP(477000, 60625, 75000, POSDIV_POWER_4, 0, 0), /* 31 */
	GPUOP(460000, 60000, 75000, POSDIV_POWER_4, 0, 0), /* 32 */
	GPUOP(442000, 59375, 75000, POSDIV_POWER_4, 0, 0), /* 33 */
	GPUOP(425000, 58750, 75000, POSDIV_POWER_4, 0, 0), /* 34 */
	GPUOP(407000, 58125, 75000, POSDIV_POWER_4, 0, 0), /* 35 */
	GPUOP(390000, 57500, 75000, POSDIV_POWER_4, 0, 0), /* 36 sign off */
	GPUOP(376000, 56875, 75000, POSDIV_POWER_4, 0, 0), /* 37 */
	GPUOP(362000, 56250, 75000, POSDIV_POWER_8, 0, 0), /* 38 */
	GPUOP(348000, 55625, 75000, POSDIV_POWER_8, 0, 0), /* 39 */
	GPUOP(334000, 55000, 75000, POSDIV_POWER_8, 0, 0), /* 40 */
	GPUOP(320000, 54375, 75000, POSDIV_POWER_8, 0, 0), /* 41 */
	GPUOP(307000, 53750, 75000, POSDIV_POWER_8, 0, 0), /* 42 */
	GPUOP(293000, 53125, 75000, POSDIV_POWER_8, 0, 0), /* 43 */
	GPUOP(279000, 52500, 75000, POSDIV_POWER_8, 0, 0), /* 44 */
	GPUOP(265000, 51875, 75000, POSDIV_POWER_8, 0, 0), /* 45 */
	GPUOP(251000, 51250, 75000, POSDIV_POWER_8, 0, 0), /* 46 */
	GPUOP(237000, 50625, 75000, POSDIV_POWER_8, 0, 0), /* 47 */
	GPUOP(224000, 50000, 75000, POSDIV_POWER_8, 0, 0), /* 48 sign off */
};

/**************************************************
 * OPP Adjustment
 **************************************************/
static struct gpufreq_adj_info g_gpu_avs_table[NUM_GPU_SIGNED_IDX] = {
#if GPUFREQ_AVS_ENABLE
	ADJOP(GPU_SIGNED_OPP_0, 0, 0, 0),
	ADJOP(GPU_SIGNED_OPP_1, 0, 0, 0),
	ADJOP(GPU_SIGNED_OPP_2, 0, 0, 0),
	ADJOP(GPU_SIGNED_OPP_3, 0, 0, 0),
#endif /* GPUFREQ_AVS_ENABLE */
};

static struct gpufreq_adj_info g_stack_avs_table[NUM_STACK_SIGNED_IDX] = {
#if GPUFREQ_AVS_ENABLE
	ADJOP(STACK_SIGNED_OPP_0, 0, 0, 0),
	ADJOP(STACK_SIGNED_OPP_1, 0, 0, 0),
	ADJOP(STACK_SIGNED_OPP_2, 0, 0, 0),
	ADJOP(STACK_SIGNED_OPP_3, 0, 0, 0),
#endif /* GPUFREQ_AVS_ENABLE */
};

static struct gpufreq_adj_info g_gpu_aging_table[][NUM_GPU_SIGNED_IDX] = {
	{ /* aging table 0 */
		ADJOP(GPU_SIGNED_OPP_0, 0, 625, 0),
		ADJOP(GPU_SIGNED_OPP_1, 0, 625, 0),
		ADJOP(GPU_SIGNED_OPP_2, 0, 625, 0),
		ADJOP(GPU_SIGNED_OPP_3, 0, 0, 0),
	},
#if GPUFREQ_ASENSOR_ENABLE
	{ /* aging table 1 */
		ADJOP(GPU_SIGNED_OPP_0, 0, 0, 0),
		ADJOP(GPU_SIGNED_OPP_1, 0, 0, 0),
		ADJOP(GPU_SIGNED_OPP_2, 0, 0, 0),
		ADJOP(GPU_SIGNED_OPP_3, 0, 0, 0),
	},
	/* aging table 2: remove for code size */
	/* aging table 3: remove for code size */
#endif /* GPUFREQ_ASENSOR_ENABLE */
};

static struct gpufreq_adj_info g_stack_aging_table[][NUM_STACK_SIGNED_IDX] = {
	{ /* aging table 0 */
		ADJOP(STACK_SIGNED_OPP_0, 0, 625, 0),
		ADJOP(STACK_SIGNED_OPP_1, 0, 625, 0),
		ADJOP(STACK_SIGNED_OPP_2, 0, 625, 0),
		ADJOP(STACK_SIGNED_OPP_3, 0, 0, 0),
	},
#if GPUFREQ_ASENSOR_ENABLE
	{ /* aging table 1 */
		ADJOP(STACK_SIGNED_OPP_0, 0, 0, 0),
		ADJOP(STACK_SIGNED_OPP_1, 0, 0, 0),
		ADJOP(STACK_SIGNED_OPP_2, 0, 0, 0),
		ADJOP(STACK_SIGNED_OPP_3, 0, 0, 0),
	},
	/* aging table 2: remove for code size */
	/* aging table 3: remove for code size */
#endif /* GPUFREQ_ASENSOR_ENABLE */
};

#endif /* __GPUFREQ_MT6985_H__ */
