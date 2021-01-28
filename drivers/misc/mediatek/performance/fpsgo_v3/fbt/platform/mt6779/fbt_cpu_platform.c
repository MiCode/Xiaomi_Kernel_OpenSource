// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "mt-plat/eas_ctrl.h"
#include "fbt_cpu_platform.h"
#include <mt-plat/fpsgo_common.h>
#include "mtk_cm_mgr_common.h"
#include <uapi/linux/sched/types.h>
#include <linux/sched/task.h>

#define API_READY 0

#if API_READY
static struct pm_qos_request dram_req;
#endif

void fbt_notify_CM_limit(int reach_limit)
{
#ifdef CONFIG_MTK_CM_MGR
	cm_mgr_perf_set_status(reach_limit);
#endif
	fpsgo_systrace_c_fbt_gm(-100, reach_limit, "notify_cm");
}

void fbt_reg_dram_request(int reg)
{
#if API_READY
	if (reg) {
		if (!pm_qos_request_active(&dram_req))
			pm_qos_add_request(&dram_req, PM_QOS_DDR_OPP,
					PM_QOS_DDR_OPP_DEFAULT_VALUE);
	} else {
		if (pm_qos_request_active(&dram_req))
			pm_qos_remove_request(&dram_req);
	}
#endif
}

void fbt_boost_dram(int boost)
{
#if API_READY
	if (!pm_qos_request_active(&dram_req)) {
		fbt_reg_dram_request(1);
		if (!pm_qos_request_active(&dram_req)) {
			fpsgo_systrace_c_fbt_gm(-100, -1, "dram_boost");
			return;
		}
	}

	if (boost)
		pm_qos_update_request(&dram_req, 0);
	else
		pm_qos_update_request(&dram_req,
				PM_QOS_DDR_OPP_DEFAULT_VALUE);

	fpsgo_systrace_c_fbt_gm(-100, boost, "dram_boost");
#endif
}

void fbt_set_boost_value(unsigned int base_blc)
{
	base_blc = clamp(base_blc, 1U, 100U);
#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
	update_eas_uclamp_min(EAS_UCLAMP_KIR_FPSGO, CGROUP_TA, (int)base_blc);
#endif
	fpsgo_systrace_c_fbt_gm(-100, base_blc, "TA_cap");
}

void fbt_clear_boost_value(void)
{
#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
	update_eas_uclamp_min(EAS_UCLAMP_KIR_FPSGO, CGROUP_TA, 0);
#endif
	fpsgo_systrace_c_fbt_gm(-100, 0, "TA_cap");

	fbt_notify_CM_limit(0);
	fbt_boost_dram(0);
}

void fbt_set_per_task_min_cap(int pid, unsigned int base_blc)
{
	int ret = -1;
	unsigned int base_blc_1024;
#ifdef CONFIG_UCLAMP_TASK
	struct sched_attr fbt2sched_attr;
	struct task_struct *p;
#endif

	if (!pid)
		return;

	base_blc_1024 = (base_blc << 10) / 100U;
	base_blc_1024 = clamp(base_blc_1024, 1U, 1024U);

#ifdef CONFIG_UCLAMP_TASK
	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (likely(p))
		get_task_struct(p);
	rcu_read_unlock();

	if (likely(p)) {
		fbt2sched_attr.sched_flags = SCHED_FLAG_UTIL_CLAMP_MIN;
		fbt2sched_attr.sched_util_min = base_blc_1024;
		ret = sched_setattr(p, &fbt2sched_attr);
		put_task_struct(p);
	}
#endif
	if (ret != 0) {
		fpsgo_systrace_c_fbt(pid, ret, "uclamp fail");
		fpsgo_systrace_c_fbt(pid, 0, "uclamp fail");
		return;
	}

	fpsgo_systrace_c_fbt_gm(pid, base_blc, "min_cap");
}

int fbt_get_L_cluster_num(void)
{
	return 0;
}

int fbt_get_L_min_ceiling(void)
{
	int freq = 0;

	return freq;
}

int fbt_get_default_boost_ta(void)
{
	return 0;
}
