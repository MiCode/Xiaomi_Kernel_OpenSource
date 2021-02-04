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

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/hardirq.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <mt-plat/aee.h>

#include "ged_debugFS.h"
#include "ged_log.h"
#include "ged_hal.h"
#include "ged_bridge.h"
#include "ged_profile_dvfs.h"
#include "ged_monitor_3D_fence.h"
#include "ged_notify_sw_vsync.h"
#include "ged_dvfs.h"
#include "ged_kpi.h"
#include "ged_frr.h"
#include "ged_fdvfs.h"

#include "ged_ge.h"
#include "ged_gpu_tuner.h"

#define GED_DRIVER_DEVICE_NAME "ged"
#ifndef GED_BUFFER_LOG_DISABLE
#ifdef GED_DEBUG
#define GED_LOG_BUF_COMMON_GLES "GLES"
static GED_LOG_BUF_HANDLE ghLogBuf_GLES;
GED_LOG_BUF_HANDLE ghLogBuf_GED;
#endif

static GED_LOG_BUF_HANDLE ghLogBuf_GPU;
#define GED_LOG_BUF_COMMON_HWC "HWC"
static GED_LOG_BUF_HANDLE ghLogBuf_HWC;
#define GED_LOG_BUF_COMMON_HWC_ERR "HWC_err"
static GED_LOG_BUF_HANDLE ghLogBuf_HWC_ERR;
#define GED_LOG_BUF_COMMON_FENCE "FENCE"
static GED_LOG_BUF_HANDLE ghLogBuf_FENCE;
static GED_LOG_BUF_HANDLE ghLogBuf_FWTrace;
static GED_LOG_BUF_HANDLE ghLogBuf_ftrace;
#endif

GED_LOG_BUF_HANDLE ghLogBuf_DVFS;
GED_LOG_BUF_HANDLE ghLogBuf_ged_srv;

/******************************************************************************
 * GED File operations
 *****************************************************************************/
static int ged_open(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	GED_LOGE("%s:%d:%d\n", __func__, MAJOR(inode->i_rdev), MINOR(inode->i_rdev));
	return 0;
}

static int ged_release(struct inode *inode, struct file *filp)
{
	if (filp->private_data) {
		void (*free_func)(void *) = ((GED_FILE_PRIVATE_BASE *)filp->private_data)->free_func;

		free_func(filp->private_data);
	}
	GED_LOGE("%s:%d:%d\n", __func__, MAJOR(inode->i_rdev), MINOR(inode->i_rdev));
	return 0;
}

static unsigned int ged_poll(struct file *file, struct poll_table_struct *ptable)
{
	return 0;
}

static ssize_t ged_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static ssize_t ged_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static long ged_dispatch(struct file *pFile, GED_BRIDGE_PACKAGE *psBridgePackageKM)
{
	int ret = -EFAULT;
	void *pvIn = NULL, *pvOut = NULL;
	typedef int (ged_bridge_func_type)(void *, void *);
	ged_bridge_func_type* pFunc = NULL;

	/* We make sure the both size are GE 0 integer.
	 */
	if (psBridgePackageKM->i32InBufferSize >= 0 && psBridgePackageKM->i32OutBufferSize >= 0) {

		if (psBridgePackageKM->i32InBufferSize > 0) {
			pvIn = kmalloc(psBridgePackageKM->i32InBufferSize, GFP_KERNEL);

			if (pvIn == NULL)
				goto dispatch_exit;

			if (ged_copy_from_user(pvIn,
						psBridgePackageKM->pvParamIn,
						psBridgePackageKM->i32InBufferSize) != 0) {
				GED_LOGE("ged_copy_from_user fail\n");
				goto dispatch_exit;
			}
		}

		if (psBridgePackageKM->i32OutBufferSize > 0) {
			pvOut = kzalloc(psBridgePackageKM->i32OutBufferSize, GFP_KERNEL);

			if (pvOut == NULL)
				goto dispatch_exit;
		}

		/* Make sure that the UM will never break the KM.
		 * Check IO size are both matched the size of IO sturct.
		 */
#define SET_FUNC_AND_CHECK_FOR_NO_TYPEDEF(func, struct_name) do { \
		pFunc = (ged_bridge_func_type *) func; \
		if (sizeof(struct GED_BRIDGE_IN_##struct_name) > \
			psBridgePackageKM->i32InBufferSize || \
			sizeof(struct GED_BRIDGE_OUT_##struct_name) > \
			psBridgePackageKM->i32OutBufferSize) { \
			GED_LOGE("%s fail io_size:%d/%d, expected: %zu/%zu", \
				"GED_BRIDGE_COMMAND_##cmd", \
				psBridgePackageKM->i32InBufferSize, \
				psBridgePackageKM->i32OutBufferSize, \
				sizeof(struct GED_BRIDGE_IN_##struct_name), \
				sizeof(struct GED_BRIDGE_OUT_##struct_name)); \
			goto dispatch_exit; \
		} } while (0)

#define SET_FUNC_AND_CHECK(func, struct_name) do { \
		pFunc = (ged_bridge_func_type *) func; \
		if (sizeof(GED_BRIDGE_IN_##struct_name) > psBridgePackageKM->i32InBufferSize || \
			sizeof(GED_BRIDGE_OUT_##struct_name) > psBridgePackageKM->i32OutBufferSize) { \
			GED_LOGE("GED_BRIDGE_COMMAND_##cmd fail io_size:%d/%d, expected: %zu/%zu", \
				psBridgePackageKM->i32InBufferSize, psBridgePackageKM->i32OutBufferSize, \
				sizeof(GED_BRIDGE_IN_##struct_name), sizeof(GED_BRIDGE_OUT_##struct_name)); \
			goto dispatch_exit; \
		} } while (0)

		/* we will change the below switch into a function pointer mapping table in the future */
		switch (GED_GET_BRIDGE_ID(psBridgePackageKM->ui32FunctionID)) {
		case GED_BRIDGE_COMMAND_LOG_BUF_GET:
			SET_FUNC_AND_CHECK(ged_bridge_log_buf_get, LOGBUFGET);
			break;
		case GED_BRIDGE_COMMAND_LOG_BUF_WRITE:
			SET_FUNC_AND_CHECK(ged_bridge_log_buf_write, LOGBUFWRITE);
			break;
		case GED_BRIDGE_COMMAND_LOG_BUF_RESET:
			SET_FUNC_AND_CHECK(ged_bridge_log_buf_reset, LOGBUFRESET);
			break;
		case GED_BRIDGE_COMMAND_BOOST_GPU_FREQ:
			SET_FUNC_AND_CHECK(ged_bridge_boost_gpu_freq, BOOSTGPUFREQ);
			break;
		case GED_BRIDGE_COMMAND_MONITOR_3D_FENCE:
			SET_FUNC_AND_CHECK(ged_bridge_monitor_3D_fence, MONITOR3DFENCE);
			break;
		case GED_BRIDGE_COMMAND_QUERY_INFO:
			SET_FUNC_AND_CHECK(ged_bridge_query_info, QUERY_INFO);
			break;
		case GED_BRIDGE_COMMAND_NOTIFY_VSYNC:
			SET_FUNC_AND_CHECK(ged_bridge_notify_vsync, NOTIFY_VSYNC);
			break;
		case GED_BRIDGE_COMMAND_DVFS_PROBE:
			SET_FUNC_AND_CHECK(ged_bridge_dvfs_probe, DVFS_PROBE);
			break;
		case GED_BRIDGE_COMMAND_DVFS_UM_RETURN:
			SET_FUNC_AND_CHECK(ged_bridge_dvfs_um_retrun, DVFS_UM_RETURN);
			break;
		case GED_BRIDGE_COMMAND_EVENT_NOTIFY:
			SET_FUNC_AND_CHECK(ged_bridge_event_notify, EVENT_NOTIFY);
			break;
		case GED_BRIDGE_COMMAND_GPU_HINT_TO_CPU:
			SET_FUNC_AND_CHECK_FOR_NO_TYPEDEF(
				ged_bridge_gpu_hint_to_cpu,
				GPU_HINT_TO_CPU);
			break;
		case GED_BRIDGE_COMMAND_HINT_FORCE_MDP:
			SET_FUNC_AND_CHECK_FOR_NO_TYPEDEF(
				ged_bridge_hint_force_mdp,
				HINT_FORCE_MDP);
			break;
		case GED_BRIDGE_COMMAND_GE_ALLOC:
			SET_FUNC_AND_CHECK(ged_bridge_ge_alloc, GE_ALLOC);
			break;
		case GED_BRIDGE_COMMAND_GE_GET:
			SET_FUNC_AND_CHECK(ged_bridge_ge_get, GE_GET);
			break;
		case GED_BRIDGE_COMMAND_GE_SET:
			SET_FUNC_AND_CHECK(ged_bridge_ge_set, GE_SET);
			break;
		case GED_BRIDGE_COMMAND_GE_INFO:
			SET_FUNC_AND_CHECK(ged_bridge_ge_info, GE_INFO);
			break;
		case GED_BRIDGE_COMMAND_GPU_TIMESTAMP:
			SET_FUNC_AND_CHECK(ged_bridge_gpu_timestamp,
				GPU_TIMESTAMP);
			break;
		case GED_BRIDGE_COMMAND_GPU_TUNER_STATUS:
			SET_FUNC_AND_CHECK_FOR_NO_TYPEDEF(
				ged_bridge_gpu_tuner_status,
				GPU_TUNER_STATUS);
			break;
		default:
			GED_LOGE("Unknown Bridge ID: %u\n", GED_GET_BRIDGE_ID(psBridgePackageKM->ui32FunctionID));
			break;
		}

		if (pFunc)
			ret = pFunc(pvIn, pvOut);

		if (psBridgePackageKM->i32OutBufferSize > 0)
		{
			if (0 != ged_copy_to_user(psBridgePackageKM->pvParamOut, pvOut, psBridgePackageKM->i32OutBufferSize))
			{
				goto dispatch_exit;
			}
		}
	}

dispatch_exit:
	kfree(pvIn);
	kfree(pvOut);

	return ret;
}

static long ged_ioctl(struct file *pFile, unsigned int ioctlCmd, unsigned long arg)
{
	int ret = -EFAULT;
	GED_BRIDGE_PACKAGE *psBridgePackageKM, *psBridgePackageUM = (GED_BRIDGE_PACKAGE*)arg;
	GED_BRIDGE_PACKAGE sBridgePackageKM;

	psBridgePackageKM = &sBridgePackageKM;
	if (0 != ged_copy_from_user(psBridgePackageKM, psBridgePackageUM, sizeof(GED_BRIDGE_PACKAGE)))
	{
		GED_LOGE("Fail to ged_copy_from_user\n");
		goto unlock_and_return;
	}

	ret = ged_dispatch(pFile, psBridgePackageKM);

unlock_and_return:

	return ret;
}

#ifdef CONFIG_COMPAT
static long ged_ioctl_compat(struct file *pFile, unsigned int ioctlCmd, unsigned long arg)
{
	typedef struct GED_BRIDGE_PACKAGE_32_TAG
	{
		unsigned int    ui32FunctionID;
		int             i32Size;
		unsigned int    ui32ParamIn;
		int             i32InBufferSize;
		unsigned int    ui32ParamOut;
		int             i32OutBufferSize;
	} GED_BRIDGE_PACKAGE_32;

	int ret = -EFAULT;
	GED_BRIDGE_PACKAGE sBridgePackageKM64;
	GED_BRIDGE_PACKAGE_32 sBridgePackageKM32;
	GED_BRIDGE_PACKAGE_32 *psBridgePackageKM32 = &sBridgePackageKM32;
	GED_BRIDGE_PACKAGE_32 *psBridgePackageUM32 = (GED_BRIDGE_PACKAGE_32*)arg;

	if (0 != ged_copy_from_user(psBridgePackageKM32, psBridgePackageUM32, sizeof(GED_BRIDGE_PACKAGE_32)))
	{
		GED_LOGE("Fail to ged_copy_from_user\n");
		goto unlock_and_return;
	}

	sBridgePackageKM64.ui32FunctionID = psBridgePackageKM32->ui32FunctionID;
	sBridgePackageKM64.i32Size = sizeof(GED_BRIDGE_PACKAGE);
	sBridgePackageKM64.pvParamIn = (void*) ((size_t) psBridgePackageKM32->ui32ParamIn);
	sBridgePackageKM64.pvParamOut = (void*) ((size_t) psBridgePackageKM32->ui32ParamOut);
	sBridgePackageKM64.i32InBufferSize = psBridgePackageKM32->i32InBufferSize;
	sBridgePackageKM64.i32OutBufferSize = psBridgePackageKM32->i32OutBufferSize;

	ret = ged_dispatch(pFile, &sBridgePackageKM64);

unlock_and_return:

	return ret;
}
#endif

/******************************************************************************
 * Module related
 *****************************************************************************/

static struct file_operations ged_fops = {
	.owner = THIS_MODULE,
	.open = ged_open,
	.release = ged_release,
	.poll = ged_poll,
	.read = ged_read,
	.write = ged_write,
	.unlocked_ioctl = ged_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ged_ioctl_compat,
#endif
};

#if 0
static struct miscdevice ged_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ged",
	.fops = &ged_fops,
};
#endif

static void ged_exit(void)
{
#ifndef GED_BUFFER_LOG_DISABLE
#ifdef GED_DVFS_DEBUG_BUF
	ged_log_buf_free(ghLogBuf_DVFS);
	ged_log_buf_free(ghLogBuf_ged_srv);
	ghLogBuf_DVFS = 0;
	ghLogBuf_ged_srv = 0;
#endif
#ifdef GED_DEBUG
	ged_log_buf_free(ghLogBuf_GED);
	ghLogBuf_GED = 0;
	ged_log_buf_free(ghLogBuf_GLES);
	ghLogBuf_GLES = 0;
#endif
	ged_log_buf_free(ghLogBuf_GPU);
	ghLogBuf_GPU = 0;
	ged_log_buf_free(ghLogBuf_FENCE);
	ghLogBuf_FENCE = 0;
	ged_log_buf_free(ghLogBuf_HWC);
	ghLogBuf_HWC = 0;
#endif

	ged_fdvfs_exit();

#ifdef MTK_FRR20
	ged_frr_system_exit();
#endif

	ged_kpi_system_exit();

	ged_dvfs_system_exit();

	ged_profile_dvfs_exit();

	//ged_notify_vsync_system_exit();

	ged_notify_sw_vsync_system_exit();

	ged_hal_exit();

	ged_log_system_exit();

	ged_debugFS_exit();

	ged_ge_exit();

	ged_gpu_tuner_exit();
	remove_proc_entry(GED_DRIVER_DEVICE_NAME, NULL);

}

static int ged_init(void)
{
	GED_ERROR err = GED_ERROR_FAIL;

	if (NULL == proc_create(GED_DRIVER_DEVICE_NAME, 0644, NULL, &ged_fops))
	{
		err = GED_ERROR_FAIL;
		GED_LOGE("ged: failed to register ged proc entry!\n");
		goto ERROR;
	}

	err = ged_debugFS_init();
	if (unlikely(err != GED_OK))
	{
		GED_LOGE("ged: failed to init debug FS!\n");
		goto ERROR;
	}

	err = ged_log_system_init();
	if (unlikely(err != GED_OK))
	{
		GED_LOGE("ged: failed to create gedlog entry!\n");
		goto ERROR;
	}

	err = ged_hal_init();
	if (unlikely(err != GED_OK))
	{
		GED_LOGE("ged: failed to create hal entry!\n");
		goto ERROR;
	}

	err = ged_notify_sw_vsync_system_init();
	if (unlikely(err != GED_OK))
	{
		GED_LOGE("ged: failed to init notify sw vsync!\n");
		goto ERROR;
	}

	err = ged_profile_dvfs_init();
	if (unlikely(err != GED_OK))
	{
		GED_LOGE("ged: failed to init profile dvfs!\n");
		goto ERROR;
	}

	err = ged_dvfs_system_init();
	if (unlikely(err != GED_OK))
	{
		GED_LOGE("ged: failed to init common dvfs!\n");
		goto ERROR;
	}

	err = ged_ge_init();
	if (unlikely(err != GED_OK)) {
		GED_LOGE("ged: failed to init gralloc_extra!\n");
		goto ERROR;
	}

	err = ged_kpi_system_init();
	if (unlikely(err != GED_OK)) {
		GED_LOGE("ged: failed to init KPI!\n");
		goto ERROR;
	}

#ifdef MTK_FRR20
	err = ged_frr_system_init();
	if (unlikely(err != GED_OK)) {
		GED_LOGE("ged: failed to init FRR Table!\n");
		goto ERROR;
	}
#endif

#ifdef GED_FDVFS_ENABLE
	err = ged_fdvfs_system_init();
	if (unlikely(err != GED_OK)) {
		GED_LOGE("ged: failed to init FDVFS!\n");
		goto ERROR;
	}
#endif
#ifndef GED_BUFFER_LOG_DISABLE
	/* common gpu info buffer */
	ged_log_buf_alloc(1024, 64 * 1024, GED_LOG_BUF_TYPE_RINGBUFFER, "gpuinfo", "gpuinfo");

	ghLogBuf_GPU = ged_log_buf_alloc(512, 128 * 512,
				GED_LOG_BUF_TYPE_RINGBUFFER, "GPU_FENCE", NULL);

#ifdef GED_DEBUG
	ghLogBuf_GLES = ged_log_buf_alloc(160, 128 * 160, GED_LOG_BUF_TYPE_RINGBUFFER, GED_LOG_BUF_COMMON_GLES, NULL);
	ghLogBuf_GED = ged_log_buf_alloc(32, 64 * 32, GED_LOG_BUF_TYPE_RINGBUFFER, "GED internal", NULL);
#endif
	ghLogBuf_HWC_ERR = ged_log_buf_alloc(2048, 2048 * 128,
			GED_LOG_BUF_TYPE_RINGBUFFER, GED_LOG_BUF_COMMON_HWC_ERR, NULL);
	ghLogBuf_HWC = ged_log_buf_alloc(4096, 128 * 4096,
			GED_LOG_BUF_TYPE_RINGBUFFER, GED_LOG_BUF_COMMON_HWC, NULL);
	ghLogBuf_FENCE = ged_log_buf_alloc(256, 128 * 256, GED_LOG_BUF_TYPE_RINGBUFFER, GED_LOG_BUF_COMMON_FENCE, NULL);
	ghLogBuf_ftrace = ged_log_buf_alloc(1024*32, 1024*1024, GED_LOG_BUF_TYPE_RINGBUFFER,
						"fence_trace", "fence_trace");
	ghLogBuf_FWTrace = ged_log_buf_alloc(1024*32, 1024*1024, GED_LOG_BUF_TYPE_QUEUEBUFFER, "fw_trace", "fw_trace");

#ifdef GED_DVFS_DEBUG_BUF
	ghLogBuf_DVFS =  ged_log_buf_alloc(20*60, 20*60*100
		, GED_LOG_BUF_TYPE_RINGBUFFER
		, "DVFS_Log", "ged_dvfs_debug");
	ghLogBuf_ged_srv =  ged_log_buf_alloc(32, 32*80, GED_LOG_BUF_TYPE_RINGBUFFER, "ged_srv_Log", "ged_srv_debug");
#endif
#endif

	err = ged_gpu_tuner_init();
	if (unlikely(err != GED_OK)) {
		GED_LOGE("ged: failed to init GPU Tuner!\n");
		goto ERROR;
	}

	return 0;

ERROR:
	ged_exit();

	return -EFAULT;
}

#ifdef GED_MODULE_LATE_INIT
late_initcall(ged_init);

#else
module_init(ged_init);
#endif
module_exit(ged_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GED Driver");
MODULE_AUTHOR("MediaTek Inc.");
