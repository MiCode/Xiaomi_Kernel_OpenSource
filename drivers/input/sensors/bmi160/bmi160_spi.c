/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi160_spi.c
 * @date     2014/11/25 14:40
 * @id       "20f77db"
 * @version  1.3
 *
 * @brief
 * This file implements moudle function, which add
 * the driver to SPI core.
*/

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include "bmi160_driver.h"

/*! @defgroup bmi160_spi_src
 *  @brief bmi160 spi driver module
 @{*/
/*! the maximum of transfer buffer size */
#define BMI_MAX_BUFFER_SIZE      32

static struct spi_device *bmi_spi_client;

/*!
 * @brief define spi wirte function
 *
 * @param dev_addr sensor device address
 * @param reg_addr register address
 * @param data the pointer of data buffer
 * @param len block size need to write
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static char bmi_spi_write_block(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	struct spi_device *client = bmi_spi_client;
	u8 buffer[BMI_MAX_BUFFER_SIZE + 1];
	struct spi_transfer xfer = {
		.tx_buf     = buffer,
		.len        = len + 1,
	};
	struct spi_message msg;

	if (len > BMI_MAX_BUFFER_SIZE)
		return -EINVAL;

	buffer[0] = reg_addr&0x7F;/* write: MSB = 0 */
	memcpy(&buffer[1], data, len);

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	return spi_sync(client, &msg);
}

/*!
 * @brief define spi read function
 *
 * @param dev_addr sensor device address
 * @param reg_addr register address
 * @param data the pointer of data buffer
 * @param len block size need to read
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static char bmi_spi_read_block(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	struct spi_device *client = bmi_spi_client;
	u8 reg = reg_addr | 0x80;/* read: MSB = 1 */
	struct spi_transfer xfer[2] = {
		[0] = {
			.tx_buf = &reg,
			.len = 1,
		},
		[1] = {
			.rx_buf = data,
			.len = len,
		}
	};
	struct spi_message msg;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer[0], &msg);
	spi_message_add_tail(&xfer[1], &msg);
	return spi_sync(client, &msg);
}

s8 bmi_burst_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u16 len)
{
	struct spi_device *client = bmi_spi_client;
	u8 reg = reg_addr | 0x80;/* read: MSB = 1 */
	struct spi_transfer xfer[2] = {
		[0] = {
			.tx_buf = &reg,
			.len = 1,
		},
		[1] = {
			.rx_buf = data,
			.len = len,
		}
	};
	struct spi_message msg;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer[0], &msg);
	spi_message_add_tail(&xfer[1], &msg);
	return spi_sync(client, &msg);
}
EXPORT_SYMBOL(bmi_burst_read_wrapper);
/*!
 * @brief BMI probe function via spi bus
 *
 * @param client the pointer of spi client
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmi_spi_probe(struct spi_device *client)
{
	int status;
	int err = 0;
	struct bmi_client_data *client_data = NULL;

	if (NULL == bmi_spi_client)
		bmi_spi_client = client;
	else{
		dev_err(&client->dev, "This driver does not support multiple clients!\n");
		return -EBUSY;
	}

	client->bits_per_word = 8;
	status = spi_setup(client);
	if (status < 0) {
		dev_err(&client->dev, "spi_setup failed!\n");
		return status;
	}

	client_data = kzalloc(sizeof(struct bmi_client_data), GFP_KERNEL);
	if (NULL == client_data) {
		dev_err(&client->dev, "no memory available");
		err = -ENOMEM;
		goto exit_err_clean;
	}

	client_data->device.bus_read = bmi_spi_read_block;
	client_data->device.bus_write = bmi_spi_write_block;

	return bmi_probe(client_data, &client->dev);

exit_err_clean:
	if (err)
		bmi_spi_client = NULL;
	return err;
}

/*!
 * @brief shutdown bmi device in spi driver
 *
 * @param client the pointer of spi client
 *
 * @return no return value
*/
static void bmi_spi_shutdown(struct spi_device *client)
{
#ifdef CONFIG_PM
	bmi_suspend(&client->dev);
#endif
}

/*!
 * @brief remove bmi spi client
 *
 * @param client the pointer of spi client
 *
 * @return zero
 * @retval zero
*/
static int bmi_spi_remove(struct spi_device *client)
{
	int err = 0;
	err = bmi_remove(&client->dev);
	bmi_spi_client = NULL;

	return err;
}

#ifdef CONFIG_PM
/*!
 * @brief suspend bmi device in spi driver
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
static int bmi_spi_suspend(struct device *dev)
{
	int err = 0;
	err = bmi_suspend(dev);
	return err;
}

/*!
 * @brief resume bmi device in spi driver
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
static int bmi_spi_resume(struct device *dev)
{
	int err = 0;
	/* post resume operation */
	err = bmi_resume(dev);

	return err;
}

/*!
 * @brief register spi device power manager hooks
*/
static const struct dev_pm_ops bmi_spi_pm_ops = {
	/**< device suspend */
	.suspend = bmi_spi_suspend,
	/**< device resume */
	.resume  = bmi_spi_resume
};
#endif

/*!
 * @brief register spi device id
*/
static const struct spi_device_id bmi_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, bmi_id);

/*!
 * @brief register spi driver hooks
*/
static struct spi_driver bmi_spi_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = SENSOR_NAME,
#ifdef CONFIG_PM
		.pm = &bmi_spi_pm_ops,
#endif
	},
	.id_table = bmi_id,
	.probe    = bmi_spi_probe,
	.shutdown = bmi_spi_shutdown,
	.remove   = bmi_spi_remove
};

/*!
 * @brief initialize bmi spi module
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int __init bmi_spi_init(void)
{
	return spi_register_driver(&bmi_spi_driver);
}

/*!
 * @brief remove bmi spi module
 *
 * @return no return value
*/
static void __exit bmi_spi_exit(void)
{
	spi_unregister_driver(&bmi_spi_driver);
}


MODULE_AUTHOR("Contact <contact@bosch-sensortec.com>");
MODULE_DESCRIPTION("BMI160 SPI DRIVER");
MODULE_LICENSE("GPL v2");

module_init(bmi_spi_init);
module_exit(bmi_spi_exit);
/*@}*/

