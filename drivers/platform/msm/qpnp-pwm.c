/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
 * Qualcomm QPNP Pulse Width Modulation (PWM) driver
 *
 * The HW module is also called LPG (Light Pattern Generator).
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/spmi.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/radix-tree.h>
#include <linux/qpnp/pwm.h>

#define QPNP_LPG_DRIVER_NAME	"qcom,qpnp-pwm"

/* LPG Control for LPG_PATTERN_CONFIG */
#define QPNP_RAMP_DIRECTION_SHIFT	4
#define QPNP_RAMP_DIRECTION_MASK	0x10
#define QPNP_PATTERN_REPEAT_SHIFT	3
#define QPNP_PATTERN_REPEAT_MASK	0x08
#define QPNP_RAMP_TOGGLE_SHIFT		2
#define QPNP_RAMP_TOGGLE_MASK		0x04
#define QPNP_EN_PAUSE_HI_SHIFT		1
#define QPNP_EN_PAUSE_HI_MASK		0x02
#define QPNP_EN_PAUSE_LO_MASK		0x01

/* LPG Control for LPG_PWM_SIZE_CLK */
#define QPNP_PWM_SIZE_SHIFT			4
#define QPNP_PWM_SIZE_MASK			0x30
#define QPNP_PWM_FREQ_CLK_SELECT_SHIFT		0
#define QPNP_PWM_FREQ_CLK_SELECT_MASK		0x03
#define QPNP_PWM_SIZE_9_BIT			0x03

#define QPNP_SET_PWM_CLK(val, clk, pwm_size) \
do { \
	val = (clk + 1) & QPNP_PWM_FREQ_CLK_SELECT_MASK; \
	val |= ((pwm_size > 6 ? QPNP_PWM_SIZE_9_BIT : 0) << \
			QPNP_PWM_SIZE_SHIFT) & QPNP_PWM_SIZE_MASK; \
} while (0)

#define QPNP_GET_PWM_SIZE(reg) ((reg & QPNP_PWM_SIZE_MASK) \
					>> QPNP_PWM_SIZE_SHIFT)

/* LPG Control for LPG_PWM_FREQ_PREDIV_CLK */
#define QPNP_PWM_FREQ_PRE_DIVIDE_SHIFT		5
#define QPNP_PWM_FREQ_PRE_DIVIDE_MASK		0x60
#define QPNP_PWM_FREQ_EXP_MASK			0x07

#define QPNP_SET_PWM_FREQ_PREDIV(val, pre_div, pre_div_exp) \
do { \
	val = (pre_div << QPNP_PWM_FREQ_PRE_DIVIDE_SHIFT) & \
				QPNP_PWM_FREQ_PRE_DIVIDE_MASK;	\
	val |= pre_div_exp & QPNP_PWM_FREQ_EXP_MASK;	\
} while (0)

/* LPG Control for LPG_PWM_TYPE_CONFIG */
#define QPNP_EN_GLITCH_REMOVAL_SHIFT		5
#define QPNP_EN_GLITCH_REMOVAL_MASK		0x20
#define QPNP_EN_FULL_SCALE_SHIFT		3
#define QPNP_EN_FULL_SCALE_MASK			0x08
#define QPNP_EN_PHASE_STAGGER_SHIFT		2
#define QPNP_EN_PHASE_STAGGER_MASK		0x04
#define QPNP_PHASE_STAGGER_MASK			0x03

/* LPG Control for PWM_VALUE_LSB */
#define QPNP_PWM_VALUE_LSB_MASK			0xFF

/* LPG Control for PWM_VALUE_MSB */
#define QPNP_PWM_VALUE_MSB_SHIFT		8
#define QPNP_PWM_VALUE_MSB_MASK			0x01

/* LPG Control for ENABLE_CONTROL */
#define QPNP_EN_PWM_HIGH_SHIFT			7
#define QPNP_EN_PWM_HIGH_MASK			0x80
#define QPNP_EN_PWM_LO_SHIFT			6
#define QPNP_EN_PWM_LO_MASK			0x40
#define QPNP_EN_PWM_OUTPUT_SHIFT		5
#define QPNP_EN_PWM_OUTPUT_MASK			0x20
#define QPNP_PWM_SRC_SELECT_SHIFT		2
#define QPNP_PWM_SRC_SELECT_MASK		0x04
#define QPNP_PWM_EN_RAMP_GEN_SHIFT		1
#define QPNP_PWM_EN_RAMP_GEN_MASK		0x02

#define QPNP_ENABLE_PWM(value) \
	(value |= (1 << QPNP_EN_PWM_OUTPUT_SHIFT) & QPNP_EN_PWM_OUTPUT_MASK)

#define QPNP_DISABLE_PWM(value)  (value &= ~QPNP_EN_PWM_OUTPUT_MASK)

/* LPG Control for RAMP_CONTROL */
#define QPNP_RAMP_START_MASK			0x01

#define QPNP_ENABLE_LUT(value) (value |= QPNP_RAMP_START_MASK)
#define QPNP_DISABLE_LUT(value) (value &= ~QPNP_RAMP_START_MASK)

/* LPG Control for RAMP_STEP_DURATION_LSB */
#define QPNP_RAMP_STEP_DURATION_LSB_MASK	0xFF

/* LPG Control for RAMP_STEP_DURATION_MSB */
#define QPNP_RAMP_STEP_DURATION_MSB_SHIFT	8
#define QPNP_RAMP_STEP_DURATION_MSB_MASK	0x01

#define QPNP_PWM_1KHZ				1024
#define QPNP_GET_RAMP_STEP_DURATION(ramp_time_ms) \
		((ramp_time_ms * QPNP_PWM_1KHZ) / 1000)

/* LPG Control for PAUSE_HI_MULTIPLIER_LSB */
#define QPNP_PAUSE_HI_MULTIPLIER_LSB_MASK	0xFF

/* LPG Control for PAUSE_HI_MULTIPLIER_MSB */
#define QPNP_PAUSE_HI_MULTIPLIER_MSB_SHIFT	8
#define QPNP_PAUSE_HI_MULTIPLIER_MSB_MASK	0x1F

/* LPG Control for PAUSE_LO_MULTIPLIER_LSB */
#define QPNP_PAUSE_LO_MULTIPLIER_LSB_MASK	0xFF

/* LPG Control for PAUSE_LO_MULTIPLIER_MSB */
#define QPNP_PAUSE_LO_MULTIPLIER_MSB_SHIFT	8
#define QPNP_PAUSE_LO_MULTIPLIER_MSB_MASK	0x1F

/* LPG Control for HI_INDEX */
#define QPNP_HI_INDEX_MASK			0x3F

/* LPG Control for LO_INDEX */
#define QPNP_LO_INDEX_MASK			0x3F

#define NUM_CLOCKS				3
#define QPNP_PWM_M_MAX				7
#define NSEC_1024HZ	(NSEC_PER_SEC / 1024)
#define NSEC_32768HZ	(NSEC_PER_SEC / 32768)
#define NSEC_19P2MHZ	(NSEC_PER_SEC / 19200000)

#define NUM_LPG_PRE_DIVIDE	4

#define PRE_DIVIDE_1		1
#define PRE_DIVIDE_3		3
#define PRE_DIVIDE_5		5
#define PRE_DIVIDE_6		6

#define SPMI_LPG_REG_ADDR_BASE	0x40
#define SPMI_LPG_REG_ADDR(b, n)	(b + SPMI_LPG_REG_ADDR_BASE + (n))
#define SPMI_MAX_BUF_LEN	8

/* SPMI LPG registers */
enum qpnp_lpg_registers_list {
	QPNP_LPG_PATTERN_CONFIG,
	QPNP_LPG_PWM_SIZE_CLK,
	QPNP_LPG_PWM_FREQ_PREDIV_CLK,
	QPNP_LPG_PWM_TYPE_CONFIG,
	QPNP_PWM_VALUE_LSB,
	QPNP_PWM_VALUE_MSB,
	QPNP_ENABLE_CONTROL,
	QPNP_RAMP_CONTROL,
	QPNP_RAMP_STEP_DURATION_LSB = QPNP_RAMP_CONTROL + 9,
	QPNP_RAMP_STEP_DURATION_MSB,
	QPNP_PAUSE_HI_MULTIPLIER_LSB,
	QPNP_PAUSE_HI_MULTIPLIER_MSB,
	QPNP_PAUSE_LO_MULTIPLIER_LSB,
	QPNP_PAUSE_LO_MULTIPLIER_MSB,
	QPNP_HI_INDEX,
	QPNP_LO_INDEX,
	QPNP_TOTAL_LPG_SPMI_REGISTERS
};

/*
 * Formula from HSID,
 * pause_time (hi/lo) = (pause_cnt- 1)*(ramp_ms)
 * OR,
 * pause_cnt = (pause_time / ramp_ms) + 1
 */
#define QPNP_SET_PAUSE_CNT(to_pause_cnt, from_pause, ramp_ms) \
	(to_pause_cnt = (from_pause / (ramp_ms ? ramp_ms : 1)) + 1)


static unsigned int pt_t[NUM_LPG_PRE_DIVIDE][NUM_CLOCKS] = {
	{	PRE_DIVIDE_1 * NSEC_1024HZ,
		PRE_DIVIDE_1 * NSEC_32768HZ,
		PRE_DIVIDE_1 * NSEC_19P2MHZ,
	},
	{	PRE_DIVIDE_3 * NSEC_1024HZ,
		PRE_DIVIDE_3 * NSEC_32768HZ,
		PRE_DIVIDE_3 * NSEC_19P2MHZ,
	},
	{	PRE_DIVIDE_5 * NSEC_1024HZ,
		PRE_DIVIDE_5 * NSEC_32768HZ,
		PRE_DIVIDE_5 * NSEC_19P2MHZ,
	},
	{	PRE_DIVIDE_6 * NSEC_1024HZ,
		PRE_DIVIDE_6 * NSEC_32768HZ,
		PRE_DIVIDE_6 * NSEC_19P2MHZ,
	},
};

static RADIX_TREE(lpg_dev_tree, GFP_KERNEL);

struct qpnp_lut_default_config {
	u32		*duty_pct_list;
	int		size;
	int		start_idx;
};

struct qpnp_lut_config {
	struct qpnp_lut_default_config def_config;
	u8		*duty_pct_list;
	int		list_size;
	int		lo_index;
	int		hi_index;
	int		lut_pause_hi_cnt;
	int		lut_pause_lo_cnt;
	int		ramp_step_ms;
	bool		ramp_direction;
	bool		pattern_repeat;
	bool		ramp_toggle;
	bool		enable_pause_hi;
	bool		enable_pause_lo;
};

struct qpnp_lpg_config {
	struct qpnp_lut_config	lut_config;
	u16			base_addr;
	u16			lut_base_addr;
	u16			lut_size;
	bool			bypass_lut;
	bool			lpg_configured;
};

struct qpnp_pwm_config {
	int				channel_id;
	bool				in_use;
	const char			*lable;
	int				pwm_value;
	int				pwm_period;
	int				pwm_duty;
	struct pwm_period_config	period;
};

/* Public facing structure */
struct pwm_device {
	struct qpnp_lpg_chip	*chip;
	struct qpnp_pwm_config	pwm_config;
};

struct qpnp_lpg_chip {
	struct	spmi_device	*spmi_dev;
	struct	pwm_device	pwm_dev;
	struct	mutex		lpg_mutex;
	struct	qpnp_lpg_config	lpg_config;
	u8	qpnp_lpg_registers[QPNP_TOTAL_LPG_SPMI_REGISTERS];
};

/* Internal functions */
static inline void qpnp_set_pattern_config(u8 *val,
			struct qpnp_lut_config *lut_config)
{
	*val = lut_config->enable_pause_lo & QPNP_EN_PAUSE_LO_MASK;
	*val |= (lut_config->enable_pause_hi << QPNP_EN_PAUSE_HI_SHIFT) &
						QPNP_EN_PAUSE_HI_MASK;
	*val |= (lut_config->ramp_toggle << QPNP_RAMP_TOGGLE_SHIFT) &
						QPNP_RAMP_TOGGLE_MASK;
	*val |= (lut_config->pattern_repeat << QPNP_PATTERN_REPEAT_SHIFT) &
						QPNP_PATTERN_REPEAT_MASK;
	*val |= (lut_config->ramp_direction << QPNP_RAMP_DIRECTION_SHIFT) &
						QPNP_RAMP_DIRECTION_MASK;
}

static inline void qpnp_set_pwm_type_config(u8 *val, bool glitch,
			bool full_scale, bool en_phase, bool phase)
{
	*val = phase;
	*val |= (en_phase << QPNP_EN_PHASE_STAGGER_SHIFT) &
				QPNP_EN_PHASE_STAGGER_MASK;
	*val |= (full_scale << QPNP_EN_FULL_SCALE_SHIFT) &
				QPNP_EN_FULL_SCALE_MASK;
	*val |= (glitch << QPNP_EN_GLITCH_REMOVAL_SHIFT) &
				QPNP_EN_GLITCH_REMOVAL_MASK;
}

static inline void qpnp_set_control(u8 *val, bool pwm_hi, bool pwm_lo,
			bool pwm_out, bool pwm_src, bool ramp_gen)
{
	*val = (ramp_gen << QPNP_PWM_EN_RAMP_GEN_SHIFT) &
				QPNP_PWM_EN_RAMP_GEN_MASK;
	*val |= (pwm_src << QPNP_PWM_SRC_SELECT_SHIFT) &
				QPNP_PWM_SRC_SELECT_MASK;
	*val |= (pwm_out << QPNP_EN_PWM_OUTPUT_SHIFT) &
				QPNP_EN_PWM_OUTPUT_MASK;
	*val |= (pwm_lo << QPNP_EN_PWM_LO_SHIFT) & QPNP_EN_PWM_LO_MASK;
	*val |= (pwm_hi << QPNP_EN_PWM_HIGH_SHIFT) & QPNP_EN_PWM_HIGH_MASK;
}

#define QPNP_ENABLE_LUT_CONTROL(p_val)	qpnp_set_control(p_val, 1, 1, 1, 0, 1)
#define QPNP_ENABLE_PWM_CONTROL(p_val)	qpnp_set_control(p_val, 1, 1, 0, 1, 0)

static inline void qpnp_convert_to_lut_flags(int *flags,
				struct qpnp_lut_config *l_config)
{
	*flags = ((l_config->ramp_direction ? PM_PWM_LUT_RAMP_UP : 0) |
		(l_config->pattern_repeat ? PM_PWM_LUT_LOOP : 0)|
		(l_config->ramp_toggle ? PM_PWM_LUT_REVERSE : 0) |
		(l_config->enable_pause_hi ? PM_PWM_LUT_PAUSE_HI_EN : 0) |
		(l_config->enable_pause_lo ? PM_PWM_LUT_PAUSE_LO_EN : 0));
}

static inline void qpnp_set_lut_params(struct lut_params *l_params,
				struct qpnp_lut_config *l_config)
{
	l_params->start_idx = l_config->def_config.start_idx;
	l_params->idx_len = l_config->def_config.size;
	l_params->lut_pause_hi = l_config->lut_pause_hi_cnt;
	l_params->lut_pause_lo = l_config->lut_pause_lo_cnt;
	l_params->ramp_step_ms = l_config->ramp_step_ms;
	qpnp_convert_to_lut_flags(&l_params->flags, l_config);
}

static void qpnp_lpg_save(u8 *u8p, u8 mask, u8 val)
{
	*u8p &= ~mask;
	*u8p |= val & mask;
}

static int qpnp_lpg_save_and_write(u8 value, u8 mask, u8 *reg, u16 base_addr,
			u16 offset, u16 size, struct qpnp_lpg_chip *chip)
{
	qpnp_lpg_save(reg, mask, value);

	return spmi_ext_register_writel(chip->spmi_dev->ctrl,
	chip->spmi_dev->sid, SPMI_LPG_REG_ADDR(base_addr, offset), reg, size);
}

/*
 * PWM Frequency = Clock Frequency / (N * T)
 *	or
 * PWM Period = Clock Period * (N * T)
 *	where
 * N = 2^9 or 2^6 for 9-bit or 6-bit PWM size
 * T = Pre-divide * 2^m, where m = 0..7 (exponent)
 *
 * This is the formula to figure out m for the best pre-divide and clock:
 * (PWM Period / N) = (Pre-divide * Clock Period) * 2^m
 */
static void qpnp_lpg_calc_period(unsigned int period_us,
				   struct pwm_period_config *period)
{
	int		n, m, clk, div;
	int		best_m, best_div, best_clk;
	unsigned int	last_err, cur_err, min_err;
	unsigned int	tmp_p, period_n;

	/* PWM Period / N */
	if (period_us < ((unsigned)(-1) / NSEC_PER_USEC)) {
		period_n = (period_us * NSEC_PER_USEC) >> 6;
		n = 6;
	} else {
		period_n = (period_us >> 9) * NSEC_PER_USEC;
		n = 9;
	}

	min_err = last_err = (unsigned)(-1);
	best_m = 0;
	best_clk = 0;
	best_div = 0;
	for (clk = 0; clk < NUM_CLOCKS; clk++) {
		for (div = 0; div < NUM_LPG_PRE_DIVIDE; div++) {
			/* period_n = (PWM Period / N) */
			/* tmp_p = (Pre-divide * Clock Period) * 2^m */
			tmp_p = pt_t[div][clk];
			for (m = 0; m <= QPNP_PWM_M_MAX; m++) {
				if (period_n > tmp_p)
					cur_err = period_n - tmp_p;
				else
					cur_err = tmp_p - period_n;

				if (cur_err < min_err) {
					min_err = cur_err;
					best_m = m;
					best_clk = clk;
					best_div = div;
				}

				if (m && cur_err > last_err)
					/* Break for bigger cur_err */
					break;

				last_err = cur_err;
				tmp_p <<= 1;
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

static void qpnp_lpg_calc_pwm_value(struct pwm_device *pwm,
				      unsigned int period_us,
				      unsigned int duty_us)
{
	unsigned int		max_pwm_value, tmp;
	struct qpnp_pwm_config	*pwm_config = &pwm->pwm_config;

	/* Figure out pwm_value with overflow handling */
	tmp = 1 << (sizeof(tmp) * 8 - pwm_config->period.pwm_size);
	if (duty_us < tmp) {
		tmp = duty_us << pwm_config->period.pwm_size;
		pwm_config->pwm_value = tmp / period_us;
	} else {
		tmp = period_us >> pwm_config->period.pwm_size;
		pwm_config->pwm_value = duty_us / tmp;
	}
	max_pwm_value = (1 << pwm_config->period.pwm_size) - 1;
	if (pwm_config->pwm_value > max_pwm_value)
		pwm_config->pwm_value = max_pwm_value;
}

static int qpnp_lpg_change_table(struct pwm_device *pwm,
					int duty_pct[], int raw_value)
{
	unsigned int		pwm_value, max_pwm_value;
	struct qpnp_lpg_chip	*chip = pwm->chip;
	struct qpnp_lut_config	*lut = &chip->lpg_config.lut_config;
	int			i, pwm_size, rc;
	int			burst_size = SPMI_MAX_BUF_LEN;
	int			list_len = lut->list_size << 1;
	int			offset = lut->lo_index << 2;

	pwm_size = QPNP_GET_PWM_SIZE(
			chip->qpnp_lpg_registers[QPNP_LPG_PWM_SIZE_CLK]) &
						QPNP_PWM_SIZE_9_BIT ? 9 : 6;

	max_pwm_value = (1 << pwm_size) - 1;

	if (unlikely(lut->list_size != (lut->hi_index - lut->lo_index + 1))) {
		pr_err("LUT internal Data structure corruption detected\n");
		pr_err("LUT list size: %d\n", lut->list_size);
		pr_err("However, index size is: %d\n",
				(lut->hi_index - lut->lo_index + 1));
		return -EINVAL;
	}

	for (i = 0; i <= lut->list_size; i++) {
		if (raw_value)
			pwm_value = duty_pct[i];
		else
			pwm_value = (duty_pct[i] << pwm_size) / 100;

		if (pwm_value > max_pwm_value)
			pwm_value = max_pwm_value;

		lut->duty_pct_list[i*2] = pwm_value;
		lut->duty_pct_list[(i*2)+1] = (pwm_value >>
			 QPNP_PWM_VALUE_MSB_SHIFT) & QPNP_PWM_VALUE_MSB_MASK;
	}

	/* Write with max allowable burst mode, each entry is of two bytes */
	for (i = 0; i < list_len;) {
		if (i + burst_size >= list_len)
			burst_size = list_len - i;
		rc = spmi_ext_register_writel(chip->spmi_dev->ctrl,
			chip->spmi_dev->sid,
			chip->lpg_config.lut_base_addr + offset + i,
			lut->duty_pct_list + i, burst_size);
		i += burst_size;
	}

	return rc;
}

static void qpnp_lpg_save_period(struct pwm_device *pwm)
{
	u8 mask, val;
	struct qpnp_lpg_chip	*chip = pwm->chip;
	struct qpnp_pwm_config	*pwm_config = &pwm->pwm_config;

	QPNP_SET_PWM_CLK(val, pwm_config->period.clk,
				pwm_config->period.pwm_size);

	mask = QPNP_PWM_SIZE_MASK | QPNP_PWM_FREQ_CLK_SELECT_MASK;

	qpnp_lpg_save(&chip->qpnp_lpg_registers[QPNP_LPG_PWM_SIZE_CLK],
							mask, val);

	QPNP_SET_PWM_FREQ_PREDIV(val, pwm_config->period.pre_div,
					pwm_config->period.pre_div_exp);

	mask = QPNP_PWM_FREQ_PRE_DIVIDE_MASK | QPNP_PWM_FREQ_EXP_MASK;

	qpnp_lpg_save(&chip->qpnp_lpg_registers[QPNP_LPG_PWM_FREQ_PREDIV_CLK],
								mask, val);
}

static int qpnp_lpg_save_pwm_value(struct pwm_device *pwm)
{
	unsigned int		max_pwm_value;
	int			pwm_size;
	u8			mask, value;
	struct qpnp_lpg_chip	*chip = pwm->chip;
	struct qpnp_pwm_config	*pwm_config = &pwm->pwm_config;
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	int rc;

	pwm_size = QPNP_GET_PWM_SIZE(
			chip->qpnp_lpg_registers[QPNP_LPG_PWM_SIZE_CLK]) &
						QPNP_PWM_SIZE_9_BIT ? 9 : 6;

	max_pwm_value = (1 << pwm_size) - 1;

	if (pwm_config->pwm_value > max_pwm_value)
		pwm_config->pwm_value = max_pwm_value;

	value = pwm_config->pwm_value;
	mask = QPNP_PWM_VALUE_LSB_MASK;

	rc = qpnp_lpg_save_and_write(value, mask,
			&pwm->chip->qpnp_lpg_registers[QPNP_PWM_VALUE_LSB],
			lpg_config->base_addr, QPNP_PWM_VALUE_LSB, 1, chip);
	if (rc)
		return rc;

	value = (pwm_config->pwm_value >> QPNP_PWM_VALUE_MSB_SHIFT) &
					QPNP_PWM_VALUE_MSB_MASK;

	mask = QPNP_PWM_VALUE_MSB_MASK;

	return qpnp_lpg_save_and_write(value, mask,
			&pwm->chip->qpnp_lpg_registers[QPNP_PWM_VALUE_MSB],
			lpg_config->base_addr, QPNP_PWM_VALUE_MSB, 1, chip);
}

static int qpnp_lpg_configure_pattern(struct pwm_device *pwm)
{
	struct qpnp_lpg_config	*lpg_config = &pwm->chip->lpg_config;
	struct qpnp_lut_config	*lut_config = &lpg_config->lut_config;
	struct qpnp_lpg_chip	*chip = pwm->chip;
	u8			value, mask;

	qpnp_set_pattern_config(&value, lut_config);

	mask = QPNP_RAMP_DIRECTION_MASK | QPNP_PATTERN_REPEAT_MASK |
			QPNP_RAMP_TOGGLE_MASK | QPNP_EN_PAUSE_HI_MASK |
			QPNP_EN_PAUSE_LO_MASK;

	return qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_LPG_PATTERN_CONFIG],
		lpg_config->base_addr, QPNP_LPG_PATTERN_CONFIG, 1, chip);
}

static int qpnp_lpg_configure_pwm(struct pwm_device *pwm)
{
	struct qpnp_lpg_config	*lpg_config = &pwm->chip->lpg_config;
	struct qpnp_lpg_chip	*chip = pwm->chip;
	int			rc;
	u8			value, mask;

	rc = spmi_ext_register_writel(chip->spmi_dev->ctrl, chip->spmi_dev->sid,
		SPMI_LPG_REG_ADDR(lpg_config->base_addr, QPNP_LPG_PWM_SIZE_CLK),
		&chip->qpnp_lpg_registers[QPNP_LPG_PWM_SIZE_CLK], 1);

	if (rc)
		return rc;

	rc = spmi_ext_register_writel(chip->spmi_dev->ctrl, chip->spmi_dev->sid,
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_LPG_PWM_FREQ_PREDIV_CLK),
		&chip->qpnp_lpg_registers[QPNP_LPG_PWM_FREQ_PREDIV_CLK], 1);
	if (rc)
		return rc;

	qpnp_set_pwm_type_config(&value, 1, 0, 0, 0);

	mask = QPNP_EN_GLITCH_REMOVAL_MASK | QPNP_EN_FULL_SCALE_MASK |
			QPNP_EN_PHASE_STAGGER_MASK | QPNP_PHASE_STAGGER_MASK;

	return qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_LPG_PWM_TYPE_CONFIG],
		lpg_config->base_addr, QPNP_LPG_PWM_TYPE_CONFIG, 1, chip);
}

static int qpnp_pwm_configure_control(struct pwm_device *pwm)
{
	struct qpnp_lpg_config	*lpg_config = &pwm->chip->lpg_config;
	struct qpnp_lpg_chip	*chip = pwm->chip;
	u8			value, mask;

	QPNP_ENABLE_PWM_CONTROL(&value);

	mask = QPNP_EN_PWM_HIGH_MASK | QPNP_EN_PWM_LO_MASK |
		QPNP_EN_PWM_OUTPUT_MASK | QPNP_PWM_SRC_SELECT_MASK |
					QPNP_PWM_EN_RAMP_GEN_MASK;

	return qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_ENABLE_CONTROL],
		lpg_config->base_addr, QPNP_ENABLE_CONTROL, 1, chip);

}

static int qpnp_lpg_configure_control(struct pwm_device *pwm)
{
	struct qpnp_lpg_config	*lpg_config = &pwm->chip->lpg_config;
	struct qpnp_lpg_chip	*chip = pwm->chip;
	u8			value, mask;

	QPNP_ENABLE_LUT_CONTROL(&value);

	mask = QPNP_EN_PWM_HIGH_MASK | QPNP_EN_PWM_LO_MASK |
		QPNP_EN_PWM_OUTPUT_MASK | QPNP_PWM_SRC_SELECT_MASK |
				QPNP_PWM_EN_RAMP_GEN_MASK;

	return qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_ENABLE_CONTROL],
		lpg_config->base_addr, QPNP_ENABLE_CONTROL, 1, chip);

}

static int qpnp_lpg_configure_ramp_step_duration(struct pwm_device *pwm)
{
	struct qpnp_lpg_config	*lpg_config = &pwm->chip->lpg_config;
	struct qpnp_lut_config	lut_config = lpg_config->lut_config;
	struct qpnp_lpg_chip	*chip = pwm->chip;
	int			rc, value;
	u8			val, mask;

	value = QPNP_GET_RAMP_STEP_DURATION(lut_config.ramp_step_ms);
	val = value & QPNP_RAMP_STEP_DURATION_LSB_MASK;
	mask = QPNP_RAMP_STEP_DURATION_LSB_MASK;

	rc = qpnp_lpg_save_and_write(val, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_RAMP_STEP_DURATION_LSB],
		lpg_config->base_addr, QPNP_RAMP_STEP_DURATION_LSB, 1, chip);
	if (rc)
		return rc;

	val = (value >> QPNP_RAMP_STEP_DURATION_MSB_SHIFT) &
				QPNP_RAMP_STEP_DURATION_MSB_MASK;

	mask = QPNP_RAMP_STEP_DURATION_MSB_MASK;

	return qpnp_lpg_save_and_write(val, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_RAMP_STEP_DURATION_MSB],
		lpg_config->base_addr, QPNP_RAMP_STEP_DURATION_MSB, 1, chip);
}

static int qpnp_lpg_configure_pause(struct pwm_device *pwm)
{
	struct qpnp_lpg_config	*lpg_config = &pwm->chip->lpg_config;
	struct qpnp_lut_config	lut_config = lpg_config->lut_config;
	struct qpnp_lpg_chip	*chip = pwm->chip;
	u8			value, mask;
	int			rc = 0;

	if (lut_config.enable_pause_hi) {
		value = lut_config.lut_pause_hi_cnt;
		mask = QPNP_PAUSE_HI_MULTIPLIER_LSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_PAUSE_HI_MULTIPLIER_LSB],
		lpg_config->base_addr, QPNP_PAUSE_HI_MULTIPLIER_LSB, 1, chip);
		if (rc)
			return rc;

		value = (lut_config.lut_pause_hi_cnt >>
			QPNP_PAUSE_HI_MULTIPLIER_MSB_SHIFT) &
					QPNP_PAUSE_HI_MULTIPLIER_MSB_MASK;

		mask = QPNP_PAUSE_HI_MULTIPLIER_MSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_PAUSE_HI_MULTIPLIER_MSB],
		lpg_config->base_addr, QPNP_PAUSE_HI_MULTIPLIER_MSB, 1, chip);
	} else {
		value = 0;
		mask = QPNP_PAUSE_HI_MULTIPLIER_LSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_PAUSE_HI_MULTIPLIER_LSB],
		lpg_config->base_addr, QPNP_PAUSE_HI_MULTIPLIER_LSB, 1, chip);
		if (rc)
			return rc;

		mask = QPNP_PAUSE_HI_MULTIPLIER_MSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_PAUSE_HI_MULTIPLIER_MSB],
		lpg_config->base_addr, QPNP_PAUSE_HI_MULTIPLIER_MSB, 1, chip);
		if (rc)
			return rc;

	}

	if (lut_config.enable_pause_lo) {
		value = lut_config.lut_pause_lo_cnt;
		mask = QPNP_PAUSE_LO_MULTIPLIER_LSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_PAUSE_LO_MULTIPLIER_LSB],
		lpg_config->base_addr, QPNP_PAUSE_LO_MULTIPLIER_LSB, 1, chip);
		if (rc)
			return rc;

		value = (lut_config.lut_pause_lo_cnt >>
				QPNP_PAUSE_LO_MULTIPLIER_MSB_SHIFT) &
					QPNP_PAUSE_LO_MULTIPLIER_MSB_MASK;

		mask = QPNP_PAUSE_LO_MULTIPLIER_MSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_PAUSE_LO_MULTIPLIER_MSB],
		lpg_config->base_addr, QPNP_PAUSE_LO_MULTIPLIER_MSB, 1, chip);
	} else {
		value = 0;
		mask = QPNP_PAUSE_LO_MULTIPLIER_LSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_PAUSE_LO_MULTIPLIER_LSB],
		lpg_config->base_addr, QPNP_PAUSE_LO_MULTIPLIER_LSB, 1, chip);
		if (rc)
			return rc;

		mask = QPNP_PAUSE_LO_MULTIPLIER_MSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_PAUSE_LO_MULTIPLIER_MSB],
		lpg_config->base_addr, QPNP_PAUSE_LO_MULTIPLIER_MSB, 1, chip);
		return rc;
	}

	return rc;
}

static int qpnp_lpg_configure_index(struct pwm_device *pwm)
{
	struct qpnp_lpg_config	*lpg_config = &pwm->chip->lpg_config;
	struct qpnp_lut_config	lut_config = lpg_config->lut_config;
	struct qpnp_lpg_chip	*chip = pwm->chip;
	u8			value, mask;
	int			rc = 0;

	value = lut_config.hi_index;
	mask = QPNP_HI_INDEX_MASK;

	rc = qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_HI_INDEX],
		lpg_config->base_addr, QPNP_HI_INDEX, 1, chip);
	if (rc)
		return rc;

	value = lut_config.lo_index;
	mask = QPNP_LO_INDEX_MASK;

	rc = qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_LO_INDEX],
		lpg_config->base_addr, QPNP_LO_INDEX, 1, chip);

	return rc;
}

static int qpnp_lpg_change_lut(struct pwm_device *pwm)
{
	int	rc;

	rc = qpnp_lpg_configure_pattern(pwm);
	if (rc) {
		pr_err("Failed to configure LUT pattern");
		return rc;
	}
	rc = qpnp_lpg_configure_pwm(pwm);
	if (rc) {
		pr_err("Failed to configure LUT pattern");
		return rc;
	}
	rc = qpnp_lpg_configure_control(pwm);
	if (rc) {
		pr_err("Failed to configure pause registers");
		return rc;
	}
	rc = qpnp_lpg_configure_ramp_step_duration(pwm);
	if (rc) {
		pr_err("Failed to configure duty time");
		return rc;
	}
	rc = qpnp_lpg_configure_pause(pwm);
	if (rc) {
		pr_err("Failed to configure pause registers");
		return rc;
	}
	rc = qpnp_lpg_configure_index(pwm);
	if (rc) {
		pr_err("Failed to configure index registers");
		return rc;
	}
	return rc;
}

static int qpnp_lpg_enable_lut(struct pwm_device *pwm)
{
	struct qpnp_lpg_config	*lpg_config = &pwm->chip->lpg_config;
	struct qpnp_lpg_chip	*chip = pwm->chip;
	u8			value, mask;

	value = pwm->chip->qpnp_lpg_registers[QPNP_RAMP_CONTROL];

	QPNP_ENABLE_LUT(value);

	mask = QPNP_RAMP_START_MASK;

	return qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_RAMP_CONTROL],
		lpg_config->base_addr, QPNP_RAMP_CONTROL, 1, chip);
}

static int qpnp_lpg_disable_lut(struct pwm_device *pwm)
{
	struct qpnp_lpg_config	*lpg_config = &pwm->chip->lpg_config;
	struct qpnp_lpg_chip	*chip = pwm->chip;
	u8			value, mask;

	value = pwm->chip->qpnp_lpg_registers[QPNP_RAMP_CONTROL];

	QPNP_DISABLE_LUT(value);

	mask = QPNP_RAMP_START_MASK;

	return qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_RAMP_CONTROL],
		lpg_config->base_addr, QPNP_RAMP_CONTROL, 1, chip);
}

static int qpnp_lpg_enable_pwm(struct pwm_device *pwm)
{
	struct qpnp_lpg_config	*lpg_config = &pwm->chip->lpg_config;
	struct qpnp_lpg_chip	*chip = pwm->chip;
	u8			value, mask;

	value = pwm->chip->qpnp_lpg_registers[QPNP_ENABLE_CONTROL];

	QPNP_ENABLE_PWM(value);

	mask = QPNP_EN_PWM_OUTPUT_MASK;

	return qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_ENABLE_CONTROL],
		lpg_config->base_addr, QPNP_RAMP_CONTROL, 1, chip);
}

static int qpnp_lpg_disable_pwm(struct pwm_device *pwm)
{
	struct qpnp_lpg_config	*lpg_config = &pwm->chip->lpg_config;
	struct qpnp_lpg_chip	*chip = pwm->chip;
	u8			value, mask;

	value = pwm->chip->qpnp_lpg_registers[QPNP_ENABLE_CONTROL];

	QPNP_DISABLE_PWM(value);

	mask = QPNP_EN_PWM_OUTPUT_MASK;

	return qpnp_lpg_save_and_write(value, mask,
		&pwm->chip->qpnp_lpg_registers[QPNP_ENABLE_CONTROL],
		lpg_config->base_addr, QPNP_RAMP_CONTROL, 1, chip);
}

static int _pwm_config(struct pwm_device *pwm, int duty_us, int period_us)
{
	struct qpnp_pwm_config		*pwm_config;
	struct qpnp_lpg_chip		*chip;
	struct pwm_period_config	*period;
	int				rc;

	chip = pwm->chip;
	pwm_config = &pwm->pwm_config;
	period = &pwm_config->period;

	if (pwm_config->pwm_period != period_us) {
		qpnp_lpg_calc_period(period_us, period);
		qpnp_lpg_save_period(pwm);
		pwm_config->pwm_period = period_us;
	}

	pwm_config->pwm_duty = duty_us;
	qpnp_lpg_calc_pwm_value(pwm, period_us, duty_us);
	rc = qpnp_lpg_save_pwm_value(pwm);

	if (rc) {
		pr_err("Could not update PWM value for channel %d rc=%d\n",
						pwm_config->channel_id, rc);
		return rc;
	}

	rc = qpnp_lpg_configure_pwm(pwm);
	if (rc) {
		pr_err("Could not configure PWM clock for\n");
		pr_err("channel %d rc=%d\n", pwm_config->channel_id, rc);
		return rc;
	}

	rc = qpnp_pwm_configure_control(pwm);
	if (rc) {
		pr_err("Could not update PWM control for");
		pr_err("channel %d rc=%d\n", pwm_config->channel_id, rc);
		return rc;
	}

	pwm->chip->lpg_config.lpg_configured = 1;

	pr_debug("duty/period=%u/%u usec: pwm_value=%d (of %d)\n",
		 (unsigned)duty_us, (unsigned)period_us,
		 pwm_config->pwm_value, 1 << period->pwm_size);

	return 0;
}

static int _pwm_lut_config(struct pwm_device *pwm, int period_us,
			int duty_pct[], struct lut_params lut_params)
{
	struct qpnp_lpg_config		*lpg_config;
	struct qpnp_lut_config		*lut_config;
	struct qpnp_lut_default_config  *def_lut_config =
					&lut_config->def_config;
	struct pwm_period_config	*period;
	struct qpnp_pwm_config		*pwm_config;
	int				start_idx = lut_params.start_idx;
	int				len = lut_params.idx_len;
	int				flags = lut_params.flags;
	int				raw_lut, ramp_step_ms;
	int				rc = 0;

	pwm_config = &pwm->pwm_config;
	lpg_config = &pwm->chip->lpg_config;
	lut_config = &lpg_config->lut_config;
	def_lut_config = &lut_config->def_config;

	if ((start_idx + len) > lpg_config->lut_size) {
		pr_err("Exceed LUT limit\n");
		return -EINVAL;
	}
	if ((unsigned)period_us > PM_PWM_PERIOD_MAX ||
		(unsigned)period_us < PM_PWM_PERIOD_MIN) {
		pr_err("Period out of range\n");
		return -EINVAL;
	}

	if (!pwm_config->in_use) {
		pr_err("channel_id: %d: stale handle?\n",
				pwm_config->channel_id);
		return -EINVAL;
	}

	period = &pwm_config->period;

	if (pwm_config->pwm_period != period_us) {
		qpnp_lpg_calc_period(period_us, period);
		qpnp_lpg_save_period(pwm);
		pwm_config->pwm_period = period_us;
	}

	if (flags & PM_PWM_LUT_NO_TABLE)
		goto after_table_write;

	raw_lut = 0;
	if (flags & PM_PWM_LUT_USE_RAW_VALUE)
		raw_lut = 1;

	lut_config->list_size = len;
	lut_config->lo_index = start_idx;
	lut_config->hi_index = start_idx + len - 1;

	/*
	 * LUT may not be specified in device tree by default.
	 * This is the first time user is configuring it.
	 */
	if (lpg_config->bypass_lut) {
		def_lut_config->duty_pct_list = kzalloc(sizeof(u32) *
							len, GFP_KERNEL);
		if (!def_lut_config->duty_pct_list) {
			pr_err("kzalloc failed on def_duty_pct_list\n");
			return -ENOMEM;
		}

		lut_config->duty_pct_list = kzalloc(lpg_config->lut_size *
						sizeof(u16), GFP_KERNEL);
		if (!lut_config->duty_pct_list) {
			pr_err("kzalloc failed on duty_pct_list\n");
			kfree(def_lut_config->duty_pct_list);
			return -ENOMEM;
		}

		def_lut_config->size = len;
		def_lut_config->start_idx = start_idx;
		memcpy(def_lut_config->duty_pct_list, duty_pct, len);

		lpg_config->bypass_lut = 0;
	}

	rc = qpnp_lpg_change_table(pwm, duty_pct, raw_lut);
	if (rc) {
		pr_err("qpnp_lpg_change_table: rc=%d\n", rc);
		return -EINVAL;
	}

after_table_write:
	ramp_step_ms = lut_params.ramp_step_ms;

	if (ramp_step_ms > PM_PWM_LUT_RAMP_STEP_TIME_MAX)
		ramp_step_ms = PM_PWM_LUT_RAMP_STEP_TIME_MAX;

	QPNP_SET_PAUSE_CNT(lut_config->lut_pause_lo_cnt,
			lut_params.lut_pause_lo, ramp_step_ms);
	if (lut_config->lut_pause_lo_cnt > PM_PWM_LUT_PAUSE_MAX)
		lut_config->lut_pause_lo_cnt = PM_PWM_LUT_PAUSE_MAX;

	QPNP_SET_PAUSE_CNT(lut_config->lut_pause_hi_cnt,
			lut_params.lut_pause_hi, ramp_step_ms);
	if (lut_config->lut_pause_hi_cnt > PM_PWM_LUT_PAUSE_MAX)
			lut_config->lut_pause_hi_cnt = PM_PWM_LUT_PAUSE_MAX;

	lut_config->ramp_step_ms = ramp_step_ms;

	lut_config->ramp_direction  = !!(flags & PM_PWM_LUT_RAMP_UP);
	lut_config->pattern_repeat  = !!(flags & PM_PWM_LUT_LOOP);
	lut_config->ramp_toggle	    = !!(flags & PM_PWM_LUT_REVERSE);
	lut_config->enable_pause_hi = !!(flags & PM_PWM_LUT_PAUSE_HI_EN);
	lut_config->enable_pause_lo = !!(flags & PM_PWM_LUT_PAUSE_LO_EN);
	lpg_config->bypass_lut = 0;

	rc = qpnp_lpg_change_lut(pwm);

	if (!rc)
		lpg_config->lpg_configured = 1;

	return rc;
}

/* APIs */
/**
 * pwm_request - request a PWM device
 * @channel_id: PWM id or channel
 * @lable: the label to identify the user
 */
struct pwm_device *pwm_request(int pwm_id, const char *lable)
{
	struct qpnp_lpg_chip	*chip;
	struct pwm_device	*pwm;

	chip = radix_tree_lookup(&lpg_dev_tree, pwm_id);

	if (!chip) {
		pr_err("Could not find PWM Device for the\n");
		pr_err("input pwm channel %d\n", pwm_id);
		return ERR_PTR(-EINVAL);
	}

	mutex_lock(&chip->lpg_mutex);

	pwm = &chip->pwm_dev;

	if (pwm->pwm_config.in_use) {
		pr_err("PWM device associated with the");
		pr_err("input pwm id: %d is in use by %s",
			pwm_id, pwm->pwm_config.lable);
		pwm = ERR_PTR(-EBUSY);
	} else {
		pwm->pwm_config.in_use = 1;
		pwm->pwm_config.lable  = lable;
	}

	mutex_unlock(&chip->lpg_mutex);

	return pwm;
}
EXPORT_SYMBOL_GPL(pwm_request);

/**
 * pwm_free - free a PWM device
 * @pwm: the PWM device
 */
void pwm_free(struct pwm_device *pwm)
{
	struct qpnp_pwm_config	*pwm_config;

	if (pwm == NULL || IS_ERR(pwm) || pwm->chip == NULL) {
		pr_err("Invalid pwm handle or no pwm_chip\n");
		return;
	}

	mutex_lock(&pwm->chip->lpg_mutex);

	pwm_config = &pwm->pwm_config;

	if (pwm_config->in_use) {
		qpnp_lpg_disable_pwm(pwm);
		qpnp_lpg_disable_lut(pwm);
		pwm_config->in_use = 0;
		pwm_config->lable = NULL;
		pwm->chip->lpg_config.lpg_configured = 0;
	}

	mutex_unlock(&pwm->chip->lpg_mutex);
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
	int rc;

	if (pwm == NULL || IS_ERR(pwm) ||
		duty_us > period_us ||
		(unsigned)period_us > PM_PWM_PERIOD_MAX ||
		(unsigned)period_us < PM_PWM_PERIOD_MIN) {
		pr_err("Invalid pwm handle or parameters\n");
		return -EINVAL;
	}

	if (!pwm->pwm_config.in_use)
		return -EINVAL;

	mutex_lock(&pwm->chip->lpg_mutex);
	rc = _pwm_config(pwm, duty_us, period_us);
	mutex_unlock(&pwm->chip->lpg_mutex);

	return rc;
}
EXPORT_SYMBOL_GPL(pwm_config);

/**
 * pwm_enable - start a PWM output toggling
 * @pwm: the PWM device
 */
int pwm_enable(struct pwm_device *pwm)
{
	struct qpnp_pwm_config	*p_config;
	struct qpnp_lpg_chip	*chip;
	int			rc = 0;

	if (pwm == NULL || IS_ERR(pwm) || pwm->chip == NULL) {
		pr_err("Invalid pwm handle or no pwm_chip\n");
		return -EINVAL;
	}

	mutex_lock(&pwm->chip->lpg_mutex);

	chip = pwm->chip;
	p_config = &pwm->pwm_config;

	if (!p_config->in_use) {
		pr_err("channel_id: %d: stale handle?\n", p_config->channel_id);
		rc = -EINVAL;
		goto out_unlock;
	}

	if (!pwm->chip->lpg_config.lpg_configured) {
		pr_err("Request received to enable PWM for channel Id: %d\n",
							p_config->channel_id);
		pr_err("However, PWM isn't configured\n");
		pr_err("falling back to defaultconfiguration\n");
		rc = _pwm_config(pwm, p_config->pwm_duty,
					p_config->pwm_period);
		if (rc) {
			pr_err("Could not apply default PWM config\n");
			goto out_unlock;
		}
	}

	rc = qpnp_lpg_enable_pwm(pwm);

out_unlock:
	mutex_unlock(&pwm->chip->lpg_mutex);
	return rc;
}
EXPORT_SYMBOL_GPL(pwm_enable);

/**
 * pwm_disable - stop a PWM output toggling
 * @pwm: the PWM device
 */
void pwm_disable(struct pwm_device *pwm)
{
	struct qpnp_pwm_config	*pwm_config;
	struct qpnp_lpg_chip	*chip;

	if (pwm == NULL || IS_ERR(pwm) || pwm->chip == NULL) {
		pr_err("Invalid pwm handle or no pwm_chip\n");
		return;
	}

	mutex_lock(&pwm->chip->lpg_mutex);

	chip = pwm->chip;
	pwm_config = &pwm->pwm_config;

	if (pwm_config->in_use) {
		if (!pwm->chip->lpg_config.lpg_configured) {
			pr_err("Request received to disable PWM for\n");
			pr_err("channel Id: %d\n", pwm_config->channel_id);
			pr_err("However PWM is not configured by any means\n");
			goto out_unlock;
		}
		qpnp_lpg_disable_pwm(pwm);
	}

out_unlock:
	mutex_unlock(&pwm->chip->lpg_mutex);
}
EXPORT_SYMBOL_GPL(pwm_disable);

/**
 * pwm_config_period - change PWM period
 *
 * @pwm: the PWM device
 * @pwm_p: period in struct qpnp_lpg_period
 */
int pwm_config_period(struct pwm_device *pwm,
			     struct pwm_period_config *period)
{
	struct qpnp_pwm_config	*pwm_config;
	struct qpnp_lpg_config	*lpg_config;
	struct qpnp_lpg_chip	*chip;
	int			rc = 0;

	if (pwm == NULL || IS_ERR(pwm) || period == NULL)
		return -EINVAL;
	if (pwm->chip == NULL)
		return -ENODEV;

	mutex_lock(&pwm->chip->lpg_mutex);

	chip = pwm->chip;
	pwm_config = &pwm->pwm_config;
	lpg_config = &chip->lpg_config;

	if (!pwm_config->in_use) {
		rc = -EINVAL;
		goto out_unlock;
	}

	pwm_config->period.pwm_size = period->pwm_size;
	pwm_config->period.clk = period->clk;
	pwm_config->period.pre_div = period->pre_div;
	pwm_config->period.pre_div_exp = period->pre_div_exp;

	qpnp_lpg_save_period(pwm);

	rc = spmi_ext_register_writel(chip->spmi_dev->ctrl, chip->spmi_dev->sid,
			SPMI_LPG_REG_ADDR(lpg_config->base_addr,
			QPNP_LPG_PWM_SIZE_CLK),
			&chip->qpnp_lpg_registers[QPNP_LPG_PWM_SIZE_CLK], 1);

	if (rc) {
		pr_err("Write failed: QPNP_LPG_PWM_SIZE_CLK register, rc: %d\n",
									rc);
		goto out_unlock;
	}

	rc = spmi_ext_register_writel(chip->spmi_dev->ctrl, chip->spmi_dev->sid,
			SPMI_LPG_REG_ADDR(lpg_config->base_addr,
			QPNP_LPG_PWM_FREQ_PREDIV_CLK),
		&chip->qpnp_lpg_registers[QPNP_LPG_PWM_FREQ_PREDIV_CLK], 1);
	if (rc) {
		pr_err("Failed to write to QPNP_LPG_PWM_FREQ_PREDIV_CLK\n");
		pr_err("register, rc = %d\n", rc);
	}

out_unlock:
	mutex_unlock(&pwm->chip->lpg_mutex);
	return rc;
}
EXPORT_SYMBOL(pwm_config_period);

/**
 * pwm_config_pwm_value - change a PWM device configuration
 * @pwm: the PWM device
 * @pwm_value: the duty cycle in raw PWM value (< 2^pwm_size)
 */
int pwm_config_pwm_value(struct pwm_device *pwm, int pwm_value)
{
	struct qpnp_lpg_config	*lpg_config;
	struct qpnp_pwm_config	*pwm_config;
	int			rc = 0;

	if (pwm == NULL || IS_ERR(pwm))
		return -EINVAL;

	if (pwm->chip == NULL)
		return -ENODEV;

	lpg_config = &pwm->chip->lpg_config;
	pwm_config = &pwm->pwm_config;

	mutex_lock(&pwm->chip->lpg_mutex);

	if (!pwm_config->in_use || !pwm_config->pwm_period) {
		rc = -EINVAL;
		goto out_unlock;
	}

	if (pwm_config->pwm_value == pwm_value)
		goto out_unlock;

	pwm_config->pwm_value = pwm_value;

	rc = qpnp_lpg_save_pwm_value(pwm);

	if (rc)
		pr_err("Could not update PWM value for channel %d rc=%d\n",
						pwm_config->channel_id, rc);

out_unlock:
	mutex_unlock(&pwm->chip->lpg_mutex);
	return rc;
}
EXPORT_SYMBOL_GPL(pwm_config_pwm_value);

/**
 * pwm_lut_config - change LPG LUT device configuration
 * @pwm: the PWM device
 * @period_us: period in micro second
 * @duty_pct: array of duty cycles in percent, like 20, 50.
 * @lut_params: Lookup table parameters
 */
int pwm_lut_config(struct pwm_device *pwm, int period_us,
		int duty_pct[], struct lut_params lut_params)
{
	int rc = 0;

	if (pwm == NULL || IS_ERR(pwm) || !lut_params.idx_len) {
		pr_err("Invalid pwm handle or idx_len=0\n");
		return -EINVAL;
	}

	if (pwm->chip == NULL)
		return -ENODEV;

	if (duty_pct == NULL && !(lut_params.flags & PM_PWM_LUT_NO_TABLE)) {
		pr_err("Invalid duty_pct with flag\n");
		return -EINVAL;
	}

	mutex_lock(&pwm->chip->lpg_mutex);

	rc = _pwm_lut_config(pwm, period_us, duty_pct, lut_params);

	mutex_unlock(&pwm->chip->lpg_mutex);

	return rc;
}
EXPORT_SYMBOL_GPL(pwm_lut_config);

/**
 * pwm_lut_enable - control a PWM device to start/stop LUT ramp
 * @pwm: the PWM device
 * @start: to start (1), or stop (0)
 */
int pwm_lut_enable(struct pwm_device *pwm, int start)
{
	struct qpnp_lpg_config	*lpg_config;
	struct qpnp_pwm_config	*p_config;
	struct lut_params	lut_params;
	int			rc = 0;

	if (pwm == NULL || IS_ERR(pwm)) {
		pr_err("Invalid pwm handle\n");
		return -EINVAL;
	}

	if (pwm->chip == NULL)
		return -ENODEV;

	lpg_config = &pwm->chip->lpg_config;
	p_config = &pwm->pwm_config;

	mutex_lock(&pwm->chip->lpg_mutex);

	if (start) {
		if (!lpg_config->lpg_configured) {
			pr_err("Request received to enable LUT for\n");
			pr_err("LPG channel %d\n", pwm->pwm_config.channel_id);
			pr_err("But LPG is not configured, falling back to\n");
			pr_err(" default LUT configuration if available\n");

			if (lpg_config->bypass_lut) {
				pr_err("No default LUT configuration found\n");
				pr_err("Use pwm_lut_config() to configure\n");
				rc = -EINVAL;
				goto out;
			}

			qpnp_set_lut_params(&lut_params,
					&lpg_config->lut_config);

			rc = _pwm_lut_config(pwm, p_config->pwm_period,
			(int *)lpg_config->lut_config.def_config.duty_pct_list,
			lut_params);
			if (rc) {
				pr_err("Could not set the default LUT conf\n");
				goto out;
			}
		}

		rc = qpnp_lpg_enable_lut(pwm);
	} else {
		if (unlikely(!lpg_config->lpg_configured)) {
			pr_err("LPG isn't configured\n");
			rc = -EINVAL;
			goto out;
		}
		rc = qpnp_lpg_disable_lut(pwm);
	}

out:
	mutex_unlock(&pwm->chip->lpg_mutex);
	return rc;
}
EXPORT_SYMBOL_GPL(pwm_lut_enable);

/* Fill in lpg device elements based on values found in device tree. */
static int qpnp_lpg_get_dt_config(struct spmi_device *spmi,
					struct qpnp_lpg_chip *chip)
{
	int			rc;
	struct resource		*res;
	struct device_node	*of_node = spmi->dev.of_node;
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	struct pwm_device	*pwm_dev = &chip->pwm_dev;
	struct qpnp_lut_config	*lut_config = &chip->lpg_config.lut_config;
	struct qpnp_lut_default_config	*def_lut_config =
						&lut_config->def_config;

	res = spmi_get_resource(spmi, 0, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&spmi->dev, "%s: node is missing base address\n",
			__func__);
		return -EINVAL;
	}

	lpg_config->base_addr = res->start;

	res = spmi_get_resource(spmi, 0, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&spmi->dev, "%s: node is missing LUT base address\n",
								__func__);
		return -EINVAL;
	}

	lpg_config->lut_base_addr = res->start;
	/* Each entry of LUT is of 2 bytes */
	lpg_config->lut_size = resource_size(res) >> 1;


	rc = of_property_read_u32(of_node, "qcom,channel-id",
				&pwm_dev->pwm_config.channel_id);
	if (rc) {
		dev_err(&spmi->dev, "%s: node is missing LPG channel id",
								__func__);
		return rc;
	}

	rc = of_property_read_u32(of_node, "qcom,period",
				&pwm_dev->pwm_config.pwm_period);
	if (rc) {
		dev_err(&spmi->dev, "%s: node is missing PWM Period value",
								__func__);
		return rc;
	}

	if (!of_get_property(of_node, "qcom,duty-percents",
						&def_lut_config->size)) {
		lpg_config->bypass_lut = 1;
	}

	if (lpg_config->bypass_lut)
		goto read_opt_props;

	rc = of_property_read_u32(of_node, "qcom,start-index",
					&def_lut_config->start_idx);

	if (rc) {
		dev_err(&spmi->dev, "Missing start index");
		return rc;
	}

	def_lut_config->size /= sizeof(u32);

	def_lut_config->duty_pct_list = kzalloc(sizeof(u32) *
					def_lut_config->size, GFP_KERNEL);
	if (!def_lut_config->duty_pct_list) {
		dev_err(&spmi->dev, "%s: kzalloc failed on duty_pct_list\n",
								__func__);
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(of_node, "qcom,duty-percents",
		def_lut_config->duty_pct_list, def_lut_config->size);
	if (rc) {
		dev_err(&spmi->dev, "invalid or missing property:\n");
		dev_err(&spmi->dev, "qcom,duty-pcts-list\n");
		kfree(def_lut_config->duty_pct_list);
		return rc;
	}

	lut_config->duty_pct_list = kzalloc(lpg_config->lut_size * sizeof(u16),
								GFP_KERNEL);
	if (!lut_config->duty_pct_list) {
		dev_err(&spmi->dev, "can not allocate duty pct list\n");
		kfree(def_lut_config->duty_pct_list);
		return -ENOMEM;
	}

read_opt_props:
	/* Initialize optional config parameters from DT if provided */
	of_property_read_u32(of_node, "qcom,duty",
					&pwm_dev->pwm_config.pwm_duty);
	of_property_read_u32(of_node, "qcom,ramp-step-duration",
					&lut_config->ramp_step_ms);
	of_property_read_u32(of_node, "qcom,lpg-lut-pause-hi",
					&lut_config->lut_pause_hi_cnt);
	of_property_read_u32(of_node, "qcom,lpg-lut-pause-lo",
					&lut_config->lut_pause_lo_cnt);
	of_property_read_u32(of_node, "qcom,lpg-lut-ramp-direction",
					(u32 *)&lut_config->ramp_direction);
	of_property_read_u32(of_node, "qcom,lpg-lut-pattern-repeat",
					(u32 *)&lut_config->pattern_repeat);
	of_property_read_u32(of_node, "qcom,lpg-lut-ramp-toggle",
					(u32 *)&lut_config->ramp_toggle);
	of_property_read_u32(of_node, "qcom,lpg-lut-enable-pause-hi",
					(u32 *)&lut_config->enable_pause_hi);
	of_property_read_u32(of_node, "qcom,lpg-lut-enable-pause-lo",
					(u32 *)&lut_config->enable_pause_lo);

	return 0;
}

static int __devinit qpnp_pwm_probe(struct spmi_device *spmi)
{
	struct qpnp_lpg_chip	*chip;
	int			rc, id;

	chip = kzalloc(sizeof *chip, GFP_KERNEL);
	if (chip == NULL) {
		pr_err("kzalloc() failed.\n");
		return -ENOMEM;
	}

	mutex_init(&chip->lpg_mutex);

	chip->spmi_dev = spmi;
	chip->pwm_dev.chip = chip;
	dev_set_drvdata(&spmi->dev, chip);

	rc = qpnp_lpg_get_dt_config(spmi, chip);

	if (rc)
		goto failed_config;

	id = chip->pwm_dev.pwm_config.channel_id;

	rc = radix_tree_insert(&lpg_dev_tree, id, chip);

	if (rc) {
		dev_err(&spmi->dev, "%s: Failed to register LPG Channel %d\n",
								__func__, id);
		goto failed_insert;
	}

	return 0;

failed_insert:
	kfree(chip->lpg_config.lut_config.duty_pct_list);
failed_config:
	dev_set_drvdata(&spmi->dev, NULL);
	mutex_destroy(&chip->lpg_mutex);
	kfree(chip);
	return rc;
}

static int __devexit qpnp_pwm_remove(struct spmi_device *spmi)
{
	struct qpnp_lpg_chip *chip;
	struct qpnp_lpg_config *lpg_config;

	chip = dev_get_drvdata(&spmi->dev);

	dev_set_drvdata(&spmi->dev, NULL);

	if (chip) {
		lpg_config = &chip->lpg_config;
		kfree(lpg_config->lut_config.duty_pct_list);
		kfree(lpg_config->lut_config.def_config.duty_pct_list);
		mutex_destroy(&chip->lpg_mutex);
		kfree(chip);
	}

	return 0;
}

static struct of_device_id spmi_match_table[] = {
	{ .compatible = QPNP_LPG_DRIVER_NAME, },
	{}
};

static const struct spmi_device_id qpnp_lpg_id[] = {
	{ QPNP_LPG_DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spmi, qpnp_lpg_id);

static struct spmi_driver qpnp_lpg_driver = {
	.driver		= {
		.name	= QPNP_LPG_DRIVER_NAME,
		.of_match_table = spmi_match_table,
		.owner = THIS_MODULE,
	},
	.probe		= qpnp_pwm_probe,
	.remove		= __devexit_p(qpnp_pwm_remove),
	.id_table	= qpnp_lpg_id,
};

/**
 * qpnp_lpg_init() - register spmi driver for qpnp-lpg
 */
int __init qpnp_lpg_init(void)
{
	return spmi_driver_register(&qpnp_lpg_driver);
}

static void __exit qpnp_lpg_exit(void)
{
	spmi_driver_unregister(&qpnp_lpg_driver);
}

MODULE_DESCRIPTION("QPNP PMIC LPG driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" QPNP_LPG_DRIVER_NAME);

subsys_initcall(qpnp_lpg_init);
module_exit(qpnp_lpg_exit);
