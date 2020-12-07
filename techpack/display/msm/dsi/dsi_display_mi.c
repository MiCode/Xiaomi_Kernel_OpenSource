/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"mi-dsi-display:[%s] " fmt, __func__

#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/err.h>

#include "msm_drv.h"
#include "sde_connector.h"
#include "msm_mmu.h"
#include "dsi_display.h"
#include "dsi_panel.h"
#include "dsi_ctrl.h"
#include "dsi_ctrl_hw.h"
#include "dsi_drm.h"
#include "dsi_clk.h"
#include "dsi_pwr.h"
#include "sde_dbg.h"
#include "dsi_parser.h"

#define to_dsi_bridge(x)     container_of((x), struct dsi_bridge, base)

static atomic64_t g_param = ATOMIC64_INIT(0);

int dsi_display_disp_param_get(struct drm_connector *connector,
			char *buf)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;
	int ret = 0;

	pr_info("last command:0x%x\n", (u32)atomic64_read(&g_param));

	if (!connector || !connector->encoder || !connector->encoder->bridge) {
		pr_err("Invalid invalid connector/encoder/bridge ptr\n");
		return -EINVAL;
	}

	c_bridge =  to_dsi_bridge(connector->encoder->bridge);
	display = c_bridge->display;
	if (!display || !display->panel) {
		pr_err("Invalid display/panel ptr\n");
		return -EINVAL;
	}

	ret = strlen(display->panel->mi_cfg.panel_read_data);
	ret = ret > 255 ? 255 : ret;
	if (ret > 0)
		memcpy(buf, display->panel->mi_cfg.panel_read_data, ret);

	return ret;
}

int dsi_display_disp_param_set(struct drm_connector *connector,
			u32 param_type)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;
	int ret = 0;

	if (!connector || !connector->encoder || !connector->encoder->bridge) {
		pr_err("Invalid invalid connector/encoder/bridge ptr\n");
		return -EINVAL;
	}

	c_bridge =  to_dsi_bridge(connector->encoder->bridge);
	display = c_bridge->display;
	if (!display || !display->panel) {
		pr_err("Invalid display/panel ptr\n");
		return -EINVAL;
	}

	atomic64_set(&g_param, param_type);
	ret = dsi_panel_disp_param_set(display->panel, param_type);

	return ret;
}

ssize_t dsi_display_panel_info_read(struct drm_connector *connector,
			char *buf)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;
	char *pname = NULL;
	ssize_t ret = 0;

	if (!connector || !connector->encoder || !connector->encoder->bridge) {
		pr_err("Invalid invalid connector/encoder/bridge ptr\n");
		return -EINVAL;
	}

	c_bridge =  to_dsi_bridge(connector->encoder->bridge);
	display = c_bridge->display;
	if (!display || !display->panel) {
		pr_err("Invalid display/panel ptr\n");
		return -EINVAL;
	}

	if (display->name) {
		/* find the last occurrence of a character in a string */
		pname = strrchr(display->name, ',');
		if (pname && ++pname)
			ret = snprintf(buf, PAGE_SIZE, "panel_name=%s\n", pname);
		else
			ret = snprintf(buf, PAGE_SIZE, "panel_name=%s\n", display->name);
	} else {
		ret = snprintf(buf, PAGE_SIZE, "panel_name=%s\n", "null");
	}

	return ret;
}

