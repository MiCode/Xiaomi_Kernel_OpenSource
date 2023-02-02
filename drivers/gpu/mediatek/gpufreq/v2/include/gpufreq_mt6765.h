/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#ifndef __GPUFREQ_MT6765_H__
#define __GPUFREQ_MT6765_H__
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
#define MT_GPUFREQ_STATIC_PWR_READY2USE
//For IMG Mfgsys
#define BUCK_ON 1
#define BUCK_OFF 0
/**************************************************
 * Clock Setting
 **************************************************/
 //Legacy
#define POSDIV_2_MAX_FREQ               (1900000)       /* KHz */
#define POSDIV_2_MIN_FREQ               (750000)        /* KHz */
#define POSDIV_4_MAX_FREQ               (950000)        /* KHz */
#define POSDIV_4_MIN_FREQ               (375000)        /* KHz */
#define POSDIV_8_MAX_FREQ               (475000)        /* KHz */
#define POSDIV_8_MIN_FREQ               (187500)        /* KHz */
#define POSDIV_16_MAX_FREQ              (237500)        /* KHz */
#define POSDIV_16_MIN_FREQ              (93750)        /* KHz */
#define POSDIV_SHIFT                    (24)            /* bit */
#define DDS_SHIFT                       (14)            /* bit */
#define TO_MHZ_HEAD                     (100)
#define TO_MHZ_TAIL                     (10)
#define ROUNDING_VALUE                  (5)
#define MFGPLL_FIN                      (26)            /* MHz */
//#define MFGPLL_FH_PLL                   (6) // used hardcoded 3 in code
//#define MFGPLL_CON0				        (g_apmixed_base + 0x24C)
#define MFGPLL_CON1				        (g_apmixed_base + 0x250)

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

/**************************************************
 * Reference Power Setting MT6765
 **************************************************/
// TODO:GKI
#define GPU_ACT_REF_POWER			(1285)		/* mW  */
#define GPU_ACT_REF_FREQ			(900000)	/* KHz */
#define GPU_ACT_REF_VOLT			(90000)		/* mV x 100 */
#define GPU_DVFS_PTPOD_DISABLE_VOLT	(65000)		/* mV x 100 */
#define GPU_LEAKAGE_POWER               (71)  // p_leakage = 71 : Legacy
/**************************************************
 * PMIC Setting MT6765
 **************************************************/
#define VGPU_MAX_VOLT		(80000)         /* mV x 100 */
///ASK PMIC OWNERS FOR MIN VOLTAGE  -->>>>
#define VGPU_MIN_VOLT       (65000)         /* mV x 100 */
#define VSRAM_MAX_VOLT      (87500)        /* mV x 100 */
#define VSRAM_MIN_VOLT      (87500)         /* mV x 100 */
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
 * Enumeration MT6765
 **************************************************/
enum gpufreq_segment {
	MT6762M_SEGMENT = 1,
	MT6762_SEGMENT,
	MT6765_SEGMENT,
	MT6765T_SEGMENT,
	MT6762D_SEGMENT,
};
enum gpufreq_clk_src {
	CLOCK_MAIN = 0,
	CLOCK_SUB,
};
/**************************************************
 * Structure MT6765
 **************************************************/
struct g_clk_info {
	struct clk *clk_mux;/* main clock for mfg setting*/
	struct clk *clk_main_parent;/* sub clock for mfg trans mux setting*/
	struct clk *clk_sub_parent; /* sub clock for mfg trans parent setting*/
};
struct g_pmic_info {
	struct regulator *reg_vcore;
	struct regulator *mtk_pm_vgpu;
};
struct gpufreq_mtcmos_info {
	struct device *pd_mfg;
	struct device *pd_mfg_async;
	struct device *pd_mfg_core0;
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
struct mt_gpufreq_power_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_power;
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
//TODO: GKI
/* OPP table based on segment : followed from legacy
 * Freq , Volt , Vsram required --> Fetched from opp table
 * Posdiv --> Not fetched from table for use. Hardcoded values used in code
 * Vagaing --> Disabled on default load[No entry for "enable-aging" in load dts].
 * Required on aging load
 * power --> 0 in mt6855/mt6768 . Keep same
 */
struct gpufreq_opp_info g_default_gpu_segment1[] = {
	GPUOP(376000, 65000, 87500, 0, 0, 0),
};
struct gpufreq_opp_info g_default_gpu_segment2[] = {
	GPUOP(650000, 80000, 87500, 0, 0, 0),
	GPUOP(500000, 70000, 87500, 0, 0, 0),
	GPUOP(400000, 65000, 87500, 0, 0, 0),
};
struct gpufreq_opp_info g_default_gpu_segment3[] = {
	GPUOP(680000, 80000, 87500, 0, 0, 0),
	GPUOP(500000, 70000, 87500, 0, 0, 0),
	GPUOP(400000, 65000, 87500, 0, 0, 0),
};
struct gpufreq_opp_info g_default_gpu_segment4[] = {
	GPUOP(730000, 80000, 87500, 0, 0, 0),
	GPUOP(500000, 70000, 87500, 0, 0, 0),
	GPUOP(400000, 65000, 87500, 0, 0, 0),
};
struct gpufreq_opp_info g_default_gpu_segment5[] = {
	GPUOP(600000, 80000, 87500, 0, 0, 0),
	GPUOP(500000, 70000, 87500, 0, 0, 0),
	GPUOP(400000, 65000, 87500, 0, 0, 0),
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
#endif /* __GPUFREQ_MT6765_H__ */
