/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include "mdss_panel.h"
#include "mdss_dsi_clk.h"
#include "mdss_dsi.h"

#define NONE_PANEL "none"
#define DSIPHY_CMN_CTRL_0		0x001c
#define DSIPHY_CMN_CTRL_1		0x0020
#define DSIPHY_CMN_LDO_CNTRL		0x004c
#define DSIPHY_CMN_GLBL_TEST_CTRL	0x0018
#define DSIPHY_PLL_CLKBUFLR_EN		0x041c

struct mdss_rgb_data {
	int ndx;
	bool res_init;
	struct platform_device *pdev;
	struct spi_device *spi;
	unsigned char *ctrl_base;
	struct dss_io_data ctrl_io;
	struct dss_io_data phy_io;
	int reg_size;
	u32 *dbg_bus;
	int dbg_bus_size;
	u32 hw_config; /* RGB setup configuration i.e. single*/
	u32 pll_src_config; /* PLL source selection for RGB clocks */
	u32 hw_rev; /* DSI h/w revision */
	struct mdss_panel_data panel_data;
	bool refresh_clk_rate;
	struct clk *pixel_clk_rgb;
	struct clk *byte_clk_rgb;
	u32 pclk_rate;
	u32 byte_clk_rate;

	/* DSI core regulators */
	struct dss_module_power panel_power_data;
	struct dss_module_power power_data[DSI_MAX_PM];

	/* DSI bus clocks */
	struct clk *mdp_core_clk;
	struct clk *mnoc_clk;
	struct clk *ahb_clk;
	struct clk *axi_clk;
	struct clk *mmss_misc_ahb_clk;

	struct clk *ext_pixel0_clk;
	struct clk *ext_byte0_clk;
	//struct clk *ext_pixel1_clk;
	struct clk *byte_clk_rcg;
	struct clk *pixel_clk_rcg;
	struct clk *byte0_parent;
	struct clk *pixel0_parent;

	void *clk_mngr;
	void *clk_handle;
	void *mdp_clk_handle;

	/* Data bus(AXI) scale settings */
	struct msm_bus_scale_pdata *bus_scale_table;
	u32 bus_handle;
	u32 bus_refcount;

	int bklt_ctrl;  /* backlight ctrl */
	int pwm_enabled;
	struct pwm_device *pwm_bl;
	struct dsi_pinctrl_res pin_res;

	u8 ctrl_state;
	bool core_power;

	/* GPIOs */
	int rst_gpio;

	int (*on)(struct mdss_panel_data *pdata);
	int (*off)(struct mdss_panel_data *pdata);

};

enum mdss_rgb_hw_config {
	SINGLE_RGB,
};

int mdss_rgb_panel_init(struct device_node *node,
		struct mdss_rgb_data *rgb_data);
int mdss_rgb_clk_refresh(struct mdss_rgb_data *rgb_data);
int mdss_rgb_clk_div_config(struct mdss_rgb_data *rgb_data,
		struct mdss_panel_info *panel_info, int frame_rate);
int rgb_panel_device_register(struct platform_device *ctrl_pdev,
		struct device_node *pan_node, struct mdss_rgb_data *rgb_data);
int mdss_rgb_write_command(struct mdss_rgb_data *rgb_data, u8 cmd);
int mdss_rgb_write_data(struct mdss_rgb_data *rgb_data, u8 data);
int mdss_rgb_read_command(struct mdss_rgb_data *rgb_data,
		u8 cmd, u8 *data, u8 data_len);
