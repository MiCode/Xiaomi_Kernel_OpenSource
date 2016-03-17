/*
* Copyright (C) 2015 InvenSense, Inc.
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
#ifndef _INV_SH_MISC_H
#define _INV_SH_MISC_H

#include <linux/ktime.h>

#include "inv_mpu_iio.h"
#include "inv_sh_data.h"

void inv_sh_misc_send_raw_data(struct inv_mpu_state *st,
				const void *data, size_t size);

void inv_sh_misc_send_sensor_data(struct inv_mpu_state *st,
					const struct inv_sh_data *sensor_data,
					ktime_t timestamp);

#endif
