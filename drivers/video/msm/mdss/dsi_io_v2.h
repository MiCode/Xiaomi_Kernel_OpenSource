/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef DSI_IO_V2_H
#define DSI_IO_V2_H

#include "mdss_panel.h"

void msm_dsi_ahb_ctrl(int enable);

int msm_dsi_io_init(struct platform_device *dev);

void msm_dsi_io_deinit(void);

int msm_dsi_clk_init(struct platform_device *dev);

void msm_dsi_clk_deinit(void);

int msm_dsi_prepare_clocks(void);

int msm_dsi_unprepare_clocks(void);

int msm_dsi_clk_set_rate(unsigned long esc_rate,
			unsigned long dsi_rate,
			unsigned long byte_rate,
			unsigned long pixel_rate);

int msm_dsi_clk_enable(void);

int msm_dsi_clk_disable(void);

int msm_dsi_regulator_init(struct platform_device *dev);

void msm_dsi_regulator_deinit(void);

int msm_dsi_regulator_enable(void);

int msm_dsi_regulator_disable(void);

int msm_dsi_phy_init(unsigned char *ctrl_base,
			struct mdss_panel_data *pdata);

void msm_dsi_phy_sw_reset(unsigned char *ctrl_base);

void msm_dsi_phy_off(unsigned char *ctrl_base);
#endif /* DSI_IO_V2_H */
