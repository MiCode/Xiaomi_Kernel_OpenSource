/* drivers/input/misc/ots_pat9125/pat9125_linux_driver.c
 *
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/input.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include "pixart_ots.h"

struct pixart_pat9125_data {
	struct i2c_client *client;
	struct input_dev *input;
	int irq_gpio;
	u32 irq_flags;
};

static int pat9125_i2c_write(struct i2c_client *client, u8 reg, u8 *data,
		int len)
{
	u8 buf[MAX_BUF_SIZE];
	int ret = 0, i;
	struct device *dev = &client->dev;

	buf[0] = reg;
	if (len >= MAX_BUF_SIZE) {
		dev_err(dev, "%s Failed: buffer size is %d [Max Limit is %d]\n",
			__func__, len, MAX_BUF_SIZE);
		return -ENODEV;
	}
	for (i = 0 ; i < len; i++)
		buf[i+1] = data[i];
	/* Returns negative errno, or else the number of bytes written. */
	ret = i2c_master_send(client, buf, len+1);
	if (ret != len+1)
		dev_err(dev, "%s Failed: writing to reg 0x%x\n", __func__, reg);

	return ret;
}

static int pat9125_i2c_read(struct i2c_client *client, u8 reg, u8 *data)
{
	u8 buf[MAX_BUF_SIZE];
	int ret;
	struct device *dev = &client->dev;

	buf[0] = reg;
	/*
	 * If everything went ok (1 msg transmitted), return #bytes transmitted,
	 * else error code. thus if transmit is ok return value 1
	 */
	ret = i2c_master_send(client, buf, 1);
	if (ret != 1) {
		dev_err(dev, "%s Failed: writing to reg 0x%x\n", __func__, reg);
		return ret;
	}
	/* returns negative errno, or else the number of bytes read */
	ret = i2c_master_recv(client, buf, 1);
	if (ret != 1) {
		dev_err(dev, "%s Failed: reading reg 0x%x\n", __func__, reg);
		return ret;
	}
	*data = buf[0];

	return ret;
}

unsigned char read_data(struct i2c_client *client, u8 addr)
{
	u8 data = 0xff;

	pat9125_i2c_read(client, addr, &data);
	return data;
}

void write_data(struct i2c_client *client, u8 addr, u8 data)
{
	pat9125_i2c_write(client, addr, &data, 1);
}

static irqreturn_t pixart_pat9125_irq(int irq, void *data)
{
	return IRQ_HANDLED;
}

static ssize_t pat9125_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char s[256], *p = s;
	int reg_data = 0, i;
	long rd_addr, wr_addr, wr_data;
	struct pixart_pat9125_data *data =
		(struct pixart_pat9125_data *)dev->driver_data;
	struct i2c_client *client = data->client;

	for (i = 0; i < sizeof(s); i++)
		s[i] = buf[i];
	*(s+1) = '\0';
	*(s+4) = '\0';
	*(s+7) = '\0';
	/* example(in console): echo w 12 34 > rw_reg */
	if (*p == 'w') {
		p += 2;
		if (!kstrtol(p, 16, &wr_addr)) {
			p += 3;
			if (!kstrtol(p, 16, &wr_data)) {
				dev_dbg(dev, "w 0x%x 0x%x\n",
					(u8)wr_addr, (u8)wr_data);
				write_data(client, (u8)wr_addr, (u8)wr_data);
			}
		}
	}
	/* example(in console): echo r 12 > rw_reg */
	else if (*p == 'r') {
		p += 2;

		if (!kstrtol(p, 16, &rd_addr)) {
			reg_data = read_data(client, (u8)rd_addr);
			dev_dbg(dev, "r 0x%x 0x%x\n",
				(unsigned int)rd_addr, reg_data);
		}
	}
	return count;
}

static ssize_t pat9125_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}
static DEVICE_ATTR(test, S_IRUGO | S_IWUSR | S_IWGRP,
		pat9125_test_show, pat9125_test_store);

static struct attribute *pat9125_attr_list[] = {
	&dev_attr_test.attr,
	NULL,
};

static struct attribute_group pat9125_attr_grp = {
	.attrs = pat9125_attr_list,
};

static int pat9125_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;
	struct pixart_pat9125_data *data;
	struct input_dev *input;
	struct device_node *np;
	struct device *dev = &client->dev;

	err = i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE);
	if (err < 0) {
		dev_err(dev, "I2C not supported\n");
		return -ENXIO;
	}

	if (client->dev.of_node) {
		data = devm_kzalloc(dev, sizeof(struct pixart_pat9125_data),
				GFP_KERNEL);
		if (!data)
			return -ENOMEM;
	} else {
		data = client->dev.platform_data;
		if (!data) {
			dev_err(dev, "Invalid pat9125 data\n");
			return -EINVAL;
		}
	}
	data->client = client;

	input = devm_input_allocate_device(dev);
	if (!input) {
		dev_err(dev, "Failed to alloc input device\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(client, data);
	input_set_drvdata(input, data);
	input->name = PAT9125_DEV_NAME;

	data->input = input;
	err = input_register_device(data->input);
	if (err < 0) {
		dev_err(dev, "Failed to register input device\n");
		goto err_register_input_device;
	}

	if (!gpio_is_valid(data->irq_gpio)) {
		dev_err(dev, "invalid irq_gpio: %d\n", data->irq_gpio);
		return -EINVAL;
	}

	err = gpio_request(data->irq_gpio, "pixart_pat9125_irq_gpio");
	if (err) {
		dev_err(dev, "unable to request gpio %d\n", data->irq_gpio);
		return err;
	}

	err = gpio_direction_input(data->irq_gpio);
	if (err) {
		dev_err(dev, "unable to set dir for gpio %d\n", data->irq_gpio);
		goto free_gpio;
	}

	if (!ots_sensor_init(client)) {
		err = -ENODEV;
		goto err_sensor_init;
	}

	err = devm_request_threaded_irq(dev, client->irq, NULL,
			pixart_pat9125_irq, (unsigned long)data->irq_flags,
			"pixart_pat9125_irq", data);
	if (err) {
		dev_err(dev, "Req irq %d failed, errno:%d\n", client->irq, err);
		goto err_request_threaded_irq;
	}

	err = sysfs_create_group(&(input->dev.kobj), &pat9125_attr_grp);
	if (err) {
		dev_err(dev, "Failed to create sysfs group, errno:%d\n", err);
		goto err_sysfs_create;
	}

	return 0;

err_sysfs_create:
err_request_threaded_irq:
err_sensor_init:
free_gpio:
	gpio_free(data->irq_gpio);
err_register_input_device:
	input_free_device(data->input);
	return err;
}

static int pat9125_i2c_remove(struct i2c_client *client)
{
	struct pixart_pat9125_data *data = i2c_get_clientdata(client);

	devm_free_irq(&client->dev, client->irq, data);
	if (gpio_is_valid(data->irq_gpio))
		gpio_free(data->irq_gpio);
	input_unregister_device(data->input);
	devm_kfree(&client->dev, data);
	data = NULL;
	return 0;
}

static int pat9125_suspend(struct device *dev)
{
	return 0;
}

static int pat9125_resume(struct device *dev)
{
	return 0;
}

static const struct i2c_device_id pat9125_device_id[] = {
	{PAT9125_DEV_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, pat9125_device_id);

static const struct dev_pm_ops pat9125_pm_ops = {
	.suspend = pat9125_suspend,
	.resume = pat9125_resume
};

static const struct of_device_id pixart_pat9125_match_table[] = {
	{ .compatible = "pixart,pat9125",},
	{ },
};

static struct i2c_driver pat9125_i2c_driver = {
	.driver = {
		   .name = PAT9125_DEV_NAME,
		   .owner = THIS_MODULE,
		   .pm = &pat9125_pm_ops,
		   .of_match_table = pixart_pat9125_match_table,
		   },
	.probe = pat9125_i2c_probe,
	.remove = pat9125_i2c_remove,
	.id_table = pat9125_device_id,
};
module_i2c_driver(pat9125_i2c_driver);

MODULE_AUTHOR("pixart");
MODULE_DESCRIPTION("pixart pat9125 driver");
MODULE_LICENSE("GPL");
