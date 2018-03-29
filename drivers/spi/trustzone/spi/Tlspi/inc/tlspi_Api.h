/*
* Copyright (c) 2013 TRUSTONIC LIMITED
* All rights reserved
* The present software is the confidential and proprietary information of
* TRUSTONIC LIMITED. You shall not disclose the present software and shall
* use it only in accordance with the terms of the license agreement you
* entered into with TRUSTONIC LIMITED. This software may be subject to
* export or import laws in certain countries.
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
typedef struct {
		tciCommandHeader_t	header;		/*< Command header */
		uint32_t			len;		/*< Length of data to process or buffer */
		uint32_t			respLen;	/**< Length of response buffer */
} tl_cmd_t;
/*
* Response structure Trustlet -> Trustlet Connector.
*/
typedef struct {
	tciResponseHeader_t	header;	/*< Response header */
	uint32_t	len;
} tl_rsp_t;
/*
* TCI message data.
*/
typedef struct {
	union {
		tl_cmd_t		cmd_spi;
		tl_rsp_t		rsp_spi;
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
} tciSpiMessage_t;

/*
* Trustlet UUID.
*/
#define TL_SPI_UUID {0x09, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

#endif/*TLSPI_H_*/
