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
#include <linux/sched/clock.h>

#include <linux/uaccess.h>

#include "fpsgo_common.h"
#include "fpsgo_usedext.h"
#include "fpsgo_sysfs.h"
#include "fbt_cpu.h"
#include "fps_composer.h"
#include "uboost.h"

#include <linux/preempt.h>
#include <linux/trace_events.h>
#include <linux/fs.h>
#include <linux/rbtree.h>
#include <trace/events/fpsgo.h>

#define TIME_1S  1000000000ULL
#define TRAVERSE_PERIOD  300000000000ULL

static struct kobject *base_kobj;
static struct rb_root render_pid_tree;
static struct rb_root BQ_id_list;
static struct rb_root linger_tree;

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

static unsigned long __read_mostly mark_addr;

#define GENERATE_STRING(name, unused) #name
static const char * const mask_string[] = {
	FPSGO_SYSTRACE_LIST(GENERATE_STRING)
};

static int fpsgo_update_tracemark(void)
{
	if (mark_addr)
		return 1;

	mark_addr = kallsyms_lookup_name("tracing_mark_write");

	if (unlikely(!mark_addr))
		return 0;

	return 1;
}

void __fpsgo_systrace_c(pid_t pid, unsigned long long bufID,
	int val, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	if (unlikely(!fpsgo_update_tracemark()))
		return;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	if (!bufID) {
		preempt_disable();
		event_trace_printk(mark_addr, "C|%d|%s|%d\n", pid, log, val);
		preempt_enable();
	} else {
		preempt_disable();
		event_trace_printk(mark_addr, "C|%d|%s|%d|0x%llx\n",
			pid, log, val, bufID);
		preempt_enable();
	}
}

void __fpsgo_systrace_b(pid_t tgid, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	if (unlikely(!fpsgo_update_tracemark()))
		return;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	preempt_disable();
	event_trace_printk(mark_addr, "B|%d|%s\n", tgid, log);
	preempt_enable();
}

void __fpsgo_systrace_e(void)
{
	if (unlikely(!fpsgo_update_tracemark()))
		return;

	preempt_disable();
	event_trace_printk(mark_addr, "E\n");
	preempt_enable();
}

void fpsgo_main_trace(const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;


	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);
	trace_fpsgo_main_log(log);
}
EXPORT_SYMBOL(fpsgo_main_trace);

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

void fpsgo_add_linger(struct render_info *thr)
{
	struct rb_node **p = &linger_tree.rb_node;
	struct rb_node *parent = NULL;
	struct render_info *tmp = NULL;

	fpsgo_lockprove(__func__);

	if (!thr)
		return;

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct render_info, linger_node);
		if ((uintptr_t)thr < (uintptr_t)tmp)
			p = &(*p)->rb_left;
		else if ((uintptr_t)thr > (uintptr_t)tmp)
			p = &(*p)->rb_right;
		else {
			FPSGO_LOGE("linger exist %d(%p)\n", thr->pid, thr);
			return;
		}
	}

	rb_link_node(&thr->linger_node, parent, p);
	rb_insert_color(&thr->linger_node, &linger_tree);
	thr->linger_ts = fpsgo_get_time();
	FPSGO_LOGI("add to linger %d(%p)(%llu)\n",
			thr->pid, thr, thr->linger_ts);
}

void fpsgo_del_linger(struct render_info *thr)
{
	fpsgo_lockprove(__func__);

	if (!thr)
		return;

	rb_erase(&thr->linger_node, &linger_tree);
	FPSGO_LOGI("del from linger %d(%p)\n", thr->pid, thr);
}

void fpsgo_traverse_linger(unsigned long long cur_ts)
{
	struct rb_node *n;
	struct render_info *pos;
	unsigned long long expire_ts;

	fpsgo_lockprove(__func__);

	if (cur_ts < TRAVERSE_PERIOD)
		return;

	expire_ts = cur_ts - TRAVERSE_PERIOD;

	n = rb_first(&linger_tree);
	while (n) {
		int tofree = 0;

		pos = rb_entry(n, struct render_info, linger_node);
		FPSGO_LOGI("-%d(%p)(%llu),", pos->pid, pos, pos->linger_ts);

		fpsgo_thread_lock(&pos->thr_mlock);

		if (pos->linger_ts && pos->linger_ts < expire_ts) {
			FPSGO_LOGI("timeout %d(%p)(%llu),",
				pos->pid, pos, pos->linger_ts);
			fpsgo_base2fbt_cancel_jerk(pos);
			fpsgo_base2uboost_cancel(pos);
			fpsgo_del_linger(pos);
			tofree = 1;
			n = rb_first(&linger_tree);
		} else
			n = rb_next(n);

		fpsgo_thread_unlock(&pos->thr_mlock);

		if (tofree)
			kfree(pos);
	}
}

int fpsgo_base_is_finished(struct render_info *thr)
{
	fpsgo_lockprove(__func__);
	fpsgo_thread_lockprove(__func__, &(thr->thr_mlock));

	if (!fpsgo_base2fbt_is_finished(thr))
		return 0;

	if (thr->uboost_info.uboosting) {
		FPSGO_LOGE("(%d, %llu)(%p)(%d, %d)\n",
			thr->pid, thr->buffer_id, thr, thr->linger,
			thr->uboost_info.uboosting);
		return 0;
	}

	return 1;
}

struct render_info *fpsgo_search_and_add_render_info(int pid,
	unsigned long long identifier, int force)
{
	struct rb_node **p = &render_pid_tree.rb_node;
	struct rb_node *parent = NULL;
	struct render_info *tmp = NULL;
	int tgid;
	unsigned long long render_key = 0;

	render_key =
		((identifier & 0xFFFFFFFFFFFF) |
		((unsigned long long)pid << 48));

	fpsgo_lockprove(__func__);

	tgid = fpsgo_get_tgid(pid);

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct render_info, render_key_node);

		if (render_key < tmp->render_key)
			p = &(*p)->rb_left;
		else if (render_key > tmp->render_key)
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
	tmp->pid = pid;
	tmp->render_key = render_key;
	tmp->identifier = identifier;
	tmp->tgid = tgid;

	rb_link_node(&tmp->render_key_node, parent, p);
	rb_insert_color(&tmp->render_key_node, &render_pid_tree);

	return tmp;
}

void fpsgo_delete_render_info(int pid,
	unsigned long long buffer_id, unsigned long long identifier)
{
	struct render_info *data;
	int delete = 0;
	int check_max_blc = 0;

	fpsgo_lockprove(__func__);

	data = fpsgo_search_and_add_render_info(pid, identifier, 0);

	if (!data)
		return;

	fpsgo_thread_lock(&data->thr_mlock);
	if (pid == fpsgo_base2fbt_get_max_blc_pid() &&
			buffer_id == fpsgo_base2fbt_get_max_blc_buffer_id())
		check_max_blc = 1;

	rb_erase(&data->render_key_node, &render_pid_tree);
	list_del(&(data->bufferid_list));
	fpsgo_base2fbt_item_del(data->pLoading, data->p_blc,
		data->dep_arr, data);
	data->pLoading = NULL;
	data->p_blc = NULL;
	data->dep_arr = NULL;

	if (fpsgo_base_is_finished(data))
		delete = 1;
	else {
		delete = 0;
		data->linger = 1;
		fpsgo_add_linger(data);
	}
	fpsgo_thread_unlock(&data->thr_mlock);

	if (check_max_blc)
		fpsgo_base2fbt_check_max_blc();

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
		iter = rb_entry(n, struct render_info, render_key_node);
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

static void fpsgo_check_BQid_status(void)
{
	struct rb_node *n;
	struct rb_node *next;
	struct BQ_id *pos;
	int tgid = 0;

	fpsgo_lockprove(__func__);

	for (n = rb_first(&BQ_id_list); n; n = next) {
		next = rb_next(n);

		pos = rb_entry(n, struct BQ_id, entry);
		tgid = fpsgo_get_tgid(pos->pid);
		if (tgid)
			continue;

		rb_erase(n, &BQ_id_list);
		kfree(pos);
	}
}

void fpsgo_clear_llf_cpu_policy(int policy)
{
	struct rb_node *n;
	struct render_info *iter;

	fpsgo_render_tree_lock(__func__);

	for (n = rb_first(&render_pid_tree); n; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, render_key_node);

		fpsgo_thread_lock(&iter->thr_mlock);
		fpsgo_base2fbt_clear_llf_policy(iter, policy);
		fpsgo_thread_unlock(&iter->thr_mlock);
	}

	fpsgo_render_tree_unlock(__func__);
}

static void fpsgo_clear_uclamp_boost_locked(void)
{
	struct rb_node *n;
	struct render_info *iter;

	fpsgo_lockprove(__func__);

	for (n = rb_first(&render_pid_tree); n; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, render_key_node);

		fpsgo_thread_lock(&iter->thr_mlock);
		fpsgo_base2fbt_set_min_cap(iter, 0);
		fpsgo_thread_unlock(&iter->thr_mlock);
	}
}

void fpsgo_clear_uclamp_boost(void)
{
	fpsgo_render_tree_lock(__func__);

	fpsgo_clear_uclamp_boost_locked();

	fpsgo_render_tree_unlock(__func__);
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
	int temp_max_pid = 0;
	unsigned long long temp_max_bufid = 0;

	if (ts < TIME_1S)
		return;

	expire_ts = ts - TIME_1S;

	fpsgo_render_tree_lock(__func__);
	temp_max_pid = fpsgo_base2fbt_get_max_blc_pid();
	temp_max_bufid = fpsgo_base2fbt_get_max_blc_buffer_id();

	n = rb_first(&render_pid_tree);
	while (n) {
		iter = rb_entry(n, struct render_info, render_key_node);

		fpsgo_thread_lock(&iter->thr_mlock);

		if (iter->t_enqueue_start < expire_ts) {
			if (iter->pid == temp_max_pid &&
				iter->buffer_id == temp_max_bufid)
				check_max_blc = 1;

			rb_erase(&iter->render_key_node, &render_pid_tree);
			list_del(&(iter->bufferid_list));
			fpsgo_base2fbt_item_del(iter->pLoading, iter->p_blc,
				iter->dep_arr, iter);
			iter->pLoading = NULL;
			iter->p_blc = NULL;
			iter->dep_arr = NULL;
			n = rb_first(&render_pid_tree);

			if (fpsgo_base_is_finished(iter))
				delete = 1;
			else {
				delete = 0;
				iter->linger = 1;
				fpsgo_add_linger(iter);
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

	fpsgo_check_BQid_status();
	fpsgo_traverse_linger(ts);

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
		iter = rb_entry(n, struct render_info, render_key_node);

		fpsgo_thread_lock(&iter->thr_mlock);

		rb_erase(&iter->render_key_node, &render_pid_tree);
		list_del(&(iter->bufferid_list));
		fpsgo_base2fbt_item_del(iter->pLoading, iter->p_blc,
			iter->dep_arr, iter);
		iter->pLoading = NULL;
		iter->p_blc = NULL;
		iter->dep_arr = NULL;
		n = rb_first(&render_pid_tree);

		if (fpsgo_base_is_finished(iter))
			delete = 1;
		else {
			delete = 0;
			iter->linger = 1;
			fpsgo_add_linger(iter);
		}

		fpsgo_thread_unlock(&iter->thr_mlock);

		if (delete == 1)
			kfree(iter);
	}

	fpsgo_render_tree_unlock(__func__);
}

int fpsgo_uboost_traverse(unsigned long long ts)
{
	struct rb_node *n;
	struct render_info *iter;
	int result = 0;

	fpsgo_render_tree_lock(__func__);

	for (n = rb_first(&render_pid_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, render_key_node);
		fpsgo_thread_lock(&iter->thr_mlock);
		fpsgo_base2uboost_compute(iter, ts);
		fpsgo_thread_unlock(&iter->thr_mlock);
	}

	fpsgo_render_tree_unlock(__func__);

	return result;
}

static struct BQ_id *fpsgo_get_BQid_by_key(unsigned long long key,
		int add, int pid, long long identifier)
{
	struct rb_node **p = &BQ_id_list.rb_node;
	struct rb_node *parent = NULL;
	struct BQ_id *pos;

	fpsgo_lockprove(__func__);

	while (*p) {
		parent = *p;
		pos = rb_entry(parent, struct BQ_id, entry);

		if (key < pos->key)
			p = &(*p)->rb_left;
		else if (key > pos->key)
			p = &(*p)->rb_right;
		else
			return pos;
	}

	if (!add)
		return NULL;

	pos = kzalloc(sizeof(*pos), GFP_KERNEL);
	if (!pos)
		return NULL;

	pos->key = key;
	pos->pid = pid;
	pos->identifier = identifier;
	rb_link_node(&pos->entry, parent, p);
	rb_insert_color(&pos->entry, &BQ_id_list);

	FPSGO_LOGI("add BQid key 0x%llx, pid %d, id 0x%llx\n",
		   key, pid, identifier);
	return pos;
}

static inline void fpsgo_del_BQid_by_pid(int pid)
{
	FPSGO_LOGI("%s should not be used, deleting pid %d\n", __func__, pid);
}

static unsigned long long fpsgo_gen_unique_key(int pid,
		int tgid, long long identifier)
{
	unsigned long long key;

	if (!tgid) {
		tgid = fpsgo_get_tgid(pid);
		if (!tgid)
			return 0ULL;
	}
	key = ((identifier & 0xFFFFFFFFFFFF)
		| ((unsigned long long)tgid << 48));
	return key;
}

struct BQ_id *fpsgo_find_BQ_id(int pid, int tgid,
		long long identifier, int action)
{
	struct rb_node *n;
	struct rb_node *next;
	struct BQ_id *pos;
	unsigned long long key;
	int done = 0;

	fpsgo_lockprove(__func__);

	switch (action) {
	case ACTION_FIND:
	case ACTION_FIND_ADD:
		key = fpsgo_gen_unique_key(pid, tgid, identifier);
		if (key == 0ULL)
			return NULL;
		FPSGO_LOGI("find %s pid %d, id %llu, key %llu\n",
			(action == ACTION_FIND_ADD)?"add":"",
			pid, identifier, key);

		return fpsgo_get_BQid_by_key(key, action == ACTION_FIND_ADD,
					     pid, identifier);

	case ACTION_FIND_DEL:
		key = fpsgo_gen_unique_key(pid, tgid, identifier);
		if (key == 0ULL)
			return NULL;

		for (n = rb_first(&BQ_id_list); n; n = next) {
			next = rb_next(n);

			pos = rb_entry(n, struct BQ_id, entry);
			if (pos->key == key) {
				FPSGO_LOGI(
					"find del pid %d, id %llu, key %llu\n",
					pid, identifier, key);
				rb_erase(n, &BQ_id_list);
				kfree(pos);
				done = 1;
				break;
			}
		}
		if (!done)
			FPSGO_LOGE("del fail key %llu\n", key);
		return NULL;
	case ACTION_DEL_PID:
		FPSGO_LOGI("del BQid pid %d\n", pid);
		fpsgo_del_BQid_by_pid(pid);
		return NULL;
	default:
		FPSGO_LOGE("[ERROR] unknown action %d\n", action);
		return NULL;
	}
}

int fpsgo_get_BQid_pair(int pid, int tgid, long long identifier,
		unsigned long long *buffer_id, int *queue_SF, int enqueue)
{
	struct BQ_id *pair;

	fpsgo_lockprove(__func__);

	pair = fpsgo_find_BQ_id(pid, tgid, identifier, ACTION_FIND);

	if (pair) {
		*buffer_id = pair->buffer_id;
		*queue_SF = pair->queue_SF;
		if (enqueue)
			pair->queue_pid = pid;
		return 1;
	}

	return 0;
}

static ssize_t systrace_mask_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i;
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			" Current enabled systrace:\n");
	pos += length;

	for (i = 0; (1U << i) < FPSGO_DEBUG_MAX; i++) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"  %-*s ... %s\n", 12, mask_string[i],
		   fpsgo_systrace_mask & (1U << i) ?
		   "On" : "Off");
		pos += length;

	}

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t systrace_mask_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	uint32_t val = -1;
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	uint32_t arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtou32(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	fpsgo_systrace_mask = val & (FPSGO_DEBUG_MAX - 1U);
	return count;
}

static KOBJ_ATTR_RW(systrace_mask);

static ssize_t fpsgo_enable_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", fpsgo_is_enable());
}

static ssize_t fpsgo_enable_store(struct kobject *kobj,
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

	if (val > 1 || val < 0)
		return count;

	fpsgo_switch_enable(val);

	return count;
}

static KOBJ_ATTR_RWO(fpsgo_enable);

static ssize_t render_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct rb_node *n;
	struct render_info *iter;
	struct task_struct *tsk;
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"\n  PID  NAME  TGID  TYPE  API  BufferID");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"    FRAME_L    ENQ_L    ENQ_S    ENQ_E");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"    DEQ_L     DEQ_S    DEQ_E\n");
	pos += length;

	fpsgo_render_tree_lock(__func__);
	rcu_read_lock();

	for (n = rb_first(&render_pid_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, render_key_node);
		tsk = find_task_by_vpid(iter->tgid);
		if (tsk) {
			get_task_struct(tsk);

			length = scnprintf(temp + pos,
					FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
					"%5d %4s %4d %4d %4d 0x%llx",
				iter->pid, tsk->comm,
				iter->tgid, iter->frame_type,
				iter->api, iter->buffer_id);
			pos += length;
			length = scnprintf(temp + pos,
					FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
					"  %4llu %4llu %4llu %4llu %4llu %4llu\n",
				iter->enqueue_length,
				iter->t_enqueue_start, iter->t_enqueue_end,
				iter->dequeue_length, iter->t_dequeue_start,
				iter->t_dequeue_end);
			pos += length;
			put_task_struct(tsk);
		}
	}

	rcu_read_unlock();
	fpsgo_render_tree_unlock(__func__);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static KOBJ_ATTR_RO(render_info);

static ssize_t force_onoff_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int result = fpsgo_is_force_enable();
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;


	switch (result) {
	case FPSGO_FORCE_OFF:
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"force off\n");
		pos += length;
		break;
	case FPSGO_FORCE_ON:
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"force on\n");
		pos += length;
		break;
	case FPSGO_FREE:
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"free\n");
		pos += length;
		break;
	default:
		break;
	}
	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t force_onoff_store(struct kobject *kobj,
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

	if (val > 2 || val < 0)
		return count;

	fpsgo_force_switch_enable(val);

	return count;
}

static KOBJ_ATTR_RW(force_onoff);


static ssize_t BQid_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct rb_node *n;
	struct BQ_id *pos;
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int posi = 0;
	int length;

	fpsgo_render_tree_lock(__func__);

	for (n = rb_first(&BQ_id_list); n; n = rb_next(n)) {
		pos = rb_entry(n, struct BQ_id, entry);
		length = scnprintf(temp + posi,
				FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
				"pid %d, tgid %d, key %llu, buffer_id %llu, queue_SF %d\n",
				pos->pid, fpsgo_get_tgid(pos->pid),
				pos->key, pos->buffer_id, pos->queue_SF);
		posi += length;

	}

	fpsgo_render_tree_unlock(__func__);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static KOBJ_ATTR_RO(BQid);

static ssize_t gpu_block_boost_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d %d %d\n",
		fpsgo_is_gpu_block_boost_enable(),
		fpsgo_is_gpu_block_boost_perf_enable(),
		fpsgo_is_gpu_block_boost_camera_enable());
}

static ssize_t gpu_block_boost_store(struct kobject *kobj,
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

	if (val > 101 || val < -1)
		return count;

	fpsgo_gpu_block_boost_enable_perf(val);

	return count;
}

static KOBJ_ATTR_RW(gpu_block_boost);


int init_fpsgo_common(void)
{
	render_pid_tree = RB_ROOT;

	BQ_id_list = RB_ROOT;
	linger_tree = RB_ROOT;

	if (!fpsgo_sysfs_create_dir(NULL, "common", &base_kobj)) {
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_systrace_mask);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_fpsgo_enable);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_force_onoff);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_render_info);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_BQid);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_gpu_block_boost);
	}

	fpsgo_update_tracemark();
	fpsgo_systrace_mask = FPSGO_DEBUG_MANDATORY;

	return 0;
}

