/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_LX7_SOC_H_
#define _CAM_LX7_SOC_H_

#include <linux/interrupt.h>

#include "cam_soc_util.h"

#define UBWC_CONFIG_MAX 2

struct lx7_soc_info {
	uint32_t icp_qos_val;
	struct {
		uint32_t ipe_fetch[UBWC_CONFIG_MAX];
		uint32_t ipe_write[UBWC_CONFIG_MAX];
		uint32_t bps_fetch[UBWC_CONFIG_MAX];
		uint32_t bps_write[UBWC_CONFIG_MAX];
	} ubwc_cfg;
};

int cam_lx7_soc_resources_init(struct cam_hw_soc_info *soc_info,
			irq_handler_t irq_handler,
			void *irq_data);
int cam_lx7_soc_resources_deinit(struct cam_hw_soc_info *soc_info);

int cam_lx7_soc_resources_enable(struct cam_hw_soc_info *soc_info);
int cam_lx7_soc_resources_disable(struct cam_hw_soc_info *soc_info);
int cam_lx7_update_clk_rate(struct cam_hw_soc_info *soc_info,
	int32_t clk_level);

#endif /* _CAM_LX7_SOC_H_ */
