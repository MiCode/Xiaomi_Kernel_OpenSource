/* Copyright (c) 2014-2015, 2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"haptic: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/of_device.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/qpnp/pwm.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/log2.h>
#include <linux/qpnp-misc.h>
#include <linux/qpnp/qpnp-haptic.h>
#include <linux/qpnp/qpnp-revid.h>
#include "../../staging/android/timed_output.h"

#define QPNP_HAP_STATUS(b)		(b + 0x0A)
#define QPNP_HAP_LRA_AUTO_RES_LO(b)	(b + 0x0B)
#define QPNP_HAP_LRA_AUTO_RES_HI(b)     (b + 0x0C)
#define QPNP_HAP_EN_CTL_REG(b)		(b + 0x46)
#define QPNP_HAP_EN_CTL2_REG(b)		(b + 0x48)
#define QPNP_HAP_AUTO_RES_CTRL(b)	(b + 0x4B)
#define QPNP_HAP_CFG1_REG(b)		(b + 0x4C)
#define QPNP_HAP_CFG2_REG(b)		(b + 0x4D)
#define QPNP_HAP_SEL_REG(b)		(b + 0x4E)
#define QPNP_HAP_LRA_AUTO_RES_REG(b)	(b + 0x4F)
#define QPNP_HAP_VMAX_REG(b)		(b + 0x51)
#define QPNP_HAP_ILIM_REG(b)		(b + 0x52)
#define QPNP_HAP_SC_DEB_REG(b)		(b + 0x53)
#define QPNP_HAP_RATE_CFG1_REG(b)	(b + 0x54)
#define QPNP_HAP_RATE_CFG2_REG(b)	(b + 0x55)
#define QPNP_HAP_INT_PWM_REG(b)		(b + 0x56)
#define QPNP_HAP_EXT_PWM_REG(b)		(b + 0x57)
#define QPNP_HAP_PWM_CAP_REG(b)		(b + 0x58)
#define QPNP_HAP_SC_CLR_REG(b)		(b + 0x59)
#define QPNP_HAP_SC_IRQ_STATUS_DELAY   msecs_to_jiffies(1000)
#define QPNP_HAP_BRAKE_REG(b)		(b + 0x5C)
#define QPNP_HAP_WAV_REP_REG(b)		(b + 0x5E)
#define QPNP_HAP_WAV_S_REG_BASE(b)	(b + 0x60)
#define QPNP_HAP_PLAY_REG(b)		(b + 0x70)
#define QPNP_HAP_SEC_ACCESS_REG(b)	(b + 0xD0)
#define QPNP_HAP_TEST2_REG(b)		(b + 0xE3)

#define QPNP_HAP_STATUS_BUSY		0x02
#define QPNP_HAP_ACT_TYPE_MASK		BIT(0)
#define QPNP_HAP_LRA			0x0
#define QPNP_HAP_ERM			0x1
#define QPNP_HAP_PM660_HW_AUTO_RES_MODE_BIT	BIT(3)
#define QPNP_HAP_AUTO_RES_MODE_MASK	GENMASK(6, 4)
#define QPNP_HAP_AUTO_RES_MODE_SHIFT	4
#define QPNP_HAP_PM660_AUTO_RES_MODE_BIT	BIT(7)
#define QPNP_HAP_PM660_AUTO_RES_MODE_SHIFT	7
#define QPNP_HAP_PM660_CALIBRATE_DURATION_MASK	GENMASK(6, 5)
#define QPNP_HAP_PM660_CALIBRATE_DURATION_SHIFT	5
#define QPNP_HAP_PM660_QWD_DRIVE_DURATION_BIT	BIT(4)
#define QPNP_HAP_PM660_QWD_DRIVE_DURATION_SHIFT	4
#define QPNP_HAP_PM660_CALIBRATE_AT_EOP_BIT	BIT(3)
#define QPNP_HAP_PM660_CALIBRATE_AT_EOP_SHIFT	3
#define QPNP_HAP_PM660_LRA_ZXD_CAL_PERIOD_BIT	GENMASK(2, 0)
#define QPNP_HAP_LRA_HIGH_Z_MASK		GENMASK(3, 2)
#define QPNP_HAP_LRA_HIGH_Z_SHIFT		2
#define QPNP_HAP_LRA_RES_CAL_PER_MASK		GENMASK(1, 0)
#define QPNP_HAP_PM660_LRA_RES_CAL_PER_MASK	GENMASK(2, 0)
#define QPNP_HAP_RES_CAL_PERIOD_MIN		4
#define QPNP_HAP_RES_CAL_PERIOD_MAX		32
#define QPNP_HAP_PM660_RES_CAL_PERIOD_MAX	256
#define QPNP_HAP_WF_SOURCE_MASK		GENMASK(5, 4)
#define QPNP_HAP_WF_SOURCE_SHIFT	4
#define QPNP_HAP_VMAX_OVD_BIT		BIT(6)
#define QPNP_HAP_VMAX_MASK		GENMASK(5, 1)
#define QPNP_HAP_VMAX_SHIFT		1
#define QPNP_HAP_VMAX_MIN_MV		116
#define QPNP_HAP_VMAX_MAX_MV		3596
#define QPNP_HAP_ILIM_MASK		BIT(0)
#define QPNP_HAP_ILIM_MIN_MV		400
#define QPNP_HAP_ILIM_MAX_MV		800
#define QPNP_HAP_SC_DEB_MASK		GENMASK(2, 0)
#define QPNP_HAP_SC_DEB_CYCLES_MIN	0
#define QPNP_HAP_DEF_SC_DEB_CYCLES	8
#define QPNP_HAP_SC_DEB_CYCLES_MAX	32
#define QPNP_HAP_SC_CLR			1
#define QPNP_HAP_INT_PWM_MASK		GENMASK(1, 0)
#define QPNP_HAP_INT_PWM_FREQ_253_KHZ	253
#define QPNP_HAP_INT_PWM_FREQ_505_KHZ	505
#define QPNP_HAP_INT_PWM_FREQ_739_KHZ	739
#define QPNP_HAP_INT_PWM_FREQ_1076_KHZ	1076
#define QPNP_HAP_WAV_SHAPE_MASK		BIT(0)
#define QPNP_HAP_RATE_CFG1_MASK		0xFF
#define QPNP_HAP_RATE_CFG2_MASK		0xF0
#define QPNP_HAP_RATE_CFG2_SHFT		8
#define QPNP_HAP_RATE_CFG_STEP_US	5
#define QPNP_HAP_WAV_PLAY_RATE_US_MIN	0
#define QPNP_HAP_DEF_WAVE_PLAY_RATE_US	5715
#define QPNP_HAP_WAV_PLAY_RATE_US_MAX	20475
#define QPNP_HAP_WAV_REP_MASK		GENMASK(6, 4)
#define QPNP_HAP_WAV_S_REP_MASK		GENMASK(1, 0)
#define QPNP_HAP_WAV_REP_SHIFT		4
#define QPNP_HAP_WAV_REP_MIN		1
#define QPNP_HAP_WAV_REP_MAX		128
#define QPNP_HAP_WAV_S_REP_MIN		1
#define QPNP_HAP_WAV_S_REP_MAX		8
#define QPNP_HAP_WF_AMP_MASK		GENMASK(5, 1)
#define QPNP_HAP_WF_OVD_BIT		BIT(6)
#define QPNP_HAP_BRAKE_PAT_MASK		0x3
#define QPNP_HAP_ILIM_MIN_MA		400
#define QPNP_HAP_ILIM_MAX_MA		800
#define QPNP_HAP_EXT_PWM_MASK		GENMASK(1, 0)
#define QPNP_HAP_EXT_PWM_FREQ_25_KHZ	25
#define QPNP_HAP_EXT_PWM_FREQ_50_KHZ	50
#define QPNP_HAP_EXT_PWM_FREQ_75_KHZ	75
#define QPNP_HAP_EXT_PWM_FREQ_100_KHZ	100
#define PWM_MAX_DTEST_LINES		4
#define QPNP_HAP_EXT_PWM_DTEST_MASK	GENMASK(6, 4)
#define QPNP_HAP_EXT_PWM_DTEST_SHFT	4
#define QPNP_HAP_EXT_PWM_PEAK_DATA	0x7F
#define QPNP_HAP_EXT_PWM_HALF_DUTY	50
#define QPNP_HAP_EXT_PWM_FULL_DUTY	100
#define QPNP_HAP_EXT_PWM_DATA_FACTOR	39
#define QPNP_HAP_WAV_SINE		0
#define QPNP_HAP_WAV_SQUARE		1
#define QPNP_HAP_WAV_SAMP_LEN		8
#define QPNP_HAP_WAV_SAMP_MAX		0x3E
#define QPNP_HAP_BRAKE_PAT_LEN		4
#define QPNP_HAP_PLAY_EN_BIT		BIT(7)
#define QPNP_HAP_EN_BIT			BIT(7)
#define QPNP_HAP_BRAKE_MASK		BIT(0)
#define QPNP_HAP_AUTO_RES_MASK		BIT(7)
#define AUTO_RES_ENABLE			BIT(7)
#define AUTO_RES_ERR_BIT		0x10
#define SC_FOUND_BIT			0x08
#define SC_MAX_COUNT			5

#define QPNP_HAP_TIMEOUT_MS_MAX		15000
#define QPNP_HAP_STR_SIZE		20
#define QPNP_HAP_MAX_RETRIES		5
#define QPNP_TEST_TIMER_MS		5

#define QPNP_HAP_TIME_REQ_FOR_BACK_EMF_GEN	20000
#define POLL_TIME_AUTO_RES_ERR_NS	(20 * NSEC_PER_MSEC)

#define MAX_POSITIVE_VARIATION_LRA_FREQ 30
#define MAX_NEGATIVE_VARIATION_LRA_FREQ -30
#define FREQ_VARIATION_STEP		5
#define AUTO_RES_ERROR_CAPTURE_RES	5
#define AUTO_RES_ERROR_MAX		30
#define ADJUSTED_LRA_PLAY_RATE_CODE_ARRSIZE \
	((MAX_POSITIVE_VARIATION_LRA_FREQ - MAX_NEGATIVE_VARIATION_LRA_FREQ) \
	 / FREQ_VARIATION_STEP)
#define LRA_DRIVE_PERIOD_POS_ERR(hap, rc_clk_err_percent) \
	(hap->init_drive_period_code = (hap->init_drive_period_code * \
		(1000 + rc_clk_err_percent_x10)) / 1000)
#define LRA_DRIVE_PERIOD_NEG_ERR(hap, rc_clk_err_percent) \
	(hap->init_drive_period_code = (hap->init_drive_period_code * \
		(1000 - rc_clk_err_percent_x10)) / 1000)

u32 adjusted_lra_play_rate_code[ADJUSTED_LRA_PLAY_RATE_CODE_ARRSIZE];

/* haptic debug register set */
static u8 qpnp_hap_dbg_regs[] = {
	0x0a, 0x0b, 0x0c, 0x46, 0x48, 0x4c, 0x4d, 0x4e, 0x4f, 0x51, 0x52, 0x53,
	0x54, 0x55, 0x56, 0x57, 0x58, 0x5c, 0x5e, 0x60, 0x61, 0x62, 0x63, 0x64,
	0x65, 0x66, 0x67, 0x70, 0xE3,
};

/* ramp up/down test sequence */
static u8 qpnp_hap_ramp_test_data[] = {
	0x0, 0x19, 0x32, 0x4C, 0x65, 0x7F, 0x65, 0x4C, 0x32, 0x19,
	0x0, 0x99, 0xB2, 0xCC, 0xE5, 0xFF, 0xE5, 0xCC, 0xB2, 0x99,
	0x0, 0x19, 0x32, 0x4C, 0x65, 0x7F, 0x65, 0x4C, 0x32, 0x19,
	0x0, 0x99, 0xB2, 0xCC, 0xE5, 0xFF, 0xE5, 0xCC, 0xB2, 0x99,
	0x0, 0x19, 0x32, 0x4C, 0x65, 0x7F, 0x65, 0x4C, 0x32, 0x19,
	0x0, 0x99, 0xB2, 0xCC, 0xE5, 0xFF, 0xE5, 0xCC, 0xB2, 0x99,
	0x0, 0x19, 0x32, 0x4C, 0x65, 0x7F, 0x65, 0x4C, 0x32, 0x19,
	0x0, 0x99, 0xB2, 0xCC, 0xE5, 0xFF, 0xE5, 0xCC, 0xB2, 0x99,
	0x0, 0x19, 0x32, 0x4C, 0x65, 0x7F, 0x65, 0x4C, 0x32, 0x19,
	0x0, 0x99, 0xB2, 0xCC, 0xE5, 0xFF, 0xE5, 0xCC, 0xB2, 0x99,
	0x0, 0x19, 0x32, 0x4C, 0x65, 0x7F, 0x65, 0x4C, 0x32, 0x19,
	0x0, 0x99, 0xB2, 0xCC, 0xE5, 0xFF, 0xE5, 0xCC, 0xB2, 0x99,
	0x0, 0x19, 0x32, 0x4C, 0x65, 0x7F, 0x65, 0x4C, 0x32, 0x19,
	0x0, 0x99, 0xB2, 0xCC, 0xE5, 0xFF, 0xE5, 0xCC, 0xB2, 0x99,
};

/* alternate max and min sequence */
static u8 qpnp_hap_min_max_test_data[] = {
	0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF,
	0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF,
	0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF,
	0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF,
	0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF,
	0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF,
	0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF,
	0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF,
	0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF,
	0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF,
	0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF,
	0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF, 0x0, 0x7F, 0x0, 0xFF,
};

/*
 * auto resonance mode
 * ZXD - Zero Cross Detect
 * QWD - Quarter Wave Drive
 * ZXD_EOP - ZXD with End Of Pattern
 */
enum qpnp_hap_auto_res_mode {
	QPNP_HAP_AUTO_RES_NONE,
	QPNP_HAP_AUTO_RES_ZXD,
	QPNP_HAP_AUTO_RES_QWD,
	QPNP_HAP_AUTO_RES_MAX_QWD,
	QPNP_HAP_AUTO_RES_ZXD_EOP,
};

enum qpnp_hap_pm660_auto_res_mode {
	QPNP_HAP_PM660_AUTO_RES_ZXD,
	QPNP_HAP_PM660_AUTO_RES_QWD,
};

/* high Z option lines */
enum qpnp_hap_high_z {
	QPNP_HAP_LRA_HIGH_Z_NONE, /* opt0 for PM660 */
	QPNP_HAP_LRA_HIGH_Z_OPT1,
	QPNP_HAP_LRA_HIGH_Z_OPT2,
	QPNP_HAP_LRA_HIGH_Z_OPT3,
};

/* play modes */
enum qpnp_hap_mode {
	QPNP_HAP_DIRECT,
	QPNP_HAP_BUFFER,
	QPNP_HAP_AUDIO,
	QPNP_HAP_PWM,
};

/* status flags */
enum qpnp_hap_status {
	AUTO_RESONANCE_ENABLED = BIT(0),
};

/* pwm channel info */
struct qpnp_pwm_info {
	struct pwm_device *pwm_dev;
	u32 pwm_channel;
	u32 duty_us;
	u32 period_us;
};

/*
 *  qpnp_hap_lra_ares_cfg - Haptic auto_resonance configuration
 *  @ lra_qwd_drive_duration - LRA QWD drive duration
 *  @ calibrate_at_eop - Calibrate at EOP
 *  @ lra_res_cal_period - LRA resonance calibration period
 *  @ auto_res_mode - auto resonace mode
 *  @ lra_high_z - high z option line
 */
struct qpnp_hap_lra_ares_cfg {
	int				lra_qwd_drive_duration;
	int				calibrate_at_eop;
	enum qpnp_hap_high_z		lra_high_z;
	u16				lra_res_cal_period;
	u8				auto_res_mode;
};

/*
 *  qpnp_hap - Haptic data structure
 *  @ spmi - spmi device
 *  @ hap_timer - hrtimer
 *  @ auto_res_err_poll_timer - hrtimer for auto-resonance error
 *  @ timed_dev - timed output device
 *  @ work - worker
 *  @ sc_work - worker to handle short circuit condition
 *  @ pwm_info - pwm info
 *  @ ares_cfg - auto resonance configuration
 *  @ lock - mutex lock
 *  @ wf_lock - mutex lock for waveform
 *  @ init_drive_period_code - the initial lra drive period code
 *  @ drive_period_code_max_limit_percent_variation - maximum limit of
      percentage variation of drive period code
 *  @ drive_period_code_min_limit_percent_variation - minimum limit og
      percentage variation of drive period code
 *  @ drive_period_code_max_limit - calculated drive period code with
      percentage variation on the higher side.
 *  @ drive_period_code_min_limit - calculated drive period code with
      percentage variation on the lower side
 *  @ play_mode - play mode
 *  @ timeout_ms - max timeout in ms
 *  @ time_required_to_generate_back_emf_us - the time required for sufficient
      back-emf to be generated for auto resonance to be successful
 *  @ vmax_mv - max voltage in mv
 *  @ ilim_ma - limiting current in ma
 *  @ sc_deb_cycles - short circuit debounce cycles
 *  @ int_pwm_freq_khz - internal pwm frequency in khz
 *  @ wave_play_rate_us - play rate for waveform
 *  @ play_time_ms - play time set by the user
 *  @ ext_pwm_freq_khz - external pwm frequency in khz
 *  @ wave_rep_cnt - waveform repeat count
 *  @ wave_s_rep_cnt - waveform sample repeat count
 *  @ play_irq - irq for play
 *  @ sc_irq - irq for short circuit
 *  @ status_flags - status
 *  @ base - base address
 *  @ act_type - actuator type
 *  @ wave_shape - waveform shape
 *  @ wave_samp - array of wave samples
 *  @ shadow_wave_samp - shadow array of wave samples
 *  @ brake_pat - pattern for active breaking
 *  @ sc_count - counter to determine the duration of short circuit condition
 *  @ lra_hw_auto_resonance - enable hardware auto resonance
 *  @ state - current state of haptics
 *  @ module_en - haptics module enable status
 *  @ wf_update - waveform update flag
 *  @ pwm_cfg_state - pwm mode configuration state
 *  @ en_brake - brake state
 *  @ sup_brake_pat - support custom brake pattern
 *  @ correct_lra_drive_freq - correct LRA Drive Frequency
 *  @ misc_clk_trim_error_reg - MISC clock trim error register if present
 *  @ clk_trim_error_code - MISC clock trim error code
 *  @ perform_lra_auto_resonance_search - whether lra auto resonance search
 *    algorithm should be performed or not.
 *  @ auto_mode - Auto mode selection
 *  @ override_auto_mode_config - Flag to override auto mode configuration with
 *    user specified values through sysfs.
 */
struct qpnp_hap {
	struct platform_device		*pdev;
	struct regmap			*regmap;
	struct regulator		*vcc_pon;
	struct hrtimer			hap_timer;
	struct hrtimer			auto_res_err_poll_timer;
	struct timed_output_dev		timed_dev;
	struct work_struct		work;
	struct delayed_work		sc_work;
	struct hrtimer			hap_test_timer;
	struct work_struct		test_work;
	struct qpnp_pwm_info		pwm_info;
	struct qpnp_hap_lra_ares_cfg	ares_cfg;
	struct mutex			lock;
	struct mutex			wf_lock;
	spinlock_t			bus_lock;
	struct completion		completion;
	enum qpnp_hap_mode		play_mode;
	u32				misc_clk_trim_error_reg;
	u32				init_drive_period_code;
	u32				timeout_ms;
	u32				time_required_to_generate_back_emf_us;
	u32				vmax_mv;
	u32				ilim_ma;
	u32				sc_deb_cycles;
	u32				int_pwm_freq_khz;
	u32				wave_play_rate_us;
	u32				play_time_ms;
	u32				ext_pwm_freq_khz;
	u32				wave_rep_cnt;
	u32				wave_s_rep_cnt;
	u32				play_irq;
	u32				sc_irq;
	u32				status_flags;
	u16				base;
	u16				last_rate_cfg;
	u16				drive_period_code_max_limit;
	u16				drive_period_code_min_limit;
	u8			drive_period_code_max_limit_percent_variation;
	u8			drive_period_code_min_limit_percent_variation;
	u8				act_type;
	u8				wave_shape;
	u8				wave_samp[QPNP_HAP_WAV_SAMP_LEN];
	u8				shadow_wave_samp[QPNP_HAP_WAV_SAMP_LEN];
	u8				brake_pat[QPNP_HAP_BRAKE_PAT_LEN];
	u8				sc_count;
	u8				ext_pwm_dtest_line;
	u8				pmic_subtype;
	u8				clk_trim_error_code;
	bool				lra_hw_auto_resonance;
	bool				vcc_pon_enabled;
	bool				state;
	bool				module_en;
	bool				manage_pon_supply;
	bool				wf_update;
	bool				pwm_cfg_state;
	bool				en_brake;
	bool				sup_brake_pat;
	bool				correct_lra_drive_freq;
	bool				perform_lra_auto_resonance_search;
	bool				auto_mode;
	bool				override_auto_mode_config;
	bool				play_irq_en;
};

static struct qpnp_hap *ghap;

/* helper to read a pmic register */
static int qpnp_hap_read_mult_reg(struct qpnp_hap *hap, u16 addr, u8 *val,
				int len)
{
	int rc;

	rc = regmap_bulk_read(hap->regmap, addr, val, len);
	if (rc < 0)
		pr_err("Error reading address: %X - ret %X\n", addr, rc);

	return rc;
}

static int qpnp_hap_read_reg(struct qpnp_hap *hap, u16 addr, u8 *val)
{
	int rc;
	uint tmp;

	rc = regmap_read(hap->regmap, addr, &tmp);
	if (rc < 0)
		pr_err("Error reading address: %X - ret %X\n", addr, rc);
	else
		*val = (u8)tmp;

	return rc;
}

/* helper to write a pmic register */
static int qpnp_hap_write_mult_reg(struct qpnp_hap *hap, u16 addr, u8 *val,
				int len)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&hap->bus_lock, flags);
	rc = regmap_bulk_write(hap->regmap, addr, val, len);
	if (rc < 0)
		pr_err("Error writing address: %X - ret %X\n", addr, rc);

	spin_unlock_irqrestore(&hap->bus_lock, flags);
	return rc;
}

static int qpnp_hap_write_reg(struct qpnp_hap *hap, u16 addr, u8 val)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&hap->bus_lock, flags);
	rc = regmap_write(hap->regmap, addr, val);
	if (rc < 0)
		pr_err("Error writing address: %X - ret %X\n", addr, rc);

	spin_unlock_irqrestore(&hap->bus_lock, flags);
	if (!rc)
		pr_debug("wrote: HAP_0x%x = 0x%x\n", addr, val);
	return rc;
}

/* helper to access secure registers */
#define QPNP_HAP_SEC_UNLOCK		0xA5
static int qpnp_hap_sec_masked_write_reg(struct qpnp_hap *hap, u16 addr,
					u8 mask, u8 val)
{
	unsigned long flags;
	int rc;
	u8 tmp = QPNP_HAP_SEC_UNLOCK;

	spin_lock_irqsave(&hap->bus_lock, flags);
	rc = regmap_write(hap->regmap, QPNP_HAP_SEC_ACCESS_REG(hap->base), tmp);
	if (rc < 0) {
		pr_err("Error writing sec_code - ret %X\n", rc);
		goto out;
	}

	rc = regmap_update_bits(hap->regmap, addr, mask, val);
	if (rc < 0)
		pr_err("Error writing address: %X - ret %X\n", addr, rc);

out:
	spin_unlock_irqrestore(&hap->bus_lock, flags);
	if (!rc)
		pr_debug("wrote: HAP_0x%x = 0x%x\n", addr, val);
	return rc;
}

static int qpnp_hap_masked_write_reg(struct qpnp_hap *hap, u16 addr, u8 mask,
					u8 val)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&hap->bus_lock, flags);
	rc = regmap_update_bits(hap->regmap, addr, mask, val);
	if (rc < 0)
		pr_err("Error writing address: %X - ret %X\n", addr, rc);

	spin_unlock_irqrestore(&hap->bus_lock, flags);
	if (!rc)
		pr_debug("wrote: HAP_0x%x = 0x%x\n", addr, val);
	return rc;
}

static void qpnp_handle_sc_irq(struct work_struct *work)
{
	struct qpnp_hap *hap = container_of(work,
				struct qpnp_hap, sc_work.work);
	u8 val;

	qpnp_hap_read_reg(hap, QPNP_HAP_STATUS(hap->base), &val);

	/* clear short circuit register */
	if (val & SC_FOUND_BIT) {
		hap->sc_count++;
		val = QPNP_HAP_SC_CLR;
		qpnp_hap_write_reg(hap, QPNP_HAP_SC_CLR_REG(hap->base), val);
	}
}

#define QPNP_HAP_CYCLES		4
static int qpnp_hap_mod_enable(struct qpnp_hap *hap, bool on)
{
	unsigned long wait_time_us;
	u8 val;
	int rc, i;

	if (hap->module_en == on)
		return 0;

	if (!on) {
		/*
		 * Wait for 4 cycles of play rate for play time is > 20 ms and
		 * wait for play time when play time is < 20 ms. This way, there
		 * will be an improvement in waiting time polling BUSY status.
		 */
		if (hap->play_time_ms <= 20)
			wait_time_us = hap->play_time_ms * 1000;
		else
			wait_time_us = QPNP_HAP_CYCLES * hap->wave_play_rate_us;

		for (i = 0; i < QPNP_HAP_MAX_RETRIES; i++) {
			rc = qpnp_hap_read_reg(hap, QPNP_HAP_STATUS(hap->base),
					&val);
			if (rc < 0)
				return rc;

			pr_debug("HAP_STATUS=0x%x\n", val);

			/* wait for play_rate cycles */
			if (val & QPNP_HAP_STATUS_BUSY) {
				usleep_range(wait_time_us, wait_time_us + 1);
				if (hap->play_mode == QPNP_HAP_DIRECT ||
					hap->play_mode == QPNP_HAP_PWM)
					break;
			} else {
				break;
			}
		}

		if (i >= QPNP_HAP_MAX_RETRIES)
			pr_debug("Haptics Busy. Force disable\n");
	}

	val = on ? QPNP_HAP_EN_BIT : 0;
	rc = qpnp_hap_write_reg(hap, QPNP_HAP_EN_CTL_REG(hap->base), val);
	if (rc < 0)
		return rc;

	hap->module_en = on;
	return 0;
}

static int qpnp_hap_play(struct qpnp_hap *hap, bool on)
{
	u8 val;
	int rc;

	val = on ? QPNP_HAP_PLAY_EN_BIT : 0;
	rc = qpnp_hap_write_reg(hap, QPNP_HAP_PLAY_REG(hap->base), val);
	return rc;
}

/* sysfs show debug registers */
static ssize_t qpnp_hap_dump_regs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);
	int count = 0, i;
	u8 val;

	for (i = 0; i < ARRAY_SIZE(qpnp_hap_dbg_regs); i++) {
		qpnp_hap_read_reg(hap, hap->base + qpnp_hap_dbg_regs[i], &val);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"qpnp_haptics: REG_0x%x = 0x%x\n",
				hap->base + qpnp_hap_dbg_regs[i],
				val);

		if (count >= PAGE_SIZE)
			return PAGE_SIZE - 1;
	}

	return count;
}

/* play irq handler */
static irqreturn_t qpnp_hap_play_irq(int irq, void *_hap)
{
	struct qpnp_hap *hap = _hap;
	int i, rc;
	u8 val;

	mutex_lock(&hap->wf_lock);

	/* Configure WAVE_SAMPLE1 to WAVE_SAMPLE8 register */
	for (i = 0; i < QPNP_HAP_WAV_SAMP_LEN && hap->wf_update; i++) {
		val = hap->wave_samp[i] = hap->shadow_wave_samp[i];
		rc = qpnp_hap_write_reg(hap,
			QPNP_HAP_WAV_S_REG_BASE(hap->base) + i, val);
		if (rc)
			goto unlock;
	}
	hap->wf_update = false;

unlock:
	mutex_unlock(&hap->wf_lock);

	return IRQ_HANDLED;
}

/* short circuit irq handler */
static irqreturn_t qpnp_hap_sc_irq(int irq, void *_hap)
{
	struct qpnp_hap *hap = _hap;
	int rc;
	u8 val;

	pr_debug("Short circuit detected\n");

	if (hap->sc_count < SC_MAX_COUNT) {
		qpnp_hap_read_reg(hap, QPNP_HAP_STATUS(hap->base), &val);
		if (val & SC_FOUND_BIT)
			schedule_delayed_work(&hap->sc_work,
					QPNP_HAP_SC_IRQ_STATUS_DELAY);
		else
			hap->sc_count = 0;
	} else {
		/* Disable haptics module if the duration of short circuit
		 * exceeds the maximum limit (5 secs).
		 */
		val = 0;
		rc = qpnp_hap_write_reg(hap, QPNP_HAP_EN_CTL_REG(hap->base),
			val);
		pr_err("Haptics disabled permanently due to short circuit\n");
	}

	return IRQ_HANDLED;
}

/* configuration api for buffer mode */
static int qpnp_hap_buffer_config(struct qpnp_hap *hap, u8 *wave_samp,
				bool overdrive)
{
	u8 buf[QPNP_HAP_WAV_SAMP_LEN], val;
	u8 *ptr;
	int rc, i;

	/* Configure the WAVE_REPEAT register */
	if (hap->wave_rep_cnt < QPNP_HAP_WAV_REP_MIN)
		hap->wave_rep_cnt = QPNP_HAP_WAV_REP_MIN;
	else if (hap->wave_rep_cnt > QPNP_HAP_WAV_REP_MAX)
		hap->wave_rep_cnt = QPNP_HAP_WAV_REP_MAX;

	if (hap->wave_s_rep_cnt < QPNP_HAP_WAV_S_REP_MIN)
		hap->wave_s_rep_cnt = QPNP_HAP_WAV_S_REP_MIN;
	else if (hap->wave_s_rep_cnt > QPNP_HAP_WAV_S_REP_MAX)
		hap->wave_s_rep_cnt = QPNP_HAP_WAV_S_REP_MAX;

	val = ilog2(hap->wave_rep_cnt) << QPNP_HAP_WAV_REP_SHIFT |
			ilog2(hap->wave_s_rep_cnt);
	rc = qpnp_hap_masked_write_reg(hap, QPNP_HAP_WAV_REP_REG(hap->base),
			QPNP_HAP_WAV_REP_MASK | QPNP_HAP_WAV_S_REP_MASK, val);
	if (rc)
		return rc;

	/* Don't set override bit in waveform sample for PM660 */
	if (hap->pmic_subtype == PM660_SUBTYPE)
		overdrive = false;

	if (wave_samp)
		ptr = wave_samp;
	else
		ptr = hap->wave_samp;

	/* Configure WAVE_SAMPLE1 to WAVE_SAMPLE8 register */
	for (i = 0; i < QPNP_HAP_WAV_SAMP_LEN; i++) {
		buf[i] = ptr[i] & QPNP_HAP_WF_AMP_MASK;
		if (buf[i])
			buf[i] |= (overdrive ? QPNP_HAP_WF_OVD_BIT : 0);
	}

	rc = qpnp_hap_write_mult_reg(hap, QPNP_HAP_WAV_S_REG_BASE(hap->base),
				buf, QPNP_HAP_WAV_SAMP_LEN);
	if (rc)
		return rc;

	return 0;
}

/* configuration api for pwm */
static int qpnp_hap_pwm_config(struct qpnp_hap *hap)
{
	u8 val = 0;
	int rc;

	/* Configure the EXTERNAL_PWM register */
	if (hap->ext_pwm_freq_khz <= QPNP_HAP_EXT_PWM_FREQ_25_KHZ) {
		hap->ext_pwm_freq_khz = QPNP_HAP_EXT_PWM_FREQ_25_KHZ;
		val = 0;
	} else if (hap->ext_pwm_freq_khz <=
				QPNP_HAP_EXT_PWM_FREQ_50_KHZ) {
		hap->ext_pwm_freq_khz = QPNP_HAP_EXT_PWM_FREQ_50_KHZ;
		val = 1;
	} else if (hap->ext_pwm_freq_khz <=
				QPNP_HAP_EXT_PWM_FREQ_75_KHZ) {
		hap->ext_pwm_freq_khz = QPNP_HAP_EXT_PWM_FREQ_75_KHZ;
		val = 2;
	} else {
		hap->ext_pwm_freq_khz = QPNP_HAP_EXT_PWM_FREQ_100_KHZ;
		val = 3;
	}

	rc = qpnp_hap_masked_write_reg(hap, QPNP_HAP_EXT_PWM_REG(hap->base),
			QPNP_HAP_EXT_PWM_MASK, val);
	if (rc)
		return rc;

	if (!hap->ext_pwm_dtest_line ||
			hap->ext_pwm_dtest_line > PWM_MAX_DTEST_LINES) {
		pr_err("invalid dtest line\n");
		return -EINVAL;
	}

	/* disable auto res for PWM mode */
	val = hap->ext_pwm_dtest_line << QPNP_HAP_EXT_PWM_DTEST_SHFT;
	rc = qpnp_hap_sec_masked_write_reg(hap, QPNP_HAP_TEST2_REG(hap->base),
		QPNP_HAP_EXT_PWM_DTEST_MASK | QPNP_HAP_AUTO_RES_MASK, val);
	if (rc)
		return rc;

	rc = pwm_config(hap->pwm_info.pwm_dev,
				hap->pwm_info.duty_us * NSEC_PER_USEC,
				hap->pwm_info.period_us * NSEC_PER_USEC);
	if (rc < 0) {
		pr_err("hap pwm config failed\n");
		pwm_free(hap->pwm_info.pwm_dev);
		return -ENODEV;
	}

	hap->pwm_cfg_state = true;

	return 0;
}

static int qpnp_hap_lra_auto_res_config(struct qpnp_hap *hap,
					struct qpnp_hap_lra_ares_cfg *tmp_cfg)
{
	struct qpnp_hap_lra_ares_cfg *ares_cfg;
	int rc;
	u8 val = 0, mask = 0;

	/* disable auto resonance for ERM */
	if (hap->act_type == QPNP_HAP_ERM) {
		val = 0x00;
		rc = qpnp_hap_write_reg(hap,
			QPNP_HAP_LRA_AUTO_RES_REG(hap->base), val);
		return rc;
	}

	if (hap->lra_hw_auto_resonance) {
		rc = qpnp_hap_masked_write_reg(hap,
			QPNP_HAP_AUTO_RES_CTRL(hap->base),
			QPNP_HAP_PM660_HW_AUTO_RES_MODE_BIT,
			QPNP_HAP_PM660_HW_AUTO_RES_MODE_BIT);
		if (rc)
			return rc;
	}

	if (tmp_cfg)
		ares_cfg = tmp_cfg;
	else
		ares_cfg = &hap->ares_cfg;

	if (ares_cfg->lra_res_cal_period < QPNP_HAP_RES_CAL_PERIOD_MIN)
		ares_cfg->lra_res_cal_period = QPNP_HAP_RES_CAL_PERIOD_MIN;

	if (hap->pmic_subtype == PM660_SUBTYPE) {
		if (ares_cfg->lra_res_cal_period >
				QPNP_HAP_PM660_RES_CAL_PERIOD_MAX)
			ares_cfg->lra_res_cal_period =
				QPNP_HAP_PM660_RES_CAL_PERIOD_MAX;

		if (ares_cfg->auto_res_mode == QPNP_HAP_PM660_AUTO_RES_QWD)
			ares_cfg->lra_res_cal_period = 0;

		if (ares_cfg->lra_res_cal_period)
			val = ilog2(ares_cfg->lra_res_cal_period /
					QPNP_HAP_RES_CAL_PERIOD_MIN) + 1;
	} else {
		if (ares_cfg->lra_res_cal_period > QPNP_HAP_RES_CAL_PERIOD_MAX)
			ares_cfg->lra_res_cal_period =
				QPNP_HAP_RES_CAL_PERIOD_MAX;

		if (ares_cfg->lra_res_cal_period)
			val = ilog2(ares_cfg->lra_res_cal_period /
					QPNP_HAP_RES_CAL_PERIOD_MIN);
	}

	if (hap->pmic_subtype == PM660_SUBTYPE) {
		val |= ares_cfg->auto_res_mode <<
			QPNP_HAP_PM660_AUTO_RES_MODE_SHIFT;
		mask = QPNP_HAP_PM660_AUTO_RES_MODE_BIT;
		val |= ares_cfg->lra_high_z <<
				QPNP_HAP_PM660_CALIBRATE_DURATION_SHIFT;
		mask |= QPNP_HAP_PM660_CALIBRATE_DURATION_MASK;
		if (ares_cfg->lra_qwd_drive_duration != -EINVAL) {
			val |= ares_cfg->lra_qwd_drive_duration <<
				QPNP_HAP_PM660_QWD_DRIVE_DURATION_SHIFT;
			mask |= QPNP_HAP_PM660_QWD_DRIVE_DURATION_BIT;
		}
		if (ares_cfg->calibrate_at_eop != -EINVAL) {
			val |= ares_cfg->calibrate_at_eop <<
				QPNP_HAP_PM660_CALIBRATE_AT_EOP_SHIFT;
			mask |= QPNP_HAP_PM660_CALIBRATE_AT_EOP_BIT;
		}
		mask |= QPNP_HAP_PM660_LRA_RES_CAL_PER_MASK;
	} else {
		val |= (ares_cfg->auto_res_mode <<
				QPNP_HAP_AUTO_RES_MODE_SHIFT);
		val |= (ares_cfg->lra_high_z << QPNP_HAP_LRA_HIGH_Z_SHIFT);
		mask = QPNP_HAP_AUTO_RES_MODE_MASK | QPNP_HAP_LRA_HIGH_Z_MASK |
			QPNP_HAP_LRA_RES_CAL_PER_MASK;
	}

	pr_debug("mode: %d hi_z period: %d cal_period: %d\n",
		ares_cfg->auto_res_mode, ares_cfg->lra_high_z,
		ares_cfg->lra_res_cal_period);

	rc = qpnp_hap_masked_write_reg(hap,
			QPNP_HAP_LRA_AUTO_RES_REG(hap->base), mask, val);
	return rc;
}

/* configuration api for play mode */
static int qpnp_hap_play_mode_config(struct qpnp_hap *hap)
{
	u8 val = 0;
	int rc;

	val = hap->play_mode << QPNP_HAP_WF_SOURCE_SHIFT;
	rc = qpnp_hap_masked_write_reg(hap, QPNP_HAP_SEL_REG(hap->base),
			QPNP_HAP_WF_SOURCE_MASK, val);
	return rc;
}

/* configuration api for max voltage */
static int qpnp_hap_vmax_config(struct qpnp_hap *hap, int vmax_mv,
				bool overdrive)
{
	u8 val = 0;
	int rc;

	if (vmax_mv < 0)
		return -EINVAL;

	/* Allow setting override bit in VMAX_CFG only for PM660 */
	if (hap->pmic_subtype != PM660_SUBTYPE)
		overdrive = false;

	if (vmax_mv < QPNP_HAP_VMAX_MIN_MV)
		vmax_mv = QPNP_HAP_VMAX_MIN_MV;
	else if (vmax_mv > QPNP_HAP_VMAX_MAX_MV)
		vmax_mv = QPNP_HAP_VMAX_MAX_MV;

	val = (vmax_mv / QPNP_HAP_VMAX_MIN_MV) << QPNP_HAP_VMAX_SHIFT;
	if (overdrive)
		val |= QPNP_HAP_VMAX_OVD_BIT;
	rc = qpnp_hap_masked_write_reg(hap, QPNP_HAP_VMAX_REG(hap->base),
			QPNP_HAP_VMAX_MASK | QPNP_HAP_VMAX_OVD_BIT, val);
	return rc;
}

/* configuration api for ilim */
static int qpnp_hap_ilim_config(struct qpnp_hap *hap)
{
	u8 val = 0;
	int rc;

	if (hap->ilim_ma < QPNP_HAP_ILIM_MIN_MA)
		hap->ilim_ma = QPNP_HAP_ILIM_MIN_MA;
	else if (hap->ilim_ma > QPNP_HAP_ILIM_MAX_MA)
		hap->ilim_ma = QPNP_HAP_ILIM_MAX_MA;

	val = (hap->ilim_ma / QPNP_HAP_ILIM_MIN_MA) - 1;
	rc = qpnp_hap_masked_write_reg(hap, QPNP_HAP_ILIM_REG(hap->base),
			QPNP_HAP_ILIM_MASK, val);
	return rc;
}

/* configuration api for short circuit debounce */
static int qpnp_hap_sc_deb_config(struct qpnp_hap *hap)
{
	u8 val = 0;
	int rc;

	if (hap->sc_deb_cycles < QPNP_HAP_SC_DEB_CYCLES_MIN)
		hap->sc_deb_cycles = QPNP_HAP_SC_DEB_CYCLES_MIN;
	else if (hap->sc_deb_cycles > QPNP_HAP_SC_DEB_CYCLES_MAX)
		hap->sc_deb_cycles = QPNP_HAP_SC_DEB_CYCLES_MAX;

	if (hap->sc_deb_cycles != QPNP_HAP_SC_DEB_CYCLES_MIN)
		val = ilog2(hap->sc_deb_cycles /
			QPNP_HAP_DEF_SC_DEB_CYCLES) + 1;
	else
		val = QPNP_HAP_SC_DEB_CYCLES_MIN;

	rc = qpnp_hap_masked_write_reg(hap, QPNP_HAP_SC_DEB_REG(hap->base),
			QPNP_HAP_SC_DEB_MASK, val);

	return rc;
}

static int qpnp_hap_int_pwm_config(struct qpnp_hap *hap)
{
	int rc;
	u8 val;

	if (hap->int_pwm_freq_khz <= QPNP_HAP_INT_PWM_FREQ_253_KHZ) {
		if (hap->pmic_subtype == PM660_SUBTYPE) {
			hap->int_pwm_freq_khz = QPNP_HAP_INT_PWM_FREQ_505_KHZ;
			val = 1;
		} else {
			hap->int_pwm_freq_khz = QPNP_HAP_INT_PWM_FREQ_253_KHZ;
			val = 0;
		}
	} else if (hap->int_pwm_freq_khz <= QPNP_HAP_INT_PWM_FREQ_505_KHZ) {
		hap->int_pwm_freq_khz = QPNP_HAP_INT_PWM_FREQ_505_KHZ;
		val = 1;
	} else if (hap->int_pwm_freq_khz <= QPNP_HAP_INT_PWM_FREQ_739_KHZ) {
		hap->int_pwm_freq_khz = QPNP_HAP_INT_PWM_FREQ_739_KHZ;
		val = 2;
	} else {
		hap->int_pwm_freq_khz = QPNP_HAP_INT_PWM_FREQ_1076_KHZ;
		val = 3;
	}

	rc = qpnp_hap_masked_write_reg(hap, QPNP_HAP_INT_PWM_REG(hap->base),
			QPNP_HAP_INT_PWM_MASK, val);
	if (rc)
		return rc;

	rc = qpnp_hap_masked_write_reg(hap, QPNP_HAP_PWM_CAP_REG(hap->base),
			QPNP_HAP_INT_PWM_MASK, val);
	return rc;
}

static int qpnp_hap_brake_config(struct qpnp_hap *hap, u8 *brake_pat)
{
	int rc, i;
	u32 temp;
	u8 *pat_ptr, val;

	if (!hap->en_brake)
		return 0;

	/* Configure BRAKE register */
	rc = qpnp_hap_masked_write_reg(hap, QPNP_HAP_EN_CTL2_REG(hap->base),
			QPNP_HAP_BRAKE_MASK, (u8)hap->en_brake);
	if (rc)
		return rc;

	if (!brake_pat)
		pat_ptr = hap->brake_pat;
	else
		pat_ptr = brake_pat;

	if (hap->sup_brake_pat) {
		for (i = QPNP_HAP_BRAKE_PAT_LEN - 1, val = 0; i >= 0; i--) {
			pat_ptr[i] &= QPNP_HAP_BRAKE_PAT_MASK;
			temp = i << 1;
			val |= pat_ptr[i] << temp;
		}
		rc = qpnp_hap_write_reg(hap, QPNP_HAP_BRAKE_REG(hap->base),
				val);
		if (rc)
			return rc;
	}

	return 0;
}

/* DT parsing api for buffer mode */
static int qpnp_hap_parse_buffer_dt(struct qpnp_hap *hap)
{
	struct platform_device *pdev = hap->pdev;
	struct property *prop;
	u32 temp;
	int rc, i;

	if (hap->wave_rep_cnt > 0 || hap->wave_s_rep_cnt > 0)
		return 0;

	hap->wave_rep_cnt = QPNP_HAP_WAV_REP_MIN;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,wave-rep-cnt", &temp);
	if (!rc) {
		hap->wave_rep_cnt = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read rep cnt\n");
		return rc;
	}

	hap->wave_s_rep_cnt = QPNP_HAP_WAV_S_REP_MIN;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,wave-samp-rep-cnt", &temp);
	if (!rc) {
		hap->wave_s_rep_cnt = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read samp rep cnt\n");
		return rc;
	}

	prop = of_find_property(pdev->dev.of_node,
			"qcom,wave-samples", &temp);
	if (!prop || temp != QPNP_HAP_WAV_SAMP_LEN) {
		pr_err("Invalid wave samples, use default");
		for (i = 0; i < QPNP_HAP_WAV_SAMP_LEN; i++)
			hap->wave_samp[i] = QPNP_HAP_WAV_SAMP_MAX;
	} else {
		memcpy(hap->wave_samp, prop->value, QPNP_HAP_WAV_SAMP_LEN);
	}

	return 0;
}

/* DT parsing api for PWM mode */
static int qpnp_hap_parse_pwm_dt(struct qpnp_hap *hap)
{
	struct platform_device *pdev = hap->pdev;
	u32 temp;
	int rc;

	hap->ext_pwm_freq_khz = QPNP_HAP_EXT_PWM_FREQ_25_KHZ;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,ext-pwm-freq-khz", &temp);
	if (!rc) {
		hap->ext_pwm_freq_khz = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read ext pwm freq\n");
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,pwm-channel", &temp);
	if (!rc)
		hap->pwm_info.pwm_channel = temp;
	else
		return rc;

	hap->pwm_info.pwm_dev = of_pwm_get(pdev->dev.of_node, NULL);

	if (IS_ERR(hap->pwm_info.pwm_dev)) {
		rc = PTR_ERR(hap->pwm_info.pwm_dev);
		pr_err("Cannot get PWM device rc:(%d)\n", rc);
		hap->pwm_info.pwm_dev = NULL;
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,period-us", &temp);
	if (!rc)
		hap->pwm_info.period_us = temp;
	else
		return rc;

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,duty-us", &temp);
	if (!rc)
		hap->pwm_info.duty_us = temp;
	else
		return rc;

	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,ext-pwm-dtest-line", &temp);
	if (!rc)
		hap->ext_pwm_dtest_line = temp;
	else
		return rc;

	return 0;
}

/* sysfs show for wave samples */
static ssize_t qpnp_hap_wf_samp_show(struct device *dev, char *buf, int index)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);

	if (index < 0 || index >= QPNP_HAP_WAV_SAMP_LEN) {
		pr_err("Invalid sample index(%d)\n", index);
		return -EINVAL;
	}

	return snprintf(buf, PAGE_SIZE, "0x%x\n",
			hap->shadow_wave_samp[index]);
}

static ssize_t qpnp_hap_wf_s0_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return qpnp_hap_wf_samp_show(dev, buf, 0);
}

static ssize_t qpnp_hap_wf_s1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return qpnp_hap_wf_samp_show(dev, buf, 1);
}

static ssize_t qpnp_hap_wf_s2_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return qpnp_hap_wf_samp_show(dev, buf, 2);
}

static ssize_t qpnp_hap_wf_s3_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return qpnp_hap_wf_samp_show(dev, buf, 3);
}

static ssize_t qpnp_hap_wf_s4_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return qpnp_hap_wf_samp_show(dev, buf, 4);
}

static ssize_t qpnp_hap_wf_s5_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return qpnp_hap_wf_samp_show(dev, buf, 5);
}

static ssize_t qpnp_hap_wf_s6_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return qpnp_hap_wf_samp_show(dev, buf, 6);
}

static ssize_t qpnp_hap_wf_s7_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return qpnp_hap_wf_samp_show(dev, buf, 7);
}

/* sysfs store for wave samples */
static ssize_t qpnp_hap_wf_samp_store(struct device *dev,
		const char *buf, size_t count, int index)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);
	int data, rc;

	if (index < 0 || index >= QPNP_HAP_WAV_SAMP_LEN) {
		pr_err("Invalid sample index(%d)\n", index);
		return -EINVAL;
	}

	rc = kstrtoint(buf, 16, &data);
	if (rc)
		return rc;

	if (data < 0 || data > 0xff) {
		pr_err("Invalid sample wf_%d (%d)\n", index, data);
		return -EINVAL;
	}

	hap->shadow_wave_samp[index] = (u8) data;
	return count;
}

static ssize_t qpnp_hap_wf_s0_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return qpnp_hap_wf_samp_store(dev, buf, count, 0);
}

static ssize_t qpnp_hap_wf_s1_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return qpnp_hap_wf_samp_store(dev, buf, count, 1);
}

static ssize_t qpnp_hap_wf_s2_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return qpnp_hap_wf_samp_store(dev, buf, count, 2);
}

static ssize_t qpnp_hap_wf_s3_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return qpnp_hap_wf_samp_store(dev, buf, count, 3);
}

static ssize_t qpnp_hap_wf_s4_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return qpnp_hap_wf_samp_store(dev, buf, count, 4);
}

static ssize_t qpnp_hap_wf_s5_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return qpnp_hap_wf_samp_store(dev, buf, count, 5);
}

static ssize_t qpnp_hap_wf_s6_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return qpnp_hap_wf_samp_store(dev, buf, count, 6);
}

static ssize_t qpnp_hap_wf_s7_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return qpnp_hap_wf_samp_store(dev, buf, count, 7);
}

/* sysfs show for wave form update */
static ssize_t qpnp_hap_wf_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", hap->wf_update);
}

/* sysfs store for updating wave samples */
static ssize_t qpnp_hap_wf_update_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);

	mutex_lock(&hap->wf_lock);
	hap->wf_update = true;
	mutex_unlock(&hap->wf_lock);

	return count;
}

/* sysfs show for wave repeat */
static ssize_t qpnp_hap_wf_rep_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", hap->wave_rep_cnt);
}

/* sysfs store for wave repeat */
static ssize_t qpnp_hap_wf_rep_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);
	int data, rc;
	u8 val;

	rc = kstrtoint(buf, 10, &data);
	if (rc)
		return rc;

	if (data < QPNP_HAP_WAV_REP_MIN)
		data = QPNP_HAP_WAV_REP_MIN;
	else if (data > QPNP_HAP_WAV_REP_MAX)
		data = QPNP_HAP_WAV_REP_MAX;

	val = ilog2(data) << QPNP_HAP_WAV_REP_SHIFT;
	rc = qpnp_hap_masked_write_reg(hap, QPNP_HAP_WAV_REP_REG(hap->base),
			QPNP_HAP_WAV_REP_MASK, val);
	if (!rc)
		hap->wave_rep_cnt = data;

	return count;
}

/* sysfs show for wave samples repeat */
static ssize_t qpnp_hap_wf_s_rep_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", hap->wave_s_rep_cnt);
}

/* sysfs store for wave samples repeat */
static ssize_t qpnp_hap_wf_s_rep_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);
	int data, rc;
	u8 val;

	rc = kstrtoint(buf, 10, &data);
	if (rc)
		return rc;

	if (data < QPNP_HAP_WAV_S_REP_MIN)
		data = QPNP_HAP_WAV_S_REP_MIN;
	else if (data > QPNP_HAP_WAV_S_REP_MAX)
		data = QPNP_HAP_WAV_S_REP_MAX;

	val = ilog2(hap->wave_s_rep_cnt);
	rc = qpnp_hap_masked_write_reg(hap, QPNP_HAP_WAV_REP_REG(hap->base),
			QPNP_HAP_WAV_S_REP_MASK, val);
	if (!rc)
		hap->wave_s_rep_cnt = data;

	return count;
}

static int parse_string(const char *in_buf, char *out_buf)
{
	int i;

	if (snprintf(out_buf, QPNP_HAP_STR_SIZE, "%s", in_buf)
		> QPNP_HAP_STR_SIZE)
		return -EINVAL;

	for (i = 0; i < strlen(out_buf); i++) {
		if (out_buf[i] == ' ' || out_buf[i] == '\n' ||
			out_buf[i] == '\t') {
			out_buf[i] = '\0';
			break;
		}
	}

	return 0;
}

/* sysfs store function for play mode*/
static ssize_t qpnp_hap_play_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);
	char str[QPNP_HAP_STR_SIZE + 1];
	int rc = 0, temp, old_mode;

	rc = parse_string(buf, str);
	if (rc < 0)
		return rc;

	if (strcmp(str, "buffer") == 0)
		temp = QPNP_HAP_BUFFER;
	else if (strcmp(str, "direct") == 0)
		temp = QPNP_HAP_DIRECT;
	else if (strcmp(str, "audio") == 0)
		temp = QPNP_HAP_AUDIO;
	else if (strcmp(str, "pwm") == 0)
		temp = QPNP_HAP_PWM;
	else
		return -EINVAL;

	if (temp == hap->play_mode)
		return count;

	if (temp == QPNP_HAP_BUFFER) {
		rc = qpnp_hap_parse_buffer_dt(hap);
		if (!rc)
			rc = qpnp_hap_buffer_config(hap, NULL, false);
	} else if (temp == QPNP_HAP_PWM && !hap->pwm_cfg_state) {
		rc = qpnp_hap_parse_pwm_dt(hap);
		if (!rc)
			rc = qpnp_hap_pwm_config(hap);
	}

	if (rc < 0)
		return rc;

	rc = qpnp_hap_mod_enable(hap, false);
	if (rc < 0)
		return rc;

	old_mode = hap->play_mode;
	hap->play_mode = temp;
	/* Configure the PLAY MODE register */
	rc = qpnp_hap_play_mode_config(hap);
	if (rc) {
		hap->play_mode = old_mode;
		return rc;
	}

	if (hap->play_mode == QPNP_HAP_AUDIO) {
		rc = qpnp_hap_mod_enable(hap, true);
		if (rc < 0) {
			hap->play_mode = old_mode;
			return rc;
		}
	}

	return count;
}

/* sysfs show function for play mode */
static ssize_t qpnp_hap_play_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);
	char *str;

	if (hap->play_mode == QPNP_HAP_BUFFER)
		str = "buffer";
	else if (hap->play_mode == QPNP_HAP_DIRECT)
		str = "direct";
	else if (hap->play_mode == QPNP_HAP_AUDIO)
		str = "audio";
	else if (hap->play_mode == QPNP_HAP_PWM)
		str = "pwm";
	else
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%s\n", str);
}

/* sysfs store for ramp test data */
static ssize_t qpnp_hap_min_max_test_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);

	int value = QPNP_TEST_TIMER_MS, i;

	mutex_lock(&hap->lock);
	qpnp_hap_mod_enable(hap, true);
	for (i = 0; i < ARRAY_SIZE(qpnp_hap_min_max_test_data); i++) {
		hrtimer_start(&hap->hap_test_timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000),
			      HRTIMER_MODE_REL);
		qpnp_hap_play_byte(qpnp_hap_min_max_test_data[i], true);
		wait_for_completion(&hap->completion);
	}

	qpnp_hap_play_byte(0, false);
	qpnp_hap_mod_enable(hap, false);
	mutex_unlock(&hap->lock);

	return count;
}

/* sysfs show function for min max test data */
static ssize_t qpnp_hap_min_max_test_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int count = 0, i;

	for (i = 0; i < ARRAY_SIZE(qpnp_hap_min_max_test_data); i++) {
		count += snprintf(buf + count, PAGE_SIZE - count,
				"qpnp_haptics: min_max_test_data[%d] = 0x%x\n",
				i, qpnp_hap_min_max_test_data[i]);

		if (count >= PAGE_SIZE)
			return PAGE_SIZE - 1;
	}

	return count;

}

/* sysfs store for ramp test data */
static ssize_t qpnp_hap_ramp_test_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);

	int value = QPNP_TEST_TIMER_MS, i;

	mutex_lock(&hap->lock);
	qpnp_hap_mod_enable(hap, true);
	for (i = 0; i < ARRAY_SIZE(qpnp_hap_ramp_test_data); i++) {
		hrtimer_start(&hap->hap_test_timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000),
			      HRTIMER_MODE_REL);
		qpnp_hap_play_byte(qpnp_hap_ramp_test_data[i], true);
		wait_for_completion(&hap->completion);
	}

	qpnp_hap_play_byte(0, false);
	qpnp_hap_mod_enable(hap, false);
	mutex_unlock(&hap->lock);

	return count;
}

/* sysfs show function for ramp test data */
static ssize_t qpnp_hap_ramp_test_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int count = 0, i;

	for (i = 0; i < ARRAY_SIZE(qpnp_hap_ramp_test_data); i++) {
		count += snprintf(buf + count, PAGE_SIZE - count,
				"qpnp_haptics: ramp_test_data[%d] = 0x%x\n",
				i, qpnp_hap_ramp_test_data[i]);

		if (count >= PAGE_SIZE)
			return PAGE_SIZE - 1;
	}

	return count;

}

static ssize_t qpnp_hap_auto_res_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);
	char *str;

	if (hap->pmic_subtype == PM660_SUBTYPE) {
		switch (hap->ares_cfg.auto_res_mode) {
		case QPNP_HAP_PM660_AUTO_RES_ZXD:
			str = "ZXD";
			break;
		case QPNP_HAP_PM660_AUTO_RES_QWD:
			str = "QWD";
			break;
		default:
			str = "None";
			break;
		}
	} else {
		switch (hap->ares_cfg.auto_res_mode) {
		case QPNP_HAP_AUTO_RES_NONE:
			str = "None";
			break;
		case QPNP_HAP_AUTO_RES_ZXD:
			str = "ZXD";
			break;
		case QPNP_HAP_AUTO_RES_QWD:
			str = "QWD";
			break;
		case QPNP_HAP_AUTO_RES_MAX_QWD:
			str = "MAX_QWD";
			break;
		case QPNP_HAP_AUTO_RES_ZXD_EOP:
			str = "ZXD_EOP";
			break;
		default:
			str = "None";
			break;
		}
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", str);
}

static ssize_t qpnp_hap_auto_res_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);
	char str[QPNP_HAP_STR_SIZE + 1];
	int rc = 0, temp;

	rc = parse_string(buf, str);
	if (rc < 0)
		return rc;

	if (hap->pmic_subtype == PM660_SUBTYPE) {
		if (strcmp(str, "ZXD") == 0 ||
			strcmp(str, "zxd") == 0)
			temp = QPNP_HAP_PM660_AUTO_RES_ZXD;
		else if (strcmp(str, "QWD") == 0 ||
				strcmp(str, "qwd") == 0)
			temp = QPNP_HAP_PM660_AUTO_RES_QWD;
		else {
			pr_err("Should be ZXD or QWD\n");
			return -EINVAL;
		}
	} else {
		if (strcmp(str, "None") == 0)
			temp = QPNP_HAP_AUTO_RES_NONE;
		else if (strcmp(str, "ZXD") == 0 ||
				strcmp(str, "zxd") == 0)
			temp = QPNP_HAP_AUTO_RES_ZXD;
		else if (strcmp(str, "QWD") == 0 ||
				strcmp(str, "qwd") == 0)
			temp = QPNP_HAP_AUTO_RES_QWD;
		else if (strcmp(str, "ZXD_EOP") == 0 ||
				strcmp(str, "zxd_eop") == 0)
			temp = QPNP_HAP_AUTO_RES_ZXD_EOP;
		else if (strcmp(str, "MAX_QWD") == 0 ||
				strcmp(str, "max_qwd") == 0)
			temp = QPNP_HAP_AUTO_RES_MAX_QWD;
		else {
			pr_err("Should be None or ZXD or QWD or ZXD_EOP or MAX_QWD\n");
			return -EINVAL;
		}
	}

	hap->ares_cfg.auto_res_mode = temp;
	return count;
}

static ssize_t qpnp_hap_hi_z_period_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);
	char *str;

	switch (hap->ares_cfg.lra_high_z) {
	case QPNP_HAP_LRA_HIGH_Z_NONE:
		str = "high_z_none";
		break;
	case QPNP_HAP_LRA_HIGH_Z_OPT1:
		str = "high_z_opt1";
		break;
	case QPNP_HAP_LRA_HIGH_Z_OPT2:
		str = "high_z_opt2";
		break;
	case QPNP_HAP_LRA_HIGH_Z_OPT3:
		str = "high_z_opt3";
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", str);
}

static ssize_t qpnp_hap_hi_z_period_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);
	int data, rc;

	rc = kstrtoint(buf, 10, &data);
	if (rc)
		return rc;

	if (data < QPNP_HAP_LRA_HIGH_Z_NONE
		|| data > QPNP_HAP_LRA_HIGH_Z_OPT3) {
		pr_err("Invalid high Z configuration\n");
		return -EINVAL;
	}

	hap->ares_cfg.lra_high_z = data;
	return count;
}

static ssize_t qpnp_hap_calib_period_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		hap->ares_cfg.lra_res_cal_period);
}

static ssize_t qpnp_hap_calib_period_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);
	int data, rc;

	rc = kstrtoint(buf, 10, &data);
	if (rc)
		return rc;

	if (data < QPNP_HAP_RES_CAL_PERIOD_MIN) {
		pr_err("Invalid auto resonance calibration period\n");
		return -EINVAL;
	}

	if (hap->pmic_subtype == PM660_SUBTYPE) {
		if (data > QPNP_HAP_PM660_RES_CAL_PERIOD_MAX) {
			pr_err("Invalid auto resonance calibration period\n");
			return -EINVAL;
		}
	} else {
		if (data > QPNP_HAP_RES_CAL_PERIOD_MAX) {
			pr_err("Invalid auto resonance calibration period\n");
			return -EINVAL;
		}
	}

	hap->ares_cfg.lra_res_cal_period = data;
	return count;
}

static ssize_t qpnp_hap_override_auto_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", hap->override_auto_mode_config);
}

static ssize_t qpnp_hap_override_auto_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);
	int data, rc;

	rc = kstrtoint(buf, 10, &data);
	if (rc)
		return rc;

	hap->override_auto_mode_config = data;
	return count;
}

static ssize_t qpnp_hap_vmax_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", hap->vmax_mv);
}

static ssize_t qpnp_hap_vmax_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);
	int data, rc;

	rc = kstrtoint(buf, 10, &data);
	if (rc)
		return rc;

	hap->vmax_mv = data;
	return count;
}

/* sysfs attributes */
static struct device_attribute qpnp_hap_attrs[] = {
	__ATTR(wf_s0, 0664, qpnp_hap_wf_s0_show, qpnp_hap_wf_s0_store),
	__ATTR(wf_s1, 0664, qpnp_hap_wf_s1_show, qpnp_hap_wf_s1_store),
	__ATTR(wf_s2, 0664, qpnp_hap_wf_s2_show, qpnp_hap_wf_s2_store),
	__ATTR(wf_s3, 0664, qpnp_hap_wf_s3_show, qpnp_hap_wf_s3_store),
	__ATTR(wf_s4, 0664, qpnp_hap_wf_s4_show, qpnp_hap_wf_s4_store),
	__ATTR(wf_s5, 0664, qpnp_hap_wf_s5_show, qpnp_hap_wf_s5_store),
	__ATTR(wf_s6, 0664, qpnp_hap_wf_s6_show, qpnp_hap_wf_s6_store),
	__ATTR(wf_s7, 0664, qpnp_hap_wf_s7_show, qpnp_hap_wf_s7_store),
	__ATTR(wf_update, 0664, qpnp_hap_wf_update_show,
		qpnp_hap_wf_update_store),
	__ATTR(wf_rep, 0664, qpnp_hap_wf_rep_show, qpnp_hap_wf_rep_store),
	__ATTR(wf_s_rep, 0664, qpnp_hap_wf_s_rep_show, qpnp_hap_wf_s_rep_store),
	__ATTR(play_mode, 0664, qpnp_hap_play_mode_show,
		qpnp_hap_play_mode_store),
	__ATTR(dump_regs, 0664, qpnp_hap_dump_regs_show, NULL),
	__ATTR(ramp_test, 0664, qpnp_hap_ramp_test_data_show,
		qpnp_hap_ramp_test_data_store),
	__ATTR(min_max_test, 0664, qpnp_hap_min_max_test_data_show,
		qpnp_hap_min_max_test_data_store),
	__ATTR(auto_res_mode, 0664, qpnp_hap_auto_res_mode_show,
		qpnp_hap_auto_res_mode_store),
	__ATTR(high_z_period, 0664, qpnp_hap_hi_z_period_show,
		qpnp_hap_hi_z_period_store),
	__ATTR(calib_period, 0664, qpnp_hap_calib_period_show,
		qpnp_hap_calib_period_store),
	__ATTR(override_auto_mode_config, 0664,
		qpnp_hap_override_auto_mode_show,
		qpnp_hap_override_auto_mode_store),
	__ATTR(vmax_mv, 0664, qpnp_hap_vmax_show, qpnp_hap_vmax_store),
};

static int calculate_lra_code(struct qpnp_hap *hap)
{
	u8 lra_drive_period_code_lo = 0, lra_drive_period_code_hi = 0;
	u32 lra_drive_period_code, lra_drive_frequency_hz, freq_variation;
	u8 start_variation = AUTO_RES_ERROR_MAX, i;
	u8 neg_idx = 0, pos_idx = ADJUSTED_LRA_PLAY_RATE_CODE_ARRSIZE - 1;
	int rc = 0;

	rc = qpnp_hap_read_reg(hap, QPNP_HAP_RATE_CFG1_REG(hap->base),
			&lra_drive_period_code_lo);
	if (rc) {
		pr_err("Error while reading RATE_CFG1 register\n");
		return rc;
	}

	rc = qpnp_hap_read_reg(hap, QPNP_HAP_RATE_CFG2_REG(hap->base),
			&lra_drive_period_code_hi);
	if (rc) {
		pr_err("Error while reading RATE_CFG2 register\n");
		return rc;
	}

	if (!lra_drive_period_code_lo && !lra_drive_period_code_hi) {
		pr_err("Unexpected Error: both RATE_CFG1 and RATE_CFG2 read 0\n");
		return -EINVAL;
	}

	lra_drive_period_code =
	 (lra_drive_period_code_hi << 8) | (lra_drive_period_code_lo & 0xff);
	lra_drive_frequency_hz = 200000 / lra_drive_period_code;

	while (start_variation >= AUTO_RES_ERROR_CAPTURE_RES) {
		freq_variation =
			 (lra_drive_frequency_hz * start_variation) / 100;
		adjusted_lra_play_rate_code[neg_idx++] =
			200000 / (lra_drive_frequency_hz - freq_variation);
		adjusted_lra_play_rate_code[pos_idx--] =
			200000 / (lra_drive_frequency_hz + freq_variation);
		start_variation -= AUTO_RES_ERROR_CAPTURE_RES;
	}

	pr_debug("lra_drive_period_code_lo = 0x%x lra_drive_period_code_hi = 0x%x\n"
		"lra_drive_period_code = 0x%x, lra_drive_frequency_hz = 0x%x\n"
		"Calculated play rate code values are :\n",
		lra_drive_period_code_lo, lra_drive_period_code_hi,
		lra_drive_period_code, lra_drive_frequency_hz);

	for (i = 0; i < ADJUSTED_LRA_PLAY_RATE_CODE_ARRSIZE; ++i)
		pr_debug(" 0x%x", adjusted_lra_play_rate_code[i]);

	return 0;
}

static int qpnp_hap_auto_res_enable(struct qpnp_hap *hap, int enable)
{
	int rc = 0;
	u32 back_emf_delay_us = hap->time_required_to_generate_back_emf_us;
	u8 val, auto_res_mode_qwd;

	if (hap->act_type != QPNP_HAP_LRA)
		return 0;

	if (hap->pmic_subtype == PM660_SUBTYPE)
		auto_res_mode_qwd = (hap->ares_cfg.auto_res_mode ==
						QPNP_HAP_PM660_AUTO_RES_QWD);
	else
		auto_res_mode_qwd = (hap->ares_cfg.auto_res_mode ==
							QPNP_HAP_AUTO_RES_QWD);

	/*
	 * Do not enable auto resonance if auto mode is enabled and auto
	 * resonance mode is QWD, meaning short pattern.
	 */
	if (hap->auto_mode && auto_res_mode_qwd && enable) {
		pr_debug("auto_mode enabled, not enabling auto_res\n");
		return 0;
	}

	if (!hap->correct_lra_drive_freq && !auto_res_mode_qwd) {
		pr_debug("correct_lra_drive_freq: %d auto_res_mode_qwd: %d\n",
			hap->correct_lra_drive_freq, auto_res_mode_qwd);
		return 0;
	}

	val = enable ? AUTO_RES_ENABLE : 0;
	/*
	 * For auto resonance detection to work properly, sufficient back-emf
	 * has to be generated. In general, back-emf takes some time to build
	 * up. When the auto resonance mode is chosen as QWD, high-z will be
	 * applied for every LRA cycle and hence there won't be enough back-emf
	 * at the start-up. Hence, the motor needs to vibrate for few LRA cycles
	 * after the PLAY bit is asserted. Enable the auto resonance after
	 * 'time_required_to_generate_back_emf_us' is completed.
	 */
	if (enable)
		usleep_range(back_emf_delay_us, back_emf_delay_us + 1);

	if (hap->pmic_subtype == PM660_SUBTYPE)
		rc = qpnp_hap_masked_write_reg(hap,
				QPNP_HAP_AUTO_RES_CTRL(hap->base),
				QPNP_HAP_AUTO_RES_MASK, val);
	else
		rc = qpnp_hap_sec_masked_write_reg(hap,
				QPNP_HAP_TEST2_REG(hap->base),
				QPNP_HAP_AUTO_RES_MASK, val);
	if (rc < 0)
		return rc;

	if (enable)
		hap->status_flags |= AUTO_RESONANCE_ENABLED;
	else
		hap->status_flags &= ~AUTO_RESONANCE_ENABLED;

	pr_debug("auto_res %sabled\n", enable ? "en" : "dis");
	return rc;
}

static void update_lra_frequency(struct qpnp_hap *hap)
{
	u8 lra_auto_res[2], val;
	u32 play_rate_code;
	u16 rate_cfg;
	int rc;

	rc = qpnp_hap_read_mult_reg(hap, QPNP_HAP_LRA_AUTO_RES_LO(hap->base),
				lra_auto_res, 2);
	if (rc < 0) {
		pr_err("Error in reading LRA_AUTO_RES_LO/HI, rc=%d\n", rc);
		return;
	}

	play_rate_code =
		 (lra_auto_res[1] & 0xF0) << 4 | (lra_auto_res[0] & 0xFF);

	pr_debug("lra_auto_res_lo = 0x%x lra_auto_res_hi = 0x%x play_rate_code = 0x%x\n",
		lra_auto_res[0], lra_auto_res[1], play_rate_code);

	rc = qpnp_hap_read_reg(hap, QPNP_HAP_STATUS(hap->base), &val);
	if (rc < 0)
		return;

	/*
	 * If the drive period code read from AUTO_RES_LO and AUTO_RES_HI
	 * registers is more than the max limit percent variation or less
	 * than the min limit percent variation specified through DT, then
	 * auto-resonance is disabled.
	 */

	if ((val & AUTO_RES_ERR_BIT) ||
		((play_rate_code <= hap->drive_period_code_min_limit) ||
		(play_rate_code >= hap->drive_period_code_max_limit))) {
		if (val & AUTO_RES_ERR_BIT)
			pr_debug("Auto-resonance error %x\n", val);
		else
			pr_debug("play rate %x out of bounds [min: 0x%x, max: 0x%x]\n",
				play_rate_code,
				hap->drive_period_code_min_limit,
				hap->drive_period_code_max_limit);
		rc = qpnp_hap_auto_res_enable(hap, 0);
		if (rc < 0)
			pr_debug("Auto-resonance write failed\n");
		return;
	}

	lra_auto_res[1] >>= 4;
	rate_cfg = lra_auto_res[1] << 8 | lra_auto_res[0];
	if (hap->last_rate_cfg == rate_cfg) {
		pr_debug("Same rate_cfg, skip updating\n");
		return;
	}

	rc = qpnp_hap_write_mult_reg(hap, QPNP_HAP_RATE_CFG1_REG(hap->base),
				lra_auto_res, 2);
	if (rc < 0) {
		pr_err("Error in writing to RATE_CFG1/2, rc=%d\n", rc);
	} else {
		pr_debug("Update RATE_CFG with [0x%x]\n", rate_cfg);
		hap->last_rate_cfg = rate_cfg;
	}
}

static enum hrtimer_restart detect_auto_res_error(struct hrtimer *timer)
{
	struct qpnp_hap *hap = container_of(timer, struct qpnp_hap,
					auto_res_err_poll_timer);
	ktime_t currtime;

	if (!(hap->status_flags & AUTO_RESONANCE_ENABLED))
		return HRTIMER_NORESTART;

	update_lra_frequency(hap);
	currtime  = ktime_get();
	hrtimer_forward(&hap->auto_res_err_poll_timer, currtime,
			ktime_set(0, POLL_TIME_AUTO_RES_ERR_NS));
	return HRTIMER_RESTART;
}

static bool is_sw_lra_auto_resonance_control(struct qpnp_hap *hap)
{
	if (hap->act_type != QPNP_HAP_LRA)
		return false;

	if (hap->lra_hw_auto_resonance)
		return false;

	if (!hap->correct_lra_drive_freq)
		return false;

	if (hap->auto_mode && hap->play_mode == QPNP_HAP_BUFFER)
		return false;

	return true;
}

/* set api for haptics */
static int qpnp_hap_set(struct qpnp_hap *hap, bool on)
{
	int rc = 0;
	unsigned long timeout_ns = POLL_TIME_AUTO_RES_ERR_NS;

	if (hap->play_mode == QPNP_HAP_PWM) {
		if (on) {
			rc = pwm_enable(hap->pwm_info.pwm_dev);
			if (rc < 0)
				return rc;
		} else {
			pwm_disable(hap->pwm_info.pwm_dev);
		}
	} else if (hap->play_mode == QPNP_HAP_BUFFER ||
			hap->play_mode == QPNP_HAP_DIRECT) {
		if (on) {
			rc = qpnp_hap_auto_res_enable(hap, 0);
			if (rc < 0)
				return rc;

			rc = qpnp_hap_mod_enable(hap, on);
			if (rc < 0)
				return rc;

			rc = qpnp_hap_play(hap, on);
			if (rc < 0)
				return rc;

			rc = qpnp_hap_auto_res_enable(hap, 1);
			if (rc < 0)
				return rc;

			if (is_sw_lra_auto_resonance_control(hap)) {
				/*
				 * Start timer to poll Auto Resonance error bit
				 */
				mutex_lock(&hap->lock);
				hrtimer_cancel(&hap->auto_res_err_poll_timer);
				hrtimer_start(&hap->auto_res_err_poll_timer,
						ktime_set(0, timeout_ns),
						HRTIMER_MODE_REL);
				mutex_unlock(&hap->lock);
			}
		} else {
			rc = qpnp_hap_play(hap, on);
			if (rc < 0)
				return rc;

			if (is_sw_lra_auto_resonance_control(hap) &&
				(hap->status_flags & AUTO_RESONANCE_ENABLED))
				update_lra_frequency(hap);

			rc = qpnp_hap_mod_enable(hap, on);
			if (rc < 0)
				return rc;

			if (is_sw_lra_auto_resonance_control(hap))
				hrtimer_cancel(&hap->auto_res_err_poll_timer);
		}
	}

	return rc;
}

static int qpnp_hap_auto_mode_config(struct qpnp_hap *hap, int time_ms)
{
	struct qpnp_hap_lra_ares_cfg ares_cfg;
	enum qpnp_hap_mode old_play_mode;
	u8 old_ares_mode;
	u8 brake_pat[QPNP_HAP_BRAKE_PAT_LEN] = {0};
	u8 wave_samp[QPNP_HAP_WAV_SAMP_LEN] = {0};
	int rc, vmax_mv;

	/* For now, this is for LRA only */
	if (hap->act_type == QPNP_HAP_ERM)
		return 0;

	old_ares_mode = hap->ares_cfg.auto_res_mode;
	old_play_mode = hap->play_mode;
	pr_debug("auto_mode, time_ms: %d\n", time_ms);
	if (time_ms <= 20) {
		wave_samp[0] = QPNP_HAP_WAV_SAMP_MAX;
		wave_samp[1] = QPNP_HAP_WAV_SAMP_MAX;
		if (time_ms > 15)
			wave_samp[2] = QPNP_HAP_WAV_SAMP_MAX;

		/* short pattern */
		rc = qpnp_hap_parse_buffer_dt(hap);
		if (!rc)
			rc = qpnp_hap_buffer_config(hap, wave_samp, true);
		if (rc < 0) {
			pr_err("Error in configuring buffer mode %d\n",
				rc);
			return rc;
		}

		ares_cfg.lra_high_z = QPNP_HAP_LRA_HIGH_Z_OPT1;
		ares_cfg.lra_res_cal_period = QPNP_HAP_RES_CAL_PERIOD_MIN;
		if (hap->pmic_subtype == PM660_SUBTYPE) {
			ares_cfg.auto_res_mode =
						QPNP_HAP_PM660_AUTO_RES_QWD;
			ares_cfg.lra_qwd_drive_duration = 0;
			ares_cfg.calibrate_at_eop = 0;
		} else {
			ares_cfg.auto_res_mode = QPNP_HAP_AUTO_RES_QWD;
			ares_cfg.lra_qwd_drive_duration = -EINVAL;
			ares_cfg.calibrate_at_eop = -EINVAL;
		}

		vmax_mv = QPNP_HAP_VMAX_MAX_MV;
		rc = qpnp_hap_vmax_config(hap, vmax_mv, true);
		if (rc < 0)
			return rc;

		rc = qpnp_hap_brake_config(hap, brake_pat);
		if (rc < 0)
			return rc;

		/* enable play_irq for buffer mode */
		if (hap->play_irq >= 0 && !hap->play_irq_en) {
			enable_irq(hap->play_irq);
			hap->play_irq_en = true;
		}

		hap->play_mode = QPNP_HAP_BUFFER;
		hap->wave_shape = QPNP_HAP_WAV_SQUARE;
	} else {
		/* long pattern */
		ares_cfg.lra_high_z = QPNP_HAP_LRA_HIGH_Z_OPT1;
		if (hap->pmic_subtype == PM660_SUBTYPE) {
			ares_cfg.auto_res_mode =
				QPNP_HAP_PM660_AUTO_RES_ZXD;
			ares_cfg.lra_res_cal_period =
				QPNP_HAP_PM660_RES_CAL_PERIOD_MAX;
			ares_cfg.lra_qwd_drive_duration = 0;
			ares_cfg.calibrate_at_eop = 1;
		} else {
			ares_cfg.auto_res_mode = QPNP_HAP_AUTO_RES_ZXD_EOP;
			ares_cfg.lra_res_cal_period =
				QPNP_HAP_RES_CAL_PERIOD_MAX;
			ares_cfg.lra_qwd_drive_duration = -EINVAL;
			ares_cfg.calibrate_at_eop = -EINVAL;
		}

		vmax_mv = hap->vmax_mv;
		rc = qpnp_hap_vmax_config(hap, vmax_mv, false);
		if (rc < 0)
			return rc;

		brake_pat[0] = 0x3;
		rc = qpnp_hap_brake_config(hap, brake_pat);
		if (rc < 0)
			return rc;

		/* enable play_irq for direct mode */
		if (hap->play_irq >= 0 && hap->play_irq_en) {
			disable_irq(hap->play_irq);
			hap->play_irq_en = false;
		}

		hap->play_mode = QPNP_HAP_DIRECT;
		hap->wave_shape = QPNP_HAP_WAV_SINE;
	}

	if (hap->override_auto_mode_config) {
		rc = qpnp_hap_lra_auto_res_config(hap, NULL);
	} else {
		hap->ares_cfg.auto_res_mode = ares_cfg.auto_res_mode;
		rc = qpnp_hap_lra_auto_res_config(hap, &ares_cfg);
	}

	if (rc < 0) {
		hap->ares_cfg.auto_res_mode = old_ares_mode;
		return rc;
	}

	rc = qpnp_hap_play_mode_config(hap);
	if (rc < 0) {
		hap->play_mode = old_play_mode;
		return rc;
	}

	rc = qpnp_hap_masked_write_reg(hap, QPNP_HAP_CFG2_REG(hap->base),
			QPNP_HAP_WAV_SHAPE_MASK, hap->wave_shape);
	if (rc < 0)
		return rc;

	return 0;
}

/* enable interface from timed output class */
static void qpnp_hap_td_enable(struct timed_output_dev *dev, int time_ms)
{
	struct qpnp_hap *hap = container_of(dev, struct qpnp_hap,
					 timed_dev);
	int rc;

	if (time_ms <= 0)
		return;

	if (time_ms < 10)
		time_ms = 10;

	mutex_lock(&hap->lock);
	if (is_sw_lra_auto_resonance_control(hap))
		hrtimer_cancel(&hap->auto_res_err_poll_timer);

	hrtimer_cancel(&hap->hap_timer);

	if (hap->auto_mode) {
		rc = qpnp_hap_auto_mode_config(hap, time_ms);
		if (rc < 0) {
			pr_err("Unable to do auto mode config\n");
			mutex_unlock(&hap->lock);
			return;
		}
	}

	time_ms = (time_ms > hap->timeout_ms ? hap->timeout_ms : time_ms);
	hap->play_time_ms = time_ms;
	hap->state = 1;
	hrtimer_start(&hap->hap_timer,
		ktime_set(time_ms / 1000, (time_ms % 1000) * 1000000),
		HRTIMER_MODE_REL);
	mutex_unlock(&hap->lock);
	schedule_work(&hap->work);
}

/* play pwm bytes */
int qpnp_hap_play_byte(u8 data, bool on)
{
	struct qpnp_hap *hap = ghap;
	int duty_ns, period_ns, duty_percent, rc;

	if (!hap) {
		pr_err("Haptics is not initialized\n");
		return -EINVAL;
	}

	if (hap->play_mode != QPNP_HAP_PWM) {
		pr_err("only PWM mode is supported\n");
		return -EINVAL;
	}

	rc = qpnp_hap_set(hap, false);
	if (rc)
		return rc;

	if (!on) {
		/*
		 * Set the pwm back to original duty for normal operations.
		 * This is not required if standard interface is not used.
		 */
		rc = pwm_config(hap->pwm_info.pwm_dev,
				hap->pwm_info.duty_us * NSEC_PER_USEC,
				hap->pwm_info.period_us * NSEC_PER_USEC);
		return rc;
	}

	/*
	 * pwm values range from 0x00 to 0xff. The range from 0x00 to 0x7f
	 * provides a postive amplitude in the sin wave form for 0 to 100%.
	 * The range from 0x80 to 0xff provides a negative amplitude in the
	 * sin wave form for 0 to 100%. Here the duty percentage is calculated
	 * based on the incoming data to accommodate this.
	 */
	if (data <= QPNP_HAP_EXT_PWM_PEAK_DATA)
		duty_percent = QPNP_HAP_EXT_PWM_HALF_DUTY +
			((data * QPNP_HAP_EXT_PWM_DATA_FACTOR) / 100);
	else
		duty_percent = QPNP_HAP_EXT_PWM_FULL_DUTY -
			((data * QPNP_HAP_EXT_PWM_DATA_FACTOR) / 100);

	period_ns = hap->pwm_info.period_us * NSEC_PER_USEC;
	duty_ns = (period_ns * duty_percent) / 100;
	rc = pwm_config(hap->pwm_info.pwm_dev,
			duty_ns,
			hap->pwm_info.period_us * NSEC_PER_USEC);
	if (rc)
		return rc;

	pr_debug("data=0x%x duty_per=%d\n", data, duty_percent);

	rc = qpnp_hap_set(hap, true);

	return rc;
}
EXPORT_SYMBOL(qpnp_hap_play_byte);

/* worker to opeate haptics */
static void qpnp_hap_worker(struct work_struct *work)
{
	struct qpnp_hap *hap = container_of(work, struct qpnp_hap,
					 work);
	u8 val = 0x00;
	int rc;

	if (hap->vcc_pon && hap->state && !hap->vcc_pon_enabled) {
		rc = regulator_enable(hap->vcc_pon);
		if (rc < 0)
			pr_err("could not enable vcc_pon regulator rc=%d\n",
				rc);
		else
			hap->vcc_pon_enabled = true;
	}

	/* Disable haptics module if the duration of short circuit
	 * exceeds the maximum limit (5 secs).
	 */
	if (hap->sc_count >= SC_MAX_COUNT) {
		rc = qpnp_hap_write_reg(hap, QPNP_HAP_EN_CTL_REG(hap->base),
			val);
	} else {
		if (hap->play_mode == QPNP_HAP_PWM)
			qpnp_hap_mod_enable(hap, hap->state);
		qpnp_hap_set(hap, hap->state);
	}

	if (hap->vcc_pon && !hap->state && hap->vcc_pon_enabled) {
		rc = regulator_disable(hap->vcc_pon);
		if (rc)
			pr_err("could not disable vcc_pon regulator rc=%d\n",
				rc);
		else
			hap->vcc_pon_enabled = false;
	}
}

/* get time api to know the remaining time */
static int qpnp_hap_get_time(struct timed_output_dev *dev)
{
	struct qpnp_hap *hap = container_of(dev, struct qpnp_hap,
							 timed_dev);

	if (hrtimer_active(&hap->hap_timer)) {
		ktime_t r = hrtimer_get_remaining(&hap->hap_timer);

		return (int)ktime_to_us(r);
	} else {
		return 0;
	}
}

/* hrtimer function handler */
static enum hrtimer_restart qpnp_hap_timer(struct hrtimer *timer)
{
	struct qpnp_hap *hap = container_of(timer, struct qpnp_hap,
							 hap_timer);

	hap->state = 0;
	schedule_work(&hap->work);

	return HRTIMER_NORESTART;
}

/* hrtimer function handler */
static enum hrtimer_restart qpnp_hap_test_timer(struct hrtimer *timer)
{
	struct qpnp_hap *hap = container_of(timer, struct qpnp_hap,
							 hap_test_timer);

	complete(&hap->completion);

	return HRTIMER_NORESTART;
}

/* suspend routines to turn off haptics */
#ifdef CONFIG_PM
static int qpnp_haptic_suspend(struct device *dev)
{
	struct qpnp_hap *hap = dev_get_drvdata(dev);

	hrtimer_cancel(&hap->hap_timer);
	cancel_work_sync(&hap->work);
	/* turn-off haptic */
	qpnp_hap_set(hap, false);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(qpnp_haptic_pm_ops, qpnp_haptic_suspend, NULL);

/* Configuration api for haptics registers */
static int qpnp_hap_config(struct qpnp_hap *hap)
{
	u8 val = 0;
	int rc;

	/*
	 * This denotes the percentage error in rc clock multiplied by 10
	 */
	u8 rc_clk_err_percent_x10;

	/* Configure the CFG1 register for actuator type */
	rc = qpnp_hap_masked_write_reg(hap, QPNP_HAP_CFG1_REG(hap->base),
			QPNP_HAP_ACT_TYPE_MASK, hap->act_type);
	if (rc)
		return rc;

	/* Configure auto resonance parameters */
	rc = qpnp_hap_lra_auto_res_config(hap, NULL);
	if (rc)
		return rc;

	/* Configure the PLAY MODE register */
	rc = qpnp_hap_play_mode_config(hap);
	if (rc)
		return rc;

	/* Configure the VMAX register */
	rc = qpnp_hap_vmax_config(hap, hap->vmax_mv, false);
	if (rc)
		return rc;

	/* Configure the ILIM register */
	rc = qpnp_hap_ilim_config(hap);
	if (rc)
		return rc;

	/* Configure the short circuit debounce register */
	rc = qpnp_hap_sc_deb_config(hap);
	if (rc)
		return rc;

	/* Configure the INTERNAL_PWM register */
	rc = qpnp_hap_int_pwm_config(hap);
	if (rc)
		return rc;

	/* Configure the WAVE SHAPE register */
	rc = qpnp_hap_masked_write_reg(hap, QPNP_HAP_CFG2_REG(hap->base),
			QPNP_HAP_WAV_SHAPE_MASK, hap->wave_shape);
	if (rc)
		return rc;

	/*
	 * Configure RATE_CFG1 and RATE_CFG2 registers.
	 * Note: For ERM these registers act as play rate and
	 * for LRA these represent resonance period
	 */
	if (hap->wave_play_rate_us < QPNP_HAP_WAV_PLAY_RATE_US_MIN)
		hap->wave_play_rate_us = QPNP_HAP_WAV_PLAY_RATE_US_MIN;
	else if (hap->wave_play_rate_us > QPNP_HAP_WAV_PLAY_RATE_US_MAX)
		hap->wave_play_rate_us = QPNP_HAP_WAV_PLAY_RATE_US_MAX;

	hap->init_drive_period_code =
			 hap->wave_play_rate_us / QPNP_HAP_RATE_CFG_STEP_US;

	/*
	 * The frequency of 19.2Mzhz RC clock is subject to variation. Currently
	 * a few PMI modules have MISC_TRIM_ERROR_RC19P2_CLK register
	 * present in their MISC  block. This register holds the frequency error
	 * in 19.2 MHz RC clock.
	 */
	if ((hap->act_type == QPNP_HAP_LRA) && hap->correct_lra_drive_freq
			&& hap->misc_clk_trim_error_reg) {
		pr_debug("TRIM register = 0x%x\n", hap->clk_trim_error_code);

		/*
		 * Extract the 4 LSBs and multiply by 7 to get
		 * the %error in RC clock multiplied by 10
		 */
		rc_clk_err_percent_x10 = (hap->clk_trim_error_code & 0x0F) * 7;

		/*
		 * If the TRIM register holds value less than 0x80,
		 * then there is a positive error in the RC clock.
		 * If the TRIM register holds value greater than or equal to
		 * 0x80, then there is a negative error in the RC clock. Bit 7
		 * is the sign bit for error code.
		 *
		 * The adjusted play rate code is calculated as follows:
		 * LRA drive period code (RATE_CFG) =
		 *	 200KHz * 1 / LRA drive frequency * ( 1 + %error/ 100)
		 *
		 * This can be rewritten as:
		 * LRA drive period code (RATE_CFG) =
		 *	200KHz * 1/LRA drive frequency *( 1 + %error * 10/1000)
		 *
		 * Since 200KHz * 1/LRA drive frequency is already calculated
		 * above we only do rest of the scaling here.
		 */
		if (hap->clk_trim_error_code & BIT(7))
			LRA_DRIVE_PERIOD_NEG_ERR(hap, rc_clk_err_percent_x10);
		else
			LRA_DRIVE_PERIOD_POS_ERR(hap, rc_clk_err_percent_x10);
	}

	pr_debug("Play rate code 0x%x\n", hap->init_drive_period_code);

	val = hap->init_drive_period_code & QPNP_HAP_RATE_CFG1_MASK;
	rc = qpnp_hap_write_reg(hap, QPNP_HAP_RATE_CFG1_REG(hap->base), val);
	if (rc)
		return rc;

	val = (hap->init_drive_period_code & 0xF00) >> QPNP_HAP_RATE_CFG2_SHFT;
	rc = qpnp_hap_write_reg(hap, QPNP_HAP_RATE_CFG2_REG(hap->base), val);
	if (rc)
		return rc;

	hap->last_rate_cfg = hap->init_drive_period_code;

	if (hap->act_type == QPNP_HAP_LRA &&
				hap->perform_lra_auto_resonance_search)
		calculate_lra_code(hap);

	if (hap->act_type == QPNP_HAP_LRA && hap->correct_lra_drive_freq) {
		hap->drive_period_code_max_limit =
			(hap->init_drive_period_code * (100 +
			hap->drive_period_code_max_limit_percent_variation))
			/ 100;
		hap->drive_period_code_min_limit =
			(hap->init_drive_period_code * (100 -
			hap->drive_period_code_min_limit_percent_variation))
			/ 100;
		pr_debug("Drive period code max limit %x min limit %x\n",
			hap->drive_period_code_max_limit,
			hap->drive_period_code_min_limit);
	}

	rc = qpnp_hap_brake_config(hap, NULL);
	if (rc < 0)
		return rc;

	if (hap->play_mode == QPNP_HAP_BUFFER)
		rc = qpnp_hap_buffer_config(hap, NULL, false);
	else if (hap->play_mode == QPNP_HAP_PWM)
		rc = qpnp_hap_pwm_config(hap);
	else if (hap->play_mode == QPNP_HAP_AUDIO)
		rc = qpnp_hap_mod_enable(hap, true);

	if (rc)
		return rc;

	/* setup play irq */
	if (hap->play_irq >= 0) {
		rc = devm_request_threaded_irq(&hap->pdev->dev, hap->play_irq,
			NULL, qpnp_hap_play_irq, IRQF_ONESHOT, "qpnp_hap_play",
			hap);
		if (rc < 0) {
			pr_err("Unable to request play(%d) IRQ(err:%d)\n",
				hap->play_irq, rc);
			return rc;
		}

		/* use play_irq only for buffer mode */
		if (hap->play_mode != QPNP_HAP_BUFFER) {
			disable_irq(hap->play_irq);
			hap->play_irq_en = false;
		}
	}

	/* setup short circuit irq */
	if (hap->sc_irq >= 0) {
		rc = devm_request_threaded_irq(&hap->pdev->dev, hap->sc_irq,
			NULL, qpnp_hap_sc_irq, IRQF_ONESHOT, "qpnp_hap_sc",
			hap);
		if (rc < 0) {
			pr_err("Unable to request sc(%d) IRQ(err:%d)\n",
				hap->sc_irq, rc);
			return rc;
		}
	}

	hap->sc_count = 0;

	return rc;
}

/* DT parsing for haptics parameters */
static int qpnp_hap_parse_dt(struct qpnp_hap *hap)
{
	struct platform_device *pdev = hap->pdev;
	struct device_node *misc_node;
	struct property *prop;
	const char *temp_str;
	u32 temp;
	int rc;

	if (of_find_property(pdev->dev.of_node, "qcom,pmic-misc", NULL)) {
		misc_node = of_parse_phandle(pdev->dev.of_node,
					"qcom,pmic-misc", 0);
		if (!misc_node)
			return -EINVAL;

		rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,misc-clk-trim-error-reg", &temp);
		if (rc < 0) {
			pr_err("Missing misc-clk-trim-error-reg\n");
			return rc;
		}

		if (!temp || temp > 0xFF) {
			pr_err("Invalid misc-clk-trim-error-reg\n");
			return -EINVAL;
		}

		hap->misc_clk_trim_error_reg = temp;
		rc = qpnp_misc_read_reg(misc_node, hap->misc_clk_trim_error_reg,
				&hap->clk_trim_error_code);
		if (rc < 0) {
			pr_err("Couldn't get clk_trim_error_code, rc=%d\n", rc);
			return -EPROBE_DEFER;
		}
	}

	hap->timeout_ms = QPNP_HAP_TIMEOUT_MS_MAX;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,timeout-ms", &temp);
	if (!rc) {
		hap->timeout_ms = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read timeout\n");
		return rc;
	}

	hap->act_type = QPNP_HAP_LRA;
	rc = of_property_read_string(pdev->dev.of_node,
			"qcom,actuator-type", &temp_str);
	if (!rc) {
		if (strcmp(temp_str, "erm") == 0)
			hap->act_type = QPNP_HAP_ERM;
		else if (strcmp(temp_str, "lra") == 0)
			hap->act_type = QPNP_HAP_LRA;
		else {
			pr_err("Invalid actuator type\n");
			return -EINVAL;
		}
	} else if (rc != -EINVAL) {
		pr_err("Unable to read actuator type\n");
		return rc;
	}

	if (hap->act_type == QPNP_HAP_LRA) {
		rc = of_property_read_string(pdev->dev.of_node,
				"qcom,lra-auto-res-mode", &temp_str);
		if (!rc) {
			if (hap->pmic_subtype == PM660_SUBTYPE) {
				hap->ares_cfg.auto_res_mode =
						QPNP_HAP_PM660_AUTO_RES_QWD;
				if (strcmp(temp_str, "zxd") == 0)
					hap->ares_cfg.auto_res_mode =
						QPNP_HAP_PM660_AUTO_RES_ZXD;
				else if (strcmp(temp_str, "qwd") == 0)
					hap->ares_cfg.auto_res_mode =
						QPNP_HAP_PM660_AUTO_RES_QWD;
			} else {
				hap->ares_cfg.auto_res_mode =
					QPNP_HAP_AUTO_RES_ZXD_EOP;
				if (strcmp(temp_str, "none") == 0)
					hap->ares_cfg.auto_res_mode =
						QPNP_HAP_AUTO_RES_NONE;
				else if (strcmp(temp_str, "zxd") == 0)
					hap->ares_cfg.auto_res_mode =
						QPNP_HAP_AUTO_RES_ZXD;
				else if (strcmp(temp_str, "qwd") == 0)
					hap->ares_cfg.auto_res_mode =
						QPNP_HAP_AUTO_RES_QWD;
				else if (strcmp(temp_str, "max-qwd") == 0)
					hap->ares_cfg.auto_res_mode =
						QPNP_HAP_AUTO_RES_MAX_QWD;
				else
					hap->ares_cfg.auto_res_mode =
						QPNP_HAP_AUTO_RES_ZXD_EOP;
			}
		} else if (rc != -EINVAL) {
			pr_err("Unable to read auto res mode\n");
			return rc;
		}

		hap->ares_cfg.lra_high_z = QPNP_HAP_LRA_HIGH_Z_OPT3;
		rc = of_property_read_string(pdev->dev.of_node,
				"qcom,lra-high-z", &temp_str);
		if (!rc) {
			if (strcmp(temp_str, "none") == 0)
				hap->ares_cfg.lra_high_z =
					QPNP_HAP_LRA_HIGH_Z_NONE;
			else if (strcmp(temp_str, "opt1") == 0)
				hap->ares_cfg.lra_high_z =
					QPNP_HAP_LRA_HIGH_Z_OPT1;
			else if (strcmp(temp_str, "opt2") == 0)
				hap->ares_cfg.lra_high_z =
					 QPNP_HAP_LRA_HIGH_Z_OPT2;
			else
				hap->ares_cfg.lra_high_z =
					 QPNP_HAP_LRA_HIGH_Z_OPT3;

			if (hap->pmic_subtype == PM660_SUBTYPE) {
				if (strcmp(temp_str, "opt0") == 0)
					hap->ares_cfg.lra_high_z =
						QPNP_HAP_LRA_HIGH_Z_NONE;
			}
		} else if (rc != -EINVAL) {
			pr_err("Unable to read LRA high-z\n");
			return rc;
		}

		hap->ares_cfg.lra_qwd_drive_duration = -EINVAL;
		rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,lra-qwd-drive-duration",
				&hap->ares_cfg.lra_qwd_drive_duration);

		hap->ares_cfg.calibrate_at_eop = -EINVAL;
		rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,lra-calibrate-at-eop",
				&hap->ares_cfg.calibrate_at_eop);

		hap->ares_cfg.lra_res_cal_period = QPNP_HAP_RES_CAL_PERIOD_MAX;
		rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,lra-res-cal-period", &temp);
		if (!rc) {
			hap->ares_cfg.lra_res_cal_period = temp;
		} else if (rc != -EINVAL) {
			pr_err("Unable to read cal period\n");
			return rc;
		}

		hap->lra_hw_auto_resonance =
				of_property_read_bool(pdev->dev.of_node,
				"qcom,lra-hw-auto-resonance");

		hap->perform_lra_auto_resonance_search =
				of_property_read_bool(pdev->dev.of_node,
				"qcom,perform-lra-auto-resonance-search");

		hap->correct_lra_drive_freq =
				of_property_read_bool(pdev->dev.of_node,
						"qcom,correct-lra-drive-freq");

		hap->drive_period_code_max_limit_percent_variation = 25;
		rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,drive-period-code-max-limit-percent-variation", &temp);
		if (!rc)
			hap->drive_period_code_max_limit_percent_variation =
								(u8) temp;

		hap->drive_period_code_min_limit_percent_variation = 25;
		rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,drive-period-code-min-limit-percent-variation", &temp);
		if (!rc)
			hap->drive_period_code_min_limit_percent_variation =
								(u8) temp;

		if (hap->ares_cfg.auto_res_mode == QPNP_HAP_AUTO_RES_QWD) {
			hap->time_required_to_generate_back_emf_us =
					QPNP_HAP_TIME_REQ_FOR_BACK_EMF_GEN;
			rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,time-required-to-generate-back-emf-us",
				&temp);
			if (!rc)
				hap->time_required_to_generate_back_emf_us =
									temp;
		} else {
			hap->time_required_to_generate_back_emf_us = 0;
		}
	}

	rc = of_property_read_string(pdev->dev.of_node,
				"qcom,play-mode", &temp_str);
	if (!rc) {
		if (strcmp(temp_str, "direct") == 0)
			hap->play_mode = QPNP_HAP_DIRECT;
		else if (strcmp(temp_str, "buffer") == 0)
			hap->play_mode = QPNP_HAP_BUFFER;
		else if (strcmp(temp_str, "pwm") == 0)
			hap->play_mode = QPNP_HAP_PWM;
		else if (strcmp(temp_str, "audio") == 0)
			hap->play_mode = QPNP_HAP_AUDIO;
		else {
			pr_err("Invalid play mode\n");
			return -EINVAL;
		}
	} else {
		pr_err("Unable to read play mode\n");
		return rc;
	}

	hap->vmax_mv = QPNP_HAP_VMAX_MAX_MV;
	rc = of_property_read_u32(pdev->dev.of_node, "qcom,vmax-mv", &temp);
	if (!rc) {
		hap->vmax_mv = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read vmax\n");
		return rc;
	}

	hap->ilim_ma = QPNP_HAP_ILIM_MIN_MV;
	rc = of_property_read_u32(pdev->dev.of_node, "qcom,ilim-ma", &temp);
	if (!rc) {
		hap->ilim_ma = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read ILim\n");
		return rc;
	}

	hap->sc_deb_cycles = QPNP_HAP_DEF_SC_DEB_CYCLES;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,sc-deb-cycles", &temp);
	if (!rc) {
		hap->sc_deb_cycles = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read sc debounce\n");
		return rc;
	}

	hap->int_pwm_freq_khz = QPNP_HAP_INT_PWM_FREQ_505_KHZ;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,int-pwm-freq-khz", &temp);
	if (!rc) {
		hap->int_pwm_freq_khz = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read int pwm freq\n");
		return rc;
	}

	hap->wave_shape = QPNP_HAP_WAV_SQUARE;
	rc = of_property_read_string(pdev->dev.of_node,
			"qcom,wave-shape", &temp_str);
	if (!rc) {
		if (strcmp(temp_str, "sine") == 0)
			hap->wave_shape = QPNP_HAP_WAV_SINE;
		else if (strcmp(temp_str, "square") == 0)
			hap->wave_shape = QPNP_HAP_WAV_SQUARE;
		else {
			pr_err("Unsupported wav shape\n");
			return -EINVAL;
		}
	} else if (rc != -EINVAL) {
		pr_err("Unable to read wav shape\n");
		return rc;
	}

	hap->wave_play_rate_us = QPNP_HAP_DEF_WAVE_PLAY_RATE_US;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,wave-play-rate-us", &temp);
	if (!rc) {
		hap->wave_play_rate_us = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read play rate\n");
		return rc;
	}

	if (hap->play_mode == QPNP_HAP_BUFFER)
		rc = qpnp_hap_parse_buffer_dt(hap);
	else if (hap->play_mode == QPNP_HAP_PWM)
		rc = qpnp_hap_parse_pwm_dt(hap);

	if (rc < 0)
		return rc;

	hap->en_brake = of_property_read_bool(pdev->dev.of_node,
				"qcom,en-brake");

	if (hap->en_brake) {
		prop = of_find_property(pdev->dev.of_node,
				"qcom,brake-pattern", &temp);
		if (!prop) {
			pr_info("brake pattern not found");
		} else if (temp != QPNP_HAP_BRAKE_PAT_LEN) {
			pr_err("Invalid len of brake pattern\n");
			return -EINVAL;
		} else {
			hap->sup_brake_pat = true;
			memcpy(hap->brake_pat, prop->value,
					QPNP_HAP_BRAKE_PAT_LEN);
		}
	}

	hap->play_irq = platform_get_irq_byname(hap->pdev, "play-irq");
	if (hap->play_irq < 0)
		pr_warn("Unable to get play irq\n");

	hap->sc_irq = platform_get_irq_byname(hap->pdev, "sc-irq");
	if (hap->sc_irq < 0) {
		pr_err("Unable to get sc irq\n");
		return hap->sc_irq;
	}

	if (of_find_property(pdev->dev.of_node, "vcc_pon-supply", NULL))
		hap->manage_pon_supply = true;

	hap->auto_mode = of_property_read_bool(pdev->dev.of_node,
				"qcom,lra-auto-mode");
	return 0;
}

static int qpnp_hap_get_pmic_revid(struct qpnp_hap *hap)
{
	struct pmic_revid_data *pmic_rev_id;
	struct device_node *revid_dev_node;

	revid_dev_node = of_parse_phandle(hap->pdev->dev.of_node,
					"qcom,pmic-revid", 0);
	if (!revid_dev_node) {
		pr_err("Missing qcom,pmic-revid property - driver failed\n");
		return -EINVAL;
	}
	pmic_rev_id = get_revid_data(revid_dev_node);
	if (IS_ERR_OR_NULL(pmic_rev_id)) {
		pr_err("Unable to get pmic_revid rc=%ld\n",
						PTR_ERR(pmic_rev_id));
		/*
		 * the revid peripheral must be registered, any failure
		 * here only indicates that the rev-id module has not
		 * probed yet.
		 */
		return -EPROBE_DEFER;
	}

	hap->pmic_subtype = pmic_rev_id->pmic_subtype;

	return 0;
}

static int qpnp_haptic_probe(struct platform_device *pdev)
{
	struct qpnp_hap *hap;
	unsigned int base;
	struct regulator *vcc_pon;
	int rc, i;

	hap = devm_kzalloc(&pdev->dev, sizeof(*hap), GFP_KERNEL);
	if (!hap)
		return -ENOMEM;
		hap->regmap = dev_get_regmap(pdev->dev.parent, NULL);
		if (!hap->regmap) {
			pr_err("Couldn't get parent's regmap\n");
			return -EINVAL;
		}

	hap->pdev = pdev;

	rc = of_property_read_u32(pdev->dev.of_node, "reg", &base);
	if (rc < 0) {
		pr_err("Couldn't find reg in node = %s rc = %d\n",
			pdev->dev.of_node->full_name, rc);
		return rc;
	}
	hap->base = base;

	dev_set_drvdata(&pdev->dev, hap);

	rc = qpnp_hap_get_pmic_revid(hap);
	if (rc) {
		pr_err("Unable to check PMIC version rc=%d\n", rc);
		return rc;
	}

	rc = qpnp_hap_parse_dt(hap);
	if (rc) {
		pr_err("DT parsing failed\n");
		return rc;
	}

	spin_lock_init(&hap->bus_lock);
	rc = qpnp_hap_config(hap);
	if (rc) {
		pr_err("hap config failed\n");
		return rc;
	}

	mutex_init(&hap->lock);
	mutex_init(&hap->wf_lock);
	INIT_WORK(&hap->work, qpnp_hap_worker);
	INIT_DELAYED_WORK(&hap->sc_work, qpnp_handle_sc_irq);
	init_completion(&hap->completion);

	hrtimer_init(&hap->hap_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hap->hap_timer.function = qpnp_hap_timer;

	hrtimer_init(&hap->hap_test_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hap->hap_test_timer.function = qpnp_hap_test_timer;

	hap->timed_dev.name = "vibrator";
	hap->timed_dev.get_time = qpnp_hap_get_time;
	hap->timed_dev.enable = qpnp_hap_td_enable;

	hrtimer_init(&hap->auto_res_err_poll_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	hap->auto_res_err_poll_timer.function = detect_auto_res_error;

	rc = timed_output_dev_register(&hap->timed_dev);
	if (rc < 0) {
		pr_err("timed_output registration failed\n");
		goto timed_output_fail;
	}

	for (i = 0; i < ARRAY_SIZE(qpnp_hap_attrs); i++) {
		rc = sysfs_create_file(&hap->timed_dev.dev->kobj,
				&qpnp_hap_attrs[i].attr);
		if (rc < 0) {
			pr_err("sysfs creation failed\n");
			goto sysfs_fail;
		}
	}

	if (hap->manage_pon_supply) {
		vcc_pon = regulator_get(&pdev->dev, "vcc_pon");
		if (IS_ERR(vcc_pon)) {
			rc = PTR_ERR(vcc_pon);
			pr_err("regulator get failed vcc_pon rc=%d\n", rc);
			goto sysfs_fail;
		}
		hap->vcc_pon = vcc_pon;
	}

	ghap = hap;

	return 0;

sysfs_fail:
	for (i--; i >= 0; i--)
		sysfs_remove_file(&hap->timed_dev.dev->kobj,
				&qpnp_hap_attrs[i].attr);
	timed_output_dev_unregister(&hap->timed_dev);
timed_output_fail:
	cancel_work_sync(&hap->work);
	hrtimer_cancel(&hap->auto_res_err_poll_timer);
	hrtimer_cancel(&hap->hap_timer);
	mutex_destroy(&hap->lock);
	mutex_destroy(&hap->wf_lock);

	return rc;
}

static int qpnp_haptic_remove(struct platform_device *pdev)
{
	struct qpnp_hap *hap = dev_get_drvdata(&pdev->dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(qpnp_hap_attrs); i++)
		sysfs_remove_file(&hap->timed_dev.dev->kobj,
				&qpnp_hap_attrs[i].attr);

	cancel_work_sync(&hap->work);
	hrtimer_cancel(&hap->auto_res_err_poll_timer);
	hrtimer_cancel(&hap->hap_timer);
	timed_output_dev_unregister(&hap->timed_dev);
	mutex_destroy(&hap->lock);
	mutex_destroy(&hap->wf_lock);
	if (hap->vcc_pon)
		regulator_put(hap->vcc_pon);

	return 0;
}

static const struct of_device_id spmi_match_table[] = {
	{ .compatible = "qcom,qpnp-haptic", },
	{ },
};

static struct platform_driver qpnp_haptic_driver = {
	.driver		= {
		.name		= "qcom,qpnp-haptic",
		.of_match_table	= spmi_match_table,
		.pm		= &qpnp_haptic_pm_ops,
	},
	.probe		= qpnp_haptic_probe,
	.remove		= qpnp_haptic_remove,
};

static int __init qpnp_haptic_init(void)
{
	return platform_driver_register(&qpnp_haptic_driver);
}
module_init(qpnp_haptic_init);

static void __exit qpnp_haptic_exit(void)
{
	return platform_driver_unregister(&qpnp_haptic_driver);
}
module_exit(qpnp_haptic_exit);

MODULE_DESCRIPTION("qpnp haptic driver");
MODULE_LICENSE("GPL v2");
