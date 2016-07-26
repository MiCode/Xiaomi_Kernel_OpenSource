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

#define pr_fmt(fmt)	"dm-drm:[%s] " fmt, __func__
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

/**
 * _dm_cache_active_displays - determine display type based on index
 * @disp_m: Pointer to display manager structure
 * Returns: Number of active displays in the system
 */
static u32 _dm_cache_active_displays(struct display_manager *disp_m)
{
	u32 count;

	if (!disp_m)
		return 0;

	disp_m->display_count = 0;

	/* query dsi displays */
	disp_m->dsi_display_count = dsi_display_get_num_of_displays();

	/* query hdmi displays */
	disp_m->hdmi_display_count = 0;

	/* query dp displays */
	disp_m->dp_display_count = 0;

	count = disp_m->dsi_display_count
		+ disp_m->hdmi_display_count
		+ disp_m->dp_display_count;

	disp_m->displays = kcalloc(count, sizeof(void *), GFP_KERNEL);
	if (!disp_m->displays) {
		disp_m->dsi_displays = 0;
		disp_m->dsi_display_count = 0;

		disp_m->hdmi_displays = 0;
		disp_m->hdmi_display_count = 0;

		disp_m->dp_displays = 0;
		disp_m->dp_display_count = 0;
	} else {
		/* get final dsi display list */
		disp_m->dsi_displays = disp_m->displays;
		disp_m->dsi_display_count =
			dsi_display_get_active_displays(disp_m->dsi_displays,
					disp_m->dsi_display_count);

		/* get final hdmi display list */
		disp_m->hdmi_displays = disp_m->dsi_displays
			+ disp_m->dsi_display_count;
		disp_m->hdmi_display_count = 0;

		/* get final dp display list */
		disp_m->dp_displays = disp_m->hdmi_displays
			+ disp_m->hdmi_display_count;
		disp_m->dp_display_count = 0;
	}

	/* set final display count */
	disp_m->display_count = disp_m->dsi_display_count
		+ disp_m->hdmi_display_count
		+ disp_m->dp_display_count;

	return disp_m->display_count;
}

/**
 * _dm_get_type_by_index - determine display type based on index
 * @disp_m: Pointer to display manager structure
 * @display_index: Incoming display index
 * Returns: DRM_MODE_CONNECTOR_ definition corresponding to display_index
 */
static int _dm_get_type_by_index(struct display_manager *disp_m,
				      u32 display_index)
{
	if (disp_m) {
		if (display_index < disp_m->dsi_display_count)
			return DRM_MODE_CONNECTOR_DSI;
		display_index -= disp_m->dsi_display_count;

		if (display_index < disp_m->hdmi_display_count)
			return DRM_MODE_CONNECTOR_HDMIA;
		display_index -= disp_m->hdmi_display_count;

		if (display_index < disp_m->dp_display_count)
			return DRM_MODE_CONNECTOR_DisplayPort;
		display_index -= disp_m->dp_display_count;
	}
	return DRM_MODE_CONNECTOR_Unknown;
}

/**
 * _dm_init_active_displays - initialize active display drivers
 * @disp_m: Pointer to display manager structure
 * Returns: Zero on success
 */
static int _dm_init_active_displays(struct display_manager *disp_m)
{
	void *display;
	int rc = 0;
	int i = 0;

	for (i = 0; i < disp_m->dsi_display_count; i++) {
		display = disp_m->dsi_displays[i];

		rc = dsi_display_dev_init(display);
		if (rc) {
			pr_err("failed to init dsi display, rc=%d\n", rc);

			for (i = i - 1; i >= 0; i--) {
				display = disp_m->dsi_displays[i];
				(void)dsi_display_dev_deinit(display);
			}
			break;
		}
	}

	/* TODO: INIT HDMI and DP displays here */
	return rc;
}

/**
 * _dm_deinit_active_displays - deconstruct active display drivers
 * @disp_m: Pointer to display manager structure
 * Returns: Zero on success
 */
static void _dm_deinit_active_displays(struct display_manager *disp_m)
{
	void *display;
	int rc, i;

	for (i = 0; i < disp_m->dsi_display_count; i++) {
		display = disp_m->dsi_displays[i];
		rc = dsi_display_dev_deinit(display);
		if (rc)
			pr_err("failed to deinit dsi display, rc=%d\n", rc);
	}

	/* TODO: DEINIT HDMI and DP displays here */
}

static int disp_manager_comp_ops_bind(struct device *dev,
				     struct device *master,
				     void *data)
{
	struct drm_device *drm;
	struct msm_drm_private *priv;
	struct display_manager *disp_m;
	void *display;
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
		display = disp_m->dsi_displays[i];

		rc = dsi_display_bind(display, drm);
		if (rc) {
			if (rc != -EPROBE_DEFER)
				pr_err("Failed to bind dsi display_%d, rc=%d\n",
					i, rc);

			/* clean up DSI bindings */
			for (i = i - 1; i >= 0; i--) {
				display = disp_m->dsi_displays[i];
				(void)dsi_display_unbind(display);
			}
			goto exit;
		}
	}

	/* TODO: BIND HDMI display here */
	/* TODO: BIND DP display here */

	priv->dm = disp_m;
exit:
	return rc;
}

static void disp_manager_comp_ops_unbind(struct device *dev,
					struct device *master,
					void *data)
{
	int rc = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct display_manager *disp_m;
	void *display;
	int i;

	if (!dev) {
		pr_err("Invalid params\n");
		return;
	}

	disp_m = platform_get_drvdata(pdev);

	/* DSI displays */
	for (i = 0; i < disp_m->dsi_display_count; i++) {
		display = disp_m->dsi_displays[i];

		rc = dsi_display_unbind(display);
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

	disp_m->display_count = _dm_cache_active_displays(disp_m);
	if (!disp_m->display_count) {
		rc = -ENODEV;
		pr_err("no displays found, rc=%d\n", rc);
		goto error_free_disp_m;
	}

	rc = _dm_init_active_displays(disp_m);
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
	_dm_deinit_active_displays(disp_m);
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

	_dm_deinit_active_displays(disp_m);
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
		pr_err("invalid params\n");
		return 0;
	}

	mutex_lock(&disp_m->lock);

	count = disp_m->display_count;

	mutex_unlock(&disp_m->lock);
	return count;
}

int display_manager_get_info_by_index(struct display_manager *disp_m,
				      u32 display_index,
				      struct msm_display_info *info)
{
	void *display;
	int rc = 0;

	if (!disp_m || !info) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	memset(info, 0, sizeof(*info));

	mutex_lock(&disp_m->lock);

	if (display_index < disp_m->display_count)
		display = disp_m->displays[display_index];

	switch (_dm_get_type_by_index(disp_m, display_index)) {
	case DRM_MODE_CONNECTOR_DSI:
		memset(info, 0x0, sizeof(*info));
		rc = dsi_display_get_info(info, display);
		if (rc) {
			pr_err("failed to get dsi info, rc=%d\n", rc);
			rc = -EINVAL;
		}
		break;
	default:
		pr_err("invalid index %d\n", display_index);
		rc = -EINVAL;
		break;
	}
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
		.mode_valid = dsi_conn_mode_valid,
		.get_info =   dsi_display_get_info,
	};
	void *display;
	int rc = -EINVAL;
	struct drm_connector *connector;

	if (!disp_m || !encoder) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&disp_m->lock);

	if (display_index < disp_m->display_count)
		display = disp_m->displays[display_index];

	switch (_dm_get_type_by_index(disp_m, display_index)) {
	case DRM_MODE_CONNECTOR_DSI:
		rc = dsi_display_drm_bridge_init(display, encoder);
		if (rc) {
			pr_err("dsi bridge init failed\n");
			break;
		}

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
	default:
		pr_err("invalid index %d\n", display_index);
		break;
	}

	mutex_unlock(&disp_m->lock);

	return rc;

}

int display_manager_drm_deinit_by_index(struct display_manager *disp_m,
					u32 display_index)
{
	void *display;
	int rc = 0;

	if (!disp_m) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&disp_m->lock);

	if (display_index < disp_m->display_count)
		display = disp_m->displays[display_index];

	switch (_dm_get_type_by_index(disp_m, display_index)) {
	case DRM_MODE_CONNECTOR_DSI:
		dsi_display_drm_bridge_deinit(display);
		break;
	default:
		pr_err("invalid index\n");
		rc = -EINVAL;
		break;
	}

	mutex_unlock(&disp_m->lock);

	return rc;
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
