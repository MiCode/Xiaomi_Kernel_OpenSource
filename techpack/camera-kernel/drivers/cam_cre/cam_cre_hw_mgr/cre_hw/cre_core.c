// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/timer.h>

#include "cam_io_util.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cre_core.h"
#include "cre_soc.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_cre_hw_intf.h"
#include "cam_cre_hw_mgr_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"
#include "cre_dev_intf.h"
#include "cam_compat.h"
#include "cre_bus_wr.h"
#include "cre_bus_rd.h"

#define CAM_CRE_RESET_TIMEOUT msecs_to_jiffies(500)

struct cam_cre_irq_data irq_data;

static int cam_cre_caps_vote(struct cam_cre_device_core_info *core_info,
	struct cam_cre_dev_bw_update *cpas_vote)
{
	int rc = 0;

	if (cpas_vote->ahb_vote_valid)
		rc = cam_cpas_update_ahb_vote(core_info->cpas_handle,
			&cpas_vote->ahb_vote);
	if (cpas_vote->axi_vote_valid)
		rc = cam_cpas_update_axi_vote(core_info->cpas_handle,
			&cpas_vote->axi_vote);
	if (rc)
		CAM_ERR(CAM_CRE, "cpas vote is failed: %d", rc);

	return rc;
}

int cam_cre_get_hw_caps(void *hw_priv, void *get_hw_cap_args,
	uint32_t arg_size)
{
	struct cam_hw_info *cre_dev = hw_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_cre_device_core_info *core_info = NULL;
	struct cam_cre_hw_ver *cre_hw_ver;
	struct cam_cre_top_reg_val *top_reg_val;

	if (!hw_priv) {
		CAM_ERR(CAM_CRE, "Invalid cam_dev_info");
		return -EINVAL;
	}

	soc_info = &cre_dev->soc_info;
	core_info = (struct cam_cre_device_core_info *)cre_dev->core_info;

	if ((!soc_info) || (!core_info)) {
		CAM_ERR(CAM_CRE, "soc_info = %x core_info = %x",
			soc_info, core_info);
		return -EINVAL;
	}

	if (!get_hw_cap_args) {
		CAM_ERR(CAM_CRE, "Invalid caps");
		return -EINVAL;
	}

	top_reg_val = core_info->cre_hw_info->cre_hw->top_reg_val;
	cre_hw_ver = get_hw_cap_args;
	cre_hw_ver->hw_ver.major =
		(core_info->hw_version & top_reg_val->major_mask) >>
		top_reg_val->major_shift;
	cre_hw_ver->hw_ver.minor =
		(core_info->hw_version & top_reg_val->minor_mask) >>
		top_reg_val->minor_shift;

	return 0;
}

static int cam_cre_dev_process_init(struct cam_cre_hw *cre_hw,
	void *cmd_args)
{
	int rc = 0;

	rc = cam_cre_top_process(cre_hw, 0, CRE_HW_INIT, cmd_args);
	if (rc)
		goto top_init_fail;

	rc = cam_cre_bus_wr_process(cre_hw, 0, CRE_HW_INIT, cmd_args);
		if (rc)
			goto bus_wr_init_fail;

	rc = cam_cre_bus_rd_process(cre_hw, 0, CRE_HW_INIT, cmd_args);
		if (rc)
			goto bus_rd_init_fail;

	return rc;

bus_rd_init_fail:
	rc = cam_cre_bus_wr_process(cre_hw, 0,
		CRE_HW_DEINIT, NULL);
bus_wr_init_fail:
	rc = cam_cre_top_process(cre_hw, 0,
		CRE_HW_DEINIT, NULL);
top_init_fail:
	return rc;
}

static int cam_cre_process_init(struct cam_cre_hw *cre_hw,
	void *cmd_args)
{
	return cam_cre_dev_process_init(cre_hw, cmd_args);
}

int cam_cre_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *cre_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_cre_device_core_info *core_info = NULL;
	struct cam_cre_cpas_vote *cpas_vote;
	int rc = 0;
	struct cam_cre_dev_init *init;
	struct cam_cre_hw *cre_hw;

	if (!device_priv) {
		CAM_ERR(CAM_CRE, "Invalid cam_dev_info");
		rc = -EINVAL;
		goto end;
	}

	soc_info = &cre_dev->soc_info;
	core_info = (struct cam_cre_device_core_info *)cre_dev->core_info;
	if ((!soc_info) || (!core_info)) {
		CAM_ERR(CAM_CRE, "soc_info = %pK core_info = %pK",
			soc_info, core_info);
		rc = -EINVAL;
		goto end;
	}
	cre_hw = core_info->cre_hw_info->cre_hw;

	cpas_vote = kzalloc(sizeof(struct cam_cre_cpas_vote), GFP_KERNEL);
	if (!cpas_vote) {
		CAM_ERR(CAM_ISP, "Out of memory");
		rc = -ENOMEM;
		goto end;
	}

	cpas_vote->ahb_vote.type = CAM_VOTE_ABSOLUTE;
	cpas_vote->ahb_vote.vote.level = CAM_SVS_VOTE;
	cpas_vote->axi_vote.num_paths = 1;
	cpas_vote->axi_vote.axi_path[0].path_data_type =
		CAM_AXI_PATH_DATA_ALL;
	cpas_vote->axi_vote.axi_path[0].transac_type =
		CAM_AXI_TRANSACTION_WRITE;
	cpas_vote->axi_vote.axi_path[0].camnoc_bw =
		CAM_CPAS_DEFAULT_AXI_BW;
	cpas_vote->axi_vote.axi_path[0].mnoc_ab_bw =
		CAM_CPAS_DEFAULT_AXI_BW;
	cpas_vote->axi_vote.axi_path[0].mnoc_ib_bw =
		CAM_CPAS_DEFAULT_AXI_BW;
	cpas_vote->axi_vote.axi_path[0].ddr_ab_bw =
		CAM_CPAS_DEFAULT_AXI_BW;
	cpas_vote->axi_vote.axi_path[0].ddr_ib_bw =
		CAM_CPAS_DEFAULT_AXI_BW;

	rc = cam_cpas_start(core_info->cpas_handle,
		&cpas_vote->ahb_vote, &cpas_vote->axi_vote);
	if (rc) {
		CAM_ERR(CAM_CRE, "cpass start failed: %d", rc);
		goto free_cpas_vote;
	}
	core_info->cpas_start = true;

	rc = cam_cre_enable_soc_resources(soc_info);
	if (rc)
		goto enable_soc_resource_failed;
	else
		core_info->clk_enable = true;

	init = init_hw_args;

	init->core_info = core_info;
	rc = cam_cre_process_init(cre_hw, init_hw_args);
	if (rc)
		goto process_init_failed;
	else
		goto free_cpas_vote;

process_init_failed:
	if (cam_cre_disable_soc_resources(soc_info))
		CAM_ERR(CAM_CRE, "disable soc resource failed");
enable_soc_resource_failed:
	if (cam_cpas_stop(core_info->cpas_handle))
		CAM_ERR(CAM_CRE, "cpas stop is failed");
	else
		core_info->cpas_start = false;
free_cpas_vote:
	cam_free_clear((void *)cpas_vote);
	cpas_vote = NULL;
end:
	return rc;
}

int cam_cre_deinit_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *cre_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_cre_device_core_info *core_info = NULL;
	int rc = 0;

	if (!device_priv) {
		CAM_ERR(CAM_CRE, "Invalid cam_dev_info");
		return -EINVAL;
	}

	soc_info = &cre_dev->soc_info;
	core_info = (struct cam_cre_device_core_info *)cre_dev->core_info;
	if ((!soc_info) || (!core_info)) {
		CAM_ERR(CAM_CRE, "soc_info = %pK core_info = %pK",
			soc_info, core_info);
		return -EINVAL;
	}

	rc = cam_cre_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_CRE, "soc disable is failed : %d", rc);
	core_info->clk_enable = false;

	if (cam_cpas_stop(core_info->cpas_handle))
		CAM_ERR(CAM_CRE, "cpas stop is failed");
	else
		core_info->cpas_start = false;

	return rc;
}

static int cam_cre_dev_process_dump_debug_reg(struct cam_cre_hw *cre_hw)
{
	int rc = 0;

	rc = cam_cre_top_process(cre_hw, -1,
		CRE_HW_DUMP_DEBUG, NULL);

	return rc;
}

static int cam_cre_dev_process_reset(struct cam_cre_hw *cre_hw, void *cmd_args)
{
	int rc = 0;

	rc = cam_cre_top_process(cre_hw, -1,
		CRE_HW_RESET, NULL);

	return rc;
}

static int cam_cre_dev_process_release(struct cam_cre_hw *cre_hw, void *cmd_args)
{
	int rc = 0;
	struct cam_cre_dev_release *cre_dev_release;

	cre_dev_release = cmd_args;
	rc = cam_cre_top_process(cre_hw, cre_dev_release->ctx_id,
		CRE_HW_RELEASE, NULL);

	rc |= cam_cre_bus_wr_process(cre_hw, cre_dev_release->ctx_id,
		CRE_HW_RELEASE, NULL);

	rc |= cam_cre_bus_rd_process(cre_hw, cre_dev_release->ctx_id,
		CRE_HW_RELEASE, NULL);

	return rc;
}

static int cam_cre_dev_process_acquire(struct cam_cre_hw *cre_hw, void *cmd_args)
{
	int rc = 0;
	struct cam_cre_dev_acquire *cre_dev_acquire;

	if (!cmd_args || !cre_hw) {
		CAM_ERR(CAM_CRE, "Invalid arguments: %pK %pK",
		cmd_args, cre_hw);
		return -EINVAL;
	}

	cre_dev_acquire = cmd_args;
	rc = cam_cre_top_process(cre_hw, cre_dev_acquire->ctx_id,
		CRE_HW_ACQUIRE, cre_dev_acquire);
	if (rc)
		goto top_acquire_fail;

	rc = cam_cre_bus_wr_process(cre_hw, cre_dev_acquire->ctx_id,
		CRE_HW_ACQUIRE, cre_dev_acquire->cre_acquire);
	if (rc)
		goto bus_wr_acquire_fail;

	rc = cam_cre_bus_rd_process(cre_hw, cre_dev_acquire->ctx_id,
		CRE_HW_ACQUIRE, cre_dev_acquire->cre_acquire);
	if (rc)
		goto bus_rd_acquire_fail;

	return 0;

bus_rd_acquire_fail:
	cam_cre_bus_wr_process(cre_hw, cre_dev_acquire->ctx_id,
		CRE_HW_RELEASE, cre_dev_acquire->cre_acquire);
bus_wr_acquire_fail:
	cam_cre_top_process(cre_hw, cre_dev_acquire->ctx_id,
		CRE_HW_RELEASE, cre_dev_acquire->cre_acquire);
top_acquire_fail:
	return rc;
}

static int cam_cre_dev_process_reg_set_update(struct cam_cre_hw *cre_hw, void *cmd_args)
{
	int rc = 0;
	struct cam_cre_dev_reg_set_update *reg_set_update;

	reg_set_update = cmd_args;

	rc = cam_cre_top_process(cre_hw, 0,
		CRE_HW_REG_SET_UPDATE, reg_set_update);
	if (rc)
		goto end;

	rc = cam_cre_bus_wr_process(cre_hw, 0,
		CRE_HW_REG_SET_UPDATE, reg_set_update);
	if (rc)
		goto end;

	rc = cam_cre_bus_rd_process(cre_hw, 0,
		CRE_HW_REG_SET_UPDATE, reg_set_update);
	if (rc)
		goto end;

end:
	return rc;
}

static int cam_cre_dev_process_prepare(struct cam_cre_hw *cre_hw, void *cmd_args)
{
	int rc = 0;
	struct cam_cre_dev_prepare_req *cre_dev_prepare_req;

	cre_dev_prepare_req = cmd_args;

	rc = cam_cre_top_process(cre_hw, cre_dev_prepare_req->ctx_data->ctx_id,
		CRE_HW_PREPARE, cre_dev_prepare_req);
	if (rc)
		goto end;

	rc = cam_cre_bus_wr_process(cre_hw,
		cre_dev_prepare_req->ctx_data->ctx_id,
		CRE_HW_PREPARE, cre_dev_prepare_req);
	if (rc)
		goto end;

	rc = cam_cre_bus_rd_process(cre_hw,
		cre_dev_prepare_req->ctx_data->ctx_id,
		CRE_HW_PREPARE, cre_dev_prepare_req);
	if (rc)
		goto end;

end:
	return rc;
}

static int cam_cre_dev_process_probe(struct cam_cre_hw *cre_hw,
	void *cmd_args)
{
	cam_cre_top_process(cre_hw, -1, CRE_HW_PROBE, NULL);
	cam_cre_bus_wr_process(cre_hw, -1, CRE_HW_PROBE, NULL);
	cam_cre_bus_rd_process(cre_hw, -1, CRE_HW_PROBE, NULL);

	return 0;
}

static int cam_cre_process_probe(struct cam_cre_hw *cre_hw,
	void *cmd_args)
{
	return cam_cre_dev_process_probe(cre_hw, cmd_args);
}

static int cam_cre_process_dump_debug_reg(struct cam_cre_hw *cre_hw)
{
	return cam_cre_dev_process_dump_debug_reg(cre_hw);
}

static int cam_cre_process_reset(struct cam_cre_hw *cre_hw,
	void *cmd_args)
{
	return cam_cre_dev_process_reset(cre_hw, cmd_args);
}

static int cam_cre_process_release(struct cam_cre_hw *cre_hw,
	void *cmd_args)
{
	return cam_cre_dev_process_release(cre_hw, cmd_args);
}

static int cam_cre_process_acquire(struct cam_cre_hw *cre_hw,
	void *cmd_args)
{
	return cam_cre_dev_process_acquire(cre_hw, cmd_args);
}

static int cam_cre_process_prepare(struct cam_cre_hw *cre_hw,
	void *cmd_args)
{
	return cam_cre_dev_process_prepare(cre_hw, cmd_args);
}

static int cam_cre_process_reg_set_update(struct cam_cre_hw *cre_hw,
	void *cmd_args)
{
	return cam_cre_dev_process_reg_set_update(cre_hw, cmd_args);
}

int cam_cre_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_hw_info *cre_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_cre_device_core_info *core_info = NULL;
	struct cam_cre_hw *cre_hw;
	unsigned long flags;

	if (!device_priv) {
		CAM_ERR(CAM_CRE, "Invalid args %x for cmd %u",
			device_priv, cmd_type);
		return -EINVAL;
	}

	soc_info = &cre_dev->soc_info;
	core_info = (struct cam_cre_device_core_info *)cre_dev->core_info;
	if ((!soc_info) || (!core_info)) {
		CAM_ERR(CAM_CRE, "soc_info = %x core_info = %x",
			soc_info, core_info);
		return -EINVAL;
	}

	cre_hw = core_info->cre_hw_info->cre_hw;
	if (!cre_hw) {
		CAM_ERR(CAM_CRE, "Invalid cre hw info");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CRE_HW_PROBE:
		rc = cam_cre_process_probe(cre_hw, cmd_args);
		break;
	case CRE_HW_ACQUIRE:
		rc = cam_cre_process_acquire(cre_hw, cmd_args);
		break;
	case CRE_HW_RELEASE:
		rc = cam_cre_process_release(cre_hw, cmd_args);
		break;
	case CRE_HW_PREPARE:
		rc = cam_cre_process_prepare(cre_hw, cmd_args);
		break;
	case CRE_HW_START:
		break;
	case CRE_HW_STOP:
		break;
	case CRE_HW_FLUSH:
		break;
	case CRE_HW_RESET:
		rc = cam_cre_process_reset(cre_hw, cmd_args);
		break;
	case CRE_HW_CLK_UPDATE: {
		struct cam_cre_dev_clk_update *clk_upd_cmd =
			(struct cam_cre_dev_clk_update *)cmd_args;

		if (!core_info->clk_enable) {
			rc = cam_soc_util_clk_enable_default(soc_info,
				CAM_SVS_VOTE);
			if (rc) {
				CAM_ERR(CAM_CRE, "Clock enable is failed");
				return rc;
			}
			core_info->clk_enable = true;
		}

		rc = cam_cre_update_clk_rate(soc_info, clk_upd_cmd->clk_rate);
		if (rc)
			CAM_ERR(CAM_CRE, "Failed to update clk: %d", rc);
		}
		break;
	case CRE_HW_CLK_DISABLE: {
		if (core_info->clk_enable)
			cam_soc_util_clk_disable_default(soc_info);

		core_info->clk_enable = false;
		}
		break;
	case CRE_HW_BW_UPDATE: {
		struct cam_cre_dev_bw_update *cpas_vote = cmd_args;

		if (!cmd_args)
			return -EINVAL;

		rc = cam_cre_caps_vote(core_info, cpas_vote);
		if (rc)
			CAM_ERR(CAM_CRE, "failed to update bw: %d", rc);
		}
		break;
	case CRE_HW_SET_IRQ_CB: {
		struct cam_cre_set_irq_cb *irq_cb = cmd_args;

		if (!cmd_args) {
			CAM_ERR(CAM_CRE, "cmd args NULL");
			return -EINVAL;
		}

		spin_lock_irqsave(&cre_dev->hw_lock, flags);
		core_info->irq_cb.cre_hw_mgr_cb = irq_cb->cre_hw_mgr_cb;
		core_info->irq_cb.data = irq_cb->data;
		spin_unlock_irqrestore(&cre_dev->hw_lock, flags);
		}
		break;
	case CRE_HW_REG_SET_UPDATE:
		rc = cam_cre_process_reg_set_update(cre_hw, cmd_args);
		break;
	case CRE_HW_DUMP_DEBUG:
		rc = cam_cre_process_dump_debug_reg(cre_hw);
		break;
	default:
		break;
	}

	return rc;
}

irqreturn_t cam_cre_irq(int irq_num, void *data)
{
	struct cam_hw_info *cre_dev = data;
	struct cam_cre_device_core_info *core_info = NULL;
	struct cam_cre_hw *cre_hw;

	if (!data) {
		CAM_ERR(CAM_CRE, "Invalid cam_dev_info or query_cap args");
		return IRQ_HANDLED;
	}

	core_info = (struct cam_cre_device_core_info *)cre_dev->core_info;
	cre_hw = core_info->cre_hw_info->cre_hw;

	irq_data.error = 0;
	irq_data.wr_buf_done = 0;

	cam_cre_top_process(cre_hw, 0, CRE_HW_ISR, &irq_data);

	if (irq_data.top_irq_status & CAM_CRE_WE_IRQ)
		cam_cre_bus_wr_process(cre_hw, 0, CRE_HW_ISR, &irq_data);
	if (irq_data.top_irq_status & CAM_CRE_FE_IRQ)
		cam_cre_bus_rd_process(cre_hw, 0, CRE_HW_ISR, &irq_data);

	spin_lock(&cre_dev->hw_lock);
	CAM_DBG(CAM_CRE, "core_info->irq_cb.cre_hw_mgr_cb %x core_info->irq_cb.data %x",
			core_info->irq_cb.cre_hw_mgr_cb, core_info->irq_cb.data);
	if (core_info->irq_cb.cre_hw_mgr_cb && core_info->irq_cb.data)
		if (irq_data.error || irq_data.wr_buf_done)
			core_info->irq_cb.cre_hw_mgr_cb(&irq_data,
				sizeof(struct cam_hw_info),
				core_info->irq_cb.data);
	spin_unlock(&cre_dev->hw_lock);

	return IRQ_HANDLED;
}
