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
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/pm_wakeup.h>

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
#include "videocodec_kernel_drv_plat.h"

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
#include "mtk_vcodec_pm_plat.h"
#include <linux/slab.h>
#include "dvfs_v2.h"

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/uaccess.h>
#include <linux/compat.h>
#endif

unsigned int is_entering_suspend;
int gBistFlag;

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

#define VCODEC_DVFS_V2 1
#ifdef VCODEC_DVFS_V2
struct codec_history *codec_hists;
struct codec_job *codec_jobs;
DEFINE_MUTEX(VcodecDVFSLock);
#endif

DEFINE_MUTEX(DecPMQoSLock);
DEFINE_MUTEX(EncPMQoSLock);

#define DRAM_DONE_POLLING_LIMIT 20000

/* disable temporary for alaska DVT verification, smi seems not ready */
#define ENABLE_MMDVFS_VDEC
#ifdef ENABLE_MMDVFS_VDEC
#define MIN_VDEC_FREQ 228
#define MIN_VENC_FREQ 228
/* <--- MM DVFS related */
/*#include <mmdvfs_config_util.h>*/
#define STD_VDEC_FREQ 320
#define STD_VENC_FREQ 320
static int gVDECBWRequested;
static int gVENCBWRequested;
#define DEFAULT_MHZ 99999
static unsigned int gVDECFrmTRAVC[4] = {12, 24, 40, 12}; /* /3 for real ratio */
static unsigned int gVDECFrmTRHEVC[4] = {12, 24, 40, 12};
/* 3rd element for VP mode */
static unsigned int gVDECFrmTRMP2_4[5] = {16, 20, 32, 50, 16};
static unsigned int gVENCFrmTRAVC[3] = {6, 12, 6};
#endif

extern void vcodec_dma_alloc_count_reset(void);

/* TODO: Check register for shared vdec/venc */
void vdec_polling_status(void)
{
	unsigned int u4DataStatusMain = 0;
	unsigned int u4DataStatus = 0;
	unsigned int u4CgStatus = 0;
	unsigned int u4Counter = 0;
	void *u4Reg = 0;

	u4CgStatus = VDO_HW_READ(KVA_VDEC_GCON_BASE);
	u4DataStatusMain = VDO_HW_READ(KVA_VDEC_VLD_BASE+(61*4));

	while ((u4CgStatus != 0) && (u4DataStatusMain & (1<<15)) &&
		((u4DataStatusMain & 1) != 1)) {
		u4CgStatus = VDO_HW_READ(KVA_VDEC_GCON_BASE);
		u4DataStatusMain = VDO_HW_READ(KVA_VDEC_VLD_BASE+(61*4));
		if (u4Counter++ > DRAM_DONE_POLLING_LIMIT) {
			unsigned int u4IntStatus = 0;
			unsigned int i = 0;

			pr_notice("[ERROR] Leftover data before power down");
			for (i = 45; i < 72; i++) {
				if (i == 45 || i == 46 || i == 52 || i == 58 ||
					i == 59 || i == 61 || i == 62 ||
					i == 63 || i == 71){
					u4Reg = KVA_VDEC_VLD_BASE+(i*4);
					u4IntStatus = VDO_HW_READ(u4Reg);
					pr_notice("[VCODEC] VLD_%d = %x\n",
						i, u4IntStatus);
				}
			}

			for (i = 66; i < 80; i++) {
				u4Reg = KVA_VDEC_MISC_BASE+(i*4);
				u4DataStatus = VDO_HW_READ(u4Reg);
				pr_info("[VCODEC] MISC_%d = %x\n",
						i, u4DataStatus);
			}
			u4Counter = 0;
			WARN_ON(1);
		}
	}
	/* pr_info("u4Counter %d\n", u4Counter); */

}

void vdec_power_on(struct mtk_vcodec_dev *dev)
{
	int ret = 0;

	pr_debug("%s +\n", __func__);

	mutex_lock(&gDrvInitParams->vdecPWRLock);
	gDrvInitParams->u4VdecPWRCounter++;
	mutex_unlock(&gDrvInitParams->vdecPWRLock);
	ret = 0;
	mtk_vcodec_clock_on(MTK_INST_DECODER, dev);

	if (gBistFlag) {
		/* GF14 type SRAM  power on config */
		VDO_HW_WRITE(KVA_MBIST_BASE, 0x93CEB);
	}
	pr_debug("%s -\n", __func__);
}

void vdec_power_off(struct mtk_vcodec_dev *dev)
{
	pr_debug("%s +\n", __func__);
	mutex_lock(&gDrvInitParams->vdecPWRLock);
	if (gDrvInitParams->u4VdecPWRCounter == 0) {
		pr_debug("[VCODEC] gDrvInitParams->u4VdecPWRCounter = 0\n");
	} else {
		vdec_polling_status();
		/* VCODEC_SEL reset */
		do {
			VDO_HW_WRITE(KVA_VDEC_GCON_BASE + 0x20, 0);
		} while (VDO_HW_READ(KVA_VDEC_GCON_BASE + 0x20) != 0);

		gDrvInitParams->u4VdecPWRCounter--;
		mtk_vcodec_clock_off(MTK_INST_DECODER, dev);

	}
	mutex_unlock(&gDrvInitParams->vdecPWRLock);
	mutex_lock(&DecPMQoSLock);

	/* pr_debug("[PMQoS] vdec_power_off reset to 0"); */
	set_vdec_bw(gVCodecDev, 0);
	gVDECBWRequested = 0;
	set_vdec_opp(gVCodecDev, 0);
	mutex_unlock(&DecPMQoSLock);
	pr_debug("%s -\n", __func__);
}

void venc_power_on(struct mtk_vcodec_dev *dev)
{
	int ret = 0;

	mutex_lock(&gDrvInitParams->vencPWRLock);
	gDrvInitParams->u4VencPWRCounter++;
	mutex_unlock(&gDrvInitParams->vencPWRLock);
	ret = 0;
	mtk_vcodec_clock_on(MTK_INST_ENCODER, dev);

	if (gBistFlag) {
		/* GF14 type SRAM  power on config */
		VDO_HW_WRITE(KVA_MBIST_BASE, 0x93CEB);
	}
}

void venc_power_off(struct mtk_vcodec_dev *dev)
{
	mutex_lock(&gDrvInitParams->vencPWRLock);
	do {
		VDO_HW_WRITE(KVA_VDEC_GCON_BASE + 0x20, 0);
	} while (VDO_HW_READ(KVA_VDEC_GCON_BASE + 0x20) != 0);

	if (gDrvInitParams->u4VencPWRCounter == 0) {
		pr_debug("[VENC] gDrvInitParams->u4VencPWRCounter = 0\n");
	} else {
		gDrvInitParams->u4VencPWRCounter--;
		mtk_vcodec_clock_off(MTK_INST_ENCODER, dev);
	}
	mutex_unlock(&gDrvInitParams->vencPWRLock);
	mutex_lock(&EncPMQoSLock);
	/* pr_debug("[PMQoS] venc_power_off reset to 0"); */
	set_venc_bw(gVCodecDev, 0);
	gVENCBWRequested = 0;
	set_venc_opp(gVCodecDev, 0);
	mutex_unlock(&EncPMQoSLock);
}

void vdec_break(void)
{
	unsigned int i;

	/* Step 1: set vdec_break */
	VDO_HW_WRITE(KVA_VDEC_MISC_BASE + 64*4, 0x1);

	/* Step 2: monitor status vdec_break_ok */
	for (i = 0; i < 5000; i++) {
		if ((VDO_HW_READ(KVA_VDEC_MISC_BASE + 65*4) & 0x11) == 0x11) {
			/* Add one line comment for avoid kernel coding style,
			 * WARNING:BRACES:
			 */
			break;
		}
	}

	if (i >= 5000) {
		unsigned int j;
		unsigned int u4DataStatus = 0;

		pr_info("Leftover data access before powering down\n");

		for (j = 68; j < 80; j++) {
			u4DataStatus = VDO_HW_READ(KVA_VDEC_MISC_BASE+(j*4));
			pr_info("[VCODEC][DUMP] MISC_%d = 0x%08x",
				j, u4DataStatus);
		}
		u4DataStatus = VDO_HW_READ(KVA_VDEC_BASE+(45*4));
		pr_info("[VCODEC][DUMP] VLD_45 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_BASE+(46*4));
		pr_info("[VCODEC][DUMP] VLD_46 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_BASE+(52*4));
		pr_info("[VCODEC][DUMP] VLD_52 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_BASE+(58*4));
		pr_info("[VCODEC][DUMP] VLD_58 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_BASE+(59*4));
		pr_info("[VCODEC][DUMP] VLD_59 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_BASE+(61*4));
		pr_info("[VCODEC][DUMP] VLD_61 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_BASE+(62*4));
		pr_info("[VCODEC][DUMP] VLD_62 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_BASE+(63*4));
		pr_info("[VCODEC][DUMP] VLD_63 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_BASE+(71*4));
		pr_info("[VCODEC][DUMP] VLD_71 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_MISC_BASE+(66*4));
		pr_info("[VCODEC][DUMP] MISC_66 = 0x%08x", u4DataStatus);
	}

	/* Step 3: software reset */
	VDO_HW_WRITE(KVA_VDEC_BASE + 66*4, 0x1);
	VDO_HW_WRITE(KVA_VDEC_BASE + 66*4, 0x0);

	/* Step 4: VCODEC reset control, include VDEC/VENC/JPGENC */
	VDO_HW_WRITE(KVA_VDEC_GCON_BASE + 5*4, 0x1);
	VDO_HW_WRITE(KVA_VDEC_GCON_BASE + 3*4, 0x1);

}

void venc_break(void)
{
	unsigned int i;
	void *VENC_SW_PAUSE   = KVA_VENC_BASE + 0xAC;
	void *VENC_IRQ_STATUS = KVA_VENC_BASE + 0x5C;
	void *VENC_SW_HRST_N  = KVA_VENC_BASE + 0xA8;
	void *VENC_IRQ_ACK    = KVA_VENC_BASE + 0x60;

	/* Step 1: raise pause hardware signal */
	VDO_HW_WRITE(VENC_SW_PAUSE, 0x1);

	/* Step 2: assume software can only tolerate 5000 APB read time. */
	for (i = 0; i < 5000; i++) {
		if (VDO_HW_READ(VENC_IRQ_STATUS) & 0x10) {
			/* Add one line comment for avoid kernel coding style,
			 * WARNING:BRACES:
			 */
			break;
		}
	}

	/* Step 3: Lower pause hardware signal and lower software hard reset
	 * signal
	 */
	if (i >= 5000) {
		VDO_HW_WRITE(VENC_SW_PAUSE, 0x0);
		VDO_HW_WRITE(VENC_SW_HRST_N, 0x0);
		VDO_HW_READ(VENC_SW_HRST_N);
	}

	/* Step 4: Lower software hard reset signal and lower pause hardware
	 * signal
	 */
	else {
		VDO_HW_WRITE(VENC_SW_HRST_N, 0x0);
		VDO_HW_READ(VENC_SW_HRST_N);
		VDO_HW_WRITE(VENC_SW_PAUSE, 0x0);
	}

	/* Step 5: Raise software hard reset signal */
	VDO_HW_WRITE(VENC_SW_HRST_N, 0x1);
	VDO_HW_READ(VENC_SW_HRST_N);
	/* Step 6: Clear pause status */
	VDO_HW_WRITE(VENC_IRQ_ACK, 0x10);
}

#if IS_ENABLED(CONFIG_PM)
int mt_vdec_runtime_suspend(struct device *pDev)
{
	struct mtk_vcodec_dev *dev = dev_get_drvdata(pDev);

	vdec_power_off(dev);
	return 0;
}

int mt_vdec_runtime_resume(struct device *pDev)
{
	struct mtk_vcodec_dev *dev = dev_get_drvdata(pDev);

	vdec_power_on(dev);
	return 0;
}

int mt_venc_runtime_suspend(struct device *pDev)
{
	struct mtk_vcodec_dev *dev = dev_get_drvdata(pDev);

	venc_power_off(dev);
	return 0;
}

int mt_venc_runtime_resume(struct device *pDev)
{
	struct mtk_vcodec_dev *dev = dev_get_drvdata(pDev);

	venc_power_on(dev);
	return 0;
}
#endif



static long vcodec_lockhw_dec_fail(struct VAL_HW_LOCK_T rHWLock,
				unsigned int FirstUseDecHW)
{
	pr_info("VCODEC_LOCKHW, HWLockEvent TimeOut, CurrentTID = %d\n",
			current->pid);
	if (FirstUseDecHW != 1) {
		mutex_lock(&gDrvInitParams->hwLock);
		if (CodecHWLock.pvHandle == 0) {
			/* Add one line comment for avoid kernel coding style,
			 * WARNING:BRACES:
			 */
			pr_info("VCODEC_LOCKHW, mediaserver may restarted");
		} else {
			/* Add one line comment for avoid kernel coding style,
			 * WARNING:BRACES:
			 */
			pr_info("VCODEC_LOCKHW, someone use HW");
		}
		mutex_unlock(&gDrvInitParams->hwLock);
	}

	return 0;
}

#define ENC_LOCK_FLOG "venc locked HW timeout 0x\%lx,\%lx,0x\%lx, type:\%d"
#define ENC_LOCK_FLOG2 "someone locked HW already 0x\%lx,\%lx,0x\%lx, type:\%d"

static long vcodec_lockhw_enc_fail(struct VAL_HW_LOCK_T rHWLock,
				unsigned int FirstUseEncHW)
{
	pr_info("VENC_LOCKHW HWLockEvent TimeOut, CurrentTID = %d\n",
			current->pid);

	if (FirstUseEncHW != 1) {
		mutex_lock(&gDrvInitParams->hwLock);
		if (CodecHWLock.pvHandle == 0) {
			pr_info("VENC_LOCKHW, mediaserver may restarted");
		} else {
			pr_info("VENC_LOCKHW, someone use HW (%d)\n",
				 gLockTimeOutCount);
			++gLockTimeOutCount;

			if (gLockTimeOutCount > 30) {
				pr_info("VENC_LOCKHW - ID %d fail\n",
						current->pid);
				pr_info(ENC_LOCK_FLOG,
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
				pr_info("VENC_LOCKHW - ID %d fail\n",
						current->pid);
				pr_info(ENC_LOCK_FLOG2,
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

static long vcodec_lockhw_vdec(struct VAL_HW_LOCK_T *pHWLock, char *bLockedHW)
{
	unsigned int FirstUseDecHW = 0;
	unsigned long ulFlagsLockHW;
	unsigned long handle = 0, handle_id = 0;
	long ret = 0;
	struct VAL_TIME_T rCurTime;
	unsigned int u4TimeInterval;
	enum VAL_RESULT_T eValRet = VAL_RESULT_NO_ERROR;
	unsigned int suspend_block_cnt = 0;
	struct codec_job *cur_job = 0;
	int target_freq;
	u64 target_freq_64;

	mutex_lock(&VcodecDVFSLock);
	handle = (unsigned long)(pHWLock->pvHandle);
	handle_id = pmem_user_v2p_video(handle);
	if (handle_id == 0) {
		pr_info("[error] handle is freed at %d\n", __LINE__);
		mutex_unlock(&VcodecDVFSLock);
		return -1;
	}

	cur_job = add_job((void *)handle_id, &codec_jobs);
	mutex_unlock(&VcodecDVFSLock);

	/* Loop until lock hw successful */
	while ((*bLockedHW) == VAL_FALSE) {
		mutex_lock(&gDrvInitParams->hwLockEventTimeoutLock);
		if (gDrvInitParams->HWLockEvent.u4TimeoutMs == 1) {
			pr_info("VDEC_LOCKHW, First Use Dec HW!!\n");
			FirstUseDecHW = 1;
		} else {
			FirstUseDecHW = 0;
		}
		mutex_unlock(&gDrvInitParams->hwLockEventTimeoutLock);

		if (FirstUseDecHW == 1) {
			/* Add one line comment for avoid kernel coding style,
			 * WARNING:BRACES:
			 */
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
		/* one process try to lock twice */
		handle = (unsigned long)(pHWLock->pvHandle);
		handle_id = pmem_user_v2p_video(handle);
		if (handle_id == 0) {
			pr_info("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&gDrvInitParams->hwLock);
			return -1;
		}

		if (CodecHWLock.pvHandle ==
			(void *)handle_id) {
			pr_info("[VDEC] Same inst double lock\n");
			pr_info("may timeout!! inst = 0x%lx, TID = %d\n",
				(unsigned long)CodecHWLock.pvHandle,
				current->pid);
		}
		mutex_unlock(&gDrvInitParams->hwLock);

		if (FirstUseDecHW == 0) {
			/*
			 * pr_debug("[VDEC] Not first time use, timeout = %d\n",
			 * gDrvInitParams->HWLockEvent.u4TimeoutMs);
			 */
			eValRet = eVideoWaitEvent(&gDrvInitParams->HWLockEvent,
						sizeof(struct VAL_EVENT_T));
		}

		if (eValRet == VAL_RESULT_INVALID_ISR) {
			ret = vcodec_lockhw_dec_fail(*pHWLock, FirstUseDecHW);
			if (ret) {
				pr_info("[VDEC] lockhw_dec_fail: %lu\n",
						ret);
				return -EFAULT;
			}
		} else if (eValRet == VAL_RESULT_RESTARTSYS) {
			pr_info("[VDEC] ERESTARTSYS when LockHW\n");
			return -ERESTARTSYS;
		}

		mutex_lock(&gDrvInitParams->hwLock);
		if (CodecHWLock.pvHandle == 0) {
			/* No one holds dec hw lock now */
			while (is_entering_suspend == 1) {
				suspend_block_cnt++;
				if (suspend_block_cnt > 100000) {
					/* Print log if trying to enter suspend
					 *  for too long
					 */
					pr_info("VDEC blocked by suspend");
					suspend_block_cnt = 0;
				}
				usleep_range(1000, 2000);
			}
			gDrvInitParams->u4VdecLockThreadId = current->pid;
			handle_id = pmem_user_v2p_video(handle);
			if (handle_id == 0) {
				pr_info("[error] handle is freed at %d\n", __LINE__);
				mutex_unlock(&gDrvInitParams->hwLock);
				return -1;
			}

			CodecHWLock.pvHandle =
				(void *)handle_id;
			CodecHWLock.eDriverType = pHWLock->eDriverType;
			eVideoGetTimeOfDay(&CodecHWLock.rLockedTime,
					sizeof(struct VAL_TIME_T));
			*bLockedHW = VAL_TRUE;

			mutex_lock(&VcodecDVFSLock);
			if (cur_job == 0) {
				target_freq = 1;
				target_freq_64 = match_freq(DEFAULT_MHZ,
					&gVCodecDev->vdec_freqs[0],
					gVCodecDev->vdec_freq_cnt);
				set_vdec_opp(gVCodecDev, target_freq_64);
			} else {
				cur_job->start = get_time_us();
				target_freq = est_freq(cur_job->handle,
					&codec_jobs, codec_hists);
				target_freq_64 = match_freq(target_freq,
					&gVCodecDev->vdec_freqs[0],
					gVCodecDev->vdec_freq_cnt);
				if (target_freq > 0) {
					gVCodecDev->dec_freq = target_freq;
					if (gVCodecDev->dec_freq > target_freq_64)
						gVCodecDev->dec_freq = (int)target_freq_64;
					cur_job->mhz = (int)target_freq_64;
					set_vdec_opp(gVCodecDev, target_freq_64);
				}
			}
			/* pr_info("VDEC cur_job freq %u, %llu, %d",
			 * target_freq, target_freq_64, dec_step_size);
			 */
			mutex_unlock(&VcodecDVFSLock);
#if IS_ENABLED(CONFIG_PM)
			pm_runtime_get_sync(gDrvInitParams->vcodec_device);
#else
#ifndef KS_POWER_WORKAROUND
			vdec_power_on(gVCodecDev);
#endif
#endif
			if (pHWLock->bSecureInst == VAL_FALSE) {
				/* Add one line comment for avoid kernel coding
				 * style, WARNING:BRACES:
				 */
				enable_irq(VDEC_IRQ_ID);
			}
		} else { /* Another one holding dec hw now */
			pr_info("VDEC_LOCKHW E\n");
			eVideoGetTimeOfDay(&rCurTime,
						sizeof(struct VAL_TIME_T));
			u4TimeInterval = (((((rCurTime.u4Sec -
					CodecHWLock.rLockedTime.u4Sec) *
					1000000) + rCurTime.u4uSec) -
					CodecHWLock.rLockedTime.u4uSec) /
					1000);
			/*
			 * pr_debug("VDEC_LOCKHW, someone use HW\n");
			 * pr_debug("Time(ms) = %d, TimeOut(ms)) = %d\n",
			 *		u4TimeInterval, pHWLock->u4TimeoutMs);
			 * pr_debug("Lock Inst=0x%lx, TID=%d, CurrTID=%d\n",
			 *	(unsigned long)CodecHWLock.pvHandle,
			 *	gDrvInitParams->u4VdecLockThreadId,
			 *	current->pid);
			 * pr_debug("rLockedTime(%ds,%dus), rCurTime(%ds,%dus)",
			 *		CodecHWLock.rLockedTime.u4Sec,
			 *		CodecHWLock.rLockedTime.u4uSec,
			 *		rCurTime.u4Sec, rCurTime.u4uSec);
			 */

			/* 2012/12/16. Cheng-Jung Never steal hardware lock */
		}
		mutex_unlock(&gDrvInitParams->hwLock);
		spin_lock_irqsave(&gDrvInitParams->lockDecHWCountLock, ulFlagsLockHW);
		gu4LockDecHWCount++;
		spin_unlock_irqrestore(&gDrvInitParams->lockDecHWCountLock, ulFlagsLockHW);
	}
	return 0;
}

static void vcodec_set_venc_opp(struct codec_job *cur_job, int *target_freq, u64 *target_freq_64)
{
	if (cur_job == 0) {
		*target_freq = 1;
		*target_freq_64 = match_freq(DEFAULT_MHZ,
			&gVCodecDev->venc_freqs[0],
			gVCodecDev->venc_freq_cnt);
		set_venc_opp(gVCodecDev, *target_freq_64);
	} else {
		cur_job->start = get_time_us();
		*target_freq = est_freq(cur_job->handle,
			&codec_jobs, codec_hists);
		*target_freq_64 = match_freq(*target_freq,
			&gVCodecDev->venc_freqs[0],
			gVCodecDev->venc_freq_cnt);
		if (target_freq > 0) {
			gVCodecDev->enc_freq = *target_freq;
			if (gVCodecDev->enc_freq > *target_freq_64)
				gVCodecDev->enc_freq = (int)(*target_freq_64);
			cur_job->mhz = (int)(*target_freq_64);
			set_venc_opp(gVCodecDev, *target_freq_64);
		}
	}
}

static long vcodec_lockhw_venc(struct VAL_HW_LOCK_T *pHWLock, char *bLockedHW)
{
	unsigned int FirstUseEncHW = 0;
	unsigned long handle = 0, handle_id = 0;
	long ret = 0;
	struct VAL_TIME_T rCurTime;
	unsigned int u4TimeInterval;
	enum VAL_RESULT_T eValRet = VAL_RESULT_NO_ERROR;
	unsigned int suspend_block_cnt = 0;
	struct codec_job *cur_job = 0;
	int target_freq = 0;
	u64 target_freq_64 = 0;

	if (pHWLock->eDriverType != VAL_DRIVER_TYPE_JPEG_ENC) {
		mutex_lock(&VcodecDVFSLock);
		handle = (unsigned long)(pHWLock->pvHandle);
		handle_id = pmem_user_v2p_video(handle);
		if (handle_id == 0) {
			pr_info("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&VcodecDVFSLock);
			return -1;
		}
		cur_job = add_job((void *)handle_id,
				&codec_jobs);
		/* pr_debug("cur_job's handle %p", cur_job->handle); */
		mutex_unlock(&VcodecDVFSLock);
	}
	while ((*bLockedHW) == VAL_FALSE) {
		/* Early break for JPEG VENC */
		if (pHWLock->u4TimeoutMs == 0) {
			if (CodecHWLock.pvHandle != 0) {
				/* Add one line comment for avoid
				 * kernel coding style, WARNING:BRACES:
				 */
				break;
			}
		}
		/* Wait to acquire Enc HW lock */
		mutex_lock(&gDrvInitParams->hwLockEventTimeoutLock);
		if (gDrvInitParams->HWLockEvent.u4TimeoutMs == 1) {
			pr_info("VENC_LOCKHW, First Use Enc HW %d!!\n",
					pHWLock->eDriverType);
			FirstUseEncHW = 1;
		} else {
			FirstUseEncHW = 0;
		}
		mutex_unlock(&gDrvInitParams->hwLockEventTimeoutLock);
		if (FirstUseEncHW == 1) {
			/* Add one line comment for avoid kernel coding
			 * style, WARNING:BRACES:
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
			if (pHWLock->u4TimeoutMs == 0) {
				/* Add one line comment for avoid kernel coding
				 * style, WARNING:BRACES:
				 */
				/* No wait */
				gDrvInitParams->HWLockEvent.u4TimeoutMs = 0;
			} else {
				/* Wait indefinitely */
				gDrvInitParams->HWLockEvent.u4TimeoutMs = 1000;
			}
		}
		mutex_unlock(&gDrvInitParams->hwLockEventTimeoutLock);

		mutex_lock(&gDrvInitParams->hwLock);
		handle = (unsigned long)(pHWLock->pvHandle);
		/* one process try to lock twice */
		handle_id = pmem_user_v2p_video(handle);
		if (handle_id == 0) {
			pr_info("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&gDrvInitParams->hwLock);
			return -1;
		}

		if (CodecHWLock.pvHandle ==
			(void *)handle_id) {
			pr_info("VENC_LOCKHW, Some inst double lock");
			pr_info("may timeout inst=0x%lx, TID=%d, type:%d\n",
				(unsigned long)CodecHWLock.pvHandle,
				current->pid, pHWLock->eDriverType);
		}
		mutex_unlock(&gDrvInitParams->hwLock);

		if (FirstUseEncHW == 0) {
			/* Add one line comment for avoid kernel coding style,
			 * WARNING:BRACES:
			 */
			eValRet = eVideoWaitEvent(&gDrvInitParams->HWLockEvent,
						sizeof(struct VAL_EVENT_T));
		}

		if (eValRet == VAL_RESULT_INVALID_ISR) {
			ret = vcodec_lockhw_enc_fail(*pHWLock, FirstUseEncHW);
			if (ret) {
				pr_info("lockhw_enc_fail: %lu\n",
					ret);
				return -EFAULT;
			}
		} else if (eValRet == VAL_RESULT_RESTARTSYS) {
			return -ERESTARTSYS;
		}

		mutex_lock(&gDrvInitParams->hwLock);
		/* No process use HW, so current process can use HW */
		if (CodecHWLock.pvHandle == 0) {
			while (is_entering_suspend == 1) {
				suspend_block_cnt++;
				if (suspend_block_cnt > 100000) {
					/* Print log if trying to enter suspend for too long*/
					pr_info("VENC blocked by suspend");
					suspend_block_cnt = 0;
				}
				usleep_range(1000, 2000);
			}
			handle_id = pmem_user_v2p_video(handle);
			if (handle_id == 0) {
				pr_info("[error] handle is freed at %d\n", __LINE__);
				mutex_unlock(&gDrvInitParams->hwLock);
				return -1;
			}

			CodecHWLock.pvHandle =
					(void *)handle_id;
			CodecHWLock.eDriverType = pHWLock->eDriverType;
			eVideoGetTimeOfDay(&CodecHWLock.rLockedTime,
					sizeof(struct VAL_TIME_T));
			/*
			 * pr_debug("VENC_LOCKHW, free to use HW\n");
			 * pr_debug("VENC_LOCKHW, handle = 0x%lx\n",
			 *	(unsigned long)CodecHWLock.pvHandle);
			 * pr_debug("Inst=0x%lx TID=%d, Time(%ds, %dus)",
			 *	(unsigned long)CodecHWLock.pvHandle,
			 *	current->pid,
			 *	CodecHWLock.rLockedTime.u4Sec,
			 *	CodecHWLock.rLockedTime.u4uSec);
			 */
			if (CodecHWLock.eDriverType !=
				VAL_DRIVER_TYPE_JPEG_ENC) {
				mutex_lock(&VcodecDVFSLock);
				vcodec_set_venc_opp(cur_job, &target_freq, &target_freq_64);
				/* pr_info("VENC cur_job freq %u, %llu, %d",
				 *target_freq, target_freq_64, enc_step_size);
				 */
				mutex_unlock(&VcodecDVFSLock);
			}
			*bLockedHW = VAL_TRUE;

#if IS_ENABLED(CONFIG_PM)
			pm_runtime_get_sync(gDrvInitParams->vcodec_device2);
#else
#ifndef KS_POWER_WORKAROUND
			venc_power_on(gVCodecDev));
#endif
#endif
			enable_irq(VENC_IRQ_ID);
		} else { /* someone use HW, and check timeout value */
			if (pHWLock->u4TimeoutMs == 0) {
				*bLockedHW = VAL_FALSE;
				mutex_unlock(&gDrvInitParams->hwLock);
				break;
			}

			eVideoGetTimeOfDay(&rCurTime,
						sizeof(struct VAL_TIME_T));
			u4TimeInterval = (((((rCurTime.u4Sec -
				CodecHWLock.rLockedTime.u4Sec) *
				1000000) + rCurTime.u4uSec) -
				CodecHWLock.rLockedTime.u4uSec) / 1000);

			/*
			 * pr_debug("VENC_LOCKHW, someone use enc HW");
			 * pr_debug("Time(ms) = %d, TimeOut(ms) = %d\n",
			 *	 u4TimeInterval, pHWLock->u4TimeoutMs);
			 * pr_debug("rLockedTime(%ds,%dus),rCurTime(%ds,%dus)",
			 *	CodecHWLock.rLockedTime.u4Sec,
			 *	CodecHWLock.rLockedTime.u4uSec,
			 *	rCurTime.u4Sec, rCurTime.u4uSec);
			 * pr_debug("LockInst=0x%lx, Inst=0x%lx, TID=%d\n",
			 *	 (unsigned long)CodecHWLock.pvHandle,
			 *	 pmem_user_v2p_video(handle),
			 *	 current->pid);
			 */
			++gLockTimeOutCount;
			pr_info("VENC_LOCKHW Ret %d Count %d First %d Lock %d ,Try %d Timeout %d",
				eValRet,
				gLockTimeOutCount,
				FirstUseEncHW,
				CodecHWLock.eDriverType,
				pHWLock->eDriverType,
				gDrvInitParams->HWLockEvent.u4TimeoutMs);
			if (gLockTimeOutCount > 30) {
				pr_info("VENC_LOCKHW %d fail 30 times",
						current->pid);
				pr_info("no TO 0x%lx,%lx,0x%lx,type:%d\n",
				(unsigned long)CodecHWLock.pvHandle,
				pmem_user_v2p_video(handle),
				handle,
				pHWLock->eDriverType);
				gLockTimeOutCount = 0;
				mutex_unlock(&gDrvInitParams->hwLock);
				return -EFAULT;
			}

			/* 2013/04/10. Cheng-Jung Never steal hardware lock */
		}

		if (*bLockedHW == VAL_TRUE) {
			/*
			 * pr_debug("VENC_LOCKHW handle:0x%lx,va:%lx,type:%d",
			 *	(unsigned long)CodecHWLock.pvHandle,
			 *	(unsigned long)(pHWLock->pvHandle),
			 *	pHWLock->eDriverType);
			 *
			 */
			gLockTimeOutCount = 0;
		}
		mutex_unlock(&gDrvInitParams->hwLock);
	}
	return 0;
}


#define ENC_LOCK_LOG \
	"VENC_LOCKHW \%d failed, locker 0x\%lx,\%lx,0x\%lx,type:\%d"

long vcodec_lockhw(unsigned long arg)
{
	unsigned char *user_data_addr;
	struct VAL_HW_LOCK_T rHWLock;
	unsigned long handle;
	enum VAL_RESULT_T eValRet;
	long ret;
	char bLockedHW = VAL_FALSE;
	unsigned long ulFlagsLockHW;
	unsigned int u4VcodecSel;
	unsigned int u4DeBlocking = 1;

	/* pr_debug("VCODEC_LOCKHW + tid = %d\n", current->pid); */

	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&rHWLock, user_data_addr, sizeof(struct
							VAL_HW_LOCK_T));
	if (ret) {
		pr_info("[VCODEC] LOCKHW copy_from_user failed: %lu\n",
				ret);
		return -EFAULT;
	}

	/* pr_debug("[VCODEC] LOCKHW eDriverType=%d\n", rHWLock.eDriverType); */
	eValRet = VAL_RESULT_INVALID_ISR;
	if (rHWLock.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC) {
		ret =  vcodec_lockhw_vdec(&rHWLock, &bLockedHW);
		if (ret != 0) {
			/* If there is error, return immediately */
			return ret;
		}
	} else if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
			(rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC)) {
		ret = vcodec_lockhw_venc(&rHWLock, &bLockedHW);
		if (ret != 0) {
			/* If there is error, return immediately */
			return ret;
		}

		if (bLockedHW == VAL_FALSE) {
			handle = (unsigned long)rHWLock.pvHandle;
			pr_info(ENC_LOCK_LOG,
				current->pid,
				(unsigned long)CodecHWLock.pvHandle,
				pmem_user_v2p_video(handle),
				handle,
				rHWLock.eDriverType);
				gLockTimeOutCount = 0;
			return -EFAULT;
		}

		spin_lock_irqsave(&gDrvInitParams->lockEncHWCountLock, ulFlagsLockHW);
		gu4LockEncHWCount++;
		spin_unlock_irqrestore(&gDrvInitParams->lockEncHWCountLock, ulFlagsLockHW);

		/*
		 * pr_debug("VCODEC_LOCKHW, get locked - ObjId =%d\n",
		 *		current->pid);
		 */

		/* pr_debug("VCODEC_LOCKHWed - tid = %d\n", current->pid); */
	} else {
		pr_info("[WARNING] VCODEC_LOCKHW Unknown instance type %d timeout %u\n",
			rHWLock.eDriverType,
			rHWLock.u4TimeoutMs);
		return -EFAULT;
	}

	/* shared vdec/venc VCODEC_SEL setting */
	if (rHWLock.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC) {
		u4VcodecSel = 0x2;
	} else if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC) {
		u4VcodecSel = 0x1;
		if (VDO_HW_READ(KVA_VDEC_GCON_BASE + 0x24) == 0) {
			do {
				VDO_HW_WRITE(KVA_VDEC_GCON_BASE + 0x24,
						u4DeBlocking);
			} while (VDO_HW_READ(KVA_VDEC_GCON_BASE + 0x24)
					!= u4DeBlocking);
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

	/* pr_debug("VCODEC_LOCKHW - tid = %d\n", current->pid); */

	return 0;
}

long vcodec_unlockhw(unsigned long arg)
{
	unsigned char *user_data_addr;
	struct VAL_HW_LOCK_T rHWLock;
	unsigned long handle = 0, handle_id = 0;
	enum VAL_RESULT_T eValRet;
	long ret;
	struct codec_job *cur_job = 0;
	/* pr_debug("VCODEC_UNLOCKHW + tid = %d\n", current->pid); */

	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&rHWLock, user_data_addr,
				sizeof(struct VAL_HW_LOCK_T));
	if (ret) {
		pr_info("[VCODEC] UNLOCKHW, copy_from_user failed: %lu\n",
				ret);
		return -EFAULT;
	}

	/* pr_debug("VCODEC_UNLOCKHW eDriverType = %d\n",
	 *		rHWLock.eDriverType);
	 */
	eValRet = VAL_RESULT_INVALID_ISR;
	handle = (unsigned long)rHWLock.pvHandle;
	if (rHWLock.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC) {
		mutex_lock(&gDrvInitParams->hwLock);

		handle_id = pmem_user_v2p_video(handle);
		if (handle_id == 0) {
			pr_info("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&gDrvInitParams->hwLock);
			return -1;
		}

		/* Current owner give up hw lock */
		if (CodecHWLock.pvHandle ==
				(void *)handle_id) {
			mutex_lock(&VcodecDVFSLock);
			cur_job = codec_jobs;
			if (cur_job == 0) {
				pr_info("VDEC job is null when unlock, error");
			} else if (cur_job->handle ==
				CodecHWLock.pvHandle) {
				cur_job->end = get_time_us();
				update_hist(cur_job, &codec_hists);
				codec_jobs = codec_jobs->next;
				kfree(cur_job);
			} else {
				pr_info("VCODEC wrong job at dec done %p, %p",
				cur_job->handle,
				CodecHWLock.pvHandle);
			}
			mutex_unlock(&VcodecDVFSLock);

			if (rHWLock.bSecureInst == VAL_FALSE) {
				/* Add one line comment for avoid kernel coding
				 * style, WARNING:BRACES:
				 */
				disable_irq(VDEC_IRQ_ID);
			}
			/* TODO: check if turning power off is ok */
#if IS_ENABLED(CONFIG_PM)
			pm_runtime_put_sync(gDrvInitParams->vcodec_device);
#else
#ifndef KS_POWER_WORKAROUND
			vdec_power_off(gVCodecDev);
#endif
#endif
			CodecHWLock.pvHandle = 0;
			CodecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
		} else { /* Not current owner */
			pr_info("[ERROR] VDEC_UNLOCKHW\n");
			pr_info("VDEC Not owner trying to unlock 0x%lx\n",
					pmem_user_v2p_video(handle));
			mutex_unlock(&gDrvInitParams->hwLock);
			return -EFAULT;
		}
		mutex_unlock(&gDrvInitParams->hwLock);
		eValRet = eVideoSetEvent(&gDrvInitParams->HWLockEvent,
					sizeof(struct VAL_EVENT_T));
	} else if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
			 rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC) {
		mutex_lock(&gDrvInitParams->hwLock);
		/* Current owner give up hw lock */
		handle_id = pmem_user_v2p_video(handle);
		if (handle_id == 0) {
			pr_info("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&gDrvInitParams->hwLock);
			return -1;
		}

		if (CodecHWLock.pvHandle ==
				(void *)handle_id) {
			if (rHWLock.eDriverType != VAL_DRIVER_TYPE_JPEG_ENC) {
				mutex_lock(&VcodecDVFSLock);
				cur_job = codec_jobs;
				if (cur_job == 0) {
					pr_info("VENC job is null when unlock, error");
				} else if (cur_job->handle ==
					CodecHWLock.pvHandle) {
					cur_job->end = get_time_us();
					update_hist(cur_job, &codec_hists);
					codec_jobs = codec_jobs->next;
					kfree(cur_job);
				} else {
					pr_info("VCODEC wrong job at enc done %p, %p",
						cur_job->handle,
						CodecHWLock.pvHandle);
				}
				mutex_unlock(&VcodecDVFSLock);
			}
			disable_irq(VENC_IRQ_ID);
			/* turn venc power off */
#if IS_ENABLED(CONFIG_PM)
			pm_runtime_put_sync(gDrvInitParams->vcodec_device2);
#else
#ifndef KS_POWER_WORKAROUND
			venc_power_off(gVCodecDev));
#endif
#endif
			CodecHWLock.pvHandle = 0;
			CodecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
		} else { /* Not current owner */
			/* [TODO] error handling */
			pr_info("[ERROR] VENC_UNLOCKHW\n");
			pr_info("VENC Not owner trying to unlock 0x%lx\n",
				pmem_user_v2p_video(handle));
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
	unsigned long handle, handle_id = 0;
	long ret;
	enum VAL_RESULT_T eValRet;

	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&val_isr, user_data_addr,
				sizeof(struct VAL_ISR_T));
	if (ret) {
		pr_info("[VCODEC] WAITISR, copy_from_user failed: %lu\n",
				ret);
		return -EFAULT;
	}

	handle = (unsigned long)val_isr.pvHandle;
	if (val_isr.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC) {
		mutex_lock(&gDrvInitParams->hwLock);

		handle_id = pmem_user_v2p_video(handle);
		if (handle_id == 0) {
			pr_info("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&gDrvInitParams->hwLock);
			return -1;
		}

		if (CodecHWLock.pvHandle ==
				(void *)handle_id) {
			/* Add one line comment for avoid kernel coding style,
			 * WARNING:BRACES:
			 */
			bLockedHW = VAL_TRUE;
		} else {
		}
		mutex_unlock(&gDrvInitParams->hwLock);

		if (bLockedHW == VAL_FALSE) {
			pr_info("VDEC_WAITISR, DO NOT have HWLock, fail\n");
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
			pr_info("VDEC_WAITISR, ERESTARTSYS\n");
			return -ERESTARTSYS;
		}
	} else if (val_isr.eDriverType == VAL_DRIVER_TYPE_H264_ENC) {
		mutex_lock(&gDrvInitParams->hwLock);

		handle_id = pmem_user_v2p_video(handle);
		if (handle_id == 0) {
			pr_info("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&gDrvInitParams->hwLock);
			return -1;
		}

		if (CodecHWLock.pvHandle ==
				(void *)handle_id) {
			/* Add one line comment for avoid kernel coding style,
			 * WARNING:BRACES:
			 */
			bLockedHW = VAL_TRUE;
		} else {
		}
		mutex_unlock(&gDrvInitParams->hwLock);

		if (bLockedHW == VAL_FALSE) {
			pr_info("VENC_WAITISR, DO NOT have HWLock, fail\n");
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
			pr_info("VENC_WAITISR, ERESTARTSYS\n");
			return -ERESTARTSYS;
		}

		if (val_isr.u4IrqStatusNum > 0) {
			val_isr.u4IrqStatus[0] = gu4HwVencIrqStatus;
			ret = copy_to_user(user_data_addr, &val_isr,
					sizeof(struct VAL_ISR_T));
			if (ret) {
				pr_info("VENC_WAITISR, cp status fail: %lu",
						ret);
				return -EFAULT;
			}
		}
	} else {
		pr_info("[WARNING] VCODEC_WAITISR Unknown instance\n");
		return -EFAULT;
	}
	return 0;
}

static long vcodec_set_frame_info(unsigned long arg)
{
	unsigned char *user_data_addr;
	long ret;
	struct VAL_FRAME_INFO_T rFrameInfo;
	int frame_type = 0;
	int b_freq_idx = 0;
	long emi_bw = 0;

	/* pr_debug("VCODEC_SET_FRAME_INFO + tid = %d\n", current->pid); */
	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&rFrameInfo, user_data_addr,
				sizeof(struct VAL_FRAME_INFO_T));
	if (ret) {
		pr_debug("%s, copy_from_user failed: %lu\n",
			__func__, ret);
		return -EFAULT;
	}

/* TODO check user
 *	mutex_lock(&VDecHWLock);
 *	if (grVcodecDecHWLock.pvHandle !=
 *	(VAL_VOID_T *)pmem_user_v2p_video((VAL_ULONG_T)rFrameInfo.handle)) {
 *
 *	MODULE_MFV_LOGD("[ERROR] VCODEC_SET_FRAME_INFO, vdec not locked");
 *		mutex_unlock(&VDecHWLock);
 *		return -EFAULT;
 *	}
 *	mutex_unlock(&VDecHWLock);
 */
	if (rFrameInfo.driver_type == VAL_DRIVER_TYPE_H264_DEC ||
		rFrameInfo.driver_type == VAL_DRIVER_TYPE_HEVC_DEC ||
		rFrameInfo.driver_type == VAL_DRIVER_TYPE_MP4_DEC ||
		rFrameInfo.driver_type == VAL_DRIVER_TYPE_MP1_MP2_DEC) {
		mutex_lock(&DecPMQoSLock);
		/* Request BW after lock hw, this should always be true */
		if (gVDECBWRequested == 0) {
			frame_type = rFrameInfo.frame_type;
			if (frame_type > 3 || frame_type < 0)
				frame_type = 0;

			if (gVCodecDev->vdec_freq_cnt > 1)
				b_freq_idx = gVCodecDev->vdec_freq_cnt - 1;

			/* 8bit * w * h * 1.5 * frame type ratio * freq ratio *
			 * decoding time relative to 1080p
			 */
			emi_bw = 8L * 1920 * 1080 * 3 * 10 * gVCodecDev->dec_freq;
			switch (rFrameInfo.driver_type) {
			case VAL_DRIVER_TYPE_H264_DEC:
				emi_bw = emi_bw * gVDECFrmTRAVC[frame_type] /
					(2 * STD_VDEC_FREQ);
				break;
			case VAL_DRIVER_TYPE_HEVC_DEC:
				emi_bw = emi_bw * gVDECFrmTRHEVC[frame_type] /
					(2 * STD_VDEC_FREQ);
				break;
			case VAL_DRIVER_TYPE_MP4_DEC:
			case VAL_DRIVER_TYPE_MP1_MP2_DEC:
				emi_bw = emi_bw * gVDECFrmTRMP2_4[frame_type] /
					(2 * STD_VDEC_FREQ);
				break;
			default:
				emi_bw = 0;
				pr_debug("Unsupported decoder type for BW");
			}

			if (rFrameInfo.is_compressed != 0)
				emi_bw = emi_bw * 6 / 10;

			/* pr_debug("UFO %d, emi_bw %ld",
			 *	rFrameInfo.is_compressed, emi_bw);
			 */

			/* input size */
			emi_bw += 8 * rFrameInfo.input_size * 100 * 1920 * 1088
					/ (rFrameInfo.frame_width *
					rFrameInfo.frame_height);

			/* pr_debug("input_size %d, w %d, h %d emi_bw %ld",
			 *	rFrameInfo.input_size, rFrameInfo.frame_width,
			 *	rFrameInfo.frame_height, emi_bw);
			 */

			/* transaction bytes to occupied BW */
			emi_bw = emi_bw * 4 / 3;

			/* bits/s to mbytes/s */
			emi_bw = emi_bw / (1024*1024) / 8;

			/* pr_info("VDEC mbytes/s emi_bw %ld", emi_bw); */
			//mtk_pm_qos_update_request(&vcodec_qos_request, (int)emi_bw);
			set_vdec_bw(gVCodecDev, (u64) emi_bw);
			gVDECBWRequested = 1;
		}
		mutex_unlock(&DecPMQoSLock);
	} else if (rFrameInfo.driver_type == VAL_DRIVER_TYPE_H264_ENC) {
		mutex_lock(&EncPMQoSLock);

		if (gVENCBWRequested == 0) {
			frame_type = rFrameInfo.frame_type;
			if (frame_type > 2 || frame_type < 0)
				frame_type = 0;

			if (gVCodecDev->venc_freq_cnt > 1)
				b_freq_idx = gVCodecDev->venc_freq_cnt - 1;

			/* 8bit * w * h * 1.5 * frame type ratio * freq ratio *
			 * decoding time relative to 1080p
			 */

			emi_bw = 8L * 1920 * 1080 * 3 * 10 * gVCodecDev->enc_freq;
			switch (rFrameInfo.driver_type) {
			case VAL_DRIVER_TYPE_H264_ENC:
				emi_bw = emi_bw * gVENCFrmTRAVC[frame_type] /
					(2 * STD_VENC_FREQ);
				break;
			default:
				emi_bw = 0;
				pr_debug("Unsupported encoder type for BW");
			}

			/* transaction bytes to occupied BW */
			emi_bw = emi_bw * 4 / 3;

			/* bits/s to mbytes/s */
			emi_bw = emi_bw / (1024*1024) / 8;

			/* pr_info("VENC mbytes/s emi_bw %ld", emi_bw); */
			set_venc_bw(gVCodecDev, (u64) emi_bw);
			gVENCBWRequested = 1;
		}
		mutex_unlock(&EncPMQoSLock);
	}
	/* pr_debug("VCODEC_SET_FRAME_INFO - tid = %d\n", current->pid); */

	return 0;
}

long vcodec_plat_unlocked_ioctl(unsigned int cmd, unsigned long arg)
{
	long ret;
	unsigned char *user_data_addr;

	pr_debug("%s %u +\n", __func__, cmd);
	switch (cmd) {
	case VCODEC_INC_DEC_EMI_USER:
	{
		/* pr_debug("VCODEC_INC_DEC_EMI_USER + tid = %d\n",
		 *		current->pid);
		 */

		mutex_lock(&gDrvInitParams->decEMILock);
		gDrvInitParams->u4DecEMICounter++;
		pr_debug("[VCODEC] DEC_EMI_USER = %d\n",
				gDrvInitParams->u4DecEMICounter);
		user_data_addr = (unsigned char *)arg;
		ret = copy_to_user(user_data_addr, &gDrvInitParams->u4DecEMICounter,
				sizeof(unsigned int));
		if (ret) {
			pr_info("INC_DEC_EMI_USER, copy_to_user fail: %lu",
					ret);
			mutex_unlock(&gDrvInitParams->decEMILock);
			return -EFAULT;
		}
		mutex_unlock(&gDrvInitParams->decEMILock);

		/* pr_debug("VCODEC_INC_DEC_EMI_USER - tid = %d\n",
		 *	current->pid);
		 */
	}
	break;

	case VCODEC_DEC_DEC_EMI_USER:
	{
		/* pr_debug("VCODEC_DEC_DEC_EMI_USER + tid = %d\n",
		 *		current->pid);
		 */

		mutex_lock(&gDrvInitParams->decEMILock);
		gDrvInitParams->u4DecEMICounter--;
		pr_debug("[VCODEC] DEC_EMI_USER = %d\n",
				gDrvInitParams->u4DecEMICounter);
		user_data_addr = (unsigned char *)arg;
		ret = copy_to_user(user_data_addr, &gDrvInitParams->u4DecEMICounter,
				sizeof(unsigned int));
		if (ret) {
			pr_info("DEC_DEC_EMI_USER, copy_to_user fail: %lu",
					ret);
			mutex_unlock(&gDrvInitParams->decEMILock);
			return -EFAULT;
		}
		mutex_unlock(&gDrvInitParams->decEMILock);

		/* pr_debug("VCODEC_DEC_DEC_EMI_USER - tid = %d\n",
		 *		current->pid);
		 */
	}
	break;

	case VCODEC_INC_PWR_USER:
	{
		/* pr_info("[INFO] VCODEC_INC_PWR_USER empty"); */
	}
	break;

	case VCODEC_DEC_PWR_USER:
	{
		/* pr_info("[INFO] VCODEC_DEC_PWR_USER empty"); */
	}
	break;

	case VCODEC_GET_CPU_LOADING_INFO:
	{
		pr_info("[INFO] VCODEC_GET_CPU_LOADING empty");
	}
	break;

	case VCODEC_GET_CORE_LOADING:
	{
		pr_info("[INFO] VCODEC_GET_CORE_LOADING empty");
	}
	break;

	case VCODEC_SET_CPU_OPP_LIMIT:
	{
		pr_info("[INFO] VCODEC_SET_CPU_OPP_LIMIT empty");
	}
	break;

	case VCODEC_SET_FRAME_INFO:
	{
		ret = vcodec_set_frame_info(arg);
		if (ret) {
			pr_debug("[ERROR] VCODEC_SET_FRAME_INFO failed! %lu\n",
				ret);
			return ret;
		}
	}
	break;

	default:
	{
		pr_info("[ERROR] vcodec_ioctl default case %u\n", cmd);
	}
	break;
	}
	return 0;
}

void vcodec_plat_release(void)
{
	/* check if someone didn't unlockHW */
	if (CodecHWLock.pvHandle != 0) {
		pr_info("err %s %d, type = %d, 0x%lx, gDrvInitParams->drvOpenCount = %d\n",
			__func__, current->pid,
			CodecHWLock.eDriverType,
			(unsigned long)CodecHWLock.pvHandle, gDrvInitParams->drvOpenCount);
		pr_info("err VCODEC_SEL 0x%x\n",
			VDO_HW_READ(KVA_VDEC_GCON_BASE + 0x20));

		/* power off */
		if (CodecHWLock.eDriverType ==
			VAL_DRIVER_TYPE_MP4_DEC ||
			CodecHWLock.eDriverType ==
				VAL_DRIVER_TYPE_HEVC_DEC ||
			CodecHWLock.eDriverType ==
				VAL_DRIVER_TYPE_H264_DEC ||
			CodecHWLock.eDriverType ==
				VAL_DRIVER_TYPE_MP1_MP2_DEC) {
			vdec_break();
			disable_irq(VDEC_IRQ_ID);
#if IS_ENABLED(CONFIG_PM)
			pm_runtime_put_sync(gDrvInitParams->vcodec_device);
#else
			vdec_power_off(gVCodecDev);
#endif
			//mtk_pm_qos_update_request(&vcodec_qos_request, 0);
			set_vdec_bw(gVCodecDev, 0);
			set_vdec_opp(gVCodecDev, 0);
			gVDECBWRequested = 0;
		} else if (CodecHWLock.eDriverType ==
				VAL_DRIVER_TYPE_H264_ENC) {
			venc_break();
			disable_irq(VENC_IRQ_ID);
#if IS_ENABLED(CONFIG_PM)
			pm_runtime_put_sync(gDrvInitParams->vcodec_device2);
#else
			venc_power_off(gVCodecDev);
#endif
			//mtk_pm_qos_update_request(&vcodec_qos_request2, 0);
			set_venc_bw(gVCodecDev, 0);
			set_venc_opp(gVCodecDev, 0);
		} else if (CodecHWLock.eDriverType ==
				VAL_DRIVER_TYPE_JPEG_ENC) {
			disable_irq(VENC_IRQ_ID);
#if IS_ENABLED(CONFIG_PM)
			pm_runtime_put_sync(gDrvInitParams->vcodec_device2);
#else
			venc_power_off(gVCodecDev);
#endif
		}
	}

	mutex_lock(&VcodecDVFSLock);
	free_hist(&codec_hists, 0);
	mutex_unlock(&VcodecDVFSLock);
}

#if IS_ENABLED(CONFIG_PM)
struct dev_pm_domain mt_vdec_pm_domain = {
	.ops = {
		SET_RUNTIME_PM_OPS(mt_vdec_runtime_suspend,
				mt_vdec_runtime_resume,
				NULL)
		}
};

struct dev_pm_domain mt_venc_pm_domain = {
	.ops = {
		SET_RUNTIME_PM_OPS(mt_venc_runtime_suspend,
				mt_venc_runtime_resume,
				NULL)
		}
};
#endif

/**
 * Suspend callbacks after user space processes are frozen
 * Since user space processes are frozen, there is no need and cannot hold same
 * mutex that protects lock owner while checking status.
 * If video codec hardware is still active now, must not aenter suspend.
 **/
int vcodec_suspend(struct platform_device *pDev, pm_message_t state)
{
	if (CodecHWLock.pvHandle != 0) {
		pr_info("%s fail due to videocodec activity", __func__);
		return -EBUSY;
	}
	pr_debug("%s ok", __func__);
	return 0;
}

int vcodec_resume(struct platform_device *pDev)
{
	pr_info("%s ok", __func__);
	return 0;
}

/**
 * Suspend notifiers before user space processes are frozen.
 * User space driver can still complete decoding/encoding of current frame.
 * Change state to is_entering_suspend to stop further tasks but allow current
 * frame to complete (LOCKHW, WAITISR, UNLOCKHW).
 * Since there is no critical section proection, it is possible for a new task
 * to start after changing to is_entering_suspend state. This case will be
 * handled by suspend callback vcodec_suspend.
 **/
static int vcodec_suspend_notifier(struct notifier_block *nb,
					unsigned long action, void *data)
{
	int wait_cnt = 0;

	pr_info("%s ok action = %ld", __func__, action);
	switch (action) {
	case PM_SUSPEND_PREPARE:
		is_entering_suspend = 1;
		while (CodecHWLock.pvHandle != 0) {
			wait_cnt++;
			if (wait_cnt > 90) {
				pr_info("%s waiting %p %d",
					__func__, CodecHWLock.pvHandle,
					(int)CodecHWLock.eDriverType);
				/* Current task is still not finished, don't
				 * care, will check again in real suspend
				 */
				return NOTIFY_DONE;
			}
			usleep_range(1000, 2000);
		}
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		is_entering_suspend = 0;
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_DONE;
}

int vcodec_plat_probe(struct platform_device *pdev, struct mtk_vcodec_dev *dev)
{
	int ret;
#if IS_ENABLED(CONFIG_PM)
	gDrvInitParams->vcodec_device->pm_domain = &mt_vdec_pm_domain;

	gDrvInitParams->vcodec_cdev2 = cdev_alloc();
	gDrvInitParams->vcodec_cdev2->owner = THIS_MODULE;
	gDrvInitParams->vcodec_cdev2->ops = &vcodec_fops;

	ret = cdev_add(gDrvInitParams->vcodec_cdev2, vcodec_devno2, 1);
	if (ret) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 */
		pr_info("[VCODEC] Can't add Vcodec Device 2\n");
	}

	gDrvInitParams->vcodec_class2 = class_create(THIS_MODULE, VCODEC_DEVNAME2);
	if (IS_ERR(gDrvInitParams->vcodec_class2)) {
		ret = PTR_ERR(gDrvInitParams->vcodec_class2);
		pr_info("[VCODEC] Unable to create class 2, err = %d", ret);
		return ret;
	}

	gDrvInitParams->vcodec_device2 =
		device_create(gDrvInitParams->vcodec_class2, NULL, vcodec_devno2,
					NULL, VCODEC_DEVNAME2);
	gDrvInitParams->vcodec_device2->pm_domain = &mt_venc_pm_domain;

	pm_runtime_enable(gDrvInitParams->vcodec_device);
	pm_runtime_enable(gDrvInitParams->vcodec_device2);
#endif

	pm_notifier(vcodec_suspend_notifier, 0);

	mtk_prepare_vdec_dvfs(dev);
	mtk_prepare_vdec_emi_bw(dev);
	if (dev->vdec_reg <= 0)
		pr_debug("Vdec get DVFS freq steps failed: %d\n", dev->vdec_reg);
	else if (dev->vdec_freq_cnt > 0 && dev->vdec_freq_cnt <= MAX_CODEC_FREQ_STEP)
		dev->dec_freq = dev->vdec_freqs[0];
	else
		dev->dec_freq = MIN_VDEC_FREQ;
	mtk_prepare_venc_dvfs(dev);
	mtk_prepare_venc_emi_bw(dev);
	if (dev->venc_reg <= 0)
		pr_debug("Venc get DVFS freq steps failed: %d\n", dev->venc_reg);
	else if (dev->venc_freq_cnt > 0 && dev->venc_freq_cnt <= MAX_CODEC_FREQ_STEP)
		dev->enc_freq = dev->venc_freqs[0];
	else
		dev->enc_freq = MIN_VENC_FREQ;

	dev->dev = &pdev->dev;
	gVCodecDev = dev;

#if IS_ENABLED(CONFIG_PM)
	dev_set_drvdata(gDrvInitParams->vcodec_device, dev);
	dev_set_drvdata(gDrvInitParams->vcodec_device2, dev);
#endif
	return 0;
}

void initDvfsParams(void)
{
#ifdef VCODEC_DVFS_V2
		mutex_lock(&VcodecDVFSLock);
		codec_hists = 0;
		codec_jobs = 0;
		mutex_unlock(&VcodecDVFSLock);
#endif
}

void vcodec_driver_plat_init(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mbist");
	if (node != NULL) {
		gBistFlag = 1;
		KVA_MBIST_BASE = of_iomap(node, 0);
	} else {
		gBistFlag = 0;
	}

	initDvfsParams();

	is_entering_suspend = 0;
}

void vcodec_driver_plat_exit(void)
{
#if IS_ENABLED(CONFIG_PM)
	cdev_del(gDrvInitParams->vcodec_cdev2);
#endif
}
