/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2020 The Linux Foundation. All rights reserved. */

/* Neuron Message Queue transport layer header file */
#ifndef __NEURON_MQ_H

#include <linux/sched.h>
#include <linux/wait.h>

/* Current version number */
#define NEURON_SHMEM_CHANNEL_V1 0xcafe0001

/* Shared Memory header definition.
 * It locates on the beginning of the shared memory.
 */
struct neuron_shmem_channel_header {
	/* channel header version */
	u32 version;
	/* field for keeping head value, which is an offset value
	 * against the ring buffer
	 */
	u32 head;
	/* tail offset */
	u32 tail_offset;
	/* field for the expecting length of the next message.*/
	u32 space_for_next;
	/* Maximum message size */
	u32 max_msg_size;
	/* ring buffer offset */
	u32 ring_buffer_offset;
	/* ring buffer lenghth */
	u32 ring_buffer_len;
	/* message alignment bytes */
	u16 message_alignment;
	/* notification flags. Currently unused. */
	u16 notification_flags;
} __packed;

/* Message header. The message payload is somewhere aligned with
 * message_alignment field after this header.
 */
struct neuron_msg_hdr {
	u32 size; // size
} __packed;

/* Packet header size*/
#define PACKET_HEADER_SIZE sizeof(struct neuron_msg_hdr)

/* Message Queue definition */
struct neuron_msg_queue {
	/* Point to field head in shared memory header */
	u32 *headp;
	/* Point to tail offset field in shared memory header */
	u32 *tailp;
	/* Point to space_for_next field in shared memory header */
	u32 *space_for_next_p;
	/* ring buffer address */
	void *ring_buffer_p;
	/* ring buffer length */
	u32 ring_buffer_len;
	/* message alignment bytes */
	u16 message_alignment;
	/* current local offset. It is the head offset for the sender
	 * and the tail offset for the receiver.
	 */
	u32 offset;
};

/* A struct for driver private data */
struct neuron_mq_data_priv {
	/* outgoing vIRQ */
	u32 virq_line;
	/* incoming vIRQ */
	u32 virq;
	/* unused at the moment */
	atomic64_t virq_payload;
	/* A counter to calculate the interrupt received. */
	u32 interrupt_counter;
	/* name of peer vm */
	u32 peer_name;
	/* label to get haven resources like doorbell and shm */
	u32 haven_label;
	/* haven tx doorbell descriptor */
	void *tx_dbl;
	/* haven rx doorbell descriptor */
	void *rx_dbl;
	/* memparcel handle after assigning label to shared memory */
	u32 shm_memparcel;
	/* haven rm status notifier block */
	struct notifier_block rm_nb;
	/* pointer to the device structure */
	struct neuron_channel *dev;
	/* shared memory mapped address */
	void *base;
	/* shared memory resource */
	struct resource buffer;
	/* Flag to show whether the channel is synced. */
	int synced;
	/* The pointer of thread doing the sync work. */
	struct task_struct *sync_thread;
	/* Waiting queue for sync thread */
	wait_queue_head_t wait_q;
	/* Message Queue */
	struct neuron_msg_queue msgq;
};

/* Setting limits for each side. */
static inline int channel_set_limits(struct neuron_channel *channel,
				     struct neuron_msg_queue *msgq)
{
	/* Default limit is one message filling the whole buffer */
	if (!channel->max_size && !channel->queue_length)
		channel->queue_length = 1;

	/* Calculate max size based on given queue length */
	if (!channel->max_size)
		channel->max_size =
			round_down((msgq->ring_buffer_len - 1) /
				   channel->queue_length,
				   msgq->message_alignment) -
				   round_up(PACKET_HEADER_SIZE,
					    msgq->message_alignment);

	/* Calculate queue length based on given max size */
	if (!channel->queue_length)
		channel->queue_length = (msgq->ring_buffer_len - 1) /
			(round_up(PACKET_HEADER_SIZE,
				  msgq->message_alignment) +
			 round_up(channel->max_size,
				  msgq->message_alignment));

	/* Assert that max_size and queue_length are sane. */
	if ((size_t)msgq->ring_buffer_len - 1 <
		channel->queue_length *
			(size_t)(round_up(channel->max_size,
				 msgq->message_alignment) +
				 round_up(PACKET_HEADER_SIZE,
					  msgq->message_alignment))) {
		dev_err(&channel->dev,
			"ring buf size %zu too small for %u * %zu messages\n",
			(size_t)msgq->ring_buffer_len,
			(unsigned int)channel->queue_length,
			channel->max_size);
		return -EINVAL;
	}

	return 0;
}

#endif
