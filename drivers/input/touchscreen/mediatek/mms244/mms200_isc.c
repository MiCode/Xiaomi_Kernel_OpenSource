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
#include <linux/firmware.h>
#include <linux/completion.h>
#include <asm/unaligned.h>

#include "tpd.h"
#include "mms200_ts.h"
extern struct i2c_client *melfas_i2c_client;
extern void mms_reboot(void);
extern int melfas_check_firmware(struct i2c_client *client);
extern int melfas_i2c_DMA_RW_isc(struct i2c_client *client,int rw,
                                 U8 *rxbuf, U16 rlen,
                                 U8 *txbuf, U16 tlen);
bool isc_entered = false;
static int mms_isc_transfer_cmd(struct i2c_client *client, int cmd)
{
  int retval;
	struct isc_packet pkt = { ISC_ADDR, cmd };
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = sizeof(struct isc_packet),
		.buf = (u8 *)&pkt,
	};
	
  retval = i2c_transfer(client->adapter, &msg, 1);
  TPD_DMESG("mms_isc_transfer_cmd, retval = %d\n",retval);
  return retval;
}

static int mms_isc_enter(struct i2c_client *client)
{
  int retval;
  isc_entered = true;
  retval = i2c_smbus_write_byte_data(client, MMS_CMD_ENTER_ISC, true);
  
  return retval;
}

static int mms_isc_exit(struct i2c_client *client)
{
    isc_entered = false;
	return mms_isc_transfer_cmd(client, ISC_CMD_EXIT);
}

static int mms_isc_read_status(struct i2c_client *client, u32 val)
{
	u8 cmd = ISC_CMD_READ_STATUS;
	u32 result = 0;
	int cnt = 100;
	int ret = 0;

	do {
		ret = i2c_smbus_read_i2c_block_data(client, cmd, 4, (u8 *)&result);
		if (result == val)
			break;
		msleep(1);
	} while (--cnt);

	if (!cnt) {
		TPD_DMESG("status read fail. cnt : %d, val : 0x%x != 0x%x\n",
			cnt, result, val);
	}

	return ret;
}
static int mms_isc_erase_page(struct i2c_client *client, int page)
{
	return mms_isc_transfer_cmd(client, ISC_CMD_PAGE_ERASE | page) ||
		mms_isc_read_status(client, ISC_PAGE_ERASE_DONE | ISC_PAGE_ERASE_ENTER | page);
}

static int mms_flash_section(struct i2c_client *client, struct mms_fw_img *img, const u8 *data,
				struct mms_ext_hdr *ext)
{
    struct isc_packet *isc_packet;
    int ret;
    int page, i;
    struct i2c_msg msg[2] = {
        {
            .addr = client->addr | I2C_DMA_FLAG,
            .flags = 0,
        }, {
            .addr = client->addr | I2C_DMA_FLAG,
            .flags = I2C_M_RD,
            .len = ISC_XFER_LEN,
        },
    };
    int ptr = img->offset;
    TPD_DMESG("mms_flash_section\n");

    isc_packet = kzalloc(sizeof(*isc_packet) + ISC_XFER_LEN, GFP_KERNEL);
    isc_packet->cmd = ISC_ADDR;

    msg[0].buf = (u8 *)isc_packet;
    msg[1].buf = kzalloc(ISC_XFER_LEN, GFP_KERNEL);

    for (page = img->start_page; page <= img->end_page; page++) 
    {
        if (ext->data[page] & EXT_INFO_ERASE)
            ret = mms_isc_erase_page(client, page);
        if (ret < 0)
        {
            TPD_DMESG("MMS-ISC, mms_isc_erase_page,ret = %d, Line: %d, Failed\n",ret, __LINE__);
            goto i2c_err;
        }
        else
            TPD_DMESG("MMS-ISC, mms_isc_erase_page, %d, Sucess\n",__LINE__);
        
        if (!(ext->data[page] & EXT_INFO_WRITE)) 
        {
            ptr += MMS_FLASH_PAGE_SZ;
            continue;
        }

        for (i = 0; i < ISC_BLOCK_NUM; i++, ptr += ISC_XFER_LEN) 
        {
            /* flash firmware */
            u32 tmp = page * 256 + i * (ISC_XFER_LEN / 4);
            put_unaligned_le32(tmp, &isc_packet->addr);
            msg[0].len = sizeof(struct isc_packet) + ISC_XFER_LEN;

            memcpy(isc_packet->data, data + ptr, ISC_XFER_LEN);
            ret = melfas_i2c_DMA_RW_isc(melfas_i2c_client,ISC_DMA_W,
                                        NULL,NULL,
                                        msg[0].buf,msg[0].len);
            if (ret < 0)
            {
                TPD_DMESG("MMS-ISC, flash firmware,ret = %d, Line: %d, Failed\n",ret, __LINE__);
                goto i2c_err;
            }
            else
                TPD_DMESG("MMS-ISC, flash firmware, Success, page = %d, i= %d\n",page,i);
            //if (i2c_transfer(client->adapter, msg, 1) != 1)
            //    goto i2c_err;

            /* verify firmware */
            tmp |= ISC_CMD_READ;
            put_unaligned_le32(tmp, &isc_packet->addr);
            msg[0].len = sizeof(struct isc_packet);
            ret = melfas_i2c_DMA_RW_isc(melfas_i2c_client,ISC_DMA_R,
                                        msg[1].buf,msg[1].len,
                                        msg[0].buf,msg[0].len);
            if (ret < 0)
            {
                TPD_DMESG("MMS-ISC, verify firmware,ret = %d, Failed, page = %d, i= %d\n",ret,page,i);
                goto i2c_err;
            }
            else
                TPD_DMESG("MMS-ISC, verify firmware, Success, page = %d, i= %d\n",page,i);
            //if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg))
            //    goto i2c_err;

            if (memcmp(isc_packet->data, msg[1].buf, ISC_XFER_LEN)) 
            {
#if FLASH_VERBOSE_DEBUG
                print_hex_dump(KERN_ERR, "mms fw wr : ",
                        DUMP_PREFIX_OFFSET, 16, 1,
                        isc_packet->data, ISC_XFER_LEN, false);

                print_hex_dump(KERN_ERR, "mms fw rd : ",
                        DUMP_PREFIX_OFFSET, 16, 1,
                        msg[1].buf, ISC_XFER_LEN, false);
#endif
                TPD_DMESG("MMS-ISC,  flash verify failed\n");
                ret = -1;
                goto out;
            }

        }
    }

    TPD_DMESG("MMS-ISC, section [%d] update succeeded\n", img->type);

    ret = 0;
    goto out;

i2c_err:
    TPD_DMESG("MMS-ISC, i2c failed @ %s\n", __func__);
    ret = -1;

out:
    kfree(isc_packet);
    kfree(msg[1].buf);

    return ret;

}


static int mms_flash_fw(const struct firmware *fw, struct i2c_client *client)
{
    int ret;
    int i;
    struct mms_bin_hdr *fw_hdr;
    struct mms_fw_img **img;

    u8 ver[MAX_SECTION_NUM];
    u8 target[MAX_SECTION_NUM];
    int offset = sizeof(struct mms_bin_hdr);
    struct mms_ext_hdr *ext;
    int retires = 3;
    bool update_flag = false;
    
    TPD_DMESG("mms_flash_fw \n");

    fw_hdr = (struct mms_bin_hdr *)fw->data;
    ext = (struct mms_ext_hdr *)(fw->data + fw_hdr->extention_offset);
    img = kzalloc(sizeof(*img) * fw_hdr->section_num, GFP_KERNEL);

    if (retires < 0) {
        TPD_DMESG("failed to obtain ver. info\n");
        memset(ver, 0xff, sizeof(ver));
    } else {
        print_hex_dump(KERN_INFO, "mms_ts fw ver : ", DUMP_PREFIX_NONE, 16, 1,
                ver, MAX_SECTION_NUM, false);
        ret = mms_isc_enter(client);
        if (ret <0)
        {
            TPD_DMESG("MMS-ISC, mms_isc_enter, %d, Failed\n",__LINE__);
            mms_reboot();
            goto out;
        }
        else
            TPD_DMESG("MMS-ISC, mms_isc_enter, %d, Sucess\n",__LINE__);
    }

    for (i = 0; i < fw_hdr->section_num; i++, offset += sizeof(struct mms_fw_img)) 
    {
        TPD_DMESG("MMS-ISC, section_num = %d, offset = %d \n",fw_hdr->section_num,sizeof(struct mms_fw_img));
        img[i] = (struct mms_fw_img *)(fw->data + offset);
        target[i] = img[i]->version;

        if (ver[img[i]->type] != target[i]) {
            TPD_DMESG("MMS-ISC, section [%d] is need to be updated. ver : 0x%x, bin : 0x%x\n",
                i, ver[i], target[i]);

            update_flag = true;

            if ((ret = mms_flash_section(client, img[i], fw->data + fw_hdr->binary_offset, ext))) {
                mms_reboot();
                goto out;
            }

        } else {
            TPD_DMESG("MMS-ISC, section [%d] is up to date\n", i);
        }
    }

    if (update_flag)
        ret = mms_isc_exit(client);

    if (ret < 0)
    {
        TPD_DMESG("MMS-ISC, mms_isc_exit, Failed\n");
        goto out;
    }
    else
    {
        TPD_DMESG("MMS-ISC, mms_isc_exit, Sucess\n");
    }
    
    
    msleep(5);
    
    mms_reboot();
    #if 0
    //update finished, check the firmware to confirm
    if (melfas_check_firmware(client)) {
        TPD_DMESG("failed to obtain version after flash\n");
        ret = -1;
        goto out;
    } else {
        for (i = 0; i < fw_hdr->section_num; i++) {
            if (ver[img[i]->type] != target[i]) {
                TPD_DMESG("version mismatch after flash. [%d] 0x%x != 0x%x\n",
                    i, ver[img[i]->type], target[i]);

                ret = -1;
                goto out;
            }
        }
    }
    #endif
    ret = 0;

out:
    kfree(img);
    if (isc_entered)//(update_flag)
    {
        ret = mms_isc_exit(client);
        if (ret < 0)
        {
            TPD_DMESG("MMS-ISC, mms_isc_exit, Failed\n");
        }
        else
        {
            TPD_DMESG("MMS-ISC, mms_isc_exit, Sucess\n");
        }
    }
    
    msleep(5);
    
    mms_reboot();
    
    //mms_ts_config(info);
    i2c_smbus_read_byte_data(client, MMS_TX_NUM);
	i2c_smbus_read_byte_data(client, MMS_RX_NUM);
    //release_firmware(fw);

    return ret;

}
//extern struct completion mms200_init_done;
/*
PR 453322,temp solution. if failed in update,we try another i2c address
*/
void mms_fw_update_controller(const struct firmware *fw, struct i2c_client *client)
{
	int retires = 3;
	int ret;
	
    TPD_DMESG("mms_fw_update_controller \n");
	if (!fw) {
		TPD_DMESG("failed to read firmware\n");
		//complete_all(&mms200_init_done);
		return;
	}

	do {
		ret = mms_flash_fw(fw, client);
		if(ret < 0)
		{
		  client->addr = 0x48;
		  TPD_DMESG("modify i2c addr to 0x48\n");
		}
		else
		{
		  client->addr = 0x20;
		  TPD_DMESG("modify i2c addr to 0x20\n");
		}
	} while ((ret != 0) && (--retires));
 	client->addr = 0x20;
	if (!retires) {
		TPD_DMESG("failed to flash firmware after retires\n");
	}
}

int isc_fw_download(struct i2c_client *client, const u8 *data, size_t len)
{
#if 0
    u8 *buff;
    u16 crc_buf;
    int src_idx;
    int dest_idx;
    int ret;
    int i, j;

    buff = kzalloc(1024, GFP_KERNEL);
    if (!buff)
    {
        TPD_DMESG("%s: failed to allocate memory\n", __func__);
        kfree(buff);

        ret = -1;
        return ret;
    }

    /*1, enterring ISC mode */
    ret = mms_isc_enter(client);
    if (ret < 0)
    {
        TPD_DMESG("fail to enter ISC mode(err=%d)\n", ret);
        return ret;
    }
    usleep_range(10000, 20000);
    TPD_DMESG("isc_fw, Step 1: Enter ISC mode\n");


#if 0
    /*2, enter ISC update mode */
	for (i = 0; i < fw_hdr->section_num; i++, offset += sizeof(struct mms_fw_img)) {
		img[i] = (struct mms_fw_img *)(fw->data + offset);
		target[i] = img[i]->version;

		if (ver[img[i]->type] != target[i]) {
			dev_info(&client->dev,
				"section [%d] is need to be updated. ver : 0x%x, bin : 0x%x\n",
				i, ver[i], target[i]);

			update_flag = true;

			if ((ret = mms_flash_section(info, img[i], fw->data + fw_hdr->binary_offset, ext))) {
				mms_reboot( );
				goto out;
			}

		} else {
			dev_info(&client->dev, "section [%d] is up to date\n", i);
		}
	}

#endif

    /*3, exit ISC mode */
    ret = mms_isc_exit(client);
    TPD_DMESG("isc_fw, Step 1: Exit ISC mode\n");
    
    kfree(buff);
    
    return ret;

#endif
}

