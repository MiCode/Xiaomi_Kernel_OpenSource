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
#ifndef _INV_SH_TIMESYNC_H
#define _INV_SH_TIMESYNC_H

#include <linux/device.h>
#include <linux/types.h>
#include <linux/ktime.h>

struct inv_sh_timesync {
	struct device *dev;
	uint32_t resolution;
	uint32_t rate;
	int64_t margin;
	uint32_t sync_sh;
	ktime_t sync;
	int64_t offset;
	int64_t new_offset;
};

void inv_sh_timesync_init(struct inv_sh_timesync *timesync, struct device *dev);

ktime_t inv_sh_timesync_get_timestamp(const struct inv_sh_timesync *timesync,
						uint32_t sh_ts, ktime_t ts);

void inv_sh_timesync_synchronize(struct inv_sh_timesync *timesync,
					uint32_t sh_ts, ktime_t ts, bool reset);

#endif
