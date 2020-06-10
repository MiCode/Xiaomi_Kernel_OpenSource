// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/list.h>

#include "apusys_drv.h"
#include "mdw_cmn.h"
#include "mdw_mem.h"
#include "mdw_mem_cmn.h"

// TODO: get VLM config from DTS
#define APUSYS_VLM_START 0x1D800000
#define APUSYS_VLM_SIZE 0x100000

struct mdw_mem_mgr {
	struct list_head list;
	struct mutex mtx;

	struct mdw_mem_ops *dops; //dram mem ops
};

static struct mdw_mem_mgr m_mgr;

//----------------------------------------------
void mdw_mem_u2k(struct apusys_mem *umem, struct apusys_kmem *kmem)
{
	kmem->khandle = umem->khandle;
	kmem->uva = umem->uva;
	kmem->iova = umem->iova;
	kmem->size = umem->size;
	kmem->iova_size = umem->iova_size;
	kmem->align = umem->align;
	kmem->cache = umem->cache;
	kmem->mem_type = umem->mem_type;
	kmem->fd = umem->fd;
	kmem->kva = 0;
}

void mdw_mem_k2u(struct apusys_kmem *kmem, struct apusys_mem *umem)
{
	umem->iova = kmem->iova;
	umem->size = kmem->size;
	umem->iova_size = kmem->iova_size;
	umem->khandle = kmem->khandle;
}

static void mdw_mem_list_add(struct mdw_mem *mmem)
{
	mutex_lock(&m_mgr.mtx);
	list_add_tail(&mmem->m_item, &m_mgr.list);
	mutex_unlock(&m_mgr.mtx);
}

static void mdw_mem_list_del(struct mdw_mem *mmem)
{
	mutex_lock(&m_mgr.mtx);
	list_del(&mmem->m_item);
	mutex_unlock(&m_mgr.mtx);
}

/**
 * Allocate memory with mapping to kva and iova.<br>
 * <b>Inputs</b><br>
 *   mem->size: memory size.<br>
 *   mem->align: memory alignment, 4KB by default.<br>
 * <b>Outputs</b><br>
 *   mem->kva: mapped kernel virtual address.<br>
 *   mem->iova: mapped iova address.<br>
 *   mem->khandle: [ION] kernel handle.<br>
 *   mem->attach: [AOSP] DMA buffer attachment.<br>
 *   mem->sgt: [AOSP] scatter list table.<br>
 * @param[in,out] mem The apusys memory
 * @return 0: Success.<br>
 *   ENODEV: memory operations doesn't exist.<br>
 *   ENOMEM: out of memory.<br>
 * @remark mem->property is set to APUSYS_MEM_PROP_ALLOC
 */
int mdw_mem_alloc(struct mdw_mem *m)
{
	int ret = 0;

	if (!m_mgr.dops || !m_mgr.dops->alloc)
		return -ENODEV;

	ret = m_mgr.dops->alloc(&m->kmem);
	if (ret)
		return ret;

	m->kmem.property = APUSYS_MEM_PROP_ALLOC;
	mdw_mem_list_add(m);

	return ret;
}

/**
 * Free memory with mapped kva or iova.<br>
 * The counter function to apusys_mem_alloc().<br>
 * @param[in] mem The allocated apusys memory
 * @return 0: Success.<br>
 *   ENODEV: memory operations doesn't exist.<br>
 */
int mdw_mem_free(struct mdw_mem *m)
{
	int ret = 0;

	if (!m_mgr.dops || !m_mgr.dops->free)
		return -ENODEV;

	ret = m_mgr.dops->free(&m->kmem);
	mdw_mem_list_del(m);

	return ret;
}

/**
 * Import memory from given fd, and map to kva, iova.<br>
 * <b>Inputs</b><br>
 *   mem->fd: file descriptor shared from ION or DMA buffer.<br>
 * <b>Outputs</b><br>
 *   mem->kva: mapped kernel virtual address.<br>
 *   mem->iova: mapped iova address.<br>
 *   mem->khandle: [ION] kernel handle.<br>
 *   mem->attach: [AOSP] DMA buffer attachment.<br>
 *   mem->sgt: [AOSP] scatter list table.<br>
 * @param[in,out] mem The apusys memory
 * @return 0: Success.<br>
 *   ENODEV: memory operations doesn't exist.<br>
 *   ENOMEM: out of memory.<br>
 * @remark mem->property is set to APUSYS_MEM_PROP_IMPORT.<br>
 */
int mdw_mem_import(struct mdw_mem *m)
{
	int ret = 0;

	if (!m_mgr.dops || !m_mgr.dops->map_iova)
		return -ENODEV;

	ret = m_mgr.dops->map_iova(&m->kmem);
	if (ret)
		return ret;

	m->kmem.property = APUSYS_MEM_PROP_IMPORT;
	mdw_mem_list_add(m);

	return ret;
}

/**
 * Unimport memory with mapped kva, iova.<br>
 * The counter function to apusys_mem_import().<br>
 * @param[in] mem The allocated apusys memory
 * @return 0: Success.<br>
 *   ENODEV: memory operations doesn't exist.<br>
 */
int mdw_mem_unimport(struct mdw_mem *m)
{
	int ret = 0;

	if (!m_mgr.dops || !m_mgr.dops->unmap_iova)
		return -ENODEV;

	ret = m_mgr.dops->unmap_iova(&m->kmem);
	mdw_mem_list_del(m);

	return ret;
}

int mdw_mem_map(struct mdw_mem *m)
{
	int ret = 0;

	if (!m_mgr.dops || !m_mgr.dops->map_kva || !m_mgr.dops->map_iova)
		return -ENODEV;

	ret = m_mgr.dops->map_kva(&m->kmem);
	if (ret)
		goto fail_map_kva;

	ret = m_mgr.dops->map_iova(&m->kmem);
	if (ret)
		goto fail_map_iova;

	m->kmem.property = APUSYS_MEM_PROP_MAP;
	mdw_mem_list_add(m);

	return 0;

fail_map_iova:
	m_mgr.dops->unmap_kva(&m->kmem);
fail_map_kva:
	return ret;
}

int mdw_mem_unmap(struct mdw_mem *m)
{
	if (!m_mgr.dops || !m_mgr.dops->unmap_iova || !m_mgr.dops->unmap_kva)
		return -ENODEV;

	m_mgr.dops->unmap_iova(&m->kmem);
	m_mgr.dops->unmap_kva(&m->kmem);
	mdw_mem_list_del(m);

	return 0;
}

int mdw_mem_flush(struct apusys_kmem *km)
{
	if (!m_mgr.dops || !m_mgr.dops->flush)
		return -ENODEV;

	return m_mgr.dops->flush(km);
}

int mdw_mem_invalidate(struct apusys_kmem *km)
{
	if (!m_mgr.dops || !m_mgr.dops->invalidate)
		return -ENODEV;

	return m_mgr.dops->invalidate(km);
}

/**
 * Map fd (mem->fd) to iova (mem->iova).
 * mem->fd is is given by "ION shared fd" or "DMA buffer descriptor".
 * @param[in,out] mem The apusys memory
 * @return 0: Success
 */
int mdw_mem_map_iova(struct apusys_kmem *km)
{
	if (!m_mgr.dops || !m_mgr.dops->map_iova)
		return -ENODEV;

	return m_mgr.dops->map_iova(km);
}

/**
 * Unmap the iova of the apusys memory (mem->iova)
 * @param[in] mem The apusys memory
 * @return 0: Success
 */
int mdw_mem_unmap_iova(struct apusys_kmem *km)
{
	if (!m_mgr.dops || !m_mgr.dops->unmap_iova)
		return -ENODEV;

	return m_mgr.dops->unmap_iova(km);
}

/**
 * Map fd (mem->fd) to kernel virtual address (mem->kva).
 * mem->fd is is given by "ION shared fd" or "DMA buffer descriptor".
 * @param[in,out] mem The apusys memory
 * @return 0: Success
 */
int mdw_mem_map_kva(struct apusys_kmem *km)
{
	if (!m_mgr.dops || !m_mgr.dops->map_kva)
		return -ENODEV;

	return m_mgr.dops->map_kva(km);
}

/**
 * Unmap the kernel virtual address of the apusys memory (mem->kva)
 * @param[in] mem The apusys memory
 * @return 0: Success
 */
int mdw_mem_unmap_kva(struct apusys_kmem *km)
{
	if (!m_mgr.dops || !m_mgr.dops->unmap_kva)
		return -ENODEV;

	return m_mgr.dops->unmap_kva(km);
}

unsigned int mdw_mem_get_support(void)
{
	unsigned int mem_support = 0;

#ifdef CONFIG_MTK_ION
	mem_support |= (1UL << APUSYS_MEM_DRAM_ION);
#endif
	mem_support |= (1UL << APUSYS_MEM_DRAM_ION_AOSP);
	mem_support |= (1UL << APUSYS_MEM_VLM);

	return mem_support;
}

void mdw_mem_get_vlm(unsigned int *start, unsigned int *size)
{
	*start = APUSYS_VLM_START;
	*size = APUSYS_VLM_SIZE;
}

int mdw_mem_init(void)
{
	memset(&m_mgr, 0, sizeof(m_mgr));
	mutex_init(&m_mgr.mtx);
	INIT_LIST_HEAD(&m_mgr.list);

#ifdef CONFIG_MTK_ION
	m_mgr.dops = mdw_mops_ion();
#else
	m_mgr.dops = mdw_mops_aosp();
#endif
	if (IS_ERR_OR_NULL(m_mgr.dops) || !m_mgr.dops->init)
		return -ENODEV;

	return m_mgr.dops->init();
}

void mdw_mem_exit(void)
{
	if (m_mgr.dops)
		m_mgr.dops->exit();
}

uint64_t apusys_mem_query_kva(uint32_t iova)
{
	struct mdw_mem *m = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	uint64_t kva = 0;

	mdw_mem_debug("query kva from iova(0x%x)\n", iova);

	mutex_lock(&m_mgr.mtx);
	list_for_each_safe(list_ptr, tmp, &m_mgr.list) {
		m = list_entry(list_ptr, struct mdw_mem, m_item);
		if (iova >= m->kmem.iova &&
			iova < m->kmem.iova + m->kmem.iova_size) {
			kva = m->kmem.kva + (uint64_t)(iova - m->kmem.iova);
			mdw_mem_debug("query kva (0x%x->0x%llx)\n", iova, kva);
		}
	}
	mutex_unlock(&m_mgr.mtx);

	return kva;
}

uint32_t apusys_mem_query_iova(uint64_t kva)
{
	struct mdw_mem *m = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	uint32_t iova = 0;

	mdw_mem_debug("query iova from kva(0x%llx)\n", kva);

	mutex_lock(&m_mgr.mtx);
	list_for_each_safe(list_ptr, tmp, &m_mgr.list) {
		m = list_entry(list_ptr, struct mdw_mem, m_item);
		if (m->kmem.kva >= kva &&
			m->kmem.kva + m->kmem.size < kva) {
			iova = m->kmem.iova + (uint32_t)(kva - m->kmem.kva);
			mdw_mem_debug("query iova (0x%llx->0x%x)\n", kva, iova);
		}
	}
	mutex_unlock(&m_mgr.mtx);

	return iova;
}

int apusys_mem_flush(struct apusys_kmem *km)
{
	return mdw_mem_flush(km);
}

int apusys_mem_invalidate(struct apusys_kmem *km)
{
	return mdw_mem_invalidate(km);
}

