/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "tps65132: %s: " fmt, __func__
#define POWER_ON_DELAY_MS 50

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>

#include "tps65132.h"

/* Data filled from device tree */
struct tps65132_platform_data {
	const char *name;
	u32 enp_gpio;
	u32 enn_gpio;
	struct tps65132_pinctrl_res pin_res;
};

struct tps65132_data {
	struct i2c_client *client;
	struct tps65132_platform_data *pdata;
	u16 addr;
};

static int tps65132_i2c_read(struct i2c_client *client, char *writebuf,
			   int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = 0,
				 .len = writelen,
				 .buf = writebuf,
			 },
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}

static int tps65132_i2c_write(struct i2c_client *client, char *writebuf,
			    int writelen)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
		 },
	};

	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s: i2c write error.\n", __func__);

	return ret;
}
static int tps65132_write_reg(struct i2c_client *client, u8 addr, const u8 val)
{
	u8 buf[2] = {0};

	buf[0] = addr;
	buf[1] = val;

	return tps65132_i2c_write(client, buf, sizeof(buf));
}

static int tps65132_read_reg(struct i2c_client *client, u8 addr, u8 *val)
{
	return tps65132_i2c_read(client, &addr, 1, val, 1);
}

/* enable enn & enp output */
int tps65132_power_on(struct tps65132_data *data, bool on)
{
	pr_debug("gpio enn enp pull up\n");

	gpio_set_value_cansleep(data->pdata->enp_gpio, 1);
	gpio_set_value_cansleep(data->pdata->enn_gpio, 1);

	return 0;
}
EXPORT_SYMBOL(tps65132_power_on);

int tps65132_power_init(struct tps65132_data *data)
{
	pr_debug("Configure 5.4V power supply\n");

	/* VPOS Voltage Setting */
	tps65132_write_reg(data->client, 0x00, 0x0E);
	/* VNEG Voltage Setting */
	tps65132_write_reg(data->client, 0x01, 0x0E);
	tps65132_write_reg(data->client, 0x03, 0x0F);
	tps65132_write_reg(data->client, 0xFF, 0xF0);
	msleep(POWER_ON_DELAY_MS);

	return 0;
}
EXPORT_SYMBOL(tps65132_power_init);

static int tps65132_parse_dt(struct device *dev,
			struct tps65132_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;

	pr_debug("start to parse devicetree\n");

	pdata->enp_gpio = of_get_named_gpio_flags(np, "ti,enp-gpio",
				0, &flags);
	if (pdata->enp_gpio < 0) {
		pr_err("enp gpio get fail\n");
		return -EINVAL;
	}
	pr_debug("enp_gpio = %d\n", pdata->enp_gpio);

	pdata->enn_gpio = of_get_named_gpio_flags(np, "ti,enn-gpio",
				0, &flags);
	if (pdata->enn_gpio < 0) {
		pr_err("enn gpio get fail\n");
		return -EINVAL;
	}

	pr_debug("enn_gpio = %d\n", pdata->enn_gpio);

	return 0;
}

static void tps65132_pinctrl_set_state(
	struct tps65132_platform_data  *platform_data,
	bool active)
{
	struct pinctrl_state *pin_state;
	int rc;

	pin_state = active ? platform_data->pin_res.gpio_state_active
				: platform_data->pin_res.gpio_state_suspend;

	if (!IS_ERR_OR_NULL(pin_state)) {
		pr_debug("tps 65132 pin status is %d\n", active);

		/* Actually wirte the pin state configuration to hardware */
		rc = pinctrl_select_state(platform_data->pin_res.pinctrl,
						pin_state);
		if (rc)
			pr_err("can not set %s pins\n",
					active ? PINCTRL_STATE_DEFAULT
					: PINCTRL_STATE_SLEEP);
	} else {
		pr_err("invalid '%s' pinstate\n",
				active ? PINCTRL_STATE_DEFAULT
				: PINCTRL_STATE_SLEEP);
	}
}

static int tps65132_pinctrl_init(struct i2c_client *client,
				struct tps65132_platform_data *platform_data)
{
	/* Try to obtain pinctrl handle */
	platform_data->pin_res.pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(platform_data->pin_res.pinctrl)) {
		pr_err("failed to get pinctrl\n");
		return PTR_ERR(platform_data->pin_res.pinctrl);
	}

	/* Get the active configuration */
	platform_data->pin_res.gpio_state_active
		= pinctrl_lookup_state(platform_data->pin_res.pinctrl,
					"pmx_tps_active");
	if (IS_ERR(platform_data->pin_res.gpio_state_active)) {
		pr_err("can not get active pinstate\n");
		return -EINVAL;
	}

	/* Get the sleep configuration */
	platform_data->pin_res.gpio_state_suspend
		= pinctrl_lookup_state(platform_data->pin_res.pinctrl,
				"pmx_tps_suspend");
	if (IS_ERR(platform_data->pin_res.gpio_state_suspend)) {
		pr_err("can not get sleep pinstate\n");
		return -EINVAL;
	}

	return 0;
}

static int tps65132_gpio_configure(struct tps65132_platform_data *pdata,
					bool state)
{
	int err = 0;

	if (gpio_is_valid(pdata->enp_gpio)) {
		/* configure tps65132 ENP gpio */
		err = gpio_request(pdata->enp_gpio, "tps65132_enp_gpio");
		if (err) {
			pr_err("enp gpio request failed\n");
			return -EINVAL;
		}

		err = gpio_direction_output(pdata->enp_gpio, 0);
		if (err) {
			pr_err("set_direction for enp gpio failed\n");
			return -EINVAL;
		}

		gpio_set_value_cansleep(pdata->enp_gpio, state);
	} else {
			pr_err("ENP GPIO not provided\n");
			return -EINVAL;
	}

	if (gpio_is_valid(pdata->enn_gpio)) {
		err = gpio_request(pdata->enn_gpio, "tps65132_enn_gpio");
		if (err) {
			pr_err("enn gpio request failed\n");
			return -EINVAL;
		}
		err = gpio_direction_output(pdata->enn_gpio, 0);
		if (err) {
			pr_err("set_direction for reset gpio failed\n");
			return -EINVAL;
		}

		gpio_set_value_cansleep(pdata->enn_gpio, state);
	} else {
			pr_err("ENN GPIO not provided\n");
			return -EINVAL;
		}
	return err;

}

static int tps65132_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct tps65132_platform_data *pdata;
	struct tps65132_data *data;
	u8 reg_value;
	u8 reg_addr;
	int err;

	pr_info("i2c addr = %x\n", client->addr);

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct tps65132_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		err = tps65132_parse_dt(&client->dev, pdata);
		if (err) {
			dev_err(&client->dev, "DT parsing failed\n");
			return err;
		}
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata) {
		dev_err(&client->dev, "Invalid pdata\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C not supported\n");
		err = -ENODEV;
		goto kfree_dev;
	}

	data = devm_kzalloc(&client->dev,
			sizeof(struct tps65132_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Not enough memory\n");
		err = -ENOMEM;
		goto kfree_dev;
	}

	data->client = client;
	data->pdata = pdata;
	data->addr = client->addr;

	i2c_set_clientdata(client, data);

	err = tps65132_pinctrl_init(client, data->pdata);
	if (err) {
		dev_err(&client->dev, "failed to get pin ctrl resources\n");
		goto free_gpio;
	} else {
		tps65132_pinctrl_set_state(data->pdata, true);
	}

	err = tps65132_gpio_configure(data->pdata, true);
	if (err < 0) {
		dev_err(&client->dev, "failed to configure GPIO\n");
		goto free_gpio;
	}
	/* check the controller id */
	reg_addr = 0xFF;
	err = tps65132_read_reg(client, reg_addr, &reg_value);
	if (err < 0) {
		dev_err(&client->dev, "version read failed");
		goto kfree_dev;
	}
	dev_info(&client->dev, "Device ID = 0x%x\n", reg_value);

	err = tps65132_power_init(data);
	if (err) {
		dev_err(&client->dev, "power init failed");
		goto kfree_dev;
	}
	return 0;

free_gpio:
	if (gpio_is_valid(pdata->enn_gpio))
		gpio_free(pdata->enn_gpio);
	if (gpio_is_valid(pdata->enp_gpio))
		gpio_free(pdata->enp_gpio);
kfree_dev:
	if (pdata)
		devm_kfree(&client->dev, pdata);
	if (data)
		devm_kfree(&client->dev, data);

	return err;
}

static int tps65132_remove(struct i2c_client *client)
{
	struct tps65132_data *data = i2c_get_clientdata(client);

	pr_debug("driver remove\n");

	if (gpio_is_valid(data->pdata->enp_gpio))
		gpio_free(data->pdata->enp_gpio);

	if (gpio_is_valid(data->pdata->enn_gpio))
		gpio_free(data->pdata->enn_gpio);

	if (data->pdata)
		devm_kfree(&client->dev, data->pdata);
	if (data)
		devm_kfree(&client->dev, data);
	return 0;
}

static const struct i2c_device_id tps65132_id[] = {
	{"tps65132", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, tps65132_id);

static struct of_device_id tps65132_match_table[] = {
	{ .compatible = "ti,tps65132", },
	{ },
};

static struct i2c_driver tps65132_driver = {
	.probe = tps65132_probe,
	.remove = tps65132_remove,
	.driver = {
		.name = "tps65132",
		.owner = THIS_MODULE,
		.of_match_table = tps65132_match_table,
		},
	.id_table = tps65132_id,
};

static int __init tps65132_init(void)
{
	pr_info("TPS65132 driver: initialize.");
	return  i2c_add_driver(&tps65132_driver);
}

static void __exit tps65132_exit(void)
{
	pr_info("TPS65132 driver: release.");
	i2c_del_driver(&tps65132_driver);
}

module_init(tps65132_init);
module_exit(tps65132_exit);

MODULE_DESCRIPTION("TPS65132 LCD Bias driver");
MODULE_LICENSE("GPL v2");
