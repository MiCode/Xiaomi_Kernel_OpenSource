/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/of_device.h>
#include <linux/spmi.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/qpnp/pwm.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/qpnp/qpnp-haptic.h>
#include "../../staging/android/timed_output.h"

#define QPNP_IRQ_FLAGS	(IRQF_TRIGGER_RISING | \
			IRQF_TRIGGER_FALLING | \
			IRQF_ONESHOT)

#define QPNP_HAP_STATUS(b)		(b + 0x0A)
#define QPNP_HAP_LRA_AUTO_RES_LO(b)	(b + 0x0B)
#define QPNP_HAP_LRA_AUTO_RES_HI(b)     (b + 0x0C)
#define QPNP_HAP_EN_CTL_REG(b)		(b + 0x46)
#define QPNP_HAP_EN_CTL2_REG(b)		(b + 0x48)
#define QPNP_HAP_ACT_TYPE_REG(b)	(b + 0x4C)
#define QPNP_HAP_WAV_SHAPE_REG(b)	(b + 0x4D)
#define QPNP_HAP_PLAY_MODE_REG(b)	(b + 0x4E)
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
#define QPNP_HAP_ACT_TYPE_MASK		0xFE
#define QPNP_HAP_LRA			0x0
#define QPNP_HAP_ERM			0x1
#define QPNP_HAP_AUTO_RES_MODE_MASK	0x8F
#define QPNP_HAP_AUTO_RES_MODE_SHIFT	4
#define QPNP_HAP_LRA_HIGH_Z_MASK	0xF3
#define QPNP_HAP_LRA_HIGH_Z_SHIFT	2
#define QPNP_HAP_LRA_RES_CAL_PER_MASK	0xFC
#define QPNP_HAP_RES_CAL_PERIOD_MIN	4
#define QPNP_HAP_RES_CAL_PERIOD_MAX	32
#define QPNP_HAP_PLAY_MODE_MASK		0xCF
#define QPNP_HAP_PLAY_MODE_SHFT		4
#define QPNP_HAP_VMAX_MASK		0xC1
#define QPNP_HAP_VMAX_SHIFT		1
#define QPNP_HAP_VMAX_MIN_MV		116
#define QPNP_HAP_VMAX_MAX_MV		3596
#define QPNP_HAP_ILIM_MASK		0xFE
#define QPNP_HAP_ILIM_MIN_MV		400
#define QPNP_HAP_ILIM_MAX_MV		800
#define QPNP_HAP_SC_DEB_MASK		0xF8
#define QPNP_HAP_SC_DEB_SUB		2
#define QPNP_HAP_SC_DEB_CYCLES_MIN	0
#define QPNP_HAP_DEF_SC_DEB_CYCLES	8
#define QPNP_HAP_SC_DEB_CYCLES_MAX	32
#define QPNP_HAP_SC_CLR			1
#define QPNP_HAP_INT_PWM_MASK		0xFC
#define QPNP_HAP_INT_PWM_FREQ_253_KHZ	253
#define QPNP_HAP_INT_PWM_FREQ_505_KHZ	505
#define QPNP_HAP_INT_PWM_FREQ_739_KHZ	739
#define QPNP_HAP_INT_PWM_FREQ_1076_KHZ	1076
#define QPNP_HAP_WAV_SHAPE_MASK		0xFE
#define QPNP_HAP_RATE_CFG1_MASK		0xFF
#define QPNP_HAP_RATE_CFG2_MASK		0xF0
#define QPNP_HAP_RATE_CFG2_SHFT		8
#define QPNP_HAP_RATE_CFG_STEP_US	5
#define QPNP_HAP_WAV_PLAY_RATE_US_MIN	0
#define QPNP_HAP_DEF_WAVE_PLAY_RATE_US	5715
#define QPNP_HAP_WAV_PLAY_RATE_US_MAX	20475
#define QPNP_HAP_WAV_REP_MASK		0x8F
#define QPNP_HAP_WAV_S_REP_MASK		0xFC
#define QPNP_HAP_WAV_REP_SHFT		4
#define QPNP_HAP_WAV_REP_MIN		1
#define QPNP_HAP_WAV_REP_MAX		128
#define QPNP_HAP_WAV_S_REP_MIN		1
#define QPNP_HAP_WAV_S_REP_MAX		8
#define QPNP_HAP_BRAKE_PAT_MASK		0x3
#define QPNP_HAP_ILIM_MIN_MA		400
#define QPNP_HAP_ILIM_MAX_MA		800
#define QPNP_HAP_EXT_PWM_MASK		0xFC
#define QPNP_HAP_EXT_PWM_FREQ_25_KHZ	25
#define QPNP_HAP_EXT_PWM_FREQ_50_KHZ	50
#define QPNP_HAP_EXT_PWM_FREQ_75_KHZ	75
#define QPNP_HAP_EXT_PWM_FREQ_100_KHZ	100
#define PWM_MAX_DTEST_LINES		4
#define QPNP_HAP_EXT_PWM_DTEST_MASK	0x0F
#define QPNP_HAP_EXT_PWM_DTEST_SHFT	4
#define QPNP_HAP_EXT_PWM_PEAK_DATA	0x7F
#define QPNP_HAP_EXT_PWM_HALF_DUTY	50
#define QPNP_HAP_EXT_PWM_FULL_DUTY	100
#define QPNP_HAP_EXT_PWM_DATA_FACTOR	39
#define QPNP_HAP_WAV_SINE		0
#define QPNP_HAP_WAV_SQUARE		1
#define QPNP_HAP_WAV_SAMP_LEN		8
#define QPNP_HAP_WAV_SAMP_MAX		0x7E
#define QPNP_HAP_BRAKE_PAT_LEN		4
#define QPNP_HAP_PLAY_EN		0x80
#define QPNP_HAP_EN			0x80
#define QPNP_HAP_BRAKE_MASK		0xFE
#define QPNP_HAP_TEST2_AUTO_RES_MASK	0x7F
#define QPNP_HAP_SEC_UNLOCK		0xA5
#define AUTO_RES_ENABLE			0x80
#define AUTO_RES_DISABLE		0x00
#define AUTO_RES_ERR_BIT		0x10
#define SC_FOUND_BIT			0x08
#define SC_MAX_DURATION			5

#define QPNP_HAP_TIMEOUT_MS_MAX		15000
#define QPNP_HAP_STR_SIZE		20
#define QPNP_HAP_MAX_RETRIES		5
#define QPNP_HAP_CYCLS			5
#define QPNP_TEST_TIMER_MS		5

#define QPNP_HAP_TIME_REQ_FOR_BACK_EMF_GEN 20000
#define AUTO_RES_ERR_CAPTURE_RES	5
#define AUTO_RES_ERR_MAX		15

#define MISC_TRIM_ERROR_RC19P2_CLK	0x09F5
#define MISC_SEC_ACCESS			0x09D0
#define MISC_SEC_UNLOCK			0xA5
#define PMI8950_MISC_SID		2

#define POLL_TIME_AUTO_RES_ERR_NS	(5 * NSEC_PER_MSEC)

#define LRA_POS_FREQ_COUNT		6
int lra_play_rate_code[LRA_POS_FREQ_COUNT];

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

/* high Z option lines */
enum qpnp_hap_high_z {
	QPNP_HAP_LRA_HIGH_Z_NONE,
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

/* pwm channel info */
struct qpnp_pwm_info {
	struct pwm_device *pwm_dev;
	u32 pwm_channel;
	u32 duty_us;
	u32 period_us;
};

/*
 *  qpnp_hap - Haptic data structure
 *  @ spmi - spmi device
 *  @ hap_timer - hrtimer
 *  @ auto_res_err_poll_timer - hrtimer for auto-resonance error
 *  @ timed_dev - timed output device
 *  @ work - worker
 *  @ auto_res_err_work - correct auto resonance error
 *  @ sc_work - worker to handle short circuit condition
 *  @ pwm_info - pwm info
 *  @ lock - mutex lock
 *  @ wf_lock - mutex lock for waveform
 *  @ play_mode - play mode
 *  @ auto_res_mode - auto resonace mode
 *  @ lra_high_z - high z option line
 *  @ timeout_ms - max timeout in ms
 *  @ time_required_to_generate_back_emf_us - the time required for sufficient
      back-emf to be generated for auto resonance to be successful
 *  @ vmax_mv - max voltage in mv
 *  @ ilim_ma - limiting current in ma
 *  @ sc_deb_cycles - short circuit debounce cycles
 *  @ int_pwm_freq_khz - internal pwm frequency in khz
 *  @ wave_play_rate_us - play rate for waveform
 *  @ ext_pwm_freq_khz - external pwm frequency in khz
 *  @ wave_rep_cnt - waveform repeat count
 *  @ wave_s_rep_cnt - waveform sample repeat count
 *  @ play_irq - irq for play
 *  @ sc_irq - irq for short circuit
 *  @ base - base address
 *  @ act_type - actuator type
 *  @ wave_shape - waveform shape
 *  @ wave_samp - array of wave samples
 *  @ shadow_wave_samp - shadow array of wave samples
 *  @ brake_pat - pattern for active breaking
 *  @ reg_en_ctl - enable control register
 *  @ reg_play - play register
 *  @ lra_res_cal_period - period for resonance calibration
 *  @ sc_duration - counter to determine the duration of short circuit condition
 *  @ state - current state of haptics
 *  @ use_play_irq - play irq usage state
 *  @ use_sc_irq - short circuit irq usage state
 *  @ wf_update - waveform update flag
 *  @ pwm_cfg_state - pwm mode configuration state
 *  @ buffer_cfg_state - buffer mode configuration state
 *  @ en_brake - brake state
 *  @ sup_brake_pat - support custom brake pattern
 *  @ correct_lra_drive_freq - correct LRA Drive Frequency
 *  @ misc_trim_error_rc19p2_clk_reg_present - if MISC Trim Error reg is present
 */
struct qpnp_hap {
	struct spmi_device *spmi;
	struct regulator *vcc_pon;
	struct hrtimer hap_timer;
	struct hrtimer auto_res_err_poll_timer;
	struct timed_output_dev timed_dev;
	struct work_struct work;
	struct work_struct auto_res_err_work;
	struct delayed_work sc_work;
	struct hrtimer hap_test_timer;
	struct work_struct test_work;
	struct qpnp_pwm_info pwm_info;
	struct mutex lock;
	struct mutex wf_lock;
	struct completion completion;
	enum qpnp_hap_mode play_mode;
	enum qpnp_hap_auto_res_mode auto_res_mode;
	enum qpnp_hap_high_z lra_high_z;
	u32 timeout_ms;
	u32 time_required_to_generate_back_emf_us;
	u32 vmax_mv;
	u32 ilim_ma;
	u32 sc_deb_cycles;
	u32 int_pwm_freq_khz;
	u32 wave_play_rate_us;
	u32 ext_pwm_freq_khz;
	u32 wave_rep_cnt;
	u32 wave_s_rep_cnt;
	u32 play_irq;
	u32 sc_irq;
	u16 base;
	u8 act_type;
	u8 wave_shape;
	u8 wave_samp[QPNP_HAP_WAV_SAMP_LEN];
	u8 shadow_wave_samp[QPNP_HAP_WAV_SAMP_LEN];
	u8 brake_pat[QPNP_HAP_BRAKE_PAT_LEN];
	u8 reg_en_ctl;
	u8 reg_play;
	u8 lra_res_cal_period;
	u8 sc_duration;
	u8 ext_pwm_dtest_line;
	bool state;
	bool use_play_irq;
	bool use_sc_irq;
	bool manage_pon_supply;
	bool wf_update;
	bool pwm_cfg_state;
	bool buffer_cfg_state;
	bool en_brake;
	bool sup_brake_pat;
	bool correct_lra_drive_freq;
	bool misc_trim_error_rc19p2_clk_reg_present;
};

static struct qpnp_hap *ghap;

/* helper to read a pmic register */
static int qpnp_hap_read_reg(struct qpnp_hap *hap, u8 *data, u16 addr)
{
	int rc;

	rc = spmi_ext_register_readl(hap->spmi->ctrl, hap->spmi->sid,
							addr, data, 1);
	if (rc < 0)
		dev_err(&hap->spmi->dev,
			"Error reading address: %X - ret %X\n", addr, rc);

	return rc;
}

/* helper to write a pmic register */
static int qpnp_hap_write_reg(struct qpnp_hap *hap, u8 *data, u16 addr)
{
	int rc;

	rc = spmi_ext_register_writel(hap->spmi->ctrl, hap->spmi->sid,
							addr, data, 1);
	if (rc < 0)
		dev_err(&hap->spmi->dev,
			"Error writing address: %X - ret %X\n", addr, rc);

	dev_dbg(&hap->spmi->dev, "write: HAP_0x%x = 0x%x\n", addr, *data);
	return rc;
}

/* helper to access secure registers */
static int qpnp_hap_sec_access(struct qpnp_hap *hap)
{
	int rc;
	u8 reg = QPNP_HAP_SEC_UNLOCK;

	rc = qpnp_hap_write_reg(hap, &reg,
		QPNP_HAP_SEC_ACCESS_REG(hap->base));
	if (rc)
		return rc;

	return 0;
}

static void qpnp_handle_sc_irq(struct work_struct *work)
{
	struct qpnp_hap *hap = container_of(work,
				struct qpnp_hap, sc_work.work);
	u8 val, reg;

	qpnp_hap_read_reg(hap, &val, QPNP_HAP_STATUS(hap->base));

	/* clear short circuit register */
	if (val & SC_FOUND_BIT) {
		hap->sc_duration++;
		reg = QPNP_HAP_SC_CLR;
		qpnp_hap_write_reg(hap, &reg, QPNP_HAP_SC_CLR_REG(hap->base));
	}
}

static int qpnp_hap_mod_enable(struct qpnp_hap *hap, int on)
{
	u8 val;
	int rc, i;

	val = hap->reg_en_ctl;
	if (on) {
		val |= QPNP_HAP_EN;
	} else {
		for (i = 0; i < QPNP_HAP_MAX_RETRIES; i++) {
			unsigned long sleep_time =
				QPNP_HAP_CYCLS * hap->wave_play_rate_us;

			rc = qpnp_hap_read_reg(hap, &val,
				QPNP_HAP_STATUS(hap->base));

			dev_dbg(&hap->spmi->dev, "HAP_STATUS=0x%x\n", val);

			/* wait for QPNP_HAP_CYCLS cycles of play rate */
			if (val & QPNP_HAP_STATUS_BUSY) {
				usleep_range(sleep_time, sleep_time + 1);
				if (hap->play_mode == QPNP_HAP_DIRECT ||
					hap->play_mode == QPNP_HAP_PWM)
					break;
			} else
				break;
		}

		if (i >= QPNP_HAP_MAX_RETRIES)
			dev_dbg(&hap->spmi->dev,
				"Haptics Busy. Force disable\n");

		val &= ~QPNP_HAP_EN;
	}

	rc = qpnp_hap_write_reg(hap, &val,
			QPNP_HAP_EN_CTL_REG(hap->base));
	if (rc < 0)
		return rc;

	hap->reg_en_ctl = val;

	return 0;
}

static int qpnp_hap_play(struct qpnp_hap *hap, int on)
{
	u8 val;
	int rc;

	val = hap->reg_play;
	if (on)
		val |= QPNP_HAP_PLAY_EN;
	else
		val &= ~QPNP_HAP_PLAY_EN;

	rc = qpnp_hap_write_reg(hap, &val,
			QPNP_HAP_PLAY_REG(hap->base));
	if (rc < 0)
		return rc;

	hap->reg_play = val;

	return 0;
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
		qpnp_hap_read_reg(hap, &val, hap->base + qpnp_hap_dbg_regs[i]);
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
	u8 reg;

	mutex_lock(&hap->wf_lock);

	/* Configure WAVE_SAMPLE1 to WAVE_SAMPLE8 register */
	for (i = 0; i < QPNP_HAP_WAV_SAMP_LEN && hap->wf_update; i++) {
		reg = hap->wave_samp[i] = hap->shadow_wave_samp[i];
		rc = qpnp_hap_write_reg(hap, &reg,
			QPNP_HAP_WAV_S_REG_BASE(hap->base) + i);
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
	u8 disable_haptics = 0x00;
	u8 val;

	dev_dbg(&hap->spmi->dev, "Short circuit detected\n");

	if (hap->sc_duration < SC_MAX_DURATION) {
		qpnp_hap_read_reg(hap, &val, QPNP_HAP_STATUS(hap->base));
		if (val & SC_FOUND_BIT)
			schedule_delayed_work(&hap->sc_work,
					QPNP_HAP_SC_IRQ_STATUS_DELAY);
		else
			hap->sc_duration = 0;
	} else {
		/* Disable haptics module if the duration of short circuit
		 * exceeds the maximum limit (5 secs).
		 */
		rc = qpnp_hap_write_reg(hap, &disable_haptics,
					QPNP_HAP_EN_CTL_REG(hap->base));
		dev_err(&hap->spmi->dev,
			"Haptics disabled permanently due to short circuit\n");
	}

	return IRQ_HANDLED;
}

/* configuration api for buffer mode */
static int qpnp_hap_buffer_config(struct qpnp_hap *hap)
{
	u8 reg = 0;
	int rc, i, temp;

	/* Configure the WAVE_REPEAT register */
	if (hap->wave_rep_cnt < QPNP_HAP_WAV_REP_MIN)
		hap->wave_rep_cnt = QPNP_HAP_WAV_REP_MIN;
	else if (hap->wave_rep_cnt > QPNP_HAP_WAV_REP_MAX)
		hap->wave_rep_cnt = QPNP_HAP_WAV_REP_MAX;

	if (hap->wave_s_rep_cnt < QPNP_HAP_WAV_S_REP_MIN)
		hap->wave_s_rep_cnt = QPNP_HAP_WAV_S_REP_MIN;
	else if (hap->wave_s_rep_cnt > QPNP_HAP_WAV_S_REP_MAX)
		hap->wave_s_rep_cnt = QPNP_HAP_WAV_S_REP_MAX;

	rc = qpnp_hap_read_reg(hap, &reg,
			QPNP_HAP_WAV_REP_REG(hap->base));
	if (rc < 0)
		return rc;
	reg &= QPNP_HAP_WAV_REP_MASK;
	temp = fls(hap->wave_rep_cnt) - 1;
	reg |= (temp << QPNP_HAP_WAV_REP_SHFT);
	reg &= QPNP_HAP_WAV_S_REP_MASK;
	temp = fls(hap->wave_s_rep_cnt) - 1;
	reg |= temp;
	rc = qpnp_hap_write_reg(hap, &reg,
			QPNP_HAP_WAV_REP_REG(hap->base));
	if (rc)
		return rc;

	/* Configure WAVE_SAMPLE1 to WAVE_SAMPLE8 register */
	for (i = 0, reg = 0; i < QPNP_HAP_WAV_SAMP_LEN; i++) {
		reg = hap->wave_samp[i];
		rc = qpnp_hap_write_reg(hap, &reg,
			QPNP_HAP_WAV_S_REG_BASE(hap->base) + i);
		if (rc)
			return rc;
	}

	/* setup play irq */
	if (hap->use_play_irq) {
		rc = devm_request_threaded_irq(&hap->spmi->dev, hap->play_irq,
			NULL, qpnp_hap_play_irq,
			QPNP_IRQ_FLAGS,
			"qpnp_play_irq", hap);
		if (rc < 0) {
			dev_err(&hap->spmi->dev,
				"Unable to request play(%d) IRQ(err:%d)\n",
				hap->play_irq, rc);
			return rc;
		}
	}

	hap->buffer_cfg_state = true;
	return 0;
}

/* configuration api for pwm */
static int qpnp_hap_pwm_config(struct qpnp_hap *hap)
{
	u8 reg = 0;
	int rc, temp;

	/* Configure the EXTERNAL_PWM register */
	if (hap->ext_pwm_freq_khz <= QPNP_HAP_EXT_PWM_FREQ_25_KHZ) {
		hap->ext_pwm_freq_khz = QPNP_HAP_EXT_PWM_FREQ_25_KHZ;
		temp = 0;
	} else if (hap->ext_pwm_freq_khz <=
				QPNP_HAP_EXT_PWM_FREQ_50_KHZ) {
		hap->ext_pwm_freq_khz = QPNP_HAP_EXT_PWM_FREQ_50_KHZ;
		temp = 1;
	} else if (hap->ext_pwm_freq_khz <=
				QPNP_HAP_EXT_PWM_FREQ_75_KHZ) {
		hap->ext_pwm_freq_khz = QPNP_HAP_EXT_PWM_FREQ_75_KHZ;
		temp = 2;
	} else {
		hap->ext_pwm_freq_khz = QPNP_HAP_EXT_PWM_FREQ_100_KHZ;
		temp = 3;
	}

	rc = qpnp_hap_read_reg(hap, &reg,
			QPNP_HAP_EXT_PWM_REG(hap->base));
	if (rc < 0)
		return rc;
	reg &= QPNP_HAP_EXT_PWM_MASK;
	reg |= temp;
	rc = qpnp_hap_write_reg(hap, &reg,
			QPNP_HAP_EXT_PWM_REG(hap->base));
	if (rc)
		return rc;

	rc = qpnp_hap_read_reg(hap, &reg,
			QPNP_HAP_TEST2_REG(hap->base));
	if (rc)
		return rc;
	if (!hap->ext_pwm_dtest_line ||
			hap->ext_pwm_dtest_line > PWM_MAX_DTEST_LINES) {
		dev_err(&hap->spmi->dev, "invalid dtest line\n");
		return -EINVAL;
	}

	/* disable auto res for PWM mode */
	reg &= QPNP_HAP_EXT_PWM_DTEST_MASK;
	temp = hap->ext_pwm_dtest_line << QPNP_HAP_EXT_PWM_DTEST_SHFT;
	reg |= temp;

	/* TEST2 is a secure access register */
	rc = qpnp_hap_sec_access(hap);
	if (rc)
		return rc;

	rc = qpnp_hap_write_reg(hap, &reg,
			QPNP_HAP_TEST2_REG(hap->base));
	if (rc)
		return rc;

	rc = pwm_config(hap->pwm_info.pwm_dev,
				hap->pwm_info.duty_us * NSEC_PER_USEC,
				hap->pwm_info.period_us * NSEC_PER_USEC);
	if (rc < 0) {
		dev_err(&hap->spmi->dev, "hap pwm config failed\n");
		pwm_free(hap->pwm_info.pwm_dev);
		return -ENODEV;
	}

	hap->pwm_cfg_state = true;

	return 0;
}

/* configuration api for play mode */
static int qpnp_hap_play_mode_config(struct qpnp_hap *hap)
{
	u8 reg = 0;
	int rc, temp;

	rc = qpnp_hap_read_reg(hap, &reg, QPNP_HAP_PLAY_MODE_REG(hap->base));
	if (rc < 0)
		return rc;
	reg &= QPNP_HAP_PLAY_MODE_MASK;
	temp = hap->play_mode << QPNP_HAP_PLAY_MODE_SHFT;
	reg |= temp;
	rc = qpnp_hap_write_reg(hap, &reg, QPNP_HAP_PLAY_MODE_REG(hap->base));
	if (rc)
		return rc;
	return 0;
}

/* configuration api for max volatge */
static int qpnp_hap_vmax_config(struct qpnp_hap *hap)
{
	u8 reg = 0;
	int rc, temp;

	if (hap->vmax_mv < QPNP_HAP_VMAX_MIN_MV)
		hap->vmax_mv = QPNP_HAP_VMAX_MIN_MV;
	else if (hap->vmax_mv > QPNP_HAP_VMAX_MAX_MV)
		hap->vmax_mv = QPNP_HAP_VMAX_MAX_MV;

	rc = qpnp_hap_read_reg(hap, &reg, QPNP_HAP_VMAX_REG(hap->base));
	if (rc < 0)
		return rc;
	reg &= QPNP_HAP_VMAX_MASK;
	temp = hap->vmax_mv / QPNP_HAP_VMAX_MIN_MV;
	reg |= (temp << QPNP_HAP_VMAX_SHIFT);
	rc = qpnp_hap_write_reg(hap, &reg, QPNP_HAP_VMAX_REG(hap->base));
	if (rc)
		return rc;

	return 0;
}

/* configuration api for short circuit debounce */
static int qpnp_hap_sc_deb_config(struct qpnp_hap *hap)
{
	u8 reg = 0;
	int rc, temp;

	if (hap->sc_deb_cycles < QPNP_HAP_SC_DEB_CYCLES_MIN)
		hap->sc_deb_cycles = QPNP_HAP_SC_DEB_CYCLES_MIN;
	else if (hap->sc_deb_cycles > QPNP_HAP_SC_DEB_CYCLES_MAX)
		hap->sc_deb_cycles = QPNP_HAP_SC_DEB_CYCLES_MAX;

	rc = qpnp_hap_read_reg(hap, &reg, QPNP_HAP_SC_DEB_REG(hap->base));
	if (rc < 0)
		return rc;
	reg &= QPNP_HAP_SC_DEB_MASK;
	if (hap->sc_deb_cycles) {
		temp = fls(hap->sc_deb_cycles) - 1;
		reg |= temp - QPNP_HAP_SC_DEB_SUB;
	}
	rc = qpnp_hap_write_reg(hap, &reg, QPNP_HAP_SC_DEB_REG(hap->base));
	if (rc)
		return rc;

	return 0;
}

/* DT parsing api for buffer mode */
static int qpnp_hap_parse_buffer_dt(struct qpnp_hap *hap)
{
	struct spmi_device *spmi = hap->spmi;
	struct property *prop;
	u32 temp;
	int rc, i;

	hap->wave_rep_cnt = QPNP_HAP_WAV_REP_MIN;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,wave-rep-cnt", &temp);
	if (!rc) {
		hap->wave_rep_cnt = temp;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read rep cnt\n");
		return rc;
	}

	hap->wave_s_rep_cnt = QPNP_HAP_WAV_S_REP_MIN;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,wave-samp-rep-cnt", &temp);
	if (!rc) {
		hap->wave_s_rep_cnt = temp;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read samp rep cnt\n");
		return rc;
	}

	prop = of_find_property(spmi->dev.of_node,
			"qcom,wave-samples", &temp);
	if (!prop || temp != QPNP_HAP_WAV_SAMP_LEN) {
		dev_err(&spmi->dev, "Invalid wave samples, use default");
		for (i = 0; i < QPNP_HAP_WAV_SAMP_LEN; i++)
			hap->wave_samp[i] = QPNP_HAP_WAV_SAMP_MAX;
	} else {
		memcpy(hap->wave_samp, prop->value, QPNP_HAP_WAV_SAMP_LEN);
	}

	hap->use_play_irq = of_property_read_bool(spmi->dev.of_node,
				"qcom,use-play-irq");
	if (hap->use_play_irq) {
		hap->play_irq = spmi_get_irq_byname(hap->spmi,
					NULL, "play-irq");
		if (hap->play_irq < 0) {
			dev_err(&spmi->dev, "Unable to get play irq\n");
			return hap->play_irq;
		}
	}

	return 0;
}

/* DT parsing api for PWM mode */
static int qpnp_hap_parse_pwm_dt(struct qpnp_hap *hap)
{
	struct spmi_device *spmi = hap->spmi;
	u32 temp;
	int rc;

	hap->ext_pwm_freq_khz = QPNP_HAP_EXT_PWM_FREQ_25_KHZ;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,ext-pwm-freq-khz", &temp);
	if (!rc) {
		hap->ext_pwm_freq_khz = temp;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read ext pwm freq\n");
		return rc;
	}

	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,pwm-channel", &temp);
	if (!rc)
		hap->pwm_info.pwm_channel = temp;
	else
		return rc;

	hap->pwm_info.pwm_dev = of_pwm_get(spmi->dev.of_node, NULL);

	if (IS_ERR(hap->pwm_info.pwm_dev)) {
		rc = PTR_ERR(hap->pwm_info.pwm_dev);
		dev_err(&spmi->dev, "Cannot get PWM device rc:(%d)\n", rc);
		hap->pwm_info.pwm_dev = NULL;
		return rc;
	}

	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,period-us", &temp);
	if (!rc)
		hap->pwm_info.period_us = temp;
	else
		return rc;

	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,duty-us", &temp);
	if (!rc)
		hap->pwm_info.duty_us = temp;
	else
		return rc;

	rc = of_property_read_u32(spmi->dev.of_node,
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
		dev_err(dev, "Invalid sample index(%d)\n", index);
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
	int data;

	if (index < 0 || index >= QPNP_HAP_WAV_SAMP_LEN) {
		dev_err(dev, "Invalid sample index(%d)\n", index);
		return -EINVAL;
	}

	if (sscanf(buf, "%x", &data) != 1)
		return -EINVAL;

	if (data < 0 || data > 0xff) {
		dev_err(dev, "Invalid sample wf_%d (%d)\n", index, data);
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
	int data, rc, temp;
	u8 reg;

	if (sscanf(buf, "%d", &data) != 1)
		return -EINVAL;

	if (data < QPNP_HAP_WAV_REP_MIN)
		data = QPNP_HAP_WAV_REP_MIN;
	else if (data > QPNP_HAP_WAV_REP_MAX)
		data = QPNP_HAP_WAV_REP_MAX;

	rc = qpnp_hap_read_reg(hap, &reg,
			QPNP_HAP_WAV_REP_REG(hap->base));
	if (rc < 0)
		return rc;
	reg &= QPNP_HAP_WAV_REP_MASK;
	temp = fls(data) - 1;
	reg |= (temp << QPNP_HAP_WAV_REP_SHFT);
	rc = qpnp_hap_write_reg(hap, &reg,
			QPNP_HAP_WAV_REP_REG(hap->base));
	if (rc)
		return rc;

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
	int data, rc, temp;
	u8 reg;

	if (sscanf(buf, "%d", &data) != 1)
		return -EINVAL;

	if (data < QPNP_HAP_WAV_S_REP_MIN)
		data = QPNP_HAP_WAV_S_REP_MIN;
	else if (data > QPNP_HAP_WAV_S_REP_MAX)
		data = QPNP_HAP_WAV_S_REP_MAX;

	rc = qpnp_hap_read_reg(hap, &reg,
			QPNP_HAP_WAV_REP_REG(hap->base));
	if (rc < 0)
		return rc;
	reg &= QPNP_HAP_WAV_S_REP_MASK;
	temp = fls(data) - 1;
	reg |= temp;
	rc = qpnp_hap_write_reg(hap, &reg,
			QPNP_HAP_WAV_REP_REG(hap->base));
	if (rc)
		return rc;

	hap->wave_s_rep_cnt = data;

	return count;
}

/* sysfs store function for play mode*/
static ssize_t qpnp_hap_play_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct qpnp_hap *hap = container_of(timed_dev, struct qpnp_hap,
					 timed_dev);
	char str[QPNP_HAP_STR_SIZE + 1];
	int rc = 0, temp, old_mode, i;

	if (snprintf(str, QPNP_HAP_STR_SIZE, "%s", buf) > QPNP_HAP_STR_SIZE)
		return -EINVAL;

	for (i = 0; i < strlen(str); i++) {
		if (str[i] == ' ' || str[i] == '\n' || str[i] == '\t') {
			str[i] = '\0';
			break;
		}
	}
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

	if (temp == QPNP_HAP_BUFFER && !hap->buffer_cfg_state) {
		rc = qpnp_hap_parse_buffer_dt(hap);
		if (!rc)
			rc = qpnp_hap_buffer_config(hap);
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

/* sysfs attributes */
static struct device_attribute qpnp_hap_attrs[] = {
	__ATTR(wf_s0, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_wf_s0_show,
			qpnp_hap_wf_s0_store),
	__ATTR(wf_s1, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_wf_s1_show,
			qpnp_hap_wf_s1_store),
	__ATTR(wf_s2, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_wf_s2_show,
			qpnp_hap_wf_s2_store),
	__ATTR(wf_s3, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_wf_s3_show,
			qpnp_hap_wf_s3_store),
	__ATTR(wf_s4, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_wf_s4_show,
			qpnp_hap_wf_s4_store),
	__ATTR(wf_s5, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_wf_s5_show,
			qpnp_hap_wf_s5_store),
	__ATTR(wf_s6, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_wf_s6_show,
			qpnp_hap_wf_s6_store),
	__ATTR(wf_s7, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_wf_s7_show,
			qpnp_hap_wf_s7_store),
	__ATTR(wf_update, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_wf_update_show,
			qpnp_hap_wf_update_store),
	__ATTR(wf_rep, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_wf_rep_show,
			qpnp_hap_wf_rep_store),
	__ATTR(wf_s_rep, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_wf_s_rep_show,
			qpnp_hap_wf_s_rep_store),
	__ATTR(play_mode, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_play_mode_show,
			qpnp_hap_play_mode_store),
	__ATTR(dump_regs, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_dump_regs_show,
			NULL),
	__ATTR(ramp_test, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_ramp_test_data_show,
			qpnp_hap_ramp_test_data_store),
	__ATTR(min_max_test, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_hap_min_max_test_data_show,
			qpnp_hap_min_max_test_data_store),
};

static void calculate_lra_code(struct qpnp_hap *hap)
{
	u8 play_rate_code_lo, play_rate_code_hi;
	int play_rate_code, neg_idx = 0, pos_idx = LRA_POS_FREQ_COUNT-1;
	int lra_init_freq, freq_variation, start_variation = AUTO_RES_ERR_MAX;

	qpnp_hap_read_reg(hap, &play_rate_code_lo,
				QPNP_HAP_RATE_CFG1_REG(hap->base));
	qpnp_hap_read_reg(hap, &play_rate_code_hi,
				QPNP_HAP_RATE_CFG2_REG(hap->base));

	play_rate_code = (play_rate_code_hi << 8) | (play_rate_code_lo & 0xff);

	lra_init_freq = 200000 / play_rate_code;

	while (start_variation >= AUTO_RES_ERR_CAPTURE_RES) {
		freq_variation = (lra_init_freq * start_variation) / 100;
		lra_play_rate_code[neg_idx++] = 200000 / (lra_init_freq -
					freq_variation);
		lra_play_rate_code[pos_idx--] = 200000 / (lra_init_freq +
					freq_variation);
		start_variation -= AUTO_RES_ERR_CAPTURE_RES;
	}
}

static int qpnp_hap_auto_res_enable(struct qpnp_hap *hap, int enable)
{
	int rc = 0;
	u8 val;

	rc = qpnp_hap_read_reg(hap, &val, QPNP_HAP_TEST2_REG(hap->base));
	if (rc < 0)
		return rc;
	val &= QPNP_HAP_TEST2_AUTO_RES_MASK;

	if (enable)
		val |= AUTO_RES_ENABLE;
	else
		val |= AUTO_RES_DISABLE;

	/* TEST2 is a secure access register */
	rc = qpnp_hap_sec_access(hap);
	if (rc)
		return rc;

	rc = qpnp_hap_write_reg(hap, &val, QPNP_HAP_TEST2_REG(hap->base));
	if (rc)
		return rc;

	return 0;
}

static void update_lra_frequency(struct qpnp_hap *hap)
{
	u8 lra_auto_res_lo = 0, lra_auto_res_hi = 0;

	qpnp_hap_read_reg(hap, &lra_auto_res_lo,
				QPNP_HAP_LRA_AUTO_RES_LO(hap->base));
	qpnp_hap_read_reg(hap, &lra_auto_res_hi,
				QPNP_HAP_LRA_AUTO_RES_HI(hap->base));

	if (lra_auto_res_lo && lra_auto_res_hi) {
		qpnp_hap_write_reg(hap, &lra_auto_res_lo,
				QPNP_HAP_RATE_CFG1_REG(hap->base));

		lra_auto_res_hi = lra_auto_res_hi >> 4;
		qpnp_hap_write_reg(hap, &lra_auto_res_hi,
				QPNP_HAP_RATE_CFG2_REG(hap->base));
	}
}

static enum hrtimer_restart detect_auto_res_error(struct hrtimer *timer)
{
	struct qpnp_hap *hap = container_of(timer, struct qpnp_hap,
					auto_res_err_poll_timer);
	u8 val;
	ktime_t currtime;

	qpnp_hap_read_reg(hap, &val, QPNP_HAP_STATUS(hap->base));

	if (val & AUTO_RES_ERR_BIT) {
		schedule_work(&hap->auto_res_err_work);
		return HRTIMER_NORESTART;
	} else {
		update_lra_frequency(hap);
		currtime  = ktime_get();
		hrtimer_forward(&hap->auto_res_err_poll_timer, currtime,
				ktime_set(0, POLL_TIME_AUTO_RES_ERR_NS));
		return HRTIMER_RESTART;
	}
}

static void correct_auto_res_error(struct work_struct *auto_res_err_work)
{
	struct qpnp_hap *hap = container_of(auto_res_err_work,
				struct qpnp_hap, auto_res_err_work);

	u8 lra_code_lo, lra_code_hi, disable_hap = 0x00;
	static int lra_freq_index;
	ktime_t currtime, remaining_time;
	int temp, rem = 0, index = lra_freq_index % LRA_POS_FREQ_COUNT;

	if (hrtimer_active(&hap->hap_timer)) {
		remaining_time  = hrtimer_get_remaining(&hap->hap_timer);
		rem = (int)ktime_to_us(remaining_time);
	}

	qpnp_hap_play(hap, 0);
	qpnp_hap_write_reg(hap, &disable_hap,
				QPNP_HAP_EN_CTL_REG(hap->base));

	lra_code_lo = lra_play_rate_code[index] & QPNP_HAP_RATE_CFG1_MASK;
	qpnp_hap_write_reg(hap, &lra_code_lo,
				QPNP_HAP_RATE_CFG1_REG(hap->base));

	qpnp_hap_read_reg(hap, &lra_code_hi,
				QPNP_HAP_RATE_CFG2_REG(hap->base));

	lra_code_hi &= QPNP_HAP_RATE_CFG2_MASK;
	temp = lra_play_rate_code[index] >> QPNP_HAP_RATE_CFG2_SHFT;
	lra_code_hi |= temp;

	qpnp_hap_write_reg(hap, &lra_code_hi,
					QPNP_HAP_RATE_CFG2_REG(hap->base));

	lra_freq_index++;

	if (rem > 0) {
		currtime = ktime_get();
		hap->state = 1;
		hrtimer_forward(&hap->hap_timer, currtime, remaining_time);
		schedule_work(&hap->work);
	}
}

/* set api for haptics */
static int qpnp_hap_set(struct qpnp_hap *hap, int on)
{
	int rc = 0;
	u8 val = 0;
	unsigned long timeout_ns = POLL_TIME_AUTO_RES_ERR_NS;
	u32 back_emf_delay_us = hap->time_required_to_generate_back_emf_us;

	if (hap->play_mode == QPNP_HAP_PWM) {
		if (on)
			rc = pwm_enable(hap->pwm_info.pwm_dev);
		else
			pwm_disable(hap->pwm_info.pwm_dev);
	} else if (hap->play_mode == QPNP_HAP_BUFFER ||
			hap->play_mode == QPNP_HAP_DIRECT) {
		if (on) {
			/*
			 * For auto resonance detection to work properly,
			 * sufficient back-emf has to be generated. In general,
			 * back-emf takes some time to build up. When the auto
			 * resonance mode is chosen as QWD, high-z will be
			 * applied for every LRA cycle and hence there won't be
			 * enough back-emf at the start-up. Hence, the motor
			 * needs to vibrate for few LRA cycles after the PLAY
			 * bit is asserted. So disable the auto resonance here
			 * and enable it after the sleep of
			 * 'time_required_to_generate_back_emf_us' is completed.
			 */
			if (hap->correct_lra_drive_freq ||
				hap->auto_res_mode == QPNP_HAP_AUTO_RES_QWD)
				qpnp_hap_auto_res_enable(hap, 0);

			rc = qpnp_hap_mod_enable(hap, on);
			if (rc < 0)
				return rc;

			rc = qpnp_hap_play(hap, on);

			if (hap->act_type == QPNP_HAP_LRA &&
				(hap->correct_lra_drive_freq ||
				hap->auto_res_mode == QPNP_HAP_AUTO_RES_QWD)) {
				usleep_range(back_emf_delay_us,
							back_emf_delay_us + 1);
				rc = qpnp_hap_auto_res_enable(hap, 1);
				if (rc < 0)
					return rc;
			}
			if (hap->act_type == QPNP_HAP_LRA &&
						hap->correct_lra_drive_freq) {
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

			if (hap->correct_lra_drive_freq) {
				rc = qpnp_hap_read_reg(hap, &val,
						QPNP_HAP_STATUS(hap->base));
				if (!(val & AUTO_RES_ERR_BIT))
					update_lra_frequency(hap);
			}

			rc = qpnp_hap_mod_enable(hap, on);
			if (hap->act_type == QPNP_HAP_LRA &&
					hap->correct_lra_drive_freq) {
				hrtimer_cancel(&hap->auto_res_err_poll_timer);
				calculate_lra_code(hap);
			}
		}
	}

	return rc;
}

/* enable interface from timed output class */
static void qpnp_hap_td_enable(struct timed_output_dev *dev, int value)
{
	struct qpnp_hap *hap = container_of(dev, struct qpnp_hap,
					 timed_dev);

	mutex_lock(&hap->lock);
	hrtimer_cancel(&hap->hap_timer);

	if (value == 0) {
		if (hap->state == 0) {
			mutex_unlock(&hap->lock);
			return;
		}
		hap->state = 0;
	} else {
		value = (value > hap->timeout_ms ?
				 hap->timeout_ms : value);
		hap->state = 1;
		hrtimer_start(&hap->hap_timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000),
			      HRTIMER_MODE_REL);
	}
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
		dev_err(&hap->spmi->dev, "only PWM mode is supported\n");
		return -EINVAL;
	}

	rc = qpnp_hap_set(hap, false);
	if (rc)
		return rc;

	if (!on) {
		/* set the pwm back to original duty for normal operations */
		/* this is not required if standard interface is not used */
		rc = pwm_config(hap->pwm_info.pwm_dev,
				hap->pwm_info.duty_us * NSEC_PER_USEC,
				hap->pwm_info.period_us * NSEC_PER_USEC);
		return rc;
	}

	/* pwm values range from 0x00 to 0xff. The range from 0x00 to 0x7f
	   provides a postive amplitude in the sin wave form for 0 to 100%.
	   The range from 0x80 to 0xff provides a negative amplitude in the
	   sin wave form for 0 to 100%. Here the duty percentage is calculated
	   based on the incoming data to accommodate this. */
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

	dev_dbg(&hap->spmi->dev, "data=0x%x duty_per=%d\n", data, duty_percent);

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
	int rc, reg_en;

	if (hap->vcc_pon) {
		reg_en = regulator_enable(hap->vcc_pon);
		if (reg_en)
			pr_err("%s: could not enable vcc_pon regulator\n",
				 __func__);
	}

	/* Disable haptics module if the duration of short circuit
	 * exceeds the maximum limit (5 secs).
	 */
	if (hap->sc_duration == SC_MAX_DURATION) {
		rc = qpnp_hap_write_reg(hap, &val,
				QPNP_HAP_EN_CTL_REG(hap->base));
	} else {
		if (hap->play_mode == QPNP_HAP_PWM)
			qpnp_hap_mod_enable(hap, hap->state);
		qpnp_hap_set(hap, hap->state);
	}

	if (hap->vcc_pon && !reg_en) {
		rc = regulator_disable(hap->vcc_pon);
		if (rc)
			pr_err("%s: could not disable vcc_pon regulator\n",
				 __func__);
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
	qpnp_hap_set(hap, 0);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(qpnp_haptic_pm_ops, qpnp_haptic_suspend, NULL);

/* Configuration api for haptics registers */
static int qpnp_hap_config(struct qpnp_hap *hap)
{
	u8 reg = 0, error_code = 0, unlock_val, rc_clk_err_percent_x10;
	u32 temp;
	int rc, i;

	/* Configure the ACTUATOR TYPE register */
	rc = qpnp_hap_read_reg(hap, &reg, QPNP_HAP_ACT_TYPE_REG(hap->base));
	if (rc < 0)
		return rc;
	reg &= QPNP_HAP_ACT_TYPE_MASK;
	reg |= hap->act_type;
	rc = qpnp_hap_write_reg(hap, &reg, QPNP_HAP_ACT_TYPE_REG(hap->base));
	if (rc)
		return rc;

	/* Configure auto resonance parameters */
	if (hap->act_type == QPNP_HAP_LRA) {
		if (hap->lra_res_cal_period < QPNP_HAP_RES_CAL_PERIOD_MIN)
			hap->lra_res_cal_period = QPNP_HAP_RES_CAL_PERIOD_MIN;
		else if (hap->lra_res_cal_period > QPNP_HAP_RES_CAL_PERIOD_MAX)
			hap->lra_res_cal_period = QPNP_HAP_RES_CAL_PERIOD_MAX;

		rc = qpnp_hap_read_reg(hap, &reg,
					QPNP_HAP_LRA_AUTO_RES_REG(hap->base));
		if (rc < 0)
			return rc;
		reg &= QPNP_HAP_AUTO_RES_MODE_MASK;
		reg |= (hap->auto_res_mode << QPNP_HAP_AUTO_RES_MODE_SHIFT);
		reg &= QPNP_HAP_LRA_HIGH_Z_MASK;
		reg |= (hap->lra_high_z << QPNP_HAP_LRA_HIGH_Z_SHIFT);
		reg &= QPNP_HAP_LRA_RES_CAL_PER_MASK;
		temp = fls(hap->lra_res_cal_period) - 1;
		reg |= (temp - 2);
		rc = qpnp_hap_write_reg(hap, &reg,
					QPNP_HAP_LRA_AUTO_RES_REG(hap->base));
		if (rc)
			return rc;
	} else {
		/* disable auto resonance for ERM */
		reg = 0x00;

		rc = qpnp_hap_write_reg(hap, &reg,
					QPNP_HAP_LRA_AUTO_RES_REG(hap->base));
		if (rc)
			return rc;
	}

	/* Configure the PLAY MODE register */
	rc = qpnp_hap_play_mode_config(hap);
	if (rc)
		return rc;

	/* Configure the VMAX register */
	rc = qpnp_hap_vmax_config(hap);
	if (rc)
		return rc;

	/* Configure the ILIM register */
	if (hap->ilim_ma < QPNP_HAP_ILIM_MIN_MA)
		hap->ilim_ma = QPNP_HAP_ILIM_MIN_MA;
	else if (hap->ilim_ma > QPNP_HAP_ILIM_MAX_MA)
		hap->ilim_ma = QPNP_HAP_ILIM_MAX_MA;

	rc = qpnp_hap_read_reg(hap, &reg, QPNP_HAP_ILIM_REG(hap->base));
	if (rc < 0)
		return rc;
	reg &= QPNP_HAP_ILIM_MASK;
	temp = (hap->ilim_ma / QPNP_HAP_ILIM_MIN_MA) >> 1;
	reg |= temp;
	rc = qpnp_hap_write_reg(hap, &reg, QPNP_HAP_ILIM_REG(hap->base));
	if (rc)
		return rc;

	/* Configure the short circuit debounce register */
	rc = qpnp_hap_sc_deb_config(hap);
	if (rc)
		return rc;

	/* Configure the INTERNAL_PWM register */
	if (hap->int_pwm_freq_khz <= QPNP_HAP_INT_PWM_FREQ_253_KHZ) {
		hap->int_pwm_freq_khz = QPNP_HAP_INT_PWM_FREQ_253_KHZ;
		temp = 0;
	} else if (hap->int_pwm_freq_khz <= QPNP_HAP_INT_PWM_FREQ_505_KHZ) {
		hap->int_pwm_freq_khz = QPNP_HAP_INT_PWM_FREQ_505_KHZ;
		temp = 1;
	} else if (hap->int_pwm_freq_khz <= QPNP_HAP_INT_PWM_FREQ_739_KHZ) {
		hap->int_pwm_freq_khz = QPNP_HAP_INT_PWM_FREQ_739_KHZ;
		temp = 2;
	} else {
		hap->int_pwm_freq_khz = QPNP_HAP_INT_PWM_FREQ_1076_KHZ;
		temp = 3;
	}

	rc = qpnp_hap_read_reg(hap, &reg, QPNP_HAP_INT_PWM_REG(hap->base));
	if (rc < 0)
		return rc;
	reg &= QPNP_HAP_INT_PWM_MASK;
	reg |= temp;
	rc = qpnp_hap_write_reg(hap, &reg, QPNP_HAP_INT_PWM_REG(hap->base));
	if (rc)
		return rc;

	rc = qpnp_hap_read_reg(hap, &reg, QPNP_HAP_PWM_CAP_REG(hap->base));
	if (rc < 0)
		return rc;
	reg &= QPNP_HAP_INT_PWM_MASK;
	reg |= temp;
	rc = qpnp_hap_write_reg(hap, &reg, QPNP_HAP_PWM_CAP_REG(hap->base));
	if (rc)
		return rc;

	/* Configure the WAVE SHAPE register */
	rc = qpnp_hap_read_reg(hap, &reg,
			QPNP_HAP_WAV_SHAPE_REG(hap->base));
	if (rc < 0)
		return rc;
	reg &= QPNP_HAP_WAV_SHAPE_MASK;
	reg |= hap->wave_shape;
	rc = qpnp_hap_write_reg(hap, &reg,
			QPNP_HAP_WAV_SHAPE_REG(hap->base));
	if (rc)
		return rc;

	/* Configure RATE_CFG1 and RATE_CFG2 registers */
	/* Note: For ERM these registers act as play rate and
	   for LRA these represent resonance period */
	if (hap->wave_play_rate_us < QPNP_HAP_WAV_PLAY_RATE_US_MIN)
		hap->wave_play_rate_us = QPNP_HAP_WAV_PLAY_RATE_US_MIN;
	else if (hap->wave_play_rate_us > QPNP_HAP_WAV_PLAY_RATE_US_MAX)
		hap->wave_play_rate_us = QPNP_HAP_WAV_PLAY_RATE_US_MAX;

	temp = hap->wave_play_rate_us / QPNP_HAP_RATE_CFG_STEP_US;

	/*
	 * The frequency of 19.2Mzhz RC clock is subject to variation. Currently
	 * PMI8950 and PMI8937 modules have MISC_TRIM_ERROR_RC19P2_CLK register
	 * present in the MISC  block. This register holds the frequency error
	 * in 19.2Mhz RC clock.
	 */
	if (hap->act_type == QPNP_HAP_LRA
			&& hap->misc_trim_error_rc19p2_clk_reg_present) {
		unlock_val = MISC_SEC_UNLOCK;
		rc = spmi_ext_register_writel(hap->spmi->ctrl,
				PMI8950_MISC_SID, MISC_SEC_ACCESS,
				&unlock_val, 1);
		if (rc)
			dev_err(&hap->spmi->dev,
				"Unable to do SEC_ACCESS rc:%d\n", rc);

		spmi_ext_register_readl(hap->spmi->ctrl, PMI8950_MISC_SID,
			 MISC_TRIM_ERROR_RC19P2_CLK, &error_code, 1);

		rc_clk_err_percent_x10 = (error_code & 0x0F) * 7;

		/*
		 * If the TRIM register holds value less than 0x80,
		 * then there is a positive error in the RC clock.
		 * If the TRIM register holds value greater than or equal to
		 * 0x80, then there is a negative error in the RC clock.
		 * The adjusted play rate code is calculated as follows:
		 * LRA_drive_period_code = (200Khz * (1+%error/100)) / LRA_freq
		 */
		if (error_code >= 128)
			temp = (temp * (1000 - rc_clk_err_percent_x10)) / 1000;
		else
			temp = (temp * (1000 + rc_clk_err_percent_x10)) / 1000;

		dev_dbg(&hap->spmi->dev,
			"TRIM register = 0x%x Play rate code after RC clock correction 0x%x\n",
					error_code, temp);
	}

	reg = temp & QPNP_HAP_RATE_CFG1_MASK;
	rc = qpnp_hap_write_reg(hap, &reg,
			QPNP_HAP_RATE_CFG1_REG(hap->base));
	if (rc)
		return rc;

	rc = qpnp_hap_read_reg(hap, &reg,
			QPNP_HAP_RATE_CFG2_REG(hap->base));
	if (rc < 0)
		return rc;
	reg &= QPNP_HAP_RATE_CFG2_MASK;
	temp = temp >> QPNP_HAP_RATE_CFG2_SHFT;
	reg |= temp;
	rc = qpnp_hap_write_reg(hap, &reg,
			QPNP_HAP_RATE_CFG2_REG(hap->base));
	if (rc)
		return rc;

	if ((hap->act_type == QPNP_HAP_LRA) && hap->correct_lra_drive_freq)
		calculate_lra_code(hap);

	/* Configure BRAKE register */
	rc = qpnp_hap_read_reg(hap, &reg, QPNP_HAP_EN_CTL2_REG(hap->base));
	if (rc < 0)
		return rc;
	reg &= QPNP_HAP_BRAKE_MASK;
	reg |= hap->en_brake;
	rc = qpnp_hap_write_reg(hap, &reg, QPNP_HAP_EN_CTL2_REG(hap->base));
	if (rc)
		return rc;

	if (hap->en_brake && hap->sup_brake_pat) {
		for (i = QPNP_HAP_BRAKE_PAT_LEN - 1, reg = 0; i >= 0; i--) {
			hap->brake_pat[i] &= QPNP_HAP_BRAKE_PAT_MASK;
			temp = i << 1;
			reg |= hap->brake_pat[i] << temp;
		}
		rc = qpnp_hap_write_reg(hap, &reg,
					QPNP_HAP_BRAKE_REG(hap->base));
		if (rc)
			return rc;
	}

	/* Cache enable control register */
	rc = qpnp_hap_read_reg(hap, &reg, QPNP_HAP_EN_CTL_REG(hap->base));
	if (rc < 0)
		return rc;
	hap->reg_en_ctl = reg;

	/* Cache play register */
	rc = qpnp_hap_read_reg(hap, &reg, QPNP_HAP_PLAY_REG(hap->base));
	if (rc < 0)
		return rc;
	hap->reg_play = reg;

	if (hap->play_mode == QPNP_HAP_BUFFER)
		rc = qpnp_hap_buffer_config(hap);
	else if (hap->play_mode == QPNP_HAP_PWM)
		rc = qpnp_hap_pwm_config(hap);
	else if (hap->play_mode == QPNP_HAP_AUDIO)
		rc = qpnp_hap_mod_enable(hap, true);

	if (rc)
		return rc;

	/* setup short circuit irq */
	if (hap->use_sc_irq) {
		rc = devm_request_threaded_irq(&hap->spmi->dev, hap->sc_irq,
			NULL, qpnp_hap_sc_irq,
			QPNP_IRQ_FLAGS,
			"qpnp_sc_irq", hap);
		if (rc < 0) {
			dev_err(&hap->spmi->dev,
				"Unable to request sc(%d) IRQ(err:%d)\n",
				hap->sc_irq, rc);
			return rc;
		}
	}

	hap->sc_duration = 0;

	return rc;
}

/* DT parsing for haptics parameters */
static int qpnp_hap_parse_dt(struct qpnp_hap *hap)
{
	struct spmi_device *spmi = hap->spmi;
	struct property *prop;
	const char *temp_str;
	u32 temp;
	int rc;

	hap->timeout_ms = QPNP_HAP_TIMEOUT_MS_MAX;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,timeout-ms", &temp);
	if (!rc) {
		hap->timeout_ms = temp;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read timeout\n");
		return rc;
	}

	hap->act_type = QPNP_HAP_LRA;
	rc = of_property_read_string(spmi->dev.of_node,
			"qcom,actuator-type", &temp_str);
	if (!rc) {
		if (strcmp(temp_str, "erm") == 0)
			hap->act_type = QPNP_HAP_ERM;
		else if (strcmp(temp_str, "lra") == 0)
			hap->act_type = QPNP_HAP_LRA;
		else {
			dev_err(&spmi->dev, "Invalid actuator type\n");
			return -EINVAL;
		}
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read actuator type\n");
		return rc;
	}

	if (hap->act_type == QPNP_HAP_LRA) {
		hap->auto_res_mode = QPNP_HAP_AUTO_RES_ZXD_EOP;
		rc = of_property_read_string(spmi->dev.of_node,
				"qcom,lra-auto-res-mode", &temp_str);
		if (!rc) {
			if (strcmp(temp_str, "none") == 0)
				hap->auto_res_mode = QPNP_HAP_AUTO_RES_NONE;
			else if (strcmp(temp_str, "zxd") == 0)
				hap->auto_res_mode = QPNP_HAP_AUTO_RES_ZXD;
			else if (strcmp(temp_str, "qwd") == 0)
				hap->auto_res_mode = QPNP_HAP_AUTO_RES_QWD;
			else if (strcmp(temp_str, "max-qwd") == 0)
				hap->auto_res_mode = QPNP_HAP_AUTO_RES_MAX_QWD;
			else
				hap->auto_res_mode = QPNP_HAP_AUTO_RES_ZXD_EOP;
		} else if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read auto res mode\n");
			return rc;
		}

		hap->lra_high_z = QPNP_HAP_LRA_HIGH_Z_OPT3;
		rc = of_property_read_string(spmi->dev.of_node,
				"qcom,lra-high-z", &temp_str);
		if (!rc) {
			if (strcmp(temp_str, "none") == 0)
				hap->lra_high_z = QPNP_HAP_LRA_HIGH_Z_NONE;
			else if (strcmp(temp_str, "opt1") == 0)
				hap->lra_high_z = QPNP_HAP_LRA_HIGH_Z_OPT1;
			else if (strcmp(temp_str, "opt2") == 0)
				hap->lra_high_z = QPNP_HAP_LRA_HIGH_Z_OPT2;
			else
				hap->lra_high_z = QPNP_HAP_LRA_HIGH_Z_OPT3;
		} else if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read LRA high-z\n");
			return rc;
		}

		hap->lra_res_cal_period = QPNP_HAP_RES_CAL_PERIOD_MAX;
		rc = of_property_read_u32(spmi->dev.of_node,
				"qcom,lra-res-cal-period", &temp);
		if (!rc) {
			hap->lra_res_cal_period = temp;
		} else if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read cal period\n");
			return rc;
		}

		hap->correct_lra_drive_freq =
				of_property_read_bool(spmi->dev.of_node,
						"qcom,correct-lra-drive-freq");

		hap->misc_trim_error_rc19p2_clk_reg_present =
				of_property_read_bool(spmi->dev.of_node,
				"qcom,misc-trim-error-rc19p2-clk-reg-present");

		if (hap->auto_res_mode == QPNP_HAP_AUTO_RES_QWD) {
			hap->time_required_to_generate_back_emf_us =
					QPNP_HAP_TIME_REQ_FOR_BACK_EMF_GEN;
			rc = of_property_read_u32(spmi->dev.of_node,
				"qcom,time-required-to-generate-back-emf-us",
				&temp);
			if (!rc)
				hap->time_required_to_generate_back_emf_us =
									temp;
		} else {
			hap->time_required_to_generate_back_emf_us = 0;
		}
	}

	rc = of_property_read_string(spmi->dev.of_node,
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
			dev_err(&spmi->dev, "Invalid play mode\n");
			return -EINVAL;
		}
	} else {
		dev_err(&spmi->dev, "Unable to read play mode\n");
		return rc;
	}

	hap->vmax_mv = QPNP_HAP_VMAX_MAX_MV;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,vmax-mv", &temp);
	if (!rc) {
		hap->vmax_mv = temp;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read vmax\n");
		return rc;
	}

	hap->ilim_ma = QPNP_HAP_ILIM_MIN_MV;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,ilim-ma", &temp);
	if (!rc) {
		hap->ilim_ma = temp;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read ILim\n");
		return rc;
	}

	hap->sc_deb_cycles = QPNP_HAP_DEF_SC_DEB_CYCLES;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,sc-deb-cycles", &temp);
	if (!rc) {
		hap->sc_deb_cycles = temp;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read sc debounce\n");
		return rc;
	}

	hap->int_pwm_freq_khz = QPNP_HAP_INT_PWM_FREQ_505_KHZ;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,int-pwm-freq-khz", &temp);
	if (!rc) {
		hap->int_pwm_freq_khz = temp;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read int pwm freq\n");
		return rc;
	}

	hap->wave_shape = QPNP_HAP_WAV_SQUARE;
	rc = of_property_read_string(spmi->dev.of_node,
			"qcom,wave-shape", &temp_str);
	if (!rc) {
		if (strcmp(temp_str, "sine") == 0)
			hap->wave_shape = QPNP_HAP_WAV_SINE;
		else if (strcmp(temp_str, "square") == 0)
			hap->wave_shape = QPNP_HAP_WAV_SQUARE;
		else {
			dev_err(&spmi->dev, "Unsupported wav shape\n");
			return -EINVAL;
		}
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read wav shape\n");
		return rc;
	}

	hap->wave_play_rate_us = QPNP_HAP_DEF_WAVE_PLAY_RATE_US;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,wave-play-rate-us", &temp);
	if (!rc) {
		hap->wave_play_rate_us = temp;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read play rate\n");
		return rc;
	}

	if (hap->play_mode == QPNP_HAP_BUFFER)
		rc = qpnp_hap_parse_buffer_dt(hap);
	else if (hap->play_mode == QPNP_HAP_PWM)
		rc = qpnp_hap_parse_pwm_dt(hap);

	if (rc < 0)
		return rc;

	hap->en_brake = of_property_read_bool(spmi->dev.of_node,
				"qcom,en-brake");

	if (hap->en_brake) {
		prop = of_find_property(spmi->dev.of_node,
				"qcom,brake-pattern", &temp);
		if (!prop) {
			dev_info(&spmi->dev, "brake pattern not found");
		} else if (temp != QPNP_HAP_BRAKE_PAT_LEN) {
			dev_err(&spmi->dev, "Invalid len of brake pattern\n");
			return -EINVAL;
		} else {
			hap->sup_brake_pat = true;
			memcpy(hap->brake_pat, prop->value,
					QPNP_HAP_BRAKE_PAT_LEN);
		}
	}

	hap->use_sc_irq = of_property_read_bool(spmi->dev.of_node,
				"qcom,use-sc-irq");
	if (hap->use_sc_irq) {
		hap->sc_irq = spmi_get_irq_byname(hap->spmi,
					NULL, "sc-irq");
		if (hap->sc_irq < 0) {
			dev_err(&spmi->dev, "Unable to get sc irq\n");
			return hap->sc_irq;
		}
	}

	if (of_find_property(spmi->dev.of_node, "vcc_pon-supply", NULL))
		hap->manage_pon_supply = true;

	return 0;
}

static int qpnp_haptic_probe(struct spmi_device *spmi)
{
	struct qpnp_hap *hap;
	struct resource *hap_resource;
	struct regulator *vcc_pon;
	int rc, i;

	hap = devm_kzalloc(&spmi->dev, sizeof(*hap), GFP_KERNEL);
	if (!hap)
		return -ENOMEM;

	hap->spmi = spmi;

	hap_resource = spmi_get_resource(spmi, 0, IORESOURCE_MEM, 0);
	if (!hap_resource) {
		dev_err(&spmi->dev, "Unable to get haptic base address\n");
		return -EINVAL;
	}
	hap->base = hap_resource->start;

	dev_set_drvdata(&spmi->dev, hap);

	rc = qpnp_hap_parse_dt(hap);
	if (rc) {
		dev_err(&spmi->dev, "DT parsing failed\n");
		return rc;
	}

	rc = qpnp_hap_config(hap);
	if (rc) {
		dev_err(&spmi->dev, "hap config failed\n");
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

	if (hap->act_type == QPNP_HAP_LRA && hap->correct_lra_drive_freq) {
		INIT_WORK(&hap->auto_res_err_work, correct_auto_res_error);
		hrtimer_init(&hap->auto_res_err_poll_timer, CLOCK_MONOTONIC,
						HRTIMER_MODE_REL);
		hap->auto_res_err_poll_timer.function = detect_auto_res_error;
	}

	rc = timed_output_dev_register(&hap->timed_dev);
	if (rc < 0) {
		dev_err(&spmi->dev, "timed_output registration failed\n");
		goto timed_output_fail;
	}

	for (i = 0; i < ARRAY_SIZE(qpnp_hap_attrs); i++) {
		rc = sysfs_create_file(&hap->timed_dev.dev->kobj,
				&qpnp_hap_attrs[i].attr);
		if (rc < 0) {
			dev_err(&spmi->dev, "sysfs creation failed\n");
			goto sysfs_fail;
		}
	}

	if (hap->manage_pon_supply) {
		vcc_pon = regulator_get(&spmi->dev, "vcc_pon");
		if (IS_ERR(vcc_pon)) {
			rc = PTR_ERR(vcc_pon);
			dev_err(&spmi->dev,
				"regulator get failed vcc_pon rc=%d\n", rc);
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
	if (hap->act_type == QPNP_HAP_LRA && hap->correct_lra_drive_freq)
		hrtimer_cancel(&hap->auto_res_err_poll_timer);
	hrtimer_cancel(&hap->hap_timer);
	mutex_destroy(&hap->lock);
	mutex_destroy(&hap->wf_lock);

	return rc;
}

static int qpnp_haptic_remove(struct spmi_device *spmi)
{
	struct qpnp_hap *hap = dev_get_drvdata(&spmi->dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(qpnp_hap_attrs); i++)
		sysfs_remove_file(&hap->timed_dev.dev->kobj,
				&qpnp_hap_attrs[i].attr);

	cancel_work_sync(&hap->work);
	if (hap->act_type == QPNP_HAP_LRA && hap->correct_lra_drive_freq)
		hrtimer_cancel(&hap->auto_res_err_poll_timer);
	hrtimer_cancel(&hap->hap_timer);
	timed_output_dev_unregister(&hap->timed_dev);
	mutex_destroy(&hap->lock);
	mutex_destroy(&hap->wf_lock);
	if (hap->vcc_pon)
		regulator_put(hap->vcc_pon);

	return 0;
}

static struct of_device_id spmi_match_table[] = {
	{ .compatible = "qcom,qpnp-haptic", },
	{ },
};

static struct spmi_driver qpnp_haptic_driver = {
	.driver		= {
		.name	= "qcom,qpnp-haptic",
		.of_match_table = spmi_match_table,
		.pm	= &qpnp_haptic_pm_ops,
	},
	.probe		= qpnp_haptic_probe,
	.remove		= qpnp_haptic_remove,
};

static int __init qpnp_haptic_init(void)
{
	return spmi_driver_register(&qpnp_haptic_driver);
}
module_init(qpnp_haptic_init);

static void __exit qpnp_haptic_exit(void)
{
	return spmi_driver_unregister(&qpnp_haptic_driver);
}
module_exit(qpnp_haptic_exit);

MODULE_DESCRIPTION("qpnp haptic driver");
MODULE_LICENSE("GPL v2");
