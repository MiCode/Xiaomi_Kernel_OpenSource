// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/signal.h>
#include <linux/printk.h>

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
	if (!strcmp(task->comm, "lmkd"))
		*reap = true;
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

	return 0;
}
module_init(init_mem_hooks);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Memory Trace Hook Call-Back Registration");
MODULE_LICENSE("GPL v2");
