/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_CPAS_SOC_H_
#define _CAM_CPAS_SOC_H_

#include "cam_soc_util.h"
#include "cam_cpas_hw.h"

#define CAM_REGULATOR_LEVEL_MAX 16
#define CAM_CPAS_MAX_TREE_NODES 50

/**
 * struct cam_cpas_vdd_ahb_mapping : Voltage to ahb level mapping
 *
 * @vdd_corner : Voltage corner value
 * @ahb_level : AHB vote level corresponds to this vdd_corner
 *
 */
struct cam_cpas_vdd_ahb_mapping {
	unsigned int vdd_corner;
	enum cam_vote_level ahb_level;
};

/**
 * struct cpas_tree_node: Generic cpas tree node for BW voting
 *
 * @cell_idx: Index to identify node from device tree and its parent
 * @level_idx: Index to identify at what level the node is present
 * @axi_port_idx: Index to identify which axi port to vote the consolidated bw
 * @path_data_type: Traffic type info from device tree (ife-vid, ife-disp etc)
 * @path_trans_type: Transaction type info from device tree (rd, wr)
 * @merge_type: Traffic merge type (calculation info) from device tree
 * @bus_width_factor: Factor for accounting bus width in CAMNOC bw calculation
 * @camnoc_bw: CAMNOC bw value at current node
 * @mnoc_ab_bw: MNOC AB bw value at current node
 * @mnoc_ib_bw: MNOC IB bw value at current node
 * @ddr_ab_bw: DDR AB bw value at current node
 * @ddr_ib_bw: DDR IB bw value at current node
 * @camnoc_max_needed: If node is needed for CAMNOC BW calculation then true
 * @constituent_paths: Constituent paths presence info from device tree
 *     Ex: For CAM_CPAS_PATH_DATA_IFE_UBWC_STATS, index corresponding to
 *     CAM_CPAS_PATH_DATA_IFE_VID, CAM_CPAS_PATH_DATA_IFE_DISP and
 *     CAM_CPAS_PATH_DATA_IFE_STATS
 * @tree_dev_node: Device node from devicetree for current tree node
 * @parent_node: Pointer to node one or more level above the current level
 *     (starting from end node of cpas client)
 *
 */
struct cam_cpas_tree_node {
	uint32_t cell_idx;
	uint32_t level_idx;
	int axi_port_idx;
	const char *node_name;
	uint32_t path_data_type;
	uint32_t path_trans_type;
	uint32_t merge_type;
	uint32_t bus_width_factor;
	uint64_t camnoc_bw;
	uint64_t mnoc_ab_bw;
	uint64_t mnoc_ib_bw;
	uint64_t ddr_ab_bw;
	uint64_t ddr_ib_bw;
	bool camnoc_max_needed;
	bool constituent_paths[CAM_CPAS_PATH_DATA_MAX];
	struct device_node *tree_dev_node;
	struct cam_cpas_tree_node *parent_node;
};

/**
 * struct cam_cpas_private_soc : CPAS private DT info
 *
 * @arch_compat: ARCH compatible string
 * @client_id_based: Whether clients are id based
 * @num_clients: Number of clients supported
 * @client_name: Client names
 * @tree_node: Array of pointers to all tree nodes required to calculate
 *      axi bw, arranged with help of cell index in device tree
 * @camera_bus_node: Device tree node from cpas node
 * @level_node: Device tree node for each level in camera_bus_node
 * @num_vdd_ahb_mapping : Number of vdd to ahb level mapping supported
 * @vdd_ahb : AHB level mapping info for the supported vdd levels
 * @control_camnoc_axi_clk : Whether CPAS driver need to set camnoc axi clk freq
 * @camnoc_bus_width : CAMNOC Bus width
 * @camnoc_axi_clk_bw_margin : BW Margin in percentage to add while calculating
 *      camnoc axi clock
 * @camnoc_axi_min_ib_bw: Min camnoc BW which varies based on target
 * @feature_mask: feature mask value for hw supported features
 *
 */
struct cam_cpas_private_soc {
	const char *arch_compat;
	bool client_id_based;
	uint32_t num_clients;
	const char *client_name[CAM_CPAS_MAX_CLIENTS];
	struct cam_cpas_tree_node *tree_node[CAM_CPAS_MAX_TREE_NODES];
	struct device_node *camera_bus_node;
	struct device_node *level_node[CAM_CPAS_MAX_TREE_LEVELS];
	uint32_t num_vdd_ahb_mapping;
	struct cam_cpas_vdd_ahb_mapping vdd_ahb[CAM_REGULATOR_LEVEL_MAX];
	bool control_camnoc_axi_clk;
	uint32_t camnoc_bus_width;
	uint32_t camnoc_axi_clk_bw_margin;
	uint64_t camnoc_axi_min_ib_bw;
	uint32_t feature_mask;
};

void cam_cpas_util_debug_parse_data(struct cam_cpas_private_soc *soc_private);
void cam_cpas_dump_axi_vote_info(
	const struct cam_cpas_client *cpas_client,
	const char *identifier,
	struct cam_axi_vote *axi_vote);
int cam_cpas_node_tree_cleanup(struct cam_cpas *cpas_core,
	struct cam_cpas_private_soc *soc_private);
int cam_cpas_soc_init_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t vfe_irq_handler, struct cam_hw_info *cpas_hw);
int cam_cpas_soc_deinit_resources(struct cam_hw_soc_info *soc_info);
int cam_cpas_soc_enable_resources(struct cam_hw_soc_info *soc_info,
	enum cam_vote_level default_level);
int cam_cpas_soc_disable_resources(struct cam_hw_soc_info *soc_info,
	bool disable_clocks, bool disable_irq);
int cam_cpas_soc_disable_irq(struct cam_hw_soc_info *soc_info);
#endif /* _CAM_CPAS_SOC_H_ */
