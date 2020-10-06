// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>

#include <linux/qcom_dma_heap.h>
#include "qcom_dma_heap_priv.h"

static int populate_heap(struct device_node *node,
				 struct platform_heap *heap)
{
	int ret;

	ret = of_property_read_string(node, "qcom,dma-heap-name", &heap->name);
	if (ret)
		goto out;

	ret = of_property_read_u32(node, "qcom,dma-heap-type", &heap->type);
	if (ret)
		goto out;
out:
	if (ret)
		pr_err("%s: Unable to populate heap, error: %d\n", __func__,
		       ret);
	return ret;
}

void free_pdata(const struct platform_data *pdata)
{
	kfree(pdata->heaps);
	kfree(pdata);
}

static int get_heap_dt_data(struct device_node *node,
			    struct platform_heap *heap)
{
	struct device_node *pnode;
	const __be32 *basep;
	u64 base, size;
	int ret = 0;

	pnode = of_parse_phandle(node, "memory-region", 0);
	if (pnode) {
		basep = of_get_address(pnode, 0, &size, NULL);
		if (basep) {
			base = of_translate_address(pnode, basep);
			if (base != OF_BAD_ADDR) {
				heap->base = base;
				heap->size = size;
			} else {
				ret = -EINVAL;
				dev_err(heap->dev,
					"Failed to get heap base/size\n");

			}
		}

		of_node_put(pnode);
	}

	return ret;
}

struct platform_data *parse_heap_dt(struct platform_device *pdev)
{
	struct platform_data *pdata = NULL;
	struct platform_heap *heaps = NULL;
	struct device_node *node;
	struct platform_device *new_dev = NULL;
	const struct device_node *dt_node = pdev->dev.of_node;
	int ret;
	u32 num_heaps = 0;
	int idx = 0;

	for_each_available_child_of_node(dt_node, node)
		num_heaps++;

	if (!num_heaps)
		return ERR_PTR(-EINVAL);

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	heaps = kcalloc(num_heaps, sizeof(struct platform_heap),
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
			ret = -EINVAL;
			goto free_heaps;
		}
		of_dma_configure(&new_dev->dev, node, true);

		pdata->heaps[idx].dev = &new_dev->dev;

		ret = populate_heap(node, &pdata->heaps[idx]);
		if (ret)
			goto free_heaps;

		ret = get_heap_dt_data(node, &pdata->heaps[idx]);
		if (ret)
			goto free_heaps;

		++idx;
	}
	return pdata;

free_heaps:
	of_node_put(node);
	free_pdata(pdata);
	return ERR_PTR(ret);
}
