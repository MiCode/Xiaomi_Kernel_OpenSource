/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_OPE_SOC_H
#define CAM_OPE_SOC_H

#include "cam_soc_util.h"

/**
 * struct ope_dev_soc
 *
 * @hfi_en: HFI enable flag
 */
struct ope_dev_soc {
	uint32_t hfi_en;
};

int cam_ope_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t ope_irq_handler, void *irq_data);


int cam_ope_enable_soc_resources(struct cam_hw_soc_info *soc_info);

int cam_ope_disable_soc_resources(struct cam_hw_soc_info *soc_info,
	bool disable_clk);

int cam_ope_update_clk_rate(struct cam_hw_soc_info *soc_info,
	uint32_t clk_rate);

int cam_ope_toggle_clk(struct cam_hw_soc_info *soc_info, bool clk_enable);
#endif /* CAM_OPE_SOC_H */
