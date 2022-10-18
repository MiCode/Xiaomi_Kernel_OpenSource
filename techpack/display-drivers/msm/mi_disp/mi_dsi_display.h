/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef _MI_DSI_DISPLAY_H_
#define _MI_DSI_DISPLAY_H_

#include "dsi_display.h"
#include "mi_disp_feature.h"

struct dsi_read_info {
	bool is_read_sucess;
	u32 rx_len;
	u8 rx_buf[256];
};
struct panel_manufaturer_info{
	char wp_info[16];
	char maxbrightness[16];
	char manufacturer_time[16];
	u32 wp_info_len;
	u32 max_brightness_len;
	u32 manufacturer_time_len;

};


char *get_display_power_mode_name(int power_mode);

int mi_get_disp_id(const char *display_type);

struct dsi_display * mi_get_primary_dsi_display(void);

struct dsi_display * mi_get_secondary_dsi_display(void);

int mi_dsi_display_set_disp_param(void *display,
			struct disp_feature_ctl *ctl);

int mi_dsi_display_get_disp_param(void *display,
			struct disp_feature_ctl *ctl);

ssize_t mi_dsi_display_show_disp_param(void *display,
			char *buf, size_t size);

int mi_dsi_display_write_dsi_cmd(void *display,
			struct dsi_cmd_rw_ctl *ctl);

int mi_dsi_display_read_dsi_cmd(void *display,
			struct dsi_cmd_rw_ctl *ctl);

int mi_dsi_display_set_mipi_rw(void *display,
			char *buf);

ssize_t mi_dsi_display_show_mipi_rw(void *display,
			char *buf, size_t size);

ssize_t mi_dsi_display_read_panel_info(void *display,
			char *buf, size_t size);

ssize_t mi_dsi_display_read_wp_info(void *display,
			char *buf, size_t size);

ssize_t mi_dsi_display_parse_manufacturer_info(char *outbuf, u32 outbuf_len,
			const char *inbuf,u32 offset,u32 len);

ssize_t mi_dsi_display_read_manufacturer_info(struct dsi_display *display,
			struct panel_manufaturer_info *info);

ssize_t mi_dsi_display_manufacturer_info_init(void *display);


ssize_t mi_dsi_display_read_manufacturer_info_by_globleparam(void *display,
			char *buf,size_t size);

ssize_t mi_dsi_display_read_manufacturer_struct_by_globleparam(void *display,
			struct panel_manufaturer_info *manufaturer_info);


ssize_t mi_dsi_display_read_gray_scale_info(void *display,
			char *buf, size_t size);

int mi_dsi_display_get_fps(void *display, u32 *fps);

int mi_dsi_display_set_doze_brightness(void *display,
			u32 doze_brightness);

int mi_dsi_display_get_doze_brightness(void *display,
			u32 *doze_brightness);

int mi_dsi_display_get_brightness(void *display,
			u32 *brightness);

int mi_dsi_display_write_dsi_cmd_set(void *display, int type);

ssize_t mi_dsi_display_show_dsi_cmd_set_type(void *display,
			char *buf, size_t size);

int mi_dsi_display_set_brightness_clone(void *display,
			u32 brightness_clone);

int mi_dsi_display_get_brightness_clone(void *display,
			u32 *brightness_clone);


int mi_dsi_display_get_max_brightness_clone(void *display,
			u32 *max_brightness_clone);

ssize_t mi_dsi_display_get_hw_vsync_info(void *display,
			char *buf, size_t size);

ssize_t mi_dsi_display_read_cell_id(void *display,
			char *buf, size_t size);

int mi_dsi_display_esd_irq_ctrl(struct dsi_display *display,
			bool enable);

void mi_dsi_display_wakeup_pending_doze_work(struct dsi_display *display);

struct drm_panel *mi_of_drm_find_panel_for_touch(const struct device_node *np);

#endif /*_MI_DSI_DISPLAY_H_*/
