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

/* MTK */
#include "mt_gpufreq.h"

int mtk_platform_init(struct platform_device *pdev, struct kbase_device *kbdev);


#ifdef ENABLE_MTK_MEMINFO
#define MTK_MEMINFO_SIZE 150

typedef struct
{
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
int mtk_kbase_report_gpu_memory_peak(void);
void mtk_kbase_set_gpu_memory_peak(void);
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

#endif /* __MTK_PLATFORM_COMMON_H__ */
