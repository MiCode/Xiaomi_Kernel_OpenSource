// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Unisoc Inc.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "vmscan_stat: " fmt

#include <linux/proc_fs.h>
#include <linux/fortify-string.h>
#include <linux/math64.h>
#include <linux/mmzone.h>
#include <linux/sched/clock.h>
#include <linux/swap.h>
#include <linux/unisoc_vd_def.h>
#include <trace/events/vmscan.h>
#include "data_collector_core.h"

#define CREATE_TRACE_POINTS

#define NS_IN_A_SEC	1000000000
#define KSWAPD_COMM_NAME	"kswapd0"
#define KSWAPD_CON_HEAD	"kswapd_consumption:"
#define DR_CON_HEAD	"dr_consumption:"
#define KSWAPD_CON_NODE	"kswapd_consumption"
#define DR_CON_NODE	"direct_reclaim_consumption"

#define NODE_BUF_SIZE	4096
#define TEMP_BUF_LEN	96

#define CPU_CONSUMPTION_RATE_THRESHOLD 50

/**
 * struct cpu_consumption_stat - a statistic window to monitor a specified activity
 * @win_start: start time of statistic window
 * @win_count: window sequence count
 * @whole_irq_dur_in_win: this activity's duration for this stat window
 */
struct cpu_consumption_stat {
	unsigned long long	win_start;
	unsigned long long	win_count;
	unsigned long long	whole_dur_in_win;
} ____cacheline_internodealigned_in_smp;

struct dr_timing_record {
	unsigned long long	dur_in_win;
	unsigned long long	win_count;
} ____cacheline_internodealigned_in_smp;

static DEFINE_PER_CPU(struct dr_timing_record, dr_timing_rec);

/**
 * kswapd statistic variables
 */
static unsigned long long kswapd_start_sum;
static struct cpu_consumption_stat mem_stat;
static DEFINE_SPINLOCK(mem_spinlock);
static struct task_struct *p_kswapd_task;

/**
 * direct reclaim statistic variables
 */
static struct cpu_consumption_stat dr_stat;
static DEFINE_SPINLOCK(dr_spinlock);

/**
 * exported nodes buffers
 */
static char kswapd_stat_buf[NODE_BUF_SIZE];
static char dr_stat_buf[NODE_BUF_SIZE];
static char kswapd_tmp_buf[NODE_BUF_SIZE];
static char dr_tmp_buf[NODE_BUF_SIZE];
static unsigned int kswapd_stat_index;
static unsigned int dr_stat_index;

static void flush_node_buf(char *p_buf, char *p_head, unsigned int *p_index)
{
	if (!p_buf || !p_head || !p_index)
		return;

	strscpy(p_buf, p_head, strlen(p_head) + 1);
	*p_index = strlen(p_head);
}

static int write_sample_to_buf(char *p_buf, char *p_content, unsigned int *p_index)
{
	int ret = -1;

	if (!p_buf || !p_content || !p_index)
		return ret;
	if (*p_index + strlen(p_content) >= NODE_BUF_SIZE - 1)
		return ret;

	ret = 0;
	strscpy(p_buf + *p_index, p_content, strlen(p_content) + 1);
	*p_index += strlen(p_content);

	return ret;
}

static ssize_t kswapd_stat_read(struct file *file, char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret = -1;

	if (spin_trylock_irq(&mem_spinlock)) {
		ret = 0;
		strscpy(kswapd_tmp_buf, kswapd_stat_buf, strlen(kswapd_stat_buf) + 1);
		flush_node_buf(kswapd_stat_buf, KSWAPD_CON_HEAD, &kswapd_stat_index);
		spin_unlock_irq(&mem_spinlock);
	}
	if (!ret) {
		ret = simple_read_from_buffer(user_buf, count, ppos,
		kswapd_tmp_buf, strlen(kswapd_tmp_buf) + 1);
	}
	return ret;
}

static ssize_t dr_stat_read(struct file *file, char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret = -1;

	if (spin_trylock_irq(&dr_spinlock)) {
		ret = 0;
		strscpy(dr_tmp_buf, dr_stat_buf, strlen(dr_stat_buf) + 1);
		flush_node_buf(dr_stat_buf, DR_CON_HEAD, &dr_stat_index);
		spin_unlock_irq(&dr_spinlock);
	}
	if (!ret) {
		ret = simple_read_from_buffer(user_buf, count, ppos,
				dr_tmp_buf, strlen(dr_tmp_buf) + 1);
	}
	return ret;
}

static const struct proc_ops kswapd_stat_fops = {
	.proc_open	= simple_open,
	.proc_read	= kswapd_stat_read,
	.proc_lseek	= default_llseek,
};

static const struct proc_ops dr_stat_fops = {
	.proc_open	= simple_open,
	.proc_read	= dr_stat_read,
	.proc_lseek	= default_llseek,
};

static int __init node_init(void)
{
	int ret = 0;

	flush_node_buf(kswapd_stat_buf, KSWAPD_CON_HEAD, &kswapd_stat_index);
	flush_node_buf(dr_stat_buf, DR_CON_HEAD, &dr_stat_index);

	if (!proc_create(KSWAPD_CON_NODE, 0660, NULL, &kswapd_stat_fops)) {
		ret = -ENOMEM;
		goto err_kswapd_node;
	}

	if (!proc_create(DR_CON_NODE, 0660, NULL, &dr_stat_fops)) {
		ret = -ENOMEM;
		goto err_dr_node;
	}

	return ret;

err_dr_node:
	remove_proc_entry(KSWAPD_CON_NODE, NULL);
err_kswapd_node:
	return ret;
}

static void __exit node_deinit(void)
{
	remove_proc_entry(KSWAPD_CON_NODE, NULL);
	remove_proc_entry(DR_CON_NODE, NULL);
}

static u64 do_the_div(u64 dividend, u64 divisor)
{
	u64 tmp;

	return div64_u64_rem(dividend, divisor, &tmp);
}

void period_mem_stat_check(void)
{
	char temp_buf[TEMP_BUF_LEN];
	unsigned long long kswapd_end;
	unsigned long long kswapd_real_win;
	unsigned long long consumption_rate;
	unsigned long long dr_end;
	unsigned long long dr_real_win;
	unsigned long long cur_time;
	size_t len;
	int ret;

	mem_stat.whole_dur_in_win = p_kswapd_task->se.sum_exec_runtime - kswapd_start_sum;
	if (mem_stat.whole_dur_in_win > 0) {
		kswapd_end = sched_clock();
		kswapd_real_win = kswapd_end - mem_stat.win_start;

		consumption_rate = do_the_div(mem_stat.whole_dur_in_win * 100, kswapd_real_win);
		if (consumption_rate > CPU_CONSUMPTION_RATE_THRESHOLD)
			pr_info(
			"KSWAPD_STAT: dur = %lld ns, win = %lld, rate = %lld, w_count = %lld\n",
			mem_stat.whole_dur_in_win, kswapd_real_win,
			consumption_rate, mem_stat.win_count);

		mem_stat.win_start = kswapd_end;
		mem_stat.win_count += 1;
		kswapd_start_sum = p_kswapd_task->se.sum_exec_runtime;
		cur_time = do_the_div(kswapd_end, NS_IN_A_SEC);

		len = sprintf(temp_buf, " %lld:%lld:%lld:%lld\n", cur_time,
				mem_stat.whole_dur_in_win, kswapd_real_win, consumption_rate);
		spin_lock(&mem_spinlock);
		ret = write_sample_to_buf(kswapd_stat_buf, temp_buf, &kswapd_stat_index);
		if (ret) {
			flush_node_buf(kswapd_stat_buf, KSWAPD_CON_HEAD, &kswapd_stat_index);
			write_sample_to_buf(kswapd_stat_buf, temp_buf, &kswapd_stat_index);
		}
		spin_unlock(&mem_spinlock);
	}

	spin_lock(&dr_spinlock);
	if (!dr_stat.whole_dur_in_win)
		goto dr_lock_end;

	dr_end = sched_clock();
	dr_real_win = dr_end - dr_stat.win_start;

	if (dr_real_win >= ONE_SEC_IN_NS) {
		dr_stat.win_count += 1;
		dr_stat.win_start = dr_end;
		consumption_rate = do_the_div(dr_stat.whole_dur_in_win * 100, dr_real_win);
		if (consumption_rate > CPU_CONSUMPTION_RATE_THRESHOLD)
			pr_info(
			"DIRECT_R_STAT: dur = %lld ns, win = %lld, rate = %lld, w_count = %lld\n",
			dr_stat.whole_dur_in_win,
			dr_real_win, consumption_rate, dr_stat.win_count);
		cur_time = do_the_div(dr_end, NS_IN_A_SEC);

		len = sprintf(temp_buf, " %lld:%lld:%lld:%lld\n",
					cur_time, dr_stat.whole_dur_in_win,
					dr_real_win, consumption_rate);
		dr_stat.whole_dur_in_win = 0;
		ret = write_sample_to_buf(dr_stat_buf, temp_buf, &dr_stat_index);
		if (ret) {
			flush_node_buf(dr_stat_buf, DR_CON_HEAD, &dr_stat_index);
			write_sample_to_buf(dr_stat_buf, temp_buf, &dr_stat_index);
		}
	}
dr_lock_end:
	spin_unlock(&dr_spinlock);
}

void __init vmscan_statistic_init(void)
{
	pg_data_t *pgdat = NODE_DATA(0);

	mem_stat.win_start = sched_clock();
	dr_stat.win_start = sched_clock();

	if (pgdat->kswapd) {
		p_kswapd_task = pgdat->kswapd;
		kswapd_start_sum = p_kswapd_task->se.sum_exec_runtime;
	} else
		pr_err("kswapd task not found! impossilbe..\n");

	node_init();
}

static void trace_direct_reclaim_begin(void *data, int order, gfp_t gfp_mask)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *) current->android_vendor_data1;

	if (unlikely(current_is_kswapd())) {
		pr_info("DIRECT_R_STAT: direct reclaim at kswapd0 thread!\n");
		return;
	}

	uni_tsk->dr_thread_timestamp_start = sched_clock();
	uni_tsk->dr_thread_duration = 0x0;
	uni_tsk->dr_thread_in_statistic = 1;
}

static void trace_direct_reclaim_end(void *data, unsigned long nr_reclaimed)
{
	char temp_buf[TEMP_BUF_LEN];
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *) current->android_vendor_data1;
	struct dr_timing_record *p_dr_t_rec = NULL;
	int cur_cpu = smp_processor_id();
	unsigned int cpu;
	unsigned long long end;
	unsigned long long dur;
	unsigned long long real_win;
	unsigned long long consumption_rate;
	unsigned long long cur_time;
	unsigned long flags;
	size_t len;
	int ret;

	if (unlikely(current_is_kswapd()))
		return;

	uni_tsk->dr_thread_in_statistic = 0;
	end = sched_clock();
	dur = end - uni_tsk->dr_thread_timestamp_start;
	uni_tsk->dr_thread_duration += dur;
	p_dr_t_rec = &per_cpu(dr_timing_rec, cur_cpu);

	spin_lock_irqsave(&dr_spinlock, flags);

	do {
		if (p_dr_t_rec->win_count < dr_stat.win_count)
			p_dr_t_rec->win_count = dr_stat.win_count;

		p_dr_t_rec->dur_in_win += uni_tsk->dr_thread_duration;
		if (end <= dr_stat.win_start)
			break;

		real_win = end - dr_stat.win_start;

		if (real_win >= ONE_SEC_IN_NS) {
			dr_stat.win_count += 1;
			p_dr_t_rec->win_count = dr_stat.win_count;
			dr_stat.win_start = end;
			for_each_possible_cpu(cpu) {
				dr_stat.whole_dur_in_win += per_cpu(dr_timing_rec, cpu).dur_in_win;
				per_cpu(dr_timing_rec, cpu).dur_in_win = 0;
			}
			consumption_rate = do_the_div(dr_stat.whole_dur_in_win * 100, real_win);
			if (consumption_rate > CPU_CONSUMPTION_RATE_THRESHOLD)
				pr_info(
				"DIRECT_R_STAT: dur = %lld ns, win = %lld, rate = %lld, w_count = %lld\n",
				dr_stat.whole_dur_in_win,
				real_win, consumption_rate, dr_stat.win_count);
			cur_time = do_the_div(end, NS_IN_A_SEC);

			len = sprintf(temp_buf, " %lld:%lld:%lld:%lld\n",
						cur_time, dr_stat.whole_dur_in_win,
						real_win, consumption_rate);
			dr_stat.whole_dur_in_win = 0;
			ret = write_sample_to_buf(dr_stat_buf, temp_buf, &dr_stat_index);
			if (ret) {
				flush_node_buf(dr_stat_buf, DR_CON_HEAD, &dr_stat_index);
				write_sample_to_buf(dr_stat_buf, temp_buf, &dr_stat_index);
			}
		}
	} while (0);

	spin_unlock_irqrestore(&dr_spinlock, flags);
}

void __init unregister_vmscan_trace_at_init(void)
{
	unregister_trace_mm_vmscan_direct_reclaim_begin(trace_direct_reclaim_begin, NULL);
	unregister_trace_mm_vmscan_direct_reclaim_end(trace_direct_reclaim_end, NULL);
}

int __init vmscan_trace_register(void)
{
	int ret;

/* mm/vmscan.c:
 *
 * EXPORT_TRACEPOINT_SYMBOL_GPL(mm_vmscan_direct_reclaim_begin);
 * EXPORT_TRACEPOINT_SYMBOL_GPL(mm_vmscan_direct_reclaim_end);
 */
	ret = register_trace_mm_vmscan_direct_reclaim_begin(trace_direct_reclaim_begin, NULL);
	if (ret)
		goto out_err;
	pr_info("D_COLLECTOR: register trace_mm_vmscan_direct_reclaim_begin done!\n");

	ret = register_trace_mm_vmscan_direct_reclaim_end(trace_direct_reclaim_end, NULL);
	if (ret)
		goto out_unregister_entry1;
	pr_info("D_COLLECTOR: register trace_mm_vmscan_direct_reclaim_end done!\n");

	return 0;

out_unregister_entry1:
	unregister_trace_mm_vmscan_direct_reclaim_begin(trace_direct_reclaim_begin, NULL);
out_err:
	return -1;
}
