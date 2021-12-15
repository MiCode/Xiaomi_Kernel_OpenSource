/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_DSI_DISPLAY_H_
#define _MI_DSI_DISPLAY_H_

#include "dsi_display.h"
#include "mi_disp_feature.h"

char *get_display_power_mode_name(int power_mode);

int mi_get_disp_id(struct dsi_display *display);

struct dsi_display * mi_get_primary_dsi_display(void);

struct dsi_display * mi_get_secondary_dsi_display(void);

int mi_dsi_display_set_disp_param(void *display,
			struct disp_feature_ctl *ctl);

ssize_t mi_dsi_display_get_disp_param(void *display,
			char *buf, size_t size);

int mi_dsi_display_write_mipi_reg(void *display,
			char *buf);

ssize_t mi_dsi_display_read_mipi_reg(void *display,
			char *buf, size_t size);

int mi_dsi_display_read_gamma_param(void *display);

ssize_t mi_dsi_display_print_gamma_param(void *display,
			char *buf, size_t size);

ssize_t mi_dsi_display_read_panel_info(void *display,
			char *buf, size_t size);

ssize_t mi_dsi_display_read_wp_info(void *display,
			char *buf, size_t size);

int mi_dsi_display_get_fps(void *display, u32 *fps);

int mi_dsi_display_set_doze_brightness(void *display,
			u32 doze_brightness);

int mi_dsi_display_get_doze_brightness(void *display,
			u32 *doze_brightness);

int mi_dsi_display_get_brightness(void *display,
			u32 *brightness);

int mi_dsi_display_write_dsi_cmd(void *display,
			struct dsi_cmd_rw_ctl *ctl);

int mi_dsi_display_read_dsi_cmd(void *display,
			struct dsi_cmd_rw_ctl *ctl);

int mi_dsi_display_write_dsi_cmd_set(void *display, int type);

ssize_t mi_dsi_display_show_dsi_cmd_set_type(void *display,
			char *buf, size_t size);

int mi_dsi_display_set_brightness_clone(void *display,
			u32 brightness_clone);

int mi_dsi_display_get_brightness_clone(void *display,
			u32 *brightness_clone);

ssize_t mi_dsi_display_get_hw_vsync_info(void *display,
			char *buf, size_t size);

int mi_dsi_display_esd_irq_ctrl(struct dsi_display *display,
			bool enable);

void mi_dsi_display_wakeup_pending_doze_work(struct dsi_display *display);

bool mi_is_doze_full_brightness_supported(void *display);

void mi_dsi_display_update_backlight(struct dsi_display *display);

int mi_dsi_display_read_nvt_bic(void *display);

char *mi_dsi_display_get_bic_data_info(void *display, int * bic_len);

char *mi_dsi_display_get_bic_reg_data_array(void *display);

ssize_t mi_dsi_display_cell_id_read(void *display,
			char *buf, size_t size);

#endif /*_MI_DSI_DISPLAY_H_*/
