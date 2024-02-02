/*
 * Copyright (c) 2012-2016, 2018 The Linux Foundation. All rights reserved.
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

#ifndef _MSM_VIDC_COMMON_H_
#define _MSM_VIDC_COMMON_H_
#include "msm_vidc_internal.h"
struct vb2_buf_entry {
	struct list_head list;
	struct vb2_buffer *vb;
};

extern const char *const mpeg_video_vidc_extradata[];

enum load_calc_quirks {
	LOAD_CALC_NO_QUIRKS = 0,
	LOAD_CALC_IGNORE_TURBO_LOAD = 1 << 0,
	LOAD_CALC_IGNORE_THUMBNAIL_LOAD = 1 << 1,
	LOAD_CALC_IGNORE_NON_REALTIME_LOAD = 1 << 2,
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
int msm_comm_try_get_prop(struct msm_vidc_inst *inst,
	enum hal_property ptype, union hal_get_property *hprop);
int msm_comm_set_scratch_buffers(struct msm_vidc_inst *inst);
int msm_comm_set_persist_buffers(struct msm_vidc_inst *inst);
int msm_comm_set_output_buffers(struct msm_vidc_inst *inst);
int msm_comm_queue_output_buffers(struct msm_vidc_inst *inst);
int msm_comm_qbuf(struct msm_vidc_inst *inst, struct vb2_buffer *vb);
void msm_comm_scale_clocks_and_bus(struct msm_vidc_inst *inst);
int msm_comm_scale_clocks(struct msm_vidc_core *core);
int msm_comm_scale_clocks_load(struct msm_vidc_core *core,
		int num_mbs_per_sec, enum load_calc_quirks quirks);
void msm_comm_flush_dynamic_buffers(struct msm_vidc_inst *inst);
int msm_comm_flush(struct msm_vidc_inst *inst, u32 flags);
int msm_comm_release_scratch_buffers(struct msm_vidc_inst *inst,
					bool check_for_reuse);
int msm_comm_release_persist_buffers(struct msm_vidc_inst *inst);
void msm_comm_release_eos_buffers(struct msm_vidc_inst *inst);
int msm_comm_release_output_buffers(struct msm_vidc_inst *inst);
int msm_comm_force_cleanup(struct msm_vidc_inst *inst);
int msm_comm_suspend(int core_id);
enum hal_extradata_id msm_comm_get_hal_extradata_index(
	enum v4l2_mpeg_vidc_extradata index);
enum hal_buffer_layout_type msm_comm_get_hal_buffer_layout(
	enum v4l2_mpeg_vidc_video_mvc_layout index);
struct hal_buffer_requirements *get_buff_req_buffer(
			struct msm_vidc_inst *inst, u32 buffer_type);
#define IS_PRIV_CTRL(idx) (\
		(V4L2_CTRL_ID2WHICH(idx) == V4L2_CTRL_CLASS_MPEG) && \
		V4L2_CTRL_DRIVER_PRIV(idx))
void msm_comm_session_clean(struct msm_vidc_inst *inst);
int msm_comm_kill_session(struct msm_vidc_inst *inst);
enum multi_stream msm_comm_get_stream_output_mode(struct msm_vidc_inst *inst);
enum hal_buffer msm_comm_get_hal_output_buffer(struct msm_vidc_inst *inst);
int msm_comm_smem_alloc(struct msm_vidc_inst *inst,
			size_t size, u32 align, u32 flags,
			enum hal_buffer buffer_type, int map_kernel,
			struct msm_smem *smem);
void msm_comm_smem_free(struct msm_vidc_inst *inst, struct msm_smem *mem);
int msm_comm_smem_cache_operations(struct msm_vidc_inst *inst,
		struct msm_smem *mem, enum smem_cache_ops cache_ops);
enum hal_video_codec get_hal_codec(int fourcc);
enum hal_domain get_hal_domain(int session_type);
int msm_comm_check_core_init(struct msm_vidc_core *core);
int msm_comm_get_inst_load(struct msm_vidc_inst *inst,
			enum load_calc_quirks quirks);
int msm_comm_get_load(struct msm_vidc_core *core,
			enum session_type type, enum load_calc_quirks quirks);
int msm_comm_set_color_format(struct msm_vidc_inst *inst,
		enum hal_buffer buffer_type, int fourcc);
int msm_comm_g_ctrl(struct msm_vidc_inst *inst, struct v4l2_control *ctrl);
int msm_comm_s_ctrl(struct msm_vidc_inst *inst, struct v4l2_control *ctrl);
int msm_comm_g_ctrl_for_id(struct msm_vidc_inst *inst, int id);
int msm_comm_ctrl_init(struct msm_vidc_inst *inst,
		struct msm_vidc_ctrl *drv_ctrls, u32 num_ctrls,
		const struct v4l2_ctrl_ops *ctrl_ops);
int msm_comm_ctrl_deinit(struct msm_vidc_inst *inst);
void msm_comm_cleanup_internal_buffers(struct msm_vidc_inst *inst);
int msm_vidc_comm_s_parm(struct msm_vidc_inst *inst, struct v4l2_streamparm *a);
bool msm_comm_turbo_session(struct msm_vidc_inst *inst);
struct msm_vidc_inst *get_inst(struct msm_vidc_core *core, void *session_id);
void put_inst(struct msm_vidc_inst *inst);
#endif
