// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#undef pr_fmt
#define pr_fmt(fmt) "Task-Turbo: " fmt

#include <linux/list.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <uapi/linux/sched/types.h>
#include <mt-plat/turbo_common.h>
#include <task_turbo.h>
#define CREATE_TRACE_POINTS
#include <trace_task_turbo.h>

#define TOP_APP_GROUP_ID	4
#define TURBO_PID_COUNT		8
#define RENDER_THREAD_NAME	"RenderThread"
#define TURBO_ENABLE		1
#define TURBO_DISABLE		0
#define SCHED_PREFER_BCPU	TURBO_ENABLE
#define SCHED_PREFER_NONE	TURBO_DISABLE

static uint32_t latency_turbo = SUB_FEAT_LOCK | SUB_FEAT_BINDER |
				SUB_FEAT_SCHED;
static uint32_t launch_turbo =  SUB_FEAT_LOCK | SUB_FEAT_BINDER |
				SUB_FEAT_SCHED | SUB_FEAT_FLAVOR_BIGCORE;
static DEFINE_MUTEX(TURBO_MUTEX_LOCK);
static pid_t turbo_pid[TURBO_PID_COUNT] = {0};
static unsigned int task_turbo_feats;

inline bool latency_turbo_enable(void)
{
	return task_turbo_feats == latency_turbo;
}

inline bool launch_turbo_enable(void)
{
	return task_turbo_feats == launch_turbo;
}

void init_turbo_attr(struct task_struct *p,
		     struct task_struct *parent)
{
	p->turbo = TURBO_DISABLE;
	p->render = 0;
	atomic_set(&(p->inherit_types), 0);
	p->inherit_cnt = 0;
	if (is_turbo_task(parent))
		p->cpu_prefer = SCHED_PREFER_NONE;
}

bool is_turbo_task(struct task_struct *p)
{
	return p && (p->turbo || atomic_read(&p->inherit_types));
}
EXPORT_SYMBOL(is_turbo_task);

int get_turbo_feats(void)
{
	return task_turbo_feats;
}

bool sub_feat_enable(int type)
{
	return get_turbo_feats() & type;
}

static inline void set_scheduler_tuning(struct task_struct *task)
{
	int cur_nice = task_nice(task);

	if (!fair_policy(task->policy))
		return;

	if (!sub_feat_enable(SUB_FEAT_SCHED))
		return;

	if (sub_feat_enable(SUB_FEAT_FLAVOR_BIGCORE))
		sched_set_cpuprefer(task->pid, SCHED_PREFER_BCPU);

	/* trigger renice for turbo task */
	set_user_nice(task, 0xbeef);

	trace_sched_turbo_nice_set(task, NICE_TO_PRIO(cur_nice), task->prio);
}

static inline void unset_scheduler_tuning(struct task_struct *task)
{
	int cur_prio = task->prio;

	if (!fair_policy(task->policy))
		return;

	sched_set_cpuprefer(task->pid, SCHED_PREFER_NONE);
	set_user_nice(task, 0xbeee);

	trace_sched_turbo_nice_set(task, cur_prio, task->prio);
}

/*
 * set task to turbo by pid
 */
static int set_turbo_task(int pid, int val)
{
	struct task_struct *p;
	int retval = 0;

	if (pid < 0 || pid > PID_MAX_DEFAULT)
		return -EINVAL;

	if (val < 0 || val > 1)
		return -EINVAL;

	rcu_read_lock();
	p = find_task_by_vpid(pid);

	if (p != NULL) {
		get_task_struct(p);
		p->turbo = val;
		if (p->turbo == TURBO_ENABLE)
			set_scheduler_tuning(p);
		else
			unset_scheduler_tuning(p);
		trace_turbo_set(p);
		put_task_struct(p);
	} else
		retval = -ESRCH;
	rcu_read_unlock();

	return retval;
}

static inline int unset_turbo_task(int pid)
{
	return set_turbo_task(pid, TURBO_DISABLE);
}

static int set_task_turbo_feats(const char *buf,
				const struct kernel_param *kp)
{
	int ret = 0, i;
	unsigned int val;

	ret = kstrtouint(buf, 0, &val);


	mutex_lock(&TURBO_MUTEX_LOCK);
	if (val == latency_turbo ||
	    val == launch_turbo  || val == 0)
		ret = param_set_uint(buf, kp);
	else
		ret = -EINVAL;

	/* if disable turbo, remove all turbo tasks */
	/* mutex_lock(&TURBO_MUTEX_LOCK); */
	if (val == 0) {
		for (i = 0; i < TURBO_PID_COUNT; i++) {
			if (turbo_pid[i]) {
				unset_turbo_task(turbo_pid[i]);
				turbo_pid[i] = 0;
			}
		}
	}
	mutex_unlock(&TURBO_MUTEX_LOCK);

	if (!ret)
		pr_info("task_turbo_feats is change to %d successfully",
				task_turbo_feats);
	return ret;
}

static struct kernel_param_ops task_turbo_feats_param_ops = {
	.set = set_task_turbo_feats,
	.get = param_get_uint,
};

param_check_uint(feats, &task_turbo_feats);
module_param_cb(feats, &task_turbo_feats_param_ops, &task_turbo_feats, 0644);
MODULE_PARM_DESC(feats, "enable task turbo features if needed");

static bool add_turbo_list_locked(pid_t pid);
static void remove_turbo_list_locked(pid_t pid);

/*
 * use pid set turbo task
 */
static int add_turbo_list_by_pid(pid_t pid)
{
	int retval = -EINVAL;

	if (!task_turbo_feats)
		return retval;

	if (pid < 0 || pid > PID_MAX_DEFAULT)
		return retval;

	mutex_lock(&TURBO_MUTEX_LOCK);
	if (!add_turbo_list_locked(pid))
		goto unlock;

	retval = set_turbo_task(pid, TURBO_ENABLE);
unlock:
	mutex_unlock(&TURBO_MUTEX_LOCK);
	return retval;
}

static pid_t turbo_pid_param;
static int set_turbo_task_param(const char *buf,
				const struct kernel_param *kp)
{
	int retval = 0;
	pid_t pid;

	retval = kstrtouint(buf, 0, &pid);

	if (!retval)
		retval = add_turbo_list_by_pid(pid);

	if (!retval)
		turbo_pid_param = pid;

	return retval;
}

static struct kernel_param_ops turbo_pid_param_ops = {
	.set = set_turbo_task_param,
	.get = param_get_int,
};

param_check_uint(turbo_pid, &turbo_pid_param);
module_param_cb(turbo_pid, &turbo_pid_param_ops, &turbo_pid_param, 0644);
MODULE_PARM_DESC(turbo_pid, "set turbo task by pid");

static int unset_turbo_list_by_pid(pid_t pid)
{
	int retval = -EINVAL;

	if (pid < 0 || pid > PID_MAX_DEFAULT)
		return retval;

	mutex_lock(&TURBO_MUTEX_LOCK);
	remove_turbo_list_locked(pid);
	retval = unset_turbo_task(pid);
	mutex_unlock(&TURBO_MUTEX_LOCK);
	return retval;
}

static pid_t unset_turbo_pid_param;
static int unset_turbo_task_param(const char *buf,
				  const struct kernel_param *kp)
{
	int retval = 0;
	pid_t pid;

	retval = kstrtouint(buf, 0, &pid);

	if (!retval)
		retval = unset_turbo_list_by_pid(pid);

	if (!retval)
		unset_turbo_pid_param = pid;

	return retval;
}

static struct kernel_param_ops unset_turbo_pid_param_ops = {
	.set = unset_turbo_task_param,
	.get = param_get_int,
};

param_check_uint(unset_turbo_pid, &unset_turbo_pid_param);
module_param_cb(unset_turbo_pid, &unset_turbo_pid_param_ops,
		&unset_turbo_pid_param, 0644);
MODULE_PARM_DESC(unset_turbo_pid, "unset turbo task by pid");

static inline int get_st_group_id(struct task_struct *task)
{
#if IS_ENABLED(CONFIG_SCHED_TUNE)
	const int subsys_id = schedtune_cgrp_id;
	struct cgroup *grp;

	rcu_read_lock();
	grp = task_cgroup(task, subsys_id);
	rcu_read_unlock();
	return grp->id;
#else
	return 0;
#endif
}

static inline bool cgroup_check_set_turbo(struct task_struct *p)
{
	if (!launch_turbo_enable())
		return false;

	if (p->turbo)
		return false;

	/* set critical tasks for UI or UX to turbo */
	return (p->render ||
	       (p == p->group_leader &&
		p->real_parent->pid != 1));
}

/*
 * record task to turbo list
 */
static bool add_turbo_list_locked(pid_t pid)
{
	int i, free_idx = -1;
	bool ret = false;

	if (unlikely(!get_turbo_feats()))
		goto done;

	for (i = 0; i < TURBO_PID_COUNT; i++) {
		if (free_idx < 0 && !turbo_pid[i])
			free_idx = i;

		if (unlikely(turbo_pid[i] == pid)) {
			free_idx = i;
			break;
		}
	}

	if (free_idx >= 0) {
		turbo_pid[free_idx] = pid;
		ret = true;
	}
done:
	return ret;
}

static void add_turbo_list(struct task_struct *p)
{
	mutex_lock(&TURBO_MUTEX_LOCK);
	if (add_turbo_list_locked(p->pid)) {
		p->turbo = TURBO_ENABLE;
		set_scheduler_tuning(p);
		trace_turbo_set(p);
	}
	mutex_unlock(&TURBO_MUTEX_LOCK);
}

/*
 * remove task from turbo list
 */
static void remove_turbo_list_locked(pid_t pid)
{
	int i;

	for (i = 0; i < TURBO_PID_COUNT; i++) {
		if (turbo_pid[i] == pid) {
			turbo_pid[i] = 0;
			break;
		}
	}
}

static void remove_turbo_list(struct task_struct *p)
{
	mutex_lock(&TURBO_MUTEX_LOCK);
	remove_turbo_list_locked(p->pid);
	p->turbo = TURBO_DISABLE;
	unset_scheduler_tuning(p);
	trace_turbo_set(p);
	mutex_unlock(&TURBO_MUTEX_LOCK);
}

void cgroup_set_turbo_task(struct task_struct *p)
{
	/* if group stune of top-app */
	if (get_st_group_id(p) == TOP_APP_GROUP_ID) {
		if (!cgroup_check_set_turbo(p))
			return;
		add_turbo_list(p);
	} else { /* other group */
		if (p->turbo)
			remove_turbo_list(p);
	}
}

extern void sys_set_turbo_task(struct task_struct *p)
{
	if (strcmp(p->comm, RENDER_THREAD_NAME))
		return;

	if (!launch_turbo_enable())
		return;

	if (get_st_group_id(p) != TOP_APP_GROUP_ID)
		return;

	p->render = 1;
	add_turbo_list(p);
}

/**************************************
 *                                    *
 * inherit turbo function             *
 *                                    *
 *************************************/

#define INHERIT_THRESHOLD	4

#define type_offset(type)		 (type * 4)
#define type_number(type)		 (1U << type_offset(type))
#define get_value_with_type(value, type)				\
	(value & ((unsigned int)(0x0000000f) << (type_offset(type))))

static inline void sub_inherit_types(struct task_struct *task, int type)
{
	atomic_sub(type_number(type), &task->inherit_types);
}

static inline void add_inherit_types(struct task_struct *task, int type)
{
	atomic_add(type_number(type), &task->inherit_types);
}

bool test_turbo_cnt(struct task_struct *task)
{
	/* TODO:limit number should be disscuss */
	return task->inherit_cnt < INHERIT_THRESHOLD;
}

bool is_inherit_turbo(struct task_struct *task, int type)
{
	unsigned int inherit_types;

	if (!task)
		return false;

	inherit_types = atomic_read(&task->inherit_types);

	if (inherit_types == 0)
		return false;

	return get_value_with_type(inherit_types, type) > 0;
}

inline bool should_set_inherit_turbo(struct task_struct *task)
{
	return is_turbo_task(task) && test_turbo_cnt(task);
}

bool start_turbo_inherit(struct task_struct *task,
			int type,
			int cnt)
{
	if (type <= START_INHERIT || type >= END_INHERIT)
		return false;

	add_inherit_types(task, type);
	if (task->inherit_cnt < cnt + 1)
		task->inherit_cnt = cnt + 1;

	/* scheduler tuning start */
	set_scheduler_tuning(task);
	return true;
}

bool stop_turbo_inherit(struct task_struct *task,
			 int type)
{
	unsigned int inherit_types;
	bool ret = false;

	if (type <= START_INHERIT || type >= END_INHERIT)
		goto done;

	inherit_types = atomic_read(&task->inherit_types);
	if (inherit_types == 0)
		goto done;

	sub_inherit_types(task, type);
	inherit_types = atomic_read(&task->inherit_types);
	if (inherit_types > 0)
		goto done;

	/* scheduler tuning stop */
	unset_scheduler_tuning(task);
	task->inherit_cnt = 0;
	ret = true;
done:
	return ret;
}

bool binder_start_turbo_inherit(struct task_struct *from,
			   struct task_struct *to)
{
	bool ret = false;

	if (!sub_feat_enable(SUB_FEAT_BINDER))
		goto done;

	if (!from || !to)
		goto done;

	if (!is_turbo_task(from) ||
		!test_turbo_cnt(from))
		goto done;

	if (!is_turbo_task(to)) {
		ret = start_turbo_inherit(to, BINDER_INHERIT,
					from->inherit_cnt);
		trace_turbo_inherit_start(from, to);
	}
done:
	return ret;
}

void binder_stop_turbo_inherit(struct task_struct *p)
{
	if (is_inherit_turbo(p, BINDER_INHERIT))
		stop_turbo_inherit(p, BINDER_INHERIT);
	trace_turbo_inherit_end(p);
}

enum rwsem_waiter_type {
	RWSEM_WAITING_FOR_WRITE,
	RWSEM_WAITING_FOR_READ
};

struct rwsem_waiter {
	struct list_head list;
	struct task_struct *task;
	enum rwsem_waiter_type type;
};

#define RWSEM_ANONYMOUSLY_OWNED	(1UL << 0)
#define RWSEM_READER_OWNED	((struct task_struct *)RWSEM_ANONYMOUSLY_OWNED)

static inline bool rwsem_owner_is_writer(struct task_struct *owner)
{
	return owner && owner != RWSEM_READER_OWNED;
}

void rwsem_start_turbo_inherit(struct rw_semaphore *sem)
{
	bool should_inherit;
	struct task_struct *owner;
	struct task_struct *cur = current;

	if (!sub_feat_enable(SUB_FEAT_LOCK))
		return;

	owner = READ_ONCE(sem->owner);
	should_inherit = should_set_inherit_turbo(cur);
	if (should_inherit) {
		if (rwsem_owner_is_writer(owner) &&
				!is_turbo_task(owner) &&
				!sem->turbo_owner) {
			start_turbo_inherit(owner,
					    RWSEM_INHERIT,
					    cur->inherit_cnt);
			sem->turbo_owner = owner;
			trace_turbo_inherit_start(cur, owner);
		}
	}
}

void rwsem_stop_turbo_inherit(struct rw_semaphore *sem)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&sem->wait_lock, flags);
	if (sem->turbo_owner == current) {
		stop_turbo_inherit(current, RWSEM_INHERIT);
		sem->turbo_owner = NULL;
		trace_turbo_inherit_end(current);
	}
	raw_spin_unlock_irqrestore(&sem->wait_lock, flags);
}

void rwsem_list_add(struct task_struct *task,
		    struct list_head *entry,
		    struct list_head *head)
{

	if (!sub_feat_enable(SUB_FEAT_LOCK)) {
		list_add_tail(entry, head);
		return;
	}

	if (is_turbo_task(task)) {
		struct list_head *pos = NULL;
		struct list_head *n = NULL;
		struct rwsem_waiter *waiter = NULL;

		/* insert turbo task pior to first non-turbo task */
		list_for_each_safe(pos, n, head) {
			waiter = list_entry(pos,
					struct rwsem_waiter, list);
			if (!is_turbo_task(waiter->task)) {
				list_add(entry, waiter->list.prev);
				return;
			}
		}
	}

	list_add_tail(entry, head);
}
