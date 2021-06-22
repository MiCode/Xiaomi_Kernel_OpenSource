// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "fbt_cpu_platform.h"
#include <sched/sched.h>
#include <mt-plat/fpsgo_common.h>


void fbt_notify_CM_limit(int reach_limit)
{
	fpsgo_systrace_c_fbt_gm(-100, 0, reach_limit, "notify_cm");
}

void fbt_reg_dram_request(int reg)
{
}

void fbt_boost_dram(int boost)
{
}

void fbt_set_per_task_cap(int pid, unsigned int min_blc, unsigned int max_blc)
{
	int ret = -1;
	unsigned int min_blc_1024;
	unsigned int max_blc_1024;
	struct task_struct *p;
	struct sched_attr attr = {};

	if (!pid)
		return;

	min_blc_1024 = (min_blc << 10) / 100U;
	min_blc_1024 = clamp(min_blc_1024, 1U, 1024U);

	max_blc_1024 = (max_blc << 10) / 100U;
	max_blc_1024 = clamp(max_blc_1024, 1U, 1024U);

	attr.sched_policy = -1;
	attr.sched_flags =
		SCHED_FLAG_KEEP_ALL |
		SCHED_FLAG_UTIL_CLAMP |
		SCHED_FLAG_RESET_ON_FORK;

	if (min_blc == 0 && max_blc == 100) {
		attr.sched_util_min = -1;
		attr.sched_util_max = -1;
	} else {
		attr.sched_util_min = (min_blc_1024 << 10) / 1280;
		attr.sched_util_max = (max_blc_1024 << 10) / 1280;
	}

	if (pid < 0)
		goto out;

	rcu_read_lock();
	p = find_task_by_vpid(pid);

	if (likely(p))
		get_task_struct(p);

	rcu_read_unlock();

	if (likely(p)) {
		/* ret = sched_setattr_nocheck(p, &attr); */
		put_task_struct(p);
	}

out:
	if (ret != 0) {
		fpsgo_systrace_c_fbt(pid, 0, ret, "uclamp fail");
		fpsgo_systrace_c_fbt(pid, 0, 0, "uclamp fail");
		return;
	}

	fpsgo_systrace_c_fbt_gm(pid, 0, attr.sched_util_min, "min_cap");
	fpsgo_systrace_c_fbt_gm(pid, 0, attr.sched_util_max, "max_cap");
}

void fbt_set_affinity(pid_t pid, unsigned int prefer_type)
{
	fpsgo_systrace_c_fbt(pid, 0, prefer_type, "set_affinity");
}

void fbt_set_cpu_prefer(int pid, unsigned int prefer_type)
{
}

int fbt_get_L_cluster_num(void)
{
	return 0;
}

int fbt_get_L_min_ceiling(void)
{
	return 0;
}

int fbt_get_default_boost_ta(void)
{
	return 0;
}

int fbt_get_default_adj_loading(void)
{
	return 1;
}

