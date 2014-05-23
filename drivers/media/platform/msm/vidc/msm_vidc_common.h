/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_VIDC_COMMON_H_
#define _MSM_VIDC_COMMON_H_
#include "msm_vidc_internal.h"
struct vb2_buf_entry {
	struct list_head list;
	struct vb2_buffer *vb;
};
struct msm_vidc_core *get_vidc_core(int core_id);
const struct msm_vidc_format *msm_comm_get_pixel_fmt_index(
	const struct msm_vidc_format fmt[], int size, int index, int fmt_type);
struct msm_vidc_format *msm_comm_get_pixel_fmt_fourcc(
	struct msm_vidc_format fmt[], int size, int fourcc, int fmt_type);
struct buf_queue *msm_comm_get_vb2q(
		struct msm_vidc_inst *inst, enum v4l2_buf_type type);
int msm_comm_try_state(struct msm_vidc_inst *inst, int state);
int msm_comm_try_get_bufreqs(struct msm_vidc_inst *inst);
int msm_comm_try_set_prop(struct msm_vidc_inst *inst,
	enum hal_property ptype, void *pdata);
int msm_comm_set_scratch_buffers(struct msm_vidc_inst *inst);
int msm_comm_set_persist_buffers(struct msm_vidc_inst *inst);
int msm_comm_set_output_buffers(struct msm_vidc_inst *inst);
int msm_comm_queue_output_buffers(struct msm_vidc_inst *inst);
int msm_comm_qbuf(struct vb2_buffer *vb);
void msm_comm_scale_clocks_and_bus(struct msm_vidc_inst *inst);
int msm_comm_flush(struct msm_vidc_inst *inst, u32 flags);
int msm_comm_release_scratch_buffers(struct msm_vidc_inst *inst);
int msm_comm_release_persist_buffers(struct msm_vidc_inst *inst);
int msm_comm_release_output_buffers(struct msm_vidc_inst *inst);
int msm_comm_force_cleanup(struct msm_vidc_inst *inst);
enum hal_extradata_id msm_comm_get_hal_extradata_index(
	enum v4l2_mpeg_vidc_extradata index);
int msm_comm_get_domain_partition(struct msm_vidc_inst *inst, u32 flags,
	enum v4l2_buf_type buf_type, int *domain, int *partition);
struct hal_buffer_requirements *get_buff_req_buffer(
			struct msm_vidc_inst *inst, u32 buffer_type);
#define IS_PRIV_CTRL(idx) (\
		(V4L2_CTRL_ID2CLASS(idx) == V4L2_CTRL_CLASS_MPEG) && \
		V4L2_CTRL_DRIVER_PRIV(idx))

int msm_comm_check_scaling_supported(struct msm_vidc_inst *inst);
int msm_comm_recover_from_session_error(struct msm_vidc_inst *inst);
enum multi_stream msm_comm_get_stream_output_mode(struct msm_vidc_inst *inst);
enum hal_buffer msm_comm_get_hal_output_buffer(struct msm_vidc_inst *inst);
struct msm_smem *msm_comm_smem_alloc(struct msm_vidc_inst *inst,
			size_t size, u32 align, u32 flags,
			enum hal_buffer buffer_type, int map_kernel);
void msm_comm_smem_free(struct msm_vidc_inst *inst, struct msm_smem *mem);
int msm_comm_smem_cache_operations(struct msm_vidc_inst *inst,
		struct msm_smem *mem, enum smem_cache_ops cache_ops);
struct msm_smem *msm_comm_smem_user_to_kernel(struct msm_vidc_inst *inst,
			int fd, u32 offset, enum hal_buffer buffer_type);
int msm_comm_smem_get_domain_partition(struct msm_vidc_inst *inst,
			u32 flags, enum hal_buffer buffer_type,
			int *domain_num, int *partition_num);
enum hal_video_codec get_hal_codec_type(int fourcc);
#endif
