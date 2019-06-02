/* Copyright (c) 2012-2017, 2019 The Linux Foundation. All rights reserved.
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
 * Qualcomm Technologies, Inc. QPNP Pulse Width Modulation (PWM) driver
 *
 * The HW module is also called LPG (Light Pattern Generator).
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/radix-tree.h>
#include <linux/qpnp/pwm.h>

#define QPNP_LPG_DRIVER_NAME	"qcom,qpnp-pwm"
#define QPNP_LPG_CHANNEL_BASE	"qpnp-lpg-channel-base"
#define QPNP_LPG_LUT_BASE	"qpnp-lpg-lut-base"

#define QPNP_PWM_MODE_ONLY_SUB_TYPE	0x0B
#define QPNP_LPG_CHAN_SUB_TYPE		0x2
#define QPNP_LPG_S_CHAN_SUB_TYPE	0x11

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
#define QPNP_PWM_SIZE_SHIFT_SUB_TYPE		2
#define QPNP_PWM_SIZE_MASK_SUB_TYPE		0x4
#define QPNP_PWM_FREQ_CLK_SELECT_MASK_SUB_TYPE	0x03
#define QPNP_PWM_SIZE_9_BIT_SUB_TYPE		0x01

#define QPNP_SET_PWM_CLK_SUB_TYPE(val, clk, pwm_size) \
do { \
	val = (clk + 1) & QPNP_PWM_FREQ_CLK_SELECT_MASK_SUB_TYPE; \
	val |= (((pwm_size > 6 ? QPNP_PWM_SIZE_9_BIT_SUB_TYPE : 0) << \
		QPNP_PWM_SIZE_SHIFT_SUB_TYPE) & QPNP_PWM_SIZE_MASK_SUB_TYPE); \
} while (0)

#define QPNP_GET_PWM_SIZE_SUB_TYPE(reg) ((reg & QPNP_PWM_SIZE_MASK_SUB_TYPE) \
				>> QPNP_PWM_SIZE_SHIFT_SUB_TYPE)

#define QPNP_PWM_SIZE_SHIFT			4
#define QPNP_PWM_SIZE_MASK			0x30
#define QPNP_PWM_FREQ_CLK_SELECT_MASK		0x03
#define QPNP_MIN_PWM_BIT_SIZE		6
#define QPNP_MAX_PWM_BIT_SIZE		9
#define QPNP_PWM_SIZES_SUPPORTED	10

#define QPNP_SET_PWM_CLK(val, clk, pwm_size) \
do { \
	val = (clk + 1) & QPNP_PWM_FREQ_CLK_SELECT_MASK; \
	val |= (((pwm_size - QPNP_MIN_PWM_BIT_SIZE) << \
		QPNP_PWM_SIZE_SHIFT) & QPNP_PWM_SIZE_MASK); \
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
	val |= (pre_div_exp & QPNP_PWM_FREQ_EXP_MASK);	\
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

/* LPG Control for PWM_SYNC */
#define QPNP_PWM_SYNC_VALUE			0x01
#define QPNP_PWM_SYNC_MASK			0x01

/* LPG Control for RAMP_CONTROL */
#define QPNP_RAMP_START_MASK			0x01

#define QPNP_ENABLE_LUT_V0(value) (value |= QPNP_RAMP_START_MASK)
#define QPNP_DISABLE_LUT_V0(value) (value &= ~QPNP_RAMP_START_MASK)
#define QPNP_ENABLE_LUT_V1(value, id) (value |= BIT(id))

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

/* LPG DTEST */
#define QPNP_LPG_DTEST_LINE_MAX			4
#define QPNP_LPG_DTEST_OUTPUT_MAX		5
#define QPNP_LPG_DTEST_OUTPUT_MASK		0x07

/* PWM DTEST */
#define QPNP_PWM_DTEST_LINE_MAX			2
#define QPNP_PWM_DTEST_OUTPUT_MAX		2
#define QPNP_PWM_DTEST_OUTPUT_MASK		0x03

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

#define SPMI_LPG_REG_BASE_OFFSET	0x40
#define SPMI_LPG_REVISION2_OFFSET	0x1
#define SPMI_LPG_REV1_RAMP_CONTROL_OFFSET	0x86
#define SPMI_LPG_SUB_TYPE_OFFSET	0x5
#define SPMI_LPG_PWM_SYNC		0x7
#define SPMI_LPG_REG_ADDR(b, n)	(b + SPMI_LPG_REG_BASE_OFFSET + (n))
#define SPMI_MAX_BUF_LEN	8

#define QPNP_PWM_LUT_NOT_SUPPORTED	0x1

/* Supported PWM sizes */
#define QPNP_PWM_SIZE_6_BIT		6
#define QPNP_PWM_SIZE_7_BIT		7
#define QPNP_PWM_SIZE_8_BIT		8
#define QPNP_PWM_SIZE_9_BIT		9

#define QPNP_PWM_SIZE_6_9_BIT		0x9
#define QPNP_PWM_SIZE_7_8_BIT		0x6
#define QPNP_PWM_SIZE_6_7_9_BIT		0xB

/*
 * Registers that don't need to be cached are defined below from an offset
 * of SPMI_LPG_REG_BASE_OFFSET.
 */
#define QPNP_LPG_SEC_ACCESS		0x90
#define QPNP_LPG_DTEST			0xA2

/* Supported time levels */
enum time_level {
	LVL_NSEC,
	LVL_USEC,
};

/* LPG revisions */
enum qpnp_lpg_revision {
	QPNP_LPG_REVISION_0 = 0x0,
	QPNP_LPG_REVISION_1 = 0x1,
};

/* LPG LUT MODE STATE */
enum qpnp_lut_state {
	QPNP_LUT_ENABLE = 0x0,
	QPNP_LUT_DISABLE = 0x1,
};

/* PWM MODE STATE */
enum qpnp_pwm_state {
	QPNP_PWM_ENABLE = 0x0,
	QPNP_PWM_DISABLE = 0x1,
};

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

struct qpnp_lut_config {
	u8	*duty_pct_list;
	int	list_len;
	int	ramp_index;
	int	lo_index;
	int	hi_index;
	int	lut_pause_hi_cnt;
	int	lut_pause_lo_cnt;
	int	ramp_step_ms;
	bool	ramp_direction;
	bool	pattern_repeat;
	bool	ramp_toggle;
	bool	enable_pause_hi;
	bool	enable_pause_lo;
};

struct qpnp_lpg_config {
	struct qpnp_lut_config	lut_config;
	u16			base_addr;
	u16			lut_base_addr;
	u16			lut_size;
};

struct _qpnp_pwm_config {
	int				pwm_value;
	int				pwm_period;	/* in microseconds */
	int				pwm_duty;	/* in microseconds */
	struct pwm_period_config	period;
	int				supported_sizes;
	int				force_pwm_size;
	bool				update_period;
};

/* Public facing structure */
struct qpnp_pwm_chip {
	struct platform_device	*pdev;
	struct regmap		*regmap;
	struct pwm_chip         chip;
	bool			enabled;
	struct _qpnp_pwm_config	pwm_config;
	struct	qpnp_lpg_config	lpg_config;
	enum pm_pwm_mode	pwm_mode;
	spinlock_t		lpg_lock;
	enum qpnp_lpg_revision	revision;
	u8			sub_type;
	u32			flags;
	u8	qpnp_lpg_registers[QPNP_TOTAL_LPG_SPMI_REGISTERS];
	int			channel_id;
	const char		*channel_owner;
	u32			dtest_line;
	u32			dtest_output;
	bool			in_test_mode;
};

/* Internal functions */
static inline struct qpnp_pwm_chip *qpnp_pwm_from_pwm_dev(
					struct pwm_device *pwm)
{
	return container_of(pwm->chip, struct qpnp_pwm_chip, chip);
}

static inline struct qpnp_pwm_chip *qpnp_pwm_from_pwm_chip(
					struct pwm_chip *chip)
{
	return container_of(chip, struct qpnp_pwm_chip, chip);
}

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

static int qpnp_set_control(struct qpnp_pwm_chip *chip, bool pwm_hi,
		bool pwm_lo, bool pwm_out, bool pwm_src, bool ramp_gen)
{
	int value;

	value = (ramp_gen << QPNP_PWM_EN_RAMP_GEN_SHIFT) |
		(pwm_src << QPNP_PWM_SRC_SELECT_SHIFT) |
		(pwm_lo << QPNP_EN_PWM_LO_SHIFT) |
		(pwm_hi << QPNP_EN_PWM_HIGH_SHIFT);
	if (chip->sub_type != QPNP_LPG_S_CHAN_SUB_TYPE)
		value |= (pwm_out << QPNP_EN_PWM_OUTPUT_SHIFT);
	return value;
}

#define QPNP_ENABLE_LUT_CONTROL(chip) \
	qpnp_set_control((chip), 0, 0, 0, 0, 1)
#define QPNP_ENABLE_PWM_CONTROL(chip) \
	qpnp_set_control((chip), 0, 0, 0, 1, 0)
#define QPNP_ENABLE_PWM_MODE(chip) \
	qpnp_set_control((chip), 1, 1, 1, 1, 0)
#define QPNP_ENABLE_PWM_MODE_GPLED_CHANNEL(chip) \
	qpnp_set_control((chip), 1, 1, 1, 1, 1)
#define QPNP_ENABLE_LPG_MODE(chip) \
	qpnp_set_control((chip), 1, 1, 1, 0, 1)
#define QPNP_DISABLE_PWM_MODE(chip) \
	qpnp_set_control((chip), 0, 0, 0, 1, 0)
#define QPNP_DISABLE_LPG_MODE(chip) \
	qpnp_set_control((chip), 0, 0, 0, 0, 1)
#define QPNP_IS_PWM_CONFIG_SELECTED(val) (val & QPNP_PWM_SRC_SELECT_MASK)

#define QPNP_ENABLE_PWM_MODE_ONLY_SUB_TYPE			0x80
#define QPNP_DISABLE_PWM_MODE_ONLY_SUB_TYPE			0x0
#define QPNP_PWM_MODE_ONLY_ENABLE_DISABLE_MASK_SUB_TYPE	0x80

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
		struct qpnp_lut_config *l_config, int s_idx, int size)
{
	l_params->start_idx = s_idx;
	l_params->idx_len = size;
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

static int qpnp_lpg_save_and_write(u8 value, u8 mask, u8 *reg, u16 addr,
				u16 size, struct qpnp_pwm_chip *chip)
{
	qpnp_lpg_save(reg, mask, value);

	return regmap_bulk_write(chip->regmap, addr, reg, size);
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
static void qpnp_lpg_calc_period(enum time_level tm_lvl,
				unsigned int period_value,
				struct qpnp_pwm_chip *chip)
{
	int		n, m, clk, div;
	int		best_m, best_div, best_clk;
	unsigned int	last_err, cur_err, min_err;
	unsigned int	tmp_p, period_n;
	int             supported_sizes = chip->pwm_config.supported_sizes;
	int		force_pwm_size = chip->pwm_config.force_pwm_size;
	struct pwm_period_config *period = &chip->pwm_config.period;

	/* PWM Period / N */
	if (supported_sizes == QPNP_PWM_SIZE_7_8_BIT)
		n = 7;
	else
		n = 6;

	if (tm_lvl == LVL_USEC) {
		if (period_value < ((unsigned int)(-1) / NSEC_PER_USEC)) {
			period_n = (period_value * NSEC_PER_USEC) >> n;
		} else {
			if (supported_sizes == QPNP_PWM_SIZE_7_8_BIT)
				n = 8;
			else
				n = 9;
			period_n = (period_value >> n) * NSEC_PER_USEC;
		}
	} else {
		period_n = period_value >> n;
	}

	if (force_pwm_size != 0) {
		if (n < force_pwm_size)
			period_n = period_n >> (force_pwm_size - n);
		else
			period_n = period_n << (n - force_pwm_size);
		n = force_pwm_size;
		pr_info("LPG channel '%d' pwm size is forced to=%d\n",
					chip->channel_id, n);
	}

	min_err = last_err = (unsigned int)(-1);
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

	/* Adapt to optimal pwm size, the higher the resolution the better */
	if (!force_pwm_size) {
		if (supported_sizes == QPNP_PWM_SIZE_7_8_BIT) {
			if (n == 7 && best_m >= 1) {
				n += 1;
				best_m -= 1;
			}
		} else if (n == 6) {
			if (best_m >= 3) {
				n += 3;
				best_m -= 3;
			} else if (best_m >= 1 && (
				chip->sub_type != QPNP_PWM_MODE_ONLY_SUB_TYPE &&
				chip->sub_type != QPNP_LPG_S_CHAN_SUB_TYPE)) {
				n += 1;
				best_m -= 1;
			}
		}
	}

	period->pwm_size = n;
	period->clk = best_clk;
	period->pre_div = best_div;
	period->pre_div_exp = best_m;
}

static void qpnp_lpg_calc_pwm_value(struct _qpnp_pwm_config *pwm_config,
				      unsigned int period_value,
				      unsigned int duty_value)
{
	unsigned int max_pwm_value, tmp;

	/* Figure out pwm_value with overflow handling */
	tmp = 1 << (sizeof(tmp) * 8 - pwm_config->period.pwm_size);
	if (duty_value < tmp) {
		tmp = duty_value << pwm_config->period.pwm_size;
		pwm_config->pwm_value = tmp / period_value;
	} else {
		tmp = period_value >> pwm_config->period.pwm_size;
		pwm_config->pwm_value = duty_value / tmp;
	}
	max_pwm_value = (1 << pwm_config->period.pwm_size) - 1;
	if (pwm_config->pwm_value > max_pwm_value)
		pwm_config->pwm_value = max_pwm_value;
	pr_debug("pwm_value: %d\n", pwm_config->pwm_value);
}

static int qpnp_lpg_change_table(struct qpnp_pwm_chip *chip,
					int duty_pct[], int raw_value)
{
	unsigned int		pwm_value, max_pwm_value;
	struct qpnp_lut_config	*lut = &chip->lpg_config.lut_config;
	int			i, pwm_size, rc = 0;
	int			burst_size = SPMI_MAX_BUF_LEN;
	int			list_len = lut->list_len << 1;
	int			offset = (lut->lo_index << 1) - 2;

	pwm_size = QPNP_GET_PWM_SIZE(
			chip->qpnp_lpg_registers[QPNP_LPG_PWM_SIZE_CLK]) +
				QPNP_MIN_PWM_BIT_SIZE;

	max_pwm_value = (1 << pwm_size) - 1;

	if (unlikely(lut->list_len != (lut->hi_index - lut->lo_index + 1))) {
		pr_err("LUT internal Data structure corruption detected\n");
		pr_err("LUT list size: %d\n", lut->list_len);
		pr_err("However, index size is: %d\n",
				(lut->hi_index - lut->lo_index + 1));
		return -EINVAL;
	}

	for (i = 0; i < lut->list_len; i++) {
		if (raw_value)
			pwm_value = duty_pct[i];
		else
			pwm_value = (duty_pct[i] << pwm_size) / 100;

		if (pwm_value > max_pwm_value)
			pwm_value = max_pwm_value;

		if (chip->pwm_config.supported_sizes == QPNP_PWM_SIZE_7_8_BIT) {
			lut->duty_pct_list[i] = pwm_value;
		} else {
			lut->duty_pct_list[i*2] = pwm_value;
			lut->duty_pct_list[(i*2)+1] = (pwm_value >>
			 QPNP_PWM_VALUE_MSB_SHIFT) & QPNP_PWM_VALUE_MSB_MASK;
		}
	}

	/*
	 * For the Keypad Backlight Lookup Table (KPDBL_LUT),
	 * offset is lo_index.
	 */
	if (chip->pwm_config.supported_sizes == QPNP_PWM_SIZE_7_8_BIT)
		offset = lut->lo_index;

	/* Write with max allowable burst mode, each entry is of two bytes */
	for (i = 0; i < list_len; i += burst_size) {
		if (i + burst_size >= list_len)
			burst_size = list_len - i;
		rc = regmap_bulk_write(chip->regmap,
			       chip->lpg_config.lut_base_addr + offset + i,
			       lut->duty_pct_list + i,
			       burst_size);
	}

	return rc;
}

static void qpnp_lpg_save_period(struct qpnp_pwm_chip *chip)
{
	u8 mask, val;
	struct _qpnp_pwm_config	*pwm_config = &chip->pwm_config;

	if (chip->sub_type == QPNP_PWM_MODE_ONLY_SUB_TYPE) {
		QPNP_SET_PWM_CLK_SUB_TYPE(val, pwm_config->period.clk,
				pwm_config->period.pwm_size);
		mask = QPNP_PWM_SIZE_MASK_SUB_TYPE |
				QPNP_PWM_FREQ_CLK_SELECT_MASK_SUB_TYPE;
	} else {
		QPNP_SET_PWM_CLK(val, pwm_config->period.clk,
				pwm_config->period.pwm_size);
		mask = QPNP_PWM_SIZE_MASK | QPNP_PWM_FREQ_CLK_SELECT_MASK;
	}

	qpnp_lpg_save(&chip->qpnp_lpg_registers[QPNP_LPG_PWM_SIZE_CLK],
							mask, val);

	QPNP_SET_PWM_FREQ_PREDIV(val, pwm_config->period.pre_div,
					pwm_config->period.pre_div_exp);

	mask = QPNP_PWM_FREQ_PRE_DIVIDE_MASK | QPNP_PWM_FREQ_EXP_MASK;

	qpnp_lpg_save(&chip->qpnp_lpg_registers[QPNP_LPG_PWM_FREQ_PREDIV_CLK],
								mask, val);
}

static int qpnp_lpg_save_pwm_value(struct qpnp_pwm_chip *chip)
{
	unsigned int		max_pwm_value;
	int			pwm_size;
	u8			mask, value;
	struct _qpnp_pwm_config	*pwm_config = &chip->pwm_config;
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	int rc;

	if (chip->sub_type == QPNP_PWM_MODE_ONLY_SUB_TYPE)
		pwm_size = QPNP_GET_PWM_SIZE_SUB_TYPE(
			chip->qpnp_lpg_registers[QPNP_LPG_PWM_SIZE_CLK]) ?
				QPNP_MAX_PWM_BIT_SIZE : QPNP_MIN_PWM_BIT_SIZE;
	else
		pwm_size = QPNP_GET_PWM_SIZE(
			chip->qpnp_lpg_registers[QPNP_LPG_PWM_SIZE_CLK]) +
				QPNP_MIN_PWM_BIT_SIZE;

	max_pwm_value = (1 << pwm_size) - 1;

	if (pwm_config->pwm_value > max_pwm_value)
		pwm_config->pwm_value = max_pwm_value;

	value = (pwm_config->pwm_value >> QPNP_PWM_VALUE_MSB_SHIFT) &
					QPNP_PWM_VALUE_MSB_MASK;

	mask = QPNP_PWM_VALUE_MSB_MASK;

	pr_debug("pwm_msb value:%d\n", value);
	rc = qpnp_lpg_save_and_write(value, mask,
			&chip->qpnp_lpg_registers[QPNP_PWM_VALUE_MSB],
			SPMI_LPG_REG_ADDR(lpg_config->base_addr,
			QPNP_PWM_VALUE_MSB), 1, chip);
	if (rc)
		return rc;

	value = pwm_config->pwm_value;
	mask = QPNP_PWM_VALUE_LSB_MASK;

	pr_debug("pwm_lsb value:%d\n", value & mask);
	rc = qpnp_lpg_save_and_write(value, mask,
			&chip->qpnp_lpg_registers[QPNP_PWM_VALUE_LSB],
			SPMI_LPG_REG_ADDR(lpg_config->base_addr,
			QPNP_PWM_VALUE_LSB), 1, chip);
	if (rc)
		return rc;

	if (chip->sub_type == QPNP_PWM_MODE_ONLY_SUB_TYPE ||
		chip->sub_type == QPNP_LPG_S_CHAN_SUB_TYPE) {
		value = QPNP_PWM_SYNC_VALUE & QPNP_PWM_SYNC_MASK;
		rc = regmap_write(chip->regmap,
					SPMI_LPG_REG_ADDR(lpg_config->base_addr,
						SPMI_LPG_PWM_SYNC),
					value);
	}

	return rc;
}

static int qpnp_lpg_configure_pattern(struct qpnp_pwm_chip *chip)
{
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	struct qpnp_lut_config	*lut_config = &lpg_config->lut_config;
	u8			value, mask;

	qpnp_set_pattern_config(&value, lut_config);

	mask = QPNP_RAMP_DIRECTION_MASK | QPNP_PATTERN_REPEAT_MASK |
			QPNP_RAMP_TOGGLE_MASK | QPNP_EN_PAUSE_HI_MASK |
			QPNP_EN_PAUSE_LO_MASK;

	return qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_LPG_PATTERN_CONFIG],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_LPG_PATTERN_CONFIG), 1, chip);
}

static int qpnp_lpg_glitch_removal(struct qpnp_pwm_chip *chip, bool enable)
{
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	u8 value, mask;

	qpnp_set_pwm_type_config(&value, enable ? 1 : 0, 0, 0, 0);

	mask = QPNP_EN_GLITCH_REMOVAL_MASK | QPNP_EN_FULL_SCALE_MASK |
			QPNP_EN_PHASE_STAGGER_MASK | QPNP_PHASE_STAGGER_MASK;

	pr_debug("pwm_type_config: %d\n", value);
	return qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_LPG_PWM_TYPE_CONFIG],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_LPG_PWM_TYPE_CONFIG), 1, chip);
}

static int qpnp_lpg_configure_pwm(struct qpnp_pwm_chip *chip)
{
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	int rc;

	pr_debug("pwm_size_clk: %d\n",
		chip->qpnp_lpg_registers[QPNP_LPG_PWM_SIZE_CLK]);
	rc = regmap_write(chip->regmap,
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_LPG_PWM_SIZE_CLK),
		*&chip->qpnp_lpg_registers[QPNP_LPG_PWM_SIZE_CLK]);

	if (rc)
		return rc;

	pr_debug("pwm_freq_prediv_clk: %d\n",
		chip->qpnp_lpg_registers[QPNP_LPG_PWM_FREQ_PREDIV_CLK]);
	rc = regmap_write(chip->regmap,
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_LPG_PWM_FREQ_PREDIV_CLK),
		*&chip->qpnp_lpg_registers[QPNP_LPG_PWM_FREQ_PREDIV_CLK]);
	if (rc)
		return rc;

	/* Disable glitch removal when LPG/PWM is configured */
	rc = qpnp_lpg_glitch_removal(chip, false);
	if (rc) {
		pr_err("Error in disabling glitch control, rc=%d\n", rc);
		return rc;
	}
	return rc;
}

static int qpnp_configure_pwm_control(struct qpnp_pwm_chip *chip)
{
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	u8			value, mask;

	if (chip->sub_type == QPNP_PWM_MODE_ONLY_SUB_TYPE)
		return 0;

	value = QPNP_ENABLE_PWM_CONTROL(chip);

	mask = QPNP_EN_PWM_HIGH_MASK | QPNP_EN_PWM_LO_MASK |
		QPNP_PWM_SRC_SELECT_MASK | QPNP_PWM_EN_RAMP_GEN_MASK;
	if (chip->sub_type != QPNP_LPG_S_CHAN_SUB_TYPE)
		mask |= QPNP_EN_PWM_OUTPUT_MASK;

	return qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_ENABLE_CONTROL],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_ENABLE_CONTROL), 1, chip);

}

static int qpnp_configure_lpg_control(struct qpnp_pwm_chip *chip)
{
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	u8			value, mask;

	value = QPNP_ENABLE_LUT_CONTROL(chip);

	mask = QPNP_EN_PWM_HIGH_MASK | QPNP_EN_PWM_LO_MASK |
		QPNP_PWM_SRC_SELECT_MASK | QPNP_PWM_EN_RAMP_GEN_MASK;
	if (chip->sub_type != QPNP_LPG_S_CHAN_SUB_TYPE)
		mask |= QPNP_EN_PWM_OUTPUT_MASK;

	return qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_ENABLE_CONTROL],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_ENABLE_CONTROL), 1, chip);

}

static int qpnp_lpg_configure_ramp_step_duration(struct qpnp_pwm_chip *chip)
{
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	struct qpnp_lut_config	lut_config = lpg_config->lut_config;
	int			rc, value;
	u8			val, mask;

	value = QPNP_GET_RAMP_STEP_DURATION(lut_config.ramp_step_ms);
	val = value & QPNP_RAMP_STEP_DURATION_LSB_MASK;
	mask = QPNP_RAMP_STEP_DURATION_LSB_MASK;

	rc = qpnp_lpg_save_and_write(val, mask,
		&chip->qpnp_lpg_registers[QPNP_RAMP_STEP_DURATION_LSB],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_RAMP_STEP_DURATION_LSB), 1, chip);
	if (rc)
		return rc;

	val = (value >> QPNP_RAMP_STEP_DURATION_MSB_SHIFT) &
				QPNP_RAMP_STEP_DURATION_MSB_MASK;

	mask = QPNP_RAMP_STEP_DURATION_MSB_MASK;

	return qpnp_lpg_save_and_write(val, mask,
		&chip->qpnp_lpg_registers[QPNP_RAMP_STEP_DURATION_MSB],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_RAMP_STEP_DURATION_MSB), 1, chip);
}

static int qpnp_lpg_configure_pause(struct qpnp_pwm_chip *chip)
{
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	struct qpnp_lut_config	lut_config = lpg_config->lut_config;
	u8			value, mask;
	int			rc = 0;

	if (lut_config.enable_pause_hi) {
		value = lut_config.lut_pause_hi_cnt;
		mask = QPNP_PAUSE_HI_MULTIPLIER_LSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_PAUSE_HI_MULTIPLIER_LSB],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_PAUSE_HI_MULTIPLIER_LSB), 1, chip);
		if (rc)
			return rc;

		value = (lut_config.lut_pause_hi_cnt >>
			QPNP_PAUSE_HI_MULTIPLIER_MSB_SHIFT) &
					QPNP_PAUSE_HI_MULTIPLIER_MSB_MASK;

		mask = QPNP_PAUSE_HI_MULTIPLIER_MSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_PAUSE_HI_MULTIPLIER_MSB],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_PAUSE_HI_MULTIPLIER_MSB), 1, chip);
	} else {
		value = 0;
		mask = QPNP_PAUSE_HI_MULTIPLIER_LSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_PAUSE_HI_MULTIPLIER_LSB],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_PAUSE_HI_MULTIPLIER_LSB), 1, chip);
		if (rc)
			return rc;

		mask = QPNP_PAUSE_HI_MULTIPLIER_MSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_PAUSE_HI_MULTIPLIER_MSB],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_PAUSE_HI_MULTIPLIER_MSB), 1, chip);
		if (rc)
			return rc;

	}

	if (lut_config.enable_pause_lo) {
		value = lut_config.lut_pause_lo_cnt;
		mask = QPNP_PAUSE_LO_MULTIPLIER_LSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_PAUSE_LO_MULTIPLIER_LSB],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_PAUSE_LO_MULTIPLIER_LSB), 1, chip);
		if (rc)
			return rc;

		value = (lut_config.lut_pause_lo_cnt >>
				QPNP_PAUSE_LO_MULTIPLIER_MSB_SHIFT) &
					QPNP_PAUSE_LO_MULTIPLIER_MSB_MASK;

		mask = QPNP_PAUSE_LO_MULTIPLIER_MSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_PAUSE_LO_MULTIPLIER_MSB],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_PAUSE_LO_MULTIPLIER_MSB), 1, chip);
	} else {
		value = 0;
		mask = QPNP_PAUSE_LO_MULTIPLIER_LSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_PAUSE_LO_MULTIPLIER_LSB],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_PAUSE_LO_MULTIPLIER_LSB), 1, chip);
		if (rc)
			return rc;

		mask = QPNP_PAUSE_LO_MULTIPLIER_MSB_MASK;

		rc = qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_PAUSE_LO_MULTIPLIER_MSB],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_PAUSE_LO_MULTIPLIER_MSB), 1, chip);
		return rc;
	}

	return rc;
}

static int qpnp_lpg_configure_index(struct qpnp_pwm_chip *chip)
{
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	struct qpnp_lut_config	lut_config = lpg_config->lut_config;
	u8			value, mask;
	int			rc = 0;

	value = lut_config.hi_index;
	mask = QPNP_HI_INDEX_MASK;

	rc = qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_HI_INDEX],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_HI_INDEX), 1, chip);
	if (rc)
		return rc;

	value = lut_config.lo_index;
	mask = QPNP_LO_INDEX_MASK;

	rc = qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_LO_INDEX],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_LO_INDEX), 1, chip);

	return rc;
}

static int qpnp_lpg_change_lut(struct qpnp_pwm_chip *chip)
{
	int	rc;

	rc = qpnp_lpg_configure_pattern(chip);
	if (rc) {
		pr_err("Failed to configure LUT pattern");
		return rc;
	}
	rc = qpnp_lpg_configure_pwm(chip);
	if (rc) {
		pr_err("Failed to configure LUT pattern");
		return rc;
	}
	rc = qpnp_configure_lpg_control(chip);
	if (rc) {
		pr_err("Failed to configure pause registers");
		return rc;
	}
	rc = qpnp_lpg_configure_ramp_step_duration(chip);
	if (rc) {
		pr_err("Failed to configure duty time");
		return rc;
	}
	rc = qpnp_lpg_configure_pause(chip);
	if (rc) {
		pr_err("Failed to configure pause registers");
		return rc;
	}
	rc = qpnp_lpg_configure_index(chip);
	if (rc) {
		pr_err("Failed to configure index registers");
		return rc;
	}
	return rc;
}

static int qpnp_dtest_config(struct qpnp_pwm_chip *chip, bool enable)
{
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	u8			value;
	u8			mask;
	u16			addr;
	int			rc = 0;

	value = 0xA5;

	addr = SPMI_LPG_REG_ADDR(lpg_config->base_addr, QPNP_LPG_SEC_ACCESS);

	rc = regmap_write(chip->regmap, addr, value);

	if (rc) {
		pr_err("Couldn't set the access for test mode\n");
		return rc;
	}

	addr = SPMI_LPG_REG_ADDR(lpg_config->base_addr,
			QPNP_LPG_DTEST + chip->dtest_line - 1);

	if (chip->sub_type == QPNP_PWM_MODE_ONLY_SUB_TYPE)
		mask = QPNP_PWM_DTEST_OUTPUT_MASK;
	else
		mask = QPNP_LPG_DTEST_OUTPUT_MASK;

	if (enable)
		value = chip->dtest_output & mask;
	else
		value = 0;

	pr_debug("Setting TEST mode for channel %d addr:%x value: %x\n",
		chip->channel_id, addr, value);

	rc = regmap_write(chip->regmap, addr, value);

	return rc;
}

static int qpnp_lpg_configure_lut_state(struct qpnp_pwm_chip *chip,
				enum qpnp_lut_state state)
{
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	u8			value1, value2, mask1, mask2;
	u8			*reg1, *reg2;
	u16			addr, addr1;
	int			rc;
	bool			test_enable;

	value1 = chip->qpnp_lpg_registers[QPNP_RAMP_CONTROL];
	reg1 = &chip->qpnp_lpg_registers[QPNP_RAMP_CONTROL];
	reg2 = &chip->qpnp_lpg_registers[QPNP_ENABLE_CONTROL];
	mask2 = QPNP_EN_PWM_HIGH_MASK | QPNP_EN_PWM_LO_MASK |
		 QPNP_PWM_SRC_SELECT_MASK | QPNP_PWM_EN_RAMP_GEN_MASK;
	if (chip->sub_type != QPNP_LPG_S_CHAN_SUB_TYPE)
		mask2 |= QPNP_EN_PWM_OUTPUT_MASK;

	if (chip->sub_type == QPNP_LPG_CHAN_SUB_TYPE
		&& chip->revision == QPNP_LPG_REVISION_0) {
		if (state == QPNP_LUT_ENABLE) {
			QPNP_ENABLE_LUT_V0(value1);
			value2 = QPNP_ENABLE_LPG_MODE(chip);
		} else {
			QPNP_DISABLE_LUT_V0(value1);
			value2 = QPNP_DISABLE_LPG_MODE(chip);
		}
		mask1 = QPNP_RAMP_START_MASK;
		addr1 = SPMI_LPG_REG_ADDR(lpg_config->base_addr,
					QPNP_RAMP_CONTROL);
	} else if ((chip->sub_type == QPNP_LPG_CHAN_SUB_TYPE
			&& chip->revision == QPNP_LPG_REVISION_1)
			|| chip->sub_type == QPNP_LPG_S_CHAN_SUB_TYPE) {
		if (state == QPNP_LUT_ENABLE) {
			QPNP_ENABLE_LUT_V1(value1,
					lpg_config->lut_config.ramp_index);
			value2 = QPNP_ENABLE_LPG_MODE(chip);
		} else {
			value2 = QPNP_DISABLE_LPG_MODE(chip);
		}
		mask1 = value1;
		addr1 = lpg_config->lut_base_addr +
			SPMI_LPG_REV1_RAMP_CONTROL_OFFSET;
	} else {
		pr_err("Unsupported LPG subtype 0x%02x, revision 0x%02x\n",
			chip->sub_type, chip->revision);
		return -EINVAL;
	}

	addr = SPMI_LPG_REG_ADDR(lpg_config->base_addr,
				QPNP_ENABLE_CONTROL);

	if (chip->in_test_mode) {
		test_enable = (state == QPNP_LUT_ENABLE) ? 1 : 0;
		rc = qpnp_dtest_config(chip, test_enable);
		if (rc)
			pr_err("Failed to configure TEST mode\n");
	}

	rc = qpnp_lpg_save_and_write(value2, mask2, reg2,
					addr, 1, chip);
	if (rc)
		return rc;

	if (state == QPNP_LUT_ENABLE
		|| (chip->sub_type == QPNP_LPG_CHAN_SUB_TYPE
		&& chip->revision == QPNP_LPG_REVISION_0))
		rc = qpnp_lpg_save_and_write(value1, mask1, reg1,
					addr1, 1, chip);
	return rc;
}

static inline int qpnp_enable_pwm_mode(struct qpnp_pwm_chip *chip)
{
	if (chip->pwm_config.supported_sizes == QPNP_PWM_SIZE_7_8_BIT)
		return QPNP_ENABLE_PWM_MODE_GPLED_CHANNEL(chip);
	return QPNP_ENABLE_PWM_MODE(chip);
}

static int qpnp_lpg_configure_pwm_state(struct qpnp_pwm_chip *chip,
					enum qpnp_pwm_state state)
{
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	u8			value, mask;
	int			rc;
	bool			test_enable;

	if (chip->sub_type == QPNP_PWM_MODE_ONLY_SUB_TYPE) {
		if (state == QPNP_PWM_ENABLE)
			value = QPNP_ENABLE_PWM_MODE_ONLY_SUB_TYPE;
		else
			value = QPNP_DISABLE_PWM_MODE_ONLY_SUB_TYPE;

		mask = QPNP_PWM_MODE_ONLY_ENABLE_DISABLE_MASK_SUB_TYPE;
	} else {
		if (state == QPNP_PWM_ENABLE)
			value = qpnp_enable_pwm_mode(chip);
		else
			value = QPNP_DISABLE_PWM_MODE(chip);

		mask = QPNP_EN_PWM_HIGH_MASK | QPNP_EN_PWM_LO_MASK |
			QPNP_PWM_SRC_SELECT_MASK | QPNP_PWM_EN_RAMP_GEN_MASK;
		if (chip->sub_type != QPNP_LPG_S_CHAN_SUB_TYPE)
			mask |= QPNP_EN_PWM_OUTPUT_MASK;
	}

	if (chip->in_test_mode) {
		test_enable = (state == QPNP_PWM_ENABLE) ? 1 : 0;
		rc = qpnp_dtest_config(chip, test_enable);
		if (rc)
			pr_err("Failed to configure TEST mode\n");
	}

	pr_debug("pwm_enable_control: %d\n", value);
	rc = qpnp_lpg_save_and_write(value, mask,
		&chip->qpnp_lpg_registers[QPNP_ENABLE_CONTROL],
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_ENABLE_CONTROL), 1, chip);
	if (rc)
		goto out;

	/*
	 * Due to LPG hardware bug, in the PWM mode, having enabled PWM,
	 * We have to write PWM values one more time.
	 */
	if (state == QPNP_PWM_ENABLE)
		return qpnp_lpg_save_pwm_value(chip);

out:
	return rc;
}

static int _pwm_config(struct qpnp_pwm_chip *chip,
				enum time_level tm_lvl,
				int duty_value, int period_value)
{
	int rc;
	struct _qpnp_pwm_config *pwm_config = &chip->pwm_config;
	struct pwm_period_config *period = &pwm_config->period;

	pwm_config->pwm_duty = (tm_lvl == LVL_USEC) ? duty_value :
			duty_value / NSEC_PER_USEC;
	qpnp_lpg_calc_pwm_value(pwm_config, period_value, duty_value);
	rc = qpnp_lpg_save_pwm_value(chip);
	if (rc)
		goto out;

	if (pwm_config->update_period) {
		rc = qpnp_lpg_configure_pwm(chip);
		if (rc)
			goto out;
		rc = qpnp_configure_pwm_control(chip);
		if (rc)
			goto out;
		if (!rc && chip->enabled) {
			rc = qpnp_lpg_configure_pwm_state(chip,
					QPNP_PWM_ENABLE);
			if (rc) {
				pr_err("Error in configuring pwm state, rc=%d\n",
						rc);
				return rc;
			}

			/* Enable the glitch removal after PWM is enabled */
			rc = qpnp_lpg_glitch_removal(chip, true);
			if (rc) {
				pr_err("Error in enabling glitch control, rc=%d\n",
						rc);
				return rc;
			}
		}
	}
	pr_debug("duty/period=%u/%u %s: pwm_value=%d (of %d)\n",
		 (unsigned int)duty_value, (unsigned int)period_value,
		 (tm_lvl == LVL_USEC) ? "usec" : "nsec",
		 pwm_config->pwm_value, 1 << period->pwm_size);

out:
	return rc;
}

static int _pwm_lut_config(struct qpnp_pwm_chip *chip, int period_us,
		int duty_pct[], struct lut_params lut_params)
{
	struct qpnp_lpg_config		*lpg_config;
	struct qpnp_lut_config		*lut_config;
	struct pwm_period_config	*period;
	struct _qpnp_pwm_config		*pwm_config;
	int				start_idx = lut_params.start_idx;
	int				len = lut_params.idx_len;
	int				flags = lut_params.flags;
	int				raw_lut, ramp_step_ms;
	int				rc = 0;

	pwm_config = &chip->pwm_config;
	lpg_config = &chip->lpg_config;
	lut_config = &lpg_config->lut_config;
	period = &pwm_config->period;

	if (flags & PM_PWM_LUT_NO_TABLE)
		goto after_table_write;

	raw_lut = 0;
	if (flags & PM_PWM_LUT_USE_RAW_VALUE)
		raw_lut = 1;

	lut_config->list_len = len;
	lut_config->lo_index = start_idx + 1;
	lut_config->hi_index = start_idx + len;

	rc = qpnp_lpg_change_table(chip, duty_pct, raw_lut);
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
	if (lut_config->lut_pause_lo_cnt > PM_PWM_MAX_PAUSE_CNT)
		lut_config->lut_pause_lo_cnt = PM_PWM_MAX_PAUSE_CNT;

	QPNP_SET_PAUSE_CNT(lut_config->lut_pause_hi_cnt,
			lut_params.lut_pause_hi, ramp_step_ms);
	if (lut_config->lut_pause_hi_cnt > PM_PWM_MAX_PAUSE_CNT)
		lut_config->lut_pause_hi_cnt = PM_PWM_MAX_PAUSE_CNT;

	lut_config->ramp_step_ms = ramp_step_ms;

	lut_config->ramp_direction  = !!(flags & PM_PWM_LUT_RAMP_UP);
	lut_config->pattern_repeat  = !!(flags & PM_PWM_LUT_LOOP);
	lut_config->ramp_toggle	    = !!(flags & PM_PWM_LUT_REVERSE);
	lut_config->enable_pause_hi = !!(flags & PM_PWM_LUT_PAUSE_HI_EN);
	lut_config->enable_pause_lo = !!(flags & PM_PWM_LUT_PAUSE_LO_EN);

	rc = qpnp_lpg_change_lut(chip);

	if (!rc && chip->enabled)
		rc = qpnp_lpg_configure_lut_state(chip, QPNP_LUT_ENABLE);

	return rc;
}

/* lpg_lock should be held while calling _pwm_enable() */
static int _pwm_enable(struct qpnp_pwm_chip *chip)
{
	int rc = 0;

	if (QPNP_IS_PWM_CONFIG_SELECTED(
		chip->qpnp_lpg_registers[QPNP_ENABLE_CONTROL]) ||
			chip->flags & QPNP_PWM_LUT_NOT_SUPPORTED) {
		rc = qpnp_lpg_configure_pwm_state(chip, QPNP_PWM_ENABLE);
		if (rc) {
			pr_err("Failed to enable PWM mode, rc=%d\n", rc);
			return rc;
		}
		rc = qpnp_lpg_glitch_removal(chip, true);
		if (rc) {
			pr_err("Failed to enable glitch removal, rc=%d\n", rc);
			return rc;
		}
	} else if (!(chip->flags & QPNP_PWM_LUT_NOT_SUPPORTED)) {
		rc = qpnp_lpg_configure_lut_state(chip, QPNP_LUT_ENABLE);
	}

	if (!rc)
		chip->enabled = true;

	return rc;
}

/* lpg_lock should be held while calling _pwm_change_mode() */
static int _pwm_change_mode(struct qpnp_pwm_chip *chip, enum pm_pwm_mode mode)
{
	int rc;

	if (mode == PM_PWM_MODE_LPG)
		rc = qpnp_configure_lpg_control(chip);
	else
		rc = qpnp_configure_pwm_control(chip);

	if (rc)
		pr_err("Failed to change the mode\n");
	return rc;
}

/* APIs */
/**
 * qpnp_pwm_free - free a PWM device
 * @pwm_chip: the PWM chip
 * @pwm: the PWM device
 */
static void qpnp_pwm_free(struct pwm_chip *pwm_chip,
		struct pwm_device *pwm)
{
	struct qpnp_pwm_chip	*chip = qpnp_pwm_from_pwm_chip(pwm_chip);
	unsigned long		flags;

	spin_lock_irqsave(&chip->lpg_lock, flags);

	qpnp_lpg_configure_pwm_state(chip, QPNP_PWM_DISABLE);
	if (!(chip->flags & QPNP_PWM_LUT_NOT_SUPPORTED))
		qpnp_lpg_configure_lut_state(chip, QPNP_LUT_DISABLE);

	chip->enabled = false;
	spin_unlock_irqrestore(&chip->lpg_lock, flags);
}

/**
 * qpnp_pwm_config - change a PWM device configuration
 * @pwm: the PWM device
 * @period_ns: period in nanoseconds
 * @duty_ns: duty cycle in nanoseconds
 */
static int qpnp_pwm_config(struct pwm_chip *pwm_chip,
	struct pwm_device *pwm, int duty_ns, int period_ns)
{
	int rc;
	unsigned long flags;
	struct qpnp_pwm_chip *chip = qpnp_pwm_from_pwm_chip(pwm_chip);
	int prev_period_us = chip->pwm_config.pwm_period;

	if ((unsigned int)period_ns < PM_PWM_PERIOD_MIN * NSEC_PER_USEC) {
		pr_err("Invalid pwm handle or parameters\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&chip->lpg_lock, flags);

	chip->pwm_config.update_period = false;
	if (prev_period_us > INT_MAX / NSEC_PER_USEC ||
			prev_period_us * NSEC_PER_USEC != period_ns) {
		qpnp_lpg_calc_period(LVL_NSEC, period_ns, chip);
		qpnp_lpg_save_period(chip);
		pwm->period = period_ns;
		chip->pwm_config.pwm_period = period_ns / NSEC_PER_USEC;
		chip->pwm_config.update_period = true;
	}

	rc = _pwm_config(chip, LVL_NSEC, duty_ns, period_ns);

	spin_unlock_irqrestore(&chip->lpg_lock, flags);

	if (rc)
		pr_err("Failed to configure PWM mode\n");

	return rc;
}

/**
 * qpnp_pwm_enable - start a PWM output toggling
 * @pwm_chip: the PWM chip
 * @pwm: the PWM device
 */
static int qpnp_pwm_enable(struct pwm_chip *pwm_chip,
		struct pwm_device *pwm)
{
	int rc;
	struct qpnp_pwm_chip *chip = qpnp_pwm_from_pwm_chip(pwm_chip);
	unsigned long flags;

	spin_lock_irqsave(&chip->lpg_lock, flags);
	rc = _pwm_enable(chip);
	if (rc)
		pr_err("Failed to enable PWM channel: %d\n", chip->channel_id);

	spin_unlock_irqrestore(&chip->lpg_lock, flags);

	return rc;
}

/**
 * qpnp_pwm_disable - stop a PWM output toggling
 * @pwm_chip: the PWM chip
 * @pwm: the PWM device
 */
static void qpnp_pwm_disable(struct pwm_chip *pwm_chip,
		struct pwm_device *pwm)
{

	struct qpnp_pwm_chip	*chip = qpnp_pwm_from_pwm_chip(pwm_chip);
	unsigned long		flags;
	int rc = 0;

	spin_lock_irqsave(&chip->lpg_lock, flags);

	if (QPNP_IS_PWM_CONFIG_SELECTED(
		chip->qpnp_lpg_registers[QPNP_ENABLE_CONTROL]) ||
			chip->flags & QPNP_PWM_LUT_NOT_SUPPORTED)
		rc = qpnp_lpg_configure_pwm_state(chip,
					QPNP_PWM_DISABLE);
	else if (!(chip->flags & QPNP_PWM_LUT_NOT_SUPPORTED))
		rc = qpnp_lpg_configure_lut_state(chip,
					QPNP_LUT_DISABLE);

	if (!rc)
		chip->enabled = false;

	spin_unlock_irqrestore(&chip->lpg_lock, flags);

	if (rc)
		pr_err("Failed to disable PWM channel: %d\n",
					chip->channel_id);
}

/**
 * pwm_change_mode - Change the PWM mode configuration
 * @pwm: the PWM device
 * @mode: Mode selection value
 */
int pwm_change_mode(struct pwm_device *pwm, enum pm_pwm_mode mode)
{
	int rc = 0;
	unsigned long flags;
	struct qpnp_pwm_chip *chip;

	if (pwm == NULL || IS_ERR(pwm) || pwm->chip == NULL) {
		pr_err("Invalid pwm handle or no pwm_chip\n");
		return -EINVAL;
	}

	if (mode < PM_PWM_MODE_PWM || mode > PM_PWM_MODE_LPG) {
		pr_err("Invalid mode value\n");
		return -EINVAL;
	}

	chip = qpnp_pwm_from_pwm_dev(pwm);

	spin_lock_irqsave(&chip->lpg_lock, flags);
	if (chip->pwm_mode != mode) {
		rc = _pwm_change_mode(chip, mode);
		if (rc) {
			pr_err("Failed to change mode: %d, rc=%d\n", mode, rc);
			goto unlock;
		}
		chip->pwm_mode = mode;
		if (chip->enabled) {
			rc = _pwm_enable(chip);
			if (rc) {
				pr_err("Failed to enable PWM, rc=%d\n", rc);
				goto unlock;
			}
		}
	}
unlock:
	spin_unlock_irqrestore(&chip->lpg_lock, flags);

	return rc;
}
EXPORT_SYMBOL(pwm_change_mode);

/**
 * pwm_config_period - change PWM period
 *
 * @pwm: the PWM device
 * @pwm_p: period in struct qpnp_lpg_period
 */
int pwm_config_period(struct pwm_device *pwm,
			     struct pwm_period_config *period)
{
	struct _qpnp_pwm_config	*pwm_config;
	struct qpnp_lpg_config	*lpg_config;
	struct qpnp_pwm_chip	*chip;
	unsigned long		flags;
	int			rc = 0;

	if (pwm == NULL || IS_ERR(pwm) || period == NULL)
		return -EINVAL;
	if (pwm->chip == NULL)
		return -ENODEV;

	chip = qpnp_pwm_from_pwm_dev(pwm);
	pwm_config = &chip->pwm_config;
	lpg_config = &chip->lpg_config;

	spin_lock_irqsave(&chip->lpg_lock, flags);

	pwm_config->period.pwm_size = period->pwm_size;
	pwm_config->period.clk = period->clk;
	pwm_config->period.pre_div = period->pre_div;
	pwm_config->period.pre_div_exp = period->pre_div_exp;

	qpnp_lpg_save_period(chip);

	rc = regmap_write(chip->regmap,
			SPMI_LPG_REG_ADDR(lpg_config->base_addr,
			QPNP_LPG_PWM_SIZE_CLK),
			*&chip->qpnp_lpg_registers[QPNP_LPG_PWM_SIZE_CLK]);

	if (rc) {
		pr_err("Write failed: QPNP_LPG_PWM_SIZE_CLK register, rc: %d\n",
									rc);
		goto out_unlock;
	}

	rc = regmap_write(chip->regmap,
		SPMI_LPG_REG_ADDR(lpg_config->base_addr,
		QPNP_LPG_PWM_FREQ_PREDIV_CLK),
		*&chip->qpnp_lpg_registers[QPNP_LPG_PWM_FREQ_PREDIV_CLK]);
	if (rc) {
		pr_err("Failed to write to QPNP_LPG_PWM_FREQ_PREDIV_CLK\n");
		pr_err("register, rc = %d\n", rc);
	}

out_unlock:
	spin_unlock_irqrestore(&chip->lpg_lock, flags);
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
	struct _qpnp_pwm_config	*pwm_config;
	struct qpnp_pwm_chip	*chip;
	unsigned long		flags;
	int			rc = 0;

	if (pwm == NULL || IS_ERR(pwm)) {
		pr_err("Invalid parameter passed\n");
		return -EINVAL;
	}

	if (pwm->chip == NULL) {
		pr_err("Invalid device handle\n");
		return -ENODEV;
	}

	chip = qpnp_pwm_from_pwm_dev(pwm);
	lpg_config = &chip->lpg_config;
	pwm_config = &chip->pwm_config;

	spin_lock_irqsave(&chip->lpg_lock, flags);

	if (pwm_config->pwm_value == pwm_value)
		goto out_unlock;

	pwm_config->pwm_value = pwm_value;

	rc = qpnp_lpg_save_pwm_value(chip);

	if (rc)
		pr_err("Could not update PWM value for channel %d rc=%d\n",
						chip->channel_id, rc);

out_unlock:
	spin_unlock_irqrestore(&chip->lpg_lock, flags);
	return rc;
}
EXPORT_SYMBOL(pwm_config_pwm_value);

/**
 * pwm_config_us - change a PWM device configuration
 * @pwm: the PWM device
 * @period_us: period in microseconds
 * @duty_us: duty cycle in microseconds
 */
int pwm_config_us(struct pwm_device *pwm, int duty_us, int period_us)
{
	int rc;
	unsigned long flags;
	struct qpnp_pwm_chip *chip;

	if (pwm == NULL || IS_ERR(pwm) ||
		duty_us > period_us ||
		(unsigned int)period_us > PM_PWM_PERIOD_MAX ||
		(unsigned int)period_us < PM_PWM_PERIOD_MIN) {
		pr_err("Invalid pwm handle or parameters\n");
		return -EINVAL;
	}

	chip = qpnp_pwm_from_pwm_dev(pwm);

	spin_lock_irqsave(&chip->lpg_lock, flags);

	chip->pwm_config.update_period = false;
	if (chip->pwm_config.pwm_period != period_us) {
		qpnp_lpg_calc_period(LVL_USEC, period_us, chip);
		qpnp_lpg_save_period(chip);
		chip->pwm_config.pwm_period = period_us;
		if ((unsigned int)period_us >
		    (unsigned int)(-1) / NSEC_PER_USEC)
			pwm->period = 0;
		else
			pwm->period = (unsigned int)period_us * NSEC_PER_USEC;
		chip->pwm_config.update_period = true;
	}

	rc = _pwm_config(chip, LVL_USEC, duty_us, period_us);

	spin_unlock_irqrestore(&chip->lpg_lock, flags);

	if (rc)
		pr_err("Failed to configure PWM mode\n");

	return rc;
}
EXPORT_SYMBOL(pwm_config_us);

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
	unsigned long flags;
	struct qpnp_pwm_chip *chip;
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

	chip = qpnp_pwm_from_pwm_dev(pwm);

	if (chip->flags & QPNP_PWM_LUT_NOT_SUPPORTED) {
		pr_err("LUT mode isn't supported\n");
		return -EINVAL;
	}

	if ((lut_params.start_idx + lut_params.idx_len) >
				chip->lpg_config.lut_size) {
		pr_err("Exceed LUT limit\n");
		return -EINVAL;
	}

	if ((unsigned int)period_us > PM_PWM_PERIOD_MAX ||
	    (unsigned int)period_us < PM_PWM_PERIOD_MIN) {
		pr_err("Period out of range\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&chip->lpg_lock, flags);

	if (chip->pwm_config.pwm_period != period_us) {
		qpnp_lpg_calc_period(LVL_USEC, period_us, chip);
		qpnp_lpg_save_period(chip);
		chip->pwm_config.pwm_period = period_us;
	}

	rc = _pwm_lut_config(chip, period_us, duty_pct, lut_params);

	spin_unlock_irqrestore(&chip->lpg_lock, flags);

	if (rc)
		pr_err("Failed to configure LUT\n");

	return rc;
}
EXPORT_SYMBOL(pwm_lut_config);

static int qpnp_parse_pwm_dt_config(struct device_node *of_pwm_node,
		struct device_node *of_parent, struct qpnp_pwm_chip *chip)
{
	int rc, period;

	rc = of_property_read_u32(of_parent, "qcom,period", (u32 *)&period);
	if (rc) {
		pr_err("node is missing PWM Period prop");
		return rc;
	}

	rc = of_property_read_u32(of_pwm_node, "qcom,duty",
				&chip->pwm_config.pwm_duty);
	if (rc) {
		pr_err("node is missing PWM Duty prop");
		return rc;
	}

	if (period < chip->pwm_config.pwm_duty || period > PM_PWM_PERIOD_MAX ||
		period < PM_PWM_PERIOD_MIN) {
		pr_err("Invalid pwm period(%d) or duty(%d)\n", period,
			chip->pwm_config.pwm_duty);
		return -EINVAL;
	}

	qpnp_lpg_calc_period(LVL_USEC, period, chip);
	qpnp_lpg_save_period(chip);
	chip->pwm_config.pwm_period = period;
	chip->pwm_config.update_period = true;

	rc = _pwm_config(chip, LVL_USEC, chip->pwm_config.pwm_duty, period);

	return rc;
}

static int qpnp_parse_lpg_dt_config(struct device_node *of_lpg_node,
		struct device_node *of_parent, struct qpnp_pwm_chip *chip)
{
	int rc, period, list_size, start_idx, *duty_pct_list;
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	struct qpnp_lut_config	*lut_config = &lpg_config->lut_config;
	struct lut_params	lut_params;

	rc = of_property_read_u32(of_parent, "qcom,period", &period);
	if (rc) {
		pr_err("node is missing PWM Period prop\n");
		return rc;
	}

	if (!of_get_property(of_lpg_node, "qcom,duty-percents", &list_size)) {
		pr_err("node is missing duty-pct list\n");
		return rc;
	}

	rc = of_property_read_u32(of_lpg_node, "cell-index", &start_idx);
	if (rc) {
		pr_err("Missing start index\n");
		return rc;
	}

	list_size /= sizeof(u32);

	if (list_size + start_idx > lpg_config->lut_size) {
		pr_err("duty pct list size overflows\n");
		return -EINVAL;
	}

	duty_pct_list = kcalloc(list_size, sizeof(*duty_pct_list), GFP_KERNEL);
	if (!duty_pct_list)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_lpg_node, "qcom,duty-percents",
						duty_pct_list, list_size);
	if (rc) {
		pr_err("invalid or missing property: qcom,duty-pcts-list\n");
		goto out;
	}

	/* Read optional properties */
	rc = of_property_read_u32(of_lpg_node, "qcom,ramp-step-duration",
				  &lut_config->ramp_step_ms);
	if (rc && rc != -EINVAL)
		goto out;

	rc = of_property_read_u32(of_lpg_node, "qcom,lpg-lut-pause-hi",
				  &lut_config->lut_pause_hi_cnt);
	if (rc && rc != -EINVAL)
		goto out;

	rc = of_property_read_u32(of_lpg_node, "qcom,lpg-lut-pause-lo",
				  &lut_config->lut_pause_lo_cnt);
	if (rc && rc != -EINVAL)
		goto out;

	rc = of_property_read_u32(of_lpg_node, "qcom,lpg-lut-ramp-direction",
				  (u32 *)&lut_config->ramp_direction);
	if (rc && rc != -EINVAL)
		goto out;

	rc = of_property_read_u32(of_lpg_node, "qcom,lpg-lut-pattern-repeat",
				  (u32 *)&lut_config->pattern_repeat);
	if (rc && rc != -EINVAL)
		goto out;

	rc = of_property_read_u32(of_lpg_node, "qcom,lpg-lut-ramp-toggle",
				  (u32 *)&lut_config->ramp_toggle);
	if (rc && rc != -EINVAL)
		goto out;

	rc = of_property_read_u32(of_lpg_node, "qcom,lpg-lut-enable-pause-hi",
				  (u32 *)&lut_config->enable_pause_hi);
	if (rc && rc != -EINVAL)
		goto out;

	rc = of_property_read_u32(of_lpg_node, "qcom,lpg-lut-enable-pause-lo",
				  (u32 *)&lut_config->enable_pause_lo);
	if (rc && rc != -EINVAL)
		goto out;
	rc = 0;

	qpnp_set_lut_params(&lut_params, lut_config, start_idx, list_size);

	_pwm_lut_config(chip, period, duty_pct_list, lut_params);

out:
	kfree(duty_pct_list);
	return rc;
}

static int qpnp_lpg_get_rev_subtype(struct qpnp_pwm_chip *chip)
{
	int rc;
	uint val;

	rc = regmap_read(chip->regmap,
			 chip->lpg_config.base_addr + SPMI_LPG_SUB_TYPE_OFFSET,
			 &val);

	if (rc) {
		pr_err("Couldn't read subtype rc: %d\n", rc);
		goto out;
	}
	 chip->sub_type = (u8)val;

	rc = regmap_read(chip->regmap,
			 chip->lpg_config.base_addr + SPMI_LPG_REVISION2_OFFSET,
			 &val);

	if (rc) {
		pr_err("Couldn't read revision2 rc: %d\n", rc);
		goto out;
	}
	chip->revision = (u8)val;

	if (chip->revision < QPNP_LPG_REVISION_0 ||
		chip->revision > QPNP_LPG_REVISION_1) {
		pr_err("Unknown LPG revision detected, rev:%d\n",
						chip->revision);
		rc = -EINVAL;
		goto out;
	}

	if (chip->sub_type != QPNP_PWM_MODE_ONLY_SUB_TYPE
		&& chip->sub_type != QPNP_LPG_CHAN_SUB_TYPE
		&& chip->sub_type != QPNP_LPG_S_CHAN_SUB_TYPE) {
		pr_err("Unknown LPG/PWM subtype detected, subtype:%d\n",
						chip->sub_type);
		rc = -EINVAL;
	}
out:
	pr_debug("LPG rev 0x%02x subtype 0x%02x rc: %d\n", chip->revision,
		chip->sub_type, rc);
	return rc;
}

/* Fill in lpg device elements based on values found in device tree. */
static int qpnp_parse_dt_config(struct platform_device *pdev,
					struct qpnp_pwm_chip *chip)
{
	int			rc, mode, lut_entry_size, list_size, i;
	const char		*label;
	const __be32		*prop;
	u32			size;
	struct device_node	*node;
	int found_pwm_subnode = 0;
	int found_lpg_subnode = 0;
	struct device_node	*of_node = pdev->dev.of_node;
	struct qpnp_lpg_config	*lpg_config = &chip->lpg_config;
	struct qpnp_lut_config	*lut_config = &lpg_config->lut_config;
	struct _qpnp_pwm_config	*pwm_config = &chip->pwm_config;
	int			force_pwm_size = 0;
	int			pwm_size_list[QPNP_PWM_SIZES_SUPPORTED];

	rc = of_property_read_u32(of_node, "qcom,channel-id",
				&chip->channel_id);
	if (rc) {
		dev_err(&pdev->dev, "%s: node is missing LPG channel id\n",
								__func__);
		return -EINVAL;
	}

	if (!of_get_property(of_node, "qcom,supported-sizes", &list_size)) {
		pr_err("Missing qcom,supported-size list\n");
		return -EINVAL;
	}

	list_size /= sizeof(u32);
	if (list_size > QPNP_PWM_SIZES_SUPPORTED) {
		pr_err(" qcom,supported-size list is too big\n");
		return -EINVAL;
	}

	rc = of_property_read_u32_array(of_node, "qcom,supported-sizes",
			pwm_size_list, list_size);

	if (rc) {
		pr_err("Invalid qcom,supported-size property\n");
		return rc;
	}

	for (i = 0; i < list_size; i++) {
		pwm_config->supported_sizes |=
			(1 << (pwm_size_list[i] - QPNP_MIN_PWM_BIT_SIZE));
	}

	if (!(pwm_config->supported_sizes == QPNP_PWM_SIZE_6_9_BIT ||
		pwm_config->supported_sizes == QPNP_PWM_SIZE_7_8_BIT ||
		pwm_config->supported_sizes == QPNP_PWM_SIZE_6_7_9_BIT)) {
		pr_err("PWM sizes list qcom,supported-size is not proper\n");
		return -EINVAL;
	}

	/*
	 * For cetrain LPG channels PWM size can be forced. So that
	 * for every requested pwm period closest pwm frequency is
	 * selected in qpnp_lpg_calc_period() for the forced pwm size.
	 */
	rc = of_property_read_u32(of_node, "qcom,force-pwm-size",
				&force_pwm_size);
	if (pwm_config->supported_sizes == QPNP_PWM_SIZE_7_8_BIT) {
		if (!(force_pwm_size == QPNP_PWM_SIZE_7_BIT ||
				force_pwm_size == QPNP_PWM_SIZE_8_BIT))
			force_pwm_size = 0;
	} else if (chip->sub_type == QPNP_PWM_MODE_ONLY_SUB_TYPE) {
		if (!(force_pwm_size == QPNP_PWM_SIZE_6_BIT ||
				force_pwm_size == QPNP_PWM_SIZE_9_BIT))
			force_pwm_size = 0;
	} else if (pwm_config->supported_sizes == QPNP_PWM_SIZE_6_7_9_BIT) {
		if (!(force_pwm_size == QPNP_PWM_SIZE_6_BIT ||
				force_pwm_size == QPNP_PWM_SIZE_7_BIT ||
				force_pwm_size == QPNP_PWM_SIZE_9_BIT))
			force_pwm_size = 0;
	}

	pwm_config->force_pwm_size = force_pwm_size;


	prop = of_get_address_by_name(pdev->dev.of_node, QPNP_LPG_CHANNEL_BASE,
			0, 0);
	if (!prop) {
		dev_err(&pdev->dev, "Couldnt find channel's base addr rc %d\n",
				rc);
		return rc;
	}
	lpg_config->base_addr = be32_to_cpu(*prop);

	rc = qpnp_lpg_get_rev_subtype(chip);
	if (rc)
		return rc;

	prop = of_get_address_by_name(pdev->dev.of_node, QPNP_LPG_LUT_BASE,
			0, 0);
	if (!prop) {
		chip->flags |= QPNP_PWM_LUT_NOT_SUPPORTED;
	} else {
		lpg_config->lut_base_addr = be32_to_cpu(*prop);
		rc = of_property_read_u32(of_node, "qcom,lpg-lut-size", &size);
		if (rc < 0) {
			dev_err(&pdev->dev, "Error reading qcom,lpg-lut-size, rc=%d\n",
					rc);
			return rc;
		}

		/*
		 * Each entry of LUT is of 2 bytes for generic LUT and of 1 byte
		 * for KPDBL/GLED LUT.
		 */
		lpg_config->lut_size = size >> 1;
		lut_entry_size = sizeof(u16);

		if (pwm_config->supported_sizes == QPNP_PWM_SIZE_7_8_BIT) {
			lpg_config->lut_size = size;
			lut_entry_size = sizeof(u8);
		}

		lut_config->duty_pct_list = kcalloc(lpg_config->lut_size,
					lut_entry_size, GFP_KERNEL);
		if (!lut_config->duty_pct_list)
			return -ENOMEM;

		rc = of_property_read_u32(of_node, "qcom,ramp-index",
						&lut_config->ramp_index);
		if (rc) {
			pr_err("Missing LPG qcom,ramp-index property\n");
			kfree(lut_config->duty_pct_list);
			return rc;
		}
	}

	rc = of_property_read_u32(of_node, "qcom,dtest-line",
		&chip->dtest_line);
	if (rc) {
		chip->in_test_mode = 0;
	} else {
		rc = of_property_read_u32(of_node, "qcom,dtest-output",
			&chip->dtest_output);
		if (rc) {
			pr_err("Missing DTEST output configuration\n");
			return rc;
		}
		chip->in_test_mode = 1;
	}

	if (chip->in_test_mode) {
		if ((chip->sub_type == QPNP_PWM_MODE_ONLY_SUB_TYPE) &&
			(chip->dtest_line > QPNP_PWM_DTEST_LINE_MAX ||
			chip->dtest_output > QPNP_PWM_DTEST_OUTPUT_MAX)) {
			pr_err("DTEST line/output values are improper for PWM channel %d\n",
				chip->channel_id);
			return -EINVAL;
		} else if (chip->dtest_line > QPNP_LPG_DTEST_LINE_MAX ||
			chip->dtest_output > QPNP_LPG_DTEST_OUTPUT_MAX) {
			pr_err("DTEST line/output values are improper for LPG channel %d\n",
				chip->channel_id);
			return -EINVAL;
		}
	}

	for_each_child_of_node(of_node, node) {
		rc = of_property_read_string(node, "label", &label);
		if (rc) {
			dev_err(&pdev->dev, "%s: Missing label property\n",
								__func__);
			goto out;
		}
		if (!strcmp(label, "pwm")) {
			rc = qpnp_parse_pwm_dt_config(node, of_node, chip);
			if (rc)
				goto out;
			found_pwm_subnode = 1;
		} else if (!strcmp(label, "lpg") &&
				!(chip->flags & QPNP_PWM_LUT_NOT_SUPPORTED)) {
			rc = qpnp_parse_lpg_dt_config(node, of_node, chip);
			if (rc)
				goto out;
			found_lpg_subnode = 1;
		} else {
			dev_err(&pdev->dev,
				"%s: Invalid value for lable prop",
								__func__);
		}
	}

	rc = of_property_read_u32(of_node, "qcom,mode-select", &mode);
	if (rc)
		goto read_opt_props;

	if (mode > PM_PWM_MODE_LPG ||
		(mode == PM_PWM_MODE_PWM && found_pwm_subnode == 0) ||
		(mode == PM_PWM_MODE_LPG && found_lpg_subnode == 0)) {
		dev_err(&pdev->dev, "%s: Invalid mode select\n", __func__);
		rc = -EINVAL;
		goto out;
	}

	chip->pwm_mode = mode;
	_pwm_change_mode(chip, mode);
	_pwm_enable(chip);

read_opt_props:
	/* Initialize optional config parameters from DT if provided */
	of_property_read_string(node, "qcom,channel-owner",
				&chip->channel_owner);

	return 0;

out:
	kfree(lut_config->duty_pct_list);
	return rc;
}

static struct pwm_ops qpnp_pwm_ops = {
	.enable = qpnp_pwm_enable,
	.disable = qpnp_pwm_disable,
	.config = qpnp_pwm_config,
	.free = qpnp_pwm_free,
	.owner = THIS_MODULE,
};

static int qpnp_pwm_probe(struct platform_device *pdev)
{
	struct qpnp_pwm_chip	*pwm_chip;
	int			rc;

	pwm_chip = kzalloc(sizeof(*pwm_chip), GFP_KERNEL);
	if (pwm_chip == NULL)
		return -ENOMEM;

	pwm_chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!pwm_chip->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	spin_lock_init(&pwm_chip->lpg_lock);

	pwm_chip->pdev = pdev;
	dev_set_drvdata(&pdev->dev, pwm_chip);

	rc = qpnp_parse_dt_config(pdev, pwm_chip);

	if (rc) {
		pr_err("Failed parsing DT parameters, rc=%d\n", rc);
		goto failed_config;
	}

	pwm_chip->chip.dev = &pdev->dev;
	pwm_chip->chip.ops = &qpnp_pwm_ops;
	pwm_chip->chip.base = -1;
	pwm_chip->chip.npwm = 1;

	rc = pwmchip_add(&pwm_chip->chip);
	if (rc < 0) {
		pr_err("pwmchip_add() failed: %d\n", rc);
		goto failed_insert;
	}

	if (pwm_chip->channel_owner)
		pwm_chip->chip.pwms[0].label = pwm_chip->channel_owner;

	pr_debug("PWM device channel:%d probed successfully\n",
		pwm_chip->channel_id);
	return 0;

failed_insert:
	kfree(pwm_chip->lpg_config.lut_config.duty_pct_list);
failed_config:
	dev_set_drvdata(&pdev->dev, NULL);
	kfree(pwm_chip);
	return rc;
}

static int qpnp_pwm_remove(struct platform_device *pdev)
{
	struct qpnp_pwm_chip *pwm_chip;
	struct qpnp_lpg_config *lpg_config;

	pwm_chip = dev_get_drvdata(&pdev->dev);

	dev_set_drvdata(&pdev->dev, NULL);

	if (pwm_chip) {
		lpg_config = &pwm_chip->lpg_config;
		pwmchip_remove(&pwm_chip->chip);
		kfree(lpg_config->lut_config.duty_pct_list);
		kfree(pwm_chip);
	}

	return 0;
}

static const struct of_device_id spmi_match_table[] = {
	{ .compatible = QPNP_LPG_DRIVER_NAME, },
	{}
};

static const struct platform_device_id qpnp_lpg_id[] = {
	{ QPNP_LPG_DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spmi, qpnp_lpg_id);

static struct platform_driver qpnp_lpg_driver = {
	.driver		= {
		.name		= QPNP_LPG_DRIVER_NAME,
		.of_match_table	= spmi_match_table,
		.owner		= THIS_MODULE,
	},
	.probe		= qpnp_pwm_probe,
	.remove		= qpnp_pwm_remove,
	.id_table	= qpnp_lpg_id,
};

/**
 * qpnp_lpg_init() - register spmi driver for qpnp-lpg
 */
int __init qpnp_lpg_init(void)
{
	return platform_driver_register(&qpnp_lpg_driver);
}

static void __exit qpnp_lpg_exit(void)
{
	platform_driver_unregister(&qpnp_lpg_driver);
}

MODULE_DESCRIPTION("QPNP PMIC LPG driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" QPNP_LPG_DRIVER_NAME);

subsys_initcall(qpnp_lpg_init);
module_exit(qpnp_lpg_exit);
