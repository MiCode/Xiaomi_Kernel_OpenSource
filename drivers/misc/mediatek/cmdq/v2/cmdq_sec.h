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

#ifndef __CMDQ_SEC_H__
#define __CMDQ_SEC_H__

#include "cmdq_core.h"

#ifndef CONFIG_MTK_CMDQ_TAB
#if defined(CMDQ_SECURE_PATH_SUPPORT)
#include "mobicore_driver_api.h"
#include "cmdq_sec_iwc_common.h"
#endif
#else
#include "cmdq_sec_iwc_common.h"
#ifdef CMDQ_SECURE_PATH_SUPPORT
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#endif
#endif

/**
 * error code for CMDQ
 */

#define CMDQ_ERR_NULL_SEC_CTX_HANDLE (6000)
#define CMDQ_ERR_SEC_CTX_SETUP (6001)
#define CMDQ_ERR_SEC_CTX_TEARDOWN (6002)

/**
 * inter-world communication state
 */
typedef enum {
	IWC_INIT = 0,
	IWC_MOBICORE_OPENED = 1,
	IWC_WSM_ALLOCATED = 2,
	IWC_SES_OPENED = 3,
	IWC_SES_MSG_PACKAGED = 4,
	IWC_SES_TRANSACTED = 5,
	IWC_SES_ON_TRANSACTED = 6,
	IWC_END_OF_ENUM = 7,
} CMDQ_IWC_STATE_ENUM;


/**
 * CMDQ secure context struct
 * note it is not global data, each process has its own CMDQ sec context
 */
typedef struct cmdqSecContextStruct {
	struct list_head listEntry;

	/* basic info */
	uint32_t tgid;		/* tgid of process context */
	uint32_t referCount;	/* reference count for open cmdq device node */

	/* iwc state */
	CMDQ_IWC_STATE_ENUM state;

	/* iwc information */
	void *iwcMessage;	/* message buffer */
#ifndef CONFIG_MTK_CMDQ_TAB
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	struct mc_uuid_t uuid;	/* Universally Unique Identifier of secure tl/dr */
	struct mc_session_handle sessionHandle;	/* session handle */
#endif
	uint32_t openMobicoreByOther;	/* true if someone has opened mobicore device in this prpocess context */
#else
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	KREE_SESSION_HANDLE sessionHandle;
	KREE_SESSION_HANDLE memSessionHandle;
#endif
#endif
} cmdqSecContextStruct, *cmdqSecContextHandle;

int32_t cmdq_sec_init_allocate_resource_thread(void *data);

/**
 * Create and destroy non-cachable shared memory,
 * used to share data for CMDQ driver between NWd and SWd
 *
 * Be careful that we should not disvlose any information about secure buffer address of
 */
int32_t cmdq_sec_create_shared_memory(cmdqSecSharedMemoryHandle *pHandle, const uint32_t size);
int32_t cmdq_sec_destroy_shared_memory(cmdqSecSharedMemoryHandle handle);

/**
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
#ifndef CONFIG_MTK_CMDQ_TAB
typedef int32_t(*CmdqSecFillIwcCB) (int32_t, void *, int32_t, void *);
#else
typedef int32_t(*CmdqSecFillIwcCB) (iwcCmdqMessage_t *_pIwc,
				    uint32_t iwcCommand,
				    struct TaskStruct *_pTask, int32_t thread);
#endif


/**
  * Entry secure world to handle secure path jobs
  * .submit task
  * .cancel error task
  */

int32_t cmdq_sec_exec_task_async_unlocked(TaskStruct *pTask, int32_t thread);
int32_t cmdq_sec_cancel_error_task_unlocked(TaskStruct *pTask, int32_t thread,
					    cmdqSecCancelTaskResultStruct *pResult);
int32_t cmdq_sec_allocate_path_resource_unlocked(bool throwAEE);


/**
  * secure path control
  */
void cmdq_sec_lock_secure_path(void);
void cmdq_sec_unlock_secure_path(void);

void cmdqSecInitialize(void);
void cmdqSecDeInitialize(void);

void cmdqSecEnableProfile(const bool enable);

/* function declaretion */
cmdqSecContextHandle cmdq_sec_context_handle_create(uint32_t tgid);



#ifdef CONFIG_MTK_CMDQ_TAB
int32_t cmdq_sec_submit_to_secure_world_async_unlocked(uint32_t iwcCommand,
						       struct TaskStruct *pTask,
						       int32_t thread,
						       CmdqSecFillIwcCB iwcFillCB, void *data, bool throwAEE);
cmdqSecContextHandle cmdq_sec_find_context_handle_unlocked(uint32_t tgid);
cmdqSecContextHandle cmdq_sec_acquire_context_handle(uint32_t tgid);
int32_t cmdq_sec_release_context_handle(uint32_t tgid);
void cmdq_sec_dump_context_list(void);

int cmdq_sec_init_secure_path(void *data);

void cmdq_sec_set_commandId(uint32_t cmdId);
const uint32_t cmdq_sec_get_commandId(void);
int32_t cmdq_sec_init_share_memory(void);
void cmdq_debug_set_sw_copy(int32_t value);
int32_t cmdq_debug_get_sw_copy(void);
void cmdq_sec_set_sec_print_count(uint32_t count, bool bPrint);
uint32_t cmdq_sec_get_sec_print_count(void);
int32_t cmdq_sec_get_log_level(void);

/* add for universal communicate with TEE */
struct transmitBufferStruct {
	void *pBuffer;		/* the share memory */
	uint32_t size;		/* share memory size */
#ifdef CMDQ_SECURE_PATH_SUPPORT
	KREE_SHAREDMEM_HANDLE shareMemHandle;
	KREE_SESSION_HANDLE cmdqHandle;
	KREE_SESSION_HANDLE memSessionHandle;
#endif
};


#if defined(CMDQ_SECURE_PATH_SUPPORT)
/* the session to communicate with TA */
KREE_SESSION_HANDLE cmdq_session_handle(void);
KREE_SESSION_HANDLE cmdq_mem_session_handle(void);
int32_t cmdq_sec_create_shared_memory(cmdqSecSharedMemoryHandle *pHandle, const uint32_t size);
int32_t cmdq_sec_destroy_shared_memory(cmdqSecSharedMemoryHandle handle);





int32_t cmdqSecRegisterSecureBuffer(struct transmitBufferStruct *pSecureData);
int32_t cmdqSecServiceCall(struct transmitBufferStruct *pSecureData, int32_t cmd);
int32_t cmdqSecUnRegisterSecureBuffer(struct transmitBufferStruct *pSecureData);
void cmdq_sec_register_secure_irq(void);
#endif


#endif

#endif				/* __DDP_CMDQ_SEC_H__ */
