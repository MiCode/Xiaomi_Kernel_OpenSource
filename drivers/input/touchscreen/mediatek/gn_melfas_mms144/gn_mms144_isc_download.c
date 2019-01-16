/*
 * mms100_ISC_download.c - Touchscreen driver for Melfas MMS-series touch controllers
 *
 * Copyright (C) 2011 Google Inc.
 * Author: Dima Zavin <dima@android.com>
 *         Simon Wilson <simonwilson@google.com>
 *
 * ISP reflashing code based on original code from Melfas.
 *
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#define WORD_SIZE  4
#define ISC_PKT_SIZE   1029
#define ISC_PKT_DATA_SIZE  1024
#define ISC_PKT_HEADER_SIZE  3
#define ISC_PKT_NUM   31
#define ISC_ENTER_ISC_CMD  0x5F
#define ISC_ENTER_ISC_DATA  0x01
#define ISC_CMD    0xAE
#define ISC_ENTER_UPDATE_DATA  0x55
#define ISC_ENTER_UPDATE_DATA_LEN 9
#define ISC_DATA_WRITE_SUB_CMD  0xF1
#define ISC_EXIT_ISC_SUB_CMD  0x0F
#define ISC_EXIT_ISC_SUB_CMD2  0xF0
#define ISC_CHECK_STATUS_CMD  0xAF
#define ISC_CONFIRM_CRC   0x03
#define ISC_DEFAULT_CRC   0xFFFF

static u16 gen_crc(u8 data, u16 pre_crc)
{
    u16 crc;
    u16 cur;
    u16 temp;
    u16 bit_1;
    u16 bit_2;
    int i;
    crc = pre_crc;
    for (i = 7; i >= 0; i--)
    {
        cur = ((data >> i) & 0x01) ^(crc & 0x0001);
        bit_1 = cur ^(crc >> 11 & 0x01);
        bit_2 = cur ^(crc >> 4 & 0x01);
        temp = (cur << 4) | (crc >> 12 & 0x0F);
        temp = (temp << 7) | (bit_1 << 6) | (crc >> 5 & 0x3F);
        temp = (temp << 4) | (bit_2 << 3) | (crc >> 1 & 0x0007);
        crc = temp;
    }
    return crc;
}

int isc_fw_download(struct i2c_client *client, const u8 *data, size_t len)
//static int isc_fw_download(struct mms_ts_info *info, const u8 *data, size_t len)
{
    u8 *buff;
    u16 crc_buf;
    int src_idx;
    int dest_idx;
    int ret;
    int i, j;

    buff = kzalloc(ISC_PKT_SIZE, GFP_KERNEL);
    if (!buff)
    {
        pr_info("%s: failed to allocate memory\n", __func__);
        kfree(buff);

        ret = -1;
        return ret;
    }

    /* enterring ISC mode */
    *buff = ISC_ENTER_ISC_DATA;
    ret = i2c_smbus_write_byte_data(client, ISC_ENTER_ISC_CMD, *buff);
    if (ret < 0)
    {
        pr_info("fail to enter ISC mode(err=%d)\n", ret);
        goto fail_to_isc_enter;
    }
    usleep_range(10000, 20000);
    dev_info(&client->dev, "Enter ISC mode\n");

    /*enter ISC update mode */
    *buff = ISC_ENTER_UPDATE_DATA;
    ret = i2c_smbus_write_i2c_block_data(client, ISC_CMD, ISC_ENTER_UPDATE_DATA_LEN, buff);
    if (ret < 0)
    {
        pr_info("fail to enter ISC update mode(err=%d)\n", ret);
        goto fail_to_isc_update;
    }
    dev_info(&client->dev, "Enter ISC update mode\n");

    /* firmware write */
    *buff = ISC_CMD;
    *(buff + 1) = ISC_DATA_WRITE_SUB_CMD;
    for (i = 0; i < ISC_PKT_NUM; i++)
    {
        *(buff + 2) = i;
        crc_buf = gen_crc(*(buff + 2), ISC_DEFAULT_CRC);
        for (j = 0; j < ISC_PKT_DATA_SIZE; j++)
        {
            dest_idx = ISC_PKT_HEADER_SIZE + j;
            src_idx = i * ISC_PKT_DATA_SIZE + ((int)(j / WORD_SIZE)) * WORD_SIZE - (j % WORD_SIZE) + 3;
            *(buff + dest_idx) = *(data + src_idx);
            crc_buf = gen_crc(*(buff + dest_idx), crc_buf);
        }
        *(buff + ISC_PKT_DATA_SIZE + ISC_PKT_HEADER_SIZE + 1) = crc_buf & 0xFF;
        *(buff + ISC_PKT_DATA_SIZE + ISC_PKT_HEADER_SIZE) = crc_buf >> 8 & 0xFF;
        ret = i2c_master_send(client, buff, ISC_PKT_SIZE);
        if (ret < 0)
        {
            pr_info("fail to firmware writing on packet %d.(%d)\n", i, ret);
            goto fail_to_fw_write;
        }
        usleep_range(1, 5);

        /* confirm CRC */
        ret = i2c_smbus_read_byte_data(client, ISC_CHECK_STATUS_CMD);
        if (ret == ISC_CONFIRM_CRC)
        {
            dev_info(&client->dev, "updating %dth firmware data packet.\n", i);
        }
        else
        {
            pr_info("fail to firmware update on %dth (%X).\n", i, ret);
            ret = -1;
            goto fail_to_confirm_crc;
        }
    }
    ret = 0;

fail_to_confirm_crc:
fail_to_fw_write:
    /* exit ISC mode */
    *buff = ISC_EXIT_ISC_SUB_CMD;
    *(buff + 1) = ISC_EXIT_ISC_SUB_CMD2;
    i2c_smbus_write_i2c_block_data(client, ISC_CMD, 2, buff);
    usleep_range(10000, 20000);

fail_to_isc_update:
fail_to_isc_enter:
    //hw_reboot_normal(info);
    kfree(buff);
    
    return ret;
}

