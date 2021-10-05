// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */
#include <linux/err.h>
#include <linux/slab.h>
#include "sde_dbg.h"

#include "msm_cooling_device.h"
#include "mi_cooling_device.h"
#include "mi_dsi_display.h"
#include "mi_dsi_panel.h"

#define BL_NODE_NAME_SIZE 32

static int mi_sde_cdev_get_max_brightness(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct sde_cdev *disp_cdev = (struct sde_cdev *)cdev->devdata;
	struct mi_sde_cdev *mi_sde_cdev = (struct mi_sde_cdev *)container_of(disp_cdev, struct mi_sde_cdev, sde_cdev);

	*state = mi_sde_cdev->panel->mi_cfg.max_brightness_clone;

	return 0;
}

static int mi_sde_cdev_get_cur_brightness(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct sde_cdev *disp_cdev = (struct sde_cdev *)cdev->devdata;
	struct mi_sde_cdev *mi_sde_cdev = (struct mi_sde_cdev *)container_of(disp_cdev, struct mi_sde_cdev, sde_cdev);

	*state = mi_sde_cdev->panel->mi_cfg.max_brightness_clone - disp_cdev->thermal_state;

	return 0;
}

static int mi_sde_cdev_set_cur_brightness(struct thermal_cooling_device *cdev,
					unsigned long state)
{
	struct sde_cdev *disp_cdev = (struct sde_cdev *)cdev->devdata;
	struct mi_sde_cdev *mi_sde_cdev = (struct mi_sde_cdev *)container_of(disp_cdev, struct mi_sde_cdev, sde_cdev);
	unsigned long brightness_lvl = 0;

	if (state > mi_sde_cdev->panel->mi_cfg.max_brightness_clone)
		return -EINVAL;

	brightness_lvl = mi_sde_cdev->panel->mi_cfg.max_brightness_clone - state;
	if (brightness_lvl == mi_sde_cdev->sde_cdev.thermal_state)
		return 0;
	disp_cdev->thermal_state = brightness_lvl;
	if (mi_sde_cdev->panel->mi_cfg.thermal_dimming) {
		sysfs_notify(&cdev->device.kobj, NULL, "cur_state");
		pr_info("thermal dimming:set thermal_brightness_limit to %d\n", brightness_lvl);
	} else {
		blocking_notifier_call_chain(&disp_cdev->notifier_head,
						brightness_lvl, (void *)disp_cdev->bd);
	}

	return 0;
}

static int mi_sde_backlight_cooling_cb(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct sde_connector *c_conn;
	struct backlight_device *bd = (struct backlight_device *)data;
	struct dsi_display *dsi_display;

	c_conn = bl_get_data(bd);
	SDE_INFO("brightness clone: thermal max brightness cap:%lu\n", val);

	dsi_display = (struct dsi_display *) c_conn->display;
	dsi_display->panel->mi_cfg.thermal_max_brightness_clone = val;

	mi_dsi_display_set_brightness_clone(c_conn->display, dsi_display->panel->mi_cfg.real_brightness_clone);
	return 0;
}

static struct thermal_cooling_device_ops mi_sde_cdev_ops = {
	.get_max_state = mi_sde_cdev_get_max_brightness,
	.get_cur_state = mi_sde_cdev_get_cur_brightness,
	.set_cur_state = mi_sde_cdev_set_cur_brightness,
};

int mi_backlight_cdev_register(struct sde_cdev *disp_cdev,
					struct device *dev,
					struct backlight_device *bd,
					struct notifier_block *n)
{
	static int display_count;
	char bl_node_name[BL_NODE_NAME_SIZE];

	if (!dev || !bd || !n)
		return -EINVAL;
	if (!of_find_property(dev->of_node, "#cooling-cells", NULL))
		return -ENODEV;

	snprintf(bl_node_name, BL_NODE_NAME_SIZE, "brightness%u-clone",
							display_count);

	disp_cdev->thermal_state = 0;
	disp_cdev->bd = bd;
	disp_cdev->cdev = thermal_of_cooling_device_register(dev->of_node,
				bl_node_name, disp_cdev,
				&mi_sde_cdev_ops);
	if (IS_ERR_OR_NULL(disp_cdev->cdev)) {
		pr_err("cooling device register failed\n");
		return -ENODEV;
	}
	BLOCKING_INIT_NOTIFIER_HEAD(&disp_cdev->notifier_head);
	blocking_notifier_chain_register(&disp_cdev->notifier_head, n);
	pr_info("register %s cooling device success\n", bl_node_name);
	display_count++;
	return 0;
}

void mi_backlight_cdev_unregister(struct sde_cdev *cdev)
{
	if (!cdev)
		return;

	thermal_cooling_device_unregister(cdev->cdev);
}

int mi_sde_backlight_setup(struct sde_connector *c_conn, struct device *dev, struct backlight_device *bd)
{
	struct dsi_display *display;
	struct mi_sde_cdev *mi_sde_cdev = NULL;
	int rc = 0;

	display = (struct dsi_display *) c_conn->display;
	display->panel->mi_cfg.thermal_max_brightness_clone = display->panel->mi_cfg.max_brightness_clone;

	mi_sde_cdev = devm_kzalloc(dev, sizeof(*mi_sde_cdev), GFP_KERNEL);
	if (!mi_sde_cdev)
		return -ENOMEM;

	mi_sde_cdev->n.notifier_call = mi_sde_backlight_cooling_cb;
	mi_sde_cdev->panel = display->panel;

	rc = mi_backlight_cdev_register(&mi_sde_cdev->sde_cdev, dev, bd, &mi_sde_cdev->n);
	if (rc) {
		SDE_ERROR("Failed to register backlight mi_cdev: %ld\n",
				    PTR_ERR(c_conn->mi_cdev));
		return -ENODEV;
	}

	c_conn->mi_cdev = mi_sde_cdev;

	return 0;
}
