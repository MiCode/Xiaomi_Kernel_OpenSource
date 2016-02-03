/* Copyright (c) 2015 The Linux Foundation. All rights reserved.
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

enum cam_ahb_clk_vote {
	/* need to update the voting requests
	 * according to dtsi entries.
	 */
	CAMERA_AHB_SUSPEND_VOTE = 0x01,
	CAMERA_AHB_SVS_VOTE = 0x02,
	CAMERA_AHB_NOMINAL_VOTE = 0x04,
	CAMERA_AHB_TURBO_VOTE = 0x08,
};

enum cam_ahb_clk_client {
	CAM_AHB_CLIENT_CSIPHY,
	CAM_AHB_CLIENT_CSID,
	CAM_AHB_CLIENT_CCI,
	CAM_AHB_CLIENT_ISPIF,
	CAM_AHB_CLIENT_VFE,
	CAM_AHB_CLIENT_CPP,
	CAM_AHB_CLIENT_FD,
	CAM_AHB_CLIENT_JPEG,
	CAM_AHB_CLIENT_MAX
};

int cam_config_ahb_clk(enum cam_ahb_clk_client id,
	enum cam_ahb_clk_vote vote);
int cam_ahb_clk_init(struct platform_device *pdev);
