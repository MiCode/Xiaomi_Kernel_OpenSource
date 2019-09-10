// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_address.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#include <linux/msm_ion_of.h>
#include "ion.h"
#include "msm_ion_priv.h"

#define ION_COMPAT_STR	"qcom,msm-ion"
#define ION_NOT_READY 0
#define ION_INIT_FAILURE 1
#define ION_READY 2

static int num_heaps;
static int status = ION_NOT_READY;
static struct ion_heap **heaps;

struct ion_heap_desc {
	unsigned int id;
	enum ion_heap_type type;
	const char *name;
};

#ifdef CONFIG_OF
static struct ion_heap_desc ion_heap_meta[] = {
	{
		.id	= ION_SYSTEM_HEAP_ID,
		.name	= ION_SYSTEM_HEAP_NAME,
	},
	{
		.id	= ION_SECURE_HEAP_ID,
		.name	= ION_SECURE_HEAP_NAME,
	},
	{
		.id	= ION_CP_MM_HEAP_ID,
		.name	= ION_MM_HEAP_NAME,
	},
	{
		.id	= ION_USER_CONTIG_HEAP_ID,
		.name	= ION_USER_CONTIG_HEAP_NAME,
	},
	{
		.id	= ION_QSECOM_HEAP_ID,
		.name	= ION_QSECOM_HEAP_NAME,
	},
	{
		.id	= ION_QSECOM_TA_HEAP_ID,
		.name	= ION_QSECOM_TA_HEAP_NAME,
	},
	{
		.id	= ION_SPSS_HEAP_ID,
		.name	= ION_SPSS_HEAP_NAME,
	},
	{
		.id	= ION_ADSP_HEAP_ID,
		.name	= ION_ADSP_HEAP_NAME,
	},
	{
		.id	= ION_SECURE_DISPLAY_HEAP_ID,
		.name	= ION_SECURE_DISPLAY_HEAP_NAME,
	},
	{
		.id     = ION_AUDIO_HEAP_ID,
		.name   = ION_AUDIO_HEAP_NAME,
	},
	{
		.id	= ION_SECURE_CARVEOUT_HEAP_ID,
		.name	= ION_SECURE_CARVEOUT_HEAP_NAME,
	}
};
#endif

#ifdef CONFIG_OF
#define MAKE_HEAP_TYPE_MAPPING(h) { .name = #h, \
			.heap_type = ION_HEAP_TYPE_##h, }

static struct heap_types_info {
	const char *name;
	int heap_type;
} heap_types_info[] = {
	MAKE_HEAP_TYPE_MAPPING(SYSTEM),
	MAKE_HEAP_TYPE_MAPPING(SYSTEM_CONTIG),
	MAKE_HEAP_TYPE_MAPPING(CARVEOUT),
	MAKE_HEAP_TYPE_MAPPING(SECURE_CARVEOUT),
	MAKE_HEAP_TYPE_MAPPING(DMA),
	MAKE_HEAP_TYPE_MAPPING(SYSTEM_SECURE),
	MAKE_HEAP_TYPE_MAPPING(HYP_CMA),
};

static struct ion_heap *ion_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_heap *heap = NULL;
	int heap_type = heap_data->type;

	switch (heap_type) {
	case ION_HEAP_TYPE_SYSTEM_CONTIG:
		pr_err("%s: Heap type is disabled: %d\n", __func__,
		       heap_data->type);
		break;
	case ION_HEAP_TYPE_SYSTEM:
		heap = ion_system_heap_create(heap_data);
		break;
	case ION_HEAP_TYPE_CARVEOUT:
		heap = ion_carveout_heap_create(heap_data);
		break;
#ifdef CONFIG_CMA
	case ION_HEAP_TYPE_DMA:
		heap = ion_cma_heap_create(heap_data);
		break;
	case (enum ion_heap_type)ION_HEAP_TYPE_HYP_CMA:
		heap = ion_cma_secure_heap_create(heap_data);
		break;
#endif
	case (enum ion_heap_type)ION_HEAP_TYPE_SYSTEM_SECURE:
		heap = ion_system_secure_heap_create(heap_data);
		break;
	case (enum ion_heap_type)ION_HEAP_TYPE_SECURE_CARVEOUT:
		heap = ion_secure_carveout_heap_create(heap_data);
		break;
	default:
		pr_err("%s: Invalid heap type %d\n", __func__,
		       heap_data->type);
		return ERR_PTR(-EINVAL);
	}

	if (IS_ERR_OR_NULL(heap)) {
		pr_err("%s: error creating heap %s type %d base %pa size %zu\n",
		       __func__, heap_data->name, heap_data->type,
		       &heap_data->base, heap_data->size);
		return ERR_PTR(-EINVAL);
	}

	heap->name = heap_data->name;
	heap->id = heap_data->id;
	return heap;
}

struct device *msm_ion_heap_device(struct ion_heap *heap)
{
	if (!heap)
		return ERR_PTR(-EINVAL);

	if (status == ION_NOT_READY)
		return ERR_PTR(-EPROBE_DEFER);
	else if (status == ION_INIT_FAILURE)
		return ERR_PTR(-ENODEV);

	return to_msm_ion_heap(heap)->dev;
}

struct device *msm_ion_heap_device_by_id(int heap_id)
{
	struct ion_heap *heap;

	if (status == ION_NOT_READY)
		return ERR_PTR(-EPROBE_DEFER);
	else if (status == ION_INIT_FAILURE)
		return ERR_PTR(-ENODEV);

	heap = get_ion_heap(heap_id);
	if (heap)
		return msm_ion_heap_device(heap);

	return ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL(msm_ion_heap_device_by_id);

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
			break;
		}
	}
	if (ret)
		pr_err("%s: Unable to populate heap, error: %d\n", __func__,
		       ret);
	return ret;
}

static void free_pdata(const struct ion_platform_data *pdata)
{
	kfree(pdata->heaps);
	kfree(pdata);
}

static int init_reserved_memory(struct device *dev, struct device_node *pnode)
{
	int ret = 0;
	struct reserved_mem *rmem = of_reserved_mem_lookup(pnode);

	if (!rmem) {
		dev_err(dev, "Failed to find reserved memory region\n");
		return -EINVAL;
	}

	/*
	 * We only need to call this when the memory-region is managed by
	 * a reserved memory region driver (e.g. CMA, coherent, etc). In that
	 * case, they will have ops for device specific initialization for
	 * the memory region. Otherwise, we have a pure carveout, which needs
	 * not be initialized.
	 */
	if (rmem->ops) {
		ret = of_reserved_mem_device_init_by_idx(dev, dev->of_node, 0);
		if (ret)
			dev_err(dev, "Failed to initialize memory region\n");
	}

	return ret;
}

static void release_reserved_memory(struct device *dev,
				    struct device_node *pnode)
{
	struct reserved_mem *rmem = of_reserved_mem_lookup(pnode);

	if (rmem && rmem->ops)
		of_reserved_mem_device_release(dev);
}

static void release_reserved_memory_regions(struct ion_platform_heap *heaps,
					    int idx)
{
	struct device *dev;
	struct device_node *node, *mem_node;

	for (idx = idx - 1; idx >= 0; idx--) {
		dev = heaps[idx].priv;
		node = dev->of_node;
		mem_node = of_parse_phandle(node, "memory-region", 0);

		if (mem_node)
			release_reserved_memory(dev, mem_node);
	}
}

static int msm_ion_get_heap_dt_data(struct device_node *node,
				    struct ion_platform_heap *heap)
{
	struct device_node *pnode;
	struct device *dev = heap->priv;
	int ret = -EINVAL;

	pnode = of_parse_phandle(node, "memory-region", 0);
	if (pnode) {
		const __be32 *basep;
		u64 size = 0;
		u64 base = 0;

		ret = init_reserved_memory(dev, pnode);
		if (ret)
			goto out;

		basep = of_get_address(pnode,  0, &size, NULL);
		if (!basep) {
			if (dev->cma_area) {
				base = cma_get_base(dev->cma_area);
				size = cma_get_size(dev->cma_area);
				ret = 0;
			} else if (dev->dma_mem) {
				base = dma_get_device_base(dev, dev->dma_mem);
				size = dma_get_size(dev->dma_mem);
				ret = 0;
			}
		} else {
			base = of_translate_address(pnode, basep);
			if (base != OF_BAD_ADDR)
				ret = 0;
		}

		if (!ret) {
			heap->base = base;
			heap->size = size;
		}
		of_node_put(pnode);
	} else {
		ret = 0;
	}

out:
	WARN(ret, "Failed to parse DT node for heap %s\n", heap->name);
	return ret;
}

static struct ion_platform_data *msm_ion_parse_dt(struct platform_device *pdev)
{
	struct ion_platform_data *pdata = 0;
	struct ion_platform_heap *heaps = NULL;
	struct device_node *node;
	struct platform_device *new_dev = NULL;
	const struct device_node *dt_node = pdev->dev.of_node;
	const __be32 *val;
	int ret = -EINVAL;
	u32 num_heaps = 0;
	int idx = 0;

	for_each_available_child_of_node(dt_node, node)
		num_heaps++;

	if (!num_heaps)
		return ERR_PTR(-EINVAL);

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	heaps = kcalloc(num_heaps, sizeof(struct ion_platform_heap),
			GFP_KERNEL);
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
		of_dma_configure(&new_dev->dev, node, true);

		pdata->heaps[idx].priv = &new_dev->dev;
		val = of_get_address(node, 0, NULL, NULL);
		if (!val) {
			pr_err("%s: Unable to find reg key\n", __func__);
			goto free_heaps;
		}
		pdata->heaps[idx].id = (u32)of_read_number(val, 1);

		ret = msm_ion_populate_heap(node, &pdata->heaps[idx]);
		if (ret)
			goto free_heaps;

		ret = msm_ion_get_heap_dt_data(node, &pdata->heaps[idx]);
		if (ret)
			goto free_heaps;

		++idx;
	}
	return pdata;

free_heaps:
	release_reserved_memory_regions(pdata->heaps, idx);
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

struct ion_heap *get_ion_heap(int heap_id)
{
	int i;
	struct ion_heap *heap;

	for (i = 0; i < num_heaps; i++) {
		heap = heaps[i];
		if (heap && heap->id == heap_id)
			return heap;
	}

	pr_err("%s: heap_id %d not found\n", __func__, heap_id);
	return NULL;
}

static int msm_ion_probe(struct platform_device *pdev)
{
	struct ion_platform_data *pdata;
	unsigned int pdata_needs_to_be_freed;
	int err = -1;
	int i;

	if (pdev->dev.of_node) {
		pdata = msm_ion_parse_dt(pdev);
		if (IS_ERR(pdata)) {
			status = ION_INIT_FAILURE;
			return PTR_ERR(pdata);
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

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];

		heaps[i] = ion_heap_create(heap_data);
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

		ion_device_add_heap(heaps[i]);
	}
	if (pdata_needs_to_be_freed)
		free_pdata(pdata);

	/*
	 * Publish the status at the end, so our interfaces know that they
	 * can stop returning -EPROBE_DEFER.
	 */
	status = ION_READY;

	return 0;

out:
	kfree(heaps);
	if (pdata_needs_to_be_freed)
		free_pdata(pdata);
	status = ION_INIT_FAILURE;
	return err;
}

static const struct of_device_id msm_ion_match_table[] = {
	{.compatible = ION_COMPAT_STR},
	{},
};

static struct platform_driver msm_ion_driver = {
	.probe = msm_ion_probe,
	.driver = {
		.name = "ion-msm",
		.of_match_table = msm_ion_match_table,
	},
};
module_platform_driver(msm_ion_driver);

MODULE_LICENSE("GPL v2");
