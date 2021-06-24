/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef _TIMESYNC_H_
#define _TIMESYNC_H_

#include <linux/spinlock.h>

struct timesync_filter {
	spinlock_t lock;
	int64_t last_time;
	int64_t max_diff;
	int64_t min_diff;
	uint32_t cnt;
	uint32_t tail;
	uint32_t bufsize;
	int64_t *buffer;
	int64_t offset;
	int64_t offset_debug;
	char *name;
};

void timesync_filter_set(struct timesync_filter *filter,
		int64_t scp_timestamp, int64_t scp_archcounter);
int64_t timesync_filter_get(struct timesync_filter *filter);
int timesync_filter_init(struct timesync_filter *filter);
void timesync_filter_exit(struct timesync_filter *filter);
void timesync_start(void);
void timesync_stop(void);
void timesync_resume(void);
void timesync_suspend(void);
int timesync_init(void);
void timesync_exit(void);

#endif
