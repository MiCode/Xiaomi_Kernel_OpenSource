 /*
  * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/notifier.h>

#include "m4u_priv.h"
#include "m4u.h"
#include "m4u_hw.h"
#include "m4u_priv.h"
#include "tee_client_api.h"

#include "tz_m4u.h"
#include "m4u_sec_gp.h"

unsigned int M4U_SEC_SESSION;

static struct m4u_sec_gp_context m4u_gp_ta_ctx = {
#if defined(CONFIG_MICROTRUST_TEE_SUPPORT) || \
	defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
		.uuid = (struct TEEC_UUID)M4U_TA_UUID,
#else
		.uuid = (TEEC_UUID)M4U_TA_UUID,
#endif
	.ctx_lock = __MUTEX_INITIALIZER(m4u_gp_ta_ctx.ctx_lock),
	.ctx_type = CTX_TYPE_TA,
};
struct m4u_sec_context m4u_ta_ctx;

void m4u_sec_set_context(void)
{
	m4u_ta_ctx.name = "m4u_ta";
	m4u_ta_ctx.imp = &m4u_gp_ta_ctx;
}

static int m4u_exec_session(struct m4u_sec_context *ctx)
{
	int ret;
	struct TEEC_Operation m4u_operation;
	struct m4u_sec_gp_context *gp_ctx = ctx->imp;

	if (!ctx->m4u_msg) {
		M4UMSG("%s TCI/DCI error\n", __func__);
		return -1;
	}

	M4ULOG_HIGH("%s, Notify 0x%x\n", __func__, ctx->m4u_msg->cmd);

	memset(&m4u_operation, 0, sizeof(struct TEEC_Operation));
#if defined(CONFIG_MICROTRUST_TEE_SUPPORT) || \
	defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
	m4u_operation.paramTypes = TEEC_PARAM_TYPES(
		TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
#endif

	m4u_operation.params[0].memref.parent = &gp_ctx->shared_mem;
	m4u_operation.params[0].memref.offset = 0;
	m4u_operation.params[0].memref.size = gp_ctx->shared_mem.size;

	ret = TEEC_InvokeCommand(&gp_ctx->session,
			ctx->m4u_msg->cmd, &m4u_operation, NULL);

	if (ret != TEEC_SUCCESS) {
		m4u_aee_print("tz_m4u Notify failed: %d\n", ret);
		goto exit;
	}

	M4ULOG_HIGH("%s, get_resp %x\n", __func__, ctx->m4u_msg->cmd);
exit:
	return ret;
}

static int m4u_sec_gp_init(struct m4u_sec_context *ctx)
{
	int ret;
	struct m4u_sec_gp_context *gp_ctx = ctx->imp;

	ret = TEEC_InitializeContext(TA_UUID, &gp_ctx->ctx);
	if (ret != TEEC_SUCCESS) {
		M4UMSG("teec_initialize_context failed: %x\n", ret);
		return ret;
	}

	M4ULOG_HIGH("%s, ta teec_initialize_context\n", __func__);


	memset(&gp_ctx->shared_mem, 0, sizeof(struct TEEC_SharedMemory));

	gp_ctx->shared_mem.size = sizeof(struct m4u_msg);
	gp_ctx->shared_mem.flags = TEEC_MEM_INPUT;

	ret = TEEC_AllocateSharedMemory(&gp_ctx->ctx, &gp_ctx->shared_mem);
	if (ret == TEEC_SUCCESS) {
		ctx->m4u_msg = (struct m4u_msg *)gp_ctx->shared_mem.buffer;
		M4ULOG_HIGH(
			"teec_allocate_shared_memory buf: 0x%p\n",
				gp_ctx->shared_mem.buffer);
	} else {
		M4UMSG("teec_allocate_shared_memory failed: %d\n", ret);
		goto exit_finalize;
	}

	if (!ctx->m4u_msg) {
		M4UMSG("m4u msg is invalid\n");
		return -1;
	}
	if (!M4U_SEC_SESSION) {
		ret = TEEC_OpenSession(&gp_ctx->ctx,
			&gp_ctx->session, &gp_ctx->uuid,
				       TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
		if (ret != TEEC_SUCCESS) {
			M4UMSG("teec_open_session failed: %x\n", ret);
			goto exit_release;

		}
		M4U_SEC_SESSION = 1;
	}

	gp_ctx->init = 1;

	M4ULOG_HIGH("%s, open TCI session success\n", __func__);
	return ret;

exit_release:
	TEEC_ReleaseSharedMemory(&gp_ctx->shared_mem);
exit_finalize:
	TEEC_FinalizeContext(&gp_ctx->ctx);
	return ret;
}

static int m4u_sec_gp_deinit(struct m4u_sec_context *ctx)
{
	struct m4u_sec_gp_context *gp_ctx = ctx->imp;

	gp_ctx->init = 0;

	TEEC_ReleaseSharedMemory(&gp_ctx->shared_mem);
	TEEC_CloseSession(&gp_ctx->session);
	TEEC_FinalizeContext(&gp_ctx->ctx);

	return 0;
}

static int m4u_sec_ta_open(void)
{
	int ret;

	ret = m4u_sec_gp_init(&m4u_ta_ctx);
	return ret;
}

static int m4u_sec_ta_close(void)
{
	int ret;

	ret = m4u_sec_gp_deinit(&m4u_ta_ctx);
	return ret;
}

int m4u_sec_context_init(void)
{
	int ret;

	ret = m4u_sec_ta_open();
	if (ret)
		return ret;

	M4ULOG_HIGH("%s:ta open session success\n", __func__);

	return 0;
}

int m4u_sec_context_deinit(void)
{
	int ret;

	ret = m4u_sec_ta_close();
	return ret;
}

struct m4u_sec_context *m4u_sec_ctx_get(unsigned int cmd)
{
	struct m4u_sec_context *ctx = NULL;
	struct m4u_sec_gp_context *gp_ctx;

	ctx = &m4u_ta_ctx;
	gp_ctx = ctx->imp;
	if (!gp_ctx->init) {
		M4UERR("%s before init\n", __func__);
		return NULL;
	}
	mutex_lock(&gp_ctx->ctx_lock);

	return ctx;
}

int m4u_sec_ctx_put(struct m4u_sec_context *ctx)
{
	struct m4u_sec_gp_context *gp_ctx = ctx->imp;

	mutex_unlock(&gp_ctx->ctx_lock);

	return 0;
}

int m4u_exec_cmd(struct m4u_sec_context *ctx)
{
	int ret;

	if (ctx->m4u_msg == NULL) {
		M4UMSG("%s TCI/DCI error\n", __func__);
		return -1;
	}
	ret = m4u_exec_session(ctx);
	if (ret < 0)
		return -1;

	return 0;
}
