/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT  */

/*
 * (C) COPYRIGHT 2020-2021 Arm Limited or its affiliates. All rights reserved.
 */


/*
 * Public interface for the handler from Partition Manager messages.
 */

#ifndef _MALI_GPU_RG_AW_MSG_BUFF_H_
#define _MALI_GPU_RG_AW_MSG_BUFF_H_

#include <linux/kernel.h>

#define MAX_AW_NUM				16

/**
 * struct msg_buff - Buffer to store a single PTM message
 * @buff_lock:     Spinlock to be hold while filling or reading the msg
 * @msg:           Payload of the message
 * @retry_count:   Number of retry attempts for this message
 */
struct msg_buff {
	spinlock_t buff_lock;
	uint64_t msg;
	int retry_count;
};

/**
 * struct ptm_msgs - Structure containing message buffers and associated info
 *
 * @msgs:			Pointer to an array of msg_buff containing the
 *				messages
 * @n_buffers:			The number of buffers in msgs
 * @mask:			Flags to indicate which members of msgs are
 *				valid
 * @last_aw_id_processed:	The last member of msgs which was processed
 */
struct ptm_msgs {
	struct msg_buff *msgs;
	int n_buffers;
	unsigned long mask;
	uint32_t last_aw_id_processed;
};

/**
 * struct ptm_msg_handler - Structure containing necessary data for storing and
 * retrieving Partition Manager messages
 *
 *  @dev:		The device to which this handler belongs
 *  @send_msgs:		Send message buffers
 *  @recv_msgs:		Receive message buffers
 *  @base_addr:		The base address of the PTM_MESSAGE registers
 *  @ptm_send_wq:	Send work queue for delayed writes
 *  @ptm_send_work:	Delayed work struct for outgoing messages
 *
 */
struct ptm_msg_handler {
	struct device *dev;
	struct ptm_msgs send_msgs;
	struct ptm_msgs recv_msgs;
	void __iomem *base_addr;
	struct workqueue_struct *ptm_send_wq;
	struct work_struct ptm_send_work;
};

/**
 * struct msg_worker_params - Parameters for a message worker
 * see (ptm_msg_process_msgs)
 *
 *  @data:	Opaque data for worker
 *  @aw_id:	AW ID being processed
 *
 */
struct msg_worker_params {
	void *data;
	int aw_id;
};

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
			int n_buffers);

/**
 * ptm_msg_handler_destroy() - Destroy the contents of a ptm_message_handler
 *				structure
 *
 * @msg_handler:	Pointer to the message handler
 *
 * Destroy any dynamically generated members of the msg_handler structure and
 * destroy the send work queue.
 */
void ptm_msg_handler_destroy(struct ptm_msg_handler *msg_handler);

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
int ptm_msg_buff_read(struct ptm_msgs *msgs, uint32_t buff_id, uint64_t *msg);

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
		uint64_t payload);

/**
 * ptm_msg_buff_retry() - Increment and check the buffer retry counter
 *
 * @msgs:	Pointer to a ptm_msgs struct containing send or receive buffers
 * @buff_id:	Index from which to increment and check the retry counter
 *
 * Return: TRUE if a retry is permissable or FALSE
 */
bool ptm_msg_buff_retry(struct ptm_msgs *msgs, uint32_t buff_id);

/**
 * ptm_msg_write() - Write a PTM_MESSAGE
 *
 * @msg_handler:	Pointer to the message handler
 * @msg_id:		Message index to target. It is equal to the AW_ID if
 *			targeting AW and zero if targeting RG.
 * @message:		64-bit message to send
 *
 * Send a message to the specified register related to the msg_id index. This is
 * simply a raw write and assumes that the status register has already been
 * checked.
 */
void ptm_msg_write(struct ptm_msg_handler *msg_handler,
		uint32_t msg_id,
		uint64_t *message);

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
		uint64_t *message);

/**
 * ptm_msg_flush_send_buffers() - Schedule a worker to flush the send buffers.
 * @msg_handler:	Pointer to the message handler
 *
 * Schedule a message send worker to flush the message sending buffers.
 */
void ptm_msg_flush_send_buffers(struct ptm_msg_handler *msg_handler);

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
int ptm_msg_send(struct ptm_msg_handler *msg_handler, uint32_t msg_id);

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
			void (*worker)(struct msg_worker_params *));

#endif
