// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/iopoll.h>
#include <linux/slab.h>
#include <media/cam_tfe.h>
#include <media/cam_defs.h>
#include <media/cam_req_mgr.h>

#include "cam_tfe_csid_core.h"
#include "cam_csid_ppi_core.h"
#include "cam_isp_hw.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_subdev.h"
#include "cam_tasklet_util.h"

/* Timeout value in msec */
#define TFE_CSID_TIMEOUT                               1000

/* Timeout values in usec */
#define CAM_TFE_CSID_TIMEOUT_SLEEP_US                  1000
#define CAM_TFE_CSID_TIMEOUT_ALL_US                    100000

/*
 * Constant Factors needed to change QTimer ticks to nanoseconds
 * QTimer Freq = 19.2 MHz
 * Time(us) = ticks/19.2
 * Time(ns) = ticks/19.2 * 1000
 */
#define CAM_TFE_CSID_QTIMER_MUL_FACTOR                 10000
#define CAM_TFE_CSID_QTIMER_DIV_FACTOR                 192

/* Max number of sof irq's triggered in case of SOF freeze */
#define CAM_TFE_CSID_IRQ_SOF_DEBUG_CNT_MAX 12

/* Max CSI Rx irq error count threshold value */
#define CAM_TFE_CSID_MAX_IRQ_ERROR_COUNT               5

static int cam_tfe_csid_is_ipp_format_supported(
	uint32_t in_format)
{
	int rc = -EINVAL;

	switch (in_format) {
	case CAM_FORMAT_MIPI_RAW_6:
	case CAM_FORMAT_MIPI_RAW_8:
	case CAM_FORMAT_MIPI_RAW_10:
	case CAM_FORMAT_MIPI_RAW_12:
		rc = 0;
		break;
	default:
		break;
	}
	return rc;
}

static int cam_tfe_csid_get_format_rdi(
	uint32_t in_format, uint32_t out_format,
	uint32_t *decode_fmt, uint32_t *plain_fmt)
{
	int rc = 0;

	switch (in_format) {
	case CAM_FORMAT_MIPI_RAW_6:
		switch (out_format) {
		case CAM_FORMAT_MIPI_RAW_6:
			*decode_fmt = 0xf;
			break;
		case CAM_FORMAT_PLAIN8:
			*decode_fmt = 0x0;
			*plain_fmt = 0x0;
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_8:
		switch (out_format) {
		case CAM_FORMAT_MIPI_RAW_8:
		case CAM_FORMAT_PLAIN128:
			*decode_fmt = 0xf;
			break;
		case CAM_FORMAT_PLAIN8:
			*decode_fmt = 0x1;
			*plain_fmt = 0x0;
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_10:
		switch (out_format) {
		case CAM_FORMAT_MIPI_RAW_10:
		case CAM_FORMAT_PLAIN128:
			*decode_fmt = 0xf;
			break;
		case CAM_FORMAT_PLAIN16_10:
			*decode_fmt = 0x2;
			*plain_fmt = 0x1;
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_12:
		switch (out_format) {
		case CAM_FORMAT_MIPI_RAW_12:
			*decode_fmt = 0xf;
			break;
		case CAM_FORMAT_PLAIN16_12:
			*decode_fmt = 0x3;
			*plain_fmt = 0x1;
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_14:
		switch (out_format) {
		case CAM_FORMAT_MIPI_RAW_14:
			*decode_fmt = 0xf;
			break;
		case CAM_FORMAT_PLAIN16_14:
			*decode_fmt = 0x4;
			*plain_fmt = 0x1;
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_16:
		switch (out_format) {
		case CAM_FORMAT_MIPI_RAW_16:
			*decode_fmt = 0xf;
			break;
		case CAM_FORMAT_PLAIN16_16:
			*decode_fmt = 0x5;
			*plain_fmt = 0x1;
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}

	if (rc)
		CAM_ERR(CAM_ISP, "Unsupported format pair in %d out %d",
			in_format, out_format);

	return rc;
}

static int cam_tfe_csid_get_format_ipp(
	uint32_t in_format,
	uint32_t *decode_fmt, uint32_t *plain_fmt)
{
	int rc = 0;

	CAM_DBG(CAM_ISP, "input format:%d",
		 in_format);

	switch (in_format) {
	case CAM_FORMAT_MIPI_RAW_6:
		*decode_fmt  = 0;
		*plain_fmt = 0;
		break;
	case CAM_FORMAT_MIPI_RAW_8:
		*decode_fmt  = 0x1;
		*plain_fmt = 0;
		break;
	case CAM_FORMAT_MIPI_RAW_10:
		*decode_fmt  = 0x2;
		*plain_fmt = 0x1;
		break;
	case CAM_FORMAT_MIPI_RAW_12:
		*decode_fmt  = 0x3;
		*plain_fmt = 0x1;
		break;
	default:
		CAM_ERR(CAM_ISP, "Unsupported format %d",
			in_format);
		rc = -EINVAL;
	}

	CAM_DBG(CAM_ISP, "decode_fmt:%d plain_fmt:%d",
		 *decode_fmt, *plain_fmt);

	return rc;
}

static int cam_tfe_csid_cid_get(struct cam_tfe_csid_hw *csid_hw,
	int32_t vc, uint32_t dt, uint32_t *cid)
{
	uint32_t  i = 0;

	/* Return already reserved CID if the VC/DT matches */
	for (i = 0; i < CAM_TFE_CSID_CID_MAX; i++) {
		if (csid_hw->cid_res[i].cnt >= 1) {
			if (csid_hw->cid_res[i].vc == vc &&
				csid_hw->cid_res[i].dt == dt) {
				csid_hw->cid_res[i].cnt++;
				*cid = i;
				CAM_DBG(CAM_ISP, "CSID:%d CID %d allocated",
					csid_hw->hw_intf->hw_idx, i);
				return 0;
			}
		}
	}

	for (i = 0; i < CAM_TFE_CSID_CID_MAX; i++) {
		if (!csid_hw->cid_res[i].cnt) {
			csid_hw->cid_res[i].vc  = vc;
			csid_hw->cid_res[i].dt  = dt;
			csid_hw->cid_res[i].cnt = 1;
			*cid = i;
			CAM_DBG(CAM_ISP, "CSID:%d CID %d allocated",
				csid_hw->hw_intf->hw_idx, i);
			return 0;
		}
	}

	CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d Free cid is not available",
		 csid_hw->hw_intf->hw_idx);
	/* Dump CID values */
	for (i = 0; i < CAM_TFE_CSID_CID_MAX; i++) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d CID:%d vc:%d dt:%d cnt:%d",
			csid_hw->hw_intf->hw_idx, i, csid_hw->cid_res[i].vc,
			csid_hw->cid_res[i].dt, csid_hw->cid_res[i].cnt);
	}
	return -EINVAL;
}

static int cam_tfe_csid_global_reset(struct cam_tfe_csid_hw *csid_hw)
{
	struct cam_hw_soc_info                *soc_info;
	const struct cam_tfe_csid_reg_offset  *csid_reg;
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

	CAM_DBG(CAM_ISP, "CSID:%d Csid reset", csid_hw->hw_intf->hw_idx);

	/* Mask all interrupts */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_mask_addr);

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_mask_addr);

	if (csid_hw->pxl_pipe_enable)
		cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_irq_mask_addr);

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++)
		cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_irq_mask_addr);

	/* clear all interrupts */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_clear_addr);

	cam_io_w_mb(csid_reg->csi2_reg->csi2_irq_mask_all,
		soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_clear_addr);

	if (csid_hw->pxl_pipe_enable)
		cam_io_w_mb(csid_reg->cmn_reg->ipp_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_irq_clear_addr);

	for (i = 0 ; i < csid_reg->cmn_reg->num_rdis; i++)
		cam_io_w_mb(csid_reg->cmn_reg->rdi_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_irq_clear_addr);

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_irq_cmd_addr);

	cam_io_w_mb(0x80, soc_info->reg_map[0].mem_base +
		csid_hw->csid_info->csid_reg->csi2_reg->csid_csi2_rx_cfg1_addr);

	/* perform the top CSID HW registers reset */
	cam_io_w_mb(csid_reg->cmn_reg->csid_rst_stb,
		soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_rst_strobes_addr);

	rc = readl_poll_timeout(soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_status_addr,
			status, (status & 0x1) == 0x1,
		CAM_TFE_CSID_TIMEOUT_SLEEP_US, CAM_TFE_CSID_TIMEOUT_ALL_US);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "CSID:%d csid_reset fail rc = %d",
			  csid_hw->hw_intf->hw_idx, rc);
		rc = -ETIMEDOUT;
	}

	status = cam_io_r(soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_status_addr);
	CAM_DBG(CAM_ISP, "Status reg %d", status);

	/* perform the SW registers reset */
	reinit_completion(&csid_hw->csid_top_complete);
	cam_io_w_mb(csid_reg->cmn_reg->csid_reg_rst_stb,
		soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_rst_strobes_addr);

	rc = wait_for_completion_timeout(&csid_hw->csid_top_complete,
		msecs_to_jiffies(TFE_CSID_TIMEOUT));
	if (rc <= 0) {
		CAM_ERR(CAM_ISP, "CSID:%d soft reg reset fail rc = %d",
			 csid_hw->hw_intf->hw_idx, rc);
		if (rc == 0)
			rc = -ETIMEDOUT;
	} else
		rc = 0;

	usleep_range(3000, 3010);
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_mask_addr);
	if (val != 0)
		CAM_ERR(CAM_ISP, "CSID:%d IRQ value after reset rc = %d",
			csid_hw->hw_intf->hw_idx, val);
	csid_hw->error_irq_count = 0;
	csid_hw->prev_boot_timestamp = 0;

	return rc;
}

static int cam_tfe_csid_path_reset(struct cam_tfe_csid_hw *csid_hw,
	struct cam_tfe_csid_reset_cfg_args  *reset)
{
	int rc = 0;
	struct cam_hw_soc_info                    *soc_info;
	struct cam_isp_resource_node              *res;
	const struct cam_tfe_csid_reg_offset      *csid_reg;
	uint32_t  reset_strb_addr, reset_strb_val, val, id;
	struct completion  *complete;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	res      = reset->node_res;

	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid hw state :%d",
			csid_hw->hw_intf->hw_idx,
			csid_hw->hw_info->hw_state);
		return -EINVAL;
	}

	if (res->res_id >= CAM_TFE_CSID_PATH_RES_MAX) {
		CAM_DBG(CAM_ISP, "CSID:%d Invalid res id%d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ISP, "CSID:%d resource:%d",
		csid_hw->hw_intf->hw_idx, res->res_id);

	if (res->res_id == CAM_TFE_CSID_PATH_RES_IPP) {
		if (!csid_reg->ipp_reg) {
			CAM_ERR(CAM_ISP, "CSID:%d IPP not supported :%d",
				 csid_hw->hw_intf->hw_idx,
				res->res_id);
			return -EINVAL;
		}

		reset_strb_addr = csid_reg->ipp_reg->csid_pxl_rst_strobes_addr;
		complete = &csid_hw->csid_ipp_complete;
		reset_strb_val = csid_reg->cmn_reg->ipp_path_rst_stb_all;

		/* Enable path reset done interrupt */
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_irq_mask_addr);
		val |= TFE_CSID_PATH_INFO_RST_DONE;
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			 csid_reg->ipp_reg->csid_pxl_irq_mask_addr);
	} else {
		id = res->res_id;
		if (!csid_reg->rdi_reg[id]) {
			CAM_ERR(CAM_ISP, "CSID:%d RDI res not supported :%d",
				 csid_hw->hw_intf->hw_idx,
				res->res_id);
			return -EINVAL;
		}

		reset_strb_addr =
			csid_reg->rdi_reg[id]->csid_rdi_rst_strobes_addr;
		complete =
			&csid_hw->csid_rdin_complete[id];
		reset_strb_val = csid_reg->cmn_reg->rdi_path_rst_stb_all;

		/* Enable path reset done interrupt */
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_irq_mask_addr);
		val |= TFE_CSID_PATH_INFO_RST_DONE;
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_irq_mask_addr);
	}

	reinit_completion(complete);

	/* Reset the corresponding tfe csid path */
	cam_io_w_mb(reset_strb_val, soc_info->reg_map[0].mem_base +
				reset_strb_addr);

	rc = wait_for_completion_timeout(complete,
		msecs_to_jiffies(TFE_CSID_TIMEOUT));
	if (rc <= 0) {
		CAM_ERR(CAM_ISP, "CSID:%d Res id %d fail rc = %d",
			 csid_hw->hw_intf->hw_idx,
			res->res_id,  rc);
		if (rc == 0)
			rc = -ETIMEDOUT;
	}

end:
	return rc;
}

static int cam_tfe_csid_cid_reserve(struct cam_tfe_csid_hw *csid_hw,
	struct cam_tfe_csid_hw_reserve_resource_args  *cid_reserv,
	uint32_t  *cid_value)
{
	int rc = 0;

	CAM_DBG(CAM_ISP,
		"CSID:%d res_id:0x%x Lane type:%d lane_num:%d dt:%d vc:%d",
		csid_hw->hw_intf->hw_idx,
		cid_reserv->in_port->res_id,
		cid_reserv->in_port->lane_type,
		cid_reserv->in_port->lane_num,
		cid_reserv->in_port->dt,
		cid_reserv->in_port->vc);

	if (cid_reserv->in_port->res_id >= CAM_ISP_TFE_IN_RES_MAX) {
		CAM_ERR(CAM_ISP, "CSID:%d  Invalid phy sel %d",
			csid_hw->hw_intf->hw_idx,
			cid_reserv->in_port->res_id);
		rc = -EINVAL;
		goto end;
	}

	if (cid_reserv->in_port->lane_type >= CAM_ISP_LANE_TYPE_MAX) {
		CAM_ERR(CAM_ISP, "CSID:%d  Invalid lane type %d",
			csid_hw->hw_intf->hw_idx,
			cid_reserv->in_port->lane_type);
		rc = -EINVAL;
		goto end;
	}

	if ((cid_reserv->in_port->lane_type ==  CAM_ISP_LANE_TYPE_DPHY &&
		cid_reserv->in_port->lane_num > 4)) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid lane num %d",
			csid_hw->hw_intf->hw_idx,
			cid_reserv->in_port->lane_num);
		rc = -EINVAL;
		goto end;
	}

	if (cid_reserv->in_port->lane_type == CAM_ISP_LANE_TYPE_CPHY &&
		cid_reserv->in_port->lane_num > 3) {
		CAM_ERR(CAM_ISP, " CSID:%d Invalid lane type %d & num %d",
			 csid_hw->hw_intf->hw_idx,
			cid_reserv->in_port->lane_type,
			cid_reserv->in_port->lane_num);
		rc = -EINVAL;
		goto end;
	}

	/* CSID  CSI2 v1.1 supports 4 vc  */
	if (cid_reserv->in_port->dt > 0x3f ||
		cid_reserv->in_port->vc > 0x3) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid vc:%d dt %d",
			csid_hw->hw_intf->hw_idx,
			cid_reserv->in_port->vc, cid_reserv->in_port->dt);
		rc = -EINVAL;
		goto end;
	}

	if (csid_hw->csi2_reserve_cnt == UINT_MAX) {
		CAM_ERR(CAM_ISP,
			"CSID%d reserve cnt reached max",
			csid_hw->hw_intf->hw_idx);
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ISP, "Reserve_cnt %u", csid_hw->csi2_reserve_cnt);

	if (csid_hw->csi2_reserve_cnt) {
		/* current configure res type should match requested res type */
		if (csid_hw->in_res_id != cid_reserv->in_port->res_id) {
			rc = -EINVAL;
			goto end;
		}

		if (csid_hw->csi2_rx_cfg.lane_cfg !=
			cid_reserv->in_port->lane_cfg  ||
			csid_hw->csi2_rx_cfg.lane_type !=
			cid_reserv->in_port->lane_type ||
			csid_hw->csi2_rx_cfg.lane_num !=
			cid_reserv->in_port->lane_num) {
			rc = -EINVAL;
			goto end;
		}
	}

	rc = cam_tfe_csid_cid_get(csid_hw,
		cid_reserv->in_port->vc,
		cid_reserv->in_port->dt,
		cid_value);
	if (rc) {
		CAM_ERR(CAM_ISP, "CSID:%d CID Reserve failed res_id %d",
			csid_hw->hw_intf->hw_idx,
			cid_reserv->in_port->res_id);
		goto end;
	}

	if (!csid_hw->csi2_reserve_cnt) {
		csid_hw->in_res_id = cid_reserv->in_port->res_id;

		csid_hw->csi2_rx_cfg.lane_cfg =
			cid_reserv->in_port->lane_cfg;
		csid_hw->csi2_rx_cfg.lane_type =
			cid_reserv->in_port->lane_type;
		csid_hw->csi2_rx_cfg.lane_num =
			cid_reserv->in_port->lane_num;

		if (cid_reserv->in_port->res_id != CAM_ISP_TFE_IN_RES_TPG)
			csid_hw->csi2_rx_cfg.phy_sel =
				(cid_reserv->in_port->res_id & 0xFF) - 1;
		else
			csid_hw->csi2_rx_cfg.phy_sel =
				(cid_reserv->phy_sel & 0xFF) - 1;
	}

	csid_hw->csi2_reserve_cnt++;
	CAM_DBG(CAM_ISP, "CSID:%d CID:%d acquired reserv cnt:%d",
		csid_hw->hw_intf->hw_idx, *cid_value,
		csid_hw->csi2_reserve_cnt);

end:
	return rc;
}

static int cam_tfe_csid_path_reserve(struct cam_tfe_csid_hw *csid_hw,
	struct cam_tfe_csid_hw_reserve_resource_args  *reserve)
{
	int rc = 0;
	struct cam_tfe_csid_path_cfg    *path_data;
	struct cam_isp_resource_node    *res;
	uint32_t          cid_value;

	/* CSID  CSI2 v2.0 supports 4 vc */
	if (reserve->in_port->dt > 0x3f || reserve->in_port->vc > 0x3 ||
		(reserve->sync_mode >= CAM_ISP_HW_SYNC_MAX)) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid vc:%d dt %d mode:%d",
			 csid_hw->hw_intf->hw_idx,
			reserve->in_port->vc, reserve->in_port->dt,
			reserve->sync_mode);
		rc = -EINVAL;
		goto end;
	}

	switch (reserve->res_id) {
	case CAM_TFE_CSID_PATH_RES_IPP:
		if (csid_hw->ipp_res.res_state !=
			CAM_ISP_RESOURCE_STATE_AVAILABLE) {
			CAM_DBG(CAM_ISP,
				"CSID:%d IPP resource not available %d",
				csid_hw->hw_intf->hw_idx,
				csid_hw->ipp_res.res_state);
			rc = -EINVAL;
			goto end;
		}

		if (cam_tfe_csid_is_ipp_format_supported(
				reserve->in_port->format)) {
			CAM_ERR(CAM_ISP,
				"CSID:%d res id:%d un support format %d",
				csid_hw->hw_intf->hw_idx, reserve->res_id,
				reserve->in_port->format);
			rc = -EINVAL;
			goto end;
		}
		rc = cam_tfe_csid_cid_reserve(csid_hw, reserve, &cid_value);
		if (rc)
			goto end;

		/* assign the IPP resource */
		res = &csid_hw->ipp_res;
		CAM_DBG(CAM_ISP,
			"CSID:%d IPP resource:%d acquired successfully",
			csid_hw->hw_intf->hw_idx, res->res_id);

		break;

	case CAM_TFE_CSID_PATH_RES_RDI_0:
	case CAM_TFE_CSID_PATH_RES_RDI_1:
	case CAM_TFE_CSID_PATH_RES_RDI_2:
		if (csid_hw->rdi_res[reserve->res_id].res_state !=
			CAM_ISP_RESOURCE_STATE_AVAILABLE) {
			CAM_ERR(CAM_ISP,
				"CSID:%d RDI:%d resource not available %d",
				csid_hw->hw_intf->hw_idx,
				reserve->res_id,
				csid_hw->rdi_res[reserve->res_id].res_state);
			rc = -EINVAL;
			goto end;
		}

		rc = cam_tfe_csid_cid_reserve(csid_hw, reserve, &cid_value);
		if (rc)
			goto end;

		res = &csid_hw->rdi_res[reserve->res_id];
		CAM_DBG(CAM_ISP,
			"CSID:%d RDI resource:%d acquire success",
			csid_hw->hw_intf->hw_idx,
			res->res_id);

		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res id:%d",
			csid_hw->hw_intf->hw_idx, reserve->res_id);
		rc = -EINVAL;
		goto end;
	}

	res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	path_data = (struct cam_tfe_csid_path_cfg   *)res->res_priv;

	CAM_DBG(CAM_ISP, "sensor width:%d height:%d fps:%d vbi:%d hbi:%d",
		reserve->in_port->sensor_width,
		reserve->in_port->sensor_height,
		reserve->in_port->sensor_fps,
		reserve->in_port->sensor_vbi,
		reserve->in_port->sensor_hbi);
	path_data->sensor_width = reserve->in_port->sensor_width;
	path_data->sensor_height = reserve->in_port->sensor_height;
	path_data->sensor_fps  = reserve->in_port->sensor_fps;
	path_data->sensor_hbi = reserve->in_port->sensor_vbi;
	path_data->sensor_vbi = reserve->in_port->sensor_hbi;

	path_data->cid = cid_value;
	path_data->in_format = reserve->in_port->format;
	path_data->out_format = reserve->out_port->format;
	path_data->sync_mode = reserve->sync_mode;
	path_data->height  = reserve->in_port->height;
	path_data->start_line = reserve->in_port->line_start;
	path_data->end_line = reserve->in_port->line_end;

	csid_hw->event_cb = reserve->event_cb;
	csid_hw->event_cb_priv = reserve->event_cb_prv;

	/* Enable crop only for ipp */
	if (reserve->res_id == CAM_TFE_CSID_PATH_RES_IPP)
		path_data->crop_enable = true;

	CAM_DBG(CAM_ISP,
		"Res id: %d height:%d line_start %d line_end %d crop_en %d",
		reserve->res_id, reserve->in_port->height,
		reserve->in_port->line_start, reserve->in_port->line_end,
		path_data->crop_enable);

	path_data->dt = reserve->in_port->dt;
	path_data->vc = reserve->in_port->vc;

	if (reserve->sync_mode == CAM_ISP_HW_SYNC_MASTER) {
		path_data->start_pixel = reserve->in_port->left_start;
		path_data->end_pixel = reserve->in_port->left_end;
		path_data->width  = reserve->in_port->left_width;
		CAM_DBG(CAM_ISP, "CSID:%d master:startpixel 0x%x endpixel:0x%x",
			csid_hw->hw_intf->hw_idx, path_data->start_pixel,
			path_data->end_pixel);
		CAM_DBG(CAM_ISP, "CSID:%d master:line start:0x%x line end:0x%x",
			csid_hw->hw_intf->hw_idx, path_data->start_line,
			path_data->end_line);
	} else if (reserve->sync_mode == CAM_ISP_HW_SYNC_SLAVE) {
		path_data->master_idx = reserve->master_idx;
		CAM_DBG(CAM_ISP, "CSID:%d master_idx=%d",
			csid_hw->hw_intf->hw_idx, path_data->master_idx);
		path_data->start_pixel = reserve->in_port->right_start;
		path_data->end_pixel = reserve->in_port->right_end;
		path_data->width  = reserve->in_port->right_width;
		CAM_DBG(CAM_ISP, "CSID:%d slave:start:0x%x end:0x%x width 0x%x",
			csid_hw->hw_intf->hw_idx, path_data->start_pixel,
			path_data->end_pixel, path_data->width);
		CAM_DBG(CAM_ISP, "CSID:%d slave:line start:0x%x line end:0x%x",
			csid_hw->hw_intf->hw_idx, path_data->start_line,
			path_data->end_line);
	} else {
		path_data->width  = reserve->in_port->left_width;
		path_data->start_pixel = reserve->in_port->left_start;
		path_data->end_pixel = reserve->in_port->left_end;
		CAM_DBG(CAM_ISP, "Res id: %d left width %d start: %d stop:%d",
			reserve->res_id, reserve->in_port->left_width,
			reserve->in_port->left_start,
			reserve->in_port->left_end);
	}

	CAM_DBG(CAM_ISP, "Res %d width %d height %d", reserve->res_id,
		path_data->width, path_data->height);
	reserve->node_res = res;

end:
	return rc;
}

static int cam_tfe_csid_enable_csi2(
	struct cam_tfe_csid_hw          *csid_hw)
{
	const struct cam_tfe_csid_reg_offset       *csid_reg;
	struct cam_hw_soc_info                     *soc_info;
	struct cam_csid_ppi_cfg                     ppi_lane_cfg;
	uint32_t val = 0;
	uint32_t ppi_index = 0, rc;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	CAM_DBG(CAM_ISP, "CSID:%d config csi2 rx",
		csid_hw->hw_intf->hw_idx);

	/* rx cfg0 */
	val = 0;
	val = (csid_hw->csi2_rx_cfg.lane_num - 1)  |
		(csid_hw->csi2_rx_cfg.lane_cfg << 4) |
		(csid_hw->csi2_rx_cfg.lane_type << 24);
	val |= (csid_hw->csi2_rx_cfg.phy_sel &
		csid_reg->csi2_reg->csi2_rx_phy_num_mask) << 20;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_cfg0_addr);

	/* rx cfg1 */
	val = (1 << csid_reg->csi2_reg->csi2_misr_enable_shift_val);

	/* enable packet ecc correction */
	val |= 1;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_cfg1_addr);

	/* Enable the CSI2 rx inerrupts */
	val = TFE_CSID_CSI2_RX_INFO_RST_DONE |
		TFE_CSID_CSI2_RX_ERROR_LANE0_FIFO_OVERFLOW |
		TFE_CSID_CSI2_RX_ERROR_LANE1_FIFO_OVERFLOW |
		TFE_CSID_CSI2_RX_ERROR_LANE2_FIFO_OVERFLOW |
		TFE_CSID_CSI2_RX_ERROR_LANE3_FIFO_OVERFLOW |
		TFE_CSID_CSI2_RX_ERROR_CPHY_EOT_RECEPTION |
		TFE_CSID_CSI2_RX_ERROR_CPHY_SOT_RECEPTION |
		TFE_CSID_CSI2_RX_ERROR_CRC |
		TFE_CSID_CSI2_RX_ERROR_ECC |
		TFE_CSID_CSI2_RX_ERROR_MMAPPED_VC_DT |
		TFE_CSID_CSI2_RX_ERROR_STREAM_UNDERFLOW |
		TFE_CSID_CSI2_RX_ERROR_UNBOUNDED_FRAME |
		TFE_CSID_CSI2_RX_ERROR_CPHY_PH_CRC;

	/* Enable the interrupt based on csid debug info set */
	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_SOT_IRQ)
		val |= TFE_CSID_CSI2_RX_INFO_PHY_DL0_SOT_CAPTURED |
			TFE_CSID_CSI2_RX_INFO_PHY_DL1_SOT_CAPTURED |
			TFE_CSID_CSI2_RX_INFO_PHY_DL2_SOT_CAPTURED |
			TFE_CSID_CSI2_RX_INFO_PHY_DL3_SOT_CAPTURED;

	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_EOT_IRQ)
		val |= TFE_CSID_CSI2_RX_INFO_PHY_DL0_EOT_CAPTURED |
			TFE_CSID_CSI2_RX_INFO_PHY_DL1_EOT_CAPTURED |
			TFE_CSID_CSI2_RX_INFO_PHY_DL2_EOT_CAPTURED |
			TFE_CSID_CSI2_RX_INFO_PHY_DL3_EOT_CAPTURED;

	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE)
		val |= TFE_CSID_CSI2_RX_INFO_SHORT_PKT_CAPTURED;

	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE)
		val |= TFE_CSID_CSI2_RX_INFO_LONG_PKT_CAPTURED;
	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE)
		val |= TFE_CSID_CSI2_RX_INFO_CPHY_PKT_HDR_CAPTURED;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_mask_addr);

	ppi_index = csid_hw->csi2_rx_cfg.phy_sel;
	if (csid_hw->ppi_hw_intf[ppi_index] && csid_hw->ppi_enable) {
		ppi_lane_cfg.lane_type = csid_hw->csi2_rx_cfg.lane_type;
		ppi_lane_cfg.lane_num  = csid_hw->csi2_rx_cfg.lane_num;
		ppi_lane_cfg.lane_cfg  = csid_hw->csi2_rx_cfg.lane_cfg;

		CAM_DBG(CAM_ISP, "ppi_index to init %d", ppi_index);
		rc = csid_hw->ppi_hw_intf[ppi_index]->hw_ops.init(
				csid_hw->ppi_hw_intf[ppi_index]->hw_priv,
				&ppi_lane_cfg,
				sizeof(struct cam_csid_ppi_cfg));
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "PPI:%d Init Failed",
					ppi_index);
			return rc;
		}
	}


	return 0;
}

static int cam_tfe_csid_disable_csi2(
	struct cam_tfe_csid_hw          *csid_hw)
{
	const struct cam_tfe_csid_reg_offset      *csid_reg;
	struct cam_hw_soc_info                    *soc_info;
	uint32_t ppi_index = 0, rc;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	CAM_DBG(CAM_ISP, "CSID:%d Disable csi2 rx",
		csid_hw->hw_intf->hw_idx);

	/* Disable the CSI2 rx inerrupts */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_mask_addr);

	/* Reset the Rx CFG registers */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_cfg0_addr);
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_cfg1_addr);

	ppi_index = csid_hw->csi2_rx_cfg.phy_sel;
	if (csid_hw->ppi_hw_intf[ppi_index] && csid_hw->ppi_enable) {
		/* De-Initialize the PPI bridge */
		CAM_DBG(CAM_ISP, "ppi_index to de-init %d\n", ppi_index);
		rc = csid_hw->ppi_hw_intf[ppi_index]->hw_ops.deinit(
				csid_hw->ppi_hw_intf[ppi_index]->hw_priv,
				NULL, 0);
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "PPI:%d De-Init Failed", ppi_index);
			return rc;
		}
	}

	return 0;
}

static int cam_tfe_csid_enable_hw(struct cam_tfe_csid_hw  *csid_hw)
{
	int rc = 0;
	const struct cam_tfe_csid_reg_offset      *csid_reg;
	struct cam_hw_soc_info              *soc_info;
	uint32_t i, val, clk_lvl;
	unsigned long flags;

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

	rc = cam_soc_util_get_clk_level(soc_info, csid_hw->clk_rate,
		soc_info->src_clk_idx, &clk_lvl);
	CAM_DBG(CAM_ISP, "CSID clock lvl %u", clk_lvl);

	rc = cam_tfe_csid_enable_soc_resources(soc_info, clk_lvl);
	if (rc) {
		CAM_ERR(CAM_ISP, "CSID:%d Enable SOC failed",
			csid_hw->hw_intf->hw_idx);
		goto err;
	}

	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_UP;
	/* Disable the top IRQ interrupt */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_mask_addr);
	/* Reset CSID top */
	rc = cam_tfe_csid_global_reset(csid_hw);
	if (rc)
		goto disable_soc;

	/* clear all interrupts */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_clear_addr);

	cam_io_w_mb(csid_reg->csi2_reg->csi2_irq_mask_all,
		soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_clear_addr);

	if (csid_hw->pxl_pipe_enable)
		cam_io_w_mb(csid_reg->cmn_reg->ipp_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_irq_clear_addr);

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

	/* enable the csi2 rx */
	rc = cam_tfe_csid_enable_csi2(csid_hw);
	if (rc)
		goto disable_soc;

	spin_lock_irqsave(&csid_hw->spin_lock, flags);
	csid_hw->fatal_err_detected = false;
	csid_hw->device_enabled = 1;
	spin_unlock_irqrestore(&csid_hw->spin_lock, flags);
	cam_tasklet_start(csid_hw->tasklet);

	return rc;

disable_soc:
	cam_tfe_csid_disable_soc_resources(soc_info);
	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
err:
	csid_hw->hw_info->open_count--;
	return rc;
}

static int cam_tfe_csid_disable_hw(struct cam_tfe_csid_hw *csid_hw)
{
	int rc = -EINVAL;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_tfe_csid_reg_offset     *csid_reg;
	unsigned long                             flags;

	/* Check for refcount */
	if (!csid_hw->hw_info->open_count) {
		CAM_WARN(CAM_ISP, "Unbalanced disable_hw");
		return rc;
	}

	/* Decrement ref Count */
	csid_hw->hw_info->open_count--;

	if (csid_hw->hw_info->open_count) {
		rc = 0;
		return rc;
	}

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = csid_hw->csid_info->csid_reg;

	/* Disable the csi2 */
	cam_tfe_csid_disable_csi2(csid_hw);

	CAM_DBG(CAM_ISP, "%s:Calling Global Reset", __func__);
	cam_tfe_csid_global_reset(csid_hw);
	CAM_DBG(CAM_ISP, "%s:Global Reset Done", __func__);

	CAM_DBG(CAM_ISP, "CSID:%d De-init CSID HW",
		csid_hw->hw_intf->hw_idx);

	/* Disable the top IRQ interrupt */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_mask_addr);

	rc = cam_tfe_csid_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_ISP, "CSID:%d Disable CSID SOC failed",
			csid_hw->hw_intf->hw_idx);

	cam_tasklet_stop(csid_hw->tasklet);
	spin_lock_irqsave(&csid_hw->spin_lock, flags);
	csid_hw->device_enabled = 0;
	spin_unlock_irqrestore(&csid_hw->spin_lock, flags);
	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	csid_hw->error_irq_count = 0;
	csid_hw->prev_boot_timestamp = 0;

	return rc;
}

static int cam_tfe_csid_init_config_pxl_path(
	struct cam_tfe_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	struct cam_tfe_csid_path_cfg             *path_data;
	const struct cam_tfe_csid_reg_offset     *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_tfe_csid_pxl_reg_offset *pxl_reg = NULL;
	uint32_t decode_format = 0, plain_format = 0, val = 0;

	path_data = (struct cam_tfe_csid_path_cfg  *) res->res_priv;
	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	pxl_reg = csid_reg->ipp_reg;
	if (!pxl_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d IPP :%d is not supported on HW",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "Config IPP Path");
	rc = cam_tfe_csid_get_format_ipp(path_data->in_format,
		&decode_format, &plain_format);
	if (rc)
		return rc;

	/*
	 * configure Pxl path and enable the time stamp capture.
	 * enable the HW measrurement blocks
	 */
	val = (path_data->vc << csid_reg->cmn_reg->vc_shift_val) |
		(path_data->dt << csid_reg->cmn_reg->dt_shift_val) |
		(path_data->cid << csid_reg->cmn_reg->dt_id_shift_val) |
		(decode_format << csid_reg->cmn_reg->fmt_shift_val) |
		(path_data->crop_enable <<
		csid_reg->cmn_reg->crop_h_en_shift_val) |
		(path_data->crop_enable <<
		csid_reg->cmn_reg->crop_v_en_shift_val) |
		(1 << 1);

	val |= (1 << pxl_reg->pix_store_en_shift_val);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		pxl_reg->csid_pxl_cfg0_addr);

	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		pxl_reg->csid_pxl_cfg1_addr);

	/* select the post irq sub sample strobe for time stamp capture */
	val |= TFE_CSID_TIMESTAMP_STB_POST_IRQ;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		pxl_reg->csid_pxl_cfg1_addr);

	if (path_data->crop_enable) {
		val = (((path_data->end_pixel & 0xFFFF) <<
			csid_reg->cmn_reg->crop_shift) |
			(path_data->start_pixel & 0xFFFF));
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			pxl_reg->csid_pxl_hcrop_addr);
		CAM_DBG(CAM_ISP, "CSID:%d Horizontal crop config val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);

		val = (((path_data->end_line & 0xFFFF) <<
			csid_reg->cmn_reg->crop_shift) |
			(path_data->start_line & 0xFFFF));
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			pxl_reg->csid_pxl_vcrop_addr);
		CAM_DBG(CAM_ISP, "CSID:%d Vertical Crop config val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);

		/* Enable generating early eof strobe based on crop config */
		if (!(csid_hw->csid_debug & TFE_CSID_DEBUG_DISABLE_EARLY_EOF)) {
			val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
				pxl_reg->csid_pxl_cfg0_addr);
			val |= (1 << pxl_reg->early_eof_en_shift_val);
			cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
				pxl_reg->csid_pxl_cfg0_addr);
		}
	}

	/* Enable the Pxl path */
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		pxl_reg->csid_pxl_cfg0_addr);
	val |= (1 << csid_reg->cmn_reg->path_en_shift_val);

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		pxl_reg->csid_pxl_cfg0_addr);

	/* Enable Error Detection Overflow ctrl mode: 2 -> Detect overflow */
	val = 0x9;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		pxl_reg->csid_pxl_err_recovery_cfg0_addr);

	/* configure the rx packet capture based on csid debug set */
	val = 0;
	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE)
		val = ((1 <<
			csid_reg->csi2_reg->csi2_capture_short_pkt_en_shift) |
			(path_data->vc <<
			csid_reg->csi2_reg->csi2_capture_short_pkt_vc_shift));

	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE)
		val |= ((1 <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_en_shift) |
			(path_data->dt <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_dt_shift) |
			(path_data->vc <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_vc_shift));

	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE)
		val |= ((1 <<
			csid_reg->csi2_reg->csi2_capture_cphy_pkt_en_shift) |
			(path_data->dt <<
			csid_reg->csi2_reg->csi2_capture_cphy_pkt_dt_shift) |
			(path_data->vc <<
			csid_reg->csi2_reg->csi2_capture_cphy_pkt_vc_shift));

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_capture_ctrl_addr);
	CAM_DBG(CAM_ISP, "rx capture control value 0x%x", val);

	res->res_state = CAM_ISP_RESOURCE_STATE_INIT_HW;

	return rc;
}

static int cam_tfe_csid_deinit_pxl_path(
	struct cam_tfe_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	const struct cam_tfe_csid_reg_offset      *csid_reg;
	struct cam_hw_soc_info                    *soc_info;
	const struct cam_tfe_csid_pxl_reg_offset  *pxl_reg = NULL;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	pxl_reg = csid_reg->ipp_reg;
	if (res->res_state != CAM_ISP_RESOURCE_STATE_INIT_HW) {
		CAM_ERR(CAM_ISP,
			"CSID:%d IPP Res type %d res_id:%d in wrong state %d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		rc = -EINVAL;
	}

	if (!pxl_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d IPP %d is not supported on HW",
			csid_hw->hw_intf->hw_idx, res->res_id);
		rc = -EINVAL;
		goto end;
	}

	/* Disable Error Recovery */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		pxl_reg->csid_pxl_err_recovery_cfg0_addr);

end:
	res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	return rc;
}

static int cam_tfe_csid_enable_pxl_path(
	struct cam_tfe_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res)
{
	const struct cam_tfe_csid_reg_offset     *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	struct cam_tfe_csid_path_cfg             *path_data;
	const struct cam_tfe_csid_pxl_reg_offset *pxl_reg = NULL;
	uint32_t                                  val = 0;

	path_data = (struct cam_tfe_csid_path_cfg   *) res->res_priv;
	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	pxl_reg = csid_reg->ipp_reg;

	if (res->res_state != CAM_ISP_RESOURCE_STATE_INIT_HW) {
		CAM_ERR(CAM_ISP,
			"CSID:%d IPP path res type:%d res_id:%d Invalid state%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	if (!pxl_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d IPP resid: %d not supported on HW",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "Enable IPP path");

	/* Set master or slave path */
	if (path_data->sync_mode == CAM_ISP_HW_SYNC_MASTER)
		/* Set halt mode as master */
		val = (TFE_CSID_HALT_MODE_MASTER  <<
			pxl_reg->halt_mode_shift) |
			(pxl_reg->halt_master_sel_master_val <<
			pxl_reg->halt_master_sel_shift);
	else if (path_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE)
		/* Set halt mode as slave and set master idx */
		val = (TFE_CSID_HALT_MODE_SLAVE << pxl_reg->halt_mode_shift);
	else
		/* Default is internal halt mode */
		val = 0;

	/*
	 * Resume at frame boundary if Master or No Sync.
	 * Slave will get resume command from Master.
	 */
	if (path_data->sync_mode == CAM_ISP_HW_SYNC_MASTER ||
		path_data->sync_mode == CAM_ISP_HW_SYNC_NONE)
		val |= CAM_TFE_CSID_RESUME_AT_FRAME_BOUNDARY;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		pxl_reg->csid_pxl_ctrl_addr);

	CAM_DBG(CAM_ISP, "CSID:%d IPP Ctrl val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);

	/* Enable the required pxl path interrupts */
	val = TFE_CSID_PATH_INFO_RST_DONE |
		TFE_CSID_PATH_ERROR_FIFO_OVERFLOW |
		TFE_CSID_PATH_IPP_ERROR_CCIF_VIOLATION |
		TFE_CSID_PATH_IPP_OVERFLOW_IRQ;

	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_SOF_IRQ)
		val |= TFE_CSID_PATH_INFO_INPUT_SOF;
	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_EOF_IRQ)
		val |= TFE_CSID_PATH_INFO_INPUT_EOF;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		pxl_reg->csid_pxl_irq_mask_addr);

	CAM_DBG(CAM_ISP, "Enable IPP IRQ mask 0x%x", val);

	res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	return 0;
}

static int cam_tfe_csid_change_pxl_halt_mode(
	struct cam_tfe_csid_hw            *csid_hw,
	struct cam_tfe_csid_hw_halt_args  *csid_halt)
{
	uint32_t val = 0;
	const struct cam_tfe_csid_reg_offset       *csid_reg;
	struct cam_hw_soc_info                     *soc_info;
	const struct cam_tfe_csid_pxl_reg_offset   *pxl_reg;
	struct cam_isp_resource_node               *res;

	res = csid_halt->node_res;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (res->res_id != CAM_TFE_CSID_PATH_RES_IPP) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res id %d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	if (res->res_state != CAM_ISP_RESOURCE_STATE_STREAMING) {
		CAM_ERR(CAM_ISP, "CSID:%d Res:%d in invalid state:%d",
			csid_hw->hw_intf->hw_idx, res->res_id, res->res_state);
		return -EINVAL;
	}

	pxl_reg = csid_reg->ipp_reg;

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		pxl_reg->csid_pxl_irq_mask_addr);

	/* configure Halt for slave */
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		pxl_reg->csid_pxl_ctrl_addr);
	val &= ~0xC;
	val |= (csid_halt->halt_mode << 2);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		pxl_reg->csid_pxl_ctrl_addr);
	CAM_DBG(CAM_ISP, "CSID:%d IPP path Res halt mode:%d configured:%x",
		csid_hw->hw_intf->hw_idx, csid_halt->halt_mode, val);

	return 0;
}

static int cam_tfe_csid_disable_pxl_path(
	struct cam_tfe_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res,
	enum cam_tfe_csid_halt_cmd       stop_cmd)
{
	int rc = 0;
	uint32_t val = 0;
	const struct cam_tfe_csid_reg_offset       *csid_reg;
	struct cam_hw_soc_info                     *soc_info;
	struct cam_tfe_csid_path_cfg               *path_data;
	const struct cam_tfe_csid_pxl_reg_offset   *pxl_reg;

	path_data = (struct cam_tfe_csid_path_cfg   *) res->res_priv;
	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (res->res_id >= CAM_TFE_CSID_PATH_RES_MAX) {
		CAM_DBG(CAM_ISP, "CSID:%d Invalid res id%d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	if (res->res_state == CAM_ISP_RESOURCE_STATE_INIT_HW ||
		res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_DBG(CAM_ISP, "CSID:%d Res:%d already in stopped state:%d",
			csid_hw->hw_intf->hw_idx, res->res_id, res->res_state);
		return rc;
	}

	pxl_reg = csid_reg->ipp_reg;
	if (res->res_state != CAM_ISP_RESOURCE_STATE_STREAMING) {
		CAM_DBG(CAM_ISP, "CSID:%d IPP path Res:%d Invalid state%d",
			csid_hw->hw_intf->hw_idx, res->res_id, res->res_state);
		return -EINVAL;
	}

	if (!pxl_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d IPP %d is not supported on HW",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	if (stop_cmd != CAM_TFE_CSID_HALT_AT_FRAME_BOUNDARY &&
		stop_cmd != CAM_TFE_CSID_HALT_IMMEDIATELY) {
		CAM_ERR(CAM_ISP,
			"CSID:%d IPP path un supported stop command:%d",
			csid_hw->hw_intf->hw_idx, stop_cmd);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "CSID:%d res_id:%d IPP path",
		csid_hw->hw_intf->hw_idx, res->res_id);

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		pxl_reg->csid_pxl_irq_mask_addr);

	if (path_data->sync_mode == CAM_ISP_HW_SYNC_MASTER ||
		path_data->sync_mode == CAM_ISP_HW_SYNC_NONE) {
		/* configure Halt */
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		pxl_reg->csid_pxl_ctrl_addr);
		val &= ~0x3;
		val |= stop_cmd;
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			pxl_reg->csid_pxl_ctrl_addr);
	}

	if (path_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE &&
		stop_cmd == CAM_TFE_CSID_HALT_IMMEDIATELY) {
		/* configure Halt for slave */
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			pxl_reg->csid_pxl_ctrl_addr);
		val &= ~0xF;
		val |= stop_cmd;
		val |= (TFE_CSID_HALT_MODE_MASTER << 2);
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			pxl_reg->csid_pxl_ctrl_addr);
	}

	return rc;
}

static int cam_tfe_csid_init_config_rdi_path(
	struct cam_tfe_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	struct cam_tfe_csid_path_cfg           *path_data;
	const struct cam_tfe_csid_reg_offset   *csid_reg;
	struct cam_hw_soc_info                 *soc_info;
	uint32_t path_format = 0, plain_fmt = 0, val = 0, id;

	path_data = (struct cam_tfe_csid_path_cfg   *) res->res_priv;
	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	id = res->res_id;
	if (!csid_reg->rdi_reg[id]) {
		CAM_ERR(CAM_ISP, "CSID:%d RDI:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, id);
		return -EINVAL;
	}

	rc = cam_tfe_csid_get_format_rdi(path_data->in_format,
		path_data->out_format, &path_format, &plain_fmt);
	if (rc)
		return rc;

	/*
	 * RDI path config and enable the time stamp capture
	 * Enable the measurement blocks
	 */
	val = (path_data->vc << csid_reg->cmn_reg->vc_shift_val) |
		(path_data->dt << csid_reg->cmn_reg->dt_shift_val) |
		(path_data->cid << csid_reg->cmn_reg->dt_id_shift_val) |
		(path_format << csid_reg->cmn_reg->fmt_shift_val) |
		(plain_fmt << csid_reg->cmn_reg->plain_fmt_shit_val) |
		(1 << 2) | 1;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_cfg0_addr);

	/* select the post irq sub sample strobe for time stamp capture */
	cam_io_w_mb(TFE_CSID_TIMESTAMP_STB_POST_IRQ,
		soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_cfg1_addr);

	/* Enable Error Detection, Overflow ctrl mode: 2 -> Detect overflow */
	val = 0x9;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_err_recovery_cfg0_addr);

	/* Configure the halt mode */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_ctrl_addr);

	/* Enable the RPP path */
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_cfg0_addr);
	val |= (1 << csid_reg->cmn_reg->path_en_shift_val);

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_cfg0_addr);

	/* configure the rx packet capture based on csid debug set */
	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE)
		val = ((1 <<
			csid_reg->csi2_reg->csi2_capture_short_pkt_en_shift) |
			(path_data->vc <<
			csid_reg->csi2_reg->csi2_capture_short_pkt_vc_shift));

	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE)
		val |= ((1 <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_en_shift) |
			(path_data->dt <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_dt_shift) |
			(path_data->vc <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_vc_shift));

	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE)
		val |= ((1 <<
			csid_reg->csi2_reg->csi2_capture_cphy_pkt_en_shift) |
			(path_data->dt <<
			csid_reg->csi2_reg->csi2_capture_cphy_pkt_dt_shift) |
			(path_data->vc <<
			csid_reg->csi2_reg->csi2_capture_cphy_pkt_vc_shift));
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_capture_ctrl_addr);

	res->res_state = CAM_ISP_RESOURCE_STATE_INIT_HW;

	return rc;
}

static int cam_tfe_csid_deinit_rdi_path(
	struct cam_tfe_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	uint32_t id;
	const struct cam_tfe_csid_reg_offset      *csid_reg;
	struct cam_hw_soc_info                    *soc_info;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	id = res->res_id;

	if (res->res_id > CAM_TFE_CSID_PATH_RES_RDI_2 ||
		res->res_state != CAM_ISP_RESOURCE_STATE_INIT_HW ||
		!csid_reg->rdi_reg[id]) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res id%d state:%d",
			csid_hw->hw_intf->hw_idx, res->res_id,
			res->res_state);
		return -EINVAL;
	}

	/* Disable Error Recovery */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_err_recovery_cfg0_addr);

	res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	return rc;
}

static int cam_tfe_csid_enable_rdi_path(
	struct cam_tfe_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res)
{
	const struct cam_tfe_csid_reg_offset      *csid_reg;
	struct cam_hw_soc_info                    *soc_info;
	uint32_t id, val;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	id = res->res_id;

	if (res->res_state != CAM_ISP_RESOURCE_STATE_INIT_HW ||
		res->res_id > CAM_TFE_CSID_PATH_RES_RDI_2 ||
		!csid_reg->rdi_reg[id]) {
		CAM_ERR(CAM_ISP,
			"CSID:%d invalid res type:%d res_id:%d state%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	/* resume at frame boundary */
	cam_io_w_mb(CAM_TFE_CSID_RESUME_AT_FRAME_BOUNDARY,
			soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_ctrl_addr);

	/* Enable the required RDI interrupts */
	val = TFE_CSID_PATH_INFO_RST_DONE | TFE_CSID_PATH_ERROR_FIFO_OVERFLOW |
		TFE_CSID_PATH_RDI_ERROR_CCIF_VIOLATION |
		TFE_CSID_PATH_RDI_OVERFLOW_IRQ;

	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_SOF_IRQ)
		val |= TFE_CSID_PATH_INFO_INPUT_SOF;
	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_EOF_IRQ)
		val |= TFE_CSID_PATH_INFO_INPUT_EOF;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_irq_mask_addr);

	res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	return 0;
}

static int cam_tfe_csid_disable_rdi_path(
	struct cam_tfe_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res,
	enum cam_tfe_csid_halt_cmd                stop_cmd)
{
	int rc = 0;
	uint32_t id, val = 0;
	const struct cam_tfe_csid_reg_offset       *csid_reg;
	struct cam_hw_soc_info                     *soc_info;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	id = res->res_id;

	if ((res->res_id > CAM_TFE_CSID_PATH_RES_RDI_2) ||
		(!csid_reg->rdi_reg[res->res_id])) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d Invalid res id%d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	if (res->res_state == CAM_ISP_RESOURCE_STATE_INIT_HW ||
		res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"CSID:%d Res:%d already in stopped state:%d",
			csid_hw->hw_intf->hw_idx,
			res->res_id, res->res_state);
		return rc;
	}

	if (res->res_state != CAM_ISP_RESOURCE_STATE_STREAMING) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"CSID:%d Res:%d Invalid res_state%d",
			csid_hw->hw_intf->hw_idx, res->res_id,
			res->res_state);
		return -EINVAL;
	}

	if (stop_cmd != CAM_TFE_CSID_HALT_AT_FRAME_BOUNDARY &&
		stop_cmd != CAM_TFE_CSID_HALT_IMMEDIATELY) {
		CAM_ERR(CAM_ISP, "CSID:%d un supported stop command:%d",
			csid_hw->hw_intf->hw_idx, stop_cmd);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "CSID:%d res_id:%d",
		csid_hw->hw_intf->hw_idx, res->res_id);

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_irq_mask_addr);

	/* Halt the RDI path */
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_ctrl_addr);
	val &= ~0x3;
	val |= stop_cmd;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_ctrl_addr);

	return rc;
}

static int cam_tfe_csid_poll_stop_status(
	struct cam_tfe_csid_hw          *csid_hw,
	uint32_t                         res_mask)
{
	int rc = 0;
	uint32_t csid_status_addr = 0, val = 0, res_id = 0;
	const struct cam_tfe_csid_reg_offset       *csid_reg;
	struct cam_hw_soc_info                     *soc_info;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	for (; res_id < CAM_TFE_CSID_PATH_RES_MAX; res_id++, res_mask >>= 1) {
		if ((res_mask & 0x1) == 0)
			continue;
		val = 0;

		if (res_id == CAM_TFE_CSID_PATH_RES_IPP) {
			csid_status_addr =
			csid_reg->ipp_reg->csid_pxl_status_addr;

			if (csid_hw->ipp_res.res_state !=
				CAM_ISP_RESOURCE_STATE_STREAMING)
				continue;

		} else {
			csid_status_addr =
				csid_reg->rdi_reg[res_id]->csid_rdi_status_addr;

			if (csid_hw->rdi_res[res_id].res_state !=
				CAM_ISP_RESOURCE_STATE_STREAMING)
				continue;

		}

		CAM_DBG(CAM_ISP, "start polling CSID:%d res_id:%d",
			csid_hw->hw_intf->hw_idx, res_id);

		rc = readl_poll_timeout(soc_info->reg_map[0].mem_base +
			csid_status_addr, val, (val & 0x1) == 0x1,
				CAM_TFE_CSID_TIMEOUT_SLEEP_US,
				CAM_TFE_CSID_TIMEOUT_ALL_US);
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

static int __cam_tfe_csid_read_timestamp(void __iomem *base,
	uint32_t msb_offset, uint32_t lsb_offset, uint64_t *timestamp)
{
	uint32_t lsb, msb, tmp, torn = 0;

	msb = cam_io_r_mb(base + msb_offset);
	do {
		tmp = msb;
		torn++;
		lsb = cam_io_r_mb(base + lsb_offset);
		msb = cam_io_r_mb(base + msb_offset);
	} while (tmp != msb);

	*timestamp = msb;
	*timestamp = (*timestamp << 32) | lsb;

	return (torn > 1);
}

static int cam_tfe_csid_get_time_stamp(
		struct cam_tfe_csid_hw   *csid_hw, void *cmd_args)
{
	struct cam_tfe_csid_get_time_stamp_args    *time_stamp;
	struct cam_isp_resource_node               *res;
	const struct cam_tfe_csid_reg_offset       *csid_reg;
	struct cam_hw_soc_info                     *soc_info;
	const struct cam_tfe_csid_rdi_reg_offset   *rdi_reg;
	struct timespec64 ts;
	uint32_t  id, torn;
	uint64_t  time_delta;

	time_stamp = (struct cam_tfe_csid_get_time_stamp_args  *)cmd_args;
	res = time_stamp->node_res;
	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (res->res_type != CAM_ISP_RESOURCE_PIX_PATH ||
		res->res_id >= CAM_TFE_CSID_PATH_RES_MAX) {
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

	if (res->res_id == CAM_TFE_CSID_PATH_RES_IPP) {
		torn = __cam_tfe_csid_read_timestamp(
			soc_info->reg_map[0].mem_base,
			csid_reg->ipp_reg->csid_pxl_timestamp_curr1_sof_addr,
			csid_reg->ipp_reg->csid_pxl_timestamp_curr0_sof_addr,
			&time_stamp->time_stamp_val);
	} else {
		id = res->res_id;
		rdi_reg = csid_reg->rdi_reg[id];
		torn = __cam_tfe_csid_read_timestamp(
			soc_info->reg_map[0].mem_base,
			rdi_reg->csid_rdi_timestamp_curr1_sof_addr,
			rdi_reg->csid_rdi_timestamp_curr0_sof_addr,
			&time_stamp->time_stamp_val);
	}

	time_stamp->time_stamp_val = mul_u64_u32_div(
		time_stamp->time_stamp_val,
		CAM_TFE_CSID_QTIMER_MUL_FACTOR,
		CAM_TFE_CSID_QTIMER_DIV_FACTOR);

	if (!csid_hw->prev_boot_timestamp) {
		ktime_get_boottime_ts64(&ts);
		time_stamp->boot_timestamp =
			(uint64_t)((ts.tv_sec * 1000000000) +
			ts.tv_nsec);
		csid_hw->prev_qtimer_ts = 0;
		CAM_DBG(CAM_ISP, "timestamp:%lld",
			time_stamp->boot_timestamp);
	} else {
		time_delta = time_stamp->time_stamp_val -
			csid_hw->prev_qtimer_ts;

		if (csid_hw->prev_boot_timestamp >
			U64_MAX - time_delta) {
			CAM_WARN(CAM_ISP, "boottimestamp overflowed");
			CAM_INFO(CAM_ISP,
			"currQTimer %lx prevQTimer %lx prevBootTimer %lx torn %d",
				time_stamp->time_stamp_val,
				csid_hw->prev_qtimer_ts,
				csid_hw->prev_boot_timestamp, torn);
			return -EINVAL;
		}

		time_stamp->boot_timestamp =
			csid_hw->prev_boot_timestamp + time_delta;
	}

	CAM_DBG(CAM_ISP,
	"currQTimer %lx prevQTimer %lx currBootTimer %lx prevBootTimer %lx torn %d",
		time_stamp->time_stamp_val,
		csid_hw->prev_qtimer_ts, time_stamp->boot_timestamp,
		csid_hw->prev_boot_timestamp, torn);

	csid_hw->prev_qtimer_ts = time_stamp->time_stamp_val;
	csid_hw->prev_boot_timestamp = time_stamp->boot_timestamp;

	return 0;
}

static int cam_tfe_csid_set_csid_debug(struct cam_tfe_csid_hw   *csid_hw,
	void *cmd_args)
{
	uint32_t  *csid_debug;

	csid_debug = (uint32_t  *) cmd_args;
	csid_hw->csid_debug = *csid_debug;
	CAM_DBG(CAM_ISP, "CSID:%d set csid debug value:%d",
		csid_hw->hw_intf->hw_idx, csid_hw->csid_debug);

	return 0;
}

static int cam_tfe_csid_get_hw_caps(void *hw_priv,
	void *get_hw_cap_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_tfe_csid_hw_caps           *hw_caps;
	struct cam_tfe_csid_hw                *csid_hw;
	struct cam_hw_info                    *csid_hw_info;
	const struct cam_tfe_csid_reg_offset  *csid_reg;

	if (!hw_priv || !get_hw_cap_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_tfe_csid_hw   *)csid_hw_info->core_info;
	csid_reg = csid_hw->csid_info->csid_reg;
	hw_caps = (struct cam_tfe_csid_hw_caps *) get_hw_cap_args;

	hw_caps->num_rdis = csid_reg->cmn_reg->num_rdis;
	hw_caps->num_pix = csid_hw->pxl_pipe_enable;
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

static int cam_tfe_csid_reset(void *hw_priv,
	void *reset_args, uint32_t arg_size)
{
	struct cam_tfe_csid_hw          *csid_hw;
	struct cam_hw_info              *csid_hw_info;
	struct cam_tfe_csid_reset_cfg_args  *reset;
	int rc = 0;

	if (!hw_priv || !reset_args || (arg_size !=
		sizeof(struct cam_tfe_csid_reset_cfg_args))) {
		CAM_ERR(CAM_ISP, "CSID:Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_tfe_csid_hw   *)csid_hw_info->core_info;
	reset   = (struct cam_tfe_csid_reset_cfg_args  *)reset_args;

	switch (reset->reset_type) {
	case CAM_TFE_CSID_RESET_GLOBAL:
		rc = cam_tfe_csid_global_reset(csid_hw);
		break;
	case CAM_TFE_CSID_RESET_PATH:
		rc = cam_tfe_csid_path_reset(csid_hw, reset);
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:Invalid reset type :%d",
			reset->reset_type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int cam_tfe_csid_reserve(void *hw_priv,
	void *reserve_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_tfe_csid_hw                    *csid_hw;
	struct cam_hw_info                        *csid_hw_info;
	struct cam_tfe_csid_hw_reserve_resource_args  *reserv;

	if (!hw_priv || !reserve_args || (arg_size !=
		sizeof(struct cam_tfe_csid_hw_reserve_resource_args))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_tfe_csid_hw   *)csid_hw_info->core_info;
	reserv = (struct cam_tfe_csid_hw_reserve_resource_args  *)reserve_args;

	if (reserv->res_type != CAM_ISP_RESOURCE_PIX_PATH) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type :%d",
			csid_hw->hw_intf->hw_idx, reserv->res_type);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "res_type %d, CSID: %u",
		reserv->res_type, csid_hw->hw_intf->hw_idx);

	mutex_lock(&csid_hw->hw_info->hw_mutex);
	rc = cam_tfe_csid_path_reserve(csid_hw, reserv);
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_tfe_csid_release(void *hw_priv,
	void *release_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_tfe_csid_hw          *csid_hw;
	struct cam_hw_info              *csid_hw_info;
	struct cam_isp_resource_node    *res;
	struct cam_tfe_csid_path_cfg    *path_data;

	if (!hw_priv || !release_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_tfe_csid_hw   *)csid_hw_info->core_info;
	res = (struct cam_isp_resource_node *)release_args;

	if (res->res_type != CAM_ISP_RESOURCE_PIX_PATH) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type:%d res id%d",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	mutex_lock(&csid_hw->hw_info->hw_mutex);
	if ((res->res_type == CAM_ISP_RESOURCE_PIX_PATH &&
		res->res_id >= CAM_TFE_CSID_PATH_RES_MAX)) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type:%d res id%d",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id);
		rc = -EINVAL;
		goto end;
	}

	csid_hw->event_cb = NULL;
	csid_hw->event_cb_priv = NULL;

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

	path_data = (struct cam_tfe_csid_path_cfg *)res->res_priv;
	if (csid_hw->cid_res[path_data->cid].cnt)
		csid_hw->cid_res[path_data->cid].cnt--;

	if (csid_hw->csi2_reserve_cnt)
		csid_hw->csi2_reserve_cnt--;

	if (!csid_hw->csi2_reserve_cnt)
		memset(&csid_hw->csi2_rx_cfg, 0,
			sizeof(struct cam_tfe_csid_csi2_rx_cfg));

	CAM_DBG(CAM_ISP, "CSID:%d res id :%d cnt:%d reserv cnt:%d",
		csid_hw->hw_intf->hw_idx,
		res->res_id, csid_hw->cid_res[path_data->cid].cnt,
		csid_hw->csi2_reserve_cnt);

	res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

end:
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_tfe_csid_reset_retain_sw_reg(
	struct cam_tfe_csid_hw *csid_hw)
{
	int rc = 0;
	uint32_t status;
	const struct cam_tfe_csid_reg_offset *csid_reg =
		csid_hw->csid_info->csid_reg;
	struct cam_hw_soc_info          *soc_info;

	soc_info = &csid_hw->hw_info->soc_info;

	/* Mask top interrupts */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_mask_addr);
	/* clear the top interrupt first */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_clear_addr);
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_irq_cmd_addr);

	cam_io_w_mb(csid_reg->cmn_reg->csid_rst_stb,
		soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_rst_strobes_addr);
	rc = readl_poll_timeout(soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_status_addr,
			status, (status & 0x1) == 0x1,
		CAM_TFE_CSID_TIMEOUT_SLEEP_US, CAM_TFE_CSID_TIMEOUT_ALL_US);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "CSID:%d csid_reset fail rc = %d",
			  csid_hw->hw_intf->hw_idx, rc);
		status = cam_io_r(soc_info->reg_map[0].mem_base +
			csid_reg->cmn_reg->csid_top_irq_status_addr);
		CAM_DBG(CAM_ISP, "Status reg %d", status);
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

static int cam_tfe_csid_init_hw(void *hw_priv,
	void *init_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_tfe_csid_hw                 *csid_hw;
	struct cam_hw_info                     *csid_hw_info;
	struct cam_isp_resource_node           *res;
	const struct cam_tfe_csid_reg_offset   *csid_reg;
	unsigned long                           flags;

	if (!hw_priv || !init_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_tfe_csid_hw   *)csid_hw_info->core_info;
	res      = (struct cam_isp_resource_node *)init_args;
	csid_reg = csid_hw->csid_info->csid_reg;

	if (res->res_type != CAM_ISP_RESOURCE_PIX_PATH) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type state %d",
			csid_hw->hw_intf->hw_idx,
			res->res_type);
		return -EINVAL;
	}

	mutex_lock(&csid_hw->hw_info->hw_mutex);
	if (res->res_type == CAM_ISP_RESOURCE_PIX_PATH &&
		res->res_id >= CAM_TFE_CSID_PATH_RES_MAX) {
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

	/* Initialize the csid hardware */
	rc = cam_tfe_csid_enable_hw(csid_hw);
	if (rc)
		goto end;

	if (res->res_id == CAM_TFE_CSID_PATH_RES_IPP)
		rc = cam_tfe_csid_init_config_pxl_path(csid_hw, res);
	else
		rc = cam_tfe_csid_init_config_rdi_path(csid_hw, res);

	rc = cam_tfe_csid_reset_retain_sw_reg(csid_hw);
	if (rc < 0)
		CAM_ERR(CAM_ISP, "CSID: Failed in SW reset");

	if (rc)
		cam_tfe_csid_disable_hw(csid_hw);

	spin_lock_irqsave(&csid_hw->spin_lock, flags);
	csid_hw->device_enabled = 1;
	spin_unlock_irqrestore(&csid_hw->spin_lock, flags);
end:
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_tfe_csid_deinit_hw(void *hw_priv,
	void *deinit_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_tfe_csid_hw                 *csid_hw;
	struct cam_hw_info                     *csid_hw_info;
	struct cam_isp_resource_node           *res;

	if (!hw_priv || !deinit_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "CSID:Invalid arguments");
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "Enter");
	res = (struct cam_isp_resource_node *)deinit_args;
	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_tfe_csid_hw   *)csid_hw_info->core_info;

	if (res->res_type != CAM_ISP_RESOURCE_PIX_PATH) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid Res type %d",
			 csid_hw->hw_intf->hw_idx,
			res->res_type);
		return -EINVAL;
	}

	mutex_lock(&csid_hw->hw_info->hw_mutex);
	if (res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_DBG(CAM_ISP, "CSID:%d Res:%d already in De-init state",
			 csid_hw->hw_intf->hw_idx,
			res->res_id);
		goto end;
	}

	CAM_DBG(CAM_ISP, "De-Init IPP Path: %d", res->res_id);

	if (res->res_id == CAM_TFE_CSID_PATH_RES_IPP)
		rc = cam_tfe_csid_deinit_pxl_path(csid_hw, res);
	else
		rc = cam_tfe_csid_deinit_rdi_path(csid_hw, res);

	/* Disable CSID HW */
	CAM_DBG(CAM_ISP, "Disabling CSID Hw");
	cam_tfe_csid_disable_hw(csid_hw);
	CAM_DBG(CAM_ISP, "%s: Exit", __func__);

end:
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_tfe_csid_start(void *hw_priv, void *start_args,
			uint32_t arg_size)
{
	int rc = 0;
	struct cam_tfe_csid_hw                 *csid_hw;
	struct cam_hw_info                     *csid_hw_info;
	struct cam_isp_resource_node           *res;
	const struct cam_tfe_csid_reg_offset   *csid_reg;

	if (!hw_priv || !start_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_tfe_csid_hw   *)csid_hw_info->core_info;
	res = (struct cam_isp_resource_node *)start_args;
	csid_reg = csid_hw->csid_info->csid_reg;

	if (res->res_type == CAM_ISP_RESOURCE_PIX_PATH &&
		res->res_id >= CAM_TFE_CSID_PATH_RES_MAX) {
		CAM_DBG(CAM_ISP, "CSID:%d Invalid res tpe:%d res id:%d",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id);
		rc = -EINVAL;
		goto end;
	}

	/* Reset sof irq debug fields */
	csid_hw->sof_irq_triggered = false;
	csid_hw->irq_debug_cnt = 0;

	CAM_DBG(CAM_ISP, "CSID:%d res_type :%d res_id:%d",
		csid_hw->hw_intf->hw_idx, res->res_type, res->res_id);

	switch (res->res_type) {
	case CAM_ISP_RESOURCE_PIX_PATH:
		if (res->res_id == CAM_TFE_CSID_PATH_RES_IPP)
			rc = cam_tfe_csid_enable_pxl_path(csid_hw, res);
		else
			rc = cam_tfe_csid_enable_rdi_path(csid_hw, res);
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type%d",
			csid_hw->hw_intf->hw_idx, res->res_type);
		break;
	}
end:
	return rc;
}

int cam_tfe_csid_halt(struct cam_tfe_csid_hw *csid_hw,
	void *halt_args)
{
	struct cam_isp_resource_node         *res;
	struct cam_tfe_csid_hw_halt_args     *csid_halt;
	int rc = 0;

	if (!csid_hw || !halt_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_halt = (struct cam_tfe_csid_hw_halt_args *)halt_args;

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
	case CAM_TFE_CSID_PATH_RES_IPP:
		rc = cam_tfe_csid_change_pxl_halt_mode(csid_hw, csid_halt);
		break;
	default:
		CAM_DBG(CAM_ISP, "CSID:%d res_id %d",
			csid_hw->hw_intf->hw_idx,
			res->res_id);
		break;
	}

	return rc;
}

static int cam_tfe_csid_stop(void *hw_priv,
	void *stop_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_tfe_csid_hw               *csid_hw;
	struct cam_hw_info                   *csid_hw_info;
	struct cam_isp_resource_node         *res;
	struct cam_tfe_csid_hw_stop_args     *csid_stop;
	uint32_t  i;
	uint32_t res_mask = 0;

	if (!hw_priv || !stop_args ||
		(arg_size != sizeof(struct cam_tfe_csid_hw_stop_args))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}
	csid_stop = (struct cam_tfe_csid_hw_stop_args  *) stop_args;

	if (!csid_stop->num_res) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_tfe_csid_hw   *)csid_hw_info->core_info;
	CAM_DBG(CAM_ISP, "CSID:%d num_res %d",
		csid_hw->hw_intf->hw_idx,
		csid_stop->num_res);

	/* Stop the resource first */
	for (i = 0; i < csid_stop->num_res; i++) {
		res = csid_stop->node_res[i];
		CAM_DBG(CAM_ISP, "CSID:%d res_type %d res_id %d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id);
		switch (res->res_type) {
		case CAM_ISP_RESOURCE_PIX_PATH:
			res_mask |= (1 << res->res_id);
			if (res->res_id == CAM_TFE_CSID_PATH_RES_IPP)
				rc = cam_tfe_csid_disable_pxl_path(csid_hw,
					res, csid_stop->stop_cmd);
			else
				rc = cam_tfe_csid_disable_rdi_path(csid_hw,
					res, csid_stop->stop_cmd);

			break;
		default:
			CAM_ERR(CAM_ISP, "CSID:%d Invalid res type%d",
				csid_hw->hw_intf->hw_idx,
				res->res_type);
			break;
		}
	}

	if (res_mask)
		rc = cam_tfe_csid_poll_stop_status(csid_hw, res_mask);

	for (i = 0; i < csid_stop->num_res; i++) {
		res = csid_stop->node_res[i];
		res->res_state = CAM_ISP_RESOURCE_STATE_INIT_HW;
	}

	CAM_DBG(CAM_ISP,  "%s: Exit", __func__);
	return rc;
}

static int cam_tfe_csid_read(void *hw_priv,
	void *read_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "CSID: un supported");
	return -EINVAL;
}

static int cam_tfe_csid_write(void *hw_priv,
	void *write_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "CSID: un supported");
	return -EINVAL;
}

static int cam_tfe_csid_sof_irq_debug(
	struct cam_tfe_csid_hw *csid_hw, void *cmd_args)
{
	int i = 0;
	uint32_t val = 0;
	bool sof_irq_enable = false;
	const struct cam_tfe_csid_reg_offset    *csid_reg;
	struct cam_hw_soc_info                  *soc_info;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (*((uint32_t *)cmd_args) == 1)
		sof_irq_enable = true;

	if (csid_hw->hw_info->hw_state ==
		CAM_HW_STATE_POWER_DOWN) {
		CAM_WARN(CAM_ISP,
			"CSID:%d powered down unable to %s sof irq",
			csid_hw->hw_intf->hw_idx,
			sof_irq_enable ? "enable" : "disable");
		return 0;
	}

	if (csid_reg->ipp_reg) {
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_irq_mask_addr);

		if (val) {
			if (sof_irq_enable)
				val |= TFE_CSID_PATH_INFO_INPUT_SOF;
			else
				val &= ~TFE_CSID_PATH_INFO_INPUT_SOF;

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
				val |= TFE_CSID_PATH_INFO_INPUT_SOF;
			else
				val &= ~TFE_CSID_PATH_INFO_INPUT_SOF;

			cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
				csid_reg->rdi_reg[i]->csid_rdi_irq_mask_addr);
			val = 0;
		}
	}

	if (sof_irq_enable) {
		csid_hw->csid_debug |= TFE_CSID_DEBUG_ENABLE_SOF_IRQ;
		csid_hw->sof_irq_triggered = true;
	} else {
		csid_hw->csid_debug &= ~TFE_CSID_DEBUG_ENABLE_SOF_IRQ;
		csid_hw->sof_irq_triggered = false;
	}

	if (!in_irq())
		CAM_INFO(CAM_ISP, "SOF freeze: CSID:%d SOF irq %s",
			csid_hw->hw_intf->hw_idx,
			sof_irq_enable ? "enabled" : "disabled");

	return 0;
}

static int cam_tfe_csid_set_csid_clock(
	struct cam_tfe_csid_hw *csid_hw, void *cmd_args)
{
	struct cam_tfe_csid_clock_update_args *clk_update = NULL;

	if (!csid_hw)
		return -EINVAL;

	clk_update =
		(struct cam_tfe_csid_clock_update_args *)cmd_args;

	csid_hw->clk_rate = clk_update->clk_rate;
	CAM_DBG(CAM_ISP, "CSID clock rate %llu", csid_hw->clk_rate);

	return 0;
}

static int cam_tfe_csid_get_regdump(struct cam_tfe_csid_hw *csid_hw,
	void *cmd_args)
{
	struct cam_tfe_csid_reg_offset    *csid_reg;
	struct cam_hw_soc_info            *soc_info;
	struct cam_isp_resource_node      *res;
	struct cam_tfe_csid_path_cfg      *path_data;
	uint32_t id;
	int val;

	csid_reg = (struct cam_tfe_csid_reg_offset   *)
			csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	res = (struct cam_isp_resource_node  *)cmd_args;
	path_data = (struct cam_tfe_csid_path_cfg   *)res->res_priv;

	if (res->res_type != CAM_ISP_RESOURCE_PIX_PATH ||
		res->res_id >= CAM_TFE_CSID_PATH_RES_MAX) {
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

	if (res->res_id == CAM_TFE_CSID_PATH_RES_IPP) {
		CAM_INFO(CAM_ISP, "Dumping CSID:%d IPP registers ",
			csid_hw->hw_intf->hw_idx);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_cfg0_addr);
		CAM_INFO(CAM_ISP, "offset 0x%x=0x08%x",
			csid_reg->ipp_reg->csid_pxl_cfg0_addr, val);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_cfg1_addr);
		CAM_INFO(CAM_ISP, "offset 0x%x=0x08%x",
			csid_reg->ipp_reg->csid_pxl_cfg1_addr, val);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_ctrl_addr);
		CAM_INFO(CAM_ISP, "offset 0x%x=0x08%x",
			csid_reg->ipp_reg->csid_pxl_ctrl_addr, val);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_hcrop_addr);
		CAM_INFO(CAM_ISP, "offset 0x%x=0x08%x",
			csid_reg->ipp_reg->csid_pxl_hcrop_addr, val);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_vcrop_addr);
		CAM_INFO(CAM_ISP, "offset 0x%x=0x08%x",
			csid_reg->ipp_reg->csid_pxl_vcrop_addr, val);
	} else {
		id = res->res_id;
		CAM_INFO(CAM_ISP, "Dumping CSID:%d RDI:%d registers ",
			csid_hw->hw_intf->hw_idx, id);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_cfg0_addr);
		CAM_INFO(CAM_ISP, "offset 0x%x=0x08%x",
			csid_reg->rdi_reg[id]->csid_rdi_cfg0_addr, val);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_cfg1_addr);
		CAM_INFO(CAM_ISP, "offset 0x%x=0x08%x",
			csid_reg->rdi_reg[id]->csid_rdi_cfg1_addr, val);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_ctrl_addr);
		CAM_INFO(CAM_ISP, "offset 0x%x=0x08%x",
			csid_reg->rdi_reg[id]->csid_rdi_ctrl_addr, val);
	}
	CAM_INFO(CAM_ISP,
		"start pix:%d end pix:%d start line:%d end line:%d w:%d h:%d",
		path_data->start_pixel, path_data->end_pixel,
		path_data->start_line, path_data->end_line,
		path_data->width, path_data->height);
	CAM_INFO(CAM_ISP,
		"clock:%d crop_enable:%d vc:%d dt:%d informat:%d outformat:%d",
		path_data->clk_rate, path_data->crop_enable,
		path_data->vc, path_data->dt,
		path_data->in_format, path_data->out_format);

	return 0;
}

static int cam_tfe_csid_dump_hw(
	struct cam_tfe_csid_hw *csid_hw, void *cmd_args)
{
	int                             i;
	uint8_t                        *dst;
	uint32_t                       *addr, *start;
	uint64_t                       *clk_addr, *clk_start;
	uint32_t                        min_len;
	uint32_t                        num_reg;
	uint32_t                        reg_size = 0;
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

	if (dump_args->buf_len <= dump_args->offset) {
		CAM_WARN(CAM_ISP,
			"Dump offset overshoot offset %zu buf_len %zu",
			dump_args->offset, dump_args->buf_len);
		return -ENOSPC;
	}

	soc_info = &csid_hw->hw_info->soc_info;
	if (dump_args->is_dump_all)
		reg_size = soc_info->reg_map[0].size;

	min_len = reg_size +
		sizeof(struct cam_isp_hw_dump_header) +
		(sizeof(uint32_t) * CAM_TFE_CSID_DUMP_MISC_NUM_WORDS);
	remain_len = dump_args->buf_len - dump_args->offset;

	if (remain_len < min_len) {
		CAM_WARN(CAM_ISP, "Dump buffer exhaust remain %zu, min %u",
			remain_len, min_len);
		return -ENOSPC;
	}

	mutex_lock(&csid_hw->hw_info->hw_mutex);
	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid HW State:%d",
			csid_hw->hw_intf->hw_idx,
			csid_hw->hw_info->hw_state);
		mutex_unlock(&csid_hw->hw_info->hw_mutex);
		return -EINVAL;
	}

	if (!dump_args->is_dump_all)
		goto dump_bw;

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
dump_bw:
	dst = (char *)dump_args->cpu_addr + dump_args->offset;
	hdr = (struct cam_isp_hw_dump_header *)dst;
	scnprintf(hdr->tag, CAM_ISP_HW_DUMP_TAG_MAX_LEN, "CSID_CLK_RATE:");
	clk_addr = (uint64_t *)(dst +
		sizeof(struct cam_isp_hw_dump_header));
	clk_start = clk_addr;
	hdr->word_size = sizeof(uint64_t);
	*clk_addr++ = csid_hw->clk_rate;
	hdr->size = hdr->word_size * (clk_addr - clk_start);
	dump_args->offset +=  hdr->size +
		sizeof(struct cam_isp_hw_dump_header);
	CAM_DBG(CAM_ISP, "offset %zu", dump_args->offset);
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return 0;
}

static int cam_tfe_csid_log_acquire_data(
	struct cam_tfe_csid_hw   *csid_hw,  void *cmd_args)
{
	struct cam_isp_resource_node  *res =
		(struct cam_isp_resource_node *)cmd_args;
	struct cam_tfe_csid_path_cfg       *path_data;
	struct cam_hw_soc_info                         *soc_info;
	const struct cam_tfe_csid_reg_offset           *csid_reg;
	uint32_t byte_cnt_ping, byte_cnt_pong;

	path_data = (struct cam_tfe_csid_path_cfg *)res->res_priv;
	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (res->res_state <= CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP,
			"CSID:%d invalid res id:%d res type: %d state:%d",
			csid_hw->hw_intf->hw_idx, res->res_id, res->res_type,
			res->res_state);
		return -EINVAL;
	}

	/* Dump all the acquire data for this  */
	CAM_INFO(CAM_ISP,
		"CSID:%d res id:%d type:%d state:%d in f:%d out f:%d st pix:%d end pix:%d st line:%d end line:%d",
		csid_hw->hw_intf->hw_idx, res->res_id, res->res_type,
		res->res_type, path_data->in_format, path_data->out_format,
		path_data->start_pixel, path_data->end_pixel,
		path_data->start_line, path_data->end_line);

	if (res->res_id >= CAM_TFE_CSID_PATH_RES_RDI_0  &&
		res->res_id <= CAM_TFE_CSID_PATH_RES_RDI_2) {
		/* read total number of bytes transmitted through RDI */
		byte_cnt_ping = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[res->res_id]->csid_rdi_byte_cntr_ping_addr);
		byte_cnt_pong = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[res->res_id]->csid_rdi_byte_cntr_pong_addr);
		CAM_INFO(CAM_ISP,
			"CSID:%d res id:%d byte cnt val ping:%d pong:%d",
			csid_hw->hw_intf->hw_idx, res->res_id,
			byte_cnt_ping, byte_cnt_pong);
	}

	return 0;

}

static int cam_tfe_csid_process_cmd(void *hw_priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_tfe_csid_hw               *csid_hw;
	struct cam_hw_info                   *csid_hw_info;

	if (!hw_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid arguments");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_tfe_csid_hw   *)csid_hw_info->core_info;

	switch (cmd_type) {
	case CAM_TFE_CSID_CMD_GET_TIME_STAMP:
		rc = cam_tfe_csid_get_time_stamp(csid_hw, cmd_args);
		break;
	case CAM_TFE_CSID_SET_CSID_DEBUG:
		rc = cam_tfe_csid_set_csid_debug(csid_hw, cmd_args);
		break;
	case CAM_TFE_CSID_SOF_IRQ_DEBUG:
		rc = cam_tfe_csid_sof_irq_debug(csid_hw, cmd_args);
		break;
	case CAM_ISP_HW_CMD_CSID_CLOCK_UPDATE:
		rc = cam_tfe_csid_set_csid_clock(csid_hw, cmd_args);
		break;
	case CAM_TFE_CSID_CMD_GET_REG_DUMP:
		rc = cam_tfe_csid_get_regdump(csid_hw, cmd_args);
		break;
	case CAM_ISP_HW_CMD_DUMP_HW:
		rc = cam_tfe_csid_dump_hw(csid_hw, cmd_args);
		break;
	case CAM_ISP_HW_CMD_CSID_CHANGE_HALT_MODE:
		rc = cam_tfe_csid_halt(csid_hw, cmd_args);
		break;
	case CAM_TFE_CSID_LOG_ACQUIRE_DATA:
		rc = cam_tfe_csid_log_acquire_data(csid_hw, cmd_args);
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d unsupported cmd:%d",
			csid_hw->hw_intf->hw_idx, cmd_type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int cam_tfe_csid_get_evt_payload(
	struct cam_tfe_csid_hw *csid_hw,
	struct cam_csid_evt_payload **evt_payload)
{

	spin_lock(&csid_hw->spin_lock);

	if (list_empty(&csid_hw->free_payload_list)) {
		*evt_payload = NULL;
		spin_unlock(&csid_hw->spin_lock);
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No free payload core %d",
			csid_hw->hw_intf->hw_idx);
		return -ENOMEM;
	}

	*evt_payload = list_first_entry(&csid_hw->free_payload_list,
			struct cam_csid_evt_payload, list);
	list_del_init(&(*evt_payload)->list);
	spin_unlock(&csid_hw->spin_lock);

	return 0;
}

static int cam_tfe_csid_put_evt_payload(
	struct cam_tfe_csid_hw *csid_hw,
	struct cam_csid_evt_payload **evt_payload)
{
	unsigned long flags;

	if (*evt_payload == NULL) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid payload core %d",
			csid_hw->hw_intf->hw_idx);
		return -EINVAL;
	}
	spin_lock_irqsave(&csid_hw->spin_lock, flags);
	list_add_tail(&(*evt_payload)->list,
		&csid_hw->free_payload_list);
	*evt_payload = NULL;
	spin_unlock_irqrestore(&csid_hw->spin_lock, flags);

	return 0;
}

static int cam_tfe_csid_evt_bottom_half_handler(
	void *handler_priv,
	void *evt_payload_priv)
{
	struct cam_tfe_csid_hw *csid_hw;
	struct cam_csid_evt_payload *evt_payload;
	const struct cam_tfe_csid_reg_offset    *csid_reg;
	struct cam_isp_hw_event_info event_info;
	int i;
	int rc = 0;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP,
			"Invalid Param handler_priv %pK evt_payload_priv %pK",
			handler_priv, evt_payload_priv);
		return 0;
	}

	csid_hw = (struct cam_tfe_csid_hw *)handler_priv;
	evt_payload = (struct cam_csid_evt_payload *)evt_payload_priv;
	csid_reg = csid_hw->csid_info->csid_reg;

	if (!csid_hw->event_cb || !csid_hw->event_cb_priv) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"hw_idx %d Invalid args %pK %pK",
			csid_hw->hw_intf->hw_idx,
			csid_hw->event_cb,
			csid_hw->event_cb_priv);
		goto end;
	}

	if (csid_hw->event_cb_priv != evt_payload->priv) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"hw_idx %d priv mismatch %pK, %pK",
			csid_hw->hw_intf->hw_idx,
			csid_hw->event_cb_priv,
			evt_payload->priv);
		goto end;
	}

	if (csid_hw->sof_irq_triggered && (evt_payload->evt_type ==
		CAM_ISP_HW_ERROR_NONE)) {
		if (evt_payload->irq_status[TFE_CSID_IRQ_REG_IPP] &
			TFE_CSID_PATH_INFO_INPUT_SOF) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d IPP SOF received",
				csid_hw->hw_intf->hw_idx);
		}

		for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++) {
			if (evt_payload->irq_status[i] &
				TFE_CSID_PATH_INFO_INPUT_SOF)
				CAM_INFO_RATE_LIMIT(CAM_ISP,
					"CSID:%d RDI:%d SOF received",
					csid_hw->hw_intf->hw_idx, i);
		}
	} else {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"CSID %d err %d phy %d irq status TOP: 0x%x RX: 0x%x IPP: 0x%x RDI0: 0x%x RDI1: 0x%x RDI2: 0x%x",
			csid_hw->hw_intf->hw_idx,
			evt_payload->evt_type,
			csid_hw->csi2_rx_cfg.phy_sel,
			evt_payload->irq_status[TFE_CSID_IRQ_REG_TOP],
			evt_payload->irq_status[TFE_CSID_IRQ_REG_RX],
			evt_payload->irq_status[TFE_CSID_IRQ_REG_IPP],
			evt_payload->irq_status[TFE_CSID_IRQ_REG_RDI0],
			evt_payload->irq_status[TFE_CSID_IRQ_REG_RDI1],
			evt_payload->irq_status[TFE_CSID_IRQ_REG_RDI2]);
	}
	/* this hunk can be extended to handle more cases
	 * which we want to offload to bottom half from
	 * irq handlers
	 */
	event_info.err_type = evt_payload->evt_type;
	event_info.hw_idx = evt_payload->hw_idx;

	switch (evt_payload->evt_type) {
	case CAM_ISP_HW_ERROR_CSID_FATAL:
		if (csid_hw->fatal_err_detected)
			break;
		csid_hw->fatal_err_detected = true;
		rc = csid_hw->event_cb(NULL,
			CAM_ISP_HW_EVENT_ERROR, (void *)&event_info);
		break;

	default:
		CAM_DBG(CAM_ISP, "CSID[%d] error type %d",
			csid_hw->hw_intf->hw_idx,
			evt_payload->evt_type);
		break;
	}
end:
	cam_tfe_csid_put_evt_payload(csid_hw, &evt_payload);
	return 0;
}

static int cam_tfe_csid_handle_hw_err_irq(
	struct cam_tfe_csid_hw *csid_hw,
	int                     evt_type,
	uint32_t               *irq_status)
{
	int      rc = 0;
	int      i;
	void    *bh_cmd = NULL;
	struct cam_csid_evt_payload *evt_payload;

	CAM_DBG(CAM_ISP, "CSID[%d] error %d",
		csid_hw->hw_intf->hw_idx, evt_type);

	rc = cam_tfe_csid_get_evt_payload(csid_hw, &evt_payload);
	if (rc) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"No free payload core %d",
			csid_hw->hw_intf->hw_idx);
		return rc;
	}

	rc = tasklet_bh_api.get_bh_payload_func(csid_hw->tasklet, &bh_cmd);
	if (rc || !bh_cmd) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"CSID[%d] Can not get cmd for tasklet, evt_type %d",
			csid_hw->hw_intf->hw_idx,
			evt_type);
		return rc;
	}

	evt_payload->evt_type = evt_type;
	evt_payload->priv = csid_hw->event_cb_priv;
	evt_payload->hw_idx = csid_hw->hw_intf->hw_idx;

	for (i = 0; i < TFE_CSID_IRQ_REG_MAX; i++)
		evt_payload->irq_status[i] = irq_status[i];

	tasklet_bh_api.bottom_half_enqueue_func(csid_hw->tasklet,
		bh_cmd,
		csid_hw,
		evt_payload,
		cam_tfe_csid_evt_bottom_half_handler);

	return rc;
}

irqreturn_t cam_tfe_csid_irq(int irq_num, void *data)
{
	struct cam_tfe_csid_hw                         *csid_hw;
	struct cam_hw_soc_info                         *soc_info;
	const struct cam_tfe_csid_reg_offset           *csid_reg;
	const struct cam_tfe_csid_csi2_rx_reg_offset   *csi2_reg;
	uint32_t                   irq_status[TFE_CSID_IRQ_REG_MAX];
	bool fatal_err_detected = false, is_error_irq = false;
	uint32_t sof_irq_debug_en = 0, log_en = 0;
	unsigned long flags;
	uint32_t i, val;

	if (!data) {
		CAM_ERR(CAM_ISP, "CSID: Invalid arguments");
		return IRQ_HANDLED;
	}

	csid_hw = (struct cam_tfe_csid_hw *)data;
	CAM_DBG(CAM_ISP, "CSID %d IRQ Handling", csid_hw->hw_intf->hw_idx);

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	csi2_reg = csid_reg->csi2_reg;

	/* read */
	irq_status[TFE_CSID_IRQ_REG_TOP] =
		cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_status_addr);

	irq_status[TFE_CSID_IRQ_REG_RX] =
		cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_status_addr);

	if (csid_hw->pxl_pipe_enable)
		irq_status[TFE_CSID_IRQ_REG_IPP] =
			cam_io_r_mb(soc_info->reg_map[0].mem_base +
				csid_reg->ipp_reg->csid_pxl_irq_status_addr);

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++)
		irq_status[i] =
		cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[i]->csid_rdi_irq_status_addr);

	/* clear */
	cam_io_w_mb(irq_status[TFE_CSID_IRQ_REG_TOP],
		soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_clear_addr);

	cam_io_w_mb(irq_status[TFE_CSID_IRQ_REG_RX],
		soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_clear_addr);

	if (csid_hw->pxl_pipe_enable)
		cam_io_w_mb(irq_status[TFE_CSID_IRQ_REG_IPP],
			soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_pxl_irq_clear_addr);

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++) {
		cam_io_w_mb(irq_status[i],
			soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_irq_clear_addr);
	}
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_irq_cmd_addr);


	/* Software register reset complete*/
	if (irq_status[TFE_CSID_IRQ_REG_TOP])
		complete(&csid_hw->csid_top_complete);

	if (irq_status[TFE_CSID_IRQ_REG_RX] &
		BIT(csid_reg->csi2_reg->csi2_rst_done_shift_val))
		complete(&csid_hw->csid_csi2_complete);

	spin_lock_irqsave(&csid_hw->spin_lock, flags);
	if (csid_hw->device_enabled == 1) {
		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_ERROR_LANE0_FIFO_OVERFLOW) {
			fatal_err_detected = true;
			goto handle_fatal_error;
		}

		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_ERROR_LANE1_FIFO_OVERFLOW) {
			fatal_err_detected = true;
			goto handle_fatal_error;
		}

		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_ERROR_LANE2_FIFO_OVERFLOW) {
			fatal_err_detected = true;
			goto handle_fatal_error;
		}
		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_ERROR_LANE3_FIFO_OVERFLOW) {
			fatal_err_detected = true;
			goto handle_fatal_error;
		}

		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_ERROR_CPHY_EOT_RECEPTION)
			csid_hw->error_irq_count++;

		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_ERROR_CPHY_SOT_RECEPTION)
			csid_hw->error_irq_count++;

		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_ERROR_STREAM_UNDERFLOW)
			csid_hw->error_irq_count++;

		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_ERROR_UNBOUNDED_FRAME)
			csid_hw->error_irq_count++;

		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_ERROR_CRC)
			is_error_irq = true;

		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_ERROR_ECC)
			is_error_irq = true;

		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_ERROR_MMAPPED_VC_DT)
			is_error_irq = true;
	}
handle_fatal_error:
	spin_unlock_irqrestore(&csid_hw->spin_lock, flags);

	if (csid_hw->error_irq_count || fatal_err_detected)
		is_error_irq = true;

	if (csid_hw->error_irq_count >
		CAM_TFE_CSID_MAX_IRQ_ERROR_COUNT) {
		fatal_err_detected = true;
		csid_hw->error_irq_count = 0;
	}

	if (fatal_err_detected) {
		/* Reset the Rx CFG registers */
		cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->csi2_reg->csid_csi2_rx_cfg0_addr);
		cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->csi2_reg->csid_csi2_rx_cfg1_addr);
		cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->csi2_reg->csid_csi2_rx_irq_mask_addr);
		/* phy_sel starts from 1 and should never be zero*/
		if (csid_hw->csi2_rx_cfg.phy_sel > 0) {
			cam_subdev_notify_message(CAM_CSIPHY_DEVICE_TYPE,
				CAM_SUBDEV_MESSAGE_IRQ_ERR,
				(csid_hw->csi2_rx_cfg.phy_sel - 1));
		}
		cam_tfe_csid_handle_hw_err_irq(csid_hw,
			CAM_ISP_HW_ERROR_CSID_FATAL, irq_status);
	}

	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_EOT_IRQ) {
		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_INFO_PHY_DL0_EOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL0_EOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_INFO_PHY_DL1_EOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL1_EOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_INFO_PHY_DL2_EOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL2_EOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_INFO_PHY_DL3_EOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL3_EOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
	}

	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_SOT_IRQ) {
		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_INFO_PHY_DL0_SOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL0_SOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_INFO_PHY_DL1_SOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL1_SOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_INFO_PHY_DL2_SOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL2_SOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_INFO_PHY_DL3_SOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL3_SOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
	}

	if ((csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE) &&
		(irq_status[TFE_CSID_IRQ_REG_RX] &
		TFE_CSID_CSI2_RX_INFO_LONG_PKT_CAPTURED)) {
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
		/* reset long pkt strobe to capture next long packet */
		val = (1 << csi2_reg->csi2_rx_long_pkt_hdr_rst_stb_shift);
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csi2_reg->csid_csi2_rx_rst_strobes_addr);
	}
	if ((csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE) &&
		(irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_INFO_SHORT_PKT_CAPTURED)) {
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
		/* reset short pkt strobe to capture next short packet */
		val = (1 << csi2_reg->csi2_rx_short_pkt_hdr_rst_stb_shift);
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csi2_reg->csid_csi2_rx_rst_strobes_addr);
	}

	if ((csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE) &&
		(irq_status[TFE_CSID_IRQ_REG_RX] &
			TFE_CSID_CSI2_RX_INFO_CPHY_PKT_HDR_CAPTURED)) {
		CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID:%d CPHY_PKT_HDR_CAPTURED",
			csid_hw->hw_intf->hw_idx);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->csid_csi2_rx_captured_cphy_pkt_hdr_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"CSID:%d cphy packet VC :%d DT:%d WC:%d",
			csid_hw->hw_intf->hw_idx,
			(val >> 22), ((val >> 16) & 0x1F), (val & 0xFFFF));
		/* reset cphy pkt strobe to capture next short packet */
		val = (1 << csi2_reg->csi2_rx_cphy_pkt_hdr_rst_stb_shift);
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csi2_reg->csid_csi2_rx_rst_strobes_addr);
	}

	if (csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_RST_IRQ_LOG) {

		if (irq_status[TFE_CSID_IRQ_REG_IPP] &
			BIT(csid_reg->cmn_reg->path_rst_done_shift_val))
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID IPP reset complete");

		if (irq_status[TFE_CSID_IRQ_REG_TOP])
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID TOP reset complete");

		if (irq_status[TFE_CSID_IRQ_REG_RX] &
			BIT(csid_reg->csi2_reg->csi2_rst_done_shift_val))
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID RX reset complete");
	}

	/* read the IPP errors */
	if (csid_hw->pxl_pipe_enable) {
		/* IPP reset done bit */
		if (irq_status[TFE_CSID_IRQ_REG_IPP] &
			BIT(csid_reg->cmn_reg->path_rst_done_shift_val)) {
			CAM_DBG(CAM_ISP, "CSID IPP reset complete");
			complete(&csid_hw->csid_ipp_complete);
		}

		if ((irq_status[TFE_CSID_IRQ_REG_IPP] &
			TFE_CSID_PATH_INFO_INPUT_SOF) &&
			(csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_SOF_IRQ)) {
			if (!csid_hw->sof_irq_triggered)
				CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d IPP SOF received",
					csid_hw->hw_intf->hw_idx);
			else
				log_en = 1;

			if (csid_hw->sof_irq_triggered)
				csid_hw->irq_debug_cnt++;
		}

		if ((irq_status[TFE_CSID_IRQ_REG_IPP] &
			TFE_CSID_PATH_INFO_INPUT_EOF) &&
			(csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_EOF_IRQ)) {
			CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID:%d IPP EOF received",
				csid_hw->hw_intf->hw_idx);
		}

		if (irq_status[TFE_CSID_IRQ_REG_IPP] &
			TFE_CSID_PATH_ERROR_FIFO_OVERFLOW) {
			/* Stop IPP path immediately */
			cam_io_w_mb(CAM_TFE_CSID_HALT_IMMEDIATELY,
				soc_info->reg_map[0].mem_base +
				csid_reg->ipp_reg->csid_pxl_ctrl_addr);
			is_error_irq = true;
		}

		if (irq_status[TFE_CSID_IRQ_REG_IPP] &
			TFE_CSID_PATH_IPP_ERROR_CCIF_VIOLATION)
			is_error_irq = true;

	}

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++) {

		if ((irq_status[i] &
			BIT(csid_reg->cmn_reg->path_rst_done_shift_val)) &&
			(csid_hw->csid_debug &
			TFE_CSID_DEBUG_ENABLE_RST_IRQ_LOG))
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d RDI%d reset complete",
				csid_hw->hw_intf->hw_idx, i);

		if (irq_status[i] &
			BIT(csid_reg->cmn_reg->path_rst_done_shift_val)) {
			CAM_DBG(CAM_ISP, "CSID:%d RDI%d reset complete",
				csid_hw->hw_intf->hw_idx, i);
			complete(&csid_hw->csid_rdin_complete[i]);
		}

		if ((irq_status[i] & TFE_CSID_PATH_INFO_INPUT_SOF) &&
			(csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_SOF_IRQ)) {
			if (!csid_hw->sof_irq_triggered)
				CAM_INFO_RATE_LIMIT(CAM_ISP,
					"CSID:%d RDI:%d SOF received",
					csid_hw->hw_intf->hw_idx, i);
			else
				log_en = 1;

			if (csid_hw->sof_irq_triggered)
				csid_hw->irq_debug_cnt++;
		}

		if ((irq_status[i] & TFE_CSID_PATH_INFO_INPUT_EOF) &&
			(csid_hw->csid_debug & TFE_CSID_DEBUG_ENABLE_EOF_IRQ)) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d RDI:%d EOF received",
				csid_hw->hw_intf->hw_idx, i);
		}

		if (irq_status[i] & TFE_CSID_PATH_ERROR_FIFO_OVERFLOW) {
			/* Stop RDI path immediately */
			is_error_irq = true;
			cam_io_w_mb(CAM_TFE_CSID_HALT_IMMEDIATELY,
				soc_info->reg_map[0].mem_base +
				csid_reg->rdi_reg[i]->csid_rdi_ctrl_addr);
		}

		if ((irq_status[i] & TFE_CSID_PATH_RDI_OVERFLOW_IRQ) ||
			(irq_status[i] &
				 TFE_CSID_PATH_RDI_ERROR_CCIF_VIOLATION))
			is_error_irq = true;
	}

	if (is_error_irq || log_en)
		cam_tfe_csid_handle_hw_err_irq(csid_hw,
			CAM_ISP_HW_ERROR_NONE, irq_status);

	if (csid_hw->irq_debug_cnt >= CAM_TFE_CSID_IRQ_SOF_DEBUG_CNT_MAX) {
		cam_tfe_csid_sof_irq_debug(csid_hw, &sof_irq_debug_en);
		csid_hw->irq_debug_cnt = 0;
	}

	CAM_DBG(CAM_ISP, "IRQ Handling exit");
	return IRQ_HANDLED;
}

int cam_tfe_csid_hw_probe_init(struct cam_hw_intf  *csid_hw_intf,
	uint32_t csid_idx)
{
	int rc = -EINVAL;
	uint32_t i, val, clk_lvl;
	struct cam_tfe_csid_path_cfg         *path_data;
	struct cam_hw_info                   *csid_hw_info;
	struct cam_tfe_csid_hw               *tfe_csid_hw = NULL;
	const struct cam_tfe_csid_reg_offset *csid_reg;

	if (csid_idx >= CAM_TFE_CSID_HW_NUM_MAX) {
		CAM_ERR(CAM_ISP, "Invalid csid index:%d", csid_idx);
		return rc;
	}

	csid_hw_info = (struct cam_hw_info  *) csid_hw_intf->hw_priv;
	tfe_csid_hw  = (struct cam_tfe_csid_hw  *) csid_hw_info->core_info;

	tfe_csid_hw->hw_intf = csid_hw_intf;
	tfe_csid_hw->hw_info = csid_hw_info;
	csid_reg = tfe_csid_hw->csid_info->csid_reg;

	CAM_DBG(CAM_ISP, "type %d index %d",
		tfe_csid_hw->hw_intf->hw_type, csid_idx);

	tfe_csid_hw->device_enabled = 0;
	tfe_csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&tfe_csid_hw->hw_info->hw_mutex);
	spin_lock_init(&tfe_csid_hw->hw_info->hw_lock);
	spin_lock_init(&tfe_csid_hw->spin_lock);
	init_completion(&tfe_csid_hw->hw_info->hw_complete);

	init_completion(&tfe_csid_hw->csid_top_complete);
	init_completion(&tfe_csid_hw->csid_csi2_complete);
	init_completion(&tfe_csid_hw->csid_ipp_complete);
	for (i = 0; i < CAM_TFE_CSID_RDI_MAX; i++)
		init_completion(&tfe_csid_hw->csid_rdin_complete[i]);

	rc = cam_tfe_csid_init_soc_resources(&tfe_csid_hw->hw_info->soc_info,
			cam_tfe_csid_irq, tfe_csid_hw);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "CSID:%d Failed to init_soc", csid_idx);
		goto err;
	}
	rc = cam_soc_util_get_clk_level(&tfe_csid_hw->hw_info->soc_info,
		tfe_csid_hw->clk_rate,
		tfe_csid_hw->hw_info->soc_info.src_clk_idx, &clk_lvl);
	CAM_DBG(CAM_ISP, "CSID clock lvl %u", clk_lvl);

	rc = cam_tfe_csid_enable_soc_resources(&tfe_csid_hw->hw_info->soc_info,
		clk_lvl);
	if (rc) {
		CAM_ERR(CAM_ISP, "CSID:%d Enable SOC failed",
			tfe_csid_hw->hw_intf->hw_idx);
		goto err;
	}

	tfe_csid_hw->hw_intf->hw_ops.get_hw_caps = cam_tfe_csid_get_hw_caps;
	tfe_csid_hw->hw_intf->hw_ops.init        = cam_tfe_csid_init_hw;
	tfe_csid_hw->hw_intf->hw_ops.deinit      = cam_tfe_csid_deinit_hw;
	tfe_csid_hw->hw_intf->hw_ops.reset       = cam_tfe_csid_reset;
	tfe_csid_hw->hw_intf->hw_ops.reserve     = cam_tfe_csid_reserve;
	tfe_csid_hw->hw_intf->hw_ops.release     = cam_tfe_csid_release;
	tfe_csid_hw->hw_intf->hw_ops.start       = cam_tfe_csid_start;
	tfe_csid_hw->hw_intf->hw_ops.stop        = cam_tfe_csid_stop;
	tfe_csid_hw->hw_intf->hw_ops.read        = cam_tfe_csid_read;
	tfe_csid_hw->hw_intf->hw_ops.write       = cam_tfe_csid_write;
	tfe_csid_hw->hw_intf->hw_ops.process_cmd = cam_tfe_csid_process_cmd;

	/* reset the cid values */
	for (i = 0; i < CAM_TFE_CSID_CID_MAX; i++) {
		tfe_csid_hw->cid_res[i].vc  = 0;
		tfe_csid_hw->cid_res[i].dt  = 0;
		tfe_csid_hw->cid_res[i].cnt = 0;
	}

	if (tfe_csid_hw->hw_intf->hw_idx == 2) {
		val = cam_io_r_mb(
			tfe_csid_hw->hw_info->soc_info.reg_map[1].mem_base +
			csid_reg->cmn_reg->top_tfe2_fuse_reg);
		if (val) {
			CAM_INFO(CAM_ISP, "TFE 2 is not supported by hardware");

			rc = cam_tfe_csid_disable_soc_resources(
				&tfe_csid_hw->hw_info->soc_info);
			if (rc)
				CAM_ERR(CAM_ISP,
					"CSID:%d Disable CSID SOC failed",
					tfe_csid_hw->hw_intf->hw_idx);
			else
				rc = -EINVAL;
			goto err;
		}
	}

	val = cam_io_r_mb(
		tfe_csid_hw->hw_info->soc_info.reg_map[1].mem_base +
		csid_reg->cmn_reg->top_tfe2_pix_pipe_fuse_reg);

	/* Initialize the IPP resources */
	if (!(val && (tfe_csid_hw->hw_intf->hw_idx == 2))) {
		CAM_DBG(CAM_ISP, "initializing the pix path");

		tfe_csid_hw->ipp_res.res_type = CAM_ISP_RESOURCE_PIX_PATH;
		tfe_csid_hw->ipp_res.res_id = CAM_TFE_CSID_PATH_RES_IPP;
		tfe_csid_hw->ipp_res.res_state =
			CAM_ISP_RESOURCE_STATE_AVAILABLE;
		tfe_csid_hw->ipp_res.hw_intf = tfe_csid_hw->hw_intf;
		path_data = kzalloc(sizeof(*path_data),
					GFP_KERNEL);
		if (!path_data) {
			rc = -ENOMEM;
			goto err;
		}
		tfe_csid_hw->ipp_res.res_priv = path_data;
		tfe_csid_hw->pxl_pipe_enable = 1;
	}

	/* Initialize the RDI resource */
	for (i = 0; i < tfe_csid_hw->csid_info->csid_reg->cmn_reg->num_rdis;
			i++) {
		/* res type is from RDI 0 to RDI2 */
		tfe_csid_hw->rdi_res[i].res_type =
			CAM_ISP_RESOURCE_PIX_PATH;
		tfe_csid_hw->rdi_res[i].res_id = i;
		tfe_csid_hw->rdi_res[i].res_state =
			CAM_ISP_RESOURCE_STATE_AVAILABLE;
		tfe_csid_hw->rdi_res[i].hw_intf = tfe_csid_hw->hw_intf;

		path_data = kzalloc(sizeof(*path_data),
			GFP_KERNEL);
		if (!path_data) {
			rc = -ENOMEM;
			goto err;
		}
		tfe_csid_hw->rdi_res[i].res_priv = path_data;
	}

	rc = cam_tasklet_init(&tfe_csid_hw->tasklet, tfe_csid_hw, csid_idx);
	if (rc) {
		CAM_ERR(CAM_ISP, "Unable to create CSID tasklet rc %d", rc);
		goto err;
	}

	INIT_LIST_HEAD(&tfe_csid_hw->free_payload_list);
	for (i = 0; i < CAM_CSID_EVT_PAYLOAD_MAX; i++) {
		INIT_LIST_HEAD(&tfe_csid_hw->evt_payload[i].list);
		list_add_tail(&tfe_csid_hw->evt_payload[i].list,
			&tfe_csid_hw->free_payload_list);
	}

	tfe_csid_hw->csid_debug = 0;
	tfe_csid_hw->error_irq_count = 0;
	tfe_csid_hw->prev_boot_timestamp = 0;

	rc = cam_tfe_csid_disable_soc_resources(
		&tfe_csid_hw->hw_info->soc_info);
	if (rc) {
		CAM_ERR(CAM_ISP, "CSID:%d Disable CSID SOC failed",
			tfe_csid_hw->hw_intf->hw_idx);
		goto err;
	}

	/* Check if ppi bridge is present or not? */
	tfe_csid_hw->ppi_enable = of_property_read_bool(
		csid_hw_info->soc_info.pdev->dev.of_node,
		"ppi-enable");

	if (!tfe_csid_hw->ppi_enable)
		return 0;

	/* Initialize the PPI bridge */
	for (i = 0; i < CAM_CSID_PPI_HW_MAX; i++) {
		rc = cam_csid_ppi_hw_init(&tfe_csid_hw->ppi_hw_intf[i], i);
		if (rc < 0) {
			CAM_INFO(CAM_ISP, "PPI init failed for PPI %d", i);
			rc = 0;
			break;
		}
	}

	return 0;
err:
	if (rc) {
		kfree(tfe_csid_hw->ipp_res.res_priv);
		for (i = 0; i <
			tfe_csid_hw->csid_info->csid_reg->cmn_reg->num_rdis;
			i++)
			kfree(tfe_csid_hw->rdi_res[i].res_priv);
	}

	return rc;
}


int cam_tfe_csid_hw_deinit(struct cam_tfe_csid_hw *tfe_csid_hw)
{
	int rc = -EINVAL;
	uint32_t i;

	if (!tfe_csid_hw) {
		CAM_ERR(CAM_ISP, "Invalid param");
		return rc;
	}

	/* release the privdate data memory from resources */
	kfree(tfe_csid_hw->ipp_res.res_priv);

	for (i = 0; i <
		tfe_csid_hw->csid_info->csid_reg->cmn_reg->num_rdis;
		i++) {
		kfree(tfe_csid_hw->rdi_res[i].res_priv);
	}

	cam_tfe_csid_deinit_soc_resources(&tfe_csid_hw->hw_info->soc_info);

	return 0;
}
