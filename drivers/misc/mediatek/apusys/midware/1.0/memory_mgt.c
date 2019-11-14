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


#include "apusys_cmn.h"
#include "apusys_options.h"
#include "apusys_drv.h"
#include "memory_mgt.h"
#include "memory_ion.h"
#include "memory_dma.h"

struct apusys_mem_mgr g_mem_mgr;
static int g_mem_type = APUSYS_MEM_DRAM_ION;

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

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_alloc(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = dma_mem_alloc(&g_mem_mgr, mem);
		break;
	default:
		LOG_ERR("invalid argument\n");
		ret = -EINVAL;
		break;
	}
	if (!ret)
		mem->property = APUSYS_MEM_PROP_ALLOC;


	return ret;
}

int apusys_mem_free(struct apusys_kmem *mem)
{
	int ret = 0;

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_free(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = dma_mem_free(&g_mem_mgr, mem);
		break;
	default:
		LOG_ERR("invalid argument\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

int apusys_mem_import(struct apusys_kmem *mem)
{
	int ret = 0;

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_import(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = -EINVAL;
		break;
	default:
		LOG_ERR("invalid argument\n");
		ret = -EINVAL;
		break;
	}

	if (!ret)
		mem->property = APUSYS_MEM_PROP_IMPORT;

	return ret;
}
int apusys_mem_unimport(struct apusys_kmem *mem)
{
	int ret = 0;

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_unimport(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = -EINVAL;
		break;
	default:
		LOG_ERR("invalid argument\n");
		ret = -EINVAL;
		break;
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
		LOG_ERR("invalid argument\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

int apusys_mem_flush(struct apusys_kmem *mem)
{
	int ret = 0;

	DEBUG_TAG;

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_flush(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = -EINVAL;
		break;
	default:
		LOG_ERR("invalid argument\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

int apusys_mem_invalidate(struct apusys_kmem *mem)
{
	int ret = 0;

	DEBUG_TAG;

	switch (g_mem_type) {
	case APUSYS_MEM_DRAM_ION:
		ret = ion_mem_invalidate(&g_mem_mgr, mem);
		break;
	case APUSYS_MEM_DRAM_DMA:
		ret = -EINVAL;
		break;
	default:
		LOG_ERR("invalid argument\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

int apusys_mem_init(struct device *dev)
{

	int ret = 0;

	LOG_INFO("+\n");
	g_mem_mgr.dev = dev;
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

	LOG_INFO("-\n");
	return ret;

}

int apusys_mem_destroy(void)
{

	int ret = 0;

	DEBUG_TAG;
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
