/*
 * Copyright (C) 2017 MediaTek Inc.

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
#include <linux/of.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif /* CONFIG_RT_REGMAP */
#include <mt-plat/upmu_common.h>
#include "rt5738-regulator.h"

static const char * const rt5738_text[] = {
	"rt57380",
	"rt57381",
	"rt57382",
};

/*
 * Exported API
 */
static int g_is_rt5738_exist;

int is_rt5738_exist(void)
{
#if defined(RT5738_IS_EXIST_NAME)
	struct regulator *reg;

	reg = regulator_get(NULL, RT5738_IS_EXIST_NAME);
	pr_info("%s: regulator_get=%s\n", __func__, RT5738_IS_EXIST_NAME);
	if (reg == NULL)
		return 0;
	regulator_put(reg);
	return 1;
#else
	pr_notice("g_is_rt5738_exist=%d\n", g_is_rt5738_exist);
	return g_is_rt5738_exist;
#endif
}

static int rt5738_read_device(void *client, u32 addr,
					 int len, void *dst)
{
	struct i2c_client *i2c = (struct i2c_client *)client;

	return i2c_smbus_read_i2c_block_data(i2c, addr, len, dst);
}

static int rt5738_write_device(void *client, u32 addr,
					 int len, const void *src)
{
	struct i2c_client *i2c = (struct i2c_client *)client;

	return i2c_smbus_write_i2c_block_data(i2c, addr, len, src);
}

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(RT5738_REG_VSEL0, 1, RT_WBITS_WR_ONCE, {});
RT_REG_DECL(RT5738_REG_VSEL1, 1, RT_WBITS_WR_ONCE, {});
RT_REG_DECL(RT5738_REG_CTRL1, 1, RT_WBITS_WR_ONCE, {});
RT_REG_DECL(RT5738_REG_ID1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5738_REG_ID2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5738_REG_MONITOR, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5738_REG_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT5738_REG_CTRL3, 1, RT_WBITS_WR_ONCE, {});
RT_REG_DECL(RT5738_REG_CTRL4, 1, RT_WBITS_WR_ONCE, {});

static const rt_register_map_t rt5738_regmap[] = {
	RT_REG(RT5738_REG_VSEL0),
	RT_REG(RT5738_REG_VSEL1),
	RT_REG(RT5738_REG_CTRL1),
	RT_REG(RT5738_REG_ID1),
	RT_REG(RT5738_REG_ID2),
	RT_REG(RT5738_REG_MONITOR),
	RT_REG(RT5738_REG_CTRL2),
	RT_REG(RT5738_REG_CTRL3),
	RT_REG(RT5738_REG_CTRL4),
};

static struct rt_regmap_fops rt5738_regmap_fops = {
	.read_device = rt5738_read_device,
	.write_device = rt5738_write_device,
};
#endif /* CONFIG_RT_REGMAP */

int rt5738_read_byte(void *client,
			uint32_t addr, uint32_t *val)
{
	int ret = 0;
	struct i2c_client *i2c = (struct i2c_client *)client;

	ret = rt5738_read_device(i2c, addr, 1, val);
	if (ret < 0)
		pr_notice("%s read 0x%02x fail\n", __func__, addr);
	return ret;
}

int rt5738_write_byte(void *client,
			uint32_t addr, uint32_t value)
{
	int ret = 0;
	struct i2c_client *i2c = (struct i2c_client *)client;

	ret = rt5738_write_device(i2c, addr, 1, &value);
	if (ret < 0)
		pr_notice("%s write 0x%02x fail\n", __func__, addr);
	return ret;
}

int rt5738_assign_bit(void *client, uint32_t reg,
					uint32_t  mask, uint32_t data)
{
	struct i2c_client *i2c = (struct i2c_client *)client;
	struct rt5738_regulator_info *ri = i2c_get_clientdata(i2c);
	unsigned char tmp = 0;
	uint32_t regval = 0;
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = rt5738_read_byte(i2c, reg, &regval);
	if (ret < 0) {
		pr_notice("%s read fail reg0x%02x data0x%02x\n",
				__func__, reg, data);
		goto OUT_ASSIGN;
	}

	tmp = ((regval & 0xff) & ~mask);
	tmp |= (data & mask);
	ret = rt5738_write_byte(i2c, reg, tmp);
	if (ret < 0)
		pr_notice("%s write fail reg0x%02x data0x%02x\n",
				__func__, reg, tmp);
OUT_ASSIGN:
	mutex_unlock(&ri->io_lock);
	return  ret;
}

#define rt5738_set_bit(i2c, reg, mask) \
	rt5738_assign_bit(i2c, reg, mask, mask)
#define rt5738_clr_bit(i2c, reg, mask) \
	rt5738_assign_bit(i2c, reg, mask, 0x00)

static int rt5738_regmap_init(struct rt5738_regulator_info *info)
{
#ifdef CONFIG_RT_REGMAP
	info->regmap_props = devm_kzalloc(info->dev,
		sizeof(struct rt_regmap_properties), GFP_KERNEL);

	info->regmap_props->name = rt5738_text[info->id];
	info->regmap_props->aliases = rt5738_text[info->id];
	info->regmap_props->register_num = ARRAY_SIZE(rt5738_regmap);
	info->regmap_props->rm = rt5738_regmap;
	info->regmap_props->rt_regmap_mode = RT_CACHE_WR_THROUGH;

	info->regmap_dev = rt_regmap_device_register(info->regmap_props,
		&rt5738_regmap_fops, info->dev, info->i2c, info);
	if (!info->regmap_dev)
		return -EINVAL;
#endif /* CONFIG_RT_REGMAP */
	return 0;
}

static struct regulator_chip rt5738_datas[] = {
	{
		.vol_reg = RT5738_VSEL_0,
		.mode_reg = RT5738_CTRL_0,
		.mode_bit = RT5738_CTRL_BIT_0,
		.enable_reg = RT5738_EN_0,
		.enable_bit = RT5738_EN_BIT_0,
	},
	{
		.vol_reg = RT5738_VSEL_1,
		.mode_reg = RT5738_CTRL_1,
		.mode_bit = RT5738_CTRL_BIT_1,
		.enable_reg = RT5738_EN_1,
		.enable_bit = RT5738_EN_BIT_1,
	},
#if defined(RT5738_NAME_2)
	{
		.vol_reg = RT5738_VSEL_2,
		.mode_reg = RT5738_CTRL_2,
		.mode_bit = RT5738_CTRL_BIT_2,
		.enable_reg = RT5738_EN_2,
		.enable_bit = RT5738_EN_BIT_2,
	},
#endif
};

static int rt5738_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned int *selector)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct regulator_chip *chip = info->reg_chip;

	if (min_uV < 1300000)
		*selector = (min_uV - 300000) / 5000;
	else
		*selector = (min_uV - 1300000) / 10000 + 200;

	if (*selector > 255)
		*selector = 255;
	return	rt5738_write_byte(info->i2c, chip->vol_reg, *selector);
}

static int rt5738_get_voltage(struct regulator_dev *rdev)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct regulator_chip *chip = info->reg_chip;
	int ret;
	uint32_t reg_val = 0;

	ret = rt5738_read_byte(info->i2c, chip->vol_reg, &reg_val);
	if (ret < 0) {
		pr_notice("%s read voltage fail\n", __func__);
		return ret;
	}

	if (reg_val > 200)
		return 1300000 + (reg_val - 200) * 10000;
	return 300000 + reg_val * 5000;
}

static int rt5738_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct regulator_chip *chip = info->reg_chip;
	int ret;

	switch (mode) {
	case REGULATOR_MODE_FAST: /* force pwm mode */
		ret = rt5738_set_bit(info->i2c, chip->mode_reg, chip->mode_bit);
		break;
	case REGULATOR_MODE_NORMAL:
	default:
		ret = rt5738_clr_bit(info->i2c, chip->mode_reg, chip->mode_bit);
		break;
	}

	return ret;
}

static unsigned int rt5738_get_mode(struct regulator_dev *rdev)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct regulator_chip *chip = info->reg_chip;
	int ret;
	uint32_t regval = 0;

	ret = rt5738_read_byte(info->i2c, chip->mode_reg, &regval);
	if (ret < 0) {
		pr_notice("%s read mode fail\n", __func__);
		return ret;
	}

	if (regval & chip->mode_bit)
		return REGULATOR_MODE_FAST;
	return REGULATOR_MODE_NORMAL;
}

static int rt5738_enable(struct regulator_dev *rdev)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct regulator_chip *chip = info->reg_chip;

	return rt5738_set_bit(info->i2c, chip->enable_reg, chip->enable_bit);
}

static int rt5738_disable(struct regulator_dev *rdev)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct regulator_chip *chip = info->reg_chip;

	if (rdev->use_count == 0) {
		pr_info("ext_buck should not be disable (use_count=%d)\n"
			, rdev->use_count);
		return -1;
	}
	return rt5738_clr_bit(info->i2c, chip->enable_reg, chip->enable_bit);
}

static int rt5738_is_enabled(struct regulator_dev *rdev)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct regulator_chip *chip = info->reg_chip;
	int ret;
	uint32_t reg_val;

	ret = rt5738_read_byte(info->i2c, chip->enable_reg, &reg_val);
	if (ret < 0)
		return ret;

	return (reg_val&chip->enable_bit) ? 1 : 0;
}

static struct regulator_ops rt5738_regulator_ops = {
	.set_voltage = rt5738_set_voltage,
	.get_voltage = rt5738_get_voltage,
	.set_mode = rt5738_set_mode,
	.get_mode = rt5738_get_mode,
	.enable = rt5738_enable,
	.disable = rt5738_disable,
	.is_enabled = rt5738_is_enabled,
};

#define REG_DESC(_name, _id) {		\
	.id = _id,			\
	.name = _name,			\
	.n_voltages = 201,		\
	.ops = &rt5738_regulator_ops,	\
	.type = REGULATOR_VOLTAGE,	\
	.owner = THIS_MODULE,		\
}

static struct regulator_desc rt5738_regulator_desc[] = {
	REG_DESC("rt5738_buck0", 0),
	REG_DESC("rt5738_buck1", 1),
#if defined(RT5738_NAME_2)
	REG_DESC("rt5738_buck2", 2),
#endif
};

static int rt5738_parse_dt(
	struct rt5738_regulator_info *info, struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val;
	int ret;

	if (!np) {
		pr_notice("%s cant find node (0x%02x)\n",
				__func__, info->i2c->addr);
		return 0;
	}

	ret = of_property_read_u32(np, "vsel_pin", &val);
	if (ret >= 0) {
		pr_info("%s set vsel_pin(%x)\n", __func__, val);
		info->pin_sel = val;
		info->id = val;
	} else {
		pr_notice("%s use chip default vsel_pin(0)\n", __func__);
		info->pin_sel = 0;
		info->id = 0;
	}

	return ret;
}

struct regulator_dev *rt5738_regulator_register(
					struct regulator_desc *desc,
					struct device *dev,
					struct regulator_init_data *init_data,
					void *driver_data)
{
	struct regulator_config config = {
		.dev = dev,
		.init_data = init_data,
		.driver_data = driver_data,
	};
	return regulator_register(desc, &config);
}

static int rt5738_i2c_probe(struct i2c_client *i2c,
					const struct i2c_device_id *id)
{
	struct rt5738_regulator_info *info;
	struct regulator_init_data *init_data = NULL;
	int ret;

	pr_info("%s ver(%s) slv(0x%02x)\n",
		__func__, RT5738_DRV_VERSION, i2c->addr);

	switch (i2c->addr) {
	case 0x50: /* RT5738_A */
	case 0x57: /* RT5738_B */
		return -ENODEV;
	case 0x52: /* RT5738_C */
	case 0x51: /* RT5738_G */
	case 0x53: /* RT5738_H */
		break;
	case 0x55: /* RT5738_F */
		return -ENODEV;
	default:
		pr_notice("%s invalid Slave Addr\n", __func__);
		return -ENODEV;
	}

	info = devm_kzalloc(&i2c->dev,
		sizeof(struct rt5738_regulator_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	ret = rt5738_parse_dt(info, &i2c->dev);
	if (ret < 0) {
		pr_notice("%s parse dt (0x%02x) fail\n", __func__, i2c->addr);
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(&i2c->dev,
					       i2c->dev.of_node,
					       NULL);
	if (init_data) {
		dev_info(&i2c->dev,
			 "regulator_name = %s, min_uV =%d, max_uV = %d\n"
			 , init_data->constraints.name
			 , init_data->constraints.min_uV
			 , init_data->constraints.max_uV);
		pr_info("rt5738 regulator_name = %s, min_uV =%d, max_uV = %d\n"
			 , init_data->constraints.name
			 , init_data->constraints.min_uV
			 , init_data->constraints.max_uV);
	} else {
		dev_info(&i2c->dev, "%s: no init data\n", __func__);
		return -EINVAL;
	}

	info->i2c = i2c;
	info->dev = &i2c->dev;
	info->desc = &rt5738_regulator_desc[info->pin_sel];
	info->reg_chip = &rt5738_datas[info->pin_sel];
	mutex_init(&info->io_lock);

	i2c_set_clientdata(i2c, info);

	ret = rt5738_regmap_init(info);
	if (ret < 0) {
		pr_notice("%s rt5738 regmap init fail\n", __func__);
		return -EINVAL;
	}

#if 1
	g_is_rt5738_exist = 0;
	if (rt5738_read_byte(info->i2c, RT5738_REG_MONITOR, &ret) >= 0)
		g_is_rt5738_exist = 1;
	pr_notice("i2c_addr=%d ret=%d g_is_rt5738_exist=%d\n"
				, i2c->addr
				, ret
				, g_is_rt5738_exist);
	return 0;
#else
	info->regulator = rt5738_regulator_register(info->desc,
						    &i2c->dev,
						    init_data,
						    info);

	if (!info->regulator) {
		pr_notice("%s rt5738 register regulator fail\n", __func__);
		return -EINVAL;
	}

	info->regulator->constraints->valid_modes_mask |=
			(REGULATOR_MODE_NORMAL|REGULATOR_MODE_FAST);
	info->regulator->constraints->valid_ops_mask |=
			REGULATOR_CHANGE_MODE;

	pr_info("%s Successfully\n", __func__);

	return 0;
#endif
}

static int rt5738_i2c_remove(struct i2c_client *i2c)
{
	struct rt5738_regulator_info *info = i2c_get_clientdata(i2c);

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
	{ .compatible = RT5738_CMPT_STR_0, },
	{ .compatible = RT5738_CMPT_STR_1, },
#if defined(RT5738_NAME_2)
	{ .compatible = RT5738_CMPT_STR_2, },
#endif
	{ },
};

static const struct i2c_device_id rt_dev_id[] = {
	{"rt5738", 0},
	{ },
};

static struct i2c_driver rt5738_i2c_driver = {
	.driver = {
		.name	= "rt5738",
		.owner	= THIS_MODULE,
		.of_match_table	= rt_match_table,
	},
	.probe	= rt5738_i2c_probe,
	.remove	= rt5738_i2c_remove,
	.id_table = rt_dev_id,
};

static int __init rt5738_i2c_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&rt5738_i2c_driver);
}
subsys_initcall(rt5738_i2c_init);

static void __exit rt5738_i2c_exit(void)
{
	i2c_del_driver(&rt5738_i2c_driver);
}
module_exit(rt5738_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION(RT5738_DRV_VERSION);
MODULE_DESCRIPTION("Regulator driver for RT5738");
