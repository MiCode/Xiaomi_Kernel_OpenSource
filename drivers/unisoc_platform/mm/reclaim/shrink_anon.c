// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 Unisoc Communications Inc.
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <trace/hooks/vmscan.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/memcontrol.h>
#include <linux/sched/loadavg.h>
#include <linux/cpufreq.h>
#include <linux/vmstat.h>
#include <linux/wait.h>
#include <linux/unisoc_vd_def.h>
#include <linux/shrink_anon.h>
#include "reclaim.h"

#define SHRINK_ANON_NAME "shrink_anon"
#define SCAN_ANON 2

static atomic_t display_off = ATOMIC_LONG_INIT(0);
static struct task_struct *shrink_anon_thread;
static int empty_round_count;
static unsigned long sysctl_mem_free_pages_limit = 25600;
static int sysctl_cpuload_limit = 40;
static unsigned long sysctl_reclaim_pages_per_cycle = 2560;
static unsigned long long sysctl_shrink_anon_exceed_ms = 10000;
static unsigned long sysctl_empty_round_check_pages = 256;
static int sysctl_max_empty_round_sleep_ms = 300000;

static struct ctl_table_header *shrink_anon_table_header;
wait_queue_head_t shrink_anon_wait;

struct ctl_table mm_child_table[] = {
	{
		.procname	= "mem_free_pages_limit",
		.data		= &sysctl_mem_free_pages_limit,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname	= "cpuload_limit",
		.data		= &sysctl_cpuload_limit,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2         = SYSCTL_INT_MAX,
	},
	{
		.procname	= "reclaim_pages_per_cycle",
		.data		= &sysctl_reclaim_pages_per_cycle,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2         = SYSCTL_INT_MAX,
	},
	{
		.procname	= "shrink_anon_exceed_ms",
		.data		= &sysctl_shrink_anon_exceed_ms,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2         = SYSCTL_INT_MAX,
	},
	{
		.procname	= "empty_round_check_pages",
		.data		= &sysctl_empty_round_check_pages,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2         = SYSCTL_INT_MAX,
	},
	{
		.procname	= "max_empty_round_sleep_ms",
		.data		= &sysctl_max_empty_round_sleep_ms,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2         = SYSCTL_INT_MAX,
	},
	{ },
};

struct ctl_table shrink_anon_table[] = {
	{
		.procname	= "unisoc_shrink_anon",
		.mode		= 0555,
		.child		= mm_child_table,
	},
	{ },
};

static bool mem_free_pages_is_ok(void)
{
	unsigned long cur_free_pages = global_zone_page_state(NR_FREE_PAGES);

	if (cur_free_pages >= sysctl_mem_free_pages_limit)
		return true;

	return false;
}

static unsigned long memcg_shrink_anon(void)
{
	struct mem_cgroup *memcg = NULL;
	unsigned long nr_reclaimed_pages;

	unsigned long nr_to_reclaim_pages = sysctl_reclaim_pages_per_cycle;

	memcg = root_mem_cgroup;

	nr_reclaimed_pages = try_to_free_mem_cgroup_pages(memcg, nr_to_reclaim_pages,
							GFP_KERNEL, true);

	return nr_reclaimed_pages;
}

static bool is_cpu_busy(void)
{
	int cpu, cpuload;

	for_each_possible_cpu(cpu) {
		if (cpu != smp_processor_id()) {
			cpuload = sched_get_cpu_util_pct(cpu);
			if (cpuload > sysctl_cpuload_limit)
				return true;
		}
	}

	return false;
}

static unsigned long shrink_anon_node(void)
{
	unsigned long nr_reclaimed = 0;
	unsigned long before_avail_pages = 0;
	unsigned long after_avail_pages = 0;
	unsigned long empty_round_check_pages = 0;
	unsigned long sleep_time_ms;
	int max_empty_round_sleep_time;

	empty_round_check_pages = sysctl_empty_round_check_pages;
	max_empty_round_sleep_time = sysctl_max_empty_round_sleep_ms;
	sleep_time_ms = sysctl_shrink_anon_exceed_ms;

	if (mem_free_pages_is_ok())
		return sleep_time_ms;

	if (is_cpu_busy())
		return sleep_time_ms;

	before_avail_pages = global_zone_page_state(NR_FREE_PAGES);
	nr_reclaimed = memcg_shrink_anon();

	if (nr_reclaimed < empty_round_check_pages) {
		empty_round_count++;
		sleep_time_ms = min(2000 * empty_round_count, max_empty_round_sleep_time);
	} else {
		empty_round_count = 0;
	}

	after_avail_pages = global_zone_page_state(NR_FREE_PAGES);
	pr_info("shrink_anon:before_memfree:%lu, reclaimed:%lu, after_memfree:%lu, sleep_time:%lu",
		before_avail_pages, nr_reclaimed, after_avail_pages, sleep_time_ms);

	return sleep_time_ms;

}

static void shrink_anon_bind_cpu(void)
{
	struct cpumask cpumask = { CPU_BITS_NONE };
	unsigned int cpu = 0;
	unsigned long cpu_cap, cpuid_cap = arch_scale_cpu_capacity(0);
	static bool shrink_anon_bind_cpus_success;

	if (likely(shrink_anon_bind_cpus_success))
		return;

	for_each_possible_cpu(cpu) {
		cpu_cap = arch_scale_cpu_capacity(cpu);

		if (cpu_cap == cpuid_cap)
			cpumask_set_cpu(cpu, &cpumask);
	}

	if (!cpumask_empty(&cpumask)) {
		set_cpus_allowed_ptr(current, &cpumask);
		shrink_anon_bind_cpus_success = true;
	}
}

static int shrink_anon_func(void *p)
{
	unsigned long shrink_anon_delay_ms;

	while (!kthread_should_stop()) {
		shrink_anon_bind_cpu();
		wait_event_freezable(shrink_anon_wait,
				(atomic_read(&(display_off)) == 0));
		shrink_anon_delay_ms = shrink_anon_node();

		if (!atomic_read(&display_off))
			msleep(shrink_anon_delay_ms);
	}

	return 1;
}

static void start_shrink_anon_thread(void)
{
	if (shrink_anon_thread)
		return;

	init_waitqueue_head(&shrink_anon_wait);

	shrink_anon_thread = kthread_run(shrink_anon_func, NULL, SHRINK_ANON_NAME);
	if (IS_ERR(shrink_anon_thread)) {
		pr_warn("Failed to create the shrink_anon thread\n");
		shrink_anon_thread = NULL;
	}
}

static void stop_shrink_anon_thread(void)
{
	if (shrink_anon_thread) {
		kthread_stop(shrink_anon_thread);
		shrink_anon_thread = NULL;
	}
}

static int shrink_anon_call(struct notifier_block *nb,
			unsigned long val, void *data)
{
	int *input = data;

	if (!data) {
		pr_info("data is NULL");
		return 0;
	}

	if (*input == 0) {
		atomic_set(&display_off, 0);
		wake_up_interruptible(&shrink_anon_wait);
	} else if (*input == 1) {
		atomic_set(&display_off, 1);
	}

	return 1;
}

static struct notifier_block shrink_anon_notifier = {
	.notifier_call = shrink_anon_call,
};

static void unisoc_tune_scan_type(void *data, char *scan_balance)
{
	if (strstr(current->comm, SHRINK_ANON_NAME)) {
		*scan_balance = SCAN_ANON;
	}
}

void shrink_anon_init(void)
{
	int ret = 0;

	register_trace_android_vh_tune_scan_type(unisoc_tune_scan_type, NULL);

	ret = register_unisoc_shrink_anon_notifier(&shrink_anon_notifier);
	if (ret)
		pr_info("failed to register unisoc_shrink_anon_notifier: %d", ret);

	if (!shrink_anon_table_header) {
		shrink_anon_table_header = register_sysctl_table(shrink_anon_table);
		if (unlikely(!shrink_anon_table_header))
			pr_info("failed register to /proc/sys/unisoc_shrink_anon\n");
	}

	start_shrink_anon_thread();

	pr_info("shrink_anon init succeed!");
}

void shrink_anon_exit(void)
{
	unregister_trace_android_vh_tune_scan_type(unisoc_tune_scan_type, NULL);

	if (unregister_unisoc_shrink_anon_notifier(&shrink_anon_notifier))
		pr_info("failed to unregister unisoc_shrink_anon_notifier");

	if (shrink_anon_table_header) {
		unregister_sysctl_table(shrink_anon_table_header);
		shrink_anon_table_header = NULL;
	}

	stop_shrink_anon_thread();
	pr_info("shrink_anon exit succeed!");
}
