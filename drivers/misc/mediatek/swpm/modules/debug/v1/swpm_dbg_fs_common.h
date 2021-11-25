/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __SWPM_DBG_FS_COMMON_H__
#define __SWPM_DBG_FS_COMMON_H__

#include <mtk_swpm_common_sysfs.h>
#include <mtk_swpm_sysfs.h>


extern int mtk_gpueb_power_modle_cmd(unsigned int enable);
extern void mtk_swpm_gpu_pm_start(void);
extern void mtk_ltr_gpu_pmu_stop(void);


#endif /* __SWPM_DBG_FS_COMMON_H__ */
