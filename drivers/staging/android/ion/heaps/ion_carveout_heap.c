// SPDX-License-Identifier: GPL-2.0
/*
 * ION Memory Allocator carveout heap helper
 *
 * Copyright (C) 2011 Google, Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include "msm_ion_priv.h"
#include "ion_secure_util.h"

#define ION_CARVEOUT_ALLOCATE_FAIL	-1

#define to_carveout_heap(_heap) \
	container_of(to_msm_ion_heap(_heap), struct ion_carveout_heap, heap)

struct ion_carveout_heap {
	struct msm_ion_heap heap;
	struct rw_semaphore mem_sem;
	struct gen_pool *pool;
	phys_addr_t base;
};

static phys_addr_t ion_carveout_allocate(struct ion_heap *heap,
					 unsigned long size)
{
	struct ion_carveout_heap *carveout_heap = to_carveout_heap(heap);
	unsigned long offset = ION_CARVEOUT_ALLOCATE_FAIL;

	down_read(&carveout_heap->mem_sem);
	if (carveout_heap->pool) {
		offset = gen_pool_alloc(carveout_heap->pool, size);
		if (!offset) {
			offset = ION_CARVEOUT_ALLOCATE_FAIL;
			goto unlock;
		}
	}

unlock:
	up_read(&carveout_heap->mem_sem);
	return offset;
}

static void ion_carveout_free(struct ion_heap *heap, phys_addr_t addr,
			      unsigned long size)
{
	struct ion_carveout_heap *carveout_heap = to_carveout_heap(heap);

	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;

	down_read(&carveout_heap->mem_sem);
	if (carveout_heap->pool)
		gen_pool_free(carveout_heap->pool, addr, size);
	up_read(&carveout_heap->mem_sem);
}

static int ion_carveout_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size,
				      unsigned long flags)
{
	struct sg_table *table;
	phys_addr_t paddr;
	int ret;
	struct ion_carveout_heap *carveout_heap = to_carveout_heap(heap);
	struct msm_ion_buf_lock_state *lock_state;
	struct device *dev = carveout_heap->heap.dev;

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_free;

	lock_state = kzalloc(sizeof(*lock_state), GFP_KERNEL);
	if (!lock_state) {
		ret = -ENOMEM;
		goto err_free_table;
	}
	buffer->priv_virt = lock_state;

	paddr = ion_carveout_allocate(heap, size);
	if (paddr == ION_CARVEOUT_ALLOCATE_FAIL) {
		ret = -ENOMEM;
		goto err_free_umap;
	}

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	buffer->sg_table = table;

	if (ion_buffer_cached(buffer))
		ion_pages_sync_for_device(dev, sg_page(table->sgl),
					  buffer->size, DMA_FROM_DEVICE);
	ion_prepare_sgl_for_force_dma_sync(buffer->sg_table);

	return 0;

err_free_umap:
	kfree(lock_state);
err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);
	return ret;
}

static void ion_carveout_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_carveout_heap *carveout_heap = to_carveout_heap(heap);
	struct msm_ion_buf_lock_state *lock_state = buffer->priv_virt;
	struct sg_table *table = buffer->sg_table;
	struct page *page = sg_page(table->sgl);
	phys_addr_t paddr = page_to_phys(page);
	struct device *dev = carveout_heap->heap.dev;

	mutex_lock(&buffer->lock);
	if (hlos_accessible_buffer(buffer))
		ion_buffer_zero(buffer);

	if (lock_state && lock_state->locked)
		pr_warn("%s: buffer is locked while being freed\n", __func__);
	mutex_unlock(&buffer->lock);

	if (ion_buffer_cached(buffer))
		ion_pages_sync_for_device(dev, page, buffer->size,
					  DMA_BIDIRECTIONAL);

	ion_carveout_free(heap, paddr, buffer->size);
	kfree(buffer->priv_virt);
	sg_free_table(table);
	kfree(table);
}

static struct ion_heap_ops carveout_heap_ops = {
	.allocate = ion_carveout_heap_allocate,
	.free = ion_carveout_heap_free,
};

static int ion_heap_clear_pages(struct page **pages, int num, pgprot_t pgprot)
{
	void *addr = vm_map_ram(pages, num, -1, pgprot);

	if (!addr)
		return -ENOMEM;
	memset(addr, 0, PAGE_SIZE * num);
	vm_unmap_ram(addr, num);

	return 0;
}

static int ion_heap_sglist_zero(struct scatterlist *sgl, unsigned int nents,
				pgprot_t pgprot)
{
	int p = 0;
	int ret = 0;
	struct sg_page_iter piter;
	struct page *pages[32];

	for_each_sg_page(sgl, &piter, nents, 0) {
		pages[p++] = sg_page_iter_page(&piter);
		if (p == ARRAY_SIZE(pages)) {
			ret = ion_heap_clear_pages(pages, p, pgprot);
			if (ret)
				return ret;
			p = 0;
		}
	}
	if (p)
		ret = ion_heap_clear_pages(pages, p, pgprot);

	return ret;
}

static int ion_carveout_pages_zero(struct page *page, size_t size,
				      pgprot_t pgprot)
{
	struct scatterlist sg;

	sg_init_table(&sg, 1);
	sg_set_page(&sg, page, size, 0);
	return ion_heap_sglist_zero(&sg, 1, pgprot);
}

static int ion_carveout_init_heap_memory(struct ion_carveout_heap *co_heap,
					 phys_addr_t base, ssize_t size,
					 bool sync)
{
	struct page *page = pfn_to_page(PFN_DOWN(base));
	struct device *dev = co_heap->heap.dev;
	int ret;

	if (sync) {
		if (!pfn_valid(PFN_DOWN(base)))
			return -EINVAL;
		ion_pages_sync_for_device(dev, page, size, DMA_BIDIRECTIONAL);
	}

	ret = ion_carveout_pages_zero(page, size,
				      pgprot_writecombine(PAGE_KERNEL));
	if (ret)
		return ret;

	co_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!co_heap->pool)
		return -ENOMEM;

	co_heap->base = base;
	gen_pool_add(co_heap->pool, co_heap->base, size, -1);
	return ret;
}

static int ion_carveout_heap_add_memory(struct ion_heap *ion_heap,
					struct sg_table *sgt)
{
	struct ion_carveout_heap *carveout_heap;
	int ret;

	if (!ion_heap || !sgt || sgt->nents != 1)
		return -EINVAL;

	carveout_heap = to_carveout_heap(ion_heap);
	down_write(&carveout_heap->mem_sem);
	if (carveout_heap->pool) {
		ret = -EBUSY;
		goto unlock;
	}

	ret = ion_carveout_init_heap_memory(carveout_heap,
					    page_to_phys(sg_page(sgt->sgl)),
					    sgt->sgl->length, true);

unlock:
	up_write(&carveout_heap->mem_sem);
	return ret;
}

static int ion_carveout_heap_remove_memory(struct ion_heap *ion_heap,
					   struct sg_table *sgt)
{
	struct ion_carveout_heap *carveout_heap;
	phys_addr_t base;
	int ret = 0;

	if (!ion_heap || !sgt || sgt->nents != 1)
		return -EINVAL;

	carveout_heap = to_carveout_heap(ion_heap);
	down_write(&carveout_heap->mem_sem);
	if (!carveout_heap->pool) {
		ret = -EINVAL;
		goto unlock;
	}

	base = page_to_phys(sg_page(sgt->sgl));
	if (carveout_heap->base != base) {
		ret = -EINVAL;
		goto unlock;
	}

	if (gen_pool_size(carveout_heap->pool) !=
	    gen_pool_avail(carveout_heap->pool)) {
		ret = -EBUSY;
		goto unlock;
	}

	gen_pool_destroy(carveout_heap->pool);
	carveout_heap->pool = NULL;
unlock:
	up_write(&carveout_heap->mem_sem);
	return ret;
}

static struct msm_ion_heap_ops msm_carveout_heap_ops = {
	.add_memory = ion_carveout_heap_add_memory,
	.remove_memory = ion_carveout_heap_remove_memory,
};

static struct ion_heap *
__ion_carveout_heap_create(struct ion_platform_heap *heap_data,
			   bool sync)
{
	struct ion_carveout_heap *carveout_heap;
	int ret;

	struct device *dev = (struct device *)heap_data->priv;
	bool dynamic_heap = of_property_read_bool(dev->of_node,
						  "qcom,dynamic-heap");

	carveout_heap = kzalloc(sizeof(*carveout_heap), GFP_KERNEL);
	if (!carveout_heap)
		return ERR_PTR(-ENOMEM);

	carveout_heap->heap.dev = dev;
	if (!dynamic_heap) {
		ret = ion_carveout_init_heap_memory(carveout_heap,
						    heap_data->base,
						    heap_data->size, sync);
		if (ret) {
			kfree(carveout_heap);
			return ERR_PTR(ret);
		}
	}

	init_rwsem(&carveout_heap->mem_sem);
	carveout_heap->heap.ion_heap.ops = &carveout_heap_ops;
	carveout_heap->heap.ion_heap.buf_ops = msm_ion_dma_buf_ops;
	carveout_heap->heap.msm_heap_ops = &msm_carveout_heap_ops;
	carveout_heap->heap.ion_heap.type =
		(enum ion_heap_type)ION_HEAP_TYPE_MSM_CARVEOUT;
	if (!dynamic_heap)
		carveout_heap->heap.ion_heap.flags = ION_HEAP_FLAG_DEFER_FREE;

	return &carveout_heap->heap.ion_heap;
}

struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *heap_data)
{
	return __ion_carveout_heap_create(heap_data, true);
}

static void ion_carveout_heap_destroy(struct ion_heap *heap)
{
	struct ion_carveout_heap *carveout_heap = to_carveout_heap(heap);

	down_write(&carveout_heap->mem_sem);
	if (carveout_heap->pool)
		gen_pool_destroy(carveout_heap->pool);
	up_write(&carveout_heap->mem_sem);
	kfree(carveout_heap);
	carveout_heap = NULL;
}

struct ion_sc_entry {
	struct list_head list;
	struct ion_heap *heap;
	u32 token;
};

struct ion_sc_heap {
	struct msm_ion_heap heap;
	struct list_head children;
};

static struct ion_heap *ion_sc_find_child(struct ion_heap *heap, u32 flags)
{
	struct ion_sc_heap *manager;
	struct ion_sc_entry *entry;

	manager = container_of(to_msm_ion_heap(heap), struct ion_sc_heap, heap);
	flags = flags & ION_FLAGS_CP_MASK;
	list_for_each_entry(entry, &manager->children, list) {
		if (entry->token == flags)
			return entry->heap;
	}
	return NULL;
}

static int ion_sc_heap_allocate(struct ion_heap *heap,
				struct ion_buffer *buffer, unsigned long len,
				 unsigned long flags)
{
	struct ion_heap *child;

	/* cache maintenance is not possible on secure memory */
	flags &= ~((unsigned long)ION_FLAG_CACHED);
	buffer->flags = flags;

	child = ion_sc_find_child(heap, flags);
	if (!child)
		return -EINVAL;
	return ion_carveout_heap_allocate(child, buffer, len, flags);
}

static void ion_sc_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *child;
	struct sg_table *table = buffer->sg_table;
	struct msm_ion_buf_lock_state *lock_state = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	child = ion_sc_find_child(buffer->heap, buffer->flags);
	if (!child) {
		WARN(1, "ion_secure_carvout: invalid buffer flags on free. Memory will be leaked\n.");
		return;
	}

	mutex_lock(&buffer->lock);
	if (hlos_accessible_buffer(buffer))
		ion_buffer_zero(buffer);

	if (lock_state && lock_state->locked)
		pr_warn("%s: buffer is locked while being freed\n", __func__);
	mutex_unlock(&buffer->lock);

	ion_carveout_free(child, paddr, buffer->size);
	kfree(buffer->priv_virt);
	sg_free_table(table);
	kfree(table);
}

static struct ion_heap_ops ion_sc_heap_ops = {
	.allocate = ion_sc_heap_allocate,
	.free = ion_sc_heap_free,
};

static int ion_sc_get_dt_token(struct ion_sc_entry *entry,
			       struct device_node *np, u64 base, u64 size)
{
	u32 token;
	int ret = -EINVAL;

	if (of_property_read_u32(np, "token", &token))
		return -EINVAL;

	ret = ion_hyp_assign_from_flags(base, size, token);
	if (ret)
		pr_err("secure_carveout_heap: Assign token 0x%x failed\n",
		       token);
	else
		entry->token = token;

	return ret;
}

static int ion_sc_add_child(struct ion_sc_heap *manager,
			    struct device_node *np)
{
	struct device *dev = manager->heap.dev;
	struct ion_platform_heap heap_data = {0};
	struct ion_sc_entry *entry;
	struct device_node *phandle;
	struct resource res;
	int ret;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	INIT_LIST_HEAD(&entry->list);

	phandle = of_parse_phandle(np, "memory-region", 0);
	if (!phandle)
		goto out_free;

	ret = of_address_to_resource(phandle, 0, &res);
	of_node_put(phandle);
	if (ret)
		goto out_free;

	heap_data.priv = dev;
	heap_data.base = res.start;
	heap_data.size = resource_size(&res);

	/* This will zero memory initially */
	entry->heap = __ion_carveout_heap_create(&heap_data, false);
	if (IS_ERR(entry->heap))
		goto out_free;

	ret = ion_sc_get_dt_token(entry, np, heap_data.base, heap_data.size);
	if (ret) {
		ion_carveout_heap_destroy(entry->heap);
		goto out_free;
	}

	list_add(&entry->list, &manager->children);
	dev_info(dev, "ion_secure_carveout: creating heap@0x%llx, size 0x%llx\n",
		 heap_data.base, heap_data.size);
	return 0;

out_free:
	kfree(entry);
	return -EINVAL;
}

static void ion_secure_carveout_heap_destroy(struct ion_heap *heap)
{
	struct ion_sc_heap *manager =
		container_of(to_msm_ion_heap(heap), struct ion_sc_heap, heap);
	struct ion_sc_entry *entry, *tmp;

	list_for_each_entry_safe(entry, tmp, &manager->children, list) {
		ion_carveout_heap_destroy(entry->heap);
		kfree(entry);
	}
	kfree(manager);
}

struct ion_heap *
ion_secure_carveout_heap_create(struct ion_platform_heap *heap_data)
{
	struct device *dev = heap_data->priv;
	int ret;
	struct ion_sc_heap *manager;
	struct device_node *np;

	manager = kzalloc(sizeof(*manager), GFP_KERNEL);
	if (!manager)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&manager->children);
	manager->heap.dev = dev;

	for_each_child_of_node(dev->of_node, np) {
		ret = ion_sc_add_child(manager, np);
		if (ret) {
			dev_err(dev, "Creating child pool %s failed\n",
				np->name);
			goto err;
		}
	}

	manager->heap.ion_heap.ops = &ion_sc_heap_ops;
	manager->heap.ion_heap.buf_ops = msm_ion_dma_buf_ops;
	manager->heap.ion_heap.type =
		(enum ion_heap_type)ION_HEAP_TYPE_SECURE_CARVEOUT;
	return &manager->heap.ion_heap;

err:
	of_node_put(np);
	ion_secure_carveout_heap_destroy(&manager->heap.ion_heap);
	return ERR_PTR(-EINVAL);
}
