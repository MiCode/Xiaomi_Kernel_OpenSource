/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#define CLUSTER_MAX 10
#define TOUCH_TIMEOUT_MS 100
#define TOUCH_FSTB_ACTIVE_MS 100

extern int (*fpsgo_get_fstb_active_fp)(long long time_diff);

struct _cpufreq {
	int min;
	int max;
} _cpufreq;

struct boost {
	spinlock_t touch_lock;
	wait_queue_head_t wq;
	struct task_struct *thread;
	int touch_event;
	atomic_t event;
};
