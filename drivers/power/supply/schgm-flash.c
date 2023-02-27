// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "SCHG-FLASH: %s: " fmt, __func__

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pmic-voter.h>
#include <linux/power_supply.h>
#include <linux/printk.h>
#include <linux/regmap.h>
#include <linux/soc/qcom/battery_charger.h>
#include "schgm-flash.h"

#define IS_BETWEEN(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

struct schgm_flash_dev {
	struct regmap		*regmap;
	struct device		*dev;
	struct power_supply	*batt_psy;
	u32			flash_derating_soc;
	u32			flash_disable_soc;
	u32			headroom_mode;
};

static int smblib_read(struct schgm_flash_dev *chg, u16 addr, u8 *val)
{
	unsigned int value;
	int rc = 0;

	rc = regmap_read(chg->regmap, addr, &value);
	if (rc >= 0)
		*val = (u8)value;

	return rc;
}

static int smblib_write(struct schgm_flash_dev *chg, u16 addr, u8 val)
{
	return regmap_write(chg->regmap, addr, val);
}

static int smblib_masked_write(struct schgm_flash_dev *chg, u16 addr, u8 mask, u8 val)
{
	return regmap_update_bits(chg->regmap, addr, mask, val);
}

static irqreturn_t schgm_flash_ilim2_irq_handler(int irq, void *data)
{
	struct schgm_flash_dev *fdev = data;
	int rc;

	rc = smblib_write(fdev, SCHGM_FLASH_S2_LATCH_RESET_CMD_REG,
				FLASH_S2_LATCH_RESET_BIT);
	if (rc < 0)
		pr_err("Couldn't reset S2_LATCH reset rc=%d\n", rc);

	return IRQ_HANDLED;
}

static irqreturn_t schgm_flash_state_change_irq_handler(int irq, void *data)
{
	struct schgm_flash_dev *fdev = data;
	int rc;
	u8 reg;

	rc = smblib_read(fdev, SCHGM_FLASH_STATUS_3_REG, &reg);
	if (rc < 0)
		pr_err("Couldn't read flash status_3 rc=%d\n", rc);
	else
		pr_debug("Flash status changed state=[%x]\n",
					(reg && FLASH_STATE_MASK));

	if ((reg & FLASH_STATE_MASK) == FLASH_ERROR_VAL) {
		rc = smblib_read(fdev, SCHGM_FLASH_STATUS_5_REG,
			&reg);
		if (!rc)
			pr_err("Flash Error: status=0x%02x\n", reg);
	}

	return IRQ_HANDLED;
}

#define FIXED_MODE		0
#define ADAPTIVE_MODE		1
static void schgm_flash_parse_dt(struct schgm_flash_dev *fdev)
{
	struct device_node *node = fdev->dev->of_node;
	u32 val;
	int rc;

	fdev->flash_derating_soc = -EINVAL;
	rc = of_property_read_u32(node, "qcom,flash-derating-soc", &val);
	if (!rc) {
		if (IS_BETWEEN(0, 100, val))
			fdev->flash_derating_soc = (val * 255) / 100;
	}

	fdev->flash_disable_soc = -EINVAL;
	rc = of_property_read_u32(node, "qcom,flash-disable-soc", &val);
	if (!rc) {
		if (IS_BETWEEN(0, 100, val))
			fdev->flash_disable_soc = (val * 255) / 100;
	}

	fdev->headroom_mode = -EINVAL;
	rc = of_property_read_u32(node, "qcom,headroom-mode", &val);
	if (!rc) {
		if (IS_BETWEEN(FIXED_MODE, ADAPTIVE_MODE, val))
			fdev->headroom_mode = val;
	}
}

static void schgm_flash_torch_priority(struct schgm_flash_dev *fdev, enum torch_mode mode)
{
	int rc;
	u8 reg;

	/*
	 * If torch is configured in default BOOST mode, skip any update in the
	 * mode configuration.
	 */
	if (fdev->headroom_mode == FIXED_MODE)
		return;

	if ((mode != TORCH_BOOST_MODE) && (mode != TORCH_BUCK_MODE))
		return;

	reg = mode;
	rc = smblib_masked_write(fdev, SCHGM_TORCH_PRIORITY_CONTROL_REG,
					TORCH_PRIORITY_CONTROL_BIT, reg);
	if (rc < 0)
		pr_err("Couldn't configure Torch priority control rc=%d\n",
				rc);

	pr_debug("Torch priority changed to: %d\n", mode);
}

static int schgm_flash_init(struct schgm_flash_dev *fdev)
{
	int rc;
	u8 reg;

	schgm_flash_parse_dt(fdev);

	if (fdev->flash_derating_soc != -EINVAL) {
		rc = smblib_write(fdev, SCHGM_SOC_BASED_FLASH_DERATE_TH_CFG_REG,
					fdev->flash_derating_soc);
		if (rc < 0) {
			pr_err("Couldn't configure SOC for flash derating rc=%d\n",
					rc);
			return rc;
		}
	}

	if (fdev->flash_disable_soc != -EINVAL) {
		rc = smblib_write(fdev, SCHGM_SOC_BASED_FLASH_DISABLE_TH_CFG_REG,
					fdev->flash_disable_soc);
		if (rc < 0) {
			pr_err("Couldn't configure SOC for flash disable rc=%d\n",
					rc);
			return rc;
		}
	}

	if (fdev->headroom_mode != -EINVAL) {
		/*
		 * configure headroom management policy for
		 * flash and torch mode.
		 */
		reg = (fdev->headroom_mode == FIXED_MODE)
					? FORCE_FLASH_BOOST_5V_BIT : 0;
		rc = smblib_write(fdev, SCHGM_FORCE_BOOST_CONTROL, reg);
		if (rc < 0) {
			pr_err("Couldn't write force boost control reg rc=%d\n",
					rc);
			return rc;
		}

		reg = (fdev->headroom_mode == FIXED_MODE)
					? TORCH_PRIORITY_CONTROL_BIT : 0;
		rc = smblib_write(fdev, SCHGM_TORCH_PRIORITY_CONTROL_REG, reg);
		if (rc < 0) {
			pr_err("Couldn't force 5V boost in torch mode rc=%d\n",
					rc);
			return rc;
		}
	}

	if ((fdev->flash_derating_soc != -EINVAL)
				|| (fdev->flash_disable_soc != -EINVAL)) {
		/* Check if SOC based derating/disable is enabled */
		rc = smblib_read(fdev, SCHGM_FLASH_CONTROL_REG, &reg);
		if (rc < 0) {
			pr_err("Couldn't read flash control reg rc=%d\n", rc);
			return rc;
		}
		if (!(reg & SOC_LOW_FOR_FLASH_EN_BIT))
			pr_warn("Soc based flash derating not enabled\n");
	}

	return 0;
}

static int schgm_flash_probe(struct platform_device *pdev)
{
	struct schgm_flash_dev *fdev;
	struct device_node *node = pdev->dev.of_node;
	int irq, rc;
	union power_supply_propval pval = {0, };

	fdev = devm_kzalloc(&pdev->dev, sizeof(*fdev), GFP_KERNEL);
	if (!fdev)
		return -ENOMEM;

	fdev->dev = &pdev->dev;

	fdev->batt_psy = power_supply_get_by_name("battery");
	if (!fdev->batt_psy) {
		pr_err("Failed to get battery supply\n");
		return -EPROBE_DEFER;
	}

	rc = power_supply_get_property(fdev->batt_psy,
					POWER_SUPPLY_PROP_MODEL_NAME,
					&pval);
	if (rc < 0) {
		pr_err("Failed to get battery model name, rc=%d\n", rc);
		return -EPROBE_DEFER;
	}

	fdev->regmap = dev_get_regmap(fdev->dev->parent, NULL);
	if (!fdev->regmap)
		return -EINVAL;

	irq = of_irq_get_byname(node, "ilim2-s2");
	if (irq < 0) {
		pr_err("Couldn't get irq ilim2-s2 by name\n");
		return irq;
	}

	rc = devm_request_threaded_irq(fdev->dev, irq, NULL,
			schgm_flash_ilim2_irq_handler,
			IRQF_ONESHOT, "ilim2-s2", fdev);
	if (rc < 0) {
		pr_err("Couldn't request irq %d\n", irq);
		return rc;
	}

	irq = of_irq_get_byname(node, "flash-state-change");
	if (irq < 0) {
		pr_err("Couldn't get irq ilim2-s2 by name\n");
		return irq;
	}

	rc = devm_request_threaded_irq(fdev->dev, irq, NULL,
			schgm_flash_state_change_irq_handler,
			IRQF_ONESHOT, "flash-state-change", fdev);
	if (rc < 0) {
		pr_err("Couldn't request irq %d\n", irq);
		return rc;
	}

	rc = schgm_flash_init(fdev);
	if (rc < 0)
		return rc;

	if ((strcmp(pval.strval, "Debug_Board") == 0))
		schgm_flash_torch_priority(fdev, TORCH_BOOST_MODE);

	return 0;
}

static const struct of_device_id schgm_flash_of_match[] = {
	{ .compatible = "qcom,schgm-flash" },
	{}
};
MODULE_DEVICE_TABLE(of, schgm_flash_of_match);

static struct platform_driver schgm_flash_driver = {
	.driver = {
		.name = "qti-schgm-flash",
		.of_match_table	= schgm_flash_of_match,
	},
	.probe = schgm_flash_probe,
};
module_platform_driver(schgm_flash_driver);

MODULE_DESCRIPTION("QTI PMIC SCHGM Flash driver");
MODULE_LICENSE("GPL v2");
