/*
 * Copyright (C) 2015 MediaTek Inc.
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
#include <asm/uaccess.h>
#include <asm/page.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
/* #include <mach/x_define_irq.h> */
#include <linux/wait.h>
#include <linux/proc_fs.h>
#include <linux/semaphore.h>
#include <dma.h>
#include <linux/delay.h>
/* #include <linux/earlysuspend.h> */
#include "sync_write.h"
/* #include "mach/mt_reg_base.h" */
#ifdef CONFIG_OF
#include <linux/clk.h>
#else
#include "mach/mt_clkmgr.h"
#endif
#include "mt_smi.h"
#ifdef CONFIG_MTK_HIBERNATION
#include <mtk_hibernate_dpm.h>
#endif

#include "videocodec_kernel_driver.h"
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/sizes.h>
#include "val_types_private.h"
#include "hal_types_private.h"
#include "val_api_private.h"
#include "val_log.h"
#include "drv_api.h"

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pm_runtime.h>
#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/uaccess.h>
#include <linux/compat.h>
#endif

#define ENABLE_MMDVFS_VDEC
#ifdef ENABLE_MMDVFS_VDEC
/* <--- MM DVFS related */
#include <mt_smi.h>
#define DROP_PERCENTAGE     45
#define RAISE_PERCENTAGE    80
#define MONITOR_DURATION_MS 4000
#define DVFS_LOW     MMDVFS_VOLTAGE_LOW
#define DVFS_HIGH    MMDVFS_VOLTAGE_HIGH
#define DVFS_DEFAULT MMDVFS_VOLTAGE_HIGH
#define MONITOR_START_MINUS_1	0
#define SW_OVERHEAD_MS 1
static VAL_BOOL_T   gMMDFVFSMonitorStarts = VAL_FALSE;
static VAL_BOOL_T   gFirstDvfsLock = VAL_FALSE;
static VAL_UINT32_T gMMDFVFSMonitorCounts;
static VAL_TIME_T   gMMDFVFSMonitorStartTime;
static VAL_TIME_T   gMMDFVFSLastLockTime;
static VAL_TIME_T   gMMDFVFSMonitorEndTime;
static VAL_UINT32_T gHWLockInterval;
static VAL_INT32_T  gHWLockMaxDuration;
static VAL_BOOL_T bIs4K;

VAL_UINT32_T TimeDiffMs(VAL_TIME_T timeOld, VAL_TIME_T timeNew)
{
/*MFV_LOGE ("@@ timeOld(%d, %d), timeNew(%d, %d)", timeOld.u4Sec, timeOld.u4uSec, timeNew.u4Sec, timeNew.u4uSec); */
	return (((((timeNew.u4Sec - timeOld.u4Sec) * 1000000) + timeNew.u4uSec) - timeOld.u4uSec) / 1000);
}

    /* raise voltage */
void SendDvfsRequest(int level)
{
	int ret = 0;

	if (bIs4K)
		level = 1;

	ret = mmdvfs_set_step(SMI_BWC_SCEN_VP, level);

	if (0 != ret)
		MODULE_MFV_LOGE("OOPS: mmdvfs_set_step error!");

	MODULE_MFV_LOGE("[info]adjust MMDVFS to level %d,is4K %d\n", level, bIs4K);
}

void VdecDvfsBegin(void)
{
	gMMDFVFSMonitorStarts = VAL_TRUE;
	gMMDFVFSMonitorCounts = 0;
	gHWLockInterval = 0;
	gFirstDvfsLock = VAL_TRUE;
	gHWLockMaxDuration = 0;
	MODULE_MFV_LOGE("@@ VdecDvfsBegin");
	/* eVideoGetTimeOfDay(&gMMDFVFSMonitorStartTime, sizeof(VAL_TIME_T)); */
}

VAL_UINT32_T VdecDvfsGetMonitorDuration(void)
{
	eVideoGetTimeOfDay(&gMMDFVFSMonitorEndTime, sizeof(VAL_TIME_T));
	return TimeDiffMs(gMMDFVFSMonitorStartTime, gMMDFVFSMonitorEndTime);
}

void VdecDvfsEnd(int level)
{
	MODULE_MFV_LOGE("VdecDVFS monitor %dms, decoded %d frames, total time %d, max duration %d, target lv %d",
	MONITOR_DURATION_MS, gMMDFVFSMonitorCounts, gHWLockInterval, gHWLockMaxDuration, level);
	gMMDFVFSMonitorStarts = VAL_FALSE;
	gMMDFVFSMonitorCounts = 0;
	gHWLockInterval = 0;
	gHWLockMaxDuration = 0;
}

VAL_UINT32_T VdecDvfsStep(void)
{
	VAL_TIME_T _now;
	VAL_UINT32_T _diff = 0;

	eVideoGetTimeOfDay(&_now, sizeof(VAL_TIME_T));
	_diff = TimeDiffMs(gMMDFVFSLastLockTime, _now);
	if (_diff > gHWLockMaxDuration)
		gHWLockMaxDuration = _diff;
	gHWLockInterval += (_diff + SW_OVERHEAD_MS);
	return _diff;
}
/* ---> */
#endif
#define VDO_HW_WRITE(ptr, data)     mt_reg_sync_writel(data, ptr)
#define VDO_HW_READ(ptr)           (*((volatile unsigned int * const)(ptr)))

#define VCODEC_DEVNAME     "Vcodec"
#define VDECDISP_DEVNAME "VDecDisp"
/*#define VENC_DEVNAME     "Venc"
#define VENCLT_DEVNAME     "Venclt"*/
#define MT8173_VCODEC_DEV_MAJOR_NUMBER 160	/* 189 */
/* #define MT8173_VENC_USE_L2C */

static dev_t vcodec_devno = MKDEV(MT8173_VCODEC_DEV_MAJOR_NUMBER, 0);
static struct cdev *vcodec_cdev;
static struct class *vcodec_class;
static struct device *vcodec_device;

/*static dev_t venc_devno;
static struct cdev *venc_cdev;
static struct class *venc_class;

static dev_t venclt_devno;
static struct cdev *venclt_cdev;
static struct class *venclt_class;*/

static DEFINE_MUTEX(IsOpenedLock);
static DEFINE_MUTEX(PWRLock);
static DEFINE_MUTEX(VdecHWLock);
static DEFINE_MUTEX(VencHWLock);
static DEFINE_MUTEX(EncEMILock);
static DEFINE_MUTEX(L2CLock);
static DEFINE_MUTEX(DecEMILock);
static DEFINE_MUTEX(DriverOpenCountLock);
static DEFINE_MUTEX(DecHWLockEventTimeoutLock);
static DEFINE_MUTEX(EncHWLockEventTimeoutLock);

static DEFINE_MUTEX(VdecPWRLock);
static DEFINE_MUTEX(VencPWRLock);

static DEFINE_SPINLOCK(DecIsrLock);
static DEFINE_SPINLOCK(EncIsrLock);
static DEFINE_SPINLOCK(LockDecHWCountLock);
static DEFINE_SPINLOCK(LockEncHWCountLock);
static DEFINE_SPINLOCK(DecISRCountLock);
static DEFINE_SPINLOCK(EncISRCountLock);


static VAL_EVENT_T DecHWLockEvent;	/* mutex : HWLockEventTimeoutLock */
static VAL_EVENT_T EncHWLockEvent;	/* mutex : HWLockEventTimeoutLock */
static VAL_EVENT_T DecIsrEvent;	/* mutex : HWLockEventTimeoutLock */
static VAL_EVENT_T EncIsrEvent;	/* mutex : HWLockEventTimeoutLock */
static VAL_INT32_T MT8173Driver_Open_Count;	/* mutex : DriverOpenCountLock */
static VAL_UINT32_T gu4PWRCounter;	/* mutex : PWRLock */
static VAL_UINT32_T gu4EncEMICounter;	/* mutex : EncEMILock */
static VAL_UINT32_T gu4DecEMICounter;	/* mutex : DecEMILock */
static VAL_UINT32_T gu4L2CCounter;	/* mutex : L2CLock */
static VAL_BOOL_T bIsOpened = VAL_FALSE;	/* mutex : IsOpenedLock */
static VAL_UINT32_T gu4HwVencIrqStatus;	/* hardware VENC IRQ status (VP8/H264) */

static VAL_UINT32_T gu4VdecPWRCounter;	/* mutex : VdecPWRLock */
static VAL_UINT32_T gu4VencPWRCounter;	/* mutex : VencPWRLock */

static VAL_UINT32_T gLockTimeOutCount;

static VAL_UINT32_T gu4VdecLockThreadId;
/*#define MT8173_VCODEC_DEBUG*/
#ifdef MT8173_VCODEC_DEBUG
#undef VCODEC_DEBUG
#define VCODEC_DEBUG printk
#undef MODULE_MFV_LOGD
#define MODULE_MFV_LOGD  printk
#else
#define VCODEC_DEBUG(...)
#undef MODULE_MFV_LOGD
#define MODULE_MFV_LOGD(...)
#endif

/* VENC physical base address */
#undef VENC_BASE

#ifdef CONFIG_ARCH_MT8163
#define VENC_BASE       0x17002000
#define VENC_LT_BASE    0x19002000
#define VENC_REGION     0x1000
#else
#define VENC_BASE       0x18002000
#define VENC_LT_BASE    0x19002000
#define VENC_REGION     0x1000
#endif

/* VDEC virtual base address */
#define VDEC_BASE_PHY   0x16000000
#define VDEC_REGION     0x29000

#define HW_BASE         0x7FFF000
#define HW_REGION       0x2000

#define INFO_BASE       0x10000000
#define INFO_REGION     0x1000

#if 0
#define VENC_IRQ_STATUS_addr        (VENC_BASE + 0x05C)
#define VENC_IRQ_ACK_addr           (VENC_BASE + 0x060)
#define VENC_MP4_IRQ_ACK_addr       (VENC_BASE + 0x678)
#define VENC_MP4_IRQ_STATUS_addr    (VENC_BASE + 0x67C)
#define VENC_ZERO_COEF_COUNT_addr   (VENC_BASE + 0x688)
#define VENC_BYTE_COUNT_addr        (VENC_BASE + 0x680)
#define VENC_MP4_IRQ_ENABLE_addr    (VENC_BASE + 0x668)

#define VENC_MP4_STATUS_addr        (VENC_BASE + 0x664)
#define VENC_MP4_MVQP_STATUS_addr   (VENC_BASE + 0x6E4)
#endif


#define VENC_IRQ_STATUS_SPS         0x1
#define VENC_IRQ_STATUS_PPS         0x2
#define VENC_IRQ_STATUS_FRM         0x4
#define VENC_IRQ_STATUS_DRAM        0x8
#define VENC_IRQ_STATUS_PAUSE       0x10
#define VENC_IRQ_STATUS_SWITCH      0x20
#define VENC_IRQ_STATUS_VPS         0x80
#define VENC_IRQ_STATUS_DRAM_VP8    0x20
#define VENC_SW_PAUSE                0x0AC
#define VENC_SW_HRST_N               0x0A8


/* #define VENC_PWR_FPGA */
/* Cheng-Jung 20120621 VENC power physical base address (FPGA only, should use API) [ */
#ifdef VENC_PWR_FPGA
#define CLK_CFG_0_addr      0x10000140
#define CLK_CFG_4_addr      0x10000150
#define VENC_PWR_addr       0x10006230
#define VENCSYS_CG_SET_addr 0x15000004

#define PWR_ONS_1_D     3
#define PWR_CKD_1_D     4
#define PWR_ONN_1_D     2
#define PWR_ISO_1_D     1
#define PWR_RST_0_D     0

#define PWR_ON_SEQ_0    ((0x1 << PWR_ONS_1_D) | (0x1 << PWR_CKD_1_D) | (0x1 << PWR_ONN_1_D) \
						| (0x1 << PWR_ISO_1_D) | (0x0 << PWR_RST_0_D))
#define PWR_ON_SEQ_1    ((0x1 << PWR_ONS_1_D) | (0x0 << PWR_CKD_1_D) | (0x1 << PWR_ONN_1_D) \
						| (0x1 << PWR_ISO_1_D) | (0x0 << PWR_RST_0_D))
#define PWR_ON_SEQ_2    ((0x1 << PWR_ONS_1_D) | (0x0 << PWR_CKD_1_D) | (0x1 << PWR_ONN_1_D) \
						| (0x0 << PWR_ISO_1_D) | (0x0 << PWR_RST_0_D))
#define PWR_ON_SEQ_3    ((0x1 << PWR_ONS_1_D) | (0x0 << PWR_CKD_1_D) | (0x1 << PWR_ONN_1_D) \
						| (0x0 << PWR_ISO_1_D) | (0x1 << PWR_RST_0_D))
/* ] */
#endif

VAL_ULONG_T KVA_VENC_IRQ_ACK_ADDR, KVA_VENC_IRQ_STATUS_ADDR, KVA_VENC_BASE;
VAL_ULONG_T KVA_VENC_LT_IRQ_ACK_ADDR, KVA_VENC_LT_IRQ_STATUS_ADDR, KVA_VENC_LT_BASE;
VAL_ULONG_T KVA_VDEC_MISC_BASE, KVA_VDEC_VLD_BASE, KVA_VDEC_BASE, KVA_VDEC_GCON_BASE, KVA_VDEC_MC_BASE;
VAL_UINT32_T VENC_IRQ_ID, VENC_LT_IRQ_ID, VDEC_IRQ_ID;
VAL_ULONG_T KVA_VENC_SW_PAUSE, KVA_VENC_SW_HRST_N;
VAL_ULONG_T KVA_VENC_LT_SW_PAUSE, KVA_VENC_LT_SW_HRST_N;

#ifdef VENC_PWR_FPGA
/* Cheng-Jung 20120621 VENC power physical base address (FPGA only, should use API) [ */
VAL_ULONG_T KVA_VENC_CLK_CFG_0_ADDR, KVA_VENC_CLK_CFG_4_ADDR, KVA_VENC_PWR_ADDR,
KVA_VENCSYS_CG_SET_ADDR;
/* ] */
#endif

/* extern unsigned long pmem_user_v2p_video(unsigned long va); */

#if defined(MT8173_VENC_USE_L2C)
/*extern int config_L2(int option); */
#endif

#ifdef CONFIG_OF
/*static struct clk *clk_venc_lt_clk;*/
/* static struct clk *clk_venc_pwr, *clk_venc_pwr2; */


struct platform_device *pvenc_dev = NULL;
struct platform_device *pvenclt_dev = NULL;

#endif
static int venc_enableIRQ(VAL_HW_LOCK_T *prHWLock);
static int venc_disableIRQ(VAL_HW_LOCK_T *prHWLock);
void vdec_power_on(void)
{
	mutex_lock(&VdecPWRLock);
	gu4VdecPWRCounter++;
	mutex_unlock(&VdecPWRLock);

	/* Central power on */

#ifdef CONFIG_OF
	MODULE_MFV_LOGD("vdec_power_on D+\n");
	mtk_smi_larb_clock_on(1, true);
	MODULE_MFV_LOGD("vdec_power_on D -\n");
#else
	enable_clock(MT_CG_DISP0_SMI_COMMON, "VDEC");
	enable_clock(MT_CG_VDEC0_VDEC, "VDEC");
	enable_clock(MT_CG_VDEC1_LARB, "VDEC");
#ifdef VDEC_USE_L2C
	/* enable_clock(MT_CG_INFRA_L2C_SRAM, "VDEC"); */
#endif
#endif
}

void vdec_power_off(void)
{
	mutex_lock(&VdecPWRLock);
	if (gu4VdecPWRCounter != 0)  {
		gu4VdecPWRCounter--;
		/* Central power off */
#ifdef CONFIG_OF
	MODULE_MFV_LOGD("vdec_power_off D+\n");
	mtk_smi_larb_clock_off(1, true);
	MODULE_MFV_LOGD("vdec_power_off D -\n");
#else
		disable_clock(MT_CG_VDEC0_VDEC, "VDEC");
		disable_clock(MT_CG_VDEC1_LARB, "VDEC");
		disable_clock(MT_CG_DISP0_SMI_COMMON, "VDEC");
#ifdef VDEC_USE_L2C
		/* disable_clock(MT_CG_INFRA_L2C_SRAM, "VDEC"); */
#endif
#endif
	}
	mutex_unlock(&VdecPWRLock);
}

void venc_power_on(void)
{
	mutex_lock(&VencPWRLock);
	gu4VencPWRCounter++;
	mutex_unlock(&VencPWRLock);

#ifdef CONFIG_OF
	MODULE_MFV_LOGD("venc_power_on D+\n");
	mtk_smi_larb_clock_on(3, true);
	#ifndef CONFIG_ARCH_MT8163
	mtk_smi_larb_clock_on(5, true);
	#endif
	/*clk_prepare(clk_venc_clk);
	clk_enable(clk_venc_clk);
	clk_prepare(clk_venc_lt_clk);
	clk_enable(clk_venc_lt_clk);*/
	MODULE_MFV_LOGD("venc_power_on D -\n");
#else
	enable_clock(MT_CG_DISP0_SMI_COMMON, "VENC");
	enable_clock(MT_CG_VENC_VENC, "VENC");
	enable_clock(MT_CG_VENC_LARB, "VENC");
#ifdef MT8173_VENC_USE_L2C
	enable_clock(MT_CG_INFRA_L2C_SRAM, "VENC");
#endif
#endif

}

void venc_power_off(void)
{
	mutex_lock(&VencPWRLock);
	if (gu4VencPWRCounter == 0) {
		MODULE_MFV_LOGD("venc_power_off none +\n");
	} else {
		gu4VencPWRCounter--;
		MODULE_MFV_LOGD("venc_power_off D+\n");
#ifdef CONFIG_OF
		/*clk_disable(clk_venc_clk);
		clk_unprepare(clk_venc_clk);
		clk_disable(clk_venc_lt_clk);
		clk_unprepare(clk_venc_lt_clk);*/

		#ifndef CONFIG_ARCH_MT8163
		mtk_smi_larb_clock_off(5, true);
		#endif
		mtk_smi_larb_clock_off(3, true);
#else
		disable_clock(MT_CG_VENC_VENC, "VENC");
		disable_clock(MT_CG_VENC_LARB, "VENC");
		disable_clock(MT_CG_DISP0_SMI_COMMON, "VENC");
#ifdef MT8173_VENC_USE_L2C
		disable_clock(MT_CG_INFRA_L2C_SRAM, "VENC");
#endif
#endif
		MODULE_MFV_LOGD("venc_power_off D-\n");
	}
	mutex_unlock(&VencPWRLock);
}

void dec_isr(void)
{
	VAL_RESULT_T eValRet;
	VAL_ULONG_T ulFlags, ulFlagsISR, ulFlagsLockHW;

	VAL_UINT32_T u4TempDecISRCount = 0;
	VAL_UINT32_T u4TempLockDecHWCount = 0;
	VAL_UINT32_T u4CgStatus = 0;
	VAL_UINT32_T u4DecDoneStatus = 0;
	VAL_UINT32_T u4Width = 0;
	VAL_UINT32_T u4Hight = 0;

	u4CgStatus = VDO_HW_READ(KVA_VDEC_GCON_BASE);
	if ((u4CgStatus & 0x10) != 0) {
		MODULE_MFV_LOGE("[MFV][ERROR] DEC ISR, VDEC active is not 0x0 (0x%08x)", u4CgStatus);
		return;
	}

	u4DecDoneStatus = VDO_HW_READ(KVA_VDEC_BASE + 0xA4);
	if ((u4DecDoneStatus & (0x1 << 16)) != 0x10000) {
		MODULE_MFV_LOGE("[MFV][ERROR] DEC ISR, Decode done status is not 0x1 (0x%08x)",
			 u4DecDoneStatus);
		return;
	}

	u4Width = VDO_HW_READ(KVA_VDEC_MC_BASE + 10 * 4) * 16;
	u4Hight = VDO_HW_READ(KVA_VDEC_MC_BASE + 11 * 4) * 16;
	bIs4K = ((u4Width * u4Hight) > 1920*1088) ? 1 : 0;
	MODULE_MFV_LOGD("is4K %d, w,h(%d, %d)", bIs4K, u4Width, u4Hight);

	spin_lock_irqsave(&DecISRCountLock, ulFlagsISR);
	gu4DecISRCount++;
	u4TempDecISRCount = gu4DecISRCount;
	spin_unlock_irqrestore(&DecISRCountLock, ulFlagsISR);

	spin_lock_irqsave(&LockDecHWCountLock, ulFlagsLockHW);
	u4TempLockDecHWCount = gu4LockDecHWCount;
	spin_unlock_irqrestore(&LockDecHWCountLock, ulFlagsLockHW);
/*
	if (u4TempDecISRCount != u4TempLockDecHWCount) {
		MODULE_MFV_LOGE("[INFO] Dec ISRCount: 0x%x, LockHWCount:0x%x\n",
			u4TempDecISRCount, u4TempLockDecHWCount);
	}
*/
	/* Clear interrupt */
	VDO_HW_WRITE(KVA_VDEC_MISC_BASE + 41 * 4, VDO_HW_READ(KVA_VDEC_MISC_BASE + 41 * 4) | 0x11);
	VDO_HW_WRITE(KVA_VDEC_MISC_BASE + 41 * 4, VDO_HW_READ(KVA_VDEC_MISC_BASE + 41 * 4) & ~0x10);


	spin_lock_irqsave(&DecIsrLock, ulFlags);
	eValRet = eVideoSetEvent(&DecIsrEvent, sizeof(VAL_EVENT_T));
	if (VAL_RESULT_NO_ERROR != eValRet)
		MODULE_MFV_LOGE("[MFV][ERROR] ISR set DecIsrEvent error\n");

	spin_unlock_irqrestore(&DecIsrLock, ulFlags);

}


void enc_isr(void)
{
	VAL_RESULT_T eValRet;
	VAL_ULONG_T ulFlagsISR, ulFlagsLockHW;

	VAL_UINT32_T u4TempEncISRCount = 0;
	VAL_UINT32_T u4TempLockEncHWCount = 0;
	/* ---------------------- */
	spin_lock_irqsave(&EncISRCountLock, ulFlagsISR);
	gu4EncISRCount++;
	u4TempEncISRCount = gu4EncISRCount;
	spin_unlock_irqrestore(&EncISRCountLock, ulFlagsISR);

	spin_lock_irqsave(&LockEncHWCountLock, ulFlagsLockHW);
	u4TempLockEncHWCount = gu4LockEncHWCount;
	spin_unlock_irqrestore(&LockEncHWCountLock, ulFlagsLockHW);
/*
	if (u4TempEncISRCount != u4TempLockEncHWCount) {
		MODULE_MFV_LOGE("[INFO] Enc ISRCount: 0x%x, LockHWCount:0x%x\n",
		u4TempEncISRCount, u4TempLockEncHWCount);
	}
*/
	if (grVcodecEncHWLock.pvHandle == 0) {
		MODULE_MFV_LOGE("[ERROR] NO one Lock Enc HW, please check!!\n");

		/* Clear all status */
		/* VDO_HW_WRITE(KVA_VENC_MP4_IRQ_ACK_ADDR, 1); */
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PAUSE);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SWITCH);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_DRAM);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SPS);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PPS);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_FRM);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PAUSE);

		/*VP8 IRQ reset */
		#ifndef CONFIG_ARCH_MT8163
		VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PAUSE);
		VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SWITCH);
		VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_DRAM);
		VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SPS);
		VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PPS);
		VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_FRM);
		VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PAUSE);
		VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_DRAM_VP8);
		#endif
		return;
	}

	if (grVcodecEncHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC) {	/*added by bin.liu  hardwire */
		gu4HwVencIrqStatus = VDO_HW_READ(KVA_VENC_IRQ_STATUS_ADDR);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PAUSE)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PAUSE);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SWITCH)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SWITCH);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_DRAM)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_DRAM);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SPS)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SPS);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PPS)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PPS);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_FRM)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_FRM);
	} else if (grVcodecEncHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_ENC) {
		gu4HwVencIrqStatus = VDO_HW_READ(KVA_VENC_LT_IRQ_STATUS_ADDR);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PAUSE)
			VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PAUSE);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SWITCH)
			VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SWITCH);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_DRAM_VP8)
			VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_DRAM_VP8);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_DRAM)
			VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_DRAM);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SPS)
			VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SPS);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PPS)
			VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PPS);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_FRM)
			VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR, VENC_IRQ_STATUS_FRM);
	}
#ifdef CONFIG_MTK_VIDEO_HEVC_SUPPORT
	else if (grVcodecEncHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC) {	/* hardwire */
		/* MODULE_MFV_LOGE("[enc_isr] VAL_DRIVER_TYPE_HEVC_ENC %d!!\n", gu4HwVencIrqStatus); */

		gu4HwVencIrqStatus = VDO_HW_READ(KVA_VENC_IRQ_STATUS_ADDR);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PAUSE)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PAUSE);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SWITCH)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SWITCH);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_DRAM)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_DRAM);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SPS)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SPS);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PPS)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PPS);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_FRM)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_FRM);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_VPS)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_VPS);
	}
#endif
	else
		MODULE_MFV_LOGE("enc_isr Invalid lock holder driver type = %d\n", grVcodecEncHWLock.eDriverType);

	eValRet = eVideoSetEvent(&EncIsrEvent, sizeof(VAL_EVENT_T));
	if (VAL_RESULT_NO_ERROR != eValRet)
		MODULE_MFV_LOGE("[VCODEC][ERROR] ISR set EncIsrEvent error\n");

	MODULE_MFV_LOGE("[MFV] enc_isr ISR set EncIsrEvent done\n");
}

static irqreturn_t video_intr_dlr(int irq, void *dev_id)
{
	dec_isr();
	return IRQ_HANDLED;
}

static irqreturn_t video_intr_dlr2(int irq, void *dev_id)
{
	enc_isr();
	return IRQ_HANDLED;
}

static long vcodec_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	VAL_LONG_T ret;
	VAL_UINT8_T *user_data_addr;
	VAL_RESULT_T eValRet;
	VAL_ULONG_T ulFlags, ulFlagsLockHW;
	VAL_HW_LOCK_T rHWLock;
	VAL_BOOL_T bLockedHW = VAL_FALSE;
	VAL_UINT32_T FirstUseDecHW = 0;
	VAL_UINT32_T FirstUseEncHW = 0;
	VAL_TIME_T rCurTime;
	VAL_UINT32_T u4TimeInterval;
	VAL_ISR_T val_isr;
	VAL_VCODEC_CORE_LOADING_T rTempCoreLoading;
	VAL_VCODEC_CPU_OPP_LIMIT_T rCpuOppLimit;
	VAL_INT32_T temp_nr_cpu_ids;
	VAL_POWER_T rPowerParam;
	VAL_MEMORY_T rTempMem;
#ifdef ENABLE_MMDVFS_VDEC
	VAL_UINT32_T _monitor_duration = 0;
	VAL_UINT32_T _diff = 0;
	VAL_UINT32_T _perc = 0;
#endif
#if 0
	VCODEC_DRV_CMD_QUEUE_T rDrvCmdQueue;
	P_VCODEC_DRV_CMD_T cmd_queue = VAL_NULL;
	VAL_UINT32_T u4Size, uValue, nCount;
#endif

	switch (cmd) {
	case VCODEC_SET_THREAD_ID:
		MODULE_MFV_LOGE("[8173] VCODEC_SET_THREAD_ID [EMPTY] + tid = %d\n", current->pid);

		MODULE_MFV_LOGE("[8173] VCODEC_SET_THREAD_ID [EMPTY] - tid = %d\n", current->pid);
		break;

	case VCODEC_ALLOC_NON_CACHE_BUFFER:
		{
			MODULE_MFV_LOGE("[8173][M4U]! VCODEC_ALLOC_NON_CACHE_BUFFER + tid = %d\n",
				 current->pid);

			user_data_addr = (VAL_UINT8_T *) arg;
			ret = copy_from_user(&rTempMem, user_data_addr, sizeof(VAL_MEMORY_T));
			if (ret) {
				MODULE_MFV_LOGE
				    ("[ERROR] VCODEC_ALLOC_NON_CACHE_BUFFER, copy_from_user failed: %lu\n",
				     ret);
				return -EFAULT;
			}

			rTempMem.u4ReservedSize /*kernel va */  =
			    (VAL_ULONG_T) dma_alloc_coherent(0, rTempMem.u4MemSize,
							     (dma_addr_t *) &rTempMem.pvMemPa,
							     GFP_KERNEL);
			if ((0 == rTempMem.u4ReservedSize) || (0 == rTempMem.pvMemPa)) {
				MODULE_MFV_LOGE
				    ("[ERROR] dma_alloc_coherent fail in VCODEC_ALLOC_NON_CACHE_BUFFER\n");
				return -EFAULT;
			}

			MODULE_MFV_LOGD("kernel va = 0x%lx, kernel pa = 0x%lx, memory size = %lu\n",
				 (VAL_ULONG_T) rTempMem.u4ReservedSize,
				 (VAL_ULONG_T) rTempMem.pvMemPa, (VAL_ULONG_T) rTempMem.u4MemSize);

			/* mutex_lock(&NonCacheMemoryListLock); */
			/* Add_NonCacheMemoryList(rTempMem.u4ReservedSize,
			(VAL_UINT32_T)rTempMem.pvMemPa, (VAL_UINT32_T)rTempMem.u4MemSize, 0, 0); */
			/* mutex_unlock(&NonCacheMemoryListLock); */

			ret = copy_to_user(user_data_addr, &rTempMem, sizeof(VAL_MEMORY_T));
			if (ret) {
				MODULE_MFV_LOGE
				    ("[ERROR] VCODEC_ALLOC_NON_CACHE_BUFFER, copy_to_user failed: %lu\n",
				     ret);
				return -EFAULT;
			}

			MODULE_MFV_LOGE("[8173][M4U]! VCODEC_ALLOC_NON_CACHE_BUFFER - tid = %d\n",
				 current->pid);
		}
		break;

	case VCODEC_FREE_NON_CACHE_BUFFER:
		{
			MODULE_MFV_LOGE("[8173][M4U]! VCODEC_FREE_NON_CACHE_BUFFER + tid = %d\n",
				 current->pid);

			user_data_addr = (VAL_UINT8_T *) arg;
			ret = copy_from_user(&rTempMem, user_data_addr, sizeof(VAL_MEMORY_T));
			if (ret) {
				MODULE_MFV_LOGE
				    ("[ERROR] VCODEC_FREE_NON_CACHE_BUFFER, copy_from_user failed: %lu\n",
				     ret);
				return -EFAULT;
			}

			dma_free_coherent(0, rTempMem.u4MemSize, (void *)rTempMem.u4ReservedSize,
					  (dma_addr_t) rTempMem.pvMemPa);

			/* mutex_lock(&NonCacheMemoryListLock); */
			/* Free_NonCacheMemoryList(rTempMem.u4ReservedSize, (VAL_UINT32_T)rTempMem.pvMemPa); */
			/* mutex_unlock(&NonCacheMemoryListLock); */

			rTempMem.u4ReservedSize = 0;
			rTempMem.pvMemPa = NULL;

			ret = copy_to_user(user_data_addr, &rTempMem, sizeof(VAL_MEMORY_T));
			if (ret) {
				MODULE_MFV_LOGE
				    ("[ERROR] VCODEC_FREE_NON_CACHE_BUFFER, copy_to_user failed: %lu\n",
				     ret);
				return -EFAULT;
			}

			MODULE_MFV_LOGE("[8173][M4U]! VCODEC_FREE_NON_CACHE_BUFFER - tid = %d\n",
				 current->pid);
		}
		break;

	case VCODEC_INC_DEC_EMI_USER:
		MODULE_MFV_LOGD("[8173] VCODEC_INC_DEC_EMI_USER + tid = %d\n", current->pid);

		mutex_lock(&DecEMILock);
		gu4DecEMICounter++;
		MODULE_MFV_LOGE("DEC_EMI_USER = %d\n", gu4DecEMICounter);
		user_data_addr = (VAL_UINT8_T *) arg;
		ret = copy_to_user(user_data_addr, &gu4DecEMICounter, sizeof(VAL_UINT32_T));
		if (ret) {
			MODULE_MFV_LOGE("[ERROR] VCODEC_INC_DEC_EMI_USER, copy_to_user failed: %lu\n",
				 ret);
			mutex_unlock(&DecEMILock);
			return -EFAULT;
		}
		mutex_unlock(&DecEMILock);

		#ifdef ENABLE_MMDVFS_VDEC
		/* MM DVFS related */
		MODULE_MFV_LOGE("@@ INC_DEC_EMI MM DVFS init");
		/* raise voltage */
		SendDvfsRequest(DVFS_DEFAULT);
		VdecDvfsBegin();
		#endif
		MODULE_MFV_LOGD("[8173] VCODEC_INC_DEC_EMI_USER - tid = %d\n", current->pid);
		break;

	case VCODEC_DEC_DEC_EMI_USER:
		MODULE_MFV_LOGD("[8173] VCODEC_DEC_DEC_EMI_USER + tid = %d\n", current->pid);

		mutex_lock(&DecEMILock);
		gu4DecEMICounter--;
		MODULE_MFV_LOGE("DEC_EMI_USER = %d\n", gu4DecEMICounter);
		user_data_addr = (VAL_UINT8_T *) arg;
		ret = copy_to_user(user_data_addr, &gu4DecEMICounter, sizeof(VAL_UINT32_T));
		if (ret) {
			MODULE_MFV_LOGE("[ERROR] VCODEC_DEC_DEC_EMI_USER, copy_to_user failed: %lu\n",
				 ret);
			mutex_unlock(&DecEMILock);
			return -EFAULT;
		}
		mutex_unlock(&DecEMILock);

		MODULE_MFV_LOGD("[8173] VCODEC_DEC_DEC_EMI_USER - tid = %d\n", current->pid);
		break;

	case VCODEC_INC_ENC_EMI_USER:
		MODULE_MFV_LOGD("[8173] VCODEC_INC_ENC_EMI_USER + tid = %d\n", current->pid);

		mutex_lock(&EncEMILock);
		gu4EncEMICounter++;
		MODULE_MFV_LOGE("ENC_EMI_USER = %d\n", gu4EncEMICounter);
		user_data_addr = (VAL_UINT8_T *) arg;
		ret = copy_to_user(user_data_addr, &gu4EncEMICounter, sizeof(VAL_UINT32_T));
		if (ret) {
			MODULE_MFV_LOGE("[ERROR] VCODEC_INC_ENC_EMI_USER, copy_to_user failed: %lu\n",
				 ret);
			mutex_unlock(&EncEMILock);
			return -EFAULT;
		}
		mutex_unlock(&EncEMILock);

		MODULE_MFV_LOGD("[8173] VCODEC_INC_ENC_EMI_USER - tid = %d\n", current->pid);
		break;

	case VCODEC_DEC_ENC_EMI_USER:
		{
			MODULE_MFV_LOGD("[8173] VCODEC_DEC_ENC_EMI_USER + tid = %d\n", current->pid);

			mutex_lock(&EncEMILock);
			gu4EncEMICounter--;
			MODULE_MFV_LOGE("ENC_EMI_USER = %d\n", gu4EncEMICounter);
			user_data_addr = (VAL_UINT8_T *) arg;
			ret = copy_to_user(user_data_addr, &gu4EncEMICounter, sizeof(VAL_UINT32_T));
			if (ret) {
				MODULE_MFV_LOGE
				    ("[ERROR] VCODEC_DEC_ENC_EMI_USER, copy_to_user failed: %lu\n",
				     ret);
				mutex_unlock(&EncEMILock);
				return -EFAULT;
			}
			mutex_unlock(&EncEMILock);

			MODULE_MFV_LOGD("[8173] VCODEC_DEC_ENC_EMI_USER - tid = %d\n", current->pid);
		}
		break;

	case VCODEC_LOCKHW:
		{
			MODULE_MFV_LOGD("[8173] VCODEC_LOCKHW + tid = %d\n", current->pid);
			user_data_addr = (VAL_UINT8_T *) arg;
			ret = copy_from_user(&rHWLock, user_data_addr, sizeof(VAL_HW_LOCK_T));
			if (ret) {
				MODULE_MFV_LOGE("[ERROR] VCODEC_LOCKHW, copy_from_user failed: %lu\n",
					 ret);
				return -EFAULT;
			}

			MODULE_MFV_LOGD("LOCKHW eDriverType = %d\n", rHWLock.eDriverType);
			eValRet = VAL_RESULT_INVALID_ISR;
			if (rHWLock.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
#ifdef CONFIG_MTK_VIDEO_HEVC_SUPPORT
			    rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
#endif
			    rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
			    rHWLock.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC ||
			    rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_DEC ||
			    rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_ADV_DEC ||
			    rHWLock.eDriverType == VAL_DRIVER_TYPE_VP9_DEC ||
			    rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_DEC) {
				while (bLockedHW == VAL_FALSE) {
					mutex_lock(&DecHWLockEventTimeoutLock);
					if (DecHWLockEvent.u4TimeoutMs == 1) {
						MODULE_MFV_LOGE
						    ("[NOT ERROR][VCODEC_LOCKHW] First Use Dec HW!!\n");
						FirstUseDecHW = 1;
					} else {
						FirstUseDecHW = 0;
					}
					mutex_unlock(&DecHWLockEventTimeoutLock);

					if (FirstUseDecHW == 1) {
						eValRet =
						    eVideoWaitEvent(&DecHWLockEvent,
								    sizeof(VAL_EVENT_T));
					}
					mutex_lock(&DecHWLockEventTimeoutLock);
					if (DecHWLockEvent.u4TimeoutMs != 1000) {
						DecHWLockEvent.u4TimeoutMs = 1000;
						FirstUseDecHW = 1;
					} else {
						FirstUseDecHW = 0;
					}
					mutex_unlock(&DecHWLockEventTimeoutLock);

					mutex_lock(&VdecHWLock);
					/* one process try to lock twice */
					if (grVcodecDecHWLock.pvHandle ==
					    (VAL_VOID_T *) pmem_user_v2p_video((VAL_ULONG_T)
									       rHWLock.pvHandle)) {
						MODULE_MFV_LOGE
						    ("[WARNING] one decoder instance try to lock twice, may cause lock HW timeout!! instance = 0x%lx, CurrentTID = %d\n",
						     (VAL_ULONG_T) grVcodecDecHWLock.pvHandle,
						     current->pid);
					}
					mutex_unlock(&VdecHWLock);

					if (FirstUseDecHW == 0) {
						MODULE_MFV_LOGD("Not first time use HW, timeout = %d\n",
							 DecHWLockEvent.u4TimeoutMs);
						eValRet =
						    eVideoWaitEvent(&DecHWLockEvent,
								    sizeof(VAL_EVENT_T));
					}

					if (VAL_RESULT_INVALID_ISR == eValRet) {
						MODULE_MFV_LOGE
						    ("[ERROR][VCODEC_LOCKHW] DecHWLockEvent TimeOut, CurrentTID = %d\n",
						     current->pid);
						if (FirstUseDecHW != 1) {
							mutex_lock(&VdecHWLock);
							if (grVcodecDecHWLock.pvHandle == 0)
								MODULE_MFV_LOGE
								    ("[WARNING] maybe mediaserver restart before, please check!!\n");
							else
								MODULE_MFV_LOGE
								    ("[WARNING] someone use HW, and check timeout value!!\n");
							mutex_unlock(&VdecHWLock);
						}
					} else if (VAL_RESULT_RESTARTSYS == eValRet) {
						MODULE_MFV_LOGE
						    ("[WARNING] VAL_RESULT_RESTARTSYS return when HWLock!!\n");
						return -ERESTARTSYS;
					}

					mutex_lock(&VdecHWLock);
					if (grVcodecDecHWLock.pvHandle == 0) {	/* No one holds dec hw lock now */
						gu4VdecLockThreadId = current->pid;
						grVcodecDecHWLock.pvHandle =
						    (VAL_VOID_T *) pmem_user_v2p_video((VAL_ULONG_T)
										       rHWLock.
										       pvHandle);
						grVcodecDecHWLock.eDriverType = rHWLock.eDriverType;
						eVideoGetTimeOfDay(&grVcodecDecHWLock.rLockedTime,
								   sizeof(VAL_TIME_T));

						MODULE_MFV_LOGD
						("No process use dec HW, so current process can use HW\n");
						MODULE_MFV_LOGD
						("LockInstance = 0x%lx CurrentTID = %d, rLockedTime(s, us) = %d, %d\n",
						(VAL_ULONG_T) grVcodecDecHWLock.pvHandle,
						     current->pid,
						     grVcodecDecHWLock.rLockedTime.u4Sec,
						     grVcodecDecHWLock.rLockedTime.u4uSec);

						bLockedHW = VAL_TRUE;
						if (VAL_RESULT_INVALID_ISR == eValRet
						    && FirstUseDecHW != 1) {
							MODULE_MFV_LOGE
							    ("[WARNING] reset power/irq when HWLock!!\n");
							vdec_power_off();
							disable_irq(VDEC_IRQ_ID);
						}
						vdec_power_on();
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT	/* Morris Yang moved to TEE */
						if (rHWLock.bSecureInst == VAL_FALSE) {
							if (request_irq
							    (VDEC_IRQ_ID,
							     (irq_handler_t) video_intr_dlr,
							     IRQF_TRIGGER_LOW, VCODEC_DEVNAME,
							     NULL) < 0)
								MODULE_MFV_LOGE
								    ("[VCODEC_DEBUG][ERROR] error to request dec irq\n");
							else
								MODULE_MFV_LOGD
								    ("[VCODEC_DEBUG] success to request dec irq\n");
							/* enable_irq(VDEC_IRQ_ID); */
						}
#else
						enable_irq(VDEC_IRQ_ID);
#endif
#ifdef ENABLE_MMDVFS_VDEC
			/* MM DVFS related */
			if (VAL_FALSE == gMMDFVFSMonitorStarts) {
				/* Continuous monitoring */
				VdecDvfsBegin();
			}
			if (VAL_TRUE == gMMDFVFSMonitorStarts) {
				MODULE_MFV_LOGD("@@ LOCK 1");
				if (gMMDFVFSMonitorCounts > MONITOR_START_MINUS_1) {
					if (VAL_TRUE == gFirstDvfsLock) {
						gFirstDvfsLock = VAL_FALSE;
						MODULE_MFV_LOGE("@@ LOCK 1 start monitor");
						eVideoGetTimeOfDay(&gMMDFVFSMonitorStartTime, sizeof(VAL_TIME_T));
					}
					eVideoGetTimeOfDay(&gMMDFVFSLastLockTime, sizeof(VAL_TIME_T));
				}
			}
#endif
					} else {	/* Another one holding dec hw now */

						MODULE_MFV_LOGE("[NOT ERROR][VCODEC_LOCKHW] E\n");
						eVideoGetTimeOfDay(&rCurTime, sizeof(VAL_TIME_T));
						u4TimeInterval =
						    (((((rCurTime.u4Sec -
							 grVcodecDecHWLock.rLockedTime.u4Sec) *
							1000000) + rCurTime.u4uSec)
						      -
						      grVcodecDecHWLock.rLockedTime.u4uSec) / 1000);

						MODULE_MFV_LOGD
						    ("someone use dec HW, and check timeout value\n");
						MODULE_MFV_LOGD
						    ("Instance = 0x%lx CurrentTID = %d, TimeInterval(ms) = %d, TimeOutValue(ms)) = %d\n",
						     (VAL_ULONG_T) grVcodecDecHWLock.pvHandle,
						     current->pid, u4TimeInterval,
						     rHWLock.u4TimeoutMs);

						MODULE_MFV_LOGE
						    ("[VCODEC_LOCKHW] Lock Instance = 0x%lx, Lock TID = %d, CurrentTID = %d, rLockedTime(%d s, %d us), rCurTime(%d s, %d us)\n",
						     (VAL_ULONG_T) grVcodecDecHWLock.pvHandle,
						     gu4VdecLockThreadId, current->pid,
						     grVcodecDecHWLock.rLockedTime.u4Sec,
						     grVcodecDecHWLock.rLockedTime.u4uSec,
						     rCurTime.u4Sec, rCurTime.u4uSec);

						/* 2012/12/16. Cheng-Jung Never steal hardware lock */
						if (0)
							/* if (u4TimeInterval >= rHWLock.u4TimeoutMs) */
						{
							grVcodecDecHWLock.pvHandle =
							    (VAL_VOID_T *)
							    pmem_user_v2p_video((VAL_ULONG_T)
										rHWLock.pvHandle);
							grVcodecDecHWLock.eDriverType =
							    rHWLock.eDriverType;
							eVideoGetTimeOfDay(&grVcodecDecHWLock.
									   rLockedTime,
									   sizeof(VAL_TIME_T));
							bLockedHW = VAL_TRUE;
							vdec_power_on();
							/* TODO: Error handling, VDEC break, reset? */
						}
					}
					mutex_unlock(&VdecHWLock);
					spin_lock_irqsave(&LockDecHWCountLock, ulFlagsLockHW);
					gu4LockDecHWCount++;
					spin_unlock_irqrestore(&LockDecHWCountLock, ulFlagsLockHW);
				}
			} else if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
				   rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC ||
				   rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_ENC ||
				   rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC) {
				while (bLockedHW == VAL_FALSE) {
					/* Early break for JPEG VENC */
					if (rHWLock.u4TimeoutMs == 0) {
						if (grVcodecEncHWLock.pvHandle != 0)
							break;
					}
					/* Wait to acquire Enc HW lock */
					mutex_lock(&EncHWLockEventTimeoutLock);
					if (EncHWLockEvent.u4TimeoutMs == 1) {
						MODULE_MFV_LOGE("[NOT ERROR][VCODEC_LOCKHW] ENC First Use HW %d!!\n", rHWLock.eDriverType);
						FirstUseEncHW = 1;
					} else {
						FirstUseEncHW = 0;
					}
					mutex_unlock(&EncHWLockEventTimeoutLock);
					if (FirstUseEncHW == 1) {
						eValRet =
						    eVideoWaitEvent(&EncHWLockEvent,
								    sizeof(VAL_EVENT_T));
					}

					mutex_lock(&EncHWLockEventTimeoutLock);
					if (EncHWLockEvent.u4TimeoutMs == 1) {
						EncHWLockEvent.u4TimeoutMs = 1000;
						FirstUseEncHW = 1;
					} else {
						FirstUseEncHW = 0;
						if (rHWLock.u4TimeoutMs == 0)
							EncHWLockEvent.u4TimeoutMs = 0;	/* No wait */
					    else
							EncHWLockEvent.u4TimeoutMs = 1000;	/* Wait indefinitely */
					}
					mutex_unlock(&EncHWLockEventTimeoutLock);

					mutex_lock(&VencHWLock);
					/* one process try to lock twice */
					if (grVcodecEncHWLock.pvHandle ==
					    (VAL_VOID_T *) pmem_user_v2p_video((VAL_ULONG_T)
									       rHWLock.pvHandle)) {
						MODULE_MFV_LOGE
						    ("[VCODEC_LOCKHW] [WARNING] one ENC instance try to lock twice, may cause lock HW timeout!! instance = 0x%lx, CurrentTID = %d, type:%d\n",
						     (VAL_ULONG_T) grVcodecEncHWLock.pvHandle,
						     current->pid, rHWLock.eDriverType);
					}
					mutex_unlock(&VencHWLock);

					if (FirstUseEncHW == 0) {
						eValRet =
						    eVideoWaitEvent(&EncHWLockEvent,
								    sizeof(VAL_EVENT_T));
					}

					if (VAL_RESULT_INVALID_ISR == eValRet) {
						MODULE_MFV_LOGE
						    ("[VCODEC_LOCKHW][ERROR][VCODEC_LOCKHW] EncHWLockEvent TimeOut, CurrentTID = %d\n",
						     current->pid);
						if (FirstUseEncHW != 1) {
							mutex_lock(&VencHWLock);
							if (grVcodecEncHWLock.pvHandle == 0) {
								MODULE_MFV_LOGE
								    ("[VCODEC_LOCKHW] [WARNING] ENC maybe mediaserver restart before, please check!!\n");
							} else {
								MODULE_MFV_LOGE
								    ("[VCODEC_LOCKHW] [WARNING] ENC someone use HW, and check timeout value!! %d\n",
								     gLockTimeOutCount);
								++gLockTimeOutCount;
								if (gLockTimeOutCount > 30) {
									MODULE_MFV_LOGE("[ERROR] VCODEC_LOCKHW - ID %d  fail, someone locked HW time out more than 30 times 0x%lx, %lx, 0x%lx, type:%d\n",
									     current->pid,
									     (VAL_ULONG_T)
									     grVcodecEncHWLock.
									     pvHandle,
									     pmem_user_v2p_video((VAL_ULONG_T) rHWLock.pvHandle), (VAL_ULONG_T) rHWLock.pvHandle, rHWLock.eDriverType);
									gLockTimeOutCount = 0;
									mutex_unlock(&VencHWLock);
									return -EFAULT;
								}

								if (rHWLock.u4TimeoutMs == 0) {
									MODULE_MFV_LOGE
									    ("[VCODEC_LOCKHW] ENC - ID %d  fail, someone locked HW already 0x%lx, %lx, 0x%lx, type:%d\n",
									     current->pid,
									     (VAL_ULONG_T)
									     grVcodecEncHWLock.
									     pvHandle,
									     pmem_user_v2p_video((VAL_ULONG_T) rHWLock.pvHandle), (VAL_ULONG_T) rHWLock.pvHandle, rHWLock.eDriverType);
									gLockTimeOutCount = 0;
									mutex_unlock(&VencHWLock);
									return -EFAULT;
								}
							}
							mutex_unlock(&VencHWLock);
						}
					} else if (VAL_RESULT_RESTARTSYS == eValRet) {
						return -ERESTARTSYS;
					}

					mutex_lock(&VencHWLock);
					if (grVcodecEncHWLock.pvHandle == 0) {	/* No process use HW, so current process can use HW */
						if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC
						    || rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_ENC ||
#ifdef CONFIG_MTK_VIDEO_HEVC_SUPPORT
						    rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC
						    ||
#endif
						    rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC) {
							grVcodecEncHWLock.pvHandle =
							    (VAL_VOID_T *)
							    pmem_user_v2p_video((VAL_ULONG_T)
										rHWLock.pvHandle);
							MODULE_MFV_LOGD
							    ("[VCODEC_LOCKHW] ENC No process use HW, so current process can use HW, handle = 0x%lx\n",
							     (VAL_ULONG_T) grVcodecEncHWLock.
							     pvHandle);
							grVcodecEncHWLock.eDriverType =
							    rHWLock.eDriverType;
							eVideoGetTimeOfDay(&grVcodecEncHWLock.
									   rLockedTime,
									   sizeof(VAL_TIME_T));

							MODULE_MFV_LOGD
							    ("[VCODEC_LOCKHW] ENC No process use HW, so current process can use HW\n");
							MODULE_MFV_LOGD
							    ("[VCODEC_LOCKHW] ENC LockInstance = 0x%lx CurrentTID = %d, rLockedTime(s, us) = %d, %d\n",
							     (VAL_ULONG_T) grVcodecEncHWLock.
							     pvHandle, current->pid,
							     grVcodecEncHWLock.rLockedTime.u4Sec,
							     grVcodecEncHWLock.rLockedTime.u4uSec);

							bLockedHW = VAL_TRUE;
							venc_enableIRQ(&rHWLock);
						}
					} else	/* someone use HW, and check timeout value */{
						if (rHWLock.u4TimeoutMs == 0) {
							bLockedHW = VAL_FALSE;
							mutex_unlock(&VencHWLock);
							break;
						}

						eVideoGetTimeOfDay(&rCurTime, sizeof(VAL_TIME_T));
						u4TimeInterval =
						    (((((rCurTime.u4Sec -
							 grVcodecEncHWLock.rLockedTime.u4Sec) *
							1000000) + rCurTime.u4uSec)
						      -
						      grVcodecEncHWLock.rLockedTime.u4uSec) / 1000);

						MODULE_MFV_LOGD
						    ("[VCODEC_LOCKHW] ENC someone use HW, and check timeout value\n");
						MODULE_MFV_LOGD
						    ("[VCODEC_LOCKHW] ENC LockInstance = 0x%lx, CurrentInstance = 0x%lx, CurrentTID = %d, TimeInterval(ms) = %d, TimeOutValue(ms)) = %d\n",
						     (VAL_ULONG_T) grVcodecEncHWLock.pvHandle,
						     pmem_user_v2p_video((VAL_ULONG_T) rHWLock.
									 pvHandle), current->pid,
						     u4TimeInterval, rHWLock.u4TimeoutMs);

						MODULE_MFV_LOGD
						    ("[VCODEC_LOCKHW] ENC LockInstance = 0x%lx, CurrentInstance = 0x%lx, CurrentTID = %d, rLockedTime(s, us) = %d, %d, rCurTime(s, us) = %d, %d\n",
						     (VAL_ULONG_T) grVcodecEncHWLock.pvHandle,
						     pmem_user_v2p_video((VAL_ULONG_T) rHWLock.
									 pvHandle), current->pid,
						     grVcodecEncHWLock.rLockedTime.u4Sec,
						     grVcodecEncHWLock.rLockedTime.u4uSec,
						     rCurTime.u4Sec, rCurTime.u4uSec);

						++gLockTimeOutCount;
						if (gLockTimeOutCount > 30) {
							MODULE_MFV_LOGE
							    ("[VCODEC_LOCKHW] ENC - ID %d  fail, someone locked HW over 30 times without timeout 0x%lx, %lx, 0x%lx, type:%d\n",
							     current->pid,
							     (VAL_ULONG_T) grVcodecEncHWLock.
							     pvHandle,
							     pmem_user_v2p_video((VAL_ULONG_T)
										 rHWLock.pvHandle),
							     (VAL_ULONG_T) rHWLock.pvHandle,
							     rHWLock.eDriverType);
							gLockTimeOutCount = 0;
							mutex_unlock(&VencHWLock);
							return -EFAULT;
						}
						/* 2013/04/10. Cheng-Jung Never steal hardware lock */
					}

					if (VAL_TRUE == bLockedHW) {
						MODULE_MFV_LOGD
						    ("[VCODEC_LOCKHW] ENC Lock ok grVcodecEncHWLock.pvHandle = 0x%lx, va:%lx, type:%d",
						     (VAL_ULONG_T) grVcodecEncHWLock.pvHandle,
						     (VAL_ULONG_T) rHWLock.pvHandle,
						     rHWLock.eDriverType);
						gLockTimeOutCount = 0;
					}
					mutex_unlock(&VencHWLock);
				}

				if (VAL_FALSE == bLockedHW) {
					MODULE_MFV_LOGE
					    ("[VCODEC_LOCKHW] ENC - ID %d  fail, someone locked HW already , 0x%lx, %lx, 0x%lx, type:%d\n",
					     current->pid, (VAL_ULONG_T) grVcodecEncHWLock.pvHandle,
					     pmem_user_v2p_video((VAL_ULONG_T) rHWLock.pvHandle),
					     (VAL_ULONG_T) rHWLock.pvHandle, rHWLock.eDriverType);
					gLockTimeOutCount = 0;
					return -EFAULT;
				}

				spin_lock_irqsave(&LockEncHWCountLock, ulFlagsLockHW);
				gu4LockEncHWCount++;
				spin_unlock_irqrestore(&LockEncHWCountLock, ulFlagsLockHW);

				MODULE_MFV_LOGD("[VCODEC_LOCKHW] ENC get locked - ObjId =%d\n", current->pid);

				MODULE_MFV_LOGD("[VCODEC_LOCKHW] ENC - tid = %d\n", current->pid);
			} else {
				MODULE_MFV_LOGE("[VCODEC_LOCKHW] [WARNING] Unknown instance\n");
				return -EFAULT;
			}
			MODULE_MFV_LOGD("[VCODEC_LOCKHW] - tid = %d\n", current->pid);
		}
		break;

	case VCODEC_UNLOCKHW:
		{
			MODULE_MFV_LOGD("[8173] VCODEC_UNLOCKHW + tid = %d\n", current->pid);
			user_data_addr = (VAL_UINT8_T *) arg;
			ret = copy_from_user(&rHWLock, user_data_addr, sizeof(VAL_HW_LOCK_T));
			if (ret) {
				MODULE_MFV_LOGE("[ERROR] VCODEC_UNLOCKHW, copy_from_user failed: %lu\n",
					 ret);
				return -EFAULT;
			}

			MODULE_MFV_LOGD("UNLOCKHW eDriverType = %d\n", rHWLock.eDriverType);
			eValRet = VAL_RESULT_INVALID_ISR;
			if (rHWLock.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
#ifdef CONFIG_MTK_VIDEO_HEVC_SUPPORT
			    rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
#endif
			    rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
			    rHWLock.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC ||
			    rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_DEC ||
			    rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_ADV_DEC ||
			    rHWLock.eDriverType == VAL_DRIVER_TYPE_VP9_DEC ||
			    rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_DEC) {
				mutex_lock(&VdecHWLock);
				if (grVcodecDecHWLock.pvHandle ==
					(VAL_VOID_T *) pmem_user_v2p_video((VAL_ULONG_T) rHWLock.pvHandle)) {
					grVcodecDecHWLock.pvHandle = 0;
					grVcodecDecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT	/* Morris Yang moved to TEE */
					if (rHWLock.bSecureInst == VAL_FALSE) {
						/* disable_irq(VDEC_IRQ_ID); */

						free_irq(VDEC_IRQ_ID, NULL);
					}
#else
					disable_irq(VDEC_IRQ_ID);
#endif
					/* TODO: check if turning power off is ok */
					vdec_power_off();
#ifdef ENABLE_MMDVFS_VDEC
			/* MM DVFS related */
			if (VAL_TRUE == gMMDFVFSMonitorStarts
				&& gMMDFVFSMonitorCounts > MONITOR_START_MINUS_1) {
				_monitor_duration = VdecDvfsGetMonitorDuration();
				if (_monitor_duration < MONITOR_DURATION_MS) {
					_diff = VdecDvfsStep();
					MODULE_MFV_LOGD("@@ UNLOCK - lock time(%d ms, %d ms), cnt=%d, _monitor_duration=%d",
					_diff, gHWLockInterval, gMMDFVFSMonitorCounts, _monitor_duration);
				} else {
					VdecDvfsStep();
					_perc = (VAL_UINT32_T)(100 * gHWLockInterval / _monitor_duration);
					MODULE_MFV_LOGD("@@ UNLOCK - reset monitor duration (%d ms), percent: %d",
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
#endif
				} else {	/* Not current owner */

					MODULE_MFV_LOGD
					    ("[ERROR] Not owner trying to unlock dec hardware 0x%lx\n",
					     pmem_user_v2p_video((VAL_ULONG_T) rHWLock.pvHandle));
					mutex_unlock(&VdecHWLock);
					return -EFAULT;
				}
				mutex_unlock(&VdecHWLock);
				eValRet = eVideoSetEvent(&DecHWLockEvent, sizeof(VAL_EVENT_T));
			} else if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
				   rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_ENC ||
#ifdef CONFIG_MTK_VIDEO_HEVC_SUPPORT
				   rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC ||
#endif
				   rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC) {
				mutex_lock(&VencHWLock);
				if (grVcodecEncHWLock.pvHandle == (VAL_VOID_T *) pmem_user_v2p_video((VAL_ULONG_T) rHWLock.pvHandle)) {
					grVcodecEncHWLock.pvHandle = 0;
					grVcodecEncHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
					venc_disableIRQ(&rHWLock);
				} else {	/* Not current owner */

					/* [TODO] error handling */
					MODULE_MFV_LOGE
					    ("[ERROR] Not owner trying to unlock enc hardware 0x%lx, pa:%lx, va:%lx type:%d\n",
					     (VAL_ULONG_T) grVcodecEncHWLock.pvHandle,
					     pmem_user_v2p_video((VAL_ULONG_T) rHWLock.pvHandle),
					     (VAL_ULONG_T) rHWLock.pvHandle, rHWLock.eDriverType);
					mutex_unlock(&VencHWLock);
					return -EFAULT;
				}
				mutex_unlock(&VencHWLock);
				eValRet = eVideoSetEvent(&EncHWLockEvent, sizeof(VAL_EVENT_T));
			} else {
				MODULE_MFV_LOGE("[WARNING] VCODEC_UNLOCKHW Unknown instance\n");
				return -EFAULT;
			}
			MODULE_MFV_LOGD("[8173] VCODEC_UNLOCKHW - tid = %d\n", current->pid);
		}
		break;

	case VCODEC_INC_PWR_USER:
		{
			MODULE_MFV_LOGD("[8173] VCODEC_INC_PWR_USER + tid = %d\n", current->pid);
			user_data_addr = (VAL_UINT8_T *) arg;
			ret = copy_from_user(&rPowerParam, user_data_addr, sizeof(VAL_POWER_T));
			if (ret) {
				MODULE_MFV_LOGE
				    ("[ERROR] VCODEC_INC_PWR_USER, copy_from_user failed: %lu\n",
				     ret);
				return -EFAULT;
			}
			MODULE_MFV_LOGD("INC_PWR_USER eDriverType = %d\n", rPowerParam.eDriverType);
			mutex_lock(&L2CLock);

#ifdef MT8173_VENC_USE_L2C
			if (rPowerParam.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
			    val_isr.eDriverType == VAL_DRIVER_TYPE_VP8_ENC ||
			    val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC) {
				gu4L2CCounter++;
				MODULE_MFV_LOGD("INC_PWR_USER L2C counter = %d\n", gu4L2CCounter);

				if (1 == gu4L2CCounter) {
					if (config_L2(0)) {
						MODULE_MFV_LOGE
						    ("[MFV][ERROR] Switch L2C size to 512K failed\n");
						mutex_unlock(&L2CLock);
						return -EFAULT;
					}
				}
			}
#endif
			mutex_unlock(&L2CLock);
			MODULE_MFV_LOGD("[8173] VCODEC_INC_PWR_USER - tid = %d\n", current->pid);
		}
		break;

	case VCODEC_DEC_PWR_USER:
		{
			MODULE_MFV_LOGD("[8173] VCODEC_DEC_PWR_USER + tid = %d\n", current->pid);
			user_data_addr = (VAL_UINT8_T *) arg;
			ret = copy_from_user(&rPowerParam, user_data_addr, sizeof(VAL_POWER_T));
			if (ret) {
				MODULE_MFV_LOGE
				    ("[ERROR] VCODEC_DEC_PWR_USER, copy_from_user failed: %lu\n",
				     ret);
				return -EFAULT;
			}
			MODULE_MFV_LOGD("DEC_PWR_USER eDriverType = %d\n", rPowerParam.eDriverType);

			mutex_lock(&L2CLock);

#ifdef MT8173_VENC_USE_L2C
			if (rPowerParam.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
			    val_isr.eDriverType == VAL_DRIVER_TYPE_VP8_ENC ||
			    val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC) {
				gu4L2CCounter--;
				MODULE_MFV_LOGD("DEC_PWR_USER L2C counter  = %d\n", gu4L2CCounter);

				if (0 == gu4L2CCounter) {
					if (config_L2(1)) {
						MODULE_MFV_LOGE
						    ("[MFV][ERROR] Switch L2C size to 0K failed\n");
						mutex_unlock(&L2CLock);
						return -EFAULT;
					}
				}
			}
#endif
			mutex_unlock(&L2CLock);
			MODULE_MFV_LOGD("[8173] VCODEC_DEC_PWR_USER - tid = %d\n", current->pid);
		}
		break;

	case VCODEC_WAITISR:
		{
			MODULE_MFV_LOGD("[8173] VCODEC_WAITISR + tid = %d\n", current->pid);
			user_data_addr = (VAL_UINT8_T *) arg;
			ret = copy_from_user(&val_isr, user_data_addr, sizeof(VAL_ISR_T));
			if (ret) {
				MODULE_MFV_LOGE("[ERROR] VCODEC_WAITISR, copy_from_user failed: %lu\n",
					 ret);
				return -EFAULT;
			}

			if (val_isr.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
#ifdef CONFIG_MTK_VIDEO_HEVC_SUPPORT
			    val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
#endif
			    val_isr.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
			    val_isr.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC ||
			    val_isr.eDriverType == VAL_DRIVER_TYPE_VC1_DEC ||
			    val_isr.eDriverType == VAL_DRIVER_TYPE_VC1_ADV_DEC ||
			    val_isr.eDriverType == VAL_DRIVER_TYPE_VP8_DEC ||
			    val_isr.eDriverType == VAL_DRIVER_TYPE_VP9_DEC) {
				mutex_lock(&VdecHWLock);
				if (grVcodecDecHWLock.pvHandle ==
				    (VAL_VOID_T *) pmem_user_v2p_video((VAL_ULONG_T) val_isr.
								       pvHandle)) {
					bLockedHW = VAL_TRUE;
				} else {
				}
				mutex_unlock(&VdecHWLock);

				if (bLockedHW == VAL_FALSE) {
					MODULE_MFV_LOGE("[ERROR] DO NOT have HWLock, so return fail\n");
					break;
				}

				spin_lock_irqsave(&DecIsrLock, ulFlags);
				DecIsrEvent.u4TimeoutMs = val_isr.u4TimeoutMs;
				spin_unlock_irqrestore(&DecIsrLock, ulFlags);

				eValRet = eVideoWaitEvent(&DecIsrEvent, sizeof(VAL_EVENT_T));
				if (VAL_RESULT_INVALID_ISR == eValRet) {
					return -2;
				} else if (VAL_RESULT_RESTARTSYS == eValRet) {
					MODULE_MFV_LOGE
					    ("[WARNING] VAL_RESULT_RESTARTSYS return when WAITISR!!\n");
					return -ERESTARTSYS;
				}
			}
#ifdef CONFIG_MTK_VIDEO_HEVC_SUPPORT
			else if (val_isr.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
				 val_isr.eDriverType == VAL_DRIVER_TYPE_VP8_ENC ||
				 val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC) {
#else
			else if (val_isr.eDriverType == VAL_DRIVER_TYPE_H264_ENC) {
#endif
				mutex_lock(&VencHWLock);
				if (grVcodecEncHWLock.pvHandle ==
				    (VAL_VOID_T *) pmem_user_v2p_video((VAL_ULONG_T) val_isr.
								       pvHandle)) {
					bLockedHW = VAL_TRUE;
				} else {
				}
				mutex_unlock(&VencHWLock);

				if (bLockedHW == VAL_FALSE) {
					MODULE_MFV_LOGE
					    ("[ERROR] DO NOT have enc HWLock, so return fail pa:%lx, va:%lx\n",
					     pmem_user_v2p_video((VAL_ULONG_T) val_isr.pvHandle),
					     (VAL_ULONG_T) val_isr.pvHandle);
					break;
				}

				spin_lock_irqsave(&EncIsrLock, ulFlags);
				EncIsrEvent.u4TimeoutMs = val_isr.u4TimeoutMs;
				spin_unlock_irqrestore(&EncIsrLock, ulFlags);

				eValRet = eVideoWaitEvent(&EncIsrEvent, sizeof(VAL_EVENT_T));
				if (VAL_RESULT_INVALID_ISR == eValRet) {
					return -2;
				} else if (VAL_RESULT_RESTARTSYS == eValRet) {
					MODULE_MFV_LOGE
					    ("[WARNING] VAL_RESULT_RESTARTSYS return when WAITISR!!\n");
					return -ERESTARTSYS;
				}

				if (val_isr.u4IrqStatusNum > 0) {
					val_isr.u4IrqStatus[0] = gu4HwVencIrqStatus;
					ret =
					    copy_to_user(user_data_addr, &val_isr,
							 sizeof(VAL_ISR_T));
					if (ret) {
						MODULE_MFV_LOGE
						    ("[ERROR] VCODEC_WAITISR, copy_to_user failed: %lu\n",
						     ret);
						return -EFAULT;
					}
				}
			} else {
				MODULE_MFV_LOGE("[WARNING] VCODEC_WAITISR Unknown instance\n");
				return -EFAULT;
			}
			MODULE_MFV_LOGD("[8173] VCODEC_WAITISR - tid = %d\n", current->pid);
		}
		break;

	case VCODEC_INITHWLOCK:
		{
			MODULE_MFV_LOGE("[8173] VCODEC_INITHWLOCK [EMPTY] + - tid = %d\n", current->pid);

			MODULE_MFV_LOGE("[8173] VCODEC_INITHWLOCK [EMPTY] - - tid = %d\n", current->pid);
		}
		break;

	case VCODEC_DEINITHWLOCK:
		{
			MODULE_MFV_LOGE("[8173] VCODEC_DEINITHWLOCK [EMPTY] + - tid = %d\n", current->pid);

			MODULE_MFV_LOGE("[8173] VCODEC_DEINITHWLOCK [EMPTY] - - tid = %d\n", current->pid);
		}
		break;

	case VCODEC_GET_CPU_LOADING_INFO:
		{
			VAL_UINT8_T *user_data_addr;
			VAL_VCODEC_CPU_LOADING_INFO_T _temp = {0};

			MODULE_MFV_LOGD("[8173] VCODEC_GET_CPU_LOADING_INFO +\n");
			user_data_addr = (VAL_UINT8_T *) arg;
			/* TODO: */
#if 0				/* Morris Yang 20120112 mark temporarily */
			_temp._cpu_idle_time = mt_get_cpu_idle(0);
			_temp._thread_cpu_time = mt_get_thread_cputime(0);
			spin_lock_irqsave(&OalHWContextLock, ulFlags);
			_temp._inst_count = getCurInstanceCount();
			spin_unlock_irqrestore(&OalHWContextLock, ulFlags);
			_temp._sched_clock = mt_sched_clock();
#endif
			ret =
			    copy_to_user(user_data_addr, &_temp,
					 sizeof(VAL_VCODEC_CPU_LOADING_INFO_T));
			if (ret) {
				MODULE_MFV_LOGE
				    ("[ERROR] VCODEC_GET_CPU_LOADING_INFO, copy_to_user failed: %lu\n",
				     ret);
				return -EFAULT;
			}

			MODULE_MFV_LOGD("[8173] VCODEC_GET_CPU_LOADING_INFO -\n");
		}
		break;

	case VCODEC_GET_CORE_LOADING:
		{
			MODULE_MFV_LOGD("[8173] VCODEC_GET_CORE_LOADING + - tid = %d\n", current->pid);

			user_data_addr = (VAL_UINT8_T *) arg;
			ret =
			    copy_from_user(&rTempCoreLoading, user_data_addr,
					   sizeof(VAL_VCODEC_CORE_LOADING_T));
			if (ret) {
				MODULE_MFV_LOGE
				    ("[ERROR] VCODEC_GET_CORE_LOADING, copy_from_user failed: %lu\n",
				     ret);
				return -EFAULT;
			}
			/* tempory remark, must enable after function check-in */
			/* rTempCoreLoading.Loading = get_cpu_load(rTempCoreLoading.CPUid); */
			ret =
			    copy_to_user(user_data_addr, &rTempCoreLoading,
					 sizeof(VAL_VCODEC_CORE_LOADING_T));
			if (ret) {
				MODULE_MFV_LOGE
				    ("[ERROR] VCODEC_GET_CORE_LOADING, copy_to_user failed: %lu\n",
				     ret);
				return -EFAULT;
			}

			MODULE_MFV_LOGD("[8173 VCODEC_GET_CORE_LOADING - - tid = %d\n", current->pid);
		}
		break;

	case VCODEC_GET_CORE_NUMBER:
		{
			MODULE_MFV_LOGD("[8173] VCODEC_GET_CORE_NUMBER + - tid = %d\n", current->pid);

			user_data_addr = (VAL_UINT8_T *) arg;
			temp_nr_cpu_ids = nr_cpu_ids;
			ret = copy_to_user(user_data_addr, &temp_nr_cpu_ids, sizeof(int));
			if (ret) {
				MODULE_MFV_LOGE
				    ("[ERROR] VCODEC_GET_CORE_NUMBER, copy_to_user failed: %lu\n",
				     ret);
				return -EFAULT;
			}
			MODULE_MFV_LOGD("[8173] VCODEC_GET_CORE_NUMBER - - tid = %d\n", current->pid);
		}
		break;
	case VCODEC_SET_CPU_OPP_LIMIT:
		{
			MODULE_MFV_LOGE("[8173] VCODEC_SET_CPU_OPP_LIMIT [EMPTY] + - tid = %d\n",
				 current->pid);
			user_data_addr = (VAL_UINT8_T *) arg;
			ret =
			    copy_from_user(&rCpuOppLimit, user_data_addr,
					   sizeof(VAL_VCODEC_CPU_OPP_LIMIT_T));
			if (ret) {
				MODULE_MFV_LOGE
				    ("[ERROR] VCODEC_SET_CPU_OPP_LIMIT, copy_from_user failed: %lu\n",
				     ret);
				return -EFAULT;
			}
			MODULE_MFV_LOGE("+VCODEC_SET_CPU_OPP_LIMIT (%d, %d, %d), tid = %d\n",
				 rCpuOppLimit.limited_freq, rCpuOppLimit.limited_cpu,
				 rCpuOppLimit.enable, current->pid);
			/* TODO: Check if cpu_opp_limit is available */
			/* ret = cpu_opp_limit(EVENT_VIDEO, rCpuOppLimit.limited_freq, rCpuOppLimit.limited_cpu, rCpuOppLimit.enable); // 0: PASS, other: FAIL */
			if (ret) {
				MODULE_MFV_LOGE("[ERROR] cpu_opp_limit failed: %lu\n", ret);
				return -EFAULT;
			}
			MODULE_MFV_LOGE("-VCODEC_SET_CPU_OPP_LIMIT tid = %d, ret = %lu\n", current->pid,
				 ret);
			MODULE_MFV_LOGE("[8173] VCODEC_SET_CPU_OPP_LIMIT [EMPTY] - - tid = %d\n",
				 current->pid);
		}
		break;
	case VCODEC_MB:
		{
			mb();
		}
		break;
#if 0
	case MFV_SET_CMD_CMD:
		MODULE_MFV_LOGD("[MFV] MFV_SET_CMD_CMD\n");
		MODULE_MFV_LOGD("[MFV] Arg = %x\n", arg);
		user_data_addr = (VAL_UINT8_T *) arg;
		ret = copy_from_user(&rDrvCmdQueue, user_data_addr, sizeof(VCODEC_DRV_CMD_QUEUE_T));
		MODULE_MFV_LOGD("[MFV] CmdNum = %d\n", rDrvCmdQueue.CmdNum);
		u4Size = (rDrvCmdQueue.CmdNum) * sizeof(VCODEC_DRV_CMD_T);

		cmd_queue = (P_VCODEC_DRV_CMD_T) kmalloc(u4Size, GFP_ATOMIC);
		if (cmd_queue != VAL_NULL && rDrvCmdQueue.pCmd != VAL_NULL) {
			ret = copy_from_user(cmd_queue, rDrvCmdQueue.pCmd, u4Size);
			while (cmd_queue->type != END_CMD) {
				switch (cmd_queue->type) {
				case ENABLE_HW_CMD:
					break;
				case DISABLE_HW_CMD:
					break;
				case WRITE_REG_CMD:
					VDO_HW_WRITE(cmd_queue->address + cmd_queue->offset,
						     cmd_queue->value);
					break;
				case READ_REG_CMD:
					uValue =
					    VDO_HW_READ(cmd_queue->address + cmd_queue->offset);
					copy_to_user((void *)cmd_queue->value, &uValue,
						     sizeof(VAL_UINT32_T));
					break;
				case WRITE_SYSRAM_CMD:
					VDO_HW_WRITE(cmd_queue->address + cmd_queue->offset,
						     cmd_queue->value);
					break;
				case READ_SYSRAM_CMD:
					uValue =
					    VDO_HW_READ(cmd_queue->address + cmd_queue->offset);
					copy_to_user((void *)cmd_queue->value, &uValue,
						     sizeof(VAL_UINT32_T));
					break;
				case MASTER_WRITE_CMD:
					uValue =
					    VDO_HW_READ(cmd_queue->address + cmd_queue->offset);
					VDO_HW_WRITE(cmd_queue->address + cmd_queue->offset,
						     cmd_queue->value | (uValue & cmd_queue->mask));
					break;
				case SETUP_ISR_CMD:
					break;
				case WAIT_ISR_CMD:
					MODULE_MFV_LOGD("HAL_CMD_SET_CMD_QUEUE: WAIT_ISR_CMD+\n");

					MODULE_MFV_LOGD("HAL_CMD_SET_CMD_QUEUE: WAIT_ISR_CMD-\n");
					break;
				case TIMEOUT_CMD:
					break;
				case WRITE_SYSRAM_RANGE_CMD:
					break;
				case READ_SYSRAM_RANGE_CMD:
					break;
				case POLL_REG_STATUS_CMD:
					uValue =
					    VDO_HW_READ(cmd_queue->address + cmd_queue->offset);
					nCount = 0;
					while ((uValue & cmd_queue->mask) != 0) {
						nCount++;
						if (nCount > 1000)
							break;

						uValue =
						    VDO_HW_READ(cmd_queue->address +
								cmd_queue->offset);
					}
					break;
				default:
					break;
				}
				cmd_queue++;
			}

		}
		break;
#endif
	default:
		MODULE_MFV_LOGE("========[ERROR] vcodec_ioctl default case======== %u\n", cmd);
		break;
	}
	return 0xFF;
}

#if IS_ENABLED(CONFIG_COMPAT)

typedef enum {
	VAL_HW_LOCK_TYPE = 0,
	VAL_POWER_TYPE,
	VAL_ISR_TYPE,
	VAL_MEMORY_TYPE
} STRUCT_TYPE;

typedef enum {
	COPY_FROM_USER = 0,
	COPY_TO_USER,
} COPY_DIRECTION;

typedef struct COMPAT_VAL_HW_LOCK {
	compat_uptr_t pvHandle;	/* /< [IN]     The video codec driver handle */
	compat_uint_t u4HandleSize;	/* /< [IN]     The size of video codec driver handle */
	compat_uptr_t pvLock;	/* /< [IN/OUT] The Lock discriptor */
	compat_uint_t u4TimeoutMs;	/* /< [IN]     The timeout ms */
	compat_uptr_t pvReserved;	/* /< [IN/OUT] The reserved parameter */
	compat_uint_t u4ReservedSize;	/* /< [IN]     The size of reserved parameter structure */
	compat_uint_t eDriverType;	/* /< [IN]     The driver type */
	char bSecureInst;	/* /< [IN]     True if this is a secure instance // MTK_SEC_VIDEO_PATH_SUPPORT */
} COMPAT_VAL_HW_LOCK_T;

typedef struct COMPAT_VAL_POWER {
	compat_uptr_t pvHandle;	/* /< [IN]     The video codec driver handle */
	compat_uint_t u4HandleSize;	/* /< [IN]     The size of video codec driver handle */
	compat_uint_t eDriverType;	/* /< [IN]     The driver type */
	char fgEnable;		/* /< [IN]     Enable or not. */
	compat_uptr_t pvReserved;	/* /< [IN/OUT] The reserved parameter */
	compat_uint_t u4ReservedSize;	/* /< [IN]     The size of reserved parameter structure */
	/* VAL_UINT32_T        u4L2CUser;              ///< [OUT]    The number of power user right now */
} COMPAT_VAL_POWER_T;

typedef struct COMPAT_VAL_ISR {
	compat_uptr_t pvHandle;	/* /< [IN]     The video codec driver handle */
	compat_uint_t u4HandleSize;	/* /< [IN]     The size of video codec driver handle */
	compat_uint_t eDriverType;	/* /< [IN]     The driver type */
	compat_uptr_t pvIsrFunction;	/* /< [IN]     The isr function */
	compat_uptr_t pvReserved;	/* /< [IN/OUT] The reserved parameter */
	compat_uint_t u4ReservedSize;	/* /< [IN]     The size of reserved parameter structure */
	compat_uint_t u4TimeoutMs;	/* /< [IN]     The timeout in ms */
	compat_uint_t u4IrqStatusNum;	/* /< [IN]     The num of return registers when HW done */
	compat_uint_t u4IrqStatus[IRQ_STATUS_MAX_NUM];	/* /< [IN/OUT] The value of return registers when HW done */
} COMPAT_VAL_ISR_T;

typedef struct COMPAT_VAL_MEMORY {
	compat_uint_t eMemType;	/* /< [IN]     The allocation memory type */
	compat_ulong_t u4MemSize;	/* /< [IN]     The size of memory allocation */
	compat_uptr_t pvMemVa;	/* /< [IN/OUT] The memory virtual address */
	compat_uptr_t pvMemPa;	/* /< [IN/OUT] The memory physical address */
	compat_uint_t eAlignment;	/* /< [IN]     The memory byte alignment setting */
	compat_uptr_t pvAlignMemVa;	/* /< [IN/OUT] The align memory virtual address */
	compat_uptr_t pvAlignMemPa;	/* /< [IN/OUT] The align memory physical address */
	compat_uint_t eMemCodec;	/* /< [IN]     The memory codec for VENC or VDEC */
	compat_uint_t i4IonShareFd;
	compat_uptr_t pIonBufhandle;
	compat_uptr_t pvReserved;	/* /< [IN/OUT] The reserved parameter */
	compat_ulong_t u4ReservedSize;	/* /< [IN]     The size of reserved parameter structure */
} COMPAT_VAL_MEMORY_T;

static int compat_copy_struct(STRUCT_TYPE eType,
			      COPY_DIRECTION eDirection, void __user *data32, void __user *data)
{
	compat_uint_t u;
	compat_ulong_t l;
	compat_uptr_t p;
	VAL_VOID_T *ptr;
	char c;
	int err = 0;

	switch (eType) {
	case VAL_HW_LOCK_TYPE:
		{
			if (eDirection == COPY_FROM_USER) {
				COMPAT_VAL_HW_LOCK_T __user *from32 =
				    (COMPAT_VAL_HW_LOCK_T *) data32;
				VAL_HW_LOCK_T __user *to = (VAL_HW_LOCK_T *) data;

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
				COMPAT_VAL_HW_LOCK_T __user *to32 = (COMPAT_VAL_HW_LOCK_T *) data32;
				VAL_HW_LOCK_T __user *from = (VAL_HW_LOCK_T *) data;

				err = get_user(ptr, &(from->pvHandle));
				err |= put_user(ptr_to_compat(ptr), &(to32->pvHandle));
				err |= get_user(u, &(from->u4HandleSize));
				err |= put_user(u, &(to32->u4HandleSize));
				err |= get_user(ptr, &(from->pvLock));
				err |= put_user(ptr_to_compat(ptr), &(to32->pvLock));
				err |= get_user(u, &(from->u4TimeoutMs));
				err |= put_user(u, &(to32->u4TimeoutMs));
				err |= get_user(ptr, &(from->pvReserved));
				err |= put_user(ptr_to_compat(ptr), &(to32->pvReserved));
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
				COMPAT_VAL_POWER_T __user *from32 = (COMPAT_VAL_POWER_T *) data32;
				VAL_POWER_T __user *to = (VAL_POWER_T *) data;

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
				COMPAT_VAL_POWER_T __user *to32 = (COMPAT_VAL_POWER_T *) data32;
				VAL_POWER_T __user *from = (VAL_POWER_T *) data;

				err = get_user(ptr, &(from->pvHandle));
				err |= put_user(ptr_to_compat(ptr), &(to32->pvHandle));
				err |= get_user(u, &(from->u4HandleSize));
				err |= put_user(u, &(to32->u4HandleSize));
				err |= get_user(u, &(from->eDriverType));
				err |= put_user(u, &(to32->eDriverType));
				err |= get_user(c, &(from->fgEnable));
				err |= put_user(c, &(to32->fgEnable));
				err |= get_user(ptr, &(from->pvReserved));
				err |= put_user(ptr_to_compat(ptr), &(to32->pvReserved));
				err |= get_user(u, &(from->u4ReservedSize));
				err |= put_user(u, &(to32->u4ReservedSize));
			}
		}
		break;
	case VAL_ISR_TYPE:
		{
			int i = 0;

			if (eDirection == COPY_FROM_USER) {
				COMPAT_VAL_ISR_T __user *from32 = (COMPAT_VAL_ISR_T *) data32;
				VAL_ISR_T __user *to = (VAL_ISR_T *) data;

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
			} else {
				COMPAT_VAL_ISR_T __user *to32 = (COMPAT_VAL_ISR_T *) data32;
				VAL_ISR_T __user *from = (VAL_ISR_T *) data;

				err = get_user(ptr, &(from->pvHandle));
				err |= put_user(ptr_to_compat(ptr), &(to32->pvHandle));
				err |= get_user(u, &(from->u4HandleSize));
				err |= put_user(u, &(to32->u4HandleSize));
				err |= get_user(u, &(from->eDriverType));
				err |= put_user(u, &(to32->eDriverType));
				err |= get_user(ptr, &(from->pvIsrFunction));
				err |= put_user(ptr_to_compat(ptr), &(to32->pvIsrFunction));
				err |= get_user(ptr, &(from->pvReserved));
				err |= put_user(ptr_to_compat(ptr), &(to32->pvReserved));
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
				COMPAT_VAL_MEMORY_T __user *from32 = (COMPAT_VAL_MEMORY_T *) data32;
				VAL_MEMORY_T __user *to = (VAL_MEMORY_T *) data;

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
				COMPAT_VAL_MEMORY_T __user *to32 = (COMPAT_VAL_MEMORY_T *) data32;
				VAL_MEMORY_T __user *from = (VAL_MEMORY_T *) data;

				err = get_user(u, &(from->eMemType));
				err |= put_user(u, &(to32->eMemType));
				err |= get_user(l, &(from->u4MemSize));
				err |= put_user(l, &(to32->u4MemSize));
				err |= get_user(ptr, &(from->pvMemVa));
				err |= put_user(ptr_to_compat(ptr), &(to32->pvMemVa));
				err |= get_user(ptr, &(from->pvMemPa));
				err |= put_user(ptr_to_compat(ptr), &(to32->pvMemPa));
				err |= get_user(u, &(from->eAlignment));
				err |= put_user(u, &(to32->eAlignment));
				err |= get_user(ptr, &(from->pvAlignMemVa));
				err |= put_user(ptr_to_compat(ptr), &(to32->pvAlignMemVa));
				err |= get_user(ptr, &(from->pvAlignMemPa));
				err |= put_user(ptr_to_compat(ptr), &(to32->pvAlignMemPa));
				err |= get_user(u, &(from->eMemCodec));
				err |= put_user(u, &(to32->eMemCodec));
				err |= get_user(u, &(from->i4IonShareFd));
				err |= put_user(u, &(to32->i4IonShareFd));
				err |= get_user(ptr, &(from->pIonBufhandle));
				err |= put_user(ptr_to_compat(ptr), &(to32->pIonBufhandle));
				err |= get_user(ptr, &(from->pvReserved));
				err |= put_user(ptr_to_compat(ptr), &(to32->pvReserved));
				err |= get_user(l, &(from->u4ReservedSize));
				err |= put_user(l, &(to32->u4ReservedSize));
			}
		}
		break;
	default:
		break;
	}

	return err;
}


static long vcodec_unlocked_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	MODULE_MFV_LOGD("[VCODEC_DEBUG] vcodec_unlocked_compat_ioctl: 0x%x\n", cmd);
	switch (cmd) {
	case VCODEC_ALLOC_NON_CACHE_BUFFER:
	case VCODEC_FREE_NON_CACHE_BUFFER:
		{
			COMPAT_VAL_MEMORY_T __user *data32;
			VAL_MEMORY_T __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(VAL_MEMORY_T));
			if (data == NULL)
				return -EFAULT;

			err =
			    compat_copy_struct(VAL_MEMORY_TYPE, COPY_FROM_USER, (void *)data32,
					       (void *)data);
			if (err)
				return err;

			ret = file->f_op->unlocked_ioctl(file, cmd, (unsigned long)data);

			err =
			    compat_copy_struct(VAL_MEMORY_TYPE, COPY_TO_USER, (void *)data32,
					       (void *)data);

			if (err)
				return err;
			return ret;
		}
		break;
	case VCODEC_LOCKHW:
	case VCODEC_UNLOCKHW:
		{
			COMPAT_VAL_HW_LOCK_T __user *data32;
			VAL_HW_LOCK_T __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(VAL_HW_LOCK_T));
			if (data == NULL)
				return -EFAULT;

			err =
			    compat_copy_struct(VAL_HW_LOCK_TYPE, COPY_FROM_USER, (void *)data32,
					       (void *)data);
			if (err)
				return err;

			ret = file->f_op->unlocked_ioctl(file, cmd, (unsigned long)data);

			err =
			    compat_copy_struct(VAL_HW_LOCK_TYPE, COPY_TO_USER, (void *)data32,
					       (void *)data);

			if (err)
				return err;
			return ret;
		}
		break;

	case VCODEC_INC_PWR_USER:
	case VCODEC_DEC_PWR_USER:
		{
			COMPAT_VAL_POWER_T __user *data32;
			VAL_POWER_T __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(VAL_POWER_T));
			if (data == NULL)
				return -EFAULT;

			err =
			    compat_copy_struct(VAL_POWER_TYPE, COPY_FROM_USER, (void *)data32,
					       (void *)data);

			if (err)
				return err;

			ret = file->f_op->unlocked_ioctl(file, cmd, (unsigned long)data);

			err =
			    compat_copy_struct(VAL_POWER_TYPE, COPY_TO_USER, (void *)data32,
					       (void *)data);

			if (err)
				return err;
			return ret;
		}
		break;

	case VCODEC_WAITISR:
		{
			COMPAT_VAL_ISR_T __user *data32;
			VAL_ISR_T __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(VAL_ISR_T));
			if (data == NULL)
				return -EFAULT;

			err =
			    compat_copy_struct(VAL_ISR_TYPE, COPY_FROM_USER, (void *)data32,
					       (void *)data);
			if (err)
				return err;

			ret = file->f_op->unlocked_ioctl(file, VCODEC_WAITISR, (unsigned long)data);

			err =
			    compat_copy_struct(VAL_ISR_TYPE, COPY_TO_USER, (void *)data32,
					       (void *)data);

			if (err)
				return err;
			return ret;
		}
		break;

	default:
		{
			return vcodec_unlocked_ioctl(file, cmd, arg);
		}
		break;
	}
	return 0;
}
#else
#define vcodec_unlocked_compat_ioctl NULL
#endif
static int vcodec_open(struct inode *inode, struct file *file)
{
	MODULE_MFV_LOGD("[VCODEC_DEBUG] vcodec_open\n");

	mutex_lock(&DriverOpenCountLock);
	MT8173Driver_Open_Count++;

	MODULE_MFV_LOGE("vcodec_open pid = %d, MT8173Driver_Open_Count %d\n", current->pid,
		 MT8173Driver_Open_Count);
	mutex_unlock(&DriverOpenCountLock);


	/* TODO: Check upper limit of concurrent users? */

	return 0;
}

static int venc_hw_reset(int type)
{
	VAL_UINT32_T uValue;
	VAL_RESULT_T eValRet;

	MODULE_MFV_LOGE("Start VENC HW Reset");

	/* //clear irq event */
	/* EncIsrEvent.u4TimeoutMs = 1; */
	/* eValRet = eVideoWaitEvent(&EncIsrEvent, sizeof(VAL_EVENT_T)); */
	/* MODULE_MFV_LOGE("ret %d", eValRet); */
	/* if type == 0//Soft Reset */
	/* step 1 */
	VDO_HW_WRITE(KVA_VENC_SW_PAUSE, 1);
	/* step 2 */
	/* EncIsrEvent.u4TimeoutMs = 10000; */
	EncIsrEvent.u4TimeoutMs = 2;
	eValRet = eVideoWaitEvent(&EncIsrEvent, sizeof(VAL_EVENT_T));
	if (VAL_RESULT_INVALID_ISR == eValRet || gu4HwVencIrqStatus != VENC_IRQ_STATUS_PAUSE) {
		uValue = VDO_HW_READ(KVA_VENC_IRQ_STATUS_ADDR);
		if (gu4HwVencIrqStatus != VENC_IRQ_STATUS_PAUSE)
			udelay(200);

		MODULE_MFV_LOGE("irq_status 0x%x", uValue);
		VDO_HW_WRITE(KVA_VENC_SW_PAUSE, 0);
		VDO_HW_WRITE(KVA_VENC_SW_HRST_N, 0);
		uValue = VDO_HW_READ(KVA_VENC_SW_HRST_N);
		MODULE_MFV_LOGE("3 HRST = %d, isr = 0x%x", uValue, gu4HwVencIrqStatus);
	} else {		/* step 4 */
		VDO_HW_WRITE(KVA_VENC_SW_HRST_N, 0);
		uValue = gu4HwVencIrqStatus;
		VDO_HW_WRITE(KVA_VENC_SW_PAUSE, 0);
		MODULE_MFV_LOGE("4 HRST = %d, isr = 0x%x", uValue, gu4HwVencIrqStatus);
	}

	VDO_HW_WRITE(KVA_VENC_SW_HRST_N, 1);
	uValue = VDO_HW_READ(KVA_VENC_SW_HRST_N);
	MODULE_MFV_LOGE("HRST = %d", uValue);

	return 1;
}

static int vcodec_flush(struct file *file, fl_owner_t id)
{
	MODULE_MFV_LOGD("[VCODEC_DEBUG] vcodec_flush, curr_tid =%d\n", current->pid);
	MODULE_MFV_LOGE("vcodec_flush pid = %d, MT8173Driver_Open_Count %d\n", current->pid, MT8173Driver_Open_Count);

	return 0;
}

static int vcodec_release(struct inode *inode, struct file *file)
{
	VAL_ULONG_T ulFlagsLockHW, ulFlagsISR;
	/* dump_stack(); */
	MODULE_MFV_LOGD("[VCODEC_DEBUG] vcodec_release, curr_tid =%d\n", current->pid);
	mutex_lock(&DriverOpenCountLock);
	MODULE_MFV_LOGE("vcodec_flush pid = %d, MT8173Driver_Open_Count %d\n", current->pid,
		MT8173Driver_Open_Count);
	MT8173Driver_Open_Count--;

	if (MT8173Driver_Open_Count == 0) {
		mutex_lock(&VencHWLock);
		if (grVcodecEncHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC) {
			venc_hw_reset(0);
			disable_irq(VENC_IRQ_ID);
			venc_power_off();
			MODULE_MFV_LOGE("Clean venc lock\n");
		}
		mutex_unlock(&VencHWLock);

		mutex_lock(&VdecHWLock);
		gu4VdecLockThreadId = 0;
		grVcodecDecHWLock.pvHandle = 0;
		grVcodecDecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
		grVcodecDecHWLock.rLockedTime.u4Sec = 0;
		grVcodecDecHWLock.rLockedTime.u4uSec = 0;
		mutex_unlock(&VdecHWLock);

		mutex_lock(&VencHWLock);
		grVcodecEncHWLock.pvHandle = 0;
		grVcodecEncHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
		grVcodecEncHWLock.rLockedTime.u4Sec = 0;
		grVcodecEncHWLock.rLockedTime.u4uSec = 0;
		mutex_unlock(&VencHWLock);

		mutex_lock(&DecEMILock);
		gu4DecEMICounter = 0;
		mutex_unlock(&DecEMILock);

		mutex_lock(&EncEMILock);
		gu4EncEMICounter = 0;
		mutex_unlock(&EncEMILock);

		mutex_lock(&PWRLock);
		gu4PWRCounter = 0;
		mutex_unlock(&PWRLock);

#ifdef MT8173_VENC_USE_L2C
		mutex_lock(&L2CLock);
		if (gu4L2CCounter != 0) {
			MODULE_MFV_LOGE("vcodec_flush pid = %d, L2 user = %d, force restore L2 settings\n",
				 current->pid, gu4L2CCounter);
			if (config_L2(1))
				MODULE_MFV_LOGE("restore L2 settings failed\n");
		}
		gu4L2CCounter = 0;
		mutex_unlock(&L2CLock);
#endif
		spin_lock_irqsave(&LockDecHWCountLock, ulFlagsLockHW);
		gu4LockDecHWCount = 0;
		spin_unlock_irqrestore(&LockDecHWCountLock, ulFlagsLockHW);

		spin_lock_irqsave(&LockEncHWCountLock, ulFlagsLockHW);
		gu4LockEncHWCount = 0;
		spin_unlock_irqrestore(&LockEncHWCountLock, ulFlagsLockHW);

		spin_lock_irqsave(&DecISRCountLock, ulFlagsISR);
		gu4DecISRCount = 0;
		spin_unlock_irqrestore(&DecISRCountLock, ulFlagsISR);

		spin_lock_irqsave(&EncISRCountLock, ulFlagsISR);
		gu4EncISRCount = 0;
		spin_unlock_irqrestore(&EncISRCountLock, ulFlagsISR);
#ifdef ENABLE_MMDVFS_VDEC
		if (VAL_TRUE == gMMDFVFSMonitorStarts) {
			gMMDFVFSMonitorStarts = VAL_FALSE;
			gMMDFVFSMonitorCounts = 0;
			gHWLockInterval = 0;
			SendDvfsRequest(DVFS_LOW);
		}
#endif
	}
#ifdef ENABLE_MMDVFS_VDEC
	mutex_lock(&DecEMILock);
	if (VAL_TRUE == gMMDFVFSMonitorStarts && 0 == gu4DecEMICounter) {
		gMMDFVFSMonitorStarts = VAL_FALSE;
		gMMDFVFSMonitorCounts = 0;
		gHWLockInterval = 0;
		SendDvfsRequest(DVFS_LOW);
	}
	mutex_unlock(&DecEMILock);
#endif
	mutex_unlock(&DriverOpenCountLock);

	return 0;
}

void vcodec_vma_open(struct vm_area_struct *vma)
{
	MODULE_MFV_LOGD("vcodec VMA open, virt %lx, phys %lx\n", vma->vm_start,
		 vma->vm_pgoff << PAGE_SHIFT);
}

void vcodec_vma_close(struct vm_area_struct *vma)
{
	MODULE_MFV_LOGD("vcodec VMA close, virt %lx, phys %lx\n", vma->vm_start,
		 vma->vm_pgoff << PAGE_SHIFT);
}

static struct vm_operations_struct vcodec_remap_vm_ops = {
	.open = vcodec_vma_open,
	.close = vcodec_vma_close,
};

static int vcodec_mmap(struct file *file, struct vm_area_struct *vma)
{
	VAL_UINT32_T u4I = 0;
	VAL_ULONG_T length;
	VAL_ULONG_T pfn;

	length = vma->vm_end - vma->vm_start;
	pfn = vma->vm_pgoff << PAGE_SHIFT;

	if (((length > VENC_REGION) || (pfn < VENC_BASE) || (pfn > VENC_BASE + VENC_REGION)) &&
		((length > VENC_REGION) || (pfn < VENC_LT_BASE) || (pfn > VENC_LT_BASE + VENC_REGION)) &&
	    ((length > VDEC_REGION) || (pfn < VDEC_BASE_PHY) || (pfn > VDEC_BASE_PHY + VDEC_REGION))
	    && ((length > HW_REGION) || (pfn < HW_BASE) || (pfn > HW_BASE + HW_REGION))
	    && ((length > INFO_REGION) || (pfn < INFO_BASE) || (pfn > INFO_BASE + INFO_REGION))
	    ) {
		VAL_ULONG_T ulAddr, ulSize;

		for (u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++) {
			if ((grNonCacheMemoryList[u4I].ulKVA != -1L)
			    && (grNonCacheMemoryList[u4I].ulKPA != -1L)) {
				ulAddr = grNonCacheMemoryList[u4I].ulKPA;
				ulSize =
				    (grNonCacheMemoryList[u4I].ulSize + 0x1000 - 1) & ~(0x1000 - 1);
				if ((length == ulSize) && (pfn == ulAddr)) {
					MODULE_MFV_LOGD(" cache idx %d\n", u4I);
					break;
				}
			}
		}

		if (u4I == VCODEC_MULTIPLE_INSTANCE_NUM_x_10) {
			MODULE_MFV_LOGE("[ERROR] mmap region error: Length(0x%lx), pfn(0x%lx)\n",
				 (VAL_ULONG_T) length, pfn);
			return -EAGAIN;
		}
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	MODULE_MFV_LOGE("[mmap] vma->start 0x%lx, vma->end 0x%lx, vma->pgoff 0x%lx\n",
		 (VAL_ULONG_T) vma->vm_start, (VAL_ULONG_T) vma->vm_end,
		 (VAL_ULONG_T) vma->vm_pgoff);
	if (remap_pfn_range
	    (vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		return -EAGAIN;
	}

	vma->vm_ops = &vcodec_remap_vm_ops;
	vcodec_vma_open(vma);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void vcodec_early_suspend(struct early_suspend *h)
{
	mutex_lock(&PWRLock);
	MODULE_MFV_LOGE("vcodec_early_suspend, tid = %d, PWR_USER = %d\n", current->pid, gu4PWRCounter);
	mutex_unlock(&PWRLock);
/*
    if (gu4PWRCounter != 0)
    {
	MODULE_MFV_LOGE("[MT6589_VCodec_early_suspend] Someone Use HW, Disable Power!\n");
	disable_clock(MT65XX_PDN_MM_VBUF, "Video_VBUF");
	disable_clock(MT_CG_VDEC0_VDE, "VideoDec");
	disable_clock(MT_CG_VENC_VEN, "VideoEnc");
	disable_clock(MT65XX_PDN_MM_GDC_SHARE_MACRO, "VideoEnc");
    }
*/
	MODULE_MFV_LOGD("vcodec_early_suspend - tid = %d\n", current->pid);
}

static void vcodec_late_resume(struct early_suspend *h)
{
	mutex_lock(&PWRLock);
	MODULE_MFV_LOGE("vcodec_late_resume, tid = %d, PWR_USER = %d\n", current->pid, gu4PWRCounter);
	mutex_unlock(&PWRLock);
/*
    if (gu4PWRCounter != 0)
    {
	MODULE_MFV_LOGE("[vcodec_late_resume] Someone Use HW, Enable Power!\n");
	enable_clock(MT65XX_PDN_MM_VBUF, "Video_VBUF");
	enable_clock(MT_CG_VDEC0_VDE, "VideoDec");
	enable_clock(MT_CG_VENC_VEN, "VideoEnc");
	enable_clock(MT65XX_PDN_MM_GDC_SHARE_MACRO, "VideoEnc");
    }
*/
	MODULE_MFV_LOGD("vcodec_late_resume - tid = %d\n", current->pid);
}

static struct early_suspend vcodec_early_suspend_handler = {
	.level = (EARLY_SUSPEND_LEVEL_DISABLE_FB - 1),
	.suspend = vcodec_early_suspend,
	.resume = vcodec_late_resume,
};
#endif

static const struct file_operations vcodec_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = vcodec_unlocked_ioctl,
	.open = vcodec_open,
	.flush = vcodec_flush,
	.release = vcodec_release,
	.mmap = vcodec_mmap,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = vcodec_unlocked_compat_ioctl,
#endif
};

static int vcodec_probe(struct platform_device *pdev)
{
	int ret;

	MODULE_MFV_LOGD("+vcodec_probe\n");

	mutex_lock(&DecEMILock);
	gu4DecEMICounter = 0;
	mutex_unlock(&DecEMILock);

	mutex_lock(&EncEMILock);
	gu4EncEMICounter = 0;
	mutex_unlock(&EncEMILock);

	mutex_lock(&PWRLock);
	gu4PWRCounter = 0;
	mutex_unlock(&PWRLock);

	mutex_lock(&L2CLock);
	gu4L2CCounter = 0;
	mutex_unlock(&L2CLock);

#ifdef CONFIG_OF
	/*clk_venc_larb = devm_clk_get(&pdev->dev, "MT_CG_VENC_SMI_LARB3");
	BUG_ON(IS_ERR(clk_venc_larb));
	clk_venc_clk = devm_clk_get(&pdev->dev, "MT_CG_VENC_CKE1");
	BUG_ON(IS_ERR(clk_venc_clk));
	clk_venc_lt_larb = devm_clk_get(&pdev->dev, "MT_CG_VENCLT_LARB");
	BUG_ON(IS_ERR(clk_venc_lt_larb));
	clk_venc_lt_clk = devm_clk_get(&pdev->dev, "MT_CG_VENCLT_CKE");
	BUG_ON(IS_ERR(clk_venc_lt_clk));*/

#if 0
	clk_vdecpwr = devm_clk_get(&pdev->dev, "MT_VDEC_POWER");
	BUG_ON(IS_ERR(clk_vdecpwr));
	clk_venc_pwr  = devm_clk_get(&pdev->dev, "MT_VENC_POWER");
	clk_venc_pwr2  = devm_clk_get(&pdev->dev, "MT_VENC_POWER2");

	/*disable all venc clock */
	clk_prepare(clk_venc_clk);
	clk_enable(clk_venc_clk);
	clk_prepare(clk_venc_lt_clk);
	clk_enable(clk_venc_lt_clk);

	clk_disable(clk_venc_clk);
	clk_unprepare(clk_venc_clk);
	clk_disable(clk_venc_lt_clk);
	clk_unprepare(clk_venc_lt_clk);
#endif
#endif

	ret = register_chrdev_region(vcodec_devno, 1, VCODEC_DEVNAME);
	if (ret)
		MODULE_MFV_LOGE("[VCODEC_DEBUG][ERROR] Can't Get Major number for VCodec Device\n");

	vcodec_cdev = cdev_alloc();
	vcodec_cdev->owner = THIS_MODULE;
	vcodec_cdev->ops = &vcodec_fops;

	ret = cdev_add(vcodec_cdev, vcodec_devno, 1);
	if (ret)
		MODULE_MFV_LOGE("[VCODEC_DEBUG][ERROR] Can't add Vcodec Device\n");

	vcodec_class = class_create(THIS_MODULE, VCODEC_DEVNAME);
	if (IS_ERR(vcodec_class)) {
		ret = PTR_ERR(vcodec_class);
		MODULE_MFV_LOGE("Unable to create class, err = %d", ret);
		return ret;
	}

	vcodec_device = device_create(vcodec_class, NULL, vcodec_devno, NULL, VCODEC_DEVNAME);

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#else
	/* if (request_irq(MT_VDEC_IRQ_ID , (irq_handler_t)video_intr_dlr, IRQF_TRIGGER_LOW, VCODEC_DEVNAME, NULL) < 0) */
	if (request_irq
	    (VDEC_IRQ_ID, (irq_handler_t) video_intr_dlr, IRQF_TRIGGER_LOW, VCODEC_DEVNAME,
	     NULL) < 0) {
		MODULE_MFV_LOGD("[VCODEC_DEBUG][ERROR] error to request dec irq\n");
	} else {
		MODULE_MFV_LOGD("[VCODEC_DEBUG] success to request dec irq: %d\n", VDEC_IRQ_ID);
	}

	/* if (request_irq(MT_VENC_IRQ_ID , (irq_handler_t)video_intr_dlr2, IRQF_TRIGGER_LOW, VCODEC_DEVNAME, NULL) < 0) */
	if (request_irq
	    (VENC_IRQ_ID, (irq_handler_t) video_intr_dlr2, IRQF_TRIGGER_LOW, VCODEC_DEVNAME,
	     NULL) < 0) {
		MODULE_MFV_LOGD("[VCODEC_DEBUG][ERROR] error to request enc irq\n");
	} else {
		MODULE_MFV_LOGD("[VCODEC_DEBUG] success to request enc irq: %d\n", VENC_IRQ_ID);
	}

	#ifndef CONFIG_ARCH_MT8163
	if (request_irq
	    (VENC_LT_IRQ_ID, (irq_handler_t) video_intr_dlr2, IRQF_TRIGGER_LOW, VCODEC_DEVNAME,
	     NULL) < 0) {
		MODULE_MFV_LOGD("[VCODEC_DEBUG][ERROR] error to request enc_LT irq\n");
	} else {
		MODULE_MFV_LOGD("[VCODEC_DEBUG] success to request enc_LT irq: %d\n", VENC_LT_IRQ_ID);
	}
	#endif

#endif

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#else
	/* disable_irq(MT_VDEC_IRQ_ID); */
	disable_irq(VDEC_IRQ_ID);
	/* disable_irq(MT_VENC_IRQ_ID); */
	disable_irq(VENC_IRQ_ID);
	#ifndef CONFIG_ARCH_MT8163
	disable_irq(VENC_LT_IRQ_ID);
	#endif
#endif
	MODULE_MFV_LOGD("[VCODEC_DEBUG] vcodec_probe Done\n");

	return 0;
}

/*
static int venc_probe(struct platform_device *pdev)
{

#ifdef CONFIG_OF
	int ret;
	struct class_device *class_dev = NULL;

	MODULE_MFV_LOGD("+venc_probe\n");

	pvenc_dev = pdev;

	ret = alloc_chrdev_region(&venc_devno, 0, 1, VENC_DEVNAME);
	if (ret)
		MODULE_MFV_LOGE("Error: Can't Get Major number for VENC_DEVNAME Device\n");
	else
		MODULE_MFV_LOGD("Get VENC Device Major number (%d)\n", venc_devno);

	venc_cdev = cdev_alloc();
	venc_cdev->owner = THIS_MODULE;
	venc_cdev->ops = NULL;

	ret = cdev_add(venc_cdev, venc_devno, 1);

	venc_class = class_create(THIS_MODULE, VENC_DEVNAME);
	class_dev =
		(struct class_device *)device_create(venc_class, NULL, venc_devno, NULL, VENC_DEVNAME);
#endif

	return 0;
}
*/

static int venc_disableIRQ(VAL_HW_LOCK_T *prHWLock)
{
	VAL_UINT32_T  u4IrqId = VENC_IRQ_ID;

	if (VAL_DRIVER_TYPE_H264_ENC == prHWLock->eDriverType
		|| VAL_DRIVER_TYPE_HEVC_ENC == prHWLock->eDriverType)
		u4IrqId = VENC_IRQ_ID;
	else if (VAL_DRIVER_TYPE_VP8_ENC == prHWLock->eDriverType)
		u4IrqId = VENC_LT_IRQ_ID;


#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT	/* Morris Yang moved to TEE */
	if (prHWLock->bSecureInst == VAL_FALSE) {
		/* disable_irq(VENC_IRQ_ID); */

		free_irq(u4IrqId, NULL);
	}
#else
		/* disable_irq(MT_VENC_IRQ_ID); */
		disable_irq(u4IrqId);
#endif
		/* turn venc power off */
		venc_power_off();
		return 0;
}

static int venc_enableIRQ(VAL_HW_LOCK_T *prHWLock)
{
	VAL_UINT32_T  u4IrqId = VENC_IRQ_ID;

	MODULE_MFV_LOGD("venc_enableIRQ+\n");
	if (VAL_DRIVER_TYPE_H264_ENC == prHWLock->eDriverType
		|| VAL_DRIVER_TYPE_HEVC_ENC == prHWLock->eDriverType)
		u4IrqId = VENC_IRQ_ID;
	else if (VAL_DRIVER_TYPE_VP8_ENC == prHWLock->eDriverType)
		u4IrqId = VENC_LT_IRQ_ID;

	venc_power_on();
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	MODULE_MFV_LOGD("[VCODEC_LOCKHW] ENC rHWLock.bSecureInst 0x%x\n", prHWLock->bSecureInst);
	if (prHWLock->bSecureInst == VAL_FALSE) {
		MODULE_MFV_LOGD("[VCODEC_LOCKHW]  ENC Request IR by type 0x%x\n", prHWLock->eDriverType);
		if (request_irq(
			u4IrqId ,
			(irq_handler_t)video_intr_dlr2,
			IRQF_TRIGGER_LOW,
			VCODEC_DEVNAME, NULL) < 0)	{
			MODULE_MFV_LOGE("[VCODEC_LOCKHW] ENC [MFV_DEBUG][ERROR] error to request enc irq\n");
		} else{
			MODULE_MFV_LOGD("[VCODEC_LOCKHW] ENC [MFV_DEBUG] success to request enc irq\n");
		}
	}

#else
	enable_irq(u4IrqId);
#endif

	MODULE_MFV_LOGD("venc_enableIRQ-\n");
	return 0;
}


static int vcodec_remove(struct platform_device *pDev)
{
	/*pm_runtime_disable(&pvenc_dev->dev);
	pm_runtime_disable(&pvenclt_dev->dev);*/
	MODULE_MFV_LOGD("vcodec_remove\n");
	return 0;
}

#ifdef CONFIG_OF
/* VDEC main device */
static const struct of_device_id vcodec_of_ids[] = {
	{.compatible = "mediatek,mt8173-vdec_gcon",},
	{}
};

static struct platform_driver VCodecDriver = {
	.probe = vcodec_probe,
	.remove = vcodec_remove,
	.driver = {
		   .name = VCODEC_DEVNAME,
		   .owner = THIS_MODULE,
		   .of_match_table = vcodec_of_ids,
		   }
};


/* Venc main device */
/*static const struct of_device_id venc_of_ids[] = {
	{.compatible = "mediatek,mt8173-venc",},
	{}
};*/
/*
static struct platform_driver VencDriver = {
	.probe = venc_probe,
	.driver = {
		   .name = VENC_DEVNAME,
		   .owner = THIS_MODULE,
		   .of_match_table = venc_of_ids,
		   }
};*/

/* Venclt main device */
/*static const struct of_device_id venclt_of_ids[] = {
	{.compatible = "mediatek,mt8173-venclt",},
	{}
};*/

/*static struct platform_driver VencltDriver = {
	.probe = venclt_probe,
	.driver = {
		   .name = VENCLT_DEVNAME,
		   .owner = THIS_MODULE,
		   .of_match_table = venclt_of_ids,
		   }
};*/
#endif

#ifdef CONFIG_MTK_HIBERNATION
/* extern void mt_irq_set_sens(unsigned int irq, unsigned int sens); */
/* extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity); */
static int vcodec_pm_restore_noirq(struct device *device)
{
	/* vdec : IRQF_TRIGGER_LOW */
	mt_irq_set_sens(VDEC_IRQ_ID, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(VDEC_IRQ_ID, MT_POLARITY_LOW);
	/* venc: IRQF_TRIGGER_LOW */
	mt_irq_set_sens(VENC_IRQ_ID, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(VENC_IRQ_ID, MT_POLARITY_LOW);

	return 0;
}
#endif

static int __init vcodec_driver_init(void)
{
	VAL_RESULT_T eValHWLockRet;
	VAL_ULONG_T ulFlags, ulFlagsLockHW, ulFlagsISR;
	struct device_node *node = NULL;

	MODULE_MFV_LOGD("+vcodec_init !!\n");

	mutex_lock(&DriverOpenCountLock);
	MT8173Driver_Open_Count = 0;
	mutex_unlock(&DriverOpenCountLock);

	/* get VENC related */
	#ifdef CONFIG_ARCH_MT8163
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-venc");
	#else
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-venc");
	#endif
	KVA_VENC_BASE = (VAL_ULONG_T) of_iomap(node, 0);
	VENC_IRQ_ID = irq_of_parse_and_map(node, 0);
	KVA_VENC_IRQ_STATUS_ADDR = KVA_VENC_BASE + 0x05C;
	KVA_VENC_IRQ_ACK_ADDR = KVA_VENC_BASE + 0x060;
	KVA_VENC_SW_PAUSE = KVA_VENC_BASE + VENC_SW_PAUSE;
	KVA_VENC_SW_HRST_N = KVA_VENC_BASE + VENC_SW_HRST_N;

	#ifndef CONFIG_ARCH_MT8163
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-venclt");
	KVA_VENC_LT_BASE = (VAL_ULONG_T) of_iomap(node, 0);
	VENC_LT_IRQ_ID = irq_of_parse_and_map(node, 0);
	KVA_VENC_LT_IRQ_STATUS_ADDR = KVA_VENC_LT_BASE + 0x05C;
	KVA_VENC_LT_IRQ_ACK_ADDR = KVA_VENC_LT_BASE + 0x060;
	KVA_VENC_LT_SW_PAUSE = KVA_VENC_LT_BASE + VENC_SW_PAUSE;
	KVA_VENC_LT_SW_HRST_N = KVA_VENC_LT_BASE + VENC_SW_HRST_N;
	#endif

	{
		struct device_node *node = NULL;

		node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-vdec");
		KVA_VDEC_BASE = (VAL_ULONG_T) of_iomap(node, 0);
		VDEC_IRQ_ID = irq_of_parse_and_map(node, 0);
		KVA_VDEC_MISC_BASE = KVA_VDEC_BASE + 0x0000;
		KVA_VDEC_VLD_BASE = KVA_VDEC_BASE + 0x1000;
		KVA_VDEC_MC_BASE = KVA_VDEC_VLD_BASE + 0x1000;
	}
	{
		struct device_node *node = NULL;

		node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-vdec_gcon");
		KVA_VDEC_GCON_BASE = (VAL_ULONG_T) of_iomap(node, 0);
		MODULE_MFV_LOGD
		    ("[DeviceTree] KVA_VENC_BASE(0x%lx), KVA_VDEC_BASE(0x%lx), KVA_VDEC_GCON_BASE(0x%lx)",
		     KVA_VENC_BASE, KVA_VDEC_BASE, KVA_VDEC_GCON_BASE);
		MODULE_MFV_LOGD("[DeviceTree] VDEC_IRQ_ID(%d), VENC_IRQ_ID(%d)", VDEC_IRQ_ID, VENC_IRQ_ID);
	}
/* KVA_VENC_IRQ_STATUS_ADDR =    (VAL_ULONG_T)ioremap(VENC_IRQ_STATUS_addr, 4); */
/* KVA_VENC_IRQ_ACK_ADDR  = (VAL_ULONG_T)ioremap(VENC_IRQ_ACK_addr, 4); */

#ifdef VENC_PWR_FPGA
	KVA_VENC_CLK_CFG_0_ADDR = (VAL_ULONG_T) ioremap(CLK_CFG_0_addr, 4);
	KVA_VENC_CLK_CFG_4_ADDR = (VAL_ULONG_T) ioremap(CLK_CFG_4_addr, 4);
	KVA_VENC_PWR_ADDR = (VAL_ULONG_T) ioremap(VENC_PWR_addr, 4);
	KVA_VENCSYS_CG_SET_ADDR = (VAL_ULONG_T) ioremap(VENCSYS_CG_SET_addr, 4);
#endif

	spin_lock_irqsave(&LockDecHWCountLock, ulFlagsLockHW);
	gu4LockDecHWCount = 0;
	spin_unlock_irqrestore(&LockDecHWCountLock, ulFlagsLockHW);

	spin_lock_irqsave(&LockEncHWCountLock, ulFlagsLockHW);
	gu4LockEncHWCount = 0;
	spin_unlock_irqrestore(&LockEncHWCountLock, ulFlagsLockHW);

	spin_lock_irqsave(&DecISRCountLock, ulFlagsISR);
	gu4DecISRCount = 0;
	spin_unlock_irqrestore(&DecISRCountLock, ulFlagsISR);

	spin_lock_irqsave(&EncISRCountLock, ulFlagsISR);
	gu4EncISRCount = 0;
	spin_unlock_irqrestore(&EncISRCountLock, ulFlagsISR);

	mutex_lock(&VdecPWRLock);
	gu4VdecPWRCounter = 0;
	mutex_unlock(&VdecPWRLock);

	mutex_lock(&VencPWRLock);
	gu4VencPWRCounter = 0;
	mutex_unlock(&VencPWRLock);

	mutex_lock(&IsOpenedLock);
	if (VAL_FALSE == bIsOpened) {
		bIsOpened = VAL_TRUE;
#ifdef CONFIG_OF
		platform_driver_register(&VCodecDriver);
		/*platform_driver_register(&VencDriver);
		platform_driver_register(&VencltDriver);*/
#else
		vcodec_probe(NULL);
#endif
	}
	mutex_unlock(&IsOpenedLock);

	mutex_lock(&VdecHWLock);
	gu4VdecLockThreadId = 0;
	grVcodecDecHWLock.pvHandle = 0;
	grVcodecDecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
	grVcodecDecHWLock.rLockedTime.u4Sec = 0;
	grVcodecDecHWLock.rLockedTime.u4uSec = 0;
	mutex_unlock(&VdecHWLock);

	mutex_lock(&VencHWLock);
	grVcodecEncHWLock.pvHandle = 0;
	grVcodecEncHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
	grVcodecEncHWLock.rLockedTime.u4Sec = 0;
	grVcodecEncHWLock.rLockedTime.u4uSec = 0;
	mutex_unlock(&VencHWLock);

	/* MT8173_HWLockEvent part */
	mutex_lock(&DecHWLockEventTimeoutLock);
	DecHWLockEvent.pvHandle = "DECHWLOCK_EVENT";
	DecHWLockEvent.u4HandleSize = sizeof("DECHWLOCK_EVENT") + 1;
	DecHWLockEvent.u4TimeoutMs = 1;
	mutex_unlock(&DecHWLockEventTimeoutLock);
	eValHWLockRet = eVideoCreateEvent(&DecHWLockEvent, sizeof(VAL_EVENT_T));
	if (VAL_RESULT_NO_ERROR != eValHWLockRet)
		MODULE_MFV_LOGE("[MFV][ERROR] create dec hwlock event error\n");

	mutex_lock(&EncHWLockEventTimeoutLock);
	EncHWLockEvent.pvHandle = "ENCHWLOCK_EVENT";
	EncHWLockEvent.u4HandleSize = sizeof("ENCHWLOCK_EVENT") + 1;
	EncHWLockEvent.u4TimeoutMs = 1;
	mutex_unlock(&EncHWLockEventTimeoutLock);
	eValHWLockRet = eVideoCreateEvent(&EncHWLockEvent, sizeof(VAL_EVENT_T));
	if (VAL_RESULT_NO_ERROR != eValHWLockRet)
		MODULE_MFV_LOGE("[MFV][ERROR] create enc hwlock event error\n");
	/* MT8173_IsrEvent part */
	spin_lock_irqsave(&DecIsrLock, ulFlags);
	DecIsrEvent.pvHandle = "DECISR_EVENT";
	DecIsrEvent.u4HandleSize = sizeof("DECISR_EVENT") + 1;
	DecIsrEvent.u4TimeoutMs = 1;
	spin_unlock_irqrestore(&DecIsrLock, ulFlags);
	eValHWLockRet = eVideoCreateEvent(&DecIsrEvent, sizeof(VAL_EVENT_T));
	if (VAL_RESULT_NO_ERROR != eValHWLockRet)
		MODULE_MFV_LOGE("[MFV][ERROR] create dec isr event error\n");

	spin_lock_irqsave(&EncIsrLock, ulFlags);
	EncIsrEvent.pvHandle = "ENCISR_EVENT";
	EncIsrEvent.u4HandleSize = sizeof("ENCISR_EVENT") + 1;
	EncIsrEvent.u4TimeoutMs = 1;
	spin_unlock_irqrestore(&EncIsrLock, ulFlags);
	eValHWLockRet = eVideoCreateEvent(&EncIsrEvent, sizeof(VAL_EVENT_T));
	if (VAL_RESULT_NO_ERROR != eValHWLockRet)
		MODULE_MFV_LOGE("[MFV][ERROR] create enc isr event error\n");

	MODULE_MFV_LOGD("[VCODEC_DEBUG] vcodec_driver_init Done\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&vcodec_early_suspend_handler);
#endif

#ifdef CONFIG_MTK_HIBERNATION
	register_swsusp_restore_noirq_func(ID_M_VCODEC, vcodec_pm_restore_noirq, NULL);
#endif

	return 0;
}

static void __exit vcodec_driver_exit(void)
{
	VAL_RESULT_T eValHWLockRet;

	MODULE_MFV_LOGD("[VCODEC_DEBUG] vcodec_driver_exit\n");

	mutex_lock(&IsOpenedLock);
	if (VAL_TRUE == bIsOpened) {
		MODULE_MFV_LOGD("+vcodec_driver_exit remove device !!\n");
#ifdef CONFIG_OF
		platform_driver_unregister(&VCodecDriver);

		/*cdev_del(venc_cdev);
		unregister_chrdev_region(venc_devno, 1);
		device_destroy(venc_class, venc_devno);
		class_destroy(venc_class);

		cdev_del(venclt_cdev);
		unregister_chrdev_region(venclt_devno, 1);
		device_destroy(venclt_class, venclt_devno);
		class_destroy(venclt_class);*/

		/*platform_driver_unregister(&VencDriver);
		platform_device_unregister(pvenc_dev);
		platform_driver_unregister(&VencltDriver);
		platform_device_unregister(pvenclt_dev);*/
#else
		bIsOpened = VAL_FALSE;
#endif
		MODULE_MFV_LOGD("+vcodec_driver_exit remove done !!\n");
	}
	mutex_unlock(&IsOpenedLock);

	cdev_del(vcodec_cdev);
	unregister_chrdev_region(vcodec_devno, 1);

	/* [TODO] iounmap the following? */
#if 0
	iounmap((void *)KVA_VENC_IRQ_STATUS_ADDR);
	iounmap((void *)KVA_VENC_IRQ_ACK_ADDR);
#endif
#ifdef VENC_PWR_FPGA
	iounmap((void *)KVA_VENC_CLK_CFG_0_ADDR);
	iounmap((void *)KVA_VENC_CLK_CFG_4_ADDR);
	iounmap((void *)KVA_VENC_PWR_ADDR);
	iounmap((void *)KVA_VENCSYS_CG_SET_ADDR);
#endif

	/* [TODO] free IRQ here */
	/* free_irq(MT_VENC_IRQ_ID, NULL); */
	free_irq(VENC_IRQ_ID, NULL);
	/* free_irq(MT_VDEC_IRQ_ID, NULL); */
	free_irq(VDEC_IRQ_ID, NULL);


	/* MT6589_HWLockEvent part */
	eValHWLockRet = eVideoCloseEvent(&DecHWLockEvent, sizeof(VAL_EVENT_T));
	if (VAL_RESULT_NO_ERROR != eValHWLockRet)
		MODULE_MFV_LOGE("[MFV][ERROR] close dec hwlock event error\n");

	eValHWLockRet = eVideoCloseEvent(&EncHWLockEvent, sizeof(VAL_EVENT_T));
	if (VAL_RESULT_NO_ERROR != eValHWLockRet)
		MODULE_MFV_LOGE("[MFV][ERROR] close enc hwlock event error\n");

	/* MT6589_IsrEvent part */
	eValHWLockRet = eVideoCloseEvent(&DecIsrEvent, sizeof(VAL_EVENT_T));
	if (VAL_RESULT_NO_ERROR != eValHWLockRet)
		MODULE_MFV_LOGE("[MFV][ERROR] close dec isr event error\n");

	eValHWLockRet = eVideoCloseEvent(&EncIsrEvent, sizeof(VAL_EVENT_T));
	if (VAL_RESULT_NO_ERROR != eValHWLockRet)
		MODULE_MFV_LOGE("[MFV][ERROR] close enc isr event error\n");
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&vcodec_early_suspend_handler);
#endif

#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_VCODEC);
#endif
}

module_init(vcodec_driver_init);
module_exit(vcodec_driver_exit);
MODULE_AUTHOR("Legis, Lu <legis.lu@mediatek.com>");
MODULE_DESCRIPTION("8173 Vcodec Driver");
MODULE_LICENSE("GPL");
