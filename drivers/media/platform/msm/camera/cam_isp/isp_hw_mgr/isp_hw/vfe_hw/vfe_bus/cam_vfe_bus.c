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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include "cam_vfe_bus.h"
#include "cam_vfe_bus_ver1.h"
#include "cam_vfe_bus_ver2.h"

int cam_vfe_bus_init(uint32_t          bus_version,
	struct cam_hw_soc_info        *soc_info,
	struct cam_hw_intf            *hw_intf,
	void                          *bus_hw_info,
	void                          *vfe_irq_controller,
	struct cam_vfe_bus           **vfe_bus)
{
	int rc = -ENODEV;

	switch (bus_version) {
	case CAM_VFE_BUS_VER_2_0:
		rc = cam_vfe_bus_ver2_init(soc_info, hw_intf, bus_hw_info,
			vfe_irq_controller, vfe_bus);
		break;
	default:
		pr_err("Unsupported Bus Version %x\n", bus_version);
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
	default:
		pr_err("Unsupported Bus Version %x\n", bus_version);
		break;
	}

	return rc;
}

