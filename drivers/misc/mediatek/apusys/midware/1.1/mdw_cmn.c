/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>

#include "mdw_cmn.h"


uint32_t mdw_cmn_get_time_diff(struct timespec *prev, struct timespec *next)
{
	uint32_t diff = 0;

	diff = (next->tv_sec - prev->tv_sec) * 1000 * 1000;
	if (next->tv_nsec - prev->tv_nsec)
		diff += (next->tv_nsec - prev->tv_nsec)/1000;

	return diff;
}
