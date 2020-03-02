/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <adsp_ipi_queue.h>

#include <linux/types.h>
#include <linux/errno.h>

#include <linux/slab.h>         /* needed by kmalloc */

#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#include <linux/delay.h>
#include <linux/jiffies.h>

#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif

#ifdef scp_debug
#undef scp_debug
#endif
#if 0 /* debug only. might make performace degrade */
#define scp_debug(x...) pr_info(x)
#else
#define scp_debug(x...)
#endif


/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define MAX_ADSP_COUNT 1
#define MAX_SCP_MSG_NUM_IN_QUEUE (16)
#define SHARE_BUF_SIZE 288
#define SCP_MSG_BUFFER_SIZE ((SHARE_BUF_SIZE) - 16)

/*
 * =============================================================================
 *                     struct def
 * =============================================================================
 */

struct scp_msg_t {
	uint32_t ipi_id;
	uint8_t buf[SCP_MSG_BUFFER_SIZE]; /* TODO: use ring buf? */
	uint32_t len;
	void (*ipi_handler)(int ipi_id, void *buf, unsigned int len);
};


struct scp_queue_element_t {
	struct scp_msg_t msg;

	spinlock_t element_lock;
	wait_queue_head_t element_wq;

	bool wait_in_thread;
	bool signal_arrival;
	int send_retval;
	uint32_t queue_counter;
};


enum { /* scp_path_t */
	SCP_PATH_A2S = 0, /* AP to SCP */
	SCP_PATH_S2A = 1, /* SCP to AP */
	SCP_NUM_PATH
};


struct scp_msg_queue_t {
	uint32_t opendsp_id;   /* enum opendsp_id */
	uint32_t scp_path;      /* enum scp_dir_t */

	struct scp_queue_element_t element[MAX_SCP_MSG_NUM_IN_QUEUE];

	uint32_t size;
	uint32_t idx_r;
	uint32_t idx_w;

	uint32_t queue_counter;

	spinlock_t queue_lock;
	wait_queue_head_t queue_wq;

	/* scp_send_msg_to_scp() / scp_process_msg_from_scp() */
	int (*scp_process_msg_func)(
		struct scp_msg_queue_t *msg_queue,
		struct scp_msg_t *p_scp_msg);

	struct task_struct *scp_thread_task;
	bool thread_enable;

	bool init;
	bool enable;
};

struct adsp_device_t {
	uint32_t opendsp_id;
	send_handler_t send_handler;
	struct scp_msg_queue_t scp_msg_queue[SCP_NUM_PATH];
};

/*
 * =============================================================================
 *                     private global members
 * =============================================================================
 */

static struct adsp_device_t g_adsp_devices[MAX_ADSP_COUNT];

/*
 * =============================================================================
 *                     utilities
 * =============================================================================
 */

inline bool scp_check_idx_msg_valid(
	const struct scp_msg_queue_t *msg_queue,
	const uint32_t idx_msg);

inline bool scp_check_queue_empty(
	const struct scp_msg_queue_t *msg_queue);

inline bool scp_check_queue_to_be_full(
	const struct scp_msg_queue_t *msg_queue);

inline uint32_t scp_get_num_messages_in_queue(
	const struct scp_msg_queue_t *msg_queue);


#ifdef CONFIG_MTK_AEE_FEATURE
#define IPI_ASSERT(exp) \
	do { \
		if (!(exp)) { \
			aee_kernel_exception_api(__FILE__, \
						 __LINE__, \
						 DB_OPT_DEFAULT, \
						 "[IPI]", \
						 "ASSERT("#exp") fail!!"); \
		} \
	} while (0)
#else
#define IPI_ASSERT(exp) \
	do { \
		if (!(exp)) { \
			pr_notice("ASSERT("#exp")!! \""  __FILE__ "\", %uL\n", \
				  __LINE__); \
		} \
	} while (0)
#endif



/*
 * =============================================================================
 *                     private function declaration
 * =============================================================================
 */

static void scp_dump_msg_in_queue(struct scp_msg_queue_t *msg_queue);

static int scp_push_msg(
	struct scp_msg_queue_t *msg_queue,
	uint32_t ipi_id,
	void *buf,
	uint32_t len,
	void (*ipi_handler)(int ipi_id, void *buf, unsigned int len),
	bool wait_in_thread,
	uint32_t *p_idx_msg,
	uint32_t *p_queue_counter);

static int scp_pop_msg(struct scp_msg_queue_t *msg_queue);

static int scp_front_msg(
	struct scp_msg_queue_t *msg_queue,
	struct scp_msg_t **pp_scp_msg,
	uint32_t *p_idx_msg);


static int scp_get_queue_element(
	struct scp_msg_queue_t *msg_queue,
	struct scp_msg_t **pp_scp_msg,
	uint32_t *p_idx_msg);


static int scp_init_single_msg_queue(
	struct scp_msg_queue_t *msg_queue,
	const uint32_t opendsp_id,
	const uint32_t scp_path);

static int scp_process_msg_thread(void *data);


static int scp_send_msg_to_scp(
	struct scp_msg_queue_t *msg_queue,
	struct scp_msg_t *p_scp_msg);

static int scp_process_msg_from_scp(
	struct scp_msg_queue_t *msg_queue,
	struct scp_msg_t *p_scp_msg);



/*
 * =============================================================================
 *                     utilities
 * =============================================================================
 */

inline bool scp_check_idx_msg_valid(
	const struct scp_msg_queue_t *msg_queue,
	const uint32_t idx_msg)
{
	if (msg_queue == NULL) {
		pr_info("%s(), msg_queue == NULL!! return\n", __func__);
		return false;
	}

	return (idx_msg < msg_queue->size) ? true : false;
}


inline bool scp_check_queue_empty(const struct scp_msg_queue_t *msg_queue)
{
	if (msg_queue == NULL) {
		pr_info("%s(), msg_queue == NULL!! return\n", __func__);
		return false;
	}

	return (msg_queue->idx_r == msg_queue->idx_w);
}


inline bool scp_check_queue_to_be_full(const struct scp_msg_queue_t *msg_queue)
{
	uint32_t idx_w_to_be = 0;

	if (msg_queue == NULL) {
		pr_info("%s(), msg_queue == NULL!! return\n", __func__);
		return false;
	}

	idx_w_to_be = msg_queue->idx_w + 1;
	if (idx_w_to_be == msg_queue->size)
		idx_w_to_be = 0;

	return (idx_w_to_be == msg_queue->idx_r) ? true : false;
}


inline uint32_t scp_get_num_messages_in_queue(
	const struct scp_msg_queue_t *msg_queue)
{
	if (msg_queue == NULL) {
		pr_info("%s(), msg_queue == NULL!! return\n", __func__);
		return 0;
	}

	return (msg_queue->idx_w >= msg_queue->idx_r) ?
	       (msg_queue->idx_w - msg_queue->idx_r) :
	       ((msg_queue->size - msg_queue->idx_r) + msg_queue->idx_w);
}

inline struct adsp_device_t *get_adsp_device_by_id(uint32_t opendsp_id)
{
	int idx = 0;

	for (idx = 0; idx < MAX_ADSP_COUNT; idx++) {
		if (g_adsp_devices[idx].opendsp_id == opendsp_id)
			return &g_adsp_devices[idx];
	}

	return NULL;
}

/*
 *  compatibility interface for old adsp queue implementation
 *
 */
send_handler_t get_send_handler_by_id(uint32_t opendsp_id)
{
	return NULL; // dummy function
}

/*
 * =============================================================================
 *                     create/destroy/init/deinit functions
 * =============================================================================
 */
int scp_ipi_queue_init_ex(uint32_t opendsp_id, send_handler_t send_handler)
{
	static atomic_t opendsp_idx = ATOMIC_INIT(0);
	struct scp_msg_queue_t *msg_queue = NULL;
	int idx_curr  = 0;
	uint32_t scp_path = 0;
	int ret = 0;

	idx_curr = atomic_add_return(1, &opendsp_idx) - 1;

	if (idx_curr >= MAX_ADSP_COUNT) {
		pr_info("%s(), opendsp_id: %u error, adsp count overflow!!\n",
			__func__,
			opendsp_id);
		return -EFAULT;
	}


	g_adsp_devices[idx_curr].opendsp_id = opendsp_id;
	g_adsp_devices[idx_curr].send_handler = send_handler;
	for (scp_path = 0; scp_path < SCP_NUM_PATH; scp_path++) {
		msg_queue = &g_adsp_devices[idx_curr].scp_msg_queue[scp_path];
		ret = scp_init_single_msg_queue(msg_queue,
			g_adsp_devices[idx_curr].opendsp_id,
			scp_path);
		if (ret != 0)
			WARN_ON(1);
	}

	return ret;
}

int scp_ipi_queue_init(uint32_t opendsp_id)
{
	send_handler_t send_handler = get_send_handler_by_id(opendsp_id);

	if (send_handler == NULL) {
		pr_info("%s(), opendsp_id: could not found send handler for dsp %u!!\n",
			__func__,
			opendsp_id);
		return -EFAULT;
	}

	return scp_ipi_queue_init_ex(opendsp_id, send_handler);
}

int scp_flush_msg_queue(uint32_t opendsp_id)
{
	struct scp_msg_queue_t *msg_queue = NULL;
	unsigned long flags = 0;
	struct adsp_device_t *p_adsp_device = NULL;

	p_adsp_device = get_adsp_device_by_id(opendsp_id);

	if (!p_adsp_device) {
		pr_err("%s(), opendsp_id: could not found adsp_device for %u!!\n",
			__func__, opendsp_id);
		return -EFAULT;
	}

	msg_queue = &p_adsp_device->scp_msg_queue[SCP_PATH_A2S];
	msg_queue->enable = false;

	spin_lock_irqsave(&msg_queue->queue_lock, flags);

	while (scp_check_queue_empty(msg_queue) == false) {
		wake_up_interruptible(
			&msg_queue->element[msg_queue->idx_r].element_wq);
		scp_pop_msg(msg_queue);
	}

	spin_unlock_irqrestore(&msg_queue->queue_lock, flags);

	return 0;
}


/*
 * =============================================================================
 *                     public functions
 * =============================================================================
 */

int scp_send_msg_to_queue(
	uint32_t opendsp_id, /* enum opendsp_id */
	uint32_t ipi_id,
	void *buf,
	uint32_t len,
	uint32_t wait_ms)
{
	struct scp_msg_queue_t *msg_queue = NULL;
	struct scp_queue_element_t *p_element = NULL;
	struct adsp_device_t *p_adsp_device = NULL;

	uint32_t idx_msg = 0;
	uint32_t queue_counter = 0;

	int retval = 0;
	unsigned long flags = 0;

	uint32_t try_cnt = 0;
	const uint32_t k_max_try_cnt = 20;
	const uint32_t k_restart_sleep_min_us = 20 * 1000;
	const uint32_t k_restart_sleep_max_us = (k_restart_sleep_min_us + 200);


	scp_debug("%s(+), opendsp_id: %u, ipi_id: %u, buf: %p, len: %u, wait_ms: %u\n",
		  __func__, opendsp_id, ipi_id, buf, len, wait_ms);

	p_adsp_device = get_adsp_device_by_id(opendsp_id);

	if (!p_adsp_device) {
		pr_err("%s(), opendsp_id: could not found adsp_device for %u!!\n",
			__func__, opendsp_id);
		return -EFAULT;
	}
	if (buf == NULL || len > SCP_MSG_BUFFER_SIZE) {
		pr_err("%s(), buf: %p, len: %u!! return\n",
			__func__, buf, len);
		return -EFAULT;
	}


	msg_queue = &p_adsp_device->scp_msg_queue[SCP_PATH_A2S];
	if (msg_queue->enable == false) {
		pr_err("%s(), queue disabled!! return\n", __func__);
		return -1;
	}


	/* NEVER sleep in ISR */
	if (in_interrupt() && wait_ms != 0) {
		pr_info("%s(), in_interrupt()!! wait_ms %u => 0!!\n",
			__func__, wait_ms);
		wait_ms = 0;
	}

	/* push message to queue */
	spin_lock_irqsave(&msg_queue->queue_lock, flags);
	retval = scp_push_msg(msg_queue,
			      ipi_id,
			      buf,
			      len,
			      NULL, /* only for recv msg from scp */
			      (wait_ms != 0),
			      &idx_msg,
			      &queue_counter);
	spin_unlock_irqrestore(&msg_queue->queue_lock, flags);
	if (retval != 0) {
		pr_info("%s(), opendsp_id: %u, push fail!!\n",
			__func__, opendsp_id);
		return retval;
	}

	/* notify queue thread to process it */
	wake_up_interruptible(&msg_queue->queue_wq);

	/* no need to wait */
	if (wait_ms == 0) {
		scp_debug("%s(-), wait_ms == 0, exit\n", __func__);
		return 0;
	}


	/* wait until message processed */
	p_element = &msg_queue->element[idx_msg];
	for (try_cnt = 0; try_cnt < k_max_try_cnt; try_cnt++) {
		retval = wait_event_interruptible_timeout(
				 p_element->element_wq,
				 (p_element->signal_arrival == true ||
				  msg_queue->enable == false),
				 msecs_to_jiffies(wait_ms));

		if (msg_queue->enable == false) {
			pr_debug("%s(), queue disable\n", __func__);
			retval = -1;
			break;
		}
		if (retval > 0) {
			retval = 0;
			break;
		}
		if (retval == 0) { /* timeout; will still send to scp later */
			pr_info("%s(), wait timeout, retval: %d, wait_ms: %u\n",
				__func__, retval, wait_ms);
			break;
		}
		if (retval == -ERESTARTSYS) {
			pr_info("%s(), -ERESTARTSYS, retval: %d, wait_ms: %u\n",
				__func__, retval, wait_ms);
			retval = -EINTR;
			usleep_range(k_restart_sleep_min_us,
				     k_restart_sleep_max_us);
		}
	}

	/* get ipc result */
	spin_lock_irqsave(&p_element->element_lock, flags);
	if (p_element->queue_counter == queue_counter) {
		/* keep retval of scp_send_msg_to_scp */
		if (p_element->signal_arrival == true)
			retval = p_element->send_retval;
		p_element->wait_in_thread = false;
	}
	spin_unlock_irqrestore(&p_element->element_lock, flags);



	scp_debug("%s(-), opendsp_id: %u, ipi_id: %u, buf: %p, len: %u, wait_ms: %u\n",
		  __func__, opendsp_id, ipi_id, buf, len, wait_ms);

	return retval;
}


int scp_dispatch_ipi_hanlder_to_queue(
	uint32_t opendsp_id,
	uint32_t ipi_id,
	void *buf,
	uint32_t len,
	void (*ipi_handler)(int ipi_id, void *buf, unsigned int len))
{
	struct scp_msg_queue_t *msg_queue = NULL;
	struct adsp_device_t *p_adsp_device = NULL;

	uint32_t idx_msg = 0;
	uint32_t queue_counter = 0;

	int retval = 0;
	unsigned long flags = 0;


	scp_debug("%s(+), opendsp_id: %u, ipi_id: %u, buf: %p, len: %u, ipi_handler: %p\n",
		  __func__, opendsp_id, ipi_id, buf, len, ipi_handler);

	p_adsp_device = get_adsp_device_by_id(opendsp_id);

	if (!p_adsp_device) {
		pr_err("%s(), opendsp_id: could not found adsp_device for %u!!\n",
			__func__, opendsp_id);
		return -EFAULT;
	}
	if (buf == NULL || len > SCP_MSG_BUFFER_SIZE) {
		pr_info("%s(), buf: %p, len: %u!! return\n",
			__func__, buf, len);
		return -EFAULT;
	}
	if (ipi_handler == NULL) {
		pr_info("%s(), NULL!! ipi_handler: %p\n",
			__func__, ipi_handler);
		return -EFAULT;
	}

	msg_queue = &p_adsp_device->scp_msg_queue[SCP_PATH_S2A];
	if (msg_queue->enable == false) {
		pr_info("%s(), queue disabled!! return\n", __func__);
		return -1;
	}


	/* push message to queue */
	spin_lock_irqsave(&msg_queue->queue_lock, flags);
	retval = scp_push_msg(msg_queue,
			      ipi_id,
			      buf,
			      len,
			      ipi_handler,
			      false,
			      &idx_msg,
			      &queue_counter);
	spin_unlock_irqrestore(&msg_queue->queue_lock, flags);
	if (retval != 0) {
		pr_info("%s(), opendsp_id: %u, push fail!!\n",
			__func__, opendsp_id);
		return retval;
	}

	/* notify queue thread to process it */
	wake_up_interruptible(&msg_queue->queue_wq);

	scp_debug("%s(-), opendsp_id: %u, ipi_id: %u, buf: %p, len: %u, ipi_handler: %p\n",
		  __func__, opendsp_id, ipi_id, buf, len, ipi_handler);

	return 0;
}



/*
 * =============================================================================
 *                     private implementation
 * =============================================================================
 */

static void scp_dump_msg_in_queue(struct scp_msg_queue_t *msg_queue)
{
	struct scp_msg_t *p_scp_msg = NULL;
	uint32_t idx_dump = msg_queue->idx_r;

	pr_info("%s(), opendsp_id: %u, idx_r: %u, idx_w: %u, queue(%u/%u)\n",
		__func__,
		msg_queue->opendsp_id,
		msg_queue->idx_r,
		msg_queue->idx_w,
		scp_get_num_messages_in_queue(msg_queue),
		msg_queue->size);

	while (idx_dump != msg_queue->idx_w) {
		/* get head msg */
		p_scp_msg = &msg_queue->element[idx_dump].msg;

		pr_info("element[%u], ipi_id: %u, len: %u\n",
			idx_dump, p_scp_msg->ipi_id, p_scp_msg->len);

		/* update dump index */
		idx_dump++;
		if (idx_dump == msg_queue->size)
			idx_dump = 0;
	}
}


static int scp_push_msg(
	struct scp_msg_queue_t *msg_queue,
	uint32_t ipi_id,
	void *buf,
	uint32_t len,
	void (*ipi_handler)(int ipi_id, void *buf, unsigned int len),
	bool wait_in_thread,
	uint32_t *p_idx_msg,
	uint32_t *p_queue_counter)
{
	struct scp_msg_t *p_scp_msg = NULL;
	struct scp_queue_element_t *p_element = NULL;

	unsigned long flags = 0;

	if (msg_queue == NULL || buf == NULL ||
	    p_idx_msg == NULL || p_queue_counter == NULL) {
		pr_info("%s(), NULL!! msg_queue: %p, buf: %p, p_idx_msg: %p, p_queue_counter: %p\n",
			__func__, msg_queue, buf, p_idx_msg, p_queue_counter);
		return -EFAULT;
	}

	/* check queue full */
	if (scp_check_queue_to_be_full(msg_queue) == true) {
		pr_info("opendsp_id: %u, ipi_id: %u, queue overflow, idx_r: %u, idx_w: %u, drop it\n",
			msg_queue->opendsp_id,
			ipi_id, msg_queue->idx_r,
			msg_queue->idx_w);
		scp_dump_msg_in_queue(msg_queue);
		WARN_ON(1);
		return -EOVERFLOW;
	}

	if (scp_check_idx_msg_valid(msg_queue, msg_queue->idx_w) == false) {
		pr_info("%s(), idx_w %u is invalid!! return\n",
			__func__, msg_queue->idx_w);
		return -1;
	}

	/* push */
	*p_idx_msg = msg_queue->idx_w;
	msg_queue->idx_w++;
	if (msg_queue->idx_w == msg_queue->size)
		msg_queue->idx_w = 0;

	/* copy */
	p_element = &msg_queue->element[*p_idx_msg];
	spin_lock_irqsave(&p_element->element_lock, flags);

	p_scp_msg = &p_element->msg;
	p_scp_msg->ipi_id = ipi_id;
	memcpy((void *)p_scp_msg->buf, buf, len);
	p_scp_msg->len = len;
	p_scp_msg->ipi_handler = ipi_handler;

	p_element->wait_in_thread = wait_in_thread;
	p_element->signal_arrival = false;
	p_element->send_retval = 0;
	p_element->queue_counter = msg_queue->queue_counter;

	*p_queue_counter = msg_queue->queue_counter;
	if (msg_queue->queue_counter == 0xFFFFFFFF)
		msg_queue->queue_counter = 0;
	else
		msg_queue->queue_counter++;
	spin_unlock_irqrestore(&p_element->element_lock, flags);

	scp_debug("%s(), opendsp_id: %u, scp_path: %u, ipi_id: %u, idx_r: %u, idx_w: %u, queue(%u/%u), *p_idx_msg: %u\n",
		  __func__,
		  msg_queue->opendsp_id,
		  msg_queue->scp_path,
		  p_scp_msg->ipi_id,
		  msg_queue->idx_r,
		  msg_queue->idx_w,
		  scp_get_num_messages_in_queue(msg_queue),
		  msg_queue->size,
		  *p_idx_msg);

	return 0;
}


static int scp_pop_msg(struct scp_msg_queue_t *msg_queue)
{
	struct scp_msg_t *p_scp_msg = NULL;

	if (msg_queue == NULL) {
		pr_info("%s(), NULL!! msg_queue: %p\n", __func__, msg_queue);
		return -EFAULT;
	}

	/* check queue empty */
	if (scp_check_queue_empty(msg_queue) == true) {
		pr_info("%s(), opendsp_id: %u, queue is empty, idx_r: %u, idx_w: %u\n",
			__func__,
			msg_queue->opendsp_id,
			msg_queue->idx_r,
			msg_queue->idx_w);
		return -1;
	}

	/* pop */
	p_scp_msg = &msg_queue->element[msg_queue->idx_r].msg;
	msg_queue->idx_r++;
	if (msg_queue->idx_r == msg_queue->size)
		msg_queue->idx_r = 0;


	scp_debug("%s(), opendsp_id: %u, scp_path: %u, ipi_id: %u, idx_r: %u, idx_w: %u, queue(%u/%u)\n",
		  __func__,
		  msg_queue->opendsp_id,
		  msg_queue->scp_path,
		  p_scp_msg->ipi_id,
		  msg_queue->idx_r,
		  msg_queue->idx_w,
		  scp_get_num_messages_in_queue(msg_queue),
		  msg_queue->size);

	return 0;
}


static int scp_front_msg(
	struct scp_msg_queue_t *msg_queue,
	struct scp_msg_t **pp_scp_msg,
	uint32_t *p_idx_msg)
{
	if (msg_queue == NULL || pp_scp_msg == NULL || p_idx_msg == NULL) {
		pr_info("%s(), NULL!! msg_queue: %p, pp_scp_msg: %p, p_idx_msg: %p\n",
			__func__, msg_queue, pp_scp_msg, p_idx_msg);
		return -EFAULT;
	}

	*pp_scp_msg = NULL;
	*p_idx_msg = 0xFFFFFFFF;

	/* check queue empty */
	if (scp_check_queue_empty(msg_queue) == true) {
		pr_info("%s(), opendsp_id: %u, queue empty, idx_r: %u, idx_w: %u\n",
			__func__, msg_queue->opendsp_id,
			msg_queue->idx_r, msg_queue->idx_w);
		return -ENOMEM;
	}

	/* front */
	if (scp_check_idx_msg_valid(msg_queue, msg_queue->idx_r) == false) {
		pr_info("%s(), idx_r %u is invalid!! return\n",
			__func__, msg_queue->idx_r);
		return -1;
	}
	*p_idx_msg = msg_queue->idx_r;
	*pp_scp_msg = &msg_queue->element[*p_idx_msg].msg;

#if 0
	scp_debug("%s(), opendsp_id: %u, scp_path: %u, ipi_id: %u, idx_r: %u, idx_w: %u, queue(%u/%u), *p_idx_msg: %u\n",
		  __func__,
		  msg_queue->opendsp_id,
		  msg_queue->scp_path,
		  msg_queue->element[*p_idx_msg].msg.ipi_id,
		  msg_queue->idx_r,
		  msg_queue->idx_w,
		  scp_get_num_messages_in_queue(msg_queue),
		  msg_queue->size,
		  *p_idx_msg);
#endif

	return 0;
}


static int scp_get_queue_element(
	struct scp_msg_queue_t *msg_queue,
	struct scp_msg_t **pp_scp_msg,
	uint32_t *p_idx_msg)
{
	bool is_empty = true;

	unsigned long flags = 0;
	int retval = 0;

	uint32_t try_cnt = 0;
	const uint32_t k_max_try_cnt = 20;
	const uint32_t k_restart_sleep_min_us = 1000;
	const uint32_t k_restart_sleep_max_us = (k_restart_sleep_min_us + 200);

	spin_lock_irqsave(&msg_queue->queue_lock, flags);
	is_empty = scp_check_queue_empty(msg_queue);
	spin_unlock_irqrestore(&msg_queue->queue_lock, flags);

	/* wait until message is pushed to queue */
	if (is_empty == true) {
		for (try_cnt = 0; try_cnt < k_max_try_cnt; try_cnt++) {
			retval = wait_event_interruptible(
					 msg_queue->queue_wq,
					 (!scp_check_queue_empty(msg_queue) ||
					  !msg_queue->thread_enable));
			if (msg_queue->thread_enable == false) {
				pr_debug("%s(), thread disable\n", __func__);
				retval = -1;
				break;
			}
			if (retval == 0) /* got msg in queue */
				break;
			if (retval == -ERESTARTSYS) {
				pr_info("%s(), -ERESTARTSYS, retval: %d\n",
					__func__, retval);
				retval = -EINTR;
				usleep_range(k_restart_sleep_min_us,
					     k_restart_sleep_max_us);
			}
		}
	}

	if (retval == 0) {
		spin_lock_irqsave(&msg_queue->queue_lock, flags);
		retval = scp_front_msg(msg_queue, pp_scp_msg, p_idx_msg);
		spin_unlock_irqrestore(&msg_queue->queue_lock, flags);
	}

	return retval;
}


static int scp_init_single_msg_queue(
	struct scp_msg_queue_t *msg_queue,
	const uint32_t opendsp_id,
	const uint32_t scp_path)
{
	struct scp_queue_element_t *p_element = NULL;
	struct scp_msg_t *p_scp_msg = NULL;

	char thread_name[32] = {0};

	int i = 0;

	if (msg_queue == NULL) {
		pr_info("%s(), NULL!! msg_queue: %p\n", __func__, msg_queue);
		return -EFAULT;
	}

	if (scp_path >= SCP_NUM_PATH) {
		pr_info("%s(), opendsp_id: %u, scp_path: %u error!!\n",
			__func__, opendsp_id, scp_path);
		return -EFAULT;
	}


	/* check double init */
	if (msg_queue->init) {
		pr_info("%s(), opendsp_id: %u already init!!\n",
			__func__, opendsp_id);
		return 0;
	}
	msg_queue->init = true;

	/* init var */
	msg_queue->opendsp_id = opendsp_id;
	msg_queue->scp_path = scp_path;

	for (i = 0; i < MAX_SCP_MSG_NUM_IN_QUEUE; i++) {
		p_element = &msg_queue->element[i];

		p_scp_msg = &p_element->msg;
		p_scp_msg->ipi_id = 0;
		memset((void *)p_scp_msg->buf, 0, SCP_MSG_BUFFER_SIZE);
		p_scp_msg->len = 0;
		p_scp_msg->ipi_handler = NULL;

		spin_lock_init(&p_element->element_lock);
		init_waitqueue_head(&p_element->element_wq);

		p_element->wait_in_thread = false;
		p_element->signal_arrival = false;
		p_element->send_retval = 0;
		p_element->queue_counter = 0;
	}

	msg_queue->size = MAX_SCP_MSG_NUM_IN_QUEUE;
	msg_queue->idx_r = 0;
	msg_queue->idx_w = 0;

	msg_queue->queue_counter = 0;

	spin_lock_init(&msg_queue->queue_lock);
	init_waitqueue_head(&msg_queue->queue_wq);


	if (scp_path == SCP_PATH_A2S) {
		msg_queue->scp_process_msg_func = scp_send_msg_to_scp;
		snprintf(thread_name,
			 sizeof(thread_name),
			 "scp_send_thread_id_%u", opendsp_id);
	} else if (scp_path == SCP_PATH_S2A) {
		msg_queue->scp_process_msg_func = scp_process_msg_from_scp;
		snprintf(thread_name,
			 sizeof(thread_name),
			 "scp_recv_thread_id_%u", opendsp_id);
	} else
		WARN_ON(1);

	/* lunch thread */
	msg_queue->scp_thread_task = kthread_create(
					     scp_process_msg_thread,
					     msg_queue,
					     thread_name);
	if (IS_ERR(msg_queue->scp_thread_task)) {
		pr_info("can not create %s kthread\n", thread_name);
		WARN_ON(1);
		msg_queue->thread_enable = false;
	} else {
		msg_queue->thread_enable = true;
		wake_up_process(msg_queue->scp_thread_task);
	}

	/* enable */
	msg_queue->enable = true;


	return (msg_queue->enable && msg_queue->thread_enable) ? 0 : -EFAULT;
}


static int scp_process_msg_thread(void *data)
{
	struct scp_msg_queue_t *msg_queue = (struct scp_msg_queue_t *)data;
	struct scp_queue_element_t *p_element = NULL;
	struct scp_msg_t *p_scp_msg = NULL;
	uint32_t idx_msg = 0;

	unsigned long flags = 0;
	int retval = 0;

	if (msg_queue == NULL) {
		pr_info("%s(), msg_queue == NULL!! return\n", __func__);
		return -EFAULT;
	}


	while (msg_queue->thread_enable && !kthread_should_stop()) {
		/* wait until element pushed */
		retval = scp_get_queue_element(msg_queue, &p_scp_msg, &idx_msg);
		if (retval != 0) {
			pr_info("%s(), scp_get_queue_element retval %d\n",
				__func__, retval);
			continue;
		}
		p_element = &msg_queue->element[idx_msg];

		/* send to scp */
		retval = msg_queue->scp_process_msg_func(msg_queue, p_scp_msg);
		if (retval != 0)
			WARN_ON(1);

		/* notify element if need */
		spin_lock_irqsave(&p_element->element_lock, flags);
		if (p_element->wait_in_thread == true) {
			p_element->send_retval = retval;
			p_element->signal_arrival = true;
			wake_up_interruptible(&p_element->element_wq);
		}
		spin_unlock_irqrestore(&p_element->element_lock, flags);


		/* pop message from queue */
		spin_lock_irqsave(&msg_queue->queue_lock, flags);
		scp_pop_msg(msg_queue);
		spin_unlock_irqrestore(&msg_queue->queue_lock, flags);
	}

	return 0;
}


static int scp_send_msg_to_scp(
	struct scp_msg_queue_t *msg_queue,
	struct scp_msg_t *p_scp_msg)
{
	const int k_max_try_count = 1500;
	int try_count = 0;
	bool send_fail = false;
	struct adsp_device_t *p_adsp_device = NULL;
	int retval = 0;

	p_adsp_device = get_adsp_device_by_id(msg_queue->opendsp_id);

	if (msg_queue == NULL || p_scp_msg == NULL) {
		pr_info("%s(), NULL!! msg_queue: %p, p_scp_msg: %p\n",
			__func__, msg_queue, p_scp_msg);
		return -EFAULT;
	}

	if (p_adsp_device == NULL) {
		pr_info("%s(), NULL!! p_adsp_device: %p\n",
			__func__, p_adsp_device);
		return -EFAULT;
	}

	for (try_count = 0; try_count < k_max_try_count; try_count++) {
		retval = p_adsp_device->send_handler(
				 p_scp_msg->ipi_id,
				 p_scp_msg->buf,
				 p_scp_msg->len);

		if (retval == 0)
			break;
		send_fail = true;
		msleep(20);
	}
	if (send_fail) {
		pr_info("%s(), opendsp_id %u retry #%d, ret %d\n",
			__func__, msg_queue->opendsp_id,
			try_count, retval);
	}

	IPI_ASSERT(retval == 0);

	return retval;
}


static int scp_process_msg_from_scp(
	struct scp_msg_queue_t *msg_queue,
	struct scp_msg_t *p_scp_msg)
{
	if (msg_queue == NULL || p_scp_msg == NULL) {
		pr_info("%s(), NULL!! msg_queue: %p, p_scp_msg: %p\n",
			__func__, msg_queue, p_scp_msg);
		return -EFAULT;
	}

	if (p_scp_msg->ipi_handler == NULL) {
		pr_info("%s(), NULL!! p_scp_msg->ipi_handler: %p\n",
			__func__, p_scp_msg->ipi_handler);
		return -EFAULT;
	}

	if (p_scp_msg->buf == NULL || p_scp_msg->len == 0) {
		pr_info("%s(), p_scp_msg->buf: %p, p_scp_msg->len: %u\n",
			__func__, p_scp_msg->buf, p_scp_msg->len);
		return -EFAULT;
	}

	/* TODO: add time info here */
	p_scp_msg->ipi_handler(
		p_scp_msg->ipi_id,
		p_scp_msg->buf,
		p_scp_msg->len);

	return 0;
}


