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

#define pr_fmt(fmt)	"msm-dsi-display:[%s] " fmt, __func__

#include <linux/list.h>
#include <linux/of.h>

#include "msm_drv.h"
#include "sde_kms.h"
#include "dsi_display.h"
#include "dsi_panel.h"
#include "dsi_ctrl.h"
#include "dsi_ctrl_hw.h"
#include "dsi_drm.h"
#include "dba_bridge.h"

#define to_dsi_display(x) container_of(x, struct dsi_display, host)
#define DSI_DBA_CLIENT_NAME "dsi"

static DEFINE_MUTEX(dsi_display_list_lock);
static LIST_HEAD(dsi_display_list);

static const struct of_device_id dsi_display_dt_match[] = {
	{.compatible = "qcom,dsi-display"},
	{}
};

static struct dsi_display *main_display;

static ssize_t debugfs_dump_info_read(struct file *file,
				      char __user *buff,
				      size_t count,
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

	for (i = 0; i < display->panel_count; i++)
		len += snprintf(buf + len, (SZ_4K - len),
			"\tPanel_%d = %s\n", i, display->panel[i]->name);

	len += snprintf(buf + len, (SZ_4K - len),
			"\tClock master = %s\n",
			display->ctrl[display->clk_master_idx].ctrl->name);

	if (copy_to_user(buff, buf, len)) {
		kfree(buf);
		return -EFAULT;
	}

	*ppos += len;

	kfree(buf);
	return len;
}


static const struct file_operations dump_info_fops = {
	.open = simple_open,
	.read = debugfs_dump_info_read,
};

static int dsi_display_debugfs_init(struct dsi_display *display)
{
	int rc = 0;
	struct dentry *dir, *dump_file;

	dir = debugfs_create_dir(display->name, NULL);
	if (IS_ERR_OR_NULL(dir)) {
		rc = PTR_ERR(dir);
		pr_err("[%s] debugfs create dir failed, rc = %d\n",
		       display->name, rc);
		goto error;
	}

	dump_file = debugfs_create_file("dump_info",
					0444,
					dir,
					display,
					&dump_info_fops);
	if (IS_ERR_OR_NULL(dump_file)) {
		rc = PTR_ERR(dump_file);
		pr_err("[%s] debugfs create file failed, rc=%d\n",
		       display->name, rc);
		goto error_remove_dir;
	}

	display->root = dir;
	return rc;
error_remove_dir:
	debugfs_remove(dir);
error:
	return rc;
}

static int dsi_dipslay_debugfs_deinit(struct dsi_display *display)
{
	debugfs_remove(display->root);

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
		(void)dsi_ctrl_set_power_state(ctrl->ctrl, DSI_CTRL_POWER_OFF);
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

		rc = dsi_ctrl_set_power_state(ctrl->ctrl, DSI_CTRL_POWER_OFF);
		if (rc) {
			pr_err("[%s] Failed to power off, rc=%d\n",
			       ctrl->ctrl->name, rc);
			goto error;
		}
	}
error:
	return rc;
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

static int dsi_display_ctrl_core_clk_on(struct dsi_display *display)
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

	rc = dsi_ctrl_set_power_state(m_ctrl->ctrl, DSI_CTRL_POWER_CORE_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to turn on clocks, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	/* Turn on rest of the controllers */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_power_state(ctrl->ctrl,
					      DSI_CTRL_POWER_CORE_CLK_ON);
		if (rc) {
			pr_err("[%s] failed to turn on clock, rc=%d\n",
			       display->name, rc);
			goto error_disable_master;
		}
	}
	return rc;
error_disable_master:
	(void)dsi_ctrl_set_power_state(m_ctrl->ctrl, DSI_CTRL_POWER_VREG_ON);
error:
	return rc;
}

static int dsi_display_ctrl_link_clk_on(struct dsi_display *display)
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
		goto error;
	}

	rc = dsi_ctrl_set_power_state(m_ctrl->ctrl, DSI_CTRL_POWER_LINK_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to turn on clocks, rc=%d\n",
		       display->name, rc);
		goto error;
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
			goto error_disable_master;
		}

		rc = dsi_ctrl_set_power_state(ctrl->ctrl,
					      DSI_CTRL_POWER_LINK_CLK_ON);
		if (rc) {
			pr_err("[%s] failed to turn on clock, rc=%d\n",
			       display->name, rc);
			goto error_disable_master;
		}
	}
	return rc;
error_disable_master:
	(void)dsi_ctrl_set_power_state(m_ctrl->ctrl,
				       DSI_CTRL_POWER_CORE_CLK_ON);
error:
	return rc;
}

static int dsi_display_ctrl_core_clk_off(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	/*
	 * In case of split DSI usecases, clock for slave DSI controllers should
	 * be disabled first before disabling clock for master controller. Slave
	 * controllers in the clock context refer to controller which source
	 * clock from another controller.
	 */

	m_ctrl = &display->ctrl[display->clk_master_idx];

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_power_state(ctrl->ctrl,
					      DSI_CTRL_POWER_VREG_ON);
		if (rc) {
			pr_err("[%s] failed to turn off clock, rc=%d\n",
			       display->name, rc);
		}
	}

	rc = dsi_ctrl_set_power_state(m_ctrl->ctrl, DSI_CTRL_POWER_VREG_ON);
	if (rc)
		pr_err("[%s] failed to turn off clocks, rc=%d\n",
		       display->name, rc);

	return rc;
}

static int dsi_display_ctrl_link_clk_off(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	/*
	 * In case of split DSI usecases, clock for slave DSI controllers should
	 * be disabled first before disabling clock for master controller. Slave
	 * controllers in the clock context refer to controller which source
	 * clock from another controller.
	 */

	m_ctrl = &display->ctrl[display->clk_master_idx];

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_power_state(ctrl->ctrl,
					      DSI_CTRL_POWER_CORE_CLK_ON);
		if (rc) {
			pr_err("[%s] failed to turn off clock, rc=%d\n",
			       display->name, rc);
		}
	}
	rc = dsi_ctrl_set_power_state(m_ctrl->ctrl, DSI_CTRL_POWER_CORE_CLK_ON);
	if (rc)
		pr_err("[%s] failed to turn off clocks, rc=%d\n",
		       display->name, rc);
	return rc;
}

static int dsi_display_ctrl_init(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_host_init(ctrl->ctrl);
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

static int dsi_display_cmd_engine_enable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	if (display->cmd_engine_refcount > 0) {
		display->cmd_engine_refcount++;
		return 0;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_ctrl_set_cmd_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_ON);
	if (rc) {
		pr_err("[%s] failed to enable cmd engine, rc=%d\n",
		       display->name, rc);
		goto error;
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
	return rc;
error_disable_master:
	(void)dsi_ctrl_set_cmd_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_OFF);
error:
	return rc;
}

static int dsi_display_cmd_engine_disable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	if (display->cmd_engine_refcount == 0) {
		pr_err("[%s] Invalid refcount\n", display->name);
		return 0;
	} else if (display->cmd_engine_refcount > 1) {
		display->cmd_engine_refcount--;
		return 0;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];
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
	return rc;
}

static int dsi_display_ctrl_host_enable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

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
			    true);
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
				    true);
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
		   DSI_CTRL_CMD_DEFER_TRIGGER | DSI_CTRL_CMD_FIFO_STORE);
	flags = (DSI_CTRL_CMD_BROADCAST | DSI_CTRL_CMD_DEFER_TRIGGER |
		 DSI_CTRL_CMD_FIFO_STORE);

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

		rc = dsi_ctrl_cmd_tx_trigger(ctrl->ctrl,
			DSI_CTRL_CMD_BROADCAST);
		if (rc) {
			pr_err("[%s] cmd trigger failed, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

	rc = dsi_ctrl_cmd_tx_trigger(m_ctrl->ctrl,
				(DSI_CTRL_CMD_BROADCAST_MASTER |
				 DSI_CTRL_CMD_BROADCAST));
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

	int rc = 0;

	if (!host || !msg) {
		pr_err("Invalid params\n");
		return 0;
	}

	rc = dsi_display_wake_up(display);
	if (rc) {
		pr_err("[%s] failed to wake up display, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	rc = dsi_display_cmd_engine_enable(display);
	if (rc) {
		pr_err("[%s] failed to enable cmd engine, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	if (display->ctrl_count > 1) {
		rc = dsi_display_broadcast_cmd(display, msg);
		if (rc) {
			pr_err("[%s] cmd broadcast failed, rc=%d\n",
			       display->name, rc);
			goto error_disable_cmd_engine;
		}
	} else {
		rc = dsi_ctrl_cmd_transfer(display->ctrl[0].ctrl, msg,
					  DSI_CTRL_CMD_FIFO_STORE);
		if (rc) {
			pr_err("[%s] cmd transfer failed, rc=%d\n",
			       display->name, rc);
			goto error_disable_cmd_engine;
		}
	}
error_disable_cmd_engine:
	(void)dsi_display_cmd_engine_disable(display);
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
		pr_err("failed to get src_byte_clk, rc=%d\n", rc);
		goto error;
	}

	src->pixel_clk = devm_clk_get(&display->pdev->dev, "src_pixel_clk");
	if (IS_ERR_OR_NULL(src->pixel_clk)) {
		rc = PTR_ERR(src->pixel_clk);
		pr_err("failed to get src_pixel_clk, rc=%d\n", rc);
		goto error;
	}

	mux->byte_clk = devm_clk_get(&display->pdev->dev, "mux_byte_clk");
	if (IS_ERR_OR_NULL(mux->byte_clk)) {
		rc = PTR_ERR(mux->byte_clk);
		pr_err("failed to get mux_byte_clk, rc=%d\n", rc);
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
		pr_err("failed to get mux_pixel_clk, rc=%d\n", rc);
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

static int dsi_display_parse_lane_map(struct dsi_display *display)
{
	int rc = 0;

	display->lane_map.physical_lane0 = DSI_LOGICAL_LANE_0;
	display->lane_map.physical_lane1 = DSI_LOGICAL_LANE_1;
	display->lane_map.physical_lane2 = DSI_LOGICAL_LANE_2;
	display->lane_map.physical_lane3 = DSI_LOGICAL_LANE_3;
	return rc;
}

static int dsi_display_parse_dt(struct dsi_display *display)
{
	int rc = 0;
	int i, size;
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

	if (of_get_property(display->pdev->dev.of_node, "qcom,dsi-panel",
			&size)) {
		display->panel_count = size / sizeof(int);
		display->panel_of = devm_kzalloc(&display->pdev->dev,
			sizeof(struct device_node *) * display->panel_count,
			GFP_KERNEL);
		if (!display->panel_of) {
			SDE_ERROR("out of memory for panel_of\n");
			rc = -ENOMEM;
			goto error;
		}
		display->panel = devm_kzalloc(&display->pdev->dev,
			sizeof(struct dsi_panel *) * display->panel_count,
			GFP_KERNEL);
		if (!display->panel) {
			SDE_ERROR("out of memory for panel\n");
			rc = -ENOMEM;
			goto error;
		}
		for (i = 0; i < display->panel_count; i++) {
			display->panel_of[i] =
				of_parse_phandle(display->pdev->dev.of_node,
				"qcom,dsi-panel", i);
			if (!display->panel_of[i]) {
				SDE_ERROR("of_parse dsi-panel failed\n");
				rc = -ENODEV;
				goto error;
			}
		}
	} else {
		SDE_ERROR("No qcom,dsi-panel of node\n");
		rc = -ENODEV;
		goto error;
	}

	if (of_get_property(display->pdev->dev.of_node, "qcom,bridge-index",
			&size)) {
		if (size / sizeof(int) != display->panel_count) {
			SDE_ERROR("size=%lu is different than count=%u\n",
				size / sizeof(int), display->panel_count);
			rc = -EINVAL;
			goto error;
		}
		display->bridge_idx = devm_kzalloc(&display->pdev->dev,
			sizeof(u32) * display->panel_count, GFP_KERNEL);
		if (!display->bridge_idx) {
			SDE_ERROR("out of memory for bridge_idx\n");
			rc = -ENOMEM;
			goto error;
		}
		for (i = 0; i < display->panel_count; i++) {
			rc = of_property_read_u32_index(
				display->pdev->dev.of_node,
				"qcom,bridge-index", i,
				&(display->bridge_idx[i]));
			if (rc) {
				SDE_ERROR(
					"read bridge-index error,i=%d rc=%d\n",
					i, rc);
				rc = -ENODEV;
				goto error;
			}
		}
	}

	rc = dsi_display_parse_lane_map(display);
	if (rc) {
		pr_err("Lane map not found, rc=%d\n", rc);
		goto error;
	}
error:
	if (rc) {
		if (display->panel_of)
			for (i = 0; i < display->panel_count; i++)
				if (display->panel_of[i])
					of_node_put(display->panel_of[i]);
		devm_kfree(&display->pdev->dev, display->panel_of);
		devm_kfree(&display->pdev->dev, display->panel);
		devm_kfree(&display->pdev->dev, display->bridge_idx);
		display->panel_count = 0;
	}
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

	for (i = 0; i < display->panel_count; i++) {
		display->panel[i] = dsi_panel_get(&display->pdev->dev,
					display->panel_of[i]);
		if (IS_ERR_OR_NULL(display->panel)) {
			rc = PTR_ERR(display->panel);
			pr_err("failed to get panel, rc=%d\n", rc);
			display->panel[i] = NULL;
			goto error_ctrl_put;
		}
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

	for (i = 0; i < display->panel_count; i++)
		dsi_panel_put(display->panel[i]);

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		dsi_phy_put(ctrl->phy);
		dsi_ctrl_put(ctrl->ctrl);
	}

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

	if (!display || !tgt) {
		pr_err("Invalid params\n");
		return false;
	}

	cur = &display->panel[0]->mode;

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

	if (cur->timing.refresh_rate == tgt->timing.refresh_rate) {
		pr_debug("timing.refresh_rate identical %d %d\n",
				cur->timing.refresh_rate,
				tgt->timing.refresh_rate);
		return false;
	}

	if (cur->pixel_clk_khz != tgt->pixel_clk_khz)
		pr_debug("pixel_clk_khz differs %d %d\n",
				cur->pixel_clk_khz, tgt->pixel_clk_khz);

	if (cur->panel_mode != tgt->panel_mode) {
		pr_debug("panel_mode differs %d %d\n",
				cur->panel_mode, tgt->panel_mode);
		return false;
	}

	if (cur->flags != tgt->flags)
		pr_debug("flags differs %d %d\n", cur->flags, tgt->flags);

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
	int i;

	if (!display || !dsi_mode) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}
	timing = &dsi_mode->timing;

	dsi_panel_get_dfps_caps(display->panel[0], &dfps_caps);
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

	panel_mode = &display->panel[0]->mode;
	memcpy(panel_mode, dsi_mode, sizeof(*panel_mode));

error:
	return rc;
}

static int dsi_display_dfps_calc_front_porch(
		u64 clk_hz,
		u32 new_fps,
		u32 a_total,
		u32 b_total,
		u32 b_fp,
		u32 *b_fp_out)
{
	s32 b_fp_new;

	if (!b_fp_out) {
		pr_err("Invalid params");
		return -EINVAL;
	}

	if (!a_total || !new_fps) {
		pr_err("Invalid pixel total or new fps in mode request\n");
		return -EINVAL;
	}

	/**
	 * Keep clock, other porches constant, use new fps, calc front porch
	 * clk = (hor * ver * fps)
	 * hfront = clk / (vtotal * fps)) - hactive - hback - hsync
	 */
	b_fp_new = (clk_hz / (a_total * new_fps)) - (b_total - b_fp);

	pr_debug("clk %llu fps %u a %u b %u b_fp %u new_fp %d\n",
			clk_hz, new_fps, a_total, b_total, b_fp, b_fp_new);

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

static int dsi_display_get_dfps_timing(struct dsi_display *display,
				       struct dsi_display_mode *adj_mode)
{
	struct dsi_dfps_capabilities dfps_caps;
	struct dsi_display_mode per_ctrl_mode;
	struct dsi_mode_info *timing;
	struct dsi_ctrl *m_ctrl;
	u64 clk_hz;

	int rc = 0;

	if (!display || !adj_mode) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}
	m_ctrl = display->ctrl[display->clk_master_idx].ctrl;

	/* Only check the first panel */
	dsi_panel_get_dfps_caps(display->panel[0], &dfps_caps);
	if (!dfps_caps.dfps_support) {
		pr_err("dfps not supported by panel\n");
		return -EINVAL;
	}

	per_ctrl_mode = *adj_mode;
	adjust_timing_by_ctrl_count(display, &per_ctrl_mode);

	if (!dsi_display_is_seamless_dfps_possible(display,
			&per_ctrl_mode, dfps_caps.type)) {
		pr_err("seamless dynamic fps not supported for mode\n");
		return -EINVAL;
	}

	/* TODO: Remove this direct reference to the dsi_ctrl */
	clk_hz = m_ctrl->clk_info.link_clks.pixel_clk_rate;
	timing = &per_ctrl_mode.timing;

	switch (dfps_caps.type) {
	case DSI_DFPS_IMMEDIATE_VFP:
		rc = dsi_display_dfps_calc_front_porch(
				clk_hz,
				timing->refresh_rate,
				DSI_H_TOTAL(timing),
				DSI_V_TOTAL(timing),
				timing->v_front_porch,
				&adj_mode->timing.v_front_porch);
		break;

	case DSI_DFPS_IMMEDIATE_HFP:
		rc = dsi_display_dfps_calc_front_porch(
				clk_hz,
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
	rc = dsi_display_get_dfps_timing(display, adj_mode);
	if (rc) {
		pr_debug("Dynamic FPS not supported for seamless\n");
	} else {
		pr_debug("Mode switch is seamless Dynamic FPS\n");
		adj_mode->flags |= DSI_MODE_FLAG_DFPS |
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

	rc = dsi_panel_get_host_cfg_for_mode(display->panel[0],
					     mode,
					     &display->config);
	if (rc) {
		pr_err("[%s] failed to get host config for mode, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	memcpy(&display->config.lane_map, &display->lane_map,
	       sizeof(display->lane_map));

	if (mode->flags & DSI_MODE_FLAG_DFPS) {
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
				mode->flags);
		if (rc) {
			pr_err("[%s] failed to update ctrl config, rc=%d\n",
			       display->name, rc);
			goto error;
		}

	}
error:
	return rc;
}

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

	if (!pdev || !pdev->dev.of_node) {
		pr_err("pdev not found\n");
		return -ENODEV;
	}

	display = devm_kzalloc(&pdev->dev, sizeof(*display), GFP_KERNEL);
	if (!display)
		return -ENOMEM;

	display->name = of_get_property(pdev->dev.of_node, "label", NULL);

	display->is_active = of_property_read_bool(pdev->dev.of_node,
						"qcom,dsi-display-active");

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
	if (display->is_active)
		main_display = display;
	return rc;
}

int dsi_display_dev_remove(struct platform_device *pdev)
{
	int rc = 0, i;
	struct dsi_display *display;
	struct dsi_display *pos, *tmp;

	if (!pdev) {
		pr_err("Invalid device\n");
		return -EINVAL;
	}

	display = platform_get_drvdata(pdev);

	mutex_lock(&dsi_display_list_lock);
	list_for_each_entry_safe(pos, tmp, &dsi_display_list, list) {
		if (pos == display) {
			list_del(&display->list);
			break;
		}
	}
	mutex_unlock(&dsi_display_list_lock);

	platform_set_drvdata(pdev, NULL);
	if (display->panel_of)
		for (i = 0; i < display->panel_count; i++)
			if (display->panel_of[i])
				of_node_put(display->panel_of[i]);
	devm_kfree(&pdev->dev, display->panel_of);
	devm_kfree(&pdev->dev, display->panel);
	devm_kfree(&pdev->dev, display->bridge_idx);
	devm_kfree(&pdev->dev, display);
	return rc;
}

u32 dsi_display_get_num_of_displays(void)
{
	u32 count = 0;
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

int dsi_display_dev_init(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("Invalid params\n");
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

int dsi_display_dev_deinit(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("Invalid params\n");
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

int dsi_display_bind(struct dsi_display *display, struct drm_device *dev)
{
	int rc = 0;
	int i, j;
	struct dsi_display_ctrl *display_ctrl;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_display_debugfs_init(display);
	if (rc) {
		pr_err("[%s]Debugfs init failed, rc=%d\n", display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		display_ctrl = &display->ctrl[i];

		rc = dsi_ctrl_drv_init(display_ctrl->ctrl, display->root);
		if (rc) {
			pr_err("[%s] Failed to initialize ctrl[%d], rc=%d\n",
			       display->name, i, rc);
			goto error_ctrl_deinit;
		}

		rc = dsi_phy_drv_init(display_ctrl->phy);
		if (rc) {
			pr_err("[%s] Failed to initialize phy[%d], rc=%d\n",
				display->name, i, rc);
			(void)dsi_ctrl_drv_deinit(display_ctrl->ctrl);
			goto error_ctrl_deinit;
		}
	}

	rc = dsi_display_mipi_host_init(display);
	if (rc) {
		pr_err("[%s] Failed to initialize mipi host, rc=%d\n",
		       display->name, rc);
		goto error_ctrl_deinit;
	}

	for (j = 0; j < display->panel_count; j++) {
		rc = dsi_panel_drv_init(display->panel[j], &display->host);
		if (rc) {
			if (rc != -EPROBE_DEFER)
				SDE_ERROR(
				"[%s]Failed to init panel driver, rc=%d\n",
				display->name, rc);
			goto error_panel_deinit;
		}
	}

	rc = dsi_panel_get_mode_count(display->panel[0],
					&display->num_of_modes);
	if (rc) {
		pr_err("[%s] Failed to get mode count, rc=%d\n",
		       display->name, rc);
		goto error_panel_deinit;
	}

	display->drm_dev = dev;
	goto error;

error_panel_deinit:
	for (j--; j >= 0; j--)
		(void)dsi_panel_drv_deinit(display->panel[j]);
	(void)dsi_display_mipi_host_deinit(display);
error_ctrl_deinit:
	for (i = i - 1; i >= 0; i--) {
		display_ctrl = &display->ctrl[i];
		(void)dsi_phy_drv_deinit(display_ctrl->phy);
		(void)dsi_ctrl_drv_deinit(display_ctrl->ctrl);
	}
	(void)dsi_dipslay_debugfs_deinit(display);
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_unbind(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *display_ctrl;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	for (i = 0; i < display->panel_count; i++) {
		rc = dsi_panel_drv_deinit(display->panel[i]);
		if (rc)
			SDE_ERROR("[%s] failed to deinit panel driver, rc=%d\n",
					display->name, rc);
	}

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

	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_drm_bridge_init(struct dsi_display *display,
		struct drm_encoder *enc)
{
	int rc = 0, i;
	struct dsi_bridge *bridge;
	struct drm_bridge *dba_bridge;
	struct dba_bridge_init init_data;
	struct drm_bridge *precede_bridge;
	struct msm_drm_private *priv = NULL;
	struct dsi_panel *panel;
	u32 *bridge_idx;
	u32 num_of_lanes = 0;

	if (!display || !enc) {
		SDE_ERROR("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	priv = display->drm_dev->dev_private;

	if (!priv) {
		SDE_ERROR("Private data is not present\n");
		rc = -EINVAL;
		goto out;
	}

	if (display->bridge) {
		SDE_ERROR("display is already initialize\n");
		goto out;
	}

	bridge = dsi_drm_bridge_init(display, display->drm_dev, enc);
	if (IS_ERR_OR_NULL(bridge)) {
		rc = PTR_ERR(bridge);
		SDE_ERROR("[%s] brige init failed, %d\n", display->name, rc);
		goto out;
	}

	display->bridge = bridge;
	priv->bridges[priv->num_bridges++] = &bridge->base;
	precede_bridge = &bridge->base;

	if (display->panel_count >= MAX_BRIDGES - 1) {
		SDE_ERROR("too many bridge chips=%d\n", display->panel_count);
		goto error_bridge;
	}

	for (i = 0; i < display->panel_count; i++) {
		panel = display->panel[i];
		if (panel && display->bridge_idx &&
			panel->dba_config.dba_panel) {
			bridge_idx = display->bridge_idx + i;
			num_of_lanes = 0;
			memset(&init_data, 0x00, sizeof(init_data));
			if (panel->host_config.data_lanes & DSI_DATA_LANE_0)
				num_of_lanes++;
			if (panel->host_config.data_lanes & DSI_DATA_LANE_1)
				num_of_lanes++;
			if (panel->host_config.data_lanes & DSI_DATA_LANE_2)
				num_of_lanes++;
			if (panel->host_config.data_lanes & DSI_DATA_LANE_3)
				num_of_lanes++;
			init_data.client_name = DSI_DBA_CLIENT_NAME;
			init_data.chip_name = panel->dba_config.bridge_name;
			init_data.id = *bridge_idx;
			init_data.display = display;
			init_data.hdmi_mode = panel->dba_config.hdmi_mode;
			init_data.num_of_input_lanes = num_of_lanes;
			init_data.precede_bridge = precede_bridge;
			dba_bridge = dba_bridge_init(display->drm_dev, enc,
							&init_data);
			if (IS_ERR_OR_NULL(dba_bridge)) {
				rc = PTR_ERR(dba_bridge);
				SDE_ERROR("[%s:%d] dba brige init failed, %d\n",
					init_data.chip_name, init_data.id, rc);
				goto error_dba_bridge;
			}
			priv->bridges[priv->num_bridges++] = dba_bridge;
			precede_bridge = dba_bridge;
		}
	}

	goto out;

error_dba_bridge:
	for (i = 1; i < MAX_BRIDGES; i++) {
		dba_bridge_cleanup(priv->bridges[i]);
		priv->bridges[i] = NULL;
	}
error_bridge:
	dsi_drm_bridge_cleanup(display->bridge);
	display->bridge = NULL;
	priv->bridges[0] = NULL;
	priv->num_bridges = 0;
out:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_drm_bridge_deinit(struct dsi_display *display)
{
	int rc = 0, i;
	struct msm_drm_private *priv = NULL;

	if (!display) {
		SDE_ERROR("Invalid params\n");
		return -EINVAL;
	}
	priv = display->drm_dev->dev_private;

	if (!priv) {
		SDE_ERROR("Private data is not present\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	for (i = 1; i < MAX_BRIDGES; i++) {
		dba_bridge_cleanup(priv->bridges[i]);
		priv->bridges[i] = NULL;
	}

	dsi_drm_bridge_cleanup(display->bridge);
	display->bridge = NULL;
	priv->bridges[0] = NULL;
	priv->num_bridges = 0;

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

	mutex_lock(&display->display_lock);
	rc = dsi_panel_get_phy_props(display->panel[0], &phy_props);
	if (rc) {
		pr_err("[%s] failed to get panel phy props, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	info->intf_type = DRM_MODE_CONNECTOR_DSI;

	info->num_of_h_tiles = display->ctrl_count;
	for (i = 0; i < info->num_of_h_tiles; i++)
		info->h_tile_instance[i] = display->ctrl[i].ctrl->index;

	info->is_connected = true;
	info->width_mm = phy_props.panel_width_mm;
	info->height_mm = phy_props.panel_height_mm;
	info->max_width = 1920;
	info->max_height = 1080;
	info->compression = MSM_DISPLAY_COMPRESS_NONE;

	switch (display->panel[0]->mode.panel_mode) {
	case DSI_OP_VIDEO_MODE:
		info->capabilities |= MSM_DISPLAY_CAP_VID_MODE;
		break;
	case DSI_OP_CMD_MODE:
		info->capabilities |= MSM_DISPLAY_CAP_CMD_MODE;
		break;
	default:
		pr_err("unknwown dsi panel mode %d\n",
				display->panel[0]->mode.panel_mode);
		break;
	}
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_get_modes(struct dsi_display *display,
			  struct dsi_display_mode *modes,
			  u32 *count)
{
	int rc = 0;
	int i;
	struct dsi_dfps_capabilities dfps_caps;
	int num_dfps_rates;

	if (!display || !count) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_panel_get_dfps_caps(display->panel[0], &dfps_caps);
	if (rc) {
		pr_err("[%s] failed to get dfps caps from panel\n",
				display->name);
		goto error;
	}

	num_dfps_rates = !dfps_caps.dfps_support ? 1 :
			dfps_caps.max_refresh_rate -
			dfps_caps.min_refresh_rate + 1;

	if (!modes) {
		/* Inflate num_of_modes by fps in dfps */
		*count = display->num_of_modes * num_dfps_rates;
		goto error;
	}

	for (i = 0; i < *count; i++) {
		/* Insert the dfps "sub-modes" between main panel modes */
		int panel_mode_idx = i / num_dfps_rates;

		rc = dsi_panel_get_mode(display->panel[0], panel_mode_idx,
					modes);
		if (rc) {
			pr_err("[%s] failed to get mode from panel\n",
			       display->name);
			goto error;
		}

		if (dfps_caps.dfps_support) {
			modes->timing.refresh_rate = dfps_caps.min_refresh_rate
					+ (i % num_dfps_rates);
			modes->pixel_clk_khz = (DSI_H_TOTAL(&modes->timing) *
					DSI_V_TOTAL(&modes->timing) *
					modes->timing.refresh_rate) / 1000;
		}

		if (display->ctrl_count > 1) { /* TODO: remove if */
			modes->timing.h_active *= display->ctrl_count;
			modes->timing.h_front_porch *= display->ctrl_count;
			modes->timing.h_sync_width *= display->ctrl_count;
			modes->timing.h_back_porch *= display->ctrl_count;
			modes->timing.h_skew *= display->ctrl_count;
			modes->pixel_clk_khz *= display->ctrl_count;
		}

		modes++;
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

	rc = dsi_panel_validate_mode(display->panel[0], &adj_mode);
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
			(mode->flags & DSI_MODE_FLAG_SEAMLESS)) {
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

	if (!display || !mode) {
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

int dsi_display_prepare(struct dsi_display *display)
{
	int rc = 0, i, j;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	for (i = 0; i < display->panel_count; i++) {
		rc = dsi_panel_pre_prepare(display->panel[i]);
		if (rc) {
			SDE_ERROR("[%s] panel pre-prepare failed, rc=%d\n",
					display->name, rc);
			goto error_panel_post_unprep;
		}
	}

	rc = dsi_display_ctrl_power_on(display);
	if (rc) {
		pr_err("[%s] failed to power on dsi controllers, rc=%d\n",
		       display->name, rc);
		goto error_panel_post_unprep;
	}

	rc = dsi_display_phy_power_on(display);
	if (rc) {
		pr_err("[%s] failed to power on dsi phy, rc = %d\n",
		       display->name, rc);
		goto error_ctrl_pwr_off;
	}

	rc = dsi_display_ctrl_core_clk_on(display);
	if (rc) {
		pr_err("[%s] failed to enable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto error_phy_pwr_off;
	}

	rc = dsi_display_phy_sw_reset(display);
	if (rc) {
		pr_err("[%s] failed to reset phy, rc=%d\n", display->name, rc);
		goto error_ctrl_clk_off;
	}

	rc = dsi_display_phy_enable(display);
	if (rc) {
		pr_err("[%s] failed to enable DSI PHY, rc=%d\n",
		       display->name, rc);
		goto error_ctrl_clk_off;
	}

	rc = dsi_display_ctrl_init(display);
	if (rc) {
		pr_err("[%s] failed to setup DSI controller, rc=%d\n",
		       display->name, rc);
		goto error_phy_disable;
	}

	rc = dsi_display_ctrl_link_clk_on(display);
	if (rc) {
		pr_err("[%s] failed to enable DSI link clocks, rc=%d\n",
		       display->name, rc);
		goto error_ctrl_deinit;
	}

	rc = dsi_display_ctrl_host_enable(display);
	if (rc) {
		pr_err("[%s] failed to enable DSI host, rc=%d\n",
		       display->name, rc);
		goto error_ctrl_link_off;
	}

	for (j = 0; j < display->panel_count; j++) {
		rc = dsi_panel_prepare(display->panel[j]);
		if (rc) {
			SDE_ERROR("[%s] panel prepare failed, rc=%d\n",
					display->name, rc);
			goto error_panel_unprep;
		}
	}

	goto error;

error_panel_unprep:
	for (j--; j >= 0; j--)
		(void)dsi_panel_unprepare(display->panel[j]);
	(void)dsi_display_ctrl_host_disable(display);
error_ctrl_link_off:
	(void)dsi_display_ctrl_link_clk_off(display);
error_ctrl_deinit:
	(void)dsi_display_ctrl_deinit(display);
error_phy_disable:
	(void)dsi_display_phy_disable(display);
error_ctrl_clk_off:
	(void)dsi_display_ctrl_core_clk_off(display);
error_phy_pwr_off:
	(void)dsi_display_phy_power_off(display);
error_ctrl_pwr_off:
	(void)dsi_display_ctrl_power_off(display);
error_panel_post_unprep:
	for (i--; i >= 0; i--)
		(void)dsi_panel_post_unprepare(display->panel[i]);
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_enable(struct dsi_display *display)
{
	int rc = 0, i;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	for (i = 0; i < display->panel_count; i++) {
		rc = dsi_panel_enable(display->panel[i]);
		if (rc) {
			SDE_ERROR("[%s] failed to enable DSI panel, rc=%d\n",
					display->name, rc);
			goto error_disable_panel;
		}
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
	for (i--; i >= 0; i--)
		(void)dsi_panel_disable(display->panel[i]);
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_post_enable(struct dsi_display *display)
{
	int rc = 0, i;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	for (i = 0; i < display->panel_count; i++) {
		rc = dsi_panel_post_enable(display->panel[i]);
		if (rc)
			SDE_ERROR("[%s] panel post-enable failed, rc=%d\n",
					display->name, rc);
	}

	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_pre_disable(struct dsi_display *display)
{
	int rc = 0, i;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	for (i = 0; i < display->panel_count; i++) {
		rc = dsi_panel_pre_disable(display->panel[i]);
		if (rc)
			SDE_ERROR("[%s] panel pre-disable failed, rc=%d\n",
					display->name, rc);
	}

	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_disable(struct dsi_display *display)
{
	int rc = 0, i;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_display_wake_up(display);
	if (rc)
		pr_err("[%s] display wake up failed, rc=%d\n",
		       display->name, rc);

	for (i = 0; i < display->panel_count; i++) {
		rc = dsi_panel_disable(display->panel[i]);
		if (rc)
			SDE_ERROR("[%s] failed to disable DSI panel, rc=%d\n",
					display->name, rc);
	}

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

	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_unprepare(struct dsi_display *display)
{
	int rc = 0, i;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_display_wake_up(display);
	if (rc)
		pr_err("[%s] display wake up failed, rc=%d\n",
		       display->name, rc);

	for (i = 0; i < display->panel_count; i++) {
		rc = dsi_panel_unprepare(display->panel[i]);
		if (rc)
			SDE_ERROR("[%s] panel unprepare failed, rc=%d\n",
					display->name, rc);
	}

	rc = dsi_display_ctrl_host_disable(display);
	if (rc)
		pr_err("[%s] failed to disable DSI host, rc=%d\n",
		       display->name, rc);

	rc = dsi_display_ctrl_link_clk_off(display);
	if (rc)
		pr_err("[%s] failed to disable Link clocks, rc=%d\n",
		       display->name, rc);

	rc = dsi_display_ctrl_deinit(display);
	if (rc)
		pr_err("[%s] failed to deinit controller, rc=%d\n",
		       display->name, rc);

	rc = dsi_display_phy_disable(display);
	if (rc)
		pr_err("[%s] failed to disable DSI PHY, rc=%d\n",
		       display->name, rc);

	rc = dsi_display_ctrl_core_clk_off(display);
	if (rc)
		pr_err("[%s] failed to disable DSI clocks, rc=%d\n",
		       display->name, rc);

	rc = dsi_display_phy_power_off(display);
	if (rc)
		pr_err("[%s] failed to power off PHY, rc=%d\n",
		       display->name, rc);

	rc = dsi_display_ctrl_power_off(display);
	if (rc)
		pr_err("[%s] failed to power DSI vregs, rc=%d\n",
		       display->name, rc);

	for (i = 0; i < display->panel_count; i++) {
		rc = dsi_panel_post_unprepare(display->panel[i]);
		if (rc)
			pr_err("[%s] panel post-unprepare failed, rc=%d\n",
				display->name, rc);
	}

	mutex_unlock(&display->display_lock);
	return rc;
}

void dsi_display_register(void)
{
	platform_driver_register(&dsi_display_driver);
}

void dsi_display_unregister(void)
{
	platform_driver_unregister(&dsi_display_driver);
}
