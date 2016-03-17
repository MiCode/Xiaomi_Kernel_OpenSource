/*!
 * @section LICENSE
 * (C) Copyright 2014 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmp280_i2c.c
 * @date     "Wed Mar 18 14:54:50 2015 +0800"
 * @id       "5d6d280"
 *
 * @brief
 * This file implements module function, which adds
 * the driver to I2C core.
*/

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include "bmp280_core.h"

static struct i2c_client *bmp_i2c_client;

/*! @defgroup bmp280_i2c_src
 *  @brief bmp280 i2c driver module
 @{*/
/*! maximum retry times during i2c transfer */
#define BMP_MAX_RETRY_I2C_XFER 10
/*! wait time after i2c transfer error occurred */
#define BMP_I2C_WRITE_DELAY_TIME 1

#ifdef BMP_USE_BASIC_I2C_FUNC
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
static char bmp_i2c_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
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

	for (retry = 0; retry < BMP_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			mdelay(BMP_I2C_WRITE_DELAY_TIME);
	}

	if (BMP_MAX_RETRY_I2C_XFER <= retry) {
		PERR("I2C xfer error");
		return -EIO;
	}

	return 0;
}

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
static char bmp_i2c_write(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
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
		for (retry = 0; retry < BMP_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0) {
				break;
			} else {
				mdelay(BMP_I2C_WRITE_DELAY_TIME);
			}
		}
		if (BMP_MAX_RETRY_I2C_XFER <= retry) {
			PERR("I2C xfer error");
			return -EIO;
		}
		reg_addr++;
		data++;
	}

	return 0;
}
#endif/*BMP_USE_BASIC_I2C_FUNC*/

/*!
 * @brief define i2c block wirte function
 *
 * @param dev_addr sensor i2c address
 * @param reg_addr register address
 * @param data the pointer of data buffer
 * @param len block size need to write
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static char bmp_i2c_write_block(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	char err = 0;

	if (NULL == bmp_i2c_client)
		return -EPERM;

#ifdef BMP_USE_BASIC_I2C_FUNC
	err = bmp_i2c_write(bmp_i2c_client, reg_addr, data, len);
#else
	err = i2c_smbus_write_i2c_block_data(bmp_i2c_client, \
			reg_addr, len, data);
#endif
	if (err < 0)
		return err;

	return 0;
}

/*!
 * @brief define i2c block read function
 *
 * @param dev_addr sensor i2c address
 * @param reg_addr register address
 * @param data the pointer of data buffer
 * @param len block size need to read
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static char bmp_i2c_read_block(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	char err = 0;

	if (NULL == bmp_i2c_client)
		return -EPERM;

#ifdef BMP_USE_BASIC_I2C_FUNC
	err = bmp_i2c_read(bmp_i2c_client, reg_addr, data, len);
#else
	err = i2c_smbus_read_i2c_block_data(bmp_i2c_client, \
			reg_addr, len, data);
#endif
	if (err < 0)
		return err;

	return 0;
}

/*!
 * @brief i2c bus operation
*/
static const struct bmp_bus_ops bmp_i2c_bus_ops = {
	/**< i2c block write pointer */
	.bus_write	= bmp_i2c_write_block,
	/**< i2c block read pointer */
	.bus_read	= bmp_i2c_read_block
};

/*!
 * @brief BMP probe function via i2c bus
 *
 * @param client the pointer of i2c client
 * @param id the pointer of i2c device id
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp_i2c_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	struct bmp_data_bus data_bus = {
		.bops = &bmp_i2c_bus_ops,
		.client = client
	};

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		PERR("i2c_check_functionality error!");
		return -EIO;
	}

	if (NULL == bmp_i2c_client)
		bmp_i2c_client = client;
	else{
		PERR("This driver does not support multiple clients!\n");
		return -EINVAL;
	}

	return bmp_probe(&client->dev, &data_bus);
}

/*!
 * @brief shutdown bmp device in i2c driver
 *
 * @param client the pointer of i2c client
 *
 * @return no return value
*/
static void bmp_i2c_shutdown(struct i2c_client *client)
{
#ifdef CONFIG_PM
	bmp_disable(&client->dev);
#endif
}

/*!
 * @brief remove bmp i2c client
 *
 * @param client the pointer of i2c client
 *
 * @return zero
 * @retval zero
*/
static int bmp_i2c_remove(struct i2c_client *client)
{
	return bmp_remove(&client->dev);
}

#ifdef CONFIG_PM
/*!
 * @brief suspend bmp device in i2c driver
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
static int bmp_i2c_suspend(struct device *dev)
{
	return bmp_disable(dev);
}

/*!
 * @brief resume bmp device in i2c driver
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
static int bmp_i2c_resume(struct device *dev)
{
	return bmp_enable(dev);
}

/*!
 * @brief register i2c device power manager hooks
*/
static const struct dev_pm_ops bmp_i2c_pm_ops = {
	/**< device suspend */
	.suspend = bmp_i2c_suspend,
	/**< device resume */
	.resume = bmp_i2c_resume
};
#endif

/*!
 * @brief register i2c device id
*/
static const struct i2c_device_id bmp_id[] = {
	{ BMP_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bmp_id);

static const struct of_device_id bmp_of_match[] = {
	{ .compatible = "bmp,bmp280", },
	{ },
};

/*!
 * @brief register i2c driver hooks
*/
static struct i2c_driver bmp_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= BMP_NAME,
#ifdef CONFIG_PM
		.pm	= &bmp_i2c_pm_ops,
#endif
		.of_match_table = of_match_ptr(bmp_of_match),
	},
	.id_table	= bmp_id,
	.probe		= bmp_i2c_probe,
	.shutdown	= bmp_i2c_shutdown,
	.remove		= bmp_i2c_remove
};

/*!
 * @brief initialize bmp i2c module
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int __init bmp_i2c_init(void)
{
	return i2c_add_driver(&bmp_i2c_driver);
}

/*!
 * @brief remove bmp i2c module
 *
 * @return no return value
*/
static void __exit bmp_i2c_exit(void)
{
	i2c_del_driver(&bmp_i2c_driver);
}


MODULE_DESCRIPTION("BMP280 I2C DRIVER");
MODULE_LICENSE("GPL v2");

module_init(bmp_i2c_init);
module_exit(bmp_i2c_exit);
/*@}*/
