/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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

#ifndef __KGSL_PWRSCALE_H
#define __KGSL_PWRSCALE_H

#include <linux/devfreq.h>
#include <linux/msm_adreno_devfreq.h>

/* devfreq governor call window in msec */
#define KGSL_GOVERNOR_CALL_INTERVAL 5

struct kgsl_power_stats {
	u64 busy_time;
	u64 ram_time;
	u64 ram_wait;
};

struct kgsl_pwrscale {
	struct devfreq *devfreq;
	struct devfreq_dev_profile profile;
	unsigned int freq_table[KGSL_MAX_PWRLEVELS];
	char last_governor[DEVFREQ_NAME_LEN];
	struct kgsl_power_stats accum_stats;
	bool enabled;
	s64 time;
	s64 on_time;
	struct srcu_notifier_head nh;
	struct workqueue_struct *devfreq_wq;
	struct work_struct devfreq_suspend_ws;
	struct work_struct devfreq_resume_ws;
	struct work_struct devfreq_notify_ws;
	unsigned long next_governor_call;
};

int kgsl_pwrscale_init(struct device *dev, const char *governor);
void kgsl_pwrscale_close(struct kgsl_device *device);

void kgsl_pwrscale_update(struct kgsl_device *device);
void kgsl_pwrscale_busy(struct kgsl_device *device);
void kgsl_pwrscale_idle(struct kgsl_device *device);
void kgsl_pwrscale_sleep(struct kgsl_device *device);
void kgsl_pwrscale_wake(struct kgsl_device *device);

void kgsl_pwrscale_enable(struct kgsl_device *device);
void kgsl_pwrscale_disable(struct kgsl_device *device);

int kgsl_devfreq_target(struct device *dev, unsigned long *freq, u32 flags);
int kgsl_devfreq_get_dev_status(struct device *, struct devfreq_dev_status *);
int kgsl_devfreq_get_cur_freq(struct device *dev, unsigned long *freq);

#define KGSL_PWRSCALE_INIT(_gov_list, _num_gov) { \
	.enabled = true, \
	.profile = { \
		.target = kgsl_devfreq_target, \
		.get_dev_status = kgsl_devfreq_get_dev_status, \
		.get_cur_freq = kgsl_devfreq_get_cur_freq, \
		.governor_data = (_gov_list), \
		.num_governor_data = (_num_gov), \
	} }
#endif
