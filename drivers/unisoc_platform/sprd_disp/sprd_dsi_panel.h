/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_DSI_PANEL_H_
#define _SPRD_DSI_PANEL_H_

#include <linux/backlight.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

enum {
	CMD_CODE_INIT = 0,
	CMD_CODE_SLEEP_IN,
	CMD_CODE_SLEEP_OUT,
	CMD_OLED_BRIGHTNESS,
	CMD_OLED_REG_LOCK,
	CMD_OLED_REG_UNLOCK,
	CMD_CODE_DOZE_IN,
	CMD_CODE_DOZE_OUT,
	CMD_CODE_RESERVED2,
	CMD_CODE_RESERVED3,
	CMD_CODE_RESERVED4,
	CMD_CODE_RESERVED5,
	CMD_CODE_MAX,
};

enum {
	SPRD_DSI_MODE_CMD = 0,
	SPRD_DSI_MODE_VIDEO_BURST,
	SPRD_DSI_MODE_VIDEO_SYNC_PULSE,
	SPRD_DSI_MODE_VIDEO_SYNC_EVENT,
};

enum {
	ESD_MODE_REG_CHECK,
	ESD_MODE_TE_CHECK,
	ESD_MODE_MIX_CHECK,
};

struct dsi_cmd_desc {
	u8 data_type;
	u8 wait;
	u8 wc_h;
	u8 wc_l;
	u8 payload[];
};

struct gpio_timing {
	u32 level;
	u32 delay;
};

struct reset_sequence {
	u32 items;
	struct gpio_timing *timing;
};

struct panel_info {
	/* common parameters */
	struct device_node *of_node;
	struct drm_display_mode mode;
	struct drm_display_mode *buildin_modes;
	int num_buildin_modes;
	int display_mode_count;
	struct gpio_desc *avdd_gpio;
	struct gpio_desc *avee_gpio;
	struct gpio_desc *reset_gpio;
	bool gpio_request_result;
	struct reset_sequence rst_on_seq;
	struct reset_sequence rst_off_seq;
	const void *cmds[CMD_CODE_MAX];
	int cmds_len[CMD_CODE_MAX];

	/* esd check parameters*/
	bool esd_check_en;
	u8 esd_check_mode;
	u16 esd_check_period;
	u32 esd_check_reg;
	u32 esd_check_val;

	/* MIPI DSI specific parameters */
	u32 format;
	u32 lanes;
	u32 hs_rate;
	u32 lp_rate;
	u32 mode_flags;
	bool use_dcs;
};

struct sprd_panel {
	struct device dev;
	struct drm_panel base;
	struct mipi_dsi_device *slave;
	struct panel_info info;
	char lcd_name[50];
	struct backlight_device *backlight;
	struct regulator *supply;
	struct delayed_work esd_work;
	bool esd_work_pending;
	struct mutex lock;
	bool enabled;
	bool is_esd_rst;
};

struct sprd_oled {
	struct backlight_device *bdev;
	struct sprd_panel *panel;
	struct dsi_cmd_desc *cmds[256];
	int cmd_len;
	int cmds_total;
	int max_level;
};

int sprd_panel_parse_lcddtb(struct device_node *lcd_node,
	struct sprd_panel *panel);
void  sprd_panel_enter_doze(struct drm_panel *p);
void  sprd_panel_exit_doze(struct drm_panel *p);

#endif /* _SPRD_DSI_PANEL_H_ */
