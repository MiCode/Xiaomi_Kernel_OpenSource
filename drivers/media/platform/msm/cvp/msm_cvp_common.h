/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */


#ifndef _MSM_CVP_COMMON_H_
#define _MSM_CVP_COMMON_H_
#include "msm_cvp_internal.h"

enum load_calc_quirks {
	LOAD_CALC_NO_QUIRKS = 0,
	LOAD_CALC_IGNORE_TURBO_LOAD = 1 << 0,
	LOAD_CALC_IGNORE_THUMBNAIL_LOAD = 1 << 1,
	LOAD_CALC_IGNORE_NON_REALTIME_LOAD = 1 << 2,
};

static inline bool is_thumbnail_session(struct msm_cvp_inst *inst)
{
	return !!(inst->flags & CVP_THUMBNAIL);
}

static inline bool is_decode_session(struct msm_cvp_inst *inst)
{
	return inst->session_type == MSM_CVP_DECODER;
}

static inline bool is_encode_session(struct msm_cvp_inst *inst)
{
	return inst->session_type == MSM_CVP_ENCODER;
}

enum hal_buffer cvp_get_hal_buffer_type(unsigned int type,
		unsigned int plane_num);
void cvp_put_inst(struct msm_cvp_inst *inst);
struct msm_cvp_inst *cvp_get_inst(struct msm_cvp_core *core,
		void *session_id);
void cvp_change_inst_state(struct msm_cvp_inst *inst,
		enum instance_state state);
struct msm_cvp_core *get_cvp_core(int core_id);
struct buf_queue *msm_cvp_comm_get_vb2q(
		struct msm_cvp_inst *inst, enum v4l2_buf_type type);
int msm_cvp_comm_try_state(struct msm_cvp_inst *inst, int state);
int msm_cvp_comm_set_buffer_count(struct msm_cvp_inst *inst,
	int host_count, int act_count, enum hal_buffer type);
int msm_cvp_comm_force_cleanup(struct msm_cvp_inst *inst);
int msm_cvp_comm_suspend(int core_id);
struct hal_buffer_requirements *get_cvp_buff_req_buffer(
			struct msm_cvp_inst *inst, u32 buffer_type);
void msm_cvp_comm_session_clean(struct msm_cvp_inst *inst);
int msm_cvp_comm_kill_session(struct msm_cvp_inst *inst);
void msm_cvp_comm_generate_session_error(struct msm_cvp_inst *inst);
void msm_cvp_comm_generate_sys_error(struct msm_cvp_inst *inst);
enum multi_stream msm_cvp_comm_get_stream_output_mode(
		struct msm_cvp_inst *inst);
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
void msm_cvp_comm_print_inst_info(struct msm_cvp_inst *inst);
struct msm_video_buffer *msm_cvp_comm_get_video_buffer(
		struct msm_cvp_inst *inst, struct vb2_buffer *vb2);
int msm_cvp_comm_unmap_video_buffer(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf);
void print_video_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst, struct msm_video_buffer *mbuf);
void kref_cvp_put_mbuf(struct msm_video_buffer *mbuf);
bool kref_cvp_get_mbuf(struct msm_cvp_inst *inst,
	struct msm_video_buffer *mbuf);
int msm_cvp_comm_num_queued_bufs(struct msm_cvp_inst *inst, u32 type);
int wait_for_sess_signal_receipt(struct msm_cvp_inst *inst,
	enum hal_command_response cmd);
int cvp_comm_set_arp_buffers(struct msm_cvp_inst *inst);
int cvp_comm_release_persist_buffers(struct msm_cvp_inst *inst);
#endif
