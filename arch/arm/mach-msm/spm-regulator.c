/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/spmi.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#include "spm.h"
#include "spm-regulator.h"

#define SPM_REGULATOR_DRIVER_NAME "qcom,spm-regulator"

struct voltage_range {
	int min_uV;
	int set_point_min_uV;
	int max_uV;
	int step_uV;
};

/* Properties for FTS2 type QPNP PMIC regulators. */

static const struct voltage_range fts2_range0 = {0, 350000, 1275000,  5000};
static const struct voltage_range fts2_range1 = {0, 700000, 2040000, 10000};

/* Specifies the PMIC internal slew rate in uV/us. */
#define QPNP_FTS2_SLEW_RATE		6000

#define QPNP_FTS2_REG_TYPE		0x04
#define QPNP_FTS2_REG_SUBTYPE		0x05
#define QPNP_FTS2_REG_VOLTAGE_RANGE	0x40
#define QPNP_FTS2_REG_VOLTAGE_SETPOINT	0x41

#define QPNP_FTS2_TYPE			0x1C
#define QPNP_FTS2_SUBTYPE		0x08

struct spm_vreg {
	struct regulator_desc		rdesc;
	struct regulator_dev		*rdev;
	struct spmi_device		*spmi_dev;
	const struct voltage_range	*range;
	int				uV;
	int				last_set_uV;
	unsigned			vlevel;
	unsigned			last_set_vlevel;
	bool				online;
	u16				spmi_base_addr;
};

static int _spm_regulator_set_voltage(struct regulator_dev *rdev)
{
	struct spm_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	if (vreg->vlevel == vreg->last_set_vlevel)
		return 0;

	rc = msm_spm_apcs_set_vdd(vreg->vlevel);
	if (rc) {
		pr_err("%s: msm_spm_set_vdd failed %d\n", vreg->rdesc.name, rc);
		return rc;
	}

	if (vreg->uV > vreg->last_set_uV) {
		/* Wait for voltage to stabalize. */
		udelay(DIV_ROUND_UP(vreg->uV - vreg->last_set_uV,
					QPNP_FTS2_SLEW_RATE));
	}
	vreg->last_set_uV = vreg->uV;
	vreg->last_set_vlevel = vreg->vlevel;

	return rc;
}

static int spm_regulator_set_voltage(struct regulator_dev *rdev, int min_uV,
					int max_uV, unsigned *selector)
{
	struct spm_vreg *vreg = rdev_get_drvdata(rdev);
	const struct voltage_range *range = vreg->range;
	int uV = min_uV;
	unsigned vlevel;

	if (uV < range->set_point_min_uV && max_uV >= range->set_point_min_uV)
		uV = range->set_point_min_uV;

	if (uV < range->set_point_min_uV || uV > range->max_uV) {
		pr_err("%s: request v=[%d, %d] is outside possible v=[%d, %d]\n",
			vreg->rdesc.name, min_uV, max_uV,
			range->set_point_min_uV, range->max_uV);
		return -EINVAL;
	}

	vlevel = DIV_ROUND_UP(uV - range->min_uV, range->step_uV);
	uV = vlevel * range->step_uV + range->min_uV;

	if (uV > max_uV) {
		pr_err("%s: request v=[%d, %d] cannot be met by any set point\n",
			vreg->rdesc.name, min_uV, max_uV);
		return -EINVAL;
	}

	vreg->vlevel = vlevel;
	vreg->uV = uV;
	*selector = vlevel -
		(vreg->range->set_point_min_uV - vreg->range->min_uV)
			/ vreg->range->step_uV;

	if (!vreg->online)
		return 0;

	return _spm_regulator_set_voltage(rdev);
}

static int spm_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct spm_vreg *vreg = rdev_get_drvdata(rdev);

	return vreg->uV;
}

static int spm_regulator_list_voltage(struct regulator_dev *rdev,
					unsigned selector)
{
	struct spm_vreg *vreg = rdev_get_drvdata(rdev);

	if (selector >= vreg->rdesc.n_voltages)
		return 0;

	return selector * vreg->range->step_uV + vreg->range->set_point_min_uV;
}

static int spm_regulator_enable(struct regulator_dev *rdev)
{
	struct spm_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = _spm_regulator_set_voltage(rdev);

	if (!rc)
		vreg->online = true;

	return rc;
}

static int spm_regulator_disable(struct regulator_dev *rdev)
{
	struct spm_vreg *vreg = rdev_get_drvdata(rdev);

	vreg->online = false;

	return 0;
}

static int spm_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct spm_vreg *vreg = rdev_get_drvdata(rdev);

	return vreg->online;
}

static struct regulator_ops spm_regulator_ops = {
	.get_voltage	= spm_regulator_get_voltage,
	.set_voltage	= spm_regulator_set_voltage,
	.list_voltage	= spm_regulator_list_voltage,
	.enable		= spm_regulator_enable,
	.disable	= spm_regulator_disable,
	.is_enabled	= spm_regulator_is_enabled,
};

static int qpnp_fts2_check_type(struct spm_vreg *vreg)
{
	int rc;
	u8 type[2];

	rc = spmi_ext_register_readl(vreg->spmi_dev->ctrl, vreg->spmi_dev->sid,
		vreg->spmi_base_addr + QPNP_FTS2_REG_TYPE, type, 2);
	if (rc) {
		dev_err(&vreg->spmi_dev->dev, "%s: could not read type register, rc=%d\n",
			__func__, rc);
		return rc;
	}

	if (type[0] != QPNP_FTS2_TYPE || type[1] != QPNP_FTS2_SUBTYPE) {
		dev_err(&vreg->spmi_dev->dev, "%s: invalid type=0x%02X or subtype=0x%02X register value\n",
			__func__, type[0], type[1]);
		return -ENODEV;
	}

	return rc;
}

static int qpnp_fts2_init_range(struct spm_vreg *vreg)
{
	int rc;
	u8 reg = 0;

	rc = spmi_ext_register_readl(vreg->spmi_dev->ctrl, vreg->spmi_dev->sid,
		vreg->spmi_base_addr + QPNP_FTS2_REG_VOLTAGE_RANGE, &reg, 1);
	if (rc) {
		dev_err(&vreg->spmi_dev->dev, "%s: could not read voltage range register, rc=%d\n",
			__func__, rc);
		return rc;
	}

	if (reg == 0x00) {
		vreg->range = &fts2_range0;
	} else if (reg == 0x01) {
		vreg->range = &fts2_range1;
	} else {
		dev_err(&vreg->spmi_dev->dev, "%s: voltage range=%d is invalid\n",
			__func__, reg);
		rc = -EINVAL;
	}

	return rc;
}

static int qpnp_fts2_init_voltage(struct spm_vreg *vreg)
{
	int rc;
	u8 reg = 0;

	rc = spmi_ext_register_readl(vreg->spmi_dev->ctrl, vreg->spmi_dev->sid,
		vreg->spmi_base_addr + QPNP_FTS2_REG_VOLTAGE_SETPOINT, &reg, 1);
	if (rc) {
		dev_err(&vreg->spmi_dev->dev, "%s: could not read voltage setpoint register, rc=%d\n",
			__func__, rc);
		return rc;
	}

	vreg->vlevel = reg;
	vreg->uV = vreg->vlevel * vreg->range->step_uV + vreg->range->min_uV;
	vreg->last_set_uV = vreg->uV;

	return rc;
}

static int __devinit spm_regulator_probe(struct spmi_device *spmi)
{
	struct device_node *node = spmi->dev.of_node;
	struct regulator_init_data *init_data;
	struct spm_vreg *vreg;
	struct resource *res;
	int rc;

	if (!node) {
		dev_err(&spmi->dev, "%s: device node missing\n", __func__);
		return -ENODEV;
	}

	vreg = devm_kzalloc(&spmi->dev, sizeof(*vreg), GFP_KERNEL);
	if (!vreg) {
		pr_err("allocation failed.\n");
		return -ENOMEM;
	}
	vreg->spmi_dev = spmi;

	res = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&spmi->dev, "%s: node is missing base address\n",
			__func__);
		return -EINVAL;
	}
	vreg->spmi_base_addr = res->start;

	rc = qpnp_fts2_check_type(vreg);
	if (rc)
		return rc;

	/*
	 * The FTS2 regulator must be initialized to range 0 or range 1 during
	 * PMIC power on sequence.  Once it is set, it cannot be changed
	 * dynamically.
	 */
	rc = qpnp_fts2_init_range(vreg);
	if (rc)
		return rc;

	rc = qpnp_fts2_init_voltage(vreg);
	if (rc)
		return rc;

	init_data = of_get_regulator_init_data(&spmi->dev, node);
	if (!init_data) {
		dev_err(&spmi->dev, "%s: unable to allocate memory\n",
				__func__);
		return -ENOMEM;
	}
	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
						| REGULATOR_CHANGE_VOLTAGE;

	if (!init_data->constraints.name) {
		dev_err(&spmi->dev, "%s: node is missing regulator name\n",
			__func__);
		return -EINVAL;
	}

	vreg->rdesc.name	= init_data->constraints.name;
	vreg->rdesc.type	= REGULATOR_VOLTAGE;
	vreg->rdesc.owner	= THIS_MODULE;
	vreg->rdesc.ops		= &spm_regulator_ops;
	vreg->rdesc.n_voltages
		= (vreg->range->max_uV - vreg->range->set_point_min_uV)
			/ vreg->range->step_uV + 1;

	vreg->rdev = regulator_register(&vreg->rdesc, &spmi->dev,
					init_data, vreg, node);
	if (IS_ERR(vreg->rdev)) {
		rc = PTR_ERR(vreg->rdev);
		dev_err(&spmi->dev, "%s: regulator_register failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	dev_set_drvdata(&spmi->dev, vreg);

	pr_info("name=%s, range=%d\n", vreg->rdesc.name,
		(vreg->range == &fts2_range0) ? 0 : 1);

	return rc;
}

static int __devexit spm_regulator_remove(struct spmi_device *spmi)
{
	struct spm_vreg *vreg = dev_get_drvdata(&spmi->dev);

	regulator_unregister(vreg->rdev);

	return 0;
}

static struct of_device_id spm_regulator_match_table[] = {
	{ .compatible = SPM_REGULATOR_DRIVER_NAME, },
	{}
};

static const struct spmi_device_id spm_regulator_id[] = {
	{ SPM_REGULATOR_DRIVER_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(spmi, spm_regulator_id);

static struct spmi_driver spm_regulator_driver = {
	.driver = {
		.name		= SPM_REGULATOR_DRIVER_NAME,
		.of_match_table = spm_regulator_match_table,
		.owner		= THIS_MODULE,
	},
	.probe		= spm_regulator_probe,
	.remove		= __devexit_p(spm_regulator_remove),
	.id_table	= spm_regulator_id,
};

/**
 * spm_regulator_init() - register spmi driver for spm-regulator
 *
 * This initialization function should be called in systems in which driver
 * registration ordering must be controlled precisely.
 *
 * Returns 0 on success or errno on failure.
 */
int __init spm_regulator_init(void)
{
	static bool has_registered;

	if (has_registered)
		return 0;
	else
		has_registered = true;

	return spmi_driver_register(&spm_regulator_driver);
}
EXPORT_SYMBOL(spm_regulator_init);

static void __exit spm_regulator_exit(void)
{
	spmi_driver_unregister(&spm_regulator_driver);
}

arch_initcall(spm_regulator_init);
module_exit(spm_regulator_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SPM regulator driver");
MODULE_ALIAS("platform:spm-regulator");
