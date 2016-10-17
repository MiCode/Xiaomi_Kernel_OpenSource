/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "sde_connector.h"
#include <linux/backlight.h>
#include "dsi_drm.h"

#define SDE_BRIGHT_TO_BL(out, v, bl_max, max_bright) do {\
	out = (2 * (v) * (bl_max) + max_bright);\
	do_div(out, 2 * max_bright);\
} while (0)

static int sde_backlight_device_update_status(struct backlight_device *bd)
{
	int brightness;
	struct drm_connector *connector;
	struct dsi_display *display;
	struct sde_connector *c_conn;
	int bl_lvl;

	brightness = bd->props.brightness;

	if ((bd->props.power != FB_BLANK_UNBLANK) ||
			(bd->props.state & BL_CORE_FBBLANK) ||
			(bd->props.state & BL_CORE_SUSPENDED))
		brightness = 0;

	connector = bl_get_data(bd);
	c_conn = to_sde_connector(connector);
	display = (struct dsi_display *) c_conn->display;
	if (brightness > display->panel->bl_config.bl_max_level)
		brightness = display->panel->bl_config.bl_max_level;

	/* This maps UI brightness into driver backlight level with
	 *        rounding
	 */
	SDE_BRIGHT_TO_BL(bl_lvl, brightness,
			display->panel->bl_config.bl_max_level,
			display->panel->bl_config.brightness_max_level);

	if (!bl_lvl && brightness)
		bl_lvl = 1;

	if (c_conn->ops.set_backlight)
		c_conn->ops.set_backlight(c_conn->display, bl_lvl);

	return 0;
}

static int sde_backlight_device_get_brightness(struct backlight_device *bd)
{
	return 0;
}

static const struct backlight_ops sde_backlight_device_ops = {
	.update_status = sde_backlight_device_update_status,
	.get_brightness = sde_backlight_device_get_brightness,
};

int sde_backlight_setup(struct drm_connector *connector)
{
	struct sde_connector *c_conn;
	struct backlight_device *bd;
	struct backlight_properties props;
	struct dsi_display *display;
	struct dsi_backlight_config *bl_config;

	if (!connector)
		return -EINVAL;

	c_conn = to_sde_connector(connector);
	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.power = FB_BLANK_UNBLANK;

	switch (c_conn->connector_type) {
	case DRM_MODE_CONNECTOR_DSI:
		display = (struct dsi_display *) c_conn->display;
		bl_config = &display->panel->bl_config;
		props.max_brightness = bl_config->brightness_max_level;
		props.brightness = bl_config->brightness_max_level;
		bd = backlight_device_register("sde-backlight",
				connector->kdev,
				connector,
				&sde_backlight_device_ops, &props);
		if (IS_ERR(bd)) {
			pr_err("Failed to register backlight: %ld\n",
					    PTR_ERR(bd));
			return -ENODEV;
		}
	}

	return 0;
}
