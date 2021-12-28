/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CPAS_SOC_H_
#define _CAM_CPAS_SOC_H_

#include <linux/soc/qcom/llcc-qcom.h>
#include "cam_soc_util.h"
#include "cam_cpas_hw.h"

#define CAM_REGULATOR_LEVEL_MAX 16
#define CAM_CPAS_MAX_TREE_NODES 61
#define CAM_CPAS_MAX_FUSE_FEATURE 10

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
 * @camnoc_axi_port_idx: Index to find which axi port to vote consolidated bw
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
 * @pri_lut_low_offset: Register offset value for priority lut low.
 *                           Valid only for level1 nodes (representing NIUs)
 * @niu_size: Size of NIU that this node represents. Size in KB
 * @curr_priority: New calculated priority
 * @applied_priority: Currently applied priority
 *
 */
struct cam_cpas_tree_node {
	uint32_t cell_idx;
	int level_idx;
	int axi_port_idx;
	int camnoc_axi_port_idx;
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
	uint32_t pri_lut_low_offset;
	uint32_t niu_size;
	uint32_t curr_priority;
	uint32_t applied_priority;
};

/**
 * struct cam_cpas_feature_info : CPAS fuse feature info
 * @feature: Identifier for feature
 * @type: Type of feature
 * @value: Fuse value
 * @enable: Feature enable or disable
 * @hw_map: Each bit position indicates if the hw_id for the feature
 */

struct cam_cpas_feature_info {
	uint32_t feature;
	uint32_t type;
	uint32_t value;
	bool enable;
	uint32_t hw_map;
};

/**
 * struct cam_sys_cache_info : Last level camera cache info
 *
 * @ref_cnt:   Ref cnt activate/deactivate cache
 * @type:      cache type small/large etc.
 * @uid:       Client user ID
 * @size:      Cache size
 * @scid:      Slice ID
 * @slic_desc: Slice descriptor
 */
struct cam_sys_cache_info {
	uint32_t                         ref_cnt;
	enum cam_sys_cache_config_types  type;
	uint32_t                         uid;
	size_t                           size;
	int32_t                          scid;
	const char                      *name;
	struct llcc_slice_desc          *slic_desc;
};


/**
 * struct cam_cpas_smart_qos_info : Smart QOS info
 *
 * @rt_wr_priority_min:   Minimum priority value for rt write nius
 * @rt_wr_priority_max:   Maximum priority value for rt write nius
 * @num_rt_wr_nius:       Number of RT Wr NIUs
 * @rt_wr_niu_node:       List of level1 nodes representing RT Wr NIUs
 */
struct cam_cpas_smart_qos_info {
	uint8_t rt_wr_priority_min;
	uint8_t rt_wr_priority_max;
	uint8_t num_rt_wr_nius;
	struct cam_cpas_tree_node *rt_wr_niu_node[CAM_CPAS_MAX_RT_WR_NIU_NODES];
};


/**
 * struct cam_cpas_private_soc : CPAS private DT info
 *
 * @arch_compat: ARCH compatible string
 * @client_id_based: Whether clients are id based
 * @bus_icc_based: Interconnect based bus interaction
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
 * @fuse_info: fuse information
 * @rpmh_info: RPMH BCM info
 * @num_feature_info: number of feature_info entries
 * @feature_info: Structure for storing feature information
 * @num_caches: Number of last level caches
 * @llcc_info: Cache info
 * @enable_smart_qos: Whether to enable Smart QoS mechanism on current chipset
 * @smart_qos_info: Pointer to smart qos info
 */
struct cam_cpas_private_soc {
	const char *arch_compat;
	bool client_id_based;
	bool bus_icc_based;
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
	struct cam_cpas_fuse_info fuse_info;
	uint32_t rpmh_info[CAM_RPMH_BCM_INFO_MAX];
	uint32_t num_feature_info;
	struct cam_cpas_feature_info  feature_info[CAM_CPAS_MAX_FUSE_FEATURE];
	uint32_t num_caches;
	struct cam_sys_cache_info *llcc_info;
	bool enable_smart_qos;
	struct cam_cpas_smart_qos_info *smart_qos_info;
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
