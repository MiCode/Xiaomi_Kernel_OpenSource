// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/oom.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/signal.h>
#include <trace/hooks/vmscan.h>
#include <linux/printk.h>

static unsigned long panic_on_oom_timeout;
struct task_struct *saved_tsk;

#define PANIC_ON_OOM_DEFER_TIMEOUT (5*HZ)


static void readahead_set(void *data, gfp_t *flag)
{
	if (*flag & __GFP_MOVABLE) {
		*flag |= __GFP_CMA;
		*flag &= ~__GFP_HIGHMEM;
	}
}

static void gfp_zone_set(void *data, gfp_t *flag)
{
	if (!IS_ENABLED(CONFIG_HIGHMEM)) {
		if ((*flag & __GFP_MOVABLE) && !(*flag & __GFP_CMA))
			*flag &= ~__GFP_HIGHMEM;
	}
}

static void set_swap_cache(void *data, gfp_t *flag)
{
	*flag |= __GFP_CMA;
}

static void reap_eligible(void *data, struct task_struct *task, bool *reap)
{
	/* TODO: Can this logic be moved to module params approach? */
	if (!strcmp(task->comm, "lmkd") || !strcmp(task->comm, "PreKillActionT"))
		*reap = true;
}

static void __oom_panic_defer(void *data, struct oom_control *oc, int *val)
{
	int ret = 0;
	struct task_struct *p;

	if (oc->chosen)
		goto out;

	rcu_read_lock();
	for_each_process(p) {
		if (tsk_is_oom_victim(p))
			break;
	}
	rcu_read_unlock();

	if (p == &init_task)
		goto out;

	if (p != saved_tsk) {
		panic_on_oom_timeout = jiffies + PANIC_ON_OOM_DEFER_TIMEOUT;
		saved_tsk = p;
		ret = -1;
	} else if (time_before_eq(jiffies, panic_on_oom_timeout)) {
		ret = -1;
	}

out:
	*val = ret;
}

static void balance_reclaim(void *unused, bool *balance_anon_file_reclaim)
{
	*balance_anon_file_reclaim = true;
}

static int __init init_mem_hooks(void)
{
	int ret;

	ret = register_trace_android_rvh_set_readahead_gfp_mask(readahead_set, NULL);
	if (ret) {
		pr_err("Failed to register readahead_gfp_mask hooks\n");
		return ret;
	}

	ret = register_trace_android_rvh_set_gfp_zone_flags(gfp_zone_set, NULL);
	if (ret) {
		pr_err("Failed to register gfp_zone_flags hooks\n");
		return ret;
	}

	ret = register_trace_android_rvh_set_skip_swapcache_flags(set_swap_cache, NULL);
	if (ret) {
		pr_err("Failed to register skip_swapcache_flags hooks\n");
		return ret;
	}

	ret = register_trace_android_vh_process_killed(reap_eligible, NULL);
	if (ret) {
		pr_err("Failed to register process_killed hooks\n");
		return ret;
	}

	ret = register_trace_android_vh_oom_check_panic(__oom_panic_defer,
							NULL);
	if (ret) {
		pr_err("Failed to register oom_check_panic hooks\n");
		return ret;
	}

	if (IS_ENABLED(CONFIG_QCOM_BALANCE_ANON_FILE_RECLAIM)) {
		ret = register_trace_android_rvh_set_balance_anon_file_reclaim(balance_reclaim,
							NULL);
		if (ret) {
			pr_err("Failed to register balance_anon_file_reclaim hooks\n");
			return ret;
		}
	}
	return 0;
}

void exit_mem_hooks(void)
{
	unregister_trace_android_vh_oom_check_panic(
			__oom_panic_defer, NULL);
}

module_init(init_mem_hooks);
module_exit(exit_mem_hooks);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Memory Trace Hook Call-Back Registration");
MODULE_LICENSE("GPL v2");
