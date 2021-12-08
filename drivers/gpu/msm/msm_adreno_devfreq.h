/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#ifndef MSM_ADRENO_DEVFREQ_H
#define MSM_ADRENO_DEVFREQ_H

#include <linux/devfreq.h>
#include <linux/notifier.h>

#define DEVFREQ_FLAG_WAKEUP_MAXFREQ	0x2
#define DEVFREQ_FLAG_FAST_HINT		0x4
#define DEVFREQ_FLAG_SLOW_HINT		0x8

struct device;

/* same as KGSL_MAX_PWRLEVELS */
#define MSM_ADRENO_MAX_PWRLEVELS 16

struct xstats {
	u64 ram_time;
	u64 ram_wait;
	int buslevel;
	unsigned long gpu_minfreq;
};

struct devfreq_msm_adreno_tz_data {
	struct notifier_block nb;
	struct {
		s64 total_time;
		s64 busy_time;
		u32 ctxt_aware_target_pwrlevel;
		u32 ctxt_aware_busy_penalty;
	} bin;
	struct {
		u64 total_time;
		u64 ram_time;
		u64 ram_wait;
		u64 gpu_time;
		u32 num;
		u32 max;
		u32 width;
		u32 *up;
		u32 *down;
		s32 *p_up;
		s32 *p_down;
		u32 *ib_kbps;
		bool floating;
	} bus;
	unsigned int device_id;
	bool is_64;
	bool disable_busy_time_burst;
	bool ctxt_aware_enable;
	/* Multiplier to change gpu busy status */
	u32 mod_percent;
};

struct msm_adreno_extended_profile {
	struct devfreq_msm_adreno_tz_data *private_data;
	struct devfreq_dev_profile profile;
};

struct msm_busmon_extended_profile {
	u32 flag;
	u32 sampling_ms;
	unsigned long percent_ab;
	unsigned long ab_mbytes;
	struct devfreq_msm_adreno_tz_data *private_data;
	struct devfreq_dev_profile profile;
};

typedef void(*getbw_func)(unsigned long *, unsigned long *, void *);

int devfreq_vbif_update_bw(void);
void devfreq_vbif_register_callback(getbw_func func, void *data);

#endif
