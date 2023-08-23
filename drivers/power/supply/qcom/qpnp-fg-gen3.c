/* Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"FG: %s: " fmt, __func__

#include <linux/ktime.h>
#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/alarmtimer.h>
#include <linux/of_platform.h>
#include <linux/of_batterydata.h>
#include <linux/platform_device.h>
#include <linux/iio/consumer.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/qpnp/qpnp-misc.h>
#include "fg-core.h"
#include "fg-reg.h"

#define FG_GEN3_DEV_NAME	"qcom,fg-gen3"

#define PERPH_SUBTYPE_REG		0x05
#define FG_BATT_SOC_PMI8998		0x10
#define FG_BATT_INFO_PMI8998		0x11
#define FG_MEM_INFO_PMI8998		0x0D

/* SRAM address and offset in ascending order */
#define ESR_PULSE_THRESH_WORD		2
#define ESR_PULSE_THRESH_OFFSET		3
#define SLOPE_LIMIT_WORD		3
#define SLOPE_LIMIT_OFFSET		0
#define CUTOFF_CURR_WORD		4
#define CUTOFF_CURR_OFFSET		0
#define CUTOFF_VOLT_WORD		5
#define CUTOFF_VOLT_OFFSET		0
#define SYS_TERM_CURR_WORD		6
#define SYS_TERM_CURR_OFFSET		0
#define VBATT_FULL_WORD			7
#define VBATT_FULL_OFFSET		0
#define ESR_FILTER_WORD			8
#define ESR_UPD_TIGHT_OFFSET		0
#define ESR_UPD_BROAD_OFFSET		1
#define ESR_UPD_TIGHT_LOW_TEMP_OFFSET	2
#define ESR_UPD_BROAD_LOW_TEMP_OFFSET	3
#define KI_COEFF_MED_DISCHG_WORD	9
#define TIMEBASE_OFFSET			1
#define KI_COEFF_MED_DISCHG_OFFSET	3
#define KI_COEFF_HI_DISCHG_WORD		10
#define KI_COEFF_HI_DISCHG_OFFSET	0
#define KI_COEFF_LOW_DISCHG_WORD	10
#define KI_COEFF_LOW_DISCHG_OFFSET	2
#define KI_COEFF_FULL_SOC_WORD		12
#define KI_COEFF_FULL_SOC_OFFSET	2
#define DELTA_MSOC_THR_WORD		12
#define DELTA_MSOC_THR_OFFSET		3
#define DELTA_BSOC_THR_WORD		13
#define DELTA_BSOC_THR_OFFSET		2
#define RECHARGE_SOC_THR_WORD		14
#define RECHARGE_SOC_THR_OFFSET		0
#define CHG_TERM_CURR_WORD		14
#define CHG_TERM_CURR_OFFSET		1
#define SYNC_SLEEP_THR_WORD		14
#define SYNC_SLEEP_THR_OFFSET		3
#define EMPTY_VOLT_WORD			15
#define EMPTY_VOLT_OFFSET		0
#define VBATT_LOW_WORD			15
#define VBATT_LOW_OFFSET		1
#define ESR_TIMER_DISCHG_MAX_WORD	17
#define ESR_TIMER_DISCHG_MAX_OFFSET	0
#define ESR_TIMER_DISCHG_INIT_WORD	17
#define ESR_TIMER_DISCHG_INIT_OFFSET	2
#define ESR_TIMER_CHG_MAX_WORD		18
#define ESR_TIMER_CHG_MAX_OFFSET	0
#define ESR_TIMER_CHG_INIT_WORD		18
#define ESR_TIMER_CHG_INIT_OFFSET	2
#define ESR_EXTRACTION_ENABLE_WORD	19
#define ESR_EXTRACTION_ENABLE_OFFSET	0
#define PROFILE_LOAD_WORD		24
#define PROFILE_LOAD_OFFSET		0
#define ESR_RSLOW_DISCHG_WORD		34
#define ESR_RSLOW_DISCHG_OFFSET		0
#define ESR_RSLOW_CHG_WORD		51
#define ESR_RSLOW_CHG_OFFSET		0
#define NOM_CAP_WORD			58
#define NOM_CAP_OFFSET			0
#define ACT_BATT_CAP_BKUP_WORD		74
#define ACT_BATT_CAP_BKUP_OFFSET	0
#define CYCLE_COUNT_WORD		75
#define CYCLE_COUNT_OFFSET		0
#define PROFILE_INTEGRITY_WORD		79
#define SW_CONFIG_OFFSET		0
#define PROFILE_INTEGRITY_OFFSET	3
#define BATT_SOC_WORD			91
#define BATT_SOC_OFFSET			0
#define FULL_SOC_WORD			93
#define FULL_SOC_OFFSET			2
#define MONOTONIC_SOC_WORD		94
#define MONOTONIC_SOC_OFFSET		2
#define CC_SOC_WORD			95
#define CC_SOC_OFFSET			0
#define CC_SOC_SW_WORD			96
#define CC_SOC_SW_OFFSET		0
#define VOLTAGE_PRED_WORD		97
#define VOLTAGE_PRED_OFFSET		0
#define OCV_WORD			97
#define OCV_OFFSET			2
#define ESR_WORD			99
#define ESR_OFFSET			0
#define RSLOW_WORD			101
#define RSLOW_OFFSET			0
#define ACT_BATT_CAP_WORD		117
#define ACT_BATT_CAP_OFFSET		0
#define LAST_BATT_SOC_WORD		119
#define LAST_BATT_SOC_OFFSET		0
#define LAST_MONOTONIC_SOC_WORD		119
#define LAST_MONOTONIC_SOC_OFFSET	2
#define ALG_FLAGS_WORD			120
#define ALG_FLAGS_OFFSET		1

/* v2 SRAM address and offset in ascending order */
#define KI_COEFF_LOW_DISCHG_v2_WORD	9
#define KI_COEFF_LOW_DISCHG_v2_OFFSET	3
#define KI_COEFF_MED_DISCHG_v2_WORD	10
#define KI_COEFF_MED_DISCHG_v2_OFFSET	0
#define KI_COEFF_HI_DISCHG_v2_WORD	10
#define KI_COEFF_HI_DISCHG_v2_OFFSET	1
#define DELTA_BSOC_THR_v2_WORD		12
#define DELTA_BSOC_THR_v2_OFFSET	3
#define DELTA_MSOC_THR_v2_WORD		13
#define DELTA_MSOC_THR_v2_OFFSET	0
#define RECHARGE_SOC_THR_v2_WORD	14
#define RECHARGE_SOC_THR_v2_OFFSET	1
#define SYNC_SLEEP_THR_v2_WORD		14
#define SYNC_SLEEP_THR_v2_OFFSET	2
#define CHG_TERM_CURR_v2_WORD		15
#define CHG_TERM_BASE_CURR_v2_OFFSET	0
#define CHG_TERM_CURR_v2_OFFSET		1
#define EMPTY_VOLT_v2_WORD		15
#define EMPTY_VOLT_v2_OFFSET		3
#define VBATT_LOW_v2_WORD		16
#define VBATT_LOW_v2_OFFSET		0
#define RECHARGE_VBATT_THR_v2_WORD	16
#define RECHARGE_VBATT_THR_v2_OFFSET	1
#define FLOAT_VOLT_v2_WORD		16
#define FLOAT_VOLT_v2_OFFSET		2

/* Other definitions */
#define SLOPE_LIMIT_COEFF_MAX		31
#define FG_SRAM_LEN			504
#define PROFILE_LEN			224
#define PROFILE_COMP_LEN		148
#define KI_COEFF_MAX			62200
#define KI_COEFF_SOC_LEVELS		3
#define BATT_THERM_NUM_COEFFS		3

static struct fg_irq_info fg_irqs[FG_GEN3_IRQ_MAX];

/* DT parameters for FG device */
struct fg_dt_props {
	bool	force_load_profile;
	bool	hold_soc_while_full;
	bool	linearize_soc;
	bool	auto_recharge_soc;
	bool    use_esr_sw;
	bool	disable_esr_pull_dn;
	bool    disable_fg_twm;
	int	cutoff_volt_mv;
	int	empty_volt_mv;
	int	vbatt_low_thr_mv;
	int	chg_term_curr_ma;
	int	chg_term_base_curr_ma;
	int	sys_term_curr_ma;
	int	cutoff_curr_ma;
	int	delta_soc_thr;
	int	recharge_soc_thr;
	int	recharge_volt_thr_mv;
	int	rsense_sel;
	int	esr_timer_charging[NUM_ESR_TIMERS];
	int	esr_timer_awake[NUM_ESR_TIMERS];
	int	esr_timer_asleep[NUM_ESR_TIMERS];
	int     esr_timer_shutdown[NUM_ESR_TIMERS];
	int	rconn_mohms;
	int	esr_clamp_mohms;
	int	cl_start_soc;
	int	cl_max_temp;
	int	cl_min_temp;
	int	cl_max_cap_inc;
	int	cl_max_cap_dec;
	int	cl_max_cap_limit;
	int	cl_min_cap_limit;
	int	jeita_hyst_temp;
	int	batt_temp_delta;
	int	esr_flt_switch_temp;
	int	esr_tight_flt_upct;
	int	esr_broad_flt_upct;
	int	esr_tight_lt_flt_upct;
	int	esr_broad_lt_flt_upct;
	int	esr_flt_rt_switch_temp;
	int	esr_tight_rt_flt_upct;
	int	esr_broad_rt_flt_upct;
	int	slope_limit_temp;
	int	esr_pulse_thresh_ma;
	int	esr_meas_curr_ma;
	int     sync_sleep_threshold_ma;
	int	bmd_en_delay_ms;
	int	ki_coeff_full_soc_dischg;
	int	jeita_thresholds[NUM_JEITA_LEVELS];
	int	ki_coeff_soc[KI_COEFF_SOC_LEVELS];
	int	ki_coeff_med_dischg[KI_COEFF_SOC_LEVELS];
	int	ki_coeff_hi_dischg[KI_COEFF_SOC_LEVELS];
	int	slope_limit_coeffs[SLOPE_LIMIT_NUM_COEFFS];
	u8	batt_therm_coeffs[BATT_THERM_NUM_COEFFS];
};

struct fg_gen3_chip {
	struct fg_dev		fg;
	struct fg_dt_props	dt;
	struct iio_channel	*batt_id_chan;
	struct iio_channel	*die_temp_chan;
	struct votable		*pl_disable_votable;
	struct mutex		qnovo_esr_ctrl_lock;
	struct fg_cyc_ctr_data	cyc_ctr;
	struct fg_cap_learning	cl;
	struct fg_ttf		ttf;
	struct delayed_work	ttf_work;
	struct delayed_work	pl_enable_work;
	enum slope_limit_status	slope_limit_sts;
	char			batt_profile[PROFILE_LEN];
	int			esr_timer_charging_default[NUM_ESR_TIMERS];
	int			ki_coeff_full_soc;
	bool			ki_coeff_dischg_en;
	bool			esr_fcc_ctrl_en;
	bool			esr_flt_cold_temp_en;
	bool			slope_limit_en;
};

static struct fg_sram_param pmi8998_v1_sram_params[] = {
	PARAM(BATT_SOC, BATT_SOC_WORD, BATT_SOC_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_default),
	PARAM(FULL_SOC, FULL_SOC_WORD, FULL_SOC_OFFSET, 2, 1, 1, 0, NULL,
		fg_decode_default),
	PARAM(VOLTAGE_PRED, VOLTAGE_PRED_WORD, VOLTAGE_PRED_OFFSET, 2, 1000,
		244141, 0, NULL, fg_decode_voltage_15b),
	PARAM(OCV, OCV_WORD, OCV_OFFSET, 2, 1000, 244141, 0, NULL,
		fg_decode_voltage_15b),
	PARAM(ESR, ESR_WORD, ESR_OFFSET, 2, 1000, 244141, 0, fg_encode_default,
		fg_decode_value_16b),
	PARAM(RSLOW, RSLOW_WORD, RSLOW_OFFSET, 2, 1000, 244141, 0, NULL,
		fg_decode_value_16b),
	PARAM(ALG_FLAGS, ALG_FLAGS_WORD, ALG_FLAGS_OFFSET, 1, 1, 1, 0, NULL,
		fg_decode_default),
	PARAM(CC_SOC, CC_SOC_WORD, CC_SOC_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_cc_soc),
	PARAM(CC_SOC_SW, CC_SOC_SW_WORD, CC_SOC_SW_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_cc_soc),
	PARAM(ACT_BATT_CAP, ACT_BATT_CAP_BKUP_WORD, ACT_BATT_CAP_BKUP_OFFSET, 2,
		1, 1, 0, NULL, fg_decode_default),
	/* Entries below here are configurable during initialization */
	PARAM(CUTOFF_VOLT, CUTOFF_VOLT_WORD, CUTOFF_VOLT_OFFSET, 2, 1000000,
		244141, 0, fg_encode_voltage, NULL),
	PARAM(EMPTY_VOLT, EMPTY_VOLT_WORD, EMPTY_VOLT_OFFSET, 1, 100000, 390625,
		-2500, fg_encode_voltage, NULL),
	PARAM(VBATT_LOW, VBATT_LOW_WORD, VBATT_LOW_OFFSET, 1, 100000, 390625,
		-2500, fg_encode_voltage, NULL),
	PARAM(VBATT_FULL, VBATT_FULL_WORD, VBATT_FULL_OFFSET, 2, 1000,
		244141, 0, fg_encode_voltage, fg_decode_voltage_15b),
	PARAM(SYS_TERM_CURR, SYS_TERM_CURR_WORD, SYS_TERM_CURR_OFFSET, 3,
		1000000, 122070, 0, fg_encode_current, NULL),
	PARAM(CHG_TERM_CURR, CHG_TERM_CURR_WORD, CHG_TERM_CURR_OFFSET, 1,
		100000, 390625, 0, fg_encode_current, NULL),
	PARAM(CUTOFF_CURR, CUTOFF_CURR_WORD, CUTOFF_CURR_OFFSET, 3,
		1000000, 122070, 0, fg_encode_current, NULL),
	PARAM(DELTA_MSOC_THR, DELTA_MSOC_THR_WORD, DELTA_MSOC_THR_OFFSET, 1,
		2048, 100, 0, fg_encode_default, NULL),
	PARAM(DELTA_BSOC_THR, DELTA_BSOC_THR_WORD, DELTA_BSOC_THR_OFFSET, 1,
		2048, 100, 0, fg_encode_default, NULL),
	PARAM(RECHARGE_SOC_THR, RECHARGE_SOC_THR_WORD, RECHARGE_SOC_THR_OFFSET,
		1, 256, 100, 0, fg_encode_default, NULL),
	PARAM(SYNC_SLEEP_THR, SYNC_SLEEP_THR_WORD, SYNC_SLEEP_THR_OFFSET,
		1, 100000, 390625, 0, fg_encode_default, NULL),
	PARAM(ESR_TIMER_DISCHG_MAX, ESR_TIMER_DISCHG_MAX_WORD,
		ESR_TIMER_DISCHG_MAX_OFFSET, 2, 1, 1, 0, fg_encode_default,
		NULL),
	PARAM(ESR_TIMER_DISCHG_INIT, ESR_TIMER_DISCHG_INIT_WORD,
		ESR_TIMER_DISCHG_INIT_OFFSET, 2, 1, 1, 0, fg_encode_default,
		NULL),
	PARAM(ESR_TIMER_CHG_MAX, ESR_TIMER_CHG_MAX_WORD,
		ESR_TIMER_CHG_MAX_OFFSET, 2, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_TIMER_CHG_INIT, ESR_TIMER_CHG_INIT_WORD,
		ESR_TIMER_CHG_INIT_OFFSET, 2, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_PULSE_THRESH, ESR_PULSE_THRESH_WORD, ESR_PULSE_THRESH_OFFSET,
		1, 100000, 390625, 0, fg_encode_default, NULL),
	PARAM(KI_COEFF_MED_DISCHG, KI_COEFF_MED_DISCHG_WORD,
		KI_COEFF_MED_DISCHG_OFFSET, 1, 1000, 244141, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_HI_DISCHG, KI_COEFF_HI_DISCHG_WORD,
		KI_COEFF_HI_DISCHG_OFFSET, 1, 1000, 244141, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_FULL_SOC, KI_COEFF_FULL_SOC_WORD,
		KI_COEFF_FULL_SOC_OFFSET, 1, 1000, 244141, 0,
		fg_encode_default, NULL),
	PARAM(ESR_TIGHT_FILTER, ESR_FILTER_WORD, ESR_UPD_TIGHT_OFFSET,
		1, 512, 1000000, 0, fg_encode_default, NULL),
	PARAM(ESR_BROAD_FILTER, ESR_FILTER_WORD, ESR_UPD_BROAD_OFFSET,
		1, 512, 1000000, 0, fg_encode_default, NULL),
	PARAM(SLOPE_LIMIT, SLOPE_LIMIT_WORD, SLOPE_LIMIT_OFFSET, 1, 8192, 1000,
		0, fg_encode_default, NULL),
};

static struct fg_sram_param pmi8998_v2_sram_params[] = {
	PARAM(BATT_SOC, BATT_SOC_WORD, BATT_SOC_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_default),
	PARAM(FULL_SOC, FULL_SOC_WORD, FULL_SOC_OFFSET, 2, 1, 1, 0, NULL,
		fg_decode_default),
	PARAM(VOLTAGE_PRED, VOLTAGE_PRED_WORD, VOLTAGE_PRED_OFFSET, 2, 1000,
		244141, 0, NULL, fg_decode_voltage_15b),
	PARAM(OCV, OCV_WORD, OCV_OFFSET, 2, 1000, 244141, 0, NULL,
		fg_decode_voltage_15b),
	PARAM(ESR, ESR_WORD, ESR_OFFSET, 2, 1000, 244141, 0, fg_encode_default,
		fg_decode_value_16b),
	PARAM(RSLOW, RSLOW_WORD, RSLOW_OFFSET, 2, 1000, 244141, 0, NULL,
		fg_decode_value_16b),
	PARAM(ALG_FLAGS, ALG_FLAGS_WORD, ALG_FLAGS_OFFSET, 1, 1, 1, 0, NULL,
		fg_decode_default),
	PARAM(CC_SOC, CC_SOC_WORD, CC_SOC_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_cc_soc),
	PARAM(CC_SOC_SW, CC_SOC_SW_WORD, CC_SOC_SW_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_cc_soc),
	PARAM(ACT_BATT_CAP, ACT_BATT_CAP_BKUP_WORD, ACT_BATT_CAP_BKUP_OFFSET, 2,
		1, 1, 0, NULL, fg_decode_default),
	PARAM(TIMEBASE, KI_COEFF_MED_DISCHG_WORD, TIMEBASE_OFFSET, 2, 1000,
		61000, 0, fg_encode_default, NULL),
	/* Entries below here are configurable during initialization */
	PARAM(CUTOFF_VOLT, CUTOFF_VOLT_WORD, CUTOFF_VOLT_OFFSET, 2, 1000000,
		244141, 0, fg_encode_voltage, NULL),
	PARAM(EMPTY_VOLT, EMPTY_VOLT_v2_WORD, EMPTY_VOLT_v2_OFFSET, 1, 1000,
		15625, -2000, fg_encode_voltage, NULL),
	PARAM(VBATT_LOW, VBATT_LOW_v2_WORD, VBATT_LOW_v2_OFFSET, 1, 1000,
		15625, -2000, fg_encode_voltage, NULL),
	PARAM(FLOAT_VOLT, FLOAT_VOLT_v2_WORD, FLOAT_VOLT_v2_OFFSET, 1, 1000,
		15625, -2000, fg_encode_voltage, NULL),
	PARAM(VBATT_FULL, VBATT_FULL_WORD, VBATT_FULL_OFFSET, 2, 1000,
		244141, 0, fg_encode_voltage, fg_decode_voltage_15b),
	PARAM(SYS_TERM_CURR, SYS_TERM_CURR_WORD, SYS_TERM_CURR_OFFSET, 3,
		1000000, 122070, 0, fg_encode_current, NULL),
	PARAM(CHG_TERM_CURR, CHG_TERM_CURR_v2_WORD, CHG_TERM_CURR_v2_OFFSET, 1,
		100000, 390625, 0, fg_encode_current, NULL),
	PARAM(CHG_TERM_BASE_CURR, CHG_TERM_CURR_v2_WORD,
		CHG_TERM_BASE_CURR_v2_OFFSET, 1, 1024, 1000, 0,
		fg_encode_current, NULL),
	PARAM(CUTOFF_CURR, CUTOFF_CURR_WORD, CUTOFF_CURR_OFFSET, 3,
		1000000, 122070, 0, fg_encode_current, NULL),
	PARAM(DELTA_MSOC_THR, DELTA_MSOC_THR_v2_WORD, DELTA_MSOC_THR_v2_OFFSET,
		1, 2048, 100, 0, fg_encode_default, NULL),
	PARAM(DELTA_BSOC_THR, DELTA_BSOC_THR_v2_WORD, DELTA_BSOC_THR_v2_OFFSET,
		1, 2048, 100, 0, fg_encode_default, NULL),
	PARAM(RECHARGE_SOC_THR, RECHARGE_SOC_THR_v2_WORD,
		RECHARGE_SOC_THR_v2_OFFSET, 1, 256, 100, 0, fg_encode_default,
		NULL),
	PARAM(SYNC_SLEEP_THR, SYNC_SLEEP_THR_v2_WORD, SYNC_SLEEP_THR_v2_OFFSET,
		1, 100000, 390625, 0, fg_encode_default, NULL),
	PARAM(RECHARGE_VBATT_THR, RECHARGE_VBATT_THR_v2_WORD,
		RECHARGE_VBATT_THR_v2_OFFSET, 1, 1000, 15625, -2000,
		fg_encode_voltage, NULL),
	PARAM(ESR_TIMER_DISCHG_MAX, ESR_TIMER_DISCHG_MAX_WORD,
		ESR_TIMER_DISCHG_MAX_OFFSET, 2, 1, 1, 0, fg_encode_default,
		NULL),
	PARAM(ESR_TIMER_DISCHG_INIT, ESR_TIMER_DISCHG_INIT_WORD,
		ESR_TIMER_DISCHG_INIT_OFFSET, 2, 1, 1, 0, fg_encode_default,
		NULL),
	PARAM(ESR_TIMER_CHG_MAX, ESR_TIMER_CHG_MAX_WORD,
		ESR_TIMER_CHG_MAX_OFFSET, 2, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_TIMER_CHG_INIT, ESR_TIMER_CHG_INIT_WORD,
		ESR_TIMER_CHG_INIT_OFFSET, 2, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_PULSE_THRESH, ESR_PULSE_THRESH_WORD, ESR_PULSE_THRESH_OFFSET,
		1, 100000, 390625, 0, fg_encode_default, NULL),
	PARAM(KI_COEFF_MED_DISCHG, KI_COEFF_MED_DISCHG_v2_WORD,
		KI_COEFF_MED_DISCHG_v2_OFFSET, 1, 1000, 244141, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_HI_DISCHG, KI_COEFF_HI_DISCHG_v2_WORD,
		KI_COEFF_HI_DISCHG_v2_OFFSET, 1, 1000, 244141, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_FULL_SOC, KI_COEFF_FULL_SOC_WORD,
		KI_COEFF_FULL_SOC_OFFSET, 1, 1000, 244141, 0,
		fg_encode_default, NULL),
	PARAM(ESR_TIGHT_FILTER, ESR_FILTER_WORD, ESR_UPD_TIGHT_OFFSET,
		1, 512, 1000000, 0, fg_encode_default, NULL),
	PARAM(ESR_BROAD_FILTER, ESR_FILTER_WORD, ESR_UPD_BROAD_OFFSET,
		1, 512, 1000000, 0, fg_encode_default, NULL),
	PARAM(SLOPE_LIMIT, SLOPE_LIMIT_WORD, SLOPE_LIMIT_OFFSET, 1, 8192, 1000,
		0, fg_encode_default, NULL),
};

static struct fg_alg_flag pmi8998_v1_alg_flags[] = {
	[ALG_FLAG_SOC_LT_OTG_MIN]	= {
		.name	= "SOC_LT_OTG_MIN",
		.bit	= BIT(0),
	},
	[ALG_FLAG_SOC_LT_RECHARGE]	= {
		.name	= "SOC_LT_RECHARGE",
		.bit	= BIT(1),
	},
	[ALG_FLAG_IBATT_LT_ITERM]	= {
		.name	= "IBATT_LT_ITERM",
		.bit	= BIT(2),
	},
	[ALG_FLAG_IBATT_GT_HPM]		= {
		.name	= "IBATT_GT_HPM",
		.bit	= BIT(3),
	},
	[ALG_FLAG_IBATT_GT_UPM]		= {
		.name	= "IBATT_GT_UPM",
		.bit	= BIT(4),
	},
	[ALG_FLAG_VBATT_LT_RECHARGE]	= {
		.name	= "VBATT_LT_RECHARGE",
		.bit	= BIT(5),
	},
	[ALG_FLAG_VBATT_GT_VFLOAT]	= {
		.invalid = true,
	},
};

static struct fg_alg_flag pmi8998_v2_alg_flags[] = {
	[ALG_FLAG_SOC_LT_OTG_MIN]	= {
		.name	= "SOC_LT_OTG_MIN",
		.bit	= BIT(0),
	},
	[ALG_FLAG_SOC_LT_RECHARGE]	= {
		.name	= "SOC_LT_RECHARGE",
		.bit	= BIT(1),
	},
	[ALG_FLAG_IBATT_LT_ITERM]	= {
		.name	= "IBATT_LT_ITERM",
		.bit	= BIT(2),
	},
	[ALG_FLAG_IBATT_GT_HPM]		= {
		.name	= "IBATT_GT_HPM",
		.bit	= BIT(4),
	},
	[ALG_FLAG_IBATT_GT_UPM]		= {
		.name	= "IBATT_GT_UPM",
		.bit	= BIT(5),
	},
	[ALG_FLAG_VBATT_LT_RECHARGE]	= {
		.name	= "VBATT_LT_RECHARGE",
		.bit	= BIT(6),
	},
	[ALG_FLAG_VBATT_GT_VFLOAT]	= {
		.name	= "VBATT_GT_VFLOAT",
		.bit	= BIT(7),
	},
};

static int fg_gen3_debug_mask;
module_param_named(
	debug_mask, fg_gen3_debug_mask, int, 0600
);

static bool fg_profile_dump;
module_param_named(
	profile_dump, fg_profile_dump, bool, 0600
);

static int fg_sram_dump_period_ms = 20000;
module_param_named(
	sram_dump_period_ms, fg_sram_dump_period_ms, int, 0600
);

static int fg_restart_mp;
static bool fg_sram_dump;

/* All getters HERE */

#define CC_SOC_30BIT	GENMASK(29, 0)
static int fg_get_charge_raw(struct fg_dev *fg, int *val)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc, cc_soc;

	rc = fg_get_sram_prop(fg, FG_SRAM_CC_SOC, &cc_soc);
	if (rc < 0) {
		pr_err("Error in getting CC_SOC, rc=%d\n", rc);
		return rc;
	}

	*val = div_s64((int64_t)cc_soc * chip->cl.nom_cap_uah, CC_SOC_30BIT);
	return 0;
}

#define BATT_SOC_32BIT	GENMASK(31, 0)
static int fg_get_charge_counter_shadow(struct fg_dev *fg, int *val)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc;
	unsigned int batt_soc;

	rc = fg_get_sram_prop(fg, FG_SRAM_BATT_SOC, &batt_soc);
	if (rc < 0) {
		pr_err("Error in getting BATT_SOC, rc=%d\n", rc);
		return rc;
	}

	*val = div_u64((uint64_t)batt_soc * chip->cl.learned_cc_uah,
							BATT_SOC_32BIT);
	return 0;
}

static int fg_get_charge_counter(struct fg_dev *fg, int *val)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc;
	int cc_soc;

	rc = fg_get_sram_prop(fg, FG_SRAM_CC_SOC_SW, &cc_soc);
	if (rc < 0) {
		pr_err("Error in getting CC_SOC_SW, rc=%d\n", rc);
		return rc;
	}

	*val = div_s64((int64_t)cc_soc * chip->cl.learned_cc_uah, CC_SOC_30BIT);
	return 0;
}

static int fg_get_jeita_threshold(struct fg_dev *fg,
				enum jeita_levels level, int *temp_decidegC)
{
	int rc;
	u8 val;
	u16 reg;

	switch (level) {
	case JEITA_COLD:
		reg = BATT_INFO_JEITA_TOO_COLD(fg);
		break;
	case JEITA_COOL:
		reg = BATT_INFO_JEITA_COLD(fg);
		break;
	case JEITA_WARM:
		reg = BATT_INFO_JEITA_HOT(fg);
		break;
	case JEITA_HOT:
		reg = BATT_INFO_JEITA_TOO_HOT(fg);
		break;
	default:
		return -EINVAL;
	}

	rc = fg_read(fg, reg, &val, 1);
	if (rc < 0) {
		pr_err("Error in reading jeita level %d, rc=%d\n", level, rc);
		return rc;
	}

	/* Resolution is 0.5C. Base is -30C. */
	*temp_decidegC = (((5 * val) / 10) - 30) * 10;
	return 0;
}

static int fg_get_battery_temp(struct fg_dev *fg, int *val)
{
	int rc = 0, temp;
	u8 buf[2];

	rc = fg_read(fg, BATT_INFO_BATT_TEMP_LSB(fg), buf, 2);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			BATT_INFO_BATT_TEMP_LSB(fg), rc);
		return rc;
	}

	temp = ((buf[1] & BATT_TEMP_MSB_MASK) << 8) |
		(buf[0] & BATT_TEMP_LSB_MASK);
	temp = DIV_ROUND_CLOSEST(temp, 4);

	/* Value is in Kelvin; Convert it to deciDegC */
	temp = (temp - 273) * 10;
	*val = temp;
	return 0;
}

static bool is_batt_empty(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	u8 status;
	int rc, vbatt_uv, msoc;

	rc = fg_read(fg, BATT_SOC_INT_RT_STS(fg), &status, 1);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			BATT_SOC_INT_RT_STS(fg), rc);
		return false;
	}

	if (!(status & MSOC_EMPTY_BIT))
		return false;

	rc = fg_get_battery_voltage(fg, &vbatt_uv);
	if (rc < 0) {
		pr_err("failed to get battery voltage, rc=%d\n", rc);
		return false;
	}

	rc = fg_get_msoc(fg, &msoc);
	if (!rc)
		pr_warn("batt_soc_rt_sts: %x vbatt: %d uV msoc:%d\n", status,
			vbatt_uv, msoc);

	return ((vbatt_uv < chip->dt.cutoff_volt_mv * 1000) ? true : false);
}

static int fg_get_debug_batt_id(struct fg_dev *fg, int *batt_id)
{
	int rc;
	u64 temp;
	u8 buf[2];

	rc = fg_read(fg, ADC_RR_FAKE_BATT_LOW_LSB(fg), buf, 2);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			ADC_RR_FAKE_BATT_LOW_LSB(fg), rc);
		return rc;
	}

	/*
	 * Fake battery threshold is encoded in the following format.
	 * Threshold (code) = (battery_id in Ohms) * 0.00015 * 2^10 / 2.5
	 */
	temp = (buf[1] << 8 | buf[0]) * 2500000;
	do_div(temp, 150 * 1024);
	batt_id[0] = temp;
	rc = fg_read(fg, ADC_RR_FAKE_BATT_HIGH_LSB(fg), buf, 2);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			ADC_RR_FAKE_BATT_HIGH_LSB(fg), rc);
		return rc;
	}

	temp = (buf[1] << 8 | buf[0]) * 2500000;
	do_div(temp, 150 * 1024);
	batt_id[1] = temp;
	pr_debug("debug batt_id range: [%d %d]\n", batt_id[0], batt_id[1]);
	return 0;
}

static bool is_debug_batt_id(struct fg_dev *fg)
{
	int debug_batt_id[2], rc;

	if (fg->batt_id_ohms < 0)
		return false;

	rc = fg_get_debug_batt_id(fg, debug_batt_id);
	if (rc < 0) {
		pr_err("Failed to get debug batt_id, rc=%d\n", rc);
		return false;
	}

	if (is_between(debug_batt_id[0], debug_batt_id[1],
		fg->batt_id_ohms)) {
		fg_dbg(fg, FG_POWER_SUPPLY, "Debug battery id: %dohms\n",
			fg->batt_id_ohms);
		return true;
	}

	return false;
}

#define DEBUG_BATT_SOC	67
#define BATT_MISS_SOC	50
#define EMPTY_SOC	0
static int fg_get_prop_capacity(struct fg_dev *fg, int *val)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc, msoc;

	if (is_debug_batt_id(fg)) {
		*val = DEBUG_BATT_SOC;
		return 0;
	}

	if (fg->fg_restarting) {
		*val = fg->last_soc;
		return 0;
	}

	if (fg->battery_missing || !fg->soc_reporting_ready) {
		*val = BATT_MISS_SOC;
		return 0;
	}

	if (is_batt_empty(fg)) {
		*val = EMPTY_SOC;
		return 0;
	}

	if (fg->charge_full) {
		*val = FULL_CAPACITY;
		return 0;
	}

	rc = fg_get_msoc(fg, &msoc);
	if (rc < 0)
		return rc;

	if (chip->dt.linearize_soc && fg->delta_soc > 0)
		*val = fg->maint_soc;
	else
		*val = msoc;
	return 0;
}

static int fg_batt_missing_config(struct fg_dev *fg, bool enable)
{
	int rc;

	rc = fg_masked_write(fg, BATT_INFO_BATT_MISS_CFG(fg),
			BM_FROM_BATT_ID_BIT, enable ? BM_FROM_BATT_ID_BIT : 0);
	if (rc < 0)
		pr_err("Error in writing to %04x, rc=%d\n",
			BATT_INFO_BATT_MISS_CFG(fg), rc);
	return rc;
}

static int fg_get_batt_id(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc, ret, batt_id = 0;

	if (!chip->batt_id_chan)
		return -EINVAL;

	rc = fg_batt_missing_config(fg, false);
	if (rc < 0) {
		pr_err("Error in disabling BMD, rc=%d\n", rc);
		return rc;
	}

	rc = iio_read_channel_processed(chip->batt_id_chan, &batt_id);
	if (rc < 0) {
		pr_err("Error in reading batt_id channel, rc:%d\n", rc);
		goto out;
	}

	/* Wait for BATT_ID to settle down before enabling BMD again */
	msleep(chip->dt.bmd_en_delay_ms);

	fg_dbg(fg, FG_STATUS, "batt_id: %d\n", batt_id);
	fg->batt_id_ohms = batt_id;
out:
	ret = fg_batt_missing_config(fg, true);
	if (ret < 0) {
		pr_err("Error in enabling BMD, ret=%d\n", ret);
		return ret;
	}

	vote(fg->batt_miss_irq_en_votable, BATT_MISS_IRQ_VOTER, true, 0);
	return rc;
}

static int fg_get_batt_profile(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	struct device_node *node = fg->dev->of_node;
	struct device_node *batt_node, *profile_node;
	const char *data;
	int rc, len;

	batt_node = of_find_node_by_name(node, "qcom,battery-data");
	if (!batt_node) {
		pr_err("Batterydata not available\n");
		return -ENXIO;
	}

	profile_node = of_batterydata_get_best_profile(batt_node,
				fg->batt_id_ohms / 1000, NULL);
	if (IS_ERR(profile_node))
		return PTR_ERR(profile_node);

	if (!profile_node) {
		pr_err("couldn't find profile handle\n");
		return -ENODATA;
	}

	rc = of_property_read_string(profile_node, "qcom,battery-type",
			&fg->bp.batt_type_str);
	if (rc < 0) {
		pr_err("battery type unavailable, rc:%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(profile_node, "qcom,max-voltage-uv",
			&fg->bp.float_volt_uv);
	if (rc < 0) {
		pr_err("battery float voltage unavailable, rc:%d\n", rc);
		fg->bp.float_volt_uv = -EINVAL;
	}

	rc = of_property_read_u32(profile_node, "qcom,fastchg-current-ma",
			&fg->bp.fastchg_curr_ma);
	if (rc < 0) {
		pr_err("battery fastchg current unavailable, rc:%d\n", rc);
		fg->bp.fastchg_curr_ma = -EINVAL;
	}

	rc = of_property_read_u32(profile_node, "qcom,fg-cc-cv-threshold-mv",
			&fg->bp.vbatt_full_mv);
	if (rc < 0) {
		pr_err("battery cc_cv threshold unavailable, rc:%d\n", rc);
		fg->bp.vbatt_full_mv = -EINVAL;
	}

	data = of_get_property(profile_node, "qcom,fg-profile-data", &len);
	if (!data) {
		pr_err("No profile data available\n");
		return -ENODATA;
	}

	if (len != PROFILE_LEN) {
		pr_err("battery profile incorrect size: %d\n", len);
		return -EINVAL;
	}

	fg->profile_available = true;
	memcpy(chip->batt_profile, data, len);

	return 0;
}

static inline void get_batt_temp_delta(int delta, u8 *val)
{
	switch (delta) {
	case 2:
		*val = BTEMP_DELTA_2K;
		break;
	case 4:
		*val = BTEMP_DELTA_4K;
		break;
	case 6:
		*val = BTEMP_DELTA_6K;
		break;
	case 10:
		*val = BTEMP_DELTA_10K;
		break;
	default:
		*val = BTEMP_DELTA_2K;
		break;
	};
}

static inline void get_esr_meas_current(int curr_ma, u8 *val)
{
	switch (curr_ma) {
	case 60:
		*val = ESR_MEAS_CUR_60MA;
		break;
	case 120:
		*val = ESR_MEAS_CUR_120MA;
		break;
	case 180:
		*val = ESR_MEAS_CUR_180MA;
		break;
	case 240:
		*val = ESR_MEAS_CUR_240MA;
		break;
	default:
		*val = ESR_MEAS_CUR_120MA;
		break;
	};

	*val <<= ESR_PULL_DOWN_IVAL_SHIFT;
}

/* Other functions HERE */

static int fg_batt_miss_irq_en_cb(struct votable *votable, void *data,
					int enable, const char *client)
{
	struct fg_dev *fg = data;

	if (!fg->irqs[BATT_MISSING_IRQ].irq)
		return 0;

	if (enable) {
		enable_irq(fg->irqs[BATT_MISSING_IRQ].irq);
		enable_irq_wake(fg->irqs[BATT_MISSING_IRQ].irq);
	} else {
		disable_irq_wake(fg->irqs[BATT_MISSING_IRQ].irq);
		disable_irq_nosync(fg->irqs[BATT_MISSING_IRQ].irq);
	}

	return 0;
}

static int fg_delta_bsoc_irq_en_cb(struct votable *votable, void *data,
					int enable, const char *client)
{
	struct fg_dev *fg = data;

	if (!fg->irqs[BSOC_DELTA_IRQ].irq)
		return 0;

	if (enable) {
		enable_irq(fg->irqs[BSOC_DELTA_IRQ].irq);
		enable_irq_wake(fg->irqs[BSOC_DELTA_IRQ].irq);
	} else {
		disable_irq_wake(fg->irqs[BSOC_DELTA_IRQ].irq);
		disable_irq_nosync(fg->irqs[BSOC_DELTA_IRQ].irq);
	}

	return 0;
}

static int fg_awake_cb(struct votable *votable, void *data, int awake,
			const char *client)
{
	struct fg_dev *fg = data;

	if (awake)
		pm_stay_awake(fg->dev);
	else
		pm_relax(fg->dev);

	pr_debug("client: %s awake: %d\n", client, awake);
	return 0;
}

static int fg_prime_cc_soc_sw(struct fg_dev *fg, unsigned int cc_soc_sw)
{
	int rc;

	rc = fg_sram_write(fg, fg->sp[FG_SRAM_CC_SOC_SW].addr_word,
		fg->sp[FG_SRAM_CC_SOC_SW].addr_byte, (u8 *)&cc_soc_sw,
		fg->sp[FG_SRAM_CC_SOC_SW].len, FG_IMA_ATOMIC);
	if (rc < 0)
		pr_err("Error in writing cc_soc_sw, rc=%d\n", rc);
	else
		fg_dbg(fg, FG_STATUS, "cc_soc_sw: %u\n", cc_soc_sw);

	return rc;
}

static int fg_save_learned_cap_to_sram(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int16_t cc_mah;
	int rc;

	if (fg->battery_missing || !chip->cl.learned_cc_uah)
		return -EPERM;

	cc_mah = div64_s64(chip->cl.learned_cc_uah, 1000);
	/* Write to a backup register to use across reboot */
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_ACT_BATT_CAP].addr_word,
			fg->sp[FG_SRAM_ACT_BATT_CAP].addr_byte, (u8 *)&cc_mah,
			fg->sp[FG_SRAM_ACT_BATT_CAP].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing act_batt_cap_bkup, rc=%d\n", rc);
		return rc;
	}

	/* Write to actual capacity register for coulomb counter operation */
	rc = fg_sram_write(fg, ACT_BATT_CAP_WORD, ACT_BATT_CAP_OFFSET,
			(u8 *)&cc_mah, fg->sp[FG_SRAM_ACT_BATT_CAP].len,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing act_batt_cap, rc=%d\n", rc);
		return rc;
	}

	fg_dbg(fg, FG_CAP_LEARN, "learned capacity %llduah/%dmah stored\n",
		chip->cl.learned_cc_uah, cc_mah);
	return 0;
}

#define CAPACITY_DELTA_DECIPCT	500
static int fg_load_learned_cap_from_sram(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc, act_cap_mah;
	int64_t delta_cc_uah, pct_nom_cap_uah;

	rc = fg_get_sram_prop(fg, FG_SRAM_ACT_BATT_CAP, &act_cap_mah);
	if (rc < 0) {
		pr_err("Error in getting ACT_BATT_CAP, rc=%d\n", rc);
		return rc;
	}

	chip->cl.learned_cc_uah = act_cap_mah * 1000;

	if (chip->cl.learned_cc_uah != chip->cl.nom_cap_uah) {
		if (chip->cl.learned_cc_uah == 0)
			chip->cl.learned_cc_uah = chip->cl.nom_cap_uah;

		delta_cc_uah = abs(chip->cl.learned_cc_uah -
					chip->cl.nom_cap_uah);
		pct_nom_cap_uah = div64_s64((int64_t)chip->cl.nom_cap_uah *
				CAPACITY_DELTA_DECIPCT, 1000);
		/*
		 * If the learned capacity is out of range by 50% from the
		 * nominal capacity, then overwrite the learned capacity with
		 * the nominal capacity.
		 */
		if (chip->cl.nom_cap_uah && delta_cc_uah > pct_nom_cap_uah) {
			fg_dbg(fg, FG_CAP_LEARN, "learned_cc_uah: %lld is higher than expected, capping it to nominal: %lld\n",
				chip->cl.learned_cc_uah, chip->cl.nom_cap_uah);
			chip->cl.learned_cc_uah = chip->cl.nom_cap_uah;
		}

		rc = fg_save_learned_cap_to_sram(fg);
		if (rc < 0)
			pr_err("Error in saving learned_cc_uah, rc=%d\n", rc);
	}

	fg_dbg(fg, FG_CAP_LEARN, "learned_cc_uah:%lld nom_cap_uah: %lld\n",
		chip->cl.learned_cc_uah, chip->cl.nom_cap_uah);
	return 0;
}

static bool is_temp_valid_cap_learning(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc, batt_temp;

	rc = fg_get_battery_temp(fg, &batt_temp);
	if (rc < 0) {
		pr_err("Error in getting batt_temp\n");
		return false;
	}

	if (batt_temp > chip->dt.cl_max_temp ||
		batt_temp < chip->dt.cl_min_temp) {
		fg_dbg(fg, FG_CAP_LEARN, "batt temp %d out of range [%d %d]\n",
			batt_temp, chip->dt.cl_min_temp, chip->dt.cl_max_temp);
		return false;
	}

	return true;
}

#define QNOVO_CL_SKEW_DECIPCT	-30
static void fg_cap_learning_post_process(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int64_t max_inc_val, min_dec_val, old_cap;
	int rc;

	if (is_qnovo_en(fg)) {
		fg_dbg(fg, FG_CAP_LEARN, "applying skew %d on current learnt capacity %lld\n",
			QNOVO_CL_SKEW_DECIPCT, chip->cl.final_cc_uah);
		chip->cl.final_cc_uah = chip->cl.final_cc_uah *
						(1000 + QNOVO_CL_SKEW_DECIPCT);
		chip->cl.final_cc_uah = div64_u64(chip->cl.final_cc_uah, 1000);
	}

	max_inc_val = chip->cl.learned_cc_uah
			* (1000 + chip->dt.cl_max_cap_inc);
	max_inc_val = div64_u64(max_inc_val, 1000);

	min_dec_val = chip->cl.learned_cc_uah
			* (1000 - chip->dt.cl_max_cap_dec);
	min_dec_val = div64_u64(min_dec_val, 1000);

	old_cap = chip->cl.learned_cc_uah;
	if (chip->cl.final_cc_uah > max_inc_val)
		chip->cl.learned_cc_uah = max_inc_val;
	else if (chip->cl.final_cc_uah < min_dec_val)
		chip->cl.learned_cc_uah = min_dec_val;
	else
		chip->cl.learned_cc_uah =
			chip->cl.final_cc_uah;

	if (chip->dt.cl_max_cap_limit) {
		max_inc_val = (int64_t)chip->cl.nom_cap_uah * (1000 +
				chip->dt.cl_max_cap_limit);
		max_inc_val = div64_u64(max_inc_val, 1000);
		if (chip->cl.final_cc_uah > max_inc_val) {
			fg_dbg(fg, FG_CAP_LEARN, "learning capacity %lld goes above max limit %lld\n",
				chip->cl.final_cc_uah, max_inc_val);
			chip->cl.learned_cc_uah = max_inc_val;
		}
	}

	if (chip->dt.cl_min_cap_limit) {
		min_dec_val = (int64_t)chip->cl.nom_cap_uah * (1000 -
				chip->dt.cl_min_cap_limit);
		min_dec_val = div64_u64(min_dec_val, 1000);
		if (chip->cl.final_cc_uah < min_dec_val) {
			fg_dbg(fg, FG_CAP_LEARN, "learning capacity %lld goes below min limit %lld\n",
				chip->cl.final_cc_uah, min_dec_val);
			chip->cl.learned_cc_uah = min_dec_val;
		}
	}

	rc = fg_save_learned_cap_to_sram(fg);
	if (rc < 0)
		pr_err("Error in saving learned_cc_uah, rc=%d\n", rc);

	fg_dbg(fg, FG_CAP_LEARN, "final cc_uah = %lld, learned capacity %lld -> %lld uah\n",
		chip->cl.final_cc_uah, old_cap, chip->cl.learned_cc_uah);
}

static int fg_cap_learning_process_full_data(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc;
	unsigned int cc_soc_sw;
	int64_t delta_cc_uah;
	unsigned int cc_soc_delta_pct;

	rc = fg_get_sram_prop(fg, FG_SRAM_CC_SOC_SW, &cc_soc_sw);
	if (rc < 0) {
		pr_err("Error in getting CC_SOC_SW, rc=%d\n", rc);
		return rc;
	}

	cc_soc_delta_pct =
		div64_s64((int64_t)(cc_soc_sw - chip->cl.init_cc_soc_sw) * 100,
			CC_SOC_30BIT);

	/* If the delta is < 50%, then skip processing full data */
	if (cc_soc_delta_pct < 50) {
		pr_err("cc_soc_delta_pct: %d\n", cc_soc_delta_pct);
		return -ERANGE;
	}

	delta_cc_uah = div64_u64(chip->cl.learned_cc_uah * cc_soc_delta_pct,
				100);
	chip->cl.final_cc_uah = chip->cl.init_cc_uah + delta_cc_uah;
	fg_dbg(fg, FG_CAP_LEARN, "Current cc_soc=%d cc_soc_delta_pct=%u total_cc_uah=%llu\n",
		cc_soc_sw, cc_soc_delta_pct, chip->cl.final_cc_uah);
	return 0;
}

static int fg_cap_learning_begin(struct fg_dev *fg, u32 batt_soc)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc;
	unsigned int batt_soc_msb, cc_soc_sw;

	batt_soc_msb = batt_soc >> 24;
	if (DIV_ROUND_CLOSEST(batt_soc_msb * 100, FULL_SOC_RAW) >
		chip->dt.cl_start_soc) {
		fg_dbg(fg, FG_CAP_LEARN, "Battery SOC %u is high!, not starting\n",
			batt_soc_msb);
		return -EINVAL;
	}

	chip->cl.init_cc_uah = div64_u64(chip->cl.learned_cc_uah * batt_soc_msb,
					FULL_SOC_RAW);

	/* Prime cc_soc_sw with battery SOC when capacity learning begins */
	cc_soc_sw = div64_u64((uint64_t)batt_soc * CC_SOC_30BIT,
				BATT_SOC_32BIT);
	rc = fg_prime_cc_soc_sw(fg, cc_soc_sw);
	if (rc < 0) {
		pr_err("Error in writing cc_soc_sw, rc=%d\n", rc);
		goto out;
	}

	chip->cl.init_cc_soc_sw = cc_soc_sw;
	fg_dbg(fg, FG_CAP_LEARN, "Capacity learning started @ battery SOC %d init_cc_soc_sw:%d\n",
		batt_soc_msb, chip->cl.init_cc_soc_sw);
out:
	return rc;
}

static int fg_cap_learning_done(struct fg_dev *fg)
{
	int rc;
	unsigned int cc_soc_sw;

	rc = fg_cap_learning_process_full_data(fg);
	if (rc < 0) {
		pr_err("Error in processing cap learning full data, rc=%d\n",
			rc);
		goto out;
	}

	/* Write a FULL value to cc_soc_sw */
	cc_soc_sw = CC_SOC_30BIT;
	rc = fg_prime_cc_soc_sw(fg, cc_soc_sw);
	if (rc < 0) {
		pr_err("Error in writing cc_soc_sw, rc=%d\n", rc);
		goto out;
	}

	fg_cap_learning_post_process(fg);
out:
	return rc;
}

static void fg_cap_learning_update(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc;
	unsigned int batt_soc, batt_soc_msb, cc_soc_sw;
	bool input_present = is_input_present(fg);
	bool prime_cc = false;

	mutex_lock(&chip->cl.lock);

	if (!is_temp_valid_cap_learning(fg) || !chip->cl.learned_cc_uah ||
		fg->battery_missing) {
		fg_dbg(fg, FG_CAP_LEARN, "Aborting cap_learning %lld\n",
			chip->cl.learned_cc_uah);
		chip->cl.active = false;
		chip->cl.init_cc_uah = 0;
		goto out;
	}

	if (fg->charge_status == fg->prev_charge_status)
		goto out;

	rc = fg_get_sram_prop(fg, FG_SRAM_BATT_SOC, &batt_soc);
	if (rc < 0) {
		pr_err("Error in getting ACT_BATT_CAP, rc=%d\n", rc);
		goto out;
	}

	batt_soc_msb = (u32)batt_soc >> 24;
	fg_dbg(fg, FG_CAP_LEARN, "Chg_status: %d cl_active: %d batt_soc: %d\n",
		fg->charge_status, chip->cl.active, batt_soc_msb);

	/* Initialize the starting point of learning capacity */
	if (!chip->cl.active) {
		if (fg->charge_status == POWER_SUPPLY_STATUS_CHARGING) {
			rc = fg_cap_learning_begin(fg, batt_soc);
			chip->cl.active = (rc == 0);
		} else {
			if ((fg->charge_status ==
					POWER_SUPPLY_STATUS_DISCHARGING) ||
					fg->charge_done)
				prime_cc = true;
		}
	} else {
		if (fg->charge_done) {
			rc = fg_cap_learning_done(fg);
			if (rc < 0)
				pr_err("Error in completing capacity learning, rc=%d\n",
					rc);

			chip->cl.active = false;
			chip->cl.init_cc_uah = 0;
		}

		if (fg->charge_status == POWER_SUPPLY_STATUS_DISCHARGING) {
			if (!input_present) {
				fg_dbg(fg, FG_CAP_LEARN, "Capacity learning aborted @ battery SOC %d\n",
					 batt_soc_msb);
				chip->cl.active = false;
				chip->cl.init_cc_uah = 0;
				prime_cc = true;
			}
		}

		if (fg->charge_status == POWER_SUPPLY_STATUS_NOT_CHARGING) {
			if (is_qnovo_en(fg) && input_present) {
				/*
				 * Don't abort the capacity learning when qnovo
				 * is enabled and input is present where the
				 * charging status can go to "not charging"
				 * intermittently.
				 */
			} else {
				fg_dbg(fg, FG_CAP_LEARN, "Capacity learning aborted @ battery SOC %d\n",
					batt_soc_msb);
				chip->cl.active = false;
				chip->cl.init_cc_uah = 0;
				prime_cc = true;
			}
		}
	}

	/*
	 * Prime CC_SOC_SW when the device is not charging or during charge
	 * termination when the capacity learning is not active.
	 */

	if (prime_cc) {
		if (fg->charge_done)
			cc_soc_sw = CC_SOC_30BIT;
		else
			cc_soc_sw = div_u64((uint64_t)batt_soc *
					CC_SOC_30BIT, BATT_SOC_32BIT);

		rc = fg_prime_cc_soc_sw(fg, cc_soc_sw);
		if (rc < 0)
			pr_err("Error in writing cc_soc_sw, rc=%d\n",
				rc);
	}

out:
	mutex_unlock(&chip->cl.lock);
}

#define KI_COEFF_MED_DISCHG_DEFAULT	1500
#define KI_COEFF_HI_DISCHG_DEFAULT	2200
static int fg_adjust_ki_coeff_dischg(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc, i, msoc;
	int ki_coeff_med = KI_COEFF_MED_DISCHG_DEFAULT;
	int ki_coeff_hi = KI_COEFF_HI_DISCHG_DEFAULT;
	u8 val;

	if (!chip->ki_coeff_dischg_en)
		return 0;

	rc = fg_get_prop_capacity(fg, &msoc);
	if (rc < 0) {
		pr_err("Error in getting capacity, rc=%d\n", rc);
		return rc;
	}

	if (fg->charge_status == POWER_SUPPLY_STATUS_DISCHARGING) {
		for (i = KI_COEFF_SOC_LEVELS - 1; i >= 0; i--) {
			if (msoc < chip->dt.ki_coeff_soc[i]) {
				ki_coeff_med = chip->dt.ki_coeff_med_dischg[i];
				ki_coeff_hi = chip->dt.ki_coeff_hi_dischg[i];
			}
		}
	}

	fg_encode(fg->sp, FG_SRAM_KI_COEFF_MED_DISCHG, ki_coeff_med, &val);
	rc = fg_sram_write(fg,
			fg->sp[FG_SRAM_KI_COEFF_MED_DISCHG].addr_word,
			fg->sp[FG_SRAM_KI_COEFF_MED_DISCHG].addr_byte, &val,
			fg->sp[FG_SRAM_KI_COEFF_MED_DISCHG].len,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ki_coeff_med, rc=%d\n", rc);
		return rc;
	}

	fg_encode(fg->sp, FG_SRAM_KI_COEFF_HI_DISCHG, ki_coeff_hi, &val);
	rc = fg_sram_write(fg,
			fg->sp[FG_SRAM_KI_COEFF_HI_DISCHG].addr_word,
			fg->sp[FG_SRAM_KI_COEFF_HI_DISCHG].addr_byte, &val,
			fg->sp[FG_SRAM_KI_COEFF_HI_DISCHG].len,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ki_coeff_hi, rc=%d\n", rc);
		return rc;
	}

	fg_dbg(fg, FG_STATUS, "Wrote ki_coeff_med %d ki_coeff_hi %d\n",
		ki_coeff_med, ki_coeff_hi);
	return 0;
}

#define KI_COEFF_FULL_SOC_DEFAULT	733
static int fg_adjust_ki_coeff_full_soc(struct fg_dev *fg, int batt_temp)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc, ki_coeff_full_soc;
	u8 val;

	if (batt_temp < 0)
		ki_coeff_full_soc = 0;
	else if (fg->charge_status == POWER_SUPPLY_STATUS_DISCHARGING)
		ki_coeff_full_soc = chip->dt.ki_coeff_full_soc_dischg;
	else
		ki_coeff_full_soc = KI_COEFF_FULL_SOC_DEFAULT;

	if (chip->ki_coeff_full_soc == ki_coeff_full_soc)
		return 0;

	fg_encode(fg->sp, FG_SRAM_KI_COEFF_FULL_SOC, ki_coeff_full_soc, &val);
	rc = fg_sram_write(fg,
			fg->sp[FG_SRAM_KI_COEFF_FULL_SOC].addr_word,
			fg->sp[FG_SRAM_KI_COEFF_FULL_SOC].addr_byte, &val,
			fg->sp[FG_SRAM_KI_COEFF_FULL_SOC].len,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ki_coeff_full_soc, rc=%d\n", rc);
		return rc;
	}

	chip->ki_coeff_full_soc = ki_coeff_full_soc;
	fg_dbg(fg, FG_STATUS, "Wrote ki_coeff_full_soc %d\n",
		ki_coeff_full_soc);
	return 0;
}

static int fg_set_recharge_voltage(struct fg_dev *fg, int voltage_mv)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	u8 buf;
	int rc;

	if (chip->dt.auto_recharge_soc)
		return 0;

	/* This configuration is available only for pmicobalt v2.0 and above */
	if (fg->wa_flags & PMI8998_V1_REV_WA)
		return 0;

	if (voltage_mv == fg->last_recharge_volt_mv)
		return 0;

	fg_dbg(fg, FG_STATUS, "Setting recharge voltage to %dmV\n",
		voltage_mv);
	fg_encode(fg->sp, FG_SRAM_RECHARGE_VBATT_THR, voltage_mv, &buf);
	rc = fg_sram_write(fg,
			fg->sp[FG_SRAM_RECHARGE_VBATT_THR].addr_word,
			fg->sp[FG_SRAM_RECHARGE_VBATT_THR].addr_byte,
			&buf, fg->sp[FG_SRAM_RECHARGE_VBATT_THR].len,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing recharge_vbatt_thr, rc=%d\n",
			rc);
		return rc;
	}

	fg->last_recharge_volt_mv = voltage_mv;
	return 0;
}

static int fg_configure_full_soc(struct fg_dev *fg, int bsoc)
{
	int rc;
	u8 full_soc[2] = {0xFF, 0xFF};

	/*
	 * Once SOC masking condition is cleared, FULL_SOC and MONOTONIC_SOC
	 * needs to be updated to reflect the same. Write battery SOC to
	 * FULL_SOC and write a full value to MONOTONIC_SOC.
	 */
	rc = fg_sram_write(fg, FULL_SOC_WORD, FULL_SOC_OFFSET,
			(u8 *)&bsoc, 2, FG_IMA_ATOMIC);
	if (rc < 0) {
		pr_err("failed to write full_soc rc=%d\n", rc);
		return rc;
	}

	rc = fg_sram_write(fg, MONOTONIC_SOC_WORD, MONOTONIC_SOC_OFFSET,
			full_soc, 2, FG_IMA_ATOMIC);
	if (rc < 0) {
		pr_err("failed to write monotonic_soc rc=%d\n", rc);
		return rc;
	}

	return 0;
}

#define AUTO_RECHG_VOLT_LOW_LIMIT_MV	3700
static int fg_charge_full_update(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	union power_supply_propval prop = {0, };
	int rc, msoc, bsoc, recharge_soc, msoc_raw;

	if (!chip->dt.hold_soc_while_full)
		return 0;

	if (!batt_psy_initialized(fg))
		return 0;

	mutex_lock(&fg->charge_full_lock);
	vote(fg->delta_bsoc_irq_en_votable, DELTA_BSOC_IRQ_VOTER,
		fg->charge_done, 0);
	rc = power_supply_get_property(fg->batt_psy, POWER_SUPPLY_PROP_HEALTH,
		&prop);
	if (rc < 0) {
		pr_err("Error in getting battery health, rc=%d\n", rc);
		goto out;
	}

	fg->health = prop.intval;
	recharge_soc = chip->dt.recharge_soc_thr;
	recharge_soc = DIV_ROUND_CLOSEST(recharge_soc * FULL_SOC_RAW,
				FULL_CAPACITY);
	rc = fg_get_sram_prop(fg, FG_SRAM_BATT_SOC, &bsoc);
	if (rc < 0) {
		pr_err("Error in getting BATT_SOC, rc=%d\n", rc);
		goto out;
	}

	/* We need 2 most significant bytes here */
	bsoc = (u32)bsoc >> 16;
	rc = fg_get_msoc_raw(fg, &msoc_raw);
	if (rc < 0) {
		pr_err("Error in getting msoc_raw, rc=%d\n", rc);
		goto out;
	}
	msoc = DIV_ROUND_CLOSEST(msoc_raw * FULL_CAPACITY, FULL_SOC_RAW);

	fg_dbg(fg, FG_STATUS, "msoc: %d bsoc: %x health: %d status: %d full: %d\n",
		msoc, bsoc, fg->health, fg->charge_status,
		fg->charge_full);
	if (fg->charge_done && !fg->charge_full) {
		if (msoc >= 99 && fg->health == POWER_SUPPLY_HEALTH_GOOD) {
			fg_dbg(fg, FG_STATUS, "Setting charge_full to true\n");
			fg->charge_full = true;
			/*
			 * Lower the recharge voltage so that VBAT_LT_RECHG
			 * signal will not be asserted soon.
			 */
			rc = fg_set_recharge_voltage(fg,
					AUTO_RECHG_VOLT_LOW_LIMIT_MV);
			if (rc < 0) {
				pr_err("Error in reducing recharge voltage, rc=%d\n",
					rc);
				goto out;
			}
		} else {
			fg_dbg(fg, FG_STATUS, "Terminated charging @ SOC%d\n",
				msoc);
		}
	} else if ((msoc_raw <= recharge_soc || !fg->charge_done)
			&& fg->charge_full) {
		if (chip->dt.linearize_soc) {
			fg->delta_soc = FULL_CAPACITY - msoc;

			/*
			 * We're spreading out the delta SOC over every 10%
			 * change in monotonic SOC. We cannot spread more than
			 * 9% in the range of 0-100 skipping the first 10%.
			 */
			if (fg->delta_soc > 9) {
				fg->delta_soc = 0;
				fg->maint_soc = 0;
			} else {
				fg->maint_soc = FULL_CAPACITY;
				fg->last_msoc = msoc;
			}
		}

		/*
		 * Raise the recharge voltage so that VBAT_LT_RECHG signal
		 * will be asserted soon as battery SOC had dropped below
		 * the recharge SOC threshold.
		 */
		rc = fg_set_recharge_voltage(fg,
					chip->dt.recharge_volt_thr_mv);
		if (rc < 0) {
			pr_err("Error in setting recharge voltage, rc=%d\n",
				rc);
			goto out;
		}

		/*
		 * If charge_done is still set, wait for recharging or
		 * discharging to happen.
		 */
		if (fg->charge_done)
			goto out;

		rc = fg_configure_full_soc(fg, bsoc);
		if (rc < 0)
			goto out;

		fg->charge_full = false;
		fg_dbg(fg, FG_STATUS, "msoc_raw = %d bsoc: %d recharge_soc: %d delta_soc: %d\n",
			msoc_raw, bsoc >> 8, recharge_soc, fg->delta_soc);
	}

out:
	mutex_unlock(&fg->charge_full_lock);
	return rc;
}

#define RCONN_CONFIG_BIT	BIT(0)
static int fg_rconn_config(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc, esr_uohms;
	u64 scaling_factor;
	u32 val = 0;

	if (!chip->dt.rconn_mohms)
		return 0;

	rc = fg_sram_read(fg, PROFILE_INTEGRITY_WORD,
			SW_CONFIG_OFFSET, (u8 *)&val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading SW_CONFIG_OFFSET, rc=%d\n", rc);
		return rc;
	}

	if (val & RCONN_CONFIG_BIT) {
		fg_dbg(fg, FG_STATUS, "Rconn already configured: %x\n", val);
		return 0;
	}

	rc = fg_get_sram_prop(fg, FG_SRAM_ESR, &esr_uohms);
	if (rc < 0) {
		pr_err("failed to get ESR, rc=%d\n", rc);
		return rc;
	}

	scaling_factor = div64_u64((u64)esr_uohms * 1000,
				esr_uohms + (chip->dt.rconn_mohms * 1000));

	rc = fg_sram_read(fg, ESR_RSLOW_CHG_WORD,
			ESR_RSLOW_CHG_OFFSET, (u8 *)&val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading ESR_RSLOW_CHG_OFFSET, rc=%d\n", rc);
		return rc;
	}

	val *= scaling_factor;
	val = div64_u64(val, 1000);
	rc = fg_sram_write(fg, ESR_RSLOW_CHG_WORD,
			ESR_RSLOW_CHG_OFFSET, (u8 *)&val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR_RSLOW_CHG_OFFSET, rc=%d\n", rc);
		return rc;
	}
	fg_dbg(fg, FG_STATUS, "esr_rslow_chg modified to %x\n", val & 0xFF);

	rc = fg_sram_read(fg, ESR_RSLOW_DISCHG_WORD,
			ESR_RSLOW_DISCHG_OFFSET, (u8 *)&val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading ESR_RSLOW_DISCHG_OFFSET, rc=%d\n", rc);
		return rc;
	}

	val *= scaling_factor;
	val = div64_u64(val, 1000);
	rc = fg_sram_write(fg, ESR_RSLOW_DISCHG_WORD,
			ESR_RSLOW_DISCHG_OFFSET, (u8 *)&val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR_RSLOW_DISCHG_OFFSET, rc=%d\n", rc);
		return rc;
	}
	fg_dbg(fg, FG_STATUS, "esr_rslow_dischg modified to %x\n",
		val & 0xFF);

	val = RCONN_CONFIG_BIT;
	rc = fg_sram_write(fg, PROFILE_INTEGRITY_WORD,
			SW_CONFIG_OFFSET, (u8 *)&val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing SW_CONFIG_OFFSET, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int fg_set_jeita_threshold(struct fg_dev *fg,
				enum jeita_levels level, int temp_decidegC)
{
	int rc;
	u8 val;
	u16 reg;

	if (temp_decidegC < -300 || temp_decidegC > 970)
		return -EINVAL;

	/* Resolution is 0.5C. Base is -30C. */
	val = DIV_ROUND_CLOSEST(((temp_decidegC / 10) + 30) * 10, 5);
	switch (level) {
	case JEITA_COLD:
		reg = BATT_INFO_JEITA_TOO_COLD(fg);
		break;
	case JEITA_COOL:
		reg = BATT_INFO_JEITA_COLD(fg);
		break;
	case JEITA_WARM:
		reg = BATT_INFO_JEITA_HOT(fg);
		break;
	case JEITA_HOT:
		reg = BATT_INFO_JEITA_TOO_HOT(fg);
		break;
	default:
		return -EINVAL;
	}

	rc = fg_write(fg, reg, &val, 1);
	if (rc < 0) {
		pr_err("Error in setting jeita level %d, rc=%d\n", level, rc);
		return rc;
	}

	return 0;
}

static int fg_set_recharge_soc(struct fg_dev *fg, int recharge_soc)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	u8 buf;
	int rc;

	if (!chip->dt.auto_recharge_soc)
		return 0;

	if (recharge_soc < 0 || recharge_soc > FULL_CAPACITY)
		return 0;

	fg_encode(fg->sp, FG_SRAM_RECHARGE_SOC_THR, recharge_soc, &buf);
	rc = fg_sram_write(fg,
			fg->sp[FG_SRAM_RECHARGE_SOC_THR].addr_word,
			fg->sp[FG_SRAM_RECHARGE_SOC_THR].addr_byte, &buf,
			fg->sp[FG_SRAM_RECHARGE_SOC_THR].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing recharge_soc_thr, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int fg_adjust_recharge_soc(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	union power_supply_propval prop = {0, };
	int rc, msoc, recharge_soc, new_recharge_soc = 0;
	bool recharge_soc_status;

	if (!chip->dt.auto_recharge_soc)
		return 0;

	rc = power_supply_get_property(fg->batt_psy, POWER_SUPPLY_PROP_HEALTH,
		&prop);
	if (rc < 0) {
		pr_err("Error in getting battery health, rc=%d\n", rc);
		return rc;
	}
	fg->health = prop.intval;

	recharge_soc = chip->dt.recharge_soc_thr;
	recharge_soc_status = fg->recharge_soc_adjusted;
	/*
	 * If the input is present and charging had been terminated, adjust
	 * the recharge SOC threshold based on the monotonic SOC at which
	 * the charge termination had happened.
	 */
	if (is_input_present(fg)) {
		if (fg->charge_done) {
			if (!fg->recharge_soc_adjusted) {
				/* Get raw monotonic SOC for calculation */
				rc = fg_get_msoc(fg, &msoc);
				if (rc < 0) {
					pr_err("Error in getting msoc, rc=%d\n",
						rc);
					return rc;
				}

				/* Adjust the recharge_soc threshold */
				new_recharge_soc = msoc - (FULL_CAPACITY -
								recharge_soc);
				fg->recharge_soc_adjusted = true;
			} else {
				/* adjusted already, do nothing */
				if (fg->health != POWER_SUPPLY_HEALTH_GOOD)
					return 0;

				/*
				 * Device is out of JEITA so restore the
				 * default value
				 */
				new_recharge_soc = recharge_soc;
				fg->recharge_soc_adjusted = false;
			}
		} else {
			if (!fg->recharge_soc_adjusted)
				return 0;

			if (fg->health != POWER_SUPPLY_HEALTH_GOOD)
				return 0;

			/* Restore the default value */
			new_recharge_soc = recharge_soc;
			fg->recharge_soc_adjusted = false;
		}
	} else {
		/* Restore the default value */
		new_recharge_soc = recharge_soc;
		fg->recharge_soc_adjusted = false;
	}

	rc = fg_set_recharge_soc(fg, new_recharge_soc);
	if (rc < 0) {
		fg->recharge_soc_adjusted = recharge_soc_status;
		pr_err("Couldn't set resume SOC for FG, rc=%d\n", rc);
		return rc;
	}

	fg_dbg(fg, FG_STATUS, "resume soc set to %d\n", new_recharge_soc);
	return 0;
}

static int fg_adjust_recharge_voltage(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc, recharge_volt_mv;

	if (chip->dt.auto_recharge_soc)
		return 0;

	fg_dbg(fg, FG_STATUS, "health: %d chg_status: %d chg_done: %d\n",
		fg->health, fg->charge_status, fg->charge_done);

	recharge_volt_mv = chip->dt.recharge_volt_thr_mv;

	/* Lower the recharge voltage in soft JEITA */
	if (fg->health == POWER_SUPPLY_HEALTH_WARM ||
			fg->health == POWER_SUPPLY_HEALTH_COOL)
		recharge_volt_mv -= 200;

	rc = fg_set_recharge_voltage(fg, recharge_volt_mv);
	if (rc < 0) {
		pr_err("Error in setting recharge_voltage, rc=%d\n",
			rc);
		return rc;
	}

	return 0;
}

static int fg_slope_limit_config(struct fg_dev *fg, int batt_temp)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	enum slope_limit_status status;
	int rc;
	u8 buf;

	if (!chip->slope_limit_en)
		return 0;

	if (fg->charge_status == POWER_SUPPLY_STATUS_CHARGING ||
		fg->charge_status == POWER_SUPPLY_STATUS_FULL) {
		if (batt_temp < chip->dt.slope_limit_temp)
			status = LOW_TEMP_CHARGE;
		else
			status = HIGH_TEMP_CHARGE;
	} else {
		if (batt_temp < chip->dt.slope_limit_temp)
			status = LOW_TEMP_DISCHARGE;
		else
			status = HIGH_TEMP_DISCHARGE;
	}

	if (chip->slope_limit_sts == status)
		return 0;

	fg_encode(fg->sp, FG_SRAM_SLOPE_LIMIT,
		chip->dt.slope_limit_coeffs[status], &buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_SLOPE_LIMIT].addr_word,
			fg->sp[FG_SRAM_SLOPE_LIMIT].addr_byte, &buf,
			fg->sp[FG_SRAM_SLOPE_LIMIT].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in configuring slope_limit coefficient, rc=%d\n",
			rc);
		return rc;
	}

	chip->slope_limit_sts = status;
	fg_dbg(fg, FG_STATUS, "Slope limit status: %d value: %x\n", status,
		buf);
	return 0;
}

static int __fg_esr_filter_config(struct fg_dev *fg,
				enum esr_filter_status esr_flt_sts)
{
	u8 esr_tight_flt, esr_broad_flt;
	int esr_tight_flt_upct, esr_broad_flt_upct;
	int rc;
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);

	if (esr_flt_sts == fg->esr_flt_sts)
		return 0;

	if (esr_flt_sts == ROOM_TEMP) {
		esr_tight_flt_upct = chip->dt.esr_tight_flt_upct;
		esr_broad_flt_upct = chip->dt.esr_broad_flt_upct;
	} else if (esr_flt_sts == LOW_TEMP) {
		esr_tight_flt_upct = chip->dt.esr_tight_lt_flt_upct;
		esr_broad_flt_upct = chip->dt.esr_broad_lt_flt_upct;
	} else if (esr_flt_sts == RELAX_TEMP) {
		esr_tight_flt_upct = chip->dt.esr_tight_rt_flt_upct;
		esr_broad_flt_upct = chip->dt.esr_broad_rt_flt_upct;
	} else {
		pr_err("Unknown esr filter config\n");
		return 0;
	}

	fg_encode(fg->sp, FG_SRAM_ESR_TIGHT_FILTER, esr_tight_flt_upct,
		&esr_tight_flt);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_ESR_TIGHT_FILTER].addr_word,
			fg->sp[FG_SRAM_ESR_TIGHT_FILTER].addr_byte,
			&esr_tight_flt,
			fg->sp[FG_SRAM_ESR_TIGHT_FILTER].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR LT tight filter, rc=%d\n", rc);
		return rc;
	}

	fg_encode(fg->sp, FG_SRAM_ESR_BROAD_FILTER, esr_broad_flt_upct,
		&esr_broad_flt);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_ESR_BROAD_FILTER].addr_word,
			fg->sp[FG_SRAM_ESR_BROAD_FILTER].addr_byte,
			&esr_broad_flt,
			fg->sp[FG_SRAM_ESR_BROAD_FILTER].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR LT broad filter, rc=%d\n", rc);
		return rc;
	}

	fg->esr_flt_sts = esr_flt_sts;
	fg_dbg(fg, FG_STATUS, "applied ESR filter %d values\n", esr_flt_sts);
	return 0;
}

#define DT_IRQ_COUNT			3
#define DELTA_TEMP_IRQ_TIME_MS		300000
#define ESR_FILTER_ALARM_TIME_MS	900000
static int fg_esr_filter_config(struct fg_dev *fg, int batt_temp,
				bool override)
{
	enum esr_filter_status esr_flt_sts = ROOM_TEMP;
	bool qnovo_en, input_present, count_temp_irq = false;
	s64 time_ms;
	int rc;
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);

	/*
	 * If the battery temperature is lower than -20 C, then skip modifying
	 * ESR filter.
	 */
	if (batt_temp < -210)
		return 0;

	qnovo_en = is_qnovo_en(fg);
	input_present = is_input_present(fg);

	/*
	 * If Qnovo is enabled, after hitting a lower battery temperature of
	 * say 6 C, count the delta battery temperature interrupts for a
	 * certain period of time when the battery temperature increases.
	 * Switch to relaxed filter coefficients once the temperature increase
	 * is qualified so that ESR accuracy can be improved.
	 */
	if (qnovo_en && !override) {
		if (input_present) {
			if (fg->esr_flt_sts == RELAX_TEMP) {
				/* do nothing */
				return 0;
			}

			count_temp_irq =  true;
			if (fg->delta_temp_irq_count) {
				/* Don't count when temperature is dropping. */
				if (batt_temp <= fg->last_batt_temp)
					count_temp_irq = false;
			} else {
				/*
				 * Starting point for counting. Check if the
				 * temperature is qualified.
				 */
				if (batt_temp > chip->dt.esr_flt_rt_switch_temp)
					count_temp_irq = false;
				else
					fg->last_delta_temp_time =
						ktime_get();
			}
		} else {
			fg->delta_temp_irq_count = 0;
			rc = alarm_try_to_cancel(&fg->esr_filter_alarm);
			if (rc < 0)
				pr_err("Couldn't cancel esr_filter_alarm\n");
		}
	}

	/*
	 * If battery temperature is lesser than 10 C (default), then apply the
	 * ESR low temperature tight and broad filter values to ESR room
	 * temperature tight and broad filters. If battery temperature is higher
	 * than 10 C, then apply back the room temperature ESR filter
	 * coefficients to ESR room temperature tight and broad filters.
	 */
	if (batt_temp > chip->dt.esr_flt_switch_temp)
		esr_flt_sts = ROOM_TEMP;
	else
		esr_flt_sts = LOW_TEMP;

	if (count_temp_irq) {
		time_ms = ktime_ms_delta(ktime_get(),
				fg->last_delta_temp_time);
		fg->delta_temp_irq_count++;
		fg_dbg(fg, FG_STATUS, "dt_irq_count: %d\n",
			fg->delta_temp_irq_count);

		if (fg->delta_temp_irq_count >= DT_IRQ_COUNT
			&& time_ms <= DELTA_TEMP_IRQ_TIME_MS) {
			fg_dbg(fg, FG_STATUS, "%d interrupts in %lld ms\n",
				fg->delta_temp_irq_count, time_ms);
			esr_flt_sts = RELAX_TEMP;
		}
	}

	rc = __fg_esr_filter_config(fg, esr_flt_sts);
	if (rc < 0)
		return rc;

	if (esr_flt_sts == RELAX_TEMP)
		alarm_start_relative(&fg->esr_filter_alarm,
			ms_to_ktime(ESR_FILTER_ALARM_TIME_MS));

	return 0;
}

#define FG_ESR_FILTER_RESTART_MS	60000
static void esr_filter_work(struct work_struct *work)
{
	struct fg_dev *fg = container_of(work,
			struct fg_dev, status_change_work);
	int rc, batt_temp;

	rc = fg_get_battery_temp(fg, &batt_temp);
	if (rc < 0) {
		pr_err("Error in getting batt_temp\n");
		alarm_start_relative(&fg->esr_filter_alarm,
			ms_to_ktime(FG_ESR_FILTER_RESTART_MS));
		goto out;
	}

	rc = fg_esr_filter_config(fg, batt_temp, true);
	if (rc < 0) {
		pr_err("Error in configuring ESR filter rc:%d\n", rc);
		alarm_start_relative(&fg->esr_filter_alarm,
			ms_to_ktime(FG_ESR_FILTER_RESTART_MS));
	}

out:
	fg->delta_temp_irq_count = 0;
	pm_relax(fg->dev);
}

static enum alarmtimer_restart fg_esr_filter_alarm_cb(struct alarm *alarm,
							ktime_t now)
{
	struct fg_dev *fg = container_of(alarm,
			struct fg_dev, esr_filter_alarm);

	fg_dbg(fg, FG_STATUS, "ESR filter alarm triggered %lld\n",
		ktime_to_ms(now));
	/*
	 * We cannot vote for awake votable here as that takes a mutex lock
	 * and this is executed in an atomic context.
	 */
	pm_stay_awake(fg->dev);
	schedule_work(&fg->esr_filter_work);

	return ALARMTIMER_NORESTART;
}

static int fg_esr_fcc_config(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	union power_supply_propval prop = {0, };
	int rc;
	bool parallel_en = false, qnovo_en;

	if (is_parallel_charger_available(fg)) {
		rc = power_supply_get_property(fg->parallel_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &prop);
		if (rc < 0) {
			pr_err("Error in reading charging_enabled from parallel_psy, rc=%d\n",
				rc);
			return rc;
		}
		parallel_en = prop.intval;
	}

	qnovo_en = is_qnovo_en(fg);

	fg_dbg(fg, FG_POWER_SUPPLY, "chg_sts: %d par_en: %d qnov_en: %d esr_fcc_ctrl_en: %d\n",
		fg->charge_status, parallel_en, qnovo_en,
		chip->esr_fcc_ctrl_en);

	if (fg->charge_status == POWER_SUPPLY_STATUS_CHARGING &&
			(parallel_en || qnovo_en)) {
		if (chip->esr_fcc_ctrl_en)
			return 0;

		/*
		 * When parallel charging or Qnovo is enabled, configure ESR
		 * FCC to 300mA to trigger an ESR pulse. Without this, FG can
		 * request the main charger to increase FCC when it is supposed
		 * to decrease it.
		 */
		rc = fg_masked_write(fg, BATT_INFO_ESR_FAST_CRG_CFG(fg),
				ESR_FAST_CRG_IVAL_MASK |
				ESR_FAST_CRG_CTL_EN_BIT,
				ESR_FCC_300MA | ESR_FAST_CRG_CTL_EN_BIT);
		if (rc < 0) {
			pr_err("Error in writing to %04x, rc=%d\n",
				BATT_INFO_ESR_FAST_CRG_CFG(fg), rc);
			return rc;
		}

		chip->esr_fcc_ctrl_en = true;
	} else {
		if (!chip->esr_fcc_ctrl_en)
			return 0;

		/*
		 * If we're here, then it means either the device is not in
		 * charging state or parallel charging / Qnovo is disabled.
		 * Disable ESR fast charge current control in SW.
		 */
		rc = fg_masked_write(fg, BATT_INFO_ESR_FAST_CRG_CFG(fg),
				ESR_FAST_CRG_CTL_EN_BIT, 0);
		if (rc < 0) {
			pr_err("Error in writing to %04x, rc=%d\n",
				BATT_INFO_ESR_FAST_CRG_CFG(fg), rc);
			return rc;
		}

		chip->esr_fcc_ctrl_en = false;
	}

	fg_dbg(fg, FG_STATUS, "esr_fcc_ctrl_en set to %d\n",
		chip->esr_fcc_ctrl_en);
	return 0;
}

static int fg_esr_timer_config(struct fg_dev *fg, bool sleep)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc, cycles_init, cycles_max;
	bool end_of_charge = false;

	end_of_charge = is_input_present(fg) && fg->charge_done;
	fg_dbg(fg, FG_STATUS, "sleep: %d eoc: %d\n", sleep, end_of_charge);

	/* ESR discharging timer configuration */
	cycles_init = sleep ? chip->dt.esr_timer_asleep[TIMER_RETRY] :
			chip->dt.esr_timer_awake[TIMER_RETRY];
	if (end_of_charge)
		cycles_init = 0;

	cycles_max = sleep ? chip->dt.esr_timer_asleep[TIMER_MAX] :
			chip->dt.esr_timer_awake[TIMER_MAX];

	rc = fg_set_esr_timer(fg, cycles_init, cycles_max, false,
		sleep ? FG_IMA_NO_WLOCK : FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in setting ESR timer, rc=%d\n", rc);
		return rc;
	}

	/* ESR charging timer configuration */
	cycles_init = cycles_max = -EINVAL;
	if (end_of_charge || sleep) {
		cycles_init = chip->dt.esr_timer_charging[TIMER_RETRY];
		cycles_max = chip->dt.esr_timer_charging[TIMER_MAX];
	} else if (is_input_present(fg)) {
		cycles_init = chip->esr_timer_charging_default[TIMER_RETRY];
		cycles_max = chip->esr_timer_charging_default[TIMER_MAX];
	}

	rc = fg_set_esr_timer(fg, cycles_init, cycles_max, true,
		sleep ? FG_IMA_NO_WLOCK : FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in setting ESR timer, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static void fg_ttf_update(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc;
	int delay_ms;
	union power_supply_propval prop = {0, };
	int online = 0;

	if (usb_psy_initialized(fg)) {
		rc = power_supply_get_property(fg->usb_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop);
		if (rc < 0) {
			pr_err("Couldn't read usb ONLINE prop rc=%d\n", rc);
			return;
		}

		online = online || prop.intval;
	}

	if (pc_port_psy_initialized(fg)) {
		rc = power_supply_get_property(fg->pc_port_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop);
		if (rc < 0) {
			pr_err("Couldn't read pc_port ONLINE prop rc=%d\n", rc);
			return;
		}

		online = online || prop.intval;
	}

	if (dc_psy_initialized(fg)) {
		rc = power_supply_get_property(fg->dc_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop);
		if (rc < 0) {
			pr_err("Couldn't read dc ONLINE prop rc=%d\n", rc);
			return;
		}

		online = online || prop.intval;
	}


	if (fg->online_status == online)
		return;

	fg->online_status = online;
	if (online)
		/* wait 35 seconds for the input to settle */
		delay_ms = 35000;
	else
		/* wait 5 seconds for current to settle during discharge */
		delay_ms = 5000;

	vote(fg->awake_votable, TTF_PRIMING, true, 0);
	cancel_delayed_work_sync(&chip->ttf_work);
	mutex_lock(&chip->ttf.lock);
	fg_circ_buf_clr(&chip->ttf.ibatt);
	fg_circ_buf_clr(&chip->ttf.vbatt);
	chip->ttf.last_ttf = 0;
	chip->ttf.last_ms = 0;
	mutex_unlock(&chip->ttf.lock);
	schedule_delayed_work(&chip->ttf_work, msecs_to_jiffies(delay_ms));
}

static void restore_cycle_counter(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc = 0, i;
	u8 data[2];

	if (!chip->cyc_ctr.en)
		return;

	mutex_lock(&chip->cyc_ctr.lock);
	for (i = 0; i < BUCKET_COUNT; i++) {
		rc = fg_sram_read(fg, CYCLE_COUNT_WORD + (i / 2),
				CYCLE_COUNT_OFFSET + (i % 2) * 2, data, 2,
				FG_IMA_DEFAULT);
		if (rc < 0)
			pr_err("failed to read bucket %d rc=%d\n", i, rc);
		else
			chip->cyc_ctr.count[i] = data[0] | data[1] << 8;
	}
	mutex_unlock(&chip->cyc_ctr.lock);
}

static void clear_cycle_counter(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc = 0, i;

	if (!chip->cyc_ctr.en)
		return;

	mutex_lock(&chip->cyc_ctr.lock);
	memset(chip->cyc_ctr.count, 0, sizeof(chip->cyc_ctr.count));
	for (i = 0; i < BUCKET_COUNT; i++) {
		chip->cyc_ctr.started[i] = false;
		chip->cyc_ctr.last_soc[i] = 0;
	}
	rc = fg_sram_write(fg, CYCLE_COUNT_WORD, CYCLE_COUNT_OFFSET,
			(u8 *)&chip->cyc_ctr.count,
			sizeof(chip->cyc_ctr.count) / (sizeof(u8 *)),
			FG_IMA_DEFAULT);
	if (rc < 0)
		pr_err("failed to clear cycle counter rc=%d\n", rc);

	mutex_unlock(&chip->cyc_ctr.lock);
}

static int fg_inc_store_cycle_ctr(struct fg_dev *fg, int bucket)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc = 0;
	u16 cyc_count;
	u8 data[2];

	if (bucket < 0 || (bucket > BUCKET_COUNT - 1))
		return 0;

	cyc_count = chip->cyc_ctr.count[bucket];
	cyc_count++;
	data[0] = cyc_count & 0xFF;
	data[1] = cyc_count >> 8;

	rc = fg_sram_write(fg, CYCLE_COUNT_WORD + (bucket / 2),
			CYCLE_COUNT_OFFSET + (bucket % 2) * 2, data, 2,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to write BATT_CYCLE[%d] rc=%d\n",
			bucket, rc);
		return rc;
	}

	chip->cyc_ctr.count[bucket] = cyc_count;
	fg_dbg(fg, FG_STATUS, "Stored count %d in bucket %d\n", cyc_count,
		bucket);

	return rc;
}

static void fg_cycle_counter_update(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc = 0, bucket, i, batt_soc;

	if (!chip->cyc_ctr.en)
		return;

	mutex_lock(&chip->cyc_ctr.lock);
	rc = fg_get_sram_prop(fg, FG_SRAM_BATT_SOC, &batt_soc);
	if (rc < 0) {
		pr_err("Failed to read battery soc rc: %d\n", rc);
		goto out;
	}

	/* We need only the most significant byte here */
	batt_soc = (u32)batt_soc >> 24;

	/* Find out which bucket the SOC falls in */
	bucket = batt_soc / BUCKET_SOC_PCT;

	if (fg->charge_status == POWER_SUPPLY_STATUS_CHARGING) {
		if (!chip->cyc_ctr.started[bucket]) {
			chip->cyc_ctr.started[bucket] = true;
			chip->cyc_ctr.last_soc[bucket] = batt_soc;
		}
	} else if (fg->charge_done || !is_input_present(fg)) {
		for (i = 0; i < BUCKET_COUNT; i++) {
			if (chip->cyc_ctr.started[i] &&
				batt_soc > chip->cyc_ctr.last_soc[i] + 2) {
				rc = fg_inc_store_cycle_ctr(fg, i);
				if (rc < 0)
					pr_err("Error in storing cycle_ctr rc: %d\n",
						rc);
				chip->cyc_ctr.last_soc[i] = 0;
				chip->cyc_ctr.started[i] = false;
			}
		}
	}

	fg_dbg(fg, FG_STATUS, "batt_soc: %d bucket: %d chg_status: %d\n",
		batt_soc, bucket, fg->charge_status);
out:
	mutex_unlock(&chip->cyc_ctr.lock);
}

static int fg_get_cycle_count(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int i, len = 0;

	if (!chip->cyc_ctr.en)
		return 0;

	mutex_lock(&chip->cyc_ctr.lock);
	for (i = 0; i < BUCKET_COUNT; i++)
		len += chip->cyc_ctr.count[i];

	mutex_unlock(&chip->cyc_ctr.lock);

	len = len / BUCKET_COUNT;

	return len;
}

static const char *fg_get_cycle_counts(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int i, len = 0;
	char *buf;

	if (!chip->cyc_ctr.en)
		return NULL;

	buf = chip->cyc_ctr.counter;
	mutex_lock(&chip->cyc_ctr.lock);
	for (i = 0; i < BUCKET_COUNT; i++) {
		if (sizeof(chip->cyc_ctr.counter) - len < 8) {
			pr_err("Invalid length %d\n", len);
			mutex_unlock(&chip->cyc_ctr.lock);
			return NULL;
		}

		len += snprintf(buf+len, 8, "%d ", chip->cyc_ctr.count[i]);
	}
	mutex_unlock(&chip->cyc_ctr.lock);

	buf[len] = '\0';
	return buf;
}

#define ESR_SW_FCC_UA				100000	/* 100mA */
#define ESR_EXTRACTION_ENABLE_MASK		BIT(0)
static void fg_esr_sw_work(struct work_struct *work)
{
	struct fg_dev *fg = container_of(work,
			struct fg_dev, esr_sw_work);
	union power_supply_propval pval = {0, };
	int rc, esr_uohms = 0;

	vote(fg->awake_votable, FG_ESR_VOTER, true, 0);
	/*
	 * Enable ESR extraction just before we reduce the FCC
	 * to make sure that FG extracts the ESR. Disable ESR
	 * extraction after FCC reduction is complete to prevent
	 * any further HW pulses.
	 */
	rc = fg_sram_masked_write(fg, ESR_EXTRACTION_ENABLE_WORD,
			ESR_EXTRACTION_ENABLE_OFFSET,
			ESR_EXTRACTION_ENABLE_MASK, 0x1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Failed to enable ESR extraction rc=%d\n", rc);
		goto done;
	}

	/* delay for 1 FG cycle to complete */
	msleep(1500);

	/* for FCC to 100mA */
	pval.intval = ESR_SW_FCC_UA;
	rc = power_supply_set_property(fg->batt_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
			&pval);
	if (rc < 0) {
		pr_err("Failed to set FCC to 100mA rc=%d\n", rc);
		goto done;
	}

	/* delay for ESR readings */
	msleep(3000);

	/* FCC to 0 (removes vote) */
	pval.intval = 0;
	rc = power_supply_set_property(fg->batt_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
			&pval);
	if (rc < 0) {
		pr_err("Failed to remove FCC vote rc=%d\n", rc);
		goto done;
	}

	fg_get_sram_prop(fg, FG_SRAM_ESR, &esr_uohms);
	fg_dbg(fg, FG_STATUS, "SW ESR done ESR=%d\n", esr_uohms);

	/* restart the alarm timer */
	alarm_start_relative(&fg->esr_sw_timer,
		ms_to_ktime(fg->esr_wakeup_ms));
done:
	rc = fg_sram_masked_write(fg, ESR_EXTRACTION_ENABLE_WORD,
			ESR_EXTRACTION_ENABLE_OFFSET,
			ESR_EXTRACTION_ENABLE_MASK, 0x0, FG_IMA_DEFAULT);
	if (rc < 0)
		pr_err("Failed to disable ESR extraction rc=%d\n", rc);


	vote(fg->awake_votable, FG_ESR_VOTER, false, 0);
	fg_relax(fg, FG_SW_ESR_WAKE);
}

static enum alarmtimer_restart
	fg_esr_sw_timer(struct alarm *alarm, ktime_t now)
{
	struct fg_dev *fg = container_of(alarm,
			struct fg_dev, esr_sw_timer);

	if (!fg->usb_present)
		return ALARMTIMER_NORESTART;

	fg_stay_awake(fg, FG_SW_ESR_WAKE);
	schedule_work(&fg->esr_sw_work);

	return ALARMTIMER_NORESTART;
}

static int fg_config_esr_sw(struct fg_dev *fg)
{
	int rc;
	union power_supply_propval prop = {0, };
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);

	if (!chip->dt.use_esr_sw)
		return 0;

	if (!usb_psy_initialized(fg))
		return 0;

	rc = power_supply_get_property(fg->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &prop);
	if (rc < 0) {
		pr_err("Error in reading usb-status rc = %d\n", rc);
		return rc;
	}

	if (fg->usb_present != prop.intval) {
		fg->usb_present = prop.intval;
		fg_dbg(fg, FG_STATUS, "USB status changed=%d\n",
						fg->usb_present);
		/* cancel any pending work */
		alarm_cancel(&fg->esr_sw_timer);
		cancel_work_sync(&fg->esr_sw_work);

		if (fg->usb_present) {
			/* disable ESR extraction across the charging cycle */
			rc = fg_sram_masked_write(fg,
					ESR_EXTRACTION_ENABLE_WORD,
					ESR_EXTRACTION_ENABLE_OFFSET,
					ESR_EXTRACTION_ENABLE_MASK,
					0x0, FG_IMA_DEFAULT);
			if (rc < 0)
				return rc;
			/* wake up early for the first ESR on insertion */
			alarm_start_relative(&fg->esr_sw_timer,
				ms_to_ktime(fg->esr_wakeup_ms / 2));
		} else {
			/* enable ESR extraction on removal */
			rc = fg_sram_masked_write(fg,
					ESR_EXTRACTION_ENABLE_WORD,
					ESR_EXTRACTION_ENABLE_OFFSET,
					ESR_EXTRACTION_ENABLE_MASK,
					0x1, FG_IMA_DEFAULT);
			if (rc < 0)
				return rc;
		}
	}

	return 0;
}

static void status_change_work(struct work_struct *work)
{
	struct fg_dev *fg = container_of(work,
			struct fg_dev, status_change_work);
	union power_supply_propval prop = {0, };
	int rc, batt_temp;

	if (!batt_psy_initialized(fg)) {
		fg_dbg(fg, FG_STATUS, "Charger not available?!\n");
		goto out;
	}

	if (!fg->soc_reporting_ready) {
		fg_dbg(fg, FG_STATUS, "Profile load is not complete yet\n");
		goto out;
	}

	rc = power_supply_get_property(fg->batt_psy, POWER_SUPPLY_PROP_STATUS,
			&prop);
	rc = fg_config_esr_sw(fg);
	if (rc < 0)
		pr_err("Failed to config SW ESR rc=%d\n", rc);

	if (rc < 0) {
		pr_err("Error in getting charging status, rc=%d\n", rc);
		goto out;
	}

	fg->charge_status = prop.intval;
	rc = power_supply_get_property(fg->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &prop);
	if (rc < 0) {
		pr_err("Error in getting charge type, rc=%d\n", rc);
		goto out;
	}

	fg->charge_type = prop.intval;
	rc = power_supply_get_property(fg->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_DONE, &prop);
	if (rc < 0) {
		pr_err("Error in getting charge_done, rc=%d\n", rc);
		goto out;
	}

	fg->charge_done = prop.intval;
	fg_cycle_counter_update(fg);
	fg_cap_learning_update(fg);

	rc = fg_charge_full_update(fg);
	if (rc < 0)
		pr_err("Error in charge_full_update, rc=%d\n", rc);

	rc = fg_adjust_recharge_soc(fg);
	if (rc < 0)
		pr_err("Error in adjusting recharge_soc, rc=%d\n", rc);

	rc = fg_adjust_recharge_voltage(fg);
	if (rc < 0)
		pr_err("Error in adjusting recharge_voltage, rc=%d\n", rc);

	rc = fg_adjust_ki_coeff_dischg(fg);
	if (rc < 0)
		pr_err("Error in adjusting ki_coeff_dischg, rc=%d\n", rc);

	rc = fg_esr_fcc_config(fg);
	if (rc < 0)
		pr_err("Error in adjusting FCC for ESR, rc=%d\n", rc);

	rc = fg_get_battery_temp(fg, &batt_temp);
	if (!rc) {
		rc = fg_slope_limit_config(fg, batt_temp);
		if (rc < 0)
			pr_err("Error in configuring slope limiter rc:%d\n",
				rc);

		rc = fg_adjust_ki_coeff_full_soc(fg, batt_temp);
		if (rc < 0)
			pr_err("Error in configuring ki_coeff_full_soc rc:%d\n",
				rc);
	}

	fg_ttf_update(fg);
	fg->prev_charge_status = fg->charge_status;
out:
	fg_dbg(fg, FG_STATUS, "charge_status:%d charge_type:%d charge_done:%d\n",
		fg->charge_status, fg->charge_type, fg->charge_done);
	fg_relax(fg, FG_STATUS_NOTIFY_WAKE);
}

static int fg_bp_params_config(struct fg_dev *fg)
{
	int rc = 0;
	u8 buf;

	/* This SRAM register is only present in v2.0 and above */
	if (!(fg->wa_flags & PMI8998_V1_REV_WA) &&
					fg->bp.float_volt_uv > 0) {
		fg_encode(fg->sp, FG_SRAM_FLOAT_VOLT,
			fg->bp.float_volt_uv / 1000, &buf);
		rc = fg_sram_write(fg, fg->sp[FG_SRAM_FLOAT_VOLT].addr_word,
			fg->sp[FG_SRAM_FLOAT_VOLT].addr_byte, &buf,
			fg->sp[FG_SRAM_FLOAT_VOLT].len, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing float_volt, rc=%d\n", rc);
			return rc;
		}
	}

	if (fg->bp.vbatt_full_mv > 0) {
		rc = fg_set_constant_chg_voltage(fg,
				fg->bp.vbatt_full_mv * 1000);
		if (rc < 0)
			return rc;
	}

	return rc;
}

#define PROFILE_LOAD_BIT	BIT(0)
#define BOOTLOADER_LOAD_BIT	BIT(1)
#define BOOTLOADER_RESTART_BIT	BIT(2)
#define HLOS_RESTART_BIT	BIT(3)
static bool is_profile_load_required(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	u8 buf[PROFILE_COMP_LEN], val;
	bool profiles_same = false;
	int rc;

	rc = fg_sram_read(fg, PROFILE_INTEGRITY_WORD,
			PROFILE_INTEGRITY_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to read profile integrity rc=%d\n", rc);
		return false;
	}

	/* Check if integrity bit is set */
	if (val & PROFILE_LOAD_BIT) {
		fg_dbg(fg, FG_STATUS, "Battery profile integrity bit is set\n");

		/* Whitelist the values */
		val &= ~PROFILE_LOAD_BIT;
		if (val != HLOS_RESTART_BIT && val != BOOTLOADER_LOAD_BIT &&
			val != (BOOTLOADER_LOAD_BIT | BOOTLOADER_RESTART_BIT)) {
			val |= PROFILE_LOAD_BIT;
			pr_warn("Garbage value in profile integrity word: 0x%x\n",
				val);
			return true;
		}

		rc = fg_sram_read(fg, PROFILE_LOAD_WORD, PROFILE_LOAD_OFFSET,
				buf, PROFILE_COMP_LEN, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in reading battery profile, rc:%d\n", rc);
			fg->profile_load_status = PROFILE_SKIPPED;
			return false;
		}
		profiles_same = memcmp(chip->batt_profile, buf,
					PROFILE_COMP_LEN) == 0;
		if (profiles_same) {
			fg_dbg(fg, FG_STATUS, "Battery profile is same, not loading it\n");
			fg->profile_load_status = PROFILE_LOADED;
			return false;
		}

		if (!chip->dt.force_load_profile) {
			pr_warn("Profiles doesn't match, skipping loading it since force_load_profile is disabled\n");
			if (fg_profile_dump) {
				pr_info("FG: loaded profile:\n");
				dump_sram(fg, buf, PROFILE_LOAD_WORD,
					PROFILE_COMP_LEN);
				pr_info("FG: available profile:\n");
				dump_sram(fg, chip->batt_profile,
					PROFILE_LOAD_WORD, PROFILE_LEN);
			}
			fg->profile_load_status = PROFILE_SKIPPED;
			return false;
		}

		fg_dbg(fg, FG_STATUS, "Profiles are different, loading the correct one\n");
	} else {
		fg_dbg(fg, FG_STATUS, "Profile integrity bit is not set\n");
		if (fg_profile_dump) {
			pr_info("FG: profile to be loaded:\n");
			dump_sram(fg, chip->batt_profile, PROFILE_LOAD_WORD,
				PROFILE_LEN);
		}
	}
	return true;
}

static void fg_update_batt_profile(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc, offset;
	u8 val;

	rc = fg_sram_read(fg, PROFILE_INTEGRITY_WORD,
			SW_CONFIG_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading SW_CONFIG_OFFSET, rc=%d\n", rc);
		return;
	}

	/*
	 * If the RCONN had not been updated, no need to update battery
	 * profile. Else, update the battery profile so that the profile
	 * modified by bootloader or HLOS matches with the profile read
	 * from device tree.
	 */

	if (!(val & RCONN_CONFIG_BIT))
		return;

	rc = fg_sram_read(fg, ESR_RSLOW_CHG_WORD,
			ESR_RSLOW_CHG_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading ESR_RSLOW_CHG_OFFSET, rc=%d\n", rc);
		return;
	}
	offset = (ESR_RSLOW_CHG_WORD - PROFILE_LOAD_WORD) * 4
			+ ESR_RSLOW_CHG_OFFSET;
	chip->batt_profile[offset] = val;

	rc = fg_sram_read(fg, ESR_RSLOW_DISCHG_WORD,
			ESR_RSLOW_DISCHG_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading ESR_RSLOW_DISCHG_OFFSET, rc=%d\n", rc);
		return;
	}
	offset = (ESR_RSLOW_DISCHG_WORD - PROFILE_LOAD_WORD) * 4
			+ ESR_RSLOW_DISCHG_OFFSET;
	chip->batt_profile[offset] = val;
}

static void clear_battery_profile(struct fg_dev *fg)
{
	u8 val = 0;
	int rc;

	rc = fg_sram_write(fg, PROFILE_INTEGRITY_WORD,
			PROFILE_INTEGRITY_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0)
		pr_err("failed to write profile integrity rc=%d\n", rc);
}

static void pl_enable_work(struct work_struct *work)
{
	struct fg_gen3_chip *chip = container_of(work,
				struct fg_gen3_chip,
				pl_enable_work.work);
	struct fg_dev *fg = &chip->fg;

	vote(chip->pl_disable_votable, ESR_FCC_VOTER, false, 0);
	vote(fg->awake_votable, ESR_FCC_VOTER, false, 0);
}

#define SOC_READY_WAIT_TIME_MS	2000
static void profile_load_work(struct work_struct *work)
{
	struct fg_dev *fg = container_of(work,
				struct fg_dev,
				profile_load_work.work);
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	u8 buf[2], val;
	int rc;

	vote(fg->awake_votable, PROFILE_LOAD, true, 0);

	rc = fg_get_batt_id(fg);
	if (rc < 0) {
		pr_err("Error in getting battery id, rc:%d\n", rc);
		goto out;
	}

	rc = fg_get_batt_profile(fg);
	if (rc < 0) {
		fg->profile_load_status = PROFILE_MISSING;
		pr_warn("profile for batt_id=%dKOhms not found..using OTP, rc:%d\n",
			fg->batt_id_ohms / 1000, rc);
		goto out;
	}

	if (!fg->profile_available)
		goto out;

	fg_update_batt_profile(fg);

	if (!is_profile_load_required(fg))
		goto done;

	clear_cycle_counter(fg);
	mutex_lock(&chip->cl.lock);
	chip->cl.learned_cc_uah = 0;
	chip->cl.active = false;
	mutex_unlock(&chip->cl.lock);

	fg_dbg(fg, FG_STATUS, "profile loading started\n");
	rc = fg_masked_write(fg, BATT_SOC_RESTART(fg), RESTART_GO_BIT, 0);
	if (rc < 0) {
		pr_err("Error in writing to %04x, rc=%d\n",
			BATT_SOC_RESTART(fg), rc);
		goto out;
	}

	/* load battery profile */
	rc = fg_sram_write(fg, PROFILE_LOAD_WORD, PROFILE_LOAD_OFFSET,
			chip->batt_profile, PROFILE_LEN, FG_IMA_ATOMIC);
	if (rc < 0) {
		pr_err("Error in writing battery profile, rc:%d\n", rc);
		goto out;
	}

	/* Set the profile integrity bit */
	val = HLOS_RESTART_BIT | PROFILE_LOAD_BIT;
	rc = fg_sram_write(fg, PROFILE_INTEGRITY_WORD,
			PROFILE_INTEGRITY_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to write profile integrity rc=%d\n", rc);
		goto out;
	}

	rc = fg_restart(fg, SOC_READY_WAIT_TIME_MS);
	if (rc < 0) {
		pr_err("Error in restarting FG, rc=%d\n", rc);
		goto out;
	}

	fg_dbg(fg, FG_STATUS, "SOC is ready\n");
	fg->profile_load_status = PROFILE_LOADED;
done:
	rc = fg_bp_params_config(fg);
	if (rc < 0)
		pr_err("Error in configuring battery profile params, rc:%d\n",
			rc);

	rc = fg_sram_read(fg, NOM_CAP_WORD, NOM_CAP_OFFSET, buf, 2,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading %04x[%d] rc=%d\n", NOM_CAP_WORD,
			NOM_CAP_OFFSET, rc);
	} else {
		chip->cl.nom_cap_uah = (int)(buf[0] | buf[1] << 8) * 1000;
		rc = fg_load_learned_cap_from_sram(fg);
		if (rc < 0)
			pr_err("Error in loading capacity learning data, rc:%d\n",
				rc);
	}

	rc = fg_rconn_config(fg);
	if (rc < 0)
		pr_err("Error in configuring Rconn, rc=%d\n", rc);

	batt_psy_initialized(fg);
	fg_notify_charger(fg);

	fg_dbg(fg, FG_STATUS, "profile loaded successfully");
out:
	fg->soc_reporting_ready = true;
	vote(fg->awake_votable, ESR_FCC_VOTER, true, 0);
	schedule_delayed_work(&chip->pl_enable_work, msecs_to_jiffies(5000));
	vote(fg->awake_votable, PROFILE_LOAD, false, 0);
	if (!work_pending(&fg->status_change_work)) {
		fg_stay_awake(fg, FG_STATUS_NOTIFY_WAKE);
		schedule_work(&fg->status_change_work);
	}
}

static void sram_dump_work(struct work_struct *work)
{
	struct fg_dev *fg = container_of(work, struct fg_dev,
					    sram_dump_work.work);
	u8 buf[FG_SRAM_LEN];
	int rc;
	s64 timestamp_ms, quotient;
	s32 remainder;

	rc = fg_sram_read(fg, 0, 0, buf, FG_SRAM_LEN, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading FG SRAM, rc:%d\n", rc);
		goto resched;
	}

	timestamp_ms = ktime_to_ms(ktime_get_boottime());
	quotient = div_s64_rem(timestamp_ms, 1000, &remainder);
	fg_dbg(fg, FG_STATUS, "SRAM Dump Started at %lld.%d\n",
		quotient, remainder);
	dump_sram(fg, buf, 0, FG_SRAM_LEN);
	timestamp_ms = ktime_to_ms(ktime_get_boottime());
	quotient = div_s64_rem(timestamp_ms, 1000, &remainder);
	fg_dbg(fg, FG_STATUS, "SRAM Dump done at %lld.%d\n",
		quotient, remainder);
resched:
	schedule_delayed_work(&fg->sram_dump_work,
			msecs_to_jiffies(fg_sram_dump_period_ms));
}

static int fg_sram_dump_sysfs(const char *val, const struct kernel_param *kp)
{
	int rc;
	struct power_supply *bms_psy;
	struct fg_gen3_chip *chip;
	struct fg_dev *fg;
	bool old_val = fg_sram_dump;

	rc = param_set_bool(val, kp);
	if (rc) {
		pr_err("Unable to set fg_sram_dump: %d\n", rc);
		return rc;
	}

	if (fg_sram_dump == old_val)
		return 0;

	bms_psy = power_supply_get_by_name("bms");
	if (!bms_psy) {
		pr_err("bms psy not found\n");
		return -ENODEV;
	}

	chip = power_supply_get_drvdata(bms_psy);
	fg = &chip->fg;
	if (fg_sram_dump)
		schedule_delayed_work(&fg->sram_dump_work,
				msecs_to_jiffies(fg_sram_dump_period_ms));
	else
		cancel_delayed_work_sync(&fg->sram_dump_work);

	return 0;
}

static struct kernel_param_ops fg_sram_dump_ops = {
	.set = fg_sram_dump_sysfs,
	.get = param_get_bool,
};

module_param_cb(sram_dump_en, &fg_sram_dump_ops, &fg_sram_dump, 0644);

static int fg_restart_sysfs(const char *val, const struct kernel_param *kp)
{
	int rc;
	struct power_supply *bms_psy;
	struct fg_gen3_chip *chip;
	struct fg_dev *fg;

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("Unable to set fg_restart_mp: %d\n", rc);
		return rc;
	}

	if (fg_restart_mp != 1) {
		pr_err("Bad value %d\n", fg_restart_mp);
		return -EINVAL;
	}

	bms_psy = power_supply_get_by_name("bms");
	if (!bms_psy) {
		pr_err("bms psy not found\n");
		return 0;
	}

	chip = power_supply_get_drvdata(bms_psy);
	fg = &chip->fg;
	rc = fg_restart(fg, SOC_READY_WAIT_TIME_MS);
	if (rc < 0) {
		pr_err("Error in restarting FG, rc=%d\n", rc);
		return rc;
	}

	pr_info("FG restart done\n");
	return rc;
}

static struct kernel_param_ops fg_restart_ops = {
	.set = fg_restart_sysfs,
	.get = param_get_int,
};

module_param_cb(restart, &fg_restart_ops, &fg_restart_mp, 0644);

#define HOURS_TO_SECONDS	3600
#define OCV_SLOPE_UV		10869
#define MILLI_UNIT		1000
#define MICRO_UNIT		1000000
#define NANO_UNIT		1000000000
static int fg_get_time_to_full_locked(struct fg_dev *fg, int *val)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc, ibatt_avg, vbatt_avg, rbatt, msoc, full_soc, act_cap_mah,
		i_cc2cv = 0, soc_cc2cv, tau, divisor, iterm, ttf_mode,
		i, soc_per_step, msoc_this_step, msoc_next_step,
		ibatt_this_step, t_predicted_this_step, ttf_slope,
		t_predicted_cv, t_predicted = 0;
	s64 delta_ms;

	if (!fg->soc_reporting_ready)
		return -ENODATA;

	if (fg->bp.float_volt_uv <= 0) {
		pr_err("battery profile is not loaded\n");
		return -ENODATA;
	}

	if (!batt_psy_initialized(fg)) {
		fg_dbg(fg, FG_TTF, "charger is not available\n");
		return -ENODATA;
	}

	rc = fg_get_prop_capacity(fg, &msoc);
	if (rc < 0) {
		pr_err("failed to get msoc rc=%d\n", rc);
		return rc;
	}
	fg_dbg(fg, FG_TTF, "msoc=%d\n", msoc);

	/* the battery is considered full if the SOC is 100% */
	if (msoc >= 100) {
		*val = 0;
		return 0;
	}

	if (is_qnovo_en(fg))
		ttf_mode = FG_TTF_MODE_QNOVO;
	else
		ttf_mode = FG_TTF_MODE_NORMAL;

	/* when switching TTF algorithms the TTF needs to be reset */
	if (chip->ttf.mode != ttf_mode) {
		fg_circ_buf_clr(&chip->ttf.ibatt);
		fg_circ_buf_clr(&chip->ttf.vbatt);
		chip->ttf.last_ttf = 0;
		chip->ttf.last_ms = 0;
		chip->ttf.mode = ttf_mode;
	}

	/* at least 10 samples are required to produce a stable IBATT */
	if (chip->ttf.ibatt.size < 10) {
		*val = -1;
		return 0;
	}

	rc = fg_circ_buf_median(&chip->ttf.ibatt, &ibatt_avg);
	if (rc < 0) {
		pr_err("failed to get IBATT AVG rc=%d\n", rc);
		return rc;
	}

	rc = fg_circ_buf_median(&chip->ttf.vbatt, &vbatt_avg);
	if (rc < 0) {
		pr_err("failed to get VBATT AVG rc=%d\n", rc);
		return rc;
	}

	ibatt_avg = -ibatt_avg / MILLI_UNIT;
	vbatt_avg /= MILLI_UNIT;

	/* clamp ibatt_avg to iterm */
	if (ibatt_avg < abs(chip->dt.sys_term_curr_ma))
		ibatt_avg = abs(chip->dt.sys_term_curr_ma);

	fg_dbg(fg, FG_TTF, "ibatt_avg=%d\n", ibatt_avg);
	fg_dbg(fg, FG_TTF, "vbatt_avg=%d\n", vbatt_avg);

	rc = fg_get_battery_resistance(fg, &rbatt);
	if (rc < 0) {
		pr_err("failed to get battery resistance rc=%d\n", rc);
		return rc;
	}

	rbatt /= MILLI_UNIT;
	fg_dbg(fg, FG_TTF, "rbatt=%d\n", rbatt);

	rc = fg_get_sram_prop(fg, FG_SRAM_ACT_BATT_CAP, &act_cap_mah);
	if (rc < 0) {
		pr_err("failed to get ACT_BATT_CAP rc=%d\n", rc);
		return rc;
	}

	rc = fg_get_sram_prop(fg, FG_SRAM_FULL_SOC, &full_soc);
	if (rc < 0) {
		pr_err("failed to get full soc rc=%d\n", rc);
		return rc;
	}
	full_soc = DIV_ROUND_CLOSEST(((u16)full_soc >> 8) * FULL_CAPACITY,
								FULL_SOC_RAW);
	act_cap_mah = full_soc * act_cap_mah / 100;
	fg_dbg(fg, FG_TTF, "act_cap_mah=%d\n", act_cap_mah);

	/* estimated battery current at the CC to CV transition */
	switch (chip->ttf.mode) {
	case FG_TTF_MODE_NORMAL:
		i_cc2cv = ibatt_avg * vbatt_avg /
			max(MILLI_UNIT, fg->bp.float_volt_uv / MILLI_UNIT);
		break;
	case FG_TTF_MODE_QNOVO:
		i_cc2cv = min(
			chip->ttf.cc_step.arr[MAX_CC_STEPS - 1] / MILLI_UNIT,
			ibatt_avg * vbatt_avg /
			max(MILLI_UNIT, fg->bp.float_volt_uv / MILLI_UNIT));
		break;
	default:
		pr_err("TTF mode %d is not supported\n", chip->ttf.mode);
		break;
	}
	fg_dbg(fg, FG_TTF, "i_cc2cv=%d\n", i_cc2cv);

	/* if we are already in CV state then we can skip estimating CC */
	if (fg->charge_type == POWER_SUPPLY_CHARGE_TYPE_TAPER)
		goto cv_estimate;

	/* estimated SOC at the CC to CV transition */
	soc_cc2cv = DIV_ROUND_CLOSEST(rbatt * i_cc2cv, OCV_SLOPE_UV);
	soc_cc2cv = 100 - soc_cc2cv;
	fg_dbg(fg, FG_TTF, "soc_cc2cv=%d\n", soc_cc2cv);

	switch (chip->ttf.mode) {
	case FG_TTF_MODE_NORMAL:
		if (soc_cc2cv - msoc <= 0)
			goto cv_estimate;

		divisor = max(100, (ibatt_avg + i_cc2cv) / 2 * 100);
		t_predicted = div_s64((s64)act_cap_mah * (soc_cc2cv - msoc) *
						HOURS_TO_SECONDS, divisor);
		break;
	case FG_TTF_MODE_QNOVO:
		soc_per_step = 100 / MAX_CC_STEPS;
		for (i = msoc / soc_per_step; i < MAX_CC_STEPS - 1; ++i) {
			msoc_next_step = (i + 1) * soc_per_step;
			if (i == msoc / soc_per_step)
				msoc_this_step = msoc;
			else
				msoc_this_step = i * soc_per_step;

			/* scale ibatt by 85% to account for discharge pulses */
			ibatt_this_step = min(
					chip->ttf.cc_step.arr[i] / MILLI_UNIT,
					ibatt_avg) * 85 / 100;
			divisor = max(100, ibatt_this_step * 100);
			t_predicted_this_step = div_s64((s64)act_cap_mah *
					(msoc_next_step - msoc_this_step) *
					HOURS_TO_SECONDS, divisor);
			t_predicted += t_predicted_this_step;
			fg_dbg(fg, FG_TTF, "[%d, %d] ma=%d t=%d\n",
				msoc_this_step, msoc_next_step,
				ibatt_this_step, t_predicted_this_step);
		}
		break;
	default:
		pr_err("TTF mode %d is not supported\n", chip->ttf.mode);
		break;
	}

cv_estimate:
	fg_dbg(fg, FG_TTF, "t_predicted_cc=%d\n", t_predicted);

	iterm = max(100, abs(chip->dt.sys_term_curr_ma) + 200);
	fg_dbg(fg, FG_TTF, "iterm=%d\n", iterm);

	if (fg->charge_type == POWER_SUPPLY_CHARGE_TYPE_TAPER)
		tau = max(MILLI_UNIT, ibatt_avg * MILLI_UNIT / iterm);
	else
		tau = max(MILLI_UNIT, i_cc2cv * MILLI_UNIT / iterm);

	rc = fg_lerp(fg_ln_table, ARRAY_SIZE(fg_ln_table), tau, &tau);
	if (rc < 0) {
		pr_err("failed to interpolate tau rc=%d\n", rc);
		return rc;
	}

	/* tau is scaled linearly from 95% to 100% SOC */
	if (msoc >= 95)
		tau = tau * 2 * (100 - msoc) / 10;

	fg_dbg(fg, FG_TTF, "tau=%d\n", tau);
	t_predicted_cv = div_s64((s64)act_cap_mah * rbatt * tau *
						HOURS_TO_SECONDS, NANO_UNIT);
	fg_dbg(fg, FG_TTF, "t_predicted_cv=%d\n", t_predicted_cv);
	t_predicted += t_predicted_cv;

	fg_dbg(fg, FG_TTF, "t_predicted_prefilter=%d\n", t_predicted);
	if (chip->ttf.last_ms != 0) {
		delta_ms = ktime_ms_delta(ktime_get_boottime(),
					  ms_to_ktime(chip->ttf.last_ms));
		if (delta_ms > 10000) {
			ttf_slope = div64_s64(
				(s64)(t_predicted - chip->ttf.last_ttf) *
				MICRO_UNIT, delta_ms);
			if (ttf_slope > -100)
				ttf_slope = -100;
			else if (ttf_slope < -2000)
				ttf_slope = -2000;

			t_predicted = div_s64(
				(s64)ttf_slope * delta_ms, MICRO_UNIT) +
				chip->ttf.last_ttf;
			fg_dbg(fg, FG_TTF, "ttf_slope=%d\n", ttf_slope);
		} else {
			t_predicted = chip->ttf.last_ttf;
		}
	}

	/* clamp the ttf to 0 */
	if (t_predicted < 0)
		t_predicted = 0;

	fg_dbg(fg, FG_TTF, "t_predicted_postfilter=%d\n", t_predicted);
	*val = t_predicted;
	return 0;
}

static int fg_get_time_to_full(struct fg_dev *fg, int *val)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc;

	mutex_lock(&chip->ttf.lock);
	rc = fg_get_time_to_full_locked(fg, val);
	mutex_unlock(&chip->ttf.lock);
	return rc;
}

#define CENTI_ICORRECT_C0	105
#define CENTI_ICORRECT_C1	20
static int fg_get_time_to_empty(struct fg_dev *fg, int *val)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc, ibatt_avg, msoc, full_soc, act_cap_mah, divisor;

	mutex_lock(&chip->ttf.lock);
	rc = fg_circ_buf_median(&chip->ttf.ibatt, &ibatt_avg);
	if (rc < 0) {
		/* try to get instantaneous current */
		rc = fg_get_battery_current(fg, &ibatt_avg);
		if (rc < 0) {
			pr_err("failed to get battery current, rc=%d\n", rc);
			mutex_unlock(&chip->ttf.lock);
			return rc;
		}
	}
	mutex_unlock(&chip->ttf.lock);

	ibatt_avg /= MILLI_UNIT;
	/* clamp ibatt_avg to 100mA */
	if (ibatt_avg < 100)
		ibatt_avg = 100;

	rc = fg_get_prop_capacity(fg, &msoc);
	if (rc < 0) {
		pr_err("Error in getting capacity, rc=%d\n", rc);
		return rc;
	}

	rc = fg_get_sram_prop(fg, FG_SRAM_ACT_BATT_CAP, &act_cap_mah);
	if (rc < 0) {
		pr_err("Error in getting ACT_BATT_CAP, rc=%d\n", rc);
		return rc;
	}

	rc = fg_get_sram_prop(fg, FG_SRAM_FULL_SOC, &full_soc);
	if (rc < 0) {
		pr_err("failed to get full soc rc=%d\n", rc);
		return rc;
	}
	full_soc = DIV_ROUND_CLOSEST(((u16)full_soc >> 8) * FULL_CAPACITY,
								FULL_SOC_RAW);
	act_cap_mah = full_soc * act_cap_mah / 100;

	divisor = CENTI_ICORRECT_C0 * 100 + CENTI_ICORRECT_C1 * msoc;
	divisor = ibatt_avg * divisor / 100;
	divisor = max(100, divisor);
	*val = act_cap_mah * msoc * HOURS_TO_SECONDS / divisor;
	return 0;
}

static int fg_update_maint_soc(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc = 0, msoc;

	if (!chip->dt.linearize_soc)
		return 0;

	mutex_lock(&fg->charge_full_lock);
	if (fg->delta_soc <= 0)
		goto out;

	rc = fg_get_msoc(fg, &msoc);
	if (rc < 0) {
		pr_err("Error in getting msoc, rc=%d\n", rc);
		goto out;
	}

	if (msoc > fg->maint_soc) {
		/*
		 * When the monotonic SOC goes above maintenance SOC, we should
		 * stop showing the maintenance SOC.
		 */
		fg->delta_soc = 0;
		fg->maint_soc = 0;
	} else if (msoc <= fg->last_msoc) {
		/* MSOC is decreasing. Decrease maintenance SOC as well */
		fg->maint_soc -= 1;
		if (!(msoc % 10)) {
			/*
			 * Reduce the maintenance SOC additionally by 1 whenever
			 * it crosses a SOC multiple of 10.
			 */
			fg->maint_soc -= 1;
			fg->delta_soc -= 1;
		}
	}

	fg_dbg(fg, FG_IRQ, "msoc: %d last_msoc: %d maint_soc: %d delta_soc: %d\n",
		msoc, fg->last_msoc, fg->maint_soc, fg->delta_soc);
	fg->last_msoc = msoc;
out:
	mutex_unlock(&fg->charge_full_lock);
	return rc;
}

static int fg_esr_validate(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc, esr_uohms;
	u8 buf[2];

	if (chip->dt.esr_clamp_mohms <= 0)
		return 0;

	rc = fg_get_sram_prop(fg, FG_SRAM_ESR, &esr_uohms);
	if (rc < 0) {
		pr_err("failed to get ESR, rc=%d\n", rc);
		return rc;
	}

	if (esr_uohms >= chip->dt.esr_clamp_mohms * 1000) {
		pr_debug("ESR %d is > ESR_clamp\n", esr_uohms);
		return 0;
	}

	esr_uohms = chip->dt.esr_clamp_mohms * 1000;
	fg_encode(fg->sp, FG_SRAM_ESR, esr_uohms, buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_ESR].addr_word,
			fg->sp[FG_SRAM_ESR].addr_byte, buf,
			fg->sp[FG_SRAM_ESR].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR, rc=%d\n", rc);
		return rc;
	}

	fg_dbg(fg, FG_STATUS, "ESR clamped to %duOhms\n", esr_uohms);
	return 0;
}

static int fg_force_esr_meas(struct fg_dev *fg)
{
	int rc;
	int esr_uohms;

	mutex_lock(&fg->qnovo_esr_ctrl_lock);
	/* force esr extraction enable */
	rc = fg_sram_masked_write(fg, ESR_EXTRACTION_ENABLE_WORD,
			ESR_EXTRACTION_ENABLE_OFFSET, BIT(0), BIT(0),
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to enable esr extn rc=%d\n", rc);
		goto out;
	}

	rc = fg_masked_write(fg, BATT_INFO_QNOVO_CFG(fg),
			LD_REG_CTRL_BIT, 0);
	if (rc < 0) {
		pr_err("Error in configuring qnovo_cfg rc=%d\n", rc);
		goto out;
	}

	rc = fg_masked_write(fg, BATT_INFO_TM_MISC1(fg),
			ESR_REQ_CTL_BIT | ESR_REQ_CTL_EN_BIT,
			ESR_REQ_CTL_BIT | ESR_REQ_CTL_EN_BIT);
	if (rc < 0) {
		pr_err("Error in configuring force ESR rc=%d\n", rc);
		goto out;
	}

	/*
	 * Release and grab the lock again after 1.5 seconds so that prepare
	 * callback can succeed if the request comes in between.
	 */
	mutex_unlock(&fg->qnovo_esr_ctrl_lock);

	/* wait 1.5 seconds for hw to measure ESR */
	msleep(1500);

	mutex_lock(&fg->qnovo_esr_ctrl_lock);
	rc = fg_masked_write(fg, BATT_INFO_TM_MISC1(fg),
			ESR_REQ_CTL_BIT | ESR_REQ_CTL_EN_BIT,
			0);
	if (rc < 0) {
		pr_err("Error in restoring force ESR rc=%d\n", rc);
		goto out;
	}

	/* If qnovo is disabled, then leave ESR extraction enabled */
	if (!fg->qnovo_enable)
		goto done;

	rc = fg_masked_write(fg, BATT_INFO_QNOVO_CFG(fg),
			LD_REG_CTRL_BIT, LD_REG_CTRL_BIT);
	if (rc < 0) {
		pr_err("Error in restoring qnovo_cfg rc=%d\n", rc);
		goto out;
	}

	/* force esr extraction disable */
	rc = fg_sram_masked_write(fg, ESR_EXTRACTION_ENABLE_WORD,
			ESR_EXTRACTION_ENABLE_OFFSET, BIT(0), 0,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to disable esr extn rc=%d\n", rc);
		goto out;
	}

done:
	fg_get_battery_resistance(fg, &esr_uohms);
	fg_dbg(fg, FG_STATUS, "ESR uohms = %d\n", esr_uohms);
out:
	mutex_unlock(&fg->qnovo_esr_ctrl_lock);
	return rc;
}

static int fg_prepare_for_qnovo(struct fg_dev *fg, int qnovo_enable)
{
	int rc = 0;

	mutex_lock(&fg->qnovo_esr_ctrl_lock);
	/* force esr extraction disable when qnovo enables */
	rc = fg_sram_masked_write(fg, ESR_EXTRACTION_ENABLE_WORD,
			ESR_EXTRACTION_ENABLE_OFFSET,
			BIT(0), qnovo_enable ? 0 : BIT(0),
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in configuring esr extraction rc=%d\n", rc);
		goto out;
	}

	rc = fg_masked_write(fg, BATT_INFO_QNOVO_CFG(fg),
			LD_REG_CTRL_BIT,
			qnovo_enable ? LD_REG_CTRL_BIT : 0);
	if (rc < 0) {
		pr_err("Error in configuring qnovo_cfg rc=%d\n", rc);
		goto out;
	}

	fg_dbg(fg, FG_STATUS, "%s for Qnovo\n",
		qnovo_enable ? "Prepared" : "Unprepared");
	fg->qnovo_enable = qnovo_enable;
out:
	mutex_unlock(&fg->qnovo_esr_ctrl_lock);
	return rc;
}

static void ttf_work(struct work_struct *work)
{
	struct fg_gen3_chip *chip = container_of(work,
				struct fg_gen3_chip, ttf_work.work);
	struct fg_dev *fg = &chip->fg;
	int rc, ibatt_now, vbatt_now, ttf;
	ktime_t ktime_now;

	mutex_lock(&chip->ttf.lock);
	if (fg->charge_status != POWER_SUPPLY_STATUS_CHARGING &&
			fg->charge_status != POWER_SUPPLY_STATUS_DISCHARGING)
		goto end_work;

	rc = fg_get_battery_current(fg, &ibatt_now);
	if (rc < 0) {
		pr_err("failed to get battery current, rc=%d\n", rc);
		goto end_work;
	}

	rc = fg_get_battery_voltage(fg, &vbatt_now);
	if (rc < 0) {
		pr_err("failed to get battery voltage, rc=%d\n", rc);
		goto end_work;
	}

	fg_circ_buf_add(&chip->ttf.ibatt, ibatt_now);
	fg_circ_buf_add(&chip->ttf.vbatt, vbatt_now);

	if (fg->charge_status == POWER_SUPPLY_STATUS_CHARGING) {
		rc = fg_get_time_to_full_locked(fg, &ttf);
		if (rc < 0) {
			pr_err("failed to get ttf, rc=%d\n", rc);
			goto end_work;
		}

		/* keep the wake lock and prime the IBATT and VBATT buffers */
		if (ttf < 0) {
			/* delay for one FG cycle */
			schedule_delayed_work(&chip->ttf_work,
							msecs_to_jiffies(1500));
			mutex_unlock(&chip->ttf.lock);
			return;
		}

		/* update the TTF reference point every minute */
		ktime_now = ktime_get_boottime();
		if (ktime_ms_delta(ktime_now,
				   ms_to_ktime(chip->ttf.last_ms)) > 60000 ||
				   chip->ttf.last_ms == 0) {
			chip->ttf.last_ttf = ttf;
			chip->ttf.last_ms = ktime_to_ms(ktime_now);
		}
	}

	/* recurse every 10 seconds */
	schedule_delayed_work(&chip->ttf_work, msecs_to_jiffies(10000));
end_work:
	vote(fg->awake_votable, TTF_PRIMING, false, 0);
	mutex_unlock(&chip->ttf.lock);
}

/* PSY CALLBACKS STAY HERE */

static int fg_psy_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *pval)
{
	struct fg_gen3_chip *chip = power_supply_get_drvdata(psy);
	struct fg_dev *fg = &chip->fg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = fg_get_prop_capacity(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_RAW:
		rc = fg_get_msoc_raw(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (fg->battery_missing)
			pval->intval = 3700000;
		else
			rc = fg_get_battery_voltage(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = fg_get_battery_current(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc = fg_get_battery_temp(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_COLD_TEMP:
		rc = fg_get_jeita_threshold(fg, JEITA_COLD, &pval->intval);
		if (rc < 0) {
			pr_err("Error in reading jeita_cold, rc=%d\n", rc);
			return rc;
		}
		break;
	case POWER_SUPPLY_PROP_COOL_TEMP:
		rc = fg_get_jeita_threshold(fg, JEITA_COOL, &pval->intval);
		if (rc < 0) {
			pr_err("Error in reading jeita_cool, rc=%d\n", rc);
			return rc;
		}
		break;
	case POWER_SUPPLY_PROP_WARM_TEMP:
		rc = fg_get_jeita_threshold(fg, JEITA_WARM, &pval->intval);
		if (rc < 0) {
			pr_err("Error in reading jeita_warm, rc=%d\n", rc);
			return rc;
		}
		break;
	case POWER_SUPPLY_PROP_HOT_TEMP:
		rc = fg_get_jeita_threshold(fg, JEITA_HOT, &pval->intval);
		if (rc < 0) {
			pr_err("Error in reading jeita_hot, rc=%d\n", rc);
			return rc;
		}
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		rc = fg_get_battery_resistance(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		rc = fg_get_sram_prop(fg, FG_SRAM_OCV, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		pval->intval = chip->cl.nom_cap_uah;
		break;
	case POWER_SUPPLY_PROP_RESISTANCE_ID:
		pval->intval = fg->batt_id_ohms;
		break;
	case POWER_SUPPLY_PROP_BATTERY_TYPE:
		pval->strval = fg_get_battery_type(fg);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		pval->intval = fg->bp.float_volt_uv;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		pval->intval = fg_get_cycle_count(fg);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNTS:
		pval->strval = fg_get_cycle_counts(fg);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW_RAW:
		rc = fg_get_charge_raw(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		pval->intval = chip->cl.init_cc_uah;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		pval->intval = chip->cl.learned_cc_uah;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		rc = fg_get_charge_counter(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW:
		rc = fg_get_charge_counter_shadow(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
		rc = fg_get_time_to_full(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		rc = fg_get_time_to_full(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		rc = fg_get_time_to_empty(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_SOC_REPORTING_READY:
		pval->intval = fg->soc_reporting_ready;
		break;
	case POWER_SUPPLY_PROP_DEBUG_BATTERY:
		pval->intval = is_debug_batt_id(fg);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		rc = fg_get_sram_prop(fg, FG_SRAM_VBATT_FULL, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CC_STEP:
		if ((chip->ttf.cc_step.sel >= 0) &&
				(chip->ttf.cc_step.sel < MAX_CC_STEPS)) {
			pval->intval =
				chip->ttf.cc_step.arr[chip->ttf.cc_step.sel];
		} else {
			pr_err("cc_step_sel is out of bounds [0, %d]\n",
				chip->ttf.cc_step.sel);
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CC_STEP_SEL:
		pval->intval = chip->ttf.cc_step.sel;
		break;
	default:
		pr_err("unsupported property %d\n", psp);
		rc = -EINVAL;
		break;
	}

	if (rc < 0)
		return -ENODATA;

	return 0;
}

static int fg_psy_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *pval)
{
	struct fg_gen3_chip *chip = power_supply_get_drvdata(psy);
	struct fg_dev *fg = &chip->fg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		rc = fg_set_constant_chg_voltage(fg, pval->intval);
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		rc = fg_force_esr_meas(fg);
		break;
	case POWER_SUPPLY_PROP_CHARGE_QNOVO_ENABLE:
		rc = fg_prepare_for_qnovo(fg, pval->intval);
		break;
	case POWER_SUPPLY_PROP_CC_STEP:
		if ((chip->ttf.cc_step.sel >= 0) &&
				(chip->ttf.cc_step.sel < MAX_CC_STEPS)) {
			chip->ttf.cc_step.arr[chip->ttf.cc_step.sel] =
								pval->intval;
		} else {
			pr_err("cc_step_sel is out of bounds [0, %d]\n",
				chip->ttf.cc_step.sel);
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CC_STEP_SEL:
		if ((pval->intval >= 0) && (pval->intval < MAX_CC_STEPS)) {
			chip->ttf.cc_step.sel = pval->intval;
		} else {
			pr_err("cc_step_sel is out of bounds [0, %d]\n",
				pval->intval);
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (chip->cl.active) {
			pr_warn("Capacity learning active!\n");
			return 0;
		}
		if (pval->intval <= 0 || pval->intval > chip->cl.nom_cap_uah) {
			pr_err("charge_full is out of bounds\n");
			return -EINVAL;
		}
		chip->cl.learned_cc_uah = pval->intval;
		rc = fg_save_learned_cap_to_sram(fg);
		if (rc < 0)
			pr_err("Error in saving learned_cc_uah, rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_COLD_TEMP:
		rc = fg_set_jeita_threshold(fg, JEITA_COLD, pval->intval);
		if (rc < 0) {
			pr_err("Error in writing jeita_cold, rc=%d\n", rc);
			return rc;
		}
		break;
	case POWER_SUPPLY_PROP_COOL_TEMP:
		rc = fg_set_jeita_threshold(fg, JEITA_COOL, pval->intval);
		if (rc < 0) {
			pr_err("Error in writing jeita_cool, rc=%d\n", rc);
			return rc;
		}
		break;
	case POWER_SUPPLY_PROP_WARM_TEMP:
		rc = fg_set_jeita_threshold(fg, JEITA_WARM, pval->intval);
		if (rc < 0) {
			pr_err("Error in writing jeita_warm, rc=%d\n", rc);
			return rc;
		}
		break;
	case POWER_SUPPLY_PROP_HOT_TEMP:
		rc = fg_set_jeita_threshold(fg, JEITA_HOT, pval->intval);
		if (rc < 0) {
			pr_err("Error in writing jeita_hot, rc=%d\n", rc);
			return rc;
		}
		break;
	default:
		break;
	}

	return rc;
}

static int fg_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_CC_STEP:
	case POWER_SUPPLY_PROP_CC_STEP_SEL:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_COLD_TEMP:
	case POWER_SUPPLY_PROP_COOL_TEMP:
	case POWER_SUPPLY_PROP_WARM_TEMP:
	case POWER_SUPPLY_PROP_HOT_TEMP:
		return 1;
	default:
		break;
	}

	return 0;
}

static void fg_external_power_changed(struct power_supply *psy)
{
	pr_debug("power supply changed\n");
}

static int fg_notifier_cb(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct fg_dev *fg = container_of(nb, struct fg_dev, nb);

	spin_lock(&fg->suspend_lock);
	if (fg->suspended) {
		/* Return if we are still suspended */
		spin_unlock(&fg->suspend_lock);
		return NOTIFY_OK;
	}
	spin_unlock(&fg->suspend_lock);

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (work_pending(&fg->status_change_work))
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "battery") == 0)
		|| (strcmp(psy->desc->name, "parallel") == 0)
		|| (strcmp(psy->desc->name, "usb") == 0)) {
		/*
		 * We cannot vote for awake votable here as that takes
		 * a mutex lock and this is executed in an atomic context.
		 */
		fg_stay_awake(fg, FG_STATUS_NOTIFY_WAKE);
		schedule_work(&fg->status_change_work);
	}

	return NOTIFY_OK;
}

static int twm_notifier_cb(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct fg_dev *fg = container_of(nb, struct fg_dev, twm_nb);

	if (action != PMIC_TWM_CLEAR &&
			action != PMIC_TWM_ENABLE) {
		pr_debug("Unsupported option %lu\n", action);
		return NOTIFY_OK;
	}

	fg->twm_state = (u8)action;

	return NOTIFY_OK;
}

static enum power_supply_property fg_psy_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_RAW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_COLD_TEMP,
	POWER_SUPPLY_PROP_COOL_TEMP,
	POWER_SUPPLY_PROP_WARM_TEMP,
	POWER_SUPPLY_PROP_HOT_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_BATTERY_TYPE,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CYCLE_COUNTS,
	POWER_SUPPLY_PROP_CHARGE_NOW_RAW,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_SOC_REPORTING_READY,
	POWER_SUPPLY_PROP_DEBUG_BATTERY,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CC_STEP,
	POWER_SUPPLY_PROP_CC_STEP_SEL,
};

static const struct power_supply_desc fg_psy_desc = {
	.name = "bms",
	.type = POWER_SUPPLY_TYPE_BMS,
	.properties = fg_psy_props,
	.num_properties = ARRAY_SIZE(fg_psy_props),
	.get_property = fg_psy_get_property,
	.set_property = fg_psy_set_property,
	.external_power_changed = fg_external_power_changed,
	.property_is_writeable = fg_property_is_writeable,
};

/* INIT FUNCTIONS STAY HERE */

#define DEFAULT_ESR_CHG_TIMER_RETRY	8
#define DEFAULT_ESR_CHG_TIMER_MAX	16
#define VOLTAGE_MODE_SAT_CLEAR_BIT	BIT(3)
static int fg_hw_init(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc;
	u8 buf[4], val;

	fg_encode(fg->sp, FG_SRAM_CUTOFF_VOLT, chip->dt.cutoff_volt_mv, buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_CUTOFF_VOLT].addr_word,
			fg->sp[FG_SRAM_CUTOFF_VOLT].addr_byte, buf,
			fg->sp[FG_SRAM_CUTOFF_VOLT].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing cutoff_volt, rc=%d\n", rc);
		return rc;
	}

	fg_encode(fg->sp, FG_SRAM_EMPTY_VOLT, chip->dt.empty_volt_mv, buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_EMPTY_VOLT].addr_word,
			fg->sp[FG_SRAM_EMPTY_VOLT].addr_byte, buf,
			fg->sp[FG_SRAM_EMPTY_VOLT].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing empty_volt, rc=%d\n", rc);
		return rc;
	}

	fg_encode(fg->sp, FG_SRAM_CHG_TERM_CURR, chip->dt.chg_term_curr_ma,
		buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_CHG_TERM_CURR].addr_word,
			fg->sp[FG_SRAM_CHG_TERM_CURR].addr_byte, buf,
			fg->sp[FG_SRAM_CHG_TERM_CURR].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing chg_term_curr, rc=%d\n", rc);
		return rc;
	}

	fg_encode(fg->sp, FG_SRAM_SYS_TERM_CURR, chip->dt.sys_term_curr_ma,
		buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_SYS_TERM_CURR].addr_word,
			fg->sp[FG_SRAM_SYS_TERM_CURR].addr_byte, buf,
			fg->sp[FG_SRAM_SYS_TERM_CURR].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing sys_term_curr, rc=%d\n", rc);
		return rc;
	}

	fg_encode(fg->sp, FG_SRAM_CUTOFF_CURR, chip->dt.cutoff_curr_ma,
		buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_CUTOFF_CURR].addr_word,
			fg->sp[FG_SRAM_CUTOFF_CURR].addr_byte, buf,
			fg->sp[FG_SRAM_CUTOFF_CURR].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing cutoff_curr, rc=%d\n", rc);
		return rc;
	}

	if (!(fg->wa_flags & PMI8998_V1_REV_WA)) {
		fg_encode(fg->sp, FG_SRAM_CHG_TERM_BASE_CURR,
			chip->dt.chg_term_base_curr_ma, buf);
		rc = fg_sram_write(fg,
				fg->sp[FG_SRAM_CHG_TERM_BASE_CURR].addr_word,
				fg->sp[FG_SRAM_CHG_TERM_BASE_CURR].addr_byte,
				buf, fg->sp[FG_SRAM_CHG_TERM_BASE_CURR].len,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing chg_term_base_curr, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (chip->dt.vbatt_low_thr_mv > 0) {
		fg_encode(fg->sp, FG_SRAM_VBATT_LOW,
			chip->dt.vbatt_low_thr_mv, buf);
		rc = fg_sram_write(fg, fg->sp[FG_SRAM_VBATT_LOW].addr_word,
				fg->sp[FG_SRAM_VBATT_LOW].addr_byte, buf,
				fg->sp[FG_SRAM_VBATT_LOW].len,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing vbatt_low_thr, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.delta_soc_thr > 0 && chip->dt.delta_soc_thr < 100) {
		fg_encode(fg->sp, FG_SRAM_DELTA_MSOC_THR,
			chip->dt.delta_soc_thr, buf);
		rc = fg_sram_write(fg,
				fg->sp[FG_SRAM_DELTA_MSOC_THR].addr_word,
				fg->sp[FG_SRAM_DELTA_MSOC_THR].addr_byte,
				buf, fg->sp[FG_SRAM_DELTA_MSOC_THR].len,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing delta_msoc_thr, rc=%d\n", rc);
			return rc;
		}

		fg_encode(fg->sp, FG_SRAM_DELTA_BSOC_THR,
			chip->dt.delta_soc_thr, buf);
		rc = fg_sram_write(fg,
				fg->sp[FG_SRAM_DELTA_BSOC_THR].addr_word,
				fg->sp[FG_SRAM_DELTA_BSOC_THR].addr_byte,
				buf, fg->sp[FG_SRAM_DELTA_BSOC_THR].len,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing delta_bsoc_thr, rc=%d\n", rc);
			return rc;
		}
	}

	/*
	 * configure battery thermal coefficients c1,c2,c3
	 * if its value is not zero.
	 */
	if (chip->dt.batt_therm_coeffs[0] > 0) {
		rc = fg_write(fg, BATT_INFO_THERM_C1(fg),
			chip->dt.batt_therm_coeffs, BATT_THERM_NUM_COEFFS);
		if (rc < 0) {
			pr_err("Error in writing battery thermal coefficients, rc=%d\n",
				rc);
			return rc;
		}
	}


	if (chip->dt.recharge_soc_thr > 0 && chip->dt.recharge_soc_thr < 100) {
		rc = fg_set_recharge_soc(fg, chip->dt.recharge_soc_thr);
		if (rc < 0) {
			pr_err("Error in setting recharge_soc, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.recharge_volt_thr_mv > 0) {
		rc = fg_set_recharge_voltage(fg, chip->dt.recharge_volt_thr_mv);
		if (rc < 0) {
			pr_err("Error in setting recharge_voltage, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (chip->dt.rsense_sel >= SRC_SEL_BATFET &&
			chip->dt.rsense_sel < SRC_SEL_RESERVED) {
		rc = fg_masked_write(fg, BATT_INFO_IBATT_SENSING_CFG(fg),
				SOURCE_SELECT_MASK, chip->dt.rsense_sel);
		if (rc < 0) {
			pr_err("Error in writing rsense_sel, rc=%d\n", rc);
			return rc;
		}
	}

	rc = fg_set_jeita_threshold(fg, JEITA_COLD,
		chip->dt.jeita_thresholds[JEITA_COLD] * 10);
	if (rc < 0) {
		pr_err("Error in writing jeita_cold, rc=%d\n", rc);
		return rc;
	}

	rc = fg_set_jeita_threshold(fg, JEITA_COOL,
		chip->dt.jeita_thresholds[JEITA_COOL] * 10);
	if (rc < 0) {
		pr_err("Error in writing jeita_cool, rc=%d\n", rc);
		return rc;
	}

	rc = fg_set_jeita_threshold(fg, JEITA_WARM,
		chip->dt.jeita_thresholds[JEITA_WARM] * 10);
	if (rc < 0) {
		pr_err("Error in writing jeita_warm, rc=%d\n", rc);
		return rc;
	}

	rc = fg_set_jeita_threshold(fg, JEITA_HOT,
		chip->dt.jeita_thresholds[JEITA_HOT] * 10);
	if (rc < 0) {
		pr_err("Error in writing jeita_hot, rc=%d\n", rc);
		return rc;
	}

	if (fg->pmic_rev_id->pmic_subtype == PMI8998_SUBTYPE) {
		chip->esr_timer_charging_default[TIMER_RETRY] =
			DEFAULT_ESR_CHG_TIMER_RETRY;
		chip->esr_timer_charging_default[TIMER_MAX] =
			DEFAULT_ESR_CHG_TIMER_MAX;
	} else {
		/* We don't need this for pm660 at present */
		chip->esr_timer_charging_default[TIMER_RETRY] = -EINVAL;
		chip->esr_timer_charging_default[TIMER_MAX] = -EINVAL;
	}

	rc = fg_set_esr_timer(fg, chip->dt.esr_timer_charging[TIMER_RETRY],
		chip->dt.esr_timer_charging[TIMER_MAX], true, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in setting ESR timer, rc=%d\n", rc);
		return rc;
	}

	rc = fg_set_esr_timer(fg, chip->dt.esr_timer_awake[TIMER_RETRY],
		chip->dt.esr_timer_awake[TIMER_MAX], false, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in setting ESR timer, rc=%d\n", rc);
		return rc;
	}

	restore_cycle_counter(fg);

	if (chip->dt.jeita_hyst_temp >= 0) {
		val = chip->dt.jeita_hyst_temp << JEITA_TEMP_HYST_SHIFT;
		rc = fg_masked_write(fg, BATT_INFO_BATT_TEMP_CFG(fg),
			JEITA_TEMP_HYST_MASK, val);
		if (rc < 0) {
			pr_err("Error in writing batt_temp_cfg, rc=%d\n", rc);
			return rc;
		}
	}

	get_batt_temp_delta(chip->dt.batt_temp_delta, &val);
	rc = fg_masked_write(fg, BATT_INFO_BATT_TMPR_INTR(fg),
			CHANGE_THOLD_MASK, val);
	if (rc < 0) {
		pr_err("Error in writing batt_temp_delta, rc=%d\n", rc);
		return rc;
	}

	rc = fg_sram_masked_write(fg, ESR_EXTRACTION_ENABLE_WORD,
				ESR_EXTRACTION_ENABLE_OFFSET,
				VOLTAGE_MODE_SAT_CLEAR_BIT,
				VOLTAGE_MODE_SAT_CLEAR_BIT,
				FG_IMA_DEFAULT);
	if (rc < 0)
		return rc;

	fg_encode(fg->sp, FG_SRAM_ESR_TIGHT_FILTER,
		chip->dt.esr_tight_flt_upct, buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_ESR_TIGHT_FILTER].addr_word,
			fg->sp[FG_SRAM_ESR_TIGHT_FILTER].addr_byte, buf,
			fg->sp[FG_SRAM_ESR_TIGHT_FILTER].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR tight filter, rc=%d\n", rc);
		return rc;
	}

	fg_encode(fg->sp, FG_SRAM_ESR_BROAD_FILTER,
		chip->dt.esr_broad_flt_upct, buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_ESR_BROAD_FILTER].addr_word,
			fg->sp[FG_SRAM_ESR_BROAD_FILTER].addr_byte, buf,
			fg->sp[FG_SRAM_ESR_BROAD_FILTER].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR broad filter, rc=%d\n", rc);
		return rc;
	}

	fg_encode(fg->sp, FG_SRAM_ESR_PULSE_THRESH,
		chip->dt.esr_pulse_thresh_ma, buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_ESR_PULSE_THRESH].addr_word,
			fg->sp[FG_SRAM_ESR_PULSE_THRESH].addr_byte, buf,
			fg->sp[FG_SRAM_ESR_PULSE_THRESH].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing esr_pulse_thresh_ma, rc=%d\n", rc);
		return rc;
	}

	get_esr_meas_current(chip->dt.esr_meas_curr_ma, &val);
	rc = fg_masked_write(fg, BATT_INFO_ESR_PULL_DN_CFG(fg),
			ESR_PULL_DOWN_IVAL_MASK, val);
	if (rc < 0) {
		pr_err("Error in writing esr_meas_curr_ma, rc=%d\n", rc);
		return rc;
	}

	if (is_debug_batt_id(fg) || chip->dt.disable_esr_pull_dn) {
		val = ESR_NO_PULL_DOWN;
		rc = fg_masked_write(fg, BATT_INFO_ESR_PULL_DN_CFG(fg),
			ESR_PULL_DOWN_MODE_MASK, val);
		if (rc < 0) {
			pr_err("Error in writing esr_pull_down, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.use_esr_sw) {
		/* Enable ESR extraction explicitly */
		rc = fg_sram_masked_write(fg, ESR_EXTRACTION_ENABLE_WORD,
				ESR_EXTRACTION_ENABLE_OFFSET,
				ESR_EXTRACTION_ENABLE_MASK,
				0x1, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in enabling ESR extraction rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.sync_sleep_threshold_ma != -EINVAL) {
		fg_encode(fg->sp, FG_SRAM_SYNC_SLEEP_THR,
			chip->dt.sync_sleep_threshold_ma, buf);
		rc = fg_sram_write(fg,
			fg->sp[FG_SRAM_SYNC_SLEEP_THR].addr_word,
			fg->sp[FG_SRAM_SYNC_SLEEP_THR].addr_byte, buf,
			fg->sp[FG_SRAM_SYNC_SLEEP_THR].len,
			FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing sync_sleep_threshold=%d\n",
				rc);
			return rc;
		}
	}

	return 0;
}

static int fg_adjust_timebase(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc = 0, die_temp;
	s32 time_base = 0;
	u8 buf[2] = {0};

	if ((fg->wa_flags & PM660_TSMC_OSC_WA) && chip->die_temp_chan) {
		rc = iio_read_channel_processed(chip->die_temp_chan, &die_temp);
		if (rc < 0) {
			pr_err("Error in reading die_temp, rc:%d\n", rc);
			return rc;
		}

		rc = fg_lerp(fg_tsmc_osc_table, ARRAY_SIZE(fg_tsmc_osc_table),
					die_temp / 1000, &time_base);
		if (rc < 0) {
			pr_err("Error to lookup fg_tsmc_osc_table rc=%d\n", rc);
			return rc;
		}

		fg_encode(fg->sp, FG_SRAM_TIMEBASE, time_base, buf);
		rc = fg_sram_write(fg,
			fg->sp[FG_SRAM_TIMEBASE].addr_word,
			fg->sp[FG_SRAM_TIMEBASE].addr_byte, buf,
			fg->sp[FG_SRAM_TIMEBASE].len, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing timebase, rc=%d\n", rc);
			return rc;
		}
	}

	return 0;
}

/* INTERRUPT HANDLERS STAY HERE */

static irqreturn_t fg_mem_xcp_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;
	u8 status;
	int rc;

	rc = fg_read(fg, MEM_IF_INT_RT_STS(fg), &status, 1);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			MEM_IF_INT_RT_STS(fg), rc);
		return IRQ_HANDLED;
	}

	fg_dbg(fg, FG_IRQ, "irq %d triggered, status:%d\n", irq, status);

	mutex_lock(&fg->sram_rw_lock);
	rc = fg_clear_dma_errors_if_any(fg);
	if (rc < 0)
		pr_err("Error in clearing DMA error, rc=%d\n", rc);

	if (status & MEM_XCP_BIT) {
		rc = fg_clear_ima_errors_if_any(fg, true);
		if (rc < 0 && rc != -EAGAIN)
			pr_err("Error in checking IMA errors rc:%d\n", rc);
	}

	mutex_unlock(&fg->sram_rw_lock);
	return IRQ_HANDLED;
}

static irqreturn_t fg_vbatt_low_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;

	fg_dbg(fg, FG_IRQ, "irq %d triggered\n", irq);
	return IRQ_HANDLED;
}

static irqreturn_t fg_batt_missing_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	u8 status;
	int rc;

	rc = fg_read(fg, BATT_INFO_INT_RT_STS(fg), &status, 1);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			BATT_INFO_INT_RT_STS(fg), rc);
		return IRQ_HANDLED;
	}

	fg_dbg(fg, FG_IRQ, "irq %d triggered sts:%d\n", irq, status);
	fg->battery_missing = (status & BT_MISS_BIT);

	if (fg->battery_missing) {
		fg->profile_available = false;
		fg->profile_load_status = PROFILE_NOT_LOADED;
		fg->soc_reporting_ready = false;
		fg->batt_id_ohms = -EINVAL;
		cancel_delayed_work_sync(&chip->pl_enable_work);
		vote(chip->pl_disable_votable, ESR_FCC_VOTER, true, 0);
		return IRQ_HANDLED;
	}

	clear_battery_profile(fg);
	schedule_delayed_work(&fg->profile_load_work, 0);

	if (fg->fg_psy)
		power_supply_changed(fg->fg_psy);

	return IRQ_HANDLED;
}

static irqreturn_t fg_delta_batt_temp_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;
	union power_supply_propval prop = {0, };
	int rc, batt_temp;

	rc = fg_get_battery_temp(fg, &batt_temp);
	if (rc < 0) {
		pr_err("Error in getting batt_temp\n");
		return IRQ_HANDLED;
	}
	fg_dbg(fg, FG_IRQ, "irq %d triggered bat_temp: %d\n", irq, batt_temp);

	rc = fg_esr_filter_config(fg, batt_temp, false);
	if (rc < 0)
		pr_err("Error in configuring ESR filter rc:%d\n", rc);

	rc = fg_slope_limit_config(fg, batt_temp);
	if (rc < 0)
		pr_err("Error in configuring slope limiter rc:%d\n", rc);

	rc = fg_adjust_ki_coeff_full_soc(fg, batt_temp);
	if (rc < 0)
		pr_err("Error in configuring ki_coeff_full_soc rc:%d\n", rc);

	if (!batt_psy_initialized(fg)) {
		fg->last_batt_temp = batt_temp;
		return IRQ_HANDLED;
	}

	power_supply_get_property(fg->batt_psy, POWER_SUPPLY_PROP_HEALTH,
		&prop);
	fg->health = prop.intval;

	if (fg->last_batt_temp != batt_temp) {
		rc = fg_adjust_timebase(fg);
		if (rc < 0)
			pr_err("Error in adjusting timebase, rc=%d\n", rc);

		rc = fg_adjust_recharge_voltage(fg);
		if (rc < 0)
			pr_err("Error in adjusting recharge_voltage, rc=%d\n",
				rc);

		fg->last_batt_temp = batt_temp;
		power_supply_changed(fg->batt_psy);
	}

	if (abs(fg->last_batt_temp - batt_temp) > 30)
		pr_warn("Battery temperature last:%d current: %d\n",
			fg->last_batt_temp, batt_temp);
	return IRQ_HANDLED;
}

static irqreturn_t fg_first_est_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;

	fg_dbg(fg, FG_IRQ, "irq %d triggered\n", irq);
	complete_all(&fg->soc_ready);
	return IRQ_HANDLED;
}

static irqreturn_t fg_soc_update_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;

	fg_dbg(fg, FG_IRQ, "irq %d triggered\n", irq);
	complete_all(&fg->soc_update);
	return IRQ_HANDLED;
}

static irqreturn_t fg_delta_bsoc_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;
	int rc;

	fg_dbg(fg, FG_IRQ, "irq %d triggered\n", irq);
	rc = fg_charge_full_update(fg);
	if (rc < 0)
		pr_err("Error in charge_full_update, rc=%d\n", rc);

	return IRQ_HANDLED;
}

static irqreturn_t fg_delta_msoc_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	int rc;

	fg_dbg(fg, FG_IRQ, "irq %d triggered\n", irq);
	fg_cycle_counter_update(fg);

	if (chip->cl.active)
		fg_cap_learning_update(fg);

	rc = fg_charge_full_update(fg);
	if (rc < 0)
		pr_err("Error in charge_full_update, rc=%d\n", rc);

	rc = fg_adjust_ki_coeff_dischg(fg);
	if (rc < 0)
		pr_err("Error in adjusting ki_coeff_dischg, rc=%d\n", rc);

	rc = fg_update_maint_soc(fg);
	if (rc < 0)
		pr_err("Error in updating maint_soc, rc=%d\n", rc);

	rc = fg_esr_validate(fg);
	if (rc < 0)
		pr_err("Error in validating ESR, rc=%d\n", rc);

	rc = fg_adjust_timebase(fg);
	if (rc < 0)
		pr_err("Error in adjusting timebase, rc=%d\n", rc);

	if (batt_psy_initialized(fg))
		power_supply_changed(fg->batt_psy);

	return IRQ_HANDLED;
}

static irqreturn_t fg_empty_soc_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;

	fg_dbg(fg, FG_IRQ, "irq %d triggered\n", irq);
	if (batt_psy_initialized(fg))
		power_supply_changed(fg->batt_psy);

	return IRQ_HANDLED;
}

static irqreturn_t fg_soc_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;

	fg_dbg(fg, FG_IRQ, "irq %d triggered\n", irq);
	return IRQ_HANDLED;
}

static irqreturn_t fg_dummy_irq_handler(int irq, void *data)
{
	pr_debug("irq %d triggered\n", irq);
	return IRQ_HANDLED;
}

static struct fg_irq_info fg_irqs[FG_GEN3_IRQ_MAX] = {
	/* BATT_SOC irqs */
	[MSOC_FULL_IRQ] = {
		.name		= "msoc-full",
		.handler	= fg_soc_irq_handler,
	},
	[MSOC_HIGH_IRQ] = {
		.name		= "msoc-high",
		.handler	= fg_soc_irq_handler,
		.wakeable	= true,
	},
	[MSOC_EMPTY_IRQ] = {
		.name		= "msoc-empty",
		.handler	= fg_empty_soc_irq_handler,
		.wakeable	= true,
	},
	[MSOC_LOW_IRQ] = {
		.name		= "msoc-low",
		.handler	= fg_soc_irq_handler,
		.wakeable	= true,
	},
	[MSOC_DELTA_IRQ] = {
		.name		= "msoc-delta",
		.handler	= fg_delta_msoc_irq_handler,
		.wakeable	= true,
	},
	[BSOC_DELTA_IRQ] = {
		.name		= "bsoc-delta",
		.handler	= fg_delta_bsoc_irq_handler,
		.wakeable	= true,
	},
	[SOC_READY_IRQ] = {
		.name		= "soc-ready",
		.handler	= fg_first_est_irq_handler,
		.wakeable	= true,
	},
	[SOC_UPDATE_IRQ] = {
		.name		= "soc-update",
		.handler	= fg_soc_update_irq_handler,
	},
	/* BATT_INFO irqs */
	[BATT_TEMP_DELTA_IRQ] = {
		.name		= "batt-temp-delta",
		.handler	= fg_delta_batt_temp_irq_handler,
		.wakeable	= true,
	},
	[BATT_MISSING_IRQ] = {
		.name		= "batt-missing",
		.handler	= fg_batt_missing_irq_handler,
		.wakeable	= true,
	},
	[ESR_DELTA_IRQ] = {
		.name		= "esr-delta",
		.handler	= fg_dummy_irq_handler,
	},
	[VBATT_LOW_IRQ] = {
		.name		= "vbatt-low",
		.handler	= fg_vbatt_low_irq_handler,
		.wakeable	= true,
	},
	[VBATT_PRED_DELTA_IRQ] = {
		.name		= "vbatt-pred-delta",
		.handler	= fg_dummy_irq_handler,
	},
	/* MEM_IF irqs */
	[DMA_GRANT_IRQ] = {
		.name		= "dma-grant",
		.handler	= fg_dummy_irq_handler,
		.wakeable	= true,
	},
	[MEM_XCP_IRQ] = {
		.name		= "mem-xcp",
		.handler	= fg_mem_xcp_irq_handler,
	},
	[IMA_RDY_IRQ] = {
		.name		= "ima-rdy",
		.handler	= fg_dummy_irq_handler,
	},
};

static int fg_parse_dt_property_u32_array(struct device_node *node,
				const char *prop_name, int *buf, int len)
{
	int rc;

	rc = of_property_count_elems_of_size(node, prop_name, sizeof(u32));
	if (rc < 0) {
		if (rc == -EINVAL)
			return 0;
		else
			return rc;
	} else if (rc != len) {
		pr_err("Incorrect length %d for %s, rc=%d\n", len, prop_name,
			rc);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(node, prop_name, buf, len);
	if (rc < 0) {
		pr_err("Error in reading %s, rc=%d\n", prop_name, rc);
		return rc;
	}

	return 0;
}

static int fg_parse_slope_limit_coefficients(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	struct device_node *node = fg->dev->of_node;
	int rc, i;

	rc = of_property_read_u32(node, "qcom,slope-limit-temp-threshold",
			&chip->dt.slope_limit_temp);
	if (rc < 0)
		return 0;

	rc = fg_parse_dt_property_u32_array(node, "qcom,slope-limit-coeffs",
		chip->dt.slope_limit_coeffs, SLOPE_LIMIT_NUM_COEFFS);
	if (rc < 0)
		return rc;

	for (i = 0; i < SLOPE_LIMIT_NUM_COEFFS; i++) {
		if (chip->dt.slope_limit_coeffs[i] > SLOPE_LIMIT_COEFF_MAX ||
			chip->dt.slope_limit_coeffs[i] < 0) {
			pr_err("Incorrect slope limit coefficient\n");
			return -EINVAL;
		}
	}

	chip->slope_limit_en = true;
	return 0;
}

static int fg_parse_ki_coefficients(struct fg_dev *fg)
{
	struct fg_gen3_chip *chip = container_of(fg, struct fg_gen3_chip, fg);
	struct device_node *node = fg->dev->of_node;
	int rc, i, temp;

	rc = of_property_read_u32(node, "qcom,ki-coeff-full-dischg", &temp);
	if (!rc)
		chip->dt.ki_coeff_full_soc_dischg = temp;

	rc = fg_parse_dt_property_u32_array(node, "qcom,ki-coeff-soc-dischg",
		chip->dt.ki_coeff_soc, KI_COEFF_SOC_LEVELS);
	if (rc < 0)
		return rc;

	rc = fg_parse_dt_property_u32_array(node, "qcom,ki-coeff-med-dischg",
		chip->dt.ki_coeff_med_dischg, KI_COEFF_SOC_LEVELS);
	if (rc < 0)
		return rc;

	rc = fg_parse_dt_property_u32_array(node, "qcom,ki-coeff-hi-dischg",
		chip->dt.ki_coeff_hi_dischg, KI_COEFF_SOC_LEVELS);
	if (rc < 0)
		return rc;

	for (i = 0; i < KI_COEFF_SOC_LEVELS; i++) {
		if (chip->dt.ki_coeff_soc[i] < 0 ||
			chip->dt.ki_coeff_soc[i] > FULL_CAPACITY) {
			pr_err("Error in ki_coeff_soc_dischg values\n");
			return -EINVAL;
		}

		if (chip->dt.ki_coeff_med_dischg[i] < 0 ||
			chip->dt.ki_coeff_med_dischg[i] > KI_COEFF_MAX) {
			pr_err("Error in ki_coeff_med_dischg values\n");
			return -EINVAL;
		}

		if (chip->dt.ki_coeff_med_dischg[i] < 0 ||
			chip->dt.ki_coeff_med_dischg[i] > KI_COEFF_MAX) {
			pr_err("Error in ki_coeff_med_dischg values\n");
			return -EINVAL;
		}
	}
	chip->ki_coeff_dischg_en = true;
	return 0;
}

#define DEFAULT_CUTOFF_VOLT_MV		3200
#define DEFAULT_EMPTY_VOLT_MV		2850
#define DEFAULT_RECHARGE_VOLT_MV	4250
#define DEFAULT_CHG_TERM_CURR_MA	100
#define DEFAULT_CHG_TERM_BASE_CURR_MA	75
#define DEFAULT_SYS_TERM_CURR_MA	-125
#define DEFAULT_CUTOFF_CURR_MA		500
#define DEFAULT_DELTA_SOC_THR		1
#define DEFAULT_RECHARGE_SOC_THR	95
#define DEFAULT_BATT_TEMP_COLD		0
#define DEFAULT_BATT_TEMP_COOL		5
#define DEFAULT_BATT_TEMP_WARM		45
#define DEFAULT_BATT_TEMP_HOT		50
#define DEFAULT_CL_START_SOC		15
#define DEFAULT_CL_MIN_TEMP_DECIDEGC	150
#define DEFAULT_CL_MAX_TEMP_DECIDEGC	500
#define DEFAULT_CL_MAX_INC_DECIPERC	5
#define DEFAULT_CL_MAX_DEC_DECIPERC	100
#define DEFAULT_CL_MIN_LIM_DECIPERC	0
#define DEFAULT_CL_MAX_LIM_DECIPERC	0
#define BTEMP_DELTA_LOW			2
#define BTEMP_DELTA_HIGH		10
#define DEFAULT_ESR_FLT_TEMP_DECIDEGC	100
#define DEFAULT_ESR_TIGHT_FLT_UPCT	3907
#define DEFAULT_ESR_BROAD_FLT_UPCT	99610
#define DEFAULT_ESR_TIGHT_LT_FLT_UPCT	30000
#define DEFAULT_ESR_BROAD_LT_FLT_UPCT	30000
#define DEFAULT_ESR_FLT_RT_DECIDEGC	60
#define DEFAULT_ESR_TIGHT_RT_FLT_UPCT	5860
#define DEFAULT_ESR_BROAD_RT_FLT_UPCT	156250
#define DEFAULT_ESR_CLAMP_MOHMS		20
#define DEFAULT_ESR_PULSE_THRESH_MA	110
#define DEFAULT_ESR_MEAS_CURR_MA	120
#define DEFAULT_BMD_EN_DELAY_MS	200
static int fg_parse_dt(struct fg_gen3_chip *chip)
{
	struct fg_dev *fg = &chip->fg;
	struct device_node *child, *revid_node, *node = fg->dev->of_node;
	u32 base, temp;
	u8 subtype;
	int rc;

	if (!node)  {
		dev_err(fg->dev, "device tree node missing\n");
		return -ENXIO;
	}

	revid_node = of_parse_phandle(node, "qcom,pmic-revid", 0);
	if (!revid_node) {
		pr_err("Missing qcom,pmic-revid property - driver failed\n");
		return -EINVAL;
	}

	fg->pmic_rev_id = get_revid_data(revid_node);
	if (IS_ERR_OR_NULL(fg->pmic_rev_id)) {
		pr_err("Unable to get pmic_revid rc=%ld\n",
			PTR_ERR(fg->pmic_rev_id));
		/*
		 * the revid peripheral must be registered, any failure
		 * here only indicates that the rev-id module has not
		 * probed yet.
		 */
		return -EPROBE_DEFER;
	}

	pr_debug("PMIC subtype %d Digital major %d\n",
		fg->pmic_rev_id->pmic_subtype, fg->pmic_rev_id->rev4);

	switch (fg->pmic_rev_id->pmic_subtype) {
	case PMI8998_SUBTYPE:
		fg->version = GEN3_FG;
		fg->use_dma = true;
		if (fg->pmic_rev_id->rev4 < PMI8998_V2P0_REV4) {
			fg->sp = pmi8998_v1_sram_params;
			fg->alg_flags = pmi8998_v1_alg_flags;
			fg->wa_flags |= PMI8998_V1_REV_WA;
		} else if (fg->pmic_rev_id->rev4 == PMI8998_V2P0_REV4) {
			fg->sp = pmi8998_v2_sram_params;
			fg->alg_flags = pmi8998_v2_alg_flags;
		} else {
			return -EINVAL;
		}
		break;
	case PM660_SUBTYPE:
		fg->version = GEN3_FG;
		fg->sp = pmi8998_v2_sram_params;
		fg->alg_flags = pmi8998_v2_alg_flags;
		fg->use_ima_single_mode = true;
		if (fg->pmic_rev_id->fab_id == PM660_FAB_ID_TSMC)
			fg->wa_flags |= PM660_TSMC_OSC_WA;
		break;
	default:
		return -EINVAL;
	}

	if (of_get_available_child_count(node) == 0) {
		dev_err(fg->dev, "No child nodes specified!\n");
		return -ENXIO;
	}

	for_each_available_child_of_node(node, child) {
		rc = of_property_read_u32(child, "reg", &base);
		if (rc < 0) {
			dev_err(fg->dev, "reg not specified in node %s, rc=%d\n",
				child->full_name, rc);
			return rc;
		}

		rc = fg_read(fg, base + PERPH_SUBTYPE_REG, &subtype, 1);
		if (rc < 0) {
			dev_err(fg->dev, "Couldn't read subtype for base %d, rc=%d\n",
				base, rc);
			return rc;
		}

		switch (subtype) {
		case FG_BATT_SOC_PMI8998:
			fg->batt_soc_base = base;
			break;
		case FG_BATT_INFO_PMI8998:
			fg->batt_info_base = base;
			break;
		case FG_MEM_INFO_PMI8998:
			fg->mem_if_base = base;
			break;
		default:
			dev_err(fg->dev, "Invalid peripheral subtype 0x%x\n",
				subtype);
			return -ENXIO;
		}
	}

	rc = of_property_read_u32(node, "qcom,rradc-base", &base);
	if (rc < 0) {
		dev_err(fg->dev, "rradc-base not specified, rc=%d\n", rc);
		return rc;
	}
	fg->rradc_base = base;

	/* Read all the optional properties below */
	rc = of_property_read_u32(node, "qcom,fg-cutoff-voltage", &temp);
	if (rc < 0)
		chip->dt.cutoff_volt_mv = DEFAULT_CUTOFF_VOLT_MV;
	else
		chip->dt.cutoff_volt_mv = temp;

	rc = of_property_read_u32(node, "qcom,fg-empty-voltage", &temp);
	if (rc < 0)
		chip->dt.empty_volt_mv = DEFAULT_EMPTY_VOLT_MV;
	else
		chip->dt.empty_volt_mv = temp;

	rc = of_property_read_u32(node, "qcom,fg-vbatt-low-thr", &temp);
	if (rc < 0)
		chip->dt.vbatt_low_thr_mv = -EINVAL;
	else
		chip->dt.vbatt_low_thr_mv = temp;

	rc = of_property_read_u32(node, "qcom,fg-chg-term-current", &temp);
	if (rc < 0)
		chip->dt.chg_term_curr_ma = DEFAULT_CHG_TERM_CURR_MA;
	else
		chip->dt.chg_term_curr_ma = temp;

	rc = of_property_read_u32(node, "qcom,fg-sys-term-current", &temp);
	if (rc < 0)
		chip->dt.sys_term_curr_ma = DEFAULT_SYS_TERM_CURR_MA;
	else
		chip->dt.sys_term_curr_ma = temp;

	rc = of_property_read_u32(node, "qcom,fg-chg-term-base-current", &temp);
	if (rc < 0)
		chip->dt.chg_term_base_curr_ma = DEFAULT_CHG_TERM_BASE_CURR_MA;
	else
		chip->dt.chg_term_base_curr_ma = temp;

	rc = of_property_read_u32(node, "qcom,fg-cutoff-current", &temp);
	if (rc < 0)
		chip->dt.cutoff_curr_ma = DEFAULT_CUTOFF_CURR_MA;
	else
		chip->dt.cutoff_curr_ma = temp;

	rc = of_property_read_u32(node, "qcom,fg-delta-soc-thr", &temp);
	if (rc < 0)
		chip->dt.delta_soc_thr = DEFAULT_DELTA_SOC_THR;
	else
		chip->dt.delta_soc_thr = temp;

	rc = of_property_read_u32(node, "qcom,fg-recharge-soc-thr", &temp);
	if (rc < 0)
		chip->dt.recharge_soc_thr = DEFAULT_RECHARGE_SOC_THR;
	else
		chip->dt.recharge_soc_thr = temp;

	rc = of_property_read_u32(node, "qcom,fg-recharge-voltage", &temp);
	if (rc < 0)
		chip->dt.recharge_volt_thr_mv = DEFAULT_RECHARGE_VOLT_MV;
	else
		chip->dt.recharge_volt_thr_mv = temp;

	chip->dt.auto_recharge_soc = of_property_read_bool(node,
					"qcom,fg-auto-recharge-soc");

	rc = of_property_read_u32(node, "qcom,fg-rsense-sel", &temp);
	if (rc < 0)
		chip->dt.rsense_sel = SRC_SEL_BATFET_SMB;
	else
		chip->dt.rsense_sel = (u8)temp & SOURCE_SELECT_MASK;

	chip->dt.jeita_thresholds[JEITA_COLD] = DEFAULT_BATT_TEMP_COLD;
	chip->dt.jeita_thresholds[JEITA_COOL] = DEFAULT_BATT_TEMP_COOL;
	chip->dt.jeita_thresholds[JEITA_WARM] = DEFAULT_BATT_TEMP_WARM;
	chip->dt.jeita_thresholds[JEITA_HOT] = DEFAULT_BATT_TEMP_HOT;
	if (of_property_count_elems_of_size(node, "qcom,fg-jeita-thresholds",
		sizeof(u32)) == NUM_JEITA_LEVELS) {
		rc = of_property_read_u32_array(node,
				"qcom,fg-jeita-thresholds",
				chip->dt.jeita_thresholds, NUM_JEITA_LEVELS);
		if (rc < 0)
			pr_warn("Error reading Jeita thresholds, default values will be used rc:%d\n",
				rc);
	}

	if (of_property_count_elems_of_size(node,
		"qcom,battery-thermal-coefficients",
		sizeof(u8)) == BATT_THERM_NUM_COEFFS) {
		rc = of_property_read_u8_array(node,
				"qcom,battery-thermal-coefficients",
				chip->dt.batt_therm_coeffs,
				BATT_THERM_NUM_COEFFS);
		if (rc < 0)
			pr_warn("Error reading battery thermal coefficients, rc:%d\n",
				rc);
	}

	rc = fg_parse_dt_property_u32_array(node, "qcom,fg-esr-timer-charging",
		chip->dt.esr_timer_charging, NUM_ESR_TIMERS);
	if (rc < 0) {
		chip->dt.esr_timer_charging[TIMER_RETRY] = -EINVAL;
		chip->dt.esr_timer_charging[TIMER_MAX] = -EINVAL;
	}

	rc = fg_parse_dt_property_u32_array(node, "qcom,fg-esr-timer-awake",
		chip->dt.esr_timer_awake, NUM_ESR_TIMERS);
	if (rc < 0) {
		chip->dt.esr_timer_awake[TIMER_RETRY] = -EINVAL;
		chip->dt.esr_timer_awake[TIMER_MAX] = -EINVAL;
	}

	rc = fg_parse_dt_property_u32_array(node, "qcom,fg-esr-timer-asleep",
		chip->dt.esr_timer_asleep, NUM_ESR_TIMERS);
	if (rc < 0) {
		chip->dt.esr_timer_asleep[TIMER_RETRY] = -EINVAL;
		chip->dt.esr_timer_asleep[TIMER_MAX] = -EINVAL;
	}

	rc = fg_parse_dt_property_u32_array(node, "qcom,fg-esr-timer-shutdown",
		chip->dt.esr_timer_shutdown, NUM_ESR_TIMERS);
	if (rc < 0) {
		chip->dt.esr_timer_shutdown[TIMER_RETRY] = -EINVAL;
		chip->dt.esr_timer_shutdown[TIMER_MAX] = -EINVAL;
	}

	chip->cyc_ctr.en = of_property_read_bool(node, "qcom,cycle-counter-en");

	chip->dt.force_load_profile = of_property_read_bool(node,
					"qcom,fg-force-load-profile");

	rc = of_property_read_u32(node, "qcom,cl-start-capacity", &temp);
	if (rc < 0)
		chip->dt.cl_start_soc = DEFAULT_CL_START_SOC;
	else
		chip->dt.cl_start_soc = temp;

	rc = of_property_read_u32(node, "qcom,cl-min-temp", &temp);
	if (rc < 0)
		chip->dt.cl_min_temp = DEFAULT_CL_MIN_TEMP_DECIDEGC;
	else
		chip->dt.cl_min_temp = temp;

	rc = of_property_read_u32(node, "qcom,cl-max-temp", &temp);
	if (rc < 0)
		chip->dt.cl_max_temp = DEFAULT_CL_MAX_TEMP_DECIDEGC;
	else
		chip->dt.cl_max_temp = temp;

	rc = of_property_read_u32(node, "qcom,cl-max-increment", &temp);
	if (rc < 0)
		chip->dt.cl_max_cap_inc = DEFAULT_CL_MAX_INC_DECIPERC;
	else
		chip->dt.cl_max_cap_inc = temp;

	rc = of_property_read_u32(node, "qcom,cl-max-decrement", &temp);
	if (rc < 0)
		chip->dt.cl_max_cap_dec = DEFAULT_CL_MAX_DEC_DECIPERC;
	else
		chip->dt.cl_max_cap_dec = temp;

	rc = of_property_read_u32(node, "qcom,cl-min-limit", &temp);
	if (rc < 0)
		chip->dt.cl_min_cap_limit = DEFAULT_CL_MIN_LIM_DECIPERC;
	else
		chip->dt.cl_min_cap_limit = temp;

	rc = of_property_read_u32(node, "qcom,cl-max-limit", &temp);
	if (rc < 0)
		chip->dt.cl_max_cap_limit = DEFAULT_CL_MAX_LIM_DECIPERC;
	else
		chip->dt.cl_max_cap_limit = temp;

	rc = of_property_read_u32(node, "qcom,fg-jeita-hyst-temp", &temp);
	if (rc < 0)
		chip->dt.jeita_hyst_temp = -EINVAL;
	else
		chip->dt.jeita_hyst_temp = temp;

	rc = of_property_read_u32(node, "qcom,fg-batt-temp-delta", &temp);
	if (rc < 0)
		chip->dt.batt_temp_delta = -EINVAL;
	else if (temp > BTEMP_DELTA_LOW && temp <= BTEMP_DELTA_HIGH)
		chip->dt.batt_temp_delta = temp;

	chip->dt.hold_soc_while_full = of_property_read_bool(node,
					"qcom,hold-soc-while-full");

	chip->dt.linearize_soc = of_property_read_bool(node,
					"qcom,linearize-soc");

	rc = fg_parse_ki_coefficients(fg);
	if (rc < 0)
		pr_err("Error in parsing Ki coefficients, rc=%d\n", rc);

	rc = of_property_read_u32(node, "qcom,fg-rconn-mohms", &temp);
	if (!rc)
		chip->dt.rconn_mohms = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-filter-switch-temp",
			&temp);
	if (rc < 0)
		chip->dt.esr_flt_switch_temp = DEFAULT_ESR_FLT_TEMP_DECIDEGC;
	else
		chip->dt.esr_flt_switch_temp = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-tight-filter-micro-pct",
			&temp);
	if (rc < 0)
		chip->dt.esr_tight_flt_upct = DEFAULT_ESR_TIGHT_FLT_UPCT;
	else
		chip->dt.esr_tight_flt_upct = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-broad-filter-micro-pct",
			&temp);
	if (rc < 0)
		chip->dt.esr_broad_flt_upct = DEFAULT_ESR_BROAD_FLT_UPCT;
	else
		chip->dt.esr_broad_flt_upct = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-tight-lt-filter-micro-pct",
			&temp);
	if (rc < 0)
		chip->dt.esr_tight_lt_flt_upct = DEFAULT_ESR_TIGHT_LT_FLT_UPCT;
	else
		chip->dt.esr_tight_lt_flt_upct = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-broad-lt-filter-micro-pct",
			&temp);
	if (rc < 0)
		chip->dt.esr_broad_lt_flt_upct = DEFAULT_ESR_BROAD_LT_FLT_UPCT;
	else
		chip->dt.esr_broad_lt_flt_upct = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-rt-filter-switch-temp",
			&temp);
	if (rc < 0)
		chip->dt.esr_flt_rt_switch_temp = DEFAULT_ESR_FLT_RT_DECIDEGC;
	else
		chip->dt.esr_flt_rt_switch_temp = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-tight-rt-filter-micro-pct",
			&temp);
	if (rc < 0)
		chip->dt.esr_tight_rt_flt_upct = DEFAULT_ESR_TIGHT_RT_FLT_UPCT;
	else
		chip->dt.esr_tight_rt_flt_upct = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-broad-rt-filter-micro-pct",
			&temp);
	if (rc < 0)
		chip->dt.esr_broad_rt_flt_upct = DEFAULT_ESR_BROAD_RT_FLT_UPCT;
	else
		chip->dt.esr_broad_rt_flt_upct = temp;

	rc = fg_parse_slope_limit_coefficients(fg);
	if (rc < 0)
		pr_err("Error in parsing slope limit coeffs, rc=%d\n", rc);

	rc = of_property_read_u32(node, "qcom,fg-esr-clamp-mohms", &temp);
	if (rc < 0)
		chip->dt.esr_clamp_mohms = DEFAULT_ESR_CLAMP_MOHMS;
	else
		chip->dt.esr_clamp_mohms = temp;

	chip->dt.esr_pulse_thresh_ma = DEFAULT_ESR_PULSE_THRESH_MA;
	rc = of_property_read_u32(node, "qcom,fg-esr-pulse-thresh-ma", &temp);
	if (!rc) {
		/* ESR pulse qualification threshold range is 1-997 mA */
		if (temp > 0 && temp < 997)
			chip->dt.esr_pulse_thresh_ma = temp;
	}

	chip->dt.esr_meas_curr_ma = DEFAULT_ESR_MEAS_CURR_MA;
	rc = of_property_read_u32(node, "qcom,fg-esr-meas-curr-ma", &temp);
	if (!rc) {
		/* ESR measurement current range is 60-240 mA */
		if (temp >= 60 || temp <= 240)
			chip->dt.esr_meas_curr_ma = temp;
	}

	chip->dt.bmd_en_delay_ms = DEFAULT_BMD_EN_DELAY_MS;
	rc = of_property_read_u32(node, "qcom,fg-bmd-en-delay-ms", &temp);
	if (!rc) {
		if (temp > DEFAULT_BMD_EN_DELAY_MS)
			chip->dt.bmd_en_delay_ms = temp;
	}

	chip->dt.sync_sleep_threshold_ma = -EINVAL;
	rc = of_property_read_u32(node,
		"qcom,fg-sync-sleep-threshold-ma", &temp);
	if (!rc) {
		if (temp >= 0 && temp < 997)
			chip->dt.sync_sleep_threshold_ma = temp;
	}

	chip->dt.use_esr_sw = of_property_read_bool(node, "qcom,fg-use-sw-esr");

	chip->dt.disable_esr_pull_dn = of_property_read_bool(node,
					"qcom,fg-disable-esr-pull-dn");

	chip->dt.disable_fg_twm = of_property_read_bool(node,
					"qcom,fg-disable-in-twm");

	return 0;
}

static void fg_cleanup(struct fg_gen3_chip *chip)
{
	struct fg_dev *fg = &chip->fg;

	fg_unregister_interrupts(fg, chip, FG_GEN3_IRQ_MAX);
	alarm_try_to_cancel(&fg->esr_filter_alarm);
	power_supply_unreg_notifier(&fg->nb);
	debugfs_remove_recursive(fg->dfs_root);
	if (fg->awake_votable)
		destroy_votable(fg->awake_votable);

	if (fg->delta_bsoc_irq_en_votable)
		destroy_votable(fg->delta_bsoc_irq_en_votable);

	if (fg->batt_miss_irq_en_votable)
		destroy_votable(fg->batt_miss_irq_en_votable);

	if (chip->batt_id_chan)
		iio_channel_release(chip->batt_id_chan);

	dev_set_drvdata(fg->dev, NULL);
}

static int fg_gen3_probe(struct platform_device *pdev)
{
	struct fg_gen3_chip *chip;
	struct fg_dev *fg;
	struct power_supply_config fg_psy_cfg;
	int rc, msoc, volt_uv, batt_temp;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	fg = &chip->fg;
	fg->dev = &pdev->dev;
	fg->debug_mask = &fg_gen3_debug_mask;
	fg->irqs = fg_irqs;
	fg->charge_status = -EINVAL;
	fg->prev_charge_status = -EINVAL;
	fg->online_status = -EINVAL;
	fg->batt_id_ohms = -EINVAL;
	chip->ki_coeff_full_soc = -EINVAL;
	fg->regmap = dev_get_regmap(fg->dev->parent, NULL);
	if (!fg->regmap) {
		dev_err(fg->dev, "Parent regmap is unavailable\n");
		return -ENXIO;
	}

	chip->batt_id_chan = iio_channel_get(fg->dev, "rradc_batt_id");
	if (IS_ERR(chip->batt_id_chan)) {
		if (PTR_ERR(chip->batt_id_chan) != -EPROBE_DEFER)
			pr_err("batt_id_chan unavailable %ld\n",
				PTR_ERR(chip->batt_id_chan));
		rc = PTR_ERR(chip->batt_id_chan);
		chip->batt_id_chan = NULL;
		return rc;
	}

	rc = of_property_match_string(fg->dev->of_node,
				"io-channel-names", "rradc_die_temp");
	if (rc >= 0) {
		chip->die_temp_chan = iio_channel_get(fg->dev,
						"rradc_die_temp");
		if (IS_ERR(chip->die_temp_chan)) {
			if (PTR_ERR(chip->die_temp_chan) != -EPROBE_DEFER)
				pr_err("rradc_die_temp unavailable %ld\n",
					PTR_ERR(chip->die_temp_chan));
			rc = PTR_ERR(chip->die_temp_chan);
			chip->die_temp_chan = NULL;
			return rc;
		}
	}

	chip->pl_disable_votable = find_votable("PL_DISABLE");
	if (chip->pl_disable_votable == NULL) {
		rc = -EPROBE_DEFER;
		goto exit;
	}

	fg->awake_votable = create_votable("FG_WS", VOTE_SET_ANY, fg_awake_cb,
					chip);
	if (IS_ERR(fg->awake_votable)) {
		rc = PTR_ERR(fg->awake_votable);
		fg->awake_votable = NULL;
		goto exit;
	}

	fg->delta_bsoc_irq_en_votable = create_votable("FG_DELTA_BSOC_IRQ",
						VOTE_SET_ANY,
						fg_delta_bsoc_irq_en_cb, fg);
	if (IS_ERR(fg->delta_bsoc_irq_en_votable)) {
		rc = PTR_ERR(fg->delta_bsoc_irq_en_votable);
		fg->delta_bsoc_irq_en_votable = NULL;
		goto exit;
	}

	fg->batt_miss_irq_en_votable = create_votable("FG_BATT_MISS_IRQ",
						VOTE_SET_ANY,
						fg_batt_miss_irq_en_cb, fg);
	if (IS_ERR(fg->batt_miss_irq_en_votable)) {
		rc = PTR_ERR(fg->batt_miss_irq_en_votable);
		fg->batt_miss_irq_en_votable = NULL;
		goto exit;
	}

	rc = fg_parse_dt(chip);
	if (rc < 0) {
		dev_err(fg->dev, "Error in reading DT parameters, rc:%d\n",
			rc);
		goto exit;
	}

	mutex_init(&fg->bus_lock);
	mutex_init(&fg->sram_rw_lock);
	mutex_init(&fg->charge_full_lock);
	mutex_init(&chip->cyc_ctr.lock);
	mutex_init(&chip->cl.lock);
	mutex_init(&chip->ttf.lock);
	mutex_init(&fg->qnovo_esr_ctrl_lock);
	spin_lock_init(&fg->suspend_lock);
	spin_lock_init(&fg->awake_lock);
	init_completion(&fg->soc_update);
	init_completion(&fg->soc_ready);
	INIT_DELAYED_WORK(&fg->profile_load_work, profile_load_work);
	INIT_DELAYED_WORK(&chip->pl_enable_work, pl_enable_work);
	INIT_WORK(&fg->status_change_work, status_change_work);
	INIT_WORK(&fg->esr_sw_work, fg_esr_sw_work);
	INIT_DELAYED_WORK(&chip->ttf_work, ttf_work);
	INIT_DELAYED_WORK(&fg->sram_dump_work, sram_dump_work);
	INIT_WORK(&fg->esr_filter_work, esr_filter_work);
	alarm_init(&fg->esr_filter_alarm, ALARM_BOOTTIME,
			fg_esr_filter_alarm_cb);

	rc = fg_memif_init(fg);
	if (rc < 0) {
		dev_err(fg->dev, "Error in initializing FG_MEMIF, rc:%d\n",
			rc);
		goto exit;
	}

	platform_set_drvdata(pdev, chip);

	rc = fg_hw_init(fg);
	if (rc < 0) {
		dev_err(fg->dev, "Error in initializing FG hardware, rc:%d\n",
			rc);
		goto exit;
	}

	if (chip->dt.use_esr_sw) {
		if (alarmtimer_get_rtcdev()) {
			alarm_init(&fg->esr_sw_timer, ALARM_BOOTTIME,
				fg_esr_sw_timer);
		} else {
			pr_err("Failed to get esw_sw alarm-timer\n");
			/* RTC always registers, hence defer until it passes */
			rc = -EPROBE_DEFER;
			goto exit;
		}
		if (chip->dt.esr_timer_charging[TIMER_MAX] != -EINVAL)
			fg->esr_wakeup_ms =
				chip->dt.esr_timer_charging[TIMER_MAX] * 1460;
		else
			fg->esr_wakeup_ms = 140000;	/* 140 seconds */
	}

	/* Register the power supply */
	fg_psy_cfg.drv_data = chip;
	fg_psy_cfg.of_node = NULL;
	fg_psy_cfg.supplied_to = NULL;
	fg_psy_cfg.num_supplicants = 0;
	fg->fg_psy = devm_power_supply_register(fg->dev, &fg_psy_desc,
			&fg_psy_cfg);
	if (IS_ERR(fg->fg_psy)) {
		pr_err("failed to register fg_psy rc = %ld\n",
				PTR_ERR(fg->fg_psy));
		goto exit;
	}

	fg->nb.notifier_call = fg_notifier_cb;
	rc = power_supply_reg_notifier(&fg->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		goto exit;
	}

	fg->twm_nb.notifier_call = twm_notifier_cb;
	rc = qpnp_misc_twm_notifier_register(&fg->twm_nb);
	if (rc < 0)
		pr_err("Failed to register twm_notifier_cb rc=%d\n", rc);

	rc = fg_register_interrupts(&chip->fg, FG_GEN3_IRQ_MAX);
	if (rc < 0) {
		dev_err(fg->dev, "Error in registering interrupts, rc:%d\n",
			rc);
		goto exit;
	}

	/* Keep SOC_UPDATE irq disabled until we require it */
	if (fg->irqs[SOC_UPDATE_IRQ].irq)
		disable_irq_nosync(fg->irqs[SOC_UPDATE_IRQ].irq);

	/* Keep BSOC_DELTA_IRQ disabled until we require it */
	vote(fg->delta_bsoc_irq_en_votable, DELTA_BSOC_IRQ_VOTER, false, 0);

	/* Keep BATT_MISSING_IRQ disabled until we require it */
	vote(fg->batt_miss_irq_en_votable, BATT_MISS_IRQ_VOTER, false, 0);

	rc = fg_debugfs_create(fg);
	if (rc < 0) {
		dev_err(fg->dev, "Error in creating debugfs entries, rc:%d\n",
			rc);
		goto exit;
	}

	rc = fg_get_battery_voltage(fg, &volt_uv);
	if (!rc)
		rc = fg_get_prop_capacity(fg, &msoc);

	if (!rc)
		rc = fg_get_battery_temp(fg, &batt_temp);

	if (!rc) {
		pr_info("battery SOC:%d voltage: %duV temp: %d\n",
				msoc, volt_uv, batt_temp);
		rc = fg_esr_filter_config(fg, batt_temp, false);
		if (rc < 0)
			pr_err("Error in configuring ESR filter rc:%d\n", rc);
	}

	device_init_wakeup(fg->dev, true);
	schedule_delayed_work(&fg->profile_load_work, 0);

	pr_debug("FG GEN3 driver probed successfully\n");
	return 0;
exit:
	fg_cleanup(chip);
	return rc;
}

static int fg_gen3_suspend(struct device *dev)
{
	struct fg_gen3_chip *chip = dev_get_drvdata(dev);
	struct fg_dev *fg = &chip->fg;
	int rc;

	spin_lock(&fg->suspend_lock);
	fg->suspended = true;
	spin_unlock(&fg->suspend_lock);

	rc = fg_esr_timer_config(fg, true);
	if (rc < 0)
		pr_err("Error in configuring ESR timer, rc=%d\n", rc);

	cancel_delayed_work_sync(&chip->ttf_work);
	if (fg_sram_dump)
		cancel_delayed_work_sync(&fg->sram_dump_work);
	return 0;
}

static int fg_gen3_resume(struct device *dev)
{
	struct fg_gen3_chip *chip = dev_get_drvdata(dev);
	struct fg_dev *fg = &chip->fg;
	int rc;

	rc = fg_esr_timer_config(fg, false);
	if (rc < 0)
		pr_err("Error in configuring ESR timer, rc=%d\n", rc);

	schedule_delayed_work(&chip->ttf_work, 0);
	if (fg_sram_dump)
		schedule_delayed_work(&fg->sram_dump_work,
				msecs_to_jiffies(fg_sram_dump_period_ms));

	if (!work_pending(&fg->status_change_work)) {
		pm_stay_awake(fg->dev);
		schedule_work(&fg->status_change_work);
	}

	spin_lock(&fg->suspend_lock);
	fg->suspended = false;
	spin_unlock(&fg->suspend_lock);

	return 0;
}

static const struct dev_pm_ops fg_gen3_pm_ops = {
	.suspend	= fg_gen3_suspend,
	.resume		= fg_gen3_resume,
};

static int fg_gen3_remove(struct platform_device *pdev)
{
	struct fg_gen3_chip *chip = dev_get_drvdata(&pdev->dev);

	fg_cleanup(chip);
	return 0;
}

static void fg_gen3_shutdown(struct platform_device *pdev)
{
	struct fg_gen3_chip *chip = dev_get_drvdata(&pdev->dev);
	struct fg_dev *fg = &chip->fg;
	int rc, bsoc;
	u8 mask;

	if (fg->charge_full) {
		rc = fg_get_sram_prop(fg, FG_SRAM_BATT_SOC, &bsoc);
		if (rc < 0) {
			pr_err("Error in getting BATT_SOC, rc=%d\n", rc);
			return;
		}

		/* We need 2 most significant bytes here */
		bsoc = (u32)bsoc >> 16;

		rc = fg_configure_full_soc(fg, bsoc);
		if (rc < 0) {
			pr_err("Error in configuring full_soc, rc=%d\n", rc);
			return;
		}
	}
	rc = fg_set_esr_timer(fg, chip->dt.esr_timer_shutdown[TIMER_RETRY],
				chip->dt.esr_timer_shutdown[TIMER_MAX], false,
				FG_IMA_NO_WLOCK);
	if (rc < 0)
		pr_err("Error in setting ESR timer at shutdown, rc=%d\n", rc);

	if (fg->twm_state == PMIC_TWM_ENABLE && chip->dt.disable_fg_twm) {
		rc = fg_masked_write(fg, BATT_SOC_EN_CTL(fg),
					FG_ALGORITHM_EN_BIT, 0);
		if (rc < 0)
			pr_err("Error in disabling FG rc=%d\n", rc);

		mask = BCL_RST_BIT | MEM_RST_BIT | ALG_RST_BIT;
		rc = fg_masked_write(fg, BATT_SOC_RST_CTRL0(fg),
					mask, mask);
		if (rc < 0)
			pr_err("Error in disabling FG resets rc=%d\n", rc);
	}

	fg_cleanup(chip);
}

static const struct of_device_id fg_gen3_match_table[] = {
	{.compatible = FG_GEN3_DEV_NAME},
	{},
};

static struct platform_driver fg_gen3_driver = {
	.driver = {
		.name = FG_GEN3_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = fg_gen3_match_table,
		.pm		= &fg_gen3_pm_ops,
	},
	.probe		= fg_gen3_probe,
	.remove		= fg_gen3_remove,
	.shutdown	= fg_gen3_shutdown,
};

static int __init fg_gen3_init(void)
{
	return platform_driver_register(&fg_gen3_driver);
}

static void __exit fg_gen3_exit(void)
{
	return platform_driver_unregister(&fg_gen3_driver);
}

module_init(fg_gen3_init);
module_exit(fg_gen3_exit);

MODULE_DESCRIPTION("QPNP Fuel gauge GEN3 driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" FG_GEN3_DEV_NAME);
