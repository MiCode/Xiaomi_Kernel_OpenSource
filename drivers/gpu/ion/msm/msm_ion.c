/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/memory_alloc.h>
#include <mach/ion.h>
#include <mach/msm_memtypes.h>
#include "../ion_priv.h"

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
	return ion_secure_heap(idev, heap_id);
}
EXPORT_SYMBOL(msm_ion_secure_heap);

int msm_ion_unsecure_heap(int heap_id)
{
	return ion_unsecure_heap(idev, heap_id);
}
EXPORT_SYMBOL(msm_ion_unsecure_heap);

static unsigned long msm_ion_get_base(unsigned long size, int memory_type)
{
	switch (memory_type) {
	case ION_EBI_TYPE:
		return allocate_contiguous_ebi_nomap(size, PAGE_SIZE);
		break;
	case ION_SMI_TYPE:
		return allocate_contiguous_memory_nomap(size, MEMTYPE_SMI,
							PAGE_SIZE);
		break;
	default:
		return 0;
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

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];

		if (heap_data->type == ION_HEAP_TYPE_CARVEOUT) {
			heap_data->base = msm_ion_get_base(heap_data->size,
							heap_data->memory_type);
			if (!heap_data->base) {
				pr_err("%s: could not get memory for heap %s"
					" (id %x)\n", __func__, heap_data->name,
					heap_data->id);
				continue;
			}
		}

		heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			pr_err("%s: could not create ion heap %s"
				" (id %x)\n", __func__, heap_data->name,
				heap_data->id);
			heaps[i] = 0;
			continue;
		}

		ion_device_add_heap(idev, heaps[i]);
	}
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

