/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"msm-dsi-display:[%s] " fmt, __func__

#include <linux/list.h>
#include <linux/of.h>
#include <linux/err.h>

#include "msm_drv.h"
#include "sde_connector.h"
#include "msm_mmu.h"
#include "dsi_display.h"
#include "dsi_panel.h"
#include "dsi_ctrl.h"
#include "dsi_ctrl_hw.h"
#include "dsi_drm.h"
#include "dsi_clk.h"
#include "dsi_pwr.h"
#include "sde_dbg.h"

#define to_dsi_display(x) container_of(x, struct dsi_display, host)
#define INT_BASE_10 10
#define NO_OVERRIDE -1

#define MISR_BUFF_SIZE	256

#define MAX_NAME_SIZE	64

static DEFINE_MUTEX(dsi_display_list_lock);
static LIST_HEAD(dsi_display_list);
static char dsi_display_primary[MAX_CMDLINE_PARAM_LEN];
static char dsi_display_secondary[MAX_CMDLINE_PARAM_LEN];
static struct dsi_display_boot_param boot_displays[MAX_DSI_ACTIVE_DISPLAY];
static struct device_node *default_active_node;
static const struct of_device_id dsi_display_dt_match[] = {
	{.compatible = "qcom,dsi-display"},
	{}
};

static struct dsi_display *main_display;

void dsi_rect_intersect(const struct dsi_rect *r1,
		const struct dsi_rect *r2,
		struct dsi_rect *result)
{
	int l, t, r, b;

	if (!r1 || !r2 || !result)
		return;

	l = max(r1->x, r2->x);
	t = max(r1->y, r2->y);
	r = min((r1->x + r1->w), (r2->x + r2->w));
	b = min((r1->y + r1->h), (r2->y + r2->h));

	if (r <= l || b <= t) {
		memset(result, 0, sizeof(*result));
	} else {
		result->x = l;
		result->y = t;
		result->w = r - l;
		result->h = b - t;
	}
}

int dsi_display_set_backlight(void *display, u32 bl_lvl)
{
	struct dsi_display *dsi_display = display;
	struct dsi_panel *panel;
	u32 bl_scale, bl_scale_ad;
	u64 bl_temp;
	int rc = 0;

	if (dsi_display == NULL || dsi_display->panel == NULL)
		return -EINVAL;

	panel = dsi_display->panel;

	if (!dsi_panel_initialized(panel))
		return -EINVAL;

	panel->bl_config.bl_level = bl_lvl;

	/* scale backlight */
	bl_scale = panel->bl_config.bl_scale;
	bl_temp = bl_lvl * bl_scale / MAX_BL_SCALE_LEVEL;

	bl_scale_ad = panel->bl_config.bl_scale_ad;
	bl_temp = (u32)bl_temp * bl_scale_ad / MAX_AD_BL_SCALE_LEVEL;

	pr_debug("bl_scale = %u, bl_scale_ad = %u, bl_lvl = %u\n",
		bl_scale, bl_scale_ad, (u32)bl_temp);

	rc = dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI core clocks, rc=%d\n",
		       dsi_display->name, rc);
		goto error;
	}

	rc = dsi_panel_set_backlight(panel, (u32)bl_temp);
	if (rc)
		pr_err("unable to set backlight\n");

	rc = dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
	if (rc) {
		pr_err("[%s] failed to disable DSI core clocks, rc=%d\n",
		       dsi_display->name, rc);
		goto error;
	}

error:
	return rc;
}

static int dsi_display_cmd_engine_enable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	mutex_lock(&m_ctrl->ctrl->ctrl_lock);

	if (display->cmd_engine_refcount > 0) {
		display->cmd_engine_refcount++;
		goto done;
	}

	rc = dsi_ctrl_set_cmd_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_ON);
	if (rc) {
		pr_err("[%s] failed to enable cmd engine, rc=%d\n",
		       display->name, rc);
		goto done;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_cmd_engine_state(ctrl->ctrl,
						   DSI_CTRL_ENGINE_ON);
		if (rc) {
			pr_err("[%s] failed to enable cmd engine, rc=%d\n",
			       display->name, rc);
			goto error_disable_master;
		}
	}

	display->cmd_engine_refcount++;
	goto done;
error_disable_master:
	(void)dsi_ctrl_set_cmd_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_OFF);
done:
	mutex_unlock(&m_ctrl->ctrl->ctrl_lock);
	return rc;
}

static int dsi_display_cmd_engine_disable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	mutex_lock(&m_ctrl->ctrl->ctrl_lock);

	if (display->cmd_engine_refcount == 0) {
		pr_err("[%s] Invalid refcount\n", display->name);
		goto done;
	} else if (display->cmd_engine_refcount > 1) {
		display->cmd_engine_refcount--;
		goto done;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_cmd_engine_state(ctrl->ctrl,
						   DSI_CTRL_ENGINE_OFF);
		if (rc)
			pr_err("[%s] failed to enable cmd engine, rc=%d\n",
			       display->name, rc);
	}

	rc = dsi_ctrl_set_cmd_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_OFF);
	if (rc) {
		pr_err("[%s] failed to enable cmd engine, rc=%d\n",
		       display->name, rc);
		goto error;
	}

error:
	display->cmd_engine_refcount = 0;
done:
	mutex_unlock(&m_ctrl->ctrl->ctrl_lock);
	return rc;
}

static bool dsi_display_validate_reg_read(struct dsi_panel *panel)
{
	int i, j = 0;
	int len = 0, *lenp;
	int group = 0, count = 0;
	struct dsi_display_mode *mode;
	struct drm_panel_esd_config *config;

	if (!panel)
		return false;

	config = &(panel->esd_config);

	lenp = config->status_valid_params ?: config->status_cmds_rlen;
	mode = panel->cur_mode;
	count = mode->priv_info->cmd_sets[DSI_CMD_SET_PANEL_STATUS].count;

	for (i = 0; i < count; i++)
		len += lenp[i];

	for (i = 0; i < len; i++)
		j += len;

	for (j = 0; j < config->groups; ++j) {
		for (i = 0; i < len; ++i) {
			if (config->return_buf[i] !=
				config->status_value[group + i])
				break;
		}

		if (i == len)
			return true;
		group += len;
	}

	return false;
}

static int dsi_display_read_status(struct dsi_display_ctrl *ctrl,
		struct dsi_panel *panel)
{
	int i, rc = 0, count = 0, start = 0, *lenp;
	struct drm_panel_esd_config *config;
	struct dsi_cmd_desc *cmds;
	u32 flags = 0;

	if (!panel)
		return -EINVAL;

	/* acquire panel_lock to make sure no commands are in progress */
	dsi_panel_acquire_panel_lock(panel);

	config = &(panel->esd_config);
	lenp = config->status_valid_params ?: config->status_cmds_rlen;
	count = config->status_cmd.count;
	cmds = config->status_cmd.cmds;
	if (cmds->last_command) {
		cmds->msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;
		flags |= DSI_CTRL_CMD_LAST_COMMAND;
	}
	flags |= (DSI_CTRL_CMD_FETCH_MEMORY | DSI_CTRL_CMD_READ);

	for (i = 0; i < count; ++i) {
		memset(config->status_buf, 0x0, SZ_4K);
		cmds[i].msg.rx_buf = config->status_buf;
		cmds[i].msg.rx_len = config->status_cmds_rlen[i];
		rc = dsi_ctrl_cmd_transfer(ctrl->ctrl, &cmds[i].msg, flags);
		if (rc <= 0) {
			pr_err("rx cmd transfer failed rc=%d\n", rc);
			goto error;
		}

		memcpy(config->return_buf + start,
			config->status_buf, lenp[i]);
		start += lenp[i];
	}

error:
	/* release panel_lock */
	dsi_panel_release_panel_lock(panel);
	return rc;
}

static int dsi_display_validate_status(struct dsi_display_ctrl *ctrl,
		struct dsi_panel *panel)
{
	int rc = 0;

	rc = dsi_display_read_status(ctrl, panel);
	if (rc <= 0) {
		goto exit;
	} else {
		/*
		 * panel status read successfully.
		 * check for validity of the data read back.
		 */
		rc = dsi_display_validate_reg_read(panel);
		if (!rc) {
			rc = -EINVAL;
			goto exit;
		}
	}

exit:
	return rc;
}

static int dsi_display_status_reg_read(struct dsi_display *display)
{
	int rc = 0, i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	pr_debug(" ++\n");

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_display_cmd_engine_enable(display);
	if (rc) {
		pr_err("cmd engine enable failed\n");
		return -EPERM;
	}

	rc = dsi_display_validate_status(m_ctrl, display->panel);
	if (rc <= 0) {
		pr_err("[%s] read status failed on master,rc=%d\n",
		       display->name, rc);
		goto exit;
	}

	if (!display->panel->sync_broadcast_en)
		goto exit;

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (ctrl == m_ctrl)
			continue;

		rc = dsi_display_validate_status(ctrl, display->panel);
		if (rc <= 0) {
			pr_err("[%s] read status failed on master,rc=%d\n",
			       display->name, rc);
			goto exit;
		}
	}

exit:
	dsi_display_cmd_engine_disable(display);
	return rc;
}

static int dsi_display_status_bta_request(struct dsi_display *display)
{
	int rc = 0;

	pr_debug(" ++\n");
	/* TODO: trigger SW BTA and wait for acknowledgment */

	return rc;
}

static int dsi_display_status_check_te(struct dsi_display *display)
{
	int rc = 0;

	pr_debug(" ++\n");
	/* TODO: wait for TE interrupt from panel */

	return rc;
}

int dsi_display_check_status(void *display)
{
	struct dsi_display *dsi_display = display;
	struct dsi_panel *panel;
	u32 status_mode;
	int rc = 0x1;

	if (dsi_display == NULL)
		return -EINVAL;

	panel = dsi_display->panel;

	status_mode = panel->esd_config.status_mode;

	mutex_lock(&dsi_display->display_lock);

	if (!panel->panel_initialized) {
		pr_debug("Panel not initialized\n");
		mutex_unlock(&dsi_display->display_lock);
		return rc;
	}

	dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
		DSI_ALL_CLKS, DSI_CLK_ON);

	if (status_mode == ESD_MODE_REG_READ) {
		rc = dsi_display_status_reg_read(dsi_display);
	} else if (status_mode == ESD_MODE_SW_BTA) {
		rc = dsi_display_status_bta_request(dsi_display);
	} else if (status_mode == ESD_MODE_PANEL_TE) {
		rc = dsi_display_status_check_te(dsi_display);
	} else {
		pr_warn("unsupported check status mode\n");
		panel->esd_config.esd_enabled = false;
	}

	dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
		DSI_ALL_CLKS, DSI_CLK_OFF);
	mutex_unlock(&dsi_display->display_lock);

	return rc;
}

int dsi_display_soft_reset(void *display)
{
	struct dsi_display *dsi_display;
	struct dsi_display_ctrl *ctrl;
	int rc = 0;
	int i;

	if (!display)
		return -EINVAL;

	dsi_display = display;

	for (i = 0 ; i < dsi_display->ctrl_count; i++) {
		ctrl = &dsi_display->ctrl[i];
		rc = dsi_ctrl_soft_reset(ctrl->ctrl);
		if (rc) {
			pr_err("[%s] failed to soft reset host_%d, rc=%d\n",
					dsi_display->name, i, rc);
			break;
		}
	}

	return rc;
}

enum dsi_pixel_format dsi_display_get_dst_format(void *display)
{
	enum dsi_pixel_format format = DSI_PIXEL_FORMAT_MAX;
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display || !dsi_display->panel) {
		pr_err("Invalid params(s) dsi_display %pK, panel %pK\n",
			dsi_display,
			((dsi_display) ? dsi_display->panel : NULL));
		return format;
	}

	format = dsi_display->panel->host_config.dst_format;
	return format;
}

static void _dsi_display_setup_misr(struct dsi_display *display)
{
	int i;

	for (i = 0; i < display->ctrl_count; i++) {
		dsi_ctrl_setup_misr(display->ctrl[i].ctrl,
				display->misr_enable,
				display->misr_frame_count);
	}
}

/**
 * dsi_display_get_cont_splash_status - Get continuous splash status.
 * @dsi_display:         DSI display handle.
 *
 * Return: boolean to signify whether continuous splash is enabled.
 */
static bool dsi_display_get_cont_splash_status(struct dsi_display *display)
{
	u32 val = 0;
	int i;
	struct dsi_display_ctrl *ctrl;
	struct dsi_ctrl_hw *hw;

	for (i = 0; i < display->ctrl_count ; i++) {
		ctrl = &(display->ctrl[i]);
		if (!ctrl || !ctrl->ctrl)
			continue;

		hw = &(ctrl->ctrl->hw);
		val = hw->ops.get_cont_splash_status(hw);
		if (!val)
			return false;
	}
	return true;
}

int dsi_display_set_power(struct drm_connector *connector,
		int power_mode, void *disp)
{
	struct dsi_display *display = disp;
	int rc = 0;

	if (!display || !display->panel) {
		pr_err("invalid display/panel\n");
		return -EINVAL;
	}

	switch (power_mode) {
	case SDE_MODE_DPMS_LP1:
		rc = dsi_panel_set_lp1(display->panel);
		break;
	case SDE_MODE_DPMS_LP2:
		rc = dsi_panel_set_lp2(display->panel);
		break;
	default:
		rc = dsi_panel_set_nolp(display->panel);
		break;
	}
	return rc;
}

static ssize_t debugfs_dump_info_read(struct file *file,
				      char __user *user_buf,
				      size_t user_len,
				      loff_t *ppos)
{
	struct dsi_display *display = file->private_data;
	char *buf;
	u32 len = 0;
	int i;

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += snprintf(buf + len, (SZ_4K - len), "name = %s\n", display->name);
	len += snprintf(buf + len, (SZ_4K - len),
			"\tResolution = %dx%d\n",
			display->config.video_timing.h_active,
			display->config.video_timing.v_active);

	for (i = 0; i < display->ctrl_count; i++) {
		len += snprintf(buf + len, (SZ_4K - len),
				"\tCTRL_%d:\n\t\tctrl = %s\n\t\tphy = %s\n",
				i, display->ctrl[i].ctrl->name,
				display->ctrl[i].phy->name);
	}

	len += snprintf(buf + len, (SZ_4K - len),
			"\tPanel = %s\n", display->panel->name);

	len += snprintf(buf + len, (SZ_4K - len),
			"\tClock master = %s\n",
			display->ctrl[display->clk_master_idx].ctrl->name);

	if (copy_to_user(user_buf, buf, len)) {
		kfree(buf);
		return -EFAULT;
	}

	*ppos += len;

	kfree(buf);
	return len;
}

static ssize_t debugfs_misr_setup(struct file *file,
				  const char __user *user_buf,
				  size_t user_len,
				  loff_t *ppos)
{
	struct dsi_display *display = file->private_data;
	char *buf;
	int rc = 0;
	size_t len;
	u32 enable, frame_count;

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(MISR_BUFF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	/* leave room for termination char */
	len = min_t(size_t, user_len, MISR_BUFF_SIZE - 1);
	if (copy_from_user(buf, user_buf, len)) {
		rc = -EINVAL;
		goto error;
	}

	buf[len] = '\0'; /* terminate the string */

	if (sscanf(buf, "%u %u", &enable, &frame_count) != 2) {
		rc = -EINVAL;
		goto error;
	}

	display->misr_enable = enable;
	display->misr_frame_count = frame_count;

	mutex_lock(&display->display_lock);
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto unlock;
	}

	_dsi_display_setup_misr(display);

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
	if (rc) {
		pr_err("[%s] failed to disable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto unlock;
	}

	rc = user_len;
unlock:
	mutex_unlock(&display->display_lock);
error:
	kfree(buf);
	return rc;
}

static ssize_t debugfs_misr_read(struct file *file,
				 char __user *user_buf,
				 size_t user_len,
				 loff_t *ppos)
{
	struct dsi_display *display = file->private_data;
	char *buf;
	u32 len = 0;
	int rc = 0;
	struct dsi_ctrl *dsi_ctrl;
	int i;
	u32 misr;
	size_t max_len = min_t(size_t, user_len, MISR_BUFF_SIZE);

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(max_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	mutex_lock(&display->display_lock);
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		dsi_ctrl = display->ctrl[i].ctrl;
		misr = dsi_ctrl_collect_misr(display->ctrl[i].ctrl);

		len += snprintf((buf + len), max_len - len,
			"DSI_%d MISR: 0x%x\n", dsi_ctrl->cell_index, misr);

		if (len >= max_len)
			break;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
	if (rc) {
		pr_err("[%s] failed to disable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	if (copy_to_user(user_buf, buf, len)) {
		rc = -EFAULT;
		goto error;
	}

	*ppos += len;

error:
	mutex_unlock(&display->display_lock);
	kfree(buf);
	return len;
}

static const struct file_operations dump_info_fops = {
	.open = simple_open,
	.read = debugfs_dump_info_read,
};

static const struct file_operations misr_data_fops = {
	.open = simple_open,
	.read = debugfs_misr_read,
	.write = debugfs_misr_setup,
};

static int dsi_display_debugfs_init(struct dsi_display *display)
{
	int rc = 0;
	struct dentry *dir, *dump_file, *misr_data;
	char name[MAX_NAME_SIZE];
	int i;

	dir = debugfs_create_dir(display->name, NULL);
	if (IS_ERR_OR_NULL(dir)) {
		rc = PTR_ERR(dir);
		pr_err("[%s] debugfs create dir failed, rc = %d\n",
		       display->name, rc);
		goto error;
	}

	dump_file = debugfs_create_file("dump_info",
					0400,
					dir,
					display,
					&dump_info_fops);
	if (IS_ERR_OR_NULL(dump_file)) {
		rc = PTR_ERR(dump_file);
		pr_err("[%s] debugfs create dump info file failed, rc=%d\n",
		       display->name, rc);
		goto error_remove_dir;
	}

	misr_data = debugfs_create_file("misr_data",
					0600,
					dir,
					display,
					&misr_data_fops);
	if (IS_ERR_OR_NULL(misr_data)) {
		rc = PTR_ERR(misr_data);
		pr_err("[%s] debugfs create misr datafile failed, rc=%d\n",
		       display->name, rc);
		goto error_remove_dir;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		struct msm_dsi_phy *phy = display->ctrl[i].phy;

		if (!phy || !phy->name)
			continue;

		snprintf(name, ARRAY_SIZE(name),
				"%s_allow_phy_power_off", phy->name);
		dump_file = debugfs_create_bool(name, 0600, dir,
				&phy->allow_phy_power_off);
		if (IS_ERR_OR_NULL(dump_file)) {
			rc = PTR_ERR(dump_file);
			pr_err("[%s] debugfs create %s failed, rc=%d\n",
			       display->name, name, rc);
			goto error_remove_dir;
		}

		snprintf(name, ARRAY_SIZE(name),
				"%s_regulator_min_datarate_bps", phy->name);
		dump_file = debugfs_create_u32(name, 0600, dir,
				&phy->regulator_min_datarate_bps);
		if (IS_ERR_OR_NULL(dump_file)) {
			rc = PTR_ERR(dump_file);
			pr_err("[%s] debugfs create %s failed, rc=%d\n",
			       display->name, name, rc);
			goto error_remove_dir;
		}
	}

	if (!debugfs_create_bool("ulps_enable", 0600, dir,
			&display->panel->ulps_enabled)) {
		pr_err("[%s] debugfs create ulps enable file failed\n",
		       display->name);
		goto error_remove_dir;
	}

	if (!debugfs_create_bool("ulps_suspend_enable", 0600, dir,
			&display->panel->ulps_suspend_enabled)) {
		pr_err("[%s] debugfs create ulps-suspend enable file failed\n",
		       display->name);
		goto error_remove_dir;
	}

	display->root = dir;
	return rc;
error_remove_dir:
	debugfs_remove(dir);
error:
	return rc;
}

static int dsi_display_debugfs_deinit(struct dsi_display *display)
{
	debugfs_remove_recursive(display->root);

	return 0;
}

static void adjust_timing_by_ctrl_count(const struct dsi_display *display,
					struct dsi_display_mode *mode)
{
	if (display->ctrl_count > 1) {
		mode->timing.h_active /= display->ctrl_count;
		mode->timing.h_front_porch /= display->ctrl_count;
		mode->timing.h_sync_width /= display->ctrl_count;
		mode->timing.h_back_porch /= display->ctrl_count;
		mode->timing.h_skew /= display->ctrl_count;
		mode->pixel_clk_khz /= display->ctrl_count;
	}
}

static int dsi_display_is_ulps_req_valid(struct dsi_display *display,
		bool enable)
{
	/* TODO: make checks based on cont. splash */
	int splash_enabled = false;

	pr_debug("checking ulps req validity\n");

	if (!dsi_panel_ulps_feature_enabled(display->panel) &&
			!display->panel->ulps_suspend_enabled) {
		pr_debug("%s: ULPS feature is not enabled\n", __func__);
		return false;
	}

	if (!dsi_panel_initialized(display->panel) &&
			!display->panel->ulps_suspend_enabled) {
		pr_debug("%s: panel not yet initialized\n", __func__);
		return false;
	}

	if (enable && display->ulps_enabled) {
		pr_debug("ULPS already enabled\n");
		return false;
	} else if (!enable && !display->ulps_enabled) {
		pr_debug("ULPS already disabled\n");
		return false;
	}

	/*
	 * No need to enter ULPS when transitioning from splash screen to
	 * boot animation since it is expected that the clocks would be turned
	 * right back on.
	 */
	if (enable && splash_enabled)
		return false;

	return true;
}


/**
 * dsi_display_set_ulps() - set ULPS state for DSI lanes.
 * @dsi_display:         DSI display handle.
 * @enable:           enable/disable ULPS.
 *
 * ULPS can be enabled/disabled after DSI host engine is turned on.
 *
 * Return: error code.
 */
static int dsi_display_set_ulps(struct dsi_display *display, bool enable)
{
	int rc = 0;
	int i = 0;
	struct dsi_display_ctrl *m_ctrl, *ctrl;


	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (!dsi_display_is_ulps_req_valid(display, enable)) {
		pr_debug("%s: skipping ULPS config, enable=%d\n",
			__func__, enable);
		return 0;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_ctrl_set_ulps(m_ctrl->ctrl, enable);
	if (rc) {
		pr_err("Ulps controller state change(%d) failed\n", enable);
		return rc;
	}

	rc = dsi_phy_set_ulps(m_ctrl->phy, &display->config, enable,
				display->clamp_enabled);
	if (rc) {
		pr_err("Ulps PHY state change(%d) failed\n", enable);
		return rc;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_ulps(ctrl->ctrl, enable);
		if (rc) {
			pr_err("Ulps controller state change(%d) failed\n",
				enable);
			return rc;
		}

		rc = dsi_phy_set_ulps(ctrl->phy, &display->config, enable,
					display->clamp_enabled);
		if (rc) {
			pr_err("Ulps PHY state change(%d) failed\n", enable);
			return rc;
		}

	}
	display->ulps_enabled = enable;
	return 0;
}

/**
 * dsi_display_set_clamp() - set clamp state for DSI IO.
 * @dsi_display:         DSI display handle.
 * @enable:           enable/disable clamping.
 *
 * Return: error code.
 */
static int dsi_display_set_clamp(struct dsi_display *display, bool enable)
{
	int rc = 0;
	int i = 0;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	bool ulps_enabled = false;


	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	ulps_enabled = display->ulps_enabled;

	rc = dsi_ctrl_set_clamp_state(m_ctrl->ctrl, enable, ulps_enabled);
	if (rc) {
		pr_err("DSI Clamp state change(%d) failed\n", enable);
		return rc;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_clamp_state(ctrl->ctrl, enable, ulps_enabled);
		if (rc) {
			pr_err("DSI Clamp state change(%d) failed\n", enable);
			return rc;
		}
	}
	display->clamp_enabled = enable;
	return 0;
}

/**
 * dsi_display_setup_ctrl() - setup DSI controller.
 * @dsi_display:         DSI display handle.
 *
 * Return: error code.
 */
static int dsi_display_ctrl_setup(struct dsi_display *display)
{
	int rc = 0;
	int i = 0;
	struct dsi_display_ctrl *ctrl, *m_ctrl;


	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	rc = dsi_ctrl_setup(m_ctrl->ctrl);
	if (rc) {
		pr_err("DSI controller setup failed\n");
		return rc;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_setup(ctrl->ctrl);
		if (rc) {
			pr_err("DSI controller setup failed\n");
			return rc;
		}
	}
	return 0;
}

static int dsi_display_phy_enable(struct dsi_display *display);

/**
 * dsi_display_phy_idle_on() - enable DSI PHY while coming out of idle screen.
 * @dsi_display:         DSI display handle.
 * @mmss_clamp:          True if clamp is enabled.
 *
 * Return: error code.
 */
static int dsi_display_phy_idle_on(struct dsi_display *display,
		bool mmss_clamp)
{
	int rc = 0;
	int i = 0;
	struct dsi_display_ctrl *m_ctrl, *ctrl;


	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (mmss_clamp && !display->phy_idle_power_off) {
		dsi_display_phy_enable(display);
		return 0;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	rc = dsi_phy_idle_ctrl(m_ctrl->phy, true);
	if (rc) {
		pr_err("DSI controller setup failed\n");
		return rc;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_phy_idle_ctrl(ctrl->phy, true);
		if (rc) {
			pr_err("DSI controller setup failed\n");
			return rc;
		}
	}
	display->phy_idle_power_off = false;
	return 0;
}

/**
 * dsi_display_phy_idle_off() - disable DSI PHY while going to idle screen.
 * @dsi_display:         DSI display handle.
 *
 * Return: error code.
 */
static int dsi_display_phy_idle_off(struct dsi_display *display)
{
	int rc = 0;
	int i = 0;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		struct msm_dsi_phy *phy = display->ctrl[i].phy;

		if (!phy)
			continue;

		if (!phy->allow_phy_power_off) {
			pr_debug("phy doesn't support this feature\n");
			return 0;
		}
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_phy_idle_ctrl(m_ctrl->phy, false);
	if (rc) {
		pr_err("[%s] failed to enable cmd engine, rc=%d\n",
		       display->name, rc);
		return rc;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_phy_idle_ctrl(ctrl->phy, false);
		if (rc) {
			pr_err("DSI controller setup failed\n");
			return rc;
		}
	}
	display->phy_idle_power_off = true;
	return 0;
}

void dsi_display_enable_event(struct dsi_display *display,
		uint32_t event_idx, struct dsi_event_cb_info *event_info,
		bool enable)
{
	uint32_t irq_status_idx = DSI_STATUS_INTERRUPT_COUNT;
	int i;

	if (!display) {
		pr_err("invalid display\n");
		return;
	}

	if (event_info)
		event_info->event_idx = event_idx;

	switch (event_idx) {
	case SDE_CONN_EVENT_VID_DONE:
		irq_status_idx = DSI_SINT_VIDEO_MODE_FRAME_DONE;
		break;
	case SDE_CONN_EVENT_CMD_DONE:
		irq_status_idx = DSI_SINT_CMD_FRAME_DONE;
		break;
	case SDE_CONN_EVENT_VID_FIFO_OVERFLOW:
	case SDE_CONN_EVENT_CMD_FIFO_UNDERFLOW:
		if (event_info) {
			for (i = 0; i < display->ctrl_count; i++)
				display->ctrl[i].ctrl->recovery_cb =
							*event_info;
		}
	default:
		/* nothing to do */
		pr_debug("[%s] unhandled event %d\n", display->name, event_idx);
		return;
	}

	if (enable) {
		for (i = 0; i < display->ctrl_count; i++)
			dsi_ctrl_enable_status_interrupt(
					display->ctrl[i].ctrl, irq_status_idx,
					event_info);
	} else {
		for (i = 0; i < display->ctrl_count; i++)
			dsi_ctrl_disable_status_interrupt(
					display->ctrl[i].ctrl, irq_status_idx);
	}
}

/**
 * dsi_config_host_engine_state_for_cont_splash()- update host engine state
 *                                                 during continuous splash.
 * @display: Handle to dsi display
 *
 */
static void dsi_config_host_engine_state_for_cont_splash
					(struct dsi_display *display)
{
	int i;
	struct dsi_display_ctrl *ctrl;
	enum dsi_engine_state host_state = DSI_CTRL_ENGINE_ON;

	/* Sequence does not matter for split dsi usecases */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;

		dsi_ctrl_update_host_engine_state_for_cont_splash(ctrl->ctrl,
							host_state);
	}
}

static int dsi_display_ctrl_power_on(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	/* Sequence does not matter for split dsi usecases */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;

		rc = dsi_ctrl_set_power_state(ctrl->ctrl,
					      DSI_CTRL_POWER_VREG_ON);
		if (rc) {
			pr_err("[%s] Failed to set power state, rc=%d\n",
			       ctrl->ctrl->name, rc);
			goto error;
		}
	}

	return rc;
error:
	for (i = i - 1; i >= 0; i--) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;
		(void)dsi_ctrl_set_power_state(ctrl->ctrl,
			DSI_CTRL_POWER_VREG_OFF);
	}
	return rc;
}

static int dsi_display_ctrl_power_off(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	/* Sequence does not matter for split dsi usecases */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;

		rc = dsi_ctrl_set_power_state(ctrl->ctrl,
			DSI_CTRL_POWER_VREG_OFF);
		if (rc) {
			pr_err("[%s] Failed to power off, rc=%d\n",
			       ctrl->ctrl->name, rc);
			goto error;
		}
	}
error:
	return rc;
}

static void dsi_display_parse_cmdline_topology(struct dsi_display *display,
					unsigned int display_type)
{
	char *boot_str = NULL;
	char *str = NULL;
	unsigned long value;

	if (display_type >= MAX_DSI_ACTIVE_DISPLAY) {
		pr_err("display_type=%d not supported\n", display_type);
		return;
	}

	if (display_type == DSI_PRIMARY)
		boot_str = dsi_display_primary;
	else
		boot_str = dsi_display_secondary;

	str = strnstr(boot_str, ":config", strlen(boot_str));
	if (!str)
		return;

	if (kstrtol(str + strlen(":config"), INT_BASE_10,
				(unsigned long *)&value)) {
		pr_err("invalid config index override: %s\n", boot_str);
		return;
	}
	display->cmdline_topology = value;

	str = strnstr(boot_str, ":timing", strlen(boot_str));
	if (!str)
		return;

	if (kstrtol(str + strlen(":timing"), INT_BASE_10,
				(unsigned long *)&value)) {
		pr_err("invalid timing index override: %s. resetting both timing and config\n",
			boot_str);
		display->cmdline_topology = NO_OVERRIDE;
		return;
	}
	display->cmdline_timing = value;
}

/**
 * dsi_display_name_compare()- compare whether DSI display name matches.
 * @node:	Pointer to device node structure
 * @display_name: Name of display to validate
 *
 * Return:	returns a bool specifying whether given display is active
 */
static bool dsi_display_name_compare(struct device_node *node,
			const char *display_name, int index)
{
	if (index >= MAX_DSI_ACTIVE_DISPLAY) {
		pr_err("Invalid Index\n");
		return false;
	}

	if (boot_displays[index].boot_disp_en) {
		if (!(strcmp(&boot_displays[index].name[0], display_name))) {
			boot_displays[index].node = node;
			return true;
		}
	}
	return false;
}

/**
 * dsi_display_parse_boot_display_selection()- Parse DSI boot display name
 *
 * Return:	returns error status
 */
static int dsi_display_parse_boot_display_selection(void)
{
	char *pos = NULL;
	char disp_buf[MAX_CMDLINE_PARAM_LEN] = {'\0'};
	int i, j, num_displays;

	if (strlen(dsi_display_primary) == 0)
		return -EINVAL;

	if ((strlen(dsi_display_secondary) > 0))
		num_displays = MAX_DSI_ACTIVE_DISPLAY;
	else {
		/*
		 * Initialize secondary dsi variables
		 * for the senario where dsi_display1
		 * is null but dsi_display0 is valid
		 */

		/* Max number of displays will be one->only Primary */
		num_displays = 1;
		boot_displays[DSI_SECONDARY].is_primary = false;
		boot_displays[DSI_SECONDARY].name[0] = '\0';
	}

	for (i = 0; i < num_displays; i++) {
		boot_displays[i].is_primary = false;
		if (i == DSI_PRIMARY) {
			strlcpy(disp_buf, &dsi_display_primary[0],
				sizeof(dsi_display_primary));
			pos = strnstr(disp_buf, ":",
				sizeof(dsi_display_primary));
		} else {
			strlcpy(disp_buf, &dsi_display_secondary[0],
				sizeof(dsi_display_secondary));
			pos = strnstr(disp_buf, ":",
				sizeof(dsi_display_secondary));
		}
		/* Use ':' as a delimiter to retrieve the display name */
		if (!pos) {
			pr_debug("display name[%s]is not valid\n", disp_buf);
			continue;
		}

		for (j = 0; (disp_buf + j) < pos; j++)
			boot_displays[i].name[j] = *(disp_buf + j);
		boot_displays[i].name[j] = '\0';

		if (i == DSI_PRIMARY) {
			boot_displays[i].is_primary = true;
			/* Currently, secondary DSI display is not supported */
			boot_displays[i].boot_disp_en = true;
		}
	}
	return 0;
}

/**
 * validate_dsi_display_selection()- validate boot DSI display selection
 *
 * Return:	returns true when both displays have unique configurations
 */
static bool validate_dsi_display_selection(void)
{
	int i, j;
	int rc = 0;
	int phy_count = 0;
	int ctrl_count = 0;
	int index = 0;
	bool ctrl_flags[MAX_DSI_ACTIVE_DISPLAY] = {false, false};
	bool phy_flags[MAX_DSI_ACTIVE_DISPLAY] = {false, false};
	struct device_node *node, *ctrl_node, *phy_node;

	for (i = 0; i < MAX_DSI_ACTIVE_DISPLAY; i++) {
		node = boot_displays[i].node;
		ctrl_count = of_count_phandle_with_args(node, "qcom,dsi-ctrl",
								NULL);

		for (j = 0; j < ctrl_count; j++) {
			ctrl_node = of_parse_phandle(node, "qcom,dsi-ctrl", j);
			rc = of_property_read_u32(ctrl_node, "cell-index",
					&index);
			of_node_put(ctrl_node);
			if (rc) {
				pr_err("cell index not set for ctrl_nodes\n");
				return false;
			}
			if (ctrl_flags[index])
				return false;
			ctrl_flags[index] = true;
		}

		phy_count = of_count_phandle_with_args(node, "qcom,dsi-phy",
								NULL);
		for (j = 0; j < phy_count; j++) {
			phy_node = of_parse_phandle(node, "qcom,dsi-phy", j);
			rc = of_property_read_u32(phy_node, "cell-index",
					&index);
			of_node_put(phy_node);
			if (rc) {
				pr_err("cell index not set phy_nodes\n");
				return false;
			}
			if (phy_flags[index])
				return false;
			phy_flags[index] = true;
		}
	}
	return true;
}

struct device_node *dsi_display_get_boot_display(int index)
{

	pr_err("index = %d\n", index);

	if (boot_displays[index].node)
		return boot_displays[index].node;
	else if ((index == (MAX_DSI_ACTIVE_DISPLAY - 1))
			&& (default_active_node))
		return default_active_node;
	else
		return NULL;
}

static int dsi_display_phy_power_on(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	/* Sequence does not matter for split dsi usecases */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;

		rc = dsi_phy_set_power_state(ctrl->phy, true);
		if (rc) {
			pr_err("[%s] Failed to set power state, rc=%d\n",
			       ctrl->phy->name, rc);
			goto error;
		}
	}

	return rc;
error:
	for (i = i - 1; i >= 0; i--) {
		ctrl = &display->ctrl[i];
		if (!ctrl->phy)
			continue;
		(void)dsi_phy_set_power_state(ctrl->phy, false);
	}
	return rc;
}

static int dsi_display_phy_power_off(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	/* Sequence does not matter for split dsi usecases */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->phy)
			continue;

		rc = dsi_phy_set_power_state(ctrl->phy, false);
		if (rc) {
			pr_err("[%s] Failed to power off, rc=%d\n",
			       ctrl->ctrl->name, rc);
			goto error;
		}
	}
error:
	return rc;
}

static int dsi_display_set_clk_src(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	/*
	 * In case of split DSI usecases, the clock for master controller should
	 * be enabled before the other controller. Master controller in the
	 * clock context refers to the controller that sources the clock.
	 */
	m_ctrl = &display->ctrl[display->clk_master_idx];

	rc = dsi_ctrl_set_clock_source(m_ctrl->ctrl,
		   &display->clock_info.src_clks);
	if (rc) {
		pr_err("[%s] failed to set source clocks for master, rc=%d\n",
			   display->name, rc);
		return rc;
	}

	/* Turn on rest of the controllers */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_clock_source(ctrl->ctrl,
			   &display->clock_info.src_clks);
		if (rc) {
			pr_err("[%s] failed to set source clocks, rc=%d\n",
				   display->name, rc);
			return rc;
		}
	}
	return 0;
}

static int dsi_display_phy_reset_config(struct dsi_display *display,
		bool enable)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_phy_reset_config(ctrl->ctrl, enable);
		if (rc) {
			pr_err("[%s] failed to %s phy reset, rc=%d\n",
			       display->name, enable ? "mask" : "unmask", rc);
			return rc;
		}
	}
	return 0;
}

static int dsi_display_ctrl_update(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_host_timing_update(ctrl->ctrl);
		if (rc) {
			pr_err("[%s] failed to update host_%d, rc=%d\n",
				   display->name, i, rc);
			goto error_host_deinit;
		}
	}

	return 0;
error_host_deinit:
	for (i = i - 1; i >= 0; i--) {
		ctrl = &display->ctrl[i];
		(void)dsi_ctrl_host_deinit(ctrl->ctrl);
	}

	return rc;
}

static int dsi_display_ctrl_init(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_host_init(ctrl->ctrl,
				display->is_cont_splash_enabled);
		if (rc) {
			pr_err("[%s] failed to init host_%d, rc=%d\n",
			       display->name, i, rc);
			goto error_host_deinit;
		}
	}

	return 0;
error_host_deinit:
	for (i = i - 1; i >= 0; i--) {
		ctrl = &display->ctrl[i];
		(void)dsi_ctrl_host_deinit(ctrl->ctrl);
	}
	return rc;
}

static int dsi_display_ctrl_deinit(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_host_deinit(ctrl->ctrl);
		if (rc) {
			pr_err("[%s] failed to deinit host_%d, rc=%d\n",
			       display->name, i, rc);
		}
	}

	return rc;
}

static int dsi_display_ctrl_host_enable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	/* Host engine states are already taken care for
	 * continuous splash case
	 */
	if (display->is_cont_splash_enabled) {
		pr_debug("cont splash enabled, host enable not required\n");
		return 0;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_ctrl_set_host_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_ON);
	if (rc) {
		pr_err("[%s] failed to enable host engine, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_host_engine_state(ctrl->ctrl,
						    DSI_CTRL_ENGINE_ON);
		if (rc) {
			pr_err("[%s] failed to enable sl host engine, rc=%d\n",
			       display->name, rc);
			goto error_disable_master;
		}
	}

	return rc;
error_disable_master:
	(void)dsi_ctrl_set_host_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_OFF);
error:
	return rc;
}

static int dsi_display_ctrl_host_disable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_host_engine_state(ctrl->ctrl,
						    DSI_CTRL_ENGINE_OFF);
		if (rc)
			pr_err("[%s] failed to disable host engine, rc=%d\n",
			       display->name, rc);
	}

	rc = dsi_ctrl_set_host_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_OFF);
	if (rc) {
		pr_err("[%s] failed to disable host engine, rc=%d\n",
		       display->name, rc);
		goto error;
	}

error:
	return rc;
}

static int dsi_display_vid_engine_enable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	m_ctrl = &display->ctrl[display->video_master_idx];

	rc = dsi_ctrl_set_vid_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_ON);
	if (rc) {
		pr_err("[%s] failed to enable vid engine, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_vid_engine_state(ctrl->ctrl,
						   DSI_CTRL_ENGINE_ON);
		if (rc) {
			pr_err("[%s] failed to enable vid engine, rc=%d\n",
			       display->name, rc);
			goto error_disable_master;
		}
	}

	return rc;
error_disable_master:
	(void)dsi_ctrl_set_vid_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_OFF);
error:
	return rc;
}

static int dsi_display_vid_engine_disable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	m_ctrl = &display->ctrl[display->video_master_idx];

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_vid_engine_state(ctrl->ctrl,
						   DSI_CTRL_ENGINE_OFF);
		if (rc)
			pr_err("[%s] failed to disable vid engine, rc=%d\n",
			       display->name, rc);
	}

	rc = dsi_ctrl_set_vid_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_OFF);
	if (rc)
		pr_err("[%s] failed to disable mvid engine, rc=%d\n",
		       display->name, rc);

	return rc;
}

static int dsi_display_phy_enable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	enum dsi_phy_pll_source m_src = DSI_PLL_SOURCE_STANDALONE;

	m_ctrl = &display->ctrl[display->clk_master_idx];
	if (display->ctrl_count > 1)
		m_src = DSI_PLL_SOURCE_NATIVE;

	rc = dsi_phy_enable(m_ctrl->phy,
			    &display->config,
			    m_src,
			    true,
			    display->is_cont_splash_enabled);
	if (rc) {
		pr_err("[%s] failed to enable DSI PHY, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_phy_enable(ctrl->phy,
				    &display->config,
				    DSI_PLL_SOURCE_NON_NATIVE,
				    true,
				    display->is_cont_splash_enabled);
		if (rc) {
			pr_err("[%s] failed to enable DSI PHY, rc=%d\n",
			       display->name, rc);
			goto error_disable_master;
		}
	}

	return rc;

error_disable_master:
	(void)dsi_phy_disable(m_ctrl->phy);
error:
	return rc;
}

static int dsi_display_phy_disable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	m_ctrl = &display->ctrl[display->clk_master_idx];

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_phy_disable(ctrl->phy);
		if (rc)
			pr_err("[%s] failed to disable DSI PHY, rc=%d\n",
			       display->name, rc);
	}

	rc = dsi_phy_disable(m_ctrl->phy);
	if (rc)
		pr_err("[%s] failed to disable DSI PHY, rc=%d\n",
		       display->name, rc);

	return rc;
}

static int dsi_display_wake_up(struct dsi_display *display)
{
	return 0;
}

static int dsi_display_broadcast_cmd(struct dsi_display *display,
				     const struct mipi_dsi_msg *msg)
{
	int rc = 0;
	u32 flags, m_flags;
	struct dsi_display_ctrl *ctrl, *m_ctrl;
	int i;

	m_flags = (DSI_CTRL_CMD_BROADCAST | DSI_CTRL_CMD_BROADCAST_MASTER |
		   DSI_CTRL_CMD_DEFER_TRIGGER | DSI_CTRL_CMD_FETCH_MEMORY);
	flags = (DSI_CTRL_CMD_BROADCAST | DSI_CTRL_CMD_DEFER_TRIGGER |
		 DSI_CTRL_CMD_FETCH_MEMORY);

	if ((msg->flags & MIPI_DSI_MSG_LASTCOMMAND)) {
		flags |= DSI_CTRL_CMD_LAST_COMMAND;
		m_flags |= DSI_CTRL_CMD_LAST_COMMAND;
	}
	/*
	 * 1. Setup commands in FIFO
	 * 2. Trigger commands
	 */
	m_ctrl = &display->ctrl[display->cmd_master_idx];
	rc = dsi_ctrl_cmd_transfer(m_ctrl->ctrl, msg, m_flags);
	if (rc) {
		pr_err("[%s] cmd transfer failed on master,rc=%d\n",
		       display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (ctrl == m_ctrl)
			continue;

		rc = dsi_ctrl_cmd_transfer(ctrl->ctrl, msg, flags);
		if (rc) {
			pr_err("[%s] cmd transfer failed, rc=%d\n",
			       display->name, rc);
			goto error;
		}

		rc = dsi_ctrl_cmd_tx_trigger(ctrl->ctrl, flags);
		if (rc) {
			pr_err("[%s] cmd trigger failed, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

	rc = dsi_ctrl_cmd_tx_trigger(m_ctrl->ctrl, m_flags);
	if (rc) {
		pr_err("[%s] cmd trigger failed for master, rc=%d\n",
		       display->name, rc);
		goto error;
	}

error:
	return rc;
}

static int dsi_display_phy_sw_reset(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	/* For continuous splash use case ctrl states are updated
	 * separately and hence we do an early return
	 */
	if (display->is_cont_splash_enabled) {
		pr_debug("cont splash enabled, phy sw reset not required\n");
		return 0;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_ctrl_phy_sw_reset(m_ctrl->ctrl);
	if (rc) {
		pr_err("[%s] failed to reset phy, rc=%d\n", display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_phy_sw_reset(ctrl->ctrl);
		if (rc) {
			pr_err("[%s] failed to reset phy, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

error:
	return rc;
}

static void dsi_display_aspace_cb_locked(void *cb_data, bool is_detach)
{
	struct dsi_display *display;
	struct dsi_display_ctrl *display_ctrl;
	int rc, cnt;

	if (!cb_data) {
		pr_err("aspace cb called with invalid cb_data\n");
		return;
	}
	display = (struct dsi_display *)cb_data;

	/*
	 * acquire panel_lock to make sure no commands are in-progress
	 * while detaching the non-secure context banks
	 */
	dsi_panel_acquire_panel_lock(display->panel);

	if (is_detach) {
		/* invalidate the stored iova */
		display->cmd_buffer_iova = 0;

		/* return the virtual address mapping */
		msm_gem_put_vaddr_locked(display->tx_cmd_buf);
		msm_gem_vunmap(display->tx_cmd_buf);

	} else {
		rc = msm_gem_get_iova_locked(display->tx_cmd_buf,
				display->aspace, &(display->cmd_buffer_iova));
		if (rc) {
			pr_err("failed to get the iova rc %d\n", rc);
			goto end;
		}

		display->vaddr =
			(void *) msm_gem_get_vaddr_locked(display->tx_cmd_buf);

		if (IS_ERR_OR_NULL(display->vaddr)) {
			pr_err("failed to get va rc %d\n", rc);
			goto end;
		}
	}

	for (cnt = 0; cnt < display->ctrl_count; cnt++) {
		display_ctrl = &display->ctrl[cnt];
		display_ctrl->ctrl->cmd_buffer_size = display->cmd_buffer_size;
		display_ctrl->ctrl->cmd_buffer_iova = display->cmd_buffer_iova;
		display_ctrl->ctrl->vaddr = display->vaddr;
		display_ctrl->ctrl->secure_mode = is_detach ? true : false;
	}

end:
	/* release panel_lock */
	dsi_panel_release_panel_lock(display->panel);
}

static int dsi_host_attach(struct mipi_dsi_host *host,
			   struct mipi_dsi_device *dsi)
{
	return 0;
}

static int dsi_host_detach(struct mipi_dsi_host *host,
			   struct mipi_dsi_device *dsi)
{
	return 0;
}

static ssize_t dsi_host_transfer(struct mipi_dsi_host *host,
				 const struct mipi_dsi_msg *msg)
{
	struct dsi_display *display = to_dsi_display(host);
	struct dsi_display_ctrl *display_ctrl;
	int rc = 0, cnt = 0;

	if (!host || !msg) {
		pr_err("Invalid params\n");
		return 0;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable all DSI clocks, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	rc = dsi_display_wake_up(display);
	if (rc) {
		pr_err("[%s] failed to wake up display, rc=%d\n",
		       display->name, rc);
		goto error_disable_clks;
	}

	rc = dsi_display_cmd_engine_enable(display);
	if (rc) {
		pr_err("[%s] failed to enable cmd engine, rc=%d\n",
		       display->name, rc);
		goto error_disable_clks;
	}

	if (display->tx_cmd_buf == NULL) {
		mutex_lock(&display->drm_dev->struct_mutex);
		display->tx_cmd_buf = msm_gem_new(display->drm_dev,
				SZ_4K,
				MSM_BO_UNCACHED);
		mutex_unlock(&display->drm_dev->struct_mutex);

		display->cmd_buffer_size = SZ_4K;

		if ((display->tx_cmd_buf) == NULL) {
			pr_err("value of display->tx_cmd_buf is NULL");
			goto error_disable_cmd_engine;
		}

		display->aspace = msm_gem_smmu_address_space_get(
				display->drm_dev, MSM_SMMU_DOMAIN_UNSECURE);
		if (!display->aspace) {
			pr_err("failed to get aspace\n");
			rc = -EINVAL;
			goto free_gem;
		}

		/* register to aspace */
		rc = msm_gem_address_space_register_cb(display->aspace,
				dsi_display_aspace_cb_locked, (void *)display);
		if (rc) {
			pr_err("failed to register callback %d", rc);
			goto free_gem;
		}

		rc = msm_gem_get_iova(display->tx_cmd_buf, display->aspace,
					&(display->cmd_buffer_iova));
		if (rc) {
			pr_err("failed to get the iova rc %d\n", rc);
			goto free_aspace_cb;
		}

		display->vaddr =
			(void *) msm_gem_get_vaddr(display->tx_cmd_buf);

		if (IS_ERR_OR_NULL(display->vaddr)) {
			pr_err("failed to get va rc %d\n", rc);
			rc = -EINVAL;
			goto put_iova;
		}

		for (cnt = 0; cnt < display->ctrl_count; cnt++) {
			display_ctrl = &display->ctrl[cnt];
			display_ctrl->ctrl->cmd_buffer_size = SZ_4K;
			display_ctrl->ctrl->cmd_buffer_iova =
						display->cmd_buffer_iova;
			display_ctrl->ctrl->vaddr = display->vaddr;
		}
	}

	if (display->ctrl_count > 1 && !(msg->flags & MIPI_DSI_MSG_UNICAST)) {
		rc = dsi_display_broadcast_cmd(display, msg);
		if (rc) {
			pr_err("[%s] cmd broadcast failed, rc=%d\n",
			       display->name, rc);
			goto error_disable_cmd_engine;
		}
	} else {
		int ctrl_idx = (msg->flags & MIPI_DSI_MSG_UNICAST) ?
				msg->ctrl : 0;

		rc = dsi_ctrl_cmd_transfer(display->ctrl[ctrl_idx].ctrl, msg,
					  DSI_CTRL_CMD_FETCH_MEMORY);
		if (rc) {
			pr_err("[%s] cmd transfer failed, rc=%d\n",
			       display->name, rc);
			goto error_disable_cmd_engine;
		}
	}

error_disable_cmd_engine:
	(void)dsi_display_cmd_engine_disable(display);
error_disable_clks:
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
	if (rc) {
		pr_err("[%s] failed to disable all DSI clocks, rc=%d\n",
		       display->name, rc);
	}
	return rc;
put_iova:
	msm_gem_put_iova(display->tx_cmd_buf, display->aspace);
free_aspace_cb:
	msm_gem_address_space_unregister_cb(display->aspace,
			dsi_display_aspace_cb_locked, display);
free_gem:
	mutex_lock(&display->drm_dev->struct_mutex);
	msm_gem_free_object(display->tx_cmd_buf);
	mutex_unlock(&display->drm_dev->struct_mutex);
error:
	return rc;
}


static struct mipi_dsi_host_ops dsi_host_ops = {
	.attach = dsi_host_attach,
	.detach = dsi_host_detach,
	.transfer = dsi_host_transfer,
};

static int dsi_display_mipi_host_init(struct dsi_display *display)
{
	int rc = 0;
	struct mipi_dsi_host *host = &display->host;

	host->dev = &display->pdev->dev;
	host->ops = &dsi_host_ops;

	rc = mipi_dsi_host_register(host);
	if (rc) {
		pr_err("[%s] failed to register mipi dsi host, rc=%d\n",
		       display->name, rc);
		goto error;
	}

error:
	return rc;
}
static int dsi_display_mipi_host_deinit(struct dsi_display *display)
{
	int rc = 0;
	struct mipi_dsi_host *host = &display->host;

	mipi_dsi_host_unregister(host);

	host->dev = NULL;
	host->ops = NULL;

	return rc;
}

static int dsi_display_clocks_deinit(struct dsi_display *display)
{
	int rc = 0;
	struct dsi_clk_link_set *src = &display->clock_info.src_clks;
	struct dsi_clk_link_set *mux = &display->clock_info.mux_clks;
	struct dsi_clk_link_set *shadow = &display->clock_info.shadow_clks;

	if (src->byte_clk) {
		devm_clk_put(&display->pdev->dev, src->byte_clk);
		src->byte_clk = NULL;
	}

	if (src->pixel_clk) {
		devm_clk_put(&display->pdev->dev, src->pixel_clk);
		src->pixel_clk = NULL;
	}

	if (mux->byte_clk) {
		devm_clk_put(&display->pdev->dev, mux->byte_clk);
		mux->byte_clk = NULL;
	}

	if (mux->pixel_clk) {
		devm_clk_put(&display->pdev->dev, mux->pixel_clk);
		mux->pixel_clk = NULL;
	}

	if (shadow->byte_clk) {
		devm_clk_put(&display->pdev->dev, shadow->byte_clk);
		shadow->byte_clk = NULL;
	}

	if (shadow->pixel_clk) {
		devm_clk_put(&display->pdev->dev, shadow->pixel_clk);
		shadow->pixel_clk = NULL;
	}

	return rc;
}

static int dsi_display_clocks_init(struct dsi_display *display)
{
	int rc = 0;
	struct dsi_clk_link_set *src = &display->clock_info.src_clks;
	struct dsi_clk_link_set *mux = &display->clock_info.mux_clks;
	struct dsi_clk_link_set *shadow = &display->clock_info.shadow_clks;

	src->byte_clk = devm_clk_get(&display->pdev->dev, "src_byte_clk");
	if (IS_ERR_OR_NULL(src->byte_clk)) {
		rc = PTR_ERR(src->byte_clk);
		src->byte_clk = NULL;
		pr_err("failed to get src_byte_clk, rc=%d\n", rc);
		goto error;
	}

	src->pixel_clk = devm_clk_get(&display->pdev->dev, "src_pixel_clk");
	if (IS_ERR_OR_NULL(src->pixel_clk)) {
		rc = PTR_ERR(src->pixel_clk);
		src->pixel_clk = NULL;
		pr_err("failed to get src_pixel_clk, rc=%d\n", rc);
		goto error;
	}

	mux->byte_clk = devm_clk_get(&display->pdev->dev, "mux_byte_clk");
	if (IS_ERR_OR_NULL(mux->byte_clk)) {
		rc = PTR_ERR(mux->byte_clk);
		pr_debug("failed to get mux_byte_clk, rc=%d\n", rc);
		mux->byte_clk = NULL;
		/*
		 * Skip getting rest of clocks since one failed. This is a
		 * non-critical failure since these clocks are requied only for
		 * dynamic refresh use cases.
		 */
		rc = 0;
		goto done;
	};

	mux->pixel_clk = devm_clk_get(&display->pdev->dev, "mux_pixel_clk");
	if (IS_ERR_OR_NULL(mux->pixel_clk)) {
		rc = PTR_ERR(mux->pixel_clk);
		mux->pixel_clk = NULL;
		pr_debug("failed to get mux_pixel_clk, rc=%d\n", rc);
		/*
		 * Skip getting rest of clocks since one failed. This is a
		 * non-critical failure since these clocks are requied only for
		 * dynamic refresh use cases.
		 */
		rc = 0;
		goto done;
	};

	shadow->byte_clk = devm_clk_get(&display->pdev->dev, "shadow_byte_clk");
	if (IS_ERR_OR_NULL(shadow->byte_clk)) {
		rc = PTR_ERR(shadow->byte_clk);
		shadow->byte_clk = NULL;
		pr_err("failed to get shadow_byte_clk, rc=%d\n", rc);
		/*
		 * Skip getting rest of clocks since one failed. This is a
		 * non-critical failure since these clocks are requied only for
		 * dynamic refresh use cases.
		 */
		rc = 0;
		goto done;
	};

	shadow->pixel_clk = devm_clk_get(&display->pdev->dev,
					 "shadow_pixel_clk");
	if (IS_ERR_OR_NULL(shadow->pixel_clk)) {
		rc = PTR_ERR(shadow->pixel_clk);
		shadow->pixel_clk = NULL;
		pr_err("failed to get shadow_pixel_clk, rc=%d\n", rc);
		/*
		 * Skip getting rest of clocks since one failed. This is a
		 * non-critical failure since these clocks are requied only for
		 * dynamic refresh use cases.
		 */
		rc = 0;
		goto done;
	};

done:
	return 0;
error:
	(void)dsi_display_clocks_deinit(display);
	return rc;
}

static int dsi_display_clk_ctrl_cb(void *priv,
	struct dsi_clk_ctrl_info clk_state_info)
{
	int rc = 0;
	struct dsi_display *display = NULL;
	void *clk_handle = NULL;

	if (!priv) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	display = priv;

	if (clk_state_info.client == DSI_CLK_REQ_MDP_CLIENT) {
		clk_handle = display->mdp_clk_handle;
	} else if (clk_state_info.client == DSI_CLK_REQ_DSI_CLIENT) {
		clk_handle = display->dsi_clk_handle;
	} else {
		pr_err("invalid clk handle, return error\n");
		return -EINVAL;
	}

	/*
	 * TODO: Wait for CMD_MDP_DONE interrupt if MDP client tries
	 * to turn off DSI clocks.
	 */
	rc = dsi_display_clk_ctrl(clk_handle,
		clk_state_info.clk_type, clk_state_info.clk_state);
	if (rc) {
		pr_err("[%s] failed to %d DSI %d clocks, rc=%d\n",
		       display->name, clk_state_info.clk_state,
		       clk_state_info.clk_type, rc);
		return rc;
	}
	return 0;
}

int dsi_pre_clkoff_cb(void *priv,
			   enum dsi_clk_type clk,
			   enum dsi_clk_state new_state)
{
	int rc = 0;
	struct dsi_display *display = priv;

	if ((clk & DSI_LINK_CLK) && (new_state == DSI_CLK_OFF)) {
		/*
		 * If ULPS feature is enabled, enter ULPS first.
		 * However, when blanking the panel, we should enter ULPS
		 * only if ULPS during suspend feature is enabled.
		 */
		if (!dsi_panel_initialized(display->panel)) {
			if (display->panel->ulps_suspend_enabled)
				rc = dsi_display_set_ulps(display, true);
		} else if (dsi_panel_ulps_feature_enabled(display->panel)) {
			rc = dsi_display_set_ulps(display, true);
		}
		if (rc)
			pr_err("%s: failed enable ulps, rc = %d\n",
			       __func__, rc);
	}

	if ((clk & DSI_CORE_CLK) && (new_state == DSI_CLK_OFF)) {
		/*
		 * Enable DSI clamps only if entering idle power collapse or
		 * when ULPS during suspend is enabled..
		 */
		if (dsi_panel_initialized(display->panel) ||
			display->panel->ulps_suspend_enabled) {
			dsi_display_phy_idle_off(display);
			rc = dsi_display_set_clamp(display, true);
			if (rc)
				pr_err("%s: Failed to enable dsi clamps. rc=%d\n",
					__func__, rc);

			rc = dsi_display_phy_reset_config(display, false);
			if (rc)
				pr_err("%s: Failed to reset phy, rc=%d\n",
						__func__, rc);
		} else {
			/* Make sure that controller is not in ULPS state when
			 * the DSI link is not active.
			 */
			rc = dsi_display_set_ulps(display, false);
			if (rc)
				pr_err("%s: failed to disable ulps. rc=%d\n",
					__func__, rc);
		}
	}

	return rc;
}

int dsi_post_clkon_cb(void *priv,
			   enum dsi_clk_type clk,
			   enum dsi_clk_state curr_state)
{
	int rc = 0;
	struct dsi_display *display = priv;
	bool mmss_clamp = false;

	if (clk & DSI_CORE_CLK) {
		mmss_clamp = display->clamp_enabled;
		/*
		 * controller setup is needed if coming out of idle
		 * power collapse with clamps enabled.
		 */
		if (mmss_clamp)
			dsi_display_ctrl_setup(display);

		if (display->ulps_enabled && mmss_clamp) {
			/*
			 * ULPS Entry Request. This is needed if the lanes were
			 * in ULPS prior to power collapse, since after
			 * power collapse and reset, the DSI controller resets
			 * back to idle state and not ULPS. This ulps entry
			 * request will transition the state of the DSI
			 * controller to ULPS which will match the state of the
			 * DSI phy. This needs to be done prior to disabling
			 * the DSI clamps.
			 *
			 * Also, reset the ulps flag so that ulps_config
			 * function would reconfigure the controller state to
			 * ULPS.
			 */
			display->ulps_enabled = false;
			rc = dsi_display_set_ulps(display, true);
			if (rc) {
				pr_err("%s: Failed to enter ULPS. rc=%d\n",
					__func__, rc);
				goto error;
			}
		}

		rc = dsi_display_phy_reset_config(display, true);
		if (rc) {
			pr_err("%s: Failed to reset phy, rc=%d\n",
						__func__, rc);
			goto error;
		}

		rc = dsi_display_set_clamp(display, false);
		if (rc) {
			pr_err("%s: Failed to disable dsi clamps. rc=%d\n",
				__func__, rc);
			goto error;
		}

		/*
		 * Phy setup is needed if coming out of idle
		 * power collapse with clamps enabled.
		 */
		if (display->phy_idle_power_off || mmss_clamp)
			dsi_display_phy_idle_on(display, mmss_clamp);
	}
	if (clk & DSI_LINK_CLK) {
		if (display->ulps_enabled) {
			rc = dsi_display_set_ulps(display, false);
			if (rc) {
				pr_err("%s: failed to disable ulps, rc= %d\n",
				       __func__, rc);
				goto error;
			}
		}
	}
error:
	return rc;
}

int dsi_post_clkoff_cb(void *priv,
			    enum dsi_clk_type clk_type,
			    enum dsi_clk_state curr_state)
{
	int rc = 0;
	struct dsi_display *display = priv;

	if (!display) {
		pr_err("%s: Invalid arg\n", __func__);
		return -EINVAL;
	}

	if ((clk_type & DSI_CORE_CLK) &&
	    (curr_state == DSI_CLK_OFF)) {

		rc = dsi_display_phy_power_off(display);
		if (rc)
			pr_err("[%s] failed to power off PHY, rc=%d\n",
				   display->name, rc);

		rc = dsi_display_ctrl_power_off(display);
		if (rc)
			pr_err("[%s] failed to power DSI vregs, rc=%d\n",
				   display->name, rc);
	}
	return rc;
}

int dsi_pre_clkon_cb(void *priv,
			  enum dsi_clk_type clk_type,
			  enum dsi_clk_state new_state)
{
	int rc = 0;
	struct dsi_display *display = priv;

	if (!display) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if ((clk_type & DSI_CORE_CLK) && (new_state == DSI_CLK_ON)) {
		/*
		 * Enable DSI core power
		 * 1.> PANEL_PM are controlled as part of
		 *     panel_power_ctrl. Needed not be handled here.
		 * 2.> CORE_PM are controlled by dsi clk manager.
		 * 3.> CTRL_PM need to be enabled/disabled
		 *     only during unblank/blank. Their state should
		 *     not be changed during static screen.
		 */

	  pr_debug("updating power states for ctrl and phy\n");
		rc = dsi_display_ctrl_power_on(display);
		if (rc) {
			pr_err("[%s] failed to power on dsi controllers, rc=%d\n",
				   display->name, rc);
			return rc;
		}

		rc = dsi_display_phy_power_on(display);
		if (rc) {
			pr_err("[%s] failed to power on dsi phy, rc = %d\n",
				   display->name, rc);
			return rc;
		}

		pr_debug("%s: Enable DSI core power\n", __func__);
	}

	return rc;
}

static void __set_lane_map_v2(u8 *lane_map_v2,
	enum dsi_phy_data_lanes lane0,
	enum dsi_phy_data_lanes lane1,
	enum dsi_phy_data_lanes lane2,
	enum dsi_phy_data_lanes lane3)
{
	lane_map_v2[DSI_LOGICAL_LANE_0] = lane0;
	lane_map_v2[DSI_LOGICAL_LANE_1] = lane1;
	lane_map_v2[DSI_LOGICAL_LANE_2] = lane2;
	lane_map_v2[DSI_LOGICAL_LANE_3] = lane3;
}

static int dsi_display_parse_lane_map(struct dsi_display *display)
{
	int rc = 0, i = 0;
	const char *data;
	u8 temp[DSI_LANE_MAX - 1];

	if (!display) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	/* lane-map-v2 supersedes lane-map-v1 setting */
	rc = of_property_read_u8_array(display->pdev->dev.of_node,
		"qcom,lane-map-v2", temp, (DSI_LANE_MAX - 1));
	if (!rc) {
		for (i = DSI_LOGICAL_LANE_0; i < (DSI_LANE_MAX - 1); i++)
			display->lane_map.lane_map_v2[i] = BIT(temp[i]);
		return 0;
	} else if (rc != EINVAL) {
		pr_debug("Incorrect mapping, configure default\n");
		goto set_default;
	}

	/* lane-map older version, for DSI controller version < 2.0 */
	data = of_get_property(display->pdev->dev.of_node,
		"qcom,lane-map", NULL);
	if (!data)
		goto set_default;

	if (!strcmp(data, "lane_map_3012")) {
		display->lane_map.lane_map_v1 = DSI_LANE_MAP_3012;
		__set_lane_map_v2(display->lane_map.lane_map_v2,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_0);
	} else if (!strcmp(data, "lane_map_2301")) {
		display->lane_map.lane_map_v1 = DSI_LANE_MAP_2301;
		__set_lane_map_v2(display->lane_map.lane_map_v2,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_1);
	} else if (!strcmp(data, "lane_map_1230")) {
		display->lane_map.lane_map_v1 = DSI_LANE_MAP_1230;
		__set_lane_map_v2(display->lane_map.lane_map_v2,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_2);
	} else if (!strcmp(data, "lane_map_0321")) {
		display->lane_map.lane_map_v1 = DSI_LANE_MAP_0321;
		__set_lane_map_v2(display->lane_map.lane_map_v2,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_1);
	} else if (!strcmp(data, "lane_map_1032")) {
		display->lane_map.lane_map_v1 = DSI_LANE_MAP_1032;
		__set_lane_map_v2(display->lane_map.lane_map_v2,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_2);
	} else if (!strcmp(data, "lane_map_2103")) {
		display->lane_map.lane_map_v1 = DSI_LANE_MAP_2103;
		__set_lane_map_v2(display->lane_map.lane_map_v2,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_3);
	} else if (!strcmp(data, "lane_map_3210")) {
		display->lane_map.lane_map_v1 = DSI_LANE_MAP_3210;
		__set_lane_map_v2(display->lane_map.lane_map_v2,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_0);
	} else {
		pr_warn("%s: invalid lane map %s specified. defaulting to lane_map0123\n",
			__func__, data);
		goto set_default;
	}
	return 0;

set_default:
	/* default lane mapping */
	__set_lane_map_v2(display->lane_map.lane_map_v2, DSI_PHYSICAL_LANE_0,
		DSI_PHYSICAL_LANE_1, DSI_PHYSICAL_LANE_2, DSI_PHYSICAL_LANE_3);
	display->lane_map.lane_map_v1 = DSI_LANE_MAP_0123;
	return 0;
}

static int dsi_display_parse_dt(struct dsi_display *display)
{
	int rc = 0;
	int i;
	u32 phy_count = 0;
	struct device_node *of_node;

	/* Parse controllers */
	for (i = 0; i < MAX_DSI_CTRLS_PER_DISPLAY; i++) {
		of_node = of_parse_phandle(display->pdev->dev.of_node,
					   "qcom,dsi-ctrl", i);
		if (!of_node) {
			if (!i) {
				pr_err("No controllers present\n");
				return -ENODEV;
			}
			break;
		}

		display->ctrl[i].ctrl_of_node = of_node;
		display->ctrl_count++;
	}

	/* Parse Phys */
	for (i = 0; i < MAX_DSI_CTRLS_PER_DISPLAY; i++) {
		of_node = of_parse_phandle(display->pdev->dev.of_node,
					   "qcom,dsi-phy", i);
		if (!of_node) {
			if (!i) {
				pr_err("No PHY devices present\n");
				rc = -ENODEV;
				goto error;
			}
			break;
		}

		display->ctrl[i].phy_of_node = of_node;
		phy_count++;
	}

	if (phy_count != display->ctrl_count) {
		pr_err("Number of controllers does not match PHYs\n");
		rc = -ENODEV;
		goto error;
	}

	of_node = of_parse_phandle(display->pdev->dev.of_node,
				   "qcom,dsi-panel", 0);
	if (!of_node) {
		pr_err("No Panel device present\n");
		rc = -ENODEV;
		goto error;
	} else {
		display->panel_of = of_node;
	}

error:
	return rc;
}

static int dsi_display_res_init(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		ctrl->ctrl = dsi_ctrl_get(ctrl->ctrl_of_node);
		if (IS_ERR_OR_NULL(ctrl->ctrl)) {
			rc = PTR_ERR(ctrl->ctrl);
			pr_err("failed to get dsi controller, rc=%d\n", rc);
			ctrl->ctrl = NULL;
			goto error_ctrl_put;
		}

		ctrl->phy = dsi_phy_get(ctrl->phy_of_node);
		if (IS_ERR_OR_NULL(ctrl->phy)) {
			rc = PTR_ERR(ctrl->phy);
			pr_err("failed to get phy controller, rc=%d\n", rc);
			dsi_ctrl_put(ctrl->ctrl);
			ctrl->phy = NULL;
			goto error_ctrl_put;
		}
	}

	display->panel = dsi_panel_get(&display->pdev->dev, display->panel_of,
						display->cmdline_topology);
	if (IS_ERR_OR_NULL(display->panel)) {
		rc = PTR_ERR(display->panel);
		pr_err("failed to get panel, rc=%d\n", rc);
		display->panel = NULL;
		goto error_ctrl_put;
	}

	rc = dsi_display_parse_lane_map(display);
	if (rc) {
		pr_err("Lane map not found, rc=%d\n", rc);
		goto error_ctrl_put;
	}

	rc = dsi_display_clocks_init(display);
	if (rc) {
		pr_err("Failed to parse clock data, rc=%d\n", rc);
		goto error_ctrl_put;
	}

	return 0;
error_ctrl_put:
	for (i = i - 1; i >= 0; i--) {
		ctrl = &display->ctrl[i];
		dsi_ctrl_put(ctrl->ctrl);
		dsi_phy_put(ctrl->phy);
	}
	return rc;
}

static int dsi_display_res_deinit(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	rc = dsi_display_clocks_deinit(display);
	if (rc)
		pr_err("clocks deinit failed, rc=%d\n", rc);

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		dsi_phy_put(ctrl->phy);
		dsi_ctrl_put(ctrl->ctrl);
	}

	if (display->panel)
		dsi_panel_put(display->panel);

	return rc;
}

static int dsi_display_validate_mode_set(struct dsi_display *display,
					 struct dsi_display_mode *mode,
					 u32 flags)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	/*
	 * To set a mode:
	 * 1. Controllers should be turned off.
	 * 2. Link clocks should be off.
	 * 3. Phy should be disabled.
	 */

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if ((ctrl->power_state > DSI_CTRL_POWER_VREG_ON) ||
		    (ctrl->phy_enabled)) {
			rc = -EINVAL;
			goto error;
		}
	}

error:
	return rc;
}

static bool dsi_display_is_seamless_dfps_possible(
		const struct dsi_display *display,
		const struct dsi_display_mode *tgt,
		const enum dsi_dfps_type dfps_type)
{
	struct dsi_display_mode *cur;

	if (!display || !tgt || !display->panel) {
		pr_err("Invalid params\n");
		return false;
	}

	cur = display->panel->cur_mode;

	if (cur->timing.h_active != tgt->timing.h_active) {
		pr_debug("timing.h_active differs %d %d\n",
				cur->timing.h_active, tgt->timing.h_active);
		return false;
	}

	if (cur->timing.h_back_porch != tgt->timing.h_back_porch) {
		pr_debug("timing.h_back_porch differs %d %d\n",
				cur->timing.h_back_porch,
				tgt->timing.h_back_porch);
		return false;
	}

	if (cur->timing.h_sync_width != tgt->timing.h_sync_width) {
		pr_debug("timing.h_sync_width differs %d %d\n",
				cur->timing.h_sync_width,
				tgt->timing.h_sync_width);
		return false;
	}

	if (cur->timing.h_front_porch != tgt->timing.h_front_porch) {
		pr_debug("timing.h_front_porch differs %d %d\n",
				cur->timing.h_front_porch,
				tgt->timing.h_front_porch);
		if (dfps_type != DSI_DFPS_IMMEDIATE_HFP)
			return false;
	}

	if (cur->timing.h_skew != tgt->timing.h_skew) {
		pr_debug("timing.h_skew differs %d %d\n",
				cur->timing.h_skew,
				tgt->timing.h_skew);
		return false;
	}

	/* skip polarity comparison */

	if (cur->timing.v_active != tgt->timing.v_active) {
		pr_debug("timing.v_active differs %d %d\n",
				cur->timing.v_active,
				tgt->timing.v_active);
		return false;
	}

	if (cur->timing.v_back_porch != tgt->timing.v_back_porch) {
		pr_debug("timing.v_back_porch differs %d %d\n",
				cur->timing.v_back_porch,
				tgt->timing.v_back_porch);
		return false;
	}

	if (cur->timing.v_sync_width != tgt->timing.v_sync_width) {
		pr_debug("timing.v_sync_width differs %d %d\n",
				cur->timing.v_sync_width,
				tgt->timing.v_sync_width);
		return false;
	}

	if (cur->timing.v_front_porch != tgt->timing.v_front_porch) {
		pr_debug("timing.v_front_porch differs %d %d\n",
				cur->timing.v_front_porch,
				tgt->timing.v_front_porch);
		if (dfps_type != DSI_DFPS_IMMEDIATE_VFP)
			return false;
	}

	/* skip polarity comparison */

	if (cur->timing.refresh_rate == tgt->timing.refresh_rate)
		pr_debug("timing.refresh_rate identical %d %d\n",
				cur->timing.refresh_rate,
				tgt->timing.refresh_rate);

	if (cur->pixel_clk_khz != tgt->pixel_clk_khz)
		pr_debug("pixel_clk_khz differs %d %d\n",
				cur->pixel_clk_khz, tgt->pixel_clk_khz);

	if (cur->dsi_mode_flags != tgt->dsi_mode_flags)
		pr_debug("flags differs %d %d\n",
				cur->dsi_mode_flags, tgt->dsi_mode_flags);

	return true;
}

static int dsi_display_dfps_update(struct dsi_display *display,
				   struct dsi_display_mode *dsi_mode)
{
	struct dsi_mode_info *timing;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	struct dsi_display_mode *panel_mode;
	struct dsi_dfps_capabilities dfps_caps;
	int rc = 0;
	int i = 0;

	if (!display || !dsi_mode || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}
	timing = &dsi_mode->timing;

	dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
	if (!dfps_caps.dfps_support) {
		pr_err("dfps not supported\n");
		return -ENOTSUPP;
	}

	if (dfps_caps.type == DSI_DFPS_IMMEDIATE_CLK) {
		pr_err("dfps clock method not supported\n");
		return -ENOTSUPP;
	}

	/* For split DSI, update the clock master first */

	pr_debug("configuring seamless dynamic fps\n\n");

	m_ctrl = &display->ctrl[display->clk_master_idx];
	rc = dsi_ctrl_async_timing_update(m_ctrl->ctrl, timing);
	if (rc) {
		pr_err("[%s] failed to dfps update host_%d, rc=%d\n",
				display->name, i, rc);
		goto error;
	}

	/* Update the rest of the controllers */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_async_timing_update(ctrl->ctrl, timing);
		if (rc) {
			pr_err("[%s] failed to dfps update host_%d, rc=%d\n",
					display->name, i, rc);
			goto error;
		}
	}

	panel_mode = display->panel->cur_mode;
	memcpy(panel_mode, dsi_mode, sizeof(*panel_mode));
	/*
	 * dsi_mode_flags flags are used to communicate with other drm driver
	 * components, and are transient. They aren't inherently part of the
	 * display panel's mode and shouldn't be saved into the cached currently
	 * active mode.
	 */
	panel_mode->dsi_mode_flags = 0;

error:
	return rc;
}

static int dsi_display_dfps_calc_front_porch(
		u32 old_fps,
		u32 new_fps,
		u32 a_total,
		u32 b_total,
		u32 b_fp,
		u32 *b_fp_out)
{
	s32 b_fp_new;
	int add_porches, diff;

	if (!b_fp_out) {
		pr_err("Invalid params");
		return -EINVAL;
	}

	if (!a_total || !new_fps) {
		pr_err("Invalid pixel total or new fps in mode request\n");
		return -EINVAL;
	}

	/*
	 * Keep clock, other porches constant, use new fps, calc front porch
	 * new_vtotal = old_vtotal * (old_fps / new_fps )
	 * new_vfp - old_vfp = new_vtotal - old_vtotal
	 * new_vfp = old_vfp + old_vtotal * ((old_fps - new_fps)/ new_fps)
	 */
	diff = abs(old_fps - new_fps);
	add_porches = mult_frac(b_total, diff, new_fps);

	if (old_fps > new_fps)
		b_fp_new = b_fp + add_porches;
	else
		b_fp_new = b_fp - add_porches;

	pr_debug("fps %u a %u b %u b_fp %u new_fp %d\n",
			new_fps, a_total, b_total, b_fp, b_fp_new);

	if (b_fp_new < 0) {
		pr_err("Invalid new_hfp calcluated%d\n", b_fp_new);
		return -EINVAL;
	}

	/**
	 * TODO: To differentiate from clock method when communicating to the
	 * other components, perhaps we should set clk here to original value
	 */
	*b_fp_out = b_fp_new;

	return 0;
}

/**
 * dsi_display_get_dfps_timing() - Get the new dfps values.
 * @display:         DSI display handle.
 * @adj_mode:        Mode value structure to be changed.
 *                   It contains old timing values and latest fps value.
 *                   New timing values are updated based on new fps.
 * @curr_refresh_rate:  Current fps rate.
 *                      If zero , current fps rate is taken from
 *                      display->panel->cur_mode.
 * Return: error code.
 */
static int dsi_display_get_dfps_timing(struct dsi_display *display,
			struct dsi_display_mode *adj_mode,
				u32 curr_refresh_rate)
{
	struct dsi_dfps_capabilities dfps_caps;
	struct dsi_display_mode per_ctrl_mode;
	struct dsi_mode_info *timing;
	struct dsi_ctrl *m_ctrl;

	int rc = 0;

	if (!display || !adj_mode) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}
	m_ctrl = display->ctrl[display->clk_master_idx].ctrl;

	dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
	if (!dfps_caps.dfps_support) {
		pr_err("dfps not supported by panel\n");
		return -EINVAL;
	}

	per_ctrl_mode = *adj_mode;
	adjust_timing_by_ctrl_count(display, &per_ctrl_mode);

	if (!curr_refresh_rate) {
		if (!dsi_display_is_seamless_dfps_possible(display,
				&per_ctrl_mode, dfps_caps.type)) {
			pr_err("seamless dynamic fps not supported for mode\n");
			return -EINVAL;
		}
		if (display->panel->cur_mode) {
			curr_refresh_rate =
				display->panel->cur_mode->timing.refresh_rate;
		} else {
			pr_err("cur_mode is not initialized\n");
			return -EINVAL;
		}
	}
	/* TODO: Remove this direct reference to the dsi_ctrl */
	timing = &per_ctrl_mode.timing;

	switch (dfps_caps.type) {
	case DSI_DFPS_IMMEDIATE_VFP:
		rc = dsi_display_dfps_calc_front_porch(
				curr_refresh_rate,
				timing->refresh_rate,
				DSI_H_TOTAL(timing),
				DSI_V_TOTAL(timing),
				timing->v_front_porch,
				&adj_mode->timing.v_front_porch);
		break;

	case DSI_DFPS_IMMEDIATE_HFP:
		rc = dsi_display_dfps_calc_front_porch(
				curr_refresh_rate,
				timing->refresh_rate,
				DSI_V_TOTAL(timing),
				DSI_H_TOTAL(timing),
				timing->h_front_porch,
				&adj_mode->timing.h_front_porch);
		if (!rc)
			adj_mode->timing.h_front_porch *= display->ctrl_count;
		break;

	default:
		pr_err("Unsupported DFPS mode %d\n", dfps_caps.type);
		rc = -ENOTSUPP;
	}

	return rc;
}

static bool dsi_display_validate_mode_seamless(struct dsi_display *display,
		struct dsi_display_mode *adj_mode)
{
	int rc = 0;

	if (!display || !adj_mode) {
		pr_err("Invalid params\n");
		return false;
	}

	/* Currently the only seamless transition is dynamic fps */
	rc = dsi_display_get_dfps_timing(display, adj_mode, 0);
	if (rc) {
		pr_debug("Dynamic FPS not supported for seamless\n");
	} else {
		pr_debug("Mode switch is seamless Dynamic FPS\n");
		adj_mode->dsi_mode_flags |= DSI_MODE_FLAG_DFPS |
				DSI_MODE_FLAG_VBLANK_PRE_MODESET;
	}

	return rc;
}

static int dsi_display_set_mode_sub(struct dsi_display *display,
				    struct dsi_display_mode *mode,
				    u32 flags)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;
	struct dsi_display_mode_priv_info *priv_info;

	priv_info = mode->priv_info;
	if (!priv_info) {
		pr_err("[%s] failed to get private info of the display mode",
			display->name);
		return -EINVAL;
	}

	rc = dsi_panel_get_host_cfg_for_mode(display->panel,
					     mode,
					     &display->config);
	if (rc) {
		pr_err("[%s] failed to get host config for mode, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	memcpy(&display->config.lane_map, &display->lane_map,
	       sizeof(display->lane_map));

	if (mode->dsi_mode_flags &
			(DSI_MODE_FLAG_DFPS | DSI_MODE_FLAG_VRR)) {
		rc = dsi_display_dfps_update(display, mode);
		if (rc) {
			pr_err("[%s]DSI dfps update failed, rc=%d\n",
					display->name, rc);
			goto error;
		}
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_update_host_config(ctrl->ctrl, &display->config,
				mode->dsi_mode_flags, display->dsi_clk_handle);
		if (rc) {
			pr_err("[%s] failed to update ctrl config, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];

		if (!ctrl->phy || !ctrl->ctrl)
			continue;

		rc = dsi_phy_set_clk_freq(ctrl->phy, &ctrl->ctrl->clk_freq);
		if (rc) {
			pr_err("[%s] failed to set phy clk freq, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

	if (priv_info->phy_timing_len) {
		for (i = 0; i < display->ctrl_count; i++) {
			ctrl = &display->ctrl[i];
			 rc = dsi_phy_set_timing_params(ctrl->phy,
				priv_info->phy_timing_val,
				priv_info->phy_timing_len);
			if (rc)
				pr_err("failed to add DSI PHY timing params");
		}
	}
error:
	return rc;
}

/**
 * _dsi_display_dev_init - initializes the display device
 * Initialization will acquire references to the resources required for the
 * display hardware to function.
 * @display:         Handle to the display
 * Returns:          Zero on success
 */
static int _dsi_display_dev_init(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("invalid display\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_display_parse_dt(display);
	if (rc) {
		pr_err("[%s] failed to parse dt, rc=%d\n", display->name, rc);
		goto error;
	}

	rc = dsi_display_res_init(display);
	if (rc) {
		pr_err("[%s] failed to initialize resources, rc=%d\n",
		       display->name, rc);
		goto error;
	}
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

/**
 * _dsi_display_dev_deinit - deinitializes the display device
 * All the resources acquired during device init will be released.
 * @display:        Handle to the display
 * Returns:         Zero on success
 */
static int _dsi_display_dev_deinit(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("invalid display\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_display_res_deinit(display);
	if (rc)
		pr_err("[%s] failed to deinitialize resource, rc=%d\n",
		       display->name, rc);

	mutex_unlock(&display->display_lock);

	return rc;
}

/**
 * dsi_display_splash_res_init() - Initialize resources for continuous splash
 * @display:    Pointer to dsi display
 * Returns:     Zero on success
 */
static int dsi_display_splash_res_init(struct  dsi_display *display)
{
	int rc = 0;

	/* Vote for gdsc required to read register address space */

	display->cont_splash_client = sde_power_client_create(display->phandle,
						"cont_splash_client");
	rc = sde_power_resource_enable(display->phandle,
			display->cont_splash_client, true);
	if (rc) {
		pr_err("failed to vote gdsc for continuous splash, rc=%d\n",
							rc);
		return -EINVAL;
	}

	/* Verify whether continuous splash is enabled or not */
	display->is_cont_splash_enabled =
		dsi_display_get_cont_splash_status(display);
	if (!display->is_cont_splash_enabled) {
		pr_err("Continuous splash is not enabled\n");
		goto splash_disabled;
	}

	/* Update splash status for clock manager */
	dsi_display_clk_mngr_update_splash_status(display->clk_mngr,
				display->is_cont_splash_enabled);

	/* Vote for Core clk and link clk. Votes on ctrl and phy
	 * regulator are inplicit from  pre clk on callback
	 */
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI link clocks, rc=%d\n",
		       display->name, rc);
		goto clk_manager_update;
	}

	/* Vote on panel regulator will be removed during suspend path */
	rc = dsi_pwr_enable_regulator(&display->panel->power_info, true);
	if (rc) {
		pr_err("[%s] failed to enable vregs, rc=%d\n",
				display->panel->name, rc);
		goto clks_disabled;
	}

	dsi_config_host_engine_state_for_cont_splash(display);

	return rc;

clks_disabled:
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);

clk_manager_update:
	/* Update splash status for clock manager */
	dsi_display_clk_mngr_update_splash_status(display->clk_mngr,
				false);

splash_disabled:
	(void)sde_power_resource_enable(display->phandle,
			display->cont_splash_client, false);
	display->is_cont_splash_enabled = false;
	return rc;
}

/**
 * dsi_display_splash_res_cleanup() - cleanup for continuous splash
 * @display:    Pointer to dsi display
 * Returns:     Zero on success
 */
int dsi_display_splash_res_cleanup(struct  dsi_display *display)
{
	int rc = 0;

	if (!display->is_cont_splash_enabled)
		return 0;

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
	if (rc)
		pr_err("[%s] failed to disable DSI link clocks, rc=%d\n",
		       display->name, rc);

	rc = sde_power_resource_enable(display->phandle,
			display->cont_splash_client, false);
	if (rc)
		pr_err("failed to remove vote on gdsc for continuous splash, rc=%d\n",
				rc);

	display->is_cont_splash_enabled = false;
	/* Update splash status for clock manager */
	dsi_display_clk_mngr_update_splash_status(display->clk_mngr,
				display->is_cont_splash_enabled);

	return rc;
}

/**
 * dsi_display_bind - bind dsi device with controlling device
 * @dev:        Pointer to base of platform device
 * @master:     Pointer to container of drm device
 * @data:       Pointer to private data
 * Returns:     Zero on success
 */
static int dsi_display_bind(struct device *dev,
		struct device *master,
		void *data)
{
	struct dsi_display_ctrl *display_ctrl;
	struct drm_device *drm;
	struct dsi_display *display;
	struct dsi_clk_info info;
	struct clk_ctrl_cb clk_cb;
	struct msm_drm_private *priv;
	void *handle = NULL;
	struct platform_device *pdev = to_platform_device(dev);
	char *client1 = "dsi_clk_client";
	char *client2 = "mdp_event_client";
	char dsi_client_name[DSI_CLIENT_NAME_SIZE];
	int i, rc = 0;

	if (!dev || !pdev || !master) {
		pr_err("invalid param(s), dev %pK, pdev %pK, master %pK\n",
				dev, pdev, master);
		return -EINVAL;
	}

	drm = dev_get_drvdata(master);
	display = platform_get_drvdata(pdev);
	if (!drm || !display) {
		pr_err("invalid param(s), drm %pK, display %pK\n",
				drm, display);
		return -EINVAL;
	}
	priv = drm->dev_private;

	mutex_lock(&display->display_lock);

	rc = dsi_display_debugfs_init(display);
	if (rc) {
		pr_err("[%s] debugfs init failed, rc=%d\n", display->name, rc);
		goto error;
	}

	memset(&info, 0x0, sizeof(info));

	for (i = 0; i < display->ctrl_count; i++) {
		display_ctrl = &display->ctrl[i];
		rc = dsi_ctrl_drv_init(display_ctrl->ctrl, display->root);
		if (rc) {
			pr_err("[%s] failed to initialize ctrl[%d], rc=%d\n",
			       display->name, i, rc);
			goto error_ctrl_deinit;
		}
		display_ctrl->ctrl->horiz_index = i;

		rc = dsi_phy_drv_init(display_ctrl->phy);
		if (rc) {
			pr_err("[%s] Failed to initialize phy[%d], rc=%d\n",
				display->name, i, rc);
			(void)dsi_ctrl_drv_deinit(display_ctrl->ctrl);
			goto error_ctrl_deinit;
		}

		memcpy(&info.c_clks[i], &display_ctrl->ctrl->clk_info.core_clks,
			sizeof(struct dsi_core_clk_info));
		memcpy(&info.l_clks[i], &display_ctrl->ctrl->clk_info.link_clks,
			sizeof(struct dsi_link_clk_info));
		info.c_clks[i].phandle = &priv->phandle;
		info.bus_handle[i] =
			display_ctrl->ctrl->axi_bus_info.bus_handle;
		info.ctrl_index[i] = display_ctrl->ctrl->cell_index;
		snprintf(dsi_client_name, DSI_CLIENT_NAME_SIZE,
						"dsi_core_client%u", i);
		info.c_clks[i].dsi_core_client = sde_power_client_create(
				info.c_clks[i].phandle, dsi_client_name);
		if (IS_ERR_OR_NULL(info.c_clks[i].dsi_core_client)) {
			pr_err("[%s] client creation failed for ctrl[%d]",
					dsi_client_name, i);
			goto error_ctrl_deinit;
		}
	}

	display->phandle = &priv->phandle;
	info.pre_clkoff_cb = dsi_pre_clkoff_cb;
	info.pre_clkon_cb = dsi_pre_clkon_cb;
	info.post_clkoff_cb = dsi_post_clkoff_cb;
	info.post_clkon_cb = dsi_post_clkon_cb;
	info.priv_data = display;
	info.master_ndx = display->clk_master_idx;
	info.dsi_ctrl_count = display->ctrl_count;
	snprintf(info.name, MAX_STRING_LEN,
			"DSI_MNGR-%s", display->name);

	display->clk_mngr = dsi_display_clk_mngr_register(&info);
	if (IS_ERR_OR_NULL(display->clk_mngr)) {
		rc = PTR_ERR(display->clk_mngr);
		display->clk_mngr = NULL;
		pr_err("dsi clock registration failed, rc = %d\n", rc);
		goto error_ctrl_deinit;
	}

	handle = dsi_register_clk_handle(display->clk_mngr, client1);
	if (IS_ERR_OR_NULL(handle)) {
		rc = PTR_ERR(handle);
		pr_err("failed to register %s client, rc = %d\n",
		       client1, rc);
		goto error_clk_deinit;
	} else {
		display->dsi_clk_handle = handle;
	}

	handle = dsi_register_clk_handle(display->clk_mngr, client2);
	if (IS_ERR_OR_NULL(handle)) {
		rc = PTR_ERR(handle);
		pr_err("failed to register %s client, rc = %d\n",
		       client2, rc);
		goto error_clk_client_deinit;
	} else {
		display->mdp_clk_handle = handle;
	}

	clk_cb.priv = display;
	clk_cb.dsi_clk_cb = dsi_display_clk_ctrl_cb;

	for (i = 0; i < display->ctrl_count; i++) {
		display_ctrl = &display->ctrl[i];

		rc = dsi_ctrl_clk_cb_register(display_ctrl->ctrl, &clk_cb);
		if (rc) {
			pr_err("[%s] failed to register ctrl clk_cb[%d], rc=%d\n",
			       display->name, i, rc);
			goto error_ctrl_deinit;
		}

		rc = dsi_phy_clk_cb_register(display_ctrl->phy, &clk_cb);
		if (rc) {
			pr_err("[%s] failed to register phy clk_cb[%d], rc=%d\n",
			       display->name, i, rc);
			goto error_ctrl_deinit;
		}
	}

	rc = dsi_display_mipi_host_init(display);
	if (rc) {
		pr_err("[%s] failed to initialize mipi host, rc=%d\n",
		       display->name, rc);
		goto error_ctrl_deinit;
	}

	rc = dsi_panel_drv_init(display->panel, &display->host);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			pr_err("[%s] failed to initialize panel driver, rc=%d\n",
			       display->name, rc);
		goto error_host_deinit;
	}

	pr_info("Successfully bind display panel '%s'\n", display->name);
	display->drm_dev = drm;

	/* Initialize resources for continuous splash */
	rc = dsi_display_splash_res_init(display);
	if (rc)
		pr_err("Continuous splash resource init failed, rc=%d\n", rc);

	goto error;

error_host_deinit:
	(void)dsi_display_mipi_host_deinit(display);
error_clk_client_deinit:
	(void)dsi_deregister_clk_handle(display->dsi_clk_handle);
error_clk_deinit:
	(void)dsi_display_clk_mngr_deregister(display->clk_mngr);
error_ctrl_deinit:
	for (i = i - 1; i >= 0; i--) {
		display_ctrl = &display->ctrl[i];
		(void)dsi_phy_drv_deinit(display_ctrl->phy);
		(void)dsi_ctrl_drv_deinit(display_ctrl->ctrl);
	}
	(void)dsi_display_debugfs_deinit(display);
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

/**
 * dsi_display_unbind - unbind dsi from controlling device
 * @dev:        Pointer to base of platform device
 * @master:     Pointer to container of drm device
 * @data:       Pointer to private data
 */
static void dsi_display_unbind(struct device *dev,
		struct device *master, void *data)
{
	struct dsi_display_ctrl *display_ctrl;
	struct dsi_display *display;
	struct platform_device *pdev = to_platform_device(dev);
	int i, rc = 0;

	if (!dev || !pdev) {
		pr_err("invalid param(s)\n");
		return;
	}

	display = platform_get_drvdata(pdev);
	if (!display) {
		pr_err("invalid display\n");
		return;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_panel_drv_deinit(display->panel);
	if (rc)
		pr_err("[%s] failed to deinit panel driver, rc=%d\n",
		       display->name, rc);

	rc = dsi_display_mipi_host_deinit(display);
	if (rc)
		pr_err("[%s] failed to deinit mipi hosts, rc=%d\n",
		       display->name,
		       rc);

	for (i = 0; i < display->ctrl_count; i++) {
		display_ctrl = &display->ctrl[i];

		rc = dsi_phy_drv_deinit(display_ctrl->phy);
		if (rc)
			pr_err("[%s] failed to deinit phy%d driver, rc=%d\n",
			       display->name, i, rc);

		rc = dsi_ctrl_drv_deinit(display_ctrl->ctrl);
		if (rc)
			pr_err("[%s] failed to deinit ctrl%d driver, rc=%d\n",
			       display->name, i, rc);
	}
	(void)dsi_display_debugfs_deinit(display);

	mutex_unlock(&display->display_lock);
}

static const struct component_ops dsi_display_comp_ops = {
	.bind = dsi_display_bind,
	.unbind = dsi_display_unbind,
};

static struct platform_driver dsi_display_driver = {
	.probe = dsi_display_dev_probe,
	.remove = dsi_display_dev_remove,
	.driver = {
		.name = "msm-dsi-display",
		.of_match_table = dsi_display_dt_match,
	},
};

int dsi_display_dev_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct dsi_display *display;
	static bool display_from_cmdline, boot_displays_parsed;
	static bool comp_add_success;
	static struct device_node *primary_np, *secondary_np;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("pdev not found\n");
		return -ENODEV;
	}

	display = devm_kzalloc(&pdev->dev, sizeof(*display), GFP_KERNEL);
	if (!display)
		return -ENOMEM;

	display->name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!display->name)
		display->name = "unknown";

	if (!boot_displays_parsed) {
		boot_displays[DSI_PRIMARY].boot_disp_en = false;
		boot_displays[DSI_SECONDARY].boot_disp_en = false;
		if (dsi_display_parse_boot_display_selection())
			pr_debug("Display Boot param not valid/available\n");

		boot_displays_parsed = true;
	}

	/* use default topology of every mode if not overridden */
	display->cmdline_topology = NO_OVERRIDE;
	display->cmdline_timing = 0;

	if ((!display_from_cmdline) &&
			(boot_displays[DSI_PRIMARY].boot_disp_en)) {
		display->is_active = dsi_display_name_compare(pdev->dev.of_node,
						display->name, DSI_PRIMARY);
		if (display->is_active) {
			if (comp_add_success) {
				(void)_dsi_display_dev_deinit(main_display);
				component_del(&main_display->pdev->dev,
					      &dsi_display_comp_ops);
				mutex_lock(&dsi_display_list_lock);
				list_del(&main_display->list);
				mutex_unlock(&dsi_display_list_lock);
				comp_add_success = false;
				default_active_node = NULL;
				pr_debug("removed the existing comp ops\n");
			}
			/*
			 * Need to add component for
			 * the secondary DSI display
			 * when more than one DSI display
			 * is supported.
			 */
			pr_debug("cmdline primary dsi: %s\n",
						display->name);
			display_from_cmdline = true;
			dsi_display_parse_cmdline_topology(display,
					DSI_PRIMARY);
			primary_np = pdev->dev.of_node;
		}
	}

	if (boot_displays[DSI_SECONDARY].boot_disp_en) {
		if (!secondary_np) {
			if (dsi_display_name_compare(pdev->dev.of_node,
				display->name, DSI_SECONDARY)) {
				pr_debug("cmdline secondary dsi: %s\n",
							display->name);
				secondary_np = pdev->dev.of_node;
				if (primary_np) {
					if (validate_dsi_display_selection()) {
					display->is_active = true;
					dsi_display_parse_cmdline_topology
						(display, DSI_SECONDARY);
					} else {
						boot_displays[DSI_SECONDARY]
							.boot_disp_en = false;
					}
				}
			}
		}
	}
	display->display_type = of_get_property(pdev->dev.of_node,
						"qcom,display-type", NULL);
	if (!display->display_type)
		display->display_type = "unknown";

	mutex_init(&display->display_lock);
	display->pdev = pdev;
	platform_set_drvdata(pdev, display);
	mutex_lock(&dsi_display_list_lock);
	list_add(&display->list, &dsi_display_list);
	mutex_unlock(&dsi_display_list_lock);

	if (!display_from_cmdline)
		display->is_active = of_property_read_bool(pdev->dev.of_node,
						"qcom,dsi-display-active");

	if (display->is_active) {
		main_display = display;
		rc = _dsi_display_dev_init(display);
		if (rc) {
			pr_err("device init failed, rc=%d\n", rc);
			return rc;
		}

		rc = component_add(&pdev->dev, &dsi_display_comp_ops);
		if (rc)
			pr_err("component add failed, rc=%d\n", rc);

		comp_add_success = true;
		pr_debug("Component_add success: %s\n", display->name);
		if (!display_from_cmdline)
			default_active_node = pdev->dev.of_node;
	}
	return rc;
}

int dsi_display_dev_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct dsi_display *display;
	struct dsi_display *pos, *tmp;

	if (!pdev) {
		pr_err("Invalid device\n");
		return -EINVAL;
	}

	display = platform_get_drvdata(pdev);

	(void)_dsi_display_dev_deinit(display);

	mutex_lock(&dsi_display_list_lock);
	list_for_each_entry_safe(pos, tmp, &dsi_display_list, list) {
		if (pos == display) {
			list_del(&display->list);
			break;
		}
	}
	mutex_unlock(&dsi_display_list_lock);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, display);
	return rc;
}

int dsi_display_get_num_of_displays(void)
{
	int count = 0;
	struct dsi_display *display;

	mutex_lock(&dsi_display_list_lock);

	list_for_each_entry(display, &dsi_display_list, list) {
		count++;
	}

	mutex_unlock(&dsi_display_list_lock);
	return count;
}

int dsi_display_get_active_displays(void **display_array, u32 max_display_count)
{
	struct dsi_display *pos;
	int i = 0;

	if (!display_array || !max_display_count) {
		if (!display_array)
			pr_err("invalid params\n");
		return 0;
	}

	mutex_lock(&dsi_display_list_lock);

	list_for_each_entry(pos, &dsi_display_list, list) {
		if (i >= max_display_count) {
			pr_err("capping display count to %d\n", i);
			break;
		}
		if (pos->is_active)
			display_array[i++] = pos;
	}

	mutex_unlock(&dsi_display_list_lock);
	return i;
}

struct dsi_display *dsi_display_get_display_by_name(const char *name)
{
	struct dsi_display *display = NULL, *pos;

	mutex_lock(&dsi_display_list_lock);

	list_for_each_entry(pos, &dsi_display_list, list) {
		if (!strcmp(name, pos->name))
			display = pos;
	}

	mutex_unlock(&dsi_display_list_lock);

	return display;
}

void dsi_display_set_active_state(struct dsi_display *display, bool is_active)
{
	mutex_lock(&display->display_lock);
	display->is_active = is_active;
	mutex_unlock(&display->display_lock);
}

int dsi_display_drm_bridge_init(struct dsi_display *display,
		struct drm_encoder *enc)
{
	int rc = 0;
	struct dsi_bridge *bridge;
	struct msm_drm_private *priv = NULL;

	if (!display || !display->drm_dev || !enc) {
		pr_err("invalid param(s)\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	priv = display->drm_dev->dev_private;

	if (!priv) {
		pr_err("Private data is not present\n");
		rc = -EINVAL;
		goto error;
	}

	if (display->bridge) {
		pr_err("display is already initialize\n");
		goto error;
	}

	bridge = dsi_drm_bridge_init(display, display->drm_dev, enc);
	if (IS_ERR_OR_NULL(bridge)) {
		rc = PTR_ERR(bridge);
		pr_err("[%s] brige init failed, %d\n", display->name, rc);
		goto error;
	}

	display->bridge = bridge;
	priv->bridges[priv->num_bridges++] = &bridge->base;

error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_drm_bridge_deinit(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	dsi_drm_bridge_cleanup(display->bridge);
	display->bridge = NULL;

	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_get_info(struct msm_display_info *info, void *disp)
{
	struct dsi_display *display;
	struct dsi_panel_phy_props phy_props;
	int i, rc;

	if (!info || !disp) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	display = disp;
	if (!display->panel) {
		pr_err("invalid display panel\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	rc = dsi_panel_get_phy_props(display->panel, &phy_props);
	if (rc) {
		pr_err("[%s] failed to get panel phy props, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	memset(info, 0, sizeof(struct msm_display_info));
	info->intf_type = DRM_MODE_CONNECTOR_DSI;
	info->num_of_h_tiles = display->ctrl_count;
	for (i = 0; i < info->num_of_h_tiles; i++)
		info->h_tile_instance[i] = display->ctrl[i].ctrl->cell_index;

	info->is_connected = true;
	info->is_primary = true;
	info->width_mm = phy_props.panel_width_mm;
	info->height_mm = phy_props.panel_height_mm;
	info->max_width = 1920;
	info->max_height = 1080;

	switch (display->panel->panel_mode) {
	case DSI_OP_VIDEO_MODE:
		info->capabilities |= MSM_DISPLAY_CAP_VID_MODE;
		break;
	case DSI_OP_CMD_MODE:
		info->capabilities |= MSM_DISPLAY_CAP_CMD_MODE;
		info->is_te_using_watchdog_timer =
			display->panel->te_using_watchdog_timer;
		break;
	default:
		pr_err("unknwown dsi panel mode %d\n",
				display->panel->panel_mode);
		break;
	}

	if (display->panel->esd_config.esd_enabled)
		info->capabilities |= MSM_DISPLAY_ESD_ENABLED;

error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_get_mode_count(struct dsi_display *display,
			u32 *count)
{
	struct dsi_dfps_capabilities dfps_caps;
	int num_dfps_rates, rc = 0;

	if (!display || !display->panel) {
		pr_err("invalid display:%d panel:%d\n", display != NULL,
				display ? display->panel != NULL : 0);
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	*count = display->panel->num_timing_nodes;

	rc = dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
	if (rc) {
		pr_err("[%s] failed to get dfps caps from panel\n",
				display->name);
		goto done;
	}

	num_dfps_rates = !dfps_caps.dfps_support ? 1 :
			dfps_caps.max_refresh_rate -
			dfps_caps.min_refresh_rate + 1;

	/* Inflate num_of_modes by fps in dfps */
	*count = display->panel->num_timing_nodes * num_dfps_rates;

done:
	mutex_unlock(&display->display_lock);

	return 0;
}

void dsi_display_put_mode(struct dsi_display *display,
	struct dsi_display_mode *mode)
{
	dsi_panel_put_mode(mode);
}

int dsi_display_get_modes(struct dsi_display *display,
			  struct dsi_display_mode *modes)
{
	struct dsi_dfps_capabilities dfps_caps;
	u32 num_dfps_rates, panel_mode_count;
	u32 mode_idx, array_idx = 0;
	int i, rc = 0;

	if (!display || !modes) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
	if (rc) {
		pr_err("[%s] failed to get dfps caps from panel\n",
				display->name);
		goto error;
	}

	num_dfps_rates = !dfps_caps.dfps_support ? 1 :
			dfps_caps.max_refresh_rate -
			dfps_caps.min_refresh_rate + 1;

	panel_mode_count = display->panel->num_timing_nodes;

	for (mode_idx = 0; mode_idx < panel_mode_count; mode_idx++) {
		struct dsi_display_mode panel_mode;
		int topology_override = NO_OVERRIDE;

		if (display->cmdline_timing == mode_idx)
			topology_override = display->cmdline_topology;

		memset(&panel_mode, 0, sizeof(panel_mode));

		rc = dsi_panel_get_mode(display->panel, mode_idx,
						&panel_mode, topology_override);
		if (rc) {
			pr_err("[%s] failed to get mode idx %d from panel\n",
				   display->name, mode_idx);
			goto error;
		}

		if (display->ctrl_count > 1) { /* TODO: remove if */
			panel_mode.timing.h_active *= display->ctrl_count;
			panel_mode.timing.h_front_porch *= display->ctrl_count;
			panel_mode.timing.h_sync_width *= display->ctrl_count;
			panel_mode.timing.h_back_porch *= display->ctrl_count;
			panel_mode.timing.h_skew *= display->ctrl_count;
			panel_mode.pixel_clk_khz *= display->ctrl_count;
		}

		for (i = 0; i < num_dfps_rates; i++) {
			struct dsi_display_mode *sub_mode = &modes[array_idx];
			u32 curr_refresh_rate;

			if (!sub_mode) {
				pr_err("invalid mode data\n");
				return -EFAULT;
			}

			memcpy(sub_mode, &panel_mode, sizeof(panel_mode));

			if (dfps_caps.dfps_support) {
				curr_refresh_rate =
					sub_mode->timing.refresh_rate;
				sub_mode->timing.refresh_rate =
					dfps_caps.min_refresh_rate +
					(i % num_dfps_rates);

				dsi_display_get_dfps_timing(display,
					sub_mode, curr_refresh_rate);

				sub_mode->pixel_clk_khz =
					(DSI_H_TOTAL(&sub_mode->timing) *
					DSI_V_TOTAL(&sub_mode->timing) *
					sub_mode->timing.refresh_rate) / 1000;
			}
			array_idx++;
		}
	}

error:
	mutex_unlock(&display->display_lock);
	return rc;
}

/**
 * dsi_display_validate_mode_vrr() - Validate if varaible refresh case.
 * @display:     DSI display handle.
 * @cur_dsi_mode:   Current DSI mode.
 * @mode:        Mode value structure to be validated.
 *               MSM_MODE_FLAG_SEAMLESS_VRR flag is set if there
 *               is change in fps but vactive and hactive are same.
 * Return: error code.
 */
int dsi_display_validate_mode_vrr(struct dsi_display *display,
			struct dsi_display_mode *cur_dsi_mode,
			struct dsi_display_mode *mode)
{
	int rc = 0;
	struct dsi_display_mode adj_mode, cur_mode;
	struct dsi_dfps_capabilities dfps_caps;
	u32 curr_refresh_rate;

	if (!display || !mode) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (!display->panel || !display->panel->cur_mode) {
		pr_debug("Current panel mode not set\n");
		return rc;
	}

	mutex_lock(&display->display_lock);

	adj_mode = *mode;
	cur_mode = *cur_dsi_mode;

	if ((cur_mode.timing.refresh_rate != adj_mode.timing.refresh_rate) &&
		(cur_mode.timing.v_active == adj_mode.timing.v_active) &&
		(cur_mode.timing.h_active == adj_mode.timing.h_active)) {

		curr_refresh_rate = cur_mode.timing.refresh_rate;
		rc = dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
		if (rc) {
			pr_err("[%s] failed to get dfps caps from panel\n",
					display->name);
			goto error;
		}

		cur_mode.timing.refresh_rate =
			adj_mode.timing.refresh_rate;

		rc = dsi_display_get_dfps_timing(display,
			&cur_mode, curr_refresh_rate);
		if (rc) {
			pr_err("[%s] seamless vrr not possible rc=%d\n",
			display->name, rc);
			goto error;
		}
		switch (dfps_caps.type) {
		/*
		 * Ignore any round off factors in porch calculation.
		 * Worse case is set to 5.
		 */
		case DSI_DFPS_IMMEDIATE_VFP:
			if (abs(DSI_V_TOTAL(&cur_mode.timing) -
				DSI_V_TOTAL(&adj_mode.timing)) > 5)
				pr_err("Mismatch vfp fps:%d new:%d given:%d\n",
				adj_mode.timing.refresh_rate,
				cur_mode.timing.v_front_porch,
				adj_mode.timing.v_front_porch);
			break;

		case DSI_DFPS_IMMEDIATE_HFP:
			if (abs(DSI_H_TOTAL(&cur_mode.timing) -
				DSI_H_TOTAL(&adj_mode.timing)) > 5)
				pr_err("Mismatch hfp fps:%d new:%d given:%d\n",
				adj_mode.timing.refresh_rate,
				cur_mode.timing.h_front_porch,
				adj_mode.timing.h_front_porch);
			break;

		default:
			pr_err("Unsupported DFPS mode %d\n",
				dfps_caps.type);
			rc = -ENOTSUPP;
		}

		pr_debug("Mode switch is seamless variable refresh\n");
		mode->dsi_mode_flags |= DSI_MODE_FLAG_VRR;
		SDE_EVT32(curr_refresh_rate, adj_mode.timing.refresh_rate,
				cur_mode.timing.h_front_porch,
				adj_mode.timing.h_front_porch);
	}

error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_validate_mode(struct dsi_display *display,
			      struct dsi_display_mode *mode,
			      u32 flags)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;
	struct dsi_display_mode adj_mode;

	if (!display || !mode) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	adj_mode = *mode;
	adjust_timing_by_ctrl_count(display, &adj_mode);

	rc = dsi_panel_validate_mode(display->panel, &adj_mode);
	if (rc) {
		pr_err("[%s] panel mode validation failed, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_validate_timing(ctrl->ctrl, &adj_mode.timing);
		if (rc) {
			pr_err("[%s] ctrl mode validation failed, rc=%d\n",
			       display->name, rc);
			goto error;
		}

		rc = dsi_phy_validate_mode(ctrl->phy, &adj_mode.timing);
		if (rc) {
			pr_err("[%s] phy mode validation failed, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

	if ((flags & DSI_VALIDATE_FLAG_ALLOW_ADJUST) &&
			(mode->dsi_mode_flags & DSI_MODE_FLAG_SEAMLESS)) {
		rc = dsi_display_validate_mode_seamless(display, mode);
		if (rc) {
			pr_err("[%s] seamless not possible rc=%d\n",
				display->name, rc);
			goto error;
		}
	}

error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_set_mode(struct dsi_display *display,
			 struct dsi_display_mode *mode,
			 u32 flags)
{
	int rc = 0;
	struct dsi_display_mode adj_mode;

	if (!display || !mode || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	adj_mode = *mode;
	adjust_timing_by_ctrl_count(display, &adj_mode);

	rc = dsi_display_validate_mode_set(display, &adj_mode, flags);
	if (rc) {
		pr_err("[%s] mode cannot be set\n", display->name);
		goto error;
	}

	rc = dsi_display_set_mode_sub(display, &adj_mode, flags);
	if (rc) {
		pr_err("[%s] failed to set mode\n", display->name);
		goto error;
	}

	if (!display->panel->cur_mode) {
		display->panel->cur_mode =
			kzalloc(sizeof(struct dsi_display_mode), GFP_KERNEL);
		if (!display->panel->cur_mode) {
			rc = -ENOMEM;
			goto error;
		}
	}

	memcpy(display->panel->cur_mode, &adj_mode, sizeof(adj_mode));
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_set_tpg_state(struct dsi_display *display, bool enable)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_set_tpg_state(ctrl->ctrl, enable);
		if (rc) {
			pr_err("[%s] failed to set tpg state for host_%d\n",
			       display->name, i);
			goto error;
		}
	}

	display->is_tpg_enabled = enable;
error:
	return rc;
}

static int dsi_display_pre_switch(struct dsi_display *display)
{
	int rc = 0;

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	rc = dsi_display_ctrl_update(display);
	if (rc) {
		pr_err("[%s] failed to update DSI controller, rc=%d\n",
			   display->name, rc);
		goto error_ctrl_clk_off;
	}

	rc = dsi_display_set_clk_src(display);
	if (rc) {
		pr_err("[%s] failed to set DSI link clock source, rc=%d\n",
			display->name, rc);
		goto error_ctrl_deinit;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_LINK_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI link clocks, rc=%d\n",
			   display->name, rc);
		goto error_ctrl_deinit;
	}

	goto error;

error_ctrl_deinit:
	(void)dsi_display_ctrl_deinit(display);
error_ctrl_clk_off:
	(void)dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
error:
	return rc;
}

static void dsi_display_handle_fifo_underflow(struct work_struct *work)
{
	struct dsi_display *display = NULL;

	display =  container_of(work, struct dsi_display, fifo_underflow_work);
	if (!display)
		return;
	pr_debug("handle DSI FIFO underflow error\n");

	dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);
	dsi_display_soft_reset(display);
	dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
}

static void dsi_display_handle_fifo_overflow(struct work_struct *work)
{
	struct dsi_display *display = NULL;
	struct dsi_display_ctrl *ctrl;
	int i, rc;
	int mask = BIT(20); /* clock lane */
	int (*cb_func)(void *event_usr_ptr,
		uint32_t event_idx, uint32_t instance_idx,
		uint32_t data0, uint32_t data1,
		uint32_t data2, uint32_t data3);
	void *data;
	u32 version = 0;

	display =  container_of(work, struct dsi_display, fifo_overflow_work);
	if (!display || !display->panel ||
			(display->panel->panel_mode != DSI_OP_VIDEO_MODE))
		return;

	pr_debug("handle DSI FIFO overflow error\n");
	dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);

	/*
	 * below recovery sequence is not applicable to
	 * hw version 2.0.0, 2.1.0 and 2.2.0, so return early.
	 */
	ctrl = &display->ctrl[display->clk_master_idx];
	version = dsi_ctrl_get_hw_version(ctrl->ctrl);
	if (!version || (version < 0x20020001))
		goto end;

	/* reset ctrl and lanes */
	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_reset(ctrl->ctrl, mask);
		rc = dsi_phy_lane_reset(ctrl->phy);
	}

	/* wait for display line count to be in active area */
	ctrl = &display->ctrl[display->clk_master_idx];
	if (ctrl->ctrl->recovery_cb.event_cb) {
		cb_func = ctrl->ctrl->recovery_cb.event_cb;
		data = ctrl->ctrl->recovery_cb.event_usr_ptr;
		rc = cb_func(data, SDE_CONN_EVENT_VID_FIFO_OVERFLOW,
				display->clk_master_idx, 0, 0, 0, 0);
		if (rc < 0) {
			pr_debug("sde callback failed\n");
			goto end;
		}
	}

	/* Enable Video mode for DSI controller */
	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		dsi_ctrl_vid_engine_en(ctrl->ctrl, true);
	}
	/*
	 * Add sufficient delay to make sure
	 * pixel transmission has started
	 */
	udelay(200);
end:
	dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
}

static void dsi_display_handle_lp_rx_timeout(struct work_struct *work)
{
	struct dsi_display *display = NULL;
	struct dsi_display_ctrl *ctrl;
	int i, rc;
	int mask = (BIT(20) | (0xF << 16)); /* clock lane and 4 data lane */
	int (*cb_func)(void *event_usr_ptr,
		uint32_t event_idx, uint32_t instance_idx,
		uint32_t data0, uint32_t data1,
		uint32_t data2, uint32_t data3);
	void *data;
	u32 version = 0;

	display =  container_of(work, struct dsi_display, fifo_overflow_work);
	if (!display || (display->panel->panel_mode != DSI_OP_VIDEO_MODE))
		return;
	pr_debug("handle DSI LP RX Timeout error\n");

	dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);

	/*
	 * below recovery sequence is not applicable to
	 * hw version 2.0.0, 2.1.0 and 2.2.0, so return early.
	 */
	ctrl = &display->ctrl[display->clk_master_idx];
	version = dsi_ctrl_get_hw_version(ctrl->ctrl);
	if (!version || (version < 0x20020001))
		goto end;

	/* reset ctrl and lanes */
	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_reset(ctrl->ctrl, mask);
		rc = dsi_phy_lane_reset(ctrl->phy);
	}

	ctrl = &display->ctrl[display->clk_master_idx];
	if (ctrl->ctrl->recovery_cb.event_cb) {
		cb_func = ctrl->ctrl->recovery_cb.event_cb;
		data = ctrl->ctrl->recovery_cb.event_usr_ptr;
		rc = cb_func(data, SDE_CONN_EVENT_VID_FIFO_OVERFLOW,
				display->clk_master_idx, 0, 0, 0, 0);
		if (rc < 0) {
			pr_debug("Target is in suspend/shutdown\n");
			goto end;
		}
	}

	/* Enable Video mode for DSI controller */
	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		dsi_ctrl_vid_engine_en(ctrl->ctrl, true);
	}

	/*
	 * Add sufficient delay to make sure
	 * pixel transmission as started
	 */
	udelay(200);
end:
	dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
}

static int dsi_display_cb_error_handler(void *data,
		uint32_t event_idx, uint32_t instance_idx,
		uint32_t data0, uint32_t data1,
		uint32_t data2, uint32_t data3)
{
	struct dsi_display *display =  data;

	if (!display)
		return -EINVAL;

	switch (event_idx) {
	case DSI_FIFO_UNDERFLOW:
		queue_work(display->err_workq, &display->fifo_underflow_work);
		break;
	case DSI_FIFO_OVERFLOW:
		queue_work(display->err_workq, &display->fifo_overflow_work);
		break;
	case DSI_LP_Rx_TIMEOUT:
		queue_work(display->err_workq, &display->lp_rx_timeout_work);
		break;
	default:
		pr_warn("unhandled error interrupt: %d\n", event_idx);
		break;
	}

	return 0;
}

static void dsi_display_register_error_handler(struct dsi_display *display)
{
	int i = 0;
	struct dsi_display_ctrl *ctrl;
	struct dsi_event_cb_info event_info;

	if (!display)
		return;

	display->err_workq = create_singlethread_workqueue("dsi_err_workq");
	if (!display->err_workq) {
		pr_err("failed to create dsi workq!\n");
		return;
	}

	INIT_WORK(&display->fifo_underflow_work,
				dsi_display_handle_fifo_underflow);
	INIT_WORK(&display->fifo_overflow_work,
				dsi_display_handle_fifo_overflow);
	INIT_WORK(&display->lp_rx_timeout_work,
				dsi_display_handle_lp_rx_timeout);

	memset(&event_info, 0, sizeof(event_info));

	event_info.event_cb = dsi_display_cb_error_handler;
	event_info.event_usr_ptr = display;

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		ctrl->ctrl->irq_info.irq_err_cb = event_info;
	}
}

static void dsi_display_unregister_error_handler(struct dsi_display *display)
{
	int i = 0;
	struct dsi_display_ctrl *ctrl;

	if (!display)
		return;

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		memset(&ctrl->ctrl->irq_info.irq_err_cb,
				0, sizeof(struct dsi_event_cb_info));
	}

	if (display->err_workq)
		destroy_workqueue(display->err_workq);
}

int dsi_display_prepare(struct dsi_display *display)
{
	int rc = 0;
	struct dsi_display_mode *mode;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (!display->panel->cur_mode) {
		pr_err("no valid mode set for the display");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	mode = display->panel->cur_mode;

	if (mode->dsi_mode_flags & DSI_MODE_FLAG_DMS) {
		/* update dsi ctrl for new mode */
		rc = dsi_display_pre_switch(display);
		if (rc)
			pr_err("[%s] panel pre-prepare-res-switch failed, rc=%d\n",
				   display->name, rc);

		goto error;
	}

	if (!display->is_cont_splash_enabled) {
		/*
		 * For continuous splash usecase we skip panel
		 * pre prepare since the regulator vote is already
		 * taken care in splash resource init
		 */
		rc = dsi_panel_pre_prepare(display->panel);
		if (rc) {
			pr_err("[%s] panel pre-prepare failed, rc=%d\n",
					display->name, rc);
			goto error;
		}
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto error_panel_post_unprep;
	}

	/*
	 * If ULPS during suspend feature is enabled, then DSI PHY was
	 * left on during suspend. In this case, we do not need to reset/init
	 * PHY. This would have already been done when the CORE clocks are
	 * turned on. However, if cont splash is disabled, the first time DSI
	 * is powered on, phy init needs to be done unconditionally.
	 */
	if (!display->panel->ulps_suspend_enabled || !display->ulps_enabled) {
		rc = dsi_display_phy_sw_reset(display);
		if (rc) {
			pr_err("[%s] failed to reset phy, rc=%d\n",
				display->name, rc);
			goto error_ctrl_clk_off;
		}

		rc = dsi_display_phy_enable(display);
		if (rc) {
			pr_err("[%s] failed to enable DSI PHY, rc=%d\n",
			       display->name, rc);
			goto error_ctrl_clk_off;
		}
	}

	rc = dsi_display_set_clk_src(display);
	if (rc) {
		pr_err("[%s] failed to set DSI link clock source, rc=%d\n",
			display->name, rc);
		goto error_phy_disable;
	}

	rc = dsi_display_ctrl_init(display);
	if (rc) {
		pr_err("[%s] failed to setup DSI controller, rc=%d\n",
		       display->name, rc);
		goto error_phy_disable;
	}
	/* Set up DSI ERROR event callback */
	dsi_display_register_error_handler(display);

	rc = dsi_display_ctrl_host_enable(display);
	if (rc) {
		pr_err("[%s] failed to enable DSI host, rc=%d\n",
		       display->name, rc);
		goto error_ctrl_deinit;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_LINK_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI link clocks, rc=%d\n",
		       display->name, rc);
		goto error_host_engine_off;
	}

	rc = dsi_display_soft_reset(display);
	if (rc) {
		pr_err("[%s] failed soft reset, rc=%d\n", display->name, rc);
		goto error_ctrl_link_off;
	}

	if (!display->is_cont_splash_enabled) {
		/*
		 * For continuous splash usecase we skip panel
		 * prepare since the pnael is already in
		 * active state and panel on commands are not needed
		 */
		rc = dsi_panel_prepare(display->panel);
		if (rc) {
			pr_err("[%s] panel prepare failed, rc=%d\n",
					display->name, rc);
			goto error_ctrl_link_off;
		}
	}
	goto error;

error_ctrl_link_off:
	(void)dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_LINK_CLK, DSI_CLK_OFF);
error_host_engine_off:
	(void)dsi_display_ctrl_host_disable(display);
error_ctrl_deinit:
	(void)dsi_display_ctrl_deinit(display);
error_phy_disable:
	(void)dsi_display_phy_disable(display);
error_ctrl_clk_off:
	(void)dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
error_panel_post_unprep:
	(void)dsi_panel_post_unprepare(display->panel);
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

static int dsi_display_calc_ctrl_roi(const struct dsi_display *display,
		const struct dsi_display_ctrl *ctrl,
		const struct msm_roi_list *req_rois,
		struct dsi_rect *out_roi)
{
	const struct dsi_rect *bounds = &ctrl->ctrl->mode_bounds;
	struct dsi_display_mode *cur_mode;
	struct msm_roi_caps *roi_caps;
	struct dsi_rect req_roi = { 0 };
	int rc = 0;

	cur_mode = display->panel->cur_mode;
	if (!cur_mode)
		return 0;

	roi_caps = &cur_mode->priv_info->roi_caps;
	if (req_rois->num_rects > roi_caps->num_roi) {
		pr_err("request for %d rois greater than max %d\n",
				req_rois->num_rects,
				roi_caps->num_roi);
		rc = -EINVAL;
		goto exit;
	}

	/**
	 * if no rois, user wants to reset back to full resolution
	 * note: h_active is already divided by ctrl_count
	 */
	if (!req_rois->num_rects) {
		*out_roi = *bounds;
		goto exit;
	}

	/* intersect with the bounds */
	req_roi.x = req_rois->roi[0].x1;
	req_roi.y = req_rois->roi[0].y1;
	req_roi.w = req_rois->roi[0].x2 - req_rois->roi[0].x1;
	req_roi.h = req_rois->roi[0].y2 - req_rois->roi[0].y1;
	dsi_rect_intersect(&req_roi, bounds, out_roi);

exit:
	/* adjust the ctrl origin to be top left within the ctrl */
	out_roi->x = out_roi->x - bounds->x;

	pr_debug("ctrl%d:%d: req (%d,%d,%d,%d) bnd (%d,%d,%d,%d) out (%d,%d,%d,%d)\n",
			ctrl->dsi_ctrl_idx, ctrl->ctrl->cell_index,
			req_roi.x, req_roi.y, req_roi.w, req_roi.h,
			bounds->x, bounds->y, bounds->w, bounds->h,
			out_roi->x, out_roi->y, out_roi->w, out_roi->h);

	return rc;
}

static int dsi_display_set_roi(struct dsi_display *display,
		struct msm_roi_list *rois)
{
	struct dsi_display_mode *cur_mode;
	struct msm_roi_caps *roi_caps;
	int rc = 0;
	int i;

	if (!display || !rois || !display->panel)
		return -EINVAL;

	cur_mode = display->panel->cur_mode;
	if (!cur_mode)
		return 0;

	roi_caps = &cur_mode->priv_info->roi_caps;
	if (!roi_caps->enabled)
		return 0;

	for (i = 0; i < display->ctrl_count; i++) {
		struct dsi_display_ctrl *ctrl = &display->ctrl[i];
		struct dsi_rect ctrl_roi;
		bool changed = false;

		rc = dsi_display_calc_ctrl_roi(display, ctrl, rois, &ctrl_roi);
		if (rc) {
			pr_err("dsi_display_calc_ctrl_roi failed rc %d\n", rc);
			return rc;
		}

		rc = dsi_ctrl_set_roi(ctrl->ctrl, &ctrl_roi, &changed);
		if (rc) {
			pr_err("dsi_ctrl_set_roi failed rc %d\n", rc);
			return rc;
		}

		if (!changed)
			continue;

		/* send the new roi to the panel via dcs commands */
		rc = dsi_panel_send_roi_dcs(display->panel, i, &ctrl_roi);
		if (rc) {
			pr_err("dsi_panel_set_roi failed rc %d\n", rc);
			return rc;
		}

		/* re-program the ctrl with the timing based on the new roi */
		rc = dsi_ctrl_setup(ctrl->ctrl);
		if (rc) {
			pr_err("dsi_ctrl_setup failed rc %d\n", rc);
			return rc;
		}
	}

	return rc;
}

int dsi_display_pre_kickoff(struct dsi_display *display,
		struct msm_display_kickoff_params *params)
{
	int rc = 0;

	/* check and setup MISR */
	if (display->misr_enable)
		_dsi_display_setup_misr(display);

	rc = dsi_display_set_roi(display, params->rois);

	return rc;
}

int dsi_display_config_ctrl_for_cont_splash(struct dsi_display *display)
{
	int rc = 0;

	if (!display || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (!display->panel->cur_mode) {
		pr_err("no valid mode set for the display");
		return -EINVAL;
	}

	if (!display->is_cont_splash_enabled)
		return 0;

	if (display->config.panel_mode == DSI_OP_VIDEO_MODE) {
		rc = dsi_display_vid_engine_enable(display);
		if (rc) {
			pr_err("[%s]failed to enable DSI video engine, rc=%d\n",
			       display->name, rc);
			goto error_out;
		}
	} else if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_cmd_engine_enable(display);
		if (rc) {
			pr_err("[%s]failed to enable DSI cmd engine, rc=%d\n",
			       display->name, rc);
			goto error_out;
		}
	} else {
		pr_err("[%s] Invalid configuration\n", display->name);
		rc = -EINVAL;
	}

error_out:
	return rc;
}

int dsi_display_enable(struct dsi_display *display)
{
	int rc = 0;
	struct dsi_display_mode *mode;

	if (!display || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (!display->panel->cur_mode) {
		pr_err("no valid mode set for the display");
		return -EINVAL;
	}

	/* Engine states and panel states are populated during splash
	 * resource init and hence we return early
	 */
	if (display->is_cont_splash_enabled) {

		dsi_display_config_ctrl_for_cont_splash(display);

		rc = dsi_display_splash_res_cleanup(display);
		if (rc) {
			pr_err("Continuous splash res cleanup failed, rc=%d\n",
				rc);
			return -EINVAL;
		}

		display->panel->panel_initialized = true;
		pr_debug("cont splash enabled, display enable not required\n");
		return 0;
	}

	mutex_lock(&display->display_lock);

	mode = display->panel->cur_mode;

	if (mode->dsi_mode_flags & DSI_MODE_FLAG_DMS) {
		rc = dsi_panel_post_switch(display->panel);
		if (rc) {
			pr_err("[%s] failed to switch DSI panel mode, rc=%d\n",
				   display->name, rc);
			goto error;
		}
	} else {
		rc = dsi_panel_enable(display->panel);
		if (rc) {
			pr_err("[%s] failed to enable DSI panel, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

	if (mode->priv_info->dsc_enabled) {
		mode->priv_info->dsc.pic_width *= display->ctrl_count;
		rc = dsi_panel_update_pps(display->panel);
		if (rc) {
			pr_err("[%s] panel pps cmd update failed, rc=%d\n",
				display->name, rc);
			goto error;
		}
	}

	if (mode->dsi_mode_flags & DSI_MODE_FLAG_DMS) {
		rc = dsi_panel_switch(display->panel);
		if (rc)
			pr_err("[%s] failed to switch DSI panel mode, rc=%d\n",
				   display->name, rc);

		goto error;
	}

	if (display->config.panel_mode == DSI_OP_VIDEO_MODE) {
		rc = dsi_display_vid_engine_enable(display);
		if (rc) {
			pr_err("[%s]failed to enable DSI video engine, rc=%d\n",
			       display->name, rc);
			goto error_disable_panel;
		}
	} else if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_cmd_engine_enable(display);
		if (rc) {
			pr_err("[%s]failed to enable DSI cmd engine, rc=%d\n",
			       display->name, rc);
			goto error_disable_panel;
		}
	} else {
		pr_err("[%s] Invalid configuration\n", display->name);
		rc = -EINVAL;
		goto error_disable_panel;
	}

	goto error;

error_disable_panel:
	(void)dsi_panel_disable(display->panel);
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_post_enable(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_panel_post_enable(display->panel);
	if (rc)
		pr_err("[%s] panel post-enable failed, rc=%d\n",
		       display->name, rc);

	/* remove the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE)
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);

	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_pre_disable(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE)
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);

	rc = dsi_panel_pre_disable(display->panel);
	if (rc)
		pr_err("[%s] panel pre-disable failed, rc=%d\n",
		       display->name, rc);

	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_disable(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_display_wake_up(display);
	if (rc)
		pr_err("[%s] display wake up failed, rc=%d\n",
		       display->name, rc);

	if (display->config.panel_mode == DSI_OP_VIDEO_MODE) {
		rc = dsi_display_vid_engine_disable(display);
		if (rc)
			pr_err("[%s]failed to disable DSI vid engine, rc=%d\n",
			       display->name, rc);
	} else if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_cmd_engine_disable(display);
		if (rc)
			pr_err("[%s]failed to disable DSI cmd engine, rc=%d\n",
			       display->name, rc);
	} else {
		pr_err("[%s] Invalid configuration\n", display->name);
		rc = -EINVAL;
	}

	rc = dsi_panel_disable(display->panel);
	if (rc)
		pr_err("[%s] failed to disable DSI panel, rc=%d\n",
		       display->name, rc);

	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_update_pps(char *pps_cmd, void *disp)
{
	struct dsi_display *display;

	if (pps_cmd == NULL || disp == NULL) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	display = disp;
	mutex_lock(&display->display_lock);
	memcpy(display->panel->dsc_pps_cmd, pps_cmd, DSI_CMD_PPS_SIZE);
	mutex_unlock(&display->display_lock);

	return 0;
}

int dsi_display_unprepare(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_display_wake_up(display);
	if (rc)
		pr_err("[%s] display wake up failed, rc=%d\n",
		       display->name, rc);

	rc = dsi_panel_unprepare(display->panel);
	if (rc)
		pr_err("[%s] panel unprepare failed, rc=%d\n",
		       display->name, rc);

	rc = dsi_display_ctrl_host_disable(display);
	if (rc)
		pr_err("[%s] failed to disable DSI host, rc=%d\n",
		       display->name, rc);

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_LINK_CLK, DSI_CLK_OFF);
	if (rc)
		pr_err("[%s] failed to disable Link clocks, rc=%d\n",
		       display->name, rc);

	/* Free up DSI ERROR event callback */
	dsi_display_unregister_error_handler(display);

	rc = dsi_display_ctrl_deinit(display);
	if (rc)
		pr_err("[%s] failed to deinit controller, rc=%d\n",
		       display->name, rc);

	if (!display->panel->ulps_suspend_enabled) {
		rc = dsi_display_phy_disable(display);
		if (rc)
			pr_err("[%s] failed to disable DSI PHY, rc=%d\n",
			       display->name, rc);
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
	if (rc)
		pr_err("[%s] failed to disable DSI clocks, rc=%d\n",
		       display->name, rc);

	rc = dsi_panel_post_unprepare(display->panel);
	if (rc)
		pr_err("[%s] panel post-unprepare failed, rc=%d\n",
		       display->name, rc);

	mutex_unlock(&display->display_lock);
	return rc;
}

static int __init dsi_display_register(void)
{
	dsi_phy_drv_register();
	dsi_ctrl_drv_register();
	return platform_driver_register(&dsi_display_driver);
}

static void __exit dsi_display_unregister(void)
{
	platform_driver_unregister(&dsi_display_driver);
	dsi_ctrl_drv_unregister();
	dsi_phy_drv_unregister();
}
module_param_string(dsi_display0, dsi_display_primary, MAX_CMDLINE_PARAM_LEN,
								0600);
MODULE_PARM_DESC(dsi_display0,
	"msm_drm.dsi_display0=<display node>:<configX> where <display node> is 'primary dsi display node name' and <configX> where x represents index in the topology list");
module_param_string(dsi_display1, dsi_display_secondary, MAX_CMDLINE_PARAM_LEN,
								0600);
MODULE_PARM_DESC(dsi_display1,
	"msm_drm.dsi_display1=<display node>:<configX> where <display node> is 'secondary dsi display node name' and <configX> where x represents index in the topology list");
module_init(dsi_display_register);
module_exit(dsi_display_unregister);
