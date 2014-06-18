/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/

#ifndef DSI_PKG_SENDER_H_
#define DSI_PKG_SENDER_H_

#include <linux/kthread.h>

#if defined(CONFIG_ADF)
#include "intel_adf_device.h"
#endif
#include "core/common/intel_dc_regs.h"
#include "core/common/dsi/dsi_config.h"

#define MAX_DCS_PARAM	8
#define MAX_PKG_NUM	2048

enum {
	DSI_PKG_DCS,
	DSI_DPI_SPK,
	DSI_PKG_GEN_SHORT_WRITE_0 = 0x03,
	DSI_PKG_GEN_SHORT_WRITE_1 = 0x13,
	DSI_PKG_GEN_SHORT_WRITE_2 = 0x23,
	DSI_PKG_GEN_READ_0 = 0x04,
	DSI_PKG_GEN_READ_1 = 0x14,
	DSI_PKG_GEN_READ_2 = 0x24,
	DSI_PKG_GEN_LONG_WRITE = 0x29,
	DSI_PKG_MCS_SHORT_WRITE_0 = 0x05,
	DSI_PKG_MCS_SHORT_WRITE_1 = 0x15,
	DSI_PKG_MCS_READ = 0x06,
	DSI_PKG_MCS_LONG_WRITE = 0x39,
};

enum {
	DSI_DPI_SPK_SHUT_DOWN = BIT0,
	DSI_DPI_SPK_TURN_ON = BIT1,
	DSI_DPI_SPK_COLOR_MODE_ON = BIT2,
	DSI_DPI_SPK_COLOR_MODE_OFF = BIT3,
	DSI_DPI_SPK_BACKLIGHT_ON = BIT4,
	DSI_DPI_SPK_BACKLIGHT_OFF = BIT5,
	DSI_DPI_SPK_RESET_TRIGGER = BIT6,
};

enum {
	DSI_LP_TRANSMISSION,
	DSI_HS_TRANSMISSION,
	DSI_DCS,
};

enum {
	DSI_PANEL_MODE_SLEEP = 0x1,
};

enum {
	DSI_PKG_SENDER_FREE  = 0x0,
	DSI_PKG_SENDER_BUSY  = 0x1,
	DSI_CONTROL_ABNORMAL = 0x2,
};

enum {
	DSI_SEND_PACKAGE,
	DSI_QUEUE_PACKAGE,
};

#define CMD_MEM_ADDR_OFFSET	0

#define CMD_DATA_SRC_SYSTEM_MEM		0
#define CMD_DATA_SRC_PIPE		1

struct dsi_gen_short_pkg {
	u8 cmd;
	u8 param;
};

struct dsi_gen_long_pkg {
	u8 *data;
	u32 len;
};

struct dsi_dcs_pkg {
	u8 cmd;
	u8 *param;
	u32 param_num;
	u8 data_src;
};

struct dsi_dpi_spk_pkg { u32 cmd; };

struct dsi_pkg {
	u8 pkg_type;
	u8 transmission_type;

	union {
		struct dsi_gen_short_pkg short_pkg;
		struct dsi_gen_long_pkg long_pkg;
		struct dsi_dcs_pkg dcs_pkg;
		struct dsi_dpi_spk_pkg spk_pkg;
	} pkg;

	struct list_head entry;
};

struct dsi_pkg_sender {
	struct intel_adf_device *dev;
	u32 status;

	u32 panel_mode;

	int pipe;
	bool work_for_slave_panel;

	struct mutex lock;
	struct list_head pkg_list;
	struct list_head free_list;

	u32 pkg_num;

	int dbi_pkg_support;

	u32 dbi_cb_phy;
	void *dbi_cb_addr;

	atomic64_t te_seq;
	atomic64_t last_screen_update;

	/*registers*/
	u32 dpll_reg;
	u32 dspcntr_reg;
	u32 pipeconf_reg;
	u32 pipestat_reg;
	u32 dsplinoff_reg;
	u32 dspsurf_reg;

	u32 mipi_intr_stat_reg;
	u32 mipi_lp_gen_data_reg;
	u32 mipi_hs_gen_data_reg;
	u32 mipi_lp_gen_ctrl_reg;
	u32 mipi_hs_gen_ctrl_reg;
	u32 mipi_gen_fifo_stat_reg;
	u32 mipi_data_addr_reg;
	u32 mipi_data_len_reg;
	u32 mipi_cmd_addr_reg;
	u32 mipi_cmd_len_reg;
	u32 mipi_dpi_control_reg;
};

extern int dsi_pkg_sender_init(struct dsi_pkg_sender *sender,
	u32 gtt_phy_addr, int type, int pipe);
extern void dsi_pkg_sender_destroy(struct dsi_pkg_sender *sender);
extern int dsi_check_fifo_empty(struct dsi_pkg_sender *sender);
extern int dsi_send_dcs(struct dsi_pkg_sender *sender,
			u8 dcs, u8 *param, u32 param_num, u8 data_src,
			int delay);
extern int dsi_send_dual_dcs(struct dsi_pkg_sender *sender,
			u8 dcs, u8 *param, u32 param_num, u8 data_src,
			int delay, bool is_dual_link);
extern int dsi_send_mcs_short_hs(struct dsi_pkg_sender *sender,
					u8 cmd, u8 param, u8 param_num,
					int delay);
extern int dsi_send_mcs_short_lp(struct dsi_pkg_sender *sender,
					u8 cmd, u8 param, u8 param_num,
					int delay);
extern int dsi_send_mcs_long_hs(struct dsi_pkg_sender *sender,
					u8 *data,
					u32 len,
					int delay);
extern int dsi_send_mcs_long_lp(struct dsi_pkg_sender *sender,
					u8 *data,
					u32 len,
					int delay);
extern int dsi_send_gen_short_hs(struct dsi_pkg_sender *sender,
					u8 param0, u8 param1, u8 param_num,
					int delay);
extern int dsi_send_gen_short_lp(struct dsi_pkg_sender *sender,
					u8 param0, u8 param1, u8 param_num,
					int delay);
extern int dsi_send_gen_long_hs(struct dsi_pkg_sender *sender,
				u8 *data,
				u32 len,
				int delay);
extern int dsi_send_gen_long_lp(struct dsi_pkg_sender *sender,
				u8 *data,
				u32 len,
				int delay);
extern int dsi_send_dpi_spk_pkg_hs(struct dsi_pkg_sender *sender,
				u32 spk_pkg);
extern int dsi_send_dpi_spk_pkg_lp(struct dsi_pkg_sender *sender,
				u32 spk_pkg);
extern int dsi_cmds_kick_out(struct dsi_pkg_sender *sender);
extern void dsi_report_te(struct dsi_pkg_sender *sender);
extern int dsi_status_check(struct dsi_pkg_sender *sender);

/*read interfaces*/
extern int dsi_read_gen_hs(struct dsi_pkg_sender *sender,
			u8 param0,
			u8 param1,
			u8 param_num,
			u8 *data,
			u32 len);
extern int dsi_read_gen_lp(struct dsi_pkg_sender *sender,
			u8 param0,
			u8 param1,
			u8 param_num,
			u8 *data,
			u32 len);
extern int dsi_read_mcs_hs(struct dsi_pkg_sender *sender,
			u8 cmd,
			u8 *data,
			u32 len);
extern int dsi_read_mcs_lp(struct dsi_pkg_sender *sender,
			u8 cmd,
			u8 *data,
			u32 len);
extern int dsi_wait_for_fifos_empty(struct dsi_pkg_sender *sender);

#endif /* DSI_PKG_SENDER_H_ */
