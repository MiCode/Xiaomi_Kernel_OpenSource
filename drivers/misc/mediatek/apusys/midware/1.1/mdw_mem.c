// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/idr.h>

#include "apusys_drv.h"
#include "mdw_cmn.h"
#include "mdw_mem.h"
#include "mdw_mem_cmn.h"
#include "reviser_export.h"

static DEFINE_IDR(apusys_mem_idr);

#define APUSYS_OPTIONS_MEM_ION
#define APUSYS_OPTIONS_MEM_VLM

struct mdw_mem_mgr {
	struct list_head list;
	struct mutex mtx;
	struct mutex mtx_idr;
	struct mdw_mem_ops *dops; //dram mem ops
};

static struct mdw_mem_mgr m_mgr;

//----------------------------------------------
void mdw_mem_u2k(struct apusys_mem *umem, struct apusys_kmem *kmem)
{
	kmem->khandle = 0;
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
	int id = 0;

	umem->iova = kmem->iova;
	umem->size = kmem->size;
	umem->iova_size = kmem->iova_size;
	if (!mdw_mem_create_idr(kmem->khandle, &id)) {
		umem->khandle = id;
		kmem->uidr = id;
	}
}

int mdw_mem_create_idr(uint64_t handle, int *id)
{
	int value = 0;

	mutex_lock(&m_mgr.mtx_idr);
	value = idr_alloc(&apusys_mem_idr, (void *)handle, 1, 0, GFP_KERNEL);
	mutex_unlock(&m_mgr.mtx_idr);
	if (value > 0) {
		*id = value;
	} else {
		mdw_drv_err("IDR create fail 0x%llx\n", handle);
		return -EINVAL;
	}

	return 0;
}
int mdw_mem_u2k_handle(struct apusys_kmem *kmem, struct apusys_mem *umem)
{
	unsigned long long handle = 0;
	int ret = 0;

	mutex_lock(&m_mgr.mtx_idr);
	handle = (uint64_t) idr_find(&apusys_mem_idr, umem->khandle);
	mutex_unlock(&m_mgr.mtx_idr);
	if (!handle) {
		mdw_drv_err("Invalid %llx\n", umem->khandle);
		mdw_drv_err("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx/%d)\n",
				kmem->fd, kmem->uva, kmem->iova, kmem->size,
				kmem->iova_size, kmem->khandle, kmem->kva,
				kmem->uidr);
		mdw_drv_err("umem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/%d)\n",
				umem->fd, umem->uva, umem->iova, umem->size,
				umem->iova_size, umem->khandle, umem->mem_type);
		ret = -EINVAL;
	} else {
		kmem->khandle = (uint64_t) handle;
	}

	return ret;
}
int mdw_mem_delete_idr(int id)
{
	mutex_lock(&m_mgr.mtx_idr);
	idr_remove(&apusys_mem_idr, id);
	mutex_unlock(&m_mgr.mtx_idr);
	return 0;
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

int mdw_mem_alloc(struct mdw_mem *m)
{
	int ret = 0;

	if (!m_mgr.dops)
		return -ENODEV;

	ret = m_mgr.dops->alloc(&m->kmem);
	if (ret)
		return ret;

	m->kmem.property = APUSYS_MEM_PROP_ALLOC;
	mdw_mem_list_add(m);

	return ret;
}

int mdw_mem_free(struct mdw_mem *m)
{
	int ret = 0;

	if (!m_mgr.dops)
		return -ENODEV;

	ret = m_mgr.dops->free(&m->kmem);
	mdw_mem_list_del(m);

	return ret;
}

int mdw_mem_import(struct mdw_mem *m)
{
	int ret = 0;

	if (!m_mgr.dops)
		return -ENODEV;

	ret = m_mgr.dops->map_iova(&m->kmem);
	if (ret)
		return ret;

	m->kmem.property = APUSYS_MEM_PROP_IMPORT;
	mdw_mem_list_add(m);

	return ret;
}

int mdw_mem_unimport(struct mdw_mem *m)
{
	int ret = 0;

	if (!m_mgr.dops)
		return -ENODEV;

	ret = m_mgr.dops->unmap_iova(&m->kmem);
	mdw_mem_list_del(m);

	return ret;
}

int mdw_mem_map(struct mdw_mem *m)
{
	int ret = 0;

	if (!m_mgr.dops)
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
	if (!m_mgr.dops)
		return -ENODEV;

	m_mgr.dops->unmap_iova(&m->kmem);
	m_mgr.dops->unmap_kva(&m->kmem);
	mdw_mem_list_del(m);

	return 0;
}

int mdw_mem_flush(struct apusys_kmem *km)
{
	if (!m_mgr.dops)
		return -ENODEV;

	return m_mgr.dops->flush(km);
}

int mdw_mem_invalidate(struct apusys_kmem *km)
{
	if (!m_mgr.dops)
		return -ENODEV;

	return m_mgr.dops->invalidate(km);
}

int mdw_mem_map_iova(struct apusys_kmem *km)
{
	if (!m_mgr.dops)
		return -ENODEV;

	return m_mgr.dops->map_iova(km);
}

int mdw_mem_unmap_iova(struct apusys_kmem *km)
{
	if (!m_mgr.dops)
		return -ENODEV;

	return m_mgr.dops->unmap_iova(km);
}

int mdw_mem_map_kva(struct apusys_kmem *km)
{
	if (!m_mgr.dops)
		return -ENODEV;

	return m_mgr.dops->map_kva(km);
}

int mdw_mem_unmap_kva(struct apusys_kmem *km)
{
	if (!m_mgr.dops)
		return -ENODEV;

	return m_mgr.dops->unmap_kva(km);
}

unsigned int mdw_mem_get_support(void)
{
	unsigned int mem_support = 0;

#ifdef APUSYS_OPTIONS_MEM_ION
	mem_support |= (1UL << APUSYS_MEM_DRAM_ION);
#endif

#ifdef APUSYS_OPTIONS_MEM_DMA
	mem_support |= (1UL << APUSYS_MEM_DRAM_DMA);
#endif

#ifdef APUSYS_OPTIONS_MEM_VLM
	mem_support |= (1UL << APUSYS_MEM_VLM);
#endif

	return mem_support;
}

void mdw_mem_get_vlm(unsigned int *start, unsigned int *size)
{
	unsigned int vlm_size = 0;
	unsigned int vlm_addr = 0;

	reviser_get_resource_vlm(&vlm_addr, &vlm_size);

	*start = vlm_addr;
	*size = vlm_size;

	//mdw_drv_info("reviser vlm_addr(0x%x) vlm_size(0x%x)\n", vlm_addr, vlm_size);
}

int mdw_mem_init(void)
{
	memset(&m_mgr, 0, sizeof(m_mgr));

	mutex_init(&m_mgr.mtx);
	mutex_init(&m_mgr.mtx_idr);
	INIT_LIST_HEAD(&m_mgr.list);

	m_mgr.dops = mdw_mem_ion_init();
	if (IS_ERR_OR_NULL(m_mgr.dops))
		return -ENODEV;

	return 0;
}

void mdw_mem_exit(void)
{
	if (m_mgr.dops)
		m_mgr.dops->destroy();
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
		mdw_mem_debug("m(0x%llx/0x%x)(0x%x/0x%x)\n",
			m->kmem.kva, m->kmem.size,
			m->kmem.iova, m->kmem.iova_size);
		if (iova >= m->kmem.iova &&
			(uint64_t)iova < (uint64_t)m->kmem.iova +
			(uint64_t)m->kmem.iova_size &&
			m->kmem.kva) {
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
		mdw_mem_debug("m(0x%llx/0x%x)(0x%x/0x%x)\n",
			m->kmem.kva, m->kmem.size,
			m->kmem.iova, m->kmem.iova_size);
		if (m->kmem.kva >= kva &&
			m->kmem.kva + m->kmem.size < kva &&
			m->kmem.iova) {
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
