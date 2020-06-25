/* Copyright (c) 2017-2018, 2020 The Linux Foundation. All rights reserved.
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

#ifndef _AIS_IFE_CSID_SOC_H_
#define _AIS_IFE_CSID_SOC_H_

#include "ais_isp_hw.h"

/*
 * struct ais_csid_soc_private:
 *
 * @Brief:                   Private SOC data specific to CSID HW Driver
 *
 * @cpas_handle:             Handle returned on registering with CPAS driver.
 *                           This handle is used for all further interface
 *                           with CPAS.
 */
struct ais_csid_soc_private {
	uint32_t cpas_handle;
};

/**
 * struct csid_device_soc_info - CSID SOC info object
 *
 * @csi_vdd_voltage:       csi vdd voltage value
 *
 */
struct csid_device_soc_info {
	int                             csi_vdd_voltage;
};

/**
 * ais_ife_csid_init_soc_resources()
 *
 * @brief:                 csid initialization function for the soc info
 *
 * @soc_info:              soc info structure pointer
 * @csid_irq_handler:      irq handler function to be registered
 * @irq_data:              irq data for the callback function
 *
 */
int ais_ife_csid_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t csid_irq_handler, void *irq_data);


/**
 * ais_ife_csid_deinit_soc_resources()
 *
 * @brief:                 csid de initialization function for the soc info
 *
 * @soc_info:              soc info structure pointer
 *
 */
int ais_ife_csid_deinit_soc_resources(struct cam_hw_soc_info *soc_info);

/**
 * ais_ife_csid_enable_soc_resources()
 *
 * @brief:                 csid soc resource enable function
 *
 * @soc_info:              soc info structure pointer
 * @clk_lvl:               vote level to start with
 *
 */
int ais_ife_csid_enable_soc_resources(struct cam_hw_soc_info  *soc_info,
	uint32_t clk_lvl);

/**
 * ais_ife_csid_disable_soc_resources()
 *
 * @brief:                 csid soc resource disable function
 *
 * @soc_info:              soc info structure pointer
 *
 */
int ais_ife_csid_disable_soc_resources(struct cam_hw_soc_info *soc_info);

/**
 * ais_ife_csid_enable_ife_force_clock()
 *
 * @brief:                 if csid testgen used for dual isp case, before
 *                         starting csid test gen, enable ife force clock on
 *                         through cpas
 *
 * @soc_info:              soc info structure pointer
 * @cpas_ife_base_offset:  cpas ife force clock base reg offset value
 *
 */
int ais_ife_csid_enable_ife_force_clock_on(struct cam_hw_soc_info  *soc_info,
	uint32_t cpas_ife_base_offset);

/**
 * ais_ife_csid_disable_ife_force_clock_on()
 *
 * @brief:                 disable the IFE force clock on after dual ISP
 *                         CSID test gen stop
 *
 * @soc_info:              soc info structure pointer
 * @cpas_ife_base_offset:  cpas ife force clock base reg offset value
 *
 */
int ais_ife_csid_disable_ife_force_clock_on(struct cam_hw_soc_info *soc_info,
	uint32_t cpas_ife_base_offset);

/**
 * ais_ife_csid_get_vote_level()
 *
 * @brief:                 get the vote level from clock rate
 *
 * @soc_info:              soc info structure pointer
 * @clock_rate             clock rate
 *
 */
uint32_t ais_ife_csid_get_vote_level(struct cam_hw_soc_info *soc_info,
	uint64_t clock_rate);

#endif
