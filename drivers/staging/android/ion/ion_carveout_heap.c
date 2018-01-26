/*
 * drivers/staging/android/ion/ion_carveout_heap.c
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
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ion.h"
#include "ion_priv.h"

struct ion_carveout_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
};

ion_phys_addr_t ion_carveout_allocate(struct ion_heap *heap,
				      unsigned long size,
				      unsigned long align)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	unsigned long offset = gen_pool_alloc(carveout_heap->pool, size);

	if (!offset)
		return ION_CARVEOUT_ALLOCATE_FAIL;

	return offset;
}

void ion_carveout_free(struct ion_heap *heap, ion_phys_addr_t addr,
		       unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;
	gen_pool_free(carveout_heap->pool, addr, size);
}

static int ion_carveout_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  ion_phys_addr_t *addr, size_t *len)
{
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	ion_phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	*addr = paddr;
	*len = buffer->size;
	return 0;
}

static int ion_carveout_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	struct sg_table *table;
	ion_phys_addr_t paddr;
	int ret;
	struct device *dev = heap->priv;

	if (align > PAGE_SIZE)
		return -EINVAL;

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_free;

	paddr = ion_carveout_allocate(heap, size, align);
	if (paddr == ION_CARVEOUT_ALLOCATE_FAIL) {
		ret = -ENOMEM;
		goto err_free_table;
	}

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	/*
	 * This is not correct - sg_dma_address needs a dma_addr_t that is valid
	 * for the targeted device, but this works on the currently targeted
	 * hardware.
	 */
	sg_dma_address(table->sgl) = sg_phys(table->sgl);

	buffer->priv_virt = table;

	if (ion_buffer_cached(buffer))
		dma_sync_sg_for_cpu(dev, table->sgl, table->nents,
				    DMA_BIDIRECTIONAL);

	return 0;

err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);
	return ret;
}

static void ion_carveout_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	struct device *dev = heap->priv;

	ion_phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	ion_heap_buffer_zero(buffer);

	if (ion_buffer_cached(buffer))
		dma_sync_sg_for_device(dev, table->sgl, table->nents,
				       DMA_BIDIRECTIONAL);

	ion_carveout_free(heap, paddr, buffer->size);
	sg_free_table(table);
	kfree(table);
}

static struct sg_table *ion_carveout_heap_map_dma(struct ion_heap *heap,
						  struct ion_buffer *buffer)
{
	return buffer->priv_virt;
}

static void ion_carveout_heap_unmap_dma(struct ion_heap *heap,
					struct ion_buffer *buffer)
{
}

static struct ion_heap_ops carveout_heap_ops = {
	.allocate = ion_carveout_heap_allocate,
	.free = ion_carveout_heap_free,
	.phys = ion_carveout_heap_phys,
	.map_dma = ion_carveout_heap_map_dma,
	.unmap_dma = ion_carveout_heap_unmap_dma,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_carveout_heap *carveout_heap;
	int ret;

	struct page *page;
	size_t size;
	struct device *dev = heap_data->priv;

	page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

	ion_pages_sync_for_device(dev, page, size, DMA_BIDIRECTIONAL);

	ret = ion_heap_pages_zero(page, size, pgprot_writecombine(PAGE_KERNEL));
	if (ret)
		return ERR_PTR(ret);

	carveout_heap = kzalloc(sizeof(*carveout_heap), GFP_KERNEL);
	if (!carveout_heap)
		return ERR_PTR(-ENOMEM);

	carveout_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!carveout_heap->pool) {
		kfree(carveout_heap);
		return ERR_PTR(-ENOMEM);
	}
	carveout_heap->base = heap_data->base;
	gen_pool_add(carveout_heap->pool, carveout_heap->base, heap_data->size,
		     -1);
	carveout_heap->heap.ops = &carveout_heap_ops;
	carveout_heap->heap.type = ION_HEAP_TYPE_CARVEOUT;
	carveout_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;

	return &carveout_heap->heap;
}

void ion_carveout_heap_destroy(struct ion_heap *heap)
{
	struct ion_carveout_heap *carveout_heap =
	     container_of(heap, struct  ion_carveout_heap, heap);

	gen_pool_destroy(carveout_heap->pool);
	kfree(carveout_heap);
	carveout_heap = NULL;
}

#include "msm/msm_ion.h"
#include <soc/qcom/secure_buffer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/msm_ion.h>

struct ion_sc_entry {
	struct list_head list;
	struct ion_heap *heap;
	u32 token;
};

struct ion_sc_heap {
	struct ion_heap heap;
	struct device *dev;
	struct list_head children;
};

static struct ion_heap *ion_sc_find_child(struct ion_heap *heap, u32 flags)
{
	struct ion_sc_heap *manager;
	struct ion_sc_entry *entry;

	manager = container_of(heap, struct ion_sc_heap, heap);
	flags = flags & ION_FLAGS_CP_MASK;
	list_for_each_entry(entry, &manager->children, list) {
		if (entry->token == flags)
			return entry->heap;
	}
	return NULL;
}

static int ion_sc_heap_allocate(struct ion_heap *heap,
				struct ion_buffer *buffer, unsigned long len,
				unsigned long align, unsigned long flags) {
	struct ion_heap *child;

	/* cache maintenance is not possible on secure memory */
	flags &= ~((unsigned long)ION_FLAG_CACHED);
	buffer->flags = flags;

	child = ion_sc_find_child(heap, flags);
	if (!child)
		return -EINVAL;

	return ion_carveout_heap_allocate(child, buffer, len, align, flags);
}

static void ion_sc_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *child;
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	ion_phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	child = ion_sc_find_child(buffer->heap, buffer->flags);
	if (!child) {
		WARN(1, "ion_secure_carvout: invalid buffer flags on free. Memory will be leaked\n.");
		return;
	}

	ion_carveout_free(child, paddr, buffer->size);
	sg_free_table(table);
	kfree(table);
}

static struct ion_heap_ops ion_sc_heap_ops = {
	.allocate = ion_sc_heap_allocate,
	.free = ion_sc_heap_free,
	.phys = ion_carveout_heap_phys,
	.map_dma = ion_carveout_heap_map_dma,
	.unmap_dma = ion_carveout_heap_unmap_dma,
};

static int ion_sc_get_dt_token(struct ion_sc_entry *entry,
			       struct device_node *np, u64 base, u64 size)
{
	u32 token;
	u32 *vmids, *modes;
	u32 nr, i;
	int ret = -EINVAL;
	u32 src_vm = VMID_HLOS;

	if (of_property_read_u32(np, "token", &token))
		return -EINVAL;

	nr = count_set_bits(token);
	vmids = kcalloc(nr, sizeof(*vmids), GFP_KERNEL);
	if (!vmids)
		return -ENOMEM;

	modes = kcalloc(nr, sizeof(*modes), GFP_KERNEL);
	if (!modes) {
		kfree(vmids);
		return -ENOMEM;
	}

	if ((token & ~ION_FLAGS_CP_MASK) ||
	    populate_vm_list(token, vmids, nr)) {
		pr_err("secure_carveout_heap: Bad token %x\n", token);
		goto out;
	}

	for (i = 0; i < nr; i++)
		if (vmids[i] == VMID_CP_SEC_DISPLAY)
			modes[i] = PERM_READ;
		else
			modes[i] = PERM_READ | PERM_WRITE;

	ret = hyp_assign_phys(base, size, &src_vm, 1, vmids, modes, nr);
	if (ret)
		pr_err("secure_carveout_heap: Assign token 0x%x failed\n",
		       token);
	else
		entry->token = token;
out:
	kfree(modes);
	kfree(vmids);
	return ret;
}

static int ion_sc_add_child(struct ion_sc_heap *manager,
			    struct device_node *np)
{
	struct device *dev = manager->dev;
	struct ion_platform_heap heap_data = {0};
	struct ion_sc_entry *entry;
	struct device_node *phandle;
	const __be32 *basep;
	u64 base, size;
	int ret;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	INIT_LIST_HEAD(&entry->list);

	phandle = of_parse_phandle(np, "memory-region", 0);
	if (!phandle)
		goto out_free;

	basep = of_get_address(phandle,  0, &size, NULL);
	if (!basep)
		goto out_free;

	base = of_translate_address(phandle, basep);
	if (base == OF_BAD_ADDR)
		goto out_free;

	heap_data.priv = dev;
	heap_data.base = base;
	heap_data.size = size;

	/* This will zero memory initially */
	entry->heap = ion_carveout_heap_create(&heap_data);
	if (IS_ERR(entry->heap))
		goto out_free;

	ret = ion_sc_get_dt_token(entry, np, base, size);
	if (ret)
		goto out_free_carveout;

	list_add(&entry->list, &manager->children);
	dev_info(dev, "ion_secure_carveout: creating heap@0x%llx, size 0x%llx\n",
		 base, size);
	return 0;

out_free_carveout:
	ion_carveout_heap_destroy(entry->heap);
out_free:
	kfree(entry);
	return -EINVAL;
}

void ion_secure_carveout_heap_destroy(struct ion_heap *heap)
{
	struct ion_sc_heap *manager =
		container_of(heap, struct ion_sc_heap, heap);
	struct ion_sc_entry *entry, *tmp;

	list_for_each_entry_safe(entry, tmp, &manager->children, list) {
		ion_carveout_heap_destroy(entry->heap);
		kfree(entry);
	}
	kfree(manager);
}

struct ion_heap *ion_secure_carveout_heap_create(
			struct ion_platform_heap *heap_data)
{
	struct device *dev = heap_data->priv;
	int ret;
	struct ion_sc_heap *manager;
	struct device_node *np;

	manager = kzalloc(sizeof(*manager), GFP_KERNEL);
	if (!manager)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&manager->children);
	manager->dev = dev;

	for_each_child_of_node(dev->of_node, np) {
		ret = ion_sc_add_child(manager, np);
		if (ret) {
			dev_err(dev, "Creating child pool %s failed\n",
				np->name);
			goto err;
		}
	}

	manager->heap.ops = &ion_sc_heap_ops;
	manager->heap.type = ION_HEAP_TYPE_SECURE_CARVEOUT;
	return &manager->heap;

err:
	ion_secure_carveout_heap_destroy(&manager->heap);
	return ERR_PTR(-EINVAL);
}
