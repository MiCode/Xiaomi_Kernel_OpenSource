/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
/*
 * Qualcomm PM8XXX Pulse Width Modulation (PWM) driver
 *
 * The HW module is also called LPG (Light Pulse Generator).
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/pwm.h>

#define PM8XXX_LPG_BANKS		8
#define PM8XXX_PWM_CHANNELS		PM8XXX_LPG_BANKS

#define PM8XXX_LPG_CTL_REGS		7

/* PM8XXX PWM */
#define SSBI_REG_ADDR_LPG_CTL_BASE	0x13C
#define SSBI_REG_ADDR_LPG_CTL(n)	(SSBI_REG_ADDR_LPG_CTL_BASE + (n))
#define SSBI_REG_ADDR_LPG_BANK_SEL	0x143
#define SSBI_REG_ADDR_LPG_BANK_EN	0x144
#define SSBI_REG_ADDR_LPG_LUT_CFG0	0x145
#define SSBI_REG_ADDR_LPG_LUT_CFG1	0x146

/* Control 0 */
#define PM8XXX_PWM_1KHZ_COUNT_MASK	0xF0
#define PM8XXX_PWM_1KHZ_COUNT_SHIFT	4

#define PM8XXX_PWM_1KHZ_COUNT_MAX	15

#define PM8XXX_PWM_OUTPUT_EN		0x08
#define PM8XXX_PWM_PWM_EN		0x04
#define PM8XXX_PWM_RAMP_GEN_EN		0x02
#define PM8XXX_PWM_RAMP_START		0x01

#define PM8XXX_PWM_PWM_START		(PM8XXX_PWM_OUTPUT_EN \
					| PM8XXX_PWM_PWM_EN)
#define PM8XXX_PWM_RAMP_GEN_START	(PM8XXX_PWM_RAMP_GEN_EN \
					| PM8XXX_PWM_RAMP_START)

/* Control 1 */
#define PM8XXX_PWM_REVERSE_EN		0x80
#define PM8XXX_PWM_BYPASS_LUT		0x40
#define PM8XXX_PWM_HIGH_INDEX_MASK	0x3F

/* Control 2 */
#define PM8XXX_PWM_LOOP_EN		0x80
#define PM8XXX_PWM_RAMP_UP		0x40
#define PM8XXX_PWM_LOW_INDEX_MASK	0x3F

/* Control 3 */
#define PM8XXX_PWM_VALUE_BIT7_0		0xFF
#define PM8XXX_PWM_VALUE_BIT5_0		0x3F

/* Control 4 */
#define PM8XXX_PWM_VALUE_BIT8		0x80

#define PM8XXX_PWM_CLK_SEL_MASK		0x60
#define PM8XXX_PWM_CLK_SEL_SHIFT	5

#define PM8XXX_PWM_CLK_SEL_NO		0
#define PM8XXX_PWM_CLK_SEL_1KHZ		1
#define PM8XXX_PWM_CLK_SEL_32KHZ	2
#define PM8XXX_PWM_CLK_SEL_19P2MHZ	3

#define PM8XXX_PWM_PREDIVIDE_MASK	0x18
#define PM8XXX_PWM_PREDIVIDE_SHIFT	3

#define PM8XXX_PWM_PREDIVIDE_2		0
#define PM8XXX_PWM_PREDIVIDE_3		1
#define PM8XXX_PWM_PREDIVIDE_5		2
#define PM8XXX_PWM_PREDIVIDE_6		3

#define PM8XXX_PWM_M_MASK		0x07
#define PM8XXX_PWM_M_MIN		0
#define PM8XXX_PWM_M_MAX		7

/* Control 5 */
#define PM8XXX_PWM_PAUSE_COUNT_HI_MASK		0xFC
#define PM8XXX_PWM_PAUSE_COUNT_HI_SHIFT		2

#define PM8XXX_PWM_PAUSE_ENABLE_HIGH		0x02
#define PM8XXX_PWM_SIZE_9_BIT			0x01

/* Control 6 */
#define PM8XXX_PWM_PAUSE_COUNT_LO_MASK		0xFC
#define PM8XXX_PWM_PAUSE_COUNT_LO_SHIFT		2

#define PM8XXX_PWM_PAUSE_ENABLE_LOW		0x02
#define PM8XXX_PWM_RESERVED			0x01

#define PM8XXX_PWM_PAUSE_COUNT_MAX		56 /* < 2^6 = 64 */

/* LUT_CFG1 */
#define PM8XXX_PWM_LUT_READ			0x40

/*
 * PWM Frequency = Clock Frequency / (N * T)
 *	or
 * PWM Period = Clock Period * (N * T)
 *	where
 * N = 2^9 or 2^6 for 9-bit or 6-bit PWM size
 * T = Pre-divide * 2^m, where m = 0..7 (exponent)
 *
 * This is the formula to figure out m for the best pre-divide and clock:
 * (PWM Period / N) / 2^m = (Pre-divide * Clock Period)
 */
#define NUM_CLOCKS	3

#define NSEC_1000HZ	(NSEC_PER_SEC / 1000)
#define NSEC_32768HZ	(NSEC_PER_SEC / 32768)
#define NSEC_19P2MHZ	(NSEC_PER_SEC / 19200000)

#define CLK_PERIOD_MIN	NSEC_19P2MHZ
#define CLK_PERIOD_MAX	NSEC_1000HZ

#define NUM_PRE_DIVIDE	3	/* No default support for pre-divide = 6 */

#define PRE_DIVIDE_0		2
#define PRE_DIVIDE_1		3
#define PRE_DIVIDE_2		5

#define PRE_DIVIDE_MIN		PRE_DIVIDE_0
#define PRE_DIVIDE_MAX		PRE_DIVIDE_2

static unsigned int pt_t[NUM_PRE_DIVIDE][NUM_CLOCKS] = {
	{	PRE_DIVIDE_0 * NSEC_1000HZ,
		PRE_DIVIDE_0 * NSEC_32768HZ,
		PRE_DIVIDE_0 * NSEC_19P2MHZ,
	},
	{	PRE_DIVIDE_1 * NSEC_1000HZ,
		PRE_DIVIDE_1 * NSEC_32768HZ,
		PRE_DIVIDE_1 * NSEC_19P2MHZ,
	},
	{	PRE_DIVIDE_2 * NSEC_1000HZ,
		PRE_DIVIDE_2 * NSEC_32768HZ,
		PRE_DIVIDE_2 * NSEC_19P2MHZ,
	},
};

#define MIN_MPT	((PRE_DIVIDE_MIN * CLK_PERIOD_MIN) << PM8XXX_PWM_M_MIN)
#define MAX_MPT	((PRE_DIVIDE_MAX * CLK_PERIOD_MAX) << PM8XXX_PWM_M_MAX)

/* Private data */
struct pm8xxx_pwm_chip;

struct pwm_device {
	int			pwm_id;		/* = bank/channel id */
	int			in_use;
	const char		*label;
	struct pm8xxx_pwm_period	period;
	int			pwm_value;
	int			pwm_period;
	int			pwm_duty;
	u8			pwm_ctl[PM8XXX_LPG_CTL_REGS];
	int			irq;
	struct pm8xxx_pwm_chip	*chip;
	int			bypass_lut;
};

struct pm8xxx_pwm_chip {
	struct pwm_device		pwm_dev[PM8XXX_PWM_CHANNELS];
	u8				bank_mask;
	struct mutex			pwm_mutex;
	struct device			*dev;
};

static struct pm8xxx_pwm_chip	*pwm_chip;

struct pm8xxx_pwm_lut {
	/* LUT parameters */
	int	lut_duty_ms;
	int	lut_lo_index;
	int	lut_hi_index;
	int	lut_pause_hi;
	int	lut_pause_lo;
	int	flags;
};

static const u16 duty_msec[PM8XXX_PWM_1KHZ_COUNT_MAX + 1] = {
	0, 1, 2, 3, 4, 6, 8, 16, 18, 24, 32, 36, 64, 128, 256, 512
};

static const u16 pause_count[PM8XXX_PWM_PAUSE_COUNT_MAX + 1] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	23, 28, 31, 42, 47, 56, 63, 83, 94, 111, 125, 167, 188, 222, 250, 333,
	375, 500, 667, 750, 800, 900, 1000, 1100,
	1200, 1300, 1400, 1500, 1600, 1800, 2000, 2500,
	3000, 3500, 4000, 4500, 5000, 5500, 6000, 6500,
	7000
};

/* Internal functions */
static int pm8xxx_pwm_bank_enable(struct pwm_device *pwm, int enable)
{
	int	rc;
	u8	reg;
	struct pm8xxx_pwm_chip	*chip;

	chip = pwm->chip;

	if (enable)
		reg = chip->bank_mask | (1 << pwm->pwm_id);
	else
		reg = chip->bank_mask & ~(1 << pwm->pwm_id);

	rc = pm8xxx_writeb(chip->dev->parent, SSBI_REG_ADDR_LPG_BANK_EN, reg);
	if (rc) {
		pr_err("pm8xxx_write(): rc=%d (Enable LPG Bank)\n", rc);
		return rc;
	}
	chip->bank_mask = reg;

	return 0;
}

static int pm8xxx_pwm_bank_sel(struct pwm_device *pwm)
{
	int	rc;

	rc = pm8xxx_writeb(pwm->chip->dev->parent, SSBI_REG_ADDR_LPG_BANK_SEL,
			   pwm->pwm_id);
	if (rc)
		pr_err("pm8xxx_write(): rc=%d (Select PWM Bank)\n", rc);
	return rc;
}

static int pm8xxx_pwm_start(struct pwm_device *pwm, int start, int ramp_start)
{
	int	rc;
	u8	reg;

	if (start) {
		reg = pwm->pwm_ctl[0] | PM8XXX_PWM_PWM_START;
		if (ramp_start)
			reg |= PM8XXX_PWM_RAMP_GEN_START;
		else
			reg &= ~PM8XXX_PWM_RAMP_GEN_START;
	} else {
		reg = pwm->pwm_ctl[0] & ~PM8XXX_PWM_PWM_START;
		reg &= ~PM8XXX_PWM_RAMP_GEN_START;
	}

	rc = pm8xxx_writeb(pwm->chip->dev->parent, SSBI_REG_ADDR_LPG_CTL(0),
			   reg);
	if (rc)
		pr_err("pm8xxx_write(): rc=%d (Enable PWM Ctl 0)\n", rc);
	else
		pwm->pwm_ctl[0] = reg;
	return rc;
}

static void pm8xxx_pwm_calc_period(unsigned int period_us,
				   struct pm8xxx_pwm_period *period)
{
	int	n, m, clk, div;
	int	best_m, best_div, best_clk;
	int	last_err, cur_err, better_err, better_m;
	unsigned int	tmp_p, last_p, min_err, period_n;

	/* PWM Period / N */
	if (period_us < (40 * USEC_PER_SEC)) {  /* ~6-bit max */
		period_n = (period_us * NSEC_PER_USEC) >> 6;
		n = 6;
	} else if (period_us < (274 * USEC_PER_SEC)) { /* overflow threshold */
		period_n = (period_us >> 6) * NSEC_PER_USEC;
		if (period_n >= MAX_MPT) {
			n = 9;
			period_n >>= 3;
		} else {
			n = 6;
		}
	} else {
		period_n = (period_us >> 9) * NSEC_PER_USEC;
		n = 9;
	}

	min_err = MAX_MPT;
	best_m = 0;
	best_clk = 0;
	best_div = 0;
	for (clk = 0; clk < NUM_CLOCKS; clk++) {
		for (div = 0; div < NUM_PRE_DIVIDE; div++) {
			tmp_p = period_n;
			last_p = tmp_p;
			for (m = 0; m <= PM8XXX_PWM_M_MAX; m++) {
				if (tmp_p <= pt_t[div][clk]) {
					/* Found local best */
					if (!m) {
						better_err = pt_t[div][clk] -
							tmp_p;
						better_m = m;
					} else {
						last_err = last_p -
							pt_t[div][clk];
						cur_err = pt_t[div][clk] -
							tmp_p;

						if (cur_err < last_err) {
							better_err = cur_err;
							better_m = m;
						} else {
							better_err = last_err;
							better_m = m - 1;
						}
					}

					if (better_err < min_err) {
						min_err = better_err;
						best_m = better_m;
						best_clk = clk;
						best_div = div;
					}
					break;
				} else {
					last_p = tmp_p;
					tmp_p >>= 1;
				}
			}
		}
	}

	/* Use higher resolution */
	if (best_m >= 3 && n == 6) {
		n += 3;
		best_m -= 3;
	}

	period->pwm_size = n;
	period->clk = best_clk;
	period->pre_div = best_div;
	period->pre_div_exp = best_m;
}

static int pm8xxx_pwm_configure(struct pwm_device *pwm,
				struct pm8xxx_pwm_lut *lut)
{
	int	i, rc, len;
	u8	reg, ramp_enabled = 0;
	struct pm8xxx_pwm_period *period = &pwm->period;

	reg = (period->pwm_size > 6) ? PM8XXX_PWM_SIZE_9_BIT : 0;
	pwm->pwm_ctl[5] = reg;

	reg = ((period->clk + 1) << PM8XXX_PWM_CLK_SEL_SHIFT)
		& PM8XXX_PWM_CLK_SEL_MASK;
	reg |= (period->pre_div << PM8XXX_PWM_PREDIVIDE_SHIFT)
		& PM8XXX_PWM_PREDIVIDE_MASK;
	reg |= period->pre_div_exp & PM8XXX_PWM_M_MASK;
	pwm->pwm_ctl[4] = reg;

	if (pwm->bypass_lut) {
		pwm->pwm_ctl[0] &= PM8XXX_PWM_PWM_START; /* keep enabled */
		pwm->pwm_ctl[1] = PM8XXX_PWM_BYPASS_LUT;
		pwm->pwm_ctl[2] = 0;

		if (period->pwm_size > 6) {
			pwm->pwm_ctl[3] = pwm->pwm_value
						& PM8XXX_PWM_VALUE_BIT7_0;
			pwm->pwm_ctl[4] |= (pwm->pwm_value >> 1)
						& PM8XXX_PWM_VALUE_BIT8;
		} else {
			pwm->pwm_ctl[3] = pwm->pwm_value
						& PM8XXX_PWM_VALUE_BIT5_0;
		}

		len = 6;
	} else {
		int	pause_cnt, j;

		/* Linear search for duty time */
		for (i = 0; i < PM8XXX_PWM_1KHZ_COUNT_MAX; i++) {
			if (duty_msec[i] >= lut->lut_duty_ms)
				break;
		}

		ramp_enabled = pwm->pwm_ctl[0] & PM8XXX_PWM_RAMP_GEN_START;
		pwm->pwm_ctl[0] &= PM8XXX_PWM_PWM_START; /* keep enabled */
		pwm->pwm_ctl[0] |= (i << PM8XXX_PWM_1KHZ_COUNT_SHIFT) &
					PM8XXX_PWM_1KHZ_COUNT_MASK;
		pwm->pwm_ctl[1] = lut->lut_hi_index &
					PM8XXX_PWM_HIGH_INDEX_MASK;
		pwm->pwm_ctl[2] = lut->lut_lo_index &
					PM8XXX_PWM_LOW_INDEX_MASK;

		if (lut->flags & PM_PWM_LUT_REVERSE)
			pwm->pwm_ctl[1] |= PM8XXX_PWM_REVERSE_EN;
		if (lut->flags & PM_PWM_LUT_RAMP_UP)
			pwm->pwm_ctl[2] |= PM8XXX_PWM_RAMP_UP;
		if (lut->flags & PM_PWM_LUT_LOOP)
			pwm->pwm_ctl[2] |= PM8XXX_PWM_LOOP_EN;

		/* Pause time */
		if (lut->flags & PM_PWM_LUT_PAUSE_HI_EN) {
			/* Linear search for pause time */
			pause_cnt = (lut->lut_pause_hi + duty_msec[i] / 2)
					/ duty_msec[i];
			for (j = 0; j < PM8XXX_PWM_PAUSE_COUNT_MAX; j++) {
				if (pause_count[j] >= pause_cnt)
					break;
			}
			pwm->pwm_ctl[5] |= (j <<
					   PM8XXX_PWM_PAUSE_COUNT_HI_SHIFT) &
						PM8XXX_PWM_PAUSE_COUNT_HI_MASK;
			pwm->pwm_ctl[5] |= PM8XXX_PWM_PAUSE_ENABLE_HIGH;
		}

		if (lut->flags & PM_PWM_LUT_PAUSE_LO_EN) {
			/* Linear search for pause time */
			pause_cnt = (lut->lut_pause_lo + duty_msec[i] / 2)
					/ duty_msec[i];
			for (j = 0; j < PM8XXX_PWM_PAUSE_COUNT_MAX; j++) {
				if (pause_count[j] >= pause_cnt)
					break;
			}
			pwm->pwm_ctl[6] = (j <<
					   PM8XXX_PWM_PAUSE_COUNT_LO_SHIFT) &
						PM8XXX_PWM_PAUSE_COUNT_LO_MASK;
			pwm->pwm_ctl[6] |= PM8XXX_PWM_PAUSE_ENABLE_LOW;
		} else {
			pwm->pwm_ctl[6] = 0;
		}

		len = 7;
	}

	pm8xxx_pwm_bank_sel(pwm);

	for (i = 0; i < len; i++) {
		rc = pm8xxx_writeb(pwm->chip->dev->parent,
				   SSBI_REG_ADDR_LPG_CTL(i),
				   pwm->pwm_ctl[i]);
		if (rc) {
			pr_err("pm8xxx_write(): rc=%d (PWM Ctl[%d])\n", rc, i);
			break;
		}
	}

	if (ramp_enabled) {
		pwm->pwm_ctl[0] |= ramp_enabled;
		pm8xxx_writeb(pwm->chip->dev->parent,
			      SSBI_REG_ADDR_LPG_CTL(0),
			      pwm->pwm_ctl[0]);
	}

	return rc;
}

/* APIs */
/**
 * pwm_request - request a PWM device
 * @pwm_id: PWM id or channel
 * @label: the label to identify the user
 */
struct pwm_device *pwm_request(int pwm_id, const char *label)
{
	struct pwm_device	*pwm;

	if (pwm_id > PM8XXX_PWM_CHANNELS || pwm_id < 0) {
		pr_err("Invalid pwm_id: %d with %s\n",
		       pwm_id, label ? label : ".");
		return ERR_PTR(-EINVAL);
	}
	if (pwm_chip == NULL) {
		pr_err("No pwm_chip\n");
		return ERR_PTR(-ENODEV);
	}

	mutex_lock(&pwm_chip->pwm_mutex);
	pwm = &pwm_chip->pwm_dev[pwm_id];
	if (!pwm->in_use) {
		pwm->in_use = 1;
		pwm->label = label;
	} else {
		pwm = ERR_PTR(-EBUSY);
	}
	mutex_unlock(&pwm_chip->pwm_mutex);

	return pwm;
}
EXPORT_SYMBOL_GPL(pwm_request);

/**
 * pwm_free - free a PWM device
 * @pwm: the PWM device
 */
void pwm_free(struct pwm_device *pwm)
{
	if (pwm == NULL || IS_ERR(pwm) || pwm->chip == NULL) {
		pr_err("Invalid pwm handle\n");
		return;
	}

	mutex_lock(&pwm->chip->pwm_mutex);
	if (pwm->in_use) {
		pm8xxx_pwm_bank_sel(pwm);
		pm8xxx_pwm_start(pwm, 0, 0);

		pwm->in_use = 0;
		pwm->label = NULL;
	}
	pm8xxx_pwm_bank_enable(pwm, 0);
	mutex_unlock(&pwm->chip->pwm_mutex);
}
EXPORT_SYMBOL_GPL(pwm_free);

/**
 * pwm_config - change a PWM device configuration
 * @pwm: the PWM device
 * @period_us: period in microseconds
 * @duty_us: duty cycle in microseconds
 */
int pwm_config(struct pwm_device *pwm, int duty_us, int period_us)
{
	struct pm8xxx_pwm_lut	lut;
	struct pm8xxx_pwm_period *period;
	unsigned int max_pwm_value, tmp;
	int	rc;

	if (pwm == NULL || IS_ERR(pwm) ||
		duty_us > period_us ||
		(unsigned)period_us > PM8XXX_PWM_PERIOD_MAX ||
		(unsigned)period_us < PM8XXX_PWM_PERIOD_MIN) {
		pr_err("Invalid pwm handle or parameters\n");
		return -EINVAL;
	}
	if (pwm->chip == NULL) {
		pr_err("No pwm_chip\n");
		return -ENODEV;
	}

	period = &pwm->period;

	mutex_lock(&pwm->chip->pwm_mutex);

	if (!pwm->in_use) {
		pr_err("pwm_id: %d: stale handle?\n", pwm->pwm_id);
		rc = -EINVAL;
		goto out_unlock;
	}

	pm8xxx_pwm_calc_period(period_us, period);

	/* Figure out pwm_value with overflow handling */
	if ((unsigned)period_us > (1 << period->pwm_size)) {
		tmp = period_us;
		tmp >>= period->pwm_size;
		pwm->pwm_value = (unsigned)duty_us / tmp;
	} else {
		tmp = duty_us;
		tmp <<= period->pwm_size;
		pwm->pwm_value = tmp / (unsigned)period_us;
	}
	max_pwm_value = (1 << period->pwm_size) - 1;
	if (pwm->pwm_value > max_pwm_value)
		pwm->pwm_value = max_pwm_value;

	pwm->bypass_lut = 1;

	rc = pm8xxx_pwm_configure(pwm, &lut);

out_unlock:
	mutex_unlock(&pwm->chip->pwm_mutex);
	return rc;
}
EXPORT_SYMBOL_GPL(pwm_config);

/**
 * pwm_enable - start a PWM output toggling
 * @pwm: the PWM device
 */
int pwm_enable(struct pwm_device *pwm)
{
	int	rc;

	if (pwm == NULL || IS_ERR(pwm)) {
		pr_err("Invalid pwm handle\n");
		return -EINVAL;
	}
	if (pwm->chip == NULL) {
		pr_err("No pwm_chip\n");
		return -ENODEV;
	}

	mutex_lock(&pwm->chip->pwm_mutex);
	if (!pwm->in_use) {
		pr_err("pwm_id: %d: stale handle?\n", pwm->pwm_id);
		rc = -EINVAL;
	} else {
		rc = pm8xxx_pwm_bank_enable(pwm, 1);

		pm8xxx_pwm_bank_sel(pwm);
		pm8xxx_pwm_start(pwm, 1, 0);
	}
	mutex_unlock(&pwm->chip->pwm_mutex);
	return rc;
}
EXPORT_SYMBOL_GPL(pwm_enable);

/**
 * pwm_disable - stop a PWM output toggling
 * @pwm: the PWM device
 */
void pwm_disable(struct pwm_device *pwm)
{
	if (pwm == NULL || IS_ERR(pwm) || pwm->chip == NULL) {
		pr_err("Invalid pwm handle or no pwm_chip\n");
		return;
	}

	mutex_lock(&pwm->chip->pwm_mutex);
	if (pwm->in_use) {
		pm8xxx_pwm_bank_sel(pwm);
		pm8xxx_pwm_start(pwm, 0, 0);

		pm8xxx_pwm_bank_enable(pwm, 0);
	}
	mutex_unlock(&pwm->chip->pwm_mutex);
}
EXPORT_SYMBOL_GPL(pwm_disable);

/**
 * pm8xxx_pwm_lut_config - change a PWM device configuration to use LUT
 * @pwm: the PWM device
 * @period_us: period in microseconds
 * @duty_pct: arrary of duty cycles in percent, like 20, 50.
 * @duty_time_ms: time for each duty cycle in milliseconds
 * @start_idx: start index in lookup table from 0 to MAX-1
 * @idx_len: number of index
 * @pause_lo: pause time in milliseconds at low index
 * @pause_hi: pause time in milliseconds at high index
 * @flags: control flags
 */
int pm8xxx_pwm_lut_config(struct pwm_device *pwm, int period_us,
			  int duty_pct[], int duty_time_ms, int start_idx,
			  int idx_len, int pause_lo, int pause_hi, int flags)
{
	struct pm8xxx_pwm_lut	lut;
	struct pm8xxx_pwm_period *period;
	unsigned int pwm_value, max_pwm_value;
	u8	cfg0, cfg1;
	int	i, len;
	int	rc;

	if (pwm == NULL || IS_ERR(pwm) || !idx_len) {
		pr_err("Invalid pwm handle or idx_len=0\n");
		return -EINVAL;
	}
	if (duty_pct == NULL && !(flags & PM_PWM_LUT_NO_TABLE)) {
		pr_err("Invalid duty_pct with flag\n");
		return -EINVAL;
	}
	if (pwm->chip == NULL) {
		pr_err("No pwm_chip\n");
		return -ENODEV;
	}
	if (idx_len >= PM_PWM_LUT_SIZE && start_idx) {
		pr_err("Wrong LUT size or index\n");
		return -EINVAL;
	}
	if ((start_idx + idx_len) > PM_PWM_LUT_SIZE) {
		pr_err("Exceed LUT limit\n");
		return -EINVAL;
	}
	if ((unsigned)period_us > PM8XXX_PWM_PERIOD_MAX ||
		(unsigned)period_us < PM8XXX_PWM_PERIOD_MIN) {
		pr_err("Period out of range\n");
		return -EINVAL;
	}

	period = &pwm->period;
	mutex_lock(&pwm->chip->pwm_mutex);

	if (!pwm->in_use) {
		pr_err("pwm_id: %d: stale handle?\n", pwm->pwm_id);
		rc = -EINVAL;
		goto out_unlock;
	}

	pm8xxx_pwm_calc_period(period_us, period);

	len = (idx_len > PM_PWM_LUT_SIZE) ? PM_PWM_LUT_SIZE : idx_len;

	if (flags & PM_PWM_LUT_NO_TABLE)
		goto after_table_write;

	max_pwm_value = (1 << period->pwm_size) - 1;
	for (i = 0; i < len; i++) {
		pwm_value = (duty_pct[i] << period->pwm_size) / 100;
		/* Avoid overflow */
		if (pwm_value > max_pwm_value)
			pwm_value = max_pwm_value;
		cfg0 = pwm_value & 0xff;
		cfg1 = (pwm_value >> 1) & 0x80;
		cfg1 |= start_idx + i;

		pm8xxx_writeb(pwm->chip->dev->parent,
			      SSBI_REG_ADDR_LPG_LUT_CFG0, cfg0);
		pm8xxx_writeb(pwm->chip->dev->parent,
			      SSBI_REG_ADDR_LPG_LUT_CFG1, cfg1);
	}

after_table_write:
	lut.lut_duty_ms = duty_time_ms;
	lut.lut_lo_index = start_idx;
	lut.lut_hi_index = start_idx + len - 1;
	lut.lut_pause_lo = pause_lo;
	lut.lut_pause_hi = pause_hi;
	lut.flags = flags;
	pwm->bypass_lut = 0;

	rc = pm8xxx_pwm_configure(pwm, &lut);

out_unlock:
	mutex_unlock(&pwm->chip->pwm_mutex);
	return rc;
}
EXPORT_SYMBOL_GPL(pm8xxx_pwm_lut_config);

/**
 * pm8xxx_pwm_lut_enable - control a PWM device to start/stop LUT ramp
 * @pwm: the PWM device
 * @start: to start (1), or stop (0)
 */
int pm8xxx_pwm_lut_enable(struct pwm_device *pwm, int start)
{
	if (pwm == NULL || IS_ERR(pwm)) {
		pr_err("Invalid pwm handle\n");
		return -EINVAL;
	}
	if (pwm->chip == NULL) {
		pr_err("No pwm_chip\n");
		return -ENODEV;
	}

	mutex_lock(&pwm->chip->pwm_mutex);
	if (start) {
		pm8xxx_pwm_bank_enable(pwm, 1);

		pm8xxx_pwm_bank_sel(pwm);
		pm8xxx_pwm_start(pwm, 1, 1);
	} else {
		pm8xxx_pwm_bank_sel(pwm);
		pm8xxx_pwm_start(pwm, 0, 0);

		pm8xxx_pwm_bank_enable(pwm, 0);
	}
	mutex_unlock(&pwm->chip->pwm_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(pm8xxx_pwm_lut_enable);

#if defined(CONFIG_DEBUG_FS)

struct pm8xxx_pwm_dbg_device;

struct pm8xxx_pwm_user {
	int				pwm_id;
	struct pwm_device		*pwm;
	int				period;
	int				duty_cycle;
	int				enable;
	struct pm8xxx_pwm_dbg_device	*dbgdev;
};

struct pm8xxx_pwm_dbg_device {
	struct mutex		dbg_mutex;
	struct device		*dev;
	struct dentry		*dent;

	struct pm8xxx_pwm_user	user[PM8XXX_PWM_CHANNELS];
};

static struct pm8xxx_pwm_dbg_device *pmic_dbg_device;

static int dbg_pwm_check_period(int period)
{
	if (period < PM8XXX_PWM_PERIOD_MIN || period > PM8XXX_PWM_PERIOD_MAX) {
		pr_err("period is invalid: %d\n", period);
		return -EINVAL;
	}
	return 0;
}

static int dbg_pwm_check_duty_cycle(int duty_cycle, const char *func_name)
{
	if (duty_cycle <= 0 || duty_cycle > 100) {
		pr_err("%s: duty_cycle is invalid: %d\n",
		      func_name, duty_cycle);
		return -EINVAL;
	}
	return 0;
}

static void dbg_pwm_check_handle(struct pm8xxx_pwm_user *puser)
{
	struct pwm_device *tmp;

	if (puser->pwm == NULL) {
		tmp = pwm_request(puser->pwm_id, "pwm-dbg");
		if (PTR_ERR(puser->pwm)) {
			pr_err("pwm_request: err=%ld\n", PTR_ERR(puser->pwm));
			puser->pwm = NULL;
		} else {
			pr_debug("[id=%d] pwm_request ok\n", puser->pwm_id);
			puser->pwm = tmp;
		}
	}
}

static int dbg_pwm_enable_set(void *data, u64 val)
{
	struct pm8xxx_pwm_user	  *puser = data;
	struct pm8xxx_pwm_dbg_device    *dbgdev = puser->dbgdev;
	int     rc;

	mutex_lock(&dbgdev->dbg_mutex);
	rc = dbg_pwm_check_duty_cycle(puser->duty_cycle, __func__);
	if (!rc) {
		puser->enable = val;
		dbg_pwm_check_handle(puser);
		if (puser->pwm) {
			if (puser->enable)
				pwm_enable(puser->pwm);
			else
				pwm_disable(puser->pwm);
		}
	}
	mutex_unlock(&dbgdev->dbg_mutex);
	return 0;
}

static int dbg_pwm_enable_get(void *data, u64 *val)
{
	struct pm8xxx_pwm_user	  *puser = data;
	struct pm8xxx_pwm_dbg_device    *dbgdev = puser->dbgdev;

	mutex_lock(&dbgdev->dbg_mutex);
	*val = puser->enable;
	mutex_unlock(&dbgdev->dbg_mutex);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(dbg_pwm_enable_fops,
			dbg_pwm_enable_get, dbg_pwm_enable_set,
			"%lld\n");

static int dbg_pwm_duty_cycle_set(void *data, u64 val)
{
	struct pm8xxx_pwm_user	  *puser = data;
	struct pm8xxx_pwm_dbg_device    *dbgdev = puser->dbgdev;
	int     rc;

	mutex_lock(&dbgdev->dbg_mutex);
	rc = dbg_pwm_check_duty_cycle(val, __func__);
	if (!rc) {
		puser->duty_cycle = val;
		dbg_pwm_check_handle(puser);
		if (puser->pwm) {
			int     duty_us;

			duty_us = puser->duty_cycle * puser->period / 100;
			pwm_config(puser->pwm, duty_us, puser->period);
		}
	}
	mutex_unlock(&dbgdev->dbg_mutex);
	return 0;
}

static int dbg_pwm_duty_cycle_get(void *data, u64 *val)
{
	struct pm8xxx_pwm_user	  *puser = data;
	struct pm8xxx_pwm_dbg_device    *dbgdev = puser->dbgdev;

	mutex_lock(&dbgdev->dbg_mutex);
	*val = puser->duty_cycle;
	mutex_unlock(&dbgdev->dbg_mutex);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(dbg_pwm_duty_cycle_fops,
			dbg_pwm_duty_cycle_get, dbg_pwm_duty_cycle_set,
			"%lld\n");

static int dbg_pwm_period_set(void *data, u64 val)
{
	struct pm8xxx_pwm_user	  *puser = data;
	struct pm8xxx_pwm_dbg_device    *dbgdev = puser->dbgdev;
	int     rc;

	mutex_lock(&dbgdev->dbg_mutex);
	rc = dbg_pwm_check_period(val);
	if (!rc)
		puser->period = val;
	mutex_unlock(&dbgdev->dbg_mutex);
	return 0;
}

static int dbg_pwm_period_get(void *data, u64 *val)
{
	struct pm8xxx_pwm_user	  *puser = data;
	struct pm8xxx_pwm_dbg_device    *dbgdev = puser->dbgdev;

	mutex_lock(&dbgdev->dbg_mutex);
	*val = puser->period;
	mutex_unlock(&dbgdev->dbg_mutex);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(dbg_pwm_period_fops,
			dbg_pwm_period_get, dbg_pwm_period_set, "%lld\n");

static int __devinit pm8xxx_pwm_dbg_probe(struct device *dev)
{
	struct pm8xxx_pwm_dbg_device    *dbgdev;
	struct dentry		   *dent;
	struct dentry		   *temp;
	struct pm8xxx_pwm_user	  *puser;
	int			     i;

	if (dev == NULL) {
		pr_err("no parent data passed in.\n");
		return -EINVAL;
	}

	dbgdev = kzalloc(sizeof *dbgdev, GFP_KERNEL);
	if (dbgdev == NULL) {
		pr_err("kzalloc() failed.\n");
		return -ENOMEM;
	}

	mutex_init(&dbgdev->dbg_mutex);

	dbgdev->dev = dev;

	dent = debugfs_create_dir("pm8xxx-pwm-dbg", NULL);
	if (dent == NULL || IS_ERR(dent)) {
		pr_err("ERR debugfs_create_dir: dent=%p\n", dent);
		return -ENOMEM;
	}

	dbgdev->dent = dent;

	for (i = 0; i < PM8XXX_PWM_CHANNELS; i++) {
		char pwm_ch[] = "0";

		pwm_ch[0] = '0' + i;
		dent = debugfs_create_dir(pwm_ch, dbgdev->dent);
		if (dent == NULL || IS_ERR(dent)) {
			pr_err("ERR: pwm=%d: dir: dent=%p\n", i, dent);
			goto debug_error;
		}

		puser = &dbgdev->user[i];
		puser->dbgdev = dbgdev;
		puser->pwm_id = i;
		temp = debugfs_create_file("period", S_IRUGO | S_IWUSR,
				dent, puser, &dbg_pwm_period_fops);
		if (temp == NULL || IS_ERR(temp)) {
			pr_err("ERR: pwm=%d: period: dent=%p\n", i, dent);
			goto debug_error;
		}

		temp = debugfs_create_file("duty-cycle", S_IRUGO | S_IWUSR,
				dent, puser, &dbg_pwm_duty_cycle_fops);
		if (temp == NULL || IS_ERR(temp)) {
			pr_err("ERR: pwm=%d: duty-cycle: dent=%p\n", i, dent);
			goto debug_error;
		}

		temp = debugfs_create_file("enable", S_IRUGO | S_IWUSR,
				dent, puser, &dbg_pwm_enable_fops);
		if (temp == NULL || IS_ERR(temp)) {
			pr_err("ERR: pwm=%d: enable: dent=%p\n", i, dent);
			goto debug_error;
		}
	}

	pmic_dbg_device = dbgdev;

	return 0;

debug_error:
	debugfs_remove_recursive(dbgdev->dent);
	return -ENOMEM;
}

static int __devexit pm8xxx_pwm_dbg_remove(void)
{
	if (pmic_dbg_device) {
		debugfs_remove_recursive(pmic_dbg_device->dent);
		kfree(pmic_dbg_device);
	}
	return 0;
}

#else

static int __devinit pm8xxx_pwm_dbg_probe(struct device *dev)
{
	return 0;
}

static int __devexit pm8xxx_pwm_dbg_remove(void)
{
	return 0;
}

#endif

static int __devinit pm8xxx_pwm_probe(struct platform_device *pdev)
{
	struct pm8xxx_pwm_chip	*chip;
	int	i;

	chip = kzalloc(sizeof *chip, GFP_KERNEL);
	if (chip == NULL) {
		pr_err("kzalloc() failed.\n");
		return -ENOMEM;
	}

	for (i = 0; i < PM8XXX_PWM_CHANNELS; i++) {
		chip->pwm_dev[i].pwm_id = i;
		chip->pwm_dev[i].chip = chip;
	}

	mutex_init(&chip->pwm_mutex);

	chip->dev = &pdev->dev;
	pwm_chip = chip;
	platform_set_drvdata(pdev, chip);

	if (pm8xxx_pwm_dbg_probe(&pdev->dev) < 0)
		pr_err("could not set up debugfs\n");

	pr_notice("OK\n");
	return 0;
}

static int __devexit pm8xxx_pwm_remove(struct platform_device *pdev)
{
	struct pm8xxx_pwm_chip	*chip = dev_get_drvdata(pdev->dev.parent);

	pm8xxx_pwm_dbg_remove();
	mutex_destroy(&chip->pwm_mutex);
	platform_set_drvdata(pdev, NULL);
	kfree(chip);
	return 0;
}

static struct platform_driver pm8xxx_pwm_driver = {
	.probe		= pm8xxx_pwm_probe,
	.remove		= __devexit_p(pm8xxx_pwm_remove),
	.driver		= {
		.name = PM8XXX_PWM_DEV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init pm8xxx_pwm_init(void)
{
	return platform_driver_register(&pm8xxx_pwm_driver);
}

static void __exit pm8xxx_pwm_exit(void)
{
	platform_driver_unregister(&pm8xxx_pwm_driver);
}

subsys_initcall(pm8xxx_pwm_init);
module_exit(pm8xxx_pwm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PM8XXX PWM driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8XXX_PWM_DEV_NAME);
