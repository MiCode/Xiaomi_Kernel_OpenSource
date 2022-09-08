/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *       Tiffany Lin <tiffany.lin@mediatek.com>
 */
#ifndef __VCODEC_DRIVER_H__
#define __VCODEC_DRIVER_H__

#include <linux/regulator/consumer.h>
#include "val_types_private.h"
#include "hal_types_private.h"
#include "val_api_private.h"

#include "mtk_vcodec_pm.h"
#define MFV_IOC_MAGIC    'M'

/* below is control message */
#define MFV_TEST_CMD			_IO(MFV_IOC_MAGIC, 0x00)
#define MFV_INIT_CMD			_IO(MFV_IOC_MAGIC, 0x01)
#define MFV_DEINIT_CMD			_IO(MFV_IOC_MAGIC, 0x02)
/* P_MFV_DRV_CMD_QUEUE_T */
#define MFV_SET_CMD_CMD			_IOW(MFV_IOC_MAGIC, 0x03, unsigned int)
/* HAL_POWER_T * */
#define MFV_SET_PWR_CMD			_IOW(MFV_IOC_MAGIC, 0x04, unsigned int)
/* HAL_ISR_T * */
#define MFV_SET_ISR_CMD			_IOW(MFV_IOC_MAGIC, 0x05, unsigned int)
#define MFV_ALLOC_MEM_CMD		_IOW(MFV_IOC_MAGIC, 0x06, unsigned int)
#define MFV_FREE_MEM_CMD		_IOW(MFV_IOC_MAGIC, 0x07, unsigned int)
/* unsigned int* */
#define MFV_MAKE_PMEM_TO_NONCACHED	_IOW(MFV_IOC_MAGIC, 0x08, unsigned int)
/* VAL_INTMEM_T* */
#define MFV_ALLOC_INT_MEM_CMD		_IOW(MFV_IOC_MAGIC, 0x09, unsigned int)
/* VAL_INTMEM_T* */
#define MFV_FREE_INT_MEM_CMD		_IOW(MFV_IOC_MAGIC, 0x0a, unsigned int)
/* HAL_POWER_T * */
#define VCODEC_WAITISR			_IOW(MFV_IOC_MAGIC, 0x0b, unsigned int)
/* VAL_HW_LOCK_T * */
#define VCODEC_LOCKHW			_IOW(MFV_IOC_MAGIC, 0x0d, unsigned int)
/* HAL_POWER_T * */
#define VCODEC_PMEM_FLUSH		_IOW(MFV_IOC_MAGIC, 0x10, unsigned int)
/* HAL_POWER_T * */
#define VCODEC_PMEM_CLEAN		_IOW(MFV_IOC_MAGIC, 0x11, unsigned int)
/* VAL_UINT32_T * */
#define VCODEC_INC_SYSRAM_USER		_IOW(MFV_IOC_MAGIC, 0x13, unsigned int)
/* VAL_UINT32_T * */
#define VCODEC_DEC_SYSRAM_USER		_IOW(MFV_IOC_MAGIC, 0x14, unsigned int)
/* VAL_UINT32_T * */
#define VCODEC_INC_ENC_EMI_USER		_IOW(MFV_IOC_MAGIC, 0x15, unsigned int)
/* VAL_UINT32_T * */
#define VCODEC_DEC_ENC_EMI_USER		_IOW(MFV_IOC_MAGIC, 0x16, unsigned int)
/* VAL_UINT32_T * */
#define VCODEC_INC_DEC_EMI_USER		_IOW(MFV_IOC_MAGIC, 0x17, unsigned int)
/* VAL_UINT32_T * */
#define VCODEC_DEC_DEC_EMI_USER		_IOW(MFV_IOC_MAGIC, 0x18, unsigned int)
/* VAL_VCODEC_OAL_HW_REGISTER_T * */
#define VCODEC_INITHWLOCK		_IOW(MFV_IOC_MAGIC, 0x20, unsigned int)
/* VAL_VCODEC_OAL_HW_REGISTER_T * */
#define VCODEC_DEINITHWLOCK		_IOW(MFV_IOC_MAGIC, 0x21, unsigned int)
/* VAL_MEMORY_T * */
#define VCODEC_ALLOC_NON_CACHE_BUFFER	_IOW(MFV_IOC_MAGIC, 0x22, unsigned int)
/* VAL_MEMORY_T * */
#define VCODEC_FREE_NON_CACHE_BUFFER	_IOW(MFV_IOC_MAGIC, 0x23, unsigned int)
/* VAL_VCODEC_THREAD_ID_T * */
#define VCODEC_SET_THREAD_ID		_IOW(MFV_IOC_MAGIC, 0x24, unsigned int)
/* VAL_INTMEM_T * */
#define VCODEC_SET_SYSRAM_INFO		_IOW(MFV_IOC_MAGIC, 0x25, unsigned int)
/* VAL_INTMEM_T * */
#define VCODEC_GET_SYSRAM_INFO		_IOW(MFV_IOC_MAGIC, 0x26, unsigned int)
/* HAL_POWER_T * */
#define VCODEC_INC_PWR_USER		_IOW(MFV_IOC_MAGIC, 0x27, unsigned int)
/* HAL_POWER_T * */
#define VCODEC_DEC_PWR_USER		_IOW(MFV_IOC_MAGIC, 0x28, unsigned int)
/* VAL_VCODEC_CPU_LOADING_INFO_T * */
#define VCODEC_GET_CPU_LOADING_INFO	_IOW(MFV_IOC_MAGIC, 0x29, unsigned int)
/* VAL_VCODEC_CORE_LOADING_T * */
#define VCODEC_GET_CORE_LOADING		_IOW(MFV_IOC_MAGIC, 0x30, unsigned int)
/* int * */
#define VCODEC_GET_CORE_NUMBER		_IOW(MFV_IOC_MAGIC, 0x31, unsigned int)
/* VAL_VCODEC_CPU_OPP_LIMIT_T * */
#define VCODEC_SET_CPU_OPP_LIMIT	_IOW(MFV_IOC_MAGIC, 0x32, unsigned int)
/* VAL_HW_LOCK_T * */
#define VCODEC_UNLOCKHW			_IOW(MFV_IOC_MAGIC, 0x33, unsigned int)
/* VAL_UINT32_T * */
#define VCODEC_MB			_IOW(MFV_IOC_MAGIC, 0x34, unsigned int)
/* VAL_BOOL_T * */
#define VCODEC_SET_LOG_COUNT		_IOW(MFV_IOC_MAGIC, 0x35, unsigned int)
/* VAL_BOOL_T * */
#define VCODEC_SET_AV_TASK_GROUP       _IOW(MFV_IOC_MAGIC, 0x36, unsigned int)
/* VAL_BOOL_T * */
#define VCODEC_SET_FRAME_INFO          _IOW(MFV_IOC_MAGIC, 0x37, unsigned int)
/* VAL_FRAME_INFO_T * */

extern const struct file_operations vcodec_fops;
#if IS_ENABLED(CONFIG_PM)
extern struct dev_pm_domain mt_vdec_pm_domain;
extern struct dev_pm_domain mt_venc_pm_domain;
#endif

/* hardware VENC IRQ status(VP8/H264) */
extern unsigned int gu4HwVencIrqStatus;
extern const char *platform;
extern unsigned int gLockTimeOutCount;
extern int gBistFlag;
extern int gDfvsFlag;
extern int gWakeLock;

extern unsigned int is_entering_suspend;
extern struct mtk_vcodec_dev *gVCodecDev;
extern void *KVA_VENC_IRQ_ACK_ADDR, *KVA_VENC_IRQ_STATUS_ADDR, *KVA_VENC_BASE;
extern void *KVA_VDEC_MISC_BASE, *KVA_VDEC_VLD_BASE;
extern void *KVA_VDEC_BASE, *KVA_VDEC_GCON_BASE, *KVA_MBIST_BASE;
extern unsigned int VENC_IRQ_ID, VDEC_IRQ_ID;

#define VDO_HW_WRITE(ptr, data)     mt_reg_sync_writel(data, ptr)
#define VDO_HW_READ(ptr)            readl((void __iomem *)ptr)

struct mtk_vcodec_drv_init_params {
	int drvOpenCount;
	unsigned int u4PWRCounter;      /* mutex : PWRLock */
	unsigned int u4EncEMICounter;   /* mutex : EncEMILock */
	unsigned int u4DecEMICounter;   /* mutex : DecEMILock */
	unsigned int u4LogCountUser;
	unsigned int u4LogCount;
	unsigned int u4VdecPWRCounter;
	unsigned int u4VencPWRCounter;
	unsigned int u4VdecLockThreadId;
	char bIsOpened;
	spinlock_t lockDecHWCountLock;
	spinlock_t lockEncHWCountLock;
	spinlock_t decISRCountLock;
	spinlock_t encISRCountLock;
	spinlock_t decIsrLock;
	spinlock_t encIsrLock;
	struct mutex vdecPWRLock;
	struct mutex vencPWRLock;
	struct mutex isOpenedLock;
	struct mutex driverOpenCountLock;
	struct mutex logCountLock;
	struct mutex hwLock;
	struct mutex hwLockEventTimeoutLock;
	struct mutex pwrLock;
	struct mutex encEMILock;
	struct mutex decEMILock;
	struct cdev *vcodec_cdev;
	struct class *vcodec_class;
	struct device *vcodec_device;
#if IS_ENABLED(CONFIG_PM)
	struct cdev *vcodec_cdev2;
	struct class *vcodec_class2;
	struct device *vcodec_device2;
#endif
	struct VAL_EVENT_T HWLockEvent;   /* mutex : HWLockEventTimeoutLock */
	struct VAL_EVENT_T DecIsrEvent;    /* mutex : HWLockEventTimeoutLock */
	struct VAL_EVENT_T EncIsrEvent;    /* mutex : HWLockEventTimeoutLock */
};
extern struct mtk_vcodec_drv_init_params *gDrvInitParams;
void initDvfsParams(void);
//define the write register function
#define mt_reg_sync_writel(v, a) \
	do {    \
		*(unsigned int *)(a) = (v);    \
		mb();  /*make sure register access in order */ \
	} while (0)

#endif /* __VCODEC_DRIVER_H__ */
