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

#ifndef CAM_ICP_HW_INTF_H
#define CAM_ICP_HW_INTF_H

#define CAM_ICP_CTX_MAX                 8

#define CAM_ICP_CMD_BUF_MAX_SIZE     128
#define CAM_ICP_MSG_BUF_MAX_SIZE     CAM_ICP_CMD_BUF_MAX_SIZE

enum cam_a5_hw_type {
	CAM_ICP_DEV_A5,
	CAM_ICP_DEV_IPE,
	CAM_ICP_DEV_BPS,
	CAM_ICP_DEV_MAX,
};
#endif
