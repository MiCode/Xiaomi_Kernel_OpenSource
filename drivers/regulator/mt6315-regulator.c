// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/mfd/mt6315/registers.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6315-misc.h>
#include <linux/regulator/mt6315-regulator.h>
#include <linux/regulator/of_regulator.h>

#define MT6315_BUCK_MODE_AUTO		0
#define MT6315_BUCK_MODE_FORCE_PWM	1
#define MT6315_BUCK_MODE_NORMAL		0
#define MT6315_BUCK_MODE_LP		2

/*
 * MT6315 regulators' information
 *
 * @desc: standard fields of regulator description.
 * @da_reg: for query status of regulators.
 * @qi: Mask for query enable signal status of regulators.
 * @modeset_reg: for operating AUTO/PWM mode register.
 * @modeset_mask: MASK for operating modeset register.
 * @modeset_shift: SHIFT for operating modeset register.
 */
struct mt6315_regulator_info {
	struct regulator_desc desc;
	struct regulation_constraints constraints;
	u32 da_vsel_reg;
	u32 da_vsel_mask;
	u32 da_vsel_shift;
	u32 da_reg;
	u32 qi;
	u32 modeset_reg;
	u32 modeset_mask;
	u32 modeset_shift;
	u32 lp_mode_reg;
	u32 lp_mode_mask;
	u32 lp_mode_shift;
};

/*
 * MTK regulators' init data
 *
 * @id: chip slave id
 * @size: num of regulators
 * @regulator_info: regulator info.
 */
struct mt_regulator_init_data {
	u32 id;
	u32 size;
	struct mt6315_regulator_info *regulator_info;
};

struct mt_regulator_drv_data {
	struct mutex lock;
	u32 reg_value;
	u32 slave_id;
};

#define MT_BUCK_EN		(REGULATOR_CHANGE_STATUS)
#define MT_BUCK_VOL		(REGULATOR_CHANGE_VOLTAGE)
#define MT_BUCK_VOL_EN		(REGULATOR_CHANGE_STATUS |	\
				 REGULATOR_CHANGE_VOLTAGE)
#define MT_BUCK_VOL_EN_MODE	(REGULATOR_CHANGE_STATUS |	\
				 REGULATOR_CHANGE_VOLTAGE |	\
				 REGULATOR_CHANGE_MODE)

#define MT_BUCK(match, _name, min, max, step, volt_ranges, _bid, mode)	\
[MT6315_ID_##_name] = {							\
	.desc = {							\
		.name = #_name,						\
		.of_match = of_match_ptr(match),			\
		.ops = &mt6315_volt_range_ops,				\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6315_ID_##_name,				\
		.owner = THIS_MODULE,					\
		.uV_step = (step),					\
		.linear_min_sel = (0x30),				\
		.n_voltages = ((max) - (min)) / (step) + 1,		\
		.min_uV = (min),					\
		.linear_ranges = volt_ranges,				\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),		\
		.vsel_reg = MT6315_PMIC_RG_BUCK_VBUCK##_bid##_VOSEL_ADDR,\
		.vsel_mask = \
			(MT6315_PMIC_RG_BUCK_VBUCK##_bid##_VOSEL_MASK << \
			MT6315_PMIC_RG_BUCK_VBUCK##_bid##_VOSEL_SHIFT),	\
		.enable_reg = MT6315_PMIC_RG_BUCK_VBUCK##_bid##_EN_ADDR,\
		.enable_mask = BIT(MT6315_PMIC_RG_BUCK_VBUCK##_bid##_EN_SHIFT),\
	},								\
	.constraints = {						\
		.valid_ops_mask = (mode),				\
		.valid_modes_mask =					\
			(REGULATOR_MODE_NORMAL |			\
			 REGULATOR_MODE_FAST |				\
			 REGULATOR_MODE_IDLE),				\
	},								\
	.da_vsel_reg = MT6315_PMIC_DA_VBUCK##_bid##_VOSEL_ADDR,		\
	.da_vsel_mask = MT6315_PMIC_DA_VBUCK##_bid##_VOSEL_MASK,	\
	.da_vsel_shift = MT6315_PMIC_DA_VBUCK##_bid##_VOSEL_SHIFT,	\
	.da_reg = MT6315_PMIC_DA_VBUCK##_bid##_EN_ADDR,			\
	.qi = BIT(0),							\
	.lp_mode_reg = MT6315_PMIC_RG_BUCK_VBUCK##_bid##_LP_ADDR,	\
	.lp_mode_mask = BIT(MT6315_PMIC_RG_BUCK_VBUCK##_bid##_LP_SHIFT),\
	.lp_mode_shift = MT6315_PMIC_RG_BUCK_VBUCK##_bid##_LP_SHIFT,	\
	.modeset_reg = MT6315_PMIC_RG_VBUCK##_bid##_FCCM_ADDR,		\
	.modeset_mask = BIT(MT6315_PMIC_RG_VBUCK##_bid##_FCCM_SHIFT),	\
	.modeset_shift = MT6315_PMIC_RG_VBUCK##_bid##_FCCM_SHIFT,	\
}

static const struct regulator_linear_range mt_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(300000, 0x30, 0xbf, 6250),
};

static int mt6315_regulator_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	if (rdev->use_count == 0) {
		dev_notice(&rdev->dev, "%s:%s should not be disable.(use_count=0)\n"
			, __func__
			, rdev->desc->name);
		ret = -1;
	} else {
		ret = regulator_disable_regmap(rdev);
	}

	return ret;
}

static int mt6315_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	struct mt6315_regulator_info *info = rdev_get_drvdata(rdev);
	int ret, regval;

	ret = regmap_read(rdev->regmap, info->da_vsel_reg, &regval);
	if (ret != 0) {
		dev_notice(&rdev->dev,
			"Failed to get mt6315 regulator voltage: %d\n", ret);
		return ret;
	}

	ret = (regval >> info->da_vsel_shift) & info->da_vsel_mask;

	return ret;
}

static unsigned int mt6315_regulator_get_mode(struct regulator_dev *rdev)
{
	struct mt6315_regulator_info *info = rdev_get_drvdata(rdev);
	int ret, regval;

	ret = regmap_read(rdev->regmap, info->modeset_reg, &regval);
	if (ret != 0) {
		dev_notice(&rdev->dev,
			"Failed to get mt6315 buck mode: %d\n", ret);
		return ret;
	}

	if ((regval & info->modeset_mask) >> info->modeset_shift ==
		MT6315_BUCK_MODE_FORCE_PWM)
		return REGULATOR_MODE_FAST;


	ret = regmap_read(rdev->regmap, info->lp_mode_reg, &regval);
	if (ret != 0) {
		dev_notice(&rdev->dev,
			"Failed to get mt6315 buck lp mode: %d\n", ret);
		return ret;
	}

	if (regval & info->lp_mode_mask)
		return REGULATOR_MODE_IDLE;
	else
		return REGULATOR_MODE_NORMAL;
}

static int mt6315_regulator_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct mt6315_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0, val;
	int curr_mode;

	curr_mode = mt6315_regulator_get_mode(rdev);
	switch (mode) {
	case REGULATOR_MODE_FAST:
		if (curr_mode == REGULATOR_MODE_IDLE) {
			WARN_ON(1);
			dev_notice(&rdev->dev,
				   "BUCK %s is LP mode, can't FPWM\n",
				   rdev->desc->name);
			return -EIO;
		}
		val = MT6315_BUCK_MODE_FORCE_PWM;
		val <<= info->modeset_shift;
		ret = regmap_update_bits(rdev->regmap,
					 info->modeset_reg,
					 info->modeset_mask,
					 val);
		break;
	case REGULATOR_MODE_NORMAL:
		if (curr_mode == REGULATOR_MODE_FAST) {
			val = MT6315_BUCK_MODE_AUTO;
			val <<= info->modeset_shift;
			ret = regmap_update_bits(rdev->regmap,
						 info->modeset_reg,
						 info->modeset_mask,
						 val);
		} else if (curr_mode == REGULATOR_MODE_IDLE) {
			val = MT6315_BUCK_MODE_NORMAL;
			val <<= info->lp_mode_shift;
			ret = regmap_update_bits(rdev->regmap,
						 info->lp_mode_reg,
						 info->lp_mode_mask,
						 val);
			udelay(100);
		}
		break;
	case REGULATOR_MODE_IDLE:
		if (curr_mode == REGULATOR_MODE_FAST) {
			WARN_ON(1);
			dev_notice(&rdev->dev,
				   "BUCK %s is FPWM mode, can't enter LP\n",
				   rdev->desc->name);
			return -EIO;
		}
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
		dev_notice(&rdev->dev,
			"Failed to set mt6315 buck mode: %d\n", ret);
		return ret;
	}

	return 0;
}

static int mt6315_get_status(struct regulator_dev *rdev)
{
	int ret;
	u32 regval;
	struct mt6315_regulator_info *info = rdev_get_drvdata(rdev);

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
	.enable = regulator_enable_regmap,
	.disable = mt6315_regulator_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6315_get_status,
	.set_mode = mt6315_regulator_set_mode,
	.get_mode = mt6315_regulator_get_mode,
};

/* The array is indexed by id(MT6315_ID_SID_XXX) */
static struct mt6315_regulator_info mt6315_6_regulators[] = {
	MT_BUCK("6_vbuck1", 6_VBUCK1, 300000, 1193750, 6250,
		mt_volt_range1, 1, MT_BUCK_VOL_EN_MODE),
	MT_BUCK("6_vbuck3", 6_VBUCK3, 300000, 1193750, 6250,
		mt_volt_range1, 3, MT_BUCK_VOL_EN_MODE),
};

static struct mt6315_regulator_info mt6315_7_regulators[] = {
	MT_BUCK("7_vbuck1", 7_VBUCK1, 300000, 1193750, 6250,
		mt_volt_range1, 1, MT_BUCK_VOL_EN_MODE),
	MT_BUCK("7_vbuck3", 7_VBUCK3, 300000, 1193750, 6250,
		mt_volt_range1, 3, MT_BUCK_VOL_EN),
};

static struct mt6315_regulator_info mt6315_3_regulators[] = {
	MT_BUCK("3_vbuck1", 3_VBUCK1, 300000, 1193750, 6250,
		mt_volt_range1, 1, MT_BUCK_VOL_EN),
	MT_BUCK("3_vbuck3", 3_VBUCK3, 300000, 1193750, 6250,
		mt_volt_range1, 3, MT_BUCK_VOL_EN),
	MT_BUCK("3_vbuck4", 3_VBUCK4, 300000, 1193750, 6250,
		mt_volt_range1, 4, MT_BUCK_VOL_EN),
};

static const struct mt_regulator_init_data mt6315_6_regulator_init_data = {
	.id = MT6315_SLAVE_ID_6,
	.size = MT6315_ID_6_MAX,
	.regulator_info = &mt6315_6_regulators[0],
};

static const struct mt_regulator_init_data mt6315_7_regulator_init_data = {
	.id = MT6315_SLAVE_ID_7,
	.size = MT6315_ID_7_MAX,
	.regulator_info = &mt6315_7_regulators[0],
};

static const struct mt_regulator_init_data mt6315_3_regulator_init_data = {
	.id = MT6315_SLAVE_ID_3,
	.size = MT6315_ID_3_MAX,
	.regulator_info = &mt6315_3_regulators[0],
};

static const struct of_device_id mt6315_of_match[] = {
	{
		.compatible = "mediatek,mt6315_6-regulator",
		.data = &mt6315_6_regulator_init_data,
	}, {
		.compatible = "mediatek,mt6315_7-regulator",
		.data = &mt6315_7_regulator_init_data,
	}, {
		.compatible = "mediatek,mt6315_3-regulator",
		.data = &mt6315_3_regulator_init_data,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, mt6315_of_match);

static ssize_t show_extbuck_access(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct mt_regulator_drv_data *drvdata = dev_get_drvdata(dev);

	pr_info("[%s] 0x%x\n", __func__, drvdata->reg_value);

	return sprintf(buf, "0x%x\n", drvdata->reg_value);
}

static ssize_t store_extbuck_access(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t size)
{
	struct mt_regulator_drv_data *drvdata;
	struct regmap *regmap;
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int sid = 0;
	unsigned int reg_val = 0;
	unsigned int reg_adr = 0;

	pr_info("[%s]\n", __func__);

	if (dev && dev->parent) {
		drvdata = dev_get_drvdata(dev);
		regmap = dev_get_regmap(dev->parent, NULL);
		if (!drvdata || !regmap)
			return -ENODEV;
	}

	if (buf != NULL && size != 0) {
		pr_info("[%s] size is %d, buf is %s\n"
			, __func__, (int)size, buf);

		pvalue = (char *)buf;
		addr = strsep(&pvalue, " ");
		val = strsep(&pvalue, " ");
		sid = drvdata->slave_id;
		if (addr)
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_adr);
		if (val) {
			ret = kstrtou32(val, 16, (unsigned int *)&reg_val);

			pr_info("[%s] write MT6315_S%d reg 0x%x with value 0x%x !\n"
				, __func__, sid, reg_adr, reg_val);
			ret = regmap_write(regmap, reg_adr, reg_val);
		} else {
			mutex_lock(&drvdata->lock);
			ret = regmap_read(regmap, reg_adr, &drvdata->reg_value);
			mutex_unlock(&drvdata->lock);
			pr_info("[%s] read MT6315_S%d reg 0x%x with value 0x%x !\n"
				, __func__, sid, reg_adr, drvdata->reg_value);
		}
	}
	return size;
}

static DEVICE_ATTR(extbuck_access, 0664,
		   show_extbuck_access, store_extbuck_access);

static int mt6315_regulator_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id;
	struct device *dev = &pdev->dev;
	struct regmap *regmap;
	struct mt6315_regulator_info *mt_regulators;
	struct mt_regulator_init_data *regulator_init_data;
	struct mt_regulator_drv_data *drvdata;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct regulation_constraints *c;
	int i;
	u32 reg_value;

	regmap = dev_get_regmap(dev->parent, NULL);
	if (!regmap)
		return -ENODEV;

	drvdata = devm_kzalloc(&pdev->dev, sizeof(struct mt_regulator_drv_data),
			       GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	of_id = of_match_device(mt6315_of_match, &pdev->dev);
	if (!of_id || !of_id->data)
		return -ENODEV;

	regulator_init_data = (struct mt_regulator_init_data *)of_id->data;
	mt_regulators = regulator_init_data->regulator_info;
	drvdata->slave_id = regulator_init_data->id;
	mutex_init(&drvdata->lock);
	platform_set_drvdata(pdev, drvdata);

	/* Read chip revision to update constraints */
	if (regmap_read(regmap, MT6315_SWCID_H, &reg_value) < 0) {
		dev_notice(&pdev->dev, "Failed to read Chip ID\n");
		return -EIO;
	}
	dev_info(&pdev->dev, "Chip ID = 0x%x\n", reg_value);

	for (i = 0; i < regulator_init_data->size; i++) {
		config.dev = &pdev->dev;
		config.driver_data = (mt_regulators + i);
		config.regmap = regmap;
		rdev = devm_regulator_register(&pdev->dev,
				&(mt_regulators + i)->desc, &config);
		if (IS_ERR(rdev)) {
			dev_notice(&pdev->dev, "failed to register %s\n",
				(mt_regulators + i)->desc.name);
			return PTR_ERR(rdev);
		}

		c = rdev->constraints;
		c->valid_ops_mask |=
			(mt_regulators + i)->constraints.valid_ops_mask;
		c->valid_modes_mask |=
			(mt_regulators + i)->constraints.valid_modes_mask;
	}

	mt6315_misc_init(regulator_init_data->id, regmap);
	/* Create sysfs entry */
	device_create_file(&pdev->dev, &dev_attr_extbuck_access);

	return 0;
}

static void mt6315_regulator_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *regmap;
	int ret = 0;

	dev_info(&pdev->dev, "%s\n", __func__);
	regmap = dev_get_regmap(dev->parent, NULL);
	if (!regmap) {
		dev_notice(&pdev->dev, "%s: invalid regmap.\n", __func__);
		return;
	}

	ret = regmap_update_bits(regmap,
		MT6315_PMIC_RG_SEQ_OFF_ADDR,
		0x1 << MT6315_PMIC_RG_SEQ_OFF_SHIFT,
		0x1 << MT6315_PMIC_RG_SEQ_OFF_SHIFT);
	if (ret < 0) {
		dev_notice(&pdev->dev, "%s: enable power off sequence failed.\n"
			   , __func__);
		return;
	}
}

static int mt6315_regulator_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_extbuck_access);

	return 0;
}

static struct platform_driver mt6315_regulator_driver = {
	.driver		= {
		.name	= "mt6315-regulator",
		.of_match_table = of_match_ptr(mt6315_of_match),
	},
	.probe = mt6315_regulator_probe,
	.shutdown = mt6315_regulator_shutdown,
	.remove = mt6315_regulator_remove,
};

module_platform_driver(mt6315_regulator_driver);

MODULE_AUTHOR("Wen Su <wen.su@mediatek.com>");
MODULE_DESCRIPTION("Mediatek MT6315 regulator driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mt6315-regulator");
