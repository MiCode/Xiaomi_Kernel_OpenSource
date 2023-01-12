/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SFE_SOC_H_
#define _CAM_SFE_SOC_H_

#include "cam_soc_util.h"
#include "cam_isp_hw.h"

/*
 * struct cam_sfe_soc_private:
 *
 * @Brief:                   Private SOC data specific to SFE HW Driver
 *
 * @cpas_handle:             Handle returned on registering with CPAS driver
 *                           This handle is used for all further interface
 *                           with CPAS.
 * @cpas_version:            CPAS version
 */
struct cam_sfe_soc_private {
	uint32_t    cpas_handle;
	uint32_t    cpas_version;
};

/*
 * cam_sfe_init_soc_resources()
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
int cam_sfe_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t sfe_irq_handler, void *irq_data);

/*
 * cam_sfe_deinit_soc_resources()
 *
 * @Brief:                   Deinitialize SOC resources including private data
 *
 * @soc_info:                Device soc information
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_sfe_deinit_soc_resources(struct cam_hw_soc_info *soc_info);

/*
 * cam_sfe_enable_soc_resources()
 *
 * @brief:                   Enable regulator, irq resources, Clocks
 *
 * @soc_info:                Device soc information
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_sfe_enable_soc_resources(struct cam_hw_soc_info *soc_info);

/*
 * cam_sfe_disable_soc_resources()
 *
 * @brief:                   Disable regulator, irq resources, Clocks
 *
 * @soc_info:                Device soc information
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_sfe_disable_soc_resources(struct cam_hw_soc_info *soc_info);

/*
 * cam_sfe_soc_enable_clk()
 *
 * @brief:                   Enable clock wsfe given name
 *
 * @soc_info:                Device soc information
 * @clk_name:                Name of clock to enable
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
extern int cam_sfe_soc_enable_clk(struct cam_hw_soc_info *soc_info,
	const char *clk_name);

/*
 * cam_sfe_soc_disable_clk()
 *
 * @brief:                   Disable clock wsfe given name
 *
 * @soc_info:                Device soc information
 * @clk_name:                Name of clock to enable
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_sfe_soc_disable_clk(struct cam_hw_soc_info *soc_info,
	const char *clk_name);

#endif /* _CAM_SFE_SOC_H_ */
