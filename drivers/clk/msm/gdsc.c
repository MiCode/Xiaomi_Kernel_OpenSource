/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/reset.h>
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
#define GMEM_CLAMP_IO_MASK	BIT(0)
#define GMEM_RESET_MASK		BIT(4)
#define BCR_BLK_ARES_BIT	BIT(0)

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
	struct reset_control	**reset_clocks;
	int			clock_count;
	int			reset_count;
	bool			toggle_mem;
	bool			toggle_periph;
	bool			toggle_logic;
	bool			resets_asserted;
	bool			root_en;
	bool			force_root_en;
	int			root_clk_idx;
	bool			no_status_check_on_disable;
	bool			is_gdsc_enabled;
	bool			allow_clear;
	bool			reset_aon;
	void __iomem		*domain_addr;
	void __iomem		*hw_ctrl_addr;
	void __iomem		*sw_reset_addr;
	u32			gds_timeout;
};

enum gdscr_status {
	ENABLED,
	DISABLED,
};

static DEFINE_MUTEX(gdsc_seq_lock);

void gdsc_allow_clear_retention(struct regulator *regulator)
{
	struct gdsc *sc = regulator_get_drvdata(regulator);

	if (sc)
		sc->allow_clear = true;
}

static int poll_gdsc_status(struct gdsc *sc, enum gdscr_status status)
{
	void __iomem *gdscr;
	int count = sc->gds_timeout;
	u32 val;

	if (sc->hw_ctrl_addr)
		gdscr = sc->hw_ctrl_addr;
	else
		gdscr = sc->gdscr;

	for (; count > 0; count--) {
		val = readl_relaxed(gdscr);
		val &= PWR_ON_MASK;
		switch (status) {
		case ENABLED:
			if (val)
				return 0;
			break;
		case DISABLED:
			if (!val)
				return 0;
			break;
		}
		/*
		 * There is no guarantee about the delay needed for the enable
		 * bit in the GDSCR to be set or reset after the GDSC state
		 * changes. Hence, keep on checking for a reasonable number
		 * of times until the bit is set with the least possible delay
		 * between succeessive tries.
		 */
		udelay(1);
	}
	return -ETIMEDOUT;
}

static int gdsc_is_enabled(struct regulator_dev *rdev)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval;

	if (!sc->toggle_logic)
		return !sc->resets_asserted;

	regval = readl_relaxed(sc->gdscr);
	if (regval & PWR_ON_MASK) {
		/*
		 * The GDSC might be turned on due to TZ/HYP vote on the
		 * votable GDS registers. Check the SW_COLLAPSE_MASK to
		 * determine if HLOS has voted for it.
		 */
		if (!(regval & SW_COLLAPSE_MASK))
			return true;
	}
	return false;
}

static int gdsc_enable(struct regulator_dev *rdev)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval, hw_ctrl_regval = 0x0;
	int i, ret = 0;

	mutex_lock(&gdsc_seq_lock);

	if (sc->root_en || sc->force_root_en)
		clk_prepare_enable(sc->clocks[sc->root_clk_idx]);

	if (sc->toggle_logic) {
		if (sc->sw_reset_addr) {
			regval = readl_relaxed(sc->sw_reset_addr);
			regval |= BCR_BLK_ARES_BIT;
			writel_relaxed(regval, sc->sw_reset_addr);
			/*
			 * BLK_ARES should be kept asserted for 1us before
			 * being de-asserted.
			 */
			wmb();
			udelay(1);

			regval &= ~BCR_BLK_ARES_BIT;
			writel_relaxed(regval, sc->sw_reset_addr);

			/* Make sure de-assert goes through before continuing */
			wmb();
		}

		if (sc->domain_addr) {
			if (sc->reset_aon) {
				regval = readl_relaxed(sc->domain_addr);
				regval |= GMEM_RESET_MASK;
				writel_relaxed(regval, sc->domain_addr);
				/*
				 * Keep reset asserted for at-least 1us before
				 * continuing.
				 */
				wmb();
				udelay(1);

				regval &= ~GMEM_RESET_MASK;
				writel_relaxed(regval, sc->domain_addr);
				/*
				 * Make sure GMEM_RESET is de-asserted before
				 * continuing.
				 */
				wmb();
			}

			regval = readl_relaxed(sc->domain_addr);
			regval &= ~GMEM_CLAMP_IO_MASK;
			writel_relaxed(regval, sc->domain_addr);
			/*
			 * Make sure CLAMP_IO is de-asserted before continuing.
			 */
			wmb();
		}

		regval = readl_relaxed(sc->gdscr);
		if (regval & HW_CONTROL_MASK) {
			dev_warn(&rdev->dev, "Invalid enable while %s is under HW control\n",
				 sc->rdesc.name);
			mutex_unlock(&gdsc_seq_lock);
			return -EBUSY;
		}

		regval &= ~SW_COLLAPSE_MASK;
		writel_relaxed(regval, sc->gdscr);

		/* Wait for 8 XO cycles before polling the status bit. */
		mb();
		udelay(1);

		ret = poll_gdsc_status(sc, ENABLED);
		if (ret) {
			regval = readl_relaxed(sc->gdscr);
			if (sc->hw_ctrl_addr) {
				hw_ctrl_regval =
					readl_relaxed(sc->hw_ctrl_addr);
				dev_warn(&rdev->dev, "%s state (after %d us timeout): 0x%x, GDS_HW_CTRL: 0x%x. Re-polling.\n",
					sc->rdesc.name, sc->gds_timeout,
					regval, hw_ctrl_regval);

				ret = poll_gdsc_status(sc, ENABLED);
				if (ret) {
					dev_err(&rdev->dev, "%s final state (after additional %d us timeout): 0x%x, GDS_HW_CTRL: 0x%x\n",
					sc->rdesc.name, sc->gds_timeout,
					readl_relaxed(sc->gdscr),
					readl_relaxed(sc->hw_ctrl_addr));

					mutex_unlock(&gdsc_seq_lock);
					return ret;
				}
			} else {
				dev_err(&rdev->dev, "%s enable timed out: 0x%x\n",
					sc->rdesc.name,
					regval);
				udelay(sc->gds_timeout);
				regval = readl_relaxed(sc->gdscr);
				dev_err(&rdev->dev, "%s final state: 0x%x (%d us after timeout)\n",
					sc->rdesc.name, regval,
					sc->gds_timeout);
				mutex_unlock(&gdsc_seq_lock);
				return ret;
			}
		}
	} else {
		for (i = 0; i < sc->reset_count; i++)
			reset_control_deassert(sc->reset_clocks[i]);
		sc->resets_asserted = false;
	}

	for (i = 0; i < sc->clock_count; i++) {
		if (unlikely(i == sc->root_clk_idx))
			continue;
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

	/* Delay to account for staggered memory powerup. */
	udelay(1);

	if (sc->force_root_en)
		clk_disable_unprepare(sc->clocks[sc->root_clk_idx]);
	sc->is_gdsc_enabled = true;

	mutex_unlock(&gdsc_seq_lock);

	return ret;
}

static int gdsc_disable(struct regulator_dev *rdev)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval;
	int i, ret = 0;

	mutex_lock(&gdsc_seq_lock);

	if (sc->force_root_en)
		clk_prepare_enable(sc->clocks[sc->root_clk_idx]);

	for (i = sc->clock_count-1; i >= 0; i--) {
		if (unlikely(i == sc->root_clk_idx))
			continue;
		if (sc->toggle_mem && sc->allow_clear)
			clk_set_flags(sc->clocks[i], CLKFLAG_NORETAIN_MEM);
		if (sc->toggle_periph && sc->allow_clear)
			clk_set_flags(sc->clocks[i], CLKFLAG_NORETAIN_PERIPH);
	}

	/* Delay to account for staggered memory powerdown. */
	udelay(1);

	if (sc->toggle_logic) {
		regval = readl_relaxed(sc->gdscr);
		if (regval & HW_CONTROL_MASK) {
			dev_warn(&rdev->dev, "Invalid disable while %s is under HW control\n",
				 sc->rdesc.name);
			mutex_unlock(&gdsc_seq_lock);
			return -EBUSY;
		}

		regval |= SW_COLLAPSE_MASK;
		writel_relaxed(regval, sc->gdscr);
		/* Wait for 8 XO cycles before polling the status bit. */
		mb();
		udelay(1);

		if (sc->no_status_check_on_disable) {
			/*
			 * Add a short delay here to ensure that gdsc_enable
			 * right after it was disabled does not put it in a
			 * weird state.
			 */
			udelay(TIMEOUT_US);
		} else {
			ret = poll_gdsc_status(sc, DISABLED);
			if (ret)
				dev_err(&rdev->dev, "%s disable timed out: 0x%x\n",
					sc->rdesc.name, regval);
		}

		if (sc->domain_addr) {
			regval = readl_relaxed(sc->domain_addr);
			regval |= GMEM_CLAMP_IO_MASK;
			writel_relaxed(regval, sc->domain_addr);
			/* Make sure CLAMP_IO is asserted before continuing. */
			wmb();
		}
	} else {
		for (i = sc->reset_count-1; i >= 0; i--)
			reset_control_assert(sc->reset_clocks[i]);
		sc->resets_asserted = true;
	}

	/*
	 * Check if gdsc_enable was called for this GDSC. If not, the root
	 * clock will not have been enabled prior to this.
	 */
	if ((sc->is_gdsc_enabled && sc->root_en) || sc->force_root_en)
		clk_disable_unprepare(sc->clocks[sc->root_clk_idx]);
	sc->is_gdsc_enabled = false;

	mutex_unlock(&gdsc_seq_lock);

	return ret;
}

static unsigned int gdsc_get_mode(struct regulator_dev *rdev)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval;

	mutex_lock(&gdsc_seq_lock);
	regval = readl_relaxed(sc->gdscr);
	mutex_unlock(&gdsc_seq_lock);
	if (regval & HW_CONTROL_MASK)
		return REGULATOR_MODE_FAST;
	return REGULATOR_MODE_NORMAL;
}

static int gdsc_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval;
	int ret = 0;

	mutex_lock(&gdsc_seq_lock);

	regval = readl_relaxed(sc->gdscr);

	/*
	 * HW control can only be enable/disabled when SW_COLLAPSE
	 * indicates on.
	 */
	if (regval & SW_COLLAPSE_MASK) {
		dev_err(&rdev->dev, "can't enable hw collapse now\n");
		mutex_unlock(&gdsc_seq_lock);
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

		ret = poll_gdsc_status(sc, ENABLED);
		if (ret)
			dev_err(&rdev->dev, "%s set_mode timed out: 0x%x\n",
				sc->rdesc.name, regval);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&gdsc_seq_lock);

	return ret;
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
	uint32_t regval, clk_dis_wait_val = CLK_DIS_WAIT_VAL;
	bool retain_mem, retain_periph, support_hw_trigger;
	int i, ret;
	u32 timeout;

	sc = devm_kzalloc(&pdev->dev, sizeof(struct gdsc), GFP_KERNEL);
	if (sc == NULL)
		return -ENOMEM;

	init_data = of_get_regulator_init_data(&pdev->dev, pdev->dev.of_node,
			&sc->rdesc);
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

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"domain_addr");
	if (res) {
		sc->domain_addr = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
		if (sc->domain_addr == NULL)
			return -ENOMEM;
	}

	sc->reset_aon = of_property_read_bool(pdev->dev.of_node,
						"qcom,reset-aon-logic");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"sw_reset");
	if (res) {
		sc->sw_reset_addr = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
		if (sc->sw_reset_addr == NULL)
			return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"hw_ctrl_addr");
	if (res) {
		sc->hw_ctrl_addr = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
		if (sc->hw_ctrl_addr == NULL)
			return -ENOMEM;
	}

	sc->gds_timeout = TIMEOUT_US;
	ret = of_property_read_u32(pdev->dev.of_node, "qcom,gds-timeout",
							&timeout);
	if (!ret)
		sc->gds_timeout = timeout;

	sc->clock_count = of_property_count_strings(pdev->dev.of_node,
					    "clock-names");
	if (sc->clock_count == -EINVAL) {
		sc->clock_count = 0;
	} else if (IS_ERR_VALUE((unsigned long)sc->clock_count)) {
		dev_err(&pdev->dev, "Failed to get clock names\n");
		return -EINVAL;
	}

	sc->clocks = devm_kzalloc(&pdev->dev,
			sizeof(struct clk *) * sc->clock_count, GFP_KERNEL);
	if (!sc->clocks)
		return -ENOMEM;

	sc->root_clk_idx = -1;

	sc->root_en = of_property_read_bool(pdev->dev.of_node,
						"qcom,enable-root-clk");
	sc->force_root_en = of_property_read_bool(pdev->dev.of_node,
						"qcom,force-enable-root-clk");
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

		if (!strcmp(clock_name, "core_root_clk"))
			sc->root_clk_idx = i;
	}

	if ((sc->root_en || sc->force_root_en) && (sc->root_clk_idx == -1)) {
		dev_err(&pdev->dev, "Failed to get root clock name\n");
		return -EINVAL;
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

	if (!of_property_read_u32(pdev->dev.of_node, "qcom,clk-dis-wait-val",
				  &clk_dis_wait_val))
		clk_dis_wait_val = clk_dis_wait_val << 12;

	/* Configure wait time between states. */
	regval &= ~(EN_REST_WAIT_MASK | EN_FEW_WAIT_MASK | CLK_DIS_WAIT_MASK);
	regval |= EN_REST_WAIT_VAL | EN_FEW_WAIT_VAL | clk_dis_wait_val;
	writel_relaxed(regval, sc->gdscr);

	sc->no_status_check_on_disable =
			of_property_read_bool(pdev->dev.of_node,
					"qcom,no-status-check-on-disable");
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
		sc->reset_count = of_property_count_strings(pdev->dev.of_node,
							"reset-names");
		if (sc->reset_count == -EINVAL) {
			sc->reset_count = 0;
		} else if (IS_ERR_VALUE((unsigned long)sc->reset_count)) {
			dev_err(&pdev->dev, "Failed to get reset reset names\n");
			return -EINVAL;
		}

		sc->reset_clocks = devm_kzalloc(&pdev->dev,
					sizeof(struct reset_control *) *
					sc->reset_count,
					GFP_KERNEL);
		if (!sc->reset_clocks)
			return -ENOMEM;

		for (i = 0; i < sc->reset_count; i++) {
			const char *reset_name;

			of_property_read_string_index(pdev->dev.of_node,
					"reset-names", i, &reset_name);
			sc->reset_clocks[i] = devm_reset_control_get(&pdev->dev,
								reset_name);
			if (IS_ERR(sc->reset_clocks[i])) {
				int rc = PTR_ERR(sc->reset_clocks[i]);

				if (rc != -EPROBE_DEFER)
					dev_err(&pdev->dev, "Failed to get %s\n",
							reset_name);
				return rc;
			}
		}

		regval &= ~SW_COLLAPSE_MASK;
		writel_relaxed(regval, sc->gdscr);

		ret = poll_gdsc_status(sc, ENABLED);
		if (ret) {
			dev_err(&pdev->dev, "%s enable timed out: 0x%x\n",
				sc->rdesc.name, regval);
			return ret;
		}
	}

	sc->allow_clear = of_property_read_bool(pdev->dev.of_node,
							"qcom,disallow-clear");
	sc->allow_clear = !sc->allow_clear;

	for (i = 0; i < sc->clock_count; i++) {
		if (retain_mem || (regval & PWR_ON_MASK) || !sc->allow_clear)
			clk_set_flags(sc->clocks[i], CLKFLAG_RETAIN_MEM);
		else
			clk_set_flags(sc->clocks[i], CLKFLAG_NORETAIN_MEM);

		if (retain_periph || (regval & PWR_ON_MASK) || !sc->allow_clear)
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

static const  struct of_device_id gdsc_match_table[] = {
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
