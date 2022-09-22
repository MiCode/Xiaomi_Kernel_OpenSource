// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

/******************************************************************************
 * camera_mem.c - Linux Camera Memory Device Driver
 *
 * DESCRIPTION:
 *     This file provides the other drivers camera memory relative functions
 *
 ******************************************************************************/

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <soc/mediatek/smi.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/suspend.h>

#include "camera_mem.h"
#include "mtk_heap.h"
#include "cam_common.h"

#if IS_ENABLED(CONFIG_COMPAT)
/* 32/64 bit compatible */
#include <linux/compat.h>
#endif

#define CAM_MEM_DEV_NAME "camera-mem"

#define LogTag "[CAM_MEM]"

#define LOG_VRB(format, args...)                                               \
	pr_debug(LogTag "[%s] " format, __func__, ##args)

#ifdef CAM_MEM_DEBUG
#define LOG_DBG(format, args...) pr_info(LogTag "[%s] " format, __func__, ##args)
#else
#define LOG_DBG(format, args...)
#endif

#define LOG_INF(format, args...) pr_info(LogTag "[%s] " format, __func__, ##args)

#define LOG_NOTICE(format, args...)                                            \
	pr_notice(LogTag "[%s] " format, __func__, ##args)


static int g_larb_num;

struct cam_mem_device {
	struct device *dev;

	/* p1: larb13/14, larb16/17/18
	 * p2: larb9, larb11 and larb20
	 */
	struct device **larbs;
};

struct CAM_MEM_USER_INFO_STRUCT {
	pid_t Pid;
	pid_t Tid;
};

struct ION_BUFFER {
	struct dma_buf        *dmaBuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t dmaAddr;
	unsigned int need_sec_handle;
};

/* A list to store the ION_BUFFER which mapped the dmaAddr(aka iova or PA) by the ioctl,
 * "CAM_MEM_ION_MAP_PA". When "CAM_MEM_ION_UNMAP_PA", it could be traversed to unmap
 * the proper ION_BUFFER.
 */
struct ION_BUFFER_LIST {
	struct list_head list;
	struct dma_buf        *dmaBuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t dmaAddr; /* physical addr / iova / secure handle */
	int memID; /* buffer fd */
	unsigned int refCnt; /* map count */
	unsigned long long timestamp; /* timestamp after mapping iova in ns */
	char username[64]; /* userspace user name */
	unsigned int need_sec_handle;
};

struct CAM_MEM_INFO_STRUCT {
	spinlock_t SpinLock_CamMemRef; /* SpinLock for UserCount */
	spinlock_t SpinLock_Larb;
	unsigned int UserCount; /* Open user count */
};

static struct CAM_MEM_INFO_STRUCT CamMemInfo;

static struct cam_mem_device cam_mem_dev;

/* prevent cam_mem race condition in vulunerbility test */
static struct mutex open_cam_mem_mutex;
static struct mutex cam_mem_pwr_ctrl_mutex;

#define HASH_BUCKET_NUM (512)

static struct mutex cam_mem_ion_mutex[HASH_BUCKET_NUM];

static struct ION_BUFFER_LIST g_ion_buf_list[HASH_BUCKET_NUM];

static unsigned int G_u4EnableLarbCount;
static atomic_t G_u4DevNodeCt;
static unsigned int g_platform_id;

#define WARNING_STR_SIZE (1024)

/*******************************************************************************
 *
 ******************************************************************************/
static inline void smi_larbs_get(void)
{
	int ret;
	int i = 0;

	if (g_larb_num <= 0)
		return;

	for (i = 0; i < g_larb_num; i++) {
		if (cam_mem_dev.larbs[i]) {
			ret = mtk_smi_larb_get(cam_mem_dev.larbs[i]);
			if (ret) {
				LOG_NOTICE("mtk_smi_larb_get larbs[%d] fail %d\n", i, ret);
			} else {
				spin_lock(&(CamMemInfo.SpinLock_Larb));
				G_u4EnableLarbCount++;
				spin_unlock(&(CamMemInfo.SpinLock_Larb));
			}
		} else
			LOG_NOTICE("No larbs[%d] device\n", i);
	}
}

/*******************************************************************************
 *
 ******************************************************************************/
static inline void smi_larbs_put(void)
{
	int i = 0;

	if (g_larb_num <= 0)
		return;

	for (i = 0; i < g_larb_num; i++) {
		if (cam_mem_dev.larbs[g_larb_num - 1 - i]) {
			mtk_smi_larb_put(cam_mem_dev.larbs[g_larb_num - 1 - i]);
			spin_lock(&(CamMemInfo.SpinLock_Larb));
			G_u4EnableLarbCount--;
			spin_unlock(&(CamMemInfo.SpinLock_Larb));
		} else
			LOG_NOTICE("cam_mem_dev.larbs[%d] is NULL!\n", g_larb_num - 1 - i);
	}
}

/*******************************************************************************
 *
 ******************************************************************************/
static void CamMem_EnableLarb(bool En)
{
	if (En) {
		smi_larbs_get(); /* !!cannot be used in spinlock!! */
	} else {
		spin_lock(&(CamMemInfo.SpinLock_Larb));
		if (G_u4EnableLarbCount == 0) {
			spin_unlock(&(CamMemInfo.SpinLock_Larb));

			LOG_DBG("G_u4EnableLarbCount aleady be 0, do nothing\n");

			return;
		}
		spin_unlock(&(CamMemInfo.SpinLock_Larb));

		/* !!cannot be used in spinlock!! */
		smi_larbs_put();
	}
}

/*******************************************************************************
 *
 ******************************************************************************/
static int cam_mem_open(struct inode *pInode, struct file *pFile)
{
	struct CAM_MEM_USER_INFO_STRUCT *pUserInfo;
	int Ret = 0;

	mutex_lock(&open_cam_mem_mutex);

	LOG_DBG("+: UserCount(%d)\n", CamMemInfo.UserCount);

	pFile->private_data =
		kmalloc(sizeof(struct CAM_MEM_USER_INFO_STRUCT), GFP_ATOMIC);

	if (unlikely(pFile->private_data == NULL)) {
		LOG_DBG(
			"ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)\n",
			current->comm, current->pid, current->tgid);

		Ret = -ENOMEM;
	} else {
		pUserInfo = (struct CAM_MEM_USER_INFO_STRUCT *)pFile->private_data;
		pUserInfo->Pid = current->pid;
		pUserInfo->Tid = current->tgid;
	}

	spin_lock(&(CamMemInfo.SpinLock_CamMemRef));
	if (CamMemInfo.UserCount > 0) {
		CamMemInfo.UserCount++;
		spin_unlock(&(CamMemInfo.SpinLock_CamMemRef));

		LOG_DBG(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist\n",
			CamMemInfo.UserCount,
			current->comm, current->pid, current->tgid);
	} else {
		CamMemInfo.UserCount++;
		spin_unlock(&(CamMemInfo.SpinLock_CamMemRef));

		LOG_DBG(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), first user\n",
			CamMemInfo.UserCount,
			current->comm, current->pid, current->tgid);
	}

	LOG_DBG("-: Ret: %d. UserCount: %d\n", Ret, CamMemInfo.UserCount);

	mutex_unlock(&open_cam_mem_mutex);

	return Ret;
}

/*******************************************************************************
 *
 ******************************************************************************/
static void cam_mem_mmu_put_dma_buffer(struct ION_BUFFER *mmu)
{
	if (likely(mmu->dmaBuf)) {
		if (unlikely((IS_MT6789(g_platform_id)) && mmu->need_sec_handle)) {
			dma_buf_put(mmu->dmaBuf);
		} else {
			dma_buf_unmap_attachment(mmu->attach, mmu->sgt,
				DMA_BIDIRECTIONAL);
			dma_buf_detach(mmu->dmaBuf, mmu->attach);
			dma_buf_put(mmu->dmaBuf);
		}
	} else
		LOG_NOTICE("mmu->dmaBuf is NULL\n");
}

static void cam_mem_unmap_all(void)
{
	int i = 0;
	struct ION_BUFFER_LIST *tmp; /* for delete the mapped node */
	struct list_head *pos, *q;
	struct ION_BUFFER pIonBuf = {NULL, NULL, NULL, 0};

	LOG_NOTICE("+\n");

	for (i = 0; i < HASH_BUCKET_NUM; i++) {
		mutex_lock(&cam_mem_ion_mutex[i]);
		if (list_empty(&g_ion_buf_list[i].list)) {
			mutex_unlock(&cam_mem_ion_mutex[i]);
			continue;
		}

		/* unmap iova and delete the node. */
		list_for_each_safe(pos, q,
			&g_ion_buf_list[i].list) {
			tmp = list_entry(pos, struct ION_BUFFER_LIST, list);

			/* unmap iova */
			pIonBuf.dmaBuf = tmp->dmaBuf;
			pIonBuf.attach = tmp->attach;
			pIonBuf.sgt = tmp->sgt;
			pIonBuf.need_sec_handle = tmp->need_sec_handle;
			cam_mem_mmu_put_dma_buffer(&pIonBuf);

			/* delete a mapped node in the list. */
			list_del(pos);
			kfree(tmp);
		}
		mutex_unlock(&cam_mem_ion_mutex[i]);
	}
	LOG_NOTICE("-\n");
}

/*******************************************************************************
 *
 ******************************************************************************/
static int cam_mem_release(struct inode *pInode, struct file *pFile)
{
	unsigned int i = 0;

	mutex_lock(&open_cam_mem_mutex);

	LOG_NOTICE("+. UserCount: %d.\n", CamMemInfo.UserCount);

	if (likely(pFile->private_data != NULL)) {
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}

	spin_lock(&(CamMemInfo.SpinLock_CamMemRef));
	CamMemInfo.UserCount--;
	if (CamMemInfo.UserCount > 0) {
		spin_unlock(&(CamMemInfo.SpinLock_CamMemRef));

		LOG_NOTICE(
			"Curr UserCount(%d), (process, pid, tgid) = (%s, %d, %d),users exist\n",
			CamMemInfo.UserCount, current->comm, current->pid,
			current->tgid);

		goto EXIT;
	} else
		spin_unlock(&(CamMemInfo.SpinLock_CamMemRef));

	spin_lock(&(CamMemInfo.SpinLock_Larb));
	i = G_u4EnableLarbCount;
	spin_unlock(&(CamMemInfo.SpinLock_Larb));

	/* clear all mapped iova */
	cam_mem_unmap_all();

	LOG_NOTICE("Disable all larbs, total count:%d.\n", i);
	while (i > 0) {
		CamMem_EnableLarb(false);
		i--;
	}

EXIT:
	LOG_NOTICE("-. UserCount: %d. G_u4EnableLarbCount:%d\n",
		CamMemInfo.UserCount, G_u4EnableLarbCount);

	mutex_unlock(&open_cam_mem_mutex);

	return 0;
}

/*******************************************************************************
 * dump single cam_mem buf list.
 ******************************************************************************/
static void dumpIonBufferList(struct ION_BUFFER_LIST *ion_buf_list,
		unsigned int entry_num, bool TurnOn)
{
	struct ION_BUFFER_LIST *entry;
	struct list_head *pos;
	int i = 0;
	unsigned int list_length = 0;
	unsigned long long second;
	unsigned long long nano_second;

	if (TurnOn == false)
		return;

	if (list_empty(&ion_buf_list->list))
		return;

	list_for_each(pos, &ion_buf_list->list) {
		list_length++;
	}

	list_for_each(pos, &ion_buf_list->list) {
		if (i >= entry_num)
			break;

		entry = list_entry(pos, struct ION_BUFFER_LIST, list);
		second = entry->timestamp;
		nano_second = do_div(second, 1000000000);

		LOG_NOTICE("%3d/%3d:memID(%3d);P:0x%lx;S:0x%zx;RC(%d);user(%s);T(%llu.%llu)\n",
			i, list_length, entry->memID, entry->dmaAddr, entry->dmaBuf->size,
			entry->refCnt, entry->username, second, nano_second);
		i++;
	}
}

/*******************************************************************************
 * dump all cam_mem buf list
 ******************************************************************************/
static void dumpAllBufferList(void)
{
	struct ION_BUFFER_LIST *entry;
	struct list_head *pos;
	int i = 0, j = 0;
	unsigned long long total_size = 0;

	LOG_NOTICE(" idx memID        pa     aligned-size   refCnt       user         timestamp\n");

	for (j = 0; j < HASH_BUCKET_NUM; j++) {

		mutex_lock(&cam_mem_ion_mutex[j]);

		if (list_empty(&g_ion_buf_list[j].list)) {
			mutex_unlock(&cam_mem_ion_mutex[j]);
			continue;
		}

		list_for_each(pos, &g_ion_buf_list[j].list) {
			unsigned long long second;
			unsigned long long nano_second;

			entry = list_entry(pos, struct ION_BUFFER_LIST, list);

			second = entry->timestamp;
			nano_second = do_div(second, 1000000000);

			total_size += entry->dmaBuf->size;

			LOG_NOTICE("#%03d   %3d   0x%09lx  0x%07zx     %2d   %-32s %llu.%llu\n",
				i,
				entry->memID,
				entry->dmaAddr, entry->dmaBuf->size,
				entry->refCnt, entry->username, second, nano_second);
				i++;
		}
		mutex_unlock(&cam_mem_ion_mutex[j]);
	}
	if (total_size > 0) {
		LOG_NOTICE("           Total Size= 0x%llx (%lld KBytes)\n",
			total_size, total_size >> 10);
	}
}


static bool cam_mem_get_secure_handle(struct ION_BUFFER *mmu,
		struct CAM_MEM_DEV_ION_NODE_STRUCT *IonNode)
{
	struct dma_buf *buf;

	if (unlikely(IonNode->memID < 0)) {
		LOG_NOTICE("invalid memID(%d)!\n", IonNode->memID);
		return false;
	}
	/* va: buffer fd from user space, we get dmabuf from buffer fd. */
	buf = dma_buf_get(IonNode->memID);

	mmu->dmaBuf = buf;

	IonNode->sec_handle = dmabuf_to_secure_handle(mmu->dmaBuf);
	if (IonNode->sec_handle == 0) {
		LOG_NOTICE("Get sec_handle failed! memID(%d)\n", IonNode->memID);
		return false;
	}

	return true;
}

/*******************************************************************************
 *
 ******************************************************************************/
static bool cam_mem_mmu_get_dma_buffer(
	struct ION_BUFFER *mmu, struct CAM_MEM_DEV_ION_NODE_STRUCT *IonNode)
{
	struct dma_buf *buf;

	if (unlikely(IonNode->memID < 0)) {
		LOG_NOTICE("invalid memID(%d)!\n", IonNode->memID);
		return false;
	}
	/* va: buffer fd from user space, we get dmabuf from buffer fd. */
	buf = dma_buf_get(IonNode->memID);
	if (IS_ERR(buf)) {
		LOG_NOTICE("dma_buf_get failed! memID(%d)\n", IonNode->memID);
		return false;
	}
	mmu->dmaBuf = buf;

	if (IonNode->need_sec_handle) {
		IonNode->sec_handle = dmabuf_to_secure_handle(mmu->dmaBuf);
		if (IonNode->sec_handle == 0) {
			LOG_NOTICE("Get sec_handle failed! memID(%d)\n", IonNode->memID);
			return false;
		}
	}

	mmu->attach = dma_buf_attach(mmu->dmaBuf, cam_mem_dev.dev);
	if (IS_ERR(mmu->attach)) {
		LOG_NOTICE("dma_buf_attach failed! memID(%d) size(0x%zx)\n",
			IonNode->memID, buf->size);
		goto err_attach;
	}

	/* Lower down the MIPS */
	mmu->attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	/* buffer and iova map */
	mmu->sgt = dma_buf_map_attachment(mmu->attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(mmu->sgt)) {
		LOG_NOTICE("dma_buf_map_attachment failed! memID(%d) size(0x%zx)\n",
			IonNode->memID, buf->size);
		goto err_map;
	}
	return true;

err_map:
	dma_buf_detach(mmu->dmaBuf, mmu->attach);
err_attach:
	dma_buf_put(mmu->dmaBuf);

	return false;
}

/*******************************************************************************
 *
 ******************************************************************************/
static long cam_mem_ioctl(struct file *pFile, unsigned int Cmd, unsigned long Param)
{
	int Ret = 0;

	struct CAM_MEM_USER_INFO_STRUCT *pUserInfo;
	struct CAM_MEM_DEV_ION_NODE_STRUCT IonNode;
	int power_on;

	if (unlikely(pFile->private_data == NULL)) {
		LOG_NOTICE(
			"private_data is NULL,(process, pid, tgid)=(%s, %d, %d)\n",
			current->comm, current->pid, current->tgid);

		return -EFAULT;
	}
	/*  */
	pUserInfo = (struct CAM_MEM_USER_INFO_STRUCT *)(pFile->private_data);

	switch (Cmd) {

	case CAM_MEM_ION_MAP_PA:
		if (likely(copy_from_user(&IonNode, (void *)Param,
			sizeof(struct CAM_MEM_DEV_ION_NODE_STRUCT)) == 0)) {
			dma_addr_t dmaPa = 0, found_dmaPa = 0;
			struct list_head *pos;
			/* tmp ION_BUFFER_LIST node to add into the list. */
			struct ION_BUFFER_LIST *tmp;
			struct ION_BUFFER_LIST *entry;
			struct ION_BUFFER mmu = {NULL, NULL, NULL, 0};
			unsigned int refCnt = 0;
			int bucketID = 0;

			if (unlikely(IonNode.memID <= 0)) {
				LOG_NOTICE(
					"[CAM_MEM_ION_MAP_PA] invalid memID(%d)\n",
					IonNode.memID);
				Ret = -EFAULT;
				break;
			}

			bucketID = IonNode.memID % HASH_BUCKET_NUM;
			mutex_lock(&cam_mem_ion_mutex[bucketID]);
			/* Check if already mapped. */
			list_for_each(pos, &g_ion_buf_list[bucketID].list) {
				entry = list_entry(pos, struct ION_BUFFER_LIST, list);
				if (unlikely(entry->memID != IonNode.memID))
					continue;

				entry->refCnt++;
				refCnt = entry->refCnt;
				found_dmaPa = entry->dmaAddr;
			}
			mutex_unlock(&cam_mem_ion_mutex[bucketID]);

			/* Map iova. */
			/*TZMP1 non-support get iova, support get secure handle*/
			if (unlikely((IS_MT6789(g_platform_id)) && IonNode.need_sec_handle)) {
				if (unlikely(cam_mem_get_secure_handle(&mmu, &IonNode) == false)) {
					LOG_NOTICE(
					"CAM_MEM_ION_MAP_PA: cam_mem_get_secure_handle fail, memID(%d)\n",
					IonNode.memID);
					Ret = -ENOMEM;
					break;
				}
			} else {
				LOG_DBG("CAM_MEM_ION_MAP_PA: do map. memID(%d)\n", IonNode.memID);
				if (unlikely(cam_mem_mmu_get_dma_buffer(&mmu, &IonNode) == false)) {
					LOG_NOTICE(
						"CAM_MEM_ION_MAP_PA: map pa failed, memID(%d)\n",
						IonNode.memID);

					dumpAllBufferList();

					Ret = -ENOMEM;
					break;
				}
				/* get iova. */
				dmaPa = sg_dma_address(mmu.sgt->sgl);
				if (unlikely(!dmaPa)) {
					Ret = -ENOMEM;
					LOG_NOTICE(
						"CAM_MEM_ION_MAP_PA: sg_dma_address fail, memID(%d)\n",
						IonNode.memID);
					break;
				}
				IonNode.dma_pa = mmu.dmaAddr = dmaPa;

				if (unlikely((found_dmaPa != dmaPa) && (found_dmaPa != 0))) {
					LOG_NOTICE(
					"memID(%d)P(0x%lx)S(0x%x): 1 fd with multi iova:\n",
						IonNode.memID, dmaPa, mmu.dmaBuf->size);
					mutex_lock(&cam_mem_ion_mutex[bucketID]);
					dumpIonBufferList(&g_ion_buf_list[bucketID], 100, true);
					mutex_unlock(&cam_mem_ion_mutex[bucketID]);
				}
			}
			/* Add an entry to global ION_BUFFER list. */
			tmp = kmalloc(
				sizeof(struct ION_BUFFER_LIST), GFP_KERNEL);
			if (unlikely(tmp == NULL)) {
				LOG_NOTICE("alloc buf list fail, memID(%d)\n",
					IonNode.memID);
				Ret = -EFAULT;
				break;
			}
			tmp->attach = mmu.attach;
			tmp->dmaBuf = mmu.dmaBuf;
			tmp->sgt = mmu.sgt;
			tmp->dmaAddr = IonNode.dma_pa;
			tmp->memID = IonNode.memID;
			tmp->timestamp = ktime_get();
			tmp->need_sec_handle = IonNode.need_sec_handle;
			if (found_dmaPa == 0)
				tmp->refCnt = 1;
			else
				tmp->refCnt = refCnt;

			strncpy(tmp->username, IonNode.username, sizeof(tmp->username));
			tmp->username[sizeof(tmp->username) - 1] = '\0';

			mutex_lock(&cam_mem_ion_mutex[bucketID]);
			list_add(&(tmp->list),
				&(g_ion_buf_list[bucketID].list));
			LOG_DBG("Add: memID(%d); pa(0x%lx);dmaBuf(0x%p);refCnt(%d); user(%s)\n",
				tmp->memID,
				tmp->dmaAddr,
				tmp->dmaBuf,
				tmp->refCnt,
				tmp->username);

			dumpIonBufferList(&g_ion_buf_list[bucketID], 10, false);

			mutex_unlock(&cam_mem_ion_mutex[bucketID]);

			if (unlikely(copy_to_user((void *)Param, &IonNode,
				sizeof(struct CAM_MEM_DEV_ION_NODE_STRUCT)) != 0)) {
				LOG_NOTICE("CAM_MEM_ION_MAP_PA: copy to user fail\n");
				Ret = -EFAULT;
			}
		} else {
			LOG_NOTICE(
				"CAM_MEM_ION_MAP_PA: copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	case CAM_MEM_ION_UNMAP_PA:
		if (likely(copy_from_user(&IonNode, (void *)Param,
			sizeof(struct CAM_MEM_DEV_ION_NODE_STRUCT)) == 0)) {
			struct ION_BUFFER_LIST *tmp; /* for delete the mapped node */
			struct list_head *pos, *q;
			struct ION_BUFFER pIonBuf = {NULL, NULL, NULL, 0};
			bool foundFD = false;
			unsigned int refCnt = 0;
			int bucketID = 0;

			if (unlikely(IonNode.memID <= 0)) {
				LOG_NOTICE("CAM_MEM_ION_UNMAP_PA: invalid memID(%d)\n",
					IonNode.memID);
				Ret = -EFAULT;
				break;
			}

			bucketID = IonNode.memID % HASH_BUCKET_NUM;
			mutex_lock(&cam_mem_ion_mutex[bucketID]);
			if (unlikely(list_empty(
				&g_ion_buf_list[bucketID].list))) {
				LOG_NOTICE("CAM_MEM_ION_UNMAP_PA: fail. list empty. memID(%d)\n",
					IonNode.memID);
				Ret = -EFAULT;
				mutex_unlock(&cam_mem_ion_mutex[bucketID]);
				break;
			}

			/* unmap and delete the node, if find the fd. */
			list_for_each_safe(pos, q,
				&g_ion_buf_list[bucketID].list) {
				tmp = list_entry(pos, struct ION_BUFFER_LIST, list);
				if (unlikely(tmp->memID != IonNode.memID))
					continue;

				foundFD = true;

				LOG_DBG("Del: memID(%d): pa=(0x%lx); refCnt(%d); (%s)\n",
					tmp->memID, tmp->dmaAddr,  tmp->refCnt,
					tmp->username);

				refCnt = tmp->refCnt - 1;
				pIonBuf.dmaBuf = tmp->dmaBuf;
				pIonBuf.attach = tmp->attach;
				pIonBuf.sgt = tmp->sgt;
				pIonBuf.need_sec_handle = tmp->need_sec_handle;

				/* delete a mapped node in the list. */
				list_del(pos);
				break;
			}

			mutex_unlock(&cam_mem_ion_mutex[bucketID]);

			if (unlikely(!foundFD)) {
				LOG_NOTICE("Warning: unmap: memID(%d); PA(0x%lx);"
					" (%s) not found.\n",
					IonNode.memID, IonNode.dma_pa,
					IonNode.username);
				Ret = -EFAULT;
				break;
			} else {
				cam_mem_mmu_put_dma_buffer(&pIonBuf);
				IonNode.dma_pa = 0;
				kfree(tmp);
			}

			/* If refCnt is still > 0, update the refCnt with the same fd. */
			if (refCnt > 0) {
				struct list_head *pos2;
				struct ION_BUFFER_LIST *entry;

				mutex_lock(&cam_mem_ion_mutex[bucketID]);
				list_for_each(pos2,
					&g_ion_buf_list[bucketID].list) {
					entry = list_entry(pos2, struct ION_BUFFER_LIST, list);
					if (unlikely(entry->memID != IonNode.memID))
						continue;
					entry->refCnt--;
				}
				mutex_unlock(&cam_mem_ion_mutex[bucketID]);
			}
#ifdef CAM_MEM_DEBUG
			mutex_lock(&cam_mem_ion_mutex[bucketID]);
			dumpIonBufferList(&g_ion_buf_list[bucketID], 10, false);
			mutex_unlock(&cam_mem_ion_mutex[bucketID]);
#endif
		} else {
			LOG_NOTICE("CAM_MEM_ION_UNMAP_PA: copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;

	case CAM_MEM_ION_GET_PA:
		if (copy_from_user(&IonNode, (void *)Param,
			sizeof(struct CAM_MEM_DEV_ION_NODE_STRUCT)) == 0) {
			struct list_head *pos;
			struct ION_BUFFER_LIST *entry;
			bool bMapped = false;
			int bucketID = 0;

			if (IonNode.memID <= 0) {
				LOG_NOTICE(
					"CAM_MEM_ION_GET_PA: invalid memID(%d)\n",
					IonNode.memID);
				Ret = -EFAULT;
				break;
			}
			LOG_NOTICE("CAM_MEM_ION_GET_PA: IonNode.memID(%d)\n",
				IonNode.memID);

			bucketID = IonNode.memID % HASH_BUCKET_NUM;
			mutex_lock(&cam_mem_ion_mutex[bucketID]);

			if (list_empty(&g_ion_buf_list[bucketID].list)) {
				LOG_NOTICE(
					"CAM_MEM_ION_GET_PA: no mapped PA for memID(%d)\n",
					IonNode.memID);
				mutex_unlock(&cam_mem_ion_mutex[bucketID]);
				break;
			}

			list_for_each(pos, &g_ion_buf_list[bucketID].list) {
				entry = list_entry(pos, struct ION_BUFFER_LIST, list);
				if (entry->memID == IonNode.memID) {
					IonNode.dma_pa = entry->dmaAddr;
					bMapped = true;
					break;
				}
			}

			if (!bMapped) {
				IonNode.dma_pa = 0;
				LOG_NOTICE("GET_PA: never mapped for memID(%d),name(%s)\n",
					IonNode.memID, IonNode.username);
			} else
				LOG_NOTICE("GET_PA: memID(%d) get pa(0x%lx), name(%s)\n",
					IonNode.memID, IonNode.dma_pa, IonNode.username);

			mutex_unlock(&cam_mem_ion_mutex[bucketID]);

			if (copy_to_user((void *)Param, &IonNode,
				sizeof(struct CAM_MEM_DEV_ION_NODE_STRUCT)) != 0) {
				LOG_NOTICE("copy to user fail\n");
				Ret = -EFAULT;
			}
		} else {
			LOG_NOTICE(
				"[CAM_MEM_ION_GET_PA]copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;

	case CAM_MEM_POWER_CTRL:
		if (likely(copy_from_user(&power_on, (void *)Param,
			sizeof(int)) == 0)) {
			mutex_lock(&cam_mem_pwr_ctrl_mutex);
			if (power_on == 1) {
				LOG_DBG("power on\n");
				CamMem_EnableLarb(true);
			} else {
				LOG_DBG("power off\n");
				CamMem_EnableLarb(false);
			}
			mutex_unlock(&cam_mem_pwr_ctrl_mutex);
		} else {
			LOG_NOTICE("CAM_MEM_POWER_CTRL: copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;

	default:
		LOG_NOTICE("Unknown Cmd(%d)\n", Cmd);
		Ret = -EPERM;
		break;
	}

	if (unlikely(Ret != 0))
		LOG_NOTICE(
			"Fail, Cmd(%d), Pid(%d), (process, pid, tgid)=(%s, %d, %d)\n",
			Cmd, pUserInfo->Pid, current->comm, current->pid,
			current->tgid);
	/*  */
	return Ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long cam_mem_ioctl_compat(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		LOG_NOTICE("No op or no unlocked_ioctl\n");
		return -ENOTTY;
	}

	return filp->f_op->unlocked_ioctl(filp, cmd, arg);
}
#endif

/*******************************************************************************
 *
 ******************************************************************************/
static dev_t CamMemDevNo;
static struct cdev *pCamMemCharDrv;
static struct class *pCamMemClass;

static const struct file_operations CamMemFileOper = {
	.owner = THIS_MODULE,
	.open = cam_mem_open,
	.release = cam_mem_release,
	.unlocked_ioctl = cam_mem_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = cam_mem_ioctl_compat,
#endif
};

/*******************************************************************************
 *
 ******************************************************************************/
static inline void cam_mem_UnregCharDev(void)
{
	LOG_NOTICE("+\n");

	/* Release char driver */
	if (pCamMemCharDrv != NULL) {
		cdev_del(pCamMemCharDrv);
		pCamMemCharDrv = NULL;
	}

	unregister_chrdev_region(CamMemDevNo, 1);
}

/*******************************************************************************
 *
 ******************************************************************************/
static inline int cam_mem_RegCharDev(void)
{
	int Ret = 0;

	/*  */
	LOG_NOTICE("+\n");
	/*  */
	Ret = alloc_chrdev_region(&CamMemDevNo, 0, 1, CAM_MEM_DEV_NAME);
	if ((Ret) < 0) {
		LOG_NOTICE("alloc_chrdev_region failed, %d\n", Ret);
		return Ret;
	}
	/* Allocate driver */
	pCamMemCharDrv = cdev_alloc();
	if (pCamMemCharDrv == NULL) {
		LOG_NOTICE("cdev_alloc failed\n");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pCamMemCharDrv, &CamMemFileOper);
	/*  */
	pCamMemCharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pCamMemCharDrv, CamMemDevNo, 1);
	if ((Ret) < 0) {
		LOG_NOTICE("Attatch file operation failed, %d\n", Ret);
		goto EXIT;
	}
/*  */
EXIT:
	if (Ret < 0)
		cam_mem_UnregCharDev();

	LOG_NOTICE("-\n");
	return Ret;
}

/*******************************************************************************
 *
 ******************************************************************************/
/*
 * Before common kernel 5.4 iommu's device link ready.
 * we need to use SMI API to power on bus directly.
 */
static void CamMem_get_larb(struct platform_device *pDev)
{
	struct device_node *node;
	struct platform_device *larb_pdev;
	unsigned int larb_id = 0;
	int i = 0;
	bool bSearchDone = false;
	struct device **tmp_larbs;

	do {
		node = of_parse_phandle(pDev->dev.of_node, "mediatek,larbs", i);

		if (!node) {
			LOG_NOTICE("%s: no more mediatek,larbs found\n",
				pDev->dev.of_node->name);
			bSearchDone = true;
			break;
		}
		larb_pdev = of_find_device_by_node(node);

		if (of_property_read_u32(node, "mediatek,larb-id", &larb_id))
			LOG_NOTICE("Error: get larb id from DTS fail!!\n");
		else
			LOG_NOTICE("%s gets larb_id=%d\n",
				pDev->dev.of_node->name, larb_id);

		of_node_put(node);
		if (!larb_pdev) {
			LOG_NOTICE("%s: no mediatek,larb device found\n",
				pDev->dev.of_node->name);
			bSearchDone = true;
			break;
		}

		tmp_larbs = krealloc(cam_mem_dev.larbs, sizeof(struct device *) * (i + 1),
			GFP_KERNEL);

		if (tmp_larbs == NULL) {
			LOG_NOTICE("Error: krealloc fail!!\n");
			bSearchDone = true;
			break;
		}

		cam_mem_dev.larbs = tmp_larbs;

		cam_mem_dev.larbs[i] = &larb_pdev->dev;
		i++;

		LOG_NOTICE("%s: get %s\n", pDev->dev.of_node->name,
			larb_pdev->dev.of_node->name);
	} while (!bSearchDone);

	g_larb_num = i;
}

/*******************************************************************************
 *
 ******************************************************************************/
static int cam_mem_buf_list_read(struct seq_file *m, void *v)
{
	struct ION_BUFFER_LIST *entry;
	struct list_head *pos;
	int i = 0, j = 0;
	unsigned long long total_size = 0;

	seq_puts(m, " idx memID        pa     aligned-size   refCnt       user        timestamp\n");
	seq_puts(m, "==================================================================\n");

	for (j = 0; j < HASH_BUCKET_NUM; j++) {

		mutex_lock(&cam_mem_ion_mutex[j]);

		if (list_empty(&g_ion_buf_list[j].list)) {
			mutex_unlock(&cam_mem_ion_mutex[j]);
			continue;
		}

		list_for_each(pos, &g_ion_buf_list[j].list) {
			unsigned long long second;
			unsigned long long nano_second;

			entry = list_entry(pos, struct ION_BUFFER_LIST, list);

			second = entry->timestamp;
			nano_second = do_div(second, 1000000000);

			total_size += entry->dmaBuf->size;

			seq_printf(m, "#%03d   %3d   0x%09lx  0x%07zx     %2d   %-32s %llu.%llu\n",
				i,
				entry->memID,
				entry->dmaAddr, entry->dmaBuf->size,
				entry->refCnt, entry->username, second, nano_second);
				i++;
		}
		mutex_unlock(&cam_mem_ion_mutex[j]);
	}
	if (total_size > 0) {
		seq_puts(m, "==================================================================\n");
		seq_printf(m, "           Total Size= 0x%llx (%lld KBytes)\n",
			total_size, total_size >> 10);
	}

	return 0;
};

static int proc_open_cam_mem_buf_list(struct inode *inode, struct file *file)
{
	return single_open(file, cam_mem_buf_list_read, NULL);
};

static const struct proc_ops fcam_mem_proc_fops = {
	.proc_open = proc_open_cam_mem_buf_list,
	.proc_read = seq_read,
};

/*******************************************************************************
 *
 ******************************************************************************/
static int cam_mem_probe(struct platform_device *pDev)
{
	int Ret = 0;
	struct device *dev = NULL;
	unsigned int bit_mask_val = 0;

	LOG_NOTICE("+\n");

	if (pDev == NULL) {
		LOG_NOTICE("Error: pDev is NULL\n");
		return -ENXIO;
	}

	atomic_inc(&G_u4DevNodeCt);

	cam_mem_dev.dev = &pDev->dev;

	Ret = cam_mem_RegCharDev();
	if ((Ret)) {
		LOG_NOTICE("Error: register char failed");
		return Ret;
	}

	CamMem_get_larb(pDev);

#if IS_ENABLED(CONFIG_ARM64)
	bit_mask_val = 34;
#else
	bit_mask_val = 31;
#endif
	if (dma_set_mask_and_coherent(&pDev->dev, DMA_BIT_MASK(bit_mask_val)))
		LOG_NOTICE("%s: No suitable DMA available, DMA_BIT_MASK(%d)\n",
			pDev->dev.of_node->name, bit_mask_val);
	else
		LOG_INF("dma_set_mask_and_coherent(%s, DMA_BIT_MASK(%d))\n",
			pDev->dev.of_node->name, bit_mask_val);

	/* Create class register */
	pCamMemClass = class_create(THIS_MODULE, "CamMemDrv");
	if (IS_ERR(pCamMemClass)) {
		Ret = PTR_ERR(pCamMemClass);
		LOG_NOTICE("Unable to create class, err = %d\n", Ret);
		goto EXIT;
	}
	dev = device_create(pCamMemClass, NULL, CamMemDevNo, NULL,
			    CAM_MEM_DEV_NAME);

	if (IS_ERR(dev)) {
		Ret = PTR_ERR(dev);
		LOG_NOTICE(
			"Error: Failed to create device: /dev/%s, err = %d\n",
			CAM_MEM_DEV_NAME, Ret);

		goto EXIT;
	}

	spin_lock_init(&(CamMemInfo.SpinLock_CamMemRef));
	spin_lock_init(&(CamMemInfo.SpinLock_Larb));

	proc_create("driver/cam_mem_buf_list", 0000, NULL, &fcam_mem_proc_fops);

EXIT:
	if (Ret < 0)
		cam_mem_UnregCharDev();

	LOG_NOTICE("-\n");
	return Ret;
}


/*******************************************************************************
 * Called when the device is being detached from the driver
 ******************************************************************************/
static int cam_mem_remove(struct platform_device *pDev)
{
	LOG_NOTICE("+\n");

	/* unregister char driver. */
	cam_mem_UnregCharDev();

	/*  */
	device_destroy(pCamMemClass, CamMemDevNo);
	/*  */
	class_destroy(pCamMemClass);
	pCamMemClass = NULL;

	remove_proc_entry("driver/cam_mem_buf_list", NULL);

	kfree(cam_mem_dev.larbs);

	return 0;
}

/*******************************************************************************
 *
 ******************************************************************************/
static int cam_mem_suspend(struct platform_device *pDev, pm_message_t Mesg)
{
	return 0;
}

/*******************************************************************************
 *
 ******************************************************************************/
static int cam_mem_resume(struct platform_device *pDev)
{
	return 0;
}

/*******************************************************************************
 *
 ******************************************************************************/
static const struct of_device_id cam_mem_of_ids[] = {
	{
		.compatible = "mediatek,cam_mem",
	},
	{}
};

static struct platform_driver CamMemDriver = {
	.probe = cam_mem_probe,
	.remove = cam_mem_remove,
	.suspend = cam_mem_suspend,
	.resume = cam_mem_resume,
	.driver = {
		.name = CAM_MEM_DEV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = cam_mem_of_ids,
#endif
	}
};

#if IS_ENABLED(CONFIG_PM)
/*******************************************************************************
 *
 ******************************************************************************/
static int cam_mem_pm_event_suspend(void)
{
	int ret = 0;
	unsigned int loopCnt;

	/* update device node count */
	atomic_dec(&G_u4DevNodeCt);

	/* Check larb counter instead of check CamMemInfo.UserCount
	 *  for ensuring current larbs are on or off
	 */
	spin_lock(&(CamMemInfo.SpinLock_Larb));
	if (!G_u4EnableLarbCount) {
		spin_unlock(&(CamMemInfo.SpinLock_Larb));

		if (CamMemInfo.UserCount != 0) {
			LOG_INF("X. UserCount=%d,Cnt:%d,devct:%d\n",
				CamMemInfo.UserCount,
				G_u4EnableLarbCount,
				atomic_read(&G_u4DevNodeCt));
		}

		return ret;
	}
	spin_unlock(&(CamMemInfo.SpinLock_Larb));

	/* last dev node will disable larb "G_u4EnableLarbCount" times */
	if (!atomic_read(&G_u4DevNodeCt)) {
		spin_lock(&(CamMemInfo.SpinLock_Larb));
		loopCnt = G_u4EnableLarbCount;
		spin_unlock(&(CamMemInfo.SpinLock_Larb));

		LOG_INF("X. last dev node,disable larb %d time\n",
			loopCnt);
		while (loopCnt > 0) {
			CamMem_EnableLarb(false);
			loopCnt--;
		}
	}

	return 0;
}

/*******************************************************************************
 *
 ******************************************************************************/
static void cam_mem_pm_event_resume(void)
{
	/* update device node count */
	atomic_inc(&G_u4DevNodeCt);

	if (CamMemInfo.UserCount == 0) {
		LOG_DBG("- X. UserCount=0\n");

		return;
	}

	CamMem_EnableLarb(true);

	LOG_INF("EnableLarbCount:%d,devct:%d\n",
		G_u4EnableLarbCount,
		atomic_read(&G_u4DevNodeCt));
}

/*******************************************************************************
 *
 ******************************************************************************/
static int cam_mem_suspend_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE: /* before enter suspend */
		cam_mem_pm_event_suspend();
		return NOTIFY_DONE;
	case PM_POST_SUSPEND: /* after resume */
		cam_mem_pm_event_resume();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block cam_mem_suspend_pm_notifier_func = {
	.notifier_call = cam_mem_suspend_pm_event,
	.priority = 0,
};
#endif

/*******************************************************************************
 *
 ******************************************************************************/
static int __init cam_mem_Init(void)
{
	int Ret = 0;
	int i = 0;

	LOG_NOTICE("+\n");

	mutex_init(&open_cam_mem_mutex);
	mutex_init(&cam_mem_pwr_ctrl_mutex);

	atomic_set(&G_u4DevNodeCt, 0);

	g_platform_id = GET_PLATFORM_ID("mediatek,cam_mem");
	LOG_NOTICE("g_platform_id = 0x%x\n", g_platform_id);
	Ret = platform_driver_register(&CamMemDriver);
	if ((Ret) < 0) {
		LOG_NOTICE("platform_driver_register fail");
		return Ret;
	}

	for (i = 0; i < HASH_BUCKET_NUM; i++) {
		mutex_init(&cam_mem_ion_mutex[i]);
		INIT_LIST_HEAD(&g_ion_buf_list[i].list);
	}

#if IS_ENABLED(CONFIG_PM)
	Ret = register_pm_notifier(&cam_mem_suspend_pm_notifier_func);
	if (Ret) {
		LOG_NOTICE("Failed to register PM notifier.\n");
		return Ret;
	}
#endif

	LOG_NOTICE("-\n");
	return Ret;
}

/*******************************************************************************
 *
 ******************************************************************************/
static void __exit cam_mem_Exit(void)
{
	LOG_NOTICE("+\n");
	/*  */
	platform_driver_unregister(&CamMemDriver);

	LOG_NOTICE("-\n");
}


module_init(cam_mem_Init);
module_exit(cam_mem_Exit);
MODULE_DESCRIPTION("Camera Mem driver");
MODULE_AUTHOR("SW7");
MODULE_LICENSE("GPL");
