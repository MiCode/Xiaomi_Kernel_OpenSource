/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include <linux/slab.h>

#include <linux/dma-mapping.h>
#include <asm/mman.h>

#include "apusys_dbg.h"
#include "apusys_plat.h"
#include "mdw_cmn.h"
#include "apusys_options.h"
#include "apusys_drv.h"
#include "memory_mgt.h"
#include "memory_ion.h"
#include "memory_dma.h"

static struct apusys_mem_mgr g_mem_mgr;
static int g_mem_type = APUSYS_MEM_DRAM_ION;

struct mem_record {
	struct apusys_kmem kmem;
	struct list_head m_list;
};

//----------------------------------------------
int apusys_mem_copy_from_user(struct apusys_mem *umem, struct apusys_kmem *kmem)
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

	return 0;
}

int apusys_mem_copy_to_user(struct apusys_mem *umem, struct apusys_kmem *kmem)
{
	umem->iova = kmem->iova;
	umem->size = kmem->size;
	umem->iova_size = kmem->iova_size;
	umem->khandle = kmem->khandle;

	return 0;
}

int apusys_mem_alloc(struct apusys_kmem *mem)
{
	int ret = 0;
	struct mem_record *mr = NULL;

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_alloc(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = dma_mem_alloc(&g_mem_mgr, mem);
		break;
	default:
		mdw_drv_err("invalid argument\n");
		ret = -EINVAL;
		break;
	}
	if (!ret) {
		mem->property = APUSYS_MEM_PROP_ALLOC;

		/* debug purpose */
		if (dbg_get_prop(DBG_PROP_QUERY_MEM) > 0) {
			mr = vmalloc(sizeof(struct mem_record));
			if (mr != NULL) {
				mdw_mem_debug("mr(0x%llx/%d/0x%x/0x%x/0x%llx)\n",
					mem->uva,
					mem->size,
					mem->iova,
					mem->iova_size,
					mem->kva);
				memcpy(&mr->kmem, mem,
					sizeof(struct apusys_kmem));

				INIT_LIST_HEAD(&mr->m_list);
				mutex_lock(&g_mem_mgr.list_mtx);
				list_add_tail(&mr->m_list,
					&g_mem_mgr.list);
				mutex_unlock(&g_mem_mgr.list_mtx);
			}
		}
	}

	return ret;
}

int apusys_mem_free(struct apusys_kmem *mem)
{
	int ret = 0;
	struct mem_record *mr = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_free(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = dma_mem_free(&g_mem_mgr, mem);
		break;
	default:
		mdw_drv_err("invalid argument\n");
		ret = -EINVAL;
		break;
	}

	/* debug purpose */
	if (!ret && dbg_get_prop(DBG_PROP_QUERY_MEM) > 0) {
		mutex_lock(&g_mem_mgr.list_mtx);
		list_for_each_safe(list_ptr, tmp, &g_mem_mgr.list) {
			mr = list_entry(list_ptr, struct mem_record, m_list);
			if (mr->kmem.iova == mem->iova) {
				mdw_mem_debug("mr(0x%llx/%d/0x%x/0x%x/0x%llx)\n",
					mem->uva,
					mem->size,
					mem->iova,
					mem->iova_size,
					mem->kva);
				list_del(&mr->m_list);
				vfree(mr);
				break;
			}
		}
		mutex_unlock(&g_mem_mgr.list_mtx);
	}

	return ret;
}

int apusys_mem_import(struct apusys_kmem *mem)
{
	int ret = 0;
	struct mem_record *mr = NULL;

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_import(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = -EINVAL;
		break;
	default:
		mdw_drv_err("invalid argument\n");
		ret = -EINVAL;
		break;
	}

	if (!ret) {
		mem->property = APUSYS_MEM_PROP_IMPORT;

		/* debug purpose */
		if (dbg_get_prop(DBG_PROP_QUERY_MEM) > 0) {
			mr = vmalloc(sizeof(struct mem_record));
			if (mr != NULL) {
				mdw_mem_debug("mr(0x%llx/%d/0x%x/0x%x/0x%llx)\n",
					mem->uva,
					mem->size,
					mem->iova,
					mem->iova_size,
					mem->kva);
				memcpy(&mr->kmem, mem,
					sizeof(struct apusys_kmem));

				INIT_LIST_HEAD(&mr->m_list);
				mutex_lock(&g_mem_mgr.list_mtx);
				list_add_tail(&mr->m_list,
					&g_mem_mgr.list);
				mutex_unlock(&g_mem_mgr.list_mtx);
			}
		}
	}
	return ret;
}
int apusys_mem_unimport(struct apusys_kmem *mem)
{
	int ret = 0;
	struct mem_record *mr = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_unimport(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = -EINVAL;
		break;
	default:
		mdw_drv_err("invalid argument\n");
		ret = -EINVAL;
		break;
	}

	/* debug purpose */
	if (!ret && dbg_get_prop(DBG_PROP_QUERY_MEM) > 0) {
		mutex_lock(&g_mem_mgr.list_mtx);
		list_for_each_safe(list_ptr, tmp, &g_mem_mgr.list) {
			mr = list_entry(list_ptr, struct mem_record, m_list);
			if (mr->kmem.iova == mem->iova) {
				mdw_mem_debug("mr(0x%llx/%d/0x%x/0x%x/0x%llx)\n",
					mem->uva,
					mem->size,
					mem->iova,
					mem->iova_size,
					mem->kva);
				list_del(&mr->m_list);
				vfree(mr);
				break;
			}
		}
		mutex_unlock(&g_mem_mgr.list_mtx);
	}

	return ret;
}

int apusys_mem_release(struct apusys_kmem *mem)
{
	int ret = 0;

	switch (mem->property) {
	case APUSYS_MEM_PROP_ALLOC:
		ret = apusys_mem_free(mem);
		break;
	case APUSYS_MEM_PROP_IMPORT:
		ret = apusys_mem_unimport(mem);
		break;
	default:
		mdw_drv_err("invalid argument\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

int apusys_mem_flush(struct apusys_kmem *mem)
{
	int ret = 0;

	mdw_lne_debug();

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_flush(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = -EINVAL;
		break;
	default:
		mdw_drv_err("invalid argument\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

int apusys_mem_invalidate(struct apusys_kmem *mem)
{
	int ret = 0;

	mdw_lne_debug();

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_invalidate(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = -EINVAL;
		break;
	default:
		mdw_drv_err("invalid argument\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

int apusys_mem_init(struct device *dev)
{
	int ret = 0;

	mdw_drv_debug("+\n");
	g_mem_mgr.dev = dev;
	INIT_LIST_HEAD(&g_mem_mgr.list);
	mutex_init(&g_mem_mgr.list_mtx);

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_init(&g_mem_mgr);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = dma_mem_init(&g_mem_mgr);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mdw_drv_debug("-\n");
	return ret;

}

int apusys_mem_destroy(void)
{

	int ret = 0;

	mdw_lne_debug();
	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_destroy(&g_mem_mgr);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = dma_mem_destroy(&g_mem_mgr);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

int apusys_mem_ctl(struct apusys_mem_ctl *ctl_data, struct apusys_kmem *mem)
{
	int ret = 0;

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_ctl(&g_mem_mgr, ctl_data, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

int apusys_mem_map_iova(struct apusys_kmem *mem)
{
	int ret = 0;

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_map_iova(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int apusys_mem_map_kva(struct apusys_kmem *mem)
{
	int ret = 0;

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_map_kva(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int apusys_mem_unmap_iova(struct apusys_kmem *mem)
{
	int ret = 0;

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_unmap_iova(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int apusys_mem_unmap_kva(struct apusys_kmem *mem)
{
	int ret = 0;

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_unmap_kva(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

unsigned int apusys_mem_get_support(void)
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

int apusys_mem_get_vlm(unsigned int *start, unsigned int *size)
{
	if (start == NULL || size == NULL)
		return -EINVAL;

	*start = APUSYS_VLM_START;
	*size = APUSYS_VLM_SIZE;

	return 0;
}

uint64_t apusys_mem_query_kva(uint32_t iova)
{
	struct mem_record *mr = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	uint64_t kva = 0;

	mdw_mem_debug("query kva from iova(0x%x)\n", iova);

	mutex_lock(&g_mem_mgr.list_mtx);
	list_for_each_safe(list_ptr, tmp, &g_mem_mgr.list) {
		mr = list_entry(list_ptr, struct mem_record, m_list);
		if (iova >= mr->kmem.iova &&
			iova < mr->kmem.iova + mr->kmem.iova_size) {
			kva = mr->kmem.kva + (uint64_t)(iova - mr->kmem.iova);
			mdw_mem_debug("query kva (0x%x->0x%llx)\n", iova, kva);
		}
	}
	mutex_unlock(&g_mem_mgr.list_mtx);

	return kva;
}
EXPORT_SYMBOL(apusys_mem_query_kva);

uint32_t apusys_mem_query_iova(uint64_t kva)
{
	struct mem_record *mr = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	uint32_t iova = 0;

	mdw_mem_debug("query iova from kva(0x%llx)\n", kva);

	mutex_lock(&g_mem_mgr.list_mtx);
	list_for_each_safe(list_ptr, tmp, &g_mem_mgr.list) {
		mr = list_entry(list_ptr, struct mem_record, m_list);
		if (mr->kmem.kva >= kva &&
			mr->kmem.kva + mr->kmem.size < kva) {
			iova = mr->kmem.iova + (uint32_t)(kva - mr->kmem.kva);
			mdw_mem_debug("query iova (0x%llx->0x%x)\n", kva, iova);
		}
	}
	mutex_unlock(&g_mem_mgr.list_mtx);

	return iova;
}
EXPORT_SYMBOL(apusys_mem_query_iova);
