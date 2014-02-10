/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include "msm_bus_core.h"
#include "msm_bus_adhoc.h"

static int get_qos_mode(struct platform_device *pdev,
			struct device_node *node, const char *qos_mode)
{
	const char *qos_names[] = {"Fixed", "Limiter", "Bypass", "Regulator"};
	int i = 0;
	int ret = 0;

	if (!qos_mode) {
		ret = 2;
		goto exit_get_qos_mode;
	}

	for (i = 0; i < ARRAY_SIZE(qos_names); i++) {
		if (!strcmp(qos_mode, qos_names[i]))
			break;
	}
	if (i == ARRAY_SIZE(qos_names)) {
		ret = -1;
		dev_dbg(&pdev->dev, "Cannot match mode qos %s using Bypass",
				qos_mode);
	} else
		ret = i;

exit_get_qos_mode:
	return ret;
}

static int *get_arr(struct platform_device *pdev,
		struct device_node *node, const char *prop,
		int *nports)
{
	int size = 0, ret;
	int *arr = NULL;

	if (of_get_property(node, prop, &size)) {
		*nports = size / sizeof(int);
	} else {
		dev_dbg(&pdev->dev, "Property %s not available\n", prop);
		*nports = 0;
		return NULL;
	}

	arr = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if ((size > 0) && ZERO_OR_NULL_PTR(arr)) {
		dev_err(&pdev->dev, "Error: Failed to alloc mem for %s\n",
				prop);
		return NULL;
	}

	ret = of_property_read_u32_array(node, prop, (u32 *)arr, *nports);
	if (ret) {
		dev_err(&pdev->dev, "Error in reading property: %s\n", prop);
		goto err;
	}

	return arr;
err:
	devm_kfree(&pdev->dev, arr);
	return NULL;
}

static struct msm_bus_fab_device_type *get_fab_device_info(
		struct device_node *dev_node,
		struct platform_device *pdev)
{
	struct msm_bus_fab_device_type *fab_dev;
	unsigned int ret;
	struct resource *res;
	const char *base_name;

	fab_dev = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_bus_fab_device_type),
			GFP_KERNEL);
	if (!fab_dev) {
		dev_err(&pdev->dev,
			"Error: Unable to allocate memory for fab_dev\n");
		return NULL;
	}

	ret = of_property_read_string(dev_node, "qcom,base-name", &base_name);
	if (ret) {
		dev_err(&pdev->dev, "Error: Unable to get base address name\n");
		goto err;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, base_name);
	if (!res) {
		dev_err(&pdev->dev, "Error getting qos base addr %s\n",
								base_name);
		goto err;
	}
	fab_dev->pqos_base = res->start;
	fab_dev->qos_range = resource_size(res);
	fab_dev->bypass_qos_prg = of_property_read_bool(dev_node,
						"qcom,bypass-qos-prg");

	ret = of_property_read_u32(dev_node, "qcom,base-offset",
			&fab_dev->base_offset);
	if (ret)
		dev_dbg(&pdev->dev, "Bus base offset is missing\n");

	ret = of_property_read_u32(dev_node, "qcom,qos-off",
			&fab_dev->qos_off);
	if (ret)
		dev_dbg(&pdev->dev, "Bus qos off is missing\n");


	ret = of_property_read_u32(dev_node, "qcom,bus-type",
						&fab_dev->bus_type);
	if (ret) {
		dev_warn(&pdev->dev, "Bus type is missing\n");
		goto err;
	}

	return fab_dev;

err:
	devm_kfree(&pdev->dev, fab_dev);
	fab_dev = 0;
	return NULL;
}

static struct msm_bus_node_info_type *get_node_info_data(
		struct device_node * const dev_node,
		struct platform_device * const pdev)
{
	struct msm_bus_node_info_type *node_info;
	unsigned int ret;
	int size;
	int i;
	struct device_node *con_node;
	struct device_node *bus_dev;
	const char *qos_mode = NULL;

	node_info = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_bus_node_info_type),
			GFP_KERNEL);
	if (!node_info) {
		dev_err(&pdev->dev,
			"Error: Unable to allocate memory for node_info\n");
		return NULL;
	}

	ret = of_property_read_u32(dev_node, "cell-id", &node_info->id);
	if (ret) {
		dev_warn(&pdev->dev, "Bus node is missing cell-id\n");
		goto err;
	}
	ret = of_property_read_string(dev_node, "label", &node_info->name);
	if (ret) {
		dev_warn(&pdev->dev, "Bus node is missing name\n");
		goto err;
	}
	node_info->qport = get_arr(pdev, dev_node, "qcom,qport",
			&node_info->num_qports);

	if (of_get_property(dev_node, "qcom,connections", &size)) {
		node_info->num_connections = size / sizeof(int);
		node_info->connections = devm_kzalloc(&pdev->dev, size,
				GFP_KERNEL);
	} else {
		node_info->num_connections = 0;
		node_info->connections = 0;
	}

	for (i = 0; i < node_info->num_connections; i++) {
		con_node = of_parse_phandle(dev_node, "qcom,connections", i);
		if (IS_ERR_OR_NULL(con_node))
			goto err;

		if (of_property_read_u32(con_node, "cell-id",
				&node_info->connections[i]))
			goto err;
		of_node_put(con_node);
	}

	bus_dev = of_parse_phandle(dev_node, "qcom,bus-dev", 0);
	if (!IS_ERR_OR_NULL(bus_dev)) {
		if (of_property_read_u32(bus_dev, "cell-id",
			&node_info->bus_device_id)) {
			dev_err(&pdev->dev, "Can't find bus device. Node %d",
					node_info->id);
			goto err;
		}

		of_node_put(bus_dev);
	} else
		dev_dbg(&pdev->dev, "Can't find bdev phandle for %d",
					node_info->id);

	node_info->is_fab_dev = of_property_read_bool(dev_node, "qcom,fab-dev");
	node_info->virt_dev = of_property_read_bool(dev_node, "qcom,virt-dev");

	ret = of_property_read_u32(dev_node, "qcom,buswidth",
						&node_info->buswidth);
	if (ret) {
		dev_dbg(&pdev->dev, "Using default 8 bytes %d", node_info->id);
		node_info->buswidth = 8;
	}

	ret = of_property_read_u32(dev_node, "qcom,mas-rpm-id",
						&node_info->mas_rpm_id);
	if (ret) {
		dev_dbg(&pdev->dev, "mas rpm id is missing\n");
		node_info->mas_rpm_id = -1;
	}

	ret = of_property_read_u32(dev_node, "qcom,slv-rpm-id",
						&node_info->slv_rpm_id);
	if (ret) {
		dev_dbg(&pdev->dev, "slv rpm id is missing\n");
		node_info->slv_rpm_id = -1;
	}

	ret = of_property_read_string(dev_node, "qcom,qos-mode", &qos_mode);
	node_info->mode = get_qos_mode(pdev, dev_node, qos_mode);

	ret = of_property_read_u32(dev_node, "qcom,prio-lvl",
						&node_info->slv_rpm_id);
	ret = of_property_read_u32(dev_node, "qcom,prio1", &node_info->prio1);
	ret = of_property_read_u32(dev_node, "qcom,prio0", &node_info->prio0);

	return node_info;

err:
	devm_kfree(&pdev->dev, node_info);
	node_info = 0;
	return NULL;
}

static unsigned int get_bus_node_device_data(
		struct device_node * const dev_node,
		struct platform_device * const pdev,
		struct msm_bus_node_device_type * const node_device)
{
	node_device->node_info = get_node_info_data(dev_node, pdev);
	if (IS_ERR_OR_NULL(node_device->node_info)) {
		dev_err(&pdev->dev, "Error: Node info missing\n");
		return -ENODATA;
	}
	node_device->ap_owned = of_property_read_bool(dev_node,
							"qcom,ap-owned");

	if (node_device->node_info->is_fab_dev) {
		dev_err(&pdev->dev, "Dev %d\n", node_device->node_info->id);

		if (!node_device->node_info->virt_dev) {
			node_device->fabdev =
				get_fab_device_info(dev_node, pdev);
			if (IS_ERR_OR_NULL(node_device->fabdev)) {
				dev_err(&pdev->dev,
					"Error: Fabric device info missing\n");
				devm_kfree(&pdev->dev, node_device->node_info);
				return -ENODATA;
			}
		}
		node_device->clk[DUAL_CTX].clk = of_clk_get_by_name(dev_node,
							"bus_clk");

		if (IS_ERR_OR_NULL(node_device->clk[DUAL_CTX].clk))
			dev_err(&pdev->dev,
				"%s:Failed to get bus clk for bus%d ctx%d",
				__func__, node_device->node_info->id,
								DUAL_CTX);

		node_device->clk[ACTIVE_CTX].clk = of_clk_get_by_name(dev_node,
							"bus_a_clk");
		if (IS_ERR_OR_NULL(node_device->clk[ACTIVE_CTX].clk))
			dev_err(&pdev->dev,
				"Failed to get bus clk for bus%d ctx%d",
				 node_device->node_info->id, ACTIVE_CTX);
	}
	return 0;
}

struct msm_bus_device_node_registration
	*msm_bus_of_to_pdata(struct platform_device *pdev)
{
	struct device_node *of_node, *child_node;
	struct msm_bus_device_node_registration *pdata;
	unsigned int i = 0, j;
	unsigned int ret;

	if (!pdev) {
		pr_err("Error: Null platform device\n");
		return NULL;
	}

	of_node = pdev->dev.of_node;

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_bus_device_node_registration),
			GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev,
				"Error: Memory allocation for pdata failed\n");
		return NULL;
	}

	pdata->num_devices = of_get_child_count(of_node);

	pdata->info = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_bus_node_device_type) *
			pdata->num_devices, GFP_KERNEL);

	if (!pdata->info) {
		dev_err(&pdev->dev,
			"Error: Memory allocation for pdata->info failed\n");
		goto err;
	}

	ret = 0;
	for_each_child_of_node(of_node, child_node) {
		ret = get_bus_node_device_data(child_node, pdev,
				&pdata->info[i]);
		if (ret) {
			dev_err(&pdev->dev, "Error: unable to initialize bus nodes\n");
			goto err_1;
		}
		i++;
	}

	dev_dbg(&pdev->dev, "bus topology:\n");
	for (i = 0; i < pdata->num_devices; i++) {
		dev_dbg(&pdev->dev, "id %d\nnum_qports %d\nnum_connections %d",
				pdata->info[i].node_info->id,
				pdata->info[i].node_info->num_qports,
				pdata->info[i].node_info->num_connections);
		dev_dbg(&pdev->dev, "\nbus_device_id %d\n buswidth %d\n",
				pdata->info[i].node_info->bus_device_id,
				pdata->info[i].node_info->buswidth);
		for (j = 0; j < pdata->info[i].node_info->num_connections;
									j++) {
			dev_dbg(&pdev->dev, "connection[%d]: %d\n", j,
					pdata->info[i].node_info->
					connections[j]);
		}
		if (pdata->info[i].fabdev)
			dev_dbg(&pdev->dev, "base_addr %zu\nbus_type %d\n",
					(size_t)pdata->info[i].
						fabdev->pqos_base,
					pdata->info[i].fabdev->bus_type);
	}
	return pdata;

err_1:
	devm_kfree(&pdev->dev, pdata->info);
err:
	devm_kfree(&pdev->dev, pdata);
	pdata = NULL;
	return NULL;
}
