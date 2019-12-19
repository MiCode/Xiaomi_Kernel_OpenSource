/* Copyright (c) 2014-2015, 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#define pr_fmt(fmt)	"haptics: %s: " fmt, __func__

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/log2.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/qpnp/qpnp-misc.h>
#include <linux/qpnp/qpnp-revid.h>

/* Register definitions */
#define HAP_STATUS_1_REG(chip)		(chip->base + 0x0A)
#define HAP_BUSY_BIT			BIT(1)
#define SC_FLAG_BIT			BIT(3)
#define AUTO_RES_ERROR_BIT		BIT(4)

#define HAP_LRA_AUTO_RES_LO_REG(chip)	(chip->base + 0x0B)
#define HAP_LRA_AUTO_RES_HI_REG(chip)	(chip->base + 0x0C)

#define HAP_INT_RT_STS_REG(chip)	(chip->base + 0x10)
#define SC_INT_RT_STS_BIT		BIT(0)
#define PLAY_INT_RT_STS_BIT		BIT(1)

#define HAP_EN_CTL_REG(chip)		(chip->base + 0x46)
#define HAP_EN_BIT			BIT(7)

#define HAP_EN_CTL2_REG(chip)		(chip->base + 0x48)
#define BRAKE_EN_BIT			BIT(0)

#define HAP_AUTO_RES_CTRL_REG(chip)	(chip->base + 0x4B)
#define AUTO_RES_EN_BIT			BIT(7)
#define AUTO_RES_ERR_RECOVERY_BIT	BIT(3)

#define HAP_CFG1_REG(chip)		(chip->base + 0x4C)
#define HAP_ACT_TYPE_MASK		BIT(0)
#define HAP_LRA				0
#define HAP_ERM				1

#define HAP_CFG2_REG(chip)		(chip->base + 0x4D)
#define HAP_WAVE_SINE			0
#define HAP_WAVE_SQUARE			1
#define HAP_LRA_RES_TYPE_MASK		BIT(0)

#define HAP_SEL_REG(chip)		(chip->base + 0x4E)
#define HAP_WF_SOURCE_MASK		GENMASK(5, 4)
#define HAP_WF_SOURCE_SHIFT		4

#define HAP_LRA_AUTO_RES_REG(chip)	(chip->base + 0x4F)
/* For pmi8998 */
#define LRA_AUTO_RES_MODE_MASK		GENMASK(6, 4)
#define LRA_AUTO_RES_MODE_SHIFT		4
#define LRA_HIGH_Z_MASK			GENMASK(3, 2)
#define LRA_HIGH_Z_SHIFT		2
#define LRA_RES_CAL_MASK		GENMASK(1, 0)
#define HAP_RES_CAL_PERIOD_MIN		4
#define HAP_RES_CAL_PERIOD_MAX		32
/* For pm660 */
#define PM660_AUTO_RES_MODE_BIT		BIT(7)
#define PM660_AUTO_RES_MODE_SHIFT	7
#define PM660_CAL_DURATION_MASK		GENMASK(6, 5)
#define PM660_CAL_DURATION_SHIFT	5
#define PM660_QWD_DRIVE_DURATION_BIT	BIT(4)
#define PM660_QWD_DRIVE_DURATION_SHIFT	4
#define PM660_CAL_EOP_BIT		BIT(3)
#define PM660_CAL_EOP_SHIFT		3
#define PM660_LRA_RES_CAL_MASK		GENMASK(2, 0)
#define HAP_PM660_RES_CAL_PERIOD_MAX	256

#define HAP_VMAX_CFG_REG(chip)		(chip->base + 0x51)
#define HAP_VMAX_OVD_BIT		BIT(6)
#define HAP_VMAX_MASK			GENMASK(5, 1)
#define HAP_VMAX_SHIFT			1
#define HAP_VMAX_MIN_MV			116
#define HAP_VMAX_MAX_MV			3596

#define HAP_ILIM_CFG_REG(chip)		(chip->base + 0x52)
#define HAP_ILIM_SEL_MASK		BIT(0)
#define HAP_ILIM_400_MA			0
#define HAP_ILIM_800_MA			1

#define HAP_SC_DEB_REG(chip)		(chip->base + 0x53)
#define HAP_SC_DEB_MASK			GENMASK(2, 0)
#define HAP_SC_DEB_CYCLES_MIN		0
#define HAP_DEF_SC_DEB_CYCLES		8
#define HAP_SC_DEB_CYCLES_MAX		32

#define HAP_RATE_CFG1_REG(chip)		(chip->base + 0x54)
#define HAP_RATE_CFG1_MASK		GENMASK(7, 0)

#define HAP_RATE_CFG2_REG(chip)		(chip->base + 0x55)
#define HAP_RATE_CFG2_MASK		GENMASK(3, 0)
/* Shift needed to convert drive period upper bits [11:8] */
#define HAP_RATE_CFG2_SHIFT		8

#define HAP_INT_PWM_REG(chip)		(chip->base + 0x56)
#define INT_PWM_FREQ_SEL_MASK		GENMASK(1, 0)
#define INT_PWM_FREQ_253_KHZ		0
#define INT_PWM_FREQ_505_KHZ		1
#define INT_PWM_FREQ_739_KHZ		2
#define INT_PWM_FREQ_1076_KHZ		3

#define HAP_EXT_PWM_REG(chip)		(chip->base + 0x57)
#define EXT_PWM_FREQ_SEL_MASK		GENMASK(1, 0)
#define EXT_PWM_FREQ_25_KHZ		0
#define EXT_PWM_FREQ_50_KHZ		1
#define EXT_PWM_FREQ_75_KHZ		2
#define EXT_PWM_FREQ_100_KHZ		3

#define HAP_PWM_CAP_REG(chip)		(chip->base + 0x58)

#define HAP_SC_CLR_REG(chip)		(chip->base + 0x59)
#define SC_CLR_BIT			BIT(0)

#define HAP_BRAKE_REG(chip)		(chip->base + 0x5C)
#define HAP_BRAKE_PAT_MASK		0x3

#define HAP_WF_REPEAT_REG(chip)		(chip->base + 0x5E)
#define WF_REPEAT_MASK			GENMASK(6, 4)
#define WF_REPEAT_SHIFT			4
#define WF_REPEAT_MIN			1
#define WF_REPEAT_MAX			128
#define WF_S_REPEAT_MASK		GENMASK(1, 0)
#define WF_S_REPEAT_MIN			1
#define WF_S_REPEAT_MAX			8

#define HAP_WF_S1_REG(chip)		(chip->base + 0x60)
#define HAP_WF_SIGN_BIT			BIT(7)
#define HAP_WF_OVD_BIT			BIT(6)
#define HAP_WF_SAMP_MAX			GENMASK(5, 1)
#define HAP_WF_SAMPLE_LEN		8

#define HAP_PLAY_REG(chip)		(chip->base + 0x70)
#define PLAY_BIT			BIT(7)
#define PAUSE_BIT			BIT(0)

#define HAP_SEC_ACCESS_REG(chip)	(chip->base + 0xD0)

#define HAP_TEST2_REG(chip)		(chip->base + 0xE3)
#define HAP_EXT_PWM_DTEST_MASK		GENMASK(6, 4)
#define HAP_EXT_PWM_DTEST_SHIFT		4
#define PWM_MAX_DTEST_LINES		4
#define HAP_EXT_PWM_PEAK_DATA		0x7F
#define HAP_EXT_PWM_HALF_DUTY		50
#define HAP_EXT_PWM_FULL_DUTY		100
#define HAP_EXT_PWM_DATA_FACTOR		39

/* Other definitions */
#define HAP_BRAKE_PAT_LEN		4
#define HAP_WAVE_SAMP_LEN		8
#define NUM_WF_SET			4
#define HAP_WAVE_SAMP_SET_LEN		(HAP_WAVE_SAMP_LEN * NUM_WF_SET)
#define HAP_RATE_CFG_STEP_US		5
#define HAP_WAVE_PLAY_RATE_US_MIN	0
#define HAP_DEF_WAVE_PLAY_RATE_US	5715
#define HAP_WAVE_PLAY_RATE_US_MAX	20475
#define HAP_MAX_PLAY_TIME_MS		15000

enum hap_brake_pat {
	NO_BRAKE = 0,
	BRAKE_VMAX_4,
	BRAKE_VMAX_2,
	BRAKE_VMAX,
};

enum hap_auto_res_mode {
	HAP_AUTO_RES_NONE,
	HAP_AUTO_RES_ZXD,
	HAP_AUTO_RES_QWD,
	HAP_AUTO_RES_MAX_QWD,
	HAP_AUTO_RES_ZXD_EOP,
};

enum hap_pm660_auto_res_mode {
	HAP_PM660_AUTO_RES_ZXD,
	HAP_PM660_AUTO_RES_QWD,
};

/* high Z option lines */
enum hap_high_z {
	HAP_LRA_HIGH_Z_NONE, /* opt0 for PM660 */
	HAP_LRA_HIGH_Z_OPT1,
	HAP_LRA_HIGH_Z_OPT2,
	HAP_LRA_HIGH_Z_OPT3,
};

/* play modes */
enum hap_mode {
	HAP_DIRECT,
	HAP_BUFFER,
	HAP_AUDIO,
	HAP_PWM,
};

/* wave/sample repeat */
enum hap_rep_type {
	HAP_WAVE_REPEAT = 1,
	HAP_WAVE_SAMP_REPEAT,
};

/* status flags */
enum hap_status {
	AUTO_RESONANCE_ENABLED = BIT(0),
};

enum hap_play_control {
	HAP_STOP,
	HAP_PAUSE,
	HAP_PLAY,
};

/* pwm channel parameters */
struct pwm_param {
	struct pwm_device	*pwm_dev;
	u32			duty_us;
	u32			period_us;
};

/*
 *  hap_lra_ares_param - Haptic auto_resonance parameters
 *  @ lra_qwd_drive_duration - LRA QWD drive duration
 *  @ calibrate_at_eop - Calibrate at EOP
 *  @ lra_res_cal_period - LRA resonance calibration period
 *  @ auto_res_mode - auto resonace mode
 *  @ lra_high_z - high z option line
 */
struct hap_lra_ares_param {
	int				lra_qwd_drive_duration;
	int				calibrate_at_eop;
	enum hap_high_z			lra_high_z;
	u16				lra_res_cal_period;
	u8				auto_res_mode;
};

/*
 *  hap_chip - Haptics data structure
 *  @ pdev - platform device pointer
 *  @ regmap - regmap pointer
 *  @ bus_lock - spin lock for bus read/write
 *  @ play_lock - mutex lock for haptics play/enable control
 *  @ haptics_work - haptics worker
 *  @ stop_timer - hrtimer for stopping haptics
 *  @ auto_res_err_poll_timer - hrtimer for auto-resonance error
 *  @ base - base address
 *  @ play_irq - irq for play
 *  @ sc_irq - irq for short circuit
 *  @ pwm_data - pwm configuration
 *  @ ares_cfg - auto resonance configuration
 *  @ play_time_ms - play time set by the user in ms
 *  @ max_play_time_ms - max play time in ms
 *  @ vmax_mv - max voltage in mv
 *  @ ilim_ma - limiting current in ma
 *  @ sc_deb_cycles - short circuit debounce cycles
 *  @ wave_play_rate_us - play rate for waveform
 *  @ last_rate_cfg - Last rate config updated
 *  @ wave_rep_cnt - waveform repeat count
 *  @ wave_s_rep_cnt - waveform sample repeat count
 *  @ ext_pwm_freq_khz - external pwm frequency in KHz
 *  @ ext_pwm_dtest_line - DTEST line for external pwm
 *  @ status_flags - status
 *  @ play_mode - play mode
 *  @ act_type - actuator type
 *  @ wave_shape - waveform shape
 *  @ wave_samp_idx - wave sample id used to refer start of a sample set
 *  @ wave_samp - array of wave samples
 *  @ brake_pat - pattern for active breaking
 *  @ en_brake - brake state
 *  @ misc_clk_trim_error_reg - MISC clock trim error register if present
 *  @ clk_trim_error_code - MISC clock trim error code
 *  @ drive_period_code_max_limit - calculated drive period code with
      percentage variation on the higher side.
 *  @ drive_period_code_min_limit - calculated drive period code with
      percentage variation on the lower side
 *  @ drive_period_code_max_var_pct - maximum limit of percentage variation of
      drive period code
 *  @ drive_period_code_min_var_pct - minimum limit of percentage variation of
      drive period code
 *  @ last_sc_time - Last time short circuit was detected
 *  @ sc_count - counter to determine the duration of short circuit
      condition
 *  @ perm_disable - Flag to disable module permanently
 *  @ state - current state of haptics
 *  @ module_en - module enable status of haptics
 *  @ lra_auto_mode - Auto mode selection
 *  @ play_irq_en - Play interrupt enable status
 *  @ auto_res_err_recovery_hw - Enable auto resonance error recovery by HW
 */
struct hap_chip {
	struct platform_device		*pdev;
	struct regmap			*regmap;
	struct pmic_revid_data		*revid;
	struct led_classdev		cdev;
	spinlock_t			bus_lock;
	struct mutex			play_lock;
	struct mutex			param_lock;
	struct work_struct		haptics_work;
	struct hrtimer			stop_timer;
	struct hrtimer			auto_res_err_poll_timer;
	u16				base;
	int				play_irq;
	int				sc_irq;
	struct pwm_param		pwm_data;
	struct hap_lra_ares_param	ares_cfg;
	u32				play_time_ms;
	u32				max_play_time_ms;
	u32				vmax_mv;
	u8				ilim_ma;
	bool				overdrive;
	u32				sc_deb_cycles;
	u32				wave_play_rate_us;
	u16				last_rate_cfg;
	int				effect_index;
	u32				effect_max;
	u8				(*effect_arry)[HAP_WAVE_SAMP_LEN];
	u32				wave_rep_cnt;
	u32				wave_s_rep_cnt;
	u32				ext_pwm_freq_khz;
	u8				ext_pwm_dtest_line;
	u32				status_flags;
	enum hap_mode			play_mode;
	u8				act_type;
	u8				wave_shape;
	u8				wave_samp_idx;
	u32				wave_samp[HAP_WAVE_SAMP_SET_LEN];
	u32				brake_pat[HAP_BRAKE_PAT_LEN];
	bool				en_brake;
	u32				misc_clk_trim_error_reg;
	u8				clk_trim_error_code;
	u16				drive_period_code_max_limit;
	u16				drive_period_code_min_limit;
	u8				drive_period_code_max_var_pct;
	u8				drive_period_code_min_var_pct;
	ktime_t				last_sc_time;
	u8				sc_count;
	bool				perm_disable;
	atomic_t			state;
	bool				module_en;
	bool				lra_auto_mode;
	bool				play_irq_en;
	bool				auto_res_err_recovery_hw;
};

static int qpnp_haptics_parse_buffer_dt(struct hap_chip *chip);
static int qpnp_haptics_parse_pwm_dt(struct hap_chip *chip);

static int qpnp_haptics_read_reg(struct hap_chip *chip, u16 addr, u8 *val,
				int len)
{
	int rc;

	rc = regmap_bulk_read(chip->regmap, addr, val, len);
	if (rc < 0)
		pr_err("Error reading address: 0x%x - rc %d\n", addr, rc);

	return rc;
}

static inline bool is_secure(u16 addr)
{
	return ((addr & 0xFF) > 0xD0);
}

static int qpnp_haptics_write_reg(struct hap_chip *chip, u16 addr, u8 *val,
				int len)
{
	unsigned long flags;
	unsigned int unlock = 0xA5;
	int rc = 0, i;

	spin_lock_irqsave(&chip->bus_lock, flags);

	if (is_secure(addr)) {
		for (i = 0; i < len; i++) {
			rc = regmap_write(chip->regmap,
					HAP_SEC_ACCESS_REG(chip), unlock);
			if (rc < 0) {
				pr_err("Error writing unlock code - rc %d\n",
					rc);
				goto out;
			}

			rc = regmap_write(chip->regmap, addr + i, val[i]);
			if (rc < 0) {
				pr_err("Error writing address 0x%x - rc %d\n",
					addr + i, rc);
				goto out;
			}
		}
	} else {
		if (len > 1)
			rc = regmap_bulk_write(chip->regmap, addr, val, len);
		else
			rc = regmap_write(chip->regmap, addr, *val);
	}

	if (rc < 0)
		pr_err("Error writing address: 0x%x - rc %d\n", addr, rc);

out:
	spin_unlock_irqrestore(&chip->bus_lock, flags);
	return rc;
}

static int qpnp_haptics_masked_write_reg(struct hap_chip *chip, u16 addr,
					u8 mask, u8 val)
{
	unsigned long flags;
	unsigned int unlock = 0xA5;
	int rc;

	spin_lock_irqsave(&chip->bus_lock, flags);
	if (is_secure(addr)) {
		rc = regmap_write(chip->regmap, HAP_SEC_ACCESS_REG(chip),
				unlock);
		if (rc < 0) {
			pr_err("Error writing unlock code - rc %d\n", rc);
			goto out;
		}
	}

	rc = regmap_update_bits(chip->regmap, addr, mask, val);
	if (rc < 0)
		pr_err("Error writing address: 0x%x - rc %d\n", addr, rc);

	if (!rc)
		pr_debug("wrote to address 0x%x = 0x%x\n", addr, val);
out:
	spin_unlock_irqrestore(&chip->bus_lock, flags);
	return rc;
}

static bool is_sw_lra_auto_resonance_control(struct hap_chip *chip)
{
	if (chip->act_type != HAP_LRA)
		return false;

	if (chip->auto_res_err_recovery_hw)
		return false;

	/*
	 * For short pattern in auto mode, we use buffer mode and auto
	 * resonance is not needed.
	 */
	if (chip->lra_auto_mode && chip->play_mode == HAP_BUFFER)
		return false;

	return true;
}

#define HAPTICS_BACK_EMF_DELAY_US	20000
static int qpnp_haptics_auto_res_enable(struct hap_chip *chip, bool enable)
{
	int rc = 0;
	u32 delay_us = HAPTICS_BACK_EMF_DELAY_US;
	u8 val, auto_res_mode_qwd;

	if (chip->act_type != HAP_LRA)
		return 0;

	if (chip->revid->pmic_subtype == PM660_SUBTYPE)
		auto_res_mode_qwd = (chip->ares_cfg.auto_res_mode ==
						HAP_PM660_AUTO_RES_QWD);
	else
		auto_res_mode_qwd = (chip->ares_cfg.auto_res_mode ==
							HAP_AUTO_RES_QWD);

	/*
	 * For auto resonance detection to work properly, sufficient back-emf
	 * has to be generated. In general, back-emf takes some time to build
	 * up. When the auto resonance mode is chosen as QWD, high-z will be
	 * applied for every LRA cycle and hence there won't be enough back-emf
	 * at the start-up. Hence, the motor needs to vibrate for few LRA cycles
	 * after the PLAY bit is asserted. Enable the auto resonance after
	 * 'time_required_to_generate_back_emf_us' is completed.
	 */

	if (auto_res_mode_qwd && enable)
		usleep_range(delay_us, delay_us + 1);

	val = enable ? AUTO_RES_EN_BIT : 0;

	if (chip->revid->pmic_subtype == PM660_SUBTYPE)
		rc = qpnp_haptics_masked_write_reg(chip,
				HAP_AUTO_RES_CTRL_REG(chip),
				AUTO_RES_EN_BIT, val);
	else
		rc = qpnp_haptics_masked_write_reg(chip, HAP_TEST2_REG(chip),
				AUTO_RES_EN_BIT, val);
	if (rc < 0)
		return rc;

	if (enable)
		chip->status_flags |= AUTO_RESONANCE_ENABLED;
	else
		chip->status_flags &= ~AUTO_RESONANCE_ENABLED;

	pr_debug("auto_res %sabled\n", enable ? "en" : "dis");
	return rc;
}

static int qpnp_haptics_update_rate_cfg(struct hap_chip *chip, u16 play_rate)
{
	int rc;
	u8 val[2];

	if (chip->last_rate_cfg == play_rate) {
		pr_debug("Same rate_cfg %x\n", play_rate);
		return 0;
	}

	val[0] = play_rate & HAP_RATE_CFG1_MASK;
	val[1] = (play_rate >> HAP_RATE_CFG2_SHIFT) & HAP_RATE_CFG2_MASK;
	rc = qpnp_haptics_write_reg(chip, HAP_RATE_CFG1_REG(chip), val, 2);
	if (rc < 0)
		return rc;

	pr_debug("Play rate code 0x%x\n", play_rate);
	chip->last_rate_cfg = play_rate;
	return 0;
}

static void qpnp_haptics_update_lra_frequency(struct hap_chip *chip)
{
	u8 lra_auto_res[2], val;
	u32 play_rate_code;
	u16 rate_cfg;
	int rc;

	rc = qpnp_haptics_read_reg(chip, HAP_LRA_AUTO_RES_LO_REG(chip),
				lra_auto_res, 2);
	if (rc < 0) {
		pr_err("Error in reading LRA_AUTO_RES_LO/HI, rc=%d\n", rc);
		return;
	}

	play_rate_code =
		 (lra_auto_res[1] & 0xF0) << 4 | (lra_auto_res[0] & 0xFF);

	pr_debug("lra_auto_res_lo = 0x%x lra_auto_res_hi = 0x%x play_rate_code = 0x%x\n",
		lra_auto_res[0], lra_auto_res[1], play_rate_code);

	rc = qpnp_haptics_read_reg(chip, HAP_STATUS_1_REG(chip), &val, 1);
	if (rc < 0)
		return;

	/*
	 * If the drive period code read from AUTO_RES_LO and AUTO_RES_HI
	 * registers is more than the max limit percent variation or less
	 * than the min limit percent variation specified through DT, then
	 * auto-resonance is disabled.
	 */

	if ((val & AUTO_RES_ERROR_BIT) ||
		((play_rate_code <= chip->drive_period_code_min_limit) ||
		(play_rate_code >= chip->drive_period_code_max_limit))) {
		if (val & AUTO_RES_ERROR_BIT)
			pr_debug("Auto-resonance error %x\n", val);
		else
			pr_debug("play rate %x out of bounds [min: 0x%x, max: 0x%x]\n",
				play_rate_code,
				chip->drive_period_code_min_limit,
				chip->drive_period_code_max_limit);
		rc = qpnp_haptics_auto_res_enable(chip, false);
		if (rc < 0)
			pr_debug("Auto-resonance disable failed\n");
		return;
	}

	/*
	 * bits[7:4] of AUTO_RES_HI should be written to bits[3:0] of RATE_CFG2
	 */
	lra_auto_res[1] >>= 4;
	rate_cfg = lra_auto_res[1] << 8 | lra_auto_res[0];
	rc = qpnp_haptics_update_rate_cfg(chip, rate_cfg);
	if (rc < 0)
		pr_debug("Error in updating rate_cfg\n");
}

#define MAX_RETRIES	5
#define HAP_CYCLES	4
static bool is_haptics_idle(struct hap_chip *chip)
{
	unsigned long wait_time_us;
	int rc, i;
	u8 val;

	rc = qpnp_haptics_read_reg(chip, HAP_STATUS_1_REG(chip), &val, 1);
	if (rc < 0)
		return false;

	if (!(val & HAP_BUSY_BIT))
		return true;

	if (chip->play_time_ms <= 20)
		wait_time_us = chip->play_time_ms * 1000;
	else
		wait_time_us = chip->wave_play_rate_us * HAP_CYCLES;

	for (i = 0; i < MAX_RETRIES; i++) {
		/* wait for play_rate cycles */
		usleep_range(wait_time_us, wait_time_us + 1);

		if (chip->play_mode == HAP_DIRECT ||
				chip->play_mode == HAP_PWM)
			return true;

		rc = qpnp_haptics_read_reg(chip, HAP_STATUS_1_REG(chip), &val,
					1);
		if (rc < 0)
			return false;

		if (!(val & HAP_BUSY_BIT))
			return true;
	}

	if (i >= MAX_RETRIES && (val & HAP_BUSY_BIT)) {
		pr_debug("Haptics Busy after %d retries\n", i);
		return false;
	}

	return true;
}

static int qpnp_haptics_mod_enable(struct hap_chip *chip, bool enable)
{
	u8 val;
	int rc;

	if (chip->module_en == enable)
		return 0;

	if (!enable) {
		if (!is_haptics_idle(chip))
			pr_debug("Disabling module forcibly\n");
	}

	val = enable ? HAP_EN_BIT : 0;
	rc = qpnp_haptics_write_reg(chip, HAP_EN_CTL_REG(chip), &val, 1);
	if (rc < 0)
		return rc;

	chip->module_en = enable;
	return 0;
}

static int qpnp_haptics_play_control(struct hap_chip *chip,
					enum hap_play_control ctrl)
{
	u8 val;
	int rc;

	switch (ctrl) {
	case HAP_STOP:
		val = 0;
		break;
	case HAP_PAUSE:
		val = PAUSE_BIT;
		break;
	case HAP_PLAY:
		val = PLAY_BIT;
		break;
	default:
		return 0;
	}

	rc = qpnp_haptics_write_reg(chip, HAP_PLAY_REG(chip), &val, 1);
	if (rc < 0) {
		pr_err("Error in writing to PLAY_REG, rc=%d\n", rc);
		return rc;
	}

	pr_debug("haptics play ctrl: %d\n", ctrl);
	return rc;
}

#define AUTO_RES_ERR_POLL_TIME_NS	(20 * NSEC_PER_MSEC)
static int qpnp_haptics_play(struct hap_chip *chip, bool enable)
{
	int rc = 0, time_ms = chip->play_time_ms;

	if (chip->perm_disable && enable)
		return 0;

	mutex_lock(&chip->play_lock);

	if (enable) {
		if (chip->play_mode == HAP_PWM) {
			rc = pwm_enable(chip->pwm_data.pwm_dev);
			if (rc < 0) {
				pr_err("Error in enabling PWM, rc=%d\n", rc);
				goto out;
			}
		}

		rc = qpnp_haptics_auto_res_enable(chip, false);
		if (rc < 0) {
			pr_err("Error in disabling auto_res, rc=%d\n", rc);
			goto out;
		}

		rc = qpnp_haptics_mod_enable(chip, true);
		if (rc < 0) {
			pr_err("Error in enabling module, rc=%d\n", rc);
			goto out;
		}

		rc = qpnp_haptics_play_control(chip, HAP_PLAY);
		if (rc < 0) {
			pr_err("Error in enabling play, rc=%d\n", rc);
			goto out;
		}

		if (chip->play_mode != HAP_BUFFER) {
			hrtimer_start(&chip->stop_timer,
				ktime_set(time_ms / MSEC_PER_SEC,
				(time_ms % MSEC_PER_SEC) * NSEC_PER_MSEC),
				HRTIMER_MODE_REL);

			rc = qpnp_haptics_auto_res_enable(chip, true);
			if (rc < 0) {
				pr_err("Error in enabling auto_res, rc=%d\n", rc);
				goto out;
			}
		} else {
			hrtimer_start(&chip->stop_timer,
				ktime_set(40 / MSEC_PER_SEC,
				(time_ms % MSEC_PER_SEC) * NSEC_PER_MSEC),
				HRTIMER_MODE_REL);
		}

		if (is_sw_lra_auto_resonance_control(chip))
			hrtimer_start(&chip->auto_res_err_poll_timer,
				ktime_set(0, AUTO_RES_ERR_POLL_TIME_NS),
				HRTIMER_MODE_REL);
	} else {
		rc = qpnp_haptics_play_control(chip, HAP_STOP);
		if (rc < 0) {
			pr_err("Error in disabling play, rc=%d\n", rc);
			goto out;
		}

		rc = qpnp_haptics_mod_enable(chip, false);
		if (rc < 0) {
			pr_err("Error in disabling module, rc=%d\n", rc);
			goto out;
		}

		if (is_sw_lra_auto_resonance_control(chip)) {
			if (chip->status_flags & AUTO_RESONANCE_ENABLED)
				qpnp_haptics_update_lra_frequency(chip);
			hrtimer_cancel(&chip->auto_res_err_poll_timer);
		}

		if (chip->play_mode == HAP_PWM)
			pwm_disable(chip->pwm_data.pwm_dev);
	}

out:
	mutex_unlock(&chip->play_lock);
	return rc;
}

static void qpnp_haptics_work(struct work_struct *work)
{
	struct hap_chip *chip = container_of(work, struct hap_chip,
						haptics_work);
	int rc;
	bool enable;

	enable = atomic_read(&chip->state);
	pr_debug("state: %d\n", enable);
	rc = qpnp_haptics_play(chip, enable);
	if (rc < 0)
		pr_err("Error in %sing haptics, rc=%d\n",
			enable ? "play" : "stopp", rc);
}

static enum hrtimer_restart hap_stop_timer(struct hrtimer *timer)
{
	struct hap_chip *chip = container_of(timer, struct hap_chip,
					stop_timer);

	atomic_set(&chip->state, 0);
	schedule_work(&chip->haptics_work);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart hap_auto_res_err_poll_timer(struct hrtimer *timer)
{
	struct hap_chip *chip = container_of(timer, struct hap_chip,
					auto_res_err_poll_timer);

	if (!(chip->status_flags & AUTO_RESONANCE_ENABLED))
		return HRTIMER_NORESTART;

	qpnp_haptics_update_lra_frequency(chip);
	hrtimer_forward(&chip->auto_res_err_poll_timer, ktime_get(),
			ktime_set(0, AUTO_RES_ERR_POLL_TIME_NS));

	return HRTIMER_NORESTART;
}

static int qpnp_haptics_suspend(struct device *dev)
{
	struct hap_chip *chip = dev_get_drvdata(dev);
	int rc;

	rc = qpnp_haptics_play(chip, false);
	if (rc < 0)
		pr_err("Error in stopping haptics, rc=%d\n", rc);

	rc = qpnp_haptics_mod_enable(chip, false);
	if (rc < 0)
		pr_err("Error in disabling module, rc=%d\n", rc);

	return 0;
}

static int qpnp_haptics_wave_rep_config(struct hap_chip *chip,
					enum hap_rep_type type)
{
	int rc;
	u8 val = 0, mask = 0;

	if (type & HAP_WAVE_REPEAT) {
		if (chip->wave_rep_cnt < WF_REPEAT_MIN)
			chip->wave_rep_cnt = WF_REPEAT_MIN;
		else if (chip->wave_rep_cnt > WF_REPEAT_MAX)
			chip->wave_rep_cnt = WF_REPEAT_MAX;
		mask = WF_REPEAT_MASK;
		val = ilog2(chip->wave_rep_cnt) << WF_REPEAT_SHIFT;
	}

	if (type & HAP_WAVE_SAMP_REPEAT) {
		if (chip->wave_s_rep_cnt < WF_S_REPEAT_MIN)
			chip->wave_s_rep_cnt = WF_S_REPEAT_MIN;
		else if (chip->wave_s_rep_cnt > WF_S_REPEAT_MAX)
			chip->wave_s_rep_cnt = WF_S_REPEAT_MAX;
		mask |= WF_S_REPEAT_MASK;
		val |= ilog2(chip->wave_s_rep_cnt);
	}

	rc = qpnp_haptics_masked_write_reg(chip, HAP_WF_REPEAT_REG(chip),
			mask, val);
	return rc;
}

/* configuration api for buffer mode */
static int qpnp_haptics_buffer_config(struct hap_chip *chip, u32 *wave_samp,
				bool overdrive)
{
	u8 buf[HAP_WAVE_SAMP_LEN];
	u32 *ptr;
	int rc, i;

	if (wave_samp) {
		ptr = wave_samp;
	} else {
		if (chip->wave_samp_idx >= ARRAY_SIZE(chip->wave_samp)) {
			pr_err("Incorrect wave_samp_idx %d\n",
				chip->wave_samp_idx);
			return -EINVAL;
		}

		ptr = &chip->wave_samp[chip->wave_samp_idx];
	}

	/* Don't set override bit in waveform sample for PM660 */
	if (chip->revid->pmic_subtype == PM660_SUBTYPE)
		overdrive = false;

	/* Configure WAVE_SAMPLE1 to WAVE_SAMPLE8 register */
	for (i = 0; i < HAP_WAVE_SAMP_LEN; i++) {
		buf[i] = ptr[i];
		if (buf[i])
			buf[i] |= (overdrive ? HAP_WF_OVD_BIT : 0);
	}

	rc = qpnp_haptics_write_reg(chip, HAP_WF_S1_REG(chip), buf,
			HAP_WAVE_SAMP_LEN);
	return rc;
}

/* configuration api for pwm */
static int qpnp_haptics_pwm_config(struct hap_chip *chip)
{
	u8 val = 0;
	int rc;

	if (chip->ext_pwm_freq_khz == 0)
		return 0;

	/* Configure the EXTERNAL_PWM register */
	if (chip->ext_pwm_freq_khz <= EXT_PWM_FREQ_25_KHZ) {
		chip->ext_pwm_freq_khz = EXT_PWM_FREQ_25_KHZ;
		val = 0;
	} else if (chip->ext_pwm_freq_khz <= EXT_PWM_FREQ_50_KHZ) {
		chip->ext_pwm_freq_khz = EXT_PWM_FREQ_50_KHZ;
		val = 1;
	} else if (chip->ext_pwm_freq_khz <= EXT_PWM_FREQ_75_KHZ) {
		chip->ext_pwm_freq_khz = EXT_PWM_FREQ_75_KHZ;
		val = 2;
	} else {
		chip->ext_pwm_freq_khz = EXT_PWM_FREQ_100_KHZ;
		val = 3;
	}

	rc = qpnp_haptics_masked_write_reg(chip, HAP_EXT_PWM_REG(chip),
			EXT_PWM_FREQ_SEL_MASK, val);
	if (rc < 0)
		return rc;

	if (chip->ext_pwm_dtest_line < 0 ||
			chip->ext_pwm_dtest_line > PWM_MAX_DTEST_LINES) {
		pr_err("invalid dtest line\n");
		return -EINVAL;
	}

	if (chip->ext_pwm_dtest_line > 0) {
		/* disable auto res for PWM mode */
		val = chip->ext_pwm_dtest_line << HAP_EXT_PWM_DTEST_SHIFT;
		rc = qpnp_haptics_masked_write_reg(chip, HAP_TEST2_REG(chip),
			HAP_EXT_PWM_DTEST_MASK | AUTO_RES_EN_BIT, val);
		if (rc < 0)
			return rc;
	}

	rc = pwm_config(chip->pwm_data.pwm_dev,
			chip->pwm_data.duty_us * NSEC_PER_USEC,
			chip->pwm_data.period_us * NSEC_PER_USEC);
	if (rc < 0) {
		pr_err("pwm_config failed, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int qpnp_haptics_lra_auto_res_config(struct hap_chip *chip,
					struct hap_lra_ares_param *tmp_cfg)
{
	struct hap_lra_ares_param *ares_cfg;
	int rc;
	u8 val = 0, mask = 0;

	/* disable auto resonance for ERM */
	if (chip->act_type == HAP_ERM) {
		val = 0x00;
		rc = qpnp_haptics_write_reg(chip, HAP_LRA_AUTO_RES_REG(chip),
					&val, 1);
		return rc;
	}

	if (chip->auto_res_err_recovery_hw) {
		rc = qpnp_haptics_masked_write_reg(chip,
			HAP_AUTO_RES_CTRL_REG(chip),
			AUTO_RES_ERR_RECOVERY_BIT, AUTO_RES_ERR_RECOVERY_BIT);
		if (rc < 0)
			return rc;
	}

	if (tmp_cfg)
		ares_cfg = tmp_cfg;
	else
		ares_cfg = &chip->ares_cfg;

	if (ares_cfg->lra_res_cal_period < HAP_RES_CAL_PERIOD_MIN)
		ares_cfg->lra_res_cal_period = HAP_RES_CAL_PERIOD_MIN;

	if (chip->revid->pmic_subtype == PM660_SUBTYPE) {
		if (ares_cfg->lra_res_cal_period >
				HAP_PM660_RES_CAL_PERIOD_MAX)
			ares_cfg->lra_res_cal_period =
				HAP_PM660_RES_CAL_PERIOD_MAX;

		if (ares_cfg->auto_res_mode == HAP_PM660_AUTO_RES_QWD)
			ares_cfg->lra_res_cal_period = 0;

		if (ares_cfg->lra_res_cal_period)
			val = ilog2(ares_cfg->lra_res_cal_period /
					HAP_RES_CAL_PERIOD_MIN) + 1;
	} else {
		if (ares_cfg->lra_res_cal_period > HAP_RES_CAL_PERIOD_MAX)
			ares_cfg->lra_res_cal_period =
				HAP_RES_CAL_PERIOD_MAX;

		if (ares_cfg->lra_res_cal_period)
			val = ilog2(ares_cfg->lra_res_cal_period /
					HAP_RES_CAL_PERIOD_MIN);
	}

	if (chip->revid->pmic_subtype == PM660_SUBTYPE) {
		val |= ares_cfg->auto_res_mode << PM660_AUTO_RES_MODE_SHIFT;
		mask = PM660_AUTO_RES_MODE_BIT;
		val |= ares_cfg->lra_high_z << PM660_CAL_DURATION_SHIFT;
		mask |= PM660_CAL_DURATION_MASK;
		if (ares_cfg->lra_qwd_drive_duration != -EINVAL) {
			val |= ares_cfg->lra_qwd_drive_duration <<
				PM660_QWD_DRIVE_DURATION_SHIFT;
			mask |= PM660_QWD_DRIVE_DURATION_BIT;
		}
		if (ares_cfg->calibrate_at_eop != -EINVAL) {
			val |= ares_cfg->calibrate_at_eop <<
				PM660_CAL_EOP_SHIFT;
			mask |= PM660_CAL_EOP_BIT;
		}
		mask |= PM660_LRA_RES_CAL_MASK;
	} else {
		val |= (ares_cfg->auto_res_mode << LRA_AUTO_RES_MODE_SHIFT);
		val |= (ares_cfg->lra_high_z << LRA_HIGH_Z_SHIFT);
		mask = LRA_AUTO_RES_MODE_MASK | LRA_HIGH_Z_MASK |
			LRA_RES_CAL_MASK;
	}

	pr_debug("mode: %d hi_z period: %d cal_period: %d\n",
		ares_cfg->auto_res_mode, ares_cfg->lra_high_z,
		ares_cfg->lra_res_cal_period);

	rc = qpnp_haptics_masked_write_reg(chip, HAP_LRA_AUTO_RES_REG(chip),
			mask, val);
	return rc;
}

/* configuration api for play mode */
static int qpnp_haptics_play_mode_config(struct hap_chip *chip)
{
	u8 val = 0;
	int rc;

	if (!is_haptics_idle(chip))
		return -EBUSY;

	val = chip->play_mode << HAP_WF_SOURCE_SHIFT;
	rc = qpnp_haptics_masked_write_reg(chip, HAP_SEL_REG(chip),
			HAP_WF_SOURCE_MASK, val);
	if (!rc) {
		if (chip->play_mode == HAP_BUFFER && !chip->play_irq_en) {
			enable_irq(chip->play_irq);
			chip->play_irq_en = true;
		} else if (chip->play_mode != HAP_BUFFER && chip->play_irq_en) {
			disable_irq(chip->play_irq);
			chip->play_irq_en = false;
		}
	}
	return rc;
}

/* configuration api for max voltage */
static int qpnp_haptics_vmax_config(struct hap_chip *chip, int vmax_mv,
				bool overdrive)
{
	u8 val = 0;
	int rc;

	if (vmax_mv < 0)
		return -EINVAL;

	/* Allow setting override bit in VMAX_CFG only for PM660 */
	if (chip->revid->pmic_subtype != PM660_SUBTYPE)
		overdrive = false;

	if (vmax_mv < HAP_VMAX_MIN_MV)
		vmax_mv = HAP_VMAX_MIN_MV;
	else if (vmax_mv > HAP_VMAX_MAX_MV)
		vmax_mv = HAP_VMAX_MAX_MV;

	val = DIV_ROUND_CLOSEST(vmax_mv, HAP_VMAX_MIN_MV);
	val <<= HAP_VMAX_SHIFT;
	if (overdrive)
		val |= HAP_VMAX_OVD_BIT;

	rc = qpnp_haptics_masked_write_reg(chip, HAP_VMAX_CFG_REG(chip),
			HAP_VMAX_MASK | HAP_VMAX_OVD_BIT, val);
	return rc;
}

/* configuration api for ilim */
static int qpnp_haptics_ilim_config(struct hap_chip *chip)
{
	int rc;

	if (chip->ilim_ma < HAP_ILIM_400_MA)
		chip->ilim_ma = HAP_ILIM_400_MA;
	else if (chip->ilim_ma > HAP_ILIM_800_MA)
		chip->ilim_ma = HAP_ILIM_800_MA;

	rc = qpnp_haptics_masked_write_reg(chip, HAP_ILIM_CFG_REG(chip),
			HAP_ILIM_SEL_MASK, chip->ilim_ma);
	return rc;
}

/* configuration api for short circuit debounce */
static int qpnp_haptics_sc_deb_config(struct hap_chip *chip)
{
	u8 val = 0;
	int rc;

	if (chip->sc_deb_cycles < HAP_SC_DEB_CYCLES_MIN)
		chip->sc_deb_cycles = HAP_SC_DEB_CYCLES_MIN;
	else if (chip->sc_deb_cycles > HAP_SC_DEB_CYCLES_MAX)
		chip->sc_deb_cycles = HAP_SC_DEB_CYCLES_MAX;

	if (chip->sc_deb_cycles != HAP_SC_DEB_CYCLES_MIN)
		val = ilog2(chip->sc_deb_cycles /
			HAP_DEF_SC_DEB_CYCLES) + 1;
	else
		val = HAP_SC_DEB_CYCLES_MIN;

	rc = qpnp_haptics_masked_write_reg(chip, HAP_SC_DEB_REG(chip),
			HAP_SC_DEB_MASK, val);

	return rc;
}

static int qpnp_haptics_brake_config(struct hap_chip *chip, u32 *brake_pat)
{
	int rc, i;
	u32 temp, *ptr;
	u8 val;

	/* Configure BRAKE register */
	rc = qpnp_haptics_masked_write_reg(chip, HAP_EN_CTL2_REG(chip),
			BRAKE_EN_BIT, (u8)chip->en_brake);
	if (rc < 0)
		return rc;

	/* If braking is not enabled, skip configuring brake pattern */
	if (!chip->en_brake)
		return 0;

	if (!brake_pat)
		ptr = chip->brake_pat;
	else
		ptr = brake_pat;

	for (i = HAP_BRAKE_PAT_LEN - 1, val = 0; i >= 0; i--) {
		ptr[i] &= HAP_BRAKE_PAT_MASK;
		temp = i << 1;
		val |= ptr[i] << temp;
	}

	rc = qpnp_haptics_write_reg(chip, HAP_BRAKE_REG(chip), &val, 1);
	if (rc < 0)
		return rc;

	return 0;
}

static int qpnp_haptics_auto_mode_config(struct hap_chip *chip, int time_ms)
{
	struct hap_lra_ares_param ares_cfg;
	enum hap_mode old_play_mode;
	u8 old_ares_mode;
	u32 brake_pat[HAP_BRAKE_PAT_LEN] = {0};
	u32 wave_samp[HAP_WAVE_SAMP_LEN] = {0};
	int rc, vmax_mv;

	if (!chip->lra_auto_mode)
		return false;

	/* For now, this is for LRA only */
	if (chip->act_type == HAP_ERM)
		return 0;

	old_ares_mode = chip->ares_cfg.auto_res_mode;
	old_play_mode = chip->play_mode;
	pr_debug("auto_mode, time_ms: %d\n", time_ms);
	if (time_ms <= 20) {
		int index;
		index = time_ms / 5;

		/*
		 * only change pattern for different vibration cycle.
		 * */
		if ( chip->effect_max) {
			int i = 0;
			if (index != chip->effect_index) {
				if (index >= chip->effect_max) {
						index = chip->effect_max - 1;
				}
				chip->effect_index = index;
				for (i = 0; i < HAP_WAVE_SAMP_LEN; i++) {
					wave_samp[i] = (u32)(chip->effect_arry[index][i]);
				}
				rc = qpnp_haptics_buffer_config(chip, wave_samp, chip->overdrive);
				if (rc < 0) {
					pr_err("Error in configuring buffer mode %d\n",
						rc);
					return rc;
				}
			}
		} else
		{
			wave_samp[0] = HAP_WF_SAMP_MAX;
			wave_samp[1] = HAP_WF_SAMP_MAX;
			rc = qpnp_haptics_buffer_config(chip, wave_samp, chip->overdrive);
			if (rc < 0) {
				pr_err("Error in configuring buffer mode %d\n",
					rc);
				return rc;
			}
		}

		rc = qpnp_haptics_wave_rep_config(chip,
			HAP_WAVE_REPEAT | HAP_WAVE_SAMP_REPEAT);
		if (rc < 0) {
			pr_err("Error in configuring wave_rep config %d\n",
				rc);
			return rc;
		}

		ares_cfg.lra_high_z = HAP_LRA_HIGH_Z_OPT1;
		ares_cfg.lra_res_cal_period = HAP_RES_CAL_PERIOD_MIN;
		if (chip->revid->pmic_subtype == PM660_SUBTYPE) {
			ares_cfg.auto_res_mode = HAP_PM660_AUTO_RES_QWD;
			ares_cfg.lra_qwd_drive_duration = 0;
			ares_cfg.calibrate_at_eop = 0;
		} else {
			ares_cfg.auto_res_mode = HAP_AUTO_RES_QWD;
			ares_cfg.lra_qwd_drive_duration = -EINVAL;
			ares_cfg.calibrate_at_eop = -EINVAL;
		}

		vmax_mv = HAP_VMAX_MAX_MV;
		rc = qpnp_haptics_vmax_config(chip, vmax_mv, true);
		if (rc < 0)
			return rc;

		rc = qpnp_haptics_brake_config(chip, brake_pat);
		if (rc < 0)
			return rc;

		/* enable play_irq for buffer mode */
		if (chip->play_irq >= 0 && !chip->play_irq_en) {
			enable_irq(chip->play_irq);
			chip->play_irq_en = true;
		}

		chip->play_mode = HAP_BUFFER;
		chip->wave_shape = HAP_WAVE_SINE;
	} else {
		/* long pattern */
		ares_cfg.lra_high_z = HAP_LRA_HIGH_Z_OPT1;
		if (chip->revid->pmic_subtype == PM660_SUBTYPE) {
			ares_cfg.auto_res_mode = HAP_PM660_AUTO_RES_ZXD;
			ares_cfg.lra_res_cal_period =
				HAP_PM660_RES_CAL_PERIOD_MAX;
			ares_cfg.lra_qwd_drive_duration = 0;
			ares_cfg.calibrate_at_eop = 1;
		} else {
			ares_cfg.auto_res_mode = HAP_AUTO_RES_QWD;
			ares_cfg.lra_res_cal_period = HAP_RES_CAL_PERIOD_MIN;
			ares_cfg.lra_qwd_drive_duration = -EINVAL;
			ares_cfg.calibrate_at_eop = -EINVAL;
		}

		vmax_mv = chip->vmax_mv;
		rc = qpnp_haptics_vmax_config(chip, vmax_mv, false);
		if (rc < 0)
			return rc;

		rc = qpnp_haptics_brake_config(chip, brake_pat);
		if (rc < 0)
			return rc;

		/* enable play_irq for direct mode */
		if (chip->play_irq >= 0 && chip->play_irq_en) {
			disable_irq(chip->play_irq);
			chip->play_irq_en = false;
		}

		chip->play_mode = HAP_DIRECT;
		chip->wave_shape = HAP_WAVE_SINE;
	}

	chip->ares_cfg.auto_res_mode = ares_cfg.auto_res_mode;
	rc = qpnp_haptics_lra_auto_res_config(chip, &ares_cfg);
	if (rc < 0) {
		chip->ares_cfg.auto_res_mode = old_ares_mode;
		return rc;
	}

	rc = qpnp_haptics_play_mode_config(chip);
	if (rc < 0) {
		chip->play_mode = old_play_mode;
		return rc;
	}

	rc = qpnp_haptics_masked_write_reg(chip, HAP_CFG2_REG(chip),
			HAP_LRA_RES_TYPE_MASK, chip->wave_shape);
	if (rc < 0)
		return rc;

	return 0;
}

static irqreturn_t qpnp_haptics_play_irq_handler(int irq, void *data)
{
	struct hap_chip *chip = data;
	int rc;

	if (chip->play_mode != HAP_BUFFER)
		goto irq_handled;

	if (chip->wave_samp[chip->wave_samp_idx + HAP_WAVE_SAMP_LEN] > 0) {
		chip->wave_samp_idx += HAP_WAVE_SAMP_LEN;
		if (chip->wave_samp_idx >= ARRAY_SIZE(chip->wave_samp)) {
			pr_debug("Samples over\n");
			/* fall through to stop playing */
		} else {
			pr_debug("moving to next sample set %d\n",
				chip->wave_samp_idx);

			rc = qpnp_haptics_buffer_config(chip, NULL, false);
			if (rc < 0) {
				pr_err("Error in configuring buffer, rc=%d\n",
					rc);
				goto irq_handled;
			}

			/*
			 * Moving to next set of wave sample. No need to stop
			 * or change the play control. Just return.
			 */
			goto irq_handled;
		}
	}

	rc = qpnp_haptics_play_control(chip, HAP_STOP);
	if (rc < 0) {
		pr_err("Error in disabling play, rc=%d\n", rc);
		goto irq_handled;
	}
	chip->wave_samp_idx = 0;

irq_handled:
	return IRQ_HANDLED;
}

#define SC_MAX_COUNT		5
#define SC_COUNT_RST_DELAY_US	1000000
static irqreturn_t qpnp_haptics_sc_irq_handler(int irq, void *data)
{
	struct hap_chip *chip = data;
	int rc;
	u8 val;
	s64 sc_delta_time_us;
	ktime_t temp;

	rc = qpnp_haptics_read_reg(chip, HAP_STATUS_1_REG(chip), &val, 1);
	if (rc < 0)
		goto irq_handled;

	if (!(val & SC_FLAG_BIT)) {
		chip->sc_count = 0;
		goto irq_handled;
	}

	pr_debug("SC irq fired\n");
	temp = ktime_get();
	sc_delta_time_us = ktime_us_delta(temp, chip->last_sc_time);
	chip->last_sc_time = temp;

	if (sc_delta_time_us > SC_COUNT_RST_DELAY_US)
		chip->sc_count = 0;
	else
		chip->sc_count++;

	val = SC_CLR_BIT;
	rc = qpnp_haptics_write_reg(chip, HAP_SC_CLR_REG(chip), &val, 1);
	if (rc < 0) {
		pr_err("Error in writing to SC_CLR_REG, rc=%d\n", rc);
		goto irq_handled;
	}

	/* Permanently disable module if SC condition persists */
	if (chip->sc_count > SC_MAX_COUNT) {
		pr_crit("SC persists, permanently disabling haptics\n");
		rc = qpnp_haptics_mod_enable(chip, false);
		if (rc < 0) {
			pr_err("Error in disabling module, rc=%d\n", rc);
			goto irq_handled;
		}
		chip->perm_disable = true;
	}

irq_handled:
	return IRQ_HANDLED;
}

/* All sysfs show/store functions below */

#define HAP_STR_SIZE	128
static int parse_string(const char *in_buf, char *out_buf)
{
	int i;

	if (snprintf(out_buf, HAP_STR_SIZE, "%s", in_buf) > HAP_STR_SIZE)
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

static ssize_t qpnp_haptics_show_state(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->module_en);
}

static ssize_t qpnp_haptics_store_state(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	/* At present, nothing to do with setting state */
	return count;
}

static ssize_t qpnp_haptics_show_duration(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	ktime_t time_rem;
	s64 time_us = 0;

	if (hrtimer_active(&chip->stop_timer)) {
		time_rem = hrtimer_get_remaining(&chip->stop_timer);
		time_us = ktime_to_us(time_rem);
	}

	return snprintf(buf, PAGE_SIZE, "%lld\n", time_us / 1000);
	return 0;
}

static ssize_t qpnp_haptics_store_duration(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	u32 val;
	int rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;

	if (val > chip->max_play_time_ms)
		return -EINVAL;

	mutex_lock(&chip->param_lock);
	rc = qpnp_haptics_auto_mode_config(chip, val);
	if (rc < 0) {
		pr_err("Unable to do auto mode config\n");
		mutex_unlock(&chip->param_lock);
		return rc;
	}

	chip->play_time_ms = val;
	mutex_unlock(&chip->param_lock);

	return count;
}

static ssize_t qpnp_haptics_store_overdrive(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	u32 val;
	int rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	/* setting 0 on duration is NOP for now */
	if (val){
		chip->overdrive = true;
	} else {
		chip->overdrive = false;
	}
	return count;
}

static ssize_t qpnp_haptics_show_overdrive(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);

	return snprintf(buf, PAGE_SIZE, "%u\n", chip->overdrive);
	return 0;
}

static ssize_t qpnp_haptics_show_activate(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t qpnp_haptics_store_activate(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	u32 val;
	int rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val != 0 && val != 1)
		return count;

	if (val) {
		hrtimer_cancel(&chip->stop_timer);
		if (is_sw_lra_auto_resonance_control(chip))
			hrtimer_cancel(&chip->auto_res_err_poll_timer);
		cancel_work_sync(&chip->haptics_work);

		atomic_set(&chip->state, 1);
		schedule_work(&chip->haptics_work);
	} else {
		rc = qpnp_haptics_mod_enable(chip, false);
		if (rc < 0) {
			pr_err("Error in disabling module, rc=%d\n", rc);
			return rc;
		}
	}

	return count;
}

static ssize_t qpnp_haptics_show_play_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	char *str;

	if (chip->play_mode == HAP_BUFFER)
		str = "buffer";
	else if (chip->play_mode == HAP_DIRECT)
		str = "direct";
	else if (chip->play_mode == HAP_AUDIO)
		str = "audio";
	else if (chip->play_mode == HAP_PWM)
		str = "pwm";
	else
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%s\n", str);
}

static ssize_t qpnp_haptics_store_play_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	char str[HAP_STR_SIZE + 1];
	int rc = 0, temp, old_mode;

	rc = parse_string(buf, str);
	if (rc < 0)
		return rc;

	if (strcmp(str, "buffer") == 0)
		temp = HAP_BUFFER;
	else if (strcmp(str, "direct") == 0)
		temp = HAP_DIRECT;
	else if (strcmp(str, "audio") == 0)
		temp = HAP_AUDIO;
	else if (strcmp(str, "pwm") == 0)
		temp = HAP_PWM;
	else
		return -EINVAL;

	if (temp == chip->play_mode)
		return count;

	if (temp == HAP_BUFFER) {
		rc = qpnp_haptics_parse_buffer_dt(chip);
		if (!rc) {
			rc = qpnp_haptics_wave_rep_config(chip,
				HAP_WAVE_REPEAT | HAP_WAVE_SAMP_REPEAT);
			if (rc < 0) {
				pr_err("Error in configuring wave_rep config %d\n",
					rc);
				return rc;
			}
		}

		rc = qpnp_haptics_buffer_config(chip, NULL, true);
	} else if (temp == HAP_PWM) {
		rc = qpnp_haptics_parse_pwm_dt(chip);
		if (!rc)
			rc = qpnp_haptics_pwm_config(chip);
	}

	if (rc < 0)
		return rc;

	rc = qpnp_haptics_mod_enable(chip, false);
	if (rc < 0)
		return rc;

	old_mode = chip->play_mode;
	chip->play_mode = temp;
	rc = qpnp_haptics_play_mode_config(chip);
	if (rc < 0) {
		chip->play_mode = old_mode;
		return rc;
	}

	if (chip->play_mode == HAP_AUDIO) {
		rc = qpnp_haptics_mod_enable(chip, true);
		if (rc < 0) {
			chip->play_mode = old_mode;
			return rc;
		}
	}

	return count;
}

static ssize_t qpnp_haptics_show_wf_samp(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	char str[HAP_STR_SIZE + 1];
	char *ptr = str;
	int i, len = 0;

	for (i = 0; i < ARRAY_SIZE(chip->wave_samp); i++) {
		len = scnprintf(ptr, HAP_STR_SIZE, "%x ", chip->wave_samp[i]);
		ptr += len;
	}
	ptr[len] = '\0';

	return snprintf(buf, PAGE_SIZE, "%s\n", str);
}

static ssize_t qpnp_haptics_store_wf_samp(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	u8 samp[HAP_WAVE_SAMP_SET_LEN] = {0};
	int bytes_read, rc;
	unsigned int data, pos = 0, i = 0;

	while (pos < count && i < ARRAY_SIZE(samp) &&
		sscanf(buf + pos, "%x%n", &data, &bytes_read) == 1) {
		/* bit 0 is not used in WF_Sx */
		samp[i++] = data & GENMASK(7, 1);
		pos += bytes_read;
	}

	for (i = 0; i < ARRAY_SIZE(chip->wave_samp); i++)
		chip->wave_samp[i] = samp[i];

	rc = qpnp_haptics_buffer_config(chip, NULL, false);
	if (rc < 0) {
		pr_err("Error in configuring buffer mode %d\n", rc);
		return rc;
	}

	return count;
}

static ssize_t qpnp_haptics_show_effect_samp(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	char str[HAP_STR_SIZE + 1];
	char *ptr = str;
	int i, len = 0;
    pr_err("%s---effect_index=%d\n", __func__, chip->effect_index);
	if (chip->effect_index == -1){
        pr_err("%s, chip->effect_index == -1\n", __func__);
		return 0;
    }
    pr_err("%s, HAP_WAVE_SAMP_LEN=%d\n", __func__, HAP_WAVE_SAMP_LEN);
	for (i = 0; i < HAP_WAVE_SAMP_LEN; i++) {
		len = scnprintf(ptr, HAP_STR_SIZE, "%x ", chip->effect_arry[chip->effect_index][i]);
		ptr += len;
	}
	ptr[len] = '\0';

	return snprintf(buf, PAGE_SIZE, "%s\n", str);
}

static ssize_t qpnp_haptics_store_effect_samp(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	int bytes_read, rc;
	unsigned int data, pos = 0, i = 0;
	u32 wave_samp[HAP_WAVE_SAMP_LEN] = {0};
	bytes_read = 0;

    pr_err("%s---effect_index=%d\n", __func__, chip->effect_index);
	if (chip->effect_index == -1)
		return 0;

	while (pos < count && i < HAP_WAVE_SAMP_LEN &&
		sscanf(buf + pos, "%x%n", &data, &bytes_read) == 1) {
        pr_err("%s, while-loop\n", __func__);
		/* bit 0 is not used in WF_Sx */
		wave_samp[i] = data;
		chip->effect_arry[chip->effect_index][i++] = data;
		pos += bytes_read;
	}

	for (i = pos; i < HAP_WAVE_SAMP_LEN; i++)
		chip->effect_arry[chip->effect_index][i++] = 0;

	rc = qpnp_haptics_buffer_config(chip, wave_samp, chip->overdrive);
	if (rc < 0) {
		pr_err("Error in configuring buffer mode %d\n", rc);
		return rc;
	}

	return count;
}

static ssize_t qpnp_haptics_show_effect_max(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	return snprintf(buf, PAGE_SIZE, "%u\n", chip->effect_max);
}

static ssize_t qpnp_haptics_store_effect_max(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);

	if (sscanf(buf, " %u", &chip->effect_max) != 1)
			return -EINVAL;
	return count;
}



static ssize_t qpnp_haptics_show_wf_rep_count(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->wave_rep_cnt);
}

static ssize_t qpnp_haptics_store_wf_rep_count(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	int data, rc, old_wave_rep_cnt;

	rc = kstrtoint(buf, 10, &data);
	if (rc < 0)
		return rc;

	old_wave_rep_cnt = chip->wave_rep_cnt;
	chip->wave_rep_cnt = data;
	rc = qpnp_haptics_wave_rep_config(chip, HAP_WAVE_REPEAT);
	if (rc < 0) {
		chip->wave_rep_cnt = old_wave_rep_cnt;
		return rc;
	}

	return count;
}

static ssize_t qpnp_haptics_show_wf_s_rep_count(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->wave_s_rep_cnt);
}

static ssize_t qpnp_haptics_store_wf_s_rep_count(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	int data, rc, old_wave_s_rep_cnt;

	rc = kstrtoint(buf, 10, &data);
	if (rc < 0)
		return rc;

	old_wave_s_rep_cnt = chip->wave_s_rep_cnt;
	chip->wave_s_rep_cnt = data;
	rc = qpnp_haptics_wave_rep_config(chip, HAP_WAVE_SAMP_REPEAT);
	if (rc < 0) {
		chip->wave_s_rep_cnt = old_wave_s_rep_cnt;
		return rc;
	}

	return count;
}

static ssize_t qpnp_haptics_show_vmax(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->vmax_mv);
}

static ssize_t qpnp_haptics_store_vmax(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	int data, rc, old_vmax_mv;

	rc = kstrtoint(buf, 10, &data);
	if (rc < 0)
		return rc;

	old_vmax_mv = chip->vmax_mv;
	chip->vmax_mv = data;
	rc = qpnp_haptics_vmax_config(chip, chip->vmax_mv, false);
	if (rc < 0) {
		chip->vmax_mv = old_vmax_mv;
		return rc;
	}

	return count;
}

static ssize_t qpnp_haptics_show_lra_auto_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->lra_auto_mode);
}

static ssize_t qpnp_haptics_store_lra_auto_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct hap_chip *chip = container_of(cdev, struct hap_chip, cdev);
	int rc, data;

	rc = kstrtoint(buf, 10, &data);
	if (rc < 0)
		return rc;

	if (data != 0 && data != 1)
		return count;

	chip->lra_auto_mode = !!data;
	return count;
}

static struct device_attribute qpnp_haptics_attrs[] = {
	__ATTR(state, 0664, qpnp_haptics_show_state, qpnp_haptics_store_state),
	__ATTR(duration, 0664, qpnp_haptics_show_duration,
		qpnp_haptics_store_duration),
	__ATTR(overdrive, 0664, qpnp_haptics_show_overdrive,
		qpnp_haptics_store_overdrive),
	__ATTR(activate, 0664, qpnp_haptics_show_activate,
		qpnp_haptics_store_activate),
	__ATTR(play_mode, 0664, qpnp_haptics_show_play_mode,
		qpnp_haptics_store_play_mode),
	__ATTR(wf_samp, 0664, qpnp_haptics_show_wf_samp,
		qpnp_haptics_store_wf_samp),
	__ATTR(effect_samp, 0664, qpnp_haptics_show_effect_samp,
		qpnp_haptics_store_effect_samp),
	__ATTR(effect_max, 0664, qpnp_haptics_show_effect_max,
		qpnp_haptics_store_effect_max),
	__ATTR(wf_rep_count, 0664, qpnp_haptics_show_wf_rep_count,
		qpnp_haptics_store_wf_rep_count),
	__ATTR(wf_s_rep_count, 0664, qpnp_haptics_show_wf_s_rep_count,
		qpnp_haptics_store_wf_s_rep_count),
	__ATTR(vmax_mv, 0664, qpnp_haptics_show_vmax, qpnp_haptics_store_vmax),
	__ATTR(lra_auto_mode, 0664, qpnp_haptics_show_lra_auto_mode,
		qpnp_haptics_store_lra_auto_mode),
};

/* Dummy functions for brightness */
static
enum led_brightness qpnp_haptics_brightness_get(struct led_classdev *cdev)
{
	return 0;
}

static void qpnp_haptics_brightness_set(struct led_classdev *cdev,
					enum led_brightness level)
{
}

static int qpnp_haptics_config(struct hap_chip *chip)
{
	u8 rc_clk_err_deci_pct;
	u16 play_rate = 0;
	int rc;

	/* Configure the CFG1 register for actuator type */
	rc = qpnp_haptics_masked_write_reg(chip, HAP_CFG1_REG(chip),
			HAP_ACT_TYPE_MASK, chip->act_type);
	if (rc < 0)
		return rc;

	/* Configure auto resonance parameters */
	rc = qpnp_haptics_lra_auto_res_config(chip, NULL);
	if (rc < 0)
		return rc;

	/* Configure the PLAY MODE register */
	rc = qpnp_haptics_play_mode_config(chip);
	if (rc < 0)
		return rc;

	/* Configure the VMAX register */
	rc = qpnp_haptics_vmax_config(chip, chip->vmax_mv, false);
	if (rc < 0)
		return rc;

	/* Configure the ILIM register */
	rc = qpnp_haptics_ilim_config(chip);
	if (rc < 0)
		return rc;

	/* Configure the short circuit debounce register */
	rc = qpnp_haptics_sc_deb_config(chip);
	if (rc < 0)
		return rc;

	/* Configure the WAVE SHAPE register */
	rc = qpnp_haptics_masked_write_reg(chip, HAP_CFG2_REG(chip),
			HAP_LRA_RES_TYPE_MASK, chip->wave_shape);
	if (rc < 0)
		return rc;

	play_rate = chip->wave_play_rate_us / HAP_RATE_CFG_STEP_US;

	/*
	 * The frequency of 19.2 MHz RC clock is subject to variation. Currently
	 * some PMI chips have MISC_TRIM_ERROR_RC19P2_CLK register present in
	 * MISC peripheral. This register holds the trim error of RC clock.
	 */
	if (chip->act_type == HAP_LRA && chip->misc_clk_trim_error_reg) {
		/*
		 * Error is available in bits[3:0] and each LSB is 0.7%.
		 * Bit 7 is the sign bit for error code. If it is set, then a
		 * negative error correction needs to be made. Otherwise, a
		 * positive error correction needs to be made.
		 */
		rc_clk_err_deci_pct = (chip->clk_trim_error_code & 0x0F) * 7;
		if (chip->clk_trim_error_code & BIT(7))
			play_rate = (play_rate *
					(1000 - rc_clk_err_deci_pct)) / 1000;
		else
			play_rate = (play_rate *
					(1000 + rc_clk_err_deci_pct)) / 1000;

		pr_debug("TRIM register = 0x%x, play_rate=%d\n",
			chip->clk_trim_error_code, play_rate);
	}

	/*
	 * Configure RATE_CFG1 and RATE_CFG2 registers.
	 * Note: For ERM these registers act as play rate and
	 * for LRA these represent resonance period
	 */
	rc = qpnp_haptics_update_rate_cfg(chip, play_rate);
	if (chip->act_type == HAP_LRA) {
		chip->drive_period_code_max_limit = (play_rate *
			(100 + chip->drive_period_code_max_var_pct)) / 100;
		chip->drive_period_code_min_limit = (play_rate *
			(100 - chip->drive_period_code_min_var_pct)) / 100;
		pr_debug("Drive period code max limit %x min limit %x\n",
			chip->drive_period_code_max_limit,
			chip->drive_period_code_min_limit);
	}

	rc = qpnp_haptics_brake_config(chip, NULL);
	if (rc < 0)
		return rc;

	if (chip->play_mode == HAP_BUFFER) {
		rc = qpnp_haptics_wave_rep_config(chip,
			HAP_WAVE_REPEAT | HAP_WAVE_SAMP_REPEAT);
		if (rc < 0)
			return rc;

		rc = qpnp_haptics_buffer_config(chip, NULL, false);
	} else if (chip->play_mode == HAP_PWM) {
		rc = qpnp_haptics_pwm_config(chip);
	} else if (chip->play_mode == HAP_AUDIO) {
		rc = qpnp_haptics_mod_enable(chip, true);
	}

	if (rc < 0)
		return rc;

	/* setup play irq */
	if (chip->play_irq >= 0) {
		rc = devm_request_threaded_irq(&chip->pdev->dev, chip->play_irq,
			NULL, qpnp_haptics_play_irq_handler, IRQF_ONESHOT,
			"haptics_play_irq", chip);
		if (rc < 0) {
			pr_err("Unable to request play(%d) IRQ(err:%d)\n",
				chip->play_irq, rc);
			return rc;
		}

		/* use play_irq only for buffer mode */
		if (chip->play_mode != HAP_BUFFER) {
			disable_irq(chip->play_irq);
			chip->play_irq_en = false;
		}
	}

	/* setup short circuit irq */
	if (chip->sc_irq >= 0) {
		rc = devm_request_threaded_irq(&chip->pdev->dev, chip->sc_irq,
			NULL, qpnp_haptics_sc_irq_handler, IRQF_ONESHOT,
			"haptics_sc_irq", chip);
		if (rc < 0) {
			pr_err("Unable to request sc(%d) IRQ(err:%d)\n",
				chip->sc_irq, rc);
			return rc;
		}
	}

	return rc;
}

static int qpnp_haptics_parse_buffer_dt(struct hap_chip *chip)
{
	struct device_node *node = chip->pdev->dev.of_node;
	u32 temp;
	int rc, i, wf_samp_len;
	struct property *prop;

	if (chip->wave_rep_cnt > 0 || chip->wave_s_rep_cnt > 0)
		return 0;


	/*
	 * brake_pat_index = -1 to make sure brake_pat will be changed in the first time.
	 * brake_pat_max = 0 to make sure disable changing brake_pattern.
	 * */
	chip->effect_index = -1;
	chip->effect_max = 0;
	rc = of_property_read_u32(node, "qcom,effect-max", &temp);
	if (!rc) {
		chip->effect_max = temp;
		prop = of_find_property(node, "qcom,effect-arry", &temp);
		if (!prop) {
				dev_info(&chip->pdev->dev, "effect arry not found");
			} else if (temp != HAP_WAVE_SAMP_LEN * chip->effect_max) {
				dev_err(&chip->pdev->dev, "Invalid len of effect arry \n");
				chip->effect_max = 0;
				return -EINVAL;
			} else {
				chip->effect_arry = (u8 (*)[HAP_WAVE_SAMP_LEN])kmalloc(HAP_WAVE_SAMP_LEN * chip->effect_max, GFP_KERNEL);
				memcpy(chip->effect_arry, prop->value,
						HAP_WAVE_SAMP_LEN *  chip->effect_max);
				for (temp = 0; temp < chip->effect_max; temp++) {
					pr_info("effect_arry:%u: %u,%u,%u,%u,%u,%u,%u,%u\n",
							temp, chip->effect_arry[temp][0], chip->effect_arry[temp][1],
							chip->effect_arry[temp][2], chip->effect_arry[temp][3],
							chip->effect_arry[temp][4], chip->effect_arry[temp][5],
							chip->effect_arry[temp][6], chip->effect_arry[temp][7]);
				}
			}
	}
	chip->wave_rep_cnt = WF_REPEAT_MIN;
	rc = of_property_read_u32(node, "qcom,wave-rep-cnt", &temp);
	if (!rc) {
		chip->wave_rep_cnt = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read rep cnt rc=%d\n", rc);
		return rc;
	}

	chip->wave_s_rep_cnt = WF_S_REPEAT_MIN;
	rc = of_property_read_u32(node,
			"qcom,wave-samp-rep-cnt", &temp);
	if (!rc) {
		chip->wave_s_rep_cnt = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read samp rep cnt rc=%d\n", rc);
		return rc;
	}

	wf_samp_len = of_property_count_elems_of_size(node,
			"qcom,wave-samples", sizeof(u32));
	if (wf_samp_len > 0) {
		if (wf_samp_len > HAP_WAVE_SAMP_SET_LEN) {
			pr_err("Invalid length for wave samples\n");
			return -EINVAL;
		}

		rc = of_property_read_u32_array(node, "qcom,wave-samples",
				chip->wave_samp, wf_samp_len);
		if (rc < 0) {
			pr_err("Error in reading qcom,wave-samples, rc=%d\n",
				rc);
			return rc;
		}
	} else {
		/* Use default values */
		for (i = 0; i < HAP_WAVE_SAMP_LEN; i++)
			chip->wave_samp[i] = HAP_WF_SAMP_MAX;
	}

	return 0;
}

static int qpnp_haptics_parse_pwm_dt(struct hap_chip *chip)
{
	struct device_node *node = chip->pdev->dev.of_node;
	u32 temp;
	int rc;

	if (chip->pwm_data.period_us > 0 && chip->pwm_data.duty_us > 0)
		return 0;

	chip->pwm_data.pwm_dev = of_pwm_get(node, NULL);
	if (IS_ERR(chip->pwm_data.pwm_dev)) {
		rc = PTR_ERR(chip->pwm_data.pwm_dev);
		pr_err("Cannot get PWM device rc=%d\n", rc);
		chip->pwm_data.pwm_dev = NULL;
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,period-us", &temp);
	if (!rc) {
		chip->pwm_data.period_us = temp;
	} else {
		pr_err("Cannot read PWM period rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,duty-us", &temp);
	if (!rc) {
		chip->pwm_data.duty_us = temp;
	} else {
		pr_err("Cannot read PWM duty rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,ext-pwm-dtest-line", &temp);
	if (!rc)
		chip->ext_pwm_dtest_line = temp;

	rc = of_property_read_u32(node, "qcom,ext-pwm-freq-khz", &temp);
	if (!rc) {
		chip->ext_pwm_freq_khz = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read ext pwm freq rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int qpnp_haptics_parse_dt(struct hap_chip *chip)
{
	struct device_node *node = chip->pdev->dev.of_node;
	struct device_node *revid_node, *misc_node;
	const char *temp_str;
	int rc, temp;

	rc = of_property_read_u32(node, "reg", &temp);
	if (rc < 0) {
		pr_err("Couldn't find reg in node = %s rc = %d\n",
			node->full_name, rc);
		return rc;
	}

	if (temp <= 0) {
		pr_err("Invalid base address %x\n", temp);
		return -EINVAL;
	}
	chip->base = (u16)temp;

	revid_node = of_parse_phandle(node, "qcom,pmic-revid", 0);
	if (!revid_node) {
		pr_err("Missing qcom,pmic-revid property\n");
		return -EINVAL;
	}

	chip->revid = get_revid_data(revid_node);
	of_node_put(revid_node);
	if (IS_ERR_OR_NULL(chip->revid)) {
		pr_err("Unable to get pmic_revid rc=%ld\n",
			PTR_ERR(chip->revid));
		/*
		 * the revid peripheral must be registered, any failure
		 * here only indicates that the rev-id module has not
		 * probed yet.
		 */
		return -EPROBE_DEFER;
	}

	if (of_find_property(node, "qcom,pmic-misc", NULL)) {
		misc_node = of_parse_phandle(node, "qcom,pmic-misc", 0);
		if (!misc_node)
			return -EINVAL;

		rc = of_property_read_u32(node, "qcom,misc-clk-trim-error-reg",
				&chip->misc_clk_trim_error_reg);
		if (rc < 0 || !chip->misc_clk_trim_error_reg) {
			pr_err("Invalid or missing misc-clk-trim-error-reg\n");
			of_node_put(misc_node);
			return rc;
		}

		rc = qpnp_misc_read_reg(misc_node,
				chip->misc_clk_trim_error_reg,
				&chip->clk_trim_error_code);
		if (rc < 0) {
			pr_err("Couldn't get clk_trim_error_code, rc=%d\n", rc);
			of_node_put(misc_node);
			return -EPROBE_DEFER;
		}
		of_node_put(misc_node);
	}

	chip->play_irq = platform_get_irq_byname(chip->pdev, "hap-play-irq");
	if (chip->play_irq < 0) {
		pr_err("Unable to get play irq\n");
		return chip->play_irq;
	}

	chip->sc_irq = platform_get_irq_byname(chip->pdev, "hap-sc-irq");
	if (chip->sc_irq < 0) {
		pr_err("Unable to get sc irq\n");
		return chip->sc_irq;
	}

	chip->act_type = HAP_LRA;
	rc = of_property_read_u32(node, "qcom,actuator-type", &temp);
	if (!rc) {
		if (temp != HAP_LRA && temp != HAP_ERM) {
			pr_err("Incorrect actuator type\n");
			return -EINVAL;
		}
		chip->act_type = temp;
	}

	chip->lra_auto_mode = of_property_read_bool(node, "qcom,lra-auto-mode");

	rc = of_property_read_string(node, "qcom,play-mode", &temp_str);
	if (!rc) {
		if (strcmp(temp_str, "direct") == 0)
			chip->play_mode = HAP_DIRECT;
		else if (strcmp(temp_str, "buffer") == 0)
			chip->play_mode = HAP_BUFFER;
		else if (strcmp(temp_str, "pwm") == 0)
			chip->play_mode = HAP_PWM;
		else if (strcmp(temp_str, "audio") == 0)
			chip->play_mode = HAP_AUDIO;
		else {
			pr_err("Invalid play mode\n");
			return -EINVAL;
		}
	} else {
		if (rc == -EINVAL && chip->act_type == HAP_LRA) {
			pr_info("Play mode not specified, using auto mode\n");
			chip->lra_auto_mode = true;
		} else {
			pr_err("Unable to read play mode\n");
			return rc;
		}
	}

	chip->max_play_time_ms = HAP_MAX_PLAY_TIME_MS;
	rc = of_property_read_u32(node, "qcom,max-play-time-ms", &temp);
	if (!rc) {
		chip->max_play_time_ms = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read max-play-time rc=%d\n", rc);
		return rc;
	}

	chip->vmax_mv = HAP_VMAX_MAX_MV;
	rc = of_property_read_u32(node, "qcom,vmax-mv", &temp);
	if (!rc) {
		chip->vmax_mv = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read Vmax rc=%d\n", rc);
		return rc;
	}

	chip->overdrive =  (of_property_read_bool(node, "qcom,overdrive"));

	chip->ilim_ma = HAP_ILIM_400_MA;
	rc = of_property_read_u32(node, "qcom,ilim-ma", &temp);
	if (!rc) {
		chip->ilim_ma = (u8)temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read ILIM rc=%d\n", rc);
		return rc;
	}

	chip->sc_deb_cycles = HAP_DEF_SC_DEB_CYCLES;
	rc = of_property_read_u32(node, "qcom,sc-dbc-cycles", &temp);
	if (!rc) {
		chip->sc_deb_cycles = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read sc debounce rc=%d\n", rc);
		return rc;
	}

	chip->wave_shape = HAP_WAVE_SQUARE;
	rc = of_property_read_string(node, "qcom,wave-shape", &temp_str);
	if (!rc) {
		if (strcmp(temp_str, "sine") == 0)
			chip->wave_shape = HAP_WAVE_SINE;
		else if (strcmp(temp_str, "square") == 0)
			chip->wave_shape = HAP_WAVE_SQUARE;
		else {
			pr_err("Unsupported wave shape\n");
			return -EINVAL;
		}
	} else if (rc != -EINVAL) {
		pr_err("Unable to read wave shape rc=%d\n", rc);
		return rc;
	}

	chip->wave_play_rate_us = HAP_DEF_WAVE_PLAY_RATE_US;
	rc = of_property_read_u32(node,
			"qcom,wave-play-rate-us", &temp);
	if (!rc) {
		chip->wave_play_rate_us = temp;
	} else if (rc != -EINVAL) {
		pr_err("Unable to read play rate rc=%d\n", rc);
		return rc;
	}

	if (chip->wave_play_rate_us < HAP_WAVE_PLAY_RATE_US_MIN)
		chip->wave_play_rate_us = HAP_WAVE_PLAY_RATE_US_MIN;
	else if (chip->wave_play_rate_us > HAP_WAVE_PLAY_RATE_US_MAX)
		chip->wave_play_rate_us = HAP_WAVE_PLAY_RATE_US_MAX;

	chip->en_brake = of_property_read_bool(node, "qcom,en-brake");

	rc = of_property_count_elems_of_size(node,
			"qcom,brake-pattern", sizeof(u32));
	if (rc > 0) {
		if (rc != HAP_BRAKE_PAT_LEN) {
			pr_err("Invalid length for brake pattern\n");
			return -EINVAL;
		}

		rc = of_property_read_u32_array(node, "qcom,brake-pattern",
				chip->brake_pat, HAP_BRAKE_PAT_LEN);
		if (rc < 0) {
			pr_err("Error in reading qcom,brake-pattern, rc=%d\n",
				rc);
			return rc;
		}
	}

	/* Read the following properties only for LRA */
	if (chip->act_type == HAP_LRA) {
		rc = of_property_read_string(node, "qcom,lra-auto-res-mode",
					&temp_str);
		if (!rc) {
			if (chip->revid->pmic_subtype == PM660_SUBTYPE) {
				chip->ares_cfg.auto_res_mode =
						HAP_PM660_AUTO_RES_QWD;
				if (strcmp(temp_str, "zxd") == 0)
					chip->ares_cfg.auto_res_mode =
						HAP_PM660_AUTO_RES_ZXD;
				else if (strcmp(temp_str, "qwd") == 0)
					chip->ares_cfg.auto_res_mode =
						HAP_PM660_AUTO_RES_QWD;
			} else {
				chip->ares_cfg.auto_res_mode =
						HAP_AUTO_RES_ZXD_EOP;
				if (strcmp(temp_str, "none") == 0)
					chip->ares_cfg.auto_res_mode =
						HAP_AUTO_RES_NONE;
				else if (strcmp(temp_str, "zxd") == 0)
					chip->ares_cfg.auto_res_mode =
						HAP_AUTO_RES_ZXD;
				else if (strcmp(temp_str, "qwd") == 0)
					chip->ares_cfg.auto_res_mode =
						HAP_AUTO_RES_QWD;
				else if (strcmp(temp_str, "max-qwd") == 0)
					chip->ares_cfg.auto_res_mode =
						HAP_AUTO_RES_MAX_QWD;
				else
					chip->ares_cfg.auto_res_mode =
						HAP_AUTO_RES_ZXD_EOP;
			}
		} else if (rc != -EINVAL) {
			pr_err("Unable to read auto res mode rc=%d\n", rc);
			return rc;
		}

		chip->ares_cfg.lra_high_z = HAP_LRA_HIGH_Z_OPT3;
		rc = of_property_read_string(node, "qcom,lra-high-z",
					&temp_str);
		if (!rc) {
			if (strcmp(temp_str, "none") == 0)
				chip->ares_cfg.lra_high_z =
					HAP_LRA_HIGH_Z_NONE;
			else if (strcmp(temp_str, "opt1") == 0)
				chip->ares_cfg.lra_high_z =
					HAP_LRA_HIGH_Z_OPT1;
			else if (strcmp(temp_str, "opt2") == 0)
				chip->ares_cfg.lra_high_z =
					 HAP_LRA_HIGH_Z_OPT2;
			else
				chip->ares_cfg.lra_high_z =
					 HAP_LRA_HIGH_Z_OPT3;
			if (chip->revid->pmic_subtype == PM660_SUBTYPE) {
				if (strcmp(temp_str, "opt0") == 0)
					chip->ares_cfg.lra_high_z =
						HAP_LRA_HIGH_Z_NONE;
			}
		} else if (rc != -EINVAL) {
			pr_err("Unable to read LRA high-z rc=%d\n", rc);
			return rc;
		}

		chip->ares_cfg.lra_res_cal_period = HAP_RES_CAL_PERIOD_MAX;
		rc = of_property_read_u32(node,
				"qcom,lra-res-cal-period", &temp);
		if (!rc) {
			chip->ares_cfg.lra_res_cal_period = temp;
		} else if (rc != -EINVAL) {
			pr_err("Unable to read cal period rc=%d\n", rc);
			return rc;
		}

		chip->ares_cfg.lra_qwd_drive_duration = -EINVAL;
		chip->ares_cfg.calibrate_at_eop = -EINVAL;
		if (chip->revid->pmic_subtype == PM660_SUBTYPE) {
			rc = of_property_read_u32(node,
					"qcom,lra-qwd-drive-duration",
					&chip->ares_cfg.lra_qwd_drive_duration);
			if (rc && rc != -EINVAL) {
				pr_err("Unable to read LRA QWD drive duration rc=%d\n",
					rc);
				return rc;
			}

			rc = of_property_read_u32(node,
					"qcom,lra-calibrate-at-eop",
					&chip->ares_cfg.calibrate_at_eop);
			if (rc && rc != -EINVAL) {
				pr_err("Unable to read Calibrate at EOP rc=%d\n",
					rc);
				return rc;
			}
		}

		chip->drive_period_code_max_var_pct = 25;
		rc = of_property_read_u32(node,
			"qcom,drive-period-code-max-variation-pct", &temp);
		if (!rc) {
			if (temp > 0 && temp < 100)
				chip->drive_period_code_max_var_pct = (u8)temp;
		} else if (rc != -EINVAL) {
			pr_err("Unable to read drive period code max var pct rc=%d\n",
				rc);
			return rc;
		}

		chip->drive_period_code_min_var_pct = 25;
		rc = of_property_read_u32(node,
			"qcom,drive-period-code-min-variation-pct", &temp);
		if (!rc) {
			if (temp > 0 && temp < 100)
				chip->drive_period_code_min_var_pct = (u8)temp;
		} else if (rc != -EINVAL) {
			pr_err("Unable to read drive period code min var pct rc=%d\n",
				rc);
			return rc;
		}

		chip->auto_res_err_recovery_hw =
			of_property_read_bool(node,
				"qcom,auto-res-err-recovery-hw");

		if (chip->revid->pmic_subtype != PM660_SUBTYPE)
			chip->auto_res_err_recovery_hw = false;
	}

	if (rc == -EINVAL)
		rc = 0;

	rc = qpnp_haptics_parse_buffer_dt(chip);

	if (chip->play_mode == HAP_PWM)
		rc = qpnp_haptics_parse_pwm_dt(chip);

	return rc;
}

static int qpnp_haptics_probe(struct platform_device *pdev)
{
	struct hap_chip *chip;
	int rc, i;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	chip->pdev = pdev;
	rc = qpnp_haptics_parse_dt(chip);
	if (rc < 0) {
		dev_err(&pdev->dev, "Error in parsing DT parameters, rc=%d\n",
			rc);
		return rc;
	}

	spin_lock_init(&chip->bus_lock);
	mutex_init(&chip->play_lock);
	mutex_init(&chip->param_lock);
	INIT_WORK(&chip->haptics_work, qpnp_haptics_work);

	rc = qpnp_haptics_config(chip);
	if (rc < 0) {
		dev_err(&pdev->dev, "Error in configuring haptics, rc=%d\n",
			rc);
		goto fail;
	}

	hrtimer_init(&chip->stop_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	chip->stop_timer.function = hap_stop_timer;
	hrtimer_init(&chip->auto_res_err_poll_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	chip->auto_res_err_poll_timer.function = hap_auto_res_err_poll_timer;
	dev_set_drvdata(&pdev->dev, chip);

	chip->cdev.name = "vibrator";
	chip->cdev.brightness_get = qpnp_haptics_brightness_get;
	chip->cdev.brightness_set = qpnp_haptics_brightness_set;
	chip->cdev.max_brightness = 100;
	rc = devm_led_classdev_register(&pdev->dev, &chip->cdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "Error in registering led class device, rc=%d\n",
			rc);
		goto register_fail;
	}

	for (i = 0; i < ARRAY_SIZE(qpnp_haptics_attrs); i++) {
		rc = sysfs_create_file(&chip->cdev.dev->kobj,
				&qpnp_haptics_attrs[i].attr);
		if (rc < 0) {
			dev_err(&pdev->dev, "Error in creating sysfs file, rc=%d\n",
				rc);
			goto sysfs_fail;
		}
	}

	pr_info("haptic probe succeed\n");

	return 0;

sysfs_fail:
	for (--i; i >= 0; i--)
		sysfs_remove_file(&chip->cdev.dev->kobj,
				&qpnp_haptics_attrs[i].attr);
register_fail:
	cancel_work_sync(&chip->haptics_work);
	hrtimer_cancel(&chip->auto_res_err_poll_timer);
	hrtimer_cancel(&chip->stop_timer);
fail:
	mutex_destroy(&chip->play_lock);
	mutex_destroy(&chip->param_lock);
	if (chip->pwm_data.pwm_dev)
		pwm_put(chip->pwm_data.pwm_dev);
	dev_set_drvdata(&pdev->dev, NULL);
	return rc;
}

static int qpnp_haptics_remove(struct platform_device *pdev)
{
	struct hap_chip *chip = dev_get_drvdata(&pdev->dev);

	cancel_work_sync(&chip->haptics_work);
	hrtimer_cancel(&chip->auto_res_err_poll_timer);
	hrtimer_cancel(&chip->stop_timer);
	mutex_destroy(&chip->play_lock);
	mutex_destroy(&chip->param_lock);
	if (chip->pwm_data.pwm_dev)
		pwm_put(chip->pwm_data.pwm_dev);
	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

static void qpnp_haptics_shutdown(struct platform_device *pdev)
{
	struct hap_chip *chip = dev_get_drvdata(&pdev->dev);

	cancel_work_sync(&chip->haptics_work);

	/* disable haptics */
	qpnp_haptics_mod_enable(chip, false);
}

static const struct dev_pm_ops qpnp_haptics_pm_ops = {
	.suspend	= qpnp_haptics_suspend,
};

static const struct of_device_id hap_match_table[] = {
	{ .compatible = "qcom,qpnp-haptics" },
	{ },
};

static struct platform_driver qpnp_haptics_driver = {
	.driver		= {
		.name		= "qcom,qpnp-haptics",
		.of_match_table	= hap_match_table,
		.pm		= &qpnp_haptics_pm_ops,
	},
	.probe		= qpnp_haptics_probe,
	.remove		= qpnp_haptics_remove,
	.shutdown	= qpnp_haptics_shutdown,
};
module_platform_driver(qpnp_haptics_driver);

MODULE_DESCRIPTION("QPNP haptics driver");
MODULE_LICENSE("GPL v2");
