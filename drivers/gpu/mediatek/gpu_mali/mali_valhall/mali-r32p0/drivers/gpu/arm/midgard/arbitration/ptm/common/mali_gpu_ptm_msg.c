// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT

/*
 * (C) COPYRIGHT 2020-2021 Arm Limited or its affiliates. All rights reserved.
 */

/*
 * This is a handler from Partition Manager messages. It manages raw reads
 * and writes to the Partition Manager registers, deferred writes if the
 * Send register is busy and message buffering. Messages are currently
 * overwritten if a message is present in the buffer but has not been
 * processed.
 *
 * The same code can be used for both end of the AW<->RG PTM_MESSAGE pipe
 * because the PTM MESSAGE register layout and semantics is identical at
 * both ends.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/iopoll.h>
#include "mali_gpu_ptm_msg.h"
#include "mali_gpu_ptm_message.h"

/* Timeout to poll MSG registers */
#define MSG_REG_POLL_SLEEP_US		100
#define PTM_SEND_RETRY_LIMIT		1000

#define PTM_MESSAGE_OFFSET(aw_id) ((aw_id) * PTM_MESSAGE_SIZE)

static void ptm_send_message_worker(struct work_struct *data);

/**
 * ptm_msg_handler_init() - Initialise a ptm_message_handler structure
 *
 * @msg_handler:	Pointer to the message handler
 * @dev:		Device to which this handler belongs
 * @base_addr:		PTM_MESSAGE base address
 * @n_buffers:		Number of send/receive buffers
 *
 * Populate the msg_handler structure, allocating appropriately sized buffers.
 * Also managed its own work queue.
 *
 * ptm_msg_handler_destroy() needs to be called in order to free resources
 * allocated in this funciton.
 *
 * Return: 0 if successfully or an error code
 */
int ptm_msg_handler_init(struct ptm_msg_handler *msg_handler,
			struct device *dev,
			void __iomem *base_addr,
			int n_buffers)
{
	int msg_buffers_size = sizeof(struct msg_buff) * n_buffers;
	int i = 0;

	*msg_handler = (struct ptm_msg_handler) {
		.dev = dev,
		.send_msgs = {
			.msgs = devm_kzalloc(dev, msg_buffers_size, GFP_KERNEL),
			.n_buffers = n_buffers,
			.mask = 0,
			.last_aw_id_processed = 0
		},
		.recv_msgs = {
			.msgs = devm_kzalloc(dev, msg_buffers_size, GFP_KERNEL),
			.n_buffers = n_buffers,
			.mask = 0,
			.last_aw_id_processed = 0,
		},
		.base_addr = base_addr
	};

	for (i = 0; i < n_buffers; i++) {
		struct msg_buff *msg_buff = &(msg_handler->recv_msgs.msgs[i]);

		spin_lock_init(&(msg_buff->buff_lock));
		msg_buff = &(msg_handler->send_msgs.msgs[i]);
		spin_lock_init(&(msg_buff->buff_lock));
	}

	/* Initialize the PTM send work queue and delayed work */
	msg_handler->ptm_send_wq =
		alloc_ordered_workqueue("ptm_send_wq", WQ_HIGHPRI);
	if (msg_handler->ptm_send_wq)
		INIT_WORK(&msg_handler->ptm_send_work, ptm_send_message_worker);
	else {
		dev_err(dev, "Failed to allocate the AW send work queue.\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * ptm_msg_handler_destroy() - Destroy the contents of a ptm_message_handler
 *				structure
 *
 * @msg_handler:	Pointer to the message handler
 *
 * Destroy any dynamically generated members of the msg_handler structure and
 * destroy the send work queue.
 */
void ptm_msg_handler_destroy(struct ptm_msg_handler *msg_handler)
{
	struct device *dev;

	if (!msg_handler)
		return;

	/* If dev is not initialised then it means ptm_msg_handler_init() has
	 * not been called on msg_handler.
	 */
	dev = msg_handler->dev;
	if (dev) {
		destroy_workqueue(msg_handler->ptm_send_wq);

		devm_kfree(dev, msg_handler->send_msgs.msgs);
		devm_kfree(dev, msg_handler->recv_msgs.msgs);
	}
}

/**
 * ptm_msg_buff_read() - Read and clear the specified message buffer
 *
 * @msgs:	Pointer to a ptm_msgs struct containing send or receive buffers
 * @buff_id:	Index from which to read the message
 * @msg:	Destination pointer for the retrieved message
 *
 * Read the buffer of the message specified by the index and clear the
 * corresponding bit in the mask to indicate that the buffer is empty.
 *
 * Return: 0 if successful or an error code
 */
int ptm_msg_buff_read(struct ptm_msgs *msgs,
			uint32_t buff_id,
			uint64_t *msg)
{
	unsigned long flags;

	if (WARN_ON(!msgs) || WARN_ON(!msg) || buff_id >= msgs->n_buffers)
		return -EINVAL;

	spin_lock_irqsave(&msgs->msgs[buff_id].buff_lock, flags);
	*msg = msgs->msgs[buff_id].msg;
	clear_bit(buff_id, &msgs->mask);
	spin_unlock_irqrestore(&msgs->msgs[buff_id].buff_lock, flags);

	return 0;
}

/**
 * ptm_msg_buff_retry() - Increment and check the buffer retry counter
 *
 * @msgs:	Pointer to a ptm_msgs struct containing send or receive buffers
 * @buff_id:	Index from which to increment and check the retry counter
 *
 * Return: TRUE if a retry is permissable or FALSE
 */
bool ptm_msg_buff_retry(struct ptm_msgs *msgs, uint32_t buff_id)
{
	unsigned long flags;
	int retry_count;

	if (WARN_ON(!msgs) || buff_id >= msgs->n_buffers)
		return -EINVAL;

	spin_lock_irqsave(&msgs->msgs[buff_id].buff_lock, flags);
	retry_count = ++msgs->msgs[buff_id].retry_count;
	spin_unlock_irqrestore(&msgs->msgs[buff_id].buff_lock, flags);

	return (retry_count > PTM_SEND_RETRY_LIMIT);
}

/**
 * ptm_msg_buff_write() - Write the message to the specified message buffer
 *
 * @msgs:	Pointer to a ptm_msgs struct containing send or receive buffers
 * @buff_id:	Index from which to write the message
 * @payload:	The message to be stored
 *
 * Store the the message in the buffer of the message specified by the index and
 * set the corresponding bit in the message bitmask to indicate that the message
 * is valid
 *
 * Return:
 * * 1    - if write was successful but overwrite happened
 * * 0    - if write was successful and no overwrite happened
 * * < 0  - if error
 */
int ptm_msg_buff_write(struct ptm_msgs *msgs,
			uint32_t buff_id,
			uint64_t payload)
{
	unsigned long flags;
	bool overwrite;

	if (WARN_ON(!msgs) || buff_id >= msgs->n_buffers)
		return -EINVAL;

	spin_lock_irqsave(&msgs->msgs[buff_id].buff_lock, flags);
	msgs->msgs[buff_id].msg = payload;
	msgs->msgs[buff_id].retry_count = 0;
	overwrite = test_and_set_bit(buff_id, &msgs->mask);
	spin_unlock_irqrestore(&msgs->msgs[buff_id].buff_lock, flags);

	return overwrite;
}

/**
 * ptm_msg_write() - Write a PTM_MESSAGE
 *
 * @msg_handler:	Pointer to the message handler
 * @msg_id:		Message to target
 * @message:		64-bit message to send
 *
 * Send a message to the specified register related to the msg_id index. This is
 * simply a raw write and assumes that the status register has already been
 * checked.
 */
void ptm_msg_write(struct ptm_msg_handler *msg_handler,
			uint32_t msg_id,
			uint64_t *message)
{
	uint32_t message_lo = *message & U32_MAX;
	uint32_t message_hi = *message >> 32;

	iowrite32(message_lo, msg_handler->base_addr +
				PTM_MESSAGE_OFFSET(msg_id) +
				PTM_OUTGOING_MESSAGE0);

	/* The PTM_OUTGOING_MESSAGE1 write must be last because it triggers
	 * the message copy and raises an interrupt on the recipient.
	 */
	iowrite32(message_hi, msg_handler->base_addr +
				PTM_MESSAGE_OFFSET(msg_id) +
				PTM_OUTGOING_MESSAGE1);
}

/**
 * ptm_msg_read() - Reads the last message received
 *
 * @msg_handler:	Pointer to the message handler
 * @msg_id:		Message index to target. It is equal to the AW_ID if
 *			targeting AW and zero if targeting RG.
 * @message:		Destination pointer for the 64-bit message
 *
 * Receive a message from the specified register related to the msg_id index.
 * This is simply a raw read and assumes that the caller should clear any IRQ
 * related to this message if necessary.
 */
void ptm_msg_read(struct ptm_msg_handler *msg_handler,
		uint32_t msg_id,
		uint64_t *message)
{
	if (!msg_handler || !message)
		return;

	*message = ioread32(msg_handler->base_addr +
			PTM_MESSAGE_OFFSET(msg_id) +
			PTM_INCOMING_MESSAGE1);
	*message <<= 32;
	*message |= ioread32(msg_handler->base_addr +
			PTM_MESSAGE_OFFSET(msg_id) +
			PTM_INCOMING_MESSAGE0);
}

/**
 * ptm_msg_process_msgs() - Process valid messages in the specified buffer
 *
 * @msgs:	Pointer to a ptm_msgs struct containing send or receive buffers
 * @data:	An opaque pointer passed to the worker
 * @worker:	A worker function which is called for every valid message
 *
 * For every message in the buffer which is flagged as being valid, the worker
 * function is called. The prm_msgs structure records the last message
 * processed so that successive calls to this function result in fair
 * processing of messages. It is the worker function's responsibility
 * to retrieve the message with ptm_msg_buff_read() which will clear that
 * message's valid flag
 */
void ptm_msg_process_msgs(struct ptm_msgs *msgs,
			void *data,
			void (*worker)(struct msg_worker_params *))
{
	uint32_t first_aw_id_to_process = msgs->last_aw_id_processed;
	uint32_t mask = 1 << first_aw_id_to_process;

	struct msg_worker_params params = {
		.data = data,
		.aw_id = first_aw_id_to_process
	};

	do {
		if (mask & msgs->mask) {
			(*worker)(&params);

			msgs->last_aw_id_processed = params.aw_id;
		}

		if (++params.aw_id >= MAX_AW_NUM) {
			mask = 1;
			params.aw_id = 0;
		} else
			mask <<= 1;
	} while (params.aw_id != first_aw_id_to_process);
}

/**
 * send_msg_worker() - Send worker function for use with send_msg_worker
 *
 * @params:	Pointer to a msg_worker_params struct containing the data
 *		needed to process a message
 *
 * If the PTM_OUTGOING_MESSAGE_STATUS register is clear, write the message to
 * the send registers
 */
static void send_msg_worker(struct msg_worker_params *params)
{
	uint64_t message;
	uint32_t message_status;
	struct ptm_msg_handler *msg_handler;
	int aw;

	if (WARN_ON(!params) || WARN_ON(!params->data))
		return;

	msg_handler = (struct ptm_msg_handler *)params->data;
	aw = params->aw_id;

	/* If the message should not be retried, remove it from the buffer */
	if (ptm_msg_buff_retry(&msg_handler->send_msgs, aw)) {
		ptm_msg_buff_read(&msg_handler->send_msgs, aw, &message);
		dev_err(msg_handler->dev,
			"Failed to send %llx to AW%d\n", message, aw);
		return;
	}

	message_status = ioread32(msg_handler->base_addr +
		PTM_MESSAGE_OFFSET(aw) +
		PTM_OUTGOING_MESSAGE_STATUS) & PTM_OUTGOING_MSG_STATUS_MASK;

	if (message_status == 0) {
		/* Read the outgoing message */
		ptm_msg_buff_read(&msg_handler->send_msgs, aw, &message);

		ptm_msg_write(msg_handler, aw, &message);
	} else {
		dev_dbg(msg_handler->dev,
		"AW%d send buffer busy. Leaving in queue\n", aw);
	}
}

/**
 * ptm_send_message_worker() - Worker thread for sending messages to
 *				a PTM_MESSAGE pipe
 *
 * @data:	Work contained within the device data.
 *
 * The PTM send register was busy so this work item was scheduled.
 * Check the AW send_msgs and send any valid messages
 */
static void ptm_send_message_worker(struct work_struct *data)
{
	struct ptm_msg_handler *msg_handler;
	int retry;

	if (WARN_ON(!data))
		return;

	msg_handler =
		container_of(data, struct ptm_msg_handler, ptm_send_work);

	for (retry = 0; retry < PTM_SEND_RETRY_LIMIT; retry++) {
		if (msg_handler->send_msgs.mask == 0)
			break;

		/* Process pending outgoing messages */
		ptm_msg_process_msgs(&msg_handler->send_msgs,
					msg_handler,
					send_msg_worker);

		usleep_range(MSG_REG_POLL_SLEEP_US >> 1,
				MSG_REG_POLL_SLEEP_US);
	}

	/* If there are messages left which require processing,
	 * queue another worker
	 */
	if (msg_handler->send_msgs.mask != 0)
		queue_work(msg_handler->ptm_send_wq,
			&msg_handler->ptm_send_work);
}

/**
 * ptm_msg_flush_send_buffers() - Schedule a worker to flush the send buffers.
 * @msg_handler:	Pointer to the message handler
 *
 * Schedule a message send worker to flush the message sending buffers.
 */
void ptm_msg_flush_send_buffers(struct ptm_msg_handler *msg_handler)
{
	if (!msg_handler)
		return;

	queue_work(msg_handler->ptm_send_wq, &msg_handler->ptm_send_work);
}

/**
 * ptm_msg_send() - Send message to the other end (AW or RG).
 *
 * @msg_handler:	Pointer to the message handler
 * @msg_id:		Message index to target. It is equal to the AW_ID if
 *			targeting AW and zero if targeting RG.
 *
 * Send a message payload to  the specified register related to the msg_id index
 * It could be AW or RG depending on the msg_handler passed.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
int ptm_msg_send(struct ptm_msg_handler *msg_handler, uint32_t msg_id)
{
	uint32_t msg_status;
	uint64_t payload;
	int error = 0;

	if (!msg_handler)
		return -EINVAL;

	msg_status = ioread32(msg_handler->base_addr +
		PTM_MESSAGE_OFFSET(msg_id) +
		PTM_OUTGOING_MESSAGE_STATUS)
			& PTM_OUTGOING_MSG_STATUS_MASK;

	if (msg_status != 0) {
		queue_work(msg_handler->ptm_send_wq,
				&msg_handler->ptm_send_work);
	} else {
		error = ptm_msg_buff_read(&msg_handler->send_msgs, msg_id,
			&payload);
		if (!error)
			ptm_msg_write(msg_handler, msg_id, &payload);
	}

	return error;
}
