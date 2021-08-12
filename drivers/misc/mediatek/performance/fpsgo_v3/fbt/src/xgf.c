// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/preempt.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/trace_events.h>
#include <linux/module.h>
#include <linux/sched/clock.h>
#include <linux/cpumask.h>

#include <mt-plat/fpsgo_common.h>

#include <trace/events/fpsgo.h>
#include <trace/events/sched.h>
#include <trace/events/ipi.h>
#include <trace/events/irq.h>
#include <trace/events/timer.h>

#include "xgf.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"

static DEFINE_MUTEX(xgf_main_lock);
static int xgf_enable;
static int xgf_ko_ready;
static struct kobject *xgf_kobj;
static unsigned long long last_check2recycle_ts;
static atomic_t xgf_atomic_val_0 = ATOMIC_INIT(0);
static atomic_t xgf_atomic_val_1 = ATOMIC_INIT(0);
static unsigned long long last_update2spid_ts;
static char *xgf_sp_name = SP_ALLOW_NAME;
static int xgf_extra_sub;
static int cur_xgf_extra_sub;
static int xgf_dep_frames = XGF_DEP_FRAMES_MIN;
static int xgf_prev_dep_frames = XGF_DEP_FRAMES_MIN;
static int xgf_spid_sub = XGF_DO_SP_SUB;
static int xgf_ema_dividend = EMA_DIVIDEND;
static int xgf_spid_ck_period = NSEC_PER_SEC;
static int xgf_sp_name_id;
static int display_rate;
int fstb_frame_num;
EXPORT_SYMBOL(fstb_frame_num);
int fstb_no_stable_thr;
EXPORT_SYMBOL(fstb_no_stable_thr);
int fstb_target_fps_margin;
EXPORT_SYMBOL(fstb_target_fps_margin);
int fstb_separate_runtime_enable;
EXPORT_SYMBOL(fstb_separate_runtime_enable);
module_param(xgf_sp_name, charp, 0644);
module_param(xgf_extra_sub, int, 0644);
module_param(xgf_dep_frames, int, 0644);
module_param(xgf_spid_sub, int, 0644);
module_param(xgf_ema_dividend, int, 0644);
module_param(xgf_spid_ck_period, int, 0644);
module_param(xgf_sp_name_id, int, 0644);

HLIST_HEAD(xgf_renders);
HLIST_HEAD(xgf_hw_events);

int (*xgf_est_runtime_fp)(
	pid_t r_pid,
	struct xgf_render *render,
	unsigned long long *runtime,
	unsigned long long ts
	);
EXPORT_SYMBOL(xgf_est_runtime_fp);

int (*fpsgo_xgf2ko_calculate_target_fps_fp)(
	int pid,
	unsigned long long bufID,
	int *target_fps_margin,
	unsigned long long cur_queue_end_ts
	);
EXPORT_SYMBOL(fpsgo_xgf2ko_calculate_target_fps_fp);

void (*fpsgo_xgf2ko_do_recycle_fp)(
	int pid,
	unsigned long long bufID
	);
EXPORT_SYMBOL(fpsgo_xgf2ko_do_recycle_fp);

static int (*xgf_stat_xchg_fp)(int enable);

int (*xgff_est_runtime_fp)(
	pid_t r_pid,
	struct xgf_render *render,
	unsigned long long *runtime,
	unsigned long long ts
	);
EXPORT_SYMBOL(xgff_est_runtime_fp);
int (*xgff_update_start_prev_index_fp)(struct xgf_render *render);
EXPORT_SYMBOL(xgff_update_start_prev_index_fp);

long long (*xgf_ema2_predict_fp)(struct xgf_ema2_predictor *pt, long long X);
EXPORT_SYMBOL(xgf_ema2_predict_fp);

void xgff_clean_deps_list(struct xgf_render *render, int pos)
{
}
EXPORT_SYMBOL(xgff_clean_deps_list);

int xgff_dep_frames_mod(struct xgf_render *render, int pos)
{
	return 0;
}
EXPORT_SYMBOL(xgff_dep_frames_mod);

struct xgf_dep *xgff_get_dep(
	pid_t tid, struct xgf_render *render, int pos, int force)
{
	return NULL;
}
EXPORT_SYMBOL(xgff_get_dep);

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

static int xgf_tracepoint_probe_register(struct tracepoint *tp,
					void *probe,
					void *data)
{
	return tracepoint_probe_register(tp, probe, data);
}

static int xgf_tracepoint_probe_unregister(struct tracepoint *tp,
					void *probe,
					void *data)
{
	return tracepoint_probe_unregister(tp, probe, data);
}

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

void *xgf_alloc(int i32Size)
{
	void *pvBuf;

	if (i32Size <= PAGE_SIZE)
		pvBuf = kzalloc(i32Size, GFP_ATOMIC);
	else
		pvBuf = vzalloc(i32Size);

	return pvBuf;
}
EXPORT_SYMBOL(xgf_alloc);

void xgf_free(void *pvBuf)
{
	kvfree(pvBuf);
}
EXPORT_SYMBOL(xgf_free);

void *xgf_atomic_val_assign(int select)
{
	if (!select)
		return (void *)(&xgf_atomic_val_0);
	else
		return (void *)(&xgf_atomic_val_1);
}
EXPORT_SYMBOL(xgf_atomic_val_assign);

int *xgf_extra_sub_assign(void)
{
	return (int *)(&cur_xgf_extra_sub);
}
EXPORT_SYMBOL(xgf_extra_sub_assign);

int *xgf_spid_sub_assign(void)
{
	return (int *)(&xgf_spid_sub);
}
EXPORT_SYMBOL(xgf_spid_sub_assign);

int xgf_atomic_read(atomic_t *val)
{
	return atomic_read(val);
}
EXPORT_SYMBOL(xgf_atomic_read);

int xgf_atomic_inc_return(atomic_t *val)
{
	return atomic_inc_return(val);
}
EXPORT_SYMBOL(xgf_atomic_inc_return);

void xgf_atomic_set(atomic_t *val, int i)
{
	atomic_set(val, i);
}
EXPORT_SYMBOL(xgf_atomic_set);

unsigned int xgf_cpumask_next(int cpu,  const struct cpumask *srcp)
{
	return cpumask_next(cpu, srcp);
}
EXPORT_SYMBOL(xgf_cpumask_next);

int xgf_num_possible_cpus(void)
{
	return num_possible_cpus();
}
EXPORT_SYMBOL(xgf_num_possible_cpus);

int xgf_get_task_wake_cpu(struct task_struct *t)
{
	return t->wake_cpu;
}
EXPORT_SYMBOL(xgf_get_task_wake_cpu);

int xgf_get_task_pid(struct task_struct *t)
{
	return t->pid;
}
EXPORT_SYMBOL(xgf_get_task_pid);

long xgf_get_task_state(struct task_struct *t)
{
	return t->state;
}
EXPORT_SYMBOL(xgf_get_task_state);

static inline int xgf_ko_is_ready(void)
{
	xgf_lockprove(__func__);

	return xgf_ko_ready;
}

int xgf_get_display_rate(void)
{
	return display_rate;
}
EXPORT_SYMBOL(xgf_get_display_rate);

int xgf_get_process_id(int pid)
{
	int process_id = -1;
	struct task_struct *tsk;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk) {
		get_task_struct(tsk);
		process_id = tsk->tgid;
		put_task_struct(tsk);
	}
	rcu_read_unlock();

	return process_id;
}
EXPORT_SYMBOL(xgf_get_process_id);

int xgf_check_main_sf_pid(int pid, int process_id)
{
	int ret = 0;
	int hrtimer_process_id;
	char hrtimer_process_name[16];
	struct task_struct *gtsk;

	hrtimer_process_id = xgf_get_process_id(pid);
	if (hrtimer_process_id < 0)
		return ret;

	rcu_read_lock();
	gtsk = find_task_by_vpid(hrtimer_process_id);
	if (gtsk) {
		get_task_struct(gtsk);
		strncpy(hrtimer_process_name, gtsk->comm, 16);
		hrtimer_process_name[15] = '\0';
		put_task_struct(gtsk);
	} else
		hrtimer_process_name[0] = '\0';
	rcu_read_unlock();

	if ((hrtimer_process_id == process_id) ||
		strstr(hrtimer_process_name, "surfaceflinger"))
		ret = 1;

	return ret;
}
EXPORT_SYMBOL(xgf_check_main_sf_pid);

int xgf_check_specific_pid(int pid)
{
	int ret = 0;
	char thread_name[16];
	struct task_struct *tsk;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk) {
		get_task_struct(tsk);
		strncpy(thread_name, tsk->comm, 16);
		thread_name[15] = '\0';
		put_task_struct(tsk);
	} else
		thread_name[0] = '\0';
	rcu_read_unlock();

	if (strstr(thread_name, "mali-") || strstr(thread_name, "Binder:"))
		ret = 1;
	else if (strstr(thread_name, "UnityMain"))
		ret = 2;

	return ret;
}
EXPORT_SYMBOL(xgf_check_specific_pid);

int fpsgo_ko2xgf_get_fps_level(int pid, unsigned long long bufID, int target_fps)
{
	return -EINVAL;
}
EXPORT_SYMBOL(fpsgo_ko2xgf_get_fps_level);

static inline int xgf_is_enable(void)
{
	xgf_lockprove(__func__);

	return xgf_enable;
}

unsigned long long xgf_get_time(void)
{
	unsigned long long temp;

	preempt_disable();
	temp = cpu_clock(smp_processor_id());
	preempt_enable();
	return temp;
}
EXPORT_SYMBOL(xgf_get_time);

static void xgf_reset_render_sector(struct xgf_render *render)
{
	struct hlist_node *s, *p;
	struct xgf_render_sector *sect_iter;
	struct xgf_pid_rec *pids_iter;

	xgf_lockprove(__func__);

	hlist_for_each_entry_safe(sect_iter, s, &render->sector_head, hlist) {
		hlist_for_each_entry_safe(pids_iter, p,
				&sect_iter->path_head, hlist) {
			hlist_del(&pids_iter->hlist);
			xgf_free(pids_iter);
		}
		INIT_HLIST_HEAD(&sect_iter->path_head);

		hlist_del(&sect_iter->hlist);
		xgf_free(sect_iter);
	}
	INIT_HLIST_HEAD(&render->sector_head);
	render->sector_nr = 0;
}

static void xgf_reset_render_hw_list(struct xgf_render *render)
{
	struct hlist_node *h;
	struct xgf_hw_rec *hw_iter;

	xgf_lockprove(__func__);

	hlist_for_each_entry_safe(hw_iter, h, &render->hw_head, hlist) {
		hlist_del(&hw_iter->hlist);
		xgf_free(hw_iter);
	}
	INIT_HLIST_HEAD(&render->hw_head);
}

static void xgf_clean_hw_events(void)
{
	struct xgf_hw_event *iter;
	struct hlist_node *t;

	xgf_lockprove(__func__);

	hlist_for_each_entry_safe(iter, t, &xgf_hw_events, hlist) {
		hlist_del(&iter->hlist);
		xgf_free(iter);
	}
}

void xgf_clean_deps_list(struct xgf_render *render, int pos)
{
	struct xgf_dep *iter;

	xgf_lockprove(__func__);

	if (pos == INNER_DEPS) {
		while (!RB_EMPTY_ROOT(&render->deps_list)) {
			iter = rb_entry(render->deps_list.rb_node,
						struct xgf_dep, rb_node);
			rb_erase(&iter->rb_node, &render->deps_list);
			xgf_free(iter);
		}
	}

	if (pos == OUTER_DEPS) {
		while (!RB_EMPTY_ROOT(&render->out_deps_list)) {
			iter = rb_entry(render->out_deps_list.rb_node,
						struct xgf_dep, rb_node);
			rb_erase(&iter->rb_node, &render->out_deps_list);
			xgf_free(iter);
		}
	}

	if (pos == PREVI_DEPS) {
		while (!RB_EMPTY_ROOT(&render->prev_deps_list)) {
			iter = rb_entry(render->prev_deps_list.rb_node,
						struct xgf_dep, rb_node);
			rb_erase(&iter->rb_node, &render->prev_deps_list);
			xgf_free(iter);
		}
	}
}
EXPORT_SYMBOL(xgf_clean_deps_list);

int xgf_dep_frames_mod(struct xgf_render *render, int pos)
{
	int ret = 0;
	int pre_dep_frames;

	xgf_lockprove(__func__);

	pre_dep_frames = render->dep_frames - 1;

	if (pos == PREVI_DEPS) {
		if (render->frame_count == 0)
			ret = INT_MAX%pre_dep_frames;
		else
			ret = (render->frame_count - 1)%pre_dep_frames;
	} else
		ret = render->frame_count%pre_dep_frames;

	return ret;
}
EXPORT_SYMBOL(xgf_dep_frames_mod);

struct xgf_dep *xgf_get_dep(
	pid_t tid, struct xgf_render *render, int pos, int force)
{
	struct rb_root *r = NULL;
	struct rb_node **p = NULL;
	struct rb_node *parent = NULL;
	struct xgf_dep *xd = NULL;
	pid_t tp;

	xgf_lockprove(__func__);

	switch (pos) {
	case INNER_DEPS:
		p = &render->deps_list.rb_node;
		r = &render->deps_list;
		break;

	case OUTER_DEPS:
		p = &render->out_deps_list.rb_node;
		r = &render->out_deps_list;
		break;

	case PREVI_DEPS:
		p = &render->prev_deps_list.rb_node;
		r = &render->prev_deps_list;
		break;

	default:
		return NULL;
	}

	while (*p) {
		parent = *p;
		xd = rb_entry(parent, struct xgf_dep, rb_node);

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

	xd = xgf_alloc(sizeof(*xd));

	if (!xd)
		return NULL;

	xd->tid = tid;
	if (xgf_spid_sub == 1 && tid == render->spid)
		xd->render_dep = 0;
	else
		xd->render_dep = 1;
	xd->frame_idx = xgf_dep_frames_mod(render, pos);

	rb_link_node(&xd->rb_node, parent, p);
	rb_insert_color(&xd->rb_node, r);

	return xd;
}
EXPORT_SYMBOL(xgf_get_dep);

static struct xgf_hw_rec *xgf_get_hw_rec(int event_type,
	unsigned long long mid, struct xgf_render *render, int force)
{
	struct xgf_hw_rec *iter;
	struct xgf_hw_rec *xhr = NULL;

	xgf_lockprove(__func__);

	hlist_for_each_entry(iter, &render->hw_head, hlist) {
		if (iter->event_type != event_type)
			continue;

		if (iter->mid != mid)
			continue;

		if (iter)
			xhr = iter;

		return xhr;
	}

	if (!force)
		return NULL;

	xhr = xgf_alloc(sizeof(*xhr));

	if (!xhr)
		return NULL;

	xhr->event_type = event_type;
	xhr->hw_type = PER_FRAME;
	xhr->mid = mid;

	hlist_add_head(&xhr->hlist, &render->hw_head);

	return xhr;
}

static void xgf_hw_event_update(unsigned long long max_ts,
	unsigned long long min_ts, int rpid, struct xgf_render *render)
{
	struct xgf_hw_event *iter;
	struct hlist_node *t;
	struct xgf_dep *xd;
	struct xgf_hw_rec *xhr;

	xgf_lockprove(__func__);

	hlist_for_each_entry_safe(iter, t, &xgf_hw_events, hlist) {
		if (!iter->tail_ts || iter->tail_ts > max_ts
				|| iter->tail_ts < min_ts)
			continue;

		xd = xgf_get_dep(iter->tid, render, INNER_DEPS, 0);
		if (!xd)
			continue;

		iter->render = rpid;
		iter->valid = 1;

		iter->event_index = render->frame_count;

		xhr = xgf_get_hw_rec(iter->event_type, iter->mid, render, 1);
	}
}

static int xgf_valid_hw_index(int check_index, int current_index)
{
	int ret = 0;
	int lower_bound = current_index - HW_MONITER_WINDOW + 1;
	int invert_bound;

	if (check_index < 0 || current_index < 0)
		return ret;

	if (unlikely(lower_bound < 0)) {
		if (check_index >= 0 && check_index <= current_index)
			ret = 1;

		invert_bound = INT_MAX + lower_bound;
		if (check_index > invert_bound)
			ret = 1;
	} else {
		if (check_index <= current_index && check_index >= lower_bound)
			ret = 1;
	}

	return ret;
}

static int xgf_is_valid_hw_event(int rpid, int event_type,
				int index, struct xgf_hw_event *event)
{
	if (event->event_index == -1 || event->event_type != event_type)
		return 0;

	if (event->render != rpid || !event->valid)
		return 0;

	if (!xgf_valid_hw_index(event->event_index, index))
		return 0;

	return 1;
}

static int xgf_hw_index_distance(int input_index, int current_index)
{
	int ret = 0;
	int lower_bound = current_index - HW_MONITER_WINDOW + 1;
	int invert_bound;

	if (input_index < 0 || current_index < 0)
		return ret;

	if (unlikely(lower_bound < 0)) {
		invert_bound = INT_MAX + lower_bound;

		if (input_index <= current_index)
			ret = current_index - input_index + 1;
		else if (input_index >= invert_bound)
			ret = (INT_MAX - input_index + 1) + (current_index + 1);
	} else
		ret = current_index - input_index + 1;

	return ret;
}

static int xgf_hw_type(int rpid, struct xgf_render *render)
{
	int event, curr_index, stored_count, valid_count, t_count;
	int level;
	struct xgf_hw_rec *hr_iter;
	struct hlist_node *hr;
	struct xgf_hw_event *he_iter;
	struct hlist_node *he;

	xgf_lockprove(__func__);

	for (event = 0; event < HW_EVENT_NUM; event++) {
		curr_index = render->frame_count;

		hlist_for_each_entry_safe(hr_iter, hr,
						&render->hw_head, hlist) {

			if (hr_iter->event_type != event)
				continue;

			valid_count = 0;
			stored_count = 0;
			level = 0;

			hlist_for_each_entry_safe(he_iter, he,
							&xgf_hw_events, hlist) {

				if (hr_iter->mid != he_iter->mid)
					continue;

				if (!xgf_is_valid_hw_event(rpid,
						event, curr_index, he_iter))
					continue;

				t_count = xgf_hw_index_distance(
					he_iter->event_index, curr_index);

				if (t_count > stored_count)
					stored_count = t_count;

				valid_count++;
			}
			xgf_trace("xgf hw type valid_count:%d stored_count:%d",
						valid_count, stored_count);

			if (valid_count > 0 && stored_count > 0) {
				level = (valid_count*10)/stored_count;
				if (level >= HW_MONITER_LEVEL)
					hr_iter->hw_type = PER_FRAME;
				else
					hr_iter->hw_type = CRO_FRAME;
			} else {
				if (stored_count > 0)
					hr_iter->hw_type = BACKGROUND;
				else {
					hlist_del(&hr_iter->hlist);
					xgf_free(hr_iter);
				}
			}
		}
	}

	return 1;
}

int xgf_hw_events_update(int rpid, struct xgf_render *render)
{
	int ret = 0;
	unsigned long long min_ts, max_ts;
	struct xgf_hw_event *iter;
	struct hlist_node *t, *p, *s;
	struct xgf_pid_rec *pids_iter;
	struct xgf_render_sector *xrs;
	struct xgf_hw_rec *xhr;

	xgf_lockprove(__func__);

	max_ts = render->curr_ts;
	min_ts = render->prev_ts;

	xgf_hw_event_update(max_ts, min_ts, rpid, render);

	ret = xgf_hw_type(rpid, render);

	/* ignore run time calculate when cross frame */
	hlist_for_each_entry_safe(iter, t, &xgf_hw_events, hlist) {
		if (!iter->tail_ts || !iter->valid
			|| iter->tail_ts > max_ts || iter->tail_ts < min_ts)
			continue;

		hlist_for_each_entry_safe(xrs, s, &render->sector_head, hlist) {
			if (iter->tail_ts > xrs->tail_ts
				|| iter->tail_ts < xrs->head_ts)
				continue;

			hlist_for_each_entry_safe(pids_iter, p,
				&xrs->path_head, hlist) {
				if (pids_iter->pid == iter->tid) {
					xhr = xgf_get_hw_rec(iter->event_type,
					iter->mid, render, 0);
					if (!xhr)
						continue;

					if (xhr->hw_type == CRO_FRAME)
						xrs->counted = 0;
				}
			}
		}
	}

	return ret;
}
EXPORT_SYMBOL(xgf_hw_events_update);

static void xgf_hw_event_begin(int event_type, int tid,
				unsigned long long mid)
{
	struct xgf_hw_event *event;
	unsigned long long now_ts = xgf_get_time();

	xgf_lockprove(__func__);

	event = xgf_alloc(sizeof(*event));

	if (event == NULL)
		return;

	event->event_index = -1;
	event->render = 0;
	event->valid = 0;
	event->event_type = event_type;
	event->head_ts = now_ts;
	event->tid = tid;
	event->mid = mid;
	event->tail_ts = 0;

	hlist_add_head(&event->hlist, &xgf_hw_events);
}

static int xgf_hw_event_end(int event_type, int tid,
				unsigned long long mid)
{
	struct xgf_hw_event *event;
	struct xgf_hw_rec *xhr;
	struct xgf_render *r_iter;
	struct hlist_node *tmp, *r_tmp;
	unsigned long long now_ts = xgf_get_time();
	int ret = PER_FRAME;

	xgf_lockprove(__func__);

	hlist_for_each_entry_safe(event, tmp, &xgf_hw_events, hlist) {
		if (event->tid != tid || event->tail_ts)
			continue;

		event->tail_ts = now_ts;
	}

	hlist_for_each_entry_safe(r_iter, r_tmp, &xgf_renders, hlist) {
		xhr = xgf_get_hw_rec(event_type, mid, r_iter, 0);

		if (xhr) {
			ret = xhr->hw_type;
			break;
		}
	}

	return ret;
}

static int xgf_hw_event_collect(int event_type, int tid,
				unsigned long long mid, int cmd)
{
	int ret = PER_FRAME;

	xgf_lock(__func__);

	if (!xgf_is_enable()) {
		xgf_unlock(__func__);
		return ret;
	}

	switch (cmd) {
	case 1:
		xgf_hw_event_begin(event_type, tid, mid);
		break;

	case 0:
	default:
		ret = xgf_hw_event_end(event_type, tid, mid);
		break;
	}

	xgf_unlock(__func__);

	return ret;
}

static int xgf_get_render(pid_t rpid, unsigned long long bufID,
	struct xgf_render **ret, int force)
{
	struct xgf_render *iter;

	xgf_lockprove(__func__);

	hlist_for_each_entry(iter, &xgf_renders, hlist) {
		if (iter->render != rpid)
			continue;

		if (iter->bufID != bufID)
			continue;

		if (ret)
			*ret = iter;
		return 0;
	}

	if (!force)
		return -EINVAL;

	iter = xgf_alloc(sizeof(*iter));

	if (!iter)
		return -ENOMEM;

	{
		struct task_struct *tsk;

		rcu_read_lock();
		tsk = find_task_by_vpid(rpid);
		if (tsk)
			get_task_struct(tsk);
		rcu_read_unlock();

		if (!tsk) {
			xgf_free(iter);
			return -EINVAL;
		}

		iter->parent = tsk->tgid;
		iter->render = rpid;
		put_task_struct(tsk);

		iter->bufID = bufID;
		iter->sector_nr = 0;
		iter->curr_index = 0;
		iter->curr_ts = 0;
		iter->prev_index = 0;
		iter->prev_ts = 0;
		iter->event_count = 0;
		iter->frame_count = 0;
		iter->u_wake_r = 0;
		iter->u_wake_r_count = 0;
		iter->queue.start_ts = 0;
		iter->queue.end_ts = 0;
		iter->deque.start_ts = 0;
		iter->deque.end_ts = 0;
		iter->ema_runtime = 0;
		iter->pre_u_runtime = 0;
		iter->u_avg_runtime = 0;
		iter->u_runtime_sd = 0;
		iter->u_runtime_idx = 0;
		iter->spid = 0;
		iter->dep_frames = xgf_prev_dep_frames;
	}

	INIT_HLIST_HEAD(&iter->sector_head);
	INIT_HLIST_HEAD(&iter->hw_head);
	hlist_add_head(&iter->hlist, &xgf_renders);

	if (ret)
		*ret = iter;
	return 0;
}

static inline int xgf_ull_multi_overflow(
	unsigned long long multiplicand, int multiplier)
{
	int ret = 0;
	unsigned long long div_after_multi;

	div_after_multi = div_u64((multiplicand*multiplier), multiplier);
	if (multiplicand != div_after_multi)
		ret = 1;

	return ret;
}

static inline unsigned long long xgf_runtime_pro_rata(
	unsigned long long val, int dividend, int divisor)
{
	unsigned long long ret = 0;

	if (xgf_ull_multi_overflow(val, dividend))
		ret = div_u64(ULLONG_MAX, divisor);
	else
		ret = div_u64((val*dividend), divisor);

	return ret;
}

static inline unsigned long long xgf_ema_cal(
	unsigned long long curr, unsigned long long prev)
{
	unsigned long long ret = 0;
	unsigned long long curr_new, prev_new;
	int xgf_ema_rest_dividend;

	if (!prev) {
		ret = curr;
		return ret;
	}

	if (xgf_ema_dividend > 9 || xgf_ema_dividend < 1)
		xgf_ema_dividend = EMA_DIVIDEND;

	xgf_ema_rest_dividend = EMA_DIVISOR - xgf_ema_dividend;

	curr_new = xgf_runtime_pro_rata(curr, xgf_ema_dividend, EMA_DIVISOR);
	prev_new =
		xgf_runtime_pro_rata(prev, xgf_ema_rest_dividend, EMA_DIVISOR);

	if (prev_new > (ULLONG_MAX - curr_new))
		ret = ULLONG_MAX; /* overflow */
	else
		ret = curr_new + prev_new;

	return ret;
}

void fpsgo_create_render_dep(void) {}

void fpsgo_update_render_dep(struct task_struct *p) {}

void xgf_igather_timer(const void * const timer, int fire) {}

void xgf_epoll_igather_timer(
	const void * const timer, ktime_t *expires, int fire)
{}

int has_xgf_dep(pid_t tid)
{
	struct xgf_dep *out_xd, *prev_xd;
	struct xgf_render *render_iter;
	struct hlist_node *n;
	pid_t query_tid;
	int ret = 0;

	xgf_lock(__func__);

	query_tid = tid;

	hlist_for_each_entry_safe(render_iter, n, &xgf_renders, hlist) {

		/* prevent minitop release ceil at sp sub condition */
		if (xgf_spid_sub && xgf_sp_name_id && query_tid
			&& query_tid == render_iter->spid) {
			ret = 1;
			break;
		}

		out_xd = xgf_get_dep(query_tid, render_iter, OUTER_DEPS, 0);
		prev_xd = xgf_get_dep(query_tid, render_iter, PREVI_DEPS, 0);

		if (!out_xd && !prev_xd)
			continue;

		if ((out_xd && out_xd->render_dep)
			|| (prev_xd && prev_xd->render_dep))
			ret = 1;

		if (ret)
			break;
	}

	xgf_unlock(__func__);
	return ret;
}

int gbe2xgf_get_dep_list_num(int pid, unsigned long long bufID)
{
	struct xgf_render *render_iter;
	struct hlist_node *n;
	struct rb_node *out_rbn;
	struct rb_node *pre_rbn;
	struct xgf_dep *out_iter;
	struct xgf_dep *pre_iter;
	int counts = 0;

	if (!pid)
		goto out;

	xgf_lock(__func__);

	hlist_for_each_entry_safe(render_iter, n, &xgf_renders, hlist) {
		if (render_iter->render != pid)
			continue;

		if (render_iter->bufID != bufID)
			continue;

		out_rbn = rb_first(&render_iter->out_deps_list);
		pre_rbn = rb_first(&render_iter->prev_deps_list);

		while (out_rbn != NULL && pre_rbn != NULL) {
			out_iter = rb_entry(out_rbn, struct xgf_dep, rb_node);
			pre_iter = rb_entry(pre_rbn, struct xgf_dep, rb_node);

			if (out_iter->tid < pre_iter->tid) {
				if (out_iter->render_dep)
					counts++;
				out_rbn = rb_next(out_rbn);
			} else if (out_iter->tid > pre_iter->tid) {
				if (pre_iter->render_dep)
					counts++;
				pre_rbn = rb_next(pre_rbn);
			} else {
				if (out_iter->render_dep
						|| pre_iter->render_dep)
					counts++;
				out_rbn = rb_next(out_rbn);
				pre_rbn = rb_next(pre_rbn);
			}
		}

		while (out_rbn != NULL) {
			out_iter = rb_entry(out_rbn, struct xgf_dep, rb_node);
			if (out_iter->render_dep)
				counts++;
			out_rbn = rb_next(out_rbn);
		}

		while (pre_rbn != NULL) {
			pre_iter = rb_entry(pre_rbn, struct xgf_dep, rb_node);
			if (pre_iter->render_dep)
				counts++;
			pre_rbn = rb_next(pre_rbn);
		}
	}

	xgf_unlock(__func__);

out:
	return counts;
}


int fpsgo_fbt2xgf_get_dep_list_num(int pid, unsigned long long bufID)
{
	struct xgf_render *render_iter;
	struct hlist_node *n;
	struct rb_node *out_rbn;
	struct rb_node *pre_rbn;
	struct xgf_dep *out_iter;
	struct xgf_dep *pre_iter;
	int counts = 0;

	if (!pid)
		goto out;

	xgf_lock(__func__);

	hlist_for_each_entry_safe(render_iter, n, &xgf_renders, hlist) {
		if (render_iter->render != pid)
			continue;

		if (render_iter->bufID != bufID)
			continue;

		out_rbn = rb_first(&render_iter->out_deps_list);
		pre_rbn = rb_first(&render_iter->prev_deps_list);

		while (out_rbn != NULL && pre_rbn != NULL) {
			out_iter = rb_entry(out_rbn, struct xgf_dep, rb_node);
			pre_iter = rb_entry(pre_rbn, struct xgf_dep, rb_node);

			if (out_iter->tid < pre_iter->tid) {
				if (out_iter->render_dep)
					counts++;
				out_rbn = rb_next(out_rbn);
			} else if (out_iter->tid > pre_iter->tid) {
				if (pre_iter->render_dep)
					counts++;
				pre_rbn = rb_next(pre_rbn);
			} else {
				if (out_iter->render_dep
						|| pre_iter->render_dep)
					counts++;
				out_rbn = rb_next(out_rbn);
				pre_rbn = rb_next(pre_rbn);
			}
		}

		while (out_rbn != NULL) {
			out_iter = rb_entry(out_rbn, struct xgf_dep, rb_node);
			if (out_iter->render_dep)
				counts++;
			out_rbn = rb_next(out_rbn);
		}

		while (pre_rbn != NULL) {
			pre_iter = rb_entry(pre_rbn, struct xgf_dep, rb_node);
			if (pre_iter->render_dep)
				counts++;
			pre_rbn = rb_next(pre_rbn);
		}
	}

	xgf_unlock(__func__);

out:
	return counts;
}

int gbe2xgf_get_dep_list(int pid, int count,
	struct gbe_runtime *arr, unsigned long long bufID)
{
	struct xgf_render *render_iter;
	struct hlist_node *n;
	struct rb_node *out_rbn;
	struct rb_node *pre_rbn;
	struct xgf_dep *out_iter;
	struct xgf_dep *pre_iter;
	int index = 0;

	if (!pid || !count)
		return 0;

	xgf_lock(__func__);

	hlist_for_each_entry_safe(render_iter, n, &xgf_renders, hlist) {
		if (render_iter->render != pid)
			continue;

		if (render_iter->bufID != bufID)
			continue;

		out_rbn = rb_first(&render_iter->out_deps_list);
		pre_rbn = rb_first(&render_iter->prev_deps_list);

		while (out_rbn != NULL && pre_rbn != NULL) {
			out_iter = rb_entry(out_rbn, struct xgf_dep, rb_node);
			pre_iter = rb_entry(pre_rbn, struct xgf_dep, rb_node);

			if (out_iter->tid < pre_iter->tid) {
				if (out_iter->render_dep && index < count) {
					arr[index].pid = out_iter->tid;
					index++;
				}
				out_rbn = rb_next(out_rbn);
			} else if (out_iter->tid > pre_iter->tid) {
				if (pre_iter->render_dep && index < count) {
					arr[index].pid = pre_iter->tid;
					index++;
				}
				pre_rbn = rb_next(pre_rbn);
			} else {
				if ((out_iter->render_dep
						|| pre_iter->render_dep)
						&& index < count) {
					arr[index].pid = out_iter->tid;
					index++;
				}
				out_rbn = rb_next(out_rbn);
				pre_rbn = rb_next(pre_rbn);
			}
		}

		while (out_rbn != NULL) {
			out_iter = rb_entry(out_rbn, struct xgf_dep, rb_node);
			if (out_iter->render_dep && index < count) {
				arr[index].pid = out_iter->tid;
				index++;
			}
			out_rbn = rb_next(out_rbn);
		}

		while (pre_rbn != NULL) {
			pre_iter = rb_entry(pre_rbn, struct xgf_dep, rb_node);
			if (pre_iter->render_dep && index < count) {
				arr[index].pid = pre_iter->tid;
				index++;
			}
			pre_rbn = rb_next(pre_rbn);
		}
	}

	xgf_unlock(__func__);

	return index;
}


int fpsgo_fbt2xgf_get_dep_list(int pid, int count,
	struct fpsgo_loading *arr, unsigned long long bufID)
{
	struct xgf_render *render_iter;
	struct hlist_node *n;
	struct rb_node *out_rbn;
	struct rb_node *pre_rbn;
	struct xgf_dep *out_iter;
	struct xgf_dep *pre_iter;
	int index = 0;

	if (!pid || !count)
		return 0;

	xgf_lock(__func__);

	hlist_for_each_entry_safe(render_iter, n, &xgf_renders, hlist) {
		if (render_iter->render != pid)
			continue;

		if (render_iter->bufID != bufID)
			continue;

		out_rbn = rb_first(&render_iter->out_deps_list);
		pre_rbn = rb_first(&render_iter->prev_deps_list);

		while (out_rbn != NULL && pre_rbn != NULL) {
			out_iter = rb_entry(out_rbn, struct xgf_dep, rb_node);
			pre_iter = rb_entry(pre_rbn, struct xgf_dep, rb_node);

			if (out_iter->tid < pre_iter->tid) {
				if (out_iter->render_dep && index < count) {
					arr[index].pid = out_iter->tid;
					index++;
				}
				out_rbn = rb_next(out_rbn);
			} else if (out_iter->tid > pre_iter->tid) {
				if (pre_iter->render_dep && index < count) {
					arr[index].pid = pre_iter->tid;
					index++;
				}
				pre_rbn = rb_next(pre_rbn);
			} else {
				if ((out_iter->render_dep
						|| pre_iter->render_dep)
						&& index < count) {
					arr[index].pid = out_iter->tid;
					index++;
				}
				out_rbn = rb_next(out_rbn);
				pre_rbn = rb_next(pre_rbn);
			}
		}

		while (out_rbn != NULL) {
			out_iter = rb_entry(out_rbn, struct xgf_dep, rb_node);
			if (out_iter->render_dep && index < count) {
				arr[index].pid = out_iter->tid;
				index++;
			}
			out_rbn = rb_next(out_rbn);
		}

		while (pre_rbn != NULL) {
			pre_iter = rb_entry(pre_rbn, struct xgf_dep, rb_node);
			if (pre_iter->render_dep && index < count) {
				arr[index].pid = pre_iter->tid;
				index++;
			}
			pre_rbn = rb_next(pre_rbn);
		}
	}

	xgf_unlock(__func__);

	return index;
}

void xgf_reset_renders(void)
{
	struct xgf_render *r_iter;
	struct hlist_node *r_tmp;

	xgf_lockprove(__func__);

	hlist_for_each_entry_safe(r_iter, r_tmp, &xgf_renders, hlist) {
		xgf_clean_deps_list(r_iter, INNER_DEPS);
		xgf_clean_deps_list(r_iter, OUTER_DEPS);
		xgf_clean_deps_list(r_iter, PREVI_DEPS);
		xgf_reset_render_sector(r_iter);
		xgf_reset_render_hw_list(r_iter);
		hlist_del(&r_iter->hlist);
		xgf_free(r_iter);
	}

	xgf_clean_hw_events();
}

void fpsgo_ctrl2xgf_nn_job_begin(unsigned int tid, unsigned long long mid)
{
	xgf_hw_event_collect(HW_AI, tid, mid, 1);
}

int fpsgo_ctrl2xgf_nn_job_end(unsigned int tid, unsigned long long mid)
{
	int hw_type = PER_FRAME;

	hw_type = xgf_hw_event_collect(HW_AI, tid, mid, 0);

	return hw_type;
}

static void xgf_enter_state_xchg(int enable)
{
	int ret = 0;

	if (enable != 0 && enable != 1) {
		ret = -1;
		goto out;
	}

	WARN_ON(!xgf_stat_xchg_fp);

	if (xgf_stat_xchg_fp)
		ret = xgf_stat_xchg_fp(enable);

out:
	xgf_trace("xgf k2ko xchg ret:%d enable:%d", ret, enable);
}

void notify_xgf_ko_ready(void)
{
	xgf_lock(__func__);

	xgf_ko_ready = 1;

	if (xgf_is_enable())
		xgf_enter_state_xchg(xgf_enable);

	xgf_unlock(__func__);
}
EXPORT_SYMBOL(notify_xgf_ko_ready);

void fpsgo_ctrl2xgf_switch_xgf(int val)
{
	xgf_lock(__func__);
	if (val != xgf_enable) {
		xgf_enable = val;

		xgf_reset_renders();

		if (xgf_ko_is_ready())
			xgf_enter_state_xchg(xgf_enable);
	}

	xgf_unlock(__func__);
}

void fpsgo_fstb2xgf_do_recycle(int fstb_active)
{
	unsigned long long now_ts = xgf_get_time();
	long long diff, check_period, recycle_period;
	struct xgf_hw_event *hw_iter;
	struct hlist_node *hw_t;

	/* over 1 seconds since last check2recycle */
	check_period = NSEC_PER_SEC;
	recycle_period = NSEC_PER_SEC >> 1;
	diff = (long long)now_ts - (long long)last_check2recycle_ts;

	xgf_trace("xgf_do_recycle now:%llu last_check:%llu fstb_act:%d",
		now_ts, last_check2recycle_ts, fstb_active);

	xgf_lock(__func__);

	if (!fstb_active) {
		xgf_reset_renders();
		goto out;
	}

	if (xgf_prev_dep_frames != xgf_dep_frames) {
		if (xgf_dep_frames < XGF_DEP_FRAMES_MIN
			|| xgf_dep_frames > XGF_DEP_FRAMES_MAX)
			xgf_dep_frames = XGF_DEP_FRAMES_MIN;
		xgf_prev_dep_frames = xgf_dep_frames;
		xgf_reset_renders();
		goto out;
	}

	if (diff < 0LL || diff < check_period)
		goto done;

	hlist_for_each_entry_safe(hw_iter, hw_t, &xgf_hw_events, hlist) {
		diff = (long long)now_ts - (long long)hw_iter->head_ts;
		if (diff < check_period)
			continue;

		hlist_del(&hw_iter->hlist);
		xgf_free(hw_iter);
	}

out:
	last_check2recycle_ts = now_ts;
done:
	xgf_unlock(__func__);
}

static char *xgf_strcat(char *dest, const char *src,
	size_t buffersize, int *overflow)
{
	int i, j;
	int bufferbound = buffersize - 1;

	for (i = 0; dest[i] != '\0'; i++)
		;
	for (j = 0; src[j] != '\0'; j++) {
		if ((i+j) < bufferbound)
			dest[i+j] = src[j];

		if ((i+j) == bufferbound) {
			*overflow = 1;
			break;
		}
	}

	if (*overflow)
		dest[bufferbound] = '\0';
	else
		dest[i+j] = '\0';

	return dest;
}

static void xgf_log_trace(const char *fmt, ...)
{
	char log[1024];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 1024))
		log[1023] = '\0';
	va_end(args);
	trace_xgf_log(log);
}

static void xgf_print_debug_log(int rpid,
	struct xgf_render *render, unsigned long long runtime)
{
	char total_pid_list[1024] = {"\0"};
	char pid[20] = {"\0"};
	int overflow = 0;
	int len = 0;

	struct xgf_render_sector *xrs;
	struct hlist_node *s, *p;
	struct xgf_pid_rec *pids_iter;

	xgf_lockprove(__func__);

	hlist_for_each_entry_safe(xrs, s, &render->sector_head, hlist) {
		if (strlen(total_pid_list) == 0)
			len = snprintf(pid, sizeof(pid), "%d", xrs->sector_id);
		else
			len = snprintf(pid, sizeof(pid), "-%d", xrs->sector_id);

		if (len < 0 || len >= sizeof(pid))
			goto error;

		overflow = 0;
		xgf_strcat(total_pid_list, pid,
			sizeof(total_pid_list), &overflow);

		if (overflow)
			goto out;

		hlist_for_each_entry_safe(pids_iter, p,
			&xrs->path_head, hlist) {
			len = snprintf(pid, sizeof(pid), ",%d", pids_iter->pid);

			if (len < 0 || len >= sizeof(pid))
				goto error;

			overflow = 0;
			xgf_strcat(total_pid_list, pid,
				sizeof(total_pid_list), &overflow);
			if (overflow)
				goto out;
		}
	}

out:
	if (overflow)
		xgf_log_trace("xgf_debug_log(of) r:%d runtime:%llu pid_list:%s",
		rpid, runtime, total_pid_list);
	else
		xgf_log_trace("xgf_debug_log r:%d runtime:%llu pid_list:%s",
		rpid, runtime, total_pid_list);

	return;

error:
	xgf_log_trace("xgf_debug_log(pid of) r:%d runtime:%llu",
		rpid, runtime);
	return;
}

static int xgf_enter_est_runtime(int rpid, struct xgf_render *render,
	unsigned long long *runtime, unsigned long long ts)
{
	int ret;

	WARN_ON(!xgf_est_runtime_fp);

	if (xgf_est_runtime_fp)
		ret = xgf_est_runtime_fp(rpid, render, runtime, ts);
	else
		ret = -ENOENT;

	return ret;
}

static int xgf_get_spid(struct xgf_render *render)
{
	struct rb_root *r;
	struct rb_node *rbn;
	struct xgf_dep *iter;
	int len, ret = 0;
	unsigned long long now_ts = xgf_get_time();
	long long diff, scan_period;

	xgf_lockprove(__func__);

	if (xgf_spid_ck_period > NSEC_PER_SEC || xgf_spid_ck_period < 0)
		xgf_spid_ck_period = NSEC_PER_SEC;

	scan_period = xgf_spid_ck_period;

	diff = (long long)now_ts - (long long)last_update2spid_ts;

	if (diff < 0LL || diff < scan_period)
		return -1;

	if (xgf_sp_name_id > 1 || xgf_sp_name_id < 0)
		xgf_sp_name_id = 0;

	if (!xgf_sp_name_id)
		xgf_sp_name = SP_ALLOW_NAME;
	else
		xgf_sp_name = SP_ALLOW_NAME2;

	len = strlen(xgf_sp_name);

	if (!len)
		return 0;

	if (xgf_sp_name[len - 1] == '\n') {
		len--;
		xgf_trace("xgf_sp_name len:%d has a change line terminal", len);
	}

	r = &render->deps_list;
	for (rbn = rb_first(r); rbn != NULL; rbn = rb_next(rbn)) {
		struct task_struct *tsk;

		iter = rb_entry(rbn, struct xgf_dep, rb_node);

		rcu_read_lock();
		tsk = find_task_by_vpid(iter->tid);
		if (tsk)
			get_task_struct(tsk);
		rcu_read_unlock();

		if (tsk) {
			if ((tsk->tgid != render->parent)
				|| (task_pid_nr(tsk) == render->render))
				goto tsk_out;

			if (!strncmp(tsk->comm, xgf_sp_name,
				len)) {
				ret = task_pid_nr(tsk);
				put_task_struct(tsk);
				goto out;
			}
tsk_out:
			put_task_struct(tsk);
		}
	}
out:
	last_update2spid_ts = now_ts;
	return ret;
}

int fpsgo_comp2xgf_qudeq_notify(int rpid, unsigned long long bufID, int cmd,
	unsigned long long *run_time, unsigned long long *mid,
	unsigned long long ts)
{
	int ret = XGF_NOTIFY_OK;
	struct xgf_render *r, **rrender;
	struct xgf_hw_rec *hr_iter;
	struct hlist_node *hr;
	unsigned long long raw_runtime = 0;
	int new_spid;
	unsigned long long t_dequeue_time = 0;
	int do_extra_sub = 0;

	if (rpid <= 0 || ts == 0)
		return XGF_PARAM_ERR;

	xgf_lock(__func__);
	if (!xgf_is_enable()) {
		xgf_unlock(__func__);
		return XGF_DISABLE;
	}

	switch (cmd) {

	case XGF_QUEUE_START:
		rrender = &r;
		if (xgf_get_render(rpid, bufID, rrender, 0)) {
			ret = XGF_THREAD_NOT_FOUND;
			goto qudeq_notify_err;
		}
		r->queue.start_ts = ts;
		xgf_reset_render_sector(r);
		break;

	case XGF_QUEUE_END:
		rrender = &r;
		if (xgf_get_render(rpid, bufID, rrender, 1)) {
			ret = XGF_THREAD_NOT_FOUND;
			goto qudeq_notify_err;
		}
		r->queue.end_ts = ts;
		cur_xgf_extra_sub = xgf_extra_sub;

		new_spid = xgf_get_spid(r);
		if (new_spid != -1) {
			xgf_trace("xgf spid:%d => %d", r->spid, new_spid);
			r->spid = new_spid;
		}

		t_dequeue_time = r->deque.end_ts - r->deque.start_ts;
		if (r->deque.start_ts && r->deque.end_ts
			&& (r->deque.end_ts > r->deque.start_ts)
			&& (t_dequeue_time > 2500000)
			&& !cur_xgf_extra_sub) {
			do_extra_sub = 1;
			cur_xgf_extra_sub = 1;
			xgf_trace("xgf extra_sub:%d => %llu", rpid, t_dequeue_time);
		}

		ret = xgf_enter_est_runtime(rpid, r, &raw_runtime, ts);

		if (do_extra_sub)
			cur_xgf_extra_sub = 0;

		if (!raw_runtime)
			*run_time = raw_runtime;
		else {
			r->ema_runtime =
				xgf_ema_cal(raw_runtime, r->ema_runtime);

			*run_time = r->ema_runtime;
		}

		fpsgo_systrace_c_fbt(rpid, bufID, raw_runtime, "raw_t_cpu");

		/* hw event for fbt */
		hlist_for_each_entry_safe(hr_iter, hr, &r->hw_head, hlist) {
			if (hr_iter->hw_type == PER_FRAME)
				*mid = hr_iter->mid;
			xgf_trace("xgf hw mid:%llu event_type:%d hw_type:%d",
			hr_iter->mid, hr_iter->event_type, hr_iter->hw_type);
		}

		xgf_print_debug_log(rpid, r, raw_runtime);
		break;

	case XGF_DEQUEUE_START:
		rrender = &r;
		if (xgf_get_render(rpid, bufID, rrender, 0)) {
			ret = XGF_THREAD_NOT_FOUND;
			goto qudeq_notify_err;
		}
		r->deque.start_ts = ts;
		break;

	case XGF_DEQUEUE_END:
		rrender = &r;
		if (xgf_get_render(rpid, bufID, rrender, 0)) {
			ret = XGF_THREAD_NOT_FOUND;
			goto qudeq_notify_err;
		}
		r->deque.end_ts = ts;
		break;

	default:
		ret = XGF_PARAM_ERR;
		break;
	}

qudeq_notify_err:
	xgf_trace("xgf result:%d at rpid:%d cmd:%d", ret, rpid, cmd);
	xgf_unlock(__func__);
	return ret;
}

void xgf_set_logical_render_runtime(int pid, unsigned long long bufID,
	unsigned long long l_runtime, unsigned long long r_runtime)
{
	int ret = 0;
	struct xgf_render *r, **rrender;

	rrender = &r;
	if (!xgf_get_render(pid, bufID, rrender, 0))
		ret = 1;

	fpsgo_main_trace("[fstb_ko][%d][0x%llx] | raw_runtime=(%llu,%llu)(%d)", pid, bufID,
		l_runtime, r_runtime, ret);
	fpsgo_systrace_c_fbt(pid, bufID, l_runtime, "raw_t_cpu_logical");
	fpsgo_systrace_c_fbt(pid, bufID, r_runtime, "raw_t_cpu_render");
}
EXPORT_SYMBOL(xgf_set_logical_render_runtime);

void xgf_set_logical_render_info(int pid, unsigned long long bufID,
	int *l_arr, int l_num, int *r_arr, int r_num,
	unsigned long long l_start_ts,
	unsigned long long f_start_ts)
{
	char l_dep_str[100] = "";
	char r_dep_str[100] = "";
	int i, length, pos = 0;

	for (i = 0; i < l_num; i++) {
		if (i == 0)
			length = scnprintf(l_dep_str + pos, 100 - pos,
				"%d", l_arr[i]);
		else
			length = scnprintf(l_dep_str + pos, 100 - pos,
				",%d", l_arr[i]);
		pos += length;
	}
	pos = 0;
	for (i = 0; i < r_num; i++) {
		if (i == 0)
			length = scnprintf(r_dep_str + pos, 100 - pos,
				"%d", r_arr[i]);
		else
			length = scnprintf(r_dep_str + pos, 100 - pos,
				",%d", r_arr[i]);
		pos += length;
	}

	fpsgo_main_trace("[fstb_ko][%d][0x%llx] | l_deplist:[%s]", pid, bufID, l_dep_str);
	fpsgo_main_trace("[fstb_ko][%d][0x%llx] | r_deplist:[%s]", pid, bufID, r_dep_str);
	fpsgo_main_trace("[fstb_ko][%d][0x%llx] | logical_start=%llu, frame_start=%llu", pid, bufID,
		l_start_ts, f_start_ts);
}
EXPORT_SYMBOL(xgf_set_logical_render_info);

void xgf_set_timer_info(int pid, unsigned long long bufID, int hrtimer_pid, int hrtimer_flag)
{
	fpsgo_systrace_c_fbt(pid, bufID, hrtimer_pid, "ctrl_fps_pid");
	fpsgo_systrace_c_fbt(pid, bufID, hrtimer_flag, "ctrl_fps_flag");
}
EXPORT_SYMBOL(xgf_set_timer_info);

static ssize_t deplist_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct xgf_render *r_iter;
	struct hlist_node *r_tmp;
	struct rb_root *r;
	struct xgf_dep *iter;
	struct rb_node *n;
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int pos = 0;
	int length;

	xgf_lock(__func__);

	hlist_for_each_entry_safe(r_iter, r_tmp, &xgf_renders, hlist) {
		r = &r_iter->deps_list;
		for (n = rb_first(r); n != NULL; n = rb_next(n)) {
			iter = rb_entry(n, struct xgf_dep, rb_node);
			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"rtid:%d bid:0x%llx itid:%d idx:%d\n",
				r_iter->render, r_iter->bufID,
				iter->tid, iter->frame_idx);
			pos += length;
		}

		r = &r_iter->out_deps_list;
		for (n = rb_first(r); n != NULL; n = rb_next(n)) {
			iter = rb_entry(n, struct xgf_dep, rb_node);

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"rtid:%d bid:0x%llx otid:%d idx:%d\n",
				 r_iter->render, r_iter->bufID,
				 iter->tid, iter->frame_idx);
			pos += length;
		}

		r = &r_iter->prev_deps_list;
		for (n = rb_first(r); n != NULL; n = rb_next(n)) {
			iter = rb_entry(n, struct xgf_dep, rb_node);
			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"rtid:%d bid:0x%llx ptid:%d idx:%d\n",
				 r_iter->render, r_iter->bufID,
				 iter->tid, iter->frame_idx);
			pos += length;
		}
	}

	xgf_unlock(__func__);
	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

struct xgf_trace_event *xgf_event_data;
EXPORT_SYMBOL(xgf_event_data);

static int xgf_nr_cpus __read_mostly;

void *xgf_event_index;
EXPORT_SYMBOL(xgf_event_index);
void *xgf_ko_enabled;
EXPORT_SYMBOL(xgf_ko_enabled);
int xgf_max_events;
EXPORT_SYMBOL(xgf_max_events);

struct fstb_trace_event *fstb_event_data;
EXPORT_SYMBOL(fstb_event_data);
atomic_t fstb_event_data_idx;
EXPORT_SYMBOL(fstb_event_data_idx);
int fstb_event_buffer_size;
EXPORT_SYMBOL(fstb_event_buffer_size);

#define MAX_XGF_EVENTS (xgf_max_events)
#define MAX_EVENT_NUM fstb_event_buffer_size

static void xgf_buffer_update(int cpu, int event, int data, int note,
				unsigned long long ts)
{
	int index;
	struct xgf_trace_event *xte;

	if (!xgf_atomic_read(xgf_ko_enabled))
		return;

Reget:
	index = xgf_atomic_inc_return(xgf_event_index);

	/* protection for if xgf_nr_cpus in error condition */
	if (unlikely(index < 0)) {
		xgf_atomic_set(xgf_event_index, 0);
		return;
	}

	/* prevent the thread that supposed to set */
	/* xgf_event_index to zero is preempted and then cases HWT */
	if (unlikely(index > (MAX_XGF_EVENTS + (xgf_nr_cpus << 1)))) {
		xgf_atomic_set(xgf_event_index, 0);
		return;
	}

	if (unlikely(index == MAX_XGF_EVENTS))
		xgf_atomic_set(xgf_event_index, 0);
	else if (unlikely(index > MAX_XGF_EVENTS))
		goto Reget;

	index -= 1;

	xte = &xgf_event_data[index];
	xte->ts = ts;
	xte->cpu = cpu;
	xte->event = event;
	xte->note = note;
	switch (event) {
	case SCHED_SWITCH:
		xte->prev_pid = data;
		break;

	case SCHED_WAKEUP:
		xte->pid = data;
		break;

	case IPI_RAISE:
		xte->target_cpu = data;
		break;

	case IRQ_ENTRY:
	case IRQ_EXIT:
	case SOFTIRQ_ENTRY:
	case SOFTIRQ_EXIT:
		xte->irqnr = data;
		break;

	default: // IPI_ENTRY, IPI_EXIT
		xte->none = 0;
		break;
	}
}

static void xgf_irq_handler_entry_tracer(void *ignore,
					int irqnr,
					struct irqaction *irq_action)
{
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	xgf_buffer_update(c_wake_cpu, IRQ_ENTRY, irqnr, c_pid, ts);
}

static void xgf_irq_handler_exit_tracer(void *ignore,
					int irqnr,
					struct irqaction *irq_action,
					int ret)
{
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	xgf_buffer_update(c_wake_cpu, IRQ_EXIT, irqnr, c_pid, ts);
}

static void xgf_softirq_entry_tracer(void *ignore, unsigned int vec_nr)
{
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	xgf_buffer_update(c_wake_cpu, SOFTIRQ_ENTRY, vec_nr, c_pid, ts);
}

static void xgf_softirq_exit_tracer(void *ignore, unsigned int vec_nr)
{
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	xgf_buffer_update(c_wake_cpu, SOFTIRQ_EXIT, vec_nr, c_pid, ts);
}

static void xgf_ipi_raise_tracer(void *ignore, const struct cpumask *mask,
				const char *reason)
{
	unsigned int i;
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	if (xgf_nr_cpus == 1)
		xgf_buffer_update(c_wake_cpu, IPI_RAISE, 0, c_pid, ts);
	else {
		for (i = -1; i = xgf_cpumask_next(i, mask), i < xgf_nr_cpus;)
			xgf_buffer_update(c_wake_cpu, IPI_RAISE, i, c_pid, ts);
	}
}

static void xgf_ipi_entry_tracer(void *ignore, const char *reason)
{
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	xgf_buffer_update(c_wake_cpu, IPI_ENTRY, 0, c_pid, ts);
}

static void xgf_ipi_exit_tracer(void *ignore, const char *reason)
{
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	xgf_buffer_update(c_wake_cpu, IPI_EXIT, 0, c_pid, ts);
}

static void xgf_sched_wakeup_tracer(void *ignore, struct task_struct *p)
{
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);
	int p_pid = xgf_get_task_pid(p);

	xgf_buffer_update(c_wake_cpu, SCHED_WAKEUP, p_pid, c_pid, ts);
}

static void xgf_sched_wakeup_new_tracer(void *ignore, struct task_struct *p)
{
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);
	int p_pid = xgf_get_task_pid(p);

	xgf_buffer_update(c_wake_cpu, SCHED_WAKEUP, p_pid, c_pid, ts);
}

static inline long xgf_trace_sched_switch_state(bool preempt,
				struct task_struct *p)
{
	long state = xgf_get_task_state(p);

	if (preempt)
		state = TASK_RUNNING | TASK_STATE_MAX;

	return state;
}

static void xgf_sched_switch_tracer(void *ignore,
				bool preempt,
				struct task_struct *prev,
				struct task_struct *next)
{
	long prev_state;
	long temp_state;
	int skip = 0;
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int prev_pid = xgf_get_task_pid(prev);

	prev_state = xgf_trace_sched_switch_state(preempt, prev);
	temp_state = prev_state & (TASK_STATE_MAX-1);

	if (!temp_state)
		skip = 1;

	xgf_buffer_update(c_wake_cpu, SCHED_SWITCH, prev_pid, skip, ts);
}

static void xgf_sched_waking_tracer(void *ignore, struct task_struct *p) { }

static void xgf_hrtimer_expire_entry_tracer(void *ignore,
	struct hrtimer *hrtimer, ktime_t *now) { }

static void xgf_hrtimer_expire_exit_tracer(void *ignore, struct hrtimer *hrtimer) { }

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool registered;
};

static struct tracepoints_table xgf_tracepoints[] = {
	{.name = "irq_handler_entry", .func = xgf_irq_handler_entry_tracer},
	{.name = "irq_handler_exit", .func = xgf_irq_handler_exit_tracer},
	{.name = "softirq_entry", .func = xgf_softirq_entry_tracer},
	{.name = "softirq_exit", .func = xgf_softirq_exit_tracer},
	{.name = "ipi_raise", .func = xgf_ipi_raise_tracer},
	{.name = "ipi_entry", .func = xgf_ipi_entry_tracer},
	{.name = "ipi_exit", .func = xgf_ipi_exit_tracer},
	{.name = "sched_wakeup", .func = xgf_sched_wakeup_tracer},
	{.name = "sched_wakeup_new", .func = xgf_sched_wakeup_new_tracer},
	{.name = "sched_switch", .func = xgf_sched_switch_tracer},
	{.name = "sched_waking", .func = xgf_sched_waking_tracer},
	{.name = "hrtimer_expire_entry", .func = xgf_hrtimer_expire_entry_tracer},
	{.name = "hrtimer_expire_exit", .func = xgf_hrtimer_expire_exit_tracer},
};

#define FOR_EACH_INTEREST_MAX \
	(sizeof(xgf_tracepoints) / sizeof(struct tracepoints_table))

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < FOR_EACH_INTEREST_MAX; i++)

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(xgf_tracepoints[i].name, tp->name) == 0)
			xgf_tracepoints[i].tp = tp;
	}
}

static void xgf_cleanup(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (xgf_tracepoints[i].registered) {
			xgf_tracepoint_probe_unregister(xgf_tracepoints[i].tp,
						xgf_tracepoints[i].func, NULL);
			xgf_tracepoints[i].registered = false;
		}
	}
}

static void __nocfi xgf_tracing_register(void)
{
	int ret;

	xgf_nr_cpus = xgf_num_possible_cpus();
	xgf_atomic_set(xgf_event_index, 0);
	atomic_set(&fstb_event_data_idx, 0);

	/* xgf_irq_handler_entry_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[0].tp,
						xgf_tracepoints[0].func,  NULL);

	if (ret) {
		pr_info("irq_handler_entry: Couldn't activate tracepoint probe to irq_handler_entry\n");
		goto fail_reg_irq_handler_entry;
	}
	xgf_tracepoints[0].registered = true;

	/* xgf_irq_handler_exit_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[1].tp,
						xgf_tracepoints[1].func,  NULL);

	if (ret) {
		pr_info("irq_handler_exit: Couldn't activate tracepoint probe to irq_handler_exit\n");
		goto fail_reg_irq_handler_exit;
	}
	xgf_tracepoints[1].registered = true;

	/* xgf_softirq_entry_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[2].tp,
						xgf_tracepoints[2].func,  NULL);

	if (ret) {
		pr_info("softirq_entry: Couldn't activate tracepoint probe to softirq_entry\n");
		goto fail_reg_softirq_entry;
	}
	xgf_tracepoints[2].registered = true;

	/* xgf_softirq_exit_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[3].tp,
						xgf_tracepoints[3].func,  NULL);

	if (ret) {
		pr_info("softirq_exit: Couldn't activate tracepoint probe to softirq_exit\n");
		goto fail_reg_softirq_exit;
	}
	xgf_tracepoints[3].registered = true;

	/* xgf_ipi_raise_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[4].tp,
						xgf_tracepoints[4].func,  NULL);

	if (ret) {
		pr_info("ipi_raise: Couldn't activate tracepoint probe to ipi_raise\n");
		goto fail_reg_ipi_raise;
	}
	xgf_tracepoints[4].registered = true;

	/* xgf_ipi_entry_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[5].tp,
						xgf_tracepoints[5].func,  NULL);

	if (ret) {
		pr_info("ipi_entry: Couldn't activate tracepoint probe to ipi_entry\n");
		goto fail_reg_ipi_entry;
	}
	xgf_tracepoints[5].registered = true;

	/* xgf_ipi_exit_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[6].tp,
						xgf_tracepoints[6].func,  NULL);

	if (ret) {
		pr_info("ipi_exit: Couldn't activate tracepoint probe to ipi_exit\n");
		goto fail_reg_ipi_exit;
	}
	xgf_tracepoints[6].registered = true;

	/* xgf_sched_wakeup_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[7].tp,
						xgf_tracepoints[7].func,  NULL);

	if (ret) {
		pr_info("wakeup trace: Couldn't activate tracepoint probe to kernel_sched_wakeup\n");
		goto fail_reg_sched_wakeup;
	}
	xgf_tracepoints[7].registered = true;

	/* xgf_sched_wakeup_new_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[8].tp,
						xgf_tracepoints[8].func,  NULL);

	if (ret) {
		pr_info("wakeup trace: Couldn't activate tracepoint probe to kernel_sched_wakeup_new\n");
		goto fail_reg_sched_wakeup_new;
	}
	xgf_tracepoints[8].registered = true;

	/* xgf_sched_switch_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[9].tp,
						xgf_tracepoints[9].func,  NULL);

	if (ret) {
		pr_info("sched trace: Couldn't activate tracepoint probe to kernel_sched_switch\n");
		goto fail_reg_sched_switch;
	}
	xgf_tracepoints[9].registered = true;

	/* xgf_sched_waking_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[10].tp,
						xgf_tracepoints[10].func,  NULL);

	if (ret) {
		pr_info("sched trace: Couldn't activate tracepoint probe to kernel_sched_waking\n");
		goto fail_reg_sched_waking;
	}
	xgf_tracepoints[10].registered = true;

	/* xgf_hrtimer_expire_entry_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[11].tp,
						xgf_tracepoints[11].func,  NULL);

	if (ret) {
		pr_info("hrtimer_entry trace: Couldn't activate tracepoint probe to kernel_hrtimer_expire_entry\n");
		goto fail_reg_hrtimer_expire_entry;
	}
	xgf_tracepoints[11].registered = true;

	/* xgf_hrtimer_expire_exit_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[12].tp,
						xgf_tracepoints[12].func,  NULL);

	if (ret) {
		pr_info("hrtimer_exit trace: Couldn't activate tracepoint probe to kernel_hrtimer_expire_exit\n");
		goto fail_reg_hrtimer_expire_exit;
	}
	xgf_tracepoints[12].registered = true;

	xgf_atomic_set(xgf_event_index, 0);
	atomic_set(&fstb_event_data_idx, 0);
	return; /* successful registered all */

fail_reg_hrtimer_expire_exit:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[12].tp,
					xgf_tracepoints[12].func,  NULL);
fail_reg_hrtimer_expire_entry:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[11].tp,
					xgf_tracepoints[11].func,  NULL);
fail_reg_sched_waking:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[10].tp,
					xgf_tracepoints[10].func,  NULL);
fail_reg_sched_switch:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[9].tp,
					xgf_tracepoints[9].func,  NULL);
	xgf_tracepoints[9].registered = false;
fail_reg_sched_wakeup_new:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[8].tp,
					xgf_tracepoints[8].func,  NULL);
	xgf_tracepoints[8].registered = false;
fail_reg_sched_wakeup:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[7].tp,
					xgf_tracepoints[7].func,  NULL);
	xgf_tracepoints[7].registered = false;
fail_reg_ipi_exit:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[6].tp,
					xgf_tracepoints[6].func,  NULL);
	xgf_tracepoints[6].registered = false;
fail_reg_ipi_entry:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[5].tp,
					xgf_tracepoints[5].func,  NULL);
	xgf_tracepoints[5].registered = false;
fail_reg_ipi_raise:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[4].tp,
					xgf_tracepoints[4].func,  NULL);
	xgf_tracepoints[4].registered = false;
fail_reg_softirq_exit:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[3].tp,
					xgf_tracepoints[3].func,  NULL);
	xgf_tracepoints[3].registered = false;
fail_reg_softirq_entry:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[2].tp,
					xgf_tracepoints[2].func,  NULL);
	xgf_tracepoints[2].registered = false;
fail_reg_irq_handler_exit:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[1].tp,
					xgf_tracepoints[1].func,  NULL);
	xgf_tracepoints[1].registered = false;
fail_reg_irq_handler_entry:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[0].tp,
					xgf_tracepoints[0].func,  NULL);
	xgf_tracepoints[0].registered = false;

	xgf_atomic_set(xgf_ko_enabled, 0);
	xgf_atomic_set(xgf_event_index, 0);
	atomic_set(&fstb_event_data_idx, 0);
}

static void __nocfi xgf_tracing_unregister(void)
{
	xgf_tracepoint_probe_unregister(xgf_tracepoints[0].tp,
					xgf_tracepoints[0].func,  NULL);
	xgf_tracepoints[0].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[1].tp,
					xgf_tracepoints[1].func,  NULL);
	xgf_tracepoints[1].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[2].tp,
					xgf_tracepoints[2].func,  NULL);
	xgf_tracepoints[2].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[3].tp,
					xgf_tracepoints[3].func,  NULL);
	xgf_tracepoints[3].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[4].tp,
					xgf_tracepoints[4].func,  NULL);
	xgf_tracepoints[4].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[5].tp,
					xgf_tracepoints[5].func,  NULL);
	xgf_tracepoints[5].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[6].tp,
					xgf_tracepoints[6].func,  NULL);
	xgf_tracepoints[6].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[7].tp,
					xgf_tracepoints[7].func,  NULL);
	xgf_tracepoints[7].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[8].tp,
					xgf_tracepoints[8].func,  NULL);
	xgf_tracepoints[8].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[9].tp,
					xgf_tracepoints[9].func,  NULL);
	xgf_tracepoints[9].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[10].tp,
					xgf_tracepoints[10].func,  NULL);
	xgf_tracepoints[10].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[11].tp,
					xgf_tracepoints[11].func,  NULL);
	xgf_tracepoints[11].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[12].tp,
					xgf_tracepoints[12].func,  NULL);
	xgf_tracepoints[12].registered = false;
	xgf_atomic_set(xgf_event_index, 0);
	atomic_set(&fstb_event_data_idx, 0);
}

int xgf_stat_xchg(int xgf_enable)
{
	int ret = -1;

	if (xgf_enable) {
		xgf_tracing_register();
		ret = 1;
		xgf_atomic_set(xgf_ko_enabled, 1);
	} else {
		xgf_tracing_unregister();
		ret = 0;
		xgf_atomic_set(xgf_ko_enabled, 0);
	}

	return ret;
}

int __init init_xgf_ko(void)
{
	int i;

	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (xgf_tracepoints[i].tp == NULL) {
			pr_debug("XGF KO Error, %s not found\n",
					xgf_tracepoints[i].name);
			xgf_cleanup();
			return -1;
		}
	}

	xgf_event_index = xgf_atomic_val_assign(0);
	atomic_set(&fstb_event_data_idx, 0);
	xgf_ko_enabled = xgf_atomic_val_assign(1);

	xgf_stat_xchg_fp = xgf_stat_xchg;

	return 0;
}

static KOBJ_ATTR_RO(deplist);

int __init init_xgf(void)
{
	init_xgf_ko();

	if (!fpsgo_sysfs_create_dir(NULL, "xgf", &xgf_kobj))
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_deplist);

	return 0;
}
