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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/types.h>

#define REG_SIZE_PER_LPG	0x100
#define LPG_BASE		"lpg-base"
#define LUT_BASE		"lut-base"

/* LPG module registers */
#define REG_LPG_PERPH_SUBTYPE		0x05
#define REG_LPG_PATTERN_CONFIG		0x40
#define REG_LPG_PWM_SIZE_CLK		0x41
#define REG_LPG_PWM_FREQ_PREDIV_CLK	0x42
#define REG_LPG_PWM_TYPE_CONFIG		0x43
#define REG_LPG_PWM_VALUE_LSB		0x44
#define REG_LPG_PWM_VALUE_MSB		0x45
#define REG_LPG_ENABLE_CONTROL		0x46
#define REG_LPG_PWM_SYNC		0x47
#define REG_LPG_RAMP_STEP_DURATION_LSB	0x50
#define REG_LPG_RAMP_STEP_DURATION_MSB	0x51
#define REG_LPG_PAUSE_HI_MULTIPLIER	0x52
#define REG_LPG_PAUSE_LO_MULTIPLIER	0x54
#define REG_LPG_HI_INDEX		0x56
#define REG_LPG_LO_INDEX		0x57

/* REG_LPG_PATTERN_CONFIG */
#define LPG_PATTERN_EN_PAUSE_LO		BIT(0)
#define LPG_PATTERN_EN_PAUSE_HI		BIT(1)
#define LPG_PATTERN_RAMP_TOGGLE		BIT(2)
#define LPG_PATTERN_REPEAT		BIT(3)
#define LPG_PATTERN_RAMP_LO_TO_HI	BIT(4)

/* REG_LPG_PERPH_SUBTYPE */
#define SUBTYPE_PWM			0x0b
#define SUBTYPE_LPG_LITE		0x11

/* REG_LPG_PWM_SIZE_CLK */
#define LPG_PWM_SIZE_LPG_MASK		BIT(4)
#define LPG_PWM_SIZE_PWM_MASK		BIT(2)
#define LPG_PWM_SIZE_LPG_SHIFT		4
#define LPG_PWM_SIZE_PWM_SHIFT		2
#define LPG_PWM_CLK_FREQ_SEL_MASK	GENMASK(1, 0)

/* REG_LPG_PWM_FREQ_PREDIV_CLK */
#define LPG_PWM_FREQ_PREDIV_MASK	GENMASK(6, 5)
#define LPG_PWM_FREQ_PREDIV_SHIFT	5
#define LPG_PWM_FREQ_EXPONENT_MASK	GENMASK(2, 0)

/* REG_LPG_PWM_TYPE_CONFIG */
#define LPG_PWM_EN_GLITCH_REMOVAL_MASK	BIT(5)

/* REG_LPG_PWM_VALUE_LSB */
#define LPG_PWM_VALUE_LSB_MASK		GENMASK(7, 0)

/* REG_LPG_PWM_VALUE_MSB */
#define LPG_PWM_VALUE_MSB_MASK		BIT(0)

/* REG_LPG_ENABLE_CONTROL */
#define LPG_EN_LPG_OUT_BIT		BIT(7)
#define LPG_EN_LPG_OUT_SHIFT		7
#define LPG_PWM_SRC_SELECT_MASK		BIT(2)
#define LPG_PWM_SRC_SELECT_SHIFT	2
#define LPG_EN_RAMP_GEN_MASK		BIT(1)
#define LPG_EN_RAMP_GEN_SHIFT		1

/* REG_LPG_PWM_SYNC */
#define LPG_PWM_VALUE_SYNC		BIT(0)

#define NUM_PWM_SIZE			2
#define NUM_PWM_CLK			3
#define NUM_CLK_PREDIV			4
#define NUM_PWM_EXP			8

#define LPG_HI_LO_IDX_MASK		GENMASK(5, 0)

/* LUT module registers */
#define REG_LPG_LUT_1_LSB		0x42
#define REG_LPG_LUT_RAMP_CONTROL	0xc8

#define LPG_LUT_VALUE_MSB_MASK		BIT(0)
#define LPG_LUT_COUNT_MAX		47

enum lpg_src {
	LUT_PATTERN = 0,
	PWM_VALUE,
};

static const int pwm_size[NUM_PWM_SIZE] = {6, 9};
static const int clk_freq_hz[NUM_PWM_CLK] = {1024, 32768, 19200000};
static const int clk_prediv[NUM_CLK_PREDIV] = {1, 3, 5, 6};
static const int pwm_exponent[NUM_PWM_EXP] = {0, 1, 2, 3, 4, 5, 6, 7};

struct lpg_ramp_config {
	u16			step_ms;
	u8			pause_hi_count;
	u8			pause_lo_count;
	u8			hi_idx;
	u8			lo_idx;
	bool			ramp_dir_low_to_hi;
	bool			pattern_repeat;
	bool			toggle;
	u32			*pattern;
	u32			pattern_length;
};

struct lpg_pwm_config {
	u32	pwm_size;
	u32	pwm_clk;
	u32	prediv;
	u32	clk_exp;
	u16	pwm_value;
	u64	best_period_ns;
};

struct qpnp_lpg_lut {
	struct qpnp_lpg_chip	*chip;
	struct mutex		lock;
	u32			reg_base;
	u32			*pattern; /* patterns in percentage */
};

struct qpnp_lpg_channel {
	struct qpnp_lpg_chip		*chip;
	struct lpg_pwm_config		pwm_config;
	struct lpg_ramp_config		ramp_config;
	u32				lpg_idx;
	u32				reg_base;
	u32				max_pattern_length;
	u8				src_sel;
	u8				subtype;
	bool				lut_written;
	u64				current_period_ns;
	u64				current_duty_ns;
};

struct qpnp_lpg_chip {
	struct pwm_chip		pwm_chip;
	struct regmap		*regmap;
	struct device		*dev;
	struct qpnp_lpg_channel	*lpgs;
	struct qpnp_lpg_lut	*lut;
	struct mutex		bus_lock;
	u32			*lpg_group;
	u32			num_lpgs;
};

static int qpnp_lpg_read(struct qpnp_lpg_channel *lpg, u16 addr, u8 *val)
{
	int rc;
	unsigned int tmp;

	mutex_lock(&lpg->chip->bus_lock);
	rc = regmap_read(lpg->chip->regmap, lpg->reg_base + addr, &tmp);
	if (rc < 0)
		dev_err(lpg->chip->dev, "Read addr 0x%x failed, rc=%d\n",
				lpg->reg_base + addr, rc);
	else
		*val = (u8)tmp;
	mutex_unlock(&lpg->chip->bus_lock);

	return rc;
}

static int qpnp_lpg_write(struct qpnp_lpg_channel *lpg, u16 addr, u8 val)
{
	int rc;

	mutex_lock(&lpg->chip->bus_lock);
	rc = regmap_write(lpg->chip->regmap, lpg->reg_base + addr, val);
	if (rc < 0)
		dev_err(lpg->chip->dev, "Write addr 0x%x with value %d failed, rc=%d\n",
				lpg->reg_base + addr, val, rc);
	mutex_unlock(&lpg->chip->bus_lock);

	return rc;
}

static int qpnp_lpg_masked_write(struct qpnp_lpg_channel *lpg,
				u16 addr, u8 mask, u8 val)
{
	int rc;

	mutex_lock(&lpg->chip->bus_lock);
	rc = regmap_update_bits(lpg->chip->regmap, lpg->reg_base + addr,
							mask, val);
	if (rc < 0)
		dev_err(lpg->chip->dev, "Update addr 0x%x to val 0x%x with mask 0x%x failed, rc=%d\n",
				lpg->reg_base + addr, val, mask, rc);
	mutex_unlock(&lpg->chip->bus_lock);

	return rc;
}

static int qpnp_lut_write(struct qpnp_lpg_lut *lut, u16 addr, u8 val)
{
	int rc;

	mutex_lock(&lut->chip->bus_lock);
	rc = regmap_write(lut->chip->regmap, lut->reg_base + addr, val);
	if (rc < 0)
		dev_err(lut->chip->dev, "Write addr 0x%x with value %d failed, rc=%d\n",
				lut->reg_base + addr, val, rc);
	mutex_unlock(&lut->chip->bus_lock);

	return rc;
}

static int qpnp_lut_masked_write(struct qpnp_lpg_lut *lut,
				u16 addr, u8 mask, u8 val)
{
	int rc;

	mutex_lock(&lut->chip->bus_lock);
	rc = regmap_update_bits(lut->chip->regmap, lut->reg_base + addr,
							mask, val);
	if (rc < 0)
		dev_err(lut->chip->dev, "Update addr 0x%x to val 0x%x with mask 0x%x failed, rc=%d\n",
				lut->reg_base + addr, val, mask, rc);
	mutex_unlock(&lut->chip->bus_lock);

	return rc;
}

static struct qpnp_lpg_channel *pwm_dev_to_qpnp_lpg(struct pwm_chip *pwm_chip,
				struct pwm_device *pwm)
{

	struct qpnp_lpg_chip *chip = container_of(pwm_chip,
			struct qpnp_lpg_chip, pwm_chip);
	u32 hw_idx = pwm->hwpwm;

	if (hw_idx >= chip->num_lpgs) {
		dev_err(chip->dev, "hw index %d out of range [0-%d]\n",
				hw_idx, chip->num_lpgs - 1);
		return NULL;
	}

	return &chip->lpgs[hw_idx];
}

static int __find_index_in_array(int member, const int array[], int length)
{
	int i;

	for (i = 0; i < length; i++) {
		if (member == array[i])
			return i;
	}

	return -EINVAL;
}

static int qpnp_lpg_set_glitch_removal(struct qpnp_lpg_channel *lpg, bool en)
{
	int rc;
	u8 mask, val;

	val = en ? LPG_PWM_EN_GLITCH_REMOVAL_MASK : 0;
	mask = LPG_PWM_EN_GLITCH_REMOVAL_MASK;
	rc = qpnp_lpg_masked_write(lpg, REG_LPG_PWM_TYPE_CONFIG, mask, val);
	if (rc < 0)
		dev_err(lpg->chip->dev, "Write LPG_PWM_TYPE_CONFIG failed, rc=%d\n",
							rc);
	return rc;
}

static int qpnp_lpg_set_pwm_config(struct qpnp_lpg_channel *lpg)
{
	int rc;
	u8 val, mask, shift;
	int pwm_size_idx, pwm_clk_idx, prediv_idx, clk_exp_idx;

	pwm_size_idx = __find_index_in_array(lpg->pwm_config.pwm_size,
			pwm_size, ARRAY_SIZE(pwm_size));
	pwm_clk_idx = __find_index_in_array(lpg->pwm_config.pwm_clk,
			clk_freq_hz, ARRAY_SIZE(clk_freq_hz));
	prediv_idx = __find_index_in_array(lpg->pwm_config.prediv,
			clk_prediv, ARRAY_SIZE(clk_prediv));
	clk_exp_idx = __find_index_in_array(lpg->pwm_config.clk_exp,
			pwm_exponent, ARRAY_SIZE(pwm_exponent));

	if (pwm_size_idx < 0 || pwm_clk_idx < 0
			|| prediv_idx < 0 || clk_exp_idx < 0)
		return -EINVAL;

	/* pwm_clk_idx is 1 bit lower than the register value */
	pwm_clk_idx += 1;
	if (lpg->subtype == SUBTYPE_PWM) {
		shift = LPG_PWM_SIZE_PWM_SHIFT;
		mask = LPG_PWM_SIZE_PWM_MASK;
	} else {
		shift = LPG_PWM_SIZE_LPG_SHIFT;
		mask = LPG_PWM_SIZE_LPG_MASK;
	}

	val = pwm_size_idx << shift | pwm_clk_idx;
	mask |= LPG_PWM_CLK_FREQ_SEL_MASK;
	rc = qpnp_lpg_masked_write(lpg, REG_LPG_PWM_SIZE_CLK, mask, val);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Write LPG_PWM_SIZE_CLK failed, rc=%d\n",
							rc);
		return rc;
	}

	val = prediv_idx << LPG_PWM_FREQ_PREDIV_SHIFT | clk_exp_idx;
	mask = LPG_PWM_FREQ_PREDIV_MASK | LPG_PWM_FREQ_EXPONENT_MASK;
	rc = qpnp_lpg_masked_write(lpg, REG_LPG_PWM_FREQ_PREDIV_CLK, mask, val);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Write LPG_PWM_FREQ_PREDIV_CLK failed, rc=%d\n",
							rc);
		return rc;
	}

	if (lpg->src_sel == LUT_PATTERN)
		return 0;

	val = lpg->pwm_config.pwm_value & LPG_PWM_VALUE_LSB_MASK;
	rc = qpnp_lpg_write(lpg, REG_LPG_PWM_VALUE_LSB, val);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Write LPG_PWM_VALUE_LSB failed, rc=%d\n",
							rc);
		return rc;
	}

	val = lpg->pwm_config.pwm_value >> 8;
	mask = LPG_PWM_VALUE_MSB_MASK;
	rc = qpnp_lpg_masked_write(lpg, REG_LPG_PWM_VALUE_MSB, mask, val);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Write LPG_PWM_VALUE_MSB failed, rc=%d\n",
							rc);
		return rc;
	}

	val = LPG_PWM_VALUE_SYNC;
	rc = qpnp_lpg_write(lpg, REG_LPG_PWM_SYNC, val);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Write LPG_PWM_SYNC failed, rc=%d\n",
							rc);
		return rc;
	}

	return rc;
}

static int qpnp_lpg_set_lut_pattern(struct qpnp_lpg_channel *lpg,
		unsigned int *pattern, unsigned int length)
{
	struct qpnp_lpg_lut *lut = lpg->chip->lut;
	int i, rc = 0;
	u16 full_duty_value, pwm_values[LPG_LUT_COUNT_MAX + 1] = {0};
	u8 lsb, msb, addr;

	if (length > lpg->max_pattern_length) {
		dev_err(lpg->chip->dev, "new pattern length (%d) larger than predefined (%d)\n",
				length, lpg->max_pattern_length);
		return -EINVAL;
	}

	/* Program LUT pattern */
	mutex_lock(&lut->lock);
	addr = REG_LPG_LUT_1_LSB + lpg->ramp_config.lo_idx * 2;
	for (i = 0; i < length; i++) {
		full_duty_value = 1 << lpg->pwm_config.pwm_size;
		pwm_values[i] = pattern[i] * full_duty_value / 100;

		if (unlikely(pwm_values[i] > full_duty_value)) {
			dev_err(lpg->chip->dev, "PWM value %d exceed the max %d\n",
					pwm_values[i], full_duty_value);
			rc = -EINVAL;
			goto unlock;
		}

		if (pwm_values[i] == full_duty_value)
			pwm_values[i] = full_duty_value - 1;

		lsb = pwm_values[i] & 0xff;
		msb = pwm_values[i] >> 8;
		rc = qpnp_lut_write(lut, addr++, lsb);
		if (rc < 0) {
			dev_err(lpg->chip->dev, "Write NO.%d LUT pattern LSB (%d) failed, rc=%d",
					i, lsb, rc);
			goto unlock;
		}

		rc = qpnp_lut_masked_write(lut, addr++,
				LPG_LUT_VALUE_MSB_MASK, msb);
		if (rc < 0) {
			dev_err(lpg->chip->dev, "Write NO.%d LUT pattern MSB (%d) failed, rc=%d",
					i, msb, rc);
			goto unlock;
		}
	}
	lpg->ramp_config.pattern_length = length;
unlock:
	mutex_unlock(&lut->lock);

	return rc;
}

static int qpnp_lpg_set_ramp_config(struct qpnp_lpg_channel *lpg)
{
	struct lpg_ramp_config *ramp = &lpg->ramp_config;
	u8 lsb, msb, addr, mask, val;
	int rc = 0;

	/* Set ramp step duration */
	lsb = ramp->step_ms & 0xff;
	msb = ramp->step_ms >> 8;
	addr = REG_LPG_RAMP_STEP_DURATION_LSB;
	rc = qpnp_lpg_write(lpg, addr, lsb);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Write RAMP_STEP_DURATION_LSB failed, rc=%d\n",
					rc);
		return rc;
	}
	rc = qpnp_lpg_write(lpg, addr + 1, msb);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Write RAMP_STEP_DURATION_MSB failed, rc=%d\n",
					rc);
		return rc;
	}

	/* Set hi_idx and lo_idx */
	rc = qpnp_lpg_masked_write(lpg, REG_LPG_HI_INDEX,
			LPG_HI_LO_IDX_MASK, ramp->hi_idx);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Write LPG_HI_IDX failed, rc=%d\n",
					rc);
		return rc;
	}

	rc = qpnp_lpg_masked_write(lpg, REG_LPG_LO_INDEX,
			LPG_HI_LO_IDX_MASK, ramp->lo_idx);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Write LPG_LO_IDX failed, rc=%d\n",
					rc);
		return rc;
	}

	/* Set pause_hi/lo_count */
	rc = qpnp_lpg_write(lpg, REG_LPG_PAUSE_HI_MULTIPLIER,
					ramp->pause_hi_count);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Write LPG_PAUSE_HI_MULTIPLIER failed, rc=%d\n",
					rc);
		return rc;
	}

	rc = qpnp_lpg_write(lpg, REG_LPG_PAUSE_LO_MULTIPLIER,
					ramp->pause_lo_count);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Write LPG_PAUSE_LO_MULTIPLIER failed, rc=%d\n",
					rc);
		return rc;
	}

	/* Set LPG_PATTERN_CONFIG */
	addr = REG_LPG_PATTERN_CONFIG;
	mask = LPG_PATTERN_EN_PAUSE_LO | LPG_PATTERN_EN_PAUSE_HI
		| LPG_PATTERN_RAMP_TOGGLE | LPG_PATTERN_REPEAT
		| LPG_PATTERN_RAMP_LO_TO_HI;
	val = 0;
	if (ramp->pause_lo_count != 0)
		val |= LPG_PATTERN_EN_PAUSE_LO;
	if (ramp->pause_hi_count != 0)
		val |= LPG_PATTERN_EN_PAUSE_HI;
	if (ramp->ramp_dir_low_to_hi)
		val |= LPG_PATTERN_RAMP_LO_TO_HI;
	if (ramp->pattern_repeat)
		val |= LPG_PATTERN_REPEAT;
	if (ramp->toggle)
		val |= LPG_PATTERN_RAMP_TOGGLE;

	rc = qpnp_lpg_masked_write(lpg, addr, mask, val);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Write LPG_PATTERN_CONFIG failed, rc=%d\n",
					rc);
		return rc;
	}

	return rc;
}

static void __qpnp_lpg_calc_pwm_period(u64 period_ns,
			struct lpg_pwm_config *pwm_config)
{
	struct lpg_pwm_config configs[NUM_PWM_SIZE];
	int i, j, m, n;
	u64 tmp1, tmp2;
	u64 clk_period_ns = 0, pwm_clk_period_ns;
	u64 clk_delta_ns = U64_MAX, min_clk_delta_ns = U64_MAX;
	u64 pwm_period_delta = U64_MAX, min_pwm_period_delta = U64_MAX;
	int pwm_size_step;

	/*
	 *              (2^pwm_size) * (2^pwm_exp) * prediv * NSEC_PER_SEC
	 * pwm_period = ---------------------------------------------------
	 *                               clk_freq_hz
	 *
	 * Searching the closest settings for the requested PWM period.
	 */
	for (n = 0; n < ARRAY_SIZE(pwm_size); n++) {
		pwm_clk_period_ns = period_ns >> pwm_size[n];
		for (i = ARRAY_SIZE(clk_freq_hz) - 1; i >= 0; i--) {
			for (j = 0; j < ARRAY_SIZE(clk_prediv); j++) {
				for (m = 0; m < ARRAY_SIZE(pwm_exponent); m++) {
					tmp1 = 1 << pwm_exponent[m];
					tmp1 *= clk_prediv[j];
					tmp2 = NSEC_PER_SEC;
					do_div(tmp2, clk_freq_hz[i]);

					clk_period_ns = tmp1 * tmp2;

					clk_delta_ns = abs(pwm_clk_period_ns
						- clk_period_ns);
					/*
					 * Find the closest setting for
					 * PWM frequency predivide value
					 */
					if (clk_delta_ns < min_clk_delta_ns) {
						min_clk_delta_ns
							= clk_delta_ns;
						configs[n].pwm_clk
							= clk_freq_hz[i];
						configs[n].prediv
							= clk_prediv[j];
						configs[n].clk_exp
							= pwm_exponent[m];
						configs[n].pwm_size
							= pwm_size[n];
						configs[n].best_period_ns
							= clk_period_ns;
					}
				}
			}
		}

		configs[n].best_period_ns *= 1 << pwm_size[n];
		/* Find the closest setting for PWM period */
		pwm_period_delta = min_clk_delta_ns << pwm_size[n];
		if (pwm_period_delta < min_pwm_period_delta) {
			min_pwm_period_delta = pwm_period_delta;
			memcpy(pwm_config, &configs[n],
					sizeof(struct lpg_pwm_config));
		}
	}

	/* Larger PWM size can achieve better resolution for PWM duty */
	for (n = ARRAY_SIZE(pwm_size) - 1; n > 0; n--) {
		if (pwm_config->pwm_size >= pwm_size[n])
			break;
		pwm_size_step = pwm_size[n] - pwm_config->pwm_size;
		if (pwm_config->clk_exp >= pwm_size_step) {
			pwm_config->pwm_size = pwm_size[n];
			pwm_config->clk_exp -= pwm_size_step;
		}
	}
	pr_debug("PWM setting for period_ns %llu: pwm_clk = %dHZ, prediv = %d, exponent = %d, pwm_size = %d\n",
			period_ns, pwm_config->pwm_clk, pwm_config->prediv,
			pwm_config->clk_exp, pwm_config->pwm_size);
	pr_debug("Actual period: %lluns\n", pwm_config->best_period_ns);
}

static void __qpnp_lpg_calc_pwm_duty(u64 period_ns, u64 duty_ns,
			struct lpg_pwm_config *pwm_config)
{
	u16 pwm_value, max_pwm_value;
	u64 tmp;

	tmp = (u64)duty_ns << pwm_config->pwm_size;
	pwm_value = (u16)div64_u64(tmp, period_ns);

	max_pwm_value = (1 << pwm_config->pwm_size) - 1;
	if (pwm_value > max_pwm_value)
		pwm_value = max_pwm_value;
	pwm_config->pwm_value = pwm_value;
}

static int qpnp_lpg_config(struct qpnp_lpg_channel *lpg,
		u64 duty_ns, u64 period_ns)
{
	int rc;

	if (duty_ns > period_ns) {
		dev_err(lpg->chip->dev, "Duty %lluns is larger than period %lluns\n",
						duty_ns, period_ns);
		return -EINVAL;
	}

	if (period_ns != lpg->current_period_ns) {
		__qpnp_lpg_calc_pwm_period(period_ns, &lpg->pwm_config);

		/* program LUT if PWM period is changed */
		if (lpg->src_sel == LUT_PATTERN) {
			rc = qpnp_lpg_set_lut_pattern(lpg,
					lpg->ramp_config.pattern,
					lpg->ramp_config.pattern_length);
			if (rc < 0) {
				dev_err(lpg->chip->dev, "set LUT pattern failed for LPG%d, rc=%d\n",
						lpg->lpg_idx, rc);
				return rc;
			}
			lpg->lut_written = true;
		}
	}

	if (period_ns != lpg->current_period_ns ||
			duty_ns != lpg->current_duty_ns)
		__qpnp_lpg_calc_pwm_duty(period_ns, duty_ns, &lpg->pwm_config);

	rc = qpnp_lpg_set_pwm_config(lpg);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Config PWM failed for channel %d, rc=%d\n",
						lpg->lpg_idx, rc);
		return rc;
	}

	lpg->current_period_ns = period_ns;
	lpg->current_duty_ns = duty_ns;

	return rc;
}

static int qpnp_lpg_pwm_config(struct pwm_chip *pwm_chip,
		struct pwm_device *pwm, int duty_ns, int period_ns)
{
	struct qpnp_lpg_channel *lpg;

	lpg = pwm_dev_to_qpnp_lpg(pwm_chip, pwm);
	if (lpg == NULL) {
		dev_err(pwm_chip->dev, "lpg not found\n");
		return -ENODEV;
	}

	return qpnp_lpg_config(lpg, (u64)duty_ns, (u64)period_ns);
}

static int qpnp_lpg_pwm_config_extend(struct pwm_chip *pwm_chip,
		struct pwm_device *pwm, u64 duty_ns, u64 period_ns)
{
	struct qpnp_lpg_channel *lpg;

	lpg = pwm_dev_to_qpnp_lpg(pwm_chip, pwm);
	if (lpg == NULL) {
		dev_err(pwm_chip->dev, "lpg not found\n");
		return -ENODEV;
	}

	return qpnp_lpg_config(lpg, duty_ns, period_ns);
}

static int qpnp_lpg_pwm_src_enable(struct qpnp_lpg_channel *lpg, bool en)
{
	struct qpnp_lpg_chip *chip = lpg->chip;
	struct qpnp_lpg_lut *lut = chip->lut;
	struct pwm_device *pwm;
	u8 mask, val;
	int i, lpg_idx, rc;

	mask = LPG_PWM_SRC_SELECT_MASK | LPG_EN_LPG_OUT_BIT |
					LPG_EN_RAMP_GEN_MASK;
	val = lpg->src_sel << LPG_PWM_SRC_SELECT_SHIFT;

	if (lpg->src_sel == LUT_PATTERN)
		val |= 1 << LPG_EN_RAMP_GEN_SHIFT;

	if (en)
		val |= 1 << LPG_EN_LPG_OUT_SHIFT;

	rc = qpnp_lpg_masked_write(lpg, REG_LPG_ENABLE_CONTROL, mask, val);
	if (rc < 0) {
		dev_err(chip->dev, "Write LPG_ENABLE_CONTROL failed, rc=%d\n",
				rc);
		return rc;
	}

	if (lpg->src_sel == LUT_PATTERN && en) {
		val = 1 << lpg->lpg_idx;
		for (i = 0; i < chip->num_lpgs; i++) {
			if (chip->lpg_group == NULL)
				break;
			if (chip->lpg_group[i] == 0)
				break;
			lpg_idx = chip->lpg_group[i] - 1;
			pwm = &chip->pwm_chip.pwms[lpg_idx];
			if ((pwm_get_output_type(pwm) == PWM_OUTPUT_MODULATED)
						&& pwm_is_enabled(pwm)) {
				rc = qpnp_lpg_masked_write(&chip->lpgs[lpg_idx],
						REG_LPG_ENABLE_CONTROL,
						LPG_EN_LPG_OUT_BIT, 0);
				if (rc < 0)
					break;
				rc = qpnp_lpg_masked_write(&chip->lpgs[lpg_idx],
						REG_LPG_ENABLE_CONTROL,
						LPG_EN_LPG_OUT_BIT,
						LPG_EN_LPG_OUT_BIT);
				if (rc < 0)
					break;
				val |= 1 << lpg_idx;
			}
		}
		mutex_lock(&lut->lock);
		rc = qpnp_lut_write(lut, REG_LPG_LUT_RAMP_CONTROL, val);
		if (rc < 0)
			dev_err(chip->dev, "Write LPG_LUT_RAMP_CONTROL failed, rc=%d\n",
					rc);
		mutex_unlock(&lut->lock);
	}

	return rc;
}

static int qpnp_lpg_pwm_set_output_type(struct pwm_chip *pwm_chip,
		struct pwm_device *pwm, enum pwm_output_type output_type)
{
	struct qpnp_lpg_channel *lpg;
	enum lpg_src src_sel;
	int rc;

	lpg = pwm_dev_to_qpnp_lpg(pwm_chip, pwm);
	if (lpg == NULL) {
		dev_err(pwm_chip->dev, "lpg not found\n");
		return -ENODEV;
	}

	if (lpg->chip->lut == NULL) {
		pr_debug("lpg%d only support PWM mode\n", lpg->lpg_idx);
		return 0;
	}

	src_sel = (output_type == PWM_OUTPUT_MODULATED) ?
				LUT_PATTERN : PWM_VALUE;
	if (src_sel == lpg->src_sel)
		return 0;

	if (src_sel == LUT_PATTERN) {
		/* program LUT if it's never been programmed */
		if (!lpg->lut_written) {
			rc = qpnp_lpg_set_lut_pattern(lpg,
					lpg->ramp_config.pattern,
					lpg->ramp_config.pattern_length);
			if (rc < 0) {
				dev_err(lpg->chip->dev, "set LUT pattern failed for LPG%d, rc=%d\n",
						lpg->lpg_idx, rc);
				return rc;
			}
			lpg->lut_written = true;
		}

		rc = qpnp_lpg_set_ramp_config(lpg);
		if (rc < 0) {
			dev_err(pwm_chip->dev, "Config LPG%d ramping failed, rc=%d\n",
					lpg->lpg_idx, rc);
			return rc;
		}
	}

	lpg->src_sel = src_sel;

	if (pwm_is_enabled(pwm)) {
		rc = qpnp_lpg_pwm_src_enable(lpg, true);
		if (rc < 0) {
			dev_err(pwm_chip->dev, "Enable PWM output failed for channel %d, rc=%d\n",
					lpg->lpg_idx, rc);
			return rc;
		}
	}

	return 0;
}

static int qpnp_lpg_pwm_set_output_pattern(struct pwm_chip *pwm_chip,
	struct pwm_device *pwm, struct pwm_output_pattern *output_pattern)
{
	struct qpnp_lpg_channel *lpg;
	u64 period_ns, duty_ns, tmp;
	u32 *percentages;
	int rc = 0, i;

	lpg = pwm_dev_to_qpnp_lpg(pwm_chip, pwm);
	if (lpg == NULL) {
		dev_err(pwm_chip->dev, "lpg not found\n");
		return -ENODEV;
	}

	if (output_pattern->num_entries > lpg->max_pattern_length) {
		dev_err(lpg->chip->dev, "pattern length %d shouldn't exceed %d\n",
				output_pattern->num_entries,
				lpg->max_pattern_length);
		return -EINVAL;
	}

	percentages = kcalloc(output_pattern->num_entries,
				sizeof(u32), GFP_KERNEL);
	if (!percentages)
		return -ENOMEM;

	period_ns = pwm_get_period_extend(pwm);
	for (i = 0; i < output_pattern->num_entries; i++) {
		duty_ns = output_pattern->duty_pattern[i];
		if (duty_ns > period_ns) {
			dev_err(lpg->chip->dev, "duty %lluns is larger than period %lluns\n",
					duty_ns, period_ns);
			goto err;
		}
		/* Translate the pattern in duty_ns to percentage */
		tmp = (u64)duty_ns * 100;
		percentages[i] = (u32)div64_u64(tmp, period_ns);
	}

	rc = qpnp_lpg_set_lut_pattern(lpg, percentages,
			output_pattern->num_entries);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Set LUT pattern failed for LPG%d, rc=%d\n",
				lpg->lpg_idx, rc);
		goto err;
	}

	lpg->lut_written = true;
	memcpy(lpg->ramp_config.pattern, percentages,
			output_pattern->num_entries);
	lpg->ramp_config.hi_idx = lpg->ramp_config.lo_idx +
				output_pattern->num_entries - 1;

	tmp = (u64)output_pattern->cycles_per_duty * period_ns;
	do_div(tmp, NSEC_PER_MSEC);
	lpg->ramp_config.step_ms = (u16)tmp;

	rc = qpnp_lpg_set_ramp_config(lpg);
	if (rc < 0)
		dev_err(pwm_chip->dev, "Config LPG%d ramping failed, rc=%d\n",
				lpg->lpg_idx, rc);
err:
	kfree(percentages);

	return rc;
}

static int qpnp_lpg_pwm_enable(struct pwm_chip *pwm_chip,
				struct pwm_device *pwm)
{
	struct qpnp_lpg_channel *lpg;
	int rc = 0;

	lpg = pwm_dev_to_qpnp_lpg(pwm_chip, pwm);
	if (lpg == NULL) {
		dev_err(pwm_chip->dev, "lpg not found\n");
		return -ENODEV;
	}

	rc = qpnp_lpg_set_glitch_removal(lpg, true);
	if (rc < 0) {
		dev_err(lpg->chip->dev, "Enable glitch-removal failed, rc=%d\n",
							rc);
		return rc;
	}

	rc = qpnp_lpg_pwm_src_enable(lpg, true);
	if (rc < 0)
		dev_err(pwm_chip->dev, "Enable PWM output failed for channel %d, rc=%d\n",
						lpg->lpg_idx, rc);

	return rc;
}

static void qpnp_lpg_pwm_disable(struct pwm_chip *pwm_chip,
				struct pwm_device *pwm)
{
	struct qpnp_lpg_channel *lpg;
	int rc;

	lpg = pwm_dev_to_qpnp_lpg(pwm_chip, pwm);
	if (lpg == NULL) {
		dev_err(pwm_chip->dev, "lpg not found\n");
		return;
	}

	rc = qpnp_lpg_pwm_src_enable(lpg, false);
	if (rc < 0) {
		dev_err(pwm_chip->dev, "Disable PWM output failed for channel %d, rc=%d\n",
						lpg->lpg_idx, rc);
		return;
	}

	rc = qpnp_lpg_set_glitch_removal(lpg, false);
	if (rc < 0)
		dev_err(lpg->chip->dev, "Disable glitch-removal failed, rc=%d\n",
							rc);
}

static int qpnp_lpg_pwm_output_types_supported(struct pwm_chip *pwm_chip,
				struct pwm_device *pwm)
{
	enum pwm_output_type type = PWM_OUTPUT_FIXED;
	struct qpnp_lpg_channel *lpg;

	lpg = pwm_dev_to_qpnp_lpg(pwm_chip, pwm);
	if (lpg == NULL) {
		dev_err(pwm_chip->dev, "lpg not found\n");
		return type;
	}

	if (lpg->chip->lut != NULL)
		type |= PWM_OUTPUT_MODULATED;

	return type;
}

#ifdef CONFIG_DEBUG_FS
static void qpnp_lpg_pwm_dbg_show(struct pwm_chip *pwm_chip, struct seq_file *s)
{
	struct qpnp_lpg_channel *lpg;
	struct lpg_pwm_config *cfg;
	struct lpg_ramp_config *ramp;
	struct pwm_device *pwm;
	int i, j;

	for (i = 0; i < pwm_chip->npwm; i++) {
		pwm = &pwm_chip->pwms[i];

		lpg = pwm_dev_to_qpnp_lpg(pwm_chip, pwm);
		if (lpg == NULL) {
			dev_err(pwm_chip->dev, "lpg not found\n");
			return;
		}

		if (test_bit(PWMF_REQUESTED, &pwm->flags)) {
			seq_printf(s, "LPG %d is requested by %s\n",
					lpg->lpg_idx + 1, pwm->label);
		} else {
			seq_printf(s, "LPG %d is free\n",
					lpg->lpg_idx + 1);
			continue;
		}

		if (pwm_is_enabled(pwm)) {
			seq_puts(s, "  enabled\n");
		} else {
			seq_puts(s, "  disabled\n");
			continue;
		}

		cfg = &lpg->pwm_config;
		seq_printf(s, "     clk = %dHz\n", cfg->pwm_clk);
		seq_printf(s, "     pwm_size = %d\n", cfg->pwm_size);
		seq_printf(s, "     prediv = %d\n", cfg->prediv);
		seq_printf(s, "     exponent = %d\n", cfg->clk_exp);
		seq_printf(s, "     pwm_value = %d\n", cfg->pwm_value);
		seq_printf(s, "  Requested period: %lluns, best period = %lluns\n",
			pwm_get_period_extend(pwm), cfg->best_period_ns);

		ramp = &lpg->ramp_config;
		if (pwm_get_output_type(pwm) == PWM_OUTPUT_MODULATED) {
			seq_puts(s, "  ramping duty percentages:");
			for (j = 0; j < ramp->pattern_length; j++)
				seq_printf(s, " %d", ramp->pattern[j]);
			seq_puts(s, "\n");
			seq_printf(s, "  ramping time per step: %dms\n",
					ramp->step_ms);
			seq_printf(s, "  ramping low index: %d\n",
					ramp->lo_idx);
			seq_printf(s, "  ramping high index: %d\n",
					ramp->hi_idx);
			seq_printf(s, "  ramping from low to high: %d\n",
					ramp->ramp_dir_low_to_hi);
			seq_printf(s, "  ramping pattern repeat: %d\n",
					ramp->pattern_repeat);
			seq_printf(s, "  ramping toggle: %d\n",
					ramp->toggle);
			seq_printf(s, "  ramping pause count at low index: %d\n",
					ramp->pause_lo_count);
			seq_printf(s, "  ramping pause count at high index: %d\n",
					ramp->pause_hi_count);
		}
	}
}
#endif

static const struct pwm_ops qpnp_lpg_pwm_ops = {
	.config = qpnp_lpg_pwm_config,
	.config_extend = qpnp_lpg_pwm_config_extend,
	.get_output_type_supported = qpnp_lpg_pwm_output_types_supported,
	.set_output_type = qpnp_lpg_pwm_set_output_type,
	.set_output_pattern = qpnp_lpg_pwm_set_output_pattern,
	.enable = qpnp_lpg_pwm_enable,
	.disable = qpnp_lpg_pwm_disable,
#ifdef CONFIG_DEBUG_FS
	.dbg_show = qpnp_lpg_pwm_dbg_show,
#endif
	.owner = THIS_MODULE,
};

static int qpnp_lpg_parse_dt(struct qpnp_lpg_chip *chip)
{
	struct device_node *child;
	struct qpnp_lpg_channel *lpg;
	struct lpg_ramp_config *ramp;
	int rc = 0, i;
	u32 base, length, lpg_chan_id, tmp;
	const __be32 *addr;

	addr = of_get_address(chip->dev->of_node, 0, NULL, NULL);
	if (!addr) {
		dev_err(chip->dev, "Get %s address failed\n", LPG_BASE);
		return -EINVAL;
	}

	base = be32_to_cpu(addr[0]);
	length = be32_to_cpu(addr[1]);

	chip->num_lpgs = length / REG_SIZE_PER_LPG;
	chip->lpgs = devm_kcalloc(chip->dev, chip->num_lpgs,
			sizeof(*chip->lpgs), GFP_KERNEL);
	if (!chip->lpgs)
		return -ENOMEM;

	for (i = 0; i < chip->num_lpgs; i++) {
		chip->lpgs[i].chip = chip;
		chip->lpgs[i].lpg_idx = i;
		chip->lpgs[i].reg_base = base + i * REG_SIZE_PER_LPG;
		chip->lpgs[i].src_sel = PWM_VALUE;
		rc = qpnp_lpg_read(&chip->lpgs[i], REG_LPG_PERPH_SUBTYPE,
				&chip->lpgs[i].subtype);
		if (rc < 0) {
			dev_err(chip->dev, "Read subtype failed, rc=%d\n", rc);
			return rc;
		}
	}

	addr = of_get_address(chip->dev->of_node, 1, NULL, NULL);
	if (!addr) {
		pr_debug("NO LUT address assigned\n");
		return 0;
	}

	chip->lut = devm_kmalloc(chip->dev, sizeof(*chip->lut), GFP_KERNEL);
	if (!chip->lut)
		return -ENOMEM;

	chip->lut->chip = chip;
	chip->lut->reg_base = be32_to_cpu(*addr);
	mutex_init(&chip->lut->lock);

	rc = of_property_count_elems_of_size(chip->dev->of_node,
			"qcom,lut-patterns", sizeof(u32));
	if (rc < 0) {
		dev_err(chip->dev, "Read qcom,lut-patterns failed, rc=%d\n",
							rc);
		return rc;
	}

	length = rc;
	if (length > LPG_LUT_COUNT_MAX) {
		dev_err(chip->dev, "qcom,lut-patterns length %d exceed max %d\n",
				length, LPG_LUT_COUNT_MAX);
		return -EINVAL;
	}

	chip->lut->pattern = devm_kcalloc(chip->dev, LPG_LUT_COUNT_MAX,
			sizeof(*chip->lut->pattern), GFP_KERNEL);
	if (!chip->lut->pattern)
		return -ENOMEM;

	rc = of_property_read_u32_array(chip->dev->of_node, "qcom,lut-patterns",
					chip->lut->pattern, length);
	if (rc < 0) {
		dev_err(chip->dev, "Get qcom,lut-patterns failed, rc=%d\n",
				rc);
		return rc;
	}

	if (of_get_available_child_count(chip->dev->of_node) == 0) {
		dev_err(chip->dev, "No ramp configuration for any LPG\n");
		return -EINVAL;
	}

	for_each_available_child_of_node(chip->dev->of_node, child) {
		rc = of_property_read_u32(child, "qcom,lpg-chan-id",
						&lpg_chan_id);
		if (rc < 0) {
			dev_err(chip->dev, "Get qcom,lpg-chan-id failed for node %s, rc=%d\n",
					child->name, rc);
			return rc;
		}

		if (lpg_chan_id > chip->num_lpgs) {
			dev_err(chip->dev, "lpg-chann-id %d is out of range 1~%d\n",
					lpg_chan_id, chip->num_lpgs);
			return -EINVAL;
		}

		/* lpg channel id is indexed from 1 in hardware */
		lpg = &chip->lpgs[lpg_chan_id - 1];
		ramp = &lpg->ramp_config;

		rc = of_property_read_u32(child, "qcom,ramp-step-ms", &tmp);
		if (rc < 0) {
			dev_err(chip->dev, "get qcom,ramp-step-ms failed for lpg%d, rc=%d\n",
					lpg_chan_id, rc);
			return rc;
		}
		ramp->step_ms = (u16)tmp;

		rc = of_property_read_u32(child, "qcom,ramp-low-index", &tmp);
		if (rc < 0) {
			dev_err(chip->dev, "get qcom,ramp-low-index failed for lpg%d, rc=%d\n",
						lpg_chan_id, rc);
			return rc;
		}
		ramp->lo_idx = (u8)tmp;
		if (ramp->lo_idx >= LPG_LUT_COUNT_MAX) {
			dev_err(chip->dev, "qcom,ramp-low-index should less than max %d\n",
					LPG_LUT_COUNT_MAX);
			return -EINVAL;
		}

		rc = of_property_read_u32(child, "qcom,ramp-high-index", &tmp);
		if (rc < 0) {
			dev_err(chip->dev, "get qcom,ramp-high-index failed for lpg%d, rc=%d\n",
						lpg_chan_id, rc);
			return rc;
		}
		ramp->hi_idx = (u8)tmp;

		if (ramp->hi_idx > LPG_LUT_COUNT_MAX) {
			dev_err(chip->dev, "qcom,ramp-high-index shouldn't exceed max %d\n",
						LPG_LUT_COUNT_MAX);
			return -EINVAL;
		}

		if (ramp->hi_idx <= ramp->lo_idx) {
			dev_err(chip->dev, "high-index(%d) should be larger than low-index(%d)\n",
						ramp->hi_idx, ramp->lo_idx);
			return -EINVAL;
		}

		ramp->pattern_length = ramp->hi_idx - ramp->lo_idx + 1;
		ramp->pattern = &chip->lut->pattern[ramp->lo_idx];
		lpg->max_pattern_length = ramp->pattern_length;

		rc = of_property_read_u32(child,
				"qcom,ramp-pause-hi-count", &tmp);
		if (rc < 0)
			ramp->pause_hi_count = 0;
		else
			ramp->pause_hi_count = (u8)tmp;

		rc = of_property_read_u32(child,
				"qcom,ramp-pause-lo-count", &tmp);
		if (rc < 0)
			ramp->pause_lo_count = 0;
		else
			ramp->pause_lo_count = (u8)tmp;

		ramp->ramp_dir_low_to_hi = of_property_read_bool(child,
				"qcom,ramp-from-low-to-high");

		ramp->pattern_repeat = of_property_read_bool(child,
				"qcom,ramp-pattern-repeat");

		ramp->toggle =  of_property_read_bool(child,
				"qcom,ramp-toggle");
	}

	rc = of_property_count_elems_of_size(chip->dev->of_node,
			"qcom,sync-channel-ids", sizeof(u32));
	if (rc < 0)
		return 0;

	length = rc;
	if (length > chip->num_lpgs) {
		dev_err(chip->dev, "qcom,sync-channel-ids has too many channels: %d\n",
				length);
		return -EINVAL;
	}

	chip->lpg_group = devm_kcalloc(chip->dev, chip->num_lpgs,
			sizeof(u32), GFP_KERNEL);
	if (!chip->lpg_group)
		return -ENOMEM;

	rc = of_property_read_u32_array(chip->dev->of_node,
			"qcom,sync-channel-ids", chip->lpg_group, length);
	if (rc < 0) {
		dev_err(chip->dev, "Get qcom,sync-channel-ids failed, rc=%d\n",
				rc);
		return rc;
	}

	for (i = 0; i < length; i++) {
		if (chip->lpg_group[i] <= 0 ||
				chip->lpg_group[i] > chip->num_lpgs) {
			dev_err(chip->dev, "lpg_group[%d]: %d is not a valid channel\n",
					i, chip->lpg_group[i]);
			return -EINVAL;
		}
	}

	/*
	 * The LPG channel in the same group should have the same ramping
	 * configuration, so force to use the ramping configuration of the
	 * 1st LPG channel in the group for sychronization.
	 */
	lpg = &chip->lpgs[chip->lpg_group[0] - 1];
	ramp = &lpg->ramp_config;

	for (i = 1; i < length; i++) {
		lpg = &chip->lpgs[chip->lpg_group[i] - 1];
		memcpy(&lpg->ramp_config, ramp, sizeof(struct lpg_ramp_config));
	}

	return 0;
}

static int qpnp_lpg_probe(struct platform_device *pdev)
{
	int rc;
	struct qpnp_lpg_chip *chip;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap) {
		dev_err(chip->dev, "Getting regmap failed\n");
		return -EINVAL;
	}

	mutex_init(&chip->bus_lock);
	rc = qpnp_lpg_parse_dt(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Devicetree properties parsing failed, rc=%d\n",
				rc);
		goto err_out;
	}

	dev_set_drvdata(chip->dev, chip);
	chip->pwm_chip.dev = chip->dev;
	chip->pwm_chip.base = -1;
	chip->pwm_chip.npwm = chip->num_lpgs;
	chip->pwm_chip.ops = &qpnp_lpg_pwm_ops;

	rc = pwmchip_add(&chip->pwm_chip);
	if (rc < 0) {
		dev_err(chip->dev, "Add pwmchip failed, rc=%d\n", rc);
		goto err_out;
	}

	return 0;
err_out:
	mutex_destroy(&chip->bus_lock);
	return rc;
}

static int qpnp_lpg_remove(struct platform_device *pdev)
{
	struct qpnp_lpg_chip *chip = dev_get_drvdata(&pdev->dev);
	int rc = 0;

	rc = pwmchip_remove(&chip->pwm_chip);
	if (rc < 0)
		dev_err(chip->dev, "Remove pwmchip failed, rc=%d\n", rc);

	mutex_destroy(&chip->bus_lock);
	dev_set_drvdata(chip->dev, NULL);

	return rc;
}

static const struct of_device_id qpnp_lpg_of_match[] = {
	{ .compatible = "qcom,pwm-lpg",},
	{ },
};

static struct platform_driver qpnp_lpg_driver = {
	.driver		= {
		.name		= "qcom,pwm-lpg",
		.of_match_table	= qpnp_lpg_of_match,
	},
	.probe		= qpnp_lpg_probe,
	.remove		= qpnp_lpg_remove,
};
module_platform_driver(qpnp_lpg_driver);

MODULE_DESCRIPTION("QTI LPG driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("pwm:pwm-lpg");
