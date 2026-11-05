// SPDX-License-Identifier: GPL-2.0
//
// cs40l26-spi.c -- CS40L26 SPI Driver
//
// Copyright 2022 Cirrus Logic, Inc.
//
// Author: Fred Treven <fred.treven@cirrus.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include <linux/mfd/cs40l26.h>

static const struct spi_device_id cs40l26_id_spi[] = {
	{"cs40l26a", 0},
	{"cs40l26b", 1},
	{"cs40l27a", 2},
	{"cs40l27b", 3},
	{}
};
MODULE_DEVICE_TABLE(spi, cs40l26_id_spi);

static const struct of_device_id cs40l26_of_match[CS40L26_NUM_DEVS + 1] = {
	{ .compatible = "cirrus,cs40l26a" },
	{ .compatible = "cirrus,cs40l26b" },
	{ .compatible = "cirrus,cs40l27a" },
	{ .compatible = "cirrus,cs40l27b" },
	{}
};
MODULE_DEVICE_TABLE(of, cs40l26_of_match);

static int cs40l26_spi_probe(struct spi_device *spi)
{
	struct cs40l26_private *cs40l26;
	int error;

	cs40l26 = devm_kzalloc(&spi->dev, sizeof(struct cs40l26_private), GFP_KERNEL);
	if (!cs40l26)
		return -ENOMEM;

	spi_set_drvdata(spi, cs40l26);

	cs40l26->regmap = devm_regmap_init_spi(spi, &cs40l26_regmap);
	if (IS_ERR(cs40l26->regmap)) {
		error = PTR_ERR(cs40l26->regmap);
		dev_err(&spi->dev, "Failed to allocate register map: %d\n", error);
		return error;
	}

	cs40l26->dev = &spi->dev;
	cs40l26->irq = spi->irq;

	return cs40l26_probe(cs40l26);
}

static void cs40l26_spi_remove(struct spi_device *spi)
{
	struct cs40l26_private *cs40l26 = spi_get_drvdata(spi);

	cs40l26_remove(cs40l26);
}

static struct spi_driver cs40l26_spi_driver = {
	.driver = {
		.name = "cs40l26",
		.of_match_table = cs40l26_of_match,
		.pm = &cs40l26_pm_ops,
	},

	.id_table = cs40l26_id_spi,
	.probe = cs40l26_spi_probe,
	.remove = cs40l26_spi_remove,
};

module_spi_driver(cs40l26_spi_driver);

MODULE_DESCRIPTION("CS40L26 SPI Driver");
MODULE_AUTHOR("Fred Treven, Cirrus Logic Inc. <fred.treven@cirrus.com>");
MODULE_LICENSE("GPL");
