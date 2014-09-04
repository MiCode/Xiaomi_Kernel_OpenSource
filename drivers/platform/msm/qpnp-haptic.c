/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/interrupt.h>
#include <linux/qpnp/pwm.h>
#include <linux/err.h>
#include <linux/delay.h>
#include "../../staging/android/timed_output.h"

#define QPNP_IRQ_FLAGS	(IRQF_TRIGGER_RISING | \
			IRQF_TRIGGER_FALLING | \
			IRQF_ONESHOT)

#define QPNP_HAP_STATUS(b)		(b + 0x0A)
#define QPNP_HAP_EN_CTL_REG(b)		(b + 0x46)
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
#define QPNP_HAP_BRAKE_REG(b)		(b + 0x5C)
#define QPNP_HAP_WAV_REP_REG(b)		(b + 0x5E)
#define QPNP_HAP_WAV_S_REG_BASE(b)	(b + 0x60)
#define QPNP_HAP_PLAY_REG(b)		(b + 0x70)

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
#define QPNP_HAP_WAV_SINE		0
#define QPNP_HAP_WAV_SQUARE		1
#define QPNP_HAP_WAV_SAMP_LEN		8
#define QPNP_HAP_WAV_SAMP_MAX		0x7E
#define QPNP_HAP_BRAKE_PAT_LEN		4
#define QPNP_HAP_PLAY_EN		0x80
#define QPNP_HAP_EN			0x80

#define QPNP_HAP_TIMEOUT_MS_MAX		15000
#define QPNP_HAP_STR_SIZE		20
#define QPNP_HAP_MAX_RETRIES		5

/* haptic debug register set */
static u8 qpnp_hap_dbg_regs[] = {
	0x0a, 0x0b, 0x0c, 0x46, 0x4c, 0x4d, 0x4e, 0x4f, 0x51, 0x52, 0x53,
	0x54, 0x55, 0x56, 0x57, 0x58, 0x5c, 0x5e, 0x60, 0x61, 0x62, 0x63,
	0x64, 0x65, 0x66, 0x67, 0x70
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
 *  @ timed_dev - timed output device
 *  @ work - worker
 *  @ pwm_info - pwm info
 *  @ lock - mutex lock
 *  @ wf_lock - mutex lock for waveform
 *  @ play_mode - play mode
 *  @ auto_res_mode - auto resonace mode
 *  @ lra_high_z - high z option line
 *  @ timeout_ms - max timeout in ms
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
 *  @ state - current state of haptics
 *  @ use_play_irq - play irq usage state
 *  @ use_sc_irq - short circuit irq usage state
 *  @ wf_update - waveform update flag
 *  @ pwm_cfg_state - pwm mode configuration state
 *  @ buffer_cfg_state - buffer mode configuration state
 */
struct qpnp_hap {
	struct spmi_device *spmi;
	struct hrtimer hap_timer;
	struct timed_output_dev timed_dev;
	struct work_struct work;
	struct qpnp_pwm_info pwm_info;
	struct mutex lock;
	struct mutex wf_lock;
	enum qpnp_hap_mode play_mode;
	enum qpnp_hap_auto_res_mode auto_res_mode;
	enum qpnp_hap_high_z lra_high_z;
	u32 timeout_ms;
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
	bool state;
	bool use_play_irq;
	bool use_sc_irq;
	bool wf_update;
	bool pwm_cfg_state;
	bool buffer_cfg_state;
};

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

static int qpnp_hap_mod_enable(struct qpnp_hap *hap, int on)
{
	u8 val;
	int rc, i;

	val = hap->reg_en_ctl;
	if (on) {
		val |= QPNP_HAP_EN;
	} else {
		for (i = 0; i < QPNP_HAP_MAX_RETRIES; i++) {
			rc = qpnp_hap_read_reg(hap, &val,
				QPNP_HAP_STATUS(hap->base));

			dev_dbg(&hap->spmi->dev, "HAP_STATUS=0x%x\n", val);

			/* wait for 4 cycles of play rate */
			if (val & QPNP_HAP_STATUS_BUSY)
				usleep(4 * hap->wave_play_rate_us);
			else
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
	u8 reg;

	/* clear short circuit register */
	dev_dbg(&hap->spmi->dev, "Short circuit detected\n");
	reg = QPNP_HAP_SC_CLR;
	qpnp_hap_write_reg(hap, &reg, QPNP_HAP_SC_CLR_REG(hap->base));

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

	hap->pwm_info.pwm_dev = pwm_request(hap->pwm_info.pwm_channel,
							 "qpnp-hap");
	if (IS_ERR_OR_NULL(hap->pwm_info.pwm_dev)) {
		dev_err(&hap->spmi->dev, "hap pwm request failed\n");
		return -ENODEV;
	}

	rc = pwm_config(hap->pwm_info.pwm_dev, hap->pwm_info.duty_us,
					hap->pwm_info.period_us);
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
	int rc = 0, temp, old_mode;

	if (snprintf(str, QPNP_HAP_STR_SIZE, "%s", buf) > QPNP_HAP_STR_SIZE)
		return -EINVAL;

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
};

/* set api for haptics */
static int qpnp_hap_set(struct qpnp_hap *hap, int on)
{
	int rc = 0;

	if (hap->play_mode == QPNP_HAP_PWM) {
		if (on)
			rc = pwm_enable(hap->pwm_info.pwm_dev);
		else
			pwm_disable(hap->pwm_info.pwm_dev);
	} else if (hap->play_mode == QPNP_HAP_BUFFER ||
			hap->play_mode == QPNP_HAP_DIRECT) {
		if (on) {
			rc = qpnp_hap_mod_enable(hap, on);
			if (rc < 0)
				return rc;
			rc = qpnp_hap_play(hap, on);
		} else {
			rc = qpnp_hap_play(hap, on);
			if (rc < 0)
				return rc;

			rc = qpnp_hap_mod_enable(hap, on);
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

	if (value == 0)
		hap->state = 0;
	else {
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

/* worker to opeate haptics */
static void qpnp_hap_worker(struct work_struct *work)
{
	struct qpnp_hap *hap = container_of(work, struct qpnp_hap,
					 work);
	qpnp_hap_set(hap, hap->state);
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
	u8 reg = 0;
	int rc, i, temp;

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

	/* Configure BRAKE register */
	for (i = QPNP_HAP_BRAKE_PAT_LEN - 1, reg = 0; i >= 0; i--) {
		hap->brake_pat[i] &= QPNP_HAP_BRAKE_PAT_MASK;
		temp = i << 1;
		reg |= hap->brake_pat[i] << temp;
	}
	rc = qpnp_hap_write_reg(hap, &reg, QPNP_HAP_BRAKE_REG(hap->base));
	if (rc)
		return rc;

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

	prop = of_find_property(spmi->dev.of_node,
			"qcom,brake-pattern", &temp);
	if (!prop) {
		dev_err(&spmi->dev, "brake pattern not found");
	} else if (temp != QPNP_HAP_BRAKE_PAT_LEN) {
		dev_err(&spmi->dev, "Invalid length of brake pattern\n");
		return -EINVAL;
	} else
		memcpy(hap->brake_pat, prop->value, QPNP_HAP_BRAKE_PAT_LEN);

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

	return 0;
}

static int qpnp_haptic_probe(struct spmi_device *spmi)
{
	struct qpnp_hap *hap;
	struct resource *hap_resource;
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

	hrtimer_init(&hap->hap_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hap->hap_timer.function = qpnp_hap_timer;

	hap->timed_dev.name = "vibrator";
	hap->timed_dev.get_time = qpnp_hap_get_time;
	hap->timed_dev.enable = qpnp_hap_td_enable;

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

	return 0;

sysfs_fail:
	for (i--; i >= 0; i--)
		sysfs_remove_file(&hap->timed_dev.dev->kobj,
				&qpnp_hap_attrs[i].attr);
	timed_output_dev_unregister(&hap->timed_dev);
timed_output_fail:
	cancel_work_sync(&hap->work);
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
	hrtimer_cancel(&hap->hap_timer);
	timed_output_dev_unregister(&hap->timed_dev);
	mutex_destroy(&hap->lock);
	mutex_destroy(&hap->wf_lock);

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
