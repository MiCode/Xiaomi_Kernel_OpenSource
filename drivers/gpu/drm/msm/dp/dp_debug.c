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

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include <linux/debugfs.h>

#include "dp_parser.h"
#include "dp_power.h"
#include "dp_catalog.h"
#include "dp_aux.h"
#include "dp_ctrl.h"
#include "dp_debug.h"
#include "drm_connector.h"
#include "dp_display.h"

#define DEBUG_NAME "drm_dp"

struct dp_debug_private {
	struct dentry *root;

	struct dp_usbpd *usbpd;
	struct dp_link *link;
	struct dp_panel *panel;
	struct drm_connector **connector;
	struct device *dev;

	struct dp_debug dp_debug;
};

static ssize_t dp_debug_write_hpd(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	int hpd;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (kstrtoint(buf, 10, &hpd) != 0)
		goto end;

	debug->usbpd->connect(debug->usbpd, hpd);
end:
	return -len;
}

static ssize_t dp_debug_write_edid_modes(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_32];
	size_t len = 0;
	int hdisplay = 0, vdisplay = 0, vrefresh = 0, aspect_ratio;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		goto end;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_32 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto clear;

	buf[len] = '\0';

	if (sscanf(buf, "%d %d %d %d", &hdisplay, &vdisplay, &vrefresh,
				&aspect_ratio) != 4)
		goto clear;

	if (!hdisplay || !vdisplay || !vrefresh)
		goto clear;

	debug->dp_debug.debug_en = true;
	debug->dp_debug.hdisplay = hdisplay;
	debug->dp_debug.vdisplay = vdisplay;
	debug->dp_debug.vrefresh = vrefresh;
	debug->dp_debug.aspect_ratio = aspect_ratio;
	goto end;
clear:
	pr_debug("clearing debug modes\n");
	debug->dp_debug.debug_en = false;
end:
	return len;
}

static ssize_t dp_debug_bw_code_write(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	u32 max_bw_code = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		return 0;

	buf[len] = '\0';

	if (kstrtoint(buf, 10, &max_bw_code) != 0)
		return 0;

	if (!is_link_rate_valid(max_bw_code)) {
		pr_err("Unsupported bw code %d\n", max_bw_code);
		return len;
	}
	debug->panel->max_bw_code = max_bw_code;
	pr_debug("max_bw_code: %d\n", max_bw_code);

	return len;
}

static ssize_t dp_debug_read_connected(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_8];
	u32 len = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	len += snprintf(buf, SZ_8, "%d\n", debug->usbpd->hpd_high);

	if (copy_to_user(user_buff, buf, len))
		return -EFAULT;

	*ppos += len;
	return len;
}

static ssize_t dp_debug_read_edid_modes(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char *buf;
	u32 len = 0;
	int rc = 0;
	struct drm_connector *connector;
	struct drm_display_mode *mode;

	if (!debug) {
		pr_err("invalid data\n");
		rc = -ENODEV;
		goto error;
	}

	connector = *debug->connector;

	if (!connector) {
		pr_err("connector is NULL\n");
		rc = -EINVAL;
		goto error;
	}

	if (*ppos)
		goto error;

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!buf) {
		rc = -ENOMEM;
		goto error;
	}

	list_for_each_entry(mode, &connector->modes, head) {
		len += snprintf(buf + len, SZ_4K - len,
		"%s %d %d %d %d %d %d %d %d %d %d 0x%x\n",
		mode->name, mode->vrefresh, mode->picture_aspect_ratio,
		mode->hdisplay, mode->hsync_start, mode->hsync_end,
		mode->htotal, mode->vdisplay, mode->vsync_start,
		mode->vsync_end, mode->vtotal, mode->flags);
	}

	if (copy_to_user(user_buff, buf, len)) {
		kfree(buf);
		rc = -EFAULT;
		goto error;
	}

	*ppos += len;
	kfree(buf);

	return len;
error:
	return rc;
}

static int dp_debug_check_buffer_overflow(int rc, int *max_size, int *len)
{
	if (rc >= *max_size) {
		pr_err("buffer overflow\n");
		return -EINVAL;
	}
	*len += rc;
	*max_size = SZ_4K - *len;

	return 0;
}

static ssize_t dp_debug_read_info(struct file *file, char __user *user_buff,
		size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char *buf;
	u32 len = 0, rc = 0;
	u64 lclk = 0;
	u32 max_size = SZ_4K;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	rc = snprintf(buf + len, max_size, "\tname = %s\n", DEBUG_NAME);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\tdp_panel\n\t\tmax_pclk_khz = %d\n",
		debug->panel->max_pclk_khz);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\tdrm_dp_link\n\t\trate = %u\n",
		debug->panel->link_info.rate);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\t\tnum_lanes = %u\n",
		debug->panel->link_info.num_lanes);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\t\tcapabilities = %lu\n",
		debug->panel->link_info.capabilities);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\tdp_panel_info:\n\t\tactive = %dx%d\n",
		debug->panel->pinfo.h_active,
		debug->panel->pinfo.v_active);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\t\tback_porch = %dx%d\n",
		debug->panel->pinfo.h_back_porch,
		debug->panel->pinfo.v_back_porch);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\t\tfront_porch = %dx%d\n",
		debug->panel->pinfo.h_front_porch,
		debug->panel->pinfo.v_front_porch);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\t\tsync_width = %dx%d\n",
		debug->panel->pinfo.h_sync_width,
		debug->panel->pinfo.v_sync_width);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\t\tactive_low = %dx%d\n",
		debug->panel->pinfo.h_active_low,
		debug->panel->pinfo.v_active_low);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\t\th_skew = %d\n",
		debug->panel->pinfo.h_skew);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\t\trefresh rate = %d\n",
		debug->panel->pinfo.refresh_rate);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\t\tpixel clock khz = %d\n",
		debug->panel->pinfo.pixel_clk_khz);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\t\tbpp = %d\n",
		debug->panel->pinfo.bpp);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	/* Link Information */
	rc = snprintf(buf + len, max_size,
		"\tdp_link:\n\t\ttest_requested = %d\n",
		debug->link->sink_request);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\t\tlane_count = %d\n", debug->link->link_params.lane_count);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\t\tbw_code = %d\n", debug->link->link_params.bw_code);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	lclk = drm_dp_bw_code_to_link_rate(
			debug->link->link_params.bw_code) * 1000;
	rc = snprintf(buf + len, max_size,
		"\t\tlclk = %lld\n", lclk);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\t\tv_level = %d\n", debug->link->phy_params.v_level);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\t\tp_level = %d\n", debug->link->phy_params.p_level);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	if (copy_to_user(user_buff, buf, len))
		goto error;

	*ppos += len;

	kfree(buf);
	return len;
error:
	kfree(buf);
	return -EINVAL;
}

static ssize_t dp_debug_bw_code_read(struct file *file,
	char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char *buf;
	u32 len = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += snprintf(buf + len, (SZ_4K - len),
			"max_bw_code = %d\n", debug->panel->max_bw_code);

	if (copy_to_user(user_buff, buf, len)) {
		kfree(buf);
		return -EFAULT;
	}

	*ppos += len;
	kfree(buf);
	return len;
}

static const struct file_operations dp_debug_fops = {
	.open = simple_open,
	.read = dp_debug_read_info,
};

static const struct file_operations edid_modes_fops = {
	.open = simple_open,
	.read = dp_debug_read_edid_modes,
	.write = dp_debug_write_edid_modes,
};

static const struct file_operations hpd_fops = {
	.open = simple_open,
	.write = dp_debug_write_hpd,
};

static const struct file_operations connected_fops = {
	.open = simple_open,
	.read = dp_debug_read_connected,
};

static const struct file_operations bw_code_fops = {
	.open = simple_open,
	.read = dp_debug_bw_code_read,
	.write = dp_debug_bw_code_write,
};

static int dp_debug_init(struct dp_debug *dp_debug)
{
	int rc = 0;
	struct dp_debug_private *debug = container_of(dp_debug,
		struct dp_debug_private, dp_debug);
	struct dentry *dir, *file, *edid_modes;
	struct dentry *hpd, *connected;
	struct dentry *max_bw_code;
	struct dentry *root = debug->root;

	dir = debugfs_create_dir(DEBUG_NAME, NULL);
	if (IS_ERR_OR_NULL(dir)) {
		rc = PTR_ERR(dir);
		pr_err("[%s] debugfs create dir failed, rc = %d\n",
		       DEBUG_NAME, rc);
		goto error;
	}

	file = debugfs_create_file("dp_debug", 0444, dir,
				debug, &dp_debug_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs create file failed, rc=%d\n",
		       DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	edid_modes = debugfs_create_file("edid_modes", 0644, dir,
					debug, &edid_modes_fops);
	if (IS_ERR_OR_NULL(edid_modes)) {
		rc = PTR_ERR(edid_modes);
		pr_err("[%s] debugfs create edid_modes failed, rc=%d\n",
		       DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	hpd = debugfs_create_file("hpd", 0644, dir,
					debug, &hpd_fops);
	if (IS_ERR_OR_NULL(hpd)) {
		rc = PTR_ERR(hpd);
		pr_err("[%s] debugfs hpd failed, rc=%d\n",
			DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	connected = debugfs_create_file("connected", 0444, dir,
					debug, &connected_fops);
	if (IS_ERR_OR_NULL(connected)) {
		rc = PTR_ERR(connected);
		pr_err("[%s] debugfs connected failed, rc=%d\n",
			DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	max_bw_code = debugfs_create_file("max_bw_code", 0644, dir,
			debug, &bw_code_fops);
	if (IS_ERR_OR_NULL(max_bw_code)) {
		rc = PTR_ERR(max_bw_code);
		pr_err("[%s] debugfs max_bw_code failed, rc=%d\n",
		       DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	root = dir;
	return rc;
error_remove_dir:
	debugfs_remove(dir);
error:
	return rc;
}

struct dp_debug *dp_debug_get(struct device *dev, struct dp_panel *panel,
			struct dp_usbpd *usbpd, struct dp_link *link,
			struct drm_connector **connector)
{
	int rc = 0;
	struct dp_debug_private *debug;
	struct dp_debug *dp_debug;

	if (!dev || !panel || !usbpd || !link) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	debug = devm_kzalloc(dev, sizeof(*debug), GFP_KERNEL);
	if (!debug) {
		rc = -ENOMEM;
		goto error;
	}

	debug->dp_debug.debug_en = false;
	debug->usbpd = usbpd;
	debug->link = link;
	debug->panel = panel;
	debug->dev = dev;
	debug->connector = connector;

	dp_debug = &debug->dp_debug;
	dp_debug->vdisplay = 0;
	dp_debug->hdisplay = 0;
	dp_debug->vrefresh = 0;

	rc = dp_debug_init(dp_debug);
	if (rc) {
		devm_kfree(dev, debug);
		goto error;
	}

	return dp_debug;
error:
	return ERR_PTR(rc);
}

static int dp_debug_deinit(struct dp_debug *dp_debug)
{
	struct dp_debug_private *debug;

	if (!dp_debug)
		return -EINVAL;

	debug = container_of(dp_debug, struct dp_debug_private, dp_debug);

	debugfs_remove(debug->root);

	return 0;
}

void dp_debug_put(struct dp_debug *dp_debug)
{
	struct dp_debug_private *debug;

	if (!dp_debug)
		return;

	debug = container_of(dp_debug, struct dp_debug_private, dp_debug);

	dp_debug_deinit(dp_debug);

	devm_kfree(debug->dev, debug);
}
