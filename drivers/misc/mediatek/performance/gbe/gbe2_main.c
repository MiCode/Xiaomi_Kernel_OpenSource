// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#include <linux/sched/clock.h>
#include <linux/sched/mm.h>
#include <linux/sched/numa_balancing.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/task.h>
#include <linux/sched/cputime.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#include "cpu_ctrl.h"
#include "eas_ctrl.h"

#include <linux/workqueue.h>
#include <linux/unistd.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include "gbe2_usedext.h"
#include "gbe_common.h"
#include "gbe_sysfs.h"
#include <linux/pm_qos.h>

#define MAX_DEP_NUM 30
#define MAIN_LOG_SIZE 256
#define DEFAULT_TIMER1_MS 100
#define DEFAULT_TIMER2_MS 1000
#define DEFAULT_MAX_BOOST_CNT 5
#define DEFAULT_LOADING_TH 20

static HLIST_HEAD(gbe_boost_units);
static DEFINE_MUTEX(gbe_lock);
static int gbe_enable;
static int fg_pid;
static int TIMER1_MS = DEFAULT_TIMER1_MS;
static int TIMER2_MS = DEFAULT_TIMER2_MS;
static int MAX_BOOST_CNT = DEFAULT_MAX_BOOST_CNT;
static int LOADING_TH = DEFAULT_LOADING_TH;

enum {
	NEW_RENDER = 0,
	FPS_UPDATE,
	BOOSTING,
	FREE,
};

struct gbe_boost_unit {
	int pid;
	unsigned long long bufID;
	int state;
	int boost_cnt;
	int dep_num;
	struct gbe_runtime dep[MAX_DEP_NUM]; //latest dep, no accumulate
	unsigned long long q_ts_ms;
	unsigned long long runtime_ts_ns;

	struct hrtimer timer1;
	struct hrtimer timer2;
	struct work_struct work1;
	struct work_struct work2;

	struct hlist_node hlist;
};


static void gbe_boost_cpu(void)
{
	struct gbe_boost_unit *iter;
	int boost = 0;

	hlist_for_each_entry(iter, &gbe_boost_units, hlist) {
		if (iter->boost_cnt) {
			boost = 1;
			break;
		}
	}

	gbe_boost(KIR_GBE2, boost);

}

static void update_runtime(struct gbe_boost_unit *iter)
{
	int i;
	struct task_struct *p;

	iter->runtime_ts_ns = ktime_to_ns(ktime_get());

	for (i = 0; i < iter->dep_num; i++) {
		rcu_read_lock();
		p = find_task_by_vpid(iter->dep[i].pid);
		if (!p) {
			iter->dep[i].runtime = 0;
			rcu_read_unlock();
		} else {
			get_task_struct(p);
			rcu_read_unlock();
			iter->dep[i].runtime = task_sched_runtime(p);
			put_task_struct(p);
		}
	}

}

static int check_dep_run_and_update(struct gbe_boost_unit *iter)
{
	int i;
	int ret  = 0;
	struct task_struct *p;
	unsigned long long cur_ts_ns = ktime_to_ns(ktime_get());
	char dep_str[MAIN_LOG_SIZE] = {"\0"};
	char temp[MAIN_LOG_SIZE] = {"\0"};
	unsigned long long new_runtime = 0;
	int tmplen;

	for (i = 0; i < iter->dep_num; i++) {
		rcu_read_lock();
		p = find_task_by_vpid(iter->dep[i].pid);
		if (!p) {
			iter->dep[i].runtime = 0;
			iter->dep[i].loading = 0;
			rcu_read_unlock();
		} else {
			get_task_struct(p);
			rcu_read_unlock();
			//iter->dep[i].runtime = p->se.avg.util_avg;
			new_runtime = task_sched_runtime(p);
			put_task_struct(p);

			iter->dep[i].loading =
#if BITS_PER_LONG == 32
				div_u64((new_runtime - iter->dep[i].runtime) * 100,
				(cur_ts_ns - iter->runtime_ts_ns));
#else
				(new_runtime - iter->dep[i].runtime) * 100
				/ (cur_ts_ns - iter->runtime_ts_ns);
#endif
			iter->dep[i].runtime = new_runtime;
		}
	}

	iter->runtime_ts_ns = cur_ts_ns;


	for (i = 0; i < iter->dep_num; i++) {
		if (iter->dep[i].loading >= LOADING_TH)
			ret = 1;

		if (strlen(dep_str) == 0)
			tmplen = snprintf(temp, sizeof(temp), "%d(%llu)",
				iter->dep[i].pid, iter->dep[i].loading);
		else
			tmplen = snprintf(temp, sizeof(temp), ",%d(%llu)",
				iter->dep[i].pid, iter->dep[i].loading);

		if (tmplen < 0 || tmplen >= sizeof(temp))
			return ret;

		if (strlen(dep_str) + strlen(temp) < MAIN_LOG_SIZE)
			strncat(dep_str, temp, strlen(temp));
	}

	gbe_trace_printk(iter->pid, "gbe", dep_str);
	gbe_trace_count(iter->pid, iter->bufID, ret, "gbe_dep_run");

	return ret;
}

static void gbe_do_timer2(struct work_struct *work)
{
	struct gbe_boost_unit *iter;
	unsigned long long cur_ts_ms = ktime_to_ms(ktime_get());

	iter = container_of(work, struct gbe_boost_unit, work2);

	mutex_lock(&gbe_lock);

	if (iter->state == FREE) {
		hlist_del(&iter->hlist);
		kfree(iter);
	} else if (iter->boost_cnt > MAX_BOOST_CNT) {
		iter->state = FREE;
		iter->boost_cnt = 0;
		gbe_trace_count(iter->pid, iter->bufID,
			iter->boost_cnt, "gbe_boost_cnt");
		gbe_trace_count(iter->pid, iter->bufID,
			iter->state, "gbe_state");
		gbe_boost_cpu();
		hrtimer_cancel(&iter->timer2);
		hrtimer_start(&iter->timer2,
			ms_to_ktime(TIMER2_MS), HRTIMER_MODE_REL);
	} else if (check_dep_run_and_update(iter)) {
		if (cur_ts_ms - iter->q_ts_ms > TIMER1_MS) {
			iter->boost_cnt++;
			gbe_trace_count(iter->pid, iter->bufID,
				iter->boost_cnt, "gbe_boost_cnt");
			hrtimer_cancel(&iter->timer2);
			hrtimer_start(&iter->timer2,
				ms_to_ktime(TIMER2_MS), HRTIMER_MODE_REL);
		} else {
			iter->state = FPS_UPDATE;
			iter->boost_cnt = 0;
			gbe_boost_cpu();
			gbe_trace_count(iter->pid, iter->bufID,
				iter->boost_cnt, "gbe_boost_cnt");
			gbe_trace_count(iter->pid, iter->bufID,
				iter->state, "gbe_state");
			hrtimer_cancel(&iter->timer1);
			hrtimer_start(&iter->timer1,
				ms_to_ktime(TIMER1_MS), HRTIMER_MODE_REL);
		}
	} else {
		iter->state = FREE;
		iter->boost_cnt = 0;
		gbe_trace_count(iter->pid, iter->bufID,
			iter->boost_cnt, "gbe_boost_cnt");
		gbe_trace_count(iter->pid, iter->bufID,
			iter->state, "gbe_state");
		gbe_boost_cpu();
		hrtimer_cancel(&iter->timer2);
		hrtimer_start(&iter->timer2,
			ms_to_ktime(TIMER2_MS), HRTIMER_MODE_REL);
	}


	mutex_unlock(&gbe_lock);
}

static enum hrtimer_restart gbe_timer2_tfn(struct hrtimer *timer)
{
	struct gbe_boost_unit *iter;

	iter = container_of(timer, struct gbe_boost_unit, timer2);
	schedule_work(&iter->work2);
	return HRTIMER_NORESTART;
}

static inline void gbe_init_timer2(struct gbe_boost_unit *iter)
{
	if (iter == NULL)
		return;
	hrtimer_init(&iter->timer2, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	iter->timer2.function = &gbe_timer2_tfn;
	INIT_WORK(&iter->work2, gbe_do_timer2);
}

static void gbe_do_timer1(struct work_struct *work)
{
	struct gbe_boost_unit *iter;

	iter = container_of(work, struct gbe_boost_unit, work1);

	mutex_lock(&gbe_lock);

	if (check_dep_run_and_update(iter)) {
		iter->state = BOOSTING;
		iter->boost_cnt = 1;
		gbe_boost_cpu();
		gbe_trace_count(iter->pid, iter->bufID,
			iter->boost_cnt, "gbe_boost_cnt");
		gbe_trace_count(iter->pid, iter->bufID,
			iter->state, "gbe_state");
		hrtimer_start(&iter->timer2, ms_to_ktime(TIMER2_MS),
			HRTIMER_MODE_REL);
	} else {
		iter->state = FREE;
		gbe_trace_count(iter->pid, iter->bufID,
			iter->state, "gbe_state");
		hrtimer_start(&iter->timer2, ms_to_ktime(TIMER2_MS),
			HRTIMER_MODE_REL);
	}
	mutex_unlock(&gbe_lock);
}

static enum hrtimer_restart gbe_timer1_tfn(struct hrtimer *timer)
{
	struct gbe_boost_unit *iter;

	iter = container_of(timer, struct gbe_boost_unit, timer1);
	schedule_work(&iter->work1);
	return HRTIMER_NORESTART;
}

static inline void gbe_init_timer1(struct gbe_boost_unit *iter)
{
	if (iter == NULL)
		return;
	hrtimer_init(&iter->timer1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	iter->timer1.function = &gbe_timer1_tfn;
	INIT_WORK(&iter->work1, gbe_do_timer1);
}

static int ignore_nonfg(int pid)
{
	struct task_struct *tsk;
	int ret = 0;
	int process_id = 0;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk) {
		get_task_struct(tsk);
		process_id = tsk->tgid;
		put_task_struct(tsk);
	} else {
		rcu_read_unlock();
		goto out;
	}
	rcu_read_unlock();

	ret = (fg_pid != process_id);

out:
	return ret;
}

void fpsgo_comp2gbe_frame_update(int pid, unsigned long long bufID)
{
	struct gbe_boost_unit *iter;

	mutex_lock(&gbe_lock);

	if (!gbe_enable || ignore_nonfg(pid)) {
		mutex_unlock(&gbe_lock);
		return;
	}

	hlist_for_each_entry(iter, &gbe_boost_units, hlist) {
		if (iter->pid == pid && iter->bufID == bufID)
			break;
	}

	if (iter == NULL) {
		iter =
			kzalloc(sizeof(*iter), GFP_KERNEL);

		if (iter == NULL)
			goto out;

		iter->pid = pid;
		iter->bufID = bufID;
		iter->state = NEW_RENDER;
		iter->boost_cnt = 0;
		hlist_add_head(&iter->hlist,
			&gbe_boost_units);
	}

	switch (iter->state) {
	case NEW_RENDER:
		iter->dep_num = gbe2xgf_get_dep_list_num(pid, bufID);
		iter->dep_num = iter->dep_num > MAX_DEP_NUM ?
			MAX_DEP_NUM : iter->dep_num;
		gbe2xgf_get_dep_list(pid, iter->dep_num, iter->dep, bufID);
		update_runtime(iter);

		gbe_trace_count(iter->pid, iter->bufID,
			iter->state, "gbe_state");
		iter->state = FPS_UPDATE;
		gbe_trace_count(iter->pid, iter->bufID,
			iter->boost_cnt, "gbe_boost_cnt");
		gbe_trace_count(iter->pid, iter->bufID,
			iter->state, "gbe_state");

		gbe_init_timer1(iter);
		gbe_init_timer2(iter);

		hrtimer_start(&iter->timer1, ms_to_ktime(TIMER1_MS),
			HRTIMER_MODE_REL);
		break;
	case FPS_UPDATE:
		iter->dep_num = gbe2xgf_get_dep_list_num(pid, bufID);
		iter->dep_num = iter->dep_num > MAX_DEP_NUM ?
			MAX_DEP_NUM : iter->dep_num;
		gbe2xgf_get_dep_list(pid, iter->dep_num, iter->dep, bufID);
		update_runtime(iter);

		hrtimer_cancel(&iter->timer1);
		hrtimer_start(&iter->timer1, ms_to_ktime(TIMER1_MS),
			HRTIMER_MODE_REL);
		break;
	case BOOSTING:
		iter->q_ts_ms = ktime_to_ms(ktime_get());
		break;
	case FREE:
		break;
	default:
		break;
	}

out:
	mutex_unlock(&gbe_lock);

}

static ssize_t gbe2_timer1_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val;

	mutex_lock(&gbe_lock);
	val = TIMER1_MS;
	mutex_unlock(&gbe_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t gbe2_timer1_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GBE_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < GBE_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GBE_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				mutex_lock(&gbe_lock);
				if (arg >= 0 && arg <= 1000)
					TIMER1_MS = arg;
				mutex_unlock(&gbe_lock);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(gbe2_timer1);

static ssize_t gbe2_timer2_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val;

	mutex_lock(&gbe_lock);
	val = TIMER2_MS;
	mutex_unlock(&gbe_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t gbe2_timer2_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GBE_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < GBE_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GBE_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				mutex_lock(&gbe_lock);
				if (arg >= 0 && arg <= 10000)
					TIMER2_MS = arg;
				mutex_unlock(&gbe_lock);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(gbe2_timer2);

static ssize_t gbe2_max_boost_cnt_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val;

	mutex_lock(&gbe_lock);
	val = MAX_BOOST_CNT;
	mutex_unlock(&gbe_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t gbe2_max_boost_cnt_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GBE_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < GBE_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GBE_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				mutex_lock(&gbe_lock);
				if (arg > 0 && arg <= 15)
					MAX_BOOST_CNT = arg;
				mutex_unlock(&gbe_lock);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(gbe2_max_boost_cnt);

static ssize_t gbe2_loading_th_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val;

	mutex_lock(&gbe_lock);
	val = LOADING_TH;
	mutex_unlock(&gbe_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t gbe2_loading_th_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GBE_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < GBE_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GBE_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				mutex_lock(&gbe_lock);
				if (arg >= 0 && arg <= 100)
					LOADING_TH = arg;
				mutex_unlock(&gbe_lock);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(gbe2_loading_th);

static ssize_t gbe_enable2_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val;

	mutex_lock(&gbe_lock);
	val = gbe_enable;
	mutex_unlock(&gbe_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t gbe_enable2_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GBE_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < GBE_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GBE_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				mutex_lock(&gbe_lock);
				gbe_enable = !!arg;
				mutex_unlock(&gbe_lock);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(gbe_enable2);

static ssize_t gbe2_fg_pid_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val;

	mutex_lock(&gbe_lock);
	val = fg_pid;
	mutex_unlock(&gbe_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t gbe2_fg_pid_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{

	char acBuffer[GBE_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < GBE_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GBE_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				mutex_lock(&gbe_lock);
				fg_pid = arg;
				mutex_unlock(&gbe_lock);
			}
		}
	}

	return count;

}

static KOBJ_ATTR_RW(gbe2_fg_pid);

static ssize_t gbe_boost_list2_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct gbe_boost_unit *iter;
	int i;
	char temp[GBE_SYSFS_MAX_BUFF_SIZE] = "";
	int pos = 0;
	int length;

	mutex_lock(&gbe_lock);
	hlist_for_each_entry(iter, &gbe_boost_units, hlist) {
		length = scnprintf(temp + pos, GBE_SYSFS_MAX_BUFF_SIZE - pos,
				"%s\t%s\t%s\t%s\n",
				"pid",
				"state",
				"boost_cnt",
				"dep_num");
		pos += length;

		length = scnprintf(temp + pos, GBE_SYSFS_MAX_BUFF_SIZE - pos,
				"%d\t%d\t%d\t%d\n",
				iter->pid,
				iter->state,
				iter->boost_cnt,
				iter->dep_num);
		pos += length;

		length = scnprintf(temp + pos, GBE_SYSFS_MAX_BUFF_SIZE - pos,
				"dep-list:\n");
		pos += length;

		for (i = 0; i < iter->dep_num; i++) {
			length =
				scnprintf(temp + pos,
				GBE_SYSFS_MAX_BUFF_SIZE - pos,
				"%d ", iter->dep[i].pid);
			pos += length;
		}
		length = scnprintf(temp + pos, GBE_SYSFS_MAX_BUFF_SIZE - pos,
				"\n");
		pos += length;
	}
	mutex_unlock(&gbe_lock);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static KOBJ_ATTR_RO(gbe_boost_list2);

void gbe2_exit(void)
{
	gbe_sysfs_remove_file(&kobj_attr_gbe_enable2);
	gbe_sysfs_remove_file(&kobj_attr_gbe2_fg_pid);
	gbe_sysfs_remove_file(&kobj_attr_gbe_boost_list2);
	gbe_sysfs_remove_file(&kobj_attr_gbe2_timer1);
	gbe_sysfs_remove_file(&kobj_attr_gbe2_timer2);
	gbe_sysfs_remove_file(&kobj_attr_gbe2_max_boost_cnt);
	gbe_sysfs_remove_file(&kobj_attr_gbe2_loading_th);
}

int gbe2_init(void)
{
	gbe_sysfs_create_file(&kobj_attr_gbe_enable2);
	gbe_sysfs_create_file(&kobj_attr_gbe2_fg_pid);
	gbe_sysfs_create_file(&kobj_attr_gbe_boost_list2);
	gbe_sysfs_create_file(&kobj_attr_gbe2_timer1);
	gbe_sysfs_create_file(&kobj_attr_gbe2_timer2);
	gbe_sysfs_create_file(&kobj_attr_gbe2_max_boost_cnt);
	gbe_sysfs_create_file(&kobj_attr_gbe2_loading_th);


	return 0;
}


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GBE");
MODULE_AUTHOR("MediaTek Inc.");
