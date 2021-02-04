/*
 *  drivers/misc/mediatek/pmic/mt6360/mt6360_pmu_i2c.c
 *  Driver for MT6360 PMU part
 *
 *  Copyright (C) 2018 Mediatek Technology Inc.
 *  cy_huang <cy_huang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>

#include "../inc/mt6360_pmu.h"

bool dbg_log_en; /* module param to enable/disable debug log */
module_param(dbg_log_en, bool, 0644);

static const struct mt6360_pmu_platform_data def_platform_data = {
	.irq_gpio = -1,
};

static int mt6360_pmu_read_device(void *client, u32 addr, int len, void *dst)
{
	return i2c_smbus_read_i2c_block_data(client, addr, len, dst);
}

static int mt6360_pmu_write_device(void *client, u32 addr,
				   int len, const void *src)
{
	return i2c_smbus_write_i2c_block_data(client, addr, len, src);
}

static struct rt_regmap_fops mt6360_pmu_regmap_fops = {
	.read_device = mt6360_pmu_read_device,
	.write_device = mt6360_pmu_write_device,
};

int mt6360_pmu_reg_read(struct mt6360_pmu_info *mpi, u8 addr)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};
	int ret;

	mt_dbg(mpi->dev, "%s: reg[%02x]\n", __func__, addr);
	mutex_lock(&mpi->io_lock);
	ret = rt_regmap_reg_read(mpi->regmap, &rrd, addr);
	mutex_unlock(&mpi->io_lock);
	return (ret < 0 ? ret : rrd.rt_data.data_u8);
#else
	u8 data = 0;
	int ret;

	mt_dbg(mpi->dev, "%s: reg[%02x]\n", __func__, addr);
	mutex_lock(&mpi->io_lock);
	ret = mt6360_pmu_read_device(mpi->i2c, addr, 1, &data);
	mutex_unlock(&mpi->io_lock);
	return (ret < 0 ? ret : data);
#endif /* CONFIG_RT_REGMAP */
}
EXPORT_SYMBOL_GPL(mt6360_pmu_reg_read);

int mt6360_pmu_reg_write(struct mt6360_pmu_info *mpi, u8 addr, u8 data)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};
	int ret;

	mt_dbg(mpi->dev, "%s reg[%02x] data [%02x]\n", __func__, addr, data);
	mutex_lock(&mpi->io_lock);
	ret = rt_regmap_reg_write(mpi->regmap, &rrd, addr, data);
	if (ret < 0)
		rt_regmap_cache_reload(mpi->regmap);
	mutex_unlock(&mpi->io_lock);
	return ret;
#else
	int ret;

	mt_dbg(mpi->dev, "%s reg[%02x] data [%02x]\n", __func__, addr, data);
	mutex_lock(&mpi->io_lock);
	ret = mt6360_pmu_write_device(mpi->i2c, addr, 1, &data);
	mutex_unlock(&mpi->io_lock);
	return ret;
#endif /* CONFIG_RT_REGMAP */
}
EXPORT_SYMBOL_GPL(mt6360_pmu_reg_write);

int mt6360_pmu_reg_update_bits(struct mt6360_pmu_info *mpi,
			       u8 addr, u8 mask, u8 data)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};
	int ret;

	mt_dbg(mpi->dev,
		"%s reg[%02x], mask[%02x], data[%02x]\n",
		__func__, addr, mask, data);
	mutex_lock(&mpi->io_lock);
	ret = rt_regmap_update_bits(mpi->regmap, &rrd, addr, mask, data);
	mutex_unlock(&mpi->io_lock);
	return ret;
#else
	u8 org = 0;
	int ret;

	mt_dbg(mpi->dev,
		"%s reg[%02x], mask[%02x], data[%02x]\n",
		__func__, addr, mask, data);
	mutex_lock(&mpi->io_lock);
	ret = mt6360_pmu_read_device(mpi->i2c, addr, 1, &org);
	if (ret < 0)
		goto out_update_bits;
	org &= ~mask;
	org |= (data & mask);
	ret = mt6360_pmu_write_device(mpi->i2c, addr, 1, &org);
out_update_bits:
	mutex_unlock(&mpi->io_lock);
	return ret;
#endif /* CONFIG_RT_REGMAP */
}
EXPORT_SYMBOL_GPL(mt6360_pmu_reg_update_bits);

int mt6360_pmu_reg_block_read(struct mt6360_pmu_info *mpi,
			      u8 addr, u8 len, u8 *dst)
{
#ifdef CONFIG_RT_REGMAP
	int ret;

	mt_dbg(mpi->dev, "%s addr[%02x], len[%d]\n", __func__, addr, len);
	mutex_lock(&mpi->io_lock);
	ret = rt_regmap_block_read(mpi->regmap, addr, len, dst);
	mutex_unlock(&mpi->io_lock);
	return ret;
#else
	int ret;

	mt_dbg(mpi->dev, "%s addr[%02x], len[%d]\n", __func__, addr, len);
	mutex_lock(&mpi->io_lock);
	ret = mt6360_pmu_read_device(mpi->i2c, addr, len, dst);
	mutex_unlock(&mpi->io_lock);
	return ret;
#endif /* CONFIG_RT_REGMAP */
}
EXPORT_SYMBOL_GPL(mt6360_pmu_reg_block_read);

int mt6360_pmu_reg_block_write(struct mt6360_pmu_info *mpi,
			       u8 addr, u8 len, const u8 *src)
{
#ifdef CONFIG_RT_REGMAP
	int ret = 0;

	mt_dbg(mpi->dev, "%s addr[%02x], len[%d]\n", __func__, addr, len);
	mutex_lock(&mpi->io_lock);
	ret = rt_regmap_block_write(mpi->regmap, addr, len, src);
	mutex_unlock(&mpi->io_lock);
	return ret;
#else
	int ret = 0;

	mt_dbg(mpi->dev, "%s addr[%02x], len[%d]\n", __func__, addr, len);
	mutex_lock(&mpi->io_lock);
	ret = mt6360_pmu_write_device(mpi->i2c, addr, len, src);
	mutex_unlock(&mpi->io_lock);
	return ret;
#endif /* CONFIG_RT_REGMAP */
}
EXPORT_SYMBOL_GPL(mt6360_pmu_reg_block_write);

static const struct mt6360_pdata_prop mt6360_pdata_props[] = {
	MT6360_PDATA_VALPROP(int_ret, struct mt6360_pmu_platform_data,
			     MT6360_PMU_IRQ_SET, 0, 0x03, NULL, 0),
};

static int mt6360_pmu_apply_pdata(struct mt6360_pmu_info *mpi,
				  struct mt6360_pmu_platform_data *pdata)
{
	int ret;

	dev_dbg(mpi->dev, "%s ++\n", __func__);
	ret = mt6360_pdata_apply_helper(mpi, pdata, mt6360_pdata_props,
					ARRAY_SIZE(mt6360_pdata_props));
	if (ret < 0)
		return ret;
	dev_dbg(mpi->dev, "%s --\n", __func__);
	return 0;
}

static const struct mt6360_val_prop mt6360_val_props[] = {
	MT6360_DT_VALPROP(int_ret, struct mt6360_pmu_platform_data),
};

static int mt6360_pmu_parse_dt_data(struct device *dev,
				    struct mt6360_pmu_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int ret;

	dev_info(dev, "%s ++\n", __func__);
	memcpy(pdata, &def_platform_data, sizeof(*pdata));
	mt6360_dt_parser_helper(np, (void *)pdata,
				mt6360_val_props, ARRAY_SIZE(mt6360_val_props));
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(np, "mt6360,intr_gpio", 0);
	if (ret < 0) {
		dev_notice(dev, "%s of get named gpio fail\n", __func__);
		goto out_parse_dt;
	}
	pdata->irq_gpio = ret;
#else
	ret = of_property_read_u32(np, "mt6360,intr_gpio_num",
				   &pdata->irq_gpio);
	if (ret < 0) {
		dev_notice(dev, "%s of gpio num fail\n", __func__);
		goto out_parse_dt;
	}
#endif /* (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND)) */
out_parse_dt:
	dev_info(dev, "%s --, irq gpio%d\n", __func__, pdata->irq_gpio);
	return 0;
}

static inline int mt6360_pmu_chip_id_check(struct i2c_client *i2c)
{
	int ret;

	ret = i2c_smbus_read_byte_data(i2c, MT6360_PMU_DEV_INFO);
	if (ret < 0)
		return ret;
	if ((ret & 0xf0) != 0x50)
		return -ENODEV;
	return (ret & 0x0f);
}

static inline void mt6360_config_of_node(struct device *dev, const char *name)
{
	struct device_node *np = NULL;

	if (unlikely(!dev) || unlikely(!name))
		return;
	np = of_find_node_by_name(NULL, name);
	if (np) {
		dev_info(dev, "find %s node\n", name);
		dev->of_node = np;
	}
}

static int mt6360_pmu_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct mt6360_pmu_platform_data *pdata = dev_get_platdata(&client->dev);
	struct mt6360_pmu_info *mpi;
	bool use_dt = client->dev.of_node;
	u8 chip_rev;
	int ret;

	dev_dbg(&client->dev, "%s\n", __func__);
	ret = mt6360_pmu_chip_id_check(client);
	if (ret < 0) {
		dev_err(&client->dev, "no device found\n");
		return ret;
	}
	chip_rev = (u8)ret;
	if (use_dt) {
		mt6360_config_of_node(&client->dev, "mt6360_pmu_dts");
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		ret = mt6360_pmu_parse_dt_data(&client->dev, pdata);
		if (ret < 0) {
			dev_err(&client->dev, "parse dt fail\n");
			return ret;
		}
		client->dev.platform_data = pdata;
	}
	if (!pdata) {
		dev_err(&client->dev, "no platform data specified\n");
		return -EINVAL;
	}
	mpi = devm_kzalloc(&client->dev, sizeof(*mpi), GFP_KERNEL);
	if (!mpi)
		return -ENOMEM;
	mpi->i2c = client;
	mpi->dev = &client->dev;
	mpi->irq = -1;
	mpi->chip_rev = chip_rev;
	mutex_init(&mpi->io_lock);
	i2c_set_clientdata(client, mpi);
	dev_info(&client->dev, "chip_rev [%02x]\n", mpi->chip_rev);

	pm_runtime_set_active(mpi->dev);
	/* regmap regiser */
	ret = mt6360_pmu_regmap_register(mpi, &mt6360_pmu_regmap_fops);
	if (ret < 0) {
		dev_err(&client->dev, "regmap register fail\n");
		goto out_regmap;
	}
	/* after regmap register, apply platform data */
	ret = mt6360_pmu_apply_pdata(mpi, pdata);
	if (ret < 0) {
		dev_err(&client->dev, "apply pdata fail\n");
		goto out_irq;
	}
	/* irq register */
	ret = mt6360_pmu_irq_register(mpi);
	if (ret < 0) {
		dev_err(&client->dev, "irq register fail\n");
		goto out_irq;
	}
	/* subdev register */
	ret = mt6360_pmu_subdev_register(mpi);
	if (ret < 0) {
		dev_err(&client->dev, "subdev register fail\n");
		goto out_irq;
	}
	pm_runtime_enable(mpi->dev);
	dev_info(&client->dev, "%s: successfully probed\n", __func__);
	return 0;
out_irq:
	mt6360_pmu_regmap_unregister(mpi);
out_regmap:
	pm_runtime_set_suspended(mpi->dev);
	return ret;
}

static int mt6360_pmu_i2c_remove(struct i2c_client *client)
{
	struct mt6360_pmu_info *mpi = i2c_get_clientdata(client);

	dev_dbg(mpi->dev, "%s\n", __func__);
	pm_runtime_disable(mpi->dev);
	/* To-do: subdev unregister */
	mt6360_pmu_irq_unregister(mpi);
	mt6360_pmu_regmap_unregister(mpi);
	pm_runtime_set_suspended(mpi->dev);
	mutex_destroy(&mpi->io_lock);
	return 0;
}

static int __maybe_unused mt6360_pmu_i2c_suspend(struct device *dev)
{
	struct mt6360_pmu_info *mpi = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);
	return mt6360_pmu_irq_suspend(mpi);
}

static int __maybe_unused mt6360_pmu_i2c_resume(struct device *dev)
{
	struct mt6360_pmu_info *mpi = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);
	return mt6360_pmu_irq_resume(mpi);
}

static SIMPLE_DEV_PM_OPS(mt6360_pmu_pm_ops,
			 mt6360_pmu_i2c_suspend, mt6360_pmu_i2c_resume);

static const struct of_device_id __maybe_unused mt6360_pmu_of_id[] = {
	{ .compatible = "mediatek,mt6360_pmu", },
	{ .compatible = "mediatek,subpmic", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6360_pmu_of_id);

static const struct i2c_device_id mt6360_pmu_i2c_id[] = {
	{ "mt6360_pmu", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, mt6360_pmu_i2c_id);

static struct i2c_driver mt6360_pmu_i2c_driver = {
	.driver = {
		.name = "mt6360_pmu",
		.owner = THIS_MODULE,
		.pm = &mt6360_pmu_pm_ops,
		.of_match_table = of_match_ptr(mt6360_pmu_of_id),
	},
	.probe = mt6360_pmu_i2c_probe,
	.remove = mt6360_pmu_i2c_remove,
	.id_table = mt6360_pmu_i2c_id,
};
module_i2c_driver(mt6360_pmu_i2c_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 PMU I2C Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
