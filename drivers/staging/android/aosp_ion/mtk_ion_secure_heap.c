// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/dma-buf.h>
#include "ion.h"

#ifdef CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM
#include "trusted_mem_api.h"
#include "memory_ssmr.h"

static size_t sec_heap_total_memory;

char heap_name[32][MAX_HEAP_NAME];

#define ION_FLAG_MM_HEAP_INIT_ZERO BIT(16)

struct ion_sec_heap {
	struct ion_heap heap;
	int ssmr_id;
};

static struct sg_table *ion_sec_heap_map_dma(struct ion_heap *heap,
					     struct ion_buffer *buffer)
{
	struct sg_table *table;
	int ret;

	pr_debug("[ION] %s enter\n", __func__);

	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}

	sg_set_page(table->sgl, 0, 0, 0);
	pr_debug("[ION] %s exit\n", __func__);
	return table;
}

static void ion_sec_heap_unmap_dma(struct ion_heap *heap,
				   struct ion_buffer *buffer)
{
	pr_debug("[ION] %s\n", __func__);
	sg_free_table(buffer->sg_table);
}

static int ion_sec_heap_allocate(struct ion_heap *heap,
				 struct ion_buffer *buffer,
				 unsigned long size,
				 unsigned long flags)
{
	struct ion_sec_heap *sec_heap = container_of(heap,
						     struct ion_sec_heap,
						     heap);
	u32 sec_handle = 0;
	u32 refcount = 0;

	pr_debug("[ION] %s enter name:%s, ssmr_id:%d, size:%zu\n",
		 __func__, heap->name, sec_heap->ssmr_id,
		 buffer->size);

	if (flags & ION_FLAG_MM_HEAP_INIT_ZERO)
		trusted_mem_api_alloc_zero(sec_heap->ssmr_id, 0, size,
					   &refcount, &sec_handle,
					   (uint8_t *)heap->name,
					   heap->id);
	else
		trusted_mem_api_alloc(sec_heap->ssmr_id, 0, size, &refcount,
				      &sec_handle, (uint8_t *)heap->name,
				      heap->id);

	if (sec_handle <= 0) {
		pr_err("%s alloc security memory failed, total size %zu\n",
		       __func__, sec_heap_total_memory);
		return -ENOMEM;
	}

	buffer->flags &= ~ION_FLAG_CACHED;
	buffer->size = size;
	buffer->sg_table = ion_sec_heap_map_dma(heap, buffer);
	sg_dma_address(buffer->sg_table->sgl) = (dma_addr_t)sec_handle;
	sec_heap_total_memory += size;

	pr_debug("[ION] secure alloc handle:0x%x\n", sec_handle);
	return 0;
}

static void ion_sec_heap_free(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	struct ion_heap *heap = buffer->heap;
	struct ion_sec_heap *sec_heap = container_of(heap,
						     struct ion_sec_heap,
						     heap);
	u32 sec_handle = 0;

	pr_debug("[ION] %s enter name:%s, ssmr_id:%d, size:%zu\n",
		 __func__, heap->name, sec_heap->ssmr_id,
		 buffer->size);

	sec_heap_total_memory -= buffer->size;
	sec_handle = (u32)sg_dma_address(buffer->sg_table->sgl);

	trusted_mem_api_unref(sec_heap->ssmr_id, sec_handle,
			      (uint8_t *)buffer->heap->name,
			      buffer->heap->id);

	ion_sec_heap_unmap_dma(heap, buffer);
	kfree(table);

	pr_debug("[ION] %s exit, total %zu\n", __func__, sec_heap_total_memory);
}

static struct ion_heap_ops sec_heap_ops = {
	.allocate = ion_sec_heap_allocate,
	.free = ion_sec_heap_free,
};

static int ion_sec_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
				   void *unused)
{
	return 0;
}

static struct ion_heap *__ion_sec_heap_create(int ssmr_id)
{
	struct ion_sec_heap *sec_heap;

	pr_info("[ION] %s enter\n", __func__);

	sec_heap = kzalloc(sizeof(*sec_heap), GFP_KERNEL);
	if (!sec_heap)
		return ERR_PTR(-ENOMEM);

	sec_heap->ssmr_id = ssmr_id;
	sec_heap->heap.ops = &sec_heap_ops;
	sec_heap->heap.type = ION_HEAP_TYPE_CUSTOM;
	sec_heap->heap.flags &= ~ION_HEAP_FLAG_DEFER_FREE;
	sec_heap->heap.debug_show = ion_sec_heap_debug_show;

	return &sec_heap->heap;
}
#endif
static int ion_sec_heap_create(void)
{
#ifdef CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM
	struct ion_heap *heap;
	int i, ssmr_id, heap_total;

	heap_total = ssmr_query_total_sec_heap_count();
	if (!heap_total)
		pr_warn("[ION] ssmr is not ready for secure heap\n");

	for (i = 0; i < heap_total; i++) {
		ssmr_id = ssmr_query_heap_info(i, heap_name[i]);
		heap = __ion_sec_heap_create(ssmr_id);
		if (IS_ERR(heap)) {
			pr_err("[ION] %s failed\n", __func__);
			return PTR_ERR(heap);
		}
		heap->name = heap_name[i];
		ion_device_add_heap(heap);
		pr_info("[ION] ssmr_id:%d, name:%s, heap_id:%u\n",
			ssmr_id, heap->name, heap->id);
	}
#else
	pr_info("[ION] ion secure heap not support\n");
#endif
	return 0;
}

device_initcall(ion_sec_heap_create);
