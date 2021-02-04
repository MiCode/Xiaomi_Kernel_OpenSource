/**
 * The platform spi device for FocalTech's fingerprint sensor.
 *
 * Copyright (C) 2016-2017 FocalTech Systems Co., Ltd. All Rights Reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
**/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/spi/spi.h>

#if defined CONFIG_ARCH_MT6735M || \
    defined CONFIG_ARCH_MT6755
# include <mt_spi.h>
#elif defined CONFIG_ARCH_MT6795
# include <mach/mt_spi.h>
#else
//# error "unknown arch platform."
#endif
//# include <mt_spi.h>

#include "ff_log.h"
#include "ff_err.h"

# undef LOG_TAG
#define LOG_TAG "ff_spi"

#define AS_DRIVER 1
#define VERBOSE_SPI_DATA 0

/* See spidev.c for implementation. */
extern int  spidev_init(void);
extern void spidev_exit(void);
extern struct spi_device *g_spidev;
extern struct spi_device* global_spi;

#if defined (CONFIG_MTK_PLATFORM)
/*static struct mt_chip_conf mt_spi_chip_conf = {
    .setuptime    = 20,
    .holdtime     = 20,
    .high_time    = 25,
    .low_time     = 25,
    .cs_idletime  = 10,
    .ulthgh_thrsh =  0,
    .sample_sel   = POSEDGE,
    .cs_pol       = ACTIVE_LOW,
    .cpol         = SPI_CPOL_0,
    .cpha         = SPI_CPHA_0,
    .tx_mlsb      = SPI_MSB,
    .rx_mlsb      = SPI_MSB,
    .tx_endian    = SPI_LENDIAN,
    .rx_endian    = SPI_LENDIAN,
    .com_mod      = DMA_TRANSFER, // FIFO_TRANSFER/DMA_TRANSFER
    .pause        = PAUSE_MODE_ENABLE,
    .finish_intr  = FINISH_INTR_EN,
    .deassert     = DEASSERT_DISABLE,
    .ulthigh      = ULTRA_HIGH_DISABLE,
    .tckdly       = TICK_DLY0,
}; */
#endif

#ifndef AS_DRIVER
static struct spi_board_info spi_desc = {
    .modalias    = "spidev",
    .bus_num     = 0,
    .chip_select = 0,
    .mode        = SPI_MODE_0,
#if defined (CONFIG_MTK_PLATFORM)
    .controller_data = 0,//&mt_spi_chip_conf,
#endif
};
static struct spi_master *g_master = NULL;
#endif
////////////////////////////////////////////////////////////////////////////////

static inline bool is_printable(char ch)
{
    return (ch > 0x1f && ch < 0x7f);
}

void ff_util_hexdump(const void *buf, int len)
{
    uint8_t *ptr = (uint8_t *)buf;
    char line[64] = {'\0', };
    int i, c, r, count = len;
    int width = 8; /* 8|16 */
    bool b_ellipsis = false;

    for (r = 0; count > 0; ++r) {
        char *pl = line;

        /* 4-1: Offset. */
        pl += sprintf(pl, "%04x  ", r * width);

        /* 4-2: Hexadecimal. */
        for (i = 0, c = 0; c < width; ++c) {
            if (count > 0) {
                pl += sprintf(pl, "%02x ", *ptr++);
                ++i, --count;
            } else {
                pl += sprintf(pl, "   ");
            }

            //if (c == (width - 1)) pl += sprintf(pl, " ");
        }
        ptr -= i, count += i;

        /* 4-3: ASCII. */
        pl += sprintf(pl, " |");
        for (c = 0; c < width; ++c) {
            if (count > 0) {
                char ch = *ptr++;
                --count;

                if (is_printable(ch)) {
                    pl += sprintf(pl, "%c", ch);
                } else {
                    pl += sprintf(pl, ".");
                }
            } else {
                pl += sprintf(pl, " ");
            }
        }
        pl += sprintf(pl, "|");

        /* 4-4: Output. */
#if 0
        ff_log_printf(FF_LOG_LEVEL_DBG, LOG_TAG, line);
#else
        if (count > (len - 64) || count <= 64) {
            ff_log_printf(FF_LOG_LEVEL_DBG, LOG_TAG, line);
        } else if (!b_ellipsis) {
            ff_log_printf(FF_LOG_LEVEL_DBG, LOG_TAG, "....");
            b_ellipsis = true;
        }
#endif
    }
}

int ff_spi_init(void)
{
#ifndef AS_DRIVER
    uint32_t speed;
    uint8_t mode, lsb, bits;
#endif
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

#if AS_DRIVER
    /* Register the spidev driver. */
if(!global_spi)
{
    err = spidev_init();
    if (err) {
        FF_LOGE("failed to register spidev driver.");
        FF_CHECK_ERR(FF_ERR_INTERNAL);
    } else {
        FF_LOGI("spidev driver has been registered.");
    }
}else{
	 FF_LOGI("spidev driver has been registered,use global spi.");
	 g_spidev=global_spi;
}

#else
    /* Retrieve the master controller handle. */
    g_master = spi_busnum_to_master(spi_desc.bus_num);
    if (!g_master) {
        FF_LOGE("there is no spi master controller on bus #%d.", spi_desc.bus_num);
        return (-ENODEV);
    }

    /* Register the spi device. */
    g_spidev = spi_new_device(g_master, &spi_desc);
    if (!g_spidev) {
        FF_LOGE("failed to register spidev device.");
        spi_master_put(g_master);
        g_master = NULL;
        return (-EBUSY);
    }

    speed = g_spidev->max_speed_hz;
    g_spidev->max_speed_hz = 4000000;
    err = spi_setup(g_spidev);
    if (err) {
        g_spidev->max_speed_hz = speed;
    }

    mode = g_spidev->mode & 0xff;
    lsb  = g_spidev->mode & SPI_LSB_FIRST;
    bits = g_spidev->bits_per_word;
    FF_LOGI("spi mode 0x%x, %d bits %sper word, %d Hz max.", mode, bits, lsb ? "(lsb first) " : "",
            g_spidev->max_speed_hz);

    FF_LOGI("spidev device has been registered.");
#endif

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ff_spi_free(void)
{
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

#if AS_DRIVER
if(!global_spi){
    if (g_spidev) {
        g_spidev = NULL;
        spidev_exit();
        FF_LOGI("spidev driver has been un-registered.");
    }
}
#else
    /* Unregister the spi device. */
    if (g_spidev) {
        spi_master_put(g_spidev->master);
        spi_unregister_device(g_spidev);

        FF_LOGI("spidev device has been unregistered.");
    }

    /* Release the master controller handle. */
    if (g_master) {
        spi_master_put(g_master);
        g_master = NULL;
    }
#endif

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

static void ff_spi_complete(void *arg)
{
    complete(arg);
}

static int ff_spi_sync(struct spi_device *spidev, struct spi_message *message)
{
    DECLARE_COMPLETION_ONSTACK(done);
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

    message->complete = ff_spi_complete;
    message->context = &done;

    err = spi_async(spidev, message);
    if (err == 0) {
        wait_for_completion(&done);
        err = message->status;
        if (err == 0) {
            err = message->actual_length;
        }
    }

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ff_spi_write_buf(const void *tx_buf, int tx_len)
{
    struct spi_message message;
    struct spi_transfer xfer;
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

    memset(&xfer, 0, sizeof(xfer));
    xfer.tx_buf = tx_buf;
    xfer.len    = (unsigned)tx_len;
    spi_message_init(&message);
    spi_message_add_tail(&xfer, &message);

#if VERBOSE_SPI_DATA
    /* Verbose. */
    ff_log_printf(FF_LOG_LEVEL_DBG, LOG_TAG, "ff_spi_write_buf(%p, %d):", xfer.tx_buf, xfer.len);
    ff_util_hexdump(xfer.tx_buf, xfer.len);
#endif

    FF_CHECK_PTR(g_spidev);
    err = ff_spi_sync(g_spidev, &message);
    if (err > 0) {
        if (err != tx_len) {
            FF_LOGE("ff_spi_sync(..) = %d.", err);
            FF_CHECK_ERR(FF_ERR_IO);
        }
        err = FF_SUCCESS;
    }

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ff_spi_write_then_read_buf(const void *tx_buf, int tx_len, void *rx_buf, int rx_len)
{
    struct spi_message message;
    struct spi_transfer xfer;
    int err = FF_SUCCESS, tx_rx_buf_len = tx_len + rx_len;
    void *tx_rx_buf = NULL;
    FF_LOGV("'%s' enter.", __func__);

    FF_CHECK_PTR(g_spidev);

    do {
        /* 4-1: Allocate memory buffer. */
        tx_rx_buf = kmalloc(tx_rx_buf_len + 1, GFP_KERNEL);
        FF_CHECK_PTR(tx_rx_buf);
        memset(tx_rx_buf, 0, tx_rx_buf_len + 1);

        /* 4-2: Prepare data buffer for WRITE. */
        memcpy(tx_rx_buf, tx_buf, tx_len);
#if VERBOSE_SPI_DATA
        /* Verbose TX buffer. */
        ff_log_printf(FF_LOG_LEVEL_DBG, LOG_TAG, "ff_spi_WRITE_then_read_buf(%p, %d, ..):",
            tx_buf, tx_len);
        ff_util_hexdump(tx_buf, tx_len);
#endif

        /* 4-3: Low-level data exchange. */
        memset(&xfer, 0, sizeof(xfer));
        xfer.tx_buf = tx_rx_buf;
        xfer.rx_buf = tx_rx_buf;
        xfer.len    = (unsigned)tx_rx_buf_len;
        spi_message_init(&message);
        spi_message_add_tail(&xfer, &message);
        err = ff_spi_sync(g_spidev, &message);
        if (err > 0) {
            if (err != tx_rx_buf_len) {
                FF_LOGE("ff_spi_sync(..) = %d.", err);
                err = FF_ERR_IO;
                break;
            }
            err = FF_SUCCESS;
        }

        /* 4-4: READ from data buffer. */
        memcpy(rx_buf, tx_rx_buf + tx_len, rx_len);
#if VERBOSE_SPI_DATA
        /* Verbose RX buffer. */
        ff_log_printf(FF_LOG_LEVEL_DBG, LOG_TAG, "ff_spi_write_then_READ_buf(.., %p, %d):",
            rx_buf, rx_len);
        ff_util_hexdump(tx_rx_buf, tx_rx_buf_len);
#endif
    } while (0);

    /* Release resource. */
    if (tx_rx_buf) {
        kfree(tx_rx_buf);
    }

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

