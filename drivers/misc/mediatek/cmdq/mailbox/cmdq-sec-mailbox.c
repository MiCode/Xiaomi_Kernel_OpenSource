// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/mailbox_controller.h>
#include <linux/sched/clock.h>
#include <linux/timer.h>

#include "cmdq-sec.h"
#include "cmdq-sec-mailbox.h"
#include "cmdq-sec-tl-api.h"
#include "cmdq-util.h"

#ifdef CMDQ_SECURE_MTEE_SUPPORT
#include "cmdq_sec_mtee.h"
#endif

#ifdef CMDQ_GP_SUPPORT
#include "cmdq-sec-gp.h"
static atomic_t m4u_init = ATOMIC_INIT(0);
#endif

#if IS_ENABLED(CONFIG_MMPROFILE)
#include <mmprofile.h>
#endif

#define CMDQ_SEC_DRV_NAME	"cmdq_sec_mbox"

#define CMDQ_THR_BASE		(0x100)
#define CMDQ_THR_SIZE		(0x80)
#define CMDQ_THR_EXEC_CNT_PA	(0x28)

/* CMDQ secure context struct
 * note it is not global data, each process has its own CMDQ sec context
 */
struct cmdq_sec_context {
	struct list_head listEntry;

	/* basic info */
	uint32_t tgid;		/* tgid of process context */
	uint32_t referCount;	/* reference count for open cmdq device node */

	/* iwc state */
	enum CMDQ_IWC_STATE_ENUM state;

	/* iwc information */
	void *iwc_msg;	/* message buffer */
	void *iwc_ex1;	/* message buffer extra */
	void *iwc_ex2;	/* message buffer extra */

#ifdef CMDQ_SECURE_SUPPORT
#ifdef CMDQ_GP_SUPPORT
	struct cmdq_sec_tee_context tee;	/* trustzone parameters */
#endif
#ifdef CMDQ_SECURE_MTEE_SUPPORT			/* MTEE parameters */
	void *mtee_iwc_msg;
	void *mtee_iwc_ex1;
	void *mtee_iwc_ex2;
	struct cmdq_sec_mtee_context mtee;
#endif
#endif
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

	u64			engineFlag;
	s32			scenario;
	u64			trigger;
};

struct cmdq_sec_thread {
	/* following part sync with struct cmdq_thread */
	struct mbox_chan	*chan;
	void __iomem		*base;
	phys_addr_t		gce_pa;
	struct list_head	task_list;
	struct timer_list	timeout;
	u32			timeout_ms;
	struct work_struct	timeout_work;
	u32			priority;
	u32			idx;
	bool			occupied;
	bool			dirty;

	/* following part only secure ctrl */
	u32			wait_cookie;
	u32			next_cookie;
	u32			task_cnt;
	struct workqueue_struct	*task_exec_wq;
};

/**
 * shared memory between normal and secure world
 */
struct cmdq_sec_shared_mem {
	void		*va;
	dma_addr_t	pa;
	u32		size;
};

#if IS_ENABLED(CONFIG_MMPROFILE)
struct cmdq_mmp_event {
	mmp_event cmdq_root;
	mmp_event cmdq;
	mmp_event queue;
	mmp_event submit;
	mmp_event invoke;
	mmp_event queue_notify;
	mmp_event notify;
	mmp_event irq;
	mmp_event cancel;
	mmp_event wait;
	mmp_event wait_done;
};

#endif

struct cmdq_sec {
	/* mbox / base / base_pa must sync with struct cmdq */
	struct mbox_controller	mbox;
	void __iomem		*base;
	phys_addr_t		base_pa;
	u8			hwid;
	// u32			irq;
	struct cmdq_base	*clt_base;
	struct cmdq_client	*clt;
	struct cmdq_pkt		*clt_pkt;
	struct work_struct	irq_notify_work;
	bool			notify_run;
	u64			sec_invoke;
	u64			sec_done;

	struct mutex		exec_lock;
	struct cmdq_sec_thread	thread[CMDQ_THR_MAX_COUNT];
	struct clk		*clock;
	bool			suspended;
	atomic_t		usage;
	struct workqueue_struct	*notify_wq;
	struct workqueue_struct	*timeout_wq;

	atomic_t			path_res;
	struct cmdq_sec_shared_mem	*shared_mem;
	struct cmdq_sec_context		*context;
	struct iwcCmdqCancelTask_t	cancel;
	struct cmdq_mmp_event		mmp;
};
static atomic_t cmdq_path_res = ATOMIC_INIT(0);
static atomic_t cmdq_path_res_mtee = ATOMIC_INIT(0);

static u32 g_cmdq_cnt;
static struct cmdq_sec *g_cmdq[2];

static s32
cmdq_sec_task_submit(struct cmdq_sec *cmdq, struct cmdq_sec_task *task,
	const u32 iwc_cmd, const u32 thrd_idx, void *data, bool mtee);

/* operator API */
static inline void
cmdq_sec_setup_tee_context_base(struct cmdq_sec_context *context)
{
#ifdef CMDQ_GP_SUPPORT
	cmdq_sec_setup_tee_context(&context->tee);
#endif

#ifdef CMDQ_SECURE_MTEE_SUPPORT
	cmdq_sec_mtee_setup_context(&context->mtee);
#endif
}

static inline s32
cmdq_sec_init_context_base(struct cmdq_sec_context *context)
{
	s32 status;

#ifdef CMDQ_GP_SUPPORT
	status = cmdq_sec_init_context(&context->tee);
	if (status < 0)
		return status;
#endif

#ifdef CMDQ_SECURE_MTEE_SUPPORT
	status = cmdq_sec_mtee_open_session(
		&context->mtee, context->mtee_iwc_msg);
#endif
	return status;
}

static inline void cmdq_mmp_init(struct cmdq_sec *cmdq)
{
#if IS_ENABLED(CONFIG_MMPROFILE)
	char name[32];
	int len;

	mmprofile_enable(1);
	if (cmdq->mmp.cmdq) {
		mmprofile_start(1);
		return;
	}

	len = snprintf(name, sizeof(name), "cmdq_sec_%hhu", cmdq->hwid);
	if (len >= sizeof(name))
		cmdq_log("len:%d over name size:%d", len, sizeof(name));

	cmdq->mmp.cmdq_root = mmprofile_register_event(MMP_ROOT_EVENT, "CMDQ");
	cmdq->mmp.cmdq = mmprofile_register_event(cmdq->mmp.cmdq_root, name);
	cmdq->mmp.queue = mmprofile_register_event(cmdq->mmp.cmdq, "queue");
	cmdq->mmp.submit = mmprofile_register_event(cmdq->mmp.cmdq, "submit");
	cmdq->mmp.invoke = mmprofile_register_event(cmdq->mmp.cmdq, "invoke");
	cmdq->mmp.queue_notify = mmprofile_register_event(cmdq->mmp.cmdq,
		"queue_notify");
	cmdq->mmp.notify = mmprofile_register_event(cmdq->mmp.cmdq, "notify");
	cmdq->mmp.irq = mmprofile_register_event(cmdq->mmp.cmdq, "irq");
	cmdq->mmp.cancel = mmprofile_register_event(cmdq->mmp.cmdq,
		"cancel_task");
	cmdq->mmp.wait = mmprofile_register_event(cmdq->mmp.cmdq, "wait");
	cmdq->mmp.wait_done = mmprofile_register_event(cmdq->mmp.cmdq,
		"wait_done");
	mmprofile_enable_event_recursive(cmdq->mmp.cmdq, 1);
	mmprofile_start(1);
#endif
}

static s32 cmdq_sec_clk_enable(struct cmdq_sec *cmdq)
{
	s32 usage = atomic_read(&cmdq->usage), err = clk_enable(cmdq->clock);

	if (err) {
		cmdq_err("clk_enable failed:%d usage:%d", err, usage);
		return err;
	}

	usage = atomic_inc_return(&cmdq->usage);
	if (usage == 1)
		cmdq_log("%s: cmdq startup gce:%pa",
			__func__, &cmdq->base_pa);
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
		cmdq_log("%s: cmdq shutdown gce:%pa",
			__func__, &cmdq->base_pa);
}

void cmdq_sec_mbox_enable(void *chan)
{
	struct cmdq_sec *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	cmdq_sec_clk_enable(cmdq);
}

void cmdq_sec_mbox_disable(void *chan)
{
	struct cmdq_sec *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	cmdq_sec_clk_disable(cmdq);
}

s32 cmdq_sec_mbox_chan_id(void *chan)
{
	struct cmdq_sec_thread *thread = ((struct mbox_chan *)chan)->con_priv;

	if (!thread || !thread->occupied)
		return -1;

	return thread->idx;
}
EXPORT_SYMBOL(cmdq_sec_mbox_chan_id);

s32 cmdq_sec_insert_backup_cookie(struct cmdq_pkt *pkt)
{
	struct cmdq_client *cl = (struct cmdq_client *)pkt->cl;
	struct cmdq_sec_thread *thread =
		((struct mbox_chan *)cl->chan)->con_priv;
	struct cmdq_sec *cmdq =
		container_of(thread->chan->mbox, struct cmdq_sec, mbox);
	struct cmdq_operand left, right;
	s32 err;

	if (!thread->occupied || !cmdq->shared_mem) {
		cmdq_err("shared_mem is NULL pkt:%p thrd-idx:%u cmdq:%p",
			pkt, thread->idx, cmdq);
		return -EFAULT;
	}
	cmdq_log("%s pkt:%p thread:%u gce:%#lx",
		__func__, pkt, thread->idx, (unsigned long)cmdq->base_pa);

	err = cmdq_pkt_read(pkt, NULL,
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

	err = cmdq_pkt_write_indriect(pkt, NULL,
		cmdq->shared_mem->pa + CMDQ_SEC_SHARED_THR_CNT_OFFSET +
		thread->idx * sizeof(u32), CMDQ_THR_SPR_IDX1, ~0);
	if (err)
		return err;
	return cmdq_pkt_set_event(pkt, CMDQ_TOKEN_SECURE_THR_EOF);
}
EXPORT_SYMBOL(cmdq_sec_insert_backup_cookie);

static u32 cmdq_sec_get_cookie(struct cmdq_sec *cmdq, u32 idx)
{
	return *(u32 *)(cmdq->shared_mem->va +
		CMDQ_SEC_SHARED_THR_CNT_OFFSET + idx * sizeof(u32));
}

s32 cmdq_sec_get_secure_thread_exec_counter(struct mbox_chan *chan)
{
	struct cmdq_sec *cmdq =
		container_of(chan->mbox, struct cmdq_sec, mbox);
	struct cmdq_sec_thread *thread =
		(struct cmdq_sec_thread *)chan->con_priv;

	return cmdq_sec_get_cookie(cmdq, thread->idx);
}

void cmdq_sec_dump_secure_thread_cookie(struct mbox_chan *chan)
{
	struct cmdq_sec *cmdq = container_of(chan->mbox, typeof(*cmdq), mbox);
	struct cmdq_sec_thread *thread =
		(struct cmdq_sec_thread *)chan->con_priv;

	cmdq_util_msg("secure shared cookie:%u wait:%u next:%u count:%u",
		cmdq_sec_get_secure_thread_exec_counter(chan),
		thread->wait_cookie,
		thread->next_cookie,
		thread->task_cnt);
}

void cmdq_sec_dump_thread_all(void *mbox_cmdq)
{
	struct cmdq_sec *cmdq = mbox_cmdq;
	u32 i;
	s32 usage = atomic_read(&cmdq->usage);

	cmdq_util_msg("cmdq:%#x usage:%d", (u32)cmdq->base_pa, usage);
	if (usage <= 0)
		return;

	for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++) {
		struct cmdq_sec_thread *thread = &cmdq->thread[i];

		if (!thread->occupied || list_empty(&thread->task_list))
			continue;

		cmdq_util_msg(
			"thd idx:%u secure shared cookie:%u wait:%u next:%u count:%u",
			thread->idx,
			cmdq_sec_get_cookie(cmdq, thread->idx),
			thread->wait_cookie,
			thread->next_cookie,
			thread->task_cnt);
	}
}

static void cmdq_sec_task_done(struct cmdq_sec_task *task, s32 err)
{
	cmdq_log("%s done task:%p pkt:%p err:%d",
		__func__, task, task->pkt, err);

	if (task->pkt->cb.cb) {
		struct cmdq_cb_data cb_data;

		cb_data.err = err;
		cb_data.data = task->pkt->cb.data;
		task->pkt->cb.cb(cb_data);
	}

	list_del_init(&task->list_entry);
	kfree(task);
}

static bool cmdq_sec_irq_handler(
	struct cmdq_sec_thread *thread, const u32 cookie, const s32 err)
{
	struct cmdq_sec_task *task, *temp, *cur_task = NULL;
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

	if (err)
		cmdq_err(
			"%s gce:%#lx thread:%u wait_cookie:%u cookie:%u done:%d err:%d",
			__func__, (unsigned long)cmdq->base_pa, thread->idx,
			thread->wait_cookie, cookie, done, err);
	else
		cmdq_log(
			"%s gce:%#lx thread:%u wait_cookie:%u cookie:%u done:%d err:%d",
			__func__, (unsigned long)cmdq->base_pa, thread->idx,
			thread->wait_cookie, cookie, done, err);

	list_for_each_entry_safe(task, temp, &thread->task_list, list_entry) {
		if (!done)
			break;

#if IS_ENABLED(CONFIG_MMPROFILE)
		mmprofile_log_ex(cmdq->mmp.irq, MMPROFILE_FLAG_PULSE,
			thread->idx, (unsigned long)task->pkt);
#endif

		cmdq_sec_task_done(task, 0);

		if (!thread->task_cnt)
			cmdq_err("thd:%u task_cnt:%u cannot below zero",
				thread->idx, thread->task_cnt);
		else
			thread->task_cnt -= 1;
		done--;
	}

	cur_task = list_first_entry_or_null(&thread->task_list,
		struct cmdq_sec_task, list_entry);

	if (err && cur_task) {
		struct cmdq_cb_data cb_data;

#if IS_ENABLED(CONFIG_MMPROFILE)
		mmprofile_log_ex(cmdq->mmp.irq, MMPROFILE_FLAG_PULSE,
			thread->idx, (unsigned long)cur_task->pkt);
#endif

		spin_unlock_irqrestore(&thread->chan->lock, flags);

		/* for error task, cancel, callback and done */
		memset(&cmdq->cancel, 0, sizeof(cmdq->cancel));
		cmdq_sec_task_submit(cmdq, cur_task,
			CMD_CMDQ_TL_CANCEL_TASK, thread->idx, &cmdq->cancel,
			((struct cmdq_sec_data *)
				cur_task->pkt->sec_data)->mtee);

		cb_data.err = err;
		cb_data.data = cur_task->pkt->err_cb.data;
		cur_task->pkt->err_cb.cb(cb_data);

		spin_lock_irqsave(&thread->chan->lock, flags);

		task = list_first_entry_or_null(&thread->task_list,
			struct cmdq_sec_task, list_entry);
		if (cur_task == task)
			cmdq_sec_task_done(cur_task, err);
		else
			cmdq_err("task list changed");

		/* error case stop all task for secure,
		 * since secure tdrv always remove all when cancel
		 */
		while (!list_empty(&thread->task_list)) {
			cur_task = list_first_entry(
				&thread->task_list, struct cmdq_sec_task,
				list_entry);

			cmdq_sec_task_done(cur_task, -ECONNABORTED);
		}
	} else if (err) {
		cmdq_msg("error but all task done, check notify callback");
	}

	if (list_empty(&thread->task_list)) {
		thread->wait_cookie = 0;
		thread->next_cookie = 0;
		thread->task_cnt = 0;
		__raw_writel(0, cmdq->shared_mem->va +
			CMDQ_SEC_SHARED_THR_CNT_OFFSET +
			thread->idx * sizeof(s32));
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		del_timer(&thread->timeout);
		cmdq_sec_clk_disable(cmdq);
		return true;
	}

	thread->wait_cookie = cookie % CMDQ_MAX_COOKIE_VALUE + 1;
	mod_timer(&thread->timeout, jiffies +
		msecs_to_jiffies(thread->timeout_ms));
	spin_unlock_irqrestore(&thread->chan->lock, flags);

	return false;
}

void cmdq_dump_summary(struct cmdq_client *client, struct cmdq_pkt *pkt);

void cmdq_sec_dump_notify_loop(void *chan)
{
	struct cmdq_sec *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);
	s32 usage = atomic_read(&cmdq->usage);

	if (usage <= 0) {
		cmdq_msg("%s cmdq sec off", __func__);
		return;
	}

	if (!cmdq->notify_run || !cmdq->clt || !cmdq->clt_pkt) {
		cmdq_msg("irq notify loop not enable clt:%p pkt:%p",
			cmdq->clt, cmdq->clt_pkt);
		return;
	}

	cmdq_dump_summary(cmdq->clt, cmdq->clt_pkt);
}

static void cmdq_sec_irq_notify_work(struct work_struct *work_item)
{
	struct cmdq_sec *cmdq = container_of(
		work_item, struct cmdq_sec, irq_notify_work);
	s32 i;
	bool stop = false, empty = true;

	mutex_lock(&cmdq->exec_lock);

#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log_ex(cmdq->mmp.notify, MMPROFILE_FLAG_START,
		cmdq->hwid, cmdq->notify_run);
#endif

	for (i = 0; i < CMDQ_MAX_SECURE_THREAD_COUNT; i++) {
		struct cmdq_sec_thread *thread =
			&cmdq->thread[CMDQ_MIN_SECURE_THREAD_ID + i];
		u32 cookie = cmdq_sec_get_cookie(cmdq, thread->idx);

		cmdq_log(
			"%s gce:%#lx thread:%u cookie:%u wait cookie:%u task count:%u",
			__func__, (unsigned long)cmdq->base_pa, thread->idx,
			cookie, thread->wait_cookie, thread->task_cnt);

		if (cookie < thread->wait_cookie || !thread->task_cnt)
			continue;

		stop |= cmdq_sec_irq_handler(thread, cookie, 0);
	}

	/* check if able to stop */
	if (stop) {
		for (i = 0; i < CMDQ_MAX_SECURE_THREAD_COUNT; i++)
			if (cmdq->thread[
				CMDQ_MIN_SECURE_THREAD_ID + i].task_cnt) {
				empty = false;
				break;
			}

		if (empty && cmdq->clt) {
			if (!cmdq->notify_run)
				cmdq_err("notify not enable gce:%#lx",
					(unsigned long)cmdq->base_pa);
			cmdq_mbox_stop(cmdq->clt);
			cmdq->notify_run = false;
		}
		cmdq_log("%s stop empty:%s gce:%#lx",
			__func__, empty ? "true" : "false",
			(unsigned long)cmdq->base_pa);
	}

#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log_ex(cmdq->mmp.notify, MMPROFILE_FLAG_END,
		cmdq->hwid, cmdq->notify_run);
#endif

	mutex_unlock(&cmdq->exec_lock);
}

static void cmdq_sec_irq_notify_callback(struct cmdq_cb_data cb_data)
{
	struct cmdq_sec *cmdq = (struct cmdq_sec *)cb_data.data;

	if (!work_pending(&cmdq->irq_notify_work)) {
		queue_work(cmdq->notify_wq, &cmdq->irq_notify_work);

#if IS_ENABLED(CONFIG_MMPROFILE)
		mmprofile_log_ex(cmdq->mmp.queue_notify, MMPROFILE_FLAG_PULSE,
			cmdq->hwid, 1);
#endif

	} else {
		cmdq_msg("last notify callback working");

#if IS_ENABLED(CONFIG_MMPROFILE)
		mmprofile_log_ex(cmdq->mmp.queue_notify, MMPROFILE_FLAG_PULSE,
			cmdq->hwid, 0);
#endif

	}
}

static s32 cmdq_sec_irq_notify_start(struct cmdq_sec *cmdq)
{
	s32 err;

	if (cmdq->notify_run)
		return 0;

	if (!cmdq->clt_pkt) {
		cmdq->clt = cmdq_mbox_create(cmdq->mbox.dev, 0);
		if (!cmdq->clt || IS_ERR(cmdq->clt)) {
			cmdq_err("clt mbox_create failed clt:%p index:%d",
				cmdq->clt, CMDQ_SEC_IRQ_THREAD);
			return -EINVAL;
		}

		cmdq->clt_pkt = cmdq_pkt_create(cmdq->clt);
		if (!cmdq->clt_pkt || IS_ERR(cmdq->clt_pkt)) {
			cmdq_err("clt_pkt cmdq_pkt_create failed pkt:%p index:%d",
				cmdq->clt_pkt, CMDQ_SEC_IRQ_THREAD);
			return -EINVAL;
		}

		INIT_WORK(&cmdq->irq_notify_work, cmdq_sec_irq_notify_work);
	}

	cmdq_pkt_wfe(cmdq->clt_pkt, CMDQ_TOKEN_SECURE_THR_EOF);
	cmdq_pkt_finalize_loop(cmdq->clt_pkt);
	cmdq_clear_event(cmdq->clt->chan, CMDQ_TOKEN_SECURE_THR_EOF);

	err = cmdq_pkt_flush_async(cmdq->clt_pkt,
		cmdq_sec_irq_notify_callback, (void *)cmdq);
	if (err < 0) {
		cmdq_err("irq cmdq_pkt_flush_async failed:%d", err);
		cmdq_mbox_stop(cmdq->clt);
	} else {
		cmdq->notify_run = true;
		cmdq_log("%s gce:%#lx",
			__func__, (unsigned long)cmdq->base_pa);
	}

	return err;
}

static s32 cmdq_sec_session_init(struct cmdq_sec_context *context)
{
	s32 err = 0;

	if (context->state >= IWC_SES_OPENED) {
		cmdq_log("session opened:%u", context->state);
		return err;
	}

	switch (context->state) {
	case IWC_INIT:
		err = cmdq_sec_init_context_base(context);
		if (err)
			break;
		context->state = IWC_CONTEXT_INITED;
	case IWC_CONTEXT_INITED:
#ifdef CMDQ_GP_SUPPORT
		if (!context->iwc_msg) {
			err = cmdq_sec_allocate_wsm(&context->tee,
				&context->iwc_msg, CMDQ_IWC_MSG,
				sizeof(struct iwcCmdqMessage_t));
			if (err)
				break;
		}
		if (!context->iwc_ex1) {
			err = cmdq_sec_allocate_wsm(&context->tee,
				&context->iwc_ex1, CMDQ_IWC_MSG1,
				sizeof(struct iwcCmdqMessageEx_t));
			if (err)
				break;
		}
		if (!context->iwc_ex2) {
			err = cmdq_sec_allocate_wsm(&context->tee,
				&context->iwc_ex2, CMDQ_IWC_MSG2,
				sizeof(struct iwcCmdqMessageEx2_t));
			if (err)
				break;
		}
#endif

#ifdef CMDQ_SECURE_MTEE_SUPPORT
		if (!context->mtee_iwc_msg ||
			!context->mtee_iwc_ex1 || !context->mtee_iwc_ex2) {
			err = cmdq_sec_mtee_allocate_wsm(&context->mtee,
				&context->mtee_iwc_msg,
				sizeof(struct iwcCmdqMessage_t),
				&context->mtee_iwc_ex1,
				sizeof(struct iwcCmdqMessageEx_t),
				&context->mtee_iwc_ex2,
				sizeof(struct iwcCmdqMessageEx2_t));
			if (err)
				break;
		}
#endif

		context->state = IWC_WSM_ALLOCATED;
	case IWC_WSM_ALLOCATED:
#ifdef CMDQ_GP_SUPPORT
		err = cmdq_sec_open_session(&context->tee, context->iwc_msg);
		if (err)
			break;
#endif
		context->state = IWC_SES_OPENED;
	default:
		break;
	}
	return err;
}

static s32 cmdq_sec_fill_iwc_msg(struct cmdq_sec_context *context,
	struct cmdq_sec_task *task, u32 thrd_idx)
{
	struct iwcCmdqMessage_t *iwc_msg = NULL;
	struct iwcCmdqMessageEx_t *iwc_msg_ex1 = NULL;
	struct iwcCmdqMessageEx2_t *iwc_msg_ex2 = NULL;
	struct cmdq_sec_data *data =
		(struct cmdq_sec_data *)task->pkt->sec_data;
	struct cmdq_pkt_buffer *buf, *last;
	u32 size = CMDQ_CMD_BUFFER_SIZE, offset = 0, *instr;
	u32 i;

	if (!data->mtee) {
		iwc_msg = (struct iwcCmdqMessage_t *)context->iwc_msg;
		iwc_msg_ex1 = (struct iwcCmdqMessageEx_t *)context->iwc_ex1;
		iwc_msg_ex2 = (struct iwcCmdqMessageEx2_t *)context->iwc_ex2;
	}
#ifdef CMDQ_SECURE_MTEE_SUPPORT
	else {
		iwc_msg =
			(struct iwcCmdqMessage_t *)context->mtee_iwc_msg;
		iwc_msg_ex1 =
			(struct iwcCmdqMessageEx_t *)context->mtee_iwc_ex1;
		iwc_msg_ex2 =
			(struct iwcCmdqMessageEx2_t *)context->mtee_iwc_ex2;
	}
#endif

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

		iwc_msg->command.metadata.addrListLength =
			data->addrMetadataCount;
		memcpy(iwc_msg->command.metadata.addrList,
			(u32 *)(unsigned long)data->addrMetadatas,
			data->addrMetadataCount * sizeof(*addr));

		addr = iwc_msg->command.metadata.addrList;
	}
	iwc_msg->command.metadata.enginesNeedDAPC = data->enginesNeedDAPC;
	iwc_msg->command.metadata.enginesNeedPortSecurity =
		data->enginesNeedPortSecurity;
	iwc_msg->command.hNormalTask = (unsigned long)task->pkt;

	iwc_msg->metaex_type = data->client_meta_type;
	iwc_msg->iwcex_available = 0;
	if (data->client_meta_size[CMDQ_IWC_MSG1] &&
		data->client_meta[CMDQ_IWC_MSG1]) {
		iwc_msg->iwcex_available |= (1 << CMDQ_IWC_MSG1);
		iwc_msg_ex1->size = data->client_meta_size[CMDQ_IWC_MSG1];
		memcpy((void *)&iwc_msg_ex1->data[0],
			data->client_meta[CMDQ_IWC_MSG1],
			data->client_meta_size[CMDQ_IWC_MSG1]);
	}

	if (data->client_meta_size[CMDQ_IWC_MSG2] &&
		data->client_meta[CMDQ_IWC_MSG2]) {
		iwc_msg->iwcex_available |= (1 << CMDQ_IWC_MSG2);
		iwc_msg_ex2->size = data->client_meta_size[CMDQ_IWC_MSG2];
		memcpy((void *)&iwc_msg_ex2->data[0],
			data->client_meta[CMDQ_IWC_MSG2],
			data->client_meta_size[CMDQ_IWC_MSG2]);
	}

	/* SVP HDR */
	iwc_msg->command.mdp_extension = data->mdp_extension;
	iwc_msg->command.readback_cnt = data->readback_cnt;
	for (i = 0; i < data->readback_cnt; i++) {
		memcpy(&iwc_msg->command.readback_engs[i],
			&data->readback_engs[i],
			sizeof(struct readback_engine));
	}

	return 0;
}

static s32 cmdq_sec_session_send(struct cmdq_sec_context *context,
	struct cmdq_sec_task *task, const u32 iwc_cmd, const u32 thrd_idx,
	struct cmdq_sec *cmdq, bool mtee)
{
	s32 err = 0;
	bool mem_ex1, mem_ex2;
	u64 cost;
	struct iwcCmdqMessage_t *iwc_msg = NULL;

	if (!mtee)
		iwc_msg = (struct iwcCmdqMessage_t *)context->iwc_msg;
#ifdef CMDQ_SECURE_MTEE_SUPPORT
	else
		iwc_msg = (struct iwcCmdqMessage_t *)context->mtee_iwc_msg;
#endif

	memset(iwc_msg, 0, sizeof(*iwc_msg));
	iwc_msg->cmd = iwc_cmd;
	iwc_msg->command.thread = thrd_idx;
	iwc_msg->cmdq_id = cmdq_util_hw_id(cmdq->base_pa);
	iwc_msg->debug.logLevel =
		cmdq_util_is_feature_en(CMDQ_LOG_FEAT_SECURE);

	switch (iwc_cmd) {
	case CMD_CMDQ_TL_SUBMIT_TASK:
		err = cmdq_sec_fill_iwc_msg(context, task, thrd_idx);
		if (err)
			return err;
		break;
	case CMD_CMDQ_TL_CANCEL_TASK:
		cmdq_util_err("cancel task thread:%u wait cookie:%u",
			thrd_idx, task->waitCookie);
		iwc_msg->cancelTask.waitCookie = task->waitCookie;
		iwc_msg->cancelTask.thread = thrd_idx;
		break;
	case CMD_CMDQ_TL_PATH_RES_ALLOCATE:
		if (!cmdq->shared_mem ||
			!cmdq->shared_mem->va) {
			cmdq_err("shared_mem is NULL");
			return -EFAULT;
		}
		iwc_msg->pathResource.size = cmdq->shared_mem->size;
		iwc_msg->pathResource.shareMemoyPA =
			cmdq->shared_mem->pa;
		iwc_msg->pathResource.useNormalIRQ = 1;

#ifdef CMDQ_SECURE_MTEE_SUPPORT
		/* TODO */
		if (mtee) {
			err = cmdq_sec_mtee_allocate_shared_memory(
				&context->mtee,
				iwc_msg->pathResource.shareMemoyPA,
				iwc_msg->pathResource.size);
			if (err) {
				cmdq_err(
					"MTEE alloc. shared memory failed");
				return err;
			}
		}
#endif

		break;
	default:
		break;
	}

	mem_ex1 = iwc_msg->iwcex_available & (1 << CMDQ_IWC_MSG1);
	mem_ex2 = iwc_msg->iwcex_available & (1 << CMDQ_IWC_MSG2);

#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log_ex(cmdq->mmp.invoke, MMPROFILE_FLAG_START,
		thrd_idx, task ? (unsigned long)task->pkt : 0);
#endif

	cmdq->sec_invoke = sched_clock();
	cmdq_log("%s execute cmdq:%p task:%lx command:%u thread:%u cookie:%d",
		__func__, cmdq, (unsigned long)task, iwc_cmd, thrd_idx,
		task ? task->waitCookie : -1);

	/* send message */
	if (!mtee) {
#ifdef CMDQ_GP_SUPPORT
		err = cmdq_sec_execute_session(&context->tee, iwc_cmd, 3000,
			mem_ex1, mem_ex2);
#endif
	}
#ifdef CMDQ_SECURE_MTEE_SUPPORT
	else
		err = cmdq_sec_mtee_execute_session(
			&context->mtee, iwc_cmd, 3000,
			mem_ex1, mem_ex2);
#endif

	cmdq->sec_done = sched_clock();
	cost = div_u64(cmdq->sec_done - cmdq->sec_invoke, 1000000);
	if (cost >= 1000)
		cmdq_msg("%s execute done cmdq:%p task:%lx cost:%lluus",
			__func__, cmdq, (unsigned long)task, cost);
	else
		cmdq_log("%s execute done cmdq:%p task:%lx cost:%lluus",
			__func__, cmdq, (unsigned long)task, cost);

#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log_ex(cmdq->mmp.invoke, MMPROFILE_FLAG_END,
		thrd_idx, task ? (unsigned long)task->pkt : 0);
#endif

	if (err)
		return err;
	context->state = IWC_SES_ON_TRANSACTED;

	return err;
}

#if defined(CMDQ_SECURE_MTEE_SUPPORT) || \
	defined(CMDQ_GP_SUPPORT)
static s32 cmdq_sec_session_reply(const u32 iwc_cmd,
	struct iwcCmdqMessage_t *iwc_msg, void *data,
	struct cmdq_sec_task *task)
{
	struct iwcCmdqCancelTask_t *cancel = data;
	struct cmdq_sec_data *sec_data = task->pkt->sec_data;

	if (iwc_cmd == CMD_CMDQ_TL_SUBMIT_TASK) {
		if (iwc_msg->rsp < 0) {
			/* submit fail case copy status */
			memcpy(&sec_data->sec_status, &iwc_msg->secStatus,
				sizeof(sec_data->sec_status));
			sec_data->response = iwc_msg->rsp;
		}
	} else if (iwc_cmd == CMD_CMDQ_TL_CANCEL_TASK && cancel) {
		/* cancel case only copy cancel result */
		memcpy(cancel, &iwc_msg->cancelTask, sizeof(*cancel));
	}

	return iwc_msg->rsp;
}
#endif

void cmdq_sec_dump_operation(void *chan)
{
	struct cmdq_sec *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	cmdq_util_msg(
		"secure tdrv op:%x thread suspend:%u reset:%u invoke:%llu/%llu",
		*(u32 *)(cmdq->shared_mem->va + CMDQ_SEC_SHARED_OP_OFFSET),
		*(u32 *)(cmdq->shared_mem->va + CMDQ_SEC_SHARED_SUSPEND_CNT),
		*(u32 *)(cmdq->shared_mem->va + CMDQ_SEC_SHARED_RESET_CNT),
		cmdq->sec_invoke, cmdq->sec_done);
}

void cmdq_sec_dump_response(void *chan, struct cmdq_pkt *pkt,
	u64 **inst, const char **dispatch)
{
	struct cmdq_sec *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);
	struct cmdq_sec_data *sec_data = pkt->sec_data;
	struct iwcCmdqCancelTask_t *cancel = &cmdq->cancel;
	struct iwcCmdqSecStatus_t *sec_status = &sec_data->sec_status;
	s32 thread_id = cmdq_sec_mbox_chan_id(chan);
	u32 i;

	if (((cancel->errInstr[1] >> 24) & 0xFF) ==
		CMDQ_CODE_WFE) {
		const uint32_t event = 0x3FF & cancel->errInstr[1];

		cmdq_util_err("secure error inst event:%u value:%d",
			event, cancel->regValue);
	}

	/* assign hang instruction for aee */
	*inst = (u64 *)cancel->errInstr;

	cmdq_util_err(
		"cancel_task pkt:%p inst:%#010x %08x aee:%d reset:%d pc:%#010x",
			pkt, cancel->errInstr[1], cancel->errInstr[0],
			cancel->throwAEE, cancel->hasReset, cancel->pc);

	cmdq_util_err(
		"last sec status cmdq:%p step:%u status:%d args:%#x %#x %#x %#x dispatch:%s",
		cmdq, sec_status->step,
		sec_status->status, sec_status->args[0],
		sec_status->args[1], sec_status->args[2],
		sec_status->args[3], sec_status->dispatch);
	for (i = 0; i < sec_status->inst_index; i += 2)
		cmdq_util_err("instr %d: %#010x %#010x", i / 2,
			sec_status->sec_inst[i], sec_status->sec_inst[i + 1]);

	switch (sec_status->status) {
	case -CMDQ_ERR_ADDR_CONVERT_HANDLE_2_PA:
		*dispatch = "TEE";
		break;
	case -CMDQ_ERR_ADDR_CONVERT_ALLOC_MVA:
	case -CMDQ_ERR_ADDR_CONVERT_ALLOC_MVA_N2S:
		switch (thread_id) {
		case CMDQ_THREAD_SEC_PRIMARY_DISP:
		case CMDQ_THREAD_SEC_SUB_DISP:
			*dispatch = "DISP";
			break;
		case CMDQ_THREAD_SEC_MDP:
			*dispatch = "MDP";
			break;
		case CMDQ_THREAD_SEC_ISP:
			*dispatch = "ISP";
			break;
		}
		break;
	}
}

static s32
cmdq_sec_task_submit(struct cmdq_sec *cmdq, struct cmdq_sec_task *task,
	const u32 iwc_cmd, const u32 thrd_idx, void *data, bool mtee)
{
	struct cmdq_sec_context *context = NULL;
	char *dispatch = "CMDQ";
	s32 err;
	bool dump_err = false;
	struct cmdq_pkt *pkt = task ? task->pkt : NULL;

	cmdq_log("task:%p iwc_cmd:%u cmdq:%p thrd-idx:%u tgid:%u",
		task, iwc_cmd, cmdq, thrd_idx, current->tgid);

#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log_ex(cmdq->mmp.submit, MMPROFILE_FLAG_PULSE,
		thrd_idx, (unsigned long)pkt);
#endif

	cmdq_trace_begin("%s_%u", __func__, iwc_cmd);

	do {
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

		if (cmdq->context->state == IWC_INIT)
			cmdq_sec_setup_tee_context_base(cmdq->context);

		err = cmdq_sec_session_init(cmdq->context);
		if (err) {
			err = -CMDQ_ERR_SEC_CTX_SETUP;
			break;
		}

#ifdef CMDQ_GP_SUPPORT
		/* do m4u sec init */
		if (atomic_cmpxchg(&m4u_init, 0, 1) == 0) {
			m4u_sec_init();
			cmdq_log("[SEC] M4U_sec_init is called\n");
		}
#endif

		err = cmdq_sec_irq_notify_start(cmdq);
		if (err < 0) {
			cmdq_err("start irq notify fail");
			break;
		}

		if (iwc_cmd == CMD_CMDQ_TL_SUBMIT_TASK && pkt)
			pkt->rec_trigger = sched_clock();
		err = cmdq_sec_session_send(
			cmdq->context, task, iwc_cmd, thrd_idx, cmdq, mtee);

		if (err) {
			cmdq_util_dump_lock();
			cmdq_util_error_enable();
			dump_err = true;
		} else {
			if (!mtee) {
#ifdef CMDQ_GP_SUPPORT
				err = cmdq_sec_session_reply(iwc_cmd,
					cmdq->context->iwc_msg, data, task);
#endif
			}
#ifdef CMDQ_SECURE_MTEE_SUPPORT
			else {
				err = cmdq_sec_session_reply(iwc_cmd,
					cmdq->context->mtee_iwc_msg,
					data, task);
			}
#endif
		}
	} while (0);

	if (err)
		cmdq_util_err(
			"sec invoke err:%d pkt:%p thread:%u dispatch:%s gce:%#lx",
			err, pkt, thrd_idx, dispatch,
			(unsigned long)cmdq->base_pa);

	if (dump_err) {
		cmdq_util_error_disable();
		cmdq_util_dump_unlock();
	}

	cmdq_trace_end();

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
	{.compatible = "mediatek,mailbox-gce-sec",},
	{}
};

void cmdq_sec_mbox_switch_normal(struct cmdq_client *cl)
{
#ifdef CMDQ_GP_SUPPORT
	struct cmdq_sec *cmdq =
		container_of(cl->chan->mbox, typeof(*cmdq), mbox);
	struct cmdq_sec_thread *thread =
		(struct cmdq_sec_thread *)cl->chan->con_priv;

	cmdq_sec_resume(cmdq->mbox.dev);
	cmdq_sec_clk_enable(cmdq);
	cmdq_log("[ IN] %s: cl:%p cmdq:%p thrd:%p idx:%u\n",
		__func__, cl, cmdq, thread, thread->idx);

	mutex_lock(&cmdq->exec_lock);
	/* TODO : use other CMD_CMDQ_TL for maintenance */
	cmdq_sec_task_submit(cmdq, NULL, CMD_CMDQ_TL_PATH_RES_RELEASE,
		thread->idx, NULL, false);
	mutex_unlock(&cmdq->exec_lock);

	cmdq_log("[OUT] %s: cl:%p cmdq:%p thrd:%p idx:%u\n",
		__func__, cl, cmdq, thread, thread->idx);
	cmdq_sec_clk_disable(cmdq);
	cmdq_sec_suspend(cmdq->mbox.dev);
#endif
}
EXPORT_SYMBOL(cmdq_sec_mbox_switch_normal);

static void cmdq_sec_task_exec_work(struct work_struct *work_item)
{
	struct cmdq_sec_task *task =
		container_of(work_item, struct cmdq_sec_task, exec_work);
	struct cmdq_sec *cmdq =
		container_of(task->thread->chan->mbox, struct cmdq_sec, mbox);
	struct cmdq_sec_data *data;
	struct cmdq_pkt_buffer *buf;
	unsigned long flags;
	s32 err;

	cmdq_log("%s gce:%#lx task:%p pkt:%p thread:%u",
		__func__, (unsigned long)cmdq->base_pa, task, task->pkt,
		task->thread->idx);

	buf = list_first_entry(
		&task->pkt->buf, struct cmdq_pkt_buffer, list_entry);

	if (!task->pkt->sec_data) {
		cmdq_err("pkt:%p without sec_data", task->pkt);
		return;
	}
	data = (struct cmdq_sec_data *)task->pkt->sec_data;

	mutex_lock(&cmdq->exec_lock);

	spin_lock_irqsave(&task->thread->chan->lock, flags);
	if (!task->thread->task_cnt) {
		mod_timer(&task->thread->timeout, jiffies +
			msecs_to_jiffies(task->thread->timeout_ms));
		task->thread->wait_cookie = 1;
		task->thread->next_cookie = 1;
		task->thread->task_cnt = 0;
		WARN_ON(cmdq_sec_clk_enable(cmdq) < 0);
		__raw_writel(0, cmdq->shared_mem->va +
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

	if (!atomic_cmpxchg(data->mtee ?
		&cmdq_path_res_mtee : &cmdq_path_res, 0, 1)) {
		err = cmdq_sec_task_submit(cmdq, NULL,
			CMD_CMDQ_TL_PATH_RES_ALLOCATE,
			CMDQ_INVALID_THREAD,
			NULL, data->mtee);
		if (err) {
			atomic_set(data->mtee ?
				&cmdq_path_res_mtee : &cmdq_path_res, 0);
			goto task_err_callback;
		}
	}

	if (task->thread->task_cnt > CMDQ_MAX_TASK_IN_SECURE_THREAD) {
		cmdq_err("task_cnt:%u cannot more than %u task:%p thrd-idx:%u",
			task->thread->task_cnt, CMDQ_MAX_TASK_IN_SECURE_THREAD,
			task, task->thread->idx);
		err = -EMSGSIZE;
		goto task_err_callback;
	}

	err = cmdq_sec_task_submit(
		cmdq, task, CMD_CMDQ_TL_SUBMIT_TASK, task->thread->idx, NULL,
		data->mtee);
	if (err)
		cmdq_err("task submit CMD_CMDQ_TL_SUBMIT_TASK failed:%d gce:%#lx task:%p thread:%u",
			err, (unsigned long)cmdq->base_pa, task,
			task->thread->idx);

task_err_callback:
	if (err) {
		struct cmdq_cb_data cb_data;

		cb_data.err = err;
		cb_data.data = task->pkt->err_cb.data;
		if (task->pkt->err_cb.cb)
			task->pkt->err_cb.cb(cb_data);

		cb_data.data = task->pkt->cb.data;
		if (task->pkt->cb.cb)
			task->pkt->cb.cb(cb_data);

		spin_lock_irqsave(&task->thread->chan->lock, flags);
		if (!task->thread->task_cnt)
			cmdq_err("thread:%u task_cnt:%u cannot below zero",
				task->thread->idx, task->thread->task_cnt);
		else
			task->thread->task_cnt -= 1;

		task->thread->next_cookie = (task->thread->next_cookie - 1 +
			CMDQ_MAX_COOKIE_VALUE) % CMDQ_MAX_COOKIE_VALUE;
		list_del(&task->list_entry);
		cmdq_msg(
			"gce:%#lx err:%d task:%p pkt:%p thread:%u task_cnt:%u wait_cookie:%u next_cookie:%u",
			(unsigned long)cmdq->base_pa, err, task, task->pkt,
			task->thread->idx, task->thread->task_cnt,
			task->thread->wait_cookie, task->thread->next_cookie);
		spin_unlock_irqrestore(&task->thread->chan->lock, flags);
		kfree(task);
	}
	mutex_unlock(&cmdq->exec_lock);
}

static int cmdq_sec_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data;
	struct cmdq_sec_data *sec_data =
		(struct cmdq_sec_data *)pkt->sec_data;
	struct cmdq_sec_thread *thread =
		(struct cmdq_sec_thread *)chan->con_priv;
	struct cmdq_sec *cmdq =
		container_of(thread->chan->mbox, struct cmdq_sec, mbox);
	struct cmdq_sec_task *task;

	if (!sec_data) {
		cmdq_err("pkt:%p sec_data not ready from thrd-idx:%u",
			pkt, thread->idx);
		return -EINVAL;
	}

	task = kzalloc(sizeof(*task), GFP_ATOMIC);
	if (!task)
		return -ENOMEM;
	pkt->task_alloc = true;

#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log_ex(cmdq->mmp.queue, MMPROFILE_FLAG_PULSE,
		thread->idx, (unsigned long)pkt);
#endif

	task->pkt = pkt;
	task->thread = thread;
	task->scenario = sec_data->scenario;
	task->engineFlag = sec_data->enginesNeedDAPC |
		sec_data->enginesNeedPortSecurity;

	INIT_WORK(&task->exec_work, cmdq_sec_task_exec_work);
	queue_work(thread->task_exec_wq, &task->exec_work);
	return 0;
}

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
	struct cmdq_sec_task *task;
	unsigned long flags;
	u64 duration;
	u32 cookie;

	mutex_lock(&cmdq->exec_lock);

	spin_lock_irqsave(&thread->chan->lock, flags);
	if (list_empty(&thread->task_list)) {
		cmdq_log("thread:%u task_list is empty", thread->idx);
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		goto done;
	}

	task = list_first_entry(
		&thread->task_list, struct cmdq_sec_task, list_entry);
	duration = div_u64(sched_clock() - task->trigger, 1000000);
	if (duration < thread->timeout_ms) {
		mod_timer(&thread->timeout, jiffies +
			msecs_to_jiffies(thread->timeout_ms - duration));
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		goto done;
	}

	cookie = cmdq_sec_get_cookie(cmdq, thread->idx);
	spin_unlock_irqrestore(&thread->chan->lock, flags);

	cmdq_err("%s duration:%llu cookie:%u thread:%u",
		__func__, duration, cookie, thread->idx);
	cmdq_sec_irq_handler(thread, cookie, -ETIMEDOUT);

done:
	mutex_unlock(&cmdq->exec_lock);
}

static int cmdq_sec_mbox_startup(struct mbox_chan *chan)
{
	struct cmdq_sec_thread *thread =
		(struct cmdq_sec_thread *)chan->con_priv;
	char name[32];
	int len;

	thread->timeout.function = cmdq_sec_thread_timeout;
	thread->timeout.data = (unsigned long)thread;
	init_timer(&thread->timeout);

	INIT_WORK(&thread->timeout_work, cmdq_sec_task_timeout_work);
	len = snprintf(name, sizeof(name), "task_exec_wq_%u", thread->idx);
	if (len >= sizeof(name))
		cmdq_log("len:%d over name size:%d", len, sizeof(name));

	thread->task_exec_wq = create_singlethread_workqueue(name);
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

#if IS_ENABLED(CONFIG_MMPROFILE)
void cmdq_sec_mmp_wait(struct mbox_chan *chan, void *pkt)
{
	struct cmdq_sec_thread *thread = chan->con_priv;
	struct cmdq_sec *cmdq = container_of(chan->mbox, typeof(*cmdq), mbox);

	mmprofile_log_ex(cmdq->mmp.wait, MMPROFILE_FLAG_PULSE,
		thread->idx, (unsigned long)pkt);
}
EXPORT_SYMBOL(cmdq_sec_mmp_wait);

void cmdq_sec_mmp_wait_done(struct mbox_chan *chan, void *pkt)
{
	struct cmdq_sec_thread *thread = chan->con_priv;
	struct cmdq_sec *cmdq = container_of(chan->mbox, typeof(*cmdq), mbox);

	mmprofile_log_ex(cmdq->mmp.wait_done, MMPROFILE_FLAG_PULSE,
		thread->idx, (unsigned long)pkt);
}
EXPORT_SYMBOL(cmdq_sec_mmp_wait_done);

#endif

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
		cmdq_err("invalid thrd-idx:%u", idx);
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

	cmdq_msg("%s", __func__);

	cmdq = devm_kzalloc(&pdev->dev, sizeof(*cmdq), GFP_KERNEL);
	if (!cmdq)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	cmdq->base_pa = res->start;
	cmdq->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(cmdq->base)) {
		cmdq_err("base devm_ioremap failed:%ld", PTR_ERR(cmdq->base));
		return PTR_ERR(cmdq->base);
	}

	cmdq->clock = devm_clk_get(&pdev->dev, "gce");
	if (IS_ERR(cmdq->clock)) {
		cmdq_err("gce devm_clk_get failed:%ld", PTR_ERR(cmdq->clock));
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

	cmdq->notify_wq = create_singlethread_workqueue("cmdq_sec_notify_wq");
	cmdq->timeout_wq = create_singlethread_workqueue("cmdq_sec_timeout_wq");
	err = mbox_controller_register(&cmdq->mbox);
	if (err) {
		cmdq_err("mbox_controller_register failed:%d", err);
		return err;
	}

	cmdq->shared_mem = devm_kzalloc(&pdev->dev,
		sizeof(*cmdq->shared_mem), GFP_KERNEL);
	if (!cmdq->shared_mem)
		return -ENOMEM;
	cmdq->shared_mem->va = dma_alloc_coherent(&pdev->dev, PAGE_SIZE,
		&cmdq->shared_mem->pa, GFP_KERNEL);
	cmdq->shared_mem->size = PAGE_SIZE;

	platform_set_drvdata(pdev, cmdq);
	WARN_ON(clk_prepare(cmdq->clock) < 0);

	cmdq->hwid = cmdq_util_track_ctrl(cmdq, cmdq->base_pa, true);

	cmdq_mmp_init(cmdq);

	cmdq_msg("cmdq:%p(%u) va:%p pa:%pa",
		cmdq, cmdq->hwid, cmdq->base, &cmdq->base_pa);

	g_cmdq[g_cmdq_cnt++] = cmdq;
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

#ifdef CMDQ_GP_SUPPORT
static s32 cmdq_sec_late_init_wsm(void *data)
{
	struct cmdq_sec *cmdq;
	struct cmdq_sec_context *context = NULL;
	s32 i = 0, err = 0;

	do {
		msleep(10000);
		cmdq = g_cmdq[i];
		if (!cmdq)
			break;
		cmdq_msg("g_cmdq_cnt:%u g_cmdq:%u pa:%pa",
			g_cmdq_cnt, i, &cmdq->base_pa);

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

		mutex_lock(&cmdq->exec_lock);
		if (cmdq->context->state == IWC_INIT)
			cmdq_sec_setup_tee_context_base(cmdq->context);

		err = cmdq_sec_session_init(cmdq->context);
		mutex_unlock(&cmdq->exec_lock);
		if (err) {
			err = -CMDQ_ERR_SEC_CTX_SETUP;
			cmdq_err("session init failed:%d", err);
			continue;
		}
	} while (++i < g_cmdq_cnt);
	return err;
}

static int __init cmdq_sec_late_init(void)
{
	struct task_struct *kthr =
		kthread_run(cmdq_sec_late_init_wsm, g_cmdq, __func__);

	if (IS_ERR(kthr))
		cmdq_err("kthread_run failed:%ld", PTR_ERR(kthr));
	return PTR_ERR(kthr);
}
late_initcall(cmdq_sec_late_init);
#endif

arch_initcall(cmdq_sec_init);
