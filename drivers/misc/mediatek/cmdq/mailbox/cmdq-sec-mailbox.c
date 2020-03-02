// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/mailbox_controller.h>
#include <linux/sched/clock.h>
#include <linux/timer.h>

#include "cmdq-sec.h"
#include "cmdq-sec-gp.h"
#include "cmdq-sec-mailbox.h"
#include "cmdq-sec-iwc-common.h"
#include "cmdq-sec-tl-api.h"

#define CMDQ_SEC_DRV_NAME	"cmdq_sec_mbox"

#define CMDQ_THR_BASE		(0x100)
#define CMDQ_THR_SIZE		(0x80)
#define CMDQ_THR_EXEC_CNT_PA	(0x28)

#define CMDQ_SYNC_TOKEN_SEC_DONE	(694)

/* reply struct for cmdq_sec_cancel_error_task */
struct cmdqSecCancelTaskResultStruct {
	/* [OUT] */
	bool throwAEE;
	bool hasReset;
	int32_t irqFlag;
	uint32_t errInstr[2];
	uint32_t regValue;
	uint32_t pc;
};

struct cmdq_sec_task {
	struct list_head	list_entry;
	dma_addr_t		pa_base;
	struct cmdq_sec_thread	*thread;
	struct cmdq_pkt		*pkt;
	u64			exec_time;
	struct work_struct	exec_work;

	bool			resetExecCnt;
	u32			waitCookie;
	// struct cmdqRecStruct
	u64				engineFlag;
	s32				scenario;
	u64				trigger;
};

struct cmdq_sec_thread {
	struct mbox_chan	*chan;
	void __iomem		*base;
	phys_addr_t		gce_pa;
	struct list_head	task_list;
	struct timer_list	timeout;
	u32			timeout_ms;
	struct work_struct	timeout_work;
	u32			priority;
	u32			idx;
	// bool			dirty;
	bool			occupied;

	u32			wait_cookie;
	u32			next_cookie;
	u32			task_cnt;
	struct workqueue_struct	*task_exec_wq;
};

/**
 * shared memory between normal and secure world
 */
struct cmdqSecSharedMemoryStruct {
	void		*va;
	dma_addr_t	pa;
	u32		size;
};

struct cmdq_sec {
	struct mbox_controller	mbox;
	void __iomem		*base;
	phys_addr_t		base_pa;
	// u32			irq;
	struct cmdq_base	*clt_base;
	struct cmdq_client	*clt;
	struct cmdq_pkt		*clt_pkt;
	struct work_struct	irq_notify_work;

	struct mutex		exec_lock;
	struct cmdq_sec_thread	thread[CMDQ_THR_MAX_COUNT];
	struct clk		*clock;
	bool			suspended;
	atomic_t		usage;
	struct workqueue_struct	*timeout_wq;

	atomic_t				path_res;
	struct cmdqSecSharedMemoryStruct	*hSecSharedMem;
	struct cmdqSecContextStruct		*context;
	struct iwcCmdqSecStatus_t		*secStatus;
};

static s32
cmdq_sec_task_submit(struct cmdq_sec *cmdq, struct cmdq_sec_task *task,
	const u32 iwc_cmd, const u32 thrd_idx, void *data);

static s32 cmdq_sec_clk_enable(struct cmdq_sec *cmdq)
{
	s32 usage = atomic_read(&cmdq->usage), err = clk_enable(cmdq->clock);

	if (err) {
		cmdq_err("clk_enable failed:%d usage:%d", err, usage);
		return err;
	}

	usage = atomic_inc_return(&cmdq->usage);
	if (usage == 1)
		cmdq_msg("%s: cmdq startup", __func__);
	return err;
}

static void cmdq_sec_clk_disable(struct cmdq_sec *cmdq)
{
	s32 usage = atomic_read(&cmdq->usage);

	if (!usage) {
		cmdq_err("usage:%d count error", usage);
		return;
	}
	clk_disable(cmdq->clock);

	usage = atomic_dec_return(&cmdq->usage);
	if (!usage)
		cmdq_msg("%s: cmdq shutdown", __func__);
}

s32 cmdq_sec_mbox_chan_id(void *chan)
{
	struct cmdq_sec_thread *thread = ((struct mbox_chan *)chan)->con_priv;

	if (!thread || !thread->occupied)
		return -1;

	return thread->idx;
}

// cmdq_sec_insert_backup_cookie_instr
s32 cmdq_sec_insert_backup_cookie(struct cmdq_pkt *pkt)
{
	struct cmdq_client *cl = (struct cmdq_client *)pkt->priv;
	struct cmdq_sec_thread *thread =
		((struct mbox_chan *)cl->chan)->con_priv;
	struct cmdq_sec *cmdq =
		container_of(thread->chan->mbox, struct cmdq_sec, mbox);
	struct cmdq_operand left, right;
	s32 err;

	cmdq_log("pkt:%p thread idx:%u", pkt, thread->idx);
	if (!cmdq->hSecSharedMem) {
		cmdq_err("hSecSharedMem is NULL");
		return -EFAULT;
	}

	err = cmdq_pkt_read(pkt, cmdq->clt_base,
		(u32)(thread->gce_pa + CMDQ_THR_BASE +
		CMDQ_THR_SIZE * thread->idx + CMDQ_THR_EXEC_CNT_PA),
		CMDQ_THR_SPR_IDX1);
	if (err)
		return err;

	left.reg = true;
	left.idx = CMDQ_THR_SPR_IDX1;
	right.reg = false;
	right.value = 1;
	cmdq_pkt_logic_command(
		pkt, CMDQ_LOGIC_ADD, CMDQ_THR_SPR_IDX1, &left, &right);

	err = cmdq_pkt_write_indriect(pkt, cmdq->clt_base,
		cmdq->hSecSharedMem->pa + CMDQ_SEC_SHARED_THR_CNT_OFFSET +
		thread->idx * sizeof(u32), CMDQ_THR_SPR_IDX1, ~0);
	if (err)
		return err;
	return cmdq_pkt_set_event(pkt, CMDQ_SYNC_TOKEN_SEC_DONE);
}
EXPORT_SYMBOL(cmdq_sec_insert_backup_cookie);

// cmdq_sec_irq_notify_stop
static void cmdq_sec_irq_notify_stop_work(struct work_struct *work_item)
{
	struct cmdq_sec *cmdq =
		container_of(work_item, struct cmdq_sec, irq_notify_work);
	s32 empty = ~0, i;

	for (i = 0; i < CMDQ_MAX_SECURE_THREAD_COUNT; i++)
		if (cmdq->thread[
			CMDQ_MIN_SECURE_THREAD_ID + i].task_cnt) {
			empty = 0;
			break;
		}
	if (empty && cmdq->clt)
		cmdq_mbox_stop(cmdq->clt);
}

// cmdq_sec_thread_irq_handle_by_cookie << cmdq_sec_thread_irq_handler
static void cmdq_sec_irq_handler(
	struct cmdq_sec_thread *thread, const u32 cookie, const s32 err)
{
	struct cmdq_sec_task *task, *temp;
	struct cmdq_sec *cmdq =
		container_of(thread->chan->mbox, struct cmdq_sec, mbox);
	unsigned long flags;
	s32 done;

	spin_lock_irqsave(&thread->chan->lock, flags);
	if (thread->wait_cookie <= cookie)
		done = cookie - thread->wait_cookie + 1;
	else if (thread->wait_cookie == (cookie + 1) % CMDQ_MAX_COOKIE_VALUE)
		done = 0;
	else
		done = CMDQ_MAX_COOKIE_VALUE - thread->wait_cookie + 1 +
			cookie + 1;
	done += err ? 1 : 0;

	cmdq_log("thread idx:%u wait_cookie:%u hw_cookie:%u done:%d err:%d",
		thread->idx, thread->wait_cookie, cookie, done, err);

	list_for_each_entry_safe(task, temp, &thread->task_list, list_entry) {
		// cmdq_sec_task_callback
		if (task->pkt->cb.cb) {
			struct cmdq_cb_data cb_data;

			cb_data.err = (done == 1 && err) ? err : 0;
			cb_data.data = task->pkt->cb.data;
			task->pkt->cb.cb(cb_data);
		}
		// cmdq_sec_remove_handle_from_thread_by_cookie
		list_del(&task->list_entry);
		if (!thread->task_cnt)
			cmdq_err("thread idx:%u task_cnt:%u cannot below zero",
				thread->idx, thread->task_cnt);
		else
			thread->task_cnt -= 1;
		if (!--done)
			break;
		kfree(task);
	}
	spin_unlock_irqrestore(&thread->chan->lock, flags);

	if (err && task) {
		struct cmdqSecCancelTaskResultStruct cancel;

		// cmdq_sec_cancel_error_task_unlocked
		memset(&cancel, 0, sizeof(cancel));
		cmdq_sec_task_submit(cmdq, task,
			CMD_CMDQ_TL_CANCEL_TASK, thread->idx, &cancel);
	}
	kfree(task);

	spin_lock_irqsave(&thread->chan->lock, flags);
	if (list_empty(&thread->task_list)) {
		thread->wait_cookie = 0;
		thread->next_cookie = 0;
		thread->task_cnt = 0;
		__raw_writel(0, cmdq->hSecSharedMem->va +
			CMDQ_SEC_SHARED_THR_CNT_OFFSET +
			thread->idx * sizeof(s32));
		queue_work(cmdq->timeout_wq, &cmdq->irq_notify_work);
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		del_timer(&thread->timeout);
		cmdq_sec_clk_disable(cmdq);
		return;
	}
	thread->wait_cookie = cookie % CMDQ_MAX_COOKIE_VALUE + 1;
	mod_timer(&thread->timeout, jiffies +
		msecs_to_jiffies(thread->timeout_ms));
	spin_unlock_irqrestore(&thread->chan->lock, flags);
}

// >> cmdq_sec_handle_irq_notify
static void cmdq_sec_irq_notify_callback(struct cmdq_cb_data cb_data)
{
	struct cmdq_sec *cmdq = (struct cmdq_sec *)cb_data.data;
	s32 i;

	for (i = 0; i < CMDQ_MAX_SECURE_THREAD_COUNT; i++) {
		struct cmdq_sec_thread *thread =
			&cmdq->thread[CMDQ_MIN_SECURE_THREAD_ID + i];
		u32 cookie = *(u32 *)(cmdq->hSecSharedMem->va +
			CMDQ_SEC_SHARED_THR_CNT_OFFSET +
			thread->idx * sizeof(s32));

		if (cookie < thread->wait_cookie || !thread->task_cnt)
			continue;
		cmdq_sec_irq_handler(thread, cookie, 0);
	}
}

static void cmdq_sec_irq_notify_start(struct cmdq_sec *cmdq)
{
	s32 err;

	cmdq_pkt_wfe(cmdq->clt_pkt, CMDQ_SYNC_TOKEN_SEC_DONE);
	cmdq_pkt_finalize_loop(cmdq->clt_pkt);
	// TODO: cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_SEC_DONE);

	err = cmdq_pkt_flush_async(cmdq->clt, cmdq->clt_pkt,
		cmdq_sec_irq_notify_callback, (void *)cmdq);
	if (err < 0) {
		cmdq_err("irq cmdq_pkt_flush_async failed:%d", err);
		cmdq_mbox_stop(cmdq->clt);
	}
}

// cmdq_sec_init_session_unlocked
static s32 cmdq_sec_session_init(struct cmdqSecContextStruct *context)
{
	s32 err = 0;

	if (context->state >= IWC_SES_OPENED) {
		cmdq_log("session opened:%u", context->state);
		return err;
	}

	// log: CMDQ_PROF_START
	switch (context->state) {
	case IWC_INIT:
		err = cmdq_sec_init_context(&context->tee);
		if (err)
			break;
		context->state = IWC_CONTEXT_INITED;
	case IWC_CONTEXT_INITED:
		if (context->iwcMessage) {
			cmdq_err("iwcMessage not NULL:%p",
				context->iwcMessage);
			err = -EINVAL;
			break;
		}
		err = cmdq_sec_allocate_wsm(&context->tee,
			&context->iwcMessage, sizeof(struct iwcCmdqMessage_t),
			&context->iwcMessageEx,
			sizeof(struct iwcCmdqMessageEx_t));
		if (err)
			break;
		context->state = IWC_WSM_ALLOCATED;
	case IWC_WSM_ALLOCATED:
		err = cmdq_sec_open_session(&context->tee, context->iwcMessage);
		if (err)
			break;
		context->state = IWC_SES_OPENED;
	default:
		break;
	}
	// log: CMDQ_PROF_END
	return err;
}

// cmdq_sec_fill_iwc_command_msg_unlocked
static s32 cmdq_sec_fill_iwc_msg(struct cmdqSecContextStruct *context,
	struct cmdq_sec_task *task, u32 thrd_idx)
{
	struct iwcCmdqMessage_t *iwc_msg =
		(struct iwcCmdqMessage_t *)context->iwcMessage;
#if 0
	struct iwcCmdqMessageEx_t *iwc_msg_ex =
		(struct iwcCmdqMessageEx_t *)context->iwcMessageEx;
#endif
	struct cmdqSecDataStruct *data =
		(struct cmdqSecDataStruct *)task->pkt->sec_data;
	struct cmdq_pkt_buffer *buf, *last;
	u32 size = CMDQ_CMD_BUFFER_SIZE, offset = 0, *instr;

	if (CMDQ_TZ_CMD_BLOCK_SIZE <
		task->pkt->cmd_buf_size + 4 * CMDQ_INST_SIZE) {
		cmdq_err("task:%p size:%zu > %u",
			task, task->pkt->cmd_buf_size, CMDQ_TZ_CMD_BLOCK_SIZE);
		return -EFAULT;
	}
	if (thrd_idx == CMDQ_INVALID_THREAD) {
		iwc_msg->command.commandSize = 0;
		iwc_msg->command.metadata.addrListLength = 0;
		return 0;
	}

	iwc_msg->command.thread = thrd_idx;
	iwc_msg->command.scenario = task->scenario;
	iwc_msg->command.priority = task->pkt->priority;
	iwc_msg->command.engineFlag = task->engineFlag;

	last = list_last_entry(&task->pkt->buf, typeof(*last), list_entry);
	list_for_each_entry(buf, &task->pkt->buf, list_entry) {
		if (buf == last)
			size -= task->pkt->avail_buf_size;
		memcpy(iwc_msg->command.pVABase + offset, buf->va_base, size);
		iwc_msg->command.commandSize += size;
		offset += size / 4;
		if (buf != last) {
			instr = iwc_msg->command.pVABase + offset;
			instr[-1] = CMDQ_CODE_JUMP << 24;
			instr[-2] = CMDQ_REG_SHIFT_ADDR(CMDQ_INST_SIZE);
		}
	}
	instr = &iwc_msg->command.pVABase[iwc_msg->command.commandSize / 4 - 4];
	if (instr[0] == 0x1 && instr[1] == 0x40000000)
		instr[0] = 0;
	else
		cmdq_err("find EOC failed: %#x %#x", instr[1], instr[0]);
	iwc_msg->command.waitCookie = task->waitCookie;
	iwc_msg->command.resetExecCnt = task->resetExecCnt;

	if (data->addrMetadataCount) {
		struct iwcCmdqAddrMetadata_t *addr;
		s32 i;

		iwc_msg->command.metadata.addrListLength =
			data->addrMetadataCount;
		memcpy(iwc_msg->command.metadata.addrList,
			(u32 *)(unsigned long)data->addrMetadatas,
			data->addrMetadataCount * sizeof(*addr));

		addr = iwc_msg->command.metadata.addrList;
		for (i = 0; i <
			ARRAY_SIZE(iwc_msg->command.metadata.addrList); i++)
			addr[i].instrIndex = addr[i].instrIndex +
				addr[i].instrIndex /
				(CMDQ_NUM_CMD(CMDQ_CMD_BUFFER_SIZE) - 1);
	}
	iwc_msg->command.metadata.enginesNeedDAPC = data->enginesNeedDAPC;
	iwc_msg->command.metadata.enginesNeedPortSecurity =
		data->enginesNeedPortSecurity;
	// TODO: cmdq_sec_fill_isp_meta

	iwc_msg->command.hNormalTask = (unsigned long)task;
	return 0;
}

// cmdq_sec_send_context_session_message
static s32 cmdq_sec_session_send(struct cmdqSecContextStruct *context,
	struct cmdq_sec_task *task, const u32 iwc_cmd, const u32 thrd_idx,
	struct cmdq_sec *cmdq)
{
	struct iwcCmdqMessage_t *iwc_msg =
		(struct iwcCmdqMessage_t *)context->iwcMessage;
	s32 err = 0;

	memset(iwc_msg, 0, sizeof(*iwc_msg));
	iwc_msg->cmd = iwc_cmd;
	iwc_msg->debug.logLevel = 1;

	switch (iwc_cmd) {
	case CMD_CMDQ_TL_SUBMIT_TASK:
		err = cmdq_sec_fill_iwc_msg(context, task, thrd_idx);
		if (err)
			return err;
	case CMD_CMDQ_TL_CANCEL_TASK:
		iwc_msg->cancelTask.waitCookie = task->waitCookie;
		iwc_msg->cancelTask.thread = thrd_idx;
		break;
	case CMD_CMDQ_TL_PATH_RES_ALLOCATE:
	case CMD_CMDQ_TL_PATH_RES_RELEASE:
		if (!cmdq->hSecSharedMem ||
			!cmdq->hSecSharedMem->va) {
			cmdq_err("hSecSharedMem is NULL");
			return -EFAULT;
		}
		iwc_msg->pathResource.size = cmdq->hSecSharedMem->size;
		iwc_msg->pathResource.shareMemoyPA =
			cmdq->hSecSharedMem->pa;
		iwc_msg->pathResource.useNormalIRQ = 1;
		break;
	default:
		break;
	}
	err = cmdq_sec_execute_session(
		&context->tee, iwc_cmd, 3000, iwc_msg->iwcMegExAvailable);
	if (err)
		return err;
	context->state = IWC_SES_ON_TRANSACTED;
	return err;
}

// cmdq_sec_handle_session_reply_unlocked
static s32 cmdq_sec_session_reply(struct iwcCmdqMessage_t *iwc_msg, void *data)
{
	struct cmdqSecCancelTaskResultStruct *cancel =
		(struct cmdqSecCancelTaskResultStruct *)data;

	if (cancel) {
		cancel->throwAEE = iwc_msg->cancelTask.throwAEE;
		cancel->hasReset = iwc_msg->cancelTask.hasReset;
		cancel->irqFlag = iwc_msg->cancelTask.irqFlag;
		cancel->errInstr[0] = iwc_msg->cancelTask.errInstr[0];
		cancel->errInstr[1] = iwc_msg->cancelTask.errInstr[1];
		cancel->regValue = iwc_msg->cancelTask.regValue;
		cancel->pc = iwc_msg->cancelTask.pc;
	}

	if (((iwc_msg->cancelTask.errInstr[1] >> 24) & 0xFF) == CMDQ_CODE_WFE)
		; // log
	return iwc_msg->rsp;
}

// cmdq_sec_handle_attach_status
static void cmdq_sec_task_attach_status(struct cmdq_sec *cmdq,
	const u32 iwc_cmd, const s32 status, char **dispatch)
{
	struct iwcCmdqSecStatus_t *secStatus;
	struct iwcCmdqMessage_t *iwc_msg;
	s32 i;

	iwc_msg = (struct iwcCmdqMessage_t *)cmdq->context->iwcMessage;
	if (!iwc_msg) {
		cmdq_msg("%s: cmdq:%p without iwcMessage", __func__, cmdq);
		return;
	}

	secStatus = cmdq->secStatus;
	if (secStatus) {
		if (iwc_cmd == CMD_CMDQ_TL_CANCEL_TASK) {
			cmdq_err(
				"last secStatus:%p cmdq:%p step:%u status:%d args:%#x %#x %#x %#x dispatch:%s",
				secStatus, cmdq, secStatus->step,
				secStatus->status, secStatus->args[0],
				secStatus->args[1], secStatus->args[2],
				secStatus->args[3], secStatus->dispatch);
		} // else ; log
		kfree(cmdq->secStatus);
		cmdq->secStatus = NULL;
	}

	secStatus = kzalloc(sizeof(*secStatus), GFP_ATOMIC);
	if (!secStatus)
		return;
	memcpy(secStatus, &iwc_msg->secStatus, sizeof(*secStatus));
	cmdq->secStatus = secStatus;

	if (!secStatus->status && !status)
		return;

	switch (secStatus->status) {
	case -CMDQ_ERR_ADDR_CONVERT_HANDLE_2_PA:
		*dispatch = "TEE";
		break;
	case CMDQ_ERR_ADDR_CONVERT_ALLOC_MVA:
	case CMDQ_ERR_ADDR_CONVERT_ALLOC_MVA_N2S:
		switch (iwc_msg->command.thread) {
		case CMDQ_THREAD_SEC_PRIMARY_DISP:
		case CMDQ_THREAD_SEC_SUB_DISP:
			*dispatch = "DISP";
			break;
		case CMDQ_THREAD_SEC_MDP:
			*dispatch = "MDP";
			break;
		}
		break;
	}

	cmdq_err(
		"secStatus:%p cmdq:%p step:%u status:%d(%d) args:%#x %#x %#x %#x dispatch:%s(%s)",
		secStatus, cmdq, secStatus->step, secStatus->status, status,
		secStatus->args[0], secStatus->args[1], secStatus->args[2],
		secStatus->args[3], secStatus->dispatch, dispatch);
	for (i = 0; i < secStatus->inst_index; i += 2)
		cmdq_err("instr %d: %#x %#x", i / 2,
			secStatus->sec_inst[i], secStatus->sec_inst[i + 1]);
}

// cmdq_sec_submit_to_secure_world_async_unlocked
static s32
cmdq_sec_task_submit(struct cmdq_sec *cmdq, struct cmdq_sec_task *task,
	const u32 iwc_cmd, const u32 thrd_idx, void *data)
{
	struct cmdqSecContextStruct *context;
	char *dispatch = "CMDQ";
	u64 entry, exit;
	s32 err;

	cmdq_log("task:%p iwc_cmd:%u cmdq:%p thrd_idx:%u tgid:%u",
		task, iwc_cmd, cmdq, thrd_idx, current->tgid);
	do {
		// cmdq_sec_context_handle_create
		if (!cmdq->context) {
			context = kzalloc(sizeof(*cmdq->context),
				GFP_ATOMIC);
			if (!context) {
				err = -CMDQ_ERR_NULL_SEC_CTX_HANDLE;
				break;
			}
			cmdq->context = context;
			cmdq->context->state = IWC_INIT;
			cmdq->context->tgid = current->tgid;
		}

		cmdq_sec_irq_notify_start(cmdq);
		// cmdq_sec_setup_context_session
		if (cmdq->context->state == IWC_INIT)
			cmdq->context->tee.uuid = (struct TEEC_UUID){
			0x09010000, 0x0, 0x0, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0} };

		err = cmdq_sec_session_init(cmdq->context);
		if (err) {
			err = -CMDQ_ERR_SEC_CTX_SETUP;
			break;
		}

		entry = sched_clock();
		err = cmdq_sec_session_send(
			cmdq->context, task, iwc_cmd, thrd_idx, cmdq);
		if (!err && iwc_cmd == CMD_CMDQ_TL_CANCEL_TASK)
			err = cmdq_sec_session_reply(
				cmdq->context->iwcMessage, data);
		exit = sched_clock();
		// log: cmdq_sec_track_task_record
		cmdq_sec_task_attach_status(cmdq, iwc_cmd, err, &dispatch);

		// TODO: cmdq_sec_teardown_context_session >>
		// TODO: cmdq_sec_deinit_session_unlocked
	} while (0);

	if (err == -ETIMEDOUT) // log
		;
	else if (err)
		;
	return err;
}

static int cmdq_sec_suspend(struct device *dev)
{
	struct cmdq_sec *cmdq = dev_get_drvdata(dev);

	cmdq_log("cmdq:%p", cmdq);
	cmdq->suspended = true;
	clk_unprepare(cmdq->clock);
	return 0;
}

static int cmdq_sec_resume(struct device *dev)
{
	struct cmdq_sec *cmdq = dev_get_drvdata(dev);

	cmdq_log("cmdq:%p", cmdq);
	WARN_ON(clk_prepare(cmdq->clock) < 0);
	cmdq->suspended = false;
	return 0;
}

static const struct dev_pm_ops cmdq_sec_pm_ops = {
	.suspend = cmdq_sec_suspend,
	.resume = cmdq_sec_resume,
};

static const struct of_device_id cmdq_sec_of_ids[] = {
	{.compatible = "mediatek,gce-svp",},
	{}
};

// cmdq_sec_exec_task_async_impl >> cmdq_sec_exec_task_async_unlocked
static void cmdq_sec_task_exec_work(struct work_struct *work_item)
{
	struct cmdq_sec_task *task =
		container_of(work_item, struct cmdq_sec_task, exec_work);
	struct cmdq_sec *cmdq =
		container_of(task->thread->chan->mbox, struct cmdq_sec, mbox);
	struct cmdqSecDataStruct *data;
	struct cmdq_pkt_buffer *buf;
	unsigned long flags;
	s32 err;

	cmdq_log("cmdq:%p task:%p thrd_idx:%u", cmdq, task, task->thread->idx);
	buf = list_first_entry(
		&task->pkt->buf, struct cmdq_pkt_buffer, list_entry);
	// log

	if (!task->pkt->sec_data) {
		cmdq_err("pkt:%p without sec_data", task->pkt);
		return;
	}
	data = (struct cmdqSecDataStruct *)task->pkt->sec_data;

	mutex_lock(&cmdq->exec_lock);
	// cmdq_sec_insert_handle_from_thread_array_by_cookie
	spin_lock_irqsave(&task->thread->chan->lock, flags);
	if (!task->thread->task_cnt) {
		mod_timer(&task->thread->timeout, jiffies +
			msecs_to_jiffies(task->thread->timeout_ms));
		task->thread->wait_cookie = 1;
		task->thread->next_cookie = 1;
		task->thread->task_cnt = 0;
		WARN_ON(cmdq_sec_clk_enable(cmdq) < 0);
		__raw_writel(0, cmdq->hSecSharedMem->va +
			CMDQ_SEC_SHARED_THR_CNT_OFFSET +
			task->thread->idx * sizeof(s32));
		// mb();
	}
	task->resetExecCnt = task->thread->task_cnt ? false : true;
	task->waitCookie = task->thread->next_cookie;
	task->thread->next_cookie = (task->thread->next_cookie + 1) %
		CMDQ_MAX_COOKIE_VALUE;
	list_add_tail(&task->list_entry, &task->thread->task_list);
	task->thread->task_cnt += 1;
	spin_unlock_irqrestore(&task->thread->chan->lock, flags);
	task->trigger = sched_clock();

	// cmdq_sec_allocate_path_resource_unlocked
	if (!atomic_cmpxchg(&cmdq->path_res, 0, 1)) {
		err = cmdq_sec_task_submit(cmdq, NULL,
			CMD_CMDQ_TL_PATH_RES_ALLOCATE, CMDQ_INVALID_THREAD,
			NULL);
		if (err) {
			atomic_set(&cmdq->path_res, 0);
			goto task_err_callback;
		}
	}

	if (task->thread->task_cnt > CMDQ_MAX_TASK_IN_SECURE_THREAD) {
		cmdq_err("task_cnt:%u cannot more than %u task:%p thrd_idx:%u",
			task->thread->task_cnt, CMDQ_MAX_TASK_IN_SECURE_THREAD,
			task, task->thread->idx);
		err = -EMSGSIZE;
		goto task_err_callback;
	}

	err = cmdq_sec_task_submit(
		cmdq, task, CMD_CMDQ_TL_SUBMIT_TASK, task->thread->idx, NULL);
	if (err) {
		cmdq_err("task submit failed:%d task:%p iwc_cmd:%u thread:%u",
			err, task, CMD_CMDQ_TL_SUBMIT_TASK, task->thread->idx);
		// log
	}

task_err_callback:
	if (err) {
		struct cmdq_cb_data cb_data;

		cb_data.err = err;
		cb_data.data = task->pkt->cb.data;
		if (task->pkt->err_cb.cb)
			task->pkt->err_cb.cb(cb_data);
		if (task->pkt->cb.cb)
			task->pkt->cb.cb(cb_data);

		// cmdq_sec_remove_handle_from_thread_by_cookie
		spin_lock_irqsave(&task->thread->chan->lock, flags);
		if (!task->thread->task_cnt)
			cmdq_err("thread idx:%u task_cnt:%u cannot below zero",
				task->thread->idx, task->thread->task_cnt);
		else
			task->thread->task_cnt -= 1;

		task->thread->next_cookie = (task->thread->next_cookie - 1 +
			CMDQ_MAX_COOKIE_VALUE) % CMDQ_MAX_COOKIE_VALUE;
		list_del(&task->list_entry);
		cmdq_msg(
			"err:%d task:%p thrd_idx:%u task_cnt:%u wait_cookie:%u next_cookie:%u",
			err, task, task->thread->idx, task->thread->task_cnt,
			task->thread->wait_cookie, task->thread->next_cookie);
		spin_unlock_irqrestore(&task->thread->chan->lock, flags);
		kfree(task);
	}
	mutex_unlock(&cmdq->exec_lock);
}

// cmdq_sec_exec_task_async_work
static int cmdq_sec_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data;
	struct cmdqSecDataStruct *sec_data =
		(struct cmdqSecDataStruct *)pkt->sec_data;
	struct cmdq_sec_thread *thread =
		(struct cmdq_sec_thread *)chan->con_priv;
	struct cmdq_sec_task *task;

	task = kzalloc(sizeof(*task), GFP_ATOMIC);
	if (!task)
		return -ENOMEM;

	task->pkt = pkt;
	task->thread = thread;
	if (sec_data) {
		task->scenario = sec_data->scenario;
		task->engineFlag = sec_data->enginesNeedDAPC |
			sec_data->enginesNeedPortSecurity;
	} else {
		cmdq_err("pkt:%p sec_data not ready from thrd_idx:%u",
			pkt, thread->idx);
		return -EINVAL;
	}

	INIT_WORK(&task->exec_work, cmdq_sec_task_exec_work);
	queue_work(thread->task_exec_wq, &task->exec_work);
	return 0;
}

// cmdq_sec_thread_handle_timeout
static void cmdq_sec_thread_timeout(unsigned long data)
{
	struct cmdq_sec_thread *thread =
		(struct cmdq_sec_thread *)data;
	struct cmdq_sec *cmdq =
		container_of(thread->chan->mbox, struct cmdq_sec, mbox);

	if (!work_pending(&thread->timeout_work))
		queue_work(cmdq->timeout_wq, &thread->timeout_work);
}

static void cmdq_sec_task_timeout_work(struct work_struct *work_item)
{
	struct cmdq_sec_thread *thread =
		container_of(work_item, struct cmdq_sec_thread, timeout_work);
	struct cmdq_sec *cmdq =
		container_of(thread->chan->mbox, struct cmdq_sec, mbox);
	struct cmdq_sec_task *task, *head, *out_task = NULL;
	unsigned long flags;
	u64 duration;
	u32 cookie, done;

	spin_lock_irqsave(&thread->chan->lock, flags);
	if (list_empty(&thread->task_list)) {
		cmdq_log("thread idx:%u task_list is empty", thread->idx);
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		return;
	}

	// cmdq_sec_thread_timeout_excceed
	head = list_first_entry_or_null(
		&thread->task_list, struct cmdq_sec_task, list_entry);
	duration = div_u64(sched_clock() - head->trigger, 1000000);
	if (duration < thread->timeout_ms) {
		mod_timer(&thread->timeout, jiffies +
			msecs_to_jiffies(thread->timeout_ms - duration));
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		return;
	}

	// cmdq_sec_get_secure_thread_exec_counter
	cookie = *(u32 *)(cmdq->hSecSharedMem->va +
		CMDQ_SEC_SHARED_THR_CNT_OFFSET + thread->idx * sizeof(s32));

	if (thread->wait_cookie <= cookie)
		done = cookie - thread->wait_cookie + 1;
	else if (thread->wait_cookie == (cookie + 1) % CMDQ_MAX_COOKIE_VALUE)
		done = 0;
	else
		done = CMDQ_MAX_COOKIE_VALUE - thread->wait_cookie + 1 +
			cookie + 1;
	list_for_each_entry(task, &thread->task_list, list_entry) {
		if (!done) {
			out_task = task;
			break;
		}
		done -= 1;
	}
	spin_unlock_irqrestore(&thread->chan->lock, flags);
	// cmdq_sec_task_err_callback
	if (out_task && out_task->pkt->err_cb.cb) {
		struct cmdq_cb_data cb_data;

		cb_data.err = -ETIMEDOUT;
		cb_data.data = out_task->pkt->cb.data;
		out_task->pkt->err_cb.cb(cb_data);
	}
	cmdq_sec_irq_handler(thread, cookie, -ETIMEDOUT);
	cmdq_log("duration:%llu cookie:%u task:%p pkt:%p thread idx:%u",
		duration, cookie, out_task, out_task->pkt, thread->idx);
}

static int cmdq_sec_mbox_startup(struct mbox_chan *chan)
{
	struct cmdq_sec_thread *thread =
		(struct cmdq_sec_thread *)chan->con_priv;

	thread->timeout.function = cmdq_sec_thread_timeout;
	thread->timeout.data = (unsigned long)thread;
	init_timer(&thread->timeout);

	INIT_WORK(&thread->timeout_work, cmdq_sec_task_timeout_work);
	thread->task_exec_wq = create_singlethread_workqueue("task_exec_wq");
	thread->occupied = true;
	return 0;
}

static void cmdq_sec_mbox_shutdown(struct mbox_chan *chan)
{
	struct cmdq_sec_thread *thread =
		(struct cmdq_sec_thread *)chan->con_priv;

	thread->occupied = false;
#if 0
	cmdq_thread_stop(thread);
#endif
}

static bool cmdq_sec_mbox_last_tx_done(struct mbox_chan *chan)
{
	return true;
}

static const struct mbox_chan_ops cmdq_sec_mbox_chan_ops = {
	.send_data = cmdq_sec_mbox_send_data,
	.startup = cmdq_sec_mbox_startup,
	.shutdown = cmdq_sec_mbox_shutdown,
	.last_tx_done = cmdq_sec_mbox_last_tx_done,
};

static struct mbox_chan *cmdq_sec_mbox_of_xlate(
	struct mbox_controller *mbox, const struct of_phandle_args *sp)
{
	struct cmdq_sec_thread *thread;
	s32 idx = sp->args[0];

	if (mbox->num_chans <= idx) {
		cmdq_err("invalid thread idx:%u", idx);
		return ERR_PTR(-EINVAL);
	}

	thread = (struct cmdq_sec_thread *)mbox->chans[idx].con_priv;
	thread->chan = &mbox->chans[idx];
	thread->timeout_ms = sp->args[1] ? sp->args[1] : CMDQ_TIMEOUT_DEFAULT;
	thread->priority = sp->args[2];
	return &mbox->chans[idx];
}

static int cmdq_sec_probe(struct platform_device *pdev)
{
	struct cmdq_sec *cmdq;
	struct resource *res;
	s32 i, err;

	cmdq = devm_kzalloc(&pdev->dev, sizeof(*cmdq), GFP_KERNEL);
	if (!cmdq)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	cmdq->base_pa = res->start;
	cmdq->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(cmdq->base)) {
		cmdq_err("base devm_ioremap failed:%d", PTR_ERR(cmdq->base));
		return PTR_ERR(cmdq->base);
	}

	cmdq->clock = devm_clk_get(&pdev->dev, "GCE");
	if (IS_ERR(cmdq->clock)) {
		cmdq_err("gce devm_clk_get failed:%d", PTR_ERR(cmdq->clock));
		return PTR_ERR(cmdq->clock);
	}

	cmdq->mbox.chans = devm_kcalloc(&pdev->dev, CMDQ_THR_MAX_COUNT,
		sizeof(*cmdq->mbox.chans), GFP_KERNEL);
	if (!cmdq->mbox.chans)
		return -ENOMEM;

	cmdq->mbox.dev = &pdev->dev;
	cmdq->mbox.ops = &cmdq_sec_mbox_chan_ops;
	cmdq->mbox.num_chans = CMDQ_THR_MAX_COUNT;
	cmdq->mbox.txdone_irq = false;
	cmdq->mbox.txdone_poll = false;
	cmdq->mbox.of_xlate = cmdq_sec_mbox_of_xlate;

	mutex_init(&cmdq->exec_lock);
	for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++) {
		cmdq->thread[i].base =
			cmdq->base + CMDQ_THR_BASE + CMDQ_THR_SIZE * i;
		cmdq->thread[i].gce_pa = cmdq->base_pa;
		INIT_LIST_HEAD(&cmdq->thread[i].task_list);
		cmdq->thread[i].idx = i;
		cmdq->thread[i].occupied = false;
		cmdq->mbox.chans[i].con_priv = &cmdq->thread[i];
	}

	cmdq->timeout_wq = create_singlethread_workqueue("cmdq_sec_timeout_wq");
	err = mbox_controller_register(&cmdq->mbox);
	if (err) {
		cmdq_err("mbox_controller_register failed:%d", err);
		return err;
	}

	cmdq->clt_base = cmdq_register_device(&pdev->dev);
	cmdq->clt = cmdq_mbox_create(&pdev->dev, 0);
	if (!cmdq->clt || IS_ERR(cmdq->clt)) {
		cmdq_err("clt mbox_create failed clt:%p index:%d",
			cmdq->clt, CMDQ_SEC_IRQ_THREAD);
		return -ENOMEM;
	}
	cmdq_pkt_cl_create(&cmdq->clt_pkt, cmdq->clt);
	if (!cmdq->clt_pkt || IS_ERR(cmdq->clt_pkt)) {
		cmdq_err("clt_pkt cmdq_pkt_cl_create failed pkt:%p index:%d",
			cmdq->clt_pkt, CMDQ_SEC_IRQ_THREAD);
		return -ENOMEM;
	}
	INIT_WORK(&cmdq->irq_notify_work, cmdq_sec_irq_notify_stop_work);

	cmdq->hSecSharedMem = devm_kzalloc(&pdev->dev,
		sizeof(*cmdq->hSecSharedMem), GFP_KERNEL);
	if (!cmdq->hSecSharedMem)
		return -ENOMEM;
	cmdq->hSecSharedMem->va = dma_alloc_coherent(&pdev->dev, PAGE_SIZE,
		&cmdq->hSecSharedMem->pa, GFP_KERNEL);
	cmdq->hSecSharedMem->size = PAGE_SIZE;

	platform_set_drvdata(pdev, cmdq);
	WARN_ON(clk_prepare(cmdq->clock) < 0);

	cmdq_log("va:%p pa:%pa", cmdq->base, &cmdq->base_pa);
	return 0;
}

static int cmdq_sec_remove(struct platform_device *pdev)
{
	struct cmdq_sec *cmdq = platform_get_drvdata(pdev);

	mbox_controller_unregister(&cmdq->mbox);
	clk_unprepare(cmdq->clock);
	return 0;
}

static struct platform_driver cmdq_sec_drv = {
	.probe = cmdq_sec_probe,
	.remove = cmdq_sec_remove,
	.driver = {
		.name = CMDQ_SEC_DRV_NAME,
		.pm = &cmdq_sec_pm_ops,
		.of_match_table = cmdq_sec_of_ids,
	},
};

static int __init cmdq_sec_init(void)
{
	s32 err;

	err = platform_driver_register(&cmdq_sec_drv);
	if (err)
		cmdq_err("platform_driver_register failed:%d", err);
	return err;
}

arch_initcall(cmdq_sec_init);
