/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TOP_TPG_HW_INTF_H_
#define _CAM_TOP_TPG_HW_INTF_H_

#include "cam_isp_hw.h"
#include "cam_hw_intf.h"
#include "cam_ife_csid_hw_intf.h"

/* Max top tpg instance */
#define CAM_TOP_TPG_HW_NUM_MAX                        3
/* Max supported number of DT for TPG */
#define CAM_TOP_TPG_MAX_SUPPORTED_DT                  4
/* TPG default pattern should be color bar */
#define CAM_TOP_TPG_DEFAULT_PATTERN                   0x8
/**
 * enum cam_top_tpg_id - top tpg hw instance id
 */
enum cam_top_tpg_id {
	CAM_TOP_TPG_ID_0,
	CAM_TOP_TPG_ID_1,
	CAM_TOP_TPG_ID_2,
	CAM_TFE_TPG_ID_MAX,
};

/**
 * struct cam_top_tpg_hw_caps- Get the top tpg hw capability
 * @major_version : Major version
 * @minor_version:  Minor version
 * @version_incr:   Version increment
 *
 */
struct cam_top_tpg_hw_caps {
	uint32_t      major_version;
	uint32_t      minor_version;
	uint32_t      version_incr;
};

/**
 * struct cam_top_tpg_ver1_reserve_args- hw reserve
 * @num_inport:   number of inport
 *                TPG support 4 dt types, each different dt comes in different
 *                in port.
 * @in_port :     Input port resource info structure pointer
 * @node_res :    Reserved resource structure pointer
 *
 */
struct cam_top_tpg_ver1_reserve_args {
	uint32_t                          num_inport;
	struct cam_isp_tfe_in_port_info  *in_port[CAM_TOP_TPG_MAX_SUPPORTED_DT];
	struct cam_isp_resource_node     *node_res;
};

/**
 * struct cam_top_tpg_ver2_reserve_args
 * @num_inport:   number of inport
 *                TPG supports 1 VC/DT combo
 * @in_port :     Input port resource info structure pointer
 * @node_res :    Reserved resource structure pointer
 *
 */
struct cam_top_tpg_ver2_reserve_args {
	uint32_t                                  num_inport;
	struct cam_isp_in_port_generic_info      *in_port;
	struct cam_isp_resource_node             *node_res;
};

/**
 * struct cam_top_tpg_ver2_reserve_args
 * @num_inport:   number of inport
 *                TPG supports 4 VC's and 4 DT's per VC
 * @in_port :     Input port resource info structure pointer
 * @node_res :    Reserved resource structure pointer
 *
 */
struct cam_top_tpg_ver3_reserve_args {
	uint32_t                                  num_inport;
	struct cam_isp_in_port_generic_info      *in_port;
	struct cam_isp_resource_node             *node_res;
};

/**
 * cam_top_tpg_hw_init()
 *
 * @brief:               Initialize function for the tpg hardware
 *
 * @top_tpg_hw:          TPG hardware instance returned
 * @hw_idex:             TPG hardware instance id
 */
int cam_top_tpg_hw_init(struct cam_hw_intf **top_tpg_hw,
	uint32_t hw_idx);

/*
 * struct cam_top_tpg_clock_update_args:
 *
 * @clk_rate:                phy rate requested
 */
struct cam_top_tpg_clock_update_args {
	uint64_t                           clk_rate;
};

#endif /* _CAM_TOP_TPG_HW_INTF_H_ */
