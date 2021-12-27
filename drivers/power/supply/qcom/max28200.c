/*
 * max28200 charger pump watch dog driver
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

#define pr_fmt(fmt)	"[max28200] %s: " fmt, __func__
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
#include <linux/of_gpio.h>
#include <linux/gpio.h>

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

enum max28200_device {
	MAX28200,
};

#define MAX28200_BB_REG		0xBB
#define MAX28200_P00_MAKS	BIT(0)
#define MAX28200_P00_LOW	0x0
#define MAX28200_P00_HIG	0x1

struct max28200_chip {
	struct device *dev;
	struct i2c_client *client;
	struct regmap    *regmap;
	struct power_supply *max28200_psy;
	struct power_supply_desc max28200_psy_d;
	bool	enabled;
	struct  delayed_work monitor_work;
	struct notifier_block   nb;
	int cp_en_gpio;
};

static bool max28200_set_watchdog_enable(struct max28200_chip *max, bool enable)
{
	int rc;
	u8 val;

	if (enable) {
		val = MAX28200_P00_HIG;
	} else {
		val = MAX28200_P00_LOW;
	}

	if (max->cp_en_gpio) {
		gpio_direction_output(max->cp_en_gpio, val);
	} else {
		rc = regmap_update_bits(max->regmap, MAX28200_BB_REG, MAX28200_P00_MAKS, val);
		if (rc) {
			max_dbg(PR_OEM, "unable to update p00 reg");
			return rc;
		}
		schedule_delayed_work(&max->monitor_work, 10 * HZ);
	}

	max->enabled = enable;

	return rc;
}

static int max28200_get_watchdog_enable(struct max28200_chip *max)
{
	return max->enabled;
}

static enum power_supply_property max28200_props[] = {
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
};

static int max28200_get_property(struct power_supply *psy, enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct max28200_chip *max = power_supply_get_drvdata(psy);

	switch (psp) {
		case POWER_SUPPLY_PROP_CHARGE_ENABLED:
			val->intval = max28200_get_watchdog_enable(max);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static int max28200_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct max28200_chip *max = power_supply_get_drvdata(psy);

	switch (prop) {
		case POWER_SUPPLY_PROP_CHARGE_ENABLED:
			max28200_set_watchdog_enable(max, !!val->intval);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int max28200_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	int ret;

	switch (prop) {
		case POWER_SUPPLY_PROP_CHARGE_ENABLED:
			ret = 1;
			break;
		default:
			ret = 0;
			break;
	}
	return ret;
}

static int max28200_psy_register(struct max28200_chip *max)
{
	struct power_supply_config max28200_psy_cfg = {};

	max->max28200_psy_d.name = "cp_wdog";
	max->max28200_psy_d.type = POWER_SUPPLY_TYPE_BMS;
	max->max28200_psy_d.properties = max28200_props;
	max->max28200_psy_d.num_properties = ARRAY_SIZE(max28200_props);
	max->max28200_psy_d.get_property = max28200_get_property;
	max->max28200_psy_d.set_property = max28200_set_property;
	max->max28200_psy_d.property_is_writeable = max28200_prop_is_writeable;

	max28200_psy_cfg.drv_data = max;
	max28200_psy_cfg.num_supplicants = 0;
	max->max28200_psy = devm_power_supply_register(max->dev,
			&max->max28200_psy_d,
			&max28200_psy_cfg);
	if (IS_ERR(max->max28200_psy)) {
		max_dbg(PR_OEM, "Failed to register max28200_psy");
		return PTR_ERR(max->max28200_psy);
	}

	return 0;
}

static int max28200_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	union power_supply_propval pval = {-EINVAL, };
	struct power_supply *psy = v;
	int rc;

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "usb") == 0)) {
		rc = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &pval);
		if (rc < 0) {
			return NOTIFY_OK;
		}
		max_dbg(PR_OEM, "usb present :%d\n", pval.intval);

	}

	return NOTIFY_OK;
}

static int max28200_register_notifier(struct max28200_chip *max)
{
	int rc;

	max->nb.notifier_call = max28200_notifier_call;
	rc = power_supply_reg_notifier(&max->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		return rc;
	}
	return 0;
}

static void max28200_psy_unregister(struct max28200_chip *max)
{
	power_supply_unregister(max->max28200_psy);
}

static void max28200_monitor_workfunc(struct work_struct *work)
{
	struct max28200_chip *max = container_of(work,
			struct max28200_chip, monitor_work.work);

	schedule_delayed_work(&max->monitor_work, 10 * HZ);
}

static int max_parse_dt(struct max28200_chip *max)
{
	struct device_node *np = max->dev->of_node;
	int ret;

	max->cp_en_gpio = of_get_named_gpio(np, "mi,cp_en_gpio", 0);
	if ((!gpio_is_valid(max->cp_en_gpio))) {
		max_dbg(PR_OEM, "Don't supprot cp en gpio, i2c control it\n");
		max->cp_en_gpio = 0;
	} else {
		ret = gpio_request(max->cp_en_gpio, "cp_en_gpio");
		if (ret) {
			max_dbg(PR_OEM, "%s: unable to request max cp en gpio [%d]\n",
					__func__, max->cp_en_gpio);
		}
		ret = gpio_direction_output(max->cp_en_gpio, 0);
		if (ret) {
			max_dbg(PR_OEM, "%s: unable to set direction cp en gpio[%d]\n",
					__func__, max->cp_en_gpio);
		}
		max_dbg(PR_OEM, "supprot cp en gpio:%d\n", max->cp_en_gpio);
	}

	return 0;
}

static struct regmap_config i2c_max28200_regmap_config = {
	.reg_bits  = 8,
	.val_bits  = 8,
	.max_register  = 0xFF,
};

static int max28200_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{

	struct max28200_chip *max;

	max = devm_kzalloc(&client->dev, sizeof(*max), GFP_DMA);
	if (!max)
		return -ENOMEM;

	max->dev = &client->dev;
	max->client = client;
	i2c_set_clientdata(client, max);

	max_parse_dt(max);
	max->regmap = devm_regmap_init_i2c(client, &i2c_max28200_regmap_config);
	if (!max->regmap)
		return -ENODEV;

	INIT_DELAYED_WORK(&max->monitor_work, max28200_monitor_workfunc);
	max28200_psy_register(max);
	max28200_register_notifier(max);

	max_dbg(PR_OEM, "max watch dog probe successfully, %s\n", id->name);
	return 0;
}

static int max28200_remove(struct i2c_client *client)
{
	struct max28200_chip *max = i2c_get_clientdata(client);

	max28200_psy_unregister(max);

	return 0;
}

static void max28200_shutdown(struct i2c_client *client)
{
	max_dbg(PR_OEM, "max watch dog driver shutdown!\n");
}

static struct of_device_id max28200_match_table[] = {
	{.compatible = "maxim,max28200",},
	{},
};
MODULE_DEVICE_TABLE(of, max28200_match_table);

static const struct i2c_device_id max28200_id[] = {
	{ "max28200", MAX28200},
	{},
};
MODULE_DEVICE_TABLE(i2c, max28200_id);

static struct i2c_driver max28200_driver = {
	.driver	= {
		.name   = "max28200",
		.owner  = THIS_MODULE,
		.of_match_table = max28200_match_table,
	},
	.id_table       = max28200_id,

	.probe          = max28200_probe,
	.remove		= max28200_remove,
	.shutdown	= max28200_shutdown,

};

module_i2c_driver(max28200_driver);

MODULE_DESCRIPTION("Mamim Max28200 Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments");
