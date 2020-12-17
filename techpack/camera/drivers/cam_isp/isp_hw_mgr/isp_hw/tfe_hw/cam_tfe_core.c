// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/ratelimit.h>
#include <media/cam_tfe.h>
#include "cam_cdm_util.h"
#include "cam_tasklet_util.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_tfe_soc.h"
#include "cam_tfe_core.h"
#include "cam_tfe_bus.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"
#include <dt-bindings/msm/msm-camera.h>

static const char drv_name[] = "tfe";

#define CAM_TFE_HW_RESET_HW_AND_REG_VAL       0x1
#define CAM_TFE_HW_RESET_HW_VAL               0x10000
#define CAM_TFE_DELAY_BW_REDUCTION_NUM_FRAMES 3
#define CAM_TFE_CAMIF_IRQ_SOF_DEBUG_CNT_MAX  2
#define CAM_TFE_DELAY_BW_REDUCTION_NUM_FRAMES 3

struct cam_tfe_top_common_data {
	struct cam_hw_soc_info                     *soc_info;
	struct cam_hw_intf                         *hw_intf;
	struct cam_tfe_top_reg_offset_common       *common_reg;
	struct cam_tfe_reg_dump_data               *reg_dump_data;
};

struct cam_tfe_top_priv {
	struct cam_tfe_top_common_data      common_data;
	struct cam_isp_resource_node        in_rsrc[CAM_TFE_TOP_IN_PORT_MAX];
	unsigned long                       hw_clk_rate;
	struct cam_axi_vote                 applied_axi_vote;
	struct cam_axi_vote             req_axi_vote[CAM_TFE_TOP_IN_PORT_MAX];
	unsigned long                   req_clk_rate[CAM_TFE_TOP_IN_PORT_MAX];
	struct cam_axi_vote             last_vote[CAM_TFE_TOP_IN_PORT_MAX *
					CAM_TFE_DELAY_BW_REDUCTION_NUM_FRAMES];
	uint32_t                        last_counter;
	uint64_t                        total_bw_applied;
	enum cam_tfe_bw_control_action
		axi_vote_control[CAM_TFE_TOP_IN_PORT_MAX];
	uint32_t                          irq_prepared_mask[3];
	void                            *tasklet_info;
	struct timeval                    sof_ts;
	struct timeval                    epoch_ts;
	struct timeval                    eof_ts;
	struct timeval                    error_ts;
};

struct cam_tfe_camif_data {
	void __iomem                                *mem_base;
	struct cam_hw_intf                          *hw_intf;
	struct cam_tfe_top_reg_offset_common        *common_reg;
	struct cam_tfe_camif_reg                    *camif_reg;
	struct cam_tfe_camif_reg_data               *reg_data;
	struct cam_hw_soc_info                      *soc_info;


	cam_hw_mgr_event_cb_func           event_cb;
	void                              *priv;
	enum cam_isp_hw_sync_mode          sync_mode;
	uint32_t                           dsp_mode;
	uint32_t                           pix_pattern;
	uint32_t                           left_first_pixel;
	uint32_t                           left_last_pixel;
	uint32_t                           right_first_pixel;
	uint32_t                           right_last_pixel;
	uint32_t                           first_line;
	uint32_t                           last_line;
	bool                               enable_sof_irq_debug;
	uint32_t                           irq_debug_cnt;
	uint32_t                           camif_debug;
	uint32_t                           camif_pd_enable;
	uint32_t                           dual_tfe_sync_sel;
};

struct cam_tfe_rdi_data {
	void __iomem                                *mem_base;
	struct cam_hw_intf                          *hw_intf;
	struct cam_tfe_top_reg_offset_common        *common_reg;
	struct cam_tfe_rdi_reg                      *rdi_reg;
	struct cam_tfe_rdi_reg_data                 *reg_data;
	cam_hw_mgr_event_cb_func                     event_cb;
	void                                        *priv;
	enum cam_isp_hw_sync_mode                    sync_mode;
	uint32_t                                     pix_pattern;
	uint32_t                                     left_first_pixel;
	uint32_t                                     left_last_pixel;
	uint32_t                                     first_line;
	uint32_t                                     last_line;
};

static int cam_tfe_validate_pix_pattern(uint32_t pattern)
{
	int rc;

	switch (pattern) {
	case CAM_ISP_TFE_PATTERN_BAYER_RGRGRG:
	case CAM_ISP_TFE_PATTERN_BAYER_GRGRGR:
	case CAM_ISP_TFE_PATTERN_BAYER_BGBGBG:
	case CAM_ISP_TFE_PATTERN_BAYER_GBGBGB:
	case CAM_ISP_TFE_PATTERN_YUV_YCBYCR:
	case CAM_ISP_TFE_PATTERN_YUV_YCRYCB:
	case CAM_ISP_TFE_PATTERN_YUV_CBYCRY:
	case CAM_ISP_TFE_PATTERN_YUV_CRYCBY:
		rc = 0;
		break;
	default:
		CAM_ERR(CAM_ISP, "Error Invalid pix pattern:%d", pattern);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int cam_tfe_get_evt_payload(struct cam_tfe_hw_core_info *core_info,
	struct cam_tfe_irq_evt_payload    **evt_payload)
{
	spin_lock(&core_info->spin_lock);
	if (list_empty(&core_info->free_payload_list)) {
		*evt_payload = NULL;
		spin_unlock(&core_info->spin_lock);
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No free payload, core id 0x%x",
			core_info->core_index);
		return -ENODEV;
	}

	*evt_payload = list_first_entry(&core_info->free_payload_list,
		struct cam_tfe_irq_evt_payload, list);
	list_del_init(&(*evt_payload)->list);
	spin_unlock(&core_info->spin_lock);

	return 0;
}

int cam_tfe_put_evt_payload(void             *core_info,
	struct cam_tfe_irq_evt_payload  **evt_payload)
{
	struct cam_tfe_hw_core_info        *tfe_core_info = core_info;
	unsigned long                       flags;

	if (!core_info) {
		CAM_ERR(CAM_ISP, "Invalid param core_info NULL");
		return -EINVAL;
	}
	if (*evt_payload == NULL) {
		CAM_ERR(CAM_ISP, "No payload to put");
		return -EINVAL;
	}

	spin_lock_irqsave(&tfe_core_info->spin_lock, flags);
	(*evt_payload)->error_type = 0;
	list_add_tail(&(*evt_payload)->list, &tfe_core_info->free_payload_list);
	*evt_payload = NULL;
	spin_unlock_irqrestore(&tfe_core_info->spin_lock, flags);

	return 0;
}

int cam_tfe_get_hw_caps(void *hw_priv, void *get_hw_cap_args,
		uint32_t arg_size)
{
	return -EPERM;
}

void cam_tfe_get_timestamp(struct cam_isp_timestamp *time_stamp)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);
	time_stamp->mono_time.tv_sec    = ts.tv_sec;
	time_stamp->mono_time.tv_usec   = ts.tv_nsec/1000;
}

int cam_tfe_irq_config(void     *tfe_core_data,
	uint32_t  *irq_mask, uint32_t num_reg, bool enable)
{
	struct cam_tfe_hw_core_info    *core_info;
	struct cam_tfe_top_priv        *top_priv;
	struct cam_hw_soc_info         *soc_info;
	void __iomem                   *mem_base;
	bool                            need_lock;
	unsigned long                   flags = 0;
	uint32_t i, val;

	if (!tfe_core_data) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"Invalid core data");
		return -EINVAL;
	}

	core_info = (struct cam_tfe_hw_core_info    *)tfe_core_data;
	top_priv = (struct cam_tfe_top_priv        *)core_info->top_priv;
	soc_info = (struct cam_hw_soc_info  *)top_priv->common_data.soc_info;
	mem_base = soc_info->reg_map[TFE_CORE_BASE_IDX].mem_base;

	need_lock = !in_irq();
	if (need_lock)
		spin_lock_irqsave(&core_info->spin_lock, flags);

	for (i = 0; i < num_reg; i++) {
		val = cam_io_r_mb(mem_base  +
			core_info->tfe_hw_info->top_irq_mask[i]);
		if (enable)
			val |= irq_mask[i];
		else
			val &= ~irq_mask[i];
		cam_io_w_mb(val, mem_base +
			core_info->tfe_hw_info->top_irq_mask[i]);
	}
	if (need_lock)
		spin_unlock_irqrestore(&core_info->spin_lock, flags);

	return 0;
}

static void cam_tfe_log_tfe_in_debug_status(
	struct cam_tfe_top_priv              *top_priv)
{
	void __iomem                         *mem_base;
	struct cam_tfe_camif_data            *camif_data;
	struct cam_tfe_rdi_data              *rdi_data;
	uint32_t  i, val_0, val_1;

	mem_base = top_priv->common_data.soc_info->reg_map[0].mem_base;

	for (i = 0; i < CAM_TFE_TOP_IN_PORT_MAX; i++) {
		if ((top_priv->in_rsrc[i].res_state !=
			CAM_ISP_RESOURCE_STATE_STREAMING))
			continue;

		if (top_priv->in_rsrc[i].res_id == CAM_ISP_HW_TFE_IN_CAMIF) {
			camif_data = (struct cam_tfe_camif_data  *)
				top_priv->in_rsrc[i].res_priv;
			val_0 = cam_io_r(mem_base  +
				camif_data->camif_reg->debug_0);
			val_1 = cam_io_r(mem_base  +
				camif_data->camif_reg->debug_1);
			CAM_INFO(CAM_ISP,
				"camif debug1:0x%x Height:0x%x, width:0x%x",
				val_1,
				((val_0 >> 16) & 0x1FFF),
				(val_0 & 0x1FFF));
			CAM_INFO(CAM_ISP,
				"Acquired sync mode:%d left start pxl:0x%x end_pixel:0x%x",
				camif_data->sync_mode,
				camif_data->left_first_pixel,
				camif_data->left_last_pixel);

			if (camif_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE)
				CAM_INFO(CAM_ISP,
					"sync mode:%d right start pxl:0x%x end_pixel:0x%x",
					camif_data->sync_mode,
					camif_data->right_first_pixel,
					camif_data->right_last_pixel);

			CAM_INFO(CAM_ISP,
				"Acquired line start:0x%x line end:0x%x",
				camif_data->first_line,
				camif_data->last_line);
		} else if ((top_priv->in_rsrc[i].res_id >=
			CAM_ISP_HW_TFE_IN_RDI0) ||
			(top_priv->in_rsrc[i].res_id <=
			CAM_ISP_HW_TFE_IN_RDI2)) {
			rdi_data = (struct cam_tfe_rdi_data  *)
				top_priv->in_rsrc[i].res_priv;
			val_0 = cam_io_r(mem_base  +
				rdi_data->rdi_reg->rdi_debug_0);
			val_1 = cam_io_r(mem_base  +
				rdi_data->rdi_reg->rdi_debug_1);
			CAM_INFO(CAM_ISP,
				"RDI res id:%d debug1:0x%x Height:0x%x, width:0x%x",
				top_priv->in_rsrc[i].res_id,
				val_1, ((val_0 >> 16) & 0x1FFF),
				(val_0 & 0x1FFF));
			CAM_INFO(CAM_ISP,
				"sync mode:%d left start pxl:0x%x end_pixel:0x%x",
				rdi_data->sync_mode,
				rdi_data->left_first_pixel,
				rdi_data->left_last_pixel);
			CAM_INFO(CAM_ISP,
				"sync mode:%d line start:0x%x line end:0x%x",
				rdi_data->sync_mode,
				rdi_data->first_line,
				rdi_data->last_line);
		}
	}
}
static void cam_tfe_log_error_irq_status(
	struct cam_tfe_hw_core_info          *core_info,
	struct cam_tfe_top_priv              *top_priv,
	struct cam_tfe_irq_evt_payload       *evt_payload)
{
	struct cam_tfe_hw_info               *hw_info;
	void __iomem                         *mem_base;
	struct cam_hw_soc_info               *soc_info;
	struct cam_tfe_soc_private           *soc_private;

	struct cam_tfe_clc_hw_status         *clc_hw_status;
	struct timespec64 ts;
	uint32_t  i, val_0, val_1, val_2, val_3;


	ktime_get_boottime_ts64(&ts);
	hw_info = core_info->tfe_hw_info;
	mem_base = top_priv->common_data.soc_info->reg_map[0].mem_base;
	soc_info = top_priv->common_data.soc_info;
	soc_private = top_priv->common_data.soc_info->soc_private;

	CAM_INFO(CAM_ISP, "current monotonic time stamp seconds %lld:%lld",
		ts.tv_sec, ts.tv_nsec/1000);
	CAM_INFO(CAM_ISP,
		"ERROR time %lld:%lld SOF %lld:%lld EPOCH %lld:%lld EOF %lld:%lld",
		top_priv->error_ts.tv_sec,
		top_priv->error_ts.tv_usec,
		top_priv->sof_ts.tv_sec,
		top_priv->sof_ts.tv_usec,
		top_priv->epoch_ts.tv_sec,
		top_priv->epoch_ts.tv_usec,
		top_priv->eof_ts.tv_sec,
		top_priv->eof_ts.tv_usec);

	val_0 = cam_io_r(mem_base  +
		top_priv->common_data.common_reg->debug_0);
	val_1 = cam_io_r(mem_base  +
		top_priv->common_data.common_reg->debug_1);
	val_2 = cam_io_r(mem_base  +
		top_priv->common_data.common_reg->debug_2);
	val_3 = cam_io_r(mem_base  +
		top_priv->common_data.common_reg->debug_3);

	CAM_INFO(CAM_ISP, "TOP IRQ[0]:0x%x IRQ[1]:0x%x IRQ[2]:0x%x",
		evt_payload->irq_reg_val[0], evt_payload->irq_reg_val[1],
		evt_payload->irq_reg_val[2]);

	CAM_INFO(CAM_ISP, "Top debug [0]:0x%x [1]:0x%x [2]:0x%x [3]:0x%x",
		val_0, val_1, val_2, val_3);

	cam_cpas_reg_read(soc_private->cpas_handle,
		CAM_CPAS_REG_CAMNOC, 0x20, true, &val_0);
	CAM_INFO(CAM_ISP, "tfe_niu_MaxWr_Low offset 0x20 val 0x%x",
		val_0);

	val_0 = cam_io_r(mem_base  +
		top_priv->common_data.common_reg->perf_pixel_count);

	val_1 = cam_io_r(mem_base  +
		top_priv->common_data.common_reg->perf_line_count);

	val_2 = cam_io_r(mem_base  +
		top_priv->common_data.common_reg->perf_stall_count);

	val_3 = cam_io_r(mem_base  +
		top_priv->common_data.common_reg->perf_always_count);

	CAM_INFO(CAM_ISP,
		"Top perf cnt pix:0x%x line:0x%x stall:0x%x always:0x%x",
		val_0, val_1, val_2, val_3);

	clc_hw_status = hw_info->clc_hw_status_info;
	for (i = 0; i < hw_info->num_clc; i++) {
		val_0 = cam_io_r(mem_base  +
			clc_hw_status[i].hw_status_reg);
		if (val_0)
			CAM_INFO(CAM_ISP,
				"CLC HW status :name:%s offset:0x%x value:0x%x",
				clc_hw_status[i].name,
				clc_hw_status[i].hw_status_reg,
				val_0);
	}

	cam_tfe_log_tfe_in_debug_status(top_priv);

	/* Check the overflow errors */
	if (evt_payload->irq_reg_val[0] & hw_info->error_irq_mask[0]) {
		if (evt_payload->irq_reg_val[0] & BIT(8))
			CAM_INFO(CAM_ISP, "PP_FRAME_DROP");

		if (evt_payload->irq_reg_val[0] & BIT(9))
			CAM_INFO(CAM_ISP, "RDI0_FRAME_DROP");

		if (evt_payload->irq_reg_val[0] & BIT(10))
			CAM_INFO(CAM_ISP, "RDI1_FRAME_DROP");

		if (evt_payload->irq_reg_val[0] & BIT(11))
			CAM_INFO(CAM_ISP, "RDI2_FRAME_DROP");

		if (evt_payload->irq_reg_val[0] & BIT(16))
			CAM_INFO(CAM_ISP, "PP_OVERFLOW");

		if (evt_payload->irq_reg_val[0] & BIT(17))
			CAM_INFO(CAM_ISP, "RDI0_OVERFLOW");

		if (evt_payload->irq_reg_val[0] & BIT(18))
			CAM_INFO(CAM_ISP, "RDI1_OVERFLOW");

		if (evt_payload->irq_reg_val[0] & BIT(19))
			CAM_INFO(CAM_ISP, "RDI2_OVERFLOW");
	}

	/* Check the violation errors */
	if (evt_payload->irq_reg_val[2] & hw_info->error_irq_mask[2]) {
		if (evt_payload->irq_reg_val[2] & BIT(0))
			CAM_INFO(CAM_ISP, "PP_CAMIF_VIOLATION");

		if (evt_payload->irq_reg_val[2] & BIT(1))
			CAM_INFO(CAM_ISP, "PP_VIOLATION");

		if (evt_payload->irq_reg_val[2] & BIT(2))
			CAM_INFO(CAM_ISP, "RDI0_CAMIF_VIOLATION");

		if (evt_payload->irq_reg_val[2] & BIT(3))
			CAM_INFO(CAM_ISP, "RDI1_CAMIF_VIOLATION");

		if (evt_payload->irq_reg_val[2] & BIT(4))
			CAM_INFO(CAM_ISP, "RDI2_CAMIF_VIOLATION");

		if (evt_payload->irq_reg_val[2] & BIT(5))
			CAM_INFO(CAM_ISP, "DIAG_VIOLATION");

		val_0 = cam_io_r(mem_base  +
		top_priv->common_data.common_reg->violation_status);
		CAM_INFO(CAM_ISP, "TOP Violation status:0x%x", val_0);
	}

	core_info->tfe_bus->bottom_half_handler(
		core_info->tfe_bus->bus_priv, false, evt_payload, true);

	CAM_INFO(CAM_ISP,
		"TFE clock rate:%d TFE total bw applied:%lld",
		top_priv->hw_clk_rate,
		top_priv->total_bw_applied);
	cam_cpas_log_votes();
}

static int cam_tfe_error_irq_bottom_half(
	struct cam_tfe_hw_core_info          *core_info,
	struct cam_tfe_top_priv              *top_priv,
	struct cam_tfe_irq_evt_payload       *evt_payload,
	cam_hw_mgr_event_cb_func              event_cb,
	void                                 *event_cb_priv)
{
	struct cam_isp_hw_event_info         evt_info;
	struct cam_tfe_hw_info              *hw_info;
	uint32_t   error_detected = 0;

	hw_info = core_info->tfe_hw_info;
	evt_info.hw_idx = core_info->core_index;
	evt_info.res_type = CAM_ISP_RESOURCE_TFE_IN;

	if (evt_payload->irq_reg_val[0] & hw_info->error_irq_mask[0]) {
		evt_info.err_type = CAM_TFE_IRQ_STATUS_OVERFLOW;
		error_detected = 1;
	}

	if ((evt_payload->bus_irq_val[0] & hw_info->bus_error_irq_mask[0]) ||
		(evt_payload->irq_reg_val[2] & hw_info->error_irq_mask[2])) {
		evt_info.err_type = CAM_TFE_IRQ_STATUS_VIOLATION;
		error_detected = 1;
	}

	if (error_detected) {
		evt_info.err_type = CAM_TFE_IRQ_STATUS_OVERFLOW;
		top_priv->error_ts.tv_sec =
			evt_payload->ts.mono_time.tv_sec;
		top_priv->error_ts.tv_usec =
			evt_payload->ts.mono_time.tv_usec;

		cam_tfe_log_error_irq_status(core_info, top_priv, evt_payload);
		if (event_cb)
			event_cb(event_cb_priv,
				CAM_ISP_HW_EVENT_ERROR, (void *)&evt_info);
		else
			CAM_ERR(CAM_ISP, "TFE:%d invalid eventcb:",
				core_info->core_index);
	}

	return 0;
}

static int cam_tfe_rdi_irq_bottom_half(
	struct cam_tfe_top_priv              *top_priv,
	struct cam_isp_resource_node         *rdi_node,
	bool                                  epoch_process,
	struct cam_tfe_irq_evt_payload       *evt_payload)
{
	struct cam_tfe_rdi_data             *rdi_priv;
	struct cam_isp_hw_event_info         evt_info;
	struct cam_hw_info                  *hw_info;

	rdi_priv = (struct cam_tfe_rdi_data    *)rdi_node->res_priv;
	hw_info = rdi_node->hw_intf->hw_priv;

	evt_info.hw_idx   = rdi_node->hw_intf->hw_idx;
	evt_info.res_id   = rdi_node->res_id;
	evt_info.res_type = rdi_node->res_type;

	if ((!epoch_process) && (evt_payload->irq_reg_val[1] &
		rdi_priv->reg_data->eof_irq_mask)) {
		CAM_DBG(CAM_ISP, "Received EOF");
		top_priv->eof_ts.tv_sec =
			evt_payload->ts.mono_time.tv_sec;
		top_priv->eof_ts.tv_usec =
			evt_payload->ts.mono_time.tv_usec;

		if (rdi_priv->event_cb)
			rdi_priv->event_cb(rdi_priv->priv,
				CAM_ISP_HW_EVENT_EOF, (void *)&evt_info);
	}

	if ((!epoch_process) && (evt_payload->irq_reg_val[1] &
		rdi_priv->reg_data->sof_irq_mask)) {
		CAM_DBG(CAM_ISP, "Received SOF");
		top_priv->sof_ts.tv_sec =
			evt_payload->ts.mono_time.tv_sec;
		top_priv->sof_ts.tv_usec =
			evt_payload->ts.mono_time.tv_usec;

		if (rdi_priv->event_cb)
			rdi_priv->event_cb(rdi_priv->priv,
				CAM_ISP_HW_EVENT_SOF, (void *)&evt_info);
	}

	if (epoch_process && (evt_payload->irq_reg_val[1] &
		rdi_priv->reg_data->epoch0_irq_mask)) {
		CAM_DBG(CAM_ISP, "Received EPOCH0");
		top_priv->epoch_ts.tv_sec =
			evt_payload->ts.mono_time.tv_sec;
		top_priv->epoch_ts.tv_usec =
			evt_payload->ts.mono_time.tv_usec;

		if (rdi_priv->event_cb)
			rdi_priv->event_cb(rdi_priv->priv,
				CAM_ISP_HW_EVENT_EPOCH, (void *)&evt_info);
	}

	return 0;
}

static int cam_tfe_camif_irq_bottom_half(
	struct cam_tfe_top_priv              *top_priv,
	struct cam_isp_resource_node         *camif_node,
	bool                                  epoch_process,
	struct cam_tfe_irq_evt_payload       *evt_payload)
{
	struct cam_tfe_camif_data            *camif_priv;
	struct cam_isp_hw_event_info          evt_info;
	struct cam_hw_info                   *hw_info;
	uint32_t                              val;

	camif_priv = camif_node->res_priv;
	hw_info = camif_node->hw_intf->hw_priv;
	evt_info.hw_idx   = camif_node->hw_intf->hw_idx;
	evt_info.res_id   = camif_node->res_id;
	evt_info.res_type = camif_node->res_type;

	if ((!epoch_process) && (evt_payload->irq_reg_val[1] &
		camif_priv->reg_data->eof_irq_mask)) {
		CAM_DBG(CAM_ISP, "Received EOF");

		top_priv->eof_ts.tv_sec =
			evt_payload->ts.mono_time.tv_sec;
		top_priv->eof_ts.tv_usec =
			evt_payload->ts.mono_time.tv_usec;

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_EOF, (void *)&evt_info);
	}

	if ((!epoch_process) && (evt_payload->irq_reg_val[1] &
		camif_priv->reg_data->sof_irq_mask)) {
		if ((camif_priv->enable_sof_irq_debug) &&
			(camif_priv->irq_debug_cnt <=
			CAM_TFE_CAMIF_IRQ_SOF_DEBUG_CNT_MAX)) {
			CAM_INFO_RATE_LIMIT(CAM_ISP, "Received SOF");

			camif_priv->irq_debug_cnt++;
			if (camif_priv->irq_debug_cnt ==
				CAM_TFE_CAMIF_IRQ_SOF_DEBUG_CNT_MAX) {
				camif_priv->enable_sof_irq_debug =
					false;
				camif_priv->irq_debug_cnt = 0;
			}
		} else
			CAM_DBG(CAM_ISP, "Received SOF");

		top_priv->sof_ts.tv_sec =
			evt_payload->ts.mono_time.tv_sec;
		top_priv->sof_ts.tv_usec =
			evt_payload->ts.mono_time.tv_usec;

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_SOF, (void *)&evt_info);
	}

	if (epoch_process  && (evt_payload->irq_reg_val[1] &
		camif_priv->reg_data->epoch0_irq_mask)) {
		CAM_DBG(CAM_ISP, "Received EPOCH");

		top_priv->epoch_ts.tv_sec =
			evt_payload->ts.mono_time.tv_sec;
		top_priv->epoch_ts.tv_usec =
			evt_payload->ts.mono_time.tv_usec;

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_EPOCH, (void *)&evt_info);
	}

	if (camif_priv->camif_debug & CAMIF_DEBUG_ENABLE_SENSOR_DIAG_STATUS) {
		val = cam_io_r(camif_priv->mem_base +
			camif_priv->common_reg->diag_sensor_status_0);
		CAM_DBG(CAM_ISP, "TFE_DIAG_SENSOR_STATUS: 0x%x",
			camif_priv->mem_base, val);
	}

	return 0;
}

static int cam_tfe_irq_bottom_half(void *handler_priv,
	void *evt_payload_priv)
{
	struct cam_tfe_hw_core_info         *core_info;
	struct cam_tfe_top_priv             *top_priv;
	struct cam_tfe_irq_evt_payload      *evt_payload;
	struct cam_tfe_camif_data           *camif_priv;
	struct cam_tfe_rdi_data             *rdi_priv;
	cam_hw_mgr_event_cb_func             event_cb = NULL;
	void                                *event_cb_priv = NULL;
	uint32_t i;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP,
			"Invalid params handle_priv:%pK, evt_payload_priv:%pK",
			handler_priv, evt_payload_priv);
		return 0;
	}

	core_info = (struct cam_tfe_hw_core_info *)handler_priv;
	top_priv = (struct cam_tfe_top_priv  *)core_info->top_priv;
	evt_payload = evt_payload_priv;

	/* process sof and eof */
	for (i = 0; i < CAM_TFE_TOP_IN_PORT_MAX; i++) {
		if ((top_priv->in_rsrc[i].res_id ==
			CAM_ISP_HW_TFE_IN_CAMIF) &&
			(top_priv->in_rsrc[i].res_state ==
			CAM_ISP_RESOURCE_STATE_STREAMING)) {
			camif_priv = (struct cam_tfe_camif_data  *)
				top_priv->in_rsrc[i].res_priv;
			event_cb = camif_priv->event_cb;
			event_cb_priv = camif_priv->priv;

			if (camif_priv->reg_data->subscribe_irq_mask[1] &
				evt_payload->irq_reg_val[1])
				cam_tfe_camif_irq_bottom_half(top_priv,
					&top_priv->in_rsrc[i], false,
					evt_payload);

		} else if ((top_priv->in_rsrc[i].res_id >=
			CAM_ISP_HW_TFE_IN_RDI0) &&
			(top_priv->in_rsrc[i].res_id <=
			CAM_ISP_HW_TFE_IN_RDI2) &&
			(top_priv->in_rsrc[i].res_state ==
			CAM_ISP_RESOURCE_STATE_STREAMING) &&
			top_priv->in_rsrc[i].rdi_only_ctx) {
			rdi_priv = (struct cam_tfe_rdi_data *)
				top_priv->in_rsrc[i].res_priv;
			event_cb = rdi_priv->event_cb;
			event_cb_priv = rdi_priv->priv;

			if (rdi_priv->reg_data->subscribe_irq_mask[1] &
				evt_payload->irq_reg_val[1])
				cam_tfe_rdi_irq_bottom_half(top_priv,
					&top_priv->in_rsrc[i], false,
					evt_payload);
		}
	}

	/* process the irq errors */
	cam_tfe_error_irq_bottom_half(core_info, top_priv, evt_payload,
		event_cb, event_cb_priv);

	/* process the reg update in the bus */
	if (evt_payload->irq_reg_val[0] &
		core_info->tfe_hw_info->bus_reg_irq_mask[0]) {
		core_info->tfe_bus->bottom_half_handler(
			core_info->tfe_bus->bus_priv, true, evt_payload, false);
	}

	/* process the epoch */
	for (i = 0; i < CAM_TFE_TOP_IN_PORT_MAX; i++) {
		if ((top_priv->in_rsrc[i].res_id ==
			CAM_ISP_HW_TFE_IN_CAMIF) &&
			(top_priv->in_rsrc[i].res_state ==
			CAM_ISP_RESOURCE_STATE_STREAMING)) {
			camif_priv = (struct cam_tfe_camif_data  *)
				top_priv->in_rsrc[i].res_priv;
			if (camif_priv->reg_data->subscribe_irq_mask[1] &
				evt_payload->irq_reg_val[1])
				cam_tfe_camif_irq_bottom_half(top_priv,
					&top_priv->in_rsrc[i], true,
					evt_payload);
		} else if ((top_priv->in_rsrc[i].res_id >=
			CAM_ISP_HW_TFE_IN_RDI0) &&
			(top_priv->in_rsrc[i].res_id <=
			CAM_ISP_HW_TFE_IN_RDI2) &&
			(top_priv->in_rsrc[i].res_state ==
			CAM_ISP_RESOURCE_STATE_STREAMING)) {
			rdi_priv = (struct cam_tfe_rdi_data *)
				top_priv->in_rsrc[i].res_priv;
			if (rdi_priv->reg_data->subscribe_irq_mask[1] &
				evt_payload->irq_reg_val[1])
				cam_tfe_rdi_irq_bottom_half(top_priv,
					&top_priv->in_rsrc[i], true,
					evt_payload);
		}
	}

	/* process the bufone */
	if (evt_payload->irq_reg_val[0] &
		core_info->tfe_hw_info->bus_reg_irq_mask[0]) {
		core_info->tfe_bus->bottom_half_handler(
			core_info->tfe_bus->bus_priv, false, evt_payload,
			false);
	}

	cam_tfe_put_evt_payload(core_info, &evt_payload);

	return 0;
}

static int cam_tfe_irq_err_top_half(
	struct cam_tfe_hw_core_info       *core_info,
	void __iomem                      *mem_base,
	uint32_t                          *top_irq_status,
	uint32_t                          *bus_irq_status)
{
	uint32_t i;

	if ((top_irq_status[0] &  core_info->tfe_hw_info->error_irq_mask[0]) ||
		(top_irq_status[2] &
		core_info->tfe_hw_info->error_irq_mask[2]) ||
		(bus_irq_status[0] &
		core_info->tfe_hw_info->bus_error_irq_mask[0])) {
		CAM_ERR(CAM_ISP,
			"Encountered Error: tfe:%d: Irq_status0=0x%x status2=0x%x",
			core_info->core_index, top_irq_status[0],
			top_irq_status[2]);
		CAM_ERR(CAM_ISP,
			"Encountered Error: tfe:%d:BUS Irq_status0=0x%x",
			core_info->core_index, bus_irq_status[0]);

		for (i = 0; i < CAM_TFE_TOP_IRQ_REG_NUM; i++)
			cam_io_w(0, mem_base +
				core_info->tfe_hw_info->top_irq_mask[i]);

		cam_io_w_mb(core_info->tfe_hw_info->global_clear_bitmask,
			mem_base + core_info->tfe_hw_info->top_irq_cmd);
	}

	return 0;
}

irqreturn_t cam_tfe_irq(int irq_num, void *data)
{
	struct cam_hw_info             *tfe_hw;
	struct cam_tfe_hw_core_info    *core_info;
	struct cam_tfe_top_priv        *top_priv;
	void __iomem                   *mem_base;
	struct cam_tfe_irq_evt_payload  *evt_payload;
	uint32_t  top_irq_status[CAM_TFE_TOP_IRQ_REG_NUM] = {0};
	uint32_t   bus_irq_status[CAM_TFE_BUS_MAX_IRQ_REGISTERS] = {0};
	uint32_t  i,  ccif_violation = 0, overflow_status = 0;
	uint32_t    image_sz_violation = 0;
	void        *bh_cmd = NULL;
	int rc = -EINVAL;

	if (!data)
		return IRQ_NONE;

	tfe_hw = (struct cam_hw_info *)data;
	core_info = (struct cam_tfe_hw_core_info *)tfe_hw->core_info;
	top_priv = (struct cam_tfe_top_priv  *)core_info->top_priv;
	mem_base = top_priv->common_data.soc_info->reg_map[0].mem_base;

	if (tfe_hw->hw_state == CAM_HW_STATE_POWER_DOWN) {
		CAM_ERR(CAM_ISP, "TFE:%d hw is not powered up",
			core_info->core_index);
		return IRQ_HANDLED;
	}

	spin_lock(&core_info->spin_lock);
	for (i = 0; i < CAM_TFE_TOP_IRQ_REG_NUM; i++)
		top_irq_status[i] = cam_io_r(mem_base  +
		core_info->tfe_hw_info->top_irq_status[i]);

	for (i = 0; i < CAM_TFE_TOP_IRQ_REG_NUM; i++)
		cam_io_w(top_irq_status[i], mem_base +
			core_info->tfe_hw_info->top_irq_clear[i]);

	cam_io_w_mb(core_info->tfe_hw_info->global_clear_bitmask,
		mem_base + core_info->tfe_hw_info->top_irq_cmd);

	CAM_DBG(CAM_ISP, "TFE:%d IRQ status_0:0x%x status_1:0x%x status_2:0x%x",
		core_info->core_index, top_irq_status[0],
		top_irq_status[1], top_irq_status[2]);

	if (top_irq_status[0] & core_info->tfe_hw_info->bus_reg_irq_mask[0]) {
		for (i = 0; i < CAM_TFE_BUS_MAX_IRQ_REGISTERS; i++)
			bus_irq_status[i] = cam_io_r(mem_base  +
				core_info->tfe_hw_info->bus_irq_status[i]);

		for (i = 0; i < CAM_TFE_BUS_MAX_IRQ_REGISTERS; i++)
			cam_io_w(bus_irq_status[i], mem_base +
				core_info->tfe_hw_info->bus_irq_clear[i]);

		ccif_violation =  cam_io_r(mem_base  +
			core_info->tfe_hw_info->bus_violation_reg);
		overflow_status = cam_io_r(mem_base  +
			core_info->tfe_hw_info->bus_overflow_reg);
		image_sz_violation = cam_io_r(mem_base  +
			core_info->tfe_hw_info->bus_image_size_vilation_reg);

		cam_io_w(core_info->tfe_hw_info->global_clear_bitmask,
			mem_base + core_info->tfe_hw_info->bus_irq_cmd);

		CAM_DBG(CAM_ISP, "TFE:%d BUS IRQ status_0:0x%x status_1:0x%x",
			core_info->core_index, bus_irq_status[0],
			bus_irq_status[1]);
	}
	spin_unlock(&core_info->spin_lock);

	/* check reset */
	if ((top_irq_status[0] & core_info->tfe_hw_info->reset_irq_mask[0]) ||
		(top_irq_status[1] &
			core_info->tfe_hw_info->reset_irq_mask[1]) ||
		(top_irq_status[2] &
			core_info->tfe_hw_info->reset_irq_mask[2])) {
		/* Reset ack */
		complete(&core_info->reset_complete);
		return IRQ_HANDLED;
	}

	/* Check the irq errors  */
	cam_tfe_irq_err_top_half(core_info, mem_base, top_irq_status,
		bus_irq_status);

	rc  = cam_tfe_get_evt_payload(core_info, &evt_payload);
	if (rc) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"No tasklet_cmd is free in queue");
		CAM_ERR_RATE_LIMIT(CAM_ISP, "IRQ status0=0x%x status2=0x%x",
			top_irq_status[0], top_irq_status[1]);
		goto end;
	}

	cam_tfe_get_timestamp(&evt_payload->ts);

	for (i = 0; i < CAM_TFE_TOP_IRQ_REG_NUM; i++)
		evt_payload->irq_reg_val[i] = top_irq_status[i];

	for (i = 0; i < CAM_TFE_BUS_MAX_IRQ_REGISTERS; i++)
		evt_payload->bus_irq_val[i] = bus_irq_status[i];

	evt_payload->ccif_violation_status = ccif_violation;
	evt_payload->overflow_status = overflow_status;
	evt_payload->image_size_violation_status = image_sz_violation;

	evt_payload->core_index = core_info->core_index;
	evt_payload->core_info  = core_info;

	rc = tasklet_bh_api.get_bh_payload_func(
		top_priv->tasklet_info, &bh_cmd);
	if (rc || !bh_cmd) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"No payload, IRQ handling frozen");
		cam_tfe_put_evt_payload(core_info, &evt_payload);
		goto end;
	}

	tasklet_bh_api.bottom_half_enqueue_func(
		top_priv->tasklet_info,
		bh_cmd,
		core_info,
		evt_payload,
		cam_tfe_irq_bottom_half);

end:
	return IRQ_HANDLED;

}

static int cam_tfe_top_set_hw_clk_rate(
	struct cam_tfe_top_priv *top_priv)
{
	struct cam_hw_soc_info        *soc_info = NULL;
	int                            i, rc = 0;
	unsigned long                  max_clk_rate = 0;

	soc_info = top_priv->common_data.soc_info;

	for (i = 0; i < CAM_TFE_TOP_IN_PORT_MAX; i++) {
		if (top_priv->req_clk_rate[i] > max_clk_rate)
			max_clk_rate = top_priv->req_clk_rate[i];
	}
	if (max_clk_rate == top_priv->hw_clk_rate)
		return 0;

	CAM_DBG(CAM_ISP, "TFE:%d Clock name=%s idx=%d clk=%llu",
		top_priv->common_data.soc_info->index,
		soc_info->clk_name[soc_info->src_clk_idx],
		soc_info->src_clk_idx, max_clk_rate);

	rc = cam_soc_util_set_src_clk_rate(soc_info, max_clk_rate);

	if (!rc)
		top_priv->hw_clk_rate = max_clk_rate;
	else
		CAM_ERR(CAM_ISP, "TFE:%d set src clock rate:%lld failed, rc=%d",
		top_priv->common_data.soc_info->index, max_clk_rate,  rc);

	return rc;
}

static struct cam_axi_vote *cam_tfe_top_delay_bw_reduction(
	struct cam_tfe_top_priv *top_priv,
	uint64_t *to_be_applied_bw)
{
	uint32_t i, j;
	int vote_idx = -1;
	uint64_t max_bw = 0;
	uint64_t total_bw;
	struct cam_axi_vote *curr_l_vote;

	for (i = 0; i < (CAM_TFE_TOP_IN_PORT_MAX *
		CAM_TFE_DELAY_BW_REDUCTION_NUM_FRAMES); i++) {
		total_bw = 0;
		curr_l_vote = &top_priv->last_vote[i];
		for (j = 0; j < curr_l_vote->num_paths; j++) {
			if (total_bw >
				(U64_MAX -
				curr_l_vote->axi_path[j].camnoc_bw)) {
				CAM_ERR(CAM_ISP, "Overflow at idx: %d", j);
				return NULL;
			}

			total_bw += curr_l_vote->axi_path[j].camnoc_bw;
		}

		if (total_bw > max_bw) {
			vote_idx = i;
			max_bw = total_bw;
		}
	}

	if (vote_idx < 0)
		return NULL;

	*to_be_applied_bw = max_bw;

	return &top_priv->last_vote[vote_idx];
}

static int cam_tfe_top_set_axi_bw_vote(
	struct cam_tfe_top_priv *top_priv,
	bool start_stop)
{
	struct cam_axi_vote *agg_vote = NULL;
	struct cam_axi_vote *to_be_applied_axi_vote = NULL;
	struct cam_hw_soc_info   *soc_info = top_priv->common_data.soc_info;
	struct cam_tfe_soc_private *soc_private = soc_info->soc_private;
	int rc = 0;
	uint32_t i;
	uint32_t num_paths = 0;
	uint64_t total_bw_new_vote = 0;
	bool bw_unchanged = true;
	bool apply_bw_update = false;

	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Error soc_private NULL");
		return -EINVAL;
	}

	agg_vote = kzalloc(sizeof(struct cam_axi_vote), GFP_KERNEL);
	if (!agg_vote) {
		CAM_ERR(CAM_ISP, "Out of memory");
		return -ENOMEM;
	}

	for (i = 0; i < CAM_TFE_TOP_IN_PORT_MAX; i++) {
		if (top_priv->axi_vote_control[i] ==
			CAM_TFE_BW_CONTROL_INCLUDE) {
			if (num_paths +
				top_priv->req_axi_vote[i].num_paths >
				CAM_CPAS_MAX_PATHS_PER_CLIENT) {
				CAM_ERR(CAM_ISP,
					"Required paths(%d) more than max(%d)",
					num_paths +
					top_priv->req_axi_vote[i].num_paths,
					CAM_CPAS_MAX_PATHS_PER_CLIENT);
				rc = -EINVAL;
				goto free_mem;
			}

			memcpy(&agg_vote->axi_path[num_paths],
				&top_priv->req_axi_vote[i].axi_path[0],
				top_priv->req_axi_vote[i].num_paths *
				sizeof(
				struct cam_axi_per_path_bw_vote));
			num_paths += top_priv->req_axi_vote[i].num_paths;
		}
	}

	agg_vote->num_paths = num_paths;

	for (i = 0; i < agg_vote->num_paths; i++) {
		CAM_DBG(CAM_PERF,
			"tfe[%d] : New BW Vote : counter[%d] [%s][%s] [%llu %llu %llu]",
			top_priv->common_data.hw_intf->hw_idx,
			top_priv->last_counter,
			cam_cpas_axi_util_path_type_to_string(
			agg_vote->axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			agg_vote->axi_path[i].transac_type),
			agg_vote->axi_path[i].camnoc_bw,
			agg_vote->axi_path[i].mnoc_ab_bw,
			agg_vote->axi_path[i].mnoc_ib_bw);

		total_bw_new_vote += agg_vote->axi_path[i].camnoc_bw;
	}

	memcpy(&top_priv->last_vote[top_priv->last_counter], agg_vote,
		sizeof(struct cam_axi_vote));
	top_priv->last_counter = (top_priv->last_counter + 1) %
		(CAM_TFE_TOP_IN_PORT_MAX *
		CAM_TFE_DELAY_BW_REDUCTION_NUM_FRAMES);

	if ((agg_vote->num_paths != top_priv->applied_axi_vote.num_paths) ||
		(total_bw_new_vote != top_priv->total_bw_applied))
		bw_unchanged = false;

	CAM_DBG(CAM_PERF,
		"tfe[%d] : applied_total=%lld, new_total=%lld unchanged=%d, start_stop=%d",
		top_priv->common_data.hw_intf->hw_idx,
		top_priv->total_bw_applied, total_bw_new_vote,
		bw_unchanged, start_stop);

	if (bw_unchanged) {
		CAM_DBG(CAM_ISP, "BW config unchanged");
		rc = 0;
		goto free_mem;
	}

	if (start_stop) {
		/* need to vote current request immediately */
		to_be_applied_axi_vote = agg_vote;
		/* Reset everything, we can start afresh */
		memset(top_priv->last_vote, 0x0, sizeof(struct cam_axi_vote) *
			(CAM_TFE_TOP_IN_PORT_MAX *
			CAM_TFE_DELAY_BW_REDUCTION_NUM_FRAMES));
		top_priv->last_counter = 0;
		top_priv->last_vote[top_priv->last_counter] = *agg_vote;
		top_priv->last_counter = (top_priv->last_counter + 1) %
			(CAM_TFE_TOP_IN_PORT_MAX *
			CAM_TFE_DELAY_BW_REDUCTION_NUM_FRAMES);
	} else {
		/*
		 * Find max bw request in last few frames. This will the bw
		 * that we want to vote to CPAS now.
		 */
		to_be_applied_axi_vote =
			cam_tfe_top_delay_bw_reduction(top_priv,
			&total_bw_new_vote);
		if (!to_be_applied_axi_vote) {
			CAM_ERR(CAM_ISP, "to_be_applied_axi_vote is NULL");
			rc = -EINVAL;
			goto free_mem;
		}
	}

	for (i = 0; i < to_be_applied_axi_vote->num_paths; i++) {
		CAM_DBG(CAM_PERF,
			"tfe[%d] : Apply BW Vote : [%s][%s] [%llu %llu %llu]",
			top_priv->common_data.hw_intf->hw_idx,
			cam_cpas_axi_util_path_type_to_string(
			to_be_applied_axi_vote->axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			to_be_applied_axi_vote->axi_path[i].transac_type),
			to_be_applied_axi_vote->axi_path[i].camnoc_bw,
			to_be_applied_axi_vote->axi_path[i].mnoc_ab_bw,
			to_be_applied_axi_vote->axi_path[i].mnoc_ib_bw);
	}

	if ((to_be_applied_axi_vote->num_paths !=
		top_priv->applied_axi_vote.num_paths) ||
		(total_bw_new_vote != top_priv->total_bw_applied))
		apply_bw_update = true;

	CAM_DBG(CAM_PERF,
		"tfe[%d] : Delayed update: applied_total=%lld, new_total=%lld apply_bw_update=%d, start_stop=%d",
		top_priv->common_data.hw_intf->hw_idx,
		top_priv->total_bw_applied, total_bw_new_vote,
		apply_bw_update, start_stop);

	if (apply_bw_update) {
		rc = cam_cpas_update_axi_vote(soc_private->cpas_handle,
			to_be_applied_axi_vote);
		if (!rc) {
			memcpy(&top_priv->applied_axi_vote,
				to_be_applied_axi_vote,
				sizeof(struct cam_axi_vote));
			top_priv->total_bw_applied = total_bw_new_vote;
		} else {
			CAM_ERR(CAM_ISP, "BW request failed, rc=%d", rc);
		}
	}

free_mem:
	kzfree(agg_vote);
	agg_vote = NULL;
	return rc;
}

static int cam_tfe_top_get_base(struct cam_tfe_top_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	uint32_t                          size = 0;
	uint32_t                          mem_base = 0;
	struct cam_isp_hw_get_cmd_update *cdm_args  = cmd_args;
	struct cam_cdm_utils_ops         *cdm_util_ops = NULL;

	if (arg_size != sizeof(struct cam_isp_hw_get_cmd_update)) {
		CAM_ERR(CAM_ISP, "Error Invalid cmd size");
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res || !top_priv ||
		!top_priv->common_data.soc_info) {
		CAM_ERR(CAM_ISP, "Error Invalid args");
		return -EINVAL;
	}

	cdm_util_ops =
		(struct cam_cdm_utils_ops *)cdm_args->res->cdm_ops;

	if (!cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid CDM ops");
		return -EINVAL;
	}

	size = cdm_util_ops->cdm_required_size_changebase();
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((size * 4) > cdm_args->cmd.size) {
		CAM_ERR(CAM_ISP, "buf size:%d is not sufficient, expected: %d",
			cdm_args->cmd.size, size);
		return -EINVAL;
	}

	mem_base = CAM_SOC_GET_REG_MAP_CAM_BASE(
		top_priv->common_data.soc_info, TFE_CORE_BASE_IDX);

	cdm_util_ops->cdm_write_changebase(
	cdm_args->cmd.cmd_buf_addr, mem_base);
	cdm_args->cmd.used_bytes = (size * 4);

	return 0;
}

static int cam_tfe_top_get_reg_update(
	struct cam_tfe_top_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	uint32_t                          size = 0;
	uint32_t                          reg_val_pair[2];
	struct cam_isp_hw_get_cmd_update *cdm_args = cmd_args;
	struct cam_cdm_utils_ops         *cdm_util_ops = NULL;
	struct cam_tfe_camif_data        *camif_rsrc_data = NULL;
	struct cam_tfe_rdi_data          *rdi_rsrc_data = NULL;
	struct cam_isp_resource_node     *in_res;

	if (arg_size != sizeof(struct cam_isp_hw_get_cmd_update)) {
		CAM_ERR(CAM_ISP, "Invalid cmd size");
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}

	cdm_util_ops = (struct cam_cdm_utils_ops *)cdm_args->res->cdm_ops;

	if (!cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid CDM ops");
		return -EINVAL;
	}

	in_res = cdm_args->res;
	size = cdm_util_ops->cdm_required_size_reg_random(1);
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((size * 4) > cdm_args->cmd.size) {
		CAM_ERR(CAM_ISP, "buf size:%d is not sufficient, expected: %d",
			cdm_args->cmd.size, size);
		return -EINVAL;
	}

	if (in_res->res_id == CAM_ISP_HW_TFE_IN_CAMIF) {
		camif_rsrc_data =  in_res->res_priv;
		reg_val_pair[0] = camif_rsrc_data->camif_reg->reg_update_cmd;
		reg_val_pair[1] =
			camif_rsrc_data->reg_data->reg_update_cmd_data;
	} else if ((in_res->res_id >= CAM_ISP_HW_TFE_IN_RDI0) &&
		(in_res->res_id <= CAM_ISP_HW_TFE_IN_RDI2)) {
		rdi_rsrc_data =  in_res->res_priv;
		reg_val_pair[0] = rdi_rsrc_data->rdi_reg->reg_update_cmd;
		reg_val_pair[1] = rdi_rsrc_data->reg_data->reg_update_cmd_data;
	}

	cdm_util_ops->cdm_write_regrandom(cdm_args->cmd.cmd_buf_addr,
		1, reg_val_pair);

	cdm_args->cmd.used_bytes = size * 4;

	return 0;
}

static int cam_tfe_top_clock_update(
	struct cam_tfe_top_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_tfe_clock_update_args     *clk_update = NULL;
	struct cam_isp_resource_node         *res = NULL;
	struct cam_hw_info                   *hw_info = NULL;
	int                                   i, rc = 0;

	clk_update =
		(struct cam_tfe_clock_update_args *)cmd_args;
	res = clk_update->node_res;

	if (!res || !res->hw_intf->hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid input res %pK", res);
		return -EINVAL;
	}

	hw_info = res->hw_intf->hw_priv;

	if (res->res_type != CAM_ISP_RESOURCE_TFE_IN ||
		res->res_id >= CAM_ISP_HW_TFE_IN_MAX) {
		CAM_ERR(CAM_ISP, "TFE:%d Invalid res_type:%d res id%d",
			res->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	for (i = 0; i < CAM_TFE_TOP_IN_PORT_MAX; i++) {
		if (top_priv->in_rsrc[i].res_id == res->res_id) {
			top_priv->req_clk_rate[i] = clk_update->clk_rate;
			break;
		}
	}

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_DBG(CAM_ISP,
			"TFE:%d Not ready to set clocks yet :%d",
			res->hw_intf->hw_idx,
			hw_info->hw_state);
	} else
		rc = cam_tfe_top_set_hw_clk_rate(top_priv);

	return rc;
}

static int cam_tfe_top_bw_update(
	struct cam_tfe_top_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_tfe_bw_update_args        *bw_update = NULL;
	struct cam_isp_resource_node         *res = NULL;
	struct cam_hw_info                   *hw_info = NULL;
	int                                   rc = 0;
	int                                   i;

	bw_update = (struct cam_tfe_bw_update_args *)cmd_args;
	res = bw_update->node_res;

	if (!res || !res->hw_intf || !res->hw_intf->hw_priv)
		return -EINVAL;

	hw_info = res->hw_intf->hw_priv;

	if (res->res_type != CAM_ISP_RESOURCE_TFE_IN ||
		res->res_id >= CAM_ISP_HW_TFE_IN_MAX) {
		CAM_ERR(CAM_ISP, "TFE:%d Invalid res_type:%d res id%d",
			res->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	for (i = 0; i < CAM_ISP_HW_TFE_IN_MAX; i++) {
		if (top_priv->in_rsrc[i].res_id == res->res_id) {
			memcpy(&top_priv->req_axi_vote[i], &bw_update->isp_vote,
				sizeof(struct cam_axi_vote));
			top_priv->axi_vote_control[i] =
				CAM_TFE_BW_CONTROL_INCLUDE;
			break;
		}
	}

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"TFE:%d Not ready to set BW yet :%d",
			res->hw_intf->hw_idx,
			hw_info->hw_state);
	} else {
		rc = cam_tfe_top_set_axi_bw_vote(top_priv, false);
	}

	return rc;
}

static int cam_tfe_top_bw_control(
	struct cam_tfe_top_priv *top_priv,
	 void *cmd_args, uint32_t arg_size)
{
	struct cam_tfe_bw_control_args       *bw_ctrl = NULL;
	struct cam_isp_resource_node         *res = NULL;
	struct cam_hw_info                   *hw_info = NULL;
	int                                   rc = 0;
	int                                   i;

	bw_ctrl = (struct cam_tfe_bw_control_args *)cmd_args;
	res = bw_ctrl->node_res;

	if (!res || !res->hw_intf->hw_priv)
		return -EINVAL;

	hw_info = res->hw_intf->hw_priv;

	if (res->res_type != CAM_ISP_RESOURCE_TFE_IN ||
		res->res_id >= CAM_ISP_HW_TFE_IN_MAX) {
		CAM_ERR(CAM_ISP, "TFE:%d Invalid res_type:%d res id%d",
			res->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	for (i = 0; i < CAM_TFE_TOP_IN_PORT_MAX; i++) {
		if (top_priv->in_rsrc[i].res_id == res->res_id) {
			top_priv->axi_vote_control[i] = bw_ctrl->action;
			break;
		}
	}

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"TFE:%d Not ready to set BW yet :%d",
			res->hw_intf->hw_idx,
			hw_info->hw_state);
	} else {
		rc = cam_tfe_top_set_axi_bw_vote(top_priv, true);
	}

	return rc;
}

static int cam_tfe_top_get_reg_dump(
	struct cam_tfe_top_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_isp_hw_get_cmd_update  *reg_dump_cmd = cmd_args;
	struct cam_tfe_soc_private        *soc_private;
	struct cam_tfe_reg_dump_data      *reg_dump_data;
	struct cam_hw_soc_info            *soc_info;
	void __iomem                      *mem_base;
	int i, j, num_reg_dump_entries;
	uint32_t val_0, val_1, val_2, val_3, wm_offset, start_offset;
	uint32_t end_offset, lut_word_size, lut_size, lut_bank_sel, lut_dmi_reg;

	if (!reg_dump_cmd) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	if ((reg_dump_cmd->res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) ||
		(reg_dump_cmd->res->res_state ==
		CAM_ISP_RESOURCE_STATE_AVAILABLE))
		return 0;

	soc_info = top_priv->common_data.soc_info;
	soc_private = top_priv->common_data.soc_info->soc_private;
	mem_base = soc_info->reg_map[TFE_CORE_BASE_IDX].mem_base;
	CAM_INFO(CAM_ISP, "dump tfe:%d registers",
		top_priv->common_data.hw_intf->hw_idx);

	reg_dump_data = top_priv->common_data.reg_dump_data;
	num_reg_dump_entries = reg_dump_data->num_reg_dump_entries;
	for (i = 0; i < num_reg_dump_entries; i++) {
		start_offset = reg_dump_data->reg_entry[i].start_offset;
		end_offset = reg_dump_data->reg_entry[i].end_offset;

		for (j = start_offset; (j + 0xc) <= end_offset; j += 0x10) {
			val_0 = cam_io_r_mb(mem_base + j);
			val_1 = cam_io_r_mb(mem_base + j + 4);
			val_2 = cam_io_r_mb(mem_base + j + 0x8);
			val_3 = cam_io_r_mb(mem_base + j + 0xc);
			CAM_INFO(CAM_ISP, "0x%04x=0x%08x 0x%08x 0x%08x 0x%08x",
				j, val_0, val_1, val_2, val_3);
		}
	}

	num_reg_dump_entries = reg_dump_data->num_lut_dump_entries;
	for (i = 0; i < num_reg_dump_entries; i++) {
		lut_bank_sel = reg_dump_data->lut_entry[i].lut_bank_sel;
		lut_size = reg_dump_data->lut_entry[i].lut_addr_size;
		lut_word_size = reg_dump_data->lut_entry[i].lut_word_size;
		lut_dmi_reg = reg_dump_data->lut_entry[i].dmi_reg_offset;

		cam_io_w_mb(lut_bank_sel, mem_base + lut_dmi_reg + 4);
		cam_io_w_mb(0, mem_base + 0xC28);

		for (j = 0; j < lut_size; j++) {
			val_0 = cam_io_r_mb(mem_base + 0xC30);
			CAM_INFO(CAM_ISP, "Bank%d:0x%x LO: 0x%x",
				lut_bank_sel, j, val_0);
		}
	}
	/* No mem selected */
	cam_io_w_mb(0, mem_base + 0xC24);
	cam_io_w_mb(0, mem_base + 0xC28);

	start_offset = reg_dump_data->bus_start_addr;
	end_offset = reg_dump_data->bus_write_top_end_addr;

	CAM_INFO(CAM_ISP, "bus start addr:0x%x end_offset:0x%x",
		start_offset, end_offset);

	for (i = start_offset; (i + 0xc) <= end_offset; i += 0x10) {
		val_0 = cam_io_r_mb(mem_base + i);
		val_1 = cam_io_r_mb(mem_base + i + 4);
		val_2 = cam_io_r_mb(mem_base + i + 0x8);
		val_3 = cam_io_r_mb(mem_base + i + 0xc);
		CAM_INFO(CAM_ISP, "0x%04x=0x%08x 0x%08x 0x%08x 0x%08x",
			i, val_0, val_1, val_2, val_3);
	}

	wm_offset = reg_dump_data->bus_client_start_addr;

	CAM_INFO(CAM_ISP, "bus wm offset:0x%x",
		wm_offset);

	for (j = 0; j < reg_dump_data->num_bus_clients; j++) {
		for (i = 0x0; (i + 0xc) <= 0x3C; i += 0x10) {
			val_0 = cam_io_r_mb(mem_base + wm_offset + i);
			val_1 = cam_io_r_mb(mem_base + wm_offset + i + 4);
			val_2 = cam_io_r_mb(mem_base + wm_offset + i + 0x8);
			val_3 = cam_io_r_mb(mem_base + wm_offset + i + 0xc);
			CAM_INFO(CAM_ISP, "0x%04x=0x%08x 0x%08x 0x%08x 0x%08x",
				(wm_offset + i), val_0, val_1, val_2, val_3);
		}
		for (i = 0x60; (i + 0xc) <= 0x80; i += 0x10) {
			val_0 = cam_io_r_mb(mem_base + wm_offset + i);
			val_1 = cam_io_r_mb(mem_base + wm_offset + i + 4);
			val_2 = cam_io_r_mb(mem_base + wm_offset + i + 0x8);
			val_3 = cam_io_r_mb(mem_base + wm_offset + i + 0xc);
			CAM_INFO(CAM_ISP, "0x%04x=0x%08x 0x%08x 0x%08x 0x%08x",
				(wm_offset + i), val_0, val_1, val_2, val_3);
		}
		wm_offset += reg_dump_data->bus_client_offset;
	}

	cam_cpas_reg_read(soc_private->cpas_handle,
		CAM_CPAS_REG_CAMNOC, 0x20, true, &val_0);
	CAM_INFO(CAM_ISP, "tfe_niu_MaxWr_Low offset 0x20 val 0x%x",
		val_0);

	/* dump the clock votings */
	CAM_INFO(CAM_ISP, "TFE:%d clk=%ld",
		top_priv->common_data.hw_intf->hw_idx,
		top_priv->hw_clk_rate);

	return 0;
}

static int cam_tfe_hw_dump(
	struct cam_tfe_hw_core_info *core_info,
	void                        *cmd_args,
	uint32_t                     arg_size)
{
	int                                i, j;
	uint8_t                           *dst;
	uint32_t                           reg_start_offset;
	uint32_t                           reg_dump_size = 0;
	uint32_t                           lut_dump_size = 0;
	uint32_t                           num_lut_dump_entries = 0;
	uint32_t                           num_reg;
	uint32_t                           lut_word_size, lut_size;
	uint32_t                           lut_bank_sel, lut_dmi_reg;
	uint32_t                           val;
	void __iomem                      *reg_base;
	void __iomem                      *mem_base;
	uint32_t                          *addr, *start;
	uint64_t                          *clk_waddr, *clk_wstart;
	size_t                             remain_len;
	uint32_t                           min_len;
	struct cam_hw_info                *tfe_hw_info;
	struct cam_hw_soc_info            *soc_info;
	struct cam_tfe_top_priv           *top_priv;
	struct cam_tfe_soc_private        *soc_private;
	struct cam_tfe_reg_dump_data      *reg_dump_data;
	struct cam_isp_hw_dump_header     *hdr;
	struct cam_isp_hw_dump_args       *dump_args =
		(struct cam_isp_hw_dump_args *)cmd_args;

	if (!dump_args || !core_info) {
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

	top_priv = (struct cam_tfe_top_priv  *)core_info->top_priv;
	tfe_hw_info =
		(struct cam_hw_info *)(top_priv->common_data.hw_intf->hw_priv);
	reg_dump_data = top_priv->common_data.reg_dump_data;
	soc_info = top_priv->common_data.soc_info;
	soc_private = top_priv->common_data.soc_info->soc_private;
	mem_base = soc_info->reg_map[TFE_CORE_BASE_IDX].mem_base;

	if (dump_args->is_dump_all) {

		/*Dump registers size*/
		for (i = 0; i < reg_dump_data->num_reg_dump_entries; i++)
			reg_dump_size +=
				(reg_dump_data->reg_entry[i].end_offset -
				reg_dump_data->reg_entry[i].start_offset);

		/*
		 * We dump the offset as well, so the total size dumped becomes
		 * multiplied by 2
		 */
		reg_dump_size *= 2;

		/* LUT dump size */
		for (i = 0; i < reg_dump_data->num_lut_dump_entries; i++)
			lut_dump_size +=
				((reg_dump_data->lut_entry[i].lut_addr_size) *
				(reg_dump_data->lut_entry[i].lut_word_size/8));

		num_lut_dump_entries = reg_dump_data->num_lut_dump_entries;
	}

	/*Minimum len comprises of:
	 * lut_dump_size + reg_dump_size + sizeof dump_header +
	 * (num_lut_dump_entries--> represents number of banks) +
	 *  (misc number of words) * sizeof(uint32_t)
	 */
	min_len = lut_dump_size + reg_dump_size +
		sizeof(struct cam_isp_hw_dump_header) +
		(num_lut_dump_entries * sizeof(uint32_t)) +
		(sizeof(uint32_t) * CAM_TFE_CORE_DUMP_MISC_NUM_WORDS);

	remain_len = dump_args->buf_len - dump_args->offset;
	if (remain_len < min_len) {
		CAM_WARN(CAM_ISP, "Dump buffer exhaust remain %zu, min %u",
			remain_len, min_len);
		return -ENOSPC;
	}

	mutex_lock(&tfe_hw_info->hw_mutex);
	if (tfe_hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "TFE:%d HW not powered up",
			core_info->core_index);
		mutex_unlock(&tfe_hw_info->hw_mutex);
		return -EPERM;
	}

	if (!dump_args->is_dump_all)
		goto dump_bw;

	dst = (uint8_t *)dump_args->cpu_addr + dump_args->offset;
	hdr = (struct cam_isp_hw_dump_header *)dst;
	hdr->word_size = sizeof(uint32_t);
	scnprintf(hdr->tag, CAM_ISP_HW_DUMP_TAG_MAX_LEN, "TFE_REG:");
	addr = (uint32_t *)(dst + sizeof(struct cam_isp_hw_dump_header));
	start = addr;
	*addr++ = soc_info->index;
	for (i = 0; i < reg_dump_data->num_reg_dump_entries; i++) {
		num_reg  = (reg_dump_data->reg_entry[i].end_offset -
			reg_dump_data->reg_entry[i].start_offset)/4;
		reg_start_offset = reg_dump_data->reg_entry[i].start_offset;
		reg_base = mem_base + reg_start_offset;
		for (j = 0; j < num_reg; j++) {
			addr[0] =
				soc_info->mem_block[TFE_CORE_BASE_IDX]->start +
				reg_start_offset + (j*4);
			addr[1] = cam_io_r(reg_base + (j*4));
			addr += 2;
		}
	}

	/*Dump bus top registers*/
	num_reg  = (reg_dump_data->bus_write_top_end_addr -
			reg_dump_data->bus_start_addr)/4;
	reg_base = mem_base + reg_dump_data->bus_start_addr;
	reg_start_offset = soc_info->mem_block[TFE_CORE_BASE_IDX]->start +
		reg_dump_data->bus_start_addr;
	for (i = 0; i < num_reg; i++) {
		addr[0] = reg_start_offset + (i*4);
		addr[1] = cam_io_r(reg_base + (i*4));
		addr += 2;
	}

	/* Dump bus clients */
	reg_base = mem_base + reg_dump_data->bus_client_start_addr;
	reg_start_offset = soc_info->mem_block[TFE_CORE_BASE_IDX]->start +
		reg_dump_data->bus_client_start_addr;
	for (j = 0; j < reg_dump_data->num_bus_clients; j++) {

		for (i = 0; i <= 0x3c; i += 4) {
			addr[0] = reg_start_offset + i;
			addr[1] = cam_io_r(reg_base + i);
			addr += 2;
		}
		for (i = 0x60; i <= 0x80; i += 4) {
			addr[0] = reg_start_offset + (i*4);
			addr[1] = cam_io_r(reg_base + (i*4));
			addr += 2;
		}
		reg_base += reg_dump_data->bus_client_offset;
		reg_start_offset += reg_dump_data->bus_client_offset;
	}

	hdr->size = hdr->word_size * (addr - start);
	dump_args->offset +=  hdr->size +
		sizeof(struct cam_isp_hw_dump_header);

	/* Dump LUT entries */
	for (i = 0; i < reg_dump_data->num_lut_dump_entries; i++) {

		lut_bank_sel = reg_dump_data->lut_entry[i].lut_bank_sel;
		lut_size = reg_dump_data->lut_entry[i].lut_addr_size;
		lut_word_size = reg_dump_data->lut_entry[i].lut_word_size;
		lut_dmi_reg = reg_dump_data->lut_entry[i].dmi_reg_offset;
		dst = (char *)dump_args->cpu_addr + dump_args->offset;
		hdr = (struct cam_isp_hw_dump_header *)dst;
		scnprintf(hdr->tag, CAM_ISP_HW_DUMP_TAG_MAX_LEN, "LUT_REG:");
		hdr->word_size = lut_word_size/8;
		addr = (uint32_t *)(dst +
			sizeof(struct cam_isp_hw_dump_header));
		start = addr;
		*addr++ = lut_bank_sel;
		cam_io_w_mb(lut_bank_sel, mem_base + lut_dmi_reg + 4);
		cam_io_w_mb(0, mem_base + 0xC28);
		for (j = 0; j < lut_size; j++) {
			*addr = cam_io_r_mb(mem_base + 0xc30);
			addr++;
		}
		hdr->size = hdr->word_size * (addr - start);
		dump_args->offset +=  hdr->size +
			sizeof(struct cam_isp_hw_dump_header);
	}
	cam_io_w_mb(0, mem_base + 0xC24);
	cam_io_w_mb(0, mem_base + 0xC28);

dump_bw:
	dst = (char *)dump_args->cpu_addr + dump_args->offset;
	hdr = (struct cam_isp_hw_dump_header *)dst;
	scnprintf(hdr->tag, CAM_ISP_HW_DUMP_TAG_MAX_LEN, "TFE_CLK_RATE_BW:");
	clk_waddr = (uint64_t *)(dst +
		sizeof(struct cam_isp_hw_dump_header));
	clk_wstart = clk_waddr;
	hdr->word_size = sizeof(uint64_t);
	*clk_waddr++ = top_priv->hw_clk_rate;
	*clk_waddr++ = top_priv->total_bw_applied;

	hdr->size = hdr->word_size * (clk_waddr - clk_wstart);
	dump_args->offset +=  hdr->size +
		sizeof(struct cam_isp_hw_dump_header);

	dst = (char *)dump_args->cpu_addr + dump_args->offset;
	hdr = (struct cam_isp_hw_dump_header *)dst;
	scnprintf(hdr->tag, CAM_ISP_HW_DUMP_TAG_MAX_LEN, "TFE_NIU_MAXWR:");
	addr = (uint32_t *)(dst +
		sizeof(struct cam_isp_hw_dump_header));
	start = addr;
	hdr->word_size = sizeof(uint32_t);
	cam_cpas_reg_read(soc_private->cpas_handle,
		CAM_CPAS_REG_CAMNOC, 0x20, true, &val);
	*addr++ = val;
	hdr->size = hdr->word_size * (addr - start);
	dump_args->offset +=  hdr->size +
		sizeof(struct cam_isp_hw_dump_header);
	mutex_unlock(&tfe_hw_info->hw_mutex);

	CAM_DBG(CAM_ISP, "offset %zu", dump_args->offset);
	return 0;
}

static int cam_tfe_camif_irq_reg_dump(
	struct cam_tfe_hw_core_info    *core_info,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_tfe_top_priv            *top_priv;
	struct cam_isp_hw_get_cmd_update   *cmd_update;
	struct cam_isp_resource_node       *camif_res = NULL;
	void __iomem                       *mem_base;
	uint32_t i;

	int rc = 0;

	if (!cmd_args) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments\n");
		return -EINVAL;
	}
	top_priv = (struct cam_tfe_top_priv  *)core_info->top_priv;
	cmd_update = (struct cam_isp_hw_get_cmd_update  *)cmd_args;
	camif_res = cmd_update->res;
	mem_base = top_priv->common_data.soc_info->reg_map[0].mem_base;
	if ((camif_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) ||
		(camif_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE)) {
		CAM_ERR(CAM_ISP, "Error! Invalid state\n");
		return 0;
	}

	for (i = 0; i < CAM_TFE_TOP_IRQ_REG_NUM; i++) {
		CAM_INFO(CAM_ISP,
			"Core Id =%d TOP IRQ status[%d ] val 0x%x",
			core_info->core_index, i,
			cam_io_r_mb(mem_base +
			core_info->tfe_hw_info->top_irq_status[i]));
	}

	for (i = 0; i < CAM_TFE_BUS_MAX_IRQ_REGISTERS; i++) {
		CAM_INFO(CAM_ISP,
			"Core Id =%d BUS IRQ status[%d ] val:0x%x",
			core_info->core_index, i,
			cam_io_r_mb(mem_base +
			core_info->tfe_hw_info->bus_irq_status[i]));
	}

	return rc;
}

int cam_tfe_top_reserve(void *device_priv,
	void *reserve_args, uint32_t arg_size)
{
	struct cam_tfe_top_priv                 *top_priv;
	struct cam_tfe_acquire_args             *args;
	struct cam_tfe_hw_tfe_in_acquire_args   *acquire_args;
	struct cam_tfe_camif_data               *camif_data;
	struct cam_tfe_rdi_data                 *rdi_data;
	uint32_t i;
	int rc = -EINVAL;

	if (!device_priv || !reserve_args) {
		CAM_ERR(CAM_ISP, "Error Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_tfe_top_priv   *)device_priv;
	args = (struct cam_tfe_acquire_args *)reserve_args;
	acquire_args = &args->tfe_in;

	for (i = 0; i < CAM_TFE_TOP_IN_PORT_MAX; i++) {
		CAM_DBG(CAM_ISP, "i :%d res_id:%d state:%d", i,
			acquire_args->res_id, top_priv->in_rsrc[i].res_state);

		if ((top_priv->in_rsrc[i].res_id == acquire_args->res_id) &&
			(top_priv->in_rsrc[i].res_state ==
			CAM_ISP_RESOURCE_STATE_AVAILABLE)) {
			rc = cam_tfe_validate_pix_pattern(
				acquire_args->in_port->pix_pattern);
			if (rc)
				return rc;

			if (acquire_args->res_id == CAM_ISP_HW_TFE_IN_CAMIF) {
				camif_data = (struct cam_tfe_camif_data    *)
					top_priv->in_rsrc[i].res_priv;
				camif_data->pix_pattern =
					acquire_args->in_port->pix_pattern;
				camif_data->dsp_mode =
					acquire_args->in_port->dsp_mode;
				camif_data->left_first_pixel =
					acquire_args->in_port->left_start;
				camif_data->left_last_pixel =
					acquire_args->in_port->left_end;
				camif_data->right_first_pixel =
					acquire_args->in_port->right_start;
				camif_data->right_last_pixel =
					acquire_args->in_port->right_end;
				camif_data->first_line =
					acquire_args->in_port->line_start;
				camif_data->last_line =
					acquire_args->in_port->line_end;
				camif_data->camif_pd_enable =
					acquire_args->camif_pd_enable;
				camif_data->dual_tfe_sync_sel =
					acquire_args->dual_tfe_sync_sel_idx;
				camif_data->sync_mode = acquire_args->sync_mode;
				camif_data->event_cb = args->event_cb;
				camif_data->priv = args->priv;

				CAM_DBG(CAM_ISP,
					"TFE:%d pix_pattern:%d dsp_mode=%d",
					top_priv->in_rsrc[i].hw_intf->hw_idx,
					camif_data->pix_pattern,
					camif_data->dsp_mode);
			} else {
				rdi_data = (struct cam_tfe_rdi_data      *)
					top_priv->in_rsrc[i].res_priv;
				rdi_data->pix_pattern =
					acquire_args->in_port->pix_pattern;
				rdi_data->sync_mode = acquire_args->sync_mode;
				rdi_data->event_cb = args->event_cb;
				rdi_data->priv = args->priv;
				rdi_data->left_first_pixel =
					acquire_args->in_port->left_start;
				rdi_data->left_last_pixel =
					acquire_args->in_port->left_end;
				rdi_data->first_line =
					acquire_args->in_port->line_start;
				rdi_data->last_line =
					acquire_args->in_port->line_end;
			}

			top_priv->in_rsrc[i].cdm_ops = acquire_args->cdm_ops;
			top_priv->in_rsrc[i].tasklet_info = args->tasklet;
			top_priv->in_rsrc[i].res_state =
				CAM_ISP_RESOURCE_STATE_RESERVED;
			top_priv->tasklet_info = args->tasklet;
			acquire_args->rsrc_node =
				&top_priv->in_rsrc[i];
			rc = 0;
			break;
		}
	}

	return rc;
}

int cam_tfe_top_release(void *device_priv,
	void *release_args, uint32_t arg_size)
{
	struct cam_tfe_top_priv            *top_priv;
	struct cam_isp_resource_node       *in_res;

	if (!device_priv || !release_args) {
		CAM_ERR(CAM_ISP, "Error Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_tfe_top_priv   *)device_priv;
	in_res = (struct cam_isp_resource_node *)release_args;

	CAM_DBG(CAM_ISP, "TFE:%d resource id:%d in state %d",
		in_res->hw_intf->hw_idx, in_res->res_id,
		in_res->res_state);
	if (in_res->res_state < CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "TFE:%d Error Resource Invalid res_state :%d",
			in_res->hw_intf->hw_idx, in_res->res_state);
		return -EINVAL;
	}
	in_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	in_res->cdm_ops = NULL;
	in_res->tasklet_info = NULL;
	in_res->rdi_only_ctx = 0;

	return 0;
}

static int cam_tfe_camif_resource_start(
	struct cam_tfe_hw_core_info         *core_info,
	struct cam_isp_resource_node        *camif_res)
{
	struct cam_tfe_camif_data           *rsrc_data;
	struct cam_tfe_soc_private          *soc_private;
	uint32_t                             val = 0;
	uint32_t                             epoch0_irq_mask;
	uint32_t                             epoch1_irq_mask;
	uint32_t                             computed_epoch_line_cfg;

	if (!camif_res || !core_info) {
		CAM_ERR(CAM_ISP, "Error Invalid input arguments");
		return -EINVAL;
	}

	if (camif_res->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "TFE:%d Error Invalid camif res res_state:%d",
			core_info->core_index, camif_res->res_state);
		return -EINVAL;
	}

	rsrc_data = (struct cam_tfe_camif_data  *)camif_res->res_priv;
	soc_private = rsrc_data->soc_info->soc_private;

	if (!soc_private) {
		CAM_ERR(CAM_ISP, "TFE:%d Error soc_private NULL",
			core_info->core_index);
		return -ENODEV;
	}

	/* Config tfe core*/
	val = 0;
	if (rsrc_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE)
		val = (1 << rsrc_data->reg_data->extern_reg_update_shift);

	if ((rsrc_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE) ||
		(rsrc_data->sync_mode == CAM_ISP_HW_SYNC_MASTER)) {
		val |= (1 << rsrc_data->reg_data->dual_tfe_pix_en_shift);
		val |= ((rsrc_data->dual_tfe_sync_sel + 1) <<
			rsrc_data->reg_data->dual_tfe_sync_sel_shift);
	}

	if (!rsrc_data->camif_pd_enable)
		val |= (1 << rsrc_data->reg_data->camif_pd_rdi2_src_sel_shift);

	/* enables the Delay Line CLC in the pixel pipeline */
	val |= BIT(rsrc_data->reg_data->delay_line_en_shift);

	cam_io_w_mb(val, rsrc_data->mem_base +
		rsrc_data->common_reg->core_cfg_0);

	CAM_DBG(CAM_ISP, "TFE:%d core_cfg 0 val:0x%x", core_info->core_index,
		val);

	val = cam_io_r(rsrc_data->mem_base +
		rsrc_data->common_reg->core_cfg_1);
	val &= ~BIT(0);
	cam_io_w_mb(val, rsrc_data->mem_base +
		rsrc_data->common_reg->core_cfg_1);
	CAM_DBG(CAM_ISP, "TFE:%d core_cfg 1 val:0x%x", core_info->core_index,
		val);

	/* Epoch config */
	epoch0_irq_mask = ((rsrc_data->last_line -
			rsrc_data->first_line) / 2) +
			rsrc_data->first_line;
	epoch1_irq_mask = rsrc_data->reg_data->epoch_line_cfg &
			0xFFFF;
	computed_epoch_line_cfg = (epoch0_irq_mask << 16) |
			epoch1_irq_mask;
	cam_io_w_mb(computed_epoch_line_cfg,
			rsrc_data->mem_base +
			rsrc_data->camif_reg->epoch_irq_cfg);
	CAM_DBG(CAM_ISP, "TFE:%d first_line: %u\n"
			"last_line: %u\n"
			"epoch_line_cfg: 0x%x",
			core_info->core_index,
			rsrc_data->first_line,
			rsrc_data->last_line,
			computed_epoch_line_cfg);

	camif_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	/* Reg Update */
	cam_io_w_mb(rsrc_data->reg_data->reg_update_cmd_data,
		rsrc_data->mem_base + rsrc_data->camif_reg->reg_update_cmd);
	CAM_DBG(CAM_ISP, "hw id:%d RUP val:%d", camif_res->hw_intf->hw_idx,
		rsrc_data->reg_data->reg_update_cmd_data);

	/* Disable sof irq debug flag */
	rsrc_data->enable_sof_irq_debug = false;
	rsrc_data->irq_debug_cnt = 0;

	if (rsrc_data->camif_debug & CAMIF_DEBUG_ENABLE_SENSOR_DIAG_STATUS) {
		val = cam_io_r_mb(rsrc_data->mem_base +
			rsrc_data->common_reg->diag_config);
		val |= rsrc_data->reg_data->enable_diagnostic_hw;
		cam_io_w_mb(val, rsrc_data->mem_base +
			rsrc_data->common_reg->diag_config);
	}

	/* Enable the irq */
	cam_tfe_irq_config(core_info, rsrc_data->reg_data->subscribe_irq_mask,
		CAM_TFE_TOP_IRQ_REG_NUM, true);

	/* Program perf counters */
	val = (1 << rsrc_data->reg_data->perf_cnt_start_cmd_shift) |
		(1 << rsrc_data->reg_data->perf_cnt_continuous_shift) |
		(1 << rsrc_data->reg_data->perf_client_sel_shift) |
		(1 << rsrc_data->reg_data->perf_window_start_shift) |
		(2 << rsrc_data->reg_data->perf_window_end_shift);
	cam_io_w_mb(val,
		rsrc_data->mem_base + rsrc_data->common_reg->perf_cnt_cfg);
	CAM_DBG(CAM_ISP, "TFE:%d perf_cfg val:%d", core_info->core_index,
		val);

	/* Enable the top debug registers */
	cam_io_w_mb(0x1,
		rsrc_data->mem_base + rsrc_data->common_reg->debug_cfg);

	CAM_DBG(CAM_ISP, "Start Camif TFE %d Done", core_info->core_index);
	return 0;
}

int cam_tfe_top_start(struct cam_tfe_hw_core_info *core_info,
	void *start_args, uint32_t arg_size)
{
	struct cam_tfe_top_priv                 *top_priv;
	struct cam_isp_resource_node            *in_res;
	struct cam_hw_info                      *hw_info = NULL;
	struct cam_tfe_rdi_data                 *rsrc_rdi_data;
	uint32_t val;
	int rc = 0;

	if (!start_args) {
		CAM_ERR(CAM_ISP, "TFE:%d Error Invalid input arguments",
			core_info->core_index);
		return -EINVAL;
	}

	top_priv = (struct cam_tfe_top_priv *)core_info->top_priv;
	in_res = (struct cam_isp_resource_node *)start_args;
	hw_info = (struct cam_hw_info  *)in_res->hw_intf->hw_priv;

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_ISP, "TFE:%d HW not powered up",
			core_info->core_index);
		rc = -EPERM;
		goto end;
	}

	rc = cam_tfe_top_set_hw_clk_rate(top_priv);
	if (rc) {
		CAM_ERR(CAM_ISP, "TFE:%d set_hw_clk_rate failed, rc=%d",
			hw_info->soc_info.index, rc);
		return rc;
	}

	rc = cam_tfe_top_set_axi_bw_vote(top_priv, true);
	if (rc) {
		CAM_ERR(CAM_ISP, "TFE:%d set_axi_bw_vote failed, rc=%d",
			core_info->core_index, rc);
		return rc;
	}

	if (in_res->res_id == CAM_ISP_HW_TFE_IN_CAMIF) {
		cam_tfe_camif_resource_start(core_info, in_res);
	} else if (in_res->res_id >= CAM_ISP_HW_TFE_IN_RDI0 ||
		in_res->res_id <= CAM_ISP_HW_TFE_IN_RDI2) {
		rsrc_rdi_data = (struct cam_tfe_rdi_data *) in_res->res_priv;
		val = (rsrc_rdi_data->pix_pattern <<
			rsrc_rdi_data->reg_data->pixel_pattern_shift);

		val |= (1 << rsrc_rdi_data->reg_data->rdi_out_enable_shift);
		cam_io_w_mb(val, rsrc_rdi_data->mem_base +
			rsrc_rdi_data->rdi_reg->rdi_module_config);

		/* Epoch config */
		cam_io_w_mb(rsrc_rdi_data->reg_data->epoch_line_cfg,
			rsrc_rdi_data->mem_base +
			rsrc_rdi_data->rdi_reg->rdi_epoch_irq);

		/* Reg Update */
		cam_io_w_mb(rsrc_rdi_data->reg_data->reg_update_cmd_data,
			rsrc_rdi_data->mem_base +
			rsrc_rdi_data->rdi_reg->reg_update_cmd);
		in_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

		/* Enable the irq */
		if (in_res->rdi_only_ctx)
			cam_tfe_irq_config(core_info,
				rsrc_rdi_data->reg_data->subscribe_irq_mask,
				CAM_TFE_TOP_IRQ_REG_NUM, true);

		CAM_DBG(CAM_ISP, "TFE:%d Start RDI %d", core_info->core_index,
			in_res->res_id - CAM_ISP_HW_TFE_IN_RDI0);
	}

	core_info->irq_err_config_cnt++;
	if (core_info->irq_err_config_cnt == 1)  {
		cam_tfe_irq_config(core_info,
			core_info->tfe_hw_info->error_irq_mask,
			CAM_TFE_TOP_IRQ_REG_NUM, true);
		top_priv->error_ts.tv_sec = 0;
		top_priv->error_ts.tv_usec = 0;
		top_priv->sof_ts.tv_sec = 0;
		top_priv->sof_ts.tv_usec = 0;
		top_priv->epoch_ts.tv_sec = 0;
		top_priv->epoch_ts.tv_usec = 0;
		top_priv->eof_ts.tv_sec = 0;
		top_priv->eof_ts.tv_usec = 0;
	}

end:
	return rc;
}

int cam_tfe_top_stop(struct cam_tfe_hw_core_info *core_info,
	void *stop_args, uint32_t arg_size)
{
	struct cam_tfe_top_priv                 *top_priv;
	struct cam_isp_resource_node            *in_res;
	struct cam_hw_info                      *hw_info = NULL;
	struct cam_tfe_camif_data               *camif_data;
	struct cam_tfe_rdi_data                 *rsrc_rdi_data;
	uint32_t val = 0;
	int i, rc = 0;

	if (!stop_args) {
		CAM_ERR(CAM_ISP, "TFE:%d Error Invalid input arguments",
			core_info->core_index);
		return -EINVAL;
	}

	top_priv = (struct cam_tfe_top_priv   *)core_info->top_priv;
	in_res = (struct cam_isp_resource_node *)stop_args;
	hw_info = (struct cam_hw_info  *)in_res->hw_intf->hw_priv;

	if (in_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED ||
		in_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE)
		return 0;

	if (in_res->res_id == CAM_ISP_HW_TFE_IN_CAMIF) {
		camif_data = (struct cam_tfe_camif_data *)in_res->res_priv;

		cam_io_w_mb(0, camif_data->mem_base +
			camif_data->camif_reg->module_cfg);

		cam_tfe_irq_config(core_info,
			camif_data->reg_data->subscribe_irq_mask,
			CAM_TFE_TOP_IRQ_REG_NUM, false);

		if (in_res->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
			in_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

		val = cam_io_r_mb(camif_data->mem_base +
				camif_data->common_reg->diag_config);
		if (val & camif_data->reg_data->enable_diagnostic_hw) {
			val &= ~camif_data->reg_data->enable_diagnostic_hw;
			cam_io_w_mb(val, camif_data->mem_base +
				camif_data->common_reg->diag_config);
		}
	}  else if ((in_res->res_id >= CAM_ISP_HW_TFE_IN_RDI0) &&
		(in_res->res_id <= CAM_ISP_HW_TFE_IN_RDI2)) {
		rsrc_rdi_data = (struct cam_tfe_rdi_data *) in_res->res_priv;
		cam_io_w_mb(0x0, rsrc_rdi_data->mem_base +
			rsrc_rdi_data->rdi_reg->rdi_module_config);

		if (in_res->rdi_only_ctx)
			cam_tfe_irq_config(core_info,
				rsrc_rdi_data->reg_data->subscribe_irq_mask,
				CAM_TFE_TOP_IRQ_REG_NUM, false);

		if (in_res->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
			in_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	} else {
		CAM_ERR(CAM_ISP, "TFE:%d Invalid res id:%d",
			core_info->core_index, in_res->res_id);
		return -EINVAL;
	}

	if (!rc) {
		for (i = 0; i < CAM_TFE_TOP_IN_PORT_MAX; i++) {
			if (top_priv->in_rsrc[i].res_id == in_res->res_id) {
				top_priv->req_clk_rate[i] = 0;
				memset(&top_priv->req_axi_vote[i], 0,
					sizeof(struct cam_axi_vote));
				top_priv->axi_vote_control[i] =
					CAM_TFE_BW_CONTROL_EXCLUDE;
				break;
			}
		}
	}

	core_info->irq_err_config_cnt--;
	if (!core_info->irq_err_config_cnt)
		cam_tfe_irq_config(core_info,
			core_info->tfe_hw_info->error_irq_mask,
			CAM_TFE_TOP_IRQ_REG_NUM, false);

	return rc;
}

int cam_tfe_top_init(
	struct cam_hw_soc_info                 *soc_info,
	struct cam_hw_intf                     *hw_intf,
	void                                   *top_hw_info,
	struct cam_tfe_hw_core_info            *core_info)
{
	struct cam_tfe_top_priv           *top_priv = NULL;
	struct cam_tfe_top_hw_info        *hw_info = top_hw_info;
	struct cam_tfe_soc_private        *soc_private = NULL;
	struct cam_tfe_camif_data         *camif_priv = NULL;
	struct cam_tfe_rdi_data           *rdi_priv = NULL;
	int i, j, rc = 0;

	top_priv = kzalloc(sizeof(struct cam_tfe_top_priv),
		GFP_KERNEL);
	if (!top_priv) {
		CAM_DBG(CAM_ISP, "TFE:%DError Failed to alloc for tfe_top_priv",
			core_info->core_index);
		rc = -ENOMEM;
		goto end;
	}
	core_info->top_priv = top_priv;

	soc_private = soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_ISP, "TFE:%d Error soc_private NULL",
			core_info->core_index);
		rc = -ENODEV;
		goto free_tfe_top_priv;
	}

	top_priv->hw_clk_rate = 0;
	memset(top_priv->last_vote, 0x0, sizeof(struct cam_axi_vote) *
		(CAM_TFE_TOP_IN_PORT_MAX *
		CAM_TFE_DELAY_BW_REDUCTION_NUM_FRAMES));
	top_priv->last_counter = 0;

	for (i = 0, j = 0; i < CAM_TFE_TOP_IN_PORT_MAX; i++) {
		top_priv->in_rsrc[i].res_type = CAM_ISP_RESOURCE_TFE_IN;
		top_priv->in_rsrc[i].hw_intf = hw_intf;
		top_priv->in_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_AVAILABLE;
		top_priv->req_clk_rate[i] = 0;
		memset(&top_priv->req_axi_vote[i], 0,
			sizeof(struct cam_axi_vote));
		top_priv->axi_vote_control[i] =
			CAM_TFE_BW_CONTROL_EXCLUDE;

		if (hw_info->in_port[i] == CAM_TFE_CAMIF_VER_1_0) {
			top_priv->in_rsrc[i].res_id =
				CAM_ISP_HW_TFE_IN_CAMIF;

			camif_priv = kzalloc(sizeof(struct cam_tfe_camif_data),
				GFP_KERNEL);
			if (!camif_priv) {
				CAM_DBG(CAM_ISP,
					"TFE:%dError Failed to alloc for camif_priv",
					core_info->core_index);
				goto free_tfe_top_priv;
			}

			top_priv->in_rsrc[i].res_priv = camif_priv;

			camif_priv->mem_base    =
				soc_info->reg_map[TFE_CORE_BASE_IDX].mem_base;
			camif_priv->camif_reg   =
				hw_info->camif_hw_info.camif_reg;
			camif_priv->common_reg  = hw_info->common_reg;
			camif_priv->reg_data    =
				hw_info->camif_hw_info.reg_data;
			camif_priv->hw_intf     = hw_intf;
			camif_priv->soc_info    = soc_info;

		} else if (hw_info->in_port[i] ==
			CAM_TFE_RDI_VER_1_0) {
			top_priv->in_rsrc[i].res_id =
				CAM_ISP_HW_TFE_IN_RDI0 + j;

			rdi_priv = kzalloc(sizeof(struct cam_tfe_rdi_data),
					GFP_KERNEL);
			if (!rdi_priv) {
				CAM_DBG(CAM_ISP,
					"TFE:%d Error Failed to alloc for rdi_priv",
					core_info->core_index);
				goto deinit_resources;
			}

			top_priv->in_rsrc[i].res_priv = rdi_priv;

			rdi_priv->mem_base   =
				soc_info->reg_map[TFE_CORE_BASE_IDX].mem_base;
			rdi_priv->hw_intf    = hw_intf;
			rdi_priv->common_reg = hw_info->common_reg;
			rdi_priv->rdi_reg    =
				hw_info->rdi_hw_info[j].rdi_reg;
			rdi_priv->reg_data =
				hw_info->rdi_hw_info[j++].reg_data;
		}  else {
			CAM_WARN(CAM_ISP, "TFE:%d Invalid inport type: %u",
				core_info->core_index, hw_info->in_port[i]);
		}
	}

	top_priv->common_data.soc_info     = soc_info;
	top_priv->common_data.hw_intf      = hw_intf;
	top_priv->common_data.common_reg   = hw_info->common_reg;
	top_priv->common_data.reg_dump_data = &hw_info->reg_dump_data;

	return rc;

deinit_resources:
	for (--i; i >= 0; i--) {

		top_priv->in_rsrc[i].start = NULL;
		top_priv->in_rsrc[i].stop  = NULL;
		top_priv->in_rsrc[i].process_cmd = NULL;
		top_priv->in_rsrc[i].top_half_handler = NULL;
		top_priv->in_rsrc[i].bottom_half_handler = NULL;

		if (!top_priv->in_rsrc[i].res_priv)
			continue;

		kfree(top_priv->in_rsrc[i].res_priv);
		top_priv->in_rsrc[i].res_priv = NULL;
		top_priv->in_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
	}
free_tfe_top_priv:
	kfree(core_info->top_priv);
	core_info->top_priv = NULL;
end:
	return rc;
}


int cam_tfe_top_deinit(struct cam_tfe_top_priv  *top_priv)
{
	int i, rc = 0;

	if (!top_priv) {
		CAM_ERR(CAM_ISP, "Error Invalid input");
		return -EINVAL;
	}

	for (i = 0; i < CAM_TFE_TOP_IN_PORT_MAX; i++) {
		top_priv->in_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_UNAVAILABLE;

		top_priv->in_rsrc[i].start = NULL;
		top_priv->in_rsrc[i].stop  = NULL;
		top_priv->in_rsrc[i].process_cmd = NULL;
		top_priv->in_rsrc[i].top_half_handler = NULL;
		top_priv->in_rsrc[i].bottom_half_handler = NULL;

		if (!top_priv->in_rsrc[i].res_priv) {
			CAM_ERR(CAM_ISP, "Error res_priv is NULL");
			return -ENODEV;
		}

		kfree(top_priv->in_rsrc[i].res_priv);
		top_priv->in_rsrc[i].res_priv = NULL;
	}

	return rc;
}

int cam_tfe_reset(void *hw_priv, void *reset_core_args, uint32_t arg_size)
{
	struct cam_hw_info                *tfe_hw  = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct cam_tfe_hw_core_info       *core_info = NULL;
	struct cam_tfe_top_priv           *top_priv  = NULL;
	struct cam_tfe_hw_info            *hw_info = NULL;
	void __iomem                      *mem_base;
	uint32_t *reset_reg_args = reset_core_args;
	uint32_t i, reset_reg_val, irq_status[3];
	int rc;

	CAM_DBG(CAM_ISP, "Enter");

	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	soc_info = &tfe_hw->soc_info;
	core_info = (struct cam_tfe_hw_core_info *)tfe_hw->core_info;
	top_priv = core_info->top_priv;
	hw_info = core_info->tfe_hw_info;
	mem_base = tfe_hw->soc_info.reg_map[TFE_CORE_BASE_IDX].mem_base;

	for (i = 0; i < CAM_TFE_TOP_IRQ_REG_NUM; i++)
		irq_status[i] = cam_io_r(mem_base +
			core_info->tfe_hw_info->top_irq_status[i]);

	for (i = 0; i < CAM_TFE_TOP_IRQ_REG_NUM; i++)
		cam_io_w(irq_status[i], mem_base +
			core_info->tfe_hw_info->top_irq_clear[i]);

	cam_io_w_mb(core_info->tfe_hw_info->global_clear_bitmask,
		mem_base + core_info->tfe_hw_info->top_irq_cmd);

	/* Mask all irq registers */
	for (i = 0; i < CAM_TFE_TOP_IRQ_REG_NUM; i++)
		cam_io_w(0, mem_base +
			core_info->tfe_hw_info->top_irq_mask[i]);

	cam_tfe_irq_config(core_info, hw_info->reset_irq_mask,
		CAM_TFE_TOP_IRQ_REG_NUM, true);

	reinit_completion(&core_info->reset_complete);

	CAM_DBG(CAM_ISP, "calling RESET on tfe %d", soc_info->index);

	switch (*reset_reg_args) {
	case CAM_TFE_HW_RESET_HW_AND_REG:
		reset_reg_val = CAM_TFE_HW_RESET_HW_AND_REG_VAL;
		break;
	default:
		reset_reg_val = CAM_TFE_HW_RESET_HW_VAL;
		break;
	}

	cam_io_w_mb(reset_reg_val, mem_base +
		top_priv->common_data.common_reg->global_reset_cmd);

	CAM_DBG(CAM_ISP, "TFE:%d waiting for tfe reset complete",
		core_info->core_index);
	/* Wait for Completion or Timeout of 100ms */
	rc = wait_for_completion_timeout(&core_info->reset_complete,
		msecs_to_jiffies(100));
	if (rc <= 0) {
		CAM_ERR(CAM_ISP, "TFE:%d Error Reset Timeout",
			core_info->core_index);
		rc = -ETIMEDOUT;
	} else {
		rc = 0;
		CAM_DBG(CAM_ISP, "TFE:%d reset complete done (%d)",
			core_info->core_index, rc);
	}

	CAM_DBG(CAM_ISP, "TFE:%d reset complete done (%d)",
		core_info->core_index, rc);

	cam_tfe_irq_config(core_info, hw_info->reset_irq_mask,
		CAM_TFE_TOP_IRQ_REG_NUM, false);

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

int cam_tfe_init_hw(void *hw_priv, void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info                *tfe_hw = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct cam_tfe_hw_core_info       *core_info = NULL;
	struct cam_tfe_top_priv           *top_priv;
	void __iomem                      *mem_base;
	int rc = 0;
	uint32_t                           reset_core_args =
					CAM_TFE_HW_RESET_HW_AND_REG;

	CAM_DBG(CAM_ISP, "Enter");
	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	soc_info = &tfe_hw->soc_info;
	core_info = (struct cam_tfe_hw_core_info *)tfe_hw->core_info;
	top_priv = (struct cam_tfe_top_priv *)core_info->top_priv;

	mutex_lock(&tfe_hw->hw_mutex);
	tfe_hw->open_count++;
	if (tfe_hw->open_count > 1) {
		mutex_unlock(&tfe_hw->hw_mutex);
		CAM_DBG(CAM_ISP, "TFE:%d has already been initialized cnt %d",
			core_info->core_index, tfe_hw->open_count);
		return 0;
	}
	mutex_unlock(&tfe_hw->hw_mutex);

	/* Turn ON Regulators, Clocks and other SOC resources */
	rc = cam_tfe_enable_soc_resources(soc_info);
	if (rc) {
		CAM_ERR(CAM_ISP, "Enable SOC failed");
		rc = -EFAULT;
		goto decrement_open_cnt;
	}
	tfe_hw->hw_state = CAM_HW_STATE_POWER_UP;

	mem_base = tfe_hw->soc_info.reg_map[TFE_CORE_BASE_IDX].mem_base;
	CAM_DBG(CAM_ISP, "TFE:%d Enable soc done", core_info->core_index);

	/* Do HW Reset */
	rc = cam_tfe_reset(hw_priv, &reset_core_args, sizeof(uint32_t));
	if (rc) {
		CAM_ERR(CAM_ISP, "TFE:%d Reset Failed rc=%d",
			core_info->core_index, rc);
		goto disable_soc;
	}

	top_priv->hw_clk_rate = 0;
	core_info->irq_err_config_cnt = 0;
	core_info->irq_err_config = false;
	rc = core_info->tfe_bus->hw_ops.init(core_info->tfe_bus->bus_priv,
		NULL, 0);
	if (rc) {
		CAM_ERR(CAM_ISP, "TFE:%d Top HW init Failed rc=%d",
			core_info->core_index, rc);
		goto disable_soc;
	}

	return rc;

disable_soc:
	cam_tfe_disable_soc_resources(soc_info);
	tfe_hw->hw_state = CAM_HW_STATE_POWER_DOWN;

decrement_open_cnt:
	mutex_lock(&tfe_hw->hw_mutex);
	tfe_hw->open_count--;
	mutex_unlock(&tfe_hw->hw_mutex);
	return rc;
}

int cam_tfe_deinit_hw(void *hw_priv, void *deinit_hw_args, uint32_t arg_size)
{
	struct cam_hw_info                *tfe_hw = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct cam_tfe_hw_core_info       *core_info = NULL;
	int rc = 0;
	uint32_t                           reset_core_args =
					CAM_TFE_HW_RESET_HW_AND_REG;

	CAM_DBG(CAM_ISP, "Enter");
	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	soc_info = &tfe_hw->soc_info;
	core_info = (struct cam_tfe_hw_core_info *)tfe_hw->core_info;

	mutex_lock(&tfe_hw->hw_mutex);
	if (!tfe_hw->open_count) {
		mutex_unlock(&tfe_hw->hw_mutex);
		CAM_ERR_RATE_LIMIT(CAM_ISP, "TFE:%d Error Unbalanced deinit",
			core_info->core_index);
		return -EFAULT;
	}
	tfe_hw->open_count--;
	if (tfe_hw->open_count) {
		mutex_unlock(&tfe_hw->hw_mutex);
		CAM_DBG(CAM_ISP, "TFE:%d open_cnt non-zero =%d",
			core_info->core_index, tfe_hw->open_count);
		return 0;
	}
	mutex_unlock(&tfe_hw->hw_mutex);

	rc = core_info->tfe_bus->hw_ops.deinit(core_info->tfe_bus->bus_priv,
		NULL, 0);
	if (rc)
		CAM_ERR(CAM_ISP, "TFE:%d Bus HW deinit Failed rc=%d",
			core_info->core_index, rc);

	rc = cam_tfe_reset(hw_priv, &reset_core_args, sizeof(uint32_t));

	/* Turn OFF Regulators, Clocks and other SOC resources */
	CAM_DBG(CAM_ISP, "TFE:%d Disable SOC resource", core_info->core_index);
	rc = cam_tfe_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_ISP, " TFE:%d Disable SOC failed",
			core_info->core_index);

	tfe_hw->hw_state = CAM_HW_STATE_POWER_DOWN;

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

int cam_tfe_reserve(void *hw_priv, void *reserve_args, uint32_t arg_size)
{
	struct cam_tfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info                *tfe_hw  = hw_priv;
	struct cam_tfe_acquire_args       *acquire;
	int rc = -ENODEV;

	if (!hw_priv || !reserve_args || (arg_size !=
		sizeof(struct cam_tfe_acquire_args))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}
	core_info = (struct cam_tfe_hw_core_info *)tfe_hw->core_info;
	acquire = (struct cam_tfe_acquire_args   *)reserve_args;

	CAM_DBG(CAM_ISP, "TFE:%d acquire res type: %d",
		core_info->core_index, acquire->rsrc_type);
	mutex_lock(&tfe_hw->hw_mutex);
	if (acquire->rsrc_type == CAM_ISP_RESOURCE_TFE_IN) {
		rc = cam_tfe_top_reserve(core_info->top_priv,
		reserve_args, arg_size);
	} else if (acquire->rsrc_type == CAM_ISP_RESOURCE_TFE_OUT) {
		rc = core_info->tfe_bus->hw_ops.reserve(
			core_info->tfe_bus->bus_priv, acquire,
			sizeof(*acquire));
	} else {
		CAM_ERR(CAM_ISP, "TFE:%d Invalid res type:%d",
			core_info->core_index, acquire->rsrc_type);
	}

	mutex_unlock(&tfe_hw->hw_mutex);

	return rc;
}


int cam_tfe_release(void *hw_priv, void *release_args, uint32_t arg_size)
{
	struct cam_tfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info                *tfe_hw  = hw_priv;
	struct cam_isp_resource_node      *isp_res;
	int rc = -ENODEV;

	if (!hw_priv || !release_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	core_info = (struct cam_tfe_hw_core_info *)tfe_hw->core_info;
	isp_res = (struct cam_isp_resource_node      *) release_args;

	mutex_lock(&tfe_hw->hw_mutex);
	if (isp_res->res_type == CAM_ISP_RESOURCE_TFE_IN)
		rc = cam_tfe_top_release(core_info->top_priv, isp_res,
			sizeof(*isp_res));
	else if (isp_res->res_type == CAM_ISP_RESOURCE_TFE_OUT) {
		rc = core_info->tfe_bus->hw_ops.release(
			core_info->tfe_bus->bus_priv, isp_res,
			sizeof(*isp_res));
	} else {
		CAM_ERR(CAM_ISP, "TFE:%d Invalid res type:%d",
			core_info->core_index, isp_res->res_type);
	}

	mutex_unlock(&tfe_hw->hw_mutex);

	return rc;
}

int cam_tfe_start(void *hw_priv, void *start_args, uint32_t arg_size)
{
	struct cam_tfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info                *tfe_hw  = hw_priv;
	struct cam_isp_resource_node      *start_res;

	int rc = 0;

	if (!hw_priv || !start_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	core_info = (struct cam_tfe_hw_core_info *)tfe_hw->core_info;
	start_res = (struct cam_isp_resource_node  *)start_args;
	core_info->tasklet_info = start_res->tasklet_info;

	mutex_lock(&tfe_hw->hw_mutex);
	if (start_res->res_type == CAM_ISP_RESOURCE_TFE_IN) {
		rc = cam_tfe_top_start(core_info, start_args,
			arg_size);
		if (rc)
			CAM_ERR(CAM_ISP, "TFE:%d Start failed. type:%d",
				core_info->core_index, start_res->res_type);

	} else if (start_res->res_type == CAM_ISP_RESOURCE_TFE_OUT) {
		rc = core_info->tfe_bus->hw_ops.start(start_res, NULL, 0);
	} else {
		CAM_ERR(CAM_ISP, "TFE:%d Invalid res type:%d",
			core_info->core_index, start_res->res_type);
		rc = -EFAULT;
	}

	mutex_unlock(&tfe_hw->hw_mutex);

	return rc;
}

int cam_tfe_stop(void *hw_priv, void *stop_args, uint32_t arg_size)
{
	struct cam_tfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info                *tfe_hw  = hw_priv;
	struct cam_isp_resource_node      *isp_res;
	int rc = -EINVAL;

	if (!hw_priv || !stop_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	core_info = (struct cam_tfe_hw_core_info *)tfe_hw->core_info;
	isp_res = (struct cam_isp_resource_node  *)stop_args;

	mutex_lock(&tfe_hw->hw_mutex);
	if (isp_res->res_type == CAM_ISP_RESOURCE_TFE_IN) {
		rc = cam_tfe_top_stop(core_info, isp_res,
			sizeof(struct cam_isp_resource_node));
	} else if (isp_res->res_type == CAM_ISP_RESOURCE_TFE_OUT) {
		rc = core_info->tfe_bus->hw_ops.stop(isp_res, NULL, 0);
	} else {
		CAM_ERR(CAM_ISP, "TFE:%d Invalid res type:%d",
			core_info->core_index, isp_res->res_type);
	}

	CAM_DBG(CAM_ISP, "TFE:%d stopped res type:%d res id:%d res_state:%d ",
		core_info->core_index, isp_res->res_type,
		isp_res->res_id, isp_res->res_state);

	mutex_unlock(&tfe_hw->hw_mutex);

	return rc;
}

int cam_tfe_read(void *hw_priv, void *read_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_tfe_write(void *hw_priv, void *write_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_tfe_process_cmd(void *hw_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_hw_info                *tfe_hw = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct cam_tfe_hw_core_info       *core_info = NULL;
	struct cam_tfe_hw_info            *hw_info = NULL;
	int rc = 0;

	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	soc_info = &tfe_hw->soc_info;
	core_info = (struct cam_tfe_hw_core_info *)tfe_hw->core_info;
	hw_info = core_info->tfe_hw_info;

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_CHANGE_BASE:
		rc = cam_tfe_top_get_base(core_info->top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_REG_UPDATE:
		rc = cam_tfe_top_get_reg_update(core_info->top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_CLOCK_UPDATE:
		rc = cam_tfe_top_clock_update(core_info->top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_BW_UPDATE_V2:
		rc = cam_tfe_top_bw_update(core_info->top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_BW_CONTROL:
		rc = cam_tfe_top_bw_control(core_info->top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_REG_DUMP:
		rc = cam_tfe_top_get_reg_dump(core_info->top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_IRQ_REGISTER_DUMP:
		rc = cam_tfe_camif_irq_reg_dump(core_info, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_QUERY_REGSPACE_DATA:
		*((struct cam_hw_soc_info **)cmd_args) = soc_info;
		break;
	case CAM_ISP_HW_CMD_DUMP_HW:
		rc = cam_tfe_hw_dump(core_info,
			cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_BUF_UPDATE:
	case CAM_ISP_HW_CMD_GET_HFR_UPDATE:
	case CAM_ISP_HW_CMD_STRIPE_UPDATE:
	case CAM_ISP_HW_CMD_STOP_BUS_ERR_IRQ:
	case CAM_ISP_HW_CMD_GET_SECURE_MODE:
		rc = core_info->tfe_bus->hw_ops.process_cmd(
			core_info->tfe_bus->bus_priv, cmd_type, cmd_args,
			arg_size);
		break;
	default:
		CAM_ERR(CAM_ISP, "TFE:%d Invalid cmd type:%d",
			core_info->core_index, cmd_type);
		rc = -EINVAL;
		break;
	}
	return rc;
}

int cam_tfe_core_init(struct cam_tfe_hw_core_info  *core_info,
	struct cam_hw_soc_info                     *soc_info,
	struct cam_hw_intf                         *hw_intf,
	struct cam_tfe_hw_info                     *tfe_hw_info)
{
	int rc = -EINVAL;
	int i;

	if (!cam_cpas_is_feature_supported(CAM_CPAS_ISP_FUSE_ID,
		hw_intf->hw_idx)) {
		CAM_INFO(CAM_ISP, "TFE:%d is not supported",
			hw_intf->hw_idx);
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ISP, "TFE:%d  is supported",
		hw_intf->hw_idx);

	rc = cam_tfe_top_init(soc_info, hw_intf, tfe_hw_info->top_hw_info,
		core_info);
	if (rc) {
		CAM_ERR(CAM_ISP, "TFE:%d Error cam_tfe_top_init failed",
			core_info->core_index);
		goto end;
	}

	rc = cam_tfe_bus_init(soc_info, hw_intf,
		tfe_hw_info->bus_hw_info, core_info,
		&core_info->tfe_bus);
	if (rc) {
		CAM_ERR(CAM_ISP, "TFE:%d Error cam_tfe_bus_init failed",
			core_info->core_index);
		goto deinit_top;
	}

	INIT_LIST_HEAD(&core_info->free_payload_list);
	for (i = 0; i < CAM_TFE_EVT_MAX; i++) {
		INIT_LIST_HEAD(&core_info->evt_payload[i].list);
		list_add_tail(&core_info->evt_payload[i].list,
			&core_info->free_payload_list);
	}

	core_info->irq_err_config = false;
	core_info->irq_err_config_cnt = 0;
	spin_lock_init(&core_info->spin_lock);
	init_completion(&core_info->reset_complete);

	return rc;

deinit_top:
	cam_tfe_top_deinit(core_info->top_priv);

end:
	return rc;
}

int cam_tfe_core_deinit(struct cam_tfe_hw_core_info  *core_info,
	struct cam_tfe_hw_info                       *tfe_hw_info)
{
	int                rc = -EINVAL;
	int                i;
	unsigned long      flags;

	spin_lock_irqsave(&core_info->spin_lock, flags);

	INIT_LIST_HEAD(&core_info->free_payload_list);
	for (i = 0; i < CAM_TFE_EVT_MAX; i++)
		INIT_LIST_HEAD(&core_info->evt_payload[i].list);

	rc = cam_tfe_bus_deinit(&core_info->tfe_bus);
	if (rc)
		CAM_ERR(CAM_ISP, "TFE:%d Error cam_tfe_bus_deinit failed rc=%d",
			core_info->core_index, rc);

	rc = cam_tfe_top_deinit(core_info->top_priv);
	kfree(core_info->top_priv);
	core_info->top_priv = NULL;

	if (rc)
		CAM_ERR(CAM_ISP, "Error cam_tfe_top_deinit failed rc=%d", rc);

	spin_unlock_irqrestore(&core_info->spin_lock, flags);

	return rc;
}
