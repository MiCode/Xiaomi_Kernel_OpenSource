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

#include "cmdq_sec.h"
#include "cmdq_device.h"
#include "cmdq_virtual.h"
#include <linux/slab.h>

#if defined(CMDQ_SECURE_PATH_SUPPORT)
/* secure path header */
#include "cmdqSecTl_Api.h"
#endif


#include "tz_cross/ta_mem.h"
#include "tz_cross/trustzone.h"
#include "kree/mem.h"

static atomic_t gDebugSecSwCopy = ATOMIC_INIT(0);
static atomic_t gDebugSecCmdId = ATOMIC_INIT(0);

#if defined(CMDQ_SECURE_PATH_SUPPORT)
static KREE_SESSION_HANDLE cmdq_session;
static KREE_SESSION_HANDLE cmdq_mem_session;
#endif

#if 1
/* lock to protect atomic secure task execution */
static DEFINE_MUTEX(gCmdqSecExecLock);

/* lock to protext atomic access
 * gCmdqSecContextList
 */
static DEFINE_MUTEX(gCmdqSecContextLock);
#if defined(CMDQ_SECURE_PATH_SUPPORT)
/* secure context list. note each porcess has its
 * own sec context
 */
static struct list_head	gCmdqSecContextList;
static cmdqSecContextHandle
	gCmdqSecContextHandle; /* secure context to cmdqSecTL */
static KREE_SHAREDMEM_HANDLE gCmdq_share_cookie_handle;
static uint32_t gSubmitTaskCount;
#endif
static uint32_t gSecPrintCount;

/*
 * for CMDQ_LOG_LEVEL
 * set log level to 0 to enable all logs
 * set log level to 3 to close all logs
 */
enum LOG_LEVEL {
	LOG_LEVEL_MSG = 0,
	LOG_LEVEL_LOG = 1,
	LOG_LEVEL_ERR = 2,
	LOG_LEVEL_MAX,
};

/* Set 1 to open once for each process context, because of below reasons:
 * 1. kmalloc size > 4KB, need pre-allocation to avoid memory fragmentation and
 * causes kmalloc fail.
 * 2. we entry secure world for config and trigger GCE, and wait in normal world
 */
#define CMDQ_OPEN_SESSION_ONCE (1)

/* function declaretion */

/* #if defined(CMDQ_SECURE_PATH_SUPPORT) */
/***********add from k2  START ***********/
void cmdq_sec_lock_secure_path(void)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	mutex_lock(&gCmdqSecExecLock);
	smp_mb(); /*memory barrier */
#else
	CMDQ_ERR("SVP feature is not on\n");
#endif
}

void cmdq_sec_unlock_secure_path(void)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	mutex_unlock(&gCmdqSecExecLock);
#else
	CMDQ_ERR("SVP feature is not on\n");
#endif
}

int32_t cmdq_sec_create_shared_memory(cmdqSecSharedMemoryHandle *pHandle,
				      const uint32_t size)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	cmdqSecSharedMemoryHandle handle = NULL;

	handle = kzalloc(sizeof(struct cmdqSecSharedMemoryStruct),
			 GFP_KERNEL);
	if (handle == NULL)
		return -ENOMEM;

	CMDQ_LOG("%s\n", __func__);

	handle->pVABase = kcalloc(size, sizeof(uint8_t), GFP_KERNEL);
	handle->size = size;
	*pHandle = handle;
#else
	CMDQ_ERR("SVP feature is not on\n");
#endif
	return 0;
}

int32_t cmdq_sec_destroy_shared_memory(cmdqSecSharedMemoryHandle handle)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	if (handle && handle->pVABase)
		kfree(handle->pVABase);

	kfree(handle);

#else
	CMDQ_ERR("SVP feature is not on\n");
#endif
	return 0;
}

/***********add from k2  END ***********/

/* the API for other code to acquire cmdq_mem session handle */
/* a NULL handle is returned when it fails */

#if defined(CMDQ_SECURE_PATH_SUPPORT)
KREE_SESSION_HANDLE cmdq_session_handle(void)
{
	CMDQ_MSG("cmdq_session_handle() acquire TEE session\n");
	if (cmdq_session == 0) {
		int ret;

		CMDQ_MSG("cmdq_session_handle() create session\n");
		CMDQ_LOG("TZ_TA_CMDQ_UUID:%s\n", TZ_TA_CMDQ_UUID);
		ret = KREE_CreateSession(TZ_TA_CMDQ_UUID, &cmdq_session);
		if (ret != TZ_RESULT_SUCCESS) {
			CMDQ_ERR(
				"cmdq_session_handle() failed to create session, ret=%d\n",
				ret);
			return 0;
		}
	}

	CMDQ_MSG("cmdq_session_handle() session=%x\n",
		 (unsigned int)cmdq_session);
	return cmdq_session;
}
#endif

#if defined(CMDQ_SECURE_PATH_SUPPORT)
KREE_SESSION_HANDLE cmdq_mem_session_handle(void)
{
	CMDQ_MSG("cmdq_mem_session_handle() acquires TEE memory session\n");
	if (cmdq_mem_session == 0) {
		int ret;

		CMDQ_MSG("cmdq_mem_session_handle() create memory session\n");

		ret = KREE_CreateSession(TZ_TA_MEM_UUID, &cmdq_mem_session);
		if (ret != TZ_RESULT_SUCCESS) {
			CMDQ_ERR(
				"cmdq_mem_session_handle() failed to create session: ret=%d\n",
				ret);
			return 0;
		}
	}

	CMDQ_MSG("cmdq_mem_session_handle() session=%x\n",
		 (unsigned int)cmdq_mem_session);
	return cmdq_mem_session;
}
#endif

#if defined(CMDQ_SECURE_PATH_SUPPORT)
static int32_t cmdq_sec_setup_context_session(cmdqSecContextHandle handle)
{
/* init handle:iwcMessage        sessionHandle   memSessionHandle */

#if CMDQ_OPEN_SESSION_ONCE
	if (handle->iwcMessage == NULL) {
#endif
		/* alloc message bufer */
		handle->iwcMessage =
			kmalloc(sizeof(struct iwcCmdqMessage_t), GFP_KERNEL);
		if (handle->iwcMessage == NULL) {
			CMDQ_ERR("handle->iwcMessage kmalloc failed!\n");
			return -ENOMEM;
		}
#if CMDQ_OPEN_SESSION_ONCE
	}
#endif
	CMDQ_MSG("handle->iwcMessage 0x%p\n", handle->iwcMessage);

	/* init session handle */
	handle->sessionHandle = cmdq_session_handle();
	if (handle->sessionHandle == 0)
		return -1;

	/* init memory session handle */
	handle->memSessionHandle = cmdq_mem_session_handle();
	if (handle->memSessionHandle == 0) {
		CMDQ_ERR("create cmdq_mem_session_handle error\n");
		return -2;
	}

	CMDQ_MSG("handle->sessionHandle 0x%x\n", handle->sessionHandle);
	CMDQ_MSG("handle->memSessionHandle 0x%x\n", handle->memSessionHandle);
	return 0;
}
#endif

#if defined(CMDQ_SECURE_PATH_SUPPORT)
static void cmdq_sec_deinit_session_unlocked(cmdqSecContextHandle handle)
{
	CMDQ_MSG("[SEC]-->SESSION_DEINIT\n");
	do {
		if (handle->iwcMessage != NULL) {
			kfree(handle->iwcMessage);
			handle->iwcMessage = NULL;
		}

	} while (0);

	CMDQ_MSG("[SEC]<--SESSION_DEINIT\n");
}
#endif

#if defined(CMDQ_SECURE_PATH_SUPPORT)
static int32_t cmdq_sec_fill_iwc_command_basic_unlocked(
	struct iwcCmdqMessage_t *_pIwc,
	uint32_t iwcCommand,
	struct TaskStruct *_pTask,
	int32_t thread)
{
	struct iwcCmdqMessage_t *pIwc;

	pIwc = (struct iwcCmdqMessage_t *)_pIwc;

	/* specify command id only, don't care other other */
	memset(pIwc, 0x0, sizeof(struct iwcCmdqMessage_t));
	pIwc->cmd = iwcCommand;

	/* medatada: debug config */
	/*pIwc->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) : (0); */
	pIwc->debug.logLevel = cmdq_sec_get_sec_print_count()
				       ? LOG_LEVEL_MSG
				       : cmdq_sec_get_log_level();
	pIwc->debug.enableProfile = cmdq_core_profile_enabled();
	return 0;
}
#endif

#if defined(CMDQ_SECURE_PATH_SUPPORT)
static int32_t cmdq_sec_fill_iwc_cancel_msg_unlocked(
	struct iwcCmdqMessage_t *_pIwc,
	uint32_t iwcCommand,
	struct TaskStruct *_pTask,
	int32_t thread)
{
	const struct TaskStruct *pTask = (struct TaskStruct *)_pTask;
	struct iwcCmdqMessage_t *pIwc = (struct iwcCmdqMessage_t *)_pIwc;

	if ((pIwc == NULL) || (pTask == NULL)) {
		CMDQ_ERR("%s invalid param\n", __func__);
		return -1;
	}

	memset(pIwc, 0x0, sizeof(struct iwcCmdqMessage_t));
	pIwc->cmd = iwcCommand;
	pIwc->cancelTask.waitCookie = pTask->secData.waitCookie;
	pIwc->cancelTask.thread = thread;
	/* medatada: debug config */
	/*	pIwc->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) :
	 * (0);
	 */
	pIwc->debug.logLevel = cmdq_sec_get_sec_print_count()
				       ? LOG_LEVEL_MSG
				       : cmdq_sec_get_log_level();
	pIwc->debug.enableProfile = cmdq_core_profile_enabled();
	CMDQ_LOG(
		"FILL:CANCEL_TASK: task: %p, thread:%d, cookie:%d, resetExecCnt:%d\n",
		pTask, thread, pTask->secData.waitCookie,
		pTask->secData.resetExecCnt);
	return 0;
}
#endif

#if defined(CMDQ_SECURE_PATH_SUPPORT)
static int32_t cmdq_sec_fill_iwc_command_msg_unlocked(
	struct iwcCmdqMessage_t *pIwc,
	uint32_t iwcCommand,
	struct TaskStruct *pTask,
	int32_t thread)
{
	int32_t status = 0;
	/* TEE will insert some instr,DAPC and M4U configuration */
	const uint32_t reservedCommandSize = 4 * CMDQ_INST_SIZE;
	struct CmdBufferStruct *cmd_buffer = NULL;
	uint32_t buffer_index = 0;

	CMDQ_MSG("enter fill_iwc_command_msg_unlock\n");

	if ((pIwc == NULL) || (pTask == NULL)) {
		CMDQ_ERR("%s invalid param", __func__);
		return -EFAULT;
	}

	/* check command size first */
	if (pTask && (CMDQ_TZ_CMD_BLOCK_SIZE <
		      (pTask->commandSize + reservedCommandSize))) {
		CMDQ_ERR("[SEC]SESSION_MSG: pTask %p commandSize %d > %d\n",
			 pTask,
			 pTask->commandSize,
			 CMDQ_TZ_CMD_BLOCK_SIZE);
		return -EFAULT;
	}

	CMDQ_MSG("[SEC]-->SESSION_MSG: cmdId[%d]\n", iwcCommand);

	/* fill message buffer for inter world communication */
	memset(pIwc, 0x0, sizeof(struct iwcCmdqMessage_t));
	pIwc->cmd = iwcCommand;
	/* metadata */
	pIwc->command.metadata.enginesNeedDAPC = pTask->secData.enginesNeedDAPC;
	pIwc->command.metadata.enginesNeedPortSecurity =
		pTask->secData.enginesNeedPortSecurity;
	pIwc->command.metadata.secMode = pTask->secData.secMode;

	if (NULL != pTask && CMDQ_INVALID_THREAD != thread) {
		/* basic data */
		pIwc->command.scenario = pTask->scenario;
		pIwc->command.thread = thread;
		pIwc->command.priority = pTask->priority;
		pIwc->command.engineFlag = pTask->engineFlag;
		pIwc->command.hNormalTask = (0LL | (uint64_t)(pTask));
		pIwc->command.commandSize = pTask->bufferSize;

		buffer_index = 0;
		list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list,
				    listEntry) {
			uint32_t copy_size =
				list_is_last(&cmd_buffer->listEntry,
					     &pTask->cmd_buffer_list)
					? CMDQ_CMD_BUFFER_SIZE -
						  pTask->buf_available_size
					: CMDQ_CMD_BUFFER_SIZE;
			uint32_t *start_va =
				(pIwc->command.pVABase +
				 buffer_index * CMDQ_CMD_BUFFER_SIZE /
					 CMDQ_INST_SIZE * 2);
			uint32_t *end_va =
				start_va + copy_size / sizeof(uint32_t);

			memcpy(start_va, (cmd_buffer->pVABase), (copy_size));

			/* we must reset the jump inst since now buffer is
			 * continues
			 */
			if (((end_va[-1] >> 24) & 0xff) == CMDQ_CODE_JUMP &&
			    (end_va[-1] & 0x1) == 1) {
				end_va[-1] = CMDQ_CODE_JUMP << 24;
				end_va[-2] = 0x8;
			}

			buffer_index++;
		}

		/* cookie */
		pIwc->command.waitCookie = pTask->secData.waitCookie;
		pIwc->command.resetExecCnt = pTask->secData.resetExecCnt;

		CMDQ_MSG(
			"[SEC]SESSION_MSG: task 0x%p, thread: %d, size: %d, bufferSize: %d, scenario:%d\n",
			pTask, thread, pTask->commandSize, pTask->bufferSize,
			pTask->scenario);
		CMDQ_MSG("flag:0x%08llx ,hNormalTask:0x%llx\n",
			 pTask->engineFlag, pIwc->command.hNormalTask);

		CMDQ_VERBOSE("[SEC]SESSION_MSG: addrList[%d][0x%llx]\n",
			     pTask->secData.addrMetadataCount,
			     pTask->secData.addrMetadatas);
		if (pTask->secData.addrMetadataCount > 0) {
			pIwc->command.metadata.addrListLength =
				pTask->secData.addrMetadataCount;
			memcpy((pIwc->command.metadata.addrList),
			       CMDQ_U32_PTR(pTask->secData.addrMetadatas),
			       (pTask->secData.addrMetadataCount) *
				       sizeof(struct iwcCmdqAddrMetadata_t));
			#if 0
			pIwc->command.metadata.srcHandle
			   = pTask->secData.srcHandle;
			pIwc->command.metadata.dstHandle
			   = pTask->secData.dstHandle;
			#endif
		}

		/* medatada: debug config */
		/*pIwc->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) :
		 * (0);
		 */
		pIwc->debug.logLevel = cmdq_sec_get_sec_print_count()
					       ? LOG_LEVEL_MSG
					       : cmdq_sec_get_log_level();
		pIwc->debug.enableProfile = cmdq_core_profile_enabled();
	} else {
		/* relase resource, or debug function will go here */
		CMDQ_VERBOSE("[SEC]-->SESSION_MSG: no task, cmdId[%d]\n",
			     iwcCommand);
		pIwc->command.commandSize = 0;
		pIwc->command.metadata.addrListLength = 0;
	}

	CMDQ_MSG("[SEC]<--SESSION_MSG[%d]\n", status);
	CMDQ_MSG("leaving fill_iwc_command_msg_unlock\n");
	return status;
}
#endif

void dump_message(const struct iwcCmdqMessage_t *message)
{
#ifdef DEBUG_SVP_INFO
	CMDQ_LOG("dump iwcCmdqMessage info\n");
	CMDQ_LOG(
		"cmdID:%d,thread:%d,scenario:%d,priority:%d,commandSize:%d,engineFlag:0x%llx\n",
		message->cmd, message->command.thread,
		message->command.scenario, message->command.priority,
		message->command.commandSize, message->command.engineFlag);
	CMDQ_LOG("pVABase:0x%p\n", message->command.pVABase);
	CMDQ_LOG("addrlistLen:%d,needDAPC:0x%llx,needPortSecurity:0x%llx\n",
		 message->command.metadata.addrListLength,
		 message->command.metadata.enginesNeedDAPC,
		 message->command.metadata.enginesNeedPortSecurity);
#else
	CMDQ_ERR("please open DEBUG_SVP_INFO macro first\n");
#endif
}

#if defined(CMDQ_SECURE_PATH_SUPPORT)
static int32_t cmdq_sec_execute_session_unlocked(cmdqSecContextHandle handle)
{
	int tzRes;

	CMDQ_PROF_START("CMDQ_SEC_EXE");

	do {
		/* Register share memory */
		union MTEEC_PARAM cmdq_param[4];
		unsigned int paramTypes;
		KREE_SHAREDMEM_HANDLE cmdq_share_handle = 0;
		struct KREE_SHAREDMEM_PARAM cmdq_shared_param;
		struct iwcCmdqMessage_t *pMessage =
			(struct iwcCmdqMessage_t *)(handle->iwcMessage);
#if 1
		/* allocate path init for shared cookie */
		if (CMD_CMDQ_TL_INIT_SHARED_MEMORY ==
		    pMessage->cmd) {
			cmdq_shared_param.buffer =
				cmdq_core_get_cmdqcontext()
					->hSecSharedMem->pVABase;
			cmdq_shared_param.size = cmdq_core_get_cmdqcontext()
							 ->hSecSharedMem->size;
			CMDQ_MSG("cmdq_shared_param.buffer %p\n",
				 cmdq_shared_param.buffer);
			CMDQ_MSG("handle->memSessionHandle:%d\n",
				 (uint32_t)handle->memSessionHandle);
			tzRes = KREE_RegisterSharedmem(handle->memSessionHandle,
						       &cmdq_share_handle,
						       &cmdq_shared_param);
			/* save for unregister */
			gCmdq_share_cookie_handle = cmdq_share_handle;
			if (tzRes != TZ_RESULT_SUCCESS) {
				CMDQ_ERR(
				"cmdq register share memory Error: %d, line:%d, cmdq_mem_session_handle(%x)\n",
				tzRes, __LINE__,
				(unsigned int)(handle->memSessionHandle));
				return tzRes;
			}
			/* KREE_Tee service call */
			cmdq_param[0].memref.handle =
				(uint32_t)cmdq_share_handle;
			cmdq_param[0].memref.offset = 0;
			cmdq_param[0].memref.size = cmdq_shared_param.size;
			paramTypes = TZ_ParamTypes1(TZPT_MEMREF_INOUT);
			tzRes = KREE_TeeServiceCall(
				handle->sessionHandle,
				CMD_CMDQ_TL_INIT_SHARED_MEMORY, paramTypes,
				cmdq_param);
			if (tzRes != TZ_RESULT_SUCCESS) {
				CMDQ_ERR(
					"CMD_CMDQ_TL_INIT_SHARED_MEMORY fail, ret=0x%x\n",
					tzRes);
				return tzRes;
			}
			CMDQ_MSG("KREE_TeeServiceCall tzRes =0x%x\n", tzRes);
			break;
		}
#endif
		cmdq_shared_param.buffer = pMessage;
		cmdq_shared_param.size = (sizeof(struct iwcCmdqMessage_t));
		CMDQ_MSG("cmdq_shared_param.buffer %p\n",
			 cmdq_shared_param.buffer);
/* dump_message((struct iwcCmdqMessage_t *) handle->iwcMessage); */

#if 0 /* add for debug */
		do {
			struct iwcCmdqMessage_t *pMessage = NULL;

			pMessage = (struct iwcCmdqMessage_t *)
				(handle->iwcMessage);
			CMDQ_ERR("dump secure task instructions\n");
			cmdq_core_dump_instructions(
				(uint64_t *)(pMessage->command.pVABase),
				pMessage->command.commandSize);
		} while (0);
#endif

		tzRes = KREE_RegisterSharedmem(handle->memSessionHandle,
					       &cmdq_share_handle,
					       &cmdq_shared_param);
		if (tzRes != TZ_RESULT_SUCCESS) {
			CMDQ_ERR(
				"cmdq register share memory Error: %d, line:%d, cmdq_mem_session_handle(%x)\n",
				tzRes, __LINE__,
				(unsigned int)(handle->memSessionHandle));
			break;
		}

		/* KREE_Tee service call */
		cmdq_param[0].memref.handle = (uint32_t)cmdq_share_handle;
		cmdq_param[0].memref.offset = 0;
		cmdq_param[0].memref.size = cmdq_shared_param.size;
		paramTypes = TZ_ParamTypes1(TZPT_MEMREF_INPUT);

		CMDQ_MSG("commandID:%d\n",
			pMessage->cmd);
		CMDQ_MSG("handle->sessionHandle:%x\n",
			handle->sessionHandle);
		CMDQ_MSG("start to enter Secure World\n");

		if (CMD_CMDQ_TL_SUBMIT_TASK ==
		    pMessage->cmd)
			gSubmitTaskCount++;
		tzRes = KREE_TeeServiceCall(
			handle->sessionHandle,
			pMessage->cmd,
			paramTypes, cmdq_param);
		if (tzRes != TZ_RESULT_SUCCESS) {
			CMDQ_ERR(
				"leave secure world KREE_TeeServiceCall fail, ret=0x%x\n",
				tzRes);
			break;
		}
		CMDQ_MSG("leave secure world KREE_TeeServiceCall tzRes =0x%x\n",
			 tzRes);

		/* Unregister share memory */
		tzRes = KREE_UnregisterSharedmem(handle->memSessionHandle,
						 cmdq_share_handle);
		if (tzRes != TZ_RESULT_SUCCESS) {
			CMDQ_ERR("KREE_UnregisterSharedmem fail, ret=0x%x\n",
				 tzRes);
			break;
		}

		/* wait respond */

		/* config with timeout */

	} while (0);

	CMDQ_PROF_END("CMDQ_SEC_EXE");

	/* return tee service call result */
	return tzRes;
}
#endif

CmdqSecFillIwcCB
cmdq_sec_get_iwc_msg_fill_cb_by_iwc_command(uint32_t iwcCommand)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	CmdqSecFillIwcCB cb = NULL;

	switch (iwcCommand) {
	case CMD_CMDQ_TL_CANCEL_TASK:
		cb = cmdq_sec_fill_iwc_cancel_msg_unlocked;
		break;
	case CMD_CMDQ_TL_SUBMIT_TASK:
		cb = cmdq_sec_fill_iwc_command_msg_unlocked;
		break;
	case CMD_CMDQ_TL_TEST_HELLO_TL:
	case CMD_CMDQ_TL_TEST_DUMMY:
	default:
		cb = cmdq_sec_fill_iwc_command_basic_unlocked;
		break;
	};
	return cb;
#else
	CMDQ_ERR("SVP feature is not on\n");
	return (CmdqSecFillIwcCB)NULL;
#endif
}

int32_t cmdq_sec_send_context_session_message(cmdqSecContextHandle handle,
					      uint32_t iwcCommand,
					      struct TaskStruct *pTask,
					      int32_t thread,
					      CmdqSecFillIwcCB iwcFillCB,
					      void *data)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	int32_t status = 0;
	int32_t iwcRsp = 0;
	const CmdqSecFillIwcCB icwcFillCB =
		(iwcFillCB == NULL)
			? cmdq_sec_get_iwc_msg_fill_cb_by_iwc_command(
				  iwcCommand)
			: (iwcFillCB);
	CMDQ_MSG("enter cmdq_sec_send_context_session_message()");

	do {
		struct iwcCmdqMessage_t *pMessage =
			(struct iwcCmdqMessage_t *)(handle->iwcMessage);
		/* fill message bufer */
		/*debug level */
		pMessage->debug.logLevel =
			cmdq_sec_get_sec_print_count()
				? LOG_LEVEL_MSG
				: cmdq_sec_get_log_level();
		status = icwcFillCB(pMessage,
				    iwcCommand, pTask, thread);
		if (status < 0)
			break;

		/* send message */
		status = cmdq_sec_execute_session_unlocked(handle);
		if (status != TZ_RESULT_SUCCESS) {
			CMDQ_ERR(
				"cmdq_sec_execute_session_unlocked status is %d\n",
				status);
			break;
		}
		/* get secure task execution result */
		/*
		 * iwcRsp = ((struct iwcCmdqMessage_t*)
		 * (handle->iwcMessage))->rsp;
		 * status = iwcRsp; //(0 == iwcRsp)? (0):(-EFAULT);
		 */

		/* and then, update task state */
		/* log print */
		if (status > 0) {
			CMDQ_ERR(
				"SEC_SEND: status[%d], cmdId[%d], iwcRsp[%d]\n",
				status, iwcCommand, iwcRsp);
		} else {
			CMDQ_MSG(
				"SEC_SEND: status[%d], cmdId[%d], iwcRsp[%d]\n",
				status, iwcCommand, iwcRsp);
		}
	} while (0);

	return status;
#else
	CMDQ_ERR("SVP feature is not on\n");
	return 0;
#endif
}

#if 0
/*added only for cmdq unit test begin */
int32_t cmdq_sec_test_proc(int testValue)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	const int32_t tgid = current->tgid;
	cmdqSecContextHandle handle = NULL;
	/* fill param */
	MTEEC_PARAM param[4];
	unsigned int paramTypes;
	int ret;

	CMDQ_MSG("DISP_IOCTL_CMDQ_SEC_TEST cmdq_test_proc\n");

	do {
		/* find handle first */
		/* handle = cmdq_sec_find_context_handle_unlocked(tgid); */
		handle = cmdq_sec_acquire_context_handle(tgid);
		if (handle == NULL) {
			CMDQ_ERR("SEC_SUBMIT: tgid %d err[NULL secCtxHandle]\n",
				tgid);
			ret = -(CMDQ_ERR_NULL_SEC_CTX_HANDLE);
			break;
		}


		param[0].value.a = (uint32_t) 111;
		param[1].value.a = (uint32_t) testValue;

		paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);

		ret =
		    KREE_TeeServiceCall(cmdq_session_handle(),
				CMD_CMDQ_TL_TEST_HELLO_TL,
				paramTypes, param);
		if (ret != TZ_RESULT_SUCCESS)
			CMDQ_ERR("TZCMD_CMDQ_TEST_HELLO fail, ret=%x\n", ret);
		else
			CMDQ_MSG("KREE_TeeServiceCall OK:%d\n", ret);

	} while (0);
	return ret;
#else
	CMDQ_ERR("SVP feature is not on\n");
	return 0;
#endif

}

/*added only for cmdq unit test end */
#endif

#if !(CMDQ_OPEN_SESSION_ONCE)
static int32_t cmdq_sec_teardown_context_session(cmdqSecContextHandle handle)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	int32_t status = 0;

	if (handle) {
		CMDQ_MSG("SEC_TEARDOWN:iwcMessage:0x%p\n", handle->iwcMessage);
		cmdq_sec_deinit_session_unlocked(handle);
	} else {
		CMDQ_ERR("SEC_TEARDOWN: null secCtxHandle\n");
		status = -1;
	}
	return status;
#else
	CMDQ_ERR("SVP feature is not on\n");
	return 0;
#endif
}
#endif

int32_t cmdq_sec_submit_to_secure_world_async_unlocked(
	uint32_t iwcCommand, struct TaskStruct *pTask, int32_t thread,
	CmdqSecFillIwcCB iwcFillCB, void *data, bool throwAEE)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT)

	const int32_t tgid = current->tgid;
	const int32_t pid = current->pid;

	cmdqSecContextHandle handle = NULL;
	int32_t status = 0;
	int32_t duration = 0;

	CMDQ_TIME tEntrySec;
	CMDQ_TIME tExitSec;

#if 0
	if (pTask != NULL)
		cmdq_core_dump_secure_metadata(&(pTask->secData));
#endif

	CMDQ_MSG("[SEC]-->SEC_SUBMIT: tgid[%d:%d]\n", tgid, pid);
	do {
		/* find handle first */

		if (gCmdqSecContextHandle == NULL)
			gCmdqSecContextHandle =
				cmdq_sec_context_handle_create(current->tgid);

		handle = gCmdqSecContextHandle;

		/* find handle first */
		/* handle = cmdq_sec_find_context_handle_unlocked(tgid); */
		/* handle = cmdq_sec_acquire_context_handle(tgid); */
		if (handle == NULL) {
			CMDQ_ERR("SEC_SUBMIT: tgid %d err[NULL secCtxHandle]\n",
				 tgid);
			status = -(CMDQ_ERR_NULL_SEC_CTX_HANDLE);
			break;
		}
		/* alloc iwc buffer */
		if (cmdq_sec_setup_context_session(handle) < 0) {
			status = -(CMDQ_ERR_SEC_CTX_SETUP);
			break;
		}
		CMDQ_MSG("leaving cmdq_sec_setup_context_session()\n");
		/*  */
		/* record profile data */
		/* tbase timer/time support is not enough currently, */
		/* so we treats entry/exit timing to secure world as the
		 * trigger/gotIRQ_&_wakeup timing
		 */
		/* if (CMD_CMDQ_TL_INIT_SHARED_MEMORY == iwcCommand)
		 * gCmdqContext.hSecSharedMem->handle = handle;
		 */

		tEntrySec = sched_clock();
		status = cmdq_sec_send_context_session_message(
			handle, iwcCommand, pTask, thread, iwcFillCB, data);
		tExitSec = sched_clock();
		CMDQ_GET_TIME_IN_US_PART(tEntrySec, tExitSec, duration);
		if (pTask) {
			pTask->trigger = tEntrySec;
			pTask->gotIRQ = tExitSec;
			pTask->wakedUp = tExitSec;
		}
#if !(CMDQ_OPEN_SESSION_ONCE)
		cmdq_sec_teardown_context_session(
			handle); /* teardown context session in Deinit */
#endif

		/* Note we entry secure for config only and wait result in
		 * normal world.
		 */
		/* No need reset module HW for config failed case */

	} while (0);

	if (-ETIMEDOUT == status) {
		/* t-base strange issue, mc_wait_notification false timeout when
		 * secure world has done
		 */
		/* because retry may failed, give up retry method */
		CMDQ_ERR(
			"CMDQ [SEC]<--SEC_SUBMIT: err[%d][mc_wait_notification timeout], pTask[0x%p], THR[%d], tgid[%d:%d]\n",
			status, pTask, thread, tgid, pid);
		CMDQ_ERR("config_duration_ms[%d], cmdId[%d]\n", duration,
			 iwcCommand);

	} else if (status < 0) {
#if 0
		if (!skipSecCtxDump) {
			mutex_lock(&gCmdqSecContextLock);
			cmdq_sec_dump_context_list();
			mutex_unlock(&gCmdqSecContextLock);
		}
#endif
		if (throwAEE) {
			/* throw AEE */
			CMDQ_AEE(
				"CMDQ",
				"[SEC]<--SEC_SUBMIT: err[%d], pTask[0x%p], THR[%d], tgid[%d:%d], config_duration_ms[%d], cmdId[%d]\n",
				status, pTask, thread, tgid, pid, duration,
				iwcCommand);
		}
	} else {
		CMDQ_MSG(
			"[SEC]<--SEC_SUBMIT: err[%d], pTask[0x%p], THR[%d], tgid[%d:%d], config_duration_ms[%d], cmdId[%d]\n",
			status, pTask, thread, tgid, pid, duration, iwcCommand);
	}
	return status;
#else
	CMDQ_ERR("SVP feature is not supported\n");
	return 0;
#endif
}

int32_t cmdq_sec_exec_task_async_unlocked(struct TaskStruct *pTask,
					  int32_t thread)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	status = cmdq_sec_submit_to_secure_world_async_unlocked(
		CMD_CMDQ_TL_SUBMIT_TASK, pTask, thread, NULL, NULL, true);

	if (status < 0)
		CMDQ_ERR("%s[%d]\n", __func__, status);

	return status;

#else
	CMDQ_ERR("SVP feature is not on\n");
	return -EFAULT;
#endif
}

int32_t
cmdq_sec_cancel_error_task_unlocked(struct TaskStruct *pTask, int32_t thread,
	struct cmdqSecCancelTaskResultStruct *pResult)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	if ((pTask == NULL) ||
	    (false == cmdq_get_func()->isSecureThread(thread)) ||
	    (pResult == NULL)) {
		CMDQ_ERR("%s invalid param, pTask:%p, thread:%d, pResult:%p\n",
			 __func__, pTask, thread, pResult);
		return -EFAULT;
	}
	status = cmdq_sec_submit_to_secure_world_async_unlocked(
		CMD_CMDQ_TL_CANCEL_TASK, pTask, thread, NULL, (void *)pResult,
		true);

	if (status <= 0)
		CMDQ_ERR("gSubmitTaskCount:%u\n", gSubmitTaskCount);

	return status;
#else
	CMDQ_ERR("secure path not support\n");
	return -EFAULT;
#endif
}

#if defined(CMDQ_SECURE_PATH_SUPPORT)
static atomic_t gCmdqSecPathResource = ATOMIC_INIT(0);
#endif

int32_t cmdq_sec_allocate_path_resource_unlocked(bool throwAEE)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	CMDQ_MSG("%s throwAEE: %s", __func__, throwAEE ? "true" : "false");

	if (atomic_cmpxchg(&gCmdqSecPathResource, 0, 1) != 0) {
		/* has allocated successfully */
		CMDQ_MSG("allocate path resource already\n");
		return status;
	}
	CMDQ_MSG("begine to allocate path resource\n");
	status = cmdq_sec_submit_to_secure_world_async_unlocked(
		CMD_CMDQ_TL_PATH_RES_ALLOCATE, NULL, -1, NULL, NULL, throwAEE);
	if (status < 0) {
		CMDQ_ERR("%s[%d] reset context\n", __func__, status);

		/* in fail case, we want function alloc again */
		atomic_set(&gCmdqSecPathResource, 0);
	}

	return status;
#else
	CMDQ_ERR("secure path not support\n");
	return -EFAULT;
#endif
}

int32_t cmdq_sec_init_share_memory(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	CMDQ_MSG("starting test init share memory\n");
	status = cmdq_sec_submit_to_secure_world_async_unlocked(
		CMD_CMDQ_TL_INIT_SHARED_MEMORY, NULL, -1, NULL, NULL, true);
	if (status < 0)
		CMDQ_ERR("%s[%d]\n", __func__, status);

	CMDQ_MSG("test init share memory end\n");
	return status;
#else
	CMDQ_ERR("secure path not support\n");
	return -EFAULT;
#endif
}

cmdqSecContextHandle cmdq_sec_find_context_handle_unlocked(uint32_t tgid)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	cmdqSecContextHandle handle = NULL;

	do {
		struct cmdqSecContextStruct *secContextEntry = NULL;
		struct list_head *pos = NULL;

		list_for_each(pos, &gCmdqSecContextList)
		{
			secContextEntry = list_entry(
				pos, struct cmdqSecContextStruct, listEntry);
			if (secContextEntry && tgid == secContextEntry->tgid) {
				handle = secContextEntry;
				break;
			}
		}
	} while (0);

	CMDQ_MSG("SecCtxHandle_SEARCH: Handle[0x%p], tgid[%d]\n", handle, tgid);
	return handle;
#else
	CMDQ_ERR("secure path not support\n");
	return NULL;
#endif
}

int32_t cmdq_sec_release_context_handle_unlocked(cmdqSecContextHandle handle)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	do {
		handle->referCount--;
		if (handle->referCount > 0)
			break;

		/* 1. clean up secure path in secure world */
		/* 2. delete secContext from list */
		list_del(&(handle->listEntry));
		/* 3. release secure path resource in normal world */
		kfree(handle);

	} while (0);
	return status;
#else
	CMDQ_ERR("secure path not support\n");
	return 0;
#endif
}

int32_t cmdq_sec_release_context_handle(uint32_t tgid)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;
	cmdqSecContextHandle handle = NULL;

	mutex_lock(&gCmdqSecContextLock);
	smp_mb(); /*memory barrier */

	handle = cmdq_sec_find_context_handle_unlocked(tgid);
	if (handle) {
		CMDQ_MSG("SecCtxHandle_RELEASE: +tgid[%d], handle[0x%p]\n",
			 tgid, handle);
		status = cmdq_sec_release_context_handle_unlocked(handle);
		CMDQ_MSG("SecCtxHandle_RELEASE: -tgid[%d], status[%d]\n", tgid,
			 status);
	} else {
		status = -1;
		CMDQ_ERR(
			"SecCtxHandle_RELEASE: err[secCtxHandle not exist], tgid[%d]\n",
			tgid);
	}

	mutex_unlock(&gCmdqSecContextLock);
	return status;
#else
	CMDQ_ERR("secure path not support\n");
	return 0;
#endif
}

cmdqSecContextHandle cmdq_sec_context_handle_create(uint32_t tgid)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	cmdqSecContextHandle handle = NULL;

	handle = kmalloc(sizeof(struct cmdqSecContextStruct),
			 GFP_ATOMIC);
	if (handle) {
		handle->iwcMessage = NULL;
		handle->tgid = tgid;
		handle->referCount = 0;
	} else {
		CMDQ_ERR("SecCtxHandle_CREATE: err[LOW_MEM], tgid[%d]\n", tgid);
	}

	CMDQ_MSG("SecCtxHandle_CREATE: create new, Handle[0x%p], tgid[%d]\n",
		 handle, tgid);
	return handle;
#else
	CMDQ_ERR("secure path not support\n");
	return NULL;
#endif
}

cmdqSecContextHandle cmdq_sec_acquire_context_handle(uint32_t tgid)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	cmdqSecContextHandle handle = NULL;

	mutex_lock(&gCmdqSecContextLock);
	smp_mb(); /*memory barrier */
	do {
		/* find sec context of a process */
		handle = cmdq_sec_find_context_handle_unlocked(tgid);
		/* if it dose not exist, create new one */
		if (handle == NULL) {
			handle = cmdq_sec_context_handle_create(tgid);
			if (handle)
				list_add_tail(&(handle->listEntry),
					      &gCmdqSecContextList);
		}
	} while (0);

	/* increase caller referCount */
	if (handle)
		handle->referCount++;

	if (handle) {
		CMDQ_MSG(
			"[CMDQ]SecCtxHandle_ACQUIRE, Handle[0x%p], tgid[%d], refCount[%d]\n",
			handle, tgid, handle->referCount);
	}
	mutex_unlock(&gCmdqSecContextLock);

	return handle;
#else
	CMDQ_ERR("secure path not support\n");
	return NULL;
#endif
}

void cmdq_sec_dump_context_list(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	struct cmdqSecContextStruct *secContextEntry = NULL;
	struct list_head *pos = NULL;

	CMDQ_ERR("=============== [CMDQ] sec context ===============\n");

	list_for_each(pos, &gCmdqSecContextList)
	{
		secContextEntry =
			list_entry(pos, struct cmdqSecContextStruct, listEntry);
		CMDQ_ERR(
			"secCtxHandle[0x%p], tgid_%d[referCount: %d], state[%d], iwc[0x%p]\n",
			secContextEntry, secContextEntry->tgid,
			secContextEntry->referCount, secContextEntry->state,
			secContextEntry->iwcMessage);
	}
#else
	CMDQ_ERR("secure path not support\n");
#endif
}

void cmdqSecDeInitialize(void)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	/* .TEE. close SVP CMDQ TEE serivice session */
	/* the sessions are created and accessed using [cmdq_session_handle()] /
	 */
	/* [cmdq_mem_session_handle()] API, and closed here. */

	struct cmdqSecContextStruct *secContextEntry = NULL;
	struct list_head *pos = NULL;
	int ret;

	ret = KREE_UnregisterSharedmem(gCmdqSecContextHandle->memSessionHandle,
				       gCmdq_share_cookie_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		CMDQ_ERR("deinit unregister share memory failed ret=%d\n", ret);
		return;
	}

	ret = KREE_CloseSession(cmdq_session);
	if (ret != TZ_RESULT_SUCCESS) {
		CMDQ_ERR("DDP close ddp_session fail ret=%d\n", ret);
		return;
	}

	ret = KREE_CloseSession(cmdq_mem_session);
	if (ret != TZ_RESULT_SUCCESS) {
		CMDQ_ERR("DDP close ddp_mem_session fail ret=%d\n", ret);
		return;
	}

	/* release shared memory */
	cmdq_sec_destroy_shared_memory(
		cmdq_core_get_cmdqcontext()->hSecSharedMem);

#if CMDQ_OPEN_SESSION_ONCE
	cmdq_sec_deinit_session_unlocked(gCmdqSecContextHandle);
#endif
	if (gCmdqSecContextHandle != NULL) {
		kfree(gCmdqSecContextHandle);
		gCmdqSecContextHandle = NULL;
	}

	/*release cmdqSecContextHandle */
	mutex_lock(&gCmdqSecContextLock);
	smp_mb(); /*memory barrier */

	list_for_each(pos, &gCmdqSecContextList)
	{
		secContextEntry =
			list_entry(pos, struct cmdqSecContextStruct, listEntry);
		if (secContextEntry != NULL) {
			secContextEntry->referCount = 0;
			cmdq_sec_release_context_handle_unlocked(
				secContextEntry);
		}
	}

	mutex_unlock(&gCmdqSecContextLock);
#else
	CMDQ_ERR("secure path not support\n");
#endif
}

int32_t cmdqSecRegisterSecureBuffer(struct transmitBufferStruct *pSecureData)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;
	struct KREE_SHAREDMEM_PARAM cmdq_shared_param;
	int tzRes = TZ_RESULT_SUCCESS;

	do {
		if (pSecureData == NULL) {
			status = -1;
			break;
		}

		cmdq_shared_param.buffer = pSecureData->pBuffer;
		cmdq_shared_param.size = pSecureData->size;

		if (pSecureData->memSessionHandle == 0) {
			pSecureData->memSessionHandle =
				cmdq_mem_session_handle();
			if (pSecureData->memSessionHandle == 0) {
				status = -2;
				break;
			}
		}

		tzRes = KREE_RegisterSharedmem(pSecureData->memSessionHandle,
					       &(pSecureData->shareMemHandle),
					       &cmdq_shared_param);

		if (tzRes != TZ_RESULT_SUCCESS) {
			status = -3;
			break;
		}
	} while (0);

	if (status != 0)
		CMDQ_ERR("cmdqSecRegisterSecureBuffer failed, status[%d]\n",
			 status);

	return status;
#else
	CMDQ_ERR("secure path not support\n");
	return 0;
#endif
}

int32_t cmdqSecServiceCall(struct transmitBufferStruct *pSecureData,
			   int32_t cmd)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	union MTEEC_PARAM cmdq_param[4];
	unsigned int paramTypes = TZ_ParamTypes1(TZPT_MEMREF_INPUT);
	int tzRes = TZ_RESULT_SUCCESS;

	if (pSecureData->cmdqHandle == 0) {
		pSecureData->cmdqHandle = cmdq_session_handle();
		if (pSecureData->cmdqHandle == 0) {
			CMDQ_ERR("get cmdq handle failed\n");
			return -1;
		}
	}

	cmdq_param[0].memref.handle = (uint32_t)pSecureData->shareMemHandle;
	cmdq_param[0].memref.offset = 0;
	cmdq_param[0].memref.size = pSecureData->size;

	tzRes = KREE_TeeServiceCall(pSecureData->cmdqHandle, cmd, paramTypes,
				    cmdq_param);
	if (tzRes != TZ_RESULT_SUCCESS) {
		CMDQ_ERR(
			"leave secure world cmdqSecServiceCall fail, ret=0x%x\n",
			tzRes);
		return -2;
	}
	return 0;
#else
	CMDQ_ERR("secure path not support\n");
	return 0;
#endif
}

int32_t cmdqSecUnRegisterSecureBuffer(struct transmitBufferStruct *pSecureData)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int ret = TZ_RESULT_SUCCESS;

	ret = KREE_UnregisterSharedmem(pSecureData->memSessionHandle,
				       pSecureData->shareMemHandle);

	if (ret != TZ_RESULT_SUCCESS) {
		CMDQ_ERR("deinit unregister share memory failed ret=%d\n", ret);
		return -1;
	}
	return 0;
#else
	CMDQ_ERR("secure path not support\n");
	return 0;
#endif
}

void cmdq_sec_register_secure_irq(void)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	/*
	 *	pass SECURE IRQ ID to trustzone
	 *	1	prepare buffer to transfer to secure world
	 *	2	register secure buffer
	 *	3	service call
	 *	4	unregister secure buffer
	 */

	/* 1	prepare buffer to transfer to secure world */
	uint32_t secureIrqID = cmdq_dev_get_irq_secure_id();
	struct transmitBufferStruct secureData;

	memset(&secureData, 0, sizeof(secureData));
	secureData.pBuffer = &secureIrqID;
	secureData.size = sizeof(secureIrqID);

	/* 2	register secure buffer */
	if (cmdqSecRegisterSecureBuffer(&secureData) != 0)
		return;
	CMDQ_LOG("register secure irq normal\n");

	/* 3	service call */
	cmdqSecServiceCall(&secureData, CMD_CMDQ_TL_REGISTER_SECURE_IRQ);

	/* 4	unregister secure buffer */
	cmdqSecUnRegisterSecureBuffer(&secureData);
#else
	CMDQ_ERR("secure path not support\n");
#endif
}

void cmdqSecInitialize(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	INIT_LIST_HEAD(&gCmdqSecContextList);
/* cmdq_sec_allocate_path_resource_unlocked(); */
#if 0
/*
 * no need to pass virtual irq id to secure world.
 * MTEE need hardware irq id instead of virtual irq id
 */
	/*	register secure IRQ handle */
	cmdq_sec_lock_secure_path();
	cmdq_sec_register_secure_irq();
	cmdq_sec_unlock_secure_path();
#endif
#if 0
	/* allocate shared memory */
	gCmdqContext.hSecSharedMem = NULL;
	cmdq_sec_create_shared_memory(&(gCmdqContext.hSecSharedMem), PAGE_SIZE);
	/* init share memory */
	cmdq_sec_lock_secure_path();
	cmdq_sec_init_share_memory();
	cmdq_sec_unlock_secure_path();
#endif

#endif
}

int cmdq_sec_init_secure_path(void *data)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int status = 0;

	CMDQ_LOG("begin to init secure path\n");
	/* allocate shared memory */
	cmdq_core_get_cmdqcontext()->hSecSharedMem = NULL;
	status = cmdq_sec_create_shared_memory(
		&(cmdq_core_get_cmdqcontext()->hSecSharedMem), PAGE_SIZE);
	/* init share memory */
	cmdq_sec_lock_secure_path();
	status &= cmdq_sec_init_share_memory();
	cmdq_sec_unlock_secure_path();
	CMDQ_LOG("init secure path done\n");
	return status;
#else
	CMDQ_ERR("secure path not support\n");
	return 0;
#endif
}

void cmdqSecEnableProfile(const bool enable)
{
	CMDQ_LOG("cmdqSecEnableProfile undefined!\n");
}

/* debug */
/*  */
void cmdq_sec_set_commandId(uint32_t cmdId)
{
	atomic_set(&gDebugSecCmdId, cmdId);
}

const uint32_t cmdq_sec_get_commandId(void)
{
	return (uint32_t)(atomic_read(&gDebugSecCmdId));
}

void cmdq_debug_set_sw_copy(int32_t value)
{
	atomic_set(&gDebugSecSwCopy, value);
}

int32_t cmdq_debug_get_sw_copy(void)
{
	return atomic_read(&gDebugSecSwCopy);
}

void cmdq_sec_set_sec_print_count(uint32_t count, bool bPrint)
{
	gSecPrintCount = count;
	if (bPrint)
		CMDQ_LOG("set sec count to %d\n", count);
}

uint32_t cmdq_sec_get_sec_print_count(void)
{
	return gSecPrintCount;
}

int32_t cmdq_sec_get_log_level(void)
{
	int32_t loglevel = cmdq_core_get_log_level();

	if (loglevel == CMDQ_LOG_LEVEL_NORMAL)
		loglevel = LOG_LEVEL_LOG;
	else if (cmdq_core_should_print_msg())
		loglevel = LOG_LEVEL_MSG;
	else if (cmdq_core_should_full_error())
		loglevel = LOG_LEVEL_LOG;
	else if (loglevel & (1 << 3))
		loglevel = LOG_LEVEL_MSG;

	return loglevel;
}

#endif
