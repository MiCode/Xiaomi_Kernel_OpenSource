
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include "bmp280_core.h"
#include "bs_log.h"

#define BMP_MAX_RETRY_SPI_XFER		10
#define BMP_SPI_WRITE_DELAY_TIME	1

static struct spi_device *bmp_spi_client;

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

static char bmp_spi_read_block(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	struct spi_device *client = bmp_spi_client;
	u8 buffer[2] = {reg_addr, 0};
	int status = spi_write(client, buffer, 2);
	if (status < 0)
		return status;

	return spi_read(client, data, len);
}

static const struct bmp_bus_ops bmp_spi_bus_ops = {
	.bus_write	= bmp_spi_write_block,
	.bus_read	= bmp_spi_read_block
};

static int __devinit bmp_spi_probe(struct spi_device *client)
{
	int status;
	struct bmp_data_bus data_bus = {
		.bops = &bmp_spi_bus_ops,
		.client = client
	};

	if (NULL == bmp_spi_client)
		bmp_spi_client = client;
	else{
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

static void bmp_spi_shutdown(struct spi_device *client)
{
#ifdef CONFIG_PM
	bmp_disable(&client->dev);
#endif
}

static int bmp_spi_remove(struct spi_device *client)
{
	return bmp_remove(&client->dev);
}

#ifdef CONFIG_PM
static int bmp_spi_suspend(struct device *dev)
{
	return bmp_disable(dev);
}

static int bmp_spi_resume(struct device *dev)
{
	return bmp_enable(dev);
}

static const struct dev_pm_ops bmp_spi_pm_ops = {
	.suspend	= bmp_spi_suspend,
	.resume		= bmp_spi_resume
};
#endif

static const struct spi_device_id bmp_id[] = {
	{ BMP_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, bmp_id);

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

static int __init bmp_spi_init(void)
{
	return spi_register_driver(&bmp_spi_driver);
}

static void __exit bmp_spi_exit(void)
{
	spi_unregister_driver(&bmp_spi_driver);
}


MODULE_DESCRIPTION("BMP280 SPI BUS DRIVER");
MODULE_LICENSE("GPL");

module_init(bmp_spi_init);
module_exit(bmp_spi_exit);
/*@}*/
