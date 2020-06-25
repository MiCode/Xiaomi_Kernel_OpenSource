/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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

#ifndef _AIS_VFE_SOC_H_
#define _AIS_VFE_SOC_H_

#include "cam_soc_util.h"
#include "ais_isp_hw.h"

#define AIS_VFE_DSP_CLK_NAME "ife_dsp_clk"

enum cam_cpas_handle_id {
	CAM_CPAS_HANDLE_CAMIF,
	CAM_CPAS_HANDLE_RAW,
	CAM_CPAS_HANDLE_MAX,
};

/*
 * struct ais_vfe_soc_private:
 *
 * @Brief:                   Private SOC data specific to VFE HW Driver
 *
 * @cpas_handle:             Handle returned on registering with CPAS driver.
 *                           This handle is used for all further interface
 *                           with CPAS.
 * @cpas_version:            Has cpas version read from Hardware
 */
struct ais_vfe_soc_private {
	uint32_t    cpas_handle[CAM_CPAS_HANDLE_MAX];
	uint32_t    cpas_version;
	struct clk *dsp_clk;
	int32_t     dsp_clk_index;
	int32_t     dsp_clk_rate;
};

/*
 * ais_vfe_init_soc_resources()
 *
 * @Brief:                   Initialize SOC resources including private data
 *
 * @soc_info:                Device soc information
 * @handler:                 Irq handler function pointer
 * @irq_data:                Irq handler function Callback data
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int ais_vfe_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t vfe_irq_handler, void *irq_data);

/*
 * ais_vfe_deinit_soc_resources()
 *
 * @Brief:                   Deinitialize SOC resources including private data
 *
 * @soc_info:                Device soc information
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int ais_vfe_deinit_soc_resources(struct cam_hw_soc_info *soc_info);

/*
 * ais_vfe_enable_soc_resources()
 *
 * @brief:                   Enable regulator, irq resources, start CPAS
 *
 * @soc_info:                Device soc information
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int ais_vfe_enable_soc_resources(struct cam_hw_soc_info *soc_info);

/*
 * ais_vfe_disable_soc_resources()
 *
 * @brief:                   Disable regulator, irq resources, stop CPAS
 *
 * @soc_info:                Device soc information
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int ais_vfe_disable_soc_resources(struct cam_hw_soc_info *soc_info);

/*
 * ais_vfe_soc_enable_clk()
 *
 * @brief:                   Enable clock with given name
 *
 * @soc_info:                Device soc information
 * @clk_name:                Name of clock to enable
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int ais_vfe_soc_enable_clk(struct cam_hw_soc_info *soc_info,
	const char *clk_name);

/*
 * ais_vfe_soc_disable_dsp_clk()
 *
 * @brief:                   Disable clock with given name
 *
 * @soc_info:                Device soc information
 * @clk_name:                Name of clock to enable
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int ais_vfe_soc_disable_clk(struct cam_hw_soc_info *soc_info,
	const char *clk_name);

#endif /* _AIS_VFE_SOC_H_ */
