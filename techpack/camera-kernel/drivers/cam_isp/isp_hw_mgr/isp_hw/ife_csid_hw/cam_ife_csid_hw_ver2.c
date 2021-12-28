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
#include "cam_ife_csid_hw_ver2.h"
#include "cam_isp_hw.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"
#include "cam_irq_controller.h"
#include "cam_tasklet_util.h"
#include "cam_cdm_util.h"
#include "cam_common_util.h"
#include "cam_subdev.h"

/* CSIPHY TPG VC/DT values */
#define CAM_IFE_CPHY_TPG_VC_VAL                         0x0
#define CAM_IFE_CPHY_TPG_DT_VAL                         0x2B

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

static void cam_ife_csid_ver2_print_debug_reg_status(
	struct cam_ife_csid_ver2_hw *csid_hw,
	struct cam_isp_resource_node    *res);

static int cam_ife_csid_ver2_set_debug(
	struct cam_ife_csid_ver2_hw *csid_hw,
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
				IFE_CSID_VER2_PATH_INFO_INPUT_SOF;
			break;
		case CAM_IFE_CSID_DEBUG_ENABLE_EOF_IRQ:
			csid_hw->debug_info.path_mask |=
				IFE_CSID_VER2_PATH_INFO_INPUT_EOF;
			break;
		case CAM_IFE_CSID_DEBUG_ENABLE_SOT_IRQ:
			csid_hw->debug_info.rx_mask |=
				IFE_CSID_VER2_RX_DL0_SOT_CAPTURED |
				IFE_CSID_VER2_RX_DL1_SOT_CAPTURED |
				IFE_CSID_VER2_RX_DL2_SOT_CAPTURED;
			break;
		case CAM_IFE_CSID_DEBUG_ENABLE_EOT_IRQ:
			csid_hw->debug_info.rx_mask |=
				IFE_CSID_VER2_RX_DL0_EOT_CAPTURED |
				IFE_CSID_VER2_RX_DL1_EOT_CAPTURED |
				IFE_CSID_VER2_RX_DL2_EOT_CAPTURED;
			break;
		case CAM_IFE_CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE:
			csid_hw->debug_info.rx_mask |=
				IFE_CSID_VER2_RX_SHORT_PKT_CAPTURED;
			break;
		case CAM_IFE_CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE:
			csid_hw->debug_info.rx_mask |=
				IFE_CSID_VER2_RX_LONG_PKT_CAPTURED;
			break;
		case CAM_IFE_CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE:
			csid_hw->debug_info.rx_mask |=
				IFE_CSID_VER2_RX_CPHY_PKT_HDR_CAPTURED;
			break;
		case CAM_IFE_DEBUG_ENABLE_UNMAPPED_VC_DT_IRQ:
			csid_hw->debug_info.rx_mask |=
				IFE_CSID_VER2_RX_UNMAPPED_VC_DT;
			break;
		default:
			break;
		}

		debug_val >>= 1;
		bit_pos++;
	}

	return 0;
}

static int cam_ife_csid_ver2_sof_irq_debug(
	struct cam_ife_csid_ver2_hw *csid_hw,
	void *cmd_args)
{
	int      i = 0;
	bool     sof_irq_enable = false;
	struct   cam_ife_csid_ver2_reg_info    *csid_reg;
	struct   cam_ife_csid_ver2_path_cfg    *path_cfg;
	struct   cam_isp_resource_node         *res;
	uint32_t irq_mask[CAM_IFE_CSID_IRQ_REG_MAX] = {0};
	uint32_t data_idx;

	if (*((uint32_t *)cmd_args) == 1)
		sof_irq_enable = true;

	if (csid_hw->hw_info->hw_state ==
		CAM_HW_STATE_POWER_DOWN) {
		CAM_WARN(CAM_ISP,
			"CSID:%u powered down unable to %s sof irq",
			csid_hw->hw_intf->hw_idx,
			(sof_irq_enable) ? "enable" : "disable");
		return 0;
	}

	data_idx = csid_hw->rx_cfg.phy_sel - 1;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;

	for (i = CAM_IFE_PIX_PATH_RES_RDI_0; i < CAM_IFE_PIX_PATH_RES_MAX;
		i++) {

		res = &csid_hw->path_res[i];
		path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;

		if (!path_cfg || !path_cfg->irq_handle)
			continue;

		irq_mask[path_cfg->irq_reg_idx] =
			IFE_CSID_VER2_PATH_INFO_INPUT_SOF;
		cam_irq_controller_update_irq(
			csid_hw->csid_irq_controller,
			path_cfg->irq_handle,
			sof_irq_enable, irq_mask);
	}

	if (sof_irq_enable) {
		csid_hw->debug_info.path_mask |=
			IFE_CSID_VER2_PATH_INFO_INPUT_SOF;
		csid_hw->flags.sof_irq_triggered = true;
	} else {
		csid_hw->debug_info.path_mask &=
			~IFE_CSID_VER2_PATH_INFO_INPUT_SOF;
		csid_hw->flags.sof_irq_triggered = false;
	}

	CAM_INFO(CAM_ISP, "SOF freeze: CSID:%u SOF irq %s",
		csid_hw->hw_intf->hw_idx,
		(sof_irq_enable) ? "enabled" : "disabled");

	CAM_INFO(CAM_ISP, "Notify CSIPHY: %d",
			csid_hw->rx_cfg.phy_sel - 1);

	cam_subdev_notify_message(CAM_CSIPHY_DEVICE_TYPE,
			CAM_SUBDEV_MESSAGE_IRQ_ERR, (void *)&data_idx);

	return 0;
}

static int cam_ife_csid_ver2_get_evt_payload(
	struct cam_ife_csid_ver2_hw *csid_hw,
	struct cam_ife_csid_ver2_evt_payload **evt_payload,
	struct list_head    *payload_list,
	spinlock_t          *lock)
{

	spin_lock(lock);

	if (list_empty(payload_list)) {
		*evt_payload = NULL;
		spin_unlock(lock);
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No free payload core %d",
			csid_hw->hw_intf->hw_idx);
		return -ENOMEM;
	}

	*evt_payload = list_first_entry(payload_list,
			struct cam_ife_csid_ver2_evt_payload, list);
	list_del_init(&(*evt_payload)->list);
	spin_unlock(lock);

	return 0;
}

static int cam_ife_csid_ver2_put_evt_payload(
	struct cam_ife_csid_ver2_hw *csid_hw,
	struct cam_ife_csid_ver2_evt_payload **evt_payload,
	struct list_head    *payload_list,
	spinlock_t          *lock)
{
	unsigned long flags;

	if (*evt_payload == NULL) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid payload core %d",
			csid_hw->hw_intf->hw_idx);
		return -EINVAL;
	}
	spin_lock_irqsave(lock, flags);
	list_add_tail(&(*evt_payload)->list,
		payload_list);
	*evt_payload = NULL;
	spin_unlock_irqrestore(lock, flags);

	return 0;
}

static int cam_ife_csid_ver2_top_err_irq_top_half(
	uint32_t                                   evt_id,
	struct cam_irq_th_payload                 *th_payload)
{
	int rc = 0;
	struct cam_ife_csid_ver2_hw               *csid_hw = NULL;
	struct cam_ife_csid_ver2_evt_payload      *evt_payload;
	struct cam_ife_csid_ver2_reg_info         *csid_reg = NULL;

	csid_hw = th_payload->handler_priv;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;

	rc  = cam_ife_csid_ver2_get_evt_payload(csid_hw, &evt_payload,
			&csid_hw->path_free_payload_list,
			&csid_hw->path_payload_lock);

	CAM_DBG(CAM_ISP, "CSID:%d TOP status: 0x%x",
		csid_hw->hw_intf->hw_idx,
		th_payload->evt_status_arr[CAM_IFE_CSID_IRQ_REG_TOP]);

	if (rc) {
		CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID:%d TOP status: 0x%x",
			csid_hw->hw_intf->hw_idx,
			th_payload->evt_status_arr[CAM_IFE_CSID_IRQ_REG_TOP]);
		return rc;
	}

	evt_payload->irq_reg_val[CAM_IFE_CSID_IRQ_REG_TOP] =
			th_payload->evt_status_arr[CAM_IFE_CSID_IRQ_REG_TOP];

	th_payload->evt_payload_priv = evt_payload;

	return 0;
}

static int cam_ife_csid_ver2_handle_buf_done_irq(
	uint32_t                                   evt_id,
	struct cam_irq_th_payload                 *th_payload)
{
	struct cam_ife_csid_ver2_hw               *csid_hw = NULL;
	int rc = 0;

	csid_hw = th_payload->handler_priv;
	CAM_DBG(CAM_ISP, "Enter");
	rc = cam_irq_controller_handle_irq(evt_id,
		csid_hw->buf_done_irq_controller, CAM_IRQ_EVT_GROUP_0);
	return (rc == IRQ_HANDLED) ? 0 : -EINVAL;
}

static int cam_ife_csid_ver2_path_err_top_half(
	uint32_t                                   evt_id,
	struct cam_irq_th_payload                 *th_payload)
{
	int32_t                                    rc;
	struct cam_hw_info                        *hw_info;
	struct cam_ife_csid_ver2_hw               *csid_hw = NULL;
	struct cam_ife_csid_ver2_evt_payload      *evt_payload;
	struct cam_isp_resource_node              *res;
	struct cam_ife_csid_ver2_reg_info         *csid_reg = NULL;
	const uint8_t                            **irq_reg_tag;
	struct cam_ife_csid_ver2_path_cfg         *path_cfg;

	res  = th_payload->handler_priv;

	if (!res) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No private returned");
		return -ENODEV;
	}

	hw_info = (struct cam_hw_info *)res->hw_intf->hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;

	rc  = cam_ife_csid_ver2_get_evt_payload(csid_hw, &evt_payload,
			&csid_hw->path_free_payload_list,
			&csid_hw->path_payload_lock);
	irq_reg_tag = cam_ife_csid_get_irq_reg_tag_ptr();

	CAM_DBG(CAM_ISP, "CSID:%d TOP status: 0x%x %s status: 0x%x",
		csid_hw->hw_intf->hw_idx,
		th_payload->evt_status_arr[CAM_IFE_CSID_IRQ_REG_TOP],
		irq_reg_tag[path_cfg->irq_reg_idx],
		th_payload->evt_status_arr[path_cfg->irq_reg_idx]);

	if (rc) {
		CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID:%d TOP status: 0x%x %s status: 0x%x",
			csid_hw->hw_intf->hw_idx,
			th_payload->evt_status_arr[CAM_IFE_CSID_IRQ_REG_TOP],
			irq_reg_tag[path_cfg->irq_reg_idx],
			th_payload->evt_status_arr[path_cfg->irq_reg_idx]);
		return rc;
	}

	evt_payload->irq_reg_val[path_cfg->irq_reg_idx] =
			th_payload->evt_status_arr[path_cfg->irq_reg_idx];
	evt_payload->irq_reg_val[CAM_IFE_CSID_IRQ_REG_TOP] =
			th_payload->evt_status_arr[CAM_IFE_CSID_IRQ_REG_TOP];

	ktime_get_boottime_ts64(&path_cfg->error_ts);
	th_payload->evt_payload_priv = evt_payload;

	return 0;
}

static int cam_ife_csid_ver2_path_top_half(
	uint32_t                                   evt_id,
	struct cam_irq_th_payload                 *th_payload)
{
	int32_t                                    rc;
	struct cam_hw_info                        *hw_info;
	struct cam_ife_csid_ver2_hw               *csid_hw = NULL;
	struct cam_ife_csid_ver2_evt_payload      *evt_payload;
	struct cam_isp_resource_node              *res;
	struct cam_ife_csid_ver2_reg_info         *csid_reg = NULL;
	const uint8_t                            **irq_reg_tag;
	struct cam_ife_csid_ver2_path_cfg         *path_cfg;

	res  = th_payload->handler_priv;

	if (!res) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No private returned");
		return -ENODEV;
	}

	hw_info = (struct cam_hw_info *)res->hw_intf->hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;

	rc  = cam_ife_csid_ver2_get_evt_payload(csid_hw, &evt_payload,
			&csid_hw->path_free_payload_list,
			&csid_hw->path_payload_lock);
	irq_reg_tag = cam_ife_csid_get_irq_reg_tag_ptr();

	CAM_DBG(CAM_ISP, "CSID:%d TOP status: 0x%x %s status: 0x%x",
		csid_hw->hw_intf->hw_idx,
		th_payload->evt_status_arr[CAM_IFE_CSID_IRQ_REG_TOP],
		irq_reg_tag[path_cfg->irq_reg_idx],
		th_payload->evt_status_arr[path_cfg->irq_reg_idx]);

	if (rc) {
		CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID:%d TOP status: 0x%x %s status: 0x%x",
			csid_hw->hw_intf->hw_idx,
			th_payload->evt_status_arr[CAM_IFE_CSID_IRQ_REG_TOP],
			irq_reg_tag[path_cfg->irq_reg_idx],
			th_payload->evt_status_arr[path_cfg->irq_reg_idx]);
		return rc;
	}

	evt_payload->irq_reg_val[path_cfg->irq_reg_idx] =
			th_payload->evt_status_arr[path_cfg->irq_reg_idx];
	evt_payload->irq_reg_val[CAM_IFE_CSID_IRQ_REG_TOP] =
			th_payload->evt_status_arr[CAM_IFE_CSID_IRQ_REG_TOP];

	th_payload->evt_payload_priv = evt_payload;

	return 0;
}

static inline void cam_ife_csid_ver2_reset_discard_frame_cfg(
	char                                        *res_name,
	struct cam_ife_csid_ver2_hw                 *csid_hw,
	struct cam_ife_csid_ver2_path_cfg           *path_cfg)
{
	int rc;

	/* Reset discard config params */
	path_cfg->discard_init_frames = false;
	path_cfg->skip_discard_frame_cfg = false;
	path_cfg->num_frames_discard = 0;
	path_cfg->sof_cnt = 0;

	/* Decrement discard frame ref cnt for this path */
	atomic_dec(&csid_hw->discard_frame_per_path);

	/* If input SOF irq is enabled explicitly - unsubscribe in th*/
	if (path_cfg->discard_irq_handle > 0) {
		rc = cam_irq_controller_unsubscribe_irq(
			csid_hw->csid_irq_controller,
			path_cfg->discard_irq_handle);
		if (rc)
			CAM_WARN(CAM_ISP,
				"Failed to unsubscribe input SOF for res: %s",
				res_name);

		path_cfg->discard_irq_handle = 0;
	}

	CAM_DBG(CAM_ISP, "CSID[%u] Reset discard frame config for res: %s discard_ref_cnt: %u",
		csid_hw->hw_intf->hw_idx, res_name,
		atomic_read(&csid_hw->discard_frame_per_path));
}

static int cam_ife_csid_ver2_discard_sof_top_half(
	uint32_t                                   evt_id,
	struct cam_irq_th_payload                 *th_payload)
{
	struct cam_hw_info                        *hw_info;
	struct cam_ife_csid_ver2_hw               *csid_hw = NULL;
	struct cam_isp_resource_node              *res;
	struct cam_ife_csid_ver2_path_cfg         *path_cfg;

	res  = th_payload->handler_priv;

	if (!res) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No private returned");
		return -ENODEV;
	}

	hw_info = (struct cam_hw_info *)res->hw_intf->hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;

	CAM_DBG(CAM_ISP, "CSID: %d status: 0x%x for res: %s",
		csid_hw->hw_intf->hw_idx,
		th_payload->evt_status_arr[path_cfg->irq_reg_idx],
		res->res_name);

	/* No need of payload since it's an exclusive th & bh */
	th_payload->evt_payload_priv = NULL;

	return 0;
}

static int cam_ife_csid_ver2_discard_sof_pix_bottom_half(
	void              *handler_priv,
	void              *evt_payload_priv)
{
	struct cam_hw_info                           *hw_info;
	struct cam_ife_csid_ver2_hw                  *csid_hw = NULL;
	struct cam_isp_resource_node                 *res;
	struct cam_ife_csid_ver2_reg_info            *csid_reg = NULL;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg = NULL;
	struct cam_ife_csid_ver2_path_cfg            *path_cfg;
	struct cam_hw_soc_info                       *soc_info;
	void    __iomem                              *base;
	uint32_t                                      val;

	if (!handler_priv) {
		CAM_ERR(CAM_ISP, "Invalid handler_priv");
		return -EINVAL;
	}

	res   =  handler_priv;
	hw_info = (struct cam_hw_info *)res->hw_intf->hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	soc_info = &csid_hw->hw_info->soc_info;
	base  = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;

	path_reg = csid_reg->path_reg[res->res_id];

	/* Count SOFs */
	path_cfg->sof_cnt++;

	CAM_DBG(CAM_ISP, "CSID[%u] Discard frame on %s path, num SOFs: %u",
		csid_hw->hw_intf->hw_idx, res->res_name, path_cfg->sof_cnt);

	spin_lock(&csid_hw->lock_state);
	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID[%d] powered down state",
			csid_hw->hw_intf->hw_idx);
		goto end;
	}

	/* Check with requested number of frames to be dropped */
	if (path_cfg->sof_cnt == path_cfg->num_frames_discard) {
		if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_MASTER ||
			path_cfg->sync_mode == CAM_ISP_HW_SYNC_NONE) {
			val = cam_io_r_mb(base + path_reg->ctrl_addr);
			val |= path_reg->resume_frame_boundary;
			cam_io_w_mb(val, base + path_reg->ctrl_addr);
			CAM_DBG(CAM_ISP,
				"CSID[%u] start cmd programmed for %s sof_cnt %u",
				csid_hw->hw_intf->hw_idx,
				res->res_name,
				path_cfg->sof_cnt);
		}
		cam_ife_csid_ver2_reset_discard_frame_cfg(res->res_name, csid_hw, path_cfg);
	}
end:
	spin_unlock(&csid_hw->lock_state);
	return 0;
}

static int cam_ife_csid_ver2_discard_sof_rdi_bottom_half(
	void              *handler_priv,
	void              *evt_payload_priv)
{
	struct cam_hw_info                           *hw_info;
	struct cam_ife_csid_ver2_hw                  *csid_hw = NULL;
	struct cam_isp_resource_node                 *res;
	struct cam_ife_csid_ver2_reg_info            *csid_reg = NULL;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg = NULL;
	struct cam_ife_csid_ver2_path_cfg            *path_cfg;
	struct cam_hw_soc_info                       *soc_info;
	void    __iomem                              *base;
	uint32_t                                      val;

	if (!handler_priv) {
		CAM_ERR(CAM_ISP, "Invalid handler_priv");
		return -EINVAL;
	}

	res   =  handler_priv;
	hw_info = (struct cam_hw_info *)res->hw_intf->hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	soc_info = &csid_hw->hw_info->soc_info;
	base  = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;
	path_reg = csid_reg->path_reg[res->res_id];

	/* Count SOFs */
	path_cfg->sof_cnt++;
	CAM_DBG(CAM_ISP, "CSID[%u] Discard frame on %s path, num SOFs: %u",
		csid_hw->hw_intf->hw_idx, res->res_name, path_cfg->sof_cnt);

	spin_lock(&csid_hw->lock_state);
	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID[%d] powered down state",
			csid_hw->hw_intf->hw_idx);
		goto end;
	}

	/* Check with requested number of frames to be dropped */
	if (path_cfg->sof_cnt == path_cfg->num_frames_discard) {
		val = cam_io_r_mb(base + path_reg->ctrl_addr);
		val |= path_reg->resume_frame_boundary;
		cam_io_w_mb(val, base + path_reg->ctrl_addr);
		CAM_DBG(CAM_ISP,
			"CSID[%u] start cmd programmed for %s sof_cnt %u",
			csid_hw->hw_intf->hw_idx,
			res->res_name, path_cfg->sof_cnt);

		cam_ife_csid_ver2_reset_discard_frame_cfg(res->res_name, csid_hw, path_cfg);
	}
end:
	spin_unlock(&csid_hw->lock_state);
	return 0;
}

static int cam_ife_csid_ver2_stop_csi2_in_err(
	struct cam_ife_csid_ver2_hw  *csid_hw)
{
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	struct cam_hw_soc_info                  *soc_info;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	CAM_DBG(CAM_ISP, "CSID:%d Stop csi2 rx",
		csid_hw->hw_intf->hw_idx);

	/* Reset the Rx CFG registers */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->cfg0_addr);
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->csi2_reg->cfg1_addr);

	if (csid_hw->rx_cfg.irq_handle)
		cam_irq_controller_disable_irq(
			csid_hw->csid_irq_controller,
			csid_hw->rx_cfg.irq_handle);

	if (csid_hw->rx_cfg.err_irq_handle)
		cam_irq_controller_disable_irq(
			csid_hw->csid_irq_controller,
			csid_hw->rx_cfg.err_irq_handle);

	return 0;
}

static int cam_ife_csid_ver2_disable_csi2(
	struct cam_ife_csid_ver2_hw  *csid_hw)
{
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	struct cam_hw_soc_info                  *soc_info;
	int rc = 0;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	CAM_DBG(CAM_ISP, "CSID:%d Disable csi2 rx",
		csid_hw->hw_intf->hw_idx);

	if (!csid_hw->flags.rx_enabled) {
		CAM_DBG(CAM_ISP, "CSID:%d Rx already disabled",
			csid_hw->hw_intf->hw_idx);
		return 0;
	}

	if (csid_hw->rx_cfg.irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			csid_hw->csid_irq_controller,
			csid_hw->rx_cfg.irq_handle);
		csid_hw->rx_cfg.irq_handle = 0;
	}

	if (csid_hw->rx_cfg.err_irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			csid_hw->csid_irq_controller,
			csid_hw->rx_cfg.err_irq_handle);
		csid_hw->rx_cfg.err_irq_handle = 0;
	}
	csid_hw->flags.rx_enabled = false;

	return 0;
}

static int cam_ife_csid_ver2_rx_err_top_half(
	uint32_t                                   evt_id,
	struct cam_irq_th_payload                 *th_payload)
{
	int32_t                                          rc = 0;
	int                                              i;
	uint32_t                                         status = 0;
	struct cam_ife_csid_ver2_hw                     *csid_hw = NULL;
	struct cam_ife_csid_ver2_reg_info               *csid_reg;
	struct cam_ife_csid_ver2_evt_payload            *evt_payload;
	const struct cam_ife_csid_csi2_rx_reg_info      *csi2_reg;
	int                                              discard_frame_count;

	csid_hw = th_payload->handler_priv;
	if (!csid_hw) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No private returned");
		return -ENODEV;
	}

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
				csid_hw->core_info->csid_reg;
	csi2_reg = csid_reg->csi2_reg;

	discard_frame_count = atomic_read(&csid_hw->discard_frame_per_path);
	if (discard_frame_count) {
		CAM_DBG(CAM_ISP, "ignoring rx error (error:0x%x, remaining frames:%d)",
			th_payload->evt_status_arr[CAM_IFE_CSID_IRQ_REG_RX], discard_frame_count);
		return -ENODEV;
	}

	if (csid_hw->flags.fatal_err_detected) {
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"CSID[%u] already handling fatal error",
			csid_hw->hw_intf->hw_idx);
		return -ENODEV;
	}

	if (csid_hw->rx_cfg.epd_supported &&
		(th_payload->evt_status_arr[CAM_IFE_CSID_IRQ_REG_RX] ==
			IFE_CSID_VER2_RX_CPHY_EOT_RECEPTION)) {
		CAM_DBG(CAM_ISP, "CSID[%u] Rcvd Only ERROR_EOT for EPD sensor",
			csid_hw->hw_intf->hw_idx);
		return -ENODEV;
	}

	status = th_payload->evt_status_arr[CAM_IFE_CSID_IRQ_REG_RX];

	if (status & csi2_reg->fatal_err_mask) {
		csid_hw->flags.fatal_err_detected = true;
		cam_ife_csid_ver2_stop_csi2_in_err(csid_hw);
		goto end;
	}

	if (status & csi2_reg->part_fatal_err_mask) {
		if (status & IFE_CSID_VER2_RX_CPHY_EOT_RECEPTION)
			csid_hw->counters.error_irq_count++;

		if (status & IFE_CSID_VER2_RX_CPHY_SOT_RECEPTION)
			csid_hw->counters.error_irq_count++;

		if (status & IFE_CSID_VER2_RX_ERROR_CRC)
			csid_hw->counters.error_irq_count++;

		if (status & IFE_CSID_VER2_RX_UNBOUNDED_FRAME)
			csid_hw->counters.error_irq_count++;

		CAM_DBG(CAM_ISP, "CSID[%u] Recoverable Error Count:%u",
			csid_hw->hw_intf->hw_idx,
			csid_hw->counters.error_irq_count);

		if (csid_hw->counters.error_irq_count >
			CAM_IFE_CSID_MAX_ERR_COUNT) {
			csid_hw->flags.fatal_err_detected = true;
			cam_ife_csid_ver2_stop_csi2_in_err(csid_hw);
		}
	}
end:
	rc  = cam_ife_csid_ver2_get_evt_payload(csid_hw, &evt_payload,
			&csid_hw->rx_free_payload_list,
			&csid_hw->rx_payload_lock);
	if (rc) {
		for (i = 0; i < th_payload->num_registers; i++)
			CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID:%d status_%d: 0x%X",
				csid_hw->hw_intf->hw_idx, i,
				th_payload->evt_status_arr[i]);
	} else {
		evt_payload->irq_reg_val[CAM_IFE_CSID_IRQ_REG_RX] =
			th_payload->evt_status_arr[CAM_IFE_CSID_IRQ_REG_RX];
		th_payload->evt_payload_priv = evt_payload;
	}
	return rc;
}

static int cam_ife_csid_ver2_handle_rx_debug_event(
	struct cam_ife_csid_ver2_hw *csid_hw,
	uint32_t bit_pos)
{
	struct cam_hw_soc_info              *soc_info;
	struct cam_ife_csid_ver2_reg_info *csid_reg;
	const struct cam_ife_csid_csi2_rx_reg_info *csi2_reg;
	uint32_t mask, val;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	csi2_reg = csid_reg->csi2_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	mask  = BIT(bit_pos);

	switch (mask) {
	case IFE_CSID_VER2_RX_LONG_PKT_CAPTURED:

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

	case IFE_CSID_VER2_RX_SHORT_PKT_CAPTURED:

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
	case IFE_CSID_VER2_RX_CPHY_PKT_HDR_CAPTURED:

		val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			csi2_reg->captured_cphy_pkt_hdr_addr);
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"Csid :%d CPHY pkt VC: %d DT: %d LC: %d",
			csid_hw->hw_intf->hw_idx,
			val & csi2_reg->vc_mask,
			val & csi2_reg->dt_mask,
			val & csi2_reg->wc_mask);
		break;
	case IFE_CSID_VER2_RX_UNMAPPED_VC_DT:
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
			csid_reg->rx_irq_desc[bit_pos].desc);
		break;
	}

	return 0;
}

static int cam_ife_csid_ver2_rx_top_half(
	uint32_t                                   evt_id,
	struct cam_irq_th_payload                 *th_payload)
{
	struct cam_ife_csid_ver2_hw                *csid_hw = NULL;
	const struct cam_ife_csid_csi2_rx_reg_info *csi2_reg;
	struct cam_ife_csid_ver2_reg_info          *csid_reg;
	uint32_t                                    irq_status;
	uint32_t                                    bit_pos = 0;

	csid_hw = th_payload->handler_priv;

	if (!csid_hw) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No private returned");
		return -ENODEV;
	}

	irq_status = th_payload->evt_status_arr[CAM_IFE_CSID_IRQ_REG_RX];

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	csi2_reg = csid_reg->csi2_reg;

	while (irq_status) {

		if ((BIT(bit_pos)) &
				csid_hw->debug_info.rx_mask)
			cam_ife_csid_ver2_handle_rx_debug_event(csid_hw,
				bit_pos);
		bit_pos++;
		irq_status >>= 1;
	}

	return 0;
}

static inline uint32_t cam_ife_csid_ver2_input_core_to_hw_idx(int core_sel)
{
	switch (core_sel) {
	case CAM_IFE_CSID_INPUT_CORE_SEL_SFE_0: return 0;
	case CAM_IFE_CSID_INPUT_CORE_SEL_SFE_1: return 1;
	/**
	 * For all invalid cases, return a very large value
	 * that can never be a valid hw idx.
	 */
	default: return 0xFFFF;
	}
}

static int cam_ife_csid_ver2_handle_event_err(
	struct cam_ife_csid_ver2_hw  *csid_hw,
	uint32_t                      irq_status,
	uint32_t                      err_type,
	bool                          is_secondary,
	struct cam_isp_resource_node *res)
{
	struct cam_isp_hw_error_event_info   err_evt_info;
	struct cam_isp_hw_event_info         evt = {0};
	struct cam_ife_csid_ver2_path_cfg   *path_cfg;

	if (!csid_hw->event_cb) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID[%u] event cb not registered",
			csid_hw->hw_intf->hw_idx);
		return 0;
	}

	evt.hw_idx   = csid_hw->hw_intf->hw_idx;
	evt.reg_val  = irq_status;
	evt.hw_type  = CAM_ISP_HW_TYPE_CSID;
	evt.is_secondary_evt = is_secondary;
	err_evt_info.err_type = err_type;
	evt.event_data = (void *)&err_evt_info;

	if (!is_secondary) {
		if (res) {
			cam_ife_csid_ver2_print_debug_reg_status(csid_hw, res);
			path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
			evt.res_id   = res->res_id;
			CAM_ERR_RATE_LIMIT(CAM_ISP,
				"csid[%u] Res:%s Err 0x%x status 0x%x time_stamp: %lld:%lld",
				csid_hw->hw_intf->hw_idx, res->res_name, err_type,
				irq_status, path_cfg->error_ts.tv_sec,
				path_cfg->error_ts.tv_nsec);
		} else {
			CAM_ERR_RATE_LIMIT(CAM_ISP,
				"csid[%u] Rx Err: 0x%x status 0x%x",
				csid_hw->hw_intf->hw_idx, err_type, irq_status);
		}
	}

	evt.in_core_idx =
		cam_ife_csid_ver2_input_core_to_hw_idx(csid_hw->top_cfg.input_core_type);

	csid_hw->event_cb(csid_hw->token, CAM_ISP_HW_EVENT_ERROR, (void *)&evt);

	return 0;
}

static int cam_ife_csid_ver2_rx_err_bottom_half(
	void                                      *handler_priv,
	void                                      *evt_payload_priv)
{
	const struct cam_ife_csid_csi2_rx_reg_info *csi2_reg;
	struct cam_ife_csid_ver2_evt_payload       *payload;
	struct cam_ife_csid_ver2_hw                *csid_hw = NULL;
	struct cam_ife_csid_ver2_reg_info          *csid_reg;
	struct cam_hw_soc_info                     *soc_info = NULL;
	uint8_t                                    *log_buf = NULL;
	uint32_t                                    irq_status;
	uint32_t                                    rx_irq_status = 0;
	size_t                                      len = 0, i;
	uint32_t                                    val = 0;
	uint32_t                                    event_type = 0;
	uint32_t                                    long_pkt_ftr_val;
	uint32_t                                    total_crc;
	uint32_t                                    data_idx;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return -EINVAL;
	}

	payload = evt_payload_priv;
	csid_hw = handler_priv;
	soc_info = &csid_hw->hw_info->soc_info;
	data_idx = csid_hw->rx_cfg.phy_sel -1;

	log_buf = csid_hw->log_buf;
	memset(log_buf, 0, sizeof(csid_hw->log_buf));

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	csi2_reg = csid_reg->csi2_reg;

	irq_status = payload->irq_reg_val[CAM_IFE_CSID_IRQ_REG_RX] &
			csi2_reg->fatal_err_mask;

	if (!csid_hw->flags.device_enabled) {
		CAM_DBG(CAM_ISP, "bottom-half after stop [0x%x]", irq_status);
		goto end;
	}

	spin_lock(&csid_hw->lock_state);
	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID[%d] powered down state",
			csid_hw->hw_intf->hw_idx);
		goto unlock;
	}

	if (irq_status) {
		bool lane_overflow = false;
		char tmp_buf[10];
		int tmp_len = 0;

		for (i = 0; i < 4; i++) {
			/* NOTE: Hardware specific bits */
			if (irq_status & (IFE_CSID_VER2_RX_LANE0_FIFO_OVERFLOW << i)) {
				tmp_len += scnprintf(tmp_buf + tmp_len, 10 - tmp_len, " %d", i);
				lane_overflow = true;
			}
		}

		if (lane_overflow)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"INPUT_FIFO_OVERFLOW [Lanes:%s]: Skew/Less Data on lanes/ Slow csid clock:%luHz",
				tmp_buf, soc_info->applied_src_clk_rate);

		if (irq_status & IFE_CSID_VER2_RX_ERROR_CPHY_PH_CRC)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"CPHY_PH_CRC: Pkt Hdr CRC mismatch");

		if (irq_status & IFE_CSID_VER2_RX_STREAM_UNDERFLOW) {
			val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
				csi2_reg->captured_long_pkt_0_addr);

			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"STREAM_UNDERFLOW: Fewer bytes rcvd than WC:%d in pkt hdr",
				val & 0xFFFF);
		}

		if (irq_status & IFE_CSID_VER2_RX_ERROR_ECC)
			CAM_ERR_BUF(CAM_ISP, log_buf,
				CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"DPHY_ERROR_ECC: Pkt hdr errors unrecoverable. ECC: 0x%x",
				cam_io_r_mb(soc_info->reg_map[0].mem_base +
					csi2_reg->captured_long_pkt_1_addr));

		CAM_ERR(CAM_ISP, "Fatal Errors: %s", log_buf);

		rx_irq_status |= irq_status;
		csid_hw->flags.fatal_err_detected = true;
	}

	irq_status = payload->irq_reg_val[CAM_IFE_CSID_IRQ_REG_RX] &
		csi2_reg->part_fatal_err_mask;

	if (irq_status) {
		len = 0;

		if (irq_status & IFE_CSID_VER2_RX_CPHY_EOT_RECEPTION)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"CPHY_EOT_RECEPTION: No EOT on lane/s, is_EPD: %c, PHY_Type: %s(%u)",
				(csid_hw->rx_cfg.epd_supported & CAM_ISP_EPD_SUPPORT) ? 'Y' : 'N',
				(csid_hw->rx_cfg.lane_type) ? "cphy" : "dphy",
				csid_hw->rx_cfg.lane_type);

		if (irq_status & IFE_CSID_VER2_RX_CPHY_SOT_RECEPTION)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"CPHY_SOT_RECEPTION: Less SOTs on lane/s");

		if (irq_status & IFE_CSID_VER2_RX_ERROR_CRC) {
			long_pkt_ftr_val = cam_io_r_mb(
				soc_info->reg_map[0].mem_base +
				csi2_reg->captured_long_pkt_ftr_addr);
			total_crc = cam_io_r_mb(
				soc_info->reg_map[0].mem_base +
				csi2_reg->total_crc_err_addr);

			if (csid_hw->rx_cfg.lane_type == CAM_ISP_LANE_TYPE_CPHY) {
				val = cam_io_r_mb(
					soc_info->reg_map[0].mem_base +
					csi2_reg->captured_cphy_pkt_hdr_addr);

				CAM_ERR_BUF(CAM_ISP, log_buf,
					CAM_IFE_CSID_LOG_BUF_LEN, &len,
					"PHY_CRC_ERROR: Long pkt payload CRC mismatch. Totl CRC Errs: %u, Rcvd CRC: 0x%x Caltd CRC: 0x%x, VC:%d DT:%d WC:%d",
					total_crc,
					long_pkt_ftr_val & 0xffff, long_pkt_ftr_val >> 16,
					val >> 22, (val >> 16) & 0x3F, val & 0xFFFF);
			} else {
				CAM_ERR_BUF(CAM_ISP, log_buf,
					CAM_IFE_CSID_LOG_BUF_LEN, &len,
					"PHY_CRC_ERROR: Long pkt payload CRC mismatch. Totl CRC Errs: %u, Rcvd CRC: 0x%x Caltd CRC: 0x%x",
					total_crc,
					long_pkt_ftr_val & 0xffff, long_pkt_ftr_val >> 16);
			}
		}

		if (irq_status & IFE_CSID_VER2_RX_UNBOUNDED_FRAME)
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"UNBOUNDED_FRAME: Frame started with EOF or No EOF");

		CAM_ERR(CAM_ISP, "Recoverable-errors: %s", log_buf);
		rx_irq_status |= irq_status;
	}

	irq_status = payload->irq_reg_val[CAM_IFE_CSID_IRQ_REG_RX] &
		csi2_reg->non_fatal_err_mask;

	if (irq_status) {
		len = 0;

		if (irq_status & IFE_CSID_VER2_RX_MMAPPED_VC_DT) {
			val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
				csi2_reg->captured_long_pkt_0_addr);

			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len,
				"MMAPPED_VC_DT: VC:%d DT:%d mapped to more than 1 csid paths",
				(val >> 22), ((val >> 16) & 0x3F));
		}

		CAM_ERR(CAM_ISP, "Non-fatal-errors: %s", log_buf);
	}

	CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID[%u] Rx Status 0x%x",
		csid_hw->hw_intf->hw_idx,
		payload->irq_reg_val[CAM_IFE_CSID_IRQ_REG_RX]);

	if (csid_hw->flags.fatal_err_detected) {
		event_type |= CAM_ISP_HW_ERROR_CSID_FATAL;
		cam_subdev_notify_message(CAM_CSIPHY_DEVICE_TYPE,
			CAM_SUBDEV_MESSAGE_IRQ_ERR, (void *)&data_idx);
	}

	if (event_type)
		cam_ife_csid_ver2_handle_event_err(csid_hw,
			rx_irq_status, event_type, false, NULL);
unlock:
	spin_unlock(&csid_hw->lock_state);
end:
	cam_ife_csid_ver2_put_evt_payload(csid_hw, &payload,
		&csid_hw->rx_free_payload_list,
		&csid_hw->rx_payload_lock);

	return 0;
}

static void cam_ife_csid_ver2_print_illegal_programming_irq_status(
	struct cam_ife_csid_ver2_hw *csid_hw,
	struct cam_isp_resource_node    *res)
{
	struct cam_ife_csid_ver2_reg_info *csid_reg = csid_hw->core_info->csid_reg;
	struct cam_ife_csid_ver2_path_cfg *path_cfg =
		(struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	struct cam_ife_csid_cid_data      *cid_data = &csid_hw->cid_data[path_cfg->cid];
	struct cam_hw_soc_info            *soc_info = &csid_hw->hw_info->soc_info;
	void __iomem                      *base =
		soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg =
		csid_reg->path_reg[res->res_id];
	uint32_t vcdt_cfg0 = 0, cfg0 = 0, mup_rup_cfg = 0, cfg1 = 0;
	uint32_t decode_fmt = 0, decode_fmt1 = 0;
	uint32_t vc, dt, vc1, dt1;

	cfg0 = cam_io_r_mb(base + path_reg->cfg0_addr);
	cfg1 = cam_io_r_mb(base + path_reg->cfg1_addr);
	vcdt_cfg0 = cam_io_r_mb(base + path_reg->multi_vcdt_cfg0_addr);
	mup_rup_cfg = cam_io_r_mb(base + csid_reg->cmn_reg->rup_aup_cmd_addr);

	if (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].valid) {
		decode_fmt = ((cfg0 >>
			csid_reg->cmn_reg->decode_format_shift_val) &
			csid_reg->cmn_reg->decode_format_mask);
		decode_fmt1 = ((cfg1 >>
			csid_reg->cmn_reg->decode_format1_shift_val) &
			csid_reg->cmn_reg->decode_format_mask);
		vc = ((cfg0 >> csid_reg->cmn_reg->vc_shift_val) &
			csid_reg->cmn_reg->vc_mask);
		dt = ((cfg0 >> csid_reg->cmn_reg->dt_shift_val) &
			csid_reg->cmn_reg->dt_mask);
		vc1 = ((cfg1 >> csid_reg->cmn_reg->multi_vcdt_vc1_shift_val) &
			csid_reg->cmn_reg->vc_mask);
		dt1 = ((cfg1 >> csid_reg->cmn_reg->multi_vcdt_dt1_shift_val) &
			csid_reg->cmn_reg->dt_mask);

		if ((decode_fmt == csid_reg->cmn_reg->decode_format_payload_only) ||
			(decode_fmt1 == csid_reg->cmn_reg->decode_format_payload_only)) {
			if (decode_fmt1 != decode_fmt) {
				CAM_ERR(CAM_ISP,
					"CSID:%d decode_fmt %d decode_fmt1 %d mismatch",
					csid_hw->hw_intf->hw_idx,
					decode_fmt,
					decode_fmt1);
			}
		}

		if ((vc == vc1) && (dt == dt1)) {
			if (decode_fmt != decode_fmt1) {
				CAM_ERR(CAM_ISP,
					"CSID:%d Wrong multi VC-DT configuration",
					csid_hw->hw_intf->hw_idx);
				CAM_ERR(CAM_ISP,
					"fmt %d fmt1 %d vc %d vc1 %d dt %d dt1 %d",
					decode_fmt, decode_fmt, vc, vc1, dt, dt1);

			}
		}
	}

	if (!((mup_rup_cfg & path_reg->rup_aup_mask) &&
		(mup_rup_cfg & csid_reg->cmn_reg->mup_shift_val))) {
		CAM_ERR(CAM_ISP,
			"CSID:%d MUP bit %d is programmed without RUP %d",
			csid_hw->hw_intf->hw_idx,
			mup_rup_cfg & path_reg->rup_aup_mask,
			mup_rup_cfg & csid_reg->cmn_reg->mup_shift_val);
	}

	if (!(csid_hw->debug_info.debug_val &
		    CAM_IFE_CSID_DEBUG_DISABLE_EARLY_EOF) &&
		csid_reg->cmn_reg->early_eof_supported) {
		if (!((cfg1 & path_reg->early_eof_en_shift_val) &&
			(cfg1 & path_reg->crop_v_en_shift_val))) {
			CAM_ERR(CAM_ISP,
				"CSID:%d Early EOF %d enabled without VCROP %d",
				csid_hw->hw_intf->hw_idx,
				cfg1 & path_reg->early_eof_en_shift_val,
				cfg1 & path_reg->crop_v_en_shift_val);

		}
	}

	CAM_INFO(CAM_ISP, "CSID:%d Illegal Programming for res [id: %d name: %s]",
		csid_hw->hw_intf->hw_idx, res->res_id, res->res_name);
}

static void cam_ife_csid_ver2_print_debug_reg_status(
	struct cam_ife_csid_ver2_hw *csid_hw,
	struct cam_isp_resource_node    *res)
{
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	void __iomem *mem_base;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg = NULL;
	uint32_t val0 = 0, val1 = 0, val2 = 0;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
		    csid_hw->core_info->csid_reg;

	mem_base = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;

	path_reg = csid_reg->path_reg[res->res_id];

	val0 = cam_io_r_mb(mem_base +
		path_reg->debug_camif_0_addr);
	val1 = cam_io_r_mb(mem_base +
		path_reg->debug_camif_1_addr);
	val2 = cam_io_r_mb(mem_base +
		path_reg->debug_halt_status_addr);

	CAM_INFO(CAM_ISP,
		"debug_camif_0: 0x%x debug_camif_1: 0x%x halt_status: 0x%x for res: %s ",
		 val0, val1, val2, res->res_name);
}

static int cam_ife_csid_ver2_parse_path_irq_status(
	struct cam_ife_csid_ver2_hw *csid_hw,
	uint32_t                     index,
	uint32_t                     err_mask,
	uint32_t                     irq_status)
{
	const uint8_t                          **irq_reg_tag;
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	uint32_t                                 bit_pos = 0;
	uint32_t                                 status;
	uint32_t                                 sof_irq_debug_en = 0;
	size_t                                   len = 0;
	uint8_t                                 *log_buf = NULL;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
		    csid_hw->core_info->csid_reg;

	log_buf = csid_hw->log_buf;
	memset(log_buf, 0, sizeof(csid_hw->log_buf));

	irq_reg_tag = cam_ife_csid_get_irq_reg_tag_ptr();

	status = irq_status & err_mask;
	while (status) {
		if (status & 0x1 )
			CAM_ERR_BUF(CAM_ISP, log_buf, CAM_IFE_CSID_LOG_BUF_LEN, &len, "%s",
				csid_reg->path_irq_desc[bit_pos].desc);
		bit_pos++;
		status >>= 1;
	}

	if (len)
		CAM_ERR_RATE_LIMIT(CAM_ISP, "CSID[%d] %s status: 0x%x Errors:%s",
			csid_hw->hw_intf->hw_idx, irq_reg_tag[index],
			irq_status, log_buf);

	status = irq_status & csid_hw->debug_info.path_mask;
	bit_pos = 0;
	while (status) {

		if (status & 0x1)
			CAM_INFO_RATE_LIMIT(CAM_ISP, "CSID[%d] IRQ %s %s ",
				csid_hw->hw_intf->hw_idx, irq_reg_tag[index],
				csid_reg->path_irq_desc[bit_pos].desc);

		bit_pos++;
		status >>= 1;
	}

	if (csid_hw->flags.sof_irq_triggered) {

		if (irq_status & IFE_CSID_VER2_PATH_INFO_INPUT_SOF)
			csid_hw->counters.irq_debug_cnt++;

		if (csid_hw->counters.irq_debug_cnt >=
			CAM_CSID_IRQ_SOF_DEBUG_CNT_MAX) {
			cam_ife_csid_ver2_sof_irq_debug(csid_hw,
				&sof_irq_debug_en);
			csid_hw->counters.irq_debug_cnt = 0;
		}
	}

	return 0;
}

static int cam_ife_csid_ver2_top_err_irq_bottom_half(
	void                                      *handler_priv,
	void                                      *evt_payload_priv)
{
	struct cam_ife_csid_ver2_evt_payload       *payload;
	struct cam_ife_csid_ver2_hw                *csid_hw = NULL;
	struct cam_ife_csid_ver2_reg_info          *csid_reg;
	uint32_t                                    irq_status;
	uint32_t                                    event_type = 0;
	uint32_t                                    i = 0;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return -EINVAL;
	}

	payload = evt_payload_priv;
	csid_hw = handler_priv;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;

	irq_status = payload->irq_reg_val[CAM_IFE_CSID_IRQ_REG_TOP] &
			csid_reg->cmn_reg->top_err_irq_mask;

	if (!irq_status) {
		CAM_ERR(CAM_ISP, "Unexpected Scenario");
		return 0;
	}

	for (i = 0; i < csid_reg->num_top_err_irqs; i++) {
		if (csid_reg->top_irq_desc[i].bitmask &
			irq_status) {
			CAM_ERR(CAM_ISP, "%s %s",
				csid_reg->top_irq_desc[i].err_name,
				csid_reg->top_irq_desc[i].desc);
			event_type |= csid_reg->top_irq_desc[i].err_type;
		}
	}

	if (event_type)
		cam_ife_csid_ver2_handle_event_err(csid_hw,
			irq_status, event_type, false, NULL);

	cam_ife_csid_ver2_put_evt_payload(csid_hw, &payload,
		&csid_hw->path_free_payload_list,
		&csid_hw->path_payload_lock);

	return 0;
}

static void cam_ife_csid_ver2_print_format_measure_info(
	struct cam_ife_csid_ver2_hw *csid_hw,
	struct cam_isp_resource_node *res)
{
	uint32_t expected_frame = 0, actual_frame = 0;
	struct cam_ife_csid_ver2_reg_info *csid_reg = csid_hw->core_info->csid_reg;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg =
		csid_reg->path_reg[res->res_id];
	struct cam_hw_soc_info *soc_info = &csid_hw->hw_info->soc_info;
	void __iomem *base = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;

	actual_frame = cam_io_r_mb(base + path_reg->format_measure0_addr);
	expected_frame = cam_io_r_mb(base + path_reg->format_measure_cfg1_addr);

	CAM_INFO(CAM_ISP, "CSID[%u] res [id :%d name : %s]",
		csid_hw->hw_intf->hw_idx,
		res->res_id, res->res_name);
	CAM_ERR_RATE_LIMIT(CAM_ISP, "Frame Size Error Expected[h: %u w: %u] Actual[h: %u w: %u]",
		((expected_frame >>
		csid_reg->cmn_reg->format_measure_height_shift_val) &
		csid_reg->cmn_reg->format_measure_height_mask_val),
		expected_frame &
		csid_reg->cmn_reg->format_measure_width_mask_val,
		((actual_frame >>
		csid_reg->cmn_reg->format_measure_height_shift_val) &
		csid_reg->cmn_reg->format_measure_height_mask_val),
		actual_frame &
		csid_reg->cmn_reg->format_measure_width_mask_val);
}

static int cam_ife_csid_ver2_ipp_bottom_half(
	void                                      *handler_priv,
	void                                      *evt_payload_priv)
{
	struct cam_ife_csid_ver2_evt_payload         *payload;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg;
	struct cam_ife_csid_ver2_reg_info            *csid_reg;
	struct cam_isp_resource_node                 *res;
	struct cam_ife_csid_ver2_hw                  *csid_hw = NULL;
	struct cam_isp_hw_event_info                  evt_info;
	struct cam_hw_soc_info                       *soc_info;
	struct cam_hw_info                           *hw_info;
	struct cam_ife_csid_ver2_path_cfg            *path_cfg;
	uint32_t                                      irq_status_ipp;
	uint32_t                                      err_mask;
	uint32_t                                      err_type = 0;
	void    __iomem                              *base;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return -EINVAL;
	}

	payload = evt_payload_priv;
	res   =  handler_priv;
	hw_info = (struct cam_hw_info *)res->hw_intf->hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;
	csid_reg = csid_hw->core_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	base  = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	irq_status_ipp = payload->irq_reg_val[path_cfg->irq_reg_idx];

	CAM_DBG(CAM_ISP, "CSID[%u] IPP status:0x%x", csid_hw->hw_intf->hw_idx,
		irq_status_ipp);

	if (!csid_hw->flags.device_enabled) {
		CAM_DBG(CAM_ISP, "bottom-half after stop [0x%x]", irq_status_ipp);
		goto end;
	}

	evt_info.hw_type  = CAM_ISP_HW_TYPE_CSID;
	evt_info.hw_idx   = csid_hw->hw_intf->hw_idx;
	evt_info.res_id   = CAM_IFE_PIX_PATH_RES_IPP;
	evt_info.res_type = CAM_ISP_RESOURCE_PIX_PATH;
	evt_info.reg_val  = irq_status_ipp;

	if (irq_status_ipp & IFE_CSID_VER2_PATH_CAMIF_EOF) {
		if (csid_hw->event_cb)
			csid_hw->event_cb(csid_hw->token,
				CAM_ISP_HW_EVENT_EOF, (void *)&evt_info);
	}

	if (irq_status_ipp & IFE_CSID_VER2_PATH_CAMIF_SOF) {
		if (csid_hw->event_cb)
			csid_hw->event_cb(csid_hw->token,
				CAM_ISP_HW_EVENT_SOF, (void *)&evt_info);
	}

	if (irq_status_ipp & IFE_CSID_VER2_PATH_RUP_DONE) {
		if (csid_hw->event_cb)
			csid_hw->event_cb(csid_hw->token,
				CAM_ISP_HW_EVENT_REG_UPDATE, (void *)&evt_info);
	}

	if (irq_status_ipp & IFE_CSID_VER2_PATH_CAMIF_EPOCH0) {
		if (csid_hw->event_cb)
			csid_hw->event_cb(csid_hw->token,
				CAM_ISP_HW_EVENT_EPOCH, (void *)&evt_info);
	}

	path_reg = csid_reg->path_reg[res->res_id];
	err_mask = path_reg->fatal_err_mask | path_reg->non_fatal_err_mask;
	spin_lock(&csid_hw->lock_state);
	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID[%d] powered down state",
			csid_hw->hw_intf->hw_idx);
		goto unlock;
	}

	cam_ife_csid_ver2_parse_path_irq_status(
		csid_hw,
		CAM_IFE_CSID_IRQ_REG_IPP,
		err_mask, irq_status_ipp);


	if (irq_status_ipp & IFE_CSID_VER2_PATH_ERROR_ILLEGAL_PROGRAM) {
		if (path_reg->fatal_err_mask &
			IFE_CSID_VER2_PATH_ERROR_ILLEGAL_PROGRAM) {
			CAM_ERR(CAM_ISP, "CSID[%u] Illegal Programming for IPP status:0x%x",
				csid_hw->hw_intf->hw_idx,irq_status_ipp);
			cam_ife_csid_ver2_print_illegal_programming_irq_status(
				csid_hw, res);
		} else {
			CAM_ERR(CAM_ISP, "Illegal Programming IRQ is not supported");
			CAM_INFO(CAM_ISP, "CSID[%u]: status:0x%x",
				csid_hw->hw_intf->hw_idx, irq_status_ipp);
		}
		err_type |= CAM_ISP_HW_ERROR_CSID_FATAL;
	}

	if (irq_status_ipp & IFE_CSID_VER2_PATH_RECOVERY_OVERFLOW)
		err_type |= CAM_ISP_HW_ERROR_RECOVERY_OVERFLOW;

	if (irq_status_ipp & (IFE_CSID_VER2_PATH_ERROR_PIX_COUNT |
		IFE_CSID_VER2_PATH_ERROR_LINE_COUNT)) {
		cam_ife_csid_ver2_print_format_measure_info(csid_hw, res);
		err_type |= CAM_ISP_HW_ERROR_CSID_FRAME_SIZE;
	}

	if (err_type)
		cam_ife_csid_ver2_handle_event_err(csid_hw,
			irq_status_ipp,
			err_type,
			false,
			res);
unlock:
	spin_unlock(&csid_hw->lock_state);
end:
	cam_ife_csid_ver2_put_evt_payload(csid_hw, &payload,
			&csid_hw->path_free_payload_list,
			&csid_hw->path_payload_lock);

	return 0;
}

static int cam_ife_csid_ver2_ppp_bottom_half(
	void                                      *handler_priv,
	void                                      *evt_payload_priv)
{
	const struct cam_ife_csid_ver2_path_reg_info *path_reg = NULL;
	struct cam_ife_csid_ver2_evt_payload         *payload;
	struct cam_ife_csid_ver2_path_cfg            *path_cfg;
	struct cam_ife_csid_ver2_reg_info            *csid_reg;
	struct cam_isp_resource_node                 *res;
	struct cam_ife_csid_ver2_hw                  *csid_hw = NULL;
	struct cam_hw_soc_info                       *soc_info;
	struct cam_hw_info                           *hw_info;
	void    __iomem                              *base;
	uint32_t                                      irq_status_ppp;
	uint32_t                                      err_mask;
	uint32_t                                      err_type = 0;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return -EINVAL;
	}

	payload = evt_payload_priv;
	res   =  handler_priv;
	hw_info = (struct cam_hw_info *)res->hw_intf->hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;
	soc_info = &csid_hw->hw_info->soc_info;
	base  = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;

	irq_status_ppp = payload->irq_reg_val[path_cfg->irq_reg_idx];

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;

	res = &csid_hw->path_res[CAM_IFE_CSID_IRQ_REG_PPP];

	path_reg = csid_reg->path_reg[res->res_id];
	err_mask = path_reg->fatal_err_mask | path_reg->non_fatal_err_mask;

	CAM_DBG(CAM_ISP, "CSID[%u] PPP status:0x%x", csid_hw->hw_intf->hw_idx,
		irq_status_ppp);

	if (!csid_hw->flags.device_enabled) {
		CAM_DBG(CAM_ISP, "bottom-half after stop [0x%x]", irq_status_ppp);
		goto end;
	}

	spin_lock(&csid_hw->lock_state);
	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID[%d] powered down state",
			csid_hw->hw_intf->hw_idx);
		goto unlock;
	}
	cam_ife_csid_ver2_parse_path_irq_status(
		csid_hw, CAM_IFE_CSID_IRQ_REG_PPP,
		err_mask, irq_status_ppp);

	if (irq_status_ppp & IFE_CSID_VER2_PATH_ERROR_ILLEGAL_PROGRAM) {
		if (path_reg->fatal_err_mask &
			IFE_CSID_VER2_PATH_ERROR_ILLEGAL_PROGRAM) {
			CAM_ERR(CAM_ISP, "CSID[%u] Illegal Programming for PPP status:0x%x",
				csid_hw->hw_intf->hw_idx,irq_status_ppp);
			cam_ife_csid_ver2_print_illegal_programming_irq_status(
				csid_hw, res);
		} else {
			CAM_ERR(CAM_ISP, "Illegal Programming IRQ is not supported");
			CAM_INFO(CAM_ISP, "CSID[%u]: status:0x%x",
				csid_hw->hw_intf->hw_idx, irq_status_ppp);
		}
		err_type |= CAM_ISP_HW_ERROR_CSID_FATAL;
	}

	if (irq_status_ppp & (IFE_CSID_VER2_PATH_ERROR_PIX_COUNT |
		IFE_CSID_VER2_PATH_ERROR_LINE_COUNT)) {
		cam_ife_csid_ver2_print_format_measure_info(csid_hw, res);
		err_type |= CAM_ISP_HW_ERROR_CSID_FRAME_SIZE;
	}

	if (irq_status_ppp & IFE_CSID_VER2_PATH_RECOVERY_OVERFLOW)
		err_type |= CAM_ISP_HW_ERROR_RECOVERY_OVERFLOW;

	if (err_type)
		cam_ife_csid_ver2_handle_event_err(csid_hw,
			irq_status_ppp,
			err_type,
			false,
			res);
unlock:
	spin_unlock(&csid_hw->lock_state);
end:
	cam_ife_csid_ver2_put_evt_payload(csid_hw, &payload,
			&csid_hw->path_free_payload_list,
			&csid_hw->path_payload_lock);

	return 0;
}

static int cam_ife_csid_ver2_rdi_bottom_half(
	void                                      *handler_priv,
	void                                      *evt_payload_priv)
{
	struct cam_ife_csid_ver2_evt_payload         *payload;
	struct cam_ife_csid_ver2_hw                  *csid_hw = NULL;
	struct cam_ife_csid_ver2_reg_info            *csid_reg;
	struct cam_ife_csid_ver2_path_cfg            *path_cfg;
	struct cam_isp_resource_node                 *res;
	const struct cam_ife_csid_ver2_path_reg_info *rdi_reg;
	struct cam_hw_soc_info                       *soc_info;
	struct cam_hw_info                           *hw_info;
	void    __iomem                              *base;
	uint32_t                                      irq_status_rdi;
	uint32_t                                      err_mask;
	uint32_t                                      err_type = 0;
	bool                                          skip_evt_notify = false;
	struct cam_isp_hw_event_info                  evt_info;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return -EINVAL;
	}

	res   =  handler_priv;
	hw_info = (struct cam_hw_info *)res->hw_intf->hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;
	payload = evt_payload_priv;
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;

	evt_info.hw_idx   = csid_hw->hw_intf->hw_idx;
	evt_info.res_type = CAM_ISP_RESOURCE_PIX_PATH;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
		csid_hw->core_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	base  = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;

	irq_status_rdi = payload->irq_reg_val[path_cfg->irq_reg_idx];
	rdi_reg = csid_reg->path_reg[res->res_id];

	if (!rdi_reg)
		goto end;

	if (!csid_hw->flags.device_enabled) {
		CAM_DBG(CAM_ISP, "bottom-half after stop [0x%x]", irq_status_rdi);
		goto end;
	}

	CAM_DBG(CAM_ISP, "CSID[%u] RDI:%d status:0x%x",
			csid_hw->hw_intf->hw_idx,
			res->res_id, irq_status_rdi);
	err_mask = rdi_reg->non_fatal_err_mask |
		rdi_reg->fatal_err_mask;

	spin_lock(&csid_hw->lock_state);
	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID[%d] powered down state",
			csid_hw->hw_intf->hw_idx);
		spin_unlock(&csid_hw->lock_state);
		goto end;
	}

	cam_ife_csid_ver2_parse_path_irq_status(csid_hw,
		path_cfg->irq_reg_idx,
		err_mask, irq_status_rdi);

	if (irq_status_rdi & IFE_CSID_VER2_PATH_ERROR_ILLEGAL_PROGRAM) {
		if (rdi_reg->fatal_err_mask &
			IFE_CSID_VER2_PATH_ERROR_ILLEGAL_PROGRAM) {
			CAM_ERR(CAM_ISP, "CSID[%u]: Illegal Programming for RDI:%d status:0x%x",
				csid_hw->hw_intf->hw_idx,
				res->res_id, irq_status_rdi);
			cam_ife_csid_ver2_print_illegal_programming_irq_status(
				csid_hw, res);
		} else {
			CAM_ERR(CAM_ISP, "Illegal Programming IRQ is not supported");
			CAM_INFO(CAM_ISP, "CSID[%u]: RDI:%d status:0x%x",
				csid_hw->hw_intf->hw_idx,
				res->res_id, irq_status_rdi);
		}
		err_type |= CAM_ISP_HW_ERROR_CSID_FATAL;
	}

	if (irq_status_rdi & IFE_CSID_VER2_PATH_RECOVERY_OVERFLOW)
		err_type |= CAM_ISP_HW_ERROR_RECOVERY_OVERFLOW;

	if (irq_status_rdi & (IFE_CSID_VER2_PATH_ERROR_PIX_COUNT |
				IFE_CSID_VER2_PATH_ERROR_LINE_COUNT)) {
		cam_ife_csid_ver2_print_format_measure_info(csid_hw, res);
		err_type |= CAM_ISP_HW_ERROR_CSID_FRAME_SIZE;
	}

	spin_unlock(&csid_hw->lock_state);
	if (err_type) {

		cam_ife_csid_ver2_handle_event_err(csid_hw,
			irq_status_rdi, err_type, false, res);
		goto end;
	}

	if (!csid_hw->event_cb) {
		CAM_DBG(CAM_ISP, "CSID[%u] no cb registered",
				csid_hw->hw_intf->hw_idx);
		goto end;
	}

	evt_info.res_id = res->res_id;
	evt_info.reg_val = irq_status_rdi;
	evt_info.hw_type = CAM_ISP_HW_TYPE_CSID;

	/* Check for specific secondary events */
	if (path_cfg->sec_evt_config.en_secondary_evt) {
		if ((irq_status_rdi & IFE_CSID_VER2_PATH_CAMIF_SOF) &&
			(path_cfg->sec_evt_config.evt_type & CAM_IFE_CSID_EVT_SOF)) {
			evt_info.is_secondary_evt = true;
			csid_hw->event_cb(csid_hw->token,
				CAM_ISP_HW_EVENT_SOF, (void *)&evt_info);
			skip_evt_notify = true;
		}

		if ((irq_status_rdi & IFE_CSID_VER2_PATH_CAMIF_EPOCH0) &&
			(path_cfg->sec_evt_config.evt_type & CAM_IFE_CSID_EVT_EPOCH)) {
			evt_info.is_secondary_evt = true;
			csid_hw->event_cb(csid_hw->token,
				CAM_ISP_HW_EVENT_EPOCH, (void *)&evt_info);
			skip_evt_notify = true;
		}

		if ((irq_status_rdi &
			IFE_CSID_VER2_PATH_SENSOR_SWITCH_OUT_OF_SYNC_FRAME_DROP) &&
			(path_cfg->sec_evt_config.evt_type &
			CAM_IFE_CSID_EVT_SENSOR_SYNC_FRAME_DROP)) {
			cam_ife_csid_ver2_handle_event_err(csid_hw,
				irq_status_rdi, CAM_ISP_HW_ERROR_CSID_SENSOR_FRAME_DROP, true, res);
		}
	}

	evt_info.is_secondary_evt = false;
	if (!path_cfg->handle_camif_irq)
		goto end;

	if (irq_status_rdi & IFE_CSID_VER2_PATH_CAMIF_EOF)
		csid_hw->event_cb(csid_hw->token,
				CAM_ISP_HW_EVENT_EOF,
				(void *)&evt_info);

	if (!skip_evt_notify && (irq_status_rdi & IFE_CSID_VER2_PATH_CAMIF_SOF))
		csid_hw->event_cb(csid_hw->token,
				CAM_ISP_HW_EVENT_SOF,
				(void *)&evt_info);

	if (irq_status_rdi & IFE_CSID_VER2_PATH_RUP_DONE)
		csid_hw->event_cb(csid_hw->token,
				CAM_ISP_HW_EVENT_REG_UPDATE,
				(void *)&evt_info);

	if (!skip_evt_notify && (irq_status_rdi & IFE_CSID_VER2_PATH_CAMIF_EPOCH0))
		csid_hw->event_cb(csid_hw->token,
				CAM_ISP_HW_EVENT_EPOCH,
				(void *)&evt_info);
end:

	cam_ife_csid_ver2_put_evt_payload(csid_hw, &payload,
			&csid_hw->path_free_payload_list,
			&csid_hw->path_payload_lock);

	return 0;
}

int cam_ife_csid_ver2_get_hw_caps(void *hw_priv,
	void *get_hw_cap_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_ife_csid_hw_caps           *hw_caps;
	struct cam_ife_csid_ver2_hw           *csid_hw;
	struct cam_hw_info                    *hw_info;
	struct cam_csid_soc_private           *soc_private = NULL;
	struct cam_ife_csid_ver2_reg_info     *csid_reg;

	if (!hw_priv || !get_hw_cap_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	hw_info = (struct cam_hw_info  *)hw_priv;

	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;
	hw_caps = (struct cam_ife_csid_hw_caps *) get_hw_cap_args;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
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
	hw_caps->only_master_rup = csid_reg->cmn_reg->only_master_rup;
	hw_caps->is_lite = soc_private->is_ife_csid_lite;

	CAM_DBG(CAM_ISP,
		"CSID:%d num-rdis:%d, num-pix:%d, major:%d minor:%d ver:%d",
		csid_hw->hw_intf->hw_idx, hw_caps->num_rdis,
		hw_caps->num_pix, hw_caps->major_version,
		hw_caps->minor_version, hw_caps->version_incr);

	return rc;
}

static int cam_ife_csid_ver2_wait_for_reset(
	struct cam_ife_csid_ver2_hw *csid_hw)
{
	unsigned long rem_jiffies = 0;
	int rc = 0;

	rem_jiffies = cam_common_wait_for_completion_timeout(
		&csid_hw->hw_info->hw_complete,
		msecs_to_jiffies(CAM_IFE_CSID_RESET_TIMEOUT_MS));

	if (rem_jiffies == 0) {
		rc = -ETIMEDOUT;
		CAM_ERR(CAM_ISP,
			"CSID[%d], sync-mode[%d] reset time out",
			csid_hw->hw_intf->hw_idx,
			csid_hw->sync_mode);
	} else {
		CAM_DBG(CAM_ISP,
		"CSID[%d], sync-mode[%d] reset success",
		csid_hw->hw_intf->hw_idx,
		csid_hw->sync_mode);
	}

	return rc;
}

static int cam_ife_csid_ver2_reset_irq_top_half(uint32_t    evt_id,
	struct cam_irq_th_payload         *th_payload)
{
	struct cam_ife_csid_ver2_hw *csid_hw;

	csid_hw = th_payload->handler_priv;

	CAM_DBG(CAM_ISP, "CSID[%d] TOP_IRQ_STATUS_0 = 0x%x", csid_hw->hw_intf->hw_idx,
		th_payload->evt_status_arr[0]);
	complete(&csid_hw->hw_info->hw_complete);

	return 0;
}

static int cam_ife_csid_ver2_internal_reset(
	struct cam_ife_csid_ver2_hw *csid_hw,
	uint32_t rst_cmd, uint32_t rst_location, uint32_t rst_mode)
{
	uint32_t val = 0;
	struct cam_ife_csid_ver2_reg_info *csid_reg;
	struct cam_hw_soc_info                *soc_info;
	void __iomem *mem_base;
	int rc = 0;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;

	soc_info = &csid_hw->hw_info->soc_info;
	mem_base = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;

	if (csid_hw->hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "CSID[%d] powered down state",
			csid_hw->hw_intf->hw_idx);
		return -EINVAL;
	}

	if (csid_hw->sync_mode == CAM_ISP_HW_SYNC_SLAVE &&
		rst_cmd == CAM_IFE_CSID_RESET_CMD_HW_RST)
		goto wait_only;

	if (rst_cmd == CAM_IFE_CSID_RESET_CMD_SW_RST)
		reinit_completion(&csid_hw->hw_info->hw_complete);

	/*Program the reset location */
	if (rst_location == CAM_IFE_CSID_RESET_LOC_PATH_ONLY)
		val |= (csid_reg->cmn_reg->rst_loc_path_only_val <<
		       csid_reg->cmn_reg->rst_location_shift_val);
	else if (rst_location == CAM_IFE_CSID_RESET_LOC_COMPLETE)
		val |= (csid_reg->cmn_reg->rst_loc_complete_csid_val <<
		       csid_reg->cmn_reg->rst_location_shift_val);

	/*Program the mode */
	if (rst_mode == CAM_CSID_HALT_AT_FRAME_BOUNDARY)
		val |= (csid_reg->cmn_reg->rst_mode_frame_boundary_val <<
			csid_reg->cmn_reg->rst_mode_shift_val);
	else if (rst_mode == CAM_CSID_HALT_IMMEDIATELY)
		val |= (csid_reg->cmn_reg->rst_mode_immediate_val <<
			csid_reg->cmn_reg->rst_mode_shift_val);

	cam_io_w_mb(val, mem_base + csid_reg->cmn_reg->reset_cfg_addr);
	val = 0;

	/*Program the cmd */
	if (rst_cmd == CAM_IFE_CSID_RESET_CMD_IRQ_CTRL)
		val = csid_reg->cmn_reg->rst_cmd_irq_ctrl_only_val;
	else if (rst_cmd == CAM_IFE_CSID_RESET_CMD_HW_RST)
		val = csid_reg->cmn_reg->rst_cmd_hw_reset_complete_val;
	else if (rst_cmd == CAM_IFE_CSID_RESET_CMD_SW_RST)
		val = csid_reg->cmn_reg->rst_cmd_sw_reset_complete_val;

	cam_io_w_mb(val, mem_base + csid_reg->cmn_reg->reset_cmd_addr);

wait_only:

	rc = cam_ife_csid_ver2_wait_for_reset(csid_hw);

	if (rc)
		CAM_ERR(CAM_ISP,
			"CSID[%u] Reset failed mode %d cmd %d loc %d",
			csid_hw->hw_intf->hw_idx,
			rst_mode, rst_cmd, rst_location);
	reinit_completion(&csid_hw->hw_info->hw_complete);
	return rc;
}

int cam_ife_csid_ver2_reset(void *hw_priv,
	void *reset_args, uint32_t arg_size)
{
	struct cam_hw_info *hw_info;
	struct cam_ife_csid_ver2_hw *csid_hw;
	struct cam_csid_reset_cfg_args  *reset;
	int rc = 0;

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;
	reset   = (struct cam_csid_reset_cfg_args  *)reset_args;

	mutex_lock(&csid_hw->hw_info->hw_mutex);

	switch (reset->reset_type) {
	case CAM_IFE_CSID_RESET_GLOBAL:
		rc = cam_ife_csid_ver2_internal_reset(csid_hw,
			CAM_IFE_CSID_RESET_CMD_SW_RST,
			CAM_IFE_CSID_RESET_LOC_COMPLETE,
			CAM_CSID_HALT_IMMEDIATELY);
		break;

	case CAM_IFE_CSID_RESET_PATH:
		rc = cam_ife_csid_ver2_internal_reset(csid_hw,
			CAM_IFE_CSID_RESET_CMD_HW_RST,
			CAM_IFE_CSID_RESET_LOC_PATH_ONLY,
			CAM_CSID_HALT_IMMEDIATELY);
		break;

	default:
		rc = -EINVAL;
		break;
	}

	if (rc)
		CAM_ERR(CAM_ISP, "CSID[%d] reset type: %s failed",
			csid_hw->hw_intf->hw_idx,
			cam_ife_csid_reset_type_to_string(reset->reset_type));
	else
		CAM_DBG(CAM_ISP, "CSID[%d] reset type: %s",
			csid_hw->hw_intf->hw_idx,
			cam_ife_csid_reset_type_to_string(reset->reset_type));

	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_ife_csid_ver2_disable_path(
	struct cam_ife_csid_ver2_hw     *csid_hw,
	struct cam_isp_resource_node    *res)
{
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	struct cam_ife_csid_ver2_path_cfg       *path_cfg;
	int                                      rc = 0;

	if (res->res_state != CAM_ISP_RESOURCE_STATE_STREAMING) {
		CAM_ERR(CAM_ISP,
			"CSID:%d path res type:%d res_id:%d Invalid state:%d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	if (res->res_id >= CAM_IFE_PIX_PATH_RES_MAX) {
		CAM_DBG(CAM_ISP, "CSID:%d Invalid res id%d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;

	if (path_cfg->irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			csid_hw->csid_irq_controller,
			path_cfg->irq_handle);
		path_cfg->irq_handle = 0;
	}

	if (path_cfg->err_irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			csid_hw->csid_irq_controller,
			path_cfg->err_irq_handle);
		path_cfg->err_irq_handle = 0;
	}

	if (path_cfg->discard_irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			csid_hw->csid_irq_controller,
			path_cfg->discard_irq_handle);
		path_cfg->discard_irq_handle = 0;
	}

	/* Reset frame drop fields at stream off */
	path_cfg->discard_init_frames = false;
	path_cfg->skip_discard_frame_cfg = false;
	path_cfg->num_frames_discard = 0;
	path_cfg->sof_cnt = 0;
	return rc;
}

static int cam_ife_csid_ver2_decode_format1_validate(
	struct cam_ife_csid_ver2_hw *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	const struct cam_ife_csid_ver2_reg_info *csid_reg =
		(struct cam_ife_csid_ver2_reg_info *)csid_hw->core_info->csid_reg;
	struct cam_ife_csid_ver2_path_cfg *path_cfg =
		(struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	struct cam_ife_csid_cid_data *cid_data = &csid_hw->cid_data[path_cfg->cid];

	/* Validation is only required  for multi vc dt use case */
	if (!cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].valid)
		return rc;

	if ((path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt ==
		csid_reg->cmn_reg->decode_format_payload_only) ||
		(path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].decode_fmt  ==
		 csid_reg->cmn_reg->decode_format_payload_only)) {
		if (path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt !=
			path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].decode_fmt) {
			CAM_ERR(CAM_ISP,
				"CSID:%d decode_fmt %d decode_fmt1 %d mismatch",
				csid_hw->hw_intf->hw_idx,
				path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt,
				path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].decode_fmt);
			rc = -EINVAL;
			goto err;
		}
	}

	if ((cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].vc ==
		cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc) &&
		(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].dt ==
		cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt)) {
		if (path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt !=
			path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].decode_fmt) {
			CAM_ERR(CAM_ISP,
				"CSID:%d Wrong multi VC-DT configuration",
					csid_hw->hw_intf->hw_idx);
			CAM_ERR(CAM_ISP,
				"fmt %d fmt1 %d vc %d vc1 %d dt %d dt1 %d",
				path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt,
				path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].decode_fmt,
				cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc,
				cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].vc,
				cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt,
				cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].dt);
			rc = -EINVAL;
			goto err;
		}
	}

	return rc;
err:
	CAM_ERR(CAM_ISP, "Invalid decode fmt1 cfg csid[%d] res [id %d name %s] rc %d",
		csid_hw->hw_intf->hw_idx, res->res_id, res->res_name, rc);
	return rc;
}

static int cam_ife_csid_hw_ver2_config_path_data(
	struct cam_ife_csid_ver2_hw *csid_hw,
	struct cam_ife_csid_ver2_path_cfg *path_cfg,
	struct cam_csid_hw_reserve_resource_args  *reserve,
	uint32_t cid)
{
	int rc = 0, i = 0;
	bool is_rpp = false;
	const struct cam_ife_csid_ver2_reg_info *csid_reg =
		(struct cam_ife_csid_ver2_reg_info *)csid_hw->core_info->csid_reg;
	struct cam_ife_csid_cid_data *cid_data = &csid_hw->cid_data[cid];
	struct cam_isp_resource_node *res = &csid_hw->path_res[reserve->res_id];

	for(i = 0; i < reserve->in_port->num_valid_vc_dt; i++)
		path_cfg->in_format[i] = reserve->in_port->format[i];

	path_cfg->cid = cid;
	path_cfg->out_format = reserve->out_port->format;
	path_cfg->sync_mode = reserve->sync_mode;
	path_cfg->height  = reserve->in_port->height;
	path_cfg->start_line = reserve->in_port->line_start;
	path_cfg->end_line = reserve->in_port->line_stop;
	path_cfg->crop_enable = reserve->crop_enable;
	path_cfg->drop_enable = reserve->drop_enable;
	path_cfg->horizontal_bin = reserve->in_port->horizontal_bin;
	path_cfg->vertical_bin = reserve->in_port->vertical_bin;
	path_cfg->qcfa_bin = reserve->in_port->qcfa_bin;
	path_cfg->num_bytes_out = reserve->in_port->num_bytes_out;
	path_cfg->sec_evt_config.en_secondary_evt = reserve->sec_evt_config.en_secondary_evt;
	path_cfg->sec_evt_config.evt_type = reserve->sec_evt_config.evt_type;

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

	switch (reserve->res_id) {
	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
	case CAM_IFE_PIX_PATH_RES_RDI_4:
		is_rpp = path_cfg->crop_enable || path_cfg->drop_enable;
		rc = cam_ife_csid_get_format_rdi(
			path_cfg->in_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0],
			path_cfg->out_format,
			&path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0],
			is_rpp);
		if (rc)
			goto end;

		if (csid_reg->cmn_reg->decode_format1_supported &&
			(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].valid)) {

			rc = cam_ife_csid_get_format_rdi(
				path_cfg->in_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1],
				path_cfg->out_format,
				&path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1],
				is_rpp);
			if (rc)
				goto end;
		}
		break;
	case CAM_IFE_PIX_PATH_RES_IPP:
	case CAM_IFE_PIX_PATH_RES_PPP:
		rc = cam_ife_csid_get_format_ipp_ppp(
			path_cfg->in_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0],
			&path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0]);
		if (rc)
			goto end;

		if (csid_reg->cmn_reg->decode_format1_supported &&
			(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].valid)) {

			rc = cam_ife_csid_get_format_ipp_ppp(
				path_cfg->in_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1],
				&path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1]);
			if (rc)
				goto end;
		}
		break;
	default:
		rc = -EINVAL;
		CAM_ERR(CAM_ISP, "Invalid Res id %u", reserve->res_id);
		break;
	}

	if (csid_reg->cmn_reg->decode_format1_supported &&
		(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].valid)) {
		rc = cam_ife_csid_ver2_decode_format1_validate(csid_hw, res);
		if (rc) {
			CAM_ERR(CAM_ISP, "CSID[%d] res %d decode fmt1 validation failed",
				csid_hw->hw_intf->hw_idx, res);
			goto end;
		}
	}

end:
	return rc;
}

static int cam_ife_csid_hw_ver2_config_rx(
	struct cam_ife_csid_ver2_hw *csid_hw,
	struct cam_csid_hw_reserve_resource_args  *reserve)
{

	/*Before calling this function we already validated the
	 * sancitity of in port args. If this function is called
	 * from somewhere else as well, please make sure  to validate the
	 * in_port args before coming here.
	 */
	if (csid_hw->counters.csi2_reserve_cnt) {
		CAM_DBG(CAM_ISP, "CSID %d Rx already reserved cnt %d",
			csid_hw->hw_intf->hw_idx,
			csid_hw->counters.csi2_reserve_cnt);
		csid_hw->counters.csi2_reserve_cnt++;
		return 0;
	}

	csid_hw->rx_cfg.lane_cfg =
		reserve->in_port->lane_cfg;
	csid_hw->rx_cfg.lane_type =
		reserve->in_port->lane_type;
	csid_hw->rx_cfg.lane_num =
		reserve->in_port->lane_num;
	csid_hw->res_type = reserve->in_port->res_type;
	csid_hw->rx_cfg.dynamic_sensor_switch_en =
		reserve->in_port->dynamic_sensor_switch_en;
	if (reserve->in_port->epd_supported)
		csid_hw->rx_cfg.epd_supported = 1;

	switch (reserve->in_port->res_type) {
	case CAM_ISP_IFE_IN_RES_TPG:
		csid_hw->rx_cfg.phy_sel = 0;
		csid_hw->rx_cfg.tpg_mux_sel = 0;
	case CAM_ISP_IFE_IN_RES_CPHY_TPG_0:
		csid_hw->rx_cfg.tpg_mux_sel = 1;
		csid_hw->rx_cfg.tpg_num_sel = 1;
		break;
	case CAM_ISP_IFE_IN_RES_CPHY_TPG_1:
		csid_hw->rx_cfg.tpg_mux_sel = 1;
		csid_hw->rx_cfg.tpg_num_sel = 2;
		break;
	case CAM_ISP_IFE_IN_RES_CPHY_TPG_2:
		csid_hw->rx_cfg.tpg_mux_sel = 1;
		csid_hw->rx_cfg.tpg_num_sel = 3;
		break;
	default:
		csid_hw->rx_cfg.tpg_mux_sel = 0;
		csid_hw->rx_cfg.phy_sel =
			(reserve->in_port->res_type & 0xFF);
		break;
	}

	csid_hw->counters.csi2_reserve_cnt++;
	CAM_DBG(CAM_ISP,
		"CSID:%d Rx lane param: cfg:%u type:%u num:%u res:%u",
		csid_hw->hw_intf->hw_idx,
		reserve->in_port->lane_cfg, reserve->in_port->lane_type,
		reserve->in_port->lane_num, reserve->in_port->res_type);

	return 0;

}

static int cam_ife_csid_ver_config_camif(
	struct cam_ife_csid_ver2_hw *csid_hw,
	struct cam_csid_hw_reserve_resource_args  *reserve,
	struct cam_ife_csid_ver2_path_cfg *path_cfg)
{
	struct cam_ife_csid_ver2_reg_info *csid_reg;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
		csid_hw->core_info->csid_reg;

	switch (reserve->res_id) {
	case CAM_IFE_PIX_PATH_RES_IPP:
	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
	case CAM_IFE_PIX_PATH_RES_RDI_4:
		path_cfg->camif_data.epoch0 =
		(path_cfg->end_line - path_cfg->start_line) /
		csid_reg->cmn_reg->epoch_div_factor;

		CAM_DBG(CAM_ISP, "CSID[%d] res_id: %u epoch0: 0x%x",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			path_cfg->camif_data.epoch0);
		break;
	default:
		CAM_DBG(CAM_ISP, "No CAMIF epoch update for res: %u", reserve->res_id);
		break;
	}

	return 0;
}

int cam_ife_csid_hw_ver2_hw_cfg(
	struct cam_ife_csid_ver2_hw *csid_hw,
	struct cam_ife_csid_ver2_path_cfg *path_cfg,
	struct cam_csid_hw_reserve_resource_args  *reserve,
	uint32_t cid)
{
	int rc = 0;

	rc = cam_ife_csid_hw_ver2_config_rx(csid_hw, reserve);

	if (rc) {
		CAM_ERR(CAM_ISP, "CSID[%d] rx config failed",
			csid_hw->hw_intf->hw_idx);
		return rc;
	}

	cam_ife_csid_hw_ver2_config_path_data(csid_hw, path_cfg,
		reserve, cid);
	rc = cam_ife_csid_ver_config_camif(csid_hw, reserve, path_cfg);

	if (rc)
		CAM_ERR(CAM_ISP, "CSID[%d] camif config failed",
			csid_hw->hw_intf->hw_idx);

	return rc;
}

static int cam_ife_csid_ver2_in_port_validate(
	struct cam_csid_hw_reserve_resource_args  *reserve,
	struct cam_ife_csid_ver2_hw     *csid_hw)
{
	int rc = 0;
	struct cam_ife_csid_ver2_reg_info *csid_reg;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;

	/* check in port args for RT streams*/
	if (!reserve->is_offline) {
		rc  = cam_ife_csid_check_in_port_args(reserve,
			csid_hw->hw_intf->hw_idx);
		if (rc)
			goto err;
	}

	if (csid_hw->counters.csi2_reserve_cnt) {

		if (csid_hw->token != reserve->cb_priv) {
			CAM_ERR(CAM_ISP,
				"CSID[%d] different Context for res %d",
				csid_hw->hw_intf->hw_idx,
				reserve->res_id);
			rc = -EINVAL;
			goto err;
		}

		if (csid_hw->res_type != reserve->in_port->res_type) {
			CAM_ERR(CAM_ISP,
				"CSID[%d] Invalid res[%d] in_res_type[%d]",
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
				"lane: num[%d %d] type[%d %d] cfg[%d %d]",
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

int cam_ife_csid_ver2_reserve(void *hw_priv,
	void *reserve_args, uint32_t arg_size)
{

	struct cam_ife_csid_ver2_hw     *csid_hw;
	struct cam_hw_info              *hw_info;
	struct cam_isp_resource_node    *res = NULL;
	struct cam_csid_hw_reserve_resource_args  *reserve;
	struct cam_ife_csid_ver2_path_cfg    *path_cfg;
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	uint32_t cid;
	int rc = 0;

	reserve = (struct cam_csid_hw_reserve_resource_args  *)reserve_args;

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;

	res = &csid_hw->path_res[reserve->res_id];
	if (res->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		/**
		 * intentionally set as DBG log to since this log gets printed when hw manager
		 * checks if resource is available
		 */
		CAM_DBG(CAM_ISP, "CSID %d Res_id %d state %d",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			res->res_state);
		return -EBUSY;
	}

	rc = cam_ife_csid_ver2_in_port_validate(reserve, csid_hw);
	if (rc) {
		CAM_ERR(CAM_ISP, "CSID %d Res_id %d port validation failed",
			csid_hw->hw_intf->hw_idx, reserve->res_id);
		return rc;
	}

	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
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

	/* Skip rx and csid cfg for offline */
	if (!reserve->is_offline) {
		rc = cam_ife_csid_hw_ver2_hw_cfg(csid_hw, path_cfg,
			reserve, cid);
		if (rc) {
			CAM_ERR(CAM_ISP, "CSID[%d] res %d hw_cfg fail",
				csid_hw->hw_intf->hw_idx, reserve->res_id);
			goto release;
		}
	}

	reserve->node_res = res;
	res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	csid_hw->event_cb = reserve->event_cb;
	csid_hw->tasklet  = reserve->tasklet;
	csid_hw->token  = reserve->cb_priv;
	reserve->buf_done_controller = csid_hw->buf_done_irq_controller;
	res->cdm_ops = reserve->cdm_ops;
	path_cfg->sfe_inline_shdr = reserve->sfe_inline_shdr;
	csid_hw->flags.offline_mode = reserve->is_offline;

	reserve->need_top_cfg = csid_reg->need_top_cfg;

	CAM_DBG(CAM_ISP, "CSID[%u] Resource[id: %d name:%s] state %d cid %d",
		csid_hw->hw_intf->hw_idx, reserve->res_id, res->res_name,
		res->res_state, cid);

	return rc;

release:
	cam_ife_csid_cid_release(&csid_hw->cid_data[cid],
		csid_hw->hw_intf->hw_idx,
		path_cfg->cid);
	return rc;
}

int cam_ife_csid_ver2_release(void *hw_priv,
	void *release_args, uint32_t arg_size)
{
	struct cam_ife_csid_ver2_hw     *csid_hw;
	struct cam_hw_info              *hw_info;
	struct cam_isp_resource_node    *res = NULL;
	struct cam_ife_csid_ver2_path_cfg    *path_cfg;
	int rc = 0;

	if (!hw_priv || !release_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;
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

	CAM_DBG(CAM_ISP, "CSID:%d res type :%d Resource [id:%d name:%s]",
		csid_hw->hw_intf->hw_idx, res->res_type,
		res->res_id, res->res_name);

	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;

	cam_ife_csid_cid_release(&csid_hw->cid_data[path_cfg->cid],
		csid_hw->hw_intf->hw_idx,
		path_cfg->cid);

	memset(path_cfg, 0, sizeof(*path_cfg));

	csid_hw->sync_mode = CAM_ISP_HW_SYNC_NONE;

	if (csid_hw->counters.csi2_reserve_cnt)
		csid_hw->counters.csi2_reserve_cnt--;

	if (!csid_hw->counters.csi2_reserve_cnt) {
		memset(&csid_hw->rx_cfg, 0,
			sizeof(struct cam_ife_csid_rx_cfg));
		memset(&csid_hw->top_cfg, 0,
			sizeof(struct cam_ife_csid_ver2_top_cfg));
		memset(&csid_hw->debug_info, 0,
			sizeof(struct cam_ife_csid_debug_info));
		csid_hw->token = NULL;
	}

	res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
end:
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_ife_csid_ver2_shdr_cfg(
	struct cam_ife_csid_ver2_hw *csid_hw,
	uint32_t res_id)
{
	struct cam_ife_csid_ver2_reg_info *csid_reg;
	uint32_t val;
	void __iomem                      *mem_base;
	struct cam_hw_soc_info            *soc_info;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	mem_base = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;
	val = cam_io_r_mb(mem_base +
		csid_reg->cmn_reg->shdr_master_slave_cfg_addr);

	switch (res_id) {

	case CAM_IFE_PIX_PATH_RES_RDI_0:
		val |= BIT(csid_reg->cmn_reg->shdr_master_rdi0_shift);
		break;
	case CAM_IFE_PIX_PATH_RES_RDI_1:
		val |= BIT(csid_reg->cmn_reg->shdr_slave_rdi1_shift);
		break;
	case CAM_IFE_PIX_PATH_RES_RDI_2:
		val |= BIT(csid_reg->cmn_reg->shdr_slave_rdi2_shift);
		break;
	default:
		break;
	}

	val |= BIT(csid_reg->cmn_reg->shdr_master_slave_en_shift);

	cam_io_w_mb(val, mem_base +
		csid_reg->cmn_reg->shdr_master_slave_cfg_addr);

	CAM_DBG(CAM_ISP, "CSID %d shdr cfg 0x%x", csid_hw->hw_intf->hw_idx,
		val);

	return 0;
}

static int cam_ife_csid_ver2_init_config_rdi_path(
	struct cam_ife_csid_ver2_hw *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg = NULL;
	const struct cam_ife_csid_ver2_common_reg_info *cmn_reg = NULL;
	uint32_t  val, cfg0 = 0, cfg1 = 0;
	struct cam_ife_csid_ver2_path_cfg *path_cfg;
	struct cam_ife_csid_cid_data *cid_data;
	void __iomem *mem_base;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;

	if (!csid_reg->path_reg[res->res_id]) {
		CAM_ERR(CAM_ISP, "CSID:%d RDI:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	cmn_reg = csid_reg->cmn_reg;
	path_reg = csid_reg->path_reg[res->res_id];
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	cid_data = &csid_hw->cid_data[path_cfg->cid];
	mem_base = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;

	/* Enable client & cfg offline mode */
	if (csid_hw->flags.offline_mode) {
		val = (1 <<
			path_reg->offline_mode_en_shift_val);
		val |= (1 << cmn_reg->path_en_shift_val);
		cam_io_w_mb(val, mem_base + path_reg->cfg0_addr);
		CAM_DBG(CAM_ISP, "CSID:%d RDI:%d cfg0: 0x%x for offline",
			csid_hw->hw_intf->hw_idx, res->res_id, val);
		return 0;
	}

	/*Configure cfg0:
	 * VC
	 * DT
	 * Timestamp enable and strobe selection for v780
	 * DT_ID cobination
	 * Decode Format
	 * Frame_id_dec_en
	 * VFR en
	 * offline mode
	 */
	cfg0 = (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc <<
			cmn_reg->vc_shift_val) |
		(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt <<
			cmn_reg->dt_shift_val) |
		(path_cfg->cid << cmn_reg->dt_id_shift_val) |
		(path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt <<
		 	cmn_reg->decode_format_shift_val);

	if (csid_reg->cmn_reg->vfr_supported)
		cfg0 |= path_cfg->vfr_en << cmn_reg->vfr_en_shift_val;

	if (csid_reg->cmn_reg->frame_id_dec_supported)
		cfg0 |= path_cfg->frame_id_dec_en <<
			cmn_reg->frame_id_decode_en_shift_val;

	if (cmn_reg->timestamp_enabled_in_cfg0)
		cfg0 |= (1 << path_reg->timestamp_en_shift_val) |
			(cmn_reg->timestamp_strobe_val <<
				cmn_reg->timestamp_stb_sel_shift_val);

	cam_io_w_mb(cfg0, mem_base + path_reg->cfg0_addr);

	/*Configure Multi VC DT combo */
	if (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].valid) {
		val = (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].vc <<
				cmn_reg->multi_vcdt_vc1_shift_val) |
			(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].dt <<
				 cmn_reg->multi_vcdt_dt1_shift_val) |
			(1 << cmn_reg->multi_vcdt_en_shift_val);

		if (csid_reg->cmn_reg->decode_format1_supported)
			val |= (path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].decode_fmt <<
				csid_reg->cmn_reg->decode_format1_shift_val);

		cam_io_w_mb(val, mem_base + path_reg->multi_vcdt_cfg0_addr);
	}

	/*configure cfg1 addr
	 * Crop/Drop parameters
	 * Timestamp enable and strobe selection
	 * Plain format
	 * Packing format
	 */
	cfg1 = (path_cfg->crop_enable << path_reg->crop_h_en_shift_val) |
		(path_cfg->crop_enable <<
		 path_reg->crop_v_en_shift_val);

	if (cmn_reg->drop_supported)
		cfg1 |= (path_cfg->drop_enable <<
				path_reg->drop_v_en_shift_val) |
			(path_cfg->drop_enable <<
				path_reg->drop_h_en_shift_val);

	if (path_reg->mipi_pack_supported)
		cfg1 |= path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].packing_fmt <<
			path_reg->packing_fmt_shift_val;

	cfg1 |= (path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].plain_fmt <<
			path_reg->plain_fmt_shift_val);

	if (csid_hw->debug_info.debug_val &
		CAM_IFE_CSID_DEBUG_ENABLE_HBI_VBI_INFO)
		cfg1 |= 1 << path_reg->format_measure_en_shift_val;

	if (!cmn_reg->timestamp_enabled_in_cfg0)
		cfg1 |= (1 << path_reg->timestamp_en_shift_val) |
			(cmn_reg->timestamp_strobe_val <<
				cmn_reg->timestamp_stb_sel_shift_val);

	cam_io_w_mb(cfg1, mem_base + path_reg->cfg1_addr);

	/* set frame drop pattern to 0 and period to 1 */
	cam_io_w_mb(1, mem_base + path_reg->frm_drop_period_addr);
	cam_io_w_mb(0, mem_base + path_reg->frm_drop_pattern_addr);

	/*TODO Need to check for any hw errata like 480 and 580*/
	/* set pxl drop pattern to 0 and period to 1 */
	cam_io_w_mb(0, mem_base + path_reg->pix_drop_pattern_addr);
	cam_io_w_mb(1, mem_base + path_reg->pix_drop_period_addr);

	/* set line drop pattern to 0 and period to 1 */
	cam_io_w_mb(0, mem_base + path_reg->line_drop_pattern_addr);
	cam_io_w_mb(1, mem_base + path_reg->line_drop_period_addr);

	/* Enable the RDI path */
	val = cam_io_r_mb(mem_base + path_reg->cfg0_addr);
	val |= (1 << cmn_reg->path_en_shift_val);
	cam_io_w_mb(val, mem_base + path_reg->cfg0_addr);

	if (path_reg->overflow_ctrl_en) {
		val = path_reg->overflow_ctrl_en |
			path_reg->overflow_ctrl_mode_val;
		cam_io_w_mb(val, mem_base +
			path_reg->err_recovery_cfg0_addr);
	}

	if (path_cfg->sfe_inline_shdr)
		cam_ife_csid_ver2_shdr_cfg(csid_hw, res->res_id);

	if (csid_hw->debug_info.debug_val &
		CAM_IFE_CSID_DEBUG_ENABLE_HBI_VBI_INFO) {
		val = cam_io_r_mb(mem_base +
			path_reg->format_measure_cfg0_addr);
		val |= csid_reg->cmn_reg->measure_en_hbi_vbi_cnt_mask;
		cam_io_w_mb(val, mem_base +
			path_reg->format_measure_cfg0_addr);
	}

	return rc;
}

static int cam_ife_csid_ver2_init_config_pxl_path(
	struct cam_ife_csid_ver2_hw *csid_hw,
	struct cam_isp_resource_node    *res)
{
	int rc = 0;
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg = NULL;
	const struct cam_ife_csid_ver2_common_reg_info *cmn_reg = NULL;
	uint32_t val = 0, cfg0 = 0, cfg1 = 0;
	struct cam_ife_csid_ver2_path_cfg *path_cfg;
	struct cam_ife_csid_cid_data *cid_data;
	void __iomem *mem_base;

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	path_reg = csid_reg->path_reg[res->res_id];

	if (!path_reg) {
		CAM_ERR(CAM_ISP,
			"CSID:%d path res type:%d res_id:%d res state %d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return -EINVAL;
	}

	cmn_reg = csid_reg->cmn_reg;
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	cid_data = &csid_hw->cid_data[path_cfg->cid];
	mem_base = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;

	/*Configure:
	 * VC
	 * DT
	 * Timestamp enable and strobe selection
	 * DT_ID cobination
	 * Decode Format
	 * Frame_id_dec_en
	 * VFR en
	 */
	cfg0 |= (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc <<
			cmn_reg->vc_shift_val) |
		(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt <<
			cmn_reg->dt_shift_val) |
		(path_cfg->cid << cmn_reg->dt_id_shift_val) |
		(path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt <<
		 	cmn_reg->decode_format_shift_val);

	if (csid_reg->cmn_reg->vfr_supported)
		cfg0 |= path_cfg->vfr_en << cmn_reg->vfr_en_shift_val;

	if (csid_reg->cmn_reg->frame_id_dec_supported)
		cfg0 |= path_cfg->frame_id_dec_en <<
			cmn_reg->frame_id_decode_en_shift_val;

	if (cmn_reg->timestamp_enabled_in_cfg0)
		cfg0 |= (1 << path_reg->timestamp_en_shift_val) |
			(cmn_reg->timestamp_strobe_val <<
				cmn_reg->timestamp_stb_sel_shift_val);

	CAM_DBG(CAM_ISP, "CSID[%d] res:%d cfg0_addr 0x%x",
		csid_hw->hw_intf->hw_idx, res->res_id, cfg0);

	cam_io_w_mb(cfg0, mem_base + path_reg->cfg0_addr);

	/*Configure Multi VC DT combo */
	if (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].valid) {
		val = (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].vc <<
				cmn_reg->multi_vcdt_vc1_shift_val) |
			(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].dt <<
				 cmn_reg->multi_vcdt_dt1_shift_val) |
			(1 << cmn_reg->multi_vcdt_en_shift_val);

		if(csid_reg->cmn_reg->decode_format1_supported)
			val |= (path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].decode_fmt <<
				csid_reg->cmn_reg->decode_format1_shift_val);

		cam_io_w_mb(val, mem_base + path_reg->multi_vcdt_cfg0_addr);
	}

	val = 0;
	/*configure cfg1 addr
	 * Binning
	 * Crop/Drop parameters
	 * Early Eof
	 * Timestamp enable and strobe selection
	 * Pix store enable
	 */

	if (csid_hw->flags.binning_enabled) {

		if (path_reg->binning_supported & CAM_IFE_CSID_BIN_HORIZONTAL)
			cfg1 |= path_cfg->horizontal_bin <<
				path_reg->bin_h_en_shift_val;

		if (path_reg->binning_supported & CAM_IFE_CSID_BIN_VERTICAL)
			cfg1 |= path_cfg->vertical_bin <<
				path_reg->bin_v_en_shift_val;

		if (path_reg->binning_supported & CAM_IFE_CSID_BIN_QCFA)
			cfg1 |= path_cfg->qcfa_bin <<
				path_reg->bin_qcfa_en_shift_val;

		if (path_cfg->qcfa_bin || path_cfg->vertical_bin ||
				path_cfg->horizontal_bin)
			cfg1 |= 1  << path_reg->bin_en_shift_val;
	}

	cfg1 |= (path_cfg->crop_enable << path_reg->crop_h_en_shift_val) |
		(path_cfg->crop_enable <<
		 path_reg->crop_v_en_shift_val);

	if (cmn_reg->drop_supported)
		cfg1 |= (path_cfg->drop_enable <<
				path_reg->drop_v_en_shift_val) |
			(path_cfg->drop_enable <<
				path_reg->drop_h_en_shift_val);

	cfg1 |= 1 << path_reg->pix_store_en_shift_val;

	/*enable early eof based on crop enable */
	if (!(csid_hw->debug_info.debug_val &
		    CAM_IFE_CSID_DEBUG_DISABLE_EARLY_EOF) &&
		cmn_reg->early_eof_supported &&
		path_cfg->crop_enable)
		cfg1 |= (1 << path_reg->early_eof_en_shift_val);

	if (csid_hw->debug_info.debug_val &
		CAM_IFE_CSID_DEBUG_ENABLE_HBI_VBI_INFO)
		cfg1 |= 1 << path_reg->format_measure_en_shift_val;

	if (!cmn_reg->timestamp_enabled_in_cfg0)
		cfg1 |= (1 << path_reg->timestamp_en_shift_val) |
			(cmn_reg->timestamp_strobe_val <<
				cmn_reg->timestamp_stb_sel_shift_val);

	CAM_DBG(CAM_ISP, "CSID[%d] res:%d cfg1_addr 0x%x",
		csid_hw->hw_intf->hw_idx, res->res_id, cfg1);

	cam_io_w_mb(cfg1, mem_base + path_reg->cfg1_addr);

	/* set frame drop pattern to 0 and period to 1 */
	cam_io_w_mb(1, mem_base + path_reg->frm_drop_period_addr);
	cam_io_w_mb(0, mem_base + path_reg->frm_drop_pattern_addr);
	/* set pxl drop pattern to 0 and period to 1 */
	cam_io_w_mb(0, mem_base + path_reg->pix_drop_pattern_addr);
	cam_io_w_mb(1, mem_base + path_reg->pix_drop_period_addr);
	/* set line drop pattern to 0 and period to 1 */
	cam_io_w_mb(0, mem_base + path_reg->line_drop_pattern_addr);
	cam_io_w_mb(1, mem_base + path_reg->line_drop_period_addr);

	/* Enable the Pxl path */
	val = cam_io_r_mb(mem_base + path_reg->cfg0_addr);
	val |= (1 << cmn_reg->path_en_shift_val);
	cam_io_w_mb(val, mem_base + path_reg->cfg0_addr);

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

	res->res_state = CAM_ISP_RESOURCE_STATE_INIT_HW;
	return rc;
}

static inline int cam_ife_csid_ver2_subscribe_sof_for_discard(
	struct cam_ife_csid_ver2_path_cfg *path_cfg,
	struct cam_ife_csid_ver2_hw       *csid_hw,
	struct cam_isp_resource_node      *res,
	CAM_IRQ_HANDLER_TOP_HALF           top_half_handler,
	CAM_IRQ_HANDLER_BOTTOM_HALF        bottom_half_handler)
{
	int rc = 0;
	uint32_t val;
	uint32_t irq_mask[CAM_IFE_CSID_IRQ_REG_MAX] = {0};

	val = IFE_CSID_VER2_PATH_INFO_INPUT_SOF;
	irq_mask[path_cfg->irq_reg_idx] = val;
	path_cfg->discard_irq_handle = cam_irq_controller_subscribe_irq(
		csid_hw->csid_irq_controller,
		CAM_IRQ_PRIORITY_0,
		irq_mask,
		res,
		top_half_handler,
		bottom_half_handler,
		csid_hw->tasklet,
		&tasklet_bh_api,
		CAM_IRQ_EVT_GROUP_0);

	if (path_cfg->discard_irq_handle < 1) {
		CAM_ERR(CAM_ISP,
			"CSID[%d] Subscribing input SOF failed for discarding %d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		rc = -EINVAL;
	}

	CAM_DBG(CAM_ISP,
		"Subscribing input SOF for discard done res: %s rc: %d",
		res->res_name, rc);
	return rc;
}

static int cam_ife_csid_ver2_program_rdi_path(
	struct cam_ife_csid_ver2_hw     *csid_hw,
	struct cam_isp_resource_node    *res,
	uint32_t                        *rup_aup_mask)
{
	int rc = 0;
	struct cam_ife_csid_ver2_reg_info *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg;
	void __iomem *mem_base;
	uint32_t val = 0;
	uint32_t irq_mask[CAM_IFE_CSID_IRQ_REG_MAX] = {0};
	struct cam_ife_csid_ver2_path_cfg *path_cfg;

	rc = cam_ife_csid_ver2_init_config_rdi_path(
		csid_hw, res);

	if (rc) {
		CAM_ERR(CAM_ISP,
			"CSID:%d %s path res type:%d res_id:%d %d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return rc;
	}

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
		    csid_hw->core_info->csid_reg;

	path_reg = csid_reg->path_reg[res->res_id];

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d RDI:%d is not supported on HW",
			 csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	mem_base = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	if (!csid_hw->flags.offline_mode) {
		CAM_DBG(CAM_ISP, "CSID:%d Rdi res: %d",
			csid_hw->hw_intf->hw_idx, res->res_id);

		/*Program the camif part */
		cam_io_w_mb(path_cfg->camif_data.epoch0 <<
			path_reg->epoch0_shift_val,
			mem_base + path_reg->epoch_irq_cfg_addr);
	}

	val = csid_hw->debug_info.path_mask;

	if (res->rdi_only_ctx) {
		path_cfg->handle_camif_irq = true;
		val |= path_reg->camif_irq_mask;
	}

	if ((csid_hw->flags.offline_mode ||
		path_cfg->sfe_inline_shdr) &&
		(res->res_id == CAM_IFE_PIX_PATH_RES_RDI_0)) {
		val |= path_reg->camif_irq_mask;
		path_cfg->handle_camif_irq = true;
	}

	/* Enable secondary events dictated by HW mgr for RDI paths */
	if (path_cfg->sec_evt_config.en_secondary_evt) {
		if (path_cfg->sec_evt_config.evt_type & CAM_IFE_CSID_EVT_SOF)
			val |= IFE_CSID_VER2_PATH_CAMIF_SOF;

		if (path_cfg->sec_evt_config.evt_type & CAM_IFE_CSID_EVT_EPOCH)
			val |= IFE_CSID_VER2_PATH_CAMIF_EPOCH0;

		CAM_DBG(CAM_ISP,
			"Enable camif: %d evt irq for res: %s",
			path_cfg->sec_evt_config.evt_type, res->res_name);
	}

	res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	path_cfg->irq_reg_idx =
		cam_ife_csid_convert_res_to_irq_reg(res->res_id);

	if (path_cfg->irq_reg_idx >= CAM_IFE_CSID_IRQ_REG_MAX) {
		CAM_ERR(CAM_ISP, "CSID[%d] Invalid irq reg id %d",
			csid_hw->hw_intf->hw_idx, path_cfg->irq_reg_idx);
		rc = -EINVAL;
		goto end;
	}

	irq_mask[CAM_IFE_CSID_IRQ_REG_TOP] = path_reg->top_irq_mask;

	irq_mask[path_cfg->irq_reg_idx] = val;
	path_cfg->irq_handle = cam_irq_controller_subscribe_irq(
		csid_hw->csid_irq_controller,
		CAM_IRQ_PRIORITY_1,
		irq_mask,
		res,
		cam_ife_csid_ver2_path_top_half,
		cam_ife_csid_ver2_rdi_bottom_half,
		csid_hw->tasklet,
		&tasklet_bh_api,
		CAM_IRQ_EVT_GROUP_0);

	if (path_cfg->irq_handle < 1) {
		CAM_ERR(CAM_ISP, "CSID[%d] Subscribe Irq fail %d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		rc = -EINVAL;
		goto end;
	}

	if (path_cfg->discard_init_frames) {
		rc = cam_ife_csid_ver2_subscribe_sof_for_discard(
			path_cfg, csid_hw, res,
			cam_ife_csid_ver2_discard_sof_top_half,
			cam_ife_csid_ver2_discard_sof_rdi_bottom_half);
		if (rc)
			goto end;
	}

	val = path_reg->fatal_err_mask | path_reg->non_fatal_err_mask;
	irq_mask[path_cfg->irq_reg_idx] = val;
	path_cfg->err_irq_handle = cam_irq_controller_subscribe_irq(
			csid_hw->csid_irq_controller,
			CAM_IRQ_PRIORITY_0,
			irq_mask,
			res,
			cam_ife_csid_ver2_path_err_top_half,
			cam_ife_csid_ver2_rdi_bottom_half,
			csid_hw->tasklet,
			&tasklet_bh_api,
			CAM_IRQ_EVT_GROUP_0);

	if (path_cfg->err_irq_handle < 1) {
		CAM_ERR(CAM_ISP, "CSID[%d] Subscribe Err Irq fail %d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		 cam_irq_controller_unsubscribe_irq(
			csid_hw->csid_irq_controller,
			path_cfg->irq_handle);
		path_cfg->irq_handle = 0;
		rc = -EINVAL;
		goto end;
	}

	*rup_aup_mask |= path_reg->rup_aup_mask;

end:
	return rc;
}


static int cam_ife_csid_ver2_program_ipp_path(
	struct cam_ife_csid_ver2_hw    *csid_hw,
	struct cam_isp_resource_node   *res,
	uint32_t                       *rup_aup_mask)
{
	int rc = 0;
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg = NULL;
	uint32_t  val = 0;
	void __iomem *mem_base;
	struct cam_ife_csid_ver2_path_cfg *path_cfg;
	uint32_t irq_mask[CAM_IFE_CSID_IRQ_REG_MAX] = {0};

	rc = cam_ife_csid_ver2_init_config_pxl_path(
		csid_hw, res);

	if (rc) {
		CAM_ERR(CAM_ISP,
			"CSID:%d %s path res type:%d res_id:%d %d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return rc;
	}

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	path_reg = csid_reg->path_reg[CAM_IFE_PIX_PATH_RES_IPP];

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d IPP is not supported on HW",
			 csid_hw->hw_intf->hw_idx);
		return -EINVAL;
	}

	mem_base = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;

	cam_io_w_mb(path_cfg->camif_data.epoch0 << path_reg->epoch0_shift_val,
		mem_base + path_reg->epoch_irq_cfg_addr);

	CAM_DBG(CAM_ISP, "csid[%d] frame_cfg 0x%x epoch_cfg 0x%x",
			csid_hw->hw_intf->hw_idx,
			val, path_cfg->camif_data.epoch0);

	path_cfg->irq_reg_idx = cam_ife_csid_get_rt_irq_idx(
			CAM_IFE_CSID_IRQ_REG_IPP,
			csid_reg->cmn_reg->num_pix,
			csid_reg->cmn_reg->num_ppp,
			csid_reg->cmn_reg->num_rdis);

	val = csid_hw->debug_info.path_mask;

	if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_NONE ||
		path_cfg->sync_mode == CAM_ISP_HW_SYNC_MASTER) {
		val |= path_reg->camif_irq_mask;
	}

	irq_mask[CAM_IFE_CSID_IRQ_REG_TOP] = path_reg->top_irq_mask;
	irq_mask[path_cfg->irq_reg_idx] = val;
	path_cfg->irq_handle = cam_irq_controller_subscribe_irq(
				    csid_hw->csid_irq_controller,
				    CAM_IRQ_PRIORITY_1,
				    irq_mask,
				    res,
				    cam_ife_csid_ver2_path_top_half,
				    cam_ife_csid_ver2_ipp_bottom_half,
				    csid_hw->tasklet,
				    &tasklet_bh_api,
				    CAM_IRQ_EVT_GROUP_0);

	if (path_cfg->irq_handle < 1) {
		CAM_ERR(CAM_ISP, "CSID[%d] Subscribe IPP Irq fail",
			csid_hw->hw_intf->hw_idx);
		rc = -EINVAL;
		goto end;
	}

	if (path_cfg->discard_init_frames) {
		rc = cam_ife_csid_ver2_subscribe_sof_for_discard(
			path_cfg, csid_hw, res,
			cam_ife_csid_ver2_discard_sof_top_half,
			cam_ife_csid_ver2_discard_sof_pix_bottom_half);
		if (rc)
			goto end;
	}

	val = path_reg->fatal_err_mask | path_reg->non_fatal_err_mask;
	irq_mask[path_cfg->irq_reg_idx] = val;
	path_cfg->err_irq_handle = cam_irq_controller_subscribe_irq(
		csid_hw->csid_irq_controller,
		CAM_IRQ_PRIORITY_0,
		irq_mask,
		res,
		cam_ife_csid_ver2_path_err_top_half,
		cam_ife_csid_ver2_ipp_bottom_half,
		csid_hw->tasklet,
		&tasklet_bh_api,
		CAM_IRQ_EVT_GROUP_0);

	if (path_cfg->err_irq_handle < 1) {
		CAM_ERR(CAM_ISP, "CSID[%d] Subscribe Err Irq fail %d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		 cam_irq_controller_unsubscribe_irq(
			csid_hw->csid_irq_controller,
			path_cfg->irq_handle);
		path_cfg->irq_handle = 0;
		goto end;
	}

	val = path_reg->start_master_sel_val <<
		path_reg->start_master_sel_shift;

	if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_MASTER) {
		/* Set start mode as master */
		val |= path_reg->start_mode_master  <<
			path_reg->start_mode_shift;
	} else if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_SLAVE) {
		/* Set start mode as slave */
		val |= path_reg->start_mode_slave <<
			path_reg->start_mode_shift;
	} else {
		/* Default is internal halt mode */
		val = 0;
	}

	cam_io_w_mb(val, mem_base + path_reg->ctrl_addr);

	CAM_DBG(CAM_ISP, "CSID:%d Pix res: %d ctrl val: 0x%x",
		csid_hw->hw_intf->hw_idx,
		res->res_id, val);

	if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_MASTER ||
		 path_cfg->sync_mode == CAM_ISP_HW_SYNC_NONE)
		*rup_aup_mask |= path_reg->rup_aup_mask;

	res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
end:
	return rc;
}

static int cam_ife_csid_ver2_enable_path(
	struct cam_ife_csid_ver2_hw *csid_hw,
	struct cam_isp_resource_node    *res)
{
	const struct cam_ife_csid_ver2_reg_info      *csid_reg;
	struct cam_hw_soc_info                       *soc_info;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg = NULL;
	uint32_t val = 0;
	uint32_t ctrl_addr = 0;
	struct cam_ife_csid_ver2_path_cfg *path_cfg;
	void __iomem *mem_base;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	soc_info = &csid_hw->hw_info->soc_info;
	mem_base = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;

	if (path_cfg->discard_init_frames) {
		CAM_DBG(CAM_ISP, "CSID[%u] skip start cmd for res: %s",
			csid_hw->hw_intf->hw_idx, res->res_id);
		goto end;
	}

	path_reg = csid_reg->path_reg[res->res_id];
	val = path_reg->resume_frame_boundary;
	ctrl_addr = path_reg->ctrl_addr;

	switch (res->res_id) {
	case CAM_IFE_PIX_PATH_RES_IPP:
	case CAM_IFE_PIX_PATH_RES_PPP:
		if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_SLAVE)
			return 0;
		break;
	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
	case CAM_IFE_PIX_PATH_RES_RDI_4:
		if (csid_hw->flags.offline_mode)
			return 0;
		break;
	default:
		return -EINVAL;
	}

	val |= cam_io_r_mb(mem_base + ctrl_addr);
	cam_io_w_mb(val, mem_base + ctrl_addr);

	CAM_DBG(CAM_ISP, "CSID[%u] start cmd programmed for res: %s",
		csid_hw->hw_intf->hw_idx, res->res_name);
end:
	/* Change state even if we don't configure start cmd */
	res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
	return 0;
}

static int cam_ife_csid_ver2_program_ppp_path(
	struct cam_ife_csid_ver2_hw     *csid_hw,
	struct cam_isp_resource_node    *res,
	uint32_t                        *rup_aup_mask)
{
	int rc = 0;
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	struct cam_hw_soc_info                   *soc_info;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg = NULL;
	uint32_t  val = 0;
	struct cam_ife_csid_ver2_path_cfg *path_cfg;
	uint32_t irq_mask[CAM_IFE_CSID_IRQ_REG_MAX] = {0};
	void __iomem *mem_base;

	rc = cam_ife_csid_ver2_init_config_pxl_path(
		csid_hw, res);

	if (rc) {
		CAM_ERR(CAM_ISP,
			"CSID:%d %s path res type:%d res_id:%d %d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id, res->res_state);
		return rc;
	}

	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	path_reg = csid_reg->path_reg[res->res_id];

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d PPP is not supported on HW",
			 csid_hw->hw_intf->hw_idx);
		return -EINVAL;
	}

	mem_base = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;

	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;

	cam_io_w_mb(path_cfg->camif_data.epoch0 << path_reg->epoch0_shift_val,
		mem_base + path_reg->epoch_irq_cfg_addr);

	path_cfg->irq_reg_idx = cam_ife_csid_get_rt_irq_idx(
				CAM_IFE_CSID_IRQ_REG_PPP,
				csid_reg->cmn_reg->num_pix,
				csid_reg->cmn_reg->num_ppp,
				csid_reg->cmn_reg->num_rdis);

	/* for dual case
	 * set ppp as slave
	 * if current csid is set as master set
	 * start_master_sel_val as 3
	 */

	if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_NONE) {
		val = 0;
	} else {
		val = path_reg->start_mode_slave <<
			path_reg->start_mode_shift;
		/* Set halt mode as internal master */
		if (csid_hw->sync_mode == CAM_ISP_HW_SYNC_MASTER)
			val |= path_reg->start_master_sel_val <<
				path_reg->start_master_sel_shift;
	}

	cam_io_w_mb(val, mem_base + path_reg->ctrl_addr);

	CAM_DBG(CAM_ISP, "CSID:%d Pix res: %d ctrl val: 0x%x",
		csid_hw->hw_intf->hw_idx, res->res_id, val);

	if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_MASTER ||
		 path_cfg->sync_mode == CAM_ISP_HW_SYNC_NONE)
		 *rup_aup_mask |= path_reg->rup_aup_mask;

	val = csid_hw->debug_info.path_mask;

	irq_mask[path_cfg->irq_reg_idx] = val;
	irq_mask[CAM_IFE_CSID_IRQ_REG_TOP] = path_reg->top_irq_mask;
	path_cfg->irq_handle = cam_irq_controller_subscribe_irq(
				csid_hw->csid_irq_controller,
				CAM_IRQ_PRIORITY_1,
				irq_mask,
				csid_hw,
				cam_ife_csid_ver2_path_top_half,
				cam_ife_csid_ver2_ppp_bottom_half,
				csid_hw->tasklet,
				&tasklet_bh_api,
				CAM_IRQ_EVT_GROUP_0);


	if (path_cfg->irq_handle < 1) {
		CAM_ERR(CAM_ISP, "CSID[%d] Subscribe PPP Irq fail",
			csid_hw->hw_intf->hw_idx);
		rc = -EINVAL;
		goto end;
	}

	if (path_cfg->discard_init_frames) {
		rc = cam_ife_csid_ver2_subscribe_sof_for_discard(
			path_cfg, csid_hw, res,
			cam_ife_csid_ver2_discard_sof_top_half,
			cam_ife_csid_ver2_discard_sof_pix_bottom_half);
		if (rc)
			goto end;
	}

	val = path_reg->fatal_err_mask | path_reg->non_fatal_err_mask;
	irq_mask[path_cfg->irq_reg_idx] = val;
	path_cfg->err_irq_handle = cam_irq_controller_subscribe_irq(
					csid_hw->csid_irq_controller,
					CAM_IRQ_PRIORITY_0,
					irq_mask,
					res,
					cam_ife_csid_ver2_path_err_top_half,
					cam_ife_csid_ver2_ipp_bottom_half,
					csid_hw->tasklet,
					&tasklet_bh_api,
					CAM_IRQ_EVT_GROUP_0);

	if (path_cfg->err_irq_handle < 1) {
		CAM_ERR(CAM_ISP, "CSID[%d] Subscribe Err Irq fail %d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		 cam_irq_controller_unsubscribe_irq(
			csid_hw->csid_irq_controller,
			path_cfg->err_irq_handle);
		path_cfg->irq_handle = 0;
		rc = -EINVAL;
	}
end:
	return rc;
}

static int cam_ife_csid_ver2_rx_capture_config(
	struct cam_ife_csid_ver2_hw *csid_hw)
{
	const struct cam_ife_csid_ver2_reg_info   *csid_reg;
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

	csid_reg = (struct cam_ife_csid_ver2_reg_info *) csid_hw->core_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (csid_hw->debug_info.debug_val &
			CAM_IFE_CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE)
		val = ((1 << csid_reg->csi2_reg->capture_short_pkt_en_shift) |
			(vc << csid_reg->csi2_reg->capture_short_pkt_vc_shift));

	/* CAM_IFE_CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE */
	val |= ((1 << csid_reg->csi2_reg->capture_long_pkt_en_shift) |
		(dt << csid_reg->csi2_reg->capture_long_pkt_dt_shift) |
		(vc << csid_reg->csi2_reg->capture_long_pkt_vc_shift));

	/* CAM_IFE_CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE */
	if (rx_cfg->lane_type == CAM_ISP_LANE_TYPE_CPHY) {
		val |= ((1 << csid_reg->csi2_reg->capture_cphy_pkt_en_shift) |
			(dt << csid_reg->csi2_reg->capture_cphy_pkt_dt_shift) |
			(vc << csid_reg->csi2_reg->capture_cphy_pkt_vc_shift));
	}

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base + csid_reg->csi2_reg->capture_ctrl_addr);

	CAM_DBG(CAM_ISP, "CSID[%d] rx capture_ctrl: 0x%x", csid_hw->hw_intf->hw_idx, val);

	return 0;
}

static int cam_ife_csid_ver2_enable_csi2(struct cam_ife_csid_ver2_hw *csid_hw)
{
	int rc = 0;
	struct cam_hw_soc_info              *soc_info;
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	const struct cam_ife_csid_csi2_rx_reg_info  *csi2_reg;
	uint32_t val = 0;
	void __iomem *mem_base;
	struct cam_ife_csid_rx_cfg        *rx_cfg;
	int vc_full_width;
	uint32_t irq_mask[CAM_IFE_CSID_IRQ_REG_MAX] = {0};

	if (csid_hw->flags.rx_enabled)
		return 0;

	if (csid_hw->flags.offline_mode)
		return 0;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	csi2_reg  = csid_reg->csi2_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	rx_cfg  = &csid_hw->rx_cfg;
	mem_base = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;

	/*Configure Rx cfg0 */
	val |= ((rx_cfg->lane_cfg << csi2_reg->lane_cfg_shift) |
		((rx_cfg->lane_num - 1) << csi2_reg->lane_num_shift) |
		(rx_cfg->lane_type << csi2_reg->phy_type_shift));

	if (rx_cfg->tpg_mux_sel) {
		val |= ((rx_cfg->tpg_num_sel << csi2_reg->tpg_num_sel_shift) |
			(rx_cfg->tpg_mux_sel << csi2_reg->tpg_mux_en_shift));
	} else {
		val |= rx_cfg->phy_sel << csi2_reg->phy_num_shift;
	}

	cam_io_w_mb(val, mem_base + csi2_reg->cfg0_addr);

	CAM_DBG(CAM_ISP, "CSID[%d] rx_cfg0: 0x%x",
		csid_hw->hw_intf->hw_idx, val);

	val = 0;
	/*Configure Rx cfg1*/
	val = 1 << csi2_reg->misr_enable_shift_val;
	val |= 1 << csi2_reg->ecc_correction_shift_en;
	val |= (rx_cfg->epd_supported << csi2_reg->epd_mode_shift_en);
	if (rx_cfg->dynamic_sensor_switch_en)
		val |= 1 << csi2_reg->dyn_sensor_switch_shift_en;

	vc_full_width = cam_ife_csid_is_vc_full_width(csid_hw->cid_data);

	if (vc_full_width == 1) {
		val |= 1 <<  csi2_reg->vc_mode_shift_val;
	} else if (vc_full_width < 0) {
		CAM_ERR(CAM_ISP, "Error VC DT");
		return -EINVAL;
	}

	cam_io_w_mb(val, mem_base + csi2_reg->cfg1_addr);
	CAM_DBG(CAM_ISP, "CSID[%d] rx_cfg1: 0x%x",
		csid_hw->hw_intf->hw_idx, val);
	val = 0;

	irq_mask[CAM_IFE_CSID_IRQ_REG_TOP] = csi2_reg->top_irq_mask;

	if (csid_hw->debug_info.rx_mask) {
		irq_mask[CAM_IFE_CSID_IRQ_REG_RX] =  csid_hw->debug_info.rx_mask;
		csid_hw->rx_cfg.irq_handle =
				    cam_irq_controller_subscribe_irq(
					    csid_hw->csid_irq_controller,
					    CAM_IRQ_PRIORITY_4,
					    irq_mask,
					    csid_hw,
					    cam_ife_csid_ver2_rx_top_half,
					    NULL,
					    NULL,
					    NULL,
					    CAM_IRQ_EVT_GROUP_0);

		if (csid_hw->rx_cfg.irq_handle < 1) {
			CAM_ERR(CAM_ISP, "CSID[%d] RX debug irq register fail",
				csid_hw->hw_intf->hw_idx);
			return -EINVAL;
		}
	}

	val = csi2_reg->fatal_err_mask | csi2_reg->part_fatal_err_mask |
		csi2_reg->non_fatal_err_mask;

	if (csid_hw->rx_cfg.epd_supported &&
		(csid_hw->rx_cfg.lane_type == CAM_ISP_LANE_TYPE_DPHY))
		val &= ~IFE_CSID_VER2_RX_CPHY_EOT_RECEPTION;

	irq_mask[CAM_IFE_CSID_IRQ_REG_RX] = val;

	csid_hw->rx_cfg.err_irq_handle =
			    cam_irq_controller_subscribe_irq(
				    csid_hw->csid_irq_controller,
				    CAM_IRQ_PRIORITY_0,
				    irq_mask,
				    csid_hw,
				    cam_ife_csid_ver2_rx_err_top_half,
				    cam_ife_csid_ver2_rx_err_bottom_half,
				    csid_hw->tasklet,
				    &tasklet_bh_api,
				    CAM_IRQ_EVT_GROUP_0);

	if (csid_hw->rx_cfg.err_irq_handle < 1) {
		CAM_ERR(CAM_ISP, "CSID[%d] RX err irq register fail",
			csid_hw->hw_intf->hw_idx);
		rc = -EINVAL;
		goto err;
	}

	csid_hw->flags.rx_enabled = true;

	cam_ife_csid_ver2_rx_capture_config(csid_hw);

	return rc;
err:
	if (csid_hw->rx_cfg.irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			csid_hw->csid_irq_controller,
			csid_hw->rx_cfg.irq_handle);
		csid_hw->rx_cfg.irq_handle = 0;
	}
	return rc;
}


static int cam_ife_csid_ver2_program_top(
	struct cam_ife_csid_ver2_hw *csid_hw)
{
	const struct cam_ife_csid_ver2_top_reg_info *top_reg;
	struct cam_ife_csid_ver2_reg_info *csid_reg;
	uint32_t val;
	struct cam_hw_soc_info              *soc_info;
	int input_core_sel;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;

	if (!csid_reg->need_top_cfg) {
		CAM_DBG(CAM_ISP, "CSID %d top not supported",
			csid_hw->hw_intf->hw_idx);
		return 0;
	}

	top_reg  = csid_reg->top_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	/* Porgram top parameters */
	input_core_sel = csid_reg->input_core_sel[csid_hw->hw_intf->hw_idx]
				[csid_hw->top_cfg.input_core_type];

	CAM_DBG(CAM_ISP, "CSID[%d] input_core_sel %d",
		csid_hw->hw_intf->hw_idx, input_core_sel);

	if (input_core_sel == -1) {
		CAM_ERR(CAM_ISP, "csid[%d] invalid top input_core_type %u",
			csid_hw->hw_intf->hw_idx,
			csid_hw->top_cfg.input_core_type);
		return -EINVAL;
	}

	val = (uint32_t)input_core_sel << top_reg->input_core_type_shift_val;
	val |= csid_hw->top_cfg.offline_sfe_en <<
			top_reg->sfe_offline_en_shift_val;
	val |= csid_hw->top_cfg.out_ife_en <<
			top_reg->out_ife_en_shift_val;

	cam_io_w_mb(val,
		soc_info->reg_map[CAM_IFE_CSID_TOP_MEM_BASE_ID].mem_base +
		top_reg->io_path_cfg0_addr[csid_hw->hw_intf->hw_idx]);

	/*Program dual csid regs */

	if (csid_hw->sync_mode == CAM_ISP_HW_SYNC_NONE)
		return 0;

	val = csid_hw->top_cfg.dual_sync_core_sel <<
		top_reg->dual_sync_sel_shift_val;
	val |= csid_hw->top_cfg.dual_en <<
		top_reg->dual_en_shift_val;
	val |= csid_hw->top_cfg.master_slave_sel <<
		top_reg->master_slave_sel_shift_val;

	cam_io_w_mb(val,
		soc_info->reg_map[CAM_IFE_CSID_TOP_MEM_BASE_ID].mem_base +
		top_reg->dual_csid_cfg0_addr[csid_hw->hw_intf->hw_idx]);

	return 0;
}

static int cam_ife_csid_ver2_enable_core(struct cam_ife_csid_ver2_hw *csid_hw)
{
	int rc = 0;
	struct cam_hw_soc_info              *soc_info;
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	uint32_t clk_lvl;
	uint32_t irq_mask[CAM_IFE_CSID_IRQ_REG_MAX] = {0};

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
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
		CAM_DBG(CAM_ISP, "CSID[%d] hw has already been enabled",
			csid_hw->hw_intf->hw_idx);
		return rc;
	}

	rc = cam_soc_util_get_clk_level(soc_info, csid_hw->clk_rate,
		soc_info->src_clk_idx, &clk_lvl);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"CSID[%d] get clk level fail rate %u",
			csid_hw->clk_rate);
	}

	CAM_DBG(CAM_ISP,
		"CSID[%d] clk lvl %u received clk_rate %u applied clk_rate %lu",
		csid_hw->hw_intf->hw_idx, clk_lvl, csid_hw->clk_rate,
		soc_info->applied_src_clk_rate);

	rc = cam_ife_csid_enable_soc_resources(soc_info, clk_lvl);

	if (rc) {
		CAM_ERR(CAM_ISP,
			"CSID[%d] Enable soc failed",
			csid_hw->hw_intf->hw_idx);
		goto err;
	}

	irq_mask[CAM_IFE_CSID_IRQ_REG_TOP] = csid_reg->cmn_reg->top_reset_irq_mask;

	csid_hw->reset_irq_handle = cam_irq_controller_subscribe_irq(
		csid_hw->csid_irq_controller,
		CAM_IRQ_PRIORITY_0,
		irq_mask,
		csid_hw,
		cam_ife_csid_ver2_reset_irq_top_half,
		NULL,
		NULL,
		NULL,
		CAM_IRQ_EVT_GROUP_0);

	if (csid_hw->reset_irq_handle < 1) {
		CAM_ERR(CAM_ISP, "csid[%d] reset irq subscribe fail",
			csid_hw->hw_intf->hw_idx);
		goto disable_res;
	}

	reinit_completion(&csid_hw->hw_info->hw_complete);
	cam_ife_csid_ver2_program_top(csid_hw);
	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_UP;

	return rc;

disable_res:
	cam_ife_csid_disable_soc_resources(soc_info);

err:
	CAM_ERR(CAM_ISP, "CSID[%d] init hw fail rc %d",
		csid_hw->hw_intf->hw_idx, rc);
	csid_hw->hw_info->open_count--;
	return rc;
}

static int cam_ife_csid_ver2_enable_hw(
	struct cam_ife_csid_ver2_hw *csid_hw)
{
	struct cam_hw_soc_info              *soc_info;
	const struct cam_ife_csid_ver2_reg_info *csid_reg = NULL;
	uint32_t  val;
	int i, rc;
	void __iomem *mem_base;
	uint32_t buf_done_irq_mask[CAM_IFE_CSID_IRQ_REG_MAX] = {0};
	uint32_t top_err_irq_mask[CAM_IFE_CSID_IRQ_REG_MAX] = {0};
	const struct cam_ife_csid_ver2_path_reg_info *path_reg = NULL;

	if (csid_hw->flags.device_enabled) {
		CAM_DBG(CAM_ISP, "CSID[%d] hw has already been enabled",
			csid_hw->hw_intf->hw_idx);
		return 0;
	}

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;
	mem_base = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;

	/* Clear IRQs */
	cam_io_w_mb(1, mem_base + csid_reg->cmn_reg->top_irq_clear_addr);

	cam_io_w_mb(csid_reg->csi2_reg->irq_mask_all,
		mem_base + csid_reg->csi2_reg->irq_clear_addr);

	path_reg = csid_reg->path_reg[CAM_IFE_PIX_PATH_RES_IPP];
	if (csid_reg->cmn_reg->num_pix)
		cam_io_w_mb(csid_reg->cmn_reg->ipp_irq_mask_all,
			mem_base + path_reg->irq_clear_addr);

	path_reg = csid_reg->path_reg[CAM_IFE_PIX_PATH_RES_PPP];
	if (csid_reg->cmn_reg->num_ppp)
		cam_io_w_mb(csid_reg->cmn_reg->ppp_irq_mask_all,
			mem_base + path_reg->irq_clear_addr);

	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++) {
		path_reg = csid_reg->path_reg[CAM_IFE_PIX_PATH_RES_RDI_0 + i];
		cam_io_w_mb(csid_reg->cmn_reg->rdi_irq_mask_all,
			mem_base + path_reg->irq_clear_addr);
	}
	cam_io_w_mb(1, mem_base + csid_reg->cmn_reg->irq_cmd_addr);

	/* Read hw version */
	val = cam_io_r_mb(mem_base + csid_reg->cmn_reg->hw_version_addr);

	buf_done_irq_mask[CAM_IFE_CSID_IRQ_REG_TOP] =
		csid_reg->cmn_reg->top_buf_done_irq_mask;
	csid_hw->buf_done_irq_handle = cam_irq_controller_subscribe_irq(
		csid_hw->csid_irq_controller,
		CAM_IRQ_PRIORITY_4,
		buf_done_irq_mask,
		csid_hw,
		cam_ife_csid_ver2_handle_buf_done_irq,
		NULL,
		NULL,
		NULL,
		CAM_IRQ_EVT_GROUP_0);


	if (csid_hw->buf_done_irq_handle < 1) {
		CAM_ERR(CAM_ISP, "csid[%d] buf done irq subscribe fail",
			csid_hw->hw_intf->hw_idx);
		return -EINVAL;
	}

	top_err_irq_mask[CAM_IFE_CSID_IRQ_REG_TOP] =
			csid_reg->cmn_reg->top_err_irq_mask;
	csid_hw->top_err_irq_handle = cam_irq_controller_subscribe_irq(
		csid_hw->csid_irq_controller,
		CAM_IRQ_PRIORITY_0,
		top_err_irq_mask,
		csid_hw,
		cam_ife_csid_ver2_top_err_irq_top_half,
		cam_ife_csid_ver2_top_err_irq_bottom_half,
		csid_hw->tasklet,
		&tasklet_bh_api,
		CAM_IRQ_EVT_GROUP_0);

	if (csid_hw->top_err_irq_handle < 1) {
		CAM_ERR(CAM_ISP, "csid[%d] top error irq subscribe fail",
			csid_hw->hw_intf->hw_idx);
		rc = -EINVAL;
		goto unsubscribe_buf_done;
	}

	rc = cam_irq_controller_register_dependent(csid_hw->csid_irq_controller,
		csid_hw->buf_done_irq_controller, buf_done_irq_mask);

	if (rc) {
		cam_irq_controller_unsubscribe_irq(csid_hw->csid_irq_controller,
			csid_hw->buf_done_irq_handle);
		rc = -EINVAL;
		goto unsubscribe_top_err;
	}

	csid_hw->flags.device_enabled = true;
	csid_hw->flags.fatal_err_detected = false;
	CAM_DBG(CAM_ISP, "CSID:%d CSID HW version: 0x%x",
		csid_hw->hw_intf->hw_idx, val);
	return 0;


unsubscribe_top_err:
	cam_irq_controller_unsubscribe_irq(csid_hw->csid_irq_controller,
		csid_hw->top_err_irq_handle);
	csid_hw->top_err_irq_handle = 0;
unsubscribe_buf_done:
	cam_irq_controller_unsubscribe_irq(csid_hw->csid_irq_controller,
		csid_hw->buf_done_irq_handle);
	csid_hw->buf_done_irq_handle = 0;
	return rc;
}

int cam_ife_csid_ver2_init_hw(void *hw_priv,
	void *init_args, uint32_t arg_size)
{
	struct cam_ife_csid_ver2_hw *csid_hw  = NULL;
	struct cam_hw_info *hw_info;
	int rc = 0;
	struct cam_isp_resource_node *res;

	if (!hw_priv || !init_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;

	mutex_lock(&csid_hw->hw_info->hw_mutex);

	rc = cam_ife_csid_ver2_enable_core(csid_hw);

	if (rc) {
		CAM_ERR(CAM_ISP, "CSID[%d] init hw fail",
			csid_hw->hw_intf->hw_idx);
		goto end;
	}
	res  = (struct cam_isp_resource_node *)init_args;

	res->res_state = CAM_ISP_RESOURCE_STATE_INIT_HW;

	CAM_DBG(CAM_ISP, "CSID[%d] init hw",
		csid_hw->hw_intf->hw_idx);
end:
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_ife_csid_ver2_disable_core(
	struct cam_ife_csid_ver2_hw *csid_hw)
{
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	const struct cam_ife_csid_ver2_top_reg_info *top_reg = NULL;
	struct cam_hw_soc_info                   *soc_info;
	struct cam_csid_soc_private              *soc_private;
	int rc = 0;

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
	soc_private = (struct cam_csid_soc_private *)
		soc_info->soc_private;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	top_reg = csid_reg->top_reg;
	cam_ife_csid_ver2_disable_csi2(csid_hw);

	/* Disable the top IRQ interrupt */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		csid_reg->cmn_reg->top_irq_mask_addr);

	if (csid_hw->reset_irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			csid_hw->csid_irq_controller,
			csid_hw->reset_irq_handle);
		csid_hw->reset_irq_handle = 0;
	}

	/*
	 * Wrapper config can be reset only by ares from
	 * camera subsystem power up or CSR ares bit
	 * in CSID clk branch both of which are not
	 * gauranteed at the end of a given CSID stream.
	 * Explicitly resetting the config for full CSIDs
	 * via AHB to avoid conflict on consecutive sessions
	 */
	if ((top_reg) && (!soc_private->is_ife_csid_lite)) {
		cam_io_w_mb(top_reg->io_path_cfg_rst_val,
			soc_info->reg_map[CAM_IFE_CSID_TOP_MEM_BASE_ID].mem_base +
			top_reg->io_path_cfg0_addr[csid_hw->hw_intf->hw_idx]);

		cam_io_w_mb(top_reg->dual_cfg_rst_val,
			soc_info->reg_map[CAM_IFE_CSID_TOP_MEM_BASE_ID].mem_base +
			top_reg->dual_csid_cfg0_addr[csid_hw->hw_intf->hw_idx]);
	}

	spin_lock_bh(&csid_hw->lock_state);
	csid_hw->flags.device_enabled = false;
	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	spin_unlock_bh(&csid_hw->lock_state);
	rc = cam_ife_csid_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_ISP, "CSID:%d Disable CSID SOC failed",
			csid_hw->hw_intf->hw_idx);

	csid_hw->counters.error_irq_count = 0;

	return rc;
}

int cam_ife_csid_ver2_deinit_hw(void *hw_priv,
	void *deinit_args, uint32_t arg_size)
{
	struct cam_ife_csid_ver2_hw *csid_hw  = NULL;
	struct cam_isp_resource_node           *res;
	struct cam_hw_info *hw_info;
	int rc = 0;

	if (!hw_priv || !deinit_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "CSID:Invalid arguments");
		return -EINVAL;
	}

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;
	res = (struct cam_isp_resource_node *)deinit_args;

	if (csid_hw->hw_info->hw_state == CAM_HW_STATE_POWER_DOWN) {
		CAM_WARN(CAM_ISP, "CSID:%u already powered down",
			csid_hw->hw_intf->hw_idx);
		return -EINVAL;
	}

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

	mutex_lock(&csid_hw->hw_info->hw_mutex);

	if (res->res_state == CAM_ISP_RESOURCE_STATE_INIT_HW)
		goto disable_hw;

	switch (res->res_id) {
	case  CAM_IFE_PIX_PATH_RES_IPP:
	case  CAM_IFE_PIX_PATH_RES_PPP:
	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
	case CAM_IFE_PIX_PATH_RES_RDI_4:
		rc = cam_ife_csid_ver2_disable_path(csid_hw, res);
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type%d",
			csid_hw->hw_intf->hw_idx, res->res_type);
		break;
	}

disable_hw:
	res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	cam_ife_csid_ver2_disable_core(csid_hw);
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	CAM_DBG(CAM_ISP, "De-Init CSID %d Path: %d",
		csid_hw->hw_intf->hw_idx, res->res_id);

	return rc;
}

int cam_ife_csid_ver2_start(void *hw_priv, void *args,
			uint32_t arg_size)
{
	struct cam_ife_csid_ver2_hw           *csid_hw  = NULL;
	struct cam_isp_resource_node          *res;
	struct cam_csid_hw_start_args         *start_args;
	struct cam_ife_csid_ver2_reg_info     *csid_reg;
	struct cam_hw_soc_info                *soc_info;
	struct cam_hw_info                    *hw_info;
	uint32_t                               rup_aup_mask = 0;
	int                                    rc = 0, i;

	if (!hw_priv || !args) {
		CAM_ERR(CAM_ISP, "CSID Invalid params");
		return  -EINVAL;
	}

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;
	soc_info = &csid_hw->hw_info->soc_info;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	start_args = (struct cam_csid_hw_start_args *)args;

	if (csid_hw->hw_info->hw_state == CAM_HW_STATE_POWER_DOWN) {
		CAM_WARN(CAM_ISP, "CSID:%u already powered down",
			csid_hw->hw_intf->hw_idx);
		return -EINVAL;
	}


	mutex_lock(&csid_hw->hw_info->hw_mutex);
	csid_hw->flags.sof_irq_triggered = false;
	csid_hw->counters.irq_debug_cnt = 0;

	rc = cam_ife_csid_ver2_enable_hw(csid_hw);

	for (i = 0; i < start_args->num_res; i++) {

		res = start_args->node_res[i];
		CAM_DBG(CAM_ISP, "CSID:%d res_type :%d res[id:%d name:%s]",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id, res->res_name);

		if (res->res_id >= CAM_IFE_PIX_PATH_RES_MAX) {
			CAM_ERR(CAM_ISP, "CSID:%d Invalid res tpe:%d res id:%d",
				csid_hw->hw_intf->hw_idx, res->res_type,
				res->res_id);
			rc = -EINVAL;
			goto end;
		}

		switch (res->res_id) {
		case  CAM_IFE_PIX_PATH_RES_IPP:
			rc = cam_ife_csid_ver2_program_ipp_path(csid_hw, res, &rup_aup_mask);
			if (rc)
				goto end;

			break;
		case  CAM_IFE_PIX_PATH_RES_PPP:
			rc = cam_ife_csid_ver2_program_ppp_path(csid_hw, res, &rup_aup_mask);
			if (rc)
				goto end;

			break;
		case CAM_IFE_PIX_PATH_RES_RDI_0:
		case CAM_IFE_PIX_PATH_RES_RDI_1:
		case CAM_IFE_PIX_PATH_RES_RDI_2:
		case CAM_IFE_PIX_PATH_RES_RDI_3:
		case CAM_IFE_PIX_PATH_RES_RDI_4:
			rc = cam_ife_csid_ver2_program_rdi_path(csid_hw, res, &rup_aup_mask);
			if (rc)
				goto end;

			break;
		default:
			CAM_ERR(CAM_ISP, "CSID:%d Invalid res type %d",
					csid_hw->hw_intf->hw_idx, res->res_type);
			break;
		}
	}

	/* Configure RUP/AUP/MUP @ streamon for all paths together */
	rup_aup_mask |= (csid_hw->rx_cfg.mup << csid_reg->cmn_reg->mup_shift_val);
	cam_io_w_mb(rup_aup_mask,
		soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base +
		csid_reg->cmn_reg->rup_aup_cmd_addr);
	CAM_DBG(CAM_ISP, "CSID:%u RUP_AUP_MUP: 0x%x at start",
		csid_hw->hw_intf->hw_idx, rup_aup_mask);

	cam_ife_csid_ver2_enable_csi2(csid_hw);

	for (i = 0; i < start_args->num_res; i++) {
		res = start_args->node_res[i];
		CAM_DBG(CAM_ISP, "CSID:%d res_type :%d res_id:%d",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id);
		cam_ife_csid_ver2_enable_path(csid_hw, res);
	}

end:
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

int cam_ife_csid_ver2_stop(void *hw_priv,
	void *stop_args, uint32_t arg_size)
{
	struct cam_ife_csid_ver2_hw *csid_hw  = NULL;
	struct cam_isp_resource_node           *res;
	struct cam_hw_info *hw_info;
	int rc = 0;
	uint32_t i;
	struct cam_csid_hw_stop_args         *csid_stop;
	struct cam_csid_reset_cfg_args       reset = {0};

	if (!hw_priv || !stop_args ||
		(arg_size != sizeof(struct cam_csid_hw_stop_args))) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;

	if (csid_hw->hw_info->hw_state == CAM_HW_STATE_POWER_DOWN) {
		CAM_WARN(CAM_ISP, "CSID:%u already powered down",
			csid_hw->hw_intf->hw_idx);
		return -EINVAL;
	}

	csid_stop = (struct cam_csid_hw_stop_args  *) stop_args;

	if (!csid_stop->num_res) {
		CAM_ERR(CAM_ISP, "CSID: Invalid args");
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "CSID:%d num_res %d", csid_hw->hw_intf->hw_idx,
		csid_stop->num_res);

	csid_hw->flags.device_enabled = false;

	reset.reset_type = (csid_hw->flags.fatal_err_detected) ? CAM_IFE_CSID_RESET_GLOBAL :
		CAM_IFE_CSID_RESET_PATH;
	cam_ife_csid_ver2_reset(hw_priv, &reset,
		sizeof(struct cam_csid_reset_cfg_args));

	atomic_set(&csid_hw->discard_frame_per_path, 0);
	mutex_lock(&csid_hw->hw_info->hw_mutex);
	for (i = 0; i < csid_stop->num_res; i++) {

		res = csid_stop->node_res[i];
		rc = cam_ife_csid_ver2_disable_path(csid_hw, res);
		res->res_state = CAM_ISP_RESOURCE_STATE_INIT_HW;
		CAM_DBG(CAM_ISP, "CSID:%d res_type %d Resource[id:%d name:%s]",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id,
			res->res_name);
	}

	if (csid_hw->buf_done_irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			csid_hw->csid_irq_controller,
			csid_hw->buf_done_irq_handle);
		csid_hw->buf_done_irq_handle = 0;

		cam_irq_controller_unregister_dependent(csid_hw->csid_irq_controller,
			csid_hw->buf_done_irq_controller);
	}

	if (csid_hw->top_err_irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			csid_hw->csid_irq_controller,
			csid_hw->top_err_irq_handle);
		csid_hw->top_err_irq_handle = 0;
	}

	cam_ife_csid_ver2_disable_csi2(csid_hw);
	mutex_unlock(&csid_hw->hw_info->hw_mutex);

	return rc;
}

int cam_ife_csid_ver2_read(void *hw_priv,
	void *read_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "CSID: un supported");

	return -EINVAL;
}

int cam_ife_csid_ver2_write(void *hw_priv,
	void *write_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "CSID: un supported");
	return -EINVAL;
}

static int cam_ife_csid_ver2_top_cfg(
	struct cam_ife_csid_ver2_hw *csid_hw, void *cmd_args)
{
	struct cam_ife_csid_top_config_args *top_args;
	struct cam_ife_csid_ver2_reg_info *csid_reg;
	uint32_t hw_idx;
	int rc = 0;

	if ((!csid_hw) || (!cmd_args))
		return -EINVAL;

	top_args = (struct cam_ife_csid_top_config_args *)cmd_args;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	hw_idx = csid_hw->hw_intf->hw_idx;
	csid_hw->top_cfg.out_ife_en = true;

	/* config out_core parameter*/

	switch (top_args->input_core_type) {
	case CAM_IFE_CSID_INPUT_CORE_NONE:
		csid_hw->top_cfg.input_core_type =
			CAM_IFE_CSID_INPUT_CORE_SEL_NONE;
		csid_hw->top_cfg.out_ife_en = false;
		break;

	case CAM_IFE_CSID_INPUT_CORE_IFE:
		csid_hw->top_cfg.input_core_type =
			CAM_IFE_CSID_INPUT_CORE_SEL_INTERNAL;
		break;

	case CAM_IFE_CSID_INPUT_CORE_SFE:
		csid_hw->top_cfg.out_ife_en = false;
	case CAM_IFE_CSID_INPUT_CORE_SFE_IFE:

		if (top_args->core_idx == 0) {
			csid_hw->top_cfg.input_core_type =
				CAM_IFE_CSID_INPUT_CORE_SEL_SFE_0;
		} else if (top_args->core_idx == 1) {
			csid_hw->top_cfg.input_core_type =
				CAM_IFE_CSID_INPUT_CORE_SEL_SFE_1;
		} else {
			rc = -EINVAL;
			CAM_ERR(CAM_ISP,
				"CSID: %d Invalid SFE node %d",
				hw_idx, top_args->core_idx);
		}

		break;

	case CAM_IFE_CSID_INPUT_CORE_CUST_IFE:

		if (!(csid_reg->csid_cust_node_map[hw_idx] &
			BIT(top_args->core_idx))) {
			CAM_ERR(CAM_ISP,
				"CSID: %d not supported for cust node %d",
				hw_idx, top_args->core_idx);
			rc = -EINVAL;
			break;
		}

		if (top_args->core_idx == 0) {
			csid_hw->top_cfg.input_core_type =
				CAM_IFE_CSID_INPUT_CORE_SEL_CUST_NODE_0;
		} else if (top_args->core_idx == 1) {
			csid_hw->top_cfg.input_core_type =
				CAM_IFE_CSID_INPUT_CORE_SEL_CUST_NODE_1;
		} else {
			rc = -EINVAL;
			CAM_ERR(CAM_ISP,
				"CSID: %d Invalid Cust node %d",
				hw_idx, top_args->core_idx);
		}
		break;
	default:
		break;
	}

	csid_hw->top_cfg.offline_sfe_en = top_args->is_sfe_offline;
	CAM_DBG(CAM_ISP,
		"CSID[%d] input_core_type:%d ife_out:%d sfe_offline:%d",
		hw_idx, csid_hw->top_cfg.input_core_type,
		csid_hw->top_cfg.out_ife_en,
		csid_hw->top_cfg.offline_sfe_en);
	CAM_DBG(CAM_ISP,
		"CSID[%d] Top config received: input_core_type%d core_idx:%d",
		hw_idx, top_args->input_core_type, top_args->core_idx);

	/*config dual sync params */

	if (csid_hw->sync_mode == CAM_ISP_HW_SYNC_NONE)
		return rc;
	else if (csid_hw->sync_mode == CAM_ISP_HW_SYNC_MASTER)
		csid_hw->top_cfg.master_slave_sel =
			csid_reg->top_reg->master_sel_val;
	else
		csid_hw->top_cfg.master_slave_sel =
			csid_reg->top_reg->slave_sel_val;

	csid_hw->top_cfg.dual_en = true;
	csid_hw->top_cfg.dual_sync_core_sel = csid_hw->dual_core_idx + 1;
	CAM_DBG(CAM_ISP,
		"CSID[%u] Top dual sync config core_sel: %d sync_mode: %d",
		hw_idx, csid_hw->sync_mode,
		csid_hw->top_cfg.dual_sync_core_sel);

	return rc;
}

static int cam_ife_csid_ver2_reg_update(
	struct cam_ife_csid_ver2_hw   *csid_hw,
	void *cmd_args, uint32_t arg_size)
{
	const struct cam_ife_csid_ver2_path_reg_info *path_reg;
	struct cam_isp_csid_reg_update_args         *rup_args = cmd_args;
	struct cam_cdm_utils_ops                    *cdm_util_ops;
	struct cam_ife_csid_ver2_reg_info           *csid_reg;
	struct cam_hw_soc_info                      *soc_info;
	uint32_t                                     size, i;
	uint32_t                                     reg_val_pair[2];
	uint32_t                                     rup_aup_mask = 0;
	int rc                                       = 0;

	if (arg_size != sizeof(struct cam_isp_csid_reg_update_args)) {
		CAM_ERR(CAM_ISP, "Invalid arg size: %d expected:%ld",
			arg_size, sizeof(struct cam_isp_csid_reg_update_args));
		return -EINVAL;
	}

	if (!rup_args) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}

	if (!rup_args->num_res ||
		rup_args->num_res > CAM_IFE_PIX_PATH_RES_MAX) {
		CAM_ERR(CAM_ISP, "Invalid num_res %u",
			rup_args->num_res);
		return -EINVAL;
	}

	cdm_util_ops = (struct cam_cdm_utils_ops *)rup_args->res[0]->cdm_ops;

	if (!cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid CDM ops");
		return -EINVAL;
	}

	size = cdm_util_ops->cdm_required_size_reg_random(1);
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((!rup_args->reg_write) && ((size * 4) > rup_args->cmd.size)) {
		CAM_ERR(CAM_ISP, "buf size:%d is not sufficient, expected: %d",
			rup_args->cmd.size, (size*4));
		return -EINVAL;
	}

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;

	for (i = 0; i < rup_args->num_res; i++) {
		path_reg = csid_reg->path_reg[rup_args->res[i]->res_id];
		if (!path_reg) {
			CAM_ERR(CAM_ISP, "Invalid Path Resource [id %d name %s]",
				rup_args->res[i]->res_id,
				rup_args->res[i]->res_name);
			rc = -EINVAL;
			goto err;
		}
		rup_aup_mask |= path_reg->rup_aup_mask;
	}

	reg_val_pair[0] = csid_reg->cmn_reg->rup_aup_cmd_addr;
	reg_val_pair[1] = rup_aup_mask;

	/* If not an actual request, configure last applied MUP */
	if (rup_args->reg_write)
		reg_val_pair[1] |= (rup_args->last_applied_mup <<
			csid_reg->cmn_reg->mup_shift_val);
	else
		reg_val_pair[1] |= (csid_hw->rx_cfg.mup <<
			csid_reg->cmn_reg->mup_shift_val);

	CAM_DBG(CAM_ISP, "CSID:%d configure rup_aup_mup: 0x%x offset: 0x%x via %s",
		csid_hw->hw_intf->hw_idx,
		reg_val_pair[1], reg_val_pair[0],
		(rup_args->reg_write ? "AHB" : "CDM"));

	if (rup_args->reg_write) {
		soc_info = &csid_hw->hw_info->soc_info;
		cam_io_w_mb(reg_val_pair[1],
			soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base +
			reg_val_pair[0]);
	} else {
		cdm_util_ops->cdm_write_regrandom(rup_args->cmd.cmd_buf_addr,
			1, reg_val_pair);
		rup_args->cmd.used_bytes = size * 4;
	}

	return rc;
err:
	CAM_ERR(CAM_ISP, "CSID[%d] wrong Resource[id:%d name:%s]",
		csid_hw->hw_intf->hw_idx,
		rup_args->res[i]->res_id,
		rup_args->res[i]->res_name);
	return rc;
}

static int cam_ife_csid_ver2_program_offline_go_cmd(
	struct cam_ife_csid_ver2_hw   *csid_hw,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_ife_csid_offline_cmd_update_args *go_args = cmd_args;
	struct cam_cdm_utils_ops                    *cdm_util_ops;
	struct cam_ife_csid_ver2_reg_info           *csid_reg;
	uint32_t                                     size;
	uint32_t                                     reg_val_pair[2];

	if (!go_args) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}

	if (arg_size !=
		sizeof(struct cam_ife_csid_offline_cmd_update_args)) {
		CAM_ERR(CAM_ISP, "Invalid arg size: %d expected:%ld",
			arg_size, sizeof(struct cam_ife_csid_offline_cmd_update_args));
		return -EINVAL;
	}

	cdm_util_ops = (struct cam_cdm_utils_ops *)go_args->res->cdm_ops;

	if (!cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid CDM ops");
		return -EINVAL;
	}

	size = cdm_util_ops->cdm_required_size_reg_random(1);
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((size * 4) > go_args->cmd.size) {
		CAM_ERR(CAM_ISP, "buf size:%d is not sufficient, expected: %d",
			go_args->cmd.size, (size*4));
		return -EINVAL;
	}

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;

	reg_val_pair[0] = csid_reg->cmn_reg->offline_cmd_addr;
	reg_val_pair[1] = 0x1;

	CAM_DBG(CAM_ISP, "CSID:%d offline_cmd 0x%x offset 0x%X",
		csid_hw->hw_intf->hw_idx,
		reg_val_pair[1], reg_val_pair[0]);

	cdm_util_ops->cdm_write_regrandom(go_args->cmd.cmd_buf_addr,
		1, reg_val_pair);

	go_args->cmd.used_bytes = size * 4;

	return 0;
}

static int cam_ife_csid_ver2_get_time_stamp(
	struct cam_ife_csid_ver2_hw  *csid_hw, void *cmd_args)
{
	const struct cam_ife_csid_ver2_path_reg_info *path_reg;
	struct cam_isp_resource_node         *res = NULL;
	uint64_t time_lo, time_hi;
	struct cam_hw_soc_info              *soc_info;
	struct cam_csid_get_time_stamp_args *timestamp_args;
	struct cam_ife_csid_ver2_reg_info *csid_reg;
	uint64_t  time_delta;
	struct timespec64 ts;
	uint32_t curr_0_sof_addr, curr_1_sof_addr;

	timestamp_args = (struct cam_csid_get_time_stamp_args *)cmd_args;
	res = timestamp_args->node_res;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
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

	path_reg = csid_reg->path_reg[res->res_id];

	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res :%d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	curr_0_sof_addr = path_reg->timestamp_curr0_sof_addr;
	curr_1_sof_addr = path_reg->timestamp_curr1_sof_addr;

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

	CAM_DBG(CAM_ISP, "Resource[id:%d name:%s timestamp:%lld]",
		res->res_id, res->res_name, timestamp_args->boot_timestamp);
	csid_hw->timestamp.prev_sof_ts = timestamp_args->time_stamp_val;
	csid_hw->timestamp.prev_boot_ts = timestamp_args->boot_timestamp;

	return 0;
}

static int cam_ife_csid_ver2_print_hbi_vbi(
	struct cam_ife_csid_ver2_hw  *csid_hw,
	struct cam_isp_resource_node *res)
{
	struct cam_hw_soc_info              *soc_info;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg = NULL;
	struct cam_ife_csid_ver2_reg_info *csid_reg;
	uint32_t  hbi, vbi;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	soc_info = &csid_hw->hw_info->soc_info;

	if (res->res_type != CAM_ISP_RESOURCE_PIX_PATH ||
		res->res_id >= CAM_IFE_PIX_PATH_RES_MAX) {
		CAM_DBG(CAM_ISP,
			"CSID[%u] Invalid res_type:%d res [id: %d name: %s]",
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

	path_reg = csid_reg->path_reg[res->res_id];
	if (!path_reg) {
		CAM_ERR(CAM_ISP, "CSID:%d invalid res %d",
			csid_hw->hw_intf->hw_idx, res->res_id);
		return -EINVAL;
	}

	hbi = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		path_reg->format_measure1_addr);
	vbi = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		path_reg->format_measure2_addr);

	CAM_INFO_RATE_LIMIT(CAM_ISP,
		"CSID[%u] Resource[id:%d name:%s hbi 0x%x vbi 0x%x]",
		csid_hw->hw_intf->hw_idx, res->res_id, res->res_name, hbi, vbi);

	return 0;
}

static int cam_ife_csid_ver2_set_dynamic_switch_config(
	struct cam_ife_csid_ver2_hw *csid_hw,
	void                        *cmd_args)
{
	struct cam_ife_csid_mode_switch_update_args *switch_update = NULL;

	if (!csid_hw)
		return -EINVAL;

	switch_update =
		(struct cam_ife_csid_mode_switch_update_args *)cmd_args;

	csid_hw->rx_cfg.mup = switch_update->mup_args.mup;
	CAM_DBG(CAM_ISP, "CSID[%u] MUP %u",
		csid_hw->hw_intf->hw_idx, csid_hw->rx_cfg.mup);

	/* Handle number of frames to initially drop based on num starting exposures */
	if (switch_update->exp_update_args.reset_discard_cfg) {
		struct cam_ife_csid_discard_frame_cfg_update *exp_update_args;
		struct cam_ife_csid_ver2_path_cfg            *path_cfg;
		struct cam_isp_resource_node                 *res;
		int i;

		exp_update_args = &switch_update->exp_update_args;
		for (i = CAM_IFE_PIX_PATH_RES_RDI_0; i <= CAM_IFE_PIX_PATH_RES_RDI_2; i++) {
			res = &csid_hw->path_res[i];
			path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
			if ((i - CAM_IFE_PIX_PATH_RES_RDI_0) >= exp_update_args->num_exposures) {
				path_cfg->skip_discard_frame_cfg = true;
				if (path_cfg->discard_init_frames) {
					path_cfg->discard_init_frames = false;
					path_cfg->num_frames_discard = 0;
					atomic_dec(&csid_hw->discard_frame_per_path);
					CAM_DBG(CAM_ISP, "CSID[%u] Reset discard config for %s",
						csid_hw->hw_intf->hw_idx, res->res_name);
				}
			}
		}
	}

	return 0;
}

static int cam_ife_csid_ver2_set_csid_clock(
	struct cam_ife_csid_ver2_hw          *csid_hw,
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

static int cam_ife_csid_ver2_mini_dump(
	struct cam_ife_csid_ver2_hw  *csid_hw,
	void *cmd_args)
{
	struct cam_ife_csid_ver2_mini_dump_data *md;
	uint32_t                                 i  = 0;
	struct cam_ife_csid_ver2_path_cfg       *path_cfg;
	struct cam_ife_csid_ver2_res_mini_dump  *md_res;
	struct cam_isp_resource_node            *res;
	struct cam_hw_mini_dump_args            *md_args;

	md_args = (struct cam_hw_mini_dump_args *)cmd_args;
	if (md_args->len < sizeof(*md)) {
		md_args->bytes_written = 0;
		return 0;
	}

	md  = (struct cam_ife_csid_ver2_mini_dump_data *)
		    ((uint8_t *)md_args->start_addr);
	md->clk_rate = csid_hw->clk_rate;
	md->hw_state = csid_hw->hw_info->hw_state;

	for (i = 0; i < CAM_IFE_PIX_PATH_RES_MAX; i++) {
		res = &csid_hw->path_res[i];
		path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
		if (!path_cfg)
			continue;

		md_res = &md->res[i];
		md_res->res_id = res->res_id;
		scnprintf(md_res->res_name, CAM_ISP_RES_NAME_LEN, res->res_name);
		memcpy(&md_res->path_cfg, path_cfg, sizeof(*path_cfg));
	}

	memcpy(&md->rx_cfg, &csid_hw->rx_cfg, sizeof(struct cam_ife_csid_rx_cfg));
	memcpy(&md->flags, &csid_hw->flags, sizeof(struct cam_ife_csid_hw_flags));
	memcpy(md->cid_data, csid_hw->cid_data,
		sizeof(struct cam_ife_csid_cid_data) * CAM_IFE_CSID_CID_MAX);
	md_args->bytes_written = sizeof(*md);

	return 0;
}

static int cam_ife_csid_ver2_dual_sync_cfg(
	struct cam_ife_csid_ver2_hw  *csid_hw,
	void *cmd_args)
{
	struct cam_ife_csid_dual_sync_args  *dual_sync_args;

	if (!csid_hw || !cmd_args) {
		CAM_ERR(CAM_ISP, "Invalid args %pK  %pK",
			csid_hw, cmd_args);
		return -EINVAL;
	}

	dual_sync_args = (struct cam_ife_csid_dual_sync_args *)cmd_args;
	csid_hw->sync_mode = dual_sync_args->sync_mode;
	csid_hw->dual_core_idx = dual_sync_args->dual_core_id;

	CAM_DBG(CAM_ISP, "CSID[%u] sync_mode %d dual_core_idx: %d",
		csid_hw->hw_intf->hw_idx, csid_hw->sync_mode,
		csid_hw->dual_core_idx);

	return 0;
}

static int cam_ife_csid_ver2_set_discard_frame_cfg(
	struct cam_ife_csid_ver2_hw    *csid_hw,
	void                           *cmd_args)
{
	struct cam_isp_resource_node                 *res;
	struct cam_ife_csid_ver2_path_cfg            *path_cfg;
	struct cam_ife_csid_discard_init_frame_args  *discard_config = NULL;

	if (!csid_hw)
		return -EINVAL;

	discard_config =
		(struct cam_ife_csid_discard_init_frame_args *)cmd_args;

	if (discard_config->num_frames == 0xffffffff) {
		CAM_ERR(CAM_ISP, "Invalid number of frames: 0x%x",
			discard_config->num_frames);
		return -EINVAL;
	}

	if (!discard_config->num_frames) {
		CAM_DBG(CAM_ISP, "No discard requested");
		return 0;
	}

	res = discard_config->res;
	if (res->res_type != CAM_ISP_RESOURCE_PIX_PATH ||
		res->res_id >= CAM_IFE_PIX_PATH_RES_MAX) {
		CAM_ERR(CAM_ISP, "CSID[%u] Invalid res_type: %d res id: %d",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	/* Handle first stream on and consecutive streamons post flush */
	if ((res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) ||
		(res->res_state == CAM_ISP_RESOURCE_STATE_INIT_HW)) {
		/* Skip if already set or need to skip based on stream on exposures */
		path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
		if (path_cfg->skip_discard_frame_cfg || path_cfg->discard_init_frames)
			goto end;

		path_cfg->discard_init_frames = true;
		path_cfg->sof_cnt = 0;
		path_cfg->num_frames_discard = discard_config->num_frames;
		atomic_inc(&csid_hw->discard_frame_per_path);
		CAM_DBG(CAM_ISP,
			"CSID[%u] discard num of frames: %u for path: %s discard_ref_cnt: %u",
			csid_hw->hw_intf->hw_idx, discard_config->num_frames, res->res_name,
			atomic_read(&csid_hw->discard_frame_per_path));
	}

end:
	return 0;
}

static int cam_ife_csid_ver2_process_cmd(void *hw_priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_ife_csid_ver2_hw          *csid_hw;
	struct cam_hw_info                   *hw_info;
	struct cam_isp_resource_node         *res = NULL;

	if (!hw_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid arguments");
		return -EINVAL;
	}

	hw_info = (struct cam_hw_info  *)hw_priv;
	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_info->core_info;

	switch (cmd_type) {
	case CAM_IFE_CSID_CMD_GET_TIME_STAMP:
		rc = cam_ife_csid_ver2_get_time_stamp(csid_hw, cmd_args);

		if (csid_hw->debug_info.debug_val &
				CAM_IFE_CSID_DEBUG_ENABLE_HBI_VBI_INFO) {
			res = ((struct cam_csid_get_time_stamp_args *)
				cmd_args)->node_res;
			cam_ife_csid_ver2_print_hbi_vbi(csid_hw, res);
		}
		break;
	case CAM_IFE_CSID_SET_CSID_DEBUG:
		rc = cam_ife_csid_ver2_set_debug(csid_hw,
			*((uint32_t *)cmd_args));
		break;
	case CAM_IFE_CSID_SOF_IRQ_DEBUG:
		rc = cam_ife_csid_ver2_sof_irq_debug(csid_hw, cmd_args);
		break;
	case CAM_ISP_HW_CMD_CSID_CLOCK_UPDATE:
		rc = cam_ife_csid_ver2_set_csid_clock(csid_hw, cmd_args);
		break;
	case CAM_ISP_HW_CMD_DUMP_HW:
		break;
	case CAM_IFE_CSID_TOP_CONFIG:
		rc = cam_ife_csid_ver2_top_cfg(csid_hw, cmd_args);
		break;
	case CAM_ISP_HW_CMD_GET_CHANGE_BASE:
		rc = cam_ife_csid_get_base(&hw_info->soc_info,
			CAM_IFE_CSID_CLC_MEM_BASE_ID,
			cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_REG_UPDATE:
		rc = cam_ife_csid_ver2_reg_update(csid_hw,
			cmd_args, arg_size);
		break;
	case CAM_IFE_CSID_SET_DUAL_SYNC_CONFIG:
		rc = cam_ife_csid_ver2_dual_sync_cfg(csid_hw,
			cmd_args);
		break;
	case CAM_ISP_HW_CSID_MINI_DUMP:
		rc  = cam_ife_csid_ver2_mini_dump(csid_hw, cmd_args);
		break;
	case CAM_IFE_CSID_PROGRAM_OFFLINE_CMD:
		rc = cam_ife_csid_ver2_program_offline_go_cmd(
			csid_hw, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_CSID_DYNAMIC_SWITCH_UPDATE:
		rc = cam_ife_csid_ver2_set_dynamic_switch_config(csid_hw, cmd_args);
		break;
	case CAM_ISP_HW_CMD_CSID_CHANGE_HALT_MODE:
		break;
	case CAM_ISP_HW_CMD_QUERY_REGSPACE_DATA: {
		struct cam_hw_soc_info *soc_info;

		soc_info = &csid_hw->hw_info->soc_info;
		*((struct cam_hw_soc_info **)cmd_args) = soc_info;
		break;
	}
	case CAM_ISP_HW_CMD_CSID_DISCARD_INIT_FRAMES:
		rc = cam_ife_csid_ver2_set_discard_frame_cfg(csid_hw, cmd_args);
		break;
	default:
		CAM_ERR(CAM_ISP, "CSID:%d unsupported cmd:%d",
			csid_hw->hw_intf->hw_idx, cmd_type);
		rc = -EINVAL;
		break;
	}
	return rc;

}

static irqreturn_t cam_ife_csid_irq(int irq_num, void *data)
{
	struct cam_ife_csid_ver2_hw *csid_hw = data;

	if (!csid_hw)
		return IRQ_NONE;

	return cam_irq_controller_handle_irq(irq_num,
		csid_hw->csid_irq_controller, CAM_IRQ_EVT_GROUP_0);
}

static void cam_ife_csid_ver2_free_res(struct cam_ife_csid_ver2_hw *csid_hw)
{

	struct cam_isp_resource_node *res;
	uint32_t num_paths;
	int i;
	struct cam_ife_csid_ver2_reg_info *csid_reg;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	num_paths = csid_reg->cmn_reg->num_udis;

	for (i = 0; i < num_paths; i++) {
		res = &csid_hw->path_res[CAM_IFE_PIX_PATH_RES_UDI_0 + i];
		kfree(res->res_priv);
		res->res_priv = NULL;
	}

	num_paths = csid_reg->cmn_reg->num_rdis;

	for (i = 0; i < num_paths; i++) {
		res = &csid_hw->path_res[CAM_IFE_PIX_PATH_RES_RDI_0 + i];
		kfree(res->res_priv);
		res->res_priv = NULL;
	}

	kfree(csid_hw->path_res[CAM_IFE_PIX_PATH_RES_IPP].res_priv);
	csid_hw->path_res[CAM_IFE_PIX_PATH_RES_IPP].res_priv = NULL;
	kfree(csid_hw->path_res[CAM_IFE_PIX_PATH_RES_PPP].res_priv);
	csid_hw->path_res[CAM_IFE_PIX_PATH_RES_PPP].res_priv = NULL;
}

static int cam_ife_ver2_hw_alloc_res(
	struct cam_isp_resource_node *res,
	uint32_t res_type,
	struct cam_hw_intf   *hw_intf,
	uint32_t res_id)

{
	struct cam_ife_csid_ver2_path_cfg *path_cfg = NULL;

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

static int cam_ife_csid_ver2_hw_init_path_res(
	struct cam_ife_csid_ver2_hw   *csid_hw)
{
	int rc = 0;
	int i;
	struct cam_ife_csid_ver2_reg_info *csid_reg;
	struct cam_isp_resource_node *res;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;

	/* Initialize the IPP resources */
	if (csid_reg->cmn_reg->num_pix) {
		res = &csid_hw->path_res[CAM_IFE_PIX_PATH_RES_IPP];
		rc = cam_ife_ver2_hw_alloc_res(
			res,
			CAM_ISP_RESOURCE_PIX_PATH,
			csid_hw->hw_intf,
			CAM_IFE_PIX_PATH_RES_IPP);
		if (rc) {
			CAM_ERR(CAM_ISP, "CSID: %d IPP res init fail",
				csid_hw->hw_intf->hw_idx);
			goto free_res;
		}
		scnprintf(csid_hw->path_res[CAM_IFE_PIX_PATH_RES_IPP].res_name,
			CAM_ISP_RES_NAME_LEN, "IPP");
	}

	/* Initialize PPP resource */
	if (csid_reg->cmn_reg->num_ppp) {
		res = &csid_hw->path_res[CAM_IFE_PIX_PATH_RES_PPP];
		rc = cam_ife_ver2_hw_alloc_res(
			res,
			CAM_ISP_RESOURCE_PIX_PATH,
			csid_hw->hw_intf,
			CAM_IFE_PIX_PATH_RES_PPP);
		if (rc) {
			CAM_ERR(CAM_ISP, "CSID: %d PPP res init fail",
				csid_hw->hw_intf->hw_idx);
			goto free_res;
		}
		scnprintf(csid_hw->path_res[CAM_IFE_PIX_PATH_RES_PPP].res_name,
			CAM_ISP_RES_NAME_LEN, "PPP");
	}

	/* Initialize the RDI resource */
	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++) {
		/* res type is from RDI 0 to RDI3 */
		res = &csid_hw->path_res[CAM_IFE_PIX_PATH_RES_RDI_0 + i];
		rc = cam_ife_ver2_hw_alloc_res(
			res,
			CAM_ISP_RESOURCE_PIX_PATH,
			csid_hw->hw_intf,
			CAM_IFE_PIX_PATH_RES_RDI_0 + i);
		if (rc) {
			CAM_ERR(CAM_ISP, "CSID: %d RDI[%d] res init fail",
				csid_hw->hw_intf->hw_idx, i);
			goto free_res;
		}
		scnprintf(res->res_name, CAM_ISP_RES_NAME_LEN, "RDI_%d", i);
	}

	/* Initialize the UDI resource */
	for (i = 0; i < csid_reg->cmn_reg->num_udis; i++) {
		/* res type is from UDI0 to UDI3 */
		res = &csid_hw->path_res[CAM_IFE_PIX_PATH_RES_UDI_0 + i];
		rc = cam_ife_ver2_hw_alloc_res(
			res,
			CAM_ISP_RESOURCE_PIX_PATH,
			csid_hw->hw_intf,
			CAM_IFE_PIX_PATH_RES_UDI_0 + i);
		if (rc) {
			CAM_ERR(CAM_ISP, "CSID: %d UDI[%d] res init fail",
				csid_hw->hw_intf->hw_idx, i);
			goto free_res;
		}
		scnprintf(res->res_name, CAM_ISP_RES_NAME_LEN, "UDI_%d", i);
	}

	return rc;

free_res:
	cam_ife_csid_ver2_free_res(csid_hw);
	return rc;
}

static int cam_ife_csid_hw_init_irq(
	struct cam_ife_csid_ver2_hw *csid_hw)
{
	int                                rc = 0;
	int                                i;
	struct cam_hw_soc_info            *soc_info;
	void __iomem                      *mem_base;
	struct cam_ife_csid_ver2_reg_info *csid_reg;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;

	soc_info = &csid_hw->hw_info->soc_info;
	mem_base = soc_info->reg_map[CAM_IFE_CSID_CLC_MEM_BASE_ID].mem_base;

	rc = cam_irq_controller_init("csid",
		mem_base, csid_reg->irq_reg_info,
		&csid_hw->csid_irq_controller);

	if (rc) {
		CAM_ERR(CAM_ISP,
			"Failed to init CSID irq controller rc = %d", rc);
		return rc;
	}

	rc = cam_irq_controller_init("csid_buf_done",
		mem_base,
		csid_reg->buf_done_irq_reg_info,
		&csid_hw->buf_done_irq_controller);

	if (rc) {
		CAM_ERR(CAM_ISP,
			"Failed to init CSID buf_done irq controller rc = %d",
			rc);
		return rc;
	}

	spin_lock_init(&csid_hw->path_payload_lock);
	INIT_LIST_HEAD(&csid_hw->path_free_payload_list);
	for (i = 0; i < CAM_IFE_CSID_VER2_PAYLOAD_MAX; i++) {
		INIT_LIST_HEAD(&csid_hw->path_evt_payload[i].list);
		list_add_tail(&csid_hw->path_evt_payload[i].list,
			&csid_hw->path_free_payload_list);
	}
	spin_lock_init(&csid_hw->rx_payload_lock);
	INIT_LIST_HEAD(&csid_hw->rx_free_payload_list);
	for (i = 0; i < CAM_IFE_CSID_VER2_PAYLOAD_MAX; i++) {
		INIT_LIST_HEAD(&csid_hw->rx_evt_payload[i].list);
		list_add_tail(&csid_hw->rx_evt_payload[i].list,
			&csid_hw->rx_free_payload_list);
	}

	return rc;
}

int cam_ife_csid_hw_ver2_init(struct cam_hw_intf *hw_intf,
	struct cam_ife_csid_core_info *core_info,
	bool is_custom)
{
	int rc = -EINVAL;
	struct cam_hw_info                   *hw_info;
	struct cam_ife_csid_ver2_hw          *csid_hw = NULL;

	if (!hw_intf || !core_info) {
		CAM_ERR(CAM_ISP, "Invalid parameters intf: %pK hw_info: %pK",
			hw_intf, core_info);
		return rc;
	}

	hw_info = (struct cam_hw_info  *)hw_intf->hw_priv;

	csid_hw = kzalloc(sizeof(struct cam_ife_csid_ver2_hw), GFP_KERNEL);

	if (!csid_hw) {
		CAM_ERR(CAM_ISP, "Csid core %d hw allocation fails",
			hw_intf->hw_idx);
		return -ENOMEM;
	}

	hw_info->core_info = csid_hw;
	csid_hw->hw_intf = hw_intf;
	csid_hw->hw_info = hw_info;
	csid_hw->core_info = core_info;
	CAM_DBG(CAM_ISP, "type %d index %d",
		hw_intf->hw_type,
		hw_intf->hw_idx);

	csid_hw->flags.device_enabled = false;
	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&csid_hw->hw_info->hw_mutex);
	spin_lock_init(&csid_hw->hw_info->hw_lock);
	spin_lock_init(&csid_hw->lock_state);
	init_completion(&csid_hw->hw_info->hw_complete);
	atomic_set(&csid_hw->discard_frame_per_path, 0);

	rc = cam_ife_csid_init_soc_resources(&csid_hw->hw_info->soc_info,
			cam_ife_csid_irq, csid_hw, is_custom);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "CSID:%d Failed to init_soc",
			hw_intf->hw_idx);
		return rc;
	}

	if (cam_cpas_is_feature_supported(CAM_CPAS_QCFA_BINNING_ENABLE,
		CAM_CPAS_HW_IDX_ANY, NULL))
		csid_hw->flags.binning_enabled = true;

	csid_hw->hw_intf->hw_ops.get_hw_caps =
						cam_ife_csid_ver2_get_hw_caps;
	csid_hw->hw_intf->hw_ops.init        = cam_ife_csid_ver2_init_hw;
	csid_hw->hw_intf->hw_ops.deinit      = cam_ife_csid_ver2_deinit_hw;
	csid_hw->hw_intf->hw_ops.reset       = cam_ife_csid_ver2_reset;
	csid_hw->hw_intf->hw_ops.reserve     = cam_ife_csid_ver2_reserve;
	csid_hw->hw_intf->hw_ops.release     = cam_ife_csid_ver2_release;
	csid_hw->hw_intf->hw_ops.start       = cam_ife_csid_ver2_start;
	csid_hw->hw_intf->hw_ops.stop        = cam_ife_csid_ver2_stop;
	csid_hw->hw_intf->hw_ops.read        = cam_ife_csid_ver2_read;
	csid_hw->hw_intf->hw_ops.write       = cam_ife_csid_ver2_write;
	csid_hw->hw_intf->hw_ops.process_cmd =
						cam_ife_csid_ver2_process_cmd;

	rc = cam_ife_csid_hw_init_irq(csid_hw);

	rc = cam_ife_csid_ver2_hw_init_path_res(csid_hw);

	if (rc) {
		CAM_ERR(CAM_ISP, "CSID[%d] Probe Init failed",
			hw_intf->hw_idx);
		return rc;
	}
	csid_hw->debug_info.debug_val = 0;
	csid_hw->counters.error_irq_count = 0;

	return 0;

}
EXPORT_SYMBOL(cam_ife_csid_hw_ver2_init);

int cam_ife_csid_hw_ver2_deinit(struct cam_hw_info *hw_priv)
{
	struct cam_ife_csid_ver2_hw   *csid_hw;
	unsigned long flags;
	int rc = -EINVAL;
	int i;

	csid_hw = (struct cam_ife_csid_ver2_hw *)hw_priv->core_info;

	if (!csid_hw) {
		CAM_ERR(CAM_ISP, "Invalid param");
		return rc;
	}

	spin_lock_irqsave(&csid_hw->path_payload_lock, flags);
	INIT_LIST_HEAD(&csid_hw->path_free_payload_list);
	for (i = 0; i < CAM_IFE_CSID_VER2_PAYLOAD_MAX; i++)
		INIT_LIST_HEAD(&csid_hw->path_evt_payload[i].list);
	spin_unlock_irqrestore(&csid_hw->path_payload_lock, flags);

	spin_lock_irqsave(&csid_hw->rx_payload_lock, flags);
	INIT_LIST_HEAD(&csid_hw->rx_free_payload_list);
	for (i = 0; i < CAM_IFE_CSID_VER2_PAYLOAD_MAX; i++)
		INIT_LIST_HEAD(&csid_hw->rx_evt_payload[i].list);
	spin_unlock_irqrestore(&csid_hw->rx_payload_lock, flags);

	rc = cam_irq_controller_deinit(&csid_hw->csid_irq_controller);
	rc = cam_irq_controller_deinit(&csid_hw->buf_done_irq_controller);

	/* release the privdate data memory from resources */
	cam_ife_csid_ver2_free_res(csid_hw);

	cam_ife_csid_deinit_soc_resources(&csid_hw->hw_info->soc_info);

	return 0;
}
EXPORT_SYMBOL(cam_ife_csid_hw_ver2_deinit);
