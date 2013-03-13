/*
 * drivers/gpu/ion/ion_secure_cma_heap.c
 *
 * Copyright (C) Linaro 2012
 * Author: <benjamin.gaignard@linaro.org> for ST-Ericsson.
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/device.h>
#include <linux/ion.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/msm_ion.h>
#include <mach/iommu_domains.h>

#include <asm/cacheflush.h>

/* for ion_heap_ops structure */
#include "ion_priv.h"
#include "msm/ion_cp_common.h"

#define ION_CMA_ALLOCATE_FAILED NULL

struct ion_secure_cma_buffer_info {
	/*
	 * This needs to come first for compatibility with the secure buffer API
	 */
	struct ion_cp_buffer secure;
	void *cpu_addr;
	dma_addr_t handle;
	struct sg_table *table;
	bool is_cached;
};

static int cma_heap_has_outer_cache;
/*
 * Create scatter-list for the already allocated DMA buffer.
 * This function could be replace by dma_common_get_sgtable
 * as soon as it will avalaible.
 */
int ion_secure_cma_get_sgtable(struct device *dev, struct sg_table *sgt,
			void *cpu_addr, dma_addr_t handle, size_t size)
{
	struct page *page = virt_to_page(cpu_addr);
	int ret;

	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (unlikely(ret))
		return ret;

	sg_set_page(sgt->sgl, page, PAGE_ALIGN(size), 0);
	sg_dma_address(sgt->sgl) = handle;
	return 0;
}

/* ION CMA heap operations functions */
static struct ion_secure_cma_buffer_info *__ion_secure_cma_allocate(
			    struct ion_heap *heap, struct ion_buffer *buffer,
			    unsigned long len, unsigned long align,
			    unsigned long flags)
{
	struct device *dev = heap->priv;
	struct ion_secure_cma_buffer_info *info;

	dev_dbg(dev, "Request buffer allocation len %ld\n", len);

	info = kzalloc(sizeof(struct ion_secure_cma_buffer_info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "Can't allocate buffer info\n");
		return ION_CMA_ALLOCATE_FAILED;
	}

	if (!ION_IS_CACHED(flags))
		info->cpu_addr = dma_alloc_writecombine(dev, len,
					&(info->handle), 0);
	else
		info->cpu_addr = dma_alloc_nonconsistent(dev, len,
					&(info->handle), 0);

	if (!info->cpu_addr) {
		dev_err(dev, "Fail to allocate buffer\n");
		goto err;
	}

	info->table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!info->table) {
		dev_err(dev, "Fail to allocate sg table\n");
		goto err;
	}

	info->is_cached = ION_IS_CACHED(flags);

	ion_secure_cma_get_sgtable(dev,
			info->table, info->cpu_addr, info->handle, len);

	info->secure.buffer = info->handle;

	/* keep this for memory release */
	buffer->priv_virt = info;
	dev_dbg(dev, "Allocate buffer %p\n", buffer);
	return info;

err:
	kfree(info);
	return ION_CMA_ALLOCATE_FAILED;
}

static int ion_secure_cma_allocate(struct ion_heap *heap,
			    struct ion_buffer *buffer,
			    unsigned long len, unsigned long align,
			    unsigned long flags)
{
	unsigned long secure_allocation = flags & ION_SECURE;
	struct ion_secure_cma_buffer_info *buf = NULL;

	if (!secure_allocation) {
		pr_err("%s: non-secure allocation disallowed from heap %s %lx\n",
			__func__, heap->name, flags);
		return -ENOMEM;
	}

	buf = __ion_secure_cma_allocate(heap, buffer, len, align, flags);

	if (buf) {
		buf->secure.want_delayed_unsecure = 0;
		atomic_set(&buf->secure.secure_cnt, 0);
		mutex_init(&buf->secure.lock);
		buf->secure.is_secure = 1;
		return 0;
	} else {
		return -ENOMEM;
	}
}


static void ion_secure_cma_free(struct ion_buffer *buffer)
{
	struct device *dev = buffer->heap->priv;
	struct ion_secure_cma_buffer_info *info = buffer->priv_virt;

	dev_dbg(dev, "Release buffer %p\n", buffer);
	/* release memory */
	dma_free_coherent(dev, buffer->size, info->cpu_addr, info->handle);
	/* release sg table */
	kfree(info->table);
	kfree(info);
}

static int ion_secure_cma_phys(struct ion_heap *heap, struct ion_buffer *buffer,
			ion_phys_addr_t *addr, size_t *len)
{
	struct device *dev = heap->priv;
	struct ion_secure_cma_buffer_info *info = buffer->priv_virt;

	dev_dbg(dev, "Return buffer %p physical address 0x%x\n", buffer,
		info->handle);

	*addr = info->handle;
	*len = buffer->size;

	return 0;
}

struct sg_table *ion_secure_cma_heap_map_dma(struct ion_heap *heap,
					 struct ion_buffer *buffer)
{
	struct ion_secure_cma_buffer_info *info = buffer->priv_virt;

	return info->table;
}

void ion_secure_cma_heap_unmap_dma(struct ion_heap *heap,
			       struct ion_buffer *buffer)
{
	return;
}

static int ion_secure_cma_mmap(struct ion_heap *mapper,
			struct ion_buffer *buffer,
			struct vm_area_struct *vma)
{
	return -EINVAL;
}

static void *ion_secure_cma_map_kernel(struct ion_heap *heap,
				struct ion_buffer *buffer)
{
	struct ion_secure_cma_buffer_info *info = buffer->priv_virt;

	atomic_inc(&info->secure.map_cnt);
	return info->cpu_addr;
}

static void ion_secure_cma_unmap_kernel(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	struct ion_secure_cma_buffer_info *info = buffer->priv_virt;

	atomic_dec(&info->secure.map_cnt);
	return;
}

int ion_secure_cma_map_iommu(struct ion_buffer *buffer,
				struct ion_iommu_map *data,
				unsigned int domain_num,
				unsigned int partition_num,
				unsigned long align,
				unsigned long iova_length,
				unsigned long flags)
{
	int ret = 0;
	struct iommu_domain *domain;
	unsigned long extra;
	unsigned long extra_iova_addr;
	struct ion_secure_cma_buffer_info *info = buffer->priv_virt;
	struct sg_table *table = info->table;
	int prot = IOMMU_WRITE | IOMMU_READ;

	data->mapped_size = iova_length;

	if (!msm_use_iommu()) {
		data->iova_addr = info->handle;
		return 0;
	}

	extra = iova_length - buffer->size;

	ret = msm_allocate_iova_address(domain_num, partition_num,
						data->mapped_size, align,
						&data->iova_addr);

	if (ret)
		goto out;

	domain = msm_get_iommu_domain(domain_num);

	if (!domain) {
		ret = -EINVAL;
		goto out1;
	}

	ret = iommu_map_range(domain, data->iova_addr, table->sgl,
				buffer->size, prot);

	if (ret) {
		pr_err("%s: could not map %lx in domain %p\n",
			__func__, data->iova_addr, domain);
		goto out1;
	}

	extra_iova_addr = data->iova_addr + buffer->size;
	if (extra) {
		unsigned long phys_addr = sg_phys(table->sgl);
		ret = msm_iommu_map_extra(domain, extra_iova_addr, phys_addr,
					extra, SZ_4K, prot);
		if (ret)
			goto out2;
	}
	return ret;

out2:
	iommu_unmap_range(domain, data->iova_addr, buffer->size);
out1:
	msm_free_iova_address(data->iova_addr, domain_num, partition_num,
				data->mapped_size);
out:
	return ret;
}


void ion_secure_cma_unmap_iommu(struct ion_iommu_map *data)
{
	unsigned int domain_num;
	unsigned int partition_num;
	struct iommu_domain *domain;

	if (!msm_use_iommu())
		return;

	domain_num = iommu_map_domain(data);
	partition_num = iommu_map_partition(data);

	domain = msm_get_iommu_domain(domain_num);

	if (!domain) {
		WARN(1, "Could not get domain %d. Corruption?\n", domain_num);
		return;
	}

	iommu_unmap_range(domain, data->iova_addr, data->mapped_size);
	msm_free_iova_address(data->iova_addr, domain_num, partition_num,
				data->mapped_size);

	return;
}

int ion_secure_cma_cache_ops(struct ion_heap *heap,
			struct ion_buffer *buffer, void *vaddr,
			unsigned int offset, unsigned int length,
			unsigned int cmd)
{
	void (*outer_cache_op)(phys_addr_t, phys_addr_t);

	switch (cmd) {
	case ION_IOC_CLEAN_CACHES:
		dmac_clean_range(vaddr, vaddr + length);
		outer_cache_op = outer_clean_range;
		break;
	case ION_IOC_INV_CACHES:
		dmac_inv_range(vaddr, vaddr + length);
		outer_cache_op = outer_inv_range;
		break;
	case ION_IOC_CLEAN_INV_CACHES:
		dmac_flush_range(vaddr, vaddr + length);
		outer_cache_op = outer_flush_range;
		break;
	default:
		return -EINVAL;
	}

	if (cma_heap_has_outer_cache) {
		struct ion_secure_cma_buffer_info *info = buffer->priv_virt;

		outer_cache_op(info->handle, info->handle + length);
	}

	return 0;
}

static int ion_secure_cma_print_debug(struct ion_heap *heap, struct seq_file *s,
			const struct rb_root *mem_map)
{
	if (mem_map) {
		struct rb_node *n;

		seq_printf(s, "\nMemory Map\n");
		seq_printf(s, "%16.s %14.s %14.s %14.s\n",
			   "client", "start address", "end address",
			   "size (hex)");

		for (n = rb_first(mem_map); n; n = rb_next(n)) {
			struct mem_map_data *data =
					rb_entry(n, struct mem_map_data, node);
			const char *client_name = "(null)";


			if (data->client_name)
				client_name = data->client_name;

			seq_printf(s, "%16.s %14pa %14pa %14lu (%lx)\n",
				   client_name, &data->addr,
				   &data->addr_end,
				   data->size, data->size);
		}
	}
	return 0;
}

static struct ion_heap_ops ion_secure_cma_ops = {
	.allocate = ion_secure_cma_allocate,
	.free = ion_secure_cma_free,
	.map_dma = ion_secure_cma_heap_map_dma,
	.unmap_dma = ion_secure_cma_heap_unmap_dma,
	.phys = ion_secure_cma_phys,
	.map_user = ion_secure_cma_mmap,
	.map_kernel = ion_secure_cma_map_kernel,
	.unmap_kernel = ion_secure_cma_unmap_kernel,
	.map_iommu = ion_secure_cma_map_iommu,
	.unmap_iommu = ion_secure_cma_unmap_iommu,
	.cache_op = ion_secure_cma_cache_ops,
	.print_debug = ion_secure_cma_print_debug,
	.secure_buffer = ion_cp_secure_buffer,
	.unsecure_buffer = ion_cp_unsecure_buffer,
};

struct ion_heap *ion_secure_cma_heap_create(struct ion_platform_heap *data)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);

	if (!heap)
		return ERR_PTR(-ENOMEM);

	heap->ops = &ion_secure_cma_ops;
	/* set device as private heaps data, later it will be
	 * used to make the link with reserved CMA memory */
	heap->priv = data->priv;
	heap->type = ION_HEAP_TYPE_SECURE_DMA;
	cma_heap_has_outer_cache = data->has_outer_cache;
	return heap;
}

void ion_secure_cma_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}
