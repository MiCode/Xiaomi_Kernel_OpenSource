/*  Copyright (c) 2011  Bosch Sensortec GmbH
    Copyright (c) 2011  Unixphere

    Based on:
    BMP085 driver, bmp085.c
    Copyright (c) 2010  Christoph Mair <christoph.mair@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/module.h>
#include <linux/i2c.h>
#include "bmp18x.h"

static int bmp18x_i2c_read_block(void *client, u8 reg, int len, char *buf)
{
	return i2c_smbus_read_i2c_block_data(client, reg, len, buf);
}

static int bmp18x_i2c_read_byte(void *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static int bmp18x_i2c_write_byte(void *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static const struct bmp18x_bus_ops bmp18x_i2c_bus_ops = {
	.read_block	= bmp18x_i2c_read_block,
	.read_byte	= bmp18x_i2c_read_byte,
	.write_byte	= bmp18x_i2c_write_byte
};

static int __devinit bmp18x_i2c_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	struct bmp18x_data_bus data_bus = {
		.bops = &bmp18x_i2c_bus_ops,
		.client = client
	};

	return bmp18x_probe(&client->dev, &data_bus);
}

static void bmp18x_i2c_shutdown(struct i2c_client *client)
{
	bmp18x_disable(&client->dev);
}

static int bmp18x_i2c_remove(struct i2c_client *client)
{
	return bmp18x_remove(&client->dev);
}

#ifdef CONFIG_PM
static int bmp18x_i2c_suspend(struct device *dev)
{
	return bmp18x_disable(dev);
}

static int bmp18x_i2c_resume(struct device *dev)
{
	return bmp18x_enable(dev);
}

static const struct dev_pm_ops bmp18x_i2c_pm_ops = {
	.suspend	= bmp18x_i2c_suspend,
	.resume		= bmp18x_i2c_resume
};
#endif

static const struct i2c_device_id bmp18x_id[] = {
	{ BMP18X_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bmp18x_id);

static struct i2c_driver bmp18x_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= BMP18X_NAME,
#ifdef CONFIG_PM
		.pm	= &bmp18x_i2c_pm_ops,
#endif
	},
	.id_table	= bmp18x_id,
	.probe		= bmp18x_i2c_probe,
	.shutdown	= bmp18x_i2c_shutdown,
	.remove		= __devexit_p(bmp18x_i2c_remove)
};

static int __init bmp18x_i2c_init(void)
{
	return i2c_add_driver(&bmp18x_i2c_driver);
}

static void __exit bmp18x_i2c_exit(void)
{
	i2c_del_driver(&bmp18x_i2c_driver);
}


MODULE_AUTHOR("Eric Andersson <eric.andersson@unixphere.com>");
MODULE_DESCRIPTION("BMP18X I2C bus driver");
MODULE_LICENSE("GPL");

module_init(bmp18x_i2c_init);
module_exit(bmp18x_i2c_exit);
