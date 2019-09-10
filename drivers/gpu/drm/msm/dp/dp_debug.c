/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#include "dp_power.h"
#include "dp_catalog.h"
#include "dp_aux.h"
#include "dp_ctrl.h"
#include "dp_debug.h"
#include "drm_connector.h"
#include "sde_connector.h"
#include "dp_display.h"

#define DEBUG_NAME "drm_dp"

struct dp_debug_private {
	struct dentry *root;
	u8 *edid;
	u32 edid_size;

	u8 *dpcd;
	u32 dpcd_size;

	int vdo;

	char exe_mode[SZ_32];
	char reg_dump[SZ_32];

	struct dp_usbpd *usbpd;
	struct dp_link *link;
	struct dp_panel *panel;
	struct dp_aux *aux;
	struct dp_catalog *catalog;
	struct drm_connector **connector;
	struct device *dev;
	struct work_struct sim_work;
	struct dp_debug dp_debug;
	struct mutex lock;
};

static int dp_debug_get_edid_buf(struct dp_debug_private *debug)
{
	int rc = 0;

	if (!debug->edid) {
		debug->edid = devm_kzalloc(debug->dev, SZ_256, GFP_KERNEL);
		if (!debug->edid) {
			rc = -ENOMEM;
			goto end;
		}

		debug->edid_size = SZ_256;
	}
end:
	return rc;
}

static int dp_debug_get_dpcd_buf(struct dp_debug_private *debug)
{
	int rc = 0;

	if (!debug->dpcd) {
		debug->dpcd = devm_kzalloc(debug->dev, SZ_1K, GFP_KERNEL);
		if (!debug->dpcd) {
			rc = -ENOMEM;
			goto end;
		}

		debug->dpcd_size = SZ_1K;
	}
end:
	return rc;
}

static ssize_t dp_debug_write_edid(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	u8 *buf = NULL, *buf_t = NULL, *edid = NULL;
	const int char_to_nib = 2;
	size_t edid_size = 0;
	size_t size = 0, edid_buf_index = 0;
	ssize_t rc = count;

	if (!debug)
		return -ENODEV;

	mutex_lock(&debug->lock);

	if (*ppos)
		goto bail;

	size = min_t(size_t, count, SZ_1K);

	buf = kzalloc(size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		rc = -ENOMEM;
		goto bail;
	}

	if (copy_from_user(buf, user_buff, size))
		goto bail;

	edid_size = size / char_to_nib;
	buf_t = buf;

	if (dp_debug_get_edid_buf(debug))
		goto bail;

	if (edid_size != debug->edid_size) {
		pr_debug("realloc debug edid\n");

		if (debug->edid) {
			devm_kfree(debug->dev, debug->edid);

			debug->edid = devm_kzalloc(debug->dev,
						edid_size, GFP_KERNEL);
			if (!debug->edid) {
				rc = -ENOMEM;
				goto bail;
			}

			debug->edid_size = edid_size;

			debug->aux->set_sim_mode(debug->aux,
					debug->dp_debug.sim_mode,
					debug->edid, debug->dpcd);
		}
	}

	while (edid_size--) {
		char t[3];
		int d;

		memcpy(t, buf_t, sizeof(char) * char_to_nib);
		t[char_to_nib] = '\0';

		if (kstrtoint(t, 16, &d)) {
			pr_err("kstrtoint error\n");
			goto bail;
		}

		if (edid_buf_index < debug->edid_size)
			debug->edid[edid_buf_index++] = d;

		buf_t += char_to_nib;
	}

	edid = debug->edid;
bail:
	kfree(buf);

	if (!debug->dp_debug.sim_mode)
		debug->panel->set_edid(debug->panel, edid);

	mutex_unlock(&debug->lock);
	return rc;
}

static ssize_t dp_debug_write_dpcd(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	u8 *buf = NULL, *buf_t = NULL, *dpcd = NULL;
	const int char_to_nib = 2;
	size_t dpcd_size = 0;
	size_t size = 0, dpcd_buf_index = 0;
	ssize_t rc = count;
	char offset_ch[5];
	u32 offset;

	if (!debug)
		return -ENODEV;

	mutex_lock(&debug->lock);

	if (*ppos)
		goto bail;

	size = min_t(size_t, count, SZ_2K);
	if (size < 4)
		goto bail;

	buf = kzalloc(size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		rc = -ENOMEM;
		goto bail;
	}

	if (copy_from_user(buf, user_buff, size))
		goto bail;

	memcpy(offset_ch, buf, 4);
	offset_ch[4] = '\0';

	if (kstrtoint(offset_ch, 16, &offset)) {
		pr_err("offset kstrtoint error\n");
		goto bail;
	}

	if (dp_debug_get_dpcd_buf(debug))
		goto bail;

	if (offset == 0xFFFF) {
		pr_err("clearing dpcd\n");
		memset(debug->dpcd, 0, debug->dpcd_size);
		goto bail;
	}

	size -= 4;
	if (size == 0)
		goto bail;

	dpcd_size = size / char_to_nib;
	buf_t = buf + 4;

	dpcd_buf_index = offset;

	while (dpcd_size--) {
		char t[3];
		int d;

		memcpy(t, buf_t, sizeof(char) * char_to_nib);
		t[char_to_nib] = '\0';

		if (kstrtoint(t, 16, &d)) {
			pr_err("kstrtoint error\n");
			goto bail;
		}

		if (dpcd_buf_index < debug->dpcd_size)
			debug->dpcd[dpcd_buf_index++] = d;

		buf_t += char_to_nib;
	}

	dpcd = debug->dpcd;
bail:
	kfree(buf);
	if (debug->dp_debug.sim_mode)
		debug->aux->dpcd_updated(debug->aux);
	else
		debug->panel->set_dpcd(debug->panel, dpcd);

	mutex_unlock(&debug->lock);
	return rc;
}

static ssize_t dp_debug_read_dpcd(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_8];
	u32 len = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	len += snprintf(buf, SZ_8, "0x%x\n", debug->aux->reg);

	len = min_t(size_t, count, len);
	if (copy_to_user(user_buff, buf, len))
		return -EFAULT;

	*ppos += len;
	return len;
}

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

	hpd &= 0x3;

	debug->dp_debug.psm_enabled = !!(hpd & BIT(1));

	debug->usbpd->simulate_connect(debug->usbpd, !!(hpd & BIT(0)));
end:
	return len;
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

static ssize_t dp_debug_tpg_write(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	u32 tpg_state = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto bail;

	buf[len] = '\0';

	if (kstrtoint(buf, 10, &tpg_state) != 0)
		goto bail;

	tpg_state &= 0x1;
	pr_debug("tpg_state: %d\n", tpg_state);

	if (tpg_state == debug->dp_debug.tpg_state)
		goto bail;

	if (debug->panel)
		debug->panel->tpg_config(debug->panel, tpg_state);

	debug->dp_debug.tpg_state = tpg_state;
bail:
	return len;
}

static ssize_t dp_debug_write_exe_mode(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_32];
	size_t len = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_32 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (sscanf(buf, "%3s", debug->exe_mode) != 1)
		goto end;

	if (strcmp(debug->exe_mode, "hw") &&
	    strcmp(debug->exe_mode, "sw") &&
	    strcmp(debug->exe_mode, "all"))
		goto end;

	debug->catalog->set_exe_mode(debug->catalog, debug->exe_mode);
end:
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

	len = min_t(size_t, count, len);
	if (copy_to_user(user_buff, buf, len))
		return -EFAULT;

	*ppos += len;
	return len;
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

static ssize_t dp_debug_read_edid_modes(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char *buf;
	u32 len = 0, ret = 0, max_size = SZ_4K;
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
	if (ZERO_OR_NULL_PTR(buf)) {
		rc = -ENOMEM;
		goto error;
	}

	mutex_lock(&connector->dev->mode_config.mutex);
	list_for_each_entry(mode, &connector->modes, head) {
		ret = snprintf(buf + len, max_size,
		"%s %d %d %d %d %d 0x%x\n",
		mode->name, mode->vrefresh, mode->picture_aspect_ratio,
		mode->htotal, mode->vtotal, mode->clock, mode->flags);
		if (dp_debug_check_buffer_overflow(ret, &max_size, &len))
			break;
	}
	mutex_unlock(&connector->dev->mode_config.mutex);

	len = min_t(size_t, count, len);
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

static ssize_t dp_debug_read_info(struct file *file, char __user *user_buff,
		size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char *buf;
	u32 len = 0, rc = 0;
	u32 max_size = SZ_4K;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf))
		return -ENOMEM;

	rc = snprintf(buf + len, max_size, "\tstate=0x%x\n", debug->aux->state);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "\tlink_rate=%u\n",
		debug->panel->link_info.rate);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "\tnum_lanes=%u\n",
		debug->panel->link_info.num_lanes);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "\tresolution=%dx%d@%dHz\n",
		debug->panel->pinfo.h_active,
		debug->panel->pinfo.v_active,
		debug->panel->pinfo.refresh_rate);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "\tpclock=%dKHz\n",
		debug->panel->pinfo.pixel_clk_khz);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "\tbpp=%d\n",
		debug->panel->pinfo.bpp);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	/* Link Information */
	rc = snprintf(buf + len, max_size, "\ttest_req=%s\n",
		dp_link_get_test_name(debug->link->sink_request));
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\tlane_count=%d\n", debug->link->link_params.lane_count);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\tbw_code=%d\n", debug->link->link_params.bw_code);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\tv_level=%d\n", debug->link->phy_params.v_level);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"\tp_level=%d\n", debug->link->phy_params.p_level);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	len = min_t(size_t, count, len);
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
	if (ZERO_OR_NULL_PTR(buf))
		return -ENOMEM;

	len += snprintf(buf + len, (SZ_4K - len),
			"max_bw_code = %d\n", debug->panel->max_bw_code);

	len = min_t(size_t, count, len);
	if (copy_to_user(user_buff, buf, len)) {
		kfree(buf);
		return -EFAULT;
	}

	*ppos += len;
	kfree(buf);
	return len;
}

static ssize_t dp_debug_tpg_read(struct file *file,
	char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_8];
	u32 len = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	len += snprintf(buf, SZ_8, "%d\n", debug->dp_debug.tpg_state);

	len = min_t(size_t, count, len);
	if (copy_to_user(user_buff, buf, len))
		return -EFAULT;

	*ppos += len;
	return len;
}

static ssize_t dp_debug_write_hdr(struct file *file,
	const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct drm_connector *connector;
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_512];
	size_t len = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	connector = *debug->connector;
	c_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(connector->state);

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_512 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (sscanf(buf, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
			&c_state->hdr_meta.hdr_supported,
			&c_state->hdr_meta.hdr_state,
			&c_state->hdr_meta.eotf,
			&c_state->hdr_meta.display_primaries_x[0],
			&c_state->hdr_meta.display_primaries_x[1],
			&c_state->hdr_meta.display_primaries_x[2],
			&c_state->hdr_meta.display_primaries_y[0],
			&c_state->hdr_meta.display_primaries_y[1],
			&c_state->hdr_meta.display_primaries_y[2],
			&c_state->hdr_meta.white_point_x,
			&c_state->hdr_meta.white_point_y,
			&c_state->hdr_meta.max_luminance,
			&c_state->hdr_meta.min_luminance,
			&c_state->hdr_meta.max_content_light_level,
			&c_state->hdr_meta.max_average_light_level) != 15) {
		pr_err("invalid input\n");
		len = -EINVAL;
	}

	debug->panel->setup_hdr(debug->panel, &c_state->hdr_meta);
end:
	return len;
}

static ssize_t dp_debug_read_hdr(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char *buf;
	u32 len = 0, i;
	u32 max_size = SZ_4K;
	int rc = 0;
	struct drm_connector *connector;
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;
	struct drm_msm_ext_hdr_metadata *hdr;

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
	if (ZERO_OR_NULL_PTR(buf)) {
		rc = -ENOMEM;
		goto error;
	}

	c_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(connector->state);

	hdr = &c_state->hdr_meta;

	rc = snprintf(buf + len, max_size,
		"============SINK HDR PARAMETERS===========\n");
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "eotf = %d\n",
		connector->hdr_eotf);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "type_one = %d\n",
		connector->hdr_metadata_type_one);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "max_luminance = %d\n",
		connector->hdr_max_luminance);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "avg_luminance = %d\n",
		connector->hdr_avg_luminance);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "min_luminance = %d\n",
		connector->hdr_min_luminance);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size,
		"============VIDEO HDR PARAMETERS===========\n");
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "hdr_state = %d\n", hdr->hdr_state);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "hdr_supported = %d\n",
			hdr->hdr_supported);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "eotf = %d\n", hdr->eotf);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "white_point_x = %d\n",
		hdr->white_point_x);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "white_point_y = %d\n",
		hdr->white_point_y);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "max_luminance = %d\n",
		hdr->max_luminance);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "min_luminance = %d\n",
		hdr->min_luminance);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "max_content_light_level = %d\n",
		hdr->max_content_light_level);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "min_content_light_level = %d\n",
		hdr->max_average_light_level);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	for (i = 0; i < HDR_PRIMARIES_COUNT; i++) {
		rc = snprintf(buf + len, max_size, "primaries_x[%d] = %d\n",
			i, hdr->display_primaries_x[i]);
		if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
			goto error;

		rc = snprintf(buf + len, max_size, "primaries_y[%d] = %d\n",
			i, hdr->display_primaries_y[i]);
		if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
			goto error;
	}

	len = min_t(size_t, count, len);
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

static ssize_t dp_debug_write_sim(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	int sim;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	mutex_lock(&debug->lock);

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (kstrtoint(buf, 10, &sim) != 0)
		goto end;

	if (sim) {
		if (dp_debug_get_edid_buf(debug))
			goto end;

		if (dp_debug_get_dpcd_buf(debug))
			goto error;
	} else {
		if (debug->edid) {
			devm_kfree(debug->dev, debug->edid);
			debug->edid = NULL;
		}

		if (debug->dpcd) {
			devm_kfree(debug->dev, debug->dpcd);
			debug->dpcd = NULL;
		}
	}

	debug->dp_debug.sim_mode = !!sim;

	debug->aux->set_sim_mode(debug->aux, debug->dp_debug.sim_mode,
			debug->edid, debug->dpcd);
end:
	mutex_unlock(&debug->lock);
	return len;
error:
	devm_kfree(debug->dev, debug->edid);
	mutex_unlock(&debug->lock);
	return len;
}

static ssize_t dp_debug_write_attention(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	int vdo;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (kstrtoint(buf, 10, &vdo) != 0)
		goto end;

	debug->vdo = vdo;

	schedule_work(&debug->sim_work);
end:
	return len;
}

static ssize_t dp_debug_write_dump(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_32];
	size_t len = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_32 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (sscanf(buf, "%31s", debug->reg_dump) != 1)
		goto end;

	/* qfprom register dump not supported */
	if (!strcmp(debug->reg_dump, "qfprom_physical"))
		strlcpy(debug->reg_dump, "clear", sizeof(debug->reg_dump));
end:
	return len;
}

static ssize_t dp_debug_read_dump(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	int rc = 0;
	struct dp_debug_private *debug = file->private_data;
	u8 *buf = NULL;
	u32 len = 0;
	char prefix[SZ_32];

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	if (!debug->usbpd->hpd_high || !strlen(debug->reg_dump))
		goto end;

	rc = debug->catalog->get_reg_dump(debug->catalog,
		debug->reg_dump, &buf, &len);
	if (rc)
		goto end;

	snprintf(prefix, sizeof(prefix), "%s: ", debug->reg_dump);
	print_hex_dump(KERN_DEBUG, prefix, DUMP_PREFIX_NONE,
		16, 4, buf, len, false);

	len = min_t(size_t, count, len);
	if (copy_to_user(user_buff, buf, len))
		return -EFAULT;

	*ppos += len;
end:
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

static const struct file_operations edid_fops = {
	.open = simple_open,
	.write = dp_debug_write_edid,
};

static const struct file_operations dpcd_fops = {
	.open = simple_open,
	.write = dp_debug_write_dpcd,
	.read = dp_debug_read_dpcd,
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
static const struct file_operations exe_mode_fops = {
	.open = simple_open,
	.write = dp_debug_write_exe_mode,
};

static const struct file_operations tpg_fops = {
	.open = simple_open,
	.read = dp_debug_tpg_read,
	.write = dp_debug_tpg_write,
};

static const struct file_operations hdr_fops = {
	.open = simple_open,
	.write = dp_debug_write_hdr,
	.read = dp_debug_read_hdr,
};

static const struct file_operations sim_fops = {
	.open = simple_open,
	.write = dp_debug_write_sim,
};

static const struct file_operations attention_fops = {
	.open = simple_open,
	.write = dp_debug_write_attention,
};

static const struct file_operations dump_fops = {
	.open = simple_open,
	.write = dp_debug_write_dump,
	.read = dp_debug_read_dump,
};

static int dp_debug_init(struct dp_debug *dp_debug)
{
	int rc = 0;
	struct dp_debug_private *debug = container_of(dp_debug,
		struct dp_debug_private, dp_debug);
	struct dentry *dir, *file;

	dir = debugfs_create_dir(DEBUG_NAME, NULL);
	if (IS_ERR_OR_NULL(dir)) {
		if (!dir)
			rc = -EINVAL;
		else
			rc = PTR_ERR(dir);
		pr_err("[%s] debugfs create dir failed, rc = %d\n",
		       DEBUG_NAME, rc);
		goto error;
	}

	debug->root = dir;

	file = debugfs_create_file("dp_debug", 0444, dir,
				debug, &dp_debug_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs create file failed, rc=%d\n",
		       DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("edid_modes", 0644, dir,
					debug, &edid_modes_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs create edid_modes failed, rc=%d\n",
		       DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("hpd", 0644, dir,
					debug, &hpd_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs hpd failed, rc=%d\n",
			DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("connected", 0444, dir,
					debug, &connected_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs connected failed, rc=%d\n",
			DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("max_bw_code", 0644, dir,
			debug, &bw_code_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs max_bw_code failed, rc=%d\n",
		       DEBUG_NAME, rc);
	}

	file = debugfs_create_file("exe_mode", 0644, dir,
			debug, &exe_mode_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs register failed, rc=%d\n",
		       DEBUG_NAME, rc);
	}

	file = debugfs_create_file("edid", 0644, dir,
					debug, &edid_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs edid failed, rc=%d\n",
			DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("dpcd", 0644, dir,
					debug, &dpcd_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs dpcd failed, rc=%d\n",
			DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("tpg_ctrl", 0644, dir,
			debug, &tpg_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs tpg failed, rc=%d\n",
		       DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("hdr", 0644, dir,
		debug, &hdr_fops);

	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs hdr failed, rc=%d\n",
			DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("sim", 0644, dir,
		debug, &sim_fops);

	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs sim failed, rc=%d\n",
			DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("attention", 0644, dir,
		debug, &attention_fops);

	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs attention failed, rc=%d\n",
			DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	file = debugfs_create_file("dump", 0644, dir,
		debug, &dump_fops);

	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		pr_err("[%s] debugfs dump failed, rc=%d\n",
			DEBUG_NAME, rc);
		goto error_remove_dir;
	}

	return 0;

error_remove_dir:
	if (!file)
		rc = -EINVAL;
	debugfs_remove_recursive(dir);
error:
	return rc;
}

static void dp_debug_sim_work(struct work_struct *work)
{
	struct dp_debug_private *debug =
		container_of(work, typeof(*debug), sim_work);

	debug->usbpd->simulate_attention(debug->usbpd, debug->vdo);
}

struct dp_debug *dp_debug_get(struct device *dev, struct dp_panel *panel,
			struct dp_usbpd *usbpd, struct dp_link *link,
			struct dp_aux *aux, struct drm_connector **connector,
			struct dp_catalog *catalog)
{
	int rc = 0;
	struct dp_debug_private *debug;
	struct dp_debug *dp_debug;

	if (!dev || !panel || !usbpd || !link || !catalog) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	debug = devm_kzalloc(dev, sizeof(*debug), GFP_KERNEL);
	if (!debug) {
		rc = -ENOMEM;
		goto error;
	}

	INIT_WORK(&debug->sim_work, dp_debug_sim_work);

	debug->dp_debug.debug_en = false;
	debug->usbpd = usbpd;
	debug->link = link;
	debug->panel = panel;
	debug->aux = aux;
	debug->dev = dev;
	debug->connector = connector;
	debug->catalog = catalog;

	dp_debug = &debug->dp_debug;
	dp_debug->vdisplay = 0;
	dp_debug->hdisplay = 0;
	dp_debug->vrefresh = 0;

	mutex_init(&debug->lock);

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

	debugfs_remove_recursive(debug->root);

	return 0;
}

void dp_debug_put(struct dp_debug *dp_debug)
{
	struct dp_debug_private *debug;

	if (!dp_debug)
		return;

	debug = container_of(dp_debug, struct dp_debug_private, dp_debug);

	dp_debug_deinit(dp_debug);

	mutex_destroy(&debug->lock);

	if (debug->edid)
		devm_kfree(debug->dev, debug->edid);

	if (debug->dpcd)
		devm_kfree(debug->dev, debug->dpcd);

	devm_kfree(debug->dev, debug);
}
