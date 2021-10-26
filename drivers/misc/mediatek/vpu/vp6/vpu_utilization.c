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

#include <linux/devfreq.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/timekeeper_internal.h>

#include "vpu_utilization.h"
#include "vpu_cmn.h"

#define VPU_TIMER_PERIOD_NS 200000000 /* ns = 200 ms */
#define VPU_TIMER_PERIOD_MS 200

static int vpu_sum_usage(struct vpu_core *vpu_core, ktime_t now)
{
	s64 diff;

	diff = ktime_us_delta(now, vpu_core->working_start);
	if (diff < 0)
		goto reset;

	if (vpu_core->util_state == VCT_EXECUTING)
		vpu_core->acc_busy += (unsigned long) diff;

reset:
	vpu_core->working_start = now;

	return 0;
}

int vpu_utilization_compute_enter(struct vpu_core *vpu_core)
{
	unsigned long flags;
	struct vpu_util *vpu_util = vpu_core->vpu_util;
	int ret = 0;

	spin_lock_irqsave(&vpu_util->lock, flags);
	ret = vpu_sum_usage(vpu_core, ktime_get());
	if (vpu_core->util_state == VCT_IDLE)
		vpu_core->util_state = VCT_EXECUTING;
	spin_unlock_irqrestore(&vpu_util->lock, flags);

	return ret;
}

int vpu_utilization_compute_leave(struct vpu_core *vpu_core)
{
	unsigned long flags;
	struct vpu_util *vpu_util = vpu_core->vpu_util;
	int ret = 0;

	spin_lock_irqsave(&vpu_util->lock, flags);
	ret = vpu_sum_usage(vpu_core, ktime_get());
	if (vpu_core->util_state == VCT_EXECUTING)
		vpu_core->util_state = VCT_IDLE;

	spin_unlock_irqrestore(&vpu_util->lock, flags);

	return ret;
}

int vpu_dvfs_get_usage(const struct vpu_core *vpu_core,
		       unsigned long *total_time, unsigned long *busy_time)
{
	unsigned long flags;
	unsigned long busy = 0;
	unsigned long total;
	struct vpu_util *vpu_util = vpu_core->vpu_util;
	int ret = 0;

	spin_lock_irqsave(&vpu_util->lock, flags);
	busy = max(busy, vpu_core->prev_busy);

	total = vpu_util->prev_total;
	spin_unlock_irqrestore(&vpu_util->lock, flags);

	*busy_time = busy / USEC_PER_MSEC;
	*total_time = total;
	if (*busy_time > *total_time)
		*busy_time = *total_time;

	if (*total_time == 0) {
		LOG_ERR("[vpu] %s total time error\n", __func__);
		return -1;
	}

	return ret;
}

static int vpu_util_set_period(struct vpu_core *vpu_core,
			       unsigned int period_ms)
{
	struct vpu_util *vpu_util = vpu_core->vpu_util;

	vpu_util->period_time = ktime_set(0, period_ms * 1000000);

	return 0;
}

static int vpu_util_set_active(struct vpu_core *vpu_core, bool active)
{
	unsigned long flags;
	struct vpu_util *vpu_util = vpu_core->vpu_util;
	int ret = 1;

	if ((vpu_util->active == true) && (active == false)) {
		spin_lock_irqsave(&vpu_util->lock, flags);
		vpu_util->active = false;
		spin_unlock_irqrestore(&vpu_util->lock, flags);

		while (ret > 0)
			ret = hrtimer_cancel(&vpu_util->timer);
	} else if ((vpu_util->active == false) && (active == true)) {
		/* reset data */
		vpu_util->period_start = ktime_get();
		vpu_core->prev_busy = 0;
		vpu_core->acc_busy = 0;
		vpu_core->working_start = vpu_util->period_start;
		vpu_core->util_state = VCT_IDLE;
		vpu_util->prev_total = 0;

		spin_lock_irqsave(&vpu_util->lock, flags);
		vpu_util->active = true;
		spin_unlock_irqrestore(&vpu_util->lock, flags);

		hrtimer_start(&vpu_util->timer, vpu_util->period_time,
			      HRTIMER_MODE_REL);
	}

	return 0;
}

int vpu_set_util_test_parameter(struct vpu_device *vpu_device, uint8_t param,
			       int argc, int *args)
{
	int ret = 0;
	int core;

	switch (param) {
	case VPU_DEBUG_UTIL_PERIOD:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
				argc);
			goto out;
		}

		for (core = 0 ; core < vpu_device->core_num; core++) {
			struct vpu_core *vpu_core = vpu_device->vpu_core[core];

			vpu_util_set_period(vpu_core, args[0]);
		}
		break;
	case VPU_DEBUG_UTIL_ENABLE:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
				argc);
			goto out;
		}

		for (core = 0 ; core < vpu_device->core_num; core++) {
			struct vpu_core *vpu_core = vpu_device->vpu_core[core];

			vpu_util_set_active(vpu_core, args[0]);
		}
		break;
	default:
		LOG_ERR("unsupport the vpu_util parameter:%d\n", param);
		break;
	}

out:
	return ret;
}


static int vpu_timer_get_usage(struct vpu_util *vpu_util)
{
	ktime_t now = ktime_get();
	struct vpu_core *vpu_core = vpu_util->vpu_core;
	unsigned long flags;
	s64 diff;
	int ret = 0;

	spin_lock_irqsave(&vpu_util->lock, flags);
	diff = ktime_ms_delta(now, vpu_util->period_start);
	if (diff < 0) {
		vpu_util->prev_total = 0;
		goto reset_usage;
	}

	vpu_util->prev_total = (unsigned long) diff;

	ret = vpu_sum_usage(vpu_core, now);
	vpu_core->prev_busy = vpu_core->acc_busy;

reset_usage:
	vpu_util->period_start = now;
	vpu_core->acc_busy = 0;
	vpu_core->working_start = vpu_util->period_start;

	spin_unlock_irqrestore(&vpu_util->lock, flags);

	#if defined(VPU_MET_READY)
	MET_Events_BusyRate_Trace(vpu_core->vpu_device, vpu_core->core);
	#endif

	return ret;
}

static enum hrtimer_restart vpu_timer_callback(struct hrtimer *timer)
{
	struct vpu_util *vpu_util;
	int ret = 0;

	vpu_util = container_of(timer, struct vpu_util, timer);

	ret = vpu_timer_get_usage(vpu_util);

	if (vpu_util->active) {
		hrtimer_start(&vpu_util->timer, vpu_util->period_time,
			      HRTIMER_MODE_REL);
	}

	return HRTIMER_NORESTART;
}

int vpu_init_util(struct vpu_core *vpu_core)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	struct device *dev = vpu_device->dev;
	struct vpu_util *vpu_util;
	ktime_t now = ktime_get();
	int ret = 0;

	vpu_util = devm_kzalloc(dev, sizeof(*vpu_util), GFP_KERNEL);
	if (!vpu_util)
		return -ENOMEM;

	vpu_util->dev = dev;
	vpu_util->vpu_core = vpu_core;

	vpu_util->period_time = ktime_set(0, VPU_TIMER_PERIOD_NS);
	vpu_util->period_start = now;

	vpu_core->prev_busy = 0;
	vpu_core->acc_busy = 0;
	vpu_core->working_start = vpu_util->period_start;
	vpu_core->util_state = VCT_IDLE;

	vpu_util->prev_total = 0;

	spin_lock_init(&vpu_util->lock);

	vpu_util->active = true;
	hrtimer_init(&vpu_util->timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);

	vpu_util->timer.function = vpu_timer_callback;
	hrtimer_start(&vpu_util->timer, vpu_util->period_time,
		      HRTIMER_MODE_REL);

	vpu_core->vpu_util = vpu_util;

	return ret;
}

int vpu_deinit_util(struct vpu_core *vpu_core)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	struct device *dev = vpu_device->dev;
	struct vpu_util *vpu_util = vpu_core->vpu_util;
	unsigned long flags;
	int ret = 1;

	spin_lock_irqsave(&vpu_util->lock, flags);
	vpu_util->active = false;
	spin_unlock_irqrestore(&vpu_util->lock, flags);

	while (ret > 0)
		ret = hrtimer_cancel(&vpu_util->timer);

	devm_kfree(dev, vpu_core->vpu_util);
	vpu_core->vpu_util = NULL;

	return ret;
}

