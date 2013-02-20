/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_IDLE_STATS_H
#define __ARCH_ARM_MACH_MSM_IDLE_STATS_H

#include <linux/types.h>
#include <linux/ioctl.h>

enum msm_idle_stats_event {
	MSM_IDLE_STATS_EVENT_BUSY_TIMER_EXPIRED = 1,
	MSM_IDLE_STATS_EVENT_COLLECTION_TIMER_EXPIRED = 2,
	MSM_IDLE_STATS_EVENT_COLLECTION_FULL = 3,
	MSM_IDLE_STATS_EVENT_TIMER_MIGRATED = 4,
};

/*
 * All time, timer, and time interval values are in units of
 * microseconds unless stated otherwise.
 */
#define MSM_IDLE_STATS_NR_MAX_INTERVALS 100
#define MSM_IDLE_STATS_MAX_TIMER 1000000

struct msm_idle_stats {
	__u32 busy_timer;
	__u32 collection_timer;

	__u32 busy_intervals[MSM_IDLE_STATS_NR_MAX_INTERVALS];
	__u32 idle_intervals[MSM_IDLE_STATS_NR_MAX_INTERVALS];
	__u32 nr_collected;
	__s64 last_busy_start;
	__s64 last_idle_start;

	enum msm_idle_stats_event event;
	__s64 return_timestamp;
};

#define MSM_IDLE_STATS_IOC_MAGIC  0xD8
#define MSM_IDLE_STATS_IOC_COLLECT  \
		_IOWR(MSM_IDLE_STATS_IOC_MAGIC, 1, struct msm_idle_stats)

#endif  /* __ARCH_ARM_MACH_MSM_IDLE_STATS_H */
