/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __CUST_MAG_H__
#define __CUST_MAG_H__

#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/types.h>

#define M_CUST_I2C_ADDR_NUM 2

struct mag_hw {
	int i2c_num;
	int direction;
	int power_id;
	int power_vol;
	unsigned char	i2c_addr[M_CUST_I2C_ADDR_NUM];
	int power_vio_id;
	int power_vio_vol;
	bool is_batch_supported;
};

int get_mag_dts_func(struct device_node *node, struct mag_hw *hw);
#endif
