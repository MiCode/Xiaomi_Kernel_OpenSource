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

void cvp_put_inst(struct msm_cvp_inst *inst);
struct msm_cvp_inst *cvp_get_inst(struct msm_cvp_core *core,
		void *session_id);
struct msm_cvp_inst *cvp_get_inst_validate(struct msm_cvp_core *core,
		void *session_id);
void cvp_change_inst_state(struct msm_cvp_inst *inst,
		enum instance_state state);
struct msm_cvp_core *get_cvp_core(int core_id);
int msm_cvp_comm_try_state(struct msm_cvp_inst *inst, int state);
int msm_cvp_deinit_core(struct msm_cvp_inst *inst);
int msm_cvp_comm_suspend(int core_id);
void msm_cvp_comm_session_clean(struct msm_cvp_inst *inst);
int msm_cvp_comm_kill_session(struct msm_cvp_inst *inst);
void msm_cvp_comm_generate_session_error(struct msm_cvp_inst *inst);
void msm_cvp_comm_generate_sys_error(struct msm_cvp_inst *inst);
int msm_cvp_comm_smem_cache_operations(struct msm_cvp_inst *inst,
		struct msm_cvp_smem *mem, enum smem_cache_ops cache_ops);
int msm_cvp_comm_check_core_init(struct msm_cvp_core *core);
void msm_cvp_comm_print_inst_info(struct msm_cvp_inst *inst);
int msm_cvp_comm_unmap_cvp_buffer(struct msm_cvp_inst *inst,
		struct msm_cvp_internal_buffer *cbuf);
void print_cvp_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst,
		struct msm_cvp_internal_buffer *cbuf);
int wait_for_sess_signal_receipt(struct msm_cvp_inst *inst,
	enum hal_command_response cmd);
int wait_for_sess_signal_receipt_fence(struct msm_cvp_inst *inst,
	enum hal_command_response cmd);
int cvp_comm_set_arp_buffers(struct msm_cvp_inst *inst);
int cvp_comm_release_persist_buffers(struct msm_cvp_inst *inst);
void print_client_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst, struct cvp_kmd_buffer *cbuf);
void msm_cvp_unmap_buf_cpu(struct msm_cvp_inst *inst, u64 ktid);
#endif
