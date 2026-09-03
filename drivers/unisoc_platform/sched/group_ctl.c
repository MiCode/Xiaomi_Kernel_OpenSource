// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Unisoc (shanghai) Technologies Co., Ltd
 */

#include <trace/hooks/cgroup.h>
#include "uni_sched.h"

#define BOOSTGROUPS_COUNT	(OTHER_GROUPS + 1)

/* Boost groups
 * Keep track of all the boost groups which impact on CPU, for example when a
 * CPU has two RUNNABLE tasks belonging to two different boost groups and thus
 * likely with different boost values.
 * Since on each system we expect only a limited number of boost groups, here
 * we use a simple array to keep track of the metrics required to compute the
 * maximum per-CPU boosting value.
 */
struct boost_groups {
	/* Maximum boost value for all RUNNABLE tasks on a CPU */
	int boost_max;
	struct {
		/* The boost for tasks on that boost group */
		int boost;
		/* Count of RUNNABLE tasks on that boost group */
		int tasks;
	} group[BOOSTGROUPS_COUNT];
	/* CPU's boost group locking */
	raw_spinlock_t lock;
};

struct uni_task_group *cgrp_table[BOOSTGROUPS_COUNT];

#ifdef CONFIG_UNISOC_GROUP_BOOST

#define ENQUEUE_TASK  1
#define DEQUEUE_TASK -1

/* Boost groups affecting each CPU in the system */
DEFINE_PER_CPU(struct boost_groups, cpu_boost_groups);

int cpu_group_boost(int cpu)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);

	return bg->boost_max;
}

unsigned long boosted_cpu_util(int cpu, unsigned long util)
{
	int boost = cpu_group_boost(cpu);
	int margin = group_boost_margin(util, boost);

	trace_sched_boost_cpu(cpu, util, boost, margin);

	return util + margin;
}

static inline bool
group_boost_cpu_active(int idx, struct boost_groups *bg)
{
	if (bg->group[idx].tasks)
		return true;

	return false;
}

static void group_boost_cpu_update(int cpu)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	int boost_max = INT_MIN;
	int idx;

	for (idx = 0; idx < BOOSTGROUPS_COUNT; ++idx) {
		/*
		 * A boost group affects a CPU only if it has
		 * RUNNABLE tasks on that CPU.
		 */
		if (!group_boost_cpu_active(idx, bg))
			continue;

		/* This boost group is active */
		if (boost_max > bg->group[idx].boost)
			continue;

		boost_max = bg->group[idx].boost;
	}

	/* when there is no task on cpu, setting the max to be zero */
	if (boost_max == INT_MIN)
		bg->boost_max = 0;
	else
		bg->boost_max = boost_max;
}

static inline void
group_boost_tasks_update(struct task_struct *p, int cpu, int idx, int task_count)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	int tasks = bg->group[idx].tasks + task_count;

	/* Update boosted tasks count while avoiding to make it negative */
	bg->group[idx].tasks = max(0, tasks);

	/* Boost group activation or deactivation on that RQ */
	if ((task_count > 0 && bg->group[idx].tasks == 1) ||
	    (task_count < 0 && bg->group[idx].tasks == 0))
		group_boost_cpu_update(cpu);

	trace_sched_group_boost_tasks_update(p, cpu, tasks, idx,
			bg->group[idx].boost, bg->boost_max);
}

static int group_boost_val_update(int idx, int boost)
{
	struct boost_groups *bg;
	int cur_boost_max;
	int old_boost;
	int cpu;

	/* Update per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);

		/*
		 * Keep track of current boost values to compute the per CPU
		 * maximum only when it has been affected by the new value of
		 * the updated boost group
		 */
		cur_boost_max = bg->boost_max;
		old_boost = bg->group[idx].boost;

		/* Update the boost value of this boost group */
		bg->group[idx].boost = boost;

		/* Check if this update increase current max */
		if (boost > cur_boost_max && group_boost_cpu_active(idx, bg)) {
			bg->boost_max = boost;

			trace_sched_groupboost_val_update(cpu, 1, bg->boost_max);
			continue;
		}

		/* Check if this update has decreased current max */
		if (cur_boost_max == old_boost && old_boost > boost) {
			group_boost_cpu_update(cpu);
			trace_sched_groupboost_val_update(cpu, -1, bg->boost_max);
			continue;
		}

		trace_sched_groupboost_val_update(cpu, 0, bg->boost_max);
	}

	return 0;
}

void group_boost_enqueue(struct rq *rq, struct task_struct *p)
{
	int cpu = cpu_of(rq);
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	unsigned long irq_flags;
	int idx;

	/*
	 * Boost group accouting is protected by a per-cpu lock and requires
	 * interrupt to be disabled to avoid race conditions for example on
	 * do_exit()::cgroup_exit() and task migration.
	 */
	raw_spin_lock_irqsave(&bg->lock, irq_flags);

	idx = uni_task_group_idx(p);

	group_boost_tasks_update(p, cpu, idx, ENQUEUE_TASK);

	raw_spin_unlock_irqrestore(&bg->lock, irq_flags);
}

void group_boost_dequeue(struct rq *rq, struct task_struct *p)
{
	int cpu = cpu_of(rq);
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	unsigned long irq_flags;
	int idx;

	/*
	 * Boost group accouting is protected by a per-cpu lock and requires
	 * interrupt to be disabled to avoid race conditions on...
	 */
	raw_spin_lock_irqsave(&bg->lock, irq_flags);

	idx = uni_task_group_idx(p);

	group_boost_tasks_update(p, cpu, idx, DEQUEUE_TASK);

	raw_spin_unlock_irqrestore(&bg->lock, irq_flags);
}
#endif

static void init_tg_params(struct task_group *tg)
{
	struct uni_task_group *uni_tg;

	uni_tg = (struct uni_task_group *) tg->android_vendor_data1;

	uni_tg->boost = 0;
#if IS_ENABLED(CONFIG_SCHED_WALT)
	if (uni_tg->idx == TOP_APP || uni_tg->idx == CAMERA_DAEMON)
		uni_tg->account_wait_time = 1;
	else
		uni_tg->account_wait_time = 0;

	uni_tg->init_task_load_pct = 0;
	uni_tg->prefer_active = 0;
#endif
}

static void update_root_group(struct cgroup_subsys_state *css)
{
	struct task_group *tg = css_tg(css);
	struct uni_task_group *uni_tg;

	uni_tg = (struct uni_task_group *) tg->android_vendor_data1;

	uni_tg->idx = ROOT_GROUP;
	cgrp_table[ROOT_GROUP] = uni_tg;

	init_tg_params(tg);
}

static void update_task_group(struct cgroup_subsys_state *css)
{
	struct task_group *tg = css_tg(css);
	struct uni_task_group *uni_tg;

	uni_tg = (struct uni_task_group *) tg->android_vendor_data1;

	if (!strcmp(css->cgroup->kn->name, "top-app")) {
		uni_tg->idx = TOP_APP;
		cgrp_table[TOP_APP] = uni_tg;
#ifdef CONFIG_UNISOC_SCHED_VIP_TASK
	} else if (!strcmp(css->cgroup->kn->name, "vip-sched")) {
		uni_tg->idx = VIP_GROUP;
		cgrp_table[VIP_GROUP] = uni_tg;
#endif
	} else if (!strcmp(css->cgroup->kn->name, "foreground")) {
		uni_tg->idx = FOREGROUND;
		cgrp_table[FOREGROUND] = uni_tg;
	} else if (!strcmp(css->cgroup->kn->name, "camera-daemon")) {
		uni_tg->idx = CAMERA_DAEMON;
		cgrp_table[CAMERA_DAEMON] = uni_tg;
	} else if (!strcmp(css->cgroup->kn->name, "background")) {
		uni_tg->idx = BACKGROUND;
		cgrp_table[BACKGROUND] = uni_tg;
	} else if (!strcmp(css->cgroup->kn->name, "system")) {
		uni_tg->idx = SYSTEM_GROUP;
		cgrp_table[SYSTEM_GROUP] = uni_tg;
	} else {
		uni_tg->idx = OTHER_GROUPS;
		cgrp_table[OTHER_GROUPS] = uni_tg;
	}

	init_tg_params(tg);
}

#ifdef CONFIG_UNISOC_GROUP_BOOST
static int sched_group_boost_handler(struct ctl_table *table, int write,
				 void __user *buffer, size_t *lenp,
				 loff_t *ppos)
{
	int ret;
	unsigned long grp_id = (unsigned long)table->data;
	static DEFINE_MUTEX(mutex);
	struct uni_task_group *uni_tg = cgrp_table[grp_id];
	int old_boost_val, boost_val;
	struct ctl_table tmp = {
		.data   = &boost_val,
		.maxlen = sizeof(int),
		.mode   = table->mode,
	};

	mutex_lock(&mutex);

	if (uni_tg) {
		old_boost_val = uni_tg->boost;
	} else {
		ret = -EINVAL;
		goto unlock_mutex;
	}

	if (!write) {
		boost_val = old_boost_val;
		ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
		goto unlock_mutex;
	}

	ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	/* check if pct values are valid */
	if (boost_val > 100 || boost_val < -100) {
		ret = -EINVAL;
		goto unlock_mutex;
	}

	if (old_boost_val != boost_val) {
		uni_tg->boost = boost_val;
		group_boost_val_update(grp_id, boost_val);
	}

unlock_mutex:
	mutex_unlock(&mutex);

	return ret;
}
#endif
#if IS_ENABLED(CONFIG_SCHED_WALT)
static int sched_account_wait_time_handler(struct ctl_table *table, int write,
				 void __user *buffer, size_t *lenp,
				 loff_t *ppos)
{
	int ret;
	unsigned long grp_id = (unsigned long)table->data;
	static DEFINE_MUTEX(mutex);
	struct uni_task_group *uni_tg = cgrp_table[grp_id];
	int val;
	struct ctl_table tmp = {
		.data   = &val,
		.maxlen = sizeof(int),
		.mode   = table->mode,
	};

	mutex_lock(&mutex);

	if (!uni_tg) {
		ret = -EINVAL;
		goto unlock_mutex;
	}

	if (!write) {
		val = uni_tg->account_wait_time;
		ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
		goto unlock_mutex;
	}

	ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	uni_tg->account_wait_time = !!val;

unlock_mutex:
	mutex_unlock(&mutex);

	return ret;
}

static int sched_init_load_pct_handler(struct ctl_table *table, int write,
				 void __user *buffer, size_t *lenp,
				 loff_t *ppos)
{
	int ret;
	unsigned long grp_id = (unsigned long)table->data;
	static DEFINE_MUTEX(mutex);
	struct uni_task_group *uni_tg = cgrp_table[grp_id];
	int val;
	struct ctl_table tmp = {
		.data   = &val,
		.maxlen = sizeof(int),
		.mode   = table->mode,
	};

	mutex_lock(&mutex);

	if (!uni_tg) {
		ret = -EINVAL;
		goto unlock_mutex;
	}

	if (!write) {
		val = uni_tg->init_task_load_pct;
		ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
		goto unlock_mutex;
	}

	ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	/* check if pct values are valid */
	if (val > 100 || val < 0) {
		ret = -EINVAL;
		goto unlock_mutex;
	}

	uni_tg->init_task_load_pct = val;

unlock_mutex:
	mutex_unlock(&mutex);

	return ret;
}
#endif

struct ctl_table root_grp_table[] = {
#ifdef CONFIG_UNISOC_GROUP_BOOST
	{
		.procname	= "boost",
		.data		= (int *)ROOT_GROUP,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= sched_group_boost_handler,
	},
#endif
#if IS_ENABLED(CONFIG_SCHED_WALT)
	{
		.procname	= "account_wait_time",
		.data		= (int *)ROOT_GROUP,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_account_wait_time_handler,
	},
	{
		.procname	= "init_task_load_pct",
		.data		= (int *)ROOT_GROUP,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_init_load_pct_handler,
	},
#endif
	{ },
};

#ifdef CONFIG_UNISOC_SCHED_VIP_TASK
struct ctl_table vip_table[] = {
#ifdef CONFIG_UNISOC_GROUP_BOOST
	{
		.procname	= "boost",
		.data		= (int *)VIP_GROUP,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= sched_group_boost_handler,
	},
#endif
	{
		.procname	= "account_wait_time",
		.data		= (int *)VIP_GROUP,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_account_wait_time_handler,
	},
	{
		.procname	= "init_task_load_pct",
		.data		= (int *)VIP_GROUP,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_init_load_pct_handler,
	},
	{ },
};
#endif

struct ctl_table topapp_table[] = {
#ifdef CONFIG_UNISOC_GROUP_BOOST
	{
		.procname	= "boost",
		.data		= (int *)TOP_APP,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= sched_group_boost_handler,
	},
#endif
#if IS_ENABLED(CONFIG_SCHED_WALT)
	{
		.procname	= "account_wait_time",
		.data		= (int *)TOP_APP,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_account_wait_time_handler,
	},
	{
		.procname	= "init_task_load_pct",
		.data		= (int *)TOP_APP,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_init_load_pct_handler,
	},
#endif
	{ },
};

struct ctl_table foreground_table[] = {
#ifdef CONFIG_UNISOC_GROUP_BOOST
	{
		.procname	= "boost",
		.data		= (int *)FOREGROUND,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= sched_group_boost_handler,
	},
#endif
#if IS_ENABLED(CONFIG_SCHED_WALT)
	{
		.procname	= "account_wait_time",
		.data		= (int *)FOREGROUND,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_account_wait_time_handler,
	},
	{
		.procname	= "init_task_load_pct",
		.data		= (int *)FOREGROUND,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_init_load_pct_handler,
	},
#endif
	{ },
};

struct ctl_table camera_daemon_table[] = {
#ifdef CONFIG_UNISOC_GROUP_BOOST
	{
		.procname	= "boost",
		.data		= (int *)CAMERA_DAEMON,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= sched_group_boost_handler,
	},
#endif
#if IS_ENABLED(CONFIG_SCHED_WALT)
	{
		.procname	= "account_wait_time",
		.data		= (int *)CAMERA_DAEMON,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_account_wait_time_handler,
	},
	{
		.procname	= "init_task_load_pct",
		.data		= (int *)CAMERA_DAEMON,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_init_load_pct_handler,
	},
#endif
	{ },
};

struct ctl_table background_table[] = {
#ifdef CONFIG_UNISOC_GROUP_BOOST
	{
		.procname	= "boost",
		.data		= (int *)BACKGROUND,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= sched_group_boost_handler,
	},
#endif
#if IS_ENABLED(CONFIG_SCHED_WALT)
	{
		.procname	= "account_wait_time",
		.data		= (int *)BACKGROUND,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_account_wait_time_handler,
	},
	{
		.procname	= "init_task_load_pct",
		.data		= (int *)BACKGROUND,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_init_load_pct_handler,
	},
#endif
	{ },
};

struct ctl_table system_table[] = {
#ifdef CONFIG_UNISOC_GROUP_BOOST
	{
		.procname	= "boost",
		.data		= (int *)SYSTEM_GROUP,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= sched_group_boost_handler,
	},
#endif
#if IS_ENABLED(CONFIG_SCHED_WALT)
	{
		.procname	= "account_wait_time",
		.data		= (int *)SYSTEM_GROUP,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_account_wait_time_handler,
	},
	{
		.procname	= "init_task_load_pct",
		.data		= (int *)SYSTEM_GROUP,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_init_load_pct_handler,
	},
#endif
	{ },
};

struct ctl_table other_group_table[] = {
#ifdef CONFIG_UNISOC_GROUP_BOOST
	{
		.procname	= "boost",
		.data		= (int *)OTHER_GROUPS,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= sched_group_boost_handler,
	},
#endif
#if IS_ENABLED(CONFIG_SCHED_WALT)
	{
		.procname	= "account_wait_time",
		.data		= (int *)OTHER_GROUPS,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_account_wait_time_handler,
	},
	{
		.procname	= "init_task_load_pct",
		.data		= (int *)OTHER_GROUPS,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_init_load_pct_handler,
	},
#endif
	{ },
};

struct ctl_table boost_table[] = {
	{
		.procname	= "root",
		.mode		= 0555,
		.child		= root_grp_table,
	},
#ifdef CONFIG_UNISOC_SCHED_VIP_TASK
	{
		.procname	= "vip-sched",
		.mode		= 0555,
		.child		= vip_table,
	},
#endif
	{
		.procname	= "top-app",
		.mode		= 0555,
		.child		= topapp_table,
	},
	{
		.procname	= "foreground",
		.mode		= 0555,
		.child		= foreground_table,
	},
	{
		.procname	= "camera-daemon",
		.mode		= 0555,
		.child		= camera_daemon_table,
	},
	{
		.procname	= "background",
		.mode		= 0555,
		.child		= background_table,
	},
	{
		.procname	= "system",
		.mode		= 0555,
		.child		= system_table,
	},
	{
		.procname	= "others",
		.mode		= 0555,
		.child		= other_group_table,
	},
	{ }
};

static void android_rvh_cpu_cgroup_online(void *data, struct cgroup_subsys_state *css)
{
	if (unlikely(uni_sched_disabled))
		return;

	update_task_group(css);
}

void init_group_control(void)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;
#ifdef CONFIG_UNISOC_GROUP_BOOST
	struct boost_groups *bg;
	int cpu;

	/* Initialize the per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);
		memset(bg, 0, sizeof(struct boost_groups));
		raw_spin_lock_init(&bg->lock);
	}
#endif
	rcu_read_lock();
	update_root_group(top_css);
	css_for_each_child(css, top_css)
		update_task_group(css);
	rcu_read_unlock();

	register_trace_android_rvh_cpu_cgroup_online(android_rvh_cpu_cgroup_online, NULL);
}

