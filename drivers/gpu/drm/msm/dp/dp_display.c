/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm-dp]: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/component.h>
#include <linux/of_irq.h>

#include "msm_drv.h"
#include "dp_usbpd.h"
#include "dp_parser.h"
#include "dp_power.h"
#include "dp_catalog.h"
#include "dp_aux.h"
#include "dp_link.h"
#include "dp_panel.h"
#include "dp_ctrl.h"
#include "dp_display.h"

static struct dp_display *g_dp_display;

struct dp_display_private {
	char *name;
	int irq;

	struct platform_device *pdev;
	struct dentry *root;
	struct mutex lock;

	struct dp_usbpd   *usbpd;
	struct dp_parser  *parser;
	struct dp_power   *power;
	struct dp_catalog *catalog;
	struct dp_aux     *aux;
	struct dp_link    *link;
	struct dp_panel   *panel;
	struct dp_ctrl    *ctrl;

	struct dp_usbpd_cb usbpd_cb;
	struct dp_display_mode mode;
	struct dp_display dp_display;
};

static const struct of_device_id dp_dt_match[] = {
	{.compatible = "qcom,dp-display"},
	{}
};

static irqreturn_t dp_display_irq(int irq, void *dev_id)
{
	struct dp_display_private *dp = dev_id;

	if (!dp) {
		pr_err("invalid data\n");
		return IRQ_NONE;
	}

	/* DP controller isr */
	dp->ctrl->isr(dp->ctrl);

	/* DP aux isr */
	dp->aux->isr(dp->aux);

	return IRQ_HANDLED;
}

static ssize_t debugfs_dp_info_read(struct file *file, char __user *buff,
		size_t count, loff_t *ppos)
{
	struct dp_display_private *dp = file->private_data;
	char *buf;
	u32 len = 0;

	if (!dp)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += snprintf(buf + len, (SZ_4K - len), "name = %s\n", dp->name);
	len += snprintf(buf + len, (SZ_4K - len),
			"\tResolution = %dx%d\n",
			dp->panel->pinfo.h_active,
			dp->panel->pinfo.v_active);

	if (copy_to_user(buff, buf, len)) {
		kfree(buf);
		return -EFAULT;
	}

	*ppos += len;

	kfree(buf);
	return len;
}

static const struct file_operations dp_debug_fops = {
	.open = simple_open,
	.read = debugfs_dp_info_read,
};

static int dp_display_debugfs_init(struct dp_display_private *dp)
{
	int rc = 0;
	struct dentry *dir, *file;

	dir = debugfs_create_dir(dp->name, NULL);
	if (IS_ERR_OR_NULL(dir)) {
		rc = PTR_ERR(dir);
		pr_err("[%s] debugfs create dir failed, rc = %d\n",
		       dp->name, rc);
		goto error;
	}

	file = debugfs_create_file("dp_debug", 0444, dir, dp, &dp_debug_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs create file failed, rc=%d\n",
		       dp->name, rc);
		goto error_remove_dir;
	}

	dp->root = dir;
	return rc;
error_remove_dir:
	debugfs_remove(dir);
error:
	return rc;
}

static int dp_display_debugfs_deinit(struct dp_display_private *dp)
{
	debugfs_remove(dp->root);
	return 0;
}

static int dp_display_bind(struct device *dev, struct device *master,
		void *data)
{
	int rc = 0;
	struct dp_display_private *dp;
	struct drm_device *drm;
	struct msm_drm_private *priv;
	struct platform_device *pdev = to_platform_device(dev);

	if (!dev || !pdev || !master) {
		pr_err("invalid param(s), dev %pK, pdev %pK, master %pK\n",
				dev, pdev, master);
		rc = -EINVAL;
		goto error;
	}

	drm = dev_get_drvdata(master);
	dp = platform_get_drvdata(pdev);
	if (!drm || !dp) {
		pr_err("invalid param(s), drm %pK, dp %pK\n",
				drm, dp);
		rc = -EINVAL;
		goto error;
	}

	dp->dp_display.drm_dev = drm;
	priv = drm->dev_private;

	mutex_lock(&dp->lock);

	rc = dp_display_debugfs_init(dp);
	if (rc) {
		pr_err("[%s]Debugfs init failed, rc=%d\n", dp->name, rc);
		goto end;
	}

	rc = dp->parser->parse(dp->parser);
	if (rc) {
		pr_err("device tree parsing failed\n");
		goto end;
	}

	rc = dp->aux->drm_aux_register(dp->aux);
	if (rc) {
		pr_err("DRM DP AUX register failed\n");
		goto end;
	}

	rc = dp->panel->sde_edid_register(dp->panel);
	if (rc) {
		pr_err("DRM DP EDID register failed\n");
		goto end;
	}

	rc = dp->power->power_client_init(dp->power, &priv->phandle);
	if (rc) {
		pr_err("Power client create failed\n");
		goto end;
	}
end:
	mutex_unlock(&dp->lock);
error:
	return rc;
}

static void dp_display_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct dp_display_private *dp;
	struct platform_device *pdev = to_platform_device(dev);

	if (!dev || !pdev) {
		pr_err("invalid param(s)\n");
		return;
	}

	dp = platform_get_drvdata(pdev);
	if (!dp) {
		pr_err("Invalid params\n");
		return;
	}

	mutex_lock(&dp->lock);

	(void)dp->power->power_client_deinit(dp->power);

	(void) dp->panel->sde_edid_deregister(dp->panel);

	(void) dp->aux->drm_aux_deregister(dp->aux);

	(void)dp_display_debugfs_deinit(dp);

	mutex_unlock(&dp->lock);
}

static const struct component_ops dp_display_comp_ops = {
	.bind = dp_display_bind,
	.unbind = dp_display_unbind,
};

static int dp_display_process_hpd_high(struct dp_display_private *dp)
{
	int rc;

	rc = dp->panel->read_dpcd(dp->panel);
	if (rc)
		goto end;

	sde_get_edid(dp->dp_display.connector, &dp->aux->drm_aux->ddc,
		(void **)&dp->panel->edid_ctrl);

	return 0;
end:
	return rc;
}

static int dp_display_process_hpd_low(struct dp_display_private *dp)
{
	dp->dp_display.is_connected = false;
	return 0;
}

static int dp_display_usbpd_configure_cb(struct device *dev)
{
	int rc = 0;
	bool flip = false;
	struct dp_display_private *dp;

	if (!dev) {
		pr_err("invalid dev\n");
		rc = -EINVAL;
		goto end;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		pr_err("no driver data found\n");
		rc = -ENODEV;
		goto end;
	}

	mutex_lock(&dp->lock);

	if (dp->usbpd->orientation == ORIENTATION_CC2)
		flip = true;

	dp->power->init(dp->power, flip);
	dp->ctrl->init(dp->ctrl, flip);
	dp->aux->init(dp->aux, dp->parser->aux_cfg);
	enable_irq(dp->irq);

	if (dp->usbpd->hpd_high)
		dp_display_process_hpd_high(dp);
	dp->dp_display.is_connected = true;

	mutex_unlock(&dp->lock);
end:
	return rc;
}

static int dp_display_usbpd_disconnect_cb(struct device *dev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dev) {
		pr_err("invalid dev\n");
		rc = -EINVAL;
		goto end;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		pr_err("no driver data found\n");
		rc = -ENODEV;
		goto end;
	}

	mutex_lock(&dp->lock);
	dp->dp_display.is_connected = false;
	disable_irq(dp->irq);
	mutex_unlock(&dp->lock);

end:
	return rc;
}

static int dp_display_usbpd_attention_cb(struct device *dev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dev) {
		pr_err("invalid dev\n");
		rc = -EINVAL;
		goto end;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		pr_err("no driver data found\n");
		rc = -ENODEV;
		goto end;
	}

	mutex_lock(&dp->lock);

	if (dp->usbpd->hpd_irq) {
		if (!dp->link->process_request(dp->link))
			goto end;
	}

	if (dp->usbpd->hpd_high)
		dp_display_process_hpd_high(dp);
	else
		dp_display_process_hpd_low(dp);

	mutex_unlock(&dp->lock);
end:
	return rc;
}

static int dp_init_sub_modules(struct dp_display_private *dp)
{
	int rc = 0;
	struct device *dev = &dp->pdev->dev;
	struct dp_usbpd_cb *cb = &dp->usbpd_cb;
	struct dp_ctrl_in ctrl_in = {
		.dev = dev,
	};

	cb->configure  = dp_display_usbpd_configure_cb;
	cb->disconnect = dp_display_usbpd_disconnect_cb;
	cb->attention  = dp_display_usbpd_attention_cb;

	dp->usbpd = dp_usbpd_get(dev, cb);
	if (IS_ERR(dp->usbpd)) {
		rc = PTR_ERR(dp->usbpd);
		pr_err("failed to initialize usbpd, rc = %d\n", rc);
		goto err;
	}

	dp->parser = dp_parser_get(dp->pdev);
	if (IS_ERR(dp->parser)) {
		rc = PTR_ERR(dp->parser);
		pr_err("failed to initialize parser, rc = %d\n", rc);
		goto err;
	}

	dp->catalog = dp_catalog_get(dev, &dp->parser->io);
	if (IS_ERR(dp->catalog)) {
		rc = PTR_ERR(dp->catalog);
		pr_err("failed to initialize catalog, rc = %d\n", rc);
		goto err;
	}

	dp->power = dp_power_get(dp->parser);
	if (IS_ERR(dp->power)) {
		rc = PTR_ERR(dp->power);
		pr_err("failed to initialize power, rc = %d\n", rc);
		goto err;
	}

	dp->aux = dp_aux_get(dev, &dp->catalog->aux);
	if (IS_ERR(dp->aux)) {
		rc = PTR_ERR(dp->aux);
		pr_err("failed to initialize aux, rc = %d\n", rc);
		goto err;
	}

	dp->panel = dp_panel_get(dev, dp->aux, &dp->catalog->panel);
	if (IS_ERR(dp->panel)) {
		rc = PTR_ERR(dp->panel);
		pr_err("failed to initialize panel, rc = %d\n", rc);
		goto err;
	}

	dp->link = dp_link_get(dev, dp->aux);
	if (IS_ERR(dp->link)) {
		rc = PTR_ERR(dp->link);
		pr_err("failed to initialize link, rc = %d\n", rc);
		goto err;
	}

	ctrl_in.link = dp->link;
	ctrl_in.panel = dp->panel;
	ctrl_in.aux = dp->aux;
	ctrl_in.power = dp->power;
	ctrl_in.catalog = &dp->catalog->ctrl;
	ctrl_in.parser = dp->parser;

	dp->ctrl = dp_ctrl_get(&ctrl_in);
	if (IS_ERR(dp->ctrl)) {
		rc = PTR_ERR(dp->ctrl);
		pr_err("failed to initialize ctrl, rc = %d\n", rc);
		goto err;
	}
err:
	return rc;
}

static int dp_display_set_mode(struct dp_display *dp_display,
		struct dp_display_mode *mode)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}
	dp = container_of(dp_display, struct dp_display_private, dp_display);

	dp->panel->pinfo = mode->timing;
	dp->panel->init_info(dp->panel);
error:
	return rc;
}

static int dp_display_prepare(struct dp_display *dp)
{
	return 0;
}

static int dp_display_enable(struct dp_display *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->lock);
	dp->ctrl->on(dp->ctrl);
	mutex_unlock(&dp->lock);
error:
	return rc;
}

static int dp_display_post_enable(struct dp_display *dp)
{
	return 0;
}

static int dp_display_pre_disable(struct dp_display *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->lock);

	dp->ctrl->off(dp->ctrl);

	mutex_unlock(&dp->lock);
error:
	return rc;
}

static int dp_display_disable(struct dp_display *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->lock);

	dp->aux->deinit(dp->aux);
	dp->ctrl->deinit(dp->ctrl);
	dp->power->deinit(dp->power);

	mutex_unlock(&dp->lock);
error:
	return rc;
}

static int dp_request_irq(struct dp_display *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	dp->irq = irq_of_parse_and_map(dp->pdev->dev.of_node, 0);
	if (dp->irq < 0) {
		rc = dp->irq;
		pr_err("failed to get irq: %d\n", rc);
		return rc;
	}

	rc = devm_request_irq(&dp->pdev->dev, dp->irq, dp_display_irq,
		IRQF_TRIGGER_HIGH, "dp_display_isr", dp);
	if (rc < 0) {
		pr_err("failed to request IRQ%u: %d\n",
				dp->irq, rc);
		return rc;
	}
	disable_irq(dp->irq);

	return 0;
}

static int dp_display_unprepare(struct dp_display *dp)
{
	return 0;
}

static int dp_display_validate_mode(struct dp_display *dp,
	struct dp_display_mode *mode)
{
	return 0;
}

static int dp_display_get_modes(struct dp_display *dp)
{
	int ret = 0;
	struct dp_display_private *dp_display;

	dp_display = container_of(dp, struct dp_display_private, dp_display);

	ret = _sde_edid_update_modes(dp->connector,
		dp_display->panel->edid_ctrl);

	return ret;
}

static int dp_display_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("pdev not found\n");
		return -ENODEV;
	}

	dp = devm_kzalloc(&pdev->dev, sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return -ENOMEM;

	mutex_init(&dp->lock);
	dp->pdev = pdev;
	dp->name = "drm_dp";

	rc = dp_init_sub_modules(dp);
	if (rc) {
		devm_kfree(&pdev->dev, dp);
		return -EPROBE_DEFER;
	}

	platform_set_drvdata(pdev, dp);

	g_dp_display = &dp->dp_display;

	g_dp_display->enable        = dp_display_enable;
	g_dp_display->post_enable   = dp_display_post_enable;
	g_dp_display->pre_disable   = dp_display_pre_disable;
	g_dp_display->disable       = dp_display_disable;
	g_dp_display->set_mode      = dp_display_set_mode;
	g_dp_display->validate_mode = dp_display_validate_mode;
	g_dp_display->get_modes     = dp_display_get_modes;
	g_dp_display->prepare       = dp_display_prepare;
	g_dp_display->unprepare     = dp_display_unprepare;
	g_dp_display->request_irq   = dp_request_irq;

	rc = component_add(&pdev->dev, &dp_display_comp_ops);
	if (rc)
		pr_err("component add failed, rc=%d\n", rc);

	return rc;
}

int dp_display_get_displays(void **displays, int count)
{
	if (!displays) {
		pr_err("invalid data\n");
		return -EINVAL;
	}

	if (count != 1) {
		pr_err("invalid number of displays\n");
		return -EINVAL;
	}

	displays[0] = g_dp_display;
	return count;
}

int dp_display_get_num_of_displays(void)
{
	return 1;
}

static void dp_display_deinit_sub_modules(struct dp_display_private *dp)
{
	dp_ctrl_put(dp->ctrl);
	dp_link_put(dp->link);
	dp_panel_put(dp->panel);
	dp_aux_put(dp->aux);
	dp_power_put(dp->power);
	dp_catalog_put(dp->catalog);
	dp_parser_put(dp->parser);
	dp_usbpd_put(dp->usbpd);
}

static int dp_display_remove(struct platform_device *pdev)
{
	struct dp_display_private *dp;

	if (!pdev)
		return -EINVAL;

	dp = platform_get_drvdata(pdev);

	dp_display_deinit_sub_modules(dp);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, dp);

	return 0;
}

static struct platform_driver dp_display_driver = {
	.probe  = dp_display_probe,
	.remove = dp_display_remove,
	.driver = {
		.name = "msm-dp-display",
		.of_match_table = dp_dt_match,
	},
};

static int __init dp_display_init(void)
{
	int ret;

	ret = platform_driver_register(&dp_display_driver);
	if (ret) {
		pr_err("driver register failed");
		return ret;
	}

	return ret;
}
module_init(dp_display_init);

static void __exit dp_display_cleanup(void)
{
	platform_driver_unregister(&dp_display_driver);
}
module_exit(dp_display_cleanup);

