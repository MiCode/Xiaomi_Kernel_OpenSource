/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_CUSTOM_HW_SUB_MOD_SOC_H_
#define _CAM_CUSTOM_HW_SUB_MOD_SOC_H_

#include "cam_soc_util.h"
/*
 * struct cam_custom_hw_soc_private:
 *
 * @Brief:                   Private SOC data specific to Custom HW Driver
 *
 * @cpas_handle:             Handle returned on registering with CPAS driver.
 *                           This handle is used for all further interface
 *                           with CPAS.
 */
struct cam_custom_hw_soc_private {
	uint32_t    cpas_handle;
};

int cam_custom_hw_sub_mod_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t irq_handler, void *irq_data);

int cam_custom_hw_sub_mod_deinit_soc_resources(
	struct cam_hw_soc_info *soc_info);

int cam_custom_hw_sub_mod_enable_soc_resources(
	struct cam_hw_soc_info *soc_info);

int cam_custom_hw_sub_mod_disable_soc_resources(
	struct cam_hw_soc_info *soc_info);

#endif /* _CAM_CUSTOM_HW_SUB_MOD_SOC_H_ */
