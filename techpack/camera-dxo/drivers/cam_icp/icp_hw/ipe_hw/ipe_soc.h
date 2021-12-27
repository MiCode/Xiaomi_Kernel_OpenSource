/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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
