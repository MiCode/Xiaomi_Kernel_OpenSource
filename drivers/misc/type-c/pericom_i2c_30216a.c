/*
 * PERICOM 30216A driver
 * Copyright (C) 2016 xiaomi Incorporated
 *
 * Copyright (C) 2012 fengwei <fengwei@xiaomi.com>
 * Copyright (c) 2015-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>

#include "pericom_i2c_30216a.h"
#include "typec_class.h"

#define DRIVER_NAME "pericom_30216a"

/*
   pericom data struct
  */
struct pericom_30216a_data{
	struct i2c_client *i2c_client;
	int irq;
	struct mutex i2c_rw_mutex;

	struct typec_dev c_dev;
};

 /**
 * pericom_30216a_i2c_read()
 *
 * Called by various functions in this driver,
 * This function reads data of an arbitrary length from the sensor,
 * starting from an assigned register address of the sensor, via I2C
 * with a retry mechanism.
 */
static int pericom_30216a_i2c_read(struct pericom_30216a_data *pericom_data,
		unsigned char *data, unsigned short length)
{
	int retval;
	unsigned char retry;
	struct i2c_msg msg[] = {
		{
			.addr = pericom_data->i2c_client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = data,
		},
	};

	mutex_lock(&(pericom_data->i2c_rw_mutex));

	for (retry = 0; retry < PERICOM_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(pericom_data->i2c_client->adapter, msg, 1) > 0) {
			retval = length;
			break;
		}
		dev_err(&pericom_data->i2c_client->dev,
				"%s: I2C retry %d\n", __func__, retry + 1);
		msleep(20);
	}

	if (retry == PERICOM_I2C_RETRY_TIMES) {
		dev_err(&pericom_data->i2c_client->dev,
				"%s: I2C read over retry limit\n", __func__);
		retval = -EIO;
	}

	mutex_unlock(&(pericom_data->i2c_rw_mutex));

	return retval;
}

 /**
 * pericom_30216a_i2c_write()
 *
 * Called by various functions in this driver
 *
 * This function writes data of an arbitrary length to the sensor,
 * starting from an assigned register address of the sensor, via I2C with
 * a retry mechanism.
 */
static int pericom_30216a_i2c_write(struct pericom_30216a_data *pericom_data,
		unsigned char *data, unsigned short length)
{
	int retval;
	unsigned char retry;
	struct i2c_msg msg[] = {
		{
			.addr = pericom_data->i2c_client->addr,
			.flags = 0,
			.len = length ,
			.buf = data,
		}
	};

	mutex_lock(&(pericom_data->i2c_rw_mutex));

	for (retry = 0; retry < PERICOM_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(pericom_data->i2c_client->adapter, msg, 1) == 1) {
			retval = length;
			break;
		}
		dev_err(&pericom_data->i2c_client->dev,
				"%s: I2C retry %d\n",
				__func__, retry + 1);
		msleep(20);
	}

	if (retry == PERICOM_I2C_RETRY_TIMES) {
		dev_err(&pericom_data->i2c_client->dev,
				"%s: I2C write over retry limit\n",
				__func__);
		retval = -EIO;
	}

	mutex_unlock(&(pericom_data->i2c_rw_mutex));

	return retval;
}

/*set mode*/
static int pericom_30216a_set_power_mode(struct pericom_30216a_data *pericom_data, enum pericom_power_mode mode)
{
	int ret;
	char buf[2] = {0, 0};

	pericom_30216a_i2c_read(pericom_data, buf, 2);
	buf[1] = (buf[1] & ~PERICOM_POWER_SAVING_MASK) | ((mode << PERICOM_POWER_SAVING_OFFSET) & PERICOM_POWER_SAVING_MASK);
	buf[1] &= ~PERICOM_INTERRUPT_MASK;

	ret = pericom_30216a_i2c_write(pericom_data, buf, 2);
	return ret;
}

static int pericom_30216a_set_role_mode(struct pericom_30216a_data *pericom_data, enum pericom_role_mode mode)
{
	int ret, i;
	char buf[4] = {0, 0, 0, 0};

	pericom_30216a_i2c_read(pericom_data, buf, 2);
	buf[1] = (buf[1] & ~PERICOM_ROLE_MODE_MASK)
			|((mode << PERICOM_ROLE_OFFSET) & PERICOM_ROLE_MODE_MASK);

	/* mask interrupt */
	buf[1] |= PERICOM_INTERRUPT_MASK;

	ret = pericom_30216a_i2c_write(pericom_data, buf, 2);

	/* sleep and wait for correct status */
	for (i = 0; i <= 5; i++) {
		msleep(1500);
		pericom_30216a_i2c_read(pericom_data, buf, 4);
		dev_info(&pericom_data->i2c_client->dev,
				"read:%d,%d,%d,%d\n", buf[0], buf[1], buf[2], buf[3]);
		/* mode=0 : device, reg4 must be 0x08
		  * mode=1: host , reg4 must be 0x04
		  * mode=2: drp,  all value is ok
		  */
		if (((mode == DEVICE_MODE) && (buf[3] & 0x08))
			|| ((mode == HOST_MODE) && (buf[3] & 0x04))
			|| (mode == DRP_MODE))
			break;
	}

	if (i > 5)
		dev_info(&pericom_data->i2c_client->dev, "try to %d mode fail \n", mode);
	/* unmask interrupt*/
	buf[1] &= ~PERICOM_INTERRUPT_MASK;
	ret = pericom_30216a_i2c_write(pericom_data, buf, 2);

	return ret;
}
/*set device mode*/
static int pericom_30216a_set_device_mode(struct pericom_30216a_data *pericom_data)
{
	return pericom_30216a_set_role_mode(pericom_data, DEVICE_MODE);
}

/* set host mode
  * success if return positive value ,or return negative
  */
static int pericom_30216a_set_host_mode(struct pericom_30216a_data *pericom_data)
{
	return pericom_30216a_set_role_mode(pericom_data, HOST_MODE);
}

/* set DRP mode
  * success if return positive value ,or return negative
  */
static int pericom_30216a_set_drp_mode(struct pericom_30216a_data *pericom_data)
{
	return pericom_30216a_set_role_mode(pericom_data, DRP_MODE);
}

/* get role mode
  * success if return positive value ,or return negative
  */
static int pericom_30216a_get_role_mode(struct pericom_30216a_data *pericom_data)
{
	int ret;
	char buf[2] = {0, 0};

	ret = pericom_30216a_i2c_read(pericom_data, buf, 2);
	buf[1] = (buf[1] & PERICOM_ROLE_MODE_MASK) >> PERICOM_ROLE_OFFSET;

	return ret > 0 ? buf[1]:ret;
}

/* set power saving mode
  * success if return positive value ,or return negative
  */
static int pericom_30216a_set_powersaving_mode(struct pericom_30216a_data *pericom_data)
{
	return pericom_30216a_set_power_mode(pericom_data, POWERSAVING_MODE);
}

/* set active mode
  * success if return positive value ,or return negative
  */
static int pericom_30216a_set_poweractive_mode(struct pericom_30216a_data *pericom_data)
{
	return pericom_30216a_set_power_mode(pericom_data, ACTIVE_MODE);
}


static int pericom_30216a_get_id_status(struct typec_dev *dev)
{
	struct pericom_30216a_data *pericom_data =
		container_of(dev, struct pericom_30216a_data, c_dev);

	return pericom_30216a_get_role_mode(pericom_data);
}

static int pericom_30216a_set_id_status(struct typec_dev *dev, int value)
{
	int rc = -1;
	struct pericom_30216a_data *pericom_data =
		container_of(dev, struct pericom_30216a_data, c_dev);

	if (value == DEVICE_MODE)
		rc = pericom_30216a_set_device_mode(pericom_data);
	else if (value == HOST_MODE)
		rc = pericom_30216a_set_host_mode(pericom_data);
	else if (value == DRP_MODE)
		rc = pericom_30216a_set_drp_mode(pericom_data);

	return rc;
}

/* get cc  orientation
  * success if return positive value ,or return 0
  * return 1 means cc1
  * return 2 means cc2.
  */
static int pericom_30216a_get_cc_orientation(struct pericom_30216a_data *pericom_data)
{
	int ret;
	char buf[4] = {0, 0, 0, 0};

	ret = pericom_30216a_i2c_read(pericom_data, buf, 4);
	buf[3] = buf[3] & PERICOM_CC_ORI_MASK;

	return ret > 0 ? buf[3]:0;
}

static int pericom_30216a_get_cc_pin(struct typec_dev *dev)
{
	struct pericom_30216a_data *pericom_data =
		container_of(dev, struct pericom_30216a_data, c_dev);

	return pericom_30216a_get_cc_orientation(pericom_data);
}
static bool ic_is_present(struct pericom_30216a_data *pericom_data)
{
	int ret;
	char buf;

	ret = pericom_30216a_i2c_read(pericom_data, &buf, 1);

	return (ret < 0) ? false:true;
}

static irqreturn_t pericom_30216a_irq_handler(int irq, void *dev_id)
{
	struct pericom_30216a_data *pericom_data = (struct pericom_30216a_data *) dev_id;
	char reg[4] = {0, 0, 0, 0};
	char curr_mode;
	static bool  plug_flag;

	pericom_30216a_i2c_read(pericom_data,  reg, 2);
	dev_info(&pericom_data->i2c_client->dev, "0.reg=%x,%x,%x,%x\n", reg[0], reg[1], reg[2], reg[3]);
	curr_mode = reg[1] & PERICOM_ROLE_MODE_MASK;
	reg[1] = reg[1] | PERICOM_INTERRUPT_MASK;
	pericom_30216a_i2c_write(pericom_data, reg, 2);

	msleep(30);

	pericom_30216a_i2c_read(pericom_data,  reg, 4);
	dev_info(&pericom_data->i2c_client->dev, "2.reg=%x,%x,%x,%x\n", reg[0], reg[1], reg[2], reg[3]);

	if ((reg[2] == 0x2) || (reg[3] == 0x00) || (reg[3] == 0x80)) {
		if ((reg[2] == 0x2) && (reg[3] == 0x00))
			plug_flag = false;
		curr_mode = DRP_MODE << PERICOM_ROLE_OFFSET;
	} else if (reg[3] == 0x04) {
		reg[1] = PERICOM_INTERRUPT_MASK;
		pericom_30216a_i2c_write(pericom_data, reg, 2);
		curr_mode = DRP_MODE << PERICOM_ROLE_OFFSET;
	} else if (reg[3] == 0x97) {
		msleep(350);
		reg[1] = PERICOM_INTERRUPT_MASK;
		pericom_30216a_i2c_write(pericom_data, reg, 2);
		curr_mode = DRP_MODE << PERICOM_ROLE_OFFSET;
	} else {
		if ((reg[3] == 0x05) || (reg[3] == 0x06)) {
			if (plug_flag == false) {
				plug_flag = true;
				reg[1] = (DEVICE_MODE << PERICOM_ROLE_OFFSET) | PERICOM_INTERRUPT_MASK;
				pericom_30216a_i2c_write(pericom_data, reg, 2);

				msleep(500);
				pericom_30216a_i2c_read(pericom_data, reg, 4);
				dev_info(&pericom_data->i2c_client->dev, "3.reg=%x,%x,%x,%x\n", reg[0], reg[1], reg[2], reg[3]);
				if (reg[3] & 0x08)
					curr_mode = DEVICE_MODE << PERICOM_ROLE_OFFSET;
				else
					curr_mode = DRP_MODE << PERICOM_ROLE_OFFSET;
			}
		}
	}
	msleep(20);
	reg[1] = curr_mode;
	pericom_30216a_i2c_write(pericom_data, reg, 2);


	return IRQ_HANDLED;
}

static int pericom_30216a_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
{
	int retval = 0;
	struct regulator *i2c_vdd;
	struct pericom_30216a_data *pericom_data = client->dev.platform_data;

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev,
				"%s: SMBus byte data not supported\n",
				__func__);
		return -EIO;
	}

	if (client->dev.of_node) {
		pericom_data = devm_kzalloc(&client->dev,
			sizeof(*pericom_data),
			GFP_KERNEL);
		if (!pericom_data) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
	}

	if (!pericom_data) {
		dev_err(&client->dev,
				"%s: No platform data found\n",
				__func__);
		return -EINVAL;
	}

	i2c_vdd = regulator_get(&client->dev, "i2c");
	if (IS_ERR(i2c_vdd)) {
		dev_err(&client->dev,	"Regulator get failed i2c ret=%ld\n", PTR_ERR(i2c_vdd));
		goto  err_regulator;
	}

	retval = regulator_enable(i2c_vdd);
	if (retval) {
		dev_err(&client->dev, "Regulator set_vtg failed i2c ret=%d\n", retval);
		goto  err_regulator;
	}

	msleep(100);

	mutex_init(&(pericom_data->i2c_rw_mutex));
	pericom_data->i2c_client = client;
	pericom_data->irq = client->irq;
	i2c_set_clientdata(client, pericom_data);

	if (!ic_is_present(pericom_data)) {
		dev_err(&client->dev, "The device is absent\n");
		retval = -ENXIO;
		goto  err_absent;
	}

	/* default drp mode and active power mode */
	pericom_30216a_set_drp_mode(pericom_data);
	pericom_30216a_set_poweractive_mode(pericom_data);
	/* interrupt */
	retval = request_threaded_irq(pericom_data->irq, NULL,
		pericom_30216a_irq_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
		DRIVER_NAME, pericom_data);

	if (retval < 0) {
		dev_err(&client->dev,
				"%s: Failed to create irq thread\n",
				__func__);
		goto err_absent;
	}

	/*register typec class*/
	pericom_data->c_dev.name = DRIVER_NAME;
	pericom_data->c_dev.get_mode = pericom_30216a_get_id_status;
	pericom_data->c_dev.set_mode = pericom_30216a_set_id_status;
	pericom_data->c_dev.get_direction = pericom_30216a_get_cc_pin;
	retval = typec_dev_register(&pericom_data->c_dev);
	if (retval < 0)
		goto err_fs;

	return retval;

err_fs:
       free_irq(pericom_data->irq, pericom_data);

err_absent:
	regulator_disable(i2c_vdd);
err_regulator:
	devm_kfree(&client->dev, pericom_data);
	return retval;
}

static int pericom_30216a_remove(struct i2c_client *client)
{
	struct pericom_30216a_data *pericom_data = i2c_get_clientdata(client);

	pericom_30216a_set_powersaving_mode(pericom_data);
	return 0;
}

static void pericom_30216a_shutdown(struct i2c_client *client)
{
	struct pericom_30216a_data *pericom_data = i2c_get_clientdata(client);

	pericom_30216a_set_powersaving_mode(pericom_data);
}

static const struct i2c_device_id pericom_30216a_id_table[] = {
	{DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, pericom_30216a_id_table);

static struct of_device_id pericom_match_table[] = {
	{ .compatible = "pericom,30216a",},
	{ },
};

static struct i2c_driver pericom_30216a_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = pericom_match_table,

	},
	.probe = pericom_30216a_probe,
	.remove = pericom_30216a_remove,
	.shutdown = pericom_30216a_shutdown,
	.id_table = pericom_30216a_id_table,
};

static int __init pericom_30216a_init(void)
{
	return i2c_add_driver(&pericom_30216a_driver);
}

static void __exit pericom_30216a_exit(void)
{
	i2c_del_driver(&pericom_30216a_driver);
}

module_init(pericom_30216a_init);
module_exit(pericom_30216a_exit);

MODULE_AUTHOR("xiaomi, Inc.");
MODULE_DESCRIPTION("Pericom 30216A I2C  Driver");
MODULE_LICENSE("GPL v2");
