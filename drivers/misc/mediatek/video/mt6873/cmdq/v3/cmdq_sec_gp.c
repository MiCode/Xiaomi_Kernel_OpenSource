// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "cmdq_core.h"
#include "cmdq_sec_gp.h"

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

	CMDQ_MSG("[SEC] enter %s\n", __func__);
#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
	while (!is_teei_ready()) {
		CMDQ_MSG("[SEC] Microtrust TEE is not ready, wait...\n");
		msleep(1000);
	}
#elif defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
	while (!is_mobicore_ready()) {
		CMDQ_MSG("[SEC] Trustonic TEE is not ready, wait...\n");
		msleep(1000);
	}
#endif
	CMDQ_LOG("[SEC]TEE is ready\n");

	status = TEEC_InitializeContext(NULL, &tee->gp_context);
	if (status != TEEC_SUCCESS)
		CMDQ_ERR("[SEC]init_context fail: status:0x%x\n", status);
	else
		CMDQ_MSG("[SEC]init_context: status:0x%x\n", status);
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

	CMDQ_MSG("%s tee:0x%p size:%u size ex:%u\n",
		__func__, tee, size, size_ex);
	tee->shared_mem.size = size;
	tee->shared_mem.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	status = TEEC_AllocateSharedMemory(&tee->gp_context,
		&tee->shared_mem);
	if (status != TEEC_SUCCESS) {
		CMDQ_LOG("[WARN][SEC]allocate_wsm: err:0x%x size:%u\n",
			status, size);
	} else {
		CMDQ_LOG("[SEC]allocate_wsm: status:0x%x wsm:0x%p size:%u\n",
			status, tee->shared_mem.buffer, size);
		*wsm_buffer = (void *)tee->shared_mem.buffer;
	}

	tee->shared_mem_ex.size = size_ex;
	tee->shared_mem_ex.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	status = TEEC_AllocateSharedMemory(&tee->gp_context,
		&tee->shared_mem_ex);
	if (status != TEEC_SUCCESS) {
		CMDQ_LOG("[WARN][SEC]allocate_wsm: err:0x%x size_ex:%u\n",
			status, size_ex);
	} else {
		CMDQ_LOG("[SEC]allocate_wsm: status:0x%x wsm:0x%p size ex:%u\n",
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
		CMDQ_ERR("[SEC]open_session: invalid param wsm buffer:0x%p\n",
			wsm_buffer);
		return -EINVAL;
	}

	status = TEEC_OpenSession(&tee->gp_context,
		&tee->session, &tee->uuid,
		TEEC_LOGIN_PUBLIC, NULL, NULL, &ret_origin);

	if (status != TEEC_SUCCESS) {
		/* print error message */
		CMDQ_ERR(
			"[SEC]open_session fail: status:0x%x ret origin:0x%08x\n",
			status, ret_origin);
	} else {
		CMDQ_MSG("[SEC]open_session: status:0x%x\n", status);
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
		CMDQ_ERR(
			"[SEC]execute: TEEC_InvokeCommand:%u err:%d memex:%s\n",
			cmd, status, share_mem_ex ? "true" : "false");
	else
		CMDQ_MSG(
			"[SEC]execute: TEEC_InvokeCommand:%u ret:%d memex:%s\n",
			cmd, status, share_mem_ex ? "true" : "false");

	return status;
}

