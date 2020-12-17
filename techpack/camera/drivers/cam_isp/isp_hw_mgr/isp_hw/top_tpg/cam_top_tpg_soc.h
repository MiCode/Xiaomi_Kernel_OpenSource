/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TOP_TPG_SOC_H_
#define _CAM_TOP_TPG_SOC_H_

#include "cam_isp_hw.h"

/*
 * struct cam_top_tpg_soc_private:
 *
 * @Brief:                   Private SOC data specific to TPG HW Driver
 *
 * @cpas_handle:             Handle returned on registering with CPAS driver.
 *                           This handle is used for all further interface
 *                           with CPAS.
 */
struct cam_top_tpg_soc_private {
	uint32_t cpas_handle;
};

/**
 * struct cam_top_tpg_device_soc_info - tpg soc SOC info object
 *
 * @csi_vdd_voltage:       csi vdd voltage value
 *
 */
struct cam_top_tpg_device_soc_info {
	int                             csi_vdd_voltage;
};

/**
 * cam_top_tpg_init_soc_resources()
 *
 * @brief:                 csid initialization function for the soc info
 *
 * @soc_info:              soc info structure pointer
 * @irq_data:              irq data for the callback function
 *
 */
int cam_top_tpg_init_soc_resources(struct cam_hw_soc_info *soc_info,
	void *irq_data);

/**
 * cam_top_tpg_deinit_soc_resources()
 *
 * @brief:                 tpg de initialization function for the soc info
 *
 * @soc_info:              soc info structure pointer
 *
 */
int cam_top_tpg_deinit_soc_resources(struct cam_hw_soc_info *soc_info);

/**
 * cam_top_tpg_enable_soc_resources()
 *
 * @brief:                 tpg soc resource enable function
 *
 * @soc_info:              soc info structure pointer
 * @clk_lvl:               vote level to start with
 *
 */
int cam_top_tpg_enable_soc_resources(struct cam_hw_soc_info  *soc_info,
	uint32_t clk_lvl);

/**
 * cam_top_tpg_disable_soc_resources()
 *
 * @brief:                 csid soc resource disable function
 *
 * @soc_info:              soc info structure pointer
 *
 */
int cam_top_tpg_disable_soc_resources(struct cam_hw_soc_info *soc_info);

#endif /* _CAM_TOP_TPG_SOC_H_ */
