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

/*
 * Create scatter-list for the already allocated DMA buffer.
 * This function could be replace by dma_common_get_sgtable
 * as soon as it will avalaible.
 */
int ion_secure_cma_get_sgtable(struct device *dev, struct sg_table *sgt,
			void *cpu_addr, dma_addr_t handle, size_t size)
{
	struct page *page = phys_to_page(handle);
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
	DEFINE_DMA_ATTRS(attrs);
	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);

	dev_dbg(dev, "Request buffer allocation len %ld\n", len);

	info = kzalloc(sizeof(struct ion_secure_cma_buffer_info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "Can't allocate buffer info\n");
		return ION_CMA_ALLOCATE_FAILED;
	}

	info->cpu_addr = dma_alloc_attrs(dev, len, &(info->handle), GFP_KERNEL,
						&attrs);

	if (!info->cpu_addr) {
		dev_err(dev, "Fail to allocate buffer\n");
		goto err;
	}

	info->table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!info->table) {
		dev_err(dev, "Fail to allocate sg table\n");
		goto err;
	}

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
	unsigned long secure_allocation = flags & ION_FLAG_SECURE;
	struct ion_secure_cma_buffer_info *buf = NULL;

	if (!secure_allocation) {
		pr_err("%s: non-secure allocation disallowed from heap %s %lx\n",
			__func__, heap->name, flags);
		return -ENOMEM;
	}

	if (ION_IS_CACHED(flags)) {
		pr_err("%s: cannot allocate cached memory from secure heap %s\n",
			__func__, heap->name);
		return -ENOMEM;
	}


	buf = __ion_secure_cma_allocate(heap, buffer, len, align, flags);

	if (buf) {
		int ret;

		buf->secure.want_delayed_unsecure = 0;
		atomic_set(&buf->secure.secure_cnt, 0);
		mutex_init(&buf->secure.lock);
		buf->secure.is_secure = 1;
		buf->secure.ignore_check = true;

		/*
		 * make sure the size is set before trying to secure
		 */
		buffer->size = len;
		ret = ion_cp_secure_buffer(buffer, ION_CP_V2, 0, 0);
		if (ret) {
			/*
			 * Don't treat the secure buffer failing here as an
			 * error for backwards compatibility reasons. If
			 * the secure fails, the map will also fail so there
			 * is no security risk.
			 */
			pr_debug("%s: failed to secure buffer\n", __func__);
		}
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

	ion_cp_unsecure_buffer(buffer, 1);
	/* release memory */
	dma_free_coherent(dev, buffer->size, info->cpu_addr, info->handle);
	sg_free_table(info->table);
	/* release sg table */
	kfree(info->table);
	kfree(info);
}

static int ion_secure_cma_phys(struct ion_heap *heap, struct ion_buffer *buffer,
			ion_phys_addr_t *addr, size_t *len)
{
	struct device *dev = heap->priv;
	struct ion_secure_cma_buffer_info *info = buffer->priv_virt;

	dev_dbg(dev, "Return buffer %p physical address 0x%pa\n", buffer,
		&info->handle);

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
	pr_info("%s: mmaping from secure heap %s disallowed\n",
		__func__, mapper->name);
	return -EINVAL;
}

static void *ion_secure_cma_map_kernel(struct ion_heap *heap,
				struct ion_buffer *buffer)
{
	pr_info("%s: kernel mapping from secure heap %s disallowed\n",
		__func__, heap->name);
	return NULL;
}

static void ion_secure_cma_unmap_kernel(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	return;
}

static int ion_secure_cma_print_debug(struct ion_heap *heap, struct seq_file *s,
			const struct list_head *mem_map)
{
	if (mem_map) {
		struct mem_map_data *data;

		seq_printf(s, "\nMemory Map\n");
		seq_printf(s, "%16.s %14.s %14.s %14.s\n",
			   "client", "start address", "end address",
			   "size (hex)");

		list_for_each_entry(data, mem_map, node) {
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
	return heap;
}

void ion_secure_cma_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}
