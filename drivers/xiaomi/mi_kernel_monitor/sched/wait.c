// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025 xiaomi. All rights reserved.
 */


#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/ratelimit.h>
#include <linux/ktime.h>
#include <linux/seq_file.h>
#include <linux/version.h>
#include <linux/atomic/atomic-long.h>
#include <linux/sched.h>
#include <trace/events/sched.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/dtask.h>
#include <linux/version.h>

#include "wait_base.h"
#include "internal.h"

#define LATENCY_STRING_FORMAT(BUF, MODULE, SCHED_STAT) sprintf(BUF, \
	#MODULE"_delta_ms: %llu\n"#MODULE"_min_thresh_ms: %d\n"#MODULE"_low_thresh_ms: %d\n"#MODULE"_high_thresh_ms: %d\n" \
	#MODULE"_max_ms: %llu\n"#MODULE"_high_cnt: %llu\n"#MODULE"_low_cnt: %llu\n"#MODULE"_min_cnt: %llu\n" \
	#MODULE"_total_ms: %llu\n"#MODULE"_total_cnt: %llu\n" \
	#MODULE"_fg_max_ms: %llu\n"#MODULE"_fg_high_cnt: %llu\n"#MODULE"_fg_low_cnt: %llu\n"#MODULE"_fg_min_cnt: %llu\n" \
	#MODULE"_fg_total_ms: %llu\n"#MODULE"_fg_total_cnt: %llu\n" \
	#MODULE"_ux_max_ms: %llu\n"#MODULE"_ux_high_cnt: %llu\n"#MODULE"_ux_low_cnt: %llu\n"#MODULE"_ux_min_cnt: %llu\n" \
	#MODULE"_ux_total_ms: %llu\n"#MODULE"_ux_total_cnt: %llu\n" \
	#MODULE"_rt_max_ms: %llu\n"#MODULE"_rt_high_cnt: %llu\n"#MODULE"_rt_low_cnt: %llu\n"#MODULE"_rt_min_cnt: %llu\n" \
	#MODULE"_rt_total_ms: %llu\n"#MODULE"_rt_total_cnt: %llu\n" \
	#MODULE"_top_app_max_ms: %llu\n"#MODULE"_top_app_high_cnt: %llu\n"#MODULE"_top_app_low_cnt: %llu\n"#MODULE"_top_app_min_cnt: %llu\n" \
	#MODULE"_top_app_total_ms: %llu\n"#MODULE"_top_app_total_cnt: %llu\n" \
	#MODULE"_bg_max_ms: %llu\n"#MODULE"_bg_high_cnt: %llu\n"#MODULE"_bg_low_cnt: %llu\n"#MODULE"_bg_min_cnt: %llu\n" \
	#MODULE"_bg_total_ms: %llu\n"#MODULE"_bg_total_cnt: %llu\n" \
	#MODULE"_sys_bg_max_ms: %llu\n"#MODULE"_sys_bg_high_cnt: %llu\n"#MODULE"_sys_bg_low_cnt: %llu\n"#MODULE"_sys_bg_min_cnt: %llu\n" \
	#MODULE"_sys_bg_total_ms: %llu\n"#MODULE"_sys_bg_total_cnt: %llu\n", \
	SCHED_STAT->delta_ms, \
	SCHED_STAT->min_thresh_ms, \
	SCHED_STAT->low_thresh_ms, \
	SCHED_STAT->high_thresh_ms, \
	SCHED_STAT->all.max_ms, \
	SCHED_STAT->all.high_cnt, \
	SCHED_STAT->all.low_cnt, \
	SCHED_STAT->all.min_cnt, \
	SCHED_STAT->all.total_ms, \
	SCHED_STAT->all.total_cnt, \
	SCHED_STAT->fg.max_ms, \
	SCHED_STAT->fg.high_cnt, \
	SCHED_STAT->fg.low_cnt, \
	SCHED_STAT->fg.min_cnt, \
	SCHED_STAT->fg.total_ms, \
	SCHED_STAT->fg.total_cnt, \
	SCHED_STAT->ux.max_ms, \
	SCHED_STAT->ux.high_cnt, \
	SCHED_STAT->ux.low_cnt, \
	SCHED_STAT->ux.min_cnt, \
	SCHED_STAT->ux.total_ms, \
	SCHED_STAT->ux.total_cnt, \
	SCHED_STAT->rt.max_ms, \
	SCHED_STAT->rt.high_cnt, \
	SCHED_STAT->rt.low_cnt, \
	SCHED_STAT->rt.min_cnt, \
	SCHED_STAT->rt.total_ms, \
	SCHED_STAT->rt.total_cnt, \
	SCHED_STAT->top.max_ms, \
	SCHED_STAT->top.high_cnt, \
	SCHED_STAT->top.low_cnt, \
	SCHED_STAT->top.min_cnt, \
	SCHED_STAT->top.total_ms, \
	SCHED_STAT->top.total_cnt, \
	SCHED_STAT->bg.max_ms, \
	SCHED_STAT->bg.high_cnt, \
	SCHED_STAT->bg.low_cnt, \
	SCHED_STAT->bg.min_cnt, \
	SCHED_STAT->bg.total_ms, \
	SCHED_STAT->bg.total_cnt, \
	SCHED_STAT->sysbg.max_ms, \
	SCHED_STAT->sysbg.high_cnt, \
	SCHED_STAT->sysbg.low_cnt, \
	SCHED_STAT->sysbg.min_cnt, \
	SCHED_STAT->sysbg.total_ms, \
	SCHED_STAT->sysbg.total_cnt)

int g_osi_debug = 1;

struct sched_stat_para sched_para[OHM_SCHED_TOTAL];

#if IS_ENABLED(CONFIG_CGROUP_SCHED)
static inline int get_task_cgroup_id(struct task_struct *task)
{
	struct cgroup_subsys_state *css = task_css(task, cpu_cgrp_id);

	return css ? css->id : -1;
}
#else
inline int get_task_cgroup_id(struct task_struct *task) { return 0; }
#endif

int test_task_top_app(struct task_struct *task)
{
	return (SA_CGROUP_TOP_APP == get_task_cgroup_id(task)) ? 1 : 0;
}
static int test_task_fg(struct task_struct *task)
{
	return (SA_CGROUP_FOREGROUND == get_task_cgroup_id(task)) ? 1 : 0;
}
static int test_task_sys_bg(struct task_struct *task)
{
	 return (SA_CGROUP_SYS_BACKGROUND == get_task_cgroup_id(task)) ? 1 : 0;
}
static int test_task_bg(struct task_struct *task)
{
	return (SA_CGROUP_BACKGROUND == get_task_cgroup_id(task)) ? 1 : 0;
}

static inline void ohm_latency_dist_record(struct sched_stat_common *stat_common, u64 delta)
{
	int i = 0;

	if (delta < 1) {
	} else if (++i && delta < 2) {
	} else if (++i && delta < 5) {
	} else if (++i && delta < 10) {
	} else if (++i && delta < 20) {
	} else if (++i && delta < 50) {
	} else if (++i && delta < 100) {
	} else if (++i && delta < 200) {
	} else if (++i && delta < 500) {
	} else if (++i && delta < 1000) {
	} else if (++i && delta < 2000) {
	} else if (++i && delta < 5000) {
	} else {
		++i;
	}

	if (i >= OHM_LATENCY_DIST_MAX)
		return;
	stat_common->latency_dist[i]++;
}

static inline void ohm_sched_stat_record_common(struct sched_stat_para *sched_stat, struct sched_stat_common *stat_common, u64 delta_ms)
{
	stat_common->total_ms += delta_ms;
	stat_common->total_cnt++;
	if (delta_ms > stat_common->max_ms) {
		stat_common->max_ms = delta_ms;
	}
	if (delta_ms >= sched_stat->high_thresh_ms) {
		stat_common->high_cnt++;
	} else if (delta_ms >= sched_stat->low_thresh_ms) {
		stat_common->low_cnt++;
	}else if (delta_ms >= sched_stat->min_thresh_ms) {
		stat_common->min_cnt++;
	}

	if (sched_stat == &sched_para[OHM_SCHED_IOWAIT]) {
		ohm_latency_dist_record(stat_common, delta_ms);
	}
}

static inline void ohm_para_reset(struct sched_stat_para *sched_para)
{
	sched_para->delta_ms = 0;
	memset(&sched_para->all, 0, sizeof(struct sched_stat_common));
	memset(&sched_para->ux, 0, sizeof(struct sched_stat_common));
	memset(&sched_para->rt, 0, sizeof(struct sched_stat_common));
	memset(&sched_para->fg, 0, sizeof(struct sched_stat_common));
	memset(&sched_para->top, 0, sizeof(struct sched_stat_common));
	memset(&sched_para->bg, 0, sizeof(struct sched_stat_common));
	memset(&sched_para->sysbg, 0, sizeof(struct sched_stat_common));
}

void ohm_schedstats_record(int sched_type, struct task_struct *task, u64 delta_ms)
{
	struct sched_stat_para *sched_stat = &sched_para[sched_type];
	unsigned long flags;

	spin_lock_irqsave(&sched_stat->lock, flags);
	if (unlikely(!sched_stat->ctrl)) {
		spin_unlock_irqrestore(&sched_stat->lock, flags);
		return;
	}
	sched_stat->delta_ms = delta_ms;
	ohm_sched_stat_record_common(sched_stat, &sched_stat->all, delta_ms);

	if (test_task_fg(task)) {
		ohm_sched_stat_record_common(sched_stat, &sched_stat->fg, delta_ms);	
	}

	if (is_mi_vip_task(task)) {
		ohm_sched_stat_record_common(sched_stat, &sched_stat->ux, delta_ms);
	}

	if (test_task_top_app(task)) {
		ohm_sched_stat_record_common(sched_stat, &sched_stat->top, delta_ms);
	}

	if (rt_task(task)) 
	{
		ohm_sched_stat_record_common(sched_stat, &sched_stat->rt, delta_ms);
	}

	if (test_task_bg(task)) {
		ohm_sched_stat_record_common(sched_stat, &sched_stat->bg, delta_ms);
	}

	if (test_task_sys_bg(task)) {
		ohm_sched_stat_record_common(sched_stat, &sched_stat->sysbg, delta_ms);
	}
	spin_unlock_irqrestore(&sched_stat->lock, flags);
	return;
}

void ohm_para_init(void)
{
	int i;

	for (i = 0; i < OHM_SCHED_TOTAL; i++) {
		ohm_para_reset(&sched_para[i]);
		spin_lock_init(&sched_para[i].lock);
		sched_para[i].min_thresh_ms = 50;
		sched_para[i].low_thresh_ms = 100;
		sched_para[i].high_thresh_ms = 500;
        sched_para[i].ctrl = true;
	}

	return;
}

static inline ssize_t sched_data_to_user(char __user *buff, size_t count, loff_t *off, char *format_str, int len)
{
	if (len > *off) {
		len -= *off;
	} else {
		len = 0;
	}
	if (copy_to_user(buff, format_str, (len < count ? len : count))) {
		return -EFAULT;
	}
	*off += len < count ? len : count;

	return (len < count ? len : count);
}

static void clear_iowait_stats(void)
{
    ohm_para_reset(&sched_para[OHM_SCHED_IOWAIT]);
}

static void clear_dstate_stats(void)
{
    ohm_para_reset(&sched_para[OHM_SCHED_DSTATE]);
}

static ssize_t iowait_read(struct file *filp, char __user *buff, size_t count, loff_t *off)
{
	int len = 0;
	struct sched_stat_para *sched_stat = &sched_para[OHM_SCHED_IOWAIT];
	char *page = kzalloc(2048, GFP_KERNEL);
	unsigned long flags;

	if (!page)
		return -ENOMEM;
	spin_lock_irqsave(&sched_stat->lock, flags);
	len = LATENCY_STRING_FORMAT(page, iowait, sched_stat);
	spin_unlock_irqrestore(&sched_stat->lock, flags);

	if (len > *off) {
		len -= *off;
	} else {
		len = 0;
	}

	if (copy_to_user(buff, page, (len < count ? len : count))) {
		kfree(page);
	return -EFAULT;
	}
	kfree(page);
	*off += len < count ? len : count;

	return (len < count ? len : count);

}

static const struct proc_ops proc_iowait_fops = {
	.proc_read = iowait_read,
	.proc_lseek = default_llseek,
};

static int reset_iowait_show(struct seq_file *m, void *ptr)
{
	seq_printf(m,"%s\n", "Data Reset");
	return 0;
}

static ssize_t reset_iowait_store(void *priv, const char __user *buf, size_t count)
{
	int val;
	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	if (val) 
	{
		clear_iowait_stats();
	}

	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(reset_iowait);

static int reset_dstate_show(struct seq_file *m, void *ptr)
{
	seq_printf(m,"%s\n", "Data Reset");
	return 0;
}

static ssize_t reset_dstate_store(void *priv, const char __user *buf, size_t count)
{
	int val;
	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	if (val) 
	{
		clear_dstate_stats();
	}

	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(reset_dstate);

static ssize_t latency_dist_read(struct file *filp, char __user *buff,
			size_t count, loff_t *off)
{
#define LATENCY_BUF_MAX 4096
	char *page;
	int len = 0;
	int i;
	struct sched_stat_para *sched_stat;
	char * const format_str[] = {"1ms", "2ms", "5ms", "10ms", "20ms", "50ms",
		"100ms", "200ms", "500ms", "1000ms", "2000ms", "5000ms", "other"};
	ssize_t ret;

	page = kmalloc(LATENCY_BUF_MAX * sizeof(char), GFP_KERNEL);
	len += sprintf(page + len, "%-16s", "latency");
	for (i = 0; i < OHM_LATENCY_DIST_MAX; i++)
		len += sprintf(page + len, "%-10s", format_str[i]);
	len += sprintf(page + len, "\n");

	sched_stat = &sched_para[OHM_SCHED_IOWAIT];
	len += sprintf(page + len, "%-16s", "iowait_all:");
	for (i = 0; i < OHM_LATENCY_DIST_MAX; i++)
		len += sprintf(page + len, "%-10llu", sched_stat->all.latency_dist[i]);
	len += sprintf(page + len, "\n");

	len += sprintf(page + len, "%-16s", "iowait_fg:");
	for (i = 0; i < OHM_LATENCY_DIST_MAX; i++)
		len += sprintf(page + len, "%-10llu", sched_stat->fg.latency_dist[i]);
	len += sprintf(page + len, "\n");

	len += sprintf(page + len, "%-16s", "iowait_ux:");
	for (i = 0; i < OHM_LATENCY_DIST_MAX; i++)
		len += sprintf(page + len, "%-10llu", sched_stat->ux.latency_dist[i]);
	len += sprintf(page + len, "\n");

	len += sprintf(page + len, "%-16s", "iowait_rt:");
	for (i = 0; i < OHM_LATENCY_DIST_MAX; i++)
		len += sprintf(page + len, "%-10llu", sched_stat->rt.latency_dist[i]);
	len += sprintf(page + len, "\n");

	len += sprintf(page + len, "%-16s", "iowait_top:");
	for (i = 0; i < OHM_LATENCY_DIST_MAX; i++)
		len += sprintf(page + len, "%-10llu", sched_stat->top.latency_dist[i]);
	len += sprintf(page + len, "\n");

	len += sprintf(page + len, "%-16s", "iowait_bg:");
	for (i = 0; i < OHM_LATENCY_DIST_MAX; i++)
		len += sprintf(page + len, "%-10llu", sched_stat->bg.latency_dist[i]);
	len += sprintf(page + len, "\n");

	len += sprintf(page + len, "%-16s", "iowait_sysbg:");
	for (i = 0; i < OHM_LATENCY_DIST_MAX; i++)
		len += sprintf(page + len, "%-10llu", sched_stat->sysbg.latency_dist[i]);
	len += sprintf(page + len, "\n");

	if (len > LATENCY_BUF_MAX)
		len = LATENCY_BUF_MAX;

	ret = sched_data_to_user(buff, count, off, page, len);

	kfree(page);

	return ret;
}

static const struct proc_ops proc_latency_dist_fops = {
	.proc_read = latency_dist_read,
	.proc_lseek = default_llseek,
};

/****** dstat statistics  ******/
static ssize_t dstate_read(struct file *filp, char __user *buff, size_t count, loff_t *off)
{
	int len = 0;

	struct sched_stat_para *sched_stat = &sched_para[OHM_SCHED_DSTATE];
	char *page = kzalloc(2048, GFP_KERNEL);
	unsigned long flags;
	if (!page)
		return -ENOMEM;
	spin_lock_irqsave(&sched_stat->lock, flags);
	len = LATENCY_STRING_FORMAT(page, dstate, sched_stat);
	spin_unlock_irqrestore(&sched_stat->lock, flags);
	if (len > *off) {
		len -= *off;
	} else {
		len = 0;
	}
	if (copy_to_user(buff, page, (len < count ? len : count))) {
		kfree(page);
		return -EFAULT;
	}
	kfree(page);
	*off += len < count ? len : count;

	return (len < count ? len : count);
}

static const struct proc_ops proc_dstate_fops = {
	.proc_read = dstate_read,
	.proc_lseek = default_llseek,

};

static void probe_sched_stat_iowait_handler(void *data, struct task_struct *tsk, u64 delay)
{
	if (is_idle_task(tsk))
	{
		return;
	}

    if (delay >= IO_WAIT_LAT_THRESHOLD && is_critical_ux_task(tsk))
    {
        ////struct rq *rq = task_rq(tsk);
        ////u64 clock = rq_clock(rq);
		stack_trace_record_event(0, IO_WAIT_LAT, delay/1000000, tsk);
		record_kern_lat_event(IO_WAIT_LAT, delay/1000000, tsk);
    }

	if (!rt_task(tsk))
	{
		ohm_schedstats_record(OHM_SCHED_IOWAIT, tsk, (delay >> 20));
    }
}

static void probe_sched_stat_blocked(void *data, struct task_struct *tsk, u64 delay)
{
	if (is_idle_task(tsk))
	{
		return;
	}

	if (!rt_task(tsk))
	{
		ohm_schedstats_record(OHM_SCHED_DSTATE, tsk, (delay >> 20));
    }
}

static int register_waitinfo_vendor_hooks(void)
{
	int ret = 0;
	REGISTER_TRACE_VH(sched_stat_blocked, probe_sched_stat_blocked);
	REGISTER_TRACE_VH(sched_stat_iowait, probe_sched_stat_iowait_handler);

	return ret;
}

static int unregister_waitinfo_vendor_hooks(void)
{
	int ret = 0;
	UNREGISTER_TRACE_VH(sched_stat_blocked, probe_sched_stat_blocked);
	UNREGISTER_TRACE_VH(sched_stat_iowait, probe_sched_stat_iowait_handler);

	return ret;
}

int waitinfo_monitor_init(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *pentry;

	ohm_para_init();
	register_waitinfo_vendor_hooks();

	pentry = proc_create("latency_dist", S_IRUGO, parent,
			&proc_latency_dist_fops);
	if (!pentry) {
		printk("create latency dist failed.\n");
		goto remove_vendorhook;
	}

	pentry = proc_create("iowait", S_IRUGO, parent, &proc_iowait_fops);
	if (!pentry) {
		printk("create iowait proc failed.\n");
		goto remove_latency_dist;
	}

	pentry = proc_create("dstate", S_IRUGO, parent, &proc_dstate_fops);
	if (!pentry) {
		printk("create dstate proc failed.\n");
        goto remove_iowait;
	}

	pentry = proc_create("reset_iowait", S_IRUGO, parent, &reset_iowait_fops);
	if (!pentry) {
		printk("create reset_iowait proc failed.\n");
        goto remove_reset_dstate;
	}

	pentry = proc_create("reset_dstate", S_IRUGO, parent, &reset_dstate_fops);
	if (!pentry) {
		printk("create reset_dstate proc failed.\n");
        goto remove_reset_iowait;
	}	
	return 0;

remove_reset_iowait:
    remove_proc_entry("reset_iowait", parent);
	
remove_reset_dstate:
    remove_proc_entry("dstate", parent);

remove_iowait:
    remove_proc_entry("iowait", parent);	

remove_latency_dist:
    remove_proc_entry("latency_dist", parent);

remove_vendorhook:
    unregister_waitinfo_vendor_hooks();

	return -1;
}

void waitinfo_monitor_exit(struct proc_dir_entry *parent)
{
	unregister_waitinfo_vendor_hooks();
	if (parent) {
		remove_proc_entry("iowait", parent);
		remove_proc_entry("latency_dist", parent);
		remove_proc_entry("dstate", parent);
		remove_proc_entry("reset_iowait", parent);
		remove_proc_entry("reset_dstate", parent);
	}
}