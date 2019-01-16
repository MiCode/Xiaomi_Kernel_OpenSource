#include <linux/slab.h>

#include "cmdq_sec.h"
#include "cmdq_def.h"
#include "cmdq_device.h"
#include "cmdq_platform.h"

static DEFINE_MUTEX(gCmdqSecExecLock);	/* lock to protect atomic secure task execution */
static DEFINE_MUTEX(gCmdqSecProfileLock);	/* ensure atomic enable/disable secure path profile */
static struct list_head gCmdqSecContextList;	/* secure context list. note each porcess has its own sec context */

/* function declaretion */
cmdqSecContextHandle cmdq_sec_context_handle_create(uint32_t tgid);

#if defined(CMDQ_SECURE_PATH_SUPPORT)

/* mobicore driver interface */
#include "mobicore_driver_api.h"
/* sectrace interface */
#ifdef CMDQ_SECTRACE_SUPPORT
#include <linux/sectrace.h>
#endif
/* secure path header */
#include "cmdqSecTl_Api.h"

#define CMDQ_DR_UUID { { 2, 0xb, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }
#define CMDQ_TL_UUID { { 9, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }

static cmdqSecContextHandle gCmdqSecContextHandle = NULL; /* secure context to cmdqSecTL */
/* sectrace */
struct mc_bulk_map gCmdqSectraceMappedInfo; /* sectrace log buffer, which mapped between NWd and SWd */

/* internal control */

/* Set 1 to open once for each process context, because of below reasons:
 * 1. mc_open_session is too slow (major)
 * 2. we entry secure world for config and trigger GCE, and wait in normal world
 */
#define CMDQ_OPEN_SESSION_ONCE (1)

/********************************************************************************
 * operator API
 *******************************************************************************/
int32_t cmdq_sec_open_mobicore_impl(uint32_t deviceId)
{
	int32_t status = 0;
	enum mc_result mcRet = mc_open_device(deviceId);

	/* Currently, a process context limits to open mobicore device once, */
	/* and mc_open_device dose not support reference cout */
	/* so skip the false alarm error.... */
	if (MC_DRV_ERR_INVALID_OPERATION == mcRet) {
		CMDQ_MSG("[SEC]_MOBICORE_OPEN: already opened, continue to execution\n");
		status = -EEXIST;
	} else if (MC_DRV_OK != mcRet) {
		CMDQ_ERR("[SEC]_MOBICORE_OPEN: err[0x%x]\n", mcRet);
		status = -1;
	}

	CMDQ_MSG("[SEC]_MOBICORE_OPEN: status[%d], ret[0x%x]\n", status, mcRet);
	return status;
}

int32_t cmdq_sec_close_mobicore_impl(const uint32_t deviceId, const uint32_t openMobicoreByOther)
{
	int32_t status = 0;
	enum mc_result mcRet = 0;

	if (1 == openMobicoreByOther) {
		/* do nothing */
		/* let last user to close mobicore.... */
		CMDQ_MSG("[SEC]_MOBICORE_CLOSE: opened by other, bypass device close\n");
	} else {
		mcRet = mc_close_device(deviceId);
		CMDQ_MSG("[SEC]_MOBICORE_CLOSE: status[%d], ret[%0x], openMobicoreByOther[%d]\n",
			 status, mcRet, openMobicoreByOther);
		if (MC_DRV_OK != mcRet) {
			CMDQ_ERR("[SEC]_MOBICORE_CLOSE: err[0x%x]\n", mcRet);
			status = -1;
		}
	}

	return status;
}

int32_t cmdq_sec_allocate_wsm_impl(uint32_t deviceId, uint8_t **ppWsm, uint32_t wsmSize)
{
	int32_t status = 0;
	enum mc_result mcRet = MC_DRV_OK;

	do {
		if ((*ppWsm) != NULL) {
			status = -1;
			CMDQ_ERR("[SEC]_WSM_ALLOC: err[pWsm is not NULL]");
			break;
		}

		/* because world shared mem(WSM) will ba managed by mobicore device, not linux kernel */
		/* instead of vmalloc/kmalloc, call mc_malloc_wasm to alloc WSM to prvent error such as */
		/* "can not resolve tci physicall address" etc */
		mcRet = mc_malloc_wsm(deviceId, 0, wsmSize, ppWsm, 0);
		if (MC_DRV_OK != mcRet) {
			CMDQ_ERR("[SEC]_WSM_ALLOC: err[0x%x]\n", mcRet);
			status = -1;
			break;
		}

		CMDQ_MSG("[SEC]_WSM_ALLOC: status[%d], *ppWsm: 0x%p\n", status, (*ppWsm));
	} while (0);

	return status;
}

int32_t cmdq_sec_free_wsm_impl(uint32_t deviceId, uint8_t **ppWsm)
{
	int32_t status = 0;
	enum mc_result mcRet = mc_free_wsm(deviceId, (*ppWsm));

	(*ppWsm) = (MC_DRV_OK == mcRet) ? (NULL) : (*ppWsm);
	CMDQ_VERBOSE("_WSM_FREE: ret[0x%x], *ppWsm[0x%p]\n", mcRet, (*ppWsm));

	if (MC_DRV_OK != mcRet) {
		CMDQ_ERR("_WSM_FREE: err[0x%x]", mcRet);
		status = -1;
	}

	return status;
}

int32_t cmdq_sec_open_session_impl(uint32_t deviceId,
				const struct mc_uuid_t *uuid,
				uint8_t *pWsm,
				uint32_t wsmSize,
				struct mc_session_handle *pSessionHandle)
{
	int32_t status = 0;
	enum mc_result mcRet = MC_DRV_OK;
	do {
		if (NULL == pWsm || NULL == pSessionHandle) {
			status = -1;
			CMDQ_ERR("[SEC]_SESSION_OPEN: invalid param, pWsm[0x%p], pSessionHandle[0x%p]\n",
			     pWsm, pSessionHandle);
			break;
		}

		memset(pSessionHandle, 0, sizeof(*pSessionHandle));
		pSessionHandle->device_id = deviceId;
		mcRet = mc_open_session(pSessionHandle, uuid, pWsm, wsmSize);
		if (MC_DRV_OK != mcRet) {
			CMDQ_ERR("[SEC]_SESSION_OPEN: err[0x%x]\n", mcRet);
			status = -1;
			break;
		}

		CMDQ_MSG("[SEC]_SESSION_OPEN: status[%d], mcRet[0x%x]\n", status, mcRet);
	} while (0);

	return status;
}

int32_t cmdq_sec_close_session_impl(struct mc_session_handle *pSessionHandle)
{
	int32_t status = 0;
	enum mc_result mcRet = mc_close_session(pSessionHandle);

	if (MC_DRV_OK != mcRet) {
		CMDQ_ERR("_SESSION_CLOSE: err[0x%x]", mcRet);
		status = -1;
	}
	return status;
}

int32_t cmdq_sec_init_session_unlocked(const struct mc_uuid_t *uuid,
				       uint8_t **ppWsm,
				       uint32_t wsmSize,
				       struct mc_session_handle *pSessionHandle,
				       CMDQ_IWC_STATE_ENUM *pIwcState,
				       uint32_t *openMobicoreByOther)
{
	int32_t openRet = 0;
	int32_t status = 0;
	uint32_t deviceId = MC_DEVICE_ID_DEFAULT;

	CMDQ_MSG("[SEC]-->SESSION_INIT: iwcState[%d]\n", (*pIwcState));

	do {
#if CMDQ_OPEN_SESSION_ONCE
		if (IWC_SES_OPENED <= (*pIwcState)) {
			CMDQ_MSG("SESSION_INIT: already opened\n");
			break;
		} else {
			CMDQ_MSG("[SEC]SESSION_INIT: open new session[%d]\n", (*pIwcState));
		}
#endif
		CMDQ_VERBOSE
		    ("[SEC]SESSION_INIT: wsmSize[%d], pSessionHandle: 0x%p\n",
		     wsmSize, pSessionHandle);

		CMDQ_PROF_START("CMDQ_SEC_INIT");

		/* open mobicore device */
		openRet = cmdq_sec_open_mobicore_impl(deviceId);
		if (-EEXIST == openRet) {
			/* mobicore has been opened in this process context */
			/* it is a ok case, so continue to execute */
			status = 0;
			(*openMobicoreByOther) = 1;
		} else if (0 > openRet) {
			status = -1;
			break;
		}
		(*pIwcState) = IWC_MOBICORE_OPENED;


		/* allocate world shared memory */
		if (0 > cmdq_sec_allocate_wsm_impl(deviceId, ppWsm, wsmSize)) {
			status = -1;
			break;
		}
		(*pIwcState) = IWC_WSM_ALLOCATED;

		/* open a secure session */
		if (0 >
		    cmdq_sec_open_session_impl(deviceId, uuid, (*ppWsm), wsmSize, pSessionHandle)) {
			status = -1;
			break;
		}
		(*pIwcState) = IWC_SES_OPENED;

		CMDQ_PROF_END("CMDQ_SEC_INIT");
	} while (0);

	CMDQ_MSG("[SEC]<--SESSION_INIT[%d]\n", status);
	return status;
}

int32_t cmdq_sec_fill_iwc_command_basic_unlocked(int32_t iwcCommand, void *_pTask, int32_t thread, void *_pIwc)
{
	iwcCmdqMessage_t *pIwc;

	pIwc = (iwcCmdqMessage_t *)_pIwc;

	/* specify command id only, don't care other other */
	memset(pIwc, 0x0, sizeof(iwcCmdqMessage_t));
	pIwc->cmd = iwcCommand;

	/* medatada: debug config */
	pIwc->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) : (0);
	pIwc->debug.enableProfile = cmdq_core_profile_enabled();

	return 0;
}

int32_t cmdq_sec_fill_iwc_command_msg_unlocked(int32_t iwcCommand, void *_pTask, int32_t thread, void *_pIwc)
{
	int32_t status;
	const TaskStruct *pTask = (TaskStruct *)_pTask;
	iwcCmdqMessage_t *pIwc;
	/* cmdqSecDr will insert some instr*/
	const uint32_t reservedCommandSize = 4 * CMDQ_INST_SIZE;

	status = 0;
	pIwc = (iwcCmdqMessage_t *)_pIwc;

	/* check command size first */
	if (pTask && (CMDQ_TZ_CMD_BLOCK_SIZE < (pTask->commandSize + reservedCommandSize))) {
		CMDQ_ERR("[SEC]SESSION_MSG: pTask %p commandSize %d > %d\n",
			pTask, pTask->commandSize, CMDQ_TZ_CMD_BLOCK_SIZE);
		return -EFAULT;
	}

	CMDQ_MSG("[SEC]-->SESSION_MSG: cmdId[%d]\n", iwcCommand);

	/* fill message buffer for inter world communication */
	memset(pIwc, 0x0, sizeof(iwcCmdqMessage_t));
	pIwc->cmd = iwcCommand;

	/* metadata */
	pIwc->command.metadata.enginesNeedDAPC = pTask->secData.enginesNeedDAPC;
	pIwc->command.metadata.enginesNeedPortSecurity = pTask->secData.enginesNeedPortSecurity;

	if (NULL != pTask && CMDQ_INVALID_THREAD != thread) {
		/* basic data */
		pIwc->command.scenario = pTask->scenario;
		pIwc->command.thread = thread;
		pIwc->command.priority = pTask->priority;
		pIwc->command.engineFlag = pTask->engineFlag;
		pIwc->command.commandSize = pTask->commandSize;
		pIwc->command.hNormalTask = 0LL | ((unsigned long) pTask);
		memcpy((pIwc->command.pVABase), (pTask->pVABase), (pTask->commandSize));

		/* cookie */
		pIwc->command.waitCookie = pTask->secData.waitCookie;
		pIwc->command.resetExecCnt = pTask->secData.resetExecCnt;

		CMDQ_MSG("[SEC]SESSION_MSG: task 0x%p, thread: %d, size: %d, bufferSize: %d, scenario:%d, flag:0x%08llx ,hNormalTask:0x%llx\n",
			pTask, thread, pTask->commandSize, pTask->bufferSize, pTask->scenario, pTask->engineFlag, pIwc->command.hNormalTask);

		CMDQ_VERBOSE("[SEC]SESSION_MSG: addrList[%d][0x%p]\n",
			pTask->secData.addrMetadataCount,CMDQ_U32_PTR(pTask->secData.addrMetadatas));
		if (0 < pTask->secData.addrMetadataCount) {
			pIwc->command.metadata.addrListLength = pTask->secData.addrMetadataCount;
			memcpy((pIwc->command.metadata.addrList), (pTask->secData.addrMetadatas),
			       (pTask->secData.addrMetadataCount) * sizeof(iwcCmdqAddrMetadata_t));
		}

		/* medatada: debug config */
		pIwc->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) : (0);
		pIwc->debug.enableProfile = cmdq_core_profile_enabled();
	} else {
		/* relase resource, or debug function will go here */
		CMDQ_VERBOSE("[SEC]-->SESSION_MSG: no task, cmdId[%d]\n", iwcCommand);
		pIwc->command.commandSize = 0;
		pIwc->command.metadata.addrListLength = 0;
	}

	CMDQ_MSG("[SEC]<--SESSION_MSG[%d]\n", status);
	return status;
}

// TODO: when do release secure command buffer
int32_t cmdq_sec_fill_iwc_resource_msg_unlocked(int32_t iwcCommand, void *_pTask, int32_t thread, void *_pIwc)
{
	iwcCmdqMessage_t *pIwc;
	cmdqSecSharedMemoryHandle pSharedMem;

	pSharedMem = cmdq_core_get_secure_shared_memory();
	if (NULL == pSharedMem) {
		CMDQ_ERR("FILL:RES, NULL shared memory\n");
		return -EFAULT;
	}

	if(pSharedMem && NULL == pSharedMem->pVABase) {
		CMDQ_ERR("FILL:RES, %p shared memory has not init\n", pSharedMem);
		return -EFAULT;
	}

	pIwc = (iwcCmdqMessage_t *)_pIwc;
	memset(pIwc, 0x0, sizeof(iwcCmdqMessage_t));

	pIwc->cmd = iwcCommand;
	pIwc->pathResource.shareMemoyPA = 0LL | (pSharedMem->MVABase);
	pIwc->pathResource.size = pSharedMem->size;

	CMDQ_MSG("FILL:RES, shared memory:%pa(0x%llx), size:%d\n",
		&(pSharedMem->MVABase), pIwc->pathResource.shareMemoyPA, pSharedMem->size);

	/* medatada: debug config */
	pIwc->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) : (0);
	pIwc->debug.enableProfile = cmdq_core_profile_enabled();

	return 0;
}

int32_t cmdq_sec_fill_iwc_cancel_msg_unlocked(int32_t iwcCommand, void *_pTask, int32_t thread, void *_pIwc)
{
	const TaskStruct *pTask = (TaskStruct *)_pTask;
	iwcCmdqMessage_t *pIwc;

	pIwc = (iwcCmdqMessage_t *)_pIwc;
	memset(pIwc, 0x0, sizeof(iwcCmdqMessage_t));

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

int32_t cmdq_sec_execute_session_unlocked(struct mc_session_handle *pSessionHandle,
					  CMDQ_IWC_STATE_ENUM *pIwcState, int32_t timeout_ms)
{
	enum mc_result mcRet;
	int32_t status = 0;
	const int32_t secureWoldTimeout_ms = (0 < timeout_ms) ?
										(timeout_ms) :
										(MC_INFINITE_TIMEOUT);

	CMDQ_PROF_START("CMDQ_SEC_EXE");

	do {
		/* notify to secure world */
		mcRet = mc_notify(pSessionHandle);
		if (MC_DRV_OK != mcRet) {
			CMDQ_ERR("[SEC]EXEC: mc_notify err[0x%x]\n", mcRet);
			status = -1;
			break;
		} else {
			CMDQ_MSG("[SEC]EXEC: mc_notify ret[0x%x]\n", mcRet);
		}
		(*pIwcState) = IWC_SES_TRANSACTED;


		/* wait respond */
		mcRet = mc_wait_notification(pSessionHandle, secureWoldTimeout_ms);
		if (MC_DRV_ERR_TIMEOUT == mcRet) {
			CMDQ_ERR
			    ("[SEC]EXEC: mc_wait_notification timeout, err[0x%x], secureWoldTimeout_ms[%d]\n",
			     mcRet, secureWoldTimeout_ms);
			status = -ETIMEDOUT;
			break;
		}

		if (MC_DRV_OK != mcRet) {
			CMDQ_ERR("[SEC]EXEC: mc_wait_notification err[0x%x]\n", mcRet);
			status = -1;
			break;
		} else {
			CMDQ_MSG("[SEC]EXEC: mc_wait_notification err[%d]\n", mcRet);
		}
		(*pIwcState) = IWC_SES_ON_TRANSACTED;
	} while (0);

	CMDQ_PROF_END("CMDQ_SEC_EXE");

	return status;
}

void cmdq_sec_deinit_session_unlocked(uint8_t **ppWsm,
				      struct mc_session_handle *pSessionHandle,
				      const CMDQ_IWC_STATE_ENUM iwcState,
				      const uint32_t openMobicoreByOther)
{
	uint32_t deviceId = MC_DEVICE_ID_DEFAULT;

	CMDQ_MSG("[SEC]-->SESSION_DEINIT\n");
	do {
		switch (iwcState) {
		case IWC_SES_ON_TRANSACTED:
		case IWC_SES_TRANSACTED:
		case IWC_SES_MSG_PACKAGED:
			/* continue next clean-up */
		case IWC_SES_OPENED:
			cmdq_sec_close_session_impl(pSessionHandle);
			/* continue next clean-up */
		case IWC_WSM_ALLOCATED:
			cmdq_sec_free_wsm_impl(deviceId, ppWsm);
			/* continue next clean-up */
		case IWC_MOBICORE_OPENED:
			cmdq_sec_close_mobicore_impl(deviceId, openMobicoreByOther);
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

/********************************************************************************
 * context handle
 *******************************************************************************/
int32_t cmdq_sec_setup_context_session(cmdqSecContextHandle handle)
{
	int32_t status = 0;
	const struct mc_uuid_t uuid = CMDQ_TL_UUID;

	/* init iwc parameter */
	if (IWC_INIT == handle->state) {
		handle->uuid = uuid;
	}
	/* init secure session */
	status = cmdq_sec_init_session_unlocked(&(handle->uuid),
						(uint8_t **) (&(handle->iwcMessage)),
						sizeof(iwcCmdqMessage_t),
						&(handle->sessionHandle),
						&(handle->state), &(handle->openMobicoreByOther));
	CMDQ_MSG("SEC_SETUP: status[%d], tgid[%d], mobicoreOpenByOther[%d]\n",
			status, handle->tgid, handle->openMobicoreByOther);
	return status;
}

int32_t cmdq_sec_handle_session_reply_unlocked(const iwcCmdqMessage_t *pIwc,
			const int32_t iwcCommand, TaskStruct *pTask, void *data)
{
	int32_t status;
	int32_t iwcRsp;
	cmdqSecCancelTaskResultStruct *pCancelResult = NULL;

	/* get secure task execution result */
	iwcRsp = (pIwc)->rsp;
	status = iwcRsp;

	if (CMD_CMDQ_TL_CANCEL_TASK == iwcCommand) {
		pCancelResult = (cmdqSecCancelTaskResultStruct *)data;
		if (pCancelResult) {
			pCancelResult->throwAEE = pIwc->cancelTask.throwAEE;
			pCancelResult->hasReset = pIwc->cancelTask.hasReset;
			pCancelResult->irqFlag = pIwc->cancelTask.irqFlag;
			pCancelResult->errInstr[0]= pIwc->cancelTask.errInstr[0]; /* argB */
			pCancelResult->errInstr[1] = pIwc->cancelTask.errInstr[1]; /* argA */
			pCancelResult->pc = pIwc->cancelTask.pc;
		}

		CMDQ_ERR("CANCEL_TASK: pTask %p, INST:(0x%08x, 0x%08x), throwAEE:%d, hasReset:%d, pc:0x%08x\n",
			pTask, pIwc->cancelTask.errInstr[1], pIwc->cancelTask.errInstr[0],
			pIwc->cancelTask.throwAEE, pIwc->cancelTask.hasReset, pIwc->cancelTask.pc);
	} else if (CMD_CMDQ_TL_PATH_RES_ALLOCATE == iwcCommand || CMD_CMDQ_TL_PATH_RES_RELEASE == iwcCommand ) {
		/* do nothing*/
	} else {
		/* note we etnry SWd to config GCE, and wait execution result in NWd */
		/* udpate taskState only if config failed*/
		if (pTask && 0 > iwcRsp) {
			pTask->taskState= TASK_STATE_ERROR;
		}
	}

	/* log print */
	if (0 < status) {
		CMDQ_ERR("SEC_SEND: status[%d], cmdId[%d], iwcRsp[%d]\n",
			status, iwcCommand, iwcRsp);
	} else {
		CMDQ_MSG("SEC_SEND: status[%d], cmdId[%d], iwcRsp[%d]\n",
			status, iwcCommand, iwcRsp);
	}

	return status;
}


CmdqSecFillIwcCB cmdq_sec_get_iwc_msg_fill_cb_by_iwc_command(uint32_t iwcCommand)
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

int32_t cmdq_sec_send_context_session_message(cmdqSecContextHandle handle,
					      uint32_t iwcCommand,
					      TaskStruct *pTask,
					      int32_t thread, CmdqSecFillIwcCB cb, void *data)
{
	int32_t status;
	const int32_t timeout_ms = (3 * 1000);
	const CmdqSecFillIwcCB iwcFillCB = (NULL == cb) ?
					cmdq_sec_get_iwc_msg_fill_cb_by_iwc_command(iwcCommand) :
					(cb);

	do {
		/* fill message bufer */
		status = iwcFillCB(iwcCommand, pTask, thread, (void*)(handle->iwcMessage));

		if (0 > status) {
			CMDQ_ERR("[SEC]fill msg buffer failed[%d], pid[%d:%d], cmdId[%d]\n",
				 status, current->tgid, current->pid, iwcCommand);
			break;
		}

		/* send message */
		status = cmdq_sec_execute_session_unlocked(
							&(handle->sessionHandle),
							&(handle->state),
							timeout_ms);
		if (0 > status) {
			CMDQ_ERR("[SEC]cmdq_sec_execute_session_unlocked failed[%d], pid[%d:%d]\n",
				 status, current->tgid, current->pid);
			break;
		}

		status = cmdq_sec_handle_session_reply_unlocked(handle->iwcMessage, iwcCommand, pTask, data);
	} while (0);

	return status;
}

int32_t cmdq_sec_teardown_context_session(cmdqSecContextHandle handle)
{
	int32_t status = 0;
	if (handle) {
		CMDQ_MSG("[SEC]SEC_TEARDOWN: state: %d, iwcMessage:0x%p\n",
			handle->state, handle->iwcMessage);

		cmdq_sec_deinit_session_unlocked(
						(uint8_t **) (&(handle->iwcMessage)),
						&(handle->sessionHandle),
						handle->state,
						handle->openMobicoreByOther);

		/* clrean up handle's attritubes */
		handle->state = IWC_INIT;

	} else {
		CMDQ_ERR("[SEC]SEC_TEARDOWN: null secCtxHandle\n");
		status = -1;
	}
	return status;
}

void cmdq_sec_track_task_record(const uint32_t iwcCommand,
				TaskStruct *pTask, CMDQ_TIME *pEntrySec, CMDQ_TIME *pExitSec)
{
	if (NULL == pTask) {
		return;
	}

	if(CMD_CMDQ_TL_SUBMIT_TASK != iwcCommand) {
		return;
	}

	/* record profile data */
	/* tbase timer/time support is not enough currently, */
	/* so we treats entry/exit timing to secure world as the trigger timing */
	pTask->entrySec = *pEntrySec;
	pTask->exitSec = *pExitSec;
	pTask->trigger = *pEntrySec;
}

int32_t cmdq_sec_submit_to_secure_world_async_unlocked(uint32_t iwcCommand,
				TaskStruct *pTask, int32_t thread, CmdqSecFillIwcCB iwcFillCB, void *data)
{
	const int32_t tgid = current->tgid;
	const int32_t pid = current->pid;
	cmdqSecContextHandle handle = NULL;
	int32_t status = 0;
	int32_t duration = 0;

	CMDQ_TIME tEntrySec;
	CMDQ_TIME tExitSec;

	CMDQ_MSG("[SEC]-->SEC_SUBMIT: tgid[%d:%d]\n", tgid, pid);
	do {
		/* find handle first */

		/* Unlike tBase user space API,
		 * tBase kernel API maintains a GLOBAL table to control mobicore device reference count.
		 * For kernel spece user, mc_open_device and session_handle don't depend on the process context.
		 * Therefore we use global secssion handle to inter-world commumication. */
		if(NULL == gCmdqSecContextHandle) {
			gCmdqSecContextHandle = cmdq_sec_context_handle_create(current->tgid);
		}
		handle = gCmdqSecContextHandle;

		if (NULL == handle) {
			CMDQ_ERR("SEC_SUBMIT: tgid %d err[NULL secCtxHandle]\n", tgid);
			status = -(CMDQ_ERR_NULL_SEC_CTX_HANDLE);
			break;
		}

		if (0 > cmdq_sec_setup_context_session(handle)) {
			status = -(CMDQ_ERR_SEC_CTX_SETUP);
			break;
		}

		tEntrySec = sched_clock();

		status = cmdq_sec_send_context_session_message(
					handle, iwcCommand, pTask, thread, iwcFillCB, data);

		tExitSec = sched_clock();
		CMDQ_GET_TIME_IN_US_PART(tEntrySec, tExitSec, duration);
		cmdq_sec_track_task_record(iwcCommand, pTask, &tEntrySec, &tExitSec);

		/* release resource */
#if !(CMDQ_OPEN_SESSION_ONCE)
		cmdq_sec_teardown_context_session(handle)
#endif

		/* Note we entry secure for config only and wait result in normal world. */
		/* No need reset module HW for config failed case*/
	} while (0);


	if (-ETIMEDOUT == status) {
		/* t-base strange issue, mc_wait_notification false timeout when secure world has done */
		/* becuase retry may failed, give up retry method */
		CMDQ_AEE("CMDQ",
			"[SEC]<--SEC_SUBMIT: err[%d][mc_wait_notification timeout], pTask[0x%p], THR[%d], tgid[%d:%d], config_duration_ms[%d], cmdId[%d]\n",
			 status, pTask, thread, tgid, pid, duration, iwcCommand);

	} else if (0 > status) {
		/* dump metadata first */
		if (pTask) {
			cmdq_core_dump_secure_metadata(&(pTask->secData));
		}

		/* throw AEE */
		CMDQ_AEE("CMDQ",
			 "[SEC]<--SEC_SUBMIT: err[%d], pTask[0x%p], THR[%d], tgid[%d:%d], config_duration_ms[%d], cmdId[%d]\n",
			 status, pTask, thread, tgid, pid, duration, iwcCommand);
	} else {
		CMDQ_LOG
		    ("[SEC]<--SEC_SUBMIT: err[%d], pTask[0x%p], THR[%d], tgid[%d:%d], config_duration_ms[%d], cmdId[%d]\n",
		     status, pTask, thread, tgid, pid, duration, iwcCommand);
	}
	return status;
}

/********************************************************************************
 * sectrace
 *******************************************************************************/

int32_t cmdq_sec_fill_iwc_command_sectrace_unlocked(int32_t iwcCommand, void *_pTask, int32_t thread, void *_pIwc)
{
	iwcCmdqMessage_t *pIwc;

	pIwc = (iwcCmdqMessage_t *)_pIwc;

	/* specify command id only, don't care other other */
	memset(pIwc, 0x0, sizeof(iwcCmdqMessage_t));
	pIwc->cmd = iwcCommand;

	switch (iwcCommand){
	case CMD_CMDQ_TL_SECTRACE_MAP:
		pIwc->sectracBuffer.addr = (uint32_t)(gCmdqSectraceMappedInfo.secure_virt_addr);
		pIwc->sectracBuffer.size = (gCmdqSectraceMappedInfo.secure_virt_len);
		break;
	case CMD_CMDQ_TL_SECTRACE_UNMAP:
	case CMD_CMDQ_TL_SECTRACE_TRANSACT:
	default:
		pIwc->sectracBuffer.addr = 0;
		pIwc->sectracBuffer.size = 0;
		break;
	}

	/* medatada: debug config */
	pIwc->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) : (0);
	pIwc->debug.enableProfile = cmdq_core_profile_enabled();

	CMDQ_LOG("[sectrace]SESSION_MSG: iwcCommand:%d, msg(sectraceBuffer, addr:0x%x, size:%d)\n",
		iwcCommand, pIwc->sectracBuffer.addr, pIwc->sectracBuffer.size);

	return 0;
}

static int cmdq_sec_sectrace_map(void *va, size_t size)
{
	int status;
	enum mc_result mcRet;

	CMDQ_LOG("[sectrace]-->map: start, va:%p, size:%d\n", va, (int)size);

	status = 0;
	cmdq_sec_lock_secure_path();
	do {
		/* HACK: submit a dummy message to ensure secure path init done */
		status = cmdq_sec_submit_to_secure_world_async_unlocked(
			CMD_CMDQ_TL_TEST_HELLO_TL, NULL, CMDQ_INVALID_THREAD, NULL, NULL);

		/* map log buffer in NWd */
		mcRet = mc_map(&(gCmdqSecContextHandle->sessionHandle),
					va,
					(uint32_t)size,
					&gCmdqSectraceMappedInfo);
		if (MC_DRV_OK != mcRet) {
			CMDQ_ERR("[sectrace]map: failed in NWd, mc_map err: 0x%x\n", mcRet);
			status = -EFAULT;
			break;
		}

		CMDQ_LOG("[sectrace]map: mc_map sectrace buffer done, gCmdqSectraceMappedInfo(va:0x%08x, size:%d)\n",
			gCmdqSectraceMappedInfo.secure_virt_addr, gCmdqSectraceMappedInfo.secure_virt_len);

		/* ask secure CMDQ to map sectrace log buffer */
		status = cmdq_sec_submit_to_secure_world_async_unlocked(
					CMD_CMDQ_TL_SECTRACE_MAP,
					NULL, CMDQ_INVALID_THREAD,
					cmdq_sec_fill_iwc_command_sectrace_unlocked,
					NULL);
		if(0 > status) {
			CMDQ_ERR("[sectrace]map: failed in SWd: %d\n", status);
			mc_unmap(&(gCmdqSecContextHandle->sessionHandle), va, &gCmdqSectraceMappedInfo);
			status = -EFAULT;
			break;
		}
	} while(0);

	cmdq_sec_unlock_secure_path();

	CMDQ_LOG("[sectrace]<--map: status: %d\n", status);

	return status;
}

static int cmdq_sec_sectrace_unmap(void *va, size_t size)
{
	int status;
	enum mc_result mcRet;

	status = 0;
	cmdq_sec_lock_secure_path();
	do {
		if(NULL == gCmdqSecContextHandle) {
			status = -EFAULT;
			break;
		}

		/* ask secure CMDQ to unmap sectrace log buffer */
		status = cmdq_sec_submit_to_secure_world_async_unlocked(
					CMD_CMDQ_TL_SECTRACE_UNMAP,
					NULL, CMDQ_INVALID_THREAD,
					cmdq_sec_fill_iwc_command_sectrace_unlocked,
					NULL);
		if(0 > status) {
			CMDQ_ERR("[sectrace]unmap: failed in SWd: %d\n", status);
			mc_unmap(&(gCmdqSecContextHandle->sessionHandle), va, &gCmdqSectraceMappedInfo);
			status = -EFAULT;
			break;
		}

		mcRet = mc_unmap(&(gCmdqSecContextHandle->sessionHandle), va, &gCmdqSectraceMappedInfo);

	} while(0);

	cmdq_sec_unlock_secure_path();

	CMDQ_LOG("[sectrace]unmap: status: %d\n", status);

	return status;
}

static int cmdq_sec_sectrace_transact(void)
{
	int status;

	CMDQ_LOG("[sectrace]-->transact\n");

	status = cmdq_sec_submit_to_secure_world_async_unlocked(
				CMD_CMDQ_TL_SECTRACE_TRANSACT,
				NULL, CMDQ_INVALID_THREAD,
				cmdq_sec_fill_iwc_command_sectrace_unlocked, NULL);

	CMDQ_LOG("[sectrace]<--transact: status: %d\n", status);

	return status;
}

int32_t cmdq_sec_sectrace_init(void)
{
#ifdef CMDQ_SECTRACE_SUPPORT
	int32_t status;
	const uint32_t CMDQ_SECTRACE_BUFFER_SIZE_KB = 64;
	union callback_func sectraceCB;

	/* use callback_tl_function becuase CMDQ use "TCI" for inter-world communication */
	sectraceCB.tl.map = cmdq_sec_sectrace_map;
	sectraceCB.tl.unmap = cmdq_sec_sectrace_unmap;
	sectraceCB.tl.transact = cmdq_sec_sectrace_transact;

	/* create sectrace entry in debug FS */
	status = init_sectrace("CMDQ_SEC",
		if_tci, /* use TCI for inter world communication */
		usage_tl_dr, /* print sectrace log for tl and driver */
		CMDQ_SECTRACE_BUFFER_SIZE_KB,
		&sectraceCB);

	CMDQ_LOG("cmdq_sec_trace_init, status:%d\n", status);
	return 0;
#else
	return 0;
#endif
}

int32_t cmdq_sec_sectrace_deinit(void)
{
#ifdef CMDQ_SECTRACE_SUPPORT
	/* destroy sectrace entry in debug FS */
	deinit_sectrace("CMDQ_SEC");
	return 0;
#else
	return 0;
#endif
}
#endif				/* CMDQ_SECURE_PATH_SUPPORT */

/********************************************************************************
 * common part: for general projects
 *******************************************************************************/
int32_t cmdq_sec_create_shared_memory(cmdqSecSharedMemoryHandle *pHandle, const uint32_t size)
{
	cmdqSecSharedMemoryHandle handle = NULL;
	void *pVA = NULL;
	dma_addr_t PA = 0;

	handle = kzalloc(sizeof(uint8_t *) * sizeof(cmdqSecSharedMemoryStruct), GFP_KERNEL);
	if (NULL == handle) {
		return -ENOMEM;
	}

	CMDQ_LOG("%s\n", __func__);

	/* allocate non-cachable memory */
	pVA = cmdq_core_alloc_hw_buffer(
				cmdq_dev_get(), size, &PA, GFP_KERNEL);

	CMDQ_MSG("%s, MVA:%pa, pVA:%p, size:%d\n", __func__, &PA, pVA, size);

	if(NULL == pVA) {
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

int32_t cmdq_sec_destroy_shared_memory(cmdqSecSharedMemoryHandle handle){

	if(handle && handle->pVABase) {
		cmdq_core_free_hw_buffer(
				cmdq_dev_get(), handle->size, handle->pVABase, handle->MVABase);
	}

	if(handle) {
		kfree(handle);
	}

	return 0;
}

int32_t cmdq_sec_exec_task_async_unlocked(TaskStruct *pTask, int32_t thread)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	status = cmdq_sec_submit_to_secure_world_async_unlocked(
				CMD_CMDQ_TL_SUBMIT_TASK, pTask, thread, NULL, NULL);
	if (0 > status) {
		CMDQ_ERR("%s[%d]\n", __func__, status);
	}

	return status;

#else
	CMDQ_ERR("secure path not support\n");
	return -EFAULT;
#endif
}

int32_t cmdq_sec_cancel_error_task_unlocked(TaskStruct *pTask, int32_t thread, cmdqSecCancelTaskResultStruct *pResult)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	if ((NULL == pTask) ||
		(false == cmdq_core_is_a_secure_thread(thread)) ||
		(NULL == pResult)) {

		CMDQ_ERR("%s invalid param, pTask:%p, thread:%d, pResult:%p\n",
		__func__, pTask, thread, pResult);
		return -EFAULT;
	}

	status = cmdq_sec_submit_to_secure_world_async_unlocked(CMD_CMDQ_TL_CANCEL_TASK,
		pTask, thread, NULL ,(void*)pResult);
	return status;
#else
	CMDQ_ERR("secure path not support\n");
	return -EFAULT;
#endif
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
static atomic_t gCmdqSecPathResource = ATOMIC_INIT(0);
#endif

int32_t cmdq_sec_allocate_path_resource_unlocked(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	if(1 == atomic_read(&gCmdqSecPathResource)) {
		/* has allocated successfully */
		return status;
	}

	status = cmdq_sec_submit_to_secure_world_async_unlocked(
				CMD_CMDQ_TL_PATH_RES_ALLOCATE, NULL, CMDQ_INVALID_THREAD, NULL, NULL);
	if (0 > status) {
		CMDQ_ERR("%s[%d]\n", __func__, status);
	} else {
		atomic_set(&gCmdqSecPathResource, 1);
	}

	return status;

#else
	CMDQ_ERR("secure path not support\n");
	return -EFAULT;
#endif
}

/********************************************************************************
 * common part: SecContextHandle handle
 *******************************************************************************/
cmdqSecContextHandle cmdq_sec_context_handle_create(uint32_t tgid)
{
	cmdqSecContextHandle handle = NULL;
	handle = kmalloc(sizeof(uint8_t *) * sizeof(cmdqSecContextStruct), GFP_ATOMIC);
	if (handle) {
		handle->state = IWC_INIT;
		handle->iwcMessage = NULL;

		handle->tgid = tgid;
		handle->referCount = 0;
		handle->openMobicoreByOther = 0;
	} else {
		CMDQ_ERR("SecCtxHandle_CREATE: err[LOW_MEM], tgid[%d]\n", tgid);
	}

	CMDQ_MSG("SecCtxHandle_CREATE: create new, H[0x%p], tgid[%d]\n", handle, tgid);
	return handle;
}

/********************************************************************************
 * common part: init, deinit, path
 *******************************************************************************/
void cmdq_sec_lock_secure_path(void)
{
	mutex_lock(&gCmdqSecExecLock);
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
}

void cmdqSecEnableProfile(const bool enable)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	CMDQ_LOG("[sectrace]enable profile %d\n", enable)

	mutex_lock(&gCmdqSecProfileLock);

	if (enable) {
		cmdq_sec_sectrace_init();
	} else {
		cmdq_sec_sectrace_deinit();
	}

	mutex_unlock(&gCmdqSecProfileLock);
#endif
}
