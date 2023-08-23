/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi160_i2c.c
 * @date     2014/11/25 14:40
 * @id       "20f77db"
 * @version  1.3
 *
 * @brief
 * This file implements moudle function, which add
 * the driver to I2C core.
*/

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include "bmi160_driver.h"

/*! @defgroup bmi160_i2c_src
 *  @brief bmi160 i2c driver module
 @{*/

static struct i2c_client *bmi_client;
/*!
 * @brief define i2c wirte function
 *
 * @param client the pointer of i2c client
 * @param reg_addr register address
 * @param data the pointer of data buffer
 * @param len block size need to write
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
/*	i2c read routine for API*/
static s8 bmi_i2c_read(struct i2c_client *client, u8 reg_addr,
			u8 *data, u8 len)
	{
#if !defined BMI_USE_BASIC_I2C_FUNC
		s32 dummy;
		if (NULL == client)
			return -EINVAL;

		while (0 != len--) {
#ifdef BMI_SMBUS
			dummy = i2c_smbus_read_byte_data(client, reg_addr);
			if (dummy < 0) {
				dev_err(&client->dev, "i2c smbus read error");
				return -EIO;
			}
			*data = (u8)(dummy & 0xff);
#else
			dummy = i2c_master_send(client, (char *)&reg_addr, 1);
			if (dummy < 0) {
				dev_err(&client->dev, "i2c bus master write error");
				return -EIO;
			}

			dummy = i2c_master_recv(client, (char *)data, 1);
			if (dummy < 0) {
				dev_err(&client->dev, "i2c bus master read error");
				return -EIO;
			}
#endif
			reg_addr++;
			data++;
		}
		return 0;
#else
		int retry;

		struct i2c_msg msg[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = 1,
			 .buf = &reg_addr,
			},

			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = len,
			 .buf = data,
			 },
		};

		for (retry = 0; retry < BMI_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0)
				break;
			else
				usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
				BMI_I2C_WRITE_DELAY_TIME * 1000);
		}

		if (BMI_MAX_RETRY_I2C_XFER <= retry) {
			dev_err(&client->dev, "I2C xfer error");
			return -EIO;
		}

		return 0;
#endif
	}


static s8 bmi_i2c_burst_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u16 len)
{
	int retry;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg_addr,
		},

		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	for (retry = 0; retry < BMI_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
				BMI_I2C_WRITE_DELAY_TIME * 1000);
	}

	if (BMI_MAX_RETRY_I2C_XFER <= retry) {
		dev_err(&client->dev, "I2C xfer error");
		return -EIO;
	}

	return 0;
}


/* i2c write routine for */
static s8 bmi_i2c_write(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
#if !defined BMI_USE_BASIC_I2C_FUNC
	s32 dummy;

#ifndef BMI_SMBUS
	u8 buffer[2];
#endif

	if (NULL == client)
		return -EPERM;

	while (0 != len--) {
#ifdef BMI_SMBUS
		dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
#else
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(client, (char *)buffer, 2);
#endif
		reg_addr++;
		data++;
		if (dummy < 0) {
			dev_err(&client->dev, "error writing i2c bus");
			return -EPERM;
		}

	}
	usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
	BMI_I2C_WRITE_DELAY_TIME * 1000);
	return 0;
#else
	u8 buffer[2];
	int retry;
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 2,
		 .buf = buffer,
		 },
	};

	while (0 != len--) {
		buffer[0] = reg_addr;
		buffer[1] = *data;
		for (retry = 0; retry < BMI_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0) {
				break;
			} else {
				usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
				BMI_I2C_WRITE_DELAY_TIME * 1000);
			}
		}
		if (BMI_MAX_RETRY_I2C_XFER <= retry) {
			dev_err(&client->dev, "I2C xfer error");
			return -EIO;
		}
		reg_addr++;
		data++;
	}

	usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
	BMI_I2C_WRITE_DELAY_TIME * 1000);
	return 0;
#endif
}


static s8 bmi_i2c_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	int err = 0;
	err = bmi_i2c_read(bmi_client, reg_addr, data, len);
	return err;
}

static s8 bmi_i2c_write_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	int err = 0;
	err = bmi_i2c_write(bmi_client, reg_addr, data, len);
	return err;
}

s8 bmi_burst_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u16 len)
{
	int err = 0;
	err = bmi_i2c_burst_read(bmi_client, reg_addr, data, len);
	return err;
}
EXPORT_SYMBOL(bmi_burst_read_wrapper);
/*!
 * @brief BMI probe function via i2c bus
 *
 * @param client the pointer of i2c client
 * @param id the pointer of i2c device id
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmi_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
		int err = 0;
		struct bmi_client_data *client_data = NULL;

		dev_info(&client->dev, "BMI160 i2c function probe entrance");

		if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
			dev_err(&client->dev, "i2c_check_functionality error!");
			err = -EIO;
			goto exit_err_clean;
		}

		if (NULL == bmi_client) {
			bmi_client = client;
		} else {
			dev_err(&client->dev,
				"this driver does not support multiple clients");
			err = -EBUSY;
			goto exit_err_clean;
		}

		client_data = kzalloc(sizeof(struct bmi_client_data),
							GFP_KERNEL);
		if (NULL == client_data) {
			dev_err(&client->dev, "no memory available");
			err = -ENOMEM;
			goto exit_err_clean;
		}

		client_data->device.bus_read = bmi_i2c_read_wrapper;
		client_data->device.bus_write = bmi_i2c_write_wrapper;

		return bmi_probe(client_data, &client->dev);

exit_err_clean:
		if (err)
			bmi_client = NULL;
		return err;
}
/*
static int bmi_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int err = 0;
	err = bmi_suspend(&client->dev);
	return err;
}

static int bmi_i2c_resume(struct i2c_client *client)
{
	int err = 0;

	err = bmi_resume(&client->dev);

	return err;
}
*/

static int bmi_i2c_remove(struct i2c_client *client)
{
	int err = 0;
	err = bmi_remove(&client->dev);
	bmi_client = NULL;

	return err;
}



static const struct i2c_device_id bmi_id[] = {
	{SENSOR_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, bmi_id);

static const struct of_device_id bmi160_of_match[] = {
	{ .compatible = "bosch-sensortec,bmi160", },
	{ .compatible = "bmi160", },
	{ .compatible = "bosch, bmi160", },
	{ }
};
MODULE_DEVICE_TABLE(of, bmi160_of_match);

static struct i2c_driver bmi_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
		.of_match_table = bmi160_of_match,
	},
	.class = I2C_CLASS_HWMON,
	.id_table = bmi_id,
	.probe = bmi_i2c_probe,
	.remove = bmi_i2c_remove,
	/*.suspend = bmi_i2c_suspend,
	.resume = bmi_i2c_resume,*/
};

static int __init BMI_i2c_init(void)
{
	return i2c_add_driver(&bmi_i2c_driver);
}

static void __exit BMI_i2c_exit(void)
{
	i2c_del_driver(&bmi_i2c_driver);
}

MODULE_AUTHOR("Contact <contact@bosch-sensortec.com>");
MODULE_DESCRIPTION("driver for " SENSOR_NAME);
MODULE_LICENSE("GPL v2");

module_init(BMI_i2c_init);
module_exit(BMI_i2c_exit);

