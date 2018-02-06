/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
#ifndef _CAM_HW_OPS_H_
#define _CAM_HW_OPS_H_

#include <media/ais/msm_ais_mgr.h>

enum cam_ahb_clk_client {
	CAM_AHB_CLIENT_CSIPHY,
	CAM_AHB_CLIENT_CSID,
	CAM_AHB_CLIENT_CCI,
	CAM_AHB_CLIENT_ISPIF,
	CAM_AHB_CLIENT_VFE0,
	CAM_AHB_CLIENT_VFE1,
	CAM_AHB_CLIENT_CPP,
	CAM_AHB_CLIENT_FD,
	CAM_AHB_CLIENT_JPEG,
	CAM_AHB_CLIENT_MAX
};

int cam_config_ahb_clk(struct device *dev, unsigned long freq,
	enum cam_ahb_clk_client id, enum cam_ahb_clk_vote vote);
int cam_ahb_clk_init(struct platform_device *pdev);
#endif
