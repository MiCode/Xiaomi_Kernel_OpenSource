// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/math64.h>
#include <linux/sched/clock.h>

#include "cmdq-sec-gp.h"

#define UUID_STR "09010000000000000000000000000000"

void cmdq_sec_setup_tee_context(struct cmdq_sec_tee_context *tee)
{
	/* 09010000 0000 0000 0000000000000000 */
	tee->uuid = (struct TEEC_UUID) { 0x09010000, 0x0, 0x0,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };
}

#include <linux/atomic.h>
static atomic_t m4u_init = ATOMIC_INIT(0);

s32 cmdq_sec_init_context(struct cmdq_sec_tee_context *tee)
{
	s32 status;

	cmdq_msg("[SEC]%s", __func__);
#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
	while (!is_teei_ready()) {
		cmdq_msg("[SEC]Microtrust TEE is not ready, wait...");
		msleep(1000);
	}
#else
	while (!is_mobicore_ready()) {
		cmdq_msg("[SEC]Trustonic TEE is not ready, wait...");
		msleep(1000);
	}
#endif
	cmdq_log("[SEC]TEE is ready");

	/* do m4u sec init */
	if (atomic_cmpxchg(&m4u_init, 0, 1) == 0) {
		m4u_sec_init();
		cmdq_msg("[SEC] M4U_sec_init is called\n");
	}

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
	void **wsm_buffer, u8 idx, u32 size)
{
	s32 status;
	struct TEEC_SharedMemory *mem = &tee->shared_mem[idx];

	if (!wsm_buffer)
		return -EINVAL;

	cmdq_msg("%s tee:0x%p size:%u idx:%hhu",
		__func__, tee, size, idx);
	mem->size = size;
	mem->flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	status = TEEC_AllocateSharedMemory(&tee->gp_context, mem);
	if (status != TEEC_SUCCESS) {
		cmdq_err("[SEC][WARN]allocate_wsm: err:%#x size:%u idx:%hhu",
			status, size, idx);
	} else {
		cmdq_log(
			"[SEC]allocate_wsm: status:%#x wsm:0x%p size:%u idx:%hhu",
			status, mem->buffer, size, idx);
		*wsm_buffer = mem->buffer;
	}

	return status;
}

s32 cmdq_sec_free_wsm(struct cmdq_sec_tee_context *tee,
	void **wsm_buffer, u8 idx)
{
	if (!wsm_buffer)
		return -EINVAL;

	TEEC_ReleaseSharedMemory(&tee->shared_mem[idx]);
	*wsm_buffer = NULL;
	return 0;
}

s32 cmdq_sec_open_session(struct cmdq_sec_tee_context *tee,
	void *wsm_buffer)
{
	s32 status, ret_origin = 0;

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
	u32 cmd, s32 timeout_ms, bool mem_ex1, bool mem_ex2)
{
	s32 status;
	struct TEEC_Operation operation;
	u64 ts = sched_clock();

	memset(&operation, 0, sizeof(struct TEEC_Operation));
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
	operation.param_types = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT,
		mem_ex1 ? TEEC_MEMREF_PARTIAL_INOUT : TEEC_NONE,
		mem_ex2 ? TEEC_MEMREF_PARTIAL_INOUT : TEEC_NONE,
		TEEC_NONE);
#else
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT,
		mem_ex1 ? TEEC_MEMREF_PARTIAL_INOUT : TEEC_NONE,
		mem_ex2 ? TEEC_MEMREF_PARTIAL_INOUT : TEEC_NONE,
		TEEC_NONE);
#endif
	operation.params[0].memref.parent = &tee->shared_mem[0];
	operation.params[0].memref.size = tee->shared_mem[0].size;
	operation.params[0].memref.offset = 0;

	if (mem_ex1) {
		operation.params[1].memref.parent = &tee->shared_mem[1];
		operation.params[1].memref.size = tee->shared_mem[1].size;
		operation.params[1].memref.offset = 0;
	}

	if (mem_ex2) {
		operation.params[2].memref.parent = &tee->shared_mem[2];
		operation.params[2].memref.size = tee->shared_mem[2].size;
		operation.params[2].memref.offset = 0;
	}

	status = TEEC_InvokeCommand(&tee->session, cmd, &operation,
		NULL);
	ts = div_u64(sched_clock() - ts, 1000000);

	if (status != TEEC_SUCCESS)
		cmdq_err(
			"[SEC]execute: TEEC_InvokeCommand:%u err:%d memex:%s memex2:%s cost:%lldus",
			cmd, status, mem_ex1 ? "true" : "false",
			mem_ex2 ? "true" : "false",
			ts);
	else if (ts > 15000)
		cmdq_msg(
			"[SEC]execute: TEEC_InvokeCommand:%u ret:%d memex1:%s memex2:%s cost:%lldus",
			cmd, status, mem_ex1 ? "true" : "false",
			mem_ex2 ? "true" : "false",
			ts);
	else
		cmdq_log(
			"[SEC]execute: TEEC_InvokeCommand:%u ret:%d memex1:%s memex2:%s cost:%lldus",
			cmd, status, mem_ex1 ? "true" : "false",
			mem_ex2 ? "true" : "false",
			ts);

	return status;
}

MODULE_LICENSE("GPL v2");
