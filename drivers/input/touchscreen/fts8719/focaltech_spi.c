/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2019, FocalTech Systems, Ltd., all rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/************************************************************************
*
* File Name: focaltech_spi.c
*
*	Author: FocalTech Driver Team
*
*   Created: 2018-11-17
*
*  Abstract: spi communication with TP
*
*   Version: v1.1
*
* Revision History:
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "focaltech_core.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define STATUS_PACKAGE			  0x05
#define COMMAND_PACKAGE			 0xC0
#define DATA_PACKAGE				0x3F
#define BUSY_QUERY_TIMEOUT		  100
#define BUSY_QUERY_DELAY			150 /* unit: us */
#define CS_HIGH_DELAY			   150 /* unit: us */
#define DELAY_AFTER_FIRST_BYTE	  30
#define SPI_HEADER_LENGTH		   4
#define SPI_BUF_LENGTH			  256

#define DATA_CRC_EN				 0x20
#define WRITE_CMD				   0x00
#define READ_CMD					(0x80 | DATA_CRC_EN)

#define CD_PACKAGE_BUFLEN		   4

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/

/*****************************************************************************
* Static variables
*****************************************************************************/

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/

/*****************************************************************************
* functions body
*****************************************************************************/
/* spi interface */
static int fts_spi_transfer(u8 *tx_buf, u8 *rx_buf, u32 len)
{
	int ret = 0;
	struct spi_device *spi = fts_data->spi;
	struct spi_message msg;
	struct spi_transfer xfer[2];

	if (!spi || !tx_buf || !rx_buf || !len) {
		FTS_ERROR("spi_device/tx_buf/rx_buf/len(%d) is invalid", len);
		return -EINVAL;
	}

	memset(&xfer[0], 0, sizeof(struct spi_transfer));
	memset(&xfer[1], 0, sizeof(struct spi_transfer));

	spi_message_init(&msg);
	xfer[0].tx_buf = &tx_buf[0];
	xfer[0].len = 1;
	xfer[0].delay_usecs = DELAY_AFTER_FIRST_BYTE;
	spi_message_add_tail(&xfer[0], &msg);

	if (len > CD_PACKAGE_BUFLEN) {
		xfer[1].tx_buf = &tx_buf[CD_PACKAGE_BUFLEN];
		xfer[1].rx_buf = &rx_buf[CD_PACKAGE_BUFLEN];
		xfer[1].len = len - CD_PACKAGE_BUFLEN;
		spi_message_add_tail(&xfer[1], &msg);
	}

	ret = spi_sync(spi, &msg);
	if (ret) {
		FTS_ERROR("spi_sync fail,ret:%d", ret);
		return ret;
	}

	udelay(CS_HIGH_DELAY);
	return ret;
}

static void crckermit(u8 *data, u16 len, u16 *crc_out)
{
	u16 i = 0;
	u16 j = 0;
	u16 crc = 0xFFFF;

	for (i = 0; i < len; i++) {
		crc ^= data[i];
		for (j = 0; j < 8; j++) {
			if (crc & 0x01)
				crc = (crc >> 1) ^ 0x8408;
			else
				crc = (crc >> 1);
		}
	}

	*crc_out = crc;
}

static int rdata_check(u8 *rdata, u32 rlen)
{
	u16 crc_calc = 0;
	u16 crc_read = 0;

	crckermit(rdata, rlen - 2, &crc_calc);
	crc_read = (u16)(rdata[rlen - 1] << 8) + rdata[rlen - 2];
	if (crc_calc != crc_read) {
		return -EIO;
	}

	return 0;
}

static int fts_wait_idle(void)
{
	int ret = 0;
	int i = 0;
	int status = 0xFF;
	int idle_status = 0x01;
	int status_mask = 0x81;
	int retry_timeout = BUSY_QUERY_TIMEOUT;
	u8 *txbuf = fts_data->bus_tx_buf;
	u8 *rxbuf = fts_data->bus_rx_buf;
	u32 txlen = 0;

	if (!fts_data->fw_is_running)
		idle_status = 0x00;

	memset(txbuf, 0x00, SPI_BUF_LENGTH);
	memset(rxbuf, 0x00, SPI_BUF_LENGTH);
	txbuf[0] = STATUS_PACKAGE;
	txlen = CD_PACKAGE_BUFLEN + 1;
	for (i = 0; i < retry_timeout; i++) {
		udelay(BUSY_QUERY_DELAY);
		ret = fts_spi_transfer(txbuf, rxbuf, txlen);
		if (ret >= 0) {
			status = (int)rxbuf[CD_PACKAGE_BUFLEN];
			if ((status & status_mask) == idle_status) {
				break;
			}
		}
	}

	if (i >= retry_timeout) {
		FTS_ERROR("spi is busy, status:0x%x", status);
		return -EIO;
	}

	return (int)status;
}

static int fts_cmdpkg_wirte(u8 ctrl, u8 *cmd, u32 cmdlen)
{
	int i = 0;
	u8 *txbuf = fts_data->bus_tx_buf;
	u8 *rxbuf = fts_data->bus_rx_buf;
	u32 txlen = 0;

	if ((!cmd) || (!cmdlen)
		|| (cmdlen > (FTX_MAX_COMMMAND_LENGTH - SPI_HEADER_LENGTH))) {
		FTS_ERROR("cmd/cmdlen(%d) is invalid", cmdlen);
		return -EINVAL;
	}

	txbuf[0] = COMMAND_PACKAGE;
	txlen = CD_PACKAGE_BUFLEN;
	txbuf[txlen++] = ctrl | (cmdlen & 0x0F);
	for (i = 0; i < cmdlen; i++) {
		txbuf[txlen++] = cmd[i];
	}

	return fts_spi_transfer(txbuf, rxbuf, txlen);
}

static int fts_boot_write(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;
	u8 *txbuf = NULL;
	u8 *rxbuf = NULL;
	u32 txlen = 0;
	u8 tmpcmd[FTX_MAX_COMMMAND_LENGTH] = { 0 };

	if ((!cmd) || (!cmdlen)
		|| (cmdlen > (FTX_MAX_COMMMAND_LENGTH - SPI_HEADER_LENGTH))) {
		FTS_ERROR("cmd/cmdlen(%d) is invalid", cmdlen);
		return -EINVAL;
	}

	mutex_lock(&ts_data->bus_lock);
	/* wait spi idle */
	ret = fts_wait_idle();
	if (ret < 0) {
		FTS_ERROR("wait spi idle fail");
		goto err_boot_write;
	}

	/* write cmd */
	memcpy(tmpcmd, cmd, cmdlen);
	if (ts_data->fw_is_running && data && datalen) {
		tmpcmd[cmdlen++] = (datalen >> 8) & 0xFF;
		tmpcmd[cmdlen++] = datalen & 0xFF;
	}
	ret = fts_cmdpkg_wirte(WRITE_CMD, tmpcmd, cmdlen);
	if (ret < 0) {
		FTS_ERROR("command package wirte fail");
		goto err_boot_write;
	}

	/* have data, transfer data */
	if (data && datalen) {
		/* wait spi idle */
		ret = fts_wait_idle();
		if (ret < 0) {
			FTS_ERROR("wait spi idle from cmd fail");
			goto err_boot_write;
		}

		/* write data */
		if (datalen > (SPI_BUF_LENGTH - SPI_HEADER_LENGTH)) {
			txbuf = kzalloc(datalen + SPI_HEADER_LENGTH, GFP_KERNEL);
			if (NULL == txbuf) {
				FTS_ERROR("txbuf malloc fail");
				ret = -ENOMEM;
				goto err_boot_write;
			}

			rxbuf = kzalloc(datalen + SPI_HEADER_LENGTH, GFP_KERNEL);
			if (NULL == rxbuf) {
				FTS_ERROR("rxbuf malloc fail");
				ret = -ENOMEM;
				goto err_boot_write;
			}
		} else {
			txbuf = ts_data->bus_tx_buf;
			rxbuf = ts_data->bus_rx_buf;
			memset(txbuf, 0x00, SPI_BUF_LENGTH);
			memset(rxbuf, 0x00, SPI_BUF_LENGTH);
		}

		txbuf[0] = DATA_PACKAGE;
		txlen = CD_PACKAGE_BUFLEN + datalen;
		memcpy(txbuf + CD_PACKAGE_BUFLEN, data, datalen);

		ret = fts_spi_transfer(txbuf, rxbuf, txlen);
		if (ret < 0) {
			FTS_ERROR("spi_transfer(wirte) fail");
		}
	}

err_boot_write:
	if (datalen > (SPI_BUF_LENGTH - SPI_HEADER_LENGTH)) {
		if (txbuf) {
			kfree(txbuf);
			txbuf = NULL;
		}

		if (rxbuf) {
			kfree(rxbuf);
			rxbuf = NULL;
		}
	}
	mutex_unlock(&ts_data->bus_lock);
	return ret;
}

int fts_write(u8 *writebuf, u32 writelen)
{
	u8 *cmd = NULL;
	u32 cmdlen = 0;
	u8 *data = NULL;
	u32 datalen = 0;

	if (!writebuf || !writelen) {
		FTS_ERROR("writebuf/len is invalid");
		return -EINVAL;
	}

	if (1 == writelen) {
		cmd = writebuf;
		cmdlen = 1;
		data = NULL;
		datalen = 0;
	} else {
		cmd = writebuf;
		cmdlen = 1;
		if (!fts_data->fw_is_running) {
			if ((cmd[0] == 0xAE) || (cmd[0] == 0x85) || (cmd[0] == 0xF2)) {
				cmdlen = 6;
			} else if (cmd[0] == 0xCC) {
				cmdlen = 7;
			}
		}
		data = writebuf + cmdlen;
		datalen = writelen - cmdlen;
	}

	return fts_boot_write(&cmd[0], cmdlen, data, datalen);
}

int fts_write_reg(u8 addr, u8 value)
{
	return fts_boot_write(&addr, 1, &value, 1);
}

int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;
	u8 *txbuf = NULL;
	u8 *rxbuf = NULL;
	u32 txlen = 0;
	u8 ctrl = READ_CMD;
	u8 tmpcmd[FTX_MAX_COMMMAND_LENGTH] = { 0 };
	u8 cmdaddr = -1;

	mutex_lock(&ts_data->bus_lock);
	if (cmd && cmdlen) {
		/* wait spi idle */
		ret = fts_wait_idle();
		if (ret < 0) {
			FTS_ERROR("wait spi idle fail");
			goto boot_read_err;
		}

		/* write cmd */
		cmdaddr = cmd[0];
		memcpy(tmpcmd, cmd, cmdlen);
		if (ts_data->fw_is_running) {
			tmpcmd[cmdlen++] = (datalen >> 8) & 0xFF;
			tmpcmd[cmdlen++] = datalen & 0xFF;
		}
		ret = fts_cmdpkg_wirte(ctrl, tmpcmd, cmdlen);
		if (ret < 0) {
			FTS_ERROR("command package wirte fail");
			goto boot_read_err;
		}

		/* wait spi idle */
		ret = fts_wait_idle();
		if (ret < 0) {
			FTS_ERROR("wait spi idle from cmd fail");
			goto boot_read_err;
		}
	}

	if (data && datalen) {
		/* write data */
		if (datalen > (SPI_BUF_LENGTH - SPI_HEADER_LENGTH)) {
			txbuf = kzalloc(datalen + SPI_HEADER_LENGTH, GFP_KERNEL);
			if (NULL == txbuf) {
				FTS_ERROR("txbuf malloc fail");
				ret = -ENOMEM;
				goto boot_read_err;
			}

			rxbuf = kzalloc(datalen + SPI_HEADER_LENGTH, GFP_KERNEL);
			if (NULL == rxbuf) {
				FTS_ERROR("rxbuf malloc fail");
				ret = -ENOMEM;
				goto boot_read_err;
			}
		} else {
			txbuf = ts_data->bus_tx_buf;
			rxbuf = ts_data->bus_rx_buf;
			memset(txbuf, 0x00, SPI_BUF_LENGTH);
			memset(rxbuf, 0x00, SPI_BUF_LENGTH);
		}

		txbuf[0] = DATA_PACKAGE;
		txlen = CD_PACKAGE_BUFLEN + datalen;
		if (ctrl & DATA_CRC_EN) {
			txlen = txlen + 2;
		}
		ret = fts_spi_transfer(txbuf, rxbuf, txlen);
		if (ret < 0) {
			FTS_ERROR("spi_transfer(read) fail");
			goto boot_read_err;
		}
		memcpy(data, rxbuf + CD_PACKAGE_BUFLEN, datalen);
		/* crc check */
		if (ctrl & DATA_CRC_EN) {
			ret = rdata_check(rxbuf + CD_PACKAGE_BUFLEN,
							  txlen - CD_PACKAGE_BUFLEN);
			if (ret < 0) {
				FTS_INFO("read data(addr:%x) crc check incorrect", cmdaddr);
				goto boot_read_err;
			}
		}
	}
boot_read_err:
	if (datalen > (SPI_BUF_LENGTH - SPI_HEADER_LENGTH)) {
		if (txbuf) {
			kfree(txbuf);
			txbuf = NULL;
		}

		if (rxbuf) {
			kfree(rxbuf);
			rxbuf = NULL;
		}
	}
	mutex_unlock(&ts_data->bus_lock);
	return ret;
}

int fts_read_reg(u8 addr, u8 *value)
{
	return fts_read(&addr, 1, value, 1);
}

int fts_bus_init(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	ts_data->bus_tx_buf = kzalloc(SPI_BUF_LENGTH, GFP_KERNEL);
	if (NULL == ts_data->bus_tx_buf) {
		FTS_ERROR("failed to allocate memory for bus_tx_buf");
		return -ENOMEM;
	}

	ts_data->bus_rx_buf = kzalloc(SPI_BUF_LENGTH, GFP_KERNEL);
	if (NULL == ts_data->bus_rx_buf) {
		FTS_ERROR("failed to allocate memory for bus_rx_buf");
		return -ENOMEM;
	}
	FTS_FUNC_EXIT();
	return 0;
}

int fts_bus_exit(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	if (ts_data && ts_data->bus_tx_buf) {
		kfree(ts_data->bus_tx_buf);
		ts_data->bus_tx_buf = NULL;
	}

	if (ts_data && ts_data->bus_rx_buf) {
		kfree(ts_data->bus_rx_buf);
		ts_data->bus_rx_buf = NULL;
	}
	FTS_FUNC_EXIT();
	return 0;
}
