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

#ifndef DSI_CONFIG_H_
#define DSI_CONFIG_H_

#include <linux/kernel.h>
#include <linux/mutex.h>

#include <video/adf.h>
#include "core/common/intel_dc_regs.h"

/*DSI data lane configuration*/
enum {
	DSI_DATA_LANE_4_0 = 0,
	DSI_DATA_LANE_3_1 = 1,
	DSI_DATA_LANE_2_2 = 2,
};

enum {
	RESET_FROM_BOOT_UP = 0,
	RESET_FROM_OSPM_RESUME,
};

enum {
	DSI_VIDEO_NON_BURST_MODE_SYNC_PULSE = 1,
	DSI_VIDEO_NON_BURST_MODE_SYNC_EVENTS = 2,
	DSI_VIDEO_BURST_MODE = 3,
};

typedef enum {
	DSI_DBI = 0,
	DSI_DPI,
} dsi_type_t;


enum {
	DSI_PANEL_SINGLE_LINK = 0,
	DSI_PANEL_DUAL_LINK = 1,
	DSI_PANEL_DUAL_PANEL = 2,
};

#define NO_GAMMA_CSC			0x0
#define ENABLE_GAMMA			(0x1 << 0)
#define ENABLE_CSC			(0x1 << 1)
#define ENABLE_GAMMA_CSC		(ENABLE_GAMMA | ENABLE_CSC)

struct dsi_registers {
	u32 vgacntr_reg;

	/*plane*/
	u32 dspcntr_reg;
	u32 dspsize_reg;
	u32 dspsurf_reg;
	u32 dsplinoff_reg;
	u32 dsppos_reg;
	u32 dspstride_reg;

	/*Drain Latency*/
	u32 ddl1_reg;
	u32 ddl2_reg;
	u32 ddl3_reg;
	u32 ddl4_reg;

	/*overlay*/
	u32 ovaadd_reg;
	u32 ovcadd_reg;

	/* csc */
	u32 color_coef_reg;

	/*pipe regs*/
	u32 htotal_reg;
	u32 hblank_reg;
	u32 hsync_reg;
	u32 vtotal_reg;
	u32 vblank_reg;
	u32 vsync_reg;
	u32 pipestat_reg;
	u32 pipesrc_reg;
	u32 pipeframehigh_reg;
	u32 pipeframepixel_reg;

	u32 dpll_reg;
	u32 pipeconf_reg;
	u32 palette_reg;
	u32 gamma_red_max_reg;
	u32 gamma_green_max_reg;
	u32 gamma_blue_max_reg;

	/*mipi port*/
	u32 mipi_reg;

	/*DSI controller regs*/
	u32 device_ready_reg;
	u32 intr_stat_reg;
	u32 intr_en_reg;
	u32 dsi_func_prg_reg;
	u32 hs_tx_timeout_reg;
	u32 lp_rx_timeout_reg;
	u32 turn_around_timeout_reg;
	u32 device_reset_timer_reg;
	u32 dpi_resolution_reg;
	u32 hsync_count_reg;
	u32 hbp_count_reg;
	u32 hfp_count_reg;
	u32 hactive_count_reg;
	u32 vsync_count_reg;
	u32 vbp_count_reg;
	u32 vfp_count_reg;
	u32 high_low_switch_count_reg;
	u32 dpi_control_reg;
	u32 dpi_data_reg;
	u32 init_count_reg;
	u32 max_return_pack_size_reg;
	u32 video_mode_format_reg;
	u32 eot_disable_reg;
	u32 lp_byteclk_reg;
	u32 lp_gen_data_reg;
	u32 hs_gen_data_reg;
	u32 lp_gen_ctrl_reg;
	u32 hs_gen_ctrl_reg;
	u32 gen_fifo_stat_reg;
	u32 hs_ls_dbi_enable_reg;
	u32 dphy_param_reg;
	u32 dbi_bw_ctrl_reg;
	u32 clk_lane_switch_time_cnt_reg;

	/*MIPI adapter regs*/
	u32 mipi_control_reg;
	u32 mipi_data_addr_reg;
	u32 mipi_data_len_reg;
	u32 mipi_cmd_addr_reg;
	u32 mipi_cmd_len_reg;

	/*dpst registers*/
	u32 histogram_intr_ctrl_reg;
	u32 histogram_logic_ctrl_reg;
	u32 aimg_enhance_bin_reg;
	u32 lvds_port_ctrl_reg;
};


struct dsi_context {
	u32 vgacntr;

	/*plane*/
	u32 dspcntr;
	u32 dspsize;
	u32 dspsurf;
	u32 dsppos;
	u32 dspstride;
	u32 dsplinoff;

	/*plane*/
	u32 sprite_dspcntr;
	u32 sprite_dspsize;
	u32 sprite_dspsurf;
	u32 sprite_dsppos;
	u32 sprite_dspstride;
	u32 sprite_dsplinoff;

	/*Drain Latency*/
	u32 ddl1;
	u32 ddl2;
	u32 ddl3;
	u32 ddl4;

	/*overlay*/
	u32 ovaadd;
	u32 ovcadd;

	/* gamma and csc */
	u32 palette[256];
	u32 color_coef[6];

	/*pipe regs*/
	u32 htotal;
	u32 hblank;
	u32 hsync;
	u32 vtotal;
	u32 vblank;
	u32 vsync;
	u32 pipestat;
	u32 pipesrc;
	u32 pipeframehigh;
	u32 pipeframepixel;

	u32 dpll;
	u32 fp;
	u32 pipeconf;

	/*mipi port*/
	u32 mipi;

	/*DSI controller regs*/
	u32 device_ready;
	u32 intr_stat;
	u32 intr_en;
	u32 dsi_func_prg;
	u32 hs_tx_timeout;
	u32 lp_rx_timeout;
	u32 turn_around_timeout;
	u32 device_reset_timer;
	u32 dpi_resolution;
	u32 dbi_fifo_throttle;
	u32 hsync_count;
	u32 hbp_count;
	u32 hfp_count;
	u32 hactive_count;
	u32 vsync_count;
	u32 vbp_count;
	u32 vfp_count;
	u32 high_low_switch_count;
	u32 dpi_control;
	u32 dpi_data;
	u32 init_count;
	u32 max_return_pack_size;
	u32 video_mode_format;
	u32 eot_disable;
	u32 lp_byteclk;
	u32 lp_gen_data;
	u32 hs_gen_data;
	u32 lp_gen_ctrl;
	u32 hs_gen_ctrl;
	u32 gen_fifo_stat;
	u32 hs_ls_dbi_enable;
	u32 dphy_param;
	u32 dbi_bw_ctrl;
	u32 clk_lane_switch_time_cnt;

	/*MIPI adapter regs*/
	u32 mipi_control;
	u32 mipi_data_addr;
	u32 mipi_data_len;
	u32 mipi_cmd_addr;
	u32 mipi_cmd_len;

	/*panel status*/
	int panel_on;
	int backlight_level;

	u32 pll_bypass_mode;
	u32 cck_div;
	/*brightness*/
	int lastbrightnesslevel;

	/*dpst register values*/
	u32 histogram_intr_ctrl;
	u32 histogram_logic_ctrl;
	u32 aimg_enhance_bin;
	u32 lvds_port_ctrl;

};

struct dsi_config {
	struct dsi_registers regs;
	struct dsi_context ctx;
	struct mutex ctx_lock;

	struct drm_mode_modeinfo perferred_mode;

	int pipe;
	int changed;

	int drv_ic_inited;

	int bpp;
	dsi_type_t type;
	u8 lane_count;
	u8 dual_link;

	/*mipi data lane config*/
	int lane_config;
	/*Virtual channel number for this encoder*/
	int channel_num;
	/*video mode configure*/
	int video_mode;
	int enable_gamma_csc;
	uint32_t s3d_format;

	/*dsr*/
	void *dsr;

	/* SDO */
	void *sdo;
};

static inline int is_dual_link(struct dsi_config *config)
{
	return config->dual_link == DSI_PANEL_DUAL_LINK;
}

static inline bool is_dual_panel(struct dsi_config *config)
{
	return config->dual_link == DSI_PANEL_DUAL_PANEL;
}

struct dsi_panel;

extern int dsi_config_init(struct dsi_config *config,
			struct dsi_panel *panel, u8 idx);
extern void dsi_config_destroy(struct dsi_config *config);

#endif /* DSI_CONFIG_H_ */
