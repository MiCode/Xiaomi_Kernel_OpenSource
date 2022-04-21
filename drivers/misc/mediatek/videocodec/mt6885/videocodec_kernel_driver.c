/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
/* #include <mach/irqs.h> */
/* #include <mach/x_define_irq.h> */
#include <linux/wait.h>
#include <linux/proc_fs.h>
#include <linux/semaphore.h>
#include <mt-plat/dma.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include "mt-plat/sync_write.h"
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/pm_qos.h>
#include <mmdvfs_pmqos.h>

#ifndef CONFIG_MTK_CLKMGR
#include <linux/clk.h>
#else
#include "mach/mt_clkmgr.h"
#endif

#ifdef CONFIG_MTK_HIBERNATION
#include <mtk_hibernate_dpm.h>
#include <mach/diso.h>
#endif

#include "videocodec_kernel_driver.h"
#include "../videocodec_kernel.h"
#include "smi_public.h"
#include <asm/cacheflush.h>
#include <linux/io.h>
#include <asm/sizes.h>
#include "val_types_private.h"
#include "hal_types_private.h"
#include "val_api_private.h"
/*#include "val_log.h"*/
#include "drv_api.h"
#include "smi_public.h"
#ifdef CONFIG_MTK_QOS_SUPPORT
/* #define QOS_DEBUG  pr_debug */
#define QOS_DEBUG(...)
#define VCODEC_DVFS_V2
#else
#define QOS_DEBUG(...)
#endif
#ifdef VCODEC_DVFS_V2
#include <linux/slab.h>
#include "dvfs_v2.h"
#endif
#define DVFS_DEBUG(...)

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/uaccess.h>
#include <linux/compat.h>
#endif

#define VDO_HW_WRITE(ptr, data)     mt_reg_sync_writel(data, ptr)
#define VDO_HW_READ(ptr)            readl((void __iomem *)ptr)

#define VCODEC_DEVNAME     "Vcodec"
#define VCODEC_DEVNAME2     "Vcodec2"
#define VCODEC_DEV_MAJOR_NUMBER 160   /* 189 */
/* #define VENC_USE_L2C */

static dev_t vcodec_devno = MKDEV(VCODEC_DEV_MAJOR_NUMBER, 0);
static dev_t vcodec_devno2 = MKDEV(VCODEC_DEV_MAJOR_NUMBER, 1);
static struct cdev *vcodec_cdev;
static struct class *vcodec_class;
static struct device *vcodec_device;
struct pm_qos_request vcodec_qos_request;
struct pm_qos_request vcodec_qos_request2;
struct pm_qos_request vcodec_qos_request_f;
struct pm_qos_request vcodec_qos_request_f2;

static struct cdev *vcodec_cdev2;
static struct class *vcodec_class2;
static struct device *vcodec_device2;

#ifndef CONFIG_MTK_SMI_EXT
static struct clk *clk_MT_CG_SMI_COMMON;      /* MM_DISP0_SMI_COMMON */
static struct clk *clk_MT_CG_GALS_VDEC2MM;   /* CLK_MM_GALS_VDEC2MM */
static struct clk *clk_MT_CG_GALS_VENC2MM;   /* CLK_MM_GALS_VENC2MM */
#endif
static struct clk *clk_MT_CG_VDEC;            /* VDEC */

static struct clk *clk_MT_CG_VENC_VENC;         /* VENC_VENC */

static struct clk *clk_MT_SCP_SYS_VDE;          /* SCP_SYS_VDE */
static struct clk *clk_MT_SCP_SYS_VEN;          /* SCP_SYS_VEN */
static struct clk *clk_MT_SCP_SYS_DIS;          /* SCP_SYS_DIS */

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
static DEFINE_MUTEX(DecPMQoSLock);
static DEFINE_MUTEX(EncPMQoSLock);

static DEFINE_MUTEX(VdecPWRLock);
static DEFINE_MUTEX(VencPWRLock);
static DEFINE_MUTEX(LogCountLock);

static DEFINE_SPINLOCK(DecIsrLock);
static DEFINE_SPINLOCK(EncIsrLock);
static DEFINE_SPINLOCK(LockDecHWCountLock);
static DEFINE_SPINLOCK(LockEncHWCountLock);
static DEFINE_SPINLOCK(DecISRCountLock);
static DEFINE_SPINLOCK(EncISRCountLock);


static struct VAL_EVENT_T DecHWLockEvent;
	/* mutex : HWLockEventTimeoutLock */
static struct VAL_EVENT_T EncHWLockEvent;
	/* mutex : HWLockEventTimeoutLock */
static struct VAL_EVENT_T DecIsrEvent;    /* mutex : HWLockEventTimeoutLock */
static struct VAL_EVENT_T EncIsrEvent;    /* mutex : HWLockEventTimeoutLock */
static int Driver_Open_Count;         /* mutex : DriverOpenCountLock */
static unsigned int gu4PWRCounter;      /* mutex : PWRLock */
static unsigned int gu4EncEMICounter;   /* mutex : EncEMILock */
static unsigned int gu4DecEMICounter;   /* mutex : DecEMILock */
static unsigned int gu4L2CCounter;      /* mutex : L2CLock */
static char bIsOpened = VAL_FALSE;    /* mutex : IsOpenedLock */
static unsigned int gu4HwVencIrqStatus;
/* hardware VENC IRQ status (VP8/H264) */

static unsigned int gu4VdecPWRCounter;  /* mutex : VdecPWRLock */
static unsigned int gu4VencPWRCounter;  /* mutex : VencPWRLock */

static unsigned int gu4LogCountUser;  /* mutex : LogCountLock */
static unsigned int gu4LogCount;

static unsigned int gLockTimeOutCount;

static unsigned int gu4VdecLockThreadId;

static int gi4DecWaitEMI;

#define USE_WAKELOCK 0

#if USE_WAKELOCK == 1
static struct wake_lock vcodec_wake_lock;
static struct wake_lock vcodec_wake_lock2;
#elif USE_WAKELOCK == 0
static unsigned int is_entering_suspend;
#endif

/* #define VCODEC_DEBUG */
#ifdef VCODEC_DEBUG
#undef VCODEC_DEBUG
#define VCODEC_DEBUG pr_info
#undef pr_debug
#define pr_debug  pr_info
#else
#define VCODEC_DEBUG(...)
#undef pr_debug
#define pr_debug(...)
#endif

/* VENC physical base address */
#undef VENC_BASE
#define VENC_BASE       0x17020000
#define VENC_REGION     0x2000

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

#if 0
/* VDEC virtual base address */
#define VDEC_MISC_BASE  (VDEC_BASE + 0x0000)
#define VDEC_VLD_BASE   (VDEC_BASE + 0x1000)
#endif

#define DRAM_DONE_POLLING_LIMIT 20000

unsigned long KVA_VENC_IRQ_ACK_ADDR, KVA_VENC_IRQ_STATUS_ADDR, KVA_VENC_BASE;
unsigned long KVA_VDEC_MISC_BASE, KVA_VDEC_VLD_BASE;
unsigned long KVA_VDEC_BASE, KVA_VDEC_GCON_BASE;
unsigned int VENC_IRQ_ID, VDEC_IRQ_ID;


/* #define KS_POWER_WORKAROUND */

/* extern unsigned long pmem_user_v2p_video(unsigned long va); */

#if defined(VENC_USE_L2C)
/* extern int config_L2(int option); */
#endif

#define VCODEC_DEBUG_SYS
#ifdef VCODEC_DEBUG_SYS
#define vcodec_attr(_name) \
static struct kobj_attribute _name##_attr = {   \
	.attr   = {                                 \
		.name = __stringify(_name),             \
		.mode = 0644,                           \
	},                                          \
	.show   = _name##_show,                     \
	.store  = _name##_store,                    \
}

#include <linux/kobject.h>
#include <linux/sysfs.h>
static struct kobject *vcodec_debug_kobject;
static unsigned int vcodecDebugMode;
#endif

/* disable temporary for alaska DVT verification, smi seems not ready */
#define ENABLE_MMDVFS_VDEC
#ifdef ENABLE_MMDVFS_VDEC
/* <--- MM DVFS related */
#include <mtk_smi.h>
#include <mmdvfs_config_util.h>
#define DROP_PERCENTAGE     50
#define RAISE_PERCENTAGE    85
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
static unsigned int gMMDFVSCurrentVoltage = DVFS_UNREQUEST;
static int gVDECBWRequested;
static int gVENCBWRequested;
static int gVDECLevel;
static unsigned int gVDECFreq[2] = {450, 312};
static unsigned int gVDECFrmTRAVC[4] = {4, 6, 10, 3}; /* /3 for real ratio */
static unsigned int gVDECFrmTRHEVC[4] = {3, 5, 13, 3};
static unsigned int gVDECFrmTRMP2_4[5] = {5, 9, 10, 20, 3};
/* 3rd element for VP mode */
static u32 dec_step_size;
static u32 enc_step_size;
static u64 g_dec_freq_steps[MAX_FREQ_STEP];
static u64 g_enc_freq_steps[MAX_FREQ_STEP];
#ifdef VCODEC_DVFS_V2
static struct codec_history *dec_hists;
static struct codec_job *dec_jobs;
static DEFINE_MUTEX(VdecDVFSLock);
static struct codec_history *enc_hists;
static struct codec_job *enc_jobs;
static DEFINE_MUTEX(VencDVFSLock);
#endif

unsigned int TimeDiffMs(struct VAL_TIME_T timeOld, struct VAL_TIME_T timeNew)
{
	/* pr_debug ("@@ timeOld(%d, %d), timeNew(%d, %d)", */
	/* timeOld.u4Sec, timeOld.u4uSec, timeNew.u4Sec, timeNew.u4uSec); */
	return ((((timeNew.u4Sec - timeOld.u4Sec) * 1000000)
		+ timeNew.u4uSec) - timeOld.u4uSec) / 1000;
}

/* raise/drop voltage */
void SendDvfsRequest(int level)
{
	int ret = 0;

	if (level == MMDVFS_VOLTAGE_LOW) {
		pr_debug("[VCODEC][MMDVFS_VDEC] (MMDVFS_FINE_STEP_OPP3)");
#ifdef CONFIG_MTK_SMI_EXT
		gVDECLevel = 1;
#endif
		gMMDFVSCurrentVoltage = MMDVFS_VOLTAGE_LOW;
	} else if (level == MMDVFS_VOLTAGE_HIGH) {
		pr_debug("[VCODEC][MMDVFS_VDEC] (MMDVFS_FINE_STEP_OPP0)");
#ifdef CONFIG_MTK_SMI_EXT
		gVDECLevel = 0;
#endif
		gMMDFVSCurrentVoltage = MMDVFS_VOLTAGE_HIGH;
	}  else if (level == DVFS_UNREQUEST) {
		pr_debug("[VCODEC][MMDVFS_VDEC] (MMDVFS_FINE_STEP_UNREQUEST)");
		gMMDFVSCurrentVoltage = DVFS_UNREQUEST;
	} else {
		pr_debug("[VCODEC][MMDVFS_VDEC] OOPS: level = %d\n", level);
	}

	if (ret != 0)
		pr_debug("[VCODEC][MMDVFS_VDEC] OOPS: mmdvfs_set_fine_step error!");
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
	if (_diff > gHWLockMaxDuration)
		gHWLockMaxDuration = _diff;

	gHWLockInterval += (_diff + SW_OVERHEAD_MS);
	return _diff;
}

void VdecDvfsAdjustment(void)
{
	unsigned int _monitor_duration = 0;
	unsigned int _diff = 0;
	unsigned int _perc = 0;

	if (gMMDFVFSMonitorStarts == VAL_TRUE &&
		gMMDFVFSMonitorCounts > MONITOR_START_MINUS_1) {
		_monitor_duration = VdecDvfsGetMonitorDuration();
		if (_monitor_duration < MONITOR_DURATION_MS) {
			_diff = VdecDvfsStep();
			pr_debug("[VCODEC][MMDVFS_VDEC] lock time(%d ms, %d ms), cnt=%d, _monitor_duration=%d\n",
				_diff, gHWLockInterval,
				gMMDFVFSMonitorCounts, _monitor_duration);
		} else {
			VdecDvfsStep();
			_perc = (unsigned int)
				(100 * gHWLockInterval / _monitor_duration);
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

		if (_diff > PAUSE_DETECTION_GAP &&
			_diff > gHWLockPrevInterval * PAUSE_DETECTION_RATIO) {
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

void *mt_venc_base_get(void)
{
	return (void *)KVA_VENC_BASE;
}
EXPORT_SYMBOL(mt_venc_base_get);

void *mt_vdec_base_get(void)
{
	return (void *)KVA_VDEC_BASE;
}
EXPORT_SYMBOL(mt_vdec_base_get);

void vdec_polling_status(void)
{
	unsigned int u4DataStatusMain = 0;
	unsigned int u4DataStatus = 0;
	unsigned int u4CgStatus = 0;
	unsigned int u4Counter = 0;

	u4CgStatus = VDO_HW_READ(KVA_VDEC_GCON_BASE);
	u4DataStatusMain = VDO_HW_READ(KVA_VDEC_VLD_BASE+(61*4));

	while ((u4CgStatus != 0) && (u4DataStatusMain & (1<<15)) &&
		((u4DataStatusMain & 1) != 1)) {
		gi4DecWaitEMI = 1;
		u4CgStatus = VDO_HW_READ(KVA_VDEC_GCON_BASE);
		u4DataStatusMain = VDO_HW_READ(KVA_VDEC_VLD_BASE+(61*4));
		if (u4Counter++ > DRAM_DONE_POLLING_LIMIT) {
			unsigned int u4IntStatus = 0;
			unsigned int i = 0;

			pr_debug("[VCODEC][ERROR] Leftover data access before powering down\n");
			for (i = 45; i < 72; i++) {
				if (i == 45 || i == 46 || i == 52 ||
					i == 58 || i == 59 || i == 61 ||
					i == 62 || i == 63 || i == 71){
					u4IntStatus =
					VDO_HW_READ(KVA_VDEC_VLD_BASE+(i*4));
					pr_debug("[VCODEC][DUMP] VLD_%d = %x\n",
						i, u4IntStatus);
				}
			}

			for (i = 66; i < 80; i++) {
				u4DataStatus =
					VDO_HW_READ(KVA_VDEC_MISC_BASE+(i*4));
				pr_debug("[VCODEC][DUMP] MISC_%d = %x\n",
					i, u4DataStatus);
			}

			/* smi_debug_bus_hanging_detect_ext2(0x1FF, 1, 0, 1); */
			/*mmsys_cg_check(); */

			u4Counter = 0;
			WARN_ON(1);
		}
	}
	gi4DecWaitEMI = 0;
	/* pr_debug("u4Counter %d\n", u4Counter); */
}

void vdec_power_on(void)
{
	int ret = 0;

	mutex_lock(&VdecPWRLock);
	gu4VdecPWRCounter++;
	mutex_unlock(&VdecPWRLock);
	ret = 0;

#ifdef CONFIG_MTK_QOS_SUPPORT
#ifndef VCODEC_DVFS_V2
	mutex_lock(&DecPMQoSLock);
	QOS_DEBUG("[PMQoS] set to (0,1) %d, freq = %llu",
		gVDECLevel, g_dec_freq_steps[gVDECLevel]);
	pm_qos_update_request(&vcodec_qos_request_f,
		g_dec_freq_steps[gVDECLevel]);
	mutex_unlock(&DecPMQoSLock);
#endif
#endif

	ret = clk_prepare_enable(clk_MT_SCP_SYS_DIS);
	if (ret) {
		/* print error log & error handling */
		pr_debug("[VCODEC][ERROR] SCP_SYS_DIS not enabled %d\n",
			ret);
	}

	smi_bus_prepare_enable(SMI_LARB1, "VDEC");

	ret = clk_prepare_enable(clk_MT_SCP_SYS_VDE);
	if (ret) {
		/* print error log & error handling */
		pr_debug("[VCODEC][ERROR] SCP_SYS_VDE not enabled %d\n",
			ret);
	}

	ret = clk_prepare_enable(clk_MT_CG_VDEC);
	if (ret) {
		/* print error log & error handling */
		pr_debug("[VCODEC][ERROR] CG_VDEC not enabled %d\n",
			ret);
	}
}

#ifdef VCODEC_DEBUG_SYS
static ssize_t vcodec_debug_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
#ifdef ENABLE_MMDVFS_VDEC
	unsigned int _monitor_duration = 0;
	unsigned int _perc = 0;

	if (gMMDFVFSMonitorStarts == VAL_TRUE &&
		gMMDFVFSMonitorCounts > MONITOR_START_MINUS_1) {
		_monitor_duration = VdecDvfsGetMonitorDuration();
		_perc = (unsigned int)
			(100 * gHWLockInterval / _monitor_duration);
		return sprintf(buf,
			"[MMDVFS_VDEC] drop_th=%d, raise_th=%d, vol=%d, percent=%d\n",
			DROP_PERCENTAGE, RAISE_PERCENTAGE,
			gMMDFVSCurrentVoltage, _perc);
	} else {
		return sprintf(buf,
			"End of monitoring interval. Please try again.\n");
	}
#else
	return sprintf(buf, "Not profiling(%d).\n", vcodecDebugMode);
#endif
}

static ssize_t vcodec_debug_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (sscanf(buf, "%du", &vcodecDebugMode) == 1) {
		/* Add one line comment to meet coding style */
		pr_debug("[VCODEC][%s] Input is stored\n", __func__);
	}
	return count;
}

vcodec_attr(vcodec_debug);

#endif

void vdec_power_off(void)
{
	mutex_lock(&VdecPWRLock);
	if (gu4VdecPWRCounter == 0) {
		pr_debug("[VCODEC] gu4VdecPWRCounter = 0\n");
	} else {
		vdec_polling_status();
		gu4VdecPWRCounter--;
		clk_disable_unprepare(clk_MT_CG_VDEC);
		clk_disable_unprepare(clk_MT_SCP_SYS_VDE);

		smi_bus_disable_unprepare(SMI_LARB1, "VDEC");

		clk_disable_unprepare(clk_MT_SCP_SYS_DIS);
	}
	mutex_unlock(&VdecPWRLock);
	mutex_lock(&DecPMQoSLock);
#ifdef CONFIG_MTK_QOS_SUPPORT
	QOS_DEBUG("[PMQoS] %s reset to 0", __func__);
	pm_qos_update_request(&vcodec_qos_request, 0);
	gVDECBWRequested = 0;
#endif
#ifdef VCODEC_DVFS_V2
	pm_qos_update_request(&vcodec_qos_request_f, 0);
#endif
	mutex_unlock(&DecPMQoSLock);
}

void venc_power_on(void)
{
	int ret = 0;

	mutex_lock(&VencPWRLock);
	gu4VencPWRCounter++;
	mutex_unlock(&VencPWRLock);
	ret = 0;

	pr_debug("[VCODEC] %s +\n", __func__);

	ret = clk_prepare_enable(clk_MT_SCP_SYS_DIS);
	if (ret) {
		/* print error log & error handling */
		pr_debug("[VCODEC][ERROR] SCP_SYS_DIS not enabled %d\n",
			ret);
	}

	smi_bus_prepare_enable(SMI_LARB4, "VENC");

	ret = clk_prepare_enable(clk_MT_SCP_SYS_VEN);
	if (ret) {
		/* print error log & error handling */
		pr_debug("[VCODEC][ERROR] SCP_SYS_VEN not enabled %d\n",
			ret);
	}

	ret = clk_prepare_enable(clk_MT_CG_VENC_VENC);
	if (ret) {
		/* print error log & error handling */
		pr_debug("[VCODEC][ERROR][%s] clk VENC not enabled %d\n",
			__func__, ret);
	}

	pr_debug("[VCODEC] %s -\n", __func__);
}

void venc_power_off(void)
{
	mutex_lock(&VencPWRLock);
	if (gu4VencPWRCounter == 0) {
		pr_debug("[VCODEC] gu4VencPWRCounter = 0\n");
	} else {
		gu4VencPWRCounter--;
		pr_debug("[VCODEC] %s +\n", __func__);

		clk_disable_unprepare(clk_MT_CG_VENC_VENC);
		clk_disable_unprepare(clk_MT_SCP_SYS_VEN);

		smi_bus_disable_unprepare(SMI_LARB4, "VENC");

		clk_disable_unprepare(clk_MT_SCP_SYS_DIS);

		pr_debug("[VCODEC] %s -\n", __func__);
	}
	mutex_unlock(&VencPWRLock);
	mutex_lock(&EncPMQoSLock);
#ifdef CONFIG_MTK_QOS_SUPPORT
	QOS_DEBUG("[PMQoS] %s reset to 0", __func__);
	pm_qos_update_request(&vcodec_qos_request2, 0);
	gVENCBWRequested = 0;
#endif
#ifdef VCODEC_DVFS_V2
	pm_qos_update_request(&vcodec_qos_request_f2, 0);
#endif
	mutex_unlock(&EncPMQoSLock);
}

void vdec_break(void)
{
	unsigned int i;

	/* Step 1: set vdec_break */
	VDO_HW_WRITE(KVA_VDEC_MISC_BASE + 64*4, 0x1);

	/* Step 2: monitor status vdec_break_ok */
	for (i = 0; i < 5000; i++) {
		if ((VDO_HW_READ(KVA_VDEC_MISC_BASE + 65*4) & 0x11) == 0x11)
			break;
	}

	if (i >= 5000) {
		unsigned int j;
		unsigned int u4DataStatus = 0;

		pr_info("[VCODEC][POTENTIAL ERROR] Leftover data access before powering down\n");

		for (j = 68; j < 80; j++) {
			u4DataStatus = VDO_HW_READ(KVA_VDEC_MISC_BASE+(j*4));
			pr_info("[VCODEC][DUMP] MISC_%d = 0x%08x",
				j, u4DataStatus);
		}
		u4DataStatus = VDO_HW_READ(KVA_VDEC_VLD_BASE+(45*4));
		pr_info("[VCODEC][DUMP] VLD_45 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_VLD_BASE+(46*4));
		pr_info("[VCODEC][DUMP] VLD_46 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_VLD_BASE+(52*4));
		pr_info("[VCODEC][DUMP] VLD_52 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_VLD_BASE+(58*4));
		pr_info("[VCODEC][DUMP] VLD_58 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_VLD_BASE+(59*4));
		pr_info("[VCODEC][DUMP] VLD_59 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_VLD_BASE+(61*4));
		pr_info("[VCODEC][DUMP] VLD_61 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_VLD_BASE+(62*4));
		pr_info("[VCODEC][DUMP] VLD_62 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_VLD_BASE+(63*4));
		pr_info("[VCODEC][DUMP] VLD_63 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_VLD_BASE+(71*4));
		pr_info("[VCODEC][DUMP] VLD_71 = 0x%08x", u4DataStatus);
		u4DataStatus = VDO_HW_READ(KVA_VDEC_MISC_BASE+(66*4));
		pr_info("[VCODEC][DUMP] MISC_66 = 0x%08x", u4DataStatus);
	}

	/* Step 3: software reset */
	VDO_HW_WRITE(KVA_VDEC_VLD_BASE + 66*4, 0x1);
	VDO_HW_WRITE(KVA_VDEC_VLD_BASE + 66*4, 0x0);
}

void venc_break(void)
{
	unsigned int i;
	unsigned long VENC_SW_PAUSE   = KVA_VENC_BASE + 0xAC;
	unsigned long VENC_IRQ_STATUS = KVA_VENC_BASE + 0x5C;
	unsigned long VENC_SW_HRST_N  = KVA_VENC_BASE + 0xA8;
	unsigned long VENC_IRQ_ACK    = KVA_VENC_BASE + 0x60;

	/* Step 1: raise pause hardware signal */
	VDO_HW_WRITE(VENC_SW_PAUSE, 0x1);

	/* Step 2: assume software can only */
	/* tolerate 5000 APB read time. */
	for (i = 0; i < 5000; i++) {
		if (VDO_HW_READ(VENC_IRQ_STATUS) & 0x10)
			break;
	}

	/* Step 3: Lower pause hardware signal */
	/* and lower software hard reset signal */
	if (i >= 5000) {
		VDO_HW_WRITE(VENC_SW_PAUSE, 0x0);
		VDO_HW_WRITE(VENC_SW_HRST_N, 0x0);
		VDO_HW_READ(VENC_SW_HRST_N);
	}

	/* Step 4: Lower software hard reset */
	/* signal and lower pause hardware signal */
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

int mt_vdec_runtime_suspend(struct device *dev)
{
	vdec_power_off();
	return 0;
}

int mt_vdec_runtime_resume(struct device *dev)
{
	vdec_power_on();
	return 0;
}

int mt_venc_runtime_suspend(struct device *dev)
{
	venc_power_off();
	return 0;
}

int mt_venc_runtime_resume(struct device *dev)
{
	venc_power_on();
	return 0;
}

void dec_isr(void)
{
	enum VAL_RESULT_T    eValRet;
	unsigned long     ulFlags, ulFlagsISR, ulFlagsLockHW;

	unsigned int u4TempDecISRCount = 0;
	unsigned int u4TempLockDecHWCount = 0;
	unsigned int u4CgStatus = 0;
	unsigned int u4DecDoneStatus = 0;

	u4CgStatus = VDO_HW_READ(KVA_VDEC_GCON_BASE);
	if ((u4CgStatus & 0x10) != 0) {
		pr_debug("[VCODEC][ERROR] DEC ISR active not 0x0(0x%08x)",
			u4CgStatus);
		return;
	}

	u4DecDoneStatus = VDO_HW_READ(KVA_VDEC_MISC_BASE+0xA4);
	if ((u4DecDoneStatus & (0x1 << 16)) != 0x10000) {
		pr_debug("[VCODEC][ERROR] DEC ISR done not 0x1(0x%08x)",
			u4DecDoneStatus);
		return;
	}

	spin_lock_irqsave(&DecISRCountLock, ulFlagsISR);
	gu4DecISRCount++;
	u4TempDecISRCount = gu4DecISRCount;
	spin_unlock_irqrestore(&DecISRCountLock, ulFlagsISR);

	spin_lock_irqsave(&LockDecHWCountLock, ulFlagsLockHW);
	u4TempLockDecHWCount = gu4LockDecHWCount;
	spin_unlock_irqrestore(&LockDecHWCountLock, ulFlagsLockHW);

	/* Clear interrupt */
	VDO_HW_WRITE(KVA_VDEC_MISC_BASE+41*4,
		VDO_HW_READ(KVA_VDEC_MISC_BASE + 41*4) | 0x11);
	VDO_HW_WRITE(KVA_VDEC_MISC_BASE+41*4,
		VDO_HW_READ(KVA_VDEC_MISC_BASE + 41*4) & ~0x10);


	spin_lock_irqsave(&DecIsrLock, ulFlags);
	eValRet = eVideoSetEvent(&DecIsrEvent, sizeof(struct VAL_EVENT_T));
	if (eValRet != VAL_RESULT_NO_ERROR)
		pr_debug("[VCODEC][ERROR] ISR set DecIsrEvent error\n");

	spin_unlock_irqrestore(&DecIsrLock, ulFlags);
}


void enc_isr(void)
{
	enum VAL_RESULT_T  eValRet;
	unsigned long ulFlagsISR, ulFlagsLockHW;


	unsigned int u4TempEncISRCount = 0;
	unsigned int u4TempLockEncHWCount = 0;
	/* ---------------------- */

	spin_lock_irqsave(&EncISRCountLock, ulFlagsISR);
	gu4EncISRCount++;
	u4TempEncISRCount = gu4EncISRCount;
	spin_unlock_irqrestore(&EncISRCountLock, ulFlagsISR);

	spin_lock_irqsave(&LockEncHWCountLock, ulFlagsLockHW);
	u4TempLockEncHWCount = gu4LockEncHWCount;
	spin_unlock_irqrestore(&LockEncHWCountLock, ulFlagsLockHW);

	if (grVcodecEncHWLock.pvHandle == 0) {
		pr_debug("[VCODEC][ERROR] NO one Lock Enc HW, please check!!\n");

		/* Clear all status */
		/* VDO_HW_WRITE(KVA_VENC_MP4_IRQ_ACK_ADDR, 1); */
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PAUSE);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SWITCH);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_DRAM);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SPS);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PPS);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_FRM);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_VPS);
		return;
	}

	if ((grVcodecEncHWLock.eDriverType ==
			VAL_DRIVER_TYPE_H264_ENC) ||
		(grVcodecEncHWLock.eDriverType ==
			VAL_DRIVER_TYPE_HEVC_ENC)) { /* hardwire */
		gu4HwVencIrqStatus = VDO_HW_READ(KVA_VENC_IRQ_STATUS_ADDR);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PAUSE)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				VENC_IRQ_STATUS_PAUSE);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SWITCH)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				VENC_IRQ_STATUS_SWITCH);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_DRAM)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				VENC_IRQ_STATUS_DRAM);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SPS)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				VENC_IRQ_STATUS_SPS);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PPS)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				VENC_IRQ_STATUS_PPS);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_FRM)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				VENC_IRQ_STATUS_FRM);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_VPS)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				VENC_IRQ_STATUS_VPS);
	} else {
		pr_debug("[VCODEC][ERROR] Invalid lock holder driver type = %d\n",
			grVcodecEncHWLock.eDriverType);
	}

	eValRet = eVideoSetEvent(&EncIsrEvent, sizeof(struct VAL_EVENT_T));
	if (eValRet != VAL_RESULT_NO_ERROR)
		pr_debug("[VCODEC][ERROR] ISR set EncIsrEvent error\n");
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

static long vcodec_lockhw_dec_fail(struct VAL_HW_LOCK_T rHWLock,
		unsigned int FirstUseDecHW)
{
	pr_debug("[ERROR] VCODEC_LOCKHW Dec TimeOut tid %d\n",
		current->pid);
	if (FirstUseDecHW != 1) {
		mutex_lock(&VdecHWLock);
		if (grVcodecDecHWLock.pvHandle == 0)
			pr_debug("[WARNING] VCODEC_LOCKHW, maybe mediaserver restart before, please check!!\n");
		else {
			pr_debug("[WARNING] VCODEC_LOCKHW, someone use HW, and check timeout value!!\n");
			pr_debug("[WARNING] current owner = 0x%lx, tid = %u, wait EMI status = %d\n",
				(unsigned long)grVcodecDecHWLock.pvHandle,
				gu4VdecLockThreadId, gi4DecWaitEMI);
		}
		mutex_unlock(&VdecHWLock);
	}

	return 0;
}

static long vcodec_lockhw_enc_fail(struct VAL_HW_LOCK_T rHWLock,
		unsigned int FirstUseEncHW)
{
	pr_debug("[ERROR] VCODEC_LOCKHW Enc TimeOut tid %d\n",
		current->pid);

	if (FirstUseEncHW != 1) {
		mutex_lock(&VencHWLock);
		if (grVcodecEncHWLock.pvHandle == 0) {
			pr_debug("[WARNING] VCODEC_LOCKHW, maybe mediaserver restart before, please check!!\n");
		} else {
			pr_debug("[WARNING] VCODEC_LOCKHW, someone use HW, and check timeout value!! %d\n",
				 gLockTimeOutCount);
			++gLockTimeOutCount;
			if (gLockTimeOutCount > 30) {
				pr_debug("[ERROR] VCODEC_LOCKHW- ID %d fail\n",
					current->pid);
				pr_debug("someone locked HW time out more than 30 times 0x%lx,%lx,0x%lx,type:%d\n",
					(unsigned long)
					grVcodecEncHWLock.pvHandle,
					pmem_user_v2p_video(
					(unsigned long)rHWLock.pvHandle),
					(unsigned long)rHWLock.pvHandle,
					rHWLock.eDriverType);
				gLockTimeOutCount = 0;
				mutex_unlock(&VencHWLock);
				return -EFAULT;
			}

			if (rHWLock.u4TimeoutMs == 0) {
				pr_debug("[ERROR] VCODEC_LOCKHW- ID %d fail\n",
					current->pid);
				pr_debug("someone locked HW already 0x%lx,%lx,0x%lx,type:%d\n",
					(unsigned long)
					grVcodecEncHWLock.pvHandle,
					pmem_user_v2p_video(
					(unsigned long)rHWLock.pvHandle),
					(unsigned long)rHWLock.pvHandle,
					rHWLock.eDriverType);
				gLockTimeOutCount = 0;
				mutex_unlock(&VencHWLock);
				return -EFAULT;
			}
		}
		mutex_unlock(&VencHWLock);
	}

	return 0;
}

void vcodec_venc_pmqos(struct codec_job *enc_cur_job)
{
#ifdef VCODEC_DVFS_V2
	int target_freq;
	u64 target_freq_64;

	mutex_lock(&VencDVFSLock);
	if (enc_cur_job == 0) {
		target_freq_64 = match_freq(99999,
			&g_enc_freq_steps[0], enc_step_size);
		pm_qos_update_request(&vcodec_qos_request_f2,
			target_freq_64);
	} else {
		enc_cur_job->start = get_time_us();
		target_freq = est_freq(enc_cur_job->handle,
			&enc_jobs, enc_hists);
		target_freq_64 = match_freq(target_freq,
			&g_enc_freq_steps[0], enc_step_size);
		if (target_freq > 0) {
			enc_cur_job->mhz = (int)target_freq_64;
			pm_qos_update_request(
				&vcodec_qos_request_f2,
			target_freq_64);
		}
	}
	DVFS_DEBUG("enc_cur_job freq %llu", target_freq_64);
	mutex_unlock(&VencDVFSLock);
#endif
}

static long vcodec_lockhw(unsigned long arg)
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
	unsigned long handle_id = 0;
#if USE_WAKELOCK == 0
	unsigned int suspend_block_cnt = 0;
#endif
#ifdef VCODEC_DVFS_V2
	struct codec_job *dec_cur_job = 0;
	struct codec_job *enc_cur_job = 0;

	int target_freq;
	u64 target_freq_64;
#endif

	pr_debug("VCODEC_LOCKHW + tid = %d\n", current->pid);

	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&rHWLock,
		user_data_addr, sizeof(struct VAL_HW_LOCK_T));
	if (ret) {
		pr_debug("[ERROR] copy_from_user failed: %lu\n", ret);
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
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VP9_DEC) {

#ifdef VCODEC_DVFS_V2
		mutex_lock(&VdecDVFSLock);
		handle_id = pmem_user_v2p_video((unsigned long)rHWLock.pvHandle);
		if (handle_id == 0) {
			DVFS_DEBUG("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&VdecDVFSLock);
			return -1;
		}
		dec_cur_job = add_job((void *)handle_id, &dec_jobs);
		DVFS_DEBUG("dec_cur_job's handle %p", dec_cur_job->handle);
		mutex_unlock(&VdecDVFSLock);
#endif

		while (bLockedHW == VAL_FALSE) {
			mutex_lock(&DecHWLockEventTimeoutLock);
			if (DecHWLockEvent.u4TimeoutMs == 1) {
				pr_debug("VCODEC_LOCKHW, First Use Dec HW!!\n");
				FirstUseDecHW = 1;
			} else {
				FirstUseDecHW = 0;
			}
			mutex_unlock(&DecHWLockEventTimeoutLock);

			if (FirstUseDecHW == 1)
				eValRet = eVideoWaitEvent(&DecHWLockEvent,
					sizeof(struct VAL_EVENT_T));

			mutex_lock(&DecHWLockEventTimeoutLock);
			if (DecHWLockEvent.u4TimeoutMs != 1000) {
				DecHWLockEvent.u4TimeoutMs = 1000;
				FirstUseDecHW = 1;
			} else {
				FirstUseDecHW = 0;
			}
			mutex_unlock(&DecHWLockEventTimeoutLock);

			mutex_lock(&VdecHWLock);
			handle_id = pmem_user_v2p_video((unsigned long)rHWLock.pvHandle);
			if (handle_id == 0) {
				pr_debug("[error] handle is freed at %d\n", __LINE__);
				mutex_unlock(&VdecHWLock);
				return -1;
			}
			/* one process try to lock twice */
			if (grVcodecDecHWLock.pvHandle ==
				(void *)handle_id) {
				pr_debug("[WARNING] VCODEC_LOCKHW, one decoder instance try to lock twice\n");
				pr_debug("may cause lock HW timeout!! instance = 0x%lx, CurrentTID = %d\n",
				(unsigned long)grVcodecDecHWLock.pvHandle,
					current->pid);
			}
			mutex_unlock(&VdecHWLock);

			if (FirstUseDecHW == 0) {
				pr_debug("VCODEC_LOCKHW, Not first time use HW, timeout = %d\n",
					 DecHWLockEvent.u4TimeoutMs);
				eValRet = eVideoWaitEvent(&DecHWLockEvent,
					sizeof(struct VAL_EVENT_T));
			}

			if (eValRet == VAL_RESULT_INVALID_ISR) {
				ret = vcodec_lockhw_dec_fail(rHWLock,
					FirstUseDecHW);
				if (ret) {
					pr_debug("[ERROR] lockhw_dec_fail failed %lu\n",
						ret);
					return -EFAULT;
				}
			} else if (eValRet == VAL_RESULT_RESTARTSYS) {
				pr_debug("[WARNING] VCODEC_LOCKHW, VAL_RESULT_RESTARTSYS return when HWLock!!\n");
				return -ERESTARTSYS;
			}

			mutex_lock(&VdecHWLock);
			/* No one holds dec hw lock now */
			if (grVcodecDecHWLock.pvHandle == 0) {
#if USE_WAKELOCK == 1
				pr_debug("wake_lock(&vcodec_wake_lock) +");
				wake_lock(&vcodec_wake_lock);
				pr_debug("wake_lock(&vcodec_wake_lock) -");
#elif USE_WAKELOCK == 0
				while (is_entering_suspend == 1) {
					suspend_block_cnt++;
					if (suspend_block_cnt > 100000) {
						pr_debug("VCODEC_LOCKHW blocked by suspend flow for long time");
						suspend_block_cnt = 0;
					}
					msleep(20);
				}
#endif
				gu4VdecLockThreadId = current->pid;
				handle_id = pmem_user_v2p_video((unsigned long)rHWLock.pvHandle);
				if (handle_id == 0) {
					pr_debug("[error] handle is freed at %d\n", __LINE__);
					mutex_unlock(&VdecHWLock);
					return -1;
				}
				grVcodecDecHWLock.pvHandle =
					(void *)handle_id;
				grVcodecDecHWLock.eDriverType =
					rHWLock.eDriverType;
				eVideoGetTimeOfDay(
					&grVcodecDecHWLock.rLockedTime,
					sizeof(struct VAL_TIME_T));

				pr_debug("VCODEC_LOCKHW, No process use dec HW, so current process can use HW\n");
				pr_debug("LockInstance = 0x%lx CurrentTID = %d, rLockedTime(s, us) = %d, %d\n",
					 (unsigned long)
					 grVcodecDecHWLock.pvHandle,
					 current->pid,
					 grVcodecDecHWLock.rLockedTime.u4Sec,
					 grVcodecDecHWLock.rLockedTime.u4uSec);

				bLockedHW = VAL_TRUE;
#ifdef VCODEC_DVFS_V2
				mutex_lock(&VdecDVFSLock);
				if (dec_cur_job == 0) {
					target_freq_64 = match_freq(99999,
						&g_dec_freq_steps[0],
						dec_step_size);
					pm_qos_update_request(
						&vcodec_qos_request_f,
						target_freq_64);
				} else {
					dec_cur_job->start = get_time_us();
					target_freq =
						est_freq(dec_cur_job->handle,
							&dec_jobs, dec_hists);
					target_freq_64 = match_freq(target_freq,
						&g_dec_freq_steps[0],
						dec_step_size);
					if (target_freq > 0) {
						dec_cur_job->mhz =
							(int)target_freq_64;
						pm_qos_update_request(
							&vcodec_qos_request_f,
							target_freq_64);
					}
				}
				DVFS_DEBUG("dec_cur_job freq %llu",
					target_freq_64);
				mutex_unlock(&VdecDVFSLock);
#endif
#ifdef CONFIG_PM
				pm_runtime_get_sync(vcodec_device);
#else
#ifndef KS_POWER_WORKAROUND
				vdec_power_on();
#endif
#endif
				if (rHWLock.bSecureInst == VAL_FALSE)
					enable_irq(VDEC_IRQ_ID);

#ifdef ENABLE_MMDVFS_VDEC
				VdecDvfsMonitorStart();
#endif


			} else { /* Another one holding dec hw now */
				pr_debug("VCODEC_LOCKHW E\n");
				eVideoGetTimeOfDay(&rCurTime,
					sizeof(struct VAL_TIME_T));
				u4TimeInterval =
					(((((rCurTime.u4Sec -
					grVcodecDecHWLock.rLockedTime.u4Sec)
					* 1000000)
					+ rCurTime.u4uSec) -
					grVcodecDecHWLock.rLockedTime.u4uSec)
					/ 1000);

				pr_debug("VCODEC_LOCKHW, someone use dec HW, and check timeout value\n");
				pr_debug("TimeInterval(ms) = %d, TimeOutValue(ms)) = %d\n",
					 u4TimeInterval, rHWLock.u4TimeoutMs);
				pr_debug("Lock Instance = 0x%lx, Lock TID = %d, CurrentTID = %d\n",
					 (unsigned long)
					 grVcodecDecHWLock.pvHandle,
					 gu4VdecLockThreadId,
					 current->pid);
				pr_debug("rLockedTime(%d s, %d us), rCurTime(%d s, %d us)\n",
					grVcodecDecHWLock.rLockedTime.u4Sec,
					grVcodecDecHWLock.rLockedTime.u4uSec,
					rCurTime.u4Sec, rCurTime.u4uSec);

			}
			mutex_unlock(&VdecHWLock);
			spin_lock_irqsave(&LockDecHWCountLock, ulFlagsLockHW);
			gu4LockDecHWCount++;
			spin_unlock_irqrestore(&LockDecHWCountLock,
				ulFlagsLockHW);
		}
	} else if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
		   rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC ||
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
				pr_debug("VCODEC_LOCKHW, First Use Enc HW %d!!\n",
					rHWLock.eDriverType);
				FirstUseEncHW = 1;
			} else {
				FirstUseEncHW = 0;
			}
			mutex_unlock(&EncHWLockEventTimeoutLock);
			if (FirstUseEncHW == 1)
				eValRet = eVideoWaitEvent(&EncHWLockEvent,
					sizeof(struct VAL_EVENT_T));

			mutex_lock(&EncHWLockEventTimeoutLock);
			if (EncHWLockEvent.u4TimeoutMs == 1) {
				EncHWLockEvent.u4TimeoutMs = 1000;
				FirstUseEncHW = 1;
			} else {
				FirstUseEncHW = 0;
				if (rHWLock.u4TimeoutMs == 0)
					EncHWLockEvent.u4TimeoutMs = 0;
				else
					EncHWLockEvent.u4TimeoutMs = 1000;

			}
			mutex_unlock(&EncHWLockEventTimeoutLock);

			mutex_lock(&VencHWLock);
			handle_id = pmem_user_v2p_video((unsigned long)rHWLock.pvHandle);
			if (handle_id == 0) {
				pr_debug("[error] handle is freed at %d\n", __LINE__);
				mutex_unlock(&VencHWLock);
				return -1;
			}
			/* one process try to lock twice */
			if (grVcodecEncHWLock.pvHandle ==
			    (void *)handle_id) {
				pr_debug("[WARNING] VCODEC_LOCKHW, one encoder instance try to lock twice\n");
				pr_debug("may cause lock HW timeout!! instance=0x%lx, CurrentTID=%d, type:%d\n",
					(unsigned long)
					grVcodecEncHWLock.pvHandle,
					current->pid, rHWLock.eDriverType);
			}
			mutex_unlock(&VencHWLock);

			if (FirstUseEncHW == 0)
				eValRet = eVideoWaitEvent(&EncHWLockEvent,
					sizeof(struct VAL_EVENT_T));

			if (eValRet == VAL_RESULT_INVALID_ISR) {
				ret = vcodec_lockhw_enc_fail(rHWLock,
					FirstUseEncHW);
				if (ret) {
					pr_debug("[ERROR] lockhw_enc_fail failed %lu\n",
						ret);
					return -EFAULT;
				}
			} else if (eValRet == VAL_RESULT_RESTARTSYS) {
				return -ERESTARTSYS;
			}

			mutex_lock(&VencHWLock);
			/* No process use HW, so current process can use HW */
			if (grVcodecEncHWLock.pvHandle == 0) {
				if (rHWLock.eDriverType ==
						VAL_DRIVER_TYPE_H264_ENC ||
					rHWLock.eDriverType ==
						VAL_DRIVER_TYPE_HEVC_ENC ||
					rHWLock.eDriverType ==
						VAL_DRIVER_TYPE_JPEG_ENC) {
#ifdef VCODEC_DVFS_V2
					mutex_lock(&VencDVFSLock);
					handle_id = pmem_user_v2p_video(
					(unsigned long)rHWLock.pvHandle);
					if (handle_id == 0) {
						DVFS_DEBUG("[error] handle is freed at %d\n",
						__LINE__);
						mutex_unlock(&VencDVFSLock);
						mutex_unlock(&VencHWLock);
						return -1;
					}
					enc_cur_job =
					add_job(
					(void *)handle_id,
					&enc_jobs);
					DVFS_DEBUG("enc_cur_job's handle %p",
						enc_cur_job->handle);
					mutex_unlock(&VencDVFSLock);
#endif
#if USE_WAKELOCK == 1
					pr_debug("wake_lock(&vcodec_wake_lock2) +");
					wake_lock(&vcodec_wake_lock2);
					pr_debug("wake_lock(&vcodec_wake_lock2) -");
#elif USE_WAKELOCK == 0
					while (is_entering_suspend == 1)
						msleep(20);
#endif
					handle_id = pmem_user_v2p_video(
					(unsigned long)rHWLock.pvHandle);
					if (handle_id == 0) {
						pr_debug("[error] handle is freed at %d\n",
						__LINE__);
						mutex_unlock(&VencHWLock);
						return -1;
					}
					grVcodecEncHWLock.pvHandle =
					(void *)handle_id;
					grVcodecEncHWLock.eDriverType =
						rHWLock.eDriverType;
					eVideoGetTimeOfDay(
						&grVcodecEncHWLock.rLockedTime,
						sizeof(struct VAL_TIME_T));

					pr_debug("VCODEC_LOCKHW, No process use HW, so current process can use HW\n");
					pr_debug("VCODEC_LOCKHW, handle = 0x%lx\n",
						(unsigned long)
						grVcodecEncHWLock.pvHandle);
					pr_debug("LockInstance = 0x%lx CurrentTID = %d, rLockedTime(s, us) = %d, %d\n",
					(unsigned long)
					grVcodecEncHWLock.pvHandle,
					current->pid,
					grVcodecEncHWLock.rLockedTime.u4Sec,
					grVcodecEncHWLock.rLockedTime.u4uSec);

					bLockedHW = VAL_TRUE;
					if (rHWLock.eDriverType ==
						VAL_DRIVER_TYPE_H264_ENC ||
						rHWLock.eDriverType ==
						VAL_DRIVER_TYPE_HEVC_ENC) {
						vcodec_venc_pmqos(enc_cur_job);
#ifdef CONFIG_PM
						pm_runtime_get_sync(
							vcodec_device2);
#else
#ifndef KS_POWER_WORKAROUND
						venc_power_on();
#endif
#endif
						enable_irq(VENC_IRQ_ID);
					}
				}
			} else { /* someone use HW, and check timeout value */
				if (rHWLock.u4TimeoutMs == 0) {
					bLockedHW = VAL_FALSE;
					mutex_unlock(&VencHWLock);
					break;
				}

				eVideoGetTimeOfDay(&rCurTime,
					sizeof(struct VAL_TIME_T));
				u4TimeInterval =
					(((((rCurTime.u4Sec -
					grVcodecEncHWLock.rLockedTime.u4Sec)
					* 1000000)
					+ rCurTime.u4uSec) -
					grVcodecEncHWLock.rLockedTime.u4uSec)
					/ 1000);

				pr_debug("VCODEC_LOCKHW, someone use enc HW, and check timeout value\n");
				pr_debug("TimeInterval(ms) = %d, TimeOutValue(ms) = %d\n",
					 u4TimeInterval, rHWLock.u4TimeoutMs);
				pr_debug("rLockedTime(s, us) = %d, %d, rCurTime(s, us) = %d, %d\n",
					 grVcodecEncHWLock.rLockedTime.u4Sec,
					 grVcodecEncHWLock.rLockedTime.u4uSec,
					 rCurTime.u4Sec, rCurTime.u4uSec);
				pr_debug("LockInstance = 0x%lx, CurrentInstance = 0x%lx, CurrentTID = %d\n",
					 (unsigned long)
					 grVcodecEncHWLock.pvHandle,
					 pmem_user_v2p_video(
					 (unsigned long)rHWLock.pvHandle),
					 current->pid);

				++gLockTimeOutCount;
				if (gLockTimeOutCount > 30) {
					pr_debug("[ERROR] VCODEC_LOCKHW %d fail,someone locked HW over 30 times\n",
						 current->pid);
					pr_debug("without timeout 0x%lx,%lx,0x%lx,type:%d\n",
						(unsigned long)
						grVcodecEncHWLock.pvHandle,
						pmem_user_v2p_video(
						(unsigned long)
						rHWLock.pvHandle),
						(unsigned long)
						rHWLock.pvHandle,
						rHWLock.eDriverType);
					gLockTimeOutCount = 0;
					mutex_unlock(&VencHWLock);
					return -EFAULT;
				}
			}

			if (bLockedHW == VAL_TRUE) {
				pr_debug("VCODEC_LOCKHW, Lock ok grVcodecEncHWLock.pvHandle = 0x%lx, va:%lx, type:%d\n",
					 (unsigned long)
					 grVcodecEncHWLock.pvHandle,
					 (unsigned long)rHWLock.pvHandle,
					 rHWLock.eDriverType);
				gLockTimeOutCount = 0;
			}
			mutex_unlock(&VencHWLock);
		}

		if (bLockedHW == VAL_FALSE) {
			pr_debug("[ERROR] VCODEC_LOCKHW %d fail,someone locked HW already,0x%lx,%lx,0x%lx,type:%d\n",
				 current->pid,
				 (unsigned long)grVcodecEncHWLock.pvHandle,
				 pmem_user_v2p_video(
				 (unsigned long)rHWLock.pvHandle),
				 (unsigned long)rHWLock.pvHandle,
				 rHWLock.eDriverType);
			gLockTimeOutCount = 0;
			return -EFAULT;
		}

		spin_lock_irqsave(&LockEncHWCountLock, ulFlagsLockHW);
		gu4LockEncHWCount++;
		spin_unlock_irqrestore(&LockEncHWCountLock, ulFlagsLockHW);

		pr_debug("get locked - ObjId =%d\n", current->pid);

		pr_debug("VCODEC_LOCKHWed - tid = %d\n", current->pid);
	} else {
		pr_debug("[WARNING] VCODEC_LOCKHW Unknown instance\n");
		return -EFAULT;
	}

	pr_debug("VCODEC_LOCKHW - tid = %d\n", current->pid);

	return 0;
}

static long vcodec_unlockhw(unsigned long arg)
{
	unsigned char *user_data_addr;
	struct VAL_HW_LOCK_T rHWLock;
	enum VAL_RESULT_T eValRet;
	long ret;
	unsigned long handle_id = 0;
#ifdef VCODEC_DVFS_V2
	struct codec_job *dec_cur_job;
	struct codec_job *enc_cur_job;
#endif

	pr_debug("VCODEC_UNLOCKHW + tid = %d\n", current->pid);

	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&rHWLock,
		user_data_addr, sizeof(struct VAL_HW_LOCK_T));
	if (ret) {
		pr_debug("[ERROR] copy_from_user failed: %lu\n", ret);
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
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VP9_DEC) {
		mutex_lock(&VdecHWLock);
		handle_id = pmem_user_v2p_video((unsigned long)rHWLock.pvHandle);
		if (handle_id == 0) {
			pr_debug("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&VdecHWLock);
			return -1;
		}
		/* Current owner give up hw lock */
		if (grVcodecDecHWLock.pvHandle ==
			(void *)handle_id) {
#ifdef VCODEC_DVFS_V2
			mutex_lock(&VdecDVFSLock);
			dec_cur_job = dec_jobs;
			if (dec_cur_job->handle ==
				grVcodecDecHWLock.pvHandle) {
				dec_cur_job->end = get_time_us();
				update_hist(dec_cur_job, &dec_hists);
				dec_jobs = dec_jobs->next;
				kfree(dec_cur_job);
			} else {
				pr_info("VCODEC wrong job at dec done %p, %p",
					dec_cur_job->handle,
					grVcodecDecHWLock.pvHandle);
			}
			mutex_unlock(&VdecDVFSLock);
#endif
			if (rHWLock.bSecureInst == VAL_FALSE)
				disable_irq(VDEC_IRQ_ID);

			/* TODO: check if turning power off is ok */
#ifdef CONFIG_PM
			pm_runtime_put_sync(vcodec_device);
#else
#ifndef KS_POWER_WORKAROUND
			vdec_power_off();
#endif
#endif

#ifdef ENABLE_MMDVFS_VDEC
			VdecDvfsAdjustment();
#endif
			grVcodecDecHWLock.pvHandle = 0;
			grVcodecDecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
		} else { /* Not current owner */
			pr_debug("[ERROR] VCODEC_UNLOCKHW\n");
			pr_debug("Not owner trying to unlock dec hardware 0x%lx\n",
				 pmem_user_v2p_video(
				 (unsigned long)rHWLock.pvHandle));
			mutex_unlock(&VdecHWLock);
			return -EFAULT;
		}
#if USE_WAKELOCK == 1
		pr_debug("wake_unlock(&vcodec_wake_lock) +");
		wake_unlock(&vcodec_wake_lock);
		pr_debug("wake_unlock(&vcodec_wake_lock) -");
#endif
		mutex_unlock(&VdecHWLock);
		eValRet = eVideoSetEvent(&DecHWLockEvent,
			sizeof(struct VAL_EVENT_T));
	} else if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
			 rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC ||
			 rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC) {
		mutex_lock(&VencHWLock);
		handle_id = pmem_user_v2p_video((unsigned long)rHWLock.pvHandle);
		if (handle_id == 0) {
			pr_debug("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&VencHWLock);
			return -1;
		}
		/* Current owner give up hw lock */
		if (grVcodecEncHWLock.pvHandle ==
			(void *)handle_id) {
#ifdef VCODEC_DVFS_V2
			mutex_lock(&VencDVFSLock);
			enc_cur_job = enc_jobs;
			if (enc_cur_job->handle ==
				grVcodecEncHWLock.pvHandle) {
				enc_cur_job->end = get_time_us();
				update_hist(enc_cur_job, &enc_hists);
				enc_jobs = enc_jobs->next;
				kfree(enc_cur_job);
			} else {
				pr_info("VCODEC wrong job at dec done %p, %p",
					enc_cur_job->handle,
					grVcodecEncHWLock.pvHandle);
			}
			mutex_unlock(&VencDVFSLock);
#endif
			if (rHWLock.eDriverType ==
					VAL_DRIVER_TYPE_H264_ENC ||
				rHWLock.eDriverType ==
					VAL_DRIVER_TYPE_HEVC_ENC) {
				disable_irq(VENC_IRQ_ID);
				/* turn venc power off */
#ifdef CONFIG_PM
				pm_runtime_put_sync(vcodec_device2);
#else
#ifndef KS_POWER_WORKAROUND
				venc_power_off();
#endif
#endif
			}
			grVcodecEncHWLock.pvHandle = 0;
			grVcodecEncHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
		} else { /* Not current owner */
			/* [TODO] error handling */
			pr_debug("[ERROR] VCODEC_UNLOCKHW\n");
			pr_debug("Not owner trying to unlock enc hardware 0x%lx, pa:%lx, va:%lx type:%d\n",
				 (unsigned long)grVcodecEncHWLock.pvHandle,
				 pmem_user_v2p_video(
				 (unsigned long)rHWLock.pvHandle),
				 (unsigned long)rHWLock.pvHandle,
				 rHWLock.eDriverType);
			mutex_unlock(&VencHWLock);
			return -EFAULT;
			}
#if USE_WAKELOCK == 1
		pr_debug("wake_unlock(&vcodec_wake_lock2) +");
		wake_unlock(&vcodec_wake_lock2);
		pr_debug("wake_unlock(&vcodec_wake_lock2) -");
#endif
		mutex_unlock(&VencHWLock);
		eValRet = eVideoSetEvent(&EncHWLockEvent,
			sizeof(struct VAL_EVENT_T));
	} else {
		pr_debug("[WARNING] VCODEC_UNLOCKHW Unknown instance\n");
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
	long ret;
	enum VAL_RESULT_T eValRet;
	unsigned long handle_id = 0;

	pr_debug("VCODEC_WAITISR + tid = %d\n", current->pid);

	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&val_isr,
		user_data_addr, sizeof(struct VAL_ISR_T));
	if (ret) {
		pr_debug("[ERROR] copy_from_user failed: %lu\n", ret);
		return -EFAULT;
	}

	if (val_isr.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_VC1_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_VC1_ADV_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_VP8_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_VP9_DEC) {
		mutex_lock(&VdecHWLock);
		handle_id = pmem_user_v2p_video((unsigned long)val_isr.pvHandle);
		if (handle_id == 0) {
			pr_debug("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&VdecHWLock);
			return -1;
		}
		if (grVcodecDecHWLock.pvHandle ==
			(void *)handle_id)
			bLockedHW = VAL_TRUE;

		mutex_unlock(&VdecHWLock);

		if (bLockedHW == VAL_FALSE) {
			pr_debug("[ERROR] VCODEC_WAITISR, DO NOT have HWLock, so return fail\n");
			return -EFAULT;
		}

		spin_lock_irqsave(&DecIsrLock, ulFlags);
		DecIsrEvent.u4TimeoutMs = val_isr.u4TimeoutMs;
		spin_unlock_irqrestore(&DecIsrLock, ulFlags);

		eValRet = eVideoWaitEvent(&DecIsrEvent,
			sizeof(struct VAL_EVENT_T));
		if (eValRet == VAL_RESULT_INVALID_ISR) {
			return -2;
		} else if (eValRet == VAL_RESULT_RESTARTSYS) {
			pr_debug("[WARNING] VCODEC_WAITISR, VAL_RESULT_RESTARTSYS return when WAITISR!!\n");
			return -ERESTARTSYS;
		}
	} else if (val_isr.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
		   val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC) {
		mutex_lock(&VencHWLock);
		handle_id = pmem_user_v2p_video((unsigned long)val_isr.pvHandle);
		if (handle_id == 0) {
			pr_debug("[error] handle is freed at %d\n", __LINE__);
			mutex_unlock(&VencHWLock);
			return -1;
		}
		if (grVcodecEncHWLock.pvHandle ==
			(void *)handle_id)
			bLockedHW = VAL_TRUE;

		mutex_unlock(&VencHWLock);

		if (bLockedHW == VAL_FALSE) {
			pr_debug("[ERROR] VCODEC_WAITISR, DO NOT have enc HWLock, so return fail pa:%lx, va:%lx\n",
				 pmem_user_v2p_video(
				 (unsigned long)val_isr.pvHandle),
				 (unsigned long)val_isr.pvHandle);
			return -EFAULT;
		}

		spin_lock_irqsave(&EncIsrLock, ulFlags);
		EncIsrEvent.u4TimeoutMs = val_isr.u4TimeoutMs;
		spin_unlock_irqrestore(&EncIsrLock, ulFlags);

		eValRet = eVideoWaitEvent(&EncIsrEvent,
			sizeof(struct VAL_EVENT_T));
		if (eValRet == VAL_RESULT_INVALID_ISR) {
			return -2;
		} else if (eValRet == VAL_RESULT_RESTARTSYS) {
			pr_debug("[WARNING] RESTARTSYS return when WAITISR!\n");
			return -ERESTARTSYS;
		}

		if (val_isr.u4IrqStatusNum > 0) {
			val_isr.u4IrqStatus[0] = gu4HwVencIrqStatus;
			ret = copy_to_user(user_data_addr,
				&val_isr, sizeof(struct VAL_ISR_T));
			if (ret) {
				pr_debug("[ERROR] copy_to_user failed %lu\n",
					ret);
				return -EFAULT;
			}
		}
	} else {
		pr_debug("[WARNING] VCODEC_WAITISR Unknown instance\n");
		return -EFAULT;
	}

	pr_debug("VCODEC_WAITISR - tid = %d\n", current->pid);

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

	pr_debug("VCODEC_SET_FRAME_INFO + tid = %d\n", current->pid);
	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&rFrameInfo,
		user_data_addr, sizeof(struct VAL_FRAME_INFO_T));
	if (ret) {
		pr_debug("[ERROR] copy_from_user failed: %lu\n", ret);
		mutex_unlock(&LogCountLock);
		return -EFAULT;
	}

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

			if (dec_step_size > 1)
				b_freq_idx = dec_step_size - 1;

			/* 8bit * w * h * 1.5 * frame type ratio */
			/* freq ratio * decoding time relative to 1080p */
			emi_bw = 8L * 3 * g_dec_freq_steps[gVDECLevel]
				* 100 * 1920 * 1088;
			switch (rFrameInfo.driver_type) {
			case VAL_DRIVER_TYPE_H264_DEC:
				emi_bw = emi_bw * gVDECFrmTRAVC[frame_type] /
					(2 * 3 * gVDECFreq[b_freq_idx]);
				QOS_DEBUG("[PMQoS] AVC t%d R%d freq%d bw%ld",
					frame_type, gVDECFrmTRAVC[frame_type],
					gVDECFreq[gVDECLevel], emi_bw);
				break;
			case VAL_DRIVER_TYPE_HEVC_DEC:
				emi_bw = emi_bw * gVDECFrmTRHEVC[frame_type] /
					(2 * 3 * gVDECFreq[b_freq_idx]);
				QOS_DEBUG("[PMQoS] HEVC t%d R%d freq%d bw%ld",
					frame_type, gVDECFrmTRHEVC[frame_type],
					gVDECFreq[gVDECLevel], emi_bw);
				break;
			case VAL_DRIVER_TYPE_MP4_DEC:
			case VAL_DRIVER_TYPE_MP1_MP2_DEC:
				emi_bw = emi_bw * gVDECFrmTRMP2_4[frame_type] /
					(2 * 3 * gVDECFreq[b_freq_idx]);
				QOS_DEBUG("[PMQoS] MP2_4 t%d R%d freq%d bw%ld",
					frame_type, gVDECFrmTRMP2_4[frame_type],
					gVDECFreq[gVDECLevel], emi_bw);
				break;
			default:
				QOS_DEBUG("[PMQoS] Unsupported decoder type");
			}

			if (rFrameInfo.is_compressed != 0)
				emi_bw = emi_bw * 6 / 10;
			QOS_DEBUG("[PMQoS Kernel] UFO %d, emi_bw %ld",
				rFrameInfo.is_compressed, emi_bw);

			/* input size */
			emi_bw +=
				8 * rFrameInfo.input_size * 100 * 1920 * 1088 /
				(rFrameInfo.frame_width *
				rFrameInfo.frame_height);

			QOS_DEBUG("[PMQoS] input_size %d, w %d h %d bw %ld",
				rFrameInfo.input_size, rFrameInfo.frame_width,
				rFrameInfo.frame_height, emi_bw);

			emi_bw = emi_bw / (1024*1024) / 8;
			/* bits/s to mbytes/s */
			QOS_DEBUG("[PMQoS Kernel] mbytes/s emi_bw %ld", emi_bw);
#ifdef CONFIG_MTK_QOS_SUPPORT
			pm_qos_update_request(&vcodec_qos_request, (int)emi_bw);
#endif
			gVDECBWRequested = 1;
		}
		mutex_unlock(&DecPMQoSLock);
	} else if (rFrameInfo.driver_type == VAL_DRIVER_TYPE_H264_ENC) {
		mutex_lock(&EncPMQoSLock);
		{
		if (rFrameInfo.frame_width * rFrameInfo.frame_height <
			1920*1088) {
			switch (rFrameInfo.frame_type) {
			case 1:
				emi_bw = 560; /* MB/s */
			break;
			case 0:
			default:
				emi_bw = 210; /* MB/s */
			break;
			}
		} else {
			switch (rFrameInfo.frame_type) {
			case 1:
				emi_bw = 1000; /* MB/s */
			break;
			case 0:
			default:
				emi_bw = 590; /* MB/s */
			break;
			}
		}
		QOS_DEBUG("[PMQoS Kernel] VENC mbytes/s emi_bw %ld", emi_bw);
#ifdef CONFIG_MTK_QOS_SUPPORT
		pm_qos_update_request(&vcodec_qos_request2, (int)emi_bw);
#endif
		gVENCBWRequested = 1;
		}
		mutex_unlock(&EncPMQoSLock);
	}
	pr_debug("VCODEC_SET_FRAME_INFO - tid = %d\n", current->pid);

	return 0;
}

static long vcodec_unlocked_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	long ret;
	unsigned char *user_data_addr;
	struct VAL_VCODEC_CORE_LOADING_T rTempCoreLoading;
	struct VAL_VCODEC_CPU_OPP_LIMIT_T rCpuOppLimit;
	int temp_nr_cpu_ids;
	struct VAL_POWER_T rPowerParam;
	char rIncLogCount;

#if 0
	VCODEC_DRV_CMD_QUEUE_T rDrvCmdQueue;
	P_VCODEC_DRV_CMD_T cmd_queue = VAL_NULL;
	unsigned int u4Size, uValue, nCount;
#endif

	switch (cmd) {
	case VCODEC_SET_THREAD_ID:
	{
	}
	break;

	case VCODEC_ALLOC_NON_CACHE_BUFFER:
	{
	}
	break;

	case VCODEC_FREE_NON_CACHE_BUFFER:
	{
	}
	break;

	case VCODEC_INC_DEC_EMI_USER:
	{
		pr_debug("VCODEC_INC_DEC_EMI_USER + tid = %d\n", current->pid);

		mutex_lock(&DecEMILock);
		gu4DecEMICounter++;
		pr_debug("[VCODEC] DEC_EMI_USER = %d\n", gu4DecEMICounter);
		user_data_addr = (unsigned char *)arg;
		ret = copy_to_user(user_data_addr,
			&gu4DecEMICounter, sizeof(unsigned int));
		if (ret) {
			pr_debug("[ERROR] copy_to_user failed: %lu\n", ret);
			mutex_unlock(&DecEMILock);
			return -EFAULT;
		}
		mutex_unlock(&DecEMILock);

#ifdef ENABLE_MMDVFS_VDEC
		/* MM DVFS related */
		/* pr_debug("[VCODEC][MMDVFS_VDEC] MM DVFS init\n"); */
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

		mutex_lock(&DecEMILock);
		gu4DecEMICounter--;
		pr_debug("[VCODEC] DEC_EMI_USER = %d\n", gu4DecEMICounter);
		user_data_addr = (unsigned char *)arg;
		ret = copy_to_user(user_data_addr,
			&gu4DecEMICounter, sizeof(unsigned int));
		if (ret) {
			pr_debug("[ERROR] copy_to_user failed: %lu\n", ret);
			mutex_unlock(&DecEMILock);
			return -EFAULT;
		}
#ifdef ENABLE_MMDVFS_VDEC
		/* MM DVFS related */
		/* pr_debug("[VCODEC][MMDVFS_VDEC] DEC_DEC_EMI MM DVFS\n"); */
		/* unrequest voltage */
		if (gu4DecEMICounter == 0) {
		/* Unrequest when all decoders exit */
			SendDvfsRequest(DVFS_UNREQUEST);
		}
#endif
		mutex_unlock(&DecEMILock);

		pr_debug("VCODEC_DEC_DEC_EMI_USER - tid = %d\n", current->pid);
	}
	break;

	case VCODEC_INC_ENC_EMI_USER:
	{
		pr_debug("VCODEC_INC_ENC_EMI_USER + tid = %d\n", current->pid);

		mutex_lock(&EncEMILock);
		gu4EncEMICounter++;
		pr_debug("[VCODEC] ENC_EMI_USER = %d\n", gu4EncEMICounter);
		user_data_addr = (unsigned char *)arg;
		ret = copy_to_user(user_data_addr,
			&gu4EncEMICounter, sizeof(unsigned int));
		if (ret) {
			pr_debug("[ERROR] copy_to_user failed: %lu\n", ret);
			mutex_unlock(&EncEMILock);
			return -EFAULT;
		}
		mutex_unlock(&EncEMILock);

		pr_debug("VCODEC_INC_ENC_EMI_USER - tid = %d\n", current->pid);
	}
	break;

	case VCODEC_DEC_ENC_EMI_USER:
	{
		pr_debug("VCODEC_DEC_ENC_EMI_USER + tid = %d\n", current->pid);

		mutex_lock(&EncEMILock);
		gu4EncEMICounter--;
		pr_debug("[VCODEC] DEC_EMI_USER = %d\n", gu4EncEMICounter);
		user_data_addr = (unsigned char *)arg;
		ret = copy_to_user(user_data_addr,
			&gu4EncEMICounter, sizeof(unsigned int));
		if (ret) {
			pr_debug("[ERROR] copy_to_user failed: %lu\n", ret);
			mutex_unlock(&EncEMILock);
			return -EFAULT;
		}
		mutex_unlock(&EncEMILock);

		pr_debug("DEC_ENC_EMI_USER - tid = %d\n", current->pid);
	}
	break;

	case VCODEC_LOCKHW:
	{
		ret = vcodec_lockhw(arg);
		if (ret) {
			pr_debug("[ERROR] VCODEC_LOCKHW failed! %lu\n", ret);
			return ret;
		}
	}
		break;

	case VCODEC_UNLOCKHW:
	{
		ret = vcodec_unlockhw(arg);
		if (ret) {
			pr_debug("[ERROR] VCODEC_UNLOCKHW failed! %lu\n", ret);
			return ret;
		}
	}
		break;

	case VCODEC_INC_PWR_USER:
	{
		pr_debug("VCODEC_INC_PWR_USER + tid = %d\n", current->pid);
		user_data_addr = (unsigned char *)arg;
		ret = copy_from_user(&rPowerParam,
			user_data_addr, sizeof(struct VAL_POWER_T));
		if (ret) {
			pr_debug("[ERROR] copy_from_user failed %lu\n", ret);
			return -EFAULT;
		}
		pr_debug("[VCODEC] INC_PWR_USER eDriverType %d\n",
			rPowerParam.eDriverType);
		mutex_lock(&L2CLock);

#ifdef VENC_USE_L2C
		if (rPowerParam.eDriverType == VAL_DRIVER_TYPE_H264_ENC) {
			gu4L2CCounter++;
			pr_debug("[VCODEC] INC_PWR_USER L2C counter %d\n",
				gu4L2CCounter);

			if (gu4L2CCounter == 1) {
				if (config_L2(0)) {
					pr_debug("[VCODEC][ERROR] Switch L2C size to 512K failed\n");
					mutex_unlock(&L2CLock);
					return -EFAULT;
				}
				pr_debug("[VCODEC] Switch L2C size to 512K successful\n");

			}
		}
#endif
		mutex_unlock(&L2CLock);
		pr_debug("VCODEC_INC_PWR_USER - tid = %d\n", current->pid);
	}
	break;

	case VCODEC_DEC_PWR_USER:
	{
		pr_debug("DEC_PWR_USER + tid = %d\n", current->pid);
		user_data_addr = (unsigned char *)arg;
		ret = copy_from_user(&rPowerParam,
			user_data_addr, sizeof(struct VAL_POWER_T));
		if (ret) {
			pr_debug("[ERROR] copy_from_user failed: %lu\n", ret);
			return -EFAULT;
		}
		pr_debug("[VCODEC] DEC_PWR_USER eDriverType %d\n",
			rPowerParam.eDriverType);

		mutex_lock(&L2CLock);

#ifdef VENC_USE_L2C
		if (rPowerParam.eDriverType == VAL_DRIVER_TYPE_H264_ENC) {
			gu4L2CCounter--;
			pr_debug("[VCODEC] DEC_PWR_USER L2C counter %d\n",
				gu4L2CCounter);

			if (gu4L2CCounter == 0) {
				if (config_L2(1)) {
					pr_debug("[VCODEC][ERROR] Switch L2C size to 0K failed\n");
					mutex_unlock(&L2CLock);
					return -EFAULT;
				}
				pr_debug("[VCODEC] Switch L2C size to 0K successful\n");
			}
		}
#endif
		mutex_unlock(&L2CLock);
		pr_debug("VCODEC_DEC_PWR_USER - tid = %d\n", current->pid);
	}
	break;

	case VCODEC_WAITISR:
	{
		ret = vcodec_waitisr(arg);
		if (ret) {
			pr_debug("[ERROR] VCODEC_WAITISR failed! %lu\n", ret);
			return ret;
		}
	}
	break;

	case VCODEC_INITHWLOCK:
	{
		pr_debug("VCODEC_INITHWLOCK+ tid %d\n", current->pid);
		pr_debug("VCODEC_INITHWLOCK- tid %d\n", current->pid);
	}
	break;

	case VCODEC_DEINITHWLOCK:
	{
		pr_debug("VCODEC_DEINITHWLOCK- tid %d\n", current->pid);
		pr_debug("VCODEC_DEINITHWLOCK- tid %d\n", current->pid);
	}
	break;

	case VCODEC_GET_CPU_LOADING_INFO:
	{
		unsigned char *user_data_addr;
		struct VAL_VCODEC_CPU_LOADING_INFO_T _temp = {0};

		pr_debug("GET_CPU_LOADING_INFO +\n");
		user_data_addr = (unsigned char *)arg;

		ret = copy_to_user(user_data_addr,
			&_temp, sizeof(struct VAL_VCODEC_CPU_LOADING_INFO_T));
		if (ret) {
			pr_debug("[ERROR] copy_to_user failed: %lu\n", ret);
			return -EFAULT;
		}

		pr_debug("GET_CPU_LOADING_INFO -\n");
	}
	break;

	case VCODEC_GET_CORE_LOADING:
	{
		pr_debug("GET_CORE_LOADING + tid %d\n", current->pid);

		user_data_addr = (unsigned char *)arg;
		ret = copy_from_user(&rTempCoreLoading,
			user_data_addr,
			sizeof(struct VAL_VCODEC_CORE_LOADING_T));
		if (ret) {
			pr_debug("[ERROR] copy_from_user failed %lu\n", ret);
			return -EFAULT;
		}
		if (rTempCoreLoading.CPUid > num_possible_cpus()) {
			pr_debug("[ERROR] CPUid(%d) > num_possible_cpus(%d)\n",
				rTempCoreLoading.CPUid, num_possible_cpus());
			return -EFAULT;
		}
		if (rTempCoreLoading.CPUid < 0) {
			pr_debug("[ERROR] rTempCoreLoading.CPUid(%d) < 0\n",
				rTempCoreLoading.CPUid);
			return -EFAULT;
		}

		ret = copy_to_user(user_data_addr,
			&rTempCoreLoading,
			sizeof(struct VAL_VCODEC_CORE_LOADING_T));
		if (ret) {
			pr_debug("[ERROR] copy_to_user failed %lu\n", ret);
			return -EFAULT;
		}
		pr_debug("GET_CORE_LOADING - tid %d\n", current->pid);
	}
	break;

	case VCODEC_GET_CORE_NUMBER:
	{
		pr_debug("VCODEC_GET_CORE_NUMBER + tid %d\n", current->pid);

		user_data_addr = (unsigned char *)arg;
		temp_nr_cpu_ids = nr_cpu_ids;
		ret = copy_to_user(user_data_addr,
			&temp_nr_cpu_ids, sizeof(int));
		if (ret) {
			pr_debug("[ERROR] copy_to_user failed: %lu\n", ret);
			return -EFAULT;
		}
		pr_debug("VCODEC_GET_CORE_NUMBER - tid %d\n", current->pid);
	}
	break;

	case VCODEC_SET_CPU_OPP_LIMIT:
	{
		pr_debug("SET_CPU_OPP_LIMIT tid %d\n", current->pid);
		user_data_addr = (unsigned char *)arg;
		ret = copy_from_user(&rCpuOppLimit,
			user_data_addr,
			sizeof(struct VAL_VCODEC_CPU_OPP_LIMIT_T));
		if (ret) {
			pr_debug("[ERROR] copy_from_user failed: %lu\n", ret);
			return -EFAULT;
		}
		pr_debug("+SET_CPU_OPP_LIMIT (%d %d %d) tid %d\n",
			rCpuOppLimit.limited_freq, rCpuOppLimit.limited_cpu,
			rCpuOppLimit.enable, current->pid);
		/* TODO: Check if cpu_opp_limit is available */
		/*
		 * ret = cpu_opp_limit(EVENT_VIDEO, rCpuOppLimit.limited_freq,
		 * rCpuOppLimit.limited_cpu, rCpuOppLimit.enable);
		 * // 0: PASS, other: FAIL
		 * if (ret) {
		 * pr_debug("[VCODEC][ERROR] cpu_opp_limit failed: %lu\n", ret);
		 *	return -EFAULT;
		 * }
		 */
		pr_debug("-SET_CPU_OPP_LIMIT tid %d, ret %lu\n",
			current->pid, ret);
		pr_debug(" [EMPTY] tid %d\n", current->pid);
	}
	break;

	case VCODEC_MB:
	{
		/* MB Reason: make sure register order is correct */
		mb();
	}
	break;

	case VCODEC_SET_LOG_COUNT:
	{
		pr_debug("VCODEC_SET_LOG_COUNT + tid = %d\n", current->pid);

		mutex_lock(&LogCountLock);
		user_data_addr = (unsigned char *)arg;
		ret = copy_from_user(&rIncLogCount,
			user_data_addr, sizeof(char));
		if (ret) {
			pr_debug("[ERROR] copy_from_user failed %lu\n", ret);
			mutex_unlock(&LogCountLock);
			return -EFAULT;
		}

		if (rIncLogCount == VAL_TRUE) {
			if (gu4LogCountUser == 0) {
				gu4LogCount = get_detect_count();
				set_detect_count(gu4LogCount + 100);
			}
			gu4LogCountUser++;
		} else {
			gu4LogCountUser--;
			if (gu4LogCountUser == 0) {
				set_detect_count(gu4LogCount);
				gu4LogCount = 0;
			}
		}
		mutex_unlock(&LogCountLock);

		pr_debug("VCODEC_SET_LOG_COUNT - tid = %d\n", current->pid);
	}
	break;

	case VCODEC_SET_FRAME_INFO:
	{
		ret = vcodec_set_frame_info(arg);
		if (ret) {
			pr_debug("[ERROR] SET_FRAME_INFO failed %lu\n", ret);
			return ret;
		}
	}
	break;
	default:
	{
		pr_debug("[ERROR] ioctl default case %u\n", cmd);
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
			struct VAL_ISR_T __user *to =
				(struct VAL_ISR_T *)data;

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


static long vcodec_unlocked_compat_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
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

		err = compat_copy_struct(VAL_MEMORY_TYPE,
			COPY_FROM_USER, (void *)data32, (void *)data);
		if (err)
			return (long)err;

		ret = file->f_op->unlocked_ioctl(file,
			cmd, (unsigned long)data);

		err = compat_copy_struct(VAL_MEMORY_TYPE,
			COPY_TO_USER, (void *)data32, (void *)data);

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

		err = compat_copy_struct(VAL_HW_LOCK_TYPE,
			COPY_FROM_USER, (void *)data32, (void *)data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file,
			cmd, (unsigned long)data);

		err = compat_copy_struct(VAL_HW_LOCK_TYPE,
			COPY_TO_USER, (void *)data32, (void *)data);

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

		err = compat_copy_struct(VAL_POWER_TYPE,
			COPY_FROM_USER, (void *)data32, (void *)data);

		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file,
			cmd, (unsigned long)data);

		err = compat_copy_struct(VAL_POWER_TYPE,
			COPY_TO_USER, (void *)data32, (void *)data);

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

		err = compat_copy_struct(VAL_ISR_TYPE,
			COPY_FROM_USER, (void *)data32, (void *)data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file,
			VCODEC_WAITISR, (unsigned long)data);

		err = compat_copy_struct(VAL_ISR_TYPE,
			COPY_TO_USER, (void *)data32, (void *)data);

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

		err = compat_copy_struct(VAL_FRAME_INFO_TYPE, COPY_FROM_USER,
			(void *)data32, (void *)data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file,
			VCODEC_SET_FRAME_INFO, (unsigned long)data);

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

	mutex_lock(&DriverOpenCountLock);
	Driver_Open_Count++;

	pr_debug("%s pid = %d, Driver_Open_Count %d\n",
		__func__, current->pid, Driver_Open_Count);
	mutex_unlock(&DriverOpenCountLock);

	/* TODO: Check upper limit of concurrent users? */

	return 0;
}

static int vcodec_flush(struct file *file, fl_owner_t id)
{
	pr_debug("%s, curr_tid =%d\n",
		__func__, current->pid);
	pr_debug("%s pid = %d, Driver_Open_Count %d\n",
		__func__, current->pid, Driver_Open_Count);

	return 0;
}

static int vcodec_release(struct inode *inode, struct file *file)
{
	unsigned long ulFlagsLockHW, ulFlagsISR;
	void *pvCheckHandle = 0;

	/* dump_stack(); */
	pr_debug("%s, curr_tid =%d\n",
		__func__, current->pid);
	mutex_lock(&DriverOpenCountLock);
	pr_debug("%s pid = %d, Driver_Open_Count %d\n",
		__func__, current->pid, Driver_Open_Count);
	Driver_Open_Count--;

	if (Driver_Open_Count == 0) {
		mutex_lock(&VdecHWLock);
		gu4VdecLockThreadId = 0;
		pvCheckHandle = grVcodecDecHWLock.pvHandle;

		/* check if someone didn't unlockHW */
		if (grVcodecDecHWLock.pvHandle != 0) {
			pr_info("[ERROR] someone didn't unlockHW pid %d eDriverType %d pvHandle 0x%lx\n",
				current->pid, grVcodecDecHWLock.eDriverType,
				(unsigned long)grVcodecDecHWLock.pvHandle);

			/* power off */
			if (grVcodecDecHWLock.eDriverType ==
					VAL_DRIVER_TYPE_MP4_DEC ||
				grVcodecDecHWLock.eDriverType ==
					VAL_DRIVER_TYPE_HEVC_DEC ||
				grVcodecDecHWLock.eDriverType ==
					VAL_DRIVER_TYPE_H264_DEC ||
				grVcodecDecHWLock.eDriverType ==
					VAL_DRIVER_TYPE_MP1_MP2_DEC) {
				vdec_break();
				pr_debug("[WARNING] VCODEC_DEC release, reset power/irq!!\n");
#ifdef CONFIG_PM
				pm_runtime_put_sync(vcodec_device);
#else
#ifndef KS_POWER_WORKAROUND
				vdec_power_off();
#endif
#endif
				disable_irq(VDEC_IRQ_ID);
			}
		}

		grVcodecDecHWLock.pvHandle = 0;
		grVcodecDecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
		grVcodecDecHWLock.rLockedTime.u4Sec = 0;
		grVcodecDecHWLock.rLockedTime.u4uSec = 0;
		mutex_unlock(&VdecHWLock);
		if (pvCheckHandle != 0)
			eVideoSetEvent(&DecHWLockEvent,
				sizeof(struct VAL_EVENT_T));

		mutex_lock(&VencHWLock);
		pvCheckHandle = grVcodecEncHWLock.pvHandle;
		if (grVcodecEncHWLock.pvHandle != 0) {
			pr_info("[ERROR] someone didn't unlockHW pid %d eDriverType %d pvHandle 0x%lx\n",
				current->pid, grVcodecEncHWLock.eDriverType,
				(unsigned long)grVcodecEncHWLock.pvHandle);
			if (grVcodecEncHWLock.eDriverType ==
					VAL_DRIVER_TYPE_H264_ENC) {
				venc_break();
				pr_debug("[WARNING] VCODEC_ENC release, reset power/irq!!\n");
#ifdef CONFIG_PM
				pm_runtime_put_sync(vcodec_device2);
#else
#ifndef KS_POWER_WORKAROUND
				venc_power_off();
#endif
#endif
				disable_irq(VENC_IRQ_ID);
			}
		}
		grVcodecEncHWLock.pvHandle = 0;
		grVcodecEncHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
		grVcodecEncHWLock.rLockedTime.u4Sec = 0;
		grVcodecEncHWLock.rLockedTime.u4uSec = 0;
		mutex_unlock(&VencHWLock);
		if (pvCheckHandle != 0)
			eVideoSetEvent(&EncHWLockEvent,
				sizeof(struct VAL_EVENT_T));

		mutex_lock(&DecEMILock);
		gu4DecEMICounter = 0;
		mutex_unlock(&DecEMILock);

		mutex_lock(&EncEMILock);
		gu4EncEMICounter = 0;
		mutex_unlock(&EncEMILock);

		mutex_lock(&PWRLock);
		gu4PWRCounter = 0;
		mutex_unlock(&PWRLock);

#if defined(VENC_USE_L2C)
		mutex_lock(&L2CLock);
		if (gu4L2CCounter != 0) {
			pr_debug("vcodec_flush pid = %d, L2 user = %d, force restore L2 settings\n",
				 current->pid, gu4L2CCounter);
			if (config_L2(1))
				pr_debug("[VCODEC][ERROR] restore L2 settings failed\n");
		}
		gu4L2CCounter = 0;
		mutex_unlock(&L2CLock);
#endif
#ifdef VCODEC_DVFS_V2
		mutex_lock(&VdecDVFSLock);
		free_hist(&dec_hists, 0);
		mutex_unlock(&VdecDVFSLock);
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
		if (gMMDFVFSMonitorStarts == VAL_TRUE) {
			gMMDFVFSMonitorStarts = VAL_FALSE;
			gMMDFVFSMonitorCounts = 0;
			gHWLockInterval = 0;
			gHWLockMaxDuration = 0;
			SendDvfsRequest(DVFS_UNREQUEST);
		}
#endif
	}
	mutex_unlock(&DriverOpenCountLock);

	return 0;
}

void vcodec_vma_open(struct vm_area_struct *vma)
{
	pr_debug("vcodec VMA open, virt %lx, phys %lx\n",
		vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void vcodec_vma_close(struct vm_area_struct *vma)
{
	pr_debug("vcodec VMA close, virt %lx, phys %lx\n",
		vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

const struct vm_operations_struct vcodec_remap_vm_ops = {
	.open = vcodec_vma_open,
	.close = vcodec_vma_close,
};

static int vcodec_mmap(struct file *file, struct vm_area_struct *vma)
{
#if 1
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
		(pfn > INFO_BASE+INFO_REGION))
	) {
		unsigned long ulAddr, ulSize;

		for (u4I = 0; u4I < MULTI_INST_NUM_x_10; u4I++) {
			if ((grNCMemoryList[u4I].ulKVA != -1L) &&
				(grNCMemoryList[u4I].ulKPA != -1L)) {
				ulAddr = grNCMemoryList[u4I].ulKPA;
				ulSize =
				(grNCMemoryList[u4I].ulSize + 0x1000 - 1)
				& ~(0x1000 - 1);
				if ((length == ulSize) && (pfn == ulAddr)) {
					pr_debug("[VCODEC] c_idx %d\n", u4I);
					break;
				}
			}
		}

		if (u4I == MULTI_INST_NUM_x_10) {
			pr_debug("[VCODEC][ERROR] mmap region error: Length(0x%lx), pfn(0x%lx)\n",
				 (unsigned long)length, pfn);
			return -EAGAIN;
		}
	}
#endif
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	pr_debug("[VCODEC][mmap] vma->start 0x%lx, vma->end 0x%lx, vma->pgoff 0x%lx\n",
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

static const struct file_operations vcodec_fops = {
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

#if USE_WAKELOCK == 0
/**
 * Suspsend callbacks after user space processes are frozen
 * Since user space processes are frozen, there is no need and cannot hold same
 * mutex that protects lock owner while checking status.
 * If video codec hardware is still active now, must not aenter suspend.
 **/
static int vcodec_suspend(struct platform_device *pDev, pm_message_t state)
{
	if (grVcodecDecHWLock.pvHandle != 0 ||
		grVcodecEncHWLock.pvHandle != 0) {
		pr_debug("%s fail due to videocodec active", __func__);
		return -EBUSY;
	}
	pr_debug("%s ok", __func__);
	return 0;
}

static int vcodec_resume(struct platform_device *pDev)
{
	pr_debug("%s ok", __func__);
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

	pr_debug("%s ok action = %ld", __func__, action);
	switch (action) {
	case PM_SUSPEND_PREPARE:
		is_entering_suspend = 1;
		while (grVcodecDecHWLock.pvHandle != 0 ||
			grVcodecEncHWLock.pvHandle != 0) {
			wait_cnt++;
			if (wait_cnt > 90) {
				pr_debug("vcodec_pm_suspend waiting for vcodec inactive %p %p",
						grVcodecDecHWLock.pvHandle,
						grVcodecEncHWLock.pvHandle);
				return NOTIFY_DONE;
			}
			msleep(20);
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
#endif

#ifdef CONFIG_PM
static struct dev_pm_domain mt_vdec_pm_domain = {
	.ops = {
		SET_RUNTIME_PM_OPS(mt_vdec_runtime_suspend,
				mt_vdec_runtime_resume,
				NULL)
		}
};

static struct dev_pm_domain mt_venc_pm_domain = {
	.ops = {
		SET_RUNTIME_PM_OPS(mt_venc_runtime_suspend,
				mt_venc_runtime_resume,
				NULL)
		}
};
#endif

static int vcodec_probe(struct platform_device *dev)
{
	int ret;

	pr_debug("+%s\n", __func__);

	mutex_lock(&VdecPWRLock);
	gi4DecWaitEMI = 0;
	mutex_unlock(&VdecPWRLock);

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

	ret = register_chrdev_region(vcodec_devno, 1, VCODEC_DEVNAME);
	if (ret)
		pr_debug("[ERROR] Can't Get Major number for VCodec Device\n");

	vcodec_cdev = cdev_alloc();
	vcodec_cdev->owner = THIS_MODULE;
	vcodec_cdev->ops = &vcodec_fops;

	ret = cdev_add(vcodec_cdev, vcodec_devno, 1);
	if (ret)
		pr_debug("[ERROR] Can't add Vcodec Device\n");

	vcodec_class = class_create(THIS_MODULE, VCODEC_DEVNAME);
	if (IS_ERR(vcodec_class)) {
		ret = PTR_ERR(vcodec_class);
		pr_debug("[VCODEC][ERROR] create class fail %d", ret);
		return ret;
	}

	vcodec_device = device_create(vcodec_class,
			NULL, vcodec_devno,
			NULL, VCODEC_DEVNAME);
#ifdef CONFIG_PM
	vcodec_device->pm_domain = &mt_vdec_pm_domain;

	vcodec_cdev2 = cdev_alloc();
	vcodec_cdev2->owner = THIS_MODULE;
	vcodec_cdev2->ops = &vcodec_fops;

	ret = cdev_add(vcodec_cdev2, vcodec_devno2, 1);
	if (ret)
		pr_debug("[ERROR] Can't add Vcodec Device 2\n");

	vcodec_class2 = class_create(THIS_MODULE, VCODEC_DEVNAME2);
	if (IS_ERR(vcodec_class2)) {
		ret = PTR_ERR(vcodec_class2);
		pr_debug("[VCODEC][ERROR] create class 2 fail %d", ret);
		return ret;
	}

	vcodec_device2 = device_create(vcodec_class2,
			NULL, vcodec_devno2,
			NULL, VCODEC_DEVNAME2);
	vcodec_device2->pm_domain = &mt_venc_pm_domain;

	pm_runtime_enable(vcodec_device);
	pm_runtime_enable(vcodec_device2);
#endif

	if (request_irq(VDEC_IRQ_ID,
		(irq_handler_t)video_intr_dlr,
		IRQF_TRIGGER_LOW, VCODEC_DEVNAME, NULL) < 0)
		pr_debug("[VCODEC][ERROR] error to request dec irq\n");
	else
		pr_debug("[VCODEC] req success dec irq %d\n", VDEC_IRQ_ID);

	if (request_irq(VENC_IRQ_ID,
		(irq_handler_t)video_intr_dlr2,
		IRQF_TRIGGER_LOW, VCODEC_DEVNAME, NULL) < 0)
		pr_debug("[VCODEC][ERROR] error to request enc irq\n");
	else
		pr_debug("[VCODEC] req success enc irq %d\n", VENC_IRQ_ID);


	disable_irq(VDEC_IRQ_ID);
	disable_irq(VENC_IRQ_ID);

#if 0
#ifndef CONFIG_MTK_SMI_EXT
	clk_MT_CG_SMI_COMMON = devm_clk_get(&dev->dev, "MT_CG_SMI_COMMON");
	if (IS_ERR(clk_MT_CG_SMI_COMMON)) {
		pr_debug("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_SMI_COMMON\n");
		return PTR_ERR(clk_MT_CG_SMI_COMMON);
	}

	clk_MT_CG_GALS_VDEC2MM = devm_clk_get(&dev->dev, "MT_CG_GALS_VDEC2MM");
	if (IS_ERR(clk_MT_CG_GALS_VDEC2MM)) {
		pr_debug("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_GALS_VDEC2MM\n");
		return PTR_ERR(clk_MT_CG_GALS_VDEC2MM);
	}

	clk_MT_CG_GALS_VENC2MM = devm_clk_get(&dev->dev, "MT_CG_GALS_VENC2MM");
	if (IS_ERR(clk_MT_CG_GALS_VENC2MM)) {
		pr_debug("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_GALS_VENC2MM\n");
		return PTR_ERR(clk_MT_CG_GALS_VENC2MM);
	}
#endif
#endif

	clk_MT_CG_VDEC = devm_clk_get(&dev->dev, "MT_CG_VDEC");
	if (IS_ERR(clk_MT_CG_VDEC)) {
		pr_debug("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_VDEC\n");
		return PTR_ERR(clk_MT_CG_VDEC);
	}

	clk_MT_CG_VENC_VENC = devm_clk_get(&dev->dev, "MT_CG_VENC");
	if (IS_ERR(clk_MT_CG_VENC_VENC)) {
		pr_debug("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_VENC_VENC\n");
		return PTR_ERR(clk_MT_CG_VENC_VENC);
	}

	clk_MT_SCP_SYS_VDE = devm_clk_get(&dev->dev, "MT_SCP_SYS_VDE");
	if (IS_ERR(clk_MT_SCP_SYS_VDE)) {
		pr_debug("[VCODEC][ERROR] Unable to devm_clk_get MT_SCP_SYS_VDE\n");
		return PTR_ERR(clk_MT_SCP_SYS_VDE);
	}

	clk_MT_SCP_SYS_VEN = devm_clk_get(&dev->dev, "MT_SCP_SYS_VEN");
	if (IS_ERR(clk_MT_SCP_SYS_VEN)) {
		pr_debug("[VCODEC][ERROR] Unable to devm_clk_get MT_SCP_SYS_VEN\n");
		return PTR_ERR(clk_MT_SCP_SYS_VEN);
	}

	clk_MT_SCP_SYS_DIS = devm_clk_get(&dev->dev, "MT_SCP_SYS_DIS");
	if (IS_ERR(clk_MT_SCP_SYS_DIS)) {
		pr_debug("[VCODEC][ERROR] Unable to devm_clk_get MT_SCP_SYS_DIS\n");
		return PTR_ERR(clk_MT_SCP_SYS_DIS);
	}

#if USE_WAKELOCK == 0
	pm_notifier(vcodec_suspend_notifier, 0);
#endif

	pr_debug("-%s Done\n", __func__);

#ifdef KS_POWER_WORKAROUND
	vdec_power_on();
	venc_power_on();
#endif
#ifdef CONFIG_MTK_QOS_SUPPORT
	pm_qos_add_request(&vcodec_qos_request,
		PM_QOS_MM_MEMORY_BANDWIDTH, PM_QOS_DEFAULT_VALUE);
	pm_qos_add_request(&vcodec_qos_request2,
		PM_QOS_MM_MEMORY_BANDWIDTH, PM_QOS_DEFAULT_VALUE);
	pm_qos_add_request(&vcodec_qos_request_f,
		PM_QOS_VDEC_FREQ, PM_QOS_DEFAULT_VALUE);
	pm_qos_add_request(&vcodec_qos_request_f2,
		PM_QOS_VENC_FREQ, PM_QOS_DEFAULT_VALUE);
	snprintf(vcodec_qos_request.owner,
		sizeof(vcodec_qos_request.owner) - 1, "vdec_bw");
	snprintf(vcodec_qos_request2.owner,
		sizeof(vcodec_qos_request2.owner) - 1, "venc_bw");
	snprintf(vcodec_qos_request_f.owner,
		sizeof(vcodec_qos_request_f.owner) - 1, "vdec_freq");
	snprintf(vcodec_qos_request_f2.owner,
		sizeof(vcodec_qos_request_f2.owner) - 1, "venc_freq");

#endif

	dec_step_size = 1;
	enc_step_size = 1;
	ret = mmdvfs_qos_get_freq_steps(PM_QOS_VDEC_FREQ,
			&g_dec_freq_steps[0], &dec_step_size);
	if (ret < 0)
		pr_debug("Vdec get MMDVFS freq steps failed %d\n", ret);

	ret = mmdvfs_qos_get_freq_steps(PM_QOS_VENC_FREQ,
			&g_enc_freq_steps[0], &enc_step_size);
	if (ret < 0)
		pr_debug("Venc get MMDVFS freq steps failed %d\n", ret);
	return 0;
}

static int vcodec_remove(struct platform_device *pDev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

#ifdef CONFIG_MTK_HIBERNATION
/* extern void mt_irq_set_sens(unsigned int irq, unsigned int sens); */
/* extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity); */
static int vcodec_pm_restore_noirq(struct device *device)
{
	/* vdec: IRQF_TRIGGER_LOW */
	mt_irq_set_sens(VDEC_IRQ_ID, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(VDEC_IRQ_ID, MT_POLARITY_LOW);
	/* venc: IRQF_TRIGGER_LOW */
	mt_irq_set_sens(VENC_IRQ_ID, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(VENC_IRQ_ID, MT_POLARITY_LOW);

	return 0;
}
#endif

static const struct of_device_id vcodec_of_match[] = {
	{ .compatible = "mediatek,vdec_gcon", },
	{/* sentinel */}
};

MODULE_DEVICE_TABLE(of, vcodec_of_match);

static struct platform_driver vcodec_driver = {
	.probe = vcodec_probe,
	.remove = vcodec_remove,
#if USE_WAKELOCK == 0
	.suspend = vcodec_suspend,
	.resume = vcodec_resume,
#endif
	.driver = {
		.name  = VCODEC_DEVNAME,
		.owner = THIS_MODULE,
		.of_match_table = vcodec_of_match,
	},
};

static int __init vcodec_driver_init(void)
{
	enum VAL_RESULT_T  eValHWLockRet;
	unsigned long ulFlags, ulFlagsLockHW, ulFlagsISR;
	int error = 0;

	pr_debug("+%s !!\n", __func__);

	mutex_lock(&DriverOpenCountLock);
	Driver_Open_Count = 0;
#if USE_WAKELOCK == 1
	wake_lock_init(&vcodec_wake_lock,
		WAKE_LOCK_SUSPEND, "vcodec_wake_lock");
	pr_debug("wake_lock_init(&vcodec_wake_lock, WAKE_LOCK_SUSPEND, \"vcodec_wake_lock\")");
	wake_lock_init(&vcodec_wake_lock2,
		WAKE_LOCK_SUSPEND, "vcodec_wake_lock2");
	pr_debug("wake_lock_init(&vcodec_wake_lock2, WAKE_LOCK_SUSPEND, \"vcodec_wake_lock2\")");
#endif
	mutex_unlock(&DriverOpenCountLock);

	mutex_lock(&LogCountLock);
	gu4LogCountUser = 0;
	gu4LogCount = 0;
	mutex_unlock(&LogCountLock);

	{
		struct device_node *node = NULL;

		node = of_find_compatible_node(NULL, NULL, "mediatek,venc");
		KVA_VENC_BASE = (unsigned long)of_iomap(node, 0);
		VENC_IRQ_ID =  irq_of_parse_and_map(node, 0);
		KVA_VENC_IRQ_STATUS_ADDR =    KVA_VENC_BASE + 0x05C;
		KVA_VENC_IRQ_ACK_ADDR  = KVA_VENC_BASE + 0x060;
	}

	{
		struct device_node *node = NULL;

		node = of_find_compatible_node(NULL, NULL, "mediatek,vdec");
		KVA_VDEC_BASE = (unsigned long)of_iomap(node, 0);
		VDEC_IRQ_ID =  irq_of_parse_and_map(node, 0);
		KVA_VDEC_MISC_BASE = KVA_VDEC_BASE + 0x0000;
		KVA_VDEC_VLD_BASE = KVA_VDEC_BASE + 0x1000;
	}
	{
		struct device_node *node = NULL;

		node = of_find_compatible_node(NULL,
				NULL, "mediatek,vdec_gcon");
		KVA_VDEC_GCON_BASE = (unsigned long)of_iomap(node, 0);

		pr_debug("[VCODEC][DeviceTree] KVA_VENC_BASE(0x%lx), KVA_VDEC_BASE(0x%lx), KVA_VDEC_GCON_BASE(0x%lx)",
			 KVA_VENC_BASE, KVA_VDEC_BASE, KVA_VDEC_GCON_BASE);
		pr_debug("[VCODEC][DeviceTree] VDEC_IRQ_ID(%d), VENC_IRQ_ID(%d)",
			 VDEC_IRQ_ID, VENC_IRQ_ID);
	}

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
	if (bIsOpened == VAL_FALSE) {
		bIsOpened = VAL_TRUE;
		/* vcodec_probe(NULL); */
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

	/* HWLockEvent part */
	mutex_lock(&DecHWLockEventTimeoutLock);
	DecHWLockEvent.pvHandle = "DECHWLOCK_EVENT";
	DecHWLockEvent.u4HandleSize = sizeof("DECHWLOCK_EVENT")+1;
	DecHWLockEvent.u4TimeoutMs = 1;
	mutex_unlock(&DecHWLockEventTimeoutLock);
	eValHWLockRet = eVideoCreateEvent(&DecHWLockEvent,
		sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_debug("[VCODEC][ERROR] create dec hwlock event error\n");

	mutex_lock(&EncHWLockEventTimeoutLock);
	EncHWLockEvent.pvHandle = "ENCHWLOCK_EVENT";
	EncHWLockEvent.u4HandleSize = sizeof("ENCHWLOCK_EVENT")+1;
	EncHWLockEvent.u4TimeoutMs = 1;
	mutex_unlock(&EncHWLockEventTimeoutLock);
	eValHWLockRet = eVideoCreateEvent(&EncHWLockEvent,
		sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_debug("[VCODEC][ERROR] create enc hwlock event error\n");

#ifdef VCODEC_DVFS_V2
	mutex_lock(&VdecDVFSLock);
	dec_hists = 0;
	dec_jobs = 0;
	mutex_unlock(&VdecDVFSLock);
#endif

	/* IsrEvent part */
	spin_lock_irqsave(&DecIsrLock, ulFlags);
	DecIsrEvent.pvHandle = "DECISR_EVENT";
	DecIsrEvent.u4HandleSize = sizeof("DECISR_EVENT")+1;
	DecIsrEvent.u4TimeoutMs = 1;
	spin_unlock_irqrestore(&DecIsrLock, ulFlags);
	eValHWLockRet = eVideoCreateEvent(&DecIsrEvent,
		sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_debug("[VCODEC][ERROR] create dec isr event error\n");

	spin_lock_irqsave(&EncIsrLock, ulFlags);
	EncIsrEvent.pvHandle = "ENCISR_EVENT";
	EncIsrEvent.u4HandleSize = sizeof("ENCISR_EVENT")+1;
	EncIsrEvent.u4TimeoutMs = 1;
	spin_unlock_irqrestore(&EncIsrLock, ulFlags);
	eValHWLockRet = eVideoCreateEvent(&EncIsrEvent,
		sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_debug("[VCODEC][ERROR] create enc isr event error\n");

#if USE_WAKELOCK == 0
	is_entering_suspend = 0;
#endif

	pr_debug("-%s Done\n", __func__);

#ifdef CONFIG_MTK_HIBERNATION
	register_swsusp_restore_noirq_func(ID_M_VCODEC,
		vcodec_pm_restore_noirq, NULL);
#endif

#ifdef VCODEC_DEBUG_SYS
	vcodec_debug_kobject = kobject_create_and_add("vcodec", NULL);

	if (!vcodec_debug_kobject) {
		pr_debug("Faile to create and add vcodec kobject");
		return -ENOMEM;
	}

	error = sysfs_create_file(vcodec_debug_kobject,
			&vcodec_debug_attr.attr);
	if (error) {
		pr_debug("Faile to create and add vcodec_debug file in /sys/vcodec/");
		return error;
	}
#endif

	return platform_driver_register(&vcodec_driver);
}

static void __exit vcodec_driver_exit(void)
{
	enum VAL_RESULT_T  eValHWLockRet;

	pr_debug("%s\n", __func__);
#if USE_WAKELOCK == 1
	mutex_lock(&DriverOpenCountLock);
	wake_lock_destroy(&vcodec_wake_lock);
	pr_debug("wake_lock_destroy(&vcodec_wake_lock)");
	wake_lock_destroy(&vcodec_wake_lock2);
	pr_debug("wake_lock_destroy(&vcodec_wake_lock2)");
	mutex_unlock(&DriverOpenCountLock);
#endif


	mutex_lock(&IsOpenedLock);
	if (bIsOpened == VAL_TRUE)
		bIsOpened = VAL_FALSE;

	mutex_unlock(&IsOpenedLock);

	cdev_del(vcodec_cdev);
	unregister_chrdev_region(vcodec_devno, 1);
#ifdef CONFIG_PM
	cdev_del(vcodec_cdev2);
#endif
	/* [TODO] iounmap the following? */
#if 0
	iounmap((void *)KVA_VENC_IRQ_STATUS_ADDR);
	iounmap((void *)KVA_VENC_IRQ_ACK_ADDR);
#endif

	free_irq(VENC_IRQ_ID, NULL);
	free_irq(VDEC_IRQ_ID, NULL);

	/* MT6589_HWLockEvent part */
	eValHWLockRet = eVideoCloseEvent(&DecHWLockEvent,
			sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_debug("[VCODEC][ERROR] close dec hwlock event error\n");

	eValHWLockRet = eVideoCloseEvent(&EncHWLockEvent,
			sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_debug("[VCODEC][ERROR] close enc hwlock event error\n");

	/* MT6589_IsrEvent part */
	eValHWLockRet = eVideoCloseEvent(&DecIsrEvent,
			sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_debug("[VCODEC][ERROR] close dec isr event error\n");

	eValHWLockRet = eVideoCloseEvent(&EncIsrEvent,
			sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_debug("[VCODEC][ERROR] close enc isr event error\n");


#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_VCODEC);
#endif

#ifdef VCODEC_DEBUG_SYS
	kobject_put(vcodec_debug_kobject);
#endif

	platform_driver_unregister(&vcodec_driver);
}

module_init(vcodec_driver_init);
module_exit(vcodec_driver_exit);
MODULE_AUTHOR("Legis, Lu <legis.lu@mediatek.com>");
MODULE_DESCRIPTION("Vcodec Driver");
MODULE_LICENSE("GPL");
