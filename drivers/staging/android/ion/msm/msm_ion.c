/* Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
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

#include <linux/err.h>
#include <linux/io.h>
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
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <linux/cma.h>
#include <linux/module.h>
#include <linux/show_mem_notifier.h>
#include <asm/cacheflush.h>
#include "../ion_priv.h"
#include "ion_cp_common.h"
#include "compat_msm_ion.h"
#include <soc/qcom/secure_buffer.h>

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
		.id	= ION_SECURE_HEAP_ID,
		.name	= ION_SECURE_HEAP_NAME,
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
		.id	= ION_SPSS_HEAP_ID,
		.name	= ION_SPSS_HEAP_NAME,
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
	},
	{
		.id	= ION_SECURE_DISPLAY_HEAP_ID,
		.name	= ION_SECURE_DISPLAY_HEAP_NAME,
	}
};
#endif

static int msm_ion_lowmem_notifier(struct notifier_block *nb,
					unsigned long action, void *data)
{
	show_ion_usage(idev);
	return 0;
}

static struct notifier_block msm_ion_nb = {
	.notifier_call = msm_ion_lowmem_notifier,
};

struct ion_client *msm_ion_client_create(const char *name)
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

int msm_ion_do_cache_offset_op(
		struct ion_client *client, struct ion_handle *handle,
		void *vaddr, unsigned int offset, unsigned long len,
		unsigned int cmd)
{
	return ion_do_cache_op(client, handle, vaddr, offset, len, cmd);
}
EXPORT_SYMBOL(msm_ion_do_cache_offset_op);

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

static void __do_cache_ops(struct page *page, unsigned int offset,
		unsigned int length, void (*op)(const void *, const void *))
{
	unsigned int left = length;
	unsigned long pfn;
	void *vaddr;

	pfn = page_to_pfn(page) + offset / PAGE_SIZE;
	page = pfn_to_page(pfn);
	offset &= ~PAGE_MASK;

	if (!PageHighMem(page)) {
		vaddr = page_address(page) + offset;
		op(vaddr, vaddr + length);
		goto out;
	}

	do {
		unsigned int len;

		len = left;
		if (len + offset > PAGE_SIZE)
			len = PAGE_SIZE - offset;

		page = pfn_to_page(pfn);
		vaddr = kmap_atomic(page);
		op(vaddr + offset, vaddr + offset + len);
		kunmap_atomic(vaddr);

		offset = 0;
		pfn++;
		left -= len;
	} while (left);

out:
	return;
}

static int ion_pages_cache_ops(struct ion_client *client,
			struct ion_handle *handle,
			void *vaddr, unsigned int offset, unsigned int length,
			unsigned int cmd)
{
	struct sg_table *table = NULL;
	struct scatterlist *sg;
	int i;
	unsigned int len = 0;
	void (*op)(const void *, const void *);


	table = ion_sg_table(client, handle);
	if (IS_ERR_OR_NULL(table))
		return PTR_ERR(table);

	switch (cmd) {
		case ION_IOC_CLEAN_CACHES:
			op = dmac_clean_range;
			break;
		case ION_IOC_INV_CACHES:
			op = dmac_inv_range;
			break;
		case ION_IOC_CLEAN_INV_CACHES:
			op = dmac_flush_range;
			break;
		default:
			return -EINVAL;
	};

	for_each_sg(table->sgl, sg, table->nents, i) {
		unsigned int sg_offset, sg_left, size = 0;

		len += sg->length;
		if (len <= offset)
			continue;

		sg_left = len - offset;
		sg_offset = sg->length - sg_left;

		size = (length < sg_left) ? length : sg_left;

		__do_cache_ops(sg_page(sg), sg_offset, size, op);

		offset += size;
		length -= size;

		if (length == 0)
			break;
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

	if (get_secure_vmid(flags) > 0)
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
	MAKE_HEAP_TYPE_MAPPING(SYSTEM_SECURE),
	MAKE_HEAP_TYPE_MAPPING(HYP_CMA),
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

static void msm_ion_get_heap_dt_data(struct device_node *node,
				 struct ion_platform_heap *heap)
{
	struct device_node *pnode;

	pnode = of_parse_phandle(node, "memory-region", 0);
	if (pnode != NULL) {
		const __be32 *basep;
		u64 size;
		u64 base;

		basep = of_get_address(pnode,  0, &size, NULL);
		if (!basep) {
			base = cma_get_base(dev_get_cma_area(heap->priv));
			size = cma_get_size(dev_get_cma_area(heap->priv));
		} else {
			base = of_translate_address(pnode, basep);
			WARN(base == OF_BAD_ADDR, "Failed to parse DT node for heap %s\n",
					heap->name);
		}
		heap->base = base;
		heap->size = size;
		of_node_put(pnode);
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

	for_each_available_child_of_node(dt_node, node)
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

	for_each_available_child_of_node(dt_node, node) {
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

		msm_ion_get_heap_dt_data(node, &pdata->heaps[idx]);

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

int ion_heap_is_system_secure_heap_type(enum ion_heap_type type)
{
	return type == ((enum ion_heap_type) ION_HEAP_TYPE_SYSTEM_SECURE);
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

bool is_secure_vmid_valid(int vmid)
{
	return (vmid == VMID_CP_TOUCH ||
		vmid == VMID_CP_BITSTREAM ||
		vmid == VMID_CP_PIXEL ||
		vmid == VMID_CP_NON_PIXEL ||
		vmid == VMID_CP_CAMERA ||
		vmid == VMID_CP_SEC_DISPLAY ||
		vmid == VMID_CP_APP ||
		vmid == VMID_CP_CAMERA_PREVIEW);
}

int get_secure_vmid(unsigned long flags)
{
	if (flags & ION_FLAG_CP_TOUCH)
		return VMID_CP_TOUCH;
	if (flags & ION_FLAG_CP_BITSTREAM)
		return VMID_CP_BITSTREAM;
	if (flags & ION_FLAG_CP_PIXEL)
		return VMID_CP_PIXEL;
	if (flags & ION_FLAG_CP_NON_PIXEL)
		return VMID_CP_NON_PIXEL;
	if (flags & ION_FLAG_CP_CAMERA)
		return VMID_CP_CAMERA;
	if (flags & ION_FLAG_CP_SEC_DISPLAY)
		return VMID_CP_SEC_DISPLAY;
	if (flags & ION_FLAG_CP_APP)
		return VMID_CP_APP;
	if (flags & ION_FLAG_CP_CAMERA_PREVIEW)
		return VMID_CP_CAMERA_PREVIEW;
	return -EINVAL;
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
				pr_info("%s: Could not import handle: %pK\n",
					__func__, handle);
				return -EINVAL;
			}
		}

		down_read(&mm->mmap_sem);

		start = (unsigned long)data.flush_data.vaddr +
			data.flush_data.offset;
		end = start + data.flush_data.length;

		if (check_vaddr_bounds(start, end)) {
			pr_err("%s: virtual address %pK is out of bounds\n",
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
		int ret;

		ret = ion_walk_heaps(client, data.prefetch_data.heap_id,
			ION_HEAP_TYPE_SECURE_DMA,
			(void *)data.prefetch_data.len,
			ion_secure_cma_prefetch);
		if (ret)
			return ret;

		ret = ion_walk_heaps(client, data.prefetch_data.heap_id,
				     ION_HEAP_TYPE_SYSTEM_SECURE,
				     (void *)&data.prefetch_data,
				     ion_system_secure_heap_prefetch);
		if (ret)
			return ret;
		break;
	}
	case ION_IOC_DRAIN:
	{
		int ret;

		ret = ion_walk_heaps(client, data.prefetch_data.heap_id,
			ION_HEAP_TYPE_SECURE_DMA,
			(void *)data.prefetch_data.len,
			ion_secure_cma_drain_pool);

		if (ret)
			return ret;

		ret = ion_walk_heaps(client, data.prefetch_data.heap_id,
				     ION_HEAP_TYPE_SYSTEM_SECURE,
				     (void *)&data.prefetch_data,
				     ion_system_secure_heap_drain);

		if (ret)
			return ret;
		break;
	}

	default:
		return -ENOTTY;
	}
	return 0;
}

#define MAX_VMAP_RETRIES 10

/**
 * An optimized page-zero'ing function. vmaps arrays of pages in large
 * chunks to minimize the number of memsets and vmaps/vunmaps.
 *
 * Note that the `pages' array should be composed of all 4K pages.
 *
 * NOTE: This function does not guarantee synchronization of the caches
 * and thus caller is responsible for handling any cache maintenance
 * operations needed.
 */
int msm_ion_heap_pages_zero(struct page **pages, int num_pages)
{
	int i, j, npages_to_vmap;
	void *ptr = NULL;

	/*
	 * As an optimization, we manually zero out all of the pages
	 * in one fell swoop here. To safeguard against insufficient
	 * vmalloc space, we only vmap `npages_to_vmap' at a time,
	 * starting with a conservative estimate of 1/8 of the total
	 * number of vmalloc pages available.
	 */
	npages_to_vmap = ((VMALLOC_END - VMALLOC_START)/8)
			>> PAGE_SHIFT;
	for (i = 0; i < num_pages; i += npages_to_vmap) {
		npages_to_vmap = min(npages_to_vmap, num_pages - i);
		for (j = 0; j < MAX_VMAP_RETRIES && npages_to_vmap;
			++j) {
			ptr = vmap(&pages[i], npages_to_vmap,
					VM_IOREMAP, PAGE_KERNEL);
			if (ptr)
				break;
			else
				npages_to_vmap >>= 1;
		}
		if (!ptr)
			return -ENOMEM;

		memset(ptr, 0, npages_to_vmap * PAGE_SIZE);
		vunmap(ptr);
	}

	return 0;
}

int msm_ion_heap_alloc_pages_mem(struct pages_mem *pages_mem)
{
	struct page **pages;
	unsigned int page_tbl_size;

	pages_mem->free_fn = kfree;
	page_tbl_size = sizeof(struct page *) * (pages_mem->size >> PAGE_SHIFT);
	if (page_tbl_size > SZ_8K) {
		/*
		 * Do fallback to ensure we have a balance between
		 * performance and availability.
		 */
		pages = kmalloc(page_tbl_size,
				__GFP_COMP | __GFP_NORETRY |
				__GFP_NOWARN);
		if (!pages) {
			pages = vmalloc(page_tbl_size);
			pages_mem->free_fn = vfree;
		}
	} else {
		pages = kmalloc(page_tbl_size, GFP_KERNEL);
	}

	if (!pages)
		return -ENOMEM;

	pages_mem->pages = pages;
	return 0;
}

void msm_ion_heap_free_pages_mem(struct pages_mem *pages_mem)
{
	pages_mem->free_fn(pages_mem->pages);
}

int msm_ion_heap_high_order_page_zero(struct device *dev, struct page *page,
				      int order)
{
	int i, ret;
	struct pages_mem pages_mem;
	int npages = 1 << order;
	pages_mem.size = npages * PAGE_SIZE;

	if (msm_ion_heap_alloc_pages_mem(&pages_mem))
		return -ENOMEM;

	for (i = 0; i < (1 << order); ++i)
		pages_mem.pages[i] = page + i;

	ret = msm_ion_heap_pages_zero(pages_mem.pages, npages);
	dma_sync_single_for_device(dev, page_to_phys(page), pages_mem.size,
				   DMA_BIDIRECTIONAL);
	msm_ion_heap_free_pages_mem(&pages_mem);
	return ret;
}

int msm_ion_heap_sg_table_zero(struct device *dev, struct sg_table *table,
			       size_t size)
{
	struct scatterlist *sg;
	int i, j, ret = 0, npages = 0;
	struct pages_mem pages_mem;

	pages_mem.size = PAGE_ALIGN(size);

	if (msm_ion_heap_alloc_pages_mem(&pages_mem))
		return -ENOMEM;

	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page = sg_page(sg);
		unsigned long len = sg->length;
		/* needed to make dma_sync_sg_for_device work: */
		sg->dma_address = sg_phys(sg);

		for (j = 0; j < len / PAGE_SIZE; j++)
			pages_mem.pages[npages++] = page + j;
	}

	ret = msm_ion_heap_pages_zero(pages_mem.pages, npages);
	dma_sync_sg_for_device(dev, table->sgl, table->nents,
			       DMA_BIDIRECTIONAL);
	msm_ion_heap_free_pages_mem(&pages_mem);
	return ret;
}

static struct ion_heap *msm_ion_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_heap *heap = NULL;

	switch ((int)heap_data->type) {
#ifdef CONFIG_CMA
	case ION_HEAP_TYPE_SECURE_DMA:
		heap = ion_secure_cma_heap_create(heap_data);
		break;
#endif
	case ION_HEAP_TYPE_SYSTEM_SECURE:
		heap = ion_system_secure_heap_create(heap_data);
		break;
	case ION_HEAP_TYPE_HYP_CMA:
		heap = ion_cma_secure_heap_create(heap_data);
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
	case ION_HEAP_TYPE_SECURE_DMA:
		ion_secure_cma_heap_destroy(heap);
		break;
#endif
	case ION_HEAP_TYPE_SYSTEM_SECURE:
		ion_system_secure_heap_destroy(heap);
		break;

	case ION_HEAP_TYPE_HYP_CMA:
		ion_cma_secure_heap_destroy(heap);
		break;
	default:
		ion_heap_destroy(heap);
	}
}

struct ion_heap *get_ion_heap(int heap_id)
{
	int i;
	struct ion_heap *heap;

	for (i = 0; i < num_heaps; i++) {
		heap = heaps[i];
		if (heap->id == heap_id)
			return heap;
	}

	pr_err("%s: heap_id %d not found\n", __func__, heap_id);
	return NULL;
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

	new_dev = ion_device_create(compat_msm_ion_ioctl);
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
	if (pdata_needs_to_be_freed)
		free_pdata(pdata);

	platform_set_drvdata(pdev, new_dev);
	/*
	 * intentionally set this at the very end to allow probes to be deferred
	 * completely until Ion is setup
	 */
	idev = new_dev;

	show_mem_notifier_register(&msm_ion_nb);
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
