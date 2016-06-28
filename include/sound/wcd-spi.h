/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __WCD_SPI_H__
#define __WCD_SPI_H__

struct wcd_spi_msg {
	/*
	 * Caller's buffer pointer that holds data to
	 * be transmitted in case of data_write and
	 * data to be copied to in case of data_read.
	 */
	void *data;

	/* Length of data to write/read */
	size_t len;

	/*
	 * Address in remote memory to write to
	 * or read from.
	 */
	u32 remote_addr;

	/* Bitmask of flags, currently unused */
	u32 flags;
};

#ifdef CONFIG_SND_SOC_WCD_SPI

int wcd_spi_data_write(struct spi_device *spi, struct wcd_spi_msg *msg);
int wcd_spi_data_read(struct spi_device *spi, struct wcd_spi_msg *msg);

#else

int wcd_spi_data_write(struct spi_device *spi, struct wcd_spi_msg *msg)
{
	return -ENODEV;
}

int wcd_spi_data_read(struct spi_device *spi, struct wcd_spi_msg *msg)
{
	return -ENODEV;
}

#endif /* End of CONFIG_SND_SOC_WCD_SPI */

#endif /* End of __WCD_SPI_H__ */
