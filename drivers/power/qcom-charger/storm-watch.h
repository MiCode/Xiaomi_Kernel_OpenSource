/* Copyright (c) 2016 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __STORM_WATCH_H
#define __STORM_WATCH_H
#include <linux/ktime.h>

/**
 * Data used to track an event storm.
 *
 * @storm_period_ms: The maximum time interval between two events. If this limit
 *                   is exceeded then the event chain will be broken and removed
 *                   from consideration for a storm.
 * @max_storm_count: The number of chained events required to trigger a storm.
 * @storm_count:     The current number of chained events.
 * @last_kt:         Kernel time of the last event seen.
 */
struct storm_watch {
	bool	enabled;
	int	storm_period_ms;
	int	max_storm_count;
	int	storm_count;
	ktime_t	last_kt;
};

bool is_storming(struct storm_watch *data);
#endif
