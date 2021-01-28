/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef __CMDQ_SEC_H__
#define __CMDQ_SEC_H__

#include "cmdq_helper_ext.h"

#if defined(CMDQ_SECURE_PATH_SUPPORT)
#include "tee_client_api.h"
#include "cmdq_sec_iwc_common.h"
#if defined(CMDQ_GP_SUPPORT)
#include "cmdq_sec_gp.h"
#else
#include "cmdq_sec_trustonic.h"
#endif
#endif /* CMDQ_SECURE_PATH_SUPPORT */


/*
 * error code for CMDQ
 */
#define CMDQ_ERR_NULL_SEC_CTX_HANDLE (6000)
#define CMDQ_ERR_SEC_CTX_SETUP (6001)
#define CMDQ_ERR_SEC_CTX_TEARDOWN (6002)

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

/* CMDQ secure context struct
 * note it is not global data, each process has its own CMDQ sec context
 */
struct cmdqSecContextStruct {
	struct list_head listEntry;

	/* basic info */
	uint32_t tgid;		/* tgid of process context */
	uint32_t referCount;	/* reference count for open cmdq device node */

	/* iwc state */
	enum CMDQ_IWC_STATE_ENUM state;

	/* iwc information */
	void *iwcMessage;	/* message buffer */
	void *iwcMessageEx;	/* message buffer extra */
	void *iwcMessageEx2;	/* message buffer extra */

#if defined(CMDQ_SECURE_PATH_SUPPORT)
	struct cmdq_sec_tee_context tee;	/* trustzone parameters */
#endif
};

extern const u32 isp_iwc_buf_size[];

int32_t cmdq_sec_init_allocate_resource_thread(void *data);

s32 cmdq_sec_task_copy_to_buffer(struct cmdqRecStruct *task,
	struct cmdqCommandStruct *desc);

const struct cmdq_controller *cmdq_sec_get_controller(void);

/*
 * Create and destroy non-cachable shared memory,
 * used to share data for CMDQ driver between NWd and SWd
 *
 * Be careful that we should not disvlose any information about secure buffer
 * address of
 */
int32_t cmdq_sec_create_shared_memory(
	struct cmdqSecSharedMemoryStruct **pHandle, const uint32_t size);
int32_t cmdq_sec_destroy_shared_memory(
	struct cmdqSecSharedMemoryStruct *handle);

/*
 * Callback to fill message buffer for secure task
 *
 * Params:
 *     init32_t command id
 *     void*	pornter of TaskStruct
 *     int32_t  CMDQ HW thread id
 *     void*    the inter-world communication buffer
 * Return:
 *     >=0 for success;
 */
typedef int32_t(*CmdqSecFillIwcCB) (
	int32_t, void *, int32_t, void *, void *, void *);


/*
 * Entry secure world to handle secure path jobs
 * .submit task
 * .cancel error task
 */

int32_t cmdq_sec_exec_task_async_unlocked(
	struct cmdqRecStruct *pTask, int32_t thread);
int32_t cmdq_sec_cancel_error_task_unlocked(
	struct cmdqRecStruct *pTask, int32_t thread,
	struct cmdqSecCancelTaskResultStruct *pResult);
int32_t cmdq_sec_allocate_path_resource_unlocked(bool throwAEE);
s32 cmdq_sec_get_secure_thread_exec_counter(const s32 thread);
s32 cmdq_sec_revise_jump(struct cmdqRecStruct *handle);
s32 cmdq_sec_handle_error_result(struct cmdqRecStruct *task, s32 thread,
	const s32 waitQ, bool throw_aee);


/* function declaretion */
struct cmdqSecContextStruct *cmdq_sec_context_handle_create(uint32_t tgid);
s32 cmdq_sec_insert_backup_cookie_instr(struct cmdqRecStruct *task, s32 thread);
void cmdq_sec_dump_secure_thread_cookie(s32 thread);
s32 cmdq_mbox_sec_chan_id(void *chan);

/*
 * secure path control
 */
void cmdq_sec_lock_secure_path(void);
void cmdq_sec_unlock_secure_path(void);

void cmdqSecInitialize(void);
void cmdqSecDeInitialize(void);

void cmdqSecEnableProfile(const bool enable);

#if defined(CMDQ_SECURE_PATH_SUPPORT)
/*
 * tee vendor interface
 */
void cmdq_sec_setup_tee_context(struct cmdq_sec_tee_context *tee);

s32 cmdq_sec_init_context(struct cmdq_sec_tee_context *tee);

s32 cmdq_sec_deinit_context(struct cmdq_sec_tee_context *tee);

s32 cmdq_sec_allocate_wsm(struct cmdq_sec_tee_context *tee, void **wsm_buffer,
	u32 size, void **wsm_buf_ex, u32 size_ex,
	void **wsm_buf_ex2, u32 size_ex2);

s32 cmdq_sec_free_wsm(struct cmdq_sec_tee_context *tee, void **wsm_buffer);

s32 cmdq_sec_open_session(struct cmdq_sec_tee_context *tee, void *wsm_buffer);

s32 cmdq_sec_close_session(struct cmdq_sec_tee_context *tee);

s32 cmdq_sec_execute_session(struct cmdq_sec_tee_context *tee,
	u32 cmd, s32 timeout_ms, bool share_mem_ex1, bool share_mem_ex2);
#endif	/* CMDQ_SECURE_PATH_SUPPORT */

#endif				/* __DDP_CMDQ_SEC_H__ */
