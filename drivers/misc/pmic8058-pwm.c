/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * Qualcomm PMIC8058 PWM driver
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/mfd/pmic8058.h>
#include <linux/pmic8058-pwm.h>
#include <linux/slab.h>

#define	PM8058_LPG_BANKS		8
#define	PM8058_PWM_CHANNELS		PM8058_LPG_BANKS	/* MAX=8 */

#define	PM8058_LPG_CTL_REGS		7

/* PMIC8058 LPG/PWM */
#define	SSBI_REG_ADDR_LPG_CTL_BASE	0x13C
#define	SSBI_REG_ADDR_LPG_CTL(n)	(SSBI_REG_ADDR_LPG_CTL_BASE + (n))
#define	SSBI_REG_ADDR_LPG_BANK_SEL	0x143
#define	SSBI_REG_ADDR_LPG_BANK_EN	0x144
#define	SSBI_REG_ADDR_LPG_LUT_CFG0	0x145
#define	SSBI_REG_ADDR_LPG_LUT_CFG1	0x146
#define	SSBI_REG_ADDR_LPG_TEST		0x147

/* Control 0 */
#define	PM8058_PWM_1KHZ_COUNT_MASK	0xF0
#define	PM8058_PWM_1KHZ_COUNT_SHIFT	4

#define	PM8058_PWM_1KHZ_COUNT_MAX	15

#define	PM8058_PWM_OUTPUT_EN		0x08
#define	PM8058_PWM_PWM_EN		0x04
#define	PM8058_PWM_RAMP_GEN_EN		0x02
#define	PM8058_PWM_RAMP_START		0x01

#define	PM8058_PWM_PWM_START		(PM8058_PWM_OUTPUT_EN \
					| PM8058_PWM_PWM_EN)
#define	PM8058_PWM_RAMP_GEN_START	(PM8058_PWM_RAMP_GEN_EN \
					| PM8058_PWM_RAMP_START)

/* Control 1 */
#define	PM8058_PWM_REVERSE_EN		0x80
#define	PM8058_PWM_BYPASS_LUT		0x40
#define	PM8058_PWM_HIGH_INDEX_MASK	0x3F

/* Control 2 */
#define	PM8058_PWM_LOOP_EN		0x80
#define	PM8058_PWM_RAMP_UP		0x40
#define	PM8058_PWM_LOW_INDEX_MASK	0x3F

/* Control 3 */
#define	PM8058_PWM_VALUE_BIT7_0		0xFF
#define	PM8058_PWM_VALUE_BIT5_0		0x3F

/* Control 4 */
#define	PM8058_PWM_VALUE_BIT8		0x80

#define	PM8058_PWM_CLK_SEL_MASK		0x60
#define	PM8058_PWM_CLK_SEL_SHIFT	5

#define	PM8058_PWM_CLK_SEL_NO		0
#define	PM8058_PWM_CLK_SEL_1KHZ		1
#define	PM8058_PWM_CLK_SEL_32KHZ	2
#define	PM8058_PWM_CLK_SEL_19P2MHZ	3

#define	PM8058_PWM_PREDIVIDE_MASK	0x18
#define	PM8058_PWM_PREDIVIDE_SHIFT	3

#define	PM8058_PWM_PREDIVIDE_2		0
#define	PM8058_PWM_PREDIVIDE_3		1
#define	PM8058_PWM_PREDIVIDE_5		2
#define	PM8058_PWM_PREDIVIDE_6		3

#define	PM8058_PWM_M_MASK	0x07
#define	PM8058_PWM_M_MIN	0
#define	PM8058_PWM_M_MAX	7

/* Control 5 */
#define	PM8058_PWM_PAUSE_COUNT_HI_MASK		0xFC
#define	PM8058_PWM_PAUSE_COUNT_HI_SHIFT		2

#define	PM8058_PWM_PAUSE_ENABLE_HIGH		0x02
#define	PM8058_PWM_SIZE_9_BIT			0x01

/* Control 6 */
#define	PM8058_PWM_PAUSE_COUNT_LO_MASK		0xFC
#define	PM8058_PWM_PAUSE_COUNT_LO_SHIFT		2

#define	PM8058_PWM_PAUSE_ENABLE_LOW		0x02
#define	PM8058_PWM_RESERVED			0x01

#define	PM8058_PWM_PAUSE_COUNT_MAX		56 /* < 2^6 = 64*/

/* LUT_CFG1 */
#define	PM8058_PWM_LUT_READ			0x40

/* TEST */
#define	PM8058_PWM_DTEST_MASK		0x38
#define	PM8058_PWM_DTEST_SHIFT		3

#define	PM8058_PWM_DTEST_BANK_MASK	0x07

/* PWM frequency support
 *
 * PWM Frequency = Clock Frequency / (N * T)
 * 	or
 * PWM Period = Clock Period * (N * T)
 * 	where
 * N = 2^9 or 2^6 for 9-bit or 6-bit PWM size
 * T = Pre-divide * 2^m, m = 0..7 (exponent)
 *
 * We use this formula to figure out m for the best pre-divide and clock:
 * (PWM Period / N) / 2^m = (Pre-divide * Clock Period)
*/
#define	NUM_CLOCKS	3

#define	NSEC_1000HZ	(NSEC_PER_SEC / 1000)
#define	NSEC_32768HZ	(NSEC_PER_SEC / 32768)
#define	NSEC_19P2MHZ	(NSEC_PER_SEC / 19200000)

#define	CLK_PERIOD_MIN	NSEC_19P2MHZ
#define	CLK_PERIOD_MAX	NSEC_1000HZ

#define	NUM_PRE_DIVIDE	3	/* No default support for pre-divide = 6 */

#define	PRE_DIVIDE_0		2
#define	PRE_DIVIDE_1		3
#define	PRE_DIVIDE_2		5

#define	PRE_DIVIDE_MIN		PRE_DIVIDE_0
#define	PRE_DIVIDE_MAX		PRE_DIVIDE_2

static char *clks[NUM_CLOCKS] = {
	"1K", "32768", "19.2M"
};

static unsigned pre_div[NUM_PRE_DIVIDE] = {
	PRE_DIVIDE_0, PRE_DIVIDE_1, PRE_DIVIDE_2
};

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

#define	MIN_MPT	((PRE_DIVIDE_MIN * CLK_PERIOD_MIN) << PM8058_PWM_M_MIN)
#define	MAX_MPT	((PRE_DIVIDE_MAX * CLK_PERIOD_MAX) << PM8058_PWM_M_MAX)

/* Private data */
struct pm8058_pwm_chip;

struct pwm_device {
	int			pwm_id;		/* = bank/channel id */
	int			in_use;
	const char		*label;
	int			pwm_period;
	int			pwm_duty;
	u8			pwm_ctl[PM8058_LPG_CTL_REGS];
	int			irq;
	struct pm8058_pwm_chip	*chip;
};

struct pm8058_pwm_chip {
	struct pwm_device	pwm_dev[PM8058_PWM_CHANNELS];
	u8			bank_mask;
	struct mutex		pwm_mutex;
	struct pm8058_chip	*pm_chip;
	struct pm8058_pwm_pdata	*pdata;
};

static struct pm8058_pwm_chip	*pwm_chip;

struct pw8058_pwm_config {
	int	pwm_size;	/* round up to 6 or 9 for 6/9-bit PWM SIZE */
	int	clk;
	int	pre_div;
	int	pre_div_exp;
	int	pwm_value;
	int	bypass_lut;

	/* LUT parameters when bypass_lut is 0 */
	int	lut_duty_ms;
	int	lut_lo_index;
	int	lut_hi_index;
	int	lut_pause_hi;
	int	lut_pause_lo;
	int	flags;
};

static u16 duty_msec[PM8058_PWM_1KHZ_COUNT_MAX + 1] = {
	0, 1, 2, 3, 4, 6, 8, 16, 18, 24, 32, 36, 64, 128, 256, 512
};

static u16 pause_count[PM8058_PWM_PAUSE_COUNT_MAX + 1] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	23, 28, 31, 42, 47, 56, 63, 83, 94, 111, 125, 167, 188, 222, 250, 333,
	375, 500, 667, 750, 800, 900, 1000, 1100,
	1200, 1300, 1400, 1500, 1600, 1800, 2000, 2500,
	3000, 3500, 4000, 4500, 5000, 5500, 6000, 6500,
	7000
};

/* Internal functions */
static int pm8058_pwm_bank_enable(struct pwm_device *pwm, int enable)
{
	int	rc;
	u8	reg;
	struct pm8058_pwm_chip	*chip;

	chip = pwm->chip;

	if (enable)
		reg = chip->bank_mask | (1 << pwm->pwm_id);
	else
		reg = chip->bank_mask & ~(1 << pwm->pwm_id);

	rc = pm8058_write(chip->pm_chip, SSBI_REG_ADDR_LPG_BANK_EN, &reg, 1);
	if (rc) {
		pr_err("%s: pm8058_write(): rc=%d (Enable LPG Bank)\n",
		       __func__, rc);
		goto bail_out;
	}
	chip->bank_mask = reg;

bail_out:
	return rc;
}

static int pm8058_pwm_bank_sel(struct pwm_device *pwm)
{
	int	rc;
	u8	reg;

	reg = pwm->pwm_id;
	rc = pm8058_write(pwm->chip->pm_chip, SSBI_REG_ADDR_LPG_BANK_SEL,
			     &reg, 1);
	if (rc)
		pr_err("%s: pm8058_write(): rc=%d (Select PWM Bank)\n",
		       __func__, rc);
	return rc;
}

static int pm8058_pwm_start(struct pwm_device *pwm, int start, int ramp_start)
{
	int	rc;
	u8	reg;

	if (start) {
		reg = pwm->pwm_ctl[0] | PM8058_PWM_PWM_START;
		if (ramp_start)
			reg |= PM8058_PWM_RAMP_GEN_START;
		else
			reg &= ~PM8058_PWM_RAMP_GEN_START;
	} else {
		reg = pwm->pwm_ctl[0] & ~PM8058_PWM_PWM_START;
		reg &= ~PM8058_PWM_RAMP_GEN_START;
	}

	rc = pm8058_write(pwm->chip->pm_chip, SSBI_REG_ADDR_LPG_CTL(0),
			  &reg, 1);
	if (rc)
		pr_err("%s: pm8058_write(): rc=%d (Enable PWM Ctl 0)\n",
		       __func__, rc);
	else
		pwm->pwm_ctl[0] = reg;
	return rc;
}

static void pm8058_pwm_calc_period(unsigned int period_us,
					   struct pw8058_pwm_config *pwm_conf)
{
	int	n, m, clk, div;
	int	best_m, best_div, best_clk;
	int	last_err, cur_err, better_err, better_m;
	unsigned int	tmp_p, last_p, min_err, period_n;

	/* PWM Period / N : handle underflow or overflow */
	if (period_us < (PM_PWM_PERIOD_MAX / NSEC_PER_USEC))
		period_n = (period_us * NSEC_PER_USEC) >> 6;
	else
		period_n = (period_us >> 6) * NSEC_PER_USEC;
	if (period_n >= MAX_MPT) {
		n = 9;
		period_n >>= 3;
	} else
		n = 6;

	min_err = MAX_MPT;
	best_m = 0;
	best_clk = 0;
	best_div = 0;
	for (clk = 0; clk < NUM_CLOCKS; clk++) {
		for (div = 0; div < NUM_PRE_DIVIDE; div++) {
			tmp_p = period_n;
			last_p = tmp_p;
			for (m = 0; m <= PM8058_PWM_M_MAX; m++) {
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

	pwm_conf->pwm_size = n;
	pwm_conf->clk = best_clk;
	pwm_conf->pre_div = best_div;
	pwm_conf->pre_div_exp = best_m;

	pr_debug("%s: period=%u: n=%d, m=%d, clk[%d]=%s, div[%d]=%d\n",
		 __func__, (unsigned)period_us, n, best_m,
		 best_clk, clks[best_clk], best_div, pre_div[best_div]);
}

static int pm8058_pwm_configure(struct pwm_device *pwm,
			 struct pw8058_pwm_config *pwm_conf)
{
	int	i, rc, len;
	u8	reg, ramp_enabled = 0;

	reg = (pwm_conf->pwm_size > 6) ? PM8058_PWM_SIZE_9_BIT : 0;
	pwm->pwm_ctl[5] = reg;

	reg = ((pwm_conf->clk + 1) << PM8058_PWM_CLK_SEL_SHIFT)
		& PM8058_PWM_CLK_SEL_MASK;
	reg |= (pwm_conf->pre_div << PM8058_PWM_PREDIVIDE_SHIFT)
		& PM8058_PWM_PREDIVIDE_MASK;
	reg |= pwm_conf->pre_div_exp & PM8058_PWM_M_MASK;
	pwm->pwm_ctl[4] = reg;

	if (pwm_conf->bypass_lut) {
		pwm->pwm_ctl[0] &= PM8058_PWM_PWM_START; /* keep enabled */
		pwm->pwm_ctl[1] = PM8058_PWM_BYPASS_LUT;
		pwm->pwm_ctl[2] = 0;

		if (pwm_conf->pwm_size > 6) {
			pwm->pwm_ctl[3] = pwm_conf->pwm_value
						& PM8058_PWM_VALUE_BIT7_0;
			pwm->pwm_ctl[4] |= (pwm_conf->pwm_value >> 1)
						& PM8058_PWM_VALUE_BIT8;
		} else {
			pwm->pwm_ctl[3] = pwm_conf->pwm_value
						& PM8058_PWM_VALUE_BIT5_0;
		}

		len = 6;
	} else {
		int	pause_cnt, j;

		/* Linear search for duty time */
		for (i = 0; i < PM8058_PWM_1KHZ_COUNT_MAX; i++) {
			if (duty_msec[i] >= pwm_conf->lut_duty_ms)
				break;
		}

		ramp_enabled = pwm->pwm_ctl[0] & PM8058_PWM_RAMP_GEN_START;
		pwm->pwm_ctl[0] &= PM8058_PWM_PWM_START; /* keep enabled */
		pwm->pwm_ctl[0] |= (i << PM8058_PWM_1KHZ_COUNT_SHIFT) &
					PM8058_PWM_1KHZ_COUNT_MASK;
		pwm->pwm_ctl[1] = pwm_conf->lut_hi_index &
					PM8058_PWM_HIGH_INDEX_MASK;
		pwm->pwm_ctl[2] = pwm_conf->lut_lo_index &
					PM8058_PWM_LOW_INDEX_MASK;

		if (pwm_conf->flags & PM_PWM_LUT_REVERSE)
			pwm->pwm_ctl[1] |= PM8058_PWM_REVERSE_EN;
		if (pwm_conf->flags & PM_PWM_LUT_RAMP_UP)
			pwm->pwm_ctl[2] |= PM8058_PWM_RAMP_UP;
		if (pwm_conf->flags & PM_PWM_LUT_LOOP)
			pwm->pwm_ctl[2] |= PM8058_PWM_LOOP_EN;

		/* Pause time */
		if (pwm_conf->flags & PM_PWM_LUT_PAUSE_HI_EN) {
			/* Linear search for pause time */
			pause_cnt = (pwm_conf->lut_pause_hi + duty_msec[i] / 2)
					/ duty_msec[i];
			for (j = 0; j < PM8058_PWM_PAUSE_COUNT_MAX; j++) {
				if (pause_count[j] >= pause_cnt)
					break;
			}
			pwm->pwm_ctl[5] = (j <<
					   PM8058_PWM_PAUSE_COUNT_HI_SHIFT) &
						PM8058_PWM_PAUSE_COUNT_HI_MASK;
			pwm->pwm_ctl[5] |= PM8058_PWM_PAUSE_ENABLE_HIGH;
		} else
			pwm->pwm_ctl[5] = 0;

		if (pwm_conf->flags & PM_PWM_LUT_PAUSE_LO_EN) {
			/* Linear search for pause time */
			pause_cnt = (pwm_conf->lut_pause_lo + duty_msec[i] / 2)
					/ duty_msec[i];
			for (j = 0; j < PM8058_PWM_PAUSE_COUNT_MAX; j++) {
				if (pause_count[j] >= pause_cnt)
					break;
			}
			pwm->pwm_ctl[6] = (j <<
					   PM8058_PWM_PAUSE_COUNT_LO_SHIFT) &
						PM8058_PWM_PAUSE_COUNT_LO_MASK;
			pwm->pwm_ctl[6] |= PM8058_PWM_PAUSE_ENABLE_LOW;
		} else
			pwm->pwm_ctl[6] = 0;

		len = 7;
	}

	pm8058_pwm_bank_sel(pwm);

	for (i = 0; i < len; i++) {
		rc = pm8058_write(pwm->chip->pm_chip,
				  SSBI_REG_ADDR_LPG_CTL(i),
				  &pwm->pwm_ctl[i], 1);
		if (rc) {
			pr_err("%s: pm8058_write(): rc=%d (PWM Ctl[%d])\n",
			       __func__, rc, i);
			break;
		}
	}

	if (ramp_enabled) {
		pwm->pwm_ctl[0] |= ramp_enabled;
		pm8058_write(pwm->chip->pm_chip, SSBI_REG_ADDR_LPG_CTL(0),
			     &pwm->pwm_ctl[0], 1);
	}

	return rc;
}

/* APIs */
/*
 * pwm_request - request a PWM device
 */
struct pwm_device *pwm_request(int pwm_id, const char *label)
{
	struct pwm_device	*pwm;

	if (pwm_id > PM8058_PWM_CHANNELS || pwm_id < 0)
		return ERR_PTR(-EINVAL);
	if (pwm_chip == NULL)
		return ERR_PTR(-ENODEV);

	mutex_lock(&pwm_chip->pwm_mutex);
	pwm = &pwm_chip->pwm_dev[pwm_id];
	if (!pwm->in_use) {
		pwm->in_use = 1;
		pwm->label = label;

		if (pwm_chip->pdata && pwm_chip->pdata->config)
			pwm_chip->pdata->config(pwm, pwm_id, 1);
	} else
		pwm = ERR_PTR(-EBUSY);
	mutex_unlock(&pwm_chip->pwm_mutex);

	return pwm;
}
EXPORT_SYMBOL(pwm_request);

/*
 * pwm_free - free a PWM device
 */
void pwm_free(struct pwm_device *pwm)
{
	if (pwm == NULL || IS_ERR(pwm) || pwm->chip == NULL)
		return;

	mutex_lock(&pwm->chip->pwm_mutex);
	if (pwm->in_use) {
		pm8058_pwm_bank_sel(pwm);
		pm8058_pwm_start(pwm, 0, 0);

		if (pwm->chip->pdata && pwm->chip->pdata->config)
			pwm->chip->pdata->config(pwm, pwm->pwm_id, 0);

		pwm->in_use = 0;
		pwm->label = NULL;
	}
	pm8058_pwm_bank_enable(pwm, 0);
	mutex_unlock(&pwm->chip->pwm_mutex);
}
EXPORT_SYMBOL(pwm_free);

/*
 * pwm_config - change a PWM device configuration
 *
 * @pwm: the PWM device
 * @period_us: period in micro second
 * @duty_us: duty cycle in micro second
 */
int pwm_config(struct pwm_device *pwm, int duty_us, int period_us)
{
	struct pw8058_pwm_config	pwm_conf;
	unsigned int max_pwm_value, tmp;
	int	rc;

	if (pwm == NULL || IS_ERR(pwm) ||
		(unsigned)duty_us > (unsigned)period_us ||
		(unsigned)period_us > PM_PWM_PERIOD_MAX ||
		(unsigned)period_us < PM_PWM_PERIOD_MIN)
		return -EINVAL;
	if (pwm->chip == NULL)
		return -ENODEV;

	mutex_lock(&pwm->chip->pwm_mutex);

	if (!pwm->in_use) {
		rc = -EINVAL;
		goto out_unlock;
	}

	pm8058_pwm_calc_period(period_us, &pwm_conf);

	/* Figure out pwm_value with overflow handling */
	if ((unsigned)period_us > (1 << pwm_conf.pwm_size)) {
		tmp = period_us;
		tmp >>= pwm_conf.pwm_size;
		pwm_conf.pwm_value = (unsigned)duty_us / tmp;
	} else {
		tmp = duty_us;
		tmp <<= pwm_conf.pwm_size;
		pwm_conf.pwm_value = tmp / (unsigned)period_us;
	}
	max_pwm_value = (1 << pwm_conf.pwm_size) - 1;
	if (pwm_conf.pwm_value > max_pwm_value)
		pwm_conf.pwm_value = max_pwm_value;

	pwm_conf.bypass_lut = 1;

	pr_debug("%s: duty/period=%u/%u usec: pwm_value=%d (of %d)\n",
		 __func__, (unsigned)duty_us, (unsigned)period_us,
		 pwm_conf.pwm_value, 1 << pwm_conf.pwm_size);

	rc = pm8058_pwm_configure(pwm, &pwm_conf);

out_unlock:
	mutex_unlock(&pwm->chip->pwm_mutex);
	return rc;
}
EXPORT_SYMBOL(pwm_config);

/*
 * pwm_enable - start a PWM output toggling
 */
int pwm_enable(struct pwm_device *pwm)
{
	int	rc;

	if (pwm == NULL || IS_ERR(pwm))
		return -EINVAL;
	if (pwm->chip == NULL)
		return -ENODEV;

	mutex_lock(&pwm->chip->pwm_mutex);
	if (!pwm->in_use)
		rc = -EINVAL;
	else {
		if (pwm->chip->pdata && pwm->chip->pdata->enable)
			pwm->chip->pdata->enable(pwm, pwm->pwm_id, 1);

		rc = pm8058_pwm_bank_enable(pwm, 1);

		pm8058_pwm_bank_sel(pwm);
		pm8058_pwm_start(pwm, 1, 0);
	}
	mutex_unlock(&pwm->chip->pwm_mutex);
	return rc;
}
EXPORT_SYMBOL(pwm_enable);

/*
 * pwm_disable - stop a PWM output toggling
 */
void pwm_disable(struct pwm_device *pwm)
{
	if (pwm == NULL || IS_ERR(pwm) || pwm->chip == NULL)
		return;

	mutex_lock(&pwm->chip->pwm_mutex);
	if (pwm->in_use) {
		pm8058_pwm_bank_sel(pwm);
		pm8058_pwm_start(pwm, 0, 0);

		pm8058_pwm_bank_enable(pwm, 0);

		if (pwm->chip->pdata && pwm->chip->pdata->enable)
			pwm->chip->pdata->enable(pwm, pwm->pwm_id, 0);
	}
	mutex_unlock(&pwm->chip->pwm_mutex);
}
EXPORT_SYMBOL(pwm_disable);

/*
 * pm8058_pwm_lut_config - change a PWM device configuration to use LUT
 *
 * @pwm: the PWM device
 * @period_us: period in micro second
 * @duty_pct: arrary of duty cycles in percent, like 20, 50.
 * @duty_time_ms: time for each duty cycle in millisecond
 * @start_idx: start index in lookup table from 0 to MAX-1
 * @idx_len: number of index
 * @pause_lo: pause time in millisecond at low index
 * @pause_hi: pause time in millisecond at high index
 * @flags: control flags
 *
 */
int pm8058_pwm_lut_config(struct pwm_device *pwm, int period_us,
			  int duty_pct[], int duty_time_ms, int start_idx,
			  int idx_len, int pause_lo, int pause_hi, int flags)
{
	struct pw8058_pwm_config	pwm_conf;
	unsigned int pwm_value, max_pwm_value;
	u8	cfg0, cfg1;
	int	i, len;
	int	rc;

	if (pwm == NULL || IS_ERR(pwm) || !idx_len)
		return -EINVAL;
	if (duty_pct == NULL && !(flags & PM_PWM_LUT_NO_TABLE))
		return -EINVAL;
	if (pwm->chip == NULL)
		return -ENODEV;
	if (idx_len >= PM_PWM_LUT_SIZE && start_idx)
		return -EINVAL;
	if ((start_idx + idx_len) > PM_PWM_LUT_SIZE)
		return -EINVAL;
	if ((unsigned)period_us > PM_PWM_PERIOD_MAX ||
		(unsigned)period_us < PM_PWM_PERIOD_MIN)
		return -EINVAL;

	mutex_lock(&pwm->chip->pwm_mutex);

	if (!pwm->in_use) {
		rc = -EINVAL;
		goto out_unlock;
	}

	pm8058_pwm_calc_period(period_us, &pwm_conf);

	len = (idx_len > PM_PWM_LUT_SIZE) ? PM_PWM_LUT_SIZE : idx_len;

	if (flags & PM_PWM_LUT_NO_TABLE)
		goto after_table_write;

	max_pwm_value = (1 << pwm_conf.pwm_size) - 1;
	for (i = 0; i < len; i++) {
		pwm_value = (duty_pct[i] << pwm_conf.pwm_size) / 100;
		/* Avoid overflow */
		if (pwm_value > max_pwm_value)
			pwm_value = max_pwm_value;
		cfg0 = pwm_value & 0xff;
		cfg1 = (pwm_value >> 1) & 0x80;
		cfg1 |= start_idx + i;

		pr_debug("%s: %d: pwm=%d\n", __func__, i, pwm_value);

		pm8058_write(pwm->chip->pm_chip,
			     SSBI_REG_ADDR_LPG_LUT_CFG0,
			     &cfg0, 1);
		pm8058_write(pwm->chip->pm_chip,
			     SSBI_REG_ADDR_LPG_LUT_CFG1,
			     &cfg1, 1);
	}

after_table_write:
	pwm_conf.lut_duty_ms = duty_time_ms;
	pwm_conf.lut_lo_index = start_idx;
	pwm_conf.lut_hi_index = start_idx + len - 1;
	pwm_conf.lut_pause_lo = pause_lo;
	pwm_conf.lut_pause_hi = pause_hi;
	pwm_conf.flags = flags;
	pwm_conf.bypass_lut = 0;

	rc = pm8058_pwm_configure(pwm, &pwm_conf);

out_unlock:
	mutex_unlock(&pwm->chip->pwm_mutex);
	return rc;
}
EXPORT_SYMBOL(pm8058_pwm_lut_config);

/*
 * pm8058_pwm_lut_enable - control a PWM device to start/stop LUT ramp
 *
 * @pwm: the PWM device
 * @start: to start (1), or stop (0)
 */
int pm8058_pwm_lut_enable(struct pwm_device *pwm, int start)
{
	if (pwm == NULL || IS_ERR(pwm))
		return -EINVAL;
	if (pwm->chip == NULL)
		return -ENODEV;

	mutex_lock(&pwm->chip->pwm_mutex);
	if (start) {
		pm8058_pwm_bank_enable(pwm, 1);

		pm8058_pwm_bank_sel(pwm);
		pm8058_pwm_start(pwm, 1, 1);
	} else {
		pm8058_pwm_bank_sel(pwm);
		pm8058_pwm_start(pwm, 0, 0);

		pm8058_pwm_bank_enable(pwm, 0);
	}
	mutex_unlock(&pwm->chip->pwm_mutex);
	return 0;
}
EXPORT_SYMBOL(pm8058_pwm_lut_enable);

#define SSBI_REG_ADDR_LED_BASE		0x131
#define SSBI_REG_ADDR_LED(n)		(SSBI_REG_ADDR_LED_BASE + (n))
#define SSBI_REG_ADDR_FLASH_BASE	0x48
#define SSBI_REG_ADDR_FLASH_DRV_1	0xFB
#define SSBI_REG_ADDR_FLASH(n)		(((n) < 2 ? \
					    SSBI_REG_ADDR_FLASH_BASE + (n) : \
					    SSBI_REG_ADDR_FLASH_DRV_1))

#define PM8058_LED_CURRENT_SHIFT	3
#define PM8058_LED_MODE_MASK		0x07

#define PM8058_FLASH_CURRENT_SHIFT	4
#define PM8058_FLASH_MODE_MASK		0x03
#define PM8058_FLASH_MODE_NONE		0
#define PM8058_FLASH_MODE_DTEST1	1
#define PM8058_FLASH_MODE_DTEST2	2
#define PM8058_FLASH_MODE_PWM		3

int pm8058_pwm_config_led(struct pwm_device *pwm, int id,
			  int mode, int max_current)
{
	int	rc;
	u8	conf;

	switch (id) {
	case PM_PWM_LED_0:
	case PM_PWM_LED_1:
	case PM_PWM_LED_2:
		conf = mode & PM8058_LED_MODE_MASK;
		conf |= (max_current / 2) << PM8058_LED_CURRENT_SHIFT;
		rc = pm8058_write(pwm->chip->pm_chip,
				  SSBI_REG_ADDR_LED(id), &conf, 1);
		break;

	case PM_PWM_LED_KPD:
	case PM_PWM_LED_FLASH:
	case PM_PWM_LED_FLASH1:
		switch (mode) {
		case PM_PWM_CONF_PWM1:
		case PM_PWM_CONF_PWM2:
		case PM_PWM_CONF_PWM3:
			conf = PM8058_FLASH_MODE_PWM;
			break;
		case PM_PWM_CONF_DTEST1:
			conf = PM8058_FLASH_MODE_DTEST1;
			break;
		case PM_PWM_CONF_DTEST2:
			conf = PM8058_FLASH_MODE_DTEST2;
			break;
		default:
			conf = PM8058_FLASH_MODE_NONE;
			break;
		}
		conf |= (max_current / 20) << PM8058_FLASH_CURRENT_SHIFT;
		id -= PM_PWM_LED_KPD;
		rc = pm8058_write(pwm->chip->pm_chip,
				  SSBI_REG_ADDR_FLASH(id), &conf, 1);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}
EXPORT_SYMBOL(pm8058_pwm_config_led);

int pm8058_pwm_set_dtest(struct pwm_device *pwm, int enable)
{
	int	rc;
	u8	reg;

	if (pwm == NULL || IS_ERR(pwm))
		return -EINVAL;
	if (pwm->chip == NULL)
		return -ENODEV;

	if (!pwm->in_use)
		rc = -EINVAL;
	else {
		reg = pwm->pwm_id & PM8058_PWM_DTEST_BANK_MASK;
		if (enable)
			/* Only Test 1 available */
			reg |= (1 << PM8058_PWM_DTEST_SHIFT) &
				PM8058_PWM_DTEST_MASK;
		rc = pm8058_write(pwm->chip->pm_chip, SSBI_REG_ADDR_LPG_TEST,
				  &reg, 1);
		if (rc)
			pr_err("%s: pm8058_write(DTEST=0x%x): rc=%d\n",
			       __func__, reg, rc);

	}
	return rc;
}
EXPORT_SYMBOL(pm8058_pwm_set_dtest);

static int __devinit pmic8058_pwm_probe(struct platform_device *pdev)
{
	struct pm8058_chip	*pm_chip;
	struct pm8058_pwm_chip	*chip;
	int	i;

	pm_chip = dev_get_drvdata(pdev->dev.parent);
	if (pm_chip == NULL) {
		pr_err("%s: no parent data passed in.\n", __func__);
		return -EFAULT;
	}

	chip = kzalloc(sizeof *chip, GFP_KERNEL);
	if (chip == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < PM8058_PWM_CHANNELS; i++) {
		chip->pwm_dev[i].pwm_id = i;
		chip->pwm_dev[i].chip = chip;
	}

	mutex_init(&chip->pwm_mutex);

	chip->pdata = pdev->dev.platform_data;
	chip->pm_chip = pm_chip;
	pwm_chip = chip;
	platform_set_drvdata(pdev, chip);

	pr_notice("%s: OK\n", __func__);
	return 0;
}

static int __devexit pmic8058_pwm_remove(struct platform_device *pdev)
{
	struct pm8058_pwm_chip	*chip = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	kfree(chip);
	return 0;
}

static struct platform_driver pmic8058_pwm_driver = {
	.probe		= pmic8058_pwm_probe,
	.remove		= __devexit_p(pmic8058_pwm_remove),
	.driver		= {
		.name = "pm8058-pwm",
		.owner = THIS_MODULE,
	},
};

static int __init pm8058_pwm_init(void)
{
	return platform_driver_register(&pmic8058_pwm_driver);
}

static void __exit pm8058_pwm_exit(void)
{
	platform_driver_unregister(&pmic8058_pwm_driver);
}

subsys_initcall(pm8058_pwm_init);
module_exit(pm8058_pwm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8058 PWM driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pmic8058_pwm");
