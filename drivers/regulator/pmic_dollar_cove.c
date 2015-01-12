/*
 * Regulator driver for DollarCove XB PMIC
 *	(Based on XPwr Design)
 * Copyright(c) 2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/intel_dollar_cove_pmic.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/delay.h>

/* LDO and BUCK voltage control registers */

/* DLDOs */
#define DCOVEX_LDO1_VOL_CTRL		0x15
#define DCOVEX_LDO2_VOL_CTRL		0x16
#define DCOVEX_LDO3_VOL_CTRL		0x17
#define DCOVEX_LDO4_VOL_CTRL		0x18
/* ELDOs */
#define DCOVEX_LDO5_VOL_CTRL		0x19
#define DCOVEX_LDO6_VOL_CTRL		0x1a
#define DCOVEX_LDO7_VOL_CTRL		0x1b
/* FLDOs */
#define DCOVEX_LDO8_VOL_CTRL		0x1c
#define DCOVEX_LDO9_VOL_CTRL		0x1d
#define DCOVEX_LDO10_VOL_CTRL		0x1d

#define DCOVEX_BUCK6_VOL_CTRL		0x20
#define DCOVEX_BUCK5_VOL_CTRL		0x21
					/* 0x22 is RESERVED */
#define DCOVEX_BUCK1_VOL_CTRL		0x23
#define DCOVEX_BUCK4_VOL_CTRL		0x24
#define DCOVEX_BUCK3_VOL_CTRL		0x25
#define DCOVEX_BUCK2_VOL_CTRL		0x26
					/* 0x27 BUCK1/2/3/4/5 DVM */
/* ADOs */
#define DCOVEX_LDO11_VOL_CTRL		0x28
#define DCOVEX_LDO12_VOL_CTRL		0x29
#define DCOVEX_LDO13_VOL_CTRL		0x2a

/* GPIOs */
#define DCOVEX_GPIO1_VOL_CTRL		0x93

#define DCOVEX_GPIO0_EN_REG		0x90
#define DCOVEX_GPIO1_EN_REG		0x92

#define DCOVEX_GPIO_EN_VAL		0x3
#define DCOVEX_GPIO_DIS_VAL		0x4

/* BUCK1 == BUCK5 */
static const unsigned int BUCK1_table[] = {
	500000, 510000, 520000, 530000, 540000, 550000, 560000, 570000, 580000,
	590000, 600000, 610000, 620000, 630000, 640000, 650000, 660000, 670000,
	680000, 690000, 700000, 710000, 720000, 730000, 740000, 750000, 760000,
	770000, 780000, 790000, 800000, 810000, 820000, 830000, 840000, 850000,
	860000, 870000, 880000, 890000, 900000, 910000, 920000, 930000, 940000,
	950000, 960000, 970000, 980000, 990000, 1000000, 1010000, 1020000,
	1030000, 1040000, 1050000, 1060000, 1070000, 1080000, 1090000, 1100000,
	1110000, 1120000, 1130000, 1140000, 1150000, 1160000, 1170000, 1180000,
	1190000, 1200000, /* 1210000, */
	1220000, 1240000, 1260000, 1280000, 1300000,
};

static const unsigned int BUCK5_table[] = {
	500000, 510000, 520000, 530000, 540000, 550000, 560000, 570000, 580000,
	590000, 600000, 610000, 620000, 630000, 640000, 650000, 660000, 670000,
	680000, 690000, 700000, 710000, 720000, 730000, 740000, 750000, 760000,
	770000, 780000, 790000, 800000, 810000, 820000, 830000, 840000, 850000,
	860000, 870000, 880000, 890000, 900000, 910000, 920000, 930000, 940000,
	950000, 960000, 970000, 980000, 990000, 1000000, 1010000, 1020000,
	1030000, 1040000, 1050000, 1060000, 1070000, 1080000, 1090000, 1100000,
	1110000, 1120000, 1130000, 1140000, 1150000, 1160000, 1170000, 1180000,
	1190000, 1200000, /* 1210000, */
	1220000, 1240000, 1260000, 1280000, 1300000,
};

/* BUCK2 == BUCK3 */
static const unsigned int BUCK2_table[] = {
	600000, 610000, 620000, 630000, 640000, 650000, 660000, 670000,
	680000, 690000, 700000, 710000, 720000, 730000, 740000, 750000, 760000,
	770000, 780000, 790000, 800000, 810000, 820000, 830000, 840000, 850000,
	860000, 870000, 880000, 890000, 900000, 910000, 920000, 930000, 940000,
	950000, 960000, 970000, 980000, 990000, 1000000, 1010000, 1020000,
	1030000, 1040000, 1050000, 1060000, 1070000, 1080000, 1090000, 1100000,
	1120000, 1140000, 1160000, 1180000, 1200000, 1220000, 1240000, 1260000,
	1280000, 1300000, 1320000, 1340000, 1360000, 1380000, 1400000, 1420000,
	1440000, 1460000, 1480000, 1500000, 1520000,
};

static const unsigned int BUCK3_table[] = {
	600000, 610000, 620000, 630000, 640000, 650000, 660000, 670000,
	680000, 690000, 700000, 710000, 720000, 730000, 740000, 750000, 760000,
	770000, 780000, 790000, 800000, 810000, 820000, 830000, 840000, 850000,
	860000, 870000, 880000, 890000, 900000, 910000, 920000, 930000, 940000,
	950000, 960000, 970000, 980000, 990000, 1000000, 1010000, 1020000,
	1030000, 1040000, 1050000, 1060000, 1070000, 1080000, 1090000, 1100000,
	1120000, 1140000, 1160000, 1180000, 1200000, 1220000, 1240000, 1260000,
	1280000, 1300000, 1320000, 1340000, 1360000, 1380000, 1400000, 1420000,
	1440000, 1460000, 1480000, 1500000, 1520000,
};

static const unsigned int BUCK4_table[] = {
	800000, 810000, 820000, 830000, 840000, 850000, 860000, 870000, 880000,
	890000, 900000, 910000, 920000, 930000, 940000, 950000, 960000, 970000,
	980000, 990000, 1000000, 1010000, 1020000, 1030000, 1040000, 1050000,
	1060000, 1070000, 1080000, 1090000, 1100000, 1110000, 1120000, 1130000,
	1140000, 1160000, 1180000, 1200000, 1220000, 1240000, 1280000, 1300000,
	1320000, 1340000, 1360000, 1380000, 1400000, 1420000, 1440000, 1460000,
	1480000, 1500000, 1520000, 1540000, 1560000, 1580000, 1600000, 1620000,
	1640000, 1660000, 1680000, 1700000, 1720000, 1740000, 1760000, 1780000,
	1800000, 1820000, 1840000,
};

static const unsigned int BUCK6_table[] = {
	1600000, 1700000, 1800000,
};

/* Can be LDO134 table */
static const unsigned int LDO1_table[] = {
	700000, 800000, 900000, 1000000, 1100000, 1200000, 1300000, 1400000,
	1500000, 1600000, 1700000, 1800000, 1900000, 2000000, 2100000, 2200000,
	2300000, 2400000, 2500000, 2600000, 2700000, 2800000, 2900000, 3000000,
	3100000, 3200000, 3300000,
};

static const unsigned int LDO2_table[] = {
	700000, 800000, 900000, 1000000, 1100000, 1200000, 1300000, 1400000,
	1500000, 1600000, 1700000, 1800000, 1900000, 2000000, 2100000, 2200000,
	2300000, 2400000, 2500000, 2600000, 2700000, 2800000, 2900000, 3000000,
	3100000, 3200000, 3300000, 3400000, 3600000, 3800000, 4000000, 4200000,
};

/* This regulator, LDO3, supplies power to the IO signal of SD card.
 * And for VSDIO, it should only support 1.8V and 3.3V. All other voltages
 * are invalid for sd card. So, fix outlet voltages to 1.8V and 3.3V.
 */
static const unsigned int LDO3_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 1800000, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 3300000,
};

static const unsigned int LDO4_table[] = {
	700000, 800000, 900000, 1000000, 1100000, 1200000, 1300000, 1400000,
	1500000, 1600000, 1700000, 1800000, 1900000, 2000000, 2100000, 2200000,
	2300000, 2400000, 2500000, 2600000, 2700000, 2800000, 2900000, 3000000,
	3100000, 3200000, 3300000,
};

/* ELDO 123 table */
static const unsigned int LDO5_table[] = {
	700000, 750000, 800000, 850000, 900000, 950000, 1000000, 1050000,
	1100000, 1150000, 1200000, 1250000, 1300000, 1350000, 1400000, 1450000,
	1500000, 1550000, 1600000, 1650000, 1700000, 1750000, 1800000, 1850000,
	1900000,
};

static const unsigned int LDO6_table[] = {
	700000, 750000, 800000, 850000, 900000, 950000, 1000000, 1050000,
	1100000, 1150000, 1200000, 1250000, 1300000, 1350000, 1400000, 1450000,
	1500000, 1550000, 1600000, 1650000, 1700000, 1750000, 1800000, 1850000,
	1900000,
};

static const unsigned int LDO7_table[] = {
	700000, 750000, 800000, 850000, 900000, 950000, 1000000, 1050000,
	1100000, 1150000, 1200000, 1250000, 1300000, 1350000, 1400000, 1450000,
	1500000, 1550000, 1600000, 1650000, 1700000, 1750000, 1800000, 1850000,
	1900000,
};

static const unsigned int LDO8_table[] = {
	700000, 750000, 800000, 850000, 900000, 950000, 1000000, 1050000,
	1100000, 1150000, 1200000, 1250000, 1300000, 1350000, 1400000, 1450000,
};

static const unsigned int LDO9_table[] = {
	700000, 750000, 800000, 850000, 900000, 950000, 1000000, 1050000,
	1100000, 1150000, 1200000, 1250000, 1300000, 1350000, 1400000, 1450000,
};

static const unsigned int LDO10_table[] = {
	700000, 750000, 800000, 850000, 900000, 950000, 1000000, 1050000,
	1100000, 1150000, 1200000, 1250000, 1300000, 1350000, 1400000, 1450000,
};

/* ALDO == LDO */
static const unsigned int LDO11_table[] = {
	700000, 800000, 900000, 1000000, 1100000, 1200000, 1300000, 1400000,
	1500000, 1600000, 1700000, 1800000, 1900000, 2000000, 2100000, 2200000,
	2300000, 2400000, 2500000, 2600000, 2700000, 2800000, 2900000, 3000000,
	3100000, 3200000, 3300000,

};

static const unsigned int LDO12_table[] = {
	700000, 800000, 900000, 1000000, 1100000, 1200000, 1300000, 1400000,
	1500000, 1600000, 1700000, 1800000, 1900000, 2000000, 2100000, 2200000,
	2300000, 2400000, 2500000, 2600000, 2700000, 2800000, 2900000, 3000000,
	3100000, 3200000, 3300000,

};

static const unsigned int LDO13_table[] = {
	700000, 800000, 900000, 1000000, 1100000, 1200000, 1300000, 1400000,
	1500000, 1600000, 1700000, 1800000, 1900000, 2000000, 2100000, 2200000,
	2300000, 2400000, 2500000, 2600000, 2700000, 2800000, 2900000, 3000000,
	3100000, 3200000, 3300000,

};

static const unsigned int GPIO1_table[] = {
	700000, 800000, 900000, 1000000, 1100000, 1200000, 1300000, 1400000,
	1500000, 1600000, 1700000, 1800000, 1900000, 2000000, 2100000, 2200000,
	2300000, 2400000, 2500000, 2600000, 2700000, 2800000, 2900000, 3000000,
	3100000, 3200000, 3300000,
};

static inline int dcovex_is_gpio_regulator(struct dcovex_regulator_info *info)
{
	return ((info->enable_reg == DCOVEX_GPIO0_EN_REG) ||
			(info->enable_reg == DCOVEX_GPIO1_EN_REG)) ?
		true : false;
}

static int dcovex_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct dcovex_regulator_info *info = rdev_get_drvdata(rdev);
	int reg_val;

	reg_val = intel_soc_pmic_readb(info->enable_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	if (dcovex_is_gpio_regulator(info))
		return reg_val == DCOVEX_GPIO_EN_VAL;
	else
		return !!(reg_val & (1 << info->enable_bit));
}

static int dcovex_regulator_enable(struct regulator_dev *rdev)
{
	struct dcovex_regulator_info *info = rdev_get_drvdata(rdev);
	int reg_val;

	/* enable_reg is output power on-off control register */
	reg_val = intel_soc_pmic_readb(info->enable_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	if (dcovex_is_gpio_regulator(info))
		reg_val = DCOVEX_GPIO_EN_VAL;
	else
		reg_val |= (1 << info->enable_bit);

	return intel_soc_pmic_writeb(info->enable_reg, reg_val);
}

static int dcovex_regulator_disable(struct regulator_dev *rdev)
{
	struct dcovex_regulator_info *info = rdev_get_drvdata(rdev);
	int reg_val;

	reg_val = intel_soc_pmic_readb(info->enable_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	if (dcovex_is_gpio_regulator(info))
		reg_val = DCOVEX_GPIO_DIS_VAL;
	else
		reg_val &= ~(1 << info->enable_bit);

	return intel_soc_pmic_writeb(info->enable_reg, reg_val);
}

static int dcovex_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	struct dcovex_regulator_info *info = rdev_get_drvdata(rdev);
	int reg_val, mask;

	/* vol_reg is voltage control registers */
	reg_val = intel_soc_pmic_readb(info->vol_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	mask = ((1 << info->vol_nbits) - 1) << info->vol_shift;
	reg_val = (reg_val & mask) >> info->vol_shift;

	return reg_val;
}

static int dcovex_regulator_set_voltage_sel(struct regulator_dev *rdev,
				unsigned selector)
{
	struct dcovex_regulator_info *info = rdev_get_drvdata(rdev);
	int reg_val, mask;

	reg_val = intel_soc_pmic_readb(info->vol_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	mask = ((1 << info->vol_nbits) - 1) << info->vol_shift;
	reg_val &= ~mask;
	reg_val |= (selector << info->vol_shift);

	if (dcovex_is_gpio_regulator(info)) {
		int ret = intel_soc_pmic_writeb(info->vol_reg, reg_val);
		/* Seems GPIO regulator needs a delay to be stable */
		usleep_range(20000, 21000);
		return ret;
	} else
		return intel_soc_pmic_writeb(info->vol_reg, reg_val);
}

static struct regulator_ops dcovex_regulator_ops = {
	.enable			= dcovex_regulator_enable,
	.disable		= dcovex_regulator_disable,
	.is_enabled		= dcovex_regulator_is_enabled,
	.get_voltage_sel	= dcovex_regulator_get_voltage_sel,
	.set_voltage_sel	= dcovex_regulator_set_voltage_sel,
	.list_voltage		= regulator_list_voltage_table,
};

/* TODO: If the mfd is regmap complaint, most of the mask, shifts not needed */
#define DCOVEX_BUCK(_id, vreg, shift, nbits, ereg, ebit)		\
{									\
	.desc	= {							\
		.name	= "BUCK" #_id,					\
		.ops	= &dcovex_regulator_ops,			\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= DCOVEX_ID_BUCK##_id,				\
		.n_voltages = ARRAY_SIZE(BUCK##_id##_table),		\
		.volt_table = BUCK##_id##_table,			\
		.owner	= THIS_MODULE,					\
	},								\
	.vol_reg	= DCOVEX_##vreg##_VOL_CTRL,			\
	.vol_shift	= (shift),					\
	.vol_nbits	= (nbits),					\
	.enable_reg	= (ereg),					\
	.enable_bit	= (ebit),					\
}

#define DCOVEX_LDO(_id, vreg, shift, nbits, ereg, ebit)			\
{									\
	.desc	= {							\
		.name	= "LDO" #_id,					\
		.ops	= &dcovex_regulator_ops,			\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= DCOVEX_ID_LDO##_id,				\
		.n_voltages = ARRAY_SIZE(LDO##_id##_table),		\
		.volt_table = LDO##_id##_table,				\
		.owner	= THIS_MODULE,					\
	},								\
	.vol_reg	= DCOVEX_##vreg##_VOL_CTRL,			\
	.vol_shift	= (shift),					\
	.vol_nbits	= (nbits),					\
	.enable_reg	= (ereg),					\
	.enable_bit	= (ebit),					\
}

#define DCOVEX_GPIO(_id, vreg, shift, nbits, ereg, ebit)		\
{									\
	.desc	= {							\
		.name	= "GPIO" #_id,					\
		.ops	= &dcovex_regulator_ops,			\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= DCOVEX_ID_GPIO##_id,				\
		.n_voltages = ARRAY_SIZE(GPIO##_id##_table),		\
		.volt_table = GPIO##_id##_table,			\
		.owner	= THIS_MODULE,					\
	},								\
	.vol_reg	= DCOVEX_##vreg##_VOL_CTRL,			\
	.vol_shift	= (shift),					\
	.vol_nbits	= (nbits),					\
	.enable_reg	= (ereg),					\
	.enable_bit	= (ebit),					\
}

static struct dcovex_regulator_info dcovex_regulator_info[] = {
	/* BUCK */
	DCOVEX_BUCK(1, BUCK1, 0, 7, 0x10, 3),
	DCOVEX_BUCK(2, BUCK2, 0, 7, 0x10, 6),
	DCOVEX_BUCK(3, BUCK3, 0, 7, 0x10, 5),
	DCOVEX_BUCK(4, BUCK4, 0, 7, 0x10, 4),
	DCOVEX_BUCK(5, BUCK5, 0, 7, 0x10, 1),
	DCOVEX_BUCK(6, BUCK6, 0, 5, 0x10, 0),

	/* DLDO */
	DCOVEX_LDO(1, LDO1, 0, 5, 0x12, 3),
	DCOVEX_LDO(2, LDO2, 0, 5, 0x12, 4),
	DCOVEX_LDO(3, LDO3, 0, 5, 0x12, 5),
	DCOVEX_LDO(4, LDO4, 0, 5, 0x12, 6),

	/* ELDO */
	DCOVEX_LDO(5, LDO5, 0, 5, 0x12, 0),
	DCOVEX_LDO(6, LDO6, 0, 5, 0x12, 1),
	DCOVEX_LDO(7, LDO7, 0, 5, 0x12, 2),

	/* ALDO */
	DCOVEX_LDO(8, LDO8, 0, 5, 0x13, 5),
	DCOVEX_LDO(9, LDO9, 0, 5, 0x13, 6),
	DCOVEX_LDO(10, LDO10, 0, 5, 0x13, 7),

	/* FLDO */
	DCOVEX_LDO(11, LDO11, 0, 4, 0x13, 2),
	DCOVEX_LDO(12, LDO12, 0, 5, 0x13, 3),
	DCOVEX_LDO(13, LDO13, 0, 5, 0x13, 4),

	/* GPIO */
	DCOVEX_GPIO(1, GPIO1, 0, 5, 0x92, 4),
};

static inline struct dcovex_regulator_info *find_regulator_info(int id)
{
	struct dcovex_regulator_info *di;
	int i;

	for (i = 0; i < ARRAY_SIZE(dcovex_regulator_info); i++) {
		di = &dcovex_regulator_info[i];
		if (di->desc.id == id)
			return di;
	}
	return NULL;
}

static int dcovex_regulator_probe(struct platform_device *pdev)
{
	struct dcovex_regulator_info *pdata = dev_get_platdata(&pdev->dev);
	struct regulator_config config = { };
	struct dcovex_regulator_info *di = NULL;

	if (!pdata) {
		dev_err(&pdev->dev, "No dcovex_regulator_info\n");
		return -EINVAL;
	}

	di = find_regulator_info(pdev->id);
	if (di == NULL) {
		dev_err(&pdev->dev, "invalid regulator\n");
		return -EINVAL;
	}

	config.dev = &pdev->dev;
	config.init_data = pdata->init_data;
	config.driver_data = di;

	pdata->regulator = regulator_register(&di->desc, &config);
	if (IS_ERR(pdata->regulator)) {
		dev_err(&pdev->dev, "failed to register regulator %s\n",
			pdata->desc.name);
		return PTR_ERR(pdata->regulator);
	}

	platform_set_drvdata(pdev, pdata->regulator);

	dev_dbg(&pdev->dev, "registered dollarcove regulator as %s\n",
			dev_name(&pdata->regulator->dev));

	return 0;
}

static int dcovex_regulator_remove(struct platform_device *pdev)
{
	regulator_unregister(platform_get_drvdata(pdev));
	return 0;
}

static const struct platform_device_id dcovex_regulator_id_table[] = {
	{ "dcovex_regulator", 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, dcovex_regulator_id_table);

static struct platform_driver dcovex_regulator_driver = {
	.driver	= {
		.name	= "dcovex_regulator",
		.owner	= THIS_MODULE,
	},
	.probe		= dcovex_regulator_probe,
	.remove		= dcovex_regulator_remove,
	.id_table	= dcovex_regulator_id_table,
};

static int __init dcovex_regulator_init(void)
{
	return platform_driver_register(&dcovex_regulator_driver);
}
module_init(dcovex_regulator_init);

static void __exit dcovex_regulator_exit(void)
{
	platform_driver_unregister(&dcovex_regulator_driver);
}
module_exit(dcovex_regulator_exit);

MODULE_DESCRIPTION("DollarCove XB regulator driver");
MODULE_AUTHOR("Srinidhi Kasagar <srinidhi.kasagar@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:dcovex_regulator");
