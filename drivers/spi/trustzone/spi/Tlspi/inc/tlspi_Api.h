/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TLSPI_H_
#define TLSPI_H_

#include "tci.h"
#include "spi.h"

/*
 * Command ID's for communication Trustlet Connector -> Trustlet.
 */
#define CMD_SPI_SEND		1
#define CMD_SPI_CONFIG		2
#define CMD_SPI_DEBUG		3
#define CMD_SPI_TEST		4

/*
 * Termination codes
 */
#define EXIT_ERROR			((uint32_t)(-1))

/*
 * command message.
 *
 * @param len Length of the data to process.
 * @param data Data to processed (cleartext or ciphertext).
 */
struct tl_cmd_t {
	    /*< Command header */
		struct tciCommandHeader_t	header;
		/*< Length of data to process or buffer */
		uint32_t			len;
		/**< Length of response buffer */
		uint32_t			respLen;
};
/*
 * Response structure Trustlet -> Trustlet Connector.
 */
struct tl_rsp_t {
	struct tciResponseHeader_t	header;	/*< Response header */
	uint32_t	len;
};
/*
 * TCI message data.
 */
struct tciSpiMessage_t {
	union {
		struct tl_cmd_t		cmd_spi;
		struct tl_rsp_t		rsp_spi;
	};
	const void	*tx_buf;
	void		*rx_buf;
	uint32_t	len;
	uint32_t	is_dma_used;

	uint32_t	tx_dma;	/*dma_addr_t*/
	uint32_t	rx_dma;	/*dma_addr_t*/
	uint32_t	is_transfer_end;

	struct mt_tl_chip_conf	*tl_chip_config;

	uint32_t	cs_change;
	/*uint8_t	bits_per_word;*/
	/*uint16_t	delay_usecs;*/
	/*uint32_t	speed_hz;*/
};

/*
 * Trustlet UUID.
 */
#define TL_SPI_UUID {0x09, 0x15, 0x00, 0x00, 0x00, 0x00, \
						   0x00, 0x00, 0x00, \
						   0x00, 0x00, 0x00, \
						   0x00, 0x00, 0x00, 0x00}

#endif/*TLSPI_H_*/
