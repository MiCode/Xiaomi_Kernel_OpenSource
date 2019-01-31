/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/preempt.h>
#include <linux/proc_fs.h>
#include <linux/trace_events.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>

#include <mt-plat/fpsgo_common.h>

#include <trace/events/fpsgo.h>

#include "xgf.h"
#include "fpsgo_base.h"

static DEFINE_MUTEX(xgf_main_lock);
static HLIST_HEAD(xgf_procs);

static int xgf_enable;
static struct dentry *debugfs_xgf_dir;
static unsigned long long last_check2recycle_ts;
static unsigned long long last_check2recycle_dep_ts;

#define MAX_RENDER_DEPS 1024
static int render_dep_count;
struct render_dep render_deps[MAX_RENDER_DEPS];
static DEFINE_SPINLOCK(render_spinlock);

int (*xgf_est_slptime_fp)(
	struct xgf_proc *proc,
	unsigned long long *slptime,
	struct xgf_tick *ref,
	struct xgf_tick *now,
	pid_t r_pid);
EXPORT_SYMBOL(xgf_est_slptime_fp);

static inline void xgf_lock(const char *tag)
{
	mutex_lock(&xgf_main_lock);
}

static inline void xgf_unlock(const char *tag)
{
	mutex_unlock(&xgf_main_lock);
}

void xgf_lockprove(const char *tag)
{
	WARN_ON(!mutex_is_locked(&xgf_main_lock));
}
EXPORT_SYMBOL(xgf_lockprove);

void xgf_trace(const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;


	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);
	trace_xgf_log(log);
}
EXPORT_SYMBOL(xgf_trace);

static void *xgf_alloc(int i32Size)
{
	void *pvBuf;

	if (i32Size <= PAGE_SIZE)
		pvBuf = kmalloc(i32Size, GFP_ATOMIC);
	else
		pvBuf = vmalloc(i32Size);

	return pvBuf;
}

static void xgf_free(void *pvBuf)
{
	kvfree(pvBuf);
}

void fpsgo_update_render_dep(struct task_struct *p)
{
	unsigned long flags;

	if (task_tgid_nr(current) != task_tgid_nr(p))
		return;

	if (p->state != TASK_INTERRUPTIBLE)
		return;

	if (in_irq() || in_softirq() || in_interrupt())
		return;

	spin_lock_irqsave(&render_spinlock, flags);
	if (render_dep_count < MAX_RENDER_DEPS) {
		render_deps[render_dep_count].currentpid =
			task_pid_nr(current);
		render_deps[render_dep_count].currenttgid =
			current->tgid;
		render_deps[render_dep_count].becalledpid =
			task_pid_nr(p);
		render_deps[render_dep_count].becalledtgid =
			p->tgid;
		render_dep_count++;
	}
	spin_unlock_irqrestore(&render_spinlock, flags);
}

static inline void xgf_timer_systrace(const void * const timer,
				      int value)
{
	xgf_lockprove(__func__);

	fpsgo_systrace_c_xgf(task_tgid_nr(current), value,
			     "%d:%s timer-%p", task_pid_nr(current),
			     current->comm, timer);
}

static inline int xgf_is_enable(void)
{
	xgf_lockprove(__func__);

	return xgf_enable;
}

/**
 * TODO: should get function pointer while xgf_enable
 */
static unsigned long long ged_get_time(void)
{
	unsigned long long temp;

	preempt_disable();
	temp = cpu_clock(smp_processor_id());
	preempt_enable();
	return temp;
}

static struct xgf_deps *xgf_get_deps(
	pid_t tid, struct xgf_proc *proc, int force, int init_render)
{
	struct rb_node **p = &proc->deps_rec.rb_node;
	struct rb_node *parent = NULL;
	struct xgf_deps *xd = NULL;
	pid_t tp;

	xgf_lockprove(__func__);

	while (*p) {
		parent = *p;
		xd = rb_entry(parent, struct xgf_deps, rb_node);

		tp = xd->tid;
		if (tid < tp)
			p = &(*p)->rb_left;
		else if (tid > tp)
			p = &(*p)->rb_right;
		else
			return xd;
	}

	if (!force)
		return NULL;

	xd = kzalloc(sizeof(*xd), GFP_KERNEL);
	if (!xd)
		return NULL;

	xd->tid = tid;
	xd->render_dep = !!init_render;
	xd->render_count = 0;
	xd->render_dep_deep = 0;
	xd->render_pre_count = 0;

	rb_link_node(&xd->rb_node, parent, p);
	rb_insert_color(&xd->rb_node, &proc->deps_rec);
	return xd;
}

void fpsgo_create_render_dep(void)
{
	struct xgf_deps *xd_caller;
	struct xgf_deps *xd_becalled;
	struct xgf_proc *proc_iter;
	struct hlist_node *n;

	struct rb_root *r;
	struct rb_node *rbn;
	struct xgf_deps *iter;

	unsigned long flags;
	int render_dep_count_buff, i, diff, one_quarter, one_eighth;
	struct render_dep *render_deps_buff;

	xgf_trace("fpsgo_create_render_dep");

	render_deps_buff = xgf_alloc(sizeof(*render_deps_buff)*MAX_RENDER_DEPS);

	if (!render_deps_buff)
		return;

	spin_lock_irqsave(&render_spinlock, flags);
	render_dep_count_buff = 0;
	if (render_dep_count) {
		memcpy(render_deps_buff, &render_deps, sizeof(render_deps));
		render_dep_count_buff = render_dep_count;
		render_dep_count = 0;
	}
	spin_unlock_irqrestore(&render_spinlock, flags);

	xgf_lock(__func__);

	if (!render_dep_count_buff)
		goto out;

	hlist_for_each_entry_safe(proc_iter, n, &xgf_procs, hlist) {
		r = &proc_iter->deps_rec;
		for (rbn = rb_first(r); rbn != NULL; rbn = rb_next(rbn)) {
			iter = rb_entry(rbn, struct xgf_deps, rb_node);
			diff = iter->render_count - iter->render_pre_count;
			one_quarter =
				proc_iter->render_thread_called_count >> 2;
			one_eighth =
				proc_iter->render_thread_called_count >> 3;
			if (diff > one_quarter) {
				iter->render_dep = 1;

				if (iter->render_dep_deep < 2)
					iter->render_dep_deep++;
			} else if ((diff < one_eighth) && iter->render_dep) {
				if (iter->render_dep_deep > 0)
					iter->render_dep_deep--;

				if ((!iter->render_dep_deep) &&
					(iter->tid != proc_iter->render))
					iter->render_dep = 0;
			}
			iter->render_pre_count = iter->render_count;
		}
		proc_iter->render_thread_called_count = 0;
		proc_iter->dep_timer_count = 0;
	}

	for (i = 0; i < render_dep_count_buff; i++) {
		hlist_for_each_entry_safe(proc_iter, n, &xgf_procs, hlist) {
			if (proc_iter->parent !=
					render_deps_buff[i].becalledtgid)
				continue;

			if (render_deps_buff[i].becalledpid ==
					proc_iter->render)
				proc_iter->render_thread_called_count++;

			xd_becalled =
				xgf_get_deps(render_deps_buff[i].becalledpid,
						proc_iter, 0, 0);

			if (!xd_becalled)
				continue;

			if (xd_becalled->render_dep) {
				xd_caller =
					xgf_get_deps(
						render_deps_buff[i].currentpid,
						proc_iter, 1, 0);

				if (!xd_caller)
					continue;

				xd_caller->render_count++;
				if (unlikely(xd_caller->render_count ==
							INT_MAX)) {
					xd_caller->render_count = 100;
					xd_caller->render_pre_count = 100;
				}
			}
		}
	}
out:
	xgf_unlock(__func__);
	xgf_free(render_deps_buff);
}

static void fpsgo_init_render_dep(pid_t rpid)
{
	struct task_struct *tsk;
	struct xgf_deps *xd;
	struct xgf_proc *iter;
	struct hlist_node *n;
	struct task_struct *p;

	xgf_lockprove(__func__);

	rcu_read_lock();
	tsk = find_task_by_vpid(rpid);
	if (tsk)
		get_task_struct(tsk);
	rcu_read_unlock();

	if (!tsk)
		return;

	hlist_for_each_entry_safe(iter, n, &xgf_procs, hlist) {
		if ((iter->parent != tsk->tgid) || (iter->render != rpid))
			continue;

		rcu_read_lock();
		p = find_task_by_vpid(iter->render);
		rcu_read_unlock();

		if (!p)
			continue;

		xd = xgf_get_deps(rpid, iter, 1, 1);
	}
	put_task_struct(tsk);
}

static void xgf_update_tick(struct xgf_proc *proc, struct xgf_tick *tick,
			    unsigned long long ts, unsigned long long runtime)
{
	struct task_struct *p;

	xgf_lockprove(__func__);

	/* render thread is not set yet */
	if (!proc->render)
		return;

	tick->ts = ts ? ts : ged_get_time();

	if (runtime) {
		tick->runtime = runtime;
		return;
	}

	rcu_read_lock();
	p = find_task_by_vpid(proc->render);
	if (p)
		get_task_struct(p);
	rcu_read_unlock();
	if (!p) {
		pr_notice("%s: get task %d failed\n", __func__,
			  proc->render);
		proc->render = 0;

		return;
	}

	tick->runtime = task_sched_runtime(p);
	put_task_struct(p);
}

static void xgf_update_dep_tick(struct xgf_proc *proc, struct xgf_deps *deps,
				struct xgf_tick *tick, unsigned long long ts)
{
	struct task_struct *p;

	xgf_lockprove(__func__);

	/* render thread or deps is not set yet */
	if (!proc->render || !deps->tid)
		return;

	tick->ts = ts;

	rcu_read_lock();
	p = find_task_by_vpid(deps->tid);
	if (p)
		get_task_struct(p);
	rcu_read_unlock();
	if (!p) {
		pr_notice("%s: get deps task %d failed\n", __func__,
			  deps->tid);
		deps->tid = 0;
		return;
	}

	tick->runtime = task_sched_runtime(p);

	put_task_struct(p);
}

static inline void xgf_timer_recycle(struct xgf_proc *proc,
	unsigned long long now_ts, long long recycle_period)
{
	struct xgf_timer *iter;
	struct hlist_node *t;
	long long diff;

	xgf_lockprove(__func__);

	hlist_for_each_entry_safe(iter, t, &proc->timer_head, hlist) {
		diff = (long long)now_ts - (long long)iter->fire.ts;
		/* clean timer expired or fire over recycle_period ago */
		if ((iter->expired &&
				(iter->expire.ts < now_ts)) ||
				(diff > recycle_period)) {
			hlist_del(&iter->hlist);
			kfree(iter);
		}
	}
}

static inline void xgf_blacked_recycle(struct rb_root *root,
	unsigned long long now_ts, long long recycle_period)
{
	struct rb_node *n;
	long long diff;

	xgf_lockprove(__func__);

	n = rb_first(root);
	while (n) {
		struct xgf_timer *iter;

		iter = rb_entry(n, struct xgf_timer, rb_node);

		diff = (long long)now_ts - (long long)iter->fire.ts;
		if (!iter->expire.ts && diff < recycle_period) {
			n = rb_next(n);
			continue;
		}

		/* clean activity over recycle_period ago */
		diff = (long long)now_ts - (long long)iter->expire.ts;
		if (diff < 0LL || diff < recycle_period) {
			n = rb_next(n);
			continue;
		}

		rb_erase(n, root);
		kfree(iter);

		n = rb_first(root);
	}
}

static inline void xgf_dep_recycle(struct rb_root *root,
	unsigned long long now_ts, long long recycle_period)
{
	struct rb_node *n;
	long long diff;
	int shift = 5;
	int bound = 1 << shift;

	xgf_lockprove(__func__);

	diff = (long long)now_ts - (long long)last_check2recycle_dep_ts;

	if (diff < 0LL || diff < (NSEC_PER_SEC << shift))
		goto out;

	n = rb_first(root);

	while (n) {
		struct xgf_deps *iter;

		iter = rb_entry(n, struct xgf_deps, rb_node);

		if (iter->render_dep || iter->render_dep_deep) {
			n = rb_next(n);
			continue;
		}

		diff = iter->render_count - iter->render_pre_count;
		if (diff > 1 || iter->render_count > bound) {
			n = rb_next(n);
			continue;
		}

		rb_erase(n, root);
		kfree(iter);

		n = rb_first(root);
	}
	last_check2recycle_dep_ts = now_ts;

out:
	return;
}

static pid_t most_valuable_timer_owner(struct xgf_proc *proc,
				pid_t current_tid, unsigned long long now_ts)
{
	struct xgf_deps *iter;
	struct rb_node *rbn;
	int temp_render_count = 0;
	pid_t ret = 0;

	xgf_lockprove(__func__);

	/*update count and has timer*/
	proc->dep_timer_count = 0;

	for (rbn = rb_first(&proc->deps_rec); rbn != NULL; rbn = rb_next(rbn)) {
		unsigned long long dur;

		iter = rb_entry(rbn, struct xgf_deps, rb_node);

		if (iter->tid == current_tid && iter->render_dep) {
			iter->has_timer = 1;
			iter->has_timer_renew_ts = now_ts;
		}

		/* clean has_timer for longtime no update */
		dur = now_ts - iter->has_timer_renew_ts;
		if (dur > (NSEC_PER_MSEC << 8)) {
			iter->has_timer = 0;
			iter->has_timer_renew_ts = now_ts;
		}

		/* calculate mvto */
		if (iter->has_timer) {
			proc->dep_timer_count++;

			if (iter->render_count > temp_render_count) {
				temp_render_count = iter->render_count;
				ret = iter->tid;
			}
		}
	}

	return ret;
}

static struct xgf_timer *xgf_get_black_rec(
		const void * const timer,
		struct xgf_proc *proc, int force)
{
	struct rb_node **p = &proc->timer_rec.rb_node;
	struct rb_node *parent = NULL;
	struct xgf_timer *xt = NULL;
	const void *ref;

	xgf_lockprove(__func__);

	while (*p) {
		parent = *p;
		xt = rb_entry(parent, struct xgf_timer, rb_node);

		ref = xt->hrtimer;
		if (timer < ref)
			p = &(*p)->rb_left;
		else if (timer > ref)
			p = &(*p)->rb_right;
		else
			return xt;
	}

	if (!force)
		return NULL;

	xt = kzalloc(sizeof(*xt), GFP_KERNEL);
	if (!xt)
		return NULL;

	xt->hrtimer = timer;

	rb_link_node(&xt->rb_node, parent, p);
	rb_insert_color(&xt->rb_node, &proc->timer_rec);
	return xt;
}

static int is_black_timer(const void * const timer,
			    struct xgf_proc *proc,
			    unsigned long long now_ts)
{
	struct xgf_timer *xt;

	xgf_lockprove(__func__);

	xt = xgf_get_black_rec(timer, proc, 1);
	if (!xt)
		return 1;

	/* a new record */
	if (!xt->fire.ts) {
		xt->fire.ts = now_ts;
		return 0;
	}

	xt->fire.ts = now_ts;

	if (!xt->expire.ts || xt->expire.ts >= now_ts)
		return 1;

	if ((now_ts - xt->expire.ts) < NSEC_PER_MSEC) {
		if (unlikely(xt->blacked == INT_MAX))
			xt->blacked = 100;
		else
			xt->blacked++;
	} else {
		if (unlikely(xt->blacked == INT_MIN))
			xt->blacked = -100;
		else
			xt->blacked--;
	}

	return (xt->blacked > 0) ? 1 : 0;
}

/**
 * is_valid_sleeper
 */
static int is_valid_sleeper(const void * const timer,
				struct xgf_proc *proc,
				unsigned long long now_ts)
{
	struct xgf_deps *xd_current;
	pid_t current_tid;
	pid_t mvt_tid;
	int ret = 0;

	xgf_lockprove(__func__);

	current_tid = task_pid_nr(current);

	xd_current = xgf_get_deps(current_tid, proc, 0, 0);

	if (!xd_current)
		goto out;

	mvt_tid = most_valuable_timer_owner(proc, current_tid, now_ts);

	/* timer for render, keep it */
	if (current_tid == proc->render) {
		ret = 1;
		goto out;
	}

	/* more then 1 timer in dependency list */
	if (proc->dep_timer_count > 1 && xd_current->render_dep) {
		ret = !is_black_timer(timer, proc, now_ts);

		if (current_tid == mvt_tid)
			ret = 1;

		goto out;
	}

	ret = xd_current->render_dep;

out:
	return ret;
}

/**
 * xgf_timer_fire - called when timer invocation
 */
static void xgf_timer_fire(const void * const timer,
			   struct xgf_proc *proc, unsigned long long runtime)
{
	struct xgf_timer *xt;
	unsigned long long now_ts;

	xgf_lockprove(__func__);
	xgf_timer_systrace(timer, 1);

	now_ts = ged_get_time();

	if (!is_valid_sleeper(timer, proc, now_ts))
		return;

	/* for sleep time estimation */
	xgf_trace("valid timer=%p", timer);
	xt = kzalloc(sizeof(*xt), GFP_KERNEL);
	if (!xt)
		return;

	xt->hrtimer = timer;
	xt->expired = 0;
	xgf_update_tick(proc, &xt->fire, now_ts, runtime);

	hlist_add_head(&xt->hlist, &proc->timer_head);
}

static void xgf_update_black_rec(const void * const timer,
		struct xgf_proc *proc, unsigned long long now_ts)
{
	struct xgf_timer *xt;
	struct xgf_deps *xd_current;

	xgf_lockprove(__func__);

	xt = xgf_get_black_rec(timer, proc, 0);
	if (!xt)
		return;

	xd_current = xgf_get_deps(task_pid_nr(current), proc, 0, 0);

	if (!xd_current)
		return;

	if (!xd_current->render_dep)
		return;

	xt->expire.ts = now_ts;
}

/**
 * xgf_timer_expire - called when timer expires
 */
static void xgf_timer_expire(const void * const timer,
			     struct xgf_proc *proc, unsigned long long runtime)
{
	struct xgf_timer *iter;
	unsigned long long now_ts;

	xgf_lockprove(__func__);
	xgf_timer_systrace(timer, 0);

	now_ts = ged_get_time();

	xgf_update_black_rec(timer, proc, now_ts);

	hlist_for_each_entry(iter, &proc->timer_head, hlist) {
		if (timer != iter->hrtimer)
			continue;

		if (iter->expired)
			continue;

		xgf_update_tick(proc, &iter->expire, now_ts, runtime);
		iter->expired = 1;
		return;
	}
}

static void xgf_timer_remove(const void * const timer,
			     struct xgf_proc *proc)
{
	struct xgf_timer *iter;
	struct hlist_node *n;

	xgf_lockprove(__func__);
	xgf_timer_systrace(timer, -1);

	hlist_for_each_entry_safe(iter, n, &proc->timer_head, hlist) {
		if (timer != iter->hrtimer)
			continue;

		if (iter->expired)
			xgf_trace("XXX remove expired timer=%p", timer);

		hlist_del(&iter->hlist);
		kfree(iter);
		return;
	}
	xgf_trace("XXX remove a ghost(?) timer=%p", timer);
}

/**
 * xgf_igather_timer - called for intelligence gathering of timer
 */
void xgf_igather_timer(const void * const timer, int fire)
{
	struct xgf_proc *iter;
	struct hlist_node *n;
	struct task_struct *p;
	unsigned long long runtime = 0;
	pid_t tpid;

	/*get timer's thread group id, i.e. tgid*/
	tpid = task_tgid_nr(current);

	xgf_lock(__func__);

	if (!xgf_is_enable()) {
		xgf_unlock(__func__);
		return;
	}

	hlist_for_each_entry_safe(iter, n, &xgf_procs, hlist) {

		if (iter->parent != tpid)
			continue;

		rcu_read_lock();
		p = find_task_by_vpid(iter->render);
		if (p) {
			get_task_struct(p);
			runtime = task_sched_runtime(p);
			put_task_struct(p);
		}
		rcu_read_unlock();
		if (!p) {
			xgf_reset_render(iter);
			hlist_del(&iter->hlist);
			kfree(iter);
			continue;
		}

		switch (fire) {
		case 1:
			xgf_timer_fire(timer, iter, runtime);
			break;
		case 0:
			xgf_timer_expire(timer, iter, runtime);
			break;
		case -1:
		default:
			xgf_timer_remove(timer, iter);
			break;
		}
	}

	xgf_unlock(__func__);
}

void xgf_epoll_igather_timer(const void * const timer,
				ktime_t *expires, int fire)
{
	if (expires && expires->tv64)
		xgf_igather_timer(timer, fire);
}

/**
 * xgf_reset_render - called while rendering switch
 * reset render timers
 */
void xgf_reset_render(struct xgf_proc *proc)
{
	struct xgf_timer *iter;
	struct xgf_deps *xd_iter;
	struct hlist_node *t;
	struct rb_node *n;

	xgf_lockprove(__func__);

	hlist_for_each_entry_safe(iter, t, &proc->timer_head, hlist) {
		hlist_del(&iter->hlist);
		kfree(iter);
	}
	INIT_HLIST_HEAD(&proc->timer_head);

	while ((n = rb_first(&proc->deps_rec))) {
		rb_erase(n, &proc->deps_rec);

		xd_iter = rb_entry(n, struct xgf_deps, rb_node);
		kfree(xd_iter);
	}

	while ((n = rb_first(&proc->timer_rec))) {
		rb_erase(n, &proc->timer_rec);

		iter = rb_entry(n, struct xgf_timer, rb_node);
		kfree(iter);
	}
}
EXPORT_SYMBOL(xgf_reset_render);

int has_xgf_dep(pid_t tid)
{
	struct xgf_deps *xd;
	struct xgf_proc *proc_iter;
	struct hlist_node *n;
	pid_t query_tid;
	int ret = 0;

	xgf_lock(__func__);

	query_tid = tid;

	hlist_for_each_entry_safe(proc_iter, n, &xgf_procs, hlist) {
		xd = xgf_get_deps(query_tid, proc_iter, 0, 0);

		if (!xd)
			continue;

		ret = xd->render_dep;

		if (ret)
			goto out;
	}

out:
	xgf_unlock(__func__);
	return ret;
}

int fpsgo_fteh2xgf_get_dep_list_num(int pid)
{
	struct xgf_proc *proc_iter;
	struct hlist_node *n;
	struct xgf_deps *iter;
	struct rb_node *rbn;
	int counts = 0;

	if (!pid)
		goto out;

	xgf_lock(__func__);

	hlist_for_each_entry_safe(proc_iter, n, &xgf_procs, hlist) {
		if (proc_iter->render != pid)
			continue;

		for (rbn = rb_first(&proc_iter->deps_rec);
			rbn != NULL; rbn = rb_next(rbn)) {
			iter = rb_entry(rbn, struct xgf_deps, rb_node);
			if (iter->render_dep)
				counts++;
		}
	}

	xgf_unlock(__func__);

out:
	return counts;
}

int fpsgo_fteh2xgf_get_dep_list(int pid, int count, struct fteh_loading *arr)
{
	struct xgf_proc *proc_iter;
	struct hlist_node *n;
	struct xgf_deps *iter;
	struct rb_node *rbn;
	int index = 0;

	if (!pid || !count)
		return 0;

	xgf_lock(__func__);

	hlist_for_each_entry_safe(proc_iter, n, &xgf_procs, hlist) {
		if (proc_iter->render != pid)
			continue;

		for (rbn = rb_first(&proc_iter->deps_rec);
			rbn != NULL; rbn = rb_next(rbn)) {
			iter = rb_entry(rbn, struct xgf_deps, rb_node);
			if (iter->render_dep && index < count) {
				arr[index].pid = iter->tid;
				index++;
			}
		}
	}

	xgf_unlock(__func__);

	return index;
}

/**
 * xgf_kzalloc
 */
void *xgf_kzalloc(size_t size)
{
	return kzalloc(size, GFP_KERNEL);
}
EXPORT_SYMBOL(xgf_kzalloc);

/**
 * xgf_kfree
 */
void xgf_kfree(const void *block)
{
	kfree(block);
}
EXPORT_SYMBOL(xgf_kfree);

/**
 * xgf_reset_procs
 */
static void xgf_reset_procs(void)
{
	struct xgf_proc *iter;
	struct hlist_node *tmp;

	xgf_lockprove(__func__);

	hlist_for_each_entry_safe(iter, tmp, &xgf_procs, hlist) {
		xgf_reset_render(iter);

		hlist_del(&iter->hlist);
		kfree(iter);
	}
}

/**
 * xgf_get_proc - get render thread's structure
 */
static int xgf_get_proc(pid_t rpid, struct xgf_proc **ret, int force)
{
	struct xgf_proc *iter;

	xgf_lockprove(__func__);

	hlist_for_each_entry(iter, &xgf_procs, hlist) {
		if (iter->render != rpid)
			continue;

		if (ret)
			*ret = iter;
		return 0;
	}

	/* ensure dequeue is observed first */
	if (!force)
		return -EINVAL;

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);
	if (iter == NULL)
		return -ENOMEM;

	{
		struct task_struct *tsk;

		rcu_read_lock();
		tsk = find_task_by_vpid(rpid);
		if (tsk)
			get_task_struct(tsk);
		rcu_read_unlock();

		if (!tsk) {
			kfree(iter);
			return -EINVAL;
		}

		iter->parent = tsk->tgid;
		iter->render = rpid;
		put_task_struct(tsk);
	}

	INIT_HLIST_HEAD(&iter->timer_head);
	hlist_add_head(&iter->hlist, &xgf_procs);

	if (ret)
		*ret = iter;
	return 0;
}

static unsigned long long xgf_qudeq_enter(int rpid,
					  struct xgf_proc *proc,
					  struct xgf_tick *ref,
					  struct xgf_tick *now)
{
	int ret;
	unsigned long long slptime;

	xgf_lockprove(__func__);

	xgf_update_tick(proc, now, 0, 0);

	/* first frame of each process */
	if (!ref->ts) {
		proc->render = rpid;
		fpsgo_systrace_c_xgf(proc->parent, rpid, "render thrd");
		return 0ULL;
	}

	trace_xgf_intvl("enter", NULL, &ref->ts, &now->ts);

	WARN_ON(!xgf_est_slptime_fp);
	if (xgf_est_slptime_fp)
		ret = xgf_est_slptime_fp(proc, &slptime, ref, now, rpid);
	else
		ret = -ENOENT;

	if (ret)
		return 0ULL;

	return slptime;
}

static void xgf_qudeq_exit(struct xgf_proc *proc, struct xgf_tick *ts,
			   unsigned long long *time)
{
	unsigned long long start;

	xgf_lockprove(__func__);

	start = ts->ts;
	xgf_update_tick(proc, ts, 0, 0);
	if (ts->ts <= start)
		*time = 0ULL;
	else
		*time = ts->ts - start;
}

static int xgf_dep_cal_sched_slptime(unsigned long long *slptime,
	struct xgf_tick *ref, struct xgf_tick *now)
{
	int ret = 0;
	long long sched_slptime;

	xgf_lockprove(__func__);

	if ((now->ts < ref->ts) || (now->runtime < ref->runtime)) {
		ret = -EINVAL;
		goto out;
	}

	sched_slptime = (now->ts - ref->ts) - (now->runtime - ref->runtime);
	if (sched_slptime < 0)
		sched_slptime = 0;

	*slptime = (unsigned long long)sched_slptime;
out:
	return ret;
}

static unsigned long long xgf_dep_qudeq_enter(struct xgf_proc *proc,
	struct xgf_deps *deps, struct xgf_tick *ref,
	struct xgf_tick *now, unsigned long long now_ts)
{
	int ret;
	unsigned long long slptime;

	xgf_lockprove(__func__);

	xgf_update_dep_tick(proc, deps, now, now_ts);

	/* first frame of each deps */
	if (!ref->ts)
		return 0ULL;

	ret = xgf_dep_cal_sched_slptime(&slptime, ref, now);

	if (ret)
		return 0ULL;

	return slptime;
}

static void xgf_dep_qudeq_exit(struct xgf_proc *proc,
	struct xgf_deps *deps, struct xgf_tick *ts,
	unsigned long long *time, unsigned long long now_ts)
{
	unsigned long long start;

	xgf_lockprove(__func__);

	start = ts->ts;
	xgf_update_dep_tick(proc, deps, ts, now_ts);
	if (ts->ts <= start)
		*time = 0ULL;
	else
		*time = ts->ts - start;
}


void fpsgo_ctrl2xgf_switch_xgf(int val)
{
	xgf_lock(__func__);
	if (val != xgf_enable) {
		xgf_enable = val;

		xgf_reset_procs();
	}

	xgf_unlock(__func__);
}

void fpsgo_fstb2xgf_do_recycle(int fstb_active)
{
	unsigned long long now_ts = ged_get_time();
	long long diff, check_period, recycle_period;
	struct xgf_proc *proc_iter;
	struct hlist_node *proc_t;

	/* over 1 seconds since last check2recycle */
	check_period = NSEC_PER_SEC;
	recycle_period = NSEC_PER_SEC >> 1;
	diff = (long long)now_ts - (long long)last_check2recycle_ts;

	xgf_trace(
		"%s at now=%llu, last check=%llu, fstb_active=%d",
		__func__, now_ts, last_check2recycle_ts, fstb_active);

	xgf_lock(__func__);

	if (!fstb_active) {
		xgf_reset_procs();
		goto out;
	}

	if (diff < 0LL || diff < check_period)
		goto done;

	hlist_for_each_entry_safe(proc_iter, proc_t, &xgf_procs, hlist) {
		diff = (long long)now_ts - (long long)proc_iter->deque.ts;

		/* has not been over check_period since last deque,
		 * do black recycle only
		 */
		if (diff < check_period) {
			xgf_timer_recycle(proc_iter, now_ts, recycle_period);
			xgf_blacked_recycle(&proc_iter->timer_rec,
					now_ts, recycle_period);
			xgf_dep_recycle(&proc_iter->deps_rec,
					now_ts, recycle_period);
			continue;
		}

		xgf_reset_render(proc_iter);
		hlist_del(&proc_iter->hlist);
		kfree(proc_iter);
	}

out:
	last_check2recycle_ts = now_ts;
done:
	xgf_unlock(__func__);
}

static unsigned long long xgf_dep_sched_slptime(int rpid,
	struct xgf_proc *proc, int cmd)
{
	struct rb_node *n;
	struct xgf_deps *deps;
	struct rb_root *r;
	unsigned long long now_ts = ged_get_time();
	unsigned long long dur = 0ULL;
	unsigned long long dep_max_slptime = 0ULL;

	xgf_lockprove(__func__);

	if (!proc->render)
		return dep_max_slptime;

	r = &proc->deps_rec;
	for (n = rb_first(r); n != NULL; n = rb_next(n)) {
		deps = rb_entry(n, struct xgf_deps, rb_node);

		if (!deps->render_dep)
			continue;

		switch (cmd) {
		case XGF_QUEUE_START:
			dur =
				xgf_dep_qudeq_enter(proc, deps, &deps->deque,
						&deps->queue, now_ts);
			break;

		case XGF_QUEUE_END:
			xgf_dep_qudeq_exit(proc, deps, &deps->queue,
					&deps->quetime, now_ts);
			break;

		case XGF_DEQUEUE_START:
			dur = xgf_dep_qudeq_enter(proc, deps, &deps->queue,
					&deps->deque, now_ts);
			break;

		case XGF_DEQUEUE_END:
			xgf_dep_qudeq_exit(proc, deps, &deps->deque,
					&deps->deqtime, now_ts);
			break;

		default:
			/* do nothing */
			break;
		}

		if (dur) {
			if (!dep_max_slptime)
				dep_max_slptime = dur;

			if (dur < dep_max_slptime)
				dep_max_slptime = dur;
		}
	}

	return dep_max_slptime;
}

int fpsgo_comp2xgf_qudeq_notify(int rpid,
		int cmd, unsigned long long *sleep_time)
{
	int ret = XGF_NOTIFY_OK;
	struct xgf_proc *p, **pproc;
	struct xgf_proc *iter;
	int proc_cnt = 0;
	int timer_cnt = 0;
	unsigned long long dur;
	unsigned long long dep_dur;
	struct xgf_timer *timer_iter;

	xgf_lock(__func__);
	if (!xgf_is_enable()) {
		xgf_unlock(__func__);
		return XGF_DISABLE;
	}

	switch (cmd) {

	case XGF_QUEUE_START:
		pproc = &p;
		if (xgf_get_proc(rpid, pproc, 0)) {
			ret = XGF_THREAD_NOT_FOUND;
			goto qudeq_notify_err;
		}

		hlist_for_each_entry(timer_iter, &p->timer_head, hlist)
			timer_cnt++;
		fpsgo_systrace_c_xgf(rpid, timer_cnt, "timer_cnt");

		dur = xgf_qudeq_enter(rpid, p, &p->deque, &p->queue);
		dep_dur = xgf_dep_sched_slptime(rpid, p, cmd);
		xgf_trace(
			"xgf queue start dur=%llu, dep_dur=%llu",
			dur, dep_dur);
		if (dur > dep_dur)
			dur = dep_dur;
		fpsgo_systrace_c_xgf(rpid, dur,
				"renew sleep time");
		p->slptime += dur;
		fpsgo_systrace_c_xgf(rpid, p->slptime,
				"frame sleep time");
		*sleep_time = p->slptime;

		hlist_for_each_entry(iter, &xgf_procs, hlist)
			proc_cnt++;
		fpsgo_systrace_c_xgf(rpid, proc_cnt, "proc_cnt");

		ret = XGF_SLPTIME_OK;
		break;

	case XGF_QUEUE_END:
		pproc = &p;
		if (xgf_get_proc(rpid, pproc, 1)) {
			ret = XGF_THREAD_NOT_FOUND;
			goto qudeq_notify_err;
		}

		xgf_qudeq_exit(p, &p->queue, &p->quetime);
		dep_dur = xgf_dep_sched_slptime(rpid, p, cmd);
		/* reset for safety */
		p->slptime = 0;
		break;

	case XGF_DEQUEUE_START:
		pproc = &p;
		if (xgf_get_proc(rpid, pproc, 0)) {
			ret = XGF_THREAD_NOT_FOUND;
			goto qudeq_notify_err;
		}

		fpsgo_init_render_dep(rpid);
		dur = xgf_qudeq_enter(rpid, p, &p->queue, &p->deque);
		dep_dur = xgf_dep_sched_slptime(rpid, p, cmd);
		xgf_trace(
			"xgf dequeue start dur=%llu, dep_dur=%llu",
			dur, dep_dur);
		if (dur > dep_dur)
			dur = dep_dur;
		fpsgo_systrace_c_xgf(rpid, dur,
				"renew sleep time");
		p->slptime += dur;
		break;

	case XGF_DEQUEUE_END:
		pproc = &p;
		if (xgf_get_proc(rpid, pproc, 0)) {
			ret = XGF_THREAD_NOT_FOUND;
			goto qudeq_notify_err;
		}

		xgf_qudeq_exit(p, &p->deque, &p->deqtime);
		dep_dur = xgf_dep_sched_slptime(rpid, p, cmd);
		break;

	default:
		ret = XGF_PARAM_ERR;
		break;
	}

qudeq_notify_err:
	xgf_unlock(__func__);
	return ret;
}

#define FPSGO_DEBUGFS_ENTRY(name) \
static int fpsgo_##name##_open(struct inode *i, struct file *file) \
{ \
	return single_open(file, fpsgo_##name##_show, i->i_private); \
} \
\
static const struct file_operations fpsgo_##name##_fops = { \
	.owner = THIS_MODULE, \
	.open = fpsgo_##name##_open, \
	.read = seq_read, \
	.write = fpsgo_##name##_write, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

static int fpsgo_black_show(struct seq_file *m, void *unused)
{
	struct rb_node *n;
	struct xgf_proc *proc;
	struct xgf_deps *iter;
	struct xgf_timer *black_iter;
	struct rb_root *r;
	int diff;

	xgf_lock(__func__);
	hlist_for_each_entry(proc, &xgf_procs, hlist) {
		seq_printf(m, " proc:%d:%d %llu\n", proc->parent,
			   proc->render, ged_get_time());

		r = &proc->deps_rec;
		for (n = rb_first(r); n != NULL; n = rb_next(n)) {
			iter = rb_entry(n, struct xgf_deps, rb_node);
			diff = iter->render_count-iter->render_pre_count;
			seq_printf(m, "pid:%d dep:%d tc:%d render_c:%d diff:%d\n",
				iter->tid, iter->render_dep,
				proc->render_thread_called_count,
				iter->render_count, diff);
		}

		r = &proc->timer_rec;
		for (n = rb_first(r); n != NULL; n = rb_next(n)) {
			black_iter = rb_entry(n, struct xgf_timer, rb_node);
			seq_printf(m, "timer:%p fire:%llu expire:%llu black:%d\n",
				   black_iter->hrtimer, black_iter->fire.ts,
				   black_iter->expire.ts, black_iter->blacked);
		}
	}
	xgf_unlock(__func__);
	return 0;
}

static ssize_t fpsgo_black_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	return cnt;
}

FPSGO_DEBUGFS_ENTRY(black);

int __init init_xgf(void)
{
	if (!fpsgo_debugfs_dir)
		return -ENODEV;

	debugfs_xgf_dir = debugfs_create_dir("xgf",
					     fpsgo_debugfs_dir);
	if (!debugfs_xgf_dir)
		return -ENODEV;

	debugfs_create_file("black",
			    0664,
			    debugfs_xgf_dir,
			    NULL,
			    &fpsgo_black_fops);

	return 0;
}
