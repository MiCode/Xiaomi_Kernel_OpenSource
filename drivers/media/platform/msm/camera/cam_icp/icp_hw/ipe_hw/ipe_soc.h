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

#ifndef CAM_IPE_SOC_H
#define CAM_IPE_SOC_H

#include "cam_soc_util.h"

int cam_ipe_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t ipe_irq_handler, void *irq_data);

int cam_ipe_enable_soc_resources(struct cam_hw_soc_info *soc_info);

int cam_ipe_disable_soc_resources(struct cam_hw_soc_info *soc_info,
	bool disable_clk);

int cam_ipe_get_gdsc_control(struct cam_hw_soc_info *soc_info);

int cam_ipe_transfer_gdsc_control(struct cam_hw_soc_info *soc_info);

int cam_ipe_update_clk_rate(struct cam_hw_soc_info *soc_info,
	uint32_t clk_rate);
int cam_ipe_toggle_clk(struct cam_hw_soc_info *soc_info, bool clk_enable);
#endif /* CAM_IPE_SOC_H */
