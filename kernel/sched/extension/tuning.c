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
	p = find_task_by_vpid(pid);
	if (p != NULL) {
		raw_spin_lock_irqsave(&p->pi_lock, flags);
		p->cpu_prefer = prefer_type;
		raw_spin_unlock_irqrestore(&p->pi_lock, flags);
		trace_sched_set_cpuprefer(p);
	} else {
		retval = -ESRCH;
	}
	rcu_read_unlock();

	return retval;
}
EXPORT_SYMBOL(sched_set_cpuprefer);
#endif

#ifdef CONFIG_MTK_SCHED_BIG_TASK_MIGRATE
void set_sched_rotation_enable(bool enable)
{
	big_task_rotation_enable = enable;
}
EXPORT_SYMBOL(set_sched_rotation_enable);
#endif /* CONFIG_MTK_SCHED_BIG_TASK_MIGRATE */

#if defined(CONFIG_CGROUPS) && defined(CONFIG_MTK_SCHED_CPU_PREFER)
#include <linux/sched.h>
#include "sched.h"
#include <linux/cgroup.h>

enum {
	SCHED_NO_BOOST = 0,
	SCHED_ALL_BOOST,
	SCHED_FG_BOOST,
};

/*global variable for recording customer's setting type*/
static int sched_boost_type = SCHED_NO_BOOST;

int get_task_group_path(struct task_group *tg, char *buf, size_t buf_len)
{
	return cgroup_path(tg->css.cgroup, buf, buf_len);
}

/*set sched boost type
 *@type: reference sched boost type
 *@return :success current type,else return -1
 */
int set_sched_boost_type(int type)
{
	if (type < SCHED_NO_BOOST || type > SCHED_FG_BOOST) {
		pr_info("value of sched boost type should between %d-%d but your valuse is %d\n",
		       SCHED_PREFER_NONE, SCHED_PREFER_END, type);
		return -1;
	}

	sched_boost_type = type;

	return sched_boost_type;
}
EXPORT_SYMBOL(set_sched_boost_type);

int get_sched_boost_type(void)
{
	return sched_boost_type;
}
EXPORT_SYMBOL(get_sched_boost_type);

bool should_sched_boost(struct task_struct *task)
{
	static const char * const priority_list[] = {"foreground", "top-app"};
	char group_path[256] = {0};
	int i = 0, len = sizeof(priority_list) / sizeof(char *);
	bool check_result = false;

	get_task_group_path(task->sched_task_group, group_path, PATH_MAX);
	for (; i < len; i++) {
		if (strstr(group_path, priority_list[i])) {
			check_result = true;
			break;
		}
	}

	return check_result;
}
/*
 * note:wait cpu_prefer defined
 */
int task_orig_cpu_prefer(struct task_struct *task)
{
	return task->cpu_prefer;
}
/*check task's boost type
 *first priority is SCHED_ALL_BOOST. if current task is fg task &
 *current is fg_boost return prefer_big
 */
int cpu_prefer(struct task_struct *task)
{
	int cpu_prefer = task_orig_cpu_prefer(task);

	switch (sched_boost_type) {
	case SCHED_ALL_BOOST:
		cpu_prefer = SCHED_PREFER_BIG;
		break;
	case SCHED_FG_BOOST:
		if (should_sched_boost(task)) {
			cpu_prefer = SCHED_PREFER_BIG;
			break;
		}
	default:
		if (cpu_prefer == SCHED_PREFER_LITTLE &&
			schedtune_task_boost(task))
			cpu_prefer = SCHED_PREFER_NONE;
	}

	return cpu_prefer;
}
EXPORT_SYMBOL(cpu_prefer);

#else

int set_sched_boost_type(int type)
{
	return -1;
}
EXPORT_SYMBOL(set_sched_boost_type);

int get_sched_boost_type(void)
{
	return 0;
}
EXPORT_SYMBOL(get_sched_boost_type);
/*
 * note:wait cpu_prefer defined
 */
int task_orig_cpu_prefer(struct task_struct *task)
{
	return 0;
}
#if defined(CONFIG_MTK_SCHED_CPU_PREFER)

/*check task's boost type*/
int cpu_prefer(struct task_struct *task)
{
	int cpu_prefer = task->cpu_prefer;

	if (cpu_prefer == SCHED_PREFER_LITTLE &&
		schedtune_task_boost(task))
		cpu_prefer = SCHED_PREFER_NONE;
	}
	return cpu_prefer;
}
#else
/*check task's boost type*/
int cpu_prefer(struct task_struct *task)
{
	return 0;
}
#endif
EXPORT_SYMBOL(cpu_prefer);
#endif
