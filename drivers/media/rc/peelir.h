/*
 * Copyright (C) Peel Technologies Inc
 * Copyright (C) 2018 XiaoMi, Inc.
 */

#ifndef PEELIR_H
#define PEELIR_H

#include <linux/types.h>


#define SPI_CPHA		0x01
#define SPI_CPOL		0x02

#define SPI_CS_HIGH		0x04
#define SPI_LSB_FIRST		0x08
#define SPI_3WIRE		0x10
#define SPI_LOOP		0x20
#define SPI_NO_CS		0x40
#define SPI_READY		0x80


#define SPI_IOC_MAGIC			'k'

struct spi_ioc_transfer {
	__u64		tx_buf;
	__u64		rx_buf;

	__u32		len;
	__u32		speed_hz;

	__u16		delay_usecs;
	__u8		bits_per_word;
	__u8		cs_change;

};

struct strIds {
	__u32		u32ID1;
	__u32		u32ID2;
	__u32		u32ID3;
};

/* Read/Write Message */
#define SPI_IOC_RD_MODE			_IOW(SPI_IOC_MAGIC, 1, __u8)
#define SPI_IOC_WR_MSG			_IOW(SPI_IOC_MAGIC, 2, __u8)
#define SPI_IOC_RD_MSG			_IOR(SPI_IOC_MAGIC, 3, __u8)
#define SPI_IOC_RD_IDS   		_IOR(SPI_IOC_MAGIC, 4, __u8)


#endif /* PEELIR_H */

