/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2017, FocalTech Systems, Ltd., all rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include "focaltech_core.h"

static DEFINE_MUTEX(i2c_rw_access);

int fts_i2c_read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen)
{
	int ret;

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
			if (ret < 0) {
				FTS_ERROR("[IIC]: i2c_transfer(write) error, ret=%d!!", ret);
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
			ret = i2c_transfer(client->adapter, msgs, 1);
			if (ret < 0) {
				FTS_ERROR("[IIC]: i2c_transfer(read) error, ret=%d!!", ret);
			}
		}
	}

	mutex_unlock(&i2c_rw_access);
	return ret;
}

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
		if (ret < 0) {
			FTS_ERROR("%s: i2c_transfer(write) error, ret=%d", __func__, ret);
		}
	}
	mutex_unlock(&i2c_rw_access);

	return ret;
}
int fts_i2c_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue)
{
	u8 buf[2] = {0};

	buf[0] = regaddr;
	buf[1] = regvalue;
	return fts_i2c_write(client, buf, sizeof(buf));
}

int fts_i2c_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue)
{
	return fts_i2c_read(client, &regaddr, 1, regvalue, 1);
}

int fts_i2c_init(void)
{
	FTS_FUNC_ENTER();

	FTS_FUNC_EXIT();
	return 0;
}
int fts_i2c_exit(void)
{
	FTS_FUNC_ENTER();

	FTS_FUNC_EXIT();
	return 0;
}

