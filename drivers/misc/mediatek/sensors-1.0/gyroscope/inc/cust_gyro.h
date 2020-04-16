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

#ifndef __CUST_GYRO_H__
#define __CUST_GYRO_H__

#include <linux/types.h>
#include <linux/of.h>

#define GYRO_CUST_I2C_ADDR_NUM 2

struct gyro_hw {
	unsigned short addr;
	int i2c_num;	/*!< the i2c bus used by the chip */
	int direction;  /*!< the direction of the chip */
	int power_id;
	int power_vol;  /*!< the Power Voltage used by the chip */
	int firlen;	 /*!< the length of low pass filter */
	int (*power)(struct gyro_hw *hw, unsigned int on, char *devname);
	unsigned char	i2c_addr[GYRO_CUST_I2C_ADDR_NUM];
	int power_vio_id;
	int power_vio_vol;  /*!< the Power Voltage used by the chip */
	bool is_batch_supported;
};

int get_gyro_dts_func(struct device_node *node, struct gyro_hw *hw);
#endif
