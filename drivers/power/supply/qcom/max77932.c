/*
 * max77932 charger pump watch dog driver
 *
 * Copyright (C) 2017 Texas Instruments Incorporated - http://www.ti.com/
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"[max77932] %s: " fmt, __func__
#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/random.h>
#include <linux/ktime.h>

enum print_reason {
	PR_INTERRUPT    = BIT(0),
	PR_REGISTER     = BIT(1),
	PR_OEM		= BIT(2),
	PR_DEBUG	= BIT(3),
};

static int debug_mask = PR_OEM;

module_param_named(
		debug_mask, debug_mask, int, 0600
		);

#define max_dbg(reason, fmt, ...)			\
	do {						\
		if (debug_mask & (reason))		\
		pr_info(fmt, ##__VA_ARGS__);	\
		else					\
		pr_debug(fmt, ##__VA_ARGS__);	\
	} while (0)

enum max77932_device {
	MAX77932,
};

struct max77932_chip {
	struct device *dev;
	struct i2c_client *client;
	struct regmap    *regmap;
	bool	chip_ok;
};

static int max_parse_dt(struct max77932_chip *max)
{
	//struct device_node *node = max->dev->of_node;

	return 0;
}

static struct regmap_config i2c_max77932_regmap_config = {
	.reg_bits  = 8,
	.val_bits  = 8,
	.max_register  = 0xFF,
};

#define MAX77932_CHIP_ID_REG		0x16
#define MAX77932_CHIP_ID		0x60

static ssize_t max_attr_show_chip_ok(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max77932_chip *max = i2c_get_clientdata(client);
	int len, rc, val;

	rc = regmap_read(max->regmap, MAX77932_CHIP_ID_REG, &val);
	if (rc < 0) {
		max_dbg(PR_OEM, "Failed to read chip id, err:%d\n", rc);
		max->chip_ok = false;
	}

	if (val == MAX77932_CHIP_ID)
		max->chip_ok = true;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			max->chip_ok);

	return len;
}

static DEVICE_ATTR(chip_ok, S_IRUGO, max_attr_show_chip_ok, NULL);

static struct attribute *max_attributes[] = {
	&dev_attr_chip_ok.attr,
	NULL,
};

static const struct attribute_group max_attr_group = {
	.attrs = max_attributes,
};

static int max77932_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{

	struct max77932_chip *max;
	int rc;

	max = devm_kzalloc(&client->dev, sizeof(*max), GFP_DMA);
	if (!max)
		return -ENOMEM;

	max->dev = &client->dev;
	max->client = client;
	i2c_set_clientdata(client, max);

	max_parse_dt(max);
	max->regmap = devm_regmap_init_i2c(client, &i2c_max77932_regmap_config);
	if (!max->regmap)
		return -ENODEV;

	rc = sysfs_create_group(&max->dev->kobj, &max_attr_group);
	if (rc)
		max_dbg(PR_OEM, "Failed to register sysfs, err:%d\n", rc);

	max_dbg(PR_OEM, "max cp probe successfully\n");
	return 0;
}

static int max77932_remove(struct i2c_client *client)
{
	//struct max77932_chip *max = i2c_get_clientdata(client);

	return 0;
}

static void max77932_shutdown(struct i2c_client *client)
{
	max_dbg(PR_OEM, "max watch dog driver shutdown!\n");
}

static struct of_device_id max77932_match_table[] = {
	{.compatible = "maxim,max77932",},
	{},
};
MODULE_DEVICE_TABLE(of, max77932_match_table);

static const struct i2c_device_id max77932_id[] = {
	{ "max77932", MAX77932},
	{},
};
MODULE_DEVICE_TABLE(i2c, max77932_id);

static struct i2c_driver max77932_driver = {
	.driver	= {
		.name   = "max77932",
		.owner  = THIS_MODULE,
		.of_match_table = max77932_match_table,
	},
	.id_table       = max77932_id,

	.probe          = max77932_probe,
	.remove		= max77932_remove,
	.shutdown	= max77932_shutdown,

};

module_i2c_driver(max77932_driver);

MODULE_DESCRIPTION("Mamim Max77932 Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments");
