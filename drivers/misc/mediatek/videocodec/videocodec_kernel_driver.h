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
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>

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
#define VCODEC_SET_AV_TASK_GROUP	_IOW(MFV_IOC_MAGIC, 0x36, unsigned int)
/* VAL_BOOL_T * */
#define VCODEC_SET_FRAME_INFO		_IOW(MFV_IOC_MAGIC, 0x37, unsigned int)
/* VAL_FRAME_INFO_T * */
#define VCODEC_MVA_ALLOCATION		_IOWR(MFV_IOC_MAGIC, 0x38, unsigned int)
/* VAL_MEM_INFO_T * */
#define VCODEC_MVA_FREE			_IOWR(MFV_IOC_MAGIC, 0x39, unsigned int)
/* VAL_MEM_INFO_T * */
#define VCODEC_CACHE_FLUSH_BUFF		_IOWR(MFV_IOC_MAGIC, 0x40, unsigned int)
/* VAL_MEM_INFO_T * */
#define VCODEC_CACHE_INVALIDATE_BUFF	_IOWR(MFV_IOC_MAGIC, 0x41, unsigned int)
/* VAL_MEM_INFO_T * */
#define VCODEC_GET_SECURE_HANDLE	_IOWR(MFV_IOC_MAGIC, 0x43, unsigned int)
/* VAL_FD_TO_SEC_HANDLE  * */
extern const struct file_operations vcodec_fops;

/* hardware VENC IRQ status(VP8/H264) */
extern unsigned int gu4HwVencIrqStatus;

extern unsigned int gLockTimeOutCount;

extern struct mtk_vcodec_dev *gVCodecDev;
extern void *KVA_VENC_IRQ_ACK_ADDR, *KVA_VENC_IRQ_STATUS_ADDR, *KVA_VENC_BASE;
extern void *KVA_VDEC_MISC_BASE, *KVA_VDEC_VLD_BASE;
extern void *KVA_VDEC_BASE, *KVA_VDEC_GCON_BASE, *KVA_MBIST_BASE;
extern unsigned int VENC_IRQ_ID, VDEC_IRQ_ID;

#define VDO_HW_WRITE(ptr, data)     mt_reg_sync_writel(data, ptr)
#define VDO_HW_READ(ptr)            readl((void __iomem *)ptr)

struct mtk_vcodec_drv_init_params {
	atomic_t drvOpenCount;
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

//define the write register function
#define mt_reg_sync_writel(v, a) \
	do {    \
		*(unsigned int *)(a) = (v);    \
		mb();  /*make sure register access in order */ \
	} while (0)

#endif /* __VCODEC_DRIVER_H__ */

#if IS_ENABLED(CONFIG_COMPAT)
enum STRUCT_TYPE {
	VAL_HW_LOCK_TYPE = 0,
	VAL_POWER_TYPE,
	VAL_ISR_TYPE,
	VAL_MEMORY_TYPE,
	VAL_FRAME_INFO_TYPE,
	VAL_MEM_OBJ_TYPE,
	VAL_SEC_HANDLE_OBJ_TYPE
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

struct COMPAT_VAL_MEM_OBJ {
	compat_u64 iova;
	compat_ulong_t len;
	compat_uint_t shared_fd;
	compat_uint_t cnt;
};

struct COMPAT_VAL_SEC_HANDLE_OBJ {
	compat_uint_t shared_fd;
	compat_uint_t sec_handle;
};
#endif
