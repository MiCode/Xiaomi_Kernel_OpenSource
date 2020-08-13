/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
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
#include <uapi/media/cam_isp.h>
#include <uapi/media/cam_defs.h>

#include "cam_req_mgr_workq.h"
#include "ais_ife_csid_core.h"
#include "ais_isp_hw.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"

/* Timeout value in msec */
#define IFE_CSID_TIMEOUT                               1000

/* TPG VC/DT values */
#define AIS_IFE_CSID_TPG_VC_VAL                        0xA
#define AIS_IFE_CSID_TPG_DT_VAL                        0x2B

/* Timeout values in usec */
#define AIS_IFE_CSID_TIMEOUT_SLEEP_US                  1000
#define AIS_IFE_CSID_TIMEOUT_ALL_US                    100000

/*
 * Constant Factors needed to change QTimer ticks to nanoseconds
 * QTimer Freq = 19.2 MHz
 * Time(us) = ticks/19.2
 * Time(ns) = ticks/19.2 * 1000
 */
#define AIS_IFE_CSID_QTIMER_MUL_FACTOR                 10000
#define AIS_IFE_CSID_QTIMER_DIV_FACTOR                 192

/* Max number of sof irq's triggered in case of SOF freeze */
#define AIS_CSID_IRQ_SOF_DEBUG_CNT_MAX 12

/* Max CSI Rx irq error count threshold value */
#define AIS_IFE_CSID_MAX_IRQ_ERROR_COUNT               5

static int ais_ife_csid_global_reset(struct ais_ife_csid_hw *csid_hw)
{
	struct cam_hw_soc_info                *soc_info;
	const struct ais_ife_csid_reg_offset  *csid_reg;
	int rc = 0;
	uint32_t val = 0, i;
	uint32_t status;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = csid_hw->csid_info->csid_reg;

	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid HW State:%d",
			csid_hw->hw_intf->hw_idx,
			csid_hw->hw_info->hw_state);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "CSID:%d Csid reset",
		csid_hw->hw_intf->hw_idx);

	/* Mask all interrupts */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_mask_addr);

	if (csid_reg->cmn_reg->num_pix)
		cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_irq_mask_addr);

	if (csid_reg->cmn_reg->num_ppp)
		cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->ppp_reg->csid_pxl_irq_mask_addr);

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++)
		cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_irq_mask_addr);

	/* clear all interrupts */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_clear_addr);

	cam_io_w_mb(csid_reg->csi2_reg->csi2_irq_mask_all,
		soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_clear_addr);

	if (csid_reg->cmn_reg->num_pix)
		cam_io_w_mb(csid_reg->cmn_reg->ipp_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_irq_clear_addr);

	if (csid_reg->cmn_reg->num_ppp)
		cam_io_w_mb(csid_reg->cmn_reg->ppp_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->ppp_reg->csid_pxl_irq_clear_addr);

	for (i = 0 ; i < csid_reg->cmn_reg->num_rdis; i++)
		cam_io_w_mb(csid_reg->cmn_reg->rdi_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_irq_clear_addr);

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_irq_cmd_addr);

	cam_io_w_mb(0x80, soc_info->reg_map[0].mem_base +
		csid_hw->csid_info->csid_reg->csi2_reg->csid_csi2_rx_cfg1_addr);

	/* enable the IPP and RDI format measure */
	if (csid_reg->cmn_reg->num_pix)
		cam_io_w_mb(0x1, soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_cfg0_addr);

	if (csid_reg->cmn_reg->num_ppp)
		cam_io_w_mb(0x1, soc_info->reg_map[0].mem_base +
			csid_reg->ppp_reg->csid_pxl_cfg0_addr);

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++)
		cam_io_w_mb(0x2, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_cfg0_addr);

	/* perform the top CSID HW registers reset */
	cam_io_w_mb(csid_reg->cmn_reg->csid_rst_stb,
		soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_rst_strobes_addr);

	rc = readl_poll_timeout(soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_status_addr,
			status, (status & 0x1) == 0x1,
		AIS_IFE_CSID_TIMEOUT_SLEEP_US, AIS_IFE_CSID_TIMEOUT_ALL_US);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "CSID:%d csid_reset fail rc = %d",
			  csid_hw->hw_intf->hw_idx, rc);
		rc = -ETIMEDOUT;
	}

	/* perform the SW registers reset */
	cam_io_w_mb(csid_reg->cmn_reg->csid_reg_rst_stb,
		soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_rst_strobes_addr);

	rc = readl_poll_timeout(soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_status_addr,
			status, (status & 0x1) == 0x1,
		AIS_IFE_CSID_TIMEOUT_SLEEP_US, AIS_IFE_CSID_TIMEOUT_ALL_US);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "CSID:%d csid_reset fail rc = %d",
			  csid_hw->hw_intf->hw_idx, rc);
		rc = -ETIMEDOUT;
	}

	usleep_range(3000, 3010);
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_mask_addr);
	if (val != 0)
		CAM_ERR(CAM_ISP, "CSID:%d IRQ value after reset rc = %d",
			csid_hw->hw_intf->hw_idx, val);
	csid_hw->error_irq_count = 0;

	for (i = 0 ; i < AIS_IFE_CSID_RDI_MAX; i++) {
		csid_hw->rdi_cfg[i].state = AIS_ISP_RESOURCE_STATE_AVAILABLE;
		csid_hw->rdi_cfg[i].sof_cnt = 0;
		csid_hw->rdi_cfg[i].prev_sof_hw_ts = 0;
		csid_hw->rdi_cfg[i].prev_sof_boot_ts = 0;
		csid_hw->rdi_cfg[i].measure_cfg.measure_enabled = 0;
	}

	return rc;
}

static int ais_ife_csid_path_reset(struct ais_ife_csid_hw *csid_hw,
		enum ais_ife_output_path_id  reset_path)
{
	int rc = 0;
	struct cam_hw_soc_info                    *soc_info;
	const struct ais_ife_csid_reg_offset      *csid_reg;
	uint32_t  reset_strb_addr, reset_strb_val, val, id;
	struct completion  *complete;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid hw state :%d",
			csid_hw->hw_intf->hw_idx,
			csid_hw->hw_info->hw_state);
		return -EINVAL;
	}

	if (reset_path >= AIS_IFE_CSID_RDI_MAX) {
		CAM_DBG(CAM_ISP, "CSID:%d Invalid res id%d",
			csid_hw->hw_intf->hw_idx, reset_path);
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ISP, "CSID:%d resource:%d",
		csid_hw->hw_intf->hw_idx, reset_path);

	id = reset_path;
	if (!csid_reg->rdi_reg[id]) {
		CAM_ERR(CAM_ISP, "CSID:%d RDI res not supported :%d",
				csid_hw->hw_intf->hw_idx,
				reset_path);
		return -EINVAL;
	}

	reset_strb_addr = csid_reg->rdi_reg[id]->csid_rdi_rst_strobes_addr;
	complete = &csid_hw->csid_rdi_complete[id];

	/* Enable path reset done interrupt */
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_irq_mask_addr);
	val |= CSID_PATH_INFO_RST_DONE;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_irq_mask_addr);

	reinit_completion(complete);
	reset_strb_val = csid_reg->cmn_reg->path_rst_stb_all;

	/* Reset the corresponding ife csid path */
	cam_io_w_mb(reset_strb_val, soc_info->reg_map[0].mem_base +
				reset_strb_addr);

	rc = wait_for_completion_timeout(complete,
		msecs_to_jiffies(IFE_CSID_TIMEOUT));
	if (rc) {
		rc = 0;
	} else {
		CAM_ERR(CAM_ISP, "CSID:%d RDI%d reset fail",
			 csid_hw->hw_intf->hw_idx, reset_path, rc);
		rc = -ETIMEDOUT;
	}

end:
	return rc;

}


static int ais_ife_csid_enable_hw(struct ais_ife_csid_hw  *csid_hw)
{
	int rc = 0;
	const struct ais_ife_csid_reg_offset      *csid_reg;
	struct cam_hw_soc_info              *soc_info;
	uint32_t i, val, clk_lvl;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	/* overflow check before increment */
	if (csid_hw->hw_info->open_count == UINT_MAX) {
		CAM_ERR(CAM_ISP, "CSID:%d Open count reached max",
			csid_hw->hw_intf->hw_idx);
		return -EINVAL;
	}

	/* Increment ref Count */
	csid_hw->hw_info->open_count++;
	if (csid_hw->hw_info->open_count > 1) {
		CAM_DBG(CAM_ISP, "CSID hw has already been enabled");
		return rc;
	}

	CAM_DBG(CAM_ISP, "CSID:%d init CSID HW",
		csid_hw->hw_intf->hw_idx);

	clk_lvl = cam_soc_util_get_vote_level(soc_info, csid_hw->clk_rate);
	CAM_DBG(CAM_ISP, "CSID clock lvl %u", clk_lvl);

	rc = ais_ife_csid_enable_soc_resources(soc_info, CAM_TURBO_VOTE);
	if (rc) {
		CAM_ERR(CAM_ISP, "CSID:%d Enable SOC failed",
			csid_hw->hw_intf->hw_idx);
		goto err;
	}

	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_UP;

	/* Reset CSID top */
	rc = ais_ife_csid_global_reset(csid_hw);
	if (rc)
		goto disable_soc;

	/* clear all interrupts */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_clear_addr);

	cam_io_w_mb(csid_reg->csi2_reg->csi2_irq_mask_all,
		soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_clear_addr);

	if (csid_reg->cmn_reg->num_pix)
		cam_io_w_mb(csid_reg->cmn_reg->ipp_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_irq_clear_addr);

	if (csid_reg->cmn_reg->num_ppp)
		cam_io_w_mb(csid_reg->cmn_reg->ppp_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->ppp_reg->csid_pxl_irq_clear_addr);

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++)
		cam_io_w_mb(csid_reg->cmn_reg->rdi_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_irq_clear_addr);

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_irq_cmd_addr);

	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->cmn_reg->csid_hw_version_addr);
	CAM_DBG(CAM_ISP, "CSID:%d CSID HW version: 0x%x",
		csid_hw->hw_intf->hw_idx, val);

	return 0;

disable_soc:
	ais_ife_csid_disable_soc_resources(soc_info);
	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
err:
	csid_hw->hw_info->open_count--;
	return rc;
}

static int ais_ife_csid_disable_hw(struct ais_ife_csid_hw *csid_hw)
{
	int rc = -EINVAL;
	uint32_t i;
	struct cam_hw_soc_info                   *soc_info;
	const struct ais_ife_csid_reg_offset     *csid_reg;
	unsigned long                             flags;

	/* Check for refcount */
	if (!csid_hw->hw_info->open_count) {
		CAM_WARN(CAM_ISP, "Unbalanced disable_hw");
		return rc;
	}

	/*  Decrement ref Count */
	csid_hw->hw_info->open_count--;

	if (csid_hw->hw_info->open_count) {
		rc = 0;
		return rc;
	}

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = csid_hw->csid_info->csid_reg;

	CAM_DBG(CAM_ISP, "Calling Global Reset");
	ais_ife_csid_global_reset(csid_hw);
	CAM_DBG(CAM_ISP, "Global Reset Done");

	CAM_DBG(CAM_ISP, "CSID:%d De-init CSID HW",
		csid_hw->hw_intf->hw_idx);

	/*disable the top IRQ interrupt */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_mask_addr);

	rc = ais_ife_csid_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_ISP, "CSID:%d Disable CSID SOC failed",
			csid_hw->hw_intf->hw_idx);

	spin_lock_irqsave(&csid_hw->lock_state, flags);
	csid_hw->device_enabled = 0;
	spin_unlock_irqrestore(&csid_hw->lock_state, flags);
	for (i = 0; i < AIS_IFE_CSID_RDI_MAX; i++) {
		csid_hw->rdi_cfg[i].state = AIS_ISP_RESOURCE_STATE_AVAILABLE;
		csid_hw->rdi_cfg[i].sof_cnt = 0;
		csid_hw->rdi_cfg[i].prev_sof_boot_ts = 0;
		csid_hw->rdi_cfg[i].prev_sof_hw_ts = 0;
		csid_hw->rdi_cfg[i].measure_cfg.measure_enabled = 0;
	}

	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	csid_hw->error_irq_count = 0;
	csid_hw->fatal_err_detected = false;


	return rc;
}


static int ais_ife_csid_enable_csi2(
	struct ais_ife_csid_hw          *csid_hw,
	struct ais_ife_csid_csi_info    *csi_info)
{
	int rc = 0;
	const struct ais_ife_csid_reg_offset   *csid_reg;
	struct cam_hw_soc_info                 *soc_info;
	uint32_t val = 0;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	CAM_DBG(CAM_ISP, "CSID:%d count:%d config csi2 rx",
		csid_hw->hw_intf->hw_idx, csid_hw->csi2_cfg_cnt);

	/* overflow check before increment */
	if (csid_hw->csi2_cfg_cnt == UINT_MAX) {
		CAM_ERR(CAM_ISP, "CSID:%d Open count reached max",
			csid_hw->hw_intf->hw_idx);
		return -EINVAL;
	}

	csid_hw->csi2_cfg_cnt++;
	if (csid_hw->csi2_cfg_cnt > 1)
		return rc;

	csid_hw->csi2_rx_cfg.phy_sel = csi_info->csiphy_id;
	csid_hw->csi2_rx_cfg.lane_num = csi_info->num_lanes;
	csid_hw->csi2_rx_cfg.lane_cfg = csi_info->lane_assign;
	csid_hw->csi2_rx_cfg.lane_type = csi_info->is_3Phase;

	/* rx cfg0 */
	val = ((csid_hw->csi2_rx_cfg.lane_num - 1) & 0x3)  |
		((csid_hw->csi2_rx_cfg.lane_cfg & 0xFFFF) << 4) |
		((csid_hw->csi2_rx_cfg.lane_type & 0x1) << 24);
	val |= (csid_hw->csi2_rx_cfg.phy_sel &
		csid_reg->csi2_reg->csi2_rx_phy_num_mask) << 20;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_cfg0_addr);

	/* rx cfg1*/
	val = (1 << csid_reg->csi2_reg->csi2_misr_enable_shift_val);
	/* if VC value is more than 3 than set full width of VC */
	if (csi_info->vc > 3)
		val |= (1 << csid_reg->csi2_reg->csi2_vc_mode_shift_val);

	/* enable packet ecc correction */
	val |= 1;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_cfg1_addr);

	/*Enable the CSI2 rx inerrupts */
	val = CSID_CSI2_RX_INFO_RST_DONE |
		CSID_CSI2_RX_ERROR_TG_FIFO_OVERFLOW |
		CSID_CSI2_RX_ERROR_LANE0_FIFO_OVERFLOW |
		CSID_CSI2_RX_ERROR_LANE1_FIFO_OVERFLOW |
		CSID_CSI2_RX_ERROR_LANE2_FIFO_OVERFLOW |
		CSID_CSI2_RX_ERROR_LANE3_FIFO_OVERFLOW |
		CSID_CSI2_RX_ERROR_CPHY_EOT_RECEPTION |
		CSID_CSI2_RX_ERROR_CPHY_SOT_RECEPTION |
		CSID_CSI2_RX_ERROR_CRC |
		CSID_CSI2_RX_ERROR_ECC |
		CSID_CSI2_RX_ERROR_MMAPPED_VC_DT |
		CSID_CSI2_RX_ERROR_STREAM_UNDERFLOW |
		CSID_CSI2_RX_ERROR_UNBOUNDED_FRAME |
		CSID_CSI2_RX_ERROR_CPHY_PH_CRC;

	/* Enable the interrupt based on csid debug info set */
	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_SOT_IRQ)
		val |= CSID_CSI2_RX_INFO_PHY_DL0_SOT_CAPTURED |
			CSID_CSI2_RX_INFO_PHY_DL1_SOT_CAPTURED |
			CSID_CSI2_RX_INFO_PHY_DL2_SOT_CAPTURED |
			CSID_CSI2_RX_INFO_PHY_DL3_SOT_CAPTURED;

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_EOT_IRQ)
		val |= CSID_CSI2_RX_INFO_PHY_DL0_EOT_CAPTURED |
			CSID_CSI2_RX_INFO_PHY_DL1_EOT_CAPTURED |
			CSID_CSI2_RX_INFO_PHY_DL2_EOT_CAPTURED |
			CSID_CSI2_RX_INFO_PHY_DL3_EOT_CAPTURED;

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE)
		val |= CSID_CSI2_RX_INFO_SHORT_PKT_CAPTURED;

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE)
		val |= CSID_CSI2_RX_INFO_LONG_PKT_CAPTURED;
	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE)
		val |= CSID_CSI2_RX_INFO_CPHY_PKT_HDR_CAPTURED;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_mask_addr);

	return 0;
}

static int ais_ife_csid_disable_csi2(struct ais_ife_csid_hw *csid_hw)
{
	int rc = 0;
	const struct ais_ife_csid_reg_offset *csid_reg;
	struct cam_hw_soc_info               *soc_info;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	CAM_DBG(CAM_ISP, "CSID:%d cnt : %d Disable csi2 rx",
		csid_hw->hw_intf->hw_idx, csid_hw->csi2_cfg_cnt);

	if (csid_hw->csi2_cfg_cnt)
		csid_hw->csi2_cfg_cnt--;

	if (csid_hw->csi2_cfg_cnt)
		return 0;

	/* Disable the CSI2 rx inerrupts */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_mask_addr);

	/* Reset the Rx CFG registers */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_cfg0_addr);
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_cfg1_addr);

	return rc;
}

static void ais_ife_csid_halt_csi2(
	struct ais_ife_csid_hw          *csid_hw)
{
	const struct ais_ife_csid_reg_offset      *csid_reg;
	struct cam_hw_soc_info                    *soc_info;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	/* Disable the CSI2 rx inerrupts */
	cam_io_w(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_mask_addr);

	/* Reset the Rx CFG registers */
	cam_io_w(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_cfg0_addr);
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_cfg1_addr);
}

static int ais_ife_csid_config_rdi_path(
	struct ais_ife_csid_hw          *csid_hw,
	struct ais_ife_rdi_init_args         *res)
{
	int rc = 0;
	const struct ais_ife_csid_reg_offset   *csid_reg;
	struct cam_hw_soc_info                 *soc_info;
	struct ais_ife_csid_path_cfg           *path_cfg;
	uint32_t val = 0, cfg0 = 0, id = 0, i = 0;
	uint32_t format_measure_addr;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	id = res->path;
	if (id >= csid_reg->cmn_reg->num_rdis || !csid_reg->rdi_reg[id]) {
		CAM_ERR(CAM_ISP, "CSID:%d RDI:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, id);
		return -EINVAL;
	}

	path_cfg = &csid_hw->rdi_cfg[id];
	path_cfg->vc = res->csi_cfg.vc;
	path_cfg->dt = res->csi_cfg.dt;
	path_cfg->cid = id;
	path_cfg->in_format = res->in_cfg.format;
	path_cfg->out_format = res->out_cfg.format;
	path_cfg->crop_enable = res->in_cfg.crop_enable;
	path_cfg->start_pixel = res->in_cfg.crop_left;
	path_cfg->end_pixel = res->in_cfg.crop_right;
	path_cfg->start_line = res->in_cfg.crop_top;
	path_cfg->end_line = res->in_cfg.crop_bottom;
	path_cfg->decode_fmt = res->in_cfg.decode_format;
	path_cfg->plain_fmt = res->in_cfg.pack_type;
	path_cfg->init_frame_drop = res->in_cfg.init_frame_drop;

	if (path_cfg->decode_fmt == 0xF)
		path_cfg->pix_enable = false;
	else
		path_cfg->pix_enable = true;

	/* select the post irq sub sample strobe for time stamp capture */
	cam_io_w_mb(CSID_TIMESTAMP_STB_POST_IRQ, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_cfg1_addr);

	if (path_cfg->crop_enable) {
		val = (((path_cfg->end_pixel & 0xFFFF) <<
			csid_reg->cmn_reg->crop_shift) |
			(path_cfg->start_pixel & 0xFFFF));

		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_rpp_hcrop_addr);
		CAM_DBG(CAM_ISP, "CSID:%d Horizontal crop config val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);

		val = (((path_cfg->end_line & 0xFFFF) <<
			csid_reg->cmn_reg->crop_shift) |
			(path_cfg->start_line & 0xFFFF));

		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_rpp_vcrop_addr);
		CAM_DBG(CAM_ISP, "CSID:%d Vertical Crop config val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);
	}

	/* configure pixel format measure */
	if (csid_hw->rdi_cfg[id].measure_cfg.measure_enabled) {
		val = ((csid_hw->rdi_cfg[id].measure_cfg.height &
		csid_reg->cmn_reg->format_measure_height_mask_val) <<
		csid_reg->cmn_reg->format_measure_height_shift_val);

		if (path_cfg->decode_fmt == 0xF)
			val |= (((csid_hw->rdi_cfg[id].measure_cfg.width *
					path_cfg->in_bpp) / 8) &
			csid_reg->cmn_reg->format_measure_width_mask_val);
		else
			val |= (csid_hw->rdi_cfg[id].measure_cfg.width &
			csid_reg->cmn_reg->format_measure_width_mask_val);

		CAM_DBG(CAM_ISP, "CSID:%d format measure cfg1 value : 0x%x",
			csid_hw->hw_intf->hw_idx, val);

		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_format_measure_cfg1_addr);

		/* enable pixel and line counter */
		cam_io_w_mb(3, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_format_measure_cfg0_addr);
	}

	/* set frame drop pattern to 0 and period to 1 */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_frm_drop_period_addr);
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_frm_drop_pattern_addr);
	/* set IRQ sum sabmple */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_irq_subsample_period_addr);
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_irq_subsample_pattern_addr);

	/* set pixel drop pattern to 0 and period to 1 */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_rpp_pix_drop_pattern_addr);
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_rpp_pix_drop_period_addr);
	/* set line drop pattern to 0 and period to 1 */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_rpp_line_drop_pattern_addr);
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_rpp_line_drop_period_addr);

	/* Configure the halt mode */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_ctrl_addr);

	/* Program the IPP CFG0 for full IFE in case of RPP path enabled */
	if (!csid_hw->ipp_fix_cfg &&
		csid_reg->cmn_reg->num_rdis == 3 &&
		path_cfg->pix_enable) {
		for (i = 0; i < AIS_IFE_CSID_RDI_MAX; i++) {
			struct ais_ife_csid_path_cfg *tmp =
				&csid_hw->rdi_cfg[i];

			/*
			 * doesn't compare with itself and
			 * not INIT/STREAMING rdi
			 */
			if (id == i ||
				tmp->state < AIS_ISP_RESOURCE_STATE_INIT_HW)
				continue;

			/*checking for multiple streams of same VC*/
			if (tmp->vc == path_cfg->vc &&
				tmp->decode_fmt == path_cfg->decode_fmt) {
				val = path_cfg->decode_fmt <<
					csid_reg->cmn_reg->fmt_shift_val;

				cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
					csid_reg->ipp_reg->csid_pxl_cfg0_addr);

				csid_hw->ipp_fix_cfg = true;
				break;
			}
		}
	}

	/* RDI path config and enable*/
	cfg0 = (path_cfg->vc << csid_reg->cmn_reg->vc_shift_val) |
		(path_cfg->dt << csid_reg->cmn_reg->dt_shift_val) |
		(path_cfg->cid << csid_reg->cmn_reg->dt_id_shift_val) |
		(path_cfg->decode_fmt << csid_reg->cmn_reg->fmt_shift_val) |
		(path_cfg->plain_fmt << csid_reg->cmn_reg->plain_fmt_shit_val) |
		(path_cfg->crop_enable  <<
			csid_reg->cmn_reg->crop_h_en_shift_val) |
		(path_cfg->crop_enable  <<
		csid_reg->cmn_reg->crop_v_en_shift_val) |
		(1 << csid_reg->cmn_reg->path_en_shift_val) |
		(1 << 2) | 3;

	cam_io_w_mb(cfg0, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_cfg0_addr);

	format_measure_addr =
		csid_reg->rdi_reg[id]->csid_rdi_format_measure_cfg0_addr;

	/* Enable the HBI/VBI counter */
	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_HBI_VBI_INFO) {
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			format_measure_addr);
		val |= csid_reg->cmn_reg->measure_en_hbi_vbi_cnt_mask;
		cam_io_w_mb(val,
			soc_info->reg_map[0].mem_base + format_measure_addr);
	}

	/* configure the rx packet capture based on csid debug set */
	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE)
		val = ((1 <<
			csid_reg->csi2_reg->csi2_capture_short_pkt_en_shift) |
			(path_cfg->vc <<
			csid_reg->csi2_reg->csi2_capture_short_pkt_vc_shift));

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE)
		val |= ((1 <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_en_shift) |
			(path_cfg->dt <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_dt_shift) |
			(path_cfg->vc <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_vc_shift));

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE)
		val |= ((1 <<
			csid_reg->csi2_reg->csi2_capture_cphy_pkt_en_shift) |
			(path_cfg->dt <<
			csid_reg->csi2_reg->csi2_capture_cphy_pkt_dt_shift) |
			(path_cfg->vc <<
			csid_reg->csi2_reg->csi2_capture_cphy_pkt_vc_shift));
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_capture_ctrl_addr);

	path_cfg->state = AIS_ISP_RESOURCE_STATE_INIT_HW;

	CAM_DBG(CAM_ISP, "CSID:%d RDI%d configured vc:%d dt:%d", id,
		csid_hw->hw_intf->hw_idx, path_cfg->vc, path_cfg->dt);

	return rc;
}

static int ais_ife_csid_deinit_rdi_path(
	struct ais_ife_csid_hw          *csid_hw,
	enum ais_ife_output_path_id    id)
{
	int rc = 0, i = 0;
	uint32_t val, format_measure_addr;
	const struct ais_ife_csid_reg_offset      *csid_reg;
	struct cam_hw_soc_info                    *soc_info;
	struct ais_ife_csid_path_cfg              *path_cfg;

	path_cfg = &csid_hw->rdi_cfg[id];
	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	format_measure_addr =
		csid_reg->rdi_reg[id]->csid_rdi_format_measure_cfg0_addr;

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_HBI_VBI_INFO) {
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_cfg0_addr);
		val &= ~csid_reg->cmn_reg->format_measure_en_val;
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_cfg0_addr);

		/* Disable the HBI/VBI counter */
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			format_measure_addr);
		val &= ~csid_reg->cmn_reg->measure_en_hbi_vbi_cnt_mask;
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			format_measure_addr);
	}

	/* Reset ipp_fix_cfg if needed. Check for multiple streams of same DT*/
	if (csid_hw->ipp_fix_cfg == true) {
		int check_cnt =  0;

		for (i = 0; i < AIS_IFE_CSID_RDI_MAX; i++) {
			struct ais_ife_csid_path_cfg *tmp =
				&csid_hw->rdi_cfg[i];

			if (i == id ||
				tmp->state == AIS_ISP_RESOURCE_STATE_AVAILABLE)
				continue;

			if (tmp->vc == path_cfg->vc &&
				tmp->decode_fmt == path_cfg->decode_fmt)
				check_cnt++;
		}

		if (check_cnt <= 1)
			csid_hw->ipp_fix_cfg = false;
	}

	path_cfg->state = AIS_ISP_RESOURCE_STATE_AVAILABLE;

	return rc;
}

static int ais_ife_csid_enable_rdi_path(
	struct ais_ife_csid_hw          *csid_hw,
	struct ais_ife_rdi_start_args   *start_cmd)
{
	const struct ais_ife_csid_reg_offset      *csid_reg;
	struct cam_hw_soc_info                    *soc_info;
	struct ais_ife_csid_path_cfg              *path_data;
	uint32_t id, val;

	id = start_cmd->path;
	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	path_data = &csid_hw->rdi_cfg[id];
	path_data->sof_cnt = 0;

	/* Enable the required RDI interrupts */
	val = CSID_PATH_INFO_RST_DONE | CSID_PATH_ERROR_FIFO_OVERFLOW;

	if (csid_reg->rdi_reg[id]->ccif_violation_en)
		val |= CSID_PATH_ERROR_CCIF_VIOLATION;

	if ((csid_hw->csid_debug & CSID_DEBUG_ENABLE_SOF_IRQ) ||
		(path_data->init_frame_drop))
		val |= CSID_PATH_INFO_INPUT_SOF;

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_EOF_IRQ)
		val |= CSID_PATH_INFO_INPUT_EOF;

	if (csid_hw->rdi_cfg[id].measure_cfg.measure_enabled)
		val |= (CSID_PATH_ERROR_PIX_COUNT |
			CSID_PATH_ERROR_LINE_COUNT);

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_irq_mask_addr);

	/*resume at frame boundary */
	if (!path_data->init_frame_drop) {
		CAM_DBG(CAM_ISP, "Start RDI:%d path", id);
		cam_io_w_mb(AIS_CSID_RESUME_AT_FRAME_BOUNDARY,
				soc_info->reg_map[0].mem_base +
				csid_reg->rdi_reg[id]->csid_rdi_ctrl_addr);
	}

	path_data->state = AIS_ISP_RESOURCE_STATE_STREAMING;

	return 0;
}


static int ais_ife_csid_disable_rdi_path(
	struct ais_ife_csid_hw          *csid_hw,
	struct ais_ife_rdi_stop_args    *stop_args,
	enum ais_ife_csid_halt_cmd       stop_cmd)
{
	int rc = 0;
	uint32_t id, val = 0;
	const struct ais_ife_csid_reg_offset       *csid_reg;
	struct cam_hw_soc_info                     *soc_info;
	struct ais_ife_csid_path_cfg               *path_data;

	id = stop_args->path;
	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	path_data = &csid_hw->rdi_cfg[id];

	if (path_data->state != AIS_ISP_RESOURCE_STATE_STREAMING) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"CSID:%d RDI:%d already in stopped state:%d",
			csid_hw->hw_intf->hw_idx,
			id, path_data->state);
		return -EINVAL;
	}

	if (stop_cmd != AIS_CSID_HALT_AT_FRAME_BOUNDARY &&
		stop_cmd != AIS_CSID_HALT_IMMEDIATELY) {
		CAM_ERR(CAM_ISP, "CSID:%d un supported stop command:%d",
			csid_hw->hw_intf->hw_idx, stop_cmd);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "CSID:%d RDI:%d",
		csid_hw->hw_intf->hw_idx, id);

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_irq_mask_addr);

	/* Halt the RDI path */
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_ctrl_addr);
	val &= ~0x3;
	val |= stop_cmd;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_ctrl_addr);

	path_data->state = AIS_ISP_RESOURCE_STATE_INIT_HW;

	return rc;
}

static int ais_ife_csid_poll_stop_status(
	struct ais_ife_csid_hw          *csid_hw,
	uint32_t                         res_mask)
{
	int rc = 0;
	uint32_t csid_status_addr = 0, val = 0, res_id = 0;
	const struct ais_ife_csid_reg_offset       *csid_reg;
	struct cam_hw_soc_info                     *soc_info;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	for (; res_id < AIS_IFE_CSID_RDI_MAX; res_id++, res_mask >>= 1) {
		if ((res_mask & 0x1) == 0)
			continue;
		val = 0;

		csid_status_addr =
			csid_reg->rdi_reg[res_id]->csid_rdi_status_addr;

		CAM_DBG(CAM_ISP, "start polling CSID:%d res_id:%d",
			csid_hw->hw_intf->hw_idx, res_id);

		rc = readl_poll_timeout(soc_info->reg_map[0].mem_base +
			csid_status_addr, val, (val & 0x1) == 0x1,
			AIS_IFE_CSID_TIMEOUT_SLEEP_US,
			AIS_IFE_CSID_TIMEOUT_ALL_US);
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "CSID:%d res:%d halt failed rc %d",
				csid_hw->hw_intf->hw_idx, res_id, rc);
			rc = -ETIMEDOUT;
			break;
		}
		CAM_DBG(CAM_ISP, "End polling CSID:%d res_id:%d",
			csid_hw->hw_intf->hw_idx, res_id);
	}

	return rc;
}


static int ais_ife_csid_get_time_stamp(
	struct ais_ife_csid_hw   *csid_hw,
	void *cmd_args)
{
	struct ais_ife_rdi_get_timestamp_args      *p_timestamp;
	const struct ais_ife_csid_reg_offset       *csid_reg;
	struct cam_hw_soc_info                     *soc_info;
	const struct ais_ife_csid_rdi_reg_offset   *rdi_reg;
	uint32_t  time_32_lsb, time_32_msb, id;
	uint64_t  time_64;

	p_timestamp = (struct ais_ife_rdi_get_timestamp_args *)cmd_args;
	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	id = p_timestamp->path;

	if (id >= AIS_IFE_CSID_RDI_MAX || !csid_reg->rdi_reg[id]) {
		CAM_DBG(CAM_ISP, "CSID:%d Invalid RDI%d",
			csid_hw->hw_intf->hw_idx, id);
		return -EINVAL;
	}

	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid dev state :%d",
			csid_hw->hw_intf->hw_idx,
			csid_hw->hw_info->hw_state);
		return -EINVAL;
	}

	rdi_reg = csid_reg->rdi_reg[id];
	time_32_lsb = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			rdi_reg->csid_rdi_timestamp_curr0_sof_addr);
	time_32_msb = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			rdi_reg->csid_rdi_timestamp_curr1_sof_addr);
	time_64 = ((uint64_t)time_32_msb << 32) | (uint64_t)time_32_lsb;
	p_timestamp->ts->cur_sof_ts = mul_u64_u32_div(time_64,
		AIS_IFE_CSID_QTIMER_MUL_FACTOR,
		AIS_IFE_CSID_QTIMER_DIV_FACTOR);

	time_32_lsb = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			rdi_reg->csid_rdi_timestamp_prev0_sof_addr);
	time_32_msb = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			rdi_reg->csid_rdi_timestamp_prev1_sof_addr);
	time_64 = ((uint64_t)time_32_msb << 32) | (uint64_t)time_32_lsb;
	p_timestamp->ts->prev_sof_ts = mul_u64_u32_div(time_64,
		AIS_IFE_CSID_QTIMER_MUL_FACTOR,
		AIS_IFE_CSID_QTIMER_DIV_FACTOR);

	csid_hw->rdi_cfg[id].prev_sof_hw_ts = p_timestamp->ts->cur_sof_ts;

	return 0;
}

static int ais_ife_csid_set_csid_debug(struct ais_ife_csid_hw   *csid_hw,
	void *cmd_args)
{
	uint32_t  *csid_debug;

	csid_debug = (uint32_t  *) cmd_args;
	csid_hw->csid_debug = *csid_debug;
	CAM_DBG(CAM_ISP, "CSID:%d set csid debug value:%d",
		csid_hw->hw_intf->hw_idx, csid_hw->csid_debug);

	return 0;
}

static int ais_ife_csid_get_hw_caps(void *hw_priv,
	void *get_hw_cap_args, uint32_t arg_size)
{
	int rc = 0;
	struct ais_ife_csid_hw_caps           *hw_caps;
	struct ais_ife_csid_hw                *csid_hw;
	struct cam_hw_info                    *csid_hw_info;
	const struct ais_ife_csid_reg_offset  *csid_reg;

	if (!hw_priv || !get_hw_cap_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct ais_ife_csid_hw   *)csid_hw_info->core_info;
	csid_reg = csid_hw->csid_info->csid_reg;
	hw_caps = (struct ais_ife_csid_hw_caps *) get_hw_cap_args;

	hw_caps->num_rdis = csid_reg->cmn_reg->num_rdis;
	hw_caps->num_pix = csid_reg->cmn_reg->num_pix;
	hw_caps->num_ppp = csid_reg->cmn_reg->num_ppp;
	hw_caps->major_version = csid_reg->cmn_reg->major_version;
	hw_caps->minor_version = csid_reg->cmn_reg->minor_version;
	hw_caps->version_incr = csid_reg->cmn_reg->version_incr;

	CAM_DBG(CAM_ISP,
		"CSID:%d No rdis:%d, no pix:%d, major:%d minor:%d ver :%d",
		csid_hw->hw_intf->hw_idx, hw_caps->num_rdis,
		hw_caps->num_pix, hw_caps->major_version,
		hw_caps->minor_version, hw_caps->version_incr);

	return rc;
}

static int ais_ife_csid_force_reset(void *hw_priv,
	void *reset_args, uint32_t arg_size)
{
	struct ais_ife_csid_hw          *csid_hw;
	struct cam_hw_info              *csid_hw_info;
	int rc = 0;

	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "CSID:Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct ais_ife_csid_hw   *)csid_hw_info->core_info;

	mutex_lock(&csid_hw->hw_info->hw_mutex);

	/* Disable CSID HW if necessary */
	if (csid_hw_info->open_count) {
		csid_hw_info->open_count = 1;

		CAM_DBG(CAM_ISP, "Disabling CSID Hw");
		rc = ais_ife_csid_disable_hw(csid_hw);
	}

	mutex_unlock(&csid_hw->hw_info->hw_mutex);

	CAM_INFO(CAM_ISP, "Exit (%d)", rc);

	return rc;
}

static int ais_ife_csid_reset_retain_sw_reg(
	struct ais_ife_csid_hw *csid_hw)
{
	int rc = 0;
	uint32_t status;
	const struct ais_ife_csid_reg_offset *csid_reg =
		csid_hw->csid_info->csid_reg;
	struct cam_hw_soc_info          *soc_info;

	soc_info = &csid_hw->hw_info->soc_info;
	/* clear the top interrupt first */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_clear_addr);
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_irq_cmd_addr);

	usleep_range(3000, 3010);

	cam_io_w_mb(csid_reg->cmn_reg->csid_rst_stb,
		soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_rst_strobes_addr);
	rc = readl_poll_timeout(soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_status_addr,
			status, (status & 0x1) == 0x1,
		AIS_IFE_CSID_TIMEOUT_SLEEP_US, AIS_IFE_CSID_TIMEOUT_ALL_US);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "CSID:%d csid_reset fail rc = %d",
			  csid_hw->hw_intf->hw_idx, rc);
		rc = -ETIMEDOUT;
	} else {
		CAM_DBG(CAM_ISP, "CSID:%d hw reset completed %d",
			csid_hw->hw_intf->hw_idx, rc);
		rc = 0;
	}
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_clear_addr);
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_irq_cmd_addr);

	return rc;
}


static int ais_ife_csid_reserve(void *hw_priv,
	void *reserve_args, uint32_t arg_size)
{
	int rc = 0;
	struct ais_ife_csid_hw                 *csid_hw;
	struct cam_hw_info                     *csid_hw_info;
	struct ais_ife_rdi_init_args            *rdi_cfg;
	const struct ais_ife_csid_reg_offset   *csid_reg;
	unsigned long                           flags;

	if (!hw_priv || !reserve_args ||
		(arg_size != sizeof(struct ais_ife_rdi_init_args))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct ais_ife_csid_hw   *)csid_hw_info->core_info;
	rdi_cfg = (struct ais_ife_rdi_init_args *)reserve_args;
	csid_reg = csid_hw->csid_info->csid_reg;

	mutex_lock(&csid_hw->hw_info->hw_mutex);
	if (rdi_cfg->path >= AIS_IFE_CSID_RDI_MAX) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid RDI%d",
			csid_hw->hw_intf->hw_idx, rdi_cfg->path);
		rc = -EINVAL;
		goto end;
	}

	if (csid_hw->rdi_cfg[rdi_cfg->path].state !=
		AIS_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP,
			"CSID:%d RDI%d Invalid state %d",
			csid_hw->hw_intf->hw_idx,
			rdi_cfg->path, csid_hw->rdi_cfg[rdi_cfg->path].state);
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ISP, "CSID:%d res path :%d",
		csid_hw->hw_intf->hw_idx, rdi_cfg->path);

	rc = ais_ife_csid_enable_csi2(csid_hw, &rdi_cfg->csi_cfg);
	if (rc)
		goto end;

	if (csid_hw->device_enabled == 0) {
		rc = ais_ife_csid_reset_retain_sw_reg(csid_hw);
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "CSID: Failed in SW reset");
			goto disable_csi2;
		} else {
			CAM_DBG(CAM_ISP, "CSID: SW reset Successful");
			spin_lock_irqsave(&csid_hw->lock_state, flags);
			csid_hw->device_enabled = 1;
			spin_unlock_irqrestore(&csid_hw->lock_state, flags);
		}
	}

	rc = ais_ife_csid_config_rdi_path(csid_hw, rdi_cfg);
	if (rc)
		goto disable_csi2;

disable_csi2:
	if (rc)
		ais_ife_csid_disable_csi2(csid_hw);
end:
	mutex_unlock(&csid_hw->hw_info->hw_mutex);

	return rc;
}

static int ais_ife_csid_release(void *hw_priv,
	void *release_args, uint32_t arg_size)
{
	int rc = 0;
	struct ais_ife_csid_hw                 *csid_hw;
	struct cam_hw_info                     *csid_hw_info;
	struct ais_ife_rdi_deinit_args         *rdi_deinit;

	if (!hw_priv || !release_args ||
		(arg_size != sizeof(struct ais_ife_rdi_deinit_args))) {
		CAM_ERR(CAM_ISP, "CSID:Invalid arguments");
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "Enter");
	rdi_deinit = (struct ais_ife_rdi_deinit_args *)release_args;
	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct ais_ife_csid_hw   *)csid_hw_info->core_info;

	mutex_lock(&csid_hw->hw_info->hw_mutex);
	if (rdi_deinit->path >= AIS_IFE_CSID_RDI_MAX) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid path:%d",
			csid_hw->hw_intf->hw_idx, rdi_deinit->path);
		rc = -EINVAL;
		goto end;
	}

	if (csid_hw->rdi_cfg[rdi_deinit->path].state <
		AIS_ISP_RESOURCE_STATE_INIT_HW) {
		CAM_ERR(CAM_ISP,
			"CSID:%d path:%d Invalid state %d",
			csid_hw->hw_intf->hw_idx,
			rdi_deinit->path,
			csid_hw->rdi_cfg[rdi_deinit->path].state);
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ISP, "De-Init RDI Path: %d", rdi_deinit->path);
	rc = ais_ife_csid_deinit_rdi_path(csid_hw, rdi_deinit->path);

	CAM_DBG(CAM_ISP, "De-Init ife_csid");
	rc |= ais_ife_csid_disable_csi2(csid_hw);

	CAM_DBG(CAM_ISP, "Exit");

end:
	mutex_unlock(&csid_hw->hw_info->hw_mutex);

	return rc;
}

static int ais_ife_csid_init_hw(void *hw_priv,
	void *init_args, uint32_t arg_size)
{
	int rc = 0;
	struct ais_ife_csid_hw                 *csid_hw;
	struct cam_hw_info                     *csid_hw_info;

	if (!hw_priv || !init_args ||
		(arg_size != sizeof(struct ais_ife_rdi_init_args))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct ais_ife_csid_hw   *)csid_hw_info->core_info;

	mutex_lock(&csid_hw->hw_info->hw_mutex);

	/* Initialize the csid hardware */
	rc = ais_ife_csid_enable_hw(csid_hw);

	mutex_unlock(&csid_hw->hw_info->hw_mutex);

	CAM_DBG(CAM_ISP, "Exit (%d)", rc);

	return rc;
}

static int ais_ife_csid_deinit_hw(void *hw_priv,
	void *deinit_args, uint32_t arg_size)
{
	int rc = 0;
	struct ais_ife_csid_hw                 *csid_hw;
	struct cam_hw_info                     *csid_hw_info;
	struct ais_ife_rdi_deinit_args         *deinit;

	if (!hw_priv || !deinit_args ||
		(arg_size != sizeof(struct ais_ife_rdi_deinit_args))) {
		CAM_ERR(CAM_ISP, "CSID:Invalid arguments");
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "Enter");

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct ais_ife_csid_hw   *)csid_hw_info->core_info;
	deinit = (struct ais_ife_rdi_deinit_args *)deinit_args;

	mutex_lock(&csid_hw->hw_info->hw_mutex);

	ais_ife_csid_path_reset(csid_hw, deinit->path);

	/* Disable CSID HW */
	CAM_DBG(CAM_ISP, "Disabling CSID Hw");
	rc = ais_ife_csid_disable_hw(csid_hw);

	mutex_unlock(&csid_hw->hw_info->hw_mutex);

	CAM_DBG(CAM_ISP, "Exit");

	return rc;
}

static int ais_ife_csid_start(void *hw_priv, void *start_args,
			uint32_t arg_size)
{
	int rc = 0;
	struct ais_ife_csid_hw                 *csid_hw;
	struct cam_hw_info                     *csid_hw_info;
	const struct ais_ife_csid_reg_offset   *csid_reg;
	struct ais_ife_rdi_start_args          *start_cmd;

	if (!hw_priv || !start_args ||
		(arg_size != sizeof(struct ais_ife_rdi_start_args))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct ais_ife_csid_hw   *)csid_hw_info->core_info;
	csid_reg = csid_hw->csid_info->csid_reg;
	start_cmd = (struct ais_ife_rdi_start_args *)start_args;

	if (start_cmd->path >= csid_reg->cmn_reg->num_rdis ||
		!csid_reg->rdi_reg[start_cmd->path]) {
		CAM_ERR(CAM_ISP, "CSID:%d RDI:%d is not supported on HW",
			csid_hw->hw_intf->hw_idx, start_cmd->path);
		rc = -EINVAL;
		goto end;
	}

	/* Reset sof irq debug fields */
	csid_hw->sof_irq_triggered = false;
	csid_hw->irq_debug_cnt = 0;

	CAM_DBG(CAM_ISP, "CSID:%d res_id:%d",
		csid_hw->hw_intf->hw_idx, start_cmd->path);

	rc = ais_ife_csid_enable_rdi_path(csid_hw, start_cmd);
end:
	return rc;
}

static int ais_ife_csid_stop(void *hw_priv,
	void *stop_args, uint32_t arg_size)
{
	int rc = 0;
	struct ais_ife_csid_hw               *csid_hw;
	struct cam_hw_info                   *csid_hw_info;
	const struct ais_ife_csid_reg_offset *csid_reg;
	struct ais_ife_rdi_stop_args         *stop_cmd;
	uint32_t  res_mask = 0;

	if (!hw_priv || !stop_args ||
		(arg_size != sizeof(struct ais_ife_rdi_stop_args))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct ais_ife_csid_hw   *)csid_hw_info->core_info;
	csid_reg = csid_hw->csid_info->csid_reg;
	stop_cmd = (struct ais_ife_rdi_stop_args  *) stop_args;

	if (stop_cmd->path >= csid_reg->cmn_reg->num_rdis ||
		!csid_reg->rdi_reg[stop_cmd->path]) {
		CAM_ERR(CAM_ISP, "CSID:%d RDI:%d is not supported on HW",
			csid_hw->hw_intf->hw_idx, stop_cmd->path);
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ISP, "CSID:%d RDI %d",
		csid_hw->hw_intf->hw_idx,
		stop_cmd->path);

	/* Stop the resource first */
	rc = ais_ife_csid_disable_rdi_path(csid_hw, stop_cmd,
			AIS_CSID_HALT_IMMEDIATELY);

	if (res_mask)
		rc = ais_ife_csid_poll_stop_status(csid_hw, res_mask);

end:
	CAM_DBG(CAM_ISP,  "Exit (%d)", rc);

	return rc;

}

static int ais_ife_csid_read(void *hw_priv,
	void *read_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "CSID: un supported");

	return -EINVAL;
}

static int ais_ife_csid_write(void *hw_priv,
	void *write_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "CSID: un supported");
	return -EINVAL;
}

static int ais_ife_csid_sof_irq_debug(
	struct ais_ife_csid_hw *csid_hw, void *cmd_args)
{
	int i = 0;
	uint32_t val = 0;
	bool sof_irq_enable = false;
	const struct ais_ife_csid_reg_offset    *csid_reg;
	struct cam_hw_soc_info                  *soc_info;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (*((uint32_t *)cmd_args) == 1)
		sof_irq_enable = true;

	if (csid_hw->hw_info->hw_state ==
		CAM_HW_STATE_POWER_DOWN) {
		CAM_WARN(CAM_ISP,
			"CSID powered down unable to %s sof irq",
			(sof_irq_enable == true) ? "enable" : "disable");
		return 0;
	}

	if (csid_reg->ipp_reg) {
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_irq_mask_addr);

		if (val) {
			if (sof_irq_enable)
				val |= CSID_PATH_INFO_INPUT_SOF;
			else
				val &= ~CSID_PATH_INFO_INPUT_SOF;

			cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
				csid_reg->ipp_reg->csid_pxl_irq_mask_addr);
			val = 0;
		}
	}

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++) {
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_irq_mask_addr);
		if (val) {
			if (sof_irq_enable)
				val |= CSID_PATH_INFO_INPUT_SOF;
			else
				val &= ~CSID_PATH_INFO_INPUT_SOF;

			cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
				csid_reg->rdi_reg[i]->csid_rdi_irq_mask_addr);
			val = 0;
		}
	}

	if (sof_irq_enable) {
		csid_hw->csid_debug |= CSID_DEBUG_ENABLE_SOF_IRQ;
		csid_hw->sof_irq_triggered = true;
	} else {
		csid_hw->csid_debug &= ~CSID_DEBUG_ENABLE_SOF_IRQ;
		csid_hw->sof_irq_triggered = false;
	}

	CAM_INFO(CAM_ISP, "SOF freeze: CSID SOF irq %s",
		(sof_irq_enable == true) ? "enabled" : "disabled");

	return 0;
}

static int ais_ife_csid_set_csid_clock(
	struct ais_ife_csid_hw *csid_hw, void *cmd_args)
{
	struct ais_ife_csid_clock_update_args *clk_update = NULL;

	if (!csid_hw)
		return -EINVAL;

	clk_update =
		(struct ais_ife_csid_clock_update_args *)cmd_args;

	csid_hw->clk_rate = clk_update->clk_rate;
	CAM_INFO(CAM_ISP, "CSID clock rate %llu", csid_hw->clk_rate);

	return 0;
}

static int ais_ife_csid_dump_hw(
	struct ais_ife_csid_hw *csid_hw, void *cmd_args)
{
	struct cam_hw_soc_info                         *soc_info;
	struct ais_isp_hw_dump_args *dump_args =
		(struct ais_isp_hw_dump_args *)cmd_args;
	int i;
	uint32_t *addr, *start;
	uint32_t num_reg;
	struct ais_isp_hw_dump_header *hdr;
	uint8_t *dst;

	if (!dump_args->cpu_addr || !dump_args->buf_len) {
		CAM_ERR(CAM_ISP,
			"lnvalid len %zu ", dump_args->buf_len);
		return -EINVAL;
	}
	soc_info = &csid_hw->hw_info->soc_info;
	/*100 bytes we store the meta info of the dump data*/
	if ((dump_args->buf_len - dump_args->offset) <
			soc_info->reg_map[0].size + 100) {
		CAM_ERR(CAM_ISP, "Dump buffer exhaust");
		return 0;
	}
	dst = (char *)dump_args->cpu_addr + dump_args->offset;
	hdr = (struct ais_isp_hw_dump_header *)dst;
	snprintf(hdr->tag, AIS_ISP_HW_DUMP_TAG_MAX_LEN,
		"CSID_REG:");
	addr = (uint32_t *)(dst + sizeof(struct ais_isp_hw_dump_header));

	start = addr;
	num_reg = soc_info->reg_map[0].size/4;
	hdr->word_size = sizeof(uint32_t);
	*addr = soc_info->index;
	addr++;
	for (i = 0; i < num_reg; i++) {
		addr[0] = soc_info->mem_block[0]->start + (i*4);
		addr[1] = cam_io_r(soc_info->reg_map[0].mem_base
			+ (i*4));
		addr += 2;
	}
	hdr->size = hdr->word_size * (addr - start);
	dump_args->offset +=  hdr->size +
		sizeof(struct ais_isp_hw_dump_header);
	CAM_DBG(CAM_ISP, "offset %d", dump_args->offset);
	return 0;
}

static int ais_ife_csid_process_cmd(void *hw_priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = 0;
	struct ais_ife_csid_hw               *csid_hw;
	struct cam_hw_info                   *csid_hw_info;

	if (!hw_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid arguments");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct ais_ife_csid_hw   *)csid_hw_info->core_info;

	switch (cmd_type) {
	case AIS_IFE_CSID_CMD_GET_TIME_STAMP:
		rc = ais_ife_csid_get_time_stamp(csid_hw, cmd_args);
		break;
	case AIS_IFE_CSID_SET_CSID_DEBUG:
		rc = ais_ife_csid_set_csid_debug(csid_hw, cmd_args);
		break;
	case AIS_IFE_CSID_SOF_IRQ_DEBUG:
		rc = ais_ife_csid_sof_irq_debug(csid_hw, cmd_args);
		break;
	case AIS_ISP_HW_CMD_CSID_CLOCK_UPDATE:
		rc = ais_ife_csid_set_csid_clock(csid_hw, cmd_args);
		break;
	case AIS_ISP_HW_CMD_DUMP_HW:
		rc = ais_ife_csid_dump_hw(csid_hw, cmd_args);
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d unsupported cmd:%d",
			csid_hw->hw_intf->hw_idx, cmd_type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int ais_csid_event_dispatch_process(void *priv, void *data)
{
	struct ais_ife_event_data evt_payload;
	struct ais_ife_csid_hw *csid_hw;
	struct ais_csid_hw_work_data *work_data;
	int rc = 0;

	csid_hw = (struct ais_ife_csid_hw *)priv;
	if (!csid_hw) {
		CAM_ERR(CAM_ISP, "Invalid parameters");
		return -EINVAL;
	}
	if (!csid_hw->event_cb) {
		CAM_ERR(CAM_ISP, "CSID%d Error Cb not registered",
			csid_hw->hw_intf->hw_idx);
		return -EINVAL;
	}
	work_data = (struct ais_csid_hw_work_data *)data;

	CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID%d [%d %d] TOP:0x%x RX:0x%x",
			csid_hw->hw_intf->hw_idx,
			work_data->evt_type,
			csid_hw->csi2_rx_cfg.phy_sel,
			work_data->irq_status[CSID_IRQ_STATUS_TOP],
			work_data->irq_status[CSID_IRQ_STATUS_RX]);

	CAM_ERR_RATE_LIMIT(CAM_ISP, " RDIs 0x%x 0x%x 0x%x 0x%x",
		work_data->irq_status[CSID_IRQ_STATUS_RDI0],
		work_data->irq_status[CSID_IRQ_STATUS_RDI1],
		work_data->irq_status[CSID_IRQ_STATUS_RDI2],
		work_data->irq_status[CSID_IRQ_STATUS_RDI3]);

	evt_payload.idx = csid_hw->hw_intf->hw_idx;
	evt_payload.boot_ts = work_data->timestamp;
	evt_payload.path = 0xF;

	switch (work_data->evt_type) {
	case AIS_IFE_MSG_CSID_ERROR:
		if (csid_hw->fatal_err_detected)
			break;
		csid_hw->fatal_err_detected = true;

		evt_payload.type = AIS_IFE_MSG_CSID_ERROR;

		rc = csid_hw->event_cb(csid_hw->event_cb_priv, &evt_payload);
		break;

	case AIS_IFE_MSG_CSID_WARNING:
		break;
	default:
		CAM_DBG(CAM_ISP, "CSID[%d] invalid error type %d",
			csid_hw->hw_intf->hw_idx,
			work_data->evt_type);
		break;
	}
	return rc;
}

static int ais_csid_dispatch_irq(struct ais_ife_csid_hw *csid_hw,
	int evt_type, uint32_t *irq_status, uint64_t timestamp)
{
	struct crm_workq_task *task;
	struct ais_csid_hw_work_data *work_data;
	int rc = 0;
	int i;

	CAM_DBG(CAM_ISP, "CSID[%d] error %d",
		csid_hw->hw_intf->hw_idx, evt_type);

	task = cam_req_mgr_workq_get_task(csid_hw->work);
	if (!task) {
		CAM_ERR(CAM_ISP, "Can not get task for worker");
		return -ENOMEM;
	}
	work_data = (struct ais_csid_hw_work_data *)task->payload;
	work_data->evt_type = evt_type;
	work_data->timestamp = timestamp;
	for (i = 0; i < CSID_IRQ_STATUS_MAX; i++)
		work_data->irq_status[i] = irq_status[i];

	task->process_cb = ais_csid_event_dispatch_process;
	rc = cam_req_mgr_workq_enqueue_task(task, csid_hw,
		CRM_TASK_PRIORITY_0);

	return rc;
}

static irqreturn_t ais_ife_csid_irq(int irq_num, void *data)
{
	struct ais_ife_csid_hw                         *csid_hw;
	struct cam_hw_soc_info                         *soc_info;
	const struct ais_ife_csid_reg_offset           *csid_reg;
	const struct ais_ife_csid_csi2_rx_reg_offset   *csi2_reg;
	struct ais_ife_csid_path_cfg                   *path_data;
	const struct ais_ife_csid_rdi_reg_offset       *rdi_reg;
	uint32_t i;
	uint32_t val, val2;
	uint32_t warn_cnt = 0;
	bool fatal_err_detected = false;
	uint32_t sof_irq_debug_en = 0;
	unsigned long flags;
	uint32_t irq_status[CSID_IRQ_STATUS_MAX] = {};
	struct timespec ts;

	if (!data) {
		CAM_ERR(CAM_ISP, "CSID: Invalid arguments");
		return IRQ_HANDLED;
	}

	get_monotonic_boottime64(&ts);

	csid_hw = (struct ais_ife_csid_hw *)data;
	CAM_DBG(CAM_ISP, "CSID %d IRQ Handling", csid_hw->hw_intf->hw_idx);

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	csi2_reg = csid_reg->csi2_reg;

	/* read */
	irq_status[CSID_IRQ_STATUS_TOP] =
		cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_status_addr);

	irq_status[CSID_IRQ_STATUS_RX] =
		cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_status_addr);

	if (csid_reg->cmn_reg->num_pix)
		irq_status[CSID_IRQ_STATUS_IPP] =
			cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_irq_status_addr);

	if (csid_reg->cmn_reg->num_ppp)
		irq_status[CSID_IRQ_STATUS_PPP] =
			cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ppp_reg->csid_pxl_irq_status_addr);

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++)
		irq_status[CSID_IRQ_STATUS_RDI0 + i] =
			cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_irq_status_addr);

	/* clear */
	cam_io_w_mb(irq_status[CSID_IRQ_STATUS_RX],
		soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_clear_addr);
	if (csid_reg->cmn_reg->num_pix)
		cam_io_w_mb(irq_status[CSID_IRQ_STATUS_IPP],
			soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_irq_clear_addr);

	if (csid_reg->cmn_reg->num_ppp)
		cam_io_w_mb(irq_status[CSID_IRQ_STATUS_PPP],
			soc_info->reg_map[0].mem_base +
			csid_reg->ppp_reg->csid_pxl_irq_clear_addr);

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++) {
		cam_io_w_mb(irq_status[CSID_IRQ_STATUS_RDI0 + i],
			soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_irq_clear_addr);
	}
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_irq_cmd_addr);

	if (irq_status[CSID_IRQ_STATUS_RX] &
		BIT(csid_reg->csi2_reg->csi2_rst_done_shift_val)) {
		CAM_DBG(CAM_ISP, "csi rx reset complete");
		complete(&csid_hw->csid_csi2_complete);
	}

	spin_lock_irqsave(&csid_hw->lock_state, flags);
	if (csid_hw->device_enabled == 1) {
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_ERROR_LANE0_FIFO_OVERFLOW) {
			fatal_err_detected = true;
			goto handle_fatal_error;
		}
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_ERROR_LANE1_FIFO_OVERFLOW) {
			fatal_err_detected = true;
			goto handle_fatal_error;
		}
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_ERROR_LANE2_FIFO_OVERFLOW) {
			fatal_err_detected = true;
			goto handle_fatal_error;
		}
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_ERROR_LANE3_FIFO_OVERFLOW) {
			fatal_err_detected = true;
			goto handle_fatal_error;
		}
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_ERROR_TG_FIFO_OVERFLOW) {
			fatal_err_detected = true;
			goto handle_fatal_error;
		}
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_ERROR_CPHY_EOT_RECEPTION) {
			warn_cnt++;
		}
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_ERROR_CPHY_SOT_RECEPTION) {
			warn_cnt++;
		}
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_ERROR_STREAM_UNDERFLOW) {
			warn_cnt++;
		}
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_ERROR_UNBOUNDED_FRAME) {
			warn_cnt++;
		}
	}

	csid_hw->error_irq_count += warn_cnt;

	if (csid_hw->error_irq_count >
		AIS_IFE_CSID_MAX_IRQ_ERROR_COUNT) {
		fatal_err_detected = true;
		csid_hw->error_irq_count = 0;
	} else if (warn_cnt) {
		uint64_t timestamp;

		timestamp = (uint64_t)((ts.tv_sec * 1000000000) + ts.tv_nsec);
		ais_csid_dispatch_irq(csid_hw,
			AIS_IFE_MSG_CSID_WARNING,
			irq_status, timestamp);
	}

handle_fatal_error:
	spin_unlock_irqrestore(&csid_hw->lock_state, flags);
	if (fatal_err_detected) {
		uint64_t timestamp;

		timestamp = (uint64_t)((ts.tv_sec * 1000000000) + ts.tv_nsec);
		CAM_INFO(CAM_ISP,
			"CSID: %d cnt: %d Halt csi2 rx irq_status_rx:0x%x",
			csid_hw->hw_intf->hw_idx, csid_hw->csi2_cfg_cnt,
			irq_status[CSID_IRQ_STATUS_RX]);
		ais_ife_csid_halt_csi2(csid_hw);
		ais_csid_dispatch_irq(csid_hw,
			AIS_IFE_MSG_CSID_ERROR,
			irq_status, timestamp);
	}

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_EOT_IRQ) {
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_INFO_PHY_DL0_EOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL0_EOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_INFO_PHY_DL1_EOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL1_EOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_INFO_PHY_DL2_EOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL2_EOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_INFO_PHY_DL3_EOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL3_EOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
	}

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_SOT_IRQ) {
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_INFO_PHY_DL0_SOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL0_SOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_INFO_PHY_DL1_SOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL1_SOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_INFO_PHY_DL2_SOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL2_SOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status[CSID_IRQ_STATUS_RX] &
			CSID_CSI2_RX_INFO_PHY_DL3_SOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL3_SOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
	}

	if ((csid_hw->csid_debug & CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE) &&
		(irq_status[CSID_IRQ_STATUS_RX] &
		 CSID_CSI2_RX_INFO_LONG_PKT_CAPTURED)) {
		CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID:%d LONG_PKT_CAPTURED",
			csid_hw->hw_intf->hw_idx);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->csid_csi2_rx_captured_long_pkt_0_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"CSID:%d long packet VC :%d DT:%d WC:%d",
			csid_hw->hw_intf->hw_idx,
			(val >> 22), ((val >> 16) & 0x3F), (val & 0xFFFF));
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->csid_csi2_rx_captured_long_pkt_1_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID:%d long packet ECC :%d",
			csid_hw->hw_intf->hw_idx, val);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->csid_csi2_rx_captured_long_pkt_ftr_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"CSID:%d long pkt cal CRC:%d expected CRC:%d",
			csid_hw->hw_intf->hw_idx, (val >> 16), (val & 0xFFFF));
	}
	if ((csid_hw->csid_debug & CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE) &&
		(irq_status[CSID_IRQ_STATUS_RX] &
		 CSID_CSI2_RX_INFO_SHORT_PKT_CAPTURED)) {
		CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID:%d SHORT_PKT_CAPTURED",
			csid_hw->hw_intf->hw_idx);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->csid_csi2_rx_captured_short_pkt_0_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"CSID:%d short pkt VC :%d DT:%d LC:%d",
			csid_hw->hw_intf->hw_idx,
			(val >> 22), ((val >> 16) & 0x1F), (val & 0xFFFF));
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->csid_csi2_rx_captured_short_pkt_1_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID:%d short packet ECC :%d",
			csid_hw->hw_intf->hw_idx, val);
	}

	if ((csid_hw->csid_debug & CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE) &&
		(irq_status[CSID_IRQ_STATUS_RX] &
		 CSID_CSI2_RX_INFO_CPHY_PKT_HDR_CAPTURED)) {
		CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID:%d CPHY_PKT_HDR_CAPTURED",
			csid_hw->hw_intf->hw_idx);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->csid_csi2_rx_captured_cphy_pkt_hdr_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"CSID:%d cphy packet VC :%d DT:%d WC:%d",
			csid_hw->hw_intf->hw_idx,
			(val >> 22), ((val >> 16) & 0x1F), (val & 0xFFFF));
	}

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++) {
		path_data = (struct ais_ife_csid_path_cfg *)
			&csid_hw->rdi_cfg[i];
		rdi_reg = csid_reg->rdi_reg[i];
		if (irq_status[CSID_IRQ_STATUS_RDI0 + i] &
			BIT(csid_reg->cmn_reg->path_rst_done_shift_val)) {
			complete(&csid_hw->csid_rdi_complete[i]);
		}

		if ((irq_status[CSID_IRQ_STATUS_RDI0 + i] &
			CSID_PATH_INFO_INPUT_SOF) &&
			(csid_hw->csid_debug & CSID_DEBUG_ENABLE_SOF_IRQ)) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID RDI:%d SOF received", i);
			if (csid_hw->sof_irq_triggered)
				csid_hw->irq_debug_cnt++;
		}

		if ((irq_status[CSID_IRQ_STATUS_RDI0 + i] &
			CSID_PATH_INFO_INPUT_SOF) &&
			(path_data->init_frame_drop) &&
			(path_data->state ==
			AIS_ISP_RESOURCE_STATE_STREAMING)) {
			path_data->sof_cnt++;
			CAM_DBG(CAM_ISP,
				"CSID:%d RDI:%d SOF cnt:%d init_frame_drop:%d",
				csid_hw->hw_intf->hw_idx, i,
				path_data->sof_cnt,
				path_data->init_frame_drop);
			if (path_data->sof_cnt ==
				path_data->init_frame_drop) {
				cam_io_w_mb(AIS_CSID_RESUME_AT_FRAME_BOUNDARY,
					soc_info->reg_map[0].mem_base +
					rdi_reg->csid_rdi_ctrl_addr);

				path_data->init_frame_drop = 0;

				if (!(csid_hw->csid_debug &
					CSID_DEBUG_ENABLE_SOF_IRQ)) {
					val = cam_io_r_mb(
					soc_info->reg_map[0].mem_base +
					rdi_reg->csid_rdi_irq_mask_addr);
					val &= ~(CSID_PATH_INFO_INPUT_SOF);
					cam_io_w_mb(val,
					soc_info->reg_map[0].mem_base +
					rdi_reg->csid_rdi_irq_mask_addr);
				}
			}
		}

		if ((irq_status[CSID_IRQ_STATUS_RDI0 + i]  &
			CSID_PATH_INFO_INPUT_EOF) &&
			(csid_hw->csid_debug & CSID_DEBUG_ENABLE_EOF_IRQ))
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID RDI:%d EOF received", i);

		if ((irq_status[CSID_IRQ_STATUS_RDI0 + i] &
			CSID_PATH_ERROR_CCIF_VIOLATION) ||
			(irq_status[CSID_IRQ_STATUS_RDI0 + i] &
			 CSID_PATH_ERROR_FIFO_OVERFLOW)) {
			CAM_ERR_RATE_LIMIT(CAM_ISP,
				"CSID:%d irq_status_rdi[%d]:0x%x",
				csid_hw->hw_intf->hw_idx, i,
				irq_status[CSID_IRQ_STATUS_RDI0 + i]);
		}
		if (irq_status[CSID_IRQ_STATUS_RDI0 + i] &
			CSID_PATH_ERROR_FIFO_OVERFLOW) {
			/* Stop RDI path immediately */
			cam_io_w_mb(AIS_CSID_HALT_IMMEDIATELY,
				soc_info->reg_map[0].mem_base +
				csid_reg->rdi_reg[i]->csid_rdi_ctrl_addr);
		}

		if ((irq_status[CSID_IRQ_STATUS_RDI0 + i] &
			CSID_PATH_ERROR_PIX_COUNT) ||
			(irq_status[CSID_IRQ_STATUS_RDI0 + i] &
			CSID_PATH_ERROR_LINE_COUNT)) {
			val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_format_measure0_addr);
			val2 = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_format_measure_cfg1_addr
			);
			CAM_ERR(CAM_ISP,
				"CSID:%d irq_status_rdi[%d]:0x%x",
				csid_hw->hw_intf->hw_idx, i,
				irq_status[CSID_IRQ_STATUS_RDI0 + i]);
			CAM_ERR(CAM_ISP,
			"Expected sz 0x%x*0x%x actual sz 0x%x*0x%x",
			((val2 >>
			csid_reg->cmn_reg->format_measure_height_shift_val) &
			csid_reg->cmn_reg->format_measure_height_mask_val),
			val2 &
			csid_reg->cmn_reg->format_measure_width_mask_val,
			((val >>
			csid_reg->cmn_reg->format_measure_height_shift_val) &
			csid_reg->cmn_reg->format_measure_height_mask_val),
			val &
			csid_reg->cmn_reg->format_measure_width_mask_val);
		}
	}

	if (csid_hw->irq_debug_cnt >= AIS_CSID_IRQ_SOF_DEBUG_CNT_MAX) {
		ais_ife_csid_sof_irq_debug(csid_hw, &sof_irq_debug_en);
		csid_hw->irq_debug_cnt = 0;
	}

	CAM_DBG(CAM_ISP, "IRQ Handling exit");
	return IRQ_HANDLED;
}

int ais_ife_csid_hw_probe_init(struct cam_hw_intf  *csid_hw_intf,
	uint32_t csid_idx)
{
	int rc = -EINVAL;
	uint32_t i;
	struct cam_hw_info                   *csid_hw_info;
	struct ais_ife_csid_hw               *ife_csid_hw = NULL;
	char worker_name[128];

	if (csid_idx >= AIS_IFE_CSID_HW_RES_MAX) {
		CAM_ERR(CAM_ISP, "Invalid csid index:%d", csid_idx);
		return rc;
	}

	csid_hw_info = (struct cam_hw_info  *) csid_hw_intf->hw_priv;
	ife_csid_hw  = (struct ais_ife_csid_hw  *) csid_hw_info->core_info;

	ife_csid_hw->hw_intf = csid_hw_intf;
	ife_csid_hw->hw_info = csid_hw_info;

	CAM_DBG(CAM_ISP, "type %d index %d",
		ife_csid_hw->hw_intf->hw_type, csid_idx);

	ife_csid_hw->device_enabled = 0;
	ife_csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&ife_csid_hw->hw_info->hw_mutex);
	spin_lock_init(&ife_csid_hw->hw_info->hw_lock);
	spin_lock_init(&ife_csid_hw->lock_state);
	init_completion(&ife_csid_hw->hw_info->hw_complete);

	init_completion(&ife_csid_hw->csid_top_complete);
	init_completion(&ife_csid_hw->csid_csi2_complete);
	init_completion(&ife_csid_hw->csid_ipp_complete);
	init_completion(&ife_csid_hw->csid_ppp_complete);
	for (i = 0; i < AIS_IFE_CSID_RDI_MAX; i++)
		init_completion(&ife_csid_hw->csid_rdi_complete[i]);

	rc = ais_ife_csid_init_soc_resources(&ife_csid_hw->hw_info->soc_info,
			ais_ife_csid_irq, ife_csid_hw);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "CSID:%d Failed to init_soc", csid_idx);
		goto err;
	}

	ife_csid_hw->hw_intf->hw_ops.get_hw_caps = ais_ife_csid_get_hw_caps;
	ife_csid_hw->hw_intf->hw_ops.init        = ais_ife_csid_init_hw;
	ife_csid_hw->hw_intf->hw_ops.deinit      = ais_ife_csid_deinit_hw;
	ife_csid_hw->hw_intf->hw_ops.reset       = ais_ife_csid_force_reset;
	ife_csid_hw->hw_intf->hw_ops.reserve     = ais_ife_csid_reserve;
	ife_csid_hw->hw_intf->hw_ops.release     = ais_ife_csid_release;
	ife_csid_hw->hw_intf->hw_ops.start       = ais_ife_csid_start;
	ife_csid_hw->hw_intf->hw_ops.stop        = ais_ife_csid_stop;
	ife_csid_hw->hw_intf->hw_ops.read        = ais_ife_csid_read;
	ife_csid_hw->hw_intf->hw_ops.write       = ais_ife_csid_write;
	ife_csid_hw->hw_intf->hw_ops.process_cmd = ais_ife_csid_process_cmd;

	/* Initialize the RDI resource */
	for (i = 0; i < ife_csid_hw->csid_info->csid_reg->cmn_reg->num_rdis;
				i++) {
		ife_csid_hw->rdi_cfg[i].state =
				AIS_ISP_RESOURCE_STATE_AVAILABLE;
	}

	ife_csid_hw->csid_debug = 0;
	ife_csid_hw->error_irq_count = 0;

	scnprintf(worker_name, sizeof(worker_name),
		"csid%u_worker", ife_csid_hw->hw_intf->hw_idx);
	CAM_DBG(CAM_ISP, "Create CSID worker %s", worker_name);
	rc = cam_req_mgr_workq_create(worker_name,
		AIS_CSID_WORKQ_NUM_TASK,
		&ife_csid_hw->work, CRM_WORKQ_USAGE_IRQ, 0);
	if (rc) {
		CAM_ERR(CAM_ISP, "Unable to create a workq, rc=%d", rc);
		goto err_deinit_soc;
	}

	for (i = 0; i < AIS_CSID_WORKQ_NUM_TASK; i++)
		ife_csid_hw->work->task.pool[i].payload =
			&ife_csid_hw->work_data[i];

	return 0;

err_deinit_soc:
	ais_ife_csid_deinit_soc_resources(&ife_csid_hw->hw_info->soc_info);
err:
	return rc;
}


int ais_ife_csid_hw_deinit(struct ais_ife_csid_hw *ife_csid_hw)
{
	int rc = 0;

	if (ife_csid_hw) {
		ais_ife_csid_deinit_soc_resources(
			&ife_csid_hw->hw_info->soc_info);
		cam_req_mgr_workq_destroy(&ife_csid_hw->work);
	} else {
		CAM_ERR(CAM_ISP, "Invalid param");
		rc = -EINVAL;
	}

	return rc;
}
