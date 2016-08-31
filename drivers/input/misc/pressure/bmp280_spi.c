/*!
 * @section LICENSE
 * $license$
 *
 * @filename $filename$
 * @date	 $date$
 * @id		 $id$
 *
 * @brief
 * This file implements moudle function, which add
 * the driver to SPI core.
*/

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include "bmp280_core.h"
#include "bs_log.h"

/*! @defgroup bmp280_spi_src
 *  @brief bmp280 spi driver module
 @{*/
/*! the maximum of retry during SPI transfer */
#define BMP_MAX_RETRY_SPI_XFER		10
/*! delay time between two error transfers */
#define BMP_SPI_WRITE_DELAY_TIME	1

static struct spi_device *bmp_spi_client;

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
static char bmp_spi_write_block(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	struct spi_device *client = bmp_spi_client;
	u8 buffer[2];
	u8 retry;

	while (0 != len--) {
		buffer[0] = reg_addr;
		buffer[1] = *data;
		for (retry = 0; retry < BMP_MAX_RETRY_SPI_XFER; retry++) {
			if (spi_write(client, buffer, 2) >= 0)
				break;
			else
				mdelay(BMP_SPI_WRITE_DELAY_TIME);
		}
		if (BMP_MAX_RETRY_SPI_XFER <= retry) {
			PERR("SPI xfer error");
			return -EIO;
		}
		reg_addr++;
		data++;
	}

	return 0;
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
static char bmp_spi_read_block(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	struct spi_device *client = bmp_spi_client;
	u8 buffer[2] = {reg_addr, 0};
	int status = spi_write(client, buffer, 2);
	if (status < 0)
		return status;

	return spi_read(client, data, len);
}

/*!
 * @brief spi bus operation
*/
static const struct bmp_bus_ops bmp_spi_bus_ops = {
	/**< spi block write pointer */
	.bus_write	= bmp_spi_write_block,
	/**< spi block read pointer */
	.bus_read	= bmp_spi_read_block
};

/*!
 * @brief BMP probe function via spi bus
 *
 * @param client the pointer of spi client
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int __devinit bmp_spi_probe(struct spi_device *client)
{
	int status;
	struct bmp_data_bus data_bus = {
		.bops = &bmp_spi_bus_ops,
		.client = client
	};

	if (NULL == bmp_spi_client)
		bmp_spi_client = client;
	else {
		PERR("This driver does not support multiple clients!\n");
		return -EINVAL;
	}

	client->bits_per_word = 8;
	status = spi_setup(client);
	if (status < 0) {
		PERR("spi_setup failed!\n");
		return status;
	}

	return bmp_probe(&client->dev, &data_bus);
}

/*!
 * @brief shutdown bmp device in spi driver
 *
 * @param client the pointer of spi client
 *
 * @return no return value
*/
static void bmp_spi_shutdown(struct spi_device *client)
{
#ifdef CONFIG_PM
	bmp_disable(&client->dev);
#endif
}

/*!
 * @brief remove bmp spi client
 *
 * @param client the pointer of spi client
 *
 * @return zero
 * @retval zero
*/
static int bmp_spi_remove(struct spi_device *client)
{
	return bmp_remove(&client->dev);
}

#ifdef CONFIG_PM
/*!
 * @brief suspend bmp device in spi driver
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
static int bmp_spi_suspend(struct device *dev)
{
	return bmp_disable(dev);
}

/*!
 * @brief resume bmp device in spi driver
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
static int bmp_spi_resume(struct device *dev)
{
	return bmp_enable(dev);
}

/*!
 * @brief register spi device power manager hooks
*/
static const struct dev_pm_ops bmp_spi_pm_ops = {
	/**< device suspend */
	.suspend	= bmp_spi_suspend,
	/**< device resume */
	.resume		= bmp_spi_resume
};
#endif

/*!
 * @brief register spi device id
*/
static const struct spi_device_id bmp_id[] = {
	{ BMP_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, bmp_id);

/*!
 * @brief register spi driver hooks
*/
static struct spi_driver bmp_spi_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= BMP_NAME,
#ifdef CONFIG_PM
		.pm	= &bmp_spi_pm_ops,
#endif
	},
	.id_table	= bmp_id,
	.probe		= bmp_spi_probe,
	.shutdown	= bmp_spi_shutdown,
	.remove		= __devexit_p(bmp_spi_remove)
};

/*!
 * @brief initialize bmp spi module
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int __init bmp_spi_init(void)
{
	return spi_register_driver(&bmp_spi_driver);
}

/*!
 * @brief remove bmp spi module
 *
 * @return no return value
*/
static void __exit bmp_spi_exit(void)
{
	spi_unregister_driver(&bmp_spi_driver);
}


MODULE_DESCRIPTION("BMP280 SPI BUS DRIVER");
MODULE_LICENSE("GPL");

module_init(bmp_spi_init);
module_exit(bmp_spi_exit);
/*@}*/
