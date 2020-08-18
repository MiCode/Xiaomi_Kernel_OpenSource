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

#ifndef __CUST_ACCELGYRO_H__
#define __CUST_ACCELGYRO_H__

#include <linux/of.h>
#include <linux/types.h>

struct accelgyro_hw {

	int direction;    /*!< the direction of the chip */
	int accel_firlen; /*!< the length of low pass filter */
	int gyro_firlen;  /*!< the length of low pass filter */

	bool accel_is_batch_supported;
	bool gyro_is_batch_supported;
};

int get_accelgyro_dts_func(struct device_node *node, struct accelgyro_hw *hw);

#endif
