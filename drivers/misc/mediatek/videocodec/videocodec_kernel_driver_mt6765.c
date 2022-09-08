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

#define VCODEC_DEVNAME     "Vcodec"
#define VCODEC_DEVNAME2     "Vcodec2"
#define VCODEC_DEV_MAJOR_NUMBER 160   /* 189 */


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

/* VENC physical base address */
#undef VENC_BASE
#define VENC_BASE       0x17002000
#define VENC_REGION     0x1000

/* VDEC virtual base address */
#define VDEC_BASE_PHY   0x17000000
#define VDEC_REGION     0x50000

#define HW_BASE         0x7FFF000
#define HW_REGION       0x2000

#define INFO_BASE       0x10000000
#define INFO_REGION     0x1000


#define DRAM_DONE_POLLING_LIMIT 20000

/* disable temporary for alaska DVT verification, smi seems not ready */
#define ENABLE_MMDVFS_VDEC
#ifdef ENABLE_MMDVFS_VDEC
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

	mutex_lock(&gDrvInitParams->vdecPWRLock);
	gDrvInitParams->u4VdecPWRCounter++;
	mutex_unlock(&gDrvInitParams->vdecPWRLock);
	ret = 0;
	mtk_vcodec_clock_on(MTK_INST_DECODER, dev);
	if (gBistFlag) {
		/* GF14 type SRAM  power on config */
		VDO_HW_WRITE(KVA_MBIST_BASE, 0x93CEB);
	}

}

void vdec_power_off(struct mtk_vcodec_dev *dev)
{

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
	if (gDfvsFlag == 1) {
		set_vdec_bw(gVCodecDev, 0);
		gVDECBWRequested = 0;
		set_vdec_opp(gVCodecDev, 0);
	}
	mutex_unlock(&DecPMQoSLock);
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
	if (gDfvsFlag == 1) {
		set_venc_bw(gVCodecDev, 0);
		gVENCBWRequested = 0;
		set_venc_opp(gVCodecDev, 0);
	}
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

	if (gDfvsFlag == 1) {
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
	}
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
			if (gWakeLock == 0) {
				while (is_entering_suspend == 1) {
					suspend_block_cnt++;
					if (suspend_block_cnt > 100000) {
						/* Print log if trying to enter suspend
						 * for too long
						 */
						pr_info("VDEC blocked by suspend");
						suspend_block_cnt = 0;
					}
					usleep_range(1000, 2000);
				}
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

			if (gDfvsFlag == 1) {
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
			}
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

	if (gDfvsFlag == 1) {
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
			if (gWakeLock == 0) {
				while (is_entering_suspend == 1) {
					suspend_block_cnt++;
					if (suspend_block_cnt > 100000) {
						/* Print log if trying to enter
						 * suspend for too long
						 */
						pr_info("VENC blocked by suspend");
						suspend_block_cnt = 0;
					}
					usleep_range(1000, 2000);
				}
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

static long vcodec_lockhw(unsigned long arg)
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

static long vcodec_unlockhw(unsigned long arg)
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
			if (gDfvsFlag == 1) {
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
			}
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
			if (gDfvsFlag == 1) {
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
			}
			disable_irq(VENC_IRQ_ID);
			/* turn venc power off */
#if IS_ENABLED(CONFIG_PM)
			pm_runtime_put_sync(gDrvInitParams->vcodec_device2);
#else
#ifndef KS_POWER_WORKAROUND
			venc_power_on(gVCodecDev));
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

static long vcodec_waitisr(unsigned long arg)
{
	unsigned char *user_data_addr;
	struct VAL_ISR_T val_isr;
	char bLockedHW = VAL_FALSE;
	unsigned long ulFlags;
	unsigned long handle, handle_id = 0;
	long ret;
	enum VAL_RESULT_T eValRet;

	/* pr_debug("VCODEC_WAITISR + tid = %d\n", current->pid); */

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

	/* pr_debug("VCODEC_WAITISR - tid = %d\n", current->pid); */

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

static long vcodec_unlocked_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	long ret;
	unsigned char *user_data_addr;
	int temp_nr_cpu_ids;
	char rIncLogCount;


	switch (cmd) {
	case VCODEC_SET_THREAD_ID:
	{
		pr_info("[INFO] VCODEC_SET_THREAD_ID empty");
	}
	break;

	case VCODEC_ALLOC_NON_CACHE_BUFFER:
	{
		pr_info("[INFO] VCODEC_ALLOC_NON_CACHE_BUFFER  empty");
	}
	break;

	case VCODEC_FREE_NON_CACHE_BUFFER:
	{
		pr_info("[INFO] VCODEC_FREE_NON_CACHE_BUFFER  empty");
	}
	break;

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

	case VCODEC_INC_ENC_EMI_USER:
	{
		/* pr_debug("VCODEC_INC_ENC_EMI_USER + tid = %d\n",
		 *		current->pid);
		 */

		mutex_lock(&gDrvInitParams->encEMILock);
		gDrvInitParams->u4EncEMICounter++;
		pr_debug("[VCODEC] ENC_EMI_USER = %d\n",
				gDrvInitParams->u4EncEMICounter);
		user_data_addr = (unsigned char *)arg;
		ret = copy_to_user(user_data_addr, &gDrvInitParams->u4EncEMICounter,
				sizeof(unsigned int));
		if (ret) {
			pr_info("INC_ENC_EMI_USER, copy_to_user fail: %lu",
					ret);
			mutex_unlock(&gDrvInitParams->encEMILock);
			return -EFAULT;
		}
		mutex_unlock(&gDrvInitParams->encEMILock);

		/* pr_debug("VCODEC_INC_ENC_EMI_USER - tid = %d\n",
		 *		current->pid);
		 */
	}
	break;

	case VCODEC_DEC_ENC_EMI_USER:
	{
		/* pr_debug("VCODEC_DEC_ENC_EMI_USER + tid = %d\n",
		 *		current->pid);
		 */

		mutex_lock(&gDrvInitParams->encEMILock);
		gDrvInitParams->u4EncEMICounter--;
		pr_info("[VCODEC] ENC_EMI_USER = %d\n",
				gDrvInitParams->u4EncEMICounter);
		user_data_addr = (unsigned char *)arg;
		ret = copy_to_user(user_data_addr, &gDrvInitParams->u4EncEMICounter,
					sizeof(unsigned int));
		if (ret) {
			pr_info("DEC_ENC_EMI_USER, copy_to_user fail: %lu",
					ret);
			mutex_unlock(&gDrvInitParams->encEMILock);
			return -EFAULT;
		}
		mutex_unlock(&gDrvInitParams->encEMILock);

		/* pr_debug("VCODEC_DEC_ENC_EMI_USER - tid = %d\n",
		 *		current->pid);
		 */
	}
	break;

	case VCODEC_LOCKHW:
	{
		ret = vcodec_lockhw(arg);
		if (ret) {
			pr_info("[ERROR] VCODEC_LOCKHW failed! %lu\n",
					ret);
			return ret;
		}
	}
		break;

	case VCODEC_UNLOCKHW:
	{
		ret = vcodec_unlockhw(arg);
		if (ret) {
			pr_info("[ERROR] VCODEC_UNLOCKHW failed! %lu\n",
					ret);
			return ret;
		}
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

	case VCODEC_WAITISR:
	{
		ret = vcodec_waitisr(arg);
		if (ret) {
			pr_info("[ERROR] VCODEC_WAITISR failed! %lu\n",
					ret);
			return ret;
		}
	}
	break;

	case VCODEC_INITHWLOCK:
	{
		pr_info("[INFO] VCODEC_INITHWLOCK empty");
	}
	break;

	case VCODEC_DEINITHWLOCK:
	{
		pr_info("[INFO] VCODEC_DEINITHWLOCK empty");
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

	case VCODEC_GET_CORE_NUMBER:
	{
		/* pr_debug("VCODEC_GET_CORE_NUMBER + - tid = %d\n",
		 *		current->pid);
		 */

		user_data_addr = (unsigned char *)arg;
		temp_nr_cpu_ids = nr_cpu_ids;
		ret = copy_to_user(user_data_addr, &temp_nr_cpu_ids,
				sizeof(int));
		if (ret) {
			pr_info("GET_CORE_NUMBER, copy_to_user failed: %lu",
					ret);
			return -EFAULT;
		}
		/* pr_debug("VCODEC_GET_CORE_NUMBER - - tid = %d\n",
		 *		current->pid);
		 */
	}
	break;

	case VCODEC_SET_CPU_OPP_LIMIT:
	{
		pr_info("[INFO] VCODEC_SET_CPU_OPP_LIMIT empty");
	}
	break;

	case VCODEC_MB:
	{
		/* For userspace to guarantee register setting order */
		mb();
	}
	break;

	case VCODEC_SET_LOG_COUNT:
	{
		/* pr_debug("VCODEC_SET_LOG_COUNT + tid = %d\n",
		 *		current->pid);
		 */

		mutex_lock(&gDrvInitParams->logCountLock);
		user_data_addr = (unsigned char *)arg;
		ret = copy_from_user(&rIncLogCount, user_data_addr,
				sizeof(char));
		if (ret) {
			pr_info("SET_LOG_COUNT, copy_from_user failed: %lu",
					ret);
			mutex_unlock(&gDrvInitParams->logCountLock);
			return -EFAULT;
		}

		if (rIncLogCount == VAL_TRUE) {
			if (gDrvInitParams->u4LogCountUser == 0) {
				/* gDrvInitParams->u4LogCount = get_detect_count(); */
				/* set_detect_count(gDrvInitParams->u4LogCount + 100); */
			}
			gDrvInitParams->u4LogCountUser++;
		} else {
			gDrvInitParams->u4LogCountUser--;
			if (gDrvInitParams->u4LogCountUser == 0) {
				/* set_detect_count(gDrvInitParams->u4LogCount); */
				gDrvInitParams->u4LogCount = 0;
			}
		}
		mutex_unlock(&gDrvInitParams->logCountLock);

		/* pr_debug("VCODEC_SET_LOG_COUNT - tid = %d\n",
		 *		current->pid);
		 */
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
	return 0xFF;
}

#if IS_ENABLED(CONFIG_COMPAT)

enum STRUCT_TYPE {
	VAL_HW_LOCK_TYPE = 0,
	VAL_POWER_TYPE,
	VAL_ISR_TYPE,
	VAL_MEMORY_TYPE,
	VAL_FRAME_INFO_TYPE
};

enum COPY_DIRECTION {
	COPY_FROM_USER = 0,
	COPY_TO_USER,
};

struct COMPAT_VAL_HW_LOCK_T {
	/* [IN]     The video codec driver handle */
	compat_uptr_t       pvHandle;
	/* [IN]     The size of video codec driver handle */
	compat_uint_t       u4HandleSize;
	/* [IN/OUT] The Lock discriptor */
	compat_uptr_t       pvLock;
	/* [IN]     The timeout ms */
	compat_uint_t       u4TimeoutMs;
	/* [IN/OUT] The reserved parameter */
	compat_uptr_t       pvReserved;
	/* [IN]     The size of reserved parameter structure */
	compat_uint_t       u4ReservedSize;
	/* [IN]     The driver type */
	compat_uint_t       eDriverType;
	/* [IN]     True if this is a secure instance */
	/* MTK_SEC_VIDEO_PATH_SUPPORT */
	char                bSecureInst;
};

struct COMPAT_VAL_POWER_T {
	/* [IN]     The video codec driver handle */
	compat_uptr_t       pvHandle;
	/* [IN]     The size of video codec driver handle */
	compat_uint_t       u4HandleSize;
	/* [IN]     The driver type */
	compat_uint_t       eDriverType;
	/* [IN]     Enable or not. */
	char                fgEnable;
	/* [IN/OUT] The reserved parameter */
	compat_uptr_t       pvReserved;
	/* [IN]     The size of reserved parameter structure */
	compat_uint_t       u4ReservedSize;
	/* [OUT]    The number of power user right now */
	/* unsigned int        u4L2CUser; */
};

struct COMPAT_VAL_ISR_T {
	/* [IN]     The video codec driver handle */
	compat_uptr_t       pvHandle;
	/* [IN]     The size of video codec driver handle */
	compat_uint_t       u4HandleSize;
	/* [IN]     The driver type */
	compat_uint_t       eDriverType;
	/* [IN]     The isr function */
	compat_uptr_t       pvIsrFunction;
	/* [IN/OUT] The reserved parameter */
	compat_uptr_t       pvReserved;
	/* [IN]     The size of reserved parameter structure */
	compat_uint_t       u4ReservedSize;
	/* [IN]     The timeout in ms */
	compat_uint_t       u4TimeoutMs;
	/* [IN]     The num of return registers when HW done */
	compat_uint_t       u4IrqStatusNum;
	/* [IN/OUT] The value of return registers when HW done */
	compat_uint_t       u4IrqStatus[IRQ_STATUS_MAX_NUM];
};

struct COMPAT_VAL_MEMORY_T {
	/* [IN]     The allocation memory type */
	compat_uint_t       eMemType;
	/* [IN]     The size of memory allocation */
	compat_ulong_t      u4MemSize;
	/* [IN/OUT] The memory virtual address */
	compat_uptr_t       pvMemVa;
	/* [IN/OUT] The memory physical address */
	compat_uptr_t       pvMemPa;
	/* [IN]     The memory byte alignment setting */
	compat_uint_t       eAlignment;
	/* [IN/OUT] The align memory virtual address */
	compat_uptr_t       pvAlignMemVa;
	/* [IN/OUT] The align memory physical address */
	compat_uptr_t       pvAlignMemPa;
	/* [IN]     The memory codec for VENC or VDEC */
	compat_uint_t       eMemCodec;
	compat_uint_t       i4IonShareFd;
	compat_uptr_t       pIonBufhandle;
	/* [IN/OUT] The reserved parameter */
	compat_uptr_t       pvReserved;
	/* [IN]     The size of reserved parameter structure */
	compat_ulong_t      u4ReservedSize;
};

struct COMPAT_VAL_FRAME_INFO_T {
	compat_uptr_t handle;
	compat_uint_t driver_type;
	compat_uint_t input_size;
	compat_uint_t frame_width;
	compat_uint_t frame_height;
	compat_uint_t frame_type;
	compat_uint_t is_compressed;
};


static int get_uptr_to_32(compat_uptr_t *p, void __user **uptr)
{
	void __user *p2p;
	int err = get_user(p2p, uptr);
	*p = ptr_to_compat(p2p);
	return err;
}
static int compat_copy_struct(
			enum STRUCT_TYPE eType,
			enum COPY_DIRECTION eDirection,
			void __user *data32,
			void __user *data)
{
	compat_uint_t u;
	compat_ulong_t l;
	compat_uptr_t p;
	char c;
	int err = 0;

	switch (eType) {
	case VAL_HW_LOCK_TYPE:
	{
		if (eDirection == COPY_FROM_USER) {
			struct COMPAT_VAL_HW_LOCK_T __user *from32 =
					(struct COMPAT_VAL_HW_LOCK_T *)data32;
			struct VAL_HW_LOCK_T __user *to =
						(struct VAL_HW_LOCK_T *)data;

			err = get_user(p, &(from32->pvHandle));
			err |= put_user(compat_ptr(p), &(to->pvHandle));
			err |= get_user(u, &(from32->u4HandleSize));
			err |= put_user(u, &(to->u4HandleSize));
			err |= get_user(p, &(from32->pvLock));
			err |= put_user(compat_ptr(p), &(to->pvLock));
			err |= get_user(u, &(from32->u4TimeoutMs));
			err |= put_user(u, &(to->u4TimeoutMs));
			err |= get_user(p, &(from32->pvReserved));
			err |= put_user(compat_ptr(p), &(to->pvReserved));
			err |= get_user(u, &(from32->u4ReservedSize));
			err |= put_user(u, &(to->u4ReservedSize));
			err |= get_user(u, &(from32->eDriverType));
			err |= put_user(u, &(to->eDriverType));
			err |= get_user(c, &(from32->bSecureInst));
			err |= put_user(c, &(to->bSecureInst));
		} else {
			struct COMPAT_VAL_HW_LOCK_T __user *to32 =
					(struct COMPAT_VAL_HW_LOCK_T *)data32;
			struct VAL_HW_LOCK_T __user *from =
						(struct VAL_HW_LOCK_T *)data;

			err = get_uptr_to_32(&p, &(from->pvHandle));
			err |= put_user(p, &(to32->pvHandle));
			err |= get_user(u, &(from->u4HandleSize));
			err |= put_user(u, &(to32->u4HandleSize));
			err |= get_uptr_to_32(&p, &(from->pvLock));
			err |= put_user(p, &(to32->pvLock));
			err |= get_user(u, &(from->u4TimeoutMs));
			err |= put_user(u, &(to32->u4TimeoutMs));
			err |= get_uptr_to_32(&p, &(from->pvReserved));
			err |= put_user(p, &(to32->pvReserved));
			err |= get_user(u, &(from->u4ReservedSize));
			err |= put_user(u, &(to32->u4ReservedSize));
			err |= get_user(u, &(from->eDriverType));
			err |= put_user(u, &(to32->eDriverType));
			err |= get_user(c, &(from->bSecureInst));
			err |= put_user(c, &(to32->bSecureInst));
		}
	}
	break;
	case VAL_POWER_TYPE:
	{
		if (eDirection == COPY_FROM_USER) {
			struct COMPAT_VAL_POWER_T __user *from32 =
					(struct COMPAT_VAL_POWER_T *)data32;
			struct VAL_POWER_T __user *to =
						(struct VAL_POWER_T *)data;

			err = get_user(p, &(from32->pvHandle));
			err |= put_user(compat_ptr(p), &(to->pvHandle));
			err |= get_user(u, &(from32->u4HandleSize));
			err |= put_user(u, &(to->u4HandleSize));
			err |= get_user(u, &(from32->eDriverType));
			err |= put_user(u, &(to->eDriverType));
			err |= get_user(c, &(from32->fgEnable));
			err |= put_user(c, &(to->fgEnable));
			err |= get_user(p, &(from32->pvReserved));
			err |= put_user(compat_ptr(p), &(to->pvReserved));
			err |= get_user(u, &(from32->u4ReservedSize));
			err |= put_user(u, &(to->u4ReservedSize));
		} else {
			struct COMPAT_VAL_POWER_T __user *to32 =
					(struct COMPAT_VAL_POWER_T *)data32;
			struct VAL_POWER_T __user *from =
					(struct VAL_POWER_T *)data;

			err = get_uptr_to_32(&p, &(from->pvHandle));
			err |= put_user(p, &(to32->pvHandle));
			err |= get_user(u, &(from->u4HandleSize));
			err |= put_user(u, &(to32->u4HandleSize));
			err |= get_user(u, &(from->eDriverType));
			err |= put_user(u, &(to32->eDriverType));
			err |= get_user(c, &(from->fgEnable));
			err |= put_user(c, &(to32->fgEnable));
			err |= get_uptr_to_32(&p, &(from->pvReserved));
			err |= put_user(p, &(to32->pvReserved));
			err |= get_user(u, &(from->u4ReservedSize));
			err |= put_user(u, &(to32->u4ReservedSize));
		}
	}
	break;
	case VAL_ISR_TYPE:
	{
		int i = 0;

		if (eDirection == COPY_FROM_USER) {
			struct COMPAT_VAL_ISR_T __user *from32 =
					(struct COMPAT_VAL_ISR_T *)data32;
			struct VAL_ISR_T __user *to = (struct VAL_ISR_T *)data;

			err = get_user(p, &(from32->pvHandle));
			err |= put_user(compat_ptr(p), &(to->pvHandle));
			err |= get_user(u, &(from32->u4HandleSize));
			err |= put_user(u, &(to->u4HandleSize));
			err |= get_user(u, &(from32->eDriverType));
			err |= put_user(u, &(to->eDriverType));
			err |= get_user(p, &(from32->pvIsrFunction));
			err |= put_user(compat_ptr(p), &(to->pvIsrFunction));
			err |= get_user(p, &(from32->pvReserved));
			err |= put_user(compat_ptr(p), &(to->pvReserved));
			err |= get_user(u, &(from32->u4ReservedSize));
			err |= put_user(u, &(to->u4ReservedSize));
			err |= get_user(u, &(from32->u4TimeoutMs));
			err |= put_user(u, &(to->u4TimeoutMs));
			err |= get_user(u, &(from32->u4IrqStatusNum));
			err |= put_user(u, &(to->u4IrqStatusNum));
			for (; i < IRQ_STATUS_MAX_NUM; i++) {
				err |= get_user(u, &(from32->u4IrqStatus[i]));
				err |= put_user(u, &(to->u4IrqStatus[i]));
			}
			return err;

		} else {
			struct COMPAT_VAL_ISR_T __user *to32 =
					(struct COMPAT_VAL_ISR_T *)data32;
			struct VAL_ISR_T __user *from =
						(struct VAL_ISR_T *)data;

			err = get_uptr_to_32(&p, &(from->pvHandle));
			err |= put_user(p, &(to32->pvHandle));
			err |= get_user(u, &(from->u4HandleSize));
			err |= put_user(u, &(to32->u4HandleSize));
			err |= get_user(u, &(from->eDriverType));
			err |= put_user(u, &(to32->eDriverType));
			err |= get_uptr_to_32(&p, &(from->pvIsrFunction));
			err |= put_user(p, &(to32->pvIsrFunction));
			err |= get_uptr_to_32(&p, &(from->pvReserved));
			err |= put_user(p, &(to32->pvReserved));
			err |= get_user(u, &(from->u4ReservedSize));
			err |= put_user(u, &(to32->u4ReservedSize));
			err |= get_user(u, &(from->u4TimeoutMs));
			err |= put_user(u, &(to32->u4TimeoutMs));
			err |= get_user(u, &(from->u4IrqStatusNum));
			err |= put_user(u, &(to32->u4IrqStatusNum));
			for (; i < IRQ_STATUS_MAX_NUM; i++) {
				err |= get_user(u, &(from->u4IrqStatus[i]));
				err |= put_user(u, &(to32->u4IrqStatus[i]));
			}
		}
	}
	break;
	case VAL_MEMORY_TYPE:
	{
		if (eDirection == COPY_FROM_USER) {
			struct COMPAT_VAL_MEMORY_T __user *from32 =
					(struct COMPAT_VAL_MEMORY_T *)data32;
			struct VAL_MEMORY_T __user *to =
						(struct VAL_MEMORY_T *)data;

			err = get_user(u, &(from32->eMemType));
			err |= put_user(u, &(to->eMemType));
			err |= get_user(l, &(from32->u4MemSize));
			err |= put_user(l, &(to->u4MemSize));
			err |= get_user(p, &(from32->pvMemVa));
			err |= put_user(compat_ptr(p), &(to->pvMemVa));
			err |= get_user(p, &(from32->pvMemPa));
			err |= put_user(compat_ptr(p), &(to->pvMemPa));
			err |= get_user(u, &(from32->eAlignment));
			err |= put_user(u, &(to->eAlignment));
			err |= get_user(p, &(from32->pvAlignMemVa));
			err |= put_user(compat_ptr(p), &(to->pvAlignMemVa));
			err |= get_user(p, &(from32->pvAlignMemPa));
			err |= put_user(compat_ptr(p), &(to->pvAlignMemPa));
			err |= get_user(u, &(from32->eMemCodec));
			err |= put_user(u, &(to->eMemCodec));
			err |= get_user(u, &(from32->i4IonShareFd));
			err |= put_user(u, &(to->i4IonShareFd));
			err |= get_user(p, &(from32->pIonBufhandle));
			err |= put_user(compat_ptr(p), &(to->pIonBufhandle));
			err |= get_user(p, &(from32->pvReserved));
			err |= put_user(compat_ptr(p), &(to->pvReserved));
			err |= get_user(l, &(from32->u4ReservedSize));
			err |= put_user(l, &(to->u4ReservedSize));
		} else {
			struct COMPAT_VAL_MEMORY_T __user *to32 =
					(struct COMPAT_VAL_MEMORY_T *)data32;

			struct VAL_MEMORY_T __user *from =
					(struct VAL_MEMORY_T *)data;

			err = get_user(u, &(from->eMemType));
			err |= put_user(u, &(to32->eMemType));
			err |= get_user(l, &(from->u4MemSize));
			err |= put_user(l, &(to32->u4MemSize));
			err |= get_uptr_to_32(&p, &(from->pvMemVa));
			err |= put_user(p, &(to32->pvMemVa));
			err |= get_uptr_to_32(&p, &(from->pvMemPa));
			err |= put_user(p, &(to32->pvMemPa));
			err |= get_user(u, &(from->eAlignment));
			err |= put_user(u, &(to32->eAlignment));
			err |= get_uptr_to_32(&p, &(from->pvAlignMemVa));
			err |= put_user(p, &(to32->pvAlignMemVa));
			err |= get_uptr_to_32(&p, &(from->pvAlignMemPa));
			err |= put_user(p, &(to32->pvAlignMemPa));
			err |= get_user(u, &(from->eMemCodec));
			err |= put_user(u, &(to32->eMemCodec));
			err |= get_user(u, &(from->i4IonShareFd));
			err |= put_user(u, &(to32->i4IonShareFd));
			err |= get_uptr_to_32(&p,
					(void __user **)&(from->pIonBufhandle));
			err |= put_user(p, &(to32->pIonBufhandle));
			err |= get_uptr_to_32(&p, &(from->pvReserved));
			err |= put_user(p, &(to32->pvReserved));
			err |= get_user(l, &(from->u4ReservedSize));
			err |= put_user(l, &(to32->u4ReservedSize));
		}
	}
	break;
	case VAL_FRAME_INFO_TYPE:
	{
		if (eDirection == COPY_FROM_USER) {
			struct COMPAT_VAL_FRAME_INFO_T __user *from32 =
				(struct COMPAT_VAL_FRAME_INFO_T *)data32;
			struct VAL_FRAME_INFO_T __user *to =
				(struct VAL_FRAME_INFO_T *)data;

			err = get_user(p, &(from32->handle));
			err |= put_user(compat_ptr(p), &(to->handle));
			err |= get_user(u, &(from32->driver_type));
			err |= put_user(u, &(to->driver_type));
			err |= get_user(u, &(from32->input_size));
			err |= put_user(u, &(to->input_size));
			err |= get_user(u, &(from32->frame_width));
			err |= put_user(u, &(to->frame_width));
			err |= get_user(u, &(from32->frame_height));
			err |= put_user(u, &(to->frame_height));
			err |= get_user(u, &(from32->frame_type));
			err |= put_user(u, &(to->frame_type));
			err |= get_user(u, &(from32->is_compressed));
			err |= put_user(u, &(to->is_compressed));
		} else {
			struct COMPAT_VAL_FRAME_INFO_T __user *to32 =
				(struct COMPAT_VAL_FRAME_INFO_T *)data32;
			struct VAL_FRAME_INFO_T __user *from =
				(struct VAL_FRAME_INFO_T *)data;

			err = get_uptr_to_32(&p, &(from->handle));
			err |= put_user(p, &(to32->handle));
			err |= get_user(u, &(from->driver_type));
			err |= put_user(u, &(to32->driver_type));
			err |= get_user(u, &(from->input_size));
			err |= put_user(u, &(to32->input_size));
			err |= get_user(u, &(from->frame_width));
			err |= put_user(u, &(to32->frame_width));
			err |= get_user(u, &(from->frame_height));
			err |= put_user(u, &(to32->frame_height));
			err |= get_user(u, &(from->frame_type));
			err |= put_user(u, &(to32->frame_type));
			err |= get_user(u, &(from->is_compressed));
			err |= put_user(u, &(to32->is_compressed));
		}
	}
	break;
	default:
	break;
	}

	return err;
}


static long vcodec_unlocked_compat_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	long ret = 0;
	/* pr_debug("vcodec_unlocked_compat_ioctl: 0x%x\n", cmd); */
	switch (cmd) {
	case VCODEC_ALLOC_NON_CACHE_BUFFER:
	case VCODEC_FREE_NON_CACHE_BUFFER:
	{
		struct COMPAT_VAL_MEMORY_T __user *data32;
		struct VAL_MEMORY_T __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(struct VAL_MEMORY_T));
		if (data == NULL)
			return -EFAULT;

		err = compat_copy_struct(VAL_MEMORY_TYPE, COPY_FROM_USER,
					(void *)data32, (void *)data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, cmd,
						(unsigned long)data);

		err = compat_copy_struct(VAL_MEMORY_TYPE, COPY_TO_USER,
					(void *)data32, (void *)data);

		if (err)
			return err;
		return ret;
	}
	break;
	case VCODEC_LOCKHW:
	case VCODEC_UNLOCKHW:
	{
		struct COMPAT_VAL_HW_LOCK_T __user *data32;
		struct VAL_HW_LOCK_T __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(struct VAL_HW_LOCK_T));
		if (data == NULL)
			return -EFAULT;

		err = compat_copy_struct(VAL_HW_LOCK_TYPE, COPY_FROM_USER,
					(void *)data32, (void *)data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, cmd,
						(unsigned long)data);

		err = compat_copy_struct(VAL_HW_LOCK_TYPE, COPY_TO_USER,
					(void *)data32, (void *)data);

		if (err)
			return err;
		return ret;
	}
	break;

	case VCODEC_INC_PWR_USER:
	case VCODEC_DEC_PWR_USER:
	{
		struct COMPAT_VAL_POWER_T __user *data32;
		struct VAL_POWER_T __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(struct VAL_POWER_T));
		if (data == NULL)
			return -EFAULT;

		err = compat_copy_struct(VAL_POWER_TYPE, COPY_FROM_USER,
					(void *)data32, (void *)data);

		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, cmd,
						(unsigned long)data);

		err = compat_copy_struct(VAL_POWER_TYPE, COPY_TO_USER,
					(void *)data32, (void *)data);

		if (err)
			return err;
		return ret;
	}
	break;

	case VCODEC_WAITISR:
	{
		struct COMPAT_VAL_ISR_T __user *data32;
		struct VAL_ISR_T __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(struct VAL_ISR_T));
		if (data == NULL)
			return -EFAULT;

		err = compat_copy_struct(VAL_ISR_TYPE, COPY_FROM_USER,
					(void *)data32, (void *)data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, VCODEC_WAITISR,
						(unsigned long)data);

		err = compat_copy_struct(VAL_ISR_TYPE, COPY_TO_USER,
					(void *)data32, (void *)data);

		if (err)
			return err;
		return ret;
	}
	break;

	case VCODEC_SET_FRAME_INFO:
	{
		struct COMPAT_VAL_FRAME_INFO_T __user *data32;
		struct VAL_FRAME_INFO_T __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(struct VAL_FRAME_INFO_T));
		if (data == NULL)
			return -EFAULT;

		err = compat_copy_struct(VAL_FRAME_INFO_TYPE,
				COPY_FROM_USER, (void *)data32, (void *)data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, VCODEC_SET_FRAME_INFO,
						(unsigned long)data);

		err = compat_copy_struct(VAL_FRAME_INFO_TYPE, COPY_TO_USER,
					(void *)data32, (void *)data);

		if (err)
			return err;
	}

	default:
		return vcodec_unlocked_ioctl(file, cmd, arg);
	}
	return 0;
}
#else
#define vcodec_unlocked_compat_ioctl NULL
#endif
static int vcodec_open(struct inode *inode, struct file *file)
{
	pr_debug("%s\n", __func__);

	mutex_lock(&gDrvInitParams->driverOpenCountLock);
	gDrvInitParams->drvOpenCount++;

	pr_info("%s pid = %d, gDrvInitParams->drvOpenCount %d\n",
			__func__, current->pid, gDrvInitParams->drvOpenCount);
	mutex_unlock(&gDrvInitParams->driverOpenCountLock);

	/* TODO: Check upper limit of concurrent users? */

	return 0;
}

static int vcodec_flush(struct file *file, fl_owner_t id)
{
	pr_debug("%s, curr_tid =%d\n", __func__, current->pid);
	pr_debug("%s pid = %d, gDrvInitParams->drvOpenCount %d\n",
			__func__, current->pid, gDrvInitParams->drvOpenCount);

	return 0;
}

static int vcodec_release(struct inode *inode, struct file *file)
{
	unsigned long ulFlagsLockHW, ulFlagsISR;

	/* dump_stack(); */
	pr_debug("%s, curr_tid =%d\n", __func__, current->pid);
	mutex_lock(&gDrvInitParams->driverOpenCountLock);
	pr_info("%s pid = %d, gDrvInitParams->drvOpenCount %d\n",
			__func__, current->pid, gDrvInitParams->drvOpenCount);
	gDrvInitParams->drvOpenCount--;

	if (gDrvInitParams->drvOpenCount == 0) {
		mutex_lock(&gDrvInitParams->hwLock);
		gDrvInitParams->u4VdecLockThreadId = 0;


		/* check if someone didn't unlockHW */
		if (CodecHWLock.pvHandle != 0) {
			pr_info("err %s %d, type = %d, 0x%lx\n",
				__func__, current->pid,
				CodecHWLock.eDriverType,
				(unsigned long)CodecHWLock.pvHandle);
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
				if (gDfvsFlag == 1) {
					set_vdec_bw(gVCodecDev, 0);
					set_vdec_opp(gVCodecDev, 0);
				}
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
				if (gDfvsFlag == 1) {
					set_venc_bw(gVCodecDev, 0);
					set_venc_opp(gVCodecDev, 0);
				}
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

		CodecHWLock.pvHandle = 0;
		CodecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
		CodecHWLock.rLockedTime.u4Sec = 0;
		CodecHWLock.rLockedTime.u4uSec = 0;
		mutex_unlock(&gDrvInitParams->hwLock);

		mutex_lock(&gDrvInitParams->decEMILock);
		gDrvInitParams->u4DecEMICounter = 0;
		mutex_unlock(&gDrvInitParams->decEMILock);

		mutex_lock(&gDrvInitParams->encEMILock);
		gDrvInitParams->u4EncEMICounter = 0;
		mutex_unlock(&gDrvInitParams->encEMILock);

		mutex_lock(&gDrvInitParams->pwrLock);
		gDrvInitParams->u4PWRCounter = 0;
		mutex_unlock(&gDrvInitParams->pwrLock);
		if (gDfvsFlag == 1) {
			mutex_lock(&VcodecDVFSLock);
			free_hist(&codec_hists, 0);
			mutex_unlock(&VcodecDVFSLock);
		}
		spin_lock_irqsave(&gDrvInitParams->lockDecHWCountLock, ulFlagsLockHW);
		gu4LockDecHWCount = 0;
		spin_unlock_irqrestore(&gDrvInitParams->lockDecHWCountLock, ulFlagsLockHW);

		spin_lock_irqsave(&gDrvInitParams->lockEncHWCountLock, ulFlagsLockHW);
		gu4LockEncHWCount = 0;
		spin_unlock_irqrestore(&gDrvInitParams->lockEncHWCountLock, ulFlagsLockHW);

		spin_lock_irqsave(&gDrvInitParams->decISRCountLock, ulFlagsISR);
		gu4DecISRCount = 0;
		spin_unlock_irqrestore(&gDrvInitParams->decISRCountLock, ulFlagsISR);

		spin_lock_irqsave(&gDrvInitParams->encISRCountLock, ulFlagsISR);
		gu4EncISRCount = 0;
		spin_unlock_irqrestore(&gDrvInitParams->encISRCountLock, ulFlagsISR);
	}
	mutex_unlock(&gDrvInitParams->driverOpenCountLock);

	return 0;
}

void vcodec_vma_open(struct vm_area_struct *vma)
{
	pr_debug("%s, virt %lx, phys %lx\n", __func__, vma->vm_start,
			vma->vm_pgoff << PAGE_SHIFT);
}

void vcodec_vma_close(struct vm_area_struct *vma)
{
	pr_debug("%s, virt %lx, phys %lx\n", __func__, vma->vm_start,
			vma->vm_pgoff << PAGE_SHIFT);
}

static const struct vm_operations_struct vcodec_remap_vm_ops = {
	.open = vcodec_vma_open,
	.close = vcodec_vma_close,
};

static int vcodec_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned int u4I = 0;
	unsigned long length;
	unsigned long pfn;

	length = vma->vm_end - vma->vm_start;
	pfn = vma->vm_pgoff<<PAGE_SHIFT;

	if (((length > VENC_REGION) || (pfn < VENC_BASE) ||
		(pfn > VENC_BASE+VENC_REGION)) &&
		((length > VDEC_REGION) || (pfn < VDEC_BASE_PHY) ||
			(pfn > VDEC_BASE_PHY+VDEC_REGION)) &&
		((length > HW_REGION) || (pfn < HW_BASE) ||
			(pfn > HW_BASE+HW_REGION)) &&
		((length > INFO_REGION) || (pfn < INFO_BASE) ||
			(pfn > INFO_BASE+INFO_REGION))) {
		unsigned long ulAddr, ulSize;

		for (u4I = 0; u4I < VCODEC_INST_NUM_x_10; u4I++) {
			if ((ncache_mem_list[u4I].ulKVA != -1L) &&
				(ncache_mem_list[u4I].ulKPA != -1L)) {
				ulAddr = ncache_mem_list[u4I].ulKPA;
				ulSize = (ncache_mem_list[u4I].ulSize +
						0x1000 - 1) & ~(0x1000 - 1);
				if ((length == ulSize) && (pfn == ulAddr)) {
					pr_debug("[VCODEC] cache idx %d\n",
							u4I);
					break;
				}
			}
		}

		if (u4I == VCODEC_INST_NUM_x_10) {
			pr_info("mmap region error: Len(0x%lx),pfn(0x%lx)",
				 (unsigned long)length, pfn);
			return -EAGAIN;
		}
	}
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	pr_debug("mmap vma->start 0x%lx, vma->end 0x%lx, vma->pgoff 0x%lx\n",
			(unsigned long)vma->vm_start,
			(unsigned long)vma->vm_end,
			(unsigned long)vma->vm_pgoff);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
		vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		return -EAGAIN;
	}

	vma->vm_ops = &vcodec_remap_vm_ops;
	vcodec_vma_open(vma);

	return 0;
}

const struct file_operations vcodec_fops = {
	.owner      = THIS_MODULE,
	.unlocked_ioctl = vcodec_unlocked_ioctl,
	.open       = vcodec_open,
	.flush      = vcodec_flush,
	.release    = vcodec_release,
	.mmap       = vcodec_mmap,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl   = vcodec_unlocked_compat_ioctl,
#endif

};

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

void initDvfsParams(void)
{
#ifdef VCODEC_DVFS_V2
		mutex_lock(&VcodecDVFSLock);
		codec_hists = 0;
		codec_jobs = 0;
		mutex_unlock(&VcodecDVFSLock);
#endif
}
