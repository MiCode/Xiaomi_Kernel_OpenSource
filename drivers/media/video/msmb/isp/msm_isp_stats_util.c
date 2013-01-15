/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <linux/io.h>
#include <media/v4l2-subdev.h>
#include "msm_isp_util.h"
#include "msm_isp_stats_util.h"

void msm_isp_process_stats_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct timeval *tv)
{
	uint32_t frame_id;
	uint32_t stats_comp_mask = 0, stats_mask = 0;
	ISP_DBG("%s: status: 0x%x\n", __func__, irq_status0);
	stats_comp_mask = vfe_dev->hw_info->vfe_ops.stats_ops.
	get_comp_mask(irq_status0, irq_status1);
	stats_mask = vfe_dev->hw_info->vfe_ops.stats_ops.
		get_wm_mask(irq_status0, irq_status1);
	if (!(stats_comp_mask || stats_mask))
		return;
	frame_id = vfe_dev->hw_info->vfe_ops.stats_ops.get_frame_id(vfe_dev);
	/* TD: process comp/non comp stats */
}

int msm_isp_request_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0;
	/*To Do*/
	return rc;
}

int msm_isp_release_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0;
	struct msm_vfe_stats_stream_release_cmd *stream_release_cmd = arg;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;
	struct msm_vfe_stats_stream *stream_info =
		&stats_data->stream_info[
			(stream_release_cmd->stream_handle & 0xFF)];

	if (stream_info == NULL)
		rc = -1;
	return rc;
}

int msm_isp_cfg_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0;
	uint32_t stats_mask = 0;
	uint8_t enable = 0;
	uint32_t pingpong_status = 0;
	struct msm_isp_buffer *buf = NULL;
	enum msm_isp_stats_type stats_type = MSM_ISP_STATS_BE;
	vfe_dev->hw_info->vfe_ops.stats_ops.
	stats_enable(vfe_dev, stats_mask, enable);
	vfe_dev->hw_info->vfe_ops.stats_ops.
	update_ping_pong_addr(vfe_dev, stats_type,
		pingpong_status, buf->mapped_info[0].paddr);
	return rc;
}
