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
	void (*op_mode_config)(int mode, struct mdss_panel_data *pdata);
	int (*tx)(struct mdss_panel_data *pdata,
		struct dsi_buf *tp, struct dsi_cmd_desc *cmds, int cnt);
	int (*rx)(struct mdss_panel_data *pdata,
		 struct dsi_buf *tp, struct dsi_buf *rp,
		struct dsi_cmd_desc *cmds, int len);
	int index;
	void *private;
};

int dsi_panel_device_register_v2(struct platform_device *pdev,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata);

void dsi_register_interface(struct dsi_interface *intf);

int dsi_cmds_rx_v2(struct mdss_panel_data *pdata,
			struct dsi_buf *tp, struct dsi_buf *rp,
			struct dsi_cmd_desc *cmds, int len);

int dsi_cmds_tx_v2(struct mdss_panel_data *pdata,
			struct dsi_buf *tp, struct dsi_cmd_desc *cmds,
			int cnt);

char *dsi_buf_init(struct dsi_buf *dp);

int dsi_buf_alloc(struct dsi_buf *dp, int size);

int dsi_cmd_dma_add(struct dsi_buf *dp, struct dsi_cmd_desc *cm);

int dsi_short_read1_resp(struct dsi_buf *rp);

int dsi_short_read2_resp(struct dsi_buf *rp);

int dsi_long_read_resp(struct dsi_buf *rp);

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
