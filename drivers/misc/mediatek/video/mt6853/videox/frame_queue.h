// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __DISP_FRAME_QUEUE_H__

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/mutex.h>

struct frame_queue_head_t {
	struct list_head queue;
	struct mutex lock;
	struct task_struct *worker;
	int session_id;
	wait_queue_head_t wq;
	int inited;
};

struct frame_queue_t {
	struct list_head link;
	struct disp_frame_cfg_t frame_cfg;
	int (*do_frame_cfg)(struct frame_queue_t *node);
};

struct frame_queue_head_t *get_frame_queue_head(int session_id);
struct frame_queue_t *frame_queue_node_create(void);
void frame_queue_node_destroy(struct frame_queue_t *node);

int frame_queue_push(struct frame_queue_head_t *head,
	struct frame_queue_t *node);

int frame_queue_wait_all_jobs_done(struct frame_queue_head_t *head);

#endif
