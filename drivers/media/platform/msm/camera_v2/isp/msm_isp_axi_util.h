/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2013-2018, 2020 The Linux Foundation. All rights reserved.
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
#ifndef __MSM_ISP_AXI_UTIL_H__
#define __MSM_ISP_AXI_UTIL_H__

#include "msm_isp.h"

#define HANDLE_TO_IDX(handle) (handle & 0xFF)
#define SRC_TO_INTF(src) \
	((src < RDI_INTF_0 || src == VFE_AXI_SRC_MAX) ? VFE_PIX_0 : \
	(VFE_RAW_0 + src - RDI_INTF_0))

int msm_isp_axi_check_stream_state(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd);

void msm_isp_reset_framedrop(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info);

int msm_isp_request_axi_stream(struct vfe_device *vfe_dev, void *arg);
void msm_isp_start_avtimer(void);
void msm_isp_stop_avtimer(void);
void msm_isp_get_avtimer_ts(struct msm_isp_timestamp *time_stamp);
int msm_isp_cfg_axi_stream(struct vfe_device *vfe_dev, void *arg);
int msm_isp_release_axi_stream(struct vfe_device *vfe_dev, void *arg);
int msm_isp_update_axi_stream(struct vfe_device *vfe_dev, void *arg);
void msm_isp_axi_cfg_update(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src);
int msm_isp_axi_halt(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_halt_cmd *halt_cmd);
int msm_isp_axi_reset(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_reset_cmd *reset_cmd);
int msm_isp_axi_restart(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_restart_cmd *restart_cmd);

void msm_isp_axi_stream_update(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src,
	struct msm_isp_timestamp *ts);

void msm_isp_process_reg_upd_epoch_irq(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src,
	enum msm_isp_comp_irq_types irq,
	struct msm_isp_timestamp *ts);

void msm_isp_notify(struct vfe_device *vfe_dev, uint32_t event_type,
	enum msm_vfe_input_src frame_src, struct msm_isp_timestamp *ts);

void msm_isp_process_axi_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1, uint32_t dual_irq_status,
	uint32_t pingpong_status, struct msm_isp_timestamp *ts);

void msm_isp_axi_disable_all_wm(struct vfe_device *vfe_dev);

int msm_isp_print_ping_pong_address(struct vfe_device *vfe_dev,
	unsigned long fault_addr);

void msm_isp_increment_frame_id(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src, struct msm_isp_timestamp *ts);

int msm_isp_drop_frame(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, struct msm_isp_timestamp *ts,
	struct msm_isp_sof_info *sof_info);

void msm_isp_halt(struct vfe_device *vfe_dev);
void msm_isp_halt_send_error(struct vfe_device *vfe_dev, uint32_t event);

void msm_isp_process_axi_irq_stream(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info,
	uint32_t pingpong_status,
	struct msm_isp_timestamp *ts);

void msm_isp_release_all_axi_stream(struct vfe_device *vfe_dev);
void msm_isp_axi_free_wm(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info);
void msm_isp_check_for_output_error(struct vfe_device *vfe_dev,
	struct msm_isp_timestamp *ts, struct msm_isp_sof_info *sof_info);

static inline int msm_isp_get_vfe_idx_for_stream_user(
				struct vfe_device *vfe_dev,
				struct msm_vfe_axi_stream *stream_info)
{
	int vfe_idx;

	for (vfe_idx = 0; vfe_idx < stream_info->num_isp; vfe_idx++) {
		if (stream_info->vfe_dev[vfe_idx] == vfe_dev)
			return vfe_idx;
	}
	return -ENOTTY;
}

static inline int msm_isp_get_vfe_idx_for_stream(struct vfe_device *vfe_dev,
				struct msm_vfe_axi_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stream_user(vfe_dev, stream_info);

	if (vfe_idx < 0) {
		WARN(1, "%s vfe index misssing for stream %d, vfe %d\n",
			__func__, stream_info->stream_src, vfe_dev->pdev->id);
		vfe_idx = 0;
	}
	return vfe_idx;
}

static inline void msm_isp_cfg_wm_scratch(struct vfe_device *vfe_dev,
				int wm,
				uint32_t pingpong_bit)
{
	vfe_dev->hw_info->vfe_ops.axi_ops.update_ping_pong_addr(
		vfe_dev->vfe_base, wm,
		pingpong_bit, vfe_dev->buf_mgr->scratch_buf_addr, 0);
}

static inline void msm_isp_cfg_stream_scratch(
				struct msm_vfe_axi_stream *stream_info,
				uint32_t pingpong_status)
{
	int i;
	int j;
	uint32_t pingpong_bit;
	int vfe_idx;

	pingpong_bit = (~(pingpong_status >> stream_info->wm[0][0]) & 0x1);
	for (i = 0; i < stream_info->num_planes; i++) {
		for (j = 0; j < stream_info->num_isp; j++) {
			vfe_idx = msm_isp_get_vfe_idx_for_stream(
					stream_info->vfe_dev[j], stream_info);
			msm_isp_cfg_wm_scratch(stream_info->vfe_dev[j],
				stream_info->wm[vfe_idx][i],
				~pingpong_bit);
		}
	}
	stream_info->buf[pingpong_bit] = NULL;
}

static inline struct msm_vfe_axi_stream *msm_isp_get_stream_common_data(
			struct vfe_device *vfe_dev, uint32_t stream_idx)
{
	struct msm_vfe_common_dev_data *common_data = vfe_dev->common_data;
	struct msm_vfe_axi_stream *stream_info;

	if (stream_idx >= VFE_AXI_SRC_MAX) {
		pr_err("invalid stream_idx %d\n", stream_idx);
		return NULL;
	}

	if (vfe_dev->is_split &&  stream_idx < RDI_INTF_0)
		stream_info = &common_data->streams[stream_idx];
	else
		stream_info = &common_data->streams[VFE_AXI_SRC_MAX *
				vfe_dev->pdev->id + stream_idx];
	return stream_info;
}

static inline struct msm_vfe_axi_stream *msm_isp_vfe_get_stream(
			struct dual_vfe_resource *dual_vfe_res,
			int vfe_id, uint32_t index)
{
	return msm_isp_get_stream_common_data(dual_vfe_res->vfe_dev[vfe_id],
						index);
}

int msm_isp_cfg_offline_ping_pong_address(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t pingpong_status,
	uint32_t buf_idx);
int msm_isp_ab_ib_update_lpm_mode(struct vfe_device *vfe_dev,
	void *arg);
#endif /* __MSM_ISP_AXI_UTIL_H__ */
