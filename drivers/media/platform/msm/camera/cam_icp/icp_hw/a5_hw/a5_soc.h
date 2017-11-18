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

#ifndef CAM_A5_SOC_H
#define CAM_A5_SOC_H

#include "cam_soc_util.h"

#define ICP_UBWC_MAX 2

struct a5_soc_info {
	char *fw_name;
	uint32_t ubwc_cfg[ICP_UBWC_MAX];
};

int cam_a5_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t a5_irq_handler, void *irq_data);

int cam_a5_enable_soc_resources(struct cam_hw_soc_info *soc_info);

int cam_a5_disable_soc_resources(struct cam_hw_soc_info *soc_info);

#endif
