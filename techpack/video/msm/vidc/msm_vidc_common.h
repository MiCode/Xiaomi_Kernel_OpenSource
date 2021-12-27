/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_VIDC_COMMON_H_
#define _MSM_VIDC_COMMON_H_
#include "msm_vidc_internal.h"
#include "msm_vidc_debug.h"

#define MAX_DEC_BATCH_SIZE                     6
#define SKIP_BATCH_WINDOW                      100
#define MIN_FRAME_QUALITY 0
#define MAX_FRAME_QUALITY 100
#define DEFAULT_FRAME_QUALITY 95
#define FRAME_QUALITY_STEP 1
#define HEIC_GRID_DIMENSION 512
#define CBR_MB_LIMIT                           (((1280+15)/16)*((720+15)/16)*30)
#define CBR_VFR_MB_LIMIT                       (((640+15)/16)*((480+15)/16)*30)
#define V4L2_CID_MPEG_VIDEO_UNKNOWN (V4L2_CID_MPEG_MSM_VIDC_BASE + 0xFFF)
#define MAX_BITRATE_DECODER_CAVLC              220000000
#define MAX_BITRATE_DECODER_2STAGE_CABAC       200000000
#define MAX_BITRATE_DECODER_1STAGE_CABAC        70000000

struct vb2_buf_entry {
	struct list_head list;
	struct vb2_buffer *vb;
};

struct getprop_buf {
	struct list_head list;
	void *data;
};

enum load_calc_quirks {
	LOAD_POWER = 0,
	LOAD_ADMISSION_CONTROL = 1,
};

enum client_set_controls {
	CLIENT_SET_I_QP = 0x1,
	CLIENT_SET_P_QP = 0x2,
	CLIENT_SET_B_QP = 0x4,
	CLIENT_SET_MIN_QP = 0x8,
	CLIENT_SET_MAX_QP = 0x10,
};

static inline bool is_turbo_session(struct msm_vidc_inst *inst)
{
	return !!(inst->flags & VIDC_TURBO);
}

static inline bool is_thumbnail_session(struct msm_vidc_inst *inst)
{
	return !!(inst->flags & VIDC_THUMBNAIL);
}

static inline bool is_low_power_session(struct msm_vidc_inst *inst)
{
	return !!(inst->flags & VIDC_LOW_POWER);
}

static inline struct v4l2_ctrl *get_ctrl(struct msm_vidc_inst *inst,
	u32 id)
{
	int i;

	if (inst->session_type == MSM_VIDC_CVP &&
	    inst->core->resources.cvp_internal)
		return inst->ctrls[0];

	for (i = 0; i < inst->num_ctrls; i++) {
		if (inst->ctrls[i]->id == id)
			return inst->ctrls[i];
	}
	s_vpr_e(inst->sid, "%s: control id (%#x) not found\n", __func__, id);
	MSM_VIDC_ERROR(true);
	return inst->ctrls[0];
}

static inline void update_ctrl(struct v4l2_ctrl *ctrl, s32 val, u32 sid)
{
	switch (ctrl->type) {
	case V4L2_CTRL_TYPE_INTEGER:
		*ctrl->p_cur.p_s32 = val;
		memcpy(ctrl->p_new.p, ctrl->p_cur.p,
			ctrl->elems * ctrl->elem_size);
		break;
	default:
		s_vpr_e(sid, "unhandled control type");
	}
}

static inline u32 get_v4l2_codec(struct msm_vidc_inst *inst)
{
	struct v4l2_format *f;
	u32 port;

	port = (inst->session_type == MSM_VIDC_DECODER) ? INPUT_PORT :
		OUTPUT_PORT;
	f = &inst->fmts[port].v4l2_fmt;
	return f->fmt.pix_mp.pixelformat;
}

static inline bool is_image_session(struct msm_vidc_inst *inst)
{
	/* Grid may or may not be enabled for an image encode session */
	return inst->session_type == MSM_VIDC_ENCODER &&
		get_v4l2_codec(inst) == V4L2_PIX_FMT_HEVC &&
		inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ;
}

static inline bool is_grid_session(struct msm_vidc_inst *inst)
{
	struct v4l2_ctrl *ctrl = NULL;
	if (inst->session_type == MSM_VIDC_ENCODER &&
		get_v4l2_codec(inst) == V4L2_PIX_FMT_HEVC) {
		ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_IMG_GRID_SIZE);
		return (ctrl->val > 0);
	}
	return 0;
}

static inline bool is_video_session(struct msm_vidc_inst *inst)
{
	return !is_grid_session(inst);
}
static inline bool is_realtime_session(struct msm_vidc_inst *inst)
{
	struct v4l2_ctrl *ctrl;
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY);
	return !!ctrl->val;
}

static inline bool is_low_latency_hint(struct msm_vidc_inst *inst)
{
	struct v4l2_ctrl *ctrl;

	if (inst->session_type != MSM_VIDC_DECODER)
		return false;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_HINT);
	return !!ctrl->val;
}

static inline bool is_secure_session(struct msm_vidc_inst *inst)
{
	return !!(inst->flags & VIDC_SECURE);
}

static inline bool is_decode_session(struct msm_vidc_inst *inst)
{
	return inst->session_type == MSM_VIDC_DECODER;
}

static inline bool is_encode_session(struct msm_vidc_inst *inst)
{
	return inst->session_type == MSM_VIDC_ENCODER;
}

static inline bool is_primary_output_mode(struct msm_vidc_inst *inst)
{
	return inst->stream_output_mode == HAL_VIDEO_DECODER_PRIMARY;
}

static inline bool is_secondary_output_mode(struct msm_vidc_inst *inst)
{
	return inst->stream_output_mode == HAL_VIDEO_DECODER_SECONDARY;
}

static inline bool in_port_reconfig(struct msm_vidc_inst *inst)
{
	return inst->in_reconfig && inst->bufq[INPUT_PORT].vb2_bufq.streaming;
}

static inline bool is_input_buffer(struct msm_vidc_buffer *mbuf)
{
	return mbuf->vvb.vb2_buf.type == INPUT_MPLANE;
}

static inline bool is_output_buffer(struct msm_vidc_buffer *mbuf)
{
	return mbuf->vvb.vb2_buf.type == OUTPUT_MPLANE;
}

static inline bool is_internal_buffer(enum hal_buffer type)
{
	u32 buf_type =
		HAL_BUFFER_INTERNAL_SCRATCH |
		HAL_BUFFER_INTERNAL_SCRATCH_1 |
		HAL_BUFFER_INTERNAL_SCRATCH_2 |
		HAL_BUFFER_INTERNAL_PERSIST |
		HAL_BUFFER_INTERNAL_PERSIST_1 |
		HAL_BUFFER_INTERNAL_RECON;
	return !!(buf_type & type);
}

static inline int msm_comm_g_ctrl(struct msm_vidc_inst *inst,
		struct v4l2_control *ctrl)
{
	return v4l2_g_ctrl(&inst->ctrl_handler, ctrl);
}

static inline int msm_comm_s_ctrl(struct msm_vidc_inst *inst,
		struct v4l2_control *ctrl)
{
	return v4l2_s_ctrl(NULL, &inst->ctrl_handler, ctrl);
}

static inline bool is_valid_operating_rate(struct msm_vidc_inst *inst, s32 val)
{
	struct hal_capability_supported *cap;

	cap = &inst->capability.cap[CAP_OPERATINGRATE];

	if (((val >> 16) < cap->min || (val >> 16) > cap->max) &&
		val != INT_MAX) {
		s_vpr_e(inst->sid,
			"Unsupported operating rate %d min %d max %d\n",
			val >> 16, cap->min, cap->max);
		return false;
	}
	return true;
}

bool is_single_session(struct msm_vidc_inst *inst, u32 ignore_flags);
int msm_comm_get_num_perf_sessions(struct msm_vidc_inst *inst);
bool is_batching_allowed(struct msm_vidc_inst *inst);
enum hal_buffer get_hal_buffer_type(unsigned int type,
		unsigned int plane_num);
void put_inst(struct msm_vidc_inst *inst);
struct msm_vidc_inst *get_inst(struct msm_vidc_core *core,
		void *inst_id);
void change_inst_state(struct msm_vidc_inst *inst, enum instance_state state);
struct msm_vidc_core *get_vidc_core(int core_id);
const struct msm_vidc_format_desc *msm_comm_get_pixel_fmt_index(
	const struct msm_vidc_format_desc fmt[], int size, int index, u32 sid);
struct msm_vidc_format_desc *msm_comm_get_pixel_fmt_fourcc(
	struct msm_vidc_format_desc fmt[], int size, int fourcc, u32 sid);
struct msm_vidc_format_constraint *msm_comm_get_pixel_fmt_constraints(
	struct msm_vidc_format_constraint fmt[], int size, int fourcc, u32 sid);
int msm_comm_set_color_format_constraints(struct msm_vidc_inst *inst,
		enum hal_buffer buffer_type,
		struct msm_vidc_format_constraint *pix_constraint);
struct buf_queue *msm_comm_get_vb2q(
		struct msm_vidc_inst *inst, enum v4l2_buf_type type);
int msm_comm_try_state(struct msm_vidc_inst *inst, int state);
int msm_comm_try_get_bufreqs(struct msm_vidc_inst *inst);
int msm_comm_try_get_buff_req(struct msm_vidc_inst *inst,
	union hal_get_property *hprop);
int msm_comm_set_recon_buffers(struct msm_vidc_inst *inst);
int msm_comm_set_scratch_buffers(struct msm_vidc_inst *inst);
int msm_comm_set_persist_buffers(struct msm_vidc_inst *inst);
int msm_comm_set_buffer_count(struct msm_vidc_inst *inst,
	int host_count, int act_count, enum hal_buffer type);
int msm_comm_set_dpb_only_buffers(struct msm_vidc_inst *inst);
int msm_comm_queue_dpb_only_buffers(struct msm_vidc_inst *inst);
int msm_comm_qbuf(struct msm_vidc_inst *inst, struct msm_vidc_buffer *mbuf);
int msm_comm_qbufs(struct msm_vidc_inst *inst);
void msm_comm_flush_dynamic_buffers(struct msm_vidc_inst *inst);
int msm_comm_flush(struct msm_vidc_inst *inst, u32 flags);
int msm_comm_release_scratch_buffers(struct msm_vidc_inst *inst,
					bool check_for_reuse);
int msm_comm_release_persist_buffers(struct msm_vidc_inst *inst);
int msm_comm_release_recon_buffers(struct msm_vidc_inst *inst);
void msm_comm_release_eos_buffers(struct msm_vidc_inst *inst);
int msm_comm_release_dpb_only_buffers(struct msm_vidc_inst *inst,
	bool force_release);
void msm_comm_validate_output_buffers(struct msm_vidc_inst *inst);
int msm_comm_force_cleanup(struct msm_vidc_inst *inst);
int msm_comm_suspend(int core_id);
int msm_comm_reset_bufreqs(struct msm_vidc_inst *inst,
	enum hal_buffer buf_type);
struct hal_buffer_requirements *get_buff_req_buffer(
			struct msm_vidc_inst *inst, u32 buffer_type);
#define IS_PRIV_CTRL(idx) (\
		(V4L2_CTRL_ID2WHICH(idx) == V4L2_CTRL_CLASS_MPEG) && \
		V4L2_CTRL_DRIVER_PRIV(idx))
void msm_comm_session_clean(struct msm_vidc_inst *inst);
int msm_comm_kill_session(struct msm_vidc_inst *inst);
void msm_comm_generate_session_error(struct msm_vidc_inst *inst);
void msm_comm_generate_sys_error(struct msm_vidc_inst *inst);
enum multi_stream msm_comm_get_stream_output_mode(struct msm_vidc_inst *inst);
int msm_comm_set_stream_output_mode(struct msm_vidc_inst *inst,
		enum multi_stream mode);
enum hal_buffer msm_comm_get_hal_output_buffer(struct msm_vidc_inst *inst);
int msm_comm_smem_alloc(struct msm_vidc_inst *inst, size_t size, u32 align,
		u32 flags, enum hal_buffer buffer_type, int map_kernel,
		struct msm_smem *smem);
void msm_comm_smem_free(struct msm_vidc_inst *inst, struct msm_smem *smem);
int msm_comm_smem_cache_operations(struct msm_vidc_inst *inst,
		struct msm_smem *mem, enum smem_cache_ops cache_ops);
enum hal_video_codec get_hal_codec(int fourcc, u32 sid);
enum hal_domain get_hal_domain(int session_type, u32 sid);
int msm_comm_check_core_init(struct msm_vidc_core *core, u32 sid);
int msm_comm_get_inst_load(struct msm_vidc_inst *inst,
			enum load_calc_quirks quirks);
int msm_comm_get_inst_load_per_core(struct msm_vidc_inst *inst,
			enum load_calc_quirks quirks);
int msm_comm_get_device_load(struct msm_vidc_core *core,
			enum session_type sess_type,
			enum load_type load_type,
			enum load_calc_quirks quirks);
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
bool msm_comm_turbo_session(struct msm_vidc_inst *inst);
void msm_comm_print_inst_info(struct msm_vidc_inst *inst);
void msm_comm_print_insts_info(struct msm_vidc_core *core);
int msm_comm_v4l2_to_hfi(int id, int value, u32 sid);
int msm_comm_hfi_to_v4l2(int id, int value, u32 sid);
int msm_comm_get_v4l2_profile(int fourcc, int profile, u32 sid);
int msm_comm_get_v4l2_level(int fourcc, int level, u32 sid);
int msm_comm_session_continue(void *instance);
int msm_vidc_send_pending_eos_buffers(struct msm_vidc_inst *inst);
enum hal_uncompressed_format msm_comm_get_hal_uncompressed(int fourcc);
u32 msm_comm_get_hfi_uncompressed(int fourcc, u32 sid);
u32 msm_comm_convert_color_fmt(u32 v4l2_fmt, u32 sid);
struct vb2_buffer *msm_comm_get_vb_using_vidc_buffer(
		struct msm_vidc_inst *inst, struct msm_vidc_buffer *mbuf);
struct msm_vidc_buffer *msm_comm_get_buffer_using_device_planes(
		struct msm_vidc_inst *inst, u32 type, u32 *planes);
struct msm_vidc_buffer *msm_comm_get_vidc_buffer(struct msm_vidc_inst *inst,
		struct vb2_buffer *vb2);
void msm_comm_put_vidc_buffer(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf);
void handle_release_buffer_reference(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf);
int msm_comm_vb2_buffer_done(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf);
int msm_comm_flush_vidc_buffer(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf);
int msm_comm_unmap_vidc_buffer(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf);
bool msm_comm_compare_dma_plane(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf, unsigned long *dma_planes, u32 i);
bool msm_comm_compare_dma_planes(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf, unsigned long *dma_planes);
bool msm_comm_compare_vb2_plane(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf, struct vb2_buffer *vb2, u32 i);
bool msm_comm_compare_vb2_planes(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf, struct vb2_buffer *vb2);
bool msm_comm_compare_device_plane(u32 sid, struct msm_vidc_buffer *mbuf,
		u32 type, u32 *planes, u32 i);
bool msm_comm_compare_device_planes(u32 sid, struct msm_vidc_buffer *mbuf,
		u32 type, u32 *planes);
int msm_comm_qbuf_cache_operations(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf);
int msm_comm_dqbuf_cache_operations(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf);
void print_vidc_buffer(u32 tag, const char *str, struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf);
void print_vb2_buffer(const char *str, struct msm_vidc_inst *inst,
		struct vb2_buffer *vb2);
void kref_put_mbuf(struct msm_vidc_buffer *mbuf);
bool kref_get_mbuf(struct msm_vidc_inst *inst, struct msm_vidc_buffer *mbuf);
int msm_comm_store_input_tag(struct msm_vidc_list *data_list,
		u32 index, u32 itag, u32 itag2, u32 sid);
int msm_comm_fetch_input_tag(struct msm_vidc_list *data_list,
		u32 index, u32 *itag, u32 *itag2, u32 sid);
int msm_comm_release_input_tag(struct msm_vidc_inst *inst);
int msm_comm_qbufs_batch(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf);
int msm_comm_qbuf_decode_batch(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf);
int schedule_batch_work(struct msm_vidc_inst *inst);
int cancel_batch_work(struct msm_vidc_inst *inst);
int msm_comm_num_queued_bufs(struct msm_vidc_inst *inst, u32 type);
int msm_comm_set_index_extradata(struct msm_vidc_inst *inst,
		uint32_t extradata_id, uint32_t value);
int msm_comm_set_extradata(struct msm_vidc_inst *inst, uint32_t extradata_id,
		uint32_t value);
bool msm_comm_check_for_inst_overload(struct msm_vidc_core *core);
void msm_vidc_batch_handler(struct work_struct *work);
int msm_comm_check_window_bitrate(struct msm_vidc_inst *inst,
		struct vidc_frame_data *frame_data);
void msm_comm_clear_window_data(struct msm_vidc_inst *inst);
void msm_comm_release_window_data(struct msm_vidc_inst *inst);
int msm_comm_set_cvp_skip_ratio(struct msm_vidc_inst *inst,
	uint32_t capture_rate, uint32_t cvp_rate);
int msm_comm_check_memory_supported(struct msm_vidc_inst *vidc_inst);
int msm_comm_update_dpb_bufreqs(struct msm_vidc_inst *inst);
#endif
