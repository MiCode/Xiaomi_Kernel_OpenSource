/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/proc_fs.h>
#include <linux/workqueue.h>
#include "mtk_gpu_meminfo.h"

#ifdef ENABLE_MTK_MEMINFO
extern unsigned int (*mtk_get_gpu_memory_usage_fp)(void);
extern bool (*mtk_dump_gpu_memory_usage_fp)(void);

int g_mtk_gpu_total_memory_usage_in_pages_debugfs;
static mtk_gpu_meminfo_type g_mtk_gpu_meminfo[MTK_MEMINFO_SIZE];


void mtk_gpu_meminfo_init(void)
{
	mtk_dump_gpu_memory_usage_fp = mtk_dump_mali_memory_usage;
	mtk_get_gpu_memory_usage_fp = mtk_get_mali_memory_usage;
}

void mtk_gpu_meminfo_remove(void)
{
	mtk_dump_gpu_memory_usage_fp = NULL;
	mtk_get_gpu_memory_usage_fp = NULL;
}

void mtk_gpu_meminfo_reset(void)
{
	int i = 0;
	for (i = 0; i < MTK_MEMINFO_SIZE; i++) {
		g_mtk_gpu_meminfo[i].pid = 0;
		g_mtk_gpu_meminfo[i].used_pages = 0;
	}
}

void mtk_gpu_meminfo_set(ssize_t index, int pid, int used_pages)
{
	g_mtk_gpu_meminfo[index].pid = pid;
	g_mtk_gpu_meminfo[index].used_pages = used_pages;
}

bool mtk_dump_mali_memory_usage(void)
{
	int i = 0;

	pr_warn(KERN_DEBUG "%10s\t%16s\n", "PID", "Memory by Page");
	pr_warn(KERN_DEBUG "============================\n");

	for (i = 0; (i < MTK_MEMINFO_SIZE) && (g_mtk_gpu_meminfo[i].pid != 0); i++) {
		pr_warn(KERN_DEBUG "%10d\t%16d\n", g_mtk_gpu_meminfo[i].pid, g_mtk_gpu_meminfo[i].used_pages);
	}

	pr_warn(KERN_DEBUG "============================\n");
	pr_warn(KERN_DEBUG "%10s\t%16u\n", "Total", g_mtk_gpu_total_memory_usage_in_pages_debugfs);
	pr_warn(KERN_DEBUG "============================\n");
	return true;
}

unsigned int mtk_get_mali_memory_usage(void)
{
	return (g_mtk_gpu_total_memory_usage_in_pages_debugfs*4096);
}
#endif
