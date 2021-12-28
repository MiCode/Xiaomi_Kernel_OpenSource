/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_BPS_HW_INTF_H
#define CAM_BPS_HW_INTF_H

#include <media/cam_defs.h>
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
	CAM_ICP_BPS_CMD_RESET,
	CAM_ICP_BPS_CMD_MAX,
};

#endif /* CAM_BPS_HW_INTF_H */
