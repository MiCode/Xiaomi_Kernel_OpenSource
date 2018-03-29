/*
 * drivers/gpu/ion/ion_carveout_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <m4u.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/seq_file.h>
#include "ion_priv.h"
#include "ion_fb_heap.h"
#include "ion_drv_priv.h"
#include "mtk/ion_drv.h"
#include "mtk/mtk_ion.h"

/*fb heap base and size denamic access*/
struct ion_fb_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
	size_t size;
};
static int ion_fb_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
		void *unused);

ion_phys_addr_t ion_fb_allocate(struct ion_heap *heap, unsigned long size,
		unsigned long align) {
	struct ion_fb_heap
	*fb_heap = container_of(heap, struct ion_fb_heap, heap);
	unsigned long offset = gen_pool_alloc(fb_heap->pool, size);

	if (!offset) {
		IONMSG("[ion_fb_alloc]:fail!\n");
		return ION_CARVEOUT_ALLOCATE_FAIL;
	}

	return offset;
}

void ion_fb_free(struct ion_heap *heap, ion_phys_addr_t addr,
		unsigned long size) {
	struct ion_fb_heap
	*fb_heap = container_of(heap, struct ion_fb_heap, heap);

	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;

	gen_pool_free(fb_heap->pool, addr, size);
}

static int ion_fb_heap_phys(struct ion_heap *heap, struct ion_buffer *buffer,
		ion_phys_addr_t *addr, size_t *len) {
	ion_fb_buffer_info *pBufferInfo = (ion_fb_buffer_info *) buffer->priv_virt;

	if (!pBufferInfo) {
		IONMSG("[ion_fb_heap_phys]: Error. Invalid buffer.\n");
		return -EFAULT; /* Invalid buffer */
	}
	if (pBufferInfo->eModuleID == -1) {
		IONMSG("[ion_fb_heap_phys]: Error. Buffer not configured.\n");
		return -EFAULT; /* Buffer not configured. */
	}
	/*IONMSG("[ion_fb_heap_phys]: eModuleID = %d, len = 0x%x, pa = 0x%lx.\n",
			pBufferInfo->eModuleID, (unsigned int) buffer->size,
			pBufferInfo->priv_phys);*/

	/*Allocate MVA*/
	mutex_lock(&(pBufferInfo->lock));
	if (pBufferInfo->MVA == 0) {
		int ret = m4u_alloc_mva_sg(pBufferInfo->eModuleID, buffer->sg_table,
				buffer->size, pBufferInfo->security, pBufferInfo->coherent,
				&pBufferInfo->MVA);
		if (ret < 0) {
			mutex_unlock(&(pBufferInfo->lock));
			IONMSG("[ion_fb_heap_phys]: Error. Allocate MVA failed.\n");
			return -EFAULT;
		}
	}

	*addr = (ion_phys_addr_t) pBufferInfo->MVA;
	mutex_unlock(&(pBufferInfo->lock));
	*len = buffer->size;

	/*IONMSG("[ion_fb_heap_phys]: MVA = 0x%x, len = 0x%x.\n", pBufferInfo->MVA, (unsigned int) buffer->size);*/

	return 0;
}

static int ion_fb_heap_allocate(struct ion_heap *heap,
		struct ion_buffer *buffer, unsigned long size, unsigned long align,
		unsigned long flags) {
	ion_fb_buffer_info *pBufferInfo = NULL;
	ion_phys_addr_t paddr;

	if (align > PAGE_SIZE)
		return -EINVAL;

	paddr = ion_fb_allocate(heap, size, align);

	/*create fb buffer info for it*/
	pBufferInfo = kzalloc(sizeof(ion_fb_buffer_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(pBufferInfo)) {
		IONMSG("[ion_fb_heap_allocate]: Error. Allocate ion_buffer failed.\n");
		return -EFAULT;
	}

	pBufferInfo->priv_phys = paddr;
	pBufferInfo->pVA = 0;
	pBufferInfo->MVA = 0;
	pBufferInfo->eModuleID = -1;
	pBufferInfo->dbg_info.value1 = 0;
	pBufferInfo->dbg_info.value2 = 0;
	pBufferInfo->dbg_info.value3 = 0;
	pBufferInfo->dbg_info.value4 = 0;
	strncpy((pBufferInfo->dbg_info.dbg_name), "nothing", ION_MM_DBG_NAME_LEN);
	mutex_init(&(pBufferInfo->lock));

	buffer->priv_virt = pBufferInfo;

	/*IONMSG("[ion_fb_heap_allocate] Success! buffer->priv_phys = %lx\n", pBufferInfo->priv_phys);*/
	return pBufferInfo->priv_phys == ION_CARVEOUT_ALLOCATE_FAIL ? -ENOMEM : 0;
}

static void ion_fb_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	ion_fb_buffer_info *pBufferInfo = (ion_fb_buffer_info *) buffer->priv_virt;

	ion_fb_free(heap, pBufferInfo->priv_phys, buffer->size);

	pBufferInfo->priv_phys = ION_CARVEOUT_ALLOCATE_FAIL;
	kfree(pBufferInfo);
}

struct sg_table *ion_fb_heap_map_dma(struct ion_heap *heap,
		struct ion_buffer *buffer) {
	struct sg_table *table;
	int ret;
	ion_fb_buffer_info *pBufferInfo = (ion_fb_buffer_info *) buffer->priv_virt;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}
	sg_set_page(table->sgl, phys_to_page(pBufferInfo->priv_phys), buffer->size, 0);
	return table;
}

void ion_fb_heap_unmap_dma(struct ion_heap *heap, struct ion_buffer *buffer)
{
	sg_free_table(buffer->sg_table);
}

static struct ion_heap_ops fb_heap_ops = {
		.allocate = ion_fb_heap_allocate,
		.free = ion_fb_heap_free,
		.phys = ion_fb_heap_phys,
		.map_dma = ion_fb_heap_map_dma,
		.unmap_dma = ion_fb_heap_unmap_dma,
		.map_user = ion_heap_map_user,
		.map_kernel = ion_heap_map_kernel,
		.unmap_kernel = ion_heap_unmap_kernel,
};

#define ION_PRINT_LOG_OR_SEQ(seq_file, fmt, args...) \
		do {\
			if (seq_file)\
				seq_printf(seq_file, fmt, ##args);\
			else\
				printk(fmt, ##args);\
		} while (0)

static void ion_fb_chunk_show(struct gen_pool *pool,
		struct gen_pool_chunk *chunk, void *data) {
	int order, nlongs, nbits, i;
	struct seq_file *s = (struct seq_file *) data;

	order = pool->min_alloc_order;
	nbits = (chunk->end_addr - chunk->start_addr) >> order;
	nlongs = BITS_TO_LONGS(nbits);

	seq_printf(s, "phys_addr=0x%x bits=", (unsigned int) chunk->phys_addr);

	for (i = 0; i < nlongs; i++)
		seq_printf(s, "0x%x ", (unsigned int) chunk->bits[i]);

	seq_puts(s, "\n");
}

static int ion_fb_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
		void *unused) {
	struct ion_fb_heap
	*fb_heap = container_of(heap, struct ion_fb_heap, heap);
	size_t size_avail, total_size;

	total_size = gen_pool_size(fb_heap->pool);
	size_avail = gen_pool_avail(fb_heap->pool);

	seq_puts(s, "************************************************************\n");
	seq_printf(s, "total_size=0x%x, free=0x%x\n", (unsigned int) total_size,
			(unsigned int) size_avail);
	seq_puts(s, "************************************************************\n");

	gen_pool_for_each_chunk(fb_heap->pool, ion_fb_chunk_show, s);
	return 0;
}

struct ion_heap *ion_fb_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_fb_heap *fb_heap;

	fb_heap = kzalloc(sizeof(struct ion_fb_heap), GFP_KERNEL);
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
	fb_heap->heap.type = ION_HEAP_TYPE_FB;
	fb_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
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

	heap_data = kzalloc(sizeof(struct ion_platform_heap), GFP_KERNEL);
	if (!heap_data)
		return -ENOMEM;

	heap_data->id = ION_HEAP_TYPE_FB;
	heap_data->type = ION_HEAP_TYPE_FB;
	heap_data->name = "ion_fb_heap";
	heap_data->base = fb_base;
	heap_data->size = fb_size;
	heap_data->align = 0x1000;
	heap_data->priv = NULL;
	ion_drv_create_heap(heap_data);

	kfree(heap_data);

	return 0;
}
