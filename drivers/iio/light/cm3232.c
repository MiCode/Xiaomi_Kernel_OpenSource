/*
 * Copyright (C) 2014 Capella Microsystems Inc.
 * Author: Kevin Tsai <ktsai@capellamicro.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2, as published
 * by the Free Software Foundation.
 *
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/init.h>
#include <linux/acpi.h>

/* Registers Address */
#define CM3232_REG_ADDR_CMD		0x00
#define CM3232_REG_ADDR_ALS		0x50
#define CM3232_REG_ADDR_ID		0x53

/* CMD register */
#define CM3232_CMD_ALS_DISABLE		BIT(0)
#define	CM3232_CMD_ALS_HS		BIT(1)

#define CM3232_CMD_ALS_IT_SHIFT         2
#define CM3232_CMD_ALS_IT_MASK          (0x07 << CM3232_CMD_ALS_IT_SHIFT)
#define CM3232_CMD_ALS_IT_DEFAULT       (0x01 << CM3232_CMD_ALS_IT_SHIFT)

#define	CM3232_CMD_ALS_RESET		BIT(6)

#define CM3232_CMD_DEFAULT		CM3232_CMD_ALS_IT_DEFAULT

#define CM3232_CALIBSCALE_DEFAULT	100000
#define CM3232_CALIBSCALE_RESOLUTION	100000
#define CM3232_MLUX_PER_LUX		1000

#define CM3232_MLUX_PER_BIT_DEFAULT	64
#define CM3232_MLUX_PER_BIT_BASE_IT	100000
static const int CM3232_als_it_bits[] = { 0, 1, 2, 3, 4, 5};
static const int CM3232_als_it_values[] = {
			100000, 200000, 400000, 800000, 1600000, 3200000};

struct cm3232_als_info {
	u32 id;
	int calibscale;
	int mlux_per_bit;
	int mlux_per_bit_base_it;
	const int *als_it_bits;
	const int *als_it_values;
	const int num_als_it;
	int als_raw;
};

static struct cm3232_als_info cm3232_als_info_default = {
	.id = 3232,
	.calibscale = CM3232_CALIBSCALE_DEFAULT,
	.mlux_per_bit = CM3232_MLUX_PER_BIT_DEFAULT,
	.mlux_per_bit_base_it = CM3232_MLUX_PER_BIT_BASE_IT,
	.als_it_bits = CM3232_als_it_bits,
	.als_it_values = CM3232_als_it_values,
	.num_als_it = ARRAY_SIZE(CM3232_als_it_bits),
};

struct cm3232_chip {
	struct i2c_client *client;
	struct mutex lock;
	struct cm3232_als_info *als_info;
	u8 regs_cmd;
};

static int cm3232_get_lux(struct cm3232_chip *chip);
static int cm3232_read_als_it(struct cm3232_chip *chip, int *val2);

/**
 * cm3232_reg_init() - Initialize CM3232 registers
 * @chip:	pointer of struct cm3232.
 *
 * Initialize CM3232 ambient light sensor register to default values.
 *
  Return: 0 for success; otherwise for error code.
 */
static int cm3232_reg_init(struct cm3232_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct cm3232_als_info *als_info;
	s32 ret;

	/* Identify device */
	ret = i2c_smbus_read_word_data(client, CM3232_REG_ADDR_ID);
	if (ret < 0)
		return ret;
	if ((ret & 0xFF) != 0x32)
		return -ENODEV;

	/* Disable and reset device */
	chip->regs_cmd = CM3232_CMD_ALS_DISABLE | CM3232_CMD_ALS_RESET;
	ret = i2c_smbus_write_byte_data(client, CM3232_REG_ADDR_CMD,
				chip->regs_cmd);
	if (ret < 0)
		return ret;

	/* Initialization */
	als_info = chip->als_info = &cm3232_als_info_default;
	chip->regs_cmd = CM3232_CMD_DEFAULT;

	/* Configure register */
	ret = i2c_smbus_write_byte_data(client, CM3232_REG_ADDR_CMD,
				chip->regs_cmd);
	if (ret < 0)
		return ret;

	return 0;
}

/**
 *  cm3232_read_als_it() - Get sensor integration time (ms)
 *  @chip:	pointer of struct cm3232
 *  @val2:	pointer of int to load the als_it value.
 *
 *  Report the current integration time in milliseconds.
 *
 *  Return: IIO_VAL_INT_PLUS_MICRO for success, otherwise -EINVAL.
 */
static int cm3232_read_als_it(struct cm3232_chip *chip, int *val2)
{
	struct cm3232_als_info *als_info = chip->als_info;
	u16 als_it;
	int i;

	als_it = chip->regs_cmd;
	als_it &= CM3232_CMD_ALS_IT_MASK;
	als_it >>= CM3232_CMD_ALS_IT_SHIFT;
	for (i = 0; i < als_info->num_als_it; i++) {
		if (als_it == als_info->als_it_bits[i]) {
			*val2 = als_info->als_it_values[i];
			return IIO_VAL_INT_PLUS_MICRO;
		}
	}

	return -EINVAL;
}

/**
 * cm3232_write_als_it() - Write sensor integration time
 * @chip:	pointer of struct cm3232.
 * @val:	integration time in milliseconds.
 *
 * Convert integration time (ms) to sensor value.
 *
 * Return: i2c_smbus_write_word_data command return value.
 */
static int cm3232_write_als_it(struct cm3232_chip *chip, int val)
{
	struct i2c_client *client = chip->client;
	struct cm3232_als_info *als_info = chip->als_info;
	u16 als_it;
	int ret, i;

	for (i = 0; i < als_info->num_als_it; i++)
		if (val <= als_info->als_it_values[i])
			break;
	if (i >= als_info->num_als_it)
		i = als_info->num_als_it - 1;

	als_it = als_info->als_it_bits[i];
	als_it <<= CM3232_CMD_ALS_IT_SHIFT;

	mutex_lock(&chip->lock);
	chip->regs_cmd &= ~CM3232_CMD_ALS_IT_MASK;
	chip->regs_cmd |= als_it;
	ret = i2c_smbus_write_byte_data(client, CM3232_REG_ADDR_CMD,
			chip->regs_cmd);
	mutex_unlock(&chip->lock);

	return ret;
}

/**
 * cm3232_get_lux() - report current lux value
 * @chip:	pointer of struct cm3232.
 *
 * Convert sensor raw data to lux.  It depends on integration
 * time and calibscale variable.
 *
 * Return: Positive value is lux, otherwise is error code.
 */
static int cm3232_get_lux(struct cm3232_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct cm3232_als_info *als_info = chip->als_info;
	int ret;
	int als_it;
	u64 tmp;

	/* Calculate mlux per bit based on als_it */
	ret = cm3232_read_als_it(chip, &als_it);
	if (ret < 0)
		return -EINVAL;
	tmp = (__force u64)als_info->mlux_per_bit;
	tmp *= als_info->mlux_per_bit_base_it;
	tmp = div_u64 (tmp, als_it);

	/* Get als_raw */
	als_info->als_raw = i2c_smbus_read_word_data(
				client,
				CM3232_REG_ADDR_ALS);
	if (als_info->als_raw < 0)
		return als_info->als_raw;

	tmp *= als_info->als_raw;
	tmp *= als_info->calibscale;
	tmp = div_u64(tmp, CM3232_CALIBSCALE_RESOLUTION);
	tmp = div_u64(tmp, CM3232_MLUX_PER_LUX);

	if (tmp > 0xFFFF)
		tmp = 0xFFFF;

	return (int)tmp;
}

static int cm3232_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long mask)
{
	struct cm3232_chip *chip = iio_priv(indio_dev);
	struct cm3232_als_info *als_info = chip->als_info;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		ret = cm3232_get_lux(chip);
		if (ret < 0)
			return ret;
		*val = ret;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_CALIBSCALE:
		*val = als_info->calibscale;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_INT_TIME:
		*val = 0;
		ret = cm3232_read_als_it(chip, val2);
		return ret;
	}

	return -EINVAL;
}

static int cm3232_write_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int val, int val2, long mask)
{
	struct cm3232_chip *chip = iio_priv(indio_dev);
	struct cm3232_als_info *als_info = chip->als_info;
	long ms;

	switch (mask) {
	case IIO_CHAN_INFO_CALIBSCALE:
		als_info->calibscale = val;
		return val;
	case IIO_CHAN_INFO_INT_TIME:
		ms = val * 1000000 + val2;
		return cm3232_write_als_it(chip, (int)ms);
	}

	return -EINVAL;
}

/**
 * cm3232_get_it_available() - Get available ALS IT value
 * @dev:	pointer of struct device.
 * @attr:	pointer of struct device_attribute.
 * @buf:	pointer of return string buffer.
 *
 * Display the available integration time in milliseconds.
 *
 * Return: string length.
 */
static ssize_t cm3232_get_it_available(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct cm3232_chip *chip = iio_priv(dev_to_iio_dev(dev));
	struct cm3232_als_info *als_info = chip->als_info;
	int i, len;

	for (i = 0, len = 0; i < als_info->num_als_it; i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "%u.%06u ",
			als_info->als_it_values[i]/1000000,
			als_info->als_it_values[i]%1000000);
	return len + scnprintf(buf + len, PAGE_SIZE - len, "\n");
}

static const struct iio_chan_spec cm3232_channels[] = {
	{
		.type = IIO_LIGHT,
		.info_mask_separate =
			BIT(IIO_CHAN_INFO_PROCESSED) |
			BIT(IIO_CHAN_INFO_CALIBSCALE) |
			BIT(IIO_CHAN_INFO_INT_TIME),
	}
};

static IIO_DEVICE_ATTR(in_illuminance_integration_time_available,
			S_IRUGO, cm3232_get_it_available, NULL, 0);

static struct attribute *cm3232_attributes[] = {
	&iio_dev_attr_in_illuminance_integration_time_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group cm3232_attribute_group = {
	.attrs = cm3232_attributes
};

static const struct iio_info cm3232_info = {
	.driver_module		= THIS_MODULE,
	.read_raw		= &cm3232_read_raw,
	.write_raw		= &cm3232_write_raw,
	.attrs			= &cm3232_attribute_group,
};

static int cm3232_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct cm3232_chip *chip;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*chip));
	if (!indio_dev) {
		dev_err(&client->dev, "devm_iio_device_alloc failed\n");
		return -ENOMEM;
	}

	chip = iio_priv(indio_dev);
	i2c_set_clientdata(client, indio_dev);
	chip->client = client;

	mutex_init(&chip->lock);
	indio_dev->dev.parent = &client->dev;
	indio_dev->channels = cm3232_channels;
	indio_dev->num_channels = ARRAY_SIZE(cm3232_channels);
	indio_dev->info = &cm3232_info;
	if (id && id->name)
		indio_dev->name = id->name;
	else
		indio_dev->name = (char *)dev_name(&client->dev);
	indio_dev->modes = INDIO_DIRECT_MODE;

	ret = cm3232_reg_init(chip);
	if (ret) {
		dev_err(&client->dev,
			"%s: register init failed\n",
			__func__);
		return ret;
	}

	ret = iio_device_register(indio_dev);
	return 0;
}

static int cm3232_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);

	iio_device_unregister(indio_dev);
	return 0;
}

static const struct i2c_device_id cm3232_id[] = {
	{ "cm3232", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, cm3232_id);

static const struct acpi_device_id cm3232_acpi_match[] = {
	{"CM3232", 0},
	{"CALS3232", 0},
	{ },
};
MODULE_DEVICE_TABLE(acpi, cm3232_acpi_match);

static const struct of_device_id cm3232_of_match[] = {
	{ .compatible = "capella,cm3232" },
	{ }
};

static struct i2c_driver cm3232_driver = {
	.driver = {
		.name	= "cm3232",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(cm3232_of_match),
		.acpi_match_table = ACPI_PTR(cm3232_acpi_match),
	},
	.id_table	= cm3232_id,
	.probe		= cm3232_probe,
	.remove		= cm3232_remove,
};

module_i2c_driver(cm3232_driver);

MODULE_AUTHOR("Kevin Tsai <ktsai@capellamicro.com>");
MODULE_DESCRIPTION("CM3232 ambient light sensor driver");
MODULE_LICENSE("GPL");
