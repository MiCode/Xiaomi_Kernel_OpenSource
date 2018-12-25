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

#include "fpsgo_base.h"
#include <asm/page.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <linux/uaccess.h>

#include "fpsgo_common.h"
#include "fpsgo_usedext.h"
#include "fbt_cpu.h"
#include "fps_composer.h"

#include <linux/preempt.h>
#include <linux/trace_events.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/rbtree.h>

#define TIME_1S  1000000000ULL

static struct rb_root render_pid_tree;

static DEFINE_MUTEX(fpsgo_render_lock);

void *fpsgo_alloc_atomic(int i32Size)
{
	void *pvBuf;

	if (i32Size <= PAGE_SIZE)
		pvBuf = kmalloc(i32Size, GFP_ATOMIC);
	else
		pvBuf = vmalloc(i32Size);

	return pvBuf;
}

void fpsgo_free(void *pvBuf, int i32Size)
{
	if (!pvBuf)
		return;

	if (i32Size <= PAGE_SIZE)
		kfree(pvBuf);
	else
		vfree(pvBuf);
}

unsigned long long fpsgo_get_time(void)
{
	unsigned long long temp;

	preempt_disable();
	temp = cpu_clock(smp_processor_id());
	preempt_enable();

	return temp;
}

uint32_t fpsgo_systrace_mask;
struct dentry *fpsgo_debugfs_dir;

static unsigned long __read_mostly mark_addr;
static struct dentry *debugfs_common_dir;

#define GENERATE_STRING(name, unused) #name
static const char * const mask_string[] = {
	FPSGO_SYSTRACE_LIST(GENERATE_STRING)
};

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

void __fpsgo_systrace_c(pid_t pid, int val, const char *fmt, ...)
{
	char log[256];
	va_list args;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	preempt_disable();
	event_trace_printk(mark_addr, "C|%d|%s|%d\n", pid, log, val);
	preempt_enable();
}

void __fpsgo_systrace_b(pid_t tgid, const char *fmt, ...)
{
	char log[256];
	va_list args;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	preempt_disable();
	event_trace_printk(mark_addr, "B|%d|%s\n", tgid, log);
	preempt_enable();
}

void __fpsgo_systrace_e(void)
{
	preempt_disable();
	event_trace_printk(mark_addr, "E\n");
	preempt_enable();
}

void fpsgo_render_tree_lock(const char *tag)
{
	mutex_lock(&fpsgo_render_lock);
}

void fpsgo_render_tree_unlock(const char *tag)
{
	mutex_unlock(&fpsgo_render_lock);
}

void fpsgo_lockprove(const char *tag)
{
	WARN_ON(!mutex_is_locked(&fpsgo_render_lock));
}

void fpsgo_thread_lock(struct mutex *mlock)
{
	fpsgo_lockprove(__func__);
	mutex_lock(mlock);
}

void fpsgo_thread_unlock(struct mutex *mlock)
{
	mutex_unlock(mlock);
}

void fpsgo_thread_lockprove(const char *tag, struct mutex *mlock)
{
	WARN_ON(!mutex_is_locked(mlock));
}

int fpsgo_get_tgid(int pid)
{
	struct task_struct *tsk;
	int tgid = 0;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk)
		get_task_struct(tsk);
	rcu_read_unlock();

	if (!tsk)
		return 0;

	tgid = tsk->tgid;
	put_task_struct(tsk);

	return tgid;
}

struct render_info *fpsgo_search_and_add_render_info(int pid, int force)
{
	struct rb_node **p = &render_pid_tree.rb_node;
	struct rb_node *parent = NULL;
	struct render_info *tmp = NULL;
	int tgid;

	fpsgo_lockprove(__func__);

	tgid = fpsgo_get_tgid(pid);

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct render_info, pid_node);

		if (pid < tmp->pid)
			p = &(*p)->rb_left;
		else if (pid > tmp->pid)
			p = &(*p)->rb_right;
		else
			return tmp;
	}

	if (!force)
		return NULL;

	tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
	if (!tmp)
		return NULL;

	mutex_init(&tmp->thr_mlock);
	INIT_LIST_HEAD(&(tmp->bufferid_list));
	INIT_LIST_HEAD(&(tmp->ui_list));
	tmp->pid = pid;
	tmp->tgid = tgid;
	fpsgo_base2fbt_node_init(tmp);

	rb_link_node(&tmp->pid_node, parent, p);
	rb_insert_color(&tmp->pid_node, &render_pid_tree);

	return tmp;
}

void fpsgo_delete_render_info(int pid)
{
	struct render_info *data;
	int delete = 0;

	fpsgo_lockprove(__func__);

	data = fpsgo_search_and_add_render_info(pid, 0);

	if (!data)
		return;

	fpsgo_thread_lock(&data->thr_mlock);
	rb_erase(&data->pid_node, &render_pid_tree);
	list_del(&(data->bufferid_list));
	list_del(&(data->ui_list));
	fpsgo_base2fbt_item_del(data->pLoading, data->p_blc);

	if (data->boost_info.proc.jerks[0].jerking == 0
		&& data->boost_info.proc.jerks[1].jerking == 0)
		delete = 1;
	else {
		delete = 0;
		data->linger = 1;
	}
	fpsgo_thread_unlock(&data->thr_mlock);

	if (delete == 1)
		kfree(data);
}

int fpsgo_has_bypass(void)
{
	struct rb_node *n;
	struct render_info *iter;
	int result = 0;

	fpsgo_lockprove(__func__);

	for (n = rb_first(&render_pid_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, pid_node);
		fpsgo_thread_lock(&iter->thr_mlock);

		if (iter->frame_type == BY_PASS_TYPE) {
			result = 1;
			fpsgo_thread_unlock(&iter->thr_mlock);
			break;
		}
		fpsgo_thread_unlock(&iter->thr_mlock);
	}

	return result;
}

void fpsgo_check_thread_status(void)
{
	unsigned long long ts = fpsgo_get_time();
	unsigned long long expire_ts;
	int delete = 0;
	int check_max_blc = 0;
	int has_bypass = 0;
	int only_bypass = 1;
	struct rb_node *n;
	struct render_info *iter;

	if (ts < TIME_1S)
		return;

	expire_ts = ts - TIME_1S;

	fpsgo_render_tree_lock(__func__);

	n = rb_first(&render_pid_tree);
	while (n) {
		iter = rb_entry(n, struct render_info, pid_node);

		fpsgo_thread_lock(&iter->thr_mlock);

		if (iter->t_enqueue_start < expire_ts) {
			if (iter->pid == fpsgo_base2fbt_get_max_blc_pid())
				check_max_blc = 1;

			rb_erase(&iter->pid_node, &render_pid_tree);
			list_del(&(iter->ui_list));
			list_del(&(iter->bufferid_list));
			fpsgo_base2com_delete_ui_pid_info(iter->ui_pid);
			fpsgo_base2fbt_item_del(iter->pLoading, iter->p_blc);
			n = rb_first(&render_pid_tree);

			if (iter->boost_info.proc.jerks[0].jerking == 0
				&& iter->boost_info.proc.jerks[1].jerking == 0)
				delete = 1;
			else {
				delete = 0;
				iter->linger = 1;
			}

			fpsgo_thread_unlock(&iter->thr_mlock);

			if (delete == 1)
				kfree(iter);

		} else {
			if (iter->frame_type == BY_PASS_TYPE)
				has_bypass = 1;

			else
				only_bypass = 0;

			n = rb_next(n);

			fpsgo_thread_unlock(&iter->thr_mlock);
		}
	}

	fpsgo_render_tree_unlock(__func__);

	fpsgo_fstb2comp_check_connect_api();

	if (check_max_blc)
		fpsgo_base2fbt_check_max_blc();
	if (RB_EMPTY_ROOT(&render_pid_tree))
		fpsgo_base2fbt_no_one_render();
	else if (only_bypass)
		fpsgo_base2fbt_only_bypass();

	fpsgo_base2fbt_set_bypass(has_bypass);
}

void fpsgo_clear(void)
{
	int delete = 0;
	struct rb_node *n;
	struct render_info *iter;

	fpsgo_render_tree_lock(__func__);

	n = rb_first(&render_pid_tree);
	while (n) {
		iter = rb_entry(n, struct render_info, pid_node);

		fpsgo_thread_lock(&iter->thr_mlock);

		rb_erase(&iter->pid_node, &render_pid_tree);
		list_del(&(iter->bufferid_list));
		list_del(&(iter->ui_list));
		fpsgo_base2fbt_item_del(iter->pLoading, iter->p_blc);
		n = rb_first(&render_pid_tree);

		if (iter->boost_info.proc.jerks[0].jerking == 0
			&& iter->boost_info.proc.jerks[1].jerking == 0)
			delete = 1;
		else {
			delete = 0;
			iter->linger = 1;
		}

		fpsgo_thread_unlock(&iter->thr_mlock);

		if (delete == 1)
			kfree(iter);
	}

	fpsgo_base2com_clear_ui_pid_info();

	fpsgo_render_tree_unlock(__func__);
}

static int fpsgo_systrace_mask_show(struct seq_file *m, void *unused)
{
	int i;

	seq_puts(m, " Current enabled systrace:\n");
	for (i = 0; (1U << i) < FPSGO_DEBUG_MAX; i++)
		seq_printf(m, "  %-*s ... %s\n", 12, mask_string[i],
			   fpsgo_systrace_mask & (1U << i) ?
			     "On" : "Off");
	return 0;
}

static ssize_t fpsgo_systrace_mask_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	uint32_t val;
	int ret;

	ret = kstrtou32_from_user(ubuf, cnt, 0, &val);
	if (ret)
		return ret;

	fpsgo_systrace_mask = val & (FPSGO_DEBUG_MAX - 1U);
	return cnt;
}

FPSGO_DEBUGFS_ENTRY(systrace_mask);

static int fpsgo_benchmark_hint_show(struct seq_file *m, void *unused)
{
	seq_printf(m, "%d\n", fpsgo_is_enable());
	return 0;
}

static ssize_t fpsgo_benchmark_hint_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	int val;
	int ret;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret)
		return ret;

	if (val > 1 || val < 0)
		return cnt;

	fpsgo_switch_enable(val);

	return cnt;
}

FPSGO_DEBUGFS_ENTRY(benchmark_hint);

static int fpsgo_render_info_show(struct seq_file *m, void *unused)
{
	struct rb_node *n;
	struct render_info *iter;
	struct task_struct *tsk;

	seq_puts(m, "\n  PID  NAME  TGID  TYPE  API  BufferID");
	seq_puts(m, "    FRAME_L    ENQ_L    ENQ_S    ENQ_E");
	seq_puts(m, "    DEQ_L     DEQ_S    DEQ_E\n");

	fpsgo_render_tree_lock(__func__);
	rcu_read_lock();

	for (n = rb_first(&render_pid_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, pid_node);
		tsk = find_task_by_vpid(iter->tgid);
		if (tsk) {
			get_task_struct(tsk);
			seq_printf(m, "%5d %4s %4d %4d %4d %4llu",
				iter->pid, tsk->comm,
				iter->tgid, iter->frame_type,
				iter->api, iter->buffer_id);
			seq_printf(m, "  %4llu %4llu %4llu %4llu %4llu %4llu %4llu\n",
				iter->self_time, iter->enqueue_length,
				iter->t_enqueue_start, iter->t_enqueue_end,
				iter->dequeue_length, iter->t_dequeue_start,
				iter->t_dequeue_end);
			put_task_struct(tsk);
		}
	}

	rcu_read_unlock();
	fpsgo_render_tree_unlock(__func__);

	return 0;
}

static ssize_t fpsgo_render_info_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	int val;
	int ret;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret)
		return ret;

	return cnt;
}

FPSGO_DEBUGFS_ENTRY(render_info);

static int fpsgo_force_onoff_show(struct seq_file *m, void *unused)
{
	int result = fpsgo_is_force_enable();

	switch (result) {
	case FPSGO_FORCE_OFF:
		seq_puts(m, "force off\n");
		break;
	case FPSGO_FORCE_ON:
		seq_puts(m, "force on\n");
		break;
	case FPSGO_FREE:
		seq_puts(m, "free\n");
		break;
	default:
		break;
	}
	return 0;
}

static ssize_t fpsgo_force_onoff_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	int val;
	int ret;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret)
		return ret;

	if (val > 2 || val < 0)
		return cnt;

	fpsgo_force_switch_enable(val);

	return cnt;
}

FPSGO_DEBUGFS_ENTRY(force_onoff);


int init_fpsgo_common(void)
{
	render_pid_tree = RB_ROOT;

	fpsgo_debugfs_dir = debugfs_create_dir("fpsgo", NULL);
	if (!fpsgo_debugfs_dir)
		return -ENODEV;

	debugfs_common_dir = debugfs_create_dir("common",
						fpsgo_debugfs_dir);
	if (!debugfs_common_dir)
		return -ENODEV;

	debugfs_create_file("systrace_mask",
			    0644,
			    debugfs_common_dir,
			    NULL,
			    &fpsgo_systrace_mask_fops);

	debugfs_create_file("fpsgo_enable",
			    0644,
			    debugfs_common_dir,
			    NULL,
			    &fpsgo_benchmark_hint_fops);

	debugfs_create_file("force_onoff",
			    0644,
			    debugfs_common_dir,
			    NULL,
			    &fpsgo_force_onoff_fops);

	debugfs_create_file("render_info",
			    0644,
			    debugfs_common_dir,
			    NULL,
			    &fpsgo_render_info_fops);

	mark_addr = kallsyms_lookup_name("tracing_mark_write");
	fpsgo_systrace_mask = FPSGO_DEBUG_MANDATORY;

	return 0;
}

