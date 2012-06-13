/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#include <linux/ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/memory_alloc.h>
#include <linux/fmem.h>
#include <mach/ion.h>
#include <mach/msm_memtypes.h>
#include "../ion_priv.h"
#include "ion_cp_common.h"

static struct ion_device *idev;
static int num_heaps;
static struct ion_heap **heaps;

struct ion_client *msm_ion_client_create(unsigned int heap_mask,
					const char *name)
{
	return ion_client_create(idev, heap_mask, name);
}
EXPORT_SYMBOL(msm_ion_client_create);

int msm_ion_secure_heap(int heap_id)
{
	return ion_secure_heap(idev, heap_id, ION_CP_V1, NULL);
}
EXPORT_SYMBOL(msm_ion_secure_heap);

int msm_ion_unsecure_heap(int heap_id)
{
	return ion_unsecure_heap(idev, heap_id, ION_CP_V1, NULL);
}
EXPORT_SYMBOL(msm_ion_unsecure_heap);

int msm_ion_secure_heap_2_0(int heap_id, enum cp_mem_usage usage)
{
	return ion_secure_heap(idev, heap_id, ION_CP_V2, (void *)usage);
}
EXPORT_SYMBOL(msm_ion_secure_heap_2_0);

int msm_ion_unsecure_heap_2_0(int heap_id, enum cp_mem_usage usage)
{
	return ion_unsecure_heap(idev, heap_id, ION_CP_V2, (void *)usage);
}
EXPORT_SYMBOL(msm_ion_unsecure_heap_2_0);

int msm_ion_do_cache_op(struct ion_client *client, struct ion_handle *handle,
			void *vaddr, unsigned long len, unsigned int cmd)
{
	return ion_do_cache_op(client, handle, vaddr, 0, len, cmd);
}
EXPORT_SYMBOL(msm_ion_do_cache_op);

static unsigned long msm_ion_get_base(unsigned long size, int memory_type,
				    unsigned int align)
{
	switch (memory_type) {
	case ION_EBI_TYPE:
		return allocate_contiguous_ebi_nomap(size, align);
		break;
	case ION_SMI_TYPE:
		return allocate_contiguous_memory_nomap(size, MEMTYPE_SMI,
							align);
		break;
	default:
		pr_err("%s: Unknown memory type %d\n", __func__, memory_type);
		return 0;
	}
}

static struct ion_platform_heap *find_heap(const struct ion_platform_heap
					   heap_data[],
					   unsigned int nr_heaps,
					   int heap_id)
{
	unsigned int i;
	for (i = 0; i < nr_heaps; ++i) {
		const struct ion_platform_heap *heap = &heap_data[i];
		if (heap->id == heap_id)
			return (struct ion_platform_heap *) heap;
	}
	return 0;
}

static void ion_set_base_address(struct ion_platform_heap *heap,
			    struct ion_platform_heap *shared_heap,
			    struct ion_co_heap_pdata *co_heap_data,
			    struct ion_cp_heap_pdata *cp_data)
{
	if (cp_data->reusable) {
		const struct fmem_data *fmem_info = fmem_get_info();

		if (!fmem_info) {
			pr_err("fmem info pointer NULL!\n");
			BUG();
		}

		heap->base = fmem_info->phys - fmem_info->reserved_size_low;
		cp_data->virt_addr = fmem_info->virt;
		pr_info("ION heap %s using FMEM\n", shared_heap->name);
	} else {
		heap->base = msm_ion_get_base(heap->size + shared_heap->size,
						shared_heap->memory_type,
						co_heap_data->align);
	}
	if (heap->base) {
		shared_heap->base = heap->base + heap->size;
		cp_data->secure_base = heap->base;
		cp_data->secure_size = heap->size + shared_heap->size;
	} else {
		pr_err("%s: could not get memory for heap %s (id %x)\n",
			__func__, heap->name, heap->id);
	}
}

static void allocate_co_memory(struct ion_platform_heap *heap,
			       struct ion_platform_heap heap_data[],
			       unsigned int nr_heaps)
{
	struct ion_co_heap_pdata *co_heap_data =
		(struct ion_co_heap_pdata *) heap->extra_data;

	if (co_heap_data->adjacent_mem_id != INVALID_HEAP_ID) {
		struct ion_platform_heap *shared_heap =
			find_heap(heap_data, nr_heaps,
				  co_heap_data->adjacent_mem_id);
		if (shared_heap) {
			struct ion_cp_heap_pdata *cp_data =
			   (struct ion_cp_heap_pdata *) shared_heap->extra_data;
			if (cp_data->fixed_position == FIXED_MIDDLE) {
				const struct fmem_data *fmem_info =
					fmem_get_info();

				if (!fmem_info) {
					pr_err("fmem info pointer NULL!\n");
					BUG();
				}

				cp_data->virt_addr = fmem_info->virt;
				if (!cp_data->secure_base) {
					cp_data->secure_base = heap->base;
					cp_data->secure_size =
						heap->size + shared_heap->size;
				}
			} else if (!heap->base) {
				ion_set_base_address(heap, shared_heap,
					co_heap_data, cp_data);
			}
		}
	}
}

/* Fixup heaps in board file to support two heaps being adjacent to each other.
 * A flag (adjacent_mem_id) in the platform data tells us that the heap phy
 * memory location must be adjacent to the specified heap. We do this by
 * carving out memory for both heaps and then splitting up the memory to the
 * two heaps. The heap specifying the "adjacent_mem_id" get the base of the
 * memory while heap specified in "adjacent_mem_id" get base+size as its
 * base address.
 * Note: Modifies platform data and allocates memory.
 */
static void msm_ion_heap_fixup(struct ion_platform_heap heap_data[],
			       unsigned int nr_heaps)
{
	unsigned int i;

	for (i = 0; i < nr_heaps; i++) {
		struct ion_platform_heap *heap = &heap_data[i];
		if (heap->type == ION_HEAP_TYPE_CARVEOUT) {
			if (heap->extra_data)
				allocate_co_memory(heap, heap_data, nr_heaps);
		}
	}
}

static void msm_ion_allocate(struct ion_platform_heap *heap)
{

	if (!heap->base && heap->extra_data) {
		unsigned int align = 0;
		switch (heap->type) {
		case ION_HEAP_TYPE_CARVEOUT:
			align =
			((struct ion_co_heap_pdata *) heap->extra_data)->align;
			break;
		case ION_HEAP_TYPE_CP:
		{
			struct ion_cp_heap_pdata *data =
				(struct ion_cp_heap_pdata *)
				heap->extra_data;
			if (data->reusable) {
				const struct fmem_data *fmem_info =
					fmem_get_info();
				heap->base = fmem_info->phys;
				data->virt_addr = fmem_info->virt;
				pr_info("ION heap %s using FMEM\n", heap->name);
			} else if (data->mem_is_fmem) {
				const struct fmem_data *fmem_info =
					fmem_get_info();
				heap->base = fmem_info->phys + fmem_info->size;
			}
			align = data->align;
			break;
		}
		default:
			break;
		}
		if (align && !heap->base) {
			heap->base = msm_ion_get_base(heap->size,
						      heap->memory_type,
						      align);
			if (!heap->base)
				pr_err("%s: could not get memory for heap %s "
				   "(id %x)\n", __func__, heap->name, heap->id);
		}
	}
}

static int is_heap_overlapping(const struct ion_platform_heap *heap1,
				const struct ion_platform_heap *heap2)
{
	unsigned long heap1_base = heap1->base;
	unsigned long heap2_base = heap2->base;
	unsigned long heap1_end = heap1->base + heap1->size - 1;
	unsigned long heap2_end = heap2->base + heap2->size - 1;

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

static int msm_ion_probe(struct platform_device *pdev)
{
	struct ion_platform_data *pdata = pdev->dev.platform_data;
	int err;
	int i;

	num_heaps = pdata->nr;

	heaps = kcalloc(pdata->nr, sizeof(struct ion_heap *), GFP_KERNEL);

	if (!heaps) {
		err = -ENOMEM;
		goto out;
	}

	idev = ion_device_create(NULL);
	if (IS_ERR_OR_NULL(idev)) {
		err = PTR_ERR(idev);
		goto freeheaps;
	}

	msm_ion_heap_fixup(pdata->heaps, num_heaps);

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];
		msm_ion_allocate(heap_data);

		heap_data->has_outer_cache = pdata->has_outer_cache;
		heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			heaps[i] = 0;
			continue;
		} else {
			if (heap_data->size)
				pr_info("ION heap %s created at %lx "
					"with size %x\n", heap_data->name,
							  heap_data->base,
							  heap_data->size);
			else
				pr_info("ION heap %s created\n",
							  heap_data->name);
		}

		ion_device_add_heap(idev, heaps[i]);
	}

	check_for_heap_overlap(pdata->heaps, num_heaps);
	platform_set_drvdata(pdev, idev);
	return 0;

freeheaps:
	kfree(heaps);
out:
	return err;
}

static int msm_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < num_heaps; i++)
		ion_heap_destroy(heaps[i]);

	ion_device_destroy(idev);
	kfree(heaps);
	return 0;
}

static struct platform_driver msm_ion_driver = {
	.probe = msm_ion_probe,
	.remove = msm_ion_remove,
	.driver = { .name = "ion-msm" }
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

