/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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
#include <linux/mailbox_client.h>
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
#include <linux/mailbox/qmp.h>

#include <dt-bindings/regulator/qcom,rpmh-regulator.h>

/* GDSCR */
#define PWR_ON_MASK		BIT(31)
#define CLK_DIS_WAIT_MASK	(0xF << 12)
#define CLK_DIS_WAIT_SHIFT	(12)
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

#define MBOX_TOUT_MS		100

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
	struct mbox_client	mbox_client;
	struct mbox_chan	*mbox;
	u32			bus_handle;
	bool			toggle_mem;
	bool			toggle_periph;
	bool			toggle_logic;
	bool			resets_asserted;
	bool			root_en;
	bool			force_root_en;
	bool			no_status_check_on_disable;
	bool			skip_disable;
	bool			bypass_skip_disable;
	bool			is_gdsc_enabled;
	bool			allow_clear;
	bool			reset_aon;
	bool			is_bus_enabled;
	int			clock_count;
	int			reset_count;
	int			root_clk_idx;
	u32			gds_timeout;
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

	/*
	 * Return the logical GDSC enable state given that it will only be
	 * physically disabled by AOP during system sleep.
	 */
	if (sc->skip_disable)
		return sc->is_gdsc_enabled;

	if (!sc->toggle_logic)
		return !sc->resets_asserted;

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

#define MAX_LEN 96

static int gdsc_qmp_enable(struct gdsc *sc)
{
	char buf[MAX_LEN] = "{class: clock, res: gpu_noc_wa}";
	struct qmp_pkt pkt;
	uint32_t regval;
	int ret;

	regmap_read(sc->regmap, REG_OFFSET, &regval);
	if (!(regval & SW_COLLAPSE_MASK)) {
		/*
		 * Do not enable via a QMP request if the GDSC is already
		 * enabled by software.
		 */
		return 0;
	}

	pkt.size = MAX_LEN;
	pkt.data = buf;

	ret = mbox_send_message(sc->mbox, &pkt);
	if (ret < 0)
		dev_err(&sc->rdev->dev, "qmp message send failed, ret=%d\n",
			ret);

	return ret;
}

static int gdsc_enable(struct regulator_dev *rdev)
{
	struct gdsc *sc = rdev_get_drvdata(rdev);
	uint32_t regval, hw_ctrl_regval = 0x0;
	int i, ret = 0;

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

		if (sc->mbox) {
			ret = gdsc_qmp_enable(sc);
			if (ret < 0)
				goto end;
		} else {
			regmap_read(sc->regmap, REG_OFFSET, &regval);
			regval &= ~SW_COLLAPSE_MASK;
			regmap_write(sc->regmap, REG_OFFSET, regval);
		}

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

	if (sc->skip_disable && !sc->bypass_skip_disable) {
		/*
		 * Don't change the GDSCR register state on disable.  AOP will
		 * handle this during system sleep.
		 */
	} else if (sc->toggle_logic) {
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

	if (sc->skip_disable) {
		if (sc->bypass_skip_disable)
			return REGULATOR_MODE_IDLE;
		return REGULATOR_MODE_NORMAL;
	}

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

	if (sc->skip_disable) {
		switch (mode) {
		case REGULATOR_MODE_IDLE:
			sc->bypass_skip_disable = true;
			break;
		case REGULATOR_MODE_NORMAL:
			sc->bypass_skip_disable = false;
			break;
		default:
			ret = -EINVAL;
			break;
		}

		return ret;
	}

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

static int gdsc_probe(struct platform_device *pdev)
{
	static atomic_t gdsc_count = ATOMIC_INIT(-1);
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data;
	struct resource *res;
	struct gdsc *sc;
	uint32_t regval, clk_dis_wait_val = 0;
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

	if (of_find_property(pdev->dev.of_node, "vdd_parent-supply", NULL)) {
		sc->parent_regulator = devm_regulator_get(&pdev->dev,
							"vdd_parent");
		if (IS_ERR(sc->parent_regulator)) {
			ret = PTR_ERR(sc->parent_regulator);
			if (ret != -EPROBE_DEFER)
				dev_err(&pdev->dev,
				"Unable to get vdd_parent regulator, err: %d\n",
					ret);
			return ret;
		}
	}

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

		ret = msm_bus_scale_client_update_request(sc->bus_handle, 1);
		if (ret) {
			dev_err(&pdev->dev, "bus scaling failed, ret=%d\n",
				ret);
			goto err;
		}
		sc->is_bus_enabled = true;
	}

	if (of_find_property(pdev->dev.of_node, "mboxes", NULL)) {
		sc->mbox_client.dev = &pdev->dev;
		sc->mbox_client.tx_block = true;
		sc->mbox_client.tx_tout = MBOX_TOUT_MS;
		sc->mbox_client.knows_txdone = false;

		sc->mbox = mbox_request_channel(&sc->mbox_client, 0);
		if (IS_ERR(sc->mbox)) {
			ret = PTR_ERR(sc->mbox);
			if (ret != -EPROBE_DEFER)
				dev_err(&pdev->dev, "mailbox channel request failed, ret=%d\n",
					ret);
			sc->mbox = NULL;
			goto err;
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

	sc->no_status_check_on_disable =
			of_property_read_bool(pdev->dev.of_node,
					"qcom,no-status-check-on-disable");
	sc->skip_disable = of_property_read_bool(pdev->dev.of_node,
					"qcom,skip-disable");
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

	if (sc->skip_disable) {
		/*
		 * If the disable skipping feature is allowed, then use mode
		 * control to enable and disable the feature at runtime instead
		 * of using it to enable and disable hardware triggering.
		 */
		init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_MODE;
		init_data->constraints.valid_modes_mask =
				REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE;
	}

	if (!sc->toggle_logic) {
		sc->reset_count = of_property_count_strings(pdev->dev.of_node,
					    "reset-names");
		if (sc->reset_count == -EINVAL) {
			sc->reset_count = 0;
		} else if (sc->reset_count < 0) {
			dev_err(&pdev->dev, "Failed to get reset clock names\n");
			ret = -EINVAL;
			goto err;
		}

		sc->reset_clocks = devm_kzalloc(&pdev->dev,
			sizeof(struct reset_control *) * sc->reset_count,
							GFP_KERNEL);
		if (!sc->reset_clocks) {
			ret = -ENOMEM;
			goto err;
		}

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
				ret = rc;
				goto err;
			}
		}

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
		ret = PTR_ERR(sc->rdev);
		goto err;
	}

	return 0;

err:
	if (sc->mbox)
		mbox_free_channel(sc->mbox);

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

	regulator_unregister(sc->rdev);

	if (sc->mbox)
		mbox_free_channel(sc->mbox);

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
