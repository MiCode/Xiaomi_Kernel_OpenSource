/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "audio_ipi_queue.h"

#include <linux/vmalloc.h>

#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#include <linux/delay.h>

#ifdef CONFIG_MTK_AUDIO_CM4_SUPPORT
#include <scp_ipi.h>
#endif

#include "audio_log.h"
#include "audio_assert.h"

#include "audio_ipi_platform.h"

#include "audio_messenger_ipi.h"
#include "audio_task.h"



/*
 * =============================================================================
 *                     log
 * =============================================================================
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[IPI][TASK_Q] %s(), " fmt "\n", __func__


/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define MAX_IPI_MSG_QUEUE_SIZE (16)


/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

struct queue_element_t {
	struct ipi_msg_t *msg;
	wait_queue_head_t wq;
};


struct msg_queue_t {
	uint8_t k_element_size;
	struct queue_element_t element[MAX_IPI_MSG_QUEUE_SIZE];

	uint8_t task_scene; /* task_scene_t */

	uint8_t idx_r;
	uint8_t idx_w;

	spinlock_t rw_lock;

	struct ipi_msg_t ipi_msg_ack;
	spinlock_t ack_lock;

	bool enable;
};


/*
 * =============================================================================
 *                     private global members
 * =============================================================================
 */

static struct ipi_queue_handler_t g_ipi_queue_handler[TASK_SCENE_SIZE];



/*
 * =============================================================================
 *                     private function declaration
 * =============================================================================
 */

static struct msg_queue_t *create_msg_queue(const uint8_t task_scene);
static void destroy_msg_queue(struct msg_queue_t *msg_queue);

static int process_message_in_queue(
	struct msg_queue_t *msg_queue,
	struct ipi_msg_t *p_ipi_msg,
	int idx_msg);


/*
 * =============================================================================
 *                     utilities
 * =============================================================================
 */

inline bool check_queue_empty(const struct msg_queue_t *msg_queue);
inline bool check_queue_to_be_full(const struct msg_queue_t *msg_queue);

inline uint8_t get_num_messages_in_queue(const struct msg_queue_t *msg_queue);

inline int push_msg(
	struct msg_queue_t *msg_queue,
	struct ipi_msg_t *p_ipi_msg);

inline int pop_msg(
	struct msg_queue_t *msg_queue,
	struct ipi_msg_t **pp_ipi_msg);

inline bool check_idx_msg_valid(
	struct msg_queue_t *msg_queue,
	int idx_msg);

inline bool check_ack_msg_valid(
	const struct ipi_msg_t *p_ipi_msg,
	const struct ipi_msg_t *p_ipi_msg_ack);


/*
 * =============================================================================
 *                     create/destroy/init/deinit functions
 * =============================================================================
 */

struct ipi_queue_handler_t *create_ipi_queue_handler(const uint8_t task_scene)
{
	struct ipi_queue_handler_t *handler = NULL;

	/* error handling */
	if (task_scene >= TASK_SCENE_SIZE) {
		pr_info("task_scene %d invalid!! return NULL", task_scene);
		return NULL;
	}

	/* create handler */
	handler = &g_ipi_queue_handler[task_scene];
	AUD_ASSERT(handler != NULL);

	if (handler->msg_queue == NULL) {
		handler->msg_queue = (void *)create_msg_queue(task_scene);
		if (handler->msg_queue == NULL) {
			pr_notice("task_scene %d create fail!!", task_scene);
			return NULL;
		}
	}

	return handler;
}


static struct msg_queue_t *create_msg_queue(const uint8_t task_scene)
{
	struct msg_queue_t *msg_queue = NULL;
	int i = 0;

	/* malloc */
	msg_queue = vmalloc(sizeof(struct msg_queue_t));
	if (msg_queue == NULL)
		return NULL;

	/* init var */
	msg_queue->k_element_size = MAX_IPI_MSG_QUEUE_SIZE;
	for (i = 0; i < msg_queue->k_element_size; i++) {
		msg_queue->element[i].msg = NULL;
		init_waitqueue_head(&msg_queue->element[i].wq);
	}

	msg_queue->task_scene = task_scene;

	msg_queue->idx_r = 0;
	msg_queue->idx_w = 0;

	spin_lock_init(&msg_queue->rw_lock);
	spin_lock_init(&msg_queue->ack_lock);

	memset(&msg_queue->ipi_msg_ack, 0, sizeof(struct ipi_msg_t));

	msg_queue->enable = true;

	return msg_queue;
}


void destroy_msg_queue(struct msg_queue_t *msg_queue)
{
	/* error handling */
	if (msg_queue == NULL) {
		pr_info("msg_queue == NULL!! return");
		return;
	}

	AUD_ASSERT(check_queue_empty(msg_queue));

	/* free */
	vfree(msg_queue);
	msg_queue = NULL;
}


void destroy_ipi_queue_handler(struct ipi_queue_handler_t *handler)
{
	/* error handling */
	if (handler == NULL) {
		pr_info("handler == NULL!! return");
		return;
	}

	/* destroy handler */
	destroy_msg_queue((struct msg_queue_t *)handler->msg_queue);
	handler->msg_queue = NULL;
}


struct ipi_queue_handler_t *get_ipi_queue_handler(const uint8_t task_scene)
{
	/* error handling */
	if (task_scene >= TASK_SCENE_SIZE) {
		pr_info("task_scene %d invalid!! return NULL", task_scene);
		return NULL;
	}

	/* TODO: get/create refine */
	return create_ipi_queue_handler(task_scene);
}


void disable_ipi_queue_handler(struct ipi_queue_handler_t *handler)
{
	struct msg_queue_t *msg_queue = NULL;

	/* error handling */
	if (handler == NULL) {
		pr_info("handler == NULL!! return");
		return;
	}

	msg_queue = (struct msg_queue_t *)handler->msg_queue;
	msg_queue->enable = false;
}


int flush_ipi_queue_handler(struct ipi_queue_handler_t *handler)
{
	struct msg_queue_t *msg_queue = NULL;
	struct ipi_msg_t *p_ipi_msg = NULL;

	const uint16_t k_max_wait_times = 100;
	uint16_t i = 0;

	unsigned long flags = 0;

	/* error handling */
	if (handler == NULL) {
		pr_info("handler == NULL!! return");
		return -1;
	}

	msg_queue = (struct msg_queue_t *)handler->msg_queue;

	spin_lock_irqsave(&msg_queue->rw_lock, flags);
	if (msg_queue->idx_r >= MAX_IPI_MSG_QUEUE_SIZE) {
		pr_info("idx_r %d >= %d(%d)!!", msg_queue->idx_r,
			MAX_IPI_MSG_QUEUE_SIZE, msg_queue->k_element_size);
	} else if (check_queue_empty(msg_queue) == false) {
		p_ipi_msg = msg_queue->element[msg_queue->idx_r].msg;

		if (p_ipi_msg->ack_type == AUDIO_IPI_MSG_NEED_ACK) {
			DUMP_IPI_MSG("fake ack return", p_ipi_msg);
			dsb(SY);
			wake_up_interruptible(
				&msg_queue->element[msg_queue->idx_r].wq);
		}
	}
	spin_unlock_irqrestore(&msg_queue->rw_lock, flags); /* TODO: check */

	for (i = 0; i < k_max_wait_times; i++) {
		if (check_queue_empty(msg_queue))
			break;
		usleep_range(500, 600);
	}

	return 0;
}


/*
 * =============================================================================
 *                     main functions
 * =============================================================================
 */

int send_message(
	struct ipi_queue_handler_t *handler,
	struct ipi_msg_t *p_ipi_msg)
{
	struct msg_queue_t *msg_queue = NULL;
	bool is_queue_empty = false;

	unsigned long flags = 0;

	int idx_msg = -1;
	int retval = 0;

	uint32_t try_cnt = 0;
	const uint32_t k_max_try_cnt = 100; /* retry 1 sec for -ERESTARTSYS */
	const uint32_t k_restart_sleep_min_us = 10 * 1000; /* 10 ms */
	const uint32_t k_restart_sleep_max_us = (k_restart_sleep_min_us + 200);


	/* error handling */
	if (handler == NULL) {
		pr_info("handler == NULL!! return");
		return -1;
	}

	if (p_ipi_msg == NULL) {
		pr_info("p_ipi_msg == NULL!! return");
		return -1;
	}

	if (is_audio_task_dsp_ready(p_ipi_msg->task_scene) == false) {
		pr_info("dsp not ready!! return");
		return -1;
	}

	/* send to scp directly (bypass audio queue, but still in IPC queue) */
	if (p_ipi_msg->ack_type == AUDIO_IPI_MSG_DIRECT_SEND)
		return send_message_to_scp(p_ipi_msg);


	/* send message in queue */
	msg_queue = (struct msg_queue_t *)handler->msg_queue;

	if (msg_queue->enable == false) {
		pr_info("queue disabled!! return");
		return -1;
	}


	spin_lock_irqsave(&msg_queue->rw_lock, flags);
	is_queue_empty = check_queue_empty(msg_queue);
	idx_msg = push_msg(msg_queue, p_ipi_msg);
	spin_unlock_irqrestore(&msg_queue->rw_lock, flags);

	if (check_idx_msg_valid(msg_queue, idx_msg) == false) {
		pr_info("idx_msg %d is invalid!! return", idx_msg);
		return -1;
	}

	/* process queue */
	if (is_queue_empty == true) { /* just send message to scp */
		/* no other working msg ack */
		if (msg_queue->ipi_msg_ack.magic != 0) {
			DUMP_IPI_MSG("ack not clean", &msg_queue->ipi_msg_ack);
			memset(&msg_queue->ipi_msg_ack,
			       0,
			       sizeof(struct ipi_msg_t));
			AUD_ASSERT(0);
		}
		retval = process_message_in_queue(
				 msg_queue, p_ipi_msg, idx_msg);
	} else { /* wait until processed, and then send message to scp */
		for (try_cnt = 0; try_cnt < k_max_try_cnt; try_cnt++) {
			retval = wait_event_interruptible(
					 msg_queue->element[idx_msg].wq,
					 (msg_queue->idx_r == idx_msg ||
					  msg_queue->enable == false));

			if (msg_queue->enable == false) {
				DUMP_IPI_MSG("enable == false", p_ipi_msg);
				retval = 0;
				break;
			}
			if (!is_audio_task_dsp_ready(p_ipi_msg->task_scene)) {
				DUMP_IPI_MSG("dsp not ready", p_ipi_msg);
				return 0;
			}
			if (msg_queue->idx_r == idx_msg) {
				retval = 0;
				break;
			}
			if (retval == 0) {
				pr_notice("wait ret 0, idx %d/%d, enable %d",
					  msg_queue->idx_r,
					  idx_msg,
					  msg_queue->enable);
				break;
			}
			if (retval == -ERESTARTSYS) {
				pr_info("-ERESTARTSYS, #%u, sleep us: %u",
					try_cnt, k_restart_sleep_min_us);
				DUMP_IPI_MSG("wait queue head", p_ipi_msg);
				retval = -EINTR;
				usleep_range(k_restart_sleep_min_us,
					     k_restart_sleep_max_us);
				continue;
			}
			pr_notice("retval: %d not handle!!", retval);
		}

		if (retval != 0) {
			DUMP_IPI_MSG("task queue timeout", p_ipi_msg);
			pr_notice("retval: %d!!", retval);
		} else if (msg_queue->idx_r == idx_msg) {
			retval = process_message_in_queue(
					 msg_queue, p_ipi_msg, idx_msg);
		} else {
			pr_notice("idx %d/%d, enable %d",
				  msg_queue->idx_r,
				  idx_msg,
				  msg_queue->enable);
			DUMP_IPI_MSG("drop msg", p_ipi_msg);
			retval = -1;
		}
	}

	return retval;
}


int send_message_ack(
	struct ipi_queue_handler_t *handler,
	struct ipi_msg_t *p_ipi_msg_ack)
{
	struct msg_queue_t *msg_queue = NULL;
	uint8_t task_scene = 0xFF;
	unsigned long flags = 0;

	/* error handling */
	if (handler == NULL) {
		pr_info("handler == NULL!! return");
		return -1;
	}

	if (p_ipi_msg_ack == NULL) {
		pr_notice("p_ipi_msg_ack = NULL, return");
		return -1;
	}

	if (p_ipi_msg_ack->ack_type != AUDIO_IPI_MSG_ACK_BACK) {
		pr_notice("ack_type %d invalid, return",
			  p_ipi_msg_ack->ack_type);
		return -1;
	}


	/* get info */
	msg_queue = (struct msg_queue_t *)handler->msg_queue;
	task_scene = msg_queue->task_scene;

	if (msg_queue->enable == false) {
		pr_info("queue disabled!! return");
		return -1;
	}


	/* get msg ack & wake up queue */
	spin_lock_irqsave(&msg_queue->ack_lock, flags);
	if (msg_queue->ipi_msg_ack.magic != 0) {
		DUMP_IPI_MSG("previous ack not clean", &msg_queue->ipi_msg_ack);
		DUMP_IPI_MSG("new ack", p_ipi_msg_ack);
		AUD_ASSERT(0);
	}

	memcpy(&msg_queue->ipi_msg_ack,
	       p_ipi_msg_ack,
	       sizeof(struct ipi_msg_t));
	dsb(SY);
	wake_up_interruptible(&msg_queue->element[msg_queue->idx_r].wq);
	spin_unlock_irqrestore(&msg_queue->ack_lock, flags);

	return 0;
}


static int process_message_in_queue(
	struct msg_queue_t *msg_queue,
	struct ipi_msg_t *p_ipi_msg,
	int idx_msg)
{
	struct ipi_msg_t *p_ipi_msg_pop = NULL;
	bool is_queue_empty = false;

	unsigned long flags = 0;
	int retval = 0;

	uint32_t try_cnt = 0;
	const uint32_t k_wait_ms = 10; /* 10 ms */
	const uint32_t k_max_try_cnt = 100; /* retry 1 sec for -ERESTARTSYS */
	const uint32_t k_restart_sleep_min_us = k_wait_ms * 1000;
	const uint32_t k_restart_sleep_max_us = (k_restart_sleep_min_us + 200);

	struct queue_element_t *p_element = NULL;
	struct ipi_msg_t *p_ack = NULL;
	bool timeout_flag = false;

	/* error handling */
	if (msg_queue == NULL) {
		pr_info("msg_queue == NULL!! return");
		return -1;
	}

	if (p_ipi_msg == NULL) {
		pr_info("p_ipi_msg == NULL!! return");
		return -1;
	}

	if (check_idx_msg_valid(msg_queue, idx_msg) == false) {
		pr_info("idx_msg %d is invalid!! return", idx_msg);
		return -1;
	}


	/* process message */
	AUD_ASSERT(idx_msg == msg_queue->idx_r);

	p_element = &msg_queue->element[msg_queue->idx_r];
	p_ack = &msg_queue->ipi_msg_ack;

	switch (p_ipi_msg->ack_type) {
	case AUDIO_IPI_MSG_BYPASS_ACK: {
		/* no need ack, send directly and then just return */
		if (msg_queue->enable == false) {
			DUMP_IPI_MSG("queue disable", p_ipi_msg);
			retval = 0;
			break;
		}
		retval = send_message_to_scp(p_ipi_msg);
		break;
	}
	case AUDIO_IPI_MSG_NEED_ACK: {
		/* need ack, send and then wait until ack back */
		if (msg_queue->enable == false) {
			DUMP_IPI_MSG("queue disable", p_ipi_msg);
			retval = 0;
			break;
		}
		retval = send_message_to_scp(p_ipi_msg);
		if (retval != 0) {
			p_ipi_msg->ack_type = AUDIO_IPI_MSG_CANCELED;
			break;
		}

		/* send to scp succeed, wait ack */
		for (try_cnt = 0; try_cnt < k_max_try_cnt; try_cnt++) {
			retval = wait_event_interruptible_timeout(
					 p_element->wq,
					 p_ack->magic == IPI_MSG_MAGIC_NUMBER ||
					 msg_queue->enable == false,
					 msecs_to_jiffies(k_wait_ms));

			if (msg_queue->enable == false) {
				DUMP_IPI_MSG("queue disable", p_ipi_msg);
				retval = -ENODEV;
				break;
			}
			if (p_ack->magic == IPI_MSG_MAGIC_NUMBER) {
				retval = 0;
				break;
			}
			if (!is_audio_task_dsp_ready(p_ipi_msg->task_scene)) {
				DUMP_IPI_MSG("dsp not ready", p_ipi_msg);
				retval = -ENODEV;
				break;
			}
			if (retval > 0) {
				pr_notice("wait ret %d, enable %d",
					  retval,
					  msg_queue->enable);
				DUMP_IPI_MSG("ack signal?", p_ipi_msg);
				retval = 0;
				break;
			}
			if (retval == 0) { /* 10 ms timeout */
				if (timeout_flag == false) {
					DUMP_IPI_MSG("wait ack", p_ipi_msg);
					timeout_flag = true;
				}
				retval = -ETIMEDOUT;
				continue;
			}
			if (retval == -ERESTARTSYS) {
				DUMP_IPI_MSG("-ERESTARTSYS", p_ipi_msg);
				retval = -EINTR;
				usleep_range(k_restart_sleep_min_us,
					     k_restart_sleep_max_us);
				continue;
			}
			pr_notice("retval: %d not handle!!", retval);
		}

		if (timeout_flag == true)
			pr_info("wait %u x %u ms, retval %d",
				k_wait_ms, try_cnt, retval);

		if (retval == -ENODEV)
			break;
		if (retval == -EINTR) {
			DUMP_IPI_MSG("-EINTR!!", p_ipi_msg);
			DUMP_IPI_MSG("-EINTR!!", p_ack);
			memset(p_ack, 0, sizeof(struct ipi_msg_t));
			break;
		}
		if (retval == -ETIMEDOUT) {
			DUMP_IPI_MSG("timeout!! msg", p_ipi_msg);
			DUMP_IPI_MSG("timeout!! ack", p_ack);
			AUD_ASSERT(0);
			break;
		}

		/* should be in pair */
		spin_lock_irqsave(&msg_queue->ack_lock, flags);
		if (!check_ack_msg_valid(p_ipi_msg, p_ack)) {
			DUMP_IPI_MSG("ack not pair", p_ipi_msg);
			DUMP_IPI_MSG("ack not pair", p_ack);
			memset(p_ack, 0, sizeof(struct ipi_msg_t));
			retval = -1;
			spin_unlock_irqrestore(&msg_queue->ack_lock, flags);
			AUD_ASSERT(0);
			break;
		}

		memcpy(p_ipi_msg, p_ack, sizeof(struct ipi_msg_t));
		memset(p_ack, 0, sizeof(struct ipi_msg_t));
		spin_unlock_irqrestore(&msg_queue->ack_lock, flags);
		retval = 0;
		break;
	}
	default:
		DUMP_IPI_MSG("invalid ack_type", p_ipi_msg);
		retval = -1;
		WARN_ON(1);
		break;
	}


	spin_lock_irqsave(&msg_queue->rw_lock, flags);

	/* pop message from queue */
	pop_msg(msg_queue, &p_ipi_msg_pop);
	AUD_ASSERT(p_ipi_msg_pop == p_ipi_msg);

	/* wake up next message */
	is_queue_empty = check_queue_empty(msg_queue);

	spin_unlock_irqrestore(&msg_queue->rw_lock, flags);


	if (is_queue_empty == false) {
		dsb(SY);
		wake_up_interruptible(&msg_queue->element[msg_queue->idx_r].wq);
	}

	return retval;
}


/*
 * =============================================================================
 *                     utilities
 * =============================================================================
 */

inline bool check_queue_empty(const struct msg_queue_t *msg_queue)
{
	/* error handling */
	if (msg_queue == NULL) {
		pr_info("msg_queue == NULL!! return");
		return false;
	}

	return (msg_queue->idx_r == msg_queue->idx_w);
}


inline bool check_queue_to_be_full(const struct msg_queue_t *msg_queue)
{
	uint8_t idx_w_to_be = 0;

	/* error handling */
	if (msg_queue == NULL) {
		pr_info("msg_queue == NULL!! return");
		return false;
	}

	idx_w_to_be = msg_queue->idx_w + 1;

	if (idx_w_to_be == msg_queue->k_element_size)
		idx_w_to_be = 0;

	return (idx_w_to_be == msg_queue->idx_r) ? true : false;
}


inline uint8_t get_num_messages_in_queue(const struct msg_queue_t *msg_queue)
{
	/* error handling */
	if (msg_queue == NULL) {
		pr_info("msg_queue == NULL!! return");
		return 0;
	}

	return (msg_queue->idx_w >= msg_queue->idx_r)
	       ? (msg_queue->idx_w - msg_queue->idx_r)
	       : ((msg_queue->k_element_size - msg_queue->idx_r) +
		  msg_queue->idx_w);
}


inline int push_msg(struct msg_queue_t *msg_queue, struct ipi_msg_t *p_ipi_msg)
{
	int idx_msg = -1;

	/* error handling */
	if (msg_queue == NULL) {
		pr_info("msg_queue == NULL!! return");
		return -1;
	}

	if (p_ipi_msg == NULL) {
		pr_notice("p_ipi_msg = NULL, return");
		return -1;
	}

	/* check queue full */
	if (check_queue_to_be_full(msg_queue) == true) {
		pr_info("task: %d, queue overflow, idx_w = %d, idx_r = %d",
			p_ipi_msg->task_scene,
			msg_queue->idx_w,
			msg_queue->idx_r);
		DUMP_IPI_MSG("drop msg", p_ipi_msg);
		return -1;
	}


	/* push */
	msg_queue->element[msg_queue->idx_w].msg = p_ipi_msg;
	idx_msg = msg_queue->idx_w;
	msg_queue->idx_w++;
	if (msg_queue->idx_w >= msg_queue->k_element_size &&
	    msg_queue->k_element_size != 0)
		msg_queue->idx_w %= msg_queue->k_element_size;

	AUD_LOG_V(
		"task %d, push msg: 0x%x, idx_msg = %d, idx_r = %d, idx_w = %d",
		p_ipi_msg->task_scene, p_ipi_msg->msg_id,
		idx_msg, msg_queue->idx_r, msg_queue->idx_w);
	AUD_LOG_V(
		"=> queue status(%d/%d)",
		get_num_messages_in_queue(msg_queue),
		msg_queue->k_element_size);

	return idx_msg;
}


inline int pop_msg(struct msg_queue_t *msg_queue, struct ipi_msg_t **pp_ipi_msg)
{
	/* error handling */
	if (msg_queue == NULL) {
		pr_info("msg_queue == NULL!! return");
		return -1;
	}

	if (pp_ipi_msg == NULL) {
		pr_info("pp_ipi_msg == NULL!! return");
		return -1;
	}


	/* check queue empty */
	if (check_queue_empty(msg_queue) == true) {
		pr_info("task: %d, queue is empty, idx_r = %d",
			msg_queue->task_scene, msg_queue->idx_r);
		return -1;
	}

	/* pop */
	*pp_ipi_msg = msg_queue->element[msg_queue->idx_r].msg;
	msg_queue->idx_r++;
	if (msg_queue->idx_r >= msg_queue->k_element_size &&
	    msg_queue->k_element_size != 0)
		msg_queue->idx_r %= msg_queue->k_element_size;


	if (*pp_ipi_msg == NULL) {
		pr_notice("p_ipi_msg = NULL, return");
		return -1;
	}

	AUD_LOG_V("task %d, pop msg: 0x%x, idx_r = %d, idx_w = %d",
		  (*pp_ipi_msg)->task_scene, (*pp_ipi_msg)->msg_id,
		  msg_queue->idx_r, msg_queue->idx_w);
	AUD_LOG_V("=> queue status(%d/%d)",
		  get_num_messages_in_queue(msg_queue),
		  msg_queue->k_element_size);

	return msg_queue->idx_r;
}

inline bool check_idx_msg_valid(struct msg_queue_t *msg_queue, int idx_msg)
{
	return (idx_msg >= 0 &&
		idx_msg < msg_queue->k_element_size)
	       ? true : false;
}


inline bool check_ack_msg_valid(
	const struct ipi_msg_t *p_ipi_msg,
	const struct ipi_msg_t *p_ipi_msg_ack)
{
	return (p_ipi_msg->task_scene == p_ipi_msg_ack->task_scene &&
		p_ipi_msg->msg_id     == p_ipi_msg_ack->msg_id) ? true : false;
}


