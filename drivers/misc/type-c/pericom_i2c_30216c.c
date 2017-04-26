/*
 * PERICOM 30216C driver
 *
 * Copyright (C) 2012 fengwei <fengwei@xiaomi.com>
 * Copyright (c) 2015-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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

#include "pericom_i2c_30216c.h"
#include "typec_class.h"

#define DRIVER_NAME "pericom_30216c"
/**
  * If the power supply of i2c isn't alwayson, you need define I2C_SUPPLY_NOT_ALWAYSON
  */
/* #define I2C_SUPPLY_NOT_ALWAYSON */

/*
   pericom data struct
  */
struct pericom_30216c_data {
	struct i2c_client *i2c_client;
	int irq;
	struct mutex i2c_rw_mutex;

	struct typec_dev c_dev;
};

 /**
 * pericom_30216c_i2c_read()
 *
 * Called by various functions in this driver,
 * This function reads data of an arbitrary length from the sensor,
 * starting from an assigned register address of the sensor, via I2C
 * with a retry mechanism.
 */
static int pericom_30216c_i2c_read(struct pericom_30216c_data *pericom_data,
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
		dev_dbg(&pericom_data->i2c_client->dev,
				"%s: I2C retry %d\n", __func__, retry + 1);
		msleep(10);
	}

	if (retry == PERICOM_I2C_RETRY_TIMES) {
		dev_err(&pericom_data->i2c_client->dev,
				"%s: I2C read over retry limit\n",	__func__);
		retval = -EIO;
	}

	mutex_unlock(&(pericom_data->i2c_rw_mutex));

	return retval;
}

 /**
 * pericom_30216c_i2c_write()
 *
 * Called by various functions in this driver
 *
 * This function writes data of an arbitrary length to the sensor,
 * starting from an assigned register address of the sensor, via I2C with
 * a retry mechanism.
 */
static int pericom_30216c_i2c_write(struct pericom_30216c_data *pericom_data,
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
		dev_dbg(&pericom_data->i2c_client->dev,
				"%s: I2C retry %d\n",
				__func__, retry + 1);
		msleep(10);
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

/* set mode */
static int pericom_30216c_set_power_mode(struct pericom_30216c_data *pericom_data, enum pericom_power_mode mode)
{
	int ret;
	char buf[2] = {0x20, 0};

	/* read reg1 value */
	pericom_30216c_i2c_read(pericom_data, buf, 2);
	/* construct reg1 value */
	buf[1] = (buf[1] & ~PERICOM_POWER_SAVING_MASK) | ((mode << PERICOM_POWER_SAVING_OFFSET) & PERICOM_POWER_SAVING_MASK);
	buf[1] &= ~PERICOM_INTERRUPT_MASK;

	/* write reg1 value */
	ret = pericom_30216c_i2c_write(pericom_data, buf, 2);
	return ret;
}

static int pericom_30216c_set_role_mode(struct pericom_30216c_data *pericom_data, enum pericom_role_mode mode)
{
	int ret, i;
	char buf[4] = {0, 0, 0, 0};

	/* read reg1 value */
	pericom_30216c_i2c_read(pericom_data, buf, 2);
	/* construct reg1 value */
	if (mode == 0x00) {
		buf[1] = (buf[1] & ~PERICOM_ROLE_MODE_MASK)
			| ((mode << PERICOM_ROLE_OFFSET) & PERICOM_ROLE_MODE_MASK);      /* device mode 0x00 */

	} else if (mode == 0x01) {
		buf[1] = (buf[1] & ~PERICOM_ROLE_MODE_MASK)
			| ((mode << PERICOM_ROLE_OFFSET) & PERICOM_ROLE_MODE_MASK);		/* Host mode 0x02,0x0a,0x12 */
	} else  /*default typec mode is trysnk mode */
		buf[1] = (buf[1] & ~PERICOM_ROLE_MODE_MASK)
			|(PERICOM_ROLE_MODE_MASK|PERICOM_DRP2_TRY_SNK);   /* TrySNK DRP mode 0x46,0x4e,0x56 */

	/* mask interrupt */
	buf[1] |= PERICOM_INTERRUPT_MASK;
	/* write reg1 value */
	ret = pericom_30216c_i2c_write(pericom_data, buf, 2);

	/* sleep and wait for correct status */
	for (i = 0; i <= 5; i++) {
		msleep(1500);
		pericom_30216c_i2c_read(pericom_data, buf, 4);
		dev_info(&pericom_data->i2c_client->dev,
				"read:%d,%d,%d,%d\n", buf[0], buf[1], buf[2], buf[3]);
		/* mode=0 : device, reg4 must be 0x08
		  * mode=1: host , reg4 must be 0x04
		  * mode=2: drp,  all value is ok
		  */
		if (((mode == DEVICE_MODE) && (buf[3] & 0x08))
			|| ((mode == HOST_MODE) && (buf[3] & 0x04))
			|| (mode == TRYSNK_DRP_MODE))
			break;
	}

	if (i > 5)
		dev_info(&pericom_data->i2c_client->dev, "try to %d mode fail \n", mode);
	/* unmask interrupt*/
	buf[1] &= ~PERICOM_INTERRUPT_MASK;
	/* write reg1 value */
	ret = pericom_30216c_i2c_write(pericom_data, buf, 2);

	return ret;
}
/*set device mode*/
static int pericom_30216c_set_device_mode(struct pericom_30216c_data *pericom_data)
{
	return pericom_30216c_set_role_mode(pericom_data, DEVICE_MODE);
}

/* set host mode
  * success if return positive value ,or return negative
  */
static int pericom_30216c_set_host_mode(struct pericom_30216c_data *pericom_data)
{
	return pericom_30216c_set_role_mode(pericom_data, HOST_MODE);
}

static int pericom_30216c_set_trysnk_drp_mode(struct pericom_30216c_data *pericom_data)
{
	return pericom_30216c_set_role_mode(pericom_data, TRYSNK_DRP_MODE);
}

/* get role mode
  * success if return positive value ,or return negative
  */
static int pericom_30216c_get_role_mode(struct pericom_30216c_data *pericom_data)
{
	int ret;
	char buf[2] = {0, 0};

	/* read reg1 value */
	ret = pericom_30216c_i2c_read(pericom_data, buf, 2);
	/* construct reg1 value */
	buf[1] = (buf[1] & PERICOM_ROLE_MODE_MASK) >> PERICOM_ROLE_OFFSET;
	return ret > 0 ? buf[1]:ret;
}

/* set power saving mode
  * success if return positive value ,or return negative
  */
static int pericom_30216c_set_powersaving_mode(struct pericom_30216c_data *pericom_data)
{
	return pericom_30216c_set_power_mode(pericom_data, POWERSAVING_MODE);
}

/* set active mode
  * success if return positive value ,or return negative
  */
static int pericom_30216c_set_poweractive_mode(struct pericom_30216c_data *pericom_data)
{
	return pericom_30216c_set_power_mode(pericom_data, ACTIVE_MODE);
}


static int pericom_30216c_get_id_status(struct typec_dev *dev)
{
	struct pericom_30216c_data *pericom_data =
		  container_of(dev, struct pericom_30216c_data, c_dev);

	return pericom_30216c_get_role_mode(pericom_data);
}

static int pericom_30216c_set_id_status(struct typec_dev *dev, int value)
{
	int rc = -1;
	struct pericom_30216c_data *pericom_data =
		  container_of(dev, struct pericom_30216c_data, c_dev);

	if (value == DEVICE_MODE)
		rc = pericom_30216c_set_device_mode(pericom_data);
	else if (value == HOST_MODE)
		rc = pericom_30216c_set_host_mode(pericom_data);
	else
		rc = pericom_30216c_set_trysnk_drp_mode(pericom_data);        /*trysnk mode*/

	return rc;
}

/* get cc  orientation
  * success if return positive value ,or return 0
  * return 1 means cc1
  * return 2 means cc2.
  */
static int pericom_30216c_get_cc_orientation(struct pericom_30216c_data *pericom_data)
{
	int ret;
	char buf[4] = {0, 0, 0, 0};

	/* read reg4 value */
	ret = pericom_30216c_i2c_read(pericom_data, buf, 4);
	/*construct reg1 value */
	buf[3] = buf[3] & PERICOM_CC_ORI_MASK;

	return ret > 0 ? buf[3]:0;
}

static int pericom_30216c_get_cc_pin(struct typec_dev *dev)
{
	struct pericom_30216c_data *pericom_data =
		  container_of(dev, struct pericom_30216c_data, c_dev);

	return pericom_30216c_get_cc_orientation(pericom_data);
}
static bool ic_is_present(struct pericom_30216c_data *pericom_data)
{
	int ret;
	char buf;

	/* read reg0 value */
	ret = pericom_30216c_i2c_read(pericom_data, &buf, 1);

	return (ret == 0x20) ? false:true;    /* 30216C ChipID is 0x20 */
}

static irqreturn_t pericom_30216c_irq_handler(int irq, void *dev_id)
{
	struct pericom_30216c_data *pericom_data = (struct pericom_30216c_data *) dev_id;
	char reg[4] = {0, 0, 0, 0};
	char curr_mode;
	static bool  plug_flag;

	/* 0.Mask interrupt */
	pericom_30216c_i2c_read(pericom_data,  reg, 2);
	dev_info(&pericom_data->i2c_client->dev, "0.reg=%x,%x,%x,%x\n", reg[0], reg[1], reg[2], reg[3]);
	curr_mode = reg[1] & (PERICOM_ROLE_MODE_MASK|PERICOM_DRP2_TRY_SNK);   /* support trysnk mode */
	reg[1] = reg[1] | PERICOM_INTERRUPT_MASK;
	pericom_30216c_i2c_write(pericom_data, reg, 2);
	/* 1.delay 30ms */
	msleep(30);
	/* 2.Read reg */
	pericom_30216c_i2c_read(pericom_data,  reg, 4);
	dev_info(&pericom_data->i2c_client->dev, "2.reg=%x,%x,%x,%x\n", reg[0], reg[1], reg[2], reg[3]);
	/* 3.Processing */
	if ((reg[2] == 0x2) || (reg[3] == 0x00) || (reg[3] == 0x80)) { /* detached */
		if ((reg[2] == 0x2) && (reg[3] == 0x00))
			plug_flag = false;
		curr_mode = (PERICOM_ROLE_MODE_MASK | PERICOM_DRP2_TRY_SNK); /*enter trysnk drp mode*/
	} else if (reg[3] == 0x04) {
		reg[1] = PERICOM_INTERRUPT_MASK;
		pericom_30216c_i2c_write(pericom_data, reg, 2);
		curr_mode = (PERICOM_ROLE_MODE_MASK | PERICOM_DRP2_TRY_SNK); /*enter trysnk drp mode*/
	} else if (reg[3] == 0x97) {
		msleep(350);
		reg[1] = PERICOM_INTERRUPT_MASK;
		pericom_30216c_i2c_write(pericom_data, reg, 2);
		curr_mode = (PERICOM_ROLE_MODE_MASK | PERICOM_DRP2_TRY_SNK); /*enter trysnk drp mode*/
	} else{ /*attached*/
		if ((reg[3] == 0x05) || (reg[3] == 0x06)) { /* Attached port status:device */
			curr_mode = (PERICOM_ROLE_MODE_MASK | PERICOM_DRP2_TRY_SNK); /*enter trysnk drp mode*/
		}
	}
	msleep(20);
	/* 4. Unmask interrupt */
	reg[1] = curr_mode;
	pericom_30216c_i2c_write(pericom_data, reg, 2);

	return IRQ_HANDLED;
}

static int pericom_30216c_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
{
	int retval = 0;
#ifdef I2C_SUPPLY_NOT_ALWAYSON
	struct regulator *i2c_vdd;
#endif
	struct pericom_30216c_data *pericom_data = client->dev.platform_data;

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
	/* Notice: the power supply of i2c is S4 and is always on
	  * so don't need these steps
	  * But, if the supply isn't alwayson, need these steps
	  */
#ifdef I2C_SUPPLY_NOT_ALWAYSON
	i2c_vdd = regulator_get(&client->dev, "i2c");
	if (IS_ERR(i2c_vdd)) {
		dev_err(&client->dev, "Regulator get failed i2c ret=%ld\n", PTR_ERR(i2c_vdd));
		goto  err_regulator;
	}

	retval = regulator_enable(i2c_vdd);
	if (retval) {
		dev_err(&client->dev, "Regulator set_vtg failed i2c ret=%d\n", retval);
		goto  err_regulator;
	}

	msleep(100);
#endif

	mutex_init(&(pericom_data->i2c_rw_mutex));
	pericom_data->i2c_client = client;
	pericom_data->irq = client->irq;
	i2c_set_clientdata(client, pericom_data);

	/* check ic present, if not present ,exit, or go on */
	if (!ic_is_present(pericom_data)) {
		dev_err(&client->dev, "The device is absent\n");
		retval = -ENXIO;
		goto  err_absent;
	}

	/* default drp mode and active power mode */
	pericom_30216c_set_trysnk_drp_mode(pericom_data);			/*initial 30216C and set trysnk drp mode to 30216c */
	pericom_30216c_set_poweractive_mode(pericom_data);
	/* interrupt */
	if (pericom_data->irq > 0) {
		retval = request_threaded_irq(pericom_data->irq, NULL,
			pericom_30216c_irq_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			DRIVER_NAME, pericom_data);

		if (retval < 0) {
			dev_err(&client->dev,
					"%s: Failed to create irq thread\n",
					__func__);
			goto err_absent;
		}
	}

	/*register typec class*/
	pericom_data->c_dev.name = DRIVER_NAME;
	pericom_data->c_dev.get_mode = pericom_30216c_get_id_status;
	pericom_data->c_dev.set_mode = pericom_30216c_set_id_status;
	pericom_data->c_dev.get_direction = pericom_30216c_get_cc_pin;
	retval = typec_dev_register(&pericom_data->c_dev);
	if (retval < 0)
		goto err_fs;
	/* enable_irq(pericom_data->irq); */

	return retval;

err_fs:
	free_irq(pericom_data->irq, pericom_data);
err_absent:
#ifdef I2C_SUPPLY_NOT_ALWAYSON
	regulator_disable(i2c_vdd);
err_regulator:
#endif
	devm_kfree(&client->dev, pericom_data);
	return retval;
}

static int pericom_30216c_remove(struct i2c_client *client)
{
	/*enter power saving mode */
	struct pericom_30216c_data *pericom_data = i2c_get_clientdata(client);

	pericom_30216c_set_powersaving_mode(pericom_data);
	return 0;
}

static void pericom_30216c_shutdown(struct i2c_client *client)
{
	/* enter power saving mode */
	struct pericom_30216c_data *pericom_data = i2c_get_clientdata(client);

	pericom_30216c_set_powersaving_mode(pericom_data);
}

static const struct i2c_device_id pericom_30216c_id_table[] = {
	{DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, pericom_30216c_id_table);

static struct of_device_id pericom_match_table[] = {
	{ .compatible = "pericom,30216c",},
	{ },
};

static struct i2c_driver pericom_30216c_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = pericom_match_table,

	},
	.probe = pericom_30216c_probe,
	.remove = pericom_30216c_remove,
	.shutdown = pericom_30216c_shutdown,
	.id_table = pericom_30216c_id_table,
};

static int __init pericom_30216c_init(void)
{
	return i2c_add_driver(&pericom_30216c_driver);
}

static void __exit pericom_30216c_exit(void)
{
	i2c_del_driver(&pericom_30216c_driver);
}

module_init(pericom_30216c_init);
module_exit(pericom_30216c_exit);

MODULE_AUTHOR("xiaomi, Inc.");
MODULE_DESCRIPTION("Pericom 30216C I2C  Driver");
MODULE_LICENSE("GPL v2");
