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
#endif
