// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024-2024 XRing Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#include "mi_lb_def.h"
#include "launch_boost_debug.h"
#include "launch_boost_com.h"
#include "pte_hot_preread.h"

struct lb_owner debug_owner;

static int lb_collect_show(struct seq_file *m, void *v)
{
	unsigned int app_index = 0;
	struct package_info *package_list = NULL;

	mutex_lock(&g_manager->package_list_lock);
	if (list_empty(&g_manager->package_infos)) {
		seq_puts(m, "g_cold_record_list_user0 is empty\n");
		goto unlock;
	}

	seq_printf(m, "total memory size %lu app_num: %u\n",
                    g_manager->total_memory , g_manager->app_num );
	list_for_each_entry(package_list, &g_manager->package_infos, app_list) {
		seq_printf(m, "[%d] app:%s, uid:%d, file num: %d, file size: %u, hit rate: %d%%, mm size: %d\n",
				app_index++,
				package_list->owner.package_name,
				package_list->owner.uid,
				package_list->files,
				package_list->total_startup_bytes,
				package_list->hit_rate,
				package_list->mm_size);
	}
unlock:
	mutex_unlock(&g_manager->package_list_lock);
	return 0;
}


static int lb_collect_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, lb_collect_show, NULL);
}

static const struct proc_ops lb_collect_file_fops = {
	.proc_open	= lb_collect_file_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

#define PRINT_SIGN "######################################################################\n"
static int lb_hot_collect_show(struct seq_file *m, void *v)
{
	unsigned int app_index = 0;
	struct pte_record *mps_rec;
	struct lb_hot_mps *mps = &g_hot_mps;
	s64 avg_rlock_time = atomic64_read(&mps->stat.total_rlock_time) / (atomic64_read(&mps->stat.total_swapin_count) ? : 1);
	s64 avg_swapin_time = atomic64_read(&mps->stat.total_swapin_time) / (atomic64_read(&mps->stat.total_swapin_count) ? : 1);
	s64 avg_fadvise_time = atomic64_read(&mps->stat.total_fadvise_time) / (atomic64_read(&mps->stat.total_swapin_count) ? : 1);
	s64 avg_preread_time = atomic64_read(&mps->stat.total_preread_time) / (atomic64_read(&mps->stat.total_swapin_count) ? : 1);

	seq_printf(m, PRINT_SIGN
			"min_swapin, time: %lld, app: %s\n"
			"max_swapin, time: %lld, app: %s\n"
			"avg time, rlock: %lld, swapin: %lld, fadvise: %lld, total: %lld, total count: %lld\n"
			"interrupt, by ioctl: %lld, by rlock: %lld, by swapin: %lld\n"
			"memory usage, pte_segment: %u (%luMB), pte_record: %u (%luMB)\n"PRINT_SIGN,
			MPS_STAT_READ_TIME(&mps->stat, min_swapin_time) == LONG_MAX ? 0 : MPS_STAT_READ_TIME(&mps->stat, min_swapin_time) , mps->stat.shortest_app,
			MPS_STAT_READ_TIME(&mps->stat, max_swapin_time), mps->stat.longest_app,
			avg_rlock_time, avg_swapin_time, avg_fadvise_time, avg_preread_time,
			atomic64_read(&mps->stat.total_swapin_count),
			atomic64_read(&mps->stat.interrupt_count[MPS_INTERRUPT_BY_IOCTL]),
			atomic64_read(&mps->stat.interrupt_count[MPS_INTERRUPT_BY_RLOCK]),
			atomic64_read(&mps->stat.interrupt_count[MPS_INTERRUPT_BY_SWAPIN]),
			atomic_read(&mps->nr_alloc_segs), BYTES_TO_MB(atomic_read(&mps->nr_alloc_segs) * sizeof(struct pte_segment)),
			atomic_read(&mps->nr_alloc_mps_recs), BYTES_TO_MB(atomic_read(&mps->nr_alloc_mps_recs) * sizeof(struct pte_record)));

    spin_lock(&mps->list_lock);
    if (list_empty(&mps->rec_list)) {
		seq_puts(m, "g_hot_record_list_user0 is empty\n");
		goto unlock;
	}

	seq_printf(m, "snapshot: %u prepage: %u err: %u\n",
			mps->nr_snapshot, mps->nr_prepage, mps->nr_error);
	seq_printf(m, "anon: %llu pre_anon: %llu file: %llu pre_file: %llu\n",
			mps->total_anon, mps->total_pre_anon, mps->total_file, mps->total_pre_file);

	list_for_each_entry(mps_rec, &mps->rec_list, rec_node) {
		seq_printf(m, "[%d] uid:%d pid:%d , segs %u pages %u %u pre %u %u hit %u %u, refer/active: %u/%u\n",
							app_index++,
							mps_rec->uid,
							mps_rec->pid,
							mps_rec->nr_segments,
							mps_rec->nr_anon,
							mps_rec->nr_file,
							mps_rec->nr_pre_anon,
							mps_rec->nr_pre_file,
							mps_rec->nr_hit_anon,
							mps_rec->nr_hit_file,
							mps_rec->nr_refer_young,
							mps_rec->nr_active);
    }
unlock:
	spin_unlock(&mps->list_lock);
	return 0;
}

static int lb_hot_collect_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, lb_hot_collect_show, NULL);
}

static const struct proc_ops lb_hot_collect_file_fops = {
	.proc_open	= lb_hot_collect_file_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release = single_release,
};

static int mi_lb_create_proc(void)
{
	struct proc_dir_entry *lb_dir, *lb_collect , *lb_hot_collect;

	lb_dir = proc_mkdir("iorap_node", NULL);
	if (!lb_dir) {
		MI_LB_ERR("proc_mkdir failed\n");
		return -ENOMEM;
	}

	lb_collect = proc_create("collect", 0, lb_dir, &lb_collect_file_fops);
	if (!lb_collect) {
		remove_proc_entry("iorap_node", NULL);
		MI_LB_ERR("proc_create lb_collect failed\n");
		return -ENOMEM;
	}

	lb_hot_collect = proc_create("hot_collect", 0, lb_dir, &lb_hot_collect_file_fops);
	if (!lb_hot_collect) {
		remove_proc_entry("collect", NULL);
		remove_proc_entry("iorap_node", NULL);
		MI_LB_ERR("proc_create lb_hot_collect failed\n");
		return -ENOMEM;
	}

	return 0;

}

int mi_lb_dbg_init(void)
{
	int error;

	error = mi_lb_create_proc();
	if (error)
		MI_LB_ERR("lb_create_proc failed\n");

	return error;
}

int g_lb_msg_level = 3;
module_param_named(lb_debug, g_lb_msg_level, int, 0644);
MODULE_PARM_DESC(lb_debug, "iorap debug msg level");

bool g_only_collect = 0;
module_param_named(lb_only_collect, g_only_collect, bool, 0644);
MODULE_PARM_DESC(lb_only_collect, "iorap only collect");

bool g_atrace_enable = 0;
module_param_named(lb_atrace_enable, g_atrace_enable, bool, 0644);
MODULE_PARM_DESC(lb_atrace_enable, "iorap atrace enable");

bool g_collect_specify_filetype = 0;
module_param_named(lb_collect_specify_filetype, g_collect_specify_filetype, bool, 0644);
MODULE_PARM_DESC(lb_collect_specify_filetype, "iorap only collect specify filetype enable");

bool g_collect_pinfile = 1;
module_param_named(lb_collect_pinfile, g_collect_pinfile, bool, 0644);
MODULE_PARM_DESC(lb_collect_pinfile, "iorap collect pinfile");

bool g_debug_verbose = 0;
module_param_named(lb_debug_verbose, g_debug_verbose, bool, 0644);
MODULE_PARM_DESC(lb_debug_verbose, "iorap debug verbose");

bool g_rlock_expire_interruput_enable = DEF_RLOCK_EXPIRE_INTERRUPT_ENABLE;
module_param_named(lb_rlock_expire_interruput_enable, g_rlock_expire_interruput_enable, bool, 0644);
MODULE_PARM_DESC(lb_rlock_expire_interruput_enable, "interrupt preread when mmap lock acquire time exceed threshold");

bool g_swapin_expire_interruput_enable = DEF_SWAPIN_EXPIRE_INTERRUPT_ENABLE;
module_param_named(lb_swapin_expire_interruput_enable, g_swapin_expire_interruput_enable, bool, 0644);
MODULE_PARM_DESC(lb_swapin_expire_interruput_enable, "interrupt preread when swapin time exceed threshold");

u64 g_rlock_acquire_threshold = DEF_RLOCK_ACQUIRE_THRESHOLD;
module_param_named(lb_rlock_acquire_threshold, g_rlock_acquire_threshold, ullong, 0644);
MODULE_PARM_DESC(lb_rlock_acquire_threshold, "(in us) mmap lock acquire time exceed this threshold will stop preread");

u64 g_swapin_total_time_threshold = DEF_SWAPIN_TOTAL_TIME_THRESHOLD;
module_param_named(lb_swapin_total_time_threshold, g_swapin_total_time_threshold, ullong, 0644);
MODULE_PARM_DESC(lb_swapin_total_time_threshold, "(in us) swapin time exceed this threshold will stop preread");

bool g_pte_mark_file_anon_enable = DEF_MARK_FILE_ANON_ENABLE;
module_param_named(lb_pte_mark_file_anon_enable, g_pte_mark_file_anon_enable, bool, 0644);
MODULE_PARM_DESC(lb_pte_mark_file_anon_enable, "mark pte_segment as anon and file or not");

bool g_pte_fadvise_enable = DEF_PTE_FADVISE_ENABLE;
module_param_named(lb_pte_fadvise_enable, g_pte_fadvise_enable, bool, 0644);
MODULE_PARM_DESC(lb_pte_fadvise_enable, "fadvise enable or not");

bool g_pte_anon_swapin_enable = DEF_PTE_ANON_SWAPIN_ENABLE;
module_param_named(lb_pte_anon_swapin_enable, g_pte_anon_swapin_enable, bool, 0644);
MODULE_PARM_DESC(lb_pte_anon_swapin_enable, "anon swapin enable or not");

u32 g_pte_max_active_apps = DEF_PTE_MAX_ACTIVE_APPS;
module_param_named(lb_pte_max_active_apps, g_pte_max_active_apps, uint, 0644);
MODULE_PARM_DESC(lb_pte_max_active_apps, "max active apps");

u32 g_max_pte_segment_count = DEF_MAX_PTE_SEGMENT_COUNT;
module_param_named(lb_max_pte_segment_count, g_max_pte_segment_count, uint, 0644);
MODULE_PARM_DESC(lb_max_pte_segment_count, "max pte segment count to limit memory usage");
