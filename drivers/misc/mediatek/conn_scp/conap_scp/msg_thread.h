/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _CONAP_SCP_MSG_THREAD_H_
#define _CONAP_SCP_MSG_THREAD_H_
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/wait.h>

#define MSG_THREAD_OP_DATA_SIZE   8
#define MSG_THREAD_OP_BUF_SIZE  64

struct msg_op_data {
	unsigned int op_id;		/* Event ID */
	unsigned int info_bit;	/* Reserved */
	size_t op_data[MSG_THREAD_OP_DATA_SIZE];	/* OP Data */
};

struct msg_op_signal {
	struct completion comp;
	unsigned int timeoutValue;
};

struct msg_op {
	struct msg_op_data op;
	struct msg_op_signal signal;
	int result;
	atomic_t ref_count;
};

struct msg_op_q {
	spinlock_t lock;
	unsigned int write;
	unsigned int read;
	unsigned int size;
	struct msg_op *queue[MSG_THREAD_OP_BUF_SIZE];
};

typedef int(*msg_opid_func) (struct msg_op_data *);

struct msg_thread_ctx {
	bool thread_stop;
	struct task_struct *pThread;
	wait_queue_head_t waitQueue;

	struct msg_op_q free_op_q;		/* free op queue */
	struct msg_op_q active_op_q;	/* active op queue */
	struct msg_op op_q_inst[MSG_THREAD_OP_BUF_SIZE];	/* real op instances */
	struct msg_op *cur_op;	/* current op */

	int op_func_size;
	const msg_opid_func *op_func;
};


#define MSG_OP_TIMEOUT 20000

int msg_thread_init(struct msg_thread_ctx *ctx, const char *name,
					const msg_opid_func *func, int op_size);
int msg_thread_deinit(struct msg_thread_ctx *ctx);

/* timeout:
 *    0: default value (by MSG_OP_TIMEOUT define)
 *    >0: cutom timeout (ms)
 */
int msg_thread_send(struct msg_thread_ctx *ctx, int opid);
int msg_thread_send_1(struct msg_thread_ctx *ctx, int opid, size_t param1);
int msg_thread_send_2(struct msg_thread_ctx *ctx, int opid, size_t param1,
					size_t param2);

int msg_thread_send_5(struct msg_thread_ctx *ctx, int opid, size_t param1,
							size_t param2, size_t param3,
							size_t param4, size_t param5);

int msg_thread_send_wait(struct msg_thread_ctx *ctx, int opid, int timeout);
int msg_thread_send_wait_1(struct msg_thread_ctx *ctx, int opid, int timeout, size_t param1);
int msg_thread_send_wait_2(struct msg_thread_ctx *ctx, int opid, int timeout,
								size_t param1, size_t param2);
int msg_thread_send_wait_3(struct msg_thread_ctx *ctx, int opid, int timeout,
							size_t param1, size_t param2,
							size_t param3);
int msg_thread_send_wait_4(struct msg_thread_ctx *ctx, int opid, int timeout,
							size_t param1, size_t param2,
							size_t param3, size_t param4);

#endif				/* _BASE_MSG_THREAD_H_ */
