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

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "cmdq_core.h"
#include "cmdq_sec_trustzone.h"
#include "tz_cross/trustzone.h"
#include "cmdqsectl_api.h"

static void *g_iwc_sec_buffer;
void cmdq_sec_alloc_iwc_buffer(void)
{
	g_iwc_sec_buffer = kmalloc(sizeof(struct iwcCmdqMessage_t),
			GFP_KERNEL);
	CMDQ_ERR("allocate iwc buffer:%p\n", g_iwc_sec_buffer);
}

void cmdq_sec_setup_tee_context(struct cmdq_sec_tee_context *tee)
{
	memset(tee, 0, sizeof(struct cmdq_sec_tee_context));
	/* "5c071864-505d-11e4-9e35-164230d1df67" */
	memset(tee->uuid, 0, sizeof(tee->uuid));
	snprintf(tee->uuid, sizeof(tee->uuid), "%s",
			"5c071864-505d-11e4-9e35-164230d1df67");
}

s32 cmdq_sec_trustzone_create_share_memory(void **va, u32 *pa, u32 size)
{
	char *alloc_va = NULL;

	alloc_va = kzalloc(size, GFP_KERNEL);
	if (!alloc_va) {
		CMDQ_ERR("kmalloc size:%d failed\n", size);
		return -ENOMEM;
	}
	*va = alloc_va;
	*pa = __pa(alloc_va);
	return 0;
}


s32 cmdq_sec_init_context(struct cmdq_sec_tee_context *tee)
{
	return 0;
}

s32 cmdq_sec_deinit_context(struct cmdq_sec_tee_context *tee)
{
	return 0;
}

s32 cmdq_sec_register_share_memory(struct cmdq_sec_tee_context *tee)
{
#if 1
	int tzRes;
	struct KREE_SHAREDMEM_PARAM cmdq_shared_param;
	union MTEEC_PARAM cmdq_param[4];
	unsigned int paramTypes;
	KREE_SHAREDMEM_HANDLE cmdq_share_handle = 0;
	struct ContextStruct *context = cmdq_core_get_context();
	static int memory_share_done;
#endif

	if (memory_share_done == true)
		return 0;
	memory_share_done = true;

#if 1
	context = cmdq_core_get_context();
	if (context == NULL ||
		!context->hSecSharedMem ||
		tee == NULL ||
		tee->mem_session == 0) {
		CMDQ_ERR("%s invalid param\n", __func__);
		return -EINVAL;
	}

	/* init share memory */
	cmdq_shared_param.buffer = context->hSecSharedMem->pVABase;
	cmdq_shared_param.size = context->hSecSharedMem->size;

	tzRes = KREE_RegisterSharedmem(tee->mem_session,
		&cmdq_share_handle,
		&cmdq_shared_param);

	if (tzRes != TZ_RESULT_SUCCESS) {
		CMDQ_ERR("register share mem fail:%d mem_session:%d\n",
			tzRes,
			tee->mem_session);
		return tzRes;
	}

	/* KREE_Tee service call */
	cmdq_param[0].memref.handle = (uint32_t) cmdq_share_handle;
	cmdq_param[0].memref.offset = 0;
	cmdq_param[0].memref.size = cmdq_shared_param.size;
	paramTypes = TZ_ParamTypes1(TZPT_MEMREF_INOUT);
	tzRes =
	    KREE_TeeServiceCall(tee->session,
				CMD_CMDQ_TL_INIT_SHARED_MEMORY, paramTypes,
				cmdq_param);
	if (tzRes != TZ_RESULT_SUCCESS) {
		CMDQ_ERR("CMD_CMDQ_TL_INIT_SHARED_MEMORY fail, ret=0x%x\n",
				tzRes);
		return tzRes;
	}
	CMDQ_MSG("KREE_TeeServiceCall tzRes =0x%x\n", tzRes);
	return 0;
#endif

}

/* allocate share memory for communicate with trustzone */
s32 cmdq_sec_allocate_wsm(struct cmdq_sec_tee_context *tee, void **wsm_buffer,
	u32 size, void **wsm_buf_ex, u32 size_ex,
	void **wsm_buf_ex2, u32 size_ex2)
{
	s32 status = 0;

	if (!wsm_buffer || !wsm_buf_ex || !wsm_buf_ex2) {
		CMDQ_ERR("%s invalid param:%p %p %p\n", __func__,
			wsm_buffer, wsm_buf_ex, wsm_buf_ex2);
		return -EINVAL;
	}
	/* because world shared mem(WSM) will ba managed by mobicore device,
	 * instead of linux kernel vmalloc/kmalloc, call mc_malloc_wasm to
	 * alloc WSM to prvent error such as "can not resolve tci physicall
	 * address" etc
	 */
	if (g_iwc_sec_buffer == NULL)
		tee->share_memory = kmalloc(sizeof(struct iwcCmdqMessage_t),
				GFP_KERNEL);
	else
		tee->share_memory = g_iwc_sec_buffer;

	if (!tee->share_memory) {
		CMDQ_ERR("share memory kmalloc failed!\n");
		return -ENOMEM;
	}

	if (tee->wsm_buf_ex == NULL) {
		tee->wsm_buf_ex = kmalloc(size_ex, GFP_KERNEL);
		if (tee->wsm_buf_ex == NULL) {
			CMDQ_ERR("ex share memory kmalloc failed!\n");
			return -ENOMEM;
		}
	}

	if (tee->wsm_buf_ex2 == NULL) {
		tee->wsm_buf_ex2 = kmalloc(size_ex2, GFP_KERNEL);
		if (tee->wsm_buf_ex2 == NULL) {
			CMDQ_ERR("ex2 share memory kmalloc failed!\n");
			return -ENOMEM;
		}
	}

	*wsm_buffer = tee->share_memory;
	*wsm_buf_ex = tee->wsm_buf_ex;
	*wsm_buf_ex2 = tee->wsm_buf_ex2;

	return status;
}

/* free share memory  */
s32 cmdq_sec_free_wsm(struct cmdq_sec_tee_context *tee,
	void **wsm_buffer)
{
	if (wsm_buffer && *wsm_buffer) {
		kfree(*wsm_buffer);
		*wsm_buffer = NULL;
		tee->share_memory = NULL;
	}


	kfree(tee->wsm_buf_ex);
	tee->wsm_buf_ex = NULL;


	kfree(tee->wsm_buf_ex2);
	tee->wsm_buf_ex2 = NULL;

	return 0;
}


/* create cmdq session & memory session */
s32 cmdq_sec_open_session(struct cmdq_sec_tee_context *tee,
	void *wsm_buffer)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	if (!tee->session) {
		int ret;

		CMDQ_LOG("TZ_TA_CMDQ_UUID:%s\n", tee->uuid);
		ret = KREE_CreateSession(tee->uuid, &tee->session);
		if (ret != TZ_RESULT_SUCCESS) {
			CMDQ_ERR("%s failed to create cmdq session, ret=%d\n",
				__func__,
				ret);
			return 0;
		}
	}

	if (!tee->mem_session) {
		int ret;

		ret = KREE_CreateSession(TZ_TA_MEM_UUID, &tee->mem_session);
		if (ret != TZ_RESULT_SUCCESS) {
			CMDQ_ERR("%s failed to create mem session, ret=%d\n",
				__func__,
				ret);
			return 0;
		}
	}

	tee->share_memory = wsm_buffer;
	cmdq_sec_register_share_memory(tee);
#else
	CMDQ_ERR("SVP feature is not on\n");
	return 0;
#endif
	return 0;
}

/* close cmdq & memory session */
s32 cmdq_sec_close_session(struct cmdq_sec_tee_context *tee)
{
	int ret;

	ret = KREE_CloseSession(tee->session);
	if (ret != TZ_RESULT_SUCCESS) {
		CMDQ_ERR("DDP close ddp_session fail ret=%d\n", ret);
		return 0;
	}

	ret = KREE_CloseSession(tee->session);
	if (ret != TZ_RESULT_SUCCESS) {
		CMDQ_ERR("DDP close ddp_session fail ret=%d\n", ret);
		return 0;
	}
	return 0;
}

s32 cmdq_sec_execute_session(struct cmdq_sec_tee_context *tee,
	u32 cmd, s32 timeout_ms)
{
	int tzRes;

	do {
		/* Register share memory */
		union MTEEC_PARAM cmdq_param[4];
		unsigned int paramTypes;
		KREE_SHAREDMEM_HANDLE cmdq_share_handle = 0;
		struct KREE_SHAREDMEM_PARAM cmdq_shared_param;

		cmdq_shared_param.buffer = tee->share_memory;
		cmdq_shared_param.size = (sizeof(struct iwcCmdqMessage_t));

#if 0				/* add for debug */
		CMDQ_ERR("dump secure task instructions in Normal world\n");
		cmdq_core_dump_instructions((uint64_t *)(((iwcCmdqMessage_t
			*)(handle->iwcMessage))->command.pVABase),
			((iwcCmdqMessage_t
			*)(handle->iwcMessage))->command.commandSize);
#endif

		tzRes =	KREE_RegisterSharedmem(tee->mem_session,
			&cmdq_share_handle,
			&cmdq_shared_param);
		if (tzRes != TZ_RESULT_SUCCESS) {
			CMDQ_ERR("regSharedmem Error: %d, mem_session(%x)\n",
				 tzRes, (unsigned int)(tee->mem_session));
			return tzRes;
		}

		/* KREE_Tee service call */
		cmdq_param[0].memref.handle = (uint32_t) cmdq_share_handle;
		cmdq_param[0].memref.offset = 0;
		cmdq_param[0].memref.size = cmdq_shared_param.size;
		paramTypes = TZ_ParamTypes1(TZPT_MEMREF_INPUT);

		tzRes =	KREE_TeeServiceCall(tee->session,
					tee->share_memory->cmd,
					paramTypes,
					cmdq_param);
		if (tzRes != TZ_RESULT_SUCCESS) {
			CMDQ_ERR("TeeServiceCall fail, ret=0x%x\n", tzRes);
			return tzRes;
		}

		/* Unregister share memory */
		KREE_UnregisterSharedmem(tee->mem_session, cmdq_share_handle);
	} while (0);

	CMDQ_PROF_END("CMDQ_SEC_EXE");

	/* return tee service call result */
	return tzRes;
}


