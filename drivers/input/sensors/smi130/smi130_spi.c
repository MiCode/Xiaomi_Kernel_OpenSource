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
 * @filename smi130_spi.c
 * @date     2014/11/25 14:40
 * @Modification Date 2018/08/28 18:20
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
#include "smi130_driver.h"

/*! @defgroup smi130_spi_src
 *  @brief smi130 spi driver module
 @{*/
/*! the maximum of transfer buffer size */
#define SMI_MAX_BUFFER_SIZE      32

static struct spi_device *smi_spi_client;

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
static char smi_spi_write_block(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	struct spi_device *client = smi_spi_client;
	u8 buffer[SMI_MAX_BUFFER_SIZE + 1];
	struct spi_transfer xfer = {
		.tx_buf     = buffer,
		.len        = len + 1,
	};
	struct spi_message msg;

	if (len > SMI_MAX_BUFFER_SIZE)
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
static char smi_spi_read_block(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	struct spi_device *client = smi_spi_client;
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

s8 smi_burst_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u16 len)
{
	struct spi_device *client = smi_spi_client;
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
EXPORT_SYMBOL(smi_burst_read_wrapper);
/*!
 * @brief SMI probe function via spi bus
 *
 * @param client the pointer of spi client
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int smi_spi_probe(struct spi_device *client)
{
	int status;
	int err = 0;
	struct smi_client_data *client_data = NULL;

	if (NULL == smi_spi_client)
		smi_spi_client = client;
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

	client_data = kzalloc(sizeof(struct smi_client_data), GFP_KERNEL);
	if (NULL == client_data) {
		dev_err(&client->dev, "no memory available");
		err = -ENOMEM;
		goto exit_err_clean;
	}

	client_data->device.bus_read = smi_spi_read_block;
	client_data->device.bus_write = smi_spi_write_block;

	return smi_probe(client_data, &client->dev);

exit_err_clean:
	if (err)
		smi_spi_client = NULL;
	return err;
}

/*!
 * @brief shutdown smi device in spi driver
 *
 * @param client the pointer of spi client
 *
 * @return no return value
*/
static void smi_spi_shutdown(struct spi_device *client)
{
#ifdef CONFIG_PM
	smi_suspend(&client->dev);
#endif
}

/*!
 * @brief remove smi spi client
 *
 * @param client the pointer of spi client
 *
 * @return zero
 * @retval zero
*/
static int smi_spi_remove(struct spi_device *client)
{
	int err = 0;
	err = smi_remove(&client->dev);
	smi_spi_client = NULL;

	return err;
}

#ifdef CONFIG_PM
/*!
 * @brief suspend smi device in spi driver
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
static int smi_spi_suspend(struct device *dev)
{
	int err = 0;
	err = smi_suspend(dev);
	return err;
}

/*!
 * @brief resume smi device in spi driver
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
static int smi_spi_resume(struct device *dev)
{
	int err = 0;
	/* post resume operation */
	err = smi_resume(dev);

	return err;
}

/*!
 * @brief register spi device power manager hooks
*/
static const struct dev_pm_ops smi_spi_pm_ops = {
	/**< device suspend */
	.suspend = smi_spi_suspend,
	/**< device resume */
	.resume  = smi_spi_resume
};
#endif

/*!
 * @brief register spi device id
*/
static const struct spi_device_id smi_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, smi_id);

/*!
 * @brief register spi driver hooks
*/
static struct spi_driver smi_spi_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = SENSOR_NAME,
#ifdef CONFIG_PM
		.pm = &smi_spi_pm_ops,
#endif
	},
	.id_table = smi_id,
	.probe    = smi_spi_probe,
	.shutdown = smi_spi_shutdown,
	.remove   = smi_spi_remove
};

/*!
 * @brief initialize smi spi module
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int __init smi_spi_init(void)
{
	return spi_register_driver(&smi_spi_driver);
}

/*!
 * @brief remove smi spi module
 *
 * @return no return value
*/
static void __exit smi_spi_exit(void)
{
	spi_unregister_driver(&smi_spi_driver);
}


MODULE_AUTHOR("Contact <contact@bosch-sensortec.com>");
MODULE_DESCRIPTION("SMI130 SPI DRIVER");
MODULE_LICENSE("GPL v2");

module_init(smi_spi_init);
module_exit(smi_spi_exit);
/*@}*/

