/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/export.h>
#include <linux/err.h>
#include <linux/msm_ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/rwsem.h>
#include <linux/uaccess.h>
#include <linux/memblock.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <asm/cacheflush.h>
#include "../ion_priv.h"
#include "ion_cp_common.h"
#include "compat_msm_ion.h"

#define ION_COMPAT_STR	"qcom,msm-ion"

static struct ion_device *idev;
static int num_heaps;
static struct ion_heap **heaps;

struct ion_heap_desc {
	unsigned int id;
	enum ion_heap_type type;
	const char *name;
	unsigned int permission_type;
};


#ifdef CONFIG_OF
static struct ion_heap_desc ion_heap_meta[] = {
	{
		.id	= ION_SYSTEM_HEAP_ID,
		.name	= ION_SYSTEM_HEAP_NAME,
	},
	{
		.id	= ION_SYSTEM_CONTIG_HEAP_ID,
		.name	= ION_KMALLOC_HEAP_NAME,
	},
	{
		.id	= ION_CP_MM_HEAP_ID,
		.name	= ION_MM_HEAP_NAME,
		.permission_type = IPT_TYPE_MM_CARVEOUT,
	},
	{
		.id	= ION_MM_FIRMWARE_HEAP_ID,
		.name	= ION_MM_FIRMWARE_HEAP_NAME,
	},
	{
		.id	= ION_CP_MFC_HEAP_ID,
		.name	= ION_MFC_HEAP_NAME,
		.permission_type = IPT_TYPE_MFC_SHAREDMEM,
	},
	{
		.id	= ION_SF_HEAP_ID,
		.name	= ION_SF_HEAP_NAME,
	},
	{
		.id	= ION_QSECOM_HEAP_ID,
		.name	= ION_QSECOM_HEAP_NAME,
	},
	{
		.id	= ION_AUDIO_HEAP_ID,
		.name	= ION_AUDIO_HEAP_NAME,
	},
	{
		.id	= ION_PIL1_HEAP_ID,
		.name	= ION_PIL1_HEAP_NAME,
	},
	{
		.id	= ION_PIL2_HEAP_ID,
		.name	= ION_PIL2_HEAP_NAME,
	},
	{
		.id	= ION_CP_WB_HEAP_ID,
		.name	= ION_WB_HEAP_NAME,
	},
	{
		.id	= ION_CAMERA_HEAP_ID,
		.name	= ION_CAMERA_HEAP_NAME,
	},
	{
		.id	= ION_ADSP_HEAP_ID,
		.name	= ION_ADSP_HEAP_NAME,
	}
};
#endif

struct ion_client *msm_ion_client_create(unsigned int heap_mask,
					const char *name)
{
	/*
	 * The assumption is that if there is a NULL device, the ion
	 * driver has not yet probed.
	 */
	if (idev == NULL)
		return ERR_PTR(-EPROBE_DEFER);

	if (IS_ERR(idev))
		return (struct ion_client *)idev;

	return ion_client_create(idev, name);
}
EXPORT_SYMBOL(msm_ion_client_create);

int msm_ion_do_cache_op(struct ion_client *client, struct ion_handle *handle,
			void *vaddr, unsigned long len, unsigned int cmd)
{
	return ion_do_cache_op(client, handle, vaddr, 0, len, cmd);
}
EXPORT_SYMBOL(msm_ion_do_cache_op);

static int ion_no_pages_cache_ops(struct ion_client *client,
			struct ion_handle *handle,
			void *vaddr,
			unsigned int offset, unsigned int length,
			unsigned int cmd)
{
	unsigned long size_to_vmap, total_size;
	int i, j, ret;
	void *ptr = NULL;
	ion_phys_addr_t buff_phys = 0;
	ion_phys_addr_t buff_phys_start = 0;
	size_t buf_length = 0;

	ret = ion_phys(client, handle, &buff_phys_start, &buf_length);
	if (ret)
		return -EINVAL;

	buff_phys = buff_phys_start;

	if (!vaddr) {
		/*
		 * Split the vmalloc space into smaller regions in
		 * order to clean and/or invalidate the cache.
		 */
		size_to_vmap = ((VMALLOC_END - VMALLOC_START)/8);
		total_size = buf_length;

		for (i = 0; i < total_size; i += size_to_vmap) {
			size_to_vmap = min(size_to_vmap, total_size - i);
			for (j = 0; j < 10 && size_to_vmap; ++j) {
				ptr = ioremap(buff_phys, size_to_vmap);
				if (ptr) {
					switch (cmd) {
					case ION_IOC_CLEAN_CACHES:
						dmac_clean_range(ptr,
							ptr + size_to_vmap);
						break;
					case ION_IOC_INV_CACHES:
						dmac_inv_range(ptr,
							ptr + size_to_vmap);
						break;
					case ION_IOC_CLEAN_INV_CACHES:
						dmac_flush_range(ptr,
							ptr + size_to_vmap);
						break;
					default:
						return -EINVAL;
					}
					buff_phys += size_to_vmap;
					break;
				} else {
					size_to_vmap >>= 1;
				}
			}
			if (!ptr) {
				pr_err("Couldn't io-remap the memory\n");
				return -EINVAL;
			}
			iounmap(ptr);
		}
	} else {
		switch (cmd) {
		case ION_IOC_CLEAN_CACHES:
			dmac_clean_range(vaddr, vaddr + length);
			break;
		case ION_IOC_INV_CACHES:
			dmac_inv_range(vaddr, vaddr + length);
			break;
		case ION_IOC_CLEAN_INV_CACHES:
			dmac_flush_range(vaddr, vaddr + length);
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

static int ion_pages_cache_ops(struct ion_client *client,
			struct ion_handle *handle,
			void *vaddr, unsigned int offset, unsigned int length,
			unsigned int cmd)
{
	struct sg_table *table = NULL;

	table = ion_sg_table(client, handle);
	if (IS_ERR_OR_NULL(table))
		return PTR_ERR(table);

	switch (cmd) {
	case ION_IOC_CLEAN_CACHES:
		if (!vaddr)
			dma_sync_sg_for_device(NULL, table->sgl,
				table->nents, DMA_TO_DEVICE);
		else
			dmac_clean_range(vaddr, vaddr + length);
		break;
	case ION_IOC_INV_CACHES:
		if (!vaddr)
			dma_sync_sg_for_cpu(NULL, table->sgl,
				table->nents, DMA_FROM_DEVICE);
		else
			dmac_inv_range(vaddr, vaddr + length);
		break;
	case ION_IOC_CLEAN_INV_CACHES:
		if (!vaddr) {
			dma_sync_sg_for_device(NULL, table->sgl,
				table->nents, DMA_TO_DEVICE);
			dma_sync_sg_for_cpu(NULL, table->sgl,
				table->nents, DMA_FROM_DEVICE);
		} else {
			dmac_flush_range(vaddr, vaddr + length);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int ion_do_cache_op(struct ion_client *client, struct ion_handle *handle,
			void *uaddr, unsigned long offset, unsigned long len,
			unsigned int cmd)
{
	int ret = -EINVAL;
	unsigned long flags;
	struct sg_table *table;
	struct page *page;

	ret = ion_handle_get_flags(client, handle, &flags);
	if (ret)
		return -EINVAL;

	if (!ION_IS_CACHED(flags))
		return 0;

	if (flags & ION_FLAG_SECURE)
		return 0;

	table = ion_sg_table(client, handle);

	if (IS_ERR_OR_NULL(table))
		return PTR_ERR(table);

	page = sg_page(table->sgl);

	if (page)
		ret = ion_pages_cache_ops(client, handle, uaddr,
					offset, len, cmd);
	else
		ret = ion_no_pages_cache_ops(client, handle, uaddr,
					offset, len, cmd);

	return ret;

}

static void msm_ion_allocate(struct ion_platform_heap *heap)
{

	if (!heap->base && heap->extra_data) {
		WARN(1, "Specifying carveout heaps without a base is deprecated. Convert to the DMA heap type instead");
		return;
	}
}

static int is_heap_overlapping(const struct ion_platform_heap *heap1,
				const struct ion_platform_heap *heap2)
{
	ion_phys_addr_t heap1_base = heap1->base;
	ion_phys_addr_t heap2_base = heap2->base;
	ion_phys_addr_t heap1_end = heap1->base + heap1->size - 1;
	ion_phys_addr_t heap2_end = heap2->base + heap2->size - 1;

	if (heap1_base == heap2_base)
		return 1;
	if (heap1_base < heap2_base && heap1_end >= heap2_base)
		return 1;
	if (heap2_base < heap1_base && heap2_end >= heap1_base)
		return 1;
	return 0;
}

static void check_for_heap_overlap(const struct ion_platform_heap heap_list[],
				   unsigned long nheaps)
{
	unsigned long i;
	unsigned long j;

	for (i = 0; i < nheaps; ++i) {
		const struct ion_platform_heap *heap1 = &heap_list[i];
		if (!heap1->base)
			continue;
		for (j = i + 1; j < nheaps; ++j) {
			const struct ion_platform_heap *heap2 = &heap_list[j];
			if (!heap2->base)
				continue;
			if (is_heap_overlapping(heap1, heap2)) {
				panic("Memory in heap %s overlaps with heap %s\n",
					heap1->name, heap2->name);
			}
		}
	}
}

#ifdef CONFIG_OF
static int msm_init_extra_data(struct device_node *node,
			       struct ion_platform_heap *heap,
			       const struct ion_heap_desc *heap_desc)
{
	int ret = 0;

	switch ((int) heap->type) {
	case ION_HEAP_TYPE_CARVEOUT:
	{
		heap->extra_data = kzalloc(sizeof(struct ion_co_heap_pdata),
					   GFP_KERNEL);
		if (!heap->extra_data)
			ret = -ENOMEM;
		break;
	}
	case ION_HEAP_TYPE_SECURE_DMA:
	{
		unsigned int val;

		ret = of_property_read_u32(node,
					"qcom,default-prefetch-size", &val);

		if (!ret) {
			heap->extra_data = kzalloc(sizeof(struct ion_cma_pdata),
					   GFP_KERNEL);

			if (!heap->extra_data) {
				ret = -ENOMEM;
			} else {
				struct ion_cma_pdata *extra = heap->extra_data;
				extra->default_prefetch_size = val;
			}
		} else {
			ret = 0;
		}
		break;
	}
	default:
		heap->extra_data = 0;
		break;
	}
	return ret;
}

#define MAKE_HEAP_TYPE_MAPPING(h) { .name = #h, \
			.heap_type = ION_HEAP_TYPE_##h, }

static struct heap_types_info {
	const char *name;
	int heap_type;
} heap_types_info[] = {
	MAKE_HEAP_TYPE_MAPPING(SYSTEM),
	MAKE_HEAP_TYPE_MAPPING(SYSTEM_CONTIG),
	MAKE_HEAP_TYPE_MAPPING(CARVEOUT),
	MAKE_HEAP_TYPE_MAPPING(CHUNK),
	MAKE_HEAP_TYPE_MAPPING(DMA),
	MAKE_HEAP_TYPE_MAPPING(SECURE_DMA),
	MAKE_HEAP_TYPE_MAPPING(REMOVED),
};

static int msm_ion_get_heap_type_from_dt_node(struct device_node *node,
					int *heap_type)
{
	const char *name;
	int i, ret = -EINVAL;
	ret = of_property_read_string(node, "qcom,ion-heap-type", &name);
	if (ret)
		goto out;
	for (i = 0; i < ARRAY_SIZE(heap_types_info); ++i) {
		if (!strcmp(heap_types_info[i].name, name)) {
			*heap_type = heap_types_info[i].heap_type;
			ret = 0;
			goto out;
		}
	}
	WARN(1, "Unknown heap type: %s. You might need to update heap_types_info in %s",
		name, __FILE__);
out:
	return ret;
}

static int msm_ion_populate_heap(struct device_node *node,
				struct ion_platform_heap *heap)
{
	unsigned int i;
	int ret = -EINVAL, heap_type = -1;
	unsigned int len = ARRAY_SIZE(ion_heap_meta);
	for (i = 0; i < len; ++i) {
		if (ion_heap_meta[i].id == heap->id) {
			heap->name = ion_heap_meta[i].name;
			ret = msm_ion_get_heap_type_from_dt_node(node,
								&heap_type);
			if (ret)
				break;
			heap->type = heap_type;
			ret = msm_init_extra_data(node, heap,
						&ion_heap_meta[i]);
			break;
		}
	}
	if (ret)
		pr_err("%s: Unable to populate heap, error: %d", __func__, ret);
	return ret;
}

static void free_pdata(const struct ion_platform_data *pdata)
{
	unsigned int i;
	for (i = 0; i < pdata->nr; ++i)
		kfree(pdata->heaps[i].extra_data);
	kfree(pdata->heaps);
	kfree(pdata);
}

static void msm_ion_get_heap_align(struct device_node *node,
				   struct ion_platform_heap *heap)
{
	unsigned int val;

	int ret = of_property_read_u32(node, "qcom,heap-align", &val);
	if (!ret) {
		switch ((int) heap->type) {
		case ION_HEAP_TYPE_CARVEOUT:
		{
			struct ion_co_heap_pdata *extra =
						heap->extra_data;
			extra->align = val;
			break;
		}
		default:
			pr_err("ION-heap %s: Cannot specify alignment for this type of heap\n",
					heap->name);
			break;
		}
	}
}

static int msm_ion_get_heap_size(struct device_node *node,
				 struct ion_platform_heap *heap)
{
	unsigned int val;
	int ret = 0;
	u32 out_values[2];
	struct device_node *pnode;

	ret = of_property_read_u32(node, "qcom,memory-reservation-size", &val);
	if (!ret)
		heap->size = val;

	ret = of_property_read_u32_array(node, "qcom,memory-fixed",
								out_values, 2);
	if (!ret) {
		heap->size = out_values[1];
		goto out;
	}

	pnode = of_parse_phandle(node, "linux,contiguous-region", 0);
	if (pnode != NULL) {
		const u32 *addr;
		u64 size;

		addr = of_get_address(pnode, 0, &size, NULL);
		if (!addr) {
			of_node_put(pnode);
			ret = -EINVAL;
			goto out;
		}
		heap->size = (u32) size;
		ret = 0;
		of_node_put(pnode);
	}

	ret = 0;
out:
	return ret;
}

static void msm_ion_get_heap_base(struct device_node *node,
				 struct ion_platform_heap *heap)
{
	u32 out_values[2];
	int ret = 0;
	struct device_node *pnode;

	ret = of_property_read_u32_array(node, "qcom,memory-fixed",
							out_values, 2);
	if (!ret)
		heap->base = out_values[0];

	pnode = of_parse_phandle(node, "linux,contiguous-region", 0);
	if (pnode != NULL) {
		heap->base = cma_get_base(heap->priv);
		of_node_put(pnode);
	}

	return;
}

static void msm_ion_get_heap_adjacent(struct device_node *node,
				      struct ion_platform_heap *heap)
{
	unsigned int val;
	int ret = of_property_read_u32(node, "qcom,heap-adjacent", &val);
	if (!ret) {
		switch (heap->type) {
		case ION_HEAP_TYPE_CARVEOUT:
		{
			struct ion_co_heap_pdata *extra = heap->extra_data;
			extra->adjacent_mem_id = val;
			break;
		}
		default:
			pr_err("ION-heap %s: Cannot specify adjcent mem id for this type of heap\n",
				heap->name);
			break;
		}
	} else {
		switch (heap->type) {
		case ION_HEAP_TYPE_CARVEOUT:
		{
			struct ion_co_heap_pdata *extra = heap->extra_data;
			extra->adjacent_mem_id = INVALID_HEAP_ID;
			break;
		}
		default:
			break;
		}
	}
}

static struct ion_platform_data *msm_ion_parse_dt(struct platform_device *pdev)
{
	struct ion_platform_data *pdata = 0;
	struct ion_platform_heap *heaps = NULL;
	struct device_node *node;
	struct platform_device *new_dev = NULL;
	const struct device_node *dt_node = pdev->dev.of_node;
	uint32_t val = 0;
	int ret = 0;
	uint32_t num_heaps = 0;
	int idx = 0;

	for_each_child_of_node(dt_node, node)
		num_heaps++;

	if (!num_heaps)
		return ERR_PTR(-EINVAL);

	pdata = kzalloc(sizeof(struct ion_platform_data), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	heaps = kzalloc(sizeof(struct ion_platform_heap)*num_heaps, GFP_KERNEL);
	if (!heaps) {
		kfree(pdata);
		return ERR_PTR(-ENOMEM);
	}

	pdata->heaps = heaps;
	pdata->nr = num_heaps;

	for_each_child_of_node(dt_node, node) {
		new_dev = of_platform_device_create(node, NULL, &pdev->dev);
		if (!new_dev) {
			pr_err("Failed to create device %s\n", node->name);
			goto free_heaps;
		}

		pdata->heaps[idx].priv = &new_dev->dev;
		/**
		 * TODO: Replace this with of_get_address() when this patch
		 * gets merged: http://
		 * permalink.gmane.org/gmane.linux.drivers.devicetree/18614
		*/
		ret = of_property_read_u32(node, "reg", &val);
		if (ret) {
			pr_err("%s: Unable to find reg key", __func__);
			goto free_heaps;
		}
		pdata->heaps[idx].id = val;

		ret = msm_ion_populate_heap(node, &pdata->heaps[idx]);
		if (ret)
			goto free_heaps;

		msm_ion_get_heap_base(node, &pdata->heaps[idx]);
		msm_ion_get_heap_align(node, &pdata->heaps[idx]);

		ret = msm_ion_get_heap_size(node, &pdata->heaps[idx]);
		if (ret)
			goto free_heaps;

		msm_ion_get_heap_adjacent(node, &pdata->heaps[idx]);

		++idx;
	}
	return pdata;

free_heaps:
	free_pdata(pdata);
	return ERR_PTR(ret);
}
#else
static struct ion_platform_data *msm_ion_parse_dt(struct platform_device *pdev)
{
	return NULL;
}

static void free_pdata(const struct ion_platform_data *pdata)
{

}
#endif

static int check_vaddr_bounds(unsigned long start, unsigned long end)
{
	struct mm_struct *mm = current->active_mm;
	struct vm_area_struct *vma;
	int ret = 1;

	if (end < start)
		goto out;

	vma = find_vma(mm, start);
	if (vma && vma->vm_start < end) {
		if (start < vma->vm_start)
			goto out;
		if (end > vma->vm_end)
			goto out;
		ret = 0;
	}

out:
	return ret;
}

int ion_heap_allow_secure_allocation(enum ion_heap_type type)
{
	return type == ((enum ion_heap_type) ION_HEAP_TYPE_SECURE_DMA);
}

int ion_heap_allow_handle_secure(enum ion_heap_type type)
{
	return type == ((enum ion_heap_type) ION_HEAP_TYPE_SECURE_DMA);
}

int ion_heap_allow_heap_secure(enum ion_heap_type type)
{
	return false;
}

/* fix up the cases where the ioctl direction bits are incorrect */
static unsigned int msm_ion_ioctl_dir(unsigned int cmd)
{
	switch (cmd) {
	case ION_IOC_CLEAN_CACHES:
	case ION_IOC_INV_CACHES:
	case ION_IOC_CLEAN_INV_CACHES:
	case ION_IOC_PREFETCH:
	case ION_IOC_DRAIN:
		return _IOC_WRITE;
	default:
		return _IOC_DIR(cmd);
	}
}

long msm_ion_custom_ioctl(struct ion_client *client,
				unsigned int cmd,
				unsigned long arg)
{
	unsigned int dir;
	union {
		struct ion_flush_data flush_data;
		struct ion_prefetch_data prefetch_data;
	} data;

	dir = msm_ion_ioctl_dir(cmd);

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	if (dir & _IOC_WRITE)
		if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;

	switch (cmd) {
	case ION_IOC_CLEAN_CACHES:
	case ION_IOC_INV_CACHES:
	case ION_IOC_CLEAN_INV_CACHES:
	{
		unsigned long start, end;
		struct ion_handle *handle = NULL;
		int ret;
		struct mm_struct *mm = current->active_mm;

		if (data.flush_data.handle > 0) {
			handle = ion_handle_get_by_id(client,
						(int)data.flush_data.handle);
			if (IS_ERR(handle)) {
				pr_info("%s: Could not find handle: %d\n",
					__func__, (int)data.flush_data.handle);
				return PTR_ERR(handle);
			}
		} else {
			handle = ion_import_dma_buf(client, data.flush_data.fd);
			if (IS_ERR(handle)) {
				pr_info("%s: Could not import handle: %p\n",
					__func__, handle);
				return -EINVAL;
			}
		}

		down_read(&mm->mmap_sem);

		start = (unsigned long) data.flush_data.vaddr;
		end = (unsigned long) data.flush_data.vaddr
			+ data.flush_data.length;

		if (start && check_vaddr_bounds(start, end)) {
			pr_err("%s: virtual address %p is out of bounds\n",
				__func__, data.flush_data.vaddr);
			ret = -EINVAL;
		} else {
			ret = ion_do_cache_op(
				client, handle, data.flush_data.vaddr,
				data.flush_data.offset,
				data.flush_data.length, cmd);
		}
		up_read(&mm->mmap_sem);

		ion_free(client, handle);

		if (ret < 0)
			return ret;
		break;
	}
	case ION_IOC_PREFETCH:
	{
		ion_walk_heaps(client, data.prefetch_data.heap_id,
			(void *)data.prefetch_data.len,
			ion_secure_cma_prefetch);
		break;
	}
	case ION_IOC_DRAIN:
	{
		ion_walk_heaps(client, data.prefetch_data.heap_id,
			(void *)data.prefetch_data.len,
			ion_secure_cma_drain_pool);
		break;
	}

	default:
		return -ENOTTY;
	}
	return 0;
}

static struct ion_heap *msm_ion_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_heap *heap = NULL;

	switch ((int)heap_data->type) {
#ifdef CONFIG_CMA
	case ION_HEAP_TYPE_DMA:
		heap = ion_cma_heap_create(heap_data);
		break;

	case ION_HEAP_TYPE_SECURE_DMA:
		heap = ion_secure_cma_heap_create(heap_data);
		break;
#endif
	case ION_HEAP_TYPE_REMOVED:
		heap = ion_removed_heap_create(heap_data);
		break;

	default:
		heap = ion_heap_create(heap_data);
	}

	if (IS_ERR_OR_NULL(heap)) {
		pr_err("%s: error creating heap %s type %d base %pa size %zu\n",
		       __func__, heap_data->name, heap_data->type,
		       &heap_data->base, heap_data->size);
		return ERR_PTR(-EINVAL);
	}

	heap->name = heap_data->name;
	heap->id = heap_data->id;
	heap->priv = heap_data->priv;
	return heap;
}

static void msm_ion_heap_destroy(struct ion_heap *heap)
{
	if (!heap)
		return;

	switch ((int)heap->type) {
#ifdef CONFIG_CMA
	case ION_HEAP_TYPE_DMA:
		ion_cma_heap_destroy(heap);
		break;
	case ION_HEAP_TYPE_SECURE_DMA:
		ion_secure_cma_heap_destroy(heap);
		break;
#endif
	case ION_HEAP_TYPE_REMOVED:
		ion_removed_heap_destroy(heap);
		break;
	default:
		ion_heap_destroy(heap);
	}
}

static int msm_ion_probe(struct platform_device *pdev)
{
	static struct ion_device *new_dev;
	struct ion_platform_data *pdata;
	unsigned int pdata_needs_to_be_freed;
	int err = -1;
	int i;
	if (pdev->dev.of_node) {
		pdata = msm_ion_parse_dt(pdev);
		if (IS_ERR(pdata)) {
			err = PTR_ERR(pdata);
			goto out;
		}
		pdata_needs_to_be_freed = 1;
	} else {
		pdata = pdev->dev.platform_data;
		pdata_needs_to_be_freed = 0;
	}

	num_heaps = pdata->nr;

	heaps = kcalloc(pdata->nr, sizeof(struct ion_heap *), GFP_KERNEL);

	if (!heaps) {
		err = -ENOMEM;
		goto out;
	}

	new_dev = ion_device_create(msm_ion_custom_ioctl, compat_msm_ion_ioctl);
	if (IS_ERR_OR_NULL(new_dev)) {
		/*
		 * set this to the ERR to indicate to the clients
		 * that Ion failed to probe.
		 */
		idev = new_dev;
		err = PTR_ERR(new_dev);
		goto freeheaps;
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];
		msm_ion_allocate(heap_data);

		heap_data->has_outer_cache = pdata->has_outer_cache;
		heaps[i] = msm_ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			heaps[i] = 0;
			continue;
		} else {
			if (heap_data->size)
				pr_info("ION heap %s created at %pa with size %zx\n",
							heap_data->name,
							  &heap_data->base,
							  heap_data->size);
			else
				pr_info("ION heap %s created\n",
							  heap_data->name);
		}

		ion_device_add_heap(new_dev, heaps[i]);
	}
	check_for_heap_overlap(pdata->heaps, num_heaps);
	if (pdata_needs_to_be_freed)
		free_pdata(pdata);

	platform_set_drvdata(pdev, new_dev);
	/*
	 * intentionally set this at the very end to allow probes to be deferred
	 * completely until Ion is setup
	 */
	idev = new_dev;
	return 0;

freeheaps:
	kfree(heaps);
	if (pdata_needs_to_be_freed)
		free_pdata(pdata);
out:
	return err;
}

static int msm_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < num_heaps; i++)
		msm_ion_heap_destroy(heaps[i]);

	ion_device_destroy(idev);
	kfree(heaps);
	return 0;
}

static struct of_device_id msm_ion_match_table[] = {
	{.compatible = ION_COMPAT_STR},
	{},
};

static struct platform_driver msm_ion_driver = {
	.probe = msm_ion_probe,
	.remove = msm_ion_remove,
	.driver = {
		.name = "ion-msm",
		.of_match_table = msm_ion_match_table,
	},
};

static int __init msm_ion_init(void)
{
	return platform_driver_register(&msm_ion_driver);
}

static void __exit msm_ion_exit(void)
{
	platform_driver_unregister(&msm_ion_driver);
}

subsys_initcall(msm_ion_init);
module_exit(msm_ion_exit);

