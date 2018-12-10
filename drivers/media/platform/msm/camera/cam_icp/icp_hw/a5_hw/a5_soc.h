/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
