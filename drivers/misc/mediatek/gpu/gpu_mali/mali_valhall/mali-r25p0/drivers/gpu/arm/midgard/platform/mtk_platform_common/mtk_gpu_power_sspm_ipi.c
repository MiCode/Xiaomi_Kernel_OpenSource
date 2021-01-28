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
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <sspm_ipi_id.h>
#include <sspm_define.h>
#endif

static int init_flag;
static bool ipi_register_flag;

struct kbase_device *pm_kbdev;
int gpu_pm_ipi_ackdata;


static DEFINE_MUTEX(gpu_pmu_info_lock);
static void gpu_send_enable_ipi(unsigned int type, unsigned int enable)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	int cmd_len, ret;
	struct gpu_pm_ipi_cmds ipi_cmd;
	if (!ipi_register_flag) {
		ret = mtk_ipi_register(&sspm_ipidev, IPIS_C_GPU_PM, NULL, NULL,
				(void *) &gpu_pm_ipi_ackdata);
		if (ret) {
			pr_info("[gpu sspm] IPIS_C_GPU_PM ipi_register fail, ret %d\n", ret);
			return;
		}
		ipi_register_flag = true;
	}
	cmd_len = sizeof(struct gpu_pm_ipi_cmds) / SSPM_MBOX_SLOT_SIZE;
	ipi_cmd.cmd[0] = type;
	ipi_cmd.cmd[1] = enable;
	ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_GPU_PM,
		IPI_SEND_POLLING, &ipi_cmd,
		cmd_len, 5000);

	if (ret) {
		pr_info("gpu_send_enable_ipi %d send fail,ret=%d\n",
		ipi_cmd.cmd[0], ret);
	}
#else
	return;
#endif
}

static void MTKGPUPower_model_kbase_setup(int flag, unsigned int interval_ns) {
	struct kbase_ioctl_hwcnt_reader_setup setup;
	int ret;

	//Get the first device - it doesn't matter in this case
	pm_kbdev = kbase_find_device(-1);
	if (!pm_kbdev)
		return;

	//Default doesn't enable all HWC
	setup.fe_bm = 0x16;
	setup.shader_bm = 0x5EC6;
	setup.tiler_bm = 0x2;
	setup.mmu_l2_bm = 0x1FC0;
	setup.buffer_count = 1;
	MTK_update_mtk_pm(flag);
	ret = MTK_kbase_vinstr_hwcnt_reader_setup(pm_kbdev->vinstr_ctx, &setup);
	//1ms = 1000000ns
	MTK_kbasep_vinstr_hwcnt_set_interval(interval_ns);
}

void MTKGPUPower_model_sspm_enable(void) {
	if (init_flag == gpm_kernel_side) {
		MTKGPUPower_model_suspend();
		MTKGPUPower_model_stop();
	}
	MTKGPUPower_model_kbase_setup(pm_swpm, 0);
	gpu_send_enable_ipi(GPU_PM_SWITCH, 1);
	init_flag = gpm_sspm_side;
}

void MTKGPUPower_model_start(unsigned int interval_ns) {
	int pm_tool = MTK_get_mtk_pm();

	mutex_lock(&gpu_pmu_info_lock);
	if (init_flag == gpm_off && pm_tool == pm_non) {
		//gpu stall counter on
		mtk_gpu_stall_create_subfs();
		mtk_gpu_stall_start();
		MTKGPUPower_model_kbase_setup(pm_ltr, interval_ns);
	} else if (pm_tool == pm_swpm) {
		//gpu stall counter on
		mtk_gpu_stall_create_subfs();
		mtk_gpu_stall_start();
		MTK_kbasep_vinstr_hwcnt_set_interval(0);
		MTK_update_mtk_pm(pm_ltr);
		MTK_kbasep_vinstr_hwcnt_set_interval(interval_ns);
	}
	if (init_flag != gpm_sspm_side)
		init_flag = gpm_kernel_side;

	mutex_unlock(&gpu_pmu_info_lock);
}
EXPORT_SYMBOL(MTKGPUPower_model_start);

void MTKGPUPower_model_start_swpm(unsigned int interval_ns){
	int pm_tool = MTK_get_mtk_pm();

	mutex_lock(&gpu_pmu_info_lock);
	//Get the first device - it doesn't matter in this case
	pm_kbdev = kbase_find_device(-1);
	if (!pm_kbdev) {
		return;
	}

	if (init_flag == gpm_sspm_side) {
		gpu_send_enable_ipi(GPU_PM_SWITCH, 0);
		init_flag = gpm_off;
	}

	if (init_flag == gpm_off && pm_tool == pm_non) {
		MTKGPUPower_model_kbase_setup(pm_swpm, interval_ns);
	}
	else if(pm_tool != pm_non){
		MTK_kbasep_vinstr_hwcnt_set_interval(0);
		MTK_update_mtk_pm(pm_swpm);
		MTK_kbasep_vinstr_hwcnt_set_interval(interval_ns);
	}
	gpu_send_enable_ipi(GPU_PM_SWITCH, 0);
	init_flag = gpm_kernel_side;
	mutex_unlock(&gpu_pmu_info_lock);
}
EXPORT_SYMBOL(MTKGPUPower_model_start_swpm);

void MTKGPUPower_model_stop(void){
	mutex_lock(&gpu_pmu_info_lock);
	if (init_flag != gpm_off) {
		if (init_flag == gpm_sspm_side) {
			gpu_send_enable_ipi(GPU_PM_SWITCH, 0);
			gpu_send_enable_ipi(GPU_PM_POWER_STATUE, 0);
		}
		MTK_update_mtk_pm(pm_non);
		MTK_kbasep_vinstr_hwcnt_release();
		mtk_gpu_stall_stop();
		mtk_gpu_stall_delete_subfs();
		init_flag = gpm_off;
	}
	mutex_unlock(&gpu_pmu_info_lock);
}
EXPORT_SYMBOL(MTKGPUPower_model_stop);

void MTKGPUPower_model_suspend(void){
	if (ipi_register_flag && init_flag == gpm_sspm_side)
		gpu_send_enable_ipi(GPU_PM_POWER_STATUE, 0);

	if (init_flag != gpm_kernel_side) {
		return;
	}
	kbase_vinstr_suspend(pm_kbdev->vinstr_ctx);
}
EXPORT_SYMBOL(MTKGPUPower_model_suspend);

void MTKGPUPower_model_resume(void){
	if (ipi_register_flag && init_flag == gpm_sspm_side)
		gpu_send_enable_ipi(GPU_PM_POWER_STATUE, 1);

	if (init_flag != gpm_kernel_side) {
		return;
	}
	kbase_vinstr_resume(pm_kbdev->vinstr_ctx);
}
EXPORT_SYMBOL(MTKGPUPower_model_resume);

int MTKGPUPower_model_init(void) {
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	int ret;

	ret = mtk_ipi_register(&sspm_ipidev, IPIS_C_GPU_PM, NULL, NULL,
			(void *) &gpu_pm_ipi_ackdata);
	if (ret) {
		pr_info("IPIS_C_GPU_PM ipi_register fail, ret %d\n", ret);
		return -1;
	}
	ipi_register_flag = true;
#endif
	return 0;
}


