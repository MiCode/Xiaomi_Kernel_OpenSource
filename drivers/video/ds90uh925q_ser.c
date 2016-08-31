/*
 * drivers/video/ds90uh925q_ser.c
 * FPDLink Serializer driver
 *
 * Copyright (C) 2012 NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/nvhost.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c/ds90uh925q_ser.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#define LVDS_SER_REG_CONFIG_1                   0x4
#define LVDS_SER_REG_CONFIG_1_BKWD_OVERRIDE     3
#define LVDS_SER_REG_CONFIG_1_BKWD              2

#define LVDS_SER_REG_DATA_PATH_CTRL             0x12
#define LVDS_SER_REG_DATA_PATH_CTRL_PASS_RGB    6

#define LVDS_SER_REG_CONFIG_0                   0x3
#define LVDS_SER_REG_CONFIG_0_TRFB              0

struct ds90uh925q_rec {
	struct ds90uh925q_platform_data *pdata;
};

static int ser_i2c_read(struct i2c_client *client, int reg, uint8_t *val)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev, "failed reading at 0x%02x\n", reg);
		return ret;
	}

	*val = ret;
	return 0;
}

static int ser_i2c_write(struct i2c_client *client, u8 reg, u8 val)
{
	int ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret)
		dev_err(&client->dev, "failed to write\n");

	return ret;
}

static int lvds_ser_config(struct i2c_client *client,
						bool is_fpdlinkII,
						bool support_hdcp,
						bool clk_rise_edge)
{
	u8 val;
	int err = 0;

	if (is_fpdlinkII) {
		err = ser_i2c_read(client, LVDS_SER_REG_CONFIG_1, &val);
		if (err < 0)
			return err;

		val |= (1 << LVDS_SER_REG_CONFIG_1_BKWD_OVERRIDE);
		val |= (1 << LVDS_SER_REG_CONFIG_1_BKWD);

		err = ser_i2c_write(client, LVDS_SER_REG_CONFIG_1, val);
		if (err < 0)
			return err;
	} else if (!support_hdcp) {
		err = ser_i2c_read(client, LVDS_SER_REG_DATA_PATH_CTRL, &val);
		if (err < 0)
			return err;

		val |= (1 << LVDS_SER_REG_DATA_PATH_CTRL_PASS_RGB);

		err = ser_i2c_write(client, LVDS_SER_REG_DATA_PATH_CTRL, val);
		if (err < 0)
			return err;
	}

	if (clk_rise_edge) {
		err = ser_i2c_read(client, LVDS_SER_REG_CONFIG_0, &val);
		if (err < 0)
			return err;

		val |= (1 << LVDS_SER_REG_CONFIG_0_TRFB);

		err = ser_i2c_write(client, LVDS_SER_REG_CONFIG_0, val);
	}

	return (err < 0 ? err : 0);
}

static int ds90uh925q_enable(struct i2c_client *client)
{
	struct ds90uh925q_rec *data = i2c_get_clientdata(client);
	int err;

	/* Turn on serializer chip */
	if (data->pdata->has_lvds_en_gpio)
		gpio_set_value(data->pdata->lvds_en_gpio, 1);

	err = lvds_ser_config(client,
				data->pdata->is_fpdlinkII,
				data->pdata->support_hdcp,
				data->pdata->clk_rise_edge);
	if (err)
		pr_err("%s: lvds failed\n", __func__);

	return err;
}

static void ds90uh925q_disable(struct i2c_client *client)
{
	struct ds90uh925q_rec *data = i2c_get_clientdata(client);

	/* Turn off serializer chip */
	if (data->pdata->has_lvds_en_gpio)
		gpio_set_value(data->pdata->lvds_en_gpio, 0);
}

static int ds90uh925q_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct ds90uh925q_rec *data;
	struct ds90uh925q_platform_data *pdata =
		client->dev.platform_data;
	int err;

	if (!i2c_check_functionality(client->adapter,
					I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "SMBUS Byte Data not Supported\n");
		return -EIO;
	}

	if (!pdata) {
		dev_err(&client->dev, "no platform data?\n");
		return -EINVAL;
	}

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	data->pdata = pdata;
	i2c_set_clientdata(client, data);

	err = ds90uh925q_enable(client);
	if (err)
		goto err_out;

	return 0;

err_out:
	i2c_set_clientdata(client, NULL);
	return err;
}

static int ds90uh925q_remove(struct i2c_client *client)
{
	struct ds90uh925q_rec *data = i2c_get_clientdata(client);

	ds90uh925q_disable(client);

	i2c_set_clientdata(client, NULL);

	return 0;
}

static int ds90uh925q_suspend(struct i2c_client *client, pm_message_t message)
{
	ds90uh925q_disable(client);

	return 0;
}

static int ds90uh925q_resume(struct i2c_client *client)
{
	return ds90uh925q_enable(client);
}


static const struct i2c_device_id ds90uh925q_id[] = {
	{ "ds90uh925q", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ds90uh925q_id);

static struct i2c_driver ds90uh925q_driver = {
	.driver = {
		.name = "ds90uh925q",
		.owner	= THIS_MODULE,
	},
	.probe    = ds90uh925q_probe,
	.remove   = ds90uh925q_remove,
	.suspend  = ds90uh925q_suspend,
	.resume   = ds90uh925q_resume,
	.id_table = ds90uh925q_id,
};

module_i2c_driver(ds90uh925q_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DS90UH925Q FPDLink Serializer driver");
MODULE_ALIAS("i2c:ds90uh925q_ser");
