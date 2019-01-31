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

#include <linux/slab.h>
#include <linux/delay.h>

#include "cmdq_sec.h"
#include "cmdq_def.h"
#include "cmdq_virtual.h"
#include "cmdq_device.h"
#include "cmdq_reg.h"
#ifdef CMDQ_MET_READY
#include <linux/met_drv.h>
#endif

/* lock to protect atomic secure task execution */
static DEFINE_MUTEX(gCmdqSecExecLock);
/* ensure atomic enable/disable secure path profile */
static DEFINE_MUTEX(gCmdqSecProfileLock);
/* secure context list. note each porcess has its own sec context */
static struct list_head gCmdqSecContextList;

#if defined(CMDQ_SECURE_PATH_SUPPORT)

/* sectrace interface */
#ifdef CMDQ_SECTRACE_SUPPORT
#include <linux/sectrace.h>
#endif
/* secure path header */
#include "cmdqsectl_api.h"

#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
#include "isee_kernel_api.h"
#endif

/* secure context to cmdqSecTL */
static struct cmdqSecContextStruct *gCmdqSecContextHandle;

/* internal control */

/* Set 1 to open once for each process context, because of below reasons:
 * 1. mc_open_session is too slow (major)
 * 2. we entry secure world for config and trigger GCE, and wait in normal world
 */
#define CMDQ_OPEN_SESSION_ONCE (1)


/* operator API */

int32_t cmdq_sec_init_session_unlocked(struct cmdqSecContextStruct *handle)
{
	int32_t openRet = 0;
	int32_t status = 0;

	CMDQ_MSG("[SEC]-->SESSION_INIT: iwcState[%d]\n", (handle->state));

	do {
#if CMDQ_OPEN_SESSION_ONCE
		if (handle->state >= IWC_SES_OPENED) {
			CMDQ_MSG("SESSION_INIT: already opened\n");
			break;
		}

		CMDQ_MSG("[SEC]SESSION_INIT: open new session[%d]\n",
			handle->state);
#endif
		CMDQ_VERBOSE(
			"[SEC]SESSION_INIT: sessionHandle:0x%p\n",
			&handle->tee.session);

		CMDQ_PROF_START(current->pid, "CMDQ_SEC_INIT");

		if (handle->state < IWC_CONTEXT_INITED) {
			/* open mobicore device */
			openRet = cmdq_sec_init_context(&handle->tee);
			if (openRet < 0) {
				status = -1;
				break;
			}
			handle->state = IWC_CONTEXT_INITED;
		}

		if (handle->state < IWC_WSM_ALLOCATED) {
			if (handle->iwcMessage) {
				CMDQ_ERR(
					"[SEC]SESSION_INIT: wsm message is not NULL\n");
				status = -EINVAL;
				break;
			}

			/* allocate world shared memory */
			status = cmdq_sec_allocate_wsm(&handle->tee,
				&handle->iwcMessage,
				sizeof(struct iwcCmdqMessage_t));
			if (status < 0)
				break;
			handle->state = IWC_WSM_ALLOCATED;
		}

		/* open a secure session */
		status = cmdq_sec_open_session(&handle->tee,
			handle->iwcMessage);
		if (status < 0)
			break;
		handle->state = IWC_SES_OPENED;

		CMDQ_PROF_END(current->pid, "CMDQ_SEC_INIT");
	} while (0);

	CMDQ_MSG("[SEC]<--SESSION_INIT[%d]\n", status);
	return status;
}

void cmdq_sec_deinit_session_unlocked(struct cmdqSecContextStruct *handle)
{
	CMDQ_MSG("[SEC]-->SESSION_DEINIT\n");
	do {
		switch (handle->state) {
		case IWC_SES_ON_TRANSACTED:
		case IWC_SES_TRANSACTED:
		case IWC_SES_MSG_PACKAGED:
			/* continue next clean-up */
		case IWC_SES_OPENED:
			cmdq_sec_close_session(&handle->tee);
			/* continue next clean-up */
		case IWC_WSM_ALLOCATED:
			cmdq_sec_free_wsm(&handle->tee, &handle->iwcMessage);
			/* continue next clean-up */
		case IWC_CONTEXT_INITED:
			cmdq_sec_deinit_context(&handle->tee);
			/* continue next clean-up */
			break;
		case IWC_INIT:
			/* CMDQ_ERR("open secure driver failed\n"); */
			break;
		default:
			break;
		}

	} while (0);

	CMDQ_MSG("[SEC]<--SESSION_DEINIT\n");
}

int32_t cmdq_sec_fill_iwc_command_basic_unlocked(
	int32_t iwcCommand, void *_pTask, int32_t thread, void *_pIwc)
{
	struct iwcCmdqMessage_t *pIwc;

	pIwc = (struct iwcCmdqMessage_t *) _pIwc;

	/* specify command id only, don't care other other */
	memset(pIwc, 0x0, sizeof(struct iwcCmdqMessage_t));
	pIwc->cmd = iwcCommand;

	/* medatada: debug config */
	pIwc->debug.logLevel = 1;
	pIwc->debug.enableProfile = cmdq_core_profile_enabled();

	return 0;
}

#define CMDQ_ENGINE_TRANS(eng_flags, eng_flags_sec, ENGINE) \
	do {	\
		if ((1LL << CMDQ_ENG_##ENGINE) & (eng_flags)) \
		(eng_flags_sec) |= (1LL << CMDQ_SEC_##ENGINE); \
	} while (0)

static u64 cmdq_sec_get_secure_engine(u64 engine_flags)
{
	u64 engine_flags_sec = 0;

	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, MDP_RDMA0);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, MDP_RDMA1);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, MDP_WDMA);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, MDP_WROT0);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, MDP_WROT1);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, DISP_RDMA0);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, DISP_RDMA1);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, DISP_WDMA0);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, DISP_WDMA1);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, DISP_OVL0);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, DISP_OVL1);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, DISP_OVL2);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, DISP_2L_OVL0);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, DISP_2L_OVL1);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, DISP_2L_OVL2);

	return engine_flags_sec;
}

static void cmdq_sec_dump_v3_replace_inst(struct TaskStruct *task,
	u32 *buffer, u32 size)
{
	u32 *p_instr_position = CMDQ_U32_PTR(task->replace_instr.position);
	u32 i;

	if (!task->replace_instr.number)
		return;

	CMDQ_ERR("replace number:%u\n", task->replace_instr.number);
	for (i = 0; i < task->replace_instr.number; i++) {
		u32 *cmd;
		u32 offset = p_instr_position[i] * CMDQ_INST_SIZE;

		if (offset >= size)
			break;
		cmd = (u32 *)((u8 *)buffer + offset);

		CMDQ_ERR("idx:%u,%u cmd:0x%08x:%08x\n",
			i, p_instr_position[i], cmd[0], cmd[1]);
	}
}

s32 cmdq_sec_fill_iwc_command_msg_unlocked(
	s32 iwc_cmd, void *task_ptr, s32 thread, void *iwc_ptr)
{
	struct TaskStruct *task = (struct TaskStruct *)task_ptr;
	struct iwcCmdqMessage_t *iwc = (struct iwcCmdqMessage_t *)iwc_ptr;
	/* cmdqSecDr will insert some instr */
	const u32 reserve_cmd_size = 4 * CMDQ_INST_SIZE;
	s32 status = 0;

	/* check task first */
	if (!task) {
		CMDQ_ERR(
			"[SEC]SESSION_MSG: Unable to fill message by empty task.\n");
		return -EFAULT;
	}

	/* check command size first */
	if ((task->commandSize + reserve_cmd_size) > CMDQ_TZ_CMD_BLOCK_SIZE) {
		CMDQ_ERR("[SEC]SESSION_MSG: task %p size %d > %d\n",
			task, task->commandSize, CMDQ_TZ_CMD_BLOCK_SIZE);
		return -EFAULT;
	}

	CMDQ_MSG("[SEC]-->SESSION_MSG: command id:%d size:%u task:0x%p\n",
		iwc_cmd, task->commandSize, task);

	/* fill message buffer for inter world communication */
	memset(iwc, 0x0, sizeof(*iwc));
	iwc->cmd = iwc_cmd;

	/* metadata */
	iwc->command.metadata.enginesNeedDAPC = cmdq_sec_get_secure_engine(
		task->secData.enginesNeedDAPC);
	iwc->command.metadata.enginesNeedPortSecurity =
		cmdq_sec_get_secure_engine(
		task->secData.enginesNeedPortSecurity);

	if (thread != CMDQ_INVALID_THREAD) {
		/* basic data */
		iwc->command.scenario = task->scenario;
		iwc->command.thread = thread;
		iwc->command.priority = task->priority;
		iwc->command.engineFlag = cmdq_sec_get_secure_engine(
			task->engineFlag);
		iwc->command.hNormalTask = 0LL | ((unsigned long)task);

		if (task->cmd_buffer_va && !task->is_client_buffer) {
			memcpy(iwc->command.pVABase, task->cmd_buffer_va,
				task->commandSize);
			iwc->command.commandSize = task->commandSize;
		} else {
			iwc->command.commandSize = 0;
			task->cmd_buffer_va = NULL;
			task->pCMDEnd = iwc->command.pVABase - 1;
			status = cmdq_sec_task_copy_to_buffer(task,
				task->desc);
			if (status < 0) {
				CMDQ_ERR("[SEC]fail to copy buffer\n");
				return status;
			}
			iwc->command.commandSize = task->commandSize;
		}

		/* cookie */
		iwc->command.waitCookie = task->secData.waitCookie;
		iwc->command.resetExecCnt = task->secData.resetExecCnt;

		CMDQ_MSG(
			"[SEC]SESSION_MSG: task:0x%p thread:%d size:%d flag:0x%016llx size:%zu\n",
			task, thread, task->commandSize, task->engineFlag,
			sizeof(iwc->command));

		CMDQ_VERBOSE("[SEC]SESSION_MSG: addrList[%d][0x%p]\n",
			task->secData.addrMetadataCount,
			CMDQ_U32_PTR(task->secData.addrMetadatas));
		if (task->secData.addrMetadataCount > 0) {
			iwc->command.metadata.addrListLength =
				task->secData.addrMetadataCount;
			memcpy((iwc->command.metadata.addrList),
				CMDQ_U32_PTR(task->secData.addrMetadatas),
				task->secData.addrMetadataCount *
				sizeof(struct cmdqSecAddrMetadataStruct));
		}

		/* medatada: debug config */
		iwc->debug.logLevel = (cmdq_core_should_print_msg()) ?
			(1) : (0);
		iwc->debug.enableProfile = cmdq_core_profile_enabled();
	} else {
		/* relase resource, or debug function will go here */
		CMDQ_VERBOSE("[SEC]-->SESSION_MSG: no task cmdId:%d\n",
			iwc_cmd);
		iwc->command.commandSize = 0;
		iwc->command.metadata.addrListLength = 0;
	}

	CMDQ_MSG("[SEC]<--SESSION_MSG status:%d\n", status);
	return status;
}

/* TODO: when do release secure command buffer */
int32_t cmdq_sec_fill_iwc_resource_msg_unlocked(
	int32_t iwcCommand, void *_pTask, int32_t thread, void *_pIwc)
{
	struct iwcCmdqMessage_t *pIwc;
	struct cmdqSecSharedMemoryStruct *pSharedMem;

	pSharedMem = cmdq_core_get_secure_shared_memory();
	if (pSharedMem == NULL) {
		CMDQ_ERR("FILL:RES, NULL shared memory\n");
		return -EFAULT;
	}

	if (pSharedMem && pSharedMem->pVABase == NULL) {
		CMDQ_ERR("FILL:RES, %p shared memory has not init\n",
			pSharedMem);
		return -EFAULT;
	}

	pIwc = (struct iwcCmdqMessage_t *) _pIwc;
	memset(pIwc, 0x0, sizeof(struct iwcCmdqMessage_t));

	pIwc->cmd = iwcCommand;
	pIwc->pathResource.shareMemoyPA = 0LL | (pSharedMem->MVABase);
	pIwc->pathResource.size = pSharedMem->size;
	pIwc->pathResource.useNormalIRQ = 1;

	CMDQ_MSG("FILL:RES, shared memory:%pa(0x%llx), size:%d\n",
		&(pSharedMem->MVABase), pIwc->pathResource.shareMemoyPA,
		pSharedMem->size);

	/* medatada: debug config */
	pIwc->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) : (0);
	pIwc->debug.enableProfile = cmdq_core_profile_enabled();

	return 0;
}

int32_t cmdq_sec_fill_iwc_cancel_msg_unlocked(
	int32_t iwcCommand, void *_pTask, int32_t thread, void *_pIwc)
{
	const struct TaskStruct *pTask = (struct TaskStruct *) _pTask;
	struct iwcCmdqMessage_t *pIwc;

	pIwc = (struct iwcCmdqMessage_t *) _pIwc;
	memset(pIwc, 0x0, sizeof(struct iwcCmdqMessage_t));

	pIwc->cmd = iwcCommand;
	pIwc->cancelTask.waitCookie = pTask->secData.waitCookie;
	pIwc->cancelTask.thread = thread;

	/* medatada: debug config */
	pIwc->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) : (0);
	pIwc->debug.enableProfile = cmdq_core_profile_enabled();

	CMDQ_LOG("FILL:CANCEL_TASK: task: %p, thread:%d, cookie:%d\n",
		 pTask, thread, pTask->secData.waitCookie);

	return 0;
}

/******************************************************************************
 * context handle
 *****************************************************************************/
s32 cmdq_sec_setup_context_session(struct cmdqSecContextStruct *handle)
{
	s32 status;

	/* init iwc parameter */
	if (handle->state == IWC_INIT)
		cmdq_sec_setup_tee_context(&handle->tee);

	/* init secure session */
	status = cmdq_sec_init_session_unlocked(handle);
	CMDQ_MSG("[SEC]setup_context: status:%d tgid:%d\n", status,
		handle->tgid);
	return status;
}

int32_t cmdq_sec_teardown_context_session(
	struct cmdqSecContextStruct *handle)
{
	int32_t status = 0;

	if (handle) {
		CMDQ_MSG("[SEC]SEC_TEARDOWN: state: %d, iwcMessage:0x%p\n",
			 handle->state, handle->iwcMessage);

		cmdq_sec_deinit_session_unlocked(handle);

		/* clrean up handle's attritubes */
		handle->state = IWC_INIT;

	} else {
		CMDQ_ERR("[SEC]SEC_TEARDOWN: null secCtxHandle\n");
		status = -1;
	}
	return status;
}

void cmdq_sec_handle_attach_status(struct TaskStruct *pTask,
	uint32_t iwcCommand, const struct iwcCmdqMessage_t *pIwc,
	int32_t sec_status_code, char **dispatch_mod_ptr)
{
	int index = 0;
	const struct iwcCmdqSecStatus_t *secStatus = NULL;

	if (!pIwc || !dispatch_mod_ptr)
		return;

	/* assign status ptr to print without task */
	secStatus = &pIwc->secStatus;

	if (pTask) {
		if (pTask->secStatus) {
			if (iwcCommand == CMD_CMDQ_TL_CANCEL_TASK) {
				const struct iwcCmdqSecStatus_t *
					last_sec_status = pTask->secStatus;

				/* cancel task uses same errored task thus
				 * secStatus may exist
				 */
				CMDQ_ERR(
					"Last secure status: %d step: 0x%08x args: 0x%08x 0x%08x 0x%08x 0x%08x dispatch: %s task: 0x%p\n",
					last_sec_status->status,
					last_sec_status->step,
					last_sec_status->args[0],
					last_sec_status->args[1],
					last_sec_status->args[2],
					last_sec_status->args[3],
					last_sec_status->dispatch, pTask);
			} else {
				/* task should not send to secure twice,
				 * aee it
				 */
				CMDQ_AEE("CMDQ",
					"Last secure status still exists, task: 0x%p command: %u\n",
					pTask, iwcCommand);
			}
			kfree(pTask->secStatus);
			pTask->secStatus = NULL;
		}

		pTask->secStatus = kzalloc(sizeof(struct iwcCmdqSecStatus_t),
			GFP_KERNEL);
		if (pTask->secStatus) {
			memcpy(pTask->secStatus, &pIwc->secStatus,
				sizeof(struct iwcCmdqSecStatus_t));
			secStatus = pTask->secStatus;
		}
	}

	if (secStatus->status != 0 || sec_status_code < 0) {
		/* secure status may contains debug information */
		CMDQ_ERR(
			"Secure status: %d (%d) step: 0x%08x args: 0x%08x 0x%08x 0x%08x 0x%08x dispatch: %s task: 0x%p\n",
			secStatus->status, sec_status_code, secStatus->step,
			secStatus->args[0], secStatus->args[1],
			secStatus->args[2], secStatus->args[3],
			secStatus->dispatch, pTask);
		for (index = 0; index < secStatus->inst_index; index += 2) {
			CMDQ_ERR("Secure instruction %d: 0x%08x:%08x\n",
				(index / 2), secStatus->sec_inst[index],
				secStatus->sec_inst[index+1]);
		}

		switch (secStatus->status) {
		case -CMDQ_ERR_ADDR_CONVERT_HANDLE_2_PA:
			*dispatch_mod_ptr = "TEE";
			break;
		case -CMDQ_ERR_ADDR_CONVERT_ALLOC_MVA:
		case -CMDQ_ERR_ADDR_CONVERT_ALLOC_MVA_N2S:
			switch (pIwc->command.thread) {
			case CMDQ_THREAD_SEC_PRIMARY_DISP:
			case CMDQ_THREAD_SEC_SUB_DISP:
				*dispatch_mod_ptr = "DISP";
				break;
			case CMDQ_THREAD_SEC_MDP:
				*dispatch_mod_ptr = "MDP";
				break;
			}
			break;
		}
	}
}

int32_t cmdq_sec_handle_session_reply_unlocked(
	const struct iwcCmdqMessage_t *pIwc, const int32_t iwcCommand,
	struct TaskStruct *pTask, void *data)
{
	int32_t status;
	int32_t iwcRsp;
	struct cmdqSecCancelTaskResultStruct *pCancelResult = NULL;

	/* get secure task execution result */
	iwcRsp = (pIwc)->rsp;
	status = iwcRsp;

	if (iwcCommand == CMD_CMDQ_TL_CANCEL_TASK) {
		pCancelResult = (struct cmdqSecCancelTaskResultStruct *) data;
		if (pCancelResult) {
			pCancelResult->throwAEE = pIwc->cancelTask.throwAEE;
			pCancelResult->hasReset = pIwc->cancelTask.hasReset;
			pCancelResult->irqFlag = pIwc->cancelTask.irqFlag;
			pCancelResult->errInstr[0] =
				pIwc->cancelTask.errInstr[0];	/* arg_b */
			pCancelResult->errInstr[1] =
				pIwc->cancelTask.errInstr[1];	/* arg_a */
			pCancelResult->regValue = pIwc->cancelTask.regValue;
			pCancelResult->pc = pIwc->cancelTask.pc;
		}

		/* for WFE, we specifically dump the event value */
		if (((pIwc->cancelTask.errInstr[1] & 0xFF000000) >> 24) ==
			CMDQ_CODE_WFE) {
			const uint32_t eventID = 0x3FF &
				pIwc->cancelTask.errInstr[1];

			CMDQ_ERR(
				"=============== [CMDQ] Error WFE Instruction Status ===============\n");
			CMDQ_ERR("CMDQ_SYNC_TOKEN_VAL of %s is %d\n",
				cmdq_core_get_event_name(eventID),
				pIwc->cancelTask.regValue);
		}

		CMDQ_ERR(
			"CANCEL_TASK: pTask %p, INST:(0x%08x, 0x%08x), throwAEE:%d, hasReset:%d, pc:0x%08x\n",
			pTask, pIwc->cancelTask.errInstr[1],
			pIwc->cancelTask.errInstr[0],
			pIwc->cancelTask.throwAEE, pIwc->cancelTask.hasReset,
			pIwc->cancelTask.pc);
	} else if (iwcCommand ==
		CMD_CMDQ_TL_PATH_RES_ALLOCATE || iwcCommand ==
		CMD_CMDQ_TL_PATH_RES_RELEASE) {
		/* do nothing */
	} else {
		/* note we etnry SWd to config GCE, and wait execution result
		 * in NWd update taskState only if config failed
		 */
		if (pTask && iwcRsp < 0)
			pTask->taskState = TASK_STATE_ERROR;
	}

	/* log print */
	if (status < 0) {
		CMDQ_ERR("SEC_SEND: status[%d], cmdId[%d], iwcRsp[%d]\n",
			 status, iwcCommand, iwcRsp);
	} else {
		CMDQ_MSG("SEC_SEND: status[%d], cmdId[%d], iwcRsp[%d]\n",
			 status, iwcCommand, iwcRsp);
	}

	return status;
}


CmdqSecFillIwcCB cmdq_sec_get_iwc_msg_fill_cb_by_iwc_command(
	uint32_t iwcCommand)
{
	CmdqSecFillIwcCB cb = NULL;

	switch (iwcCommand) {
	case CMD_CMDQ_TL_CANCEL_TASK:
		cb = cmdq_sec_fill_iwc_cancel_msg_unlocked;
		break;
	case CMD_CMDQ_TL_PATH_RES_ALLOCATE:
	case CMD_CMDQ_TL_PATH_RES_RELEASE:
		cb = cmdq_sec_fill_iwc_resource_msg_unlocked;
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
}

static s32 cmdq_sec_send_context_session_message(
	struct cmdqSecContextStruct *handle, u32 iwc_command,
	struct TaskStruct *task, s32 thread, void *data)
{
	s32 status;
	const s32 timeout_ms = 3 * 1000;

	const CmdqSecFillIwcCB fill_iwc_cb =
		cmdq_sec_get_iwc_msg_fill_cb_by_iwc_command(iwc_command);

	do {
		/* fill message bufer */
		status = fill_iwc_cb(iwc_command, task, thread,
			(void *)(handle->iwcMessage));

		if (status < 0) {
			CMDQ_ERR(
				"[SEC]fill msg buffer failed:%d pid:%d:%d cmdId:%d\n",
				 status, current->tgid, current->pid,
				 iwc_command);
			break;
		}

		/* send message */
		status = cmdq_sec_execute_session(&handle->tee, iwc_command,
			timeout_ms);
		if (status != 0) {
			CMDQ_ERR(
				"[SEC]cmdq_sec_execute_session_unlocked failed[%d], pid[%d:%d]\n",
				 status, current->tgid, current->pid);
			break;
		}
		/* only update state in success */
		handle->state = IWC_SES_ON_TRANSACTED;

		status = cmdq_sec_handle_session_reply_unlocked(
			handle->iwcMessage, iwc_command, task, data);
	} while (0);

	return status;
}

void cmdq_sec_track_task_record(const uint32_t iwcCommand,
	struct TaskStruct *pTask, CMDQ_TIME *pEntrySec, CMDQ_TIME *pExitSec)
{
	if (!pTask)
		return;

	if (iwcCommand != CMD_CMDQ_TL_SUBMIT_TASK)
		return;

	/* record profile data
	 * tbase timer/time support is not enough currently,
	 * so we treats entry/exit timing to secure world as the trigger
	 * timing
	 */
	pTask->entrySec = *pEntrySec;
	pTask->exitSec = *pExitSec;
	pTask->trigger = *pEntrySec;
}

static void cmdq_core_dump_secure_metadata(struct cmdqSecDataStruct *pSecData)
{
	uint32_t i = 0;
	struct cmdqSecAddrMetadataStruct *pAddr = NULL;

	if (pSecData == NULL)
		return;

	pAddr = (struct cmdqSecAddrMetadataStruct *)(
		CMDQ_U32_PTR(pSecData->addrMetadatas));

	CMDQ_LOG("========= pSecData: %p dump =========\n", pSecData);
	CMDQ_LOG(
		"count:%d(%d), enginesNeedDAPC:0x%llx, enginesPortSecurity:0x%llx\n",
		pSecData->addrMetadataCount, pSecData->addrMetadataMaxCount,
		pSecData->enginesNeedDAPC, pSecData->enginesNeedPortSecurity);

	if (pAddr == NULL)
		return;

	for (i = 0; i < pSecData->addrMetadataCount; i++) {
		CMDQ_LOG(
			"idx:%d, type:%d, baseHandle:0x%016llx, blockOffset:%u, offset:%u, size:%u, port:%u\n",
			i, pAddr[i].type, (u64)pAddr[i].baseHandle,
			pAddr[i].blockOffset, pAddr[i].offset,
			pAddr[i].size, pAddr[i].port);
	}
}

static void cmdq_sec_dump_secure_task_status(void)
{
	const u32 task_va_offset = CMDQ_SEC_SHARED_TASK_VA_OFFSET;
	const u32 task_op_offset = CMDQ_SEC_SHARED_OP_OFFSET;
	u32 *pVA;
	s32 va_value_lo, va_value_hi, op_value;
	struct ContextStruct *context = cmdq_core_get_context();

	if (!context->hSecSharedMem) {
		CMDQ_ERR("%s shared memory is not created\n", __func__);
		return;
	}

	pVA = (u32 *)(context->hSecSharedMem->pVABase + task_va_offset);
	va_value_lo = *pVA;

	pVA = (u32 *)(context->hSecSharedMem->pVABase + task_va_offset +
		sizeof(u32));
	va_value_hi = *pVA;

	pVA = (u32 *)(context->hSecSharedMem->pVABase + task_op_offset);
	op_value = *pVA;

	CMDQ_ERR("[shared_op_status]task VA:0x%04x%04x op:%d\n",
		va_value_hi, va_value_lo, op_value);
}

int32_t cmdq_sec_submit_to_secure_world_async_unlocked(uint32_t iwcCommand,
	struct TaskStruct *pTask, int32_t thread, void *data, bool throwAEE)
{
	const int32_t tgid = current->tgid;
	const int32_t pid = current->pid;
	struct cmdqSecContextStruct *handle = NULL;
	int32_t status = 0;
	int32_t duration = 0;
	char long_msg[CMDQ_LONGSTRING_MAX];
	u32 msg_offset;
	s32 msg_max_size;
	char *dispatch_mod = "CMDQ";

	CMDQ_TIME tEntrySec;
	CMDQ_TIME tExitSec;

	CMDQ_MSG("[SEC]-->SEC_SUBMIT: tgid[%d:%d]\n", tgid, pid);
	do {
		/* find handle first */

		/* Unlike tBase user space API,
		 * tBase kernel API maintains a GLOBAL table to control
		 * mobicore device reference count.
		 * For kernel spece user, mc_open_device and session_handle
		 * don't depend on the process context.
		 * Therefore we use global secssion handle to inter-world
		 * commumication.
		 */
		if (gCmdqSecContextHandle == NULL)
			gCmdqSecContextHandle =
				cmdq_sec_context_handle_create(current->tgid);

		handle = gCmdqSecContextHandle;

		if (!handle) {
			CMDQ_ERR(
				"SEC_SUBMIT: tgid %d err[NULL secCtxHandle]\n",
				tgid);
			status = -(CMDQ_ERR_NULL_SEC_CTX_HANDLE);
			break;
		}

		if (cmdq_sec_setup_context_session(handle) < 0) {
			status = -(CMDQ_ERR_SEC_CTX_SETUP);
			break;
		}

		tEntrySec = sched_clock();

		status = cmdq_sec_send_context_session_message(handle,
			iwcCommand, pTask, thread, data);

		tExitSec = sched_clock();
		CMDQ_GET_TIME_IN_US_PART(tEntrySec, tExitSec, duration);
		cmdq_sec_track_task_record(iwcCommand, pTask, &tEntrySec,
			&tExitSec);

		cmdq_sec_handle_attach_status(pTask, iwcCommand,
			handle->iwcMessage, status, &dispatch_mod);

		/* release resource */
#if !(CMDQ_OPEN_SESSION_ONCE)
		cmdq_sec_teardown_context_session(handle)
#endif
		    /* Note we entry secure for config only and wait result in
		     * normal world.
		     * No need reset module HW for config failed case
		     */
	} while (0);

	if (status == -ETIMEDOUT) {
		/* t-base strange issue, mc_wait_notification false timeout
		 * when secure world has done
		 * because retry may failed, give up retry method
		 */
		cmdq_long_string_init(true, long_msg, &msg_offset,
		&msg_max_size);
		cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
			"[SEC]<--SEC_SUBMIT: err[%d][mc_wait_notification timeout], pTask[0x%p], THR[%d],",
			status, pTask, thread);
		cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
			" tgid[%d:%d], config_duration_ms[%d], cmdId[%d]\n",
			tgid, pid, duration, iwcCommand);

		if (throwAEE)
			CMDQ_AEE("TEE", "%s", long_msg);

		cmdq_core_turnon_first_dump(pTask);
		cmdq_sec_dump_secure_task_status();
		CMDQ_ERR("%s", long_msg);
	} else if (status < 0) {
		cmdq_long_string_init(true, long_msg, &msg_offset,
			&msg_max_size);
		cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
			"[SEC]<--SEC_SUBMIT: err[%d], pTask[0x%p], THR[%d], tgid[%d:%d],",
			status, pTask, thread, tgid, pid);
		cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
			" config_duration_ms[%d], cmdId[%d]\n",
			duration, iwcCommand);

		if (throwAEE)
			CMDQ_AEE(dispatch_mod, "%s", long_msg);

		cmdq_core_turnon_first_dump(pTask);
		CMDQ_ERR("%s", long_msg);

		/* dump metadata first */
		if (pTask)
			cmdq_core_dump_secure_metadata(&(pTask->secData));
	} else {
		cmdq_long_string_init(false, long_msg, &msg_offset,
			&msg_max_size);
		cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
			"[SEC]<--SEC_SUBMIT: err[%d], pTask[0x%p], THR[%d], tgid[%d:%d],",
			status, pTask, thread, tgid, pid);
		cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
			" config_duration_ms[%d], cmdId[%d]\n",
			duration, iwcCommand);
		CMDQ_MSG("%s", long_msg);
	}
	return status;
}

int32_t cmdq_sec_init_allocate_resource_thread(void *data)
{
	int32_t status = 0;

	cmdq_sec_lock_secure_path();

	gCmdqSecContextHandle = NULL;
	status = cmdq_sec_allocate_path_resource_unlocked(false);

	cmdq_sec_unlock_secure_path();

	return status;
}

static s32 cmdq_sec_prepare_command(struct TaskStruct *task,
	struct cmdqCommandStruct *pCommandDesc)
{
	/* use buffer directly to avoid copy */
	task->cmd_buffer_va = (u32 *)pCommandDesc->pVABase;
	task->is_client_buffer = true;
	task->pCMDEnd = NULL;
	return 0;
}

static void cmdq_sec_append_command(struct TaskStruct *task,
	u32 arg_a, u32 arg_b)
{
	task->pCMDEnd[1] = arg_b;
	task->pCMDEnd[2] = arg_a;
	task->pCMDEnd += 2;
	task->commandSize += CMDQ_INST_SIZE;
}

/*
 * Insert instruction to back secure threads' cookie count to normal world
 * Return:
 *     < 0, return the error code
 *     >=0, okay case, return number of bytes for inserting instruction
 */
static s32 cmdq_sec_insert_backup_cookie_instr(
	struct TaskStruct *task, s32 thread)
{
	const enum cmdq_gpr_reg valueRegId = CMDQ_DATA_REG_DEBUG;
	const enum cmdq_gpr_reg destRegId = CMDQ_DATA_REG_DEBUG_DST;
	const u32 regAddr = CMDQ_THR_EXEC_CNT_PA(thread);
	struct ContextStruct *context = cmdq_core_get_context();
	u64 addrCookieOffset = CMDQ_SEC_SHARED_THR_CNT_OFFSET + thread *
		sizeof(u32);
	u64 WSMCookieAddr = context->hSecSharedMem->MVABase + addrCookieOffset;
	const uint32_t subsysBit = cmdq_get_func()->getSubsysLSBArgA();
	int32_t subsysCode = cmdq_core_subsys_from_phys_addr(regAddr);
	u32 highAddr = 0;
	const enum cmdq_event regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;

	if (!context->hSecSharedMem) {
		CMDQ_ERR("%s shared memory is not created\n", __func__);
		return -EFAULT;
	}

	CMDQ_VERBOSE("backup secure cookie for thread:%d task:0x%p\n",
		thread, task);

	/* use SYNC TOKEN to make sure only 1 thread access at a time
	 * bit 0-11: wait_value
	 * bit 15: to_wait, true
	 * bit 31: to_update, true
	 * bit 16-27: update_value
	 * wait and clear
	 */
	cmdq_sec_append_command(task,
		(CMDQ_CODE_WFE << 24) | regAccessToken,
		(1 << 31) | (1 << 15) | 1);

	if (subsysCode != CMDQ_SPECIAL_SUBSYS_ADDR) {
		/* Load into 32-bit GPR (R0-R15) */
		cmdq_sec_append_command(task,
			(CMDQ_CODE_READ << 24) | (regAddr & 0xffff) |
			((subsysCode & 0x1f) << subsysBit) | (2 << 21),
			valueRegId);
	} else {
		/*
		 * for special sw subsys addr,
		 * we don't read directly due to append command will acquire
		 * CMDQ_SYNC_TOKEN_GPR_SET_4 event again.
		 */

		/* set GPR to address */
		cmdq_sec_append_command(task,
			(CMDQ_CODE_MOVE << 24) | ((valueRegId & 0x1f) << 16) |
			(4 << 21), regAddr);

		/* read data from address in GPR to GPR */
		cmdq_sec_append_command(task,
			(CMDQ_CODE_READ << 24) | ((valueRegId & 0x1f) << 16) |
			(6 << 21), valueRegId);
	}

	/* cookie +1 because we get before EOC */
	cmdq_sec_append_command(task,
		(CMDQ_CODE_LOGIC << 24) | (1 << 23) | (1 << 22) |
		(CMDQ_LOGIC_ADD << 16) | (valueRegId + CMDQ_GPR_V3_OFFSET),
		((valueRegId + CMDQ_GPR_V3_OFFSET) << 16) | 0x1);

	/* Note that <MOVE> arg_b is 48-bit */
	/* so writeAddress is split into 2 parts */
	/* and we store address in 64-bit GPR (P0-P7) */
	CMDQ_GET_HIGH_ADDR(WSMCookieAddr, highAddr);
	cmdq_sec_append_command(task,
		(CMDQ_CODE_MOVE << 24) | highAddr |
		((destRegId & 0x1f) << 16) | (4 << 21),
		(u32)WSMCookieAddr);

	/* write to memory */
	cmdq_sec_append_command(task,
		(CMDQ_CODE_WRITE << 24) |
		((destRegId & 0x1f) << 16) | (6 << 21),
		valueRegId);

	/* set directly */
	cmdq_sec_append_command(task,
		(CMDQ_CODE_WFE << 24) | regAccessToken,
		((1 << 31) | (1 << 16)));

	return 0;
}

s32 cmdq_sec_task_copy_to_buffer(struct TaskStruct *task,
	struct cmdqCommandStruct *desc)
{
	s32 status;
	u32 *copy_cmd_src, pre_copy_size;
	const bool user_space_req = cmdq_core_is_request_from_user_space(
		task->scenario);
	u32 *buffer_head;

	if (!desc) {
		CMDQ_AEE("CMDQ", "client desc should be provide task:0x%p\n",
			task);
		return -EINVAL;
	}

	/* copy commands to buffer, except last 2 instruction EOC+JUMP.
	 * end cmd will copy after post read
	 */
	copy_cmd_src = CMDQ_U32_PTR(desc->pVABase);
	pre_copy_size = desc->blockSize - 2 * CMDQ_INST_SIZE;

	buffer_head = task->pCMDEnd + 1;
	status = cmdq_core_copy_buffer_impl(buffer_head, copy_cmd_src,
		pre_copy_size, user_space_req);
	if (status < 0) {
		CMDQ_ERR(
			"copy command fail status:%d size:%u dest:0x%p task:0x%p user space:%s\n",
			status, pre_copy_size, buffer_head, task,
			user_space_req ? "true" : "false");
		dump_stack();
		return status;
	}
	task->commandSize += pre_copy_size;
	task->pCMDEnd = (u32 *)((u8 *)task->pCMDEnd + pre_copy_size);

	cmdq_sec_dump_v3_replace_inst(task, buffer_head, pre_copy_size);

	status = cmdq_sec_insert_backup_cookie_instr(task,
		task->exclusive_thread);
	if (status < 0) {
		CMDQ_ERR(
			"insert backup cookie fail task:%p status:%d size:%u thread:%d\n",
			task, status, task->commandSize,
			task->exclusive_thread);
		return status;
	}

	cmdq_core_append_backup_reg_inst(task, desc);

#ifdef CMDQ_APPEND_WITHOUT_SUSPEND
	if (likely(!task->loopCallback)) {
		cmdq_sec_append_command(task,
			(CMDQ_CODE_WFE << 24) |
			CMDQ_SYNC_TOKEN_APPEND_THR(task->exclusive_thread),
			(0 << 31) | (1 << 15) | 1);
	}
#endif

	/* copy END instructsions EOC+JUMP */
	status = cmdq_core_copy_buffer_impl(task->pCMDEnd + 1,
		(u32 *)((u8 *)copy_cmd_src + pre_copy_size),
		2 * CMDQ_INST_SIZE, user_space_req);
	if (status < 0) {
		CMDQ_ERR("copy end fail task:%p status:%d size:%u\n",
			task, status, task->commandSize);
		return status;
	}
	task->commandSize += 2 * CMDQ_INST_SIZE;
	task->pCMDEnd += 4;

	CMDQ_MSG("copy cmd result:%d size:%u buffer end:0x%p\n",
		status, task->commandSize, task->pCMDEnd);

	return status;
}

static s32 cmdq_sec_task_copy_command(struct cmdqCommandStruct *desc,
	struct TaskStruct *task)
{
	u32 addition_size = 7 * CMDQ_INST_SIZE;
	s32 status;

	/* allocate necessary buffer
	 * for backup cookie instructions: +7
	 * for append without suspend: +1
	 */
#ifdef CMDQ_APPEND_WITHOUT_SUSPEND
	addition_size += CMDQ_INST_SIZE;
#endif

	/* extra size for backup registers instruction */
	addition_size += cmdq_core_get_reg_extra_size(task, desc);

	task->cmd_buffer_va = kzalloc(desc->blockSize + addition_size,
		GFP_KERNEL);
	if (unlikely(!task->cmd_buffer_va)) {
		CMDQ_ERR("fail to allocate command buffer for task:0x%p\n",
			task);
		return -ENOMEM;
	}
	task->commandSize = 0;
	task->pCMDEnd = task->cmd_buffer_va - 1;

	status = cmdq_sec_task_copy_to_buffer(task, desc);

	if (unlikely(status < 0)) {
		CMDQ_ERR("fail to copy task instructions status:%d\n", status);
		kfree(task->cmd_buffer_va);
		task->cmd_buffer_va = NULL;
		task->pCMDEnd = NULL;
		task->commandSize = 0;
		task->bufferSize = 0;
	} else {
		/* in success case update size */
		task->bufferSize = task->commandSize;

		/* mark buffer as kenel allocate */
		task->is_client_buffer = false;
	}

	return status;
}

static s32 cmdq_sec_task_compose(struct cmdqCommandStruct *desc,
	struct TaskStruct *task)
{
	s32 status;

	CMDQ_MSG("compose secure task:0x%p\n", task);

	task->desc = desc;
	task->secStatus = NULL;
	task->secData.is_secure = true;
	task->secData.enginesNeedDAPC = desc->secData.enginesNeedDAPC;
	task->secData.enginesNeedPortSecurity =
	    desc->secData.enginesNeedPortSecurity;
	task->secData.addrMetadataCount = desc->secData.addrMetadataCount;
	if (task->secData.addrMetadataCount > 0) {
		u32 metadata_length;
		void *p_metadatas;

		metadata_length = (task->secData.addrMetadataCount) *
			sizeof(struct cmdqSecAddrMetadataStruct);
		/* create sec data task buffer for working */
		p_metadatas = kzalloc(metadata_length, GFP_KERNEL);
		if (p_metadatas) {
			memcpy(p_metadatas, CMDQ_U32_PTR(
				desc->secData.addrMetadatas), metadata_length);
			task->secData.addrMetadatas =
				(cmdqU32Ptr_t)(unsigned long)p_metadatas;
		} else {
			CMDQ_AEE("CMDQ",
				"Can't alloc secData buffer count:%d alloacted_size:%d\n",
				 task->secData.addrMetadataCount,
				 metadata_length);
			return -ENOMEM;
		}
	} else {
		task->secData.addrMetadatas = 0;
	}

	/* check thread valid */
	if (task->exclusive_thread == CMDQ_INVALID_THREAD) {
		task->exclusive_thread = cmdq_get_func()->getThreadID(
			task->scenario, true);
		CMDQ_ERR("secure should assign exclusive thread:%d\n",
			task->exclusive_thread);
	}

	/* use buffer from user space directly */
	status = cmdq_sec_prepare_command(task, desc);
	if (status < 0) {
		/* raise AEE first */
		CMDQ_AEE("CMDQ",
			"Can't alloc secure task and buffer error status:%d\n",
			status);
	}

	return status;
}

static s32 cmdq_sec_get_thread_id(s32 scenario)
{
	return cmdq_get_func()->getThreadID(scenario, true);
}

static s32 cmdq_sec_exec_task_prepare(struct TaskStruct *task, s32 thread)
{
	return 0;
}

static s32 cmdq_sec_exec_task_async_impl(struct TaskStruct *task,
	s32 thread_id)
{
	s32 status;
	struct ThreadStruct *thread;
	s32 cookie;
	char long_msg[CMDQ_LONGSTRING_MAX];
	u32 msg_offset;
	s32 msg_max_size;
	unsigned long flags;

	cmdq_long_string_init(false, long_msg, &msg_offset, &msg_max_size);
	cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
		   "-->EXEC: task:0x%p on thread:%d begin va:0x%p is client:%s",
		   task, thread_id, task->cmd_buffer_va,
		   task->is_client_buffer ? "true" : "false");
	cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
		   " command size:%d bufferSize:%d scenario:%d flag:0x%llx\n",
		   task->commandSize, task->bufferSize,
		   task->scenario, task->engineFlag);
	CMDQ_MSG("%s", long_msg);

	thread = &cmdq_core_get_context()->thread[thread_id];

	cmdq_sec_lock_secure_path();
	do {
		/* setup whole patah */
		status = cmdq_sec_allocate_path_resource_unlocked(true);
		if (status < 0)
			break;

		/* update task's thread info */
		task->thread = thread_id;
		task->irqFlag = 0;
		task->taskState = TASK_STATE_BUSY;

		cmdq_core_lock_exec_path(&flags);
		/* insert task to pThread's task lsit, and */
		/* delay HW config when entry SWd */
		if (thread->taskCount <= 0) {
			cookie = 1;
			cmdq_core_insert_task_from_thread_array_by_cookie(
				task, thread, cookie, true);
			task->secData.resetExecCnt = true;
		} else {
			/* append directly */
			cookie = thread->nextCookie;
			cmdq_core_insert_task_from_thread_array_by_cookie(
				task, thread, cookie, false);
			task->secData.resetExecCnt = false;
		}
		task->secData.waitCookie = cookie;
		cmdq_core_unlock_exec_path(&flags);

		task->trigger = sched_clock();

		/* execute */
		status = cmdq_sec_exec_task_async_unlocked(task, thread_id);
		if (status < 0) {
			/* config failed case, dump for more detail */
			cmdq_core_attach_error_task(task, thread_id);
			cmdq_core_turnoff_first_dump();
			cmdq_core_remove_task_from_thread_array_with_lock(
				thread, cookie);
		}
	} while (0);
	cmdq_sec_unlock_secure_path();

	if (status >= 0 && (task->taskState == TASK_STATE_KILLED ||
		task->taskState == TASK_STATE_ERROR ||
		task->taskState == TASK_STATE_ERR_IRQ)) {
		CMDQ_ERR("secure execute task async fail task:0x%p\n", task);
	}

	return status;
}

static s32 cmdq_sec_handle_wait_result(struct TaskStruct *task,
	s32 thread, const s32 waitQ)
{
	s32 i;
	s32 status;
	struct ThreadStruct *thread_context;
	/* error report */
	bool throw_aee = false;
	const char *module = NULL;
	s32 irq_flag = 0;
	struct cmdqSecCancelTaskResultStruct result;
	char parsed_inst[128] = { 0 };

	/* Init default status */
	status = 0;
	thread_context = &cmdq_core_get_context()->thread[thread];
	memset(&result, 0, sizeof(result));

	/* lock cmdqSecLock */
	cmdq_sec_lock_secure_path();

	do {
		s32 start_sec_thread = CMDQ_MIN_SECURE_THREAD_ID;
		struct task_private *private = CMDQ_TASK_PRIVATE(task);
		unsigned long flags = 0L;

		/* check if this task has finished */
		if (task->taskState == TASK_STATE_DONE)
			break;

		/* Oops, tha tasks is not done.
		 * We have several possible error scenario:
		 * 1. task still running (hang / timeout)
		 * 2. IRQ pending (done or error/timeout IRQ)
		 * 3. task's SW thread has been signaled (e.g. SIGKILL)
		 */

		/* dump shared cookie */
		CMDQ_LOG(
			"WAIT: [1]secure path failed task:%p thread:%d shared_cookie(%d, %d, %d)\n",
			task, thread,
			cmdq_sec_get_secure_thread_exec_counter(
				start_sec_thread),
			cmdq_sec_get_secure_thread_exec_counter(
				start_sec_thread + 1),
			cmdq_sec_get_secure_thread_exec_counter(
				start_sec_thread + 2));

		/* suppose that task failed, entry secure world to confirm it
		 * we entry secure world:
		 * .check if pending IRQ and update cookie value to shared
		 * memory
		 * .confirm if task execute done
		 * if not, do error handle
		 *     .recover M4U & DAPC setting
		 *     .dump secure HW thread
		 *     .reset CMDQ secure HW thread
		 */
		cmdq_sec_cancel_error_task_unlocked(task, thread, &result);

		status = -ETIMEDOUT;
		throw_aee = !(private && private->internal &&
			private->ignore_timeout);
		/* shall we pass the error instru back from secure path?? */
#if 0
		cmdq_core_parse_error(task, thread, &module, &irqFlag,
			&instA, &instB);
#endif
		module = cmdq_get_func()->parseErrorModule(task);

		/* module dump */
		cmdq_core_attach_error_task(task, thread);

		/* module reset */
		/* TODO: get needReset infor by secure thread PC */
		cmdq_core_reset_hw_engine(task->engineFlag);

		cmdq_core_lock_exec_path(&flags);
		/* remove all tasks in tread since we have reset HW thread
		 * in SWd
		 */
		for (i = 0; i < cmdq_core_max_task_in_thread(thread); i++) {
			task = thread_context->pCurTask[i];
			if (!task)
				continue;
			cmdq_core_remove_task_from_thread_array_by_cookie(
				thread_context, i, TASK_STATE_ERROR);
		}
		thread_context->taskCount = 0;
		thread_context->waitCookie = thread_context->nextCookie;
		cmdq_core_unlock_exec_path(&flags);
	} while (0);

	/* unlock cmdqSecLock */
	cmdq_sec_unlock_secure_path();

	/* throw AEE if nessary */
	if (throw_aee) {
		const u32 instA = result.errInstr[1];
		const u32 instB = result.errInstr[0];
		const u32 op = (instA & 0xFF000000) >> 24;

		cmdq_core_interpret_instruction(parsed_inst,
			sizeof(parsed_inst), op,
			instA & (~0xFF000000), instB);

		CMDQ_AEE(module,
			"%s in CMDQ IRQ:0x%02x, INST:(0x%08x, 0x%08x), OP:%s => %s\n",
			 module, irq_flag, instA, instB,
			 cmdq_core_parse_op(op), parsed_inst);
	}

	return status;
}


static void cmdq_sec_free_buffer_impl(struct TaskStruct *task)
{
	if (!task->is_client_buffer && task->cmd_buffer_va) {
		/* allocated kernel buffer for secure must free */
		kfree(task->cmd_buffer_va);
	}

	/* TODO: check if we need reset more in secData */
	if (!task->secData.addrMetadatas) {
		kfree(CMDQ_U32_PTR(task->secData.addrMetadatas));
		task->secData.addrMetadatas = 0;
	}

	kfree(task->secStatus);
	task->secStatus = NULL;

	task->cmd_buffer_va = NULL;
	task->is_client_buffer = false;
	task->secData.is_secure = false;
}

static void cmdq_sec_dump_err_buffer(const struct TaskStruct *task, u32 *hwpc)
{
	if (!task->cmd_buffer_va)
		return;

	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
		task->cmd_buffer_va, task->commandSize, true);
	cmdq_core_save_hex_first_dump("", 16, 4, task->cmd_buffer_va,
		task->commandSize);
}

static void cmdq_sec_dump_summary(const struct TaskStruct *task, s32 thread,
	const struct TaskStruct **ngtask_out,
	struct NGTaskInfoStruct **nginfo_out)
{
	u32 i;

	if (!task->secStatus)
		return;

	/* secure status may contains debug information */
	CMDQ_ERR(
		"Secure status: %d step: 0x%08x args: 0x%08x 0x%08x 0x%08x 0x%08x task: 0x%p\n",
		task->secStatus->status,
		task->secStatus->step,
		task->secStatus->args[0], task->secStatus->args[1],
		task->secStatus->args[2], task->secStatus->args[3],
		task);
	for (i = 0; i < task->secStatus->inst_index; i += 2) {
		CMDQ_ERR("Secure instruction %d: 0x%08x:%08x\n",
			i / 2,
			task->secStatus->sec_inst[i],
			task->secStatus->sec_inst[i+1]);
	}
}

/* core controller for secure function */
static const struct cmdq_controller g_cmdq_sec_ctrl = {
	.compose = cmdq_sec_task_compose,
	.copy_command = cmdq_sec_task_copy_command,
	.get_thread_id = cmdq_sec_get_thread_id,
	.execute_prepare = cmdq_sec_exec_task_prepare,
	.execute = cmdq_sec_exec_task_async_impl,
	.handle_wait_result = cmdq_sec_handle_wait_result,
	.free_buffer = cmdq_sec_free_buffer_impl,
	.append_command = cmdq_sec_append_command,
	.dump_err_buffer = cmdq_sec_dump_err_buffer,
	.dump_summary = cmdq_sec_dump_summary,
	.change_jump = false,
};

/* CMDQ Secure controller */
const struct cmdq_controller *cmdq_sec_get_controller(void)
{
	return &g_cmdq_sec_ctrl;
}
#endif				/* CMDQ_SECURE_PATH_SUPPORT */

/******************************************************************************
 * common part: for general projects
 *****************************************************************************/
int32_t cmdq_sec_create_shared_memory(
	struct cmdqSecSharedMemoryStruct **pHandle, const uint32_t size)
{
	struct cmdqSecSharedMemoryStruct *handle = NULL;
	void *pVA = NULL;
	dma_addr_t PA = 0;

	handle = kzalloc(sizeof(uint8_t *) *
		sizeof(struct cmdqSecSharedMemoryStruct), GFP_KERNEL);
	if (handle == NULL)
		return -ENOMEM;

	CMDQ_LOG("%s\n", __func__);

	/* allocate non-cachable memory */
	pVA = cmdq_core_alloc_hw_buffer(cmdq_dev_get(), size, &PA,
		GFP_KERNEL);

	CMDQ_MSG("%s, MVA:%pa, pVA:%p, size:%d\n", __func__, &PA, pVA, size);

	if (pVA == NULL) {
		kfree(handle);
		return -ENOMEM;
	}

	/* update memory information */
	handle->size = size;
	handle->pVABase = pVA;
	handle->MVABase = PA;

	*pHandle = handle;

	return 0;
}

int32_t cmdq_sec_destroy_shared_memory(
	struct cmdqSecSharedMemoryStruct *handle)
{

	if (handle && handle->pVABase) {
		cmdq_core_free_hw_buffer(cmdq_dev_get(), handle->size,
			handle->pVABase, handle->MVABase);
	}

	kfree(handle);
	handle = NULL;

	return 0;
}

int32_t cmdq_sec_exec_task_async_unlocked(
	struct TaskStruct *pTask, int32_t thread)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	status = cmdq_sec_submit_to_secure_world_async_unlocked(
		CMD_CMDQ_TL_SUBMIT_TASK, pTask, thread, NULL, true);
	if (status < 0) {
		/* Error status print */
		CMDQ_ERR("%s[%d]\n", __func__, status);
	}

	return status;

#else
	CMDQ_ERR("secure path not support\n");
	return -EFAULT;
#endif
}

int32_t cmdq_sec_cancel_error_task_unlocked(
	struct TaskStruct *pTask, int32_t thread,
	struct cmdqSecCancelTaskResultStruct *pResult)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	if (!pTask || !cmdq_get_func()->isSecureThread(thread) || !pResult) {

		CMDQ_ERR("%s invalid param, pTask:%p, thread:%d, pResult:%p\n",
			 __func__, pTask, thread, pResult);
		return -EFAULT;
	}

	status = cmdq_sec_submit_to_secure_world_async_unlocked(
		CMD_CMDQ_TL_CANCEL_TASK,
		pTask, thread, (void *)pResult, true);
	return status;
#else
	CMDQ_ERR("secure path not support\n");
	return -EFAULT;
#endif
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
static atomic_t gCmdqSecPathResource = ATOMIC_INIT(0);
#endif

int32_t cmdq_sec_allocate_path_resource_unlocked(bool throwAEE)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	CMDQ_MSG("%s throwAEE: %s", __func__, throwAEE ? "true" : "false");

	if (atomic_cmpxchg(&gCmdqSecPathResource, 0, 1) != 0) {
		/* has allocated successfully */
		return status;
	}

	status = cmdq_sec_submit_to_secure_world_async_unlocked(
		CMD_CMDQ_TL_PATH_RES_ALLOCATE, NULL,
		CMDQ_INVALID_THREAD, NULL, throwAEE);
	if (status < 0) {
		/* Error status print */
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

s32 cmdq_sec_get_secure_thread_exec_counter(const s32 thread)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	const s32 offset = CMDQ_SEC_SHARED_THR_CNT_OFFSET + thread *
		sizeof(s32);
	u32 *pVA;
	u32 value;

	if (!cmdq_get_func()->isSecureThread(thread)) {
		CMDQ_ERR("%s invalid param, thread:%d\n", __func__, thread);
		return -EFAULT;
	}

	if (!cmdq_core_get_context()->hSecSharedMem) {
		CMDQ_ERR("%s shared memory is not created\n", __func__);
		return -EFAULT;
	}

	pVA = (u32 *)(cmdq_core_get_context()->hSecSharedMem->pVABase +
		offset);
	value = *pVA;

	CMDQ_VERBOSE("[shared_cookie] get thread %d CNT(%p) value is %d\n",
		thread, pVA, value);

	return value;
#else
	CMDQ_ERR(
		"func:%s failed since CMDQ secure path not support in this proj\n",
		__func__);
	return -EFAULT;
#endif

}

/******************************************************************************
 * common part: SecContextHandle handle
 *****************************************************************************/
struct cmdqSecContextStruct *cmdq_sec_context_handle_create(
	uint32_t tgid)
{
	struct cmdqSecContextStruct *handle = NULL;

	handle = kzalloc(sizeof(uint8_t *) *
		sizeof(struct cmdqSecContextStruct), GFP_ATOMIC);
	if (handle) {
		handle->state = IWC_INIT;
		handle->tgid = tgid;
	} else {
		CMDQ_ERR("SecCtxHandle_CREATE: err[LOW_MEM], tgid[%d]\n",
			tgid);
	}

	CMDQ_MSG("SecCtxHandle_CREATE: create new, H[0x%p], tgid[%d]\n",
		handle, tgid);
	return handle;
}

/******************************************************************************
 * common part: init, deinit, path
 *****************************************************************************/
void cmdq_sec_lock_secure_path(void)
{
	mutex_lock(&gCmdqSecExecLock);
	/* set memory barrier for lock */
	smp_mb();
}

void cmdq_sec_unlock_secure_path(void)
{
	mutex_unlock(&gCmdqSecExecLock);
}

void cmdqSecDeInitialize(void)
{
}

void cmdqSecInitialize(void)
{
	INIT_LIST_HEAD(&gCmdqSecContextList);

#ifdef CMDQ_SECURE_PATH_SUPPORT
	CMDQ_MSG("sec controller:0x%p\n", &g_cmdq_sec_ctrl);
#endif
}

void cmdqSecEnableProfile(const bool enable)
{
}

