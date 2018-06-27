/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
		CAM_ERR(CAM_CPAS, "invalid input arg %pK %pK",
			soc_private, pdev);
		return -EINVAL;
	}

	of_node = pdev->dev.of_node;

	rc = of_property_read_string_index(of_node, "arch-compat", 0,
		(const char **)&soc_private->arch_compat);
	if (rc) {
		CAM_ERR(CAM_CPAS, "device %s failed to read arch-compat",
			pdev->name);
		return rc;
	}


	soc_private->hw_version = 0;
	rc = of_property_read_u32(of_node,
		"qcom,cpas-hw-ver", &soc_private->hw_version);
	if (rc) {
		CAM_ERR(CAM_CPAS, "device %s failed to read cpas-hw-ver",
			pdev->name);
		return rc;
	}

	CAM_DBG(CAM_CPAS, "CPAS HW VERSION %x", soc_private->hw_version);

	soc_private->camnoc_axi_min_ib_bw = 0;
	rc = of_property_read_u64(of_node,
		"camnoc-axi-min-ib-bw",
		&soc_private->camnoc_axi_min_ib_bw);
	if (rc == -EOVERFLOW) {
		soc_private->camnoc_axi_min_ib_bw = 0;
		rc = of_property_read_u32(of_node,
			"camnoc-axi-min-ib-bw",
			(u32 *)&soc_private->camnoc_axi_min_ib_bw);
	}

	if (rc) {
		CAM_DBG(CAM_CPAS,
			"failed to read camnoc-axi-min-ib-bw rc:%d", rc);
		soc_private->camnoc_axi_min_ib_bw =
			CAM_CPAS_AXI_MIN_CAMNOC_IB_BW;
	}

	CAM_DBG(CAM_CPAS, "camnoc-axi-min-ib-bw = %llu",
		soc_private->camnoc_axi_min_ib_bw);

	soc_private->client_id_based = of_property_read_bool(of_node,
		"client-id-based");

	count = of_property_count_strings(of_node, "client-names");
	if (count <= 0) {
		CAM_ERR(CAM_CPAS, "no client-names found");
		count = 0;
		return -EINVAL;
	}
	soc_private->num_clients = count;
	CAM_DBG(CAM_CPAS,
		"arch-compat=%s, client_id_based = %d, num_clients=%d",
		soc_private->arch_compat, soc_private->client_id_based,
		soc_private->num_clients);

	for (i = 0; i < soc_private->num_clients; i++) {
		rc = of_property_read_string_index(of_node,
			"client-names", i, &soc_private->client_name[i]);
		if (rc) {
			CAM_ERR(CAM_CPAS, "no client-name at cnt=%d", i);
			return -ENODEV;
		}
		CAM_DBG(CAM_CPAS, "Client[%d] : %s", i,
			soc_private->client_name[i]);
	}

	count = of_property_count_strings(of_node, "client-axi-port-names");
	if ((count <= 0) || (count != soc_private->num_clients)) {
		CAM_ERR(CAM_CPAS, "incorrect client-axi-port-names info %d %d",
			count, soc_private->num_clients);
		count = 0;
		return -EINVAL;
	}

	for (i = 0; i < soc_private->num_clients; i++) {
		rc = of_property_read_string_index(of_node,
			"client-axi-port-names", i,
			&soc_private->client_axi_port_name[i]);
		if (rc) {
			CAM_ERR(CAM_CPAS, "no client-name at cnt=%d", i);
			return -ENODEV;
		}
		CAM_DBG(CAM_CPAS, "Client AXI Port[%d] : %s", i,
			soc_private->client_axi_port_name[i]);
	}

	soc_private->axi_camnoc_based = of_property_read_bool(of_node,
		"client-bus-camnoc-based");

	soc_private->control_camnoc_axi_clk = of_property_read_bool(of_node,
		"control-camnoc-axi-clk");

	if (soc_private->control_camnoc_axi_clk == true) {
		rc = of_property_read_u32(of_node, "camnoc-bus-width",
			&soc_private->camnoc_bus_width);
		if (rc || (soc_private->camnoc_bus_width == 0)) {
			CAM_ERR(CAM_CPAS, "Bus width not found rc=%d, %d",
				rc, soc_private->camnoc_bus_width);
			return rc;
		}

		rc = of_property_read_u32(of_node,
			"camnoc-axi-clk-bw-margin-perc",
			&soc_private->camnoc_axi_clk_bw_margin);

		if (rc) {
			/* this is not fatal, overwrite rc */
			rc = 0;
			soc_private->camnoc_axi_clk_bw_margin = 0;
		}
	}

	CAM_DBG(CAM_CPAS,
		"control_camnoc_axi_clk=%d, width=%d, margin=%d",
		soc_private->control_camnoc_axi_clk,
		soc_private->camnoc_bus_width,
		soc_private->camnoc_axi_clk_bw_margin);

	count = of_property_count_u32_elems(of_node, "vdd-corners");
	if ((count > 0) && (count <= CAM_REGULATOR_LEVEL_MAX) &&
		(of_property_count_strings(of_node, "vdd-corner-ahb-mapping") ==
		count)) {
		const char *ahb_string;

		for (i = 0; i < count; i++) {
			rc = of_property_read_u32_index(of_node, "vdd-corners",
				i, &soc_private->vdd_ahb[i].vdd_corner);
			if (rc) {
				CAM_ERR(CAM_CPAS,
					"vdd-corners failed at index=%d", i);
				return -ENODEV;
			}

			rc = of_property_read_string_index(of_node,
				"vdd-corner-ahb-mapping", i, &ahb_string);
			if (rc) {
				CAM_ERR(CAM_CPAS,
					"no ahb-mapping at index=%d", i);
				return -ENODEV;
			}

			rc = cam_soc_util_get_level_from_string(ahb_string,
				&soc_private->vdd_ahb[i].ahb_level);
			if (rc) {
				CAM_ERR(CAM_CPAS,
					"invalid ahb-string at index=%d", i);
				return -EINVAL;
			}

			CAM_DBG(CAM_CPAS,
				"Vdd-AHB mapping [%d] : [%d] [%s] [%d]", i,
				soc_private->vdd_ahb[i].vdd_corner,
				ahb_string, soc_private->vdd_ahb[i].ahb_level);
		}

		soc_private->num_vdd_ahb_mapping = count;
	}

	return 0;
}

int cam_cpas_soc_init_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t irq_handler, void *irq_data)
{
	int rc = 0;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc) {
		CAM_ERR(CAM_CPAS, "failed in get_dt_properties, rc=%d", rc);
		return rc;
	}

	if (soc_info->irq_line && !irq_handler) {
		CAM_ERR(CAM_CPAS, "Invalid IRQ handler");
		return -EINVAL;
	}

	rc = cam_soc_util_request_platform_resource(soc_info, irq_handler,
		irq_data);
	if (rc) {
		CAM_ERR(CAM_CPAS, "failed in request_platform_resource, rc=%d",
			rc);
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
		CAM_ERR(CAM_CPAS, "failed in get_custom_info, rc=%d", rc);
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
		CAM_ERR(CAM_CPAS, "release platform failed, rc=%d", rc);

	kfree(soc_info->soc_private);
	soc_info->soc_private = NULL;

	return rc;
}

int cam_cpas_soc_enable_resources(struct cam_hw_soc_info *soc_info,
	enum cam_vote_level default_level)
{
	int rc = 0;

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		default_level, true);
	if (rc)
		CAM_ERR(CAM_CPAS, "enable platform resource failed, rc=%d", rc);

	return rc;
}

int cam_cpas_soc_disable_resources(struct cam_hw_soc_info *soc_info,
	bool disable_clocks, bool disable_irq)
{
	int rc = 0;

	rc = cam_soc_util_disable_platform_resource(soc_info,
		disable_clocks, disable_irq);
	if (rc)
		CAM_ERR(CAM_CPAS, "disable platform failed, rc=%d", rc);

	return rc;
}

int cam_cpas_soc_disable_irq(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;

	rc = cam_soc_util_irq_disable(soc_info);
	if (rc)
		CAM_ERR(CAM_CPAS, "disable irq failed, rc=%d", rc);

	return rc;
}
