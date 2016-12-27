/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <linux/of_device.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/msm-bus.h>
#include <linux/of_irq.h>

#include "msm_drv.h"
#include "msm_kms.h"
#include "msm_gpu.h"
#include "sde_connector.h"

#include "dsi_display.h"
#include "dsi_drm.h"
#include "display_manager.h"

static u32 dm_get_num_of_displays(struct display_manager *disp_m)
{
	u32 count = 0;

	count = dsi_display_get_num_of_displays();
	disp_m->display_count = count;
	disp_m->dsi_display_count = count;

	/* TODO: get HDMI and DP display count here */
	return disp_m->display_count;
}

static int dm_set_active_displays(struct display_manager *disp_m)
{
	/* TODO: Make changes from DT config here */
	return 0;
}

static int dm_init_active_displays(struct display_manager *disp_m)
{
	int rc = 0;
	int i = 0;
	struct dsi_display *dsi_display;

	for (i = 0; i < disp_m->dsi_display_count; i++) {
		dsi_display = dsi_display_get_display_by_index(i);
		if (!dsi_display || !dsi_display_is_active(dsi_display))
			continue;

		rc = dsi_display_dev_init(dsi_display);
		if (rc) {
			pr_err("failed to init dsi display, rc=%d\n", rc);
			goto error_deinit_dsi_displays;
		}
	}

	/* TODO: INIT HDMI and DP displays here */
	return rc;
error_deinit_dsi_displays:
	for (i = i - 1; i >= 0; i--) {
		dsi_display = dsi_display_get_display_by_index(i);
		if (dsi_display && dsi_display_is_active(dsi_display))
			(void)dsi_display_dev_deinit(dsi_display);
	}

	return rc;
}

static int dm_deinit_active_displays(struct display_manager *disp_m)
{
	int rc = 0;
	int i = 0;
	struct dsi_display *dsi_display;

	for (i = 0; i < disp_m->dsi_display_count; i++) {
		dsi_display = dsi_display_get_display_by_index(i);
		if (!dsi_display || !dsi_display_is_active(dsi_display))
			continue;

		rc = dsi_display_dev_deinit(dsi_display);
		if (rc)
			pr_err("failed to deinit dsi display, rc=%d\n", rc);
	}

	/* TODO: DEINIT HDMI and DP displays here */
	return rc;
}

static int disp_manager_comp_ops_bind(struct device *dev,
				     struct device *master,
				     void *data)
{
	struct drm_device *drm;
	struct msm_drm_private *priv;
	struct display_manager *disp_m;
	struct dsi_display *dsi_display;
	int i, rc = -EINVAL;

	if (master && dev) {
		drm = dev_get_drvdata(master);
		disp_m = platform_get_drvdata(to_platform_device(dev));
		if (drm && drm->dev_private && disp_m)
			rc = 0;
	}

	if (rc) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	priv = drm->dev_private;
	disp_m->drm_dev = drm;

	/* DSI displays */
	for (i = 0; i < disp_m->dsi_display_count; i++) {
		dsi_display = dsi_display_get_display_by_index(i);
		if (!dsi_display) {
			pr_err("Display does not exist\n");
			continue;
		}

		if (!dsi_display_is_active(dsi_display))
			continue;

		rc = dsi_display_bind(dsi_display, drm);
		if (rc) {
			if (rc != -EPROBE_DEFER)
				pr_err("Failed to bind dsi display_%d, rc=%d\n",
					i, rc);
			goto error_unbind_dsi;
		}
	}

	/* TODO: BIND HDMI display here */
	/* TODO: BIND DP display here */

	priv->dm = disp_m;
	return rc;
error_unbind_dsi:
	for (i = i - 1; i >= 0; i--) {
		dsi_display = dsi_display_get_display_by_index(i);
		if (!dsi_display || !dsi_display_is_active(dsi_display))
			continue;
		(void)dsi_display_unbind(dsi_display);
	}
	return rc;
}

static void disp_manager_comp_ops_unbind(struct device *dev,
					struct device *master,
					void *data)
{
	int rc = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct display_manager *disp_m;
	struct dsi_display *dsi_display;
	int i;

	if (!dev) {
		pr_err("Invalid params\n");
		return;
	}

	disp_m = platform_get_drvdata(pdev);

	/* DSI displays */
	for (i = 0; i < disp_m->dsi_display_count; i++) {
		dsi_display = dsi_display_get_display_by_index(i);
		if (!dsi_display || !dsi_display_is_active(dsi_display))
			continue;

		rc = dsi_display_unbind(dsi_display);
		if (rc)
			pr_err("failed to unbind dsi display_%d, rc=%d\n",
			       i, rc);
	}

	/* TODO: UNBIND HDMI display here */
	/* TODO: UNBIND DP display here */
}

static const struct of_device_id displays_dt_match[] = {
	{.compatible = "qcom,dsi-display"},
	{.compatible = "qcom,hdmi-display"},
	{.compatible = "qcom,dp-display"},
	{}
};

static const struct component_ops disp_manager_comp_ops = {
	.bind = disp_manager_comp_ops_bind,
	.unbind = disp_manager_comp_ops_unbind,
};

static int disp_manager_dev_probe(struct platform_device *pdev)
{
	struct display_manager *disp_m;
	int rc = 0;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("pdev not found\n");
		return -ENODEV;
	}

	disp_m = devm_kzalloc(&pdev->dev, sizeof(*disp_m), GFP_KERNEL);
	if (!disp_m)
		return -ENOMEM;

	disp_m->name = "qcom,display-manager";

	of_platform_populate(pdev->dev.of_node, displays_dt_match,
			     NULL, &pdev->dev);

	disp_m->display_count = dm_get_num_of_displays(disp_m);
	if (!disp_m->display_count) {
		rc = -ENODEV;
		pr_err("No display found, rc=%d\n", rc);
		goto error_free_disp_m;
	}

	rc = dm_set_active_displays(disp_m);
	if (rc) {
		pr_err("failed to set active displays, rc=%d\n", rc);
		goto error_remove_displays;
	}

	rc = dm_init_active_displays(disp_m);
	if (rc) {
		pr_err("failed to initialize displays, rc=%d\n", rc);
		goto error_remove_displays;
	}

	rc = component_add(&pdev->dev, &disp_manager_comp_ops);
	if (rc) {
		pr_err("failed to add component, rc=%d\n", rc);
		goto error_deinit_displays;
	}

	mutex_init(&disp_m->lock);
	platform_set_drvdata(pdev, disp_m);

	return rc;
error_deinit_displays:
	(void)dm_deinit_active_displays(disp_m);
error_remove_displays:
	of_platform_depopulate(&pdev->dev);
error_free_disp_m:
	devm_kfree(&pdev->dev, disp_m);
	return rc;
}

static int disp_manager_dev_remove(struct platform_device *pdev)
{
	struct display_manager *disp_m;

	if (!pdev) {
		pr_err("invalid pdev argument\n");
		return -ENODEV;
	}

	disp_m = platform_get_drvdata(pdev);

	(void)dm_deinit_active_displays(disp_m);
	of_platform_depopulate(&pdev->dev);
	devm_kfree(&pdev->dev, disp_m);

	return 0;
}

static const struct of_device_id disp_manager_dt_match[] = {
	{.compatible = "qcom,display-manager"},
	{}
};

static struct platform_driver disp_manager_driver = {
	.probe = disp_manager_dev_probe,
	.remove = disp_manager_dev_remove,
	.driver = {
		.name = "msm-display-manager",
		.of_match_table = disp_manager_dt_match,
	},
};


int display_manager_get_count(struct display_manager *disp_m)
{
	int count;

	if (!disp_m) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&disp_m->lock);

	count = 1; /* TODO: keep track of active displays */

	mutex_unlock(&disp_m->lock);
	return count;
}

int display_manager_get_info_by_index(struct display_manager *disp_m,
				      u32 display_index,
				      struct display_info *info)
{
	int rc = 0;
	int i, j;
	struct dsi_display *display;
	struct dsi_display_info dsi_info;

	if (!disp_m || !info) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	memset(info, 0, sizeof(*info));

	mutex_lock(&disp_m->lock);

	for (i = 0; i < disp_m->dsi_display_count; i++) {
		display = dsi_display_get_display_by_index(i);
		if (!display || !dsi_display_is_active(display))
			continue;

		memset(&dsi_info, 0x0, sizeof(dsi_info));
		rc = dsi_display_get_info(display, &dsi_info);
		if (rc) {
			pr_err("failed to get display info, rc=%d\n", rc);
			goto error;
		}

		info->intf = DISPLAY_INTF_DSI;
		info->num_of_h_tiles = dsi_info.num_of_h_tiles;

		for (j = 0; j < info->num_of_h_tiles; j++)
			info->h_tile_instance[j] = dsi_info.h_tile_ids[j];

		info->is_hot_pluggable = dsi_info.is_hot_pluggable;
		info->is_connected = dsi_info.is_connected;
		info->is_edid_supported = dsi_info.is_edid_supported;
		info->max_width = 1920; /* TODO: */
		info->max_height = 1080; /* TODO: */
		info->compression = DISPLAY_COMPRESSION_NONE;
		if (dsi_info.op_mode == DSI_OP_VIDEO_MODE) {
			info->intf_mode |= DISPLAY_INTF_MODE_VID;
		} else if (dsi_info.op_mode == DSI_OP_CMD_MODE) {
			info->intf_mode |= DISPLAY_INTF_MODE_CMD;
		} else {
			pr_err("unknwown dsi op_mode %d\n", dsi_info.op_mode);
			rc = -EINVAL;
			goto error;
		}
		break;
	}

error:
	mutex_unlock(&disp_m->lock);
	return rc;
}

int display_manager_drm_init_by_index(struct display_manager *disp_m,
				      u32 display_index,
				      struct drm_encoder *encoder)
{
	static const struct sde_connector_ops dsi_ops = {
		.post_init =  dsi_conn_post_init,
		.detect =     dsi_conn_detect,
		.get_modes =  dsi_connector_get_modes,
		.mode_valid = dsi_conn_mode_valid
	};
	int rc = -EINVAL;
	int i;
	struct dsi_display *display;
	struct drm_connector *connector;

	if (!disp_m || !encoder) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&disp_m->lock);

	for (i = 0; i < disp_m->dsi_display_count; i++) {
		display = dsi_display_get_display_by_index(i);
		if (!display || !dsi_display_is_active(display))
			continue;

		rc = dsi_display_drm_bridge_init(display, encoder);
		if (rc)
			continue;

		connector = sde_connector_init(disp_m->drm_dev,
				encoder,
				0,
				display,
				&dsi_ops,
				DRM_CONNECTOR_POLL_HPD,
				DRM_MODE_CONNECTOR_DSI);
		if (!connector)
			rc = -ENOMEM;
		else if (IS_ERR(connector))
			rc = PTR_ERR(connector);
		break;
	}

	mutex_unlock(&disp_m->lock);

	return rc;

}

int display_manager_drm_deinit_by_index(struct display_manager *disp_m,
					u32 display_index)
{
	int i;
	struct dsi_display *display;

	if (!disp_m) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&disp_m->lock);

	for (i = 0; i < disp_m->dsi_display_count; i++) {
		display = dsi_display_get_display_by_index(i);
		if (!display || !dsi_display_is_active(display))
			continue;

		dsi_display_drm_bridge_deinit(display);
		break;
	}

	mutex_unlock(&disp_m->lock);

	return 0;
}


void display_manager_register(void)
{
	dsi_phy_drv_register();
	dsi_ctrl_drv_register();
	dsi_display_register();
	platform_driver_register(&disp_manager_driver);
}
void display_manager_unregister(void)
{
	platform_driver_unregister(&disp_manager_driver);
	dsi_display_unregister();
	dsi_ctrl_drv_unregister();
	dsi_phy_drv_unregister();
}
