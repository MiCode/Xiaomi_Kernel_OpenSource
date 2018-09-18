/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>

enum actutor_type {
	ACT_LRA,
	ACT_ERM,
};

enum lra_res_sig_shape {
	RES_SIG_SINE,
	RES_SIG_SQUARE,
};

enum lra_auto_res_mode {
	AUTO_RES_MODE_ZXD,
	AUTO_RES_MODE_QWD,
};

enum wf_src {
	INT_WF_VMAX,
	INT_WF_BUFFER,
	EXT_WF_AUDIO,
	EXT_WF_PWM,
};

enum haptics_custom_effect_param {
	CUSTOM_DATA_EFFECT_IDX,
	CUSTOM_DATA_TIMEOUT_SEC_IDX,
	CUSTOM_DATA_TIMEOUT_MSEC_IDX,
	CUSTOM_DATA_LEN,
};

/* common definitions */
#define HAP_BRAKE_PATTERN_MAX		4
#define HAP_WAVEFORM_BUFFER_MAX		8
#define HAP_VMAX_MV_DEFAULT		1800
#define HAP_VMAX_MV_MAX			3596
#define HAP_ILIM_MA_DEFAULT		400
#define HAP_ILIM_MA_MAX			800
#define HAP_PLAY_RATE_US_DEFAULT	5715
#define HAP_PLAY_RATE_US_MAX		20475
#define HAP_PLAY_RATE_US_LSB		5
#define VMAX_MIN_PLAY_TIME_US		20000
#define HAP_SC_DET_MAX_COUNT		5
#define HAP_SC_DET_TIME_US		1000000
#define FF_EFFECT_COUNT_MAX		32
#define HAP_DISABLE_DELAY_USEC		1000

/* haptics module register definitions */
#define REG_HAP_STATUS1			0x0A
#define HAP_SC_DET_BIT			BIT(3)
#define HAP_BUSY_BIT			BIT(1)

#define REG_HAP_EN_CTL1			0x46
#define HAP_EN_BIT			BIT(7)

#define REG_HAP_EN_CTL2			0x48
#define HAP_AUTO_STANDBY_EN_BIT		BIT(1)
#define HAP_BRAKE_EN_BIT		BIT(0)

#define REG_HAP_EN_CTL3			0x4A
#define HAP_HBRIDGE_EN_BIT		BIT(7)
#define HAP_PWM_SIGNAL_EN_BIT		BIT(6)
#define HAP_ILIM_EN_BIT			BIT(5)
#define HAP_ILIM_CC_EN_BIT		BIT(4)
#define HAP_AUTO_RES_RBIAS_EN_BIT	BIT(3)
#define HAP_DAC_EN_BIT			BIT(2)
#define HAP_ZX_HYST_EN_BIT		BIT(1)
#define HAP_PWM_CTL_EN_BIT		BIT(0)

#define REG_HAP_AUTO_RES_CTRL		0x4B
#define HAP_AUTO_RES_EN_BIT		BIT(7)
#define HAP_SEL_AUTO_RES_PERIOD		BIT(6)
#define HAP_AUTO_RES_CNT_ERR_DELTA_MASK	GENMASK(5, 4)
#define HAP_AUTO_RES_CNT_ERR_DELTA_SHIFT	4
#define HAP_AUTO_RES_ERR_RECOVERY_BIT	BIT(3)
#define HAP_AUTO_RES_EN_DLY_MASK	GENMASK(2, 0)
#define AUTO_RES_CNT_ERR_DELTA(x)	(x << HAP_AUTO_RES_CNT_ERR_DELTA_SHIFT)
#define AUTO_RES_EN_DLY(x)		x

#define REG_HAP_CFG1			0x4C
#define REG_HAP_CFG2			0x4D
#define HAP_LRA_RES_TYPE_BIT		BIT(0)

#define REG_HAP_SEL			0x4E
#define HAP_WF_SOURCE_MASK		GENMASK(5, 4)
#define HAP_WF_SOURCE_SHIFT		4
#define HAP_WF_TRIGGER_BIT		BIT(0)
#define HAP_WF_SOURCE_VMAX		(0 << HAP_WF_SOURCE_SHIFT)
#define HAP_WF_SOURCE_BUFFER		(1 << HAP_WF_SOURCE_SHIFT)
#define HAP_WF_SOURCE_AUDIO		(2 << HAP_WF_SOURCE_SHIFT)
#define HAP_WF_SOURCE_PWM		(3 << HAP_WF_SOURCE_SHIFT)

#define REG_HAP_AUTO_RES_CFG		0x4F
#define HAP_AUTO_RES_MODE_BIT		BIT(7)
#define HAP_AUTO_RES_MODE_SHIFT		7
#define HAP_AUTO_RES_CAL_DURATON_MASK	GENMASK(6, 5)
#define HAP_CAL_EOP_EN_BIT		BIT(3)
#define HAP_CAL_PERIOD_MASK		GENMASK(2, 0)
#define HAP_CAL_OPT3_EVERY_8_PERIOD	2

#define REG_HAP_SLEW_CFG		0x50
#define REG_HAP_VMAX_CFG		0x51
#define HAP_VMAX_SIGN_BIT		BIT(7)
#define HAP_VMAX_OVD_BIT		BIT(6)
#define HAP_VMAX_MV_MASK		GENMASK(5, 1)
#define HAP_VMAX_MV_SHIFT		1
#define HAP_VMAX_MV_LSB			116

#define REG_HAP_ILIM_CFG		0x52
#define REG_HAP_SC_DEB_CFG		0x53
#define REG_HAP_RATE_CFG1		0x54
#define REG_HAP_RATE_CFG2		0x55
#define REG_HAP_INTERNAL_PWM		0x56
#define REG_HAP_EXTERNAL_PWM		0x57
#define REG_HAP_PWM			0x58

#define REG_HAP_SC_CLR			0x59
#define HAP_SC_CLR_BIT			BIT(0)

#define REG_HAP_ZX_CFG			0x5A
#define HAP_ZX_DET_DEB_MASK		GENMASK(2, 0)
#define ZX_DET_DEB_10US			0
#define ZX_DET_DEB_20US			1
#define ZX_DET_DEB_40US			2
#define ZX_DET_DEB_80US			3

#define REG_HAP_BRAKE			0x5C
#define HAP_BRAKE_PATTERN_MASK		0x3
#define HAP_BRAKE_PATTERN_SHIFT		2

#define REG_HAP_WF_REPEAT		0x5E
#define HAP_WF_REPEAT_MASK		GENMASK(6, 4)
#define HAP_WF_REPEAT_SHIFT		4
#define HAP_WF_S_REPEAT_MASK		GENMASK(1, 0)

#define REG_HAP_WF_S1			0x60
#define HAP_WF_SIGN_BIT			BIT(7)
#define HAP_WF_OVD_BIT			BIT(6)
#define HAP_WF_AMP_BIT			GENMASK(5, 1)
#define HAP_WF_AMP_SHIFT		1

#define REG_HAP_PLAY			0x70
#define HAP_PLAY_BIT			BIT(7)

#define REG_HAP_SEC_ACCESS		0xD0

struct qti_hap_effect {
	int			id;
	u8			*pattern;
	int			pattern_length;
	u16			play_rate_us;
	u16			vmax_mv;
	u8			wf_repeat_n;
	u8			wf_s_repeat_n;
	u8			brake[HAP_BRAKE_PATTERN_MAX];
	int			brake_pattern_length;
	bool			brake_en;
	bool			lra_auto_res_disable;
};

struct qti_hap_play_info {
	struct qti_hap_effect	*effect;
	u16			vmax_mv;
	int			length_us;
	int			playing_pos;
	bool			playing_pattern;
};

struct qti_hap_config {
	enum actutor_type	act_type;
	enum lra_res_sig_shape	lra_shape;
	enum lra_auto_res_mode	lra_auto_res_mode;
	enum wf_src		ext_src;
	u16			vmax_mv;
	u16			ilim_ma;
	u16			play_rate_us;
	bool			lra_allow_variable_play_rate;
	bool			use_ext_wf_src;
};

struct qti_hap_chip {
	struct platform_device		*pdev;
	struct device			*dev;
	struct regmap			*regmap;
	struct input_dev		*input_dev;
	struct pwm_device		*pwm_dev;
	struct qti_hap_config		config;
	struct qti_hap_play_info	play;
	struct qti_hap_effect		*predefined;
	struct qti_hap_effect		constant;
	struct regulator		*vdd_supply;
	struct hrtimer			stop_timer;
	struct hrtimer			hap_disable_timer;
	struct dentry			*hap_debugfs;
	spinlock_t			bus_lock;
	ktime_t				last_sc_time;
	int				play_irq;
	int				sc_irq;
	int				effects_count;
	int				sc_det_count;
	u16				reg_base;
	bool				perm_disable;
	bool				play_irq_en;
	bool				vdd_enabled;
};

static int wf_repeat[8] = {1, 2, 4, 8, 16, 32, 64, 128};
static int wf_s_repeat[4] = {1, 2, 4, 8};

static inline bool is_secure(u8 addr)
{
	return ((addr & 0xFF) > 0xD0);
}

static int qti_haptics_read(struct qti_hap_chip *chip,
			u8 addr, u8 *val, int len)
{
	int rc = 0;
	unsigned long flags;

	spin_lock_irqsave(&chip->bus_lock, flags);

	rc = regmap_bulk_read(chip->regmap, chip->reg_base + addr, val, len);
	if (rc < 0)
		dev_err(chip->dev, "Reading addr 0x%x failed, rc=%d\n",
				addr, rc);
	spin_unlock_irqrestore(&chip->bus_lock, flags);

	return rc;
}

static int qti_haptics_write(struct qti_hap_chip *chip,
		u8 addr, u8 *val, int len)
{
	int rc = 0, i;
	unsigned long flags;

	spin_lock_irqsave(&chip->bus_lock, flags);
	if (is_secure(addr)) {
		for (i = 0; i < len; i++) {
			rc = regmap_write(chip->regmap,
					chip->reg_base + REG_HAP_SEC_ACCESS,
					0xA5);
			if (rc < 0) {
				dev_err(chip->dev, "write SEC_ACCESS failed, rc=%d\n",
						rc);
				goto unlock;
			}

			rc = regmap_write(chip->regmap,
					chip->reg_base + addr + i, val[i]);
			if (rc < 0) {
				dev_err(chip->dev, "write val 0x%x to addr 0x%x failed, rc=%d\n",
						val[i], addr + i, rc);
				goto unlock;
			}
		}
	} else {
		if (len > 1)
			rc = regmap_bulk_write(chip->regmap,
					chip->reg_base + addr, val, len);
		else
			rc = regmap_write(chip->regmap,
					chip->reg_base + addr, *val);

			if (rc < 0)
				dev_err(chip->dev, "write addr 0x%x failed, rc=%d\n",
						addr, rc);
	}

	for (i = 0; i < len; i++)
		dev_dbg(chip->dev, "Update addr 0x%x to val 0x%x\n",
				addr + i, val[i]);

unlock:
	spin_unlock_irqrestore(&chip->bus_lock, flags);
	return rc;
}

static int qti_haptics_masked_write(struct qti_hap_chip *chip, u8 addr,
		u8 mask, u8 val)
{
	int rc;
	unsigned long flags;

	spin_lock_irqsave(&chip->bus_lock, flags);
	if (is_secure(addr)) {
		rc = regmap_write(chip->regmap,
				chip->reg_base + REG_HAP_SEC_ACCESS,
				0xA5);
		if (rc < 0) {
			dev_err(chip->dev, "write SEC_ACCESS failed, rc=%d\n",
					rc);
			goto unlock;
		}
	}

	rc = regmap_update_bits(chip->regmap, chip->reg_base + addr, mask, val);
	if (rc < 0)
		dev_err(chip->dev, "Update addr 0x%x to val 0x%x with mask 0x%x failed, rc=%d\n",
				addr, val, mask, rc);

	dev_dbg(chip->dev, "Update addr 0x%x to val 0x%x with mask 0x%x\n",
			addr, val, mask);
unlock:
	spin_unlock_irqrestore(&chip->bus_lock, flags);

	return rc;
}

static void construct_constant_waveform_in_pattern(
		struct qti_hap_play_info *play)
{
	struct qti_hap_chip *chip = container_of(play,
			struct qti_hap_chip, play);
	struct qti_hap_config *config = &chip->config;
	struct qti_hap_effect *effect = play->effect;
	int total_samples, samples, left, magnitude, i, j, k;
	int delta = INT_MAX, delta_min = INT_MAX;

	/* Using play_rate_us in config for constant waveform */
	effect->play_rate_us = config->play_rate_us;
	total_samples = play->length_us / effect->play_rate_us;
	left = play->length_us % effect->play_rate_us;

	if (total_samples <= HAP_WAVEFORM_BUFFER_MAX) {
		effect->pattern_length = total_samples;
		effect->wf_s_repeat_n = 0;
		effect->wf_repeat_n = 0;
	} else {
		/*
		 * Find a closest setting to achieve the constant waveform
		 * with the required length by using buffer waveform source:
		 * play_length_us = pattern_length * wf_s_repeat_n
		 *		* wf_repeat_n * play_rate_us
		 */
		for (i = 0; i < ARRAY_SIZE(wf_repeat); i++) {
			for (j = 0; j < ARRAY_SIZE(wf_s_repeat); j++) {
				for (k = 1; k <= HAP_WAVEFORM_BUFFER_MAX; k++) {
					samples = k * wf_s_repeat[j] *
						wf_repeat[i];
					delta = abs(total_samples - samples);
					if (delta < delta_min) {
						delta_min = delta;
						effect->pattern_length = k;
						effect->wf_s_repeat_n = j;
						effect->wf_repeat_n = i;
					}
					if (samples > total_samples)
						break;
				}
			}
		}
	}

	if (left > 0 && effect->pattern_length < HAP_WAVEFORM_BUFFER_MAX)
		effect->pattern_length++;

	play->length_us = effect->pattern_length * effect->play_rate_us;
	dev_dbg(chip->dev, "total_samples = %d, pattern_length = %d, wf_s_repeat = %d, wf_repeat = %d\n",
			total_samples, effect->pattern_length,
			wf_s_repeat[effect->wf_s_repeat_n],
			wf_repeat[effect->wf_repeat_n]);

	for (i = 0; i < effect->pattern_length; i++) {
		magnitude = play->vmax_mv / HAP_VMAX_MV_LSB;
		effect->pattern[i] = (u8)magnitude << HAP_WF_AMP_SHIFT;
	}
}

static int qti_haptics_config_wf_buffer(struct qti_hap_chip *chip)
{
	struct qti_hap_play_info *play = &chip->play;
	struct qti_hap_effect *effect = play->effect;
	u8 addr, pattern[HAP_WAVEFORM_BUFFER_MAX] = {0};
	int rc = 0;
	size_t len;

	if (play->playing_pos == effect->pattern_length) {
		dev_dbg(chip->dev, "pattern playing done\n");
		return 0;
	}

	if (effect->pattern_length - play->playing_pos
			>= HAP_WAVEFORM_BUFFER_MAX)
		len = HAP_WAVEFORM_BUFFER_MAX;
	else
		len = effect->pattern_length - play->playing_pos;

	dev_dbg(chip->dev, "copy %d bytes start from %d\n",
			(int)len, play->playing_pos);
	memcpy(pattern, &effect->pattern[play->playing_pos], len);

	play->playing_pos += len;

	addr = REG_HAP_WF_S1;
	rc = qti_haptics_write(chip, REG_HAP_WF_S1, pattern,
			HAP_WAVEFORM_BUFFER_MAX);
	if (rc < 0)
		dev_err(chip->dev, "Program WF_SAMPLE failed, rc=%d\n", rc);

	return rc;
}

static int qti_haptics_config_wf_repeat(struct qti_hap_chip *chip)
{
	struct qti_hap_effect *effect = chip->play.effect;
	u8 addr, mask, val;
	int rc = 0;

	addr = REG_HAP_WF_REPEAT;
	mask = HAP_WF_REPEAT_MASK | HAP_WF_S_REPEAT_MASK;
	val = effect->wf_repeat_n << HAP_WF_REPEAT_SHIFT;
	val |= effect->wf_s_repeat_n;
	rc = qti_haptics_masked_write(chip, addr, mask, val);
	if (rc < 0)
		dev_err(chip->dev, "Program WF_REPEAT failed, rc=%d\n", rc);

	return rc;
}

static int qti_haptics_play(struct qti_hap_chip *chip, bool play)
{
	int rc = 0;
	u8 val = play ? HAP_PLAY_BIT : 0;

	rc = qti_haptics_write(chip,
			REG_HAP_PLAY, &val, 1);
	if (rc < 0)
		dev_err(chip->dev, "%s playing haptics failed, rc=%d\n",
				play ? "start" : "stop", rc);

	return rc;
}

static int qti_haptics_module_en(struct qti_hap_chip *chip, bool en)
{
	int rc = 0;
	u8 val = en ? HAP_EN_BIT : 0;

	rc = qti_haptics_write(chip,
			REG_HAP_EN_CTL1, &val, 1);
	if (rc < 0)
		dev_err(chip->dev, "%s haptics failed, rc=%d\n",
				en ? "enable" : "disable", rc);


	return rc;
}

static int qti_haptics_config_vmax(struct qti_hap_chip *chip, int vmax_mv)
{
	u8 addr, mask, val;
	int rc;

	addr = REG_HAP_VMAX_CFG;
	mask = HAP_VMAX_MV_MASK;
	val = (vmax_mv / HAP_VMAX_MV_LSB) << HAP_VMAX_MV_SHIFT;
	rc = qti_haptics_masked_write(chip, addr, mask, val);
	if (rc < 0)
		dev_err(chip->dev, "write VMAX_CFG failed, rc=%d\n",
				rc);

	return rc;
}

static int qti_haptics_config_wf_src(struct qti_hap_chip *chip,
						enum wf_src src)
{
	u8 addr, mask, val = 0;
	int rc;

	addr = REG_HAP_SEL;
	mask = HAP_WF_SOURCE_MASK | HAP_WF_TRIGGER_BIT;
	val = src << HAP_WF_SOURCE_SHIFT;
	if (src == EXT_WF_AUDIO || src == EXT_WF_PWM)
		val |= HAP_WF_TRIGGER_BIT;

	rc = qti_haptics_masked_write(chip, addr, mask, val);
	if (rc < 0)
		dev_err(chip->dev, "set HAP_SEL failed, rc=%d\n", rc);

	return rc;
}

static int qti_haptics_config_play_rate_us(struct qti_hap_chip *chip,
						int play_rate_us)
{
	u8 addr, val[2];
	int tmp, rc;

	addr = REG_HAP_RATE_CFG1;
	tmp = play_rate_us / HAP_PLAY_RATE_US_LSB;
	val[0] = tmp & 0xff;
	val[1] = (tmp >> 8) & 0xf;
	rc = qti_haptics_write(chip, addr, val, 2);
	if (rc < 0)
		dev_err(chip->dev, "write play_rate failed, rc=%d\n", rc);

	return rc;
}

static int qti_haptics_brake_enable(struct qti_hap_chip *chip, bool en)
{
	u8 addr, mask, val;
	int rc;

	addr = REG_HAP_EN_CTL2;
	mask = HAP_BRAKE_EN_BIT;
	val = en ? HAP_BRAKE_EN_BIT : 0;
	rc = qti_haptics_masked_write(chip, addr, mask, val);
	if (rc < 0)
		dev_err(chip->dev, "write BRAKE_EN failed, rc=%d\n", rc);

	return rc;
}

static int qti_haptics_config_brake(struct qti_hap_chip *chip, u8 *brake)
{
	u8 addr,  val;
	int i, rc;

	addr = REG_HAP_BRAKE;
	for (val = 0, i = 0; i < HAP_BRAKE_PATTERN_MAX; i++)
		val |= (brake[i] & HAP_BRAKE_PATTERN_MASK) <<
			i * HAP_BRAKE_PATTERN_SHIFT;

	rc = qti_haptics_write(chip, addr, &val, 1);
	if (rc < 0) {
		dev_err(chip->dev, "write brake pattern failed, rc=%d\n", rc);
		return rc;
	}
	/*
	 * Set BRAKE_EN regardless of the brake pattern, this helps to stop
	 * playing immediately once the valid values in WF_Sx are played.
	 */
	rc = qti_haptics_brake_enable(chip, true);

	return rc;
}

static int qti_haptics_lra_auto_res_enable(struct qti_hap_chip *chip, bool en)
{
	int rc;
	u8 addr, val, mask;

	addr = REG_HAP_AUTO_RES_CTRL;
	mask = HAP_AUTO_RES_EN_BIT;
	val = en ? HAP_AUTO_RES_EN_BIT : 0;
	rc = qti_haptics_masked_write(chip, addr, mask, val);
	if (rc < 0)
		dev_err(chip->dev, "set AUTO_RES_CTRL failed, rc=%d\n", rc);

	return rc;
}

#define HAP_CLEAR_PLAYING_RATE_US	15
static int qti_haptics_clear_settings(struct qti_hap_chip *chip)
{
	int rc;
	u8 pattern[HAP_WAVEFORM_BUFFER_MAX] = {1, 0, 0, 0, 0, 0, 0, 0};

	rc = qti_haptics_brake_enable(chip, false);
	if (rc < 0)
		return rc;

	rc = qti_haptics_lra_auto_res_enable(chip, false);
	if (rc < 0)
		return rc;

	rc = qti_haptics_config_play_rate_us(chip, HAP_CLEAR_PLAYING_RATE_US);
	if (rc < 0)
		return rc;

	rc = qti_haptics_write(chip, REG_HAP_WF_S1, pattern,
			HAP_WAVEFORM_BUFFER_MAX);
	if (rc < 0)
		return rc;

	rc = qti_haptics_play(chip, true);
	if (rc < 0)
		return rc;

	rc = qti_haptics_play(chip, false);
	if (rc < 0)
		return rc;

	return 0;
}

static int qti_haptics_load_constant_waveform(struct qti_hap_chip *chip)
{
	struct qti_hap_play_info *play = &chip->play;
	struct qti_hap_config *config = &chip->config;
	int rc = 0;

	rc = qti_haptics_config_play_rate_us(chip, config->play_rate_us);
	if (rc < 0)
		return rc;
	/*
	 * Using VMAX waveform source if playing length is >= 20ms,
	 * otherwise using buffer waveform source and calculate the
	 * pattern length and repeating times to achieve accurate
	 * playing time accuracy.
	 */
	if (play->length_us >= VMAX_MIN_PLAY_TIME_US) {
		rc = qti_haptics_config_vmax(chip, play->vmax_mv);
		if (rc < 0)
			return rc;

		/* Enable Auto-Resonance when VMAX wf-src is selected */
		if (config->act_type == ACT_LRA) {
			rc = qti_haptics_lra_auto_res_enable(chip, true);
			if (rc < 0)
				return rc;
		}

		/* Set WF_SOURCE to VMAX */
		rc = qti_haptics_config_wf_src(chip, INT_WF_VMAX);
		if (rc < 0)
			return rc;

		play->playing_pattern = false;
		play->effect = NULL;
	} else {
		rc = qti_haptics_config_vmax(chip, config->vmax_mv);
		if (rc < 0)
			return rc;

		play->effect = &chip->constant;
		play->playing_pos = 0;
		/* Format and config waveform in patterns */
		construct_constant_waveform_in_pattern(play);
		rc = qti_haptics_config_wf_buffer(chip);
		if (rc < 0)
			return rc;

		rc = qti_haptics_config_wf_repeat(chip);
		if (rc < 0)
			return rc;

		/* Set WF_SOURCE to buffer */
		rc = qti_haptics_config_wf_src(chip, INT_WF_BUFFER);
		if (rc < 0)
			return rc;

		play->playing_pattern = true;
	}

	return 0;
}

static int qti_haptics_load_predefined_effect(struct qti_hap_chip *chip,
		int effect_idx)
{
	struct qti_hap_play_info *play = &chip->play;
	struct qti_hap_config *config = &chip->config;
	int rc = 0;

	if (effect_idx >= chip->effects_count)
		return -EINVAL;

	play->effect = &chip->predefined[effect_idx];
	play->playing_pos = 0;
	rc = qti_haptics_config_vmax(chip, play->vmax_mv);
	if (rc < 0)
		return rc;

	rc = qti_haptics_config_play_rate_us(chip, play->effect->play_rate_us);
	if (rc < 0)
		return rc;

	if (config->act_type == ACT_LRA) {
		rc = qti_haptics_lra_auto_res_enable(chip,
				!play->effect->lra_auto_res_disable);
		if (rc < 0)
			return rc;
	}

	/* Set brake pattern in the effect */
	rc = qti_haptics_config_brake(chip, play->effect->brake);
	if (rc < 0)
		return rc;

	rc = qti_haptics_config_wf_buffer(chip);
	if (rc < 0)
		return rc;

	rc = qti_haptics_config_wf_repeat(chip);
	if (rc < 0)
		return rc;

	/* Set WF_SOURCE to buffer */
	rc = qti_haptics_config_wf_src(chip, INT_WF_BUFFER);
	if (rc < 0)
		return rc;

	play->playing_pattern = true;

	return 0;
}

static irqreturn_t qti_haptics_play_irq_handler(int irq, void *data)
{
	struct qti_hap_chip *chip = (struct qti_hap_chip *)data;
	struct qti_hap_play_info *play = &chip->play;
	struct qti_hap_effect *effect = play->effect;
	int rc;

	dev_dbg(chip->dev, "play_irq triggered\n");
	if (play->playing_pos == effect->pattern_length) {
		dev_dbg(chip->dev, "waveform playing done\n");
		if (chip->play_irq_en) {
			disable_irq_nosync(chip->play_irq);
			chip->play_irq_en = false;
		}

		goto handled;
	}

	/* Config to play remaining patterns */
	rc = qti_haptics_config_wf_repeat(chip);
	if (rc < 0)
		goto handled;

	rc = qti_haptics_config_wf_buffer(chip);
	if (rc < 0)
		goto handled;

handled:
	return IRQ_HANDLED;
}

static irqreturn_t qti_haptics_sc_irq_handler(int irq, void *data)
{
	struct qti_hap_chip *chip = (struct qti_hap_chip *)data;
	u8 addr, val;
	ktime_t temp;
	s64 sc_delta_time_us;
	int rc;

	dev_dbg(chip->dev, "sc_irq triggered\n");
	addr = REG_HAP_STATUS1;
	rc = qti_haptics_read(chip, addr, &val, 1);
	if (rc < 0) {
		dev_err(chip->dev, "read HAP_STATUS1 failed, rc=%d\n", rc);
		goto handled;
	}

	if (!(val & HAP_SC_DET_BIT))
		goto handled;

	temp = ktime_get();
	sc_delta_time_us = ktime_us_delta(temp, chip->last_sc_time);
	chip->last_sc_time = temp;

	if (sc_delta_time_us > HAP_SC_DET_TIME_US)
		chip->sc_det_count = 0;
	else
		chip->sc_det_count++;

	addr = REG_HAP_SC_CLR;
	val = HAP_SC_CLR_BIT;
	rc = qti_haptics_write(chip, addr, &val, 1);
	if (rc < 0) {
		dev_err(chip->dev, "write SC_CLR failed, rc=%d\n", rc);
		goto handled;
	}

	if (chip->sc_det_count > HAP_SC_DET_MAX_COUNT) {
		rc = qti_haptics_module_en(chip, false);
		if (rc < 0)
			goto handled;

		dev_crit(chip->dev, "Short circuit persists, disable haptics\n");
		chip->perm_disable = true;
	}

handled:
	return IRQ_HANDLED;
}

static inline void get_play_length(struct qti_hap_play_info *play,
		int *length_us)
{
	struct qti_hap_effect *effect = play->effect;
	int tmp;

	tmp = effect->pattern_length * effect->play_rate_us;
	tmp *= wf_s_repeat[effect->wf_s_repeat_n];
	tmp *= wf_repeat[effect->wf_repeat_n];
	if (effect->brake_en)
		tmp += effect->play_rate_us * effect->brake_pattern_length;

	*length_us = tmp;
}

static int qti_haptics_upload_effect(struct input_dev *dev,
		struct ff_effect *effect, struct ff_effect *old)
{
	struct qti_hap_chip *chip = input_get_drvdata(dev);
	struct qti_hap_config *config = &chip->config;
	struct qti_hap_play_info *play = &chip->play;
	int rc = 0, tmp, i;
	s16 level, data[CUSTOM_DATA_LEN];
	ktime_t rem;
	s64 time_us;

	if (hrtimer_active(&chip->hap_disable_timer)) {
		rem = hrtimer_get_remaining(&chip->hap_disable_timer);
		time_us = ktime_to_us(rem);
		dev_dbg(chip->dev, "waiting for playing clear sequence: %lld us\n",
				time_us);
		usleep_range(time_us, time_us + 100);
	}

	switch (effect->type) {
	case FF_CONSTANT:
		play->length_us = effect->replay.length * USEC_PER_MSEC;
		level = effect->u.constant.level;
		tmp = level * config->vmax_mv;
		play->vmax_mv = tmp / 0x7fff;
		dev_dbg(chip->dev, "upload constant effect, length = %dus, vmax_mv=%d\n",
				play->length_us, play->vmax_mv);

		rc = qti_haptics_load_constant_waveform(chip);
		if (rc < 0) {
			dev_err(chip->dev, "Play constant waveform failed, rc=%d\n",
					rc);
			return rc;
		}
		break;

	case FF_PERIODIC:
		if (chip->effects_count == 0)
			return -EINVAL;

		if (effect->u.periodic.waveform != FF_CUSTOM) {
			dev_err(chip->dev, "Only accept custom waveforms\n");
			return -EINVAL;
		}

		if (copy_from_user(data, effect->u.periodic.custom_data,
					sizeof(s16) * CUSTOM_DATA_LEN))
			return -EFAULT;

		for (i = 0; i < chip->effects_count; i++)
			if (chip->predefined[i].id ==
					data[CUSTOM_DATA_EFFECT_IDX])
				break;

		if (i == chip->effects_count) {
			dev_err(chip->dev, "predefined effect %d is NOT supported\n",
					data[0]);
			return -EINVAL;
		}

		level = effect->u.periodic.magnitude;
		tmp = level * chip->predefined[i].vmax_mv;
		play->vmax_mv = tmp / 0x7fff;

		dev_dbg(chip->dev, "upload effect %d, vmax_mv=%d\n",
				chip->predefined[i].id, play->vmax_mv);
		rc = qti_haptics_load_predefined_effect(chip, i);
		if (rc < 0) {
			dev_err(chip->dev, "Play predefined effect %d failed, rc=%d\n",
					chip->predefined[i].id, rc);
			return rc;
		}

		get_play_length(play, &play->length_us);
		data[CUSTOM_DATA_TIMEOUT_SEC_IDX] =
			play->length_us / USEC_PER_SEC;
		data[CUSTOM_DATA_TIMEOUT_MSEC_IDX] =
			(play->length_us % USEC_PER_SEC) / USEC_PER_MSEC;

		/*
		 * Copy the custom data contains the play length back to
		 * userspace so that the userspace client can wait and
		 * send stop playing command after it's done.
		 */
		if (copy_to_user(effect->u.periodic.custom_data, data,
					sizeof(s16) * CUSTOM_DATA_LEN))
			return -EFAULT;
		break;

	default:
		dev_err(chip->dev, "Unsupported effect type: %d\n",
				effect->type);
		return -EINVAL;
	}

	if (chip->vdd_supply && !chip->vdd_enabled) {
		rc = regulator_enable(chip->vdd_supply);
		if (rc < 0) {
			dev_err(chip->dev, "Enable VDD supply failed, rc=%d\n",
					rc);
			return rc;
		}
		chip->vdd_enabled = true;
	}

	return 0;
}

static int qti_haptics_playback(struct input_dev *dev, int effect_id, int val)
{
	struct qti_hap_chip *chip = input_get_drvdata(dev);
	struct qti_hap_play_info *play = &chip->play;
	s64 secs;
	unsigned long nsecs;
	int rc = 0;

	dev_dbg(chip->dev, "playback, val = %d\n", val);
	if (!!val) {
		rc = qti_haptics_module_en(chip, true);
		if (rc < 0)
			return rc;

		rc = qti_haptics_play(chip, true);
		if (rc < 0)
			return rc;

		if (play->playing_pattern) {
			if (!chip->play_irq_en) {
				enable_irq(chip->play_irq);
				chip->play_irq_en = true;
			}
			/* Toggle PLAY when playing pattern */
			rc = qti_haptics_play(chip, false);
			if (rc < 0)
				return rc;
		} else {
			if (chip->play_irq_en) {
				disable_irq_nosync(chip->play_irq);
				chip->play_irq_en = false;
			}
			secs = play->length_us / USEC_PER_SEC;
			nsecs = (play->length_us % USEC_PER_SEC) *
				NSEC_PER_USEC;
			hrtimer_start(&chip->stop_timer, ktime_set(secs, nsecs),
					HRTIMER_MODE_REL);
		}
	} else {
		play->length_us = 0;
		rc = qti_haptics_play(chip, false);
		if (rc < 0)
			return rc;

		if (chip->play_irq_en) {
			disable_irq_nosync(chip->play_irq);
			chip->play_irq_en = false;
		}
	}

	return rc;
}

static int qti_haptics_erase(struct input_dev *dev, int effect_id)
{
	struct qti_hap_chip *chip = input_get_drvdata(dev);
	int delay_us, rc = 0;

	if (chip->vdd_supply && chip->vdd_enabled) {
		rc = regulator_disable(chip->vdd_supply);
		if (rc < 0) {
			dev_err(chip->dev, "Disable VDD supply failed, rc=%d\n",
					rc);
			return rc;
		}
		chip->vdd_enabled = false;
	}

	rc = qti_haptics_clear_settings(chip);
	if (rc < 0) {
		dev_err(chip->dev, "clear setting failed, rc=%d\n", rc);
		return rc;
	}

	if (chip->play.effect)
		delay_us = chip->play.effect->play_rate_us;
	else
		delay_us = chip->config.play_rate_us;

	delay_us += HAP_DISABLE_DELAY_USEC;
	hrtimer_start(&chip->hap_disable_timer,
			ktime_set(0, delay_us * NSEC_PER_USEC),
			HRTIMER_MODE_REL);

	return rc;
}

static void qti_haptics_set_gain(struct input_dev *dev, u16 gain)
{
	struct qti_hap_chip *chip = input_get_drvdata(dev);
	struct qti_hap_config *config = &chip->config;
	struct qti_hap_play_info *play = &chip->play;

	if (gain == 0)
		return;

	if (gain > 0x7fff)
		gain = 0x7fff;

	play->vmax_mv = ((u32)(gain * config->vmax_mv)) / 0x7fff;
	qti_haptics_config_vmax(chip, play->vmax_mv);
}

static int qti_haptics_hw_init(struct qti_hap_chip *chip)
{
	struct qti_hap_config *config = &chip->config;
	u8 addr, val, mask;
	int rc = 0;

	/* Config actuator type */
	addr = REG_HAP_CFG1;
	val = config->act_type;
	rc = qti_haptics_write(chip, addr, &val, 1);
	if (rc < 0) {
		dev_err(chip->dev, "write actuator type failed, rc=%d\n", rc);
		return rc;
	}

	/* Config ilim_ma */
	addr = REG_HAP_ILIM_CFG;
	val = config->ilim_ma == 400 ? 0 : 1;
	rc = qti_haptics_write(chip, addr, &val, 1);
	if (rc < 0) {
		dev_err(chip->dev, "write ilim_ma failed, rc=%d\n", rc);
		return rc;
	}

	/* Set HAP_EN_CTL3 */
	addr = REG_HAP_EN_CTL3;
	val = HAP_HBRIDGE_EN_BIT | HAP_PWM_SIGNAL_EN_BIT | HAP_ILIM_EN_BIT |
		HAP_ILIM_CC_EN_BIT | HAP_AUTO_RES_RBIAS_EN_BIT |
		HAP_DAC_EN_BIT | HAP_PWM_CTL_EN_BIT;
	rc = qti_haptics_write(chip, addr, &val, 1);
	if (rc < 0) {
		dev_err(chip->dev, "set EN_CTL3 failed, rc=%d\n", rc);
		return rc;
	}

	/* Set ZX_CFG */
	addr = REG_HAP_ZX_CFG;
	mask = HAP_ZX_DET_DEB_MASK;
	val = ZX_DET_DEB_80US;
	rc = qti_haptics_masked_write(chip, addr, mask, val);
	if (rc < 0) {
		dev_err(chip->dev, "write ZX_CFG failed, rc=%d\n", rc);
		return rc;
	}

	/*
	 * Config play rate: this is the resonance period for LRA,
	 * or the play duration of each waveform sample for ERM.
	 */
	rc = qti_haptics_config_play_rate_us(chip, config->play_rate_us);
	if (rc < 0)
		return rc;

	/* Set external waveform source if it's used */
	if (config->use_ext_wf_src) {
		rc = qti_haptics_config_wf_src(chip, config->ext_src);
		if (rc < 0)
			return rc;
	}

	/*
	 * Skip configurations below for ERM actuator
	 * as they're only for LRA actuators
	 */
	if (config->act_type == ACT_ERM)
		return 0;

	addr = REG_HAP_CFG2;
	val = config->lra_shape;
	rc = qti_haptics_write(chip, addr, &val, 1);
	if (rc < 0) {
		dev_err(chip->dev, "write lra_sig_shape failed, rc=%d\n", rc);
		return rc;
	}

	addr = REG_HAP_AUTO_RES_CFG;
	mask = HAP_AUTO_RES_MODE_BIT | HAP_CAL_EOP_EN_BIT | HAP_CAL_PERIOD_MASK;
	val = config->lra_auto_res_mode << HAP_AUTO_RES_MODE_SHIFT;
	val |= HAP_CAL_EOP_EN_BIT | HAP_CAL_OPT3_EVERY_8_PERIOD;
	rc = qti_haptics_masked_write(chip, addr, mask, val);
	if (rc < 0) {
		dev_err(chip->dev, "set AUTO_RES_CFG failed, rc=%d\n", rc);
		return rc;
	}

	addr = REG_HAP_AUTO_RES_CTRL;
	val = HAP_AUTO_RES_EN_BIT | HAP_SEL_AUTO_RES_PERIOD |
		AUTO_RES_CNT_ERR_DELTA(2) | HAP_AUTO_RES_ERR_RECOVERY_BIT |
		AUTO_RES_EN_DLY(4);
	rc = qti_haptics_write(chip, addr, &val, 1);
	if (rc < 0) {
		dev_err(chip->dev, "set AUTO_RES_CTRL failed, rc=%d\n",
				rc);
		return rc;
	}

	return 0;
}

static enum hrtimer_restart qti_hap_stop_timer(struct hrtimer *timer)
{
	struct qti_hap_chip *chip = container_of(timer, struct qti_hap_chip,
			stop_timer);
	int rc;

	chip->play.length_us = 0;
	rc = qti_haptics_play(chip, false);
	if (rc < 0)
		dev_err(chip->dev, "Stop playing failed, rc=%d\n", rc);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart qti_hap_disable_timer(struct hrtimer *timer)
{
	struct qti_hap_chip *chip = container_of(timer, struct qti_hap_chip,
			hap_disable_timer);
	int rc;

	rc = qti_haptics_module_en(chip, false);
	if (rc < 0)
		dev_err(chip->dev, "Disable haptics module failed, rc=%d\n",
				rc);

	return HRTIMER_NORESTART;
}

static void verify_brake_setting(struct qti_hap_effect *effect)
{
	int i = effect->brake_pattern_length - 1;
	u8 val = 0;

	for (; i >= 0; i--) {
		if (effect->brake[i] != 0)
			break;

		effect->brake_pattern_length--;
	}

	for (i = 0; i < effect->brake_pattern_length; i++) {
		effect->brake[i] &= HAP_BRAKE_PATTERN_MASK;
		val |= effect->brake[i] << (i * HAP_BRAKE_PATTERN_SHIFT);
	}

	effect->brake_en = (val != 0);
}

static int qti_haptics_parse_dt(struct qti_hap_chip *chip)
{
	struct qti_hap_config *config = &chip->config;
	const struct device_node *node = chip->dev->of_node;
	struct device_node *child_node;
	struct qti_hap_effect *effect;
	const char *str;
	int rc = 0, tmp, i = 0, j, m;

	rc = of_property_read_u32(node, "reg", &tmp);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to reg base, rc=%d\n", rc);
		return rc;
	}
	chip->reg_base = (u16)tmp;

	chip->sc_irq = platform_get_irq_byname(chip->pdev, "hap-sc-irq");
	if (chip->sc_irq < 0) {
		dev_err(chip->dev, "Failed to get hap-sc-irq\n");
		return chip->sc_irq;
	}

	chip->play_irq = platform_get_irq_byname(chip->pdev, "hap-play-irq");
	if (chip->play_irq < 0) {
		dev_err(chip->dev, "Failed to get hap-play-irq\n");
		return chip->play_irq;
	}

	config->act_type = ACT_LRA;
	rc = of_property_read_string(node, "qcom,actuator-type", &str);
	if (!rc) {
		if (strcmp(str, "erm") == 0) {
			config->act_type = ACT_ERM;
		} else if (strcmp(str, "lra") == 0) {
			config->act_type = ACT_LRA;
		} else {
			dev_err(chip->dev, "Invalid actuator type: %s\n",
					str);
			return -EINVAL;
		}
	}

	config->vmax_mv = HAP_VMAX_MV_DEFAULT;
	rc = of_property_read_u32(node, "qcom,vmax-mv", &tmp);
	if (!rc)
		config->vmax_mv = (tmp > HAP_VMAX_MV_MAX) ?
			HAP_VMAX_MV_MAX : tmp;

	config->ilim_ma = HAP_ILIM_MA_DEFAULT;
	rc = of_property_read_u32(node, "qcom,ilim-ma", &tmp);
	if (!rc)
		config->ilim_ma = (tmp >= HAP_ILIM_MA_MAX) ?
			HAP_ILIM_MA_MAX : HAP_ILIM_MA_DEFAULT;

	config->play_rate_us = HAP_PLAY_RATE_US_DEFAULT;
	rc = of_property_read_u32(node, "qcom,play-rate-us", &tmp);
	if (!rc)
		config->play_rate_us = (tmp >= HAP_PLAY_RATE_US_MAX) ?
			HAP_PLAY_RATE_US_MAX : tmp;

	if (of_find_property(node, "qcom,external-waveform-source", NULL)) {
		if (!of_property_read_string(node,
				"qcom,external-waveform-source", &str)) {
			if (strcmp(str, "audio") == 0) {
				config->ext_src = EXT_WF_AUDIO;
			} else if (strcmp(str, "pwm") == 0) {
				config->ext_src = EXT_WF_PWM;
			} else {
				dev_err(chip->dev, "Invalid external waveform source: %s\n",
						str);
				return -EINVAL;
			}
		}
		config->use_ext_wf_src = true;
	}

	if (of_find_property(node, "vdd-supply", NULL)) {
		chip->vdd_supply = devm_regulator_get(chip->dev, "vdd");
		if (IS_ERR(chip->vdd_supply)) {
			rc = PTR_ERR(chip->vdd_supply);
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev, "Failed to get vdd regulator");
			return rc;
		}
	}

	if (config->act_type == ACT_LRA) {
		config->lra_shape = RES_SIG_SINE;
		rc = of_property_read_string(node,
				"qcom,lra-resonance-sig-shape", &str);
		if (!rc) {
			if (strcmp(str, "sine") == 0) {
				config->lra_shape = RES_SIG_SINE;
			} else if (strcmp(str, "square") == 0) {
				config->lra_shape = RES_SIG_SQUARE;
			} else {
				dev_err(chip->dev, "Invalid resonance signal shape: %s\n",
						str);
				return -EINVAL;
			}
		}

		config->lra_allow_variable_play_rate = of_property_read_bool(
				node, "qcom,lra-allow-variable-play-rate");

		config->lra_auto_res_mode = AUTO_RES_MODE_ZXD;
		rc = of_property_read_string(node,
				"qcom,lra-auto-resonance-mode", &str);
		if (!rc) {
			if (strcmp(str, "zxd") == 0) {
				config->lra_auto_res_mode = AUTO_RES_MODE_ZXD;
			} else if (strcmp(str, "qwd") == 0) {
				config->lra_auto_res_mode = AUTO_RES_MODE_QWD;
			} else {
				dev_err(chip->dev, "Invalid auto resonance mode: %s\n",
						str);
				return -EINVAL;
			}
		}
	}

	chip->constant.pattern = devm_kcalloc(chip->dev,
			HAP_WAVEFORM_BUFFER_MAX,
			sizeof(u8), GFP_KERNEL);
	if (!chip->constant.pattern)
		return -ENOMEM;

	tmp = of_get_available_child_count(node);
	if (tmp == 0)
		return 0;

	chip->predefined = devm_kcalloc(chip->dev, tmp,
			sizeof(*chip->predefined), GFP_KERNEL);
	if (!chip->predefined)
		return -ENOMEM;

	chip->effects_count = tmp;

	for_each_available_child_of_node(node, child_node) {
		effect = &chip->predefined[i++];
		rc = of_property_read_u32(child_node, "qcom,effect-id",
				&effect->id);
		if (rc < 0) {
			dev_err(chip->dev, "Read qcom,effect-id failed, rc=%d\n",
					rc);
			return rc;
		}

		effect->vmax_mv = config->vmax_mv;
		rc = of_property_read_u32(child_node, "qcom,wf-vmax-mv", &tmp);
		if (rc < 0)
			dev_dbg(chip->dev, "Read qcom,wf-vmax-mv failed, rc=%d\n",
					rc);
		else
			effect->vmax_mv = (tmp > HAP_VMAX_MV_MAX) ?
				HAP_VMAX_MV_MAX : tmp;

		rc = of_property_count_elems_of_size(child_node,
				"qcom,wf-pattern", sizeof(u8));
		if (rc < 0) {
			dev_err(chip->dev, "Count qcom,wf-pattern property failed, rc=%d\n",
					rc);
			return rc;
		} else if (rc == 0) {
			dev_dbg(chip->dev, "qcom,wf-pattern has no data\n");
			return -EINVAL;
		}

		effect->pattern_length = rc;
		effect->pattern = devm_kcalloc(chip->dev,
				effect->pattern_length, sizeof(u8), GFP_KERNEL);
		if (!effect->pattern)
			return -ENOMEM;

		rc = of_property_read_u8_array(child_node, "qcom,wf-pattern",
				effect->pattern, effect->pattern_length);
		if (rc < 0) {
			dev_err(chip->dev, "Read qcom,wf-pattern property failed, rc=%d\n",
					rc);
			return rc;
		}

		effect->play_rate_us = config->play_rate_us;
		rc = of_property_read_u32(child_node, "qcom,wf-play-rate-us",
				&tmp);
		if (rc < 0)
			dev_dbg(chip->dev, "Read qcom,wf-play-rate-us failed, rc=%d\n",
					rc);
		else
			effect->play_rate_us = tmp;

		if (config->act_type == ACT_LRA &&
				!config->lra_allow_variable_play_rate &&
				config->play_rate_us != effect->play_rate_us) {
			dev_warn(chip->dev, "play rate should match with LRA resonance frequency\n");
			effect->play_rate_us = config->play_rate_us;
		}

		rc = of_property_read_u32(child_node, "qcom,wf-repeat-count",
				&tmp);
		if (rc < 0) {
			dev_dbg(chip->dev, "Read qcom,wf-repeat-count failed, rc=%d\n",
					rc);
		} else {
			for (j = 0; j < ARRAY_SIZE(wf_repeat); j++)
				if (tmp <= wf_repeat[j])
					break;

			effect->wf_repeat_n = j;
		}

		rc = of_property_read_u32(child_node, "qcom,wf-s-repeat-count",
				&tmp);
		if (rc < 0) {
			dev_dbg(chip->dev, "Read qcom,wf-s-repeat-count failed, rc=%d\n",
					rc);
		} else {
			for (j = 0; j < ARRAY_SIZE(wf_s_repeat); j++)
				if (tmp <= wf_s_repeat[j])
					break;

			effect->wf_s_repeat_n = j;
		}

		effect->lra_auto_res_disable = of_property_read_bool(child_node,
				"qcom,lra-auto-resonance-disable");

		tmp = of_property_count_elems_of_size(child_node,
				"qcom,wf-brake-pattern", sizeof(u8));
		if (tmp <= 0)
			continue;

		if (tmp > HAP_BRAKE_PATTERN_MAX) {
			dev_err(chip->dev, "wf-brake-pattern shouldn't be more than %d bytes\n",
					HAP_BRAKE_PATTERN_MAX);
			return -EINVAL;
		}

		rc = of_property_read_u8_array(child_node,
				"qcom,wf-brake-pattern", effect->brake, tmp);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to get wf-brake-pattern, rc=%d\n",
					rc);
			return rc;
		}

		effect->brake_pattern_length = tmp;
		verify_brake_setting(effect);
	}

	for (j = 0; j < i; j++) {
		dev_dbg(chip->dev, "effect: %d\n", chip->predefined[j].id);
		dev_dbg(chip->dev, "        vmax: %d mv\n",
				chip->predefined[j].vmax_mv);
		dev_dbg(chip->dev, "        play_rate: %d us\n",
				chip->predefined[j].play_rate_us);
		for (m = 0; m < chip->predefined[j].pattern_length; m++)
			dev_dbg(chip->dev, "        pattern[%d]: 0x%x\n",
					m, chip->predefined[j].pattern[m]);
		for (m = 0; m < chip->predefined[j].brake_pattern_length; m++)
			dev_dbg(chip->dev, "        brake_pattern[%d]: 0x%x\n",
					m, chip->predefined[j].brake[m]);
		dev_dbg(chip->dev, "    brake_en: %d\n",
				chip->predefined[j].brake_en);
		dev_dbg(chip->dev, "    wf_repeat_n: %d\n",
				chip->predefined[j].wf_repeat_n);
		dev_dbg(chip->dev, "    wf_s_repeat_n: %d\n",
				chip->predefined[j].wf_s_repeat_n);
		dev_dbg(chip->dev, "    lra_auto_res_disable: %d\n",
				chip->predefined[j].lra_auto_res_disable);
	}

	return 0;
}

#ifdef CONFIG_DEBUG_FS
static int play_rate_dbgfs_read(void *data, u64 *val)
{
	struct qti_hap_effect *effect = (struct qti_hap_effect *)data;

	*val = effect->play_rate_us;

	return 0;
}

static int play_rate_dbgfs_write(void *data, u64 val)
{
	struct qti_hap_effect *effect = (struct qti_hap_effect *)data;

	if (val > HAP_PLAY_RATE_US_MAX)
		val = HAP_PLAY_RATE_US_MAX;

	effect->play_rate_us = val;

	return 0;
}

static int vmax_dbgfs_read(void *data, u64 *val)
{
	struct qti_hap_effect *effect = (struct qti_hap_effect *)data;

	*val = effect->vmax_mv;

	return 0;
}

static int vmax_dbgfs_write(void *data, u64 val)
{
	struct qti_hap_effect *effect = (struct qti_hap_effect *)data;

	if (val > HAP_VMAX_MV_MAX)
		val = HAP_VMAX_MV_MAX;

	effect->vmax_mv = val;

	return 0;
}

static int wf_repeat_n_dbgfs_read(void *data, u64 *val)
{
	struct qti_hap_effect *effect = (struct qti_hap_effect *)data;

	*val = wf_repeat[effect->wf_repeat_n];

	return 0;
}

static int wf_repeat_n_dbgfs_write(void *data, u64 val)
{
	struct qti_hap_effect *effect = (struct qti_hap_effect *)data;
	int i;

	for (i = 0; i < ARRAY_SIZE(wf_repeat); i++)
		if (val == wf_repeat[i])
			break;

	if (i == ARRAY_SIZE(wf_repeat))
		pr_err("wf_repeat value %llu is invalid\n", val);
	else
		effect->wf_repeat_n = i;

	return 0;
}

static int wf_s_repeat_n_dbgfs_read(void *data, u64 *val)
{
	struct qti_hap_effect *effect = (struct qti_hap_effect *)data;

	*val = wf_s_repeat[effect->wf_s_repeat_n];

	return 0;
}

static int wf_s_repeat_n_dbgfs_write(void *data, u64 val)
{
	struct qti_hap_effect *effect = (struct qti_hap_effect *)data;
	int i;

	for (i = 0; i < ARRAY_SIZE(wf_s_repeat); i++)
		if (val == wf_s_repeat[i])
			break;

	if (i == ARRAY_SIZE(wf_s_repeat))
		pr_err("wf_s_repeat value %llu is invalid\n", val);
	else
		effect->wf_s_repeat_n = i;

	return 0;
}


static int auto_res_dbgfs_read(void *data, u64 *val)
{
	struct qti_hap_effect *effect = (struct qti_hap_effect *)data;

	*val = !effect->lra_auto_res_disable;

	return 0;
}

static int auto_res_dbgfs_write(void *data, u64 val)
{
	struct qti_hap_effect *effect = (struct qti_hap_effect *)data;

	effect->lra_auto_res_disable = !val;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(play_rate_debugfs_ops,  play_rate_dbgfs_read,
		play_rate_dbgfs_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(vmax_debugfs_ops, vmax_dbgfs_read,
		vmax_dbgfs_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(wf_repeat_n_debugfs_ops,  wf_repeat_n_dbgfs_read,
		wf_repeat_n_dbgfs_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(wf_s_repeat_n_debugfs_ops,  wf_s_repeat_n_dbgfs_read,
		wf_s_repeat_n_dbgfs_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(auto_res_debugfs_ops,  auto_res_dbgfs_read,
		auto_res_dbgfs_write, "%llu\n");

#define CHAR_PER_PATTERN 8
static ssize_t brake_pattern_dbgfs_read(struct file *filep,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct qti_hap_effect *effect =
		(struct qti_hap_effect *)filep->private_data;
	char *kbuf, *tmp;
	int rc, length, i, len;

	kbuf = kcalloc(CHAR_PER_PATTERN, HAP_BRAKE_PATTERN_MAX, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	tmp = kbuf;
	for (length = 0, i = 0; i < HAP_BRAKE_PATTERN_MAX; i++) {
		len = snprintf(tmp, CHAR_PER_PATTERN, "0x%x ",
				effect->brake[i]);
		tmp += len;
		length += len;
	}

	kbuf[length++] = '\n';
	kbuf[length++] = '\0';

	rc = simple_read_from_buffer(buf, count, ppos, kbuf, length);

	kfree(kbuf);
	return rc;
}

static ssize_t brake_pattern_dbgfs_write(struct file *filep,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct qti_hap_effect *effect =
		(struct qti_hap_effect *)filep->private_data;
	char *kbuf, *token;
	int rc = 0, i = 0, j;
	u32 val;

	kbuf = kmalloc(count + 1, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	rc = copy_from_user(kbuf, buf, count);
	if (rc > 0) {
		rc = -EFAULT;
		goto err;
	}

	kbuf[count] = '\0';
	*ppos += count;

	while ((token = strsep(&kbuf, " ")) != NULL) {
		rc = kstrtouint(token, 0, &val);
		if (rc < 0) {
			rc = -EINVAL;
			goto err;
		}

		effect->brake[i++] = val & HAP_BRAKE_PATTERN_MASK;

		if (i >= HAP_BRAKE_PATTERN_MAX)
			break;
	}

	for (j = i; j < HAP_BRAKE_PATTERN_MAX; j++)
		effect->brake[j] = 0;

	effect->brake_pattern_length = i;
	verify_brake_setting(effect);

	rc = count;
err:
	kfree(kbuf);
	return rc;
}

static const struct file_operations brake_pattern_dbgfs_ops = {
	.read = brake_pattern_dbgfs_read,
	.write = brake_pattern_dbgfs_write,
	.owner = THIS_MODULE,
	.open = simple_open,
};

static ssize_t pattern_dbgfs_read(struct file *filep,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct qti_hap_effect *effect =
		(struct qti_hap_effect *)filep->private_data;
	char *kbuf, *tmp;
	int rc, length, i, len;

	kbuf = kcalloc(CHAR_PER_PATTERN, effect->pattern_length, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	tmp = kbuf;
	for (length = 0, i = 0; i < effect->pattern_length; i++) {
		len = snprintf(tmp, CHAR_PER_PATTERN, "0x%x ",
				effect->pattern[i]);
		tmp += len;
		length += len;
	}

	kbuf[length++] = '\n';
	kbuf[length++] = '\0';

	rc = simple_read_from_buffer(buf, count, ppos, kbuf, length);

	kfree(kbuf);
	return rc;
}

static ssize_t pattern_dbgfs_write(struct file *filep,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct qti_hap_effect *effect =
		(struct qti_hap_effect *)filep->private_data;
	char *kbuf, *token;
	int rc = 0, i = 0, j;
	u32 val;

	kbuf = kmalloc(count + 1, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	rc = copy_from_user(kbuf, buf, count);
	if (rc > 0) {
		rc = -EFAULT;
		goto err;
	}

	kbuf[count] = '\0';
	*ppos += count;

	while ((token = strsep(&kbuf, " ")) != NULL) {
		rc = kstrtouint(token, 0, &val);
		if (rc < 0) {
			rc = -EINVAL;
			goto err;
		}

		effect->pattern[i++] = val & 0xff;

		if (i >= effect->pattern_length)
			break;
	}

	for (j = i; j < effect->pattern_length; j++)
		effect->pattern[j] = 0;

	rc = count;
err:
	kfree(kbuf);
	return rc;
}

static const struct file_operations pattern_dbgfs_ops = {
	.read = pattern_dbgfs_read,
	.write = pattern_dbgfs_write,
	.owner = THIS_MODULE,
	.open = simple_open,
};

static int create_effect_debug_files(struct qti_hap_effect *effect,
				struct dentry *dir)
{
	struct dentry *file;

	file = debugfs_create_file("play_rate_us", 0644, dir,
			effect, &play_rate_debugfs_ops);
	if (!file) {
		pr_err("create play-rate debugfs node failed\n");
		return -ENOMEM;
	}

	file = debugfs_create_file("vmax_mv", 0644, dir,
			effect, &vmax_debugfs_ops);
	if (!file) {
		pr_err("create vmax debugfs node failed\n");
		return -ENOMEM;
	}

	file = debugfs_create_file("wf_repeat_n", 0644, dir,
			effect, &wf_repeat_n_debugfs_ops);
	if (!file) {
		pr_err("create wf-repeat debugfs node failed\n");
		return -ENOMEM;
	}

	file = debugfs_create_file("wf_s_repeat_n", 0644, dir,
			effect, &wf_s_repeat_n_debugfs_ops);
	if (!file) {
		pr_err("create wf-s-repeat debugfs node failed\n");
		return -ENOMEM;
	}

	file = debugfs_create_file("lra_auto_res_en", 0644, dir,
			effect, &auto_res_debugfs_ops);
	if (!file) {
		pr_err("create lra-auto-res-en debugfs node failed\n");
		return -ENOMEM;
	}

	file = debugfs_create_file("brake", 0644, dir,
			effect, &brake_pattern_dbgfs_ops);
	if (!file) {
		pr_err("create brake debugfs node failed\n");
		return -ENOMEM;
	}

	file = debugfs_create_file("pattern", 0644, dir,
			effect, &pattern_dbgfs_ops);
	if (!file) {
		pr_err("create pattern debugfs node failed\n");
		return -ENOMEM;
	}

	return 0;
}

static int qti_haptics_add_debugfs(struct qti_hap_chip *chip)
{
	struct dentry *hap_dir, *effect_dir;
	char str[12] = {0};
	int i, rc = 0;

	hap_dir = debugfs_create_dir("haptics", NULL);
	if (!hap_dir) {
		pr_err("create haptics debugfs directory failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < chip->effects_count; i++) {
		snprintf(str, ARRAY_SIZE(str), "effect%d", i);
		effect_dir = debugfs_create_dir(str, hap_dir);
		if (!effect_dir) {
			pr_err("create %s debugfs directory failed\n", str);
			rc = -ENOMEM;
			goto cleanup;
		}

		rc = create_effect_debug_files(&chip->predefined[i],
				effect_dir);
		if (rc < 0) {
			rc = -ENOMEM;
			goto cleanup;
		}
	}

	chip->hap_debugfs = hap_dir;
	return 0;

cleanup:
	debugfs_remove_recursive(hap_dir);
	return rc;
}
#endif

static int qti_haptics_probe(struct platform_device *pdev)
{
	struct qti_hap_chip *chip;
	struct input_dev *input_dev;
	struct ff_device *ff;
	int rc = 0, effect_count_max;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	input_dev = devm_input_allocate_device(&pdev->dev);
	if (!input_dev)
		return -ENOMEM;

	chip->pdev = pdev;
	chip->dev = &pdev->dev;
	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap) {
		dev_err(chip->dev, "Failed to get regmap handle\n");
		return -ENXIO;
	}

	rc = qti_haptics_parse_dt(chip);
	if (rc < 0) {
		dev_err(chip->dev, "parse device-tree failed, rc=%d\n", rc);
		return rc;
	}

	spin_lock_init(&chip->bus_lock);

	rc = qti_haptics_hw_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "parse device-tree failed, rc=%d\n", rc);
		return rc;
	}

	rc = devm_request_threaded_irq(chip->dev, chip->play_irq, NULL,
			qti_haptics_play_irq_handler,
			IRQF_ONESHOT, "hap_play_irq", chip);
	if (rc < 0) {
		dev_err(chip->dev, "request play-irq failed, rc=%d\n", rc);
		return rc;
	}

	disable_irq(chip->play_irq);
	chip->play_irq_en = false;

	rc = devm_request_threaded_irq(chip->dev, chip->sc_irq, NULL,
			qti_haptics_sc_irq_handler,
			IRQF_ONESHOT, "hap_sc_irq", chip);
	if (rc < 0) {
		dev_err(chip->dev, "request sc-irq failed, rc=%d\n", rc);
		return rc;
	}

	hrtimer_init(&chip->stop_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	chip->stop_timer.function = qti_hap_stop_timer;
	hrtimer_init(&chip->hap_disable_timer, CLOCK_MONOTONIC,
						HRTIMER_MODE_REL);
	chip->hap_disable_timer.function = qti_hap_disable_timer;
	input_dev->name = "qti-haptics";
	input_set_drvdata(input_dev, chip);
	chip->input_dev = input_dev;

	input_set_capability(input_dev, EV_FF, FF_CONSTANT);
	input_set_capability(input_dev, EV_FF, FF_GAIN);
	if (chip->effects_count != 0) {
		input_set_capability(input_dev, EV_FF, FF_PERIODIC);
		input_set_capability(input_dev, EV_FF, FF_CUSTOM);
	}

	if (chip->effects_count + 1 > FF_EFFECT_COUNT_MAX)
		effect_count_max = chip->effects_count + 1;
	else
		effect_count_max = FF_EFFECT_COUNT_MAX;
	rc = input_ff_create(input_dev, effect_count_max);
	if (rc < 0) {
		dev_err(chip->dev, "create FF input device failed, rc=%d\n",
				rc);
		return rc;
	}

	ff = input_dev->ff;
	ff->upload = qti_haptics_upload_effect;
	ff->playback = qti_haptics_playback;
	ff->erase = qti_haptics_erase;
	ff->set_gain = qti_haptics_set_gain;

	rc = input_register_device(input_dev);
	if (rc < 0) {
		dev_err(chip->dev, "register input device failed, rc=%d\n",
				rc);
		goto destroy_ff;
	}

	dev_set_drvdata(chip->dev, chip);
#ifdef CONFIG_DEBUG_FS
	rc = qti_haptics_add_debugfs(chip);
	if (rc < 0)
		dev_dbg(chip->dev, "create debugfs failed, rc=%d\n", rc);
#endif
	return 0;

destroy_ff:
	input_ff_destroy(chip->input_dev);
	return rc;
}

static int qti_haptics_remove(struct platform_device *pdev)
{
	struct qti_hap_chip *chip = dev_get_drvdata(&pdev->dev);

#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(chip->hap_debugfs);
#endif
	input_ff_destroy(chip->input_dev);
	dev_set_drvdata(chip->dev, NULL);

	return 0;
}

static void qti_haptics_shutdown(struct platform_device *pdev)
{
	struct qti_hap_chip *chip = dev_get_drvdata(&pdev->dev);
	int rc;

	dev_dbg(chip->dev, "Shutdown!\n");

	qti_haptics_module_en(chip, false);

	if (chip->vdd_supply && chip->vdd_enabled) {
		rc = regulator_disable(chip->vdd_supply);
		if (rc < 0) {
			dev_err(chip->dev, "Disable VDD supply failed, rc=%d\n",
					rc);
			return;
		}
		chip->vdd_enabled = false;
	}
}

static const struct of_device_id haptics_match_table[] = {
	{ .compatible = "qcom,haptics" },
	{ .compatible = "qcom,pm660-haptics" },
	{ .compatible = "qcom,pm8150b-haptics" },
	{},
};

static struct platform_driver qti_haptics_driver = {
	.driver		= {
		.name = "qcom,haptics",
		.owner = THIS_MODULE,
		.of_match_table = haptics_match_table,
	},
	.probe		= qti_haptics_probe,
	.remove		= qti_haptics_remove,
	.shutdown	= qti_haptics_shutdown,
};
module_platform_driver(qti_haptics_driver);

MODULE_DESCRIPTION("QTI haptics driver");
MODULE_LICENSE("GPL v2");
