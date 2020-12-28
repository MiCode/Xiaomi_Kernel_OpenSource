/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_BPS_SOC_H_
#define _CAM_BPS_SOC_H_

#include "cam_soc_util.h"

int cam_bps_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t bps_irq_handler, void *irq_data);

int cam_bps_enable_soc_resources(struct cam_hw_soc_info *soc_info);

int cam_bps_disable_soc_resources(struct cam_hw_soc_info *soc_info,
	bool disable_clk);

int cam_bps_get_gdsc_control(struct cam_hw_soc_info *soc_info);

int cam_bps_transfer_gdsc_control(struct cam_hw_soc_info *soc_info);

int cam_bps_update_clk_rate(struct cam_hw_soc_info *soc_info,
	uint32_t clk_rate);
int cam_bps_toggle_clk(struct cam_hw_soc_info *soc_info, bool clk_enable);
void cam_bps_deinit_soc_resources(struct cam_hw_soc_info *soc_info);
#endif /* _CAM_BPS_SOC_H_*/
