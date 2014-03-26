/*
 * Copyright (C) 2014 Capella Microsystems Inc.
 * Author: Kevin Tsai <ktsai@capellamicro.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2, as published
 * by the Free Software Foundation.
 *
 * Special thanks Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com>
 * help to add ACPI support.
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

#ifdef	CONFIG_ACPI
#include <linux/acpi.h>
#endif /* CONFIG_ACPI */

/* Registers Address */
#define CM3218X_REG_ADDR_CMD 0x00
#define CM3218X_REG_ADDR_WH 0x01
#define CM3218X_REG_ADDR_WL 0x02
#define CM3218X_REG_ADDR_TEST 0x03
#define CM3218X_REG_ADDR_ALS 0x04
#define CM3218X_REG_ADDR_STATUS 0x06
#define CM3218X_REG_ADDR_ID 0x07

/* Number of Configurable Registers */
#define CM3218X_CONF_REG_NUM 16

/* CMD register */
#define CM3218X_CMD_ALS_DISABLE BIT(0)
#define CM3218X_CMD_ALS_INT_EN BIT(1)
#define CM3218X_CMD_ALS_THRES_WINDOW BIT(2)

#define CM3218X_CMD_ALS_PERS_SHIFT 4
#define CM3218X_CMD_ALS_PERS_MASK (0x03 << CM3218X_CMD_ALS_PERS_SHIFT)
#define CM3218X_CMD_ALS_PERS_DEFAULT (0x01 << CM3218X_CMD_ALS_PERS_SHIFT)

#define CM3218X_CMD_ALS_IT_SHIFT 6
#define CM3218X_CMD_ALS_IT_MASK (0x0F << CM3218X_CMD_ALS_IT_SHIFT)
#define CM3218X_CMD_ALS_IT_DEFAULT (0x01 << CM3218X_CMD_ALS_IT_SHIFT)

#define CM3218X_CMD_ALS_HS_SHIFT 11
#define CM3218X_CMD_ALS_HS_MASK (0x01 << CM3218X_CMD_ALS_HS_SHIFT)
#define CM3218X_CMD_ALS_HS_DEFAULT (0x00 << CM3218X_CMD_ALS_HS_SHIFT)

#define CM3218X_CMD_DEFAULT (CM3218X_CMD_ALS_THRES_WINDOW |\
		CM3218X_CMD_ALS_PERS_DEFAULT |\
		CM3218X_CMD_ALS_IT_DEFAULT |\
		CM3218X_CMD_ALS_HS_DEFAULT)

#define CM3218X_WH_DEFAULT 0xFFFF
#define CM3218X_WL_DEFAULT 0x0000

#define CM3218X_CALIBSCALE_DEFAULT 100000
#define CM3218X_CALIBSCALE_RESOLUTION 100000
#define CM3218X_MLUX_PER_LUX 1000
#define CM3218X_THRESHOLD_PERCENT 10	/* 10 percent */

#define CM3218X_ARA 0x0C

/* CM3218X family */
enum {
	cm3218,
	cm32181,
	cm32182
};

/* CM3218 Family */
#define CM3218_MLUX_PER_BIT_DEFAULT 5	/* Depend on system */
#define CM3218_MLUX_PER_BIT_BASE_IT 800000
static const int CM3218_als_it_bits[] = {0, 1, 2, 3};
static const int CM3218_als_it_values[] = {100000, 200000, 400000, 800000};

/* CM32181 Family */
#define CM32181_MLUX_PER_BIT_DEFAULT 5
#define CM32181_MLUX_PER_BIT_BASE_IT 800000
static const int CM32181_als_it_bits[] = {12, 8, 0, 1, 2, 3};
static const int CM32181_als_it_values[] = {
			25000, 50000, 100000, 200000, 400000, 800000};

struct cm3218x_als_info {
	u32 id;
	int int_type;
#define CM3218X_INT_TYPE_SMBUS 0
#define CM3218X_INT_TYPE_I2C 1
	int regs_bmp;
	int calibscale;
	int mlux_per_bit;
	int mlux_per_bit_base_it;
	const int *als_it_bits;
	const int *als_it_values;
	const int num_als_it;
	int als_raw;
};

static struct cm3218x_als_info cm3218_info = {
	.id = 3218,
	.int_type = CM3218X_INT_TYPE_SMBUS,
	.regs_bmp = 0x0F,
	.calibscale = CM3218X_CALIBSCALE_DEFAULT,
	.mlux_per_bit = CM3218_MLUX_PER_BIT_DEFAULT,
	.mlux_per_bit_base_it = CM3218_MLUX_PER_BIT_BASE_IT,
	.als_it_bits = CM3218_als_it_bits,
	.als_it_values = CM3218_als_it_values,
	.num_als_it = ARRAY_SIZE(CM3218_als_it_bits),
};

static struct cm3218x_als_info cm32181_info = {
	.id = 32181,
	.int_type = CM3218X_INT_TYPE_I2C,
	.regs_bmp = 0x0F,
	.calibscale = CM3218X_CALIBSCALE_DEFAULT,
	.mlux_per_bit = CM32181_MLUX_PER_BIT_DEFAULT,
	.mlux_per_bit_base_it = CM32181_MLUX_PER_BIT_BASE_IT,
	.als_it_bits = CM32181_als_it_bits,
	.als_it_values = CM32181_als_it_values,
	.num_als_it = ARRAY_SIZE(CM32181_als_it_bits),
};

static struct cm3218x_als_info cm32182_info = {
	.id = 32182,
	.int_type = CM3218X_INT_TYPE_I2C,
	.regs_bmp = 0x0F,
	.calibscale = CM3218X_CALIBSCALE_DEFAULT,
	.mlux_per_bit = CM32181_MLUX_PER_BIT_DEFAULT,
	.mlux_per_bit_base_it = CM32181_MLUX_PER_BIT_BASE_IT,
	.als_it_bits = CM32181_als_it_bits,
	.als_it_values = CM32181_als_it_values,
	.num_als_it = ARRAY_SIZE(CM32181_als_it_bits),
};

struct cm3218x_chip {
	struct i2c_client *client;
	struct mutex lock;
	u16 conf_regs[CM3218X_CONF_REG_NUM];
	struct cm3218x_als_info *als_info;
};

static int cm3218x_get_lux(struct cm3218x_chip *chip);
static int cm3218x_threshold_update(struct cm3218x_chip *chip, int percent);
static int cm3218x_read_als_it(struct cm3218x_chip *chip, int *val2);

/**
 * cm3218x_read_ara() - Read ARA register
 * @chip:       pointer of struct cm3218x.
 *
 * Read SMBus ARA register.
 *
 * Return: -ENODEV for failed.  Otherwise return the register value.
 */
static int cm3218x_read_ara(struct cm3218x_chip *chip)
{
	unsigned char rxdata;
	struct i2c_msg msgs[] = {
		{
			.addr = CM3218X_ARA,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = &rxdata,
		},
	};

	if (i2c_transfer(chip->client->adapter, msgs, 1) < 0)
		return -ENODEV;
	return rxdata;
}

/**
 * cm3218x_interrupt_config() - Enable/Disable CM3218X interrupt
 * @chip:	pointer of struct cm3218x.
 * @enable:	0 to disable; otherwise to enable
 *
 * Config CM3218X interrupt control bit.
 *
 * Return: 0 for success; otherwise for error code.
 */
static int cm3218x_interrupt_config(struct cm3218x_chip *chip, int enable)
{
	struct i2c_client *client = chip->client;
	struct cm3218x_als_info *als_info = chip->als_info;
	int status;

	if (!als_info)
		return -ENODEV;

	/* Force to clean interrupt */
	if (als_info->int_type == CM3218X_INT_TYPE_I2C) {
		status = i2c_smbus_read_word_data(client,
				CM3218X_REG_ADDR_STATUS);
		if (status < 0)
			als_info->int_type = CM3218X_INT_TYPE_SMBUS;
	}
	if (als_info->int_type == CM3218X_INT_TYPE_SMBUS)
		cm3218x_read_ara(chip);

	if (enable)
		chip->conf_regs[CM3218X_REG_ADDR_CMD] |=
			CM3218X_CMD_ALS_INT_EN;
	else
		chip->conf_regs[CM3218X_REG_ADDR_CMD] &=
			~CM3218X_CMD_ALS_INT_EN;

	status = i2c_smbus_write_word_data(client, CM3218X_REG_ADDR_CMD,
		chip->conf_regs[CM3218X_REG_ADDR_CMD]);

	if (status < 0)
		return -ENODEV;

	return status;
}

#ifdef CONFIG_ACPI
/**
 * cm3218x_acpi_get_cpm_info() - Get CPM object from ACPI
 * @client	pointer of struct i2c_client.
 * @obj_name	pointer of ACPI object name.
 * @count	maximum size of return array.
 * @vals	pointer of array for return elements.
 *
 * Convert ACPI CPM table to array. Special thanks Srinivas Pandruvada's
 * help to implement this routine.
 *
 * Return: -ENODEV for fail.  Otherwise is number of elements.
 */
static int cm3218x_acpi_get_cpm_info(struct i2c_client *client, char *obj_name,
							int count, u64 *vals)
{
	acpi_handle handle;
	struct acpi_buffer buffer = {ACPI_ALLOCATE_BUFFER, NULL};
	int i;
	acpi_status status;
	union acpi_object *cpm;

	handle = ACPI_HANDLE(&client->dev);
	if (!handle)
		return -ENODEV;

	status = acpi_evaluate_object(handle, obj_name, NULL, &buffer);
	if (ACPI_FAILURE(status)) {
		dev_err(&client->dev, "object %s not found\n", obj_name);
		return -ENODEV;
	}

	cpm = buffer.pointer;
	for (i = 0; i < cpm->package.count && i < count; ++i) {
		union acpi_object *elem;
		elem = &(cpm->package.elements[i]);
		vals[i] = elem->integer.value;
	}

	kfree(buffer.pointer);

	return cpm->package.count;
}
#endif /* CONFIG_ACPI */

/**
 * cm3218x_reg_init() - Initialize CM3218X registers
 * @chip:	pointer of struct cm3218x.
 *
 * Initialize CM3218X ambient light sensor register to default values.
 *
  Return: 0 for success; otherwise for error code.
 */
static int cm3218x_reg_init(struct cm3218x_chip *chip)
{
	struct i2c_client *client = chip->client;
	int i;
	s32 ret;
	struct cm3218x_als_info *als_info;
#ifdef CONFIG_ACPI
	int cpm_elem_count;
	u64 cpm_elems[20];
#endif /* CONFIG_ACPI */

	/* Default device */
	chip->als_info = &cm3218_info;
	chip->conf_regs[CM3218X_REG_ADDR_CMD] = CM3218X_CMD_DEFAULT;
	chip->conf_regs[CM3218X_REG_ADDR_WH] = CM3218X_WH_DEFAULT;
	chip->conf_regs[CM3218X_REG_ADDR_WL] = CM3218X_WL_DEFAULT;

	/* Disable interrupt */
	cm3218x_interrupt_config(chip, 0);

	/* Disable Test Mode */
	i2c_smbus_write_word_data(client, CM3218X_REG_ADDR_TEST, 0x0000);

	/* Disable device */
	i2c_smbus_write_word_data(client, CM3218X_REG_ADDR_CMD,
		CM3218X_CMD_ALS_DISABLE);

	/* Identify device */
	ret = i2c_smbus_read_word_data(client, CM3218X_REG_ADDR_ID);
	if (ret < 0)
		return ret;
	switch (ret & 0xFF) {
	case 0x18:
		als_info = chip->als_info = &cm3218_info;
		if (ret & 0x0800)
			als_info->int_type = CM3218X_INT_TYPE_I2C;
		else
			als_info->int_type = CM3218X_INT_TYPE_SMBUS;
		break;
	case 0x81:
		als_info = chip->als_info = &cm32181_info;
		break;
	case 0x82:
		als_info = chip->als_info = &cm32182_info;
		break;
	default:
		return -ENODEV;
	}

#ifdef CONFIG_ACPI
	if (ACPI_HANDLE(&client->dev)) {
		/* Load from ACPI */
		cpm_elem_count = cm3218x_acpi_get_cpm_info(client, "CPM0",
							ARRAY_SIZE(cpm_elems),
							cpm_elems);
		if (cpm_elem_count > 0) {
			int header_num = 3;
			int reg_num = cpm_elem_count - header_num;

			als_info->id = cpm_elems[0];
			als_info->regs_bmp = cpm_elems[2];
			for (i = 0; i < reg_num; i++)
				if (als_info->regs_bmp & (1<<i))
					chip->conf_regs[i] =
						cpm_elems[header_num+i];
		}

		cpm_elem_count = cm3218x_acpi_get_cpm_info(client, "CPM1",
							ARRAY_SIZE(cpm_elems),
							cpm_elems);
		if (cpm_elem_count > 0) {
			als_info->mlux_per_bit = (int)cpm_elems[0] / 100;
			als_info->calibscale = (int)cpm_elems[1];
		}
	}
#endif /* CONFIG_ACPI */

	/* Force to disable interrupt */
	chip->conf_regs[CM3218X_REG_ADDR_CMD] &= ~CM3218X_CMD_ALS_INT_EN;

	/* Initialize registers */
	for (i = 0; i < CM3218X_CONF_REG_NUM; i++) {
		if (als_info->regs_bmp & (1<<i)) {
			ret = i2c_smbus_write_word_data(client, i,
						chip->conf_regs[i]);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

/**
 *  cm3218x_read_als_it() - Get sensor integration time (ms)
 *  @chip:	pointer of struct cm3218x
 *  @val2:	pointer of int to load the als_it value.
 *
 *  Report the current integration time in milliseconds.
 *
 *  Return: IIO_VAL_INT_PLUS_MICRO for success, otherwise -EINVAL.
 */
static int cm3218x_read_als_it(struct cm3218x_chip *chip, int *val2)
{
	struct cm3218x_als_info *als_info = chip->als_info;
	u16 als_it;
	int i;

	als_it = chip->conf_regs[CM3218X_REG_ADDR_CMD];
	als_it &= CM3218X_CMD_ALS_IT_MASK;
	als_it >>= CM3218X_CMD_ALS_IT_SHIFT;
	for (i = 0; i < als_info->num_als_it; i++) {
		if (als_it == als_info->als_it_bits[i]) {
			*val2 = als_info->als_it_values[i];
			return IIO_VAL_INT_PLUS_MICRO;
		}
	}

	return -EINVAL;
}

/**
 * cm3218x_write_als_it() - Write sensor integration time
 * @chip:	pointer of struct cm3218x.
 * @val:	integration time in milliseconds.
 *
 * Convert integration time (ms) to sensor value.
 *
 * Return: i2c_smbus_write_word_data command return value.
 */
static int cm3218x_write_als_it(struct cm3218x_chip *chip, int val)
{
	struct i2c_client *client = chip->client;
	struct cm3218x_als_info *als_info = chip->als_info;
	u16 als_it;
	int ret, i;

	for (i = 0; i < als_info->num_als_it; i++)
		if (val <= als_info->als_it_values[i])
			break;
	if (i >= als_info->num_als_it)
		i = als_info->num_als_it - 1;

	als_it = als_info->als_it_bits[i];
	als_it <<= CM3218X_CMD_ALS_IT_SHIFT;

	mutex_lock(&chip->lock);
	chip->conf_regs[CM3218X_REG_ADDR_CMD] &=
			~CM3218X_CMD_ALS_IT_MASK;
	chip->conf_regs[CM3218X_REG_ADDR_CMD] |=
			als_it;
	ret = i2c_smbus_write_word_data(client, CM3218X_REG_ADDR_CMD,
			chip->conf_regs[CM3218X_REG_ADDR_CMD]);
	mutex_unlock(&chip->lock);

	return ret;
}

/**
 * cm3218x_get_lux() - report current lux value
 * @chip:	pointer of struct cm3218x.
 *
 * Convert sensor raw data to lux.  It depends on integration
 * time and calibscale variable.
 *
 * Return: Positive value is lux, otherwise is error code.
 */
static int cm3218x_get_lux(struct cm3218x_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct cm3218x_als_info *als_info = chip->als_info;
	int ret;
	int als_it;
	u64 tmp;

	/* Calculate mlux per bit based on als_it */
	ret = cm3218x_read_als_it(chip, &als_it);
	if (ret < 0)
		return -EINVAL;
	tmp = (__force u64)als_info->mlux_per_bit;
	tmp *= als_info->mlux_per_bit_base_it;
	tmp = div_u64(tmp, als_it);

	/* Get als_raw */
	if (!(chip->conf_regs[CM3218X_REG_ADDR_CMD] & CM3218X_CMD_ALS_INT_EN))
		als_info->als_raw = i2c_smbus_read_word_data(
					client,
					CM3218X_REG_ADDR_ALS);
	if (als_info->als_raw < 0)
		return als_info->als_raw;

	tmp *= als_info->als_raw;
	tmp *= als_info->calibscale;
	tmp = div_u64(tmp, CM3218X_CALIBSCALE_RESOLUTION);
	tmp = div_u64(tmp, CM3218X_MLUX_PER_LUX);

	if (tmp > 0xFFFF)
		tmp = 0xFFFF;

	return (int)tmp;
}

static int cm3218x_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long mask)
{
	struct cm3218x_chip *chip = iio_priv(indio_dev);
	struct cm3218x_als_info *als_info = chip->als_info;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		ret = cm3218x_get_lux(chip);
		if (ret < 0)
			return ret;
		*val = ret;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_CALIBSCALE:
		*val = als_info->calibscale;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_INT_TIME:
		*val = 0;
		ret = cm3218x_read_als_it(chip, val2);
		return ret;
	}

	return -EINVAL;
}

static int cm3218x_write_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int val, int val2, long mask)
{
	struct cm3218x_chip *chip = iio_priv(indio_dev);
	struct cm3218x_als_info *als_info = chip->als_info;
	long ms;

	switch (mask) {
	case IIO_CHAN_INFO_CALIBSCALE:
		als_info->calibscale = val;
		return val;
	case IIO_CHAN_INFO_INT_TIME:
		ms = val * 1000000 + val2;
		return cm3218x_write_als_it(chip, (int)ms);
	}

	return -EINVAL;
}

/**
 * cm3218x_get_it_available() - Get available ALS IT value
 * @dev:	pointer of struct device.
 * @attr:	pointer of struct device_attribute.
 * @buf:	pointer of return string buffer.
 *
 * Display the available integration time in milliseconds.
 *
 * Return: string length.
 */
static ssize_t cm3218x_get_it_available(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct cm3218x_chip *chip = iio_priv(dev_to_iio_dev(dev));
	struct cm3218x_als_info *als_info = chip->als_info;
	int i, len;

	for (i = 0, len = 0; i < als_info->num_als_it; i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "%u.%06u ",
			als_info->als_it_values[i]/1000000,
			als_info->als_it_values[i]%1000000);
	return len + scnprintf(buf + len, PAGE_SIZE - len, "\n");
}

/**
 * cm3218x_threshold_update() - Update the threshold registers.
 * @chip:	pointer of struct cm3218x_chip.
 * @percent:	+/- percent.
 *
 * Based on the current ALS value, update the hi and low threshold registers.
 *
 * Return: 0 for success; otherwise for error code.
 */
static int cm3218x_threshold_update(struct cm3218x_chip *chip, int percent)
{
	struct i2c_client *client = chip->client;
	struct cm3218x_als_info *als_info = chip->als_info;
	int ret;
	int wh, wl;

	ret = als_info->als_raw = i2c_smbus_read_word_data(client,
					CM3218X_REG_ADDR_ALS);
	if (ret < 0)
		return ret;

	wh = wl = ret;
	ret *= percent;
	ret /= 100;
	if (ret < 1)
		ret = 1;
	wh += ret;
	wl -= ret;
	if (wh > 65535)
		wh = 65535;
	if (wl < 0)
		wl = 0;

	chip->conf_regs[CM3218X_REG_ADDR_WH] = wh;
	ret = i2c_smbus_write_word_data(
		client,
		CM3218X_REG_ADDR_WH,
		chip->conf_regs[CM3218X_REG_ADDR_WH]);
	if (ret < 0)
		return ret;

	chip->conf_regs[CM3218X_REG_ADDR_WL] = wl;
	ret = i2c_smbus_write_word_data(
		client,
		CM3218X_REG_ADDR_WL,
		chip->conf_regs[CM3218X_REG_ADDR_WL]);

	return ret;
}

/**
 * cm3218x_event_handler() - Interrupt handling routine.
 * @irq:	irq number.
 * @private:	pointer of void.
 *
 * Clean interrupt and reset threshold registers.
 *
 * Return: IRQ_HANDLED.
 */
static irqreturn_t cm3218x_event_handler(int irq, void *private)
{
	struct iio_dev *dev_info = private;
	struct cm3218x_chip *chip = iio_priv(dev_info);
	int ret;

	mutex_lock(&chip->lock);

	/* Disable interrupt */
	ret = cm3218x_interrupt_config(chip, 0);
	if (ret < 0)
		goto error_handler_unlock;

	/* Update Hi/Lo windows */
	ret = cm3218x_threshold_update(chip, CM3218X_THRESHOLD_PERCENT);
	if (ret < 0)
		goto error_handler_unlock;

	/* Enable interrupt */
	cm3218x_interrupt_config(chip, 1);

error_handler_unlock:
	mutex_unlock(&chip->lock);
	return IRQ_HANDLED;
}

static const struct iio_chan_spec cm3218x_channels[] = {
	{
		.type = IIO_LIGHT,
		.info_mask_separate =
			BIT(IIO_CHAN_INFO_PROCESSED) |
			BIT(IIO_CHAN_INFO_CALIBSCALE) |
			BIT(IIO_CHAN_INFO_INT_TIME),
	}
};

static IIO_DEVICE_ATTR(in_illuminance_integration_time_available,
			S_IRUGO, cm3218x_get_it_available, NULL, 0);

static struct attribute *cm3218x_attributes[] = {
	&iio_dev_attr_in_illuminance_integration_time_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group cm3218x_attribute_group = {
	.attrs = cm3218x_attributes
};

static const struct iio_info cm3218x_info = {
	.driver_module		= THIS_MODULE,
	.read_raw		= &cm3218x_read_raw,
	.write_raw		= &cm3218x_write_raw,
	.attrs			= &cm3218x_attribute_group,
};

static int cm3218x_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct cm3218x_chip *chip;
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
	indio_dev->channels = cm3218x_channels;
	indio_dev->num_channels = ARRAY_SIZE(cm3218x_channels);
	indio_dev->info = &cm3218x_info;
	if (id && id->name)
		indio_dev->name = id->name;
	else
		indio_dev->name = (char *)dev_name(&client->dev);
	indio_dev->modes = INDIO_DIRECT_MODE;

	ret = cm3218x_reg_init(chip);
	if (ret) {
		dev_err(&client->dev,
			"%s: register init failed\n",
			__func__);
		return ret;
	}

	if (client->irq) {
		ret = request_threaded_irq(client->irq,
					NULL,
					cm3218x_event_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"cm3218x_event",
					indio_dev);

		if (ret < 0) {
			dev_err(&client->dev, "irq request error %d\n",
				-ret);
			goto error_disable_int;
		}
	}

	ret = iio_device_register(indio_dev);
	if (ret < 0) {
		dev_err(&client->dev,
			"%s: regist device failed\n",
			__func__);
		goto error_free_irq;
	}

	if (client->irq) {
		ret = cm3218x_threshold_update(chip, CM3218X_THRESHOLD_PERCENT);
		if (ret < 0)
			goto error_free_irq;

		ret = cm3218x_interrupt_config(chip, 1);
		if (ret < 0)
			goto error_free_irq;
	}

	return 0;

error_free_irq:
	if (client->irq)
		free_irq(client->irq, indio_dev);
error_disable_int:
	cm3218x_interrupt_config(chip, 0);
	return ret;
}

static int cm3218x_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct cm3218x_chip *chip = iio_priv(indio_dev);

	cm3218x_interrupt_config(chip, 0);
	if (client->irq)
		free_irq(client->irq, indio_dev);
	iio_device_unregister(indio_dev);
	return 0;
}

static const struct i2c_device_id cm3218x_id[] = {
	{ "cm3218", cm3218},
	{ "cm32181", cm32181},
	{ "cm32182", cm32182},
	{ }
};

MODULE_DEVICE_TABLE(i2c, cm3218x_id);

static const struct of_device_id cm3218x_of_match[] = {
	{ .compatible = "capella,cm3218" },
	{ }
};

#ifdef CONFIG_ACPI
static const struct acpi_device_id cm3218x_acpi_match[] = {
	{ "CPLM3218", 0},
	{},
};

MODULE_DEVICE_TABLE(acpi, cm3218x_acpi_match);
#endif /* CONFIG_ACPI */

static struct i2c_driver cm3218x_driver = {
	.driver = {
		.name	= "cm3218x",
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(cm3218x_acpi_match),
#endif /* CONFIG_ACPI */
		.of_match_table = of_match_ptr(cm3218x_of_match),
		.owner	= THIS_MODULE,
	},
	.id_table	= cm3218x_id,
	.probe		= cm3218x_probe,
	.remove		= cm3218x_remove,
};

module_i2c_driver(cm3218x_driver);

MODULE_AUTHOR("Kevin Tsai <ktsai@capellamicro.com>");
MODULE_DESCRIPTION("CM3218X ambient light sensor driver");
MODULE_LICENSE("GPL");
