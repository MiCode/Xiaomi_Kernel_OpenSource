// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/rcupdate.h>
#include "tuning.h"

#ifdef CONFIG_UCLAMP_TASK
int set_task_util_min(pid_t pid, unsigned int util_min)
{
	unsigned int upper_bound;
	struct task_struct *p;
	int ret = 0;

	mutex_lock(&uclamp_mutex);
	rcu_read_lock();
	p = find_task_by_vpid(pid);

	if (!p) {
		ret = -ESRCH;
		goto out;
	}

	upper_bound = p->uclamp[UCLAMP_MAX].value;

	if (util_min > upper_bound || util_min < 0) {
		ret = -EINVAL;
		goto out;
	}

	p->uclamp[UCLAMP_MIN].user_defined = true;
	uclamp_group_get(p, NULL, &p->uclamp[UCLAMP_MIN],
			UCLAMP_MIN, util_min);

out:
	rcu_read_unlock();
	mutex_unlock(&uclamp_mutex);

	return ret;
}
EXPORT_SYMBOL(set_task_util_min);

int set_task_util_max(pid_t pid, unsigned int util_max)
{
	unsigned int lower_bound;
	struct task_struct *p;
	int ret = 0;

	mutex_lock(&uclamp_mutex);
	rcu_read_lock();
	p = find_task_by_vpid(pid);

	if (!p) {
		ret = -ESRCH;
		goto out;
	}

	lower_bound = p->uclamp[UCLAMP_MIN].value;

	if (util_max < lower_bound || util_max > 1024) {
		ret = -EINVAL;
		goto out;
	}

	p->uclamp[UCLAMP_MAX].user_defined = true;
	uclamp_group_get(p, NULL, &p->uclamp[UCLAMP_MAX],
			UCLAMP_MAX, util_max);

out:
	rcu_read_unlock();
	mutex_unlock(&uclamp_mutex);

	return ret;
}
EXPORT_SYMBOL(set_task_util_max);

#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
int uclamp_min_for_perf_idx(int idx, int min_value)
{
	int ret;
	struct uclamp_se *uc_se_min, *uc_se_max;
	struct cgroup_subsys_state *css = NULL;

	if (min_value > SCHED_CAPACITY_SCALE || min_value < 0)
		return -ERANGE;

	ret = schedtune_css_uclamp(idx, UCLAMP_MAX, &css, &uc_se_max);
	if (ret)
		return -EINVAL;
	if (uc_se_max->value < min_value)
		return -EINVAL;

	ret = schedtune_css_uclamp(idx, UCLAMP_MIN, &css, &uc_se_min);
	if (ret)
		return -EINVAL;
	if (uc_se_min->value == min_value)
		return 0;


	mutex_lock(&uclamp_mutex);

	uclamp_group_get(NULL, css, uc_se_min, UCLAMP_MIN, min_value);

	cpu_util_update(css, UCLAMP_MIN, uc_se_min->group_id, min_value);

	mutex_unlock(&uclamp_mutex);

	return 0;
}
EXPORT_SYMBOL(uclamp_min_for_perf_idx);

int uclamp_max_for_perf_idx(int idx, int max_value)
{
	int ret;
	struct uclamp_se *uc_se_min, *uc_se_max;
	struct cgroup_subsys_state *css = NULL;

	if (max_value > SCHED_CAPACITY_SCALE || max_value < 0)
		return -ERANGE;

	ret = schedtune_css_uclamp(idx, UCLAMP_MAX, &css, &uc_se_min);
	if (ret)
		return -EINVAL;
	if (uc_se_max->value == max_value)
		return 0;

	ret = schedtune_css_uclamp(idx, UCLAMP_MIN, &css, &uc_se_min);
	if (ret)
		return -EINVAL;
	if (uc_se_min->value > max_value)
		return -EINVAL;


	mutex_lock(&uclamp_mutex);

	uclamp_group_get(NULL, css, uc_se_max, UCLAMP_MAX, max_value);

	cpu_util_update(css, UCLAMP_MAX, uc_se_max->group_id, max_value);

	mutex_unlock(&uclamp_mutex);

	return 0;
}
EXPORT_SYMBOL(uclamp_max_for_perf_idx);
#endif

#ifdef CONFIG_MTK_SCHED_CPU_PREFER
int sched_set_cpuprefer(pid_t pid, unsigned int prefer_type)
{
	struct task_struct *p;
	unsigned long flags;
	int retval = 0;

	if (!valid_cpu_prefer(prefer_type) || pid < 0)
		return -EINVAL;

	rcu_read_lock();
	retval = -ESRCH;
	p = find_task_by_vpid(pid);
	if (p != NULL) {
		raw_spin_lock_irqsave(&p->pi_lock, flags);
		p->cpu_prefer = prefer_type;
		raw_spin_unlock_irqrestore(&p->pi_lock, flags);
		trace_sched_set_cpuprefer(p);
	}
	rcu_read_unlock();

	return retval;
}
EXPORT_SYMBOL(sched_set_cpuprefer);
#endif

#endif
