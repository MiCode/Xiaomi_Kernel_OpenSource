// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/iopoll.h>
#include <linux/slab.h>
#include <media/cam_defs.h>

#include "cam_csid_ppi_core.h"
#include "cam_csid_ppi_dev.h"
#include "cam_soc_util.h"
#include "cam_debug_util.h"
#include "cam_io_util.h"
#include "cam_common_util.h"

static int cam_csid_ppi_reset(struct cam_csid_ppi_hw *ppi_hw)
{
	struct cam_hw_soc_info                *soc_info;
	const struct cam_csid_ppi_reg_offset  *ppi_reg;
	int rc = 0;
	uint32_t status;

	soc_info = &ppi_hw->hw_info->soc_info;
	ppi_reg  = ppi_hw->ppi_info->ppi_reg;

	CAM_DBG(CAM_ISP, "PPI:%d reset", ppi_hw->hw_intf->hw_idx);

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_mask_addr);
	cam_io_w_mb(PPI_RST_CONTROL, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_set_addr);
	cam_io_w_mb(PPI_RST_CONTROL, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_rst_cmd_addr);
	cam_io_w_mb(PPI_IRQ_CMD_SET, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_cmd_addr);

	rc = cam_common_read_poll_timeout(soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_status_addr,
		1000, 500000, 0x1, 0x1, &status);

	CAM_DBG(CAM_ISP, "PPI:%d reset status %d", ppi_hw->hw_intf->hw_idx,
		status);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "PPI:%d ppi_reset fail rc = %d status = %d",
			ppi_hw->hw_intf->hw_idx, rc, status);
		return rc;
	}
	cam_io_w_mb(PPI_RST_CONTROL, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_clear_addr);
	cam_io_w_mb(PPI_IRQ_CMD_CLEAR, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_cmd_addr);

	return 0;
}

static int cam_csid_ppi_enable_hw(struct cam_csid_ppi_hw  *ppi_hw)
{
	int rc = 0;
	int32_t i;
	uint64_t val;
	const struct cam_csid_ppi_reg_offset *ppi_reg;
	struct cam_hw_soc_info               *soc_info;
	uint32_t err_irq_mask;

	ppi_reg  = ppi_hw->ppi_info->ppi_reg;
	soc_info = &ppi_hw->hw_info->soc_info;

	CAM_DBG(CAM_ISP, "PPI:%d init PPI HW", ppi_hw->hw_intf->hw_idx);

	ppi_hw->hw_info->open_count++;
	if (ppi_hw->hw_info->open_count > 1) {
		CAM_DBG(CAM_ISP, "PPI:%d dual vfe already enabled",
			ppi_hw->hw_intf->hw_idx);
		return 0;
	}

	for (i = 0; i < soc_info->num_clk; i++) {
		rc = cam_soc_util_clk_enable(soc_info, false, i, -1, NULL);
		if (rc)
			goto clk_disable;
	}

	rc = cam_csid_ppi_reset(ppi_hw);
	if (rc)
		goto clk_disable;

	err_irq_mask = PPI_IRQ_FIFO0_OVERFLOW | PPI_IRQ_FIFO1_OVERFLOW |
		PPI_IRQ_FIFO2_OVERFLOW;
	cam_io_w_mb(err_irq_mask, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_mask_addr);
	rc  = cam_soc_util_irq_enable(soc_info);
	if (rc)
		goto clk_disable;

	cam_io_w_mb(PPI_RST_CONTROL, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_clear_addr);
	cam_io_w_mb(PPI_IRQ_CMD_CLEAR, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_cmd_addr);
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_hw_version_addr);
	CAM_DBG(CAM_ISP, "PPI:%d PPI HW version: 0x%x",
		ppi_hw->hw_intf->hw_idx, val);
	ppi_hw->device_enabled = 1;

	return 0;
clk_disable:
	for (--i; i >= 0; i--)
		cam_soc_util_clk_disable(soc_info, false, i);
	ppi_hw->hw_info->open_count--;
	return rc;
}

static int cam_csid_ppi_disable_hw(struct cam_csid_ppi_hw *ppi_hw)
{
	int rc = 0;
	int i;
	struct cam_hw_soc_info               *soc_info;
	const struct cam_csid_ppi_reg_offset *ppi_reg;
	uint64_t ppi_cfg_val = 0;

	CAM_DBG(CAM_ISP, "PPI:%d De-init PPI HW",
		ppi_hw->hw_intf->hw_idx);

	if (!ppi_hw->hw_info->open_count) {
		CAM_WARN(CAM_ISP, "ppi[%d] unbalanced disable hw",
			ppi_hw->hw_intf->hw_idx);
		return -EINVAL;
	}
	ppi_hw->hw_info->open_count--;

	if (ppi_hw->hw_info->open_count)
		return rc;

	soc_info = &ppi_hw->hw_info->soc_info;
	ppi_reg = ppi_hw->ppi_info->ppi_reg;

	CAM_DBG(CAM_ISP, "Calling PPI Reset");
	cam_csid_ppi_reset(ppi_hw);
	CAM_DBG(CAM_ISP, "PPI Reset Done");

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_mask_addr);
	cam_soc_util_irq_disable(soc_info);

	for (i = 0; i < CAM_CSID_PPI_LANES_MAX; i++)
		ppi_cfg_val &= ~PPI_CFG_CPHY_DLX_EN(i);

	cam_io_w_mb(ppi_cfg_val, soc_info->reg_map[0].mem_base +
			ppi_reg->ppi_module_cfg_addr);

	ppi_hw->device_enabled = 0;

	for (i = 0; i < soc_info->num_clk; i++)
		cam_soc_util_clk_disable(soc_info, false, i);

	return rc;
}

static int cam_csid_ppi_init_hw(void *hw_priv, void *init_args,
		uint32_t arg_size)
{
	int i, rc = 0;
	uint32_t num_lanes;
	uint32_t cphy;
	uint32_t ppi_cfg_val = 0;
	struct cam_csid_ppi_hw                *ppi_hw;
	struct cam_hw_info                    *ppi_hw_info;
	const struct cam_csid_ppi_reg_offset  *ppi_reg;
	struct cam_hw_soc_info                *soc_info;
	struct cam_csid_ppi_cfg                ppi_cfg;

	if (!hw_priv || !init_args ||
		(arg_size != sizeof(struct cam_csid_ppi_cfg))) {
		CAM_ERR(CAM_ISP, "PPI: Invalid args");
		rc = -EINVAL;
		goto end;
	}

	ppi_hw_info = (struct cam_hw_info *)hw_priv;
	ppi_hw      = (struct cam_csid_ppi_hw *)ppi_hw_info->core_info;
	ppi_reg     = ppi_hw->ppi_info->ppi_reg;
	ppi_cfg     = *((struct cam_csid_ppi_cfg *)init_args);

	rc = cam_csid_ppi_enable_hw(ppi_hw);
	if (rc)
		goto end;

	num_lanes = ppi_cfg.lane_num;
	cphy = ppi_cfg.lane_type;
	CAM_DBG(CAM_ISP, "lane_cfg  0x%x | num_lanes  0x%x | lane_type 0x%x",
		ppi_cfg.lane_cfg, num_lanes, cphy);

	if (cphy) {
		ppi_cfg_val |= PPI_CFG_CPHY_DLX_SEL(0);
		ppi_cfg_val |= PPI_CFG_CPHY_DLX_SEL(1);
	} else {
		ppi_cfg_val = 0;
	}

	for (i = 0; i < CAM_CSID_PPI_LANES_MAX; i++)
		ppi_cfg_val |= PPI_CFG_CPHY_DLX_EN(i);

	CAM_DBG(CAM_ISP, "ppi_cfg_val 0x%x", ppi_cfg_val);
	soc_info = &ppi_hw->hw_info->soc_info;
	cam_io_w_mb(ppi_cfg_val, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_module_cfg_addr);

	CAM_DBG(CAM_ISP, "ppi cfg 0x%x",
		cam_io_r_mb(soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_module_cfg_addr));
end:
	return rc;
}

static int cam_csid_ppi_deinit_hw(void *hw_priv, void *deinit_args,
		uint32_t arg_size)
{
	int rc = 0;
	struct cam_csid_ppi_hw  *ppi_hw;
	struct cam_hw_info      *ppi_hw_info;

	CAM_DBG(CAM_ISP, "Enter");

	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "PPI:Invalid arguments");
		rc = -EINVAL;
		goto end;
	}

	ppi_hw_info = (struct cam_hw_info  *)hw_priv;
	ppi_hw      = (struct cam_csid_ppi_hw *)ppi_hw_info->core_info;

	CAM_DBG(CAM_ISP, "Disabling PPI Hw");
	rc = cam_csid_ppi_disable_hw(ppi_hw);
	if (rc < 0)
		CAM_DBG(CAM_ISP, "Exit with %d", rc);
end:
	return rc;
}

int cam_csid_ppi_hw_probe_init(struct cam_hw_intf  *ppi_hw_intf,
	uint32_t ppi_idx)
{
	int rc = -EINVAL;
	struct cam_hw_info        *ppi_hw_info;
	struct cam_csid_ppi_hw    *csid_ppi_hw = NULL;

	if (ppi_idx >= CAM_CSID_PPI_HW_MAX) {
		CAM_ERR(CAM_ISP, "Invalid ppi index:%d", ppi_idx);
		goto err;
	}

	ppi_hw_info = (struct cam_hw_info  *) ppi_hw_intf->hw_priv;
	csid_ppi_hw  = (struct cam_csid_ppi_hw  *) ppi_hw_info->core_info;

	csid_ppi_hw->hw_intf = ppi_hw_intf;
	csid_ppi_hw->hw_info = ppi_hw_info;

	CAM_DBG(CAM_ISP, "type %d index %d",
		csid_ppi_hw->hw_intf->hw_type, ppi_idx);

	rc = cam_csid_ppi_init_soc_resources(&csid_ppi_hw->hw_info->soc_info,
		cam_csid_ppi_irq, csid_ppi_hw);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "PPI:%d Failed to init_soc", ppi_idx);
		goto err;
	}

	csid_ppi_hw->hw_intf->hw_ops.init   = cam_csid_ppi_init_hw;
	csid_ppi_hw->hw_intf->hw_ops.deinit = cam_csid_ppi_deinit_hw;
	return 0;
err:
	return rc;
}

int cam_csid_ppi_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t ppi_irq_handler, void *irq_data)
{
	int rc = 0;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc) {
		CAM_ERR(CAM_ISP, "PPI: Failed to get dt properties");
		goto end;
	}

	rc = cam_soc_util_request_platform_resource(soc_info, ppi_irq_handler,
		irq_data);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"PPI: Error Request platform resources failed rc=%d",
			rc);
		goto err;
	}
end:
	return rc;
err:
	cam_soc_util_release_platform_resource(soc_info);
	return rc;
}

irqreturn_t cam_csid_ppi_irq(int irq_num, void *data)
{
	uint32_t      irq_status = 0;
	uint32_t      i, ppi_cfg_val = 0;
	bool          fatal_err_detected = false;

	struct cam_csid_ppi_hw                *ppi_hw;
	struct cam_hw_soc_info                *soc_info;
	const struct cam_csid_ppi_reg_offset  *ppi_reg;

	if (!data) {
		CAM_ERR(CAM_ISP, "PPI: Invalid arguments");
		return IRQ_HANDLED;
	}

	ppi_hw = (struct cam_csid_ppi_hw *)data;
	ppi_reg = ppi_hw->ppi_info->ppi_reg;
	soc_info = &ppi_hw->hw_info->soc_info;

	if (ppi_hw->device_enabled != 1)
		goto ret;

	irq_status = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_status_addr);

	cam_io_w_mb(irq_status, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_clear_addr);

	cam_io_w_mb(PPI_IRQ_CMD_CLEAR, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_cmd_addr);

	CAM_DBG(CAM_ISP, "PPI %d irq status 0x%x", ppi_hw->hw_intf->hw_idx,
			irq_status);

	if (irq_status & PPI_IRQ_RST_DONE) {
		CAM_DBG(CAM_ISP, "PPI Reset Done");
		goto ret;
	}
	if ((irq_status & PPI_IRQ_FIFO0_OVERFLOW) ||
		(irq_status & PPI_IRQ_FIFO1_OVERFLOW) ||
		(irq_status & PPI_IRQ_FIFO2_OVERFLOW)) {
		fatal_err_detected = true;
		goto handle_fatal_error;
	}

handle_fatal_error:
	if (fatal_err_detected) {
		CAM_ERR(CAM_ISP, "PPI: %d irq_status:0x%x",
			ppi_hw->hw_intf->hw_idx, irq_status);
		for (i = 0; i < CAM_CSID_PPI_LANES_MAX; i++)
			ppi_cfg_val &= ~PPI_CFG_CPHY_DLX_EN(i);

		cam_io_w_mb(ppi_cfg_val, soc_info->reg_map[0].mem_base +
			ppi_reg->ppi_module_cfg_addr);
	}
ret:
	CAM_DBG(CAM_ISP, "IRQ Handling exit");
	return IRQ_HANDLED;
}

int cam_csid_ppi_hw_deinit(struct cam_csid_ppi_hw *csid_ppi_hw)
{
	if (!csid_ppi_hw) {
		CAM_ERR(CAM_ISP, "Invalid param");
		return -EINVAL;
	}
	return cam_soc_util_release_platform_resource(
		&csid_ppi_hw->hw_info->soc_info);
}

