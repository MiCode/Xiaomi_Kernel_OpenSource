/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#define DEV_NAME "ir-learning"
#define SPI_BUF_LEN (256*1024)

struct mt_irlearning {
	unsigned int spi_clock; /* SPI clock source */
	unsigned int spi_hz; /* SPI clock output */
	unsigned int spi_data_invert;
	unsigned int spi_cs_invert;
	void *spi_buffer;
	struct spi_device *spi_dev;
};

#define SPI_IOC_READ_WAVE       _IOR('k', 1, __u8)
#define SPI_IOC_GET_SAMPLE_RATE _IOR('k', 2, __u8)
