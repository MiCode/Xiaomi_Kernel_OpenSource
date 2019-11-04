/*
 * Copyright (c) 2016, 2018-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"dsi-ctrl:[%s] " fmt, __func__

#include <linux/of_device.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/msm-bus.h>
#include <linux/of_irq.h>
#include <video/mipi_display.h>

#include "msm_drv.h"
#include "msm_kms.h"
#include "msm_gpu.h"
#include "dsi_ctrl.h"
#include "dsi_ctrl_hw.h"
#include "dsi_clk_pwr.h"
#include "dsi_catalog.h"

#define DSI_CTRL_DEFAULT_LABEL "MDSS DSI CTRL"

#define DSI_CTRL_TX_TO_MS     200

#define TO_ON_OFF(x) ((x) ? "ON" : "OFF")
/**
 * enum dsi_ctrl_driver_ops - controller driver ops
 */
enum dsi_ctrl_driver_ops {
	DSI_CTRL_OP_POWER_STATE_CHANGE,
	DSI_CTRL_OP_CMD_ENGINE,
	DSI_CTRL_OP_VID_ENGINE,
	DSI_CTRL_OP_HOST_ENGINE,
	DSI_CTRL_OP_CMD_TX,
	DSI_CTRL_OP_ULPS_TOGGLE,
	DSI_CTRL_OP_CLAMP_TOGGLE,
	DSI_CTRL_OP_SET_CLK_SOURCE,
	DSI_CTRL_OP_HOST_INIT,
	DSI_CTRL_OP_TPG,
	DSI_CTRL_OP_PHY_SW_RESET,
	DSI_CTRL_OP_ASYNC_TIMING,
	DSI_CTRL_OP_MAX
};

struct dsi_ctrl_list_item {
	struct dsi_ctrl *ctrl;
	struct list_head list;
};

static LIST_HEAD(dsi_ctrl_list);
static DEFINE_MUTEX(dsi_ctrl_list_lock);

static const enum dsi_ctrl_version dsi_ctrl_v1_4 = DSI_CTRL_VERSION_1_4;
static const enum dsi_ctrl_version dsi_ctrl_v2_0 = DSI_CTRL_VERSION_2_0;

static const struct of_device_id msm_dsi_of_match[] = {
	{
		.compatible = "qcom,dsi-ctrl-hw-v1.4",
		.data = &dsi_ctrl_v1_4,
	},
	{
		.compatible = "qcom,dsi-ctrl-hw-v2.0",
		.data = &dsi_ctrl_v2_0,
	},
	{}
};

static ssize_t debugfs_state_info_read(struct file *file,
				       char __user *buff,
				       size_t count,
				       loff_t *ppos)
{
	struct dsi_ctrl *dsi_ctrl = file->private_data;
	char *buf;
	u32 len = 0;

	if (!dsi_ctrl)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	/* Dump current state */
	len += snprintf((buf + len), (SZ_4K - len), "Current State:\n");
	len += snprintf((buf + len), (SZ_4K - len),
			"\tPOWER_STATUS = %s\n\tCORE_CLOCK = %s\n",
			TO_ON_OFF(dsi_ctrl->current_state.pwr_enabled),
			TO_ON_OFF(dsi_ctrl->current_state.core_clk_enabled));
	len += snprintf((buf + len), (SZ_4K - len),
			"\tLINK_CLOCK = %s\n\tULPS_STATUS = %s\n",
			TO_ON_OFF(dsi_ctrl->current_state.link_clk_enabled),
			TO_ON_OFF(dsi_ctrl->current_state.ulps_enabled));
	len += snprintf((buf + len), (SZ_4K - len),
			"\tCLAMP_STATUS = %s\n\tCTRL_ENGINE = %s\n",
			TO_ON_OFF(dsi_ctrl->current_state.clamp_enabled),
			TO_ON_OFF(dsi_ctrl->current_state.controller_state));
	len += snprintf((buf + len), (SZ_4K - len),
			"\tVIDEO_ENGINE = %s\n\tCOMMAND_ENGINE = %s\n",
			TO_ON_OFF(dsi_ctrl->current_state.vid_engine_state),
			TO_ON_OFF(dsi_ctrl->current_state.cmd_engine_state));

	/* Dump clock information */
	len += snprintf((buf + len), (SZ_4K - len), "\nClock Info:\n");
	len += snprintf((buf + len), (SZ_4K - len),
			"\tBYTE_CLK = %llu, PIXEL_CLK = %llu, ESC_CLK = %llu\n",
			dsi_ctrl->clk_info.link_clks.byte_clk_rate,
			dsi_ctrl->clk_info.link_clks.pixel_clk_rate,
			dsi_ctrl->clk_info.link_clks.esc_clk_rate);

	if (len > count)
		len = count;

	/* TODO: make sure that this does not exceed 4K */
	if (copy_to_user(buff, buf, len)) {
		kfree(buf);
		return -EFAULT;
	}

	*ppos += len;
	kfree(buf);
	return len;
}

static ssize_t debugfs_reg_dump_read(struct file *file,
				     char __user *buff,
				     size_t count,
				     loff_t *ppos)
{
	struct dsi_ctrl *dsi_ctrl = file->private_data;
	char *buf;
	u32 len = 0;

	if (!dsi_ctrl)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (dsi_ctrl->current_state.core_clk_enabled) {
		len = dsi_ctrl->hw.ops.reg_dump_to_buffer(&dsi_ctrl->hw,
							  buf,
							  SZ_4K);
	} else {
		len = snprintf((buf + len), (SZ_4K - len),
			       "Core clocks are not turned on, cannot read\n");
	}

	if (len > count)
		len = count;

	/* TODO: make sure that this does not exceed 4K */
	if (copy_to_user(buff, buf, len)) {
		kfree(buf);
		return -EFAULT;
	}

	*ppos += len;
	kfree(buf);
	return len;
}

static const struct file_operations state_info_fops = {
	.open = simple_open,
	.read = debugfs_state_info_read,
};

static const struct file_operations reg_dump_fops = {
	.open = simple_open,
	.read = debugfs_reg_dump_read,
};

static int dsi_ctrl_debugfs_init(struct dsi_ctrl *dsi_ctrl,
				 struct dentry *parent)
{
	int rc = 0;
	struct dentry *dir, *state_file, *reg_dump;

	dir = debugfs_create_dir(dsi_ctrl->name, parent);
	if (IS_ERR_OR_NULL(dir)) {
		rc = PTR_ERR(dir);
		pr_err("[DSI_%d] debugfs create dir failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error;
	}

	state_file = debugfs_create_file("state_info",
					 0444,
					 dir,
					 dsi_ctrl,
					 &state_info_fops);
	if (IS_ERR_OR_NULL(state_file)) {
		rc = PTR_ERR(state_file);
		pr_err("[DSI_%d] state file failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error_remove_dir;
	}

	reg_dump = debugfs_create_file("reg_dump",
				       0444,
				       dir,
				       dsi_ctrl,
				       &reg_dump_fops);
	if (IS_ERR_OR_NULL(reg_dump)) {
		rc = PTR_ERR(reg_dump);
		pr_err("[DSI_%d] reg dump file failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error_remove_dir;
	}

	dsi_ctrl->debugfs_root = dir;
error_remove_dir:
	debugfs_remove(dir);
error:
	return rc;
}

static int dsi_ctrl_debugfs_deinit(struct dsi_ctrl *dsi_ctrl)
{
	debugfs_remove(dsi_ctrl->debugfs_root);
	return 0;
}

static int dsi_ctrl_check_state(struct dsi_ctrl *dsi_ctrl,
				enum dsi_ctrl_driver_ops op,
				u32 op_state)
{
	int rc = 0;
	struct dsi_ctrl_state_info *state = &dsi_ctrl->current_state;

	switch (op) {
	case DSI_CTRL_OP_POWER_STATE_CHANGE:
		if (state->power_state == op_state) {
			pr_debug("[%d] No change in state, pwr_state=%d\n",
			       dsi_ctrl->index, op_state);
			rc = -EINVAL;
		} else if (state->power_state == DSI_CTRL_POWER_LINK_CLK_ON) {
			if ((state->cmd_engine_state == DSI_CTRL_ENGINE_ON) ||
			    (state->vid_engine_state == DSI_CTRL_ENGINE_ON) ||
			    (state->controller_state == DSI_CTRL_ENGINE_ON)) {
				pr_debug("[%d]State error: op=%d: %d, %d, %d\n",
				       dsi_ctrl->index,
				       op_state,
				       state->cmd_engine_state,
				       state->vid_engine_state,
				       state->controller_state);
				rc = -EINVAL;
			}
		}
		break;
	case DSI_CTRL_OP_CMD_ENGINE:
		if (state->cmd_engine_state == op_state) {
			pr_debug("[%d] No change in state, cmd_state=%d\n",
			       dsi_ctrl->index, op_state);
			rc = -EINVAL;
		} else if ((state->power_state != DSI_CTRL_POWER_LINK_CLK_ON) ||
			   (state->controller_state != DSI_CTRL_ENGINE_ON)) {
			pr_debug("[%d]State error: op=%d: %d, %d\n",
			       dsi_ctrl->index,
			       op,
			       state->power_state,
			       state->controller_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_VID_ENGINE:
		if (state->vid_engine_state == op_state) {
			pr_debug("[%d] No change in state, cmd_state=%d\n",
			       dsi_ctrl->index, op_state);
			rc = -EINVAL;
		} else if ((state->power_state != DSI_CTRL_POWER_LINK_CLK_ON) ||
			   (state->controller_state != DSI_CTRL_ENGINE_ON)) {
			pr_debug("[%d]State error: op=%d: %d, %d\n",
			       dsi_ctrl->index,
			       op,
			       state->power_state,
			       state->controller_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_HOST_ENGINE:
		if (state->controller_state == op_state) {
			pr_debug("[%d] No change in state, ctrl_state=%d\n",
			       dsi_ctrl->index, op_state);
			rc = -EINVAL;
		} else if (state->power_state != DSI_CTRL_POWER_LINK_CLK_ON) {
			pr_debug("[%d]State error (link is off): op=%d:, %d\n",
			       dsi_ctrl->index,
			       op_state,
			       state->power_state);
			rc = -EINVAL;
		} else if ((op_state == DSI_CTRL_ENGINE_OFF) &&
			   ((state->cmd_engine_state != DSI_CTRL_ENGINE_OFF) ||
			    (state->vid_engine_state != DSI_CTRL_ENGINE_OFF))) {
			pr_debug("[%d]State error (eng on): op=%d: %d, %d\n",
				  dsi_ctrl->index,
				  op_state,
				  state->cmd_engine_state,
				  state->vid_engine_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_CMD_TX:
		if ((state->power_state != DSI_CTRL_POWER_LINK_CLK_ON) ||
		    (state->host_initialized != true) ||
		    (state->cmd_engine_state != DSI_CTRL_ENGINE_ON)) {
			pr_debug("[%d]State error: op=%d: %d, %d, %d\n",
			       dsi_ctrl->index,
			       op,
			       state->power_state,
			       state->host_initialized,
			       state->cmd_engine_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_HOST_INIT:
		if (state->host_initialized == op_state) {
			pr_debug("[%d] No change in state, host_init=%d\n",
			       dsi_ctrl->index, op_state);
			rc = -EINVAL;
		} else if (state->power_state != DSI_CTRL_POWER_CORE_CLK_ON) {
			pr_debug("[%d]State error: op=%d: %d\n",
			       dsi_ctrl->index, op, state->power_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_ULPS_TOGGLE:
		if (state->ulps_enabled == op_state) {
			pr_debug("[%d] No change in state, ulps_enabled=%d\n",
			       dsi_ctrl->index, op_state);
			rc = -EINVAL;
		} else if ((state->power_state != DSI_CTRL_POWER_LINK_CLK_ON) ||
			   (state->controller_state != DSI_CTRL_ENGINE_ON)) {
			pr_debug("[%d]State error: op=%d: %d, %d\n",
			       dsi_ctrl->index,
			       op,
			       state->power_state,
			       state->controller_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_CLAMP_TOGGLE:
		if (state->clamp_enabled == op_state) {
			pr_debug("[%d] No change in state, clamp_enabled=%d\n",
			       dsi_ctrl->index, op_state);
			rc = -EINVAL;
		} else if ((state->power_state != DSI_CTRL_POWER_LINK_CLK_ON) ||
			   (state->controller_state != DSI_CTRL_ENGINE_ON)) {
			pr_debug("[%d]State error: op=%d: %d, %d\n",
			       dsi_ctrl->index,
			       op,
			       state->power_state,
			       state->controller_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_SET_CLK_SOURCE:
		if (state->power_state == DSI_CTRL_POWER_LINK_CLK_ON) {
			pr_debug("[%d] State error: op=%d: %d\n",
			       dsi_ctrl->index,
			       op,
			       state->power_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_TPG:
		if (state->tpg_enabled == op_state) {
			pr_debug("[%d] No change in state, tpg_enabled=%d\n",
			       dsi_ctrl->index, op_state);
			rc = -EINVAL;
		} else if ((state->power_state != DSI_CTRL_POWER_LINK_CLK_ON) ||
			   (state->controller_state != DSI_CTRL_ENGINE_ON)) {
			pr_debug("[%d]State error: op=%d: %d, %d\n",
			       dsi_ctrl->index,
			       op,
			       state->power_state,
			       state->controller_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_PHY_SW_RESET:
		if (state->power_state != DSI_CTRL_POWER_CORE_CLK_ON) {
			pr_debug("[%d]State error: op=%d: %d\n",
			       dsi_ctrl->index, op, state->power_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_ASYNC_TIMING:
		if (state->vid_engine_state != op_state) {
			pr_err("[%d] Unexpected engine state vid_state=%d\n",
			       dsi_ctrl->index, op_state);
			rc = -EINVAL;
		}
		break;
	default:
		rc = -ENOTSUPP;
		break;
	}

	return rc;
}

static void dsi_ctrl_update_state(struct dsi_ctrl *dsi_ctrl,
				  enum dsi_ctrl_driver_ops op,
				  u32 op_state)
{
	struct dsi_ctrl_state_info *state = &dsi_ctrl->current_state;

	switch (op) {
	case DSI_CTRL_OP_POWER_STATE_CHANGE:
		state->power_state = op_state;
		if (op_state == DSI_CTRL_POWER_OFF) {
			state->pwr_enabled = false;
			state->core_clk_enabled = false;
			state->link_clk_enabled = false;
		} else if (op_state == DSI_CTRL_POWER_VREG_ON) {
			state->pwr_enabled = true;
			state->core_clk_enabled = false;
			state->link_clk_enabled = false;
		} else if (op_state == DSI_CTRL_POWER_CORE_CLK_ON) {
			state->pwr_enabled = true;
			state->core_clk_enabled = true;
			state->link_clk_enabled = false;
		} else if (op_state == DSI_CTRL_POWER_LINK_CLK_ON) {
			state->pwr_enabled = true;
			state->core_clk_enabled = true;
			state->link_clk_enabled = true;
		}
		break;
	case DSI_CTRL_OP_CMD_ENGINE:
		state->cmd_engine_state = op_state;
		break;
	case DSI_CTRL_OP_VID_ENGINE:
		state->vid_engine_state = op_state;
		break;
	case DSI_CTRL_OP_HOST_ENGINE:
		state->controller_state = op_state;
		break;
	case DSI_CTRL_OP_ULPS_TOGGLE:
		state->ulps_enabled = (op_state == 1) ? true : false;
		break;
	case DSI_CTRL_OP_CLAMP_TOGGLE:
		state->clamp_enabled = (op_state == 1) ? true : false;
		break;
	case DSI_CTRL_OP_SET_CLK_SOURCE:
		state->clk_source_set = (op_state == 1) ? true : false;
		break;
	case DSI_CTRL_OP_HOST_INIT:
		state->host_initialized = (op_state == 1) ? true : false;
		break;
	case DSI_CTRL_OP_TPG:
		state->tpg_enabled = (op_state == 1) ? true : false;
		break;
	case DSI_CTRL_OP_CMD_TX:
	case DSI_CTRL_OP_PHY_SW_RESET:
	default:
		break;
	}
}

static int dsi_ctrl_init_regmap(struct platform_device *pdev,
				struct dsi_ctrl *ctrl)
{
	int rc = 0;
	void __iomem *ptr;

	ptr = msm_ioremap(pdev, "dsi_ctrl", ctrl->name);
	if (IS_ERR(ptr)) {
		rc = PTR_ERR(ptr);
		return rc;
	}

	ctrl->hw.base = ptr;
	pr_debug("[%s] map dsi_ctrl registers to %pK\n", ctrl->name,
		 ctrl->hw.base);

	ptr = msm_ioremap(pdev, "mmss_misc", ctrl->name);
	if (IS_ERR(ptr)) {
		rc = PTR_ERR(ptr);
		return rc;
	}

	ctrl->hw.mmss_misc_base = ptr;
	pr_debug("[%s] map mmss_misc registers to %pK\n", ctrl->name,
		 ctrl->hw.mmss_misc_base);
	return rc;
}

static int dsi_ctrl_clocks_deinit(struct dsi_ctrl *ctrl)
{
	struct dsi_core_clk_info *core = &ctrl->clk_info.core_clks;
	struct dsi_link_clk_info *link = &ctrl->clk_info.link_clks;
	struct dsi_clk_link_set *rcg = &ctrl->clk_info.rcg_clks;

	if (core->mdp_core_clk)
		devm_clk_put(&ctrl->pdev->dev, core->mdp_core_clk);
	if (core->iface_clk)
		devm_clk_put(&ctrl->pdev->dev, core->iface_clk);
	if (core->core_mmss_clk)
		devm_clk_put(&ctrl->pdev->dev, core->core_mmss_clk);
	if (core->bus_clk)
		devm_clk_put(&ctrl->pdev->dev, core->bus_clk);

	memset(core, 0x0, sizeof(*core));

	if (link->byte_clk)
		devm_clk_put(&ctrl->pdev->dev, link->byte_clk);
	if (link->pixel_clk)
		devm_clk_put(&ctrl->pdev->dev, link->pixel_clk);
	if (link->esc_clk)
		devm_clk_put(&ctrl->pdev->dev, link->esc_clk);

	memset(link, 0x0, sizeof(*link));

	if (rcg->byte_clk)
		devm_clk_put(&ctrl->pdev->dev, rcg->byte_clk);
	if (rcg->pixel_clk)
		devm_clk_put(&ctrl->pdev->dev, rcg->pixel_clk);

	memset(rcg, 0x0, sizeof(*rcg));

	return 0;
}

static int dsi_ctrl_clocks_init(struct platform_device *pdev,
				struct dsi_ctrl *ctrl)
{
	int rc = 0;
	struct dsi_core_clk_info *core = &ctrl->clk_info.core_clks;
	struct dsi_link_clk_info *link = &ctrl->clk_info.link_clks;
	struct dsi_clk_link_set *rcg = &ctrl->clk_info.rcg_clks;

	core->mdp_core_clk = devm_clk_get(&pdev->dev, "mdp_core_clk");
	if (IS_ERR(core->mdp_core_clk)) {
		rc = PTR_ERR(core->mdp_core_clk);
		pr_err("failed to get mdp_core_clk, rc=%d\n", rc);
		goto fail;
	}

	core->iface_clk = devm_clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(core->iface_clk)) {
		rc = PTR_ERR(core->iface_clk);
		pr_err("failed to get iface_clk, rc=%d\n", rc);
		goto fail;
	}

	core->core_mmss_clk = devm_clk_get(&pdev->dev, "core_mmss_clk");
	if (IS_ERR(core->core_mmss_clk)) {
		rc = PTR_ERR(core->core_mmss_clk);
		pr_err("failed to get core_mmss_clk, rc=%d\n", rc);
		goto fail;
	}

	core->bus_clk = devm_clk_get(&pdev->dev, "bus_clk");
	if (IS_ERR(core->bus_clk)) {
		rc = PTR_ERR(core->bus_clk);
		pr_err("failed to get bus_clk, rc=%d\n", rc);
		goto fail;
	}

	link->byte_clk = devm_clk_get(&pdev->dev, "byte_clk");
	if (IS_ERR(link->byte_clk)) {
		rc = PTR_ERR(link->byte_clk);
		pr_err("failed to get byte_clk, rc=%d\n", rc);
		goto fail;
	}

	link->pixel_clk = devm_clk_get(&pdev->dev, "pixel_clk");
	if (IS_ERR(link->pixel_clk)) {
		rc = PTR_ERR(link->pixel_clk);
		pr_err("failed to get pixel_clk, rc=%d\n", rc);
		goto fail;
	}

	link->esc_clk = devm_clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(link->esc_clk)) {
		rc = PTR_ERR(link->esc_clk);
		pr_err("failed to get esc_clk, rc=%d\n", rc);
		goto fail;
	}

	rcg->byte_clk = devm_clk_get(&pdev->dev, "byte_clk_rcg");
	if (IS_ERR(rcg->byte_clk)) {
		rc = PTR_ERR(rcg->byte_clk);
		pr_err("failed to get byte_clk_rcg, rc=%d\n", rc);
		goto fail;
	}

	rcg->pixel_clk = devm_clk_get(&pdev->dev, "pixel_clk_rcg");
	if (IS_ERR(rcg->pixel_clk)) {
		rc = PTR_ERR(rcg->pixel_clk);
		pr_err("failed to get pixel_clk_rcg, rc=%d\n", rc);
		goto fail;
	}

	return 0;
fail:
	dsi_ctrl_clocks_deinit(ctrl);
	return rc;
}

static int dsi_ctrl_supplies_deinit(struct dsi_ctrl *ctrl)
{
	int i = 0;
	int rc = 0;
	struct dsi_regulator_info *regs;

	regs = &ctrl->pwr_info.digital;
	for (i = 0; i < regs->count; i++) {
		if (!regs->vregs[i].vreg)
			pr_err("vreg is NULL, should not reach here\n");
		else
			devm_regulator_put(regs->vregs[i].vreg);
	}

	regs = &ctrl->pwr_info.host_pwr;
	for (i = 0; i < regs->count; i++) {
		if (!regs->vregs[i].vreg)
			pr_err("vreg is NULL, should not reach here\n");
		else
			devm_regulator_put(regs->vregs[i].vreg);
	}

	if (!ctrl->pwr_info.host_pwr.vregs) {
		devm_kfree(&ctrl->pdev->dev, ctrl->pwr_info.host_pwr.vregs);
		ctrl->pwr_info.host_pwr.vregs = NULL;
		ctrl->pwr_info.host_pwr.count = 0;
	}

	if (!ctrl->pwr_info.digital.vregs) {
		devm_kfree(&ctrl->pdev->dev, ctrl->pwr_info.digital.vregs);
		ctrl->pwr_info.digital.vregs = NULL;
		ctrl->pwr_info.digital.count = 0;
	}

	return rc;
}

static int dsi_ctrl_supplies_init(struct platform_device *pdev,
				  struct dsi_ctrl *ctrl)
{
	int rc = 0;
	int i = 0;
	struct dsi_regulator_info *regs;
	struct regulator *vreg = NULL;

	rc = dsi_clk_pwr_get_dt_vreg_data(&pdev->dev,
					  &ctrl->pwr_info.digital,
					  "qcom,core-supply-entries");
	if (rc) {
		pr_err("failed to get digital supply, rc = %d\n", rc);
		goto error;
	}

	rc = dsi_clk_pwr_get_dt_vreg_data(&pdev->dev,
					  &ctrl->pwr_info.host_pwr,
					  "qcom,ctrl-supply-entries");
	if (rc) {
		pr_err("failed to get host power supplies, rc = %d\n", rc);
		goto error_digital;
	}

	regs = &ctrl->pwr_info.digital;
	for (i = 0; i < regs->count; i++) {
		vreg = devm_regulator_get(&pdev->dev, regs->vregs[i].vreg_name);
		if (IS_ERR(vreg)) {
			pr_err("failed to get %s regulator\n",
			       regs->vregs[i].vreg_name);
			rc = PTR_ERR(vreg);
			goto error_host_pwr;
		}
		regs->vregs[i].vreg = vreg;
	}

	regs = &ctrl->pwr_info.host_pwr;
	for (i = 0; i < regs->count; i++) {
		vreg = devm_regulator_get(&pdev->dev, regs->vregs[i].vreg_name);
		if (IS_ERR(vreg)) {
			pr_err("failed to get %s regulator\n",
			       regs->vregs[i].vreg_name);
			for (--i; i >= 0; i--)
				devm_regulator_put(regs->vregs[i].vreg);
			rc = PTR_ERR(vreg);
			goto error_digital_put;
		}
		regs->vregs[i].vreg = vreg;
	}

	return rc;

error_digital_put:
	regs = &ctrl->pwr_info.digital;
	for (i = 0; i < regs->count; i++)
		devm_regulator_put(regs->vregs[i].vreg);
error_host_pwr:
	devm_kfree(&pdev->dev, ctrl->pwr_info.host_pwr.vregs);
	ctrl->pwr_info.host_pwr.vregs = NULL;
	ctrl->pwr_info.host_pwr.count = 0;
error_digital:
	devm_kfree(&pdev->dev, ctrl->pwr_info.digital.vregs);
	ctrl->pwr_info.digital.vregs = NULL;
	ctrl->pwr_info.digital.count = 0;
error:
	return rc;
}

static int dsi_ctrl_axi_bus_client_init(struct platform_device *pdev,
					struct dsi_ctrl *ctrl)
{
	int rc = 0;
	struct dsi_ctrl_bus_scale_info *bus = &ctrl->axi_bus_info;

	bus->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (IS_ERR_OR_NULL(bus->bus_scale_table)) {
		rc = PTR_ERR(bus->bus_scale_table);
		pr_err("msm_bus_cl_get_pdata() failed, rc = %d\n", rc);
		bus->bus_scale_table = NULL;
		return rc;
	}

	bus->bus_handle = msm_bus_scale_register_client(bus->bus_scale_table);
	if (!bus->bus_handle) {
		rc = -EINVAL;
		pr_err("failed to register axi bus client\n");
	}

	return rc;
}

static int dsi_ctrl_axi_bus_client_deinit(struct dsi_ctrl *ctrl)
{
	struct dsi_ctrl_bus_scale_info *bus = &ctrl->axi_bus_info;

	if (bus->bus_handle) {
		msm_bus_scale_unregister_client(bus->bus_handle);

		bus->bus_handle = 0;
	}

	return 0;
}

static int dsi_ctrl_validate_panel_info(struct dsi_ctrl *dsi_ctrl,
					struct dsi_host_config *config)
{
	int rc = 0;
	struct dsi_host_common_cfg *host_cfg = &config->common_config;

	if (config->panel_mode >= DSI_OP_MODE_MAX) {
		pr_err("Invalid dsi operation mode (%d)\n", config->panel_mode);
		rc = -EINVAL;
		goto err;
	}

	if ((host_cfg->data_lanes & (DSI_CLOCK_LANE - 1)) == 0) {
		pr_err("No data lanes are enabled\n");
		rc = -EINVAL;
		goto err;
	}
err:
	return rc;
}

static int dsi_ctrl_update_link_freqs(struct dsi_ctrl *dsi_ctrl,
				      struct dsi_host_config *config)
{
	int rc = 0;
	u32 num_of_lanes = 0;
	u32 bpp = 3;
	u64 h_period, v_period, bit_rate, pclk_rate, bit_rate_per_lane,
	    byte_clk_rate;
	struct dsi_host_common_cfg *host_cfg = &config->common_config;
	struct dsi_mode_info *timing = &config->video_timing;

	if (host_cfg->data_lanes & DSI_DATA_LANE_0)
		num_of_lanes++;
	if (host_cfg->data_lanes & DSI_DATA_LANE_1)
		num_of_lanes++;
	if (host_cfg->data_lanes & DSI_DATA_LANE_2)
		num_of_lanes++;
	if (host_cfg->data_lanes & DSI_DATA_LANE_3)
		num_of_lanes++;

	h_period = DSI_H_TOTAL(timing);
	v_period = DSI_V_TOTAL(timing);

	bit_rate = h_period * v_period * timing->refresh_rate * bpp * 8;
	bit_rate_per_lane = bit_rate;
	do_div(bit_rate_per_lane, num_of_lanes);
	pclk_rate = bit_rate;
	do_div(pclk_rate, (8 * bpp));
	byte_clk_rate = bit_rate_per_lane;
	do_div(byte_clk_rate, 8);
	pr_debug("bit_clk_rate = %llu, bit_clk_rate_per_lane = %llu\n",
		 bit_rate, bit_rate_per_lane);
	pr_debug("byte_clk_rate = %llu, pclk_rate = %llu\n",
		  byte_clk_rate, pclk_rate);

	rc = dsi_clk_set_link_frequencies(&dsi_ctrl->clk_info.link_clks,
					  pclk_rate,
					  byte_clk_rate,
					  config->esc_clk_rate_hz);
	if (rc)
		pr_err("Failed to update link frequencies\n");

	return rc;
}

static int dsi_ctrl_enable_supplies(struct dsi_ctrl *dsi_ctrl, bool enable)
{
	int rc = 0;

	if (enable) {
		rc = dsi_pwr_enable_regulator(&dsi_ctrl->pwr_info.host_pwr,
					      true);
		if (rc) {
			pr_err("failed to enable host power regs, rc=%d\n", rc);
			goto error;
		}

		rc = dsi_pwr_enable_regulator(&dsi_ctrl->pwr_info.digital,
					      true);
		if (rc) {
			pr_err("failed to enable gdsc, rc=%d\n", rc);
			(void)dsi_pwr_enable_regulator(
						&dsi_ctrl->pwr_info.host_pwr,
						false
						);
			goto error;
		}
	} else {
		rc = dsi_pwr_enable_regulator(&dsi_ctrl->pwr_info.digital,
					      false);
		if (rc) {
			pr_err("failed to disable gdsc, rc=%d\n", rc);
			goto error;
		}

		rc = dsi_pwr_enable_regulator(&dsi_ctrl->pwr_info.host_pwr,
					      false);
		if (rc) {
			pr_err("failed to disable host power regs, rc=%d\n",
			       rc);
			goto error;
		}
	}
error:
	return rc;
}

static int dsi_ctrl_vote_for_bandwidth(struct dsi_ctrl *dsi_ctrl, bool on)
{
	int rc = 0;
	bool changed = false;
	struct dsi_ctrl_bus_scale_info *axi_bus = &dsi_ctrl->axi_bus_info;

	if (on) {
		if (axi_bus->refcount == 0)
			changed = true;

		axi_bus->refcount++;
	} else {
		if (axi_bus->refcount != 0) {
			axi_bus->refcount--;

			if (axi_bus->refcount == 0)
				changed = true;
		} else {
			pr_err("bus bw votes are not balanced\n");
		}
	}

	if (changed) {
		rc = msm_bus_scale_client_update_request(axi_bus->bus_handle,
							 on ? 1 : 0);
		if (rc)
			pr_err("bus scale client update failed, rc=%d\n", rc);
	}

	return rc;
}

static int dsi_ctrl_copy_and_pad_cmd(struct dsi_ctrl *dsi_ctrl,
				     const struct mipi_dsi_packet *packet,
				     u8 **buffer,
				     u32 *size)
{
	int rc = 0;
	u8 *buf = NULL;
	u32 len, i;

	len = packet->size;
	len += 0x3; len &= ~0x03; /* Align to 32 bits */

	buf = devm_kzalloc(&dsi_ctrl->pdev->dev, len * sizeof(u8), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < len; i++) {
		if (i >= packet->size)
			buf[i] = 0xFF;
		else if (i < sizeof(packet->header))
			buf[i] = packet->header[i];
		else
			buf[i] = packet->payload[i - sizeof(packet->header)];
	}

	if (packet->payload_length > 0)
		buf[3] |= BIT(6);

	buf[3] |= BIT(7);
	*buffer = buf;
	*size = len;

	return rc;
}

static int dsi_message_tx(struct dsi_ctrl *dsi_ctrl,
			  const struct mipi_dsi_msg *msg,
			  u32 flags)
{
	int rc = 0;
	struct mipi_dsi_packet packet;
	struct dsi_ctrl_cmd_dma_fifo_info cmd;
	u32 hw_flags = 0;
	u32 length = 0;
	u8 *buffer = NULL;

	if (!(flags & DSI_CTRL_CMD_FIFO_STORE)) {
		pr_err("Memory DMA is not supported, use FIFO\n");
		goto error;
	}

	rc = mipi_dsi_create_packet(&packet, msg);
	if (rc) {
		pr_err("Failed to create message packet, rc=%d\n", rc);
		goto error;
	}

	if (flags & DSI_CTRL_CMD_FIFO_STORE) {
		rc = dsi_ctrl_copy_and_pad_cmd(dsi_ctrl,
					       &packet,
					       &buffer,
					       &length);
		if (rc) {
			pr_err("[%s] failed to copy message, rc=%d\n",
			       dsi_ctrl->name, rc);
			goto error;
		}
		cmd.command =  (u32 *)buffer;
		cmd.size = length;
		cmd.en_broadcast = (flags & DSI_CTRL_CMD_BROADCAST) ?
				     true : false;
		cmd.is_master = (flags & DSI_CTRL_CMD_BROADCAST_MASTER) ?
				  true : false;
		cmd.use_lpm = (msg->flags & MIPI_DSI_MSG_USE_LPM) ?
				  true : false;
	}

	hw_flags |= (flags & DSI_CTRL_CMD_DEFER_TRIGGER) ?
			DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER : 0;

	if (!(flags & DSI_CTRL_CMD_DEFER_TRIGGER))
		reinit_completion(&dsi_ctrl->int_info.cmd_dma_done);

	if (flags & DSI_CTRL_CMD_FIFO_STORE)
		dsi_ctrl->hw.ops.kickoff_fifo_command(&dsi_ctrl->hw,
						      &cmd,
						      hw_flags);

	if (!(flags & DSI_CTRL_CMD_DEFER_TRIGGER)) {
		u32 retry = 10;
		u32 status = 0;
		u64 error = 0;
		u32 mask = (DSI_CMD_MODE_DMA_DONE);

		while ((status == 0) && (retry > 0)) {
			udelay(1000);
			status = dsi_ctrl->hw.ops.get_interrupt_status(
								&dsi_ctrl->hw);
			error = dsi_ctrl->hw.ops.get_error_status(
								&dsi_ctrl->hw);
			status &= mask;
			retry--;
			dsi_ctrl->hw.ops.clear_interrupt_status(&dsi_ctrl->hw,
								status);
			dsi_ctrl->hw.ops.clear_error_status(&dsi_ctrl->hw,
							    error);
		}
		pr_debug("INT STATUS = %x, retry = %d\n", status, retry);
		if (retry == 0)
			pr_err("[DSI_%d]Command transfer failed\n",
			       dsi_ctrl->index);

		dsi_ctrl->hw.ops.reset_cmd_fifo(&dsi_ctrl->hw);
	}
error:
	if (buffer)
		devm_kfree(&dsi_ctrl->pdev->dev, buffer);
	return rc;
}

static int dsi_set_max_return_size(struct dsi_ctrl *dsi_ctrl,
				   const struct mipi_dsi_msg *rx_msg,
				   u32 size)
{
	int rc = 0;
	u8 tx[2] = { (u8)(size & 0xFF), (u8)(size >> 8) };
	struct mipi_dsi_msg msg = {
		.channel = rx_msg->channel,
		.type = MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE,
		.tx_len = 2,
		.tx_buf = tx,
	};

	rc = dsi_message_tx(dsi_ctrl, &msg, 0x0);
	if (rc)
		pr_err("failed to send max return size packet, rc=%d\n", rc);

	return rc;
}

static int dsi_message_rx(struct dsi_ctrl *dsi_ctrl,
			  const struct mipi_dsi_msg *msg,
			  u32 flags)
{
	int rc = 0;
	u32 rd_pkt_size;
	u32 total_read_len;
	u32 bytes_read = 0, tot_bytes_read = 0;
	u32 current_read_len;
	bool short_resp = false;
	bool read_done = false;

	if (msg->rx_len <= 2) {
		short_resp = true;
		rd_pkt_size = msg->rx_len;
		total_read_len = 4;
	} else {
		short_resp = false;
		current_read_len = 10;
		if (msg->rx_len < current_read_len)
			rd_pkt_size = msg->rx_len;
		else
			rd_pkt_size = current_read_len;

		total_read_len = current_read_len + 6;
	}

	while (!read_done) {
		rc = dsi_set_max_return_size(dsi_ctrl, msg, rd_pkt_size);
		if (rc) {
			pr_err("Failed to set max return packet size, rc=%d\n",
			       rc);
			goto error;
		}

		rc = dsi_message_tx(dsi_ctrl, msg, flags);
		if (rc) {
			pr_err("Message transmission failed, rc=%d\n", rc);
			goto error;
		}


		tot_bytes_read += bytes_read;
		if (short_resp)
			read_done = true;
		else if (msg->rx_len <= tot_bytes_read)
			read_done = true;
	}
error:
	return rc;
}


static int dsi_enable_ulps(struct dsi_ctrl *dsi_ctrl)
{
	int rc = 0;
	u32 lanes;
	u32 ulps_lanes;

	if (dsi_ctrl->host_config.panel_mode == DSI_OP_CMD_MODE)
		lanes = dsi_ctrl->host_config.common_config.data_lanes;

	lanes |= DSI_CLOCK_LANE;
	dsi_ctrl->hw.ops.ulps_request(&dsi_ctrl->hw, lanes);

	ulps_lanes = dsi_ctrl->hw.ops.get_lanes_in_ulps(&dsi_ctrl->hw);

	if ((lanes & ulps_lanes) != lanes) {
		pr_err("Failed to enter ULPS, request=0x%x, actual=0x%x\n",
		       lanes, ulps_lanes);
		rc = -EIO;
	}

	return rc;
}

static int dsi_disable_ulps(struct dsi_ctrl *dsi_ctrl)
{
	int rc = 0;
	u32 ulps_lanes, lanes = 0;

	if (dsi_ctrl->host_config.panel_mode == DSI_OP_CMD_MODE)
		lanes = dsi_ctrl->host_config.common_config.data_lanes;

	lanes |= DSI_CLOCK_LANE;
	ulps_lanes = dsi_ctrl->hw.ops.get_lanes_in_ulps(&dsi_ctrl->hw);

	if ((lanes & ulps_lanes) != lanes)
		pr_err("Mismatch between lanes in ULPS\n");

	lanes &= ulps_lanes;

	dsi_ctrl->hw.ops.ulps_exit(&dsi_ctrl->hw, lanes);

	/* 1 ms delay is recommended by specification */
	udelay(1000);

	dsi_ctrl->hw.ops.clear_ulps_request(&dsi_ctrl->hw, lanes);

	ulps_lanes = dsi_ctrl->hw.ops.get_lanes_in_ulps(&dsi_ctrl->hw);
	if (ulps_lanes & lanes) {
		pr_err("Lanes (0x%x) stuck in ULPS\n", ulps_lanes);
		rc = -EIO;
	}

	return rc;
}

static int dsi_ctrl_drv_state_init(struct dsi_ctrl *dsi_ctrl)
{
	int rc = 0;
	bool splash_enabled = false;
	struct dsi_ctrl_state_info *state = &dsi_ctrl->current_state;

	if (!splash_enabled) {
		state->power_state = DSI_CTRL_POWER_OFF;
		state->cmd_engine_state = DSI_CTRL_ENGINE_OFF;
		state->vid_engine_state = DSI_CTRL_ENGINE_OFF;
		state->pwr_enabled = false;
		state->core_clk_enabled = false;
		state->link_clk_enabled = false;
		state->ulps_enabled = false;
		state->clamp_enabled = false;
		state->clk_source_set = false;
	}

	return rc;
}

int dsi_ctrl_intr_deinit(struct dsi_ctrl *dsi_ctrl)
{
	struct dsi_ctrl_interrupts *ints = &dsi_ctrl->int_info;

	devm_free_irq(&dsi_ctrl->pdev->dev, ints->irq, dsi_ctrl);

	return 0;
}

static int dsi_ctrl_buffer_deinit(struct dsi_ctrl *dsi_ctrl)
{
	if (dsi_ctrl->tx_cmd_buf) {
		msm_gem_put_iova(dsi_ctrl->tx_cmd_buf, 0);

		msm_gem_free_object(dsi_ctrl->tx_cmd_buf);
		dsi_ctrl->tx_cmd_buf = NULL;
	}

	return 0;
}

int dsi_ctrl_buffer_init(struct dsi_ctrl *dsi_ctrl)
{
	int rc = 0;
	u32 iova = 0;

	dsi_ctrl->tx_cmd_buf = msm_gem_new(dsi_ctrl->drm_dev,
					   SZ_4K,
					   MSM_BO_UNCACHED);

	if (IS_ERR(dsi_ctrl->tx_cmd_buf)) {
		rc = PTR_ERR(dsi_ctrl->tx_cmd_buf);
		pr_err("failed to allocate gem, rc=%d\n", rc);
		dsi_ctrl->tx_cmd_buf = NULL;
		goto error;
	}

	dsi_ctrl->cmd_buffer_size = SZ_4K;

	rc = msm_gem_get_iova(dsi_ctrl->tx_cmd_buf, 0, &iova);
	if (rc) {
		pr_err("failed to get iova, rc=%d\n", rc);
		(void)dsi_ctrl_buffer_deinit(dsi_ctrl);
		goto error;
	}

	if (iova & 0x07) {
		pr_err("Tx command buffer is not 8 byte aligned\n");
		rc = -ENOTSUPP;
		(void)dsi_ctrl_buffer_deinit(dsi_ctrl);
		goto error;
	}
error:
	return rc;
}

static int dsi_enable_io_clamp(struct dsi_ctrl *dsi_ctrl, bool enable)
{
	bool en_ulps = dsi_ctrl->current_state.ulps_enabled;
	u32 lanes = 0;

	if (dsi_ctrl->host_config.panel_mode == DSI_OP_CMD_MODE)
		lanes = dsi_ctrl->host_config.common_config.data_lanes;

	lanes |= DSI_CLOCK_LANE;

	if (enable)
		dsi_ctrl->hw.ops.clamp_enable(&dsi_ctrl->hw, lanes, en_ulps);
	else
		dsi_ctrl->hw.ops.clamp_disable(&dsi_ctrl->hw, lanes, en_ulps);

	return 0;
}

static int dsi_ctrl_dev_probe(struct platform_device *pdev)
{
	struct dsi_ctrl *dsi_ctrl;
	struct dsi_ctrl_list_item *item;
	const struct of_device_id *id;
	enum dsi_ctrl_version version;
	u32 index = 0;
	int rc = 0;

	id = of_match_node(msm_dsi_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;

	version = *(enum dsi_ctrl_version *)id->data;

	item = devm_kzalloc(&pdev->dev, sizeof(*item), GFP_KERNEL);
	if (!item)
		return -ENOMEM;

	dsi_ctrl = devm_kzalloc(&pdev->dev, sizeof(*dsi_ctrl), GFP_KERNEL);
	if (!dsi_ctrl)
		return -ENOMEM;

	rc = of_property_read_u32(pdev->dev.of_node, "cell-index", &index);
	if (rc) {
		pr_debug("cell index not set, default to 0\n");
		index = 0;
	}

	dsi_ctrl->index = index;

	dsi_ctrl->name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!dsi_ctrl->name)
		dsi_ctrl->name = DSI_CTRL_DEFAULT_LABEL;

	rc = dsi_ctrl_init_regmap(pdev, dsi_ctrl);
	if (rc) {
		pr_err("Failed to parse register information, rc = %d\n", rc);
		goto fail;
	}

	rc = dsi_ctrl_clocks_init(pdev, dsi_ctrl);
	if (rc) {
		pr_err("Failed to parse clock information, rc = %d\n", rc);
		goto fail;
	}

	rc = dsi_ctrl_supplies_init(pdev, dsi_ctrl);
	if (rc) {
		pr_err("Failed to parse voltage supplies, rc = %d\n", rc);
		goto fail_clks;
	}

	dsi_ctrl->version = version;
	rc = dsi_catalog_ctrl_setup(&dsi_ctrl->hw, dsi_ctrl->version,
				    dsi_ctrl->index);
	if (rc) {
		pr_err("Catalog does not support version (%d)\n",
		       dsi_ctrl->version);
		goto fail_supplies;
	}

	rc = dsi_ctrl_axi_bus_client_init(pdev, dsi_ctrl);
	if (rc)
		pr_err("failed to init axi bus client, rc = %d\n", rc);

	item->ctrl = dsi_ctrl;

	mutex_lock(&dsi_ctrl_list_lock);
	list_add(&item->list, &dsi_ctrl_list);
	mutex_unlock(&dsi_ctrl_list_lock);

	mutex_init(&dsi_ctrl->ctrl_lock);

	dsi_ctrl->pdev = pdev;
	platform_set_drvdata(pdev, dsi_ctrl);

	pr_debug("Probe successful for %s\n", dsi_ctrl->name);

	return 0;

fail_supplies:
	(void)dsi_ctrl_supplies_deinit(dsi_ctrl);
fail_clks:
	(void)dsi_ctrl_clocks_deinit(dsi_ctrl);
fail:
	return rc;
}

static int dsi_ctrl_dev_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct dsi_ctrl *dsi_ctrl;
	struct list_head *pos, *tmp;

	dsi_ctrl = platform_get_drvdata(pdev);

	mutex_lock(&dsi_ctrl_list_lock);
	list_for_each_safe(pos, tmp, &dsi_ctrl_list) {
		struct dsi_ctrl_list_item *n = list_entry(pos,
						  struct dsi_ctrl_list_item,
						  list);
		if (n->ctrl == dsi_ctrl) {
			list_del(&n->list);
			break;
		}
	}
	mutex_unlock(&dsi_ctrl_list_lock);

	mutex_lock(&dsi_ctrl->ctrl_lock);
	rc = dsi_ctrl_axi_bus_client_deinit(dsi_ctrl);
	if (rc)
		pr_err("failed to deinitialize axi bus client, rc = %d\n", rc);

	rc = dsi_ctrl_supplies_deinit(dsi_ctrl);
	if (rc)
		pr_err("failed to deinitialize voltage supplies, rc=%d\n", rc);

	rc = dsi_ctrl_clocks_deinit(dsi_ctrl);
	if (rc)
		pr_err("failed to deinitialize clocks, rc=%d\n", rc);

	mutex_unlock(&dsi_ctrl->ctrl_lock);

	mutex_destroy(&dsi_ctrl->ctrl_lock);
	devm_kfree(&pdev->dev, dsi_ctrl);

	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver dsi_ctrl_driver = {
	.probe = dsi_ctrl_dev_probe,
	.remove = dsi_ctrl_dev_remove,
	.driver = {
		.name = "drm_dsi_ctrl",
		.of_match_table = msm_dsi_of_match,
	},
};

/**
 * dsi_ctrl_get() - get a dsi_ctrl handle from an of_node
 * @of_node:    of_node of the DSI controller.
 *
 * Gets the DSI controller handle for the corresponding of_node. The ref count
 * is incremented to one and all subsequent gets will fail until the original
 * clients calls a put.
 *
 * Return: DSI Controller handle.
 */
struct dsi_ctrl *dsi_ctrl_get(struct device_node *of_node)
{
	struct list_head *pos, *tmp;
	struct dsi_ctrl *ctrl = NULL;

	mutex_lock(&dsi_ctrl_list_lock);
	list_for_each_safe(pos, tmp, &dsi_ctrl_list) {
		struct dsi_ctrl_list_item *n;

		n = list_entry(pos, struct dsi_ctrl_list_item, list);
		if (n->ctrl->pdev->dev.of_node == of_node) {
			ctrl = n->ctrl;
			break;
		}
	}
	mutex_unlock(&dsi_ctrl_list_lock);

	if (!ctrl) {
		pr_err("Device with of node not found\n");
		ctrl = ERR_PTR(-EPROBE_DEFER);
		return ctrl;
	}

	mutex_lock(&ctrl->ctrl_lock);
	if (ctrl->refcount == 1) {
		pr_err("[%s] Device in use\n", ctrl->name);
		ctrl = ERR_PTR(-EBUSY);
	} else {
		ctrl->refcount++;
	}
	mutex_unlock(&ctrl->ctrl_lock);
	return ctrl;
}

/**
 * dsi_ctrl_put() - releases a dsi controller handle.
 * @dsi_ctrl:       DSI controller handle.
 *
 * Releases the DSI controller. Driver will clean up all resources and puts back
 * the DSI controller into reset state.
 */
void dsi_ctrl_put(struct dsi_ctrl *dsi_ctrl)
{
	mutex_lock(&dsi_ctrl->ctrl_lock);

	if (dsi_ctrl->refcount == 0)
		pr_err("Unbalanced dsi_ctrl_put call\n");
	else
		dsi_ctrl->refcount--;

	mutex_unlock(&dsi_ctrl->ctrl_lock);
}

/**
 * dsi_ctrl_drv_init() - initialize dsi controller driver.
 * @dsi_ctrl:      DSI controller handle.
 * @parent:        Parent directory for debug fs.
 *
 * Initializes DSI controller driver. Driver should be initialized after
 * dsi_ctrl_get() succeeds.
 *
 * Return: error code.
 */
int dsi_ctrl_drv_init(struct dsi_ctrl *dsi_ctrl, struct dentry *parent)
{
	int rc = 0;

	if (!dsi_ctrl || !parent) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);
	rc = dsi_ctrl_drv_state_init(dsi_ctrl);
	if (rc) {
		pr_err("Failed to initialize driver state, rc=%d\n", rc);
		goto error;
	}

	rc = dsi_ctrl_debugfs_init(dsi_ctrl, parent);
	if (rc) {
		pr_err("[DSI_%d] failed to init debug fs, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error;
	}

error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_drv_deinit() - de-initializes dsi controller driver
 * @dsi_ctrl:      DSI controller handle.
 *
 * Releases all resources acquired by dsi_ctrl_drv_init().
 *
 * Return: error code.
 */
int dsi_ctrl_drv_deinit(struct dsi_ctrl *dsi_ctrl)
{
	int rc = 0;

	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_ctrl_debugfs_deinit(dsi_ctrl);
	if (rc)
		pr_err("failed to release debugfs root, rc=%d\n", rc);

	rc = dsi_ctrl_buffer_deinit(dsi_ctrl);
	if (rc)
		pr_err("Failed to free cmd buffers, rc=%d\n", rc);

	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_phy_sw_reset() - perform a PHY software reset
 * @dsi_ctrl:         DSI controller handle.
 *
 * Performs a PHY software reset on the DSI controller. Reset should be done
 * when the controller power state is DSI_CTRL_POWER_CORE_CLK_ON and the PHY is
 * not enabled.
 *
 * This function will fail if driver is in any other state.
 *
 * Return: error code.
 */
int dsi_ctrl_phy_sw_reset(struct dsi_ctrl *dsi_ctrl)
{
	int rc = 0;

	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);
	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_PHY_SW_RESET, 0x0);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error;
	}

	dsi_ctrl->hw.ops.phy_sw_reset(&dsi_ctrl->hw);

	pr_debug("[DSI_%d] PHY soft reset done\n", dsi_ctrl->index);
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_PHY_SW_RESET, 0x0);
error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_seamless_timing_update() - update only controller timing
 * @dsi_ctrl:          DSI controller handle.
 * @timing:            New DSI timing info
 *
 * Updates host timing values to conduct a seamless transition to new timing
 * For example, to update the porch values in a dynamic fps switch.
 *
 * Return: error code.
 */
int dsi_ctrl_async_timing_update(struct dsi_ctrl *dsi_ctrl,
		struct dsi_mode_info *timing)
{
	struct dsi_mode_info *host_mode;
	int rc = 0;

	if (!dsi_ctrl || !timing) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_ASYNC_TIMING,
			DSI_CTRL_ENGINE_ON);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto exit;
	}

	host_mode = &dsi_ctrl->host_config.video_timing;
	memcpy(host_mode, timing, sizeof(*host_mode));

	dsi_ctrl->hw.ops.set_video_timing(&dsi_ctrl->hw, host_mode);

exit:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_host_init() - Initialize DSI host hardware.
 * @dsi_ctrl:        DSI controller handle.
 *
 * Initializes DSI controller hardware with host configuration provided by
 * dsi_ctrl_update_host_config(). Initialization can be performed only during
 * DSI_CTRL_POWER_CORE_CLK_ON state and after the PHY SW reset has been
 * performed.
 *
 * Return: error code.
 */
int dsi_ctrl_host_init(struct dsi_ctrl *dsi_ctrl)
{
	int rc = 0;

	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);
	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_HOST_INIT, 0x1);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error;
	}

	dsi_ctrl->hw.ops.setup_lane_map(&dsi_ctrl->hw,
					&dsi_ctrl->host_config.lane_map);

	dsi_ctrl->hw.ops.host_setup(&dsi_ctrl->hw,
				    &dsi_ctrl->host_config.common_config);

	if (dsi_ctrl->host_config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_ctrl->hw.ops.cmd_engine_setup(&dsi_ctrl->hw,
					&dsi_ctrl->host_config.common_config,
					&dsi_ctrl->host_config.u.cmd_engine);

		dsi_ctrl->hw.ops.setup_cmd_stream(&dsi_ctrl->hw,
				dsi_ctrl->host_config.video_timing.h_active,
				dsi_ctrl->host_config.video_timing.h_active * 3,
				dsi_ctrl->host_config.video_timing.v_active,
				0x0);
	} else {
		dsi_ctrl->hw.ops.video_engine_setup(&dsi_ctrl->hw,
					&dsi_ctrl->host_config.common_config,
					&dsi_ctrl->host_config.u.video_engine);
		dsi_ctrl->hw.ops.set_video_timing(&dsi_ctrl->hw,
					  &dsi_ctrl->host_config.video_timing);
	}



	dsi_ctrl->hw.ops.enable_status_interrupts(&dsi_ctrl->hw, 0x0);
	dsi_ctrl->hw.ops.enable_error_interrupts(&dsi_ctrl->hw, 0x0);

	/* Perform a soft reset before enabling dsi controller */
	dsi_ctrl->hw.ops.soft_reset(&dsi_ctrl->hw);
	pr_debug("[DSI_%d]Host initialization complete\n", dsi_ctrl->index);
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_HOST_INIT, 0x1);
error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_host_deinit() - De-Initialize DSI host hardware.
 * @dsi_ctrl:        DSI controller handle.
 *
 * De-initializes DSI controller hardware. It can be performed only during
 * DSI_CTRL_POWER_CORE_CLK_ON state after LINK clocks have been turned off.
 *
 * Return: error code.
 */
int dsi_ctrl_host_deinit(struct dsi_ctrl *dsi_ctrl)
{
	int rc = 0;

	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_HOST_INIT, 0x0);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		pr_err("driver state check failed, rc=%d\n", rc);
		goto error;
	}

	pr_debug("[DSI_%d] Host deinitization complete\n", dsi_ctrl->index);
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_HOST_INIT, 0x0);
error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_update_host_config() - update dsi host configuration
 * @dsi_ctrl:          DSI controller handle.
 * @config:            DSI host configuration.
 * @flags:             dsi_mode_flags modifying the behavior
 *
 * Updates driver with new Host configuration to use for host initialization.
 * This function call will only update the software context. The stored
 * configuration information will be used when the host is initialized.
 *
 * Return: error code.
 */
int dsi_ctrl_update_host_config(struct dsi_ctrl *ctrl,
				struct dsi_host_config *config,
				int flags)
{
	int rc = 0;

	if (!ctrl || !config) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&ctrl->ctrl_lock);

	rc = dsi_ctrl_validate_panel_info(ctrl, config);
	if (rc) {
		pr_err("panel validation failed, rc=%d\n", rc);
		goto error;
	}

	if (!(flags & DSI_MODE_FLAG_SEAMLESS)) {
		rc = dsi_ctrl_update_link_freqs(ctrl, config);
		if (rc) {
			pr_err("[%s] failed to update link frequencies, rc=%d\n",
			       ctrl->name, rc);
			goto error;
		}
	}

	pr_debug("[DSI_%d]Host config updated\n", ctrl->index);
	memcpy(&ctrl->host_config, config, sizeof(ctrl->host_config));
error:
	mutex_unlock(&ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_validate_timing() - validate a video timing configuration
 * @dsi_ctrl:       DSI controller handle.
 * @timing:         Pointer to timing data.
 *
 * Driver will validate if the timing configuration is supported on the
 * controller hardware.
 *
 * Return: error code if timing is not supported.
 */
int dsi_ctrl_validate_timing(struct dsi_ctrl *dsi_ctrl,
			     struct dsi_mode_info *mode)
{
	int rc = 0;

	if (!dsi_ctrl || !mode) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);
	mutex_unlock(&dsi_ctrl->ctrl_lock);

	return rc;
}

/**
 * dsi_ctrl_cmd_transfer() - Transfer commands on DSI link
 * @dsi_ctrl:             DSI controller handle.
 * @msg:                  Message to transfer on DSI link.
 * @flags:                Modifiers for message transfer.
 *
 * Command transfer can be done only when command engine is enabled. The
 * transfer API will block until either the command transfer finishes or
 * the timeout value is reached. If the trigger is deferred, it will return
 * without triggering the transfer. Command parameters are programmed to
 * hardware.
 *
 * Return: error code.
 */
int dsi_ctrl_cmd_transfer(struct dsi_ctrl *dsi_ctrl,
			  const struct mipi_dsi_msg *msg,
			  u32 flags)
{
	int rc = 0;

	if (!dsi_ctrl || !msg) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_CMD_TX, 0x0);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error;
	}

	rc = dsi_ctrl_vote_for_bandwidth(dsi_ctrl, true);
	if (rc) {
		pr_err("bandwidth request failed, rc=%d\n", rc);
		goto error;
	}

	if (flags & DSI_CTRL_CMD_READ) {
		rc = dsi_message_rx(dsi_ctrl, msg, flags);
		if (rc)
			pr_err("read message failed, rc=%d\n", rc);
	} else {
		rc = dsi_message_tx(dsi_ctrl, msg, flags);
		if (rc)
			pr_err("command msg transfer failed, rc = %d\n", rc);
	}

	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_CMD_TX, 0x0);

	(void)dsi_ctrl_vote_for_bandwidth(dsi_ctrl, false);
error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_cmd_tx_trigger() - Trigger a deferred command.
 * @dsi_ctrl:              DSI controller handle.
 * @flags:                 Modifiers.
 *
 * Return: error code.
 */
int dsi_ctrl_cmd_tx_trigger(struct dsi_ctrl *dsi_ctrl, u32 flags)
{
	int rc = 0;
	u32 status = 0;
	u32 mask = (DSI_CMD_MODE_DMA_DONE);

	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	reinit_completion(&dsi_ctrl->int_info.cmd_dma_done);

	dsi_ctrl->hw.ops.trigger_command_dma(&dsi_ctrl->hw);

	if ((flags & DSI_CTRL_CMD_BROADCAST) &&
	    (flags & DSI_CTRL_CMD_BROADCAST_MASTER)) {
		u32 retry = 10;

		while ((status == 0) && (retry > 0)) {
			udelay(1000);
			status = dsi_ctrl->hw.ops.get_interrupt_status(
								&dsi_ctrl->hw);
			status &= mask;
			retry--;
			dsi_ctrl->hw.ops.clear_interrupt_status(&dsi_ctrl->hw,
								status);
		}
		pr_debug("INT STATUS = %x, retry = %d\n", status, retry);
		if (retry == 0)
			pr_err("[DSI_%d]Command transfer failed\n",
			       dsi_ctrl->index);
	}

	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_set_power_state() - set power state for dsi controller
 * @dsi_ctrl:          DSI controller handle.
 * @state:             Power state.
 *
 * Set power state for DSI controller. Power state can be changed only when
 * Controller, Video and Command engines are turned off.
 *
 * Return: error code.
 */
int dsi_ctrl_set_power_state(struct dsi_ctrl *dsi_ctrl,
			     enum dsi_power_state state)
{
	int rc = 0;
	bool core_clk_enable = false;
	bool link_clk_enable = false;
	bool reg_enable = false;
	struct dsi_ctrl_state_info *drv_state;

	if (!dsi_ctrl || (state >= DSI_CTRL_POWER_MAX)) {
		pr_err("Invalid Params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_POWER_STATE_CHANGE,
				  state);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error;
	}

	if (state == DSI_CTRL_POWER_LINK_CLK_ON)
		reg_enable = core_clk_enable = link_clk_enable = true;
	else if (state == DSI_CTRL_POWER_CORE_CLK_ON)
		reg_enable = core_clk_enable = true;
	else if (state == DSI_CTRL_POWER_VREG_ON)
		reg_enable = true;

	drv_state = &dsi_ctrl->current_state;

	if ((reg_enable) && (reg_enable != drv_state->pwr_enabled)) {
		rc = dsi_ctrl_enable_supplies(dsi_ctrl, true);
		if (rc) {
			pr_err("[%d]failed to enable voltage supplies, rc=%d\n",
			       dsi_ctrl->index, rc);
			goto error;
		}
	}

	if ((core_clk_enable) &&
	    (core_clk_enable != drv_state->core_clk_enabled)) {
		rc = dsi_clk_enable_core_clks(&dsi_ctrl->clk_info.core_clks,
					      true);
		if (rc) {
			pr_err("[%d] failed to enable core clocks, rc=%d\n",
			       dsi_ctrl->index, rc);
			goto error;
		}
	}

	if (link_clk_enable != drv_state->link_clk_enabled) {
		rc = dsi_clk_enable_link_clks(&dsi_ctrl->clk_info.link_clks,
					      link_clk_enable);
		if (rc) {
			pr_err("[%d] failed to enable link clocks, rc=%d\n",
			       dsi_ctrl->index, rc);
			goto error;
		}
	}

	if ((!core_clk_enable) &&
	    (core_clk_enable != drv_state->core_clk_enabled)) {
		rc = dsi_clk_enable_core_clks(&dsi_ctrl->clk_info.core_clks,
					      false);
		if (rc) {
			pr_err("[%d] failed to disable core clocks, rc=%d\n",
			       dsi_ctrl->index, rc);
			goto error;
		}
	}

	if ((!reg_enable) && (reg_enable != drv_state->pwr_enabled)) {
		rc = dsi_ctrl_enable_supplies(dsi_ctrl, false);
		if (rc) {
			pr_err("[%d]failed to disable vreg supplies, rc=%d\n",
			       dsi_ctrl->index, rc);
			goto error;
		}
	}

	pr_debug("[DSI_%d] Power state updated to %d\n", dsi_ctrl->index,
		 state);
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_POWER_STATE_CHANGE, state);
error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_set_tpg_state() - enable/disable test pattern on the controller
 * @dsi_ctrl:          DSI controller handle.
 * @on:                enable/disable test pattern.
 *
 * Test pattern can be enabled only after Video engine (for video mode panels)
 * or command engine (for cmd mode panels) is enabled.
 *
 * Return: error code.
 */
int dsi_ctrl_set_tpg_state(struct dsi_ctrl *dsi_ctrl, bool on)
{
	int rc = 0;

	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_TPG, on);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error;
	}

	if (on) {
		if (dsi_ctrl->host_config.panel_mode == DSI_OP_VIDEO_MODE) {
			dsi_ctrl->hw.ops.video_test_pattern_setup(&dsi_ctrl->hw,
							  DSI_TEST_PATTERN_INC,
							  0xFFFF);
		} else {
			dsi_ctrl->hw.ops.cmd_test_pattern_setup(
							&dsi_ctrl->hw,
							DSI_TEST_PATTERN_INC,
							0xFFFF,
							0x0);
		}
	}
	dsi_ctrl->hw.ops.test_pattern_enable(&dsi_ctrl->hw, on);

	pr_debug("[DSI_%d]Set test pattern state=%d\n", dsi_ctrl->index, on);
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_TPG, on);
error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_set_host_engine_state() - set host engine state
 * @dsi_ctrl:            DSI Controller handle.
 * @state:               Engine state.
 *
 * Host engine state can be modified only when DSI controller power state is
 * set to DSI_CTRL_POWER_LINK_CLK_ON and cmd, video engines are disabled.
 *
 * Return: error code.
 */
int dsi_ctrl_set_host_engine_state(struct dsi_ctrl *dsi_ctrl,
				   enum dsi_engine_state state)
{
	int rc = 0;

	if (!dsi_ctrl || (state >= DSI_CTRL_ENGINE_MAX)) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_HOST_ENGINE, state);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error;
	}

	if (state == DSI_CTRL_ENGINE_ON)
		dsi_ctrl->hw.ops.ctrl_en(&dsi_ctrl->hw, true);
	else
		dsi_ctrl->hw.ops.ctrl_en(&dsi_ctrl->hw, false);

	pr_debug("[DSI_%d] Set host engine state = %d\n", dsi_ctrl->index,
		 state);
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_HOST_ENGINE, state);
error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_set_cmd_engine_state() - set command engine state
 * @dsi_ctrl:            DSI Controller handle.
 * @state:               Engine state.
 *
 * Command engine state can be modified only when DSI controller power state is
 * set to DSI_CTRL_POWER_LINK_CLK_ON.
 *
 * Return: error code.
 */
int dsi_ctrl_set_cmd_engine_state(struct dsi_ctrl *dsi_ctrl,
				  enum dsi_engine_state state)
{
	int rc = 0;

	if (!dsi_ctrl || (state >= DSI_CTRL_ENGINE_MAX)) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_CMD_ENGINE, state);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error;
	}

	if (state == DSI_CTRL_ENGINE_ON)
		dsi_ctrl->hw.ops.cmd_engine_en(&dsi_ctrl->hw, true);
	else
		dsi_ctrl->hw.ops.cmd_engine_en(&dsi_ctrl->hw, false);

	pr_debug("[DSI_%d] Set cmd engine state = %d\n", dsi_ctrl->index,
		 state);
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_CMD_ENGINE, state);
error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_set_vid_engine_state() - set video engine state
 * @dsi_ctrl:            DSI Controller handle.
 * @state:               Engine state.
 *
 * Video engine state can be modified only when DSI controller power state is
 * set to DSI_CTRL_POWER_LINK_CLK_ON.
 *
 * Return: error code.
 */
int dsi_ctrl_set_vid_engine_state(struct dsi_ctrl *dsi_ctrl,
				  enum dsi_engine_state state)
{
	int rc = 0;
	bool on;

	if (!dsi_ctrl || (state >= DSI_CTRL_ENGINE_MAX)) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_VID_ENGINE, state);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error;
	}

	on = (state == DSI_CTRL_ENGINE_ON) ? true : false;
	dsi_ctrl->hw.ops.video_engine_en(&dsi_ctrl->hw, on);

	/* perform a reset when turning off video engine */
	if (!on)
		dsi_ctrl->hw.ops.soft_reset(&dsi_ctrl->hw);

	pr_debug("[DSI_%d] Set video engine state = %d\n", dsi_ctrl->index,
		 state);
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_VID_ENGINE, state);
error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_set_ulps() - set ULPS state for DSI lanes.
 * @dsi_ctrl:         DSI controller handle.
 * @enable:           enable/disable ULPS.
 *
 * ULPS can be enabled/disabled after DSI host engine is turned on.
 *
 * Return: error code.
 */
int dsi_ctrl_set_ulps(struct dsi_ctrl *dsi_ctrl, bool enable)
{
	int rc = 0;

	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_ULPS_TOGGLE, enable);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error;
	}

	if (enable)
		rc = dsi_enable_ulps(dsi_ctrl);
	else
		rc = dsi_disable_ulps(dsi_ctrl);

	if (rc) {
		pr_err("[DSI_%d] Ulps state change(%d) failed, rc=%d\n",
		       dsi_ctrl->index, enable, rc);
		goto error;
	}

	pr_debug("[DSI_%d] ULPS state = %d\n", dsi_ctrl->index, enable);
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_ULPS_TOGGLE, enable);
error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_set_clamp_state() - set clamp state for DSI phy
 * @dsi_ctrl:             DSI controller handle.
 * @enable:               enable/disable clamping.
 *
 * Clamps can be enabled/disabled while DSI contoller is still turned on.
 *
 * Return: error code.
 */
int dsi_ctrl_set_clamp_state(struct dsi_ctrl *dsi_ctrl, bool enable)
{
	int rc = 0;

	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_CLAMP_TOGGLE, enable);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error;
	}

	rc = dsi_enable_io_clamp(dsi_ctrl, enable);
	if (rc) {
		pr_err("[DSI_%d] Failed to enable IO clamp\n", dsi_ctrl->index);
		goto error;
	}

	pr_debug("[DSI_%d] Clamp state = %d\n", dsi_ctrl->index, enable);
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_CLAMP_TOGGLE, enable);
error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_set_clock_source() - set clock source fpr dsi link clocks
 * @dsi_ctrl:        DSI controller handle.
 * @source_clks:     Source clocks for DSI link clocks.
 *
 * Clock source should be changed while link clocks are disabled.
 *
 * Return: error code.
 */
int dsi_ctrl_set_clock_source(struct dsi_ctrl *dsi_ctrl,
			      struct dsi_clk_link_set *source_clks)
{
	int rc = 0;
	u32 op_state = 0;

	if (!dsi_ctrl || !source_clks) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	if (source_clks->pixel_clk && source_clks->byte_clk)
		op_state = 1;

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_SET_CLK_SOURCE,
				 op_state);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->index, rc);
		goto error;
	}

	rc = dsi_clk_update_parent(source_clks, &dsi_ctrl->clk_info.rcg_clks);
	if (rc) {
		pr_err("[DSI_%d]Failed to update link clk parent, rc=%d\n",
		       dsi_ctrl->index, rc);
		(void)dsi_clk_update_parent(&dsi_ctrl->clk_info.pll_op_clks,
					    &dsi_ctrl->clk_info.rcg_clks);
		goto error;
	}

	dsi_ctrl->clk_info.pll_op_clks.byte_clk = source_clks->byte_clk;
	dsi_ctrl->clk_info.pll_op_clks.pixel_clk = source_clks->pixel_clk;

	pr_debug("[DSI_%d] Source clocks are updated\n", dsi_ctrl->index);

	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_SET_CLK_SOURCE, op_state);

error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_drv_register() - register platform driver for dsi controller
 */
void dsi_ctrl_drv_register(void)
{
	platform_driver_register(&dsi_ctrl_driver);
}

/**
 * dsi_ctrl_drv_unregister() - unregister platform driver
 */
void dsi_ctrl_drv_unregister(void)
{
	platform_driver_unregister(&dsi_ctrl_driver);
}
