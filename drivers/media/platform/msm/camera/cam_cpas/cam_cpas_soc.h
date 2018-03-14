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

#ifndef _CAM_CPAS_SOC_H_
#define _CAM_CPAS_SOC_H_

#include "cam_soc_util.h"
#include "cam_cpas_hw.h"

#define CAM_REGULATOR_LEVEL_MAX 16

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
 * struct cam_cpas_private_soc : CPAS private DT info
 *
 * @arch_compat: ARCH compatible string
 * @hw_version: Camera HW version
 * @client_id_based: Whether clients are id based
 * @num_clients: Number of clients supported
 * @client_name: Client names
 * @axi_camnoc_based: Whether AXi access is camnoc based
 * @client_axi_port_name: AXI Port name for each client
 * @axi_port_list_node : Node representing AXI Ports list
 * @num_vdd_ahb_mapping : Number of vdd to ahb level mapping supported
 * @vdd_ahb : AHB level mapping info for the supported vdd levels
 * @control_camnoc_axi_clk : Whether CPAS driver need to set camnoc axi clk freq
 * @camnoc_bus_width : CAMNOC Bus width
 * @camnoc_axi_clk_bw_margin : BW Margin in percentage to add while calculating
 *      camnoc axi clock
 *
 */
struct cam_cpas_private_soc {
	const char *arch_compat;
	uint32_t hw_version;
	bool client_id_based;
	uint32_t num_clients;
	const char *client_name[CAM_CPAS_MAX_CLIENTS];
	bool axi_camnoc_based;
	const char *client_axi_port_name[CAM_CPAS_MAX_CLIENTS];
	struct device_node *axi_port_list_node;
	uint32_t num_vdd_ahb_mapping;
	struct cam_cpas_vdd_ahb_mapping vdd_ahb[CAM_REGULATOR_LEVEL_MAX];
	bool control_camnoc_axi_clk;
	uint32_t camnoc_bus_width;
	uint32_t camnoc_axi_clk_bw_margin;
};

int cam_cpas_soc_init_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t vfe_irq_handler, void *irq_data);
int cam_cpas_soc_deinit_resources(struct cam_hw_soc_info *soc_info);
int cam_cpas_soc_enable_resources(struct cam_hw_soc_info *soc_info,
	enum cam_vote_level default_level);
int cam_cpas_soc_disable_resources(struct cam_hw_soc_info *soc_info,
	bool disable_clocks, bool disble_irq);
int cam_cpas_soc_disable_irq(struct cam_hw_soc_info *soc_info);
#endif /* _CAM_CPAS_SOC_H_ */
