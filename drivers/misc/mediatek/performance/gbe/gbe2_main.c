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
				(new_runtime - iter->dep[i].runtime) * 100
				/ (cur_ts_ns - iter->runtime_ts_ns);
			iter->dep[i].runtime = new_runtime;
		}
	}

	iter->runtime_ts_ns = cur_ts_ns;


	for (i = 0; i < iter->dep_num; i++) {
		if (iter->dep[i].loading >= LOADING_TH)
			ret = 1;

		if (strlen(dep_str) == 0)
			snprintf(temp, sizeof(temp), "%d(%llu)",
				iter->dep[i].pid, iter->dep[i].loading);
		else
			snprintf(temp, sizeof(temp), ",%d(%llu)",
				iter->dep[i].pid, iter->dep[i].loading);

		if (strlen(dep_str) + strlen(temp) < MAIN_LOG_SIZE)
			strncat(dep_str, temp, strlen(temp));
	}

	gbe_trace_printk(iter->pid, "gbe", dep_str);
	gbe_trace_count(iter->pid, ret, "gbe_dep_run");

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
		gbe_trace_count(iter->pid,
			iter->boost_cnt, "gbe_boost_cnt");
		gbe_trace_count(iter->pid,
			iter->state, "gbe_state");
		gbe_boost_cpu();
		hrtimer_cancel(&iter->timer2);
		hrtimer_start(&iter->timer2,
			ms_to_ktime(TIMER2_MS), HRTIMER_MODE_REL);
	} else if (check_dep_run_and_update(iter)) {
		if (cur_ts_ms - iter->q_ts_ms > TIMER1_MS) {
			iter->boost_cnt++;
			gbe_trace_count(iter->pid,
				iter->boost_cnt, "gbe_boost_cnt");
			hrtimer_cancel(&iter->timer2);
			hrtimer_start(&iter->timer2,
				ms_to_ktime(TIMER2_MS), HRTIMER_MODE_REL);
		} else {
			iter->state = FPS_UPDATE;
			iter->boost_cnt = 0;
			gbe_boost_cpu();
			gbe_trace_count(iter->pid,
				iter->boost_cnt, "gbe_boost_cnt");
			gbe_trace_count(iter->pid,
				iter->state, "gbe_state");
			hrtimer_cancel(&iter->timer1);
			hrtimer_start(&iter->timer1,
				ms_to_ktime(TIMER1_MS), HRTIMER_MODE_REL);
		}
	} else {
		iter->state = FREE;
		iter->boost_cnt = 0;
		gbe_trace_count(iter->pid,
			iter->boost_cnt, "gbe_boost_cnt");
		gbe_trace_count(iter->pid,
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
		gbe_trace_count(iter->pid,
			iter->boost_cnt, "gbe_boost_cnt");
		gbe_trace_count(iter->pid,
			iter->state, "gbe_state");
		hrtimer_start(&iter->timer2, ms_to_ktime(TIMER2_MS),
			HRTIMER_MODE_REL);
	} else {
		iter->state = FREE;
		gbe_trace_count(iter->pid,
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

void fpsgo_comp2gbe_frame_update(int pid)
{
	struct gbe_boost_unit *iter;

	mutex_lock(&gbe_lock);

	if (!gbe_enable || ignore_nonfg(pid)) {
		mutex_unlock(&gbe_lock);
		return;
	}

	hlist_for_each_entry(iter, &gbe_boost_units, hlist) {
		if (iter->pid == pid)
			break;
	}

	if (iter == NULL) {
		iter =
			kzalloc(sizeof(*iter), GFP_KERNEL);

		if (iter == NULL)
			goto out;

		iter->pid = pid;
		iter->state = NEW_RENDER;
		hlist_add_head(&iter->hlist,
			&gbe_boost_units);
	}

	switch (iter->state) {
	case NEW_RENDER:
		iter->dep_num = gbe2xgf_get_dep_list_num(pid);
		iter->dep_num = iter->dep_num > MAX_DEP_NUM ?
			MAX_DEP_NUM : iter->dep_num;
		gbe2xgf_get_dep_list(pid, iter->dep_num, iter->dep);
		update_runtime(iter);

		gbe_trace_count(iter->pid,
			iter->state, "gbe_state");
		iter->state = FPS_UPDATE;
		gbe_trace_count(iter->pid,
			iter->boost_cnt, "gbe_boost_cnt");
		gbe_trace_count(iter->pid,
			iter->state, "gbe_state");

		gbe_init_timer1(iter);
		gbe_init_timer2(iter);

		hrtimer_start(&iter->timer1, ms_to_ktime(TIMER1_MS),
			HRTIMER_MODE_REL);
		break;
	case FPS_UPDATE:
		iter->dep_num = gbe2xgf_get_dep_list_num(pid);
		iter->dep_num = iter->dep_num > MAX_DEP_NUM ?
			MAX_DEP_NUM : iter->dep_num;
		gbe2xgf_get_dep_list(pid, iter->dep_num, iter->dep);
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

#define GBE_DEBUGFS_ENTRY(name) \
	static int gbe_##name##_open(struct inode *i, struct file *file) \
{ \
	return single_open(file, gbe_##name##_show, i->i_private); \
} \
\
static const struct file_operations gbe_##name##_fops = { \
	.owner = THIS_MODULE, \
	.open = gbe_##name##_open, \
	.read = seq_read, \
	.write = gbe_##name##_write, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

static int gbe_timer1_show(struct seq_file *m, void *unused)
{
	mutex_lock(&gbe_lock);
	seq_printf(m, "%d\n", TIMER1_MS);
	mutex_unlock(&gbe_lock);
	return 0;
}

static ssize_t gbe_timer1_write(struct file *flip,
		const char *ubuf, size_t cnt, loff_t *data)
{

	int ret;
	int val;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret)
		return ret;

	if ((val < 0) || (val > 1000))
		return -EINVAL;

	mutex_lock(&gbe_lock);
	TIMER1_MS = val;
	mutex_unlock(&gbe_lock);

	return cnt;
}

GBE_DEBUGFS_ENTRY(timer1);

static int gbe_timer2_show(struct seq_file *m, void *unused)
{
	mutex_lock(&gbe_lock);
	seq_printf(m, "%d\n", TIMER2_MS);
	mutex_unlock(&gbe_lock);
	return 0;
}

static ssize_t gbe_timer2_write(struct file *flip,
		const char *ubuf, size_t cnt, loff_t *data)
{

	int ret;
	int val;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret)
		return ret;

	if ((val < 0) || (val > 10000))
		return -EINVAL;

	mutex_lock(&gbe_lock);
	TIMER2_MS = val;
	mutex_unlock(&gbe_lock);

	return cnt;
}

GBE_DEBUGFS_ENTRY(timer2);

static int gbe_max_boost_cnt_show(struct seq_file *m, void *unused)
{
	mutex_lock(&gbe_lock);
	seq_printf(m, "%d\n", MAX_BOOST_CNT);
	mutex_unlock(&gbe_lock);
	return 0;
}

static ssize_t gbe_max_boost_cnt_write(struct file *flip,
		const char *ubuf, size_t cnt, loff_t *data)
{

	int ret;
	int val;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret)
		return ret;

	if ((val <= 0) || (val > 15))
		return -EINVAL;

	mutex_lock(&gbe_lock);
	MAX_BOOST_CNT = val;
	mutex_unlock(&gbe_lock);

	return cnt;
}

GBE_DEBUGFS_ENTRY(max_boost_cnt);

static int gbe_loading_th_show(struct seq_file *m, void *unused)
{
	mutex_lock(&gbe_lock);
	seq_printf(m, "%d\n", LOADING_TH);
	mutex_unlock(&gbe_lock);
	return 0;
}

static ssize_t gbe_loading_th_write(struct file *flip,
		const char *ubuf, size_t cnt, loff_t *data)
{

	int ret;
	int val;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret)
		return ret;

	if ((val < 0) || (val > 100))
		return -EINVAL;

	mutex_lock(&gbe_lock);
	LOADING_TH = val;
	mutex_unlock(&gbe_lock);

	return cnt;
}

GBE_DEBUGFS_ENTRY(loading_th);

static int gbe_enable2_show(struct seq_file *m, void *unused)
{
	mutex_lock(&gbe_lock);
	seq_printf(m, "%d\n", gbe_enable);
	mutex_unlock(&gbe_lock);
	return 0;
}

static ssize_t gbe_enable2_write(struct file *flip,
		const char *ubuf, size_t cnt, loff_t *data)
{

	int ret;
	int val;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret)
		return ret;

	if ((val < 0) || (val > 1))
		return -EINVAL;

	mutex_lock(&gbe_lock);
	gbe_enable = val;
	mutex_unlock(&gbe_lock);

	return cnt;
}

GBE_DEBUGFS_ENTRY(enable2);

static int gbe_fg_pid_show(struct seq_file *m, void *unused)
{
	mutex_lock(&gbe_lock);
	seq_printf(m, "%d\n", fg_pid);
	mutex_unlock(&gbe_lock);
	return 0;
}

static ssize_t gbe_fg_pid_write(struct file *flip,
		const char *ubuf, size_t cnt, loff_t *data)
{

	int ret;
	int val;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret)
		return ret;

	if (val < 0)
		return -EINVAL;

	mutex_lock(&gbe_lock);
	fg_pid = val;
	mutex_unlock(&gbe_lock);

	return cnt;
}

GBE_DEBUGFS_ENTRY(fg_pid);

static int gbe_boost_list_show(struct seq_file *m, void *unused)
{
	struct gbe_boost_unit *iter;
	int i;

	mutex_lock(&gbe_lock);
	hlist_for_each_entry(iter, &gbe_boost_units, hlist) {
		seq_printf(m, "%s\t%s\t%s\t%s\n",
				"pid",
				"state",
				"boost_cnt",
				"dep_num");
		seq_printf(m, "%d\t%d\t%d\t%d\n",
				iter->pid,
				iter->state,
				iter->boost_cnt,
				iter->dep_num);
		seq_puts(m, "dep-list:\n");
		for (i = 0; i < iter->dep_num; i++)
			seq_printf(m, "%d ", iter->dep[i].pid);
		seq_puts(m, "\n");
	}
	mutex_unlock(&gbe_lock);

	return 0;
}

static ssize_t gbe_boost_list_write(struct file *flip,
		const char *buffer, size_t count, loff_t *data)
{
	return count;
}

GBE_DEBUGFS_ENTRY(boost_list);

void gbe2_exit(void)
{
}

int gbe2_init(void)
{

	if (!gbe_debugfs_dir)
		return -ENODEV;

	debugfs_create_file("gbe_enable2",
			0644,
			gbe_debugfs_dir,
			NULL,
			&gbe_enable2_fops);

	debugfs_create_file("gbe2_fg_pid",
			0644,
			gbe_debugfs_dir,
			NULL,
			&gbe_fg_pid_fops);

	debugfs_create_file("gbe_boost_list2",
			0644,
			gbe_debugfs_dir,
			NULL,
			&gbe_boost_list_fops);

	debugfs_create_file("gbe2_timer1",
			0644,
			gbe_debugfs_dir,
			NULL,
			&gbe_timer1_fops);

	debugfs_create_file("gbe2_timer2",
			0644,
			gbe_debugfs_dir,
			NULL,
			&gbe_timer2_fops);

	debugfs_create_file("gbe2_max_boost_cnt",
			0644,
			gbe_debugfs_dir,
			NULL,
			&gbe_max_boost_cnt_fops);

	debugfs_create_file("gbe2_loading_th",
			0644,
			gbe_debugfs_dir,
			NULL,
			&gbe_loading_th_fops);


	return 0;
}


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GBE");
MODULE_AUTHOR("MediaTek Inc.");
