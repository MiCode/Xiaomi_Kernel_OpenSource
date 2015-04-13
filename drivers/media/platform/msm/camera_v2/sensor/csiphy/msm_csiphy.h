/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
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
#include <media/msm_cam_sensor.h>
#include "msm_sd.h"
#include "msm_camera_io_util.h"

#define MAX_CSIPHY 3
#define CSIPHY_NUM_CLK_MAX  16

struct csiphy_reg_parms_t {
/*MIPI CSI PHY registers*/
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
};

struct csiphy_reg_3ph_parms_t {
/*MIPI CSI PHY registers*/
	uint32_t mipi_csiphy_3ph_cmn_ctrl5_addr;
	uint32_t mipi_csiphy_3ph_cmn_ctrl6_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl34_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl35_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl36_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl2_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl3_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl6_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl7_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl8_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl9_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl10_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl11_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl17_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl24_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl51_addr;
	uint32_t mipi_csiphy_3ph_lnn_ctrl25_addr;
	uint32_t mipi_csiphy_3ph_cmn_ctrl7_addr;
	uint32_t mipi_csiphy_3ph_cmn_ctrl11_addr;
	uint32_t mipi_csiphy_3ph_cmn_ctrl12_addr;
	uint32_t mipi_csiphy_3ph_cmn_ctrl13_addr;
	uint32_t mipi_csiphy_3ph_cmn_ctrl14_addr;
	uint32_t mipi_csiphy_3ph_cmn_ctrl15_addr;
	uint32_t mipi_csiphy_3ph_cmn_ctrl16_addr;
	uint32_t mipi_csiphy_3ph_cmn_ctrl17_addr;
	uint32_t mipi_csiphy_3ph_cmn_ctrl18_addr;
	uint32_t mipi_csiphy_3ph_cmn_ctrl19_addr;
	uint32_t mipi_csiphy_3ph_cmn_ctrl20_addr;
	uint32_t mipi_csiphy_3ph_cmn_ctrl21_addr;
	uint32_t mipi_csiphy_2ph_lnn_misc1_addr;
	uint32_t mipi_csiphy_3ph_cmn_ctrl0_addr;
	uint32_t mipi_csiphy_2ph_lnn_cfg1_addr;
	uint32_t mipi_csiphy_2ph_lnn_cfg2_addr;
	uint32_t mipi_csiphy_2ph_lnn_cfg3_addr;
	uint32_t mipi_csiphy_2ph_lnn_cfg4_addr;
	uint32_t mipi_csiphy_2ph_lnn_cfg5_addr;
	uint32_t mipi_csiphy_2ph_lnn_cfg6_addr;
	uint32_t mipi_csiphy_2ph_lnn_cfg7_addr;
	uint32_t mipi_csiphy_2ph_lnn_cfg8_addr;
	uint32_t mipi_csiphy_2ph_lnn_cfg9_addr;
	uint32_t mipi_csiphy_2ph_lnn_ctrl15_addr;
	uint32_t mipi_csiphy_2ph_lnn_test_imp_addr;
	uint32_t mipi_csiphy_2ph_lnn_test_force;
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
	struct resource *mem;
	struct resource *clk_mux_mem;
	struct resource *irq;
	struct resource *io;
	struct resource *clk_mux_io;
	void __iomem *base;
	void __iomem *clk_mux_base;
	struct mutex mutex;
	uint32_t hw_version;
	uint32_t hw_dts_version;
	enum msm_csiphy_state_t csiphy_state;
	struct csiphy_ctrl_t *ctrl_reg;
	uint32_t num_clk;
	struct clk *csiphy_clk[CSIPHY_NUM_CLK_MAX];
	struct msm_cam_clk_info csiphy_clk_info[CSIPHY_NUM_CLK_MAX];
	int32_t ref_count;
	uint16_t lane_mask[MAX_CSIPHY];
	uint32_t is_3_1_20nm_hw;
	uint32_t csiphy_clk_index;
	uint32_t csiphy_max_clk;
	uint8_t csiphy_3phase;
	uint8_t num_irq_registers;
};

#define VIDIOC_MSM_CSIPHY_RELEASE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 9, void *)
#endif
