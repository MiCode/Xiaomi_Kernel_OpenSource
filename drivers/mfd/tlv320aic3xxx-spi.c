/*
 * drivers/mfd/tlv320aic3xxx-spi.c
 *
 * Copyright (C) 2012-2013 NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#include <linux/mfd/tlv320aic3xxx-core.h>

struct regmap_config tlv320aic3xxx_spi_regmap = {
	.reg_bits = 7,
	.pad_bits = 1,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
	.read_flag_mask = 0x1,
};

#ifdef CONFIG_PM
static int aic3xxx_suspend(struct device *dev)
{
	struct aic3xxx *aic3xxx = dev_get_drvdata(dev);

	aic3xxx->suspended = true;

	return 0;
}

static int aic3xxx_resume(struct device *dev)
{
	struct aic3xxx *aic3xxx = dev_get_drvdata(dev);

	aic3xxx->suspended = false;

	return 0;
}
#endif


static int tlv320aic3xxx_spi_probe(struct spi_device *spi)
{
	const struct spi_device_id *id = spi_get_device_id(spi);
	struct aic3xxx *tlv320aic3xxx;
	const struct regmap_config *regmap_config;
	int ret;

	switch (id->driver_data) {
	case TLV320AIC3262:
		regmap_config = &tlv320aic3xxx_spi_regmap;
		break;
#ifdef CONFIG_MFD_AIC3285
	case TLV320AIC3285:
		regmap_config = &tlv320aic3285_spi_regmap;
		break;
#endif
	default:
		dev_err(&spi->dev, "Unknown device type %ld\n",
			id->driver_data);
		return -EINVAL;
	}

	tlv320aic3xxx =
		devm_kzalloc(&spi->dev, sizeof(struct aic3xxx), GFP_KERNEL);
	if (tlv320aic3xxx == NULL)
		return -ENOMEM;

	tlv320aic3xxx->regmap = devm_regmap_init_spi(spi, regmap_config);
	if (IS_ERR(tlv320aic3xxx->regmap)) {
		ret = PTR_ERR(tlv320aic3xxx->regmap);
		dev_err(&spi->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	tlv320aic3xxx->type = id->driver_data;
	tlv320aic3xxx->dev = &spi->dev;
	tlv320aic3xxx->irq = spi->irq;

	return aic3xxx_device_init(tlv320aic3xxx, tlv320aic3xxx->irq);
}

static int tlv320aic3xxx_spi_remove(struct spi_device *spi)
{
	struct aic3xxx *tlv320aic3xxx = dev_get_drvdata(&spi->dev);
	aic3xxx_device_exit(tlv320aic3xxx);
	return 0;
}

static const struct spi_device_id aic3xxx_spi_ids[] = {
	{"tlv320aic3xxx", TLV320AIC3262},
	{"tlv320aic3285", TLV320AIC3285},
	{ }
};
MODULE_DEVICE_TABLE(spi, aic3xxx_spi_ids);

static UNIVERSAL_DEV_PM_OPS(aic3xxx_pm_ops, aic3xxx_suspend, aic3xxx_resume,
				NULL);


static struct spi_driver tlv320aic3xxx_spi_driver = {
	.driver = {
		.name	= "tlv320aic3xxx",
		.owner	= THIS_MODULE,
		.pm	= &aic3xxx_pm_ops,
	},
	.probe		= tlv320aic3xxx_spi_probe,
	.remove		= tlv320aic3xxx_spi_remove,
	.id_table	= aic3xxx_spi_ids,
};

module_spi_driver(tlv320aic3xxx_spi_driver);

MODULE_DESCRIPTION("TLV320AIC3XXX SPI bus interface");
MODULE_AUTHOR("Mukund Navada <navada@ti.com>");
MODULE_LICENSE("GPL");
