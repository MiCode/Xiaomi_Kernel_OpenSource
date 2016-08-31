/* include/linux/input/isl29035.h
 *
 * Copyright (C) 2016 XiaoMi, Inc.
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

#ifndef LINUX_INPUT_isl29035_H
#define LINUX_INPUT_isl29035_H

struct i2c_client; /* forward declaration */

struct isl29035_platform_data {
	unsigned long als_factor;
	unsigned long als_highrange; /* 2 means auto range */
	/* threshold for switch scale value */
	u16 als_lowthres;
	u16 als_highthres;
	/* minimal change percent for reporting */
	unsigned long als_sensitive;

	/* optional callback for platform needs */
	int (*setup)(struct i2c_client *client,
			struct isl29035_platform_data *pdata);
	int (*teardown)(struct i2c_client *client,
			struct isl29035_platform_data *pdata);
	void *context;
};

#endif
