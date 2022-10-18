/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_VFE_SOC_H_
#define _CAM_VFE_SOC_H_

#include "cam_soc_util.h"
#include "cam_isp_hw.h"

#define CAM_VFE_DSP_CLK_NAME "ife_dsp_clk"

#define UBWC_STATIC_CONFIG_MAX 2

/*
 * struct cam_vfe_soc_private:
 *
 * @Brief:                   Private SOC data specific to VFE HW Driver
 *
 * @cpas_handle:             Handle returned on registering with CPAS driver.
 *                           This handle is used for all further interface
 *                           with CPAS.
 * @cpas_version:            Has cpas version read from Hardware
 * @rt_wrapper_base:         Base address of the RT-Wrapper if the hw is in rt-wrapper
 * @dsp_clk_index:           DSP clk index in optional clocks
 * @ubwc_static_ctrl:        UBWC static control configuration
 * @is_ife_lite:             Flag to indicate full vs lite IFE
 * @ife_clk_src:             IFE source clock
 * @num_pid:                 Number of pids of ife
 * @pid:                     IFE pid values list
 */
struct cam_vfe_soc_private {
	uint32_t    cpas_handle;
	uint32_t    cpas_version;
	uint32_t    rt_wrapper_base;
	int32_t     dsp_clk_index;
	uint32_t    ubwc_static_ctrl[UBWC_STATIC_CONFIG_MAX];
	bool        is_ife_lite;
	uint64_t    ife_clk_src;
	uint32_t    num_pid;
	uint32_t    pid[CAM_ISP_HW_MAX_PID_VAL];
};

/*
 * cam_vfe_init_soc_resources()
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
int cam_vfe_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t vfe_irq_handler, void *irq_data);

/*
 * cam_vfe_deinit_soc_resources()
 *
 * @Brief:                   Deinitialize SOC resources including private data
 *
 * @soc_info:                Device soc information
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_vfe_deinit_soc_resources(struct cam_hw_soc_info *soc_info);

/*
 * cam_vfe_enable_soc_resources()
 *
 * @brief:                   Enable regulator, irq resources, start CPAS
 *
 * @soc_info:                Device soc information
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_vfe_enable_soc_resources(struct cam_hw_soc_info *soc_info);

/*
 * cam_vfe_disable_soc_resources()
 *
 * @brief:                   Disable regulator, irq resources, stop CPAS
 *
 * @soc_info:                Device soc information
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_vfe_disable_soc_resources(struct cam_hw_soc_info *soc_info);

/*
 * cam_vfe_soc_enable_clk()
 *
 * @brief:                   Enable clock with given name
 *
 * @soc_info:                Device soc information
 * @clk_name:                Name of clock to enable
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_vfe_soc_enable_clk(struct cam_hw_soc_info *soc_info,
	const char *clk_name);

/*
 * cam_vfe_soc_disable_dsp_clk()
 *
 * @brief:                   Disable clock with given name
 *
 * @soc_info:                Device soc information
 * @clk_name:                Name of clock to enable
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_vfe_soc_disable_clk(struct cam_hw_soc_info *soc_info,
	const char *clk_name);

#endif /* _CAM_VFE_SOC_H_ */
