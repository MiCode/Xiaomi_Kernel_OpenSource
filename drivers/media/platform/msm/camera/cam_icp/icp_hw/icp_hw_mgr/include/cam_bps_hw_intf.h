/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#ifndef CAM_BPS_HW_INTF_H
#define CAM_BPS_HW_INTF_H

#include <uapi/media/cam_defs.h>
#include <media/cam_icp.h>
#include "cam_hw_mgr_intf.h"
#include "cam_icp_hw_intf.h"

enum cam_icp_bps_cmd_type {
	CAM_ICP_BPS_CMD_FW_DOWNLOAD,
	CAM_ICP_BPS_CMD_POWER_COLLAPSE,
	CAM_ICP_BPS_CMD_POWER_RESUME,
	CAM_ICP_BPS_CMD_SET_FW_BUF,
	CAM_ICP_BPS_CMD_VOTE_CPAS,
	CAM_ICP_BPS_CMD_CPAS_START,
	CAM_ICP_BPS_CMD_CPAS_STOP,
	CAM_ICP_BPS_CMD_UPDATE_CLK,
	CAM_ICP_BPS_CMD_DISABLE_CLK,
	CAM_ICP_BPS_CMD_MAX,
};

#endif /* CAM_BPS_HW_INTF_H */
