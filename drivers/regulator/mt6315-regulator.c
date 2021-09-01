// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6315-regulator.h>
#include <linux/regulator/of_regulator.h>

#define SET_OFFSET	0x1
#define CLR_OFFSET	0x2

#define MT6315_REG_WIDTH	8

#define MT6315_BUCK_MODE_AUTO		0
#define MT6315_BUCK_MODE_FORCE_PWM	1
#define MT6315_BUCK_MODE_NORMAL		0
#define MT6315_BUCK_MODE_LP		2

struct mt6315_regulator_info {
	struct regulator_desc desc;
	u32 da_vsel_reg;
	u32 da_reg;
	u32 qi;
	u32 modeset_reg;
	u32 modeset_mask;
	u32 lp_mode_reg;
	u32 lp_mode_mask;
	u32 lp_mode_shift;
};

struct mt6315_init_data {
	u32 id;
	u32 size;
	u32 buck1_modeset_mask;
	u32 buck3_modeset_mask;
};

struct mt6315_chip {
	struct device *dev;
	struct regmap *regmap;
	u32 slave_id;
};

#define MT_BUCK(match, _name, volt_ranges, _bid, _vsel, _modeset_mask)	\
[MT6315_ID_##_name] = {					\
	.desc = {					\
		.name = #_name,				\
		.of_match = of_match_ptr(match),	\
		.ops = &mt6315_volt_range_ops,		\
		.type = REGULATOR_VOLTAGE,		\
		.id = MT6315_ID_##_name,		\
		.owner = THIS_MODULE,			\
		.n_voltages = 0xbf,			\
		.linear_ranges = volt_ranges,		\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.vsel_reg = _vsel,			\
		.vsel_mask = 0xff,			\
		.enable_reg = MT6315_BUCK_TOP_CON0,	\
		.enable_mask = BIT(_bid - 1),		\
		.of_map_mode = mt6315_map_mode,		\
	},						\
	.da_vsel_reg = MT6315_BUCK_VBUCK##_bid##_DBG0,	\
	.da_reg = MT6315_BUCK_VBUCK##_bid##_DBG4,	\
	.qi = BIT(0),					\
	.lp_mode_reg = MT6315_BUCK_TOP_CON1,		\
	.lp_mode_mask = BIT(_bid - 1),			\
	.lp_mode_shift = _bid - 1,			\
	.modeset_reg = MT6315_BUCK_TOP_4PHASE_ANA_CON42,	\
	.modeset_mask = _modeset_mask,			\
}

static const struct linear_range mt_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 0xbf, 6250),
};

static int mt6315_regulator_enable(struct regulator_dev *rdev)
{
	return regmap_write(rdev->regmap, rdev->desc->enable_reg + SET_OFFSET,
			    rdev->desc->enable_mask);
}

static int mt6315_regulator_disable(struct regulator_dev *rdev)
{
	return regmap_write(rdev->regmap, rdev->desc->enable_reg + CLR_OFFSET,
			    rdev->desc->enable_mask);
}

static unsigned int mt6315_map_mode(u32 mode)
{
	switch (mode) {
	case MT6315_BUCK_MODE_AUTO:
		return REGULATOR_MODE_NORMAL;
	case MT6315_BUCK_MODE_FORCE_PWM:
		return REGULATOR_MODE_FAST;
	case MT6315_BUCK_MODE_LP:
		return REGULATOR_MODE_IDLE;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static int mt6315_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	struct mt6315_regulator_info *info;
	int ret = 0, reg_addr = 0, reg_val = 0, reg_en = 0;

	info = container_of(rdev->desc, struct mt6315_regulator_info, desc);
	ret = regmap_read(rdev->regmap, info->da_reg, &reg_en);
	if (ret != 0) {
		dev_notice(&rdev->dev, "Failed to get enable reg: %d\n", ret);
		return ret;
	}

	if (reg_en & info->qi)
		reg_addr = info->da_vsel_reg;
	else
		reg_addr = rdev->desc->vsel_reg;

	ret = regmap_read(rdev->regmap, reg_addr, &reg_val);
	if (ret != 0) {
		dev_err(&rdev->dev,
			"Failed to get mt6315 regulator voltage: %d\n", ret);
		return ret;
	}

	ret = reg_val & rdev->desc->vsel_mask;
	return ret;
}

static unsigned int mt6315_regulator_get_mode(struct regulator_dev *rdev)
{
	struct mt6315_init_data *pdata = rdev_get_drvdata(rdev);
	struct mt6315_regulator_info *info;
	int ret = 0, regval = 0;
	u32 modeset_mask;

	info = container_of(rdev->desc, struct mt6315_regulator_info, desc);
	ret = regmap_read(rdev->regmap, info->modeset_reg, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev,
			"Failed to get mt6315 buck mode: %d\n", ret);
		return ret;
	}

	if (rdev_get_id(rdev) == MT6315_ID_VBUCK1)
		modeset_mask = pdata->buck1_modeset_mask;
	else if (rdev_get_id(rdev) == MT6315_ID_VBUCK3)
		modeset_mask = pdata->buck3_modeset_mask;
	else
		modeset_mask = info->modeset_mask;

	if ((regval & modeset_mask) == modeset_mask)
		return REGULATOR_MODE_FAST;

	ret = regmap_read(rdev->regmap, info->lp_mode_reg, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev,
			"Failed to get mt6315 buck lp mode: %d\n", ret);
		return ret;
	}

	if (regval & info->lp_mode_mask)
		return REGULATOR_MODE_IDLE;
	else
		return REGULATOR_MODE_NORMAL;
}

static int mt6315_regulator_set_mode(struct regulator_dev *rdev,
				     u32 mode)
{
	struct mt6315_init_data *pdata = rdev_get_drvdata(rdev);
	struct mt6315_regulator_info *info;
	int ret = 0, val, curr_mode;
	u32 modeset_mask;

	info = container_of(rdev->desc, struct mt6315_regulator_info, desc);
	if (rdev_get_id(rdev) == MT6315_ID_VBUCK1)
		modeset_mask = pdata->buck1_modeset_mask;
	else if (rdev_get_id(rdev) == MT6315_ID_VBUCK3)
		modeset_mask = pdata->buck3_modeset_mask;
	else
		modeset_mask = info->modeset_mask;

	curr_mode = mt6315_regulator_get_mode(rdev);
	switch (mode) {
	case REGULATOR_MODE_FAST:
		ret = regmap_update_bits(rdev->regmap,
					 info->modeset_reg,
					 modeset_mask,
					 modeset_mask);
		break;
	case REGULATOR_MODE_NORMAL:
		if (curr_mode == REGULATOR_MODE_FAST) {
			ret = regmap_update_bits(rdev->regmap,
						 info->modeset_reg,
						 modeset_mask,
						 0);
		} else if (curr_mode == REGULATOR_MODE_IDLE) {
			ret = regmap_update_bits(rdev->regmap,
						 info->lp_mode_reg,
						 info->lp_mode_mask,
						 0);
			usleep_range(100, 110);
		}
		break;
	case REGULATOR_MODE_IDLE:
		val = MT6315_BUCK_MODE_LP >> 1;
		val <<= info->lp_mode_shift;
		ret = regmap_update_bits(rdev->regmap,
					 info->lp_mode_reg,
					 info->lp_mode_mask,
					 val);
		break;
	default:
		ret = -EINVAL;
		goto err_mode;
	}

err_mode:
	if (ret != 0) {
		dev_err(&rdev->dev,
			"Failed to set mt6315 buck mode: %d\n", ret);
		return ret;
	}

	return 0;
}

static int mt6315_get_status(struct regulator_dev *rdev)
{
	int ret = 0;
	u32 regval = 0;
	struct mt6315_regulator_info *info;

	info = container_of(rdev->desc, struct mt6315_regulator_info, desc);
	ret = regmap_read(rdev->regmap, info->da_reg, &regval);
	if (ret != 0) {
		dev_notice(&rdev->dev, "Failed to get enable reg: %d\n", ret);
		return ret;
	}

	return (regval & info->qi) ? REGULATOR_STATUS_ON : REGULATOR_STATUS_OFF;
}

static const struct regulator_ops mt6315_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = mt6315_regulator_get_voltage_sel,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = mt6315_regulator_enable,
	.disable = mt6315_regulator_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6315_get_status,
	.set_mode = mt6315_regulator_set_mode,
	.get_mode = mt6315_regulator_get_mode,
};

static struct mt6315_regulator_info mt6315_regulators[] = {
	MT_BUCK("vbuck1", VBUCK1, mt_volt_range1, 1,
		MT6315_BUCK_TOP_ELR0, 0),
	MT_BUCK("vbuck3", VBUCK3, mt_volt_range1, 3,
		MT6315_BUCK_TOP_ELR4, 0x4),
	MT_BUCK("vbuck4", VBUCK4, mt_volt_range1, 4,
		MT6315_BUCK_TOP_ELR6, 0x8),
};

static struct mt6315_init_data mt6315_3_init_data = {
	.id = MT6315_SLAVE_ID_3,
	.size = MT6315_ID_3_MAX,
	.buck1_modeset_mask = 0x3,
	.buck3_modeset_mask = 0x4,
};

static struct mt6315_init_data mt6315_6_init_data = {
	.id = MT6315_SLAVE_ID_6,
	.size = MT6315_ID_6_MAX,
	.buck1_modeset_mask = 0xB,
	.buck3_modeset_mask = 0x4,
};

static struct mt6315_init_data mt6315_7_init_data = {
	.id = MT6315_SLAVE_ID_7,
	.size = MT6315_ID_7_MAX,
	.buck1_modeset_mask = 0x3,
	.buck3_modeset_mask = 0x4,
};

static const struct of_device_id mt6315_of_match[] = {
	{
		.compatible = "mediatek,mt6315_3-regulator",
		.data = &mt6315_3_init_data,
	}, {
		.compatible = "mediatek,mt6315_6-regulator",
		.data = &mt6315_6_init_data,
	}, {
		.compatible = "mediatek,mt6315_7-regulator",
		.data = &mt6315_7_init_data,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, mt6315_of_match);

static int mt6315_regulator_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id;
	struct device *dev = &pdev->dev;
	struct regmap *regmap;
	struct mt6315_init_data *pdata;
	struct mt6315_chip *chip;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct device_node *node = pdev->dev.of_node;
	u32 val = 0;
	int i;

	regmap = dev_get_regmap(dev->parent, NULL);
	if (!regmap)
		return -ENODEV;

	chip = devm_kzalloc(dev, sizeof(struct mt6315_chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	of_id = of_match_device(mt6315_of_match, dev);
	if (!of_id || !of_id->data)
		return -ENODEV;

	pdata = (struct mt6315_init_data *)of_id->data;
	chip->slave_id = pdata->id;
	if (!of_property_read_u32(node, "buck-size", &val))
		pdata->size = val;
	if (!of_property_read_u32(node, "buck1-modeset-mask", &val))
		pdata->buck1_modeset_mask = val;
	if (!of_property_read_u32(node, "buck3-modeset-mask", &val))
		pdata->buck3_modeset_mask = val;
	chip->dev = dev;
	chip->regmap = regmap;
	dev_set_drvdata(dev, chip);

	dev->fwnode = &(dev->of_node->fwnode);
	if (dev->fwnode && !dev->fwnode->dev)
		dev->fwnode->dev = dev;

	config.dev = dev;
	config.driver_data = pdata;
	config.regmap = regmap;
	for (i = 0; i < pdata->size; i++) {
		rdev = devm_regulator_register(dev,
					       &(mt6315_regulators + i)->desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(dev, "failed to register %s\n",
				(mt6315_regulators + i)->desc.name);
			continue;
		}
	}

	return 0;
}

static void mt6315_regulator_shutdown(struct platform_device *pdev)
{
	struct mt6315_chip *chip = dev_get_drvdata(&pdev->dev);
	int ret = 0;

	ret |= regmap_write(chip->regmap,
				MT6315_TOP_TMA_KEY_H, PROTECTION_KEY_H);
	ret |= regmap_write(chip->regmap, MT6315_TOP_TMA_KEY, PROTECTION_KEY);
	ret |= regmap_update_bits(chip->regmap, MT6315_TOP2_ELR7, 1, 1);
	ret |= regmap_write(chip->regmap, MT6315_TOP_TMA_KEY, 0);
	ret |= regmap_write(chip->regmap, MT6315_TOP_TMA_KEY_H, 0);
	if (ret < 0)
		dev_err(&pdev->dev, "%s: SLV_%d enable power off sequence failed.\n",
			__func__, chip->slave_id);
}

static struct platform_driver mt6315_regulator_driver = {
	.driver		= {
		.name	= "mt6315-regulator",
		.of_match_table = mt6315_of_match,
	},
	.probe = mt6315_regulator_probe,
	.shutdown = mt6315_regulator_shutdown,
};

module_platform_driver(mt6315_regulator_driver);

MODULE_AUTHOR("Hsin-Hsiung Wang <hsin-hsiung.wang@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6315 PMIC");
MODULE_LICENSE("GPL");
