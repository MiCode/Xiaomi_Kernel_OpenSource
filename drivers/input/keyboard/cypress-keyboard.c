/*
 * Cypress keyboard driver
 *
 * Copyright (c) 2014, Intel Corporation.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/acpi.h>

/*registers*/
#define SENSOR_EN	0x0
#define		SENSOR_CS0	0
#define		SENSOR_CS1	1
#define BUTTON_STAT	0xAA
#define		BUTTON_ACTIVE	1

struct cy_kb {
	struct mutex mutex;
	struct i2c_client *client;
	struct input_dev *input_dev;
	int gpio;
	unsigned int irq;
	u16 button_status;
	bool suspended;
};

static int cy_kb_read(struct cy_kb *data,
			u16 reg, u16 len, void *buf)
{
	int ret;
	struct i2c_msg xfer[2];
	u8 regbuf[2];
	bool retry = false;

	regbuf[0] = reg & 0xff;
	regbuf[1] = (reg >> 8) & 0xff;

	xfer[0].addr = data->client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 2;
	xfer[0].buf = regbuf;

	xfer[1].addr = data->client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = buf;

retry_read:
	ret = i2c_transfer(data->client->adapter, xfer, ARRAY_SIZE(xfer));
	if (ret != ARRAY_SIZE(xfer)) {
		if (!retry) {
			dev_dbg(&data->client->dev, "i2c read retry\n");
			retry = true;
			goto retry_read;
		} else {
			dev_err(&data->client->dev, "i2c transfer failed (%d)\n",
				ret);
			return -EIO;
		}
	}
	return 0;
}

static int cy_kb_write(struct cy_kb *data,
			u16 reg, u16 val)
{
	int ret;
	int count;
	u8 *buf;
	bool retry = false;

	count = sizeof(val) + 2;
	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;
	memcpy(&buf[2], &val, sizeof(val));

retry_write:
	ret = i2c_master_send(data->client, buf, count);
	if (ret != count) {
		if (!retry) {
			dev_dbg(&data->client->dev, "i2c write retry\n");
			retry = true;
			goto retry_write;
		} else {
			dev_err(&data->client->dev, "i2c send failed (%d)\n",
				ret);
			ret = -EIO;
		}
	} else {
		ret = 0;
	}

	kfree(buf);
	return ret;
}

static irqreturn_t cy_kb_interrupt(int irq, void *dev_id)
{
	int ret;
	struct cy_kb *data = dev_id;
	u8 buf = 0x0;

	mutex_lock(&data->mutex);

	ret = cy_kb_read(data, BUTTON_STAT, sizeof(buf), &buf);
	if (ret) {
		dev_err(&data->client->dev, "can't read button status\n");
		goto out;
	}
	dev_dbg(&data->client->dev, "Button status 0x%02x\n", buf);

	if ((buf ^ data->button_status) & (BUTTON_ACTIVE << SENSOR_CS0)) {
		input_event(data->input_dev, EV_KEY, KEY_BACK,
			(buf >> SENSOR_CS0) & BUTTON_ACTIVE);
	}

	if ((buf ^ data->button_status) & (BUTTON_ACTIVE << SENSOR_CS1)) {
		input_event(data->input_dev, EV_KEY, KEY_MENU,
			(buf >> SENSOR_CS1) & BUTTON_ACTIVE);
	}

	input_sync(data->input_dev);
	data->button_status = buf;
out:
	mutex_unlock(&data->mutex);
	return IRQ_HANDLED;
}

static int cy_kb_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	struct cy_kb *data;
	struct gpio_desc *gpio;

	data = kzalloc(sizeof(struct cy_kb), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Fail to allocate memory\n");
		return -ENOMEM;
	}
	i2c_set_clientdata(client, data);
	mutex_init(&data->mutex);
	data->client = client;

	gpio = devm_gpiod_get_index(&client->dev, "cypress_gpio_int", 0);
	if (!IS_ERR(gpio)) {
		data->gpio = desc_to_gpio(gpio);
		data->irq = gpiod_to_irq(gpio_to_desc(data->gpio));
	} else
		dev_err(&client->dev, "Fail to get gpio interrupt\n");

	ret = request_threaded_irq(data->irq, NULL, cy_kb_interrupt,
				     IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				     client->name, data);
	if (ret) {
		dev_err(&client->dev, "Fail to register irq\n");
		goto err_free_irq;
	}

	data->input_dev = input_allocate_device();
	if (!data->input_dev) {
		dev_err(&client->dev, "Fail to allocate input device\n");
		ret = -ENOMEM;
		goto err_alloc_input;
	}
	data->input_dev->name = client->name;
	data->input_dev->id.bustype = BUS_I2C;
	data->input_dev->dev.parent = &data->client->dev;
	input_set_capability(data->input_dev, EV_KEY, KEY_BACK);
	input_set_capability(data->input_dev, EV_KEY, KEY_MENU);
	ret = input_register_device(data->input_dev);
	if (ret) {
		dev_err(&client->dev, "Fail to register input device\n");
		goto err_reg_input;
	}

	return 0;

err_reg_input:
	input_free_device(data->input_dev);
err_alloc_input:
	free_irq(data->irq, data);
	gpio_free(data->gpio);
err_free_irq:
	kfree(data);
	return ret;
}

static int cy_kb_remove(struct i2c_client *client)
{
	struct cy_kb *data = i2c_get_clientdata(client);

	free_irq(data->irq, data);
	gpio_free(data->gpio);
	input_unregister_device(data->input_dev);
	kfree(data);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int cy_kb_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cy_kb *data = i2c_get_clientdata(client);

	mutex_lock(&data->mutex);
	if (!data->suspended)
		cy_kb_write(data, SENSOR_EN, 0);
	data->suspended = true;
	mutex_unlock(&data->mutex);

	return 0;
}

static int cy_kb_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cy_kb *data = i2c_get_clientdata(client);

	mutex_lock(&data->mutex);
	if (data->suspended)
		cy_kb_write(data, SENSOR_EN, SENSOR_CS0 | SENSOR_CS1);
	data->suspended = false;
	mutex_unlock(&data->mutex);

	return 0;
}
static SIMPLE_DEV_PM_OPS(cy_kb_pm_ops, cy_kb_suspend, cy_kb_resume);
#endif

static const struct i2c_device_id cy_kb_id[] = {
	{ "CY8MBCR3108:00", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cy_kb_id);

#ifdef CONFIG_ACPI
static struct acpi_device_id cy_kb_acpi_id[] = {
	{ "CY8MBCR3108", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, cy_kb_acpi_id);
#endif

static struct i2c_driver cy_kb_driver = {
	.driver = {
		.name	= "cypress keyboard",
		.owner	= THIS_MODULE,
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(cy_kb_acpi_id),
#endif
#ifdef CONFIG_PM_SLEEP
		.pm = &cy_kb_pm_ops,
#endif
	},
	.probe	= cy_kb_probe,
	.remove	= cy_kb_remove,
	.id_table = cy_kb_id,
};

static int __init cy_kb_init(void)
{
	return i2c_add_driver(&cy_kb_driver);
}

static void __exit cy_kb_exit(void)
{
	i2c_del_driver(&cy_kb_driver);
}

module_init(cy_kb_init);
module_exit(cy_kb_exit);

MODULE_AUTHOR("EIG IO team");
MODULE_DESCRIPTION("cy8cmbr3108 driver");
MODULE_LICENSE("GPL v2");
