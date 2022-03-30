// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_PLATFORM_COMMON_H__
#define __MTK_PLATFORM_COMMON_H__

#include <linux/platform_device.h>

struct kbase_device *mtk_common_get_kbdev(void);

bool mtk_common_pm_is_mfg_active(void);
void mtk_common_pm_mfg_active(void);
void mtk_common_pm_mfg_idle(void);

int mtk_common_gpufreq_commit(int opp_idx);
int mtk_common_ged_dvfs_get_last_commit_idx(void);

int mtk_common_device_init(struct kbase_device *kbdev);
void mtk_common_device_term(struct kbase_device *kbdev);

#if IS_ENABLED(CONFIG_PROC_FS)
void mtk_common_procfs_init(void);
void mtk_common_procfs_exit(void);
#endif

int mtk_platform_device_init(struct kbase_device *kbdev);
void mtk_platform_device_term(struct kbase_device *kbdev);

#endif /* __MTK_PLATFORM_COMMON_H__ */
