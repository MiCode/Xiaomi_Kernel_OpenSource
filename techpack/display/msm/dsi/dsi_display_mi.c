/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#include "dsi_panel_mi.h"
#include "dsi_ctrl.h"
#include "dsi_ctrl_hw.h"
#include "dsi_drm.h"
#include "dsi_clk.h"
#include "dsi_pwr.h"
#include "sde_dbg.h"
#include "dsi_parser.h"
#include "dsi_mi_feature.h"
#include "../msm_drv.h"

#define to_dsi_bridge(x)     container_of((x), struct dsi_bridge, base)

static atomic64_t g_param = ATOMIC64_INIT(0);

static char oled_pmic_id_str[4] = {0};
static char oled_wp_info_str[32] = {0};
static bool wp_info_cmdline_flag = 0;

static int __init
oled_pmic_id_setup (char *str)
{
	size_t count = ((strlen(str) > sizeof(oled_pmic_id_str)) ?
					sizeof(oled_pmic_id_str): strlen(str));
	strncpy(oled_pmic_id_str, str, count);
	return 1;
}
__setup("androidboot.oled_pmic_id=", oled_pmic_id_setup);


static int __init
oled_wp_info_setup (char *str)
{
	size_t count = ((strlen(str) > sizeof(oled_wp_info_str)) ?
					sizeof(oled_wp_info_str): strlen(str));
	strncpy(oled_wp_info_str, str, count);
	pr_info("androidboot.oled_wp=%s\n", oled_wp_info_str);
	wp_info_cmdline_flag = 1;
	return 1;
}
__setup("androidboot.oled_wp=", oled_wp_info_setup);

int dsi_display_set_disp_param(struct drm_connector *connector,
			u32 param_type)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;
	int ret = 0;

	if (!connector || !connector->encoder || !connector->encoder->bridge) {
		pr_err("Invalid connector/encoder/bridge ptr\n");
		return -EINVAL;
	}

	c_bridge =  to_dsi_bridge(connector->encoder->bridge);
	display = c_bridge->display;
	if (!display || !display->panel) {
		pr_err("Invalid display/panel ptr\n");
		return -EINVAL;
	}

	atomic64_set(&g_param, param_type);
	ret = dsi_panel_set_disp_param(display->panel, param_type);

	return ret;
}

int dsi_display_get_disp_param(struct drm_connector *connector,
			u32 *param_type)
{
	*param_type = (u32)atomic64_read(&g_param);
	return 0;
}

int dsi_display_write_mipi_reg(struct drm_connector *connector,
			char *buf)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;

	if (!connector || !connector->encoder || !connector->encoder->bridge) {
		pr_err("Invalid connector/encoder/bridge ptr\n");
		return -EINVAL;
	}

	c_bridge =  to_dsi_bridge(connector->encoder->bridge);
	display = c_bridge->display;
	if (!display || !display->panel) {
		pr_err("Invalid display/panel ptr\n");
		return -EINVAL;
	}

	return dsi_panel_write_mipi_reg(display->panel, buf);
}

ssize_t dsi_display_read_mipi_reg(struct drm_connector *connector,
			char *buf)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;

	if (!connector || !connector->encoder || !connector->encoder->bridge) {
		pr_err("Invalid connector/encoder/bridge ptr\n");
		return -EINVAL;
	}

	c_bridge =  to_dsi_bridge(connector->encoder->bridge);
	display = c_bridge->display;
	if (!display || !display->panel) {
		pr_err("Invalid display/panel ptr\n");
		return -EINVAL;
	}

	return dsi_panel_read_mipi_reg(display->panel, buf);
}

ssize_t dsi_display_read_oled_pmic_id(struct drm_connector *connector,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE , "%s\n", oled_pmic_id_str);
}

ssize_t dsi_display_read_panel_info(struct drm_connector *connector,
			char *buf)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;
	char *pname = NULL;
	ssize_t ret = 0;

	pname = dsi_display_get_cmdline_panel_info();
	if (pname) {
		ret = snprintf(buf, PAGE_SIZE, "panel_name=%s\n", pname);
		kfree(pname);
	} else {
		if (!connector || !connector->encoder || !connector->encoder->bridge) {
			pr_err("Invalid connector/encoder/bridge ptr\n");
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
			if (pname && *pname)
				ret = snprintf(buf, PAGE_SIZE, "panel_name=%s\n", ++pname);
			else
				ret = snprintf(buf, PAGE_SIZE, "panel_name=%s\n", display->name);
		} else {
			ret = snprintf(buf, PAGE_SIZE, "panel_name=%s\n", "null");
		}
	}

	return ret;
}

ssize_t dsi_display_read_wp_info(struct drm_connector *connector,
			char *buf)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;

	if (wp_info_cmdline_flag) {
		return snprintf(buf, PAGE_SIZE , "%s\n", oled_wp_info_str);
	} else {
		if (!connector || !connector->encoder || !connector->encoder->bridge) {
			pr_err("Invalid connector/encoder/bridge ptr\n");
			return -EINVAL;
		}

		c_bridge =	to_dsi_bridge(connector->encoder->bridge);
		display = c_bridge->display;
		if (!display || !display->panel) {
			pr_err("Invalid display/panel ptr\n");
			return -EINVAL;
		}

		return dsi_panel_read_wp_info(display->panel, buf);
	}
}

ssize_t dsi_display_read_dynamic_fps(struct drm_connector *connector,
			char *buf)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;
	struct dsi_display_mode *cur_mode = NULL;
	ssize_t ret = 0;

	if (!connector || !connector->encoder || !connector->encoder->bridge) {
		pr_err("Invalid connector/encoder/bridge ptr\n");
		return -EINVAL;
	}

	c_bridge =  to_dsi_bridge(connector->encoder->bridge);
	display = c_bridge->display;
	if (!display || !display->panel) {
		pr_err("Invalid display/panel ptr\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	cur_mode = display->panel->cur_mode;
	if (cur_mode) {
		ret = snprintf(buf, PAGE_SIZE, "%d\n", cur_mode->timing.refresh_rate);

	} else {
		ret = snprintf(buf, PAGE_SIZE, "%s\n", "null");
	}
	mutex_unlock(&display->display_lock);

	return ret;
}

int dsi_display_set_doze_brightness(struct drm_connector *connector,
			int doze_brightness)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;

	if (!connector || !connector->encoder || !connector->encoder->bridge) {
		pr_err("Invalid connector/encoder/bridge ptr\n");
		return -EINVAL;
	}

	c_bridge =  to_dsi_bridge(connector->encoder->bridge);
	display = c_bridge->display;
	if (!display || !display->panel) {
		pr_err("Invalid display/panel ptr\n");
		return -EINVAL;
	}

	return dsi_panel_set_doze_brightness(display->panel, doze_brightness, true);
}

ssize_t dsi_display_get_doze_brightness(struct drm_connector *connector,
			char *buf)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;

	if (!connector || !connector->encoder || !connector->encoder->bridge) {
		pr_err("Invalid connector/encoder/bridge ptr\n");
		return -EINVAL;
	}

	c_bridge =  to_dsi_bridge(connector->encoder->bridge);
	display = c_bridge->display;
	if (!display || !display->panel) {
		pr_err("Invalid display/panel ptr\n");
		return -EINVAL;
	}

	return dsi_panel_get_doze_brightness(display->panel, buf);
}

int dsi_display_read_gamma_param(struct drm_connector *connector)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;
	int rc = 0;

	if (!connector || !connector->encoder || !connector->encoder->bridge) {
		pr_err("Invalid connector/encoder/bridge ptr\n");
		return -EINVAL;
	}

	c_bridge =  to_dsi_bridge(connector->encoder->bridge);
	display = c_bridge->display;
	if (!display || !display->panel) {
		pr_err("Invalid display/panel ptr\n");
		return -EINVAL;
	}

	rc = dsi_panel_read_gamma_param(display->panel);
	if (rc) {
		pr_err("Failed to read gamma para, rc=%d\n", rc);
	}

	return rc;
}

ssize_t dsi_display_print_gamma_param(struct drm_connector *connector, char *buf)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;

	if (!connector || !connector->encoder || !connector->encoder->bridge) {
		pr_err("Invalid connector/encoder/bridge ptr\n");
		return -EINVAL;
	}

	c_bridge =  to_dsi_bridge(connector->encoder->bridge);
	display = c_bridge->display;
	if (!display || !display->panel) {
		pr_err("Invalid display/panel ptr\n");
		return -EINVAL;
	}

	return dsi_panel_print_gamma_param(display->panel, buf);

}

int dsi_display_hbm_set_disp_param(struct drm_connector *connector,
				u32 op_code)
{
	int rc;
	struct sde_connector *c_conn;

	c_conn = to_sde_connector(connector);

	pr_debug("%s fod hbm command:0x%x \n", __func__, op_code);

	if (op_code == DISPPARAM_HBM_FOD_ON) {
		rc = dsi_display_set_disp_param(connector, DISPPARAM_HBM_FOD_ON);
	} else if (op_code == DISPPARAM_HBM_FOD_OFF) {
		/* close HBM and restore DC */
		rc = dsi_display_set_disp_param(connector, DISPPARAM_HBM_FOD_OFF);
	} else if(op_code == DISPPARAM_DIMMING_OFF) {
		rc = dsi_display_set_disp_param(connector, DISPPARAM_DIMMING_OFF);
	} else if (op_code == DISPPARAM_HBM_BACKLIGHT_RESEND) {
		rc = dsi_display_set_disp_param(connector, DISPPARAM_HBM_BACKLIGHT_RESEND);
	}

	return rc;
}

ssize_t dsi_display_fod_get(struct drm_connector *connector, char *buf)
{
	struct dsi_display *display = NULL;
	struct dsi_bridge *c_bridge = NULL;

	if (!connector || !connector->encoder || !connector->encoder->bridge) {
		pr_err("Invalid connector/encoder/bridge ptr\n");
		return -EINVAL;
	}

	c_bridge = to_dsi_bridge(connector->encoder->bridge);
	display = c_bridge->display;
	if (!display || !display->panel) {
		pr_err("Invalid display/panel ptr\n");
		return -EINVAL;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", display->panel->mi_cfg.fod_ui_ready);
}

int dsi_display_esd_irq_ctrl(struct dsi_display *display,
		bool enable)
{
	int rc = 0;

	if (!display) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_panel_esd_irq_ctrl(display->panel, enable);
	if (rc)
		pr_err("[%s] failed to set esd irq, rc=%d\n",
				display->name, rc);

	mutex_unlock(&display->display_lock);

	return rc;
}

ssize_t complete_commit_time_get(struct drm_connector *connector, char *buf)
{
	struct dsi_display *dsi_display = NULL;
	struct dsi_bridge *c_bridge;
	struct msm_drm_private *priv = NULL;

	if ((connector == NULL) || (connector->encoder == NULL)
			|| (connector->encoder->bridge == NULL))
		return 0;

	c_bridge =  to_dsi_bridge(connector->encoder->bridge);
	dsi_display = c_bridge->display;

	if ((dsi_display == NULL) || (dsi_display->panel == NULL))
		return -EINVAL;

	priv = dsi_display->drm_dev->dev_private;

	return snprintf(buf, PAGE_SIZE, "%lld\n", priv->complete_commit_time);

}

int dsi_display_set_thermal_hbm_disabled(struct drm_connector *connector,
			bool thermal_hbm_disabled)
{
	struct sde_connector *c_conn = NULL;
	struct dsi_display *display = NULL;

	if (!connector) {
		pr_err("invalid argument\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	if (!c_conn->display) {
		pr_err("invalid connector display\n");
		return -EINVAL;
	}

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI) {
		pr_err("unsupported connector (%s)\n", connector->name);
		return -EINVAL;
	}

	display = (struct dsi_display *)c_conn->display;
	return dsi_panel_set_thermal_hbm_disabled(display->panel, thermal_hbm_disabled);
}

int dsi_display_get_thermal_hbm_disabled(struct drm_connector *connector,
			bool *thermal_hbm_disabled)
{
	struct sde_connector *c_conn = NULL;
	struct dsi_display *display = NULL;

	if (!connector) {
		pr_err("invalid argument\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	if (!c_conn->display) {
		pr_err("invalid connector display\n");
		return -EINVAL;
	}

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI) {
		pr_err("unsupported connector (%s)\n", connector->name);
		return -EINVAL;
	}

	display = (struct dsi_display *)c_conn->display;
	return dsi_panel_get_thermal_hbm_disabled(display->panel, thermal_hbm_disabled);
}

ssize_t dsi_display_get_hw_vsync_info(struct drm_connector *connector,
			char *buf)
{
	struct sde_connector *c_conn = NULL;
	struct dsi_display *display = NULL;

	if (!connector) {
		pr_err("invalid argument\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	if (!c_conn->display) {
		pr_err("invalid connector display\n");
		return -EINVAL;
	}

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI) {
		pr_err("unsupported connector (%s)\n", connector->name);
		return -EINVAL;
	}

	display = (struct dsi_display *)c_conn->display;

	return calc_hw_vsync_info(display->panel, buf);
}
