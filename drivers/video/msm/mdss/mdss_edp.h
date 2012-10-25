/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef MDSS_EDP_H
#define MDSS_EDP_H

#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>

#include "mdss_panel.h"

#define edp_read(offset) readl_relaxed((offset))
#define edp_write(offset, data) writel_relaxed((data), (offset))

struct display_timing_desc {
	u32 pclk;
	u32 h_addressable; /* addressable + boder = active */
	u32 h_border;
	u32 h_blank;	/* fporch + bporch + sync_pulse = blank */
	u32 h_fporch;
	u32 h_sync_pulse;
	u32 v_addressable; /* addressable + boder = active */
	u32 v_border;
	u32 v_blank;	/* fporch + bporch + sync_pulse = blank */
	u32 v_fporch;
	u32 v_sync_pulse;
	u32 width_mm;
	u32 height_mm;
	u32 interlaced;
	u32 stereo;
	u32 sync_type;
	u32 sync_separate;
	u32 vsync_pol;
	u32 hsync_pol;
};

struct edp_edid {
	char id_name[4];
	short id_product;
	char version;
	char revision;
	char video_digital;
	char color_depth;	/* 6, 8, 10, 12 and 14 bits */
	char color_format;	/* RGB 4:4:4, YCrCb 4:4:4, Ycrcb 4:2:2 */
	char dpm;		/* display power management */
	char sync_digital;	/* 1 = digital */
	char sync_separate;	/* 1 = separate */
	char vsync_pol;		/* 0 = negative, 1 = positive */
	char hsync_pol;		/* 0 = negative, 1 = positive */
	char ext_block_cnt;
	struct display_timing_desc timing[4];
};

struct dpcd_cap {
	char max_lane_count;
	u32 max_link_clk;  /* 162, 270 and 540 Mb, divided by 10 */
};

struct mdss_edp_drv_pdata {
	/* device driver */
	int (*on) (struct mdss_panel_data *pdata);
	int (*off) (struct mdss_panel_data *pdata);
	struct platform_device *pdev;

	/* edp specific */
	struct mdss_panel_data panel_data;
	unsigned char *edp_base;
	unsigned char *mmss_cc_base;
	struct edp_edid edid;
	struct dpcd_cap dpcd;

	/* regulators */
	struct regulator *vdda_vreg;

	/* clocks */
	struct clk *aux_clk;
	struct clk *pixel_clk;
	struct clk *ahb_clk;
	struct clk *link_clk;
	int clk_on;

	/* gpios */
	int gpio_panel_en;
	int gpio_panel_pwm;

	/* backlight */
	struct pwm_device *bl_pwm;
	int lpg_channel;
	int pwm_period;
};

void mdss_edp_phy_sw_reset(unsigned char *edp_base);
void mdss_edp_pll_configure(unsigned char *edp_base, int rate);
void mdss_edp_enable_lane_bist(unsigned char *edp_base, int lane, int enable);
void mdss_edp_enable_mainlink(unsigned char *edp_base, int enable);
void mdss_edp_hw_powerup(unsigned char *edp_base, int enable);
void mdss_edp_clk_enable(struct mdss_edp_drv_pdata *edp_drv);
void mdss_edp_clk_disable(struct mdss_edp_drv_pdata *edp_drv);
int mdss_edp_clk_init(struct mdss_edp_drv_pdata *edp_drv);
void mdss_edp_clk_deinit(struct mdss_edp_drv_pdata *edp_drv);
void mdss_edp_prepare_clocks(struct mdss_edp_drv_pdata *edp_drv);
void mdss_edp_unprepare_clocks(struct mdss_edp_drv_pdata *edp_drv);
void mdss_edp_config_clk(unsigned char *edp_base, unsigned char *mmss_cc_base);
void mdss_edp_unconfig_clk(unsigned char *edp_base,
		unsigned char *mmss_cc_base);
void mdss_edp_phy_misc_cfg(unsigned char *edp_base);

#endif /* MDSS_EDP_H */
