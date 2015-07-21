/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/hrtimer.h>
#include <linux/debugfs.h>
#include <linux/ktime.h>
#include <trace/events/power.h>
#include "trace_stat.h"
#include "trace.h"

struct trans {
	struct rb_node node;
	unsigned int cpu;
	unsigned int start_freq;
	unsigned int end_freq;
	unsigned int min_us;
	unsigned int max_us;
	ktime_t total_t;
	unsigned int count;
};
static struct rb_root freq_trans_tree = RB_ROOT;

static struct trans *tr_search(struct rb_root *root, unsigned int cpu,
			       unsigned int start_freq, unsigned int end_freq)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct trans *tr = container_of(node, struct trans, node);

		if (cpu < tr->cpu)
			node = node->rb_left;
		else if (cpu > tr->cpu)
			node = node->rb_right;
		else if (start_freq < tr->start_freq)
			node = node->rb_left;
		else if (start_freq > tr->start_freq)
			node = node->rb_right;
		else if (end_freq < tr->end_freq)
			node = node->rb_left;
		else if (end_freq > tr->end_freq)
			node = node->rb_right;
		else
			return tr;
	}
	return NULL;
}

static int tr_insert(struct rb_root *root, struct trans *tr)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;

	while (*new) {
		struct trans *this = container_of(*new, struct trans, node);

		parent = *new;
		if (tr->cpu < this->cpu)
			new = &((*new)->rb_left);
		else if (tr->cpu > this->cpu)
			new = &((*new)->rb_right);
		else if (tr->start_freq < this->start_freq)
			new = &((*new)->rb_left);
		else if (tr->start_freq > this->start_freq)
			new = &((*new)->rb_right);
		else if (tr->end_freq < this->end_freq)
			new = &((*new)->rb_left);
		else if (tr->end_freq > this->end_freq)
			new = &((*new)->rb_right);
		else
			return -EINVAL;
	}

	rb_link_node(&tr->node, parent, new);
	rb_insert_color(&tr->node, root);

	return 0;
}

struct trans_state {
	spinlock_t lock;
	unsigned int start_freq;
	unsigned int end_freq;
	ktime_t start_t;
	bool started;
};
static DEFINE_PER_CPU(struct trans_state, freq_trans_state);

static DEFINE_SPINLOCK(state_lock);

static void probe_start(void *ignore, unsigned int start_freq,
			unsigned int end_freq, unsigned int cpu)
{
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	per_cpu(freq_trans_state, cpu).start_freq = start_freq;
	per_cpu(freq_trans_state, cpu).end_freq = end_freq;
	per_cpu(freq_trans_state, cpu).start_t = ktime_get();
	per_cpu(freq_trans_state, cpu).started = true;
	spin_unlock_irqrestore(&state_lock, flags);
}

static void probe_end(void *ignore, unsigned int cpu)
{
	unsigned long flags;
	struct trans *tr;
	s64 dur_us;
	ktime_t dur_t, end_t = ktime_get();

	spin_lock_irqsave(&state_lock, flags);

	if (!per_cpu(freq_trans_state, cpu).started)
		goto out;

	dur_t = ktime_sub(end_t, per_cpu(freq_trans_state, cpu).start_t);
	dur_us = ktime_to_us(dur_t);

	tr = tr_search(&freq_trans_tree, cpu,
		       per_cpu(freq_trans_state, cpu).start_freq,
		       per_cpu(freq_trans_state, cpu).end_freq);
	if (!tr) {
		tr = kzalloc(sizeof(*tr), GFP_ATOMIC);
		if (!tr) {
			WARN_ONCE(1, "CPU frequency trace is now invalid!\n");
			goto out;
		}

		tr->start_freq = per_cpu(freq_trans_state, cpu).start_freq;
		tr->end_freq = per_cpu(freq_trans_state, cpu).end_freq;
		tr->cpu = cpu;
		tr->min_us = UINT_MAX;
		tr_insert(&freq_trans_tree, tr);
	}
	tr->total_t = ktime_add(tr->total_t, dur_t);
	tr->count++;

	if (dur_us > tr->max_us)
		tr->max_us = dur_us;
	if (dur_us < tr->min_us)
		tr->min_us = dur_us;

	per_cpu(freq_trans_state, cpu).started = false;
out:
	spin_unlock_irqrestore(&state_lock, flags);
}

static void *freq_switch_stat_start(struct tracer_stat *trace)
{
	struct rb_node *n;
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	n = rb_first(&freq_trans_tree);
	spin_unlock_irqrestore(&state_lock, flags);

	return n;
}

static void *freq_switch_stat_next(void *prev, int idx)
{
	struct rb_node *n;
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	n = rb_next(prev);
	spin_unlock_irqrestore(&state_lock, flags);

	return n;
}

static int freq_switch_stat_show(struct seq_file *s, void *p)
{
	unsigned long flags;
	struct trans *tr = p;

	spin_lock_irqsave(&state_lock, flags);
	seq_printf(s, "%3d %9d %8d %5d %6lld %6d %6d\n", tr->cpu,
		   tr->start_freq, tr->end_freq, tr->count,
		   div_s64(ktime_to_us(tr->total_t), tr->count),
		   tr->min_us, tr->max_us);
	spin_unlock_irqrestore(&state_lock, flags);

	return 0;
}

static void freq_switch_stat_release(void *stat)
{
	struct trans *tr = stat;
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	rb_erase(&tr->node, &freq_trans_tree);
	spin_unlock_irqrestore(&state_lock, flags);
	kfree(tr);
}

static int freq_switch_stat_headers(struct seq_file *s)
{
	seq_puts(s, "CPU START_KHZ  END_KHZ COUNT AVG_US MIN_US MAX_US\n");
	seq_puts(s, "  |         |        |     |      |      |      |\n");
	return 0;
}

struct tracer_stat freq_switch_stats __read_mostly = {
	.name = "cpu_freq_switch",
	.stat_start = freq_switch_stat_start,
	.stat_next = freq_switch_stat_next,
	.stat_show = freq_switch_stat_show,
	.stat_release = freq_switch_stat_release,
	.stat_headers = freq_switch_stat_headers
};

static void trace_freq_switch_disable(void)
{
	unregister_stat_tracer(&freq_switch_stats);
	unregister_trace_cpu_frequency_switch_end(probe_end, NULL);
	unregister_trace_cpu_frequency_switch_start(probe_start, NULL);
	pr_info("disabled cpu frequency switch time profiling\n");
}

static int trace_freq_switch_enable(void)
{
	int ret;

	ret = register_trace_cpu_frequency_switch_start(probe_start, NULL);
	if (ret)
		goto out;

	ret = register_trace_cpu_frequency_switch_end(probe_end, NULL);
	if (ret)
		goto err_register_switch_end;

	ret = register_stat_tracer(&freq_switch_stats);
	if (ret)
		goto err_register_stat_tracer;

	pr_info("enabled cpu frequency switch time profiling\n");
	return 0;

err_register_stat_tracer:
	unregister_trace_cpu_frequency_switch_end(probe_end, NULL);
err_register_switch_end:
	register_trace_cpu_frequency_switch_start(probe_start, NULL);
out:
	pr_err("failed to enable cpu frequency switch time profiling\n");

	return ret;
}

static DEFINE_MUTEX(debugfs_lock);
static bool trace_freq_switch_enabled;

static int debug_toggle_tracing(void *data, u64 val)
{
	int ret = 0;

	mutex_lock(&debugfs_lock);

	if (val == 1 && !trace_freq_switch_enabled)
		ret = trace_freq_switch_enable();
	else if (val == 0 && trace_freq_switch_enabled)
		trace_freq_switch_disable();
	else if (val > 1)
		ret = -EINVAL;

	if (!ret)
		trace_freq_switch_enabled = val;

	mutex_unlock(&debugfs_lock);

	return ret;
}

static int debug_tracing_state_get(void *data, u64 *val)
{
	mutex_lock(&debugfs_lock);
	*val = trace_freq_switch_enabled;
	mutex_unlock(&debugfs_lock);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(debug_tracing_state_fops, debug_tracing_state_get,
			debug_toggle_tracing, "%llu\n");

static int __init trace_freq_switch_init(void)
{
	struct dentry *d_tracer = tracing_init_dentry();

	if (!d_tracer)
		return 0;

	debugfs_create_file("cpu_freq_switch_profile_enabled",
		S_IRUGO | S_IWUSR, d_tracer, NULL, &debug_tracing_state_fops);

	return 0;
}
late_initcall(trace_freq_switch_init);
