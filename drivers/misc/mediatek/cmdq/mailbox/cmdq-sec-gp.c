/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/soc/mediatek/mtk-cmdq.h>

#include "cmdq-sec-gp.h"

#define UUID_STR "09010000000000000000000000000000"

void cmdq_sec_setup_tee_context(struct cmdq_sec_tee_context *tee)
{
	/* 09010000 0000 0000 0000000000000000 */
	tee->uuid = (struct TEEC_UUID) { 0x09010000, 0x0, 0x0,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };
}

s32 cmdq_sec_init_context(struct cmdq_sec_tee_context *tee)
{
	s32 status;

	cmdq_msg("[SEC] enter %s", __func__);
#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
	while (!is_teei_ready()) {
		cmdq_msg("[SEC] Microtrust TEE is not ready, wait...");
		msleep(1000);
	}
#else
	while (!is_mobicore_ready()) {
		cmdq_msg("[SEC] Trustonic TEE is not ready, wait...");
		msleep(1000);
	}
#endif
	cmdq_log("[SEC]TEE is ready");

	status = TEEC_InitializeContext(NULL, &tee->gp_context);
	if (status != TEEC_SUCCESS)
		cmdq_err("[SEC]init_context fail: status:0x%x", status);
	else
		cmdq_msg("[SEC]init_context: status:0x%x", status);
	return status;
}

s32 cmdq_sec_deinit_context(struct cmdq_sec_tee_context *tee)
{
	TEEC_FinalizeContext(&tee->gp_context);
	return 0;
}

s32 cmdq_sec_allocate_wsm(struct cmdq_sec_tee_context *tee,
	void **wsm_buffer, u32 size, void **wsm_buf_ex, u32 size_ex)
{
	s32 status;

	if (!wsm_buffer || !wsm_buf_ex)
		return -EINVAL;

	cmdq_msg("%s tee:0x%p size:%u size ex:%u",
		__func__, tee, size, size_ex);
	tee->shared_mem.size = size;
	tee->shared_mem.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	status = TEEC_AllocateSharedMemory(&tee->gp_context,
		&tee->shared_mem);
	if (status != TEEC_SUCCESS) {
		cmdq_err("[WARN][SEC]allocate_wsm: err:0x%x size:%u",
			status, size);
	} else {
		cmdq_log("[SEC]allocate_wsm: status:0x%x wsm:0x%p size:%u",
			status, tee->shared_mem.buffer, size);
		*wsm_buffer = (void *)tee->shared_mem.buffer;
	}

	tee->shared_mem_ex.size = size_ex;
	tee->shared_mem_ex.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	status = TEEC_AllocateSharedMemory(&tee->gp_context,
		&tee->shared_mem_ex);
	if (status != TEEC_SUCCESS) {
		cmdq_err("[WARN][SEC]allocate_wsm: err:0x%x size_ex:%u",
			status, size_ex);
	} else {
		cmdq_log("[SEC]allocate_wsm: status:0x%x wsm:0x%p size ex:%u",
			status, tee->shared_mem_ex.buffer, size_ex);
		*wsm_buf_ex = (void *)tee->shared_mem_ex.buffer;
	}

	return status;
}

s32 cmdq_sec_free_wsm(struct cmdq_sec_tee_context *tee,
	void **wsm_buffer)
{
	if (!wsm_buffer)
		return -EINVAL;

	TEEC_ReleaseSharedMemory(&tee->shared_mem);
	*wsm_buffer = NULL;
	return 0;
}

s32 cmdq_sec_open_session(struct cmdq_sec_tee_context *tee,
	void *wsm_buffer)
{
	s32 status, ret_origin;

	if (!wsm_buffer) {
		cmdq_err("[SEC]open_session: invalid param wsm buffer:0x%p",
			wsm_buffer);
		return -EINVAL;
	}

	status = TEEC_OpenSession(&tee->gp_context,
		&tee->session, &tee->uuid,
		TEEC_LOGIN_PUBLIC, NULL, NULL, &ret_origin);

	if (status != TEEC_SUCCESS) {
		/* print error message */
		cmdq_err(
			"[SEC]open_session fail: status:0x%x ret origin:0x%08x",
			status, ret_origin);
	} else {
		cmdq_msg("[SEC]open_session: status:0x%x", status);
	}

	return status;
}

s32 cmdq_sec_close_session(struct cmdq_sec_tee_context *tee)
{
	TEEC_CloseSession(&tee->session);
	return 0;
}

s32 cmdq_sec_execute_session(struct cmdq_sec_tee_context *tee,
	u32 cmd, s32 timeout_ms, bool share_mem_ex)
{
	s32 status;
	struct TEEC_Operation operation;

	memset(&operation, 0, sizeof(struct TEEC_Operation));
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
	operation.param_types = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT,
		share_mem_ex ? TEEC_MEMREF_PARTIAL_INOUT : TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
#else
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT,
		share_mem_ex ? TEEC_MEMREF_PARTIAL_INOUT : TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
#endif
	operation.params[0].memref.parent = &tee->shared_mem;
	operation.params[0].memref.size = tee->shared_mem.size;
	operation.params[0].memref.offset = 0;

	if (share_mem_ex) {
		operation.params[1].memref.parent = &tee->shared_mem_ex;
		operation.params[1].memref.size = tee->shared_mem_ex.size;
		operation.params[1].memref.offset = 0;
	}

	status = TEEC_InvokeCommand(&tee->session, cmd, &operation,
		NULL);
	if (status != TEEC_SUCCESS)
		cmdq_err(
			"[SEC]execute: TEEC_InvokeCommand:%u err:%d memex:%s",
			cmd, status, share_mem_ex ? "true" : "false");
	else
		cmdq_msg(
			"[SEC]execute: TEEC_InvokeCommand:%u ret:%d memex:%s",
			cmd, status, share_mem_ex ? "true" : "false");

	return status;
}

