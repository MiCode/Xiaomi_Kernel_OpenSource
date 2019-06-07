/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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
#include <linux/clk.h>
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
#include <linux/debugfs.h>
#include <linux/fs.h>

#define MAX_CHANNELS	10
#define PERIOD_TO_HZ(period_ns) ((1  * 1000000000UL) / period_ns)

/* Offsets */
#define PWM_TOPCTL0	0x0
#define ENABLE_STATUS	0xc

/* offsets per frame */
#define PWM_CTL0	0x0
#define PWM_CTL1	0x4
#define PWM_CTL2	0x8
#define PWM_CYC_CFG	0xC
#define PWM_UPDATE	0x10
#define PWM_PERIOD_CNT	0x14

/*
 *struct pdm_pwm_frames - Information regarding per pdm frame
 * @frame_id: Id number associated with each frame.
 * @reg_offset: offset of each frame from base pdm.
 * @current_period_ns: Current period of the particular frame.
 * @current_duty_ns: Current duty cycle of the particular frame.
 * @current_freq: Current frequency of frame.
 * @freq_set: This bool flag is responsible for setting period once
 *	      per frame.
 * @mutex: mutex lock per frame.
 */
struct pdm_pwm_frames {
	u32	frame_id;
	u32	reg_offset;
	u64	current_period_ns;
	u64	current_duty_ns;
	unsigned long current_freq;
	bool	freq_set;
	struct mutex frame_lock; /* PWM per frame lock */
};

/*
 *struct pdm_pwm_chip - Information regarding per pdm
 * @pwm_chip: information per pdm.
 * @regmap: regmap of each pdm.
 * @device: pdm device.
 * @pdm_pwm_frames: structure for all frames of each pdm.
 * @pdm_ahb_clk: pdm clock for enabling pdm block
 * @pwm_core_clk: pwm clock for enabling each pwm.
 * @mutex: mutex lock per frame.
 * @pwm_core_rate: core rate of pdm_ahb_clk.
 * @num_frames: number of frames in each pdm.
 */
struct pdm_pwm_chip {
	struct pwm_chip		pwm_chip;
	struct regmap		*regmap;
	struct device		*dev;
	struct pdm_pwm_frames	*frames;
	struct clk		*pdm_ahb_clk;
	struct clk		*pwm_core_clk;
	struct mutex		lock;  /*
					* This lock to be used for
					* Enable/Disable as it is per PWM
					* channel.
					*/
	unsigned long		pwm_core_rate;
	u32			num_frames;
};

static int __pdm_pwm_calc_pwm_frequency(struct pdm_pwm_chip *chip,
					int period_ns, u32 hw_idx)
{
	unsigned long cyc_cfg, freq;
	int ret;

	/* PWM client could set the period only once, due to HW limitation. */
	if (chip->frames[hw_idx].freq_set)
		return 0;

	freq = PERIOD_TO_HZ(period_ns);
	if (!freq) {
		pr_err("Frequency cannot be Zero\n");
		return -EINVAL;
	}
	if (freq > (chip->pwm_core_rate >> 1) || freq <=
						(chip->pwm_core_rate >> 16)) {
		pr_debug("Freq %ld is not in range Max=%ld Min=%ld\n", freq,
		(chip->pwm_core_rate >> 1), (chip->pwm_core_rate >> 16) + 1);
		return -ERANGE;
	}
	cyc_cfg = DIV_ROUND_CLOSEST(chip->pwm_core_rate, freq) - 1;

	ret = regmap_update_bits(chip->regmap,
				chip->frames[hw_idx].reg_offset + PWM_CYC_CFG,
						GENMASK(15, 0), cyc_cfg);
	if (ret)
		return ret;

	chip->frames[hw_idx].current_freq = freq;
	chip->frames[hw_idx].freq_set = true;
	chip->frames[hw_idx].current_period_ns = period_ns;

	return 0;
}

static int pdm_pwm_config(struct pdm_pwm_chip *chip, u32 hw_idx,
				int duty_ns, int period_ns)
{
	unsigned long ctl1;
	int current_period = period_ns, ret;
	u32 cyc_cfg;

	/*
	 * 1. Enable GCC_PDM_AHB_CBCR clock for PDM block Access
	 * 2. pwm_core_rate = clk_get_rate(pwm_core_clk); for now it is
	 * 19.2MHz.
	 * 3. min_freq = pwm_core_rate/2 ^ 16;
	 * 4. max_freq = pwm_core_rate/2;
	 * 5. calculate the frequency based on the period_ns and compare.
	 */
	ret = clk_prepare_enable(chip->pdm_ahb_clk);
	if (ret)
		return ret;

	ret = clk_prepare_enable(chip->pwm_core_clk);
	if (ret)
		goto fail;

	mutex_lock(&chip->frames[hw_idx].frame_lock);

	ret = __pdm_pwm_calc_pwm_frequency(chip, current_period, hw_idx);
	if (ret)
		goto out;

	if (chip->frames[hw_idx].current_period_ns != period_ns) {
		pr_err("Period cannot be updated, calculating dutycycle on old period\n");
		current_period = chip->frames[hw_idx].current_period_ns;
	}

	ctl1 = DIV_ROUND_CLOSEST(chip->pwm_core_rate,
					chip->frames[hw_idx].current_freq);

	ctl1 = DIV_ROUND_CLOSEST(ctl1 * (DIV_ROUND_CLOSEST((duty_ns * 100),
							current_period)), 100);

	regmap_read(chip->regmap, chip->frames[hw_idx].reg_offset
					+ PWM_CYC_CFG, &cyc_cfg);
	if ((ctl1 > cyc_cfg || ctl1 <= 0) && duty_ns != 0) {
		pr_err("Duty cycle cannot be set at and beyond/below this limit\n");
		goto out;
	}

	ret = regmap_update_bits(chip->regmap, chip->frames[hw_idx].reg_offset
					+ PWM_CTL2, GENMASK(15, 0), 0);
	if (ret)
		goto out;

	ret = regmap_update_bits(chip->regmap, chip->frames[hw_idx].reg_offset
					+ PWM_CTL1, GENMASK(15, 0), ctl1);
	if (ret)
		goto out;

	ret = regmap_update_bits(chip->regmap, chip->frames[hw_idx].reg_offset
					+ PWM_UPDATE, BIT(0), 1);
	if (ret)
		goto out;

	chip->frames[hw_idx].current_duty_ns = duty_ns;
out:
	mutex_unlock(&chip->frames[hw_idx].frame_lock);

	clk_disable_unprepare(chip->pwm_core_clk);
fail:
	clk_disable_unprepare(chip->pdm_ahb_clk);

	return ret;
}

static void pdm_pwm_free(struct pwm_chip *pwm_chip, struct pwm_device *pwm)
{
	struct pdm_pwm_chip *chip = container_of(pwm_chip,
					struct pdm_pwm_chip, pwm_chip);
	u32 hw_idx = pwm->hwpwm;

	mutex_lock(&chip->lock);

	chip->frames[hw_idx].freq_set = false;
	chip->frames[hw_idx].current_period_ns = 0;
	chip->frames[hw_idx].current_duty_ns = 0;

	mutex_unlock(&chip->lock);
}

static int pdm_pwm_enable(struct pdm_pwm_chip *chip, struct pwm_device *pwm)
{
	u32 ret, val;
	u32 hw_idx = pwm->hwpwm;

	ret = clk_prepare_enable(chip->pdm_ahb_clk);
	if (ret)
		return ret;

	ret = clk_prepare_enable(chip->pwm_core_clk);
	if (ret) {
		clk_disable_unprepare(chip->pdm_ahb_clk);
		return ret;
	}

	mutex_lock(&chip->lock);

	/* Check the channel in Chip channel and enable the BIT in PWM_TOP */
	pr_debug("%s:PWM device Label %s, HW index %u, PWM index %u\n", __func__
					, pwm->label, hw_idx, pwm->pwm);
	pr_debug("%s: PWM frame-index %d, frame-offset 0x%x\n", __func__,
			chip->frames[hw_idx].frame_id,
					chip->frames[hw_idx].reg_offset);

	val  = BIT(chip->frames[hw_idx].frame_id);
	ret = regmap_update_bits(chip->regmap, PWM_TOPCTL0, val, val);

	mutex_unlock(&chip->lock);

	return ret;
}

static void pdm_pwm_disable(struct pdm_pwm_chip *chip, struct pwm_device *pwm)
{
	u32 val, hw_idx = pwm->hwpwm;

	mutex_lock(&chip->lock);

	/* Check the channel in the chip and disable the BIT in PWM_TOP */
	pr_debug("%s:PWM device Label %s\n", __func__, pwm->label);

	val = BIT(chip->frames[hw_idx].frame_id);
	regmap_update_bits(chip->regmap, PWM_TOPCTL0, val, 0);

	mutex_unlock(&chip->lock);

	clk_disable_unprepare(chip->pwm_core_clk);
	clk_disable_unprepare(chip->pdm_ahb_clk);
}

static int pdm_pwm_apply(struct pwm_chip *pwm_chip, struct pwm_device *pwm,
					struct pwm_state *state)
{
	struct pdm_pwm_chip *chip = container_of(pwm_chip,
				struct pdm_pwm_chip, pwm_chip);
	struct pwm_state curr_state;
	int ret;

	pwm_get_state(pwm, &curr_state);

	if (state->period < curr_state.period)
		return -EINVAL;

	if (state->period != curr_state.period ||
		state->duty_cycle != curr_state.duty_cycle) {
		ret = pdm_pwm_config(chip, pwm->hwpwm, state->duty_cycle,
						state->period);
		if (ret) {
			pr_err("%s: Failed to update PWM configuration\n",
								 __func__);
			return ret;
		}
	}

	if (state->enabled != curr_state.enabled) {
		if (state->enabled)
			return pdm_pwm_enable(chip, pwm);

		pdm_pwm_disable(chip, pwm);
	}

	return 0;
}

#ifdef CONFIG_DEBUG_FS
static void pdm_pwm_dbg_show(struct pwm_chip *pwm_chip, struct seq_file *s)
{
	struct pdm_pwm_chip *chip = container_of(pwm_chip,
					struct pdm_pwm_chip, pwm_chip);
	struct pwm_device *pwm;
	u32 i, hw_idx, tmp;
	int ret;

	ret = clk_prepare_enable(chip->pdm_ahb_clk);
	if (ret)
		pr_err("unable to enable pdm_ahb_clk\n");

	ret = clk_prepare_enable(chip->pwm_core_clk);
	if (ret)
		pr_err("unable to enable pwm_core_clk\n");

	for (i = 0; i < pwm_chip->npwm; i++) {
		pwm = &pwm_chip->pwms[i];
		hw_idx = pwm->hwpwm;

		seq_printf(s, "\nframe_id %d ",
					chip->frames[hw_idx].frame_id);

		regmap_read(chip->regmap, ENABLE_STATUS, &tmp);
		if (BIT((chip->frames[hw_idx].frame_id) + BIT(0)) & tmp)
			seq_puts(s, ": Enable\n\n");
		else
			seq_puts(s, ": Disable\n\n");

		regmap_read(chip->regmap, chip->frames[hw_idx].reg_offset
							+ PWM_CTL1, &tmp);
		seq_printf(s, "pwm_ctl1       =  0x%x\n", tmp);

		regmap_read(chip->regmap, chip->frames[hw_idx].reg_offset +
							PWM_CTL2, &tmp);
		seq_printf(s, "pwm_ctl2       =  0x%x\n", tmp);

		regmap_read(chip->regmap, chip->frames[hw_idx].reg_offset +
							PWM_CYC_CFG, &tmp);
		seq_printf(s, "pwm_cyc_cfg    =  0x%x\n", tmp);

		regmap_read(chip->regmap, chip->frames[hw_idx].reg_offset +
							PWM_PERIOD_CNT, &tmp);
		seq_printf(s, "pwm_period_cnt =  0x%x\n\n", tmp);

		seq_printf(s, "current frequency  (Hz) =  %lu\n",
			PERIOD_TO_HZ(chip->frames[hw_idx].current_period_ns));
		seq_printf(s, "current period     (ns) =  %lu\n",
				chip->frames[hw_idx].current_period_ns);
		seq_printf(s, "current duty cycle (ns) =  %lu\n",
					chip->frames[hw_idx].current_duty_ns);
		seq_printf(s, "current duty       (%%)  =  %lu\n\n",
		DIV_ROUND_CLOSEST((chip->frames[hw_idx].current_duty_ns * 100),
				chip->frames[hw_idx].current_period_ns));
	}

	clk_disable_unprepare(chip->pwm_core_clk);
	clk_disable_unprepare(chip->pdm_ahb_clk);
}
#endif


static const struct pwm_ops pdm_pwm_ops = {
	.free = pdm_pwm_free,
	.apply = pdm_pwm_apply,
#ifdef CONFIG_DEBUG_FS
	.dbg_show = pdm_pwm_dbg_show,
#endif
};

static const struct regmap_config pwm_regmap_config = {
	.reg_bits   = 32,
	.reg_stride = 4,
	.val_bits   = 32,
	.fast_io    = true,
};

static int pdm_pwm_parse_dt(struct platform_device *pdev,
				struct pdm_pwm_chip *chip)
{
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *frame_node;
	void __iomem *base;
	int count, ret;

	chip->pdm_ahb_clk = devm_clk_get(chip->dev, "pdm_ahb_clk");
	if (IS_ERR(chip->pdm_ahb_clk)) {
		if (PTR_ERR(chip->pdm_ahb_clk) != -EPROBE_DEFER)
			dev_err(chip->dev, "Unable to get ahb clock handle\n");
		return PTR_ERR(chip->pdm_ahb_clk);
	}

	chip->pwm_core_clk = devm_clk_get(chip->dev, "pwm_core_clk");
	if (IS_ERR(chip->pwm_core_clk)) {
		if (PTR_ERR(chip->pwm_core_clk) != -EPROBE_DEFER)
			dev_err(chip->dev, "Unable to get core clock handle\n");
		return PTR_ERR(chip->pwm_core_clk);
	}

	chip->pwm_core_rate = clk_get_rate(chip->pwm_core_clk);
	if (!chip->pwm_core_rate)
		chip->pwm_core_rate = 19200000;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(chip->dev, "Failed to get reg base resource\n");
		return -EINVAL;
	}

	base = devm_ioremap(chip->dev, res->start, resource_size(res));
	if (!base)
		return -ENOMEM;

	chip->regmap = devm_regmap_init_mmio(chip->dev, base,
						&pwm_regmap_config);
	if (!chip->regmap) {
		dev_err(chip->dev, "Couldn't get regmap\n");
		return -EINVAL;
	}

	chip->num_frames = of_get_child_count(np);
	if (!chip->num_frames || chip->num_frames > MAX_CHANNELS) {
		dev_err(chip->dev, "PWM frames 0-%u are supported.\n");
		return -EINVAL;
	}

	chip->frames = devm_kcalloc(chip->dev, chip->num_frames,
			sizeof(*chip->frames), GFP_KERNEL);
	if (!chip->frames)
		return -ENOMEM;

	count = 0;
	for_each_available_child_of_node(np, frame_node) {
		u32 n, off;

		if (of_property_read_u32(frame_node, "frame-index", &n)) {
			pr_err(FW_BUG "Missing frame-index.\n");
			of_node_put(frame_node);
			return -EINVAL;
		}
		chip->frames[count].frame_id = n;

		if (of_property_read_u32(frame_node, "frame-offset", &off)) {
			pr_err(FW_BUG "Missing frame-offset.\n");
			of_node_put(frame_node);
			return -EINVAL;
		}
		chip->frames[count].reg_offset = off;

		mutex_init(&chip->frames[count].frame_lock);
		count++;
	}

	ret = clk_prepare_enable(chip->pdm_ahb_clk);
	if (ret)
		return 0;

	ret = regmap_update_bits(chip->regmap, PWM_TOPCTL0, GENMASK(9, 0), 0);
	if (ret)
		return 0;

	clk_disable_unprepare(chip->pdm_ahb_clk);

	return 0;
}

static int pdm_pwm_probe(struct platform_device *pdev)
{
	struct pdm_pwm_chip *chip;
	int rc;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;

	mutex_init(&chip->lock);
	rc = pdm_pwm_parse_dt(pdev, chip);
	if (rc < 0) {
		dev_err(chip->dev, "Devicetree properties parsing failed, rc=%d\n",
				rc);
		goto err_out;
	}

	dev_set_drvdata(chip->dev, chip);
	chip->pwm_chip.dev = chip->dev;
	chip->pwm_chip.base = -1;
	chip->pwm_chip.npwm = chip->num_frames;
	chip->pwm_chip.ops = &pdm_pwm_ops;

	rc = pwmchip_add(&chip->pwm_chip);
	if (rc < 0) {
		dev_err(chip->dev, "Add pwmchip failed, rc=%d\n", rc);
		goto err_out;
	}

	dev_info(chip->dev, "pwmchip driver success.\n");
	return 0;
err_out:
	mutex_destroy(&chip->lock);
	return rc;
}

static int pdm_pwm_remove(struct platform_device *pdev)
{
	struct pdm_pwm_chip *chip = dev_get_drvdata(&pdev->dev);
	int rc = 0;

	rc = pwmchip_remove(&chip->pwm_chip);
	if (rc < 0)
		dev_err(chip->dev, "Remove pwmchip failed, rc=%d\n", rc);

	mutex_destroy(&chip->lock);

	dev_set_drvdata(chip->dev, NULL);

	return rc;
}

static const struct of_device_id pdm_pwm_of_match[] = {
	{ .compatible = "qcom,pdm-pwm",},
	{ },
};

static struct platform_driver pdm_pwm_driver = {
	.driver		= {
		.name		= "pdm-pwm",
		.of_match_table	= pdm_pwm_of_match,
	},
	.probe		= pdm_pwm_probe,
	.remove		= pdm_pwm_remove,
};
module_platform_driver(pdm_pwm_driver);

MODULE_DESCRIPTION("QTI PDM PWM driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("pwm:pdm-pwm");
