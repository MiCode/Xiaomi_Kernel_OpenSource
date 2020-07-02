// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
