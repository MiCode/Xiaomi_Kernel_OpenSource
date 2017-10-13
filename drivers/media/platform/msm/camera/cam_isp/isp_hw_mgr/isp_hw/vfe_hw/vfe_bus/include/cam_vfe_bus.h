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
#include "cam_hw_intf.h"
#include "cam_isp_hw.h"

#define CAM_VFE_BUS_VER_1_0             0x1000
#define CAM_VFE_BUS_VER_2_0             0x2000

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
 * @hw_ops:                  Hardware interface functions
 * @top_half_handler:        Top Half handler function
 * @bottom_half_handler:     Bottom Half handler function
 */
struct cam_vfe_bus {
	void               *bus_priv;

	struct cam_hw_ops              hw_ops;
	CAM_IRQ_HANDLER_TOP_HALF       top_half_handler;
	CAM_IRQ_HANDLER_BOTTOM_HALF    bottom_half_handler;
};

/*
 * cam_vfe_bus_init()
 *
 * @Brief:                   Initialize Bus layer
 *
 * @bus_version:             Version of BUS to initialize
 * @soc_info:                Soc Information for the associated HW
 * @hw_intf:                 HW Interface of HW to which this resource belongs
 * @bus_hw_info:             BUS HW info that contains details of BUS registers
 * @vfe_irq_controller:      VFE IRQ Controller to use for subscribing to Top
 *                           level IRQs
 * @vfe_bus:                 Pointer to vfe_bus structure which will be filled
 *                           and returned on successful initialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_vfe_bus_init(uint32_t          bus_version,
	struct cam_hw_soc_info        *soc_info,
	struct cam_hw_intf            *hw_intf,
	void                          *bus_hw_info,
	void                          *vfe_irq_controller,
	struct cam_vfe_bus            **vfe_bus);

/*
 * cam_vfe_bus_deinit()
 *
 * @Brief:                   Deinitialize Bus layer
 *
 * @bus_version:             Version of BUS to deinitialize
 * @vfe_bus:                 Pointer to vfe_bus structure to deinitialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_vfe_bus_deinit(uint32_t        bus_version,
	struct cam_vfe_bus           **vfe_bus);

#endif /* _CAM_VFE_BUS_ */
