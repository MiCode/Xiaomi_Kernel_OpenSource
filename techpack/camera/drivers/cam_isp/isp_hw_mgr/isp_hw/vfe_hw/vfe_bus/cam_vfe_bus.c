// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include "cam_vfe_bus.h"
#include "cam_vfe_bus_ver1.h"
#include "cam_vfe_bus_ver2.h"
#include "cam_vfe_bus_rd_ver1.h"
#include "cam_vfe_bus_ver3.h"
#include "cam_debug_util.h"

int cam_vfe_bus_init(uint32_t          bus_version,
	int                            bus_type,
	struct cam_hw_soc_info        *soc_info,
	struct cam_hw_intf            *hw_intf,
	void                          *bus_hw_info,
	void                          *vfe_irq_controller,
	struct cam_vfe_bus           **vfe_bus)
{
	int rc = -ENODEV;

	switch (bus_type) {
	case BUS_TYPE_WR:
		switch (bus_version) {
		case CAM_VFE_BUS_VER_2_0:
			rc = cam_vfe_bus_ver2_init(soc_info, hw_intf,
				bus_hw_info, vfe_irq_controller, vfe_bus);
			break;
		case CAM_VFE_BUS_VER_3_0:
			rc = cam_vfe_bus_ver3_init(soc_info, hw_intf,
				bus_hw_info, vfe_irq_controller, vfe_bus);
			break;
		default:
			CAM_ERR(CAM_ISP, "Unsupported Bus WR Version 0x%x",
				bus_version);
			break;
		}
		break;
	case BUS_TYPE_RD:
		switch (bus_version) {
		case CAM_VFE_BUS_RD_VER_1_0:
			/* Call vfe bus rd init function */
			rc = cam_vfe_bus_rd_ver1_init(soc_info, hw_intf,
				bus_hw_info, vfe_irq_controller, vfe_bus);
			break;
		default:
			CAM_ERR(CAM_ISP, "Unsupported Bus RD Version 0x%x",
				bus_version);
			break;
		}
		break;
	default:
		CAM_ERR(CAM_ISP, "Unsupported Bus type %d", bus_type);
		break;
	}

	return rc;
}

int cam_vfe_bus_deinit(uint32_t        bus_version,
	struct cam_vfe_bus           **vfe_bus)
{
	int rc = -ENODEV;

	switch (bus_version) {
	case CAM_VFE_BUS_VER_2_0:
		rc = cam_vfe_bus_ver2_deinit(vfe_bus);
		break;
	case CAM_VFE_BUS_VER_3_0:
		rc = cam_vfe_bus_ver3_deinit(vfe_bus);
		break;
	default:
		CAM_ERR(CAM_ISP, "Unsupported Bus Version %x", bus_version);
		break;
	}

	return rc;
}
