// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
