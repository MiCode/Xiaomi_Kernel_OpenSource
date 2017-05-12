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

#ifndef _CAM_VFE_BUS_H_
#define _CAM_VFE_BUS_H_

#include <uapi/media/cam_isp.h>
#include "cam_isp_hw.h"

#define CAM_VFE_BUS_VER_1_0 0x1000
#define CAM_VFE_BUS_VER_2_0 0x2000

enum cam_vfe_bus_plane_type {
	PLANE_Y,
	PLANE_C,
	PLANE_MAX,
};

/*
 * struct cam_vfe_bus:
 *
 * @Brief:                   Bus interface structure
 *
 * @bus_priv:                Private data of BUS
 * @acquire_resource:        Function pointer for acquiring BUS output resource
 * @release_resource:        Function pointer for releasing BUS resource
 * @start_resource:          Function for starting BUS Output resource
 * @stop_resource:           Function for stopping BUS Output resource
 * @process_cmd:             Function to process commands specific to BUS
 *                           resources
 * @top_half_handler:        Top Half handler function
 * @bottom_half_handler:     Bottom Half handler function
 */
struct cam_vfe_bus {
	void               *bus_priv;

	int (*acquire_resource)(void *bus_priv, void *acquire_args);
	int (*release_resource)(void *bus_priv,
		struct cam_isp_resource_node *vfe_out);
	int (*start_resource)(struct cam_isp_resource_node *vfe_out);
	int (*stop_resource)(struct cam_isp_resource_node *vfe_out);
	int (*process_cmd)(void *priv, uint32_t cmd_type, void *cmd_args,
		uint32_t arg_size);
	CAM_IRQ_HANDLER_TOP_HALF       top_half_handler;
	CAM_IRQ_HANDLER_BOTTOM_HALF    bottom_half_handler;
};

/*
 * cam_vfe_bus_init()
 *
 * @Brief:                   Initialize Bus layer
 *
 * @bus_version:             Version of BUS to initialize
 * @mem_base:                Mapped base address of register space
 * @hw_intf:                 HW Interface of HW to which this resource belongs
 * @bus_hw_info:             BUS HW info that contains details of BUS registers
 * @vfe_irq_controller:      VFE IRQ Controller to use for subscribing to Top
 *                           level IRQs
 * @vfe_bus:                 Pointer to vfe_bus structure which will be filled
 *                           and returned on successful initialize
 */
int cam_vfe_bus_init(uint32_t          bus_version,
	void __iomem                  *mem_base,
	struct cam_hw_intf            *hw_intf,
	void                          *bus_hw_info,
	void                          *vfe_irq_controller,
	struct cam_vfe_bus            **vfe_bus);

#endif /* _CAM_VFE_BUS_ */
