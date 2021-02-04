/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2017, FocalTech Systems, Ltd., all rights reserved.
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
* File Name: focaltech_i2c.c
*
*    Author: fupeipei
*
*   Created: 2016-08-04
*
*  Abstract: i2c communication with TP
*
*   Version: v1.0
*
* Revision History:
*        v1.0:
*            First release. By fupeipei 2016-08-04
*        v2.0:
*            1. Add retry times after I2C transfer error
*            2. Move HID2STD function
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "focaltech_core.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define DMA_BUFFER_LENGTH                   I2C_BUFFER_LENGTH_MAXINUM
#define I2C_RETRY_NUMBER                    3

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/

/*****************************************************************************
* Static variables
*****************************************************************************/
static DEFINE_MUTEX(i2c_rw_access);

#ifdef CONFIG_MTK_I2C_EXTENSION
u8 *g_dma_buff_va = NULL;
dma_addr_t g_dma_buff_pa = 0;
#endif

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
#ifdef CONFIG_MTK_I2C_EXTENSION
static void fts_i2c_msg_dma_alloct(void);
static void fts_i2c_msg_dma_release(void);
#endif
/*****************************************************************************
* functions body
*****************************************************************************/

#ifdef CONFIG_MTK_I2C_EXTENSION
/************************************************************************
* Name: fts_i2c_read
* Brief: i2c read
* Input: i2c info, write buf, write len, read buf, read len
* Output: get data in the 3rd buf
* Return: fail <0
***********************************************************************/
int fts_i2c_read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen)
{
    int ret = 0;
    int i = 0;

    if (client == NULL) {
        FTS_ERROR("[IIC][DMA]i2c_client==NULL!");
        return -EINVAL;
    }

    mutex_lock(&i2c_rw_access);
    if ( (writelen > 0) && (writelen <= DMA_BUFFER_LENGTH) ) {
        memcpy(g_dma_buff_va, writebuf, writelen);
        client->addr = ((client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG);
        for (i = 0; i < I2C_RETRY_NUMBER; i++) {
            ret = i2c_master_send(client, (unsigned char *)g_dma_buff_pa, writelen);
            if (ret != writelen) {
                FTS_ERROR("[IIC]: i2c_master_send faild, ret=%d!!", ret);
            } else
                break;
        }
        client->addr = client->addr & I2C_MASK_FLAG & (~ I2C_DMA_FLAG);
    }
    if ( (readlen > 0) && (readlen <= DMA_BUFFER_LENGTH) ) {
        client->addr = ((client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG);
        for (i = 0; i < I2C_RETRY_NUMBER; i++) {
            ret = i2c_master_recv(client, (unsigned char *)g_dma_buff_pa, readlen);
            if (ret != readlen) {
                FTS_ERROR("[IIC]: i2c_master_recv faild, ret=%d!!", ret);
            } else
                break;
        }
        memcpy(readbuf, g_dma_buff_va, readlen);
        client->addr = client->addr & I2C_MASK_FLAG & (~ I2C_DMA_FLAG);
    } else
        FTS_ERROR("i2c read len(%d) fail", readlen);
    mutex_unlock(&i2c_rw_access);
    return ret;
}

/************************************************************************
* Name: fts_i2c_write
* Brief: i2c write
* Input: i2c info, write buf, write len
* Output: no
* Return: fail <0
***********************************************************************/
int fts_i2c_write(struct i2c_client *client, char *writebuf, int writelen)
{
    int ret = 0;
    int i = 0;

    if (client == NULL) {
        FTS_ERROR("[IIC][DMA]i2c_client==NULL!");
        return -EINVAL;
    }

    mutex_lock(&i2c_rw_access);
    if ( (writelen > 0) && (writelen <= DMA_BUFFER_LENGTH) ) {
        memcpy(g_dma_buff_va, writebuf, writelen);
        client->addr = ((client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG);
        for (i = 0; i < I2C_RETRY_NUMBER; i++) {
            ret = i2c_master_send(client, (unsigned char *)g_dma_buff_pa, writelen);
            if (ret != writelen) {
                FTS_ERROR("[IIC]: i2c_master_send faild, ret=%d!!", ret);
            } else
                break;
        }
        client->addr = client->addr & I2C_MASK_FLAG & (~ I2C_DMA_FLAG);
    } else
        FTS_ERROR("i2c write len(%d) fail", writelen);
    mutex_unlock(&i2c_rw_access);

    return ret;
}

/************************************************************************
* Name: fs_i2c_msg_dma_alloct
* Brief: i2c allot
* Input: void
* Output: no
* Return: fail <0
***********************************************************************/
static void fts_i2c_msg_dma_alloct(void)
{
    FTS_FUNC_ENTER();
    if (NULL == g_dma_buff_va) {
        fts_data->input_dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
        g_dma_buff_va = (u8 *)dma_alloc_coherent(&fts_data->input_dev->dev, DMA_BUFFER_LENGTH, &g_dma_buff_pa, GFP_KERNEL);

        if (!g_dma_buff_va) {
            FTS_ERROR("[IIC]: Allocate DMA I2C Buffer failed!!");
        }
    }
    FTS_FUNC_EXIT();
}

/************************************************************************
* Name: fs_i2c_msg_dma_release
* Brief: i2c release
* Input: void
* Output: no
* Return: fail <0
***********************************************************************/
static void fts_i2c_msg_dma_release(void)
{
    FTS_FUNC_ENTER();
    if (g_dma_buff_va) {
        dma_free_coherent(NULL, DMA_BUFFER_LENGTH, g_dma_buff_va, g_dma_buff_pa);
        g_dma_buff_va = NULL;
        g_dma_buff_pa = 0;
        FTS_ERROR("[IIC]: Allocated DMA I2C Buffer release!!");
    }
    FTS_FUNC_EXIT();
}
#else
/************************************************************************
* Name: fts_i2c_read
* Brief: i2c read
* Input: i2c info, write buf, write len, read buf, read len
* Output: no
* Return: fail <0
***********************************************************************/
int fts_i2c_read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen)
{
    int ret = 0;
    int i = 0;

    if (client == NULL) {
        FTS_ERROR("[IIC][%s]i2c_client==NULL!", __func__);
        return -EINVAL;
    }

    mutex_lock(&i2c_rw_access);
    if (readlen > 0) {
        if (writelen > 0) {
            struct i2c_msg msgs[] = {
                {
                    .addr = client->addr,
                    .flags = 0,
                    .len = writelen,
                    .buf = writebuf,
                },
                {
                    .addr = client->addr,
                    .flags = I2C_M_RD,
                    .len = readlen,
                    .buf = readbuf,
                },
            };
            for (i = 0; i < I2C_RETRY_NUMBER; i++) {
                ret = i2c_transfer(client->adapter, msgs, 2);
                if (ret < 0) {
                    FTS_ERROR("[IIC]: i2c_transfer(write) error, ret=%d!!", ret);
                } else
                    break;
            }
        } else {
            struct i2c_msg msgs[] = {
                {
                    .addr = client->addr,
                    .flags = I2C_M_RD,
                    .len = readlen,
                    .buf = readbuf,
                },
            };
            for (i = 0; i < I2C_RETRY_NUMBER; i++) {
                ret = i2c_transfer(client->adapter, msgs, 1);
                if (ret < 0) {
                    FTS_ERROR("[IIC]: i2c_transfer(read) error, ret=%d!!", ret);
                } else
                    break;
            }
        }
    }

    mutex_unlock(&i2c_rw_access);
    return ret;
}

/************************************************************************
* Name: fts_i2c_write
* Brief: i2c write
* Input: i2c info, write buf, write len
* Output: no
* Return: fail <0
***********************************************************************/
int fts_i2c_write(struct i2c_client *client, char *writebuf, int writelen)
{
    int ret = 0;
    int i = 0;

    if (client == NULL) {
        FTS_ERROR("[IIC][%s]i2c_client==NULL!", __func__);
        return -EINVAL;
    }

    mutex_lock(&i2c_rw_access);
    if (writelen > 0) {
        struct i2c_msg msgs[] = {
            {
                .addr = client->addr,
                .flags = 0,
                .len = writelen,
                .buf = writebuf,
            },
        };
        for (i = 0; i < I2C_RETRY_NUMBER; i++) {
            ret = i2c_transfer(client->adapter, msgs, 1);
            if (ret < 0) {
                FTS_ERROR("[IIC]: i2c_transfer(write) error, ret=%d!!", ret);
            } else
                break;
        }
    }
    mutex_unlock(&i2c_rw_access);

    return ret;
}
#endif

/************************************************************************
* Name: fts_i2c_write_reg
* Brief: write register
* Input: i2c info, reg address, reg value
* Output: no
* Return: fail <0
***********************************************************************/
int fts_i2c_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue)
{
    u8 buf[2] = {0};

    buf[0] = regaddr;
    buf[1] = regvalue;
    return fts_i2c_write(client, buf, sizeof(buf));
}
/************************************************************************
* Name: fts_i2c_read_reg
* Brief: read register
* Input: i2c info, reg address, reg value
* Output: get reg value
* Return: fail <0
***********************************************************************/
int fts_i2c_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue)
{
    return fts_i2c_read(client, &regaddr, 1, regvalue, 1);
}

/************************************************************************
* HID to standard I2C
***********************************************************************/
void fts_i2c_hid2std(struct i2c_client *client)
{
    int ret = 0;
    u8 buf[3] = {0xeb, 0xaa, 0x09};

    ret = fts_i2c_write(client, buf, 3);
    if (ret < 0)
        FTS_ERROR("hid2std cmd write fail");
    else {
        msleep(10);
        buf[0] = buf[1] = buf[2] = 0;
        ret = fts_i2c_read(client, NULL, 0, buf, 3);
        if (ret < 0)
            FTS_ERROR("hid2std cmd read fail");
        else if ((0xeb == buf[0]) && (0xaa == buf[1]) && (0x08 == buf[2])) {
            FTS_DEBUG("hidi2c change to stdi2c successful");
        } else {
            FTS_ERROR("hidi2c change to stdi2c fail");
        }
    }
}


/************************************************************************
* Name: fts_i2c_init
* Brief: fts i2c init
* Input:
* Output:
* Return:
***********************************************************************/
int fts_i2c_init(void)
{
    FTS_FUNC_ENTER();
#ifdef CONFIG_MTK_I2C_EXTENSION
    fts_i2c_msg_dma_alloct();
#endif

    FTS_FUNC_EXIT();
    return 0;
}
/************************************************************************
* Name: fts_i2c_exit
* Brief: fts i2c exit
* Input:
* Output:
* Return:
***********************************************************************/
int fts_i2c_exit(void)
{
    FTS_FUNC_ENTER();
#ifdef CONFIG_MTK_I2C_EXTENSION
    fts_i2c_msg_dma_release();
#endif
    FTS_FUNC_EXIT();
    return 0;
}

