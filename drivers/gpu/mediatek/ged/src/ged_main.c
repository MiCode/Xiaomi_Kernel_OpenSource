// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mt-plat/aee.h>

#ifdef GED_DEBUG_FS
#include "ged_debugFS.h"
#endif
#include "ged_sysfs.h"
#include "ged_log.h"
#include "ged_hal.h"
#include "ged_bridge.h"
#include "ged_monitor_3D_fence.h"
#include "ged_notify_sw_vsync.h"
#include "ged_dvfs.h"
#include "ged_kpi.h"
#include "ged_ge.h"
#include "ged_gpu_tuner.h"

/**
 * ===============================================
 * SECTION : Local functions declaration
 * ===============================================
 */
static int ged_open(struct inode *inode, struct file *filp);
static int ged_release(struct inode *inode, struct file *filp);
static unsigned int ged_poll(struct file *file,
	struct poll_table_struct *ptable);
static ssize_t ged_read(struct file *filp,
	char __user *buf, size_t count, loff_t *f_pos);
static ssize_t ged_write(struct file *filp,
	const char __user *buf, size_t count, loff_t *f_pos);
static long ged_dispatch(struct file *pFile,
	struct GED_BRIDGE_PACKAGE *psBridgePackageKM);
static long ged_ioctl(struct file *pFile,
	unsigned int ioctlCmd, unsigned long arg);
#ifdef CONFIG_COMPAT
static long ged_ioctl_compat(struct file *pFile,
	unsigned int ioctlCmd, unsigned long arg);
#endif
static int ged_pdrv_probe(struct platform_device *pdev);
static void ged_exit(void);
static int ged_init(void);

/**
 * ===============================================
 * SECTION : Local variables definition
 * ===============================================
 */
#define GED_DRIVER_DEVICE_NAME "ged"

static GED_LOG_BUF_HANDLE ghLogBuf_GPU;

#ifdef GED_DEBUG
#define GED_LOG_BUF_COMMON_GLES "GLES"
static GED_LOG_BUF_HANDLE ghLogBuf_GLES;
GED_LOG_BUF_HANDLE ghLogBuf_GED;
#endif /* GED_DEBUG */

#define GED_LOG_BUF_COMMON_HWC_ERR "HWC_err"
static GED_LOG_BUF_HANDLE ghLogBuf_HWC_ERR;
#define GED_LOG_BUF_COMMON_HWC "HWC"
static GED_LOG_BUF_HANDLE ghLogBuf_HWC;
#define GED_LOG_BUF_COMMON_FENCE "FENCE"
static GED_LOG_BUF_HANDLE ghLogBuf_FENCE;
static GED_LOG_BUF_HANDLE ghLogBuf_ftrace;

#ifdef GED_DVFS_DEBUG_BUF
GED_LOG_BUF_HANDLE ghLogBuf_DVFS;
#endif /* GED_DVFS_DEBUG_BUF */

GED_LOG_BUF_HANDLE gpufreq_ged_log;

static const struct of_device_id g_ged_of_match[] = {
	{ .compatible = "mediatek,ged" },
	{ /* sentinel */ }
};
static struct platform_driver g_ged_pdrv = {
	.probe = ged_pdrv_probe,
	.remove = NULL,
	.driver = {
		.name = "ged",
		.owner = THIS_MODULE,
		.of_match_table = g_ged_of_match,
	},
};

static const struct file_operations ged_fops = {
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

/******************************************************************************
 * GED File operations
 *****************************************************************************/
static int ged_open(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	GED_LOGD("@%s: %d:%d\n", __func__, MAJOR(inode->i_rdev),
		MINOR(inode->i_rdev));
	return 0;
}

static int ged_release(struct inode *inode, struct file *filp)
{
	if (filp->private_data) {
		void (*free_func)(void *f) =
			((struct GED_FILE_PRIVATE_BASE *)
			filp->private_data)->free_func;

		free_func(filp->private_data);
	}
	GED_LOGD("@%s: %d:%d\n", __func__, MAJOR(inode->i_rdev),
		MINOR(inode->i_rdev));
	return 0;
}

static unsigned int
ged_poll(struct file *file, struct poll_table_struct *ptable)
{
	return 0;
}

static ssize_t
ged_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static ssize_t ged_write(struct file *filp,
	const char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static long ged_dispatch(struct file *pFile,
	struct GED_BRIDGE_PACKAGE *psBridgePackageKM)
{
	int ret = -EFAULT;
	void *pvIn = NULL, *pvOut = NULL;

	/* We make sure the both size are GE 0 integer.
	 */
	if (psBridgePackageKM->i32InBufferSize >= 0
		&& psBridgePackageKM->i32OutBufferSize >= 0) {

		if (psBridgePackageKM->i32InBufferSize > 0) {
			int32_t inputBufferSize =
					psBridgePackageKM->i32InBufferSize;

			if (GED_BRIDGE_COMMAND_GE_ALLOC ==
					GED_GET_BRIDGE_ID(
					psBridgePackageKM->ui32FunctionID)) {
				inputBufferSize = sizeof(int) +
				sizeof(uint32_t) * GE_ALLOC_STRUCT_NUM;
			}

			pvIn = kmalloc(inputBufferSize, GFP_KERNEL);

			if (pvIn == NULL)
				goto dispatch_exit;

			if (ged_copy_from_user(pvIn,
				psBridgePackageKM->pvParamIn,
				psBridgePackageKM->i32InBufferSize) != 0) {
				GED_LOGE("Failed to ged_copy_from_user\n");
				goto dispatch_exit;
			}
		}

		if (psBridgePackageKM->i32OutBufferSize > 0) {
			pvOut = kzalloc(psBridgePackageKM->i32OutBufferSize,
				GFP_KERNEL);

			if (pvOut == NULL)
				goto dispatch_exit;
		}

		/* Make sure that the UM will never break the KM.
		 * Check IO size are both matched the size of IO sturct.
		 */
#define VALIDATE_ARG(struct_name) do { \
	if (sizeof(struct GED_BRIDGE_IN_##struct_name)\
		> psBridgePackageKM->i32InBufferSize ||\
		sizeof(struct GED_BRIDGE_OUT_##struct_name)\
		> psBridgePackageKM->i32OutBufferSize) {\
		GED_LOGE("GED_BRIDGE_COMMAND_##cmd failed io_size:",\
		"%d/%d, expected: %zu/%zu",\
		psBridgePackageKM->i32InBufferSize,\
		psBridgePackageKM->i32OutBufferSize,\
		sizeof(struct GED_BRIDGE_IN_##struct_name),\
		sizeof(struct GED_BRIDGE_OUT_##struct_name));\
		goto dispatch_exit;\
	} } while (0)

		/* we will change the below switch into
		 * a function pointer mapping table in the future
		 */
		switch (GED_GET_BRIDGE_ID(psBridgePackageKM->ui32FunctionID)) {
		case GED_BRIDGE_COMMAND_LOG_BUF_GET:
			VALIDATE_ARG(LOGBUFGET);
			ret = ged_bridge_log_buf_get(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_LOG_BUF_WRITE:
			VALIDATE_ARG(LOGBUFWRITE);
			ret = ged_bridge_log_buf_write(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_LOG_BUF_RESET:
			VALIDATE_ARG(LOGBUFRESET);
			ret = ged_bridge_log_buf_reset(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_BOOST_GPU_FREQ:
			VALIDATE_ARG(BOOSTGPUFREQ);
			ret = ged_bridge_boost_gpu_freq(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_MONITOR_3D_FENCE:
			VALIDATE_ARG(MONITOR3DFENCE);
			ret = ged_bridge_monitor_3D_fence(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_QUERY_INFO:
			VALIDATE_ARG(QUERY_INFO);
			ret = ged_bridge_query_info(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_NOTIFY_VSYNC:
			VALIDATE_ARG(NOTIFY_VSYNC);
			ret = ged_bridge_notify_vsync(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_DVFS_PROBE:
			VALIDATE_ARG(DVFS_PROBE);
			ret = ged_bridge_dvfs_probe(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_DVFS_UM_RETURN:
			VALIDATE_ARG(DVFS_UM_RETURN);
			ret = ged_bridge_dvfs_um_retrun(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_EVENT_NOTIFY:
			VALIDATE_ARG(EVENT_NOTIFY);
			ret = ged_bridge_event_notify(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_GPU_HINT_TO_CPU:
			VALIDATE_ARG(GPU_HINT_TO_CPU);
			ret = ged_bridge_gpu_hint_to_cpu(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_HINT_FORCE_MDP:
			VALIDATE_ARG(HINT_FORCE_MDP);
			ret = ged_bridge_hint_force_mdp(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_GE_ALLOC:
			VALIDATE_ARG(GE_ALLOC);
			ret = ged_bridge_ge_alloc(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_GE_GET:
			VALIDATE_ARG(GE_GET);
			ret = ged_bridge_ge_get(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_GE_SET:
			VALIDATE_ARG(GE_SET);
			ret = ged_bridge_ge_set(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_GE_INFO:
			VALIDATE_ARG(GE_INFO);
			ret = ged_bridge_ge_info(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_GPU_TIMESTAMP:
			VALIDATE_ARG(GPU_TIMESTAMP);
			ret = ged_bridge_gpu_timestamp(pvIn, pvOut);
			break;
		case GED_BRIDGE_COMMAND_GPU_TUNER_STATUS:
			VALIDATE_ARG(GPU_TUNER_STATUS);
			ret = ged_bridge_gpu_tuner_status(pvIn, pvOut);
			break;
		default:
			GED_LOGE("Unknown Bridge ID: %u\n",
			GED_GET_BRIDGE_ID(psBridgePackageKM->ui32FunctionID));
			break;
		}

		if (psBridgePackageKM->i32OutBufferSize > 0) {
			if (ged_copy_to_user(psBridgePackageKM->pvParamOut,
			pvOut, psBridgePackageKM->i32OutBufferSize) != 0) {
				goto dispatch_exit;
			}
		}
	}

dispatch_exit:
	kfree(pvIn);
	kfree(pvOut);

	return ret;
}

static long
ged_ioctl(struct file *pFile, unsigned int ioctlCmd, unsigned long arg)
{
	int ret = -EFAULT;
	struct GED_BRIDGE_PACKAGE *psBridgePackageKM;
	struct GED_BRIDGE_PACKAGE *psBridgePackageUM =
		(struct GED_BRIDGE_PACKAGE *)arg;
	struct GED_BRIDGE_PACKAGE sBridgePackageKM;

	psBridgePackageKM = &sBridgePackageKM;
	if (ged_copy_from_user(psBridgePackageKM, psBridgePackageUM,
		sizeof(struct GED_BRIDGE_PACKAGE)) != 0) {
		GED_LOGE("Failed to ged_copy_from_user\n");
		goto unlock_and_return;
	}

	ret = ged_dispatch(pFile, psBridgePackageKM);

unlock_and_return:

	return ret;
}

#ifdef CONFIG_COMPAT
static long
ged_ioctl_compat(struct file *pFile, unsigned int ioctlCmd, unsigned long arg)
{
	struct GED_BRIDGE_PACKAGE_32 {
		unsigned int    ui32FunctionID;
		int             i32Size;
		unsigned int    ui32ParamIn;
		int             i32InBufferSize;
		unsigned int    ui32ParamOut;
		int             i32OutBufferSize;
	};

	int ret = -EFAULT;
	struct GED_BRIDGE_PACKAGE sBridgePackageKM64;
	struct GED_BRIDGE_PACKAGE_32 sBridgePackageKM32;
	struct GED_BRIDGE_PACKAGE_32 *psBridgePackageKM32 =
		&sBridgePackageKM32;
	struct GED_BRIDGE_PACKAGE_32 *psBridgePackageUM32 =
		(struct GED_BRIDGE_PACKAGE_32 *)arg;

	if (ged_copy_from_user(psBridgePackageKM32,
		psBridgePackageUM32,
		sizeof(struct GED_BRIDGE_PACKAGE_32)) != 0) {
		GED_LOGE("Failed to ged_copy_from_user\n");
		goto unlock_and_return;
	}

	sBridgePackageKM64.ui32FunctionID = psBridgePackageKM32->ui32FunctionID;
	sBridgePackageKM64.i32Size = sizeof(struct GED_BRIDGE_PACKAGE);
	sBridgePackageKM64.pvParamIn =
		(void *) ((size_t) psBridgePackageKM32->ui32ParamIn);
	sBridgePackageKM64.pvParamOut =
		(void *) ((size_t) psBridgePackageKM32->ui32ParamOut);
	sBridgePackageKM64.i32InBufferSize =
		psBridgePackageKM32->i32InBufferSize;
	sBridgePackageKM64.i32OutBufferSize =
		psBridgePackageKM32->i32OutBufferSize;

	ret = ged_dispatch(pFile, &sBridgePackageKM64);

unlock_and_return:

	return ret;
}
#endif

/******************************************************************************
 * Module related
 *****************************************************************************/
/*
 * ged driver probe
 */
static int ged_pdrv_probe(struct platform_device *pdev)
{
	GED_LOGI("@%s: ged driver probe\n", __func__);

	return 0;
}

/*
 * unregister the gpufreq driver, remove fs node
 */
static void ged_exit(void)
{
#ifndef GED_BUFFER_LOG_DISABLE
	ged_log_buf_free(gpufreq_ged_log);
	gpufreq_ged_log = 0;

#ifdef GED_DVFS_DEBUG_BUF
	ged_log_buf_free(ghLogBuf_DVFS);
	ghLogBuf_DVFS = 0;
#endif

	ged_log_buf_free(ghLogBuf_ftrace);
	ghLogBuf_ftrace = 0;
	ged_log_buf_free(ghLogBuf_FENCE);
	ghLogBuf_FENCE = 0;
	ged_log_buf_free(ghLogBuf_HWC);
	ghLogBuf_HWC = 0;
	ged_log_buf_free(ghLogBuf_HWC_ERR);
	ghLogBuf_HWC_ERR = 0;

#ifdef GED_DEBUG
	ged_log_buf_free(ghLogBuf_GED);
	ghLogBuf_GED = 0;
	ged_log_buf_free(ghLogBuf_GLES);
	ghLogBuf_GLES = 0;
#endif

	ged_log_buf_free(ghLogBuf_GPU);
	ghLogBuf_GPU = 0;
#endif /* GED_BUFFER_LOG_DISABLE */

	ged_gpu_tuner_exit();

	ged_kpi_system_exit();

	ged_ge_exit();

	ged_dvfs_system_exit();

	ged_notify_sw_vsync_system_exit();

	ged_hal_exit();

	ged_log_system_exit();

#ifdef GED_DEBUG_FS
	ged_debugFS_exit();
#endif

	ged_sysfs_exit();

	remove_proc_entry(GED_DRIVER_DEVICE_NAME, NULL);

	platform_driver_unregister(&g_ged_pdrv);
}

/*
 * register the ged driver, create fs node
 */
static int ged_init(void)
{
	GED_ERROR err = GED_ERROR_FAIL;

	GED_LOGI("@%s: start to initialize ged driver\n", __func__);
	if (proc_create(GED_DRIVER_DEVICE_NAME, 0644, NULL, &ged_fops)
		== NULL) {
		err = GED_ERROR_FAIL;
		GED_LOGE("Failed to register ged proc entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_init();
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to init sys FS!\n");
		goto ERROR;
	}

#ifdef GED_DEBUG_FS
	err = ged_debugFS_init();
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to init debug FS!\n");
		goto ERROR;
	}
#endif

	err = ged_log_system_init();
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gedlog entry!\n");
		goto ERROR;
	}

	err = ged_hal_init();
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create hal entry!\n");
		goto ERROR;
	}

	err = ged_notify_sw_vsync_system_init();
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to init notify sw vsync!\n");
		goto ERROR;
	}

	err = ged_dvfs_system_init();
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to init common dvfs!\n");
		goto ERROR;
	}

	err = ged_ge_init();
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to init gralloc_extra!\n");
		goto ERROR;
	}

	err = ged_kpi_system_init();
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to init KPI!\n");
		goto ERROR;
	}

	err = ged_gpu_tuner_init();
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to init GPU Tuner!\n");
		goto ERROR;
	}

#ifndef GED_BUFFER_LOG_DISABLE
	ghLogBuf_GPU = ged_log_buf_alloc(512, 128 * 512,
		GED_LOG_BUF_TYPE_RINGBUFFER, "GPU_FENCE", NULL);

#ifdef GED_DEBUG
	ghLogBuf_GLES = ged_log_buf_alloc(160, 128 * 160,
		GED_LOG_BUF_TYPE_RINGBUFFER, GED_LOG_BUF_COMMON_GLES, NULL);
	ghLogBuf_GED = ged_log_buf_alloc(32, 64 * 32,
		GED_LOG_BUF_TYPE_RINGBUFFER, "GED internal", NULL);
#endif

	ghLogBuf_HWC_ERR = ged_log_buf_alloc(2048, 2048 * 128,
		GED_LOG_BUF_TYPE_RINGBUFFER, GED_LOG_BUF_COMMON_HWC_ERR, NULL);
	ghLogBuf_HWC = ged_log_buf_alloc(4096, 128 * 4096,
		GED_LOG_BUF_TYPE_RINGBUFFER, GED_LOG_BUF_COMMON_HWC, NULL);
	ghLogBuf_FENCE = ged_log_buf_alloc(256, 128 * 256,
		GED_LOG_BUF_TYPE_RINGBUFFER, GED_LOG_BUF_COMMON_FENCE, NULL);
	ghLogBuf_ftrace = ged_log_buf_alloc(1024*32, 1024*1024,
		GED_LOG_BUF_TYPE_RINGBUFFER,
		"fence_trace", "fence_trace");

#ifdef GED_DVFS_DEBUG_BUF
	ghLogBuf_DVFS =  ged_log_buf_alloc(20*60, 20*60*100
		, GED_LOG_BUF_TYPE_RINGBUFFER
		, "DVFS_Log", "ged_dvfs_debug");
#endif

	gpufreq_ged_log = ged_log_buf_alloc(1024, 64 * 1024,
			GED_LOG_BUF_TYPE_RINGBUFFER, "gfreq", "gfreq");
#else
	ghLogBuf_GPU = 0;

#ifdef GED_DEBUG
	ghLogBuf_GLES = 0;
	ghLogBuf_GED = 0;
#endif

	ghLogBuf_HWC_ERR = 0;
	ghLogBuf_HWC = 0;
	ghLogBuf_FENCE = 0;
	ghLogBuf_ftrace = 0;

#ifdef GED_DVFS_DEBUG_BUF
	ghLogBuf_DVFS = 0;
#endif

	gpufreq_ged_log = 0;
#endif /* GED_BUFFER_LOG_DISABLE */

	/* register platform driver */
	err = platform_driver_register(&g_ged_pdrv);
	if (err) {
		GED_LOGE("@%s: failed to register ged driver\n",
		__func__);
		goto ERROR;
	}

	return 0;

ERROR:
	ged_exit();

	return -EFAULT;
}

module_init(ged_init);
module_exit(ged_exit);

MODULE_DEVICE_TABLE(of, g_ged_of_match);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GED Driver");
MODULE_AUTHOR("MediaTek Inc.");
