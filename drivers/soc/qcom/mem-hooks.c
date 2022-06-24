// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/oom.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/vmscan.h>
#include <linux/printk.h>
#include <linux/nodemask.h>
#include <linux/kthread.h>
#include <linux/swap.h>

static unsigned long panic_on_oom_timeout;
struct task_struct *saved_tsk;
static uint kswapd_threads;
module_param_named(kswapd_threads, kswapd_threads, uint, 0644);

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

static void __maybe_unused __oom_panic_defer(void *data, struct oom_control *oc, int *val)
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

static int kswapd_per_node_run(int nid, unsigned int kswapd_threads)
{
	pg_data_t *pgdat = NODE_DATA(nid);
	unsigned int hid, start = 0;
	int ret = 0;

	if (pgdat->kswapd) {
		start = 1;
		pgdat->mkswapd[0] = pgdat->kswapd;
	}

	for (hid = start; hid < kswapd_threads; ++hid) {
		pgdat->mkswapd[hid] = kthread_run(kswapd, pgdat, "kswapd%d:%d",
								nid, hid);
		if (IS_ERR(pgdat->mkswapd[hid])) {
			/* failure at boot is fatal */
			WARN_ON(system_state < SYSTEM_RUNNING);
			pr_err("Failed to start kswapd%d on node %d\n",
				hid, nid);
			ret = PTR_ERR(pgdat->mkswapd[hid]);
			pgdat->mkswapd[hid] = NULL;
			continue;
		}
		if (!pgdat->kswapd)
			pgdat->kswapd = pgdat->mkswapd[hid];
	}
	return ret;
}

static void kswapd_per_node_stop(int nid, unsigned int kswapd_threads)
{
	int hid = 0;
	struct task_struct *kswapd;

	for (hid = 0; hid < kswapd_threads; hid++) {
		kswapd = NODE_DATA(nid)->mkswapd[hid];
		if (kswapd) {
			kthread_stop(kswapd);
			NODE_DATA(nid)->mkswapd[hid] = NULL;
		}
	}
	NODE_DATA(nid)->kswapd = NULL;
}

static void kswapd_threads_set(void *unused, int nid, bool *skip, bool run)
{
	*skip = true;
	if (run)
		kswapd_per_node_run(nid, kswapd_threads);
	else
		kswapd_per_node_stop(nid, kswapd_threads);

}

static int init_kswapd_per_node_hook(void)
{
	int ret = 0;
	int nid;

	if (kswapd_threads > MAX_KSWAPD_THREADS) {
		pr_err("Failed to set kswapd_threads to %d ,Max limit is %d\n",
				kswapd_threads, MAX_KSWAPD_THREADS);
		return ret;
	} else if (kswapd_threads > 1) {
		ret = register_trace_android_vh_kswapd_per_node(kswapd_threads_set, NULL);
		if (ret) {
			pr_err("Failed to register kswapd_per_node hooks\n");
			return ret;
		}
		for_each_node_state(nid, N_MEMORY)
			kswapd_per_node_run(nid, kswapd_threads);
	}
	return ret;
}

static int __init init_mem_hooks(void)
{
	int ret;

	ret = init_kswapd_per_node_hook();
	if (ret)
		return ret;

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

module_init(init_mem_hooks);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Memory Trace Hook Call-Back Registration");
MODULE_LICENSE("GPL v2");
