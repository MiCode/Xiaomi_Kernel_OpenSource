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
#include "gbe_usedext.h"
#include <linux/pm_qos.h>

#define MAX_DEP_NUM 30
#define MAIN_LOG_SIZE 256
#define TIMER1_MS 100
#define TIMER2_MS 1000
#define MAX_BOOST_CNT 15
#define LOADING_TH 25
#define NUM_FRAME_TO_FREE 5

#define SYSTEMUI_STR "ndroid.systemui"

static HLIST_HEAD(gbe_boost_units);
static DEFINE_MUTEX(gbe_lock);
struct dentry *gbe_debugfs_dir;
static int gbe_enable;
static int cluster_num;
static struct pm_qos_request dram_req;

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

static unsigned long __read_mostly tracing_mark_write_addr;
static inline void __mt_update_tracing_mark_write_addr(void)
{
	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr =
			kallsyms_lookup_name("tracing_mark_write");
}
static void gbe_trace_printk(int pid, char *module, char *string)
{
	__mt_update_tracing_mark_write_addr();
	preempt_disable();
	event_trace_printk(tracing_mark_write_addr, "%d [%s] %s\n",
			pid, module, string);
	preempt_enable();
}

static void gbe_trace_count(int tid, int val, const char *fmt, ...)
{
	char log[32];
	va_list args;


	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	__mt_update_tracing_mark_write_addr();
	preempt_disable();

	if (!strstr(CONFIG_MTK_PLATFORM, "mt8")) {
		event_trace_printk(tracing_mark_write_addr, "C|%d|%s|%d\n",
				tid, log, val);
	} else {
		event_trace_printk(tracing_mark_write_addr, "C|%s|%d\n",
				log, val);
	}

	preempt_enable();
}

static void gbe_boost_cpu(void)
{
	struct ppm_limit_data *pld;
	struct gbe_boost_unit *iter;
	int uclamp_pct, pm_req;
	int i;
	int boost = 0;

	hlist_for_each_entry(iter, &gbe_boost_units, hlist) {
		if (iter->boost_cnt) {
			boost = 1;
			break;
		}
	}

	pld =
		kcalloc(cluster_num, sizeof(struct ppm_limit_data),
				GFP_KERNEL);

	if (!pld)
		return;

	if (!pm_qos_request_active(&dram_req))
		pm_qos_add_request(&dram_req, PM_QOS_DDR_OPP,
				PM_QOS_DDR_OPP_DEFAULT_VALUE);

	if (boost) {
		for (i = 0; i < cluster_num; i++) {
			pld[i].max = 3000000;
			pld[i].min = 3000000;
		}
		uclamp_pct = 100;
		pm_req = 0;
	} else {
		for (i = 0; i < cluster_num; i++) {
			pld[i].max = -1;
			pld[i].min = -1;
		}
		uclamp_pct = 0;
		pm_req = PM_QOS_DDR_OPP_DEFAULT_VALUE;
	}

	update_userlimit_cpu_freq(CPU_KIR_GBE, cluster_num, pld);
	update_eas_uclamp_min(EAS_UCLAMP_KIR_GBE, CGROUP_TA, uclamp_pct);
	pm_qos_update_request(&dram_req, pm_req);

	kfree(pld);
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
		if (iter->dep[i].loading > LOADING_TH)
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

static int ignore_systemui(int pid)
{
	struct task_struct *tsk, *gtsk;
	int ret = 0;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk) {
		get_task_struct(tsk);
		gtsk = find_task_by_vpid(tsk->tgid);
		put_task_struct(tsk);
		if (gtsk)
			get_task_struct(gtsk);
		else {
			rcu_read_unlock();
			goto out;
		}
	} else {
		rcu_read_unlock();
		goto out;
	}
	rcu_read_unlock();

	ret = !strncmp(SYSTEMUI_STR, gtsk->comm, 16);
	put_task_struct(gtsk);

out:
	return ret;

}

void fpsgo_comp2gbe_frame_update(int pid)
{
	struct gbe_boost_unit *iter;

	if (ignore_systemui(pid))
		return;

	mutex_lock(&gbe_lock);

	if (!gbe_enable) {
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

static int gbe_enable_show(struct seq_file *m, void *unused)
{
	mutex_lock(&gbe_lock);
	seq_printf(m, "%d\n", gbe_enable);
	mutex_unlock(&gbe_lock);
	return 0;
}

static ssize_t gbe_enable_write(struct file *flip,
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

GBE_DEBUGFS_ENTRY(enable);

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

static void __exit gbe_exit(void)
{
}

static int __init gbe_init(void)
{
	gbe_debugfs_dir = debugfs_create_dir("gbe", NULL);
	if (!gbe_debugfs_dir)
		return -ENODEV;

	debugfs_create_file("gbe_enable",
			0644,
			gbe_debugfs_dir,
			NULL,
			&gbe_enable_fops);


	debugfs_create_file("gbe_boost_list",
			0644,
			gbe_debugfs_dir,
			NULL,
			&gbe_boost_list_fops);

	cluster_num = arch_get_nr_clusters();

	return 0;
}

module_init(gbe_init);
module_exit(gbe_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GBE");
MODULE_AUTHOR("MediaTek Inc.");
