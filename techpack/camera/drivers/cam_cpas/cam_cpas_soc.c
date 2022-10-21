// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

static uint cpas_dump;
module_param(cpas_dump, uint, 0644);


void cam_cpas_dump_axi_vote_info(
	const struct cam_cpas_client *cpas_client,
	const char *identifier,
	struct cam_axi_vote *axi_vote)
{
	int i;

	if (!cpas_dump)
		return;

	if (!axi_vote || (axi_vote->num_paths >
		CAM_CPAS_MAX_PATHS_PER_CLIENT)) {
		CAM_ERR(CAM_PERF, "Invalid num_paths %d",
			axi_vote ? axi_vote->num_paths : -1);
		return;
	}

	for (i = 0; i < axi_vote->num_paths; i++) {
		CAM_INFO(CAM_PERF,
		"Client [%s][%d] : [%s], Path=[%d] [%d], camnoc[%llu], mnoc_ab[%llu], mnoc_ib[%llu]",
		cpas_client->data.identifier, cpas_client->data.cell_index,
		identifier,
		axi_vote->axi_path[i].path_data_type,
		axi_vote->axi_path[i].transac_type,
		axi_vote->axi_path[i].camnoc_bw,
		axi_vote->axi_path[i].mnoc_ab_bw,
		axi_vote->axi_path[i].mnoc_ib_bw);
	}

}

void cam_cpas_util_debug_parse_data(
	struct cam_cpas_private_soc *soc_private)
{
	int i, j;
	struct cam_cpas_tree_node *curr_node = NULL;

	if (!cpas_dump)
		return;

	for (i = 0; i < CAM_CPAS_MAX_TREE_NODES; i++) {
		if (!soc_private->tree_node[i])
			break;

		curr_node = soc_private->tree_node[i];
		CAM_INFO(CAM_CPAS,
			"NODE cell_idx: %d, level: %d, name: %s, axi_port_idx: %d, merge_type: %d, parent_name: %s",
			curr_node->cell_idx, curr_node->level_idx,
			curr_node->node_name, curr_node->axi_port_idx,
			curr_node->merge_type, curr_node->parent_node ?
			curr_node->parent_node->node_name : "no parent");

		if (curr_node->level_idx)
			continue;

		CAM_INFO(CAM_CPAS, "path_type: %d, transac_type: %s",
			curr_node->path_data_type,
			cam_cpas_axi_util_trans_type_to_string(
			curr_node->path_trans_type));

		for (j = 0; j < CAM_CPAS_PATH_DATA_MAX; j++) {
			CAM_INFO(CAM_CPAS, "Constituent path: %d",
				curr_node->constituent_paths[j] ? j : -1);
		}
	}

	CAM_INFO(CAM_CPAS, "NUMBER OF NODES PARSED: %d", i);
}

int cam_cpas_node_tree_cleanup(struct cam_cpas *cpas_core,
	struct cam_cpas_private_soc *soc_private)
{
	int i = 0;

	for (i = 0; i < CAM_CPAS_MAX_TREE_NODES; i++) {
		if (soc_private->tree_node[i]) {
			of_node_put(soc_private->tree_node[i]->tree_dev_node);
			kfree(soc_private->tree_node[i]);
			soc_private->tree_node[i] = NULL;
		}
	}

	for (i = 0; i < CAM_CPAS_MAX_TREE_LEVELS; i++) {
		if (soc_private->level_node[i]) {
			of_node_put(soc_private->level_node[i]);
			soc_private->level_node[i] = NULL;
		}
	}

	if (soc_private->camera_bus_node) {
		of_node_put(soc_private->camera_bus_node);
		soc_private->camera_bus_node = NULL;
	}

	mutex_destroy(&cpas_core->tree_lock);

	return 0;
}

static int cam_cpas_util_path_type_to_idx(uint32_t *path_data_type)
{
	if (*path_data_type >= CAM_CPAS_PATH_DATA_CONSO_OFFSET)
		*path_data_type = CAM_CPAS_MAX_GRAN_PATHS_PER_CLIENT +
			(*path_data_type % CAM_CPAS_MAX_GRAN_PATHS_PER_CLIENT);
	else
		*path_data_type %= CAM_CPAS_MAX_GRAN_PATHS_PER_CLIENT;

	if (*path_data_type >= CAM_CPAS_PATH_DATA_MAX) {
		CAM_ERR(CAM_CPAS, "index Invalid: %d", path_data_type);
		return -EINVAL;
	}

	return 0;
}

static int cam_cpas_parse_node_tree(struct cam_cpas *cpas_core,
	struct device_node *of_node, struct cam_cpas_private_soc *soc_private)
{
	struct device_node *camera_bus_node;
	struct device_node *level_node;
	struct device_node *curr_node;
	struct device_node *parent_node;
	struct device_node *mnoc_node;
	int mnoc_idx = 0;
	uint32_t path_idx;
	bool camnoc_max_needed = false;
	struct cam_cpas_tree_node *curr_node_ptr = NULL;
	struct cam_cpas_client *curr_client = NULL;
	const char *client_name = NULL;
	uint32_t client_idx = 0, cell_idx = 0, level_idx = 0;
	int rc = 0, count = 0, i;

	camera_bus_node = of_find_node_by_name(of_node, "camera-bus-nodes");
	if (!camera_bus_node) {
		CAM_ERR(CAM_CPAS, "Camera Bus node not found in cpas DT node");
		return -EINVAL;
	}

	soc_private->camera_bus_node = camera_bus_node;

	for_each_available_child_of_node(camera_bus_node, level_node) {
		rc = of_property_read_u32(level_node, "level-index",
			&level_idx);
		if (rc) {
			CAM_ERR(CAM_CPAS, "Error raeding level idx rc: %d", rc);
			return rc;
		}
		if (level_idx >= CAM_CPAS_MAX_TREE_LEVELS) {
			CAM_ERR(CAM_CPAS, "Invalid level idx: %d", level_idx);
			return -EINVAL;
		}

		soc_private->level_node[level_idx] = level_node;
		camnoc_max_needed = of_property_read_bool(level_node,
			"camnoc-max-needed");

		for_each_available_child_of_node(level_node, curr_node) {
			curr_node_ptr =
				kzalloc(sizeof(struct cam_cpas_tree_node),
				GFP_KERNEL);
			if (!curr_node_ptr)
				return -ENOMEM;

			curr_node_ptr->tree_dev_node = curr_node;
			rc = of_property_read_u32(curr_node, "cell-index",
				&curr_node_ptr->cell_idx);
			if (rc) {
				CAM_ERR(CAM_CPAS, "Node index not found");
				return rc;
			}

			if (curr_node_ptr->cell_idx >=
				CAM_CPAS_MAX_TREE_NODES) {
				CAM_ERR(CAM_CPAS, "Invalid cell idx: %d",
					cell_idx);
				return -EINVAL;
			}

			soc_private->tree_node[curr_node_ptr->cell_idx] =
				curr_node_ptr;
			curr_node_ptr->level_idx = level_idx;

			rc = of_property_read_string(curr_node, "node-name",
				&curr_node_ptr->node_name);
			if (rc) {
				CAM_ERR(CAM_CPAS,
					"failed to read node-name rc=%d",
					rc);
				return rc;
			}

			curr_node_ptr->camnoc_max_needed = camnoc_max_needed;
			rc = of_property_read_u32(curr_node, "bus-width-factor",
				&curr_node_ptr->bus_width_factor);
			if (rc)
				curr_node_ptr->bus_width_factor = 1;

			rc = of_property_read_u32(curr_node,
				"traffic-merge-type",
				&curr_node_ptr->merge_type);

			curr_node_ptr->axi_port_idx = -1;
			mnoc_node = of_find_node_by_name(curr_node,
				"qcom,axi-port-mnoc");
			if (mnoc_node) {
				if (mnoc_idx >= CAM_CPAS_MAX_AXI_PORTS)
					return -EINVAL;

				cpas_core->axi_port[mnoc_idx].axi_port_node
					= mnoc_node;
				rc =  of_property_read_string(
					curr_node, "qcom,axi-port-name",
					&cpas_core->axi_port[mnoc_idx]
					.axi_port_name);
				if (rc) {
					CAM_ERR(CAM_CPAS,
					"failed to read mnoc-port-name rc=%d",
						rc);
					return rc;
				}
				cpas_core->axi_port
					[mnoc_idx].ib_bw_voting_needed
				= of_property_read_bool(curr_node,
					"ib-bw-voting-needed");
				curr_node_ptr->axi_port_idx = mnoc_idx;
				mnoc_idx++;
				cpas_core->num_axi_ports++;
			}

			rc = of_property_read_string(curr_node,
				"client-name", &client_name);
			if (!rc) {
				rc = of_property_read_u32(curr_node,
				"traffic-data", &curr_node_ptr->path_data_type);
				if (rc) {
					CAM_ERR(CAM_CPAS,
						"Path Data type not found");
					return rc;
				}

				rc = cam_cpas_util_path_type_to_idx(
					&curr_node_ptr->path_data_type);
				if (rc)
					return rc;

				rc = of_property_read_u32(curr_node,
					"traffic-transaction-type",
					&curr_node_ptr->path_trans_type);
				if (rc) {
					CAM_ERR(CAM_CPAS,
						"Path Transac type not found");
					return rc;
				}

				if (curr_node_ptr->path_trans_type >=
					CAM_CPAS_TRANSACTION_MAX) {
					CAM_ERR(CAM_CPAS,
						"Invalid transac type: %d",
						curr_node_ptr->path_trans_type);
					return -EINVAL;
				}

				count = of_property_count_u32_elems(curr_node,
					"constituent-paths");
				for (i = 0; i < count; i++) {
					rc = of_property_read_u32_index(
						curr_node, "constituent-paths",
						i, &path_idx);
					if (rc) {
						CAM_ERR(CAM_CPAS,
						"No constituent path at %d", i);
						return rc;
					}

					rc = cam_cpas_util_path_type_to_idx(
						&path_idx);
					if (rc)
						return rc;

					curr_node_ptr->constituent_paths
						[path_idx] = true;
				}

				rc = cam_common_util_get_string_index(
					soc_private->client_name,
					soc_private->num_clients,
					client_name, &client_idx);
				if (rc) {
					CAM_ERR(CAM_CPAS,
					"client name not found in list: %s",
					client_name);
					return rc;
				}

				if (client_idx >= CAM_CPAS_MAX_CLIENTS)
					return -EINVAL;

				curr_client =
					cpas_core->cpas_client[client_idx];
				curr_client->tree_node_valid = true;
				curr_client->tree_node
					[curr_node_ptr->path_data_type]
					[curr_node_ptr->path_trans_type] =
					curr_node_ptr;
				CAM_DBG(CAM_CPAS,
					"CLIENT NODE ADDED: %d %d %s",
					curr_node_ptr->path_data_type,
					curr_node_ptr->path_trans_type,
					client_name);
			}

			parent_node = of_parse_phandle(curr_node,
				"parent-node", 0);
			if (parent_node) {
				of_property_read_u32(parent_node, "cell-index",
					&cell_idx);
				curr_node_ptr->parent_node =
					soc_private->tree_node[cell_idx];
			} else {
				CAM_DBG(CAM_CPAS,
					"no parent node at this level");
			}
		}
	}
	mutex_init(&cpas_core->tree_lock);
	cam_cpas_util_debug_parse_data(soc_private);

	return 0;
}


int cam_cpas_get_hw_features(struct platform_device *pdev,
	struct cam_cpas_private_soc *soc_private)
{
	struct device_node *of_node;
	void *fuse;
	uint32_t fuse_addr, fuse_bit;
	uint32_t fuse_val = 0, feature_bit_pos;
	int count = 0, i = 0;

	of_node = pdev->dev.of_node;
	count = of_property_count_u32_elems(of_node, "cam_hw_fuse");

	for (i = 0; (i + 3) <= count; i = i + 3) {
		of_property_read_u32_index(of_node, "cam_hw_fuse", i,
				&feature_bit_pos);
		of_property_read_u32_index(of_node, "cam_hw_fuse", i + 1,
				&fuse_addr);
		of_property_read_u32_index(of_node, "cam_hw_fuse", i + 2,
				&fuse_bit);
		CAM_INFO(CAM_CPAS, "feature_bit 0x%x addr 0x%x, bit %d",
				feature_bit_pos, fuse_addr, fuse_bit);

		fuse = ioremap(fuse_addr, 4);
		if (fuse) {
			fuse_val = cam_io_r(fuse);
			if (fuse_val & BIT(fuse_bit))
				soc_private->feature_mask |= feature_bit_pos;
			else
				soc_private->feature_mask &= ~feature_bit_pos;
		}
		CAM_INFO(CAM_CPAS, "fuse %pK, fuse_val %x, feature_mask %x",
				fuse, fuse_val, soc_private->feature_mask);

	}

	return 0;
}

int cam_cpas_get_custom_dt_info(struct cam_hw_info *cpas_hw,
	struct platform_device *pdev, struct cam_cpas_private_soc *soc_private)
{
	struct device_node *of_node;
	int count = 0, i = 0, rc = 0;
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;

	if (!soc_private || !pdev) {
		CAM_ERR(CAM_CPAS, "invalid input arg %pK %pK",
			soc_private, pdev);
		return -EINVAL;
	}

	of_node = pdev->dev.of_node;
	soc_private->feature_mask = 0xFFFFFFFF;

	rc = of_property_read_string(of_node, "arch-compat",
		&soc_private->arch_compat);
	if (rc) {
		CAM_ERR(CAM_CPAS, "device %s failed to read arch-compat",
			pdev->name);
		return rc;
	}

	cam_cpas_get_hw_features(pdev, soc_private);

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
	} else if (count > CAM_CPAS_MAX_CLIENTS) {
		CAM_ERR(CAM_CPAS, "Number of clients %d greater than max %d",
			count, CAM_CPAS_MAX_CLIENTS);
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
			return -EINVAL;
		}

		cpas_core->cpas_client[i] =
			kzalloc(sizeof(struct cam_cpas_client), GFP_KERNEL);
		if (!cpas_core->cpas_client[i]) {
			rc = -ENOMEM;
			goto cleanup_clients;
		}

		CAM_DBG(CAM_CPAS, "Client[%d] : %s", i,
			soc_private->client_name[i]);
	}

	soc_private->control_camnoc_axi_clk = of_property_read_bool(of_node,
		"control-camnoc-axi-clk");

	if (soc_private->control_camnoc_axi_clk == true) {
		rc = of_property_read_u32(of_node, "camnoc-bus-width",
			&soc_private->camnoc_bus_width);
		if (rc || (soc_private->camnoc_bus_width == 0)) {
			CAM_ERR(CAM_CPAS, "Bus width not found rc=%d, %d",
				rc, soc_private->camnoc_bus_width);
			goto cleanup_clients;
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
				rc = -ENODEV;
				goto cleanup_clients;
			}

			rc = of_property_read_string_index(of_node,
				"vdd-corner-ahb-mapping", i, &ahb_string);
			if (rc) {
				CAM_ERR(CAM_CPAS,
					"no ahb-mapping at index=%d", i);
				rc = -ENODEV;
				goto cleanup_clients;
			}

			rc = cam_soc_util_get_level_from_string(ahb_string,
				&soc_private->vdd_ahb[i].ahb_level);
			if (rc) {
				CAM_ERR(CAM_CPAS,
					"invalid ahb-string at index=%d", i);
				rc = -EINVAL;
				goto cleanup_clients;
			}

			CAM_DBG(CAM_CPAS,
				"Vdd-AHB mapping [%d] : [%d] [%s] [%d]", i,
				soc_private->vdd_ahb[i].vdd_corner,
				ahb_string, soc_private->vdd_ahb[i].ahb_level);
		}

		soc_private->num_vdd_ahb_mapping = count;
	}

	rc = cam_cpas_parse_node_tree(cpas_core, of_node, soc_private);
	if (rc) {
		CAM_ERR(CAM_CPAS, "Node tree parsing failed rc: %d", rc);
		goto cleanup_tree;
	}

	return 0;

cleanup_tree:
	cam_cpas_node_tree_cleanup(cpas_core, soc_private);
cleanup_clients:
	cam_cpas_util_client_cleanup(cpas_hw);
	return rc;
}

int cam_cpas_soc_init_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t irq_handler, struct cam_hw_info *cpas_hw)
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
		cpas_hw);
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

	rc = cam_cpas_get_custom_dt_info(cpas_hw, soc_info->pdev,
		soc_info->soc_private);
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
