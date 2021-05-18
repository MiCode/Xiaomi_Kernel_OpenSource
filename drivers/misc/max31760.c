// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/rwlock.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>

struct max31760 {
	struct device *dev;
	u8 i2c_addr;
	struct regmap *regmap;
	u32 fan_pwr_en;
	u32 fan_pwr_bp;
	struct i2c_client *i2c_client;
	int pwm;
	bool fan_off;
};

static void turn_gpio(struct max31760 *pdata, bool on)
{
	if (on) {
		gpio_direction_output(pdata->fan_pwr_en, 0);
		gpio_set_value(pdata->fan_pwr_en, 1);
		pr_debug("%s gpio:%d set to high\n", __func__,
					pdata->fan_pwr_en);
		msleep(20);
		gpio_direction_output(pdata->fan_pwr_bp, 0);
		gpio_set_value(pdata->fan_pwr_bp, 1);
		pr_debug("%s gpio:%d set to high\n", __func__,
					pdata->fan_pwr_bp);
		msleep(20);
	} else {
		gpio_direction_output(pdata->fan_pwr_en, 1);
		gpio_set_value(pdata->fan_pwr_en, 0);
		pr_debug("%s gpio:%d set to low\n", __func__,
					pdata->fan_pwr_en);
		msleep(20);
		gpio_direction_output(pdata->fan_pwr_bp, 1);
		gpio_set_value(pdata->fan_pwr_bp, 0);
		pr_debug("%s gpio:%d set to low\n", __func__,
					pdata->fan_pwr_bp);
		msleep(20);
	}
}

static int max31760_i2c_reg_get(struct max31760 *pdata,
				u8 reg)
{
	int ret;
	u32 val1;

	pr_debug("%s, reg:%x\n", __func__, reg);
	ret = regmap_read(pdata->regmap, (unsigned int)reg, &val1);
	if (ret < 0) {
		pr_err("%s failed reading reg 0x%02x failure\n", __func__, reg);
		return ret;
	}

	pr_debug("%s success reading reg 0x%x=0x%x, val1=%x\n",
					 __func__, reg, val1, val1);

	return 0;
}

static int max31760_i2c_reg_set(struct max31760 *pdata,
					u8 reg, u8 val)
{
	int ret;
	int i;

	for (i = 0; i < 10; i++) {
		ret = regmap_write(pdata->regmap, reg, val);
		if (ret >= 0)
			return ret;
		msleep(20);
	}
	if (ret < 0)
		pr_err("%s loop:%d failed to write reg 0x%02x=0x%02x\n",
			 __func__, i, reg, val);
	return ret;
}

static ssize_t fan_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct max31760 *pdata;
	int ret;

	pdata =  dev_get_drvdata(dev);
	if (!pdata) {
		pr_err("invalid driver pointer\n");
		return -ENODEV;
	}

	if (pdata->fan_off)
		ret = scnprintf(buf, PAGE_SIZE, "off\n");
	else
		ret = scnprintf(buf, PAGE_SIZE, "0x%x\n", pdata->pwm);

	return ret;
}

static ssize_t fan_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	long val, val1;
	struct max31760 *pdata;

	pdata =  dev_get_drvdata(dev);
	if (!pdata) {
		pr_err("invalid driver pointer\n");
		return -ENODEV;
	}

	kstrtol(buf, 0, &val);
	val1 = val >> 8;
	pr_debug("%s, count:%d  val:%lx, val1:%lx, buf:%s\n",
				 __func__, count, val, val1, buf);
	if (val1 == 0x50) {
		val1 = val & 0xFF;
		pr_debug("%s, reg value val1:%lx\n", __func__, val1);
		max31760_i2c_reg_set(pdata, 0x50, val1);
		return count;
	}

	if (val == 0xff) {
		turn_gpio(pdata, false);
		pdata->fan_off = true;
	} else if (val == 0xfe) {
		pdata->fan_off = false;
		turn_gpio(pdata, true);
		max31760_i2c_reg_set(pdata, 0x00, pdata->pwm);
	} else {
		max31760_i2c_reg_set(pdata, 0x00, (int)val);
		pdata->pwm = (int)val;
	}

	return count;
}

static DEVICE_ATTR_RW(fan);

static struct attribute *max31760_fs_attrs[] = {
	&dev_attr_fan.attr,
	NULL
};

static struct attribute_group max31760_fs_attr_group = {
	.attrs = max31760_fs_attrs,
};

static int max31760_parse_dt(struct device *dev,
				struct max31760 *pdata)
{
	struct device_node *np = dev->of_node;
	int ret;

	pdata->fan_pwr_en =
		of_get_named_gpio(np, "qcom,fan-pwr-en", 0);
	if (!gpio_is_valid(pdata->fan_pwr_en)) {
		pr_err("%s fan_pwr_en gpio not specified\n", __func__);
		ret = -EINVAL;
	} else {
		ret = gpio_request(pdata->fan_pwr_en, "fan_pwr_en");
		if (ret) {
			pr_err("max31760 fan_pwr_en gpio request failed\n");
			goto error1;
		}
	}

	pdata->fan_pwr_bp =
		of_get_named_gpio(np, "qcom,fan-pwr-bp", 0);
	if (!gpio_is_valid(pdata->fan_pwr_bp)) {
		pr_err("%s fan_pwr_bp gpio not specified\n", __func__);
		ret = -EINVAL;
	} else
		ret = gpio_request(pdata->fan_pwr_bp, "fan_pwr_bp");
		if (ret) {
			pr_err("max31760 fan_pwr_bp gpio request failed\n");
			goto error2;
	}
	turn_gpio(pdata, true);

	return ret;

error2:
	gpio_free(pdata->fan_pwr_bp);
error1:
	gpio_free(pdata->fan_pwr_en);
	return ret;
}

static int max31760_fan_pwr_enable_vregs(struct device *dev,
				 struct max31760 *pdata)
{
	int ret;
	struct regulator *reg;

	/* Fan Control LDO L10A */
	reg = devm_regulator_get(dev, "pm8150_l10");
	if (!IS_ERR(reg)) {
		regulator_set_load(reg, 600000);
		ret = regulator_enable(reg);
		if (ret < 0) {
			pr_err("%s pm8150_l10 failed\n", __func__);
			return -EINVAL;
		}
	}

	/* Fan Control LDO S4 */
	reg = devm_regulator_get(dev, "pm8150_s4");
	if (!IS_ERR(reg)) {
		regulator_set_load(reg, 600000);
		ret = regulator_enable(reg);
		if (ret < 0) {
			pr_err("%s pm8150_s4 failed\n", __func__);
			return -EINVAL;
		}
	}

	return ret;
}

static const struct regmap_config max31760_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF,
};

static int max31760_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret;
	struct max31760 *pdata;

	if (!client || !client->dev.of_node) {
		pr_err("%s invalid input\n", __func__);
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s device doesn't support I2C\n", __func__);
		return -ENODEV;
	}

	pdata = devm_kzalloc(&client->dev,
		sizeof(struct max31760), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->regmap = devm_regmap_init_i2c(client, &max31760_regmap);
	if (IS_ERR(pdata->regmap)) {
		ret = PTR_ERR(pdata->regmap);
		pr_err("%s Failed to allocate regmap: %d\n", __func__, ret);
		return -EINVAL;
	}

	ret = max31760_parse_dt(&client->dev, pdata);
	if (ret) {
		pr_err("%s failed to parse device tree\n", __func__);
		return -EINVAL;
	}

	ret = max31760_fan_pwr_enable_vregs(&client->dev, pdata);
	if (ret) {
		pr_err("%s failed to pwr regulators\n", __func__);
		return -EINVAL;
	}

	pdata->dev = &client->dev;
	i2c_set_clientdata(client, pdata);

	pdata->i2c_client = client;

	dev_set_drvdata(&client->dev, pdata);

	ret = sysfs_create_group(&pdata->dev->kobj, &max31760_fs_attr_group);
	if (ret)
		pr_err("%s unable to register max31760 sysfs nodes\n");

	/* 00 - 0x01 -- 33Hz */
	/* 01 - 0x09 -- 150Hz */
	/* 10 - 0x11 -- 1500Hz */
	/* 11 - 0x19 -- 25Khz */
	pdata->pwm = 0x19;
	max31760_i2c_reg_set(pdata, 0x00, pdata->pwm);
	max31760_i2c_reg_set(pdata, 0x01, 0x11);
	max31760_i2c_reg_set(pdata, 0x02, 0x31);
	max31760_i2c_reg_set(pdata, 0x03, 0x45);
	max31760_i2c_reg_set(pdata, 0x04, 0xff);
	max31760_i2c_reg_set(pdata, 0x50, 0xcf);
	max31760_i2c_reg_set(pdata, 0x01, 0x11);
	max31760_i2c_reg_set(pdata, 0x00, pdata->pwm);
	max31760_i2c_reg_get(pdata, 0x00);

	return ret;
}

static int max31760_remove(struct i2c_client *client)
{
	struct max31760 *pdata = i2c_get_clientdata(client);

	if (!pdata)
		goto end;

	sysfs_remove_group(&pdata->dev->kobj, &max31760_fs_attr_group);
	turn_gpio(pdata, false);
end:
	return 0;
}


static void max31760_shutdown(struct i2c_client *client)
{
}

static int max31760_suspend(struct device *dev, pm_message_t state)
{
	struct max31760 *pdata =  dev_get_drvdata(dev);

	dev_dbg(dev, "suspend\n");
	if (pdata)
		turn_gpio(pdata, false);
	return 0;
}

static int max31760_resume(struct device *dev)
{
	struct max31760 *pdata =  dev_get_drvdata(dev);

	dev_dbg(dev, "resume\n");
	if (pdata) {
		turn_gpio(pdata, true);
		max31760_i2c_reg_set(pdata, 0x00, pdata->pwm);
	}
	return 0;
}

static const struct of_device_id max31760_id_table[] = {
	{ .compatible = "maxim,xrfancontroller",},
	{ },
};
static const struct i2c_device_id max31760_i2c_table[] = {
	{ "xrfancontroller", 0 },
	{ },
};

static struct i2c_driver max31760_i2c_driver = {
	.probe = max31760_probe,
	.remove = max31760_remove,
	.shutdown = max31760_shutdown,
	.driver = {
		.name = "maxim xrfancontroller",
		.of_match_table = max31760_id_table,
		.suspend = max31760_suspend,
		.resume = max31760_resume,
	},
	.id_table = max31760_i2c_table,
};
module_i2c_driver(max31760_i2c_driver);
MODULE_DEVICE_TABLE(i2c, max31760_i2c_table);
MODULE_DESCRIPTION("Maxim 31760 Fan Controller");
MODULE_LICENSE("GPL v2");
