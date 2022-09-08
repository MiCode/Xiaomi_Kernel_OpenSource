/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
