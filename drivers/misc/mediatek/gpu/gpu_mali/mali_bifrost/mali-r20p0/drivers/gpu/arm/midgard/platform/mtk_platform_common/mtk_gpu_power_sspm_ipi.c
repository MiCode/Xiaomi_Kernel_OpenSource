/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <mtk_gpu_utility.h>
#include <linux/uaccess.h>
#include <mali_kbase_gator_api.h>
#include <platform/mtk_mfg_counter.h>
#include <linux/sched.h>
#include <linux/mutex.h>

#include "mtk_gpu_power_sspm_ipi.h"
#include "mali_kbase.h"
#include "mali_kbase_ioctl.h"
#include "mali_kbase_vinstr.h"

/* sspm reserved mem info for sspm upower */
//#include <sspm_reservedmem_define.h>
//#include <mtk_swpm_platform.h>
//phys_addr_t data_phy_addr, data_virt_addr;
//unsigned long long data_size;
//static unsigned int gpu_hw_counter[GPU_POWER_COUNTER_LAST];
static int init_flag;
struct kbase_device *pm_kbdev;

static DEFINE_MUTEX(gpu_pmu_info_lock);
void MTKGPUPower_model_start(unsigned int interval_ns){
	struct kbase_ioctl_hwcnt_reader_setup setup;
	mutex_lock(&gpu_pmu_info_lock);
	//Get the first device - it doesn't matter in this case
	pm_kbdev = kbase_find_device(-1);
	if (!pm_kbdev){
		return;
	}
	/* Default doesn't enable all HWC */
	setup.jm_bm = 0x16;
	setup.shader_bm = 0x1CC2;
	setup.tiler_bm = 0x2;
	setup.mmu_l2_bm = 0x1FC0;
	setup.buffer_count = 1;
	kbase_vinstr_hwcnt_reader_setup(pm_kbdev->vinstr_ctx, &setup);
	//1ms = 1000000ns
	MTK_kbasep_vinstr_hwcnt_set_interval(pm_kbdev->vinstr_ctx, interval_ns);
	init_flag = 1;

	mutex_unlock(&gpu_pmu_info_lock);
}
EXPORT_SYMBOL(MTKGPUPower_model_start);

void MTKGPUPower_model_stop(void){
	mutex_lock(&gpu_pmu_info_lock);
	MTK_kbasep_vinstr_hwcnt_release(pm_kbdev->vinstr_ctx);
	//reset gpu_hw_counter
	init_flag = 0;
	mutex_unlock(&gpu_pmu_info_lock);
}
EXPORT_SYMBOL(MTKGPUPower_model_stop);

void MTKGPUPower_model_suspend(void){
	if (!init_flag){
		return;
	}
	kbase_vinstr_suspend(pm_kbdev->vinstr_ctx);
}
EXPORT_SYMBOL(MTKGPUPower_model_suspend);

void MTKGPUPower_model_resume(void){
	if (!init_flag){
		return;
	}
	kbase_vinstr_resume(pm_kbdev->vinstr_ctx);
}
EXPORT_SYMBOL(MTKGPUPower_model_resume);
