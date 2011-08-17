/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/mfd/pmic8058.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/pm8058-xo.h>

/* XO buffer masks and values */

#define XO_PULLDOWN_MASK	0x08
#define XO_PULLDOWN_ENABLE	0x08
#define XO_PULLDOWN_DISABLE	0x00

#define XO_BUFFER_MASK		0x04
#define XO_BUFFER_ENABLE	0x04
#define XO_BUFFER_DISABLE	0x00

#define XO_MODE_MASK		0x01
#define XO_MODE_MANUAL		0x00

#define XO_ENABLE_MASK		(XO_MODE_MASK | XO_BUFFER_MASK)
#define XO_ENABLE		(XO_MODE_MANUAL | XO_BUFFER_ENABLE)
#define XO_DISABLE		(XO_MODE_MANUAL | XO_BUFFER_DISABLE)

struct pm8058_xo_buffer {
	struct pm8058_xo_pdata		*pdata;
	struct regulator_dev		*rdev;
	u16				ctrl_addr;
	u8				ctrl_reg;
};

#define XO_BUFFER(_id, _ctrl_addr) \
	[PM8058_XO_ID_##_id] = { \
		.ctrl_addr = _ctrl_addr, \
	}

static struct pm8058_xo_buffer pm8058_xo_buffer[] = {
	XO_BUFFER(A0, 0x185),
	XO_BUFFER(A1, 0x186),
};

static int pm8058_xo_buffer_write(struct pm8058_chip *chip,
		u16 addr, u8 val, u8 mask, u8 *reg_save)
{
	u8	reg;
	int	rc = 0;

	reg = (*reg_save & ~mask) | (val & mask);
	if (reg != *reg_save)
		rc = pm8058_write(chip, addr, &reg, 1);

	if (rc)
		pr_err("FAIL: pm8058_write: rc=%d\n", rc);
	else
		*reg_save = reg;
	return rc;
}

static int pm8058_xo_buffer_enable(struct regulator_dev *dev)
{
	struct pm8058_xo_buffer *xo = rdev_get_drvdata(dev);
	struct pm8058_chip *chip = dev_get_drvdata(dev->dev.parent);
	int rc;

	rc = pm8058_xo_buffer_write(chip, xo->ctrl_addr, XO_ENABLE,
				    XO_ENABLE_MASK, &xo->ctrl_reg);
	if (rc)
		pr_err("FAIL: pm8058_xo_buffer_write: rc=%d\n", rc);

	return rc;
}

static int pm8058_xo_buffer_is_enabled(struct regulator_dev *dev)
{
	struct pm8058_xo_buffer *xo = rdev_get_drvdata(dev);

	if (xo->ctrl_reg & XO_BUFFER_ENABLE)
		return 1;
	else
		return 0;
}

static int pm8058_xo_buffer_disable(struct regulator_dev *dev)
{
	struct pm8058_xo_buffer *xo = rdev_get_drvdata(dev);
	struct pm8058_chip *chip = dev_get_drvdata(dev->dev.parent);
	int rc;

	rc = pm8058_xo_buffer_write(chip, xo->ctrl_addr, XO_DISABLE,
				    XO_ENABLE_MASK, &xo->ctrl_reg);
	if (rc)
		pr_err("FAIL: pm8058_xo_buffer_write: rc=%d\n", rc);

	return rc;
}

static struct regulator_ops pm8058_xo_ops = {
	.enable = pm8058_xo_buffer_enable,
	.disable = pm8058_xo_buffer_disable,
	.is_enabled = pm8058_xo_buffer_is_enabled,
};

#define VREG_DESCRIP(_id, _name, _ops) \
	[_id] = { \
		.id = _id, \
		.name = _name, \
		.ops = _ops, \
		.type = REGULATOR_VOLTAGE, \
		.owner = THIS_MODULE, \
	}

static struct regulator_desc pm8058_xo_buffer_desc[] = {
	VREG_DESCRIP(PM8058_XO_ID_A0, "8058_xo_a0", &pm8058_xo_ops),
	VREG_DESCRIP(PM8058_XO_ID_A1, "8058_xo_a1", &pm8058_xo_ops),
};

static int pm8058_init_xo_buffer(struct pm8058_chip *chip,
				 struct pm8058_xo_buffer *xo)
{
	int	rc;

	/* Save the current control register state */
	rc = pm8058_read(chip, xo->ctrl_addr, &xo->ctrl_reg, 1);

	if (rc)
		pr_err("FAIL: pm8058_read: rc=%d\n", rc);
	return rc;
}

static int __devinit pm8058_xo_buffer_probe(struct platform_device *pdev)
{
	struct regulator_desc *rdesc;
	struct pm8058_chip *chip;
	struct pm8058_xo_buffer *xo;
	int rc = 0;

	if (pdev == NULL)
		return -EINVAL;

	if (pdev->id >= 0 && pdev->id < PM8058_XO_ID_MAX) {
		chip = platform_get_drvdata(pdev);
		rdesc = &pm8058_xo_buffer_desc[pdev->id];
		xo = &pm8058_xo_buffer[pdev->id];
		xo->pdata = pdev->dev.platform_data;

		rc = pm8058_init_xo_buffer(chip, xo);
		if (rc)
			goto bail;

		xo->rdev = regulator_register(rdesc, &pdev->dev,
					&xo->pdata->init_data, xo);
		if (IS_ERR(xo->rdev)) {
			rc = PTR_ERR(xo->rdev);
			pr_err("FAIL: regulator_register(%s): rc=%d\n",
				pm8058_xo_buffer_desc[pdev->id].name, rc);
		}
	} else {
		rc = -ENODEV;
	}

bail:
	if (rc)
		pr_err("Error: xo-id=%d, rc=%d\n", pdev->id, rc);

	return rc;
}

static int __devexit pm8058_xo_buffer_remove(struct platform_device *pdev)
{
	regulator_unregister(pm8058_xo_buffer[pdev->id].rdev);
	return 0;
}

static struct platform_driver pm8058_xo_buffer_driver = {
	.probe = pm8058_xo_buffer_probe,
	.remove = __devexit_p(pm8058_xo_buffer_remove),
	.driver = {
		.name = PM8058_XO_BUFFER_DEV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init pm8058_xo_buffer_init(void)
{
	return platform_driver_register(&pm8058_xo_buffer_driver);
}

static void __exit pm8058_xo_buffer_exit(void)
{
	platform_driver_unregister(&pm8058_xo_buffer_driver);
}

subsys_initcall(pm8058_xo_buffer_init);
module_exit(pm8058_xo_buffer_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8058 XO buffer driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8058_XO_BUFFER_DEV_NAME);
