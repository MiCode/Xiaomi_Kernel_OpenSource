/* Copyright (c) 2008-2012, The Linux Foundation. All rights reserved.
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

#ifndef MSM_FB_PANEL_H
#define MSM_FB_PANEL_H

#include "msm_fb_def.h"

struct msm_fb_data_type;

typedef void (*msm_fb_vsync_handler_type) (void *arg);

/* panel id type */
typedef struct panel_id_s {
	uint16 id;
	uint16 type;
} panel_id_type;

/* panel type list */
#define NO_PANEL		0xffff	/* No Panel */
#define MDDI_PANEL		1	/* MDDI */
#define EBI2_PANEL		2	/* EBI2 */
#define LCDC_PANEL		3	/* internal LCDC type */
#define EXT_MDDI_PANEL		4	/* Ext.MDDI */
#define TV_PANEL		5	/* TV */
#define HDMI_PANEL		6	/* HDMI TV */
#define DTV_PANEL		7	/* DTV */
#define MIPI_VIDEO_PANEL	8	/* MIPI */
#define MIPI_CMD_PANEL		9	/* MIPI */
#define WRITEBACK_PANEL		10	/* Wifi display */
#define LVDS_PANEL		11	/* LVDS */

/* panel class */
typedef enum {
	DISPLAY_LCD = 0,	/* lcd = ebi2/mddi */
	DISPLAY_LCDC,		/* lcdc */
	DISPLAY_TV,		/* TV Out */
	DISPLAY_EXT_MDDI,	/* External MDDI */
} DISP_TARGET;

/* panel device locaiton */
typedef enum {
	DISPLAY_1 = 0,		/* attached as first device */
	DISPLAY_2,		/* attached on second device */
	DISPLAY_3,              /* attached on third writeback device */
	MAX_PHYS_TARGET_NUM,
} DISP_TARGET_PHYS;

enum {
	BLT_SWITCH_TG_OFF,
	BLT_SWITCH_TG_ON
};

/* panel info type */
struct lcd_panel_info {
	__u32 vsync_enable;
	__u32 refx100;
	__u32 v_back_porch;
	__u32 v_front_porch;
	__u32 v_pulse_width;
	__u32 hw_vsync_mode;
	__u32 vsync_notifier_period;
	__u32 blt_ctrl;
	__u32 blt_mode;
	__u32 rev;
};

struct lcdc_panel_info {
	__u32 h_back_porch;
	__u32 h_front_porch;
	__u32 h_pulse_width;
	__u32 v_back_porch;
	__u32 v_front_porch;
	__u32 v_pulse_width;
	__u32 border_clr;
	__u32 underflow_clr;
	__u32 hsync_skew;
	/* Pad width */
	uint32 xres_pad;
	/* Pad height */
	uint32 yres_pad;
	boolean is_sync_active_high;
};

struct mddi_panel_info {
	__u32 vdopkt;
	boolean is_type1;
};

struct mipi_panel_info {
	char mode;		/* video/cmd */
	char interleave_mode;
	char crc_check;
	char ecc_check;
	char dst_format;	/* shared by video and command */
	char data_lane0;
	char data_lane1;
	char data_lane2;
	char data_lane3;
	char dlane_swap;	/* data lane swap */
	char rgb_swap;
	char b_sel;
	char g_sel;
	char r_sel;
	char rx_eot_ignore;
	char tx_eot_append;
	char t_clk_post; /* 0xc0, DSI_CLKOUT_TIMING_CTRL */
	char t_clk_pre;  /* 0xc0, DSI_CLKOUT_TIMING_CTRL */
	char vc;	/* virtual channel */
	struct mipi_dsi_phy_ctrl *dsi_phy_db;
	/* video mode */
	char pulse_mode_hsa_he;
	char hfp_power_stop;
	char hbp_power_stop;
	char hsa_power_stop;
	char eof_bllp_power_stop;
	char bllp_power_stop;
	char traffic_mode;
	char frame_rate;
	/* command mode */
	char interleave_max;
	char insert_dcs_cmd;
	char wr_mem_continue;
	char wr_mem_start;
	char te_sel;
	char stream;	/* 0 or 1 */
	char mdp_trigger;
	char dma_trigger;
	uint32 dsi_pclk_rate;
	/* byte to esc clk ratio */
	uint32 esc_byte_ratio;
	/* The packet-size should not bet changed */
	char no_max_pkt_size;
	/* Clock required during LP commands */
	char force_clk_lane_hs;
};

enum lvds_mode {
	LVDS_SINGLE_CHANNEL_MODE,
	LVDS_DUAL_CHANNEL_MODE,
};

struct lvds_panel_info {
	enum lvds_mode channel_mode;
	/* Channel swap in dual mode */
	char channel_swap;
};

struct msm_panel_info {
	__u32 xres;
	__u32 yres;
	__u32 bpp;
	__u32 mode2_xres;
	__u32 mode2_yres;
	__u32 mode2_bpp;
	__u32 type;
	__u32 wait_cycle;
	DISP_TARGET_PHYS pdest;
	__u32 bl_max;
	__u32 bl_min;
	__u32 fb_num;
	__u32 clk_rate;
	__u32 clk_min;
	__u32 clk_max;
	__u32 frame_count;
	__u32 is_3d_panel;
	__u32 frame_rate;
	__u32 frame_interval;

	struct mddi_panel_info mddi;
	struct lcd_panel_info lcd;
	struct lcdc_panel_info lcdc;
	struct mipi_panel_info mipi;
	struct lvds_panel_info lvds;
};

#define MSM_FB_SINGLE_MODE_PANEL(pinfo)		\
	do {					\
		(pinfo)->mode2_xres = 0;	\
		(pinfo)->mode2_yres = 0;	\
		(pinfo)->mode2_bpp = 0;		\
	} while (0)

struct msm_fb_panel_data {
	struct msm_panel_info panel_info;
	void (*set_rect) (int x, int y, int xres, int yres);
	void (*set_vsync_notifier) (msm_fb_vsync_handler_type, void *arg);
	void (*set_backlight) (struct msm_fb_data_type *);

	/* function entry chain */
	int (*on) (struct platform_device *pdev);
	int (*off) (struct platform_device *pdev);
	int (*late_init) (struct platform_device *pdev);
	int (*power_ctrl) (boolean enable);
	struct platform_device *next;
	int (*clk_func) (int enable);
};

enum {
	MDP4_OVERLAY_BLT_SWITCH_TG_OFF,
	MDP4_OVERLAY_BLT_SWITCH_TG_ON,
	MDP4_OVERLAY_BLT_SWITCH_POLL
};

enum {
	MDP4_OVERLAY_MODE_BLT_CTRL,
	MDP4_OVERLAY_MODE_BLT_ALWAYS_ON,
	MDP4_OVERLAY_MODE_BLT_ALWAYS_OFF
};

/*===========================================================================
  FUNCTIONS PROTOTYPES
============================================================================*/
struct platform_device *msm_fb_device_alloc(struct msm_fb_panel_data *pdata,
						u32 type, u32 id);
int panel_next_on(struct platform_device *pdev);
int panel_next_off(struct platform_device *pdev);
int panel_next_late_init(struct platform_device *pdev);

int lcdc_device_register(struct msm_panel_info *pinfo);

int mddi_toshiba_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel);

#endif /* MSM_FB_PANEL_H */
