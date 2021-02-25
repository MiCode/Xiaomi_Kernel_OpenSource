// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/slab.h>

#include "dp_power.h"
#include "dp_catalog.h"
#include "dp_aux.h"
#include "dp_debug.h"
#include "drm_connector.h"
#include "sde_connector.h"
#include "dp_display.h"
#include "dp_pll.h"
#include "dp_hpd.h"

#define DEBUG_NAME "drm_dp"

struct dp_debug_private {
	struct dentry *root;
	u8 *edid;
	u32 edid_size;

	u8 *dpcd;
	u32 dpcd_size;

	u32 mst_con_id;
	bool hotplug;

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
		debug->dpcd = devm_kzalloc(debug->dev, SZ_4K, GFP_KERNEL);
		if (!debug->dpcd) {
			rc = -ENOMEM;
			goto end;
		}

		debug->dpcd_size = SZ_4K;
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
		DP_DEBUG("realloc debug edid\n");

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
			DP_ERR("kstrtoint error\n");
			goto bail;
		}

		if (debug->edid && (edid_buf_index < debug->edid_size))
			debug->edid[edid_buf_index++] = d;

		buf_t += char_to_nib;
	}

	edid = debug->edid;
bail:
	kfree(buf);
	debug->panel->set_edid(debug->panel, edid);

	/*
	 * print edid status as this code is executed
	 * only while running in debug mode which is manually
	 * triggered by a tester or a script.
	 */
	DP_INFO("[%s]\n", edid ? "SET" : "CLEAR");

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
	const u32 dp_receiver_cap_size = 16;

	if (!debug)
		return -ENODEV;

	mutex_lock(&debug->lock);

	if (*ppos)
		goto bail;

	size = min_t(size_t, count, SZ_2K);

	if (size <= 4)
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

	if (dp_debug_get_dpcd_buf(debug))
		goto bail;

	if (offset == 0xFFFF) {
		DP_ERR("clearing dpcd\n");
		memset(debug->dpcd, 0, debug->dpcd_size);
		goto bail;
	}

	size -= 4;
	if (size == 0)
		goto bail;

	dpcd_size = size / char_to_nib;
	data_len = dpcd_size;
	buf_t = buf + 4;

	dpcd_buf_index = offset;

	while (dpcd_size--) {
		char t[3];
		int d;

		memcpy(t, buf_t, sizeof(char) * char_to_nib);
		t[char_to_nib] = '\0';

		if (kstrtoint(t, 16, &d)) {
			DP_ERR("kstrtoint error\n");
			goto bail;
		}

		if (dpcd_buf_index < debug->dpcd_size)
			debug->dpcd[dpcd_buf_index++] = d;

		buf_t += char_to_nib;
	}

	dpcd = debug->dpcd;
bail:
	kfree(buf);

	if (!dpcd || (size / char_to_nib) >= dp_receiver_cap_size ||
	    offset == 0xffff) {
		debug->panel->set_dpcd(debug->panel, dpcd);
		/*
		 * print dpcd status as this code is executed
		 * only while running in debug mode which is manually
		 * triggered by a tester or a script.
		 */
		if (!dpcd || (offset == 0xffff))
			DP_INFO("[%s]\n", "CLEAR");
		else
			DP_INFO("[%s]\n", "SET");
	}

	mutex_unlock(&debug->lock);

	debug->aux->dpcd_updated(debug->aux);
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
	bool notify = false;

	if (!debug || !debug->aux || !debug->dpcd)
		return -ENODEV;

	mutex_lock(&debug->lock);
	if (*ppos)
		goto end;

	buf = kzalloc(buf_size, GFP_KERNEL);
	if (!buf)
		goto end;

	len += snprintf(buf, buf_size, "0x%x", debug->aux->reg);

	if (!debug->aux->read) {
		while (1) {
			if (debug->aux->reg + offset >= buf_size ||
			    offset >= debug->aux->size)
				break;

			len += snprintf(buf + len, buf_size - len, "0x%x",
				debug->dpcd[debug->aux->reg + offset++]);
		}

		notify = true;
	}

	len = min_t(size_t, count, len);
	if (!copy_to_user(user_buff, buf, len))
		*ppos += len;

	kfree(buf);
end:
	mutex_unlock(&debug->lock);

	if (notify)
		debug->aux->dpcd_updated(debug->aux);

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
	DP_DEBUG("clearing debug modes\n");
	debug->dp_debug.debug_en = false;
end:
	return len;
}

static ssize_t dp_debug_write_edid_modes_mst(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	struct dp_mst_connector *mst_connector;
	char buf[SZ_512];
	char *read_buf;
	size_t len = 0;

	int hdisplay = 0, vdisplay = 0, vrefresh = 0, aspect_ratio = 0;
	int con_id = 0, offset = 0, debug_en = 0;
	bool in_list = false;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		goto end;

	len = min_t(size_t, count, SZ_512 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto end;

	buf[len] = '\0';
	read_buf = buf;

	mutex_lock(&debug->dp_debug.dp_mst_connector_list.lock);
	while (sscanf(read_buf, "%d %d %d %d %d %d%n", &debug_en, &con_id,
			&hdisplay, &vdisplay, &vrefresh, &aspect_ratio,
			&offset) == 6) {
		list_for_each_entry(mst_connector,
				&debug->dp_debug.dp_mst_connector_list.list,
				list) {
			if (mst_connector->con_id == con_id) {
				in_list = true;
				mst_connector->debug_en = (bool) debug_en;
				mst_connector->hdisplay = hdisplay;
				mst_connector->vdisplay = vdisplay;
				mst_connector->vrefresh = vrefresh;
				mst_connector->aspect_ratio = aspect_ratio;
				DP_INFO("Setting %dx%dp%d on conn %d\n",
					hdisplay, vdisplay, vrefresh, con_id);
			}
		}

		if (!in_list)
			DP_DEBUG("dp connector id %d is invalid\n", con_id);

		in_list = false;
		read_buf += offset;
	}
	mutex_unlock(&debug->dp_debug.dp_mst_connector_list.lock);
end:
	return len;
}

static ssize_t dp_debug_write_mst_con_id(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	struct dp_mst_connector *mst_connector;
	char buf[SZ_32];
	size_t len = 0;
	int con_id = 0, status;
	bool in_list = false;
	const int dp_en = BIT(3), hpd_high = BIT(7), hpd_irq = BIT(8);
	int vdo = dp_en | hpd_high | hpd_irq;

	if (!debug)
		return -ENODEV;

	if (*ppos)
		goto end;

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_32 - 1);
	if (copy_from_user(buf, user_buff, len))
		goto clear;

	buf[len] = '\0';

	if (sscanf(buf, "%d %d", &con_id, &status) != 2) {
		len = 0;
		goto end;
	}

	if (!con_id)
		goto clear;

	/* Verify that the connector id is for a valid mst connector. */
	mutex_lock(&debug->dp_debug.dp_mst_connector_list.lock);
	list_for_each_entry(mst_connector,
			&debug->dp_debug.dp_mst_connector_list.list, list) {
		if (mst_connector->con_id == con_id) {
			in_list = true;
			debug->mst_con_id = con_id;
			mst_connector->state = status;
			break;
		}
	}
	mutex_unlock(&debug->dp_debug.dp_mst_connector_list.lock);

	if (!in_list && status != connector_status_connected) {
		DP_ERR("invalid connector id %u\n", con_id);
		goto end;
	}

	if (status == connector_status_unknown)
		goto end;

	debug->dp_debug.mst_hpd_sim = true;

	if (status == connector_status_connected) {
		DP_INFO("plug mst connector\n", con_id, status);
		debug->dp_debug.mst_sim_add_con = true;
	} else {
		DP_INFO("unplug mst connector %d\n", con_id, status);
	}

	debug->hpd->simulate_attention(debug->hpd, vdo);
	goto end;
clear:
	DP_DEBUG("clearing mst_con_id\n");
	debug->mst_con_id = 0;
end:
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

	debug->dp_debug.mst_hpd_sim = true;
	debug->dp_debug.mst_sim_add_con = true;
	debug->hpd->simulate_attention(debug->hpd, vdo);
end:
	return len;
}

static ssize_t dp_debug_write_mst_con_remove(struct file *file,
		const char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	struct dp_mst_connector *mst_connector;
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

	/* Verify that the connector id is for a valid mst connector. */
	mutex_lock(&debug->dp_debug.dp_mst_connector_list.lock);
	list_for_each_entry(mst_connector,
			&debug->dp_debug.dp_mst_connector_list.list, list) {
		if (mst_connector->con_id == con_id) {
			in_list = true;
			break;
		}
	}
	mutex_unlock(&debug->dp_debug.dp_mst_connector_list.lock);

	if (!in_list) {
		DRM_ERROR("invalid connector id %u\n", con_id);
		goto end;
	}

	debug->dp_debug.mst_hpd_sim = true;
	debug->dp_debug.mst_sim_remove_con = true;
	debug->dp_debug.mst_sim_remove_con_id = con_id;
	debug->hpd->simulate_attention(debug->hpd, vdo);
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

	/* Leave room for termination char */
	len = min_t(size_t, count, SZ_8 - 1);
	if (copy_from_user(buf, user_buff, len))
		return -EFAULT;

	buf[len] = '\0';

	if (sscanf(buf, "%d %u", &mst_sideband_mode, &mst_port_cnt) != 2) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	if (mst_port_cnt > DP_MST_SIM_MAX_PORTS) {
		DP_ERR("port cnt:%d exceeding max:%d\n", mst_port_cnt,
				DP_MST_SIM_MAX_PORTS);
		return -EINVAL;
	}

	debug->parser->has_mst_sideband = mst_sideband_mode ? true : false;
	debug->dp_debug.mst_port_cnt = mst_port_cnt;
	DP_DEBUG("mst_sideband_mode: %d port_cnt:%d\n",
			mst_sideband_mode, mst_port_cnt);
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

static ssize_t dp_debug_read_edid_modes_mst(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dp_debug_private *debug = file->private_data;
	struct dp_mst_connector *mst_connector;
	char *buf;
	u32 len = 0, ret = 0, max_size = SZ_4K;
	int rc = 0;
	struct drm_connector *connector;
	struct drm_display_mode *mode;
	bool in_list = false;

	if (!debug) {
		DP_ERR("invalid data\n");
		rc = -ENODEV;
		goto error;
	}

	mutex_lock(&debug->dp_debug.dp_mst_connector_list.lock);
	list_for_each_entry(mst_connector,
			&debug->dp_debug.dp_mst_connector_list.list, list) {
		if (mst_connector->con_id == debug->mst_con_id) {
			connector = mst_connector->conn;
			in_list = true;
		}
	}
	mutex_unlock(&debug->dp_debug.dp_mst_connector_list.lock);

	if (!in_list) {
		DP_ERR("connector %u not in mst list\n", debug->mst_con_id);
		rc = -EINVAL;
		goto error;
	}

	if (!connector) {
		DP_ERR("connector is NULL\n");
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

	mutex_lock(&connector->dev->mode_config.mutex);
	list_for_each_entry(mode, &connector->modes, head) {
		ret = snprintf(buf + len, max_size,
				"%s %d %d %d %d %d 0x%x\n",
				mode->name, mode->vrefresh,
				mode->picture_aspect_ratio, mode->htotal,
				mode->vtotal, mode->clock, mode->flags);
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
	struct dp_mst_connector *mst_connector;
	char *buf;
	u32 len = 0, ret = 0, max_size = SZ_4K;
	int rc = 0;
	struct drm_connector *connector;

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

	mutex_lock(&debug->dp_debug.dp_mst_connector_list.lock);
	list_for_each_entry(mst_connector,
			&debug->dp_debug.dp_mst_connector_list.list, list) {
		/* Do not print info for head node */
		if (mst_connector->con_id == -1)
			continue;

		connector = mst_connector->conn;

		if (!connector) {
			DP_ERR("connector for id %d is NULL\n",
					mst_connector->con_id);
			continue;
		}

		ret = scnprintf(buf + len, max_size,
				"conn name:%s, conn id:%d state:%d\n",
				connector->name, connector->base.id,
				connector->status);
		if (dp_debug_check_buffer_overflow(ret, &max_size, &len))
			break;
	}
	mutex_unlock(&debug->dp_debug.dp_mst_connector_list.lock);

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
	struct dp_mst_connector *mst_connector;
	struct drm_connector *connector;
	bool in_list = false;

	if (!debug) {
		DP_ERR("invalid data\n");
		return -ENODEV;
	}

	mutex_lock(&debug->dp_debug.dp_mst_connector_list.lock);
	list_for_each_entry(mst_connector,
			&debug->dp_debug.dp_mst_connector_list.list, list) {
		if (mst_connector->con_id == debug->mst_con_id) {
			connector = mst_connector->conn;
			in_list = true;
		}
	}
	mutex_unlock(&debug->dp_debug.dp_mst_connector_list.lock);

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
	if (sim) {
		if (dp_debug_get_edid_buf(debug))
			return;

		if (dp_debug_get_dpcd_buf(debug)) {
			devm_kfree(debug->dev, debug->edid);
			debug->edid = NULL;
			return;
		}

		debug->dp_debug.sim_mode = true;
		debug->aux->set_sim_mode(debug->aux, true,
			debug->edid, debug->dpcd);
		debug->ctrl->set_sim_mode(debug->ctrl, true);
	} else {
		if (debug->hotplug) {
			DP_WARN("sim mode off before hotplug disconnect\n");
			debug->hpd->simulate_connect(debug->hpd, false);
			debug->hotplug = false;
		}
		debug->aux->abort(debug->aux, true);
		debug->ctrl->abort(debug->ctrl, true);

		debug->aux->set_sim_mode(debug->aux, false, NULL, NULL);
		debug->ctrl->set_sim_mode(debug->ctrl, false);
		debug->dp_debug.sim_mode = false;

		debug->panel->set_edid(debug->panel, 0);
		if (debug->edid) {
			devm_kfree(debug->dev, debug->edid);
			debug->edid = NULL;
		}

		debug->panel->set_dpcd(debug->panel, 0);
		if (debug->dpcd) {
			devm_kfree(debug->dev, debug->dpcd);
			debug->dpcd = NULL;
		}
	}

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

	file = debugfs_create_u32("max_lclk_khz", 0644, dir,
			&debug->parser->max_lclk_khz);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs max_lclk_khz failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_u32("lane_count", 0644, dir,
			&debug->panel->lane_count);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs lane_count failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}

	file = debugfs_create_u32("link_bw_code", 0644, dir,
			&debug->panel->link_bw_code);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs link_bw_code failed, rc=%d\n",
		       DEBUG_NAME, rc);
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

	file = debugfs_create_u32("disconnect_delay_ms", 0644, dir,
		&debug->dp_debug.disconnect_delay_ms);
	if (IS_ERR_OR_NULL(file)) {
		rc = PTR_ERR(file);
		DP_ERR("[%s] debugfs disconnect_delay_ms failed, rc=%d\n",
		       DEBUG_NAME, rc);
		return rc;
	}
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

u8 *dp_debug_get_edid(struct dp_debug *dp_debug)
{
	struct dp_debug_private *debug;

	if (!dp_debug)
		return NULL;

	debug = container_of(dp_debug, struct dp_debug_private, dp_debug);

	return debug->edid;
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

	debug->dp_debug.debug_en = false;
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

	dp_debug = &debug->dp_debug;
	dp_debug->vdisplay = 0;
	dp_debug->hdisplay = 0;
	dp_debug->vrefresh = 0;

	mutex_init(&debug->lock);

	rc = dp_debug_init(dp_debug);
	if (rc) {
		devm_kfree(in->dev, debug);
		goto error;
	}

	debug->aux->access_lock = &debug->lock;
	dp_debug->get_edid = dp_debug_get_edid;
	dp_debug->abort = dp_debug_abort;
	dp_debug->set_mst_con = dp_debug_set_mst_con;

	INIT_LIST_HEAD(&dp_debug->dp_mst_connector_list.list);

	/*
	 * Do not associate the head of the list with any connector in order to
	 * maintain backwards compatibility with the SST use case.
	 */
	dp_debug->dp_mst_connector_list.con_id = -1;
	dp_debug->dp_mst_connector_list.conn = NULL;
	dp_debug->dp_mst_connector_list.debug_en = false;
	mutex_init(&dp_debug->dp_mst_connector_list.lock);

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

	return 0;
}

void dp_debug_put(struct dp_debug *dp_debug)
{
	struct dp_debug_private *debug;

	if (!dp_debug)
		return;

	debug = container_of(dp_debug, struct dp_debug_private, dp_debug);

	dp_debug_deinit(dp_debug);

	mutex_destroy(&dp_debug->dp_mst_connector_list.lock);
	mutex_destroy(&debug->lock);

	if (debug->edid)
		devm_kfree(debug->dev, debug->edid);

	if (debug->dpcd)
		devm_kfree(debug->dev, debug->dpcd);

	devm_kfree(debug->dev, debug);
}
