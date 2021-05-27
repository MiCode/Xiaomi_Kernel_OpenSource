/*
 * Copyright (C) 2021 MediaTek Inc.
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


struct kbase_device *mtk_common_get_kbdev(void);

bool mtk_common_pm_is_mfg_active(void);
void mtk_common_pm_mfg_active(void);
void mtk_common_pm_mfg_idle(void);

int mtk_common_gpufreq_commit(int opp_idx);
int mtk_common_ged_dvfs_get_last_commit_idx(void);

int mtk_common_device_init(struct kbase_device *kbdev);
void mtk_common_device_term(struct kbase_device *kbdev);

#ifdef CONFIG_PROC_FS
void mtk_common_procfs_init(void);
void mtk_common_procfs_exit(void);
#endif

int mtk_platform_device_init(struct kbase_device *kbdev);
void mtk_platform_device_term(struct kbase_device *kbdev);

#endif /* __MTK_PLATFORM_COMMON_H__ */
