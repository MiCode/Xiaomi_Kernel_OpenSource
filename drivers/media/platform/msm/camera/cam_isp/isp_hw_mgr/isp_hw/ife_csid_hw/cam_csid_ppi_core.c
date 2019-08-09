/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#include <linux/iopoll.h>
#include <linux/slab.h>
#include <uapi/media/cam_defs.h>

#include "cam_csid_ppi_core.h"
#include "cam_csid_ppi_dev.h"
#include "cam_soc_util.h"
#include "cam_debug_util.h"
#include "cam_io_util.h"

static int cam_csid_ppi_reset(struct cam_csid_ppi_hw *ppi_hw)
{
	struct cam_hw_soc_info                *soc_info;
	const struct cam_csid_ppi_reg_offset  *ppi_reg;
	int rc = 0;
	uint32_t val = 0;
	uint32_t clear_mask;
	uint32_t status;

	soc_info = &ppi_hw->hw_info->soc_info;
	ppi_reg  = ppi_hw->ppi_info->ppi_reg;

	CAM_DBG(CAM_ISP, "PPI:%d reset", ppi_hw->hw_intf->hw_idx);

	/* Mask all interrupts */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_mask_addr);

	/* clear all interrupts */
	clear_mask = PPI_IRQ_FIFO0_OVERFLOW | PPI_IRQ_FIFO1_OVERFLOW |
		PPI_IRQ_FIFO2_OVERFLOW;
	cam_io_w_mb(clear_mask, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_clear_addr);
	cam_io_w_mb(PPI_IRQ_CMD_CLEAR, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_cmd_addr);

	/* perform the top PPI HW registers reset */
	cam_io_w_mb(PPI_RST_CONTROL, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_rst_cmd_addr);

	rc = readl_poll_timeout(soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_status_addr, status,
		(status & 0x1) == 0x1, 1000, 100000);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "PPI:%d ppi_reset fail rc = %d",
			  ppi_hw->hw_intf->hw_idx, rc);
		rc = -ETIMEDOUT;
	}
	CAM_DBG(CAM_ISP, "PPI: reset status %d", status);

	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_mask_addr);
	if (val != 0)
		CAM_ERR(CAM_ISP, "PPI:%d IRQ value after reset rc = %d",
			ppi_hw->hw_intf->hw_idx, val);

	return rc;
}

static int cam_csid_ppi_enable_hw(struct cam_csid_ppi_hw  *ppi_hw)
{
	int rc = 0;
	uint32_t i;
	uint64_t val;
	const struct cam_csid_ppi_reg_offset *ppi_reg;
	struct cam_hw_soc_info               *soc_info;

	ppi_reg  = ppi_hw->ppi_info->ppi_reg;
	soc_info = &ppi_hw->hw_info->soc_info;

	CAM_DBG(CAM_ISP, "PPI:%d init PPI HW", ppi_hw->hw_intf->hw_idx);

	for (i = 0; i < soc_info->num_clk; i++) {
	/* Passing zero in clk_rate results in setting no clk_rate */
		rc = cam_soc_util_clk_enable(soc_info->clk[i],
			soc_info->clk_name[i], 0);
		if (rc)
			goto clk_disable;
	}

	rc  = cam_soc_util_irq_enable(soc_info);
	if (rc)
		goto clk_disable;

	/* Reset PPI */
	rc = cam_csid_ppi_reset(ppi_hw);
	if (rc)
		goto irq_disable;

	/* Clear the RST done IRQ */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_clear_addr);
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_cmd_addr);

	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			ppi_reg->ppi_hw_version_addr);
	CAM_DBG(CAM_ISP, "PPI:%d PPI HW version: 0x%x",
		ppi_hw->hw_intf->hw_idx, val);

	ppi_hw->device_enabled = 1;
	return 0;
irq_disable:
	cam_soc_util_irq_disable(soc_info);
clk_disable:
	for (i--; i >= 0; i--) {
		cam_soc_util_clk_disable(soc_info->clk[i],
			soc_info->clk_name[i]);
	}
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

	soc_info = &ppi_hw->hw_info->soc_info;
	ppi_reg = ppi_hw->ppi_info->ppi_reg;

	CAM_DBG(CAM_ISP, "%s:Calling PPI Reset\n", __func__);
	cam_csid_ppi_reset(ppi_hw);
	CAM_DBG(CAM_ISP, "%s:PPI Reset Done\n", __func__);

	/* disable the clocks */
	for (i = 0; i < soc_info->num_clk; i++)
		cam_soc_util_clk_disable(soc_info->clk[i],
			soc_info->clk_name[i]);

	/* disable the interrupt */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_mask_addr);
	cam_soc_util_irq_disable(soc_info);

	/* disable lanes */
	for (i = 0; i < CAM_CSID_PPI_LANES_MAX; i++)
		ppi_cfg_val &= ~PPI_CFG_CPHY_DLX_EN(i);

	cam_io_w_mb(ppi_cfg_val, soc_info->reg_map[0].mem_base +
			ppi_reg->ppi_module_cfg_addr);

	ppi_hw->device_enabled = 0;

	return rc;
}

static int cam_csid_ppi_init_hw(void *hw_priv, void *init_args,
		uint32_t arg_size)
{
	int i, rc = 0;
	uint32_t num_lanes;
	uint32_t lanes[CAM_CSID_PPI_HW_MAX] = {0, 0, 0, 0};
	uint32_t cphy;
	bool dl0, dl1;
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

	dl0 = dl1 = false;
	ppi_hw_info = (struct cam_hw_info *)hw_priv;
	ppi_hw      = (struct cam_csid_ppi_hw *)ppi_hw_info->core_info;
	ppi_reg     = ppi_hw->ppi_info->ppi_reg;
	ppi_cfg     = *((struct cam_csid_ppi_cfg *)init_args);

	/* Initialize the ppi hardware */
	rc = cam_csid_ppi_enable_hw(ppi_hw);
	if (rc)
		goto end;

	/* Do lane configuration here*/
	num_lanes = ppi_cfg.lane_num;
	/* lane_type = 1 refers to cphy */
	cphy = ppi_cfg.lane_type;
	CAM_DBG(CAM_ISP, "lane_cfg  0x%x | num_lanes  0x%x | lane_type 0x%x",
		ppi_cfg.lane_cfg, num_lanes, cphy);

	for (i = 0; i < num_lanes; i++) {
		lanes[i] = ppi_cfg.lane_cfg & (0x3 << (4 * i));
		(lanes[i] < 2) ? (dl0 = true) : (dl1 = true);
		CAM_DBG(CAM_ISP, "lanes[%d] %d", i, lanes[i]);
	}

	if (num_lanes) {
		if (cphy) {
			for (i = 0; i < num_lanes; i++) {
				/* Select Cphy */
				ppi_cfg_val |= PPI_CFG_CPHY_DLX_SEL(lanes[i]);
				/* Enable lane Cphy */
				ppi_cfg_val |= PPI_CFG_CPHY_DLX_EN(lanes[i]);
			}
		} else {
			if (dl0)
				/* Enable lane 0 */
				ppi_cfg_val |= PPI_CFG_CPHY_DLX_EN(0);
			if (dl1)
				/* Enable lane 1 */
				ppi_cfg_val |= PPI_CFG_CPHY_DLX_EN(1);
		}
	} else {
		CAM_ERR(CAM_ISP,
			"Number of lanes to enable is cannot be zero");
		rc = -1;
		goto end;
	}

	CAM_DBG(CAM_ISP, "ppi_cfg_val 0x%x", ppi_cfg_val);
	soc_info = &ppi_hw->hw_info->soc_info;
	cam_io_w_mb(ppi_cfg_val, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_module_cfg_addr);
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

	CAM_DBG(CAM_ISP, "Disabling PPI Hw\n");
	rc = cam_csid_ppi_disable_hw(ppi_hw);
	if (rc < 0)
		CAM_DBG(CAM_ISP, "%s: Exit with %d\n", __func__, rc);
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
	CAM_DBG(CAM_ISP, "PPI %d IRQ Handling", ppi_hw->hw_intf->hw_idx);
	ppi_reg = ppi_hw->ppi_info->ppi_reg;
	soc_info = &ppi_hw->hw_info->soc_info;

	/* read */
	irq_status = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_status_addr);

	/* clear */
	cam_io_w_mb(irq_status, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_clear_addr);

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		ppi_reg->ppi_irq_cmd_addr);

	CAM_DBG(CAM_ISP, "PPI %d irq status 0x%x", ppi_hw->hw_intf->hw_idx,
			irq_status);

	if (ppi_hw->device_enabled == 1) {
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
	}

handle_fatal_error:
	if (fatal_err_detected) {
		CAM_ERR(CAM_ISP, "PPI: %d irq_status:0x%x",
			ppi_hw->hw_intf->hw_idx, irq_status);
		/* disable lanes */
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
	/* release the privdate data memory from resources */
	return cam_soc_util_release_platform_resource(
		&csid_ppi_hw->hw_info->soc_info);
}

