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

#ifndef __MTK_PLATFORM_COMMON_H__
#define __MTK_PLATFORM_COMMON_H__

#include <linux/platform_device.h>
#include "mtk_mfg_counter.h"

#ifdef ENABLE_COMMON_DVFS
#include "mtk_gpufreq.h"
#endif /* ENABLE_COMMON_DVFS */

int mtk_common_init(struct platform_device *pdev, struct kbase_device *kbdev);
int mtk_common_deinit(struct platform_device *pdev, struct kbase_device *kbdev);
int mtk_platform_init(struct platform_device *pdev, struct kbase_device *kbdev);

#ifdef ENABLE_MTK_MEMINFO
#define MTK_MEMINFO_SIZE 150

/*
* Add by mediatek, Hook the memory query function pointer to (*mtk_get_gpu_memory_usage_fp) in order to
* provide the gpu total memory usage to mlogger module
*/
extern unsigned int (*mtk_get_gpu_memory_usage_fp)(void);

/*
* Add by mediatek, Hook the memory dump function pointer to (*ged_mem_dump_gpu_memory_usag_fp) in order to
* provide the gpu detail memory usage by PID to mlogger module
*/
extern bool (*mtk_dump_gpu_memory_usage_fp)(void);

typedef struct {
	int pid;
	int used_pages;
} mtk_gpu_meminfo_type;
#endif /* ENABLE_MTK_MEMINFO */

enum mtk_vgpu_power_on {
	MTK_VGPU_POWER_OFF,	/** VGPU_POWER_OFF */
	MTK_VGPU_POWER_ON,	/** VGPU_POWER_ON */
};


#ifdef ENABLE_MTK_MEMINFO
void mtk_kbase_gpu_memory_debug_init(void);
void mtk_kbase_gpu_memory_debug_remove(void);
void mtk_kbase_reset_gpu_meminfo(void);
void mtk_kbase_set_gpu_meminfo(ssize_t index, int pid, int used_pages);
bool mtk_kbase_dump_gpu_memory_usage(void);
unsigned int mtk_kbase_report_gpu_memory_usage(void);
extern int g_mtk_gpu_total_memory_usage_in_pages_debugfs;
#endif /* ENABLE_MTK_MEMINFO */

#ifdef CONFIG_PROC_FS
void proc_mali_register(void);
void proc_mali_unregister(void);
#endif /* CONFIG_PROC_FS */

/* MTK internal vgpu function */

int mtk_get_vgpu_power_on_flag(void);
int mtk_set_vgpu_power_on_flag(int power_on_id);

int mtk_set_mt_gpufreq_target(int freq_id);

void mtk_trigger_aee_report(const char *msg);
void mtk_trigger_emi_report(u64 pa);

int mtk_kbase_is_gpu_always_on(void);
int mtk_kbase_gpu_debug_log(void);
unsigned long mtk_get_ged_dvfs_last_commit_idx(void);

#endif /* __MTK_PLATFORM_COMMON_H__ */
