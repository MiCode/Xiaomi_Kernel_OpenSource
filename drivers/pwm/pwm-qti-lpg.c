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
#include <linux/types.h>

#define REG_SIZE_PER_LPG	0x100

#define REG_LPG_PWM_SIZE_CLK		0x41
#define REG_LPG_PWM_FREQ_PREDIV_CLK	0x42
#define REG_LPG_PWM_TYPE_CONFIG		0x43
#define REG_LPG_PWM_VALUE_LSB		0x44
#define REG_LPG_PWM_VALUE_MSB		0x45
#define REG_LPG_ENABLE_CONTROL		0x46
#define REG_LPG_PWM_SYNC		0x47

/* REG_LPG_PWM_SIZE_CLK */
#define LPG_PWM_SIZE_MASK		BIT(4)
#define LPG_PWM_SIZE_SHIFT		4
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

enum {
	LUT_PATTERN = 0,
	PWM_OUTPUT,
};

static const int pwm_size[NUM_PWM_SIZE] = {6, 9};
static const int clk_freq_hz[NUM_PWM_CLK] = {1024, 32768, 19200000};
static const int clk_prediv[NUM_CLK_PREDIV] = {1, 3, 5, 6};
static const int pwm_exponent[NUM_PWM_EXP] = {0, 1, 2, 3, 4, 5, 6, 7};

struct lpg_pwm_config {
	u32	pwm_size;
	u32	pwm_clk;
	u32	prediv;
	u32	clk_exp;
	u16	pwm_value;
	u32	best_period_ns;
};

struct qpnp_lpg_channel {
	struct qpnp_lpg_chip		*chip;
	struct lpg_pwm_config		pwm_config;
	u32				lpg_idx;
	u32				reg_base;
	u8				src_sel;
	int				current_period_ns;
	int				current_duty_ns;
};

struct qpnp_lpg_chip {
	struct pwm_chip		pwm_chip;
	struct regmap		*regmap;
	struct device		*dev;
	struct qpnp_lpg_channel	*lpgs;
	struct mutex		bus_lock;
	u32			num_lpgs;
};

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

static struct qpnp_lpg_channel *pwm_dev_to_qpnp_lpg(struct pwm_chip *pwm_chip,
				struct pwm_device *pwm) {

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

static int qpnp_lpg_set_pwm_config(struct qpnp_lpg_channel *lpg)
{
	int rc;
	u8 val, mask;
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
	val = pwm_size_idx << LPG_PWM_SIZE_SHIFT | pwm_clk_idx;
	mask = LPG_PWM_SIZE_MASK | LPG_PWM_CLK_FREQ_SEL_MASK;
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

static void __qpnp_lpg_calc_pwm_period(int period_ns,
			struct lpg_pwm_config *pwm_config)
{
	struct lpg_pwm_config configs[NUM_PWM_SIZE];
	int i, j, m, n;
	int tmp1, tmp2;
	int clk_period_ns = 0, pwm_clk_period_ns;
	int clk_delta_ns = INT_MAX, min_clk_delta_ns = INT_MAX;
	int pwm_period_delta = INT_MAX, min_pwm_period_delta = INT_MAX;
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
					tmp2 = NSEC_PER_SEC / clk_freq_hz[i];

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
		if (min_clk_delta_ns < INT_MAX >> pwm_size[n])
			pwm_period_delta = min_clk_delta_ns << pwm_size[n];
		else
			pwm_period_delta = INT_MAX;
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
	pr_debug("PWM setting for period_ns %d: pwm_clk = %dHZ, prediv = %d, exponent = %d, pwm_size = %d\n",
			period_ns, pwm_config->pwm_clk, pwm_config->prediv,
			pwm_config->clk_exp, pwm_config->pwm_size);
	pr_debug("Actual period: %dns\n", pwm_config->best_period_ns);
}

static void __qpnp_lpg_calc_pwm_duty(int period_ns, int duty_ns,
			struct lpg_pwm_config *pwm_config)
{
	u16 pwm_value, max_pwm_value;

	if ((1 << pwm_config->pwm_size) > (INT_MAX / duty_ns))
		pwm_value = duty_ns / (period_ns >> pwm_config->pwm_size);
	else
		pwm_value = (duty_ns << pwm_config->pwm_size) / period_ns;

	max_pwm_value = (1 << pwm_config->pwm_size) - 1;
	if (pwm_value > max_pwm_value)
		pwm_value = max_pwm_value;
	pwm_config->pwm_value = pwm_value;
}

static int qpnp_lpg_pwm_config(struct pwm_chip *pwm_chip,
		struct pwm_device *pwm, int duty_ns, int period_ns)
{
	struct qpnp_lpg_channel *lpg;
	int rc = 0;

	lpg = pwm_dev_to_qpnp_lpg(pwm_chip, pwm);
	if (lpg == NULL) {
		dev_err(pwm_chip->dev, "lpg not found\n");
		return -ENODEV;
	}

	if (duty_ns > period_ns) {
		dev_err(pwm_chip->dev, "Duty %dns is larger than period %dns\n",
						duty_ns, period_ns);
		return -EINVAL;
	}

	if (period_ns != lpg->current_period_ns)
		__qpnp_lpg_calc_pwm_period(period_ns, &lpg->pwm_config);

	if (period_ns != lpg->current_period_ns ||
			duty_ns != lpg->current_duty_ns)
		__qpnp_lpg_calc_pwm_duty(period_ns, duty_ns, &lpg->pwm_config);

	rc = qpnp_lpg_set_pwm_config(lpg);
	if (rc < 0)
		dev_err(pwm_chip->dev, "Config PWM failed for channel %d, rc=%d\n",
						lpg->lpg_idx, rc);

	return rc;
}

static int qpnp_lpg_pwm_enable(struct pwm_chip *pwm_chip,
				struct pwm_device *pwm)
{
	struct qpnp_lpg_channel *lpg;
	int rc = 0;
	u8 mask, val;

	lpg = pwm_dev_to_qpnp_lpg(pwm_chip, pwm);
	if (lpg == NULL) {
		dev_err(pwm_chip->dev, "lpg not found\n");
		return -ENODEV;
	}

	mask = LPG_PWM_SRC_SELECT_MASK | LPG_EN_LPG_OUT_BIT;
	val = lpg->src_sel << LPG_PWM_SRC_SELECT_SHIFT | LPG_EN_LPG_OUT_BIT;

	rc = qpnp_lpg_masked_write(lpg, REG_LPG_ENABLE_CONTROL, mask, val);
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
	u8 mask, val;

	lpg = pwm_dev_to_qpnp_lpg(pwm_chip, pwm);
	if (lpg == NULL) {
		dev_err(pwm_chip->dev, "lpg not found\n");
		return;
	}

	mask = LPG_PWM_SRC_SELECT_MASK | LPG_EN_LPG_OUT_BIT;
	val = lpg->src_sel << LPG_PWM_SRC_SELECT_SHIFT;

	rc = qpnp_lpg_masked_write(lpg, REG_LPG_ENABLE_CONTROL, mask, val);
	if (rc < 0)
		dev_err(pwm_chip->dev, "Disable PWM output failed for channel %d, rc=%d\n",
						lpg->lpg_idx, rc);
}

#ifdef CONFIG_DEBUG_FS
static void qpnp_lpg_pwm_dbg_show(struct pwm_chip *pwm_chip, struct seq_file *s)
{
	struct qpnp_lpg_channel *lpg;
	struct lpg_pwm_config *cfg;
	struct pwm_device *pwm;
	int i;

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
		seq_printf(s, "  Requested period: %dns, best period = %dns\n",
				pwm_get_period(pwm), cfg->best_period_ns);
	}
}
#endif

static const struct pwm_ops qpnp_lpg_pwm_ops = {
	.config = qpnp_lpg_pwm_config,
	.enable = qpnp_lpg_pwm_enable,
	.disable = qpnp_lpg_pwm_disable,
#ifdef CONFIG_DEBUG_FS
	.dbg_show = qpnp_lpg_pwm_dbg_show,
#endif
	.owner = THIS_MODULE,
};

static int qpnp_lpg_parse_dt(struct qpnp_lpg_chip *chip)
{
	int rc = 0, i;
	u64 base, length;
	const __be32 *addr;

	addr = of_get_address(chip->dev->of_node, 0, NULL, NULL);
	if (!addr) {
		dev_err(chip->dev, "Getting address failed\n");
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
		chip->lpgs[i].src_sel = PWM_OUTPUT;
	}

	return rc;
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

	rc = qpnp_lpg_parse_dt(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Devicetree properties parsing failed, rc=%d\n",
				rc);
		return rc;
	}

	dev_set_drvdata(chip->dev, chip);

	mutex_init(&chip->bus_lock);
	chip->pwm_chip.dev = chip->dev;
	chip->pwm_chip.base = -1;
	chip->pwm_chip.npwm = chip->num_lpgs;
	chip->pwm_chip.ops = &qpnp_lpg_pwm_ops;

	rc = pwmchip_add(&chip->pwm_chip);
	if (rc < 0) {
		dev_err(chip->dev, "Add pwmchip failed, rc=%d\n", rc);
		mutex_destroy(&chip->bus_lock);
	}

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
