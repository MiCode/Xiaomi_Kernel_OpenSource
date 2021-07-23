/*
 * Copyright (C) 2019 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif /* CONFIG_RT_REGMAP */
#include "mt6691-regulator.h"


struct regulator_chip {
	struct regulator_desc desc;
	unsigned char vol_reg;
	unsigned char vol_mask;
	unsigned char mode_reg;
	unsigned char mode_bit;
	unsigned char enable_reg;
	unsigned char enable_bit;
};

/*
 * Exported API
 */
static int g_is_mt6691_exist;

int is_mt6691_exist(void)
{
#if defined(MT6691_IS_EXIST_NAME)
	struct regulator *reg;

	reg = regulator_get(NULL, MT6691_IS_EXIST_NAME);
	pr_info("%s: regulator_get=%s\n", __func__, MT6691_IS_EXIST_NAME);
	if (reg == NULL)
		return 0;
	regulator_put(reg);
	return 1;
#else
	pr_notice("g_is_mt6691_exist=%d\n", g_is_mt6691_exist);
	return g_is_mt6691_exist;
#endif
}

static int mt6691_read_device(void *client, u32 addr,
					 int len, void *dst)
{
	struct i2c_client *i2c = (struct i2c_client *)client;

	return i2c_smbus_read_i2c_block_data(i2c, addr, len, dst);
}

static int mt6691_write_device(void *client, u32 addr,
					 int len, const void *src)
{
	struct i2c_client *i2c = (struct i2c_client *)client;

	return i2c_smbus_write_i2c_block_data(i2c, addr, len, src);
}

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(MT6691_REG_VSEL0, 1, RT_WBITS_WR_ONCE, {});
RT_REG_DECL(MT6691_REG_VSEL1, 1, RT_WBITS_WR_ONCE, {});
RT_REG_DECL(MT6691_REG_CTRL1, 1, RT_WBITS_WR_ONCE, {});
RT_REG_DECL(MT6691_REG_ID1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6691_REG_ID2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6691_REG_MONITOR, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6691_REG_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6691_REG_CTRL3, 1, RT_WBITS_WR_ONCE, {});
RT_REG_DECL(MT6691_REG_CTRL4, 1, RT_WBITS_WR_ONCE, {});

static const rt_register_map_t mt6691_regmap[] = {
	RT_REG(MT6691_REG_VSEL0),
	RT_REG(MT6691_REG_VSEL1),
	RT_REG(MT6691_REG_CTRL1),
	RT_REG(MT6691_REG_ID1),
	RT_REG(MT6691_REG_ID2),
	RT_REG(MT6691_REG_MONITOR),
	RT_REG(MT6691_REG_CTRL2),
	RT_REG(MT6691_REG_CTRL3),
	RT_REG(MT6691_REG_CTRL4),
};

static struct rt_regmap_fops mt6691_regmap_fops = {
	.read_device = mt6691_read_device,
	.write_device = mt6691_write_device,
};
#endif /* CONFIG_RT_REGMAP */

int mt6691_read_byte(void *client,
			uint32_t addr, uint32_t *val)
{
	int ret = 0;
	struct i2c_client *i2c = (struct i2c_client *)client;

	ret = mt6691_read_device(i2c, addr, 1, val);
	if (ret < 0)
		pr_notice("%s read 0x%02x fail\n", __func__, addr);
	return ret;
}

int mt6691_write_byte(void *client, uint32_t addr, uint32_t value)
{
	int ret = 0;
	struct i2c_client *i2c = (struct i2c_client *)client;

	ret = mt6691_write_device(i2c, addr, 1, &value);
	if (ret < 0)
		pr_notice("%s write 0x%02x fail\n", __func__, addr);
	return ret;
}

int mt6691_assign_bit(void *client, uint32_t reg, int32_t  mask, uint32_t data)
{
	struct i2c_client *i2c = (struct i2c_client *)client;
	struct mt6691_regulator_info *ri = i2c_get_clientdata(i2c);
	unsigned char tmp = 0;
	uint32_t regval = 0;
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = mt6691_read_byte(i2c, reg, &regval);
	if (ret < 0) {
		pr_notice("%s read fail reg0x%02x data0x%02x\n",
				__func__, reg, data);
		goto OUT_ASSIGN;
	}

	tmp = ((regval & 0xff) & ~mask);
	tmp |= (data & mask);
	ret = mt6691_write_byte(i2c, reg, tmp);
	if (ret < 0)
		pr_notice("%s write fail reg0x%02x data0x%02x\n",
				__func__, reg, tmp);
OUT_ASSIGN:
	mutex_unlock(&ri->io_lock);
	return  ret;
}

#define mt6691_set_bit(i2c, reg, mask) \
	mt6691_assign_bit(i2c, reg, mask, mask)
#define mt6691_clr_bit(i2c, reg, mask) \
	mt6691_assign_bit(i2c, reg, mask, 0x00)

static int mt6691_regmap_init(struct mt6691_regulator_info *info)
{
#ifdef CONFIG_RT_REGMAP
	info->regmap_props = devm_kzalloc(info->dev,
		sizeof(struct rt_regmap_properties), GFP_KERNEL);

	info->regmap_props->name = info->reg_chip->desc.name;
	info->regmap_props->aliases = info->reg_chip->desc.name;
	info->regmap_props->register_num = ARRAY_SIZE(mt6691_regmap);
	info->regmap_props->rm = mt6691_regmap;
	info->regmap_props->rt_regmap_mode = RT_CACHE_WR_THROUGH;

	info->regmap_dev = rt_regmap_device_register(info->regmap_props,
		&mt6691_regmap_fops, info->dev, info->i2c, info);
	if (!info->regmap_dev)
		return -EINVAL;
#endif /* CONFIG_RT_REGMAP */
	return 0;
}

static int mt6691_set_voltage(struct regulator_dev *rdev,
			      int min_uV, int max_uV, unsigned int *selector)
{
	struct mt6691_regulator_info *info = rdev_get_drvdata(rdev);

	if (min_uV < 1300000)
		*selector = (min_uV - 300000) / 5000;
	else
		*selector = (min_uV - 1300000) / 10000 + 200;

	if (*selector > 255)
		*selector = 255;
	return mt6691_write_byte(info->i2c,
				  info->reg_chip->vol_reg, *selector);
}

static int mt6691_get_voltage(struct regulator_dev *rdev)
{
	struct mt6691_regulator_info *info = rdev_get_drvdata(rdev);
	int ret;
	uint32_t reg_val = 0;

	ret = mt6691_read_byte(info->i2c, info->reg_chip->vol_reg, &reg_val);
	if (ret < 0) {
		pr_notice("%s read voltage fail\n", __func__);
		return ret;
	}

	if (reg_val > 200)
		return 1300000 + (reg_val - 200) * 10000;
	return 300000 + reg_val * 5000;
}

static int hl7593_set_voltage(struct regulator_dev *rdev,
			      int min_uV, int max_uV, unsigned int *selector)
{
	struct mt6691_regulator_info *info = rdev_get_drvdata(rdev);

	*selector = (min_uV - 600000) / 6250;

	return mt6691_assign_bit(info->i2c, info->reg_chip->vol_reg,
				 info->reg_chip->vol_mask, *selector);
}

static int hl7593_get_voltage(struct regulator_dev *rdev)
{
	struct mt6691_regulator_info *info = rdev_get_drvdata(rdev);
	int ret;
	uint32_t reg_val = 0;

	ret = mt6691_read_byte(info->i2c, info->reg_chip->vol_reg, &reg_val);
	if (ret < 0) {
		pr_notice("%s read voltage fail\n", __func__);
		return ret;
	}

	reg_val &= info->reg_chip->vol_mask;
	return 600000 + reg_val * 6250;
}

static int mt6691_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct mt6691_regulator_info *info = rdev_get_drvdata(rdev);
	int ret;

	switch (mode) {
	case REGULATOR_MODE_FAST: /* force pwm mode */
		ret = mt6691_set_bit(info->i2c, info->reg_chip->mode_reg,
				     info->reg_chip->mode_bit);
		break;
	case REGULATOR_MODE_NORMAL:
	default:
		ret = mt6691_clr_bit(info->i2c, info->reg_chip->mode_reg,
				     info->reg_chip->mode_bit);
		break;
	}

	return ret;
}

static unsigned int mt6691_get_mode(struct regulator_dev *rdev)
{
	struct mt6691_regulator_info *info = rdev_get_drvdata(rdev);
	int ret;
	uint32_t regval = 0;

	ret = mt6691_read_byte(info->i2c, info->reg_chip->mode_reg, &regval);
	if (ret < 0) {
		pr_notice("%s read mode fail\n", __func__);
		return ret;
	}

	if (regval & info->reg_chip->mode_bit)
		return REGULATOR_MODE_FAST;
	return REGULATOR_MODE_NORMAL;
}

static int mt6691_enable(struct regulator_dev *rdev)
{
	struct mt6691_regulator_info *info = rdev_get_drvdata(rdev);

	return mt6691_set_bit(info->i2c, info->reg_chip->enable_reg,
			      info->reg_chip->enable_bit);
}

static int mt6691_disable(struct regulator_dev *rdev)
{
	struct mt6691_regulator_info *info = rdev_get_drvdata(rdev);

	if (rdev->use_count == 0) {
		pr_info("ext_buck should not be disable (use_count=%d)\n"
			, rdev->use_count);
		return -1;
	}
	return mt6691_clr_bit(info->i2c, info->reg_chip->enable_reg,
			      info->reg_chip->enable_bit);
}

static int mt6691_is_enabled(struct regulator_dev *rdev)
{
	struct mt6691_regulator_info *info = rdev_get_drvdata(rdev);
	int ret;
	uint32_t reg_val;

	ret = mt6691_read_byte(info->i2c, info->reg_chip->enable_reg, &reg_val);
	if (ret < 0)
		return ret;

	return (reg_val & info->reg_chip->enable_bit) ? 1 : 0;
}

static struct regulator_ops mt6691_regulator_ops = {
	.set_voltage = mt6691_set_voltage,
	.get_voltage = mt6691_get_voltage,
	.set_mode = mt6691_set_mode,
	.get_mode = mt6691_get_mode,
	.enable = mt6691_enable,
	.disable = mt6691_disable,
	.is_enabled = mt6691_is_enabled,
};

static struct regulator_ops hl7593_regulator_ops = {
	.set_voltage = hl7593_set_voltage,
	.get_voltage = hl7593_get_voltage,
	.set_mode = mt6691_set_mode,
	.get_mode = mt6691_get_mode,
	.enable = mt6691_enable,
	.disable = mt6691_disable,
	.is_enabled = mt6691_is_enabled,
};

#define REG_CHIP(_id, _vsel)	\
{						\
	.desc = {				\
		.id = _id,			\
		.name = "mt6691_buck"#_id,	\
		.n_voltages = 201,		\
		.ops = &mt6691_regulator_ops,	\
		.type = REGULATOR_VOLTAGE,	\
		.owner = THIS_MODULE,		\
	},					\
	.vol_reg = MT6691_REG_VSEL##_vsel,	\
	.vol_mask = 0xFF,			\
	.mode_reg = MT6691_CTRL_1,		\
	.mode_bit = (1 << _vsel),		\
	.enable_reg = MT6691_REG_CTRL2,		\
	.enable_bit = (1 << _vsel),		\
}

#define HL7593_REG_CHIP(_id, _vsel)	\
{						\
	.desc = {				\
		.id = _id,			\
		.name = "hl7593_buck"#_id,	\
		.n_voltages = 128,		\
		.ops = &hl7593_regulator_ops,	\
		.type = REGULATOR_VOLTAGE,	\
		.owner = THIS_MODULE,		\
	},					\
	.vol_reg = MT6691_REG_VSEL##_vsel,	\
	.vol_mask = 0x7F,			\
	.mode_reg = MT6691_CTRL_1,		\
	.mode_bit = (1 << _vsel),		\
	.enable_reg = MT6691_REG_VSEL##_vsel,	\
	.enable_bit = (1 << 7),			\
}

static struct regulator_chip mt6691_datas[] = {
	REG_CHIP(0, 0), /* VDD2 1.125V */
	REG_CHIP(1, 1), /* VDDQ 0.6V */
	REG_CHIP(2, 0), /* VMDDR 0.75V */
	REG_CHIP(3, 0), /* VUFS12 1.225V */
	HL7593_REG_CHIP(2, 0), /* VMDDR 0.75V */
	HL7593_REG_CHIP(3, 0), /* VUFS12 1.2V */
};

struct regulator_dev *mt6691_regulator_register(
					const struct regulator_desc *desc,
					struct device *dev,
					struct regulator_init_data *init_data,
					void *driver_data)
{
	struct regulator_config config = {
		.dev = dev,
		.init_data = init_data,
		.driver_data = driver_data,
		.of_node = dev->of_node,
	};
	return regulator_register(desc, &config);
}

static int mt6691_i2c_probe(struct i2c_client *i2c,
					const struct i2c_device_id *id)
{
	struct mt6691_regulator_info *info;
	struct regulator_init_data *init_data = NULL;
	int ret;

	switch (i2c->addr) {
	case 0x50: /* MT6691ZXP */
	case 0x57: /* MT6691OOP */
	case 0x51: /* MT6691SVP */
	case 0x56: /* MT6691OTP */
		break;
	default:
		dev_info(&i2c->dev, "%s invalid Slave Addr\n", __func__);
		return -ENODEV;
	}

	info = devm_kzalloc(&i2c->dev,
		sizeof(struct mt6691_regulator_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	init_data = of_get_regulator_init_data(&i2c->dev, i2c->dev.of_node,
					       NULL);
	if (init_data) {
		dev_info(&i2c->dev,
			 "regulator_name = %s, min_uV =%d, max_uV = %d\n",
			 init_data->constraints.name,
			 init_data->constraints.min_uV,
			 init_data->constraints.max_uV);
	} else {
		dev_info(&i2c->dev, "%s: no init data\n", __func__);
		return -EINVAL;
	}

	info->i2c = i2c;
	info->dev = &i2c->dev;
	info->reg_chip = of_device_get_match_data(&i2c->dev);
	mutex_init(&info->io_lock);

	i2c_set_clientdata(i2c, info);

#if 0
	g_is_mt6691_exist = 1;
	if (mt6691_read_byte(info->i2c, MT6691_REG_MONITOR, &ret) >= 0)
		g_is_mt6691_exist &= 1;
	else
		g_is_mt6691_exist &= 0;
	dev_info(&i2c->dev, "i2c_slv=0x%x ret=0x%x g_is_mt6691_exist=%d\n",
		 i2c->addr, ret, g_is_mt6691_exist);
	return 0;
#else
	info->regulator = mt6691_regulator_register(&info->reg_chip->desc,
						    &i2c->dev,
						    init_data,
						    info);

	if (IS_ERR(info->regulator)) {
		switch (i2c->addr) {
		case 0x51: /* MT6691SVP */
			i2c->addr = 0x61; /* HL7593WL0A */
			info->reg_chip = &mt6691_datas[4];
			break;
		case 0x56: /* MT6691OTP */
			i2c->addr = 0x60; /* HL7593WL07 */
			info->reg_chip = &mt6691_datas[5];
			break;
		default:
			pr_notice("mt6691 register regulator fail\n");
			return -EINVAL;
		}
		info->regulator =
			mt6691_regulator_register(&info->reg_chip->desc,
						  &i2c->dev,
						  init_data,
						  info);
		if (IS_ERR(info->regulator)) {
			pr_notice("mt6691 register regulator fail\n");
			return -EINVAL;
		}
	}
	g_is_mt6691_exist = 1;

	info->regulator->constraints->valid_modes_mask |=
			(REGULATOR_MODE_NORMAL|REGULATOR_MODE_FAST);
	info->regulator->constraints->valid_ops_mask |=
			REGULATOR_CHANGE_MODE;

	ret = mt6691_regmap_init(info);
	if (ret < 0) {
		dev_info(&i2c->dev, "%s mt6691 regmap init fail\n", __func__);
		return -EINVAL;
	}

	pr_info("%s Successfully\n", __func__);

	return 0;
#endif
}

static int mt6691_i2c_remove(struct i2c_client *i2c)
{
	struct mt6691_regulator_info *info = i2c_get_clientdata(i2c);

	if (info) {
		regulator_unregister(info->regulator);
#ifdef CONFIG_RT_REGMAP
		rt_regmap_device_unregister(info->regmap_dev);
#endif /* CONFIG_RT_REGMAP */
		mutex_destroy(&info->io_lock);
	}
	return 0;
}

/* Must reserve one empty object at the end */
static const struct of_device_id rt_match_table[] = {
	{	/* VDD2=1.125V (0x57) */
		.compatible = MT6691_CMPT_STR_0,
		.data = &mt6691_datas[0],
	}, {	/* VDDQ=1.125V (0x57) */
		.compatible = MT6691_CMPT_STR_1,
		.data = &mt6691_datas[1],
	}, {	/* VMDDR=0.75V (0x51) */
		.compatible = MT6691_CMPT_STR_2,
		.data = &mt6691_datas[2],
	}, {	/* VUFS12=1.225V (0x56) */
		.compatible = "mediatek,ext_buck_vufs12",
		.data = &mt6691_datas[3],
	}, {
		/* Sentinel */
	},
};

static const struct i2c_device_id rt_dev_id[] = {
	{"mt6691", 0},
	{ },
};

static struct i2c_driver mt6691_i2c_driver = {
	.driver = {
		.name	= "mt6691",
		.owner	= THIS_MODULE,
		.of_match_table	= rt_match_table,
	},
	.probe	= mt6691_i2c_probe,
	.remove	= mt6691_i2c_remove,
	.id_table = rt_dev_id,
};

static int __init mt6691_i2c_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&mt6691_i2c_driver);
}
subsys_initcall(mt6691_i2c_init);

static void __exit mt6691_i2c_exit(void)
{
	i2c_del_driver(&mt6691_i2c_driver);
}
module_exit(mt6691_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION(MT6691_DRV_VERSION);
MODULE_DESCRIPTION("Regulator driver for MT6691");
