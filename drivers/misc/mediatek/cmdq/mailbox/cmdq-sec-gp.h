/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __CMDQ_SEC_GP_H__
#define __CMDQ_SEC_GP_H__

#include <linux/types.h>
#include <linux/delay.h>

#include "tee_client_api.h"
#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
#include "teei_client_main.h"
#endif
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
#include "mobicore_driver_api.h"
#endif

/* context for tee vendor */
struct cmdq_sec_tee_context {
	/* Universally Unique Identifier of secure tl/dr */
	struct TEEC_UUID uuid;
	struct TEEC_Context gp_context; /* basic context */
	struct TEEC_Session session; /* session handle */
	struct TEEC_SharedMemory shared_mem[4]; /* shared memory */
};

void cmdq_sec_setup_tee_context(struct cmdq_sec_tee_context *tee);
s32 cmdq_sec_init_context(struct cmdq_sec_tee_context *tee);
s32 cmdq_sec_allocate_wsm(struct cmdq_sec_tee_context *tee,
	void **wsm_buffer, u8 idx, u32 size);
s32 cmdq_sec_open_session(struct cmdq_sec_tee_context *tee, void *wsm_buffer);
s32 cmdq_sec_execute_session(struct cmdq_sec_tee_context *tee,
	u32 cmd, s32 timeout_ms, bool mem_ex1, bool mem_ex2);

int m4u_sec_init(void);

#endif	/* __CMDQ_SEC_GP_H__ */
