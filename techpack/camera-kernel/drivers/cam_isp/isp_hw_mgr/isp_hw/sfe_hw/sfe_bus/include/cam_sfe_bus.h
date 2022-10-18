/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SFE_BUS_H_
#define _CAM_SFE_BUS_H_

#include "cam_sfe_hw_intf.h"

#define CAM_SFE_BUS_WR_VER_1_0          0x1000
#define CAM_SFE_BUS_RD_VER_1_0          0x1000
#define CAM_SFE_BUS_MAX_MID_PER_PORT    4

#define CAM_SFE_ADD_REG_VAL_PAIR(buf_array, index, offset, val)    \
	do {                                               \
		buf_array[(index)++] = offset;             \
		buf_array[(index)++] = val;                \
	} while (0)

#define ALIGNUP(value, alignment) \
	((value + alignment - 1) / alignment * alignment)

#define CACHE_ALLOC_NONE               0
#define CACHE_ALLOC_ALLOC              1
#define CACHE_ALLOC_ALLOC_CLEAN        2
#define CACHE_ALLOC_ALLOC_TRANS        3
#define CACHE_ALLOC_CLEAN              5
#define CACHE_ALLOC_DEALLOC            6
#define CACHE_ALLOC_FORGET             7
#define CACHE_ALLOC_TBH_ALLOC          8

#define DISABLE_CACHING_FOR_ALL           0xFFFFFF
#define CACHE_SCRATCH_RD_ALLOC_SHIFT      0
#define CACHE_SCRATCH_WR_ALLOC_SHIFT      4
#define CACHE_SCRATCH_DEBUG_SHIFT         8
#define CACHE_BUF_RD_ALLOC_SHIFT          12
#define CACHE_BUF_WR_ALLOC_SHIFT          16
#define CACHE_BUF_DEBUG_SHIFT             20

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
 * struct cam_sfe_bus_cache_dbg_cfg:
 *
 * @Brief:                   Bus cache debug cfg
 *
 * @disable_all:             Disable caching for all [scratch/snapshot]
 * @disable_for_scratch:     Disable caching for scratch
 * @scratch_dbg_cfg:         Scratch alloc configured
 * @scratch_alloc:           Alloc type for scratch
 * @disable_for_buf:         Disable caching for buffer
 * @buf_dbg_cfg:             Buf alloc configured
 * @buf_alloc:               Alloc type for actual buffer
 */
struct cam_sfe_bus_cache_dbg_cfg {
	bool disable_all;

	bool disable_for_scratch;
	bool scratch_dbg_cfg;
	uint32_t scratch_alloc;

	bool disable_for_buf;
	bool buf_dbg_cfg;
	uint32_t buf_alloc;
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


/*
 * cam_sfe_bus_parse_cache_cfg()
 *
 * @Brief:                   Parse SFE debug config
 *
 * @is_read:                 If set it's RM
 * @debug_val:               Debug val to be parsed
 * @dbg_cfg:                 Debug cfg of RM/WM
 *
 */
void cam_sfe_bus_parse_cache_cfg(
	bool is_read,
	uint32_t debug_val,
	struct cam_sfe_bus_cache_dbg_cfg *dbg_cfg);

#endif /* _CAM_SFE_BUS_ */
