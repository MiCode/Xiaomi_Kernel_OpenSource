/*
* Copyright (C) 2015 InvenSense, Inc.
* Copyright (C) 2018 XiaoMi, Inc.
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
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/ktime.h>
#include <linux/math64.h>

#include "inv_sh_timesync.h"

static inline ktime_t get_sh_time(const struct inv_sh_timesync *timesync,
					uint32_t sh_ts)
{
	ktime_t time;
	uint64_t val;

	val = (uint64_t)sh_ts * (uint64_t)timesync->resolution;
	time = ns_to_ktime(val);

	return time;
}

static int64_t compute_offset(const struct inv_sh_timesync *timesync,
				uint32_t time_sh)
{
	int64_t delta, delta_ms;
	int64_t offset;

	delta = (int64_t)time_sh - (int64_t)timesync->sync_sh;
	delta *= timesync->resolution;
	delta_ms = div_s64(delta, 1000000);

	if (timesync->new_offset >= timesync->offset) {
		offset = timesync->offset + delta_ms * timesync->rate;
		if (offset > timesync->new_offset)
			offset = timesync->new_offset;
		else if (offset < timesync->offset)
			offset = timesync->offset;
	} else {
		offset = timesync->offset - delta_ms * timesync->rate;
		if (offset < timesync->new_offset)
			offset = timesync->new_offset;
		else if (offset > timesync->offset)
			offset = timesync->offset;
	}

	return offset;
}

void inv_sh_timesync_init(struct inv_sh_timesync *timesync, struct device *dev)
{
	timesync->dev = dev;
	timesync->resolution = 100 * 1000;
	timesync->rate = 50 * 1000;
	timesync->margin = 20 * 1000000LL;
	timesync->sync_sh = 0;
	timesync->sync = ktime_set(0, 0);
	timesync->offset = 0;
	timesync->new_offset = timesync->offset;
}

ktime_t inv_sh_timesync_get_timestamp(const struct inv_sh_timesync *timesync,
					uint32_t sh_ts, ktime_t ts)
{
	const ktime_t time_sh = get_sh_time(timesync, sh_ts);
	int64_t offset;
	int64_t val;
	ktime_t time;

	if (ktime_to_ns(timesync->sync) == 0)
		return ktime_sub_ns(ts, timesync->margin);

	offset = compute_offset(timesync, sh_ts);
	val = ktime_to_ns(time_sh) + offset;
	time = ns_to_ktime(val);

	return time;
}

void inv_sh_timesync_synchronize(struct inv_sh_timesync *timesync,
					uint32_t sh_ts, ktime_t ts, bool reset)
{
	const ktime_t time_sh = get_sh_time(timesync, sh_ts);
	int64_t old_offset, new_offset;

	new_offset = ktime_to_ns(ts) - ktime_to_ns(time_sh) - timesync->margin;
	old_offset = compute_offset(timesync, sh_ts);

	timesync->new_offset = new_offset;
	if (ktime_to_ns(timesync->sync) == 0 || reset) {
		timesync->offset = new_offset;
		dev_dbg(timesync->dev, "first offset: %lld\n",
				timesync->new_offset);
	} else {
		timesync->offset = old_offset;
		dev_dbg(timesync->dev, "new offset: %lld\n",
				timesync->new_offset);
	}

	timesync->sync_sh = sh_ts;
	timesync->sync = ts;
}
