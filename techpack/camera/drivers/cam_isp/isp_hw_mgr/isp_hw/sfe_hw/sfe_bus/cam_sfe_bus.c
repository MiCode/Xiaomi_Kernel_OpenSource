// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "cam_sfe_bus.h"
#include "cam_sfe_bus_rd.h"
#include "cam_sfe_bus_wr.h"
#include "cam_debug_util.h"

int cam_sfe_bus_init(
	uint32_t                       bus_version,
	int                            bus_type,
	struct cam_hw_soc_info        *soc_info,
	struct cam_hw_intf            *hw_intf,
	void                          *bus_hw_info,
	void                          *sfe_irq_controller,
	struct cam_sfe_bus           **sfe_bus)
{
	int rc = -ENODEV;

	switch (bus_type) {
	case BUS_TYPE_SFE_WR:
		switch (bus_version) {
		case CAM_SFE_BUS_WR_VER_1_0:
			rc = cam_sfe_bus_wr_init(soc_info, hw_intf,
				bus_hw_info, sfe_irq_controller, sfe_bus);
			break;
		default:
			CAM_ERR(CAM_SFE, "Unsupported Bus WR Version 0x%x",
				bus_version);
			break;
		}
		break;
	case BUS_TYPE_SFE_RD:
		switch (bus_version) {
		case CAM_SFE_BUS_RD_VER_1_0:
			rc = cam_sfe_bus_rd_init(soc_info, hw_intf,
				bus_hw_info, sfe_irq_controller, sfe_bus);
			break;
		default:
			CAM_ERR(CAM_SFE, "Unsupported Bus RD Version 0x%x",
				bus_version);
			break;
		}
		break;
	default:
		CAM_ERR(CAM_SFE, "Unsupported Bus type %d", bus_type);
		break;
	}

	return rc;
}

int cam_sfe_bus_deinit(
	uint32_t              bus_version,
	int                   bus_type,
	struct cam_sfe_bus  **sfe_bus)
{
	int rc = -ENODEV;

	switch (bus_type) {
	case BUS_TYPE_SFE_WR:
		switch (bus_version) {
		case CAM_SFE_BUS_WR_VER_1_0:
			rc = cam_sfe_bus_wr_deinit(sfe_bus);
			break;
		default:
			CAM_ERR(CAM_SFE, "Unsupported Bus WR Version 0x%x",
				bus_version);
			break;
		}
		break;
	case BUS_TYPE_SFE_RD:
		switch (bus_version) {
		case CAM_SFE_BUS_RD_VER_1_0:
			rc = cam_sfe_bus_rd_deinit(sfe_bus);
			break;
		default:
			CAM_ERR(CAM_SFE, "Unsupported Bus RD Version 0x%x",
				bus_version);
			break;
		}
		break;
	default:
		CAM_ERR(CAM_SFE, "Unsupported Bus type %d", bus_type);
		break;
	}

	return rc;
}

