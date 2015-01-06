/*
 * pmic_whiskey_cove.c - CherryTrail regulator driver
 *
 * Copyright (c) 2014, Intel Corporation.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/regulator/intel_whiskey_cove_pmic.h>

/* voltage control regulator offsets */

/* buck boost regulators */
#define WCOVE_V3P3A_CTRL	0x5e
/* buck regulators */
#define WCOVE_V1P8A_CTRL	0x56
#define WCOVE_V1P05A_CTRL	0x3b
#define WCOVE_V1P15_CTRL	0x3c
#define WCOVE_VDDQ_CTRL		0x58
/* boot regulators */
/* ldo regulators */
#define WCOVE_VPROG1A_CTRL	0x90
#define WCOVE_VPROG1B_CTRL	0x91
#define WCOVE_VPROG1F_CTRL	0x95
#define WCOVE_V1P8SX_CTRL	0x57
#define WCOVE_V1P2A_CTRL	0x59
#define WCOVE_V1P2SX_CTRL	0x5a
#define WCOVE_VSDIO_CTRL	0x67
#define WCOVE_V2P8SX_CTRL	0x5d
#define WCOVE_V3P3SD_CTRL	0x5f
#define WCOVE_VPROG2D_CTRL	0x99
#define WCOVE_VPROG3A_CTRL	0x9a
#define WCOVE_VPROG3B_CTRL	0x9b
#define WCOVE_VPROG4A_CTRL	0x9c
#define WCOVE_VPROG4B_CTRL	0x9d
#define WCOVE_VPROG4C_CTRL	0x9e
#define WCOVE_VPROG4D_CTRL	0x9f
#define WCOVE_VPROG5A_CTRL	0xa0
#define WCOVE_VPROG5B_CTRL	0xa1
#define WCOVE_VPROG6A_CTRL	0xa2
#define WCOVE_VPROG6B_CTRL	0xa3


/* voltage selector regulator offsets */

/* buck boost regulators */
#define WCOVE_V3P3A_VSEL	0x68
/* buck regulators */
#define WCOVE_V1P8A_VSEL	0x5b
#define WCOVE_V1P05A_VSEL	0x3d
#define WCOVE_V1P15_VSEL	0x3e
#define WCOVE_VDDQ_VSEL		0x5c
/* boot regulators */
/* ldo regulators */
#define WCOVE_VPROG1A_VSEL	0xc0
#define WCOVE_VPROG1B_VSEL	0xc1
#define WCOVE_V1P8SX_VSEL	0xc2
#define WCOVE_V1P2SX_VSEL	0xc3
#define WCOVE_V1P2A_VSEL	0xc4
#define WCOVE_VPROG1F_VSEL	0xc5
#define WCOVE_VSDIO_VSEL	0xc6
#define WCOVE_V2P8SX_VSEL	0xc7
#define WCOVE_V3P3SD_VSEL	0xc8
#define WCOVE_VPROG2D_VSEL	0xc9
#define WCOVE_VPROG3A_VSEL	0xca
#define WCOVE_VPROG3B_VSEL	0xcb
#define WCOVE_VPROG4A_VSEL	0xcc
#define WCOVE_VPROG4B_VSEL	0xcd
#define WCOVE_VPROG4C_VSEL	0xce
#define WCOVE_VPROG4D_VSEL	0xcf
#define WCOVE_VPROG5A_VSEL	0xd0
#define WCOVE_VPROG5B_VSEL	0xd1
#define WCOVE_VPROG6A_VSEL	0xd2
#define WCOVE_VPROG6B_VSEL	0xd3


/* number of voltage variations exposed */

/* buck boost regulators */
#define WCOVE_V3P3A_VRANGE	8
/* buck regulators */
#define WCOVE_V1P8A_VRANGE	256
#define WCOVE_V1P05A_VRANGE	256
#define WCOVE_VDDQ_VRANGE	120
/* boot regulators */
/* ldo regulators */
#define WCOVE_VPROG1A_VRANGE	53
#define WCOVE_VPROG1B_VRANGE	53
#define WCOVE_VPROG1F_VRANGE	53
#define WCOVE_V1P8SX_VRANGE	53
#define WCOVE_V1P2SX_VRANGE	53
#define WCOVE_V1P2A_VRANGE	53
#define WCOVE_VSDIO_VRANGE	53
#define WCOVE_V2P8SX_VRANGE	53
#define WCOVE_V3P3SD_VRANGE	53
#define WCOVE_VPROG2D_VRANGE	53
#define WCOVE_VPROG3A_VRANGE	53
#define WCOVE_VPROG3B_VRANGE	53
#define WCOVE_VPROG4A_VRANGE	53
#define WCOVE_VPROG4B_VRANGE	53
#define WCOVE_VPROG4C_VRANGE	53
#define WCOVE_VPROG4D_VRANGE	53
#define WCOVE_VPROG5A_VRANGE	53
#define WCOVE_VPROG5B_VRANGE	53
#define WCOVE_VPROG6A_VRANGE	53
#define WCOVE_VPROG6B_VRANGE	53


/* voltage tables */
static unsigned int WCOVE_V3P3A_VSEL_TABLE[WCOVE_V3P3A_VRANGE],
		    WCOVE_V1P8A_VSEL_TABLE[WCOVE_V1P8A_VRANGE],
		    WCOVE_V1P05A_VSEL_TABLE[WCOVE_V1P05A_VRANGE],
		    WCOVE_VDDQ_VSEL_TABLE[WCOVE_VDDQ_VRANGE],
		    WCOVE_V1P8SX_VSEL_TABLE[WCOVE_V1P8SX_VRANGE],
		    WCOVE_V1P2SX_VSEL_TABLE[WCOVE_V1P2SX_VRANGE],
		    WCOVE_V1P2A_VSEL_TABLE[WCOVE_V1P2A_VRANGE],
		    WCOVE_V2P8SX_VSEL_TABLE[WCOVE_V2P8SX_VRANGE],
		    WCOVE_V3P3SD_VSEL_TABLE[WCOVE_V3P3SD_VRANGE],
		    WCOVE_VPROG1A_VSEL_TABLE[WCOVE_VPROG1A_VRANGE],
		    WCOVE_VPROG1B_VSEL_TABLE[WCOVE_VPROG1B_VRANGE],
		    WCOVE_VPROG1F_VSEL_TABLE[WCOVE_VPROG1F_VRANGE],
		    WCOVE_VPROG2D_VSEL_TABLE[WCOVE_VPROG2D_VRANGE],
		    WCOVE_VPROG3A_VSEL_TABLE[WCOVE_VPROG3A_VRANGE],
		    WCOVE_VPROG3B_VSEL_TABLE[WCOVE_VPROG3B_VRANGE],
		    WCOVE_VPROG4A_VSEL_TABLE[WCOVE_VPROG4A_VRANGE],
		    WCOVE_VPROG4B_VSEL_TABLE[WCOVE_VPROG4B_VRANGE],
		    WCOVE_VPROG4C_VSEL_TABLE[WCOVE_VPROG4C_VRANGE],
		    WCOVE_VPROG4D_VSEL_TABLE[WCOVE_VPROG4D_VRANGE],
		    WCOVE_VPROG5A_VSEL_TABLE[WCOVE_VPROG5A_VRANGE],
		    WCOVE_VPROG5B_VSEL_TABLE[WCOVE_VPROG5B_VRANGE],
		    WCOVE_VPROG6A_VSEL_TABLE[WCOVE_VPROG6A_VRANGE],
		    WCOVE_VPROG6B_VSEL_TABLE[WCOVE_VPROG6B_VRANGE];

/*
 * The VSDIO regulator should only support 1.8V and 3.3V. All other
 * voltages are invalid for sd card, so disable them here.
 */
static const unsigned int WCOVE_VSDIO_VSEL_TABLE[WCOVE_VSDIO_VRANGE] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 1800000, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 3300000, 0, 0
};

static int wcove_regulator_enable(struct regulator_dev *rdev)
{
	struct wcove_regulator_info *pmic_info = rdev_get_drvdata(rdev);
	int reg_val;

	reg_val = intel_soc_pmic_readb(pmic_info->vctl_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	reg_val &= ~pmic_info->vctl_mask;
	reg_val |= pmic_info->reg_enbl_mask;

	return intel_soc_pmic_writeb(pmic_info->vctl_reg, reg_val);
}

static int wcove_regulator_disable(struct regulator_dev *rdev)
{
	struct wcove_regulator_info *pmic_info = rdev_get_drvdata(rdev);
	int reg_val;

	reg_val = intel_soc_pmic_readb(pmic_info->vctl_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	reg_val &= ~pmic_info->vctl_mask;
	reg_val |= pmic_info->reg_dsbl_mask;

	return intel_soc_pmic_writeb(pmic_info->vctl_reg, reg_val);
}

static int wcove_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct wcove_regulator_info *pmic_info = rdev_get_drvdata(rdev);
	int reg_val;

	reg_val = intel_soc_pmic_readb(pmic_info->vctl_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	reg_val &= pmic_info->vctl_mask;

	return reg_val & pmic_info->reg_enbl_mask;
}

static int wcove_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	struct wcove_regulator_info *pmic_info = rdev_get_drvdata(rdev);
	int reg_val, vsel;

	reg_val = intel_soc_pmic_readb(pmic_info->vsel_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	vsel = (reg_val & pmic_info->vsel_mask) - pmic_info->start;

	return vsel;
}

static int wcove_regulator_set_voltage_sel(struct regulator_dev *rdev,
		unsigned selector)
{
	struct wcove_regulator_info *pmic_info = rdev_get_drvdata(rdev);
	int reg_val;

	reg_val = intel_soc_pmic_readb(pmic_info->vsel_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	reg_val &= ~pmic_info->vsel_mask;
	reg_val |= (selector + pmic_info->start);

	return intel_soc_pmic_writeb(pmic_info->vsel_reg, reg_val);
}

/* regulator ops */
static struct regulator_ops wcove_regulator_ops = {
	.enable = wcove_regulator_enable,
	.disable = wcove_regulator_disable,
	.is_enabled = wcove_regulator_is_enabled,
	.get_voltage_sel = wcove_regulator_get_voltage_sel,
	.set_voltage_sel = wcove_regulator_set_voltage_sel,
	.list_voltage = regulator_list_voltage_table,
};

#define WCOVE_REG(_id, minv, maxv, strt, vselmsk, vscale, \
					vctlmsk, enbl, dsbl, rt_flag) \
{\
	.desc = {\
		.name	= ""#_id,\
		.ops	= &wcove_regulator_ops,\
		.type	= REGULATOR_VOLTAGE,\
		.id	= WCOVE_ID_##_id,\
		.owner	= THIS_MODULE,\
	},\
	.regulator = NULL, \
	.init_data = NULL, \
	.vctl_reg	= WCOVE_##_id##_CTRL,\
	.vsel_reg	= WCOVE_##_id##_VSEL,\
	.min_mV		= (minv),\
	.max_mV		= (maxv),\
	.start		= (strt),\
	.vsel_mask	= (vselmsk),\
	.scale		= (vscale),\
	.nvolts		= WCOVE_##_id##_VRANGE,\
	.vctl_mask	= (vctlmsk),\
	.reg_enbl_mask	= (enbl),\
	.reg_dsbl_mask	= (dsbl),\
	.vtable		= WCOVE_##_id##_VSEL_TABLE,\
	.runtime_table	= (rt_flag),\
}

/* Regulator descriptions */
static struct wcove_regulator_info regulators_info[] = {
	WCOVE_REG(V3P3A, 3000, 3350, 0x0, 0x07, 50, 0x1, 0x1, 0x0, true),
	WCOVE_REG(V1P8A, 250, 2100, 0x0, 0xff, 10, 0x1, 0x1, 0x0, true),
	WCOVE_REG(V1P05A, 250, 2100, 0x0, 0xff, 10, 0x1, 0x1, 0x0, true),
	WCOVE_REG(VDDQ, 250, 1440, 0x0, 0x7f, 10, 0x1, 0x1, 0x0, true),
	WCOVE_REG(V1P8SX, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x1, 0x0, true),
	WCOVE_REG(V1P2A, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x1, 0x0, true),
	WCOVE_REG(V1P2SX, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x1, 0x0, true),
	WCOVE_REG(V2P8SX, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x1, 0x0, true),
	WCOVE_REG(VSDIO, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, false),
	WCOVE_REG(V3P3SD, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
	WCOVE_REG(VPROG1A, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
	WCOVE_REG(VPROG1B, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
	WCOVE_REG(VPROG1F, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
	WCOVE_REG(VPROG2D, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
	WCOVE_REG(VPROG3A, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
	WCOVE_REG(VPROG3B, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
	WCOVE_REG(VPROG4A, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
	WCOVE_REG(VPROG4B, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
	WCOVE_REG(VPROG4C, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
	WCOVE_REG(VPROG4D, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
	WCOVE_REG(VPROG5A, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
	WCOVE_REG(VPROG5B, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
	WCOVE_REG(VPROG6A, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
	WCOVE_REG(VPROG6B, 800, 3400, 0x0b, 0x3f, 50, 0x07, 0x01, 0x0, true),
};

static inline struct wcove_regulator_info *wcove_find_regulator_info(int id)
{
	struct wcove_regulator_info *reg_info;
	int i;

	for (i = 0; i < ARRAY_SIZE(regulators_info); i++) {
		if (regulators_info[i].desc.id == id) {
			reg_info = &regulators_info[i];
			return reg_info;
		}
	}
	return NULL;
}

static void initialize_vtable(struct wcove_regulator_info *reg_info)
{
	unsigned int i, volt;

	if (reg_info->runtime_table == true) {
		for (i = 0; i < reg_info->nvolts; i++) {
			volt = reg_info->min_mV + (i * reg_info->scale);
			if (volt < reg_info->min_mV)
				volt = reg_info->min_mV;
			if (volt > reg_info->max_mV)
				volt = reg_info->max_mV;
			/* set value in uV */
			reg_info->vtable[i] = volt*1000;
		}
	}
	reg_info->desc.volt_table = reg_info->vtable;
	reg_info->desc.n_voltages = reg_info->nvolts;
}

static int wcove_regulator_probe(struct platform_device *pdev)
{
	struct wcove_regulator_info *pdata = dev_get_platdata(&pdev->dev);
	struct regulator_config config = { };
	struct wcove_regulator_info *reg_info = NULL;

	if (!pdata) {
		dev_err(&pdev->dev, "No regulator info\n");
		return -EINVAL;
	}

	reg_info = wcove_find_regulator_info(pdev->id);
	if (reg_info == NULL) {
		dev_err(&pdev->dev, "invalid regulator %d\n", pdev->id);
		return -EINVAL;
	}

	if (IS_ERR(pdata->init_data))
			config.init_data = reg_info->init_data;
	else
		config.init_data = pdata->init_data;

	initialize_vtable(reg_info);
	config.dev = &pdev->dev;
	config.driver_data = reg_info;

	pdata->regulator = regulator_register(&reg_info->desc, &config);

	if (IS_ERR(pdata->regulator)) {
		dev_err(&pdev->dev, "failed to register regulator as %s\n",
				reg_info->desc.name);
		return PTR_ERR(pdata->regulator);
	}

	platform_set_drvdata(pdev, pdata->regulator);

	dev_dbg(&pdev->dev, "registered whiskey cove regulator as %s\n",
			dev_name(&pdata->regulator->dev));

	return 0;
}

static int wcove_regulator_remove(struct platform_device *pdev)
{
	regulator_unregister(platform_get_drvdata(pdev));
	return 0;
}

static const struct platform_device_id wcove_regulator_id_table[] = {
	{ "wcove_regulator", 0},
	{ },
};

MODULE_DEVICE_TABLE(platform, wcove_regulator_id_table);

static struct platform_driver wcove_regulator_driver = {
	.driver = {
		.name = "wcove_regulator",
		.owner = THIS_MODULE,
	},
	.probe = wcove_regulator_probe,
	.remove = wcove_regulator_remove,
	.id_table = wcove_regulator_id_table,
};

static int __init wcove_regulator_init(void)
{
	return platform_driver_register(&wcove_regulator_driver);
}
arch_initcall_sync(wcove_regulator_init);

static void __exit wcove_regulator_exit(void)
{
	platform_driver_unregister(&wcove_regulator_driver);
}
module_exit(wcove_regulator_exit);

MODULE_DESCRIPTION("WhiskeyCove regulator driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:intel_regulator");
