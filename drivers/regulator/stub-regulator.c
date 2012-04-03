/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/regulator/driver.h>
#include <linux/regulator/stub-regulator.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>

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

static int __devinit regulator_stub_probe(struct platform_device *pdev)
{
	struct stub_regulator_pdata *vreg_pdata;
	struct regulator_desc *rdesc;
	struct regulator_stub *vreg_priv;
	int rc;

	vreg_pdata = pdev->dev.platform_data;
	if (!vreg_pdata) {
		dev_err(&pdev->dev, "%s: no platform data\n", __func__);
		return -EINVAL;
	}

	vreg_priv = kzalloc(sizeof(*vreg_priv), GFP_KERNEL);
	if (!vreg_priv) {
		dev_err(&pdev->dev, "%s: Unable to allocate memory\n",
				__func__);
		return -ENOMEM;
	}
	dev_set_drvdata(&pdev->dev, vreg_priv);

	rdesc = &vreg_priv->rdesc;
	strncpy(vreg_priv->name, vreg_pdata->init_data.constraints.name,
						   STUB_REGULATOR_MAX_NAME);
	rdesc->name = vreg_priv->name;
	rdesc->ops = &regulator_stub_ops;

	/*
	 * Ensure that voltage set points are handled correctly for regulators
	 * which have a specified voltage constraint range, as well as those
	 * that do not.
	 */
	if (vreg_pdata->init_data.constraints.min_uV == 0 &&
	    vreg_pdata->init_data.constraints.max_uV == 0)
		rdesc->n_voltages = 0;
	else
		rdesc->n_voltages = 2;

	rdesc->id    = pdev->id;
	rdesc->owner = THIS_MODULE;
	rdesc->type  = REGULATOR_VOLTAGE;
	vreg_priv->system_uA = vreg_pdata->system_uA;
	vreg_priv->hpm_min_load = vreg_pdata->hpm_min_load;
	vreg_priv->voltage = vreg_pdata->init_data.constraints.min_uV;

	vreg_priv->rdev = regulator_register(rdesc, &pdev->dev,
			&(vreg_pdata->init_data), vreg_priv, NULL);
	if (IS_ERR(vreg_priv->rdev)) {
		rc = PTR_ERR(vreg_priv->rdev);
		vreg_priv->rdev = NULL;
		dev_err(&pdev->dev, "%s: regulator_register failed\n",
				__func__);
		goto err_probe;
	}

	return 0;

err_probe:
	regulator_stub_cleanup(vreg_priv);
	return rc;
}

static int __devexit regulator_stub_remove(struct platform_device *pdev)
{
	struct regulator_stub *vreg_priv = dev_get_drvdata(&pdev->dev);

	regulator_stub_cleanup(vreg_priv);
	return 0;
}

static struct platform_driver regulator_stub_driver = {
	.probe	= regulator_stub_probe,
	.remove	= __devexit_p(regulator_stub_remove),
	.driver	= {
		.name	= STUB_REGULATOR_DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

int __init regulator_stub_init(void)
{
	static int registered;

	if (registered)
		return 0;
	else
		registered = 1;
	return platform_driver_register(&regulator_stub_driver);
}
postcore_initcall(regulator_stub_init);
EXPORT_SYMBOL(regulator_stub_init);

static void __exit regulator_stub_exit(void)
{
	platform_driver_unregister(&regulator_stub_driver);
}
module_exit(regulator_stub_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("stub regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform: " STUB_REGULATOR_DRIVER_NAME);
