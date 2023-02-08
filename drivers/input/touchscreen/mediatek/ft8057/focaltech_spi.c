/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, FocalTech Systems, Ltd., all rights reserved.
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
*    Author: FocalTech Driver Team
*
*   Created: 2019-03-21
*
*  Abstract: new spi protocol communication with TP
*
*   Version: v1.0
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
#define SPI_RETRY_NUMBER            3
#define CS_HIGH_DELAY               150 /* unit: us */
#define SPI_BUF_LENGTH              4096

#define DATA_CRC_EN                 0x20
#define WRITE_CMD                   0x00
#define READ_CMD                    (0x80 | DATA_CRC_EN)

#define SPI_DUMMY_BYTE              3
#define SPI_HEADER_LENGTH           6   /*CRC*/
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
    struct spi_transfer xfer = {
        .tx_buf = tx_buf,
        .rx_buf = rx_buf,
        .len    = len,
    };

    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);

    ret = spi_sync(spi, &msg);
    if (ret) {
        FTS_ERROR("spi_sync fail,ret:%d", ret);
        return ret;
    }

    return ret;
}

static void fts_spi_buf_show(u8 *data, int datalen)
{
    int i = 0;
    int count = 0;
    int size = 0;
    char *tmpbuf = NULL;

    if (!data || (datalen <= 0)) {
        FTS_ERROR("data/datalen is invalid");
        return;
    }

    size = (datalen > 256) ? 256 : datalen;
    tmpbuf = kzalloc(1024, GFP_KERNEL);
    if (!tmpbuf) {
        FTS_ERROR("tmpbuf zalloc fail");
        return;
    }

    for (i = 0; i < size; i++)
        count += snprintf(tmpbuf + count, 1024 - count, "%02X ", data[i]);

    FTS_DEBUG("%s", tmpbuf);
    if (tmpbuf) {
        kfree(tmpbuf);
        tmpbuf = NULL;
    }
}

static void crckermit(u8 *data, u32 len, u16 *crc_out)
{
    u32 i = 0;
    u32 j = 0;
    u16 crc = 0xFFFF;

    for ( i = 0; i < len; i++) {
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
        fts_spi_buf_show(rdata, rlen);
        return -EIO;
    }

    return 0;
}

int fts_write(u8 *writebuf, u32 writelen)
{
    int ret = 0;
    int i = 0;
    struct fts_ts_data *ts_data = fts_data;
    u8 *txbuf = NULL;
    u8 *rxbuf = NULL;
    u32 txlen = 0;
    u32 txlen_need = writelen + SPI_HEADER_LENGTH + ts_data->dummy_byte;
    u32 datalen = writelen - 1;

    if (!writebuf || !writelen) {
        FTS_ERROR("writebuf/len is invalid");
        return -EINVAL;
    }

    mutex_lock(&ts_data->bus_lock);
    if (txlen_need > SPI_BUF_LENGTH) {
        txbuf = kzalloc(txlen_need, GFP_KERNEL);
        if (NULL == txbuf) {
            FTS_ERROR("txbuf malloc fail");
            ret = -ENOMEM;
            goto err_write;
        }

        rxbuf = kzalloc(txlen_need, GFP_KERNEL);
        if (NULL == rxbuf) {
            FTS_ERROR("rxbuf malloc fail");
            ret = -ENOMEM;
            goto err_write;
        }
    } else {
        txbuf = ts_data->bus_tx_buf;
        rxbuf = ts_data->bus_rx_buf;
        memset(txbuf, 0x0, SPI_BUF_LENGTH);
        memset(rxbuf, 0x0, SPI_BUF_LENGTH);
    }

    txbuf[txlen++] = writebuf[0];
    txbuf[txlen++] = WRITE_CMD;
    txbuf[txlen++] = (datalen >> 8) & 0xFF;
    txbuf[txlen++] = datalen & 0xFF;
    if (datalen > 0) {
        txlen = txlen + SPI_DUMMY_BYTE;
        memcpy(&txbuf[txlen], &writebuf[1], datalen);
        txlen = txlen + datalen;
    }

    for (i = 0; i < SPI_RETRY_NUMBER; i++) {
        ret = fts_spi_transfer(txbuf, rxbuf, txlen);
        if ((0 == ret) && ((rxbuf[3] & 0xA0) == 0)) {
            break;
        } else {
            FTS_DEBUG("data write(addr:%x),status:%x,retry:%d,ret:%d",
                      writebuf[0], rxbuf[3], i, ret);
            ret = -EIO;
            udelay(CS_HIGH_DELAY);
        }
    }
    if (ret < 0) {
        FTS_ERROR("data write(addr:%x) fail,status:%x,ret:%d",
                  writebuf[0], rxbuf[3], ret);
    }

err_write:
    if (txlen_need > SPI_BUF_LENGTH) {
        if (txbuf) {
            kfree(txbuf);
            txbuf = NULL;
        }

        if (rxbuf) {
            kfree(rxbuf);
            rxbuf = NULL;
        }
    }

    udelay(CS_HIGH_DELAY);
    mutex_unlock(&ts_data->bus_lock);
    return ret;
}

int fts_write_reg(u8 addr, u8 value)
{
    u8 writebuf[2] = { 0 };

    writebuf[0] = addr;
    writebuf[1] = value;
    return fts_write(writebuf, 2);
}

int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen)
{
    int ret = 0;
    int i = 0;
    struct fts_ts_data *ts_data = fts_data;
    u8 *txbuf = NULL;
    u8 *rxbuf = NULL;
    u32 txlen = 0;
    u32 txlen_need = datalen + SPI_HEADER_LENGTH + ts_data->dummy_byte;
    u8 ctrl = READ_CMD;
    u32 dp = 0;

    if (!cmd || !cmdlen || !data || !datalen) {
        FTS_ERROR("cmd/cmdlen/data/datalen is invalid");
        return -EINVAL;
    }

    mutex_lock(&ts_data->bus_lock);
    if (txlen_need > SPI_BUF_LENGTH) {
        txbuf = kzalloc(txlen_need, GFP_KERNEL);
        if (NULL == txbuf) {
            FTS_ERROR("txbuf malloc fail");
            ret = -ENOMEM;
            goto err_read;
        }

        rxbuf = kzalloc(txlen_need, GFP_KERNEL);
        if (NULL == rxbuf) {
            FTS_ERROR("rxbuf malloc fail");
            ret = -ENOMEM;
            goto err_read;
        }
    } else {
        txbuf = ts_data->bus_tx_buf;
        rxbuf = ts_data->bus_rx_buf;
        memset(txbuf, 0x0, SPI_BUF_LENGTH);
        memset(rxbuf, 0x0, SPI_BUF_LENGTH);
    }

    txbuf[txlen++] = cmd[0];
    txbuf[txlen++] = ctrl;
    txbuf[txlen++] = (datalen >> 8) & 0xFF;
    txbuf[txlen++] = datalen & 0xFF;
    dp = txlen + SPI_DUMMY_BYTE;
    txlen = dp + datalen;
    if (ctrl & DATA_CRC_EN) {
        txlen = txlen + 2;
    }

    for (i = 0; i < SPI_RETRY_NUMBER; i++) {
        ret = fts_spi_transfer(txbuf, rxbuf, txlen);
        if ((0 == ret) && ((rxbuf[3] & 0xA0) == 0)) {
            memcpy(data, &rxbuf[dp], datalen);
            /* crc check */
            if (ctrl & DATA_CRC_EN) {
                ret = rdata_check(&rxbuf[dp], txlen - dp);
                if (ret < 0) {
                    FTS_DEBUG("data read(addr:%x) crc abnormal,retry:%d",
                              cmd[0], i);
                    udelay(CS_HIGH_DELAY);
                    continue;
                }
            }
            break;
        } else {
            FTS_DEBUG("data read(addr:%x) status:%x,retry:%d,ret:%d",
                      cmd[0], rxbuf[3], i, ret);
            ret = -EIO;
            udelay(CS_HIGH_DELAY);
        }
    }

    if (ret < 0) {
        FTS_ERROR("data read(addr:%x) %s,status:%x,ret:%d", cmd[0],
                  (i >= SPI_RETRY_NUMBER) ? "crc abnormal" : "fail",
                  rxbuf[3], ret);
    }

err_read:
    if (txlen_need > SPI_BUF_LENGTH) {
        if (txbuf) {
            kfree(txbuf);
            txbuf = NULL;
        }

        if (rxbuf) {
            kfree(rxbuf);
            rxbuf = NULL;
        }
    }

    udelay(CS_HIGH_DELAY);
    mutex_unlock(&ts_data->bus_lock);
    return ret;
}

int fts_read_reg(u8 addr, u8 *value)
{
    return fts_read(&addr, 1, value, 1);
}


int fts_spi_transfer_direct(u8 *writebuf, u32 writelen, u8 *readbuf, u32 readlen)
{
    int ret = 0;
    struct fts_ts_data *ts_data = fts_data;
    u8 *txbuf = NULL;
    u8 *rxbuf = NULL;
    bool read_cmd = (readbuf && readlen) ? 1 : 0;
    u32 txlen = (read_cmd) ? readlen : writelen;

    if (!writebuf || !writelen) {
        FTS_ERROR("writebuf/len is invalid");
        return -EINVAL;
    }

    mutex_lock(&ts_data->bus_lock);
    if (txlen > SPI_BUF_LENGTH) {
        txbuf = kzalloc(txlen, GFP_KERNEL);
        if (NULL == txbuf) {
            FTS_ERROR("txbuf malloc fail");
            ret = -ENOMEM;
            goto err_spi_dir;
        }

        rxbuf = kzalloc(txlen, GFP_KERNEL);
        if (NULL == rxbuf) {
            FTS_ERROR("rxbuf malloc fail");
            ret = -ENOMEM;
            goto err_spi_dir;
        }
    } else {
        txbuf = ts_data->bus_tx_buf;
        rxbuf = ts_data->bus_rx_buf;
        memset(txbuf, 0x0, SPI_BUF_LENGTH);
        memset(rxbuf, 0x0, SPI_BUF_LENGTH);
    }

    memcpy(txbuf, writebuf, writelen);
    ret = fts_spi_transfer(txbuf, rxbuf, txlen);
    if (ret < 0) {
        FTS_ERROR("data read(addr:%x) fail,status:%x,ret:%d", txbuf[0], rxbuf[3], ret);
        goto err_spi_dir;
    }

    if (read_cmd) {
        memcpy(readbuf, rxbuf, txlen);
    }

    ret = 0;
err_spi_dir:
    if (txlen > SPI_BUF_LENGTH) {
        if (txbuf) {
            kfree(txbuf);
            txbuf = NULL;
        }

        if (rxbuf) {
            kfree(rxbuf);
            rxbuf = NULL;
        }
    }

    udelay(CS_HIGH_DELAY);
    mutex_unlock(&ts_data->bus_lock);
    return ret;
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

    ts_data->dummy_byte = SPI_DUMMY_BYTE;
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

