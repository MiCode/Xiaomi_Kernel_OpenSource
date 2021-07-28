// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME "@(%s:%d) " fmt, __func__, __LINE__
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/current.h>
#include "msg_thread.h"

#define MSG_OP_SIZE(prb) ((prb)->size)
#define MSG_OP_MASK(prb) (MSG_OP_SIZE(prb) - 1)
#define MSG_OP_COUNT(prb) ((prb)->write - (prb)->read)
#define MSG_OP_FULL(prb) (MSG_OP_COUNT(prb) >= MSG_OP_SIZE(prb))
#define MSG_OP_EMPTY(prb) ((prb)->write == (prb)->read)

#define MSG_OP_INIT(prb, qsize) \
do { \
	(prb)->read = (prb)->write = 0; \
	(prb)->size = (qsize); \
} while (0)

#define MSG_OP_PUT(prb, value) \
do { \
	if (!MSG_OP_FULL(prb)) { \
		(prb)->queue[(prb)->write & MSG_OP_MASK(prb)] = value; \
		++((prb)->write); \
	} \
	else { \
		pr_warn("Message queue is full."); \
	} \
} while (0)

#define MSG_OP_GET(prb, value) \
do { \
	if (!MSG_OP_EMPTY(prb)) { \
		value = (prb)->queue[(prb)->read & MSG_OP_MASK(prb)]; \
		++((prb)->read); \
		if (MSG_OP_EMPTY(prb)) { \
			(prb)->read = (prb)->write = 0; \
		} \
	} \
	else { \
		value = NULL; \
		pr_warn("Message queue is empty."); \
	} \
} while (0)

/*
 * Utility functions
 */
static int msg_evt_put_op_to_q(struct msg_op_q *op_q, struct msg_op *op)
{
	int ret = 0;
	unsigned long flags;

	if (!op_q || !op) {
		pr_err("invalid input param: pOpQ(0x%p), pLxOp(0x%p)\n", op_q, op);
		return -1;
	}

	spin_lock_irqsave(&op_q->lock, flags);

	/* acquire lock success */
	if (!MSG_OP_FULL(op_q))
		MSG_OP_PUT(op_q, op);
	else {
		pr_warn("MSG_OP_FULL(%p -> %p)\n", op, op_q);
		ret = -3;
	}

	spin_unlock_irqrestore(&op_q->lock, flags);
	return ret;
}


/*
 * Utility functions
 */
static struct msg_op *msg_evt_get_op_from_q(struct msg_op_q *op_q)
{
	unsigned long flags;
	struct msg_op *op;

	if (op_q == NULL) {
		pr_err("pOpQ = NULL\n");
		return NULL;
	}

	spin_lock_irqsave(&op_q->lock, flags);

	/* acquire lock success */
	MSG_OP_GET(op_q, op);

	spin_unlock_irqrestore(&op_q->lock, flags);

	if (op == NULL)
		pr_warn("MSG_OP_GET(%p) return NULL\n", op_q);

	return op;
}

/*
 *  msg_evt_thread API
 */

int msg_evt_put_op_to_free_queue(struct msg_thread_ctx *ctx, struct msg_op *op)
{
	if (msg_evt_put_op_to_q(&ctx->free_op_q, op))
		return -1;
	return 0;
}


struct msg_op *msg_evt_get_free_op(struct msg_thread_ctx *ctx)
{
	struct msg_op *op = NULL;

	op = msg_evt_get_op_from_q(&ctx->free_op_q);
	if (op)
		memset(op, 0, sizeof(struct msg_op));
	return op;
}

int msg_evt_put_op_to_active(struct msg_thread_ctx *ctx, struct msg_op *op)
{
	struct msg_op_signal *signal = NULL;
	int wait_ret = -1;
	int ret = 0;

	do {
		if (!op) {
			pr_err("msg_thread_ctx op(0x%p)\n", op);
			break;
		}

		signal = &op->signal;
		if (signal->timeoutValue) {
			op->result = -9;
			//osal_signal_init(signal);
			init_completion(&signal->comp);
			atomic_set(&op->ref_count, 1);
		} else
			atomic_set(&op->ref_count, 0);

		/* Increment ref_count by 1 as wmtd thread will hold a reference also,
		 * this must be done here instead of on target thread, because
		 * target thread might not be scheduled until a much later time,
		 * allowing current thread to decrement ref_count at the end of function,
		 * putting op back to free queue before target thread has a chance to process.
		 */
		atomic_inc(&op->ref_count);

		/* put to active Q */
		ret = msg_evt_put_op_to_q(&ctx->active_op_q, op);

		if (ret) {
			pr_warn("put to active queue fail\n");
			atomic_dec(&op->ref_count);
			break;
		}

		/* wake up thread */
		wake_up_interruptible(&ctx->waitQueue);

		if (signal->timeoutValue == 0)
			break;

		/* check result */
		wait_ret = wait_for_completion_timeout(&signal->comp,
				msecs_to_jiffies(5000)); // 5 sec

		if (wait_ret == 0)
			pr_warn("opId(%d) completion timeout\n", op->op.op_id);
		else if (op->result)
			pr_info("opId(%d) result:%d\n",
					op->op.op_id, op->result);

		/* op completes, check result */
		ret = op->result;
	} while (0);

	if (op != NULL && signal != NULL &&
		atomic_dec_and_test(&op->ref_count)) {
		/* put Op back to freeQ */
		msg_evt_put_op_to_free_queue(ctx, op);
	}

	return ret;
}


int msg_thread_send(struct msg_thread_ctx *ctx, int opid)
{
	return msg_thread_send_2(ctx, opid, 0, 0);
}

int msg_thread_send_1(struct msg_thread_ctx *ctx, int opid, size_t param1)
{
	return msg_thread_send_2(ctx, opid, param1, 0);
}

int msg_thread_send_2(struct msg_thread_ctx *ctx, int opid, size_t param1, size_t param2)
{
	struct msg_op *op = NULL;
	struct msg_op_signal *signal;
	int ret;

	op = msg_evt_get_free_op(ctx);
	if (!op) {
		pr_err("[%s] can't get free op\n", __func__);
		return -1;
	}
	op->op.op_id = opid;
	op->op.op_data[0] = param1;
	op->op.op_data[1] = param2;

	signal = &op->signal;
	signal->timeoutValue = 0;

	ret = msg_evt_put_op_to_active(ctx, op);

	return ret;
}

int msg_thread_send_5(struct msg_thread_ctx *ctx, int opid, size_t param1,
							size_t param2, size_t param3,
							size_t param4, size_t param5)
{
	struct msg_op *op = NULL;
	struct msg_op_signal *signal;
	int ret;

	op = msg_evt_get_free_op(ctx);
	if (!op) {
		pr_err("[%s] can't get free op\n", __func__);
		return -1;
	}
	op->op.op_id = opid;
	op->op.op_data[0] = param1;
	op->op.op_data[1] = param2;
	op->op.op_data[2] = param3;
	op->op.op_data[3] = param4;
	op->op.op_data[4] = param5;

	signal = &op->signal;
	signal->timeoutValue = 0;

	ret = msg_evt_put_op_to_active(ctx, op);

	return ret;

}

int msg_thread_send_wait(struct msg_thread_ctx *ctx, int opid, int timeout)
{
	return msg_thread_send_wait_3(ctx, opid, timeout, 0, 0, 0);
}

int msg_thread_send_wait_1(struct msg_thread_ctx *ctx, int opid, int timeout,
							size_t param1)
{
	return msg_thread_send_wait_3(ctx, opid, timeout, param1, 0, 0);
}


int msg_thread_send_wait_2(struct msg_thread_ctx *ctx, int opid, int timeout,
							size_t param1,
							size_t param2)
{
	return msg_thread_send_wait_3(ctx, opid, timeout, param1, param2, 0);
}

int msg_thread_send_wait_3(struct msg_thread_ctx *ctx,
							int opid, int timeout,
							size_t param1,
							size_t param2,
							size_t param3)
{
	struct msg_op *op = NULL;
	struct msg_op_signal *signal = NULL;
	int ret;

	op = msg_evt_get_free_op(ctx);
	if (!op) {
		pr_err("[%s] can't get free op\n", __func__);
		return -1;
	}
	op->op.op_id = opid;
	op->op.op_data[0] = param1;
	op->op.op_data[1] = param2;
	op->op.op_data[2] = param3;

	signal = &op->signal;
	signal->timeoutValue = timeout > 0 ? timeout : MSG_OP_TIMEOUT;
	ret = msg_evt_put_op_to_active(ctx, op);
	return ret;

}


int msg_thread_send_wait_4(struct msg_thread_ctx *ctx, int opid, int timeout, size_t param1,
							size_t param2, size_t param3, size_t param4)
{
	struct msg_op *op = NULL;
	struct msg_op_signal *signal = NULL;
	int ret;

	op = msg_evt_get_free_op(ctx);
	if (!op) {
		pr_err("[%s] can't get free op\n", __func__);
		return -1;
	}
	op->op.op_id = opid;
	op->op.op_data[0] = param1;
	op->op.op_data[1] = param2;
	op->op.op_data[2] = param3;
	op->op.op_data[3] = param4;

	signal = &op->signal;
	signal->timeoutValue = timeout > 0 ? timeout : MSG_OP_TIMEOUT;
	ret = msg_evt_put_op_to_active(ctx, op);
	return ret;

}

int msg_evt_set_current_op(struct msg_thread_ctx *ctx, struct msg_op *op)
{
	ctx->cur_op = op;
	return 0;
}

int msg_evt_opid_handler(struct msg_thread_ctx *ctx, struct msg_op_data *op)
{
	int opid, ret;

	/* sanity check */
	if (op == NULL) {
		pr_warn("null op\n");
		return -1;
	}

	opid = op->op_id;

	if (opid >= ctx->op_func_size) {
		pr_err("msg_evt_thread invalid OPID(%d)\n", opid);
		return -3;
	}

	if (ctx->op_func[opid] == NULL) {
		pr_err("null handler (%d)\n", opid);
		return -4;
	}
	ret = (*(ctx->op_func[opid])) (op);
	return ret;
}

unsigned int msg_evt_wait_event_checker(struct msg_thread_ctx *ctx)
{
	unsigned long flags;
	int ret = 0;

	if (ctx) {
		spin_lock_irqsave(&ctx->active_op_q.lock, flags);
		ret = !MSG_OP_EMPTY(&ctx->active_op_q);
		spin_unlock_irqrestore(&ctx->active_op_q.lock, flags);
		return ret;
	}
	return 0;
}

static int msg_evt_thread(void *pvData)
{
	struct msg_thread_ctx *ctx = (struct msg_thread_ctx *)pvData;
	struct task_struct *p_thread = NULL;
	struct msg_op *op;
	int ret;

	if (ctx == NULL || ctx->pThread == NULL) {
		pr_err("[%s] ctx is NULL", __func__);
		return -1;
	}
	p_thread = ctx->pThread;

	for (;;) {
		op = NULL;

		wait_event_interruptible(ctx->waitQueue,
			(kthread_should_stop() || msg_evt_wait_event_checker(ctx)));

		if ((p_thread) && !IS_ERR_OR_NULL(p_thread) && kthread_should_stop()) {
			pr_info("[%s] thread should stop now...\n", __func__);
			/* TODO: clean up active opQ */
			break;
		}

		/* get Op from activeQ */
		op = msg_evt_get_op_from_q(&ctx->active_op_q);
		if (!op) {
			pr_warn("get op from activeQ fail\n");
			continue;
		}

		/* TODO: save op history */
		//msg_op_history_save(&ctx->op_history, op);
		msg_evt_set_current_op(ctx, op);
		ret = msg_evt_opid_handler(ctx, &op->op);
		msg_evt_set_current_op(ctx, NULL);

		if (ret)
			pr_warn("opid (0x%x) failed, ret(%d)\n",
							op->op.op_id, ret);

		if (atomic_dec_and_test(&op->ref_count)) {
			/* msg_evt_free_op(ctx) */
			msg_evt_put_op_to_free_queue(ctx, op);
		} else if (op->signal.timeoutValue) {
			op->result = ret;
			complete(&(op->signal.comp));
		}
	}

	ctx->thread_stop = true;
	pr_debug("msg evt thread exists\n");
	return 0;
}

int msg_thread_init(struct msg_thread_ctx *ctx, const char *name,
					const msg_opid_func *func, int op_size)
{
	int r = 0, i;
	struct task_struct *p_thread;

	memset((void *)ctx, 0, sizeof(struct msg_thread_ctx));
	p_thread = ctx->pThread;

	ctx->op_func = func;
	ctx->op_func_size = op_size;

	p_thread = kthread_create(msg_evt_thread,
				ctx, name);

	if (IS_ERR(p_thread)) {
		pr_err("[%s] create thread fail", __func__);
		return -1;
	}

	ctx->pThread = p_thread;

	init_waitqueue_head(&ctx->waitQueue);
	spin_lock_init(&ctx->active_op_q.lock);
	spin_lock_init(&ctx->free_op_q.lock);

	/* Initialize op queue */
	MSG_OP_INIT(&ctx->free_op_q, MSG_THREAD_OP_BUF_SIZE);
	MSG_OP_INIT(&ctx->active_op_q, MSG_THREAD_OP_BUF_SIZE);

	/* Put all to free Q */
	for (i = 0; i < MSG_THREAD_OP_BUF_SIZE; i++) {
		init_completion(&(ctx->op_q_inst[i].signal.comp));
		msg_evt_put_op_to_free_queue(ctx, &(ctx->op_q_inst[i]));
	}

	wake_up_process(p_thread);
	ctx->thread_stop = false;

	return r;
}

int msg_thread_deinit(struct msg_thread_ctx *ctx)
{
	int r;
	unsigned int retry = 0;
	struct task_struct *p_thread = ctx->pThread;

	if ((p_thread) && !IS_ERR_OR_NULL(p_thread)) {
		r = kthread_stop(p_thread);
		if (r) {
			pr_err("thread_stop(0x%p) fail(%d)\n", p_thread, r);
			return -1;
		}
	}

	while (retry < 10 && !ctx->thread_stop) {
		// Waiting for thread to stop
		msleep(20);
		retry++;
	}

	if (retry == 10)
		pr_err("[%s] Fail to stop msg thread\n", __func__);

	memset(ctx, 0, sizeof(struct msg_thread_ctx));

	pr_debug("[%s] DONE\n", __func__);
	return 0;
}
