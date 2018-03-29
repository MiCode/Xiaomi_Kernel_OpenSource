/*
 * (C) Copyright 2010
 * MediaTek <www.MediaTek.com>
 *
 * MTK GPU Extension Device
 *
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
//#include <mach/system.h>
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

#ifdef ENABLE_FRR_FOR_MT6XXX_PLATFORM
#include "ged_vsync.h"
#endif

#include "ged_ge.h"

#define GED_DRIVER_DEVICE_NAME "ged"

#define GED_IOCTL_PARAM_BUF_SIZE 0x3000 //12KB

#ifdef GED_DEBUG
#define GED_LOG_BUF_COMMON_GLES "GLES"
static GED_LOG_BUF_HANDLE ghLogBuf_GLES = 0;
GED_LOG_BUF_HANDLE ghLogBuf_GED = 0;
#endif

#define GED_LOG_BUF_COMMON_HWC "HWC"
static GED_LOG_BUF_HANDLE ghLogBuf_HWC = 0;
#define GED_LOG_BUF_COMMON_FENCE "FENCE"
static GED_LOG_BUF_HANDLE ghLogBuf_FENCE = 0;

GED_LOG_BUF_HANDLE ghLogBuf_DVFS = 0;
GED_LOG_BUF_HANDLE ghLogBuf_ged_srv = 0;



static void* gvIOCTLParamBuf = NULL;

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
	void *pvInt, *pvOut;
	typedef int (ged_bridge_func_type)(void *, void *);
	ged_bridge_func_type* pFunc = NULL;

	/* We make sure the both size and the sum of them are GE 0 integer.
	 * The sum will not overflow to zero, because we will get zero from two GE 0 integers
	 * if and only if they are both zero in a 2's complement numeral system.
	 * That is: if overflow happen, the sum will be a negative number.
	 */
	if (psBridgePackageKM->i32InBufferSize >= 0 && psBridgePackageKM->i32OutBufferSize >= 0
			&& psBridgePackageKM->i32InBufferSize + psBridgePackageKM->i32OutBufferSize >= 0
			&& psBridgePackageKM->i32InBufferSize + psBridgePackageKM->i32OutBufferSize
			< GED_IOCTL_PARAM_BUF_SIZE)
	{
		pvInt = gvIOCTLParamBuf;
		pvOut = (void *)((char *)pvInt + (uintptr_t)psBridgePackageKM->i32InBufferSize);
		if (psBridgePackageKM->i32InBufferSize > 0)
		{
			if (0 != ged_copy_from_user(pvInt, psBridgePackageKM->pvParamIn, psBridgePackageKM->i32InBufferSize))
			{
				GED_LOGE("ged_copy_from_user fail\n");
				return ret;
			}
		}

		// we will change the below switch into a function pointer mapping table in the future
		switch (GED_GET_BRIDGE_ID(psBridgePackageKM->ui32FunctionID))
		{
			case GED_BRIDGE_COMMAND_LOG_BUF_GET:
				pFunc = (ged_bridge_func_type*)ged_bridge_log_buf_get;
				break;
			case GED_BRIDGE_COMMAND_LOG_BUF_WRITE:
				pFunc = (ged_bridge_func_type*)ged_bridge_log_buf_write;
				break;
			case GED_BRIDGE_COMMAND_LOG_BUF_RESET:
				pFunc = (ged_bridge_func_type*)ged_bridge_log_buf_reset;
				break;
			case GED_BRIDGE_COMMAND_BOOST_GPU_FREQ:
				pFunc = (ged_bridge_func_type*)ged_bridge_boost_gpu_freq;
				break;
			case GED_BRIDGE_COMMAND_MONITOR_3D_FENCE:
				pFunc = (ged_bridge_func_type*)ged_bridge_monitor_3D_fence;
				break;
			case GED_BRIDGE_COMMAND_QUERY_INFO:
				pFunc = (ged_bridge_func_type*)ged_bridge_query_info;
				break;
			case GED_BRIDGE_COMMAND_NOTIFY_VSYNC:
				pFunc = (ged_bridge_func_type*)ged_bridge_notify_vsync;
				break;
			case GED_BRIDGE_COMMAND_DVFS_PROBE:
				pFunc = (ged_bridge_func_type*)ged_bridge_dvfs_probe;
				break;
			case GED_BRIDGE_COMMAND_DVFS_UM_RETURN:
				pFunc = (ged_bridge_func_type*)ged_bridge_dvfs_um_retrun;
				break;
			case GED_BRIDGE_COMMAND_EVENT_NOTIFY:
				pFunc = (ged_bridge_func_type*)ged_bridge_event_notify;
				break;
#ifdef ENABLE_FRR_FOR_MT6XXX_PLATFORM
            case GED_BRIDGE_COMMAND_VSYNC_WAIT:
				pFunc = (ged_bridge_func_type*)ged_bridge_vsync_wait;
				break;
#endif
			case GED_BRIDGE_COMMAND_GE_ALLOC:
				pFunc = (ged_bridge_func_type *)ged_bridge_ge_alloc;
				break;
			case GED_BRIDGE_COMMAND_GE_RETAIN:
				pFunc = (ged_bridge_func_type *)ged_bridge_ge_retain;
				break;
			case GED_BRIDGE_COMMAND_GE_RELEASE:
				pFunc = (ged_bridge_func_type *)ged_bridge_ge_release;
				break;
			case GED_BRIDGE_COMMAND_GE_GET:
				pFunc = (ged_bridge_func_type *)ged_bridge_ge_get;
				break;
			case GED_BRIDGE_COMMAND_GE_SET:
				pFunc = (ged_bridge_func_type *)ged_bridge_ge_set;
				break;
			default:
				GED_LOGE("Unknown Bridge ID: %u\n", GED_GET_BRIDGE_ID(psBridgePackageKM->ui32FunctionID));
				break;
		}

		if (pFunc)
		{
			ret = pFunc(pvInt, pvOut);
		}

		switch (GED_GET_BRIDGE_ID(psBridgePackageKM->ui32FunctionID)) {
		case GED_BRIDGE_COMMAND_GE_ALLOC:
			{
				GED_BRIDGE_OUT_GE_ALLOC *out = (GED_BRIDGE_OUT_GE_ALLOC *)pvOut;

				if (out->eError == GED_OK) {
					ged_ge_init_context(&pFile->private_data);
					ged_ge_context_ref(pFile->private_data, out->ge_hnd);
				}
			}
			break;
		case GED_BRIDGE_COMMAND_GE_RETAIN:
			{
				GED_BRIDGE_IN_GE_RETAIN *in = (GED_BRIDGE_IN_GE_RETAIN *)pvInt;
				GED_BRIDGE_OUT_GE_RETAIN *out = (GED_BRIDGE_OUT_GE_RETAIN *)pvOut;

				if (out->eError == GED_OK) {
					ged_ge_init_context(&pFile->private_data);
					ged_ge_context_ref(pFile->private_data, in->ge_hnd);
				}
			}
			break;
		case GED_BRIDGE_COMMAND_GE_RELEASE:
			{
				GED_BRIDGE_IN_GE_RELEASE *in = (GED_BRIDGE_IN_GE_RELEASE *)pvInt;
				GED_BRIDGE_OUT_GE_RELEASE *out = (GED_BRIDGE_OUT_GE_RELEASE *)pvOut;

				if (out->eError == GED_OK)
					ged_ge_context_deref(pFile->private_data, in->ge_hnd);

			}
			break;
		}

		if (psBridgePackageKM->i32OutBufferSize > 0)
		{
			if (0 != ged_copy_to_user(psBridgePackageKM->pvParamOut, pvOut, psBridgePackageKM->i32OutBufferSize))
			{
				return ret;
			}
		}
	}

	return ret;
}

DEFINE_SEMAPHORE(ged_dal_sem);

static long ged_ioctl(struct file *pFile, unsigned int ioctlCmd, unsigned long arg)
{
	int ret = -EFAULT;
	GED_BRIDGE_PACKAGE *psBridgePackageKM, *psBridgePackageUM = (GED_BRIDGE_PACKAGE*)arg;
	GED_BRIDGE_PACKAGE sBridgePackageKM;

	if (down_interruptible(&ged_dal_sem) < 0)
	{
		GED_LOGE("Fail to down ged_dal_sem\n");
		return -ERESTARTSYS;
	}

	psBridgePackageKM = &sBridgePackageKM;
	if (0 != ged_copy_from_user(psBridgePackageKM, psBridgePackageUM, sizeof(GED_BRIDGE_PACKAGE)))
	{
		GED_LOGE("Fail to ged_copy_from_user\n");
		goto unlock_and_return;
	}

	ret = ged_dispatch(pFile, psBridgePackageKM);

unlock_and_return:
	up(&ged_dal_sem);

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

	if (down_interruptible(&ged_dal_sem) < 0)
	{
		GED_LOGE("Fail to down ged_dal_sem\n");
		return -ERESTARTSYS;
	}

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
	up(&ged_dal_sem);

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
	ged_log_buf_free(ghLogBuf_FENCE);
	ghLogBuf_FENCE = 0;
	ged_log_buf_free(ghLogBuf_HWC);
	ghLogBuf_HWC = 0;

	ged_dvfs_system_exit();

	ged_profile_dvfs_exit();

	//ged_notify_vsync_system_exit();

	ged_notify_sw_vsync_system_exit();

	ged_hal_exit();

	ged_log_system_exit();

	ged_debugFS_exit();

	ged_ge_exit();

	remove_proc_entry(GED_DRIVER_DEVICE_NAME, NULL);

	if (gvIOCTLParamBuf)
	{
		vfree(gvIOCTLParamBuf);
		gvIOCTLParamBuf = NULL;
	}
}

static int ged_init(void)
{
	GED_ERROR err = GED_ERROR_FAIL;

	gvIOCTLParamBuf = vmalloc(GED_IOCTL_PARAM_BUF_SIZE);
	if (NULL == gvIOCTLParamBuf)
	{
		err = GED_ERROR_OOM;
		goto ERROR;
	}

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

	/* common gpu info buffer */
	ged_log_buf_alloc(32, 128 * 32, GED_LOG_BUF_TYPE_QUEUEBUFFER, "gpuinfo", "gpuinfo");

#ifdef GED_DEBUG
	ghLogBuf_GLES = ged_log_buf_alloc(160, 128 * 160, GED_LOG_BUF_TYPE_RINGBUFFER, GED_LOG_BUF_COMMON_GLES, NULL);
	ghLogBuf_GED = ged_log_buf_alloc(32, 64 * 32, GED_LOG_BUF_TYPE_RINGBUFFER, "GED internal", NULL);
#endif
	ghLogBuf_HWC = ged_log_buf_alloc(4096, 128 * 4096, GED_LOG_BUF_TYPE_RINGBUFFER, GED_LOG_BUF_COMMON_HWC, NULL);
	ghLogBuf_FENCE = ged_log_buf_alloc(256, 128 * 256, GED_LOG_BUF_TYPE_RINGBUFFER, GED_LOG_BUF_COMMON_FENCE, NULL);

#ifdef GED_DVFS_DEBUG_BUF
#ifdef GED_LOG_SIZE_LIMITED
	ghLogBuf_DVFS =  ged_log_buf_alloc(20*60, 20*60*80, GED_LOG_BUF_TYPE_RINGBUFFER, "DVFS_Log", "ged_dvfs_debug_limited");
#else
	ghLogBuf_DVFS =  ged_log_buf_alloc(20*60*10, 20*60*10*80, GED_LOG_BUF_TYPE_RINGBUFFER, "DVFS_Log", "ged_dvfs_debug");
#endif
	ghLogBuf_ged_srv =  ged_log_buf_alloc(32, 32*80, GED_LOG_BUF_TYPE_RINGBUFFER, "ged_srv_Log", "ged_srv_debug");
#endif

	return 0;

ERROR:
	ged_exit();

	return -EFAULT;
}

module_init(ged_init);
module_exit(ged_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GED Driver");
MODULE_AUTHOR("MediaTek Inc.");
