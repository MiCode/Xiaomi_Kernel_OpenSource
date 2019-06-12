/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2010-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __KGSL_PWRSCALE_H
#define __KGSL_PWRSCALE_H

#include <linux/msm_adreno_devfreq.h>
#include "kgsl_pwrctrl.h"

/* devfreq governor call window in usec */
#define KGSL_GOVERNOR_CALL_INTERVAL 10000

/* Power events to be tracked with history */
#define KGSL_PWREVENT_STATE	0
#define KGSL_PWREVENT_GPU_FREQ	1
#define KGSL_PWREVENT_BUS_FREQ	2
#define KGSL_PWREVENT_POPP	3
#define KGSL_PWREVENT_MAX	4

/**
 * Amount of time running at a level to be considered
 * "stable" in msec
 */
#define STABLE_TIME	150

/* Amount of idle time needed to re-set stability in usec */
#define POPP_RESET_TIME	1000000

/* Number of POPP levels */
#define POPP_MAX	4

/* POPP state bits */
#define POPP_ON		BIT(0)
#define POPP_PUSH	BIT(1)

struct kgsl_popp {
	int gpu_x;
	int ddr_y;
};

struct kgsl_power_stats {
	u64 busy_time;
	u64 ram_time;
	u64 ram_wait;
};

struct kgsl_pwr_event {
	unsigned int data;
	ktime_t start;
	s64 duration;
};

struct kgsl_pwr_history {
	struct kgsl_pwr_event *events;
	unsigned int type;
	unsigned int index;
	unsigned int size;
};

/**
 * struct kgsl_pwrscale - Power scaling settings for a KGSL device
 * @devfreqptr - Pointer to the devfreq device
 * @gpu_profile - GPU profile data for the devfreq device
 * @bus_profile - Bus specific data for the bus devfreq device
 * @freq_table - GPU frequencies for the DCVS algorithm
 * @last_governor - Prior devfreq governor
 * @accum_stats - Accumulated statistics for various frequency calculations
 * @enabled - Whether or not power scaling is enabled
 * @time - Last submitted sample timestamp
 * @on_time - Timestamp when gpu busy begins
 * @freq_change_time - Timestamp of last freq change or popp update
 * @nh - Notifier for the partner devfreq bus device
 * @devfreq_wq - Main devfreq workqueue
 * @devfreq_suspend_ws - Pass device suspension to devfreq
 * @devfreq_resume_ws - Pass device resume to devfreq
 * @devfreq_notify_ws - Notify devfreq to update sampling
 * @next_governor_call - Timestamp after which the governor may be notified of
 * a new sample
 * @history - History of power events with timestamps and durations
 * @popp_level - Current level of POPP mitigation
 * @popp_state - Control state for POPP, on/off, recently pushed, etc
 * @cooling_dev - Thermal cooling device handle
 * @ctxt_aware_enable - Whether or not ctxt aware DCVS feature is enabled
 * @ctxt_aware_busy_penalty - The time in microseconds required to trigger
 * ctxt aware power level jump
 * @ctxt_aware_target_pwrlevel - pwrlevel to jump on in case of ctxt aware
 * power level jump
 */
struct kgsl_pwrscale {
	struct devfreq *devfreqptr;
	struct msm_adreno_extended_profile gpu_profile;
	struct msm_busmon_extended_profile bus_profile;
	unsigned long freq_table[KGSL_MAX_PWRLEVELS];
	char last_governor[DEVFREQ_NAME_LEN];
	struct kgsl_power_stats accum_stats;
	bool enabled;
	ktime_t time;
	s64 on_time;
	s64 freq_change_time;
	struct srcu_notifier_head nh;
	struct workqueue_struct *devfreq_wq;
	struct work_struct devfreq_suspend_ws;
	struct work_struct devfreq_resume_ws;
	struct work_struct devfreq_notify_ws;
	ktime_t next_governor_call;
	struct kgsl_pwr_history history[KGSL_PWREVENT_MAX];
	int popp_level;
	unsigned long popp_state;
	struct thermal_cooling_device *cooling_dev;
	bool ctxt_aware_enable;
	unsigned int ctxt_aware_target_pwrlevel;
	unsigned int ctxt_aware_busy_penalty;
};

int kgsl_pwrscale_init(struct device *dev, const char *governor);
void kgsl_pwrscale_close(struct kgsl_device *device);

void kgsl_pwrscale_update(struct kgsl_device *device);
void kgsl_pwrscale_update_stats(struct kgsl_device *device);
void kgsl_pwrscale_busy(struct kgsl_device *device);
void kgsl_pwrscale_sleep(struct kgsl_device *device);
void kgsl_pwrscale_wake(struct kgsl_device *device);

void kgsl_pwrscale_midframe_timer_restart(struct kgsl_device *device);
void kgsl_pwrscale_midframe_timer_cancel(struct kgsl_device *device);

void kgsl_pwrscale_enable(struct kgsl_device *device);
void kgsl_pwrscale_disable(struct kgsl_device *device, bool turbo);

int kgsl_devfreq_target(struct device *dev, unsigned long *freq, u32 flags);
int kgsl_devfreq_get_dev_status(struct device *dev,
			struct devfreq_dev_status *stat);
int kgsl_devfreq_get_cur_freq(struct device *dev, unsigned long *freq);

int kgsl_busmon_target(struct device *dev, unsigned long *freq, u32 flags);
int kgsl_busmon_get_dev_status(struct device *dev,
			struct devfreq_dev_status *stat);
int kgsl_busmon_get_cur_freq(struct device *dev, unsigned long *freq);

bool kgsl_popp_check(struct kgsl_device *device);


#define KGSL_PWRSCALE_INIT(_priv_data) { \
	.enabled = true, \
	.gpu_profile = { \
		.private_data = _priv_data, \
		.profile = { \
			.target = kgsl_devfreq_target, \
			.get_dev_status = kgsl_devfreq_get_dev_status, \
			.get_cur_freq = kgsl_devfreq_get_cur_freq, \
	} }, \
	.bus_profile = { \
		.private_data = _priv_data, \
		.profile = { \
			.target = kgsl_busmon_target, \
			.get_dev_status = kgsl_busmon_get_dev_status, \
			.get_cur_freq = kgsl_busmon_get_cur_freq, \
	} }, \
	.history[KGSL_PWREVENT_STATE].size = 20, \
	.history[KGSL_PWREVENT_GPU_FREQ].size = 3, \
	.history[KGSL_PWREVENT_BUS_FREQ].size = 5, \
	.history[KGSL_PWREVENT_POPP].size = 5, \
	}
#endif
