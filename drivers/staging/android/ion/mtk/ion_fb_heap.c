/*
 * Copyright (C) 2015 MediaTek Inc.
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
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/seq_file.h>
#include "ion_priv.h"
#include "ion_fb_heap.h"
#include "ion_drv_priv.h"
#include "mtk/ion_drv.h"
#include "mtk/mtk_ion.h"
//tablet
#ifdef CONFIG_MTK_IOMMU
#include "pseudo_m4u.h"
#endif
//smart phone m4u
#ifdef CONFIG_MTK_M4U
#include <m4u.h>
#endif
//smart phone iommu
#ifdef CONFIG_MTK_IOMMU_V2
#include <mach/pseudo_m4u.h>
#endif

#define ION_FB_ALLOCATE_FAIL	-1
/*fb heap base and size denamic access*/
struct ion_fb_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
	size_t size;
};

struct sg_table *ion_fb_heap_map_dma(struct ion_heap *heap,
				     struct ion_buffer *buffer)
{
	struct sg_table *table;
	int ret;
	struct ion_fb_buffer_info *buffer_info =
	    (struct ion_fb_buffer_info *)buffer->priv_virt;

	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}
	sg_set_page(table->sgl, phys_to_page(buffer_info->priv_phys),
		    buffer->size, 0);
	sg_dma_len(table->sgl) = buffer->size;
	table->sgl->length = buffer->size;
	return table;
}

void ion_fb_heap_unmap_dma(struct ion_heap *heap, struct ion_buffer *buffer)
{
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
}

ion_phys_addr_t ion_fb_allocate(struct ion_heap *heap, unsigned long size,
				unsigned long align)
{
	struct ion_fb_heap
	*fb_heap = container_of(heap, struct ion_fb_heap, heap);
	unsigned long offset = gen_pool_alloc(fb_heap->pool, size);

	if (!offset) {
		IONMSG("[ion_fb_alloc]:fail!\n");
		return ION_FB_ALLOCATE_FAIL;
	}

	return offset;
}

void ion_fb_free(struct ion_heap *heap, ion_phys_addr_t addr,
		 unsigned long size)
{
	struct ion_fb_heap
	*fb_heap = container_of(heap, struct ion_fb_heap, heap);

	if (addr == ION_FB_ALLOCATE_FAIL)
		return;

	gen_pool_free(fb_heap->pool, addr, size);
}

static int ion_fb_heap_phys(struct ion_heap *heap, struct ion_buffer *buffer,
			    ion_phys_addr_t *addr, size_t *len)
{
	struct ion_fb_buffer_info *buffer_info =
	    (struct ion_fb_buffer_info *)buffer->priv_virt;
	struct port_mva_info_t port_info;
	int domain_idx = 0;
	if (!buffer_info) {
		IONMSG("%s: Error. Invalid buffer.\n", __func__);
		return -EFAULT;	/* Invalid buffer */
	}
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	if (buffer_info->module_id == -1) {
		IONMSG("%s: Error. Buffer not configured.\n", __func__);
		return -EDOM;
	}
#endif

#ifdef MTK_ION_DMABUF_SUPPORT
	if (buffer_info->module_id == M4U_PORT_GPU)
		return 0;
#endif

	memset((void *)&port_info, 0, sizeof(port_info));
	port_info.emoduleid = buffer_info->module_id;
	port_info.cache_coherent = buffer_info->coherent;
	port_info.security = buffer_info->security;
	port_info.buf_size = buffer->size;
	port_info.flags = 0;
	domain_idx = ion_get_domain_id(1, &port_info.emoduleid);
	/*Allocate MVA */
	mutex_lock(&buffer_info->lock);
	if (buffer_info->MVA[domain_idx] == 0) {
#if (defined(CONFIG_MTK_M4U)) || defined(CONFIG_MTK_PSEUDO_M4U)
		int ret = m4u_alloc_mva_sg(&port_info, buffer->sg_table);

		if (ret < 0) {
			mutex_unlock(&buffer_info->lock);
			IONMSG(" %s: Error. Alloc MVA failed.\n", __func__);
			return -EFAULT;
		}
#endif
		buffer_info->MVA[domain_idx] = port_info.mva;
		*addr = (ion_phys_addr_t)buffer_info->MVA[domain_idx];
	} else {
		*addr = (ion_phys_addr_t)buffer_info->MVA[domain_idx];
	}

	mutex_unlock(&buffer_info->lock);
	*len = buffer->size;

	return 0;
}

static int ion_fb_heap_allocate(struct ion_heap *heap,
				struct ion_buffer *buffer, unsigned long size,
				unsigned long align, unsigned long flags)
{
	struct ion_fb_buffer_info *buffer_info = NULL;
	int domain_idx = 0;
	ion_phys_addr_t paddr;

	if (align > PAGE_SIZE)
		return -EINVAL;

	paddr = ion_fb_allocate(heap, size, align);

	/*create fb buffer info for it */
	buffer_info = kzalloc(sizeof(*buffer_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(buffer_info)) {
		IONMSG(" %s: Error. Alloc ion_buffer failed.\n", __func__);
		return -EFAULT;
	}

	buffer_info->priv_phys = paddr;
	buffer_info->VA = 0;
	buffer_info->fix_module_id = -1;
	buffer_info->pid = -1;
	buffer_info->mva_cnt = 0;
	for (domain_idx = 0; domain_idx < DOMAIN_NUM; domain_idx++) {
		buffer_info->MVA[domain_idx] = 0;
		buffer_info->FIXED_MVA[domain_idx] = 0;
		buffer_info->iova_start[domain_idx] = 0;
		buffer_info->iova_end[domain_idx] = 0;
		buffer_info->port[domain_idx] = -1;
	}
	buffer_info->module_id = -1;
	buffer_info->dbg_info.value1 = 0;
	buffer_info->dbg_info.value2 = 0;
	buffer_info->dbg_info.value3 = 0;
	buffer_info->dbg_info.value4 = 0;
	strncpy((buffer_info->dbg_info.dbg_name), "nothing",
		ION_MM_DBG_NAME_LEN);
	mutex_init(&buffer_info->lock);

	buffer->priv_virt = buffer_info;
	buffer->size = size;
	buffer->sg_table = ion_fb_heap_map_dma(heap, buffer);

	return buffer_info->priv_phys == ION_FB_ALLOCATE_FAIL ? -ENOMEM : 0;
}

static void ion_fb_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_fb_buffer_info *buffer_info =
	    (struct ion_fb_buffer_info *)buffer->priv_virt;
	struct sg_table *table = buffer->sg_table;
	int domain_idx = 0;
	if (!buffer_info) {
		IONMSG(" %s: Error: buffer_info is NULL.\n", __func__);
		return;
	}

	buffer->priv_virt = NULL;
#if (defined(CONFIG_MTK_M4U)) || defined(CONFIG_MTK_PSEUDO_M4U)
	for (domain_idx = 0; domain_idx < DOMAIN_NUM; domain_idx++) {
		if (buffer_info->MVA[domain_idx])
			m4u_dealloc_mva_sg(buffer_info->module_id, table,
					   buffer->size,
					   buffer_info->MVA[domain_idx]);
	}
#endif
	ion_fb_free(heap, buffer_info->priv_phys, buffer->size);
	ion_fb_heap_unmap_dma(heap, buffer);
	buffer_info->priv_phys = ION_FB_ALLOCATE_FAIL;
	kfree(buffer_info);
}

static struct ion_heap_ops fb_heap_ops = {
	.allocate = ion_fb_heap_allocate,
	.free = ion_fb_heap_free,
	.phys = ion_fb_heap_phys,
	///.map_dma = ion_fb_heap_map_dma,
	///.unmap_dma = ion_fb_heap_unmap_dma,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

#define ION_DUMP(seq_files, fmt, args...) \
do {\
	struct seq_file *file = (struct seq_file *)seq_files;\
	char *fmat = fmt;\
	if (file)\
		seq_printf(file, fmat, ##args);\
	else\
		printk(fmat, ##args);\
} while (0)

static void ion_fb_chunk_show(struct gen_pool *pool,
			      struct gen_pool_chunk *chunk, void *data)
{
	int order, nlongs, nbits, i;
	struct seq_file *s = (struct seq_file *)data;

	order = pool->min_alloc_order;
	nbits = (chunk->end_addr - chunk->start_addr) >> order;
	nlongs = BITS_TO_LONGS(nbits);

	ION_DUMP(s, "phys_addr=0x%x bits=", (unsigned int)chunk->phys_addr);

	for (i = 0; i < nlongs; i++)
		ION_DUMP(s, "0x%x ", (unsigned int)chunk->bits[i]);

	ION_DUMP(s, "\n");
}

static int ion_fb_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
				  void *unused)
{
	struct ion_fb_heap
	*fb_heap = container_of(heap, struct ion_fb_heap, heap);
	size_t size_avail, total_size;

	total_size = gen_pool_size(fb_heap->pool);
	size_avail = gen_pool_avail(fb_heap->pool);

	ION_DUMP(s,
		 "********************************************************\n");
	ION_DUMP(s, "total_size=0x%x, free=0x%x\n", (unsigned int)total_size,
		 (unsigned int)size_avail);
	ION_DUMP(s,
		 "********************************************************\n");

	gen_pool_for_each_chunk(fb_heap->pool, ion_fb_chunk_show, s);
	return 0;
}

struct ion_heap *ion_fb_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_fb_heap *fb_heap;

	fb_heap = kzalloc(sizeof(*fb_heap), GFP_KERNEL);
	if (!fb_heap)
		return ERR_PTR(-ENOMEM);

	fb_heap->pool = gen_pool_create(12, -1);
	if (!fb_heap->pool) {
		kfree(fb_heap);
		return ERR_PTR(-ENOMEM);
	}

	fb_heap->base = heap_data->base;
	fb_heap->size = heap_data->size;
	gen_pool_add(fb_heap->pool, fb_heap->base, fb_heap->size, -1);
	fb_heap->heap.ops = &fb_heap_ops;
	fb_heap->heap.type = (unsigned int)ION_HEAP_TYPE_FB;
	fb_heap->heap.flags = (unsigned int)ION_HEAP_FLAG_DEFER_FREE;
	fb_heap->heap.debug_show = ion_fb_heap_debug_show;

	return &fb_heap->heap;
}

void ion_fb_heap_destroy(struct ion_heap *heap)
{
	struct ion_fb_heap
	*fb_heap = container_of(heap, struct ion_fb_heap, heap);

	gen_pool_destroy(fb_heap->pool);
	kfree(fb_heap);
	fb_heap = NULL;
}

int ion_drv_create_FB_heap(ion_phys_addr_t fb_base, size_t fb_size)
{
	struct ion_platform_heap *heap_data;

	heap_data = kzalloc(sizeof(*heap_data), GFP_KERNEL);
	if (!heap_data)
		return -ENOMEM;

	heap_data->id = ION_HEAP_TYPE_FB;
	heap_data->type = (unsigned int)ION_HEAP_TYPE_FB;
	heap_data->name = "ion_fb_heap";
	heap_data->base = fb_base;
	heap_data->size = fb_size;
	heap_data->align = 0x1000;
	heap_data->priv = NULL;
	ion_drv_create_heap(heap_data);

	kfree(heap_data);

	return 0;
}
