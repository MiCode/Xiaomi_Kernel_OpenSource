/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include "cam_ife_csid_core.h"
#include "cam_isp_hw.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"

/* Timeout value in msec */
#define IFE_CSID_TIMEOUT                               1000

/* TPG VC/DT values */
#define CAM_IFE_CSID_TPG_VC_VAL                        0xA
#define CAM_IFE_CSID_TPG_DT_VAL                        0x2B

/* Timeout values in usec */
#define CAM_IFE_CSID_TIMEOUT_SLEEP_US                  1000
#define CAM_IFE_CSID_TIMEOUT_ALL_US                    100000

/*
 * Constant Factors needed to change QTimer ticks to nanoseconds
 * QTimer Freq = 19.2 MHz
 * Time(us) = ticks/19.2
 * Time(ns) = ticks/19.2 * 1000
 */
#define CAM_IFE_CSID_QTIMER_MUL_FACTOR                 10000
#define CAM_IFE_CSID_QTIMER_DIV_FACTOR                 192

/* Max number of sof irq's triggered in case of SOF freeze */
#define CAM_CSID_IRQ_SOF_DEBUG_CNT_MAX 6

/* Max CSI Rx irq error count threshold value */
#define CAM_IFE_CSID_MAX_IRQ_ERROR_COUNT               100

static int cam_ife_csid_is_ipp_format_supported(
	uint32_t in_format)
{
	int rc = -EINVAL;

	switch (in_format) {
	case CAM_FORMAT_MIPI_RAW_6:
	case CAM_FORMAT_MIPI_RAW_8:
	case CAM_FORMAT_MIPI_RAW_10:
	case CAM_FORMAT_MIPI_RAW_12:
	case CAM_FORMAT_MIPI_RAW_14:
	case CAM_FORMAT_MIPI_RAW_16:
	case CAM_FORMAT_MIPI_RAW_20:
	case CAM_FORMAT_DPCM_10_6_10:
	case CAM_FORMAT_DPCM_10_8_10:
	case CAM_FORMAT_DPCM_12_6_12:
	case CAM_FORMAT_DPCM_12_8_12:
	case CAM_FORMAT_DPCM_14_8_14:
	case CAM_FORMAT_DPCM_14_10_14:
		rc = 0;
		break;
	default:
		break;
	}
	return rc;
}

static int cam_ife_csid_get_format_rdi(
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
	case CAM_FORMAT_MIPI_RAW_20:
		switch (out_format) {
		case CAM_FORMAT_MIPI_RAW_20:
			*decode_fmt = 0xf;
			break;
		case CAM_FORMAT_PLAIN32_20:
			*decode_fmt = 0x6;
			*plain_fmt = 0x2;
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_FORMAT_DPCM_10_6_10:
		*decode_fmt  = 0x7;
		*plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_10_8_10:
		*decode_fmt  = 0x8;
		*plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_12_6_12:
		*decode_fmt  = 0x9;
		*plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_12_8_12:
		*decode_fmt  = 0xA;
		*plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_14_8_14:
		*decode_fmt  = 0xB;
		*plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_14_10_14:
		*decode_fmt  = 0xC;
		*plain_fmt = 0x1;
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

static int cam_ife_csid_get_format_ipp(
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
	case CAM_FORMAT_MIPI_RAW_14:
		*decode_fmt  = 0x4;
		*plain_fmt = 0x1;
		break;
	case CAM_FORMAT_MIPI_RAW_16:
		*decode_fmt  = 0x5;
		*plain_fmt = 0x1;
		break;
	case CAM_FORMAT_MIPI_RAW_20:
		*decode_fmt  = 0x6;
		*plain_fmt = 0x2;
		break;
	case CAM_FORMAT_DPCM_10_6_10:
		*decode_fmt  = 0x7;
		*plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_10_8_10:
		*decode_fmt  = 0x8;
		*plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_12_6_12:
		*decode_fmt  = 0x9;
		*plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_12_8_12:
		*decode_fmt  = 0xA;
		*plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_14_8_14:
		*decode_fmt  = 0xB;
		*plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_14_10_14:
		*decode_fmt  = 0xC;
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

static int cam_ife_csid_cid_get(struct cam_ife_csid_hw *csid_hw,
	struct cam_isp_resource_node **res, int32_t vc, uint32_t dt)
{
	struct cam_ife_csid_cid_data    *cid_data;
	uint32_t  i = 0;

	*res = NULL;

	/* Return already reserved CID if the VC/DT matches */
	for (i = 0; i < CAM_IFE_CSID_CID_RES_MAX; i++) {
		if (csid_hw->cid_res[i].res_state >=
			CAM_ISP_RESOURCE_STATE_RESERVED) {
			cid_data = (struct cam_ife_csid_cid_data *)
				csid_hw->cid_res[i].res_priv;
			if (cid_data->vc == vc && cid_data->dt == dt) {
				cid_data->cnt++;
				*res = &csid_hw->cid_res[i];
				return 0;
			}
		}
	}

	for (i = 0; i < CAM_IFE_CSID_CID_RES_MAX; i++) {
		if (csid_hw->cid_res[i].res_state ==
			CAM_ISP_RESOURCE_STATE_AVAILABLE) {
			cid_data = (struct cam_ife_csid_cid_data *)
				csid_hw->cid_res[i].res_priv;
			cid_data->vc  = vc;
			cid_data->dt  = dt;
			cid_data->cnt = 1;
			csid_hw->cid_res[i].res_state =
				CAM_ISP_RESOURCE_STATE_RESERVED;
			*res = &csid_hw->cid_res[i];
			CAM_DBG(CAM_ISP, "CSID:%d CID %d allocated",
				csid_hw->hw_intf->hw_idx,
				csid_hw->cid_res[i].res_id);
			return 0;
		}
	}

	CAM_ERR(CAM_ISP, "CSID:%d Free cid is not available",
		 csid_hw->hw_intf->hw_idx);

	return -EINVAL;
}


static int cam_ife_csid_global_reset(struct cam_ife_csid_hw *csid_hw)
{
	struct cam_hw_soc_info          *soc_info;
	struct cam_ife_csid_reg_offset  *csid_reg;
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

	if (csid_reg->cmn_reg->no_pix)
		cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_irq_mask_addr);

	for (i = 0; i < csid_reg->cmn_reg->no_rdis; i++)
		cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_irq_mask_addr);

	/* clear all interrupts */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_clear_addr);

	cam_io_w_mb(csid_reg->csi2_reg->csi2_irq_mask_all,
		soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_clear_addr);

	if (csid_reg->cmn_reg->no_pix)
		cam_io_w_mb(csid_reg->cmn_reg->ipp_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_irq_clear_addr);

	for (i = 0 ; i < csid_reg->cmn_reg->no_rdis; i++)
		cam_io_w_mb(csid_reg->cmn_reg->rdi_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_irq_clear_addr);

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_irq_cmd_addr);

	cam_io_w_mb(0x80, soc_info->reg_map[0].mem_base +
		csid_hw->csid_info->csid_reg->csi2_reg->csid_csi2_rx_cfg1_addr);

	/* enable the IPP and RDI format measure */
	if (csid_reg->cmn_reg->no_pix)
		cam_io_w_mb(0x1, soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_cfg0_addr);

	for (i = 0; i < csid_reg->cmn_reg->no_rdis; i++)
		cam_io_w_mb(0x2, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_cfg0_addr);

	/* perform the top CSID HW and SW registers reset */
	cam_io_w_mb(csid_reg->cmn_reg->csid_rst_stb_sw_all,
		soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_rst_strobes_addr);

	rc = readl_poll_timeout(soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_status_addr,
			status, (status & 0x1) == 0x1,
		CAM_IFE_CSID_TIMEOUT_SLEEP_US, CAM_IFE_CSID_TIMEOUT_ALL_US);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "CSID:%d csid_reset fail rc = %d",
			  csid_hw->hw_intf->hw_idx, rc);
		rc = -ETIMEDOUT;
	}

	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_mask_addr);
	if (val != 0)
		CAM_ERR(CAM_ISP, "CSID:%d IRQ value after reset rc = %d",
			csid_hw->hw_intf->hw_idx, val);
	csid_hw->error_irq_count = 0;

	return rc;
}

static int cam_ife_csid_path_reset(struct cam_ife_csid_hw *csid_hw,
	struct cam_csid_reset_cfg_args  *reset)
{
	int rc = 0;
	struct cam_hw_soc_info              *soc_info;
	struct cam_isp_resource_node        *res;
	struct cam_ife_csid_reg_offset      *csid_reg;
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

	if (res->res_id >= CAM_IFE_PIX_PATH_RES_MAX) {
		CAM_DBG(CAM_ISP, "CSID:%d Invalid res id%d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ISP, "CSID:%d resource:%d",
		csid_hw->hw_intf->hw_idx, res->res_id);

	if (res->res_id == CAM_IFE_PIX_PATH_RES_IPP) {
		if (!csid_reg->ipp_reg) {
			CAM_ERR(CAM_ISP, "CSID:%d IPP not supported :%d",
				 csid_hw->hw_intf->hw_idx,
				res->res_id);
			return -EINVAL;
		}

		reset_strb_addr = csid_reg->ipp_reg->csid_ipp_rst_strobes_addr;
		complete = &csid_hw->csid_ipp_complete;

		/* Enable path reset done interrupt */
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_irq_mask_addr);
		val |= CSID_PATH_INFO_RST_DONE;
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			 csid_reg->ipp_reg->csid_ipp_irq_mask_addr);

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

		/* Enable path reset done interrupt */
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_irq_mask_addr);
		val |= CSID_PATH_INFO_RST_DONE;
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_irq_mask_addr);
	}

	init_completion(complete);
	reset_strb_val = csid_reg->cmn_reg->path_rst_stb_all;

	/* Enable the Test gen before reset */
	cam_io_w_mb(1,	csid_hw->hw_info->soc_info.reg_map[0].mem_base +
		csid_reg->tpg_reg->csid_tpg_ctrl_addr);

	/* Reset the corresponding ife csid path */
	cam_io_w_mb(reset_strb_val, soc_info->reg_map[0].mem_base +
				reset_strb_addr);

	rc = wait_for_completion_timeout(complete,
		msecs_to_jiffies(IFE_CSID_TIMEOUT));
	if (rc <= 0) {
		CAM_ERR(CAM_ISP, "CSID:%d Res id %d fail rc = %d",
			 csid_hw->hw_intf->hw_idx,
			res->res_id,  rc);
		if (rc == 0)
			rc = -ETIMEDOUT;
	}

	/* Disable Test Gen after reset*/
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->tpg_reg->csid_tpg_ctrl_addr);

end:
	return rc;

}

static int cam_ife_csid_cid_reserve(struct cam_ife_csid_hw *csid_hw,
	struct cam_csid_hw_reserve_resource_args  *cid_reserv)
{
	int rc = 0;
	struct cam_ife_csid_cid_data       *cid_data;
	uint32_t camera_hw_version;

	CAM_DBG(CAM_ISP,
		"CSID:%d res_sel:0x%x Lane type:%d lane_num:%d dt:%d vc:%d",
		csid_hw->hw_intf->hw_idx,
		cid_reserv->in_port->res_type,
		cid_reserv->in_port->lane_type,
		cid_reserv->in_port->lane_num,
		cid_reserv->in_port->dt,
		cid_reserv->in_port->vc);

	if (cid_reserv->in_port->res_type >= CAM_ISP_IFE_IN_RES_MAX) {
		CAM_ERR(CAM_ISP, "CSID:%d  Invalid phy sel %d",
			csid_hw->hw_intf->hw_idx,
			cid_reserv->in_port->res_type);
		rc = -EINVAL;
		goto end;
	}

	if (cid_reserv->in_port->lane_type >= CAM_ISP_LANE_TYPE_MAX &&
		cid_reserv->in_port->res_type != CAM_ISP_IFE_IN_RES_TPG) {
		CAM_ERR(CAM_ISP, "CSID:%d  Invalid lane type %d",
			csid_hw->hw_intf->hw_idx,
			cid_reserv->in_port->lane_type);
		rc = -EINVAL;
		goto end;
	}

	if ((cid_reserv->in_port->lane_type ==  CAM_ISP_LANE_TYPE_DPHY &&
		cid_reserv->in_port->lane_num > 4) &&
		cid_reserv->in_port->res_type != CAM_ISP_IFE_IN_RES_TPG) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid lane num %d",
			csid_hw->hw_intf->hw_idx,
			cid_reserv->in_port->lane_num);
		rc = -EINVAL;
		goto end;
	}
	if ((cid_reserv->in_port->lane_type == CAM_ISP_LANE_TYPE_CPHY &&
		cid_reserv->in_port->lane_num > 3) &&
		cid_reserv->in_port->res_type != CAM_ISP_IFE_IN_RES_TPG) {
		CAM_ERR(CAM_ISP, " CSID:%d Invalid lane type %d & num %d",
			 csid_hw->hw_intf->hw_idx,
			cid_reserv->in_port->lane_type,
			cid_reserv->in_port->lane_num);
		rc = -EINVAL;
		goto end;
	}

	/* CSID  CSI2 v2.0 supports 31 vc  */
	if (cid_reserv->in_port->dt > 0x3f ||
		cid_reserv->in_port->vc > 0x1f) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid vc:%d dt %d",
			csid_hw->hw_intf->hw_idx,
			cid_reserv->in_port->vc, cid_reserv->in_port->dt);
		rc = -EINVAL;
		goto end;
	}

	if (cid_reserv->in_port->res_type == CAM_ISP_IFE_IN_RES_TPG && (
		(cid_reserv->in_port->format < CAM_FORMAT_MIPI_RAW_8 &&
		cid_reserv->in_port->format > CAM_FORMAT_MIPI_RAW_16))) {
		CAM_ERR(CAM_ISP, " CSID:%d Invalid tpg decode fmt %d",
			 csid_hw->hw_intf->hw_idx,
			cid_reserv->in_port->format);
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

	rc = cam_cpas_get_cpas_hw_version(&camera_hw_version);
	if (rc) {
		CAM_ERR(CAM_ISP, "Failed to get HW version rc:%d", rc);
		goto end;
	}
	CAM_DBG(CAM_ISP, "HW version: %d", camera_hw_version);

	switch (camera_hw_version) {
	case CAM_CPAS_TITAN_NONE:
	case CAM_CPAS_TITAN_MAX:
		CAM_ERR(CAM_ISP, "Invalid HW version: %d", camera_hw_version);
		break;
	case CAM_CPAS_TITAN_170_V100:
	case CAM_CPAS_TITAN_170_V110:
	case CAM_CPAS_TITAN_170_V120:
		if (cid_reserv->in_port->res_type == CAM_ISP_IFE_IN_RES_PHY_3 &&
			csid_hw->hw_intf->hw_idx != 2) {
			rc = -EINVAL;
			goto end;
		}
		break;
	default:
		break;
	}
	CAM_DBG(CAM_ISP, "Reserve_cnt %u", csid_hw->csi2_reserve_cnt);

	if (csid_hw->csi2_reserve_cnt) {
		/* current configure res type should match requested res type */
		if (csid_hw->res_type != cid_reserv->in_port->res_type) {
			rc = -EINVAL;
			goto end;
		}

		if (cid_reserv->in_port->res_type != CAM_ISP_IFE_IN_RES_TPG) {
			if (csid_hw->csi2_rx_cfg.lane_cfg !=
				cid_reserv->in_port->lane_cfg  ||
				csid_hw->csi2_rx_cfg.lane_type !=
				cid_reserv->in_port->lane_type ||
				csid_hw->csi2_rx_cfg.lane_num !=
				cid_reserv->in_port->lane_num) {
				rc = -EINVAL;
				goto end;
				}
		} else {
			if (csid_hw->tpg_cfg.in_format !=
				cid_reserv->in_port->format     ||
				csid_hw->tpg_cfg.width !=
				cid_reserv->in_port->left_width ||
				csid_hw->tpg_cfg.height !=
				cid_reserv->in_port->height     ||
				csid_hw->tpg_cfg.test_pattern !=
				cid_reserv->in_port->test_pattern) {
				rc = -EINVAL;
				goto end;
			}
		}
	}

	switch (cid_reserv->res_id) {
	case CAM_IFE_PIX_PATH_RES_IPP:
		if (csid_hw->ipp_res.res_state !=
			CAM_ISP_RESOURCE_STATE_AVAILABLE) {
			CAM_DBG(CAM_ISP,
				"CSID:%d IPP resource not available",
				csid_hw->hw_intf->hw_idx);
			rc = -EINVAL;
			goto end;
		}
		break;
	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
		if (csid_hw->rdi_res[cid_reserv->res_id].res_state !=
			CAM_ISP_RESOURCE_STATE_AVAILABLE) {
			CAM_ERR(CAM_ISP,
				"CSID:%d RDI:%d resource not available",
				csid_hw->hw_intf->hw_idx,
				cid_reserv->res_id);
			rc = -EINVAL;
			goto end;
		}
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID%d: Invalid csid path",
			csid_hw->hw_intf->hw_idx);
		rc = -EINVAL;
		goto end;
	}

	rc = cam_ife_csid_cid_get(csid_hw,
		&cid_reserv->node_res,
		cid_reserv->in_port->vc,
		cid_reserv->in_port->dt);
	if (rc) {
		CAM_ERR(CAM_ISP, "CSID:%d CID Reserve failed res_type %d",
			csid_hw->hw_intf->hw_idx,
			cid_reserv->in_port->res_type);
		goto end;
	}
	cid_data = (struct cam_ife_csid_cid_data *)
		cid_reserv->node_res->res_priv;

	if (!csid_hw->csi2_reserve_cnt) {
		csid_hw->res_type = cid_reserv->in_port->res_type;

		csid_hw->csi2_rx_cfg.lane_cfg =
			cid_reserv->in_port->lane_cfg;
		csid_hw->csi2_rx_cfg.lane_type =
			cid_reserv->in_port->lane_type;
		csid_hw->csi2_rx_cfg.lane_num =
			cid_reserv->in_port->lane_num;

		if (cid_reserv->in_port->res_type == CAM_ISP_IFE_IN_RES_TPG) {
			csid_hw->csi2_rx_cfg.phy_sel = 0;
			if (cid_reserv->in_port->format >
			    CAM_FORMAT_MIPI_RAW_16) {
				CAM_ERR(CAM_ISP, " Wrong TPG format");
				rc = -EINVAL;
				goto end;
			}
			csid_hw->tpg_cfg.in_format =
				cid_reserv->in_port->format;
			csid_hw->tpg_cfg.usage_type =
				cid_reserv->in_port->usage_type;
			if (cid_reserv->in_port->usage_type)
				csid_hw->tpg_cfg.width =
					(cid_reserv->in_port->right_stop + 1);
			else
				csid_hw->tpg_cfg.width =
					cid_reserv->in_port->left_width;

			csid_hw->tpg_cfg.height = cid_reserv->in_port->height;
			csid_hw->tpg_cfg.test_pattern =
				cid_reserv->in_port->test_pattern;

			CAM_DBG(CAM_ISP, "CSID:%d TPG width:%d height:%d",
				csid_hw->hw_intf->hw_idx,
				csid_hw->tpg_cfg.width,
				csid_hw->tpg_cfg.height);

			cid_data->tpg_set = 1;
		} else {
			csid_hw->csi2_rx_cfg.phy_sel =
				(cid_reserv->in_port->res_type & 0xFF) - 1;
		}
	}

	csid_hw->csi2_reserve_cnt++;
	CAM_DBG(CAM_ISP, "CSID:%d CID:%d acquired",
		csid_hw->hw_intf->hw_idx,
		cid_reserv->node_res->res_id);

end:
	return rc;
}


static int cam_ife_csid_path_reserve(struct cam_ife_csid_hw *csid_hw,
	struct cam_csid_hw_reserve_resource_args  *reserve)
{
	int rc = 0;
	struct cam_ife_csid_path_cfg    *path_data;
	struct cam_isp_resource_node    *res;

	/* CSID  CSI2 v2.0 supports 31 vc */
	if (reserve->in_port->dt > 0x3f || reserve->in_port->vc > 0x1f ||
		(reserve->sync_mode >= CAM_ISP_HW_SYNC_MAX)) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid vc:%d dt %d mode:%d",
			 csid_hw->hw_intf->hw_idx,
			reserve->in_port->vc, reserve->in_port->dt,
			reserve->sync_mode);
		rc = -EINVAL;
		goto end;
	}

	switch (reserve->res_id) {
	case CAM_IFE_PIX_PATH_RES_IPP:
		if (csid_hw->ipp_res.res_state !=
			CAM_ISP_RESOURCE_STATE_AVAILABLE) {
			CAM_DBG(CAM_ISP,
				"CSID:%d IPP resource not available %d",
				csid_hw->hw_intf->hw_idx,
				csid_hw->ipp_res.res_state);
			rc = -EINVAL;
			goto end;
		}

		if (cam_ife_csid_is_ipp_format_supported(
				reserve->in_port->format)) {
			CAM_ERR(CAM_ISP,
				"CSID:%d res id:%d un support format %d",
				csid_hw->hw_intf->hw_idx, reserve->res_id,
				reserve->in_port->format);
			rc = -EINVAL;
			goto end;
		}

		/* assign the IPP resource */
		res = &csid_hw->ipp_res;
		CAM_DBG(CAM_ISP,
			"CSID:%d IPP resource:%d acquired successfully",
			csid_hw->hw_intf->hw_idx, res->res_id);

			break;
	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
		if (csid_hw->rdi_res[reserve->res_id].res_state !=
			CAM_ISP_RESOURCE_STATE_AVAILABLE) {
			CAM_DBG(CAM_ISP,
				"CSID:%d RDI:%d resource not available %d",
				csid_hw->hw_intf->hw_idx,
				reserve->res_id,
				csid_hw->rdi_res[reserve->res_id].res_state);
			rc = -EINVAL;
			goto end;
		} else {
			res = &csid_hw->rdi_res[reserve->res_id];
			CAM_DBG(CAM_ISP,
				"CSID:%d RDI resource:%d acquire success",
				csid_hw->hw_intf->hw_idx,
				res->res_id);
		}

		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res id:%d",
			csid_hw->hw_intf->hw_idx, reserve->res_id);
		rc = -EINVAL;
		goto end;
	}

	res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	path_data = (struct cam_ife_csid_path_cfg   *)res->res_priv;

	path_data->cid = reserve->cid;
	path_data->in_format = reserve->in_port->format;
	path_data->out_format = reserve->out_port->format;
	path_data->sync_mode = reserve->sync_mode;
	path_data->height  = reserve->in_port->height;
	path_data->start_line = reserve->in_port->line_start;
	path_data->end_line = reserve->in_port->line_stop;

	/* Enable RDI crop for single ife use case only */
	switch (reserve->res_id) {
	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
		if (reserve->in_port->usage_type)
			path_data->crop_enable = false;
		else
			path_data->crop_enable = true;

		break;
	case CAM_IFE_PIX_PATH_RES_IPP:
		path_data->crop_enable = true;
		break;
	default:
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ISP,
		"Res id: %d height:%d line_start %d line_stop %d crop_en %d",
		reserve->res_id, reserve->in_port->height,
		reserve->in_port->line_start, reserve->in_port->line_stop,
		path_data->crop_enable);

	if (reserve->in_port->res_type == CAM_ISP_IFE_IN_RES_TPG) {
		path_data->dt = CAM_IFE_CSID_TPG_DT_VAL;
		path_data->vc = CAM_IFE_CSID_TPG_VC_VAL;
	} else {
		path_data->dt = reserve->in_port->dt;
		path_data->vc = reserve->in_port->vc;
	}

	if (reserve->sync_mode == CAM_ISP_HW_SYNC_MASTER) {
		path_data->start_pixel = reserve->in_port->left_start;
		path_data->end_pixel = reserve->in_port->left_stop;
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
		path_data->end_pixel = reserve->in_port->right_stop;
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
		path_data->end_pixel = reserve->in_port->left_stop;
		CAM_DBG(CAM_ISP, "Res id: %d left width %d start: %d stop:%d",
			reserve->res_id, reserve->in_port->left_width,
			reserve->in_port->left_start,
			reserve->in_port->left_stop);
	}

	CAM_DBG(CAM_ISP, "Res %d width %d height %d", reserve->res_id,
		path_data->width, path_data->height);
	reserve->node_res = res;

end:
	return rc;
}

static int cam_ife_csid_enable_hw(struct cam_ife_csid_hw  *csid_hw)
{
	int rc = 0;
	struct cam_ife_csid_reg_offset      *csid_reg;
	struct cam_hw_soc_info              *soc_info;
	uint32_t i, val;

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

	rc = cam_ife_csid_enable_soc_resources(soc_info);
	if (rc) {
		CAM_ERR(CAM_ISP, "CSID:%d Enable SOC failed",
			csid_hw->hw_intf->hw_idx);
		goto err;
	}

	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_UP;
	/* Reset CSID top */
	rc = cam_ife_csid_global_reset(csid_hw);
	if (rc) {
		goto disable_soc;
	}

	/* clear all interrupts */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_clear_addr);

	cam_io_w_mb(csid_reg->csi2_reg->csi2_irq_mask_all,
		soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_clear_addr);

	if (csid_reg->cmn_reg->no_pix)
		cam_io_w_mb(csid_reg->cmn_reg->ipp_irq_mask_all,
			soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_irq_clear_addr);

	for (i = 0; i < csid_reg->cmn_reg->no_rdis; i++)
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
	cam_ife_csid_disable_soc_resources(soc_info);
	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
err:
	csid_hw->hw_info->open_count--;
	return rc;
}

static int cam_ife_csid_disable_hw(struct cam_ife_csid_hw *csid_hw)
{
	int rc = -EINVAL;
	struct cam_hw_soc_info             *soc_info;
	struct cam_ife_csid_reg_offset     *csid_reg;

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

	CAM_DBG(CAM_ISP, "%s:Calling Global Reset\n", __func__);
	cam_ife_csid_global_reset(csid_hw);
	CAM_DBG(CAM_ISP, "%s:Global Reset Done\n", __func__);

	CAM_DBG(CAM_ISP, "CSID:%d De-init CSID HW",
		csid_hw->hw_intf->hw_idx);

	/*disable the top IRQ interrupt */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_mask_addr);

	rc = cam_ife_csid_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_ISP, "CSID:%d Disable CSID SOC failed",
			csid_hw->hw_intf->hw_idx);

	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	csid_hw->error_irq_count = 0;
	return rc;
}


static int cam_ife_csid_tpg_start(struct cam_ife_csid_hw   *csid_hw,
	struct cam_isp_resource_node       *res)
{
	int rc = 0;
	uint32_t  val = 0;
	struct cam_hw_soc_info    *soc_info;
	struct cam_ife_csid_reg_offset *csid_reg = NULL;

	csid_hw->tpg_start_cnt++;
	if (csid_hw->tpg_start_cnt == 1) {
		/*Enable the TPG */
		CAM_DBG(CAM_ISP, "CSID:%d start CSID TPG",
			csid_hw->hw_intf->hw_idx);

		soc_info = &csid_hw->hw_info->soc_info;
		{
			uint32_t val;
			uint32_t i;
			uint32_t base = 0x600;

			CAM_DBG(CAM_ISP, "================ TPG ============");
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
		}

		/* Enable the IFE force clock on for dual isp case */
		csid_reg = csid_hw->csid_info->csid_reg;
		if (csid_hw->tpg_cfg.usage_type) {
			rc = cam_ife_csid_enable_ife_force_clock_on(soc_info,
				csid_reg->tpg_reg->tpg_cpas_ife_reg_offset);
			if (rc)
				return rc;
		}

		CAM_DBG(CAM_ISP, "============ TPG control ============");
		val = (4 << 20);
		val |= (0x80 << 8);
		val |= (((csid_hw->csi2_rx_cfg.lane_num - 1) & 0x3) << 4);
		val |= 7;

		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->tpg_reg->csid_tpg_ctrl_addr);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base + 0x600);
		CAM_DBG(CAM_ISP, "reg 0x%x = 0x%x", 0x600, val);
	}

	return 0;
}

static int cam_ife_csid_tpg_stop(struct cam_ife_csid_hw   *csid_hw,
	struct cam_isp_resource_node       *res)
{
	int rc = 0;
	struct cam_hw_soc_info    *soc_info;
	struct cam_ife_csid_reg_offset *csid_reg = NULL;

	if (csid_hw->tpg_start_cnt)
		csid_hw->tpg_start_cnt--;

	if (csid_hw->tpg_start_cnt)
		return 0;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = csid_hw->csid_info->csid_reg;

	/* disable the TPG */
	if (!csid_hw->tpg_start_cnt) {
		CAM_DBG(CAM_ISP, "CSID:%d stop CSID TPG",
			csid_hw->hw_intf->hw_idx);

		/* Disable the IFE force clock on for dual isp case */
		if (csid_hw->tpg_cfg.usage_type)
			rc = cam_ife_csid_disable_ife_force_clock_on(soc_info,
				csid_reg->tpg_reg->tpg_cpas_ife_reg_offset);

		/*stop the TPG */
		cam_io_w_mb(0,  soc_info->reg_map[0].mem_base +
		csid_hw->csid_info->csid_reg->tpg_reg->csid_tpg_ctrl_addr);
	}

	return 0;
}


static int cam_ife_csid_config_tpg(struct cam_ife_csid_hw   *csid_hw,
	struct cam_isp_resource_node       *res)
{
	struct cam_ife_csid_reg_offset *csid_reg;
	struct cam_hw_soc_info         *soc_info;
	uint32_t val = 0;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	CAM_DBG(CAM_ISP, "CSID:%d TPG config",
		csid_hw->hw_intf->hw_idx);

	/* configure one DT, infinite frames */
	val = (0 << 16) | (1 << 10) | CAM_IFE_CSID_TPG_VC_VAL;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->tpg_reg->csid_tpg_vc_cfg0_addr);

	/* vertical blanking count = 0x3FF, horzontal blanking count = 0x740*/
	val = (0x3FF << 12) | 0x740;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->tpg_reg->csid_tpg_vc_cfg1_addr);

	cam_io_w_mb(0x12345678, soc_info->reg_map[0].mem_base +
		csid_hw->csid_info->csid_reg->tpg_reg->csid_tpg_lfsr_seed_addr);

	val = csid_hw->tpg_cfg.width << 16 |
		csid_hw->tpg_cfg.height;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->tpg_reg->csid_tpg_dt_n_cfg_0_addr);

	cam_io_w_mb(CAM_IFE_CSID_TPG_DT_VAL, soc_info->reg_map[0].mem_base +
		csid_reg->tpg_reg->csid_tpg_dt_n_cfg_1_addr);

	/*
	 * in_format is the same as the input resource format.
	 * it is one larger than the register spec format.
	 */
	val = ((csid_hw->tpg_cfg.in_format - 1) << 16) | 0x8;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->tpg_reg->csid_tpg_dt_n_cfg_2_addr);

	/* static frame with split color bar */
	val =  1 << 5;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->tpg_reg->csid_tpg_color_bars_cfg_addr);
	/* config pix pattern */
	cam_io_w_mb(csid_hw->tpg_cfg.test_pattern,
		soc_info->reg_map[0].mem_base +
		csid_reg->tpg_reg->csid_tpg_common_gen_cfg_addr);

	return 0;
}

static int cam_ife_csid_enable_csi2(
	struct cam_ife_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	struct cam_ife_csid_reg_offset       *csid_reg;
	struct cam_hw_soc_info               *soc_info;
	struct cam_ife_csid_cid_data         *cid_data;
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

	cid_data = (struct cam_ife_csid_cid_data *)res->res_priv;

	res->res_state  = CAM_ISP_RESOURCE_STATE_STREAMING;
	csid_hw->csi2_cfg_cnt++;
	if (csid_hw->csi2_cfg_cnt > 1)
		return rc;

	/* rx cfg0 */
	val = (csid_hw->csi2_rx_cfg.lane_num - 1)  |
		(csid_hw->csi2_rx_cfg.lane_cfg << 4) |
		(csid_hw->csi2_rx_cfg.lane_type << 24);
	val |= (csid_hw->csi2_rx_cfg.phy_sel & 0x3) << 20;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_cfg0_addr);

	/* rx cfg1*/
	val = (1 << csid_reg->csi2_reg->csi2_misr_enable_shift_val);
	/* if VC value is more than 3 than set full width of VC */
	if (cid_data->vc > 3)
		val |= (1 << csid_reg->csi2_reg->csi2_vc_mode_shift_val);

	/* enable packet ecc correction */
	val |= 1;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_cfg1_addr);

	if (csid_hw->res_type == CAM_ISP_IFE_IN_RES_TPG) {
		/* Config the TPG */
		rc = cam_ife_csid_config_tpg(csid_hw, res);
		if (rc) {
			res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
			return rc;
		}
	}

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

static int cam_ife_csid_disable_csi2(
	struct cam_ife_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res)
{
	struct cam_ife_csid_reg_offset      *csid_reg;
	struct cam_hw_soc_info              *soc_info;

	if (res->res_id >= CAM_IFE_CSID_CID_MAX) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res id :%d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	CAM_DBG(CAM_ISP, "CSID:%d cnt : %d Disable csi2 rx",
		csid_hw->hw_intf->hw_idx, csid_hw->csi2_cfg_cnt);

	if (csid_hw->csi2_cfg_cnt)
		csid_hw->csi2_cfg_cnt--;

	if (csid_hw->csi2_cfg_cnt)
		return 0;

	/*Disable the CSI2 rx inerrupts */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_mask_addr);

	res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	return 0;
}

static int cam_ife_csid_init_config_ipp_path(
	struct cam_ife_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	struct cam_ife_csid_path_cfg           *path_data;
	struct cam_ife_csid_reg_offset         *csid_reg;
	struct cam_hw_soc_info                 *soc_info;
	uint32_t decode_format = 0, plain_format = 0, val = 0;

	path_data = (struct cam_ife_csid_path_cfg  *) res->res_priv;
	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (!csid_reg->ipp_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d IPP:%d is not supported on HW",
			csid_hw->hw_intf->hw_idx,
			res->res_id);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "Config IPP Path");
	rc = cam_ife_csid_get_format_ipp(path_data->in_format,
		&decode_format, &plain_format);
	if (rc)
		return rc;

	/*
	 * configure the IPP and enable the time stamp capture.
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
		(1 << 1) | 1;
	val |= (1 << csid_reg->ipp_reg->pix_store_en_shift_val);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_cfg0_addr);

	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_cfg1_addr);

	/* select the post irq sub sample strobe for time stamp capture */
	val |= CSID_TIMESTAMP_STB_POST_IRQ;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_cfg1_addr);

	if (path_data->crop_enable) {
		val = (((path_data->end_pixel & 0xFFFF) <<
			csid_reg->cmn_reg->crop_shift) |
			(path_data->start_pixel & 0xFFFF));
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_hcrop_addr);
		CAM_DBG(CAM_ISP, "CSID:%d Horizontal crop config val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);

		val = (((path_data->end_line & 0xFFFF) <<
			csid_reg->cmn_reg->crop_shift) |
			(path_data->start_line & 0xFFFF));
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_vcrop_addr);
		CAM_DBG(CAM_ISP, "CSID:%d Vertical Crop config val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);

		/* Enable generating early eof strobe based on crop config */
		if (!(csid_hw->csid_debug & CSID_DEBUG_DISABLE_EARLY_EOF)) {
			val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
				csid_reg->ipp_reg->csid_ipp_cfg0_addr);
			val |= (1 <<
				csid_reg->ipp_reg->early_eof_en_shift_val);
			cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
				csid_reg->ipp_reg->csid_ipp_cfg0_addr);
		}
	}

	/* set frame drop pattern to 0 and period to 1 */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_frm_drop_period_addr);
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_frm_drop_pattern_addr);
	/* set irq sub sample pattern to 0 and period to 1 */
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_irq_subsample_period_addr);
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_irq_subsample_pattern_addr);
	/* set pixel drop pattern to 0 and period to 1 */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_pix_drop_pattern_addr);
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_pix_drop_period_addr);
	/* set line drop pattern to 0 and period to 1 */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_line_drop_pattern_addr);
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_line_drop_period_addr);


	/* Enable the IPP path */
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_cfg0_addr);
	val |= (1 << csid_reg->cmn_reg->path_en_shift_val);

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_HBI_VBI_INFO)
		val |= csid_reg->cmn_reg->format_measure_en_val;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_cfg0_addr);

	/* Enable the HBI/VBI counter */
	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_HBI_VBI_INFO) {
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_format_measure_cfg0_addr);
		val |= csid_reg->cmn_reg->measure_en_hbi_vbi_cnt_mask;
		cam_io_w_mb(val,
			soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_format_measure_cfg0_addr);
	}

	/* configure the rx packet capture based on csid debug set */
	val = 0;
	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE)
		val = ((1 <<
			csid_reg->csi2_reg->csi2_capture_short_pkt_en_shift) |
			(path_data->vc <<
			csid_reg->csi2_reg->csi2_capture_short_pkt_vc_shift));

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE)
		val |= ((1 <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_en_shift) |
			(path_data->dt <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_dt_shift) |
			(path_data->vc <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_vc_shift));

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE)
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

static int cam_ife_csid_deinit_ipp_path(
	struct cam_ife_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	uint32_t val = 0;
	struct cam_ife_csid_reg_offset      *csid_reg;
	struct cam_hw_soc_info              *soc_info;
	struct cam_ife_csid_ipp_reg_offset  *ipp_reg;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (res->res_state != CAM_ISP_RESOURCE_STATE_INIT_HW) {
		CAM_ERR(CAM_ISP,
			"CSID:%d Res type %d res_id:%d in wrong state %d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		rc = -EINVAL;
	}

	if (!csid_reg->ipp_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d IPP %d is not supported on HW",
			csid_hw->hw_intf->hw_idx,
			res->res_id);
		rc = -EINVAL;
		goto end;
	}

	ipp_reg = csid_reg->ipp_reg;
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			ipp_reg->csid_ipp_cfg0_addr);
	if (val & csid_reg->cmn_reg->format_measure_en_val) {
		val &= ~csid_reg->cmn_reg->format_measure_en_val;
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			ipp_reg->csid_ipp_cfg0_addr);

		/* Disable the HBI/VBI counter */
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			ipp_reg->csid_ipp_format_measure_cfg0_addr);
		val &= ~csid_reg->cmn_reg->measure_en_hbi_vbi_cnt_mask;
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			ipp_reg->csid_ipp_format_measure_cfg0_addr);
	}

end:
	res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	return rc;
}

static int cam_ife_csid_enable_ipp_path(
	struct cam_ife_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res)
{
	struct cam_ife_csid_reg_offset    *csid_reg;
	struct cam_hw_soc_info            *soc_info;
	struct cam_ife_csid_path_cfg      *path_data;
	uint32_t val = 0;

	path_data = (struct cam_ife_csid_path_cfg   *) res->res_priv;
	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (res->res_state != CAM_ISP_RESOURCE_STATE_INIT_HW) {
		CAM_ERR(CAM_ISP,
			"CSID:%d res type:%d res_id:%d Invalid state%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	if (!csid_reg->ipp_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d IPP %d not supported on HW",
			csid_hw->hw_intf->hw_idx,
			res->res_id);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "Enable IPP path");


	/* Set master or slave IPP */
	if (path_data->sync_mode == CAM_ISP_HW_SYNC_MASTER)
		/*Set halt mode as master */
		val = CSID_HALT_MODE_MASTER << 2;
	else if (path_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE)
		/*Set halt mode as slave and set master idx */
		val = path_data->master_idx  << 4 | CSID_HALT_MODE_SLAVE << 2;
	else
		/* Default is internal halt mode */
		val = 0;

	/*
	 * Resume at frame boundary if Master or No Sync.
	 * Slave will get resume command from Master.
	 */
	if (path_data->sync_mode == CAM_ISP_HW_SYNC_MASTER ||
		path_data->sync_mode == CAM_ISP_HW_SYNC_NONE)
		val |= CAM_CSID_RESUME_AT_FRAME_BOUNDARY;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_ctrl_addr);

	CAM_DBG(CAM_ISP, "CSID:%d IPP Ctrl val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);

	/* Enable the required ipp interrupts */
	val = CSID_PATH_INFO_RST_DONE | CSID_PATH_ERROR_FIFO_OVERFLOW;

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_SOF_IRQ)
		val |= CSID_PATH_INFO_INPUT_SOF;
	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_EOF_IRQ)
		val |= CSID_PATH_INFO_INPUT_EOF;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_irq_mask_addr);
	CAM_DBG(CAM_ISP, "Enable IPP IRQ mask 0x%x", val);

	res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	return 0;
}

static int cam_ife_csid_disable_ipp_path(
	struct cam_ife_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res,
	enum cam_ife_csid_halt_cmd       stop_cmd)
{
	int rc = 0;
	struct cam_ife_csid_reg_offset       *csid_reg;
	struct cam_hw_soc_info               *soc_info;
	struct cam_ife_csid_path_cfg         *path_data;

	path_data = (struct cam_ife_csid_path_cfg   *) res->res_priv;
	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (res->res_id >= CAM_IFE_PIX_PATH_RES_MAX) {
		CAM_DBG(CAM_ISP, "CSID:%d Invalid res id%d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	if (res->res_state == CAM_ISP_RESOURCE_STATE_INIT_HW ||
		res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_DBG(CAM_ISP, "CSID:%d Res:%d already in stopped state:%d",
			 csid_hw->hw_intf->hw_idx,
			res->res_id, res->res_state);
		return rc;
	}

	if (res->res_state != CAM_ISP_RESOURCE_STATE_STREAMING) {
		CAM_DBG(CAM_ISP, "CSID:%d Res:%d Invalid state%d",
			csid_hw->hw_intf->hw_idx, res->res_id,
			res->res_state);
		return -EINVAL;
	}

	if (!csid_reg->ipp_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d IPP%d is not supported on HW",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	if (stop_cmd != CAM_CSID_HALT_AT_FRAME_BOUNDARY &&
		stop_cmd != CAM_CSID_HALT_IMMEDIATELY) {
		CAM_ERR(CAM_ISP, "CSID:%d un supported stop command:%d",
			csid_hw->hw_intf->hw_idx, stop_cmd);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "CSID:%d res_id:%d",
		csid_hw->hw_intf->hw_idx, res->res_id);

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->ipp_reg->csid_ipp_irq_mask_addr);

	return rc;
}


static int cam_ife_csid_init_config_rdi_path(
	struct cam_ife_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	struct cam_ife_csid_path_cfg           *path_data;
	struct cam_ife_csid_reg_offset         *csid_reg;
	struct cam_hw_soc_info                 *soc_info;
	uint32_t path_format = 0, plain_fmt = 0, val = 0, id;
	uint32_t format_measure_addr;

	path_data = (struct cam_ife_csid_path_cfg   *) res->res_priv;
	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	id = res->res_id;
	if (!csid_reg->rdi_reg[id]) {
		CAM_ERR(CAM_ISP, "CSID:%d RDI:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, id);
		return -EINVAL;
	}

	rc = cam_ife_csid_get_format_rdi(path_data->in_format,
		path_data->out_format, &path_format, &plain_fmt);
	if (rc)
		return rc;

	/* if path decode format is payload only then RDI crop is not applied */
	if (path_format == 0xF)
		path_data->crop_enable = 0;

	/*
	 * RDI path config and enable the time stamp capture
	 * Enable the measurement blocks
	 */
	val = (path_data->vc << csid_reg->cmn_reg->vc_shift_val) |
		(path_data->dt << csid_reg->cmn_reg->dt_shift_val) |
		(path_data->cid << csid_reg->cmn_reg->dt_id_shift_val) |
		(path_format << csid_reg->cmn_reg->fmt_shift_val) |
		(plain_fmt << csid_reg->cmn_reg->plain_fmt_shit_val) |
		(path_data->crop_enable  <<
			csid_reg->cmn_reg->crop_h_en_shift_val) |
		(path_data->crop_enable  <<
		csid_reg->cmn_reg->crop_v_en_shift_val) |
		(1 << 2) | 3;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_cfg0_addr);

	/* select the post irq sub sample strobe for time stamp capture */
	cam_io_w_mb(CSID_TIMESTAMP_STB_POST_IRQ, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_cfg1_addr);

	if (path_data->crop_enable) {
		val = (((path_data->end_pixel & 0xFFFF) <<
			csid_reg->cmn_reg->crop_shift) |
			(path_data->start_pixel & 0xFFFF));

		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_rpp_hcrop_addr);
		CAM_DBG(CAM_ISP, "CSID:%d Horizontal crop config val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);

		val = (((path_data->end_line & 0xFFFF) <<
			csid_reg->cmn_reg->crop_shift) |
			(path_data->start_line & 0xFFFF));

		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_rpp_vcrop_addr);
		CAM_DBG(CAM_ISP, "CSID:%d Vertical Crop config val: 0x%x",
			csid_hw->hw_intf->hw_idx, val);
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

	/* Enable the RPP path */
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_cfg0_addr);
	val |= (1 << csid_reg->cmn_reg->path_en_shift_val);

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_HBI_VBI_INFO)
		val |= csid_reg->cmn_reg->format_measure_en_val;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
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
			(path_data->vc <<
			csid_reg->csi2_reg->csi2_capture_short_pkt_vc_shift));

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE)
		val |= ((1 <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_en_shift) |
			(path_data->dt <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_dt_shift) |
			(path_data->vc <<
			csid_reg->csi2_reg->csi2_capture_long_pkt_vc_shift));

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE)
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

static int cam_ife_csid_deinit_rdi_path(
	struct cam_ife_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	uint32_t id, val, format_measure_addr;
	struct cam_ife_csid_reg_offset      *csid_reg;
	struct cam_hw_soc_info              *soc_info;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	id = res->res_id;

	if (res->res_id > CAM_IFE_PIX_PATH_RES_RDI_3 ||
		res->res_state != CAM_ISP_RESOURCE_STATE_INIT_HW ||
		!csid_reg->rdi_reg[id]) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res id%d state:%d",
			csid_hw->hw_intf->hw_idx, res->res_id,
			res->res_state);
		return -EINVAL;
	}

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

	res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	return rc;
}

static int cam_ife_csid_enable_rdi_path(
	struct cam_ife_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res)
{
	struct cam_ife_csid_reg_offset      *csid_reg;
	struct cam_hw_soc_info              *soc_info;
	uint32_t id, val;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	id = res->res_id;

	if (res->res_state != CAM_ISP_RESOURCE_STATE_INIT_HW ||
		res->res_id > CAM_IFE_PIX_PATH_RES_RDI_3 ||
		!csid_reg->rdi_reg[id]) {
		CAM_ERR(CAM_ISP,
			"CSID:%d invalid res type:%d res_id:%d state%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	/*resume at frame boundary */
	cam_io_w_mb(CAM_CSID_RESUME_AT_FRAME_BOUNDARY,
			soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[id]->csid_rdi_ctrl_addr);

	/* Enable the required RDI interrupts */
	val = CSID_PATH_INFO_RST_DONE | CSID_PATH_ERROR_FIFO_OVERFLOW;

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_SOF_IRQ)
		val |= CSID_PATH_INFO_INPUT_SOF;
	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_EOF_IRQ)
		val |= CSID_PATH_INFO_INPUT_EOF;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_irq_mask_addr);

	res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	return 0;
}


static int cam_ife_csid_disable_rdi_path(
	struct cam_ife_csid_hw          *csid_hw,
	struct cam_isp_resource_node    *res,
	enum cam_ife_csid_halt_cmd                stop_cmd)
{
	int rc = 0;
	uint32_t id;
	struct cam_ife_csid_reg_offset       *csid_reg;
	struct cam_hw_soc_info               *soc_info;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	id = res->res_id;

	if ((res->res_id > CAM_IFE_PIX_PATH_RES_RDI_3) ||
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

	if (stop_cmd != CAM_CSID_HALT_AT_FRAME_BOUNDARY &&
		stop_cmd != CAM_CSID_HALT_IMMEDIATELY) {
		CAM_ERR(CAM_ISP, "CSID:%d un supported stop command:%d",
			csid_hw->hw_intf->hw_idx, stop_cmd);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "CSID:%d res_id:%d",
		csid_hw->hw_intf->hw_idx, res->res_id);

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[id]->csid_rdi_irq_mask_addr);

	return rc;
}

static int cam_ife_csid_get_hbi_vbi(
	struct cam_ife_csid_hw   *csid_hw,
	struct cam_isp_resource_node *res)
{
	uint32_t  hbi, vbi;
	const struct cam_ife_csid_reg_offset     *csid_reg;
	const struct cam_ife_csid_rdi_reg_offset *rdi_reg;
	struct cam_hw_soc_info                   *soc_info;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (res->res_type != CAM_ISP_RESOURCE_PIX_PATH ||
		res->res_id >= CAM_IFE_PIX_PATH_RES_MAX) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res_type:%d res id%d",
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

	if (res->res_id == CAM_IFE_PIX_PATH_RES_IPP) {
		hbi = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_format_measure1_addr);
		vbi = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_format_measure2_addr);
	} else {
		rdi_reg = csid_reg->rdi_reg[res->res_id];
		hbi = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			rdi_reg->csid_rdi_format_measure1_addr);
		vbi = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			rdi_reg->csid_rdi_format_measure2_addr);
	}

	CAM_INFO_RATE_LIMIT(CAM_ISP, "Resource %u HBI: 0x%x", res->res_id,
		hbi);
	CAM_INFO_RATE_LIMIT(CAM_ISP, "Resource %u VBI: 0x%x", res->res_id,
		vbi);

	return 0;
}


static int cam_ife_csid_get_time_stamp(
		struct cam_ife_csid_hw   *csid_hw, void *cmd_args)
{
	struct cam_csid_get_time_stamp_args  *time_stamp;
	struct cam_isp_resource_node         *res;
	struct cam_ife_csid_reg_offset       *csid_reg;
	struct cam_hw_soc_info               *soc_info;
	struct cam_ife_csid_rdi_reg_offset   *rdi_reg;
	struct timespec64 ts;
	uint32_t  time_32, id;

	time_stamp = (struct cam_csid_get_time_stamp_args  *)cmd_args;
	res = time_stamp->node_res;
	csid_reg = csid_hw->csid_info->csid_reg;
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

	if (res->res_id == CAM_IFE_PIX_PATH_RES_IPP) {
		time_32 = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_timestamp_curr1_sof_addr);
		time_stamp->time_stamp_val = time_32;
		time_stamp->time_stamp_val = time_stamp->time_stamp_val << 32;
		time_32 = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_timestamp_curr0_sof_addr);
		time_stamp->time_stamp_val |= time_32;
	} else {
		id = res->res_id;
		rdi_reg = csid_reg->rdi_reg[id];
		time_32 = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			rdi_reg->csid_rdi_timestamp_curr1_sof_addr);
		time_stamp->time_stamp_val = time_32;
		time_stamp->time_stamp_val = time_stamp->time_stamp_val << 32;

		time_32 = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			rdi_reg->csid_rdi_timestamp_curr0_sof_addr);
		time_stamp->time_stamp_val |= time_32;
	}

	time_stamp->time_stamp_val = mul_u64_u32_div(
		time_stamp->time_stamp_val,
		CAM_IFE_CSID_QTIMER_MUL_FACTOR,
		CAM_IFE_CSID_QTIMER_DIV_FACTOR);

	get_monotonic_boottime64(&ts);
	time_stamp->boot_timestamp = (uint64_t)((ts.tv_sec * 1000000000) +
		ts.tv_nsec);

	return 0;
}

static int cam_ife_csid_set_csid_debug(struct cam_ife_csid_hw   *csid_hw,
	void *cmd_args)
{
	uint32_t  *csid_debug;

	csid_debug = (uint32_t  *) cmd_args;
	csid_hw->csid_debug = *csid_debug;
	CAM_DBG(CAM_ISP, "CSID:%d set csid debug value:%d",
		csid_hw->hw_intf->hw_idx, csid_hw->csid_debug);

	return 0;
}

static int cam_ife_csid_get_hw_caps(void *hw_priv,
	void *get_hw_cap_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_ife_csid_hw_caps     *hw_caps;
	struct cam_ife_csid_hw          *csid_hw;
	struct cam_hw_info              *csid_hw_info;
	struct cam_ife_csid_reg_offset  *csid_reg;

	if (!hw_priv || !get_hw_cap_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_ife_csid_hw   *)csid_hw_info->core_info;
	csid_reg = csid_hw->csid_info->csid_reg;
	hw_caps = (struct cam_ife_csid_hw_caps *) get_hw_cap_args;

	hw_caps->no_rdis = csid_reg->cmn_reg->no_rdis;
	hw_caps->no_pix = csid_reg->cmn_reg->no_pix;
	hw_caps->major_version = csid_reg->cmn_reg->major_version;
	hw_caps->minor_version = csid_reg->cmn_reg->minor_version;
	hw_caps->version_incr = csid_reg->cmn_reg->version_incr;

	CAM_DBG(CAM_ISP,
		"CSID:%d No rdis:%d, no pix:%d, major:%d minor:%d ver :%d",
		csid_hw->hw_intf->hw_idx, hw_caps->no_rdis,
		hw_caps->no_pix, hw_caps->major_version, hw_caps->minor_version,
		hw_caps->version_incr);

	return rc;
}

static int cam_ife_csid_reset(void *hw_priv,
	void *reset_args, uint32_t arg_size)
{
	struct cam_ife_csid_hw          *csid_hw;
	struct cam_hw_info              *csid_hw_info;
	struct cam_csid_reset_cfg_args  *reset;
	int rc = 0;

	if (!hw_priv || !reset_args || (arg_size !=
		sizeof(struct cam_csid_reset_cfg_args))) {
		CAM_ERR(CAM_ISP, "CSID:Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_ife_csid_hw   *)csid_hw_info->core_info;
	reset   = (struct cam_csid_reset_cfg_args  *)reset_args;

	switch (reset->reset_type) {
	case CAM_IFE_CSID_RESET_GLOBAL:
		rc = cam_ife_csid_global_reset(csid_hw);
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

	return rc;
}

static int cam_ife_csid_reserve(void *hw_priv,
	void *reserve_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_ife_csid_hw                    *csid_hw;
	struct cam_hw_info                        *csid_hw_info;
	struct cam_csid_hw_reserve_resource_args  *reserv;

	if (!hw_priv || !reserve_args || (arg_size !=
		sizeof(struct cam_csid_hw_reserve_resource_args))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_ife_csid_hw   *)csid_hw_info->core_info;
	reserv = (struct cam_csid_hw_reserve_resource_args  *)reserve_args;

	mutex_lock(&csid_hw->hw_info->hw_mutex);
	switch (reserv->res_type) {
	case CAM_ISP_RESOURCE_CID:
		rc = cam_ife_csid_cid_reserve(csid_hw, reserv);
		break;
	case CAM_ISP_RESOURCE_PIX_PATH:
		rc = cam_ife_csid_path_reserve(csid_hw, reserv);
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type :%d",
			csid_hw->hw_intf->hw_idx, reserv->res_type);
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_ife_csid_release(void *hw_priv,
	void *release_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_ife_csid_hw          *csid_hw;
	struct cam_hw_info              *csid_hw_info;
	struct cam_isp_resource_node    *res;
	struct cam_ife_csid_cid_data    *cid_data;

	if (!hw_priv || !release_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_ife_csid_hw   *)csid_hw_info->core_info;
	res = (struct cam_isp_resource_node *)release_args;

	mutex_lock(&csid_hw->hw_info->hw_mutex);
	if ((res->res_type == CAM_ISP_RESOURCE_CID &&
		res->res_id >= CAM_IFE_CSID_CID_MAX) ||
		(res->res_type == CAM_ISP_RESOURCE_PIX_PATH &&
		res->res_id >= CAM_IFE_PIX_PATH_RES_MAX)) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type:%d res id%d",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id);
		rc = -EINVAL;
		goto end;
	}

	if (res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_DBG(CAM_ISP,
			"CSID:%d res type:%d Res %d  in released state",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id);
		goto end;
	}

	if (res->res_type == CAM_ISP_RESOURCE_PIX_PATH &&
		res->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_DBG(CAM_ISP,
			"CSID:%d res type:%d Res id:%d invalid state:%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ISP, "CSID:%d res type :%d Resource id:%d",
		csid_hw->hw_intf->hw_idx, res->res_type, res->res_id);

	switch (res->res_type) {
	case CAM_ISP_RESOURCE_CID:
		cid_data = (struct cam_ife_csid_cid_data    *) res->res_priv;
		if (cid_data->cnt)
			cid_data->cnt--;

		if (!cid_data->cnt)
			res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

		if (csid_hw->csi2_reserve_cnt)
			csid_hw->csi2_reserve_cnt--;

		if (!csid_hw->csi2_reserve_cnt)
			memset(&csid_hw->csi2_rx_cfg, 0,
				sizeof(struct cam_ife_csid_csi2_rx_cfg));

		CAM_DBG(CAM_ISP, "CSID:%d res id :%d cnt:%d reserv cnt:%d",
			 csid_hw->hw_intf->hw_idx,
			res->res_id, cid_data->cnt, csid_hw->csi2_reserve_cnt);

		break;
	case CAM_ISP_RESOURCE_PIX_PATH:
		res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type:%d res id%d",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id);
		rc = -EINVAL;
		break;
	}

end:
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_ife_csid_reset_retain_sw_reg(
	struct cam_ife_csid_hw *csid_hw)
{
	int rc = 0;
	uint32_t status;
	struct cam_ife_csid_reg_offset *csid_reg =
		csid_hw->csid_info->csid_reg;
	struct cam_hw_soc_info          *soc_info;

	soc_info = &csid_hw->hw_info->soc_info;
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
		CAM_IFE_CSID_TIMEOUT_SLEEP_US, CAM_IFE_CSID_TIMEOUT_ALL_US);
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

static int cam_ife_csid_init_hw(void *hw_priv,
	void *init_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_ife_csid_hw                 *csid_hw;
	struct cam_hw_info                     *csid_hw_info;
	struct cam_isp_resource_node           *res;
	struct cam_ife_csid_reg_offset         *csid_reg;

	if (!hw_priv || !init_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_ife_csid_hw   *)csid_hw_info->core_info;
	res      = (struct cam_isp_resource_node *)init_args;
	csid_reg = csid_hw->csid_info->csid_reg;

	mutex_lock(&csid_hw->hw_info->hw_mutex);
	if ((res->res_type == CAM_ISP_RESOURCE_CID &&
		res->res_id >= CAM_IFE_CSID_CID_MAX) ||
		(res->res_type == CAM_ISP_RESOURCE_PIX_PATH &&
		res->res_id >= CAM_IFE_PIX_PATH_RES_MAX)) {
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
	rc = cam_ife_csid_enable_hw(csid_hw);
	if (rc)
		goto end;

	switch (res->res_type) {
	case CAM_ISP_RESOURCE_CID:
		rc = cam_ife_csid_enable_csi2(csid_hw, res);
		break;
	case CAM_ISP_RESOURCE_PIX_PATH:
		if (res->res_id == CAM_IFE_PIX_PATH_RES_IPP)
			rc = cam_ife_csid_init_config_ipp_path(csid_hw, res);
		else
			rc = cam_ife_csid_init_config_rdi_path(csid_hw, res);

		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type state %d",
			 csid_hw->hw_intf->hw_idx,
			res->res_type);
		break;
	}

	rc = cam_ife_csid_reset_retain_sw_reg(csid_hw);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "CSID: Failed in SW reset");
	}

	if (rc)
		cam_ife_csid_disable_hw(csid_hw);
end:
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_ife_csid_deinit_hw(void *hw_priv,
	void *deinit_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_ife_csid_hw                 *csid_hw;
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
	csid_hw = (struct cam_ife_csid_hw   *)csid_hw_info->core_info;

	mutex_lock(&csid_hw->hw_info->hw_mutex);
	if (res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_DBG(CAM_ISP, "CSID:%d Res:%d already in De-init state",
			 csid_hw->hw_intf->hw_idx,
			res->res_id);
		goto end;
	}

	switch (res->res_type) {
	case CAM_ISP_RESOURCE_CID:
		CAM_DBG(CAM_ISP, "De-Init ife_csid");
		rc = cam_ife_csid_disable_csi2(csid_hw, res);
		break;
	case CAM_ISP_RESOURCE_PIX_PATH:
		CAM_DBG(CAM_ISP, "De-Init Pix Path: %d\n", res->res_id);
		if (res->res_id == CAM_IFE_PIX_PATH_RES_IPP)
			rc = cam_ife_csid_deinit_ipp_path(csid_hw, res);
		else
			rc = cam_ife_csid_deinit_rdi_path(csid_hw, res);

		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d Invalid Res type %d",
			 csid_hw->hw_intf->hw_idx,
			res->res_type);
		goto end;
	}

	/* Disable CSID HW */
	CAM_DBG(CAM_ISP, "Disabling CSID Hw\n");
	cam_ife_csid_disable_hw(csid_hw);
	CAM_DBG(CAM_ISP, "%s: Exit\n", __func__);

end:
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_ife_csid_start(void *hw_priv, void *start_args,
			uint32_t arg_size)
{
	int rc = 0;
	struct cam_ife_csid_hw                 *csid_hw;
	struct cam_hw_info                     *csid_hw_info;
	struct cam_isp_resource_node           *res;
	struct cam_ife_csid_reg_offset         *csid_reg;

	if (!hw_priv || !start_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_ife_csid_hw   *)csid_hw_info->core_info;
	res = (struct cam_isp_resource_node *)start_args;
	csid_reg = csid_hw->csid_info->csid_reg;

	if ((res->res_type == CAM_ISP_RESOURCE_CID &&
		res->res_id >= CAM_IFE_CSID_CID_MAX) ||
		(res->res_type == CAM_ISP_RESOURCE_PIX_PATH &&
		res->res_id >= CAM_IFE_PIX_PATH_RES_MAX)) {
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
	case CAM_ISP_RESOURCE_CID:
		if (csid_hw->res_type ==  CAM_ISP_IFE_IN_RES_TPG)
			rc = cam_ife_csid_tpg_start(csid_hw, res);
		break;
	case CAM_ISP_RESOURCE_PIX_PATH:
		if (res->res_id == CAM_IFE_PIX_PATH_RES_IPP)
			rc = cam_ife_csid_enable_ipp_path(csid_hw, res);
		else
			rc = cam_ife_csid_enable_rdi_path(csid_hw, res);
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type%d",
			 csid_hw->hw_intf->hw_idx,
			res->res_type);
		break;
	}
end:
	return rc;
}

static int cam_ife_csid_stop(void *hw_priv,
	void *stop_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_ife_csid_hw               *csid_hw;
	struct cam_hw_info                   *csid_hw_info;
	struct cam_isp_resource_node         *res;
	struct cam_csid_hw_stop_args         *csid_stop;
	uint32_t  i;

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

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_ife_csid_hw   *)csid_hw_info->core_info;
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
		case CAM_ISP_RESOURCE_CID:
			if (csid_hw->res_type == CAM_ISP_IFE_IN_RES_TPG)
				rc = cam_ife_csid_tpg_stop(csid_hw, res);
			break;
		case CAM_ISP_RESOURCE_PIX_PATH:
			if (res->res_id == CAM_IFE_PIX_PATH_RES_IPP)
				rc = cam_ife_csid_disable_ipp_path(csid_hw,
						res, csid_stop->stop_cmd);
			else
				rc = cam_ife_csid_disable_rdi_path(csid_hw,
						res, csid_stop->stop_cmd);

			break;
		default:
			CAM_ERR(CAM_ISP, "CSID:%d Invalid res type%d",
				csid_hw->hw_intf->hw_idx,
				res->res_type);
			break;
		}
	}

	for (i = 0; i < csid_stop->num_res; i++) {
		res = csid_stop->node_res[i];
		res->res_state = CAM_ISP_RESOURCE_STATE_INIT_HW;
	}

	CAM_DBG(CAM_ISP,  "%s: Exit\n", __func__);

	return rc;

}

static int cam_ife_csid_read(void *hw_priv,
	void *read_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "CSID: un supported");

	return -EINVAL;
}

static int cam_ife_csid_write(void *hw_priv,
	void *write_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "CSID: un supported");
	return -EINVAL;
}

static int cam_ife_csid_sof_irq_debug(
	struct cam_ife_csid_hw *csid_hw, void *cmd_args)
{
	int i = 0;
	uint32_t val = 0;
	bool sof_irq_enable = false;
	struct cam_ife_csid_reg_offset    *csid_reg;
	struct cam_hw_soc_info            *soc_info;

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (*((uint32_t *)cmd_args) == 1)
		sof_irq_enable = true;

	if (csid_reg->ipp_reg) {
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
				csid_reg->ipp_reg->csid_ipp_irq_mask_addr);

		if (val) {
			if (sof_irq_enable)
				val |= CSID_PATH_INFO_INPUT_SOF;
			else
				val &= ~CSID_PATH_INFO_INPUT_SOF;

			cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
				csid_reg->ipp_reg->csid_ipp_irq_mask_addr);
			val = 0;
		}
	}

	for (i = 0; i < csid_reg->cmn_reg->no_rdis; i++) {
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

static int cam_ife_csid_process_cmd(void *hw_priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_ife_csid_hw               *csid_hw;
	struct cam_hw_info                   *csid_hw_info;
	struct cam_isp_resource_node         *res = NULL;

	if (!hw_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid arguments");
		return -EINVAL;
	}

	csid_hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_ife_csid_hw   *)csid_hw_info->core_info;

	switch (cmd_type) {
	case CAM_IFE_CSID_CMD_GET_TIME_STAMP:
		rc = cam_ife_csid_get_time_stamp(csid_hw, cmd_args);
		if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_HBI_VBI_INFO) {
			res = ((struct cam_csid_get_time_stamp_args *)
				cmd_args)->node_res;
			cam_ife_csid_get_hbi_vbi(csid_hw, res);
		}
		break;
	case CAM_IFE_CSID_SET_CSID_DEBUG:
		rc = cam_ife_csid_set_csid_debug(csid_hw, cmd_args);
		break;
	case CAM_IFE_CSID_SOF_IRQ_DEBUG:
		rc = cam_ife_csid_sof_irq_debug(csid_hw, cmd_args);
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d unsupported cmd:%d",
			csid_hw->hw_intf->hw_idx, cmd_type);
		rc = -EINVAL;
		break;
	}

	return rc;

}

irqreturn_t cam_ife_csid_irq(int irq_num, void *data)
{
	struct cam_ife_csid_hw          *csid_hw;
	struct cam_hw_soc_info          *soc_info;
	struct cam_ife_csid_reg_offset  *csid_reg;
	struct cam_ife_csid_csi2_rx_reg_offset *csi2_reg;
	uint32_t i, irq_status_top, irq_status_rx, irq_status_ipp = 0;
	uint32_t irq_status_rdi[4] = {0, 0, 0, 0};
	uint32_t val, sof_irq_disable = 0;

	csid_hw = (struct cam_ife_csid_hw *)data;

	CAM_DBG(CAM_ISP, "CSID %d IRQ Handling", csid_hw->hw_intf->hw_idx);

	if (!data) {
		CAM_ERR(CAM_ISP, "CSID: Invalid arguments");
		return IRQ_HANDLED;
	}

	csid_reg = csid_hw->csid_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	csi2_reg = csid_reg->csi2_reg;

	/* read */
	irq_status_top = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_top_irq_status_addr);

	irq_status_rx = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_status_addr);

	if (csid_reg->cmn_reg->no_pix)
		irq_status_ipp = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_irq_status_addr);


	for (i = 0; i < csid_reg->cmn_reg->no_rdis; i++)
		irq_status_rdi[i] = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		csid_reg->rdi_reg[i]->csid_rdi_irq_status_addr);

	/* clear */
	cam_io_w_mb(irq_status_rx, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->csid_csi2_rx_irq_clear_addr);
	if (csid_reg->cmn_reg->no_pix)
		cam_io_w_mb(irq_status_ipp, soc_info->reg_map[0].mem_base +
			csid_reg->ipp_reg->csid_ipp_irq_clear_addr);

	for (i = 0; i < csid_reg->cmn_reg->no_rdis; i++) {
		cam_io_w_mb(irq_status_rdi[i], soc_info->reg_map[0].mem_base +
			csid_reg->rdi_reg[i]->csid_rdi_irq_clear_addr);
	}
	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->csid_irq_cmd_addr);

	CAM_DBG(CAM_ISP, "irq_status_top = 0x%x", irq_status_top);
	CAM_DBG(CAM_ISP, "irq_status_rx = 0x%x", irq_status_rx);
	CAM_DBG(CAM_ISP, "irq_status_ipp = 0x%x", irq_status_ipp);
	CAM_DBG(CAM_ISP, "irq_status_rdi0= 0x%x", irq_status_rdi[0]);
	CAM_DBG(CAM_ISP, "irq_status_rdi1= 0x%x", irq_status_rdi[1]);
	CAM_DBG(CAM_ISP, "irq_status_rdi2= 0x%x", irq_status_rdi[2]);

	if (irq_status_rx & BIT(csid_reg->csi2_reg->csi2_rst_done_shift_val)) {
		CAM_DBG(CAM_ISP, "csi rx reset complete");
		complete(&csid_hw->csid_csi2_complete);
	}

	if (irq_status_rx & CSID_CSI2_RX_ERROR_LANE0_FIFO_OVERFLOW) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d lane 0 over flow",
			 csid_hw->hw_intf->hw_idx);
		csid_hw->error_irq_count++;
	}
	if (irq_status_rx & CSID_CSI2_RX_ERROR_LANE1_FIFO_OVERFLOW) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d lane 1 over flow",
			 csid_hw->hw_intf->hw_idx);
		csid_hw->error_irq_count++;
	}
	if (irq_status_rx & CSID_CSI2_RX_ERROR_LANE2_FIFO_OVERFLOW) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d lane 2 over flow",
			 csid_hw->hw_intf->hw_idx);
		csid_hw->error_irq_count++;
	}
	if (irq_status_rx & CSID_CSI2_RX_ERROR_LANE3_FIFO_OVERFLOW) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d lane 3 over flow",
			 csid_hw->hw_intf->hw_idx);
		csid_hw->error_irq_count++;
	}
	if (irq_status_rx & CSID_CSI2_RX_ERROR_TG_FIFO_OVERFLOW) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d TG OVER  FLOW",
			 csid_hw->hw_intf->hw_idx);
	}
	if (irq_status_rx & CSID_CSI2_RX_ERROR_CPHY_EOT_RECEPTION) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d CPHY_EOT_RECEPTION",
			 csid_hw->hw_intf->hw_idx);
	}
	if (irq_status_rx & CSID_CSI2_RX_ERROR_CPHY_SOT_RECEPTION) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d CPHY_SOT_RECEPTION",
			 csid_hw->hw_intf->hw_idx);
	}
	if (irq_status_rx & CSID_CSI2_RX_ERROR_CPHY_PH_CRC) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d CPHY_PH_CRC",
			 csid_hw->hw_intf->hw_idx);
	}
	if (irq_status_rx & CSID_CSI2_RX_ERROR_CRC) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d ERROR_CRC",
			 csid_hw->hw_intf->hw_idx);
		csid_hw->error_irq_count++;
	}
	if (irq_status_rx & CSID_CSI2_RX_ERROR_ECC) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d ERROR_ECC",
			 csid_hw->hw_intf->hw_idx);
	}
	if (irq_status_rx & CSID_CSI2_RX_ERROR_MMAPPED_VC_DT) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d MMAPPED_VC_DT",
			 csid_hw->hw_intf->hw_idx);
	}
	if (irq_status_rx & CSID_CSI2_RX_ERROR_STREAM_UNDERFLOW) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d ERROR_STREAM_UNDERFLOW",
			 csid_hw->hw_intf->hw_idx);
		csid_hw->error_irq_count++;
	}
	if (irq_status_rx & CSID_CSI2_RX_ERROR_UNBOUNDED_FRAME) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID:%d UNBOUNDED_FRAME",
			 csid_hw->hw_intf->hw_idx);
		csid_hw->error_irq_count++;
	}

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_EOT_IRQ) {
		if (irq_status_rx & CSID_CSI2_RX_INFO_PHY_DL0_EOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL0_EOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status_rx & CSID_CSI2_RX_INFO_PHY_DL1_EOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL1_EOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status_rx & CSID_CSI2_RX_INFO_PHY_DL2_EOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL2_EOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status_rx & CSID_CSI2_RX_INFO_PHY_DL3_EOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL3_EOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
	}

	if (csid_hw->csid_debug & CSID_DEBUG_ENABLE_SOT_IRQ) {
		if (irq_status_rx & CSID_CSI2_RX_INFO_PHY_DL0_SOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL0_SOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status_rx & CSID_CSI2_RX_INFO_PHY_DL1_SOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL1_SOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status_rx & CSID_CSI2_RX_INFO_PHY_DL2_SOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL2_SOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
		if (irq_status_rx & CSID_CSI2_RX_INFO_PHY_DL3_SOT_CAPTURED) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID:%d PHY_DL3_SOT_CAPTURED",
				csid_hw->hw_intf->hw_idx);
		}
	}

	if ((csid_hw->csid_debug & CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE) &&
		(irq_status_rx & CSID_CSI2_RX_INFO_LONG_PKT_CAPTURED)) {
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
		(irq_status_rx & CSID_CSI2_RX_INFO_SHORT_PKT_CAPTURED)) {
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
		(irq_status_rx & CSID_CSI2_RX_INFO_CPHY_PKT_HDR_CAPTURED)) {
		CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID:%d CPHY_PKT_HDR_CAPTURED",
			csid_hw->hw_intf->hw_idx);
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->csid_csi2_rx_captured_cphy_pkt_hdr_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"CSID:%d cphy packet VC :%d DT:%d WC:%d",
			csid_hw->hw_intf->hw_idx,
			(val >> 22), ((val >> 16) & 0x1F), (val & 0xFFFF));
	}

	/*read the IPP errors */
	if (csid_reg->cmn_reg->no_pix) {
		/* IPP reset done bit */
		if (irq_status_ipp &
			BIT(csid_reg->cmn_reg->path_rst_done_shift_val)) {
			CAM_DBG(CAM_ISP, "CSID IPP reset complete");
			complete(&csid_hw->csid_ipp_complete);
		}

		if ((irq_status_ipp & CSID_PATH_INFO_INPUT_SOF) &&
			(csid_hw->csid_debug & CSID_DEBUG_ENABLE_SOF_IRQ)) {
			CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID:%d IPP SOF received",
				csid_hw->hw_intf->hw_idx);
			if (csid_hw->sof_irq_triggered)
				csid_hw->irq_debug_cnt++;
		}

		if ((irq_status_ipp & CSID_PATH_INFO_INPUT_EOF) &&
			(csid_hw->csid_debug & CSID_DEBUG_ENABLE_EOF_IRQ))
			CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID:%d IPP EOF received",
				csid_hw->hw_intf->hw_idx);

		if (irq_status_ipp & CSID_PATH_ERROR_FIFO_OVERFLOW) {
			CAM_ERR_RATE_LIMIT(CAM_ISP,
				"CSID:%d IPP fifo over flow",
				csid_hw->hw_intf->hw_idx);
			/*Stop IPP path immediately */
			cam_io_w_mb(CAM_CSID_HALT_IMMEDIATELY,
				soc_info->reg_map[0].mem_base +
				csid_reg->ipp_reg->csid_ipp_ctrl_addr);
		}
	}

	for (i = 0; i < csid_reg->cmn_reg->no_rdis; i++) {
		if (irq_status_rdi[i] &
			BIT(csid_reg->cmn_reg->path_rst_done_shift_val)) {
			CAM_DBG(CAM_ISP, "CSID RDI%d reset complete", i);
			complete(&csid_hw->csid_rdin_complete[i]);
		}

		if ((irq_status_rdi[i] & CSID_PATH_INFO_INPUT_SOF) &&
			(csid_hw->csid_debug & CSID_DEBUG_ENABLE_SOF_IRQ)) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID RDI:%d SOF received", i);
			if (csid_hw->sof_irq_triggered)
				csid_hw->irq_debug_cnt++;
		}

		if ((irq_status_rdi[i]  & CSID_PATH_INFO_INPUT_EOF) &&
			(csid_hw->csid_debug & CSID_DEBUG_ENABLE_EOF_IRQ))
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"CSID RDI:%d EOF received", i);

		if (irq_status_rdi[i] & CSID_PATH_ERROR_FIFO_OVERFLOW) {
			CAM_ERR_RATE_LIMIT(CAM_ISP,
				"CSID:%d RDI fifo over flow",
				csid_hw->hw_intf->hw_idx);
			/*Stop RDI path immediately */
			cam_io_w_mb(CAM_CSID_HALT_IMMEDIATELY,
				soc_info->reg_map[0].mem_base +
				csid_reg->rdi_reg[i]->csid_rdi_ctrl_addr);
		}
	}

	if (csid_hw->irq_debug_cnt >= CAM_CSID_IRQ_SOF_DEBUG_CNT_MAX) {
		cam_ife_csid_sof_irq_debug(csid_hw, &sof_irq_disable);
		csid_hw->irq_debug_cnt = 0;
	}

	if (csid_hw->error_irq_count >
		CAM_IFE_CSID_MAX_IRQ_ERROR_COUNT) {
		/* Mask line overflow, underflow, unbound interrupts */
		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csid_reg->csi2_reg->csid_csi2_rx_irq_mask_addr);

		val &=  ~(CSID_CSI2_RX_ERROR_LANE0_FIFO_OVERFLOW |
			CSID_CSI2_RX_ERROR_LANE1_FIFO_OVERFLOW |
			CSID_CSI2_RX_ERROR_LANE2_FIFO_OVERFLOW |
			CSID_CSI2_RX_ERROR_LANE3_FIFO_OVERFLOW |
			CSID_CSI2_RX_ERROR_CRC                 |
			CSID_CSI2_RX_ERROR_STREAM_UNDERFLOW    |
			CSID_CSI2_RX_ERROR_UNBOUNDED_FRAME);

		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			csid_reg->csi2_reg->csid_csi2_rx_irq_mask_addr);
		CAM_WARN(CAM_ISP, "Masked csi rx error interrupts");
		csid_hw->error_irq_count = 0;
	}

	CAM_DBG(CAM_ISP, "IRQ Handling exit");
	return IRQ_HANDLED;
}

int cam_ife_csid_hw_probe_init(struct cam_hw_intf  *csid_hw_intf,
	uint32_t csid_idx)
{
	int rc = -EINVAL;
	uint32_t i;
	uint32_t num_paths;
	struct cam_ife_csid_path_cfg         *path_data;
	struct cam_ife_csid_cid_data         *cid_data;
	struct cam_hw_info                   *csid_hw_info;
	struct cam_ife_csid_hw               *ife_csid_hw = NULL;

	if (csid_idx >= CAM_IFE_CSID_HW_RES_MAX) {
		CAM_ERR(CAM_ISP, "Invalid csid index:%d", csid_idx);
		return rc;
	}

	csid_hw_info = (struct cam_hw_info  *) csid_hw_intf->hw_priv;
	ife_csid_hw  = (struct cam_ife_csid_hw  *) csid_hw_info->core_info;

	ife_csid_hw->hw_intf = csid_hw_intf;
	ife_csid_hw->hw_info = csid_hw_info;

	CAM_DBG(CAM_ISP, "type %d index %d",
		ife_csid_hw->hw_intf->hw_type, csid_idx);


	ife_csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&ife_csid_hw->hw_info->hw_mutex);
	spin_lock_init(&ife_csid_hw->hw_info->hw_lock);
	init_completion(&ife_csid_hw->hw_info->hw_complete);

	init_completion(&ife_csid_hw->csid_top_complete);
	init_completion(&ife_csid_hw->csid_csi2_complete);
	init_completion(&ife_csid_hw->csid_ipp_complete);
	for (i = 0; i < CAM_IFE_CSID_RDI_MAX; i++)
		init_completion(&ife_csid_hw->csid_rdin_complete[i]);


	rc = cam_ife_csid_init_soc_resources(&ife_csid_hw->hw_info->soc_info,
			cam_ife_csid_irq, ife_csid_hw);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "CSID:%d Failed to init_soc", csid_idx);
		goto err;
	}

	ife_csid_hw->hw_intf->hw_ops.get_hw_caps = cam_ife_csid_get_hw_caps;
	ife_csid_hw->hw_intf->hw_ops.init        = cam_ife_csid_init_hw;
	ife_csid_hw->hw_intf->hw_ops.deinit      = cam_ife_csid_deinit_hw;
	ife_csid_hw->hw_intf->hw_ops.reset       = cam_ife_csid_reset;
	ife_csid_hw->hw_intf->hw_ops.reserve     = cam_ife_csid_reserve;
	ife_csid_hw->hw_intf->hw_ops.release     = cam_ife_csid_release;
	ife_csid_hw->hw_intf->hw_ops.start       = cam_ife_csid_start;
	ife_csid_hw->hw_intf->hw_ops.stop        = cam_ife_csid_stop;
	ife_csid_hw->hw_intf->hw_ops.read        = cam_ife_csid_read;
	ife_csid_hw->hw_intf->hw_ops.write       = cam_ife_csid_write;
	ife_csid_hw->hw_intf->hw_ops.process_cmd = cam_ife_csid_process_cmd;

	num_paths = ife_csid_hw->csid_info->csid_reg->cmn_reg->no_pix +
		ife_csid_hw->csid_info->csid_reg->cmn_reg->no_rdis;
	/* Initialize the CID resource */
	for (i = 0; i < num_paths; i++) {
		ife_csid_hw->cid_res[i].res_type = CAM_ISP_RESOURCE_CID;
		ife_csid_hw->cid_res[i].res_id = i;
		ife_csid_hw->cid_res[i].res_state  =
					CAM_ISP_RESOURCE_STATE_AVAILABLE;
		ife_csid_hw->cid_res[i].hw_intf = ife_csid_hw->hw_intf;

		cid_data = kzalloc(sizeof(struct cam_ife_csid_cid_data),
					GFP_KERNEL);
		if (!cid_data) {
			rc = -ENOMEM;
			goto err;
		}
		ife_csid_hw->cid_res[i].res_priv = cid_data;
	}

	/* Initialize the IPP resources */
	if (ife_csid_hw->csid_info->csid_reg->cmn_reg->no_pix) {
		ife_csid_hw->ipp_res.res_type = CAM_ISP_RESOURCE_PIX_PATH;
		ife_csid_hw->ipp_res.res_id = CAM_IFE_PIX_PATH_RES_IPP;
		ife_csid_hw->ipp_res.res_state =
			CAM_ISP_RESOURCE_STATE_AVAILABLE;
		ife_csid_hw->ipp_res.hw_intf = ife_csid_hw->hw_intf;
		path_data = kzalloc(sizeof(struct cam_ife_csid_path_cfg),
					GFP_KERNEL);
		if (!path_data) {
			rc = -ENOMEM;
			goto err;
		}
		ife_csid_hw->ipp_res.res_priv = path_data;
	}

	/* Initialize the RDI resource */
	for (i = 0; i < ife_csid_hw->csid_info->csid_reg->cmn_reg->no_rdis;
				i++) {
		/* res type is from RDI 0 to RDI3 */
		ife_csid_hw->rdi_res[i].res_type =
			CAM_ISP_RESOURCE_PIX_PATH;
		ife_csid_hw->rdi_res[i].res_id = i;
		ife_csid_hw->rdi_res[i].res_state =
			CAM_ISP_RESOURCE_STATE_AVAILABLE;
		ife_csid_hw->rdi_res[i].hw_intf = ife_csid_hw->hw_intf;

		path_data = kzalloc(sizeof(struct cam_ife_csid_path_cfg),
					GFP_KERNEL);
		if (!path_data) {
			rc = -ENOMEM;
			goto err;
		}
		ife_csid_hw->rdi_res[i].res_priv = path_data;
	}

	ife_csid_hw->csid_debug = 0;
	ife_csid_hw->error_irq_count = 0;
	return 0;
err:
	if (rc) {
		kfree(ife_csid_hw->ipp_res.res_priv);
		for (i = 0; i <
			ife_csid_hw->csid_info->csid_reg->cmn_reg->no_rdis; i++)
			kfree(ife_csid_hw->rdi_res[i].res_priv);

		for (i = 0; i < CAM_IFE_CSID_CID_RES_MAX; i++)
			kfree(ife_csid_hw->cid_res[i].res_priv);

	}

	return rc;
}


int cam_ife_csid_hw_deinit(struct cam_ife_csid_hw *ife_csid_hw)
{
	int rc = -EINVAL;
	uint32_t i;

	if (!ife_csid_hw) {
		CAM_ERR(CAM_ISP, "Invalid param");
		return rc;
	}

	/* release the privdate data memory from resources */
	kfree(ife_csid_hw->ipp_res.res_priv);
	for (i = 0; i <
		ife_csid_hw->csid_info->csid_reg->cmn_reg->no_rdis;
		i++) {
		kfree(ife_csid_hw->rdi_res[i].res_priv);
	}
	for (i = 0; i < CAM_IFE_CSID_CID_RES_MAX; i++)
		kfree(ife_csid_hw->cid_res[i].res_priv);

	cam_ife_csid_deinit_soc_resources(&ife_csid_hw->hw_info->soc_info);

	return 0;
}
