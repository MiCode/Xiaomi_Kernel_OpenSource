/* Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
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

#ifndef MSM_CSIPHY_H
#define MSM_CSIPHY_H

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>
#include <media/ais/msm_ais_sensor.h>
#include "msm_sd.h"
#include "msm_camera_io_util.h"
#include "msm_camera_dt_util.h"
#include "cam_soc_api.h"

#define MAX_CSIPHY 3
#define CSIPHY_NUM_CLK_MAX  16

struct csiphy_reg_t {
	uint32_t addr;
	uint32_t data;
};

struct csiphy_reg_parms_t {
	/* MIPI CSI PHY registers */
	uint32_t mipi_csiphy_lnn_cfg1_addr;
	uint32_t mipi_csiphy_lnn_cfg2_addr;
	uint32_t mipi_csiphy_lnn_cfg3_addr;
	uint32_t mipi_csiphy_lnn_cfg4_addr;
	uint32_t mipi_csiphy_lnn_cfg5_addr;
	uint32_t mipi_csiphy_lnck_cfg1_addr;
	uint32_t mipi_csiphy_lnck_cfg2_addr;
	uint32_t mipi_csiphy_lnck_cfg3_addr;
	uint32_t mipi_csiphy_lnck_cfg4_addr;
	uint32_t mipi_csiphy_lnn_test_imp;
	uint32_t mipi_csiphy_lnn_misc1_addr;
	uint32_t mipi_csiphy_glbl_reset_addr;
	uint32_t mipi_csiphy_glbl_pwr_cfg_addr;
	uint32_t mipi_csiphy_glbl_irq_cmd_addr;
	uint32_t mipi_csiphy_hw_version_addr;
	uint32_t mipi_csiphy_interrupt_status0_addr;
	uint32_t mipi_csiphy_interrupt_mask0_addr;
	uint32_t mipi_csiphy_interrupt_mask_val;
	uint32_t mipi_csiphy_interrupt_mask_addr;
	uint32_t mipi_csiphy_interrupt_clear0_addr;
	uint32_t mipi_csiphy_interrupt_clear_addr;
	uint32_t mipi_csiphy_mode_config_shift;
	uint32_t mipi_csiphy_glbl_t_init_cfg0_addr;
	uint32_t mipi_csiphy_t_wakeup_cfg0_addr;
	uint32_t csiphy_version;
	uint32_t combo_clk_mask;
};

struct csiphy_reg_3ph_parms_t {
/*MIPI CSI PHY registers*/
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl5;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl6;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl34;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl35;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl36;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl1;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl2;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl3;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl5;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl6;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl7;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl8;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl9;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl10;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl11;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl12;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl13;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl14;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl15;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl16;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl17;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl18;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl19;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl21;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl23;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl24;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl25;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl26;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl27;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl28;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl29;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl30;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl31;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl32;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl33;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl51;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl7;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl11;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl12;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl13;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl14;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl15;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl16;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl17;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl18;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl19;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl20;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl21;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_misc1;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl0;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg1;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg2;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg3;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg4;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg5;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg6;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg7;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg8;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg9;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_ctrl15;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_test_imp;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_test_force;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_ctrl5;
	struct csiphy_reg_t mipi_csiphy_3ph_lnck_cfg1;
};

struct csiphy_ctrl_t {
	struct csiphy_reg_parms_t csiphy_reg;
	struct csiphy_reg_3ph_parms_t csiphy_3ph_reg;
};

enum msm_csiphy_state_t {
	CSIPHY_POWER_UP,
	CSIPHY_POWER_DOWN,
};

struct csiphy_device {
	struct platform_device *pdev;
	struct msm_sd_subdev msm_sd;
	struct v4l2_subdev subdev;
	struct resource *irq;
	void __iomem *base;
	void __iomem *clk_mux_base;
	struct mutex mutex;
	uint32_t hw_version;
	uint32_t hw_dts_version;
	enum msm_csiphy_state_t csiphy_state;
	struct csiphy_ctrl_t *ctrl_reg;
	size_t num_all_clk;
	struct clk **csiphy_all_clk;
	struct msm_cam_clk_info *csiphy_all_clk_info;
	uint32_t num_clk;
	struct clk *csiphy_clk[CSIPHY_NUM_CLK_MAX];
	struct msm_cam_clk_info csiphy_clk_info[CSIPHY_NUM_CLK_MAX];
	struct clk *csiphy_3p_clk[2];
	struct msm_cam_clk_info csiphy_3p_clk_info[2];
	unsigned char csi_3phase;
	int32_t ref_count;
	uint16_t lane_mask[MAX_CSIPHY];
	uint32_t is_3_1_20nm_hw;
	uint32_t csiphy_clk_index;
	uint32_t csiphy_max_clk;
	uint8_t csiphy_3phase;
	uint8_t num_irq_registers;
	uint32_t csiphy_sof_debug;
	uint32_t csiphy_sof_debug_count;
	uint32_t is_combo_mode;
	struct camera_vreg_t *csiphy_vreg;
	struct regulator *csiphy_reg_ptr[MAX_REGULATOR];
	int32_t regulator_count;
	struct msm_camera_csiphy_params csiphy_params;
};

#define VIDIOC_MSM_CSIPHY_RELEASE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 9, void *)
#endif
