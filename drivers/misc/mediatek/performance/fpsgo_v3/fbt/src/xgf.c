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
#include <linux/module.h>
#include <linux/sched/clock.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/kallsyms.h>
#include <linux/tracepoint.h>
#include <linux/sched/task.h>
#include <linux/kernel.h>
#include <trace/trace.h>
#include <trace/events/sched.h>
#include <trace/events/ipi.h>
#include <trace/events/irq.h>
#include <trace/events/timer.h>
#include <mt-plat/fpsgo_common.h>

#include "xgf.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fpsgo_usedext.h"
#include "fstb.h"

static DEFINE_MUTEX(xgf_main_lock);
static DEFINE_MUTEX(fstb_ko_lock);
static DEFINE_MUTEX(xgff_frames_lock);
static DEFINE_MUTEX(xgf_policy_cmd_lock);
static int xgf_enable;
int xgf_trace_enable;
static int xgf_log_trace_enable;
static int xgf_ko_ready;
static int xgf_nr_cpus __read_mostly;
static struct kobject *xgf_kobj;
static struct kmem_cache *xgf_render_cachep __ro_after_init;
static struct kmem_cache *xgf_dep_cachep __ro_after_init;
static struct kmem_cache *xgf_runtime_sect_cachep __ro_after_init;
static struct kmem_cache *xgf_spid_cachep __ro_after_init;
static struct kmem_cache *xgff_frame_cachep __ro_after_init;
static struct rb_root xgf_policy_cmd_tree;
static unsigned long long last_check2recycle_ts;
static unsigned long long last_update2spid_ts;
static unsigned long long xgf_ema_mse;
static unsigned long long xgf_ema2_mse;
static char *xgf_sp_name = SP_ALLOW_NAME;
static int xgf_extra_sub;
static int xgf_force_no_extra_sub;
static int cur_xgf_extra_sub;
static int xgf_dep_frames = 10;
static int xgf_prev_dep_frames = 10;
static int xgf_spid_sub = XGF_DO_SP_SUB;
static int xgf_ema_dividend = EMA_DIVIDEND;
static int xgf_spid_ck_period = NSEC_PER_SEC;
static int xgf_sp_name_id;
static int xgf_spid_list_length;
static int xgf_wspid_list_length;
static int xgf_cfg_spid;
static int xgf_ema2_enable;
static int xgf_display_rate = DEFAULT_DFRC;
static int is_xgff_mips_exp_enable;

int fstb_frame_num = 20;
EXPORT_SYMBOL(fstb_frame_num);
int fstb_no_stable_thr = 5;
EXPORT_SYMBOL(fstb_no_stable_thr);
int fstb_can_update_thr = 60;
EXPORT_SYMBOL(fstb_can_update_thr);
int fstb_target_fps_margin_low_fps = 3;
EXPORT_SYMBOL(fstb_target_fps_margin_low_fps);
int fstb_target_fps_margin_high_fps = 5;
EXPORT_SYMBOL(fstb_target_fps_margin_high_fps);
int fstb_fps_num = TARGET_FPS_LEVEL;
EXPORT_SYMBOL(fstb_fps_num);
int fstb_fps_choice[TARGET_FPS_LEVEL] = {20, 25, 30, 40, 45, 60, 90, 120, 144, 240};
EXPORT_SYMBOL(fstb_fps_choice);
int fstb_no_r_timer_enable;
EXPORT_SYMBOL(fstb_no_r_timer_enable);

module_param(xgf_sp_name, charp, 0644);
module_param(xgf_extra_sub, int, 0644);
module_param(xgf_force_no_extra_sub, int, 0644);
module_param(xgf_dep_frames, int, 0644);
module_param(xgf_spid_sub, int, 0644);
module_param(xgf_ema_dividend, int, 0644);
module_param(xgf_spid_ck_period, int, 0644);
module_param(xgf_sp_name_id, int, 0644);
module_param(xgf_cfg_spid, int, 0644);
module_param(xgf_ema2_enable, int, 0644);
module_param(fstb_frame_num, int, 0644);
module_param(fstb_no_stable_thr, int, 0644);
module_param(fstb_can_update_thr, int, 0644);
module_param(fstb_target_fps_margin_low_fps, int, 0644);
module_param(fstb_target_fps_margin_high_fps, int, 0644);
module_param(fstb_no_r_timer_enable, int, 0644);

HLIST_HEAD(xgf_renders);
HLIST_HEAD(xgff_frames);
HLIST_HEAD(xgf_spid_list);
HLIST_HEAD(xgf_wspid_list);

int (*xgf_est_runtime_fp)(
	pid_t r_pid,
	struct xgf_render *render,
	unsigned long long *runtime,
	unsigned long long ts
	);
EXPORT_SYMBOL(xgf_est_runtime_fp);
int (*xgff_est_runtime_fp)(
	pid_t r_pid,
	struct xgf_render *render,
	unsigned long long *runtime,
	unsigned long long ts
	);
EXPORT_SYMBOL(xgff_est_runtime_fp);
int (*xgff_update_start_prev_index_fp)(struct xgf_render *render);
EXPORT_SYMBOL(xgff_update_start_prev_index_fp);
int (*fpsgo_xgf2ko_calculate_target_fps_fp)(
	int pid,
	unsigned long long bufID,
	int *target_fps_margin,
	unsigned long long cur_queue_end_ts,
	int eara_is_active
	);
EXPORT_SYMBOL(fpsgo_xgf2ko_calculate_target_fps_fp);
void (*fpsgo_xgf2ko_do_recycle_fp)(
	int pid,
	unsigned long long bufID
	);
EXPORT_SYMBOL(fpsgo_xgf2ko_do_recycle_fp);
long long (*xgf_ema2_predict_fp)(struct xgf_ema2_predictor *pt, long long X);
EXPORT_SYMBOL(xgf_ema2_predict_fp);
void (*xgf_ema2_init_fp)(struct xgf_ema2_predictor *pt);
EXPORT_SYMBOL(xgf_ema2_init_fp);

static inline void xgf_lock(const char *tag)
{
	mutex_lock(&xgf_main_lock);
}

static inline void xgf_unlock(const char *tag)
{
	mutex_unlock(&xgf_main_lock);
}

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

	if (!xgf_trace_enable)
		return;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);
	trace_printk(log);
}
EXPORT_SYMBOL(xgf_trace);

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
	return t->__state;
}
EXPORT_SYMBOL(xgf_get_task_state);

static inline int xgf_ko_is_ready(void)
{
	return xgf_ko_ready;
}

int xgf_get_display_rate(void)
{
	return xgf_display_rate;
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
	int tmp_process_id;
	char tmp_process_name[16];
	char tmp_thread_name[16];
	struct task_struct *gtsk, *tsk;

	tmp_process_id = xgf_get_process_id(pid);
	if (tmp_process_id < 0)
		return ret;

	rcu_read_lock();
	gtsk = find_task_by_vpid(tmp_process_id);
	if (gtsk) {
		get_task_struct(gtsk);
		strncpy(tmp_process_name, gtsk->comm, 16);
		tmp_process_name[15] = '\0';
		put_task_struct(gtsk);
	} else
		tmp_process_name[0] = '\0';
	rcu_read_unlock();

	if ((tmp_process_id == process_id) ||
		strstr(tmp_process_name, "surfaceflinger"))
		ret = 1;

	if (ret) {
		rcu_read_lock();
		tsk = find_task_by_vpid(pid);
		if (tsk) {
			get_task_struct(tsk);
			strncpy(tmp_thread_name, tsk->comm, 16);
			tmp_thread_name[15] = '\0';
			put_task_struct(tsk);
		} else
			tmp_thread_name[0] = '\0';
		rcu_read_unlock();

		if (strstr(tmp_thread_name, "RTHeartBeat") ||
			strstr(tmp_thread_name, "mali-"))
			ret = 0;
	}

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

	if (strstr(thread_name, SP_ALLOW_NAME))
		ret = 1;

	return ret;
}
EXPORT_SYMBOL(xgf_check_specific_pid);

void fpsgo_ctrl2xgf_set_display_rate(int dfrc_fps)
{
	xgf_lock(__func__);
	xgf_display_rate = dfrc_fps;
	xgf_unlock(__func__);
}

static inline int xgf_is_enable(void)
{
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

unsigned long long xgf_calculate_sqrt(unsigned long long x)
{
	unsigned long long b, m, y = 0;

	if (x <= 1)
		return x;

	m = 1ULL << (__fls(x) & ~1ULL);
	while (m != 0) {
		b = y + m;
		y >>= 1;

		if (x >= b) {
			x -= b;
			y += m;
		}
		m >>= 2;
	}

	return y;
}
EXPORT_SYMBOL(xgf_calculate_sqrt);

static void *xgf_render_alloc(void)
{
	void *pvBuf = NULL;

	if (xgf_render_cachep)
		pvBuf = kmem_cache_alloc(xgf_render_cachep,
			GFP_KERNEL | __GFP_ZERO);

	return pvBuf;
}

static void xgf_render_free(void *pvBuf)
{
	if (xgf_render_cachep)
		kmem_cache_free(xgf_render_cachep, pvBuf);
}

static void *xgf_dep_alloc(void)
{
	void *pvBuf = NULL;

	if (xgf_dep_cachep)
		pvBuf = kmem_cache_alloc(xgf_dep_cachep,
			GFP_ATOMIC | __GFP_ZERO);

	return pvBuf;
}

static void xgf_dep_free(void *pvBuf)
{
	if (xgf_dep_cachep)
		kmem_cache_free(xgf_dep_cachep, pvBuf);
}

static void *xgf_runtime_sect_alloc(void)
{
	void *pvBuf = NULL;

	if (xgf_runtime_sect_cachep)
		pvBuf = kmem_cache_alloc(xgf_runtime_sect_cachep,
			GFP_ATOMIC | __GFP_ZERO);

	return pvBuf;
}

static void xgf_runtime_sect_free(void *pvBuf)
{
	if (xgf_runtime_sect_cachep)
		kmem_cache_free(xgf_runtime_sect_cachep, pvBuf);
}

static void *xgf_spid_alloc(void)
{
	void *pvBuf = NULL;

	if (xgf_spid_cachep)
		pvBuf = kmem_cache_alloc(xgf_spid_cachep,
			GFP_ATOMIC | __GFP_ZERO);

	return pvBuf;
}

static void xgf_spid_free(void *pvBuf)
{
	if (xgf_spid_cachep)
		kmem_cache_free(xgf_spid_cachep, pvBuf);
}

static void *xgff_frame_alloc(void)
{
	void *pvBuf = NULL;

	if (xgff_frame_cachep)
		pvBuf = kmem_cache_alloc(xgff_frame_cachep,
			GFP_KERNEL | __GFP_ZERO);

	return pvBuf;
}

static void xgff_frame_free(void *pvBuf)
{
	if (xgff_frame_cachep)
		kmem_cache_free(xgff_frame_cachep, pvBuf);
}

void *xgf_alloc(int size, int cmd)
{
	void *pvBuf;

	switch (cmd) {
	case XGF_RENDER:
		pvBuf = xgf_render_alloc();
		break;

	case XGF_DEP:
		pvBuf = xgf_dep_alloc();
		break;

	case XGF_RUNTIME_SECT:
		pvBuf = xgf_runtime_sect_alloc();
		break;

	case XGF_SPID:
		pvBuf = xgf_spid_alloc();
		break;

	case XGFF_FRAME:
		pvBuf = xgff_frame_alloc();
		break;

	default:
		if (size <= PAGE_SIZE)
			pvBuf = kzalloc(size, GFP_ATOMIC);
		else
			pvBuf = vzalloc(size);
		break;
	}

	return pvBuf;
}
EXPORT_SYMBOL(xgf_alloc);

void xgf_free(void *pvBuf, int cmd)
{
	switch (cmd) {
	case XGF_RENDER:
		xgf_render_free(pvBuf);
		break;

	case XGF_DEP:
		xgf_dep_free(pvBuf);
		break;

	case XGF_RUNTIME_SECT:
		xgf_runtime_sect_free(pvBuf);
		break;

	case XGF_SPID:
		xgf_spid_free(pvBuf);
		break;

	case XGFF_FRAME:
		xgff_frame_free(pvBuf);
		break;

	default:
		kvfree(pvBuf);
		break;
	}
}
EXPORT_SYMBOL(xgf_free);

static struct xgf_policy_cmd *xgf_get_policy_cmd(int tgid, int ema2_enable,
	unsigned long long ts, int force)
{
	struct rb_node **p = &xgf_policy_cmd_tree.rb_node;
	struct rb_node *parent;
	struct xgf_policy_cmd *iter;

	while (*p) {
		parent = *p;
		iter = rb_entry(parent, struct xgf_policy_cmd, rb_node);

		if (tgid < iter->tgid)
			p = &(*p)->rb_left;
		else if (tgid > iter->tgid)
			p = &(*p)->rb_right;
		else
			return iter;
	}

	if (!force)
		return NULL;

	iter = xgf_alloc(sizeof(*iter), NATIVE_ALLOC);
	if (!iter)
		return NULL;

	iter->tgid = tgid;
	iter->ema2_enable = ema2_enable;
	iter->ts = ts;

	rb_link_node(&iter->rb_node, parent, p);
	rb_insert_color(&iter->rb_node, &xgf_policy_cmd_tree);

	return iter;
}

int set_xgf_spid_list(char *proc_name,
		char *thrd_name, int action)
{
	struct xgf_spid *new_xgf_spid;
	int retval = 0;

	xgf_lock(__func__);

	if (!strncmp("0", proc_name, 1) &&
			!strncmp("0", thrd_name, 1)) {

		struct xgf_spid *iter;
		struct hlist_node *t;

		hlist_for_each_entry_safe(iter, t, &xgf_spid_list, hlist) {
			hlist_del(&iter->hlist);
			xgf_free(iter, XGF_SPID);
		}

		xgf_spid_list_length = 0;
		goto out;
	}

	if (xgf_spid_list_length >= XGF_MAX_SPID_LIST_LENGTH) {
		retval = -ENOMEM;
		goto out;
	}

	new_xgf_spid = xgf_alloc(sizeof(*new_xgf_spid), XGF_SPID);
	if (!new_xgf_spid) {
		retval = -ENOMEM;
		goto out;
	}

	if (!strncpy(new_xgf_spid->process_name, proc_name, 16)) {
		xgf_free(new_xgf_spid, XGF_SPID);
		retval = -ENOMEM;
		goto out;
	}
	new_xgf_spid->process_name[15] = '\0';

	if (!strncpy(new_xgf_spid->thread_name,	thrd_name, 16)) {
		xgf_free(new_xgf_spid, XGF_SPID);
		retval = -ENOMEM;
		goto out;
	}
	new_xgf_spid->thread_name[15] = '\0';

	new_xgf_spid->pid = 0;
	new_xgf_spid->rpid = 0;
	new_xgf_spid->tid = 0;
	new_xgf_spid->bufID = 0;
	new_xgf_spid->action = action;

	hlist_add_head(&new_xgf_spid->hlist, &xgf_spid_list);

	xgf_spid_list_length++;

out:
	xgf_unlock(__func__);

	return retval;
}

static ssize_t xgf_spid_list_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct xgf_spid *xgf_spid_iter = NULL;
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int pos = 0;
	int length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		"%s\t%s\t%s\n",
		"process_name",
		"thread_name",
		"action");
	pos += length;

	hlist_for_each_entry(xgf_spid_iter, &xgf_spid_list, hlist) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%s\t%s\t%d\n",
			xgf_spid_iter->process_name,
			xgf_spid_iter->thread_name,
			xgf_spid_iter->action);
		pos += length;
	}

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		"\n%s\t%s\t%s\t%s\t%s\t%s\n",
		"process_name",
		"thread_name",
		"render",
		"pid",
		"tid",
		"action");
	pos += length;

	hlist_for_each_entry(xgf_spid_iter, &xgf_wspid_list, hlist) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%s\t%s\t%d\t%d\t%d\n",
			xgf_spid_iter->process_name,
			xgf_spid_iter->thread_name,
			xgf_spid_iter->rpid,
			xgf_spid_iter->pid,
			xgf_spid_iter->tid,
			xgf_spid_iter->action);
		pos += length;
	}

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t xgf_spid_list_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int ret = count;
	char proc_name[16], thrd_name[16];
	int action;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			acBuffer[count] = '\0';
			if (sscanf(acBuffer, "%15s %15s %d",
				proc_name, thrd_name, &action) != 3)
				goto err;

			if (set_xgf_spid_list(proc_name, thrd_name, action))
				goto err;
		}
	}

err:
	return ret;
}

static KOBJ_ATTR_RW(xgf_spid_list);

static ssize_t xgf_trace_enable_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%d\n", xgf_trace_enable);
	pos += length;

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t xgf_trace_enable_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int val = -1;
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer,
				FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	if (val < 0 || val > 1)
		return count;
	xgf_trace_enable = val;

	return count;
}

static KOBJ_ATTR_RW(xgf_trace_enable);

static ssize_t xgf_log_trace_enable_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%d\n", xgf_log_trace_enable);
	pos += length;

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t xgf_log_trace_enable_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int val = -1;
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer,
				FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	if (val < 0 || val > 1)
		return count;
	xgf_log_trace_enable = val;

	return count;
}

static KOBJ_ATTR_RW(xgf_log_trace_enable);

static void xgf_reset_wspid_list(void)
{
	struct xgf_spid *xgf_spid_iter;
	struct hlist_node *t;

	hlist_for_each_entry_safe(xgf_spid_iter, t, &xgf_wspid_list, hlist) {
		hlist_del(&xgf_spid_iter->hlist);
		xgf_free(xgf_spid_iter, XGF_SPID);
		xgf_wspid_list_length--;
	}
}

static void xgf_render_reset_wspid_list(struct xgf_render *render)
{
	struct xgf_spid *xgf_spid_iter;
	struct hlist_node *t;

	hlist_for_each_entry_safe(xgf_spid_iter, t, &xgf_wspid_list, hlist) {
		if (xgf_spid_iter->rpid == render->render
			&& xgf_spid_iter->bufID == render->bufID) {
			hlist_del(&xgf_spid_iter->hlist);
			xgf_free(xgf_spid_iter, XGF_SPID);
			xgf_wspid_list_length--;
		}
	}
}

static int xgf_render_setup_wspid_list(struct xgf_render *render)
{
	int ret = 1;
	struct xgf_spid *xgf_spid_iter;
	struct task_struct *gtsk, *sib;
	struct xgf_spid *new_xgf_spid;
	int tlen = 0;

	rcu_read_lock();
	gtsk = find_task_by_vpid(render->parent);
	if (gtsk) {
		get_task_struct(gtsk);
		list_for_each_entry(sib, &gtsk->thread_group, thread_group) {

			get_task_struct(sib);

			hlist_for_each_entry(xgf_spid_iter, &xgf_spid_list, hlist) {
				if (strncmp(gtsk->comm, xgf_spid_iter->process_name, 16))
					continue;

				tlen = strlen(xgf_spid_iter->thread_name);

				if (!strncmp(sib->comm, xgf_spid_iter->thread_name, tlen)) {
					new_xgf_spid = xgf_alloc(sizeof(*new_xgf_spid), XGF_SPID);
					if (!new_xgf_spid) {
						ret = -ENOMEM;
						put_task_struct(sib);
						goto out;
					}

					if (!strncpy(new_xgf_spid->process_name,
							xgf_spid_iter->process_name, 16)) {
						xgf_free(new_xgf_spid, XGF_SPID);
						ret = -ENOMEM;
						put_task_struct(sib);
						goto out;
					}
					new_xgf_spid->process_name[15] = '\0';

					if (!strncpy(new_xgf_spid->thread_name,
							xgf_spid_iter->thread_name, 16)) {
						xgf_free(new_xgf_spid, XGF_SPID);
						ret = -ENOMEM;
						put_task_struct(sib);
						goto out;
					}
					new_xgf_spid->thread_name[15] = '\0';

					new_xgf_spid->pid = gtsk->pid;
					new_xgf_spid->rpid = render->render;
					new_xgf_spid->tid = sib->pid;
					new_xgf_spid->bufID = render->bufID;
					new_xgf_spid->action = xgf_spid_iter->action;
					hlist_add_head(&new_xgf_spid->hlist, &xgf_wspid_list);
					xgf_wspid_list_length++;
				}
			}
			put_task_struct(sib);
		}
out:
		put_task_struct(gtsk);
	}
	rcu_read_unlock();
	return ret;
}

static void xgf_render_dep_reset(struct xgf_render *render)
{
	int i, total;
	struct xgf_render_dep *xrd;

	if (render->xrd_arr_idx < 0 || render->xrd_arr_idx > MAX_DEP_PATH_NUM)
		total = MAX_DEP_PATH_NUM;
	else
		total = render->xrd_arr_idx;

	for (i = 0; i < total; i++) {
		xrd = &render->xrd_arr[i];
		xrd->xdt_arr_idx = 0;
		xrd->finish_flag = 0;
		xrd->track_tid = 0;
		xrd->track_ts = 0;
		xrd->raw_head_index = 0;
		xrd->raw_head_ts = 0;
		xrd->raw_tail_index = 0;
		xrd->raw_tail_ts = 0;
		xrd->specific_flag = 0;
	}

	render->xrd_arr_idx = 0;
	render->l_num = 0;
	render->r_num = 0;
}

void xgf_clean_deps_list(struct xgf_render *render, int pos)
{
	struct xgf_dep *iter;

	if (pos == INNER_DEPS) {
		while (!RB_EMPTY_ROOT(&render->deps_list)) {
			iter = rb_entry(render->deps_list.rb_node,
						struct xgf_dep, rb_node);
			rb_erase(&iter->rb_node, &render->deps_list);
			xgf_free(iter, XGF_DEP);
		}
	}

	if (pos == OUTER_DEPS) {
		while (!RB_EMPTY_ROOT(&render->out_deps_list)) {
			iter = rb_entry(render->out_deps_list.rb_node,
						struct xgf_dep, rb_node);
			rb_erase(&iter->rb_node, &render->out_deps_list);
			xgf_free(iter, XGF_DEP);
		}
	}

	if (pos == PREVI_DEPS) {
		while (!RB_EMPTY_ROOT(&render->prev_deps_list)) {
			iter = rb_entry(render->prev_deps_list.rb_node,
						struct xgf_dep, rb_node);
			rb_erase(&iter->rb_node, &render->prev_deps_list);
			xgf_free(iter, XGF_DEP);
		}
	}
}
EXPORT_SYMBOL(xgf_clean_deps_list);

int xgf_dep_frames_mod(struct xgf_render *render, int pos)
{
	int ret = 0;
	int pre_dep_frames = render->dep_frames;

	if (pos == PREVI_DEPS) {
		if (render->frame_count == 0)
			ret = INT_MAX % pre_dep_frames;
		else
			ret = (render->frame_count - 1) % pre_dep_frames;
	} else
		ret = render->frame_count % pre_dep_frames;

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

	xd = xgf_alloc(sizeof(*xd), XGF_DEP);

	if (!xd)
		return NULL;

	xd->tid = tid;
	xd->render_dep = 1;
	xd->frame_idx = xgf_dep_frames_mod(render, pos);
	xd->action = 0;

	rb_link_node(&xd->rb_node, parent, p);
	rb_insert_color(&xd->rb_node, r);

	return xd;
}
EXPORT_SYMBOL(xgf_get_dep);

void xgf_update_deps_list(struct xgf_render *render, int pos)
{
	struct xgf_dep *xd;
	struct rb_root *prev_r;
	struct xgf_dep *prev_iter;
	struct rb_root *out_r;
	struct xgf_dep *out_iter;
	struct rb_node *n;
	int prev_frame_index = xgf_dep_frames_mod(render, PREVI_DEPS);
	int curr_frame_index = xgf_dep_frames_mod(render, OUTER_DEPS);

	if (pos != PREVI_DEPS)
		return;

	prev_r = &render->prev_deps_list;
	n = rb_first(prev_r);
	while (n) {
		prev_iter = rb_entry(n, struct xgf_dep, rb_node);
		if (prev_iter->frame_idx == curr_frame_index) {
			rb_erase(n, prev_r);
			n = rb_first(prev_r);
			xgf_free(prev_iter, XGF_DEP);
		} else
			n = rb_next(n);
	}

	out_r = &render->out_deps_list;
	for (n = rb_first(out_r); n; n = rb_next(n)) {
		out_iter = rb_entry(n, struct xgf_dep, rb_node);
		xd = xgf_get_dep(out_iter->tid, render, PREVI_DEPS, 0);
		if (xd)
			xd->frame_idx = prev_frame_index;
		else
			xd = xgf_get_dep(out_iter->tid, render, PREVI_DEPS, 1);
	}
}
EXPORT_SYMBOL(xgf_update_deps_list);

void xgf_duplicate_deps_list(struct xgf_render *render)
{
	struct xgf_dep *xd;
	struct rb_root *r;
	struct xgf_dep *iter;
	struct rb_node *n;

	r = &render->deps_list;
	for (n = rb_first(r); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct xgf_dep, rb_node);
		xd = xgf_get_dep(iter->tid, render, OUTER_DEPS, 1);
	}
}
EXPORT_SYMBOL(xgf_duplicate_deps_list);

static void xgf_ema2_free(struct xgf_ema2_predictor *pt)
{
	kfree(pt);
}

static struct xgf_ema2_predictor *xgf_ema2_get_pred(void)
{
	struct xgf_ema2_predictor *pt = kzalloc(sizeof(*pt), GFP_KERNEL);

	if (xgf_ema2_init_fp)
		xgf_ema2_init_fp(pt);
	else {
		if (pt) {
			xgf_ema2_free(pt);
			pt = 0;
		}
	}

	if (!pt)
		return 0;

	return pt;
}

static void xgf_ema2_dump_rho(struct xgf_ema2_predictor *pt, char *buffer)
{
	int i;

	if (!pt)
		return;

	for (i = 0; i < N; i++)
		buffer += sprintf(buffer, " %lld", pt->rho[i]);
	buffer += sprintf(buffer, " -");
	for (i = 0; i < N; i++)
		buffer += sprintf(buffer, " %lld", pt->L[i]);
}

static void xgf_ema2_dump_info_frames(struct xgf_ema2_predictor *pt, char *buffer)
{
	int i;

	if (!pt)
		return;

	for (i = 0; i < N; i++)
		buffer += sprintf(buffer, " %lld", pt->L[i]);
}


static int xgf_get_render(pid_t rpid, unsigned long long bufID, struct xgf_render **ret,
	int force, unsigned long long queue_end_ts)
{
	struct xgf_render *iter;

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

	iter = xgf_alloc(sizeof(*iter), XGF_RENDER);

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
			xgf_free(iter, XGF_RENDER);
			return -EINVAL;
		}

		iter->parent = tsk->tgid;
		iter->render = rpid;
		put_task_struct(tsk);

		iter->bufID = bufID;
		iter->xrd_arr_idx = 0;
		iter->l_num = 0;
		iter->r_num = 0;
		iter->curr_index = 0;
		iter->curr_ts = 0;
		iter->prev_index = 0;
		iter->prev_ts = 0;
		iter->event_count = 0;
		iter->prev_queue_end_ts = queue_end_ts;
		iter->frame_count = 0;
		iter->queue.start_ts = 0;
		iter->queue.end_ts = 0;
		iter->deque.start_ts = 0;
		iter->deque.end_ts = 0;
		iter->raw_runtime = 0;
		iter->ema_runtime = 0;
		iter->spid = 0;
		iter->dep_frames = xgf_prev_dep_frames;
		iter->ema2_enable = 0;
		iter->ema2_pt = NULL;
	}
	hlist_add_head(&iter->hlist, &xgf_renders);

	iter->ema2_pt = xgf_ema2_get_pred();

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
		ret = ULLONG_MAX;
	else
		ret = curr_new + prev_new;

	return ret;
}

static void xgf_add_pid2prev_dep(struct xgf_render *render, int tid, int action)
{
	struct xgf_dep *xd;
	int curr_frame_index;

	if (!tid || !render || tid < 0)
		return;

	curr_frame_index = xgf_dep_frames_mod(render, PREVI_DEPS);
	xd = xgf_get_dep(tid, render, PREVI_DEPS, 0);
	if (xd)
		xd->frame_idx = curr_frame_index;
	else
		xd = xgf_get_dep(tid, render, PREVI_DEPS, 1);
	xd->action = action;
}

static void xgf_wspid_list_add2prev(struct xgf_render *render)
{
	struct xgf_spid *xgf_spid_iter;
	struct hlist_node *t;

	hlist_for_each_entry_safe(xgf_spid_iter, t, &xgf_wspid_list, hlist) {
		if (xgf_spid_iter->rpid == render->render
			&& xgf_spid_iter->bufID == render->bufID)
			xgf_add_pid2prev_dep(render, xgf_spid_iter->tid, xgf_spid_iter->action);
	}
}

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

		if (render_iter->spid)
			xgf_add_pid2prev_dep(render_iter, render_iter->spid, 0);

		if (xgf_cfg_spid)
			xgf_wspid_list_add2prev(render_iter);

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
				if ((out_iter->render_dep || pre_iter->render_dep))
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

		if (render_iter->spid)
			xgf_add_pid2prev_dep(render_iter, render_iter->spid, 0);

		xgf_add_pid2prev_dep(render_iter, pid, 0);

		if (xgf_cfg_spid)
			xgf_wspid_list_add2prev(render_iter);

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
				if ((out_iter->render_dep || pre_iter->render_dep))
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

		if (render_iter->spid)
			xgf_add_pid2prev_dep(render_iter, render_iter->spid, 0);

		if (xgf_cfg_spid)
			xgf_wspid_list_add2prev(render_iter);

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
				if ((out_iter->render_dep || pre_iter->render_dep)
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

		if (render_iter->spid)
			xgf_add_pid2prev_dep(render_iter, render_iter->spid, 0);

		xgf_add_pid2prev_dep(render_iter, pid, 0);

		if (xgf_cfg_spid)
			xgf_wspid_list_add2prev(render_iter);

		out_rbn = rb_first(&render_iter->out_deps_list);
		pre_rbn = rb_first(&render_iter->prev_deps_list);

		while (out_rbn != NULL && pre_rbn != NULL) {
			out_iter = rb_entry(out_rbn, struct xgf_dep, rb_node);
			pre_iter = rb_entry(pre_rbn, struct xgf_dep, rb_node);

			if (out_iter->tid < pre_iter->tid) {
				if (out_iter->render_dep && index < count) {
					arr[index].pid = out_iter->tid;
					arr[index].action = out_iter->action;
					index++;
				}
				out_rbn = rb_next(out_rbn);
			} else if (out_iter->tid > pre_iter->tid) {
				if (pre_iter->render_dep && index < count) {
					arr[index].pid = pre_iter->tid;
					arr[index].action = pre_iter->action;
					index++;
				}
				pre_rbn = rb_next(pre_rbn);
			} else {
				if ((out_iter->render_dep || pre_iter->render_dep)
					&& index < count) {
					arr[index].pid = out_iter->tid;
					arr[index].action = out_iter->action;
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
				arr[index].action = out_iter->action;
				index++;
			}
			out_rbn = rb_next(out_rbn);
		}

		while (pre_rbn != NULL) {
			pre_iter = rb_entry(pre_rbn, struct xgf_dep, rb_node);
			if (pre_iter->render_dep && index < count) {
				arr[index].pid = pre_iter->tid;
				arr[index].action = pre_iter->action;
				index++;
			}
			pre_rbn = rb_next(pre_rbn);
		}
	}

	xgf_unlock(__func__);

	return index;
}

int xgff_get_dep_list(int pid, int count,
	unsigned int *arr, struct xgf_render *render_iter)
{
	struct rb_node *out_rbn;
	struct rb_node *pre_rbn;
	struct xgf_dep *out_iter;
	struct xgf_dep *pre_iter;
	int index = 0;

	if (!count)
		return 0;

	out_rbn = rb_first(&render_iter->out_deps_list);
	pre_rbn = rb_first(&render_iter->prev_deps_list);

	while (out_rbn != NULL && pre_rbn != NULL) {
		out_iter = rb_entry(out_rbn, struct xgf_dep, rb_node);
		pre_iter = rb_entry(pre_rbn, struct xgf_dep, rb_node);

		if (out_iter->tid < pre_iter->tid) {
			if (out_iter->render_dep && index < count) {
				arr[index] = out_iter->tid;
				index++;
			}
			out_rbn = rb_next(out_rbn);
		} else if (out_iter->tid > pre_iter->tid) {
			if (pre_iter->render_dep && index < count) {
				arr[index] = pre_iter->tid;
				index++;
			}
			pre_rbn = rb_next(pre_rbn);
		} else {
			if ((out_iter->render_dep || pre_iter->render_dep)
				&& index < count) {
				arr[index] = out_iter->tid;
				index++;
			}
			out_rbn = rb_next(out_rbn);
			pre_rbn = rb_next(pre_rbn);
		}
	}

	while (out_rbn != NULL) {
		out_iter = rb_entry(out_rbn, struct xgf_dep, rb_node);
		if (out_iter->render_dep && index < count) {
			arr[index] = out_iter->tid;
			index++;
		}
		out_rbn = rb_next(out_rbn);
	}

	while (pre_rbn != NULL) {
		pre_iter = rb_entry(pre_rbn, struct xgf_dep, rb_node);
		if (pre_iter->render_dep && index < count) {
			arr[index] = pre_iter->tid;
			index++;
		}
		pre_rbn = rb_next(pre_rbn);
	}

	return index;
}

static void xgf_reset_render(struct xgf_render *iter)
{
	xgf_clean_deps_list(iter, INNER_DEPS);
	xgf_clean_deps_list(iter, OUTER_DEPS);
	xgf_clean_deps_list(iter, PREVI_DEPS);
	xgf_render_dep_reset(iter);

	if (iter->ema2_pt) {
		xgf_ema2_free(iter->ema2_pt);
		iter->ema2_pt = NULL;
	}

	hlist_del(&iter->hlist);
	xgf_free(iter, XGF_RENDER);
}

static void xgff_reset_render(struct xgff_frame *iter)
{
	xgf_clean_deps_list(&iter->xgfrender, INNER_DEPS);
	xgf_clean_deps_list(&iter->xgfrender, OUTER_DEPS);
	xgf_clean_deps_list(&iter->xgfrender, PREVI_DEPS);
	xgf_render_dep_reset(&iter->xgfrender);

	if (iter->xgfrender.ema2_pt) {
		xgf_ema2_free(iter->xgfrender.ema2_pt);
		iter->xgfrender.ema2_pt = NULL;
	}

	hlist_del(&iter->hlist);
	xgf_free(iter, XGFF_FRAME);
}

void xgf_reset_all_renders(void)
{
	struct xgf_render *r_iter;
	struct hlist_node *r_tmp;

	hlist_for_each_entry_safe(r_iter, r_tmp, &xgf_renders, hlist) {
		xgf_reset_render(r_iter);
	}
	xgf_reset_wspid_list();
}

static void xgff_reset_all_renders(void)
{
	struct xgff_frame *r;
	struct hlist_node *h;

	mutex_lock(&xgff_frames_lock);

	hlist_for_each_entry_safe(r, h, &xgff_frames, hlist) {
		xgff_reset_render(r);
	}

	mutex_unlock(&xgff_frames_lock);
}

void fpsgo_fstb2xgf_do_recycle(int fstb_active)
{
	unsigned long long now_ts = xgf_get_time();
	long long diff, check_period, recycle_period;
	struct xgf_render *r_iter;
	struct xgff_frame *rr_iter;
	struct hlist_node *r_t;

	/* over 1 seconds since last check2recycle */
	check_period = NSEC_PER_SEC;
	recycle_period = NSEC_PER_SEC >> 1;
	diff = (long long)now_ts - (long long)last_check2recycle_ts;

	xgf_trace("xgf_do_recycle now:%llu last_check:%llu fstb_act:%d",
		now_ts, last_check2recycle_ts, fstb_active);

	xgf_lock(__func__);

	if (!fstb_active) {
		xgf_reset_all_renders();
		xgff_reset_all_renders();
		goto out;
	}

	if (xgf_prev_dep_frames != xgf_dep_frames) {
		if (xgf_dep_frames < XGF_DEP_FRAMES_MIN
			|| xgf_dep_frames > XGF_DEP_FRAMES_MAX)
			xgf_dep_frames = XGF_DEP_FRAMES_MIN;
		xgf_prev_dep_frames = xgf_dep_frames;
		xgf_reset_all_renders();
		xgff_reset_all_renders();
		goto out;
	}

	if (diff < 0LL || diff < check_period)
		goto done;

	hlist_for_each_entry_safe(r_iter, r_t, &xgf_renders, hlist) {
		diff = now_ts - r_iter->queue.end_ts;
		if (diff >= check_period)
			xgf_reset_render(r_iter);
	}

out:
	last_check2recycle_ts = now_ts;
done:
	xgf_unlock(__func__);

	mutex_lock(&xgff_frames_lock);
	hlist_for_each_entry_safe(rr_iter, r_t, &xgff_frames, hlist) {
		diff = now_ts - rr_iter->ts;
		if (diff >= check_period)
			xgff_reset_render(rr_iter);
	}
	mutex_unlock(&xgff_frames_lock);
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

	if (!xgf_log_trace_enable)
		return;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 1024))
		log[1023] = '\0';
	va_end(args);
	trace_printk(log);
}

static void xgf_print_critical_path_info(struct xgf_render *r)
{
	char total_pid_list[1024] = {"\0"};
	char pid[20] = {"\0"};
	int i, j;
	int overflow = 0;
	int len = 0;
	struct xgf_render_dep *xrd;
	struct xgf_dep_task *xdt;

	if (!xgf_log_trace_enable)
		return;

	if (r->xrd_arr_idx < 0 || r->xrd_arr_idx > MAX_DEP_PATH_NUM)
		goto error;

	for (i = 0; i < r->xrd_arr_idx; i++) {
		xrd = &r->xrd_arr[i];

		if (strlen(total_pid_list) == 0)
			len = snprintf(pid, sizeof(pid), "%dth", i+1);
		else
			len = snprintf(pid, sizeof(pid), "|%dth", i+1);
		if (len < 0 || len >= sizeof(pid))
			goto error;

		overflow = 0;
		xgf_strcat(total_pid_list, pid,
			sizeof(total_pid_list), &overflow);
		if (overflow)
			goto out;

		if (xrd->xdt_arr_idx < 0 || xrd->xdt_arr_idx > MAX_DEP_TASK_NUM)
			goto error;

		for (j = 0; j < xrd->xdt_arr_idx; j++) {
			xdt = &xrd->xdt_arr[j];

			len = snprintf(pid, sizeof(pid), ",%d", xdt->tid);

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
		xgf_log_trace("[xgf][%d][0x%llx] | (of) %s",
		r->render, r->bufID, total_pid_list);
	else
		xgf_log_trace("[xgf][%d][0x%llx] | %s",
		r->render, r->bufID, total_pid_list);

	return;

error:
	xgf_log_trace("[xgf][%d][0x%llx] | %s error",
		r->render, r->bufID, __func__);
	return;
}

static int xgf_enter_est_runtime(int rpid, struct xgf_render *render,
	unsigned long long *runtime, unsigned long long ts)
{
	int ret;


	if (xgf_est_runtime_fp)
		ret = xgf_est_runtime_fp(rpid, render, runtime, ts);
	else
		ret = -ENOENT;

	return ret;
}

static int xgff_enter_est_runtime(int rpid, struct xgf_render *render,
	unsigned long long *runtime, unsigned long long ts)
{
	int ret;

	if (xgff_est_runtime_fp)
		ret = xgff_est_runtime_fp(rpid, render, runtime, ts);
	else
		ret = -ENOENT;

	return ret;
}

int fpsgo_fstb2xgf_get_target_fps(int pid, unsigned long long bufID,
	int *target_fps_margin, unsigned long long cur_queue_end_ts,
	int eara_is_active)
{
	int target_fps;

	mutex_lock(&fstb_ko_lock);

	if (fpsgo_xgf2ko_calculate_target_fps_fp)
		target_fps = fpsgo_xgf2ko_calculate_target_fps_fp(pid, bufID,
			target_fps_margin, cur_queue_end_ts, eara_is_active);
	else
		target_fps = -ENOENT;

	mutex_unlock(&fstb_ko_lock);

	return target_fps;
}

int fpsgo_fstb2xgf_notify_recycle(int pid, unsigned long long bufID)
{
	int ret = 1;

	mutex_lock(&fstb_ko_lock);

	if (fpsgo_xgf2ko_do_recycle_fp)
		fpsgo_xgf2ko_do_recycle_fp(pid, bufID);
	else
		ret = -ENOENT;

	mutex_unlock(&fstb_ko_lock);

	return ret;
}

void xgf_get_runtime(pid_t tid, u64 *runtime)
{
	struct task_struct *p;

	if (unlikely(!tid))
		return;

	rcu_read_lock();
	p = find_task_by_vpid(tid);
	if (!p) {
		xgf_trace(" %5d not found to erase", tid);
		rcu_read_unlock();
		return;
	}
	get_task_struct(p);
	rcu_read_unlock();

	*runtime = (u64)fpsgo_task_sched_runtime(p);
	put_task_struct(p);
}
EXPORT_SYMBOL(xgf_get_runtime);

int xgf_get_logical_tid(int rpid, int tgid, int *l_tid,
	unsigned long long prev_ts, unsigned long long last_ts)
{
	int max_tid = -1;
	unsigned long long tmp_runtime, max_runtime = 0;
	struct task_struct *gtsk, *sib;

	if (last_ts - prev_ts < NSEC_PER_SEC)
		return 0;

	rcu_read_lock();
	gtsk = find_task_by_vpid(tgid);
	if (gtsk) {
		get_task_struct(gtsk);
		list_for_each_entry(sib, &gtsk->thread_group, thread_group) {
			tmp_runtime = 0;

			get_task_struct(sib);

			if (sib->pid == rpid) {
				put_task_struct(sib);
				continue;
			}

			tmp_runtime = (u64)fpsgo_task_sched_runtime(sib);
			if (tmp_runtime > max_runtime) {
				max_runtime = tmp_runtime;
				max_tid = sib->pid;
			}

			put_task_struct(sib);
		}
		put_task_struct(gtsk);
	}
	rcu_read_unlock();

	if (max_tid > 0 && max_runtime > 0)
		*l_tid = max_tid;
	else
		*l_tid = -1;

	return 1;
}
EXPORT_SYMBOL(xgf_get_logical_tid);

static int xgf_get_spid(struct xgf_render *render)
{
	struct rb_root *r;
	struct rb_node *rbn;
	struct xgf_dep *iter;
	int len, ret = 0;
	unsigned long long now_ts = xgf_get_time();
	long long diff, scan_period;
	unsigned long long spid_runtime, t_spid_runtime;

	if (xgf_spid_ck_period > NSEC_PER_SEC || xgf_spid_ck_period < 0)
		xgf_spid_ck_period = NSEC_PER_SEC;

	scan_period = xgf_spid_ck_period;

	diff = (long long)now_ts - (long long)last_update2spid_ts;

	if (diff < 0LL || diff < scan_period)
		return -1;

	/* reset and build spid list*/
	if (xgf_cfg_spid) {
		xgf_render_reset_wspid_list(render);
		xgf_render_setup_wspid_list(render);
	}

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

	spid_runtime = 0;

	if (render->spid && xgf_sp_name_id)
		xgf_get_runtime(render->spid, &spid_runtime);

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
				if (xgf_sp_name_id) {
					t_spid_runtime = (u64)fpsgo_task_sched_runtime(tsk);
					if (t_spid_runtime > spid_runtime)
						ret = task_pid_nr(tsk);
				} else
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

	if (!ret && render->spid && xgf_sp_name_id && spid_runtime)
		ret = render->spid;

	return ret;
}

int fpsgo_comp2xgf_qudeq_notify(int rpid, unsigned long long bufID, int cmd,
	unsigned long long *run_time, unsigned long long ts, int skip)
{
	int ret = XGF_NOTIFY_OK;
	struct xgf_render *r, **rrender;
	struct xgf_policy_cmd *policy;
	unsigned long long raw_runtime = 0;
	int new_spid;
	unsigned long long t_dequeue_time = 0;
	int do_extra_sub = 0;
	unsigned long long q2q_time = 0;
	unsigned long long tmp_runtime = 0;
	unsigned long long delta = 0;
	unsigned long long time_scale = 1000;
	long long ema2_offset = 0;
	char buf[256] = {0};

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
		if (xgf_get_render(rpid, bufID, rrender, 0, ts)) {
			ret = XGF_THREAD_NOT_FOUND;
			goto qudeq_notify_err;
		}
		r->queue.start_ts = ts;
		xgf_render_dep_reset(r);
		break;

	case XGF_QUEUE_END:
		rrender = &r;
		if (xgf_get_render(rpid, bufID, rrender, 1, ts)) {
			ret = XGF_THREAD_NOT_FOUND;
			goto qudeq_notify_err;
		}

		q2q_time = ts - r->queue.end_ts;
		r->queue.end_ts = ts;
		cur_xgf_extra_sub = xgf_extra_sub;

		if (skip)
			goto queue_end_skip;

		new_spid = xgf_get_spid(r);
		if (new_spid != -1) {
			xgf_trace("xgf spid:%d => %d", r->spid, new_spid);
			r->spid = new_spid;
		}

		t_dequeue_time = r->deque.end_ts - r->deque.start_ts;
		if (r->deque.start_ts && r->deque.end_ts
			&& (r->deque.end_ts > r->deque.start_ts)
			&& (t_dequeue_time > 2500000)
			&& !cur_xgf_extra_sub && !xgf_force_no_extra_sub) {
			do_extra_sub = 1;
			cur_xgf_extra_sub = 1;
			xgf_trace("xgf extra_sub:%d => %llu", rpid, t_dequeue_time);
		}

		ret = xgf_enter_est_runtime(rpid, r, &raw_runtime, ts);
		fpsgo_systrace_c_fbt(rpid, bufID, ret, "xgf_ret");

		if (do_extra_sub)
			cur_xgf_extra_sub = 0;

		if (!raw_runtime)
			*run_time = raw_runtime;
		else {
			xgf_trace("xgf raw_runtime:%llu q2qtime:%llu", raw_runtime, q2q_time);
			/* error handling for raw_t_cpu */
			if (q2q_time && raw_runtime > q2q_time)
				raw_runtime = q2q_time;

			mutex_lock(&xgf_policy_cmd_lock);
			policy = xgf_get_policy_cmd(r->parent, r->ema2_enable, ts, 0);
			if (xgf_ema2_enable)
				r->ema2_enable = 1;
			else if (!policy)
				r->ema2_enable = 0;
			else {
				policy->ts = ts;
				r->ema2_enable = policy->ema2_enable;
			}
			mutex_unlock(&xgf_policy_cmd_lock);

			if (r->ema2_enable) {
				if (!r->ema2_pt)
					r->ema2_pt = xgf_ema2_get_pred();

				//calculate mse
				delta = abs((long long)(raw_runtime/time_scale) -
					(long long)(r->ema_runtime/time_scale));
				xgf_ema_mse += delta*delta;

				if (r->ema2_pt)
					delta = abs((long long)(raw_runtime/time_scale) -
						(long long)(r->ema2_pt->xt_last));
				xgf_ema2_mse += delta*delta;

				//predict next frame
				r->ema_runtime = xgf_ema_cal(raw_runtime, r->ema_runtime);
				if (xgf_ema2_predict_fp && r->ema2_pt) {
					ema2_offset = raw_runtime - r->ema2_pt->xt_last*time_scale;
					tmp_runtime =
						xgf_ema2_predict_fp(r->ema2_pt,
							raw_runtime/time_scale)
						* time_scale;
					xgf_ema2_dump_info_frames(r->ema2_pt, buf);
					xgf_trace("xgf ema2 sts t:%d err:%d ei:%d eo:%d",
						r->ema2_pt->t, r->ema2_pt->err_code,
						r->ema2_pt->invalid_input_cnt,
						r->ema2_pt->invalid_negative_output_cnt);
					xgf_trace("xgf ema2 sts_ext i:%d-%d of:%lld L:%s",
						r->ema2_pt->rmsprop_initialized,
						r->ema2_pt->x_record_initialized,
						ema2_offset,
						buf);
					xgf_trace("xgf ema2 raw_t:%llu alpha:%llu ema2:%lld",
						raw_runtime, r->ema_runtime, tmp_runtime);

					if (tmp_runtime <= 0)
						tmp_runtime = r->ema_runtime;

					if (r->ema2_pt->acc_idx == 1) {
						xgf_ema2_dump_rho(r->ema2_pt, buf);
						xgf_trace("xgf ema2 oneshot:%d rho-L:%s ",
							r->ema2_pt->ar_coeff_valid, buf);
						xgf_ema_mse = xgf_ema_mse/
							(r->ema2_pt->ar_coeff_frames*
								time_scale*time_scale);
						xgf_ema2_mse = xgf_ema2_mse/
							(r->ema2_pt->ar_coeff_frames*
								time_scale*time_scale);

						xgf_trace("xgf ema2 mse_alpha:%lld mse_ema2:%lld",
							xgf_ema_mse, xgf_ema2_mse);
						fpsgo_systrace_c_fbt(rpid, bufID, xgf_ema_mse,
							"mse_alpha");
						fpsgo_systrace_c_fbt(rpid, bufID, xgf_ema2_mse,
							"mse_ema2");
						xgf_ema_mse = 0;
						xgf_ema2_mse = 0;
					}

					*run_time = tmp_runtime;
				} else
					*run_time = r->ema_runtime;
			} else {
				r->ema_runtime = xgf_ema_cal(raw_runtime, r->ema_runtime);
				*run_time = r->ema_runtime;
			}
		}

		r->raw_runtime = raw_runtime;
		fpsgo_systrace_c_fbt(rpid, bufID, raw_runtime, "raw_t_cpu");
		if (r->ema2_enable)
			fpsgo_systrace_c_fbt(rpid, bufID, tmp_runtime, "ema2_t_cpu");

		xgf_print_critical_path_info(r);
queue_end_skip:
		r->prev_queue_end_ts = ts;
		break;

	case XGF_DEQUEUE_START:
		rrender = &r;
		if (xgf_get_render(rpid, bufID, rrender, 0, ts)) {
			ret = XGF_THREAD_NOT_FOUND;
			goto qudeq_notify_err;
		}
		r->deque.start_ts = ts;
		break;

	case XGF_DEQUEUE_END:
		rrender = &r;
		if (xgf_get_render(rpid, bufID, rrender, 0, ts)) {
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

void xgf_set_timer_info(int pid, unsigned long long bufID, int hrtimer_pid, int hrtimer_flag,
	unsigned long long hrtimer_ts, unsigned long long prev_queue_end_ts)
{
	int diff;

	if (hrtimer_ts >= prev_queue_end_ts)
		diff = (int)(hrtimer_ts - prev_queue_end_ts);
	else {
		diff = (int)(prev_queue_end_ts - hrtimer_ts);
		diff *= -1;
	}

	fpsgo_systrace_c_fbt(pid, bufID, hrtimer_pid, "ctrl_fps_tid");
	fpsgo_systrace_c_fbt(pid, bufID, hrtimer_flag, "ctrl_fps_flag");
	fpsgo_systrace_c_xgf(pid, bufID, diff, "ctrl_fps_ts");
}
EXPORT_SYMBOL(xgf_set_timer_info);

static int xgff_find_frame(pid_t tID, unsigned long long bufID,
	unsigned long long  frameID, struct xgff_frame **ret)
{
	struct xgff_frame *iter;

	hlist_for_each_entry(iter, &xgff_frames, hlist) {
		if (iter->tid != tID)
			continue;

		if (iter->bufid != bufID)
			continue;

		if (iter->frameid != frameID)
			continue;

		if (ret)
			*ret = iter;
		return 0;
	}

	return -EINVAL;
}

static int xgff_new_frame(pid_t tID, unsigned long long bufID,
	unsigned long long frameID, struct xgff_frame **ret, int force,
	unsigned long long ts)
{
	struct xgff_frame *iter;
	struct xgf_render *xriter;
	struct task_struct *tsk;

	iter = xgf_alloc(sizeof(*iter), XGFF_FRAME);

	if (!iter)
		return -ENOMEM;

	rcu_read_lock();
	tsk = find_task_by_vpid(tID);
	if (tsk)
		get_task_struct(tsk);
	rcu_read_unlock();

	if (!tsk) {
		xgf_free(iter, XGFF_FRAME);
		return -EINVAL;
	}

	// init xgff_frame
	iter->parent = tsk->tgid;
	put_task_struct(tsk);

	iter->tid = tID;
	iter->bufid = bufID;
	iter->frameid = frameID;
	iter->ts = ts;

	xriter = &iter->xgfrender;
	{
		xriter->parent = iter->parent;
		xriter->render = iter->tid;
		xriter->bufID = iter->bufid;
		xriter->xrd_arr_idx = 0;
		xriter->l_num = 0;
		xriter->r_num = 0;
		xriter->prev_index = 0;
		xriter->prev_ts = 0;
		xriter->curr_index = 0;
		xriter->curr_ts = 0;
		xriter->event_count = 0;
		xriter->prev_queue_end_ts = ts;
		xriter->frame_count = 0;
		xriter->deque.start_ts = 0;
		xriter->deque.end_ts = 0;
		xriter->queue.start_ts = 0;
		xriter->queue.end_ts = 0;
		xriter->ema_runtime = 0;
		xriter->spid = 0;
		xriter->dep_frames = xgf_prev_dep_frames;
		xriter->ema2_enable = 0;
		xriter->ema2_pt = NULL;
	}

	if (ret)
		*ret = iter;
	return 0;
}


static void switch_xgff_mips_exp_enable(int active)
{
	if (active == is_xgff_mips_exp_enable)
		return;
	mutex_lock(&xgff_frames_lock);
	is_xgff_mips_exp_enable = active;
	mutex_unlock(&xgff_frames_lock);
}

static int xgff_get_start_runtime(int rpid, unsigned long long queueid,
	unsigned int deplist_size, unsigned int *deplist,
	struct xgff_runtime *dep_runtime, int *count_dep_runtime,
	unsigned long frameid)
{
	int ret = XGF_NOTIFY_OK;
	int i;
	unsigned long long runtime = 0;
	struct task_struct *p;

	*count_dep_runtime = 0;
	if (!dep_runtime || !count_dep_runtime) {
		ret = XGF_DEP_LIST_ERR;
		goto out;
	}

	if (!deplist && deplist_size > 0) {
		ret = XGF_DEP_LIST_ERR;
		goto out;
	}

	for (i = 0; i < deplist_size; i++) {
		rcu_read_lock();
		p = find_task_by_vpid(deplist[i]);
		if (!p) {
			rcu_read_unlock();
		} else {
			get_task_struct(p);
			rcu_read_unlock();
			runtime = fpsgo_task_sched_runtime(p);
			put_task_struct(p);

			dep_runtime[*count_dep_runtime].pid = deplist[i];
			dep_runtime[*count_dep_runtime].loading = runtime;
			(*count_dep_runtime)++;
			xgf_trace("[XGFF] start_dep_runtime: %llu, pid: %d", runtime, deplist[i]);
		}
	}

out:
	xgf_trace("[XGFF][%s] ret=%d, frame_id=%lu, count_dep=%d", __func__, ret,
		frameid, *count_dep_runtime);
	return ret;
}

static int _xgff_frame_start(
		unsigned int tid,
		unsigned long long queueid,
		unsigned long long frameid,
		unsigned int *pdeplistsize,
		unsigned int *pdeplist,
		unsigned long long ts)
{
	int ret = XGF_NOTIFY_OK, is_start_dep;
	struct xgff_frame *r, **rframe;
	// ToDo, check if xgf is enabled

	is_start_dep = 0;

	mutex_lock(&xgff_frames_lock);
	rframe = &r;

	if (!xgff_update_start_prev_index_fp) {
		ret = XGF_PARAM_ERR;
		goto qudeq_notify_err;
	}

	if (xgff_find_frame(tid, queueid, frameid, rframe) == 0) {
		ret = XGF_PARAM_ERR;
		goto qudeq_notify_err;
	}

	if (xgff_new_frame(tid, queueid, frameid, rframe, 1, ts)) {
		ret = XGF_PARAM_ERR;
		goto qudeq_notify_err;
	}

	r->ploading = fbt_xgff_list_loading_add(tid, queueid, ts);

	// ToDo, check tid is related to the caller
	// record index and timestamp
	xgff_update_start_prev_index_fp(&r->xgfrender);

	hlist_add_head(&r->hlist, &xgff_frames);

	if (!pdeplist || !pdeplistsize) {
		xgf_trace("[%s] !pdeplist || !pdeplistsize", __func__);
		is_start_dep = 0;
		r->is_start_dep = 0;
		ret = XGF_PARAM_ERR;
		goto qudeq_notify_err;
	}

	is_start_dep = 1;
	r->is_start_dep = 1;
	ret = xgff_get_start_runtime(tid, queueid, *pdeplistsize, pdeplist,
		r->dep_runtime, &r->count_dep_runtime, r->frameid);

qudeq_notify_err:
	xgf_trace("xgff result:%d at rpid:%d cmd:xgff_frame_start", ret, tid);
	xgf_trace("[XGFF] tid=%d, queueid=%llu, frameid=%llu, rframe=%lu, is_start_dep=%d",
		tid, queueid, frameid, rframe, is_start_dep);

	mutex_unlock(&xgff_frames_lock);

	return ret;
}

static int xgff_enter_est_runtime_dep_from_start(int rpid, unsigned long long queueid,
	struct xgff_runtime *dep_runtime, int count_dep_runtime, unsigned long long *runtime,
	unsigned long frameid)
{
	int ret = XGF_NOTIFY_OK;
	int i, dep_runtime_size;
	unsigned long long runtime_tmp, total_runtime = 0;
	struct task_struct *p;

	if (!dep_runtime) {
		ret = XGF_DEP_LIST_ERR;
		goto out;
	}

	dep_runtime_size = count_dep_runtime;

	for (i = 0; i < dep_runtime_size; i++) {
		rcu_read_lock();
		p = find_task_by_vpid(dep_runtime[i].pid);
		if (!p) {
			rcu_read_unlock();
			xgf_trace("[XGFF] Error: Can't get runtime of pid=%d at frame end.",
				dep_runtime[i].pid);
			ret = XGF_DEP_LIST_ERR;
		} else {
			get_task_struct(p);
			rcu_read_unlock();
			runtime_tmp = fpsgo_task_sched_runtime(p);
			put_task_struct(p);
			if (runtime_tmp && dep_runtime[i].loading &&
				runtime_tmp >= dep_runtime[i].loading)
				dep_runtime[i].loading = runtime_tmp - dep_runtime[i].loading;
			else
				dep_runtime[i].loading = 0;

			total_runtime += dep_runtime[i].loading;
			xgf_trace("[XGFF] dep_runtime: %llu, now: %llu, pid: %d",
				dep_runtime[i].loading, runtime_tmp, dep_runtime[i].pid);
		}
	}
	*runtime = total_runtime;
out:
	xgf_trace("[XGFF][%s] ret=%d, frame_id=%lu, count_dep=%d", __func__, ret,
		frameid, count_dep_runtime);
	return ret;
}

void print_dep(unsigned int deplist_size, unsigned int *deplist)
{
	char *dep_str = NULL;
	char temp[7] = {"\0"};
	int i = 0;

	dep_str = kcalloc(deplist_size + 1, 7 * sizeof(char),
				GFP_KERNEL);
	if (!dep_str)
		return;
	for (i = 0; i < deplist_size; i++) {
		if (strlen(dep_str) == 0)
			snprintf(temp, sizeof(temp), "%d", deplist[i]);
		else
			snprintf(temp, sizeof(temp), ",%d", deplist[i]);

		if (strlen(dep_str) + strlen(temp) < 256)
			strncat(dep_str, temp, strlen(temp));
	}
	xgf_trace("[XGFF] EXP_deplist: %s", dep_str);
	kfree(dep_str);
}

static int _xgff_frame_end(
		unsigned int tid,
		unsigned long long queueid,
		unsigned long long frameid,
		unsigned long long *cputime,
		unsigned int *area,
		unsigned int *pdeplistsize,
		unsigned int *pdeplist,
		unsigned long long ts)
{
	int ret = XGF_NOTIFY_OK, ret_exp = XGF_NOTIFY_OK;
	struct xgff_frame *r = NULL, **rframe = NULL;
	int iscancel = 0;
	unsigned long long raw_runtime = 0, raw_runtime_exp = 0;
	unsigned int newdepsize = 0, deplist_size_exp = 0;
	unsigned int deplist_exp[XGF_DEP_FRAMES_MAX];

	if (pdeplistsize && *pdeplistsize == 0)
		iscancel = 1;

	// ToDo, check if xgf is enabled
	// ToDo, check tid is related to the caller

	mutex_lock(&xgff_frames_lock);
	rframe = &r;
	if (xgff_find_frame(tid, queueid, frameid, rframe)) {
		ret = ret_exp = XGF_THREAD_NOT_FOUND;
		mutex_unlock(&xgff_frames_lock);
		goto qudeq_notify_err;
	}

	// ToDo, check tid is related to the caller

	// remove this frame
	hlist_del(&r->hlist);

	mutex_unlock(&xgff_frames_lock);

	if (!iscancel) {

		*area = fbt_xgff_get_loading_by_cluster(&(r->ploading), ts, 0, 0, NULL);

		// Sum up all PELT sched_runtime in deplist to reduce MIPS.
		if (r->is_start_dep) {
			ret = xgff_enter_est_runtime_dep_from_start(tid, queueid, r->dep_runtime,
				r->count_dep_runtime, &raw_runtime, r->frameid);
			fpsgo_systrace_c_fbt(tid, queueid, raw_runtime, "xgff_runtime");
		} else {
			r->xgfrender.render = tid;
			ret = xgff_enter_est_runtime(tid, &r->xgfrender, &raw_runtime, ts);
		}
		*cputime = raw_runtime;

		// post handle est time
		if (is_xgff_mips_exp_enable && r->is_start_dep) {
			deplist_size_exp = XGF_DEP_FRAMES_MAX;

			r->xgfrender.render = tid;

			ret_exp = xgff_enter_est_runtime(tid, &r->xgfrender, &raw_runtime_exp, ts);

			// copy deplist
			if (ret_exp != XGF_SLPTIME_OK) {
				deplist_size_exp = 0;
			} else {
				newdepsize = xgff_get_dep_list(tid, deplist_size_exp,
							deplist_exp, &r->xgfrender);
				print_dep(deplist_size_exp, deplist_exp);
				ret_exp = XGF_NOTIFY_OK;
			}
			xgf_trace("[XGFF][EXP] xgf_ret: %d at rpid:%d, runtime:%llu", ret_exp, tid,
				raw_runtime_exp);
			fpsgo_systrace_c_fbt_debug(tid, queueid, raw_runtime_exp,
				"xgff_runtime_original");
		}
	}

	xgf_clean_deps_list(&r->xgfrender, INNER_DEPS);
	xgf_clean_deps_list(&r->xgfrender, OUTER_DEPS);
	xgf_clean_deps_list(&r->xgfrender, PREVI_DEPS);
	xgf_render_dep_reset(&r->xgfrender);

	xgf_free(r, XGFF_FRAME);

qudeq_notify_err:
	xgf_trace("[XGFF] non_xgf_ret: %d at rpid:%d, runtime:%llu", ret, tid, raw_runtime);

	return ret;
}

static int xgff_frame_startend(unsigned int startend,
		unsigned int tid,
		unsigned long long queueid,
		unsigned long long frameid,
		unsigned long long *cputime,
		unsigned int *area,
		unsigned int *pdeplistsize,
		unsigned int *pdeplist)
{
	unsigned long long cur_ts;

	if (!fpsgo_is_enable())
		return XGF_DISABLE;

	cur_ts = fpsgo_get_time();

	fpsgo_systrace_c_xgf(tid, queueid, startend, "xgffs_queueid");
	fpsgo_systrace_c_xgf(tid, queueid, frameid, "xgffs_frameid");

	if (startend)
		return _xgff_frame_start(tid, queueid, frameid, pdeplistsize, pdeplist, cur_ts);

	return _xgff_frame_end(tid, queueid, frameid, cputime, area, pdeplistsize,
				pdeplist, cur_ts);
}

static void xgff_frame_getdeplist_maxsize(unsigned int *pdeplistsize)
{
	if (!pdeplistsize)
		return;
	*pdeplistsize = XGF_DEP_FRAMES_MAX;
}

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
				"[%d][0x%llx] | I tid:%d idx:%d\n",
				r_iter->render, r_iter->bufID,
				iter->tid, iter->frame_idx);
			pos += length;
		}

		r = &r_iter->out_deps_list;
		for (n = rb_first(r); n != NULL; n = rb_next(n)) {
			iter = rb_entry(n, struct xgf_dep, rb_node);

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"[%d][0x%llx] | O tid:%d idx:%d\n",
				 r_iter->render, r_iter->bufID,
				 iter->tid, iter->frame_idx);
			pos += length;
		}

		r = &r_iter->prev_deps_list;
		for (n = rb_first(r); n != NULL; n = rb_next(n)) {
			iter = rb_entry(n, struct xgf_dep, rb_node);
			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"[%d][0x%llx] | P tid:%d idx:%d\n",
				 r_iter->render, r_iter->bufID,
				 iter->tid, iter->frame_idx);
			pos += length;
		}
	}

	xgf_unlock(__func__);
	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static KOBJ_ATTR_RO(deplist);

static ssize_t runtime_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct xgf_render *r_iter;
	struct hlist_node *r_tmp;
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int pos = 0;
	int length;

	xgf_lock(__func__);

	hlist_for_each_entry_safe(r_iter, r_tmp, &xgf_renders, hlist) {
		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"rtid:%d bid:0x%llx cpu_runtime:%d (%d)\n",
			r_iter->render, r_iter->bufID,
			r_iter->ema_runtime,
			r_iter->raw_runtime);
		pos += length;
	}

	xgf_unlock(__func__);
	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static KOBJ_ATTR_RO(runtime);

static ssize_t xgf_status_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct xgf_render *r_iter;
	struct xgf_render_dep *xrd;
	struct hlist_node *h1;
	struct rb_root *rbr;
	struct rb_node *rbn;
	struct xgff_frame *rr_iter;
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int pos = 0, length = 0, i, j;
	int num[7] = {0};

	xgf_lock(__func__);
	num[0] = 0;
	hlist_for_each_entry_safe(r_iter, h1, &xgf_renders, hlist) {
		for (i = 1; i <= 5; i++)
			num[i] = 0;

		if (r_iter->xrd_arr_idx < 0 || r_iter->xrd_arr_idx > MAX_DEP_PATH_NUM)
			continue;
		for (i = 0; i < r_iter->xrd_arr_idx; i++) {
			xrd = &r_iter->xrd_arr[i];
			if (xrd->xdt_arr_idx < 0 || xrd->xdt_arr_idx > MAX_DEP_TASK_NUM)
				continue;
			for (j = 0; j < xrd->xdt_arr_idx; j++)
				num[2]++;
			num[1]++;
		}

		rbr = &r_iter->deps_list;
		for (rbn = rb_first(rbr); rbn != NULL; rbn = rb_next(rbn))
			num[3]++;

		rbr = &r_iter->out_deps_list;
		for (rbn = rb_first(rbr); rbn != NULL; rbn = rb_next(rbn))
			num[4]++;

		rbr = &r_iter->prev_deps_list;
		for (rbn = rb_first(rbr); rbn != NULL; rbn = rb_next(rbn))
			num[5]++;

		num[0]++;

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%dth [%d][0x%llx] r_sect:%d r_pid:%d dep:(%d,%d,%d)\n",
			 num[0], r_iter->render, r_iter->bufID, num[1], num[2],
			 num[3], num[4], num[5]);
		pos += length;
	}
	xgf_unlock(__func__);

	mutex_lock(&xgff_frames_lock);
	num[6] = 0;
	hlist_for_each_entry_safe(rr_iter, h1, &xgff_frames, hlist) {
		num[6]++;

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%dth [%d][0x%llx]\n",
			 num[6], rr_iter->tid, rr_iter->bufid);
		pos += length;
	}
	mutex_unlock(&xgff_frames_lock);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static KOBJ_ATTR_RO(xgf_status);

static ssize_t xgf_ema2_enable_by_pid_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int pos = 0;
	int length;
	struct xgf_policy_cmd *iter;
	struct rb_root *rbr;
	struct rb_node *rbn;

	mutex_lock(&xgf_policy_cmd_lock);

	rbr = &xgf_policy_cmd_tree;
	for (rbn = rb_first(rbr); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct xgf_policy_cmd, rb_node);
		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"tgid:%d\tema2_enable:%d\tts:%llu\n",
			iter->tgid, iter->ema2_enable, iter->ts);
		pos += length;
	}

	mutex_unlock(&xgf_policy_cmd_lock);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t xgf_ema2_enable_by_pid_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int tgid;
	int ema2_enable;
	unsigned long long ts = fpsgo_get_time();
	struct xgf_policy_cmd *iter;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (sscanf(acBuffer, "%d %d", &tgid, &ema2_enable) == 2) {
				mutex_lock(&xgf_policy_cmd_lock);
				if (ema2_enable > 0)
					iter = xgf_get_policy_cmd(tgid, !!ema2_enable, ts, 1);
				else {
					iter = xgf_get_policy_cmd(tgid, ema2_enable, ts, 0);
					if (iter) {
						rb_erase(&iter->rb_node, &xgf_policy_cmd_tree);
						kfree(iter);
					}
				}
				mutex_unlock(&xgf_policy_cmd_lock);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(xgf_ema2_enable_by_pid);

atomic_t xgf_ko_enable;
EXPORT_SYMBOL(xgf_ko_enable);
struct fpsgo_trace_event *xgf_event_buffer;
EXPORT_SYMBOL(xgf_event_buffer);
atomic_t xgf_event_buffer_idx;
EXPORT_SYMBOL(xgf_event_buffer_idx);
int xgf_event_buffer_size;
EXPORT_SYMBOL(xgf_event_buffer_size);
struct fpsgo_trace_event *fstb_event_buffer;
EXPORT_SYMBOL(fstb_event_buffer);
atomic_t fstb_event_buffer_idx;
EXPORT_SYMBOL(fstb_event_buffer_idx);
int fstb_event_buffer_size;
EXPORT_SYMBOL(fstb_event_buffer_size);

#define MAX_XGF_EVENT_NUM xgf_event_buffer_size
#define MAX_FSTB_EVENT_NUM fstb_event_buffer_size

static void xgf_buffer_record_irq_waking_switch(int cpu, int event,
	int data, int note, int state, unsigned long long ts)
{
	int index;
	struct fpsgo_trace_event *fte;

	if (!atomic_read(&xgf_ko_enable))
		return;

Reget:
	index = atomic_inc_return(&xgf_event_buffer_idx);

	if (unlikely(index <= 0)) {
		atomic_set(&xgf_event_buffer_idx, 0);
		return;
	}

	if (unlikely(index > (MAX_XGF_EVENT_NUM + (xgf_nr_cpus << 1)))) {
		atomic_set(&xgf_event_buffer_idx, 0);
		return;
	}

	if (unlikely(index == MAX_XGF_EVENT_NUM))
		atomic_set(&xgf_event_buffer_idx, 0);
	else if (unlikely(index > MAX_XGF_EVENT_NUM))
		goto Reget;

	index -= 1;

	fte = &xgf_event_buffer[index];
	fte->ts = ts;
	fte->cpu = cpu;
	fte->event = event;
	fte->note = note;
	fte->state = state;
	fte->pid = data;
}

static void fstb_buffer_record_waking_timer(int cpu, int event,
	int data, int note, unsigned long long ts)
{
	int index;
	struct fpsgo_trace_event *fte;

	if (!atomic_read(&xgf_ko_enable))
		return;

Reget:
	index = atomic_inc_return(&fstb_event_buffer_idx);

	if (unlikely(index <= 0)) {
		atomic_set(&fstb_event_buffer_idx, 0);
		return;
	}

	if (unlikely(index > (MAX_FSTB_EVENT_NUM + (xgf_nr_cpus << 1)))) {
		atomic_set(&fstb_event_buffer_idx, 0);
		return;
	}

	if (unlikely(index == MAX_FSTB_EVENT_NUM))
		atomic_set(&fstb_event_buffer_idx, 0);
	else if (unlikely(index > MAX_FSTB_EVENT_NUM))
		goto Reget;

	index -= 1;

	fte = &fstb_event_buffer[index];
	fte->ts = ts;
	fte->cpu = cpu;
	fte->event = event;
	fte->note = note;
	fte->pid = data;
	fte->state = 0;
}

static void xgf_irq_handler_entry_tracer(void *ignore,
					int irqnr,
					struct irqaction *irq_action)
{
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	xgf_buffer_record_irq_waking_switch(c_wake_cpu, IRQ_ENTRY,
		0, c_pid, irqnr, ts);
}

static void xgf_irq_handler_exit_tracer(void *ignore,
					int irqnr,
					struct irqaction *irq_action,
					int ret)
{
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	xgf_buffer_record_irq_waking_switch(c_wake_cpu, IRQ_EXIT,
		0, c_pid, irqnr, ts);
}

static inline long xgf_trace_sched_switch_state(bool preempt,
				struct task_struct *p)
{
	long state = 0;

	if (!p)
		goto out;

	state = xgf_get_task_state(p);

	if (preempt)
		state = TASK_RUNNING | TASK_STATE_MAX;

out:
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
	int prev_pid;
	int next_pid;

	if (!prev || !next)
		return;

	prev_pid = xgf_get_task_pid(prev);
	next_pid = xgf_get_task_pid(next);
	prev_state = xgf_trace_sched_switch_state(preempt, prev);
	temp_state = prev_state & (TASK_STATE_MAX-1);

	if (!temp_state)
		skip = 1;

	if (temp_state)
		xgf_buffer_record_irq_waking_switch(c_wake_cpu, SCHED_SWITCH,
			next_pid, prev_pid, 1, ts);
	else
		xgf_buffer_record_irq_waking_switch(c_wake_cpu, SCHED_SWITCH,
			next_pid, prev_pid, 0, ts);
}

static void xgf_sched_waking_tracer(void *ignore, struct task_struct *p)
{
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);
	int p_pid;

	if (!p)
		return;

	p_pid = xgf_get_task_pid(p);

	xgf_buffer_record_irq_waking_switch(c_wake_cpu, SCHED_WAKING,
		p_pid, c_pid, 512, ts);
	fstb_buffer_record_waking_timer(c_wake_cpu, SCHED_WAKING,
		p_pid, c_pid, ts);
}

static void xgf_hrtimer_expire_entry_tracer(void *ignore,
	struct hrtimer *hrtimer, ktime_t *now)
{
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	fstb_buffer_record_waking_timer(c_wake_cpu, HRTIMER_ENTRY,
		0, c_pid, ts);

}

static void xgf_hrtimer_expire_exit_tracer(void *ignore, struct hrtimer *hrtimer)
{
	unsigned long long ts = xgf_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	fstb_buffer_record_waking_timer(c_wake_cpu, HRTIMER_EXIT,
		0, c_pid, ts);
}

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool registered;
};

static struct tracepoints_table xgf_tracepoints[] = {
	{.name = "irq_handler_entry", .func = xgf_irq_handler_entry_tracer},
	{.name = "irq_handler_exit", .func = xgf_irq_handler_exit_tracer},
	{.name = "sched_switch", .func = xgf_sched_switch_tracer},
	{.name = "sched_waking", .func = xgf_sched_waking_tracer},
	{.name = "hrtimer_expire_entry", .func = xgf_hrtimer_expire_entry_tracer},
	{.name = "hrtimer_expire_exit", .func = xgf_hrtimer_expire_exit_tracer},
};

static void __nocfi xgf_tracing_register(void)
{
	int ret;

	xgf_nr_cpus = xgf_num_possible_cpus();

	/* xgf_irq_handler_entry_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[0].tp,
						xgf_tracepoints[0].func,  NULL);

	if (ret) {
		pr_info("irq trace: Couldn't activate tracepoint probe to irq_handler_entry\n");
		goto fail_reg_irq_handler_entry;
	}
	xgf_tracepoints[0].registered = true;

	/* xgf_irq_handler_exit_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[1].tp,
						xgf_tracepoints[1].func,  NULL);

	if (ret) {
		pr_info("irq trace: Couldn't activate tracepoint probe to irq_handler_exit\n");
		goto fail_reg_irq_handler_exit;
	}
	xgf_tracepoints[1].registered = true;

	/* xgf_sched_switch_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[2].tp,
						xgf_tracepoints[2].func,  NULL);

	if (ret) {
		pr_info("sched trace: Couldn't activate tracepoint probe to sched_switch\n");
		goto fail_reg_sched_switch;
	}
	xgf_tracepoints[2].registered = true;

	/* xgf_sched_waking_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[3].tp,
						xgf_tracepoints[3].func,  NULL);

	if (ret) {
		pr_info("sched trace: Couldn't activate tracepoint probe to sched_waking\n");
		goto fail_reg_sched_waking;
	}
	xgf_tracepoints[3].registered = true;

	/* xgf_hrtimer_expire_entry_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[4].tp,
						xgf_tracepoints[4].func,  NULL);

	if (ret) {
		pr_info("hrtimer trace: Couldn't activate tracepoint probe to hrtimer_expire_entry\n");
		goto fail_reg_hrtimer_expire_entry;
	}
	xgf_tracepoints[4].registered = true;

	/* xgf_hrtimer_expire_exit_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[5].tp,
						xgf_tracepoints[5].func,  NULL);

	if (ret) {
		pr_info("hrtimer trace: Couldn't activate tracepoint probe to hrtimer_expire_exit\n");
		goto fail_reg_hrtimer_expire_exit;
	}
	xgf_tracepoints[5].registered = true;

	atomic_set(&xgf_event_buffer_idx, 0);
	atomic_set(&fstb_event_buffer_idx, 0);
	return; /* successful registered all */

fail_reg_hrtimer_expire_exit:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[5].tp,
					xgf_tracepoints[5].func,  NULL);
	xgf_tracepoints[5].registered = false;
fail_reg_hrtimer_expire_entry:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[4].tp,
					xgf_tracepoints[4].func,  NULL);
	xgf_tracepoints[4].registered = false;
fail_reg_sched_waking:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[3].tp,
					xgf_tracepoints[3].func,  NULL);
	xgf_tracepoints[3].registered = false;
fail_reg_sched_switch:
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

	atomic_set(&xgf_ko_enable, 0);
	atomic_set(&xgf_event_buffer_idx, 0);
	atomic_set(&fstb_event_buffer_idx, 0);
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

	atomic_set(&xgf_ko_enable, 0);
	atomic_set(&xgf_event_buffer_idx, 0);
	atomic_set(&fstb_event_buffer_idx, 0);
}

static int xgf_stat_xchg(int xgf_enable)
{
	int ret = -1;

	if (xgf_enable) {
		xgf_tracing_register();
		ret = 1;
		atomic_set(&xgf_ko_enable, 1);
	} else {
		xgf_tracing_unregister();
		ret = 0;
		atomic_set(&xgf_ko_enable, 0);
	}

	return ret;
}

static void xgf_enter_state_xchg(int enable)
{
	int ret = 0;

	if (enable != 0 && enable != 1) {
		ret = -1;
		goto out;
	}

	ret = xgf_stat_xchg(enable);

out:
	xgf_trace("xgf k2ko xchg ret:%d enable:%d", ret, enable);
}

void fpsgo_ctrl2xgf_switch_xgf(int val)
{
	xgf_lock(__func__);
	if (val != xgf_enable) {
		xgf_enable = val;

		xgf_reset_all_renders();
		xgff_reset_all_renders();

		if (xgf_ko_is_ready())
			xgf_enter_state_xchg(xgf_enable);
	}

	xgf_unlock(__func__);
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

static void clean_xgf_tp(void)
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

int __init init_xgf_mm(void)
{
	xgf_render_cachep = kmem_cache_create("xgf_render",
		sizeof(struct xgf_render), 0, SLAB_HWCACHE_ALIGN, NULL);
	if (!xgf_render_cachep)
		goto err;
	xgf_dep_cachep = kmem_cache_create("xgf_dep",
		sizeof(struct xgf_dep), 0, SLAB_HWCACHE_ALIGN, NULL);
	if (!xgf_dep_cachep)
		goto err;
	xgf_runtime_sect_cachep = kmem_cache_create("xgf_runtime_sect",
		sizeof(struct xgf_runtime_sect), 0, SLAB_HWCACHE_ALIGN, NULL);
	if (!xgf_runtime_sect_cachep)
		goto err;
	xgf_spid_cachep = kmem_cache_create("xgf_spid",
		sizeof(struct xgf_spid), 0, SLAB_HWCACHE_ALIGN, NULL);
	if (!xgf_spid_cachep)
		goto err;
	xgff_frame_cachep = kmem_cache_create("xgff_frame",
		sizeof(struct xgff_frame), 0, SLAB_HWCACHE_ALIGN, NULL);
	if (!xgff_frame_cachep)
		goto err;

	return 0;

err:
	return -1;
}

void __exit clean_xgf_mm(void)
{
	kmem_cache_destroy(xgf_render_cachep);
	kmem_cache_destroy(xgf_dep_cachep);
	kmem_cache_destroy(xgf_runtime_sect_cachep);
	kmem_cache_destroy(xgf_spid_cachep);
	kmem_cache_destroy(xgff_frame_cachep);
}

int __init init_xgf_ko(void)
{
	int i;

	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (xgf_tracepoints[i].tp == NULL) {
			pr_debug("XGF KO Error, %s not found\n",
					xgf_tracepoints[i].name);
			clean_xgf_tp();
			return -1;
		}
	}

	atomic_set(&xgf_ko_enable, 1);
	atomic_set(&xgf_event_buffer_idx, 0);
	atomic_set(&fstb_event_buffer_idx, 0);

	return 0;
}

static ssize_t xgff_mips_exp_enable_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", is_xgff_mips_exp_enable);
}

static ssize_t xgff_mips_exp_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				switch_xgff_mips_exp_enable(!!arg);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(xgff_mips_exp_enable);

int __init init_xgf(void)
{
	init_xgf_ko();
	init_xgf_mm();

	if (!fpsgo_sysfs_create_dir(NULL, "xgf", &xgf_kobj)) {
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_deplist);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_runtime);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_spid_list);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_trace_enable);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_log_trace_enable);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_status);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_ema2_enable_by_pid);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgff_mips_exp_enable);
	}

	xgf_policy_cmd_tree = RB_ROOT;
	xgff_frame_startend_fp = xgff_frame_startend;
	xgff_frame_getdeplist_maxsize_fp = xgff_frame_getdeplist_maxsize;

	return 0;
}

int __exit exit_xgf(void)
{
	xgf_lock(__func__);
	xgf_reset_all_renders();
	xgf_unlock(__func__);
	xgff_reset_all_renders();

	clean_xgf_tp();
	clean_xgf_mm();

	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_deplist);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_runtime);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_spid_list);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_trace_enable);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_log_trace_enable);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_status);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_ema2_enable_by_pid);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgff_mips_exp_enable);

	fpsgo_sysfs_remove_dir(&xgf_kobj);

	return 0;
}
