/*
* Copyright (C) 2012 Invensense, Inc.
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
*/

#ifndef _INV_MPU_DTS_H_
#define _INV_MPU_DTS_H_

#include <linux/device.h>
#include <linux/iio/imu/mpu.h>

int inv_mpu_parse_dt(struct device *dev, struct mpu_platform_data *pdata);

#endif  /* #ifndef _INV_MPU_DTS_H_ */
