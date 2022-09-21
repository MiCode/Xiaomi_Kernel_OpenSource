/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SFE_BUS_H_
#define _CAM_SFE_BUS_H_

#include "cam_sfe_hw_intf.h"

#define CAM_SFE_BUS_WR_VER_1_0          0x1000
#define CAM_SFE_BUS_RD_VER_1_0          0x1000

#define CAM_SFE_ADD_REG_VAL_PAIR(buf_array, index, offset, val)    \
	do {                                               \
		buf_array[(index)++] = offset;             \
		buf_array[(index)++] = val;                \
	} while (0)

#define ALIGNUP(value, alignment) \
	((value + alignment - 1) / alignment * alignment)

enum cam_sfe_bus_sfe_core_id {
	CAM_SFE_BUS_SFE_CORE_0,
	CAM_SFE_BUS_SFE_CORE_1,
	CAM_SFE_BUS_SFE_CORE_MAX,
};

enum cam_sfe_bus_plane_type {
	PLANE_Y,
	PLANE_C,
	PLANE_MAX,
};

enum cam_sfe_bus_type {
	BUS_TYPE_SFE_WR,
	BUS_TYPE_SFE_RD,
	BUS_TYPE_SFE_MAX,
};

/*
 * struct cam_sfe_bus:
 *
 * @Brief:                   Bus interface structure
 *
 * @bus_priv:                Private data of BUS
 * @hw_ops:                  Hardware interface functions
 * @top_half_handler:        Top Half handler function
 * @bottom_half_handler:     Bottom Half handler function
 */
struct cam_sfe_bus {
	void                          *bus_priv;
	struct cam_hw_ops              hw_ops;
	CAM_IRQ_HANDLER_TOP_HALF       top_half_handler;
	CAM_IRQ_HANDLER_BOTTOM_HALF    bottom_half_handler;
};

/*
 * cam_sfe_bus_init()
 *
 * @Brief:                   Initialize Bus layer
 *
 * @bus_version:             Version of BUS to initialize
 * @bus_type:                Bus Type RD/WR
 * @soc_info:                Soc Information for the associated HW
 * @hw_intf:                 HW Interface of HW to which this resource belongs
 * @bus_hw_info:             BUS HW info that contains details of BUS registers
 * @sfe_irq_controller:      SFE irq controller
 * @sfe_bus:                 Pointer to sfe_bus structure which will be filled
 *                           and returned on successful initialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_sfe_bus_init(
	uint32_t      bus_version,
	int                            bus_type,
	struct cam_hw_soc_info        *soc_info,
	struct cam_hw_intf            *hw_intf,
	void                          *bus_hw_info,
	void                          *sfe_irq_controller,
	struct cam_sfe_bus           **sfe_bus);

/*
 * cam_sfe_bus_deinit()
 *
 * @Brief:                   Deinitialize Bus layer
 *
 * @bus_version:             Version of BUS to deinitialize
 * @sfe_bus:                 Pointer to sfe_bus structure to deinitialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_sfe_bus_deinit(
	uint32_t                   bus_version,
	int                        bus_type,
	struct cam_sfe_bus       **sfe_bus);

#endif /* _CAM_SFE_BUS_ */
