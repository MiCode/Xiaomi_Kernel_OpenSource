// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/slab.h>

/**
 * @regmap: regmap used to access PMIC registers
 */
struct pm2250_spmi {
	struct regmap *regmap;
};

static const struct of_device_id pm2250_id_table[] = {
	{ .compatible = "qcom,pm2250-spmi" },
	{ },
};
MODULE_DEVICE_TABLE(of, pm2250_id_table);

/**
 * pm2250_spmi_write: Function to write to PMIC register
 * @device: node for rouleur device
 * @reg: PMIC register to write value
 * @value: Value to be written to PMIC register
 */
int pm2250_spmi_write(struct device *dev, int reg, int value)
{
	int rc;
	struct pm2250_spmi *spmi_dd;

	if (!of_device_is_compatible(dev->of_node, "qcom,pm2250-spmi")) {
		pr_err("%s: Device node is invalid\n", __func__);
		return -EINVAL;
	}

	spmi_dd = dev_get_drvdata(dev);
	if (!spmi_dd)
		return -EINVAL;

	rc = regmap_write(spmi_dd->regmap, reg, value);
	if (rc)
		dev_err(dev, "%s: Write to PMIC register failed\n", __func__);

	return rc;
}
EXPORT_SYMBOL(pm2250_spmi_write);

/**
 * pm2250_spmi_read: Function to read PMIC register
 * @device: node for rouleur device
 * @reg: PMIC register to read value
 * @value: Pointer to value of reg to be read
 */
int pm2250_spmi_read(struct device *dev, int reg, int *value)
{
	int rc;
	struct pm2250_spmi *spmi_dd;

	if (!of_device_is_compatible(dev->of_node, "qcom,pm2250-spmi")) {
		pr_err("%s: Device node is invalid\n", __func__);
		return -EINVAL;
	}

	spmi_dd = dev_get_drvdata(dev);
	if (!spmi_dd)
		return -EINVAL;

	rc = regmap_read(spmi_dd->regmap, reg, value);
	if (rc)
		dev_err(dev, "%s: Read from PMIC register failed\n", __func__);

	return rc;
}
EXPORT_SYMBOL(pm2250_spmi_read);

static int pm2250_spmi_probe(struct platform_device *pdev)
{
	struct pm2250_spmi *spmi_dd;
	const struct of_device_id *match;

	match = of_match_node(pm2250_id_table, pdev->dev.of_node);
	if (!match)
		return -ENXIO;

	spmi_dd = devm_kzalloc(&pdev->dev, sizeof(*spmi_dd), GFP_KERNEL);
	if (spmi_dd == NULL)
		return -ENOMEM;

	spmi_dd->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!spmi_dd->regmap) {
		dev_err(&pdev->dev, "Parent regmap unavailable.\n");
		return -ENXIO;
	}

	platform_set_drvdata(pdev, spmi_dd);

	dev_dbg(&pdev->dev, "Probe success !!\n");

	return 0;
}

static int pm2250_spmi_remove(struct platform_device *pdev)
{
	of_platform_depopulate(&pdev->dev);
	return 0;
}

static struct platform_driver pm2250_spmi_driver = {
	.probe		= pm2250_spmi_probe,
	.remove		= pm2250_spmi_remove,
	.driver	= {
		.name		= "pm2250-spmi",
		.of_match_table	= pm2250_id_table,
	},
};
module_platform_driver(pm2250_spmi_driver);

MODULE_ALIAS("platform:pm2250-spmi");
MODULE_DESCRIPTION("PMIC SPMI driver");
MODULE_LICENSE("GPL v2");
