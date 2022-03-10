// SPDX-License-Identifier: GPL-2.0

/*
 * cs35l43-spi.c -- CS35l41 SPI driver
 *
 * Copyright 2017 Cirrus Logic, Inc.
 *
 * Author:	David Rhodes	<david.rhodes@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/regulator/consumer.h>

#include "wm_adsp.h"
#include "cs35l43.h"
#include <sound/cs35l43.h>

static struct regmap_config cs35l43_regmap_spi = {
	.reg_bits = 32,
	.val_bits = 32,
	.pad_bits = 16,
	.reg_stride = 4,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.val_format_endian = REGMAP_ENDIAN_BIG,
	.max_register = CS35L43_DSP1_PMEM_5114,
	.num_reg_defaults = 0,
	.volatile_reg = cs35l43_volatile_reg,
	.readable_reg = cs35l43_readable_reg,
	.precious_reg = cs35l43_precious_reg,
	.cache_type = REGCACHE_RBTREE,
};

static const struct spi_device_id cs35l43_id_spi[] = {
	{"cs35l43", 0},
	{}
};

MODULE_DEVICE_TABLE(spi, cs35l43_id_spi);

static int cs35l43_spi_probe(struct spi_device *spi)
{
	const struct regmap_config *regmap_config = &cs35l43_regmap_spi;
	struct cs35l43_platform_data *pdata =
					dev_get_platdata(&spi->dev);
	struct cs35l43_private *cs35l43;
	int ret;

	cs35l43 = devm_kzalloc(&spi->dev,
			       sizeof(struct cs35l43_private),
			       GFP_KERNEL);
	if (cs35l43 == NULL)
		return -ENOMEM;


	spi_set_drvdata(spi, cs35l43);
	cs35l43->regmap = devm_regmap_init_spi(spi, regmap_config);
	if (IS_ERR(cs35l43->regmap)) {
		ret = PTR_ERR(cs35l43->regmap);
		dev_info(&spi->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	cs35l43->dev = &spi->dev;
	cs35l43->irq = spi->irq;

	return cs35l43_probe(cs35l43, pdata);
}

static int cs35l43_spi_remove(struct spi_device *spi)
{
	struct cs35l43_private *cs35l43 = spi_get_drvdata(spi);

	return cs35l43_remove(cs35l43);
}

static const struct of_device_id cs35l43_of_match[] = {
	{.compatible = "cirrus,cs35l43"},
	{},
};
MODULE_DEVICE_TABLE(of, cs35l43_of_match);

static struct spi_driver cs35l43_spi_driver = {
	.driver = {
		.name		= "cs35l43",
		.of_match_table = cs35l43_of_match,
	},
	.id_table	= cs35l43_id_spi,
	.probe		= cs35l43_spi_probe,
	.remove		= cs35l43_spi_remove,
};

module_spi_driver(cs35l43_spi_driver);

MODULE_DESCRIPTION("SPI CS35L43 driver");
MODULE_AUTHOR("David Rhodes, Cirrus Logic Inc, <david.rhodes@cirrus.com>");
MODULE_LICENSE("GPL");
