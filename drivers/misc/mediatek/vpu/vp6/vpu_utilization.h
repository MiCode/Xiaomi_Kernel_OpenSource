/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Mao Lin <Zih-Ling.Lin@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __VPU_UTILIZATION_H__
#define __VPU_UTILIZATION_H__

#include <linux/hrtimer.h>
#include <linux/spinlock.h>


struct vpu_util {
	struct device *dev;
	struct vpu_core *vpu_core;

	spinlock_t lock;

	bool active;
	struct hrtimer timer;
	ktime_t period_time;
	ktime_t period_start;
	unsigned long prev_total;
};
int vpu_utilization_compute_enter(struct vpu_core *vpu_core);
int vpu_utilization_compute_leave(struct vpu_core *vpu_core);
int vpu_dvfs_get_usage(const struct vpu_core *vpu_core,
			unsigned long *total_time, unsigned long *busy_time);
int vpu_init_util(struct vpu_core *vpu_core);
int vpu_deinit_util(struct vpu_core *vpu_core);

#endif /* __VPU_UTILIZATION_H__ */
