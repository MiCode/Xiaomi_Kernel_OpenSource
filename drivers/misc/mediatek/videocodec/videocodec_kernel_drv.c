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
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>
#include <linux/iommu.h>
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
#include "mtk_vcodec_mem.h"
#include "mtk_vcodec_pm.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_pm_codec.h"
#include "mtk_vcodec_pm_plat.h"
#include "videocodec_kernel_drv_plat.h"

#include <linux/slab.h>
#include "dvfs_v2.h"
#include "mtk_heap.h"

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/uaccess.h>
#include <linux/compat.h>
#endif

static dev_t vcodec_devno = MKDEV(VCODEC_DEV_MAJOR_NUMBER, 0);

/* hardware VENC IRQ status(VP8/H264) */
unsigned int gu4HwVencIrqStatus;
const char *platform;
unsigned int gLockTimeOutCount;

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

struct mtk_vcodec_dev *gVCodecDev;
struct mtk_vcodec_drv_init_params *gDrvInitParams;
unsigned int VENC_IRQ_ID, VDEC_IRQ_ID;

/* #define KS_POWER_WORKAROUND */

void *mt_venc_base_get(void)
{
	return (void *)KVA_VENC_BASE;
}

void *mt_vdec_base_get(void)
{
	return (void *)KVA_VDEC_BASE;
}

/* DMA Buf operations */
static long vcodec_get_mva_allocation(struct device *dev,
					unsigned long arg,
					struct mtk_vcodec_queue *vcodec_queue)
{
	unsigned char *user_data_addr;
	long ret;
	struct VAL_MEM_INFO_T rMemObj;
	void *mem_priv;
	pr_debug("%s +\n", __func__);
	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&rMemObj, user_data_addr,
				sizeof(struct VAL_MEM_INFO_T));
	if (ret) {
		pr_debug("%s, copy_from_user failed: %lu\n",
			__func__, ret);
		return -EFAULT;
	}
	pr_debug("%s, rMemObj.shared_fd: %d, len: %d\n", __func__,
		rMemObj.shared_fd, rMemObj.len);
	mem_priv = mtk_vcodec_get_buffer(dev, vcodec_queue, &rMemObj);
	if (IS_ERR_OR_NULL(mem_priv) == true) {
		rMemObj.iova = (unsigned long long)-1;
		ret = (long)copy_to_user(user_data_addr,
			&rMemObj,
			(unsigned long)sizeof(struct VAL_MEM_INFO_T));
		pr_info("[VCODEC] ALLOCATION  failed!\n");
			return -EFAULT;
	}
	pr_debug("[%s][%d] iova = %llx, cnt = %d, fd: rMemObj.shared_fd: %d\n",
		__func__, __LINE__, rMemObj.iova, rMemObj.cnt, rMemObj.shared_fd);
	if (copy_to_user((void *)arg, &rMemObj, sizeof(struct VAL_MEM_INFO_T))) {
		pr_info("Copy to user error\n");
		return -EFAULT;
	}
	pr_debug("%s -\n", __func__);
	return 0;
}

long vcodec_cache_flush_buff(struct device *dev, unsigned long arg, unsigned int op)
{
	unsigned char *user_data_addr;
	long ret;
	struct VAL_MEM_INFO_T rMemObj;
	struct dma_buf *dmabuf = NULL;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;

	pr_debug("%s +\n", __func__);
	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&rMemObj, user_data_addr,
				sizeof(struct VAL_MEM_INFO_T));
	if (ret) {
		pr_debug("%s, copy_from_user failed : %lu\n",
			__func__, ret);
		return -EFAULT;
	}
	if (rMemObj.shared_fd == 0) {
		pr_info("%s, shared_fd : %d can't be 0, please check\n", __func__,
			rMemObj.shared_fd);
		return -1;
	}
	dmabuf = dma_buf_get(rMemObj.shared_fd);
	attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(attach)) {
		pr_info("[%s] attach fail, return\n", __func__);
		dma_buf_put(dmabuf);
		return -EFAULT;
	}
	sgt = dma_buf_map_attachment(attach, op);
	if (IS_ERR(sgt)) {
		pr_info("map failed, detach and return\n");
		dma_buf_detach(dmabuf, attach);
		return -EFAULT;
	}
	dma_sync_sg_for_device(dev, sgt->sgl, sgt->orig_nents, op);
	dma_buf_unmap_attachment(attach, sgt, op);
	dma_buf_detach(dmabuf, attach);
	dma_buf_put(dmabuf);
	pr_debug("%s -\n", __func__);
	return 0;
}

static long vcodec_get_mva_free(struct device *dev,
				unsigned long arg,
				struct mtk_vcodec_queue *vcodec_queue)
{
	unsigned char *user_data_addr;
	long ret = -1;
	struct VAL_MEM_INFO_T rMemObj;
	pr_debug("%s +\n", __func__);
	user_data_addr = (unsigned char *)arg;
	ret = (long)copy_from_user(&rMemObj, user_data_addr,
				(unsigned long)sizeof(struct VAL_MEM_INFO_T));
	if (ret) {
		pr_debug("%s, copy_from_user failed: %lu\n",
			__func__, ret);
		return -EFAULT;
	}
	ret = mtk_vcodec_free_buffer(vcodec_queue, &rMemObj);
	if (ret)
		pr_info("%s, failed: %lu\n",
			__func__, ret);
	return 0;
}

//get secure handle for SVP
static long vcodec_get_secure_handle(struct device *dev, unsigned long arg)
{
#if IS_ENABLED(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)
	unsigned char *user_data_addr;
	long ret = 0;
	struct VAL_FD_TO_SEC_HANDLE rSecObj;
	struct dma_buf *dmabuf = NULL;
	uint32_t sec_handle = 0;

	pr_info("%s +\n", __func__);
	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&rSecObj, user_data_addr,
				sizeof(struct VAL_FD_TO_SEC_HANDLE));
	if (ret) {
		pr_info("%s, copy_from_user failed: %lu\n",
			__func__, ret);
		dma_buf_put(dmabuf);
		ret = -EFAULT;
	return ret;
	}

	pr_info("%s, [b]rSecObj.shared_fd: %d\n", __func__, rSecObj.shared_fd);
	dmabuf = dma_buf_get(rSecObj.shared_fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		pr_info("%s dma_buf_get fail: %ld\n", __func__, PTR_ERR(dmabuf));
		dma_buf_put(dmabuf);
		ret = -EFAULT;
		return ret;
	}

	sec_handle = (VAL_UINT32_T)dmabuf_to_secure_handle(dmabuf);
	if (!sec_handle) {
		pr_info("%s get secure handle failed\n", __func__);
		dma_buf_put(dmabuf);
		ret = -EFAULT;
		return ret;
	}
	rSecObj.sec_handle = (VAL_UINT32_T)sec_handle;

	if (copy_to_user((void *)arg, &rSecObj, sizeof(struct VAL_FD_TO_SEC_HANDLE))) {
		pr_info("Copy to user error\n");
		dma_buf_put(dmabuf);
		ret = -EFAULT;
		return ret;
	}

	pr_info("[%s][%d] sec_handle = %llx -\n", __func__, __LINE__, rSecObj.sec_handle);
	dma_buf_put(dmabuf);
	return ret;
#else
	pr_info("[%s][%d] SVP not supported -\n", __func__, __LINE__);
	return 0;
#endif
}


/* Vcodec file operations */
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
	unsigned long length = vma->vm_end - vma->vm_start;
	unsigned long pa_start = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pfn;
	unsigned long start = vma->vm_start;

	pr_info("%s vma->start 0x%lx, vma->end 0x%lx, vma->pgoff 0x%lx, pa_start 0x%lx\n", __func__,
			(unsigned long)vma->vm_start,
			(unsigned long)vma->vm_end,
			(unsigned long)vma->vm_pgoff,
			(unsigned long)pa_start);
	length = vma->vm_end - vma->vm_start;
	pfn = vma->vm_pgoff<<PAGE_SHIFT;

	if ((length <  VDEC_REGION && (pa_start >= VDEC_BASE_PHY &&
		pa_start < VDEC_BASE_PHY + VDEC_REGION)) ||
		(length <  VENC_REGION && (pa_start >= VENC_BASE_PHY &&
		pa_start < VENC_BASE_PHY + VENC_REGION))) {
		vma->vm_pgoff = pa_start >> PAGE_SHIFT;
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		if (remap_pfn_range(vma, start, vma->vm_pgoff,
			PAGE_SIZE, vma->vm_page_prot) == true)
			return -EAGAIN;
	} else {
		pr_info("mmap region error: Len(0x%lx),pfn(0x%lx)",
			(unsigned long)length, pfn);
		return -EAGAIN;
	}
	vma->vm_ops = &vcodec_remap_vm_ops;
	vcodec_vma_open(vma);
	return 0;
}

static int vcodec_open(struct inode *inode, struct file *file)
{
	struct mtk_vcodec_queue *vcodec_queue;
	pr_debug("%s\n", __func__);
	mutex_lock(&gDrvInitParams->driverOpenCountLock);
	vcodec_queue = mtk_vcodec_mem_init(gVCodecDev->dev);
	if (vcodec_queue == NULL) {
		mutex_unlock(&gDrvInitParams->driverOpenCountLock);
		pr_info("%s pid = %d, vcodec_queue mem init failed\n",
			__func__, current->pid);
		return -ENOMEM;
	}
	vcodec_queue->vcodec = gVCodecDev->dev;
	file->private_data = vcodec_queue;
	atomic_inc(&gDrvInitParams->drvOpenCount);
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

static long vcodec_unlocked_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	long ret;
	unsigned char *user_data_addr;
	int temp_nr_cpu_ids;
	char rIncLogCount;
	unsigned int flush_dma_direction;
	struct mtk_vcodec_queue *vcodec_queue =
		(struct mtk_vcodec_queue *)file->private_data;

	pr_debug("%s %u +\n", __func__, cmd);
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

	case VCODEC_MB:
	{
		/* For userspace to guarantee register setting order */
		mb();
	}
	break;

	case VCODEC_SET_LOG_COUNT:
	{
		pr_debug("VCODEC_SET_LOG_COUNT + tid = %d\n", current->pid);

		mutex_lock(&gDrvInitParams->logCountLock);
		user_data_addr = (unsigned char *)arg;
		ret = copy_from_user(&rIncLogCount, user_data_addr,
			sizeof(char));
		if (ret) {
			pr_info("[ERROR] VCODEC_SET_LOG_COUNT, copy_from_user failed: %lu\n",
				ret);
			mutex_unlock(&gDrvInitParams->logCountLock);
			return -EFAULT;
		}

		if (rIncLogCount == VAL_TRUE) {
			if (gDrvInitParams->u4LogCountUser == 0) {
#if IS_ENABLED(CONFIG_CONSOLE_LOCK_DURATION_DETECT)
				gDrvInitParams->u4LogCount = get_detect_count();
				set_detect_count(gDrvInitParams->u4LogCount + 100);
#endif
			}
			gDrvInitParams->u4LogCountUser++;
		} else {
			gDrvInitParams->u4LogCountUser--;
			if (gDrvInitParams->u4LogCountUser == 0) {
#if IS_ENABLED(CONFIG_CONSOLE_LOCK_DURATION_DETECT)
				set_detect_count(gDrvInitParams->u4LogCount);
#endif
				gDrvInitParams->u4LogCount = 0;
			}
		}
		mutex_unlock(&gDrvInitParams->logCountLock);

		pr_debug("VCODEC_SET_LOG_COUNT - tid = %d\n", current->pid);
	}
	break;

	case VCODEC_MVA_ALLOCATION:
	{
		mutex_lock(&vcodec_queue->dev_lock);
		ret =  vcodec_get_mva_allocation(gVCodecDev->dev, arg, vcodec_queue);
		mutex_unlock(&vcodec_queue->dev_lock);
		if (ret) {
			pr_info("[ERROR] VCODEC_MVA_ALLOCATION failed! %lu\n",
				ret);
			return ret;
		}
			pr_info("VCODEC_MVA_ALLOCATION succeed! %lu\n",
				ret);

	}
	break;
	case VCODEC_MVA_FREE:
	{
		mutex_lock(&vcodec_queue->dev_lock);
		ret = vcodec_get_mva_free(gVCodecDev->dev, arg, vcodec_queue);
		mutex_unlock(&vcodec_queue->dev_lock);
		if (ret) {
			pr_debug("[ERROR] VCODEC_MVA_FREE failed! %lu\n",
				ret);
			return ret;
		}

	}
	break;
	case VCODEC_CACHE_FLUSH_BUFF:
	case VCODEC_CACHE_INVALIDATE_BUFF:
	{
		flush_dma_direction =
			((cmd == VCODEC_CACHE_FLUSH_BUFF) ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
		ret = vcodec_cache_flush_buff(gVCodecDev->dev, arg, flush_dma_direction);
		if (ret) {
			pr_debug("[ERROR] VCODEC_CACHE_FLUSH_BUFF failed! %lu\n",
				ret);
			return ret;
		}
	}
	break;
	case VCODEC_GET_SECURE_HANDLE:
	{
		ret = vcodec_get_secure_handle(gVCodecDev->dev, arg);
		pr_info("[INFO] VCODEC_GET_SECURE_HANDLE");
		if (ret) {
			pr_debug("[ERROR] VCODEC_GET_SECURE_HANDLE failed! %lu\n",
				ret);
			return ret;
		}
	}
	break;
	default:
	{
		ret = vcodec_plat_unlocked_ioctl(cmd, arg);
		return ret;
	}
	break;
	}
	return 0;
}

static int vcodec_release(struct inode *inode, struct file *file)
{
	unsigned long ulFlagsLockHW, ulFlagsISR;

	/* dump_stack(); */
	pr_debug("%s, curr_tid =%d\n", __func__, current->pid);
	mutex_lock(&gDrvInitParams->driverOpenCountLock);
	if (file->private_data)
		mtk_vcodec_mem_release((struct mtk_vcodec_queue *)file->private_data);
	pr_info("%s pid = %d, gDrvInitParams->drvOpenCount %d\n",
			__func__, current->pid, gDrvInitParams->drvOpenCount);
	if (atomic_dec_and_test(&gDrvInitParams->drvOpenCount)) {
		mutex_lock(&gDrvInitParams->hwLock);

		vcodec_plat_release();

		gDrvInitParams->u4VdecLockThreadId = 0;
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

/* For Compatible Use */
#if IS_ENABLED(CONFIG_COMPAT)
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
	compat_u64 ull;
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
	case VAL_MEM_OBJ_TYPE:
	{
		if (eDirection == COPY_FROM_USER) {
			struct COMPAT_VAL_MEM_OBJ __user *from32 =
				(struct COMPAT_VAL_MEM_OBJ *)data32;
			struct VAL_MEM_INFO_T __user *to =
				(struct VAL_MEM_INFO_T *)data;

			err |= get_user(ull, &(from32->iova));
			err |= put_user(ull, &(to->iova));
			err |= get_user(l, &(from32->len));
			err |= put_user(l, &(to->len));
			err |= get_user(u, &(from32->shared_fd));
			err |= put_user(u, &(to->shared_fd));
			err |= get_user(u, &(from32->cnt));
			err |= put_user(u, &(to->cnt));
		} else {
			struct COMPAT_VAL_MEM_OBJ __user *to32 =
				(struct COMPAT_VAL_MEM_OBJ *)data32;
			struct VAL_MEM_INFO_T __user *from =
				(struct VAL_MEM_INFO_T *)data;
			err |= get_user(ull, &(from->iova));
			err |= put_user(ull, &(to32->iova));
			err |= get_user(l, &(from->len));
			err |= put_user(l, &(to32->len));
			err |= get_user(u, &(from->shared_fd));
			err |= put_user(u, &(to32->shared_fd));
			err |= get_user(u, &(from->cnt));
			err |= put_user(u, &(to32->cnt));
		}
	}
	break;
	case VAL_SEC_HANDLE_OBJ_TYPE:
	{
		if (eDirection == COPY_FROM_USER) {
			struct COMPAT_VAL_SEC_HANDLE_OBJ __user *from32 =
				(struct COMPAT_VAL_SEC_HANDLE_OBJ *)data32;
			struct VAL_FD_TO_SEC_HANDLE __user *to =
				(struct VAL_FD_TO_SEC_HANDLE *)data;
			err |= get_user(u, &(from32->shared_fd));
			err |= put_user(u, &(to->shared_fd));
			err |= get_user(u, &(from32->sec_handle));
			err |= put_user(u, &(to->sec_handle));
		} else {
			struct COMPAT_VAL_SEC_HANDLE_OBJ __user *to32 =
				(struct COMPAT_VAL_SEC_HANDLE_OBJ *)data32;
			struct VAL_FD_TO_SEC_HANDLE __user *from =
				(struct VAL_FD_TO_SEC_HANDLE *)data;
			err |= get_user(u, &(from->shared_fd));
			err |= put_user(u, &(to32->shared_fd));
			err |= get_user(u, &(from->sec_handle));
			err |= put_user(u, &(to32->sec_handle));
		}
	}
	break;
	default:
	break;
	}

	return err;
}

long vcodec_unlocked_compat_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	long ret = 0;

	pr_debug("%s: %u\n", __func__, cmd);
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
	break;

	case VCODEC_MVA_ALLOCATION:
	{
		struct COMPAT_VAL_MEM_OBJ __user *data32;
		struct VAL_MEM_INFO_T __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(struct VAL_MEM_INFO_T));
		if (data == NULL)
			return -EFAULT;

		err = compat_copy_struct(VAL_MEM_OBJ_TYPE,
				COPY_FROM_USER, (void *)data32, (void *)data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, VCODEC_MVA_ALLOCATION,
						(unsigned long)data);

		err = compat_copy_struct(VAL_MEM_OBJ_TYPE, COPY_TO_USER,
					(void *)data32, (void *)data);

		if (err)
			return err;
	}
	break;

	case VCODEC_MVA_FREE:
	{
		struct COMPAT_VAL_MEM_OBJ __user *data32;
		struct VAL_MEM_INFO_T __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(struct VAL_MEM_INFO_T));
		if (data == NULL)
			return -EFAULT;

		err = compat_copy_struct(VAL_MEM_OBJ_TYPE,
				COPY_FROM_USER, (void *)data32, (void *)data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, VCODEC_MVA_FREE,
						(unsigned long)data);

		err = compat_copy_struct(VAL_MEM_OBJ_TYPE, COPY_TO_USER,
					(void *)data32, (void *)data);

		if (err)
			return err;
	}
	break;

	case VCODEC_CACHE_FLUSH_BUFF:
	case VCODEC_CACHE_INVALIDATE_BUFF:
	{
		struct COMPAT_VAL_MEM_OBJ __user *data32;
		struct VAL_MEM_INFO_T __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(struct VAL_MEM_INFO_T));
		if (data == NULL)
			return -EFAULT;

		err = compat_copy_struct(VAL_MEM_OBJ_TYPE,
				COPY_FROM_USER, (void *)data32, (void *)data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, VCODEC_CACHE_FLUSH_BUFF,
						(unsigned long)data);

		err = compat_copy_struct(VAL_MEM_OBJ_TYPE, COPY_TO_USER,
					(void *)data32, (void *)data);

		if (err)
			return err;
	}
	break;
	case VCODEC_GET_SECURE_HANDLE:
	{
		struct COMPAT_VAL_SEC_HANDLE_OBJ __user *data32;
		struct VAL_MEM_INFO_T __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(struct VAL_FD_TO_SEC_HANDLE));
		if (data == NULL)
			return -EFAULT;

		err = compat_copy_struct(VAL_SEC_HANDLE_OBJ_TYPE,
				COPY_FROM_USER, (void *)data32, (void *)data);
		if (err)
			return err;
		ret = file->f_op->unlocked_ioctl(file, VCODEC_GET_SECURE_HANDLE,
						(unsigned long)data);

		err = compat_copy_struct(VAL_SEC_HANDLE_OBJ_TYPE, COPY_TO_USER,
					(void *)data32, (void *)data);
		if (err)
			return err;
	}
	break;
	default:
		return vcodec_unlocked_ioctl(file, cmd, arg);
	}
	return 0;
}
#else
#define vcodec_unlocked_compat_ioctl NULL
#endif

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

	dev->io_domain = iommu_get_domain_for_dev(&pdev->dev);
	if (dev->io_domain == NULL) {
		pr_info("[VCODEC] Failed to get io_domain\n");
		return -EPROBE_DEFER;
	}
	pr_info("io_domain: %p", dev->io_domain);
	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));
	dev_info(&pdev->dev, "64-bit DMA enable\n");
	if (ret) {
		ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));
		if (ret) {
			pr_info("64-bit DMA enable failed\n");
			return ret;
		}
	}

	if (!pdev->dev.dma_parms) {
		pdev->dev.dma_parms =
			devm_kzalloc(&pdev->dev, sizeof(*pdev->dev.dma_parms), GFP_KERNEL);
	}
	if (pdev->dev.dma_parms) {
		ret = dma_set_max_seg_size(&pdev->dev, (unsigned int)DMA_BIT_MASK(34));
		if (ret)
			pr_info("Failed to set DMA segment size\n");
	}
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

	pr_debug("%s Done\n", __func__);

	vcodec_plat_probe(pdev, dev);
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

static int __init vcodec_driver_init(void)
{
	enum VAL_RESULT_T  eValHWLockRet;
	unsigned long ulFlags, ulFlagsLockHW, ulFlagsISR;

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

	mutex_lock(&gDrvInitParams->logCountLock);
	gDrvInitParams->u4LogCountUser = 0;
	gDrvInitParams->u4LogCount = 0;
	mutex_unlock(&gDrvInitParams->logCountLock);

	{
		struct device_node *node = NULL;

		node = of_find_compatible_node(NULL, NULL, "mediatek,venc");
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

	vcodec_driver_plat_init();
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

	vcodec_driver_plat_exit();
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
