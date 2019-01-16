#ifndef __CMDQ_SEC_H__
#define __CMDQ_SEC_H__

#include "cmdq_core.h"

#if defined(CMDQ_SECURE_PATH_SUPPORT)
#include "mobicore_driver_api.h"
#include "cmdq_sec_iwc_common.h"
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
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	struct mc_uuid_t uuid;	/* Universally Unique Identifier of secure tl/dr */
	struct mc_session_handle sessionHandle;	/* session handle */
#endif
	uint32_t openMobicoreByOther;	/* true if someone has opened mobicore device in this prpocess context */
} cmdqSecContextStruct, *cmdqSecContextHandle;

/**
 * Create and destory non-cachable shared memory,
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
typedef int32_t(*CmdqSecFillIwcCB) (int32_t, void *, int32_t, void *);


/**
  * Entry secure world to handle secure path jobs
  * .submit task
  * .cancel error task
  */

int32_t cmdq_sec_exec_task_async_unlocked(TaskStruct *pTask, int32_t thread);
int32_t cmdq_sec_cancel_error_task_unlocked(TaskStruct *pTask, int32_t thread, cmdqSecCancelTaskResultStruct *pResult);
int32_t cmdq_sec_allocate_path_resource_unlocked(void);


/**
  * secure path control
  */
void cmdq_sec_lock_secure_path(void);
void cmdq_sec_unlock_secure_path(void);

void cmdqSecInitialize(void);
void cmdqSecDeInitialize(void);

void cmdqSecEnableProfile(const bool enable);

#endif				/* __DDP_CMDQ_SEC_H__ */
