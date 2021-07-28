/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <linux/completion.h>

#ifndef _ENGINE_REQUESTS_H_
#define _ENGINE_REQUESTS_H_

#define MAX_REQUEST_SIZE_PER_ENGINE 4
#define MAX_FRAMES_PER_REQUEST 16

/* Regulation Options */
#define FRAME_BASE_REGULATION 0
#define REQUEST_BASE_REGULATION 1
#define REQUEST_REGULATION FRAME_BASE_REGULATION

enum FRAME_STATUS_ENUM {
	FRAME_STATUS_EMPTY,	/* 0 */
	FRAME_STATUS_ENQUE,	/* 1 */
	FRAME_STATUS_RUNNING,	/* 2 */
	FRAME_STATUS_FINISHED,	/* 3 */
	FRAME_STATUS_TOTAL
};

enum REQUEST_STATE_ENUM {
	REQUEST_STATE_EMPTY,	/* 0 */
	REQUEST_STATE_PENDING,	/* 1 */
	REQUEST_STATE_RUNNING,	/* 2 */
	REQUEST_STATE_FINISHED,	/* 3 */
	REQUEST_STATE_TOTAL
};

struct ring_ctrl {
	unsigned int wcnt; /* enque */
	unsigned int rcnt; /* deque */
	unsigned int icnt; /* IRQ ringing counter w/o reset */
	unsigned int gcnt; /* GCE */
	unsigned int size;
	/* init MAX_REQUEST_SIZE_PER_ENGINE MAX_FRAMES_PER_REQUEST */
};

struct frame {
	enum FRAME_STATUS_ENUM state;
	void *data; /* points to engine data */
};

struct rsc_request {
	enum REQUEST_STATE_ENUM state;
	pid_t pid;
	struct ring_ctrl fctl;
	struct frame frames[MAX_FRAMES_PER_REQUEST];
	bool pending_run; /* pending frame in a running request */
};

struct engine_ops {
	int (*req_enque_cb)(struct frame *frames, void *req);
	int (*req_deque_cb)(struct frame *frames, void *req);
	int (*frame_handler)(struct frame *frame);
	int (*req_feedback_cb)(struct frame *frame);
};

struct engine_requests {
	struct ring_ctrl req_ctl;
	struct rsc_request reqs[MAX_REQUEST_SIZE_PER_ENGINE];
	const struct engine_ops *ops;
	struct completion req_handler_done;

	bool req_running;
	seqlock_t seqlock;
};

signed int init_ring_ctl(struct ring_ctrl *rctl);
signed int init_request(struct rsc_request *req);
signed int set_frame_data(struct frame *f, void *engine);
signed int register_requests(struct engine_requests *eng, size_t size);
signed int unregister_requests(struct engine_requests *eng);
signed int request_handler(struct engine_requests *eng, spinlock_t *lock);


/*TODO: APIs to manipulate requests  */
int set_engine_ops(struct engine_requests *eng, const struct engine_ops *ops);

signed int enque_request(struct engine_requests *eng, unsigned int fcnt,
							void *req, pid_t pid);
signed int deque_request(struct engine_requests *eng, unsigned int *fcnt,
								void *req);
int update_request(struct engine_requests *eng, pid_t *pid);
bool request_running(struct engine_requests *eng);

signed int request_dump(struct engine_requests *eng);
#endif
