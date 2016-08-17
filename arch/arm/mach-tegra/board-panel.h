/*
 * arch/arm/mach-tegra/board-panel.h
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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

#define    BOARD_E1582    0x062e
#define    BOARD_E1605    0x0645
#define    BOARD_E1577    0x0629

#define    BOARD_E1627    0x065b
#define    BOARD_E1639    0x0667
#define    BOARD_E1631    0x065f


struct tegra_panel {
	void (*init_sd_settings)(struct tegra_dc_sd_settings *);
	void (*init_dc_out)(struct tegra_dc_out *);
	void (*init_fb_data)(struct tegra_fb_data *);
	void (*init_cmu_data)(struct tegra_dc_platform_data *);
	void (*set_disp_device)(struct platform_device *);
	void (*init_resources)(struct resource *, int n_resources);
	int (*register_bl_dev)(void);
	int (*register_i2c_bridge)(void);

};

extern atomic_t sd_brightness;
#ifdef CONFIG_TEGRA_DC
	extern atomic_t display_ready;
#else
	static __maybe_unused atomic_t display_ready = ATOMIC_INIT(1);
#endif
extern struct tegra_panel dsi_l_720p_5;
extern struct tegra_panel dsi_j_720p_4_7;
extern struct tegra_panel dsi_s_1080p_5;
extern struct tegra_panel dsi_p_wuxga_10_1;
extern struct tegra_panel dsi_a_1080p_11_6;
extern struct tegra_panel dsi_s_wqxga_10_1;
extern struct tegra_panel dsi_lgd_wxga_7_0;

int tegra_init_hdmi(struct platform_device *pdev,
		     struct platform_device *phost1x);
