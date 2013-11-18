/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef DSI_V2_H
#define DSI_V2_H

#include <linux/list.h>
#include <mach/scm-io.h>

#include "mdss_dsi.h"
#include "mdss_panel.h"

#define DSI_BUF_SIZE	1024
#define DSI_MRPS	0x04  /* Maximum Return Packet Size */

struct dsi_interface {
	int (*on)(struct mdss_panel_data *pdata);
	int (*off)(struct mdss_panel_data *pdata);
	int (*cont_on)(struct mdss_panel_data *pdata);
	int (*clk_ctrl)(struct mdss_panel_data *pdata, int enable);
	void (*op_mode_config)(int mode, struct mdss_panel_data *pdata);
	int index;
	void *private;
};

int dsi_panel_device_register_v2(struct platform_device *pdev,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata);

void dsi_register_interface(struct dsi_interface *intf);

int dsi_buf_alloc(struct dsi_buf *dp, int size);

void dsi_set_tx_power_mode(int mode);

void dsi_ctrl_config_deinit(struct platform_device *pdev,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata);

int dsi_ctrl_config_init(struct platform_device *pdev,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata);

int dsi_ctrl_gpio_request(struct mdss_dsi_ctrl_pdata *ctrl_pdata);

void dsi_ctrl_gpio_free(struct mdss_dsi_ctrl_pdata *ctrl_pdata);

struct mdss_panel_cfg *mdp3_panel_intf_type(int intf_val);

int mdp3_panel_get_boot_cfg(void);

#endif /* DSI_V2_H */
