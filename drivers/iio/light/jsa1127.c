/*
 * JSA1127 Ambient Light Sensor Driver
 *
 * Copyright (c) 2014, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/acpi.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#define JSA1127_DRV_NAME		"jsa1127"

#define JSA1127_CMD_SHUTDOWN		0x8C
#define JSA1127_CMD_CONT_INTEG_MODE	0x0C
#define JSA1127_CMD_ONETIME_INTEG_MODE	0x04
#define JSA1127_CMD_START_INTEG		0x08
#define JSA1127_CMD_STOP_INTEG		0x30
#define JSA1127_CMD_CHIP_TEST		0x8E

#define JSA1127_CMD_MASK		0XFF

#define JSA1127_DATA_VALID_MASK		0x80
#define JSA1127_DATA_MSB_MASK		0x7F
#define JSA1127_DATA_LSB_MASK		0xFF
#define JSA1127_DATA_MASK		0x7FFF

#define JSA1127_1MS_INTEG_RESOLUTION	5450000
#define JSA1127_DEF_INTEG_DELAY		100 /* milliseconds */
#define JSA1127_DEF_RINT_VALUE		100 /* Kohm */

enum jsa1127_op_mode {
	JSA1127_SHUTDOWN_MODE, /* Shutdown the device */
	JSA1127_SINGLE_CONV_MODE, /* stops conv after single integ cycle */
	JSA1127_CONT_CONV_MODE, /* Continous data mode */
};

struct jsa1127_data {
	struct i2c_client *client;
	struct mutex lock;
	u8 opmode;
	unsigned long rint_value; /* RINT resistor value in Kohm */
	unsigned long integ_delay; /* Integration delay in ms */
	u8 state_flags; /* suspend state cache */
};

static unsigned long round_to_next(unsigned long x, unsigned long y)
{
        unsigned long __z;

        if (x % y < y / 2)
                __z = x - (x % (y));
        else
                __z = (((x) + (y - 1)) / y) * y;

        return __z;
}

static int jsa1127_send_cmd(struct jsa1127_data *data, u8 cmd)
{
	int ret;

	ret = i2c_master_send(data->client, &cmd, 1);

	if (ret < 0)
		dev_err(&data->client->dev,
			"jsa1127 send cmd (0x%02X) error(%d)\n", cmd, ret);

	return ret;
}

static int jsa1127_read_data(struct jsa1127_data *data, char *buf, u8 len)
{
	int ret;

	ret = i2c_master_recv(data->client, buf, len);

	if (ret < 0)
		dev_err(&data->client->dev,
			"jsa1127 read data error(%d)\n", ret);
	return ret;
}

static int jsa1127_set_op_mode(struct jsa1127_data *data,
				enum jsa1127_op_mode opmode)
{
	int ret;

	switch (opmode) {
		case JSA1127_SHUTDOWN_MODE:
			ret = jsa1127_send_cmd(data, JSA1127_CMD_SHUTDOWN);
			break;
		case JSA1127_SINGLE_CONV_MODE:
			ret = jsa1127_send_cmd(data,
						JSA1127_CMD_ONETIME_INTEG_MODE);
			break;
		case JSA1127_CONT_CONV_MODE:
			ret = jsa1127_send_cmd(data, JSA1127_CMD_CONT_INTEG_MODE);
			break;
		default:
			ret = -EINVAL;
	}

	if (ret > 0)
		data->opmode = opmode;
	else
		dev_err(&data->client->dev, "set op mode error %d\n", opmode);

	return ret;
}

static int jsa1127_read_channel(struct jsa1127_data *data, int *val)
{
	int ret;
	u8 buf[2];

	ret = jsa1127_set_op_mode(data, JSA1127_SINGLE_CONV_MODE);

	if (ret < 0)
		goto read_chan_err;

	/* start conversion */
	ret = jsa1127_send_cmd(data, JSA1127_CMD_START_INTEG);

	if (ret < 0)
		goto read_chan_err;

	/* wait for data */
	msleep(data->integ_delay);

	/* stop conversion */
	ret = jsa1127_send_cmd(data, JSA1127_CMD_STOP_INTEG);

	if (ret < 0)
		goto read_chan_err;

	/* read data */
	ret = jsa1127_read_data(data, buf, 2);

	if (ret < 0)
		goto read_chan_err;

	/* check for valid data */
	if (!(buf[1] & JSA1127_DATA_VALID_MASK))
		ret = -EINVAL;
	else
		*val = (buf[1] << 8 | buf[0]) & JSA1127_DATA_MASK;

	/* shutdown the device */
	ret = jsa1127_set_op_mode(data, JSA1127_SHUTDOWN_MODE);

	if (ret < 0)
		goto read_chan_err;

	return ret;

read_chan_err:
	dev_err(&data->client->dev, "read channel error %d\n", ret);
	return ret;
}

static int jsa1127_read_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int *val, int *val2, long mask)
{
	int ret;
	struct jsa1127_data *data = iio_priv(indio_dev);

	mutex_lock(&data->lock);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = jsa1127_read_channel(data, val);
		ret = ret < 0 ? ret : IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = JSA1127_1MS_INTEG_RESOLUTION / data->integ_delay;
		*val2 = BIT(15);
		ret = IIO_VAL_FRACTIONAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&data->lock);

	return ret;
}

static const struct iio_chan_spec jsa1127_channels[] = {
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_SCALE),
	},
};

static const struct iio_info jsa1127_info = {
	.driver_module		= THIS_MODULE,
	.read_raw		= &jsa1127_read_raw,
};

static int jsa1127_chip_init(struct jsa1127_data *data)
{
	int ret;
	u8 output;

	/* shutdown the device */
	ret = jsa1127_set_op_mode(data, JSA1127_SHUTDOWN_MODE);

	if (ret < 0)
		goto chip_init_err;

	/* self test the device */
	ret = jsa1127_send_cmd(data, JSA1127_CMD_CHIP_TEST);

	if (ret < 0)
		goto chip_init_err;

	ret = jsa1127_read_data(data, &output, 1);

	if (ret < 0)
		goto chip_init_err;

	if (output != JSA1127_CMD_CHIP_TEST) {
		ret = -ENODEV;
		goto chip_init_err;
	}

	data->integ_delay = JSA1127_DEF_INTEG_DELAY;
	data->rint_value = JSA1127_DEF_RINT_VALUE;

	return 0;

chip_init_err:
	dev_err(&data->client->dev, "chip init error %d\n", ret);
	return ret;
}

static int jsa1127_acpi_probe(struct i2c_client *client,
				struct jsa1127_data *data)
{
	const struct acpi_device_id *id;
	struct device *dev;
	acpi_status status;
	unsigned long long output;

	if (!client)
		return -EINVAL;

	dev = &client->dev;

	if (!ACPI_HANDLE(dev))
		return -ENODEV;

	id = acpi_match_device(dev->driver->acpi_match_table, dev);

	if (!id)
		return -ENODEV;

	status = acpi_evaluate_integer(ACPI_HANDLE(dev), "RINT",
				NULL, &output);

	if (ACPI_FAILURE(status))
		return -EINVAL;

	data->rint_value = output;

	data->integ_delay = round_to_next(data->rint_value, 100);

	return 0;
}

static int jsa1127_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct jsa1127_data *data;
	struct iio_dev *indio_dev;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));

	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);

	i2c_set_clientdata(client, indio_dev);

	data->client = client;
	mutex_init(&data->lock);
	indio_dev->dev.parent = &client->dev;
	indio_dev->channels = jsa1127_channels;
	indio_dev->num_channels = ARRAY_SIZE(jsa1127_channels);
	indio_dev->name = JSA1127_DRV_NAME;
	indio_dev->modes = INDIO_DIRECT_MODE;

	ret = jsa1127_chip_init(data);

	if (ret < 0)
		return ret;

	ret = jsa1127_acpi_probe(client, data);

	if (ret)
		dev_info(&client->dev, "acpi probe failed (%d)\n", ret);

	indio_dev->info = &jsa1127_info;

	ret = devm_iio_device_register(&client->dev, indio_dev);
	if (ret) {
		dev_err(&client->dev, "%s: regist device failed\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static int jsa1127_remove(struct i2c_client *client)
{
	struct jsa1127_data *data;
	int ret;

	data = iio_priv(i2c_get_clientdata(client));

	mutex_lock(&data->lock);

	/* shutdown the device */
	ret = jsa1127_set_op_mode(data, JSA1127_SHUTDOWN_MODE);

	if (ret < 0)
		dev_err(&client->dev, "send shutdown cmd failed\n");

	mutex_unlock(&data->lock);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int jsa1127_suspend(struct device *dev)
{
	int ret;
	u8 opmode;
	struct jsa1127_data *data;

	data = iio_priv(i2c_get_clientdata(to_i2c_client(dev)));

	mutex_lock(&data->lock);

	opmode = data->opmode;

	/* shutdown the device */
	ret = jsa1127_set_op_mode(data, JSA1127_SHUTDOWN_MODE);

	if (ret < 0)
		dev_err(dev, "send shutdown cmd failed\n");
	else
		data->state_flags = opmode;

	mutex_unlock(&data->lock);

	return 0;
}

static int jsa1127_resume(struct device *dev)
{
	int ret;
	struct jsa1127_data *data;

	data = iio_priv(i2c_get_clientdata(to_i2c_client(dev)));

	mutex_lock(&data->lock);

	if (data->opmode != data->state_flags)
		ret = jsa1127_set_op_mode(data, data->state_flags);

	mutex_unlock(&data->lock);

	return 0;
}

static SIMPLE_DEV_PM_OPS(jsa1127_pm_ops, jsa1127_suspend, jsa1127_resume);

#define JSA1127_PM_OPS &jsa1127_pm_ops

#else

#define JSA1127_PM_OPS NULL

#endif

static const struct acpi_device_id jsa1127_acpi_match[] = {
	{"JSA1127", 0},
	{ },
};
MODULE_DEVICE_TABLE(acpi, jsa1127_acpi_match);

static const struct i2c_device_id jsa1127_id[] = {
	{ JSA1127_DRV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, jsa1127_id);

static struct i2c_driver jsa1127_driver = {
	.driver = {
		.name	= JSA1127_DRV_NAME,
		.pm	= JSA1127_PM_OPS,
		.owner	= THIS_MODULE,
		.acpi_match_table = ACPI_PTR(jsa1127_acpi_match),
	},
	.probe		= jsa1127_probe,
	.remove		= jsa1127_remove,
	.id_table	= jsa1127_id,
};
module_i2c_driver(jsa1127_driver);

MODULE_AUTHOR("Sathya Kuppuswamy <sathyanarayanan.kuppuswamy@linux.intel.com>");
MODULE_DESCRIPTION("JSA1127 ambient light sensor driver");
MODULE_LICENSE("GPL v2");
