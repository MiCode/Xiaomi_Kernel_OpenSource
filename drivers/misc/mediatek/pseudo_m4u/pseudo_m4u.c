/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/cdev.h>
#include <linux/iommu.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/dma-iommu.h>
#ifndef CONFIG_ARM64
#include <asm/dma-iommu.h>
#include <asm/memory.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <linux/highmem.h>
#include <asm/memory.h>
#else
#include <asm/system_misc.h> /* for build of show_pte in a64. */
#endif
#include <soc/mediatek/smi.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/memblock.h>
#include <asm/cacheflush.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/pagemap.h>
#include <linux/compat.h>
#include "pseudo_m4u.h"
#include "pseudo_m4u_log.h"
#include "mt_smi.h"

int m4u_log_level = 2;
int m4u_log_to_uart = 2;

LIST_HEAD(pseudo_sglist);
/* this is the mutex lock to protect mva_sglist->list*/
static DEFINE_MUTEX(pseudo_list_mutex);

static const struct of_device_id mtk_pseudo_of_ids[] = {
	{ .compatible = "mediatek,mtk-pseudo-m4u",},
	{}
};

static struct device *pseudo_m4u_dev;

/* garbage collect related */

struct m4u_device *gM4uDev;

/*
 * will define userspace port id according to kernel
 */
static inline int m4u_user2kernel_port(int userport)
{
	return userport;
}

static inline int m4u_kernel2user_port(int kernelport)
{
	return kernelport;
}

static int MTK_M4U_flush(struct file *a_pstFile, fl_owner_t a_id)
{
	return 0;
}

struct m4u_client_t *m4u_create_client(void)
{
	struct m4u_client_t *client;

	client = kmalloc(sizeof(struct m4u_client_t), GFP_ATOMIC);
	if (!client)
		return NULL;

	mutex_init(&(client->dataMutex));
	mutex_lock(&(client->dataMutex));
	client->open_pid = current->pid;
	client->open_tgid = current->tgid;
	INIT_LIST_HEAD(&(client->mvaList));
	mutex_unlock(&(client->dataMutex));

	return client;
}
EXPORT_SYMBOL(m4u_create_client);

int m4u_destroy_client(struct m4u_client_t *client)
{
	struct m4u_buf_info_t *pMvaInfo;
	unsigned int mva, size;
	int port;

	while (1) {
		mutex_lock(&(client->dataMutex));
		if (list_empty(&client->mvaList)) {
			mutex_unlock(&(client->dataMutex));
			break;
		}
		pMvaInfo = container_of(client->mvaList.next,
					struct m4u_buf_info_t, link);
		M4U_MSG(
			"warnning: clean garbage at m4u close: module=%s,va=0x%lx,mva=0x%x,size=%d\n",
			m4u_get_port_name(pMvaInfo->port), pMvaInfo->va,
			pMvaInfo->mva, pMvaInfo->size);

		port = pMvaInfo->port;
		mva = pMvaInfo->mva;
		size = pMvaInfo->size;

		mutex_unlock(&(client->dataMutex));

		/* m4u_dealloc_mva will lock client->dataMutex again */
		pseudo_dealloc_mva(client, port, mva);
	}

	kfree(client);

	return 0;
}
EXPORT_SYMBOL(m4u_destroy_client);

static int MTK_M4U_open(struct inode *inode, struct file *file)
{
	struct m4u_client_t *client;

	M4U_DBG("enter %s process : %s\n", __func__, current->comm);
	client = m4u_create_client();
	if (IS_ERR_OR_NULL(client)) {
		M4U_ERR("createclientfail\n");
		return -ENOMEM;
	}

	file->private_data = client;

	return 0;
}

static int MTK_M4U_release(struct inode *inode, struct file *file)
{
	struct m4u_client_t *client = file->private_data;

	M4U_DBG("enter %s process : %s\n", __func__, current->comm);
	m4u_destroy_client(client);
	return 0;
}

/*
 * for mt8127 implement, the tee service call just implement the config port
 * feature.
 * there's no caller of m4u_alloc_mva_sec, both in normal world nor in security
 * world. since the security world did not reserve the mva range in normal
 * world. That's to say, we just need to handle config port, and do not need to
 * handle alloc mva sec dealloc mva sec etc.
 */
#ifdef M4U_TEE_SERVICE_ENABLE

#include "tz_cross/trustzone.h"
#include "trustzone/kree/system.h"
#include "tz_cross/ta_m4u.h"

KREE_SESSION_HANDLE m4u_session;
bool m4u_tee_en;

static DEFINE_MUTEX(gM4u_port_tee);
static int pseudo_m4u_session_init(void)
{
	int ret;

	ret = KREE_CreateSession(TZ_TA_M4U_UUID, &m4u_session);
	if (ret != TZ_RESULT_SUCCESS) {
		M4U_ERR("m4u CreateSession error %d\n", ret);
		return -1;
	}
	M4U_MSG("create session : 0x%x\n", (unsigned int)m4u_session);
	m4u_tee_en = true;
	return 0;
}

int m4u_larb_restore_sec(unsigned int larb_idx)
{
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	int ret;

	if (!m4u_tee_en)  /*tee may not init*/
		return -2;

	if (larb_idx == 0 || larb_idx == 4) { /*only support disp*/
		param[0].value.a = larb_idx;
		paramTypes = TZ_ParamTypes1(TZPT_VALUE_INPUT);

		ret = KREE_TeeServiceCall(m4u_session,
			M4U_TZCMD_LARB_REG_RESTORE,
			paramTypes, param);
		if (ret != TZ_RESULT_SUCCESS) {
			M4U_ERR("m4u reg backup SeviceCall error %d\n", ret);
			return -1;
		}
	}
	return 0;
}

int m4u_larb_backup_sec(unsigned int larb_idx)
{
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	int ret;

	if (!m4u_tee_en)  /*tee may not init */
		return -2;

	if (larb_idx == 0 || larb_idx == 4) { /*only support disp*/
		param[0].value.a = larb_idx;
		paramTypes = TZ_ParamTypes1(TZPT_VALUE_INPUT);

		ret = KREE_TeeServiceCall(m4u_session,
					M4U_TZCMD_LARB_REG_BACKUP,
					paramTypes, param);
		if (ret != TZ_RESULT_SUCCESS) {
			M4U_ERR("reg backup ServiceCall error %d\n", ret);
			return -1;
		}
	}
	return 0;
}

int smi_reg_backup_sec(void)
{
	uint32_t paramTypes;
	int ret;

	paramTypes = TZ_ParamTypes1(TZPT_NONE);
	ret = KREE_TeeServiceCall(m4u_session, M4U_TZCMD_REG_BACKUP,
				paramTypes, NULL);
	if (ret != TZ_RESULT_SUCCESS) {
		M4U_ERR("m4u reg backup ServiceCall error %d\n", ret);
		return -1;
	}
	return 0;
}

int smi_reg_restore_sec(void)
{
	uint32_t paramTypes;
	int ret;

	paramTypes = TZ_ParamTypes1(TZPT_NONE);
	ret = KREE_TeeServiceCall(m4u_session, M4U_TZCMD_REG_RESTORE,
				paramTypes, NULL);
	if (ret != TZ_RESULT_SUCCESS) {
		M4U_ERR("m4u reg backup ServiceCall error %d\n", ret);
		return -1;
	}

	return 0;
}

int pseudo_m4u_do_config_port(struct M4U_PORT_STRUCT *pM4uPort)
{
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	int ret;

	/* do not config port if session has not been inited. */
	if (!m4u_session)
		return 0;

	param[0].value.a = pM4uPort->ePortID;
	param[0].value.b = pM4uPort->Virtuality;
	param[1].value.a = pM4uPort->Distance;
	param[1].value.b = pM4uPort->Direction;

	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);

	mutex_lock(&gM4u_port_tee);
	ret = KREE_TeeServiceCall(m4u_session, M4U_TZCMD_CONFIG_PORT,
				  paramTypes, param);
	mutex_unlock(&gM4u_port_tee);

	if (ret != TZ_RESULT_SUCCESS)
		M4U_ERR("m4u_config_port ServiceCall error 0x%x\n", ret);

	return 0;
}

static int pseudo_m4u_sec_init(unsigned int u4NonSecPa,
			unsigned int L2_enable, unsigned int *security_mem_size)
{
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	int ret;

	param[0].value.a = u4NonSecPa;
	param[0].value.b = 0;/* 4gb */
	param[1].value.a = 1;
	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT,
					TZPT_VALUE_OUTPUT);
	ret = KREE_TeeServiceCall(m4u_session, M4U_TZCMD_SEC_INIT,
			paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS) {
		M4U_ERR("m4u sec init error 0x%x\n", ret);
		return -1;
	}

	*security_mem_size = param[1].value.a;
	return 0;
}

/* the caller should enable smi clock, it should be only called by mtk_smi.c */
int pseudo_config_port_tee(int kernelport)
{
	struct M4U_PORT_STRUCT pM4uPort;

	pM4uPort.ePortID = m4u_kernel2user_port(kernelport);
	pM4uPort.Virtuality = 1;
	pM4uPort.Distance = 1;
	pM4uPort.Direction = 1;

	return pseudo_m4u_do_config_port(&pM4uPort);
}

/* Only for debug. If the port is nonsec, dump 0 for it. */
int m4u_dump_secpgd(unsigned int larbid, unsigned int portid,
		    unsigned long fault_mva)
{
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	int ret;

	param[0].value.a = larbid << 5 | portid;
	param[0].value.b = fault_mva & 0xfffff000;
	param[1].value.a = 0xf;
	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT);
	ret = KREE_TeeServiceCall(m4u_session, M4U_TZCMD_SECPGTDUMP,
			paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS) {
		M4U_ERR("m4u sec dump error 0x%x\n", ret);
		return -1;
	}

	return param[1].value.a;
}
#endif

int m4u_dma_cache_flush_all(void)
{
	/* L1 cache clean before hw read */
	smp_inner_dcache_flush_all();

	/* L2 cache maintenance by physical pages */
	outer_flush_all();

	return 0;
}

/* Return a device for iommu ops. */
struct device *m4u_get_larbdev(int portid)
{
	return pseudo_m4u_dev;
}

int m4u_config_port(struct M4U_PORT_STRUCT *pM4uPort)
{
	int ret = 0;
#ifdef M4U_TEE_SERVICE_ENABLE
	/* Enable larb's clock. */
	ret = pseudo_m4u_do_config_port(pM4uPort);
#endif
	return ret;
}

int m4u_config_port_array(struct m4u_port_array *port_array)
{
	return 0;
}

/* static struct iova_domain *giovad; */
#ifndef CONFIG_ARM64
static int __arm_coherent_iommu_map_sg(struct device *dev,
		struct scatterlist *sg,
		int nents, enum dma_data_direction dir, unsigned long attrs);

static void __arm_coherent_iommu_unmap_sg(struct device *dev,
		struct scatterlist *sg,
		int nents, enum dma_data_direction dir, unsigned long attrs);
#endif


static void m4u_show_pte(struct mm_struct *mm, unsigned long addr)
{
#ifndef CONFIG_ARM64
	show_pte(mm, addr);
#else
	show_pte(addr);
#endif
}

/*
 * since the device have been attached, then we get from the dma_ops->map_sg is
 * arm_iommu_map_sg
 */
static int __m4u_alloc_mva(int port, unsigned long va, unsigned int size,
			   struct sg_table *sg_table, unsigned int *retmva)
{
	struct mva_sglist *mva_sg;
	struct sg_table *table = NULL;
	int ret, kernelport = m4u_user2kernel_port(port);
	struct device *dev = m4u_get_larbdev(kernelport);

	dma_addr_t dma_addr;

	if (!va && !sg_table) {
		M4U_ERR("va and sg_table are all NULL\n");
		return -EINVAL;
	}

	/* this is for ion mm heap and ion fb heap usage. */
	if (sg_table) {
		struct scatterlist *s = sg_table->sgl, *ng;
		phys_addr_t phys;
		int i;

		table = kzalloc(sizeof(*table), GFP_KERNEL);
		if (!table)
			return -ENOMEM;
		ret = sg_alloc_table(table, sg_table->nents, GFP_KERNEL);
		if (ret) {
			kfree(table);
			*retmva = 0;
			return ret;
		}

		ng = table->sgl;

		for (i = 0; i < sg_table->nents; i++) {
			phys = sg_phys(s);
			size += s->length;
			sg_set_page(ng, sg_page(s), s->length, s->offset);
			s = sg_next(s);
			ng = sg_next(ng);
		}
	}

	if (!table) {
		table = m4u_create_sgtable(va, size);
		/* table = pseudo_get_sg(port, va, size); */
		if (IS_ERR_OR_NULL(table)) {
			table = NULL;
			M4U_ERR("pseudo_get_sg failed\n");
			ret = -EINVAL;
			goto err;
		}
	}

	if (sg_table) {
#ifdef CONFIG_ARM64
		iommu_dma_map_sg(dev, table->sgl, table->nents,
				 IOMMU_READ | IOMMU_WRITE);
#else
		__arm_coherent_iommu_map_sg(dev, table->sgl, table->nents,
					    1, 0);
#endif
		dma_addr = sg_dma_address(table->sgl);
	} else {
#ifdef CONFIG_ARM64
		iommu_dma_map_sg(dev, table->sgl, table->orig_nents,
				 IOMMU_READ | IOMMU_WRITE);
#else
		__arm_coherent_iommu_map_sg(dev, table->sgl, table->orig_nents,
					    1, 0);
#endif
		dma_addr = sg_dma_address(table->sgl);
	}

#ifdef CONFIG_ARM64
	if (dma_addr == 0) {
#else
	if (dma_addr == ARM_MAPPING_ERROR) {
#endif
		M4U_ERR(
			"%s, %d alloc mva failed, port is %s, dma_address is 0x%lx, size is 0x%x\n",
			__func__, __LINE__, m4u_get_port_name(port),
			(unsigned long)dma_addr, size);
		M4U_ERR(
			"SUSPECT that iova have been all exhaust, maybe there's someone hold too much mva\n");
		ret = -ENOMEM;
		goto err;
	}

	*retmva = dma_addr;

	mva_sg = kzalloc(sizeof(*mva_sg), GFP_KERNEL);
	if (!mva_sg) {
		ret = -ENOMEM;
		goto err;
	}
	mva_sg->table = table;
	mva_sg->mva = *retmva;

	m4u_add_sgtable(mva_sg);

	M4U_DBG("%s, %d mva is 0x%x, dma_address is 0x%lx, size is 0x%x\n",
		__func__, __LINE__, mva_sg->mva, (unsigned long)dma_addr, size);
	return 0;

err:
	if (table) {
		sg_free_table(table);
		kfree(table);
	}
	*retmva = 0;
	return ret;
}

static struct m4u_buf_info_t *m4u_alloc_buf_info(void)
{
	struct m4u_buf_info_t *pList = NULL;

	pList = kzalloc(sizeof(struct m4u_buf_info_t), GFP_KERNEL);
	if (pList == NULL)
		return NULL;
	M4U_DBG("pList size %d, ptr %p\n",
		(int)sizeof(struct m4u_buf_info_t), pList);
	INIT_LIST_HEAD(&(pList->link));
	return pList;
}

static int m4u_free_buf_info(struct m4u_buf_info_t *pList)
{
	kfree(pList);
	return 0;
}

static int m4u_client_add_buf(struct m4u_client_t *client,
			      struct m4u_buf_info_t *pList)
{
	mutex_lock(&(client->dataMutex));
	list_add(&(pList->link), &(client->mvaList));
	mutex_unlock(&(client->dataMutex));

	return 0;
}

/***********************************************************/
/** find or delete a buffer from client list
 * @param   client   -- client to be searched
 * @param   mva      -- mva to be searched
 * @param   del      -- should we del this buffer from client?
 *
 * @return buffer_info if found, NULL on fail
 * @remark
 * @see
 * @to-do    we need to add multi domain support here.
 * @author K Zhang      @date 2013/11/14
 ************************************************************/
static struct m4u_buf_info_t *m4u_client_find_buf(struct m4u_client_t *client,
						  unsigned int mva, int del)
{
	struct list_head *pListHead;
	struct m4u_buf_info_t *pList = NULL;
	struct m4u_buf_info_t *ret = NULL;

	if (client == NULL) {
		M4U_ERR("m4u_delete_from_garbage_list(), client is NULL!\n");
		return NULL;
	}

	mutex_lock(&(client->dataMutex));
	list_for_each(pListHead, &(client->mvaList)) {
		pList = container_of(pListHead, struct m4u_buf_info_t, link);
		if (pList->mva == mva)
			break;
	}
	if (pListHead == &(client->mvaList)) {
		ret = NULL;
	} else {
		if (del)
			list_del(pListHead);
		ret = pList;
	}


	mutex_unlock(&(client->dataMutex));

	return ret;
}

/* interface for ion */
static struct m4u_client_t *ion_m4u_client;

int m4u_alloc_mva_sg(struct port_mva_info_t *port_info,
				struct sg_table *sg_table)
{
	if (!ion_m4u_client) {
		ion_m4u_client = m4u_create_client();
		if (IS_ERR_OR_NULL(ion_m4u_client)) {
			ion_m4u_client = NULL;
			return -1;
		}
	}

	return pseudo_alloc_mva(ion_m4u_client, port_info->emoduleid, 0,
				sg_table, port_info->buf_size, 0,
				port_info->flags, &port_info->mva);
}


int pseudo_alloc_mva(struct m4u_client_t *client, int port,
			  unsigned long va, struct sg_table *sg_table,
			  unsigned int size, unsigned int prot,
			  unsigned int flags, unsigned int *pMva)
{
	int ret, offset;
	struct m4u_buf_info_t *pbuf_info;
	unsigned int mva = 0;
	unsigned long va_align = va;
	unsigned int mva_align, size_align = size;

	/* align the va to allocate continues iova. */
	offset = m4u_va_align(&va_align, &size_align);
	/* pbuf_info for userspace compatible */
	pbuf_info = m4u_alloc_buf_info();

	pbuf_info->va = va;
	pbuf_info->port = port;
	pbuf_info->size = size;
	pbuf_info->prot = prot;
	pbuf_info->flags = flags;
	pbuf_info->sg_table = sg_table;

	ret = __m4u_alloc_mva(port, va_align, size_align, sg_table, &mva_align);
	if (ret) {
		M4U_ERR("error alloc mva, %s, %d\n", __func__, __LINE__);
		mva = 0;
		goto err;
	}

	mva = mva_align + offset;
	pbuf_info->mva = mva;
	*pMva = mva;

	m4u_client_add_buf(client, pbuf_info);

	return 0;
err:
	m4u_free_buf_info(pbuf_info);
	return ret;
}

int pseudo_dealloc_mva(struct m4u_client_t *client, int port, unsigned int mva)
{
	struct m4u_buf_info_t *pMvaInfo;
	int offset, ret;

	pMvaInfo = m4u_client_find_buf(client, mva, 1);

	offset = m4u_va_align(&pMvaInfo->va, &pMvaInfo->size);
	pMvaInfo->mva -= offset;

	ret = __m4u_dealloc_mva(port, pMvaInfo->va, pMvaInfo->size, mva, NULL);
	if (ret)
		return ret;

	m4u_free_buf_info(pMvaInfo);
	return ret;

}

int m4u_dealloc_mva_sg(int eModuleID,
		       struct sg_table *sg_table,
		       const unsigned int BufSize, const unsigned int MVA)
{
	if (!sg_table) {
		M4U_ERR("%s, %d, sg_table is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	return __m4u_dealloc_mva(eModuleID, 0, BufSize, MVA, sg_table);
}

struct sg_table *m4u_find_sgtable(unsigned int mva)
{
	struct mva_sglist *entry;

	mutex_lock(&pseudo_list_mutex);

	list_for_each_entry(entry, &pseudo_sglist, list) {
		if (entry->mva == mva) {
			mutex_unlock(&pseudo_list_mutex);
			return entry->table;
		}
	}

	mutex_unlock(&pseudo_list_mutex);
	return NULL;
}

struct sg_table *m4u_del_sgtable(unsigned int mva)
{
	struct mva_sglist *entry, *tmp;
	struct sg_table *table;

	M4U_DBG("%s, %d, mva = 0x%x\n", __func__, __LINE__, mva);
	mutex_lock(&pseudo_list_mutex);
	list_for_each_entry_safe(entry, tmp, &pseudo_sglist, list) {
		M4U_DBG("%s, %d, entry->mva = 0x%x\n", __func__, __LINE__,
			entry->mva);
		if (entry->mva == mva) {
			list_del(&entry->list);
			mutex_unlock(&pseudo_list_mutex);
			table = entry->table;
			M4U_DBG("%s, %d, mva is 0x%x, entry->mva is 0x%x\n",
				__func__, __LINE__, mva, entry->mva);
			kfree(entry);
			return table;
		}
	}
	mutex_unlock(&pseudo_list_mutex);

	return NULL;
}

struct sg_table *m4u_add_sgtable(struct mva_sglist *mva_sg)
{
	struct sg_table *table;

	table = m4u_find_sgtable(mva_sg->mva);
	if (table)
		return table;

	table = mva_sg->table;
	mutex_lock(&pseudo_list_mutex);
	list_add(&mva_sg->list, &pseudo_sglist);
	mutex_unlock(&pseudo_list_mutex);

	M4U_DBG("adding pseudo_sglist, mva = 0x%x\n", mva_sg->mva);
	return table;
}

/* make sure the va size is page aligned to get the continues iova. */
int m4u_va_align(unsigned long *addr, unsigned int *size)
{
	int offset, remain;

	/* we need to align the bufaddr to make sure the iova is continues */
	offset = *addr & (M4U_PAGE_SIZE - 1);
	if (offset) {
		*addr &= ~(M4U_PAGE_SIZE - 1);
		*size += offset;
	}

	/* make sure we alloc one page size iova at least */
	remain = *size % M4U_PAGE_SIZE;
	if (remain)
		*size += M4U_PAGE_SIZE - remain;
	/* dma32 would skip the last page, we added it here */
	/* *size += PAGE_SIZE; */
	return offset;
}

/* put ref count on all pages in sgtable */
int m4u_put_sgtable_pages(struct sg_table *table, int nents)
{
	int i;
	struct scatterlist *sg;

	for_each_sg(table->sgl, sg, nents, i) {
		struct page *page = sg_page(sg);

		if (IS_ERR(page))
			return 0;
		if (page) {
			if (!PageReserved(page))
				SetPageDirty(page);
			put_page(page);
		}
	}
	return 0;
}
static int m4u_dump_mmaps(unsigned long addr)
{
	struct vm_area_struct *vma;

	M4U_MSG(
		"addr=0x%lx, name=%s, pid=0x%x,",
		addr, current->comm, current->pid);

	vma = find_vma(current->mm, addr);

	if (vma && (addr >= vma->vm_start)) {
		M4U_MSG("find vma: 0x%16lx-0x%16lx, flags=0x%lx\n",
			   (vma->vm_start), (vma->vm_end), vma->vm_flags);
		return 0;
	}

	M4U_ERR("cannot find vma for addr 0x%lx\n", addr);
	return -1;
}

/* to-do: need modification to support 4G DRAM */
static phys_addr_t m4u_user_v2p(unsigned long va)
{
	unsigned long pageOffset = (va & (PAGE_SIZE - 1));
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	phys_addr_t pa;

	if (current == NULL) {
		M4U_ERR("warning: %s, current is NULL!\n",
			__func__);
		return 0;
	}
	if (current->mm == NULL) {
		M4U_ERR(
			"warning: %s, current->mm is NULL! tgid=0x%x, name=%s\n",
		       __func__, current->tgid, current->comm);
		return 0;
	}

	pgd = pgd_offset(current->mm, va);	/* what is tsk->mm */
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		M4U_ERR("%s(), va=0x%lx, pgd invalid!\n",
			__func__, va);
		return 0;
	}

	pud = pud_offset(pgd, va);
	if (pud_none(*pud) || pud_bad(*pud)) {
		M4U_ERR("%s(), va=0x%lx, pud invalid!\n",
			__func__, va);
		return 0;
	}

	pmd = pmd_offset(pud, va);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		M4U_ERR("%s(), va=0x%lx, pmd invalid!\n",
				__func__, va);
		return 0;
	}

	pte = pte_offset_map(pmd, va);
	if (pte_present(*pte)) {
		/* pa=(pte_val(*pte) & (PAGE_MASK)) | pageOffset; */
		pa = (pte_val(*pte) & (PHYS_MASK) &
			(~((phys_addr_t) 0xfff))) | pageOffset;
		pte_unmap(pte);
		return pa;
	}

	pte_unmap(pte);

	M4U_ERR("%s(), va=0x%lx, pte invalid!\n",
		__func__, va);
	return 0;
}

#if 0
static int m4u_fill_sgtable_user(struct vm_area_struct *vma,
		unsigned long va, int page_num,
				 struct scatterlist **pSg, int has_page)
{
	unsigned long va_align;
	phys_addr_t pa = 0;
	int i;
	long ret = 0;
	struct scatterlist *sg = *pSg;
	struct page *pages;
	int gup_flags;

	va_align = round_down(va, PAGE_SIZE);
	gup_flags = FOLL_TOUCH | FOLL_POPULATE | FOLL_MLOCK;
	if (vma->vm_flags & VM_LOCKONFAULT)
		gup_flags &= ~FOLL_POPULATE;
	/*
	 * We want to touch writable mappings with a write fault in order
	 * to break COW, except for shared mappings because these don't COW
	 * and we would not want to dirty them for nothing.
	 */
	if ((vma->vm_flags & (VM_WRITE | VM_SHARED)) == VM_WRITE)
		gup_flags |= FOLL_WRITE;

	/*
	 * We want mlock to succeed for regions that have any permissions
	 * other than PROT_NONE.
	 */
	if (vma->vm_flags & (VM_READ | VM_WRITE | VM_EXEC))
		gup_flags |= FOLL_FORCE;


	for (i = 0; i < page_num; i++) {
		int fault_cnt;
		unsigned long va_tmp = va_align+i*PAGE_SIZE;

		pa = 0;

		for (fault_cnt = 0; fault_cnt < 3000; fault_cnt++) {
			if (has_page) {
				ret = get_user_pages(va_tmp, 1,
							 gup_flags,
							 &pages, NULL);

				if (ret == 1)
					pa = page_to_phys(pages) |
						(va_tmp & ~PAGE_MASK);
			} else {
				pa = m4u_user_v2p(va_tmp);
				if (!pa) {
					handle_mm_fault(vma, va_tmp,
					(vma->vm_flags & VM_WRITE) ?
					FAULT_FLAG_WRITE : 0);
				}
			}

			if (pa) {
				/* Add one line comment for avoid
				 *kernel coding style, WARNING:BRACES:
				 */
				break;
			}
			cond_resched();
		}

		if (!pa || !sg) {
			struct vm_area_struct *vma_temp;

			M4U_ERR("%s: fail(0x%lx) va=0x%lx,page_num=0x%x\n",
				__func__, ret, va, page_num);
			M4U_ERR("%s: fail_va=0x%lx,pa=0x%lx,sg=0x%p,i=%d\n",
				__func__, va_tmp, (unsigned long)pa, sg, i);
			vma_temp = find_vma(current->mm, va_tmp);
			if (vma_temp != NULL) {
				M4U_ERR(
					"vm_start=0x%lx,vm_end=%lx,vm_flg=%lx\n",
					vma->vm_start, vma->vm_end,
					vma->vm_flags);
				M4U_ERR(
					"vma_temp_start=0x%lx, vma_temp_end=%lx,vm_temp_flag= %lx\n",
					vma_temp->vm_start,
					vma_temp->vm_end, vma_temp->vm_flags);
			}

			m4u_show_pte(current->mm, va_tmp);
			m4u_dump_mmaps(va);
			m4u_dump_mmaps(va_tmp);
			return -1;
		}

		if (fault_cnt > 2) {
			M4U_ERR("warning: handle_mm_fault for %d times\n",
				fault_cnt);
			m4u_show_pte(current->mm, va_tmp);
			m4u_dump_mmaps(va_tmp);
		}
		/* debug check... */
		if ((pa & (PAGE_SIZE - 1)) != 0) {
			M4U_ERR("pa error,pa: 0x%lx, va: 0x%lx, align: 0x%lx\n",
				(unsigned long)pa, va_tmp, va_align);
		}

		if (has_page) {
			struct page *page;

			page = phys_to_page(pa);
			/*m4u_pr_err("page=0x%x, pfn=%d\n",
			 * page, __phys_to_pfn(pa));
			 */
			sg_set_page(sg, page, PAGE_SIZE, 0);
			#ifdef CONFIG_NEED_SG_DMA_LENGTH
				sg->dma_length = sg->length;
			#endif
		} else {
			sg_dma_address(sg) = pa;
			sg_dma_len(sg) = PAGE_SIZE;
		}
		sg = sg_next(sg);
	}
	*pSg = sg;
	return 0;
}


static int m4u_create_sgtable_user(
	unsigned long va_align, struct sg_table *table)
{
	int ret = 0;
	struct vm_area_struct *vma;
	struct scatterlist *sg = table->sgl;
	unsigned int left_page_num = table->nents;
	unsigned long va = va_align;

	down_read(&current->mm->mmap_sem);

	while (left_page_num) {
		unsigned int vma_page_num;

		vma = find_vma(current->mm, va);
		if (vma == NULL || vma->vm_start > va) {
			M4U_ERR("cannot find vma: va=0x%lx, vma=0x%p\n",
				va, vma);
			if (vma != NULL) {
				M4U_ERR(
					"vm_start=0x%lx, vm_end=0x%lx,vm_flag= 0x%lx\n",
					vma->vm_start, vma->vm_end,
					vma->vm_flags);
			}
			m4u_dump_mmaps(va);
			ret = -1;
			goto out;
		} else {
			/* m4u_pr_warn("%s va: 0x%lx, vma->vm_start=0x%lx,
			 * vma->vm_end=0x%lx\n",
			 *__func__, va, vma->vm_start, vma->vm_end);
			 */
		}

		vma_page_num = (vma->vm_end - va) / PAGE_SIZE;
		vma_page_num = min(vma_page_num, left_page_num);

		if ((vma->vm_flags) & VM_PFNMAP) {
			/* ion va or ioremap vma has this flag */
			/* VM_PFNMAP: Page-ranges managed
			 * without "struct page", just pure PFN
			 */
			ret = m4u_fill_sgtable_user(
			vma, va, vma_page_num, &sg, 0);
			M4U_DBG("alloc_mva VM_PFNMAP va=0x%lx, page_num=0x%x\n",
				va, vma_page_num);
		} else {
			/* Add one line comment for avoid kernel
			 * coding style, WARNING:BRACES:
			 */
			ret = m4u_fill_sgtable_user(
				vma, va, vma_page_num, &sg, 1);
			if (-1 == ret) {
				struct vm_area_struct *vma_temp;

				vma_temp = find_vma(current->mm, va_align);
				if (!vma_temp) {
					M4U_ERR("%s cannot find vma\n",
						__func__);
					return -1;
				}
				M4U_ERR(
					"%s: vm_start=0x%lx, vm_end=0x%lx,vm_flag= 0x%lx\n",
					__func__, vma_temp->vm_start,
					vma_temp->vm_end, vma_temp->vm_flags);
			}
		}
		if (ret) {
			/* Add one line comment for avoid kernel
			 * coding style, WARNING:BRACES:
			 */
			goto out;
		}

		left_page_num -= vma_page_num;
		va += vma_page_num * PAGE_SIZE;
	}

out:
	up_read(&current->mm->mmap_sem);
	return ret;
}
#endif

static struct frame_vector *m4u_get_vaddr_framevec(unsigned long va_base,
						int nr_pages)
{
	struct frame_vector *vec = NULL;
	struct mm_struct *mm = current->mm;
	int ret = 0;
	struct vm_area_struct *vma;
	int gup_flags;

	vma = find_vma(mm, va_base);
	if (!vma) {
		M4U_ERR("%s: pid %d, get mva fail, va 0x%lx(pgnum %d)\n",
			__func__, current->pid, va_base, nr_pages);
		return ERR_PTR(-EINVAL);
	}
	if ((va_base + (nr_pages << PAGE_SHIFT)) > vma->vm_end) {
		M4U_ERR("%s: pid %d, va 0x%lx(pgnum %d), vma 0x%lx~0x%lx\n",
			__func__, current->pid, va_base, nr_pages,
			vma->vm_start, vma->vm_end);
		return ERR_PTR(-EINVAL);
	}

	vec = frame_vector_create(nr_pages);
	if (!vec)
		return ERR_PTR(-ENOMEM);

	gup_flags = FOLL_TOUCH | FOLL_POPULATE | FOLL_MLOCK;
	if (vma->vm_flags & VM_LOCKONFAULT)
		gup_flags &= ~FOLL_POPULATE;
	/*
	 * We want to touch writable mappings with a write fault
	 * in order to break COW, except for shared mappings
	 * because these don't COW and we would not want to
	 * dirty them for nothing.
	 */
	if ((vma->vm_flags & (VM_WRITE | VM_SHARED)) == VM_WRITE)
		gup_flags |= FOLL_WRITE;
	/*
	 * We want mlock to succeed for regions that have any
	 * permissions other than PROT_NONE.
	 */
	if (vma->vm_flags & (VM_READ | VM_WRITE | VM_EXEC))
		gup_flags |= FOLL_FORCE;

	ret = get_vaddr_frames(va_base, nr_pages, gup_flags, vec);
	if (ret < 0)
		goto m4u_get_frmvec_dst;
	else if (ret != nr_pages) {
		ret = -EFAULT;
		goto m4u_get_frmvec_rls;
	}

	return vec;

m4u_get_frmvec_rls:
	put_vaddr_frames(vec);
m4u_get_frmvec_dst:
	frame_vector_destroy(vec);
	return ERR_PTR(ret);
}

static void m4u_put_vaddr_framevec(struct frame_vector *vec)
{
	put_vaddr_frames(vec);
	frame_vector_destroy(vec);
}

/* make a sgtable for virtual buffer */
struct sg_table *m4u_create_sgtable(unsigned long va, unsigned int size)
{
	struct sg_table *table = NULL;
	struct scatterlist *s;
	int ret, page_num, i;
	unsigned long va_align, offset;
	struct frame_vector *vec = NULL;

	page_num = M4U_GET_PAGE_NUM(va, size);
	va_align = round_down(va, PAGE_SIZE);
	offset = va & ~PAGE_MASK;

	M4U_DBG("%s va=0x%lx, PG OFF=0x%lx, VM START~END=0x%lx~0x%lx\n",
		__func__, va, PAGE_OFFSET, VMALLOC_START, VMALLOC_END);

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);

	vec = m4u_get_vaddr_framevec(va_align, page_num);
	if (IS_ERR_OR_NULL(vec)) {
		kfree(table);
		M4U_ERR(
			"%s get frmvec fail %ld: va = 0x%lx, sz = 0x%x, pgnum = %d\n",
			__func__, PTR_ERR(vec), va, size, page_num);
		return (struct sg_table *)vec;
	}
	ret = frame_vector_to_pages(vec);
	if (ret < 0) {
		M4U_ERR("%s get frmvec pg fail\n", __func__);
		goto m4u_create_sgtbl_out;
	}

	ret = sg_alloc_table_from_pages(table, frame_vector_pages(vec),
					page_num, offset, size, GFP_KERNEL);
	if (ret) {
		M4U_ERR("%s set tbl pages fail\n", __func__);
		goto m4u_create_sgtbl_out;
	}

	/*
	 * For sg_alloc_table with empty table object, it will not clear the
	 * sgtable->sgl space to zero which allocated by kmalloc.
	 * In here, we manually initialize the dma information of sgl for
	 * stable execution.
	 */
	for_each_sg(table->sgl, s, table->nents, i) {
		sg_dma_address(s) = 0;
		sg_dma_len(s) = 0;
	}

m4u_create_sgtbl_out:
	m4u_put_vaddr_framevec(vec);

	if (ret) {
		kfree(table);
		M4U_ERR("%s fail %d: va = 0x%lx, sz = 0x%x, pgnum = %d\n",
			__func__, ret, va, size, page_num);
		return ERR_PTR(ret);
	}

	return table;
}

/* the caller should make sure the mva offset have been eliminated. */
int __m4u_dealloc_mva(int eModuleID,
		      const unsigned long BufAddr,
		      const unsigned int BufSize, const unsigned int MVA,
		      struct sg_table *sg_table)
{
	struct sg_table *table = NULL;
	int kernelport = m4u_user2kernel_port(eModuleID);
	struct device *dev = m4u_get_larbdev(kernelport);
	unsigned long addr_align = MVA;
	unsigned int size_align = BufSize;
	int offset;

	if (!dev) {
		M4U_ERR("%s, %d, dev is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	M4U_DBG(
		"m4u_dealloc_mva, module = %s, addr = 0x%lx, size = 0x%x,MVA = 0x%x, mva_end = 0x%x\n",
		m4u_get_port_name(kernelport), BufAddr, BufSize, MVA,
		MVA + BufSize - 1);

	/* for ion sg alloc, we did not align the mva in allocation. */
	if (!sg_table)
		offset = m4u_va_align(&addr_align, &size_align);

	if (sg_table) {
		struct m4u_buf_info_t *m4u_buf_info;

		m4u_buf_info = m4u_client_find_buf(ion_m4u_client, addr_align,
						   1);
		if (m4u_buf_info && m4u_buf_info->mva != addr_align)
			M4U_ERR("warning: %s, %d, mva addr are not same\n",
				__func__, __LINE__);
		table = m4u_del_sgtable(addr_align);
		if (!table) {
			M4U_ERR(
				"%s-%d could not find the table from mva 0x%x\n",
				__func__, __LINE__, MVA);
			M4U_ERR(
				"m4u_dealloc_mva, module = %s, addr = 0x%lx,size = 0x%x, MVA = 0x%x, mva_end = 0x%x\n",
				m4u_get_port_name(kernelport), BufAddr,
				BufSize, MVA, MVA + BufSize - 1);
			dump_stack();
			return -EINVAL;
		}

		if (sg_page(table->sgl) != sg_page(sg_table->sgl)) {
			M4U_ERR("%s, %d, error, sg have not been added\n",
				__func__, __LINE__);
			return -EINVAL;
		}

		m4u_free_buf_info(m4u_buf_info);
	}

	if (!table)
		table = m4u_del_sgtable(addr_align);

	if (table) {
		/* Free iova and unmap pgtable items.*/
#ifdef CONFIG_ARM64
		iommu_dma_unmap_sg(dev, table->sgl, table->orig_nents, 0, 0);
#else
		__arm_coherent_iommu_unmap_sg(dev, table->sgl, table->nents, 0,
					      0);
#endif
	} else {
		M4U_ERR("could not found the sgtable and would return error\n");
		return -EINVAL;
	}


	if (BufAddr) {
		/* from user space */
		if (BufAddr < PAGE_OFFSET) {
			struct vm_area_struct *vma = NULL;

			M4UTRACE();
			if (current->mm) {
				down_read(&current->mm->mmap_sem);
				vma = find_vma(current->mm, BufAddr);
			} else if (current->active_mm) {
				down_read(&current->active_mm->mmap_sem);
				vma = NULL;
			}
			M4UTRACE();
			if (vma == NULL) {
				M4U_ERR(
					"cannot find vma: module=%s, va=0x%lx,size=0x%x\n",
					m4u_get_port_name(eModuleID), BufAddr,
					BufSize);
				if (current->mm)
					up_read(&current->mm->mmap_sem);
				else if (current->active_mm)
					up_read(&current->active_mm->mmap_sem);
				goto out;
			}
			if ((vma->vm_flags) & VM_PFNMAP) {
				if (current->mm)
					up_read(&current->mm->mmap_sem);
				else if (current->active_mm)
					up_read(&current->active_mm->mmap_sem);
				goto out;
			}
			if (current->mm)
				up_read(&current->mm->mmap_sem);
			else if (current->active_mm)
				up_read(&current->active_mm->mmap_sem);
			if (!((BufAddr >= VMALLOC_START) &&
			    (BufAddr <= VMALLOC_END)))
				if (!sg_table) {
					if (BufAddr +  BufSize < vma->vm_end)
						m4u_put_sgtable_pages(table,
								table->nents);
					else
						m4u_put_sgtable_pages(table,
							table->nents - 1);
				}
		}
	}

out:
	if (table) {
		sg_free_table(table);
		kfree(table);
	}

	M4UTRACE();
	return 0;

}

/*
 * when using this interface, the m4u alloc mva have make the va and mva with
 * the page offset, so the caller should make sure the offset and size is
 * already eliminated.
 * or the iova we got from map_sg could not be unmapped all.
 */
int m4u_dealloc_mva(int eModuleID,
		    const unsigned long BufAddr, const unsigned int BufSize,
		    const unsigned int MVA)
{
	return __m4u_dealloc_mva(eModuleID, BufAddr, BufSize, MVA, NULL);
}

void m4u_dma_cache_flush_range(void *start, size_t size)
{
#ifndef CONFIG_MTK_CACHE_FLUSH_RANGE_PARALLEL
#ifdef CONFIG_ARM64
		__dma_flush_area((void *)start, size);
#else
		dmac_flush_range((void *)start, (void *)(start + size));
#endif
#else
	mt_smp_cache_flush_m4u(start, size);
#endif
}
/*
 * the cache sync related ops are just copy from original implementation of
 * mt7623.
 */
static struct vm_struct *cache_map_vm_struct;
int m4u_cache_sync_init(void)
{
	cache_map_vm_struct = get_vm_area(PAGE_SIZE, VM_ALLOC);
	if (!cache_map_vm_struct)
		return -ENOMEM;

	return 0;
}

static void *m4u_cache_map_page_va(struct page *page)
{
	int ret;
	struct page **ppPage = &page;

	ret = map_vm_area(cache_map_vm_struct, PAGE_KERNEL, ppPage);
	if (ret) {
		M4U_ERR("error to map page\n");
		return NULL;
	}
	return cache_map_vm_struct->addr;
}

static void m4u_cache_unmap_page_va(unsigned long va)
{
	unmap_kernel_range((unsigned long)cache_map_vm_struct->addr, PAGE_SIZE);
}


static int __m4u_cache_sync_kernel(const void *start,
	size_t size, enum M4U_CACHE_SYNC_ENUM sync_type)
{
	if (sync_type == M4U_CACHE_CLEAN_BY_RANGE)
		dmac_map_area((void *)start, size, DMA_TO_DEVICE);
	else if (sync_type == M4U_CACHE_INVALID_BY_RANGE)
		dmac_unmap_area((void *)start, size, DMA_FROM_DEVICE);
	else if (sync_type == M4U_CACHE_FLUSH_BY_RANGE)
#ifndef CONFIG_MTK_CACHE_FLUSH_RANGE_PARALLEL
#ifdef CONFIG_ARM64
		__dma_flush_area((void *)start, size);
#else
		dmac_flush_range((void *)start, (void *)(start + size));
#endif


#else
		mt_smp_cache_flush_m4u(start, size);
#endif

	return 0;
}

static struct page *m4u_cache_get_page(unsigned long va)
{
	unsigned long start;
	phys_addr_t pa;
	struct page *page;

	start = va & (~M4U_PAGE_MASK);
	pa = m4u_user_v2p(start);
	if (pa == 0) {
		M4U_ERR(
			"error m4u_get_phys user_v2p return 0 on va=0x%lx\n",
			start);
		/* dump_page(page); */
		m4u_dump_mmaps(start);
		m4u_show_pte(current->mm, va);
		return NULL;
	}
	page = phys_to_page(pa);

	return page;
}

/* lock to protect cache_map_vm_struct */
static DEFINE_MUTEX(gM4u_cache_sync_user_lock);

static int __m4u_cache_sync_user(unsigned long start,
	size_t size, enum M4U_CACHE_SYNC_ENUM sync_type)
{
	unsigned long map_size, map_start, map_end;
	unsigned long end = start + size;
	/* unsigned int fragment; */
	struct page *page;
	unsigned long map_va, map_va_align;
	int ret = 0;

	mutex_lock(&gM4u_cache_sync_user_lock);

	if (!cache_map_vm_struct) {
		M4U_ERR("error: cache_map_vm_struct is NULL, retry\n");
		m4u_cache_sync_init();
	}
	if (!cache_map_vm_struct) {
		M4U_ERR(
			"error: cache_map_vm_struct is NULL, no vmalloc area\n");
		ret = -1;
		goto out;
	}

	M4U_DBG("__m4u_sync_user: start=0x%lx, size=0x%x\n", start,
		(unsigned int)size);

	map_start = start;
	while (map_start < end) {
		map_end = min((map_start & (~M4U_PAGE_MASK)) + M4U_PAGE_SIZE,
			      end);
		map_size = map_end - map_start;

		page = m4u_cache_get_page(map_start);
		if (!page) {
			ret = -1;
			goto out;
		}

		map_va = (unsigned long)m4u_cache_map_page_va(page);
		if (!map_va) {
			ret = -1;
			goto out;
		}

		map_va_align = map_va | (map_start & (M4U_PAGE_SIZE - 1));

		M4U_DBG("__m4u_sync_user:start=0x%lx, size=0x%lx,va=0x%lx\n",
		       map_start, map_size, map_va_align);
		__m4u_cache_sync_kernel((void *)map_va_align,
			map_size, sync_type);

		m4u_cache_unmap_page_va(map_va);
		map_start = map_end;
	}

out:
	mutex_unlock(&gM4u_cache_sync_user_lock);

	return ret;

}

int m4u_cache_sync_by_range(unsigned long va, unsigned int size,
			    enum M4U_CACHE_SYNC_ENUM sync_type,
			    struct sg_table *table)
{
	int ret = 0;

	if (va < PAGE_OFFSET) {	/* from user space */
		ret = __m4u_cache_sync_user(va, size, sync_type);
	} else {
		ret = __m4u_cache_sync_kernel((void *)va, size, sync_type);
	}

#ifdef CONFIG_OUTER_CACHE
	{
		struct scatterlist *sg;
		int i;

		for_each_sg(table->sgl, sg, table->nents, i) {
			unsigned int len = sg_dma_len(sg);
			phys_addr_t phys_addr = get_sg_phys(sg);

			if (sync_type == M4U_CACHE_CLEAN_BY_RANGE)
				outer_clean_range(phys_addr, phys_addr + len);
			else if (sync_type == M4U_CACHE_INVALID_BY_RANGE)
				outer_inv_range(phys_addr, phys_addr + len);
			else if (sync_type == M4U_CACHE_FLUSH_BY_RANGE)
				outer_flush_range(phys_addr, phys_addr + len);
		}
	}
#endif

	return ret;
}


/*    notes: only mva allocated by m4u_alloc_mva can use this function.*/
/*	if buffer is allocated by ion, please use ion_cache_sync*/

int m4u_cache_sync(struct m4u_client_t *client,
			int port,
		   unsigned long va, unsigned int size, unsigned int mva,
		   enum M4U_CACHE_SYNC_ENUM sync_type)
{
	int ret = 0;

	M4U_DBG(
	"cache_sync port=%s, va=0x%lx, size=0x%x, mva=0x%x, type=%d\n",
		   m4u_get_port_name(port), va, size, mva, sync_type);

	if (sync_type < M4U_CACHE_CLEAN_ALL) {
		struct m4u_buf_info_t *pMvaInfo = NULL;

		if (client)
			pMvaInfo = m4u_client_find_buf(client, mva, 0);

		/* some user may sync mva from other client
		 * (eg. ovl may not know
		 * who allocated this buffer, but he need to sync cache).
		 * we make a workaround here by query mva from mva manager
		 */
		/* if (!pMvaInfo)
		 *	pMvaInfo = mva_get_priv(mva);
		 */

		if (!pMvaInfo) {
			M4U_ERR(
				"cache sync fail, cannot find buf: mva=0x%x, client=0x%p\n",
				mva, client);
			return -1;
		}

		if ((pMvaInfo->size != size) || (pMvaInfo->va != va)) {
			M4U_ERR(
				"cache_sync fail: expect mva=0x%x,size=0x%x,va=0x%lx, but mva=0x%x,size=0x%x,va=0x%lx\n",
			       pMvaInfo->mva, pMvaInfo->size,
			       pMvaInfo->va, mva, size, va);
			return -1;
		}
		/* va size should be cache line align */
		if ((va | size) & (L1_CACHE_BYTES - 1)) {
			M4U_ERR(
				"warning: cache_sync not align: va=0x%lx,size=0x%x,align=0x%x\n",
			       va, size, L1_CACHE_BYTES);
		}

		ret = m4u_cache_sync_by_range(va, size,
			sync_type, pMvaInfo->sg_table);
	} else {
		/* All cache operation */
		if (sync_type == M4U_CACHE_CLEAN_ALL) {
			smp_inner_dcache_flush_all();
			outer_clean_all();
		} else if (sync_type == M4U_CACHE_INVALID_ALL) {
			M4U_ERR("no one can use invalid all!\n");
			return -1;
		} else if (sync_type == M4U_CACHE_FLUSH_ALL) {
			smp_inner_dcache_flush_all();
			outer_flush_all();
		}
	}
	return ret;
}

void m4u_dma_map_area(void *start, size_t size, enum M4U_DMA_DIR dir)
{
	if (dir == M4U_DMA_FROM_DEVICE)
		dmac_map_area(start, size, DMA_FROM_DEVICE);
	else if (dir == M4U_DMA_TO_DEVICE)
		dmac_map_area(start, size, DMA_TO_DEVICE);
	else if (dir == M4U_DMA_BIDIRECTIONAL)
		dmac_map_area(start, size, DMA_BIDIRECTIONAL);
}

void m4u_dma_unmap_area(void *start, size_t size, enum M4U_DMA_DIR dir)
{
	if (dir == M4U_DMA_FROM_DEVICE)
		dmac_unmap_area(start, size, DMA_FROM_DEVICE);
	else if (dir == M4U_DMA_TO_DEVICE)
		dmac_unmap_area(start, size, DMA_TO_DEVICE);
	else if (dir == M4U_DMA_BIDIRECTIONAL)
		dmac_unmap_area(start, size, DMA_BIDIRECTIONAL);
}

static long m4u_dma_op(struct m4u_client_t *client, int port,
		unsigned long va, unsigned int size, unsigned int mva,
		enum M4U_DMA_TYPE dma_type, enum M4U_DMA_DIR dma_dir)
{
	struct scatterlist *sg;
	int i, j;
	struct sg_table *table = NULL;
	int npages = 0;
	unsigned long start = -1;
	struct m4u_buf_info_t *pMvaInfo = NULL;

	if (client)
		pMvaInfo = m4u_client_find_buf(client, mva, 0);

	/* some user may sync mva from other client*/
	/*(eg. ovl may not know who allocated this buffer,*/
	/*but he need to sync cache).*/
	/*we make a workaround here by query mva from mva manager */
	/*
	 * if (!pMvaInfo)
	 *	pMvaInfo = mva_get_priv(mva);
	 */

	if (!pMvaInfo) {
		M4U_ERR(
			"m4u dma fail,cannot find buf: mva=0x%x, client=0x%p.\n",
			mva, client);
		return -1;
	}

	if ((pMvaInfo->size != size) || (pMvaInfo->va != va)) {
		M4U_ERR(
			"m4u dma fail: expect mva=0x%x,size=0x%x,va=0x%lx, but mva=0x%x,size=0x%x,va=0x%lx\n",
			pMvaInfo->mva, pMvaInfo->size,
			pMvaInfo->va, mva, size, va);
		return -1;
	}
	/* va size should be cache line align */
	if ((va|size) & (L1_CACHE_BYTES-1))
		M4U_ERR(
			"warning: cache_sync not align: va=0x%lx,size=0x%x,align=0x%x\n",
			va, size, L1_CACHE_BYTES);

	table = pMvaInfo->sg_table;
	/* npages = PAGE_ALIGN(size) / PAGE_SIZE; */
	npages = M4U_GET_PAGE_NUM(va, size);

	mutex_lock(&gM4u_cache_sync_user_lock);

	if (!cache_map_vm_struct) {
		M4U_ERR("error: cache_map_vm_struct is NULL, retry\n");
		m4u_cache_sync_init();
	}

	if (!cache_map_vm_struct) {
		M4U_ERR(
			"error: cache_map_vm_struct is NULL, no vmalloc area\n");
		mutex_unlock(&gM4u_cache_sync_user_lock);
		return -ENOMEM;
	}

	for_each_sg(table->sgl, sg, table->nents, i) {
		int npages_this_entry = PAGE_ALIGN(sg_dma_len(sg)) / PAGE_SIZE;
		struct page *page = sg_page(sg);

		if (!page) {
			phys_addr_t pa = sg_dma_address(sg);

			if (!pa) {
				M4U_ERR("%s fail, VM_PFNMAP, no page.\n",
					__func__);
				return -EFAULT;
			}
			page = phys_to_page(pa);
			if (!pfn_valid(page_to_pfn(page))) {
				M4U_ERR(
					"%s fail, VM_PFNMAP, no page, va = 0x%lx, size = 0x%x, npages = 0x%x.\n",
					__func__, va, size, npages);
				return -EFAULT;
			}
		}

		if (i >= npages)
			M4U_ERR(
				"sg table is over pages number, i=%d, npages=0x%x\n",
				i, npages);

		for (j = 0; j < npages_this_entry; j++) {
			start = (unsigned long) m4u_cache_map_page_va(page++);

			if (IS_ERR_OR_NULL((void *) start)) {
				M4U_ERR("cannot do cache sync: ret=%lu\n",
					start);
				mutex_unlock(&gM4u_cache_sync_user_lock);
				return -EFAULT;
			}

			if (dma_type == M4U_DMA_MAP_AREA)
				m4u_dma_map_area((void *)start,
					PAGE_SIZE, dma_dir);
			else if (dma_type == M4U_DMA_UNMAP_AREA)
				m4u_dma_unmap_area((void *)start,
					PAGE_SIZE, dma_dir);
			else if (dma_type == M4U_DMA_FLUSH_BY_RANGE)
				m4u_dma_cache_flush_range((void *)start,
						PAGE_SIZE);

			m4u_cache_unmap_page_va(start);
		}
	}

	mutex_unlock(&gM4u_cache_sync_user_lock);

	return 0;
}


/*
 * inherent this from original m4u driver, we use this to make sure we
 * could still support userspace ioctl commands.
 */
static long
MTK_M4U_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct M4U_MOUDLE_STRUCT m4u_module;
	struct M4U_PORT_STRUCT m4u_port;
	int PortID;
	int ModuleID;
	struct M4U_CACHE_STRUCT m4u_cache_data;
	struct M4U_DMA_STRUCT m4u_dma_data;
	struct m4u_client_t *client = filp->private_data;

	switch (cmd) {
	case MTK_M4U_T_POWER_ON:
		ret = copy_from_user(&ModuleID, (void *)arg,
				     sizeof(unsigned int));
		if (ret) {
			M4U_ERR("MTK_M4U_T_POWER_ON,copy_from_user failed,%d\n",
				ret);
			return -EFAULT;
		}
		ret = 0;
		break;

	case MTK_M4U_T_POWER_OFF:
		ret = copy_from_user(&ModuleID, (void *)arg,
				     sizeof(unsigned int));
		if (ret) {
			M4U_ERR(
				"MTK_M4U_T_POWER_OFF,copy_from_user failed,%d\n",
				ret);
			return -EFAULT;
		}
		ret = 0;
		break;

	case MTK_M4U_T_ALLOC_MVA:
		ret = copy_from_user(&m4u_module, (void *)arg,
				     sizeof(struct M4U_MOUDLE_STRUCT));
		if (ret) {
			M4U_ERR(
				"MTK_M4U_T_ALLOC_MVA,copy_from_user failed:%d\n",
				ret);
			return -EFAULT;
		}

		if (!m4u_portid_valid(m4u_module.port)) {
			M4U_ERR("m4u T_ALLOC_MVA, portid %d failed\n",
				m4u_module.port);
			return -EINVAL;
		}

		M4U_DBG("T_ALLOC_MVA, %s, %d\n", __func__, __LINE__);
		ret = pseudo_alloc_mva(client, m4u_module.port,
				m4u_module.BufAddr,
				NULL, m4u_module.BufSize, m4u_module.prot,
				m4u_module.flags, &(m4u_module.MVAStart));

		if (ret)
			return ret;

		ret = copy_to_user(
			&(((struct M4U_MOUDLE_STRUCT *) arg)->MVAStart),
			&(m4u_module.MVAStart),
			sizeof(unsigned int));
		if (ret) {
			M4U_ERR("T_ALLOC_MVA,copy_from_user failed:%d\n",
				ret);
			return -EFAULT;
		}
		break;

	case MTK_M4U_T_DEALLOC_MVA:
		{
			int offset;
			struct m4u_buf_info_t *pMvaInfo;
			unsigned long align_mva;

			ret = copy_from_user(&m4u_module, (void *)arg,
				sizeof(struct M4U_MOUDLE_STRUCT));
			if (ret) {
				M4U_ERR("T_DEALLOC_MVA,cpy failed:%d\n",
					ret);
				return -EFAULT;
			}

			M4U_DBG(
				"DEALLOC_MVA, eModuleID: %d, VABuf:0x%lx, Length : %d, MVAStart = 0x%x\n",
				m4u_module.port, m4u_module.BufAddr,
				m4u_module.BufSize, m4u_module.MVAStart);

			if (!m4u_module.BufAddr || !m4u_module.BufSize) {
				M4U_DBG(
				     "MTK_M4U_T_DEALLOC_MVA va:0x%lx, sz:0x%x",
				     m4u_module.BufAddr, m4u_module.BufSize);
				/* return -EINVAL;*/
			}

			/*
			 * we store the not aligned value in m4u client, but
			 * aligned value in pseudo_sglist, so we need to delete
			 * from client with the not-aligned value and deallocate
			 * mva with the aligned value.
			 */
			pMvaInfo = m4u_client_find_buf(client,
						       m4u_module.MVAStart, 1);

			/* to pass the code defect check */
			align_mva = m4u_module.MVAStart;
			offset = m4u_va_align(&align_mva, &m4u_module.BufSize);
			m4u_module.MVAStart = align_mva & 0xffffffff;
			/* m4u_module.MVAStart -= offset; */

			if (m4u_module.MVAStart != 0) {
				m4u_dealloc_mva(m4u_module.port,
						m4u_module.BufAddr,
						m4u_module.BufSize,
						m4u_module.MVAStart);
				m4u_free_buf_info(pMvaInfo);
			} else {
				M4U_ERR(
					"warning: dealloc a registered buffer.\n");
				M4U_ERR(
					"error to dealloc mva : id = %s, va = 0x%lx, size = %d, mva = 0x%x\n",
					m4u_get_port_name(m4u_module.port),
					m4u_module.BufAddr, m4u_module.BufSize,
					m4u_module.MVAStart);
			}

		}

		break;

	case MTK_M4U_T_DUMP_INFO:
		ret = copy_from_user(&ModuleID, (void *)arg,
				     sizeof(unsigned int));
		if (ret) {
			M4U_ERR("MTK_M4U_T_DUMP_INFO failed,%d\n", ret);
			return -EFAULT;
		}

		break;

	case MTK_M4U_T_CACHE_SYNC:
		ret = copy_from_user(&m4u_cache_data, (void *)arg,
				     sizeof(struct M4U_CACHE_STRUCT));
		if (ret) {
			M4U_ERR("m4u_cache_sync,copy_from_user failed:%d\n",
				ret);
			return -EFAULT;
		}

		if (!m4u_portid_valid(m4u_cache_data.port)) {
			M4U_ERR("m4u T_CACHE_SYNC,portid %d failed\n",
				m4u_cache_data.port);
			return -EINVAL;
		}

		ret = m4u_cache_sync(client, m4u_cache_data.port,
					m4u_cache_data.va,
					m4u_cache_data.size, m4u_cache_data.mva,
					m4u_cache_data.eCacheSync);
		break;

	case MTK_M4U_T_DMA_OP:
		ret = copy_from_user(&m4u_dma_data, (void *) arg,
				sizeof(struct M4U_DMA_STRUCT));
		if (ret) {
			M4U_ERR("dma map/unmap area,cpy failed:%d\n", ret);
			return -EFAULT;
		}

		if (!m4u_portid_valid(m4u_dma_data.port)) {
			M4U_ERR("m4u dma map/unmap area,portid %d failed\n",
				m4u_dma_data.port);
			return -EINVAL;
		}

		ret = m4u_dma_op(client, m4u_dma_data.port, m4u_dma_data.va,
				m4u_dma_data.size, m4u_dma_data.mva,
				m4u_dma_data.eDMAType, m4u_dma_data.eDMADir);
		break;

	case MTK_M4U_T_CONFIG_PORT:
		ret = copy_from_user(&m4u_port, (void *)arg,
				     sizeof(struct M4U_PORT_STRUCT));
		if (ret) {
			M4U_ERR("T_CONFIG_PORT,cpy failed:%d\n", ret);
			return -EFAULT;
		}

		ret = m4u_config_port(&m4u_port);
		break;


	case MTK_M4U_T_MONITOR_START:
		ret = copy_from_user(&PortID, (void *)arg,
				     sizeof(unsigned int));
		if (ret) {
			M4U_ERR("MONITOR_START,cpy failed,%d\n", ret);
			return -EFAULT;
		}
		ret = 0;

		break;

	case MTK_M4U_T_MONITOR_STOP:
		ret = copy_from_user(&PortID, (void *)arg,
				     sizeof(unsigned int));
		if (ret) {
			M4U_ERR("T_MONITOR_STOP,cpy failed,%d\n", ret);
			return -EFAULT;
		}
		ret = 0;
		break;

	case MTK_M4U_T_CACHE_FLUSH_ALL:
		m4u_dma_cache_flush_all();
		break;

	case MTK_M4U_T_CONFIG_PORT_ARRAY:
		{
			struct m4u_port_array port_array;

			ret = copy_from_user(&port_array, (void *)arg,
						sizeof(struct m4u_port_array));
			if (ret) {
				M4U_ERR("T_CONFIG_PORT,cpy failed:%d\n", ret);
				return -EFAULT;
			}

			ret = m4u_config_port_array(&port_array);
		}
		break;

	case MTK_M4U_T_CONFIG_MAU:
	case MTK_M4U_T_CONFIG_TF:
		break;

	default:
		M4U_ERR("MTK M4U ioctl:No such command!!\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)

static int
compat_get_m4u_module_struct(struct COMPAT_M4U_MOUDLE_STRUCT __user *data32,
			     struct M4U_MOUDLE_STRUCT __user *data)
{
	compat_uint_t u;
	compat_ulong_t l;
	int err;

	err = get_user(u, &(data32->port));
	err |= put_user(u, &(data->port));
	err |= get_user(l, &(data32->BufAddr));
	err |= put_user(l, &(data->BufAddr));
	err |= get_user(u, &(data32->BufSize));
	err |= put_user(u, &(data->BufSize));
	err |= get_user(u, &(data32->prot));
	err |= put_user(u, &(data->prot));
	err |= get_user(u, &(data32->MVAStart));
	err |= put_user(u, &(data->MVAStart));
	err |= get_user(u, &(data32->MVAEnd));
	err |= put_user(u, &(data->MVAEnd));
	err |= get_user(u, &(data32->flags));
	err |= put_user(u, &(data->flags));

	return err;
}

static int
compat_put_m4u_module_struct(struct COMPAT_M4U_MOUDLE_STRUCT __user *data32,
			     struct M4U_MOUDLE_STRUCT __user *data)
{
	compat_uint_t u;
	compat_ulong_t l;
	int err;


	err = get_user(u, &(data->port));
	err |= put_user(u, &(data32->port));
	err |= get_user(l, &(data->BufAddr));
	err |= put_user(l, &(data32->BufAddr));
	err |= get_user(u, &(data->BufSize));
	err |= put_user(u, &(data32->BufSize));
	err |= get_user(u, &(data->prot));
	err |= put_user(u, &(data32->prot));
	err |= get_user(u, &(data->MVAStart));
	err |= put_user(u, &(data32->MVAStart));
	err |= get_user(u, &(data->MVAEnd));
	err |= put_user(u, &(data32->MVAEnd));
	err |= get_user(u, &(data->flags));
	err |= put_user(u, &(data32->flags));

	return err;
}

static int
compat_get_m4u_cache_struct(struct COMPAT_M4U_CACHE_STRUCT __user *data32,
			    struct M4U_CACHE_STRUCT __user *data)
{
	compat_uint_t u;
	compat_ulong_t l;
	int err;

	err = get_user(u, &(data32->port));
	err |= put_user(u, &(data->port));
	err |= get_user(u, &(data32->eCacheSync));
	err |= put_user(u, &(data->eCacheSync));
	err |= get_user(l, &(data32->va));
	err |= put_user(l, &(data->va));
	err |= get_user(u, &(data32->size));
	err |= put_user(u, &(data->size));
	err |= get_user(u, &(data32->mva));
	err |= put_user(u, &(data->mva));

	return err;
}


long
MTK_M4U_COMPAT_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = -ENOIOCTLCMD;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_MTK_M4U_T_ALLOC_MVA:
		{
			struct COMPAT_M4U_MOUDLE_STRUCT __user *data32;
			struct M4U_MOUDLE_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_m4u_module_struct(data32, data);
			if (err)
				return err;

			ret = filp->f_op->unlocked_ioctl(filp,
							MTK_M4U_T_ALLOC_MVA,
							(unsigned long)data);

			err = compat_put_m4u_module_struct(data32, data);

			if (err)
				return err;

			break;
		}
	case COMPAT_MTK_M4U_T_DEALLOC_MVA:
		{
			struct COMPAT_M4U_MOUDLE_STRUCT __user *data32;
			struct M4U_MOUDLE_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(
					sizeof(struct M4U_MOUDLE_STRUCT));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_m4u_module_struct(data32, data);
			if (err)
				return err;

			ret = filp->f_op->unlocked_ioctl(filp,
							MTK_M4U_T_DEALLOC_MVA,
							(unsigned long)data);
			break;
		}
	case COMPAT_MTK_M4U_T_CACHE_SYNC:
		{
			struct COMPAT_M4U_CACHE_STRUCT __user *data32;
			struct M4U_CACHE_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(
					sizeof(struct M4U_CACHE_STRUCT));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_m4u_cache_struct(data32, data);
			if (err)
				return err;

			ret = filp->f_op->unlocked_ioctl(filp,
							MTK_M4U_T_CACHE_SYNC,
							(unsigned long)data);
			break;
		}
	case MTK_M4U_T_POWER_ON:
	/* fallthrough */
	case MTK_M4U_T_POWER_OFF:
	/* fallthrough */
	case MTK_M4U_T_DUMP_INFO:
	/* fallthrough */
	case MTK_M4U_T_CONFIG_PORT:
	/* fallthrough */
	case MTK_M4U_T_MONITOR_START:
	/* fallthrough */
	case MTK_M4U_T_MONITOR_STOP:
	/* fallthrough */
	case MTK_M4U_T_CACHE_FLUSH_ALL:
	/* fallthrough */
	case MTK_M4U_T_CONFIG_PORT_ARRAY:
		ret = filp->f_op->unlocked_ioctl(filp, cmd,
				(unsigned long)compat_ptr(arg));
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

#else

#define MTK_M4U_COMPAT_ioctl  NULL

#endif


/***********************************************************/
/** map mva buffer to kernel va buffer
 *   this function should ONLY used for DEBUG
 ************************************************************/
int m4u_mva_map_kernel(unsigned int mva,
	unsigned long size, unsigned long *map_va,
	unsigned int *map_size)
{
	struct m4u_buf_info_t *pMvaInfo;
	struct sg_table *table;
	struct scatterlist *sg;
	int i, j, k, ret = 0;
	struct page **pages;
	unsigned int page_num;
	void *kernel_va;
	unsigned int kernel_size;

	pMvaInfo = m4u_client_find_buf(ion_m4u_client, mva, 0);

	if (!pMvaInfo || pMvaInfo->size < size) {
		M4U_ERR("%s cannot find mva: mva=0x%x, size=0x%lx\n",
			__func__, mva, size);
		if (pMvaInfo)
			M4U_ERR("pMvaInfo: mva=0x%x, size=0x%x\n",
				pMvaInfo->mva, pMvaInfo->size);
		return -1;
	}

	table = pMvaInfo->sg_table;

	page_num = M4U_GET_PAGE_NUM(mva, size);
	pages = vmalloc(sizeof(struct page *) * page_num);
	if (pages == NULL) {
		M4U_ERR("mva_map_kernel:error to vmalloc for %d\n",
			(unsigned int)sizeof(struct page *) * page_num);
		return -1;
	}

	k = 0;
	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page_start;
		int pages_in_this_sg = PAGE_ALIGN(sg_dma_len(sg)) / PAGE_SIZE;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
		if (sg_dma_address(sg) == 0)
			pages_in_this_sg = PAGE_ALIGN(sg->length) / PAGE_SIZE;
#endif
		page_start = sg_page(sg);
		for (j = 0; j < pages_in_this_sg; j++) {
			pages[k++] = page_start++;
			if (k >= page_num)
				goto get_pages_done;
		}
	}

get_pages_done:
	if (k < page_num) {
		/* this should not happen, because we have
		 * checked the size before.
		 */
		M4U_ERR(
			"mva_map_kernel:only get %d pages: mva=0x%x, size=0x%lx, pg_num=%d\n",
			k, mva, size, page_num);
		ret = -1;
		goto error_out;
	}

	kernel_va = 0;
	kernel_size = 0;
	kernel_va = vmap(pages, page_num, VM_MAP, PAGE_KERNEL);
	if (kernel_va == 0 || (unsigned long)kernel_va & M4U_PAGE_MASK) {
		M4U_ERR(
			"mva_map_kernel:vmap fail: page_num=%d, kernel_va=0x%p\n",
			page_num, kernel_va);
		ret = -2;
		goto error_out;
	}

	kernel_va += ((unsigned long)mva & (M4U_PAGE_MASK));

	*map_va = (unsigned long)kernel_va;
	*map_size = (unsigned int)size;

error_out:
	vfree(pages);
	M4U_DBG(
		"mva_map_kernel:mva=0x%x,size=0x%lx,map_va=0x%lx,map_size=0x%x\n",
		mva, size, *map_va, *map_size);

	return ret;
}
EXPORT_SYMBOL(m4u_mva_map_kernel);

int m4u_mva_unmap_kernel(unsigned int mva,
		unsigned long size, unsigned long map_va)
{
	M4U_DBG("mva_unmap_kernel:mva=0x%x,size=0x%lx,va=0x%lx\n",
		mva, size, map_va);
	vunmap((void *)(map_va & (~M4U_PAGE_MASK)));
	return 0;
}
EXPORT_SYMBOL(m4u_mva_unmap_kernel);


#ifndef CONFIG_ARM64
/* A64 Direct mapping is helped via iommu framework. */
static struct dma_iommu_mapping *dmapping;
static dma_addr_t __alloc_iova(struct dma_iommu_mapping *mapping,
				      size_t size);
static void __free_iova(struct dma_iommu_mapping *mapping,
			       dma_addr_t addr, size_t size);

static void dma_cache_maint_page(struct page *page, unsigned long offset,
	size_t size, enum dma_data_direction dir,
	void (*op)(const void *, size_t, int))
{
	unsigned long pfn;
	size_t left = size;

	pfn = page_to_pfn(page) + offset / PAGE_SIZE;
	offset %= PAGE_SIZE;

	/*
	 * A single sg entry may refer to multiple physically contiguous
	 * pages.  But we still need to process highmem pages individually.
	 * If highmem is not configured then the bulk of this loop gets
	 * optimized out.
	 */
	do {
		size_t len = left;
		void *vaddr;

		page = pfn_to_page(pfn);

		if (PageHighMem(page)) {
			if (len + offset > PAGE_SIZE)
				len = PAGE_SIZE - offset;

			if (cache_is_vipt_nonaliasing()) {
				vaddr = kmap_atomic(page);
				op(vaddr + offset, len, dir);
				kunmap_atomic(vaddr);
			} else {
				vaddr = kmap_high_get(page);
				if (vaddr) {
					op(vaddr + offset, len, dir);
					kunmap_high(page);
				}
			}
		} else {
			vaddr = page_address(page) + offset;
			op(vaddr, len, dir);
		}
		offset = 0;
		pfn++;
		left -= len;
	} while (left);
}

static int __dma_direction_to_prot(enum dma_data_direction dir)
{
	int prot;

	switch (dir) {
	case DMA_BIDIRECTIONAL:
		prot = IOMMU_READ | IOMMU_WRITE;
		break;
	case DMA_TO_DEVICE:
		prot = IOMMU_READ;
		break;
	case DMA_FROM_DEVICE:
		prot = IOMMU_WRITE;
		break;
	default:
		prot = 0;
	}

	return prot;
}

/*
 * Make an area consistent for devices.
 * Note: Drivers should NOT use this function directly, as it will break
 * platforms with CONFIG_DMABOUNCE.
 * Use the driver DMA support - see dma-mapping.h (dma_sync_*)
 */
static void __dma_page_cpu_to_dev(struct page *page, unsigned long off,
	size_t size, enum dma_data_direction dir)
{
	phys_addr_t paddr;

	dma_cache_maint_page(page, off, size, dir, dmac_map_area);

	paddr = page_to_phys(page) + off;
	if (dir == DMA_FROM_DEVICE)
		outer_inv_range(paddr, paddr + size);
	else
		outer_clean_range(paddr, paddr + size);
	/* FIXME: non-speculating: flush on bidirectional mappings? */
}

static void __dma_page_dev_to_cpu(struct page *page, unsigned long off,
	size_t size, enum dma_data_direction dir)
{
	phys_addr_t paddr = page_to_phys(page) + off;

	/* FIXME: non-speculating: not required */
	/* in any case, don't bother invalidating if DMA to device */
	if (dir != DMA_TO_DEVICE) {
		outer_inv_range(paddr, paddr + size);

		dma_cache_maint_page(page, off, size, dir, dmac_unmap_area);
	}

	/*
	 * Mark the D-cache clean for these pages to avoid extra flushing.
	 */
	if (dir != DMA_TO_DEVICE && size >= PAGE_SIZE) {
		unsigned long pfn;
		size_t left = size;

		pfn = page_to_pfn(page) + off / PAGE_SIZE;
		off %= PAGE_SIZE;
		if (off) {
			pfn++;
			left -= PAGE_SIZE - off;
		}
		while (left >= PAGE_SIZE) {
			page = pfn_to_page(pfn++);
			set_bit(PG_dcache_clean, &page->flags);
			left -= PAGE_SIZE;
		}
	}
}

static int
__iommu_remove_mapping(struct device *dev, dma_addr_t iova, size_t size)
{
	struct dma_iommu_mapping *mapping = to_dma_iommu_mapping(dev);

	/*
	 * add optional in-page offset from iova to size and align
	 * result to page size
	 */
	size = PAGE_ALIGN((iova & ~PAGE_MASK) + size);
	iova &= PAGE_MASK;

	iommu_unmap(mapping->domain, iova, size);
	__free_iova(mapping, iova, size);
	return 0;
}

/*
 * Map a part of the scatter-gather list into contiguous io address space
 */
static int __map_sg_chunk(struct device *dev, struct scatterlist *sg,
			  size_t size, dma_addr_t *handle,
			  enum dma_data_direction dir, unsigned long attrs,
			  bool is_coherent)
{
	struct dma_iommu_mapping *mapping = to_dma_iommu_mapping(dev);
	dma_addr_t iova, iova_base;
	int ret = 0;
	unsigned int count;
	struct scatterlist *s;
	int prot;

	size = PAGE_ALIGN(size);

	iova_base = iova = __alloc_iova(mapping, size);
	if (iova == ARM_MAPPING_ERROR)
		return -ENOMEM;

	for (count = 0, s = sg; count < (size >> PAGE_SHIFT); s = sg_next(s)) {

		phys_addr_t phys;
		unsigned int len;

		/* for some pa do not have struct pages, we get the pa from
		 * sg_dma_address.
		 * we have set the iova to ARM_MAPPING_ERROR in __iommu_map_sg
		 * for which have pages
		 */
		if (!sg_dma_address(s) || sg_dma_address(s) == ARM_MAPPING_ERROR
			|| !sg_dma_len(s)) {
			phys = page_to_phys(sg_page(s));
			len = PAGE_ALIGN(s->offset + s->length);
		} else {
			phys = sg_dma_address(s);
			len = sg_dma_len(s);
			/* clear the dma address after we get the pa. */
			sg_dma_address(s) = ARM_MAPPING_ERROR;
			sg_dma_len(s) = 0;
		}
		if (!is_coherent &&
			(attrs & DMA_ATTR_SKIP_CPU_SYNC) == 0
			&& (sg_page(s)))
			__dma_page_cpu_to_dev(sg_page(s), s->offset,
					      s->length, dir);

		prot = __dma_direction_to_prot(dir);

		ret = iommu_map(mapping->domain, iova, phys, len, prot);
		if (ret < 0)
			goto fail;
		count += len >> PAGE_SHIFT;
		iova += len;
	}
	*handle = iova_base;

	return 0;
fail:
	*handle = ARM_MAPPING_ERROR;
	iommu_unmap(mapping->domain, iova_base, count * PAGE_SIZE);
	__free_iova(mapping, iova_base, size);
	return ret;
}

static int __iommu_map_sg(struct device *dev, struct scatterlist *sg, int nents,
		     enum dma_data_direction dir, unsigned long attrs,
		     bool is_coherent)
{
	struct scatterlist *s = sg, *dma = sg, *start = sg;
	int i, count = 0;
	unsigned int offset = s->offset;
	unsigned int size = s->offset + s->length;
	unsigned int max = dma_get_max_seg_size(dev);

	for (i = 1; i < nents; i++) {
		s = sg_next(s);
		/*
		 * this is for pseudo m4u driver user, since some user space
		 * memory do not have struct pages, and we need to store the pa
		 * in sg->dma_address, this is to avoid the dma to modify this
		 * value.
		 */
		if (!sg_dma_address(s) || !sg_dma_len(s)) {
			sg_dma_address(s) = ARM_MAPPING_ERROR;
			sg_dma_len(s) = 0;
		}
		if (s->offset || (size & ~PAGE_MASK) ||
		    size + s->length > max) {
			if (__map_sg_chunk(dev, start, size, &dma->dma_address,
			    dir, attrs, is_coherent) < 0)
				goto bad_mapping;

			dma->dma_address += offset;
			dma->dma_length = size - offset;

			size = offset = s->offset;
			start = s;
			dma = sg_next(dma);
			count += 1;
		}
		if ((sg_dma_address(s)) &&
		     (sg_dma_address(s) != ARM_MAPPING_ERROR) &&
		     (sg_dma_len(s)))
			size += sg_dma_len(s);
		else
			size += s->length;
	}

	/* map sg chunk would leave the last page if address is page aligned */
	if ((sg_dma_address(s)) &&
	    (sg_dma_address(s) != ARM_MAPPING_ERROR) && (sg_dma_len(s))) {
		size += PAGE_SIZE;
		/*
		 * Add on plus page size to make sure th map and unmap would
		 * reach the end
		 */
		/*sg_dma_len(s) += PAGE_SIZE;*/
	}
	if (__map_sg_chunk(dev, start, size, &dma->dma_address, dir, attrs,
		is_coherent) < 0)
		goto bad_mapping;

	dma->dma_address += offset;
	dma->dma_length = size - offset;

	return count+1;

bad_mapping:
	for_each_sg(sg, s, count, i)
		__iommu_remove_mapping(dev, sg_dma_address(s), sg_dma_len(s));
	/* tell the pseudo driver that the map have been failed. */
	if (sg_dma_address(sg) && sg_dma_len(sg)) {
		sg_dma_address(sg) = ARM_MAPPING_ERROR;
		sg_dma_len(sg) = 0;
	}
	return 0;
}

/**
 * arm_coherent_iommu_map_sg - map a set of SG buffers for streaming mode DMA
 * @dev: valid struct device pointer
 * @sg: list of buffers
 * @nents: number of buffers to map
 * @dir: DMA transfer direction
 *
 * Map a set of i/o coherent buffers described by scatterlist in streaming
 * mode for DMA. The scatter gather list elements are merged together (if
 * possible) and tagged with the appropriate dma address and length. They are
 * obtained via sg_dma_{address,length}.
 */
int __arm_coherent_iommu_map_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir, unsigned long attrs)
{
	return __iommu_map_sg(dev, sg, nents, dir, attrs, true);
}

static void __iommu_unmap_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir, unsigned long attrs,
		bool is_coherent)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i) {
		if (sg_dma_len(s))
			__iommu_remove_mapping(dev, sg_dma_address(s),
					       sg_dma_len(s));
		if (!is_coherent &&
			(attrs & DMA_ATTR_SKIP_CPU_SYNC) == 0 && (sg_page(s)))
			__dma_page_dev_to_cpu(sg_page(s), s->offset,
					      s->length, dir);
	}
}

/**
 * arm_coherent_iommu_unmap_sg - unmap a set of SG buffers mapped by dma_map_sg
 * @dev: valid struct device pointer
 * @sg: list of buffers
 * @nents: number of buffers to unmap (same as was passed to dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 *
 * Unmap a set of streaming mode DMA translations.  Again, CPU access
 * rules concerning calls here are the same as for dma_unmap_single().
 */
void __arm_coherent_iommu_unmap_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir, unsigned long attrs)
{
	__iommu_unmap_sg(dev, sg, nents, dir, attrs, true);
}

static int __extend_iommu_mapping(struct dma_iommu_mapping *mapping)
{
	int next_bitmap;

	if (mapping->nr_bitmaps >= mapping->extensions)
		return -EINVAL;

	next_bitmap = mapping->nr_bitmaps;
	mapping->bitmaps[next_bitmap] = kzalloc(mapping->bitmap_size,
						GFP_ATOMIC);
	if (!mapping->bitmaps[next_bitmap])
		return -ENOMEM;

	mapping->nr_bitmaps++;

	return 0;
}

static dma_addr_t __alloc_iova(struct dma_iommu_mapping *mapping,
				      size_t size)
{
	unsigned int order = get_order(size);
	unsigned int align = 0;
	unsigned int count, start;
	size_t mapping_size = mapping->bits << PAGE_SHIFT;
	unsigned long flags;
	dma_addr_t iova;
	int i;

	if (order > CONFIG_ARM_DMA_IOMMU_ALIGNMENT)
		order = CONFIG_ARM_DMA_IOMMU_ALIGNMENT;

	count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	align = (1 << order) - 1;

	spin_lock_irqsave(&mapping->lock, flags);
	for (i = 0; i < mapping->nr_bitmaps; i++) {
		start = bitmap_find_next_zero_area(mapping->bitmaps[i],
				mapping->bits, 0, count, align);

		if (start > mapping->bits)
			continue;

		bitmap_set(mapping->bitmaps[i], start, count);
		break;
	}

	/*
	 * No unused range found. Try to extend the existing mapping
	 * and perform a second attempt to reserve an IO virtual
	 * address range of size bytes.
	 */
	if (i == mapping->nr_bitmaps) {
		if (__extend_iommu_mapping(mapping)) {
			spin_unlock_irqrestore(&mapping->lock, flags);
			return ARM_MAPPING_ERROR;
		}

		start = bitmap_find_next_zero_area(mapping->bitmaps[i],
				mapping->bits, 0, count, align);

		if (start > mapping->bits) {
			spin_unlock_irqrestore(&mapping->lock, flags);
			return ARM_MAPPING_ERROR;
		}

		bitmap_set(mapping->bitmaps[i], start, count);
	}
	spin_unlock_irqrestore(&mapping->lock, flags);

	iova = mapping->base + (mapping_size * i);
	iova += start << PAGE_SHIFT;

	return iova;
}

static void __free_iova(struct dma_iommu_mapping *mapping,
			       dma_addr_t addr, size_t size)
{
	unsigned int start, count;
	size_t mapping_size = mapping->bits << PAGE_SHIFT;
	unsigned long flags;
	dma_addr_t bitmap_base;
	u32 bitmap_index;

	if (!size)
		return;

	bitmap_index = (u32) (addr - mapping->base) / (u32) mapping_size;
	WARN_ON(addr < mapping->base || bitmap_index > mapping->extensions);

	bitmap_base = mapping->base + mapping_size * bitmap_index;

	start = (addr - bitmap_base) >>	PAGE_SHIFT;

	if (addr + size > bitmap_base + mapping_size) {
		/*
		 * The address range to be freed reaches into the iova
		 * range of the next bitmap. This should not happen as
		 * we don't allow this in __alloc_iova (at the
		 * moment).
		 */
		WARN_ON(1);
		return;
	}

	count = size >> PAGE_SHIFT;

	spin_lock_irqsave(&mapping->lock, flags);
	bitmap_clear(mapping->bitmaps[bitmap_index], start, count);
	spin_unlock_irqrestore(&mapping->lock, flags);
}
#endif


#ifdef M4U_TEE_SERVICE_ENABLE
unsigned int mtk_init_tz_m4u(void)
{
	/* init the sec_mem_size to 400M to avoid build error. */
	static unsigned int sec_mem_size = 400 * 0x100000;
	static unsigned int tz_m4u_inited;
	/*reserve mva range for sec */
	int gM4U_L2_enable = 1;

	if (tz_m4u_inited)
		return sec_mem_size;

	pseudo_m4u_session_init();
	pseudo_m4u_sec_init(0, gM4U_L2_enable, &sec_mem_size);

	tz_m4u_inited = 1;

	return sec_mem_size;
}
#endif

static const struct file_operations g_stMTK_M4U_fops = {
	.owner = THIS_MODULE,
	.open = MTK_M4U_open,
	.release = MTK_M4U_release,
	.flush = MTK_M4U_flush,
	.unlocked_ioctl = MTK_M4U_ioctl,
	.compat_ioctl = MTK_M4U_COMPAT_ioctl,
};

/*
 * Here's something need to be done in probe, we should get the following
 * information from dts
 * 1. get the reserved memory range for fb buffer and sec trustzone mva range
 * 2. get the larb port device for attach device in order to config m4u port
 */

/*
 * iommus = <&iommu portid>
 */
static int pseudo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
#ifndef CONFIG_ARM64
	struct device_node *node = dev->of_node;
	struct platform_device *pimudev;

	node = of_parse_phandle(pdev->dev.of_node, "iommus", 0);
	if (!node)
		return 0;

	pimudev = of_find_device_by_node(node);
	of_node_put(node);
	if (WARN_ON(!pimudev))
		return -EINVAL;

	dmapping = pimudev->dev.archdata.iommu;
	WARN_ON(!dmapping);
#endif
	{
		struct device_dma_parameters *dma_param;

		/* dma will split the iova into max size to 65535 byte by
		 * default if we do not set this.
		 */
		dma_param = devm_kzalloc(dev, sizeof(*dma_param), GFP_KERNEL);
		if (!dma_param)
			return -ENOMEM;

		/* set the iova to 256MB for one time map, this should be
		 * suffice for ION
		 */
		dma_param->max_segment_size = 0x10000000;
		dev = &pdev->dev;
		dev->dma_parms = dma_param;
	}


	gM4uDev = kzalloc(sizeof(struct m4u_device), GFP_KERNEL);
	if (!gM4uDev)
		return -ENOMEM;

	gM4uDev->m4u_dev_proc_entry = proc_create("m4u", 0444, NULL,
						  &g_stMTK_M4U_fops);
	if (!gM4uDev->m4u_dev_proc_entry) {
		M4U_ERR("proc m4u create error\n");
		return -ENODEV;
	}

	m4u_cache_sync_init();

	pseudo_m4u_dev = &pdev->dev;

	return 0;
}

static int pseudo_remove(struct platform_device *pdev)
{
	return 0;
}

static int pseudo_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	#ifdef M4U_TEE_SERVICE_ENABLE
	smi_reg_backup_sec();
	#endif
	return 0;
}

static int pseudo_resume(struct platform_device *pdev)
{
	#ifdef M4U_TEE_SERVICE_ENABLE
	smi_reg_restore_sec();
	#endif
	return 0;
}


#ifdef CONFIG_COMPAT
struct MTK_SMI_COMPAT_BWC_CONFIG {
	compat_int_t scenario;
	compat_int_t b_on_off;
};

#define COMPAT_MTK_IOC_SMI_BWC_CONFIG  \
	MTK_IOW(24, struct MTK_SMI_COMPAT_BWC_CONFIG)

static int
compat_get_smi_bwc_config_struct(
		struct MTK_SMI_COMPAT_BWC_CONFIG __user *data32,
		struct MTK_SMI_BWC_CONFIG __user *data)
{
	compat_int_t i;
	int err;

	err = get_user(i, &(data32->scenario));
	err |= put_user(i, &(data->scenario));
	err |= get_user(i, &(data32->b_on_off));
	err |= put_user(i, &(data->b_on_off));

	return err;
}

static long
MTK_SMI_COMPAT_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_MTK_IOC_SMI_BWC_CONFIG:
		{
			if (COMPAT_MTK_IOC_SMI_BWC_CONFIG ==
			    MTK_IOC_SMI_BWC_CONFIG) {
				return filp->f_op->unlocked_ioctl(
						filp, cmd,
						(unsigned long)compat_ptr(arg));
			} else {

				struct MTK_SMI_COMPAT_BWC_CONFIG __user *data32;
				struct MTK_SMI_BWC_CONFIG __user *data;
				int err;

				data32 = compat_ptr(arg);
				data = compat_alloc_user_space(
					sizeof(struct MTK_SMI_BWC_CONFIG));

				if (data == NULL)
					return -EFAULT;

				err = compat_get_smi_bwc_config_struct(data32,
									data);
				if (err)
					return err;

				ret = filp->f_op->unlocked_ioctl(filp,
							MTK_IOC_SMI_BWC_CONFIG,
							(unsigned long)data);
				return ret;
			}
		}
		break;

	case MTK_IOC_SMI_DUMP_LARB:
	case MTK_IOC_SMI_DUMP_COMMON:
	case MTK_IOC_MMDVFS_CMD:
		return filp->f_op->unlocked_ioctl(filp, cmd,
						(unsigned long)compat_ptr(arg));

	default:
		return -ENOIOCTLCMD;
	}

	return ret;
}
#else
#define MTK_SMI_COMPAT_ioctl  NULL
#endif

static int
smi_bwc_config(struct MTK_SMI_BWC_CONFIG *p_conf, unsigned int *pu4LocalCnt)
{
	int scenario = p_conf->scenario;

	/* Bandwidth Limiter */
	switch (scenario) {
	case SMI_BWC_SCEN_VP:
	case SMI_BWC_SCEN_VP_4KOSD:
	case SMI_BWC_SCEN_SWDEC_VP: {
		bool osd_4k = !!(scenario == SMI_BWC_SCEN_VP_4KOSD);

		if (p_conf->b_on_off)
			mtk_smi_vp_setting(osd_4k);
		else
			mtk_smi_init_setting();
	} break;

	case SMI_BWC_SCEN_VR:
	case SMI_BWC_SCEN_VR_SLOW:
	case SMI_BWC_SCEN_VENC:

	case SMI_BWC_SCEN_NORMAL:
		mtk_smi_init_setting();
		break;

	case SMI_BWC_SCEN_MM_GPU:
		break;
	default:
		mtk_smi_init_setting();
		break;
	}

	return 0;
}

static int smi_open(struct inode *inode, struct file *file)
{
	file->private_data = kmalloc_array(SMI_BWC_SCEN_CNT,
					   sizeof(unsigned int), GFP_ATOMIC);

	if (file->private_data == NULL)
		return -ENOMEM;

	memset(file->private_data, 0, SMI_BWC_SCEN_CNT * sizeof(unsigned int));

	return 0;
}

static int smi_release(struct inode *inode, struct file *file)
{
	if (file->private_data != NULL) {
		kfree(file->private_data);
		file->private_data = NULL;
	}

	return 0;
}

static long smi_ioctl(struct file *pFile, unsigned int cmd, unsigned long param)
{
	int ret = 0;

	switch (cmd) {
	case MTK_IOC_SMI_BWC_CONFIG:
		{
			struct MTK_SMI_BWC_CONFIG cfg;

			ret = copy_from_user(&cfg, (void *)param,
					     sizeof(struct MTK_SMI_BWC_CONFIG));
			if (ret) {
				pr_err("SMI_BWC_CONFIG, cpy failed: %d\n", ret);
				return -EFAULT;
			}

			ret = smi_bwc_config(&cfg, NULL);
		}
		break;

	case MTK_IOC_SMI_BWC_INFO_SET:
	case MTK_IOC_SMI_BWC_INFO_GET:
	case MTK_IOC_SMI_DUMP_LARB:
	case MTK_IOC_SMI_DUMP_COMMON:
		pr_debug("smi ioctl: those command does not support anymore\n");
		break;

	default:
		return -1;
	}

	return ret;
}

static const struct file_operations smi_fops = {
	.owner = THIS_MODULE,
	.open = smi_open,
	.release = smi_release,
	.unlocked_ioctl = smi_ioctl,
	.compat_ioctl = MTK_SMI_COMPAT_ioctl
};

static dev_t smi_dev_no = MKDEV(MTK_SMI_MAJOR_NUMBER, 0);
static inline int smi_register(void)
{
	struct cdev *psmidev;

	if (alloc_chrdev_region(&smi_dev_no, 0, 1, "MTK_SMI")) {
		pr_err("Allocate device No. failed");
		return -EAGAIN;
	}
	/* Allocate driver */
	psmidev = cdev_alloc();

	if (psmidev == NULL) {
		unregister_chrdev_region(smi_dev_no, 1);
		pr_err("Allocate mem for kobject failed");
		return -ENOMEM;
	}
	/* Attatch file operation. */
	cdev_init(psmidev, &smi_fops);
	psmidev->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(psmidev, smi_dev_no, 1)) {
		pr_err("Attatch file operation failed");
		unregister_chrdev_region(smi_dev_no, 1);
		return -EAGAIN;
	}

	return 0;
}

static struct class *psmi_class;
static int smi_dev_register(void)
{
	int ret;
	struct device *smidevice = NULL;

	if (smi_register()) {
		pr_err("register SMI failed\n");
		return -EAGAIN;
	}

	psmi_class = class_create(THIS_MODULE, "MTK_SMI");
	if (IS_ERR(psmi_class)) {
		ret = PTR_ERR(psmi_class);
		pr_err("Unable to create class, err = %d", ret);
		return ret;
	}

	smidevice = device_create(psmi_class, NULL, smi_dev_no,
				  NULL, "MTK_SMI");

	return 0;
}

static int __init smi_init(void)
{
	return smi_dev_register();
}

static struct platform_driver pseudo_driver = {
	.probe = pseudo_probe,
	.remove = pseudo_remove,
	.suspend = pseudo_suspend,
	.resume = pseudo_resume,
	.driver = {
		.name = "pseudo-m4u",
		.of_match_table = mtk_pseudo_of_ids,
		.owner = THIS_MODULE,
	}
};

static int __init mtk_pseudo_init(void)
{
	if (platform_driver_register(&pseudo_driver)) {
		M4U_ERR("failed to register pseudo driver");
		return -ENODEV;
	}

	if (smi_init()) {
		M4U_ERR("smi bwc init failed\n");
		return -EINVAL;
	}

	return 0;
}

static void __exit mtk_pseudo_exit(void)
{
	platform_driver_unregister(&pseudo_driver);
}

module_init(mtk_pseudo_init);
module_exit(mtk_pseudo_exit);
