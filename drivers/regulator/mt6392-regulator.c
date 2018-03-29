/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Chen Zhong <chen.zhong@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6392/registers.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6392-regulator.h>
#include <linux/regulator/of_regulator.h>

#define MT6392_LDO_MODE_NORMAL	0
#define MT6392_LDO_MODE_LP	1

/*
 * MT6392 regulators' information
 *
 * @desc: standard fields of regulator description.
 * @qi: Mask for query enable signal status of regulators
 * @vselon_reg: Register sections for hardware control mode of bucks
 * @vselctrl_reg: Register for controlling the buck control mode.
 * @vselctrl_mask: Mask for query buck's voltage control mode.
 */
struct mt6392_regulator_info {
	struct regulator_desc desc;
	u32 qi;
	u32 vselon_reg;
	u32 vselctrl_reg;
	u32 vselctrl_mask;
	u32 modeset_reg;
	u32 modeset_mask;
};

#define MT6392_BUCK(match, vreg, min, max, step, volt_ranges, enreg,	\
		vosel, vosel_mask, voselon, vosel_ctrl)			\
[MT6392_ID_##vreg] = {							\
	.desc = {							\
		.name = #vreg,						\
		.of_match = of_match_ptr(match),			\
		.ops = &mt6392_volt_range_ops,				\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6392_ID_##vreg,					\
		.owner = THIS_MODULE,					\
		.n_voltages = (max - min)/step + 1,			\
		.linear_ranges = volt_ranges,				\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),		\
		.vsel_reg = vosel,					\
		.vsel_mask = vosel_mask,				\
		.enable_reg = enreg,					\
		.enable_mask = BIT(0),					\
	},								\
	.qi = BIT(13),							\
	.vselon_reg = voselon,						\
	.vselctrl_reg = vosel_ctrl,					\
	.vselctrl_mask = BIT(1),					\
}

#define MT6392_LDO(match, vreg, ldo_volt_table, enreg, enbit, vosel,	\
		vosel_mask, _modeset_reg, _modeset_mask)		\
[MT6392_ID_##vreg] = {							\
	.desc = {							\
		.name = #vreg,						\
		.of_match = of_match_ptr(match),			\
		.ops = &mt6392_volt_table_ops,				\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6392_ID_##vreg,					\
		.owner = THIS_MODULE,					\
		.n_voltages = ARRAY_SIZE(ldo_volt_table),		\
		.volt_table = ldo_volt_table,				\
		.vsel_reg = vosel,					\
		.vsel_mask = vosel_mask,				\
		.enable_reg = enreg,					\
		.enable_mask = BIT(enbit),				\
	},								\
	.qi = BIT(15),							\
	.modeset_reg = _modeset_reg,					\
	.modeset_mask = _modeset_mask,					\
}

#define MT6392_REG_FIXED(match, vreg, enreg, enbit, volt,		\
		_modeset_reg, _modeset_mask)				\
[MT6392_ID_##vreg] = {							\
	.desc = {							\
		.name = #vreg,						\
		.of_match = of_match_ptr(match),			\
		.ops = &mt6392_volt_fixed_ops,				\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6392_ID_##vreg,					\
		.owner = THIS_MODULE,					\
		.n_voltages = 1,					\
		.enable_reg = enreg,					\
		.enable_mask = BIT(enbit),				\
		.min_uV = volt,						\
	},								\
	.qi = BIT(15),							\
	.modeset_reg = _modeset_reg,					\
	.modeset_mask = _modeset_mask,					\
}

static const struct regulator_linear_range buck_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(700000, 0, 0x7f, 6250),
};

static const struct regulator_linear_range buck_volt_range2[] = {
	REGULATOR_LINEAR_RANGE(1400000, 0, 0x7f, 12500),
};

static const u32 ldo_volt_table1[] = {
	1800000, 1900000, 2000000, 2200000,
};

static const u32 ldo_volt_table2[] = {
	1500000, 1800000, 2500000, 2800000,
};

static const u32 ldo_volt_table3[] = {
	3300000, 3400000, 3500000, 3600000,
};

static const u32 ldo_volt_table4[] = {
	1800000, 3300000,
};

static const u32 ldo_volt_table5[] = {
	3000000, 3300000,
};

static const u32 ldo_volt_table6[] = {
	1200000, 1300000, 1500000, 1800000, 2000000, 2800000, 3000000, 3300000,
};

static const u32 ldo_volt_table7[] = {
	1200000, 1350000, 1500000, 1800000,
};

static const u32 ldo_volt_table8[] = {
	1200000, 1300000, 1500000, 1800000,
};

static const u32 ldo_volt_table9[] = {
	1200000, 1300000, 1500000, 1800000, 2500000, 2800000, 3000000, 2000000,
};

static const u32 ldo_volt_table10[] = {
	1800000, 2000000,
};

static int mt6392_get_status(struct regulator_dev *rdev)
{
	int ret;
	u32 regval;
	struct mt6392_regulator_info *info = rdev_get_drvdata(rdev);

	ret = regmap_read(rdev->regmap, info->desc.enable_reg, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to get enable reg: %d\n", ret);
		return ret;
	}

	return (regval & info->qi) ? REGULATOR_STATUS_ON : REGULATOR_STATUS_OFF;
}

static int mt6392_ldo_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	int ret, val = 0;
	struct mt6392_regulator_info *info = rdev_get_drvdata(rdev);

	if (!info->modeset_mask) {
		dev_err(&rdev->dev, "regulator %s doesn't support set_mode\n",
			info->desc.name);
		return -EINVAL;
	}

	switch (mode) {
	case REGULATOR_MODE_STANDBY:
		val = MT6392_LDO_MODE_LP;
		break;
	case REGULATOR_MODE_NORMAL:
		val = MT6392_LDO_MODE_NORMAL;
		break;
	default:
		return -EINVAL;
	}

	val <<= ffs(info->modeset_mask) - 1;

	ret = regmap_update_bits(rdev->regmap, info->modeset_reg,
				  info->modeset_mask, val);

	return ret;
}

static unsigned int mt6392_ldo_get_mode(struct regulator_dev *rdev)
{
	unsigned int val;
	unsigned int mode;
	int ret;
	struct mt6392_regulator_info *info = rdev_get_drvdata(rdev);

	if (!info->modeset_mask) {
		dev_err(&rdev->dev, "regulator %s doesn't support get_mode\n",
			info->desc.name);
		return -EINVAL;
	}

	ret = regmap_read(rdev->regmap, info->modeset_reg, &val);
	if (ret < 0)
		return ret;

	val &= info->modeset_mask;
	val >>= ffs(info->modeset_mask) - 1;

	if (val & 0x1)
		mode = REGULATOR_MODE_STANDBY;
	else
		mode = REGULATOR_MODE_NORMAL;

	return mode;
}

static struct regulator_ops mt6392_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6392_get_status,
};

static struct regulator_ops mt6392_volt_table_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6392_get_status,
	.set_mode = mt6392_ldo_set_mode,
	.get_mode = mt6392_ldo_get_mode,
};

static struct regulator_ops mt6392_volt_fixed_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6392_get_status,
	.set_mode = mt6392_ldo_set_mode,
	.get_mode = mt6392_ldo_get_mode,
};

/* The array is indexed by id(MT6392_ID_XXX) */
static struct mt6392_regulator_info mt6392_regulators[] = {
	MT6392_BUCK("buck_vproc", VPROC, 700000, 1493750, 6250,
		buck_volt_range1, MT6392_VPROC_CON7, MT6392_VPROC_CON9, 0x7f,
		MT6392_VPROC_CON10, MT6392_VPROC_CON5),
	MT6392_BUCK("buck_vsys", VSYS, 1400000, 2987500, 12500,
		buck_volt_range2, MT6392_VSYS_CON7, MT6392_VSYS_CON9, 0x7f,
		MT6392_VSYS_CON10, MT6392_VSYS_CON5),
	MT6392_BUCK("buck_vcore", VCORE, 700000, 1493750, 6250,
		buck_volt_range1, MT6392_VCORE_CON7, MT6392_VCORE_CON9, 0x7f,
		MT6392_VCORE_CON10, MT6392_VCORE_CON5),
	MT6392_REG_FIXED("ldo_vxo22", VXO22, MT6392_ANALDO_CON1, 10, 2200000,
		MT6392_ANALDO_CON1, 0x2),
	MT6392_LDO("ldo_vaud22", VAUD22, ldo_volt_table1,
		MT6392_ANALDO_CON2, 14, MT6392_ANALDO_CON8, 0x60,
		MT6392_ANALDO_CON2, 0x2),
	MT6392_LDO("ldo_vcama", VCAMA, ldo_volt_table2,
		MT6392_ANALDO_CON4, 15, MT6392_ANALDO_CON10, 0x60, -1, 0),
	MT6392_REG_FIXED("ldo_vaud28", VAUD28, MT6392_ANALDO_CON23, 14, 2800000,
		MT6392_ANALDO_CON23, 0x2),
	MT6392_REG_FIXED("ldo_vadc18", VADC18, MT6392_ANALDO_CON25, 14, 1800000,
		MT6392_ANALDO_CON25, 0x2),
	MT6392_LDO("ldo_vcn35_bt", VCN35_BT, ldo_volt_table3,
		MT6392_ANALDO_CON16, 7, MT6392_ANALDO_CON16, 0xC,
		MT6392_ANALDO_CON21, 0x2),
	MT6392_LDO("ldo_vcn35_wifi", VCN35_WIFI, ldo_volt_table3,
		MT6392_ANALDO_CON17, 12, MT6392_ANALDO_CON16, 0xC,
		MT6392_ANALDO_CON21, 0x2),
	MT6392_REG_FIXED("ldo_vio28", VIO28, MT6392_DIGLDO_CON0, 14, 2800000,
		MT6392_DIGLDO_CON0, 0x2),
	MT6392_REG_FIXED("ldo_vusb", VUSB, MT6392_DIGLDO_CON2, 14, 3300000,
		MT6392_DIGLDO_CON2, 0x2),
	MT6392_LDO("ldo_vmc", VMC, ldo_volt_table4,
		MT6392_DIGLDO_CON3, 12, MT6392_DIGLDO_CON24, 0x10,
		MT6392_DIGLDO_CON3, 0x2),
	MT6392_LDO("ldo_vmch", VMCH, ldo_volt_table5,
		MT6392_DIGLDO_CON5, 14, MT6392_DIGLDO_CON26, 0x80,
		MT6392_DIGLDO_CON5, 0x2),
	MT6392_LDO("ldo_vemc3v3", VEMC3V3, ldo_volt_table5,
		MT6392_DIGLDO_CON6, 14, MT6392_DIGLDO_CON27, 0x80,
		MT6392_DIGLDO_CON6, 0x2),
	MT6392_LDO("ldo_vgp1", VGP1, ldo_volt_table6,
		MT6392_DIGLDO_CON7, 15, MT6392_DIGLDO_CON28, 0xE0,
		MT6392_DIGLDO_CON7, 0x2),
	MT6392_LDO("ldo_vgp2", VGP2, ldo_volt_table6,
		MT6392_DIGLDO_CON8, 15, MT6392_DIGLDO_CON29, 0xE0,
		MT6392_DIGLDO_CON8, 0x2),
	MT6392_REG_FIXED("ldo_vcn18", VCN18, MT6392_DIGLDO_CON11, 14, 1800000,
		MT6392_DIGLDO_CON11, 0x2),
	MT6392_REG_FIXED("ldo_vrtc", VRTC, MT6392_DIGLDO_CON15, 8, 2800000,
		-1, 0),
	MT6392_LDO("ldo_vcamaf", VCAMAF, ldo_volt_table6,
		MT6392_DIGLDO_CON31, 15, MT6392_DIGLDO_CON32, 0xE0,
		MT6392_DIGLDO_CON31, 0x2),
	MT6392_LDO("ldo_vm", VM, ldo_volt_table7,
		MT6392_DIGLDO_CON47, 14, MT6392_DIGLDO_CON48, 0x30,
		MT6392_DIGLDO_CON47, 0x2),
	MT6392_REG_FIXED("ldo_vio18", VIO18, MT6392_DIGLDO_CON49, 14, 1800000,
		MT6392_DIGLDO_CON49, 0x2),
	MT6392_LDO("ldo_vcamd", VCAMD, ldo_volt_table8,
		MT6392_DIGLDO_CON51, 14, MT6392_DIGLDO_CON52, 0x60,
		MT6392_DIGLDO_CON51, 0x2),
	MT6392_REG_FIXED("ldo_vcamio", VCAMIO, MT6392_DIGLDO_CON53, 14, 1800000,
		MT6392_DIGLDO_CON53, 0x2),
	MT6392_LDO("ldo_vm25", VM25, ldo_volt_table9,
		MT6392_DIGLDO_CON55, 14, MT6392_DIGLDO_CON56, 0xE000,
		MT6392_DIGLDO_CON55, 0x2),
	MT6392_LDO("ldo_vefuse", VEFUSE, ldo_volt_table10,
		MT6392_DIGLDO_CON57, 14, MT6392_DIGLDO_CON58, 0x10,
		MT6392_DIGLDO_CON57, 0x2),
};

static int mt6392_set_buck_vosel_reg(struct platform_device *pdev)
{
	struct mt6397_chip *mt6392 = dev_get_drvdata(pdev->dev.parent);
	int i;
	u32 regval;

	for (i = 0; i < MT6392_MAX_REGULATOR; i++) {
		if (mt6392_regulators[i].vselctrl_reg) {
			if (regmap_read(mt6392->regmap,
				mt6392_regulators[i].vselctrl_reg,
				&regval) < 0) {
				dev_err(&pdev->dev,
					"Failed to read buck ctrl\n");
				return -EIO;
			}

			if (regval & mt6392_regulators[i].vselctrl_mask) {
				mt6392_regulators[i].desc.vsel_reg =
				mt6392_regulators[i].vselon_reg;
			}
		}
	}

	return 0;
}

static int mt6392_regulator_probe(struct platform_device *pdev)
{
	struct mt6397_chip *mt6392 = dev_get_drvdata(pdev->dev.parent);
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	int i;
	u32 reg_value;

	/* Query buck controller to select activated voltage register part */
	if (mt6392_set_buck_vosel_reg(pdev))
		return -EIO;

	/* Read PMIC chip revision to update constraints and voltage table */
	if (regmap_read(mt6392->regmap, MT6392_CID, &reg_value) < 0) {
		dev_err(&pdev->dev, "Failed to read Chip ID\n");
		return -EIO;
	}
	dev_info(&pdev->dev, "Chip ID = 0x%x\n", reg_value);

	for (i = 0; i < MT6392_MAX_REGULATOR; i++) {
		config.dev = &pdev->dev;
		config.driver_data = &mt6392_regulators[i];
		config.regmap = mt6392->regmap;
		rdev = devm_regulator_register(&pdev->dev,
				&mt6392_regulators[i].desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev, "failed to register %s\n",
				mt6392_regulators[i].desc.name);
			return PTR_ERR(rdev);
		}
	}
	return 0;
}

static const struct platform_device_id mt6392_platform_ids[] = {
	{"mt6392-regulator", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mt6392_platform_ids);

static const struct of_device_id mt6392_of_match[] = {
	{ .compatible = "mediatek,mt6392-regulator", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mt6392_of_match);

static struct platform_driver mt6392_regulator_driver = {
	.driver = {
		.name = "mt6392-regulator",
		.of_match_table = of_match_ptr(mt6392_of_match),
	},
	.probe = mt6392_regulator_probe,
	.id_table = mt6392_platform_ids,
};

module_platform_driver(mt6392_regulator_driver);

MODULE_AUTHOR("Chen Zhong <chen.zhong@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6392 PMIC");
MODULE_LICENSE("GPL v2");
