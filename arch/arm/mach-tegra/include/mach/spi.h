/*
 * arch/arm/mach-tegra/include/mach/spi.h
 *
 * Copyright (c) 2010, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __MACH_TEGRA_SPI_H
#define __MACH_TEGRA_SPI_H

#include <linux/types.h>
#include <linux/spi/spi.h>

typedef int	(*callback)(void *client_data);

/**
 * register_spi_slave_callback - registers notification callback provided by
 * the client.
 * This callback indicate that the controller is all set to receive/transfer
 * data.
 * @spi: struct spi_device - refer to linux/spi/spi.h
 * @func: Callback function
 * @client_data: Data to be passed in callback
 * Context: can not sleep
 */
int spi_tegra_register_callback(struct spi_device *spi, callback func,
	void *client_data);

#endif
