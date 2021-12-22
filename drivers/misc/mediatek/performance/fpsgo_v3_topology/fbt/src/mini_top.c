// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/math64.h>
#include <linux/sort.h>
#include <linux/sched/clock.h>
#include <linux/sched/task.h>
#include <linux/sched/cputime.h>

#include <trace/events/fpsgo.h>
#include <mt-plat/fpsgo_common.h>

#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fbt_cpu.h"
#include "fstb.h"
#include "xgf.h"
#include "mini_top.h"


/* 32 ms based */
#define _ALIVE_THRS			((u64)(0x1 << (25 + 1)))



static atomic_t __minitop_enable = ATOMIC_INIT(0);

/* Configurable (re-write from define) */
static int __minitop_n;
static int __warmup_order;
static int __cooldn_order;
static int __thrs_heavy;

static DEFINE_MUTEX(minitop_mlock);
static struct kobject *minitop_kobj;
static unsigned int minitop_life;

static int nr_cpus __read_mostly;
static struct rb_root minitop_root;
static u64 heartbeat_ts;

static DEFINE_SPINLOCK(minitop_mwlock);
static LIST_HEAD(minitop_mws);
static struct minitop_work mwa[3];


static void minitop_trace(const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);
	trace_minitop_log(log);
}

static int __util_cmp(const void *a, const void *b)
{
	return ((const struct tid_util *)b)->util -
	       ((const struct tid_util *)a)->util;
}

static inline void minitop_lock(const char *tag)
{
	mutex_lock(&minitop_mlock);
}

static inline void minitop_unlock(const char *tag)
{
	mutex_unlock(&minitop_mlock);
}

static inline void minitop_lockprove(const char *tag)
{
	WARN_ON(!mutex_is_locked(&minitop_mlock));
}

/**
 * __warmup_mask - return a mask involving configurable parameter
 */
static inline unsigned int __warmup_mask(void)
{
	return (unsigned int)((0x1 << __warmup_order) - 1);
}

static inline int __thrs_heavy_util_half(void)
{
	return (int)(((__thrs_heavy << 10) / 100) >> 1);
}

static void minitop_nominate_work(struct work_struct *);

static inline struct minitop_work *minitop_get_work(void)
{
	unsigned long flags;
	struct minitop_work *mw;

	spin_lock_irqsave(&minitop_mwlock, flags);
	mw = list_first_entry_or_null(&minitop_mws,
				      struct minitop_work, link);
	if (!mw)
		goto end_get;

	INIT_WORK(&mw->work, minitop_nominate_work);
	list_del_init(&mw->link);
end_get:
	spin_unlock_irqrestore(&minitop_mwlock, flags);
	return mw;
}

static inline void minitop_put_work(struct minitop_work *mw)
{
	unsigned long flags;

	if (!mw)
		return;

	spin_lock_irqsave(&minitop_mwlock, flags);
	list_add_tail(&mw->link, &minitop_mws);
	spin_unlock_irqrestore(&minitop_mwlock, flags);
}

static inline int minitop_if_active_then_lock(void)
{
	minitop_lock(__func__);
	if (atomic_read(&__minitop_enable) == 1)
		return 1;

	minitop_unlock(__func__);
	return 0;
}

static inline void __minitop_cleanup(void)
{
	struct minitop_rec *mr;

	minitop_lockprove(__func__);

	while (!RB_EMPTY_ROOT(&minitop_root)) {
		mr = rb_entry(minitop_root.rb_node, struct minitop_rec,
			      node);
		rb_erase(&mr->node, &minitop_root);
		kfree(mr);
	}
}

static void minitop_cleanup(void)
{
	if (!minitop_if_active_then_lock())
		return;
	__minitop_cleanup();
	minitop_unlock(__func__);

	/* Enable ceiling control to reset */
	fbt_switch_ceiling(1);

	minitop_trace("clean up");
}

static struct minitop_rec *__minitop_nominate(pid_t tid, int source)
{
	struct rb_node **p = &minitop_root.rb_node;
	struct rb_node *parent = NULL;
	struct minitop_rec *mr;

	minitop_lockprove(__func__);

	minitop_trace(" %5d being nominated", tid);

	while (*p) {
		parent = *p;
		mr = rb_entry(parent, struct minitop_rec, node);

		if (mr->tid < tid)
			p = &(*p)->rb_left;
		else if (mr->tid > tid)
			p = &(*p)->rb_right;
		else {
			mr->source |= source;
			return mr;
		}
	}

	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (unlikely(!mr))
		return NULL;

	mr->tid = tid;
	mr->source = source;

	rb_link_node(&mr->node, parent, p);
	rb_insert_color(&mr->node, &minitop_root);
	return mr;
}

static int __get_runtime(pid_t tid, u64 *runtime)
{
	struct task_struct *p;

	if (unlikely(!tid))
		return -EINVAL;

	rcu_read_lock();
	p = find_task_by_vpid(tid);
	if (!p) {
		minitop_trace(" %5d not found to erase", tid);
		rcu_read_unlock();
		return -ESRCH;
	}
	get_task_struct(p);
	rcu_read_unlock();

	*runtime = (u64)task_sched_runtime(p);
	put_task_struct(p);

	return 0;
}

static u64 __get_timestamp(void)
{
	u64 ts;

	preempt_disable();
	ts = cpu_clock(smp_processor_id());
	preempt_enable();
	return ts;
}

static inline int __init_mr_inst(struct minitop_rec *mr, u64 runtime_in)
{
	int ret;
	u64 runtime;

	minitop_lockprove(__func__);

	if (runtime_in)
		runtime = runtime_in;
	else {
		ret = __get_runtime(mr->tid, &runtime);
		if (ret)
			return ret;
	}

	mr->runtime_inst = runtime;
	minitop_trace(" %5d inst runtime %12llu", mr->tid, mr->runtime_inst);
	return 0;
}

static inline int __init_mr(struct minitop_rec *mr)
{
	int ret;
	u64 runtime;

	minitop_lockprove(__func__);

	ret = __get_runtime(mr->tid, &runtime);
	if (ret)
		return ret;

	mr->init_runtime = runtime;
	mr->init_timestamp = __get_timestamp();
	minitop_trace(" %5d init...%12llu", mr->tid, mr->init_runtime);

	return __init_mr_inst(mr, runtime);
}

static inline int __minitop_is_render(struct minitop_rec *mr)
{
	int ret;

	ret = has_xgf_dep(mr->tid);
	return !!ret;
}

static int __minitop_has_heavy(struct minitop_rec *mr, int *heavy)
{
	int ret = -EINVAL;
	u64 runtime, ts, ratio;
	int keep = 0;

	minitop_lockprove(__func__);

	if (unlikely(!heavy))
		return ret;

	ret = __get_runtime(mr->tid, &runtime);
	if (ret)
		goto err_cleanup;
	/* Re-init return value for usage of following "goto" */
	ret = -EINVAL;

	if (unlikely(runtime < mr->init_runtime))
		goto err_cleanup;

	if (unlikely(runtime < mr->runtime_inst))
		goto err_cleanup;

	ts = __get_timestamp();
	if (unlikely(ts < mr->init_timestamp))
		goto err_cleanup;

	/* Such a long-life task ?! */
	if (unlikely(mr->life == U32_MAX))
		goto err_cleanup;


	mr->life++;

	/* TODO: overflow check */
	/* Since unit of @ratio is %, multiply numerator by 100 */
	ratio = (runtime - mr->init_runtime) * 100;
	/* 32 ms * slot size */
	ratio >>= (25 + __warmup_order);
	mr->ratio = div_u64(ratio, mr->life);

	minitop_trace(" %5d %3llu%% %12llu->%12llu life-%d debnc-%d",
		      mr->tid, mr->ratio, mr->init_runtime, runtime,
		      mr->life, mr->debnc);

	if (mr->source & MINITOP_FTEH) {
		if (mr->debnc_fteh > 0)
			keep |= 1;
		else
			minitop_trace(" %5d %3llu%% end debnc FTEH",
				      mr->tid, mr->ratio);
	}

	if (mr->source & MINITOP_FBT) {
		/* Instant life count is just one */
		mr->ratio_inst = (runtime - mr->runtime_inst) * 100;
		mr->ratio_inst >>= (25 + __warmup_order);
		mr->ratio_inst = clamp((int)(mr->ratio_inst), 1, 100);

		__init_mr_inst(mr, 0);

		if (mr->debnc_fbt > 0)
			keep |= 1;
		else
			minitop_trace(" %5d %3d%% end debnc FBT",
				      mr->tid, (int)mr->ratio_inst);
	}

	if ((mr->source & MINITOP_SCHED) == 0)
		goto end_sched_nomi;


	/* Check thread nominated by scheduler at last */
	if (mr->ratio >= __thrs_heavy) {
		mr->debnc = (int)(0x1 << __cooldn_order);
		mr->ever  = 1;
		*heavy = 1;
		return 0;
	}

	if (mr->debnc > 0)
		mr->debnc--;

	if (mr->debnc > 0) {
		*heavy = 1;
		return 0;
	}

	/*
	 * A light or end-debounce thread; return error to remove once
	 * no one needs this
	 */
end_sched_nomi:
	ret = keep ? 0 : -EAGAIN;
	if (mr->ever)
		minitop_trace(" %5d %3llu%% end debnc heavy",
			      mr->tid, mr->ratio);

err_cleanup:
	*heavy = 0;
	return ret;
}

static int minitop_has_heavy(void)
{
	struct rb_node *node, *next;
	struct minitop_rec *mr;
	int heavy = 0, h;
	int curr_tid = 0;
	static int last_tid;
	int ret;

	minitop_lockprove(__func__);

	for (node = rb_first(&minitop_root); node; node = next) {
		next = rb_next(node);

		mr = rb_entry(node, struct minitop_rec, node);

		if (!mr->init_runtime) {
			ret = __init_mr(mr);
			if (ret) {
				rb_erase(node, &minitop_root);
				kfree(mr);
			}
			/* Do no more at init stage */
			continue;
		}

		ret = __minitop_has_heavy(mr, &h);
		if (ret) {
			rb_erase(node, &minitop_root);
			kfree(mr);
			continue;
		}

		/* Exist a heavy thread at least; house keeping only */
		if (heavy)
			continue;

		/* Such heavy should NOT be render */
		if (__minitop_is_render(mr)) {
			/* Reveal heavy render */
			if (h)
				minitop_trace(" %5d render", mr->tid);
			continue;
		}

		heavy = h;
		if (h)
			last_tid = curr_tid = mr->tid;
	}

	if (heavy)
		fpsgo_systrace_c(FPSGO_DEBUG_MANDATORY, curr_tid, 0, 1,
				 "minitop_free_ceiling");
	else if (last_tid) {
		fpsgo_systrace_c(FPSGO_DEBUG_MANDATORY, last_tid, 0, 0,
				 "minitop_free_ceiling");
		last_tid = 0;
	}

	minitop_trace("minitop switch ceiling %s by curr/last=%5d/%5d",
		      heavy ? "off" : "on", curr_tid, last_tid);
	return !!heavy;
}


static inline int minitop_fpsgo_active(void)
{
	/* FIXME: hint should come from high-level module, ex: main */
	if (is_fstb_active(1000000LL)) /* 1000 ms */
		return 1;

	minitop_trace("FPS inactive, clean up");
	minitop_cleanup();
	return 0;
}

static int minitop_heartbeat(void)
{
	u64 curr_ts;
	int ret = 0; /* default 0 */

	minitop_lockprove(__func__);

	/*
	 * UINT_MAX = 2^n -1, which should step to 2^n if no overflow
	 * issue. Consider @life is used to trigger warm up activity
	 * every 2^@_WARMUP_ORDER, which is a good initial value
	 * to replace 2^n and triggers warm up also.
	 */
	if (unlikely(minitop_life == UINT_MAX))
		minitop_life = 0x1 << __warmup_order;
	else
		minitop_life++;


	curr_ts = __get_timestamp();
	if (unlikely(curr_ts < heartbeat_ts))
		goto ret_hrtb;

	if (curr_ts - heartbeat_ts > _ALIVE_THRS)
		goto ret_hrtb;

	ret = 1;

ret_hrtb:
	heartbeat_ts = curr_ts;
	if (!ret)
		minitop_trace("re-alive");
	return ret;
}

static void minitop_nominate_work(struct work_struct *work)
{
	struct minitop_work *mw;
	int i, j;
	struct tid_util *tu;
	int ret = -1; /* MUST init as -1 */
	int alive;

	/* MUST get @mw first */
	mw = container_of(work, struct minitop_work, work);

	if (!minitop_fpsgo_active())
		goto end_nomi_work;

	tu = kcalloc(nr_cpus, sizeof(*tu), GFP_KERNEL);
	if (!tu)
		goto end_nomi_work;

	for (i = 0; i < nr_cpus; i++) {
		tu[i].tid  = mw->tid[i];
		tu[i].util = mw->util[i];
	}
	minitop_put_work(mw);
	sort(tu, nr_cpus, sizeof(struct tid_util), __util_cmp, NULL);


	if (!minitop_if_active_then_lock())
		return;
	/*
	 * Scheduler callback is hooked on sched-tick, which may enter
	 * NOHZ and no more callback will be seen until leaving NOHZ.
	 * If we back to alive from NOHZ, do some housekeeping here.
	 */
	alive = minitop_heartbeat();

	for (i = 0, j = 0; (i < __minitop_n && j < nr_cpus); j++) {
		/*
		 * Filter duplicated nominees to try our best to
		 * monitor @MINITOP_N if possible, but this depends
		 * on nominee list provided by scheduler. If number
		 * of distinguished nominated thread is less than
		 * @MINITOP_N, we can just monitor that much threads
		 * in this round.
		 */
		if (j != 0 && tu[j].tid == tu[j-1].tid)
			continue;

		i++;

		/*
		 * Filter tasks, whose loading is far from threshold.
		 * Besides, for such sorted list, once an entry fails
		 * the threshold, the following would fail too. Exit.
		 */
		if (tu[j].util < __thrs_heavy_util_half())
			break;

		__minitop_nominate(tu[j].tid, MINITOP_SCHED);
	}

	kfree(tu);

	/* Per-window warm up, @ret is either 0 or 1 */
	if ((minitop_life & __warmup_mask()) == 0)
		ret = minitop_has_heavy();

	minitop_unlock(__func__);

	/*
	 * Once to suffice warm-up window, switch ceiling. Otherwise,
	 * if not alive in the past, switch ceiling on for safety.
	 */
	if (ret != -1)
		fbt_switch_ceiling(!ret);
	else if (!alive)
		fbt_switch_ceiling(1);

	return;

end_nomi_work:
	minitop_put_work(mw);
}

void fpsgo_sched_nominate(pid_t *tid, int *util)
{
	struct minitop_work *mw;
	bool suc;
	int i;

	if (atomic_read(&__minitop_enable) != 1)
		return;

	for (i = 0; i < nr_cpus; i++)
		minitop_trace("sched: %5d util-%4d", tid[i], util[i]);

	mw = minitop_get_work();
	if (unlikely(!mw))
		return;

	memcpy(mw->tid, tid, sizeof(pid_t) * nr_cpus);
	memcpy(mw->util, util, sizeof(int) * nr_cpus);

	suc = schedule_work(&mw->work);
	if (unlikely(!suc))
		minitop_put_work(mw);
}

/**
 * FBT (Frame Budget Tunner) supports
 */
int fpsgo_fbt2minitop_start(int count, struct fpsgo_loading *fl)
{
	int i;
	struct minitop_rec *mr;

	if (!minitop_if_active_then_lock())
		return -EAGAIN;

	for (i = 0; i < count; i++) {
		minitop_trace(" %5d being nominated FBT", fl[i].pid);
		if (!fl[i].pid)
			continue;
		mr = __minitop_nominate(fl[i].pid, MINITOP_FBT);
		if (!mr)
			continue;

		mr->debnc_fbt = (int)(0x1 << __cooldn_order);
	}

	minitop_unlock(__func__);
	return 0;
}

int fpsgo_fbt2minitop_end(void)
{
	struct rb_node *n, *next;
	struct minitop_rec *mr;

	if (!minitop_if_active_then_lock())
		return -EAGAIN;

	for (n = rb_first(&minitop_root); n; n = next) {
		next = rb_next(n);

		mr = rb_entry(n, struct minitop_rec, node);
		if (mr->source != MINITOP_FBT) {
			mr->source &= ~(MINITOP_FBT);
			continue;
		}

		rb_erase(n, &minitop_root);
		kfree(mr);
	}

	minitop_unlock(__func__);
	return 0;
}

int fpsgo_fbt2minitop_query(int count, struct fpsgo_loading *fl)
{
	struct rb_node *n = minitop_root.rb_node;
	struct minitop_rec *mr;
	int i;

	if (!minitop_if_active_then_lock())
		return -EAGAIN;

	for (n = rb_first(&minitop_root); n; n = rb_next(n)) {
		mr = rb_entry(n, struct minitop_rec, node);
		if (mr->source & MINITOP_FBT)
			mr->debnc_fbt--;
	}

	for (i = 0; i < count; i++) {
		if (!fl[i].pid)
			continue;

		n = minitop_root.rb_node;
		while (n) {
			mr = rb_entry(n, struct minitop_rec, node);

			if (mr->tid < fl[i].pid)
				n = n->rb_left;
			else if (mr->tid > fl[i].pid)
				n = n->rb_right;
			else {
				/* Treat 0 as uninitialized value; skip it */
				fl[i].loading = (int)(mr->ratio_inst);
				minitop_trace(" give FBT %5d loading=%5d ",
						fl[i].pid, fl[i].loading);
				break;
			}
			/* Not found */
			fl[i].loading = -1;
		}
	}

	minitop_unlock(__func__);

	return 0;
}

int fpsgo_fbt2minitop_query_single(pid_t pid)
{
	struct rb_node *n = minitop_root.rb_node;
	struct minitop_rec *mr;

	if (!pid)
		return -1;

	if (!minitop_if_active_then_lock())
		return -1;

	n = minitop_root.rb_node;
	while (n) {
		mr = rb_entry(n, struct minitop_rec, node);

		if (mr->tid < pid)
			n = n->rb_left;
		else if (mr->tid > pid)
			n = n->rb_right;
		else {
			int ret;

			/* Treat 0 as uninitialized value; skip it */
			ret = (int)(mr->ratio_inst);
			minitop_trace(" give FBT %5d loading=%5d (single)",
					pid, ret);
			/* Recharge; leave decay to full query */
			mr->debnc_fbt = (int)(0x1 << __cooldn_order);

			minitop_unlock(__func__);
			return ret;
		}
	}

	minitop_unlock(__func__);
	return -1;
}

#define MINITOP_SYSFS_WRITE(name, lb, ub); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	int val = -1; \
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE]; \
	int arg; \
\
	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, \
				FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (kstrtoint(acBuffer, 0, &arg) == 0) { \
				val = arg; \
			} else \
				return count; \
		} \
	} \
\
	if ((val < (lb)) || (val > (ub))) \
		return count; \
\
	if (!minitop_if_active_then_lock()) \
		return count; \
	__##name = val; \
	__minitop_cleanup(); \
	minitop_unlock(__func__); \
\
	fbt_switch_ceiling(1); \
	return count; \
}

static ssize_t list_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int pos = 0;
	int length;
	struct rb_node *n;
	struct minitop_rec *mr;

	if (!minitop_if_active_then_lock())
		return scnprintf(buf, PAGE_SIZE, "%s", temp);

	for (n = rb_first(&minitop_root); n; n = rb_next(n)) {
		mr = rb_entry(n, struct minitop_rec, node);
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			" %5d %3llu%% src-0x%2x life-%d debnc-%d/%d/%d\n",
			mr->tid, mr->ratio, (unsigned int)mr->source,
			mr->life, mr->debnc, mr->debnc_fteh,
			mr->debnc_fbt);
		pos += length;
	}

	minitop_unlock(__func__);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static KOBJ_ATTR_RO(list);

static ssize_t minitop_n_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	minitop_lock(__func__);
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			" Top %d is tracked\n", __minitop_n);
	pos += length;
	minitop_unlock(__func__);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

MINITOP_SYSFS_WRITE(minitop_n, 1, nr_cpus);

static KOBJ_ATTR_RW(minitop_n);

static ssize_t warmup_order_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	minitop_lock(__func__);
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			" Slot size is 2^%d=%d tick(s)\n",
		   __warmup_order, (1 << __warmup_order));
	pos += length;
	minitop_unlock(__func__);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

MINITOP_SYSFS_WRITE(warmup_order, 0, 20)

static KOBJ_ATTR_RW(warmup_order);

static ssize_t cooldn_order_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	minitop_lock(__func__);
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			" Debounce slot is 2^%d=%d\n",
		   __cooldn_order, (1 << __cooldn_order));
	pos += length;
	minitop_unlock(__func__);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

MINITOP_SYSFS_WRITE(cooldn_order, 0, 10)

static KOBJ_ATTR_RW(cooldn_order);

static ssize_t thrs_heavy_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	minitop_lock(__func__);
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			" Threshold of heavy is %d\n", __thrs_heavy);
	pos += length;
	minitop_unlock(__func__);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

MINITOP_SYSFS_WRITE(thrs_heavy, 0, 100)

static KOBJ_ATTR_RW(thrs_heavy);


static ssize_t enable_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	minitop_lock(__func__);
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			" Mini TOP is %s\n",
		   atomic_read(&__minitop_enable) ? "ON" : "OFF");
	pos += length;
	minitop_unlock(__func__);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t enable_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	if ((val < 0) || (val > 1))
		return count;

	minitop_lock(__func__);
	atomic_set(&__minitop_enable, val);
	__minitop_cleanup();
	minitop_unlock(__func__);

	/* Switch ceiling on for safety */
	fbt_switch_ceiling(1);
	return count;
}

static KOBJ_ATTR_RW(enable);

void __exit minitop_exit(void)
{
	minitop_cleanup();

	fpsgo_sysfs_remove_file(minitop_kobj, &kobj_attr_list);
	fpsgo_sysfs_remove_file(minitop_kobj, &kobj_attr_minitop_n);
	fpsgo_sysfs_remove_file(minitop_kobj, &kobj_attr_warmup_order);
	fpsgo_sysfs_remove_file(minitop_kobj, &kobj_attr_cooldn_order);
	fpsgo_sysfs_remove_file(minitop_kobj, &kobj_attr_thrs_heavy);
	fpsgo_sysfs_remove_file(minitop_kobj, &kobj_attr_enable);

	fpsgo_sysfs_remove_dir(&minitop_kobj);
}

int __init minitop_init(void)
{
	int i;

	/* Configurable */
	__minitop_n    = 3;
	__warmup_order = 3;
	__cooldn_order = 2;
	__thrs_heavy   = 70;


	nr_cpus        = num_possible_cpus();
	minitop_root   = RB_ROOT;
	heartbeat_ts   = __get_timestamp();
	for (i = 0; i < 3; i++) {
		INIT_WORK(&mwa[i].work, minitop_nominate_work);
		INIT_LIST_HEAD(&mwa[i].link);
		list_add_tail(&mwa[i].link, &minitop_mws);
	}

	if (!fpsgo_sysfs_create_dir(NULL, "minitop", &minitop_kobj)) {
		fpsgo_sysfs_create_file(minitop_kobj, &kobj_attr_list);
		fpsgo_sysfs_create_file(minitop_kobj, &kobj_attr_minitop_n);
		fpsgo_sysfs_create_file(minitop_kobj, &kobj_attr_warmup_order);
		fpsgo_sysfs_create_file(minitop_kobj, &kobj_attr_cooldn_order);
		fpsgo_sysfs_create_file(minitop_kobj, &kobj_attr_thrs_heavy);
		fpsgo_sysfs_create_file(minitop_kobj, &kobj_attr_enable);
	}


	/* once everything is ready, hook into scheduler */
#ifdef CONFIG_MTK_SCHED_RQAVG_KS
	fpsgo_sched_nominate_fp = fpsgo_sched_nominate;
#endif

	atomic_set(&__minitop_enable, 1);
	return 0;
}
