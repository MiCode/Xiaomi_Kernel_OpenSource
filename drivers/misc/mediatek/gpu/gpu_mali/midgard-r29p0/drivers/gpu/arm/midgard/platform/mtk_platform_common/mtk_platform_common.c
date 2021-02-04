/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include <mali_kbase.h>
#include <mali_kbase_mem.h>

#include <linux/proc_fs.h>

#include "platform/mtk_platform_common.h"
#include "mtk_gpufreq.h"
#include <mali_kbase_pm_internal.h>

#include <ged_log.h>

#include <linux/workqueue.h>
#include <mt-plat/aee.h>

#ifdef ENABLE_MTK_MEMINFO

int g_mtk_gpu_total_memory_usage_in_pages_debugfs;
atomic_t g_mtk_gpu_total_memory_usage_in_pages;
atomic_t g_mtk_gpu_peak_memory_usage_in_pages;
static struct mtk_gpu_meminfo_type g_mtk_gpu_meminfo[MTK_MEMINFO_SIZE];

int g_mtk_gpu_efuse_set_already = 0;

/* todo: remove it
extern u32 kbasep_get_gl_utilization(void);
extern u32 kbasep_get_cl_js0_utilization(void);
extern u32 kbasep_get_cl_js1_utilization(void);
*/

/* on:1, off:0 */
int g_vgpu_power_on_flag = 0;
DEFINE_MUTEX(g_flag_lock);

void mtk_kbase_gpu_memory_debug_init(void)
{
	mtk_dump_gpu_memory_usage_fp = mtk_kbase_dump_gpu_memory_usage;
	mtk_get_gpu_memory_usage_fp = mtk_kbase_report_gpu_memory_usage;
}

void mtk_kbase_gpu_memory_debug_remove(void)
{
	mtk_dump_gpu_memory_usage_fp = NULL;
	mtk_get_gpu_memory_usage_fp = NULL;
}

void mtk_kbase_reset_gpu_meminfo(void)
{
	int i = 0;

	for (i = 0; i < MTK_MEMINFO_SIZE; i++) {
		g_mtk_gpu_meminfo[i].pid = 0;
		g_mtk_gpu_meminfo[i].used_pages = 0;
	}
}

void mtk_kbase_set_gpu_meminfo(ssize_t index, int pid, int used_pages)
{
	g_mtk_gpu_meminfo[index].pid = pid;
	g_mtk_gpu_meminfo[index].used_pages = used_pages;
}

KBASE_EXPORT_TEST_API(mtk_kbase_dump_gpu_memory_usage)
bool mtk_kbase_dump_gpu_memory_usage(void)
{
	int i = 0;

	/*output the total memory usage and cap for this device*/
	pr_warn("%10s\t%16s\n", "PID", "Memory by Page");
	pr_warn("============================\n");

	for (i = 0; (i < MTK_MEMINFO_SIZE) && (g_mtk_gpu_meminfo[i].pid != 0); i++) {
		pr_warn("%10d\t%16d\n", g_mtk_gpu_meminfo[i].pid,
				g_mtk_gpu_meminfo[i].used_pages);
	}

	pr_warn("============================\n");
	pr_warn("%10s\t%16u\n",
			"Total",
			g_mtk_gpu_total_memory_usage_in_pages_debugfs);
	pr_warn("============================\n");
	return true;
}

KBASE_EXPORT_TEST_API(mtk_kbase_report_gpu_memory_usage)
unsigned int mtk_kbase_report_gpu_memory_usage(void)
{
#if 0
	ssize_t ret = 0;
	struct list_head *entry;
	const struct list_head *kbdev_list;
	int pages = 0;

	kbdev_list = kbase_dev_list_get();
	list_for_each(entry, kbdev_list) {
		struct kbase_device *kbdev = NULL;
		kbasep_kctx_list_element *element;

		kbdev = list_entry(entry, struct kbase_device, entry);
		pages = atomic_read(&(kbdev->memdev.used_pages));
	}
	kbase_dev_list_put(kbdev_list);
	pr_info("gpu total memory %d\n", pages*4096);
#endif
	return (atomic_read(&g_mtk_gpu_total_memory_usage_in_pages)*4096);
}

int mtk_kbase_report_gpu_memory_peak(void)
{
	return (atomic_read(&g_mtk_gpu_peak_memory_usage_in_pages)*4096);
}

void mtk_kbase_set_gpu_memory_peak(void)
{
	int curr;
	int peak;

	curr = atomic_read(&g_mtk_gpu_total_memory_usage_in_pages);
	peak = atomic_read(&g_mtk_gpu_peak_memory_usage_in_pages);

	if (curr > peak)
		atomic_set(&g_mtk_gpu_peak_memory_usage_in_pages, curr);
}
#endif /* ENABLE_MTK_MEMINFO */

static int last_fail_commit_id = -1;

int mtk_get_vgpu_power_on_flag(void)
{
	return g_vgpu_power_on_flag;
}

int mtk_set_vgpu_power_on_flag(int power_on_id)
{
	mutex_lock(&g_flag_lock);

	if (power_on_id && last_fail_commit_id != -1) {
		mt_gpufreq_target(last_fail_commit_id);
		last_fail_commit_id = -1;
	}

	g_vgpu_power_on_flag = power_on_id;
	mutex_unlock(&g_flag_lock);
	return 0;
}

int mtk_set_mt_gpufreq_target(int freq_id)
{
	int ret = 0;

	mutex_lock(&g_flag_lock);
	if (MTK_VGPU_POWER_ON == mtk_get_vgpu_power_on_flag())
		ret = mt_gpufreq_target(freq_id);
	else
		last_fail_commit_id = freq_id;
	mutex_unlock(&g_flag_lock);
	return ret;
}
