/*
 * Copyright (c) 2012, 2016, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/stub-regulator.h>

#define STUB_REGULATOR_MAX_NAME 40

struct regulator_stub {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
	int			voltage;
	bool			enabled;
	int			mode;
	int			hpm_min_load;
	int			system_uA;
	char			name[STUB_REGULATOR_MAX_NAME];
};

static int regulator_stub_set_voltage(struct regulator_dev *rdev, int min_uV,
				  int max_uV, unsigned *selector)
{
	struct regulator_stub *vreg_priv = rdev_get_drvdata(rdev);

	vreg_priv->voltage = min_uV;
	return 0;
}

static int regulator_stub_get_voltage(struct regulator_dev *rdev)
{
	struct regulator_stub *vreg_priv = rdev_get_drvdata(rdev);

	return vreg_priv->voltage;
}

static int regulator_stub_list_voltage(struct regulator_dev *rdev,
				    unsigned selector)
{
	struct regulation_constraints *constraints = rdev->constraints;

	if (selector >= 2)
		return -EINVAL;
	else if (selector == 0)
		return constraints->min_uV;
	else
		return constraints->max_uV;
}

static unsigned int regulator_stub_get_mode(struct regulator_dev *rdev)
{
	struct regulator_stub *vreg_priv = rdev_get_drvdata(rdev);

	return vreg_priv->mode;
}

static int regulator_stub_set_mode(struct regulator_dev *rdev,
				   unsigned int mode)
{
	struct regulator_stub *vreg_priv = rdev_get_drvdata(rdev);

	if (mode != REGULATOR_MODE_NORMAL && mode != REGULATOR_MODE_IDLE) {
		dev_err(&rdev->dev, "%s: invalid mode requested %u\n",
							__func__, mode);
		return -EINVAL;
	}
	vreg_priv->mode = mode;
	return 0;
}

static unsigned int regulator_stub_get_optimum_mode(struct regulator_dev *rdev,
		int input_uV, int output_uV, int load_uA)
{
	struct regulator_stub *vreg_priv = rdev_get_drvdata(rdev);
	unsigned int mode;

	if (load_uA + vreg_priv->system_uA >= vreg_priv->hpm_min_load)
		mode = REGULATOR_MODE_NORMAL;
	else
		mode = REGULATOR_MODE_IDLE;

	return mode;
}

static int regulator_stub_enable(struct regulator_dev *rdev)
{
	struct regulator_stub *vreg_priv = rdev_get_drvdata(rdev);

	vreg_priv->enabled = true;
	return 0;
}

static int regulator_stub_disable(struct regulator_dev *rdev)
{
	struct regulator_stub *vreg_priv = rdev_get_drvdata(rdev);

	vreg_priv->enabled = false;
	return 0;
}

static int regulator_stub_is_enabled(struct regulator_dev *rdev)
{
	struct regulator_stub *vreg_priv = rdev_get_drvdata(rdev);

	return vreg_priv->enabled;
}

/* Real regulator operations. */
static struct regulator_ops regulator_stub_ops = {
	.enable			= regulator_stub_enable,
	.disable		= regulator_stub_disable,
	.is_enabled		= regulator_stub_is_enabled,
	.set_voltage		= regulator_stub_set_voltage,
	.get_voltage		= regulator_stub_get_voltage,
	.list_voltage		= regulator_stub_list_voltage,
	.set_mode		= regulator_stub_set_mode,
	.get_mode		= regulator_stub_get_mode,
	.get_optimum_mode	= regulator_stub_get_optimum_mode,
};

static void regulator_stub_cleanup(struct regulator_stub *vreg_priv)
{
	if (vreg_priv && vreg_priv->rdev)
		regulator_unregister(vreg_priv->rdev);
	kfree(vreg_priv);
}

static int regulator_stub_probe(struct platform_device *pdev)
{
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data = NULL;
	struct device *dev = &pdev->dev;
	struct stub_regulator_pdata *vreg_pdata;
	struct regulator_desc *rdesc;
	struct regulator_stub *vreg_priv;
	int rc;

	vreg_priv = kzalloc(sizeof(*vreg_priv), GFP_KERNEL);
	if (!vreg_priv)
		return -ENOMEM;

	if (dev->of_node) {
		/* Use device tree. */
		init_data = of_get_regulator_init_data(dev, dev->of_node,
							&vreg_priv->rdesc);
		if (!init_data) {
			dev_err(dev, "%s: unable to allocate memory\n",
					__func__);
			rc = -ENOMEM;
			goto err_probe;
		}

		if (init_data->constraints.name == NULL) {
			dev_err(dev, "%s: regulator name not specified\n",
				__func__);
			rc = -EINVAL;
			goto err_probe;
		}

		if (of_get_property(dev->of_node, "parent-supply", NULL))
			init_data->supply_regulator = "parent";

		of_property_read_u32(dev->of_node, "qcom,system-load",
					&vreg_priv->system_uA);
		of_property_read_u32(dev->of_node, "qcom,hpm-min-load",
					&vreg_priv->hpm_min_load);

		init_data->constraints.input_uV	= init_data->constraints.max_uV;

		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_STATUS;
		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_VOLTAGE;
		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_DRMS;
		init_data->constraints.valid_modes_mask
			= REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE;
	} else {
		/* Use platform data. */
		vreg_pdata = dev->platform_data;
		if (!vreg_pdata) {
			dev_err(dev, "%s: no platform data\n", __func__);
			rc = -EINVAL;
			goto err_probe;
		}
		init_data = &vreg_pdata->init_data;

		vreg_priv->system_uA = vreg_pdata->system_uA;
		vreg_priv->hpm_min_load = vreg_pdata->hpm_min_load;
	}

	dev_set_drvdata(dev, vreg_priv);

	rdesc = &vreg_priv->rdesc;
	strlcpy(vreg_priv->name, init_data->constraints.name,
						   STUB_REGULATOR_MAX_NAME);
	rdesc->name = vreg_priv->name;
	rdesc->ops = &regulator_stub_ops;

	/*
	 * Ensure that voltage set points are handled correctly for regulators
	 * which have a specified voltage constraint range, as well as those
	 * that do not.
	 */
	if (init_data->constraints.min_uV == 0 &&
	    init_data->constraints.max_uV == 0)
		rdesc->n_voltages = 0;
	else
		rdesc->n_voltages = 2;

	rdesc->id    = pdev->id;
	rdesc->owner = THIS_MODULE;
	rdesc->type  = REGULATOR_VOLTAGE;
	vreg_priv->voltage = init_data->constraints.min_uV;
	if (vreg_priv->system_uA >= vreg_priv->hpm_min_load)
		vreg_priv->mode = REGULATOR_MODE_NORMAL;
	else
		vreg_priv->mode = REGULATOR_MODE_IDLE;

	reg_config.dev = dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = vreg_priv;
	reg_config.of_node = dev->of_node;
	vreg_priv->rdev = regulator_register(rdesc, &reg_config);

	if (IS_ERR(vreg_priv->rdev)) {
		rc = PTR_ERR(vreg_priv->rdev);
		vreg_priv->rdev = NULL;
		if (rc != -EPROBE_DEFER)
			dev_err(dev, "%s: regulator_register failed\n",
				__func__);
		goto err_probe;
	}

	return 0;

err_probe:
	regulator_stub_cleanup(vreg_priv);
	return rc;
}

static int regulator_stub_remove(struct platform_device *pdev)
{
	struct regulator_stub *vreg_priv = dev_get_drvdata(&pdev->dev);

	regulator_stub_cleanup(vreg_priv);
	return 0;
}

static const struct of_device_id regulator_stub_match_table[] = {
	{ .compatible = "qcom," STUB_REGULATOR_DRIVER_NAME, },
	{}
};

static struct platform_driver regulator_stub_driver = {
	.probe	= regulator_stub_probe,
	.remove	= regulator_stub_remove,
	.driver	= {
		.name	= STUB_REGULATOR_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = regulator_stub_match_table,
	},
};

int __init regulator_stub_init(void)
{
	static int registered;

	if (registered)
		return 0;

	registered = 1;

	return platform_driver_register(&regulator_stub_driver);
}
EXPORT_SYMBOL(regulator_stub_init);
postcore_initcall(regulator_stub_init);

static void __exit regulator_stub_exit(void)
{
	platform_driver_unregister(&regulator_stub_driver);
}
module_exit(regulator_stub_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("stub regulator driver");
MODULE_ALIAS("platform: " STUB_REGULATOR_DRIVER_NAME);
