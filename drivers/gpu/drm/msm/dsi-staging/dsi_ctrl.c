/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include "msm_mmu.h"
#include "dsi_ctrl.h"
#include "dsi_ctrl_hw.h"
#include "dsi_clk.h"
#include "dsi_pwr.h"
#include "dsi_catalog.h"

#include "sde_dbg.h"

#define DSI_CTRL_DEFAULT_LABEL "MDSS DSI CTRL"

#define DSI_CTRL_TX_TO_MS     200

#define TO_ON_OFF(x) ((x) ? "ON" : "OFF")

#define CEIL(x, y)              (((x) + ((y)-1)) / (y))
/**
 * enum dsi_ctrl_driver_ops - controller driver ops
 */
enum dsi_ctrl_driver_ops {
	DSI_CTRL_OP_POWER_STATE_CHANGE,
	DSI_CTRL_OP_CMD_ENGINE,
	DSI_CTRL_OP_VID_ENGINE,
	DSI_CTRL_OP_HOST_ENGINE,
	DSI_CTRL_OP_CMD_TX,
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
static const enum dsi_ctrl_version dsi_ctrl_v2_2 = DSI_CTRL_VERSION_2_2;
static const enum dsi_ctrl_version dsi_ctrl_v2_3 = DSI_CTRL_VERSION_2_3;

static const struct of_device_id msm_dsi_of_match[] = {
	{
		.compatible = "qcom,dsi-ctrl-hw-v1.4",
		.data = &dsi_ctrl_v1_4,
	},
	{
		.compatible = "qcom,dsi-ctrl-hw-v2.0",
		.data = &dsi_ctrl_v2_0,
	},
	{
		.compatible = "qcom,dsi-ctrl-hw-v2.2",
		.data = &dsi_ctrl_v2_2,
	},
	{
		.compatible = "qcom,dsi-ctrl-hw-v2.3",
		.data = &dsi_ctrl_v2_3,
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
			"\tCTRL_ENGINE = %s\n",
			TO_ON_OFF(dsi_ctrl->current_state.controller_state));
	len += snprintf((buf + len), (SZ_4K - len),
			"\tVIDEO_ENGINE = %s\n\tCOMMAND_ENGINE = %s\n",
			TO_ON_OFF(dsi_ctrl->current_state.vid_engine_state),
			TO_ON_OFF(dsi_ctrl->current_state.cmd_engine_state));

	/* Dump clock information */
	len += snprintf((buf + len), (SZ_4K - len), "\nClock Info:\n");
	len += snprintf((buf + len), (SZ_4K - len),
			"\tBYTE_CLK = %u, PIXEL_CLK = %u, ESC_CLK = %u\n",
			dsi_ctrl->clk_freq.byte_clk_rate,
			dsi_ctrl->clk_freq.pix_clk_rate,
			dsi_ctrl->clk_freq.esc_clk_rate);

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
	struct dsi_clk_ctrl_info clk_info;
	int rc = 0;

	if (!dsi_ctrl)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	clk_info.client = DSI_CLK_REQ_DSI_CLIENT;
	clk_info.clk_type = DSI_CORE_CLK;
	clk_info.clk_state = DSI_CLK_ON;

	rc = dsi_ctrl->clk_cb.dsi_clk_cb(dsi_ctrl->clk_cb.priv, clk_info);
	if (rc) {
		pr_err("failed to enable DSI core clocks\n");
		kfree(buf);
		return rc;
	}

	if (dsi_ctrl->hw.ops.reg_dump_to_buffer)
		len = dsi_ctrl->hw.ops.reg_dump_to_buffer(&dsi_ctrl->hw,
				buf, SZ_4K);

	clk_info.clk_state = DSI_CLK_OFF;
	rc = dsi_ctrl->clk_cb.dsi_clk_cb(dsi_ctrl->clk_cb.priv, clk_info);
	if (rc) {
		pr_err("failed to disable DSI core clocks\n");
		kfree(buf);
		return rc;
	}


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
	char dbg_name[DSI_DEBUG_NAME_LEN];

	dir = debugfs_create_dir(dsi_ctrl->name, parent);
	if (IS_ERR_OR_NULL(dir)) {
		rc = PTR_ERR(dir);
		pr_err("[DSI_%d] debugfs create dir failed, rc=%d\n",
		       dsi_ctrl->cell_index, rc);
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
		       dsi_ctrl->cell_index, rc);
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
		       dsi_ctrl->cell_index, rc);
		goto error_remove_dir;
	}

	dsi_ctrl->debugfs_root = dir;

	snprintf(dbg_name, DSI_DEBUG_NAME_LEN, "dsi%d_ctrl",
						dsi_ctrl->cell_index);
	sde_dbg_reg_register_base(dbg_name, dsi_ctrl->hw.base,
				msm_iomap_size(dsi_ctrl->pdev, "dsi_ctrl"));
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

static inline struct msm_gem_address_space*
dsi_ctrl_get_aspace(struct dsi_ctrl *dsi_ctrl,
		int domain)
{
	if (!dsi_ctrl || !dsi_ctrl->drm_dev)
		return NULL;

	return msm_gem_smmu_address_space_get(dsi_ctrl->drm_dev, domain);
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
			pr_err("[%d] No change in state, pwr_state=%d\n",
			       dsi_ctrl->cell_index, op_state);
			rc = -EINVAL;
		} else if (state->power_state == DSI_CTRL_POWER_VREG_ON) {
			if (state->vid_engine_state == DSI_CTRL_ENGINE_ON) {
				pr_err("[%d]State error: op=%d: %d\n",
				       dsi_ctrl->cell_index,
				       op_state,
				       state->vid_engine_state);
				rc = -EINVAL;
			}
		}
		break;
	case DSI_CTRL_OP_CMD_ENGINE:
		if (state->cmd_engine_state == op_state) {
			pr_err("[%d] No change in state, cmd_state=%d\n",
			       dsi_ctrl->cell_index, op_state);
			rc = -EINVAL;
		} else if ((state->power_state != DSI_CTRL_POWER_VREG_ON) ||
			   (state->controller_state != DSI_CTRL_ENGINE_ON)) {
			pr_err("[%d]State error: op=%d: %d, %d\n",
			       dsi_ctrl->cell_index,
			       op,
			       state->power_state,
			       state->controller_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_VID_ENGINE:
		if (state->vid_engine_state == op_state) {
			pr_err("[%d] No change in state, cmd_state=%d\n",
			       dsi_ctrl->cell_index, op_state);
			rc = -EINVAL;
		} else if ((state->power_state != DSI_CTRL_POWER_VREG_ON) ||
			   (state->controller_state != DSI_CTRL_ENGINE_ON)) {
			pr_err("[%d]State error: op=%d: %d, %d\n",
			       dsi_ctrl->cell_index,
			       op,
			       state->power_state,
			       state->controller_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_HOST_ENGINE:
		if (state->controller_state == op_state) {
			pr_err("[%d] No change in state, ctrl_state=%d\n",
			       dsi_ctrl->cell_index, op_state);
			rc = -EINVAL;
		} else if (state->power_state != DSI_CTRL_POWER_VREG_ON) {
			pr_err("[%d]State error (link is off): op=%d:, %d\n",
			       dsi_ctrl->cell_index,
			       op_state,
			       state->power_state);
			rc = -EINVAL;
		} else if ((op_state == DSI_CTRL_ENGINE_OFF) &&
			   ((state->cmd_engine_state != DSI_CTRL_ENGINE_OFF) ||
			    (state->vid_engine_state != DSI_CTRL_ENGINE_OFF))) {
			pr_err("[%d]State error (eng on): op=%d: %d, %d\n",
				  dsi_ctrl->cell_index,
				  op_state,
				  state->cmd_engine_state,
				  state->vid_engine_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_CMD_TX:
		if ((state->power_state != DSI_CTRL_POWER_VREG_ON) ||
		    (state->host_initialized != true) ||
		    (state->cmd_engine_state != DSI_CTRL_ENGINE_ON)) {
			pr_err("[%d]State error: op=%d: %d, %d, %d\n",
			       dsi_ctrl->cell_index,
			       op,
			       state->power_state,
			       state->host_initialized,
			       state->cmd_engine_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_HOST_INIT:
		if (state->host_initialized == op_state) {
			pr_err("[%d] No change in state, host_init=%d\n",
			       dsi_ctrl->cell_index, op_state);
			rc = -EINVAL;
		} else if (state->power_state != DSI_CTRL_POWER_VREG_ON) {
			pr_err("[%d]State error: op=%d: %d\n",
			       dsi_ctrl->cell_index, op, state->power_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_TPG:
		if (state->tpg_enabled == op_state) {
			pr_err("[%d] No change in state, tpg_enabled=%d\n",
			       dsi_ctrl->cell_index, op_state);
			rc = -EINVAL;
		} else if ((state->power_state != DSI_CTRL_POWER_VREG_ON) ||
			   (state->controller_state != DSI_CTRL_ENGINE_ON)) {
			pr_err("[%d]State error: op=%d: %d, %d\n",
			       dsi_ctrl->cell_index,
			       op,
			       state->power_state,
			       state->controller_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_PHY_SW_RESET:
		if (state->power_state != DSI_CTRL_POWER_VREG_ON) {
			pr_err("[%d]State error: op=%d: %d\n",
			       dsi_ctrl->cell_index, op, state->power_state);
			rc = -EINVAL;
		}
		break;
	case DSI_CTRL_OP_ASYNC_TIMING:
		if (state->vid_engine_state != op_state) {
			pr_err("[%d] Unexpected engine state vid_state=%d\n",
			       dsi_ctrl->cell_index, op_state);
			rc = -EINVAL;
		}
		break;
	default:
		rc = -ENOTSUPP;
		break;
	}

	return rc;
}

bool dsi_ctrl_validate_host_state(struct dsi_ctrl *dsi_ctrl)
{
	struct dsi_ctrl_state_info *state = &dsi_ctrl->current_state;

	if (!state) {
		pr_err("Invalid host state for DSI controller\n");
		return -EINVAL;
	}

	if (!state->host_initialized)
		return true;

	return false;
}

static void dsi_ctrl_update_state(struct dsi_ctrl *dsi_ctrl,
				  enum dsi_ctrl_driver_ops op,
				  u32 op_state)
{
	struct dsi_ctrl_state_info *state = &dsi_ctrl->current_state;

	switch (op) {
	case DSI_CTRL_OP_POWER_STATE_CHANGE:
		state->power_state = op_state;
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

	switch (ctrl->version) {
	case DSI_CTRL_VERSION_1_4:
	case DSI_CTRL_VERSION_2_0:
		ptr = msm_ioremap(pdev, "mmss_misc", ctrl->name);
		if (IS_ERR(ptr)) {
			pr_err("mmss_misc base address not found for [%s]\n",
					ctrl->name);
			rc = PTR_ERR(ptr);
			return rc;
		}
		ctrl->hw.mmss_misc_base = ptr;
		ctrl->hw.disp_cc_base = NULL;
		break;
	case DSI_CTRL_VERSION_2_2:
	case DSI_CTRL_VERSION_2_3:
		ptr = msm_ioremap(pdev, "disp_cc_base", ctrl->name);
		if (IS_ERR(ptr)) {
			pr_err("disp_cc base address not found for [%s]\n",
					ctrl->name);
			rc = PTR_ERR(ptr);
			return rc;
		}
		ctrl->hw.disp_cc_base = ptr;
		ctrl->hw.mmss_misc_base = NULL;
		break;
	default:
		break;
	}

	return rc;
}

static int dsi_ctrl_clocks_deinit(struct dsi_ctrl *ctrl)
{
	struct dsi_core_clk_info *core = &ctrl->clk_info.core_clks;
	struct dsi_link_lp_clk_info *lp_link = &ctrl->clk_info.lp_link_clks;
	struct dsi_link_hs_clk_info *hs_link = &ctrl->clk_info.hs_link_clks;
	struct dsi_clk_link_set *rcg = &ctrl->clk_info.rcg_clks;

	if (core->mdp_core_clk)
		devm_clk_put(&ctrl->pdev->dev, core->mdp_core_clk);
	if (core->iface_clk)
		devm_clk_put(&ctrl->pdev->dev, core->iface_clk);
	if (core->core_mmss_clk)
		devm_clk_put(&ctrl->pdev->dev, core->core_mmss_clk);
	if (core->bus_clk)
		devm_clk_put(&ctrl->pdev->dev, core->bus_clk);
	if (core->mnoc_clk)
		devm_clk_put(&ctrl->pdev->dev, core->mnoc_clk);

	memset(core, 0x0, sizeof(*core));

	if (hs_link->byte_clk)
		devm_clk_put(&ctrl->pdev->dev, hs_link->byte_clk);
	if (hs_link->pixel_clk)
		devm_clk_put(&ctrl->pdev->dev, hs_link->pixel_clk);
	if (lp_link->esc_clk)
		devm_clk_put(&ctrl->pdev->dev, lp_link->esc_clk);
	if (hs_link->byte_intf_clk)
		devm_clk_put(&ctrl->pdev->dev, hs_link->byte_intf_clk);

	memset(hs_link, 0x0, sizeof(*hs_link));
	memset(lp_link, 0x0, sizeof(*lp_link));

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
	struct dsi_link_lp_clk_info *lp_link = &ctrl->clk_info.lp_link_clks;
	struct dsi_link_hs_clk_info *hs_link = &ctrl->clk_info.hs_link_clks;
	struct dsi_clk_link_set *rcg = &ctrl->clk_info.rcg_clks;

	core->mdp_core_clk = devm_clk_get(&pdev->dev, "mdp_core_clk");
	if (IS_ERR(core->mdp_core_clk)) {
		core->mdp_core_clk = NULL;
		pr_debug("failed to get mdp_core_clk, rc=%d\n", rc);
	}

	core->iface_clk = devm_clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(core->iface_clk)) {
		core->iface_clk = NULL;
		pr_debug("failed to get iface_clk, rc=%d\n", rc);
	}

	core->core_mmss_clk = devm_clk_get(&pdev->dev, "core_mmss_clk");
	if (IS_ERR(core->core_mmss_clk)) {
		core->core_mmss_clk = NULL;
		pr_debug("failed to get core_mmss_clk, rc=%d\n", rc);
	}

	core->bus_clk = devm_clk_get(&pdev->dev, "bus_clk");
	if (IS_ERR(core->bus_clk)) {
		core->bus_clk = NULL;
		pr_debug("failed to get bus_clk, rc=%d\n", rc);
	}

	core->mnoc_clk = devm_clk_get(&pdev->dev, "mnoc_clk");
	if (IS_ERR(core->mnoc_clk)) {
		core->mnoc_clk = NULL;
		pr_debug("can't get mnoc clock, rc=%d\n", rc);
	}

	hs_link->byte_clk = devm_clk_get(&pdev->dev, "byte_clk");
	if (IS_ERR(hs_link->byte_clk)) {
		rc = PTR_ERR(hs_link->byte_clk);
		pr_err("failed to get byte_clk, rc=%d\n", rc);
		goto fail;
	}

	hs_link->pixel_clk = devm_clk_get(&pdev->dev, "pixel_clk");
	if (IS_ERR(hs_link->pixel_clk)) {
		rc = PTR_ERR(hs_link->pixel_clk);
		pr_err("failed to get pixel_clk, rc=%d\n", rc);
		goto fail;
	}

	lp_link->esc_clk = devm_clk_get(&pdev->dev, "esc_clk");
	if (IS_ERR(lp_link->esc_clk)) {
		rc = PTR_ERR(lp_link->esc_clk);
		pr_err("failed to get esc_clk, rc=%d\n", rc);
		goto fail;
	}

	hs_link->byte_intf_clk = devm_clk_get(&pdev->dev, "byte_intf_clk");
	if (IS_ERR(hs_link->byte_intf_clk)) {
		hs_link->byte_intf_clk = NULL;
		pr_debug("can't find byte intf clk, rc=%d\n", rc);
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

	rc = dsi_pwr_get_dt_vreg_data(&pdev->dev,
					  &ctrl->pwr_info.digital,
					  "qcom,core-supply-entries");
	if (rc)
		pr_debug("failed to get digital supply, rc = %d\n", rc);

	rc = dsi_pwr_get_dt_vreg_data(&pdev->dev,
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
	if (ctrl->pwr_info.digital.vregs)
		devm_kfree(&pdev->dev, ctrl->pwr_info.digital.vregs);
	ctrl->pwr_info.digital.vregs = NULL;
	ctrl->pwr_info.digital.count = 0;
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
		pr_debug("msm_bus_cl_get_pdata() failed, rc = %d\n", rc);
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

/* Function returns number of bits per pxl */
static int dsi_ctrl_pixel_format_to_bpp(enum dsi_pixel_format dst_format)
{
	u32 bpp = 0;

	switch (dst_format) {
	case DSI_PIXEL_FORMAT_RGB111:
		bpp = 3;
		break;
	case DSI_PIXEL_FORMAT_RGB332:
		bpp = 8;
		break;
	case DSI_PIXEL_FORMAT_RGB444:
		bpp = 12;
		break;
	case DSI_PIXEL_FORMAT_RGB565:
		bpp = 16;
		break;
	case DSI_PIXEL_FORMAT_RGB666:
	case DSI_PIXEL_FORMAT_RGB666_LOOSE:
		bpp = 18;
		break;
	case DSI_PIXEL_FORMAT_RGB888:
		bpp = 24;
		break;
	default:
		bpp = 24;
		break;
	}
	return bpp;
}

static int dsi_ctrl_update_link_freqs(struct dsi_ctrl *dsi_ctrl,
	struct dsi_host_config *config, void *clk_handle)
{
	int rc = 0;
	u32 num_of_lanes = 0;
	u32 bpp;
	u64 h_period, v_period, bit_rate, pclk_rate, bit_rate_per_lane,
	    byte_clk_rate;
	struct dsi_host_common_cfg *host_cfg = &config->common_config;
	struct dsi_mode_info *timing = &config->video_timing;

	/* Get bits per pxl in desitnation format */
	bpp = dsi_ctrl_pixel_format_to_bpp(host_cfg->dst_format);

	if (host_cfg->data_lanes & DSI_DATA_LANE_0)
		num_of_lanes++;
	if (host_cfg->data_lanes & DSI_DATA_LANE_1)
		num_of_lanes++;
	if (host_cfg->data_lanes & DSI_DATA_LANE_2)
		num_of_lanes++;
	if (host_cfg->data_lanes & DSI_DATA_LANE_3)
		num_of_lanes++;

	if (config->bit_clk_rate_hz == 0) {
		h_period = DSI_H_TOTAL_DSC(timing);
		v_period = DSI_V_TOTAL(timing);
		bit_rate = h_period * v_period * timing->refresh_rate * bpp;
	} else {
		bit_rate = config->bit_clk_rate_hz * num_of_lanes;
	}

	bit_rate_per_lane = bit_rate;
	do_div(bit_rate_per_lane, num_of_lanes);
	pclk_rate = bit_rate;
	do_div(pclk_rate, bpp);
	byte_clk_rate = bit_rate_per_lane;
	do_div(byte_clk_rate, 8);
	pr_debug("bit_clk_rate = %llu, bit_clk_rate_per_lane = %llu\n",
		 bit_rate, bit_rate_per_lane);
	pr_debug("byte_clk_rate = %llu, pclk_rate = %llu\n",
		  byte_clk_rate, pclk_rate);

	dsi_ctrl->clk_freq.byte_clk_rate = byte_clk_rate;
	dsi_ctrl->clk_freq.pix_clk_rate = pclk_rate;
	dsi_ctrl->clk_freq.esc_clk_rate = config->esc_clk_rate_hz;

	rc = dsi_clk_set_link_frequencies(clk_handle, dsi_ctrl->clk_freq,
					dsi_ctrl->cell_index);
	if (rc)
		pr_err("Failed to update link frequencies\n");

	return rc;
}

static int dsi_ctrl_enable_supplies(struct dsi_ctrl *dsi_ctrl, bool enable)
{
	int rc = 0;

	if (enable) {
		if (!dsi_ctrl->current_state.host_initialized) {
			rc = dsi_pwr_enable_regulator(
				&dsi_ctrl->pwr_info.host_pwr, true);
			if (rc) {
				pr_err("failed to enable host power regs\n");
				goto error;
			}
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

		if (!dsi_ctrl->current_state.host_initialized) {
			rc = dsi_pwr_enable_regulator(
				&dsi_ctrl->pwr_info.host_pwr, false);
			if (rc) {
				pr_err("failed to disable host power regs\n");
				goto error;
			}
		}
	}
error:
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


	/* send embedded BTA for read commands */
	if ((buf[2] & 0x3f) == MIPI_DSI_DCS_READ)
		buf[3] |= BIT(5);

	*buffer = buf;
	*size = len;

	return rc;
}

int dsi_ctrl_wait_for_cmd_mode_mdp_idle(struct dsi_ctrl *dsi_ctrl)
{
	int rc = 0;

	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (dsi_ctrl->host_config.panel_mode != DSI_OP_CMD_MODE)
		return -EINVAL;

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_ctrl->hw.ops.wait_for_cmd_mode_mdp_idle(&dsi_ctrl->hw);

	mutex_unlock(&dsi_ctrl->ctrl_lock);

	return rc;
}

static void dsi_ctrl_wait_for_video_done(struct dsi_ctrl *dsi_ctrl)
{
	u32 v_total = 0, v_blank = 0, sleep_ms = 0, fps = 0, ret;
	struct dsi_mode_info *timing;

	/**
	 * No need to wait if the panel is not video mode or
	 * if DSI controller supports command DMA scheduling or
	 * if we are sending init commands.
	 */
	if ((dsi_ctrl->host_config.panel_mode != DSI_OP_VIDEO_MODE) ||
		(dsi_ctrl->version >= DSI_CTRL_VERSION_2_2) ||
		(dsi_ctrl->current_state.vid_engine_state !=
					DSI_CTRL_ENGINE_ON))
		return;

	dsi_ctrl->hw.ops.clear_interrupt_status(&dsi_ctrl->hw,
				DSI_VIDEO_MODE_FRAME_DONE);

	dsi_ctrl_enable_status_interrupt(dsi_ctrl,
				DSI_SINT_VIDEO_MODE_FRAME_DONE, NULL);
	reinit_completion(&dsi_ctrl->irq_info.vid_frame_done);
	ret = wait_for_completion_timeout(
			&dsi_ctrl->irq_info.vid_frame_done,
			msecs_to_jiffies(DSI_CTRL_TX_TO_MS));
	if (ret <= 0)
		pr_debug("wait for video done failed\n");
	dsi_ctrl_disable_status_interrupt(dsi_ctrl,
				DSI_SINT_VIDEO_MODE_FRAME_DONE);

	timing = &(dsi_ctrl->host_config.video_timing);
	v_total = timing->v_sync_width + timing->v_back_porch +
			timing->v_front_porch + timing->v_active;
	v_blank = timing->v_sync_width + timing->v_back_porch;
	fps = timing->refresh_rate;

	sleep_ms = CEIL((v_blank * 1000), (v_total * fps)) + 1;
	udelay(sleep_ms * 1000);
}

void dsi_message_setup_tx_mode(struct dsi_ctrl *dsi_ctrl,
		u32 cmd_len,
		u32 *flags)
{
	/**
	 * Setup the mode of transmission
	 * override cmd fetch mode during secure session
	 */
	if (dsi_ctrl->secure_mode) {
		*flags &= ~DSI_CTRL_CMD_FETCH_MEMORY;
		*flags |= DSI_CTRL_CMD_FIFO_STORE;
		pr_debug("[%s] override to TPG during secure session\n",
				dsi_ctrl->name);
		return;
	}

	/* Check to see if cmd len plus header is greater than fifo size */
	if ((cmd_len + 4) > DSI_EMBEDDED_MODE_DMA_MAX_SIZE_BYTES) {
		*flags |= DSI_CTRL_CMD_NON_EMBEDDED_MODE;
		pr_debug("[%s] override to non-embedded mode,cmd len =%d\n",
				dsi_ctrl->name, cmd_len);
		return;
	}
}

int dsi_message_validate_tx_mode(struct dsi_ctrl *dsi_ctrl,
		u32 cmd_len,
		u32 *flags)
{
	int rc = 0;

	if (*flags & DSI_CTRL_CMD_FIFO_STORE) {
		/* if command size plus header is greater than fifo size */
		if ((cmd_len + 4) > DSI_CTRL_MAX_CMD_FIFO_STORE_SIZE) {
			pr_err("Cannot transfer Cmd in FIFO config\n");
			return -ENOTSUPP;
		}
		if (!dsi_ctrl->hw.ops.kickoff_fifo_command) {
			pr_err("Cannot transfer command,ops not defined\n");
			return -ENOTSUPP;
		}
	}

	if (*flags & DSI_CTRL_CMD_NON_EMBEDDED_MODE) {
		if (*flags & DSI_CTRL_CMD_BROADCAST) {
			pr_err("Non embedded not supported with broadcast\n");
			return -ENOTSUPP;
		}
		if (!dsi_ctrl->hw.ops.kickoff_command_non_embedded_mode) {
			pr_err(" Cannot transfer command,ops not defined\n");
			return -ENOTSUPP;
		}
	}
	return rc;
}

static int dsi_message_tx(struct dsi_ctrl *dsi_ctrl,
			  const struct mipi_dsi_msg *msg,
			  u32 flags)
{
	int rc = 0, ret = 0;
	struct mipi_dsi_packet packet;
	struct dsi_ctrl_cmd_dma_fifo_info cmd;
	struct dsi_ctrl_cmd_dma_info cmd_mem;
	u32 hw_flags = 0;
	u32 length = 0;
	u8 *buffer = NULL;
	u32 cnt = 0, line_no = 0x1;
	u8 *cmdbuf;
	struct dsi_mode_info *timing;
	struct dsi_ctrl_hw_ops dsi_hw_ops = dsi_ctrl->hw.ops;

	/* Select the tx mode to transfer the command */
	dsi_message_setup_tx_mode(dsi_ctrl, msg->tx_len, &flags);

	/* Validate the mode before sending the command */
	rc = dsi_message_validate_tx_mode(dsi_ctrl, msg->tx_len, &flags);
	if (rc) {
		pr_err(" Cmd tx validation failed, cannot transfer cmd\n");
		rc = -ENOTSUPP;
		goto error;
	}

	if (flags & DSI_CTRL_CMD_NON_EMBEDDED_MODE) {
		cmd_mem.offset = dsi_ctrl->cmd_buffer_iova;
		cmd_mem.en_broadcast = (flags & DSI_CTRL_CMD_BROADCAST) ?
			true : false;
		cmd_mem.is_master = (flags & DSI_CTRL_CMD_BROADCAST_MASTER) ?
			true : false;
		cmd_mem.use_lpm = (msg->flags & MIPI_DSI_MSG_USE_LPM) ?
			true : false;
		cmd_mem.datatype = msg->type;
		cmd_mem.length = msg->tx_len;

		dsi_ctrl->cmd_len = msg->tx_len;
		memcpy(dsi_ctrl->vaddr, msg->tx_buf, msg->tx_len);
		pr_debug(" non-embedded mode , size of command =%zd\n",
					msg->tx_len);

		goto kickoff;
	}

	rc = mipi_dsi_create_packet(&packet, msg);
	if (rc) {
		pr_err("Failed to create message packet, rc=%d\n", rc);
		goto error;
	}

	rc = dsi_ctrl_copy_and_pad_cmd(dsi_ctrl,
			&packet,
			&buffer,
			&length);
	if (rc) {
		pr_err("[%s] failed to copy message, rc=%d\n",
				dsi_ctrl->name, rc);
		goto error;
	}

	if ((msg->flags & MIPI_DSI_MSG_LASTCOMMAND))
		buffer[3] |= BIT(7);//set the last cmd bit in header.

	if (flags & DSI_CTRL_CMD_FETCH_MEMORY) {
		/* Embedded mode config is selected */
		cmd_mem.offset = dsi_ctrl->cmd_buffer_iova;
		cmd_mem.en_broadcast = (flags & DSI_CTRL_CMD_BROADCAST) ?
			true : false;
		cmd_mem.is_master = (flags & DSI_CTRL_CMD_BROADCAST_MASTER) ?
			true : false;
		cmd_mem.use_lpm = (msg->flags & MIPI_DSI_MSG_USE_LPM) ?
			true : false;

		cmdbuf = (u8 *)(dsi_ctrl->vaddr);

		msm_gem_sync(dsi_ctrl->tx_cmd_buf);
		for (cnt = 0; cnt < length; cnt++)
			cmdbuf[dsi_ctrl->cmd_len + cnt] = buffer[cnt];

		dsi_ctrl->cmd_len += length;

		if (!(msg->flags & MIPI_DSI_MSG_LASTCOMMAND)) {
			goto error;
		} else {
			cmd_mem.length = dsi_ctrl->cmd_len;
			dsi_ctrl->cmd_len = 0;
		}

	} else if (flags & DSI_CTRL_CMD_FIFO_STORE) {
		cmd.command =  (u32 *)buffer;
		cmd.size = length;
		cmd.en_broadcast = (flags & DSI_CTRL_CMD_BROADCAST) ?
				     true : false;
		cmd.is_master = (flags & DSI_CTRL_CMD_BROADCAST_MASTER) ?
				  true : false;
		cmd.use_lpm = (msg->flags & MIPI_DSI_MSG_USE_LPM) ?
				  true : false;
	}

kickoff:
	timing = &(dsi_ctrl->host_config.video_timing);
	if (timing)
		line_no += timing->v_back_porch + timing->v_sync_width +
				timing->v_active;
	if ((dsi_ctrl->host_config.panel_mode == DSI_OP_VIDEO_MODE) &&
		dsi_hw_ops.schedule_dma_cmd &&
		(dsi_ctrl->current_state.vid_engine_state ==
					DSI_CTRL_ENGINE_ON))
		dsi_hw_ops.schedule_dma_cmd(&dsi_ctrl->hw,
				line_no);

	hw_flags |= (flags & DSI_CTRL_CMD_DEFER_TRIGGER) ?
			DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER : 0;

	if ((msg->flags & MIPI_DSI_MSG_LASTCOMMAND))
		hw_flags |= DSI_CTRL_CMD_LAST_COMMAND;

	if (flags & DSI_CTRL_CMD_DEFER_TRIGGER) {
		if (flags & DSI_CTRL_CMD_FETCH_MEMORY) {
			if (flags & DSI_CTRL_CMD_NON_EMBEDDED_MODE) {
				dsi_hw_ops.kickoff_command_non_embedded_mode(
							&dsi_ctrl->hw,
							&cmd_mem,
							hw_flags);
			} else {
				dsi_hw_ops.kickoff_command(
						&dsi_ctrl->hw,
						&cmd_mem,
						hw_flags);
			}
		} else if (flags & DSI_CTRL_CMD_FIFO_STORE) {
			dsi_hw_ops.kickoff_fifo_command(&dsi_ctrl->hw,
							      &cmd,
							      hw_flags);
		}
	}

	if (!(flags & DSI_CTRL_CMD_DEFER_TRIGGER)) {
		dsi_ctrl_wait_for_video_done(dsi_ctrl);
		dsi_ctrl_enable_status_interrupt(dsi_ctrl,
					DSI_SINT_CMD_MODE_DMA_DONE, NULL);
		if (dsi_hw_ops.mask_error_intr)
			dsi_hw_ops.mask_error_intr(&dsi_ctrl->hw,
					BIT(DSI_FIFO_OVERFLOW), true);
		reinit_completion(&dsi_ctrl->irq_info.cmd_dma_done);

		if (flags & DSI_CTRL_CMD_FETCH_MEMORY) {
			if (flags & DSI_CTRL_CMD_NON_EMBEDDED_MODE) {
				dsi_hw_ops.kickoff_command_non_embedded_mode(
							&dsi_ctrl->hw,
							&cmd_mem,
							hw_flags);
			} else {
				dsi_hw_ops.kickoff_command(
						&dsi_ctrl->hw,
						&cmd_mem,
						hw_flags);
			}
		} else if (flags & DSI_CTRL_CMD_FIFO_STORE) {
			dsi_hw_ops.kickoff_fifo_command(&dsi_ctrl->hw,
							      &cmd,
							      hw_flags);
		}

		ret = wait_for_completion_timeout(
				&dsi_ctrl->irq_info.cmd_dma_done,
				msecs_to_jiffies(DSI_CTRL_TX_TO_MS));

		if (ret == 0) {
			u32 status = dsi_hw_ops.get_interrupt_status(
								&dsi_ctrl->hw);
			u32 mask = DSI_CMD_MODE_DMA_DONE;

			if (status & mask) {
				status |= (DSI_CMD_MODE_DMA_DONE |
						DSI_BTA_DONE);
				dsi_hw_ops.clear_interrupt_status(
								&dsi_ctrl->hw,
								status);
				dsi_ctrl_disable_status_interrupt(dsi_ctrl,
						DSI_SINT_CMD_MODE_DMA_DONE);
				complete_all(&dsi_ctrl->irq_info.cmd_dma_done);
				pr_warn("dma_tx done but irq not triggered\n");
			} else {
				rc = -ETIMEDOUT;
				dsi_ctrl_disable_status_interrupt(dsi_ctrl,
						DSI_SINT_CMD_MODE_DMA_DONE);
				pr_err("[DSI_%d]Command transfer failed\n",
						dsi_ctrl->cell_index);
			}
		}

		if (dsi_hw_ops.mask_error_intr && !dsi_ctrl->esd_check_underway)
			dsi_hw_ops.mask_error_intr(&dsi_ctrl->hw,
					BIT(DSI_FIFO_OVERFLOW), false);
		dsi_hw_ops.reset_cmd_fifo(&dsi_ctrl->hw);

		/*
		 * DSI 2.2 needs a soft reset whenever we send non-embedded
		 * mode command followed by embedded mode. Otherwise it will
		 * result in smmu write faults with DSI as client.
		 */
		if (flags & DSI_CTRL_CMD_NON_EMBEDDED_MODE) {
			dsi_hw_ops.soft_reset(&dsi_ctrl->hw);
			dsi_ctrl->cmd_len = 0;
		}
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
	u32 flags = DSI_CTRL_CMD_FETCH_MEMORY;
	struct mipi_dsi_msg msg = {
		.channel = rx_msg->channel,
		.type = MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE,
		.tx_len = 2,
		.tx_buf = tx,
	};

	rc = dsi_message_tx(dsi_ctrl, &msg, flags);
	if (rc)
		pr_err("failed to send max return size packet, rc=%d\n", rc);

	return rc;
}

/* Helper functions to support DCS read operation */
static int dsi_parse_short_read1_resp(const struct mipi_dsi_msg *msg,
		unsigned char *buff)
{
	u8 *data = msg->rx_buf;
	int read_len = 1;

	if (!data)
		return 0;

	/* remove dcs type */
	if (msg->rx_len >= 1)
		data[0] = buff[1];
	else
		read_len = 0;

	return read_len;
}

static int dsi_parse_short_read2_resp(const struct mipi_dsi_msg *msg,
		unsigned char *buff)
{
	u8 *data = msg->rx_buf;
	int read_len = 2;

	if (!data)
		return 0;

	/* remove dcs type */
	if (msg->rx_len >= 2) {
		data[0] = buff[1];
		data[1] = buff[2];
	} else {
		read_len = 0;
	}

	return read_len;
}

static int dsi_parse_long_read_resp(const struct mipi_dsi_msg *msg,
		unsigned char *buff)
{
	if (!msg->rx_buf)
		return 0;

	/* remove dcs type */
	if (msg->rx_buf && msg->rx_len)
		memcpy(msg->rx_buf, buff + 4, msg->rx_len);

	return msg->rx_len;
}

static int dsi_message_rx(struct dsi_ctrl *dsi_ctrl,
			  const struct mipi_dsi_msg *msg,
			  u32 flags)
{
	int rc = 0;
	u32 rd_pkt_size, total_read_len, hw_read_cnt;
	u32 current_read_len = 0, total_bytes_read = 0;
	bool short_resp = false;
	bool read_done = false;
	u32 dlen, diff, rlen;
	unsigned char *buff;
	char cmd;

	if (!msg) {
		pr_err("Invalid msg\n");
		rc = -EINVAL;
		goto error;
	}

	rlen = msg->rx_len;
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
	buff = msg->rx_buf;

	while (!read_done) {
		rc = dsi_set_max_return_size(dsi_ctrl, msg, rd_pkt_size);
		if (rc) {
			pr_err("Failed to set max return packet size, rc=%d\n",
			       rc);
			goto error;
		}

		/* clear RDBK_DATA registers before proceeding */
		dsi_ctrl->hw.ops.clear_rdbk_register(&dsi_ctrl->hw);

		rc = dsi_message_tx(dsi_ctrl, msg, flags);
		if (rc) {
			pr_err("Message transmission failed, rc=%d\n", rc);
			goto error;
		}
		/*
		 * wait before reading rdbk_data register, if any delay is
		 * required after sending the read command.
		 */
		if (msg->wait_ms)
			usleep_range(msg->wait_ms * 1000,
				     ((msg->wait_ms * 1000) + 10));

		dlen = dsi_ctrl->hw.ops.get_cmd_read_data(&dsi_ctrl->hw,
					buff, total_bytes_read,
					total_read_len, rd_pkt_size,
					&hw_read_cnt);
		if (!dlen)
			goto error;

		if (short_resp)
			break;

		if (rlen <= current_read_len) {
			diff = current_read_len - rlen;
			read_done = true;
		} else {
			diff = 0;
			rlen -= current_read_len;
		}

		dlen -= 2; /* 2 bytes of CRC */
		dlen -= diff;
		buff += dlen;
		total_bytes_read += dlen;
		if (!read_done) {
			current_read_len = 14; /* Not first read */
			if (rlen < current_read_len)
				rd_pkt_size += rlen;
			else
				rd_pkt_size += current_read_len;
		}
	}

	if (hw_read_cnt < 16 && !short_resp)
		buff = msg->rx_buf + (16 - hw_read_cnt);
	else
		buff = msg->rx_buf;

	/* parse the data read from panel */
	cmd = buff[0];
	switch (cmd) {
	case MIPI_DSI_RX_ACKNOWLEDGE_AND_ERROR_REPORT:
		pr_err("Rx ACK_ERROR\n");
		rc = 0;
		break;
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
		rc = dsi_parse_short_read1_resp(msg, buff);
		break;
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
		rc = dsi_parse_short_read2_resp(msg, buff);
		break;
	case MIPI_DSI_RX_GENERIC_LONG_READ_RESPONSE:
	case MIPI_DSI_RX_DCS_LONG_READ_RESPONSE:
		rc = dsi_parse_long_read_resp(msg, buff);
		break;
	default:
		pr_warn("Invalid response\n");
		rc = 0;
	}

error:
	return rc;
}

static int dsi_enable_ulps(struct dsi_ctrl *dsi_ctrl)
{
	int rc = 0;
	u32 lanes = 0;
	u32 ulps_lanes;

	lanes = dsi_ctrl->host_config.common_config.data_lanes;

	rc = dsi_ctrl->hw.ops.wait_for_lane_idle(&dsi_ctrl->hw, lanes);
	if (rc) {
		pr_err("lanes not entering idle, skip ULPS\n");
		return rc;
	}

	if (!dsi_ctrl->hw.ops.ulps_ops.ulps_request ||
			!dsi_ctrl->hw.ops.ulps_ops.ulps_exit) {
		pr_debug("DSI controller ULPS ops not present\n");
		return 0;
	}

	lanes |= DSI_CLOCK_LANE;
	dsi_ctrl->hw.ops.ulps_ops.ulps_request(&dsi_ctrl->hw, lanes);

	ulps_lanes = dsi_ctrl->hw.ops.ulps_ops.get_lanes_in_ulps(&dsi_ctrl->hw);

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

	dsi_ctrl->hw.ops.clear_phy0_ln_err(&dsi_ctrl->hw);

	if (!dsi_ctrl->hw.ops.ulps_ops.ulps_request ||
			!dsi_ctrl->hw.ops.ulps_ops.ulps_exit) {
		pr_debug("DSI controller ULPS ops not present\n");
		return 0;
	}

	lanes = dsi_ctrl->host_config.common_config.data_lanes;
	lanes |= DSI_CLOCK_LANE;

	ulps_lanes = dsi_ctrl->hw.ops.ulps_ops.get_lanes_in_ulps(&dsi_ctrl->hw);

	if ((lanes & ulps_lanes) != lanes)
		pr_err("Mismatch between lanes in ULPS\n");

	lanes &= ulps_lanes;

	dsi_ctrl->hw.ops.ulps_ops.ulps_exit(&dsi_ctrl->hw, lanes);

	ulps_lanes = dsi_ctrl->hw.ops.ulps_ops.get_lanes_in_ulps(&dsi_ctrl->hw);
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
		state->power_state = DSI_CTRL_POWER_VREG_OFF;
		state->cmd_engine_state = DSI_CTRL_ENGINE_OFF;
		state->vid_engine_state = DSI_CTRL_ENGINE_OFF;
	}

	return rc;
}

static int dsi_ctrl_buffer_deinit(struct dsi_ctrl *dsi_ctrl)
{
	struct msm_gem_address_space *aspace = NULL;

	if (dsi_ctrl->tx_cmd_buf) {
		aspace = dsi_ctrl_get_aspace(dsi_ctrl,
				MSM_SMMU_DOMAIN_UNSECURE);
		if (!aspace) {
			pr_err("failed to get address space\n");
			return -ENOMEM;
		}

		msm_gem_put_iova(dsi_ctrl->tx_cmd_buf, aspace);

		mutex_lock(&dsi_ctrl->drm_dev->struct_mutex);
		msm_gem_free_object(dsi_ctrl->tx_cmd_buf);
		mutex_unlock(&dsi_ctrl->drm_dev->struct_mutex);
		dsi_ctrl->tx_cmd_buf = NULL;
	}

	return 0;
}

int dsi_ctrl_buffer_init(struct dsi_ctrl *dsi_ctrl)
{
	int rc = 0;
	u64 iova = 0;
	struct msm_gem_address_space *aspace = NULL;

	aspace = dsi_ctrl_get_aspace(dsi_ctrl, MSM_SMMU_DOMAIN_UNSECURE);
	if (!aspace) {
		pr_err("failed to get address space\n");
		return -ENOMEM;
	}

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

	rc = msm_gem_get_iova(dsi_ctrl->tx_cmd_buf, aspace, &iova);
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

static int dsi_enable_io_clamp(struct dsi_ctrl *dsi_ctrl,
		bool enable, bool ulps_enabled)
{
	u32 lanes = 0;

	if (dsi_ctrl->host_config.panel_mode == DSI_OP_CMD_MODE)
		lanes = dsi_ctrl->host_config.common_config.data_lanes;

	lanes |= DSI_CLOCK_LANE;

	if (enable)
		dsi_ctrl->hw.ops.clamp_enable(&dsi_ctrl->hw,
			lanes, ulps_enabled);
	else
		dsi_ctrl->hw.ops.clamp_disable(&dsi_ctrl->hw,
			lanes, ulps_enabled);

	return 0;
}

static int dsi_ctrl_dts_parse(struct dsi_ctrl *dsi_ctrl,
				  struct device_node *of_node)
{
	u32 index = 0;
	int rc = 0;

	if (!dsi_ctrl || !of_node) {
		pr_err("invalid dsi_ctrl:%d or of_node:%d\n",
					dsi_ctrl != NULL, of_node != NULL);
		return -EINVAL;
	}

	rc = of_property_read_u32(of_node, "cell-index", &index);
	if (rc) {
		pr_debug("cell index not set, default to 0\n");
		index = 0;
	}

	dsi_ctrl->cell_index = index;
	dsi_ctrl->name = of_get_property(of_node, "label", NULL);
	if (!dsi_ctrl->name)
		dsi_ctrl->name = DSI_CTRL_DEFAULT_LABEL;

	dsi_ctrl->phy_isolation_enabled = of_property_read_bool(of_node,
				    "qcom,dsi-phy-isolation-enabled");

	dsi_ctrl->null_insertion_enabled = of_property_read_bool(of_node,
					"qcom,null-insertion-enabled");

	return 0;
}

static int dsi_ctrl_dev_probe(struct platform_device *pdev)
{
	struct dsi_ctrl *dsi_ctrl;
	struct dsi_ctrl_list_item *item;
	const struct of_device_id *id;
	enum dsi_ctrl_version version;
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

	dsi_ctrl->version = version;
	dsi_ctrl->irq_info.irq_num = -1;
	dsi_ctrl->irq_info.irq_stat_mask = 0x0;

	spin_lock_init(&dsi_ctrl->irq_info.irq_lock);

	rc = dsi_ctrl_dts_parse(dsi_ctrl, pdev->dev.of_node);
	if (rc) {
		pr_err("ctrl:%d dts parse failed, rc = %d\n",
						dsi_ctrl->cell_index, rc);
		goto fail;
	}

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

	rc = dsi_catalog_ctrl_setup(&dsi_ctrl->hw, dsi_ctrl->version,
		dsi_ctrl->cell_index, dsi_ctrl->phy_isolation_enabled,
		dsi_ctrl->null_insertion_enabled);
	if (rc) {
		pr_err("Catalog does not support version (%d)\n",
		       dsi_ctrl->version);
		goto fail_supplies;
	}

	rc = dsi_ctrl_axi_bus_client_init(pdev, dsi_ctrl);
	if (rc)
		pr_debug("failed to init axi bus client, rc = %d\n", rc);

	item->ctrl = dsi_ctrl;

	mutex_lock(&dsi_ctrl_list_lock);
	list_add(&item->list, &dsi_ctrl_list);
	mutex_unlock(&dsi_ctrl_list_lock);

	mutex_init(&dsi_ctrl->ctrl_lock);
	dsi_ctrl->secure_mode = false;

	dsi_ctrl->pdev = pdev;
	platform_set_drvdata(pdev, dsi_ctrl);
	pr_info("Probe successful for %s\n", dsi_ctrl->name);

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
		.suppress_bind_attrs = true,
	},
};

#if defined(CONFIG_DEBUG_FS)

void dsi_ctrl_debug_dump(void)
{
	struct list_head *pos, *tmp;
	struct dsi_ctrl *ctrl = NULL;

	mutex_lock(&dsi_ctrl_list_lock);
	list_for_each_safe(pos, tmp, &dsi_ctrl_list) {
		struct dsi_ctrl_list_item *n;

		n = list_entry(pos, struct dsi_ctrl_list_item, list);
		ctrl = n->ctrl;
		pr_err("dsi ctrl:%d\n", ctrl->cell_index);
		ctrl->hw.ops.debug_bus(&ctrl->hw);
	}
	mutex_unlock(&dsi_ctrl_list_lock);
}

#endif
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
		mutex_unlock(&ctrl->ctrl_lock);
		ctrl = ERR_PTR(-EBUSY);
		return ctrl;
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
		pr_err("Unbalanced %s call\n", __func__);
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
		       dsi_ctrl->cell_index, rc);
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

int dsi_ctrl_clk_cb_register(struct dsi_ctrl *dsi_ctrl,
	struct clk_ctrl_cb *clk_cb)
{
	if (!dsi_ctrl || !clk_cb) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	dsi_ctrl->clk_cb.priv = clk_cb->priv;
	dsi_ctrl->clk_cb.dsi_clk_cb = clk_cb->dsi_clk_cb;
	return 0;
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
		       dsi_ctrl->cell_index, rc);
		goto error;
	}

	dsi_ctrl->hw.ops.phy_sw_reset(&dsi_ctrl->hw);

	pr_debug("[DSI_%d] PHY soft reset done\n", dsi_ctrl->cell_index);
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
		       dsi_ctrl->cell_index, rc);
		goto exit;
	}

	host_mode = &dsi_ctrl->host_config.video_timing;
	memcpy(host_mode, timing, sizeof(*host_mode));
	dsi_ctrl->hw.ops.set_timing_db(&dsi_ctrl->hw, true);
	dsi_ctrl->hw.ops.set_video_timing(&dsi_ctrl->hw, host_mode);

exit:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_timing_db_update() - update only controller Timing DB
 * @dsi_ctrl:          DSI controller handle.
 * @enable:            Enable/disable Timing DB register
 *
 *  Update timing db register value during dfps usecases
 *
 * Return: error code.
 */
int dsi_ctrl_timing_db_update(struct dsi_ctrl *dsi_ctrl,
		bool enable)
{
	int rc = 0;

	if (!dsi_ctrl) {
		pr_err("Invalid dsi_ctrl\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_ASYNC_TIMING,
			DSI_CTRL_ENGINE_ON);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->cell_index, rc);
		goto exit;
	}

	/*
	 * Add HW recommended delay for dfps feature.
	 * When prefetch is enabled, MDSS HW works on 2 vsync
	 * boundaries i.e. mdp_vsync and panel_vsync.
	 * In the current implementation we are only waiting
	 * for mdp_vsync. We need to make sure that interface
	 * flush is after panel_vsync. So, added the recommended
	 * delays after dfps update.
	 */
	usleep_range(2000, 2010);

	dsi_ctrl->hw.ops.set_timing_db(&dsi_ctrl->hw, enable);

exit:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

int dsi_ctrl_setup(struct dsi_ctrl *dsi_ctrl)
{
	int rc = 0;

	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	dsi_ctrl->hw.ops.setup_lane_map(&dsi_ctrl->hw,
					&dsi_ctrl->host_config.lane_map);

	dsi_ctrl->hw.ops.host_setup(&dsi_ctrl->hw,
				    &dsi_ctrl->host_config.common_config);

	if (dsi_ctrl->host_config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_ctrl->hw.ops.cmd_engine_setup(&dsi_ctrl->hw,
					&dsi_ctrl->host_config.common_config,
					&dsi_ctrl->host_config.u.cmd_engine);

		dsi_ctrl->hw.ops.setup_cmd_stream(&dsi_ctrl->hw,
				&dsi_ctrl->host_config.video_timing,
				dsi_ctrl->host_config.video_timing.h_active * 3,
				0x0,
				&dsi_ctrl->roi);
		dsi_ctrl->hw.ops.cmd_engine_en(&dsi_ctrl->hw, true);
	} else {
		dsi_ctrl->hw.ops.video_engine_setup(&dsi_ctrl->hw,
					&dsi_ctrl->host_config.common_config,
					&dsi_ctrl->host_config.u.video_engine);
		dsi_ctrl->hw.ops.set_video_timing(&dsi_ctrl->hw,
					  &dsi_ctrl->host_config.video_timing);
		dsi_ctrl->hw.ops.video_engine_en(&dsi_ctrl->hw, true);
	}

	dsi_ctrl->hw.ops.enable_status_interrupts(&dsi_ctrl->hw, 0x0);
	dsi_ctrl->hw.ops.enable_error_interrupts(&dsi_ctrl->hw, 0xFF00E0);
	dsi_ctrl->hw.ops.ctrl_en(&dsi_ctrl->hw, true);

	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

int dsi_ctrl_set_roi(struct dsi_ctrl *dsi_ctrl, struct dsi_rect *roi,
		bool *changed)
{
	int rc = 0;

	if (!dsi_ctrl || !roi || !changed) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);
	if ((!dsi_rect_is_equal(&dsi_ctrl->roi, roi)) ||
			dsi_ctrl->modeupdated) {
		*changed = true;
		memcpy(&dsi_ctrl->roi, roi, sizeof(dsi_ctrl->roi));
		dsi_ctrl->modeupdated = false;
	} else
		*changed = false;

	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_phy_reset_config() - Mask/unmask propagation of ahb reset signal
 *	to DSI PHY hardware.
 * @dsi_ctrl:        DSI controller handle.
 * @enable:			Mask/unmask the PHY reset signal.
 *
 * Return: error code.
 */
int dsi_ctrl_phy_reset_config(struct dsi_ctrl *dsi_ctrl, bool enable)
{
	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (dsi_ctrl->hw.ops.phy_reset_config)
		dsi_ctrl->hw.ops.phy_reset_config(&dsi_ctrl->hw, enable);

	return 0;
}

static bool dsi_ctrl_check_for_spurious_error_interrupts(
					struct dsi_ctrl *dsi_ctrl)
{
	const unsigned long intr_check_interval = msecs_to_jiffies(1000);
	const unsigned int interrupt_threshold = 15;
	unsigned long jiffies_now = jiffies;

	if (!dsi_ctrl) {
		pr_err("Invalid DSI controller structure\n");
		return false;
	}

	if (dsi_ctrl->jiffies_start == 0)
		dsi_ctrl->jiffies_start = jiffies;

	dsi_ctrl->error_interrupt_count++;

	if ((jiffies_now - dsi_ctrl->jiffies_start) < intr_check_interval) {
		if (dsi_ctrl->error_interrupt_count > interrupt_threshold) {
			pr_warn("Detected spurious interrupts on dsi ctrl\n");
			return true;
		}
	} else {
		dsi_ctrl->jiffies_start = jiffies;
		dsi_ctrl->error_interrupt_count = 1;
	}
	return false;
}

static void dsi_ctrl_handle_error_status(struct dsi_ctrl *dsi_ctrl,
				unsigned long int error)
{
	struct dsi_event_cb_info cb_info;

	cb_info = dsi_ctrl->irq_info.irq_err_cb;

	/* disable error interrupts */
	if (dsi_ctrl->hw.ops.error_intr_ctrl)
		dsi_ctrl->hw.ops.error_intr_ctrl(&dsi_ctrl->hw, false);

	/* clear error interrupts first */
	if (dsi_ctrl->hw.ops.clear_error_status)
		dsi_ctrl->hw.ops.clear_error_status(&dsi_ctrl->hw,
					error);

	/* DTLN PHY error */
	if (error & 0x3000E00)
		pr_err("dsi PHY contention error: 0x%lx\n", error);

	/* TX timeout error */
	if (error & 0xE0) {
		if (error & 0xA0) {
			if (cb_info.event_cb) {
				cb_info.event_idx = DSI_LP_Rx_TIMEOUT;
				(void)cb_info.event_cb(cb_info.event_usr_ptr,
							cb_info.event_idx,
							dsi_ctrl->cell_index,
							0, 0, 0, 0);
			}
		}
		pr_err("tx timeout error: 0x%lx\n", error);
	}

	/* DSI FIFO OVERFLOW error */
	if (error & 0xF0000) {
		u32 mask = 0;

		if (dsi_ctrl->hw.ops.get_error_mask)
			mask = dsi_ctrl->hw.ops.get_error_mask(&dsi_ctrl->hw);
		/* no need to report FIFO overflow if already masked */
		if (cb_info.event_cb && !(mask & 0xf0000)) {
			cb_info.event_idx = DSI_FIFO_OVERFLOW;
			(void)cb_info.event_cb(cb_info.event_usr_ptr,
						cb_info.event_idx,
						dsi_ctrl->cell_index,
						0, 0, 0, 0);
			pr_err("dsi FIFO OVERFLOW error: 0x%lx\n", error);
		}
	}

	/* DSI FIFO UNDERFLOW error */
	if (error & 0xF00000) {
		if (cb_info.event_cb) {
			cb_info.event_idx = DSI_FIFO_UNDERFLOW;
			(void)cb_info.event_cb(cb_info.event_usr_ptr,
						cb_info.event_idx,
						dsi_ctrl->cell_index,
						0, 0, 0, 0);
		}
		pr_err("dsi FIFO UNDERFLOW error: 0x%lx\n", error);
	}

	/* DSI PLL UNLOCK error */
	if (error & BIT(8))
		pr_err("dsi PLL unlock error: 0x%lx\n", error);

	/* ACK error */
	if (error & 0xF)
		pr_err("ack error: 0x%lx\n", error);

	/*
	 * DSI Phy can go into bad state during ESD influence. This can
	 * manifest as various types of spurious error interrupts on
	 * DSI controller. This check will allow us to handle afore mentioned
	 * case and prevent us from re enabling interrupts until a full ESD
	 * recovery is completed.
	 */
	if (dsi_ctrl_check_for_spurious_error_interrupts(dsi_ctrl) &&
				dsi_ctrl->esd_check_underway) {
		dsi_ctrl->hw.ops.soft_reset(&dsi_ctrl->hw);
		return;
	}

	/* enable back DSI interrupts */
	if (dsi_ctrl->hw.ops.error_intr_ctrl)
		dsi_ctrl->hw.ops.error_intr_ctrl(&dsi_ctrl->hw, true);
}

/**
 * dsi_ctrl_isr - interrupt service routine for DSI CTRL component
 * @irq: Incoming IRQ number
 * @ptr: Pointer to user data structure (struct dsi_ctrl)
 * Returns: IRQ_HANDLED if no further action required
 */
static irqreturn_t dsi_ctrl_isr(int irq, void *ptr)
{
	struct dsi_ctrl *dsi_ctrl;
	struct dsi_event_cb_info cb_info;
	unsigned long flags;
	uint32_t status = 0x0, i;
	uint64_t errors = 0x0;

	if (!ptr)
		return IRQ_NONE;
	dsi_ctrl = ptr;

	/* check status interrupts */
	if (dsi_ctrl->hw.ops.get_interrupt_status)
		status = dsi_ctrl->hw.ops.get_interrupt_status(&dsi_ctrl->hw);

	/* check error interrupts */
	if (dsi_ctrl->hw.ops.get_error_status)
		errors = dsi_ctrl->hw.ops.get_error_status(&dsi_ctrl->hw);

	/* clear interrupts */
	if (dsi_ctrl->hw.ops.clear_interrupt_status)
		dsi_ctrl->hw.ops.clear_interrupt_status(&dsi_ctrl->hw, 0x0);

	SDE_EVT32_IRQ(dsi_ctrl->cell_index, status, errors);

	/* handle DSI error recovery */
	if (status & DSI_ERROR)
		dsi_ctrl_handle_error_status(dsi_ctrl, errors);

	if (status & DSI_CMD_MODE_DMA_DONE) {
		dsi_ctrl_disable_status_interrupt(dsi_ctrl,
					DSI_SINT_CMD_MODE_DMA_DONE);
		complete_all(&dsi_ctrl->irq_info.cmd_dma_done);
	}

	if (status & DSI_CMD_FRAME_DONE) {
		dsi_ctrl_disable_status_interrupt(dsi_ctrl,
					DSI_SINT_CMD_FRAME_DONE);
		complete_all(&dsi_ctrl->irq_info.cmd_frame_done);
	}

	if (status & DSI_VIDEO_MODE_FRAME_DONE) {
		dsi_ctrl_disable_status_interrupt(dsi_ctrl,
					DSI_SINT_VIDEO_MODE_FRAME_DONE);
		complete_all(&dsi_ctrl->irq_info.vid_frame_done);
	}

	if (status & DSI_BTA_DONE) {
		u32 fifo_overflow_mask = (DSI_DLN0_HS_FIFO_OVERFLOW |
					DSI_DLN1_HS_FIFO_OVERFLOW |
					DSI_DLN2_HS_FIFO_OVERFLOW |
					DSI_DLN3_HS_FIFO_OVERFLOW);
		dsi_ctrl_disable_status_interrupt(dsi_ctrl,
					DSI_SINT_BTA_DONE);
		complete_all(&dsi_ctrl->irq_info.bta_done);
		if (dsi_ctrl->hw.ops.clear_error_status)
			dsi_ctrl->hw.ops.clear_error_status(&dsi_ctrl->hw,
					fifo_overflow_mask);
	}

	for (i = 0; status && i < DSI_STATUS_INTERRUPT_COUNT; ++i) {
		if (status & 0x1) {
			spin_lock_irqsave(&dsi_ctrl->irq_info.irq_lock, flags);
			cb_info = dsi_ctrl->irq_info.irq_stat_cb[i];
			spin_unlock_irqrestore(
					&dsi_ctrl->irq_info.irq_lock, flags);

			if (cb_info.event_cb)
				(void)cb_info.event_cb(cb_info.event_usr_ptr,
						cb_info.event_idx,
						dsi_ctrl->cell_index,
						irq, 0, 0, 0);
		}
		status >>= 1;
	}

	return IRQ_HANDLED;
}

/**
 * _dsi_ctrl_setup_isr - register ISR handler
 * @dsi_ctrl: Pointer to associated dsi_ctrl structure
 * Returns: Zero on success
 */
static int _dsi_ctrl_setup_isr(struct dsi_ctrl *dsi_ctrl)
{
	int irq_num, rc;

	if (!dsi_ctrl)
		return -EINVAL;
	if (dsi_ctrl->irq_info.irq_num != -1)
		return 0;

	init_completion(&dsi_ctrl->irq_info.cmd_dma_done);
	init_completion(&dsi_ctrl->irq_info.vid_frame_done);
	init_completion(&dsi_ctrl->irq_info.cmd_frame_done);
	init_completion(&dsi_ctrl->irq_info.bta_done);

	irq_num = platform_get_irq(dsi_ctrl->pdev, 0);
	if (irq_num < 0) {
		pr_err("[DSI_%d] Failed to get IRQ number, %d\n",
				dsi_ctrl->cell_index, irq_num);
		rc = irq_num;
	} else {
		rc = devm_request_threaded_irq(&dsi_ctrl->pdev->dev, irq_num,
				dsi_ctrl_isr, NULL, 0, "dsi_ctrl", dsi_ctrl);
		if (rc) {
			pr_err("[DSI_%d] Failed to request IRQ, %d\n",
					dsi_ctrl->cell_index, rc);
		} else {
			dsi_ctrl->irq_info.irq_num = irq_num;
			disable_irq_nosync(irq_num);

			pr_info("[DSI_%d] IRQ %d registered\n",
					dsi_ctrl->cell_index, irq_num);
		}
	}
	return rc;
}

/**
 * _dsi_ctrl_destroy_isr - unregister ISR handler
 * @dsi_ctrl: Pointer to associated dsi_ctrl structure
 */
static void _dsi_ctrl_destroy_isr(struct dsi_ctrl *dsi_ctrl)
{
	if (!dsi_ctrl || !dsi_ctrl->pdev || dsi_ctrl->irq_info.irq_num < 0)
		return;

	if (dsi_ctrl->irq_info.irq_num != -1) {
		devm_free_irq(&dsi_ctrl->pdev->dev,
				dsi_ctrl->irq_info.irq_num, dsi_ctrl);
		dsi_ctrl->irq_info.irq_num = -1;
	}
}

void dsi_ctrl_enable_status_interrupt(struct dsi_ctrl *dsi_ctrl,
		uint32_t intr_idx, struct dsi_event_cb_info *event_info)
{
	unsigned long flags;

	if (!dsi_ctrl || dsi_ctrl->irq_info.irq_num == -1 ||
			intr_idx >= DSI_STATUS_INTERRUPT_COUNT)
		return;

	spin_lock_irqsave(&dsi_ctrl->irq_info.irq_lock, flags);

	if (dsi_ctrl->irq_info.irq_stat_refcount[intr_idx] == 0) {
		/* enable irq on first request */
		if (dsi_ctrl->irq_info.irq_stat_mask == 0)
			enable_irq(dsi_ctrl->irq_info.irq_num);

		/* update hardware mask */
		dsi_ctrl->irq_info.irq_stat_mask |= BIT(intr_idx);
		dsi_ctrl->hw.ops.enable_status_interrupts(&dsi_ctrl->hw,
				dsi_ctrl->irq_info.irq_stat_mask);
	}
	++(dsi_ctrl->irq_info.irq_stat_refcount[intr_idx]);

	if (event_info)
		dsi_ctrl->irq_info.irq_stat_cb[intr_idx] = *event_info;

	spin_unlock_irqrestore(&dsi_ctrl->irq_info.irq_lock, flags);
}

void dsi_ctrl_disable_status_interrupt(struct dsi_ctrl *dsi_ctrl,
		uint32_t intr_idx)
{
	unsigned long flags;

	if (!dsi_ctrl || dsi_ctrl->irq_info.irq_num == -1 ||
			intr_idx >= DSI_STATUS_INTERRUPT_COUNT)
		return;

	spin_lock_irqsave(&dsi_ctrl->irq_info.irq_lock, flags);

	if (dsi_ctrl->irq_info.irq_stat_refcount[intr_idx])
		if (--(dsi_ctrl->irq_info.irq_stat_refcount[intr_idx]) == 0) {
			dsi_ctrl->irq_info.irq_stat_mask &= ~BIT(intr_idx);
			dsi_ctrl->hw.ops.enable_status_interrupts(&dsi_ctrl->hw,
					dsi_ctrl->irq_info.irq_stat_mask);

			/* don't need irq if no lines are enabled */
			if (dsi_ctrl->irq_info.irq_stat_mask == 0)
				disable_irq_nosync(dsi_ctrl->irq_info.irq_num);
		}

	spin_unlock_irqrestore(&dsi_ctrl->irq_info.irq_lock, flags);
}

int dsi_ctrl_host_timing_update(struct dsi_ctrl *dsi_ctrl)
{
	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (dsi_ctrl->hw.ops.host_setup)
		dsi_ctrl->hw.ops.host_setup(&dsi_ctrl->hw,
				&dsi_ctrl->host_config.common_config);

	if (dsi_ctrl->host_config.panel_mode == DSI_OP_CMD_MODE) {
		if (dsi_ctrl->hw.ops.cmd_engine_setup)
			dsi_ctrl->hw.ops.cmd_engine_setup(&dsi_ctrl->hw,
					&dsi_ctrl->host_config.common_config,
					&dsi_ctrl->host_config.u.cmd_engine);

		if (dsi_ctrl->hw.ops.setup_cmd_stream)
			dsi_ctrl->hw.ops.setup_cmd_stream(&dsi_ctrl->hw,
				&dsi_ctrl->host_config.video_timing,
				dsi_ctrl->host_config.video_timing.h_active * 3,
				0x0, NULL);
	} else {
		pr_err("invalid panel mode for resolution switch\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * dsi_ctrl_update_host_init_state() - Update the host initialization state.
 * @dsi_ctrl:        DSI controller handle.
 * @enable:        boolean signifying host state.
 *
 * Update the host initialization status only while exiting from ulps during
 * suspend state.
 *
 * Return: error code.
 */
int dsi_ctrl_update_host_init_state(struct dsi_ctrl *dsi_ctrl, bool enable)
{
	int rc = 0;
	u32 state = enable ? 0x1 : 0x0;

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_HOST_INIT, state);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->cell_index, rc);
		return rc;
	}
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_HOST_INIT, state);
	return rc;
}

/**
 * dsi_ctrl_host_init() - Initialize DSI host hardware.
 * @dsi_ctrl:        DSI controller handle.
 * @is_splash_enabled:        boolean signifying splash status.
 *
 * Initializes DSI controller hardware with host configuration provided by
 * dsi_ctrl_update_host_config(). Initialization can be performed only during
 * DSI_CTRL_POWER_CORE_CLK_ON state and after the PHY SW reset has been
 * performed.
 *
 * Return: error code.
 */
int dsi_ctrl_host_init(struct dsi_ctrl *dsi_ctrl, bool is_splash_enabled)
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
		       dsi_ctrl->cell_index, rc);
		goto error;
	}

	/* For Splash usecases we omit hw operations as bootloader
	 * already takes care of them
	 */
	if (!is_splash_enabled) {
		dsi_ctrl->hw.ops.setup_lane_map(&dsi_ctrl->hw,
					&dsi_ctrl->host_config.lane_map);

		dsi_ctrl->hw.ops.host_setup(&dsi_ctrl->hw,
				    &dsi_ctrl->host_config.common_config);

		if (dsi_ctrl->host_config.panel_mode == DSI_OP_CMD_MODE) {
			dsi_ctrl->hw.ops.cmd_engine_setup(&dsi_ctrl->hw,
					&dsi_ctrl->host_config.common_config,
					&dsi_ctrl->host_config.u.cmd_engine);

			dsi_ctrl->hw.ops.setup_cmd_stream(&dsi_ctrl->hw,
				&dsi_ctrl->host_config.video_timing,
				dsi_ctrl->host_config.video_timing.h_active * 3,
				0x0,
				NULL);
		} else {
			dsi_ctrl->hw.ops.video_engine_setup(&dsi_ctrl->hw,
					&dsi_ctrl->host_config.common_config,
					&dsi_ctrl->host_config.u.video_engine);
			dsi_ctrl->hw.ops.set_video_timing(&dsi_ctrl->hw,
					  &dsi_ctrl->host_config.video_timing);
		}
	}

	dsi_ctrl->hw.ops.enable_status_interrupts(&dsi_ctrl->hw, 0x0);
	dsi_ctrl->hw.ops.enable_error_interrupts(&dsi_ctrl->hw, 0xFF00E0);

	pr_debug("[DSI_%d]Host initialization complete, continuous splash status:%d\n",
		dsi_ctrl->cell_index, is_splash_enabled);
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_HOST_INIT, 0x1);
error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_isr_configure() - API to register/deregister dsi isr
 * @dsi_ctrl:              DSI controller handle.
 * @enable:		   variable to control register/deregister isr
 */
void dsi_ctrl_isr_configure(struct dsi_ctrl *dsi_ctrl, bool enable)
{
	if (!dsi_ctrl)
		return;

	mutex_lock(&dsi_ctrl->ctrl_lock);
	if (enable)
		_dsi_ctrl_setup_isr(dsi_ctrl);
	else
		_dsi_ctrl_destroy_isr(dsi_ctrl);

	mutex_unlock(&dsi_ctrl->ctrl_lock);
}

int dsi_ctrl_soft_reset(struct dsi_ctrl *dsi_ctrl)
{
	if (!dsi_ctrl)
		return -EINVAL;

	mutex_lock(&dsi_ctrl->ctrl_lock);
	dsi_ctrl->hw.ops.soft_reset(&dsi_ctrl->hw);
	mutex_unlock(&dsi_ctrl->ctrl_lock);

	pr_debug("[DSI_%d]Soft reset complete\n", dsi_ctrl->cell_index);
	return 0;
}

int dsi_ctrl_reset(struct dsi_ctrl *dsi_ctrl, int mask)
{
	int rc = 0;

	if (!dsi_ctrl)
		return -EINVAL;

	mutex_lock(&dsi_ctrl->ctrl_lock);
	rc = dsi_ctrl->hw.ops.ctrl_reset(&dsi_ctrl->hw, mask);
	mutex_unlock(&dsi_ctrl->ctrl_lock);

	return rc;
}

int dsi_ctrl_get_hw_version(struct dsi_ctrl *dsi_ctrl)
{
	int rc = 0;

	if (!dsi_ctrl)
		return -EINVAL;

	mutex_lock(&dsi_ctrl->ctrl_lock);
	rc = dsi_ctrl->hw.ops.get_hw_version(&dsi_ctrl->hw);
	mutex_unlock(&dsi_ctrl->ctrl_lock);

	return rc;
}

int dsi_ctrl_vid_engine_en(struct dsi_ctrl *dsi_ctrl, bool on)
{
	int rc = 0;

	if (!dsi_ctrl)
		return -EINVAL;

	mutex_lock(&dsi_ctrl->ctrl_lock);
	dsi_ctrl->hw.ops.video_engine_en(&dsi_ctrl->hw, on);
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
		       dsi_ctrl->cell_index, rc);
		pr_err("driver state check failed, rc=%d\n", rc);
		goto error;
	}

	pr_debug("[DSI_%d] Host deinitization complete\n",
		dsi_ctrl->cell_index);
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
				int flags, void *clk_handle)
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

	if (!(flags & (DSI_MODE_FLAG_SEAMLESS | DSI_MODE_FLAG_VRR))) {
		rc = dsi_ctrl_update_link_freqs(ctrl, config, clk_handle);
		if (rc) {
			pr_err("[%s] failed to update link frequencies, rc=%d\n",
			       ctrl->name, rc);
			goto error;
		}
	}

	pr_debug("[DSI_%d]Host config updated\n", ctrl->cell_index);
	memcpy(&ctrl->host_config, config, sizeof(ctrl->host_config));
	ctrl->mode_bounds.x = ctrl->host_config.video_timing.h_active *
			ctrl->horiz_index;
	ctrl->mode_bounds.y = 0;
	ctrl->mode_bounds.w = ctrl->host_config.video_timing.h_active;
	ctrl->mode_bounds.h = ctrl->host_config.video_timing.v_active;
	memcpy(&ctrl->roi, &ctrl->mode_bounds, sizeof(ctrl->mode_bounds));
	ctrl->modeupdated = true;
	ctrl->roi.x = 0;
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
		       dsi_ctrl->cell_index, rc);
		goto error;
	}

	if (flags & DSI_CTRL_CMD_READ) {
		rc = dsi_message_rx(dsi_ctrl, msg, flags);
		if (rc <= 0)
			pr_err("read message failed read length, rc=%d\n", rc);
	} else {
		rc = dsi_message_tx(dsi_ctrl, msg, flags);
		if (rc)
			pr_err("command msg transfer failed, rc = %d\n", rc);
	}

	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_CMD_TX, 0x0);

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
	int rc = 0, ret = 0;
	u32 status = 0;
	u32 mask = (DSI_CMD_MODE_DMA_DONE);

	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	/* Dont trigger the command if this is not the last ocmmand */
	if (!(flags & DSI_CTRL_CMD_LAST_COMMAND))
		return rc;

	mutex_lock(&dsi_ctrl->ctrl_lock);

	if (!(flags & DSI_CTRL_CMD_BROADCAST_MASTER))
		dsi_ctrl->hw.ops.trigger_command_dma(&dsi_ctrl->hw);

	if ((flags & DSI_CTRL_CMD_BROADCAST) &&
		(flags & DSI_CTRL_CMD_BROADCAST_MASTER)) {
		dsi_ctrl_wait_for_video_done(dsi_ctrl);
		dsi_ctrl_enable_status_interrupt(dsi_ctrl,
					DSI_SINT_CMD_MODE_DMA_DONE, NULL);
		if (dsi_ctrl->hw.ops.mask_error_intr)
			dsi_ctrl->hw.ops.mask_error_intr(&dsi_ctrl->hw,
					BIT(DSI_FIFO_OVERFLOW), true);
		reinit_completion(&dsi_ctrl->irq_info.cmd_dma_done);

		/* trigger command */
		dsi_ctrl->hw.ops.trigger_command_dma(&dsi_ctrl->hw);

		ret = wait_for_completion_timeout(
				&dsi_ctrl->irq_info.cmd_dma_done,
				msecs_to_jiffies(DSI_CTRL_TX_TO_MS));

		if (ret == 0) {
			status = dsi_ctrl->hw.ops.get_interrupt_status(
								&dsi_ctrl->hw);
			if (status & mask) {
				status |= (DSI_CMD_MODE_DMA_DONE |
						DSI_BTA_DONE);
				dsi_ctrl->hw.ops.clear_interrupt_status(
								&dsi_ctrl->hw,
								status);
				dsi_ctrl_disable_status_interrupt(dsi_ctrl,
						DSI_SINT_CMD_MODE_DMA_DONE);
				complete_all(&dsi_ctrl->irq_info.cmd_dma_done);
				pr_warn("dma_tx done but irq not triggered\n");
			} else {
				rc = -ETIMEDOUT;
				dsi_ctrl_disable_status_interrupt(dsi_ctrl,
						DSI_SINT_CMD_MODE_DMA_DONE);
				pr_err("[DSI_%d]Command transfer failed\n",
						dsi_ctrl->cell_index);
			}
		}
		if (dsi_ctrl->hw.ops.mask_error_intr &&
				!dsi_ctrl->esd_check_underway)
			dsi_ctrl->hw.ops.mask_error_intr(&dsi_ctrl->hw,
					BIT(DSI_FIFO_OVERFLOW), false);

		if (flags & DSI_CTRL_CMD_NON_EMBEDDED_MODE) {
			dsi_ctrl->hw.ops.soft_reset(&dsi_ctrl->hw);
			dsi_ctrl->cmd_len = 0;
		}
	}

	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_cache_misr - Cache frame MISR value
 * @dsi_ctrl: Pointer to associated dsi_ctrl structure
 */
void dsi_ctrl_cache_misr(struct dsi_ctrl *dsi_ctrl)
{
	u32 misr;

	if (!dsi_ctrl || !dsi_ctrl->hw.ops.collect_misr)
		return;

	misr = dsi_ctrl->hw.ops.collect_misr(&dsi_ctrl->hw,
				dsi_ctrl->host_config.panel_mode);

	if (misr)
		dsi_ctrl->misr_cache = misr;

	pr_debug("DSI_%d misr_cache = %x\n", dsi_ctrl->cell_index,
		dsi_ctrl->misr_cache);
}

/**
 * dsi_ctrl_get_host_engine_init_state() - Return host init state
 * @dsi_ctrl:          DSI controller handle.
 * @state:             Controller initialization state
 *
 * Return: error code.
 */
int dsi_ctrl_get_host_engine_init_state(struct dsi_ctrl *dsi_ctrl,
		bool *state)
{
	if (!dsi_ctrl || !state) {
		pr_err("Invalid Params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);
	*state = dsi_ctrl->current_state.host_initialized;
	mutex_unlock(&dsi_ctrl->ctrl_lock);

	return 0;
}

/**
 * dsi_ctrl_update_host_engine_state_for_cont_splash() -
 *            set engine state for dsi controller during continuous splash
 * @dsi_ctrl:          DSI controller handle.
 * @state:             Engine state.
 *
 * Set host engine state for DSI controller during continuous splash.
 *
 * Return: error code.
 */
int dsi_ctrl_update_host_engine_state_for_cont_splash(struct dsi_ctrl *dsi_ctrl,
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
		       dsi_ctrl->cell_index, rc);
		goto error;
	}

	pr_debug("[DSI_%d] Set host engine state = %d\n", dsi_ctrl->cell_index,
		 state);
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_HOST_ENGINE, state);
error:
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

	if (!dsi_ctrl || (state >= DSI_CTRL_POWER_MAX)) {
		pr_err("Invalid Params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_POWER_STATE_CHANGE,
				  state);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->cell_index, rc);
		goto error;
	}

	if (state == DSI_CTRL_POWER_VREG_ON) {
		rc = dsi_ctrl_enable_supplies(dsi_ctrl, true);
		if (rc) {
			pr_err("[%d]failed to enable voltage supplies, rc=%d\n",
			       dsi_ctrl->cell_index, rc);
			goto error;
		}
	} else if (state == DSI_CTRL_POWER_VREG_OFF) {
		rc = dsi_ctrl_enable_supplies(dsi_ctrl, false);
		if (rc) {
			pr_err("[%d]failed to disable vreg supplies, rc=%d\n",
			       dsi_ctrl->cell_index, rc);
			goto error;
		}
	}

	pr_debug("[DSI_%d] Power state updated to %d\n", dsi_ctrl->cell_index,
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
		       dsi_ctrl->cell_index, rc);
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

	pr_debug("[DSI_%d]Set test pattern state=%d\n",
		dsi_ctrl->cell_index, on);
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
		       dsi_ctrl->cell_index, rc);
		goto error;
	}

	if (state == DSI_CTRL_ENGINE_ON)
		dsi_ctrl->hw.ops.ctrl_en(&dsi_ctrl->hw, true);
	else
		dsi_ctrl->hw.ops.ctrl_en(&dsi_ctrl->hw, false);

	pr_debug("[DSI_%d] Set host engine state = %d\n", dsi_ctrl->cell_index,
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

	rc = dsi_ctrl_check_state(dsi_ctrl, DSI_CTRL_OP_CMD_ENGINE, state);
	if (rc) {
		pr_err("[DSI_%d] Controller state check failed, rc=%d\n",
		       dsi_ctrl->cell_index, rc);
		goto error;
	}

	if (state == DSI_CTRL_ENGINE_ON)
		dsi_ctrl->hw.ops.cmd_engine_en(&dsi_ctrl->hw, true);
	else
		dsi_ctrl->hw.ops.cmd_engine_en(&dsi_ctrl->hw, false);

	pr_debug("[DSI_%d] Set cmd engine state = %d\n", dsi_ctrl->cell_index,
		 state);
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_CMD_ENGINE, state);
error:
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
		       dsi_ctrl->cell_index, rc);
		goto error;
	}

	on = (state == DSI_CTRL_ENGINE_ON) ? true : false;
	dsi_ctrl->hw.ops.video_engine_en(&dsi_ctrl->hw, on);

	/* perform a reset when turning off video engine */
	if (!on)
		dsi_ctrl->hw.ops.soft_reset(&dsi_ctrl->hw);

	pr_debug("[DSI_%d] Set video engine state = %d\n", dsi_ctrl->cell_index,
		 state);
	dsi_ctrl_update_state(dsi_ctrl, DSI_CTRL_OP_VID_ENGINE, state);
error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_set_ulps() - set ULPS state for DSI lanes.
 * @dsi_ctrl:		DSI controller handle.
 * @enable:		enable/disable ULPS.
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

	if (enable)
		rc = dsi_enable_ulps(dsi_ctrl);
	else
		rc = dsi_disable_ulps(dsi_ctrl);

	if (rc) {
		pr_err("[DSI_%d] Ulps state change(%d) failed, rc=%d\n",
			dsi_ctrl->cell_index, enable, rc);
		goto error;
	}
	pr_debug("[DSI_%d] ULPS state = %d\n", dsi_ctrl->cell_index, enable);

error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_set_clamp_state() - set clamp state for DSI phy
 * @dsi_ctrl:             DSI controller handle.
 * @enable:               enable/disable clamping.
 *
 * Clamps can be enabled/disabled while DSI controller is still turned on.
 *
 * Return: error code.
 */
int dsi_ctrl_set_clamp_state(struct dsi_ctrl *dsi_ctrl,
		bool enable, bool ulps_enabled)
{
	int rc = 0;

	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (!dsi_ctrl->hw.ops.clamp_enable ||
			!dsi_ctrl->hw.ops.clamp_disable) {
		pr_debug("No clamp control for DSI controller\n");
		return 0;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_enable_io_clamp(dsi_ctrl, enable, ulps_enabled);
	if (rc) {
		pr_err("[DSI_%d] Failed to enable IO clamp\n",
			dsi_ctrl->cell_index);
		goto error;
	}

	pr_debug("[DSI_%d] Clamp state = %d\n", dsi_ctrl->cell_index, enable);
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

	if (!dsi_ctrl || !source_clks) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_ctrl->ctrl_lock);

	rc = dsi_clk_update_parent(source_clks, &dsi_ctrl->clk_info.rcg_clks);
	if (rc) {
		pr_err("[DSI_%d]Failed to update link clk parent, rc=%d\n",
		       dsi_ctrl->cell_index, rc);
		(void)dsi_clk_update_parent(&dsi_ctrl->clk_info.pll_op_clks,
					    &dsi_ctrl->clk_info.rcg_clks);
		goto error;
	}

	dsi_ctrl->clk_info.pll_op_clks.byte_clk = source_clks->byte_clk;
	dsi_ctrl->clk_info.pll_op_clks.pixel_clk = source_clks->pixel_clk;

	pr_debug("[DSI_%d] Source clocks are updated\n", dsi_ctrl->cell_index);

error:
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return rc;
}

/**
 * dsi_ctrl_setup_misr() - Setup frame MISR
 * @dsi_ctrl:              DSI controller handle.
 * @enable:                enable/disable MISR.
 * @frame_count:           Number of frames to accumulate MISR.
 *
 * Return: error code.
 */
int dsi_ctrl_setup_misr(struct dsi_ctrl *dsi_ctrl,
			bool enable,
			u32 frame_count)
{
	if (!dsi_ctrl) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (!dsi_ctrl->hw.ops.setup_misr)
		return 0;

	mutex_lock(&dsi_ctrl->ctrl_lock);
	dsi_ctrl->misr_enable = enable;
	dsi_ctrl->hw.ops.setup_misr(&dsi_ctrl->hw,
			dsi_ctrl->host_config.panel_mode,
			enable, frame_count);
	mutex_unlock(&dsi_ctrl->ctrl_lock);
	return 0;
}

/**
 * dsi_ctrl_collect_misr() - Read frame MISR
 * @dsi_ctrl:              DSI controller handle.
 *
 * Return: MISR value.
 */
u32 dsi_ctrl_collect_misr(struct dsi_ctrl *dsi_ctrl)
{
	u32 misr;

	if (!dsi_ctrl || !dsi_ctrl->hw.ops.collect_misr)
		return 0;

	misr = dsi_ctrl->hw.ops.collect_misr(&dsi_ctrl->hw,
				dsi_ctrl->host_config.panel_mode);
	if (!misr)
		misr = dsi_ctrl->misr_cache;

	pr_debug("DSI_%d cached misr = %x, final = %x\n",
		dsi_ctrl->cell_index, dsi_ctrl->misr_cache, misr);

	return misr;
}

void dsi_ctrl_mask_error_status_interrupts(struct dsi_ctrl *dsi_ctrl, u32 idx,
		bool mask_enable)
{
	if (!dsi_ctrl || !dsi_ctrl->hw.ops.error_intr_ctrl
			|| !dsi_ctrl->hw.ops.clear_error_status) {
		pr_err("Invalid params\n");
		return;
	}

	/*
	 * Mask DSI error status interrupts and clear error status
	 * register
	 */
	mutex_lock(&dsi_ctrl->ctrl_lock);
	if (idx & BIT(DSI_ERR_INTR_ALL)) {
		/*
		 * The behavior of mask_enable is different in ctrl register
		 * and mask register and hence mask_enable is manipulated for
		 * selective error interrupt masking vs total error interrupt
		 * masking.
		 */

		dsi_ctrl->hw.ops.error_intr_ctrl(&dsi_ctrl->hw, !mask_enable);
		dsi_ctrl->hw.ops.clear_error_status(&dsi_ctrl->hw,
					DSI_ERROR_INTERRUPT_COUNT);
	} else {
		dsi_ctrl->hw.ops.mask_error_intr(&dsi_ctrl->hw, idx,
								mask_enable);
		dsi_ctrl->hw.ops.clear_error_status(&dsi_ctrl->hw,
					DSI_ERROR_INTERRUPT_COUNT);
	}
	mutex_unlock(&dsi_ctrl->ctrl_lock);
}

/**
 * dsi_ctrl_irq_update() - Put a irq vote to process DSI error
 *				interrupts at any time.
 * @dsi_ctrl:              DSI controller handle.
 * @enable:		   variable to enable/disable irq
 */
void dsi_ctrl_irq_update(struct dsi_ctrl *dsi_ctrl, bool enable)
{
	if (!dsi_ctrl)
		return;

	mutex_lock(&dsi_ctrl->ctrl_lock);
	if (enable)
		dsi_ctrl_enable_status_interrupt(dsi_ctrl,
					DSI_SINT_ERROR, NULL);
	else
		dsi_ctrl_disable_status_interrupt(dsi_ctrl,
					DSI_SINT_ERROR);

	mutex_unlock(&dsi_ctrl->ctrl_lock);
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
