/* include/linux/input/isl29028.h
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

#ifndef LINUX_INPUT_ISL29028_H
#define LINUX_INPUT_ISL29028_H

struct i2c_client;		/* forward declaration */

struct isl29028_platform_data {
	unsigned long als_factor;
	unsigned long als_highrange;	/* 2 means auto range */
	/* threshold for switch scale value */
	u16 als_lowthres;
	u16 als_highthres;
	/* minimal change percent for reporting */
	unsigned long als_sensitive;
	/* default proximity sample period */
	unsigned long prox_period;
	/* threshold for proximity detection */
	/* the two threshold written to hardware are:
	   low:  (prox_null_value + prox_lowthres_offset),
	   high: (prox_null_value + prox_lowthres_offset + prox_threswindow)
	 */
	u8 prox_null_value;
	u8 prox_lowthres_offset;	/* must > 30 for margin */
	u8 prox_threswindow;
	/* optional callback for platform needs */
	int (*setup) (struct i2c_client * client,
			struct isl29028_platform_data * pdata);
	int (*teardown) (struct i2c_client * client,
			struct isl29028_platform_data * pdata);
	void *context;
};

#endif
