// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/iopoll.h>
#include <linux/slab.h>
#include <media/cam_tfe.h>
#include <media/cam_defs.h>

#include "cam_top_tpg_core.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"
#include "cam_top_tpg_ver1.h"
#include "cam_top_tpg_ver2.h"
#include "cam_top_tpg_ver3.h"

int cam_top_tpg_get_format(
	uint32_t                                     in_format,
	uint32_t                                    *tpg_encode_format)
{
	int                                          rc = 0;

	switch (in_format) {
	case CAM_FORMAT_MIPI_RAW_6:
		*tpg_encode_format = 0;
		break;
	case CAM_FORMAT_MIPI_RAW_8:
		*tpg_encode_format = 1;
		break;
	case CAM_FORMAT_MIPI_RAW_10:
		*tpg_encode_format = 2;
		break;
	case CAM_FORMAT_MIPI_RAW_12:
		*tpg_encode_format = 3;
		break;
	case CAM_FORMAT_MIPI_RAW_14:
		*tpg_encode_format = 4;
		break;
	case CAM_FORMAT_MIPI_RAW_16:
		*tpg_encode_format = 5;
		break;
	default:
		CAM_ERR(CAM_ISP, "Unsupported input encode format %d",
			in_format);
		rc = -EINVAL;
	}
	return rc;
}

static int cam_top_tpg_release(void *hw_priv,
	void *release_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_top_tpg_hw           *tpg_hw;
	struct cam_hw_info              *tpg_hw_info;
	struct cam_top_tpg_cfg          *tpg_data;
	struct cam_isp_resource_node    *tpg_res;

	if (!hw_priv || !release_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "TPG: Invalid args");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info  *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw   *)tpg_hw_info->core_info;
	tpg_res = (struct cam_isp_resource_node *)release_args;

	mutex_lock(&tpg_hw->hw_info->hw_mutex);
	if ((tpg_res->res_type != CAM_ISP_RESOURCE_TPG) ||
		(tpg_res->res_state <= CAM_ISP_RESOURCE_STATE_AVAILABLE)) {
		CAM_ERR(CAM_ISP, "TPG:%d Invalid res type:%d res_state:%d",
			tpg_hw->hw_intf->hw_idx, tpg_res->res_type,
			tpg_res->res_state);
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ISP, "TPG:%d res type :%d",
		tpg_hw->hw_intf->hw_idx, tpg_res->res_type);

	tpg_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	tpg_data = (struct cam_top_tpg_cfg *)tpg_res->res_priv;
	memset(tpg_data, 0, sizeof(struct cam_top_tpg_cfg));

end:
	mutex_unlock(&tpg_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_top_tpg_init_hw(void *hw_priv,
	void *init_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_top_tpg_hw                  *tpg_hw;
	struct cam_hw_info                     *tpg_hw_info;
	struct cam_isp_resource_node           *tpg_res;
	struct cam_hw_soc_info                 *soc_info;
	uint32_t clk_lvl;

	if (!hw_priv || !init_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "TPG: Invalid args");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info  *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw   *)tpg_hw_info->core_info;
	tpg_res = (struct cam_isp_resource_node *)init_args;
	soc_info = &tpg_hw->hw_info->soc_info;

	if (tpg_res->res_type != CAM_ISP_RESOURCE_TPG) {
		CAM_ERR(CAM_ISP, "TPG:%d Invalid res type state %d",
			tpg_hw->hw_intf->hw_idx,
			tpg_res->res_type);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "TPG:%d init HW res type :%d",
		tpg_hw->hw_intf->hw_idx, tpg_res->res_type);
	mutex_lock(&tpg_hw->hw_info->hw_mutex);
	/* overflow check before increment */
	if (tpg_hw->hw_info->open_count == UINT_MAX) {
		CAM_ERR(CAM_ISP, "TPG:%d Open count reached max",
			tpg_hw->hw_intf->hw_idx);
		mutex_unlock(&tpg_hw->hw_info->hw_mutex);
		return -EINVAL;
	}

	/* Increment ref Count */
	tpg_hw->hw_info->open_count++;
	if (tpg_hw->hw_info->open_count > 1) {
		CAM_DBG(CAM_ISP, "TPG hw has already been enabled");
		mutex_unlock(&tpg_hw->hw_info->hw_mutex);
		return rc;
	}

	rc = cam_soc_util_get_clk_level(soc_info, tpg_hw->clk_rate,
		soc_info->src_clk_idx, &clk_lvl);
	CAM_DBG(CAM_ISP, "TPG phy clock level %u", clk_lvl);

	rc = cam_top_tpg_enable_soc_resources(soc_info, clk_lvl);
	if (rc) {
		CAM_ERR(CAM_ISP, "TPG:%d Enable SOC failed",
			tpg_hw->hw_intf->hw_idx);
		goto err;
	}

	tpg_hw->hw_info->hw_state = CAM_HW_STATE_POWER_UP;

	mutex_unlock(&tpg_hw->hw_info->hw_mutex);
	return rc;

err:
	tpg_hw->hw_info->open_count--;
	mutex_unlock(&tpg_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_top_tpg_deinit_hw(void *hw_priv,
	void *deinit_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_top_tpg_hw                 *tpg_hw;
	struct cam_hw_info                    *tpg_hw_info;
	struct cam_isp_resource_node          *tpg_res;
	struct cam_hw_soc_info                *soc_info;

	if (!hw_priv || !deinit_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "TPG:Invalid arguments");
		return -EINVAL;
	}

	tpg_res = (struct cam_isp_resource_node *)deinit_args;
	tpg_hw_info = (struct cam_hw_info  *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw   *)tpg_hw_info->core_info;

	if (tpg_res->res_type != CAM_ISP_RESOURCE_TPG) {
		CAM_ERR(CAM_ISP, "TPG:%d Invalid Res type %d",
			tpg_hw->hw_intf->hw_idx,
			tpg_res->res_type);
		return -EINVAL;
	}

	mutex_lock(&tpg_hw->hw_info->hw_mutex);
	/* Check for refcount */
	if (!tpg_hw->hw_info->open_count) {
		CAM_WARN(CAM_ISP, "Unbalanced disable_hw");
		goto end;
	}

	/* Decrement ref Count */
	tpg_hw->hw_info->open_count--;
	if (tpg_hw->hw_info->open_count) {
		rc = 0;
		goto end;
	}

	soc_info = &tpg_hw->hw_info->soc_info;
	rc = cam_top_tpg_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_ISP, "TPG:%d Disable SOC failed",
			tpg_hw->hw_intf->hw_idx);

	tpg_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	CAM_DBG(CAM_ISP, "TPG:%d deint completed", tpg_hw->hw_intf->hw_idx);

end:
	mutex_unlock(&tpg_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_top_tpg_read(void *hw_priv,
	void *read_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "TPG: un supported");

	return -EINVAL;
}

static int cam_top_tpg_write(void *hw_priv,
	void *write_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "TPG: un supported");
	return -EINVAL;
}

static int cam_top_tpg_set_phy_clock(
	struct cam_top_tpg_hw *csid_hw, void *cmd_args)
{
	struct cam_top_tpg_clock_update_args *clk_update = NULL;

	if (!csid_hw)
		return -EINVAL;

	clk_update =
		(struct cam_top_tpg_clock_update_args *)cmd_args;

	csid_hw->clk_rate = clk_update->clk_rate;
	CAM_DBG(CAM_ISP, "CSI PHY clock rate %llu", csid_hw->clk_rate);

	return 0;
}

irqreturn_t cam_top_tpg_irq(int irq_num, void *data)
{
	struct cam_top_tpg_hw                        *tpg_hw;
	struct cam_hw_soc_info                       *soc_info;
	struct cam_top_tpg_ver3_reg_offset           *tpg_reg;
	uint32_t                                      irq_status_top;
	unsigned long                                 flags;

	if (!data) {
		CAM_ERR(CAM_ISP, "TPG: Invalid arguments");
		return IRQ_HANDLED;
	}


	tpg_hw = (struct cam_top_tpg_hw *) data;
	CAM_DBG(CAM_ISP, "CPHY TPG %d IRQ Handling", tpg_hw->hw_intf->hw_idx);

	soc_info = &tpg_hw->hw_info->soc_info;
	tpg_reg = tpg_hw->tpg_info->tpg_reg;

	irq_status_top = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		tpg_reg->tpg_top_irq_status);

	spin_lock_irqsave(&tpg_hw->hw_info->hw_lock, flags);
	/* clear */
	cam_io_w_mb(irq_status_top, soc_info->reg_map[0].mem_base +
		tpg_reg->tpg_top_irq_clear);
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		tpg_reg->tpg_top_irq_cmd);

	spin_unlock_irqrestore(&tpg_hw->hw_info->hw_lock, flags);

	CAM_DBG(CAM_ISP, "TPG: irq_status_top = 0x%x", irq_status_top);
	return IRQ_HANDLED;
}

static int cam_top_tpg_set_top_tpg_pattern(struct cam_top_tpg_hw *tpg_hw,
	void *cmd_args)
{
	uint32_t *top_tpg_pattern;

	top_tpg_pattern = (uint32_t  *) cmd_args;
	tpg_hw->tpg_pattern = *top_tpg_pattern;
	CAM_DBG(CAM_ISP, "TPG:%d set tpg debug value:%d",
		tpg_hw->hw_intf->hw_idx, tpg_hw->tpg_pattern);

	return 0;
}

static int cam_top_tpg_process_cmd(void *hw_priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_top_tpg_hw               *tpg_hw;
	struct cam_hw_info                  *tpg_hw_info;

	if (!hw_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid arguments");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info  *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw   *)tpg_hw_info->core_info;

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_TPG_PHY_CLOCK_UPDATE:
		rc = cam_top_tpg_set_phy_clock(tpg_hw, cmd_args);
		break;
	case CAM_ISP_HW_CMD_TPG_SET_PATTERN:
		rc = cam_top_tpg_set_top_tpg_pattern(tpg_hw, cmd_args);
		break;
	default:
		CAM_ERR(CAM_ISP, "TPG:%d unsupported cmd:%d",
			tpg_hw->hw_intf->hw_idx, cmd_type);
		rc = -EINVAL;
		break;
	}

	return 0;
}

int cam_top_tpg_probe_init(struct cam_hw_intf  *tpg_hw_intf,
	uint32_t tpg_idx)
{
	int rc = -EINVAL;
	struct cam_top_tpg_cfg             *tpg_data;
	struct cam_hw_info                 *tpg_hw_info;
	struct cam_top_tpg_hw              *tpg_hw = NULL;
	uint32_t hw_version = 0;

	if (tpg_idx >= CAM_TOP_TPG_HW_NUM_MAX) {
		CAM_ERR(CAM_ISP, "Invalid tpg index:%d", tpg_idx);
		return rc;
	}

	tpg_hw_info = (struct cam_hw_info  *)tpg_hw_intf->hw_priv;
	tpg_hw      = (struct cam_top_tpg_hw  *)tpg_hw_info->core_info;

	tpg_hw->hw_intf = tpg_hw_intf;
	tpg_hw->hw_info = tpg_hw_info;

	CAM_DBG(CAM_ISP, "type %d index %d",
		tpg_hw->hw_intf->hw_type, tpg_idx);

	tpg_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&tpg_hw->hw_info->hw_mutex);
	spin_lock_init(&tpg_hw->hw_info->hw_lock);
	spin_lock_init(&tpg_hw->lock_state);
	init_completion(&tpg_hw->hw_info->hw_complete);
	init_completion(&tpg_hw->tpg_complete);

	rc = cam_top_tpg_init_soc_resources(&tpg_hw->hw_info->soc_info,
		cam_top_tpg_irq, tpg_hw);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "TPG:%d Failed to init_soc", tpg_idx);
		goto err;
	}

	tpg_hw->hw_intf->hw_ops.init        = cam_top_tpg_init_hw;
	tpg_hw->hw_intf->hw_ops.deinit      = cam_top_tpg_deinit_hw;
	tpg_hw->hw_intf->hw_ops.reset       = NULL;
	tpg_hw->hw_intf->hw_ops.release     = cam_top_tpg_release;
	tpg_hw->hw_intf->hw_ops.read        = cam_top_tpg_read;
	tpg_hw->hw_intf->hw_ops.write       = cam_top_tpg_write;
	tpg_hw->hw_intf->hw_ops.process_cmd = cam_top_tpg_process_cmd;

	hw_version = tpg_hw->tpg_info->hw_dts_version;
	if (hw_version == CAM_TOP_TPG_VERSION_1)
		cam_top_tpg_ver1_init(tpg_hw);
	else if (hw_version == CAM_TOP_TPG_VERSION_2)
		cam_top_tpg_ver2_init(tpg_hw);
	else if (hw_version == CAM_TOP_TPG_VERSION_3)
		cam_top_tpg_ver3_init(tpg_hw);

	tpg_hw->tpg_res.res_type = CAM_ISP_RESOURCE_TPG;
	tpg_hw->tpg_res.res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	tpg_hw->tpg_res.hw_intf = tpg_hw->hw_intf;
	tpg_data = kzalloc(sizeof(*tpg_data), GFP_KERNEL);
	if (!tpg_data) {
		rc = -ENOMEM;
		goto err;
	}
	tpg_hw->tpg_res.res_priv = tpg_data;

	cam_top_tpg_enable_soc_resources(&tpg_hw->hw_info->soc_info,
		CAM_SVS_VOTE);

	cam_top_tpg_disable_soc_resources(&tpg_hw->hw_info->soc_info);

err:
	return rc;
}

int cam_top_tpg_deinit(struct cam_top_tpg_hw *top_tpg_hw)
{
	int rc = -EINVAL;

	if (!top_tpg_hw) {
		CAM_ERR(CAM_ISP, "Invalid param");
		return rc;
	}

	/* release the privdate data memory from resources */
	kfree(top_tpg_hw->tpg_res.res_priv);
	cam_top_tpg_deinit_soc_resources(&top_tpg_hw->hw_info->soc_info);

	return 0;
}
