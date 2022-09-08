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

#include <asm/cacheflush.h>
#include <linux/io.h>
#include "val_types_private.h"
#include "hal_types_private.h"
#include "val_api_private.h"
#include "drv_api.h"

#include "videocodec_kernel_driver.h"
#include "videocodec_kernel.h"
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

static dev_t vcodec_devno = MKDEV(VCODEC_DEV_MAJOR_NUMBER, 0);
static dev_t vcodec_devno2 = MKDEV(VCODEC_DEV_MAJOR_NUMBER, 1);


/* hardware VENC IRQ status(VP8/H264) */
unsigned int gu4HwVencIrqStatus;
const char *platform;
unsigned int gLockTimeOutCount;


unsigned int is_entering_suspend;

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

void *KVA_VENC_IRQ_ACK_ADDR, *KVA_VENC_IRQ_STATUS_ADDR, *KVA_VENC_BASE;
void *KVA_VDEC_MISC_BASE, *KVA_VDEC_VLD_BASE;
void *KVA_VDEC_BASE, *KVA_VDEC_GCON_BASE, *KVA_MBIST_BASE;
int gBistFlag;
int gDfvsFlag;
int gWakeLock;
struct mtk_vcodec_dev *gVCodecDev;
struct mtk_vcodec_drv_init_params *gDrvInitParams;
unsigned int VENC_IRQ_ID, VDEC_IRQ_ID;

/* #define KS_POWER_WORKAROUND */

/* disable temporary for alaska DVT verification, smi seems not ready */
#define ENABLE_MMDVFS_VDEC
#ifdef ENABLE_MMDVFS_VDEC
#define MIN_VDEC_FREQ 228
#define MIN_VENC_FREQ 228
#endif

unsigned int TimeDiffMs(struct VAL_TIME_T timeOld, struct VAL_TIME_T timeNew)
{
	/* pr_info ("@@ timeOld(%d, %d), timeNew(%d, %d)", */
	/* timeOld.u4Sec, timeOld.u4uSec, timeNew.u4Sec, timeNew.u4uSec); */
	return ((((timeNew.u4Sec - timeOld.u4Sec) * 1000000) + timeNew.u4uSec) -
		 timeOld.u4uSec) / 1000;
}

void *mt_venc_base_get(void)
{
	return (void *)KVA_VENC_BASE;
}

void *mt_vdec_base_get(void)
{
	return (void *)KVA_VDEC_BASE;
}

/**
 * Suspend callbacks after user space processes are frozen
 * Since user space processes are frozen, there is no need and cannot hold same
 * mutex that protects lock owner while checking status.
 * If video codec hardware is still active now, must not aenter suspend.
 **/
static int vcodec_suspend(struct platform_device *pDev, pm_message_t state)
{
	if (gWakeLock == 0) {
		if (CodecHWLock.pvHandle != 0) {
			pr_info("%s fail due to videocodec activity", __func__);
			return -EBUSY;
		}
		pr_debug("%s ok", __func__);
	}
	return 0;
}

static int vcodec_resume(struct platform_device *pDev)
{
	if (gWakeLock == 0)
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


static int vcodec_probe(struct platform_device *pdev)
{
	int ret;
	struct mtk_vcodec_dev *dev;

	pr_debug("+%s\n", __func__);

	mutex_lock(&gDrvInitParams->decEMILock);
	gDrvInitParams->u4DecEMICounter = 0;
	mutex_unlock(&gDrvInitParams->decEMILock);

	mutex_lock(&gDrvInitParams->encEMILock);
	gDrvInitParams->u4EncEMICounter = 0;
	mutex_unlock(&gDrvInitParams->encEMILock);

	mutex_lock(&gDrvInitParams->pwrLock);
	gDrvInitParams->u4PWRCounter = 0;
	mutex_unlock(&gDrvInitParams->pwrLock);

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	dev->plat_dev = pdev;
	KVA_VDEC_GCON_BASE = of_iomap(pdev->dev.of_node, 0);
	ret = register_chrdev_region(vcodec_devno, 1, VCODEC_DEVNAME);
	if (ret) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 */
		pr_info("[VCODEC] Can't Get Major number\n");
	}

	gDrvInitParams->vcodec_cdev = cdev_alloc();
	gDrvInitParams->vcodec_cdev->owner = THIS_MODULE;
	gDrvInitParams->vcodec_cdev->ops = &vcodec_fops;

	ret = cdev_add(gDrvInitParams->vcodec_cdev, vcodec_devno, 1);
	if (ret) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 */
		pr_info("[VCODEC] Can't add Vcodec Device\n");
	}

	gDrvInitParams->vcodec_class = class_create(THIS_MODULE, VCODEC_DEVNAME);
	if (IS_ERR(gDrvInitParams->vcodec_class)) {
		ret = PTR_ERR(gDrvInitParams->vcodec_class);
		pr_info("[VCODEC] Unable to create class err = %d ",
				 ret);
		return ret;
	}

	gDrvInitParams->vcodec_device =
		device_create(gDrvInitParams->vcodec_class, NULL, vcodec_devno, NULL,
					VCODEC_DEVNAME);
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
	dev->dec_irq = VDEC_IRQ_ID;
	dev->enc_irq = VENC_IRQ_ID;
	dev->dec_reg_base[VDEC_BASE] = KVA_VDEC_BASE;
	dev->enc_reg_base[VENC_BASE] = KVA_VENC_BASE;
	dev->enc_reg_base[VENC_GCON] = KVA_VDEC_GCON_BASE;
	ret = mtk_vcodec_irq_setup(pdev, dev);
	if (ret) {
		dev_info(&pdev->dev, "Failed to IRQ setup");
		return ret;
	}

	ret = mtk_vcodec_init_pm(dev);

	if (ret) {
		dev_info(&pdev->dev, "Failed to get mt vcodec clock source!");
		return ret;
	}
	pr_debug("%s ret : %d Done\n ", __func__, ret);

	if (gWakeLock == 0)
		pm_notifier(vcodec_suspend_notifier, 0);

	pr_debug("%s Done\n", __func__);
	if (gDfvsFlag == 1) {
		mtk_prepare_vdec_dvfs(dev);
		mtk_prepare_vdec_emi_bw(dev);
		if (dev->vdec_reg <= 0)
			pr_debug("Vdec get DVFS freq steps failed: %d\n", dev->vdec_reg);
		else if (dev->vdec_freq_cnt > 0 && dev->vdec_freq_cnt <= MAX_CODEC_FREQ_STEP)
			dev->dec_freq = dev->vdec_freqs[dev->vdec_freq_cnt - 1];
		else
			dev->dec_freq = MIN_VDEC_FREQ;
		mtk_prepare_venc_dvfs(dev);
		mtk_prepare_venc_emi_bw(dev);
		if (dev->venc_reg <= 0)
			pr_debug("Venc get DVFS freq steps failed: %d\n", dev->venc_reg);
		else if (dev->venc_freq_cnt > 0 && dev->venc_freq_cnt <= MAX_CODEC_FREQ_STEP)
			dev->enc_freq = dev->venc_freqs[dev->venc_freq_cnt - 1];
		else
			dev->enc_freq = MIN_VENC_FREQ;
	}
	gVCodecDev = dev;
	return 0;
}

static int vcodec_remove(struct platform_device *pDev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

#if IS_ENABLED(CONFIG_MTK_HIBERNATION)
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
	{ .compatible = "mediatek,venc_gcon", },
	{ .compatible = "mediatek,vdec_gcon", },
	{/* sentinel */}
};

MODULE_DEVICE_TABLE(of, vcodec_of_match);

static struct platform_driver vcodec_driver = {
	.probe = vcodec_probe,
	.remove = vcodec_remove,
	.suspend = vcodec_suspend,
	.resume = vcodec_resume,
	.driver = {
		.name  = VCODEC_DEVNAME,
		.owner = THIS_MODULE,
		.of_match_table = vcodec_of_match,
	},
};


static void mtk_platform_init_flgs(void)
{
	if (!strcmp(MTK_PLATFORM_MT6765, platform)) {
		gBistFlag = 0;
		gDfvsFlag = 1;
		gWakeLock = 0;
	} else if (!strcmp(MTK_PLATFORM_MT6761, platform)) {
		gBistFlag = 1;
		gDfvsFlag = 1;
		gWakeLock = 0;

	} else {
		gBistFlag = 0;
		gDfvsFlag = 0;
		gWakeLock = -1;

	}
}

static int __init vcodec_driver_init(void)
{
	enum VAL_RESULT_T  eValHWLockRet;
	unsigned long ulFlags, ulFlagsLockHW, ulFlagsISR;
	int ret;

	pr_debug("+%s !!\n", __func__);

	gDrvInitParams = kzalloc(sizeof(*gDrvInitParams), GFP_KERNEL);
	gDrvInitParams->bIsOpened = VAL_FALSE;
	mutex_init(&gDrvInitParams->driverOpenCountLock);
	mutex_init(&gDrvInitParams->logCountLock);
	mutex_init(&gDrvInitParams->vdecPWRLock);
	mutex_init(&gDrvInitParams->vencPWRLock);
	mutex_init(&gDrvInitParams->isOpenedLock);
	mutex_init(&gDrvInitParams->hwLock);
	mutex_init(&gDrvInitParams->hwLockEventTimeoutLock);
	mutex_init(&gDrvInitParams->pwrLock);
	mutex_init(&gDrvInitParams->encEMILock);
	mutex_init(&gDrvInitParams->decEMILock);


	spin_lock_init(&gDrvInitParams->lockDecHWCountLock);
	spin_lock_init(&gDrvInitParams->lockEncHWCountLock);
	spin_lock_init(&gDrvInitParams->decISRCountLock);
	spin_lock_init(&gDrvInitParams->encISRCountLock);
	spin_lock_init(&gDrvInitParams->decIsrLock);
	spin_lock_init(&gDrvInitParams->encIsrLock);

	mutex_lock(&gDrvInitParams->driverOpenCountLock);
	gDrvInitParams->drvOpenCount = 0;
	mutex_unlock(&gDrvInitParams->driverOpenCountLock);

	mutex_lock(&gDrvInitParams->logCountLock);
	gDrvInitParams->u4LogCountUser = 0;
	gDrvInitParams->u4LogCount = 0;
	mutex_unlock(&gDrvInitParams->logCountLock);

	{
		struct device_node *node = NULL;

		node = of_find_compatible_node(NULL, NULL, "mediatek,venc");
		ret = of_property_read_string(node, "mediatek,platform", &platform);
		if (ret < 0)
			pr_info("[VCODEC][ERROR] please must add platform entry in venc node\n");
		mtk_platform_init_flgs();
		KVA_VENC_BASE = of_iomap(node, 0);
		VENC_IRQ_ID =  irq_of_parse_and_map(node, 0);
		KVA_VENC_IRQ_STATUS_ADDR =    KVA_VENC_BASE + 0x05C;
		KVA_VENC_IRQ_ACK_ADDR  = KVA_VENC_BASE + 0x060;
	}

	{
		struct device_node *node = NULL;

		node = of_find_compatible_node(NULL, NULL, "mediatek,vdec");
		KVA_VDEC_BASE = of_iomap(node, 0);
		VDEC_IRQ_ID =  irq_of_parse_and_map(node, 0);
		KVA_VDEC_MISC_BASE = KVA_VDEC_BASE + 0x5000;
		KVA_VDEC_VLD_BASE = KVA_VDEC_BASE + 0x0000;
	}

	if (gBistFlag == 1) {
		{
			struct device_node *node = NULL;

			node = of_find_compatible_node(NULL, NULL, "mediatek,mbist");
			if (node != NULL)
				KVA_MBIST_BASE = of_iomap(node, 0);
		}
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

	mutex_lock(&gDrvInitParams->vdecPWRLock);
	gDrvInitParams->u4VdecPWRCounter = 0;
	mutex_unlock(&gDrvInitParams->vdecPWRLock);

	mutex_lock(&gDrvInitParams->vencPWRLock);
	gDrvInitParams->u4VencPWRCounter = 0;
	mutex_unlock(&gDrvInitParams->vencPWRLock);

	mutex_lock(&gDrvInitParams->isOpenedLock);
	if (gDrvInitParams->bIsOpened == VAL_FALSE)
		gDrvInitParams->bIsOpened = VAL_TRUE;
	mutex_unlock(&gDrvInitParams->isOpenedLock);

	mutex_lock(&gDrvInitParams->hwLock);
	gDrvInitParams->u4VdecLockThreadId = 0;
	CodecHWLock.pvHandle = 0;
	CodecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
	CodecHWLock.rLockedTime.u4Sec = 0;
	CodecHWLock.rLockedTime.u4uSec = 0;
	mutex_unlock(&gDrvInitParams->hwLock);

	/* HWLockEvent part */
	mutex_lock(&gDrvInitParams->hwLockEventTimeoutLock);
	gDrvInitParams->HWLockEvent.pvHandle = "VCODECHWLOCK_EVENT";
	gDrvInitParams->HWLockEvent.u4HandleSize = sizeof("VCODECHWLOCK_EVENT")+1;
	gDrvInitParams->HWLockEvent.u4TimeoutMs = 1;
	mutex_unlock(&gDrvInitParams->hwLockEventTimeoutLock);
	eValHWLockRet = eVideoCreateEvent(&gDrvInitParams->HWLockEvent,
					sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 */
		pr_info("[VCODEC][ERROR] create vcodec hwlock event error\n");
	}

	if (gDfvsFlag == 1)
		initDvfsParams();

	/* IsrEvent part */
	spin_lock_irqsave(&gDrvInitParams->decIsrLock, ulFlags);
	gDrvInitParams->DecIsrEvent.pvHandle = "DECISR_EVENT";
	gDrvInitParams->DecIsrEvent.u4HandleSize = sizeof("DECISR_EVENT")+1;
	gDrvInitParams->DecIsrEvent.u4TimeoutMs = 1;
	spin_unlock_irqrestore(&gDrvInitParams->decIsrLock, ulFlags);
	eValHWLockRet = eVideoCreateEvent(&gDrvInitParams->DecIsrEvent,
					sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 */
		pr_info("[VCODEC][ERROR] create dec isr event error\n");
	}

	spin_lock_irqsave(&gDrvInitParams->encIsrLock, ulFlags);
	gDrvInitParams->EncIsrEvent.pvHandle = "ENCISR_EVENT";
	gDrvInitParams->EncIsrEvent.u4HandleSize = sizeof("ENCISR_EVENT")+1;
	gDrvInitParams->EncIsrEvent.u4TimeoutMs = 1;
	spin_unlock_irqrestore(&gDrvInitParams->encIsrLock, ulFlags);
	eValHWLockRet = eVideoCreateEvent(&gDrvInitParams->EncIsrEvent,
					sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 */
		pr_info("[VCODEC][ERROR] create enc isr event error\n");
	}

	if (gWakeLock == 0)
		is_entering_suspend = 0;

	pr_debug("%s Done\n", __func__);

#if IS_ENABLED(CONFIG_MTK_HIBERNATION)
	register_swsusp_restore_noirq_func(ID_M_VCODEC,
					vcodec_pm_restore_noirq, NULL);
#endif

	return platform_driver_register(&vcodec_driver);
}

static void __exit vcodec_driver_exit(void)
{
	enum VAL_RESULT_T  eValHWLockRet;

	pr_debug("%s\n", __func__);

	mutex_lock(&gDrvInitParams->isOpenedLock);
	if (gDrvInitParams->bIsOpened == VAL_TRUE) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 */
		gDrvInitParams->bIsOpened = VAL_FALSE;
	}
	mutex_unlock(&gDrvInitParams->isOpenedLock);

	cdev_del(gDrvInitParams->vcodec_cdev);
	unregister_chrdev_region(vcodec_devno, 1);
#if IS_ENABLED(CONFIG_PM)
	cdev_del(gDrvInitParams->vcodec_cdev2);
#endif
	/* [TODO] iounmap the following? */

	free_irq(VENC_IRQ_ID, NULL);
	free_irq(VDEC_IRQ_ID, NULL);

	/* MT6589_HWLockEvent part */
	eValHWLockRet = eVideoCloseEvent(&gDrvInitParams->HWLockEvent,
					sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 */
		pr_info("[VCODEC][ERROR] close dec hwlock event error\n");
	}

	/* MT6589_IsrEvent part */
	eValHWLockRet = eVideoCloseEvent(&gDrvInitParams->DecIsrEvent,
					sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 */
		pr_info("[VCODEC][ERROR] close dec isr event error\n");
	}

	eValHWLockRet = eVideoCloseEvent(&gDrvInitParams->EncIsrEvent,
					sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 */
		pr_info("[VCODEC][ERROR] close enc isr event error\n");
	}

#if IS_ENABLED(CONFIG_MTK_HIBERNATION)
	unregister_swsusp_restore_noirq_func(ID_M_VCODEC);
#endif

	platform_driver_unregister(&vcodec_driver);
}

module_init(vcodec_driver_init);
module_exit(vcodec_driver_exit);
MODULE_AUTHOR("Legis, Lu <legis.lu@mediatek.com>");
MODULE_DESCRIPTION("MT6765 Vcodec Driver");
MODULE_LICENSE("GPL v2");
