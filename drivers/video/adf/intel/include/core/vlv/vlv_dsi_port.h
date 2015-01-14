/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _VLV_DSI_PORT_H_
#define _VLV_DSI_PORT_H_

#include <core/common/dsi/dsi_pipe.h>
#include <core/vlv/vlv_dc_config.h>

struct vlv_dsi_port {
	enum port port_id;
	u32 offset;
	u32 operation_mode;
	u32 dr_offset; /* device_ready */
	u32 eot_offset;
	u32 func_prg_offset;
	u32 fifo_stat_offset;
	u32 ctrl_offset;
	u32 dphy_param_offset;
	u32 dpi_res_offset;
	u32 hactive_count_offset;
	u32 hfp_count_offset;
	u32 hsync_count_offset;
	u32 hbp_count_offset;
	u32 vfp_count_offset;
	u32 vsync_count_offset;
	u32 vbp_count_offset;
	u32 max_ret_pkt_offset;
	u32 hs_tx_timeout_offset;
	u32 lp_rx_timeout_offset;
	u32 ta_timeout_offset;
	u32 device_reset_timer_offset;
	u32 init_count_offset;
	u32 hl_switch_count_offset;
	u32 lp_byteclk_offset;
	u32 dbi_bw_offset;
	u32 lane_switch_time_offset;
	u32 video_mode_offset;

	/* cmd mode offsets */
	bool hs_mode;
	u32 hs_ls_dbi_enable_offset;
	u32 hs_gen_ctrl_offset;
	u32 lp_gen_ctrl_offset;
	u32 hs_gen_data_offset;
	u32 lp_gen_data_offset;
	u32 dpi_ctrl_offset;

	u32 intr_stat_offset;/*FIXME: to be moved to interrupt obj */
};

u32  vlv_dsi_port_prepare(struct vlv_dsi_port *port,
		struct drm_mode_modeinfo *mode,
		struct dsi_config *config);
u32 vlv_dsi_port_pre_enable(struct vlv_dsi_port *port,
		struct drm_mode_modeinfo *mode,
		struct dsi_config *config);
u32 vlv_dsi_port_enable(struct vlv_dsi_port *port, u32 port_bits);
u32 vlv_dsi_port_disable(struct vlv_dsi_port *port,
		struct dsi_config *config);
bool vlv_dsi_port_set_device_ready(struct vlv_dsi_port *port);
bool vlv_dsi_port_clear_device_ready(struct vlv_dsi_port *port);
u32  vlv_dsi_port_wait_for_fifo_empty(struct vlv_dsi_port *port);
bool vlv_dsi_port_can_be_disabled(struct vlv_dsi_port *port);
u32 vlv_dsi_port_is_vid_mode(struct vlv_dsi_port *port);
u32 vlv_dsi_port_is_cmd_mode(struct vlv_dsi_port *port);
void vlv_dsi_port_cmd_hs_mode_enable(struct vlv_dsi_port *port, bool enable);
int vlv_dsi_port_cmd_vc_dcs_write(struct vlv_dsi_port *port, int channel,
		const u8 *data, int len);
int vlv_dsi_port_cmd_vc_generic_write(struct vlv_dsi_port *port, int channel,
		const u8 *data, int len);
int vlv_dsi_port_cmd_vc_dcs_read(struct vlv_dsi_port *port, int channel,
		u8 dcs_cmd, u8 *buf, int buflen);
int vlv_dsi_port_cmd_vc_generic_read(struct vlv_dsi_port *port, int channel,
		u8 *reqdata, int reqlen, u8 *buf, int buflen);
int vlv_dsi_port_cmd_dpi_send_cmd(struct vlv_dsi_port *port, u32 cmd, bool hs);
void vlv_dsi_port_cmd_hs_mode_enable(struct vlv_dsi_port *port,
			bool enable);
int vlv_dsi_port_cmd_vc_dcs_write(struct vlv_dsi_port *port, int channel,
			const u8 *data, int len);
int vlv_dsi_port_cmd_vc_generic_write(struct vlv_dsi_port *port, int channel,
			const u8 *data, int len);
int vlv_dsi_port_cmd_vc_dcs_read(struct vlv_dsi_port *port, int channel,
			u8 dcs_cmd, u8 *buf, int buflen);
int vlv_dsi_port_cmd_vc_generic_read(struct vlv_dsi_port *port, int channel,
			u8 *reqdata, int reqlen, u8 *buf, int buflen);
int vlv_dsi_port_cmd_dpi_send_cmd(struct vlv_dsi_port *port, u32 cmd, bool hs);


#endif /* _VLV_DSI_PORT_H_ */
