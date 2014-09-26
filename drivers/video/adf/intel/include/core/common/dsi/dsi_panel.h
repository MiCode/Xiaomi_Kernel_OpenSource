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
#ifndef DSI_PANEL_H_
#define DSI_PANEL_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <drm/drm_mode.h>
#ifndef CONFIG_ADF_INTEL_VLV
#include <linux/panel_psb_drv.h>
#endif

#include "core/common/dsi/dsi_config.h"

struct dsi_pipe;

/*DSI panel connection status*/
enum {
	DSI_PANEL_CONNECTED = 0,
	DSI_PANEL_DISCONNECTED,
};

struct panel_info {
	/*panel physical size*/
	u32 width_mm;
	u32 height_mm;
	/*DBI or DPI*/
	u8 dsi_type;
	/*number of data lanes*/
	u8 lane_num;
	/*bits per pixel*/
	u8 bpp;
	/*whether need dual link mode*/
	u8 dual_link;
};

/**
 *Panel specific operations.
 *
 *@get_config_mode: return the fixed display mode of panel.
 *@update_fb: command mode panel only. update on-panel framebuffer.
 *@get_panel_info: return panel information. such as physical size.
 *@reset: panel hard reset.
 *@drv_ic_init: initialize panel driver IC and additional HW initialization.
 *@detect: return the panel physical connection status.
 *@dsi_controller_init: Initialize MIPI IP for this panel.
 *@power_on: turn on panel. e.g. send a TURN_ON special packet.
 *@power_off: turn off panel. e.g. send a SHUT_DOWN special packet.
 *
 *When adding a new panel, the driver code should implement these callbacks
 *according to corresponding panel specs. DPI and DBI implementation will
 *call these callbacks to take the specific actions for the new panel.
 */
struct panel_ops {
	int (*get_config_mode)(struct dsi_config *, struct drm_mode_modeinfo *);
	int (*dsi_controller_init)(struct dsi_pipe *intf);
	int (*get_panel_info)(struct dsi_config *, struct panel_info *);
	int (*reset)(struct dsi_pipe *intf);
	int (*exit_deep_standby)(struct dsi_pipe *intf);
	int (*detect)(struct dsi_pipe *intf);
	int (*power_on)(struct dsi_pipe *intf);
	int (*power_off)(struct dsi_pipe *intf);
	int (*set_brightness)(struct dsi_pipe *intf, int level);
	int (*drv_ic_init)(struct dsi_pipe *intf);
	int (*drv_set_panel_mode)(struct dsi_pipe *intf);
	int (*disable_panel_power)(struct dsi_pipe *intf);
	int (*enable_backlight)(struct dsi_pipe *intf);
	int (*disable_backlight)(struct dsi_pipe *intf);
};

struct dsi_panel {
	u8 panel_id;
	struct panel_info info;
	struct panel_ops *ops;
};

extern struct dsi_panel *get_dsi_panel_by_id(u8 id);
extern const struct dsi_panel *get_dsi_panel(void);

/* declare get panel callbacks */
extern const struct dsi_panel *get_generic_panel(void);
extern const struct dsi_panel *cmi_get_panel(void);
extern struct dsi_panel *jdi_cmd_get_panel(void);
extern struct dsi_panel *jdi_vid_get_panel(void);
extern struct dsi_panel *sharp_10x19_cmd_get_panel(void);
extern struct dsi_panel *sharp_10x19_dual_cmd_get_panel(void);
extern struct dsi_panel *sharp_25x16_vid_get_panel(void);
extern struct dsi_panel *sharp_25x16_cmd_get_panel(void);

#endif /* DSI_PANEL_H_ */
