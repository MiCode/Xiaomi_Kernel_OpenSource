/*
 * arch/arm/mach-tegra/board-panel.h
 *
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MACH_TEGRA_BOARD_PANEL_H
#define __MACH_TEGRA_BOARD_PANEL_H

#include <linux/platform_device.h>
#include "tegra-board-id.h"

struct tegra_panel {
	void (*init_sd_settings)(struct tegra_dc_sd_settings *);
	void (*init_dc_out)(struct tegra_dc_out *);
	void (*init_fb_data)(struct tegra_fb_data *);
	void (*init_cmu_data)(struct tegra_dc_platform_data *);
	void (*set_disp_device)(struct platform_device *);
	int (*register_bl_dev)(void);
	int (*register_i2c_bridge)(void);
};

enum {
	TEGRA_GPIO_RESET,
	TEGRA_GPIO_BL_ENABLE,
	TEGRA_GPIO_PWM,
	TEGRA_GPIO_TE,
	TEGRA_N_GPIO_PANEL, /* add new gpio above this entry */
};

/* tegra_panel_of will replace tegra_panel once we completely move to DT */
struct tegra_panel_of {
	int panel_gpio[TEGRA_N_GPIO_PANEL];
	bool panel_gpio_populated;
};
static struct tegra_panel_of __maybe_unused panel_of = {
	.panel_gpio = {-1, -1, -1, -1},
};

struct tegra_panel_ops {
	int (*enable)(struct device *);
	int (*postpoweron)(struct device *);
	int (*prepoweroff)(void);
	int (*disable)(void);
	int (*hotplug_init)(struct device *);
	int (*postsuspend)(void);
	void (*hotplug_report)(bool);
};
extern struct tegra_panel_ops dsi_p_wuxga_10_1_ops;
extern struct tegra_panel_ops dsi_lgd_wxga_7_0_ops;

extern struct tegra_panel dsi_l_720p_5;
extern struct tegra_panel dsi_j_720p_4_7;
extern struct tegra_panel dsi_s_1080p_5;
extern struct tegra_panel dsi_p_wuxga_10_1;
extern struct tegra_panel dsi_a_1080p_11_6;
extern struct tegra_panel dsi_s_wqxga_10_1;
extern struct tegra_panel dsi_lgd_wxga_7_0;
extern struct tegra_panel dsi_a_1080p_14_0;
extern struct tegra_panel edp_a_1080p_14_0;
extern struct tegra_panel edp_s_wqxgap_15_6;
extern struct tegra_panel edp_s_uhdtv_15_6;
extern struct tegra_panel lvds_c_1366_14;
extern struct tegra_panel dsi_l_720p_5_loki;
extern struct tegra_panel dsi_j_1440_810_5_8;
extern struct tegra_panel dsi_j_720p_5;
extern struct tegra_panel dsi_a_1200_1920_7_0;
extern struct tegra_panel dsi_a_1200_800_8_0;

void tegra_dsi_resources_init(u8 dsi_instance,
			struct resource *resources, int n_resources);

void tegra_dsi_update_init_cmd_gpio_rst(struct tegra_dc_out *dsi_disp1_out);

int tegra_panel_gpio_get_dt(const char *comp_str,
				struct tegra_panel_of *panel);

int tegra_panel_reset(struct tegra_panel_of *panel, unsigned int delay_ms);

int tegra_init_hdmi(struct platform_device *pdev,
			struct platform_device *phost1x);
#endif /* __MACH_TEGRA_BOARD_PANEL_H */
