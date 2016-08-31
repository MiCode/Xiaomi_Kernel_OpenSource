/*
 * Define the OF_DEV_AUXDATA for different Tegra controllers.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef MACH_TEGRA_OF_DEV_AUXDATA_H__
#define MACH_TEGRA_OF_DEV_AUXDATA_H__

#ifdef CONFIG_USE_OF

#define T114_SPI_OF_DEV_AUXDATA	\
	OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000d400, "spi-tegra114.0", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000d600, "spi-tegra114.1", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000d800, "spi-tegra114.2", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000da00, "spi-tegra114.3", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000dc00, "spi-tegra114.4", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000de00, "spi-tegra114.5", NULL)

#define T114_I2C_OF_DEV_AUXDATA \
        OF_DEV_AUXDATA("nvidia,tegra114-i2c", 0x7000c000, "tegra11-i2c.0", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra114-i2c", 0x7000c400, "tegra11-i2c.1", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra114-i2c", 0x7000c500, "tegra11-i2c.2", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra114-i2c", 0x7000c700, "tegra11-i2c.3", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra114-i2c", 0x7000d000, "tegra11-i2c.4", NULL)


#define T124_SPI_OF_DEV_AUXDATA	T114_SPI_OF_DEV_AUXDATA

#define T124_I2C_OF_DEV_AUXDATA \
        OF_DEV_AUXDATA("nvidia,tegra124-i2c", 0x7000c000, "tegra12-i2c.0", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra124-i2c", 0x7000c400, "tegra12-i2c.1", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra124-i2c", 0x7000c500, "tegra12-i2c.2", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra124-i2c", 0x7000c700, "tegra12-i2c.3", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra124-i2c", 0x7000d000, "tegra12-i2c.4", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra124-i2c", 0x7000d100, "tegra12-i2c.5", NULL)

#endif

#endif /* MACH_TEGRA_OF_DEV_AUXDATA_H__ */
