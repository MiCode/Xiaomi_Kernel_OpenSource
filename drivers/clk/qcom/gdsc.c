/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk/msm-clk.h>

#define PWR_ON_MASK		BIT(31)
#define EN_REST_WAIT_MASK	(0xF << 20)
#define EN_FEW_WAIT_MASK	(0xF << 16)
#define CLK_DIS_WAIT_MASK	(0xF << 12)
#define SW_OVERRIDE_MASK	BIT(2)
#define HW_CONTROL_MASK		BIT(1)
#define SW_COLLAPSE_MASK	BIT(0)

/* Wait 2^n CXO cycles between all states. Here, n=2 (4 cycles). */
#define EN_REST_WAIT_VAL	(0x2 << 20)
#define EN_FEW_WAIT_VAL		(0x8 << 16)
#define CLK_DIS_WAIT_VAL	(0x2 << 12)

#define TIMEOUT_US		100

struct gdsc {
	struct regulator_dev	*rdev;
	struct regulator_desc	rdesc;
	void __iomem		*gdscr;
	struct clk		**clocks;
	int			clock_count;
	bool			toggle_mem;
	bool			toggle_periph;
	bool			toggle_logic;
	bool			resets_asserted;
	bool			root_en;
};

static int gdsc_is_enabled(struct regulator_dev *rdev)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);

	if (!sc->toggle_logic)
		return !sc->resets_asserted;

	return !!(readl_relaxed(sc->gdscr) & PWR_ON_MASK);
}

static int gdsc_enable(struct regulator_dev *rdev)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval;
	int i, ret;

	if (sc->root_en) {
		for (i = 0; i < sc->clock_count; i++)
			clk_prepare_enable(sc->clocks[i]);
	}

	if (sc->toggle_logic) {
		regval = readl_relaxed(sc->gdscr);
		if (regval & HW_CONTROL_MASK) {
			dev_warn(&rdev->dev, "Invalid enable while %s is under HW control\n",
				 sc->rdesc.name);
			return -EBUSY;
		}

		regval &= ~SW_COLLAPSE_MASK;
		writel_relaxed(regval, sc->gdscr);

		ret = readl_tight_poll_timeout(sc->gdscr, regval,
					regval & PWR_ON_MASK, TIMEOUT_US);
		if (ret) {
			dev_err(&rdev->dev, "%s enable timed out: 0x%x\n",
				sc->rdesc.name, regval);
			udelay(TIMEOUT_US);
			regval = readl_relaxed(sc->gdscr);
			dev_err(&rdev->dev, "%s final state: 0x%x (%d us after timeout)\n",
				sc->rdesc.name, regval, TIMEOUT_US);
			return ret;
		}
	} else {
		for (i = 0; i < sc->clock_count; i++)
			clk_reset(sc->clocks[i], CLK_RESET_DEASSERT);
		sc->resets_asserted = false;
	}

	for (i = 0; i < sc->clock_count; i++) {
		if (sc->toggle_mem)
			clk_set_flags(sc->clocks[i], CLKFLAG_RETAIN_MEM);
		if (sc->toggle_periph)
			clk_set_flags(sc->clocks[i], CLKFLAG_RETAIN_PERIPH);
	}

	/*
	 * If clocks to this power domain were already on, they will take an
	 * additional 4 clock cycles to re-enable after the rail is enabled.
	 * Delay to account for this. A delay is also needed to ensure clocks
	 * are not enabled within 400ns of enabling power to the memories.
	 */
	udelay(1);

	return 0;
}

static int gdsc_disable(struct regulator_dev *rdev)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval;
	int i, ret = 0;

	for (i = sc->clock_count-1; i >= 0; i--) {
		if (sc->toggle_mem)
			clk_set_flags(sc->clocks[i], CLKFLAG_NORETAIN_MEM);
		if (sc->toggle_periph)
			clk_set_flags(sc->clocks[i], CLKFLAG_NORETAIN_PERIPH);
	}

	if (sc->toggle_logic) {
		regval = readl_relaxed(sc->gdscr);
		if (regval & HW_CONTROL_MASK) {
			dev_warn(&rdev->dev, "Invalid disable while %s is under HW control\n",
				 sc->rdesc.name);
			return -EBUSY;
		}

		regval |= SW_COLLAPSE_MASK;
		writel_relaxed(regval, sc->gdscr);

		ret = readl_tight_poll_timeout(sc->gdscr, regval,
					       !(regval & PWR_ON_MASK),
						TIMEOUT_US);
		if (ret)
			dev_err(&rdev->dev, "%s disable timed out: 0x%x\n",
				sc->rdesc.name, regval);
	} else {
		for (i = sc->clock_count-1; i >= 0; i--)
			clk_reset(sc->clocks[i], CLK_RESET_ASSERT);
		sc->resets_asserted = true;
	}

	if (sc->root_en) {
		for (i = sc->clock_count-1; i >= 0; i--)
			clk_disable_unprepare(sc->clocks[i]);
	}

	return ret;
}

static unsigned int gdsc_get_mode(struct regulator_dev *rdev)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval;

	regval = readl_relaxed(sc->gdscr);
	if (regval & HW_CONTROL_MASK)
		return REGULATOR_MODE_FAST;
	return REGULATOR_MODE_NORMAL;
}

static int gdsc_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval;
	int ret;

	regval = readl_relaxed(sc->gdscr);

	/*
	 * HW control can only be enable/disabled when SW_COLLAPSE
	 * indicates on.
	 */
	if (regval & SW_COLLAPSE_MASK) {
		dev_err(&rdev->dev, "can't enable hw collapse now\n");
		return -EBUSY;
	}

	switch (mode) {
	case REGULATOR_MODE_FAST:
		/* Turn on HW trigger mode */
		regval |= HW_CONTROL_MASK;
		writel_relaxed(regval, sc->gdscr);
		/*
		 * There may be a race with internal HW trigger signal,
		 * that will result in GDSC going through a power down and
		 * up cycle.  In case HW trigger signal is controlled by
		 * firmware that also poll same status bits as we do, FW
		 * might read an 'on' status before the GDSC can finish
		 * power cycle.  We wait 1us before returning to ensure
		 * FW can't immediately poll the status bit.
		 */
		mb();
		udelay(1);
		break;

	case REGULATOR_MODE_NORMAL:
		/* Turn off HW trigger mode */
		regval &= ~HW_CONTROL_MASK;
		writel_relaxed(regval, sc->gdscr);
		/*
		 * There may be a race with internal HW trigger signal,
		 * that will result in GDSC going through a power down and
		 * up cycle.  If we poll too early, status bit will
		 * indicate 'on' before the GDSC can finish the power cycle.
		 * Account for this case by waiting 1us before polling.
		 */
		mb();
		udelay(1);
		ret = readl_tight_poll_timeout(sc->gdscr, regval,
					regval & PWR_ON_MASK, TIMEOUT_US);
		if (ret) {
			dev_err(&rdev->dev, "%s set_mode timed out: 0x%x\n",
				sc->rdesc.name, regval);
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct regulator_ops gdsc_ops = {
	.is_enabled = gdsc_is_enabled,
	.enable = gdsc_enable,
	.disable = gdsc_disable,
	.set_mode = gdsc_set_mode,
	.get_mode = gdsc_get_mode,
};

static int gdsc_probe(struct platform_device *pdev)
{
	static atomic_t gdsc_count = ATOMIC_INIT(-1);
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data;
	struct resource *res;
	struct gdsc *sc;
	uint32_t regval;
	bool retain_mem, retain_periph, support_hw_trigger;
	int i, ret;

	sc = devm_kzalloc(&pdev->dev, sizeof(struct gdsc), GFP_KERNEL);
	if (sc == NULL)
		return -ENOMEM;

	init_data = of_get_regulator_init_data(&pdev->dev, pdev->dev.of_node);
	if (init_data == NULL)
		return -ENOMEM;

	if (of_get_property(pdev->dev.of_node, "parent-supply", NULL))
		init_data->supply_regulator = "parent";

	ret = of_property_read_string(pdev->dev.of_node, "regulator-name",
				      &sc->rdesc.name);
	if (ret)
		return ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL)
		return -EINVAL;
	sc->gdscr = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (sc->gdscr == NULL)
		return -ENOMEM;

	sc->clock_count = of_property_count_strings(pdev->dev.of_node,
					    "clock-names");
	if (sc->clock_count == -EINVAL) {
		sc->clock_count = 0;
	} else if (IS_ERR_VALUE(sc->clock_count)) {
		dev_err(&pdev->dev, "Failed to get clock names\n");
		return -EINVAL;
	}

	sc->clocks = devm_kzalloc(&pdev->dev,
			sizeof(struct clk *) * sc->clock_count, GFP_KERNEL);
	if (!sc->clocks)
		return -ENOMEM;

	sc->root_en = of_property_read_bool(pdev->dev.of_node,
						"qcom,enable-root-clk");

	for (i = 0; i < sc->clock_count; i++) {
		const char *clock_name;
		of_property_read_string_index(pdev->dev.of_node, "clock-names",
					      i, &clock_name);
		sc->clocks[i] = devm_clk_get(&pdev->dev, clock_name);
		if (IS_ERR(sc->clocks[i])) {
			int rc = PTR_ERR(sc->clocks[i]);
			if (rc != -EPROBE_DEFER)
				dev_err(&pdev->dev, "Failed to get %s\n",
					clock_name);
			return rc;
		}
	}

	sc->rdesc.id = atomic_inc_return(&gdsc_count);
	sc->rdesc.ops = &gdsc_ops;
	sc->rdesc.type = REGULATOR_VOLTAGE;
	sc->rdesc.owner = THIS_MODULE;
	platform_set_drvdata(pdev, sc);

	/*
	 * Disable HW trigger: collapse/restore occur based on registers writes.
	 * Disable SW override: Use hardware state-machine for sequencing.
	 */
	regval = readl_relaxed(sc->gdscr);
	regval &= ~(HW_CONTROL_MASK | SW_OVERRIDE_MASK);

	/* Configure wait time between states. */
	regval &= ~(EN_REST_WAIT_MASK | EN_FEW_WAIT_MASK | CLK_DIS_WAIT_MASK);
	regval |= EN_REST_WAIT_VAL | EN_FEW_WAIT_VAL | CLK_DIS_WAIT_VAL;
	writel_relaxed(regval, sc->gdscr);

	retain_mem = of_property_read_bool(pdev->dev.of_node,
					    "qcom,retain-mem");
	sc->toggle_mem = !retain_mem;
	retain_periph = of_property_read_bool(pdev->dev.of_node,
					    "qcom,retain-periph");
	sc->toggle_periph = !retain_periph;
	sc->toggle_logic = !of_property_read_bool(pdev->dev.of_node,
						"qcom,skip-logic-collapse");
	support_hw_trigger = of_property_read_bool(pdev->dev.of_node,
						    "qcom,support-hw-trigger");
	if (support_hw_trigger) {
		init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_MODE;
		init_data->constraints.valid_modes_mask |=
				REGULATOR_MODE_NORMAL | REGULATOR_MODE_FAST;
	}

	if (!sc->toggle_logic) {
		regval &= ~SW_COLLAPSE_MASK;
		writel_relaxed(regval, sc->gdscr);

		ret = readl_tight_poll_timeout(sc->gdscr, regval,
					regval & PWR_ON_MASK, TIMEOUT_US);
		if (ret) {
			dev_err(&pdev->dev, "%s enable timed out: 0x%x\n",
				sc->rdesc.name, regval);
			return ret;
		}
	}

	for (i = 0; i < sc->clock_count; i++) {
		if (retain_mem || (regval & PWR_ON_MASK))
			clk_set_flags(sc->clocks[i], CLKFLAG_RETAIN_MEM);
		else
			clk_set_flags(sc->clocks[i], CLKFLAG_NORETAIN_MEM);

		if (retain_periph || (regval & PWR_ON_MASK))
			clk_set_flags(sc->clocks[i], CLKFLAG_RETAIN_PERIPH);
		else
			clk_set_flags(sc->clocks[i], CLKFLAG_NORETAIN_PERIPH);
	}

	reg_config.dev = &pdev->dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = sc;
	reg_config.of_node = pdev->dev.of_node;
	sc->rdev = regulator_register(&sc->rdesc, &reg_config);
	if (IS_ERR(sc->rdev)) {
		dev_err(&pdev->dev, "regulator_register(\"%s\") failed.\n",
			sc->rdesc.name);
		return PTR_ERR(sc->rdev);
	}

	return 0;
}

static int gdsc_remove(struct platform_device *pdev)
{
	struct gdsc *sc = platform_get_drvdata(pdev);
	regulator_unregister(sc->rdev);
	return 0;
}

static struct of_device_id gdsc_match_table[] = {
	{ .compatible = "qcom,gdsc" },
	{}
};

static struct platform_driver gdsc_driver = {
	.probe		= gdsc_probe,
	.remove		= gdsc_remove,
	.driver		= {
		.name		= "gdsc",
		.of_match_table = gdsc_match_table,
		.owner		= THIS_MODULE,
	},
};

static int __init gdsc_init(void)
{
	return platform_driver_register(&gdsc_driver);
}
subsys_initcall(gdsc_init);

static void __exit gdsc_exit(void)
{
	platform_driver_unregister(&gdsc_driver);
}
module_exit(gdsc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM8974 GDSC power rail regulator driver");
