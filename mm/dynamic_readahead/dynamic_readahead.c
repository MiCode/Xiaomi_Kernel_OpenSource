// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 Oplus. All rights reserved.
 */
#define pr_fmt(fmt) "dynamic_readahead: " fmt

#include <linux/types.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/mm_types.h>
#include <linux/mmzone.h>
#include <linux/sched.h>
#include <linux/vmstat.h>
#include <linux/fs.h>
#include <trace/hooks/mm.h>
#include <linux/cgroup.h>
#include <linux/pagemap.h>
#include <linux/magic.h>
#include <linux/uidgid.h>

#define APPS_UID_HEAD 10000 /* Android user-apps uid start from 10000 */

//extern bool mi_link_vip_task(struct task_struct *tsk);
//extern bool mi_vip_task(struct task_struct *tsk);
static unsigned long long high_wm = 0;
/* true by default, false when dynamic_readahead=N in cmdline */
static bool enable = true;
static bool ra_close_large_folio_enable = true;
static bool fault_around_registered = false;

static struct zone *next_zone_(struct zone *zone)
{
	pg_data_t *pgdat = zone->zone_pgdat;
	if (zone < pgdat->node_zones + MAX_NR_ZONES - 1)
		zone++;
	else
		zone = NULL;
	return zone;
}

#define for_each_zone_(zone)			        \
	for (zone = (NODE_DATA(numa_node_id()))->node_zones; \
	     zone;					\
	     zone = next_zone_(zone))

static inline bool is_lowmem(void)
{
	return global_zone_page_state(NR_FREE_PAGES) < high_wm;
}

static bool current_is_key_task(void)
{
	bool vip_task = true;
	// vip_task = mi_vip_task(current) || mi_link_vip_task(current);
	vip_task = strncmp(task_css(current, cpuset_cgrp_id)->cgroup->kn->name,
			   "background", 11) == 0 ? false : true;
	return vip_task || rt_task(current);
}

static void adjust_readaround(void *ignore, unsigned int ra_pages, pgoff_t pgoff, pgoff_t *start,
			      unsigned int *size, unsigned int *async_size)
{
	unsigned int dy_ra_pages = ra_pages / 2;

	if (enable && !current_is_key_task() && is_lowmem()) {
		*start = max_t(long, 0, pgoff - dy_ra_pages / 2);
		*size = dy_ra_pages;
		*async_size = dy_ra_pages / 4;
	}
}

static void adjust_readahead(void *ignore, struct readahead_control *ractl,
			     unsigned long *max_pages)
{
	struct file_ra_state *ra = ractl->ra;

	if (enable && !current_is_key_task() && is_lowmem()) {
		*max_pages = min_t(long, *max_pages, ra->ra_pages / 2);
	}
}

static void ra_close_lf_page_cache_ra_order_bypass(void *data, struct readahead_control *ractl,
                struct file_ra_state *ra, int new_order, gfp_t *gfp, bool *bypass)
{
	if (!ra_close_large_folio_enable || !ractl || !ractl->mapping ||
			!ractl->mapping->host || !ractl->mapping->host->i_sb)
		return;

	if (ractl->mapping->host->i_sb->s_magic == EROFS_SUPER_MAGIC_V1) {
		*bypass = true;
	}
}

static int register_dymamic_readahead_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_vh_ra_tuning_max_page(adjust_readahead, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_ra_tuning_max_page failed! ret=%d\n", ret);
		goto out;
	}

	ret = register_trace_android_vh_tune_mmap_readaround(adjust_readaround, NULL);
	if (ret != 0) {
		unregister_trace_android_vh_ra_tuning_max_page(adjust_readahead, NULL);
		pr_err("register_trace_android_vh_tune_mmap_readaround failed! ret=%d\n", ret);
		goto out;
	}

	ret = register_trace_android_vh_page_cache_ra_order_bypass(
					ra_close_lf_page_cache_ra_order_bypass, NULL);
	if (ret) {
		unregister_trace_android_vh_tune_mmap_readaround(adjust_readaround, NULL);
		unregister_trace_android_vh_ra_tuning_max_page(adjust_readahead, NULL);
		pr_err("register hook trace_android_vh_page_cache_ra_order_bypass failed! ret=%d\n", ret);
	}

out:
	return ret;
}

static void should_fault_around(void *data, struct vm_fault *vmf, bool *should_around)
{
	uid_t uid;

	uid = __kuid_val(current_uid());
	if (uid >= APPS_UID_HEAD)
		*should_around = true;
	else
		*should_around = false;
}

static void register_fault_around_hooks(void)
{
	int ret;

	ret = register_trace_android_vh_should_fault_around(should_fault_around, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_should_fault_around failed! ret=%d\n", ret);
	} else {
		fault_around_registered = true;
	}
}

static void unregister_dymamic_readahead_hooks(void)
{
	unregister_trace_android_vh_ra_tuning_max_page(adjust_readahead, NULL);
	unregister_trace_android_vh_tune_mmap_readaround(adjust_readaround, NULL);
	unregister_trace_android_vh_page_cache_ra_order_bypass(
					ra_close_lf_page_cache_ra_order_bypass, NULL);
}

static void unregister_fault_around_hooks(void)
{
	if (fault_around_registered) {
		unregister_trace_android_vh_should_fault_around(should_fault_around, NULL);
		fault_around_registered = false;
	}
}

static int __init dynamic_readahead_init(void)
{
	struct zone *zone;
	int ret = 0;

	ret = register_dymamic_readahead_hooks();
	if (ret != 0)
		return ret;

	register_fault_around_hooks();

	for_each_zone_(zone) {
		high_wm += high_wmark_pages(zone);
	}

	pr_info("dynamic_readahead_init high_wm: %llu\n", high_wm);
	return 0;
}

static void __exit dynamic_readahead_exit(void)
{
	unregister_fault_around_hooks();
	unregister_dymamic_readahead_hooks();
}

module_init(dynamic_readahead_init);
module_exit(dynamic_readahead_exit);
module_param(enable, bool, 0600);
MODULE_PARM_DESC(enable, "dynamic_readahead.enable=1 to indicate supporting dynamic readahead or not ");
module_param(ra_close_large_folio_enable, bool, 0600);
MODULE_PARM_DESC(ra_close_large_folio_enable, "ra_close_large_folio_enable = 1 to indicate supporting ra_close_large_folio");
MODULE_LICENSE("GPL v2");
