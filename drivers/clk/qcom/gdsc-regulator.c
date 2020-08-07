// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/msm-bus.h>
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

#include <dt-bindings/regulator/qcom,rpmh-regulator-levels.h>

/* GDSCR */
#define PWR_ON_MASK		BIT(31)
#define CLK_DIS_WAIT_MASK	(0xF << 12)
#define CLK_DIS_WAIT_SHIFT	(12)
#define RETAIN_FF_ENABLE_MASK	BIT(11)
#define SW_OVERRIDE_MASK	BIT(2)
#define HW_CONTROL_MASK		BIT(1)
#define SW_COLLAPSE_MASK	BIT(0)

/* Domain Address */
#define GMEM_CLAMP_IO_MASK	BIT(0)
#define GMEM_RESET_MASK         BIT(4)

/* SW Reset */
#define BCR_BLK_ARES_BIT	BIT(0)

/* Register Offset */
#define REG_OFFSET		0x0

/* Timeout Delay */
#define TIMEOUT_US		100

struct gdsc {
	struct regulator_dev	*rdev;
	struct regulator_desc	rdesc;
	void __iomem		*gdscr;
	struct regmap           *regmap;
	struct regmap           *domain_addr;
	struct regmap           *hw_ctrl;
	struct regmap           *sw_reset;
	struct clk		**clocks;
	struct regulator	*parent_regulator;
	struct reset_control	**reset_clocks;
	struct msm_bus_scale_pdata *bus_pdata;
	u32			bus_handle;
	bool			toggle_mem;
	bool			toggle_periph;
	bool			toggle_logic;
	bool			retain_ff_enable;
	bool			resets_asserted;
	bool			root_en;
	bool			force_root_en;
	bool			no_status_check_on_disable;
	bool			is_gdsc_enabled;
	bool			allow_clear;
	bool			reset_aon;
	bool			is_bus_enabled;
	int			clock_count;
	int			reset_count;
	int			root_clk_idx;
	u32			gds_timeout;
	bool			skip_disable_before_enable;
};

enum gdscr_status {
	ENABLED,
	DISABLED,
};

static inline u32 gdsc_mb(struct gdsc *gds)
{
	u32 reg;

	regmap_read(gds->regmap, REG_OFFSET, &reg);
	return reg;
}

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
		 * between successive tries.
		 */
		udelay(1);
	}

	return -ETIMEDOUT;
}

static int gdsc_is_enabled(struct regulator_dev *rdev)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval;
	int ret;
	bool is_enabled = false;

	if (!sc->toggle_logic)
		return !sc->resets_asserted;

	if (sc->skip_disable_before_enable)
		return false;

	if (sc->parent_regulator) {
		/*
		 * The parent regulator for the GDSC is required to be on to
		 * make any register accesses to the GDSC base. Return false
		 * if the parent supply is disabled.
		 */
		if (regulator_is_enabled(sc->parent_regulator) <= 0)
			return false;

		/*
		 * Place an explicit vote on the parent rail to cover cases when
		 * it might be disabled between this point and reading the GDSC
		 * registers.
		 */
		if (regulator_set_voltage(sc->parent_regulator,
					RPMH_REGULATOR_LEVEL_LOW_SVS, INT_MAX))
			return false;

		if (regulator_enable(sc->parent_regulator)) {
			regulator_set_voltage(sc->parent_regulator, 0, INT_MAX);
			return false;
		}
	}

	if (sc->bus_handle && !sc->is_bus_enabled) {
		ret = msm_bus_scale_client_update_request(sc->bus_handle, 1);
		if (ret) {
			dev_err(&rdev->dev, "bus scaling failed, ret=%d\n",
				ret);
			goto end;
		}
	}

	regmap_read(sc->regmap, REG_OFFSET, &regval);

	if (regval & PWR_ON_MASK) {
		/*
		 * The GDSC might be turned on due to TZ/HYP vote on the
		 * votable GDS registers. Check the SW_COLLAPSE_MASK to
		 * determine if HLOS has voted for it.
		 */
		if (!(regval & SW_COLLAPSE_MASK))
			is_enabled = true;
	}


	if (sc->bus_handle && !sc->is_bus_enabled)
		msm_bus_scale_client_update_request(sc->bus_handle, 0);
end:
	if (sc->parent_regulator) {
		regulator_disable(sc->parent_regulator);
		regulator_set_voltage(sc->parent_regulator, 0, INT_MAX);
	}

	return is_enabled;
}

static int gdsc_enable(struct regulator_dev *rdev)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval, hw_ctrl_regval = 0x0;
	int i, ret = 0;

	if (sc->skip_disable_before_enable)
		return 0;

	if (sc->parent_regulator) {
		ret = regulator_set_voltage(sc->parent_regulator,
				RPMH_REGULATOR_LEVEL_LOW_SVS, INT_MAX);
		if (ret)
			return ret;
	}

	if (sc->bus_handle) {
		ret = msm_bus_scale_client_update_request(sc->bus_handle, 1);
		if (ret) {
			dev_err(&rdev->dev, "bus scaling failed, ret=%d\n",
				ret);
			goto end;
		}
		sc->is_bus_enabled = true;
	}

	if (sc->root_en || sc->force_root_en)
		clk_prepare_enable(sc->clocks[sc->root_clk_idx]);

	regmap_read(sc->regmap, REG_OFFSET, &regval);
	if (regval & HW_CONTROL_MASK) {
		dev_warn(&rdev->dev, "Invalid enable while %s is under HW control\n",
				sc->rdesc.name);
		ret = -EBUSY;
		goto end;
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
			gdsc_mb(sc);
			udelay(1);

			regval &= ~BCR_BLK_ARES_BIT;
			regmap_write(sc->sw_reset, REG_OFFSET, regval);
			/* Make sure de-assert goes through before continuing */
			gdsc_mb(sc);
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
				gdsc_mb(sc);
				udelay(1);

				regval &= ~GMEM_RESET_MASK;
				regmap_write(sc->domain_addr, REG_OFFSET,
							regval);
				/*
				 * Make sure GMEM_RESET is de-asserted before
				 * continuing.
				 */
				gdsc_mb(sc);
			}

			regmap_read(sc->domain_addr, REG_OFFSET, &regval);
			regval &= ~GMEM_CLAMP_IO_MASK;
			regmap_write(sc->domain_addr, REG_OFFSET, regval);

			/*
			 * Make sure CLAMP_IO is de-asserted before continuing.
			 */
			gdsc_mb(sc);
		}

		regmap_read(sc->regmap, REG_OFFSET, &regval);
		regval &= ~SW_COLLAPSE_MASK;
		regmap_write(sc->regmap, REG_OFFSET, regval);

		/* Wait for 8 XO cycles before polling the status bit. */
		gdsc_mb(sc);
		udelay(1);

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
					goto end;
				}
			} else {
				dev_err(&rdev->dev, "%s enable timed out: 0x%x\n",
					sc->rdesc.name,
					regval);
				udelay(sc->gds_timeout);

				regmap_read(sc->regmap, REG_OFFSET, &regval);
				dev_err(&rdev->dev, "%s final state: 0x%x (%d us after timeout)\n",
					sc->rdesc.name, regval,
					sc->gds_timeout);
				goto end;
			}
		}

		if (sc->retain_ff_enable && !(regval & RETAIN_FF_ENABLE_MASK)) {
			regval |= RETAIN_FF_ENABLE_MASK;
			regmap_write(sc->regmap, REG_OFFSET, regval);
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
	sc->skip_disable_before_enable = false;
end:
	if (ret && sc->bus_handle) {
		msm_bus_scale_client_update_request(sc->bus_handle, 0);
		sc->is_bus_enabled = false;
	}

	if (ret && sc->parent_regulator)
		regulator_set_voltage(sc->parent_regulator, 0, INT_MAX);

	return ret;
}

static int gdsc_disable(struct regulator_dev *rdev)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval;
	int i, ret = 0;

	/*
	 * Protect GDSC against late_init disabling when the GDSC is enabled
	 * by an entity outside external to HLOS.
	 */
	if (sc->skip_disable_before_enable) {
		dev_dbg(&rdev->dev, "Skip Disabling: %s\n", sc->rdesc.name);
		sc->skip_disable_before_enable = false;
		return 0;
	}

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

		/* Wait for 8 XO cycles before polling the status bit. */
		gdsc_mb(sc);
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

	if (sc->bus_handle) {
		ret = msm_bus_scale_client_update_request(sc->bus_handle, 0);
		if (ret)
			dev_err(&rdev->dev, "bus scaling failed, ret=%d\n",
				ret);
		sc->is_bus_enabled = false;
	}

	if (sc->parent_regulator)
		regulator_set_voltage(sc->parent_regulator, 0, INT_MAX);

	sc->is_gdsc_enabled = false;

	return ret;
}

static unsigned int gdsc_get_mode(struct regulator_dev *rdev)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval;
	int ret;

	if (sc->parent_regulator) {
		ret = regulator_set_voltage(sc->parent_regulator,
					RPMH_REGULATOR_LEVEL_LOW_SVS, INT_MAX);
		if (ret)
			return ret;

		ret = regulator_enable(sc->parent_regulator);
		if (ret) {
			regulator_set_voltage(sc->parent_regulator, 0, INT_MAX);
			return ret;
		}
	}

	if (sc->bus_handle && !sc->is_bus_enabled) {
		ret = msm_bus_scale_client_update_request(sc->bus_handle, 1);
		if (ret) {
			dev_err(&rdev->dev, "bus scaling failed, ret=%d\n",
				ret);
			if (sc->parent_regulator) {
				regulator_disable(sc->parent_regulator);
				regulator_set_voltage(sc->parent_regulator, 0,
							INT_MAX);
			}
			return ret;
		}
	}

	regmap_read(sc->regmap, REG_OFFSET, &regval);

	if (sc->bus_handle && !sc->is_bus_enabled)
		msm_bus_scale_client_update_request(sc->bus_handle, 0);

	if (sc->parent_regulator) {
		regulator_disable(sc->parent_regulator);
		regulator_set_voltage(sc->parent_regulator, 0, INT_MAX);
	}

	if (regval & HW_CONTROL_MASK)
		return REGULATOR_MODE_FAST;

	return REGULATOR_MODE_NORMAL;
}

static int gdsc_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval;
	int ret = 0;

	if (sc->parent_regulator) {
		ret = regulator_set_voltage(sc->parent_regulator,
				RPMH_REGULATOR_LEVEL_LOW_SVS, INT_MAX);
		if (ret)
			return ret;

		ret = regulator_enable(sc->parent_regulator);
		if (ret) {
			regulator_set_voltage(sc->parent_regulator, 0, INT_MAX);
			return ret;
		}
	}

	if (sc->bus_handle && !sc->is_bus_enabled) {
		ret = msm_bus_scale_client_update_request(sc->bus_handle, 1);
		if (ret) {
			dev_err(&rdev->dev, "bus scaling failed, ret=%d\n",
				ret);
			if (sc->parent_regulator) {
				regulator_disable(sc->parent_regulator);
				regulator_set_voltage(sc->parent_regulator, 0,
							INT_MAX);
			}
			return ret;
		}
	}

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
		gdsc_mb(sc);
		udelay(1);
		break;
	case REGULATOR_MODE_NORMAL:
		/* Turn off HW trigger mode */
		regval &= ~HW_CONTROL_MASK;
		regmap_write(sc->regmap, REG_OFFSET, regval);
		/*
		 * There may be a race with internal HW trigger signal,
		 * that will result in GDSC going through a power down and
		 * up cycle. Account for this case by waiting 1us before
		 * proceeding.
		 */
		gdsc_mb(sc);
		udelay(1);

		/*
		 * While switching from HW to SW mode, HW may be busy
		 * updating internal required signals. Polling for PWR_ON
		 * ensures that the GDSC switches to SW mode before software
		 * starts to use SW mode.
		 */
		if (sc->is_gdsc_enabled) {
			ret = poll_gdsc_status(sc, ENABLED);
			if (ret)
				dev_err(&rdev->dev, "%s enable timed out\n",
					sc->rdesc.name);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (sc->bus_handle && !sc->is_bus_enabled)
		msm_bus_scale_client_update_request(sc->bus_handle, 0);

	if (sc->parent_regulator) {
		regulator_disable(sc->parent_regulator);
		regulator_set_voltage(sc->parent_regulator, 0, INT_MAX);
	}

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

static int gdsc_parse_dt_data(struct gdsc *sc, struct device *dev,
				struct regulator_init_data **init_data)
{
	int ret;

	*init_data = of_get_regulator_init_data(dev, dev->of_node, &sc->rdesc);
	if (*init_data == NULL)
		return -ENOMEM;

	if (of_get_property(dev->of_node, "parent-supply", NULL))
		(*init_data)->supply_regulator = "parent";

	ret = of_property_read_string(dev->of_node, "regulator-name",
					&sc->rdesc.name);
	if (ret)
		return ret;

	if (of_find_property(dev->of_node, "domain-addr", NULL)) {
		sc->domain_addr = syscon_regmap_lookup_by_phandle(dev->of_node,
								"domain-addr");
		if (IS_ERR(sc->domain_addr))
			return PTR_ERR(sc->domain_addr);
	}

	if (of_find_property(dev->of_node, "sw-reset", NULL)) {
		sc->sw_reset = syscon_regmap_lookup_by_phandle(dev->of_node,
								"sw-reset");
		if (IS_ERR(sc->sw_reset))
			return PTR_ERR(sc->sw_reset);
	}

	if (of_find_property(dev->of_node, "hw-ctrl-addr", NULL)) {
		sc->hw_ctrl = syscon_regmap_lookup_by_phandle(dev->of_node,
								"hw-ctrl-addr");
		if (IS_ERR(sc->hw_ctrl))
			return PTR_ERR(sc->hw_ctrl);
	}

	sc->gds_timeout = TIMEOUT_US;
	of_property_read_u32(dev->of_node, "qcom,gds-timeout",
				&sc->gds_timeout);

	sc->clock_count = of_property_count_strings(dev->of_node,
							"clock-names");
	if (sc->clock_count == -EINVAL) {
		sc->clock_count = 0;
	} else if (sc->clock_count < 0) {
		dev_err(dev, "Failed to get clock names, ret=%d\n",
			sc->clock_count);
		return sc->clock_count;
	}

	sc->root_en = of_property_read_bool(dev->of_node,
						"qcom,enable-root-clk");
	sc->force_root_en = of_property_read_bool(dev->of_node,
						"qcom,force-enable-root-clk");
	sc->reset_aon = of_property_read_bool(dev->of_node,
						"qcom,reset-aon-logic");
	sc->toggle_mem = !of_property_read_bool(dev->of_node,
						"qcom,retain-mem");
	sc->toggle_periph = !of_property_read_bool(dev->of_node,
						"qcom,retain-periph");
	sc->allow_clear = !of_property_read_bool(dev->of_node,
						"qcom,disallow-clear");
	sc->no_status_check_on_disable = of_property_read_bool(dev->of_node,
					"qcom,no-status-check-on-disable");
	sc->retain_ff_enable = of_property_read_bool(dev->of_node,
						"qcom,retain-regs");
	sc->skip_disable_before_enable = of_property_read_bool(dev->of_node,
					"qcom,skip-disable-before-sw-enable");

	sc->toggle_logic = !of_property_read_bool(dev->of_node,
						"qcom,skip-logic-collapse");
	if (!sc->toggle_logic) {
		sc->reset_count = of_property_count_strings(dev->of_node,
							    "reset-names");
		if (sc->reset_count == -EINVAL) {
			sc->reset_count = 0;
		} else if (sc->reset_count < 0) {
			dev_err(dev, "Failed to get reset clock names\n");
			return sc->reset_count;
		}
	}

	if (of_find_property(dev->of_node, "qcom,support-hw-trigger", NULL)) {
		(*init_data)->constraints.valid_ops_mask |=
				REGULATOR_CHANGE_MODE;
		(*init_data)->constraints.valid_modes_mask |=
				REGULATOR_MODE_NORMAL | REGULATOR_MODE_FAST;
	}

	return 0;
}

static int gdsc_get_resources(struct gdsc *sc, struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret, i;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "Failed to get address resource\n");
		return -EINVAL;
	}

	sc->gdscr = devm_ioremap(dev, res->start, resource_size(res));
	if (sc->gdscr == NULL)
		return -ENOMEM;

	sc->regmap = devm_regmap_init_mmio(dev, sc->gdscr, &gdsc_regmap_config);
	if (!sc->regmap) {
		dev_err(dev, "Couldn't get regmap\n");
		return -EINVAL;
	}

	if (of_find_property(dev->of_node, "vdd_parent-supply", NULL)) {
		sc->parent_regulator = devm_regulator_get(dev, "vdd_parent");
		if (IS_ERR(sc->parent_regulator)) {
			ret = PTR_ERR(sc->parent_regulator);
			if (ret != -EPROBE_DEFER)
				dev_err(dev, "Unable to get vdd_parent regulator, ret=%d\n",
					ret);
			return ret;
		}
	}

	sc->clocks = devm_kcalloc(dev, sc->clock_count, sizeof(*sc->clocks),
				  GFP_KERNEL);
	if (sc->clock_count && !sc->clocks)
		return -ENOMEM;

	sc->root_clk_idx = -1;
	for (i = 0; i < sc->clock_count; i++) {
		const char *clock_name;

		of_property_read_string_index(dev->of_node, "clock-names", i,
					      &clock_name);

		sc->clocks[i] = devm_clk_get(dev, clock_name);
		if (IS_ERR(sc->clocks[i])) {
			ret = PTR_ERR(sc->clocks[i]);
			if (ret != -EPROBE_DEFER)
				dev_err(dev, "Failed to get %s, ret=%d\n",
					clock_name, ret);
			return ret;
		}

		if (!strcmp(clock_name, "core_root_clk"))
			sc->root_clk_idx = i;
	}

	if ((sc->root_en || sc->force_root_en) && (sc->root_clk_idx == -1)) {
		dev_err(dev, "Failed to get root clock name\n");
		return -EINVAL;
	}

	if (!sc->toggle_logic) {
		sc->reset_clocks = devm_kcalloc(&pdev->dev, sc->reset_count,
						sizeof(*sc->reset_clocks),
						GFP_KERNEL);
		if (sc->reset_count && !sc->reset_clocks)
			return -ENOMEM;

		for (i = 0; i < sc->reset_count; i++) {
			const char *reset_name;

			of_property_read_string_index(pdev->dev.of_node,
						"reset-names", i, &reset_name);
			sc->reset_clocks[i] = devm_reset_control_get(&pdev->dev,
								reset_name);
			if (IS_ERR(sc->reset_clocks[i])) {
				ret = PTR_ERR(sc->reset_clocks[i]);
				if (ret != -EPROBE_DEFER)
					dev_err(&pdev->dev, "Failed to get %s, ret=%d\n",
						reset_name, ret);
				return ret;
			}
		}
	}

	if (of_find_property(pdev->dev.of_node, "qcom,msm-bus,name", NULL)) {
		sc->bus_pdata = msm_bus_cl_get_pdata(pdev);
		if (!sc->bus_pdata) {
			dev_err(&pdev->dev, "Failed to get bus config data\n");
			return -EINVAL;
		}

		sc->bus_handle = msm_bus_scale_register_client(sc->bus_pdata);
		if (!sc->bus_handle) {
			dev_err(&pdev->dev, "Failed to register bus client\n");
			/*
			 * msm_bus_scale_register_client() returns 0 for all
			 * errors including when called before the bus driver
			 * probes.  Therefore, return -EPROBE_DEFER here so that
			 * probing can be retried and this case handled.
			 */
			return -EPROBE_DEFER;
		}
	}

	return 0;
}

static int gdsc_probe(struct platform_device *pdev)
{
	static atomic_t gdsc_count = ATOMIC_INIT(-1);
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data = NULL;
	struct gdsc *sc;
	uint32_t regval, clk_dis_wait_val = 0;
	int i, ret;

	sc = devm_kzalloc(&pdev->dev, sizeof(*sc), GFP_KERNEL);
	if (sc == NULL)
		return -ENOMEM;

	ret = gdsc_parse_dt_data(sc, &pdev->dev, &init_data);
	if (ret)
		return ret;

	ret = gdsc_get_resources(sc, pdev);
	if (ret)
		goto err;

	if (sc->bus_handle) {
		/*
		 * Request non-zero bus bandwidth to ensure that the slave
		 * hardware block containing the GDSC is not disconnected from
		 * the bus.  This allows register IO for the GDSC to succeed.
		 */
		ret = msm_bus_scale_client_update_request(sc->bus_handle, 1);
		if (ret) {
			dev_err(&pdev->dev, "bus scaling failed, ret=%d\n",
				ret);
			goto err;
		}
		sc->is_bus_enabled = true;
	}

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

	regmap_write(sc->regmap, REG_OFFSET, regval);

	if (!sc->toggle_logic) {
		regval &= ~SW_COLLAPSE_MASK;
		regmap_write(sc->regmap, REG_OFFSET, regval);

		ret = poll_gdsc_status(sc, ENABLED);
		if (ret) {
			dev_err(&pdev->dev, "%s enable timed out: 0x%x\n",
				sc->rdesc.name, regval);
			goto err;
		}
	}

	if (sc->bus_handle) {
		regmap_read(sc->regmap, REG_OFFSET, &regval);
		if (!(regval & PWR_ON_MASK) || (regval & SW_COLLAPSE_MASK)) {
			/*
			 * Software is not enabling the GDSC so remove the
			 * bus vote.
			 */
			msm_bus_scale_client_update_request(sc->bus_handle, 0);
			sc->is_bus_enabled = false;
		}
	}

	for (i = 0; i < sc->clock_count; i++) {
		if (!sc->toggle_mem || (regval & PWR_ON_MASK) ||
		    !sc->allow_clear)
			clk_set_flags(sc->clocks[i], CLKFLAG_RETAIN_MEM);
		else
			clk_set_flags(sc->clocks[i], CLKFLAG_NORETAIN_MEM);

		if (!sc->toggle_periph || (regval & PWR_ON_MASK) ||
		    !sc->allow_clear)
			clk_set_flags(sc->clocks[i], CLKFLAG_RETAIN_PERIPH);
		else
			clk_set_flags(sc->clocks[i], CLKFLAG_NORETAIN_PERIPH);
	}

	sc->rdesc.id = atomic_inc_return(&gdsc_count);
	sc->rdesc.ops = &gdsc_ops;
	sc->rdesc.type = REGULATOR_VOLTAGE;
	sc->rdesc.owner = THIS_MODULE;

	reg_config.dev = &pdev->dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = sc;
	reg_config.of_node = pdev->dev.of_node;
	reg_config.regmap = sc->regmap;

	sc->rdev = devm_regulator_register(&pdev->dev, &sc->rdesc, &reg_config);
	if (IS_ERR(sc->rdev)) {
		ret = PTR_ERR(sc->rdev);
		dev_err(&pdev->dev, "regulator_register(\"%s\") failed, ret=%d\n",
			sc->rdesc.name, ret);
		goto err;
	}

	platform_set_drvdata(pdev, sc);

	return 0;

err:
	if (sc->bus_handle) {
		if (sc->is_bus_enabled)
			msm_bus_scale_client_update_request(sc->bus_handle, 0);
		msm_bus_scale_unregister_client(sc->bus_handle);
	}

	return ret;
}

static int gdsc_remove(struct platform_device *pdev)
{
	struct gdsc *sc = platform_get_drvdata(pdev);

	if (sc->bus_handle) {
		if (sc->is_bus_enabled)
			msm_bus_scale_client_update_request(sc->bus_handle, 0);
		msm_bus_scale_unregister_client(sc->bus_handle);
	}

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
