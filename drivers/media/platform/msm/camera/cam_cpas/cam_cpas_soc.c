/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/device.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "cam_cpas_api.h"
#include "cam_cpas_hw_intf.h"
#include "cam_cpas_hw.h"
#include "cam_cpas_soc.h"

int cam_cpas_get_custom_dt_info(struct platform_device *pdev,
	struct cam_cpas_private_soc *soc_private)
{
	struct device_node *of_node;
	int count = 0, i = 0, rc = 0;

	if (!soc_private || !pdev) {
		pr_err("invalid input arg %pK %pK\n", soc_private, pdev);
		return -EINVAL;
	}

	of_node = pdev->dev.of_node;

	rc = of_property_read_string_index(of_node, "arch-compat", 0,
		(const char **)&soc_private->arch_compat);
	if (rc) {
		pr_err("device %s failed to read arch-compat\n", pdev->name);
		return rc;
	}

	soc_private->client_id_based = of_property_read_bool(of_node,
		"client-id-based");

	count = of_property_count_strings(of_node, "client-names");
	if (count <= 0) {
		pr_err("no client-names found\n");
		count = 0;
		return -EINVAL;
	}
	soc_private->num_clients = count;
	CPAS_CDBG("arch-compat=%s, client_id_based = %d, num_clients=%d\n",
		soc_private->arch_compat, soc_private->client_id_based,
		soc_private->num_clients);

	for (i = 0; i < soc_private->num_clients; i++) {
		rc = of_property_read_string_index(of_node,
			"client-names", i, &soc_private->client_name[i]);
		if (rc) {
			pr_err("no client-name at cnt=%d\n", i);
			return -ENODEV;
		}
		CPAS_CDBG("Client[%d] : %s\n", i, soc_private->client_name[i]);
	}

	count = of_property_count_strings(of_node, "client-axi-port-names");
	if ((count <= 0) || (count != soc_private->num_clients)) {
		pr_err("incorrect client-axi-port-names info %d %d\n",
			count, soc_private->num_clients);
		count = 0;
		return -EINVAL;
	}

	for (i = 0; i < soc_private->num_clients; i++) {
		rc = of_property_read_string_index(of_node,
			"client-axi-port-names", i,
			&soc_private->client_axi_port_name[i]);
		if (rc) {
			pr_err("no client-name at cnt=%d\n", i);
			return -ENODEV;
		}
		CPAS_CDBG("Client AXI Port[%d] : %s\n", i,
			soc_private->client_axi_port_name[i]);
	}

	soc_private->axi_camnoc_based = of_property_read_bool(of_node,
		"client-bus-camnoc-based");

	return 0;
}

int cam_cpas_soc_init_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t irq_handler, void *irq_data)
{
	int rc = 0;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc) {
		pr_err("failed in get_dt_properties, rc=%d\n", rc);
		return rc;
	}

	if (soc_info->irq_line && !irq_handler) {
		pr_err("Invalid IRQ handler\n");
		return -EINVAL;
	}

	rc = cam_soc_util_request_platform_resource(soc_info, irq_handler,
		irq_data);
	if (rc) {
		pr_err("failed in request_platform_resource, rc=%d\n", rc);
		return rc;
	}

	soc_info->soc_private = kzalloc(sizeof(struct cam_cpas_private_soc),
		GFP_KERNEL);
	if (!soc_info->soc_private) {
		rc = -ENOMEM;
		goto release_res;
	}

	rc = cam_cpas_get_custom_dt_info(soc_info->pdev, soc_info->soc_private);
	if (rc) {
		pr_err("failed in get_custom_info, rc=%d\n", rc);
		goto free_soc_private;
	}

	return rc;

free_soc_private:
	kfree(soc_info->soc_private);
release_res:
	cam_soc_util_release_platform_resource(soc_info);
	return rc;
}

int cam_cpas_soc_deinit_resources(struct cam_hw_soc_info *soc_info)
{
	int rc;

	rc = cam_soc_util_release_platform_resource(soc_info);
	if (rc)
		pr_err("release platform failed, rc=%d\n", rc);

	kfree(soc_info->soc_private);
	soc_info->soc_private = NULL;

	return rc;
}

int cam_cpas_soc_enable_resources(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;

	rc = cam_soc_util_enable_platform_resource(soc_info, true, true);
	if (rc)
		pr_err("enable platform resource failed, rc=%d\n", rc);

	return rc;
}

int cam_cpas_soc_disable_resources(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;

	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc)
		pr_err("disable platform failed, rc=%d\n", rc);

	return rc;
}
