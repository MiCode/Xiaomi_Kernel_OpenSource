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

#include "cmdq_core.h"
#include "cmdq_sec_gp.h"

#define UUID_STR "09010000000000000000000000000000"

void cmdq_sec_setup_tee_context(struct cmdq_sec_tee_context *tee)
{
	/* 09010000 0000 0000 0000000000000000 */
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
	tee->uuid = (struct TEEC_UUID) { 0x09010000, 0x0, 0x0,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };
#else
	tee->uuid = (TEEC_UUID) { 0x09010000, 0x0, 0x0,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };
#endif
}

s32 cmdq_sec_init_context(struct cmdq_sec_tee_context *tee)
{
	s32 status;

#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
	while (!is_teei_ready()) {
		CMDQ_MSG("[SEC]TEE is not ready, wait...\n");
		msleep(1000);
	}
	CMDQ_LOG("[SEC]TEE is ready\n");
#endif

	status = TEEC_InitializeContext(UUID_STR, &tee->gp_context);
	if (status != TEEC_SUCCESS)
		CMDQ_ERR("[SEC]init_context fail: status:0x%x\n", status);
	else
		CMDQ_LOG("[SEC]init_context: status:0x%x\n", status);
	return status;
}

s32 cmdq_sec_deinit_context(struct cmdq_sec_tee_context *tee)
{
	TEEC_FinalizeContext(&tee->gp_context);
	return 0;
}

s32 cmdq_sec_allocate_wsm(struct cmdq_sec_tee_context *tee,
	void **wsm_buffer, u32 size)
{
	s32 status;

	if (!wsm_buffer)
		return -EINVAL;

	tee->shared_mem.size = size;
	tee->shared_mem.flags = TEEC_MEM_INPUT;
	status = TEEC_AllocateSharedMemory(&tee->gp_context,
		&tee->shared_mem);
	if (status != TEEC_SUCCESS) {
		CMDQ_ERR("[SEC]allocate_wsm: err:0x%x\n", status);
	} else {
		CMDQ_MSG("[SEC]allocate_wsm: status:0x%x pWsm:0x%p\n",
			status, tee->shared_mem.buffer);
		*wsm_buffer = (void *)tee->shared_mem.buffer;
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
	s32 status;

	if (!wsm_buffer) {
		CMDQ_ERR("[SEC]open_session: invalid param wsm buffer:0x%p\n",
			wsm_buffer);
		return -EINVAL;
	}

	status = TEEC_OpenSession(&tee->gp_context,
		&tee->session, &tee->uuid,
		TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);

	if (status != TEEC_SUCCESS) {
		/* print error message */
		CMDQ_ERR("[SEC]open_session fail: status:0x%x\n",
			status);
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
	u32 cmd, s32 timeout_ms)
{
	s32 status;
	struct TEEC_Operation operation;

	memset(&operation, 0, sizeof(struct TEEC_Operation));
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
	operation.param_types = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT,
		TEEC_NONE, TEEC_NONE, TEEC_NONE);
#else
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT,
		TEEC_NONE, TEEC_NONE, TEEC_NONE);
#endif
	operation.params[0].memref.parent = &tee->shared_mem;
	operation.params[0].memref.size = tee->shared_mem.size;
	operation.params[0].memref.offset = 0;

	status = TEEC_InvokeCommand(&tee->session, cmd, &operation,
		NULL);
	if (status != TEEC_SUCCESS)
		CMDQ_ERR("[SEC]execute: TEEC_InvokeCommand:%u err:%d\n",
			cmd, status);
	else
		CMDQ_MSG("[SEC]execute: TEEC_InvokeCommand:%u ret:%d\n",
			cmd, status);

	return status;
}

