/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Xiaomi Inc.
 */

#ifndef __MI_PANEL_EXT_H__
#define __MI_PANEL_EXT_H__

#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <drm/mediatek_drm.h>
#include "../mtk_panel_ext.h"

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[130];
};

struct LCM_param_read_write {
	unsigned int read_count;
	unsigned char read_buffer[64];
};

struct LCM_mipi_read_write {
	unsigned int read_enable;
	unsigned int read_count;
	unsigned char read_buffer[80];
	struct LCM_setting_table lcm_setting_table;
};

struct LCM_led_i2c_read_write {
	unsigned int read_enable;
	unsigned int read_count;
	unsigned char buffer[64];
};

enum KTZ8863A_REG_RW {
	KTZ8863A_REG_WRITE,
	KTZ8863A_REG_READ,
};

extern struct mi_disp_notifier g_notify_data;
extern struct LCM_mipi_read_write lcm_mipi_read_write;
extern struct LCM_led_i2c_read_write lcm_led_i2c_read_write;

int mtk_ddic_dsi_read_cmd(struct mtk_ddic_dsi_msg *cmd_msg);
int mtk_ddic_dsi_send_cmd(struct mtk_ddic_dsi_msg *cmd_msg, bool blocking, bool queueing);
int mtk_ddic_dsi_send_cmd_queue(struct mtk_ddic_dsi_msg *cmd_msg, bool blocking, bool queueing, bool need_lock);
int mtk_ddic_dsi_wait_te_send_cmd(struct mtk_ddic_dsi_msg *cmd_msg, bool blocking);

#endif
