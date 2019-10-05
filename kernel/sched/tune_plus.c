/*
 * Copyright (C) 2018 MediaTek Inc.
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

#define MET_STUNE_DEBUG 1

#if MET_STUNE_DEBUG
#include <mt-plat/met_drv.h>
#endif

int stune_task_threshold;

/* A mutex for set stune_task_threshold */
static DEFINE_MUTEX(stune_threshold_mutex);

int set_stune_task_threshold(int threshold)
{
	if (threshold > 1024 || threshold < -1)
		return -EINVAL;

	mutex_lock(&stune_threshold_mutex);

	if (threshold < 0)
		stune_task_threshold = default_stune_threshold;
	else
		stune_task_threshold = threshold;

	mutex_unlock(&stune_threshold_mutex);

#if MET_STUNE_DEBUG
	met_tag_oneshot(0, "sched_stune_threshold", stune_task_threshold);
#endif

	return 0;
}

int sched_stune_task_threshold_handler(struct ctl_table *table,
					int write, void __user *buffer,
					size_t *lenp, loff_t *ppos)
{
	int ret;
	int old_threshold;

	old_threshold = stune_task_threshold;
	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (!ret && write) {
		ret = set_stune_task_threshold(stune_task_threshold);
		if (ret)
			stune_task_threshold = old_threshold;
	}

	return ret;
}
