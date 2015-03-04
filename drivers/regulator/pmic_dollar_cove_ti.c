/*
 * Regulator driver for DollarCove TI PMIC
 *	(Based on TI Design)
 * Copyright(c) 2015 Intel Corporation
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
#include <linux/regulator/intel_dollar_cove_ti_pmic.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/delay.h>

/* BUCK voltage control registers */

#define DCOVETI_BUCK3_CTRL		0x24
#define DCOVETI_BUCK4_CTRL		0x27
#define DCOVETI_BUCK5_CTRL		0x28
#define DCOVETI_BUCK5_VSEL_CTRL		0x29
#define DCOVETI_BUCK6_CTRL		0x2C

/* LDO voltage control registers */
#define DCOVETI_LDO1_CTRL		0x41
#define DCOVETI_LDO2_CTRL		0x42
#define DCOVETI_LDO3_CTRL		0x43
#define DCOVETI_LDO5_CTRL		0x45
#define DCOVETI_LDO6_CTRL		0x46
#define DCOVETI_LDO7_CTRL		0x47
#define DCOVETI_LDO8_CTRL		0x48
#define DCOVETI_LDO9_CTRL		0x49
#define DCOVETI_LDO10_CTRL		0x4A
#define DCOVETI_LDO11_CTRL		0x4B
#define DCOVETI_LDO12_CTRL		0x4C
#define DCOVETI_LDO13_CTRL		0x4D
#define DCOVETI_LDO14_CTRL		0x4E

static const unsigned int BUCK3_table[] = {
	0, 0, 0, 0, 0, 800000, 810000, 820000, 830000, 840000, 850000, 860000,
	870000, 880000,	890000, 900000, 910000, 920000, 930000, 940000, 950000,
	960000, 970000,	980000, 990000, 1000000, 1010000, 1020000, 1030000,
	1040000, 1050000, 1060000, 1070000, 1080000, 1090000, 1100000, 1110000,
	1120000, 1130000, 1140000, 1160000, 1180000, 1190000, 1200000, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const unsigned int BUCK4_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1150000, 1175000, 1200000,
	1225000, 1250000, 1275000, 1300000, 1325000, 1350000, 1375000, 1400000,
	1425000, 1450000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const unsigned int BUCK5_table[] = {
	400000, 410000, 420000, 430000, 440000, 450000, 460000, 470000, 480000,
	490000, 500000, 510000, 520000, 530000, 540000, 550000, 560000, 570000,
	580000, 590000, 600000, 610000, 620000, 630000, 640000, 650000, 660000,
	670000, 680000, 690000, 700000, 710000, 720000, 730000, 740000, 750000,
	760000, 770000, 780000, 790000, 800000, 810000, 820000, 830000, 840000,
	850000, 860000, 870000, 880000, 890000, 900000, 910000, 920000, 930000,
	940000, 950000, 960000, 970000, 980000, 990000, 1000000, 1010000,
	1020000, 1030000, 1040000, 1050000, 1060000, 1070000, 1080000, 1090000,
	1100000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const unsigned int BUCK6_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1700000,
	1750000, 1800000, 1850000, 1900000, 1950000, 2000000, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const unsigned int LDO1_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1300000, 1350000,
	1250000, 1200000, 1150000, 1100000, 1000000, 1050000, 950000, 900000, 0,
};

static const unsigned int LDO2_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1300000, 1350000,
	1250000, 1200000, 1150000, 1100000, 1000000, 1050000, 950000, 900000, 0,
};

static const unsigned int LDO3_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3300000, 3250000, 3200000,
	3150000, 3100000, 3050000, 3000000, 2950000, 2900000, 2850000, 2800000,
	2750000, 2700000, 2650000, 2600000, 2550000, 2500000, 2450000, 2400000,
	2350000, 2300000, 2250000, 2200000, 2150000, 2100000, 2050000, 2000000,
	1950000, 1900000, 1850000, 1800000, 1750000, 1700000, 1650000, 1600000,
	1550000, 1500000, 1450000, 1400000, 1350000, 1300000, 1250000, 1200000,
	1150000, 1100000, 1050000, 1000000, 950000, 900000, 0,
};

static const unsigned int LDO5_table[] = {
	0, 580000, 600000, 620000, 640000, 660000, 680000, 700000, 720000, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,
};

static const unsigned int LDO6_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3300000, 3250000, 3200000,
	3150000, 3100000, 3050000, 3000000, 2950000, 2900000, 2850000, 2800000,
	2750000, 2700000, 2650000, 2600000, 2550000, 2500000, 2450000, 2400000,
	2350000, 2300000, 2250000, 2200000, 2150000, 2100000, 2050000, 2000000,
	1950000, 1900000, 1850000, 1800000, 1750000, 1700000, 1650000, 1600000,
	1550000, 1500000, 1450000, 1400000, 1350000, 1300000, 1250000, 1200000,
	1150000, 1100000, 1050000, 1000000, 950000, 900000, 0,
};

static const unsigned int LDO7_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3300000, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1800000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const unsigned int LDO8_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3300000, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1800000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const unsigned int LDO9_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3300000, 3250000, 3200000,
	3150000, 3100000, 3050000, 3000000, 2950000, 2900000, 2850000, 2800000,
	2750000, 2700000, 2650000, 2600000, 2550000, 2500000, 2450000, 2400000,
	2350000, 2300000, 2250000, 2200000, 2150000, 2100000, 2050000, 2000000,
	1950000, 1900000, 1850000, 1800000, 1750000, 1700000, 1650000, 1600000,
	1550000, 1500000, 1450000, 1400000, 1350000, 1300000, 1250000, 1200000,
	1150000, 1100000, 1050000, 1000000, 950000, 900000, 0,
};

static const unsigned int LDO10_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3300000, 3250000, 3200000,
	3150000, 3100000, 3050000, 3000000, 2950000, 2900000, 2850000, 2800000,
	2750000, 2700000, 2650000, 2600000, 2550000, 2500000, 2450000, 2400000,
	2350000, 2300000, 2250000, 2200000, 2150000, 2100000, 2050000, 2000000,
	1950000, 1900000, 1850000, 1800000, 1750000, 1700000, 1650000, 1600000,
	1550000, 1500000, 1450000, 1400000, 1350000, 1300000, 1250000, 1200000,
	1150000, 1100000, 1050000, 1000000, 950000, 900000, 0,
};

static const unsigned int LDO11_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3300000, 3250000, 3200000,
	3150000, 3100000, 3050000, 3000000, 2950000, 2900000, 2850000, 2800000,
	2750000, 2700000, 2650000, 2600000, 2550000, 2500000, 2450000, 2400000,
	2350000, 2300000, 2250000, 2200000, 2150000, 2100000, 2050000, 2000000,
	1950000, 1900000, 1850000, 1800000, 1750000, 1700000, 1650000, 1600000,
	1550000, 1500000, 1450000, 1400000, 1350000, 1300000, 1250000, 1200000,
	1150000, 1100000, 1050000, 1000000, 950000, 900000, 0,
};

static const unsigned int LDO12_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3300000, 3250000, 3200000,
	3150000, 3100000, 3050000, 3000000, 2950000, 2900000, 2850000, 2800000,
	2750000, 2700000, 2650000, 2600000, 2550000, 2500000, 2450000, 2400000,
	2350000, 2300000, 2250000, 2200000, 2150000, 2100000, 2050000, 2000000,
	1950000, 1900000, 1850000, 1800000, 1750000, 1700000, 1650000, 1600000,
	1550000, 1500000, 1450000, 1400000, 1350000, 1300000, 1250000, 1200000,
	1150000, 1100000, 1050000, 1000000, 950000, 900000, 0,
};
static const unsigned int LDO13_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3300000, 3250000, 3200000,
	3150000, 3100000, 3050000, 3000000, 2950000, 2900000, 2850000, 2800000,
	2750000, 2700000, 2650000, 2600000, 2550000, 2500000, 2450000, 2400000,
	2350000, 2300000, 2250000, 2200000, 2150000, 2100000, 2050000, 2000000,
	1950000, 1900000, 1850000, 1800000, 1750000, 1700000, 1650000, 1600000,
	1550000, 1500000, 1450000, 1400000, 1350000, 1300000, 1250000, 1200000,
	1150000, 1100000, 1050000, 1000000, 950000, 900000, 0,
};

static const unsigned int LDO14_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3300000, 3250000, 3200000,
	3150000, 3100000, 3050000, 3000000, 2950000, 2900000, 2850000, 2800000,
	2750000, 2700000, 2650000, 2600000, 2550000, 2500000, 2450000, 2400000,
	2350000, 2300000, 2250000, 2200000, 2150000, 2100000, 2050000, 2000000,
	1950000, 1900000, 1850000, 1800000, 1750000, 1700000, 1650000, 1600000,
	1550000, 1500000, 1450000, 1400000, 1350000, 1300000, 1250000, 1200000,
	1150000, 1100000, 1050000, 1000000, 950000, 900000, 0,
};

static int dcoveti_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct dcoveti_regulator_info *info = rdev_get_drvdata(rdev);
	int reg_val;

	reg_val = intel_soc_pmic_readb(info->enable_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	return !!(reg_val & (1 << info->enable_bit));
}

static int dcoveti_regulator_enable(struct regulator_dev *rdev)
{
	struct dcoveti_regulator_info *info = rdev_get_drvdata(rdev);
	int reg_val;

	reg_val = intel_soc_pmic_readb(info->enable_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	reg_val |= (1 << info->enable_bit);

	return intel_soc_pmic_writeb(info->enable_reg, reg_val);
}

static int dcoveti_regulator_disable(struct regulator_dev *rdev)
{
	struct dcoveti_regulator_info *info = rdev_get_drvdata(rdev);
	int reg_val;

	reg_val = intel_soc_pmic_readb(info->enable_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	reg_val &= ~(1 << info->enable_bit);

	return intel_soc_pmic_writeb(info->enable_reg, reg_val);
}

static int dcoveti_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	struct dcoveti_regulator_info *info = rdev_get_drvdata(rdev);
	int reg_val, mask;

	reg_val = intel_soc_pmic_readb(info->vol_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	mask = ((1 << info->vol_nbits) - 1) << info->vol_shift;
	reg_val = (reg_val & mask) >> info->vol_shift;

	return reg_val;
}

static int dcoveti_regulator_set_voltage_sel(struct regulator_dev *rdev,
				unsigned selector)
{
	struct dcoveti_regulator_info *info = rdev_get_drvdata(rdev);
	int reg_val, mask;

	reg_val = intel_soc_pmic_readb(info->vol_reg);
	if (reg_val < 0) {
		dev_err(&rdev->dev, "error reading pmic, %x\n", reg_val);
		return reg_val;
	}

	mask = ((1 << info->vol_nbits) - 1) << info->vol_shift;
	reg_val &= ~mask;
	reg_val |= (selector << info->vol_shift);

	return intel_soc_pmic_writeb(info->vol_reg, reg_val);
}

static struct regulator_ops dcoveti_regulator_ops = {
	.enable			= dcoveti_regulator_enable,
	.disable		= dcoveti_regulator_disable,
	.is_enabled		= dcoveti_regulator_is_enabled,
	.get_voltage_sel	= dcoveti_regulator_get_voltage_sel,
	.set_voltage_sel	= dcoveti_regulator_set_voltage_sel,
	.list_voltage		= regulator_list_voltage_table,
};

/* TODO: If the mfd is regmap complaint, most of the mask, shifts not needed */
#define DCOVETI_BUCK(_id, vreg, shift, nbits, ereg, ebit)		\
{									\
	.desc	= {							\
		.name	= "BUCK" #_id,					\
		.ops	= &dcoveti_regulator_ops,			\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= DCOVETI_ID_BUCK##_id,				\
		.n_voltages = ARRAY_SIZE(BUCK##_id##_table),		\
		.volt_table = BUCK##_id##_table,			\
		.owner	= THIS_MODULE,					\
	},								\
	.vol_reg	= DCOVETI_##vreg##_CTRL,			\
	.vol_shift	= (shift),					\
	.vol_nbits	= (nbits),					\
	.enable_reg	= (ereg),					\
	.enable_bit	= (ebit),					\
}

#define DCOVETI_LDO(_id, vreg, shift, nbits, ereg, ebit)		\
{									\
	.desc	= {							\
		.name	= "LDO" #_id,					\
		.ops	= &dcoveti_regulator_ops,			\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= DCOVETI_ID_LDO##_id,				\
		.n_voltages = ARRAY_SIZE(LDO##_id##_table),		\
		.volt_table = LDO##_id##_table,				\
		.owner	= THIS_MODULE,					\
	},								\
	.vol_reg	= DCOVETI_##vreg##_CTRL,			\
	.vol_shift	= (shift),					\
	.vol_nbits	= (nbits),					\
	.enable_reg	= (ereg),					\
	.enable_bit	= (ebit),					\
}

static struct dcoveti_regulator_info dcoveti_regulator_info[] = {
	/* BUCK */
	DCOVETI_BUCK(3, BUCK3, 2, 6, DCOVETI_BUCK3_CTRL, 0),
	DCOVETI_BUCK(4, BUCK4, 2, 6, DCOVETI_BUCK4_CTRL, 0),
	DCOVETI_BUCK(5, BUCK5_VSEL, 1, 7, DCOVETI_BUCK5_CTRL, 0),
	DCOVETI_BUCK(6, BUCK6, 2, 6, DCOVETI_BUCK6_CTRL, 0),
	/* LDO */
	DCOVETI_LDO(1, LDO1, 1, 6, DCOVETI_LDO1_CTRL, 0),
	DCOVETI_LDO(2, LDO2, 1, 6, DCOVETI_LDO2_CTRL, 0),
	DCOVETI_LDO(3, LDO3, 1, 6, DCOVETI_LDO3_CTRL, 0),
	DCOVETI_LDO(5, LDO5, 1, 6, DCOVETI_LDO5_CTRL, 0),
	DCOVETI_LDO(6, LDO6, 1, 6, DCOVETI_LDO6_CTRL, 0),
	DCOVETI_LDO(7, LDO7, 1, 6, DCOVETI_LDO7_CTRL, 0),
	DCOVETI_LDO(8, LDO8, 1, 6, DCOVETI_LDO8_CTRL, 0),
	DCOVETI_LDO(9, LDO9, 1, 6, DCOVETI_LDO9_CTRL, 0),
	DCOVETI_LDO(10, LDO10, 1, 6, DCOVETI_LDO10_CTRL, 0),
	DCOVETI_LDO(11, LDO11, 1, 6, DCOVETI_LDO11_CTRL, 0),
	DCOVETI_LDO(12, LDO12, 1, 6, DCOVETI_LDO12_CTRL, 0),
	DCOVETI_LDO(13, LDO13, 1, 6, DCOVETI_LDO13_CTRL, 0),
	DCOVETI_LDO(14, LDO13, 1, 6, DCOVETI_LDO14_CTRL, 0),
};

static inline struct dcoveti_regulator_info *find_regulator_info(int id)
{
	struct dcoveti_regulator_info *di;
	int i;

	for (i = 0; i < ARRAY_SIZE(dcoveti_regulator_info); i++) {
		di = &dcoveti_regulator_info[i];
		if (di->desc.id == id)
			return di;
	}
	return NULL;
}

static int dcoveti_regulator_probe(struct platform_device *pdev)
{
	struct dcoveti_regulator_info *pdata = dev_get_platdata(&pdev->dev);
	struct regulator_config config = { };
	struct dcoveti_regulator_info *di = NULL;

	if (!pdata) {
		dev_err(&pdev->dev, "No dcoveti_regulator_info\n");
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

static int dcoveti_regulator_remove(struct platform_device *pdev)
{
	regulator_unregister(platform_get_drvdata(pdev));
	return 0;
}

static const struct platform_device_id dcoveti_regulator_id_table[] = {
	{ "dcoveti_regulator", 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, dcoveti_regulator_id_table);

static struct platform_driver dcoveti_regulator_driver = {
	.driver	= {
		.name	= "dcoveti_regulator",
		.owner	= THIS_MODULE,
	},
	.probe		= dcoveti_regulator_probe,
	.remove		= dcoveti_regulator_remove,
	.id_table	= dcoveti_regulator_id_table,
};

static int __init dcoveti_regulator_init(void)
{
	return platform_driver_register(&dcoveti_regulator_driver);
}
module_init(dcoveti_regulator_init);

static void __exit dcoveti_regulator_exit(void)
{
	platform_driver_unregister(&dcoveti_regulator_driver);
}
module_exit(dcoveti_regulator_exit);

MODULE_DESCRIPTION("DollarCove TI regulator driver");
MODULE_AUTHOR("Ananth Krishna <ananth.krishna.r@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:dcoveti_regulator");
