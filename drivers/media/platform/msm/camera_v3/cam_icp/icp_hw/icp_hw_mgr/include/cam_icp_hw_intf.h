/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#define CAM_ICP_CMD_BUF_MAX_SIZE     128
#define CAM_ICP_MSG_BUF_MAX_SIZE     CAM_ICP_CMD_BUF_MAX_SIZE

enum cam_a5_hw_type {
	CAM_ICP_DEV_A5,
	CAM_ICP_DEV_IPE,
	CAM_ICP_DEV_BPS,
	CAM_ICP_DEV_MAX,
};

/**
 * struct cam_a5_clk_update_cmd - Payload for hw manager command
 *
 * @curr_clk_rate:        clk rate to HW
 * @ipe_bps_pc_enable     power collpase enable flag
 */
struct cam_a5_clk_update_cmd {
	uint32_t  curr_clk_rate;
	bool  ipe_bps_pc_enable;
};
#endif
