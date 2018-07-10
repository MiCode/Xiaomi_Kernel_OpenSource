/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "gdsc: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/mfd/syscon.h>
#include <linux/clk/qcom.h>

/* GDSCR */
#define PWR_ON_MASK		BIT(31)
#define CLK_DIS_WAIT_MASK	(0xF << 12)
#define CLK_DIS_WAIT_SHIFT	(12)
#define EN_FEW_WAIT_MASK	(0xF << 16)
#define EN_FEW_WAIT_SHIFT	(16)
#define EN_REST_WAIT_MASK	(0xF << 20)
#define EN_REST_WAIT_SHIFT	(20)
#define SW_OVERRIDE_MASK	BIT(2)
#define HW_CONTROL_MASK		BIT(1)
#define SW_COLLAPSE_MASK	BIT(0)

/* CFG_GDSCR */
#define GDSC_POWER_UP_COMPLETE		BIT(16)
#define GDSC_POWER_DOWN_COMPLETE	BIT(15)

/* Domain Address */
#define GMEM_CLAMP_IO_MASK	BIT(0)
#define GMEM_RESET_MASK         BIT(4)

/* SW Reset */
#define BCR_BLK_ARES_BIT	BIT(0)

/* Register Offset */
#define REG_OFFSET		0x0
#define CFG_GDSCR_OFFSET	0x4

/* Timeout Delay */
#define TIMEOUT_US		100

/* TOGGLE SW COLLAPSE */
#define TOGGLE_SW_COLLAPSE_IN_DISABLE	BIT(0)

struct gdsc {
	struct regulator_dev	*rdev;
	struct regulator_desc	rdesc;
	void __iomem		*gdscr;
	struct regmap           *regmap;
	struct regmap           *domain_addr;
	struct regmap           *hw_ctrl;
	struct regmap           *sw_reset;
	struct clk		**clocks;
	struct reset_control	**reset_clocks;
	bool			toggle_mem;
	bool			toggle_periph;
	bool			toggle_logic;
	bool			resets_asserted;
	bool			root_en;
	bool			force_root_en;
	bool			no_status_check_on_disable;
	bool			is_gdsc_enabled;
	bool			allow_clear;
	bool			reset_aon;
	bool			poll_cfg_gdscr;
	int			clock_count;
	int			reset_count;
	int			root_clk_idx;
	u32			gds_timeout;
	u32			flags;
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
	struct regmap *regmap;
	int count = sc->gds_timeout;
	u32 val;

	if (sc->hw_ctrl)
		regmap = sc->hw_ctrl;
	else
		regmap = sc->regmap;

	for (; count > 0; count--) {
		regmap_read(regmap, REG_OFFSET, &val);
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

static int poll_cfg_gdsc_status(struct gdsc *sc, enum gdscr_status status)
{
	struct regmap *regmap = sc->regmap;
	int count = sc->gds_timeout;
	u32 val;

	for (; count > 0; count--) {
		regmap_read(regmap, CFG_GDSCR_OFFSET, &val);

		switch (status) {
		case ENABLED:
			if (val & GDSC_POWER_UP_COMPLETE)
				return 0;
			break;
		case DISABLED:
			if (val & GDSC_POWER_DOWN_COMPLETE)
				return 0;
			break;
		}
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

	regmap_read(sc->regmap, REG_OFFSET, &regval);

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
	uint32_t regval, cfg_regval, hw_ctrl_regval = 0x0;
	int i, ret = 0;

	mutex_lock(&gdsc_seq_lock);

	if (sc->root_en || sc->force_root_en)
		clk_prepare_enable(sc->clocks[sc->root_clk_idx]);

	regmap_read(sc->regmap, REG_OFFSET, &regval);
	if (regval & HW_CONTROL_MASK) {
		dev_warn(&rdev->dev, "Invalid enable while %s is under HW control\n",
				sc->rdesc.name);
		mutex_unlock(&gdsc_seq_lock);
		return -EBUSY;
	}

	if (sc->toggle_logic) {
		if (sc->sw_reset) {
			regmap_read(sc->sw_reset, REG_OFFSET, &regval);
			regval |= BCR_BLK_ARES_BIT;
			regmap_write(sc->sw_reset, REG_OFFSET, regval);
			/*
			 * BLK_ARES should be kept asserted for 1us before
			 * being de-asserted.
			 */
			wmb();
			udelay(1);

			regval &= ~BCR_BLK_ARES_BIT;
			regmap_write(sc->sw_reset, REG_OFFSET, regval);
			/* Make sure de-assert goes through before continuing */
			wmb();
		}

		if (sc->domain_addr) {
			if (sc->reset_aon) {
				regmap_read(sc->domain_addr, REG_OFFSET,
								&regval);
				regval |= GMEM_RESET_MASK;
				regmap_write(sc->domain_addr, REG_OFFSET,
								regval);
				/*
				 * Keep reset asserted for at-least 1us before
				 * continuing.
				 */
				wmb();
				udelay(1);

				regval &= ~GMEM_RESET_MASK;
				regmap_write(sc->domain_addr, REG_OFFSET,
							regval);
				/*
				 * Make sure GMEM_RESET is de-asserted before
				 * continuing.
				 */
				wmb();
			}

			regmap_read(sc->domain_addr, REG_OFFSET, &regval);
			regval &= ~GMEM_CLAMP_IO_MASK;
			regmap_write(sc->domain_addr, REG_OFFSET, regval);

			/*
			 * Make sure CLAMP_IO is de-asserted before continuing.
			 */
			wmb();
		}

		regmap_read(sc->regmap, REG_OFFSET, &regval);
		regval &= ~SW_COLLAPSE_MASK;
		regmap_write(sc->regmap, REG_OFFSET, regval);

		/* Wait for 8 XO cycles before polling the status bit. */
		mb();
		udelay(1);

		if (sc->poll_cfg_gdscr)
			ret = poll_cfg_gdsc_status(sc, ENABLED);
		else
			ret = poll_gdsc_status(sc, ENABLED);
		if (ret) {
			regmap_read(sc->regmap, REG_OFFSET, &regval);

			if (sc->hw_ctrl) {
				regmap_read(sc->hw_ctrl, REG_OFFSET,
						&hw_ctrl_regval);
				dev_warn(&rdev->dev, "%s state (after %d us timeout): 0x%x, GDS_HW_CTRL: 0x%x. Re-polling.\n",
					sc->rdesc.name, sc->gds_timeout,
					regval, hw_ctrl_regval);

				ret = poll_gdsc_status(sc, ENABLED);
				if (ret) {
					regmap_read(sc->regmap, REG_OFFSET,
								&regval);
					regmap_read(sc->hw_ctrl, REG_OFFSET,
							&hw_ctrl_regval);
					dev_err(&rdev->dev, "%s final state (after additional %d us timeout): 0x%x, GDS_HW_CTRL: 0x%x\n",
						sc->rdesc.name, sc->gds_timeout,
						regval, hw_ctrl_regval);

					mutex_unlock(&gdsc_seq_lock);
					return ret;
				}
			} else {
				dev_err(&rdev->dev, "%s enable timed out: 0x%x\n",
					sc->rdesc.name,
					regval);
				udelay(sc->gds_timeout);

				if (sc->poll_cfg_gdscr) {
					regmap_read(sc->regmap, REG_OFFSET,
							&regval);
					regmap_read(sc->regmap,
						CFG_GDSCR_OFFSET, &cfg_regval);
					dev_err(&rdev->dev, "%s final state: gdscr - 0x%x, cfg_gdscr - 0x%x (%d us after timeout)\n",
						sc->rdesc.name, regval,
						cfg_regval, sc->gds_timeout);
				} else {
					regmap_read(sc->regmap, REG_OFFSET,
							&regval);
					dev_err(&rdev->dev, "%s final state: 0x%x (%d us after timeout)\n",
						sc->rdesc.name, regval,
						sc->gds_timeout);
				}
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

	for (i = sc->clock_count - 1; i >= 0; i--) {
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
		regmap_read(sc->regmap, REG_OFFSET, &regval);
		regval |= SW_COLLAPSE_MASK;
		regmap_write(sc->regmap, REG_OFFSET, regval);

		if (sc->flags & TOGGLE_SW_COLLAPSE_IN_DISABLE) {
			regval &= ~SW_COLLAPSE_MASK;
			regmap_write(sc->regmap, REG_OFFSET, regval);
			regval |= SW_COLLAPSE_MASK;
			regmap_write(sc->regmap, REG_OFFSET, regval);
		}

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
			if (sc->poll_cfg_gdscr)
				ret = poll_cfg_gdsc_status(sc, DISABLED);
			else
				ret = poll_gdsc_status(sc, DISABLED);
			if (ret)
				dev_err(&rdev->dev, "%s disable timed out: 0x%x\n",
					sc->rdesc.name, regval);
		}

		if (sc->domain_addr) {
			regmap_read(sc->domain_addr, REG_OFFSET, &regval);
			regval |= GMEM_CLAMP_IO_MASK;
			regmap_write(sc->domain_addr, REG_OFFSET, regval);
		}

	} else {
		for (i = sc->reset_count - 1; i >= 0; i--)
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
	regmap_read(sc->regmap, REG_OFFSET, &regval);
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

	regmap_read(sc->regmap, REG_OFFSET, &regval);

	switch (mode) {
	case REGULATOR_MODE_FAST:
		/* Turn on HW trigger mode */
		regval |= HW_CONTROL_MASK;
		regmap_write(sc->regmap, REG_OFFSET, regval);
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
		regmap_write(sc->regmap, REG_OFFSET, regval);
		/*
		 * There may be a race with internal HW trigger signal,
		 * that will result in GDSC going through a power down and
		 * up cycle.  If we poll too early, status bit will
		 * indicate 'on' before the GDSC can finish the power cycle.
		 * Account for this case by waiting 1us before polling.
		 */
		mb();
		udelay(1);

		if (sc->poll_cfg_gdscr)
			ret = poll_cfg_gdsc_status(sc, ENABLED);
		else
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

static const struct regmap_config gdsc_regmap_config = {
	.reg_bits   = 32,
	.reg_stride = 4,
	.val_bits   = 32,
	.fast_io    = true,
};

static int gdsc_probe(struct platform_device *pdev)
{
	static atomic_t gdsc_count = ATOMIC_INIT(-1);
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data;
	struct resource *res;
	struct gdsc *sc;
	uint32_t regval, clk_dis_wait_val = 0;
	uint32_t en_few_wait_val, en_rest_wait_val;
	bool retain_mem, retain_periph, support_hw_trigger, prop_val;
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
	if (res == NULL) {
		dev_err(&pdev->dev, "Failed to get resources\n");
		return -EINVAL;
	}

	sc->gdscr = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (sc->gdscr == NULL)
		return -ENOMEM;

	sc->regmap = devm_regmap_init_mmio(&pdev->dev, sc->gdscr,
							&gdsc_regmap_config);
	if (!sc->regmap) {
		dev_err(&pdev->dev, "Couldn't get regmap\n");
		return -EINVAL;
	}

	if (of_find_property(pdev->dev.of_node, "domain-addr", NULL)) {
		sc->domain_addr = syscon_regmap_lookup_by_phandle
					(pdev->dev.of_node, "domain-addr");
		if (IS_ERR(sc->domain_addr))
			return -ENODEV;
	}

	if (of_find_property(pdev->dev.of_node, "sw-reset", NULL)) {
		sc->sw_reset = syscon_regmap_lookup_by_phandle
						(pdev->dev.of_node, "sw-reset");
		if (IS_ERR(sc->sw_reset))
			return -ENODEV;
	}

	if (of_find_property(pdev->dev.of_node, "hw-ctrl-addr", NULL)) {
		sc->hw_ctrl = syscon_regmap_lookup_by_phandle(
					pdev->dev.of_node, "hw-ctrl-addr");
		if (IS_ERR(sc->hw_ctrl))
			return -ENODEV;
	}

	sc->poll_cfg_gdscr = of_property_read_bool(pdev->dev.of_node,
						"qcom,poll-cfg-gdscr");

	sc->gds_timeout = TIMEOUT_US;

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,gds-timeout",
							&timeout);
	if (!ret)
		sc->gds_timeout = timeout;

	sc->clock_count = of_property_count_strings(pdev->dev.of_node,
					    "clock-names");
	if (sc->clock_count == -EINVAL) {
		sc->clock_count = 0;
	} else if (sc->clock_count < 0) {
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

	prop_val = of_property_read_bool(pdev->dev.of_node,
						"qcom,toggle-sw-collapse-in-disable");
	if (prop_val)
		sc->flags |= TOGGLE_SW_COLLAPSE_IN_DISABLE;

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

	sc->reset_aon = of_property_read_bool(pdev->dev.of_node,
							"qcom,reset-aon-logic");

	sc->rdesc.id = atomic_inc_return(&gdsc_count);
	sc->rdesc.ops = &gdsc_ops;
	sc->rdesc.type = REGULATOR_VOLTAGE;
	sc->rdesc.owner = THIS_MODULE;
	platform_set_drvdata(pdev, sc);

	/*
	 * Disable HW trigger: collapse/restore occur based on registers writes.
	 * Disable SW override: Use hardware state-machine for sequencing.
	 */
	regmap_read(sc->regmap, REG_OFFSET, &regval);
	regval &= ~(HW_CONTROL_MASK | SW_OVERRIDE_MASK);

	if (!of_property_read_u32(pdev->dev.of_node, "qcom,clk-dis-wait-val",
				  &clk_dis_wait_val)) {
		clk_dis_wait_val = clk_dis_wait_val << CLK_DIS_WAIT_SHIFT;

		/* Configure wait time between states. */
		regval &= ~(CLK_DIS_WAIT_MASK);
		regval |= clk_dis_wait_val;
	}

	if (!of_property_read_u32(pdev->dev.of_node, "qcom,en-few-wait-val",
				  &en_few_wait_val)) {
		en_few_wait_val <<= EN_FEW_WAIT_SHIFT;

		regval &= ~(EN_FEW_WAIT_MASK);
		regval |= en_few_wait_val;
	}

	if (!of_property_read_u32(pdev->dev.of_node, "qcom,en-rest-wait-val",
				  &en_rest_wait_val)) {
		en_rest_wait_val <<= EN_REST_WAIT_SHIFT;

		regval &= ~(EN_REST_WAIT_MASK);
		regval |= en_rest_wait_val;
	}

	regmap_write(sc->regmap, REG_OFFSET, regval);

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
		} else if (sc->reset_count < 0) {
			dev_err(&pdev->dev, "Failed to get reset clock names\n");
			return -EINVAL;
		}

		sc->reset_clocks = devm_kzalloc(&pdev->dev,
			sizeof(struct reset_control *) * sc->reset_count,
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
		regmap_write(sc->regmap, REG_OFFSET, regval);

		if (sc->poll_cfg_gdscr)
			ret = poll_cfg_gdsc_status(sc, ENABLED);
		else
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
	reg_config.regmap = sc->regmap;

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

static const struct of_device_id gdsc_match_table[] = {
	{ .compatible = "qcom,gdsc" },
	{}
};

static struct platform_driver gdsc_driver = {
	.probe  = gdsc_probe,
	.remove = gdsc_remove,
	.driver = {
		.name = "gdsc",
		.of_match_table = gdsc_match_table,
		.owner = THIS_MODULE,
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
