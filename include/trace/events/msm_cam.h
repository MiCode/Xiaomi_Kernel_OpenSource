/* Copyright (c) 2011-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM msm_cam

#if !defined(_TRACE_MSM_VFE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MSM_VFE_H

#include "msm_isp.h"
#include <linux/types.h>
#include <linux/tracepoint.h>

struct vfe_device;

TRACE_EVENT(msm_cam_isp_overflow,

	TP_PROTO(struct vfe_device *vfe_dev,
		unsigned int irq_status0,
		unsigned int irq_status1),

	TP_ARGS(vfe_dev, irq_status0, irq_status1),

	TP_STRUCT__entry(
		__field(unsigned int, device_id)
		__field(unsigned int, is_split)
		__field(unsigned int, irq_status0)
		__field(unsigned int, irq_status1)
		__field(unsigned int, msm_isp_last_overflow_ab)
		__field(unsigned int, msm_isp_last_overflow_ib)
		__field(unsigned int, msm_isp_vfe_clk_rate)
		__field(unsigned int, encoder_state)
		__field(unsigned int, encoder_width0)
		__field(unsigned int, encoder_height0)
		__field(unsigned int, encoder_width1)
		__field(unsigned int, encoder_height1)
		__field(unsigned int, viewfinder_state)
		__field(unsigned int, viewfinder_width0)
		__field(unsigned int, viewfinder_height0)
		__field(unsigned int, viewfinder_width1)
		__field(unsigned int, viewfinder_height1)
		__field(unsigned int, video_state)
		__field(unsigned int, video_width0)
		__field(unsigned int, video_height0)
		__field(unsigned int, video_width1)
		__field(unsigned int, video_height1)
		__field(unsigned int, camif_state)
		__field(unsigned int, camif_width0)
		__field(unsigned int, camif_height0)
		__field(unsigned int, camif_width1)
		__field(unsigned int, camif_height1)
		__field(unsigned int, ideal_state)
		__field(unsigned int, ideal_width0)
		__field(unsigned int, ideal_height0)
		__field(unsigned int, ideal_width1)
		__field(unsigned int, ideal_height1)
		__field(unsigned int, rdi0_state)
		__field(unsigned int, rdi0_width0)
		__field(unsigned int, rdi0_height0)
		__field(unsigned int, rdi0_width1)
		__field(unsigned int, rdi0_height1)
		__field(unsigned int, rdi1_state)
		__field(unsigned int, rdi1_width0)
		__field(unsigned int, rdi1_height0)
		__field(unsigned int, rdi1_width1)
		__field(unsigned int, rdi1_height1)
		__field(unsigned int, rdi2_state)
		__field(unsigned int, rdi2_width0)
		__field(unsigned int, rdi2_height0)
		__field(unsigned int, rdi2_width1)
		__field(unsigned int, rdi2_height1)
		),

	TP_fast_assign(
		__entry->device_id = vfe_dev->pdev->id;
		__entry->is_split = vfe_dev->is_split;
		__entry->irq_status0 = irq_status0;
		__entry->irq_status1 = irq_status1;
		__entry->msm_isp_last_overflow_ab =
			vfe_dev->msm_isp_last_overflow_ab;
		__entry->msm_isp_last_overflow_ib =
			vfe_dev->msm_isp_last_overflow_ib;
		__entry->msm_isp_vfe_clk_rate = vfe_dev->msm_isp_vfe_clk_rate;
		__entry->encoder_state = vfe_dev->axi_data.stream_info[0].state;
		__entry->encoder_width0 = vfe_dev->axi_data.stream_info[0].
			plane_cfg[0].output_width;
		__entry->encoder_height0 = vfe_dev->axi_data.stream_info[0].
			plane_cfg[0].output_height;
		__entry->encoder_width1 = vfe_dev->axi_data.stream_info[0].
			plane_cfg[1].output_width;
		__entry->encoder_height1 = vfe_dev->axi_data.stream_info[0].
			plane_cfg[1].output_height;
		__entry->viewfinder_state =
			vfe_dev->axi_data.stream_info[1].state;
		__entry->viewfinder_width0 = vfe_dev->axi_data.stream_info[1].
			plane_cfg[0].output_width;
		__entry->viewfinder_height0 = vfe_dev->axi_data.stream_info[1].
			plane_cfg[0].output_height;
		__entry->viewfinder_width1 = vfe_dev->axi_data.stream_info[1].
			plane_cfg[1].output_width;
		__entry->viewfinder_height1 = vfe_dev->axi_data.stream_info[1].
			plane_cfg[1].output_height;
		__entry->video_state = vfe_dev->axi_data.stream_info[2].state;
		__entry->video_width0 = vfe_dev->axi_data.stream_info[2].
			plane_cfg[0].output_width;
		__entry->video_height0 = vfe_dev->axi_data.stream_info[2].
			plane_cfg[0].output_height;
		__entry->video_width1 = vfe_dev->axi_data.stream_info[2].
			plane_cfg[1].output_width;
		__entry->video_height1 = vfe_dev->axi_data.stream_info[2].
			plane_cfg[1].output_height;
		__entry->camif_state = vfe_dev->axi_data.stream_info[3].state;
		__entry->camif_width0 = vfe_dev->axi_data.stream_info[3].
			plane_cfg[0].output_width;
		__entry->camif_height0 = vfe_dev->axi_data.stream_info[3].
			plane_cfg[0].output_height;
		__entry->camif_width1 = vfe_dev->axi_data.stream_info[3].
			plane_cfg[1].output_width;
		__entry->camif_height1 = vfe_dev->axi_data.stream_info[3].
			plane_cfg[1].output_height;
		__entry->ideal_state = vfe_dev->axi_data.stream_info[0].state;
		__entry->ideal_width0 = vfe_dev->axi_data.stream_info[0].
			plane_cfg[0].output_width;
		__entry->ideal_height0 = vfe_dev->axi_data.stream_info[0].
			plane_cfg[0].output_height;
		__entry->ideal_width1 = vfe_dev->axi_data.stream_info[0].
			plane_cfg[1].output_width;
		__entry->ideal_height1 = vfe_dev->axi_data.stream_info[0].
			plane_cfg[1].output_height;
		__entry->rdi0_state = vfe_dev->axi_data.stream_info[1].state;
		__entry->rdi0_width0 = vfe_dev->axi_data.stream_info[1].
			plane_cfg[0].output_width;
		__entry->rdi0_height0 = vfe_dev->axi_data.stream_info[1].
			plane_cfg[0].output_height;
		__entry->rdi0_width1 = vfe_dev->axi_data.stream_info[1].
			plane_cfg[1].output_width;
		__entry->rdi0_height1 = vfe_dev->axi_data.stream_info[1].
			plane_cfg[1].output_height;
		__entry->rdi1_state = vfe_dev->axi_data.stream_info[2].state;
		__entry->rdi1_width0 = vfe_dev->axi_data.stream_info[2].
			plane_cfg[0].output_width;
		__entry->rdi1_height0 = vfe_dev->axi_data.stream_info[2].
			plane_cfg[0].output_height;
		__entry->rdi1_width1 = vfe_dev->axi_data.stream_info[2].
			plane_cfg[1].output_width;
		__entry->rdi1_height1 = vfe_dev->axi_data.stream_info[2].
			plane_cfg[1].output_height;
		__entry->rdi2_state = vfe_dev->axi_data.stream_info[3].state;
		__entry->rdi2_width0 = vfe_dev->axi_data.stream_info[3].
			plane_cfg[0].output_width;
		__entry->rdi2_height0 = vfe_dev->axi_data.stream_info[3].
			plane_cfg[0].output_height;
		__entry->rdi2_width1 = vfe_dev->axi_data.stream_info[3].
			plane_cfg[1].output_width;
		__entry->rdi2_height1 = vfe_dev->axi_data.stream_info[3].
			plane_cfg[1].output_height;
	),

	TP_printk(
		"Overflow detected id=%d is_split=%d irq0 = %d irq1 = %d\n"
		"ab_bw %d ib_bw %d clock %d\n"
		"Encoder src state %d plane0_width %d plane0_ht %d\n"
		"Encoder src plane1_width %d plane1_ht %d\n"
		"viewfinder src state %d plane0_width %d plane0_ht %d\n"
		"viewfinder src plane1_width %d plane1_ht %d\n"
		"Video src state %d plane0_width %d plane0_ht %d\n"
		"Video src plane1_width %d plane1_ht %d\n"
		"Camif src state %d plane0_width %d plane0_ht %d\n"
		"Camif src plane1_width %d plane1_ht %d\n"
		"Ideal_raw src state %d plane0_width %d plane0_ht %d\n"
		"Ideal_raw plane1_width %d plane1_ht %d\n"
		"RDI0 src state %d plane0_width %d plane0_ht %d\n"
		"RDI0 plane1_width %d plane1_ht %d\n"
		"RDI1 src state %d plane0_width %d plane0_ht %d\n"
		"RDI1 plane1_width %d plane1_ht %d\n"
		"RDI2 src state %d plane0_width %d plane0_ht %d\n"
		"RDI2 plane1_width %d plane1_ht %d\n",
		__entry->device_id, __entry->is_split,
		__entry->irq_status0, __entry->irq_status1,
		__entry->msm_isp_last_overflow_ab,
		__entry->msm_isp_last_overflow_ib,
		__entry->msm_isp_vfe_clk_rate,
		__entry->encoder_state,
		__entry->encoder_width0, __entry->encoder_height0,
		__entry->encoder_width1, __entry->encoder_height1,
		__entry->viewfinder_state,
		__entry->viewfinder_width0, __entry->viewfinder_height0,
		__entry->viewfinder_width1, __entry->viewfinder_height1,
		__entry->video_state,
		__entry->video_width0, __entry->video_height0,
		__entry->video_width1, __entry->video_height1,
		__entry->camif_state,
		__entry->camif_width0, __entry->camif_height0,
		__entry->camif_width1, __entry->camif_height1,
		__entry->ideal_state,
		__entry->ideal_width0, __entry->ideal_height0,
		__entry->ideal_width1, __entry->ideal_height1,
		__entry->rdi0_state,
		__entry->rdi0_width0, __entry->rdi0_height0,
		__entry->rdi0_width1, __entry->rdi0_height1,
		__entry->rdi1_state,
		__entry->rdi1_width0, __entry->rdi1_height0,
		__entry->rdi1_width1, __entry->rdi1_height1,
		__entry->rdi2_state,
		__entry->rdi2_width0, __entry->rdi2_height0,
		__entry->rdi2_width1, __entry->rdi2_height1
	)
);

#endif /* _MSM_CAM_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
