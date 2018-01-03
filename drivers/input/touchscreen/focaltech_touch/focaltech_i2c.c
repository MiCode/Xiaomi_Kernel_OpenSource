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
 ************************************************************************/

/*****************************************************************************
 * Included header files
 *****************************************************************************/
#include "focaltech_core.h"

/*****************************************************************************
 * Private constant and macro definitions using #define
 *****************************************************************************/

/*****************************************************************************
 * Private enumerations, structures and unions using typedef
 *****************************************************************************/

/*****************************************************************************
 * Static variables
 *****************************************************************************/
static DEFINE_MUTEX(i2c_rw_access);

/*****************************************************************************
 * Global variable or extern global variabls/functions
 *****************************************************************************/

/*****************************************************************************
 * Static function prototypes
 *****************************************************************************/

/*****************************************************************************
 * functions body
 *****************************************************************************/

/************************************************************************
 * Name: fts_i2c_read
 * Brief: i2c read
 * Input: i2c info, write buf, write len, read buf, read len
 * Output: get data in the 3rd buf
 * Return: fail <0
 ***********************************************************************/
int fts_i2c_read(struct i2c_client *client, char *writebuf,
		int writelen, char *readbuf, int readlen)
{
	int ret = -EIO;

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
			ret = i2c_transfer(client->adapter, msgs, 2);
			if (ret < 0)
				FTS_ERROR("[IIC]: i2c_write error %d!!", ret);
		} else {
			struct i2c_msg msgs[] = {
				{
					.addr = client->addr,
					.flags = I2C_M_RD,
					.len = readlen,
					.buf = readbuf,
				},
			};
			ret = i2c_transfer(client->adapter, msgs, 1);
			if (ret < 0)
				FTS_ERROR("[IIC]: i2c_read error %d!!", ret);
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
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			FTS_ERROR("[IIC]: i2c_write error, ret=%d", ret);
	}

	mutex_unlock(&i2c_rw_access);

	return ret;
}

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
 * Name: fts_i2c_init
 * Brief: fts i2c init
 * Input:
 * Output:
 * Return:
 ***********************************************************************/
int fts_i2c_init(void)
{
	FTS_FUNC_ENTER();

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

	FTS_FUNC_EXIT();
	return 0;
}

