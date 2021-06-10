/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CMDQ_SEC_MAILBOX_H__
#define __CMDQ_SEC_MAILBOX_H__

#define CMDQ_INVALID_THREAD		(-1)
#define CMDQ_SEC_IRQ_THREAD		(15)

/* Thread that are high-priority (display threads) */
#define CMDQ_MAX_SECURE_THREAD_COUNT	(5)
#define CMDQ_MIN_SECURE_THREAD_ID	(8)

#define CMDQ_THREAD_SEC_PRIMARY_DISP	(CMDQ_MIN_SECURE_THREAD_ID)
#define CMDQ_THREAD_SEC_SUB_DISP	(CMDQ_MIN_SECURE_THREAD_ID + 1)
#define CMDQ_THREAD_SEC_MDP		(CMDQ_MIN_SECURE_THREAD_ID + 2)
#define CMDQ_THREAD_SEC_ISP		(CMDQ_MIN_SECURE_THREAD_ID + 3)

/* max value of CMDQ_THR_EXEC_CMD_CNT (value starts from 0) */
#ifdef CMDQ_USE_LARGE_MAX_COOKIE
#define CMDQ_MAX_COOKIE_VALUE           (0xFFFFFFFF)
#else
#define CMDQ_MAX_COOKIE_VALUE           (0xFFFF)
#endif

/* error code for CMDQ */
#define CMDQ_ERR_NULL_SEC_CTX_HANDLE (6000)
#define CMDQ_ERR_SEC_CTX_SETUP (6001)
#define CMDQ_ERR_SEC_CTX_TEARDOWN (6002)

#ifdef CMDQ_SECURE_MTEE_SUPPORT
#include "cmdq_sec_mtee.h"
#endif

/*
 * inter-world communication state
 */
enum CMDQ_IWC_STATE_ENUM {
	IWC_INIT = 0,
	IWC_CONTEXT_INITED = 1,
	IWC_WSM_ALLOCATED = 2,
	IWC_SES_OPENED = 3,
	IWC_SES_MSG_PACKAGED = 4,
	IWC_SES_TRANSACTED = 5,
	IWC_SES_ON_TRANSACTED = 6,
	IWC_END_OF_ENUM = 7,
};

void cmdq_sec_mbox_enable(void *chan);
void cmdq_sec_mbox_disable(void *chan);
s32 cmdq_sec_mbox_chan_id(void *chan);
void cmdq_sec_dump_secure_thread_cookie(struct mbox_chan *chan);
void cmdq_sec_dump_thread_all(void *mbox_cmdq);
void cmdq_sec_dump_notify_loop(void *chan);
void cmdq_sec_dump_operation(void *chan);
void cmdq_sec_dump_response(void *chan, struct cmdq_pkt *pkt,
	u64 **inst, const char **dispatch);

#ifdef CMDQ_SECURE_MTEE_SUPPORT
void cmdq_sec_mtee_setup_context(struct cmdq_sec_mtee_context *tee);
s32 cmdq_sec_mtee_allocate_shared_memory(struct cmdq_sec_mtee_context *tee,
	const dma_addr_t MVABase, const u32 size);
s32 cmdq_sec_mtee_allocate_wsm(struct cmdq_sec_mtee_context *tee,
	void **wsm_buffer, u32 size, void **wsm_buf_ex, u32 size_ex,
	void **wsm_buf_ex2, u32 size_ex2);
s32 cmdq_sec_mtee_free_wsm(
	struct cmdq_sec_mtee_context *tee, void **wsm_buffer);
s32 cmdq_sec_mtee_open_session(
	struct cmdq_sec_mtee_context *tee, void *wsm_buffer);
s32 cmdq_sec_mtee_close_session(struct cmdq_sec_mtee_context *tee);
s32 cmdq_sec_mtee_execute_session(struct cmdq_sec_mtee_context *tee,
	u32 cmd, s32 timeout_ms, bool share_mem_ex, bool share_mem_ex2);
#endif

#if IS_ENABLED(CONFIG_MMPROFILE)
void cmdq_sec_mmp_wait(struct mbox_chan *chan, void *pkt);
void cmdq_sec_mmp_wait_done(struct mbox_chan *chan, void *pkt);
#endif

#endif
