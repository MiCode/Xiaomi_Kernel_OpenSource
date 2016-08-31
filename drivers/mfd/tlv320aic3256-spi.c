/*
 * tlv320aic325x-spi.c  -- driver for TLV320AIC3XXX
 *
 * Author:      Mukund Navada <navada@ti.com>
 *				Mehar Bajwa <mehar.bajwa@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#include <linux/mfd/tlv320aic325x-core.h>

struct regmap_config aic325x_spi_regmap = {
	.reg_bits = 7,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
	.read_flag_mask = 0x1,
	.pad_bits = 1,
};

static int tlv320aic325x_spi_probe(struct spi_device *spi)
{
	const struct spi_device_id *id = spi_get_device_id(spi);
	struct aic325x *aic325x;
	const struct regmap_config *regmap_config;
	int ret;

	regmap_config = &aic325x_spi_regmap;

	aic325x = devm_kzalloc(&spi->dev, sizeof(struct aic325x), GFP_KERNEL);
	if (aic325x == NULL)
		return -ENOMEM;

	aic325x->regmap = devm_regmap_init_spi(spi, regmap_config);
	if (IS_ERR(aic325x->regmap)) {
		ret = PTR_ERR(aic325x->regmap);
		dev_err(&spi->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	aic325x->type = id->driver_data;
	aic325x->dev = &spi->dev;
	aic325x->irq = spi->irq;

	return aic325x_device_init(aic325x);
}

static int tlv320aic325x_spi_remove(struct spi_device *spi)
{
	struct aic325x *aic325x = dev_get_drvdata(&spi->dev);
	aic325x_device_exit(aic325x);
	return 0;
}

static const struct spi_device_id aic325x_spi_ids[] = {
	{"tlv320aic3262", TLV320AIC3262},
	{"tlv320aic325x", TLV320AIC3256},
	{ }
};
MODULE_DEVICE_TABLE(spi, aic325x_spi_ids);

static struct spi_driver aic325x_spi_driver = {
	.driver = {
		.name	= "tlv320aic325x",
		.owner	= THIS_MODULE,
	},
	.probe		= tlv320aic325x_spi_probe,
	.remove		= tlv320aic325x_spi_remove,
	.id_table	= aic325x_spi_ids,
};

module_spi_driver(aic325x_spi_driver);

MODULE_DESCRIPTION("AIC3XXX SPI bus interface");
MODULE_AUTHOR("Mukund Navada <navada@ti.com>");
MODULE_LICENSE("GPL");
