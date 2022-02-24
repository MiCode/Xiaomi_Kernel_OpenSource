// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "cmdq_sec_trustonic.h"

void cmdq_sec_setup_tee_context(struct cmdq_sec_tee_context *tee)
{
	/* 09010000 0000 0000 0000000000000000 */
	tee->uuid = (struct mc_uuid_t){ { 9, 1, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0 } };
}

s32 cmdq_sec_init_context(struct cmdq_sec_tee_context *tee)
{
	s32 status;
	enum mc_result mcRet = MC_DRV_ERR_UNKNOWN;
	s32 retry_cnt = 0, max_retry = 30;

	do {
		status = 0;
		mcRet = mc_open_device(MC_DEVICE_ID_DEFAULT);

		/* Currently, a process context limits to open mobicore device
		 * once, and mc_open_device dose not support reference cout
		 * so skip the false alarm error....
		 */
		if (mcRet == MC_DRV_ERR_INVALID_OPERATION) {
			CMDQ_MSG(
				"[SEC]init_context: already opened, continue to execution\n");
			status = 0;
			tee->open_mobicore_by_other = 1;
		} else if (mcRet != MC_DRV_OK) {
			CMDQ_ERR("[SEC]init_context: err:0x%x, retry:%d\n",
				mcRet, retry_cnt);
			status = -1;
			msleep_interruptible(2000);
			retry_cnt++;
			continue;
		}
		break;
	} while (retry_cnt < max_retry);

	if (retry_cnt >= max_retry) {
		/* print error message */
		CMDQ_ERR(
			"[SEC]init_context fail: status:%d mcRet:0x%x retry:%d\n",
			status, mcRet, retry_cnt);
	} else {
		CMDQ_MSG("[SEC]init_context: status:%d mcRet:0x%x retry:%d\n",
			status, mcRet, retry_cnt);
	}

	return status;
}

s32 cmdq_sec_deinit_context(struct cmdq_sec_tee_context *tee)
{
	s32 status = 0;
	enum mc_result mcRet = 0;

	if (tee->open_mobicore_by_other == 1) {
		/* do nothing */
		/* let last user to close mobicore.... */
		CMDQ_MSG(
			"[SEC]_MOBICORE_CLOSE: opened by other, bypass device close\n");
	} else {
		mcRet = mc_close_device(MC_DEVICE_ID_DEFAULT);
		CMDQ_MSG(
			"[SEC]_MOBICORE_CLOSE: status:%d ret:0x%x openMobicoreByOther:%d\n",
			 status, mcRet, tee->open_mobicore_by_other);
		if (mcRet != MC_DRV_OK) {
			CMDQ_ERR("[SEC]_MOBICORE_CLOSE: err:0x%x\n", mcRet);
			status = -1;
		}
	}

	return status;
}

s32 cmdq_sec_allocate_wsm(struct cmdq_sec_tee_context *tee,
	void **wsm_buffer, u32 size)
{
	s32 status;
	enum mc_result mcRet = MC_DRV_OK;

	if (!wsm_buffer)
		return -EINVAL;

	/* because world shared mem(WSM) will ba managed by mobicore device,
	 * instead of linux kernel vmalloc/kmalloc, call mc_malloc_wasm to
	 * alloc WSM to prvent error such as "can not resolve tci physicall
	 * address" etc
	 */
	mcRet = mc_malloc_wsm(MC_DEVICE_ID_DEFAULT, 0, size,
		(u8 **)wsm_buffer, 0);
	if (mcRet != MC_DRV_OK) {
		CMDQ_ERR("[SEC]allocate_wsm: err:0x%x\n", mcRet);
		status = -EINVAL;
	} else {
		CMDQ_MSG("[SEC]allocate_wsm: status:%d *ppWsm:0x%p\n",
			status, *wsm_buffer);
		tee->wsm_size = size;
	}

	return status;
}

s32 cmdq_sec_free_wsm(struct cmdq_sec_tee_context *tee,
	void **wsm_buffer)
{
	s32 status = 0;
	enum mc_result mcRet;

	if (!wsm_buffer)
		return -EINVAL;

	mcRet = mc_free_wsm(MC_DEVICE_ID_DEFAULT,
		*wsm_buffer);

	if (mcRet != MC_DRV_OK) {
		CMDQ_ERR("free_wsm: err:0x%x", mcRet);
		status = -EINVAL;
	} else {
		*wsm_buffer = NULL;
		CMDQ_VERBOSE("free_wsm: ret:0x%x\n", mcRet);
	}

	return status;
}

s32 cmdq_sec_open_session(struct cmdq_sec_tee_context *tee,
	void *wsm_buffer)
{
	s32 status;
	s32 retry_cnt = 0, max_retry = 30;
	enum mc_result mcRet = MC_DRV_OK;

	if (!wsm_buffer) {
		CMDQ_ERR("[SEC]open_session: invalid param wsm buffer:0x%p\n",
			wsm_buffer);
		return -EINVAL;
	}

	tee->session.device_id = MC_DEVICE_ID_DEFAULT;

	do {
		mcRet = mc_open_session(&tee->session,
			&tee->uuid, wsm_buffer, tee->wsm_size);
		if (mcRet != MC_DRV_OK) {
			CMDQ_ERR("[SEC]open_session: err:0x%x retry:%d\n",
				mcRet, retry_cnt);
			retry_cnt++;
			msleep_interruptible(2000);
			status = -1;
			continue;
		}

		/* Open Session success */
		status = 0;
		break;
	} while (retry_cnt < max_retry);

	if (retry_cnt >= max_retry) {
		/* print error message */
		CMDQ_ERR(
			"[SEC]open_session fail: status:%d mcRet:0x%x retry:%d\n",
			status, mcRet, retry_cnt);
	} else {
		CMDQ_MSG(
			"[SEC]open_session: status:%d mcRet:0x%x retry:%d\n",
			status, mcRet, retry_cnt);
	}

	return status;
}

s32 cmdq_sec_close_session(struct cmdq_sec_tee_context *tee)
{
	s32 status = 0;
	enum mc_result mcRet = mc_close_session(&tee->session);

	if (mcRet != MC_DRV_OK) {
		CMDQ_ERR("close_session: err:0x%x", mcRet);
		status = -1;
	}
	return status;
}

s32 cmdq_sec_execute_session(struct cmdq_sec_tee_context *tee,
	s32 timeout_ms)
{
	enum mc_result mcRet;
	s32 status = 0;
	const s32 sec_timeout = timeout_ms > 0 ?
		timeout_ms : MC_INFINITE_TIMEOUT;

	CMDQ_PROF_START(current->pid, "CMDQ_SEC_EXE");

	do {
		/* notify to secure world */
		mcRet = mc_notify(&tee->session);
		if (mcRet != MC_DRV_OK) {
			CMDQ_ERR("[SEC]EXEC: mc_notify err:0x%x\n", mcRet);
			status = -1;
			break;
		}

		CMDQ_MSG("[SEC]EXEC: mc_notify ret:0x%x\n", mcRet);

		/* wait respond */
		mcRet = mc_wait_notification(&tee->session, sec_timeout);
		if (mcRet == MC_DRV_ERR_TIMEOUT) {
			CMDQ_ERR(
				"[SEC]EXEC: mc_wait_notification timeout, err:0x%x secureWoldTimeout_ms:%d\n",
				mcRet, sec_timeout);
			status = -ETIMEDOUT;
			break;
		}

		if (mcRet != MC_DRV_OK) {
			CMDQ_ERR("[SEC]EXEC: mc_wait_notification err:0x%x\n",
				mcRet);
			status = -1;
			break;
		}

		CMDQ_MSG("[SEC]EXEC: mc_wait_notification err:%d\n", mcRet);
	} while (0);

	CMDQ_PROF_END(current->pid, "CMDQ_SEC_EXE");

	return status;
}

