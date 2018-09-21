/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * (C) Modification Copyright 2018 Robert Bosch Kft  All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * Special: Description of the Software:
 *
 * This software module (hereinafter called "Software") and any
 * information on application-sheets (hereinafter called "Information") is
 * provided free of charge for the sole purpose to support your application
 * work. 
 *
 * As such, the Software is merely an experimental software, not tested for
 * safety in the field and only intended for inspiration for further development 
 * and testing. Any usage in a safety-relevant field of use (like automotive,
 * seafaring, spacefaring, industrial plants etc.) was not intended, so there are
 * no precautions for such usage incorporated in the Software.
 * 
 * The Software is specifically designed for the exclusive use for Bosch
 * Sensortec products by personnel who have special experience and training. Do
 * not use this Software if you do not have the proper experience or training.
 * 
 * This Software package is provided as is and without any expressed or
 * implied warranties, including without limitation, the implied warranties of
 * merchantability and fitness for a particular purpose.
 * 
 * Bosch Sensortec and their representatives and agents deny any liability for
 * the functional impairment of this Software in terms of fitness, performance
 * and safety. Bosch Sensortec and their representatives and agents shall not be
 * liable for any direct or indirect damages or injury, except as otherwise
 * stipulated in mandatory applicable law.
 * The Information provided is believed to be accurate and reliable. Bosch
 * Sensortec assumes no responsibility for the consequences of use of such
 * Information nor for any infringement of patents or other rights of third
 * parties which may result from its use.
 * 
 *------------------------------------------------------------------------------
 * The following Product Disclaimer does not apply to the BSX4-HAL-4.1NoFusion Software 
 * which is licensed under the Apache License, Version 2.0 as stated above.  
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Product Disclaimer
 *
 * Common:
 *
 * Assessment of Products Returned from Field
 *
 * Returned products are considered good if they fulfill the specifications / 
 * test data for 0-mileage and field listed in this document.
 *
 * Engineering Samples
 * 
 * Engineering samples are marked with (e) or (E). Samples may vary from the
 * valid technical specifications of the series product contained in this
 * data sheet. Therefore, they are not intended or fit for resale to
 * third parties or for use in end products. Their sole purpose is internal
 * client testing. The testing of an engineering sample may in no way replace
 * the testing of a series product. Bosch assumes no liability for the use
 * of engineering samples. The purchaser shall indemnify Bosch from all claims
 * arising from the use of engineering samples.
 *
 * Intended use
 *
 * Provided that SMI130 is used within the conditions (environment, application,
 * installation, loads) as described in this TCD and the corresponding
 * agreed upon documents, Bosch ensures that the product complies with
 * the agreed properties. Agreements beyond this require
 * the written approval by Bosch. The product is considered fit for the intended
 * use when the product successfully has passed the tests
 * in accordance with the TCD and agreed upon documents.
 *
 * It is the responsibility of the customer to ensure the proper application
 * of the product in the overall system/vehicle.
 *
 * Bosch does not assume any responsibility for changes to the environment
 * of the product that deviate from the TCD and the agreed upon documents 
 * as well as all applications not released by Bosch
  *
 * The resale and/or use of products are at the purchaser’s own risk and 
 * responsibility. The examination and testing of the SMI130 
 * is the sole responsibility of the purchaser.
 *
 * The purchaser shall indemnify Bosch from all third party claims 
 * arising from any product use not covered by the parameters of 
 * this product data sheet or not approved by Bosch and reimburse Bosch 
 * for all costs and damages in connection with such claims.
 *
 * The purchaser must monitor the market for the purchased products,
 * particularly with regard to product safety, and inform Bosch without delay
 * of all security relevant incidents.
 *
 * Application Examples and Hints
 *
 * With respect to any application examples, advice, normal values
 * and/or any information regarding the application of the device,
 * Bosch hereby disclaims any and all warranties and liabilities of any kind,
 * including without limitation warranties of
 * non-infringement of intellectual property rights or copyrights
 * of any third party.
 * The information given in this document shall in no event be regarded 
 * as a guarantee of conditions or characteristics. They are provided
 * for illustrative purposes only and no evaluation regarding infringement
 * of intellectual property rights or copyrights or regarding functionality,
 * performance or error has been made.
 *
 * @filename smi130_i2c.c
 * @date     2014/11/25 14:40
 * @Modification Date 2018/08/28 18:20
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
#include "smi130_driver.h"

/*! @defgroup smi130_i2c_src
 *  @brief smi130 i2c driver module
 @{*/

static struct i2c_client *smi_client;
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
static s8 smi_i2c_read(struct i2c_client *client, u8 reg_addr,
			u8 *data, u8 len)
	{
#if !defined SMI_USE_BASIC_I2C_FUNC
		s32 dummy;
		if (NULL == client)
			return -EINVAL;

		while (0 != len--) {
#ifdef SMI_SMBUS
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

		for (retry = 0; retry < SMI_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0)
				break;
			else
				usleep_range(SMI_I2C_WRITE_DELAY_TIME * 1000,
				SMI_I2C_WRITE_DELAY_TIME * 1000);
		}

		if (SMI_MAX_RETRY_I2C_XFER <= retry) {
			dev_err(&client->dev, "I2C xfer error");
			return -EIO;
		}

		return 0;
#endif
	}


static s8 smi_i2c_burst_read(struct i2c_client *client, u8 reg_addr,
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

	for (retry = 0; retry < SMI_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			usleep_range(SMI_I2C_WRITE_DELAY_TIME * 1000,
				SMI_I2C_WRITE_DELAY_TIME * 1000);
	}

	if (SMI_MAX_RETRY_I2C_XFER <= retry) {
		dev_err(&client->dev, "I2C xfer error");
		return -EIO;
	}

	return 0;
}


/* i2c write routine for */
static s8 smi_i2c_write(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
#if !defined SMI_USE_BASIC_I2C_FUNC
	s32 dummy;

#ifndef SMI_SMBUS
	u8 buffer[2];
#endif

	if (NULL == client)
		return -EPERM;

	while (0 != len--) {
#ifdef SMI_SMBUS
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
	usleep_range(SMI_I2C_WRITE_DELAY_TIME * 1000,
	SMI_I2C_WRITE_DELAY_TIME * 1000);
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
		for (retry = 0; retry < SMI_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0) {
				break;
			} else {
				usleep_range(SMI_I2C_WRITE_DELAY_TIME * 1000,
				SMI_I2C_WRITE_DELAY_TIME * 1000);
			}
		}
		if (SMI_MAX_RETRY_I2C_XFER <= retry) {
			dev_err(&client->dev, "I2C xfer error");
			return -EIO;
		}
		reg_addr++;
		data++;
	}

	usleep_range(SMI_I2C_WRITE_DELAY_TIME * 1000,
	SMI_I2C_WRITE_DELAY_TIME * 1000);
	return 0;
#endif
}


static s8 smi_i2c_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	int err = 0;
	err = smi_i2c_read(smi_client, reg_addr, data, len);
	return err;
}

static s8 smi_i2c_write_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	int err = 0;
	err = smi_i2c_write(smi_client, reg_addr, data, len);
	return err;
}

s8 smi_burst_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u16 len)
{
	int err = 0;
	err = smi_i2c_burst_read(smi_client, reg_addr, data, len);
	return err;
}
EXPORT_SYMBOL(smi_burst_read_wrapper);
/*!
 * @brief SMI probe function via i2c bus
 *
 * @param client the pointer of i2c client
 * @param id the pointer of i2c device id
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int smi_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
		int err = 0;
		struct smi_client_data *client_data = NULL;

		dev_info(&client->dev, "SMI130 i2c function probe entrance");

		if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
			dev_err(&client->dev, "i2c_check_functionality error!");
			err = -EIO;
			goto exit_err_clean;
		}

		if (NULL == smi_client) {
			smi_client = client;
		} else {
			dev_err(&client->dev,
				"this driver does not support multiple clients");
			err = -EBUSY;
			goto exit_err_clean;
		}

		client_data = kzalloc(sizeof(struct smi_client_data),
							GFP_KERNEL);
		if (NULL == client_data) {
			dev_err(&client->dev, "no memory available");
			err = -ENOMEM;
			goto exit_err_clean;
		}

		client_data->device.bus_read = smi_i2c_read_wrapper;
		client_data->device.bus_write = smi_i2c_write_wrapper;

		return smi_probe(client_data, &client->dev);

exit_err_clean:
		if (err)
			smi_client = NULL;
		return err;
}
/*
static int smi_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int err = 0;
	err = smi_suspend(&client->dev);
	return err;
}

static int smi_i2c_resume(struct i2c_client *client)
{
	int err = 0;

	err = smi_resume(&client->dev);

	return err;
}
*/

static int smi_i2c_remove(struct i2c_client *client)
{
	int err = 0;
	err = smi_remove(&client->dev);
	smi_client = NULL;

	return err;
}



static const struct i2c_device_id smi_id[] = {
	{SENSOR_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, smi_id);

static const struct of_device_id smi130_of_match[] = {
	{ .compatible = "bosch-sensortec,smi130", },
	{ .compatible = "smi130", },
	{ .compatible = "bosch, smi130", },
	{ }
};
MODULE_DEVICE_TABLE(of, smi130_of_match);

static struct i2c_driver smi_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
		.of_match_table = smi130_of_match,
	},
	.class = I2C_CLASS_HWMON,
	.id_table = smi_id,
	.probe = smi_i2c_probe,
	.remove = smi_i2c_remove,
	/*.suspend = smi_i2c_suspend,
	.resume = smi_i2c_resume,*/
};

static int __init SMI_i2c_init(void)
{
	return i2c_add_driver(&smi_i2c_driver);
}

static void __exit SMI_i2c_exit(void)
{
	i2c_del_driver(&smi_i2c_driver);
}

MODULE_AUTHOR("Contact <contact@bosch-sensortec.com>");
MODULE_DESCRIPTION("driver for " SENSOR_NAME);
MODULE_LICENSE("GPL v2");

module_init(SMI_i2c_init);
module_exit(SMI_i2c_exit);

