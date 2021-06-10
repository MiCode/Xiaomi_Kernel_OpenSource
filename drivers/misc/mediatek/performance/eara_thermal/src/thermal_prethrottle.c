// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#include <linux/sched/signal.h>
#include "thermal_internal.h"
#include "fpsgo_common.h"
#include "fstb.h"

#define EARA_MAX_COUNT 10

static int eara_enable;
static int condition;
static DECLARE_WAIT_QUEUE_HEAD(eara_thrm_q);
static DEFINE_MUTEX(pre_lock);

static void set_tfps_diff(int max_cnt, int *pid, unsigned long long *buf_id, int *diff)
{
	int i;

	mutex_lock(&pre_lock);

	if (!eara_enable) {
		mutex_unlock(&pre_lock);
		return;
	}

	for (i = 0; i < max_cnt; i++) {
		if (pid[i] == 0)
			break;
		pr_debug("EARA set %d %llu: %d\n", pid[i], buf_id[i], diff[i]);
		eara2fstb_tfps_mdiff(pid[i], buf_id[i], diff[i]);
	}

	mutex_unlock(&pre_lock);
}

static void get_tfps_pair(int max_cnt, int *pid, unsigned long long *buf_id, int *tfps)
{
	mutex_lock(&pre_lock);

	if (!eara_enable) {
		mutex_unlock(&pre_lock);
		return;
	}

	memset(pid, 0, max_cnt * sizeof(int));
	memset(buf_id, 0, max_cnt * sizeof(unsigned long long));
	memset(tfps, 0, max_cnt * sizeof(int));
	eara2fstb_get_tfps(max_cnt, pid, buf_id, tfps);

	mutex_unlock(&pre_lock);
}

static void switch_eara(int enable)
{
	mutex_lock(&pre_lock);

	eara_enable = enable;

	mutex_unlock(&pre_lock);
}

static void active_event(int is_active)
{
	int to_activate = 0;

	mutex_lock(&pre_lock);

	if (!eara_enable) {
		mutex_unlock(&pre_lock);
		return;
	}

	if (!condition && is_active)
		to_activate = 1;

	condition = is_active;

	mutex_unlock(&pre_lock);

	if (to_activate)
		wake_up_interruptible(&eara_thrm_q);
}

void __exit eara_thrm_pre_exit(void)
{
	eara_enable_fp = NULL;
	eara_set_tfps_diff_fp = NULL;
	eara_get_tfps_pair_fp = NULL;
	eara_pre_active_fp = NULL;

}

int __init eara_thrm_pre_init(void)
{
	eara_enable_fp = switch_eara;
	eara_set_tfps_diff_fp = set_tfps_diff;
	eara_get_tfps_pair_fp = get_tfps_pair;
	eara_pre_active_fp = active_event;

	return 0;
}

