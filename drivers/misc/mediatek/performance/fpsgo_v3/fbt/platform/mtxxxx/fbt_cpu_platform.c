// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "fbt_cpu_platform.h"
#include <sched/sched.h>
#include <mt-plat/fpsgo_common.h>

#define API_READY 0

#if API_READY
static struct pm_qos_request dram_req;
#endif
static struct cpumask mask[FPSGO_PREFER_TOTAL];
static int mask_done;

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

void fbt_set_boost_value(unsigned int base_blc)
{
	base_blc = clamp(base_blc, 1U, 100U);
	fpsgo_systrace_c_fbt_gm(-100, 0, base_blc, "TA_cap");
}

void fbt_clear_boost_value(void)
{
	fpsgo_systrace_c_fbt_gm(-100, 0, 0, "TA_cap");

	fbt_notify_CM_limit(0);
	fbt_boost_dram(0);
}

void fbt_set_per_task_min_cap(int pid, unsigned int base_blc)
{
	int ret = -1;
	unsigned int base_blc_1024;
	struct task_struct *p;

	if (!pid)
		return;

	base_blc_1024 = (base_blc << 10) / 100U;
	base_blc_1024 = clamp(base_blc_1024, 1U, 1024U);

	struct sched_attr attr = {
		.sched_flags =
			SCHED_FLAG_KEEP_PARAMS |
			SCHED_FLAG_UTIL_CLAMP_MIN |
			SCHED_FLAG_RESET_ON_FORK,
		.sched_util_min = base_blc_1024,
	};

	if (pid < 0)
		goto out;

	rcu_read_lock();
	p = find_task_by_vpid(pid);

	if (likely(p))
		get_task_struct(p);

	rcu_read_unlock();

	if (likely(p)) {
		ret = sched_setattr(p, &attr);
		put_task_struct(p);
	}

out:
	if (ret != 0) {
		fpsgo_systrace_c_fbt(pid, 0, ret, "uclamp fail");
		fpsgo_systrace_c_fbt(pid, 0, 0, "uclamp fail");
		return;
	}

	fpsgo_systrace_c_fbt_gm(pid, 0, base_blc, "min_cap");
}

static int generate_cpu_mask(unsigned int prefer_type, struct cpumask *cpu_mask)
{
	if (prefer_type == FPSGO_PREFER_BIG) {
		cpumask_clear(cpu_mask);
		cpumask_set_cpu(0, cpu_mask);
		cpumask_set_cpu(1, cpu_mask);
		cpumask_set_cpu(2, cpu_mask);
		cpumask_set_cpu(3, cpu_mask);
	} else if (prefer_type == FPSGO_PREFER_LITTLE) {
		cpumask_setall(cpu_mask);
		cpumask_clear_cpu(0, cpu_mask);
		cpumask_clear_cpu(1, cpu_mask);
		cpumask_clear_cpu(2, cpu_mask);
		cpumask_clear_cpu(3, cpu_mask);
	} else if (prefer_type == FPSGO_PREFER_NONE)
		cpumask_setall(cpu_mask);
	else
		return -1;

	mask_done = 1;

	return 0;
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
	return 0;
}

