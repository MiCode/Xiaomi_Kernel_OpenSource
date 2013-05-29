/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include "msm_bus_core.h"

#define KBTOB(a) (a * 1000ULL)
static const char * const hw_sel_name[] = {"RPM", "NoC", "BIMC", NULL};
static const char * const mode_sel_name[] = {"Fixed", "Limiter", "Bypass",
						"Regulator", NULL};

static int get_num(const char *const str[], const char *name)
{
	int i = 0;

	do {
		if (!strcmp(name, str[i]))
			return i;

		i++;
	} while (str[i] != NULL);

	pr_err("Error: string %s not found\n", name);
	return -EINVAL;
}

static struct msm_bus_scale_pdata *get_pdata(struct platform_device *pdev,
	struct device_node *of_node)
{
	struct msm_bus_scale_pdata *pdata = NULL;
	struct msm_bus_paths *usecase = NULL;
	int i = 0, j, ret, num_usecases = 0, num_paths, len;
	const uint32_t *vec_arr = NULL;
	bool mem_err = false;

	if (!pdev) {
		pr_err("Error: Null Platform device\n");
		return NULL;
	}

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

	if (of_property_read_bool(of_node, "qcom,msm-bus,active-only"))
		pdata->active_only = 1;
	else {
		pr_debug("active_only flag absent.\n");
		pr_debug("Using dual context by default\n");
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
	if (vec_arr == NULL) {
		pr_err("Error: Vector array not found\n");
		goto err;
	}

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
				KBTOB(be32_to_cpu(vec_arr[index + 2]));
			usecase[i].vectors[j].ib = (uint64_t)
				KBTOB(be32_to_cpu(vec_arr[index + 3]));
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

	if (!pdev) {
		pr_err("Error: Null Platform device\n");
		return NULL;
	}

	of_node = pdev->dev.of_node;
	pdata = get_pdata(pdev, of_node);
	if (!pdata) {
		pr_err("Error getting bus pdata!\n");
		return NULL;
	}

	return pdata;
}
EXPORT_SYMBOL(msm_bus_cl_get_pdata);

/**
 * msm_bus_cl_pdata_from_node() - Generate bus client data from device tree
 * node provided by clients. This function should be used when a client
 * driver needs to register multiple bus-clients from a single device-tree
 * node associated with the platform-device.
 *
 * of_node: The subnode containing information about the bus scaling
 * data
 *
 * pdev: Platform device associated with the device-tree node
 *
 * The function returns a valid pointer to the allocated bus-scale-pdata
 * if the vectors were correctly read from the client's device node.
 * Any error in reading or parsing the device node will return NULL
 * to the caller.
 */
struct msm_bus_scale_pdata *msm_bus_pdata_from_node(
		struct platform_device *pdev, struct device_node *of_node)
{
	struct msm_bus_scale_pdata *pdata = NULL;

	if (!pdev) {
		pr_err("Error: Null Platform device\n");
		return NULL;
	}

	if (!of_node) {
		pr_err("Error: Null of_node passed to bus driver\n");
		return NULL;
	}

	pdata = get_pdata(pdev, of_node);
	if (!pdata) {
		pr_err("Error getting bus pdata!\n");
		return NULL;
	}

	return pdata;
}
EXPORT_SYMBOL(msm_bus_pdata_from_node);

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

static int *get_arr(struct platform_device *pdev,
		const struct device_node *node, const char *prop,
		int *nports)
{
	int size = 0, ret;
	int *arr = NULL;

	if (of_get_property(node, prop, &size)) {
		*nports = size / sizeof(int);
	} else {
		pr_debug("Property %s not available\n", prop);
		*nports = 0;
		return NULL;
	}

	arr = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if ((size > 0) && ZERO_OR_NULL_PTR(arr)) {
		pr_err("Error: Failed to alloc mem for %s\n", prop);
		return NULL;
	}

	ret = of_property_read_u32_array(node, prop, (u32 *)arr, *nports);
	if (ret) {
		pr_err("Error in reading property: %s\n", prop);
		goto err;
	}

	return arr;
err:
	devm_kfree(&pdev->dev, arr);
	return NULL;
}

static struct msm_bus_node_info *get_nodes(struct device_node *of_node,
	struct platform_device *pdev,
	struct msm_bus_fabric_registration *pdata)
{
	struct msm_bus_node_info *info;
	struct device_node *child_node = NULL;
	int i = 0, ret;
	u32 temp;

	for_each_child_of_node(of_node, child_node) {
		i++;
	}

	pdata->len = i;
	info = (struct msm_bus_node_info *)
		devm_kzalloc(&pdev->dev, sizeof(struct msm_bus_node_info) *
			pdata->len, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(info)) {
		pr_err("Failed to alloc memory for nodes: %d\n", pdata->len);
		goto err;
	}

	i = 0;
	child_node = NULL;
	for_each_child_of_node(of_node, child_node) {
		const char *sel_str;

		ret = of_property_read_string(child_node, "label",
			&info[i].name);
		if (ret)
			pr_err("Error reading node label\n");

		ret = of_property_read_u32(child_node, "cell-id", &info[i].id);
		if (ret) {
			pr_err("Error reading node id\n");
			goto err;
		}

		if (of_property_read_bool(child_node, "qcom,gateway"))
			info[i].gateway = 1;

		of_property_read_u32(child_node, "qcom,mas-hw-id",
			&info[i].mas_hw_id);

		of_property_read_u32(child_node, "qcom,slv-hw-id",
			&info[i].slv_hw_id);
		info[i].masterp = get_arr(pdev, child_node,
					"qcom,masterp", &info[i].num_mports);
		/* No need to store number of qports */
		info[i].qport = get_arr(pdev, child_node,
					"qcom,qport", &ret);
		pdata->nmasters += info[i].num_mports;


		info[i].slavep = get_arr(pdev, child_node,
					"qcom,slavep", &info[i].num_sports);
		pdata->nslaves += info[i].num_sports;


		info[i].tier = get_arr(pdev, child_node,
					"qcom,tier", &info[i].num_tiers);

		if (of_property_read_bool(child_node, "qcom,ahb"))
			info[i].ahb = 1;

		ret = of_property_read_string(child_node, "qcom,hw-sel",
			&sel_str);
		if (ret)
			info[i].hw_sel = 0;
		else {
			ret =  get_num(hw_sel_name, sel_str);
			if (ret < 0) {
				pr_err("Invalid hw-sel\n");
				goto err;
			}

			info[i].hw_sel = ret;
		}

		of_property_read_u32(child_node, "qcom,buswidth",
			&info[i].buswidth);
		of_property_read_u32(child_node, "qcom,ws", &info[i].ws);
		ret = of_property_read_u32(child_node, "qcom,thresh",
			&temp);
		if (!ret)
			info[i].th = (uint64_t)KBTOB(temp);

		ret = of_property_read_u32(child_node, "qcom,bimc,bw",
			&temp);
		if (!ret)
			info[i].bimc_bw = (uint64_t)KBTOB(temp);

		of_property_read_u32(child_node, "qcom,bimc,gp",
			&info[i].bimc_gp);
		of_property_read_u32(child_node, "qcom,bimc,thmp",
			&info[i].bimc_thmp);
		ret = of_property_read_string(child_node, "qcom,mode",
			&sel_str);
		if (ret)
			info[i].mode = 0;
		else {
			ret = get_num(mode_sel_name, sel_str);
			if (ret < 0) {
				pr_err("Unknown mode :%s\n", sel_str);
				goto err;
			}

			info[i].mode = ret;
		}

		info[i].dual_conf =
			of_property_read_bool(child_node, "qcom,dual-conf");

		ret = of_property_read_string(child_node, "qcom,mode-thresh",
			&sel_str);
		if (ret)
			info[i].mode_thresh = 0;
		else {
			ret = get_num(mode_sel_name, sel_str);
			if (ret < 0) {
				pr_err("Unknown mode :%s\n", sel_str);
				goto err;
			}

			info[i].mode_thresh = ret;
			MSM_BUS_DBG("AXI: THreshold mode set: %d\n",
				info[i].mode_thresh);
		}

		ret = of_property_read_string(child_node, "qcom,perm-mode",
			&sel_str);
		if (ret)
			info[i].perm_mode = 0;
		else {
			ret = get_num(mode_sel_name, sel_str);
			if (ret < 0)
				goto err;

			info[i].perm_mode = 1 << ret;
		}

		of_property_read_u32(child_node, "qcom,prio-lvl",
			&info[i].prio_lvl);
		of_property_read_u32(child_node, "qcom,prio-rd",
			&info[i].prio_rd);
		of_property_read_u32(child_node, "qcom,prio-wr",
			&info[i].prio_wr);
		of_property_read_u32(child_node, "qcom,prio0", &info[i].prio0);
		of_property_read_u32(child_node, "qcom,prio1", &info[i].prio1);
		ret = of_property_read_string(child_node, "qcom,slaveclk-dual",
			&info[i].slaveclk[DUAL_CTX]);
		if (!ret)
			pr_debug("Got slaveclk_dual: %s\n",
				info[i].slaveclk[DUAL_CTX]);
		else
			info[i].slaveclk[DUAL_CTX] = NULL;

		ret = of_property_read_string(child_node,
			"qcom,slaveclk-active", &info[i].slaveclk[ACTIVE_CTX]);
		if (!ret)
			pr_debug("Got slaveclk_active\n");
		else
			info[i].slaveclk[ACTIVE_CTX] = NULL;

		ret = of_property_read_string(child_node, "qcom,memclk-dual",
			&info[i].memclk[DUAL_CTX]);
		if (!ret)
			pr_debug("Got memclk_dual\n");
		else
			info[i].memclk[DUAL_CTX] = NULL;

		ret = of_property_read_string(child_node, "qcom,memclk-active",
			&info[i].memclk[ACTIVE_CTX]);
		if (!ret)
			pr_debug("Got memclk_active\n");
		else
			info[i].memclk[ACTIVE_CTX] = NULL;

		ret = of_property_read_string(child_node, "qcom,iface-clk-node",
			&info[i].iface_clk_node);
		if (!ret)
			pr_debug("Got iface_clk_node\n");
		else
			info[i].iface_clk_node = NULL;

		pr_debug("Node name: %s\n", info[i].name);
		of_node_put(child_node);
		i++;
	}

	pr_debug("Bus %d added: %d masters\n", pdata->id, pdata->nmasters);
	pr_debug("Bus %d added: %d slaves\n", pdata->id, pdata->nslaves);
	return info;
err:
	return NULL;
}

struct msm_bus_fabric_registration
	*msm_bus_of_get_fab_data(struct platform_device *pdev)
{
	struct device_node *of_node;
	struct msm_bus_fabric_registration *pdata;
	bool mem_err = false;
	int ret = 0;
	const char *sel_str;

	if (!pdev) {
		pr_err("Error: Null platform device\n");
		return NULL;
	}

	of_node = pdev->dev.of_node;
	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_bus_fabric_registration), GFP_KERNEL);
	if (!pdata) {
		pr_err("Error: Memory allocation for pdata failed\n");
		mem_err = true;
		goto err;
	}

	ret = of_property_read_string(of_node, "label", &pdata->name);
	if (ret) {
		pr_err("Error: label not found\n");
		goto err;
	}
	pr_debug("Fab_of: Read name: %s\n", pdata->name);

	ret = of_property_read_u32(of_node, "cell-id",
		&pdata->id);
	if (ret) {
		pr_err("Error: num-usecases not found\n");
		goto err;
	}
	pr_debug("Fab_of: Read id: %u\n", pdata->id);

	if (of_property_read_bool(of_node, "qcom,ahb"))
		pdata->ahb = 1;

	ret = of_property_read_string(of_node, "qcom,fabclk-dual",
		&pdata->fabclk[DUAL_CTX]);
	if (ret) {
		pr_debug("fabclk_dual not available\n");
		pdata->fabclk[DUAL_CTX] = NULL;
	} else
		pr_debug("Fab_of: Read clk dual ctx: %s\n",
			pdata->fabclk[DUAL_CTX]);
	ret = of_property_read_string(of_node, "qcom,fabclk-active",
		&pdata->fabclk[ACTIVE_CTX]);
	if (ret) {
		pr_debug("Error: fabclk_active not available\n");
		pdata->fabclk[ACTIVE_CTX] = NULL;
	} else
		pr_debug("Fab_of: Read clk act ctx: %s\n",
			pdata->fabclk[ACTIVE_CTX]);

	ret = of_property_read_u32(of_node, "qcom,ntieredslaves",
		&pdata->ntieredslaves);
	if (ret) {
		pr_err("Error: ntieredslaves not found\n");
		goto err;
	}

	ret = of_property_read_u32(of_node, "qcom,qos-freq", &pdata->qos_freq);
	if (ret)
		pr_debug("qos_freq not available\n");

	ret = of_property_read_string(of_node, "qcom,hw-sel", &sel_str);
	if (ret) {
		pr_err("Error: hw_sel not found\n");
		goto err;
	} else {
		ret = get_num(hw_sel_name, sel_str);
		if (ret < 0)
			goto err;

		pdata->hw_sel = ret;
	}

	if (of_property_read_bool(of_node, "qcom,virt"))
		pdata->virt = true;

	if (of_property_read_bool(of_node, "qcom,rpm-en"))
		pdata->rpm_enabled = 1;

	pdata->info = get_nodes(of_node, pdev, pdata);
	return pdata;
err:
	return NULL;
}
EXPORT_SYMBOL(msm_bus_of_get_fab_data);
