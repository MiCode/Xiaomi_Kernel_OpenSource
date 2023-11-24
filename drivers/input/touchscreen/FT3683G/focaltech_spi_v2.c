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
*   Version: v2.0
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
/* N17 code for HQ-310974 by xionglei6 at 2023/08/14 start */
const char* tp_vendor = NULL;
/* N17 code for HQ-310974 by xionglei6 at 2023/08/14 end */

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
    if (txlen_need > FTS_MAX_BUS_BUF) {
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
        memset(txbuf, 0x0, FTS_MAX_BUS_BUF);
        memset(rxbuf, 0x0, FTS_MAX_BUS_BUF);
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
    if (txlen_need > FTS_MAX_BUS_BUF) {
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
    if (txlen_need > FTS_MAX_BUS_BUF) {
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
        memset(txbuf, 0x0, FTS_MAX_BUS_BUF);
        memset(rxbuf, 0x0, FTS_MAX_BUS_BUF);
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
    if (txlen_need > FTS_MAX_BUS_BUF) {
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


int fts_bus_transfer_direct(u8 *writebuf, u32 writelen, u8 *readbuf, u32 readlen)
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
    if (txlen > FTS_MAX_BUS_BUF) {
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
        memset(txbuf, 0x0, FTS_MAX_BUS_BUF);
        memset(rxbuf, 0x0, FTS_MAX_BUS_BUF);
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
    if (txlen > FTS_MAX_BUS_BUF) {
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

int fts_bus_configure(struct fts_ts_data *ts_data, u8 *buf, u32 size)
{
    int ret = 0;
    FTS_FUNC_ENTER();
    if (ts_data->spi && buf && size) {
        ts_data->spi->mode = buf[0];
        ts_data->spi->bits_per_word = buf[1];
        ts_data->spi->max_speed_hz = *(u32 *)(buf + 3);
        FTS_INFO("spi,mode=%d,bits=%d,speed=%d", ts_data->spi->mode,
                 ts_data->spi->bits_per_word, ts_data->spi->max_speed_hz);
        ret = spi_setup(ts_data->spi);
        if (ret < 0) {
            FTS_ERROR("spi setup fail,ret:%d", ret);
        }
    }
    FTS_FUNC_EXIT();
    return ret;
}

/*****************************************************************************
* TP Driver
*****************************************************************************/
static int fts_ts_probe(struct spi_device *spi)
{
    int ret = 0;
    struct fts_ts_data *ts_data = NULL;

    FTS_INFO("Touch Screen(SPI-2 BUS) driver prboe...");
    spi->mode = SPI_MODE_0;
    spi->bits_per_word = 8;
    spi->chip_select = 0;  // it's important
    ret = spi_setup(spi);
    if (ret < 0) {
        FTS_ERROR("spi setup fail");
        return ret;
    }

    /* malloc memory for global struct variable */
    ts_data = (struct fts_ts_data *)kzalloc(sizeof(*ts_data), GFP_KERNEL);
    if (!ts_data) {
        FTS_ERROR("allocate memory for fts_data fail");
        return -ENOMEM;
    }

    ts_data->spi = spi;
    ts_data->dev = &spi->dev;
    ts_data->log_level = 1;
    ts_data->bus_type = BUS_TYPE_SPI;
    ts_data->bus_ver = BUS_VER_V2;
    ts_data->dummy_byte = SPI_DUMMY_BYTE;
    /* N17 code for HQ-291087 by liunianliang at 2023/5/29 start */
    snprintf(ts_data->vendor, 32, tp_vendor);
    /* N17 code for HQ-291087 by liunianliang at 2023/5/29 end */

    spi_set_drvdata(spi, ts_data);

    ret = fts_ts_probe_entry(ts_data);
    if (ret) {
        FTS_ERROR("Touch Screen(SPI BUS) driver probe fail");
        spi_set_drvdata(spi, NULL);
        kfree_safe(ts_data);
        return ret;
    }

    FTS_INFO("Touch Screen(SPI BUS) driver prboe successfully");
    return 0;
}

static int fts_ts_remove(struct spi_device *spi)
{
    struct fts_ts_data *ts_data = spi_get_drvdata(spi);
    FTS_FUNC_ENTER();
    if (ts_data) {
        fts_ts_remove_entry(ts_data);
        spi_set_drvdata(spi, NULL);
        kfree_safe(ts_data);
    }
    FTS_FUNC_EXIT();
    return 0;
}

#if IS_ENABLED(CONFIG_PM) && FTS_PATCH_COMERR_PM
static int fts_pm_suspend(struct device *dev)
{
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    FTS_INFO("system enters into pm_suspend");
    ts_data->pm_suspend = true;
    reinit_completion(&ts_data->pm_completion);
    return 0;
}

static int fts_pm_resume(struct device *dev)
{
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    FTS_INFO("system resumes from pm_suspend");
    ts_data->pm_suspend = false;
    complete(&ts_data->pm_completion);
    return 0;
}

static const struct dev_pm_ops fts_dev_pm_ops = {
    .suspend = fts_pm_suspend,
    .resume = fts_pm_resume,
};
#endif

static const struct spi_device_id fts_ts_id[] = {
    {FTS_DRIVER_NAME, 0},
    {},
};
static const struct of_device_id fts_dt_match[] = {
    {.compatible = "focaltech,fts", },
    {},
};
MODULE_DEVICE_TABLE(of, fts_dt_match);

static struct spi_driver fts_ts_spi_driver = {
    .probe = fts_ts_probe,
    .remove = fts_ts_remove,
    .driver = {
        .name = FTS_DRIVER_NAME,
        .owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM) && FTS_PATCH_COMERR_PM
        .pm = &fts_dev_pm_ops,
#endif
        .of_match_table = of_match_ptr(fts_dt_match),
    },
    .id_table = fts_ts_id,
};

/* N17 code for HQ-291087 by liunianliang at 2023/5/29 start */
static int skip_load = 0;
module_param(skip_load, int, S_IRUSR);

static int force_load = 0;
module_param(force_load, int, S_IRUSR);

struct tag_videolfb {
    u64 fb_base;
    u32 islcmfound;
    u32 fps;
    u32 vram;
    char lcmname[1];
};

struct tag_bootmode {
    u32 size;
    u32 tag;
    u32 bootmode;
    u32 boottype;
};

static int __init fts_ts_spi_init(void)
{
    int ret = 0;


    struct device_node *lcm_name;
    struct tag_videolfb *videolfb_tag = NULL;
    struct tag_bootmode *tag_boot = NULL;
    unsigned long size = 0;

    /* for debug, you can use: 'insmod focaltech_tp.ko skip_load=1' */
    if (skip_load) {
        FTS_ERROR("get a skip flag, don't load focaltech_tp ko!");
        return 0;
    }

    if (force_load)
        goto force_load_ko;

    lcm_name = of_find_node_by_path("/chosen");

    if (lcm_name) {
        tag_boot = (struct tag_bootmode *)of_get_property(lcm_name, "atag,boot", NULL);
        if (tag_boot && (tag_boot->bootmode == 8 || tag_boot->bootmode == 9)) {
            FTS_ERROR("in kpoc mode, don't load focaltech_tp ko!");
            return 0;
        }

        videolfb_tag = (struct tag_videolfb *)of_get_property(lcm_name,
                "atag,videolfb",(int *)&size);
        if (!videolfb_tag) {
            FTS_ERROR("Invalid lcm name!!!");
            return 0;
        }
        FTS_ERROR("fts_ts_spi_init read lcm name : %s", videolfb_tag->lcmname);
        if (strcmp("n17_36_02_0a_dsc_vdo_lcm_drv",
                videolfb_tag->lcmname) == 0) {
            FTS_ERROR("not focaltech tp!!!");
            return 0;
        } else if (strcmp("n17_42_0d_0b_dsc_vdo_lcm_drv",
                videolfb_tag->lcmname) == 0) {
            tp_vendor = FTS_MODULE_NAME;
        } else {
            tp_vendor = FTS_MODULE2_NAME;
        }
        FTS_INFO("focaltech tp, vendor is %s !!!", tp_vendor);
    }

force_load_ko:

    FTS_FUNC_ENTER();
    ret = spi_register_driver(&fts_ts_spi_driver);
    if ( ret != 0 ) {
        FTS_ERROR("Focaltech touch screen driver init failed!");
    }
    FTS_FUNC_EXIT();
    return ret;
}

static void __exit fts_ts_spi_exit(void)
{
    spi_unregister_driver(&fts_ts_spi_driver);
}

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
/* N17 code for HQ-291087 by liunianliang at 2023/5/29 end */
module_init(fts_ts_spi_init);
module_exit(fts_ts_spi_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver(SPI)");
MODULE_LICENSE("GPL v2");
