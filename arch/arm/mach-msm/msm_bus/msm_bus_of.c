/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "AXI: %s(): " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <mach/msm_bus.h>

#define KBTOMB(a) (a * 1000ULL)
/**
 * msm_bus_cl_get_pdata() - Generate bus client data from device tree
 * provided by clients.
 *
 * of_node: Device tree node to extract information from
 *
 * The function returns a valid pointer to the allocated bus-scale-pdata
 * if the vectors were correctly read from the client's device node.
 * Any error in reading or parsing the device node will return NULL
 * to the caller.
 */
struct msm_bus_scale_pdata *msm_bus_cl_get_pdata(struct platform_device *pdev)
{
	struct device_node *of_node;
	struct msm_bus_scale_pdata *pdata = NULL;
	struct msm_bus_paths *usecase = NULL;
	int i = 0, j, ret, num_usecases = 0, num_paths, len;
	const uint32_t *vec_arr = NULL;
	bool mem_err = false;

	if (!pdev) {
		pr_err("Error: Null Platform device\n");
		return NULL;
	}

	of_node = pdev->dev.of_node;
	pdata = devm_kzalloc(&pdev->dev, sizeof(struct msm_bus_scale_pdata),
		GFP_KERNEL);
	if (!pdata) {
		pr_err("Error: Memory allocation for pdata failed\n");
		mem_err = true;
		goto err;
	}

	ret = of_property_read_string(of_node, "qcom,msm-bus,name",
		&pdata->name);
	if (ret) {
		pr_err("Error: Client name not found\n");
		goto err;
	}

	ret = of_property_read_u32(of_node, "qcom,msm-bus,num-cases",
		&num_usecases);
	if (ret) {
		pr_err("Error: num-usecases not found\n");
		goto err;
	}

	pdata->num_usecases = num_usecases;
	ret = of_property_read_u32(of_node, "qcom,msm-bus,active-only",
		&pdata->active_only);
	if (ret) {
		pr_info("active_only flag absent.\n");
		pr_info("Using dual context by default\n");
	}

	usecase = devm_kzalloc(&pdev->dev, (sizeof(struct msm_bus_paths) *
		pdata->num_usecases), GFP_KERNEL);
	if (!usecase) {
		pr_err("Error: Memory allocation for paths failed\n");
		mem_err = true;
		goto err;
	}

	ret = of_property_read_u32(of_node, "qcom,msm-bus,num-paths",
		&num_paths);
	if (ret) {
		pr_err("Error: num_paths not found\n");
		goto err;
	}

	vec_arr = of_get_property(of_node, "qcom,msm-bus,vectors-KBps", &len);
	if (len != num_usecases * num_paths * sizeof(uint32_t) * 4) {
		pr_err("Error: Length-error on getting vectors\n");
		goto err;
	}

	for (i = 0; i < num_usecases; i++) {
		usecase[i].num_paths = num_paths;
		usecase[i].vectors = devm_kzalloc(&pdev->dev, num_paths *
			sizeof(struct msm_bus_vectors), GFP_KERNEL);
		if (!usecase[i].vectors) {
			mem_err = true;
			pr_err("Error: Mem alloc failure in vectors\n");
			goto err;
		}

		for (j = 0; j < num_paths; j++) {
			int index = ((i * num_paths) + j) * 4;
			usecase[i].vectors[j].src = be32_to_cpu(vec_arr[index]);
			usecase[i].vectors[j].dst =
				be32_to_cpu(vec_arr[index + 1]);
			usecase[i].vectors[j].ab = (uint64_t)
				KBTOMB(be32_to_cpu(vec_arr[index + 2]));
			usecase[i].vectors[j].ib = (uint64_t)
				KBTOMB(be32_to_cpu(vec_arr[index + 3]));
		}
	}

	pdata->usecase = usecase;
	return pdata;
err:
	if (mem_err) {
		for (; i > 0; i--)
			kfree(usecase[i-1].vectors);

		kfree(usecase);
		kfree(pdata);
	}

	return NULL;
}
EXPORT_SYMBOL(msm_bus_cl_get_pdata);

/**
 * msm_bus_cl_clear_pdata() - Clear pdata allocated from device-tree
 * of_node: Device tree node to extract information from
 */
void msm_bus_cl_clear_pdata(struct msm_bus_scale_pdata *pdata)
{
	int i;

	for (i = 0; i < pdata->num_usecases; i++)
		kfree(pdata->usecase[i].vectors);

	kfree(pdata->usecase);
	kfree(pdata);
}
EXPORT_SYMBOL(msm_bus_cl_clear_pdata);
