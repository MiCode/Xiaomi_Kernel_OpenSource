// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/slab.h>
#include <linux/delay.h>
#include <mt-plat/sync_write.h>

#include "cmdq_sec.h"
#include "cmdq_def.h"
#include "cmdq_virtual.h"
#include "cmdq_device.h"
#include "cmdq_record.h"
#include "cmdq_mdp_common.h"
#include <linux/of_device.h>
#include <linux/mailbox_controller.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/sched/clock.h>

#ifdef CMDQ_MET_READY
#include <linux/met_drv.h>
#endif

#define CMDQ_DRIVER_NAME		"mtk_cmdq_tee_mbox"

#define CMDQ_IRQ_MASK			GENMASK(CMDQ_THR_MAX_COUNT - 1, 0)

#define CMDQ_THR_IRQ_STATUS		0x10
#define CMDQ_LOADED_THR			0x18
#define CMDQ_THR_SLOT_CYCLES		0x30

#define CMDQ_THR_BASE			0x100
#define CMDQ_THR_SIZE			0x80
#define CMDQ_THR_ENABLE_TASK		0x04
#define CMDQ_THR_SUSPEND_TASK		0x08
#define CMDQ_CURR_IRQ_STATUS		0x10
#define CMDQ_THR_EXEC_CNT_PA		0x28

#define CMDQ_THR_ENABLED		0x1
#define CMDQ_THR_IRQ_DONE		0x1
#define CMDQ_THR_IRQ_ERROR		0x12
#define CMDQ_THR_SUSPEND		0x1
#define CMDQ_THR_RESUME			0x0
#define CMDQ_THR_DO_WARM_RESET		BIT(0)
#define CMDQ_THR_WARM_RESET		0x00
#define CMDQ_THR_ACTIVE_SLOT_CYCLES	0x3200
#define CMDQ_THR_DISABLED		0x0
#define CMDQ_REG_GET32(addr)		(readl((void *)addr) & 0xFFFFFFFF)
#define CMDQ_REG_SET32(addr, val)	mt_reg_sync_writel(val, (addr))
#define CMDQ_CMD_CNT			(CMDQ_NUM_CMD(CMDQ_CMD_BUFFER_SIZE) - 1)


/* lock to protect atomic secure task execution */
static DEFINE_MUTEX(gCmdqSecExecLock);
/* ensure atomic enable/disable secure path profile */
static DEFINE_MUTEX(gCmdqSecProfileLock);
static DEFINE_SPINLOCK(cmdq_sec_task_list_lock);

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
static struct cmdq_pkt *cmdq_sec_irq_pkt;

/* internal control */

/* Set 1 to open once for each process context, because of below reasons:
 * 1. mc_open_session is too slow (major)
 * 2. we entry secure world for config and trigger GCE, and wait in normal world
 */
#define CMDQ_OPEN_SESSION_ONCE (1)

struct cmdq_task {
	struct cmdqRecStruct	*handle;
	struct cmdq_sec_thread	*thread;
	struct work_struct	task_exec_work;
};

struct cmdq_sec_thread {
	struct mbox_chan	*chan;
	void __iomem		*base;
	phys_addr_t		gce_pa;
	u32			idx;
	u32			wait_cookie;
	u32			next_cookie;
	u32			task_cnt;
	struct cmdq_task	*task_list[CMDQ_MAX_TASK_IN_SECURE_THREAD];
	struct workqueue_struct *task_exec_wq;
	struct timer_list	timeout;
	struct work_struct	timeout_work;
	u32			timeout_ms;
	u32			priority;
	bool			occupied;
};

struct cmdq {
	struct mbox_controller	mbox;
	void __iomem		*base;
	phys_addr_t		base_pa;
	struct cmdq_sec_thread	thread[CMDQ_THR_MAX_COUNT];
	bool			suspended;
	u32			irq;
	atomic_t		usage;
	struct clk		*clock;
	struct workqueue_struct *timeout_wq;
};

/* TODO: should be removed? */
static struct cmdq *g_cmdq;

const u32 isp_iwc_buf_size[] = {
	CMDQ_SEC_ISP_CQ_SIZE,
	CMDQ_SEC_ISP_VIRT_SIZE,
	CMDQ_SEC_ISP_TILE_SIZE,
	CMDQ_SEC_ISP_BPCI_SIZE,
	CMDQ_SEC_ISP_LSCI_SIZE,
	CMDQ_SEC_ISP_LCEI_SIZE,
	CMDQ_SEC_ISP_DEPI_SIZE,
	CMDQ_SEC_ISP_DMGI_SIZE,
};

#define CMDQ_ISP_BUFS(name) \
{ \
	.va = iwc->command.name, \
	.sz = &(iwc->command.name##_size), \
}

#define CMDQ_ISP_BUFS_EX(name) \
{ \
	.va = iwc_ex->isp.name, \
	.sz = &(iwc_ex->isp.name##_size), \
}

void cmdq_sec_thread_irq_handler(struct cmdq *cmdq,
	struct cmdq_sec_thread *thread);


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
				sizeof(struct iwcCmdqMessage_t),
				&handle->iwcMessageEx,
				sizeof(struct iwcCmdqMessageEx_t));
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

	CMDQ_MSG("[SEC]<--SESSION_INIT[%d], iwcState[%d]\n", status,
		handle->state);
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
	int32_t iwcCommand, void *_pTask, int32_t thread, void *_pIwc,
	void *_iwcex)
{
	struct iwcCmdqMessage_t *pIwc;

	pIwc = (struct iwcCmdqMessage_t *) _pIwc;

	/* specify command id only, don't care other other */
	memset(pIwc, 0x0, sizeof(struct iwcCmdqMessage_t));
	pIwc->cmd = iwcCommand;

	/* medatada: debug config */
	pIwc->debug.logLevel = 1;

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

	/* MDP engines */
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, MDP_RDMA0);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, MDP_RDMA1);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, MDP_WDMA);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, MDP_WROT0);
	CMDQ_ENGINE_TRANS(engine_flags, engine_flags_sec, MDP_WROT1);
	engine_flags_sec |= cmdq_mdp_get_func()->mdpGetSecEngine(engine_flags);

	/* DISP engines */
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

static void cmdq_sec_fill_client_meta(struct cmdqRecStruct *task,
	struct iwcCmdqMessage_t *iwc, struct iwcCmdqMessageEx_t *iwc_ex)
{
	/* send iwc ex with isp meta */
	iwc->iwcMegExAvailable = true;
	iwc->metaex_type = task->sec_meta_type;
	iwc_ex->meta.size = task->sec_meta_size;

	/* copy client meta */
	memcpy((void *)iwc_ex->meta.data, task->sec_client_meta,
		task->sec_meta_size);
}

static void cmdq_sec_fill_isp_cq_meta(struct cmdqRecStruct *task,
	struct iwcCmdqMessage_t *iwc, struct iwcCmdqMessageEx_t *iwc_ex)
{
	u32 i;
	struct iwc_meta_buf {
		u32 *va;
		u32 *sz;
	} bufs[ARRAY_SIZE(task->secData.ispMeta.ispBufs)] = {
		CMDQ_ISP_BUFS_EX(isp_cq_desc),
		CMDQ_ISP_BUFS_EX(isp_cq_virt),
		CMDQ_ISP_BUFS_EX(isp_tile),
		CMDQ_ISP_BUFS_EX(isp_bpci),
		CMDQ_ISP_BUFS_EX(isp_lsci),
		CMDQ_ISP_BUFS(isp_lcei),
		CMDQ_ISP_BUFS_EX(isp_depi),
		CMDQ_ISP_BUFS_EX(isp_dmgi),
	};

	if (!task->secData.ispMeta.ispBufs[0].size ||
		!task->secData.ispMeta.ispBufs[1].size ||
		!task->secData.ispMeta.ispBufs[2].size) {
		memset(&iwc->command.isp_metadata, 0,
			sizeof(iwc->command.isp_metadata));
		for (i = 0; i < ARRAY_SIZE(bufs); i++)
			*bufs[i].sz = 0;
		iwc->iwcMegExAvailable = false;
		return;
	}

	/* send iwc ex with isp meta */
	iwc->iwcMegExAvailable = true;
	iwc->metaex_type = CMDQ_METAEX_CQ;

	if (sizeof(iwc->command.isp_metadata) !=
		sizeof(task->secData.ispMeta)) {
		CMDQ_AEE("CMDQ",
			"isp meta struct not match %zu to %zu\n",
			sizeof(iwc->command.isp_metadata),
			sizeof(task->secData.ispMeta));
		return;
	}

	/* copy isp meta */
	memcpy(&iwc->command.isp_metadata, &task->secData.ispMeta,
		sizeof(iwc->command.isp_metadata));

	for (i = 0; i < ARRAY_SIZE(task->secData.ispMeta.ispBufs); i++) {
		if (!task->secData.ispMeta.ispBufs[i].va ||
			!task->secData.ispMeta.ispBufs[i].size)
			continue;
		if (task->secData.ispMeta.ispBufs[i].size >
			isp_iwc_buf_size[i]) {
			CMDQ_ERR("isp buf %u size:%llu max:%u\n",
				i, task->secData.ispMeta.ispBufs[i].size,
				isp_iwc_buf_size[i]);
			*bufs[i].sz = 0;
			continue;
		}

		*bufs[i].sz = task->secData.ispMeta.ispBufs[i].size;
		memcpy(bufs[i].va, CMDQ_U32_PTR(
			task->secData.ispMeta.ispBufs[i].va),
			task->secData.ispMeta.ispBufs[i].size);
	}
}

s32 cmdq_sec_fill_iwc_command_msg_unlocked(
	s32 iwc_cmd, void *task_ptr, s32 thread, void *iwc_ptr,
	void *iwcex_ptr)
{
	struct cmdqRecStruct *task = (struct cmdqRecStruct *)task_ptr;
	struct iwcCmdqMessage_t *iwc = (struct iwcCmdqMessage_t *)iwc_ptr;
	struct iwcCmdqMessageEx_t *iwcex =
		(struct iwcCmdqMessageEx_t *)iwcex_ptr;
	/* cmdqSecDr will insert some instr */
	const u32 reserve_cmd_size = 4 * CMDQ_INST_SIZE;
	s32 status = 0;
	struct cmdq_pkt_buffer *buf, *last_buf;
	u32 copy_offset = 0, copy_size;
	u32 *last_inst;

	/* check task first */
	if (!task) {
		CMDQ_ERR(
			"[SEC]SESSION_MSG: Unable to fill message by empty task.\n");
		return -EFAULT;
	}

	/* check command size first */
	if ((task->pkt->cmd_buf_size + reserve_cmd_size) >
		CMDQ_TZ_CMD_BLOCK_SIZE) {
		CMDQ_ERR("[SEC]SESSION_MSG: task %p size %zu > %d\n",
			task, task->pkt->cmd_buf_size, CMDQ_TZ_CMD_BLOCK_SIZE);
		return -EFAULT;
	}

	CMDQ_MSG(
		"[SEC]-->SESSION_MSG: command id:%d size:%zu task:0x%p log:%s\n",
		iwc_cmd, task->pkt->cmd_buf_size, task,
		cmdq_core_should_secure_log() ? "true" : "false");

	/* fill message buffer for inter world communication */
	memset(iwc, 0x0, sizeof(*iwc));
	iwc->cmd = iwc_cmd;

	/* metadata */
	iwc->command.metadata.enginesNeedDAPC =
		cmdq_sec_get_secure_engine(task->secData.enginesNeedDAPC);
	iwc->command.metadata.enginesNeedPortSecurity =
		cmdq_sec_get_secure_engine(
		task->secData.enginesNeedPortSecurity);

	memset(iwcex, 0x0, sizeof(*iwcex));

	/* try general secure client meta available
	 * if not, try if cq meta available
	 */
	if (task->sec_client_meta && task->sec_meta_size)
		cmdq_sec_fill_client_meta(task, iwc, iwcex);
	else
		cmdq_sec_fill_isp_cq_meta(task, iwc, iwcex);

	if (thread == CMDQ_INVALID_THREAD) {
		/* relase resource, or debug function will go here */
		iwc->command.commandSize = 0;
		iwc->command.metadata.addrListLength = 0;

		CMDQ_MSG("[SEC]<--SESSION_MSG: no task cmdId:%d\n", iwc_cmd);
		return 0;
	}

	/* basic data */
	iwc->command.scenario = task->scenario;
	iwc->command.thread = thread;
	iwc->command.priority = task->pkt->priority;
	iwc->command.engineFlag = cmdq_sec_get_secure_engine(task->engineFlag);
	iwc->command.hNormalTask = 0LL | ((unsigned long)task);

	/* assign extension and read back parameter */
	iwc->command.extension = task->secData.extension;
	iwc->command.readback_pa = task->reg_values_pa;

	last_buf = list_last_entry(&task->pkt->buf, typeof(*last_buf),
		list_entry);
	list_for_each_entry(buf, &task->pkt->buf, list_entry) {
		u32 avail_buf_size = task->pkt->avail_buf_size;

		copy_size = (buf == last_buf) ?
			CMDQ_CMD_BUFFER_SIZE - avail_buf_size :
			CMDQ_CMD_BUFFER_SIZE;

		memcpy(iwc->command.pVABase + copy_offset,
			buf->va_base, copy_size);

		/* commandSize is command total size in byte */
		iwc->command.commandSize += copy_size;
		copy_offset += copy_size / 4;

		if (buf != last_buf) {
			last_inst = iwc->command.pVABase + copy_offset;
			last_inst[-1] = CMDQ_CODE_JUMP << 24;
			last_inst[-2] = CMDQ_REG_SHIFT_ADDR(CMDQ_INST_SIZE);
		}
	}

	/* do not gen irq */
	last_inst = &iwc->command.pVABase[iwc->command.commandSize / 4 - 4];
	if (last_inst[0] == 0x1 && last_inst[1] == 0x40000000)
		last_inst[0] = 0;
	else
		CMDQ_ERR("fail to find eoc with 0x%08x%08x\n",
			last_inst[1], last_inst[0]);

	/* cookie */
	iwc->command.waitCookie = task->secData.waitCookie;
	iwc->command.resetExecCnt = task->secData.resetExecCnt;

	CMDQ_MSG(
		"[SEC]SESSION_MSG: task:0x%p thread:%d size:%zu flag:0x%016llx size:%zu\n",
		task, thread, task->pkt->cmd_buf_size, task->engineFlag,
		sizeof(iwc->command));

	CMDQ_VERBOSE("[SEC]SESSION_MSG: addrList[%d][0x%p]\n",
		task->secData.addrMetadataCount,
		CMDQ_U32_PTR(task->secData.addrMetadatas));
	if (task->secData.addrMetadataCount > 0) {
		struct iwcCmdqAddrMetadata_t *addr;
		u8 i;

		iwc->command.metadata.addrListLength =
			task->secData.addrMetadataCount;
		memcpy((iwc->command.metadata.addrList),
			CMDQ_U32_PTR(task->secData.addrMetadatas),
			task->secData.addrMetadataCount *
			sizeof(struct cmdqSecAddrMetadataStruct));

		/* command copy from user space may insert jump,
		 * thus adjust index of handle list
		 */
		addr = iwc->command.metadata.addrList;
		for (i = 0; i < ARRAY_SIZE(iwc->command.metadata.addrList);
			i++) {
			addr[i].instrIndex = addr[i].instrIndex +
				addr[i].instrIndex / CMDQ_CMD_CNT;
		}
	}

	/* medatada: debug config */
	iwc->debug.logLevel = (cmdq_core_should_secure_log()) ? (1) : (0);

	CMDQ_MSG("[SEC]<--SESSION_MSG status:%d\n", status);
	return status;
}

/* TODO: when do release secure command buffer */
int32_t cmdq_sec_fill_iwc_resource_msg_unlocked(
	int32_t iwcCommand, void *_pTask, int32_t thread, void *_pIwc,
	void *_iwcex)
{
	struct iwcCmdqMessage_t *pIwc;
	struct cmdqSecSharedMemoryStruct *pSharedMem;

	pSharedMem = cmdq_core_get_secure_shared_memory();
	if (!pSharedMem) {
		CMDQ_ERR("FILL:RES, NULL shared memory\n");
		return -EFAULT;
	}

	if (pSharedMem && !pSharedMem->pVABase) {
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

	return 0;
}

int32_t cmdq_sec_fill_iwc_cancel_msg_unlocked(
	int32_t iwcCommand, void *_pTask, int32_t thread, void *_pIwc,
	void *_iwcex)
{
	const struct cmdqRecStruct *pTask = (struct cmdqRecStruct *) _pTask;
	struct iwcCmdqMessage_t *pIwc;

	pIwc = (struct iwcCmdqMessage_t *) _pIwc;
	memset(pIwc, 0x0, sizeof(struct iwcCmdqMessage_t));

	pIwc->cmd = iwcCommand;
	pIwc->cancelTask.waitCookie = pTask->secData.waitCookie;
	pIwc->cancelTask.thread = thread;

	/* medatada: debug config */
	pIwc->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) : (0);

	CMDQ_LOG("FILL:CANCEL_TASK: task:0x%p thread:%d cookie:%d\n",
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

void cmdq_sec_handle_attach_status(struct cmdqRecStruct *pTask,
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
	struct cmdqRecStruct *pTask, void *data)
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
			"CANCEL_TASK: task:0x%p INST:(0x%08x, 0x%08x), throwAEE:%d hasReset:%d pc:0x%08x\n",
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
#if 0
		if (pTask && iwcRsp < 0)
			pTask->state = TASK_STATE_ERROR;
#endif
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
	struct cmdqRecStruct *task, s32 thread, void *data)
{
	s32 status;
	const s32 timeout_ms = 3 * 1000;
	struct iwcCmdqMessage_t *iwc;

	const CmdqSecFillIwcCB fill_iwc_cb =
		cmdq_sec_get_iwc_msg_fill_cb_by_iwc_command(iwc_command);

	CMDQ_MSG("[SEC]%s cmd:%d task:0x%p state:%d\n",
		__func__, iwc_command, task, handle->state);

	do {
		/* fill message bufer */
		status = fill_iwc_cb(iwc_command, task, thread,
			(void *)(handle->iwcMessage),
			(void *)(handle->iwcMessageEx));

		if (status) {
			CMDQ_ERR(
				"[SEC]fill msg buffer failed:%d pid:%d:%d cmdId:%d\n",
				 status, current->tgid, current->pid,
				 iwc_command);
			break;
		}

		iwc = (struct iwcCmdqMessage_t *)handle->iwcMessage;


		/* send message */
		status = cmdq_sec_execute_session(&handle->tee, iwc_command,
			timeout_ms, iwc->iwcMegExAvailable);
		if (status) {
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

	if (status < 0)
		CMDQ_ERR(
			"[SEC]%s leave cmd:%d task:0x%p state:%d status:%d\n",
			__func__, iwc_command, task, handle->state, status);

	return status;
}

void cmdq_sec_track_task_record(const uint32_t iwcCommand,
	struct cmdqRecStruct *pTask, CMDQ_TIME *pEntrySec, CMDQ_TIME *pExitSec)
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

static void cmdq_core_dump_secure_metadata(struct cmdqRecStruct *task)
{
	uint32_t i = 0;
	struct cmdqSecAddrMetadataStruct *pAddr = NULL;
	struct cmdqSecDataStruct *pSecData = &task->secData;

	if (!pSecData)
		return;

	pAddr = (struct cmdqSecAddrMetadataStruct *)(
		CMDQ_U32_PTR(pSecData->addrMetadatas));

	CMDQ_LOG("========= pSecData:0x%p dump =========\n", pSecData);
	CMDQ_LOG(
		"count:%d(%d), enginesNeedDAPC:0x%llx, enginesPortSecurity:0x%llx\n",
		pSecData->addrMetadataCount, pSecData->addrMetadataMaxCount,
		pSecData->enginesNeedDAPC, pSecData->enginesNeedPortSecurity);

	if (!pAddr)
		return;

	for (i = 0; i < pSecData->addrMetadataCount; i++) {
		CMDQ_LOG(
			"idx:%u %u type:%d baseHandle:0x%016llx blockOffset:%u offset:%u size:%u port:%u\n",
			i, pAddr[i].instrIndex, pAddr[i].type,
			(u64)pAddr[i].baseHandle, pAddr[i].blockOffset,
			pAddr[i].offset, pAddr[i].size, pAddr[i].port);
	}

	CMDQ_ERR("dapc:0x%llx sec port:0x%llx\n",
		task->secData.enginesNeedDAPC,
		task->secData.enginesNeedPortSecurity);
}

void cmdq_sec_dump_secure_thread_cookie(s32 thread)
{
	if (!cmdq_get_func()->isSecureThread(thread))
		return;

	CMDQ_LOG("secure shared cookie:%u wait:%u next:%u count:%u\n",
		cmdq_sec_get_secure_thread_exec_counter(thread),
		g_cmdq->thread[thread].wait_cookie,
		g_cmdq->thread[thread].next_cookie,
		g_cmdq->thread[thread].task_cnt);
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

static void cmdq_sec_handle_irq_notify(struct cmdq *cmdq,
	struct cmdq_sec_thread *thread)
{
	u32 i;
	u32 irq_flag;

	for (i = CMDQ_MIN_SECURE_THREAD_ID; i < CMDQ_MIN_SECURE_THREAD_ID +
		CMDQ_MAX_SECURE_THREAD_COUNT; i++)
		cmdq_sec_thread_irq_handler(cmdq, &cmdq->thread[i]);

	if (thread) {
		irq_flag = readl(thread->base + CMDQ_THR_IRQ_STATUS);
		writel(~irq_flag, thread->base + CMDQ_THR_IRQ_STATUS);
	}
}

static void cmdq_sec_irq_notify_callback(struct cmdq_cb_data data)
{
	struct cmdq *cmdq = (struct cmdq *)data.data;

	CMDQ_VERBOSE("secure irq err:%d\n", data.err);

	cmdq_sec_handle_irq_notify(cmdq, NULL);
}

static void cmdq_sec_irq_notify_start(void)
{
	struct cmdq_client *clt;
	s32 err;

	/* must lock gCmdqSecExecLock before call,
	 * since it start when sec task begin
	 * and end when task list empty.
	 */

	if (cmdq_sec_irq_pkt)
		return;

	clt = cmdq_helper_mbox_client(CMDQ_SEC_IRQ_THREAD);
	if (!clt) {
		CMDQ_ERR("no irq thread client\n");
		return;
	}

	cmdq_sec_irq_pkt = cmdq_pkt_create(clt);
	cmdq_pkt_wfe(cmdq_sec_irq_pkt, CMDQ_SYNC_SECURE_THR_EOF);
	cmdq_pkt_finalize_loop(cmdq_sec_irq_pkt);

	cmdqCoreClearEvent(CMDQ_SYNC_SECURE_THR_EOF);

	if (cmdq_sec_irq_pkt->cl != clt) {
		CMDQ_LOG(
			"[warn]client not match before flush sec irq pkt:%p and %p\n",
			cmdq_sec_irq_pkt->cl, clt);
		cmdq_sec_irq_pkt->cl = clt;
	}
	err = cmdq_pkt_flush_async(cmdq_sec_irq_pkt,
		cmdq_sec_irq_notify_callback, (void *)g_cmdq);
	if (err < 0) {
		CMDQ_ERR("fail to start irq thread err:%d\n", err);
		cmdq_mbox_stop(clt);
		cmdq_pkt_destroy(cmdq_sec_irq_pkt);
		cmdq_sec_irq_pkt = NULL;
	}

	CMDQ_MSG("irq notify started pkt:0x%p\n", cmdq_sec_irq_pkt);
}

static void cmdq_sec_irq_notify_stop(void)
{
	struct cmdq_client *clt;

	if (!cmdq_sec_irq_pkt)
		return;

	clt = cmdq_helper_mbox_client(CMDQ_SEC_IRQ_THREAD);
	if (!clt) {
		CMDQ_ERR("no irq thread client\n");
		return;
	}

	cmdq_mbox_stop(clt);
	cmdq_pkt_destroy(cmdq_sec_irq_pkt);
	cmdq_sec_irq_pkt = NULL;
}

int32_t cmdq_sec_submit_to_secure_world_async_unlocked(uint32_t iwcCommand,
	struct cmdqRecStruct *pTask, int32_t thread, void *data, bool throwAEE)
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
		if (!gCmdqSecContextHandle)
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

		/* always check and lunch irq notify loop thread */
		if (pTask)
			cmdq_sec_irq_notify_start();

		if (cmdq_sec_setup_context_session(handle) < 0) {
			status = -(CMDQ_ERR_SEC_CTX_SETUP);
			CMDQ_ERR(
				"[SEC] cmdq_sec_setup_context_session failed, status[%d]\n",
				status);
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
			cmdq_core_dump_secure_metadata(pTask);
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

/* empty function to compatible with mailbox */
s32 cmdq_sec_insert_backup_cookie(struct cmdq_pkt *pkt)
{
	return 0;
}

s32 cmdq_sec_insert_backup_cookie_instr(struct cmdqRecStruct *task, s32 thread)
{
	struct cmdq_client *cl = cmdq_helper_mbox_client(thread);
	struct cmdq_sec_thread *sec_thread = (
		(struct mbox_chan *)cl->chan)->con_priv;
	const u32 regAddr = (u32)(sec_thread->gce_pa + CMDQ_THR_BASE +
		CMDQ_THR_SIZE * thread + CMDQ_THR_EXEC_CNT_PA);
	struct ContextStruct *context = cmdq_core_get_context();
	u64 addrCookieOffset = CMDQ_SEC_SHARED_THR_CNT_OFFSET + thread *
		sizeof(u32);
	u64 WSMCookieAddr;
	s32 err;
	struct cmdq_operand left;
	struct cmdq_operand right;

	if (!context->hSecSharedMem) {
		CMDQ_ERR("%s shared memory is not created\n", __func__);
		return -EFAULT;
	}

	CMDQ_VERBOSE("backup secure cookie for thread:%d task:0x%p\n",
		thread, task);

	err = cmdq_pkt_read(task->pkt, cmdq_helper_mbox_base(), regAddr,
		CMDQ_THR_SPR_IDX1);
	if (err != 0) {
		CMDQ_ERR("fail to read pkt:%p reg:%#x err:%d\n",
			task->pkt, regAddr, err);
		return err;
	}

	left.reg = true;
	left.idx = CMDQ_THR_SPR_IDX1;
	right.reg = false;
	right.value = 1;

	cmdq_pkt_logic_command(task->pkt, CMDQ_LOGIC_ADD, CMDQ_THR_SPR_IDX1,
		&left, &right);

	WSMCookieAddr = context->hSecSharedMem->MVABase + addrCookieOffset;
	err = cmdq_pkt_write_indriect(task->pkt, cmdq_helper_mbox_base(),
		WSMCookieAddr, CMDQ_THR_SPR_IDX1, ~0);
	if (err < 0) {
		CMDQ_ERR("fail to write pkt:%p wsm:%#llx err:%d\n",
			task->pkt, WSMCookieAddr, err);
		return err;
	}

	/* trigger notify thread so that normal world start handling
	 * with new backup cookie
	 */
	cmdq_pkt_set_event(task->pkt, CMDQ_SYNC_SECURE_THR_EOF);

	return 0;
}

static s32 cmdq_sec_remove_handle_from_thread_by_cookie(
	struct cmdq_sec_thread *thread, s32 index)
{
	struct cmdq_task *task;

	if (!thread || index < 0 || index >= CMDQ_MAX_TASK_IN_SECURE_THREAD) {
		CMDQ_ERR(
			"remove task from thread array, invalid param THR:0x%p task_slot:%d\n",
			thread, index);
		return -EINVAL;
	}

	task = thread->task_list[index];

	if (!task) {
		CMDQ_ERR("%s task:0x%p is invalid\n", __func__, task);
		return -EINVAL;
	}

	CMDQ_VERBOSE("remove task slot:%d\n", index);

	thread->task_list[index] = NULL;
	thread->task_cnt--;

	if (thread->task_cnt < 0) {
		/* Error status print */
		CMDQ_ERR("%s end taskCount < 0\n", __func__);
	}

	CMDQ_MSG("%s leave\n", __func__);

	return 0;
}

static void cmdq_sec_task_callback(struct cmdq_pkt *pkt, s32 err)
{
	struct cmdq_cb_data cmdq_cb_data;

	if (pkt->cb.cb) {
		cmdq_cb_data.err = err;
		cmdq_cb_data.data = pkt->cb.data;
		pkt->cb.cb(cmdq_cb_data);
	}
}

static void cmdq_sec_release_task(struct cmdq_task *task)
{
	if (unlikely(!task))
		CMDQ_ERR("%s task:0x%p invalid\n", __func__, task);

	kfree(task);
}

s32 cmdq_sec_handle_wait_result_impl(struct cmdqRecStruct *handle, s32 thread,
	bool throw_aee)
{
	s32 i;
	s32 status = 0;
	struct cmdq_sec_thread *thread_context;
	/* error report */
	const char *module = NULL;
	s32 irq_flag = 0;
	struct cmdqSecCancelTaskResultStruct result;
	char parsed_inst[128] = { 0 };
	unsigned long flags;
	bool task_empty;

	/* Init default status */
	status = 0;
	memset(&result, 0, sizeof(result));
	/* TODO: revise later... */
	thread_context = &g_cmdq->thread[thread];

	CMDQ_MSG("%s handle:0x%p pkt:0x%p thread:%d\n",
		__func__, handle, handle->pkt, thread);

	/* lock cmdqSecLock */
	cmdq_sec_lock_secure_path();

	do {
		/* check if this task has finished */
		if (handle->state == TASK_STATE_DONE ||
			handle->state == TASK_STATE_KILLED)
			break;
		else if (handle->state == TASK_STATE_ERR_IRQ)
			status = -EINVAL;
		else if (handle->state == TASK_STATE_TIMEOUT)
			status = -ETIMEDOUT;

		/* Oops, tha tasks is not done.
		 * We have several possible error scenario:
		 * 1. task still running (hang / timeout)
		 * 2. IRQ pending (done or error/timeout IRQ)
		 * 3. task's SW thread has been signaled (e.g. SIGKILL)
		 */

		/* dump shared cookie */
		CMDQ_LOG(
			"WAIT: [1]secure path failed task:0x%p thread:%d shared_cookie(%d, %d, %d)\n",
			handle, thread,
			cmdq_sec_get_secure_thread_exec_counter(
				CMDQ_MIN_SECURE_THREAD_ID),
			cmdq_sec_get_secure_thread_exec_counter(
				CMDQ_MIN_SECURE_THREAD_ID + 1),
			cmdq_sec_get_secure_thread_exec_counter(
				CMDQ_MIN_SECURE_THREAD_ID + 2));

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
		cmdq_sec_cancel_error_task_unlocked(handle, thread, &result);
		/* shall we pass the error instru back from secure path?? */
		module = cmdq_get_func()->parseErrorModule(handle);

		/* module dump */
		cmdq_core_attach_error_handle(handle, handle->thread);

		spin_lock_irqsave(&cmdq_sec_task_list_lock, flags);

		/* remove all tasks in tread since we have reset HW thread
		 * in SWd
		 */
		for (i = 0; i < CMDQ_MAX_TASK_IN_SECURE_THREAD; i++) {
			struct cmdq_task *task = thread_context->task_list[i];

			if (!task)
				continue;

			cmdq_sec_remove_handle_from_thread_by_cookie(
				thread_context, i);
			cmdq_sec_task_callback(task->handle->pkt,
				-ECONNABORTED);
			cmdq_sec_release_task(task);
		}

		thread_context->task_cnt = 0;
		thread_context->wait_cookie = thread_context->next_cookie;

		spin_unlock_irqrestore(&cmdq_sec_task_list_lock, flags);
	} while (0);

	spin_lock_irqsave(&cmdq_sec_task_list_lock, flags);
	task_empty = true;
	for (i = CMDQ_MIN_SECURE_THREAD_ID;
		i < CMDQ_MIN_SECURE_THREAD_ID + CMDQ_MAX_SECURE_THREAD_COUNT;
		i++) {
		if (g_cmdq->thread[i].task_cnt > 0) {
			task_empty = false;
			break;
		}
	}
	spin_unlock_irqrestore(&cmdq_sec_task_list_lock, flags);

	if (task_empty)
		cmdq_sec_irq_notify_stop();

	/* unlock cmdqSecLock */
	cmdq_sec_unlock_secure_path();

	/* throw AEE if nessary */
	if (throw_aee && status) {
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

s32 cmdq_sec_handle_wait_result(struct cmdqRecStruct *handle, s32 thread)
{
	return cmdq_sec_handle_wait_result_impl(handle, thread, true);
}


static s32 cmdq_sec_get_thread_id(s32 scenario)
{
	return cmdq_get_func()->getThreadID(scenario, true);
}

/* core controller for secure function */
static const struct cmdq_controller g_cmdq_sec_ctrl = {
	.handle_wait_result = cmdq_sec_handle_wait_result,
	.get_thread_id = cmdq_sec_get_thread_id,
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
	if (!handle)
		return -ENOMEM;

	CMDQ_LOG("%s\n", __func__);

	/* allocate non-cachable memory */
	pVA = cmdq_core_alloc_hw_buffer(cmdq_dev_get(), size, &PA,
		GFP_KERNEL);

	CMDQ_MSG("%s, MVA:%pa, pVA:0x%p size:%d\n", __func__, &PA, pVA, size);

	if (!pVA) {
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
	struct cmdqRecStruct *handle, int32_t thread)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	CMDQ_MSG("%s handle:0x%p thread:%d\n", __func__, handle, thread);
	status = cmdq_sec_submit_to_secure_world_async_unlocked(
		CMD_CMDQ_TL_SUBMIT_TASK, handle, thread, NULL, true);
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
	struct cmdqRecStruct *pTask, int32_t thread,
	struct cmdqSecCancelTaskResultStruct *pResult)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	if (!pTask || !cmdq_get_func()->isSecureThread(thread) || !pResult) {

		CMDQ_ERR("%s invalid param, pTask:0x%p thread:%d pResult:%p\n",
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

	CMDQ_MSG("[SEC]%s throwAEE: %s",
		__func__, throwAEE ? "true" : "false");

	if (atomic_cmpxchg(&gCmdqSecPathResource, 0, 1) != 0) {
		/* has allocated successfully */
		return status;
	}

	status = cmdq_sec_submit_to_secure_world_async_unlocked(
		CMD_CMDQ_TL_PATH_RES_ALLOCATE, NULL,
		CMDQ_INVALID_THREAD, NULL, throwAEE);
	if (status) {
		/* Error status print */
		CMDQ_ERR("%s[%d] reset context\n", __func__, status);

		/* in fail case, we want function alloc again */
		atomic_set(&gCmdqSecPathResource, 0);
	}

	if (status)
		CMDQ_ERR("[SEC]%s leave status:%d\n", __func__, status);
	else
		CMDQ_MSG("[SEC]%s leave status:%d\n", __func__, status);

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

static int cmdq_suspend(struct device *dev)
{
	return 0;
}

static int cmdq_resume(struct device *dev)
{
	struct cmdq *cmdq = dev_get_drvdata(dev);

	WARN_ON(clk_prepare(cmdq->clock) < 0);
	cmdq->suspended = false;
	return 0;
}

static int cmdq_remove(struct platform_device *pdev)
{
	struct cmdq *cmdq = platform_get_drvdata(pdev);

	mbox_controller_unregister(&cmdq->mbox);
	clk_unprepare(cmdq->clock);
	return 0;
}

static s32 cmdq_sec_insert_handle_from_thread_array_by_cookie(
	struct cmdq_task *task, struct cmdq_sec_thread *thread,
	const s32 cookie, const bool reset_thread)
{
	if (!task || !thread) {
		CMDQ_ERR(
			"invalid param pTask:0x%p pThread:0x%p cookie:%d needReset:%d\n",
			task, thread, cookie, reset_thread);
		return -EFAULT;
	}

	if (reset_thread == true) {
		s32 offset;
		void *va_base;

		thread->wait_cookie = cookie;
		thread->next_cookie = cookie + 1;
		if (thread->next_cookie > CMDQ_MAX_COOKIE_VALUE) {
			/* Reach the maximum cookie */
			thread->next_cookie = 0;
		}

		/* taskCount must start from 0. */
		/* and we are the first task, so set to 1. */
		thread->task_cnt = 1;

		/* reset wsm to clear remain cookie */
		offset = CMDQ_SEC_SHARED_THR_CNT_OFFSET +
				thread->idx * sizeof(s32);
		va_base = cmdq_core_get_context()->hSecSharedMem->pVABase;
		CMDQ_REG_SET32(va_base + offset, 0);
	} else {
		thread->next_cookie += 1;
		if (thread->next_cookie > CMDQ_MAX_COOKIE_VALUE) {
			/* Reach the maximum cookie */
			thread->next_cookie = 0;
		}

		thread->task_cnt++;
	}

	thread->task_list[cookie % CMDQ_MAX_TASK_IN_SECURE_THREAD] = task;
	task->handle->secData.waitCookie = cookie;
	task->handle->secData.resetExecCnt = reset_thread;

	CMDQ_MSG("%s leave task:0x%p handle:0x%p insert with idx:%d\n",
		__func__, task, task->handle,
		cookie % CMDQ_MAX_TASK_IN_SECURE_THREAD);

	return 0;
}

static void cmdq_sec_exec_task_async_impl(struct work_struct *work_item)
{
	s32 status;
	char long_msg[CMDQ_LONGSTRING_MAX];
	u32 msg_offset;
	s32 msg_max_size;
	u32 thread_id;
	struct cmdq_pkt_buffer *buf;
	struct cmdq_task *task = container_of(work_item, struct cmdq_task,
		task_exec_work);
	struct cmdqRecStruct *handle = task->handle;
	struct cmdq_sec_thread *thread = task->thread;
	u32 cookie;
	unsigned long flags;

	thread_id = thread->idx;
	buf = list_first_entry(&handle->pkt->buf, struct cmdq_pkt_buffer,
		list_entry);

	cmdq_long_string_init(false, long_msg, &msg_offset, &msg_max_size);

	cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
		"-->EXEC: pkt:0x%p on thread:%d begin va:0x%p\n",
		handle->pkt, thread_id, buf->va_base);
	cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
		" command size:%zu bufferSize:%zu scenario:%d flag:0x%llx\n",
		handle->pkt->cmd_buf_size, handle->pkt->buf_size,
		handle->scenario, handle->engineFlag);
	CMDQ_MSG("%s", long_msg);

	if (!handle->secData.is_secure)
		CMDQ_ERR("not secure %s", long_msg);

	cmdq_sec_lock_secure_path();
	do {
		/* insert handle to task list, and */
		/* delay HW config when entry SWd */
		spin_lock_irqsave(&cmdq_sec_task_list_lock, flags);

		if (thread->task_cnt <= 0) {
			/* TODO: enable clock */
			cookie = 1;
			cmdq_sec_insert_handle_from_thread_array_by_cookie(
				task, thread, cookie, true);

			mod_timer(&thread->timeout,
				jiffies + msecs_to_jiffies(thread->timeout_ms));
		} else {
			/* append directly */
			cookie = thread->next_cookie;
			cmdq_sec_insert_handle_from_thread_array_by_cookie(
				task, thread, cookie, false);
		}

		spin_unlock_irqrestore(&cmdq_sec_task_list_lock, flags);

		handle->state = TASK_STATE_BUSY;
		handle->trigger = sched_clock();

		/* setup whole patah */
		status = cmdq_sec_allocate_path_resource_unlocked(true);
		if (status) {
			CMDQ_ERR("[SEC]%s failed status:%d\n",
				__func__, status);
			break;
		}

		/* execute */
		status = cmdq_sec_exec_task_async_unlocked(handle, thread_id);
		if (status) {
			u32 cookie = thread->next_cookie;

			CMDQ_ERR("[SEC]%s failed status:%d handle state:%d\n",
				__func__, status, handle->state);

			/* config failed case, dump for more detail */
			cmdq_core_attach_error_handle(handle, thread_id);
			cmdq_core_turnoff_first_dump();

			spin_lock_irqsave(&cmdq_sec_task_list_lock, flags);
			cmdq_sec_remove_handle_from_thread_by_cookie(
				thread, cookie);

			spin_unlock_irqrestore(&cmdq_sec_task_list_lock, flags);
			handle->state = TASK_STATE_ERROR;

		}

	} while (0);
	cmdq_sec_unlock_secure_path();

	if (status < 0)
		CMDQ_ERR("[SEC]%s leave with error status:%d\n",
			__func__, status);

	CMDQ_MSG("<--EXEC: done\n");
}

static s32 cmdq_sec_exec_task_async_work(struct cmdqRecStruct *handle,
	struct cmdq_sec_thread *thread)
{
	struct cmdq_task *task;

	/* TODO: check suspend? */
#if 0
	struct cmdq *cmdq;

	cmdq = dev_get_drvdata(thread->chan->mbox->dev);
	/* not allow to send task when cmdq is suspend */
	WARN_ON(cmdq->suspended);
#endif
	CMDQ_MSG("[SEC]%s handle:0x%p pkt:0x%p thread:%d\n",
		__func__, handle, handle->pkt, thread->idx);

	task = kzalloc(sizeof(*task), GFP_ATOMIC);
	if (unlikely(!task)) {
		CMDQ_ERR("unable to allocate task\n");
		return -ENOMEM;
	}

	/* update task's thread info */
	handle->thread = thread->idx;
	task->handle = handle;
	task->thread = thread;

	INIT_WORK(&task->task_exec_work, cmdq_sec_exec_task_async_impl);
	queue_work(thread->task_exec_wq, &task->task_exec_work);

	CMDQ_MSG("[SEC]%s leave handle:0x%p pkt:0x%p thread:%d\n",
		__func__, handle, handle->pkt, thread->idx);

	return 0;
}

static int cmdq_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data;
	s32 status;

	status = cmdq_sec_exec_task_async_work(pkt->user_data, chan->con_priv);
	if (status)
		CMDQ_ERR(
			"[SEC]%s leave chan:0x%p pkt:0x%p handle:0x%p status:%d\n",
			__func__, chan, pkt, pkt->user_data, status);
	return 0;
}

static bool cmdq_sec_thread_timeout_excceed(struct cmdq_sec_thread *thread)
{
	struct cmdq_task *task = NULL;
	struct cmdqRecStruct *handle;
	u64 duration, now, timeout;
	s32 i, last_idx;
	CMDQ_TIME last_trigger = 0;

	for (i = CMDQ_MAX_TASK_IN_SECURE_THREAD - 1; i >= 0; i--) {
		/* task put in array from index 1 */
		if (!thread->task_list[i])
			continue;
		if (thread->task_list[i]->handle->trigger > last_trigger &&
			last_trigger)
			break;

		last_idx = i;
		task = thread->task_list[i];
		last_trigger = thread->task_list[i]->handle->trigger;
	}

	if (!task) {
		CMDQ_MSG("timeout excceed no timeout task in list\n");
		return true;
	}

	handle = task->handle;
	now = sched_clock();
	timeout = thread->timeout_ms;
	duration = div_s64(now - handle->trigger, 1000000);
	if (duration < timeout) {
		mod_timer(&thread->timeout, jiffies +
			msecs_to_jiffies(timeout - duration));
		CMDQ_MSG(
			"timeout excceed ignore handle:0x%p pkt:0x%p trigger:%llu\n",
			handle, handle->pkt, handle->trigger);
		return false;
	}

	return true;
}

static bool cmdq_sec_task_list_empty(struct cmdq_sec_thread *thread)
{
	u32 i;

	for (i = 0; i < CMDQ_MAX_TASK_IN_SECURE_THREAD; i++) {
		if (thread->task_list[i])
			return false;
	}

	return true;
}

static void cmdq_sec_task_err_callback(struct cmdq_pkt *pkt, s32 err)
{
	struct cmdq_cb_data cmdq_cb_data;

	if (pkt->err_cb.cb) {
		cmdq_cb_data.err = err;
		cmdq_cb_data.data = pkt->cb.data;
		pkt->err_cb.cb(cmdq_cb_data);
	}
}

static void cmdq_sec_thread_irq_handle_by_cookie(
	struct cmdq_sec_thread *thread, s32 err, u32 cookie)
{
	u32 task_done_cnt, i;
	unsigned long flags;
	u32 max_task_cnt = CMDQ_MAX_TASK_IN_SECURE_THREAD;
	struct cmdq_task *task;

	spin_lock_irqsave(&cmdq_sec_task_list_lock, flags);

	if (thread->wait_cookie <= cookie) {
		task_done_cnt = cookie - thread->wait_cookie + 1;
	} else if ((cookie + 1) % CMDQ_MAX_COOKIE_VALUE ==
		thread->wait_cookie) {
		task_done_cnt = 0;
		CMDQ_MSG("IRQ: duplicated cookie: waitCookie:%d hwCookie:%d",
			thread->wait_cookie, cookie);
	} else {
		/* Counter wrapped */
		task_done_cnt =
			(CMDQ_MAX_COOKIE_VALUE - thread->wait_cookie + 1) +
			(cookie + 1);
		CMDQ_ERR(
			"IRQ: counter wrapped: waitCookie:%d hwCookie:%d count:%d",
			thread->wait_cookie, cookie, task_done_cnt);
	}

	for (i = (thread->wait_cookie % max_task_cnt); task_done_cnt > 0;
		task_done_cnt--, i++) {
		s32 tmp_err;

		if (i >= max_task_cnt)
			i = 0;

		if (!thread->task_list[i])
			continue;

		task = thread->task_list[i];
		tmp_err = task_done_cnt == 1 ? err : 0;
		cmdq_sec_remove_handle_from_thread_by_cookie(task->thread, i);
		/*
		 * check secure world work is finished or not
		 * since secure world has lower performance
		 */
		cmdq_sec_task_callback(task->handle->pkt, tmp_err);
		cmdq_sec_release_task(task);
	}

	if (err) {
		/* remove all task */
		struct cmdq_task *tmp;

		for (i = 0; i < CMDQ_MAX_TASK_IN_SECURE_THREAD; i++) {
			if (!thread->task_list[i])
				continue;

			tmp = thread->task_list[i];
			cmdq_sec_remove_handle_from_thread_by_cookie(
					thread->task_list[i]->thread, i);
			cmdq_sec_task_callback(tmp->handle->pkt,
				-ECONNABORTED);
			cmdq_sec_release_task(tmp);
		}
		spin_unlock_irqrestore(&cmdq_sec_task_list_lock, flags);
		return;
	}

	if (cmdq_sec_task_list_empty(thread)) {
		u32 *va = cmdq_core_get_context()->hSecSharedMem->pVABase +
			CMDQ_SEC_SHARED_THR_CNT_OFFSET +
			thread->idx * sizeof(s32);

		/* clear task count, wait cookie and current cookie
		 * to avoid process again
		 */
		thread->wait_cookie = 0;
		thread->task_cnt = 0;
		*va = 0;

		spin_unlock_irqrestore(&cmdq_sec_task_list_lock, flags);
		return;
	}

	thread->wait_cookie = cookie + 1;
	if (thread->wait_cookie > CMDQ_MAX_COOKIE_VALUE) {
		/* min cookie value is 0 */
		thread->wait_cookie -= (CMDQ_MAX_COOKIE_VALUE + 1);
	}
	task = thread->task_list[thread->wait_cookie %
		CMDQ_MAX_TASK_IN_SECURE_THREAD];

	if (task) {
		mod_timer(&thread->timeout,
			jiffies + msecs_to_jiffies(thread->timeout_ms));
	} else {
		u32 i;

		CMDQ_ERR("%s task is empty, wait cookie:%d dump for sure\n",
			__func__, thread->wait_cookie);
		for (i = 0; i < CMDQ_MAX_TASK_IN_SECURE_THREAD; i++) {
			task = thread->task_list[i];
			if (task)
				CMDQ_LOG(
					"task_list[%d]:0x%p handle:0x%p pkt:0x%p\n",
					i, task, task->handle,
					task->handle->pkt);
			else
				CMDQ_LOG("task_list[%d]]:0x%p\n", i, task);
		}

	}

	spin_unlock_irqrestore(&cmdq_sec_task_list_lock, flags);
}

static void cmdq_sec_thread_handle_timeout(unsigned long data)
{
	struct cmdq_sec_thread *thread = (struct cmdq_sec_thread *)data;
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq, mbox);

	if (!work_pending(&thread->timeout_work))
		queue_work(cmdq->timeout_wq, &thread->timeout_work);
}

static void cmdq_sec_task_timeout_work(struct work_struct *work_item)
{
	struct cmdq_sec_thread *thread = container_of(work_item,
		struct cmdq_sec_thread, timeout_work);
	unsigned long flags;
	struct cmdq_task *timeout_task;
	u32 cookie, wait_cookie;

	spin_lock_irqsave(&cmdq_sec_task_list_lock, flags);

	if (cmdq_sec_task_list_empty(thread)) {
		CMDQ_MSG("thd:%d is empty\n", thread->idx);
		spin_unlock_irqrestore(&cmdq_sec_task_list_lock, flags);
		return;
	}

	if (!cmdq_sec_thread_timeout_excceed(thread)) {
		spin_unlock_irqrestore(&cmdq_sec_task_list_lock, flags);
		return;
	}

	cookie = cmdq_sec_get_secure_thread_exec_counter(thread->idx);
	timeout_task = thread->task_list[(cookie + 1) %
		CMDQ_MAX_TASK_IN_SECURE_THREAD];

	wait_cookie = thread->wait_cookie;

	spin_unlock_irqrestore(&cmdq_sec_task_list_lock, flags);

	if (timeout_task) {
		CMDQ_ERR(
			"timeout for thread:0x%p idx:%u hw cookie:%d wait_cookie:%d timeout task:0x%p handle:0x%p pkt:0x%p\n",
			thread->base, thread->idx,
			cookie, wait_cookie, timeout_task,
			timeout_task->handle, timeout_task->handle->pkt);

		cmdq_sec_task_err_callback(timeout_task->handle->pkt,
			-ETIMEDOUT);
	}

	cmdq_sec_thread_irq_handle_by_cookie(thread, -ETIMEDOUT, cookie + 1);
}

static int cmdq_mbox_startup(struct mbox_chan *chan)
{
	/* initialize when request channel */
	struct cmdq_sec_thread *thread = chan->con_priv;

	init_timer(&thread->timeout);
	thread->timeout.function = cmdq_sec_thread_handle_timeout;
	thread->timeout.data = (unsigned long)thread;
	INIT_WORK(&thread->timeout_work, cmdq_sec_task_timeout_work);
	thread->task_exec_wq = create_singlethread_workqueue("task_exec_wq");
	thread->occupied = true;
	return 0;
}

static void cmdq_mbox_shutdown(struct mbox_chan *chan)
{
	struct cmdq_sec_thread *thread = chan->con_priv;

	thread->occupied = false;
#if 0
	cmdq_thread_stop(chan->con_priv);
#endif
}

static bool cmdq_mbox_last_tx_done(struct mbox_chan *chan)
{
	return true;
}

static const struct mbox_chan_ops cmdq_mbox_chan_ops = {
	.send_data = cmdq_mbox_send_data,
	.startup = cmdq_mbox_startup,
	.shutdown = cmdq_mbox_shutdown,
	.last_tx_done = cmdq_mbox_last_tx_done,
};

static struct mbox_chan *cmdq_sec_xlate(struct mbox_controller *mbox,
		const struct of_phandle_args *sp)
{
	int ind = sp->args[0];
	struct cmdq_sec_thread *thread;

	if (ind >= mbox->num_chans) {
		CMDQ_ERR("invalid thread id:%d\n", ind);
		return ERR_PTR(-EINVAL);
	}

	thread = mbox->chans[ind].con_priv;
	thread->timeout_ms = sp->args[1] != 0 ?
		sp->args[1] : CMDQ_TIMEOUT_DEFAULT;
	thread->priority = sp->args[2];
	thread->chan = &mbox->chans[ind];

	return &mbox->chans[ind];
}

s32 cmdq_mbox_sec_chan_id(void *chan)
{
	struct cmdq_sec_thread *thread = ((struct mbox_chan *)chan)->con_priv;

	if (!thread || !thread->occupied)
		return -1;

	return thread->idx;
}

void cmdq_sec_thread_irq_handler(struct cmdq *cmdq,
	struct cmdq_sec_thread *thread)
{
	u32 cookie;
#if 0
	s32 err = 0;
	u32 irq_flag;
#endif

	cookie = cmdq_sec_get_secure_thread_exec_counter(thread->idx);
	if (cookie < thread->wait_cookie || !thread->task_cnt)
		return;

	CMDQ_MSG("%s thread:%d cookie:%u wait:%u cnt:%u\n",
		__func__, thread->idx, cookie, thread->wait_cookie,
		thread->task_cnt);

#if 0
	irq_flag = readl(thread->base + CMDQ_THR_IRQ_STATUS);
	writel(~irq_flag, thread->base + CMDQ_THR_IRQ_STATUS);
	CMDQ_MSG("CMDQ_THR_IRQ_STATUS:%u thread idx:%u cookie:%u\n",
		irq_flag, thread->idx, cookie);

	/*
	 * When ISR call this function, another CPU core could run
	 * "release task" right before we acquire the spin lock, and thus
	 * reset / disable this GCE thread, so we need to check the enable
	 * bit of this GCE thread.
	 */
	if (!(readl(thread->base + CMDQ_THR_ENABLE_TASK) & CMDQ_THR_ENABLED)) {
		CMDQ_MSG("[warn]not enable thread:%u cookie:%u wait:%u\n",
			thread->idx, cookie, thread->wait_cookie);
	}

	if (irq_flag & CMDQ_THR_IRQ_ERROR) {
		err = -EINVAL;
		cookie += 1;
	} else if (irq_flag & CMDQ_THR_IRQ_DONE) {
		err = 0;
	}

	if (err)
		CMDQ_ERR("%s err:%d thread:%d cookie:%d\n",
			__func__, err, thread->idx, cookie);

	cmdq_sec_thread_irq_handle_by_cookie(thread, err, cookie);
#else
	cmdq_sec_thread_irq_handle_by_cookie(thread, 0, cookie);
#endif
}

#if 0
static irqreturn_t cmdq_sec_irq_handler(int irq, void *dev)
{
	struct cmdq *cmdq = dev;
	unsigned long irq_status;
	u32 loaded_thd;
	int bit;
	bool normal_irq = false;

	if (!cmdq_thread_in_use())
		return IRQ_HANDLED;

	clk_enable(cmdq->clock);
	irq_status = readl(cmdq->base + CMDQ_CURR_IRQ_STATUS) & CMDQ_IRQ_MASK;
	clk_disable(cmdq->clock);

	CMDQ_MSG("%s CMDQ_CURR_IRQ_STATUS: 0x%x 0x%x load:0x%x\n", __func__,
		(u32)irq_status, (u32)(irq_status ^ CMDQ_IRQ_MASK),
		loaded_thd);
	if (!(irq_status ^ CMDQ_IRQ_MASK))
		return IRQ_HANDLED;

	for_each_clear_bit(bit, &irq_status, fls(CMDQ_IRQ_MASK)) {
		struct cmdq_sec_thread *thread = &cmdq->thread[bit];

		if (bit == CMDQ_SEC_IRQ_THREAD) {
			cmdq_sec_handle_irq_notify(cmdq, thread);
			continue;
		}

		if (!thread->occupied) {
			normal_irq = true;
			continue;
		}

		CMDQ_LOG("secure thread irq:%d\n", bit);
		cmdq_sec_thread_irq_handler(cmdq, thread);
	}

	/* let normal controller handle if normal irq coming  */
	return normal_irq ? IRQ_NONE : IRQ_HANDLED;
}
#endif

static int cmdq_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct cmdq *cmdq;
	int err, i;

	cmdq = devm_kzalloc(dev, sizeof(*cmdq), GFP_KERNEL);
	if (!cmdq)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	cmdq->base_pa = res->start;
	cmdq->base = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR(cmdq->base)) {
		cmdq_err("failed to ioremap gce\n");
		return PTR_ERR(cmdq->base);
	}

#if 0
	cmdq->irq = platform_get_irq(pdev, 0);
	if (!cmdq->irq) {
		CMDQ_ERR("failed to get irq\n");
		return -EINVAL;
	}

	err = devm_request_irq(dev, cmdq->irq, cmdq_sec_irq_handler,
		IRQF_SHARED, "mtk_cmdq", cmdq);
	if (err < 0) {
		CMDQ_ERR("failed to register ISR (%d)\n", err);
		return err;
	}
#endif
	dev_dbg(dev, "cmdq device: addr:0x%p va:0x%p irq:%d mask:%#x",
		dev, cmdq->base, cmdq->irq, (u32)CMDQ_IRQ_MASK);

	cmdq->clock = devm_clk_get(dev, "gce");
	if (IS_ERR(cmdq->clock)) {
		CMDQ_ERR("failed to get gce clk\n");
		return PTR_ERR(cmdq->clock);
	}

	cmdq->mbox.dev = dev;
	cmdq->mbox.chans = devm_kcalloc(dev, CMDQ_THR_MAX_COUNT,
					sizeof(*cmdq->mbox.chans), GFP_KERNEL);
	if (!cmdq->mbox.chans)
		return -ENOMEM;

	cmdq->mbox.num_chans = CMDQ_THR_MAX_COUNT;
	cmdq->mbox.ops = &cmdq_mbox_chan_ops;
	cmdq->mbox.of_xlate = cmdq_sec_xlate;

	/* make use of TXDONE_BY_ACK */
	cmdq->mbox.txdone_irq = false;
	cmdq->mbox.txdone_poll = false;

	for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++) {
		cmdq->thread[i].base = cmdq->base + CMDQ_THR_BASE +
				CMDQ_THR_SIZE * i;
		cmdq->thread[i].gce_pa = cmdq->base_pa;
		cmdq->thread[i].idx = i;
		cmdq->mbox.chans[i].con_priv = &cmdq->thread[i];
	}

	err = mbox_controller_register(&cmdq->mbox);
	if (err < 0) {
		CMDQ_ERR("failed to register mailbox:%d\n", err);
		return err;
	}

	cmdq->timeout_wq = create_singlethread_workqueue(
		"cmdq_timeout_wq");

	platform_set_drvdata(pdev, cmdq);
	WARN_ON(clk_prepare(cmdq->clock) < 0);

	/* TODO: should remove? */
	g_cmdq = cmdq;

	cmdq_msg("cmdq device: addr:0x%p va:0x%p irq:%d",
		dev, cmdq->base, cmdq->irq);

	return 0;
}

static const struct dev_pm_ops cmdq_pm_ops = {
	.suspend = cmdq_suspend,
	.resume = cmdq_resume,
};

static const struct of_device_id cmdq_of_ids[] = {
	{.compatible = "mediatek,mailbox-gce-svp"},
	{}
};

static struct platform_driver cmdq_drv = {
	.probe = cmdq_probe,
	.remove = cmdq_remove,
	.driver = {
		.name = CMDQ_DRIVER_NAME,
		.pm = &cmdq_pm_ops,
		.of_match_table = cmdq_of_ids,
	}
};


static __init int cmdq_init(void)
{
	u32 err = 0;

	CMDQ_LOG("%s enter", __func__);

	err = platform_driver_register(&cmdq_drv);
	if (err) {
		CMDQ_ERR("platform driver register failed:%d", err);
		return err;
	}

	return 0;
}

arch_initcall(cmdq_init);
