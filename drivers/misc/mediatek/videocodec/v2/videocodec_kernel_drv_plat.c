// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *       Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <asm/page.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/proc_fs.h>
#include <linux/semaphore.h>
#include <linux/delay.h>

#if IS_ENABLED(CONFIG_MTK_CLKMGR)
#include "mach/mt_clkmgr.h"
#else
#include <linux/clk.h>
#endif

#if IS_ENABLED(CONFIG_MTK_IOMMU)
#include <linux/iommu.h>
#endif

#if IS_ENABLED(CONFIG_MTK_HIBERNATION)
#include <mtk_hibernate_dpm.h>
#include <mach/diso.h>
#endif

#include "videocodec_kernel_driver.h"
#include "videocodec_kernel.h"
#include <asm/cacheflush.h>
#include <linux/io.h>
#include "val_types_private.h"
#include "hal_types_private.h"
#include "val_api_private.h"
#include "drv_api.h"

#include "mtk_vcodec_pm.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_pm_codec.h"
#include <linux/slab.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/uaccess.h>
#include <linux/compat.h>
#endif

#if IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
#define VCODEC_FPGAPORTING
#endif

/* #define VCODEC_DEBUG */
#ifdef VCODEC_DEBUG
#undef VCODEC_DEBUG
#define VCODEC_DEBUG pr_info
#undef pr_debug
#define pr_debug pr_info
#else
#define VCODEC_DEBUG(...)
#undef pr_debug
#define pr_debug(...)
#endif

#define ENABLE_MMDVFS_VDEC
#ifdef ENABLE_MMDVFS_VDEC
/* <--- MM DVFS related */
#include "mmdvfs_mgr.h"
#include "mtk-smi-bwc.h"
#include <mmdvfs_config_util.h>
#define DROP_PERCENTAGE     50
#define RAISE_PERCENTAGE    90
#define MONITOR_DURATION_MS 4000
#define DVFS_UNREQUEST (-1)
#define DVFS_LOW     MMDVFS_VOLTAGE_LOW
#define DVFS_HIGH    MMDVFS_VOLTAGE_HIGH
#define DVFS_DEFAULT MMDVFS_VOLTAGE_HIGH
#define MONITOR_START_MINUS_1   0
#define SW_OVERHEAD_MS 1
#define PAUSE_DETECTION_GAP     200
#define PAUSE_DETECTION_RATIO   2
static char   gMMDFVFSMonitorStarts = VAL_FALSE;
static char   gFirstDvfsLock = VAL_FALSE;
static unsigned int gMMDFVFSMonitorCounts;
static struct VAL_TIME_T   gMMDFVFSMonitorStartTime;
static struct VAL_TIME_T   gMMDFVFSLastLockTime;
static struct VAL_TIME_T   gMMDFVFSLastUnlockTime;
static struct VAL_TIME_T   gMMDFVFSMonitorEndTime;
static unsigned int gHWLockInterval;
static int  gHWLockMaxDuration;
static unsigned int gHWLockPrevInterval;

unsigned int TimeDiffMs(struct VAL_TIME_T timeOld, struct VAL_TIME_T timeNew)
{
	/* pr_info ("@@ timeOld(%d, %d), timeNew(%d, %d)", */
	/* timeOld.u4Sec, timeOld.u4uSec, timeNew.u4Sec, timeNew.u4uSec); */
	return ((((timeNew.u4Sec - timeOld.u4Sec) * 1000000) + timeNew.u4uSec) -
		 timeOld.u4uSec) / 1000;
}

/* raise/drop voltage */
void SendDvfsRequest(int level)
{
#ifndef VCODEC_FPGAPORTING
	int ret = 0;

#if IS_ENABLED(CONFIG_MTK_SMI_BWC)
	if (level == MMDVFS_VOLTAGE_LOW) {
		pr_debug("[VCODEC][MMDVFS_VDEC] %s(MMDVFS_FINE_STEP_OPP3)",
			__func__);
		ret = mmdvfs_set_fine_step(SMI_BWC_SCEN_VP, MMDVFS_FINE_STEP_OPP3);
	} else if (level == MMDVFS_VOLTAGE_HIGH) {
		pr_debug("[VCODEC][MMDVFS_VDEC] %s(MMDVFS_FINE_STEP_OPP0)",
			__func__);
		ret = mmdvfs_set_fine_step(SMI_BWC_SCEN_VP, MMDVFS_FINE_STEP_OPP0);
	} else if (level == DVFS_UNREQUEST) {
		pr_debug("[VCODEC][MMDVFS_VDEC] %s(MMDVFS_FINE_STEP_UNREQUEST)",
			__func__);
		ret = mmdvfs_set_fine_step(SMI_BWC_SCEN_VP, MMDVFS_FINE_STEP_UNREQUEST);
	} else {
		pr_debug("[VCODEC][MMDVFS_VDEC] OOPS: level = %d\n", level);
	}
#endif

	if (ret != 0) {
		/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
		pr_info("[VCODEC][MMDVFS_VDEC] OOPS: mmdvfs_set_fine_step error!");
	}
#endif
}

void VdecDvfsBegin(void)
{
	gMMDFVFSMonitorStarts = VAL_TRUE;
	gMMDFVFSMonitorCounts = 0;
	gHWLockInterval = 0;
	gFirstDvfsLock = VAL_TRUE;
	gHWLockMaxDuration = 0;
	gHWLockPrevInterval = 999999;
	pr_debug("[VCODEC][MMDVFS_VDEC] %s", __func__);
/* eVideoGetTimeOfDay(&gMMDFVFSMonitorStartTime, sizeof(struct VAL_TIME_T)); */
}

unsigned int VdecDvfsGetMonitorDuration(void)
{
	eVideoGetTimeOfDay(&gMMDFVFSMonitorEndTime, sizeof(struct VAL_TIME_T));
	return TimeDiffMs(gMMDFVFSMonitorStartTime, gMMDFVFSMonitorEndTime);
}

void VdecDvfsEnd(int level)
{
	pr_debug("[VCODEC][MMDVFS_VDEC] VdecDVFS monitor %dms, decoded %d frames\n",
		 MONITOR_DURATION_MS,
		 gMMDFVFSMonitorCounts);
	pr_debug("[VCODEC][MMDVFS_VDEC] total time %d, max duration %d, target lv %d\n",
		 gHWLockInterval,
		 gHWLockMaxDuration,
		 level);
	gMMDFVFSMonitorStarts = VAL_FALSE;
	gMMDFVFSMonitorCounts = 0;
	gHWLockInterval = 0;
	gHWLockMaxDuration = 0;
}

unsigned int VdecDvfsStep(void)
{
	unsigned int _diff = 0;

	eVideoGetTimeOfDay(&gMMDFVFSLastUnlockTime, sizeof(struct VAL_TIME_T));
	_diff = TimeDiffMs(gMMDFVFSLastLockTime, gMMDFVFSLastUnlockTime);
	if (_diff > gHWLockMaxDuration) {
		/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
		gHWLockMaxDuration = _diff;
	}
	gHWLockInterval += (_diff + SW_OVERHEAD_MS);
	return _diff;
}

void VdecDvfsAdjustment(void)
{
	unsigned int _monitor_duration = 0;
	unsigned int _diff = 0;
	unsigned int _perc = 0;

	if (gMMDFVFSMonitorStarts == VAL_TRUE && gMMDFVFSMonitorCounts > MONITOR_START_MINUS_1) {
		_monitor_duration = VdecDvfsGetMonitorDuration();
		if (_monitor_duration < MONITOR_DURATION_MS) {
			_diff = VdecDvfsStep();
			pr_debug("[VCODEC][MMDVFS_VDEC] lock time(%d ms, %d ms), cnt=%d, _monitor_duration=%d\n",
				 _diff, gHWLockInterval,
				 gMMDFVFSMonitorCounts,
				 _monitor_duration);
		} else {
			VdecDvfsStep();
			_perc = (unsigned int)(100 * gHWLockInterval
				/ _monitor_duration);
			pr_debug("[VCODEC][MMDVFS_VDEC] DROP_PERCENTAGE = %d, RAISE_PERCENTAGE = %d\n",
				 DROP_PERCENTAGE, RAISE_PERCENTAGE);
			pr_debug("[VCODEC][MMDVFS_VDEC] reset monitor duration (%d ms), percent: %d\n",
				 _monitor_duration, _perc);
			if (_perc < DROP_PERCENTAGE) {
				SendDvfsRequest(DVFS_LOW);
				VdecDvfsEnd(DVFS_LOW);
			} else if (_perc > RAISE_PERCENTAGE) {
				SendDvfsRequest(DVFS_HIGH);
				VdecDvfsEnd(DVFS_HIGH);
			} else {
				VdecDvfsEnd(-1);
			}
		}
	}
	gMMDFVFSMonitorCounts++;
}

void VdecDvfsMonitorStart(void)
{
	unsigned int _diff = 0;
	struct VAL_TIME_T   _now;

	if (gMMDFVFSMonitorStarts == VAL_TRUE) {
		eVideoGetTimeOfDay(&_now, sizeof(struct VAL_TIME_T));
		_diff = TimeDiffMs(gMMDFVFSLastUnlockTime, _now);
	/* pr_debug("[VCODEC][MMDVFS_VDEC]
	 * Pause handle prev_diff = %dms, diff = %dms\n",
	 */
		/*gHWLockPrevInterval, _diff); */
		if (_diff > PAUSE_DETECTION_GAP &&
			_diff > gHWLockPrevInterval * PAUSE_DETECTION_RATIO) {
			/* pr_debug("[VCODEC][MMDVFS_VDEC] Pause detected, reset\n"); */
			/* Reset monitoring period if pause is detected */
			SendDvfsRequest(DVFS_HIGH);
			VdecDvfsBegin();
		}
		gHWLockPrevInterval = _diff;
	}
	if (gMMDFVFSMonitorStarts == VAL_FALSE) {
		/* Continuous monitoring */
		VdecDvfsBegin();
	}
	if (gMMDFVFSMonitorStarts == VAL_TRUE) {
		pr_debug("[VCODEC][MMDVFS_VDEC] LOCK 1\n");
		if (gMMDFVFSMonitorCounts > MONITOR_START_MINUS_1) {
			if (gFirstDvfsLock == VAL_TRUE) {
				gFirstDvfsLock = VAL_FALSE;
	/* pr_debug("[VCODEC][MMDVFS_VDEC] LOCK 1 start monitor*/
	/*		 instance = 0x%p\n", grVcodecDecHWLock.pvHandle); */
				eVideoGetTimeOfDay(&gMMDFVFSMonitorStartTime,
					sizeof(struct VAL_TIME_T));
			}
			eVideoGetTimeOfDay(&gMMDFVFSLastLockTime,
				sizeof(struct VAL_TIME_T));
		}
	}
}
/* ---> */
#endif

void vdec_power_on(struct mtk_vcodec_dev *dev)
{
	int ret = 0;

	mutex_lock(&gDrvInitParams->vdecPWRLock);
	gDrvInitParams->u4VdecPWRCounter++;
	mutex_unlock(&gDrvInitParams->vdecPWRLock);
	ret = 0;
	mtk_vcodec_clock_on(MTK_INST_DECODER, dev);
}

void vdec_power_off(struct mtk_vcodec_dev *dev)
{
	mutex_lock(&gDrvInitParams->vdecPWRLock);
	if (gDrvInitParams->u4VdecPWRCounter == 0) {
		pr_debug("[VCODEC] gDrvInitParams->u4VdecPWRCounter = 0\n");
	} else {
		gDrvInitParams->u4VdecPWRCounter--;
		mtk_vcodec_clock_off(MTK_INST_DECODER, dev);

	}
	mutex_unlock(&gDrvInitParams->vdecPWRLock);
}

void venc_power_on(struct mtk_vcodec_dev *dev)
{
	int ret = 0;

	mutex_lock(&gDrvInitParams->vencPWRLock);
	gDrvInitParams->u4VencPWRCounter++;
	mutex_unlock(&gDrvInitParams->vencPWRLock);
	ret = 0;

	pr_debug("[VCODEC] %s +\n", __func__);

	mtk_vcodec_clock_on(MTK_INST_ENCODER, dev);
	pr_debug("[VCODEC] %s -\n", __func__);
}

void venc_power_off(struct mtk_vcodec_dev *dev)
{
	mutex_lock(&gDrvInitParams->vencPWRLock);
	if (gDrvInitParams->u4VencPWRCounter == 0) {
		pr_debug("[VCODEC] gu4VencPWRCounter = 0\n");
	} else {
		gDrvInitParams->u4VencPWRCounter--;
		pr_debug("[VCODEC] %s +\n", __func__);
		mtk_vcodec_clock_off(MTK_INST_ENCODER, dev);
		pr_debug("[VCODEC] %s -\n", __func__);
	}
	mutex_unlock(&gDrvInitParams->vencPWRLock);
}

static long vcodec_lockhw_dec_fail(struct VAL_HW_LOCK_T rHWLock,
	unsigned int FirstUseDecHW)
{
	pr_info("[ERROR]@1 VCODEC_LOCKHW, Dec HWLockEvent TimeOut, CurrentTID = %d\n",
		current->pid);
	if (FirstUseDecHW != 1) {
		mutex_lock(&gDrvInitParams->hwLock);
		if (CodecHWLock.pvHandle == 0) {
			/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
			pr_info("[WARNING] VCODEC_LOCKHW, maybe mediaserver restart before, please check!!\n");
		} else {
			/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
			pr_info("[WARNING] VCODEC_LOCKHW, someone use HW, and check timeout value!!\n");
		}
		mutex_unlock(&gDrvInitParams->hwLock);
	}

	return 0;
}

static long vcodec_lockhw_enc_fail(struct VAL_HW_LOCK_T rHWLock,
	unsigned int FirstUseEncHW)
{
	pr_info("[ERROR] VCODEC_LOCKHW Enc HWLockEvent TimeOut, CurrentTID = %d\n",
		current->pid);

	if (FirstUseEncHW != 1) {
		mutex_lock(&gDrvInitParams->hwLock);
		if (CodecHWLock.pvHandle == 0) {
			pr_info("[WARNING] VCODEC_LOCKHW, maybe mediaserver restart before, please check!!\n");
		} else {
			pr_info("[WARNING] VCODEC_LOCKHW, someone use HW, and check timeout value!! %d\n",
				 gLockTimeOutCount);
			++gLockTimeOutCount;
			if (gLockTimeOutCount > 30) {
				pr_debug("[ERROR] VCODEC_LOCKHW - ID %d fail\n",
					current->pid);
				pr_info("someone locked HW time out more than 30 times 0x%lx,%lx,0x%lx,type:%d\n",
				(unsigned long)CodecHWLock.pvHandle,
				pmem_user_v2p_video(
				(unsigned long)rHWLock.pvHandle),
				(unsigned long)rHWLock.pvHandle,
				rHWLock.eDriverType);
				gLockTimeOutCount = 0;
				mutex_unlock(&gDrvInitParams->hwLock);
				return -EFAULT;
			}

			if (rHWLock.u4TimeoutMs == 0) {
				pr_debug("[ERROR] VCODEC_LOCKHW - ID %d fail\n",
					current->pid);
				pr_info("someone locked HW already 0x%lx,%lx,0x%lx,type:%d\n",
				(unsigned long)CodecHWLock.pvHandle,
				pmem_user_v2p_video(
				(unsigned long)rHWLock.pvHandle),
				(unsigned long)rHWLock.pvHandle,
				rHWLock.eDriverType);
				gLockTimeOutCount = 0;
				mutex_unlock(&gDrvInitParams->hwLock);
				return -EFAULT;
			}
		}
		mutex_unlock(&gDrvInitParams->hwLock);
	}

	return 0;
}

long vcodec_lockhw(unsigned long arg)
{
	unsigned char *user_data_addr;
	struct VAL_HW_LOCK_T rHWLock;
	enum VAL_RESULT_T eValRet;
	long ret;
	char bLockedHW = VAL_FALSE;
	unsigned int FirstUseDecHW = 0;
	unsigned int FirstUseEncHW = 0;
	struct VAL_TIME_T rCurTime;
	unsigned int u4TimeInterval;
	unsigned long ulFlagsLockHW;
	unsigned int u4VcodecSel;
	unsigned int u4DeBlcoking = 1;
	unsigned long handle_id = 0;

	pr_debug("VCODEC_LOCKHW + tid = %d\n", current->pid);

	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&rHWLock, user_data_addr,
		sizeof(struct VAL_HW_LOCK_T));
	if (ret) {
		pr_info("[ERROR] VCODEC_LOCKHW, copy_from_user failed: %lu\n",
			ret);
		return -EFAULT;
	}

	pr_debug("[VCODEC] LOCKHW eDriverType = %d\n", rHWLock.eDriverType);
	eValRet = VAL_RESULT_INVALID_ISR;
	if (rHWLock.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_ADV_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_DEC) {
		while (bLockedHW == VAL_FALSE) {
			mutex_lock(&gDrvInitParams->hwLockEventTimeoutLock);
			if (gDrvInitParams->HWLockEvent.u4TimeoutMs == 1) {
				pr_debug("VCODEC_LOCKHW, First Use Dec HW!!\n");
				FirstUseDecHW = 1;
			} else {
				FirstUseDecHW = 0;
			}
			mutex_unlock(&gDrvInitParams->hwLockEventTimeoutLock);

			if (FirstUseDecHW == 1) {
				eValRet = eVideoWaitEvent(&gDrvInitParams->HWLockEvent,
					sizeof(struct VAL_EVENT_T));
			}
			mutex_lock(&gDrvInitParams->hwLockEventTimeoutLock);
			if (gDrvInitParams->HWLockEvent.u4TimeoutMs != 1000) {
				gDrvInitParams->HWLockEvent.u4TimeoutMs = 1000;
				FirstUseDecHW = 1;
			} else {
				FirstUseDecHW = 0;
			}
			mutex_unlock(&gDrvInitParams->hwLockEventTimeoutLock);

			mutex_lock(&gDrvInitParams->hwLock);
			handle_id = pmem_user_v2p_video((unsigned long)rHWLock.pvHandle);
			if (handle_id == 0) {
				pr_info("[error] handle is freed at %d\n", __LINE__);
				mutex_unlock(&gDrvInitParams->hwLock);
				return -1;
			}

			/* one process try to lock twice */
			if (CodecHWLock.pvHandle ==
				(void *)handle_id) {
				pr_info("[WARNING] VCODEC_LOCKHW, one decoder instance try to lock twice\n");
				pr_debug("may cause lock HW timeout!! instance = 0x%lx, CurrentTID = %d\n",
				(unsigned long)CodecHWLock.pvHandle,
				current->pid);
			}
			mutex_unlock(&gDrvInitParams->hwLock);

			if (FirstUseDecHW == 0) {
				pr_debug("VCODEC_LOCKHW, Not first time use HW, timeout = %d\n",
					 gDrvInitParams->HWLockEvent.u4TimeoutMs);
				eValRet = eVideoWaitEvent(&gDrvInitParams->HWLockEvent,
					sizeof(struct VAL_EVENT_T));
			}

			if (eValRet == VAL_RESULT_INVALID_ISR) {
				ret = vcodec_lockhw_dec_fail(rHWLock,
					FirstUseDecHW);
				if (ret) {
					pr_info("[ERROR] vcodec_lockhw_dec_fail failed: %lu\n",
						ret);
					return -EFAULT;
				}
			} else if (eValRet == VAL_RESULT_RESTARTSYS) {
				pr_info("[WARNING] VCODEC_LOCKHW, VAL_RESULT_RESTARTSYS return when HWLock!!\n");
				return -ERESTARTSYS;
			}

			mutex_lock(&gDrvInitParams->hwLock);
			if (CodecHWLock.pvHandle == 0) { /* No one holds dec hw lock now */
				gDrvInitParams->u4VdecLockThreadId = current->pid;
				handle_id = pmem_user_v2p_video((unsigned long)rHWLock.pvHandle);
				if (handle_id == 0) {
					pr_info("[error] handle is freed at %d\n", __LINE__);
					mutex_unlock(&gDrvInitParams->hwLock);
					return -1;
				}
				CodecHWLock.pvHandle =
					(void *)handle_id;
				CodecHWLock.eDriverType = rHWLock.eDriverType;
				eVideoGetTimeOfDay(&CodecHWLock.rLockedTime,
					sizeof(struct VAL_TIME_T));

				pr_debug("VCODEC_LOCKHW, No process use dec HW, so current process can use HW\n");
				pr_debug("LockInstance = 0x%lx CurrentTID = %d, rLockedTime(s, us) = %d, %d\n",
					 (unsigned long)CodecHWLock.pvHandle,
					 current->pid,
					 CodecHWLock.rLockedTime.u4Sec,
					 CodecHWLock.rLockedTime.u4uSec);

				bLockedHW = VAL_TRUE;
				if (eValRet == VAL_RESULT_INVALID_ISR
					&& FirstUseDecHW != 1) {
					pr_info("[WARNING] VCODEC_LOCKHW, reset power/irq when HWLock!!\n");
#ifndef KS_POWER_WORKAROUND
					vdec_power_off(gVCodecDev);
#endif
					disable_irq(VDEC_IRQ_ID);
				}
#ifndef KS_POWER_WORKAROUND
				vdec_power_on(gVCodecDev);
#endif
				if (rHWLock.bSecureInst == VAL_FALSE) {
/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
					enable_irq(VDEC_IRQ_ID);
				}

#ifdef ENABLE_MMDVFS_VDEC
				VdecDvfsMonitorStart();
#endif
			} else { /* Another one holding dec hw now */
				pr_info("VCODEC_LOCKHW E\n");
				eVideoGetTimeOfDay(&rCurTime,
					sizeof(struct VAL_TIME_T));
				u4TimeInterval = (((((rCurTime.u4Sec -
					CodecHWLock.rLockedTime.u4Sec)
					* 1000000)
					+ rCurTime.u4uSec) -
					CodecHWLock.rLockedTime.u4uSec)
					/ 1000);

				pr_debug("VCODEC_LOCKHW, someone use dec HW, and check timeout value\n");
				pr_debug("TimeInterval(ms) = %d, TimeOutValue(ms)) = %d\n",
					 u4TimeInterval, rHWLock.u4TimeoutMs);
				pr_debug("Lock Instance = 0x%lx, Lock TID = %d, CurrentTID = %d\n",
					 (unsigned long)CodecHWLock.pvHandle,
					 gDrvInitParams->u4VdecLockThreadId,
					 current->pid);
				pr_debug("rLockedTime(%d s, %d us), rCurTime(%d s, %d us)\n",
					CodecHWLock.rLockedTime.u4Sec,
					CodecHWLock.rLockedTime.u4uSec,
					rCurTime.u4Sec, rCurTime.u4uSec);

				/* 2012/12/16. Cheng-Jung Never steal hardware lock */
			}
			mutex_unlock(&gDrvInitParams->hwLock);
			spin_lock_irqsave(&gDrvInitParams->lockDecHWCountLock,
				ulFlagsLockHW);
			gu4LockDecHWCount++;
			spin_unlock_irqrestore(&gDrvInitParams->lockDecHWCountLock,
				ulFlagsLockHW);
		}
	} else if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
		   rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC ||
		   rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC) {
		while (bLockedHW == VAL_FALSE) {
			/* Early break for JPEG VENC */
			if (rHWLock.u4TimeoutMs == 0) {
				if (CodecHWLock.pvHandle != 0) {
					/* Add one line comment for avoid kernel coding style,
					 * WARNING:BRACES:
					 */
					break;
				}
			}

			/* Wait to acquire Enc HW lock */
			mutex_lock(&gDrvInitParams->hwLockEventTimeoutLock);
			if (gDrvInitParams->HWLockEvent.u4TimeoutMs == 1) {
				pr_debug("VCODEC_LOCKHW, First Use Enc HW %d!!\n",
					rHWLock.eDriverType);
				FirstUseEncHW = 1;
			} else {
				FirstUseEncHW = 0;
			}
			mutex_unlock(&gDrvInitParams->hwLockEventTimeoutLock);
			if (FirstUseEncHW == 1) {
				/* Add one line comment for avoid kernel coding style,
				 * WARNING:BRACES:
				 */
				eValRet = eVideoWaitEvent(&gDrvInitParams->HWLockEvent,
						sizeof(struct VAL_EVENT_T));
			}

			mutex_lock(&gDrvInitParams->hwLockEventTimeoutLock);
			if (gDrvInitParams->HWLockEvent.u4TimeoutMs == 1) {
				gDrvInitParams->HWLockEvent.u4TimeoutMs = 1000;
				FirstUseEncHW = 1;
			} else {
				FirstUseEncHW = 0;
				if (rHWLock.u4TimeoutMs == 0) {
					/* Add one line comment for avoid kernel coding style,
					 * WARNING:BRACES:
					 */
					gDrvInitParams->HWLockEvent.u4TimeoutMs = 0; /* No wait */
				} else {
					/* Wait indefinitely */
					gDrvInitParams->HWLockEvent.u4TimeoutMs = 1000;
				}
			}
			mutex_unlock(&gDrvInitParams->hwLockEventTimeoutLock);

			mutex_lock(&gDrvInitParams->hwLock);
			handle_id = pmem_user_v2p_video((unsigned long)rHWLock.pvHandle);
			if (handle_id == 0) {
				pr_info("[error] handle is freed at %d\n", __LINE__);
				mutex_unlock(&gDrvInitParams->hwLock);
				return -1;
			}
			/* one process try to lock twice */
			if (CodecHWLock.pvHandle ==
			(void *)handle_id) {
				pr_info("[WARNING] VCODEC_LOCKHW, one encoder instance try to lock twice\n");
				pr_debug("may cause lock HW timeout!! instance=0x%lx, CurrentTID=%d, type:%d\n",
					(unsigned long)CodecHWLock.pvHandle,
					current->pid, rHWLock.eDriverType);
			}
			mutex_unlock(&gDrvInitParams->hwLock);

			if (FirstUseEncHW == 0) {
/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
				eValRet = eVideoWaitEvent(&gDrvInitParams->HWLockEvent,
						sizeof(struct VAL_EVENT_T));
			}

			if (eValRet == VAL_RESULT_INVALID_ISR) {
				ret = vcodec_lockhw_enc_fail(rHWLock,
					FirstUseEncHW);
				if (ret) {
					pr_info("[ERROR] vcodec_lockhw_enc_fail failed: %lu\n",
						ret);
					return -EFAULT;
				}
			} else if (eValRet == VAL_RESULT_RESTARTSYS) {
				return -ERESTARTSYS;
			}

			mutex_lock(&gDrvInitParams->hwLock);
			if (CodecHWLock.pvHandle == 0) {
				/* No process use HW, so current process can use HW */
				if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
					rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC ||
					rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC) {
					handle_id = pmem_user_v2p_video(
					(unsigned long)rHWLock.pvHandle);
					if (handle_id == 0) {
						pr_info("[error] handle is freed at %d\n",
						__LINE__);
						mutex_unlock(&gDrvInitParams->hwLock);
						return -1;
					}
					CodecHWLock.pvHandle =
					(void *)handle_id;
					CodecHWLock.eDriverType = rHWLock.eDriverType;
					eVideoGetTimeOfDay(
						&CodecHWLock.rLockedTime,
						sizeof(struct VAL_TIME_T));

					pr_debug("VCODEC_LOCKHW, No process use HW, so current process can use HW\n");
					pr_debug("VCODEC_LOCKHW, handle = 0x%lx\n",
					(unsigned long)CodecHWLock.pvHandle);
					pr_debug("LockInstance = 0x%lx CurrentTID = %d, rLockedTime(s, us) = %d, %d\n",
					(unsigned long)CodecHWLock.pvHandle,
					current->pid,
					CodecHWLock.rLockedTime.u4Sec,
					CodecHWLock.rLockedTime.u4uSec);

					bLockedHW = VAL_TRUE;
					if ((rHWLock.eDriverType ==
						VAL_DRIVER_TYPE_H264_ENC) ||
						(rHWLock.eDriverType ==
						VAL_DRIVER_TYPE_HEVC_ENC) ||
						(rHWLock.eDriverType ==
						VAL_DRIVER_TYPE_JPEG_ENC)) {
#ifndef KS_POWER_WORKAROUND
						venc_power_on(gVCodecDev);
#endif
						enable_irq(VENC_IRQ_ID);
					}
				}
#ifdef ENABLE_MMDVFS_VDEC
				VdecDvfsMonitorStart();
#endif
			} else { /* someone use HW, and check timeout value */
				if (rHWLock.u4TimeoutMs == 0) {
					bLockedHW = VAL_FALSE;
					mutex_unlock(&gDrvInitParams->hwLock);
					break;
				}

				eVideoGetTimeOfDay(&rCurTime,
					sizeof(struct VAL_TIME_T));
				u4TimeInterval = (((((rCurTime.u4Sec -
				CodecHWLock.rLockedTime.u4Sec) * 1000000)
					+ rCurTime.u4uSec) -
					CodecHWLock.rLockedTime.u4uSec)
					/ 1000);

				pr_debug("VCODEC_LOCKHW, someone use enc HW, and check timeout value\n");
				pr_debug("TimeInterval(ms) = %d, TimeOutValue(ms) = %d\n",
					 u4TimeInterval, rHWLock.u4TimeoutMs);
				pr_debug("rLockedTime(s, us) = %d, %d, rCurTime(s, us) = %d, %d\n",
					 CodecHWLock.rLockedTime.u4Sec,
					 CodecHWLock.rLockedTime.u4uSec,
					 rCurTime.u4Sec, rCurTime.u4uSec);
				pr_debug("LockInstance = 0x%lx, CurrentInstance = 0x%lx, CurrentTID = %d\n",
				(unsigned long)CodecHWLock.pvHandle,
				pmem_user_v2p_video(
					(unsigned long)rHWLock.pvHandle),
				current->pid);

				++gLockTimeOutCount;
				if (gLockTimeOutCount > 30) {
					pr_info("[ERROR] VCODEC_LOCKHW %d fail,someone locked HW over 30 times\n",
						 current->pid);
					pr_debug("without timeout 0x%lx,%lx,0x%lx,type:%d\n",
					(unsigned long)CodecHWLock.pvHandle,
					pmem_user_v2p_video(
					(unsigned long)rHWLock.pvHandle),
					(unsigned long)rHWLock.pvHandle,
					rHWLock.eDriverType);
					gLockTimeOutCount = 0;
					mutex_unlock(&gDrvInitParams->hwLock);
					return -EFAULT;
				}

				/* 2013/04/10. Cheng-Jung Never steal hardware lock */
			}

			if (bLockedHW == VAL_TRUE) {
				pr_debug("VCODEC_LOCKHW, Enc Lock ok CodecHWLock.pvHandle = 0x%lx, va:%lx, type:%d\n",
					 (unsigned long)CodecHWLock.pvHandle,
					 (unsigned long)rHWLock.pvHandle,
					 rHWLock.eDriverType);
				gLockTimeOutCount = 0;
			}
			mutex_unlock(&gDrvInitParams->hwLock);
		}

		if (bLockedHW == VAL_FALSE) {
			pr_info("[ERROR] VCODEC_LOCKHW %d fail,someone locked HW already,0x%lx,%lx,0x%lx,type:%d\n",
			current->pid,
			(unsigned long)CodecHWLock.pvHandle,
			pmem_user_v2p_video((unsigned long)rHWLock.pvHandle),
			(unsigned long)rHWLock.pvHandle,
			rHWLock.eDriverType);
			gLockTimeOutCount = 0;
			return -EFAULT;
		}

		spin_lock_irqsave(&gDrvInitParams->lockEncHWCountLock, ulFlagsLockHW);
		gu4LockEncHWCount++;
		spin_unlock_irqrestore(&gDrvInitParams->lockEncHWCountLock, ulFlagsLockHW);

		pr_debug("VCODEC_LOCKHW, get locked - ObjId =%d\n",
			current->pid);

		pr_debug("VCODEC_LOCKHWed - tid = %d\n", current->pid);
	} else {
		pr_info("[WARNING] VCODEC_LOCKHW Unknown instance\n");
		return -EFAULT;
	}

	/* MT6763 VCODEC_SEL setting */
	if (rHWLock.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_ADV_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_DEC) {
		u4VcodecSel = 0x2;
	} else if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC) {
		u4VcodecSel = 0x1;
		if (VDO_HW_READ(KVA_VDEC_GCON_BASE + 0x24) == 0) {
			do {
				VDO_HW_WRITE(KVA_VDEC_GCON_BASE + 0x24, u4DeBlcoking);
			} while (VDO_HW_READ(KVA_VDEC_GCON_BASE + 0x24) != u4DeBlcoking);
		}
	} else if (rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC) {
		u4VcodecSel = 0x4;
	} else {
		u4VcodecSel = 0x0;
		pr_info("[WARNING] Unknown driver type\n");
	}

	if (VDO_HW_READ(KVA_VDEC_GCON_BASE + 0x20) == 0) {
		do {
			VDO_HW_WRITE(KVA_VDEC_GCON_BASE + 0x20, u4VcodecSel);
		} while (VDO_HW_READ(KVA_VDEC_GCON_BASE + 0x20) != u4VcodecSel);
	} else {
		pr_info("[WARNING] VCODEC_SEL is not 0\n");
	}
	if (u4VcodecSel == 0x2)
		VDO_HW_WRITE(KVA_VDEC_GCON_BASE + 0x10, 0x1);
	else
		VDO_HW_WRITE(KVA_VDEC_GCON_BASE + 0x10, 0x0);

	pr_debug("VCODEC_LOCKHW - tid = %d\n", current->pid);

	return 0;
}

long vcodec_unlockhw(unsigned long arg)
{
	unsigned char *user_data_addr;
	struct VAL_HW_LOCK_T rHWLock;
	enum VAL_RESULT_T eValRet;
	long ret;
	unsigned long handle_id = 0;

	pr_debug("VCODEC_UNLOCKHW + tid = %d\n", current->pid);

	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&rHWLock, user_data_addr,
		sizeof(struct VAL_HW_LOCK_T));
	if (ret) {
		pr_info("[ERROR] VCODEC_UNLOCKHW, copy_from_user failed: %lu\n",
			ret);
		return -EFAULT;
	}

	pr_debug("VCODEC_UNLOCKHW eDriverType = %d\n", rHWLock.eDriverType);
	eValRet = VAL_RESULT_INVALID_ISR;
	if (rHWLock.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_ADV_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_DEC) {
		mutex_lock(&gDrvInitParams->hwLock);
		handle_id = pmem_user_v2p_video((unsigned long)rHWLock.pvHandle);
		if (handle_id == 0) {
			pr_info("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&gDrvInitParams->hwLock);
			return -1;
		}
		/* Current owner give up hw lock */
		if (CodecHWLock.pvHandle ==
		(void *)handle_id) {
			CodecHWLock.pvHandle = 0;
			CodecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
			if (rHWLock.bSecureInst == VAL_FALSE) {
				/* Add one line comment for avoid kernel coding style,
				 * WARNING:BRACES:
				 */
				disable_irq(VDEC_IRQ_ID);
			}

			/* MT6763 VCODEC_SEL reset */
			do {
				VDO_HW_WRITE(KVA_VDEC_GCON_BASE + 0x20, 0);
			} while (VDO_HW_READ(KVA_VDEC_GCON_BASE + 0x20) != 0);

			/* TODO: check if turning power off is ok */
#ifndef KS_POWER_WORKAROUND
			vdec_power_off(gVCodecDev);
#endif

#ifdef ENABLE_MMDVFS_VDEC
			VdecDvfsAdjustment();
#endif
		} else { /* Not current owner */
			pr_debug("[ERROR] VCODEC_UNLOCKHW\n");
			pr_info("Not owner trying to unlock dec hardware 0x%lx\n",
			pmem_user_v2p_video((unsigned long)rHWLock.pvHandle));
			mutex_unlock(&gDrvInitParams->hwLock);
			return -EFAULT;
		}
		mutex_unlock(&gDrvInitParams->hwLock);
		eValRet = eVideoSetEvent(&gDrvInitParams->HWLockEvent,
				sizeof(struct VAL_EVENT_T));
	} else if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
			 rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC ||
			 rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC) {
		mutex_lock(&gDrvInitParams->hwLock);
		handle_id = pmem_user_v2p_video((unsigned long)rHWLock.pvHandle);
		if (handle_id == 0) {
			pr_info("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&gDrvInitParams->hwLock);
			return -1;
		}
		/* Current owner give up hw lock */
		if (CodecHWLock.pvHandle ==
		(void *)handle_id) {
			CodecHWLock.pvHandle = 0;
			CodecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
			if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
				rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC ||
				rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC) {
				disable_irq(VENC_IRQ_ID);

				/* MT6763 VCODEC_SEL reset */
				do {
					VDO_HW_WRITE(KVA_VDEC_GCON_BASE + 0x20, 0);
				} while (VDO_HW_READ(KVA_VDEC_GCON_BASE + 0x20) != 0);

				/* turn venc power off */
#ifndef KS_POWER_WORKAROUND
				venc_power_off(gVCodecDev);
#endif

#ifdef ENABLE_MMDVFS_VDEC
			VdecDvfsAdjustment();
#endif
			}
		} else { /* Not current owner */
			/* [TODO] error handling */
			pr_debug("[ERROR] VCODEC_UNLOCKHW\n");
			pr_info("Not owner trying to unlock enc hardware 0x%lx, pa:%lx, va:%lx type:%d\n",
			(unsigned long)CodecHWLock.pvHandle,
			pmem_user_v2p_video((unsigned long)rHWLock.pvHandle),
			(unsigned long)rHWLock.pvHandle,
				 rHWLock.eDriverType);
			mutex_unlock(&gDrvInitParams->hwLock);
			return -EFAULT;
			}
		mutex_unlock(&gDrvInitParams->hwLock);
		eValRet = eVideoSetEvent(&gDrvInitParams->HWLockEvent,
				sizeof(struct VAL_EVENT_T));
	} else {
		pr_info("[WARNING] VCODEC_UNLOCKHW Unknown instance\n");
		return -EFAULT;
	}

	pr_debug("VCODEC_UNLOCKHW - tid = %d\n", current->pid);

	return 0;
}

long vcodec_waitisr(unsigned long arg)
{
	unsigned char *user_data_addr;
	struct VAL_ISR_T val_isr;
	char bLockedHW = VAL_FALSE;
	unsigned long ulFlags;
	long ret;
	enum VAL_RESULT_T eValRet;
	unsigned long handle_id = 0;

	pr_debug("VCODEC_WAITISR + tid = %d\n", current->pid);

	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&val_isr, user_data_addr,
		sizeof(struct VAL_ISR_T));
	if (ret) {
		pr_info("[ERROR] VCODEC_WAITISR, copy_from_user failed: %lu\n",
			ret);
		return -EFAULT;
	}

	if (val_isr.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_VC1_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_VC1_ADV_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_VP8_DEC) {
		mutex_lock(&gDrvInitParams->hwLock);
		handle_id = pmem_user_v2p_video((unsigned long)val_isr.pvHandle);
		if (handle_id == 0) {
			pr_info("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&gDrvInitParams->hwLock);
			return -1;
		}
		if (CodecHWLock.pvHandle ==
		(void *)handle_id) {
/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
			bLockedHW = VAL_TRUE;
		} else {
		}
		mutex_unlock(&gDrvInitParams->hwLock);

		if (bLockedHW == VAL_FALSE) {
			pr_info("[ERROR] VCODEC_WAITISR, DO NOT have HWLock, so return fail\n");
			return -EFAULT;
		}

		spin_lock_irqsave(&gDrvInitParams->decIsrLock, ulFlags);
		gDrvInitParams->DecIsrEvent.u4TimeoutMs = val_isr.u4TimeoutMs;
		spin_unlock_irqrestore(&gDrvInitParams->decIsrLock, ulFlags);

		eValRet = eVideoWaitEvent(&gDrvInitParams->DecIsrEvent,
				sizeof(struct VAL_EVENT_T));

		if (eValRet == VAL_RESULT_INVALID_ISR) {
			return -2;
		} else if (eValRet == VAL_RESULT_RESTARTSYS) {
			pr_info("[WARNING] VCODEC_WAITISR, VAL_RESULT_RESTARTSYS return when WAITISR!!\n");
			return -ERESTARTSYS;
		}
	} else if (val_isr.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
		   val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC) {
		mutex_lock(&gDrvInitParams->hwLock);
		handle_id = pmem_user_v2p_video((unsigned long)val_isr.pvHandle);
		if (handle_id == 0) {
			pr_info("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&gDrvInitParams->hwLock);
			return -1;
		}
		if (CodecHWLock.pvHandle ==
		(void *)handle_id) {
/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
			bLockedHW = VAL_TRUE;
		} else {
		}
		mutex_unlock(&gDrvInitParams->hwLock);

		if (bLockedHW == VAL_FALSE) {
			pr_info("[ERROR] VCODEC_WAITISR, DO NOT have enc HWLock, so return fail pa:%lx, va:%lx\n",
			pmem_user_v2p_video((unsigned long)val_isr.pvHandle),
				(unsigned long)val_isr.pvHandle);
			return -EFAULT;
		}

		spin_lock_irqsave(&gDrvInitParams->encIsrLock, ulFlags);
		gDrvInitParams->EncIsrEvent.u4TimeoutMs = val_isr.u4TimeoutMs;
		spin_unlock_irqrestore(&gDrvInitParams->encIsrLock, ulFlags);

		eValRet = eVideoWaitEvent(&gDrvInitParams->EncIsrEvent,
				sizeof(struct VAL_EVENT_T));
		if (eValRet == VAL_RESULT_INVALID_ISR) {
			return -2;
		} else if (eValRet == VAL_RESULT_RESTARTSYS) {
			pr_info("[WARNING] VCODEC_WAITISR, VAL_RESULT_RESTARTSYS return when WAITISR!!\n");
			return -ERESTARTSYS;
		}

		if (val_isr.u4IrqStatusNum > 0) {
			val_isr.u4IrqStatus[0] = gu4HwVencIrqStatus;
			ret = copy_to_user(user_data_addr, &val_isr,
				sizeof(struct VAL_ISR_T));
			if (ret) {
				pr_info("[ERROR] VCODEC_WAITISR, copy_to_user failed: %lu\n",
					ret);
				return -EFAULT;
			}
		}
	} else {
		pr_info("[WARNING] VCODEC_WAITISR Unknown instance\n");
		return -EFAULT;
	}

	pr_debug("VCODEC_WAITISR - tid = %d\n", current->pid);

	return 0;
}

long vcodec_plat_unlocked_ioctl(unsigned int cmd, unsigned long arg)
{
	long ret;
	unsigned char *user_data_addr;
	struct VAL_VCODEC_CORE_LOADING_T rTempCoreLoading;
	struct VAL_VCODEC_CPU_OPP_LIMIT_T rCpuOppLimit;

	switch (cmd) {
	case VCODEC_INC_DEC_EMI_USER:
	{
		pr_debug("VCODEC_INC_DEC_EMI_USER + tid = %d\n", current->pid);

		mutex_lock(&gDrvInitParams->decEMILock);
		gDrvInitParams->u4DecEMICounter++;
		pr_debug("[VCODEC] DEC_EMI_USER = %d\n", gDrvInitParams->u4DecEMICounter);
		user_data_addr = (unsigned char *)arg;
		ret = copy_to_user(user_data_addr, &gDrvInitParams->u4DecEMICounter,
			sizeof(unsigned int));
		if (ret) {
			pr_info("[ERROR] VCODEC_INC_DEC_EMI_USER, copy_to_user failed: %lu\n",
				ret);
			mutex_unlock(&gDrvInitParams->decEMILock);
			return -EFAULT;
		}
		mutex_unlock(&gDrvInitParams->decEMILock);

#ifdef ENABLE_MMDVFS_VDEC
/* MM DVFS related */
/* pr_debug("[VCODEC][MMDVFS_VDEC] INC_DEC_EMI MM DVFS init\n"); */
/* raise voltage */
		SendDvfsRequest(DVFS_DEFAULT);
		VdecDvfsBegin();
#endif

		pr_debug("VCODEC_INC_DEC_EMI_USER - tid = %d\n", current->pid);
	}
	break;

	case VCODEC_DEC_DEC_EMI_USER:
	{
		pr_debug("VCODEC_DEC_DEC_EMI_USER + tid = %d\n", current->pid);

		mutex_lock(&gDrvInitParams->decEMILock);
		gDrvInitParams->u4DecEMICounter--;
		pr_debug("[VCODEC] DEC_EMI_USER = %d\n", gDrvInitParams->u4DecEMICounter);
		user_data_addr = (unsigned char *)arg;
		ret = copy_to_user(user_data_addr, &gDrvInitParams->u4DecEMICounter,
			sizeof(unsigned int));
		if (ret) {
			pr_info("[ERROR] VCODEC_DEC_DEC_EMI_USER, copy_to_user failed: %lu\n",
				ret);
			mutex_unlock(&gDrvInitParams->decEMILock);
			return -EFAULT;
		}
#ifdef ENABLE_MMDVFS_VDEC
		/* MM DVFS related */
		/* pr_debug("[VCODEC][MMDVFS_VDEC] DEC_DEC_EMI MM DVFS\n"); */
		/* unrequest voltage */
		if (gDrvInitParams->u4DecEMICounter == 0) {
		/* Unrequest when all decoders exit */
			SendDvfsRequest(DVFS_UNREQUEST);
		}
#endif
		mutex_unlock(&gDrvInitParams->decEMILock);

		pr_debug("VCODEC_DEC_DEC_EMI_USER - tid = %d\n", current->pid);
	}
	break;

	case VCODEC_GET_CPU_LOADING_INFO:
	{
		unsigned char *user_data_addr;
		struct VAL_VCODEC_CPU_LOADING_INFO_T _temp = {0};

		pr_debug("VCODEC_GET_CPU_LOADING_INFO +\n");
		user_data_addr = (unsigned char *)arg;
		/* TODO: */
		ret = copy_to_user(user_data_addr, &_temp,
			sizeof(struct VAL_VCODEC_CPU_LOADING_INFO_T));
		if (ret) {
			pr_info("[ERROR] VCODEC_GET_CPU_LOADING_INFO, copy_to_user failed: %lu\n",
				ret);
			return -EFAULT;
		}

		pr_debug("VCODEC_GET_CPU_LOADING_INFO -\n");
	}
	break;

	case VCODEC_GET_CORE_LOADING:
	{
		pr_debug("VCODEC_GET_CORE_LOADING + - tid = %d\n",
			current->pid);

		user_data_addr = (unsigned char *)arg;
		ret = copy_from_user(&rTempCoreLoading, user_data_addr,
			sizeof(struct VAL_VCODEC_CORE_LOADING_T));
		if (ret) {
			pr_info("[ERROR] VCODEC_GET_CORE_LOADING, copy_from_user failed: %lu\n",
				ret);
			return -EFAULT;
		}

		if (rTempCoreLoading.CPUid < 0) {
			pr_info("[ERROR] rTempCoreLoading.CPUid < 0\n");
			return -EFAULT;
		}

		if (rTempCoreLoading.CPUid > num_possible_cpus()) {
			pr_info("[ERROR] rTempCoreLoading.CPUid(%d) > num_possible_cpus(%u)\n",
			rTempCoreLoading.CPUid, num_possible_cpus());
			return -EFAULT;
		}
	/*get_cpu_load(rTempCoreLoading.CPUid);*/
		rTempCoreLoading.Loading = 0;
		ret = copy_to_user(user_data_addr, &rTempCoreLoading,
				sizeof(struct VAL_VCODEC_CORE_LOADING_T));
		if (ret) {
			pr_info("[ERROR] VCODEC_GET_CORE_LOADING, copy_to_user failed: %lu\n",
				ret);
			return -EFAULT;
		}
		pr_debug("VCODEC_GET_CORE_LOADING - - tid = %d\n",
			current->pid);
	}
	break;

	case VCODEC_SET_CPU_OPP_LIMIT:
	{
		pr_debug("VCODEC_SET_CPU_OPP_LIMIT [EMPTY] + - tid = %d\n",
			current->pid);
		user_data_addr = (unsigned char *)arg;
		ret = copy_from_user(&rCpuOppLimit, user_data_addr,
			sizeof(struct VAL_VCODEC_CPU_OPP_LIMIT_T));
		if (ret) {
			pr_info("[ERROR] VCODEC_SET_CPU_OPP_LIMIT, copy_from_user failed: %lu\n",
				ret);
			return -EFAULT;
		}
		pr_debug("+VCODEC_SET_CPU_OPP_LIMIT (%d, %d, %d), tid = %d\n",
			rCpuOppLimit.limited_freq, rCpuOppLimit.limited_cpu,
			rCpuOppLimit.enable, current->pid);
		/* TODO: Check if cpu_opp_limit is available */
		/*
		 * ret = cpu_opp_limit(EVENT_VIDEO, rCpuOppLimit.limited_freq,
		 * rCpuOppLimit.limited_cpu, rCpuOppLimit.enable); // 0: PASS, other: FAIL
		 * if (ret) {
		 * pr_info("[VCODEC][ERROR] cpu_opp_limit failed: %lu\n", ret);
		 *	return -EFAULT;
		 * }
		 */
		pr_debug("-VCODEC_SET_CPU_OPP_LIMIT tid = %d, ret = %lu\n",
			current->pid, ret);
		pr_debug("VCODEC_SET_CPU_OPP_LIMIT [EMPTY] - - tid = %d\n",
			current->pid);
	}
	break;

	default:
	{
		pr_info("========[ERROR] vcodec_ioctl default case======== %u\n",
			cmd);
	}
	break;

	}
	return 0;
}

void vcodec_plat_release(void)
{
#ifdef ENABLE_MMDVFS_VDEC
	if (gMMDFVFSMonitorStarts == VAL_TRUE) {
		gMMDFVFSMonitorStarts = VAL_FALSE;
		gMMDFVFSMonitorCounts = 0;
		gHWLockInterval = 0;
		gHWLockMaxDuration = 0;
		SendDvfsRequest(DVFS_UNREQUEST);
	}
#endif
}

int vcodec_plat_probe(struct platform_device *pdev, struct mtk_vcodec_dev *dev)
{
	dev->dev = &pdev->dev;
	gVCodecDev = dev;
#if IS_ENABLED(CONFIG_PM)
	dev_set_drvdata(gDrvInitParams->vcodec_device, dev);
#endif
	return 0;
}

int vcodec_suspend(struct platform_device *pDev, pm_message_t state)
{
	return 0;
}

int vcodec_resume(struct platform_device *pDev)
{
	return 0;
}

void vcodec_driver_plat_init(void)
{
}

void vcodec_driver_plat_exit(void)
{
}
