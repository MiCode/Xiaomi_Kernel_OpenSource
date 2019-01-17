/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */


#ifndef _MSM_CVP_COMMON_H_
#define _MSM_CVP_COMMON_H_
#include "msm_cvp_internal.h"

#define MAX_DEC_BATCH_SIZE                     6
#define MAX_DEC_BATCH_WIDTH                    1920
#define MAX_DEC_BATCH_HEIGHT                   1088
#define SKIP_BATCH_WINDOW                      100
#define MIN_FRAME_QUALITY 0
#define MAX_FRAME_QUALITY 100
#define DEFAULT_FRAME_QUALITY 80
#define FRAME_QUALITY_STEP 1
#define HEIC_GRID_DIMENSION 512
#define CBR_MB_LIMIT                           (((1280+15)/16)*((720+15)/16)*30)
#define CBR_VFR_MB_LIMIT                       (((640+15)/16)*((480+15)/16)*30)

struct vb2_buf_entry {
	struct list_head list;
	struct vb2_buffer *vb;
};

struct getprop_buf {
	struct list_head list;
	void *data;
};

extern const char *const mpeg_video_cvp_extradata[];

enum load_calc_quirks {
	LOAD_CALC_NO_QUIRKS = 0,
	LOAD_CALC_IGNORE_TURBO_LOAD = 1 << 0,
	LOAD_CALC_IGNORE_THUMBNAIL_LOAD = 1 << 1,
	LOAD_CALC_IGNORE_NON_REALTIME_LOAD = 1 << 2,
};

static inline bool is_turbo_session(struct msm_cvp_inst *inst)
{
	return !!(inst->flags & CVP_TURBO);
}

static inline bool is_thumbnail_session(struct msm_cvp_inst *inst)
{
	return !!(inst->flags & CVP_THUMBNAIL);
}

static inline bool is_low_power_session(struct msm_cvp_inst *inst)
{
	return !!(inst->flags & CVP_LOW_POWER);
}

static inline bool is_realtime_session(struct msm_cvp_inst *inst)
{
	return !!(inst->flags & CVP_REALTIME);
}

static inline bool is_decode_session(struct msm_cvp_inst *inst)
{
	return inst->session_type == MSM_CVP_DECODER;
}

static inline bool is_encode_session(struct msm_cvp_inst *inst)
{
	return inst->session_type == MSM_CVP_ENCODER;
}

static inline bool is_primary_output_mode(struct msm_cvp_inst *inst)
{
	return inst->stream_output_mode == HAL_VIDEO_DECODER_PRIMARY;
}

static inline bool is_secondary_output_mode(struct msm_cvp_inst *inst)
{
	return inst->stream_output_mode == HAL_VIDEO_DECODER_SECONDARY;
}

static inline int msm_comm_g_ctrl(struct msm_cvp_inst *inst,
		struct v4l2_control *ctrl)
{
	return v4l2_g_ctrl(&inst->ctrl_handler, ctrl);
}

static inline int msm_comm_s_ctrl(struct msm_cvp_inst *inst,
		struct v4l2_control *ctrl)
{
	return v4l2_s_ctrl(NULL, &inst->ctrl_handler, ctrl);
}
bool cvp_is_batching_allowed(struct msm_cvp_inst *inst);
enum hal_buffer cvp_get_hal_buffer_type(unsigned int type,
		unsigned int plane_num);
void cvp_put_inst(struct msm_cvp_inst *inst);
struct msm_cvp_inst *cvp_get_inst(struct msm_cvp_core *core,
		void *session_id);
void cvp_change_inst_state(struct msm_cvp_inst *inst,
		enum instance_state state);
struct msm_cvp_core *get_cvp_core(int core_id);
struct msm_cvp_format_constraint *msm_cvp_comm_get_pixel_fmt_constraints(
	struct msm_cvp_format_constraint fmt[], int size, int fourcc);
int msm_cvp_comm_set_color_format_constraints(struct msm_cvp_inst *inst,
		enum hal_buffer buffer_type,
		struct msm_cvp_format_constraint *pix_constraint);
struct buf_queue *msm_cvp_comm_get_vb2q(
		struct msm_cvp_inst *inst, enum v4l2_buf_type type);
int msm_cvp_comm_try_state(struct msm_cvp_inst *inst, int state);
int msm_cvp_comm_try_set_prop(struct msm_cvp_inst *inst,
	enum hal_property ptype, void *pdata);
int msm_cvp_comm_try_get_prop(struct msm_cvp_inst *inst,
	enum hal_property ptype, union hal_get_property *hprop);
int msm_cvp_comm_set_buffer_count(struct msm_cvp_inst *inst,
	int host_count, int act_count, enum hal_buffer type);
int msm_cvp_comm_queue_output_buffers(struct msm_cvp_inst *inst);
int msm_cvp_comm_qbuf(struct msm_cvp_inst *inst, struct msm_video_buffer *mbuf);
void msm_cvp_comm_flush_dynamic_buffers(struct msm_cvp_inst *inst);
int msm_cvp_comm_flush(struct msm_cvp_inst *inst, u32 flags);
int msm_cvp_comm_release_scratch_buffers(struct msm_cvp_inst *inst,
					bool check_for_reuse);
int msm_cvp_comm_release_persist_buffers(struct msm_cvp_inst *inst);
int msm_cvp_comm_release_recon_buffers(struct msm_cvp_inst *inst);
void msm_cvp_comm_release_eos_buffers(struct msm_cvp_inst *inst);
int msm_cvp_comm_release_output_buffers(struct msm_cvp_inst *inst,
	bool force_release);
void msm_cvp_comm_validate_output_buffers(struct msm_cvp_inst *inst);
int msm_cvp_comm_force_cleanup(struct msm_cvp_inst *inst);
int msm_cvp_comm_suspend(int core_id);
enum hal_extradata_id msm_cvp_comm_get_hal_extradata_index(
	enum v4l2_mpeg_cvp_extradata index);
int msm_cvp_comm_reset_bufreqs(struct msm_cvp_inst *inst,
	enum hal_buffer buf_type);
int msm_cvp_comm_copy_bufreqs(struct msm_cvp_inst *inst,
	enum hal_buffer src_type, enum hal_buffer dst_type);
struct hal_buffer_requirements *get_cvp_buff_req_buffer(
			struct msm_cvp_inst *inst, u32 buffer_type);
#define IS_PRIV_CTRL(idx) (\
		(V4L2_CTRL_ID2WHICH(idx) == V4L2_CTRL_CLASS_MPEG) && \
		V4L2_CTRL_DRIVER_PRIV(idx))
void msm_cvp_comm_session_clean(struct msm_cvp_inst *inst);
int msm_cvp_comm_kill_session(struct msm_cvp_inst *inst);
void msm_cvp_comm_generate_session_error(struct msm_cvp_inst *inst);
void msm_cvp_comm_generate_sys_error(struct msm_cvp_inst *inst);
enum multi_stream msm_cvp_comm_get_stream_output_mode(
		struct msm_cvp_inst *inst);
int msm_cvp_comm_set_stream_output_mode(struct msm_cvp_inst *inst,
		enum multi_stream mode);
enum hal_buffer msm_cvp_comm_get_hal_output_buffer(struct msm_cvp_inst *inst);
int msm_cvp_comm_smem_alloc(struct msm_cvp_inst *inst, size_t size, u32 align,
		u32 flags, enum hal_buffer buffer_type, int map_kernel,
		struct msm_smem *smem);
void msm_cvp_comm_smem_free(struct msm_cvp_inst *inst, struct msm_smem *smem);
int msm_cvp_comm_smem_cache_operations(struct msm_cvp_inst *inst,
		struct msm_smem *mem, enum smem_cache_ops cache_ops);
enum hal_video_codec get_cvp_hal_codec(int fourcc);
enum hal_domain get_cvp_hal_domain(int session_type);
int msm_cvp_comm_check_core_init(struct msm_cvp_core *core);
int msm_cvp_comm_get_inst_load(struct msm_cvp_inst *inst,
			enum load_calc_quirks quirks);
int msm_cvp_comm_get_inst_load_per_core(struct msm_cvp_inst *inst,
			enum load_calc_quirks quirks);
int msm_cvp_comm_get_load(struct msm_cvp_core *core,
			enum session_type type, enum load_calc_quirks quirks);
int msm_cvp_comm_set_color_format(struct msm_cvp_inst *inst,
		enum hal_buffer buffer_type, int fourcc);
int msm_comm_g_ctrl(struct msm_cvp_inst *inst, struct v4l2_control *ctrl);
int msm_comm_s_ctrl(struct msm_cvp_inst *inst, struct v4l2_control *ctrl);
int msm_cvp_comm_g_ctrl_for_id(struct msm_cvp_inst *inst, int id);
int msm_cvp_comm_ctrl_init(struct msm_cvp_inst *inst,
		struct msm_cvp_ctrl *drv_ctrls, u32 num_ctrls,
		const struct v4l2_ctrl_ops *ctrl_ops);
int msm_cvp_comm_ctrl_deinit(struct msm_cvp_inst *inst);
void msm_cvp_comm_cleanup_internal_buffers(struct msm_cvp_inst *inst);
int msm_cvp_comm_s_parm(struct msm_cvp_inst *inst, struct v4l2_streamparm *a);
bool msm_cvp_comm_turbo_session(struct msm_cvp_inst *inst);
void msm_cvp_comm_print_inst_info(struct msm_cvp_inst *inst);
int msm_cvp_comm_hal_to_v4l2(int id, int value);
int msm_cvp_comm_get_v4l2_profile(int fourcc, int profile);
int msm_cvp_comm_get_v4l2_level(int fourcc, int level);
int msm_cvp_comm_session_continue(void *instance);
enum hal_uncompressed_format msm_cvp_comm_get_hal_uncompressed(int fourcc);
u32 cvp_get_frame_size_nv12(int plane, u32 height, u32 width);
u32 cvp_get_frame_size_nv12_512(int plane, u32 height, u32 width);
u32 cvp_get_frame_size_nv12_ubwc(int plane, u32 height, u32 width);
u32 cvp_get_frame_size_rgba(int plane, u32 height, u32 width);
u32 cvp_get_frame_size_nv21(int plane, u32 height, u32 width);
u32 cvp_get_frame_size_tp10_ubwc(int plane, u32 height, u32 width);
u32 cvp_get_frame_size_p010(int plane, u32 height, u32 width);
struct vb2_buffer *msm_cvp_comm_get_vb_using_video_buffer(
		struct msm_cvp_inst *inst, struct msm_video_buffer *mbuf);
struct msm_video_buffer *msm_cvp_comm_get_buffer_using_device_planes(
		struct msm_cvp_inst *inst, u32 type, u32 *planes);
struct msm_video_buffer *msm_cvp_comm_get_video_buffer(
		struct msm_cvp_inst *inst, struct vb2_buffer *vb2);
void msm_cvp_comm_put_video_buffer(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf);
int msm_cvp_comm_vb2_buffer_done(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf);
int msm_cvp_comm_flush_video_buffer(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf);
int msm_cvp_comm_unmap_video_buffer(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf);
bool msm_cvp_comm_compare_dma_plane(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf,
		unsigned long *dma_planes, u32 i);
bool msm_cvp_comm_compare_dma_planes(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf, unsigned long *dma_planes);
bool msm_cvp_comm_compare_vb2_plane(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf, struct vb2_buffer *vb2, u32 i);
bool msm_cvp_comm_compare_vb2_planes(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf, struct vb2_buffer *vb2);
bool msm_cvp_comm_compare_device_plane(struct msm_video_buffer *mbuf,
		u32 type, u32 *planes, u32 i);
bool msm_cvp_comm_compare_device_planes(struct msm_video_buffer *mbuf,
		u32 type, u32 *planes);
int msm_cvp_comm_qbuf_cache_operations(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf);
int msm_cvp_comm_dqbuf_cache_operations(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf);
void print_video_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst, struct msm_video_buffer *mbuf);
void print_cvp_vb2_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst, struct vb2_buffer *vb2);
void print_cvp_v4l2_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst, struct v4l2_buffer *v4l2);
void kref_cvp_put_mbuf(struct msm_video_buffer *mbuf);
bool kref_cvp_get_mbuf(struct msm_cvp_inst *inst,
	struct msm_video_buffer *mbuf);
void msm_cvp_comm_store_mark_data(struct msm_cvp_list *data_list,
		u32 index, u32 mark_data, u32 mark_target);
void msm_cvp_comm_fetch_mark_data(struct msm_cvp_list *data_list,
		u32 index, u32 *mark_data, u32 *mark_target);
int msm_cvp_comm_release_mark_data(struct msm_cvp_inst *inst);
int msm_cvp_comm_qbuf_decode_batch(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf);
int msm_cvp_comm_num_queued_bufs(struct msm_cvp_inst *inst, u32 type);
int wait_for_sess_signal_receipt(struct msm_cvp_inst *inst,
	enum hal_command_response cmd);
#endif
