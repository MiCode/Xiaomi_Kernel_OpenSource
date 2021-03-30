/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TFE_CSID_SOC_H_
#define _CAM_TFE_CSID_SOC_H_

#include "cam_isp_hw.h"

/*
 * struct cam_csid_soc_private:
 *
 * @Brief:                   Private SOC data specific to CSID HW Driver
 *
 * @cpas_handle:             Handle returned on registering with CPAS driver.
 *                           This handle is used for all further interface
 *                           with CPAS.
 */
struct cam_tfe_csid_soc_private {
	uint32_t cpas_handle;
};

/**
 * struct csid_device_soc_info - CSID SOC info object
 *
 * @csi_vdd_voltage:       Csi vdd voltage value
 *
 */
struct cam_tfe_csid_device_soc_info {
	int                             csi_vdd_voltage;
};

/**
 * cam_tfe_csid_init_soc_resources()
 *
 * @brief:                 Csid initialization function for the soc info
 *
 * @soc_info:              Soc info structure pointer
 * @csid_irq_handler:      Irq handler function to be registered
 * @irq_data:              Irq data for the callback function
 *
 */
int cam_tfe_csid_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t csid_irq_handler, void *irq_data);


/**
 * cam_tfe_csid_deinit_soc_resources()
 *
 * @brief:                 Csid de initialization function for the soc info
 *
 * @soc_info:              Soc info structure pointer
 *
 */
int cam_tfe_csid_deinit_soc_resources(struct cam_hw_soc_info *soc_info);

/**
 * cam_tfe_csid_enable_soc_resources()
 *
 * @brief:                 Csid soc resource enable function
 *
 * @soc_info:              Soc info structure pointer
 * @clk_lvl:               Vote level to start with
 *
 */
int cam_tfe_csid_enable_soc_resources(struct cam_hw_soc_info  *soc_info,
	uint32_t clk_lvl);

/**
 * cam_tfe_csid_disable_soc_resources()
 *
 * @brief:                 Csid soc resource disable function
 *
 * @soc_info:              Soc info structure pointer
 *
 */
int cam_tfe_csid_disable_soc_resources(struct cam_hw_soc_info *soc_info);

/**
 * cam_tfe_csid_enable_tfe_force_clock()
 *
 * @brief:                 If csid testgen used for dual isp case, before
 *                         starting csid test gen, enable tfe force clock on
 *                         through cpas
 *
 * @soc_info:              Soc info structure pointer
 * @cpas_tfe_base_offset:  Cpas tfe force clock base reg offset value
 *
 */
int cam_tfe_csid_enable_tfe_force_clock_on(struct cam_hw_soc_info  *soc_info,
	uint32_t cpas_tfe_base_offset);

/**
 * cam_tfe_csid_disable_tfe_force_clock_on()
 *
 * @brief:                 Disable the TFE force clock on after dual ISP
 *                         CSID test gen stop
 *
 * @soc_info:              Soc info structure pointer
 * @cpas_tfe_base_offset:  Cpas tfe force clock base reg offset value
 *
 */
int cam_tfe_csid_disable_tfe_force_clock_on(struct cam_hw_soc_info *soc_info,
	uint32_t cpas_tfe_base_offset);

/**
 * cam_tfe_csid_get_vote_level()
 *
 * @brief:                 Get the vote level from clock rate
 *
 * @soc_info:              Soc info structure pointer
 * @clock_rate             Clock rate
 *
 */
uint32_t cam_tfe_csid_get_vote_level(struct cam_hw_soc_info *soc_info,
	uint64_t clock_rate);

#endif /* _CAM_TFE_CSID_SOC_H_ */
