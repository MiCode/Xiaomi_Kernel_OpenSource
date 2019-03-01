/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include <linux/alarmtimer.h>
#include <linux/irq.h>
#include <linux/ktime.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_batterydata.h>
#include <linux/platform_device.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/thermal.h>
#include "fg-core.h"
#include "fg-reg.h"
#include "fg-alg.h"

#define FG_GEN4_DEV_NAME	"qcom,fg-gen4"
#define TTF_AWAKE_VOTER		"fg_ttf_awake"

#define PERPH_SUBTYPE_REG		0x05
#define FG_BATT_SOC_PM8150B		0x10
#define FG_BATT_INFO_PM8150B		0x11
#define FG_MEM_IF_PM8150B		0x0D
#define FG_ADC_RR_PM8150B		0x13

#define FG_SRAM_LEN			972
#define PROFILE_LEN			416
#define PROFILE_COMP_LEN		24
#define KI_COEFF_SOC_LEVELS		3
#define ESR_CAL_LEVELS			2
#define KI_COEFF_MAX			15564
#define SLOPE_LIMIT_NUM_COEFFS		4
#define SLOPE_LIMIT_COEFF_MAX		31128
#define BATT_THERM_NUM_COEFFS		5
#define RSLOW_NUM_COEFFS		4

/* SRAM address/offset definitions in ascending order */
#define BATT_THERM_CONFIG_WORD		3
#define RATIO_CENTER_OFFSET		1
#define BATT_THERM_COEFF_WORD		4
#define BATT_THERM_COEFF_OFFSET		0
#define BATT_TEMP_CONFIG_WORD		9
#define BATT_TEMP_HOT_OFFSET		0
#define BATT_TEMP_COLD_OFFSET		1
#define BATT_TEMP_CONFIG2_WORD		10
#define BATT_TEMP_HYST_DELTA_OFFSET	0
#define ESR_CAL_SOC_MIN_OFFSET		1
#define ESR_CAL_THRESH_WORD		11
#define ESR_CAL_SOC_MAX_OFFSET		0
#define ESR_CAL_TEMP_MIN_OFFSET		1
#define ESR_PULSE_THRESH_WORD		12
#define ESR_CAL_TEMP_MAX_OFFSET		0
#define ESR_PULSE_THRESH_OFFSET		1
#define DELTA_ESR_THR_WORD		14
#define DELTA_ESR_THR_OFFSET		0
#define ESR_TIMER_DISCHG_MAX_WORD	17
#define ESR_TIMER_DISCHG_MAX_OFFSET	0
#define ESR_TIMER_DISCHG_INIT_WORD	17
#define ESR_TIMER_DISCHG_INIT_OFFSET	1
#define ESR_TIMER_CHG_MAX_WORD		18
#define ESR_TIMER_CHG_MAX_OFFSET	0
#define ESR_TIMER_CHG_INIT_WORD		18
#define ESR_TIMER_CHG_INIT_OFFSET	1
#define CUTOFF_CURR_WORD		19
#define CUTOFF_CURR_OFFSET		0
#define CUTOFF_VOLT_WORD		20
#define CUTOFF_VOLT_OFFSET		0
#define SYS_TERM_CURR_WORD		22
#define SYS_TERM_CURR_OFFSET		0
#define VBATT_FULL_WORD			23
#define VBATT_FULL_OFFSET		0
#define KI_COEFF_FULL_SOC_NORM_WORD	24
#define KI_COEFF_FULL_SOC_NORM_OFFSET	1
#define KI_COEFF_LOW_DISCHG_WORD	25
#define KI_COEFF_FULL_SOC_LOW_OFFSET	0
#define KI_COEFF_LOW_DISCHG_OFFSET	1
#define KI_COEFF_MED_DISCHG_WORD	26
#define KI_COEFF_MED_DISCHG_OFFSET	0
#define KI_COEFF_HI_DISCHG_WORD		26
#define KI_COEFF_HI_DISCHG_OFFSET	1
#define KI_COEFF_LOW_CHG_WORD		28
#define KI_COEFF_LOW_CHG_OFFSET		0
#define KI_COEFF_MED_CHG_WORD		28
#define KI_COEFF_MED_CHG_OFFSET		1
#define KI_COEFF_HI_CHG_WORD		29
#define KI_COEFF_HI_CHG_OFFSET		0
#define DELTA_BSOC_THR_WORD		30
#define DELTA_BSOC_THR_OFFSET		1
#define SLOPE_LIMIT_WORD		32
#define SLOPE_LIMIT_OFFSET		0
#define DELTA_MSOC_THR_WORD		32
#define DELTA_MSOC_THR_OFFSET		1
#define VBATT_LOW_WORD			35
#define VBATT_LOW_OFFSET		1
#define SYS_CONFIG_WORD			60
#define SYS_CONFIG_OFFSET		0
#define SYS_CONFIG2_OFFSET		1
#define PROFILE_LOAD_WORD		65
#define PROFILE_LOAD_OFFSET		0
#define RSLOW_COEFF_DISCHG_WORD		78
#define RSLOW_COEFF_LOW_OFFSET		0
#define RSLOW_CONFIG_WORD		241
#define RSLOW_CONFIG_OFFSET		0
#define NOM_CAP_WORD			271
#define NOM_CAP_OFFSET			0
#define RCONN_WORD			275
#define RCONN_OFFSET			0
#define ACT_BATT_CAP_WORD		285
#define ACT_BATT_CAP_OFFSET		0
#define BATT_AGE_LEVEL_WORD		288
#define BATT_AGE_LEVEL_OFFSET		0
#define CYCLE_COUNT_WORD		291
#define CYCLE_COUNT_OFFSET		0
#define PROFILE_INTEGRITY_WORD		299
#define PROFILE_INTEGRITY_OFFSET	0
#define IBAT_FINAL_WORD			320
#define IBAT_FINAL_OFFSET		0
#define VBAT_FINAL_WORD			321
#define VBAT_FINAL_OFFSET		0
#define BATT_TEMP_WORD			328
#define BATT_TEMP_OFFSET		0
#define ESR_WORD			331
#define ESR_OFFSET			0
#define ESR_MDL_WORD			335
#define ESR_MDL_OFFSET			0
#define ESR_CHAR_WORD			336
#define ESR_CHAR_OFFSET			0
#define ESR_DELTA_DISCHG_WORD		340
#define ESR_DELTA_DISCHG_OFFSET		0
#define ESR_DELTA_CHG_WORD		341
#define ESR_DELTA_CHG_OFFSET		0
#define ESR_ACT_WORD			342
#define ESR_ACT_OFFSET			0
#define RSLOW_WORD			368
#define RSLOW_OFFSET			0
#define OCV_WORD			417
#define OCV_OFFSET			0
#define VOLTAGE_PRED_WORD		432
#define VOLTAGE_PRED_OFFSET		0
#define BATT_SOC_WORD			449
#define BATT_SOC_OFFSET			0
#define FULL_SOC_WORD			455
#define FULL_SOC_OFFSET			0
#define CC_SOC_SW_WORD			458
#define CC_SOC_SW_OFFSET		0
#define CC_SOC_WORD			460
#define CC_SOC_OFFSET			0
#define MONOTONIC_SOC_WORD		463
#define MONOTONIC_SOC_OFFSET		0

/* v2 SRAM address and offset in ascending order */
#define RSLOW_SCALE_FN_DISCHG_V2_WORD	281
#define RSLOW_SCALE_FN_DISCHG_V2_OFFSET	0
#define RSLOW_SCALE_FN_CHG_V2_WORD	285
#define RSLOW_SCALE_FN_CHG_V2_OFFSET	0
#define ACT_BATT_CAP_v2_WORD		287
#define ACT_BATT_CAP_v2_OFFSET		0
#define RSLOW_v2_WORD			371
#define RSLOW_v2_OFFSET			0
#define OCV_v2_WORD			425
#define OCV_v2_OFFSET			0
#define VOLTAGE_PRED_v2_WORD		440
#define VOLTAGE_PRED_v2_OFFSET		0
#define BATT_SOC_v2_WORD		455
#define BATT_SOC_v2_OFFSET		0
#define FULL_SOC_v2_WORD		461
#define FULL_SOC_v2_OFFSET		0
#define CC_SOC_SW_v2_WORD		464
#define CC_SOC_SW_v2_OFFSET		0
#define CC_SOC_v2_WORD			466
#define CC_SOC_v2_OFFSET		0
#define MONOTONIC_SOC_v2_WORD		469
#define MONOTONIC_SOC_v2_OFFSET		0
#define FIRST_LOG_CURRENT_v2_WORD	471
#define FIRST_LOG_CURRENT_v2_OFFSET	0

static struct fg_irq_info fg_irqs[FG_GEN4_IRQ_MAX];

/* DT parameters for FG device */
struct fg_dt_props {
	bool	force_load_profile;
	bool	hold_soc_while_full;
	bool	linearize_soc;
	bool	rapid_soc_dec_en;
	bool	five_pin_battery;
	bool	esr_calib_dischg;
	bool	multi_profile_load;
	int	cutoff_volt_mv;
	int	empty_volt_mv;
	int	cutoff_curr_ma;
	int	sys_term_curr_ma;
	int	delta_soc_thr;
	int	esr_timer_chg_fast[NUM_ESR_TIMERS];
	int	esr_timer_chg_slow[NUM_ESR_TIMERS];
	int	esr_timer_dischg_fast[NUM_ESR_TIMERS];
	int	esr_timer_dischg_slow[NUM_ESR_TIMERS];
	u32	esr_cal_soc_thresh[ESR_CAL_LEVELS];
	int	esr_cal_temp_thresh[ESR_CAL_LEVELS];
	int	esr_filter_factor;
	int	delta_esr_disable_count;
	int	delta_esr_thr_uohms;
	int	rconn_uohms;
	int	batt_temp_cold_thresh;
	int	batt_temp_hot_thresh;
	int	batt_temp_hyst;
	int	batt_temp_delta;
	u32	batt_therm_freq;
	int	esr_pulse_thresh_ma;
	int	esr_meas_curr_ma;
	int	slope_limit_temp;
	int	ki_coeff_low_chg;
	int	ki_coeff_med_chg;
	int	ki_coeff_hi_chg;
	int	ki_coeff_full_soc_dischg[2];
	int	ki_coeff_soc[KI_COEFF_SOC_LEVELS];
	int	ki_coeff_low_dischg[KI_COEFF_SOC_LEVELS];
	int	ki_coeff_med_dischg[KI_COEFF_SOC_LEVELS];
	int	ki_coeff_hi_dischg[KI_COEFF_SOC_LEVELS];
	int	slope_limit_coeffs[SLOPE_LIMIT_NUM_COEFFS];
};

struct fg_gen4_chip {
	struct fg_dev		fg;
	struct fg_dt_props	dt;
	struct cycle_counter	*counter;
	struct cap_learning	*cl;
	struct ttf		*ttf;
	struct votable		*delta_esr_irq_en_votable;
	struct votable		*pl_disable_votable;
	struct votable		*cp_disable_votable;
	struct votable		*parallel_current_en_votable;
	struct votable		*mem_attn_irq_en_votable;
	struct work_struct	esr_calib_work;
	struct alarm		esr_fast_cal_timer;
	struct delayed_work	pl_enable_work;
	struct work_struct	pl_current_en_work;
	struct completion	mem_attn;
	char			batt_profile[PROFILE_LEN];
	enum slope_limit_status	slope_limit_sts;
	int			ki_coeff_full_soc[2];
	int			delta_esr_count;
	int			recharge_soc_thr;
	int			esr_actual;
	int			esr_nominal;
	int			soh;
	int			esr_soh_cycle_count;
	int			batt_age_level;
	int			last_batt_age_level;
	bool			first_profile_load;
	bool			ki_coeff_dischg_en;
	bool			slope_limit_en;
	bool			esr_fast_calib;
	bool			esr_fast_calib_done;
	bool			esr_fast_cal_timer_expired;
	bool			esr_fast_calib_retry;
	bool			esr_fcc_ctrl_en;
	bool			esr_soh_notified;
	bool			rslow_low;
	bool			rapid_soc_dec_en;
	bool			vbatt_low;
};

struct bias_config {
	u8	status_reg;
	u8	lsb_reg;
	int	bias_kohms;
};

static int fg_gen4_debug_mask;
module_param_named(
	debug_mask, fg_gen4_debug_mask, int, 0600
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
static bool fg_esr_fast_cal_en;

static struct fg_sram_param pm8150b_v1_sram_params[] = {
	PARAM(BATT_SOC, BATT_SOC_WORD, BATT_SOC_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_default),
	PARAM(FULL_SOC, FULL_SOC_WORD, FULL_SOC_OFFSET, 2, 1, 1, 0,
		fg_encode_default, fg_decode_default),
	PARAM(MONOTONIC_SOC, MONOTONIC_SOC_WORD, MONOTONIC_SOC_OFFSET, 2, 1, 1,
		0, NULL, fg_decode_default),
	PARAM(VOLTAGE_PRED, VOLTAGE_PRED_WORD, VOLTAGE_PRED_OFFSET, 2, 1000,
		244141, 0, NULL, fg_decode_voltage_15b),
	PARAM(OCV, OCV_WORD, OCV_OFFSET, 2, 1000, 244141, 0, NULL,
		fg_decode_voltage_15b),
	PARAM(VBAT_FINAL, VBAT_FINAL_WORD, VBAT_FINAL_OFFSET, 2, 1000, 244141,
		0, NULL, fg_decode_voltage_15b),
	PARAM(IBAT_FINAL, IBAT_FINAL_WORD, IBAT_FINAL_OFFSET, 2, 1000, 488282,
		0, NULL, fg_decode_current_16b),
	PARAM(ESR, ESR_WORD, ESR_OFFSET, 2, 1000, 244141, 0, fg_encode_default,
		fg_decode_value_16b),
	PARAM(ESR_MDL, ESR_MDL_WORD, ESR_MDL_OFFSET, 2, 1000, 244141, 0,
		fg_encode_default, fg_decode_value_16b),
	PARAM(ESR_ACT, ESR_ACT_WORD, ESR_ACT_OFFSET, 2, 1000, 244141, 0,
		fg_encode_default, fg_decode_value_16b),
	PARAM(RSLOW, RSLOW_WORD, RSLOW_OFFSET, 2, 1000, 244141, 0, NULL,
		fg_decode_value_16b),
	PARAM(CC_SOC, CC_SOC_WORD, CC_SOC_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_cc_soc),
	PARAM(CC_SOC_SW, CC_SOC_SW_WORD, CC_SOC_SW_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_cc_soc),
	PARAM(ACT_BATT_CAP, ACT_BATT_CAP_WORD, ACT_BATT_CAP_OFFSET, 2,
		1, 1, 0, NULL, fg_decode_default),
	/* Entries below here are configurable during initialization */
	PARAM(CUTOFF_VOLT, CUTOFF_VOLT_WORD, CUTOFF_VOLT_OFFSET, 2, 1000000,
		244141, 0, fg_encode_voltage, NULL),
	PARAM(VBATT_LOW, VBATT_LOW_WORD, VBATT_LOW_OFFSET, 1, 1000,
		15625, -2000, fg_encode_voltage, NULL),
	PARAM(VBATT_FULL, VBATT_FULL_WORD, VBATT_FULL_OFFSET, 2, 1000,
		244141, 0, fg_encode_voltage, fg_decode_voltage_15b),
	PARAM(CUTOFF_CURR, CUTOFF_CURR_WORD, CUTOFF_CURR_OFFSET, 2,
		100000, 48828, 0, fg_encode_current, NULL),
	PARAM(SYS_TERM_CURR, SYS_TERM_CURR_WORD, SYS_TERM_CURR_OFFSET, 2,
		100000, 48828, 0, fg_encode_current, NULL),
	PARAM(DELTA_MSOC_THR, DELTA_MSOC_THR_WORD, DELTA_MSOC_THR_OFFSET,
		1, 2048, 1000, 0, fg_encode_default, NULL),
	PARAM(DELTA_BSOC_THR, DELTA_BSOC_THR_WORD, DELTA_BSOC_THR_OFFSET,
		1, 2048, 1000, 0, fg_encode_default, NULL),
	PARAM(ESR_TIMER_DISCHG_MAX, ESR_TIMER_DISCHG_MAX_WORD,
		ESR_TIMER_DISCHG_MAX_OFFSET, 1, 1, 1, 0, fg_encode_default,
		NULL),
	PARAM(ESR_TIMER_DISCHG_INIT, ESR_TIMER_DISCHG_INIT_WORD,
		ESR_TIMER_DISCHG_INIT_OFFSET, 1, 1, 1, 0, fg_encode_default,
		NULL),
	PARAM(ESR_TIMER_CHG_MAX, ESR_TIMER_CHG_MAX_WORD,
		ESR_TIMER_CHG_MAX_OFFSET, 1, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_TIMER_CHG_INIT, ESR_TIMER_CHG_INIT_WORD,
		ESR_TIMER_CHG_INIT_OFFSET, 1, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_PULSE_THRESH, ESR_PULSE_THRESH_WORD, ESR_PULSE_THRESH_OFFSET,
		1, 1000, 15625, 0, fg_encode_default, NULL),
	PARAM(DELTA_ESR_THR, DELTA_ESR_THR_WORD, DELTA_ESR_THR_OFFSET, 2, 1000,
		61036, 0, fg_encode_default, NULL),
	PARAM(KI_COEFF_FULL_SOC, KI_COEFF_FULL_SOC_NORM_WORD,
		KI_COEFF_FULL_SOC_NORM_OFFSET, 1, 1000, 61035, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_LOW_DISCHG, KI_COEFF_LOW_DISCHG_WORD,
		KI_COEFF_LOW_DISCHG_OFFSET, 1, 1000, 61035, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_MED_DISCHG, KI_COEFF_MED_DISCHG_WORD,
		KI_COEFF_MED_DISCHG_OFFSET, 1, 1000, 61035, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_HI_DISCHG, KI_COEFF_HI_DISCHG_WORD,
		KI_COEFF_HI_DISCHG_OFFSET, 1, 1000, 61035, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_LOW_CHG, KI_COEFF_LOW_CHG_WORD, KI_COEFF_LOW_CHG_OFFSET,
		1, 1000, 61035, 0, fg_encode_default, NULL),
	PARAM(KI_COEFF_MED_CHG, KI_COEFF_MED_CHG_WORD, KI_COEFF_MED_CHG_OFFSET,
		1, 1000, 61035, 0, fg_encode_default, NULL),
	PARAM(KI_COEFF_HI_CHG, KI_COEFF_HI_CHG_WORD, KI_COEFF_HI_CHG_OFFSET, 1,
		1000, 61035, 0, fg_encode_default, NULL),
	PARAM(SLOPE_LIMIT, SLOPE_LIMIT_WORD, SLOPE_LIMIT_OFFSET, 1, 8192,
		1000000, 0, fg_encode_default, NULL),
	PARAM(BATT_TEMP_COLD, BATT_TEMP_CONFIG_WORD, BATT_TEMP_COLD_OFFSET, 1,
		1, 1, 0, fg_encode_default, NULL),
	PARAM(BATT_TEMP_HOT, BATT_TEMP_CONFIG_WORD, BATT_TEMP_HOT_OFFSET, 1,
		1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_CAL_SOC_MIN, BATT_TEMP_CONFIG2_WORD, ESR_CAL_SOC_MIN_OFFSET,
		1, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_CAL_SOC_MAX, ESR_CAL_THRESH_WORD, ESR_CAL_SOC_MAX_OFFSET,
		1, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_CAL_TEMP_MIN, ESR_CAL_THRESH_WORD, ESR_CAL_TEMP_MIN_OFFSET,
		1, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_CAL_TEMP_MAX, ESR_PULSE_THRESH_WORD, ESR_CAL_TEMP_MAX_OFFSET,
		1, 1, 1, 0, fg_encode_default, NULL),
};

static struct fg_sram_param pm8150b_v2_sram_params[] = {
	PARAM(BATT_SOC, BATT_SOC_v2_WORD, BATT_SOC_v2_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_default),
	PARAM(FULL_SOC, FULL_SOC_v2_WORD, FULL_SOC_v2_OFFSET, 2, 1, 1, 0,
		fg_encode_default, fg_decode_default),
	PARAM(MONOTONIC_SOC, MONOTONIC_SOC_v2_WORD, MONOTONIC_SOC_v2_OFFSET, 2,
		1, 1, 0, NULL, fg_decode_default),
	PARAM(VOLTAGE_PRED, VOLTAGE_PRED_v2_WORD, VOLTAGE_PRED_v2_OFFSET, 2,
		1000, 244141, 0, NULL, fg_decode_voltage_15b),
	PARAM(OCV, OCV_v2_WORD, OCV_v2_OFFSET, 2, 1000, 244141, 0, NULL,
		fg_decode_voltage_15b),
	PARAM(VBAT_FINAL, VBAT_FINAL_WORD, VBAT_FINAL_OFFSET, 2, 1000, 244141,
		0, NULL, fg_decode_voltage_15b),
	PARAM(IBAT_FINAL, IBAT_FINAL_WORD, IBAT_FINAL_OFFSET, 2, 1000, 488282,
		0, NULL, fg_decode_current_16b),
	PARAM(ESR, ESR_WORD, ESR_OFFSET, 2, 1000, 244141, 0, fg_encode_default,
		fg_decode_value_16b),
	PARAM(ESR_MDL, ESR_MDL_WORD, ESR_MDL_OFFSET, 2, 1000, 244141, 0,
		fg_encode_default, fg_decode_value_16b),
	PARAM(ESR_ACT, ESR_ACT_WORD, ESR_ACT_OFFSET, 2, 1000, 244141, 0,
		fg_encode_default, fg_decode_value_16b),
	PARAM(RSLOW, RSLOW_v2_WORD, RSLOW_v2_OFFSET, 2, 1000, 244141, 0, NULL,
		fg_decode_value_16b),
	PARAM(CC_SOC, CC_SOC_v2_WORD, CC_SOC_v2_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_cc_soc),
	PARAM(CC_SOC_SW, CC_SOC_SW_v2_WORD, CC_SOC_SW_v2_OFFSET, 4, 1, 1, 0,
		NULL, fg_decode_cc_soc),
	PARAM(ACT_BATT_CAP, ACT_BATT_CAP_v2_WORD, ACT_BATT_CAP_v2_OFFSET, 2,
		1, 1, 0, NULL, fg_decode_default),
	/* Entries below here are configurable during initialization */
	PARAM(CUTOFF_VOLT, CUTOFF_VOLT_WORD, CUTOFF_VOLT_OFFSET, 2, 1000000,
		244141, 0, fg_encode_voltage, NULL),
	PARAM(VBATT_LOW, VBATT_LOW_WORD, VBATT_LOW_OFFSET, 1, 1000,
		15625, -2000, fg_encode_voltage, NULL),
	PARAM(VBATT_FULL, VBATT_FULL_WORD, VBATT_FULL_OFFSET, 2, 1000,
		244141, 0, fg_encode_voltage, fg_decode_voltage_15b),
	PARAM(CUTOFF_CURR, CUTOFF_CURR_WORD, CUTOFF_CURR_OFFSET, 2,
		100000, 48828, 0, fg_encode_current, NULL),
	PARAM(SYS_TERM_CURR, SYS_TERM_CURR_WORD, SYS_TERM_CURR_OFFSET, 2,
		100000, 48828, 0, fg_encode_current, NULL),
	PARAM(DELTA_MSOC_THR, DELTA_MSOC_THR_WORD, DELTA_MSOC_THR_OFFSET,
		1, 2048, 1000, 0, fg_encode_default, NULL),
	PARAM(DELTA_BSOC_THR, DELTA_BSOC_THR_WORD, DELTA_BSOC_THR_OFFSET,
		1, 2048, 1000, 0, fg_encode_default, NULL),
	PARAM(ESR_TIMER_DISCHG_MAX, ESR_TIMER_DISCHG_MAX_WORD,
		ESR_TIMER_DISCHG_MAX_OFFSET, 1, 1, 1, 0, fg_encode_default,
		NULL),
	PARAM(ESR_TIMER_DISCHG_INIT, ESR_TIMER_DISCHG_INIT_WORD,
		ESR_TIMER_DISCHG_INIT_OFFSET, 1, 1, 1, 0, fg_encode_default,
		NULL),
	PARAM(ESR_TIMER_CHG_MAX, ESR_TIMER_CHG_MAX_WORD,
		ESR_TIMER_CHG_MAX_OFFSET, 1, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_TIMER_CHG_INIT, ESR_TIMER_CHG_INIT_WORD,
		ESR_TIMER_CHG_INIT_OFFSET, 1, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_PULSE_THRESH, ESR_PULSE_THRESH_WORD, ESR_PULSE_THRESH_OFFSET,
		1, 1000, 15625, 0, fg_encode_default, NULL),
	PARAM(DELTA_ESR_THR, DELTA_ESR_THR_WORD, DELTA_ESR_THR_OFFSET, 2, 1000,
		61036, 0, fg_encode_default, NULL),
	PARAM(KI_COEFF_FULL_SOC, KI_COEFF_FULL_SOC_NORM_WORD,
		KI_COEFF_FULL_SOC_NORM_OFFSET, 1, 1000, 61035, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_LOW_DISCHG, KI_COEFF_LOW_DISCHG_WORD,
		KI_COEFF_LOW_DISCHG_OFFSET, 1, 1000, 61035, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_MED_DISCHG, KI_COEFF_MED_DISCHG_WORD,
		KI_COEFF_MED_DISCHG_OFFSET, 1, 1000, 61035, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_HI_DISCHG, KI_COEFF_HI_DISCHG_WORD,
		KI_COEFF_HI_DISCHG_OFFSET, 1, 1000, 61035, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_LOW_CHG, KI_COEFF_LOW_CHG_WORD, KI_COEFF_LOW_CHG_OFFSET,
		1, 1000, 61035, 0, fg_encode_default, NULL),
	PARAM(KI_COEFF_MED_CHG, KI_COEFF_MED_CHG_WORD, KI_COEFF_MED_CHG_OFFSET,
		1, 1000, 61035, 0, fg_encode_default, NULL),
	PARAM(KI_COEFF_HI_CHG, KI_COEFF_HI_CHG_WORD, KI_COEFF_HI_CHG_OFFSET, 1,
		1000, 61035, 0, fg_encode_default, NULL),
	PARAM(SLOPE_LIMIT, SLOPE_LIMIT_WORD, SLOPE_LIMIT_OFFSET, 1, 8192,
		1000000, 0, fg_encode_default, NULL),
	PARAM(BATT_TEMP_COLD, BATT_TEMP_CONFIG_WORD, BATT_TEMP_COLD_OFFSET, 1,
		1, 1, 0, fg_encode_default, NULL),
	PARAM(BATT_TEMP_HOT, BATT_TEMP_CONFIG_WORD, BATT_TEMP_HOT_OFFSET, 1,
		1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_CAL_SOC_MIN, BATT_TEMP_CONFIG2_WORD, ESR_CAL_SOC_MIN_OFFSET,
		1, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_CAL_SOC_MAX, ESR_CAL_THRESH_WORD, ESR_CAL_SOC_MAX_OFFSET,
		1, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_CAL_TEMP_MIN, ESR_CAL_THRESH_WORD, ESR_CAL_TEMP_MIN_OFFSET,
		1, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_CAL_TEMP_MAX, ESR_PULSE_THRESH_WORD, ESR_CAL_TEMP_MAX_OFFSET,
		1, 1, 1, 0, fg_encode_default, NULL),
};

/* All get functions below */

struct bias_config id_table[3] = {
	{0x65, 0x66, 400},
	{0x6D, 0x6E, 100},
	{0x75, 0x76, 30},
};

#define MAX_BIAS_CODE	0x70E4
static int fg_gen4_get_batt_id(struct fg_gen4_chip *chip)
{
	struct fg_dev *fg = &chip->fg;
	int i, rc, batt_id_kohms;
	u16 tmp = 0, bias_code = 0, delta = 0;
	u8 val, bias_id = 0;

	for (i = 0; i < ARRAY_SIZE(id_table); i++)  {
		rc = fg_read(fg, fg->rradc_base + id_table[i].status_reg, &val,
				1);
		if (rc < 0) {
			pr_err("Failed to read bias_sts, rc=%d\n", rc);
			return rc;
		}

		if (val & BIAS_STS_READY) {
			rc = fg_read(fg, fg->rradc_base + id_table[i].lsb_reg,
					(u8 *)&tmp, 2);
			if (rc < 0) {
				pr_err("Failed to read bias_lsb_reg, rc=%d\n",
					rc);
				return rc;
			}
		}

		pr_debug("bias_code[%d]: 0x%04x\n", i, tmp);

		/*
		 * Bias code closer to MAX_BIAS_CODE/2 is the one which should
		 * be used for calculating battery id.
		 */
		if (!delta || abs(tmp - MAX_BIAS_CODE / 2) < delta) {
			bias_id = i;
			bias_code = tmp;
			delta = abs(tmp - MAX_BIAS_CODE / 2);
		}
	}

	pr_debug("bias_id: %d bias_code: 0x%04x\n", bias_id, bias_code);

	/*
	 * Following equation is used for calculating battery id.
	 * batt_id(KOhms) = bias_id(KOhms) / ((MAX_BIAS_CODE / bias_code) - 1)
	 */
	batt_id_kohms = (id_table[bias_id].bias_kohms * bias_code) * 10 /
			(MAX_BIAS_CODE - bias_code);
	fg->batt_id_ohms = (batt_id_kohms * 1000) / 10;
	return 0;
}

static int fg_gen4_get_nominal_capacity(struct fg_gen4_chip *chip,
					int64_t *nom_cap_uah)
{
	struct fg_dev *fg = &chip->fg;
	int rc;
	u8 buf[2];

	rc = fg_sram_read(fg, NOM_CAP_WORD, NOM_CAP_OFFSET, buf, 2,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading %04x[%d] rc=%d\n", NOM_CAP_WORD,
			NOM_CAP_OFFSET, rc);
		return rc;
	}

	*nom_cap_uah = (int)(buf[0] | buf[1] << 8) * 1000;
	fg_dbg(fg, FG_CAP_LEARN, "nom_cap_uah: %lld\n", *nom_cap_uah);
	return 0;
}

static int fg_gen4_get_learned_capacity(void *data, int64_t *learned_cap_uah)
{
	struct fg_gen4_chip *chip = data;
	struct fg_dev *fg;
	int rc, act_cap_mah;

	if (!chip)
		return -ENODEV;

	fg = &chip->fg;
	rc = fg_get_sram_prop(fg, FG_SRAM_ACT_BATT_CAP, &act_cap_mah);
	if (rc < 0) {
		pr_err("Error in getting ACT_BATT_CAP, rc=%d\n", rc);
		return rc;
	}

	*learned_cap_uah = act_cap_mah * 1000;

	fg_dbg(fg, FG_CAP_LEARN, "learned_cap_uah:%lld\n", *learned_cap_uah);
	return 0;
}

#define CC_SOC_30BIT	GENMASK(29, 0)
#define BATT_SOC_32BIT	GENMASK(31, 0)
static int fg_gen4_get_charge_raw(struct fg_gen4_chip *chip, int *val)
{
	int rc, cc_soc;
	int64_t nom_cap_uah;

	rc = fg_get_sram_prop(&chip->fg, FG_SRAM_CC_SOC, &cc_soc);
	if (rc < 0) {
		pr_err("Error in getting CC_SOC, rc=%d\n", rc);
		return rc;
	}

	rc = fg_gen4_get_nominal_capacity(chip, &nom_cap_uah);
	if (rc < 0) {
		pr_err("Error in getting nominal capacity, rc=%d\n", rc);
		return rc;
	}

	*val = div_s64(cc_soc * nom_cap_uah, CC_SOC_30BIT);
	return 0;
}

static int fg_gen4_get_charge_counter(struct fg_gen4_chip *chip, int *val)
{
	int rc, cc_soc;
	int64_t learned_cap_uah;

	rc = fg_get_sram_prop(&chip->fg, FG_SRAM_CC_SOC_SW, &cc_soc);
	if (rc < 0) {
		pr_err("Error in getting CC_SOC_SW, rc=%d\n", rc);
		return rc;
	}

	rc = fg_gen4_get_learned_capacity(chip, &learned_cap_uah);
	if (rc < 0) {
		pr_err("Error in getting learned capacity, rc=%d\n", rc);
		return rc;
	}

	*val = div_s64(cc_soc * learned_cap_uah, CC_SOC_30BIT);
	return 0;
}

static int fg_gen4_get_charge_counter_shadow(struct fg_gen4_chip *chip,
						int *val)
{
	int rc, batt_soc;
	int64_t learned_cap_uah;

	rc = fg_get_sram_prop(&chip->fg, FG_SRAM_BATT_SOC, &batt_soc);
	if (rc < 0) {
		pr_err("Error in getting BATT_SOC, rc=%d\n", rc);
		return rc;
	}

	rc = fg_gen4_get_learned_capacity(chip, &learned_cap_uah);
	if (rc < 0) {
		pr_err("Error in getting learned capacity, rc=%d\n", rc);
		return rc;
	}

	*val = div_u64((u32)batt_soc * learned_cap_uah, BATT_SOC_32BIT);
	return 0;
}

static int fg_gen4_get_battery_temp(struct fg_dev *fg, int *val)
{
	int rc = 0;
	u16 buf;

	rc = fg_sram_read(fg, BATT_TEMP_WORD, BATT_TEMP_OFFSET, (u8 *)&buf,
			2, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Failed to read BATT_TEMP_WORD rc=%d\n", rc);
		return rc;
	}

	/*
	 * 10 bits representing temperature from -128 to 127 and each LSB is
	 * 0.25 C. Multiply by 10 to convert it to deci degrees C.
	 */
	*val = sign_extend32(buf, 9) * 100 / 40;

	return 0;
}

static int fg_gen4_tz_get_temp(void *data, int *temperature)
{
	struct fg_dev *fg = (struct fg_dev *)data;
	int rc, temp;

	if (!temperature)
		return -EINVAL;

	rc = fg_gen4_get_battery_temp(fg, &temp);
	if (rc < 0)
		return rc;

	/* Convert deciDegC to milliDegC */
	*temperature = temp * 100;
	return rc;
}

static struct thermal_zone_of_device_ops fg_gen4_tz_ops = {
	.get_temp = fg_gen4_tz_get_temp,
};

static int fg_gen4_get_debug_batt_id(struct fg_dev *fg, int *batt_id)
{
	int rc;
	u16 tmp;

	rc = fg_read(fg, ADC_RR_FAKE_BATT_LOW_LSB(fg), (u8 *)&tmp, 2);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			ADC_RR_FAKE_BATT_LOW_LSB(fg), rc);
		return rc;
	}

	/*
	 * Following equation is used for calculating battery id.
	 * batt_id(KOhms) = 30000 / ((MAX_BIAS_CODE / bias_code) - 1)
	 */

	batt_id[0] = (30000 * tmp) / (MAX_BIAS_CODE - tmp);

	rc = fg_read(fg, ADC_RR_FAKE_BATT_HIGH_LSB(fg), (u8 *)&tmp, 2);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			ADC_RR_FAKE_BATT_HIGH_LSB(fg), rc);
		return rc;
	}

	batt_id[1] = (30000 * tmp) / (MAX_BIAS_CODE - tmp);
	pr_debug("debug batt_id range: [%d %d]\n", batt_id[0], batt_id[1]);
	return 0;
}

static bool is_debug_batt_id(struct fg_dev *fg)
{
	int debug_batt_id[2], rc;

	if (fg->batt_id_ohms < 0)
		return false;

	rc = fg_gen4_get_debug_batt_id(fg, debug_batt_id);
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

static int fg_gen4_get_cell_impedance(struct fg_gen4_chip *chip, int *val)
{
	struct fg_dev *fg = &chip->fg;
	int rc, esr_uohms, temp, vbat_term_mv, v_delta, rprot_uohms = 0;

	rc = fg_get_battery_resistance(fg, &esr_uohms);
	if (rc < 0)
		return rc;

	if (!chip->dt.five_pin_battery)
		goto out;

	if (fg->charge_type != POWER_SUPPLY_CHARGE_TYPE_TAPER ||
		fg->bp.float_volt_uv <= 0)
		goto out;

	rc = fg_get_battery_voltage(fg, &vbat_term_mv);
	if (rc < 0)
		goto out;

	rc = fg_get_sram_prop(fg, FG_SRAM_VBAT_FINAL, &temp);
	if (rc < 0) {
		pr_err("Error in getting VBAT_FINAL rc:%d\n", rc);
		goto out;
	}

	v_delta = abs(temp - fg->bp.float_volt_uv);

	rc = fg_get_sram_prop(fg, FG_SRAM_IBAT_FINAL, &temp);
	if (rc < 0) {
		pr_err("Error in getting IBAT_FINAL rc:%d\n", rc);
		goto out;
	}

	if (!temp)
		goto out;

	rprot_uohms = div64_u64((u64)v_delta * 1000000, abs(temp));
	pr_debug("v_delta: %d ibat_final: %d rprot_uohms: %d\n", v_delta, temp,
		rprot_uohms);
out:
	*val = esr_uohms - rprot_uohms;
	return rc;
}

static int fg_gen4_get_prop_capacity(struct fg_dev *fg, int *val)
{
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
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

	if (chip->vbatt_low) {
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

/* ALG callback functions below */

static int fg_gen4_get_ttf_param(void *data, enum ttf_param param, int *val)
{
	struct fg_gen4_chip *chip = data;
	struct fg_dev *fg;
	int rc = 0, act_cap_mah, full_soc;

	if (!chip)
		return -ENODEV;

	fg = &chip->fg;
	if (fg->battery_missing)
		return -EPERM;

	switch (param) {
	case TTF_MSOC:
		rc = fg_gen4_get_prop_capacity(fg, val);
		break;
	case TTF_VBAT:
		rc = fg_get_battery_voltage(fg, val);
		break;
	case TTF_IBAT:
		rc = fg_get_battery_current(fg, val);
		break;
	case TTF_FCC:
		rc = fg_get_sram_prop(fg, FG_SRAM_ACT_BATT_CAP, &act_cap_mah);
		if (rc < 0) {
			pr_err("Failed to get ACT_BATT_CAP rc=%d\n", rc);
			break;
		}

		rc = fg_get_sram_prop(fg, FG_SRAM_FULL_SOC, &full_soc);
		if (rc < 0) {
			pr_err("Failed to get FULL_SOC rc=%d\n", rc);
			break;
		}

		full_soc = DIV_ROUND_CLOSEST(((u16)full_soc >> 8) *
						FULL_CAPACITY, FULL_SOC_RAW);
		*val = full_soc * act_cap_mah / FULL_CAPACITY;
		break;
	case TTF_MODE:
		if (is_qnovo_en(fg))
			*val = TTF_MODE_QNOVO;
		else if (chip->ttf->step_chg_cfg_valid)
			*val = TTF_MODE_V_STEP_CHG;
		else
			*val = TTF_MODE_NORMAL;
		break;
	case TTF_ITERM:
		*val = chip->dt.sys_term_curr_ma;
		break;
	case TTF_RBATT:
		rc = fg_gen4_get_cell_impedance(chip, val);
		break;
	case TTF_VFLOAT:
		*val = fg->bp.float_volt_uv;
		break;
	case TTF_CHG_TYPE:
		*val = fg->charge_type;
		break;
	case TTF_CHG_STATUS:
		*val = fg->charge_status;
		break;
	default:
		pr_err_ratelimited("Unsupported parameter %d\n", param);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int fg_gen4_store_learned_capacity(void *data, int64_t learned_cap_uah)
{
	struct fg_gen4_chip *chip = data;
	struct fg_dev *fg;
	int16_t cc_mah;
	int rc;

	if (!chip)
		return -ENODEV;

	fg = &chip->fg;
	if (fg->battery_missing || !learned_cap_uah)
		return -EPERM;

	cc_mah = div64_s64(learned_cap_uah, 1000);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_ACT_BATT_CAP].addr_word,
			fg->sp[FG_SRAM_ACT_BATT_CAP].addr_byte, (u8 *)&cc_mah,
			fg->sp[FG_SRAM_ACT_BATT_CAP].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing act_batt_cap_bkup, rc=%d\n", rc);
		return rc;
	}

	fg_dbg(fg, FG_CAP_LEARN, "learned capacity %llduah/%dmah stored\n",
		chip->cl->learned_cap_uah, cc_mah);
	return 0;
}

static int fg_gen4_prime_cc_soc_sw(void *data, u32 batt_soc)
{
	struct fg_gen4_chip *chip = data;
	struct fg_dev *fg;
	int rc, cc_soc_sw;

	if (!chip)
		return -ENODEV;

	fg = &chip->fg;
	if (batt_soc == CC_SOC_30BIT)
		cc_soc_sw = batt_soc;
	else
		cc_soc_sw = div64_s64((int64_t)batt_soc * CC_SOC_30BIT,
				BATT_SOC_32BIT);

	rc = fg_sram_write(fg, fg->sp[FG_SRAM_CC_SOC_SW].addr_word,
		fg->sp[FG_SRAM_CC_SOC_SW].addr_byte, (u8 *)&cc_soc_sw,
		fg->sp[FG_SRAM_CC_SOC_SW].len, FG_IMA_ATOMIC);
	if (rc < 0)
		pr_err("Error in writing cc_soc_sw, rc=%d\n", rc);
	else
		fg_dbg(fg, FG_STATUS, "cc_soc_sw: %x\n", cc_soc_sw);

	return rc;
}

static int fg_gen4_get_cc_soc_sw(void *data, int *cc_soc_sw)
{
	struct fg_gen4_chip *chip = data;
	struct fg_dev *fg;
	int rc, temp;

	if (!chip)
		return -ENODEV;

	fg = &chip->fg;
	rc = fg_get_sram_prop(fg, FG_SRAM_CC_SOC_SW, &temp);
	if (rc < 0) {
		pr_err("Error in getting CC_SOC_SW, rc=%d\n", rc);
		return rc;
	}

	*cc_soc_sw = temp;
	return rc;
}

static int fg_gen4_restore_count(void *data, u16 *buf, int length)
{
	struct fg_gen4_chip *chip = data;
	int id, rc = 0;
	u8 tmp[2];

	if (!chip)
		return -ENODEV;

	if (!buf || length > BUCKET_COUNT)
		return -EINVAL;

	for (id = 0; id < length; id++) {
		rc = fg_sram_read(&chip->fg, CYCLE_COUNT_WORD + id,
				CYCLE_COUNT_OFFSET, (u8 *)tmp, 2,
				FG_IMA_DEFAULT);
		if (rc < 0)
			pr_err("failed to read bucket %d rc=%d\n", id, rc);
		else
			*buf++ = tmp[0] | tmp[1] << 8;
	}

	return rc;
}

static int fg_gen4_store_count(void *data, u16 *buf, int id, int length)
{
	struct fg_gen4_chip *chip = data;
	int rc;

	if (!chip)
		return -ENODEV;

	if (!buf || length > BUCKET_COUNT * 2 || id < 0 ||
		id > BUCKET_COUNT - 1 || ((id * 2) + length) > BUCKET_COUNT * 2)
		return -EINVAL;

	rc = fg_sram_write(&chip->fg, CYCLE_COUNT_WORD + id, CYCLE_COUNT_OFFSET,
			(u8 *)buf, length, FG_IMA_DEFAULT);
	if (rc < 0)
		pr_err("failed to write bucket %d rc=%d\n", rc);

	return rc;
}

/* All worker and helper functions below */

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

static void fg_gen4_update_rslow_coeff(struct fg_dev *fg, int batt_temp)
{
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	int rc, i;
	bool rslow_low = false;
	u8 buf[RSLOW_NUM_COEFFS];

	if (!fg->bp.rslow_normal_coeffs || !fg->bp.rslow_low_coeffs)
		return;

	/* Update Rslow low coefficients when Tbatt is < 0 C */
	if (batt_temp < 0)
		rslow_low = true;

	if (chip->rslow_low == rslow_low)
		return;

	for (i = 0; i < RSLOW_NUM_COEFFS; i++) {
		if (rslow_low)
			buf[i] = fg->bp.rslow_low_coeffs[i] & 0xFF;
		else
			buf[i] = fg->bp.rslow_normal_coeffs[i] & 0xFF;
	}

	rc = fg_sram_write(fg, RSLOW_COEFF_DISCHG_WORD, RSLOW_COEFF_LOW_OFFSET,
			buf, RSLOW_NUM_COEFFS, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Failed to write RLOW_COEFF_DISCHG_WORD rc=%d\n", rc);
	} else {
		chip->rslow_low = rslow_low;
		fg_dbg(fg, FG_STATUS, "Updated Rslow %s coefficients\n",
			rslow_low ? "low" : "normal");
	}
}

#define KI_COEFF_FULL_SOC_NORM_DEFAULT		733
#define KI_COEFF_FULL_SOC_LOW_DEFAULT		184
static int fg_gen4_adjust_ki_coeff_full_soc(struct fg_gen4_chip *chip,
						int batt_temp)
{
	struct fg_dev *fg = &chip->fg;
	int rc, ki_coeff_full_soc_norm, ki_coeff_full_soc_low;
	u8 val;

	if (batt_temp < 0) {
		ki_coeff_full_soc_norm = 0;
		ki_coeff_full_soc_low = 0;
	} else if (fg->charge_status == POWER_SUPPLY_STATUS_DISCHARGING) {
		ki_coeff_full_soc_norm = chip->dt.ki_coeff_full_soc_dischg[0];
		ki_coeff_full_soc_low = chip->dt.ki_coeff_full_soc_dischg[1];
	} else {
		ki_coeff_full_soc_norm = KI_COEFF_FULL_SOC_NORM_DEFAULT;
		ki_coeff_full_soc_low = KI_COEFF_FULL_SOC_LOW_DEFAULT;
	}

	if (chip->ki_coeff_full_soc[0] == ki_coeff_full_soc_norm &&
		chip->ki_coeff_full_soc[1] == ki_coeff_full_soc_low)
		return 0;

	fg_encode(fg->sp, FG_SRAM_KI_COEFF_FULL_SOC, ki_coeff_full_soc_norm,
		&val);
	rc = fg_sram_write(fg, KI_COEFF_FULL_SOC_NORM_WORD,
			KI_COEFF_FULL_SOC_NORM_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ki_coeff_full_soc_norm, rc=%d\n", rc);
		return rc;
	}

	fg_encode(fg->sp, FG_SRAM_KI_COEFF_FULL_SOC, ki_coeff_full_soc_low,
		&val);
	rc = fg_sram_write(fg, KI_COEFF_LOW_DISCHG_WORD,
			KI_COEFF_FULL_SOC_LOW_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ki_coeff_full_soc_low, rc=%d\n", rc);
		return rc;
	}

	chip->ki_coeff_full_soc[0] = ki_coeff_full_soc_norm;
	chip->ki_coeff_full_soc[1] = ki_coeff_full_soc_low;
	fg_dbg(fg, FG_STATUS, "Wrote ki_coeff_full_soc [%d %d]\n",
		ki_coeff_full_soc_norm, ki_coeff_full_soc_low);
	return 0;
}

#define KI_COEFF_LOW_DISCHG_DEFAULT	428
#define KI_COEFF_MED_DISCHG_DEFAULT	245
#define KI_COEFF_HI_DISCHG_DEFAULT	123
static int fg_gen4_adjust_ki_coeff_dischg(struct fg_dev *fg)
{
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	int rc, i, msoc;
	int ki_coeff_low = KI_COEFF_LOW_DISCHG_DEFAULT;
	int ki_coeff_med = KI_COEFF_MED_DISCHG_DEFAULT;
	int ki_coeff_hi = KI_COEFF_HI_DISCHG_DEFAULT;
	u8 val;

	if (!chip->ki_coeff_dischg_en)
		return 0;

	rc = fg_gen4_get_prop_capacity(fg, &msoc);
	if (rc < 0) {
		pr_err("Error in getting capacity, rc=%d\n", rc);
		return rc;
	}

	if (fg->charge_status == POWER_SUPPLY_STATUS_DISCHARGING) {
		for (i = KI_COEFF_SOC_LEVELS - 1; i >= 0; i--) {
			if (msoc < chip->dt.ki_coeff_soc[i]) {
				ki_coeff_low = chip->dt.ki_coeff_low_dischg[i];
				ki_coeff_med = chip->dt.ki_coeff_med_dischg[i];
				ki_coeff_hi = chip->dt.ki_coeff_hi_dischg[i];
			}
		}
	}

	if (ki_coeff_low > 0) {
		fg_encode(fg->sp, FG_SRAM_KI_COEFF_LOW_DISCHG, ki_coeff_low,
			&val);
		rc = fg_sram_write(fg,
			fg->sp[FG_SRAM_KI_COEFF_LOW_DISCHG].addr_word,
			fg->sp[FG_SRAM_KI_COEFF_LOW_DISCHG].addr_byte, &val,
			fg->sp[FG_SRAM_KI_COEFF_LOW_DISCHG].len,
			FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing ki_coeff_low, rc=%d\n", rc);
			return rc;
		}
		fg_dbg(fg, FG_STATUS, "Wrote ki_coeff_low %d\n", ki_coeff_low);
	}

	if (ki_coeff_med > 0) {
		fg_encode(fg->sp, FG_SRAM_KI_COEFF_MED_DISCHG, ki_coeff_med,
			&val);
		rc = fg_sram_write(fg,
			fg->sp[FG_SRAM_KI_COEFF_MED_DISCHG].addr_word,
			fg->sp[FG_SRAM_KI_COEFF_MED_DISCHG].addr_byte, &val,
			fg->sp[FG_SRAM_KI_COEFF_MED_DISCHG].len,
			FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing ki_coeff_med, rc=%d\n", rc);
			return rc;
		}
		fg_dbg(fg, FG_STATUS, "Wrote ki_coeff_med %d\n", ki_coeff_med);
	}

	if (ki_coeff_hi > 0) {
		fg_encode(fg->sp, FG_SRAM_KI_COEFF_HI_DISCHG, ki_coeff_hi,
			&val);
		rc = fg_sram_write(fg,
			fg->sp[FG_SRAM_KI_COEFF_HI_DISCHG].addr_word,
			fg->sp[FG_SRAM_KI_COEFF_HI_DISCHG].addr_byte, &val,
			fg->sp[FG_SRAM_KI_COEFF_HI_DISCHG].len,
			FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing ki_coeff_hi, rc=%d\n", rc);
			return rc;
		}
		fg_dbg(fg, FG_STATUS, "Wrote ki_coeff_hi %d\n", ki_coeff_hi);
	}

	return 0;
}

static int fg_gen4_slope_limit_config(struct fg_gen4_chip *chip, int batt_temp)
{
	struct fg_dev *fg = &chip->fg;
	enum slope_limit_status status;
	int rc;
	u8 buf;

	if (!chip->slope_limit_en || chip->rapid_soc_dec_en)
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

static int fg_gen4_configure_cutoff_current(struct fg_dev *fg, int current_ma)
{
	int rc;
	u8 buf[2];

	fg_encode(fg->sp, FG_SRAM_CUTOFF_CURR, current_ma, buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_CUTOFF_CURR].addr_word,
			fg->sp[FG_SRAM_CUTOFF_CURR].addr_byte, buf,
			fg->sp[FG_SRAM_CUTOFF_CURR].len, FG_IMA_DEFAULT);
	if (rc < 0)
		pr_err("Error in writing cutoff_curr, rc=%d\n", rc);

	return rc;
}

#define SLOPE_LIMIT_DEFAULT	5738
#define CUTOFF_CURRENT_MAX	15999
static int fg_gen4_rapid_soc_config(struct fg_gen4_chip *chip, bool en)
{
	struct fg_dev *fg = &chip->fg;
	int rc, slope_limit_coeff, cutoff_curr_ma;
	u8 buf;

	slope_limit_coeff = en ? SLOPE_LIMIT_COEFF_MAX : SLOPE_LIMIT_DEFAULT;
	fg_encode(fg->sp, FG_SRAM_SLOPE_LIMIT, slope_limit_coeff, &buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_SLOPE_LIMIT].addr_word,
			fg->sp[FG_SRAM_SLOPE_LIMIT].addr_byte, &buf,
			fg->sp[FG_SRAM_SLOPE_LIMIT].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in configuring slope_limit coefficient, rc=%d\n",
			rc);
		return rc;
	}

	cutoff_curr_ma = en ? CUTOFF_CURRENT_MAX : chip->dt.cutoff_curr_ma;
	rc = fg_gen4_configure_cutoff_current(fg, cutoff_curr_ma);
	if (rc < 0)
		return rc;

	fg_dbg(fg, FG_STATUS, "Configured slope limit coeff %d cutoff current %d mA\n",
		slope_limit_coeff, cutoff_curr_ma);
	return 0;
}

static int fg_gen4_get_batt_profile(struct fg_dev *fg)
{
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	struct device_node *node = fg->dev->of_node;
	struct device_node *batt_node, *profile_node;
	const char *data;
	int rc, len, i, tuple_len, avail_age_level = 0;

	batt_node = of_find_node_by_name(node, "qcom,battery-data");
	if (!batt_node) {
		pr_err("Batterydata not available\n");
		return -ENXIO;
	}

	if (chip->dt.multi_profile_load)
		profile_node = of_batterydata_get_best_aged_profile(batt_node,
					fg->batt_id_ohms / 1000,
					chip->batt_age_level, &avail_age_level);
	else
		profile_node = of_batterydata_get_best_profile(batt_node,
					fg->batt_id_ohms / 1000, NULL);
	if (IS_ERR(profile_node))
		return PTR_ERR(profile_node);

	if (!profile_node) {
		pr_err("couldn't find profile handle\n");
		return -ENODATA;
	}

	if (chip->dt.multi_profile_load &&
		chip->batt_age_level != avail_age_level) {
		fg_dbg(fg, FG_STATUS, "Batt_age_level %d doesn't exist, using %d\n",
			chip->batt_age_level, avail_age_level);
		chip->batt_age_level = avail_age_level;
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

	if (of_find_property(profile_node, "qcom,therm-coefficients", &len)) {
		len /= sizeof(u32);
		if (len == BATT_THERM_NUM_COEFFS) {
			if (!fg->bp.therm_coeffs) {
				fg->bp.therm_coeffs = devm_kcalloc(fg->dev,
					BATT_THERM_NUM_COEFFS, sizeof(u32),
					GFP_KERNEL);
				if (!fg->bp.therm_coeffs)
					return -ENOMEM;
			}
		}

		rc = of_property_read_u32_array(profile_node,
			"qcom,therm-coefficients", fg->bp.therm_coeffs, len);
		if (rc < 0) {
			pr_err("Couldn't read therm coefficients, rc:%d\n", rc);
			devm_kfree(fg->dev, fg->bp.therm_coeffs);
			fg->bp.therm_coeffs = NULL;
		}

		rc = of_property_read_u32(profile_node,
			"qcom,therm-center-offset", &fg->bp.therm_ctr_offset);
		if (rc < 0) {
			pr_err("battery therm-center-offset unavailable, rc:%d\n",
				rc);
			fg->bp.therm_ctr_offset = -EINVAL;
		}
	}

	/*
	 * Currently step charging thresholds should be read only for Vbatt
	 * based and not for SOC based.
	 */
	if (!of_property_read_bool(profile_node, "qcom,soc-based-step-chg") &&
		of_find_property(profile_node, "qcom,step-chg-ranges", &len) &&
		fg->bp.float_volt_uv > 0 && fg->bp.fastchg_curr_ma > 0) {
		len /= sizeof(u32);
		tuple_len = len / (sizeof(struct range_data) / sizeof(u32));
		if (tuple_len <= 0 || tuple_len > MAX_STEP_CHG_ENTRIES)
			return -EINVAL;

		mutex_lock(&chip->ttf->lock);
		chip->ttf->step_chg_cfg =
			kcalloc(len, sizeof(*chip->ttf->step_chg_cfg),
				GFP_KERNEL);
		if (!chip->ttf->step_chg_cfg) {
			mutex_unlock(&chip->ttf->lock);
			return -ENOMEM;
		}

		chip->ttf->step_chg_data =
			kcalloc(tuple_len, sizeof(*chip->ttf->step_chg_data),
				GFP_KERNEL);
		if (!chip->ttf->step_chg_data) {
			kfree(chip->ttf->step_chg_cfg);
			mutex_unlock(&chip->ttf->lock);
			return -ENOMEM;
		}

		rc = read_range_data_from_node(profile_node,
				"qcom,step-chg-ranges",
				chip->ttf->step_chg_cfg,
				fg->bp.float_volt_uv,
				fg->bp.fastchg_curr_ma * 1000);
		if (rc < 0) {
			pr_err("Error in reading qcom,step-chg-ranges from battery profile, rc=%d\n",
				rc);
			kfree(chip->ttf->step_chg_data);
			kfree(chip->ttf->step_chg_cfg);
			chip->ttf->step_chg_cfg = NULL;
			mutex_unlock(&chip->ttf->lock);
			return rc;
		}

		chip->ttf->step_chg_num_params = tuple_len;
		chip->ttf->step_chg_cfg_valid = true;
		mutex_unlock(&chip->ttf->lock);

		if (chip->ttf->step_chg_cfg_valid) {
			for (i = 0; i < tuple_len; i++)
				pr_debug("Vbatt_low: %d Vbatt_high: %d FCC: %d\n",
				chip->ttf->step_chg_cfg[i].low_threshold,
				chip->ttf->step_chg_cfg[i].high_threshold,
				chip->ttf->step_chg_cfg[i].value);
		}
	}

	if (of_find_property(profile_node, "qcom,therm-pull-up", NULL)) {
		rc = of_property_read_u32(profile_node, "qcom,therm-pull-up",
				&fg->bp.therm_pull_up_kohms);
		if (rc < 0) {
			pr_err("Couldn't read therm-pull-up, rc:%d\n", rc);
			fg->bp.therm_pull_up_kohms = -EINVAL;
		}
	}

	if (of_find_property(profile_node, "qcom,rslow-normal-coeffs", NULL) &&
		of_find_property(profile_node, "qcom,rslow-low-coeffs", NULL)) {
		if (!fg->bp.rslow_normal_coeffs) {
			fg->bp.rslow_normal_coeffs = devm_kcalloc(fg->dev,
						RSLOW_NUM_COEFFS, sizeof(u32),
						GFP_KERNEL);
			if (!fg->bp.rslow_normal_coeffs)
				return -ENOMEM;
		}

		if (!fg->bp.rslow_low_coeffs) {
			fg->bp.rslow_low_coeffs = devm_kcalloc(fg->dev,
						RSLOW_NUM_COEFFS, sizeof(u32),
						GFP_KERNEL);
			if (!fg->bp.rslow_low_coeffs) {
				devm_kfree(fg->dev, fg->bp.rslow_normal_coeffs);
				fg->bp.rslow_normal_coeffs = NULL;
				return -ENOMEM;
			}
		}

		rc = fg_parse_dt_property_u32_array(profile_node,
			"qcom,rslow-normal-coeffs",
			fg->bp.rslow_normal_coeffs, RSLOW_NUM_COEFFS);
		if (rc < 0) {
			devm_kfree(fg->dev, fg->bp.rslow_normal_coeffs);
			fg->bp.rslow_normal_coeffs = NULL;
			devm_kfree(fg->dev, fg->bp.rslow_low_coeffs);
			fg->bp.rslow_low_coeffs = NULL;
			return rc;
		}

		rc = fg_parse_dt_property_u32_array(profile_node,
			"qcom,rslow-low-coeffs",
			fg->bp.rslow_low_coeffs, RSLOW_NUM_COEFFS);
		if (rc < 0) {
			devm_kfree(fg->dev, fg->bp.rslow_normal_coeffs);
			fg->bp.rslow_normal_coeffs = NULL;
			devm_kfree(fg->dev, fg->bp.rslow_low_coeffs);
			fg->bp.rslow_low_coeffs = NULL;
			return rc;
		}
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

static int fg_gen4_bp_params_config(struct fg_dev *fg)
{
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	int rc, i;
	u8 buf, therm_coeffs[BATT_THERM_NUM_COEFFS * 2];
	u8 rslow_coeffs[RSLOW_NUM_COEFFS], val, mask;
	u16 rslow_scalefn;

	if (fg->bp.vbatt_full_mv > 0) {
		rc = fg_set_constant_chg_voltage(fg,
				fg->bp.vbatt_full_mv * 1000);
		if (rc < 0)
			return rc;
	}

	if (fg->bp.therm_coeffs) {
		/* Each coefficient is a 16-bit value */
		for (i = 0; i < BATT_THERM_NUM_COEFFS; i++) {
			therm_coeffs[i*2] = fg->bp.therm_coeffs[i] & 0xFF;
			therm_coeffs[i*2 + 1] = fg->bp.therm_coeffs[i] >> 8;
		}
		rc = fg_sram_write(fg, BATT_THERM_COEFF_WORD,
			BATT_THERM_COEFF_OFFSET, therm_coeffs,
			BATT_THERM_NUM_COEFFS * 2, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing therm-coeffs, rc=%d\n", rc);
			return rc;
		}

		buf = fg->bp.therm_ctr_offset;
		rc = fg_sram_write(fg, BATT_THERM_CONFIG_WORD,
			RATIO_CENTER_OFFSET, &buf, 1, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing therm-ctr-offset, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (fg->bp.rslow_normal_coeffs && fg->bp.rslow_low_coeffs) {
		rc = fg_sram_read(fg, RSLOW_COEFF_DISCHG_WORD,
				RSLOW_COEFF_LOW_OFFSET, rslow_coeffs,
				RSLOW_NUM_COEFFS, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Failed to read RLOW_COEFF_DISCHG_WORD rc=%d\n",
				rc);
			return rc;
		}

		/* Read Rslow coefficients back and set the status */
		for (i = 0; i < RSLOW_NUM_COEFFS; i++) {
			buf = fg->bp.rslow_low_coeffs[i] & 0xFF;
			if (rslow_coeffs[i] == buf) {
				chip->rslow_low = true;
			} else {
				chip->rslow_low = false;
				break;
			}
		}
		fg_dbg(fg, FG_STATUS, "Rslow_low: %d\n", chip->rslow_low);
	}

	/*
	 * Since this SRAM word falls inside profile region, configure it after
	 * the profile is loaded. This parameter doesn't come from battery
	 * profile DT property.
	 */
	if (fg->wa_flags & PM8150B_V1_RSLOW_COMP_WA) {
		val = 0;
		mask = BIT(1);
		rc = fg_sram_masked_write(fg, RSLOW_CONFIG_WORD,
				RSLOW_CONFIG_OFFSET, mask, val, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing RSLOW_CONFIG_WORD, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (fg->wa_flags & PM8150B_V2_RSLOW_SCALE_FN_WA) {
		rslow_scalefn = 0x4000;
		rc = fg_sram_write(fg, RSLOW_SCALE_FN_CHG_V2_WORD,
				RSLOW_SCALE_FN_CHG_V2_OFFSET,
				(u8 *)&rslow_scalefn, 2, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing RSLOW_SCALE_FN_CHG_WORD rc=%d\n",
				rc);
			return rc;
		}

		rc = fg_sram_write(fg, RSLOW_SCALE_FN_DISCHG_V2_WORD,
				RSLOW_SCALE_FN_DISCHG_V2_OFFSET,
				(u8 *)&rslow_scalefn, 2, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing RSLOW_SCALE_FN_DISCHG_WORD rc=%d\n",
				rc);
			return rc;
		}
	}

	if (fg->bp.therm_pull_up_kohms > 0) {
		switch (fg->bp.therm_pull_up_kohms) {
		case 30:
			buf = BATT_THERM_PULL_UP_30K;
			break;
		case 100:
			buf = BATT_THERM_PULL_UP_100K;
			break;
		case 400:
			buf = BATT_THERM_PULL_UP_400K;
			break;
		default:
			return -EINVAL;
		}

		rc = fg_masked_write(fg, ADC_RR_BATT_THERM_BASE_CFG1(fg),
					BATT_THERM_PULL_UP_MASK, buf);
		if (rc < 0) {
			pr_err("failed to write to 0x%04X, rc=%d\n",
				ADC_RR_BATT_THERM_BASE_CFG1(fg), rc);
			return rc;
		}
	}

	return 0;
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

#define PROFILE_LOAD_BIT	BIT(0)
#define BOOTLOADER_LOAD_BIT	BIT(1)
#define BOOTLOADER_RESTART_BIT	BIT(2)
#define HLOS_RESTART_BIT	BIT(3)
#define FIRST_PROFILE_LOAD_BIT	BIT(4)
static bool is_profile_load_required(struct fg_gen4_chip *chip)
{
	struct fg_dev *fg = &chip->fg;
	u8 buf[PROFILE_COMP_LEN], val;
	bool profiles_same = false, valid_integrity = false;
	int rc, i;
	u8 white_list_values[] = {
		HLOS_RESTART_BIT,
		BOOTLOADER_LOAD_BIT,
		BOOTLOADER_LOAD_BIT | BOOTLOADER_RESTART_BIT,
		BOOTLOADER_RESTART_BIT | HLOS_RESTART_BIT,
		BOOTLOADER_LOAD_BIT | FIRST_PROFILE_LOAD_BIT,
		BOOTLOADER_LOAD_BIT | BOOTLOADER_RESTART_BIT |
			FIRST_PROFILE_LOAD_BIT,
		HLOS_RESTART_BIT | BOOTLOADER_RESTART_BIT |
			FIRST_PROFILE_LOAD_BIT,
	};

	if (chip->dt.multi_profile_load &&
		(chip->batt_age_level != chip->last_batt_age_level))
		return true;

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
		for (i = 0; i < ARRAY_SIZE(white_list_values); i++)  {
			if (val == white_list_values[i]) {
				valid_integrity = true;
				break;
			}
		}

		if (!valid_integrity) {
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

#define SOC_READY_WAIT_TIME_MS	1000
static int qpnp_fg_gen4_load_profile(struct fg_gen4_chip *chip)
{
	struct fg_dev *fg = &chip->fg;
	u8 val, mask, buf[2];
	int rc;
	bool normal_profile_load = false;

	/*
	 * This is used to determine if the profile is loaded normally i.e.
	 * either multi profile loading is disabled OR if it is enabled, then
	 * only loading the profile with battery age level 0 is considered.
	 */
	normal_profile_load = !chip->dt.multi_profile_load ||
				(chip->dt.multi_profile_load &&
					chip->batt_age_level == 0);
	if (normal_profile_load) {
		rc = fg_masked_write(fg, BATT_SOC_RESTART(fg), RESTART_GO_BIT,
					0);
		if (rc < 0) {
			pr_err("Error in writing to %04x, rc=%d\n",
				BATT_SOC_RESTART(fg), rc);
			return rc;
		}
	}

	/* load battery profile */
	rc = fg_sram_write(fg, PROFILE_LOAD_WORD, PROFILE_LOAD_OFFSET,
			chip->batt_profile, PROFILE_LEN, FG_IMA_ATOMIC);
	if (rc < 0) {
		pr_err("Error in writing battery profile, rc:%d\n", rc);
		return rc;
	}

	if (normal_profile_load) {
		/* Enable side loading for voltage and current */
		val = mask = BIT(0);
		rc = fg_sram_masked_write(fg, SYS_CONFIG_WORD,
				SYS_CONFIG_OFFSET, mask, val, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in setting SYS_CONFIG_WORD[0], rc=%d\n",
				rc);
			return rc;
		}

		/* Clear first logged main current ADC values */
		buf[0] = buf[1] = 0;
		rc = fg_sram_write(fg, FIRST_LOG_CURRENT_v2_WORD,
				FIRST_LOG_CURRENT_v2_OFFSET, buf, 2,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in clearing FIRST_LOG_CURRENT rc=%d\n",
				rc);
			return rc;
		}
	}

	/* Set the profile integrity bit */
	val = HLOS_RESTART_BIT | PROFILE_LOAD_BIT;
	rc = fg_sram_write(fg, PROFILE_INTEGRITY_WORD,
			PROFILE_INTEGRITY_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to write profile integrity rc=%d\n", rc);
		return rc;
	}

	if (chip->dt.multi_profile_load && chip->batt_age_level >= 0) {
		val = (u8)chip->batt_age_level;
		rc = fg_sram_write(fg, BATT_AGE_LEVEL_WORD,
				BATT_AGE_LEVEL_OFFSET, &val, 1, FG_IMA_ATOMIC);
		if (rc < 0) {
			pr_err("Error in writing batt_age_level, rc:%d\n", rc);
			return rc;
		}
	}

	if (normal_profile_load) {
		rc = fg_restart(fg, SOC_READY_WAIT_TIME_MS);
		if (rc < 0) {
			pr_err("Error in restarting FG, rc=%d\n", rc);
			return rc;
		}

		/* Clear side loading for voltage and current */
		val = 0;
		mask = BIT(0);
		rc = fg_sram_masked_write(fg, SYS_CONFIG_WORD,
				SYS_CONFIG_OFFSET, mask, val, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in clearing SYS_CONFIG_WORD[0], rc=%d\n",
				rc);
			return rc;
		}
	}

	return 0;
}

static void profile_load_work(struct work_struct *work)
{
	struct fg_dev *fg = container_of(work,
				struct fg_dev,
				profile_load_work.work);
	struct fg_gen4_chip *chip = container_of(fg,
				struct fg_gen4_chip, fg);
	int64_t nom_cap_uah;
	u8 val, buf[2];
	int rc;

	vote(fg->awake_votable, PROFILE_LOAD, true, 0);

	rc = fg_gen4_get_batt_id(chip);
	if (rc < 0) {
		pr_err("Error in getting battery id, rc:%d\n", rc);
		goto out;
	}

	rc = fg_gen4_get_batt_profile(fg);
	if (rc < 0) {
		fg->profile_load_status = PROFILE_MISSING;
		pr_warn("profile for batt_id=%dKOhms not found..using OTP, rc:%d\n",
			fg->batt_id_ohms / 1000, rc);
		goto out;
	}

	if (!fg->profile_available)
		goto out;

	if (!is_profile_load_required(chip))
		goto done;

	if (!chip->dt.multi_profile_load)
		clear_cycle_count(chip->counter);

	fg_dbg(fg, FG_STATUS, "profile loading started\n");

	rc = qpnp_fg_gen4_load_profile(chip);
	if (rc < 0)
		goto out;

	fg_dbg(fg, FG_STATUS, "SOC is ready\n");
	fg->profile_load_status = PROFILE_LOADED;

	if (fg->wa_flags & PM8150B_V1_DMA_WA)
		msleep(1000);

	/*
	 * Whenever battery profile is loaded, read nominal capacity and write
	 * it to actual (or aged) capacity as it is outside the profile region
	 * and might contain OTP values.
	 */
	rc = fg_sram_read(fg, NOM_CAP_WORD, NOM_CAP_OFFSET, buf, 2,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading %04x[%d] rc=%d\n", NOM_CAP_WORD,
			NOM_CAP_OFFSET, rc);
	} else {
		rc = fg_sram_write(fg, fg->sp[FG_SRAM_ACT_BATT_CAP].addr_word,
			fg->sp[FG_SRAM_ACT_BATT_CAP].addr_byte, buf,
			fg->sp[FG_SRAM_ACT_BATT_CAP].len, FG_IMA_DEFAULT);
		if (rc < 0)
			pr_err("Error in writing to ACT_BATT_CAP rc=%d\n", rc);
	}
done:
	rc = fg_sram_read(fg, PROFILE_INTEGRITY_WORD,
			PROFILE_INTEGRITY_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (!rc && (val & FIRST_PROFILE_LOAD_BIT)) {
		fg_dbg(fg, FG_STATUS, "First profile load bit is set\n");
		chip->first_profile_load = true;
	}

	rc = fg_gen4_bp_params_config(fg);
	if (rc < 0)
		pr_err("Error in configuring battery profile params, rc:%d\n",
			rc);

	rc = fg_gen4_get_nominal_capacity(chip, &nom_cap_uah);
	if (!rc) {
		rc = cap_learning_post_profile_init(chip->cl, nom_cap_uah);
		if (rc < 0)
			pr_err("Error in cap_learning_post_profile_init rc=%d\n",
				rc);
	}

	batt_psy_initialized(fg);
	fg_notify_charger(fg);

	schedule_delayed_work(&chip->ttf->ttf_work, msecs_to_jiffies(10000));
	fg_dbg(fg, FG_STATUS, "profile loaded successfully");
out:
	if (chip->dt.multi_profile_load && rc < 0)
		chip->batt_age_level = chip->last_batt_age_level;
	fg->soc_reporting_ready = true;
	vote(fg->awake_votable, ESR_FCC_VOTER, true, 0);
	schedule_delayed_work(&chip->pl_enable_work, msecs_to_jiffies(5000));
	vote(fg->awake_votable, PROFILE_LOAD, false, 0);
	if (!work_pending(&fg->status_change_work)) {
		pm_stay_awake(fg->dev);
		schedule_work(&fg->status_change_work);
	}
}

static void get_batt_psy_props(struct fg_dev *fg)
{
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	union power_supply_propval prop = {0, };
	int rc;

	if (!batt_psy_initialized(fg))
		return;

	rc = power_supply_get_property(fg->batt_psy, POWER_SUPPLY_PROP_STATUS,
			&prop);
	if (rc < 0) {
		pr_err("Error in getting charging status, rc=%d\n", rc);
		return;
	}

	fg->charge_status = prop.intval;
	rc = power_supply_get_property(fg->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &prop);
	if (rc < 0) {
		pr_err("Error in getting charge type, rc=%d\n", rc);
		return;
	}

	fg->charge_type = prop.intval;
	rc = power_supply_get_property(fg->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_DONE, &prop);
	if (rc < 0) {
		pr_err("Error in getting charge_done, rc=%d\n", rc);
		return;
	}

	fg->charge_done = prop.intval;
	rc = power_supply_get_property(fg->batt_psy, POWER_SUPPLY_PROP_HEALTH,
		&prop);
	if (rc < 0) {
		pr_err("Error in getting battery health, rc=%d\n", rc);
		return;
	}

	fg->health = prop.intval;

	if (!chip->recharge_soc_thr) {
		rc = power_supply_get_property(fg->batt_psy,
			POWER_SUPPLY_PROP_RECHARGE_SOC, &prop);
		if (rc < 0) {
			pr_err("Error in getting recharge SOC, rc=%d\n", rc);
			return;
		}

		if (prop.intval < 0)
			pr_debug("Recharge SOC not configured %d\n",
				prop.intval);
		else
			chip->recharge_soc_thr = prop.intval;
	}
}

static int fg_gen4_esr_soh_update(struct fg_dev *fg)
{
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	int rc, msoc, esr_uohms, tmp;

	if (!fg->soc_reporting_ready || fg->battery_missing) {
		chip->esr_actual = -EINVAL;
		chip->esr_nominal = -EINVAL;
		return 0;
	}

	rc = get_cycle_count(chip->counter, &tmp);
	if (rc < 0)
		pr_err("Couldn't get cycle count rc=%d\n", rc);
	else if (tmp != chip->esr_soh_cycle_count)
		chip->esr_soh_notified = false;

	if (fg->charge_status != POWER_SUPPLY_STATUS_CHARGING ||
			chip->esr_soh_notified)
		return 0;

	rc = fg_get_msoc(fg, &msoc);
	if (rc < 0) {
		pr_err("Error in getting msoc, rc=%d\n", rc);
		return rc;
	}

	if (msoc != ESR_SOH_SOC) {
		fg_dbg(fg, FG_STATUS, "msoc: %d, not publishing ESR params\n",
			msoc);
		return 0;
	}

	rc = fg_get_sram_prop(fg, FG_SRAM_ESR_ACT, &esr_uohms);
	if (rc < 0) {
		pr_err("Error in getting esr_actual, rc=%d\n",
			rc);
		return rc;
	}
	chip->esr_actual = esr_uohms;

	rc = fg_get_sram_prop(fg, FG_SRAM_ESR_MDL, &esr_uohms);
	if (rc < 0) {
		pr_err("Error in getting esr_nominal, rc=%d\n",
			rc);
		chip->esr_actual = -EINVAL;
		return rc;
	}
	chip->esr_nominal = esr_uohms;

	fg_dbg(fg, FG_STATUS, "esr_actual: %d esr_nominal: %d\n",
		chip->esr_actual, chip->esr_nominal);

	if (fg->batt_psy)
		power_supply_changed(fg->batt_psy);

	rc = get_cycle_count(chip->counter, &chip->esr_soh_cycle_count);
	if (rc < 0)
		pr_err("Couldn't get cycle count rc=%d\n", rc);

	chip->esr_soh_notified = true;

	return 0;
}

static int fg_gen4_update_maint_soc(struct fg_dev *fg)
{
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
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
	} else if (fg->maint_soc && msoc <= fg->last_msoc) {
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

	fg_dbg(fg, FG_STATUS, "msoc: %d last_msoc: %d maint_soc: %d delta_soc: %d\n",
		msoc, fg->last_msoc, fg->maint_soc, fg->delta_soc);
	fg->last_msoc = msoc;
out:
	mutex_unlock(&fg->charge_full_lock);
	return rc;
}

static int fg_gen4_configure_full_soc(struct fg_dev *fg, int bsoc)
{
	int rc;
	u8 full_soc[2] = {0xFF, 0xFF}, buf[2];

	/*
	 * Once SOC masking condition is cleared, FULL_SOC and MONOTONIC_SOC
	 * needs to be updated to reflect the same. Write battery SOC to
	 * FULL_SOC and write a full value to MONOTONIC_SOC.
	 */
	fg_encode(fg->sp, FG_SRAM_FULL_SOC, bsoc, buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_FULL_SOC].addr_word,
			fg->sp[FG_SRAM_FULL_SOC].addr_byte, buf,
			fg->sp[FG_SRAM_FULL_SOC].len, FG_IMA_ATOMIC);
	if (rc < 0) {
		pr_err("failed to write full_soc rc=%d\n", rc);
		return rc;
	}

	rc = fg_sram_write(fg, fg->sp[FG_SRAM_MONOTONIC_SOC].addr_word,
			fg->sp[FG_SRAM_MONOTONIC_SOC].addr_byte, full_soc,
			fg->sp[FG_SRAM_MONOTONIC_SOC].len, FG_IMA_ATOMIC);
	if (rc < 0) {
		pr_err("failed to write monotonic_soc rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int fg_gen4_set_recharge_soc(struct fg_dev *fg, int recharge_soc)
{
	union power_supply_propval prop = {0, };
	int rc;

	if (recharge_soc < 0 || recharge_soc > FULL_CAPACITY || !fg->batt_psy)
		return 0;

	prop.intval = recharge_soc;
	rc = power_supply_set_property(fg->batt_psy,
		POWER_SUPPLY_PROP_RECHARGE_SOC, &prop);
	if (rc < 0) {
		pr_err("Error in setting recharge SOC, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int fg_gen4_adjust_recharge_soc(struct fg_gen4_chip *chip)
{
	struct fg_dev *fg = &chip->fg;
	int rc, msoc, recharge_soc, new_recharge_soc = 0;
	bool recharge_soc_status;

	if (!chip->recharge_soc_thr)
		return 0;

	recharge_soc = chip->recharge_soc_thr;
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
				 * Device is out of JEITA. Restore back default
				 * threshold.
				 */

				new_recharge_soc = recharge_soc;
				fg->recharge_soc_adjusted = false;
				return 0;
			}
		} else {
			if (!fg->recharge_soc_adjusted)
				return 0;

			/*
			 * If we are here, then it means that recharge SOC
			 * had been adjusted already and it could be probably
			 * because of early termination. We shouldn't restore
			 * the original recharge SOC threshold if the health is
			 * not good, which means battery is in JEITA zone.
			 */
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

	if (recharge_soc_status == fg->recharge_soc_adjusted)
		return 0;

	rc = fg_gen4_set_recharge_soc(fg, new_recharge_soc);
	if (rc < 0) {
		fg->recharge_soc_adjusted = recharge_soc_status;
		pr_err("Couldn't set recharge SOC, rc=%d\n", rc);
		return rc;
	}

	fg_dbg(fg, FG_STATUS, "recharge soc set to %d\n", new_recharge_soc);
	return 0;
}

static int fg_gen4_charge_full_update(struct fg_dev *fg)
{
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	union power_supply_propval prop = {0, };
	int rc, msoc, bsoc, recharge_soc, msoc_raw;

	if (!chip->dt.hold_soc_while_full)
		return 0;

	if (!batt_psy_initialized(fg))
		return 0;

	mutex_lock(&fg->charge_full_lock);
	vote(fg->delta_bsoc_irq_en_votable, DELTA_BSOC_IRQ_VOTER,
		fg->charge_done, 0);
	rc = power_supply_get_property(fg->batt_psy,
		POWER_SUPPLY_PROP_RECHARGE_SOC, &prop);
	if (rc < 0) {
		pr_err("Error in getting recharge_soc, rc=%d\n", rc);
		goto out;
	}

	recharge_soc = prop.intval;
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
		 * If charge_done is still set, wait for recharging or
		 * discharging to happen.
		 */
		if (fg->charge_done)
			goto out;

		rc = fg_gen4_configure_full_soc(fg, bsoc);
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

static int fg_gen4_esr_fcc_config(struct fg_gen4_chip *chip)
{
	struct fg_dev *fg = &chip->fg;
	union power_supply_propval prop = {0, };
	int rc;
	bool parallel_en = false, cp_en = false, qnovo_en, esr_fcc_ctrl_en;
	u8 val, mask;

	if (is_parallel_charger_available(fg)) {
		rc = power_supply_get_property(fg->parallel_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &prop);
		if (rc < 0)
			pr_err_ratelimited("Error in reading charging_enabled from parallel_psy, rc=%d\n",
				rc);
		else
			parallel_en = prop.intval;
	}

	if (chip->cp_disable_votable)
		cp_en = !get_effective_result(chip->cp_disable_votable);

	qnovo_en = is_qnovo_en(fg);

	fg_dbg(fg, FG_POWER_SUPPLY, "chg_sts: %d par_en: %d cp_en: %d qnov_en: %d esr_fcc_ctrl_en: %d\n",
		fg->charge_status, parallel_en, cp_en, qnovo_en,
		chip->esr_fcc_ctrl_en);

	if (fg->charge_status == POWER_SUPPLY_STATUS_CHARGING &&
			(parallel_en || qnovo_en || cp_en)) {
		if (chip->esr_fcc_ctrl_en)
			return 0;

		/*
		 * When parallel charging or Qnovo or Charge pump is enabled,
		 * configure ESR FCC to 300mA to trigger an ESR pulse. Without
		 * this, FG can request the main charger to increase FCC when it
		 * is supposed to decrease it.
		 */
		val = GEN4_ESR_FCC_300MA << GEN4_ESR_FAST_CRG_IVAL_SHIFT |
			ESR_FAST_CRG_CTL_EN_BIT;
		esr_fcc_ctrl_en = true;
	} else {
		if (!chip->esr_fcc_ctrl_en)
			return 0;

		/*
		 * If we're here, then it means either the device is not in
		 * charging state or parallel charging / Qnovo / Charge pump is
		 * disabled. Disable ESR fast charge current control in SW.
		 */
		val = GEN4_ESR_FCC_1A << GEN4_ESR_FAST_CRG_IVAL_SHIFT;
		esr_fcc_ctrl_en = false;
	}

	mask = GEN4_ESR_FAST_CRG_IVAL_MASK | ESR_FAST_CRG_CTL_EN_BIT;
	rc = fg_masked_write(fg, BATT_INFO_ESR_FAST_CRG_CFG(fg), mask, val);
	if (rc < 0) {
		pr_err("Error in writing to %04x, rc=%d\n",
			BATT_INFO_ESR_FAST_CRG_CFG(fg), rc);
		return rc;
	}

	chip->esr_fcc_ctrl_en = esr_fcc_ctrl_en;
	fg_dbg(fg, FG_STATUS, "esr_fcc_ctrl_en set to %d\n",
		chip->esr_fcc_ctrl_en);
	return 0;
}

static int fg_gen4_configure_esr_cal_soc(struct fg_dev *fg, int soc_min,
					int soc_max)
{
	int rc;
	u8 buf[2];

	fg_encode(fg->sp, FG_SRAM_ESR_CAL_SOC_MIN, soc_min, buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_ESR_CAL_SOC_MIN].addr_word,
			fg->sp[FG_SRAM_ESR_CAL_SOC_MIN].addr_byte, buf,
			fg->sp[FG_SRAM_ESR_CAL_SOC_MIN].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR_CAL_SOC_MIN, rc=%d\n", rc);
		return rc;
	}

	fg_encode(fg->sp, FG_SRAM_ESR_CAL_SOC_MAX, soc_max, buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_ESR_CAL_SOC_MAX].addr_word,
			fg->sp[FG_SRAM_ESR_CAL_SOC_MAX].addr_byte, buf,
			fg->sp[FG_SRAM_ESR_CAL_SOC_MAX].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR_CAL_SOC_MAX, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int fg_gen4_configure_esr_cal_temp(struct fg_dev *fg, int temp_min,
					int temp_max)
{
	int rc;
	u8 buf[2];

	fg_encode(fg->sp, FG_SRAM_ESR_CAL_TEMP_MIN, temp_min, buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_ESR_CAL_TEMP_MIN].addr_word,
			fg->sp[FG_SRAM_ESR_CAL_TEMP_MIN].addr_byte, buf,
			fg->sp[FG_SRAM_ESR_CAL_TEMP_MIN].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR_CAL_TEMP_MIN, rc=%d\n", rc);
		return rc;
	}

	fg_encode(fg->sp, FG_SRAM_ESR_CAL_TEMP_MAX, temp_max, buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_ESR_CAL_TEMP_MAX].addr_word,
			fg->sp[FG_SRAM_ESR_CAL_TEMP_MAX].addr_byte, buf,
			fg->sp[FG_SRAM_ESR_CAL_TEMP_MAX].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR_CAL_TEMP_MAX, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

#define ESR_CAL_TEMP_MIN	-127
#define ESR_CAL_TEMP_MAX	127
static int fg_gen4_esr_fast_calib_config(struct fg_gen4_chip *chip, bool en)
{
	struct fg_dev *fg = &chip->fg;
	int rc, esr_timer_chg_init, esr_timer_chg_max, esr_timer_dischg_init,
		esr_timer_dischg_max, esr_fast_cal_ms, esr_cal_soc_min,
		esr_cal_soc_max, esr_cal_temp_min, esr_cal_temp_max;
	u8 val, mask;

	esr_timer_chg_init = esr_timer_chg_max = -EINVAL;
	esr_timer_dischg_init = esr_timer_dischg_max = -EINVAL;
	if (en) {
		esr_timer_chg_init = chip->dt.esr_timer_chg_fast[TIMER_RETRY];
		esr_timer_chg_max = chip->dt.esr_timer_chg_fast[TIMER_MAX];
		esr_timer_dischg_init =
				chip->dt.esr_timer_dischg_fast[TIMER_RETRY];
		esr_timer_dischg_max =
				chip->dt.esr_timer_dischg_fast[TIMER_MAX];

		esr_cal_soc_min = 0;
		esr_cal_soc_max = FULL_SOC_RAW;
		esr_cal_temp_min = ESR_CAL_TEMP_MIN;
		esr_cal_temp_max = ESR_CAL_TEMP_MAX;

		vote(chip->delta_esr_irq_en_votable, DELTA_ESR_IRQ_VOTER,
			true, 0);
		chip->esr_fast_calib_done = false;
	} else {
		chip->esr_fast_calib_done = true;

		esr_timer_chg_init = chip->dt.esr_timer_chg_slow[TIMER_RETRY];
		esr_timer_chg_max = chip->dt.esr_timer_chg_slow[TIMER_MAX];
		esr_timer_dischg_init =
				chip->dt.esr_timer_dischg_slow[TIMER_RETRY];
		esr_timer_dischg_max =
				chip->dt.esr_timer_dischg_slow[TIMER_MAX];

		esr_cal_soc_min = chip->dt.esr_cal_soc_thresh[0];
		esr_cal_soc_max = chip->dt.esr_cal_soc_thresh[1];
		esr_cal_temp_min = chip->dt.esr_cal_temp_thresh[0];
		esr_cal_temp_max = chip->dt.esr_cal_temp_thresh[1];

		vote(chip->delta_esr_irq_en_votable, DELTA_ESR_IRQ_VOTER,
			false, 0);
	}

	rc = fg_set_esr_timer(fg, esr_timer_chg_init, esr_timer_chg_max, true,
				FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in setting ESR charge timer, rc=%d\n",
			rc);
		return rc;
	}

	rc = fg_set_esr_timer(fg, esr_timer_dischg_init, esr_timer_dischg_max,
				false, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in setting ESR discharge timer, rc=%d\n",
			rc);
		return rc;
	}

	rc = fg_gen4_configure_esr_cal_soc(fg, esr_cal_soc_min,
			esr_cal_soc_max);
	if (rc < 0) {
		pr_err("Error in configuring SOC thresholds, rc=%d\n",
			rc);
		return rc;
	}

	rc = fg_gen4_configure_esr_cal_temp(fg, esr_cal_temp_min,
			esr_cal_temp_max);
	if (rc < 0) {
		pr_err("Error in configuring temperature thresholds, rc=%d\n",
			rc);
		return rc;
	}

	/*
	 * Disable ESR discharging timer and ESR pulsing during
	 * discharging when ESR fast calibration is disabled. Otherwise, keep
	 * it enabled so that ESR pulses can happen during discharging.
	 */
	val = en ? BIT(6) | BIT(7) : 0;
	mask = BIT(6) | BIT(7);
	rc = fg_sram_masked_write(fg, SYS_CONFIG_WORD,
			SYS_CONFIG_OFFSET, mask, val, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing SYS_CONFIG_WORD, rc=%d\n", rc);
		return rc;
	}

	if (en) {
		/* Set ESR fast calibration timer to 50 seconds as default */
		esr_fast_cal_ms = 50000;
		if (chip->dt.esr_timer_chg_fast > 0 &&
			chip->dt.delta_esr_disable_count > 0)
			esr_fast_cal_ms = 3 * chip->dt.delta_esr_disable_count *
				chip->dt.esr_timer_chg_fast[TIMER_MAX] * 1000;

		alarm_start_relative(&chip->esr_fast_cal_timer,
					ms_to_ktime(esr_fast_cal_ms));
	} else {
		alarm_cancel(&chip->esr_fast_cal_timer);
	}

	fg_dbg(fg, FG_STATUS, "%sabling ESR fast calibration\n",
		en ? "En" : "Dis");
	return 0;
}

/* All irq handlers below this */

static irqreturn_t fg_mem_attn_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);

	fg_dbg(fg, FG_IRQ, "irq %d triggered\n", irq);
	complete_all(&chip->mem_attn);

	return IRQ_HANDLED;
}

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

static irqreturn_t fg_delta_esr_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	int esr_uohms, rc;

	rc = fg_get_battery_resistance(fg, &esr_uohms);
	if (rc < 0)
		return IRQ_HANDLED;

	fg_dbg(fg, FG_IRQ, "irq %d triggered esr_uohms: %d\n", irq, esr_uohms);

	if (chip->esr_fast_calib) {
		vote(fg->awake_votable, ESR_CALIB, true, 0);
		schedule_work(&chip->esr_calib_work);
	}

	return IRQ_HANDLED;
}

static irqreturn_t fg_vbatt_low_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	int rc, vbatt_mv, msoc_raw;

	rc = fg_get_battery_voltage(fg, &vbatt_mv);
	if (rc < 0)
		return IRQ_HANDLED;

	vbatt_mv /= 1000;
	rc = fg_get_msoc_raw(fg, &msoc_raw);
	if (rc < 0)
		return IRQ_HANDLED;

	fg_dbg(fg, FG_IRQ, "irq %d triggered vbatt_mv: %d msoc_raw:%d\n", irq,
		vbatt_mv, msoc_raw);

	if (vbatt_mv < chip->dt.cutoff_volt_mv) {
		if (chip->dt.rapid_soc_dec_en) {
			/*
			 * Set this flag so that slope limiter coefficient
			 * cannot be configured during rapid SOC decrease.
			 */
			chip->rapid_soc_dec_en = true;

			rc = fg_gen4_rapid_soc_config(chip, true);
			if (rc < 0)
				pr_err("Error in configuring for rapid SOC reduction rc:%d\n",
					rc);
		} else {
			/* Set the flag to show 0% */
			chip->vbatt_low = true;
		}
	}

	if (batt_psy_initialized(fg))
		power_supply_changed(fg->batt_psy);

	return IRQ_HANDLED;
}

static irqreturn_t fg_batt_missing_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	u8 status;
	int rc;

	rc = fg_read(fg, ADC_RR_INT_RT_STS(fg), &status, 1);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			ADC_RR_INT_RT_STS(fg), rc);
		return IRQ_HANDLED;
	}

	fg_dbg(fg, FG_IRQ, "irq %d triggered sts:%d\n", irq, status);
	fg->battery_missing = (status & ADC_RR_BT_MISS_BIT);

	if (fg->battery_missing) {
		fg->profile_available = false;
		fg->profile_load_status = PROFILE_NOT_LOADED;
		fg->soc_reporting_ready = false;
		fg->batt_id_ohms = -EINVAL;

		mutex_lock(&chip->ttf->lock);
		chip->ttf->step_chg_cfg_valid = false;
		chip->ttf->step_chg_num_params = 0;
		kfree(chip->ttf->step_chg_cfg);
		chip->ttf->step_chg_cfg = NULL;
		kfree(chip->ttf->step_chg_data);
		chip->ttf->step_chg_data = NULL;
		mutex_unlock(&chip->ttf->lock);
		cancel_delayed_work_sync(&chip->pl_enable_work);
		vote(fg->awake_votable, ESR_FCC_VOTER, false, 0);
		if (chip->pl_disable_votable)
			vote(chip->pl_disable_votable, ESR_FCC_VOTER, true, 0);
		if (chip->cp_disable_votable)
			vote(chip->cp_disable_votable, ESR_FCC_VOTER, true, 0);
		return IRQ_HANDLED;
	}

	clear_battery_profile(fg);
	schedule_delayed_work(&fg->profile_load_work, 0);

	if (fg->fg_psy)
		power_supply_changed(fg->fg_psy);

	return IRQ_HANDLED;
}

static irqreturn_t fg_batt_temp_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;

	fg_dbg(fg, FG_IRQ, "irq %d triggered\n", irq);
	return IRQ_HANDLED;
}

static irqreturn_t fg_delta_batt_temp_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	int rc, batt_temp;

	rc = fg_gen4_get_battery_temp(fg, &batt_temp);
	if (rc < 0) {
		pr_err("Error in getting batt_temp\n");
		return IRQ_HANDLED;
	}

	fg_dbg(fg, FG_IRQ, "irq %d triggered batt_temp:%d\n", irq, batt_temp);

	rc = fg_gen4_slope_limit_config(chip, batt_temp);
	if (rc < 0)
		pr_err("Error in configuring slope limiter rc:%d\n", rc);

	rc = fg_gen4_adjust_ki_coeff_full_soc(chip, batt_temp);
	if (rc < 0)
		pr_err("Error in configuring ki_coeff_full_soc rc:%d\n", rc);

	if (abs(fg->last_batt_temp - batt_temp) > 30)
		pr_warn("Battery temperature last:%d current: %d\n",
			fg->last_batt_temp, batt_temp);

	if (fg->last_batt_temp != batt_temp)
		fg->last_batt_temp = batt_temp;

	if (batt_psy_initialized(fg))
		power_supply_changed(fg->batt_psy);

	fg_gen4_update_rslow_coeff(fg, batt_temp);
	return IRQ_HANDLED;
}

static irqreturn_t fg_soc_ready_irq_handler(int irq, void *data)
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

	rc = fg_gen4_charge_full_update(fg);
	if (rc < 0)
		pr_err("Error in charge_full_update, rc=%d\n", rc);

	return IRQ_HANDLED;
}

static irqreturn_t fg_delta_msoc_irq_handler(int irq, void *data)
{
	struct fg_dev *fg = data;
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	int rc, batt_soc, batt_temp, msoc_raw;
	bool input_present = is_input_present(fg);

	rc = fg_get_msoc_raw(fg, &msoc_raw);
	if (!rc)
		fg_dbg(fg, FG_IRQ, "irq %d triggered msoc_raw: %d\n", irq,
			msoc_raw);

	get_batt_psy_props(fg);

	rc = fg_get_sram_prop(fg, FG_SRAM_BATT_SOC, &batt_soc);
	if (rc < 0)
		pr_err("Failed to read battery soc rc: %d\n", rc);
	else
		cycle_count_update(chip->counter, (u32)batt_soc >> 24,
			fg->charge_status, fg->charge_done, input_present);

	rc = fg_gen4_get_battery_temp(fg, &batt_temp);
	if (rc < 0) {
		pr_err("Failed to read battery temp rc: %d\n", rc);
	} else {
		if (chip->cl->active)
			cap_learning_update(chip->cl, batt_temp, batt_soc,
				fg->charge_status, fg->charge_done,
				input_present, is_qnovo_en(fg));

		rc = fg_gen4_slope_limit_config(chip, batt_temp);
		if (rc < 0)
			pr_err("Error in configuring slope limiter rc:%d\n",
				rc);
	}

	rc = fg_gen4_charge_full_update(fg);
	if (rc < 0)
		pr_err("Error in charge_full_update, rc=%d\n", rc);

	rc = fg_gen4_esr_soh_update(fg);
	if (rc < 0)
		pr_err("Error in updating ESR for SOH, rc=%d\n", rc);

	rc = fg_gen4_update_maint_soc(fg);
	if (rc < 0)
		pr_err("Error in updating maint_soc, rc=%d\n", rc);

	rc = fg_gen4_adjust_ki_coeff_dischg(fg);
	if (rc < 0)
		pr_err("Error in adjusting ki_coeff_dischg, rc=%d\n", rc);

	/*
	 * If ESR fast calibration is done even before 3 delta ESR interrupts
	 * had fired, then it is possibly a failed attempt. In such cases,
	 * retry ESR fast calibration once again. This will get restored to
	 * normal config once the timer expires or delta ESR interrupt count
	 * reaches the threshold.
	 */
	if (chip->esr_fast_calib && chip->esr_fast_calib_done &&
		(chip->delta_esr_count < 3) && !chip->esr_fast_calib_retry) {
		rc = fg_gen4_esr_fast_calib_config(chip, true);
		if (rc < 0)
			pr_err("Error in configuring esr_fast_calib, rc=%d\n",
				rc);
		else
			chip->esr_fast_calib_retry = true;
	}

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

static struct fg_irq_info fg_irqs[FG_GEN4_IRQ_MAX] = {
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
		.handler	= fg_soc_ready_irq_handler,
		.wakeable	= true,
	},
	[SOC_UPDATE_IRQ] = {
		.name		= "soc-update",
		.handler	= fg_soc_update_irq_handler,
	},
	/* BATT_INFO irqs */
	[ESR_DELTA_IRQ] = {
		.name		= "esr-delta",
		.handler	= fg_delta_esr_irq_handler,
		.wakeable	= true,
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
	[MEM_ATTN_IRQ] = {
		.name		= "mem-attn",
		.handler	= fg_mem_attn_irq_handler,
		.wakeable	= true,
	},
	[DMA_GRANT_IRQ] = {
		.name		= "dma-grant",
		.handler	= fg_dummy_irq_handler,
		.wakeable	= true,
	},
	[MEM_XCP_IRQ] = {
		.name		= "ima-xcp",
		.handler	= fg_mem_xcp_irq_handler,
		.wakeable	= true,
	},
	[DMA_XCP_IRQ] = {
		.name		= "dma-xcp",
		.handler	= fg_dummy_irq_handler,
		.wakeable	= true,
	},
	[IMA_RDY_IRQ] = {
		.name		= "ima-rdy",
		.handler	= fg_dummy_irq_handler,
	},
	/* ADC_RR irqs */
	[BATT_TEMP_COLD_IRQ] = {
		.name		= "batt-temp-cold",
		.handler	= fg_batt_temp_irq_handler,
		.wakeable	= true,
	},
	[BATT_TEMP_HOT_IRQ] = {
		.name		= "batt-temp-hot",
		.handler	= fg_batt_temp_irq_handler,
		.wakeable	= true,
	},
	[BATT_TEMP_DELTA_IRQ] = {
		.name		= "batt-temp-delta",
		.handler	= fg_delta_batt_temp_irq_handler,
		.wakeable	= true,
	},
	[BATT_ID_IRQ] = {
		.name		= "batt-id",
		.handler	= fg_dummy_irq_handler,
	},
	[BATT_MISSING_IRQ] = {
		.name		= "batt-missing",
		.handler	= fg_batt_missing_irq_handler,
		.wakeable	= true,
	},
};

static enum alarmtimer_restart fg_esr_fast_cal_timer(struct alarm *alarm,
							ktime_t time)
{
	struct fg_gen4_chip *chip = container_of(alarm, struct fg_gen4_chip,
					esr_fast_cal_timer);
	struct fg_dev *fg = &chip->fg;

	if (!chip->esr_fast_calib_done) {
		fg_dbg(fg, FG_STATUS, "ESR fast calibration timer expired\n");

		/*
		 * We cannot vote for awake votable here as that takes
		 * a mutex lock and this is executed in an atomic context.
		 */
		pm_stay_awake(fg->dev);
		chip->esr_fast_cal_timer_expired = true;
		schedule_work(&chip->esr_calib_work);
	}

	return ALARMTIMER_NORESTART;
}

static void esr_calib_work(struct work_struct *work)
{
	struct fg_gen4_chip *chip = container_of(work, struct fg_gen4_chip,
				    esr_calib_work);
	struct fg_dev *fg = &chip->fg;
	int rc, fg_esr_meas_diff;
	s16 esr_raw, esr_char_raw, esr_delta, esr_meas_diff, esr_filtered;
	u8 buf[2];

	if (chip->delta_esr_count > chip->dt.delta_esr_disable_count ||
		chip->esr_fast_calib_done) {
		fg_dbg(fg, FG_STATUS, "delta_esr_count: %d esr_fast_calib_done:%d\n",
			chip->delta_esr_count, chip->esr_fast_calib_done);
		goto out;
	}

	/*
	 * If the number of delta ESR interrupts fired is more than the count
	 * to disable the interrupt OR ESR fast calibration timer is expired
	 * OR after one retry, disable ESR fast calibration.
	 */
	if (chip->delta_esr_count >= chip->dt.delta_esr_disable_count ||
		chip->esr_fast_cal_timer_expired) {
		rc = fg_gen4_esr_fast_calib_config(chip, false);
		if (rc < 0)
			pr_err("Error in configuring esr_fast_calib, rc=%d\n",
				rc);

		if (chip->esr_fast_cal_timer_expired) {
			pm_relax(fg->dev);
			chip->esr_fast_cal_timer_expired = false;
		}

		if (chip->esr_fast_calib_retry)
			chip->esr_fast_calib_retry = false;

		goto out;
	}

	rc = fg_sram_read(fg, ESR_WORD, ESR_OFFSET, buf, 2,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading ESR, rc=%d\n", rc);
		goto out;
	}
	esr_raw = buf[1] << 8 | buf[0];

	rc = fg_sram_read(fg, ESR_CHAR_WORD, ESR_CHAR_OFFSET, buf, 2,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading ESR_CHAR, rc=%d\n", rc);
		goto out;
	}
	esr_char_raw = buf[1] << 8 | buf[0];

	esr_meas_diff = esr_raw - esr_char_raw;

	rc = fg_sram_read(fg, ESR_DELTA_DISCHG_WORD, ESR_DELTA_DISCHG_OFFSET,
			buf, 2, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading ESR_DELTA_DISCHG, rc=%d\n", rc);
		goto out;
	}
	esr_delta = buf[1] << 8 | buf[0];
	fg_dbg(fg, FG_STATUS, "esr_raw: 0x%x esr_char_raw: 0x%x esr_meas_diff: 0x%x esr_delta: 0x%x\n",
		esr_raw, esr_char_raw, esr_meas_diff, esr_delta);

	fg_esr_meas_diff = esr_meas_diff - (esr_delta / 32);

	/* Don't filter for the first attempt so that ESR can converge faster */
	if (!chip->delta_esr_count)
		esr_filtered = fg_esr_meas_diff;
	else
		esr_filtered = fg_esr_meas_diff >> chip->dt.esr_filter_factor;

	esr_delta = esr_delta + (esr_filtered * 32);

	/* Bound the limits */
	if (esr_delta > SHRT_MAX)
		esr_delta = SHRT_MAX;
	else if (esr_delta < SHRT_MIN)
		esr_delta = SHRT_MIN;

	fg_dbg(fg, FG_STATUS, "fg_esr_meas_diff: 0x%x esr_filt: 0x%x esr_delta_new: 0x%x\n",
		fg_esr_meas_diff, esr_filtered, esr_delta);

	buf[0] = esr_delta & 0xff;
	buf[1] = (esr_delta >> 8) & 0xff;
	rc = fg_sram_write(fg, ESR_DELTA_DISCHG_WORD, ESR_DELTA_DISCHG_OFFSET,
			buf, 2, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR_DELTA_DISCHG, rc=%d\n", rc);
		goto out;
	}

	rc = fg_sram_write(fg, ESR_DELTA_CHG_WORD, ESR_DELTA_CHG_OFFSET,
			buf, 2, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR_DELTA_CHG, rc=%d\n", rc);
		goto out;
	}

	chip->delta_esr_count++;
	fg_dbg(fg, FG_STATUS, "Wrote ESR delta [0x%x 0x%x]\n", buf[0], buf[1]);
out:
	vote(fg->awake_votable, ESR_CALIB, false, 0);
}

static void pl_current_en_work(struct work_struct *work)
{
	struct fg_gen4_chip *chip = container_of(work,
				struct fg_gen4_chip,
				pl_current_en_work);
	struct fg_dev *fg = &chip->fg;
	bool input_present = is_input_present(fg), en;

	en = fg->charge_done ? false : input_present;

	if (get_effective_result(chip->parallel_current_en_votable) == en)
		return;

	vote(chip->parallel_current_en_votable, FG_PARALLEL_EN_VOTER, en, 0);
}

static void pl_enable_work(struct work_struct *work)
{
	struct fg_gen4_chip *chip = container_of(work,
				struct fg_gen4_chip,
				pl_enable_work.work);
	struct fg_dev *fg = &chip->fg;

	if (chip->pl_disable_votable)
		vote(chip->pl_disable_votable, ESR_FCC_VOTER, false, 0);
	if (chip->cp_disable_votable)
		vote(chip->cp_disable_votable, ESR_FCC_VOTER, false, 0);
	vote(fg->awake_votable, ESR_FCC_VOTER, false, 0);
}

static void status_change_work(struct work_struct *work)
{
	struct fg_dev *fg = container_of(work,
			struct fg_dev, status_change_work);
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	int rc, batt_soc, batt_temp;
	bool input_present, qnovo_en;

	if (fg->battery_missing) {
		pm_relax(fg->dev);
		return;
	}

	if (!chip->pl_disable_votable)
		chip->pl_disable_votable = find_votable("PL_DISABLE");

	if (!chip->cp_disable_votable)
		chip->cp_disable_votable = find_votable("CP_DISABLE");

	if (!batt_psy_initialized(fg)) {
		fg_dbg(fg, FG_STATUS, "Charger not available?!\n");
		goto out;
	}

	if (!fg->soc_reporting_ready) {
		fg_dbg(fg, FG_STATUS, "Profile load is not complete yet\n");
		goto out;
	}

	get_batt_psy_props(fg);

	rc = fg_get_sram_prop(fg, FG_SRAM_BATT_SOC, &batt_soc);
	if (rc < 0) {
		pr_err("Failed to read battery soc rc: %d\n", rc);
		goto out;
	}

	rc = fg_gen4_get_battery_temp(fg, &batt_temp);
	if (rc < 0) {
		pr_err("Failed to read battery temp rc: %d\n", rc);
		goto out;
	}

	input_present = is_input_present(fg);
	qnovo_en = is_qnovo_en(fg);
	cycle_count_update(chip->counter, (u32)batt_soc >> 24,
		fg->charge_status, fg->charge_done, input_present);

	if (fg->charge_status != fg->prev_charge_status)
		cap_learning_update(chip->cl, batt_temp, batt_soc,
			fg->charge_status, fg->charge_done, input_present,
			qnovo_en);

	rc = fg_gen4_charge_full_update(fg);
	if (rc < 0)
		pr_err("Error in charge_full_update, rc=%d\n", rc);

	rc = fg_gen4_slope_limit_config(chip, batt_temp);
	if (rc < 0)
		pr_err("Error in configuring slope limiter rc:%d\n", rc);

	rc = fg_gen4_adjust_ki_coeff_dischg(fg);
	if (rc < 0)
		pr_err("Error in adjusting ki_coeff_dischg, rc=%d\n", rc);

	rc = fg_gen4_adjust_ki_coeff_full_soc(chip, batt_temp);
	if (rc < 0)
		pr_err("Error in configuring ki_coeff_full_soc rc:%d\n", rc);

	rc = fg_gen4_adjust_recharge_soc(chip);
	if (rc < 0)
		pr_err("Error in adjusting recharge SOC, rc=%d\n", rc);

	rc = fg_gen4_esr_fcc_config(chip);
	if (rc < 0)
		pr_err("Error in adjusting FCC for ESR, rc=%d\n", rc);

	if (is_parallel_charger_available(fg)) {
		cancel_work_sync(&chip->pl_current_en_work);
		schedule_work(&chip->pl_current_en_work);
	}

	ttf_update(chip->ttf, input_present);
	fg->prev_charge_status = fg->charge_status;
out:
	fg_dbg(fg, FG_STATUS, "charge_status:%d charge_type:%d charge_done:%d\n",
		fg->charge_status, fg->charge_type, fg->charge_done);
	pm_relax(fg->dev);
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
	struct fg_gen4_chip *chip;
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

	power_supply_put(bms_psy);
	if (fg->battery_missing) {
		pr_warn("Battery is missing\n");
		return 0;
	}

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
	struct fg_gen4_chip *chip;
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
	power_supply_put(bms_psy);
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

static int fg_esr_fast_cal_sysfs(const char *val, const struct kernel_param *kp)
{
	int rc;
	struct power_supply *bms_psy;
	struct fg_gen4_chip *chip;
	bool old_val = fg_esr_fast_cal_en;

	rc = param_set_bool(val, kp);
	if (rc) {
		pr_err("Unable to set fg_sram_dump: %d\n", rc);
		return rc;
	}

	if (fg_esr_fast_cal_en == old_val)
		return 0;

	bms_psy = power_supply_get_by_name("bms");
	if (!bms_psy) {
		pr_err("bms psy not found\n");
		return -ENODEV;
	}

	chip = power_supply_get_drvdata(bms_psy);
	power_supply_put(bms_psy);

	if (!chip)
		return -ENODEV;

	if (fg_esr_fast_cal_en)
		chip->delta_esr_count = 0;

	rc = fg_gen4_esr_fast_calib_config(chip, fg_esr_fast_cal_en);
	if (rc < 0)
		return rc;

	return 0;
}

static struct kernel_param_ops fg_esr_cal_ops = {
	.set = fg_esr_fast_cal_sysfs,
	.get = param_get_bool,
};

module_param_cb(esr_fast_cal_en, &fg_esr_cal_ops, &fg_esr_fast_cal_en, 0644);

/* All power supply functions here */

static int fg_psy_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *pval)
{
	struct fg_gen4_chip *chip = power_supply_get_drvdata(psy);
	struct fg_dev *fg = &chip->fg;
	int rc = 0, val;
	int64_t temp;

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = fg_gen4_get_prop_capacity(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_RAW:
		rc = fg_get_msoc_raw(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CC_SOC:
		rc = fg_get_sram_prop(&chip->fg, FG_SRAM_CC_SOC, &val);
		if (rc < 0) {
			pr_err("Error in getting CC_SOC, rc=%d\n", rc);
			return rc;
		}
		/* Show it in centi-percentage */
		pval->intval = div_s64((int64_t)val * 10000,  CC_SOC_30BIT);
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
		rc = fg_gen4_get_battery_temp(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		rc = fg_get_battery_resistance(fg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_ESR_ACTUAL:
		pval->intval = chip->esr_actual;
		break;
	case POWER_SUPPLY_PROP_ESR_NOMINAL:
		pval->intval = chip->esr_nominal;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		rc = fg_get_sram_prop(fg, FG_SRAM_OCV, &pval->intval);
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
	case POWER_SUPPLY_PROP_CHARGE_NOW_RAW:
		rc = fg_gen4_get_charge_raw(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		rc = fg_gen4_get_learned_capacity(chip, &temp);
		if (!rc)
			pval->intval = (int)temp;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		rc = fg_gen4_get_nominal_capacity(chip, &temp);
		if (!rc)
			pval->intval = (int)temp;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		rc = fg_gen4_get_charge_counter(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW:
		rc = fg_gen4_get_charge_counter_shadow(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		rc = get_cycle_count(chip->counter, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNTS:
		rc = get_cycle_counts(chip->counter, &pval->strval);
		if (rc < 0)
			pval->strval = NULL;
		break;
	case POWER_SUPPLY_PROP_SOC_REPORTING_READY:
		pval->intval = fg->soc_reporting_ready;
		break;
	case POWER_SUPPLY_PROP_CLEAR_SOH:
		pval->intval = chip->first_profile_load;
		break;
	case POWER_SUPPLY_PROP_SOH:
		pval->intval = chip->soh;
		break;
	case POWER_SUPPLY_PROP_DEBUG_BATTERY:
		pval->intval = is_debug_batt_id(fg);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		rc = fg_get_sram_prop(fg, FG_SRAM_VBATT_FULL, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
		rc = ttf_get_time_to_full(chip->ttf, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		rc = ttf_get_time_to_empty(chip->ttf, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CC_STEP:
		if ((chip->ttf->cc_step.sel >= 0) &&
				(chip->ttf->cc_step.sel < MAX_CC_STEPS)) {
			pval->intval =
				chip->ttf->cc_step.arr[chip->ttf->cc_step.sel];
		} else {
			pr_err("cc_step_sel is out of bounds [0, %d]\n",
				chip->ttf->cc_step.sel);
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CC_STEP_SEL:
		pval->intval = chip->ttf->cc_step.sel;
		break;
	case POWER_SUPPLY_PROP_BATT_AGE_LEVEL:
		pval->intval = chip->batt_age_level;
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
	struct fg_gen4_chip *chip = power_supply_get_drvdata(psy);
	struct fg_dev *fg = &chip->fg;
	int rc = 0;
	u8 val, mask;

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (chip->cl->active) {
			pr_warn("Capacity learning active!\n");
			return 0;
		}
		if (pval->intval <= 0 || pval->intval > chip->cl->nom_cap_uah) {
			pr_err("charge_full is out of bounds\n");
			return -EINVAL;
		}
		mutex_lock(&chip->cl->lock);
		rc = fg_gen4_store_learned_capacity(chip, pval->intval);
		if (!rc)
			chip->cl->learned_cap_uah = pval->intval;
		mutex_unlock(&chip->cl->lock);
		break;
	case POWER_SUPPLY_PROP_CC_STEP:
		if ((chip->ttf->cc_step.sel >= 0) &&
				(chip->ttf->cc_step.sel < MAX_CC_STEPS)) {
			chip->ttf->cc_step.arr[chip->ttf->cc_step.sel] =
								pval->intval;
		} else {
			pr_err("cc_step_sel is out of bounds [0, %d]\n",
				chip->ttf->cc_step.sel);
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CC_STEP_SEL:
		if ((pval->intval >= 0) && (pval->intval < MAX_CC_STEPS)) {
			chip->ttf->cc_step.sel = pval->intval;
		} else {
			pr_err("cc_step_sel is out of bounds [0, %d]\n",
				pval->intval);
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_ESR_ACTUAL:
		chip->esr_actual = pval->intval;
		break;
	case POWER_SUPPLY_PROP_ESR_NOMINAL:
		chip->esr_nominal = pval->intval;
		break;
	case POWER_SUPPLY_PROP_SOH:
		chip->soh = pval->intval;
		break;
	case POWER_SUPPLY_PROP_CLEAR_SOH:
		if (chip->first_profile_load && !pval->intval) {
			fg_dbg(fg, FG_STATUS, "Clearing first profile load bit\n");
			val = 0;
			mask = FIRST_PROFILE_LOAD_BIT;
			rc = fg_sram_masked_write(fg, PROFILE_INTEGRITY_WORD,
					PROFILE_INTEGRITY_OFFSET, mask, val,
					FG_IMA_DEFAULT);
			if (rc < 0)
				pr_err("Error in writing to profile integrity word rc=%d\n",
					rc);
			else
				chip->first_profile_load = false;
		}
		break;
	case POWER_SUPPLY_PROP_BATT_AGE_LEVEL:
		if (!chip->dt.multi_profile_load || pval->intval < 0 ||
			chip->batt_age_level == pval->intval)
			return -EINVAL;
		chip->last_batt_age_level = chip->batt_age_level;
		chip->batt_age_level = pval->intval;
		schedule_delayed_work(&fg->profile_load_work, 0);
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
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CC_STEP:
	case POWER_SUPPLY_PROP_CC_STEP_SEL:
	case POWER_SUPPLY_PROP_ESR_ACTUAL:
	case POWER_SUPPLY_PROP_ESR_NOMINAL:
	case POWER_SUPPLY_PROP_SOH:
	case POWER_SUPPLY_PROP_CLEAR_SOH:
	case POWER_SUPPLY_PROP_BATT_AGE_LEVEL:
		return 1;
	default:
		break;
	}

	return 0;
}

static enum power_supply_property fg_psy_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_RAW,
	POWER_SUPPLY_PROP_CC_SOC,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_ESR_ACTUAL,
	POWER_SUPPLY_PROP_ESR_NOMINAL,
	POWER_SUPPLY_PROP_BATTERY_TYPE,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_NOW_RAW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW,
	POWER_SUPPLY_PROP_CYCLE_COUNTS,
	POWER_SUPPLY_PROP_SOC_REPORTING_READY,
	POWER_SUPPLY_PROP_CLEAR_SOH,
	POWER_SUPPLY_PROP_SOH,
	POWER_SUPPLY_PROP_DEBUG_BATTERY,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_CC_STEP,
	POWER_SUPPLY_PROP_CC_STEP_SEL,
	POWER_SUPPLY_PROP_BATT_AGE_LEVEL,
};

static const struct power_supply_desc fg_psy_desc = {
	.name = "bms",
	.type = POWER_SUPPLY_TYPE_BMS,
	.properties = fg_psy_props,
	.num_properties = ARRAY_SIZE(fg_psy_props),
	.get_property = fg_psy_get_property,
	.set_property = fg_psy_set_property,
	.property_is_writeable = fg_property_is_writeable,
};

/* All callback functions below */

static int fg_notifier_cb(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct fg_dev *fg = container_of(nb, struct fg_dev, nb);

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
		pm_stay_awake(fg->dev);
		schedule_work(&fg->status_change_work);
	}

	return NOTIFY_OK;
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

static int fg_gen4_ttf_awake_voter(void *data, bool val)
{
	struct fg_gen4_chip *chip = data;
	struct fg_dev *fg = &chip->fg;

	if (!chip)
		return -ENODEV;

	if (fg->battery_missing ||
		fg->profile_load_status == PROFILE_NOT_LOADED)
		return -EPERM;

	vote(fg->awake_votable, TTF_AWAKE_VOTER, val, 0);
	return 0;
}

static int fg_wait_for_mem_attn(struct fg_gen4_chip *chip)
{
	struct fg_dev *fg = &chip->fg;
	int rc, retries = 2;
	ktime_t now;
	s64 time_us;

	reinit_completion(&chip->mem_attn);
	now = ktime_get();

	while (retries--) {
		/* Wait for MEM_ATTN completion */
		rc = wait_for_completion_interruptible_timeout(
			&chip->mem_attn, msecs_to_jiffies(1000));
		if (rc > 0) {
			rc = 0;
			break;
		} else if (!rc) {
			rc = -ETIMEDOUT;
		}
	}

	time_us = ktime_us_delta(ktime_get(), now);
	if (rc < 0)
		pr_err("wait for mem_attn timed out rc=%d\n", rc);

	fg_dbg(fg, FG_STATUS, "mem_attn wait time: %lld us\n", time_us);
	return rc;
}

static int fg_parallel_current_en_cb(struct votable *votable, void *data,
					int enable, const char *client)
{
	struct fg_dev *fg = data;
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	int rc;
	u8 val, mask;

	vote(chip->mem_attn_irq_en_votable, MEM_ATTN_IRQ_VOTER, true, 0);

	/* Wait for MEM_ATTN interrupt */
	rc = fg_wait_for_mem_attn(chip);
	if (rc < 0)
		return rc;

	val = enable ? SMB_MEASURE_EN_BIT : 0;
	mask = SMB_MEASURE_EN_BIT;
	rc = fg_masked_write(fg, BATT_INFO_FG_CNV_CHAR_CFG(fg), mask, val);
	if (rc < 0)
		pr_err("Error in writing to 0x%04x, rc=%d\n",
			BATT_INFO_FG_CNV_CHAR_CFG(fg), rc);

	vote(chip->mem_attn_irq_en_votable, MEM_ATTN_IRQ_VOTER, false, 0);
	fg_dbg(fg, FG_STATUS, "Parallel current summing: %d\n", enable);

	return rc;
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

static int fg_gen4_delta_esr_irq_en_cb(struct votable *votable, void *data,
					int enable, const char *client)
{
	struct fg_dev *fg = data;

	if (!fg->irqs[ESR_DELTA_IRQ].irq)
		return 0;

	if (enable) {
		enable_irq(fg->irqs[ESR_DELTA_IRQ].irq);
		enable_irq_wake(fg->irqs[ESR_DELTA_IRQ].irq);
	} else {
		disable_irq_wake(fg->irqs[ESR_DELTA_IRQ].irq);
		disable_irq_nosync(fg->irqs[ESR_DELTA_IRQ].irq);
	}

	return 0;
}

static int fg_gen4_mem_attn_irq_en_cb(struct votable *votable, void *data,
					int enable, const char *client)
{
	struct fg_dev *fg = data;

	if (!fg->irqs[MEM_ATTN_IRQ].irq)
		return 0;

	if (enable) {
		enable_irq(fg->irqs[MEM_ATTN_IRQ].irq);
		enable_irq_wake(fg->irqs[MEM_ATTN_IRQ].irq);
	} else {
		disable_irq_wake(fg->irqs[MEM_ATTN_IRQ].irq);
		disable_irq_nosync(fg->irqs[MEM_ATTN_IRQ].irq);
	}

	fg_dbg(fg, FG_STATUS, "%sabled mem_attn irq\n", enable ? "en" : "dis");
	return 0;
}

/* All init functions below this */

static int fg_alg_init(struct fg_gen4_chip *chip)
{
	struct fg_dev *fg = &chip->fg;
	struct cycle_counter *counter;
	struct cap_learning *cl;
	struct ttf *ttf;
	int rc;

	counter = devm_kzalloc(fg->dev, sizeof(*counter), GFP_KERNEL);
	if (!counter)
		return -ENOMEM;

	counter->restore_count = fg_gen4_restore_count;
	counter->store_count = fg_gen4_store_count;
	counter->data = chip;

	rc = cycle_count_init(counter);
	if (rc < 0) {
		dev_err(fg->dev, "Error in initializing cycle counter, rc:%d\n",
			rc);
		counter->data = NULL;
		devm_kfree(fg->dev, counter);
		return rc;
	}

	chip->counter = counter;

	cl = devm_kzalloc(fg->dev, sizeof(*cl), GFP_KERNEL);
	if (!cl)
		return -ENOMEM;

	cl->cc_soc_max = CC_SOC_30BIT;
	cl->get_cc_soc = fg_gen4_get_cc_soc_sw;
	cl->prime_cc_soc = fg_gen4_prime_cc_soc_sw;
	cl->get_learned_capacity = fg_gen4_get_learned_capacity;
	cl->store_learned_capacity = fg_gen4_store_learned_capacity;
	cl->data = chip;

	rc = cap_learning_init(cl);
	if (rc < 0) {
		dev_err(fg->dev, "Error in initializing capacity learning, rc:%d\n",
			rc);
		counter->data = NULL;
		cl->data = NULL;
		devm_kfree(fg->dev, counter);
		devm_kfree(fg->dev, cl);
		return rc;
	}

	chip->cl = cl;

	ttf = devm_kzalloc(fg->dev, sizeof(*ttf), GFP_KERNEL);
	if (!ttf)
		return -ENOMEM;

	ttf->get_ttf_param = fg_gen4_get_ttf_param;
	ttf->awake_voter = fg_gen4_ttf_awake_voter;
	ttf->iterm_delta = 0;
	ttf->data = chip;

	rc = ttf_tte_init(ttf);
	if (rc < 0) {
		dev_err(fg->dev, "Error in initializing ttf, rc:%d\n", rc);
		ttf->data = NULL;
		counter->data = NULL;
		cl->data = NULL;
		devm_kfree(fg->dev, ttf);
		devm_kfree(fg->dev, counter);
		devm_kfree(fg->dev, cl);
		return rc;
	}

	chip->ttf = ttf;

	return 0;
}

static int fg_gen4_esr_calib_config(struct fg_gen4_chip *chip)
{
	struct fg_dev *fg = &chip->fg;
	u8 buf[2], val, mask;
	int rc;

	if (chip->esr_fast_calib) {
		rc = fg_gen4_esr_fast_calib_config(chip, true);
		if (rc < 0)
			return rc;
	} else {
		if (chip->dt.esr_timer_chg_slow[TIMER_RETRY] >= 0 &&
			chip->dt.esr_timer_chg_slow[TIMER_MAX] >= 0) {
			rc = fg_set_esr_timer(fg,
				chip->dt.esr_timer_chg_slow[TIMER_RETRY],
				chip->dt.esr_timer_chg_slow[TIMER_MAX], true,
				FG_IMA_DEFAULT);
			if (rc < 0) {
				pr_err("Error in setting ESR charge timer, rc=%d\n",
					rc);
				return rc;
			}
		}

		if (chip->dt.esr_timer_dischg_slow[TIMER_RETRY] >= 0 &&
			chip->dt.esr_timer_dischg_slow[TIMER_MAX] >= 0) {
			rc = fg_set_esr_timer(fg,
				chip->dt.esr_timer_dischg_slow[TIMER_RETRY],
				chip->dt.esr_timer_dischg_slow[TIMER_MAX],
				false, FG_IMA_DEFAULT);
			if (rc < 0) {
				pr_err("Error in setting ESR discharge timer, rc=%d\n",
					rc);
				return rc;
			}
		}

		if (chip->dt.esr_calib_dischg) {
			/* Allow ESR calibration only during discharging */
			val = BIT(6) | BIT(7);
			mask = BIT(1) | BIT(6) | BIT(7);
			rc = fg_sram_masked_write(fg, SYS_CONFIG_WORD,
					SYS_CONFIG_OFFSET, mask, val,
					FG_IMA_DEFAULT);
			if (rc < 0) {
				pr_err("Error in writing SYS_CONFIG_WORD, rc=%d\n",
					rc);
				return rc;
			}

			/* Disable ESR charging timer */
			val = 0;
			mask = BIT(0);
			rc = fg_sram_masked_write(fg, SYS_CONFIG_WORD,
					SYS_CONFIG2_OFFSET, mask, val,
					FG_IMA_DEFAULT);
			if (rc < 0) {
				pr_err("Error in writing SYS_CONFIG2_OFFSET, rc=%d\n",
					rc);
				return rc;
			}
		} else {
			/*
			 * Disable ESR discharging timer and ESR pulsing during
			 * discharging when ESR fast calibration is disabled.
			 */
			val = 0;
			mask = BIT(6) | BIT(7);
			rc = fg_sram_masked_write(fg, SYS_CONFIG_WORD,
					SYS_CONFIG_OFFSET, mask, val,
					FG_IMA_DEFAULT);
			if (rc < 0) {
				pr_err("Error in writing SYS_CONFIG_WORD, rc=%d\n",
					rc);
				return rc;
			}
		}
	}

	/*
	 * Delta ESR interrupt threshold should be configured as specified if
	 * ESR fast calibration is disabled. Else, set it to max (4000 mOhms).
	 */
	fg_encode(fg->sp, FG_SRAM_DELTA_ESR_THR,
		chip->esr_fast_calib ? 4000000 : chip->dt.delta_esr_thr_uohms,
		buf);
	rc = fg_sram_write(fg,
			fg->sp[FG_SRAM_DELTA_ESR_THR].addr_word,
			fg->sp[FG_SRAM_DELTA_ESR_THR].addr_byte, buf,
			fg->sp[FG_SRAM_DELTA_ESR_THR].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing DELTA_ESR_THR, rc=%d\n", rc);
		return rc;
	}

	return rc;
}

#define BATT_TEMP_HYST_MASK	GENMASK(3, 0)
#define BATT_TEMP_DELTA_MASK	GENMASK(7, 4)
#define BATT_TEMP_DELTA_SHIFT	4
static int fg_gen4_hw_init(struct fg_gen4_chip *chip)
{
	struct fg_dev *fg = &chip->fg;
	int rc;
	u8 buf[4], val, mask;

	rc = fg_read(fg, ADC_RR_INT_RT_STS(fg), &val, 1);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			ADC_RR_INT_RT_STS(fg), rc);
		return rc;
	}
	fg->battery_missing = (val & ADC_RR_BT_MISS_BIT);

	if (fg->battery_missing) {
		pr_warn("Not initializing FG because of battery missing\n");
		return 0;
	}

	fg_encode(fg->sp, FG_SRAM_CUTOFF_VOLT, chip->dt.cutoff_volt_mv, buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_CUTOFF_VOLT].addr_word,
			fg->sp[FG_SRAM_CUTOFF_VOLT].addr_byte, buf,
			fg->sp[FG_SRAM_CUTOFF_VOLT].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing cutoff_volt, rc=%d\n", rc);
		return rc;
	}

	rc = fg_gen4_configure_cutoff_current(fg, chip->dt.cutoff_curr_ma);
	if (rc < 0)
		return rc;

	fg_encode(fg->sp, FG_SRAM_SYS_TERM_CURR, chip->dt.sys_term_curr_ma,
		buf);
	rc = fg_sram_write(fg, fg->sp[FG_SRAM_SYS_TERM_CURR].addr_word,
			fg->sp[FG_SRAM_SYS_TERM_CURR].addr_byte, buf,
			fg->sp[FG_SRAM_SYS_TERM_CURR].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing sys_term_curr, rc=%d\n", rc);
		return rc;
	}

	if (chip->dt.empty_volt_mv > 0) {
		fg_encode(fg->sp, FG_SRAM_VBATT_LOW,
			chip->dt.empty_volt_mv, buf);
		rc = fg_sram_write(fg, fg->sp[FG_SRAM_VBATT_LOW].addr_word,
				fg->sp[FG_SRAM_VBATT_LOW].addr_byte, buf,
				fg->sp[FG_SRAM_VBATT_LOW].len,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing empty_volt_mv, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.delta_soc_thr > 0 && chip->dt.delta_soc_thr < 125) {
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

	if (chip->dt.batt_temp_cold_thresh != -EINVAL) {
		fg_encode(fg->sp, FG_SRAM_BATT_TEMP_COLD,
			chip->dt.batt_temp_cold_thresh, buf);
		rc = fg_sram_write(fg, fg->sp[FG_SRAM_BATT_TEMP_COLD].addr_word,
				fg->sp[FG_SRAM_BATT_TEMP_COLD].addr_byte, buf,
				fg->sp[FG_SRAM_BATT_TEMP_COLD].len,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing batt_temp_cold_thresh, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (chip->dt.batt_temp_hot_thresh != -EINVAL) {
		fg_encode(fg->sp, FG_SRAM_BATT_TEMP_HOT,
			chip->dt.batt_temp_hot_thresh, buf);
		rc = fg_sram_write(fg, fg->sp[FG_SRAM_BATT_TEMP_HOT].addr_word,
				fg->sp[FG_SRAM_BATT_TEMP_HOT].addr_byte, buf,
				fg->sp[FG_SRAM_BATT_TEMP_HOT].len,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing batt_temp_hot_thresh, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (chip->dt.batt_temp_hyst != -EINVAL) {
		val = chip->dt.batt_temp_hyst & BATT_TEMP_HYST_MASK;
		mask = BATT_TEMP_HYST_MASK;
		rc = fg_sram_masked_write(fg, BATT_TEMP_CONFIG2_WORD,
				BATT_TEMP_HYST_DELTA_OFFSET, mask, val,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing batt_temp_hyst, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.batt_temp_delta != -EINVAL) {
		val = (chip->dt.batt_temp_delta << BATT_TEMP_DELTA_SHIFT)
				& BATT_TEMP_DELTA_MASK;
		mask = BATT_TEMP_DELTA_MASK;
		rc = fg_sram_masked_write(fg, BATT_TEMP_CONFIG2_WORD,
				BATT_TEMP_HYST_DELTA_OFFSET, mask, val,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing batt_temp_delta, rc=%d\n", rc);
			return rc;
		}
	}

	val = (u8)chip->dt.batt_therm_freq;
	rc = fg_write(fg, ADC_RR_BATT_THERM_FREQ(fg), &val, 1);
	if (rc < 0) {
		pr_err("failed to write to 0x%04X, rc=%d\n",
			 ADC_RR_BATT_THERM_FREQ(fg), rc);
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

	if (is_debug_batt_id(fg)) {
		val = ESR_NO_PULL_DOWN;
		rc = fg_masked_write(fg, BATT_INFO_ESR_PULL_DN_CFG(fg),
			ESR_PULL_DOWN_MODE_MASK, val);
		if (rc < 0) {
			pr_err("Error in writing esr_pull_down, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.rconn_uohms) {
		/*
		 * Read back Rconn to see if it's already configured. If it is
		 * a non-zero value, then skip configuring it.
		 */
		rc = fg_sram_read(fg, RCONN_WORD, RCONN_OFFSET, buf, 2,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error reading Rconn, rc=%d\n", rc);
			return rc;
		}

		if (!buf[0] && !buf[1]) {
			/* Rconn has same encoding as ESR */
			fg_encode(fg->sp, FG_SRAM_ESR, chip->dt.rconn_uohms,
				buf);
			rc = fg_sram_write(fg, RCONN_WORD, RCONN_OFFSET, buf, 2,
					FG_IMA_DEFAULT);
			if (rc < 0) {
				pr_err("Error writing Rconn, rc=%d\n", rc);
				return rc;
			}
		} else {
			pr_debug("Skipping configuring Rconn [0x%x 0x%x]\n",
				buf[0], buf[1]);
		}
	}

	if (chip->dt.ki_coeff_low_chg != -EINVAL) {
		fg_encode(fg->sp, FG_SRAM_KI_COEFF_LOW_CHG,
			chip->dt.ki_coeff_low_chg, &val);
		rc = fg_sram_write(fg,
			fg->sp[FG_SRAM_KI_COEFF_LOW_CHG].addr_word,
			fg->sp[FG_SRAM_KI_COEFF_LOW_CHG].addr_byte, &val,
			fg->sp[FG_SRAM_KI_COEFF_LOW_CHG].len,
			FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing ki_coeff_low_chg, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (chip->dt.ki_coeff_med_chg != -EINVAL) {
		fg_encode(fg->sp, FG_SRAM_KI_COEFF_MED_CHG,
			chip->dt.ki_coeff_med_chg, &val);
		rc = fg_sram_write(fg,
			fg->sp[FG_SRAM_KI_COEFF_MED_CHG].addr_word,
			fg->sp[FG_SRAM_KI_COEFF_MED_CHG].addr_byte, &val,
			fg->sp[FG_SRAM_KI_COEFF_MED_CHG].len,
			FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing ki_coeff_med_chg, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (chip->dt.ki_coeff_hi_chg != -EINVAL) {
		fg_encode(fg->sp, FG_SRAM_KI_COEFF_HI_CHG,
			chip->dt.ki_coeff_hi_chg, &val);
		rc = fg_sram_write(fg,
			fg->sp[FG_SRAM_KI_COEFF_HI_CHG].addr_word,
			fg->sp[FG_SRAM_KI_COEFF_HI_CHG].addr_byte, &val,
			fg->sp[FG_SRAM_KI_COEFF_HI_CHG].len,
			FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing ki_coeff_hi_chg, rc=%d\n", rc);
			return rc;
		}
	}

	rc = fg_gen4_esr_calib_config(chip);
	if (rc < 0)
		return rc;

	rc = restore_cycle_count(chip->counter);
	if (rc < 0) {
		pr_err("Error in restoring cycle_count, rc=%d\n", rc);
		return rc;
	}

	chip->batt_age_level = chip->last_batt_age_level = -EINVAL;
	if (chip->dt.multi_profile_load) {
		rc = fg_sram_read(fg, BATT_AGE_LEVEL_WORD,
			BATT_AGE_LEVEL_OFFSET, &val, 1, FG_IMA_DEFAULT);
		if (!rc)
			chip->batt_age_level = chip->last_batt_age_level = val;
	}
	return 0;
}

static int fg_parse_slope_limit_coefficients(struct fg_dev *fg)
{
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	struct device_node *node = fg->dev->of_node;
	int rc, i;

	if (!of_find_property(node, "qcom,slope-limit-coeffs", NULL))
		return 0;

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
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	struct device_node *node = fg->dev->of_node;
	int rc, i;

	if (of_find_property(node, "qcom,ki-coeff-full-dischg", NULL)) {
		rc = fg_parse_dt_property_u32_array(node,
			"qcom,ki-coeff-full-dischg",
			chip->dt.ki_coeff_full_soc_dischg, 2);
		if (rc < 0)
			return rc;

		if (chip->dt.ki_coeff_full_soc_dischg[0] < 62 ||
			chip->dt.ki_coeff_full_soc_dischg[0] > 15564 ||
			chip->dt.ki_coeff_full_soc_dischg[1] < 62 ||
			chip->dt.ki_coeff_full_soc_dischg[1] > 15564) {
			pr_err("Error in ki_coeff_full_soc_dischg values\n");
			return -EINVAL;
		}
	}

	chip->dt.ki_coeff_low_chg = -EINVAL;
	of_property_read_u32(node, "qcom,ki-coeff-low-chg",
		&chip->dt.ki_coeff_low_chg);

	chip->dt.ki_coeff_med_chg = -EINVAL;
	of_property_read_u32(node, "qcom,ki-coeff-med-chg",
		&chip->dt.ki_coeff_med_chg);

	chip->dt.ki_coeff_hi_chg = -EINVAL;
	of_property_read_u32(node, "qcom,ki-coeff-hi-chg",
		&chip->dt.ki_coeff_hi_chg);

	if (!of_find_property(node, "qcom,ki-coeff-soc-dischg", NULL) ||
		(!of_find_property(node, "qcom,ki-coeff-low-dischg", NULL) &&
		!of_find_property(node, "qcom,ki-coeff-med-dischg", NULL) &&
		!of_find_property(node, "qcom,ki-coeff-hi-dischg", NULL)))
		return 0;

	rc = fg_parse_dt_property_u32_array(node, "qcom,ki-coeff-soc-dischg",
		chip->dt.ki_coeff_soc, KI_COEFF_SOC_LEVELS);
	if (rc < 0)
		return rc;

	rc = fg_parse_dt_property_u32_array(node, "qcom,ki-coeff-low-dischg",
		chip->dt.ki_coeff_low_dischg, KI_COEFF_SOC_LEVELS);
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

		if (chip->dt.ki_coeff_low_dischg[i] < 0 ||
			chip->dt.ki_coeff_low_dischg[i] > KI_COEFF_MAX) {
			pr_err("Error in ki_coeff_low_dischg values\n");
			return -EINVAL;
		}

		if (chip->dt.ki_coeff_med_dischg[i] < 0 ||
			chip->dt.ki_coeff_med_dischg[i] > KI_COEFF_MAX) {
			pr_err("Error in ki_coeff_med_dischg values\n");
			return -EINVAL;
		}

		if (chip->dt.ki_coeff_hi_dischg[i] < 0 ||
			chip->dt.ki_coeff_hi_dischg[i] > KI_COEFF_MAX) {
			pr_err("Error in ki_coeff_hi_dischg values\n");
			return -EINVAL;
		}
	}
	chip->ki_coeff_dischg_en = true;
	return 0;
}

#define DEFAULT_ESR_DISABLE_COUNT	5
#define DEFAULT_ESR_FILTER_FACTOR	2
#define DEFAULT_DELTA_ESR_THR		1832
static int fg_parse_esr_cal_params(struct fg_dev *fg)
{
	struct fg_gen4_chip *chip = container_of(fg, struct fg_gen4_chip, fg);
	struct device_node *node = fg->dev->of_node;
	int rc, i, temp;

	if (chip->dt.esr_timer_dischg_slow[TIMER_RETRY] >= 0 &&
			chip->dt.esr_timer_dischg_slow[TIMER_MAX] >= 0) {
		/* ESR calibration only during discharging */
		chip->dt.esr_calib_dischg = of_property_read_bool(node,
						"qcom,fg-esr-calib-dischg");
		if (chip->dt.esr_calib_dischg)
			return 0;
	}

	if (!of_find_property(node, "qcom,fg-esr-cal-soc-thresh", NULL) ||
		!of_find_property(node, "qcom,fg-esr-cal-temp-thresh", NULL))
		return 0;

	rc = fg_parse_dt_property_u32_array(node, "qcom,fg-esr-cal-soc-thresh",
		chip->dt.esr_cal_soc_thresh, ESR_CAL_LEVELS);
	if (rc < 0) {
		pr_err("Invalid SOC thresholds for ESR fast cal, rc=%d\n", rc);
		return rc;
	}

	rc = fg_parse_dt_property_u32_array(node, "qcom,fg-esr-cal-temp-thresh",
		chip->dt.esr_cal_temp_thresh, ESR_CAL_LEVELS);
	if (rc < 0) {
		pr_err("Invalid temperature thresholds for ESR fast cal, rc=%d\n",
			rc);
		return rc;
	}

	for (i = 0; i < ESR_CAL_LEVELS; i++) {
		if (chip->dt.esr_cal_soc_thresh[i] > FULL_SOC_RAW) {
			pr_err("esr_cal_soc_thresh value shouldn't exceed %d\n",
				FULL_SOC_RAW);
			return -EINVAL;
		}

		if (chip->dt.esr_cal_temp_thresh[i] < ESR_CAL_TEMP_MIN ||
			chip->dt.esr_cal_temp_thresh[i] > ESR_CAL_TEMP_MAX) {
			pr_err("esr_cal_temp_thresh value should be within [%d %d]\n",
				ESR_CAL_TEMP_MIN, ESR_CAL_TEMP_MAX);
			return -EINVAL;
		}
	}

	chip->dt.delta_esr_disable_count = DEFAULT_ESR_DISABLE_COUNT;
	rc = of_property_read_u32(node, "qcom,fg-delta-esr-disable-count",
		&temp);
	if (!rc)
		chip->dt.delta_esr_disable_count = temp;

	chip->dt.esr_filter_factor = DEFAULT_ESR_FILTER_FACTOR;
	rc = of_property_read_u32(node, "qcom,fg-esr-filter-factor",
		&temp);
	if (!rc)
		chip->dt.esr_filter_factor = temp;

	chip->dt.delta_esr_thr_uohms = DEFAULT_DELTA_ESR_THR;
	rc = of_property_read_u32(node, "qcom,fg-delta-esr-thr", &temp);
	if (!rc)
		chip->dt.delta_esr_thr_uohms = temp;

	chip->esr_fast_calib = true;
	return 0;
}

#define DEFAULT_CUTOFF_VOLT_MV		3100
#define DEFAULT_EMPTY_VOLT_MV		2812
#define DEFAULT_SYS_TERM_CURR_MA	-125
#define DEFAULT_CUTOFF_CURR_MA		200
#define DEFAULT_DELTA_SOC_THR		5	/* 0.5 % */
#define DEFAULT_CL_START_SOC		15
#define DEFAULT_CL_MIN_TEMP_DECIDEGC	150
#define DEFAULT_CL_MAX_TEMP_DECIDEGC	500
#define DEFAULT_CL_MAX_INC_DECIPERC	5
#define DEFAULT_CL_MAX_DEC_DECIPERC	100
#define DEFAULT_CL_MIN_LIM_DECIPERC	0
#define DEFAULT_CL_MAX_LIM_DECIPERC	0
#define DEFAULT_CL_DELTA_BATT_SOC	10
#define BTEMP_DELTA_LOW			0
#define BTEMP_DELTA_HIGH		3
#define DEFAULT_ESR_PULSE_THRESH_MA	47
#define DEFAULT_ESR_MEAS_CURR_MA	120
static int fg_gen4_parse_dt(struct fg_gen4_chip *chip)
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
	of_node_put(revid_node);
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
	case PM8150B_SUBTYPE:
		fg->version = GEN4_FG;
		fg->use_dma = true;
		fg->sp = pm8150b_v2_sram_params;
		if (fg->pmic_rev_id->rev4 == PM8150B_V1P0_REV4) {
			fg->sp = pm8150b_v1_sram_params;
			fg->wa_flags |= PM8150B_V1_DMA_WA;
			fg->wa_flags |= PM8150B_V1_RSLOW_COMP_WA;
		} else if (fg->pmic_rev_id->rev4 == PM8150B_V2P0_REV4) {
			fg->wa_flags |= PM8150B_V2_RSLOW_SCALE_FN_WA;
		}
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
		case FG_BATT_SOC_PM8150B:
			fg->batt_soc_base = base;
			break;
		case FG_BATT_INFO_PM8150B:
			fg->batt_info_base = base;
			break;
		case FG_MEM_IF_PM8150B:
			fg->mem_if_base = base;
			break;
		case FG_ADC_RR_PM8150B:
			fg->rradc_base = base;
			break;
		default:
			dev_err(fg->dev, "Invalid peripheral subtype 0x%x\n",
				subtype);
			return -ENXIO;
		}
	}

	/* Read all the optional properties below */
	rc = of_property_read_u32(node, "qcom,fg-cutoff-voltage", &temp);
	if (rc < 0)
		chip->dt.cutoff_volt_mv = DEFAULT_CUTOFF_VOLT_MV;
	else
		chip->dt.cutoff_volt_mv = temp;

	rc = of_property_read_u32(node, "qcom,fg-cutoff-current", &temp);
	if (rc < 0)
		chip->dt.cutoff_curr_ma = DEFAULT_CUTOFF_CURR_MA;
	else
		chip->dt.cutoff_curr_ma = temp;

	rc = of_property_read_u32(node, "qcom,fg-empty-voltage", &temp);
	if (rc < 0)
		chip->dt.empty_volt_mv = DEFAULT_EMPTY_VOLT_MV;
	else
		chip->dt.empty_volt_mv = temp;

	rc = of_property_read_u32(node, "qcom,fg-sys-term-current", &temp);
	if (rc < 0)
		chip->dt.sys_term_curr_ma = DEFAULT_SYS_TERM_CURR_MA;
	else
		chip->dt.sys_term_curr_ma = temp;

	rc = of_property_read_u32(node, "qcom,fg-delta-soc-thr", &temp);
	if (rc < 0)
		chip->dt.delta_soc_thr = DEFAULT_DELTA_SOC_THR;
	else
		chip->dt.delta_soc_thr = temp;

	chip->dt.esr_timer_chg_fast[TIMER_RETRY] = -EINVAL;
	chip->dt.esr_timer_chg_fast[TIMER_MAX] = -EINVAL;
	rc = fg_parse_dt_property_u32_array(node, "qcom,fg-esr-timer-chg-fast",
		chip->dt.esr_timer_chg_fast, NUM_ESR_TIMERS);
	if (rc < 0)
		return rc;

	chip->dt.esr_timer_dischg_fast[TIMER_RETRY] = -EINVAL;
	chip->dt.esr_timer_dischg_fast[TIMER_MAX] = -EINVAL;
	rc = fg_parse_dt_property_u32_array(node,
		"qcom,fg-esr-timer-dischg-fast", chip->dt.esr_timer_dischg_fast,
		NUM_ESR_TIMERS);
	if (rc < 0)
		return rc;

	chip->dt.esr_timer_chg_slow[TIMER_RETRY] = -EINVAL;
	chip->dt.esr_timer_chg_slow[TIMER_MAX] = -EINVAL;
	rc = fg_parse_dt_property_u32_array(node, "qcom,fg-esr-timer-chg-slow",
		chip->dt.esr_timer_chg_slow, NUM_ESR_TIMERS);
	if (rc < 0)
		return rc;

	chip->dt.esr_timer_dischg_slow[TIMER_RETRY] = -EINVAL;
	chip->dt.esr_timer_dischg_slow[TIMER_MAX] = -EINVAL;
	rc = fg_parse_dt_property_u32_array(node,
		"qcom,fg-esr-timer-dischg-slow", chip->dt.esr_timer_dischg_slow,
		NUM_ESR_TIMERS);
	if (rc < 0)
		return rc;

	chip->dt.force_load_profile = of_property_read_bool(node,
					"qcom,fg-force-load-profile");

	rc = of_property_read_u32(node, "qcom,cl-start-capacity", &temp);
	if (rc < 0)
		chip->cl->dt.max_start_soc = DEFAULT_CL_START_SOC;
	else
		chip->cl->dt.max_start_soc = temp;

	chip->cl->dt.min_delta_batt_soc = DEFAULT_CL_DELTA_BATT_SOC;
	/* read from DT property and update, if value exists */
	of_property_read_u32(node, "qcom,cl-min-delta-batt-soc",
					&chip->cl->dt.min_delta_batt_soc);

	chip->cl->dt.cl_wt_enable = of_property_read_bool(node,
						"qcom,cl-wt-enable");

	rc = of_property_read_u32(node, "qcom,cl-min-temp", &temp);
	if (rc < 0)
		chip->cl->dt.min_temp = DEFAULT_CL_MIN_TEMP_DECIDEGC;
	else
		chip->cl->dt.min_temp = temp;

	rc = of_property_read_u32(node, "qcom,cl-max-temp", &temp);
	if (rc < 0)
		chip->cl->dt.max_temp = DEFAULT_CL_MAX_TEMP_DECIDEGC;
	else
		chip->cl->dt.max_temp = temp;

	rc = of_property_read_u32(node, "qcom,cl-max-increment", &temp);
	if (rc < 0)
		chip->cl->dt.max_cap_inc = DEFAULT_CL_MAX_INC_DECIPERC;
	else
		chip->cl->dt.max_cap_inc = temp;

	rc = of_property_read_u32(node, "qcom,cl-max-decrement", &temp);
	if (rc < 0)
		chip->cl->dt.max_cap_dec = DEFAULT_CL_MAX_DEC_DECIPERC;
	else
		chip->cl->dt.max_cap_dec = temp;

	rc = of_property_read_u32(node, "qcom,cl-min-limit", &temp);
	if (rc < 0)
		chip->cl->dt.min_cap_limit = DEFAULT_CL_MIN_LIM_DECIPERC;
	else
		chip->cl->dt.min_cap_limit = temp;

	rc = of_property_read_u32(node, "qcom,cl-max-limit", &temp);
	if (rc < 0)
		chip->cl->dt.max_cap_limit = DEFAULT_CL_MAX_LIM_DECIPERC;
	else
		chip->cl->dt.max_cap_limit = temp;

	of_property_read_u32(node, "qcom,cl-skew", &chip->cl->dt.skew_decipct);

	rc = of_property_read_u32(node, "qcom,fg-batt-temp-hot", &temp);
	if (rc < 0)
		chip->dt.batt_temp_hot_thresh = -EINVAL;
	else
		chip->dt.batt_temp_hot_thresh = temp;

	rc = of_property_read_u32(node, "qcom,fg-batt-temp-cold", &temp);
	if (rc < 0)
		chip->dt.batt_temp_cold_thresh = -EINVAL;
	else
		chip->dt.batt_temp_cold_thresh = temp;

	rc = of_property_read_u32(node, "qcom,fg-batt-temp-hyst", &temp);
	if (rc < 0)
		chip->dt.batt_temp_hyst = -EINVAL;
	else if (temp >= BTEMP_DELTA_LOW && temp <= BTEMP_DELTA_HIGH)
		chip->dt.batt_temp_hyst = temp;

	rc = of_property_read_u32(node, "qcom,fg-batt-temp-delta", &temp);
	if (rc < 0)
		chip->dt.batt_temp_delta = -EINVAL;
	else if (temp >= BTEMP_DELTA_LOW && temp <= BTEMP_DELTA_HIGH)
		chip->dt.batt_temp_delta = temp;

	chip->dt.batt_therm_freq = 8;
	rc = of_property_read_u32(node, "qcom,fg-batt-therm-freq", &temp);
	if (temp > 0 && temp <= 255)
		chip->dt.batt_therm_freq = temp;

	chip->dt.hold_soc_while_full = of_property_read_bool(node,
					"qcom,hold-soc-while-full");

	chip->dt.linearize_soc = of_property_read_bool(node,
					"qcom,linearize-soc");

	rc = fg_parse_ki_coefficients(fg);
	if (rc < 0)
		pr_err("Error in parsing Ki coefficients, rc=%d\n", rc);

	rc = of_property_read_u32(node, "qcom,fg-rconn-uohms", &temp);
	if (!rc)
		chip->dt.rconn_uohms = temp;

	rc = fg_parse_slope_limit_coefficients(fg);
	if (rc < 0)
		pr_err("Error in parsing slope limit coeffs, rc=%d\n", rc);

	chip->dt.esr_pulse_thresh_ma = DEFAULT_ESR_PULSE_THRESH_MA;
	rc = of_property_read_u32(node, "qcom,fg-esr-pulse-thresh-ma", &temp);
	if (!rc) {
		if (temp > 0 && temp < 1000)
			chip->dt.esr_pulse_thresh_ma = temp;
	}

	chip->dt.esr_meas_curr_ma = DEFAULT_ESR_MEAS_CURR_MA;
	rc = of_property_read_u32(node, "qcom,fg-esr-meas-curr-ma", &temp);
	if (!rc) {
		/* ESR measurement current range is 60-240 mA */
		if (temp >= 60 || temp <= 240)
			chip->dt.esr_meas_curr_ma = temp;
	}

	rc = fg_parse_esr_cal_params(fg);
	if (rc < 0)
		return rc;

	chip->dt.rapid_soc_dec_en = of_property_read_bool(node,
					"qcom,rapid-soc-dec-en");

	chip->dt.five_pin_battery = of_property_read_bool(node,
					"qcom,five-pin-battery");
	chip->dt.multi_profile_load = of_property_read_bool(node,
					"qcom,multi-profile-load");
	return 0;
}

static void fg_gen4_cleanup(struct fg_gen4_chip *chip)
{
	struct fg_dev *fg = &chip->fg;

	fg_unregister_interrupts(fg, chip, FG_GEN4_IRQ_MAX);

	cancel_work(&fg->status_change_work);
	cancel_delayed_work_sync(&fg->profile_load_work);
	cancel_delayed_work_sync(&fg->sram_dump_work);
	cancel_work_sync(&chip->pl_current_en_work);

	power_supply_unreg_notifier(&fg->nb);
	debugfs_remove_recursive(fg->dfs_root);

	if (fg->awake_votable)
		destroy_votable(fg->awake_votable);

	if (fg->delta_bsoc_irq_en_votable)
		destroy_votable(fg->delta_bsoc_irq_en_votable);

	if (chip->delta_esr_irq_en_votable)
		destroy_votable(chip->delta_esr_irq_en_votable);

	if (chip->parallel_current_en_votable)
		destroy_votable(chip->parallel_current_en_votable);

	if (chip->mem_attn_irq_en_votable)
		destroy_votable(chip->mem_attn_irq_en_votable);

	dev_set_drvdata(fg->dev, NULL);
}

static int fg_gen4_probe(struct platform_device *pdev)
{
	struct fg_gen4_chip *chip;
	struct fg_dev *fg;
	struct power_supply_config fg_psy_cfg;
	int rc, msoc, volt_uv, batt_temp;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	fg = &chip->fg;
	fg->dev = &pdev->dev;
	fg->debug_mask = &fg_gen4_debug_mask;
	fg->irqs = fg_irqs;
	fg->charge_status = -EINVAL;
	fg->prev_charge_status = -EINVAL;
	fg->online_status = -EINVAL;
	fg->batt_id_ohms = -EINVAL;
	chip->ki_coeff_full_soc[0] = -EINVAL;
	chip->ki_coeff_full_soc[1] = -EINVAL;
	chip->esr_soh_cycle_count = -EINVAL;
	fg->regmap = dev_get_regmap(fg->dev->parent, NULL);
	if (!fg->regmap) {
		dev_err(fg->dev, "Parent regmap is unavailable\n");
		return -ENXIO;
	}

	mutex_init(&fg->bus_lock);
	mutex_init(&fg->sram_rw_lock);
	mutex_init(&fg->charge_full_lock);
	init_completion(&fg->soc_update);
	init_completion(&fg->soc_ready);
	init_completion(&chip->mem_attn);
	INIT_WORK(&fg->status_change_work, status_change_work);
	INIT_WORK(&chip->esr_calib_work, esr_calib_work);
	INIT_DELAYED_WORK(&fg->profile_load_work, profile_load_work);
	INIT_DELAYED_WORK(&fg->sram_dump_work, sram_dump_work);
	INIT_DELAYED_WORK(&chip->pl_enable_work, pl_enable_work);
	INIT_WORK(&chip->pl_current_en_work, pl_current_en_work);

	fg->awake_votable = create_votable("FG_WS", VOTE_SET_ANY,
					fg_awake_cb, fg);
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

	chip->delta_esr_irq_en_votable = create_votable("FG_DELTA_ESR_IRQ",
						VOTE_SET_ANY,
						fg_gen4_delta_esr_irq_en_cb,
						chip);
	if (IS_ERR(chip->delta_esr_irq_en_votable)) {
		rc = PTR_ERR(chip->delta_esr_irq_en_votable);
		chip->delta_esr_irq_en_votable = NULL;
		goto exit;
	}

	chip->mem_attn_irq_en_votable = create_votable("FG_MEM_ATTN_IRQ",
						VOTE_SET_ANY,
						fg_gen4_mem_attn_irq_en_cb, fg);
	if (IS_ERR(chip->mem_attn_irq_en_votable)) {
		rc = PTR_ERR(chip->mem_attn_irq_en_votable);
		chip->mem_attn_irq_en_votable = NULL;
		goto exit;
	}

	chip->parallel_current_en_votable = create_votable("FG_SMB_MEAS_EN",
						VOTE_SET_ANY,
						fg_parallel_current_en_cb, fg);
	if (IS_ERR(chip->parallel_current_en_votable)) {
		rc = PTR_ERR(chip->parallel_current_en_votable);
		chip->parallel_current_en_votable = NULL;
		goto exit;
	}

	rc = fg_alg_init(chip);
	if (rc < 0) {
		dev_err(fg->dev, "Error in alg_init, rc:%d\n",
			rc);
		goto exit;
	}

	rc = fg_gen4_parse_dt(chip);
	if (rc < 0) {
		dev_err(fg->dev, "Error in reading DT parameters, rc:%d\n",
			rc);
		goto exit;
	}

	if (chip->esr_fast_calib) {
		if (alarmtimer_get_rtcdev()) {
			alarm_init(&chip->esr_fast_cal_timer, ALARM_BOOTTIME,
				fg_esr_fast_cal_timer);
		} else {
			dev_err(fg->dev, "Failed to initialize esr_fast_cal timer\n");
			rc = -EPROBE_DEFER;
			goto exit;
		}
	}

	rc = fg_memif_init(fg);
	if (rc < 0) {
		dev_err(fg->dev, "Error in initializing FG_MEMIF, rc:%d\n",
			rc);
		goto exit;
	}

	platform_set_drvdata(pdev, chip);
	rc = fg_gen4_hw_init(chip);
	if (rc < 0) {
		dev_err(fg->dev, "Error in initializing FG hardware, rc:%d\n",
			rc);
		goto exit;
	}

	/* Register the power supply */
	fg_psy_cfg.drv_data = fg;
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

	rc = fg_register_interrupts(&chip->fg, FG_GEN4_IRQ_MAX);
	if (rc < 0) {
		dev_err(fg->dev, "Error in registering interrupts, rc:%d\n",
			rc);
		goto exit;
	}

	if (fg->irqs[MEM_ATTN_IRQ].irq)
		irq_set_status_flags(fg->irqs[MEM_ATTN_IRQ].irq,
					IRQ_DISABLE_UNLAZY);

	/* Keep SOC_UPDATE irq disabled until we require it */
	if (fg->irqs[SOC_UPDATE_IRQ].irq)
		disable_irq_nosync(fg->irqs[SOC_UPDATE_IRQ].irq);

	/* Keep BSOC_DELTA_IRQ disabled until we require it */
	vote(fg->delta_bsoc_irq_en_votable, DELTA_BSOC_IRQ_VOTER, false, 0);

	/* Keep MEM_ATTN_IRQ disabled until we require it */
	vote(chip->mem_attn_irq_en_votable, MEM_ATTN_IRQ_VOTER, false, 0);

	rc = fg_debugfs_create(fg);
	if (rc < 0) {
		dev_err(fg->dev, "Error in creating debugfs entries, rc:%d\n",
			rc);
		goto exit;
	}

	rc = fg_get_battery_voltage(fg, &volt_uv);
	if (!rc)
		rc = fg_get_msoc(fg, &msoc);

	if (!rc)
		rc = fg_gen4_get_battery_temp(fg, &batt_temp);

	if (!rc)
		rc = fg_gen4_get_batt_id(chip);

	if (!rc) {
		fg->last_batt_temp = batt_temp;
		pr_info("battery SOC:%d voltage: %duV temp: %d id: %d ohms\n",
			msoc, volt_uv, batt_temp, fg->batt_id_ohms);
	}

	fg->tz_dev = thermal_zone_of_sensor_register(fg->dev, 0, fg,
							&fg_gen4_tz_ops);
	if (IS_ERR_OR_NULL(fg->tz_dev)) {
		rc = PTR_ERR(fg->tz_dev);
		fg->tz_dev = NULL;
		dev_dbg(fg->dev, "Couldn't register with thermal framework rc:%d\n",
			rc);
	}

	device_init_wakeup(fg->dev, true);
	if (!fg->battery_missing)
		schedule_delayed_work(&fg->profile_load_work, 0);

	pr_debug("FG GEN4 driver probed successfully\n");
	return 0;
exit:
	fg_gen4_cleanup(chip);
	return rc;
}

static int fg_gen4_remove(struct platform_device *pdev)
{
	struct fg_gen4_chip *chip = dev_get_drvdata(&pdev->dev);

	fg_gen4_cleanup(chip);
	return 0;
}

static void fg_gen4_shutdown(struct platform_device *pdev)
{
	struct fg_gen4_chip *chip = dev_get_drvdata(&pdev->dev);
	struct fg_dev *fg = &chip->fg;
	int rc, bsoc;

	fg_unregister_interrupts(fg, chip, FG_GEN4_IRQ_MAX);

	if (chip->rapid_soc_dec_en) {
		rc = fg_gen4_rapid_soc_config(chip, false);
		if (rc < 0)
			pr_err("Error in reverting rapid SOC decrease config rc:%d\n",
				rc);
	}

	rc = fg_get_sram_prop(fg, FG_SRAM_BATT_SOC, &bsoc);
	if (rc < 0) {
		pr_err("Error in getting BATT_SOC, rc=%d\n", rc);
		return;
	}

	if (fg->charge_full) {
		/* We need 2 most significant bytes here */
		bsoc = (u32)bsoc >> 16;

		rc = fg_gen4_configure_full_soc(fg, bsoc);
		if (rc < 0) {
			pr_err("Error in configuring full_soc, rc=%d\n", rc);
			return;
		}
	}

	/*
	 * Charging status doesn't matter when the device shuts down and we
	 * have to treat this as charge done. Hence pass charge_done as true.
	 */
	cycle_count_update(chip->counter, (u32)bsoc >> 24,
		POWER_SUPPLY_STATUS_NOT_CHARGING, true, is_input_present(fg));
}

static int fg_gen4_suspend(struct device *dev)
{
	struct fg_gen4_chip *chip = dev_get_drvdata(dev);
	struct fg_dev *fg = &chip->fg;

	cancel_delayed_work_sync(&chip->ttf->ttf_work);
	if (fg_sram_dump)
		cancel_delayed_work_sync(&fg->sram_dump_work);
	return 0;
}

static int fg_gen4_resume(struct device *dev)
{
	struct fg_gen4_chip *chip = dev_get_drvdata(dev);
	struct fg_dev *fg = &chip->fg;

	schedule_delayed_work(&chip->ttf->ttf_work, 0);
	if (fg_sram_dump)
		schedule_delayed_work(&fg->sram_dump_work,
				msecs_to_jiffies(fg_sram_dump_period_ms));
	return 0;
}

static const struct dev_pm_ops fg_gen4_pm_ops = {
	.suspend	= fg_gen4_suspend,
	.resume		= fg_gen4_resume,
};

static const struct of_device_id fg_gen4_match_table[] = {
	{.compatible = FG_GEN4_DEV_NAME},
	{},
};

static struct platform_driver fg_gen4_driver = {
	.driver = {
		.name = FG_GEN4_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = fg_gen4_match_table,
		.pm		= &fg_gen4_pm_ops,
	},
	.probe		= fg_gen4_probe,
	.remove		= fg_gen4_remove,
	.shutdown	= fg_gen4_shutdown,
};

module_platform_driver(fg_gen4_driver);

MODULE_DESCRIPTION("QPNP Fuel gauge GEN4 driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" FG_GEN4_DEV_NAME);
