// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "fbt_cpu_platform.h"
#include <sched/sched.h>
#include <mt-plat/fpsgo_common.h>
#include <linux/cpumask.h>
#include "fpsgo_base.h"

#define API_READY 0

static struct cpumask mask[FPSGO_PREFER_TOTAL];
static int mask_done;

void fbt_notify_CM_limit(int reach_limit)
{
#if IS_ENABLED(CONFIG_MTK_CM_MGR)
	cm_mgr_perf_set_status(reach_limit);
#endif
	fpsgo_systrace_c_fbt_gm(-100, 0, reach_limit, "notify_cm");
}

void fbt_reg_dram_request(int reg)
{
}

void fbt_boost_dram(int boost)
{

	if (boost)
	fpsgo_sentcmd(FPSGO_SET_DRAM, 0, -1);
	else
	fpsgo_sentcmd(FPSGO_SET_DRAM, -1, -1);

	fpsgo_systrace_c_fbt_gm(-100, 0, boost, "dram_boost");
}

void fbt_set_boost_value(unsigned int base_blc)
{
	base_blc = clamp(base_blc, 1U, 100U);
	fpsgo_sentcmd(FPSGO_SET_BOOST_TA, base_blc, -1);
	fpsgo_systrace_c_fbt_gm(-100, 0, base_blc, "TA_cap");
}

void fbt_clear_boost_value(void)
{
	fpsgo_sentcmd(FPSGO_SET_BOOST_TA, -1, -1);
	fpsgo_systrace_c_fbt_gm(-100, 0, 0, "TA_cap");

	fbt_notify_CM_limit(0);
	fbt_boost_dram(0);
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
		ret = sched_setattr_nocheck(p, &attr);
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

static int generate_cpu_mask(unsigned int prefer_type, struct cpumask *cpu_mask)
{
	if (prefer_type == FPSGO_PREFER_LITTLE) {
		cpumask_setall(cpu_mask);
		cpumask_clear_cpu(4, cpu_mask);
		cpumask_clear_cpu(5, cpu_mask);
		cpumask_clear_cpu(6, cpu_mask);
		cpumask_clear_cpu(7, cpu_mask);
	} else if (prefer_type == FPSGO_PREFER_NONE)
		cpumask_setall(cpu_mask);
	else if (prefer_type == FPSGO_PREFER_BIG) {
		cpumask_clear(cpu_mask);
		cpumask_set_cpu(7, cpu_mask);
	} else if (prefer_type == FPSGO_PREFER_L_M) {
		cpumask_setall(cpu_mask);
		cpumask_clear_cpu(7, cpu_mask);
	} else
		return -1;

	mask_done = 1;

	return 0;
}

void fbt_set_affinity(pid_t pid, unsigned int prefer_type)
{
	long ret;

	if (!mask_done) {
		generate_cpu_mask(FPSGO_PREFER_LITTLE,
					&mask[FPSGO_PREFER_LITTLE]);
		generate_cpu_mask(FPSGO_PREFER_NONE, &mask[FPSGO_PREFER_NONE]);
		generate_cpu_mask(FPSGO_PREFER_BIG, &mask[FPSGO_PREFER_BIG]);
		generate_cpu_mask(FPSGO_PREFER_L_M, &mask[FPSGO_PREFER_L_M]);
	}

#if API_READY
	ret = sched_setaffinity(pid, &mask[prefer_type]);
#else
	ret = -100;
#endif
	if (ret != 0) {
		fpsgo_systrace_c_fbt(pid, 0, ret, "setaffinity fail");
		fpsgo_systrace_c_fbt(pid, 0, 0, "setaffinity fail");
		return;
	}
	fpsgo_systrace_c_fbt(pid, 0, prefer_type, "set_affinity");
}

void fbt_set_cpu_prefer(int pid, unsigned int prefer_type)
{
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

int fbt_get_default_adj_count(void)
{
	return 10;
}

int fbt_get_default_adj_tdiff(void)
{
	return 1000000;
}

int fbt_get_cluster_limit(int *cluster, int *freq, int *r_freq)
{
/*
 * when return value is zero -> no limit
 * when cluster is not set -> no limit
 * when cluster is set and freq is set -> ceiling limit
 * when cluster is set and r_freq is set -> rescue ceiling limit
 */
	*cluster = 2;
	*freq = 2600000;
	return 1;
}

int fbt_get_default_uboost(void)
{
	return 75;
}

int fbt_get_default_qr_enable(void)
{
	return 1;
}

int fbt_get_default_gcc_enable(void)
{
	return 1;
}

int fbt_get_l_min_bhropp(void)
{
	return 0;
}

