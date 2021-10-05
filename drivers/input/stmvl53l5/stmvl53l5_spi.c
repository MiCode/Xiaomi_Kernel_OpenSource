/*
* This file is part of VL53L5 Kernel Driver
*
* Copyright (C) 2020, STMicroelectronics - All Rights Reserved
* Copyright (C) 2021 XiaoMi, Inc.
*
* License terms: STMicroelectronics Proprietary in accordance with licensing
* terms at www.st.com/sla0081
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*/

#include <linux/spi/spi.h>
#include <stddef.h>

#include "stmvl53l5_spi.h"


#define SPI_READWRITE_BIT 0x8000

#define SPI_WRITE_MASK(x) (x | SPI_READWRITE_BIT)
#define SPI_READ_MASK(x)  (x & ~SPI_READWRITE_BIT)

int stmvl53l5_spi_write(struct spi_data_t *spi_data, int index, uint8_t *data, uint16_t len)
{
	int status = 0;
	uint8_t index_bytes[2] = {0};
	struct spi_message m;
	struct spi_transfer t[2];

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	index_bytes[0] = ((SPI_WRITE_MASK(index) & 0xff00) >> 8);
	index_bytes[1] = (SPI_WRITE_MASK(index) & 0xff);

	t[0].tx_buf = index_bytes;
	t[0].len = 2;

	t[1].tx_buf = data;
	t[1].len = (unsigned int)len;

	spi_message_add_tail(&t[0], &m);
	spi_message_add_tail(&t[1], &m);

	status = spi_sync(spi_data->device, &m);
	if (status != 0) {
		printk("stmvl53l5 : spi_sync failed. %d", status);
		goto out;
	}

out:
	return status;
}

int stmvl53l5_spi_read(struct spi_data_t *spi_data, int index, uint8_t *data, uint16_t len)
{
	int status = 0;
	uint8_t index_bytes[2] = {0};
	struct spi_message m;
	struct spi_transfer t[2];

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	index_bytes[0] = ((SPI_READ_MASK(index) >> 8) & 0xff);
	index_bytes[1] = (SPI_READ_MASK(index) & 0xff);

	t[0].tx_buf = index_bytes;
	t[0].len = 2;

	t[1].rx_buf = data;
	t[1].len = (unsigned int)len;

	spi_message_add_tail(&t[0], &m);
	spi_message_add_tail(&t[1], &m);

	status = spi_sync(spi_data->device, &m);
	if (status != 0) {
		printk("stmvl53l5 : spi_sync failed. %d", status);
		goto out;
	}

out:
	return status;
}
