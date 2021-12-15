/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __CMDQ_RECORD_PRIVATE_H__
#define __CMDQ_RECORD_PRIVATE_H__

#include "mdp_cmdq_record.h"

#ifdef __cplusplus
extern "C" {
#endif

s32 cmdq_append_command(struct cmdqRecStruct *handle,
	enum cmdq_code code,
	u32 arg_a, u32 arg_b, u32 arg_a_type, u32 arg_b_type);
s32 cmdq_op_finalize_command(struct cmdqRecStruct *handle, bool loop);

s32 cmdq_setup_sec_data_of_command_desc_by_rec_handle(
	struct cmdqCommandStruct *pDesc, struct cmdqRecStruct *handle);

s32 cmdq_setup_replace_of_command_desc_by_rec_handle(
	struct cmdqCommandStruct *pDesc, struct cmdqRecStruct *handle);

s32 cmdq_rec_setup_profile_marker_data(
	struct cmdqCommandStruct *pDesc, struct cmdqRecStruct *handle);

s32 cmdq_task_create_delay_thread_dram(void **pp_delay_thread_buffer,
	u32 *buffer_size);
s32 cmdq_task_create_delay_thread_sram(void **pp_delay_thread_buffer,
	u32 *buffer_size, u32 *cpr_offset);

#ifdef __cplusplus
}
#endif
#endif				/* __CMDQ_RECORD_PRIVATE_H__ */
