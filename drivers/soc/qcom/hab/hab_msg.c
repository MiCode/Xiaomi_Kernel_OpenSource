/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "hab.h"

static int hab_rx_queue_empty(struct virtual_channel *vchan)
{
	int ret;

	spin_lock_bh(&vchan->rx_lock);
	ret = list_empty(&vchan->rx_list);
	spin_unlock_bh(&vchan->rx_lock);
	return ret;
}

static struct hab_message*
hab_msg_alloc(struct physical_channel *pchan, size_t sizebytes)
{
	struct hab_message *message;

	message = kzalloc(sizeof(*message) + sizebytes, GFP_ATOMIC);
	if (!message)
		return NULL;

	message->sizebytes =
		physical_channel_read(pchan, message->data, sizebytes);

	return message;
}

void hab_msg_free(struct hab_message *message)
{
	kfree(message);
}

struct hab_message *
hab_msg_dequeue(struct virtual_channel *vchan, int wait_flag)
{
	struct hab_message *message = NULL;
	int ret = 0;

	if (wait_flag) {
		if (hab_rx_queue_empty(vchan))
			ret = wait_event_interruptible(vchan->rx_queue,
				!hab_rx_queue_empty(vchan) ||
				vchan->otherend_closed);
	}

	if (!ret && !vchan->otherend_closed) {
		spin_lock_bh(&vchan->rx_lock);
		if (!list_empty(&vchan->rx_list)) {
			message = list_first_entry(&vchan->rx_list,
				struct hab_message, node);
			list_del(&message->node);
		}
		spin_unlock_bh(&vchan->rx_lock);
	}

	return message;
}

static void hab_msg_queue(struct virtual_channel *vchan,
					struct hab_message *message)
{
	spin_lock_bh(&vchan->rx_lock);
	list_add_tail(&message->node, &vchan->rx_list);
	spin_unlock_bh(&vchan->rx_lock);

	wake_up_interruptible(&vchan->rx_queue);
}

static int hab_export_enqueue(struct virtual_channel *vchan,
		struct export_desc *exp)
{
	struct uhab_context *ctx = vchan->ctx;

	spin_lock_bh(&ctx->imp_lock);
	list_add_tail(&exp->node, &ctx->imp_whse);
	ctx->import_total++;
	spin_unlock_bh(&ctx->imp_lock);

	return 0;
}

static int hab_send_export_ack(struct physical_channel *pchan,
		struct export_desc *exp)
{
	struct hab_export_ack exp_ack = {
		.export_id = exp->export_id,
		.vcid_local = exp->vcid_local,
		.vcid_remote = exp->vcid_remote
	};
	struct hab_header header = HAB_HEADER_INITIALIZER;

	HAB_HEADER_SET_SIZE(header, sizeof(exp_ack));
	HAB_HEADER_SET_TYPE(header, HAB_PAYLOAD_TYPE_EXPORT_ACK);
	HAB_HEADER_SET_ID(header, exp->vcid_local);
	return physical_channel_send(pchan, &header, &exp_ack);
}

static int hab_receive_create_export_ack(struct physical_channel *pchan,
		struct uhab_context *ctx)
{
	struct hab_export_ack_recvd *ack_recvd =
		kzalloc(sizeof(*ack_recvd), GFP_ATOMIC);

	if (!ack_recvd)
		return -ENOMEM;

	if (physical_channel_read(pchan,
		&ack_recvd->ack,
		sizeof(ack_recvd->ack)) != sizeof(ack_recvd->ack))
		return -EIO;

	spin_lock_bh(&ctx->expq_lock);
	list_add_tail(&ack_recvd->node, &ctx->exp_rxq);
	spin_unlock_bh(&ctx->expq_lock);

	return 0;
}

void hab_msg_recv(struct physical_channel *pchan,
		struct hab_header *header)
{
	int ret;
	struct hab_message *message;
	struct hab_device *dev = pchan->habdev;
	size_t sizebytes = HAB_HEADER_GET_SIZE(*header);
	uint32_t payload_type = HAB_HEADER_GET_TYPE(*header);
	uint32_t vchan_id = HAB_HEADER_GET_ID(*header);
	struct virtual_channel *vchan = NULL;
	struct export_desc *exp_desc;

	/* get the local virtual channel if it isn't an open message */
	if (payload_type != HAB_PAYLOAD_TYPE_INIT &&
		payload_type != HAB_PAYLOAD_TYPE_INIT_ACK &&
		payload_type != HAB_PAYLOAD_TYPE_ACK) {
		vchan = hab_vchan_get(pchan, vchan_id);
		if (!vchan) {
			return;
		} else if (vchan->otherend_closed) {
			hab_vchan_put(vchan);
			return;
		}
	}

	switch (payload_type) {
	case HAB_PAYLOAD_TYPE_MSG:
		message = hab_msg_alloc(pchan, sizebytes);
		if (!message)
			break;

		hab_msg_queue(vchan, message);
		break;

	case HAB_PAYLOAD_TYPE_INIT:
	case HAB_PAYLOAD_TYPE_INIT_ACK:
	case HAB_PAYLOAD_TYPE_ACK:
		ret = hab_open_request_add(pchan, header);
		if (ret)
			break;
		wake_up_interruptible(&dev->openq);
		break;

	case HAB_PAYLOAD_TYPE_EXPORT:
		exp_desc = kzalloc(sizebytes, GFP_ATOMIC);
		if (!exp_desc)
			break;

		if (physical_channel_read(pchan, exp_desc, sizebytes) !=
			sizebytes) {
			vfree(exp_desc);
			break;
		}

		exp_desc->domid_local = pchan->dom_id;

		hab_export_enqueue(vchan, exp_desc);
		hab_send_export_ack(pchan, exp_desc);
		break;

	case HAB_PAYLOAD_TYPE_EXPORT_ACK:
		ret = hab_receive_create_export_ack(pchan, vchan->ctx);
		if (ret)
			break;

		wake_up_interruptible(&vchan->ctx->exp_wq);
		break;

	case HAB_PAYLOAD_TYPE_CLOSE:
		hab_vchan_stop(vchan);
		break;

	default:
		break;
	}
	if (vchan)
		hab_vchan_put(vchan);
}
