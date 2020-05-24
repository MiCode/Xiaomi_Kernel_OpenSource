// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018, 2020, The Linux Foundation. All rights reserved. */

#include <linux/of_device.h>
#include <linux/delay.h>
#include <video/mipi_display.h>
#include <linux/pwm.h>

#include "mdss_rgb.h"

#define RAMCTRL_CMD             0xb0
#define RGBCTRL_CMD             0xb1
#define PORCTRL_CMD             0xb2
#define GCTRL_CMD               0xb7
#define VCOMS_CMD               0xbb
#define LCMCTRL_CMD             0xc0
#define VDVVRHEN_CMD            0xc2
#define VRHS_CMD                0xc3
#define VDVS_CMD                0xc4
#define VCMOFSET_CMD		0xc5
#define FRCTRL2_CMD             0xc6
#define PWCTRL1_CMD             0xd0
#define PVGAMCTRL_CMD           0xe0
#define NVGAMCTRL_CMD           0xe1
#define SPI2EN_CMD		0xe7
#define PWCTRL2_CMD		0xe8

static void mdss_rgb_panel_reset(struct mdss_rgb_data *rgb_data)
{
	if (gpio_is_valid(rgb_data->rst_gpio)) {
		gpio_set_value(rgb_data->rst_gpio, 1);
		usleep_range(10000, 20000);
		gpio_set_value(rgb_data->rst_gpio, 0);
		usleep_range(10000, 20000);
		gpio_set_value(rgb_data->rst_gpio, 1);
		usleep_range(10000, 20000);
	}
}

static int mdss_rgb_panel_on(struct mdss_panel_data *pdata)
{
	struct mdss_rgb_data *rgb_data = NULL;

	if (pdata == NULL) {
		pr_err("%s: invalid mdss panel data\n", __func__);
		return -EINVAL;
	}

	rgb_data = container_of(pdata, struct mdss_rgb_data,
			panel_data);

	mdss_rgb_panel_reset(rgb_data);

	mdss_rgb_write_command(rgb_data, MIPI_DCS_EXIT_SLEEP_MODE);
	msleep(120);

	/* display and color format setting */
	mdss_rgb_write_command(rgb_data, MIPI_DCS_SET_ADDRESS_MODE);
	mdss_rgb_write_data(rgb_data, 0x00);

	mdss_rgb_write_command(rgb_data, MIPI_DCS_SET_PIXEL_FORMAT);
	mdss_rgb_write_data(rgb_data, 0x06);

	/* frame rate setting */
	/* RAM Control */
	mdss_rgb_write_command(rgb_data, RAMCTRL_CMD);
	mdss_rgb_write_data(rgb_data, 0x11);
	mdss_rgb_write_data(rgb_data, 0xf0);

	/* RGB Interface control */
	mdss_rgb_write_command(rgb_data, RGBCTRL_CMD);
	mdss_rgb_write_data(rgb_data, 0x4c);
	mdss_rgb_write_data(rgb_data, 0x05);
	mdss_rgb_write_data(rgb_data, 0x14);

	/* Porch setting */
	mdss_rgb_write_command(rgb_data, PORCTRL_CMD);
	mdss_rgb_write_data(rgb_data, 0x0c);
	mdss_rgb_write_data(rgb_data, 0x0c);
	mdss_rgb_write_data(rgb_data, 0x00);
	mdss_rgb_write_data(rgb_data, 0x33);
	mdss_rgb_write_data(rgb_data, 0x33);

	/* Gate Control */
	mdss_rgb_write_command(rgb_data, GCTRL_CMD);
	mdss_rgb_write_data(rgb_data, 0x35);
	/* VGH=13.26V, VGL=-10.43V */

	/* Power setting */
	/* vcom setting */
	mdss_rgb_write_command(rgb_data, VCOMS_CMD);
	mdss_rgb_write_data(rgb_data, 0x19);	/* 1.05 */

	/* LCM Control */
	mdss_rgb_write_command(rgb_data, LCMCTRL_CMD);
	mdss_rgb_write_data(rgb_data, 0x2c);

	/* VDV and VRH Command Enable */
	mdss_rgb_write_command(rgb_data, VDVVRHEN_CMD);
	mdss_rgb_write_data(rgb_data, 0x01);

	/* VRH Set, VAP(GVDD) & VAN(GVCL) */
	mdss_rgb_write_command(rgb_data, VRHS_CMD);
	mdss_rgb_write_data(rgb_data, 0x1d);

	/* set VDV */
	mdss_rgb_write_command(rgb_data, VDVS_CMD);
	mdss_rgb_write_data(rgb_data, 0x20);	/* VDV=0V */

	/* vcom Offset Set */
	mdss_rgb_write_command(rgb_data, VCMOFSET_CMD);
	mdss_rgb_write_data(rgb_data, 0x20);	/* vcom Offset=0V */

	/* Frame rate control in normal mode */
	mdss_rgb_write_command(rgb_data, FRCTRL2_CMD);
	mdss_rgb_write_data(rgb_data, 0x0f);	/* dot inversion & 60Hz */

	/* Power control 1 */
	mdss_rgb_write_command(rgb_data, PWCTRL1_CMD);
	mdss_rgb_write_data(rgb_data, 0xa4);
	/* AVDD=6.8V; AVCL=-4.8V; VDDS=2.3V */
	mdss_rgb_write_data(rgb_data, 0xa1);

	/* Power control 2 */
	mdss_rgb_write_command(rgb_data, PWCTRL2_CMD);
	mdss_rgb_write_data(rgb_data, 0x83);

	/* Set Gamma */
	mdss_rgb_write_command(rgb_data, PVGAMCTRL_CMD);
	mdss_rgb_write_data(rgb_data, 0xd2);
	mdss_rgb_write_data(rgb_data, 0x13);
	mdss_rgb_write_data(rgb_data, 0x16);
	mdss_rgb_write_data(rgb_data, 0x0d);
	mdss_rgb_write_data(rgb_data, 0x0f);
	mdss_rgb_write_data(rgb_data, 0x38);
	mdss_rgb_write_data(rgb_data, 0x3d);
	mdss_rgb_write_data(rgb_data, 0x43);
	mdss_rgb_write_data(rgb_data, 0x4d);
	mdss_rgb_write_data(rgb_data, 0x1b);
	mdss_rgb_write_data(rgb_data, 0x16);
	mdss_rgb_write_data(rgb_data, 0x14);
	mdss_rgb_write_data(rgb_data, 0x1b);
	mdss_rgb_write_data(rgb_data, 0x1f);

	/* Set Gamma */
	mdss_rgb_write_command(rgb_data, NVGAMCTRL_CMD);
	mdss_rgb_write_data(rgb_data, 0xd2);
	mdss_rgb_write_data(rgb_data, 0x13);
	mdss_rgb_write_data(rgb_data, 0x16);
	mdss_rgb_write_data(rgb_data, 0x0d);
	mdss_rgb_write_data(rgb_data, 0x0f);
	mdss_rgb_write_data(rgb_data, 0x38);
	mdss_rgb_write_data(rgb_data, 0x3d);
	mdss_rgb_write_data(rgb_data, 0x43);
	mdss_rgb_write_data(rgb_data, 0x4d);
	mdss_rgb_write_data(rgb_data, 0x1b);
	mdss_rgb_write_data(rgb_data, 0x16);
	mdss_rgb_write_data(rgb_data, 0x14);
	mdss_rgb_write_data(rgb_data, 0x1b);
	mdss_rgb_write_data(rgb_data, 0x1f);

	/* SPI2 Enable */
	mdss_rgb_write_command(rgb_data, SPI2EN_CMD);
	mdss_rgb_write_data(rgb_data, 0x10);	/* Enable 2 data lane mode */

	mdss_rgb_write_command(rgb_data, MIPI_DCS_SET_DISPLAY_ON);
	msleep(30);

	pr_debug("%s: RGB panel power on done\n", __func__);
	return 0;
}

static int mdss_rgb_panel_off(struct mdss_panel_data *pdata)
{
	struct mdss_rgb_data *rgb_data = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid mdss panel data\n", __func__);
		return -EINVAL;
	}

	rgb_data = container_of(pdata, struct mdss_rgb_data,
			panel_data);

	mdss_rgb_write_command(rgb_data, MIPI_DCS_SET_DISPLAY_OFF);

	return 0;
}

static int mdss_rgb_panel_timing_from_dt(struct device_node *np,
		struct mdss_panel_timing *pt,
		struct mdss_panel_data *panel_data)
{
	u64 tmp64;
	int rc;
	struct mdss_panel_info *pinfo;

	pinfo = &panel_data->panel_info;

	rc = of_property_read_u32(np, "qcom,mdss-rgb-panel-width", &pt->xres);
	if (rc) {
		pr_err("%s:%d, panel width not specified\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	rc = of_property_read_u32(np, "qcom,mdss-rgb-panel-height", &pt->yres);
	if (rc) {
		pr_err("%s:%d, panel height not specified\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	pt->h_front_porch = 6;
	rc = of_property_read_u32(np, "qcom,mdss-rgb-h-front-porch",
			&pt->h_front_porch);

	pt->h_back_porch = 6;
	rc = of_property_read_u32(np, "qcom,mdss-rgb-h-back-porch",
			&pt->h_back_porch);

	pt->h_pulse_width = 2;
	rc = of_property_read_u32(np, "qcom,mdss-rgb-h-pulse-width",
			&pt->h_pulse_width);

	pt->hsync_skew = 0;
	rc = of_property_read_u32(np, "qcom,mdss-rgb-h-sync-skew",
			&pt->hsync_skew);

	pt->v_back_porch = 6;
	rc = of_property_read_u32(np, "qcom,mdss-rgb-v-back-porch",
			&pt->v_back_porch);

	pt->v_front_porch = 6;
	rc = of_property_read_u32(np, "qcom,mdss-rgb-v-front-porch",
			&pt->v_front_porch);

	pt->v_pulse_width = 2;
	rc = of_property_read_u32(np, "qcom,mdss-rgb-v-pulse-width",
			&pt->v_pulse_width);

	pt->border_left = 0;
	rc = of_property_read_u32(np, "qcom,mdss-rgb-h-left-border",
			&pt->border_left);

	pt->border_right = 0;
	rc = of_property_read_u32(np, "qcom,mdss-rgb-h-right-border",
			&pt->border_right);

	pt->border_top = 0;
	rc = of_property_read_u32(np, "qcom,mdss-rgb-v-top-border",
			&pt->border_top);

	pt->border_bottom = 0;
	rc = of_property_read_u32(np, "qcom,mdss-rgb-v-bottom-border",
			&pt->border_bottom);

	pt->frame_rate = DEFAULT_FRAME_RATE;
	rc = of_property_read_u32(np, "qcom,mdss-rgb-panel-framerate",
			(u32 *)&pt->frame_rate);

	rc = of_property_read_u64(np, "qcom,mdss-rgb-panel-clockrate", &tmp64);
	if (rc == -EOVERFLOW) {
		tmp64 = 0;
		rc = of_property_read_u32(np, "qcom,mdss-rgb-panel-clockrate",
				(u32 *)&tmp64);
	}
	pt->clk_rate = !rc ? tmp64 : 0;

	pt->name = kstrdup(np->name, GFP_KERNEL);
	pr_debug("%s: found new timing \"%s\"\n", __func__, np->name);

	return 0;
}

int mdss_rgb_clk_refresh(struct mdss_rgb_data *rgb_data)
{
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_panel_data *pdata = &rgb_data->panel_data;
	int rc = 0;

	if (!pdata) {
		pr_err("%s: invalid mdss panel data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;

	mdss_rgb_clk_div_config(rgb_data, &pdata->panel_info,
			pdata->panel_info.mipi.frame_rate);

	rgb_data->refresh_clk_rate = false;
	rgb_data->pclk_rate = pdata->panel_info.mipi.dsi_pclk_rate;
	rgb_data->byte_clk_rate = pdata->panel_info.clk_rate / 8;
	pr_debug("%s rgb_data->byte_clk_rate=%d rgb_data->pclk_rate=%d\n",
		__func__, rgb_data->byte_clk_rate, rgb_data->pclk_rate);

	rc = mdss_dsi_clk_set_link_rate(rgb_data->clk_handle,
			MDSS_DSI_LINK_BYTE_CLK, rgb_data->byte_clk_rate,
			MDSS_DSI_CLK_UPDATE_CLK_RATE_AT_ON);
	if (rc) {
		pr_err("%s: byte_clk - clk_set_rate failed\n",
				__func__);
		return rc;
	}

	rc = mdss_dsi_clk_set_link_rate(rgb_data->clk_handle,
			MDSS_DSI_LINK_PIX_CLK, rgb_data->pclk_rate,
			MDSS_DSI_CLK_UPDATE_CLK_RATE_AT_ON);
	if (rc)
		pr_err("%s: pixel_clk - clk_set_rate failed\n",
				__func__);
	return rc;
}

int mdss_rgb_panel_timing_switch(struct mdss_rgb_data *rgb_data,
		struct mdss_panel_timing *timing)
{
	struct mdss_panel_info *pinfo = &rgb_data->panel_data.panel_info;
	struct mdss_panel_data *panel_data = &rgb_data->panel_data;

	if (!timing)
		return -EINVAL;

	if (timing == panel_data->current_timing) {
		pr_warn("%s: panel timing \"%s\" already set\n", __func__,
				timing->name);
		return 0; /* nothing to do */
	}

	//Copy Timings from display_timing struct to panel_info struct
	mdss_panel_info_from_timing(timing, pinfo);

	panel_data->current_timing = timing;
	if (!timing->clk_rate)
		rgb_data->refresh_clk_rate = true;

	mdss_rgb_clk_refresh(rgb_data);

	return 0;
}


static int mdss_rgb_panel_parse_display_timings(struct device_node *np,
		struct mdss_rgb_data *rgb_data)
{
	struct device_node *timings_np;
	struct mdss_panel_data *panel_data = &rgb_data->panel_data;
	int rc = 0;

	INIT_LIST_HEAD(&panel_data->timings_list);

	timings_np = of_get_child_by_name(np, "qcom,mdss-rgb-display-timings");

	if (!timings_np) {
		/*
		 * display timings node is not available, fallback to reading
		 * timings directly from root node instead
		 */
		struct mdss_panel_timing pt;

		memset(&pt, 0, sizeof(struct mdss_panel_timing));

		pr_debug("reading display-timings from panel node\n");
		rc = mdss_rgb_panel_timing_from_dt(np, &pt, panel_data);
		if (!rc) {
			/*
			 * Switch to the parsed timings
			 */
			rc = mdss_rgb_panel_timing_switch(rgb_data, &pt);
		}
	}

	return rc;
}

static int mdss_rgb_panel_parse_dt(struct device_node *np,
		struct mdss_rgb_data *rgb_data)
{
	int rc = 0;
	struct mdss_panel_info *pinfo = &(rgb_data->panel_data.panel_info);

	pinfo->physical_width = 0;
	rc = of_property_read_u32(np, "qcom,mdss-pan-physical-width-dimension",
			&pinfo->physical_width);

	pinfo->physical_height = 0;
	rc = of_property_read_u32(np, "qcom,mdss-pan-physical-height-dimension",
			&pinfo->physical_height);

	pinfo->bpp = 24;
	rc = of_property_read_u32(np, "qcom,mdss-rgb-bpp", &pinfo->bpp);
	if (rc) {
		pr_err("%s:%d, bpp not specified\n", __func__, __LINE__);
		return -EINVAL;
	}

	pinfo->mipi.mode = DSI_VIDEO_MODE;

	switch (pinfo->bpp) {
	case 16:
		pinfo->mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB565;
		break;
	case 18:
		pinfo->mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB666;
		break;
	case 24:
	default:
		pinfo->mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
		break;
	}

	rc = mdss_rgb_panel_parse_display_timings(np, rgb_data);
	return rc;
}

int mdss_rgb_panel_init(struct device_node *node,
		struct mdss_rgb_data *rgb_data)
{
	int rc = 0;
	static const char *panel_name;
	struct mdss_panel_info *pinfo;

	if (!node) {
		pr_err("%s: Invalid device node\n", __func__);
		return -ENODEV;
	}

	pinfo = &rgb_data->panel_data.panel_info;

	pinfo->panel_name[0] = '\0';
	panel_name = of_get_property(node, "qcom,mdss-rgb-panel-name", NULL);
	if (!panel_name) {
		pr_info("%s:%d, Panel name not specified\n",
				__func__, __LINE__);
	} else {
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);
		strlcpy(&pinfo->panel_name[0], panel_name,
				sizeof(pinfo->panel_name));
	}

	rc = mdss_rgb_panel_parse_dt(node, rgb_data);
	if (rc)
		return rc;

	rgb_data->on = mdss_rgb_panel_on;
	rgb_data->off = mdss_rgb_panel_off;
	return rc;
}
