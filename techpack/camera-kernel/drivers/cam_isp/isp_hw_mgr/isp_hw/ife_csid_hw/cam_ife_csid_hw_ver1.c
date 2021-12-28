// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/iopoll.h>
#include <linux/slab.h>

#include <media/cam_isp.h>
#include <media/cam_defs.h>
#include <media/cam_req_mgr.h>

#include <dt-bindings/msm-camera.h>

#include "cam_ife_csid_common.h"
#include "cam_ife_csid_hw_ver1.h"
#include "cam_isp_hw.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_tasklet_util.h"
#include "cam_common_util.h"
#include "cam_subdev.h"

#define IFE_CSID_TIMEOUT                               1000

/* Timeout values in usec */
#define CAM_IFE_CSID_TIMEOUT_SLEEP_US                  1000
#define CAM_IFE_CSID_TIMEOUT_ALL_US                    100000

#define CAM_IFE_CSID_RESET_TIMEOUT_MS                  100

/*
 * Constant Factors needed to change QTimer ticks to nanoseconds
 * QTimer Freq = 19.2 MHz
 * Time(us) = ticks/19.2
 * Time(ns) = ticks/19.2 * 1000
 */
#define CAM_IFE_CSID_QTIMER_MUL_FACTOR                 10000
#define CAM_IFE_CSID_QTIMER_DIV_FACTOR                 192

/* Max number of sof irq's triggered in case of SOF freeze */
#define CAM_CSID_IRQ_SOF_DEBUG_CNT_MAX 12

/* Max CSI Rx irq error count threshold value */
#define CAM_IFE_CSID_MAX_IRQ_ERROR_COUNT               100

#define CAM_IFE_CSID_VER1_STATUS_MAX_NUM 32

static const struct cam_ife_csid_irq_desc ver1_rx_irq_desc[] = {
	{
		.desc = "PHY_DL0_EOT_CAPTURED",
	},
	{
		.desc = "PHY_DL1_EOT_CAPTURED",
	},
	{
		.desc = "PHY_DL2_EOT_CAPTURED",
	},
	{
		.desc = "PHY_DL3_EOT_CAPTURED",
	},
	{
		.desc = "PHY_DL0_SOT_CAPTURED",
	},
	{
		.desc = "PHY_DL1_SOT_CAPTURED",
	},
	{
		.desc = "PHY_DL2_SOT_CAPTURED",
	},
	{
		.desc = "PHY_DL3_SOT_CAPTURED",
	},
	{
		.desc = "LONG_PKT",
	},
	{
		.desc = "SHORT_PKT",
	},
	{
		.desc = "CPHY_PKT_HDR",
	},
	{
		.desc = "CPHY_EOT_RECEPTION: No EOT on lane/s",
	},
	{
		.desc = "CPHY_SOT_RECEPTION: Less SOTs on lane/s",
	},
	{
		.desc = "CSID:%d CPHY_PH_CRC CPHY: Pkt Hdr CRC mismatch",
	},
	{
		.desc = "WARNING_ECC",
	},
	{
		.desc = "ERROR_LANE0_FIFO_OVERFLOW",
	},
	{
		.desc = "ERROR_LANE1_FIFO_OVERFLOW",
	},
	{
		.desc = "ERROR_LANE2_FIFO_OVERFLOW",
	},
	{
		.desc = "ERROR_LANE3_FIFO_OVERFLOW",
	},
	{
		.desc = "ERROR_CRC",
	},
	{
		.desc = "ERROR_ECC",
	},
	{
		.desc = "ERROR_MMAPPED_VC_DT",
	},
	{
		.desc = "ERROR_UNMAPPED_VC_DT",
	},
	{
		.desc = "ERROR_STREAM_UNDERFLOW",
	},
	{
		.desc = "ERROR_UNBOUNDED_FRAME",
	},
	{
		.desc = "RST_DONE",
	},
};

static const struct cam_ife_csid_irq_desc ver1_path_irq_desc[] = {
	{
		.desc = "",
	},
	{
		.desc = "Reset_Done",
	},
	{
		.desc = "PATH_ERROR_O/P_FIFO_OVERFLOW: Slow IFE read",
	},
	{
		.desc = "SUBSAMPLED_EOF",
	},
	{
		.desc = "SUBSAMPLED_SOF",
	},
	{
		.desc = "FRAME_DROP_EOF",
	},
	{
		.desc = "FRAME_DROP_EOL",
	},
	{
		.desc = "FRAME_DROP_SOL",
	},
	{
		.desc = "FRAME_DROP_SOF",
	},
	{
		.desc = "INPUT_EOF",
	},
	{
		.desc = "INPUT_EOL",
	},
	{
		.desc = "INPUT_SOL",
	},
	{
		.desc = "INPUT_SOF",
	},
	{
		.desc = "ERROR_PIX_COUNT",
	},
	{
		.desc = "ERROR_LINE_COUNT",
	},
	{
		.desc = "PATH_ERROR_CCIF_VIOLATION: Bad frame timings",
	},
	{
		.desc = "FRAME_DROP",
	},
	{
		.desc =
			"PATH_OVERFLOW_RECOVERY: Back pressure/output fifo ovrfl",
	},
	{
		.desc = "ERROR_REC_CCIF_VIOLATION",
	},
	{
		.desc = "VCDT_GRP0_SEL",
	},
	{
		.desc = "VCDT_GRP1_SEL",
	},
	{
		.desc = "VCDT_GRP_CHANGE",
	},
};

static int cam_ife_csid_ver1_set_debug(
	struct cam_ife_csid_ver1_hw *csid_hw,
	uint32_t debug_val)
{
	int bit_pos = 0;
	uint32_t val;

	memset(&csid_hw->debug_info, 0,
		sizeof(struct cam_ife_csid_debug_info));
	csid_hw->debug_info.debug_val = debug_val;

	while (debug_val) {

		if (!(debug_val & 0x1)) {
			debug_val >>= 1;
			bit_pos++;
			continue;
		}

		val = BIT(bit_pos);

		switch (val) {
		case CAM_IFE_CSID_DEBUG_ENABLE_SOF_IRQ:
			csid_hw->debug_info.path_mask |=
				IFE_CSID_VER1_PATH_INFO_INPUT_SOF;
			break;
		case CAM_IFE_CSID_DEBUG_ENABLE_EOF_IRQ:
			csid_hw->debug_info.path_mask |=
				IFE_CSID_VER1_PATH_INFO_INPUT_EOF;
			break;
		case CAM_IFE_CSID_DEBUG_ENABLE_SOT_IRQ:
			csid_hw->debug_info.rx_mask |=
				IFE_CSID_VER1_RX_DL0_SOT_CAPTURED |
				IFE_CSID_VER1_RX_DL1_SOT_CAPTURED |
				IFE_CSID_VER1_RX_DL2_SOT_CAPTURED;
			break;
		case CAM_IFE_CSID_DEBUG_ENABLE_EOT_IRQ:
			csid_hw->debug_info.rx_mask |=
				IFE_CSID_VER1_RX_DL0_EOT_CAPTURED |
				IFE_CSID_VER1_RX_DL1_EOT_CAPTURED |
				IFE_CSID_VER1_RX_DL2_EOT_CAPTURED;
			break;
		case CAM_IFE_CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE:
			csid_hw->debug_info.rx_mask |=
				IFE_CSID_VER1_RX_SHORT_PKT_CAPTURED;
			break;
		case CAM_IFE_CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE:
			csid_hw->debug_info.rx_mask |=
				IFE_CSID_VER1_RX_LONG_PKT_CAPTURED;
			break;
		case CAM_IFE_CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE:
			csid_hw->debug_info.rx_mask |=
				IFE_CSID_VER1_RX_CPHY_PKT_HDR_CAPTURED;
			break;
		case CAM_IFE_DEBUG_ENABLE_UNMAPPED_VC_DT_IRQ:
			csid_hw->debug_info.rx_mask |=
				IFE_CSID_VER1_RX_UNMAPPED_VC_DT;
			break;
		default:
			break;
		}

		debug_val >>= 1;
		bit_pos++;
	}

	return 0;
}

int cam_ife_csid_ver1_get_hw_caps(void *hw_priv,
	void *get_hw_cap_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_ife_csid_hw_caps           *hw_caps;
	struct cam_ife_csid_ver1_hw           *csid_hw;
	struct cam_hw_info                    *hw_info;
	struct cam_ife_csid_ver1_reg_info *csid_reg;
	struct cam_csid_soc_private *soc_private = NULL;

	if (!hw_priv || !get_hw_cap_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	hw_info = (struct cam_hw_info  *)hw_priv;

	csid_hw = (struct cam_ife_csid_ver1_hw *)hw_info->core_info;
	hw_caps = (struct cam_ife_csid_hw_caps *) get_hw_cap_args;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;
	soc_private = (struct cam_csid_soc_private *)
			csid_hw->hw_info->soc_info.soc_private;

	hw_caps->num_rdis = csid_reg->cmn_reg->num_rdis;
	hw_caps->num_pix = csid_reg->cmn_reg->num_pix;
	hw_caps->num_ppp = csid_reg->cmn_reg->num_ppp;
	hw_caps->major_version = csid_reg->cmn_reg->major_version;
	hw_caps->minor_version = csid_reg->cmn_reg->minor_version;
	hw_caps->version_incr = csid_reg->cmn_reg->version_incr;
	hw_caps->global_reset_en = csid_reg->cmn_reg->global_reset;
	hw_caps->rup_en = csid_reg->cmn_reg->rup_supported;
	hw_caps->is_lite = soc_private->is_ife_csid_lite;

	CAM_DBG(CAM_ISP,
		"CSID:%d No rdis:%d, no pix:%d, major:%d minor:%d ver :%d",
		csid_hw->hw_intf->hw_idx, hw_caps->num_rdis,
		hw_caps->num_pix, hw_caps->major_version,
		hw_caps->minor_version, hw_caps->version_incr);

	return rc;
}

static int cam_ife_csid_ver1_prepare_reset(
	struct cam_ife_csid_ver1_hw     *csid_hw)
{
	struct cam_hw_soc_info                *soc_info;
	struct cam_ife_csid_ver1_reg_info *csid_reg;
	uint32_t i;
	unsigned long flags;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid HW State:%d",
			csid_hw->hw_intf->hw_idx,
			csid_hw->hw_info->hw_state);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "CSID:%d Csid reset",
		csid_hw->hw_intf->hw_idx);

	spin_lock_irqsave(&csid_hw->hw_info->hw_lock, flags);

	/* Mask all interrupts */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->irq_mask_addr);

	if (csid_reg->cmn_reg->num_pix)
		cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->irq_mask_addr);

	if (csid_reg->cmn_reg->num_ppp)
		cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->ppp_reg->irq_mask_addr);

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++)
		cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->irq_mask_addr);

	/* clear all interrupts */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->top_irq_clear_addr);

	cam_io_w_mb(csid_reg->csi2_reg->irq_mask_all,
		soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->irq_clear_addr);

	if (csid_reg->cmn_reg->num_pix)
		cam_io_w_mb(csid_reg->cmn_reg->ipp_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->irq_clear_addr);

	if (csid_reg->cmn_reg->num_ppp)
		cam_io_w_mb(csid_reg->cmn_reg->ppp_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->ppp_reg->irq_clear_addr);

	for (i = 0 ; i < csid_reg->cmn_reg->num_rdis; i++)
		cam_io_w_mb(csid_reg->cmn_reg->rdi_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->irq_clear_addr);

	for (i = 0 ; i < csid_reg->cmn_reg->num_udis; i++)
		cam_io_w_mb(csid_reg->cmn_reg->udi_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->udi_reg[i]->irq_clear_addr);

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->irq_cmd_addr);

	spin_unlock_irqrestore(&csid_hw->hw_info->hw_lock, flags);

	cam_io_w_mb(0x80, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->cfg1_addr);

	/* enable the IPP and RDI format measure */
	if (csid_reg->cmn_reg->num_pix)
		cam_io_w_mb(0x1, soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->cfg0_addr);

	if (csid_reg->cmn_reg->num_ppp)
		cam_io_w_mb(0x1, soc_info->reg_map[0].mem_base +
			csid_reg->ppp_reg->cfg0_addr);

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++)
		cam_io_w_mb(0x2, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->cfg0_addr);
	return 0;
}

static int cam_ife_csid_ver1_hw_reset(
	struct cam_ife_csid_ver1_hw     *csid_hw)
{
	int rc = 0;
	const struct cam_ife_csid_ver1_reg_info *csid_reg;
	struct cam_hw_soc_info          *soc_info;
	uint32_t val = 0;
	unsigned long flags, rem_jiffies = 0;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	reinit_completion(
		&csid_hw->irq_complete[CAM_IFE_CSID_IRQ_REG_TOP]);

	spin_lock_irqsave(&csid_hw->hw_info->hw_lock, flags);

	csid_hw->flags.process_reset = true;

	/* clear the top interrupt first */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->top_irq_clear_addr);
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->irq_cmd_addr);

	/* enable top reset complete IRQ */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->top_irq_mask_addr);
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->irq_cmd_addr);

	/* perform the top CSID registers reset */
	val = csid_reg->cmn_reg->rst_hw_reg_stb;

	cam_io_w_mb(val,
		soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->rst_strobes_addr);

	spin_unlock_irqrestore(&csid_hw->hw_info->hw_lock, flags);
	CAM_DBG(CAM_ISP, "CSID reset start");

	rem_jiffies = cam_common_wait_for_completion_timeout(
			&csid_hw->irq_complete[CAM_IFE_CSID_IRQ_REG_TOP],
			msecs_to_jiffies(CAM_IFE_CSID_RESET_TIMEOUT_MS));

	if (rem_jiffies == 0) {
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->cmn_reg->top_irq_status_addr);
		if (val & 0x1) {
			/* clear top reset IRQ */
			cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
				csid_reg->cmn_reg->top_irq_clear_addr);
			cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
				csid_reg->cmn_reg->irq_cmd_addr);
			CAM_DBG(CAM_ISP, "CSID:%d HW reset completed %d",
				csid_hw->hw_intf->hw_idx,
				rem_jiffies);
			goto end;
		}
		CAM_ERR(CAM_ISP, "CSID:%d hw csid_reset fail rc = %d",
			rem_jiffies);
		rc = -ETIMEDOUT;
	} else {
		CAM_DBG(CAM_ISP, "CSID:%d hw reset completed %d",
			csid_hw->hw_intf->hw_idx,
			rem_jiffies);
	}
end:
	csid_hw->flags.process_reset = false;
	return rc;
}

static int cam_ife_csid_ver1_sw_reset(
	struct cam_ife_csid_ver1_hw  *csid_hw)
{
	int rc = 0;
	const struct cam_ife_csid_ver1_reg_info *csid_reg;
	struct cam_hw_soc_info          *soc_info;
	uint32_t val = 0;
	unsigned long flags, rem_jiffies = 0;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	reinit_completion(
		&csid_hw->irq_complete[CAM_IFE_CSID_IRQ_REG_TOP]);

	spin_lock_irqsave(&csid_hw->hw_info->hw_lock, flags);

	csid_hw->flags.process_reset = true;

	/* clear the top interrupt first */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->top_irq_clear_addr);
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->irq_cmd_addr);

	/* perform the top CSID registers reset */
	val = csid_reg->cmn_reg->rst_sw_reg_stb;
	cam_io_w_mb(val,
		soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->rst_strobes_addr);

	/*
	 * for SW reset, we enable the IRQ after since the mask
	 * register has been reset
	 */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->top_irq_mask_addr);
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->irq_cmd_addr);

	spin_unlock_irqrestore(&csid_hw->hw_info->hw_lock, flags);
	CAM_DBG(CAM_ISP, "CSID[%d] top reset start",
		csid_hw->hw_intf->hw_idx);

	rem_jiffies = cam_common_wait_for_completion_timeout(
			&csid_hw->irq_complete[CAM_IFE_CSID_IRQ_REG_TOP],
			msecs_to_jiffies(CAM_IFE_CSID_RESET_TIMEOUT_MS));

	if (rem_jiffies == 0) {
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->cmn_reg->top_irq_status_addr);
		if (val & 0x1) {
			/* clear top reset IRQ */
			cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
				csid_reg->cmn_reg->top_irq_clear_addr);
			cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
				csid_reg->cmn_reg->irq_cmd_addr);
			CAM_DBG(CAM_ISP, "CSID:%d sw reset completed %d",
				csid_hw->hw_intf->hw_idx,
				rem_jiffies);
			goto end;
		}
		CAM_ERR(CAM_ISP, "CSID:%d sw csid_reset fail rc = %d",
			csid_hw->hw_intf->hw_idx,
			rem_jiffies);
		rc = -ETIMEDOUT;
	} else {
		CAM_DBG(CAM_ISP, "CSID:%d sw reset completed %d",
			csid_hw->hw_intf->hw_idx,
			rem_jiffies);
	}
end:
	csid_hw->flags.process_reset = false;
	return rc;
}

static int cam_ife_csid_ver1_global_reset(
		struct cam_ife_csid_ver1_hw *csid_hw)
{
	int rc = 0;

	rc = cam_ife_csid_ver1_prepare_reset(csid_hw);

	if (rc) {
		CAM_ERR(CAM_ISP, "CSID[%d] prepare reset failed");
		goto end;
	}

	rc = cam_ife_csid_ver1_hw_reset(csid_hw);

	if (rc) {
		CAM_ERR(CAM_ISP, "CSID[%d] hw reset failed");
		goto end;
	}

	rc = cam_ife_csid_ver1_sw_reset(csid_hw);

	if (rc)
		CAM_ERR(CAM_ISP, "CSID[%d] sw reset failed");
end:
	return rc;
}

static int cam_ife_csid_path_reset(
	struct cam_ife_csid_ver1_hw *csid_hw,
	struct cam_csid_reset_cfg_args  *reset)
{
	const struct cam_ife_csid_ver1_path_reg_info *path_reg = NULL;
	const struct cam_ife_csid_ver1_reg_info      *csid_reg;
	struct cam_isp_resource_node                 *res;
	struct cam_hw_soc_info                       *soc_info;
	unsigned long                                 rem_jiffies;
	uint32_t                                      val;
	int                                           rc = 0;
	int                                           irq_reg = 0;
	int                                           id = 0;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;
	res = reset->node_res;

	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid hw state :%d",
			csid_hw->hw_intf->hw_idx,
			csid_hw->hw_info->hw_state);
		return -EINVAL;
	}

	if (res->res_id >= CAM_IFE_PIX_PATH_RES_MAX) {
		CAM_DBG(CAM_ISP, "CSID:%d Invalid res id%d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "CSID:%d reset Resource[id:%d name:%s]",
		csid_hw->hw_intf->hw_idx, res->res_id, res->res_name);

	switch (res->res_id) {
	case CAM_IFE_PIX_PATH_RES_IPP:
		path_reg = csid_reg->ipp_reg;
		break;
	case CAM_IFE_PIX_PATH_RES_PPP:
		path_reg = csid_reg->ppp_reg;
		break;
	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
		path_reg = csid_reg->rdi_reg[res->res_id];
		break;
	case CAM_IFE_PIX_PATH_RES_UDI_0:
	case CAM_IFE_PIX_PATH_RES_UDI_1:
	case CAM_IFE_PIX_PATH_RES_UDI_2:
		id = res->res_id - CAM_IFE_PIX_PATH_RES_UDI_0;
		path_reg = csid_reg->udi_reg[id];
		break;
	default:
		break;
	}

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "Invalid res %d", res->res_id);
		return -EINVAL;
	}


	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		path_reg->irq_mask_addr);
	val |= 1 << csid_reg->cmn_reg->rst_done_shift_val;
	irq_reg = cam_ife_csid_convert_res_to_irq_reg(res->res_id);
	reinit_completion(&csid_hw->irq_complete[irq_reg]);

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		path_reg->irq_mask_addr);
	cam_io_w_mb(csid_reg->cmn_reg->path_rst_stb_all,
		soc_info->reg_map[0].mem_base + path_reg->rst_strobes_addr);

	rem_jiffies = cam_common_wait_for_completion_timeout(
			&csid_hw->irq_complete[irq_reg],
			msecs_to_jiffies(IFE_CSID_TIMEOUT));

	CAM_DBG(CAM_ISP, "CSID:%d resource[id:%d  name:%s] reset done",
		csid_hw->hw_intf->hw_idx, res->res_id, res->res_name);

	if (!rem_jiffies) {
		rc = -ETIMEDOUT;
		CAM_ERR(CAM_ISP, "CSID:%d Res id %d fail rc = %d",
			csid_hw->hw_intf->hw_idx,
			res->res_id, rc);
	}

	return rc;
}

int cam_ife_csid_ver1_reset(void *hw_priv,
	void *reset_args, uint32_t arg_size)
{
	struct cam_hw_info *hw_info;
	struct cam_ife_csid_ver1_hw *csid_hw;
	struct cam_csid_reset_cfg_args  *reset;
	int rc = 0;

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver1_hw *)hw_info->core_info;
	reset   = (struct cam_csid_reset_cfg_args  *)reset_args;

	mutex_lock(&csid_hw->hw_info->hw_mutex);

	switch (reset->reset_type) {
	case CAM_IFE_CSID_RESET_GLOBAL:
		rc = cam_ife_csid_ver1_global_reset(csid_hw);
		break;
	case CAM_IFE_CSID_RESET_PATH:
		rc = cam_ife_csid_path_reset(csid_hw, reset);
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:Invalid reset type :%d",
			reset->reset_type);
		rc = -EINVAL;
		break;
	}

	CAM_DBG(CAM_ISP, "CSID[%d] reset type :%d",
		csid_hw->hw_intf->hw_idx,
		reset->reset_type);

	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_ife_csid_ver1_deinit_rdi_path(
	struct cam_ife_csid_ver1_hw     *csid_hw,
	struct cam_isp_resource_node    *res)
{
	const struct cam_ife_csid_ver1_path_reg_info *path_reg;
	const struct cam_ife_csid_ver1_reg_info      *csid_reg;
	struct cam_ife_csid_core_info                *core_info;
	struct cam_hw_soc_info                       *soc_info;
	void __iomem                                 *mem_base;
	int                                           rc = 0;
	uint32_t                                      val;

	if (res->res_state != CAM_ISP_RESOURCE_STATE_INIT_HW ||
			res->res_id > CAM_IFE_PIX_PATH_RES_RDI_4) {
		CAM_ERR(CAM_ISP,
			"CSID:%d %s path res type:%d res_id:%d Invalid state%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	core_info = csid_hw->core_info;
	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)core_info->csid_reg;

	path_reg = csid_reg->rdi_reg[res->res_id];

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d RDI:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	mem_base = soc_info->reg_map[0].mem_base;

	if (path_reg->overflow_ctrl_en)
		cam_io_w_mb(0, mem_base +
			path_reg->err_recovery_cfg0_addr);

	val = cam_io_r_mb(mem_base + path_reg->cfg0_addr);

	if (val & path_reg->format_measure_en_shift_val) {
		val &= ~path_reg->format_measure_en_shift_val;
		cam_io_w_mb(val, mem_base +
			path_reg->cfg0_addr);

		/* Disable the HBI/VBI counter */
		val = cam_io_r_mb(mem_base +
			path_reg->format_measure_cfg0_addr);
		val &= ~csid_reg->cmn_reg->measure_en_hbi_vbi_cnt_mask;
		cam_io_w_mb(val, mem_base +
			path_reg->format_measure_cfg0_addr);
	}

	res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	return rc;
}

static int cam_ife_csid_ver1_deinit_udi_path(
	struct cam_ife_csid_ver1_hw     *csid_hw,
	struct cam_isp_resource_node    *res)
{
	const struct cam_ife_csid_ver1_path_reg_info *path_reg;
	const struct cam_ife_csid_ver1_reg_info      *csid_reg;
	struct cam_ife_csid_core_info                *core_info;
	struct cam_hw_soc_info                       *soc_info;
	void __iomem                                 *mem_base;
	uint32_t                                      val, id;
	int                                           rc = 0;

	if ((res->res_state != CAM_ISP_RESOURCE_STATE_INIT_HW) ||
			(res->res_id < CAM_IFE_PIX_PATH_RES_UDI_0 ||
			 res->res_id > CAM_IFE_PIX_PATH_RES_UDI_2)) {
		CAM_ERR(CAM_ISP,
			"CSID:%d %s path res type:%d res_id:%d Invalid state%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	core_info = csid_hw->core_info;
	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)core_info->csid_reg;

	id =  res->res_id > CAM_IFE_PIX_PATH_RES_UDI_0;
	path_reg = csid_reg->udi_reg[id];

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d RDI:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	mem_base = soc_info->reg_map[0].mem_base;

	if (path_reg->overflow_ctrl_en)
		cam_io_w_mb(0, mem_base +
			path_reg->err_recovery_cfg0_addr);

	val = cam_io_r_mb(mem_base + path_reg->cfg0_addr);

	if (val & BIT(path_reg->format_measure_en_shift_val)) {
		val &= ~BIT(path_reg->format_measure_en_shift_val);
		cam_io_w_mb(val, mem_base +
			path_reg->cfg0_addr);

		/* Disable the HBI/VBI counter */
		val = cam_io_r_mb(mem_base +
			path_reg->format_measure_cfg0_addr);
		val &= ~csid_reg->cmn_reg->measure_en_hbi_vbi_cnt_mask;
		cam_io_w_mb(val, mem_base +
			path_reg->format_measure_cfg0_addr);
	}

	res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	return rc;
}
static int cam_ife_csid_ver1_deinit_pxl_path(
	struct cam_ife_csid_ver1_hw     *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	void __iomem *mem_base;
	uint32_t  val;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_ife_csid_ver1_reg_info *csid_reg;
	const struct cam_ife_csid_ver1_path_reg_info *path_reg = NULL;
	struct cam_ife_csid_core_info  *core_info;

	if (res->res_state != CAM_ISP_RESOURCE_STATE_INIT_HW) {
		CAM_ERR(CAM_ISP,
			"CSID:%d %s path res type:%d res_id:%d Invalid state%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	core_info = csid_hw->core_info;
	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)core_info->csid_reg;

	if (res->res_id == CAM_IFE_PIX_PATH_RES_IPP)
		path_reg = csid_reg->ipp_reg;
	else if (res->res_id == CAM_IFE_PIX_PATH_RES_PPP)
		path_reg = csid_reg->ppp_reg;

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d PIX:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	mem_base = soc_info->reg_map[0].mem_base;

	if (path_reg->overflow_ctrl_en)
		cam_io_w_mb(0, mem_base +
			path_reg->err_recovery_cfg0_addr);

	val = cam_io_r_mb(mem_base + path_reg->cfg0_addr);

	if (val & path_reg->format_measure_en_shift_val) {
		val &= ~path_reg->format_measure_en_shift_val;
		cam_io_w_mb(val, mem_base +
			path_reg->cfg0_addr);

		/* Disable the HBI/VBI counter */
		val = cam_io_r_mb(mem_base +
			path_reg->format_measure_cfg0_addr);
		val &= ~csid_reg->cmn_reg->measure_en_hbi_vbi_cnt_mask;
		cam_io_w_mb(val, mem_base +
			path_reg->format_measure_cfg0_addr);
	}

	res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	return rc;
}

static int cam_ife_csid_ver1_stop_pxl_path(
	struct cam_ife_csid_ver1_hw     *csid_hw,
	struct cam_isp_resource_node    *res,
	enum cam_ife_csid_halt_cmd       stop_cmd)
{
	int rc = 0;
	uint32_t  val;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_ife_csid_ver1_reg_info *csid_reg;
	const struct cam_ife_csid_ver1_path_reg_info *path_reg = NULL;
	struct cam_ife_csid_ver1_path_cfg *path_cfg;
	uint32_t halt_cmd = 0;

	if (res->res_state != CAM_ISP_RESOURCE_STATE_STREAMING) {
		CAM_ERR(CAM_ISP,
			"CSID:%d %s path res type:%d res_id:%d Invalid state%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	if (res->res_id >= CAM_IFE_PIX_PATH_RES_MAX) {
		CAM_DBG(CAM_ISP, "CSID:%d Invalid res id%d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	if (res->res_id == CAM_IFE_PIX_PATH_RES_IPP)
		path_reg = csid_reg->ipp_reg;
	else if (res->res_id == CAM_IFE_PIX_PATH_RES_PPP)
		path_reg = csid_reg->ppp_reg;

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d PIX:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	path_cfg  = (struct cam_ife_csid_ver1_path_cfg *)res->res_priv;

	if (stop_cmd == CAM_CSID_HALT_IMMEDIATELY) {
		halt_cmd = path_reg->halt_immediate;
	} else if (stop_cmd == CAM_CSID_HALT_AT_FRAME_BOUNDARY) {
		halt_cmd = path_reg->halt_frame_boundary;
	} else {
		CAM_ERR(CAM_ISP, "CSID:%d un supported stop command:%d",
			csid_hw->hw_intf->hw_idx, stop_cmd);
		return -EINVAL;
	}

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		path_reg->irq_mask_addr);

	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		path_reg->ctrl_addr);

	if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_MASTER ||
		path_cfg->sync_mode == CAM_ISP_HW_SYNC_NONE) {
		/* configure Halt for master */
		val &= ~0x3;
		val |= halt_cmd;
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			path_reg->ctrl_addr);
	}

	if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_SLAVE &&
		stop_cmd == CAM_CSID_HALT_IMMEDIATELY) {
		/* configure Halt for slave */
		val &= ~0xF;
		val |= halt_cmd;
		val |= (path_reg->halt_mode_master <<
			path_reg->halt_mode_shift);
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			path_reg->ctrl_addr);
	}

	return rc;
}

static int cam_ife_csid_ver1_stop_rdi_path(
	struct cam_ife_csid_ver1_hw     *csid_hw,
	struct cam_isp_resource_node    *res,
	enum cam_ife_csid_halt_cmd       stop_cmd)
{
	const struct cam_ife_csid_ver1_path_reg_info *path_reg;
	struct cam_ife_csid_ver1_reg_info            *csid_reg;
	struct cam_ife_csid_core_info                *core_info;
	struct cam_hw_soc_info                       *soc_info;
	void __iomem                                 *mem_base;
	uint32_t                                      val;
	int                                           rc = 0;

	if (res->res_state != CAM_ISP_RESOURCE_STATE_STREAMING) {
		CAM_ERR(CAM_ISP,
			"CSID:%d %s path res type:%d res_id:%d Invalid state%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	if (res->res_id >= CAM_IFE_CSID_RDI_MAX) {
		CAM_DBG(CAM_ISP, "CSID:%d Invalid res id%d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	core_info = csid_hw->core_info;
	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)core_info->csid_reg;
	path_reg = csid_reg->rdi_reg[res->res_id];

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d PIX:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	mem_base = soc_info->reg_map[0].mem_base;

	if (stop_cmd != CAM_CSID_HALT_AT_FRAME_BOUNDARY &&
		stop_cmd != CAM_CSID_HALT_IMMEDIATELY) {
		CAM_ERR(CAM_ISP, "CSID:%d un supported stop command:%d",
			csid_hw->hw_intf->hw_idx, stop_cmd);
		return -EINVAL;
	}

	cam_io_w_mb(0, mem_base + path_reg->irq_mask_addr);

	val = cam_io_r_mb(mem_base + path_reg->ctrl_addr);
	val &= ~0x3;
	val |= stop_cmd;
	cam_io_w_mb(val, mem_base + path_reg->ctrl_addr);

	return rc;
}

static int cam_ife_csid_ver1_stop_udi_path(
	struct cam_ife_csid_ver1_hw     *csid_hw,
	struct cam_isp_resource_node    *res,
	enum cam_ife_csid_halt_cmd       stop_cmd)
{
	const struct cam_ife_csid_ver1_path_reg_info *path_reg;
	struct cam_ife_csid_ver1_reg_info            *csid_reg;
	struct cam_ife_csid_core_info                *core_info;
	struct cam_hw_soc_info                       *soc_info;
	void __iomem                                 *mem_base;
	uint32_t                                      val, id;
	int                                           rc = 0;

	if (res->res_state != CAM_ISP_RESOURCE_STATE_STREAMING) {
		CAM_ERR(CAM_ISP,
			"CSID:%d %s path res type:%d res_id:%d Invalid state%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	if (res->res_id >= CAM_IFE_PIX_PATH_RES_MAX) {
		CAM_DBG(CAM_ISP, "CSID:%d Invalid res id%d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	id = res->res_id - CAM_IFE_PIX_PATH_RES_UDI_0;
	core_info = csid_hw->core_info;
	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)core_info->csid_reg;
	path_reg = csid_reg->udi_reg[id];

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d RDI:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	mem_base = soc_info->reg_map[0].mem_base;

	if (stop_cmd != CAM_CSID_HALT_AT_FRAME_BOUNDARY &&
		stop_cmd != CAM_CSID_HALT_IMMEDIATELY) {
		CAM_ERR(CAM_ISP, "CSID:%d un supported stop command:%d",
			csid_hw->hw_intf->hw_idx, stop_cmd);
		return -EINVAL;
	}

	cam_io_w_mb(0, mem_base + path_reg->irq_mask_addr);

	val = cam_io_r_mb(mem_base + path_reg->ctrl_addr);
	val &= ~0x3;
	val |= stop_cmd;
	cam_io_w_mb(val, mem_base + path_reg->ctrl_addr);

	return rc;
}

static int cam_ife_csid_hw_ver1_path_cfg(
	struct cam_ife_csid_ver1_hw *csid_hw,
	struct cam_ife_csid_ver1_path_cfg *path_cfg,
	struct cam_csid_hw_reserve_resource_args  *reserve,
	uint32_t cid)
{

	path_cfg->cid = cid;
	path_cfg->in_format = reserve->in_port->format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0];
	path_cfg->out_format = reserve->out_port->format;
	path_cfg->sync_mode = reserve->sync_mode;
	path_cfg->height  = reserve->in_port->height;
	path_cfg->start_line = reserve->in_port->line_start;
	path_cfg->end_line = reserve->in_port->line_stop;
	path_cfg->crop_enable = reserve->crop_enable;
	path_cfg->drop_enable = reserve->drop_enable;
	path_cfg->horizontal_bin = reserve->in_port->horizontal_bin;
	path_cfg->qcfa_bin = reserve->in_port->qcfa_bin;
	path_cfg->num_bytes_out = reserve->in_port->num_bytes_out;

	if (reserve->sync_mode == CAM_ISP_HW_SYNC_MASTER) {
		path_cfg->start_pixel = reserve->in_port->left_start;
		path_cfg->end_pixel = reserve->in_port->left_stop;
		path_cfg->width  = reserve->in_port->left_width;

		if (reserve->res_id >= CAM_IFE_PIX_PATH_RES_RDI_0 &&
			reserve->res_id <= (CAM_IFE_PIX_PATH_RES_RDI_0 +
			CAM_IFE_CSID_RDI_MAX - 1)) {
			path_cfg->end_pixel = reserve->in_port->right_stop;
			path_cfg->width = path_cfg->end_pixel -
				path_cfg->start_pixel + 1;
		}

		CAM_DBG(CAM_ISP,
			"CSID:%d res:%d master:startpixel 0x%x endpixel:0x%x",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			path_cfg->start_pixel, path_cfg->end_pixel);
		CAM_DBG(CAM_ISP,
			"CSID:%d res:%d master:line start:0x%x line end:0x%x",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			path_cfg->start_line, path_cfg->end_line);
	} else if (reserve->sync_mode == CAM_ISP_HW_SYNC_SLAVE) {
		path_cfg->start_pixel = reserve->in_port->right_start;
		path_cfg->end_pixel = reserve->in_port->right_stop;
		path_cfg->width  = reserve->in_port->right_width;
		CAM_DBG(CAM_ISP,
			"CSID:%d res:%d slave:start:0x%x end:0x%x width 0x%x",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			path_cfg->start_pixel, path_cfg->end_pixel,
			path_cfg->width);
		CAM_DBG(CAM_ISP,
			"CSID:%d res:%d slave:line start:0x%x line end:0x%x",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			path_cfg->start_line, path_cfg->end_line);
	} else {
		path_cfg->width  = reserve->in_port->left_width;
		path_cfg->start_pixel = reserve->in_port->left_start;
		path_cfg->end_pixel = reserve->in_port->left_stop;
		CAM_DBG(CAM_ISP,
			"CSID:%d res:%d left width %d start: %d stop:%d",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			reserve->in_port->left_width,
			reserve->in_port->left_start,
			reserve->in_port->left_stop);
	}
	return 0;
}

static int cam_ife_csid_ver1_rx_capture_config(
	struct cam_ife_csid_ver1_hw *csid_hw)
{
	const struct cam_ife_csid_ver1_reg_info   *csid_reg;
	struct cam_hw_soc_info                    *soc_info;
	struct cam_ife_csid_rx_cfg                *rx_cfg;
	uint32_t vc, dt, i;
	uint32_t val = 0;

	for (i = 0; i < CAM_IFE_CSID_CID_MAX; i++)
		if (csid_hw->cid_data[i].cid_cnt)
			break;

	if (i == CAM_IFE_CSID_CID_MAX) {
		CAM_WARN(CAM_ISP, "CSID[%d] no valid cid",
			csid_hw->hw_intf->hw_idx);
		return 0;
	}
	rx_cfg = &csid_hw->rx_cfg;

	vc  = csid_hw->cid_data[i].vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc;
	dt  = csid_hw->cid_data[i].vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt;

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (csid_hw->debug_info.debug_val &
			CAM_IFE_CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE)
		val = ((1 << csid_reg->csi2_reg->capture_short_pkt_en_shift) |
			(vc << csid_reg->csi2_reg->capture_short_pkt_vc_shift));

	/* CAM_IFE_CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE */
	val |= ((1 << csid_reg->csi2_reg->capture_long_pkt_en_shift) |
		(dt << csid_reg->csi2_reg->capture_long_pkt_dt_shift) |
		(vc << csid_reg->csi2_reg->capture_long_pkt_vc_shift));

	/* CAM_IFE_CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE*/
	if (rx_cfg->lane_type == CAM_ISP_LANE_TYPE_CPHY) {
		val |= ((1 << csid_reg->csi2_reg->capture_cphy_pkt_en_shift) |
			(dt << csid_reg->csi2_reg->capture_cphy_pkt_dt_shift) |
			(vc << csid_reg->csi2_reg->capture_cphy_pkt_vc_shift));
	}

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->capture_ctrl_addr);
	return 0;
}

static int cam_ife_csid_ver1_tpg_config(
	struct cam_ife_csid_ver1_hw    *csid_hw,
	struct cam_csid_hw_reserve_resource_args  *reserve)
{

	if (csid_hw->flags.tpg_configured)
		return 0;

	switch (reserve->in_port->test_pattern) {
	case CAM_ISP_PATTERN_BAYER_RGRGRG:
	case CAM_ISP_PATTERN_BAYER_GRGRGR:
	case CAM_ISP_PATTERN_BAYER_BGBGBG:
	case CAM_ISP_PATTERN_BAYER_GBGBGB:
		csid_hw->tpg_cfg.test_pattern = reserve->in_port->test_pattern;
		break;
	case CAM_ISP_PATTERN_YUV_YCBYCR:
	case CAM_ISP_PATTERN_YUV_YCRYCB:
	case CAM_ISP_PATTERN_YUV_CBYCRY:
	case CAM_ISP_PATTERN_YUV_CRYCBY:
		csid_hw->tpg_cfg.test_pattern =
			CAM_IFE_CSID_TPG_TEST_PATTERN_YUV;
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID[%d] invalid test_pattern %d",
			csid_hw->hw_intf->hw_idx,
			reserve->in_port->test_pattern);
		return -EINVAL;
	}

	switch (reserve->in_port->format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0]) {
	case CAM_FORMAT_MIPI_RAW_8:
		csid_hw->tpg_cfg.encode_format = CAM_IFE_CSID_TPG_ENCODE_RAW8;
		break;
	case CAM_FORMAT_MIPI_RAW_10:
		csid_hw->tpg_cfg.encode_format = CAM_IFE_CSID_TPG_ENCODE_RAW10;
		break;
	case CAM_FORMAT_MIPI_RAW_12:
		csid_hw->tpg_cfg.encode_format = CAM_IFE_CSID_TPG_ENCODE_RAW12;
		break;
	case CAM_FORMAT_MIPI_RAW_14:
		csid_hw->tpg_cfg.encode_format = CAM_IFE_CSID_TPG_ENCODE_RAW14;
		break;
	case CAM_FORMAT_MIPI_RAW_16:
		csid_hw->tpg_cfg.encode_format = CAM_IFE_CSID_TPG_ENCODE_RAW16;
		break;
	case CAM_FORMAT_YUV422:
		csid_hw->tpg_cfg.encode_format = CAM_IFE_CSID_TPG_ENCODE_RAW8;
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID[%d] invalid input format %d",
			csid_hw->hw_intf->hw_idx,
			reserve->in_port->format);
		return -EINVAL;
	}

	if (reserve->in_port->usage_type)
		csid_hw->tpg_cfg.width = reserve->in_port->right_stop + 1;
	else
		csid_hw->tpg_cfg.width = reserve->in_port->left_width;
	csid_hw->tpg_cfg.height = reserve->in_port->height;
	csid_hw->flags.tpg_configured = true;
	csid_hw->tpg_cfg.vc =  reserve->in_port->vc[0];
	csid_hw->tpg_cfg.dt =  reserve->in_port->dt[0];
	return 0;
}

static int cam_ife_csid_ver1_tpg_start(
	struct cam_ife_csid_ver1_hw *csid_hw)
{
	int rc = 0;
	uint32_t  val = 0;
	uint32_t i;
	uint32_t base;
	struct cam_hw_soc_info    *soc_info;
	const struct cam_ife_csid_ver1_reg_info *csid_reg = NULL;

	if (csid_hw->flags.tpg_enabled)
		return 0;

	/*Enable the TPG */
	CAM_DBG(CAM_ISP, "CSID:%d start CSID TPG",
		csid_hw->hw_intf->hw_idx);

	soc_info = &csid_hw->hw_info->soc_info;

	CAM_DBG(CAM_ISP, "================ TPG ============");
	base = 0x600;

	for (i = 0; i < 16; i++) {
		val = cam_io_r_mb(
			soc_info->reg_map[0].mem_base +
			base + i * 4);
		CAM_DBG(CAM_ISP, "reg 0x%x = 0x%x",
			(base + i*4), val);
	}

	CAM_DBG(CAM_ISP, "================ IPP =============");
	base = 0x200;
	for (i = 0; i < 10; i++) {
		val = cam_io_r_mb(
			soc_info->reg_map[0].mem_base +
			base + i * 4);
		CAM_DBG(CAM_ISP, "reg 0x%x = 0x%x",
			(base + i*4), val);
	}

	CAM_DBG(CAM_ISP, "================ RX =============");
	base = 0x100;
	for (i = 0; i < 5; i++) {
		val = cam_io_r_mb(
			soc_info->reg_map[0].mem_base +
			base + i * 4);
		CAM_DBG(CAM_ISP, "reg 0x%x = 0x%x",
			(base + i*4), val);
	}

	/* Enable the IFE force clock on for dual isp case */
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	if (csid_hw->tpg_cfg.usage_type) {
		rc = cam_ife_csid_enable_ife_force_clock_on(soc_info,
			csid_reg->tpg_reg->cpas_ife_reg_offset);
		if (rc)
			return rc;
	}

	CAM_DBG(CAM_ISP, "============ TPG control ============");
	val = csid_reg->tpg_reg->ctrl_cfg |
		    ((csid_hw->rx_cfg.lane_num - 1) &
		    csid_reg->tpg_reg->num_active_lanes_mask);

	csid_hw->flags.tpg_enabled = true;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->tpg_reg->ctrl_addr);
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base + 0x600);
	CAM_DBG(CAM_ISP, "reg 0x%x = 0x%x", 0x600, val);

	return 0;
}

static int cam_ife_csid_ver1_tpg_stop(struct cam_ife_csid_ver1_hw   *csid_hw)
{
	int rc = 0;
	struct cam_hw_soc_info                 *soc_info;
	const struct cam_ife_csid_ver1_reg_info *csid_reg = NULL;

	if (!csid_hw->flags.tpg_enabled)
		return 0;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	/* disable the TPG */
	CAM_DBG(CAM_ISP, "CSID:%d stop CSID TPG",
		csid_hw->hw_intf->hw_idx);

	/* Disable the IFE force clock on for dual isp case */
	if (csid_hw->tpg_cfg.usage_type)
		rc = cam_ife_csid_disable_ife_force_clock_on(soc_info,
			csid_reg->tpg_reg->cpas_ife_reg_offset);

	/*stop the TPG */
	cam_io_w_mb(0,  soc_info->reg_map[0].mem_base +
		csid_reg->tpg_reg->ctrl_addr);
	csid_hw->flags.tpg_enabled = false;
	csid_hw->flags.tpg_configured = false;

	return 0;
}

static int cam_ife_csid_ver1_init_tpg_hw(
	struct cam_ife_csid_ver1_hw *csid_hw)
{
	const struct cam_ife_csid_ver1_reg_info *csid_reg;
	struct cam_ife_csid_ver1_tpg_reg_info       *tpg_reg;
	struct cam_hw_soc_info               *soc_info;
	uint32_t val = 0;

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;
	tpg_reg =  csid_reg->tpg_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	CAM_DBG(CAM_ISP, "CSID:%d TPG config",
		csid_hw->hw_intf->hw_idx);

	/* configure one DT, infinite frames */
	val = (tpg_reg->num_frames << tpg_reg->num_frame_shift) |
		    csid_hw->tpg_cfg.vc;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		tpg_reg->vc_cfg0_addr);

	/* vertical blanking count = 0x3FF, horzontal blanking count = 0x740*/
	val = (tpg_reg->vbi << tpg_reg->vbi_shift) |
		    (tpg_reg->hbi << tpg_reg->hbi_shift);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		tpg_reg->vc_cfg1_addr);

	cam_io_w_mb(tpg_reg->lfsr_seed, soc_info->reg_map[0].mem_base +
		tpg_reg->lfsr_seed_addr);

	val = csid_hw->tpg_cfg.width << tpg_reg->width_shift |
		csid_hw->tpg_cfg.height;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		tpg_reg->dt_n_cfg_0_addr);

	cam_io_w_mb(csid_hw->tpg_cfg.dt, soc_info->reg_map[0].mem_base +
		csid_reg->tpg_reg->dt_n_cfg_1_addr);

	/*
	 * in_format is the same as the input resource format.
	 * it is one larger than the register spec format.
	 */
	val = ((csid_hw->tpg_cfg.encode_format) << tpg_reg->fmt_shift) |
		    tpg_reg->payload_mode;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->tpg_reg->dt_n_cfg_2_addr);

	/* static frame with split color bar */
	val =  tpg_reg->color_bar << tpg_reg->color_bar_shift;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		tpg_reg->color_bars_cfg_addr);
	/* config pix pattern */
	cam_io_w_mb(csid_hw->tpg_cfg.test_pattern,
		soc_info->reg_map[0].mem_base +
		tpg_reg->common_gen_cfg_addr);

	return 0;
}

static int cam_ife_csid_hw_ver1_rx_cfg(
	struct cam_ife_csid_ver1_hw *csid_hw,
	struct cam_csid_hw_reserve_resource_args  *reserve)
{

	if (csid_hw->counters.csi2_reserve_cnt) {
		csid_hw->counters.csi2_reserve_cnt++;
		CAM_DBG(CAM_ISP, "CSID %u Rx already reserved cnt %u",
			csid_hw->hw_intf->hw_idx,
			csid_hw->counters.csi2_reserve_cnt);
		return 0;
	}

	csid_hw->rx_cfg.lane_cfg =
		reserve->in_port->lane_cfg;
	csid_hw->rx_cfg.lane_type =
		reserve->in_port->lane_type;
	csid_hw->rx_cfg.lane_num =
		reserve->in_port->lane_num;
	csid_hw->res_type = reserve->in_port->res_type;
	csid_hw->rx_cfg.epd_supported =
		reserve->in_port->epd_supported;

	switch (reserve->in_port->res_type) {
	case CAM_ISP_IFE_IN_RES_TPG:
		csid_hw->rx_cfg.phy_sel = 0;
		break;
	case CAM_ISP_IFE_IN_RES_CPHY_TPG_0:
		csid_hw->rx_cfg.phy_sel = 0;
		break;
	case CAM_ISP_IFE_IN_RES_CPHY_TPG_1:
		csid_hw->rx_cfg.phy_sel = 1;
		break;
	case CAM_ISP_IFE_IN_RES_CPHY_TPG_2:
		csid_hw->rx_cfg.phy_sel = 2;
		break;
	default:
		csid_hw->rx_cfg.phy_sel =
			(reserve->in_port->res_type & 0xFF) - 1;
		break;
	}

	csid_hw->counters.csi2_reserve_cnt++;
	CAM_DBG(CAM_ISP,
		"CSID:%u Lane cfg:0x%x type:%u num:%u res:0x%x, res_cnt %u",
		csid_hw->hw_intf->hw_idx,
		reserve->in_port->lane_cfg, reserve->in_port->lane_type,
		reserve->in_port->lane_num, reserve->in_port->res_type,
		csid_hw->counters.csi2_reserve_cnt);

	return 0;

}

int cam_ife_csid_hw_ver1_hw_cfg(
	struct cam_ife_csid_ver1_hw *csid_hw,
	struct cam_ife_csid_ver1_path_cfg *path_cfg,
	struct cam_csid_hw_reserve_resource_args  *reserve,
	uint32_t cid)
{
	int rc = 0;

	cam_ife_csid_hw_ver1_rx_cfg(csid_hw, reserve);
	cam_ife_csid_hw_ver1_path_cfg(csid_hw, path_cfg,
		reserve, cid);

	if (reserve->res_type == CAM_ISP_IFE_IN_RES_TPG) {
		rc = cam_ife_csid_ver1_tpg_config(csid_hw, reserve);

		if (rc)
			CAM_ERR(CAM_ISP,
				"CSID[%d] Res_id %d tpg config fail",
				csid_hw->hw_intf->hw_idx, reserve->res_id);
	}

	return rc;
}

static int cam_ife_csid_check_cust_node(
	struct cam_csid_hw_reserve_resource_args  *reserve,
	struct cam_ife_csid_ver1_hw *csid_hw)
{
	struct cam_ife_csid_ver1_reg_info *csid_reg;

	if (!reserve->in_port->cust_node)
		return 0;

	if (reserve->in_port->usage_type == CAM_ISP_RES_USAGE_DUAL) {
		CAM_ERR(CAM_ISP,
			"Dual IFE is not supported for cust_node %u",
			reserve->in_port->cust_node);
		return -EINVAL;
	}

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	if (!(csid_reg->csid_cust_node_map[csid_hw->hw_intf->hw_idx] &
		BIT(reserve->in_port->cust_node))) {
		CAM_ERR(CAM_ISP,
			"Invalid CSID:%u cust_node %u",
			csid_hw->hw_intf->hw_idx,
			reserve->in_port->cust_node);
		return -EINVAL;
	}

	return 0;
}

static bool cam_ife_csid_ver1_is_width_valid_by_fuse(
	struct cam_ife_csid_ver1_hw *csid_hw,
	uint32_t width)
{
	struct cam_ife_csid_ver1_reg_info *csid_reg;
	uint32_t fuse_val                 = UINT_MAX;

	cam_cpas_is_feature_supported(CAM_CPAS_MP_LIMIT_FUSE,
		CAM_CPAS_HW_IDX_ANY, &fuse_val);

	if (fuse_val == UINT_MAX) {
		CAM_DBG(CAM_ISP, "CSID[%u] Fuse not present",
			csid_hw->hw_intf->hw_idx);
		return true;
	}

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	if (fuse_val > csid_reg->width_fuse_max_val) {
		CAM_ERR(CAM_ISP, "Invalid fuse value %u", fuse_val);
		return false;
	}

	if (width > csid_reg->fused_max_width[fuse_val]) {
		CAM_ERR(CAM_ISP,
			"CSID[%u] Resolution not supported required_width: %d max_supported_width: %d",
			csid_hw->hw_intf->hw_idx,
			width, csid_reg->fused_max_width[fuse_val]);
		return false;
	}

	return true;
}

static bool cam_ife_csid_ver1_is_width_valid_by_dt(
	struct cam_ife_csid_ver1_hw *csid_hw,
	uint32_t  width)
{
	struct cam_csid_soc_private *soc_private = NULL;

	soc_private = (struct cam_csid_soc_private *)
			csid_hw->hw_info->soc_info.soc_private;

	if (!soc_private->max_width_enabled)
		return true;

	if (width > soc_private->max_width) {
		CAM_ERR(CAM_ISP,
			"CSID[%u] Resolution not supported required_width: %d max_supported_width: %d",
			csid_hw->hw_intf->hw_idx,
			width, soc_private->max_width);
		return false;
	}

	return true;
}

bool cam_ife_csid_ver1_is_width_valid(
	struct cam_csid_hw_reserve_resource_args  *reserve,
	struct cam_ife_csid_ver1_hw *csid_hw)
{
	uint32_t width = 0;

	if (reserve->res_id != CAM_IFE_PIX_PATH_RES_IPP)
		return true;

	if (reserve->sync_mode == CAM_ISP_HW_SYNC_MASTER ||
		reserve->sync_mode == CAM_ISP_HW_SYNC_NONE)
		width = reserve->in_port->left_stop -
			reserve->in_port->left_start + 1;
	else if (reserve->sync_mode == CAM_ISP_HW_SYNC_SLAVE)
		width = reserve->in_port->right_stop -
			reserve->in_port->right_start + 1;

	if (!cam_ife_csid_ver1_is_width_valid_by_fuse(csid_hw, width)) {
		CAM_ERR(CAM_ISP, "CSID[%u] width limited by fuse",
			csid_hw->hw_intf->hw_idx);
		return false;
	}

	if (!cam_ife_csid_ver1_is_width_valid_by_dt(csid_hw, width)) {
		CAM_ERR(CAM_ISP, "CSID[%u] width limited by dt",
			csid_hw->hw_intf->hw_idx);
		return false;
	}

	return true;
}

static int cam_ife_csid_ver1_in_port_validate(
	struct cam_csid_hw_reserve_resource_args  *reserve,
	struct cam_ife_csid_ver1_hw     *csid_hw)
{
	int rc = 0;
	struct cam_ife_csid_ver1_reg_info *csid_reg;

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	/* check in port args */
	rc  = cam_ife_csid_check_in_port_args(reserve,
		csid_hw->hw_intf->hw_idx);
	if (rc)
		goto err;

	if (!cam_ife_csid_ver1_is_width_valid(reserve, csid_hw))
		goto err;

	if (reserve->in_port->cust_node) {
		rc = cam_ife_csid_check_cust_node(reserve, csid_hw);

		if (rc) {
			CAM_ERR(CAM_ISP, "Custom node config error");
			goto err;
		}
	}

	if (csid_hw->counters.csi2_reserve_cnt) {

		if (csid_hw->res_type != reserve->in_port->res_type) {
			CAM_ERR(CAM_ISP,
				"CSID[%d ]Invalid res[%x] in_res_type[%x]",
				csid_hw->hw_intf->hw_idx,
				csid_hw->res_type,
				reserve->in_port->res_type);
			rc = -EINVAL;
			goto err;
		}

		if (csid_hw->rx_cfg.lane_cfg !=
			reserve->in_port->lane_cfg  ||
			csid_hw->rx_cfg.lane_type !=
			reserve->in_port->lane_type ||
			csid_hw->rx_cfg.lane_num !=
			reserve->in_port->lane_num) {
			CAM_ERR(CAM_ISP,
				"[%d] lane: num[%d %d] type[%d %d] cfg[%d %d]",
				csid_hw->hw_intf->hw_idx,
				csid_hw->rx_cfg.lane_num,
				reserve->in_port->lane_num,
				csid_hw->rx_cfg.lane_type,
				reserve->in_port->lane_type,
				csid_hw->rx_cfg.lane_cfg,
				reserve->in_port->lane_cfg);
			rc = -EINVAL;
			goto err;
		}
	}

	return rc;
err:
	CAM_ERR(CAM_ISP, "Invalid args csid[%d] rc %d",
		csid_hw->hw_intf->hw_idx, rc);
	return rc;
}


int cam_ife_csid_ver1_reserve(void *hw_priv,
	void *reserve_args, uint32_t arg_size)
{

	struct cam_ife_csid_ver1_hw     *csid_hw;
	struct cam_hw_info              *hw_info;
	struct cam_isp_resource_node    *res = NULL;
	struct cam_csid_hw_reserve_resource_args  *reserve;
	struct cam_ife_csid_ver1_path_cfg    *path_cfg;
	uint32_t cid;
	int rc = 0;

	reserve = (struct cam_csid_hw_reserve_resource_args  *)reserve_args;

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver1_hw *)hw_info->core_info;

	res = &csid_hw->path_res[reserve->res_id];

	if (res->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_DBG(CAM_ISP, "CSID %d Res_id %d state %d",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			res->res_state);
		return -EINVAL;
	}

	rc = cam_ife_csid_ver1_in_port_validate(reserve, csid_hw);

	CAM_DBG(CAM_ISP, "CSID[%d] res_id %d",
		csid_hw->hw_intf->hw_idx, reserve->res_id);

	if (rc) {
		CAM_DBG(CAM_ISP, "CSID %d Res_id %d port validation failed",
			csid_hw->hw_intf->hw_idx, reserve->res_id);
		return rc;
	}

	path_cfg = (struct cam_ife_csid_ver1_path_cfg *)res->res_priv;

	if (!path_cfg) {
		CAM_ERR(CAM_ISP,
			"CSID %d Unallocated Res_id %d state %d",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			res->res_state);
		return -EINVAL;
	}

	rc = cam_ife_csid_cid_reserve(csid_hw->cid_data, &cid,
		csid_hw->hw_intf->hw_idx, reserve);

	if (rc) {
		CAM_ERR(CAM_ISP, "CSID %d Res_id %d state %d invalid cid %d",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			res->res_state, cid);
		return rc;
	}

	rc = cam_ife_csid_hw_ver1_hw_cfg(csid_hw, path_cfg,
		reserve, cid);
	res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	reserve->node_res = res;
	csid_hw->event_cb = reserve->event_cb;
	csid_hw->token = reserve->cb_priv;

	CAM_DBG(CAM_ISP, "CSID %d Resource[id:%d name:%s] state %d cid %d",
		csid_hw->hw_intf->hw_idx, reserve->res_id,
		res->res_name, res->res_state, cid);

	return 0;
}

int cam_ife_csid_ver1_release(void *hw_priv,
	void *release_args, uint32_t arg_size)
{
	struct cam_ife_csid_ver1_hw     *csid_hw;
	struct cam_hw_info              *hw_info;
	struct cam_isp_resource_node    *res = NULL;
	struct cam_ife_csid_ver1_path_cfg    *path_cfg;
	int rc = 0;

	if (!hw_priv || !release_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver1_hw *)hw_info->core_info;
	res = (struct cam_isp_resource_node *)release_args;

	if (res->res_type != CAM_ISP_RESOURCE_PIX_PATH) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type:%d res id%d",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	mutex_lock(&csid_hw->hw_info->hw_mutex);

	if ((res->res_type == CAM_ISP_RESOURCE_PIX_PATH &&
		res->res_id >= CAM_IFE_PIX_PATH_RES_MAX)) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type:%d res id%d",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id);
		rc = -EINVAL;
		goto end;
	}

	if ((res->res_state <= CAM_ISP_RESOURCE_STATE_AVAILABLE) ||
		(res->res_state >= CAM_ISP_RESOURCE_STATE_STREAMING)) {
		CAM_WARN(CAM_ISP,
			"CSID:%d res type:%d Res %d in state %d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id,
			res->res_state);
		goto end;
	}

	CAM_DBG(CAM_ISP, "CSID:%d res type :%d Resource id:%d",
		csid_hw->hw_intf->hw_idx, res->res_type, res->res_id);

	path_cfg = (struct cam_ife_csid_ver1_path_cfg *)res->res_priv;
	cam_ife_csid_cid_release(&csid_hw->cid_data[path_cfg->cid],
		csid_hw->hw_intf->hw_idx,
		path_cfg->cid);
	memset(path_cfg, 0, sizeof(*path_cfg));

	if (csid_hw->counters.csi2_reserve_cnt)
		csid_hw->counters.csi2_reserve_cnt--;
	if (!csid_hw->counters.csi2_reserve_cnt) {
		memset(&csid_hw->rx_cfg, 0,
			sizeof(struct cam_ife_csid_rx_cfg));
		memset(&csid_hw->debug_info, 0,
			sizeof(struct cam_ife_csid_debug_info));
		csid_hw->event_cb = NULL;
		csid_hw->token = NULL;
	}

	CAM_DBG(CAM_ISP,
		"CSID:%d res type :%d Resource[id:%d name:%s] csi2_reserve_cnt:%d",
		csid_hw->hw_intf->hw_idx, res->res_type, res->res_id,
		res->res_name, csid_hw->counters.csi2_reserve_cnt);

	res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
end:
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_ife_csid_ver1_start_rdi_path(
	struct cam_ife_csid_ver1_hw *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	struct cam_ife_csid_ver1_reg_info *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_ife_csid_ver1_path_reg_info *path_reg;
	struct cam_ife_csid_core_info  *core_info;
	void __iomem *mem_base;
	uint32_t val;

	if (res->res_state != CAM_ISP_RESOURCE_STATE_INIT_HW ||
		res->res_id > CAM_IFE_PIX_PATH_RES_RDI_4) {
		CAM_ERR(CAM_ISP,
			"CSID:%d %s path res type:%d res_id:%d Invalid state%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	core_info = csid_hw->core_info;
	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)core_info->csid_reg;

	path_reg = csid_reg->rdi_reg[res->res_id];

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d RDI:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	mem_base = soc_info->reg_map[0].mem_base;
	 /* Resume at frame boundary */
	cam_io_w_mb(CAM_CSID_RESUME_AT_FRAME_BOUNDARY,
		mem_base + path_reg->ctrl_addr);

	CAM_DBG(CAM_ISP, "CSID:%d start: %s",
		csid_hw->hw_intf->hw_idx, res->res_name);

	val = path_reg->fatal_err_mask | path_reg->non_fatal_err_mask |
		csid_hw->debug_info.path_mask |
		IFE_CSID_VER1_PATH_INFO_RST_DONE;
	cam_io_w_mb(val, mem_base + path_reg->irq_mask_addr);

	res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
	return rc;
}

static int cam_ife_csid_ver1_start_udi_path(
	struct cam_ife_csid_ver1_hw *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	struct cam_ife_csid_ver1_reg_info *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_ife_csid_ver1_path_reg_info *path_reg;
	struct cam_ife_csid_core_info  *core_info;
	void __iomem *mem_base;
	uint32_t val, id;

	if ((res->res_state != CAM_ISP_RESOURCE_STATE_INIT_HW) ||
		(res->res_id < CAM_IFE_PIX_PATH_RES_UDI_0 ||
		 res->res_id > CAM_IFE_PIX_PATH_RES_UDI_2)) {
		CAM_ERR(CAM_ISP,
			"CSID:%d %s path res type:%d res_id:%d Invalid state%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	core_info = csid_hw->core_info;
	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)core_info->csid_reg;

	id = res->res_id - CAM_IFE_PIX_PATH_RES_UDI_2;

	path_reg = csid_reg->udi_reg[id];

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d UDI:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	mem_base = soc_info->reg_map[0].mem_base;
	 /* Resume at frame boundary */
	cam_io_w_mb(CAM_CSID_RESUME_AT_FRAME_BOUNDARY,
		mem_base + path_reg->ctrl_addr);

	CAM_DBG(CAM_ISP, "CSID:%d Start:%s",
		csid_hw->hw_intf->hw_idx, res->res_name);

	val = path_reg->fatal_err_mask | path_reg->non_fatal_err_mask |
		csid_hw->debug_info.path_mask |
		IFE_CSID_VER1_PATH_INFO_RST_DONE;
	cam_io_w_mb(val, mem_base + path_reg->irq_mask_addr);

	res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
	return rc;
}

static int cam_ife_csid_ver1_start_pix_path(
	struct cam_ife_csid_ver1_hw *csid_hw,
	struct cam_isp_resource_node    *res)
{

	int rc = 0;
	const struct cam_ife_csid_ver1_reg_info *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_ife_csid_ver1_path_reg_info *path_reg = NULL;
	uint32_t  val = 0;
	struct cam_ife_csid_ver1_path_cfg *path_cfg;
	struct cam_ife_csid_ver1_path_cfg *ipp_path_cfg;

	if (res->res_state != CAM_ISP_RESOURCE_STATE_INIT_HW) {
		CAM_ERR(CAM_ISP,
			"CSID:%d %s path res type:%d res_id:%d Invalid state%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	if (res->res_id == CAM_IFE_PIX_PATH_RES_IPP)
		path_reg = csid_reg->ipp_reg;
	else if (res->res_id == CAM_IFE_PIX_PATH_RES_PPP)
		path_reg = csid_reg->ppp_reg;

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d PIX:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	path_cfg = (struct cam_ife_csid_ver1_path_cfg *)res->res_priv;

	if (res->res_id == CAM_IFE_PIX_PATH_RES_IPP) {

		path_reg = csid_reg->ipp_reg;
		val = path_reg->halt_master_sel_master_val <<
			path_reg->halt_master_sel_shift;

		if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_MASTER) {
			/* Set start mode as master */
			val |= path_reg->halt_mode_master  <<
				path_reg->halt_mode_shift;
		} else if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_SLAVE) {
			/* Set start mode as slave */
			val |= path_reg->halt_mode_slave <<
				path_reg->halt_mode_shift;
		} else {
			/* Default is internal halt mode */
			val = 0;
		}
	} else if (res->res_id == CAM_IFE_PIX_PATH_RES_PPP) {

		path_reg = csid_reg->ppp_reg;

		/* for dual case
		 * set ppp as slave
		 * if current csid is set as master set
		 * start_master_sel_val as 3
		 */

		if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_NONE) {
			val = 0;
		} else {
			val = path_reg->halt_mode_slave <<
				path_reg->halt_mode_shift;
			/* Set halt mode as master */

			ipp_path_cfg = (struct cam_ife_csid_ver1_path_cfg *)
					csid_hw->path_res
					[CAM_IFE_PIX_PATH_RES_IPP].res_priv;

			if (ipp_path_cfg->sync_mode == CAM_ISP_HW_SYNC_MASTER)
				val |= path_reg->halt_master_sel_master_val <<
					path_reg->halt_master_sel_shift;
		}
	}

	/*
	 * Resume at frame boundary if Master or No Sync.
	 * Slave will get resume command from Master.
	 */
	if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_MASTER ||
		path_cfg->sync_mode == CAM_ISP_HW_SYNC_NONE)
		val |= path_reg->resume_frame_boundary;

	cam_io_w_mb(val,
		soc_info->reg_map[0].mem_base + path_reg->ctrl_addr);

	CAM_DBG(CAM_ISP, "CSID:%d Resource[id:%d name:%s]ctrl val: 0x%x",
		csid_hw->hw_intf->hw_idx,
		res->res_id, res->res_name, val);

	val = path_reg->fatal_err_mask | path_reg->non_fatal_err_mask |
		csid_hw->debug_info.path_mask |
		IFE_CSID_VER1_PATH_INFO_RST_DONE;
	cam_io_w_mb(val,
		soc_info->reg_map[0].mem_base + path_reg->irq_mask_addr);

	res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
	return rc;
}

static int cam_ife_csid_ver1_enable_csi2(struct cam_ife_csid_ver1_hw *csid_hw)
{
	int rc = 0;
	struct cam_hw_soc_info              *soc_info;
	const struct cam_ife_csid_ver1_reg_info *csid_reg;
	const struct cam_ife_csid_csi2_rx_reg_info  *csi2_reg;
	uint32_t val = 0;
	struct cam_ife_csid_rx_cfg        *rx_cfg;
	int vc_full_width;

	if (csid_hw->flags.rx_enabled)
		return 0;

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;
	csi2_reg  = csid_reg->csi2_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	rx_cfg  = &csid_hw->rx_cfg;

	/*Configure Rx cfg0 */

	val = (rx_cfg->lane_cfg << csi2_reg->lane_cfg_shift) |
		((rx_cfg->lane_num - 1) << csi2_reg->lane_num_shift) |
		(rx_cfg->lane_type << csi2_reg->phy_type_shift) |
		(rx_cfg->phy_sel  << csi2_reg->phy_num_shift);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csi2_reg->cfg0_addr);

	/*Configure Rx cfg1*/
	val = 1 << csi2_reg->misr_enable_shift_val;
	val |= 1 << csi2_reg->ecc_correction_shift_en;

	vc_full_width = cam_ife_csid_is_vc_full_width(csid_hw->cid_data);

	if (vc_full_width == 1) {
		val |= 1 <<  csi2_reg->vc_mode_shift_val;
	} else if (vc_full_width < 0) {
		CAM_ERR(CAM_ISP, "Error VC DT");
		return -EINVAL;
	}

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csi2_reg->cfg1_addr);
	rc = cam_ife_csid_ver1_hw_reset(csid_hw);

	if (rc) {
		CAM_ERR(CAM_ISP, "CSID[%d] hw reset fail",
			csid_hw->hw_intf->hw_idx);
		return rc;
	}

	csid_hw->flags.rx_enabled = true;

	if (csid_hw->res_type == CAM_ISP_IFE_IN_RES_TPG)
		cam_ife_csid_ver1_init_tpg_hw(csid_hw);

	/* Enable the interrupt based on csid debug info set
	 * Fatal error mask
	 * Partly fatal error mask
	 * Rx reset done irq
	 */
	val = csi2_reg->fatal_err_mask | csi2_reg->part_fatal_err_mask |
		csid_hw->debug_info.rx_mask | IFE_CSID_VER1_RX_RST_DONE;

	/*EPD supported sensors do not send EOT, error will be generated
	 * if this irq is enabled
	 */
	if (csid_hw->rx_cfg.epd_supported)
		val &= ~IFE_CSID_VER1_RX_CPHY_EOT_RECEPTION;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csi2_reg->irq_mask_addr);
	cam_ife_csid_ver1_rx_capture_config(csid_hw);

	return rc;
}

static int cam_ife_csid_ver1_init_config_rdi_path(
	struct cam_ife_csid_ver1_hw *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	const struct cam_ife_csid_ver1_reg_info *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_ife_csid_ver1_path_reg_info *path_reg = NULL;
	const struct cam_ife_csid_ver1_common_reg_info *cmn_reg = NULL;
	uint32_t  val;
	struct cam_ife_csid_ver1_path_cfg *path_cfg;
	struct cam_ife_csid_cid_data *cid_data;
	bool is_rpp = false;
	void __iomem *mem_base;
	struct cam_ife_csid_path_format path_format = {0};

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	if (!csid_reg->rdi_reg[res->res_id]) {
		CAM_ERR(CAM_ISP, "CSID:%d RDI:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	cmn_reg = csid_reg->cmn_reg;
	path_reg = csid_reg->rdi_reg[res->res_id];
	path_cfg = (struct cam_ife_csid_ver1_path_cfg *)res->res_priv;
	cid_data = &csid_hw->cid_data[path_cfg->cid];
	mem_base = soc_info->reg_map[0].mem_base;
	is_rpp = path_cfg->crop_enable || path_cfg->drop_enable;
	rc = cam_ife_csid_get_format_rdi(path_cfg->in_format,
		path_cfg->out_format, &path_format, is_rpp);
	if (rc)
		return rc;

	/*Configure cfg0:
	 * Timestamp enable
	 * VC
	 * DT
	 * DT_ID cobination
	 * Decode Format
	 * Crop/Drop parameters
	 * Plain format
	 * Packing format
	 */
	val = (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc <<
			cmn_reg->vc_shift_val) |
		(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt <<
			cmn_reg->dt_shift_val) |
		(path_cfg->cid << cmn_reg->dt_id_shift_val) |
		(path_format.decode_fmt <<
			cmn_reg->decode_format_shift_val);

	if (csid_hw->debug_info.debug_val &
		CAM_IFE_CSID_DEBUG_ENABLE_HBI_VBI_INFO)
		val |= 1 << path_reg->format_measure_en_shift_val;

	val |= (path_cfg->crop_enable << path_reg->crop_h_en_shift_val) |
		(path_cfg->crop_enable <<
		 path_reg->crop_v_en_shift_val);

	if (cmn_reg->drop_supported)
		val |= (path_cfg->drop_enable <<
				path_reg->drop_v_en_shift_val) |
			(path_cfg->drop_enable <<
				path_reg->drop_h_en_shift_val);

	if (path_reg->mipi_pack_supported)
		val |= path_format.packing_fmt <<
			path_reg->packing_fmt_shift_val;

	val |= path_format.plain_fmt << path_reg->plain_fmt_shift_val;
	val |= 1 << path_reg->timestamp_en_shift_val;

	cam_io_w_mb(val, mem_base + path_reg->cfg0_addr);

	/*Configure Multi VC DT combo */
	if (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].valid) {
		val = (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].vc <<
				cmn_reg->multi_vcdt_vc1_shift_val) |
			(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].dt <<
				cmn_reg->multi_vcdt_dt1_shift_val) |
			(1 << cmn_reg->multi_vcdt_en_shift_val);

		cam_io_w_mb(val, mem_base + path_reg->multi_vcdt_cfg0_addr);
	}

	val = 0;
	/*configure cfg1 addr
	 * Timestamp strobe selection
	 */
	val |= cmn_reg->timestamp_strobe_val <<
			cmn_reg->timestamp_stb_sel_shift_val;

	cam_io_w_mb(val, mem_base + path_reg->cfg1_addr);

	if (path_cfg->crop_enable) {
		val = (((path_cfg->end_pixel & cmn_reg->crop_pix_start_mask) <<
			cmn_reg->crop_shift_val) |
			(path_cfg->start_pixel & cmn_reg->crop_pix_end_mask));
		cam_io_w_mb(val, mem_base + path_reg->hcrop_addr);
		CAM_DBG(CAM_ISP, "CSID:%d Horizontal crop config val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);

		val = (((path_cfg->end_line & cmn_reg->crop_line_start_mask) <<
			csid_reg->cmn_reg->crop_shift_val) |
			(path_cfg->start_line & cmn_reg->crop_line_end_mask));
		cam_io_w_mb(val, mem_base + path_reg->vcrop_addr);
		CAM_DBG(CAM_ISP, "CSID:%d Vertical Crop config val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);
	}

	/* set frame drop pattern to 0 and period to 1 */
	cam_io_w_mb(1, mem_base + path_reg->frm_drop_period_addr);
	cam_io_w_mb(0, mem_base + path_reg->frm_drop_pattern_addr);
	/* set irq sub sample pattern to 0 and period to 1 */
	cam_io_w_mb(1, mem_base + path_reg->irq_subsample_period_addr);
	cam_io_w_mb(0, mem_base + path_reg->irq_subsample_pattern_addr);

	/*TODO Need to check for any hw errata like 480 and 580*/
	/* set pxl drop pattern to 0 and period to 1 */
	cam_io_w_mb(0, mem_base + path_reg->pix_drop_pattern_addr);
	cam_io_w_mb(1, mem_base + path_reg->pix_drop_period_addr);

	/* set line drop pattern to 0 and period to 1 */
	cam_io_w_mb(0, mem_base + path_reg->line_drop_pattern_addr);
	cam_io_w_mb(1, mem_base + path_reg->line_drop_period_addr);

	if (path_reg->overflow_ctrl_en) {
		val = path_reg->overflow_ctrl_en |
			path_reg->overflow_ctrl_mode_val;
		cam_io_w_mb(val, mem_base + path_reg->err_recovery_cfg0_addr);
	}

	if (csid_hw->debug_info.debug_val &
		CAM_IFE_CSID_DEBUG_ENABLE_HBI_VBI_INFO) {
		val = cam_io_r_mb(mem_base +
			path_reg->format_measure_cfg0_addr);
		val |= csid_reg->cmn_reg->measure_en_hbi_vbi_cnt_mask;
		cam_io_w_mb(val, mem_base +
			path_reg->format_measure_cfg0_addr);
	}

	/* Enable the RDI path */
	val = cam_io_r_mb(mem_base + path_reg->cfg0_addr);
	val |= (1 << cmn_reg->path_en_shift_val);
	cam_io_w_mb(val, mem_base + path_reg->cfg0_addr);
	CAM_DBG(CAM_ISP, "%s cfg0 0x%x", res->res_name, val);
	res->res_state = CAM_ISP_RESOURCE_STATE_INIT_HW;

	return rc;
}

static int cam_ife_csid_ver1_init_config_udi_path(
	struct cam_ife_csid_ver1_hw *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	const struct cam_ife_csid_ver1_reg_info *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_ife_csid_ver1_path_reg_info *path_reg = NULL;
	const struct cam_ife_csid_ver1_common_reg_info *cmn_reg = NULL;
	uint32_t  val;
	struct cam_ife_csid_ver1_path_cfg *path_cfg;
	struct cam_ife_csid_cid_data *cid_data;
	bool is_rpp = false;
	void __iomem *mem_base;
	struct cam_ife_csid_path_format path_format = {0};
	uint32_t id;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	id = res->res_id - CAM_IFE_PIX_PATH_RES_UDI_0;

	if (!csid_reg->udi_reg[id]) {
		CAM_ERR(CAM_ISP, "CSID:%d UDI:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	cmn_reg = csid_reg->cmn_reg;
	path_reg = csid_reg->udi_reg[id];
	path_cfg = (struct cam_ife_csid_ver1_path_cfg *)res->res_priv;
	cid_data = &csid_hw->cid_data[path_cfg->cid];
	mem_base = soc_info->reg_map[0].mem_base;
	is_rpp = path_cfg->crop_enable || path_cfg->drop_enable;
	rc = cam_ife_csid_get_format_rdi(path_cfg->in_format,
		path_cfg->out_format, &path_format, is_rpp);
	if (rc)
		return rc;

	/*Configure cfg0:
	 * Timestamp enable
	 * VC
	 * DT
	 * DT_ID cobination
	 * Decode Format
	 * Crop/Drop parameters
	 * Plain format
	 * Packing format
	 */
	val = (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc <<
			cmn_reg->vc_shift_val) |
		(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt <<
			cmn_reg->dt_shift_val) |
		(path_cfg->cid << cmn_reg->dt_id_shift_val) |
		(path_format.decode_fmt << cmn_reg->decode_format_shift_val);

	if (csid_hw->debug_info.debug_val &
		CAM_IFE_CSID_DEBUG_ENABLE_HBI_VBI_INFO)
		val |= 1 << path_reg->format_measure_en_shift_val;

	val |= (path_cfg->crop_enable << path_reg->crop_h_en_shift_val) |
		(path_cfg->crop_enable <<
		 path_reg->crop_v_en_shift_val);

	if (cmn_reg->drop_supported)
		val |= (path_cfg->drop_enable <<
				path_reg->drop_v_en_shift_val) |
			(path_cfg->drop_enable <<
				path_reg->drop_h_en_shift_val);

	if (path_reg->mipi_pack_supported)
		val |= path_format.packing_fmt <<
			path_reg->packing_fmt_shift_val;

	val |= path_format.plain_fmt << path_reg->plain_fmt_shift_val;

	cam_io_w_mb(val, mem_base + path_reg->cfg0_addr);

	val = 0;
	/*configure cfg1 addr
	 * Timestamp strobe selection
	 */
	val |= (1 << path_reg->timestamp_en_shift_val) |
		(cmn_reg->timestamp_strobe_val <<
			cmn_reg->timestamp_stb_sel_shift_val);

	cam_io_w_mb(val, mem_base + path_reg->cfg1_addr);

	if (path_cfg->crop_enable) {
		val = (((path_cfg->end_pixel & cmn_reg->crop_pix_start_mask) <<
			cmn_reg->crop_shift_val) |
			(path_cfg->start_pixel & cmn_reg->crop_pix_end_mask));
		cam_io_w_mb(val, mem_base + path_reg->hcrop_addr);
		CAM_DBG(CAM_ISP, "CSID:%d Horizontal crop config val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);

		val = (((path_cfg->end_line & cmn_reg->crop_line_start_mask) <<
			csid_reg->cmn_reg->crop_shift_val) |
			(path_cfg->start_line & cmn_reg->crop_line_end_mask));
		cam_io_w_mb(val, mem_base + path_reg->vcrop_addr);
		CAM_DBG(CAM_ISP, "CSID:%d Vertical Crop config val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);
	}

	/* set frame drop pattern to 0 and period to 1 */
	cam_io_w_mb(1, mem_base + path_reg->frm_drop_period_addr);
	cam_io_w_mb(0, mem_base + path_reg->frm_drop_pattern_addr);
	/* set irq sub sample pattern to 0 and period to 1 */
	cam_io_w_mb(1, mem_base + path_reg->irq_subsample_period_addr);
	cam_io_w_mb(0, mem_base + path_reg->irq_subsample_pattern_addr);

	/*TODO Need to check for any hw errata like 480 and 580*/
	/* set pxl drop pattern to 0 and period to 1 */
	cam_io_w_mb(0, mem_base + path_reg->pix_drop_pattern_addr);
	cam_io_w_mb(1, mem_base + path_reg->pix_drop_period_addr);

	/* set line drop pattern to 0 and period to 1 */
	cam_io_w_mb(0, mem_base + path_reg->line_drop_pattern_addr);
	cam_io_w_mb(1, mem_base + path_reg->line_drop_period_addr);

	/* Enable the UDI path */
	val = cam_io_r_mb(mem_base + path_reg->cfg0_addr);
	val |= (1 << cmn_reg->path_en_shift_val);
	cam_io_w_mb(val, mem_base + path_reg->cfg0_addr);
	CAM_DBG(CAM_ISP, "%s cfg0 0x%x", res->res_name, val);

	if (path_reg->overflow_ctrl_en) {
		val = path_reg->overflow_ctrl_en |
			path_reg->overflow_ctrl_mode_val;
		cam_io_w_mb(val, mem_base + path_reg->err_recovery_cfg0_addr);
	}

	if (csid_hw->debug_info.debug_val &
		CAM_IFE_CSID_DEBUG_ENABLE_HBI_VBI_INFO) {
		val = cam_io_r_mb(mem_base +
			path_reg->format_measure_cfg0_addr);
		val |= csid_reg->cmn_reg->measure_en_hbi_vbi_cnt_mask;
		cam_io_w_mb(val, mem_base +
			path_reg->format_measure_cfg0_addr);
	}

	res->res_state = CAM_ISP_RESOURCE_STATE_INIT_HW;
	return rc;
}

static int cam_ife_csid_ver1_init_config_pxl_path(
	struct cam_ife_csid_ver1_hw *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	const struct cam_ife_csid_ver1_reg_info *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_ife_csid_ver1_path_reg_info *path_reg = NULL;
	const struct cam_ife_csid_ver1_common_reg_info *cmn_reg = NULL;
	uint32_t val = 0;
	struct cam_ife_csid_ver1_path_cfg *path_cfg;
	struct cam_ife_csid_cid_data *cid_data;
	void __iomem *mem_base;
	struct cam_ife_csid_path_format path_format = {0};

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	if (res->res_id ==  CAM_IFE_PIX_PATH_RES_IPP)
		path_reg = csid_reg->ipp_reg;
	else if (res->res_id ==  CAM_IFE_PIX_PATH_RES_PPP)
		path_reg = csid_reg->ppp_reg;

	if (!path_reg)
		return -EINVAL;

	cmn_reg = csid_reg->cmn_reg;

	path_cfg = (struct cam_ife_csid_ver1_path_cfg *)res->res_priv;
	cid_data = &csid_hw->cid_data[path_cfg->cid];
	mem_base = soc_info->reg_map[0].mem_base;

	rc = cam_ife_csid_get_format_ipp_ppp(path_cfg->in_format,
		&path_format);

	/*Configure:
	 * VC
	 * DT
	 * DT_ID cobination
	 * Decode Format
	 * Early eof
	 * timestamp enable
	 * crop/drop enable
	 * Binning
	 */
	val = (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc <<
			cmn_reg->vc_shift_val) |
		(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt <<
			cmn_reg->dt_shift_val) |
		(path_cfg->cid << cmn_reg->dt_id_shift_val) |
		(path_format.decode_fmt <<
			cmn_reg->decode_format_shift_val);

	/*enable early eof based on crop enable */
	if (!(csid_hw->debug_info.debug_val &
		CAM_IFE_CSID_DEBUG_DISABLE_EARLY_EOF) &&
		cmn_reg->early_eof_supported &&
		path_cfg->crop_enable)
		val |= (1 << path_reg->early_eof_en_shift_val);

	if (cmn_reg->drop_supported)
		val |= (path_cfg->drop_enable <<
				path_reg->drop_v_en_shift_val) |
			(path_cfg->drop_enable <<
				path_reg->drop_h_en_shift_val);

	val |= (1 << path_reg->pix_store_en_shift_val) |
		(1 << path_reg->timestamp_en_shift_val);

	val |= (path_cfg->crop_enable << path_reg->crop_h_en_shift_val) |
		(path_cfg->crop_enable <<
		 path_reg->crop_v_en_shift_val);

	if ((path_reg->binning_supported & CAM_IFE_CSID_BIN_HORIZONTAL) &&
		path_cfg->horizontal_bin)
		val |= 1 << path_reg->bin_h_en_shift_val;

	if ((path_reg->binning_supported & CAM_IFE_CSID_BIN_VERTICAL) &&
		path_cfg->vertical_bin)
		val |= 1 << path_reg->bin_v_en_shift_val;

	if ((path_reg->binning_supported & CAM_IFE_CSID_BIN_QCFA) &&
		path_cfg->qcfa_bin)
		val |=  1 << path_reg->bin_qcfa_en_shift_val;

	if ((path_cfg->qcfa_bin || path_cfg->vertical_bin ||
		path_cfg->horizontal_bin) && path_reg->binning_supported)
		val |= 1 << path_reg->bin_en_shift_val;

	if (csid_hw->debug_info.debug_val &
		CAM_IFE_CSID_DEBUG_ENABLE_HBI_VBI_INFO)
		val |= 1 << path_reg->format_measure_en_shift_val;

	CAM_DBG(CAM_ISP, "CSID[%u] cfg0_addr val %x",
		csid_hw->hw_intf->hw_idx, val);

	cam_io_w_mb(val, mem_base + path_reg->cfg0_addr);

	/*Configure Multi VC DT combo */
	if (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].valid) {
		val = (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].vc <<
				cmn_reg->multi_vcdt_vc1_shift_val) |
			(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].dt <<
				 cmn_reg->multi_vcdt_dt1_shift_val) |
			(1 << cmn_reg->multi_vcdt_en_shift_val);
		cam_io_w_mb(val, mem_base + path_reg->multi_vcdt_cfg0_addr);
	}

	/*configure cfg1 addr
	 * timestamp strobe selection
	 */

	val = cmn_reg->timestamp_strobe_val <<
		cmn_reg->timestamp_stb_sel_shift_val;

	cam_io_w_mb(val, mem_base + path_reg->cfg1_addr);

	if (path_cfg->crop_enable) {
		val = (((path_cfg->end_pixel & cmn_reg->crop_pix_start_mask) <<
			cmn_reg->crop_shift_val) |
			(path_cfg->start_pixel & cmn_reg->crop_pix_end_mask));
		cam_io_w_mb(val, mem_base + path_reg->hcrop_addr);
		CAM_DBG(CAM_ISP, "CSID:%d Horizontal crop config val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);

		val = (((path_cfg->end_line & cmn_reg->crop_line_start_mask) <<
			csid_reg->cmn_reg->crop_shift_val) |
			(path_cfg->start_line & cmn_reg->crop_line_end_mask));
		cam_io_w_mb(val, mem_base + path_reg->vcrop_addr);
		CAM_DBG(CAM_ISP, "CSID:%d Vertical Crop config val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);
	}

	/* set frame drop pattern to 0 and period to 1 */
	cam_io_w_mb(1, mem_base + path_reg->frm_drop_period_addr);
	cam_io_w_mb(0, mem_base + path_reg->frm_drop_pattern_addr);
	/* set irq sub sample pattern to 0 and period to 1 */
	cam_io_w_mb(1, mem_base + path_reg->irq_subsample_period_addr);
	cam_io_w_mb(0, mem_base + path_reg->irq_subsample_pattern_addr);
	/* set pxl drop pattern to 0 and period to 1 */
	cam_io_w_mb(0, mem_base + path_reg->pix_drop_pattern_addr);
	cam_io_w_mb(1, mem_base + path_reg->pix_drop_period_addr);
	/* set line drop pattern to 0 and period to 1 */
	cam_io_w_mb(0, mem_base + path_reg->line_drop_pattern_addr);
	cam_io_w_mb(1, mem_base + path_reg->line_drop_period_addr);

	if (path_reg->overflow_ctrl_en) {
		val = path_reg->overflow_ctrl_en |
			path_reg->overflow_ctrl_mode_val;
		cam_io_w_mb(val, mem_base + path_reg->err_recovery_cfg0_addr);
	}

	if (csid_hw->debug_info.debug_val &
		CAM_IFE_CSID_DEBUG_ENABLE_HBI_VBI_INFO) {
		val = cam_io_r_mb(mem_base +
			path_reg->format_measure_cfg0_addr);
		val |= csid_reg->cmn_reg->measure_en_hbi_vbi_cnt_mask;
		cam_io_w_mb(val,
			mem_base + path_reg->format_measure_cfg0_addr);
	}

	/* Enable the Pxl path */
	val = cam_io_r_mb(mem_base + path_reg->cfg0_addr);
	val |= (1 << cmn_reg->path_en_shift_val);
	cam_io_w_mb(val, mem_base + path_reg->cfg0_addr);
	CAM_DBG(CAM_ISP, "%s cfg0 0x%x", res->res_name, val);

	res->res_state = CAM_ISP_RESOURCE_STATE_INIT_HW;

	return rc;
}

static int cam_ife_csid_ver1_enable_hw(struct cam_ife_csid_ver1_hw *csid_hw)
{
	int rc = 0;
	struct cam_hw_soc_info              *soc_info;
	const struct cam_ife_csid_ver1_reg_info *csid_reg;
	uint32_t clk_lvl, i, val;
	unsigned long flags;

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;
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

	rc = cam_soc_util_get_clk_level(soc_info, csid_hw->clk_rate,
		soc_info->src_clk_idx, &clk_lvl);

	CAM_DBG(CAM_ISP,
		"CSID[%d] clk lvl %u received clk_rate %u applied clk_rate %lu",
		csid_hw->hw_intf->hw_idx, clk_lvl, csid_hw->clk_rate,
		soc_info->applied_src_clk_rate);

	rc = cam_ife_csid_enable_soc_resources(soc_info, clk_lvl);

	if (rc) {
		CAM_ERR(CAM_ISP, "CSID:%d Enable SOC failed",
			csid_hw->hw_intf->hw_idx);
		goto err;
	}
	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_UP;

	rc = cam_ife_csid_ver1_global_reset(csid_hw);

	if (rc) {
		CAM_ERR(CAM_ISP, "CSID[%d] global reset failed");
		goto disable_soc;
	}


	/* Clear IRQs */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->top_irq_clear_addr);

	cam_io_w_mb(csid_reg->csi2_reg->irq_mask_all,
		soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->irq_clear_addr);

	if (csid_reg->cmn_reg->num_pix)
		cam_io_w_mb(csid_reg->cmn_reg->ipp_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->irq_clear_addr);

	if (csid_reg->cmn_reg->num_ppp)
		cam_io_w_mb(csid_reg->cmn_reg->ppp_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->irq_clear_addr);

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++)
		cam_io_w_mb(csid_reg->cmn_reg->rdi_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->irq_clear_addr);

	for (i = 0; i < csid_reg->cmn_reg->num_udis; i++)
		cam_io_w_mb(csid_reg->cmn_reg->udi_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->udi_reg[i]->irq_clear_addr);

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->irq_cmd_addr);

	/* Read hw version */
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->hw_version_addr);

	CAM_DBG(CAM_ISP, "CSID:%d Enabled CSID HW version: 0x%x",
		csid_hw->hw_intf->hw_idx, val);
	memset(&csid_hw->timestamp, 0, sizeof(struct cam_ife_csid_timestamp));
	spin_lock_irqsave(&csid_hw->lock_state, flags);
	csid_hw->flags.fatal_err_detected = false;
	csid_hw->flags.device_enabled = true;
	spin_unlock_irqrestore(&csid_hw->lock_state, flags);
	cam_tasklet_start(csid_hw->tasklet);

	return rc;

disable_soc:
	cam_ife_csid_disable_soc_resources(soc_info);
	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
err:
	csid_hw->hw_info->open_count--;
	return rc;
}

int cam_ife_csid_ver1_init_hw(void *hw_priv,
	void *init_args, uint32_t arg_size)
{
	struct cam_ife_csid_ver1_hw *csid_hw  = NULL;
	struct cam_isp_resource_node           *res;
	struct cam_hw_info *hw_info;
	int rc = 0;

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver1_hw *)hw_info->core_info;

	if (!hw_priv || !init_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	rc = cam_ife_csid_ver1_enable_hw(csid_hw);

	if (rc) {
		CAM_ERR(CAM_ISP, "CSID:%d Enable hw fail",
			csid_hw->hw_intf->hw_idx);
		return rc;
	}

	mutex_lock(&csid_hw->hw_info->hw_mutex);
	res = (struct cam_isp_resource_node *)init_args;
	if (res->res_type == CAM_ISP_RESOURCE_PIX_PATH &&
		res->res_id >= CAM_IFE_PIX_PATH_RES_MAX) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res tpe:%d res id%d",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id);
		rc = -EINVAL;
		goto end;
	}

	if ((res->res_type == CAM_ISP_RESOURCE_PIX_PATH) &&
		(res->res_state != CAM_ISP_RESOURCE_STATE_RESERVED)) {
		CAM_ERR(CAM_ISP,
			"CSID:%d res type:%d res_id:%dInvalid state %d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ISP, "CSID:%d res type :%d res_id:%d",
		csid_hw->hw_intf->hw_idx, res->res_type, res->res_id);
	res  = (struct cam_isp_resource_node *)init_args;

	switch (res->res_id) {
	case CAM_IFE_PIX_PATH_RES_IPP:
	case CAM_IFE_PIX_PATH_RES_PPP:
		rc = cam_ife_csid_ver1_init_config_pxl_path(
			csid_hw, res);
		break;
	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
	case CAM_IFE_PIX_PATH_RES_RDI_4:
		rc = cam_ife_csid_ver1_init_config_rdi_path(
			csid_hw, res);
		break;
	case CAM_IFE_PIX_PATH_RES_UDI_0:
	case CAM_IFE_PIX_PATH_RES_UDI_1:
	case CAM_IFE_PIX_PATH_RES_UDI_2:
		rc = cam_ife_csid_ver1_init_config_udi_path(
			csid_hw, res);
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d Invalid Res id %d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		break;
	}

	rc = cam_ife_csid_ver1_hw_reset(csid_hw);

	if (rc < 0)
		CAM_ERR(CAM_ISP, "CSID:%d Failed in HW reset",
			csid_hw->hw_intf->hw_idx);

	mutex_unlock(&csid_hw->hw_info->hw_mutex);

	return 0;
end:
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_ife_csid_ver1_disable_csi2(
	struct cam_ife_csid_ver1_hw  *csid_hw)
{
	const struct cam_ife_csid_ver1_reg_info *csid_reg;
	struct cam_hw_soc_info                   *soc_info;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	if (!csid_hw->flags.rx_enabled)	{
		CAM_DBG(CAM_ISP, "CSID:%d Rx already disabled",
			csid_hw->hw_intf->hw_idx);
		return 0;
	}

	/* Disable the CSI2 rx inerrupts */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->irq_mask_addr);

	/* Reset the Rx CFG registers */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->cfg0_addr);
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->cfg1_addr);
	csid_hw->flags.rx_enabled = false;

	CAM_DBG(CAM_ISP, "CSID:%d Disable csi2 rx",
		csid_hw->hw_intf->hw_idx);

	return 0;
}

static int cam_ife_csid_ver1_disable_hw(
	struct cam_ife_csid_ver1_hw *csid_hw)
{
	const struct cam_ife_csid_ver1_reg_info *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	int rc = 0;
	unsigned long                             flags;

	/* Check for refcount */
	if (!csid_hw->hw_info->open_count) {
		CAM_WARN(CAM_ISP, "Unbalanced disable_hw");
		return rc;
	}

	/* Decrement ref Count */
	csid_hw->hw_info->open_count--;

	if (csid_hw->hw_info->open_count)
		return rc;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	cam_ife_csid_ver1_disable_csi2(csid_hw);
	cam_ife_csid_ver1_global_reset(csid_hw);
	/* Disable the top IRQ interrupt */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->top_irq_mask_addr);

	cam_tasklet_stop(csid_hw->tasklet);
	rc = cam_ife_csid_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_ISP, "CSID:%d Disable CSID SOC failed",
			csid_hw->hw_intf->hw_idx);

	spin_lock_irqsave(&csid_hw->lock_state, flags);
	csid_hw->flags.device_enabled = false;
	spin_unlock_irqrestore(&csid_hw->lock_state, flags);
	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	csid_hw->counters.error_irq_count = 0;

	return rc;
}

int cam_ife_csid_ver1_deinit_hw(void *hw_priv,
	void *deinit_args, uint32_t arg_size)
{
	struct cam_ife_csid_ver1_hw *csid_hw  = NULL;
	struct cam_isp_resource_node           *res;
	struct cam_hw_info *hw_info;
	int rc = 0;

	if (!hw_priv || !deinit_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "CSID:Invalid arguments");
		return -EINVAL;
	}

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver1_hw *)hw_info->core_info;
	res = (struct cam_isp_resource_node *)deinit_args;

	if (res->res_type != CAM_ISP_RESOURCE_PIX_PATH) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid Res type %d",
			 csid_hw->hw_intf->hw_idx,
			res->res_type);
		return -EINVAL;
	}

	if (res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_DBG(CAM_ISP, "CSID:%d Res:%d already in De-init state",
			csid_hw->hw_intf->hw_idx,
			res->res_id);
		return -EINVAL;
	}

	switch (res->res_id) {
	case  CAM_IFE_PIX_PATH_RES_IPP:
	case  CAM_IFE_PIX_PATH_RES_PPP:
		rc = cam_ife_csid_ver1_deinit_pxl_path(csid_hw, res);
		break;
	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
	case CAM_IFE_PIX_PATH_RES_RDI_4:
		rc = cam_ife_csid_ver1_deinit_rdi_path(csid_hw, res);
		break;
	case CAM_IFE_PIX_PATH_RES_UDI_0:
	case CAM_IFE_PIX_PATH_RES_UDI_1:
	case CAM_IFE_PIX_PATH_RES_UDI_2:
		rc = cam_ife_csid_ver1_deinit_udi_path(csid_hw, res);
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type%d",
			csid_hw->hw_intf->hw_idx, res->res_type);
		break;
	}

	/* Disable CSID HW */
	cam_ife_csid_ver1_disable_hw(csid_hw);
	CAM_DBG(CAM_ISP, "De-Init CSID %d Path: %d",
		csid_hw->hw_intf->hw_idx, res->res_id);

	return rc;
}

int cam_ife_csid_ver1_start(void *hw_priv, void *args,
			uint32_t arg_size)
{
	struct cam_ife_csid_ver1_hw *csid_hw  = NULL;
	struct cam_isp_resource_node           *res;
	struct cam_hw_info *hw_info;
	struct cam_csid_hw_start_args         *start_args;
	int rc = 0;
	uint32_t i = 0;

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver1_hw *)hw_info->core_info;
	start_args = (struct cam_csid_hw_start_args *)args;

	csid_hw->flags.sof_irq_triggered = false;
	csid_hw->counters.irq_debug_cnt = 0;

	CAM_DBG(CAM_ISP, "CSID:%d num_res %u",
		csid_hw->hw_intf->hw_idx, start_args->num_res);

	cam_ife_csid_ver1_enable_csi2(csid_hw);

	if (csid_hw->res_type == CAM_ISP_IFE_IN_RES_TPG)
		cam_ife_csid_ver1_tpg_start(csid_hw);

	for (i = 0; i < start_args->num_res; i++) {

		res = start_args->node_res[i];
		if (!res || res->res_type != CAM_ISP_RESOURCE_PIX_PATH) {
			CAM_ERR(CAM_ISP, "CSID:%d: res: %p, res type: %d",
				csid_hw->hw_intf->hw_idx, res, (res) ? res->res_type : -1);
			rc = -EINVAL;
			goto end;
		}
		CAM_DBG(CAM_ISP, "CSID:%d res_type :%d res: %s",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_name);

		switch (res->res_id) {
		case  CAM_IFE_PIX_PATH_RES_IPP:
		case  CAM_IFE_PIX_PATH_RES_PPP:
			rc = cam_ife_csid_ver1_start_pix_path(csid_hw, res);
			break;
		case CAM_IFE_PIX_PATH_RES_RDI_0:
		case CAM_IFE_PIX_PATH_RES_RDI_1:
		case CAM_IFE_PIX_PATH_RES_RDI_2:
		case CAM_IFE_PIX_PATH_RES_RDI_3:
		case CAM_IFE_PIX_PATH_RES_RDI_4:
			rc = cam_ife_csid_ver1_start_rdi_path(csid_hw, res);
			break;
		case CAM_IFE_PIX_PATH_RES_UDI_0:
		case CAM_IFE_PIX_PATH_RES_UDI_1:
		case CAM_IFE_PIX_PATH_RES_UDI_2:
			rc = cam_ife_csid_ver1_start_udi_path(csid_hw, res);
			break;
		default:
			CAM_ERR(CAM_ISP, "CSID:%d Invalid res type%d",
					csid_hw->hw_intf->hw_idx, res->res_type);
			rc = -EINVAL;
			break;
		}
	}

	if (rc && res)
		CAM_ERR(CAM_ISP, "CSID:%d start fail res type:%d res id:%d",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id);
end:
	return rc;
}

static int cam_ife_csid_poll_stop_status(
	struct cam_ife_csid_ver1_hw          *csid_hw,
	uint32_t                         res_mask)
{
	int rc = 0, id;
	uint32_t status_addr = 0, val = 0, res_id = 0;
	const struct cam_ife_csid_ver1_reg_info *csid_reg;
	struct cam_hw_soc_info                     *soc_info;

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	for (; res_id < CAM_IFE_PIX_PATH_RES_MAX; res_id++, res_mask >>= 1) {
		if ((res_mask & 0x1) == 0)
			continue;
		val = 0;

		if (res_id == CAM_IFE_PIX_PATH_RES_IPP) {
			status_addr =
			csid_reg->ipp_reg->status_addr;
		} else if (res_id == CAM_IFE_PIX_PATH_RES_PPP) {
			status_addr =
				csid_reg->ppp_reg->status_addr;
		} else if (res_id == CAM_IFE_PIX_PATH_RES_RDI_0 ||
			res_id == CAM_IFE_PIX_PATH_RES_RDI_1 ||
			res_id == CAM_IFE_PIX_PATH_RES_RDI_2 ||
			res_id == CAM_IFE_PIX_PATH_RES_RDI_3 ||
			res_id == CAM_IFE_PIX_PATH_RES_RDI_4) {
			status_addr =
				csid_reg->rdi_reg[res_id]->status_addr;
		} else if (res_id == CAM_IFE_PIX_PATH_RES_UDI_0 ||
			res_id == CAM_IFE_PIX_PATH_RES_UDI_1 ||
			res_id == CAM_IFE_PIX_PATH_RES_UDI_2) {
			id = res_id - CAM_IFE_PIX_PATH_RES_UDI_0;
			status_addr =
				csid_reg->udi_reg[id]->status_addr;
		} else {
			CAM_ERR(CAM_ISP, "Invalid res_id: %u", res_id);
			rc = -EINVAL;
			break;
		}

		CAM_DBG(CAM_ISP, "start polling CSID:%d res_id:%d",
			csid_hw->hw_intf->hw_idx, res_id);

		rc = cam_common_read_poll_timeout(
			    soc_info->reg_map[0].mem_base +
			    status_addr,
			    CAM_IFE_CSID_TIMEOUT_SLEEP_US,
			    CAM_IFE_CSID_TIMEOUT_ALL_US,
			    0x1, 0x1, &val);

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

static int cam_ife_csid_change_pxl_halt_mode(
	struct cam_ife_csid_ver1_hw       *csid_hw,
	struct cam_ife_csid_hw_halt_args  *csid_halt)
{
	uint32_t val = 0;
	const struct cam_ife_csid_ver1_reg_info    *csid_reg;
	struct cam_hw_soc_info                     *soc_info;
	struct cam_isp_resource_node               *res;

	res = csid_halt->node_res;

	csid_reg = csid_hw->core_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (res->res_id != CAM_IFE_PIX_PATH_RES_IPP) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res id %d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	if (res->res_state != CAM_ISP_RESOURCE_STATE_STREAMING) {
		CAM_ERR(CAM_ISP, "CSID:%d Res:%d in invalid state:%d",
			csid_hw->hw_intf->hw_idx, res->res_id, res->res_state);
		return -EINVAL;
	}


	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->irq_mask_addr);

	/* configure Halt for slave */
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->ctrl_addr);
	val &= ~0xC;
	val |= (csid_halt->halt_mode << 2);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->ctrl_addr);
	CAM_DBG(CAM_ISP, "CSID:%d IPP path Res halt mode:%d configured:%x",
		csid_hw->hw_intf->hw_idx, csid_halt->halt_mode, val);

	return 0;
}

int cam_ife_csid_halt(struct cam_ife_csid_ver1_hw *csid_hw,
		void *halt_args)
{
	struct cam_isp_resource_node         *res;
	struct cam_ife_csid_hw_halt_args     *csid_halt;
	int rc = 0;

	if (!csid_hw || !halt_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_halt = (struct cam_ife_csid_hw_halt_args *)halt_args;

	/* Change the halt mode */
	res = csid_halt->node_res;
	CAM_DBG(CAM_ISP, "CSID:%d res_type %d res_id %d",
		csid_hw->hw_intf->hw_idx,
		res->res_type, res->res_id);

	if (res->res_type != CAM_ISP_RESOURCE_PIX_PATH) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type %d",
			csid_hw->hw_intf->hw_idx,
			res->res_type);
		return -EINVAL;
	}

	switch (res->res_id) {
	case CAM_IFE_PIX_PATH_RES_IPP:
		rc = cam_ife_csid_change_pxl_halt_mode(csid_hw, csid_halt);
		break;
	default:
		CAM_DBG(CAM_ISP, "CSID:%d res_id %d",
			csid_hw->hw_intf->hw_idx,
			res->res_id);
		break;
	}

	return rc;
}

int cam_ife_csid_ver1_stop(void *hw_priv,
	void *stop_args, uint32_t arg_size)
{
	struct cam_ife_csid_ver1_hw *csid_hw  = NULL;
	struct cam_isp_resource_node           *res;
	struct cam_hw_info *hw_info;
	int rc = 0;
	uint32_t i;
	struct cam_csid_hw_stop_args         *csid_stop;
	uint32_t res_mask = 0;

	if (!hw_priv || !stop_args ||
		(arg_size != sizeof(struct cam_csid_hw_stop_args))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_stop = (struct cam_csid_hw_stop_args  *) stop_args;

	if (!csid_stop->num_res) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver1_hw *)hw_info->core_info;
	CAM_DBG(CAM_ISP, "CSID:%d num_res %d",
		csid_hw->hw_intf->hw_idx,
		csid_stop->num_res);
	cam_ife_csid_ver1_tpg_stop(csid_hw);
	/* Stop the resource first */
	for (i = 0; i < csid_stop->num_res; i++) {

		res = csid_stop->node_res[i];
		res_mask |= (1 << res->res_id);
		CAM_DBG(CAM_ISP, "CSID:%d res_type %d Resource[id:%d name:%s]",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_name);

		switch (res->res_id) {
		case CAM_IFE_PIX_PATH_RES_IPP:
		case CAM_IFE_PIX_PATH_RES_PPP:
			rc = cam_ife_csid_ver1_stop_pxl_path(csid_hw,
				res, csid_stop->stop_cmd);
			break;
		case CAM_IFE_PIX_PATH_RES_RDI_0:
		case CAM_IFE_PIX_PATH_RES_RDI_1:
		case CAM_IFE_PIX_PATH_RES_RDI_2:
		case CAM_IFE_PIX_PATH_RES_RDI_3:
		case CAM_IFE_PIX_PATH_RES_RDI_4:
			rc = cam_ife_csid_ver1_stop_rdi_path(csid_hw,
				res, csid_stop->stop_cmd);
			break;
		case CAM_IFE_PIX_PATH_RES_UDI_0:
		case CAM_IFE_PIX_PATH_RES_UDI_1:
		case CAM_IFE_PIX_PATH_RES_UDI_2:
			rc = cam_ife_csid_ver1_stop_udi_path(csid_hw,
				res, csid_stop->stop_cmd);
			break;
		default:
			CAM_ERR(CAM_ISP, "Invalid res_id: %u",
				res->res_id);
			break;
		}
	}

	if (res_mask)
		rc = cam_ife_csid_poll_stop_status(csid_hw, res_mask);

	for (i = 0; i < csid_stop->num_res; i++) {
		res = csid_stop->node_res[i];
		res->res_state = CAM_ISP_RESOURCE_STATE_INIT_HW;
	}

	csid_hw->counters.error_irq_count = 0;

	return rc;
}

int cam_ife_csid_ver1_read(void *hw_priv,
	void *read_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "CSID: un supported");

	return -EINVAL;
}

int cam_ife_csid_ver1_write(void *hw_priv,
	void *write_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "CSID: un supported");
	return -EINVAL;
}

static int cam_ife_csid_ver1_sof_irq_debug(
	struct cam_ife_csid_ver1_hw *csid_hw,
	void *cmd_args)
{
	int i = 0;
	uint32_t val = 0;
	bool sof_irq_enable = false;
	struct cam_hw_soc_info                  *soc_info;
	struct cam_ife_csid_ver1_reg_info *csid_reg;
	uint32_t data_idx;

	if (*((uint32_t *)cmd_args) == 1)
		sof_irq_enable = true;

	if (csid_hw->hw_info->hw_state ==
		CAM_HW_STATE_POWER_DOWN) {
		CAM_WARN(CAM_ISP,
			"CSID powered down unable to %s sof irq",
			(sof_irq_enable) ? "enable" : "disable");
		return 0;
	}

	data_idx = csid_hw->rx_cfg.phy_sel;
	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	for (i = 0; i < csid_reg->cmn_reg->num_pix; i++) {

		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->irq_mask_addr);

		if (sof_irq_enable)
			val |= IFE_CSID_VER1_PATH_INFO_INPUT_SOF;
		else
			val &= ~IFE_CSID_VER1_PATH_INFO_INPUT_SOF;

		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->irq_mask_addr);
	}

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++) {
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->irq_mask_addr);

		if (sof_irq_enable)
			val |= IFE_CSID_VER1_PATH_INFO_INPUT_SOF;
		else
			val &= ~IFE_CSID_VER1_PATH_INFO_INPUT_SOF;

		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->irq_mask_addr);
	}

	for (i = 0; i < csid_reg->cmn_reg->num_udis; i++) {
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->udi_reg[i]->irq_mask_addr);

		if (sof_irq_enable)
			val |= IFE_CSID_VER1_PATH_INFO_INPUT_SOF;
		else
			val &= ~IFE_CSID_VER1_PATH_INFO_INPUT_SOF;

		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->udi_reg[i]->irq_mask_addr);
	}

	if (sof_irq_enable) {
		csid_hw->debug_info.path_mask |=
			IFE_CSID_VER1_PATH_INFO_INPUT_SOF;
		csid_hw->debug_info.debug_val |=
			CAM_IFE_CSID_DEBUG_ENABLE_SOF_IRQ;
		csid_hw->flags.sof_irq_triggered = true;
	} else {
		csid_hw->debug_info.path_mask &=
			~IFE_CSID_VER1_PATH_INFO_INPUT_SOF;
		csid_hw->debug_info.debug_val &=
			~CAM_IFE_CSID_DEBUG_ENABLE_SOF_IRQ;
		csid_hw->flags.sof_irq_triggered = false;
	}

	CAM_INFO(CAM_ISP, "SOF freeze: CSID SOF irq %s",
		(sof_irq_enable) ? "enabled" : "disabled");

	CAM_INFO(CAM_ISP, "Notify CSIPHY: %d",
			csid_hw->rx_cfg.phy_sel);

	cam_subdev_notify_message(CAM_CSIPHY_DEVICE_TYPE,
			CAM_SUBDEV_MESSAGE_IRQ_ERR,
			(void *)&data_idx);

	return 0;
}


static int cam_ife_csid_ver1_print_hbi_vbi(
	struct cam_ife_csid_ver1_hw  *csid_hw,
	struct cam_isp_resource_node *res)
{
	struct cam_ife_csid_ver1_path_reg_info      *path_reg = NULL;
	struct cam_ife_csid_ver1_reg_info           *csid_reg;
	struct cam_hw_soc_info                      *soc_info;
	uint32_t                                     hbi, vbi;

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (res->res_type != CAM_ISP_RESOURCE_PIX_PATH ||
		res->res_id >= CAM_IFE_PIX_PATH_RES_MAX) {
		CAM_DBG(CAM_ISP, "CSID[%u] Invalid res_type:%d res [id: %d name: %s]",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id, res->res_name);
		return -EINVAL;
	}

	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID[%u] Invalid dev state :%d",
			csid_hw->hw_intf->hw_idx,
			csid_hw->hw_info->hw_state);
		return -EINVAL;
	}

	switch (res->res_id) {
	case CAM_IFE_PIX_PATH_RES_IPP:
		path_reg = csid_reg->ipp_reg;
		break;
	case CAM_IFE_PIX_PATH_RES_PPP:
		path_reg = csid_reg->ppp_reg;
		break;
	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
	case CAM_IFE_PIX_PATH_RES_RDI_4:
		path_reg = csid_reg->rdi_reg[res->res_id];
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID[%u] invalid res %d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID[%u] invalid res: %d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	hbi = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		path_reg->format_measure1_addr);
	vbi = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		path_reg->format_measure2_addr);
	CAM_INFO_RATE_LIMIT(CAM_ISP,
		"CSID[%u] res[id: %d name: %s] hbi 0x%x vbi 0x%x",
		csid_hw->hw_intf->hw_idx, res->res_id, res->res_name,
		hbi, vbi);

	return 0;
}

static int cam_ife_csid_ver1_get_time_stamp(
	struct cam_ife_csid_ver1_hw  *csid_hw, void *cmd_args)
{
	struct cam_isp_resource_node         *res = NULL;
	uint64_t time_lo, time_hi;
	struct cam_hw_soc_info              *soc_info;
	struct cam_csid_get_time_stamp_args *timestamp_args;
	struct cam_ife_csid_ver1_reg_info *csid_reg;
	uint64_t  time_delta;
	struct timespec64 ts;
	uint32_t curr_0_sof_addr, curr_1_sof_addr;

	timestamp_args = (struct cam_csid_get_time_stamp_args *)cmd_args;
	res = timestamp_args->node_res;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (res->res_type != CAM_ISP_RESOURCE_PIX_PATH ||
		res->res_id >= CAM_IFE_PIX_PATH_RES_MAX) {
		CAM_DBG(CAM_ISP, "CSID:%d Invalid res_type:%d res id%d",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid dev state :%d",
			csid_hw->hw_intf->hw_idx,
			csid_hw->hw_info->hw_state);
		return -EINVAL;
	}

	switch (res->res_id) {
	case CAM_IFE_PIX_PATH_RES_IPP:
		curr_0_sof_addr = csid_reg->ipp_reg->timestamp_curr0_sof_addr;
		curr_1_sof_addr = csid_reg->ipp_reg->timestamp_curr1_sof_addr;
		break;
	case CAM_IFE_PIX_PATH_RES_PPP:
		curr_0_sof_addr = csid_reg->ppp_reg->timestamp_curr0_sof_addr;
		curr_1_sof_addr = csid_reg->ppp_reg->timestamp_curr1_sof_addr;
		break;
	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
	case CAM_IFE_PIX_PATH_RES_RDI_4:
		curr_0_sof_addr =
			csid_reg->rdi_reg
			[res->res_id]->timestamp_curr0_sof_addr;
		curr_1_sof_addr =
			csid_reg->rdi_reg
			[res->res_id]->timestamp_curr1_sof_addr;
	break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d invalid res %d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	time_hi = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			curr_1_sof_addr);
	time_lo = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			curr_0_sof_addr);
	timestamp_args->time_stamp_val = (time_hi << 32) | time_lo;

	timestamp_args->time_stamp_val = mul_u64_u32_div(
		timestamp_args->time_stamp_val,
		CAM_IFE_CSID_QTIMER_MUL_FACTOR,
		CAM_IFE_CSID_QTIMER_DIV_FACTOR);

	time_delta = timestamp_args->time_stamp_val -
		csid_hw->timestamp.prev_sof_ts;

	if (!csid_hw->timestamp.prev_boot_ts) {
		ktime_get_boottime_ts64(&ts);
		timestamp_args->boot_timestamp =
			(uint64_t)((ts.tv_sec * 1000000000) +
			ts.tv_nsec);
	} else {
		timestamp_args->boot_timestamp =
			csid_hw->timestamp.prev_boot_ts + time_delta;
	}

	CAM_DBG(CAM_ISP, "timestamp:%lld",
		timestamp_args->boot_timestamp);
	csid_hw->timestamp.prev_sof_ts = timestamp_args->time_stamp_val;
	csid_hw->timestamp.prev_boot_ts = timestamp_args->boot_timestamp;

	return 0;
}

static int cam_ife_csid_ver1_set_csid_clock(
	struct cam_ife_csid_ver1_hw          *csid_hw,
	void *cmd_args)
{
	struct cam_ife_csid_clock_update_args *clk_update = NULL;

	if (!csid_hw)
		return -EINVAL;

	clk_update =
		(struct cam_ife_csid_clock_update_args *)cmd_args;

	csid_hw->clk_rate = clk_update->clk_rate;

	return 0;
}

int cam_ife_csid_ver1_set_csid_qcfa(
	struct cam_ife_csid_ver1_hw  *csid_hw,
	void *cmd_args)
{
	struct  cam_ife_csid_ver1_path_cfg *path_cfg = NULL;
	struct cam_ife_csid_qcfa_update_args *qcfa_update = NULL;
	struct cam_isp_resource_node *res = NULL;

	if (!csid_hw || !cmd_args) {
		CAM_ERR(CAM_ISP, "Invalid param %pK %pK",
			csid_hw, cmd_args);
		return -EINVAL;
	}

	qcfa_update =
		(struct cam_ife_csid_qcfa_update_args *)cmd_args;
	res = qcfa_update->res;

	if (!res) {
		CAM_ERR(CAM_ISP, "CSID[%u] NULL res",
			csid_hw->hw_intf->hw_idx);
		return -EINVAL;
	}

	path_cfg = (struct  cam_ife_csid_ver1_path_cfg *)res->res_priv;

	if (!path_cfg) {
		CAM_ERR(CAM_ISP, "CSID[%u] Invalid res_id %u",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	path_cfg->qcfa_bin = qcfa_update->qcfa_binning;

	CAM_DBG(CAM_ISP, "CSID %u QCFA binning %d",
		csid_hw->hw_intf->hw_idx,
		path_cfg->qcfa_bin);

	return 0;
}

static int cam_ife_csid_ver1_dump_hw(
	struct cam_ife_csid_ver1_hw *csid_hw, void *cmd_args)
{
	int                             i;
	uint8_t                        *dst;
	uint32_t                       *addr, *start;
	uint32_t                        min_len;
	uint32_t                        num_reg;
	size_t                          remain_len;
	struct cam_isp_hw_dump_header  *hdr;
	struct cam_isp_hw_dump_args    *dump_args =
		(struct cam_isp_hw_dump_args *)cmd_args;
	struct cam_hw_soc_info         *soc_info;

	if (!dump_args) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}
	if (!dump_args->cpu_addr || !dump_args->buf_len) {
		CAM_ERR(CAM_ISP,
			"Invalid params %pK %zu",
			(void *)dump_args->cpu_addr,
			dump_args->buf_len);
		return -EINVAL;
	}
	soc_info = &csid_hw->hw_info->soc_info;
	if (dump_args->buf_len <= dump_args->offset) {
		CAM_WARN(CAM_ISP,
			"Dump offset overshoot offset %zu buf_len %zu",
			dump_args->offset, dump_args->buf_len);
		return -ENOSPC;
	}
	min_len = soc_info->reg_map[0].size +
		sizeof(struct cam_isp_hw_dump_header) +
		sizeof(uint32_t);
	remain_len = dump_args->buf_len - dump_args->offset;
	if (remain_len < min_len) {
		CAM_WARN(CAM_ISP, "Dump buffer exhaust remain %zu, min %u",
			remain_len, min_len);
		return -ENOSPC;
	}
	dst = (uint8_t *)dump_args->cpu_addr + dump_args->offset;
	hdr = (struct cam_isp_hw_dump_header *)dst;
	scnprintf(hdr->tag, CAM_ISP_HW_DUMP_TAG_MAX_LEN, "CSID_REG:");
	addr = (uint32_t *)(dst + sizeof(struct cam_isp_hw_dump_header));

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
		sizeof(struct cam_isp_hw_dump_header);
	CAM_DBG(CAM_ISP, "offset %zu", dump_args->offset);
	return 0;
}

static int cam_ife_csid_log_acquire_data(
	struct cam_ife_csid_ver1_hw *csid_hw, void *cmd_args)
{
	struct cam_isp_resource_node          *res = NULL;
	struct cam_hw_soc_info                *soc_info;
	struct cam_ife_csid_ver1_reg_info     *csid_reg;
	struct cam_ife_csid_ver1_path_cfg     *path_cfg = NULL;
	struct cam_ife_csid_ver1_path_reg_info *rdi_reg = NULL;
	uint32_t byte_cnt_ping, byte_cnt_pong;

	res = (struct cam_isp_resource_node *)cmd_args;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (res->res_state <= CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP,
			"CSID:%d invalid res id:%d res type: %d state:%d",
			csid_hw->hw_intf->hw_idx, res->res_id, res->res_type,
			res->res_state);
		return -EINVAL;
	}

	path_cfg = (struct cam_ife_csid_ver1_path_cfg *)res->res_priv;
	/* Dump all the acquire data for this hardware */
	CAM_INFO(CAM_ISP,
		"CSID:%d res id:%d type:%d state:%d in f:%d out f:%d st pix:%d end pix:%d st line:%d end line:%d h bin:%d qcfa bin:%d",
		csid_hw->hw_intf->hw_idx, res->res_id, res->res_type,
		res->res_type, path_cfg->in_format, path_cfg->out_format,
		path_cfg->start_pixel, path_cfg->end_pixel,
		path_cfg->start_line, path_cfg->end_line,
		path_cfg->horizontal_bin, path_cfg->qcfa_bin);

	if (res->res_id >= CAM_IFE_PIX_PATH_RES_RDI_0  &&
		res->res_id <= CAM_IFE_PIX_PATH_RES_RDI_3) {
		rdi_reg = csid_reg->rdi_reg[res->res_id];
		/* read total number of bytes transmitted through RDI */
		byte_cnt_ping = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			rdi_reg->byte_cntr_ping_addr);
		byte_cnt_pong = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			rdi_reg->byte_cntr_pong_addr);
		CAM_INFO(CAM_ISP,
			"CSID:%d res id:%d byte cnt val ping:%d pong:%d",
			csid_hw->hw_intf->hw_idx, res->res_id,
			byte_cnt_ping, byte_cnt_pong);
	}

	return 0;
}

static int cam_ife_csid_ver1_dump_csid_clock(
	struct cam_ife_csid_ver1_hw *csid_hw, void *cmd_args)
{
	if (!csid_hw)
		return -EINVAL;

	CAM_INFO(CAM_ISP, "CSID:%d clock rate %llu",
		csid_hw->hw_intf->hw_idx,
		csid_hw->clk_rate);

	return 0;
}

static int cam_ife_csid_ver1_process_cmd(void *hw_priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_ife_csid_ver1_hw          *csid_hw;
	struct cam_hw_info                   *hw_info;
	struct cam_isp_resource_node         *res = NULL;

	if (!hw_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid arguments");
		return -EINVAL;
	}

	hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver1_hw *)hw_info->core_info;

	switch (cmd_type) {
	case CAM_IFE_CSID_CMD_GET_TIME_STAMP:
		rc = cam_ife_csid_ver1_get_time_stamp(csid_hw, cmd_args);

		if (csid_hw->debug_info.debug_val &
				CAM_IFE_CSID_DEBUG_ENABLE_HBI_VBI_INFO) {
			res = ((struct cam_csid_get_time_stamp_args *)
				cmd_args)->node_res;
			cam_ife_csid_ver1_print_hbi_vbi(csid_hw, res);
		}

		break;
	case CAM_IFE_CSID_SET_CSID_DEBUG:
		rc = cam_ife_csid_ver1_set_debug(csid_hw,
			*((uint32_t *)cmd_args));
		break;
	case CAM_IFE_CSID_SOF_IRQ_DEBUG:
		rc = cam_ife_csid_ver1_sof_irq_debug(csid_hw, cmd_args);
		break;
	case CAM_ISP_HW_CMD_CSID_CLOCK_UPDATE:
		rc = cam_ife_csid_ver1_set_csid_clock(csid_hw, cmd_args);
		break;
	case CAM_ISP_HW_CMD_CSID_QCFA_SUPPORTED:
		rc = cam_ife_csid_ver1_set_csid_qcfa(csid_hw,
			cmd_args);
		break;
	case CAM_ISP_HW_CMD_DUMP_HW:
		rc = cam_ife_csid_ver1_dump_hw(csid_hw, cmd_args);
		break;
	case CAM_ISP_HW_CMD_CSID_CLOCK_DUMP:
		rc = cam_ife_csid_ver1_dump_csid_clock(csid_hw, cmd_args);
		break;
	case CAM_IFE_CSID_TOP_CONFIG:
		break;
	case CAM_IFE_CSID_SET_DUAL_SYNC_CONFIG:
		break;
	case CAM_IFE_CSID_LOG_ACQUIRE_DATA:
		cam_ife_csid_log_acquire_data(csid_hw, cmd_args);
		break;
	case CAM_ISP_HW_CMD_CSID_CHANGE_HALT_MODE:
		rc = cam_ife_csid_halt(csid_hw, cmd_args);
		break;
	case CAM_ISP_HW_CMD_CSID_DISCARD_INIT_FRAMES:
		/* Not supported for V1 */
		rc = 0;
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d unsupported cmd:%d",
			csid_hw->hw_intf->hw_idx, cmd_type);
		rc = -EINVAL;
		break;
	}
	return rc;

}

static int cam_ife_csid_ver1_handle_rx_debug_event(
	struct cam_ife_csid_ver1_hw *csid_hw,
	uint32_t bit_pos)
{
	struct cam_hw_soc_info              *soc_info;
	struct cam_ife_csid_ver1_reg_info *csid_reg;
	const struct cam_ife_csid_csi2_rx_reg_info *csi2_reg;
	uint32_t mask, val;

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;
	csi2_reg = csid_reg->csi2_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	mask  = BIT(bit_pos);

	switch (mask) {
	case IFE_CSID_VER1_RX_LONG_PKT_CAPTURED:

		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->captured_long_pkt_0_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"Csid :%d Long pkt VC: %d DT: %d WC: %d",
			csid_hw->hw_intf->hw_idx,
			val & csi2_reg->vc_mask,
			val & csi2_reg->dt_mask,
			val & csi2_reg->wc_mask);

		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->captured_long_pkt_1_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"Csid :%d Long pkt ECC: %d",
			csid_hw->hw_intf->hw_idx, val);

		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->captured_long_pkt_ftr_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"Csid :%d Long pkt cal CRC: %d expected CRC: %d",
			csid_hw->hw_intf->hw_idx,
			val & csi2_reg->calc_crc_mask,
			val & csi2_reg->expected_crc_mask);
		break;

	case IFE_CSID_VER1_RX_SHORT_PKT_CAPTURED:

		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->captured_short_pkt_0_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"Csid :%d Long pkt VC: %d DT: %d LC: %d",
			csid_hw->hw_intf->hw_idx,
			val & csi2_reg->vc_mask,
			val & csi2_reg->dt_mask,
			val & csi2_reg->wc_mask);

		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->captured_short_pkt_1_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"Csid :%d Long pkt ECC: %d",
			csid_hw->hw_intf->hw_idx, val);
		break;
	case IFE_CSID_VER1_RX_CPHY_PKT_HDR_CAPTURED:

		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->captured_cphy_pkt_hdr_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"Csid :%d CPHY pkt VC: %d DT: %d LC: %d",
			csid_hw->hw_intf->hw_idx,
			val & csi2_reg->vc_mask,
			val & csi2_reg->dt_mask,
			val & csi2_reg->wc_mask);
		break;
	case IFE_CSID_VER1_RX_UNMAPPED_VC_DT:
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->cap_unmap_long_pkt_hdr_0_addr);

		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"CSID:%d UNMAPPED_VC_DT: VC:%d DT:%d WC:%d not mapped to any csid paths",
			csid_hw->hw_intf->hw_idx, (val >> 22),
			((val >> 16) & 0x3F), (val & 0xFFFF));

		csid_hw->counters.error_irq_count++;

		CAM_DBG(CAM_ISP, "CSID[%u] Recoverable Error Count:%u",
			csid_hw->hw_intf->hw_idx,
			csid_hw->counters.error_irq_count);
		break;
	default:
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"CSID[%d] RX_IRQ: %s",
			csid_hw->hw_intf->hw_idx,
			ver1_rx_irq_desc[bit_pos].desc);
		break;
	}

	return 0;
}

static int cam_ife_csid_ver1_handle_event_err(
	struct cam_ife_csid_ver1_hw *csid_hw,
	struct cam_ife_csid_ver1_evt_payload *evt_payload,
	uint32_t err_type)
{
	struct cam_isp_hw_error_event_info err_evt_info;
	struct cam_isp_hw_event_info event_info = {0};
	int rc = 0;

	event_info.hw_idx = evt_payload->hw_idx;
	err_evt_info.err_type = err_type;
	event_info.hw_type = CAM_ISP_HW_TYPE_CSID;
	event_info.event_data = (void *)&err_evt_info;

	CAM_DBG(CAM_ISP, "CSID[%d] Error type %d",
		csid_hw->hw_intf->hw_idx, err_type);

	rc = csid_hw->event_cb(csid_hw->token,
		CAM_ISP_HW_EVENT_ERROR, (void *)&event_info);

	return rc;
}

static int cam_ife_csid_ver1_put_evt_payload(
	struct cam_ife_csid_ver1_hw *csid_hw,
	struct cam_ife_csid_ver1_evt_payload **evt_payload,
	struct list_head    *payload_list)
{
	unsigned long flags;

	if (*evt_payload == NULL) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid payload core %d",
			csid_hw->hw_intf->hw_idx);
		return -EINVAL;
	}
	spin_lock_irqsave(&csid_hw->lock_state, flags);
	list_add_tail(&(*evt_payload)->list,
		payload_list);
	*evt_payload = NULL;
	spin_unlock_irqrestore(&csid_hw->lock_state, flags);

	return 0;
}

static int cam_ife_csid_ver1_get_evt_payload(
	struct cam_ife_csid_ver1_hw *csid_hw,
	struct cam_ife_csid_ver1_evt_payload **evt_payload,
	struct list_head    *payload_list)
{

	spin_lock(&csid_hw->lock_state);

	if (list_empty(payload_list)) {
		*evt_payload = NULL;
		spin_unlock(&csid_hw->lock_state);
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No free payload core %d",
			csid_hw->hw_intf->hw_idx);
		return -ENOMEM;
	}

	*evt_payload = list_first_entry(payload_list,
			struct cam_ife_csid_ver1_evt_payload, list);
	list_del_init(&(*evt_payload)->list);
	spin_unlock(&csid_hw->lock_state);

	return 0;
}

static int cam_ife_csid_ver1_rx_bottom_half_handler(
		struct cam_ife_csid_ver1_hw *csid_hw,
		struct cam_ife_csid_ver1_evt_payload *evt_payload)
{
	const struct cam_ife_csid_csi2_rx_reg_info *csi2_reg;
	struct cam_ife_csid_ver1_reg_info          *csid_reg;
	uint8_t                                    *log_buf = NULL;
	uint32_t                                    irq_status;
	uint32_t                                    val;
	uint32_t                                    event_type = 0;
	size_t                                      len = 0;
	struct cam_hw_soc_info                     *soc_info;
	uint32_t                                    data_idx;

	if (!csid_hw || !evt_payload) {
		CAM_ERR(CAM_ISP,
			"Invalid Param handler_priv %pK evt_payload_priv %pK",
			csid_hw, evt_payload);
		return -EINVAL;
	}

	data_idx = csid_hw->rx_cfg.phy_sel;
	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;
	csi2_reg = csid_reg->csi2_reg;

	irq_status = evt_payload->irq_status[CAM_IFE_CSID_IRQ_REG_RX]
			& csi2_reg->fatal_err_mask;
	log_buf = csid_hw->log_buf;
	memset(log_buf, 0, sizeof(csid_hw->log_buf));

	CAM_INFO(CAM_ISP, "IRQ_Status: 0x%x",
		evt_payload->irq_status[CAM_IFE_CSID_IRQ_REG_RX]);

	if (irq_status) {
		CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len, "Fatal Errors:\n");

		if (irq_status & IFE_CSID_VER1_RX_LANE0_FIFO_OVERFLOW)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"RX_ERROR_LANE0_FIFO_OVERFLOW: Skew/Less Data on lanes/ Slow csid clock:%luHz\n",
				soc_info->applied_src_clk_rate);

		if (irq_status & IFE_CSID_VER1_RX_LANE1_FIFO_OVERFLOW)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"RX_ERROR_LANE1_FIFO_OVERFLOW: Skew/Less Data on lanes/ Slow csid clock:%luHz\n",
				soc_info->applied_src_clk_rate);

		if (irq_status & IFE_CSID_VER1_RX_LANE2_FIFO_OVERFLOW)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"RX_ERROR_LANE2_FIFO_OVERFLOW: Skew/Less Data on lanes/ Slow csid clock:%luHz\n",
				soc_info->applied_src_clk_rate);

		if (irq_status & IFE_CSID_VER1_RX_LANE3_FIFO_OVERFLOW)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"RX_ERROR_LANE3_FIFO_OVERFLOW: Skew/Less Data on lanes/ Slow csid clock:%luHz\n",
				soc_info->applied_src_clk_rate);

		if (irq_status & IFE_CSID_VER1_RX_TG_FIFO_OVERFLOW) {
			event_type |= CAM_ISP_HW_ERROR_CSID_FIFO_OVERFLOW;
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"RX_ERROR_TPG_FIFO_OVERFLOW: Backpressure from IFE\n");
		}

		if (irq_status & IFE_CSID_VER1_RX_CPHY_PH_CRC)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"CPHY_PH_CRC: Pkt Hdr CRC mismatch\n");

		if (irq_status & IFE_CSID_VER1_RX_STREAM_UNDERFLOW) {
			val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
				csi2_reg->captured_long_pkt_0_addr);

			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"ERROR_STREAM_UNDERFLOW: Fewer bytes rcvd than WC:%d in pkt hdr\n",
				val & 0xFFFF);
		}

		if (irq_status & IFE_CSID_VER1_RX_ERROR_ECC)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"DPHY_ERROR_ECC: Pkt hdr errors unrecoverable\n");
	}

	irq_status = evt_payload->irq_status[CAM_IFE_CSID_IRQ_REG_RX] &
		csi2_reg->part_fatal_err_mask;

	if (irq_status) {
		CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
			"Recoverable-errors:\n");

		if (irq_status & IFE_CSID_VER1_RX_CPHY_EOT_RECEPTION)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"CPHY_EOT_RECEPTION: No EOT on lane/s, is_EPD: %d, PHY_Type: %s(%u)\n",
				csid_hw->rx_cfg.epd_supported,
				(csid_hw->rx_cfg.lane_type) ? "cphy" : "dphy",
				csid_hw->rx_cfg.lane_type);

		if (irq_status & IFE_CSID_VER1_RX_CPHY_SOT_RECEPTION)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"CPHY_SOT_RECEPTION: Less SOTs on lane/s\n");

		if (irq_status & IFE_CSID_VER1_RX_ERROR_CRC)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"CPHY_ERROR_CRC: Long pkt payload CRC mismatch\n");

		if (irq_status & IFE_CSID_VER1_RX_UNBOUNDED_FRAME)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"UNBOUNDED_FRAME: Frame started with EOF or No EOF\n");
	}

	irq_status = evt_payload->irq_status[CAM_IFE_CSID_IRQ_REG_RX] &
		csi2_reg->non_fatal_err_mask;

	if (irq_status) {
		CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
			"Non-fatal errors:\n");
		if (irq_status & IFE_CSID_VER1_RX_MMAPPED_VC_DT) {
			val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
				csi2_reg->captured_long_pkt_0_addr);

			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"MMAPPED_VC_DT: VC:%d DT:%d mapped to more than 1 csid paths\n",
				(val >> 22), ((val >> 16) & 0x3F));
		}
	}

	if (len)
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID[%u] %s",
			csid_hw->hw_intf->hw_idx, log_buf);

	if (csid_hw->flags.fatal_err_detected) {
		event_type |= CAM_ISP_HW_ERROR_CSID_FATAL;
		cam_subdev_notify_message(CAM_CSIPHY_DEVICE_TYPE,
				CAM_SUBDEV_MESSAGE_IRQ_ERR,
				(void *)&data_idx);
	}
	if (event_type)
		cam_ife_csid_ver1_handle_event_err(csid_hw,
			evt_payload, event_type);

	return IRQ_HANDLED;
}

static int cam_ife_csid_ver1_path_bottom_half_handler(
	const struct cam_ife_csid_ver1_path_reg_info  *path_reg,
	struct cam_ife_csid_ver1_evt_payload          *evt_payload,
	struct cam_ife_csid_ver1_hw                   *csid_hw,
	uint32_t                                       index)
{
	const uint8_t                               **irq_reg_tag;
	uint8_t                                     *log_buf = NULL;
	uint32_t                                     bit_pos = 0;
	uint32_t                                     irq_status;
	size_t                                       len = 0;

	if (!csid_hw || !evt_payload) {
		CAM_ERR(CAM_ISP,
			"Invalid Param csid_hw %pK evt_payload %pK",
			csid_hw, evt_payload);
		return 0;
	}

	irq_status = evt_payload->irq_status[index] & path_reg->fatal_err_mask;
	bit_pos = 0;
	log_buf = csid_hw->log_buf;
	memset(log_buf, 0, sizeof(csid_hw->log_buf));
	irq_reg_tag = cam_ife_csid_get_irq_reg_tag_ptr();

	while (irq_status) {
		if ((irq_status & 0x1))
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"%s\n", ver1_path_irq_desc[bit_pos]);
		bit_pos++;
		irq_status >>= 1;
	}

	if (len)
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"CSID[%u] %s: %s",
			csid_hw->hw_intf->hw_idx,
			irq_reg_tag[index],
			log_buf);

	if (evt_payload->irq_status[index] &
		IFE_CSID_VER1_PATH_ERROR_FIFO_OVERFLOW)
		cam_ife_csid_ver1_handle_event_err(csid_hw,
			evt_payload, CAM_ISP_HW_ERROR_CSID_FIFO_OVERFLOW);

	return 0;
}

static int cam_ife_csid_ver1_bottom_half_handler(
		void *handler_priv,
		void *evt_payload_priv)
{
	const struct cam_ife_csid_ver1_path_reg_info  *path_reg;
	struct cam_ife_csid_ver1_evt_payload          *evt_payload;
	struct cam_ife_csid_ver1_reg_info             *csid_reg;
	struct cam_ife_csid_ver1_hw                   *csid_hw;
	int                                            i;
	int                                            id = 0;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP,
			"Invalid Param handler_priv %pK evt_payload_priv %pK",
			handler_priv, evt_payload_priv);
		return -EINVAL;
	}

	csid_hw = (struct cam_ife_csid_ver1_hw *)handler_priv;
	evt_payload = (struct cam_ife_csid_ver1_evt_payload *)evt_payload_priv;

	if (evt_payload->irq_status[CAM_IFE_CSID_IRQ_REG_RX])
		cam_ife_csid_ver1_rx_bottom_half_handler(
			csid_hw, evt_payload);

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	for (i = CAM_IFE_CSID_IRQ_REG_RDI_0; i < CAM_IFE_CSID_IRQ_REG_MAX;
		i++) {

		if (!evt_payload->irq_status[i])
			continue;

		path_reg = NULL;

		switch (i) {
		case  CAM_IFE_CSID_IRQ_REG_IPP:
			path_reg = csid_reg->ipp_reg;
			break;
		case  CAM_IFE_CSID_IRQ_REG_PPP:
			path_reg = csid_reg->ppp_reg;
			break;
		case  CAM_IFE_CSID_IRQ_REG_RDI_0:
		case  CAM_IFE_CSID_IRQ_REG_RDI_1:
		case  CAM_IFE_CSID_IRQ_REG_RDI_2:
		case  CAM_IFE_CSID_IRQ_REG_RDI_3:
		case  CAM_IFE_CSID_IRQ_REG_RDI_4:
			id = i - CAM_IFE_CSID_IRQ_REG_RDI_0;
			path_reg = csid_reg->rdi_reg[id];
			break;
		case  CAM_IFE_CSID_IRQ_REG_UDI_0:
		case  CAM_IFE_CSID_IRQ_REG_UDI_1:
		case  CAM_IFE_CSID_IRQ_REG_UDI_2:
			id = i - CAM_IFE_CSID_IRQ_REG_UDI_0;
			path_reg = csid_reg->udi_reg[id];
			break;
		default:
			break;
		}

		if (!path_reg)
			continue;

		cam_ife_csid_ver1_path_bottom_half_handler(
			path_reg, evt_payload, csid_hw, i);
	}

	cam_ife_csid_ver1_put_evt_payload(csid_hw, &evt_payload,
		&csid_hw->free_payload_list);

	return IRQ_HANDLED;
}

static int cam_ife_csid_ver1_rx_top_half(
	uint32_t                             *irq_status,
	struct cam_ife_csid_ver1_hw          *csid_hw,
	uint32_t                             *need_bh_sched)
{
	const struct cam_ife_csid_csi2_rx_reg_info *csi2_reg;
	struct cam_ife_csid_ver1_reg_info          *csid_reg;
	struct cam_hw_soc_info                     *soc_info;
	uint32_t                                    status = 0;
	uint32_t                                    debug_bits;
	uint32_t                                    bit_pos = 0;

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;
	csi2_reg = csid_reg->csi2_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	status = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->irq_status_addr);

	if (csid_hw->rx_cfg.epd_supported)
		status = status & (~IFE_CSID_VER1_RX_CPHY_EOT_RECEPTION);

	irq_status[CAM_IFE_CSID_IRQ_REG_RX] = status;

	if (!status)
		return IRQ_HANDLED;

	cam_io_w_mb(status,
		soc_info->reg_map[0].mem_base +
		csi2_reg->irq_clear_addr);

	if (csid_hw->flags.process_reset)
		return 0;

	if (status & BIT(csi2_reg->rst_done_shift_val)) {
		CAM_DBG(CAM_ISP, "CSID[%d] rx_reset done",
			csid_hw->hw_intf->hw_idx);
		complete(&csid_hw->irq_complete
			[CAM_IFE_CSID_IRQ_REG_RX]);
		return IRQ_HANDLED;
	}

	if (csid_hw->flags.fatal_err_detected) {
		CAM_DBG(CAM_ISP, "CSID[%u] already handling fatal error",
			csid_hw->hw_intf->hw_idx);
		return 0;
	}

	if (status & csi2_reg->fatal_err_mask) {
		csid_hw->flags.fatal_err_detected = true;
		cam_ife_csid_ver1_disable_csi2(csid_hw);
	}

	if (status & csi2_reg->part_fatal_err_mask) {
		if (status & IFE_CSID_VER1_RX_CPHY_EOT_RECEPTION)
			csid_hw->counters.error_irq_count++;

		if (status & IFE_CSID_VER1_RX_CPHY_SOT_RECEPTION)
			csid_hw->counters.error_irq_count++;

		if (status & IFE_CSID_VER1_RX_ERROR_CRC)
			csid_hw->counters.error_irq_count++;

		if (status & IFE_CSID_VER1_RX_UNBOUNDED_FRAME)
			csid_hw->counters.error_irq_count++;

		CAM_DBG(CAM_ISP, "CSID[%u] Recoverable Error Count:%u",
			csid_hw->hw_intf->hw_idx,
			csid_hw->counters.error_irq_count);

		if (csid_hw->counters.error_irq_count >
			CAM_IFE_CSID_MAX_ERR_COUNT) {
			csid_hw->flags.fatal_err_detected = true;
			cam_ife_csid_ver1_disable_csi2(csid_hw);
		}
	}

	debug_bits = status & csid_hw->debug_info.rx_mask;

	while (debug_bits) {

		if (debug_bits & 0x1)
			cam_ife_csid_ver1_handle_rx_debug_event(csid_hw,
				bit_pos);
		bit_pos++;
		debug_bits >>= 1;
	}

	*need_bh_sched = status & (csi2_reg->fatal_err_mask |
				csi2_reg->part_fatal_err_mask |
				csi2_reg->non_fatal_err_mask);

	return IRQ_HANDLED;
}

static int cam_ife_csid_ver1_path_top_half(
	uint32_t                             *irq_status,
	struct cam_ife_csid_ver1_hw          *csid_hw,
	uint32_t                             *need_bh,
	uint32_t                              index)
{
	struct cam_ife_csid_ver1_path_reg_info *path_reg = NULL;
	struct cam_ife_csid_ver1_reg_info      *csid_reg;
	struct cam_hw_soc_info                 *soc_info;
	const uint8_t                         **irq_reg_tag;
	uint32_t                                status = 0;
	uint32_t                                debug_bits;
	uint32_t                                bit_pos = 0;
	int                                     id = 0;
	uint32_t                                sof_irq_debug_en = 0;
	uint32_t                                val , val1;

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;

	switch (index) {
	case CAM_IFE_CSID_IRQ_REG_IPP:
		path_reg = csid_reg->ipp_reg;
		break;
	case CAM_IFE_CSID_IRQ_REG_PPP:
		path_reg = csid_reg->ppp_reg;
		break;
	case CAM_IFE_CSID_IRQ_REG_RDI_0:
	case CAM_IFE_CSID_IRQ_REG_RDI_1:
	case CAM_IFE_CSID_IRQ_REG_RDI_2:
	case CAM_IFE_CSID_IRQ_REG_RDI_3:
	case CAM_IFE_CSID_IRQ_REG_RDI_4:
		id = index - CAM_IFE_CSID_IRQ_REG_RDI_0;
		path_reg = csid_reg->rdi_reg[id];
		break;
	case CAM_IFE_CSID_IRQ_REG_UDI_0:
	case CAM_IFE_CSID_IRQ_REG_UDI_1:
	case CAM_IFE_CSID_IRQ_REG_UDI_2:
		id = index - CAM_IFE_CSID_IRQ_REG_UDI_0;
		path_reg = csid_reg->udi_reg[id];
		break;
	default:
		break;
	}

	if (!path_reg)
		return 0;

	soc_info = &csid_hw->hw_info->soc_info;
	status = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			path_reg->irq_status_addr);
	cam_io_w_mb(status,
		soc_info->reg_map[0].mem_base +
		path_reg->irq_clear_addr);

	irq_status[index] = status;

	if (csid_hw->flags.process_reset)
		return 0;

	if (status & BIT(csid_reg->cmn_reg->rst_done_shift_val)) {
		CAM_DBG(CAM_ISP, "irq_reg:%d  reset done", index);
		complete(&csid_hw->irq_complete[index]);
		return 0;
	}

	if (csid_hw->flags.sof_irq_triggered &&
		(status & IFE_CSID_VER1_PATH_INFO_INPUT_SOF)) {
		csid_hw->counters.irq_debug_cnt++;
	}

	if ((status & IFE_CSID_VER1_PATH_ERROR_PIX_COUNT ) ||
		(status & IFE_CSID_VER1_PATH_ERROR_LINE_COUNT)) {
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
				path_reg->format_measure0_addr);
		val1 = cam_io_r_mb(soc_info->reg_map[0].mem_base +
				path_reg->format_measure_cfg1_addr);
		CAM_ERR(CAM_ISP,
			"Expected:: h:  0x%x w: 0x%x actual:: h: 0x%x w: 0x%x [format_measure0: 0x%x]",
			((val1 >>
			csid_reg->cmn_reg->format_measure_height_shift_val) &
			csid_reg->cmn_reg->format_measure_height_mask_val),
			val1 &
			csid_reg->cmn_reg->format_measure_width_mask_val,
			((val >>
			csid_reg->cmn_reg->format_measure_height_shift_val) &
			csid_reg->cmn_reg->format_measure_height_mask_val),
			val &
			csid_reg->cmn_reg->format_measure_width_mask_val,
			val);
	}

	if (status & path_reg->fatal_err_mask)
		cam_io_w_mb(CAM_CSID_HALT_IMMEDIATELY,
			soc_info->reg_map[0].mem_base +
			path_reg->ctrl_addr);

	debug_bits = status & csid_hw->debug_info.path_mask;
	irq_reg_tag = cam_ife_csid_get_irq_reg_tag_ptr();

	while (debug_bits) {
		if ((debug_bits & 0x1))
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID[%d] IRQ %s %s ",
				csid_hw->hw_intf->hw_idx,
				irq_reg_tag[index],
				ver1_path_irq_desc[bit_pos]);

		bit_pos++;
		debug_bits >>= 1;
	}

	*need_bh = status & (path_reg->fatal_err_mask |
			path_reg->non_fatal_err_mask);

	if (csid_hw->counters.irq_debug_cnt >= CAM_CSID_IRQ_SOF_DEBUG_CNT_MAX) {
		cam_ife_csid_ver1_sof_irq_debug(csid_hw, &sof_irq_debug_en);
		csid_hw->counters.irq_debug_cnt = 0;
	}

	return IRQ_HANDLED;
}

static irqreturn_t cam_ife_csid_irq(int irq_num, void *data)
{
	struct cam_ife_csid_ver1_evt_payload  *evt_payload = NULL;
	struct cam_ife_csid_ver1_reg_info     *csid_reg;
	struct cam_ife_csid_ver1_hw           *csid_hw;
	struct cam_hw_soc_info                *soc_info;
	void                                  *bh_cmd = NULL;
	unsigned long                          flags;
	uint32_t                               status[CAM_IFE_CSID_IRQ_REG_MAX];
	uint32_t                               need_rx_bh = 0;
	uint32_t                               need_path_bh = 0;
	uint32_t                               need_bh_sched = 0;
	int                                    i;
	int                                    rc = 0;

	csid_hw = (struct cam_ife_csid_ver1_hw *)data;
	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			csid_hw->core_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	memset(status, 0, sizeof(status));

	spin_lock_irqsave(&csid_hw->hw_info->hw_lock, flags);

	status[CAM_IFE_CSID_IRQ_REG_TOP] =
		cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->top_irq_status_addr);
	cam_io_w_mb(status[CAM_IFE_CSID_IRQ_REG_TOP],
		soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->top_irq_clear_addr);

	for (i = CAM_IFE_CSID_IRQ_REG_RX; i < CAM_IFE_CSID_IRQ_REG_MAX; i++) {

		switch (i) {
		case CAM_IFE_CSID_IRQ_REG_RX:
			cam_ife_csid_ver1_rx_top_half(
				status, csid_hw, &need_rx_bh);
			break;
		case  CAM_IFE_CSID_IRQ_REG_IPP:
		case  CAM_IFE_CSID_IRQ_REG_PPP:
		case  CAM_IFE_CSID_IRQ_REG_RDI_0:
		case  CAM_IFE_CSID_IRQ_REG_RDI_1:
		case  CAM_IFE_CSID_IRQ_REG_RDI_2:
		case  CAM_IFE_CSID_IRQ_REG_RDI_3:
		case  CAM_IFE_CSID_IRQ_REG_RDI_4:
		case  CAM_IFE_CSID_IRQ_REG_UDI_0:
		case  CAM_IFE_CSID_IRQ_REG_UDI_1:
		case  CAM_IFE_CSID_IRQ_REG_UDI_2:
			cam_ife_csid_ver1_path_top_half(
				status, csid_hw, &need_path_bh, i);
			break;
		default:
			break;
		}
		need_bh_sched |= (need_rx_bh | need_path_bh);
	}

	if (status[CAM_IFE_CSID_IRQ_REG_TOP] & IFE_CSID_VER1_TOP_IRQ_DONE) {
		csid_hw->flags.process_reset = false;
		goto handle_top_reset;
	}

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->irq_cmd_addr);
	spin_unlock_irqrestore(
		&csid_hw->hw_info->hw_lock, flags);

	if (!need_bh_sched)
		return IRQ_HANDLED;

	rc = cam_ife_csid_ver1_get_evt_payload(csid_hw, &evt_payload,
		&csid_hw->free_payload_list);

	if (!evt_payload) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID[%u], no free tasklet",
			csid_hw->hw_intf->hw_idx);
		return IRQ_HANDLED;
	}


	rc = tasklet_bh_api.get_bh_payload_func(csid_hw->tasklet, &bh_cmd);

	if (rc || !bh_cmd) {
		cam_ife_csid_ver1_put_evt_payload(csid_hw, &evt_payload,
			&csid_hw->free_payload_list);
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"CSID[%d] Can not get cmd for tasklet, status %x",
			csid_hw->hw_intf->hw_idx,
			status[CAM_IFE_CSID_IRQ_REG_TOP]);
		return IRQ_HANDLED;
	}

	evt_payload->priv = csid_hw->token;
	evt_payload->hw_idx = csid_hw->hw_intf->hw_idx;

	for (i = 0; i < CAM_IFE_CSID_IRQ_REG_MAX; i++)
		evt_payload->irq_status[i] = status[i];

	tasklet_bh_api.bottom_half_enqueue_func(csid_hw->tasklet,
		bh_cmd,
		csid_hw,
		evt_payload,
		cam_ife_csid_ver1_bottom_half_handler);

	return IRQ_HANDLED;

handle_top_reset:
	CAM_DBG(CAM_ISP, "csid top reset complete");
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->irq_cmd_addr);
	spin_unlock_irqrestore(
		&csid_hw->hw_info->hw_lock, flags);
	complete(&csid_hw->irq_complete[CAM_IFE_CSID_IRQ_REG_TOP]);
	return IRQ_HANDLED;
}

static void cam_ife_csid_ver1_free_res(struct cam_ife_csid_ver1_hw *ife_csid_hw)
{
	struct cam_isp_resource_node *res;
	uint32_t num_paths;
	int i;
	struct cam_ife_csid_ver1_reg_info *csid_reg;

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			ife_csid_hw->core_info->csid_reg;
	num_paths = csid_reg->cmn_reg->num_udis;

	for (i = 0; i < num_paths; i++) {
		res = &ife_csid_hw->path_res[CAM_IFE_PIX_PATH_RES_UDI_0 + i];
		kfree(res->res_priv);
		res->res_priv = NULL;
	}

	num_paths = csid_reg->cmn_reg->num_rdis;

	for (i = 0; i < num_paths; i++) {
		res = &ife_csid_hw->path_res[CAM_IFE_PIX_PATH_RES_RDI_0 + i];
		kfree(res->res_priv);
		res->res_priv = NULL;
	}

	kfree(ife_csid_hw->path_res[CAM_IFE_PIX_PATH_RES_IPP].res_priv);
	ife_csid_hw->path_res[CAM_IFE_PIX_PATH_RES_IPP].res_priv = NULL;
	kfree(ife_csid_hw->path_res[CAM_IFE_PIX_PATH_RES_PPP].res_priv);
	ife_csid_hw->path_res[CAM_IFE_PIX_PATH_RES_PPP].res_priv = NULL;
}

static int cam_ife_ver1_hw_alloc_res(
	struct cam_isp_resource_node *res,
	uint32_t res_type,
	struct cam_hw_intf   *hw_intf,
	uint32_t res_id)

{
	struct cam_ife_csid_ver1_path_cfg *path_cfg = NULL;

	path_cfg = kzalloc(sizeof(*path_cfg), GFP_KERNEL);

	if (!path_cfg)
		return -ENOMEM;

	res->res_id = res_id;
	res->res_type = res_type;
	res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	res->hw_intf = hw_intf;
	res->res_priv = path_cfg;

	return 0;
}

static int cam_ife_csid_ver1_hw_init_path_res(
	struct cam_ife_csid_ver1_hw   *ife_csid_hw)
{
	int rc = 0;
	int i;
	struct cam_ife_csid_ver1_reg_info *csid_reg;
	struct cam_isp_resource_node *res;

	csid_reg = (struct cam_ife_csid_ver1_reg_info *)
			ife_csid_hw->core_info->csid_reg;

	/* Initialize the IPP resources */
	if (csid_reg->cmn_reg->num_pix) {
		res = &ife_csid_hw->path_res[CAM_IFE_PIX_PATH_RES_IPP];
		rc = cam_ife_ver1_hw_alloc_res(
			res,
			CAM_ISP_RESOURCE_PIX_PATH,
			ife_csid_hw->hw_intf,
			CAM_IFE_PIX_PATH_RES_IPP);
		if (rc) {
			CAM_ERR(CAM_ISP, "CSID: %d IPP res init fail",
				ife_csid_hw->hw_intf->hw_idx);
			goto free_res;
		}
		scnprintf(res->res_name, CAM_ISP_RES_NAME_LEN, "IPP");
	}

	/* Initialize PPP resource */
	if (csid_reg->cmn_reg->num_ppp) {
		res = &ife_csid_hw->path_res[CAM_IFE_PIX_PATH_RES_PPP];
		rc = cam_ife_ver1_hw_alloc_res(
			res,
			CAM_ISP_RESOURCE_PIX_PATH,
			ife_csid_hw->hw_intf,
			CAM_IFE_PIX_PATH_RES_PPP);
		if (rc) {
			CAM_ERR(CAM_ISP, "CSID: %d PPP res init fail",
				ife_csid_hw->hw_intf->hw_idx);
			goto free_res;
		}
		scnprintf(res->res_name,
				CAM_ISP_RES_NAME_LEN, "PPP");
	}

	/* Initialize the RDI resource */
	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++) {
		/* res type is from RDI 0 to RDI3 */
		res = &ife_csid_hw->path_res[CAM_IFE_PIX_PATH_RES_RDI_0 + i];
		rc = cam_ife_ver1_hw_alloc_res(
			res,
			CAM_ISP_RESOURCE_PIX_PATH,
			ife_csid_hw->hw_intf,
			CAM_IFE_PIX_PATH_RES_RDI_0 + i);
		if (rc) {
			CAM_ERR(CAM_ISP, "CSID: %d RDI[%d] res init fail",
				ife_csid_hw->hw_intf->hw_idx, i);
			goto free_res;
		}
		scnprintf(res->res_name, CAM_ISP_RES_NAME_LEN, "RDI_%d", i);
	}

	/* Initialize the UDI resource */
	for (i = 0; i < csid_reg->cmn_reg->num_udis; i++) {
		/* res type is from UDI0 to UDI3 */
		res = &ife_csid_hw->path_res[CAM_IFE_PIX_PATH_RES_UDI_0 + i];
		rc = cam_ife_ver1_hw_alloc_res(
			res,
			CAM_ISP_RESOURCE_PIX_PATH,
			ife_csid_hw->hw_intf,
			CAM_IFE_PIX_PATH_RES_UDI_0 + i);
		if (rc) {
			CAM_ERR(CAM_ISP, "CSID: %d UDI[%d] res init fail",
				ife_csid_hw->hw_intf->hw_idx, i);
			goto free_res;
		}
		scnprintf(res->res_name, CAM_ISP_RES_NAME_LEN, "UDI_%d", i);
	}

	return rc;

free_res:
	cam_ife_csid_ver1_free_res(ife_csid_hw);
	return rc;
}

int cam_ife_csid_hw_ver1_init(struct cam_hw_intf  *hw_intf,
	struct cam_ife_csid_core_info *csid_core_info,
	bool is_custom)
{
	int                             rc = -EINVAL;
	uint32_t                        i;
	struct cam_hw_info             *hw_info;
	struct cam_ife_csid_ver1_hw    *ife_csid_hw = NULL;

	if (!hw_intf || !csid_core_info) {
		CAM_ERR(CAM_ISP, "Invalid parameters intf: %pK hw_info: %pK",
			hw_intf, csid_core_info);
		return rc;
	}

	hw_info = (struct cam_hw_info  *)hw_intf->hw_priv;

	ife_csid_hw = kzalloc(sizeof(struct cam_ife_csid_ver1_hw), GFP_KERNEL);

	if (!ife_csid_hw) {
		CAM_ERR(CAM_ISP, "Csid core %d hw allocation fails",
			hw_intf->hw_idx);
		return -ENOMEM;
	}

	ife_csid_hw->hw_intf = hw_intf;
	ife_csid_hw->hw_info = hw_info;
	ife_csid_hw->core_info = csid_core_info;
	hw_info->core_info = ife_csid_hw;

	CAM_DBG(CAM_ISP, "type %d index %d",
		hw_intf->hw_type,
		hw_intf->hw_idx);

	ife_csid_hw->flags.device_enabled = false;
	ife_csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&ife_csid_hw->hw_info->hw_mutex);
	spin_lock_init(&ife_csid_hw->hw_info->hw_lock);
	spin_lock_init(&ife_csid_hw->lock_state);
	init_completion(&ife_csid_hw->hw_info->hw_complete);

	for (i = 0; i < CAM_IFE_CSID_IRQ_REG_MAX; i++)
		init_completion(&ife_csid_hw->irq_complete[i]);

	rc = cam_ife_csid_init_soc_resources(&ife_csid_hw->hw_info->soc_info,
			cam_ife_csid_irq, ife_csid_hw, is_custom);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "CSID:%d Failed to init_soc",
			hw_intf->hw_idx);
		return rc;
	}

	if (cam_cpas_is_feature_supported(CAM_CPAS_QCFA_BINNING_ENABLE,
		CAM_CPAS_HW_IDX_ANY, NULL))
		ife_csid_hw->flags.binning_enabled = true;

	ife_csid_hw->hw_intf->hw_ops.get_hw_caps =
						cam_ife_csid_ver1_get_hw_caps;
	ife_csid_hw->hw_intf->hw_ops.init        = cam_ife_csid_ver1_init_hw;
	ife_csid_hw->hw_intf->hw_ops.deinit      = cam_ife_csid_ver1_deinit_hw;
	ife_csid_hw->hw_intf->hw_ops.reset       = cam_ife_csid_ver1_reset;
	ife_csid_hw->hw_intf->hw_ops.reserve     = cam_ife_csid_ver1_reserve;
	ife_csid_hw->hw_intf->hw_ops.release     = cam_ife_csid_ver1_release;
	ife_csid_hw->hw_intf->hw_ops.start       = cam_ife_csid_ver1_start;
	ife_csid_hw->hw_intf->hw_ops.stop        = cam_ife_csid_ver1_stop;
	ife_csid_hw->hw_intf->hw_ops.read        = cam_ife_csid_ver1_read;
	ife_csid_hw->hw_intf->hw_ops.write       = cam_ife_csid_ver1_write;
	ife_csid_hw->hw_intf->hw_ops.process_cmd =
						cam_ife_csid_ver1_process_cmd;

	rc = cam_ife_csid_ver1_hw_init_path_res(ife_csid_hw);
	if (rc) {
		CAM_ERR(CAM_ISP, "CSID[%d] Probe Init failed",
			hw_intf->hw_idx);
		return rc;
	}

	rc  = cam_tasklet_init(&ife_csid_hw->tasklet, ife_csid_hw,
			hw_intf->hw_idx);
	if (rc) {
		CAM_ERR(CAM_ISP, "CSID[%d] init tasklet failed",
			hw_intf->hw_idx);
		goto err;
	}

	INIT_LIST_HEAD(&ife_csid_hw->free_payload_list);
	for (i = 0; i < CAM_IFE_CSID_VER1_EVT_PAYLOAD_MAX; i++) {
		INIT_LIST_HEAD(&ife_csid_hw->evt_payload[i].list);
		list_add_tail(&ife_csid_hw->evt_payload[i].list,
			&ife_csid_hw->free_payload_list);
	}

	ife_csid_hw->debug_info.debug_val = 0;
	ife_csid_hw->counters.error_irq_count = 0;

	return 0;
err:
	cam_ife_csid_ver1_free_res(ife_csid_hw);
	return rc;
}
EXPORT_SYMBOL(cam_ife_csid_hw_ver1_init);

int cam_ife_csid_hw_ver1_deinit(struct cam_hw_info *hw_priv)
{
	struct cam_ife_csid_ver1_hw   *csid_hw;
	int rc = -EINVAL;


	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid param");
		return rc;
	}

	csid_hw = (struct cam_ife_csid_ver1_hw *)hw_priv->core_info;

	/* release the privdate data memory from resources */
	cam_ife_csid_ver1_free_res(csid_hw);

	cam_ife_csid_deinit_soc_resources(&csid_hw->hw_info->soc_info);

	return 0;
}
EXPORT_SYMBOL(cam_ife_csid_hw_ver1_deinit);
