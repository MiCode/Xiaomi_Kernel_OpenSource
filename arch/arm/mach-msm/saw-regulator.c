/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/pmic8901.h>

#include "spm.h"

#define FTSMPS_VCTRL_BAND_MASK		0xC0
#define FTSMPS_VCTRL_BAND_1		0x40
#define FTSMPS_VCTRL_BAND_2		0x80
#define FTSMPS_VCTRL_BAND_3		0xC0
#define FTSMPS_VCTRL_VPROG_MASK		0x3F

#define FTSMPS_BAND1_UV_MIN		350000
#define FTSMPS_BAND1_UV_MAX		650000
/* 3 LSB's of program voltage must be 0 in band 1. */
/* Logical step size */
#define FTSMPS_BAND1_UV_LOG_STEP	50000
/* Physical step size */
#define FTSMPS_BAND1_UV_PHYS_STEP	6250

#define FTSMPS_BAND2_UV_MIN		700000
#define FTSMPS_BAND2_UV_MAX		1400000
#define FTSMPS_BAND2_UV_STEP		12500

#define FTSMPS_BAND3_UV_MIN		1400000
#define FTSMPS_BAND3_UV_SET_POINT_MIN	1500000
#define FTSMPS_BAND3_UV_MAX		3300000
#define FTSMPS_BAND3_UV_STEP		50000

struct saw_vreg {
	struct regulator_desc		desc;
	struct regulator_dev		*rdev;
	char				*name;
	int				uV;
};

/* Minimum core operating voltage */
#define MIN_CORE_VOLTAGE		950000

/* Specifies the PMIC internal slew rate in uV/us. */
#define REGULATOR_SLEW_RATE		1250

static int saw_get_voltage(struct regulator_dev *rdev)
{
	struct saw_vreg *vreg = rdev_get_drvdata(rdev);

	return vreg->uV;
}

static int saw_set_voltage(struct regulator_dev *rdev, int min_uV, int max_uV,
			   unsigned *selector)
{
	struct saw_vreg *vreg = rdev_get_drvdata(rdev);
	int uV = min_uV;
	int rc;
	u8 vprog, band;

	if (uV < FTSMPS_BAND1_UV_MIN && max_uV >= FTSMPS_BAND1_UV_MIN)
		uV = FTSMPS_BAND1_UV_MIN;

	if (uV < FTSMPS_BAND1_UV_MIN || uV > FTSMPS_BAND3_UV_MAX) {
		pr_err("%s: request v=[%d, %d] is outside possible "
			"v=[%d, %d]\n", vreg->name, min_uV, max_uV,
			FTSMPS_BAND1_UV_MIN, FTSMPS_BAND3_UV_MAX);
		return -EINVAL;
	}

	/* Round up for set points in the gaps between bands. */
	if (uV > FTSMPS_BAND1_UV_MAX && uV < FTSMPS_BAND2_UV_MIN)
		uV = FTSMPS_BAND2_UV_MIN;
	else if (uV > FTSMPS_BAND2_UV_MAX
			&& uV < FTSMPS_BAND3_UV_SET_POINT_MIN)
		uV = FTSMPS_BAND3_UV_SET_POINT_MIN;

	if (uV > FTSMPS_BAND2_UV_MAX) {
		vprog = (uV - FTSMPS_BAND3_UV_MIN + FTSMPS_BAND3_UV_STEP - 1)
			/ FTSMPS_BAND3_UV_STEP;
		band = FTSMPS_VCTRL_BAND_3;
		uV = FTSMPS_BAND3_UV_MIN + vprog * FTSMPS_BAND3_UV_STEP;
	} else if (uV > FTSMPS_BAND1_UV_MAX) {
		vprog = (uV - FTSMPS_BAND2_UV_MIN + FTSMPS_BAND2_UV_STEP - 1)
			/ FTSMPS_BAND2_UV_STEP;
		band = FTSMPS_VCTRL_BAND_2;
		uV = FTSMPS_BAND2_UV_MIN + vprog * FTSMPS_BAND2_UV_STEP;
	} else {
		vprog = (uV - FTSMPS_BAND1_UV_MIN
				+ FTSMPS_BAND1_UV_LOG_STEP - 1)
			/ FTSMPS_BAND1_UV_LOG_STEP;
		uV = FTSMPS_BAND1_UV_MIN + vprog * FTSMPS_BAND1_UV_LOG_STEP;
		vprog *= FTSMPS_BAND1_UV_LOG_STEP / FTSMPS_BAND1_UV_PHYS_STEP;
		band = FTSMPS_VCTRL_BAND_1;
	}

	if (uV > max_uV) {
		pr_err("%s: request v=[%d, %d] cannot be met by any setpoint\n",
			vreg->name, min_uV, max_uV);
		return -EINVAL;
	}

	rc = msm_spm_set_vdd(rdev_get_id(rdev), band | vprog);
	if (!rc) {
		if (uV > vreg->uV) {
			/* Wait for voltage to stabalize. */
			udelay((uV - vreg->uV) / REGULATOR_SLEW_RATE);
		}
		vreg->uV = uV;
	} else {
		pr_err("%s: msm_spm_set_vdd failed %d\n", vreg->name, rc);
	}

	return rc;
}

static struct regulator_ops saw_ops = {
	.get_voltage = saw_get_voltage,
	.set_voltage = saw_set_voltage,
};

static int __devinit saw_probe(struct platform_device *pdev)
{
	struct regulator_init_data *init_data;
	struct saw_vreg *vreg;
	int rc = 0;

	if (!pdev->dev.platform_data) {
		pr_err("platform data required.\n");
		return -EINVAL;
	}

	init_data = pdev->dev.platform_data;
	if (!init_data->constraints.name) {
		pr_err("regulator name must be specified in constraints.\n");
		return -EINVAL;
	}

	vreg = kzalloc(sizeof(struct saw_vreg), GFP_KERNEL);
	if (!vreg) {
		pr_err("kzalloc failed.\n");
		return -ENOMEM;
	}

	vreg->name = kstrdup(init_data->constraints.name, GFP_KERNEL);
	if (!vreg->name) {
		pr_err("kzalloc failed.\n");
		rc = -ENOMEM;
		goto free_vreg;
	}

	vreg->desc.name  = vreg->name;
	vreg->desc.id    = pdev->id;
	vreg->desc.ops   = &saw_ops;
	vreg->desc.type  = REGULATOR_VOLTAGE;
	vreg->desc.owner = THIS_MODULE;
	vreg->uV	 = MIN_CORE_VOLTAGE;

	vreg->rdev = regulator_register(&vreg->desc, &pdev->dev, init_data,
					vreg);
	if (IS_ERR(vreg->rdev)) {
		rc = PTR_ERR(vreg->rdev);
		pr_err("regulator_register failed, rc=%d.\n", rc);
		goto free_name;
	}

	platform_set_drvdata(pdev, vreg);

	pr_info("id=%d, name=%s\n", pdev->id, vreg->name);

	return rc;

free_name:
	kfree(vreg->name);
free_vreg:
	kfree(vreg);

	return rc;
}

static int __devexit saw_remove(struct platform_device *pdev)
{
	struct saw_vreg *vreg = platform_get_drvdata(pdev);

	regulator_unregister(vreg->rdev);
	kfree(vreg->name);
	kfree(vreg);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver saw_driver = {
	.probe = saw_probe,
	.remove = __devexit_p(saw_remove),
	.driver = {
		.name = "saw-regulator",
		.owner = THIS_MODULE,
	},
};

static int __init saw_init(void)
{
	return platform_driver_register(&saw_driver);
}

static void __exit saw_exit(void)
{
	platform_driver_unregister(&saw_driver);
}

postcore_initcall(saw_init);
module_exit(saw_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SAW regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:saw-regulator");
