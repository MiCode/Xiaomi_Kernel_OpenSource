// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/slab.h>
#include <drm/drm_dp_mst_helper.h>
#include <drm/drm_probe_helper.h>

#include "dp_power.h"
#include "dp_catalog.h"
#include "dp_aux.h"
#include "dp_debug.h"
#include "drm/drm_connector.h"
#include "sde_connector.h"
#include "dp_display.h"
#include "dp_pll.h"
#include "dp_hpd.h"
#include "dp_mst_sim.h"
#include "dp_mst_drm.h"

#define DEBUG_NAME "drm_dp"

struct dp_debug_private {
	struct dentry *root;

	u32 dpcd_offset;
	u32 dpcd_size;

	u32 mst_con_id;
	u32 mst_edid_idx;
	bool hotplug;
	u32 sim_mode;

	char exe_mode[SZ_32];
	char reg_dump[SZ_32];

	struct dp_hpd *hpd;
	struct dp_link *link;
	struct dp_panel *panel;
	struct dp_aux *aux;
	struct dp_catalog *catalog;
	struct drm_connector **connector;
	struct device *dev;
	struct dp_debug dp_debug;
	struct dp_parser *parser;
	struct dp_ctrl *ctrl;
	struct dp_pll *pll;
	struct dp_display *display;
	struct mutex lock;
	struct dp_aux_bridge *sim_bridge;
};

static int dp_debug_sim_hpd_cb(void *arg, bool hpd, bool hpd_irq)
{
	struct dp_debug_private *debug = arg;
	int vdo = 0;

	if (hpd_irq) {
		vdo |= BIT(7);

		if (hpd)
			vdo |= BIT(8);

		return debug->hpd->simulate_attention(debug->hpd, vdo);
	} else {
		return debug->hpd->simulate_connect(debug->hpd, hpd);
	}
}

static int dp_debug_attach_sim_bridge(struct dp_debug_private *debug)
{
	int ret;

	if (debug->sim_bridge)
		return 0;

	ret = dp_sim_create_bridge(debug->dev, &debug->sim_bridge);
	if (ret)
		return ret;

	dp_sim_update_port_num(debug->sim_bridge, 1);

	if (debug->sim_bridge->register_hpd)
		debug->sim_bridge->register_hpd(debug->sim_bridge,
				dp_debug_sim_hpd_cb, debug);

	return 0;
}

static void dp_debug_enable_sim_mode(struct dp_debug_private *debug,
		u32 mode_mask)
{
	/* return if mode is already enabled */
	if ((debug->sim_mode & mode_mask) == mode_mask)
		return;

	/* create bridge if not yet */
	if (dp_debug_attach_sim_bridge(debug))
		return;

	/* switch to bridge mode */
	if (!debug->sim_mode)
		debug->aux->set_sim_mode(debug->aux, debug->sim_bridge);

	/* update sim mode */
	debug->sim_mode |= mode_mask;
	dp_sim_set_sim_mode(debug->sim_bridge, debug->sim_mode);
}

static void dp_debug_disable_sim_mode(struct dp_debug_private *debug,
		u32 mode_mask)
{
	/* return if mode is already disabled */
	if (!(debug->sim_mode & mode_mask))
		return;

	/* update sim mode */
	debug->sim_mode &= ~mode_mask;
	dp_sim_set_sim_mode(debug->sim_bridge, debug->sim_mode);

	/* switch to normal mode */
	if (!debug->sim_mode)
		debug->aux->set_sim_mode(debug->aux, NULL);
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
	size = edid_size;

	edid = kzalloc(size, GFP_KERNEL);
	if (!edid)
		goto bail;

	while (size--) {
		char t[3];
		int d;

		memcpy(t, buf_t, sizeof(char) * char_to_nib);
		t[char_to_nib] = '\0';

		if (kstrtoint(t, 16, &d)) {
			DP_ERR("kstrtoint error\n");
			goto bail;
		}

		edid[edid_buf_index++] = d;
		buf_t += char_to_nib;
	}

	dp_debug_enable_sim_mode(debug, DP_SIM_MODE_EDID);
	dp_mst_clear_edid_cache(debug->display);
	dp_sim_update_port_edid(debug->sim_bridge, debug->mst_edid_idx,
			edid, edid_size);
bail:
	kfree(buf);
	kfree(edid);

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
	u32 offset, data_len;

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
		DP_ERR("offset kstrtoint error\n");
		goto bail;
	}
	debug->dpcd_offset = offset;

	size -= 4;
	if (size < char_to_nib)
		goto bail;

	dpcd_size = size / char_to_nib;
	data_len = dpcd_size;
	buf_t = buf + 4;

	dpcd = kzalloc(dpcd_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(dpcd)) {
		rc = -ENOMEM;
		goto bail;
	}

	while (dpcd_size--) {
		char t[3];
		int d;

		memcpy(t, buf_t, sizeof(char) * char_to_nib);
		t[char_to_nib] = '\0';

		if (kstrtoint(t, 16, &d)) {
			DP_ERR("kstrtoint error\n");
			goto bail;
		}

		dpcd[dpcd_buf_index++] = d;

		buf_t += char_to_nib;
	}

	/*
	 * if link training status registers are reprogramed,
	 * read link training status from simulator, otherwise
	 * read link training status from real aux channel.
	 */
	if (offset <= DP_LANE0_1_STATUS &&
			offset + dpcd_buf_index > DP_LANE0_1_STATUS)
		dp_debug_enable_sim_mode(debug,
			DP_SIM_MODE_DPCD_READ | DP_SIM_MODE_LINK_TRAIN);
	else
		dp_debug_enable_sim_mode(debug, DP_SIM_MODE_DPCD_READ);

	dp_sim_write_dpcd_reg(debug->sim_bridge,
			dpcd, dpcd_buf_index, offset);
	debug->dpcd_size = dpcd_buf_index;

bail:
	kfree(buf);
	kfree(dpcd);

	mutex_unlock(&debug->lock);
	return rc;
}

static ssize_t dp_debug_read_dpcd(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char *buf;
	int const buf_size = SZ_4K;
	u32 offset = 0;
	u32 len = 0;
	u8 *dpcd;

	if (!debug || !debug->aux)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(buf_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	mutex_lock(&debug->lock);
	dpcd = kzalloc(buf_size, GFP_KERNEL);
	if (!dpcd)
		goto bail;

	/*
	 * In simulation mode, this function returns the last written DPCD node.
	 * For a real monitor plug in, it dumps the first byte at the last written DPCD address
	 * unless the address is 0, in which case the first 20 bytes are dumped
	 */
	if (debug->dp_debug.sim_mode) {
		dp_sim_read_dpcd_reg(debug->sim_bridge, dpcd, debug->dpcd_size, debug->dpcd_offset);
	} else {
		if (debug->dpcd_offset) {
			debug->dpcd_size = 1;
			if (drm_dp_dpcd_read(debug->aux->drm_aux, debug->dpcd_offset, dpcd,
					debug->dpcd_size) != 1)
				goto bail;
		} else {
			debug->dpcd_size = sizeof(debug->panel->dpcd);
			memcpy(dpcd, debug->panel->dpcd, debug->dpcd_size);
		}
	}

	len += scnprintf(buf + len , buf_size - len, "%04x: ", debug->dpcd_offset);

	while (offset < debug->dpcd_size)
		len += scnprintf(buf + len, buf_size - len, "%02x ", dpcd[offset++]);

	kfree(dpcd);

	len = min_t(size_t, count, len);
	if (!copy_to_user(user_buff, buf, len))
		*ppos += len;

bail:
	mutex_unlock(&debug->lock);
	kfree(buf);

	return len;
}

static ssize_t dp_debug_write_hpd(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	int const hpd_data_mask = 0x7;
	int hpd = 0;

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

	hpd &= hpd_data_mask;
	debug->hotplug = !!(hpd & BIT(0));

	debug->dp_debug.psm_enabled = !!(hpd & BIT(1));

	/*
	 * print hotplug value as this code is executed
	 * only while running in debug mode which is manually
	 * triggered by a tester or a script.
	 */
	DP_INFO("%s\n", debug->hotplug ? "[CONNECT]" : "[DISCONNECT]");

	debug->hpd->simulate_connect(debug->hpd, debug->hotplug);
end:
	return len;
}

static ssize_t dp_debug_write_edid_modes(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	struct dp_panel *panel;
	char buf[SZ_32];
	size_t len = 0;
	int hdisplay = 0, vdisplay = 0, vrefresh = 0, aspect_ratio;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		goto end;

	panel = debug->panel;

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

	panel->mode_override = true;
	panel->hdisplay = hdisplay;
	panel->vdisplay = vdisplay;
	panel->vrefresh = vrefresh;
	panel->aspect_ratio = aspect_ratio;
	goto end;
clear:
	DP_DEBUG("clearing debug modes\n");
	panel->mode_override = false;
end:
	return len;
}

static ssize_t dp_debug_write_edid_modes_mst(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	struct drm_connector *connector;
	struct sde_connector *sde_conn;
	struct dp_panel *panel = NULL;
	char buf[SZ_512];
	char *read_buf;
	size_t len = 0;

	int hdisplay = 0, vdisplay = 0, vrefresh = 0, aspect_ratio = 0;
	int con_id = 0, offset = 0, debug_en = 0;

	if (!debug)
		return -ENODEV;

	mutex_lock(&debug->lock);

	if (*ppos)
		goto end;

	len = min_t(size_t, count, SZ_512 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';
	read_buf = buf;

	while (sscanf(read_buf, "%d %d %d %d %d %d%n", &debug_en, &con_id,
			&hdisplay, &vdisplay, &vrefresh, &aspect_ratio,
			&offset) == 6) {
		connector = drm_connector_lookup((*debug->connector)->dev,
				NULL, con_id);
		if (connector) {
			sde_conn = to_sde_connector(connector);
			panel = sde_conn->drv_panel;
			if (panel && sde_conn->mst_port) {
				panel->mode_override = debug_en;
				panel->hdisplay = hdisplay;
				panel->vdisplay = vdisplay;
				panel->vrefresh = vrefresh;
				panel->aspect_ratio = aspect_ratio;
			} else {
				DP_ERR("connector id %d is not mst\n", con_id);
			}
			drm_connector_put(connector);
		} else {
			DP_ERR("invalid connector id %d\n", con_id);
		}

		read_buf += offset;
	}
end:
	mutex_unlock(&debug->lock);
	return len;
}

static ssize_t dp_debug_write_mst_con_id(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	struct drm_connector *connector;
	struct sde_connector *sde_conn;
	struct drm_dp_mst_port *mst_port;
	struct dp_panel *dp_panel;
	char buf[SZ_32];
	size_t len = 0;
	int con_id = 0, status;

	if (!debug)
		return -ENODEV;

	mutex_lock(&debug->lock);

	if (*ppos)
		goto end;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_32 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto clear;

	buf[len] = '\0';

	if (sscanf(buf, "%d %d", &con_id, &status) != 2)
		goto end;

	if (!con_id)
		goto clear;

	connector = drm_connector_lookup((*debug->connector)->dev,
			NULL, con_id);
	if (!connector) {
		DP_ERR("invalid connector id %u\n", con_id);
		goto end;
	}

	sde_conn = to_sde_connector(connector);

	if (!sde_conn->drv_panel || !sde_conn->mst_port) {
		DP_ERR("invalid connector state %d\n", con_id);
		goto out;
	}

	debug->mst_con_id = con_id;

	if (status == connector_status_connected)
		DP_INFO("plug mst connector %d\n", con_id);
	else
		DP_INFO("unplug mst connector %d\n", con_id);

	if (status == connector_status_unknown)
		goto out;

	mst_port = sde_conn->mst_port;
	dp_panel = sde_conn->drv_panel;

	if (debug->dp_debug.sim_mode) {
		dp_sim_update_port_status(debug->sim_bridge,
				mst_port->port_num, status);
	} else {
		dp_panel->mst_hide = (status == connector_status_disconnected);
		drm_kms_helper_hotplug_event(connector->dev);
	}

out:
	drm_connector_put(connector);
	goto end;
clear:
	DP_DEBUG("clearing mst_con_id\n");
	debug->mst_con_id = 0;
end:
	mutex_unlock(&debug->lock);
	return len;
}

static ssize_t dp_debug_write_mst_con_add(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_32];
	size_t len = 0;
	const int dp_en = BIT(3), hpd_high = BIT(7), hpd_irq = BIT(8);
	int vdo = dp_en | hpd_high | hpd_irq;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_32 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	debug->dp_debug.mst_sim_add_con = true;
	debug->hpd->simulate_attention(debug->hpd, vdo);
end:
	return len;
}

static ssize_t dp_debug_write_mst_con_remove(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	struct drm_connector_list_iter conn_iter;
	struct drm_connector *connector;
	char buf[SZ_32];
	size_t len = 0;
	int con_id = 0;
	bool in_list = false;
	const int dp_en = BIT(3), hpd_high = BIT(7), hpd_irq = BIT(8);
	int vdo = dp_en | hpd_high | hpd_irq;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_32 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (sscanf(buf, "%d", &con_id) != 1) {
		len = 0;
		goto end;
	}

	if (!con_id)
		goto end;

	drm_connector_list_iter_begin((*debug->connector)->dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if (connector->base.id == con_id) {
			in_list = true;
			break;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	if (!in_list) {
		DRM_ERROR("invalid connector id %u\n", con_id);
		goto end;
	}

	debug->dp_debug.mst_sim_remove_con = true;
	debug->dp_debug.mst_sim_remove_con_id = con_id;
	debug->hpd->simulate_attention(debug->hpd, vdo);
end:
	return len;
}

static ssize_t dp_debug_mmrm_clk_cb_write(struct file *file,
		 const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	struct dss_clk_mmrm_cb mmrm_cb_data;
	struct mmrm_client_notifier_data notifier_data;
	struct dp_display *dp_display;
	int cb_type;

	if (!debug)
		return -ENODEV;
	if (*ppos)
		return 0;

	dp_display = debug->display;

	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		return 0;

	buf[len] = '\0';

	if (kstrtoint(buf, 10, &cb_type) != 0)
		return 0;
	if (cb_type != MMRM_CLIENT_RESOURCE_VALUE_CHANGE)
		return 0;

	notifier_data.cb_type = MMRM_CLIENT_RESOURCE_VALUE_CHANGE;
	mmrm_cb_data.phandle = (void *)dp_display;
	notifier_data.pvt_data = (void *)&mmrm_cb_data;

	dp_display_mmrm_callback(&notifier_data);

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
		DP_ERR("Unsupported bw code %d\n", max_bw_code);
		return len;
	}
	debug->panel->max_bw_code = max_bw_code;
	DP_DEBUG("max_bw_code: %d\n", max_bw_code);

	return len;
}

static ssize_t dp_debug_mst_mode_read(struct file *file,
	char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[64];
	ssize_t len;

	len = scnprintf(buf, sizeof(buf),
			"mst_mode = %d, mst_state = %d\n",
			debug->parser->has_mst,
			debug->panel->mst_state);

	return simple_read_from_buffer(user_buff, count, ppos, buf, len);
}

static ssize_t dp_debug_mst_mode_write(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	u32 mst_mode = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		return 0;

	buf[len] = '\0';

	if (kstrtoint(buf, 10, &mst_mode) != 0)
		return 0;

	debug->parser->has_mst = mst_mode ? true : false;
	DP_DEBUG("mst_enable: %d\n", mst_mode);

	return len;
}

static ssize_t dp_debug_max_pclk_khz_write(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	u32 max_pclk = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		return 0;

	buf[len] = '\0';

	if (kstrtoint(buf, 10, &max_pclk) != 0)
		return 0;

	if (max_pclk > debug->parser->max_pclk_khz)
		DP_ERR("requested: %d, max_pclk_khz:%d\n", max_pclk,
				debug->parser->max_pclk_khz);
	else
		debug->dp_debug.max_pclk_khz = max_pclk;

	DP_DEBUG("max_pclk_khz: %d\n", max_pclk);

	return len;
}

static ssize_t dp_debug_max_pclk_khz_read(struct file *file,
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
			"max_pclk_khz = %d, org: %d\n",
			debug->dp_debug.max_pclk_khz,
			debug->parser->max_pclk_khz);

	len = min_t(size_t, count, len);
	if (copy_to_user(user_buff, buf, len)) {
		kfree(buf);
		return -EFAULT;
	}

	*ppos += len;
	kfree(buf);
	return len;
}

static ssize_t dp_debug_mst_sideband_mode_write(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	int mst_sideband_mode = 0;
	u32 mst_port_cnt = 0;

	if (!debug)
		return -ENODEV;

	mutex_lock(&debug->lock);

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		return -EFAULT;

	buf[len] = '\0';

	if (sscanf(buf, "%d %u", &mst_sideband_mode, &mst_port_cnt) != 2) {
		DP_ERR("invalid input\n");
		goto bail;
	}

	if (!mst_port_cnt)
		mst_port_cnt = 1;

	debug->mst_edid_idx = 0;

	if (mst_sideband_mode)
		dp_debug_disable_sim_mode(debug, DP_SIM_MODE_MST);
	else
		dp_debug_enable_sim_mode(debug, DP_SIM_MODE_MST);

	dp_sim_update_port_num(debug->sim_bridge, mst_port_cnt);

	buf[0] = !mst_sideband_mode;
	dp_sim_write_dpcd_reg(debug->sim_bridge, buf, 1, DP_MSTM_CAP);

	DP_DEBUG("mst_sideband_mode: %d port_cnt:%d\n",
			mst_sideband_mode, mst_port_cnt);

bail:
	mutex_unlock(&debug->lock);
	return count;
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
	DP_DEBUG("tpg_state: %d\n", tpg_state);

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

	len += snprintf(buf, SZ_8, "%d\n", debug->hpd->hpd_high);

	len = min_t(size_t, count, len);
	if (copy_to_user(user_buff, buf, len))
		return -EFAULT;

	*ppos += len;
	return len;
}

static ssize_t dp_debug_write_hdcp(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char buf[SZ_8];
	size_t len = 0;
	int hdcp = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';

	if (kstrtoint(buf, 10, &hdcp) != 0)
		goto end;

	debug->dp_debug.hdcp_disabled = !hdcp;
end:
	return len;
}

static ssize_t dp_debug_read_hdcp(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	u32 len = 0;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		return 0;

	len = sizeof(debug->dp_debug.hdcp_status);

	len = min_t(size_t, count, len);
	if (copy_to_user(user_buff, debug->dp_debug.hdcp_status, len))
		return -EFAULT;

	*ppos += len;
	return len;
}

static int dp_debug_check_buffer_overflow(int rc, int *max_size, int *len)
{
	if (rc >= *max_size) {
		DP_ERR("buffer overflow\n");
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
		DP_ERR("invalid data\n");
		rc = -ENODEV;
		goto error;
	}

	connector = *debug->connector;

	if (!connector) {
		DP_ERR("connector is NULL\n");
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
		mode->name, drm_mode_vrefresh(mode), mode->picture_aspect_ratio,
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

static ssize_t dp_debug_read_edid_modes_mst(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char *buf;
	u32 len = 0, ret = 0, max_size = SZ_4K;
	struct drm_connector *connector;
	struct drm_display_mode *mode;

	if (!debug) {
		DP_ERR("invalid data\n");
		return -ENODEV;
	}

	if (*ppos)
		return 0;

	connector = drm_connector_lookup((*debug->connector)->dev,
			NULL, debug->mst_con_id);
	if (!connector) {
		DP_ERR("connector %u not in mst list\n", debug->mst_con_id);
		return 0;
	}

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!buf)
		goto clean;

	mutex_lock(&connector->dev->mode_config.mutex);
	list_for_each_entry(mode, &connector->modes, head) {
		ret = snprintf(buf + len, max_size,
				"%s %d %d %d %d %d 0x%x\n",
				mode->name, drm_mode_vrefresh(mode),
				mode->picture_aspect_ratio, mode->htotal,
				mode->vtotal, mode->clock, mode->flags);
		if (dp_debug_check_buffer_overflow(ret, &max_size, &len))
			break;
	}
	mutex_unlock(&connector->dev->mode_config.mutex);

	len = min_t(size_t, count, len);
	if (copy_to_user(user_buff, buf, len)) {
		len = -EFAULT;
		goto clean;
	}

	*ppos += len;
clean:
	kfree(buf);
	drm_connector_put(connector);
	return len;
}

static ssize_t dp_debug_read_mst_con_id(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char *buf;
	u32 len = 0, ret = 0, max_size = SZ_4K;
	int rc = 0;

	if (!debug) {
		DP_ERR("invalid data\n");
		rc = -ENODEV;
		goto error;
	}

	if (*ppos)
		goto error;

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!buf) {
		rc = -ENOMEM;
		goto error;
	}

	ret = snprintf(buf, max_size, "%u\n", debug->mst_con_id);
	len += ret;

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

static ssize_t dp_debug_read_mst_conn_info(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	struct drm_connector_list_iter conn_iter;
	struct drm_connector *connector;
	struct sde_connector *sde_conn;
	struct dp_display *display;
	char *buf;
	u32 len = 0, ret = 0, max_size = SZ_4K;
	int rc = 0;

	if (!debug) {
		DP_ERR("invalid data\n");
		rc = -ENODEV;
		goto error;
	}

	if (*ppos)
		goto error;

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!buf) {
		rc = -ENOMEM;
		goto error;
	}

	drm_connector_list_iter_begin((*debug->connector)->dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		sde_conn = to_sde_connector(connector);
		display = sde_conn->display;
		if (!sde_conn->mst_port ||
				display->base_connector != (*debug->connector))
			continue;
		ret = scnprintf(buf + len, max_size,
				"conn name:%s, conn id:%d state:%d\n",
				connector->name, connector->base.id,
				connector->status);
		if (dp_debug_check_buffer_overflow(ret, &max_size, &len))
			break;
	}
	drm_connector_list_iter_end(&conn_iter);

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

static int dp_debug_print_hdr_params_to_buf(struct drm_connector *connector,
		char *buf, u32 size)
{
	int rc;
	u32 i, len = 0, max_size = size;
	struct sde_connector *c_conn;
	struct sde_connector_state *c_state;
	struct drm_msm_ext_hdr_metadata *hdr;

	c_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(connector->state);

	hdr = &c_state->hdr_meta;

	rc = snprintf(buf + len, max_size,
		"============SINK HDR PARAMETERS===========\n");
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "eotf = %d\n",
		c_conn->hdr_eotf);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "type_one = %d\n",
		c_conn->hdr_metadata_type_one);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "hdr_plus_app_ver = %d\n",
			c_conn->hdr_plus_app_ver);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "max_luminance = %d\n",
		c_conn->hdr_max_luminance);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "avg_luminance = %d\n",
		c_conn->hdr_avg_luminance);
	if (dp_debug_check_buffer_overflow(rc, &max_size, &len))
		goto error;

	rc = snprintf(buf + len, max_size, "min_luminance = %d\n",
		c_conn->hdr_min_luminance);
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

	if (hdr->hdr_plus_payload && hdr->hdr_plus_payload_size) {
		u32 rowsize = 16, rem;
		struct sde_connector_dyn_hdr_metadata *dhdr =
				&c_state->dyn_hdr_meta;

		/**
		 * Do not use user pointer from hdr->hdr_plus_payload directly,
		 * instead use kernel's cached copy of payload data.
		 */
		for (i = 0; i < dhdr->dynamic_hdr_payload_size; i += rowsize) {
			rc = snprintf(buf + len, max_size, "DHDR: ");
			if (dp_debug_check_buffer_overflow(rc, &max_size,
					&len))
				goto error;

			rem = dhdr->dynamic_hdr_payload_size - i;
			rc = hex_dump_to_buffer(&dhdr->dynamic_hdr_payload[i],
				min(rowsize, rem), rowsize, 1, buf + len,
				max_size, false);
			if (dp_debug_check_buffer_overflow(rc, &max_size,
					&len))
				goto error;

			rc = snprintf(buf + len, max_size, "\n");
			if (dp_debug_check_buffer_overflow(rc, &max_size,
					&len))
				goto error;
		}
	}

	return len;
error:
	return -EOVERFLOW;
}

static ssize_t dp_debug_read_hdr(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char *buf = NULL;
	u32 len = 0;
	u32 max_size = SZ_4K;
	struct drm_connector *connector;

	if (!debug) {
		DP_ERR("invalid data\n");
		return -ENODEV;
	}

	connector = *debug->connector;

	if (!connector) {
		DP_ERR("connector is NULL\n");
		return -EINVAL;
	}

	if (*ppos)
		return 0;

	buf = kzalloc(max_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf))
		return -ENOMEM;

	len = dp_debug_print_hdr_params_to_buf(connector, buf, max_size);
	if (len == -EOVERFLOW) {
		kfree(buf);
		return len;
	}

	len = min_t(size_t, count, len);
	if (copy_to_user(user_buff, buf, len)) {
		kfree(buf);
		return -EFAULT;
	}

	*ppos += len;
	kfree(buf);
	return len;
}

static ssize_t dp_debug_read_hdr_mst(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	char *buf = NULL;
	u32 len = 0, max_size = SZ_4K;
	struct drm_connector_list_iter conn_iter;
	struct drm_connector *connector;
	bool in_list = false;

	if (!debug) {
		DP_ERR("invalid data\n");
		return -ENODEV;
	}

	drm_connector_list_iter_begin((*debug->connector)->dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if (connector->base.id == debug->mst_con_id) {
			in_list = true;
			break;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	if (!in_list) {
		DP_ERR("connector %u not in mst list\n", debug->mst_con_id);
		return -EINVAL;
	}

	if (!connector) {
		DP_ERR("connector is NULL\n");
		return -EINVAL;
	}

	if (*ppos)
		return 0;


	buf = kzalloc(max_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf))
		return -ENOMEM;

	len = dp_debug_print_hdr_params_to_buf(connector, buf, max_size);
	if (len == -EOVERFLOW) {
		kfree(buf);
		return len;
	}

	len = min_t(size_t, count, len);
	if (copy_to_user(user_buff, buf, len)) {
		kfree(buf);
		return -EFAULT;
	}

	*ppos += len;
	kfree(buf);
	return len;
}

static void dp_debug_set_sim_mode(struct dp_debug_private *debug, bool sim)
{
	struct drm_connector_list_iter conn_iter;
	struct drm_connector *connector;
	struct sde_connector *sde_conn;
	struct dp_display *display;
	struct dp_panel *panel;

	if (sim) {
		debug->dp_debug.sim_mode = true;
		dp_debug_enable_sim_mode(debug, DP_SIM_MODE_ALL);
	} else {
		if (debug->hotplug) {
			DP_WARN("sim mode off before hotplug disconnect\n");
			debug->hpd->simulate_connect(debug->hpd, false);
			debug->hotplug = false;
		}
		debug->aux->abort(debug->aux, true);
		debug->ctrl->abort(debug->ctrl, true);

		debug->dp_debug.sim_mode = false;

		debug->mst_edid_idx = 0;
		dp_debug_disable_sim_mode(debug, DP_SIM_MODE_ALL);
	}

	/* clear override settings in panel */
	drm_connector_list_iter_begin((*debug->connector)->dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		sde_conn = to_sde_connector(connector);
		display = sde_conn->display;
		if (display->base_connector == (*debug->connector)) {
			panel = sde_conn->drv_panel;
			panel->mode_override = false;
			panel->mst_hide = false;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	/*
	 * print simulation status as this code is executed
	 * only while running in debug mode which is manually
	 * triggered by a tester or a script.
	 */
	DP_INFO("%s\n", sim ? "[ON]" : "[OFF]");
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

	dp_debug_set_sim_mode(debug, sim);
end:
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

	debug->hpd->simulate_attention(debug->hpd, vdo);
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

	if (!debug->hpd->hpd_high || !strlen(debug->reg_dump))
		goto end;

	rc = debug->catalog->get_reg_dump(debug->catalog,
		debug->reg_dump, &buf, &len);
	if (rc)
		goto end;

	snprintf(prefix, sizeof(prefix), "%s: ", debug->reg_dump);
	print_hex_dump_debug(prefix, DUMP_PREFIX_NONE,
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

static const struct file_operations edid_modes_mst_fops = {
	.open = simple_open,
	.read = dp_debug_read_edid_modes_mst,
	.write = dp_debug_write_edid_modes_mst,
};

static const struct file_operations mst_conn_info_fops = {
	.open = simple_open,
	.read = dp_debug_read_mst_conn_info,
};

static const struct file_operations mst_con_id_fops = {
	.open = simple_open,
	.read = dp_debug_read_mst_con_id,
	.write = dp_debug_write_mst_con_id,
};

static const struct file_operations mst_con_add_fops = {
	.open = simple_open,
	.write = dp_debug_write_mst_con_add,
};

static const struct file_operations mst_con_remove_fops = {
	.open = simple_open,
	.write = dp_debug_write_mst_con_remove,
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
	.read = dp_debug_read_hdr,
};

static const struct file_operations hdr_mst_fops = {
	.open = simple_open,
	.read = dp_debug_read_hdr_mst,
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

static const struct file_operations mst_mode_fops = {
	.open = simple_open,
	.write = dp_debug_mst_mode_write,
	.read = dp_debug_mst_mode_read,
};

static const struct file_operations mst_sideband_mode_fops = {
	.open = simple_open,
	.write = dp_debug_mst_sideband_mode_write,
};

static const struct file_operations max_pclk_khz_fops = {
	.open = simple_open,
	.write = dp_debug_max_pclk_khz_write,
	.read = dp_debug_max_pclk_khz_read,
};

static const struct file_operations hdcp_fops = {
	.open = simple_open,
	.write = dp_debug_write_hdcp,
	.read = dp_debug_read_hdcp,
};

static const struct file_operations mmrm_clk_cb_fops = {
	.open = simple_open,
	.write = dp_debug_mmrm_clk_cb_write,
};

static int dp_debug_init_mst(struct dp_debug_private *debug, struct dentry *dir)
{
	int rc = 0;
	struct dentry *file;

	file = debugfs_create_file("mst_con_id", 0644, dir,
					debug, &mst_con_id_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs create mst_con_id failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("mst_con_info", 0644, dir,
					debug, &mst_conn_info_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs create mst_conn_info failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("mst_con_add", 0644, dir,
					debug, &mst_con_add_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DRM_ERROR("[%s] debugfs create mst_con_add failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("mst_con_remove", 0644, dir,
					debug, &mst_con_remove_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DRM_ERROR("[%s] debugfs create mst_con_remove failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("mst_mode", 0644, dir,
			debug, &mst_mode_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs mst_mode failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("mst_sideband_mode", 0644, dir,
			debug, &mst_sideband_mode_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs mst_sideband_mode failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	debugfs_create_u32("mst_edid_idx", 0644, dir, &debug->mst_edid_idx);

	return rc;
}

static int dp_debug_init_link(struct dp_debug_private *debug,
		struct dentry *dir)
{
	int rc = 0;
	struct dentry *file;

	file = debugfs_create_file("max_bw_code", 0644, dir,
			debug, &bw_code_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs max_bw_code failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("max_pclk_khz", 0644, dir,
			debug, &max_pclk_khz_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs max_pclk_khz failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	debugfs_create_u32("max_lclk_khz", 0644, dir, &debug->parser->max_lclk_khz);

	debugfs_create_u32("lane_count", 0644, dir, &debug->panel->lane_count);

	debugfs_create_u32("link_bw_code", 0644, dir, &debug->panel->link_bw_code);

	file = debugfs_create_file("mmrm_clk_cb", 0644, dir, debug, &mmrm_clk_cb_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs mmrm_clk_cb failed, rc=%d\n", DEBUG_NAME, rc);
		return rc;
	}

	return rc;
}

static int dp_debug_init_hdcp(struct dp_debug_private *debug,
		struct dentry *dir)
{
	int rc = 0;
	struct dentry *file;

	file = debugfs_create_bool("hdcp_wait_sink_sync", 0644, dir,
			&debug->dp_debug.hdcp_wait_sink_sync);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs hdcp_wait_sink_sync failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_bool("force_encryption", 0644, dir,
			&debug->dp_debug.force_encryption);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs force_encryption failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	return rc;
}

static int dp_debug_init_sink_caps(struct dp_debug_private *debug,
		struct dentry *dir)
{
	int rc = 0;
	struct dentry *file;

	file = debugfs_create_file("edid_modes", 0644, dir,
					debug, &edid_modes_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs create edid_modes failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("edid_modes_mst", 0644, dir,
					debug, &edid_modes_mst_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs create edid_modes_mst failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("edid", 0644, dir,
					debug, &edid_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs edid failed, rc=%d\n",
			DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("dpcd", 0644, dir,
					debug, &dpcd_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs dpcd failed, rc=%d\n",
			DEBUG_NAME, rc);
		return rc;
	}

	return rc;
}

static int dp_debug_init_status(struct dp_debug_private *debug,
		struct dentry *dir)
{
	int rc = 0;
	struct dentry *file;

	file = debugfs_create_file("dp_debug", 0444, dir,
				debug, &dp_debug_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs create file failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("connected", 0444, dir,
					debug, &connected_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs connected failed, rc=%d\n",
			DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("hdr", 0400, dir, debug, &hdr_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs hdr failed, rc=%d\n",
			DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("hdr_mst", 0400, dir, debug, &hdr_mst_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs hdr_mst failed, rc=%d\n",
			DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("hdcp", 0644, dir, debug, &hdcp_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs hdcp failed, rc=%d\n",
			DEBUG_NAME, rc);
		return rc;
	}

	return rc;
}

static int dp_debug_init_sim(struct dp_debug_private *debug, struct dentry *dir)
{
	int rc = 0;
	struct dentry *file;

	file = debugfs_create_file("hpd", 0644, dir, debug, &hpd_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs hpd failed, rc=%d\n",
			DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("sim", 0644, dir, debug, &sim_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs sim failed, rc=%d\n",
			DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("attention", 0644, dir,
			debug, &attention_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs attention failed, rc=%d\n",
			DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_bool("skip_uevent", 0644, dir,
			&debug->dp_debug.skip_uevent);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs skip_uevent failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_bool("force_multi_func", 0644, dir,
			&debug->hpd->force_multi_func);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs force_multi_func failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	return rc;
}

static int dp_debug_init_dsc_fec(struct dp_debug_private *debug,
		struct dentry *dir)
{
	int rc = 0;
	struct dentry *file;

	file = debugfs_create_bool("dsc_feature_enable", 0644, dir,
			&debug->parser->dsc_feature_enable);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs dsc_feature failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_bool("fec_feature_enable", 0644, dir,
			&debug->parser->fec_feature_enable);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs fec_feature_enable failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	return rc;
}

static int dp_debug_init_tpg(struct dp_debug_private *debug, struct dentry *dir)
{
	int rc = 0;
	struct dentry *file;

	file = debugfs_create_file("tpg_ctrl", 0644, dir,
			debug, &tpg_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs tpg failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	return rc;
}

static int dp_debug_init_reg_dump(struct dp_debug_private *debug,
		struct dentry *dir)
{
	int rc = 0;
	struct dentry *file;

	file = debugfs_create_file("exe_mode", 0644, dir,
			debug, &exe_mode_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs register failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_file("dump", 0644, dir,
		debug, &dump_fops);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs dump failed, rc=%d\n",
			DEBUG_NAME, rc);
		return rc;
	}

	return rc;
}

static int dp_debug_init_feature_toggle(struct dp_debug_private *debug,
		struct dentry *dir)
{
	int rc = 0;
	struct dentry *file;

	file = debugfs_create_bool("ssc_enable", 0644, dir,
			&debug->pll->ssc_en);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs ssc_enable failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_bool("widebus_mode", 0644, dir,
			&debug->parser->has_widebus);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs widebus_mode failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	return rc;
}

static int dp_debug_init_configs(struct dp_debug_private *debug,
		struct dentry *dir)
{
	int rc = 0;
	struct dentry *file;

	file = debugfs_create_ulong("connect_notification_delay_ms", 0644, dir,
		&debug->dp_debug.connect_notification_delay_ms);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs connect_notification_delay_ms failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}
	debug->dp_debug.connect_notification_delay_ms =
		DEFAULT_CONNECT_NOTIFICATION_DELAY_MS;

	debugfs_create_u32("disconnect_delay_ms", 0644, dir, &debug->dp_debug.disconnect_delay_ms);

	debug->dp_debug.disconnect_delay_ms = DEFAULT_DISCONNECT_DELAY_MS;

	return rc;

}

static int dp_debug_init(struct dp_debug *dp_debug)
{
	int rc = 0;
	struct dp_debug_private *debug = container_of(dp_debug,
		struct dp_debug_private, dp_debug);
	struct dentry *dir;

	if (!IS_ENABLED(CONFIG_DEBUG_FS)) {
		DP_WARN("Not creating debug root dir.");
		debug->root = NULL;
		return 0;
	}

	dir = debugfs_create_dir(DEBUG_NAME, NULL);
	if (IS_ERR_OR_NULL(dir)) {
		if (!dir)
			rc = -EINVAL;
		else
			rc = PTR_ERR(dir);
		DP_ERR("[%s] debugfs create dir failed, rc = %d\n",
		       DEBUG_NAME, rc);
		goto error;
	}

	debug->root = dir;

	rc = dp_debug_init_status(debug, dir);
	if (rc)
		goto error_remove_dir;

	rc = dp_debug_init_sink_caps(debug, dir);
	if (rc)
		goto error_remove_dir;

	rc = dp_debug_init_mst(debug, dir);
	if (rc)
		goto error_remove_dir;

	rc = dp_debug_init_link(debug, dir);
	if (rc)
		goto error_remove_dir;

	rc = dp_debug_init_hdcp(debug, dir);
	if (rc)
		goto error_remove_dir;

	rc = dp_debug_init_sim(debug, dir);
	if (rc)
		goto error_remove_dir;

	rc = dp_debug_init_dsc_fec(debug, dir);
	if (rc)
		goto error_remove_dir;

	rc = dp_debug_init_tpg(debug, dir);
	if (rc)
		goto error_remove_dir;

	rc = dp_debug_init_reg_dump(debug, dir);
	if (rc)
		goto error_remove_dir;

	rc = dp_debug_init_feature_toggle(debug, dir);
	if (rc)
		goto error_remove_dir;

	rc = dp_debug_init_configs(debug, dir);
	if (rc)
		goto error_remove_dir;

	return 0;

error_remove_dir:
	debugfs_remove_recursive(dir);
error:
	return rc;
}

static void dp_debug_abort(struct dp_debug *dp_debug)
{
	struct dp_debug_private *debug;

	if (!dp_debug)
		return;

	debug = container_of(dp_debug, struct dp_debug_private, dp_debug);

	mutex_lock(&debug->lock);
	dp_debug_set_sim_mode(debug, false);
	mutex_unlock(&debug->lock);
}

static void dp_debug_set_mst_con(struct dp_debug *dp_debug, int con_id)
{
	struct dp_debug_private *debug;

	if (!dp_debug)
		return;

	debug = container_of(dp_debug, struct dp_debug_private, dp_debug);
	mutex_lock(&debug->lock);
	debug->mst_con_id = con_id;
	mutex_unlock(&debug->lock);
	DP_INFO("Selecting mst connector %d\n", con_id);
}

struct dp_debug *dp_debug_get(struct dp_debug_in *in)
{
	int rc = 0;
	struct dp_debug_private *debug;
	struct dp_debug *dp_debug;

	if (!in->dev || !in->panel || !in->hpd || !in->link ||
	    !in->catalog || !in->ctrl || !in->pll) {
		DP_ERR("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	debug = devm_kzalloc(in->dev, sizeof(*debug), GFP_KERNEL);
	if (!debug) {
		rc = -ENOMEM;
		goto error;
	}

	debug->hpd = in->hpd;
	debug->link = in->link;
	debug->panel = in->panel;
	debug->aux = in->aux;
	debug->dev = in->dev;
	debug->connector = in->connector;
	debug->catalog = in->catalog;
	debug->parser = in->parser;
	debug->ctrl = in->ctrl;
	debug->pll = in->pll;
	debug->display = in->display;

	dp_debug = &debug->dp_debug;

	mutex_init(&debug->lock);

	rc = dp_debug_init(dp_debug);
	if (rc) {
		devm_kfree(in->dev, debug);
		goto error;
	}

	debug->aux->access_lock = &debug->lock;
	dp_debug->abort = dp_debug_abort;
	dp_debug->set_mst_con = dp_debug_set_mst_con;

	dp_debug->max_pclk_khz = debug->parser->max_pclk_khz;

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

	if (debug->sim_bridge)
		dp_sim_destroy_bridge(debug->sim_bridge);

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

	devm_kfree(debug->dev, debug);
}
