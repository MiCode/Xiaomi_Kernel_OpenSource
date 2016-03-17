/*
 * PERICOM 30216A driver
 *
 * Copyright (C) 2016 XiaoMi, Inc.
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

#include "ti_i2c_tusb320.h"
#include "typec_class.h"

#define DRIVER_NAME "ti_tusb320"

/*
   tusb320 data struct
  */
struct ti_tusb320_data{
	struct i2c_client *i2c_client;

	int irq;
	struct mutex i2c_rw_mutex;
	struct typec_dev c_dev;
};

static bool int_disable;

 /**
 * ti_tusb320_i2c_read()
 *
 * Called by various functions in this driver,
 * This function reads data of an arbitrary length from the sensor,
 * starting from an assigned register address of the sensor, via I2C
 * with a retry mechanism.
 */
static int ti_tusb320_i2c_read(struct ti_tusb320_data *ti_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval;
	unsigned char retry;
	unsigned char buf;
	struct i2c_msg msg[] = {
		{
			.addr = ti_data->i2c_client->addr,
			.flags = 0,
			.len = 1,
			.buf = &buf,
		},
		{
			.addr = ti_data->i2c_client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = data,
		},
	};

	buf = addr & 0xff;
	mutex_lock(&(ti_data->i2c_rw_mutex));

	for (retry = 0; retry < TI_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(ti_data->i2c_client->adapter, msg, 2) > 0) {
			retval = length;
			break;
		}
		dev_err(&ti_data->i2c_client->dev,
				"%s: I2C retry %d\n", __func__, retry + 1);
		msleep(20);
	}

	if (retry == TI_I2C_RETRY_TIMES) {
		dev_err(&ti_data->i2c_client->dev,
				"%s: I2C read over retry limit\n", __func__);
		retval = -EIO;
	}

	mutex_unlock(&(ti_data->i2c_rw_mutex));

	return retval;
}

 /**
 * ti_tusb320_i2c_write()
 *
 * Called by various functions in this driver
 *
 * This function writes data of an arbitrary length to the sensor,
 * starting from an assigned register address of the sensor, via I2C with
 * a retry mechanism.
 */
static int ti_tusb320_i2c_write(struct ti_tusb320_data *ti_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval;
	unsigned char retry;
	unsigned char buf[length + 1];
	struct i2c_msg msg[] = {
		{
			.addr = ti_data->i2c_client->addr,
			.flags = 0,
			.len = length + 1,
			.buf = buf,
		}
	};

	buf[0] = addr & 0xff;
	memcpy(&buf[1], &data[0], length);
	mutex_lock(&(ti_data->i2c_rw_mutex));

	for (retry = 0; retry < TI_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(ti_data->i2c_client->adapter, msg, 1) == 1) {
			retval = length;
			break;
		}
		dev_err(&ti_data->i2c_client->dev,
				"%s: I2C retry %d\n",
				__func__, retry + 1);
		msleep(20);
	}

	if (retry == TI_I2C_RETRY_TIMES) {
		dev_err(&ti_data->i2c_client->dev,
				"%s: I2C write over retry limit\n",
				__func__);
		retval = -EIO;
	}

	mutex_unlock(&(ti_data->i2c_rw_mutex));

	return retval;
}

static int ti_tusb320l_set_role_mode(struct ti_tusb320_data *ti_data, enum ti_role_mode mode)
{
	int ret;
	char buf;

	/*1. Set disable_term register */
	buf = TI_DISABLE_TERM_VAL;
	ret = ti_tusb320_i2c_write(ti_data, TI_ROLE_MODE_REG, &buf, 1);
	if (ret < 0)
		dev_info(&ti_data->i2c_client->dev, "set TI_ROLE_MODE_REG fail\n");
	/*2. change mode select */
	ti_tusb320_i2c_read(ti_data, TI_ROLE_MODE_REG, &buf, 1);
	buf = (buf & ~TI_ROLE_MODE_MASK)|((mode << TI_ROLE_OFFSET) & TI_ROLE_MODE_MASK);
	ret = ti_tusb320_i2c_write(ti_data, TI_ROLE_MODE_REG, &buf, 1);
	if (ret < 0)
		dev_info(&ti_data->i2c_client->dev, "set TI_ROLE_MODE_REG fail\n");
	/*3. wait 5ms */
	msleep(5);
	/*4. Clear disable term register */
	buf &= ~TI_DISABLE_TERM_VAL;
	ret = ti_tusb320_i2c_write(ti_data, TI_ROLE_MODE_REG, &buf, 1);
	if (ret < 0)
		dev_info(&ti_data->i2c_client->dev, "set TI_ROLE_MODE_REG fail\n");

	return ret;
}

static int ti_tusb320_set_role_mode(struct ti_tusb320_data *ti_data, enum ti_role_mode mode)
{
	int ret;
	int i;
	char buf;

	dev_info(&ti_data->i2c_client->dev, "enter set role mode\n");
	int_disable = true;
	/* 0. change mode select to UFP*/
	buf = DEVICE_MODE << TI_ROLE_OFFSET;
	ti_tusb320_i2c_write(ti_data, TI_ROLE_MODE_REG, &buf, 1);

	/* 1. set DISABLE_RD_RP (address 0x45 bit 2) */
	buf = TI_SET_DISABLE_RD_RP;
	ret = ti_tusb320_i2c_write(ti_data, TI_DISABLE_RD_RP_REG, &buf, 1);
	if (ret < 0)
		dev_info(&ti_data->i2c_client->dev, "set DISBALE_RD_RP fail\n");

	/* 2. set I2C_SOFT_RESET (address 0x0a bit3) */
	ti_tusb320_i2c_read(ti_data, TI_ROLE_MODE_REG, &buf, 1);
	buf |= TI_SOFT_RESET;
	ret = ti_tusb320_i2c_write(ti_data, TI_ROLE_MODE_REG, &buf, 1);
	if (ret < 0)
		dev_info(&ti_data->i2c_client->dev, "set DISBALE_RD_RP fail\n");
	msleep(100);
	/* 3. change MODE_SELECT to any mode */
	ti_tusb320_i2c_read(ti_data, TI_ROLE_MODE_REG, &buf, 1);
	buf = (buf & ~TI_ROLE_MODE_MASK)|((mode << TI_ROLE_OFFSET) & TI_ROLE_MODE_MASK);
	ret = ti_tusb320_i2c_write(ti_data, TI_ROLE_MODE_REG, &buf, 1);
	msleep(2);
	/* 4. clear DISABLE_RD_RP */
	buf = TI_CLR_DISABLE_RD_RP;
	ret = ti_tusb320_i2c_write(ti_data, TI_DISABLE_RD_RP_REG, &buf, 1);
	if (ret < 0)
		dev_info(&ti_data->i2c_client->dev, "set DISBALE_RD_RP fail\n");

	for (i = 0; i <= 5; i++) {
		if (mode == DRP_MODE) break;
		msleep(1200);
		ti_tusb320_i2c_read(ti_data, TI_STATUS_REG, &buf, 1);
		dev_info(&ti_data->i2c_client->dev, "i=%d, buf = 0x%x\n", i, buf);
		if ((((buf >> 6) == 1) && (mode == HOST_MODE))
			|| (((buf >> 6) == 2) && (mode == DEVICE_MODE)))
			break;
	}
	dev_info(&ti_data->i2c_client->dev, "exit set role mode\n");
	int_disable = false;
	return ret;
}
/*set device mode*/
static int ti_tusb320_set_device_mode(struct ti_tusb320_data *ti_data)
{
	if (ti_data->i2c_client->addr == TI_TUSB320L_ADDR)
		return ti_tusb320l_set_role_mode(ti_data, DEVICE_MODE);

	return ti_tusb320_set_role_mode(ti_data, DEVICE_MODE);
}

/* set host mode
  * success if return positive value ,or return negative
  */
static int ti_tusb320_set_host_mode(struct ti_tusb320_data *ti_data)
{
	if (ti_data->i2c_client->addr == TI_TUSB320L_ADDR)
		return ti_tusb320l_set_role_mode(ti_data, HOST_MODE);

	return ti_tusb320_set_role_mode(ti_data, HOST_MODE);
}

static int ti_tusb320_set_trysnk_drp_mode(struct ti_tusb320_data *ti_data)
{
	int ret;
	char buf;

	/*1. Set disable_term register */
	buf = TI_DISABLE_TERM_VAL;
	ret = ti_tusb320_i2c_write(ti_data, TI_ROLE_MODE_REG, &buf, 1);
	if (ret < 0)
		dev_info(&ti_data->i2c_client->dev, "set TI_ROLE_MODE_REG fail\n");
	/*2. change mode select */
	ti_tusb320_i2c_read(ti_data, TI_ROLE_MODE_REG, &buf, 1);

	buf = (buf & ~TI_DRP_ROLE_MODE_MASK)|((TRYSNK_DRP_MODE << TI_DRP_ROLE_OFFSET) & TI_DRP_ROLE_MODE_MASK);

	ret = ti_tusb320_i2c_write(ti_data, TI_ROLE_MODE_REG, &buf, 1);
	if (ret < 0)
		dev_info(&ti_data->i2c_client->dev, "set TI_ROLE_MODE_REG fail\n");
	/*3. wait 5ms */
	msleep(5);
	/*4. Clear disable term register */
	buf &= ~TI_DISABLE_TERM_VAL;
	ret = ti_tusb320_i2c_write(ti_data, TI_ROLE_MODE_REG, &buf, 1);
	if (ret < 0)
		dev_info(&ti_data->i2c_client->dev, "set TI_ROLE_MODE_REG fail\n");

	return ret;
}

/* set DRP mode
  * success if return positive value ,or return negative
  */
static int ti_tusb320_set_drp_mode(struct ti_tusb320_data *ti_data)
{
	if (ti_data->i2c_client->addr == TI_TUSB320L_ADDR)
		ti_tusb320_set_trysnk_drp_mode(ti_data);

	return ti_tusb320_set_role_mode(ti_data, DRP_MODE);
}

/* get role mode
  * success if return positive value ,or return negative
  */
static int ti_tusb320_get_role_mode(struct ti_tusb320_data *ti_data)
{
	int ret;
	char attached_state;

	ret = ti_tusb320_i2c_read(ti_data, TI_ROLE_MODE_REG, &attached_state, 1);

	return ret > 0 ? (((attached_state&0X30) >> 4) == 0 ? 2:((attached_state&0X30) >> 4) - 1):ret;
}
static int ti_tusb320_get_id_status(struct typec_dev *dev)
{
	struct ti_tusb320_data *ti_data =
		  container_of(dev, struct ti_tusb320_data, c_dev);

	return ti_tusb320_get_role_mode(ti_data);
}

/*
   for compatible,
   buf = 0  means device mode, 1 means host mode,2 means drp mode
  */
static int ti_tusb320_set_id_status(struct typec_dev *dev, int value)
{
	int rc = -1;
	struct ti_tusb320_data *ti_data =
		  container_of(dev, struct ti_tusb320_data, c_dev);

	if (value == 0)
		rc = ti_tusb320_set_device_mode(ti_data);
	else if (value == 1)
		rc = ti_tusb320_set_host_mode(ti_data);
	else if (value == 2)
		rc = ti_tusb320_set_drp_mode(ti_data);

	return rc;
}

/* get cc  orientation
  * success if return positive value ,or return 0
  * return 1 means cc1
  * return 2 means cc2.
  */
static int ti_tusb320_get_cc_orientation(struct ti_tusb320_data *ti_data)
{
	int ret;
	char attached_state;

	ret = ti_tusb320_i2c_read(ti_data, TI_STATUS_REG, &attached_state, 1);

	return ret > 0 ? ((attached_state&0x20) ? 2 : 1):0;
}


static int ti_tusb320_get_cc_pin(struct typec_dev *dev)
{
	struct ti_tusb320_data *ti_data =
		  container_of(dev, struct ti_tusb320_data, c_dev);

	return ti_tusb320_get_cc_orientation(ti_data);
}

static bool ic_is_present(struct ti_tusb320_data *ti_data)
{
	int ret;
	char buf[12];

	ret = ti_tusb320_i2c_read(ti_data, TI_DEVICE_ID_REG, buf, 11);
	buf[11] = '\0';
	if (ret > 0)
		dev_info(&ti_data->i2c_client->dev, "%s,%d,%d\n", buf, buf[9], buf[10]);

	return (ret < 0) ? false:true;
}

static irqreturn_t ti_tusb320_irq_handler(int irq, void *dev_id)
{
	struct ti_tusb320_data *ti_data  = (struct ti_tusb320_data *) dev_id;
	static char try_snk_attempt;
	char attached_state ;
	char buf;

	if (int_disable) {
		goto clr_int;
	}
	ti_tusb320_i2c_read(ti_data, TI_STATUS_REG, &attached_state, 1);
	dev_info(&ti_data->i2c_client->dev, "Enter Ti Interrupt, attached_state:0x%x\n", attached_state);
	if (try_snk_attempt == 1) {
		try_snk_attempt = 0;
	} else {
		if ((attached_state & 0xC0) == 0x40) {
			try_snk_attempt++;
			buf = TI_SOFT_RESET;
			ti_tusb320_i2c_write(ti_data, TI_ROLE_MODE_REG, &buf, 1);
			msleep(25);

			ti_tusb320_i2c_write(ti_data, TI_ROLE_MODE_REG, &buf, 1);
			msleep(400);

			buf = TI_MAX_DUTY_CYCLE | TI_CLEAR_INT;
			ti_tusb320_i2c_write(ti_data, TI_STATUS_REG, &buf, 1);
		} else if ((attached_state & 0xC0) == 0) {
			ti_tusb320_set_drp_mode(ti_data);
		}
	}

clr_int:
	buf = TI_CLEAR_INT;
	ti_tusb320_i2c_write(ti_data, TI_STATUS_REG, &buf, 1);

	return IRQ_HANDLED;
}

static int ti_tusb320_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
{
	int retval = 0;
	struct regulator *i2c_vdd;
	struct ti_tusb320_data *tusb320_data =
			client->dev.platform_data;

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev,
				"%s: SMBus byte data not supported\n",
				__func__);
		return -EIO;
	}

	if (client->dev.of_node) {
		tusb320_data = devm_kzalloc(&client->dev,
			sizeof(*tusb320_data),
			GFP_KERNEL);
		if (!tusb320_data) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
	}

	if (!tusb320_data) {
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

	mutex_init(&(tusb320_data->i2c_rw_mutex));
	tusb320_data->i2c_client = client;
	tusb320_data->irq = client->irq;
	i2c_set_clientdata(client, tusb320_data);

	if (!ic_is_present(tusb320_data)) {
		dev_err(&client->dev, "The device is absent\n");
		retval = -ENXIO;
		goto  err_absent;
	}

	if (client->addr == TI_TUSB320L_ADDR) {
		retval = ti_tusb320_set_trysnk_drp_mode(tusb320_data);
		if (retval < 0)
			dev_err(&client->dev,
				"%s: Failed to set trysnk \n",
				__func__);
		goto normal;
	}
	/* default drp mode and active power mode */
	retval = ti_tusb320_set_drp_mode(tusb320_data);
	/* interrupt */
	retval = request_threaded_irq(tusb320_data->irq, NULL,
		ti_tusb320_irq_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
		DRIVER_NAME, tusb320_data);

	if (retval < 0) {
		dev_err(&client->dev,
				"%s: Failed to create irq thread\n",
				__func__);
		goto err_absent;
	}


normal:
	/*register typec class*/
	tusb320_data->c_dev.name = DRIVER_NAME;
	tusb320_data->c_dev.get_mode = ti_tusb320_get_id_status;
	tusb320_data->c_dev.set_mode = ti_tusb320_set_id_status;
	tusb320_data->c_dev.get_direction = ti_tusb320_get_cc_pin;
	retval = typec_dev_register(&tusb320_data->c_dev);
	if (retval < 0)
		goto err_fs;

	return retval;

err_fs:
	if (client->addr != TI_TUSB320L_ADDR)
		free_irq(tusb320_data->irq, tusb320_data);
err_absent:
	regulator_disable(i2c_vdd);
err_regulator:
	devm_kfree(&client->dev, tusb320_data);
	return retval;
}

static int ti_tusb320_remove(struct i2c_client *client)
{
	return 0;
}

static void ti_tusb320_shutdown(struct i2c_client *client)
{
	struct ti_tusb320_data *ti_data = i2c_get_clientdata(client);

	ti_tusb320_set_device_mode(ti_data);
}

static const struct i2c_device_id ti_tusb320_id_table[] = {
	{DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, ti_tusb320_id_table);

static struct of_device_id ti_match_table[] = {
	{ .compatible = "ti,tusb320",},
	{ .compatible = "ti,tusb320l",},
	{ },
};

static struct i2c_driver ti_tusb320_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ti_match_table,

	},
	.probe = ti_tusb320_probe,
	.remove = ti_tusb320_remove,
	.shutdown = ti_tusb320_shutdown,
	.id_table = ti_tusb320_id_table,
};

static int __init ti_tusb320_init(void)
{
	return i2c_add_driver(&ti_tusb320_driver);
}

static void __exit ti_tusb320_exit(void)
{
	i2c_del_driver(&ti_tusb320_driver);
}

module_init(ti_tusb320_init);
module_exit(ti_tusb320_exit);

MODULE_AUTHOR("xiaomi, Inc.");
MODULE_DESCRIPTION("Ti tusb320 I2C  Driver");
MODULE_LICENSE("GPL v2");
