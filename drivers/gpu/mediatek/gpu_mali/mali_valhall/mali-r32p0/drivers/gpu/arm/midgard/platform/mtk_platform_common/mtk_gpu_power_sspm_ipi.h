// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_GPU_POWER_MODEL_H__
#define __MTK_GPU_POWER_MODEL_H__
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>


#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>


enum {
	GPU_PM_POWER_STATUE,
	GPU_PM_SWITCH,	// 0 : API 1 : SSPM
	GPU_PM_LAST,
	GPU_PM_LAST2
};

enum {
	gpm_off,
	gpm_kernel_side,
	gpm_sspm_side
};

struct gpu_pm_ipi_cmds {
	unsigned int cmd[GPU_PM_LAST];
};

void MTKGPUPower_model_stop(void);
void MTKGPUPower_model_start(unsigned int interval_ns);
void MTKGPUPower_model_start_swpm(unsigned int interval_ns);
void MTKGPUPower_model_suspend(void);
void MTKGPUPower_model_resume(void);
int MTKGPUPower_model_init(void);
void MTKGPUPower_model_destroy(void);

void MTKGPUPower_model_sspm_enable(void);
extern void (*mtk_ltr_gpu_pmu_start_fp)(unsigned int interval_ns);
extern void (*mtk_ltr_gpu_pmu_stop_fp)(void);


#endif
