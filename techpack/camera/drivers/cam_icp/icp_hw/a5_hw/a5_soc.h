/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_A5_SOC_H
#define CAM_A5_SOC_H

#include "cam_soc_util.h"

#define ICP_UBWC_MAX 2

struct a5_ubwc_cfg_ext {
	uint32_t ubwc_ipe_fetch_cfg[ICP_UBWC_MAX];
	uint32_t ubwc_ipe_write_cfg[ICP_UBWC_MAX];
	uint32_t ubwc_bps_fetch_cfg[ICP_UBWC_MAX];
	uint32_t ubwc_bps_write_cfg[ICP_UBWC_MAX];
};

struct a5_soc_info {
	const char *fw_name;
	bool ubwc_config_ext;
	uint32_t a5_qos_val;
	union {
		uint32_t ubwc_cfg[ICP_UBWC_MAX];
		struct a5_ubwc_cfg_ext ubwc_cfg_ext;
	} uconfig;
};

int cam_a5_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t a5_irq_handler, void *irq_data);

void cam_a5_deinit_soc_resources(struct cam_hw_soc_info *soc_info);

int cam_a5_enable_soc_resources(struct cam_hw_soc_info *soc_info);

int cam_a5_disable_soc_resources(struct cam_hw_soc_info *soc_info);

int cam_a5_update_clk_rate(struct cam_hw_soc_info *soc_info,
	int32_t clk_level);
#endif
