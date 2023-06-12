// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
#include <linux/notifier.h>
#include <linux/irqdomain.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <dt-bindings/interrupt-controller/arm-gic.h>

#include <linux/haven/hh_dbl.h>
#include <linux/haven/hh_msgq.h>
#include <linux/haven/hh_errno.h>
#include <linux/haven/hh_common.h>
#include <linux/haven/hh_rm_drv.h>

#include "hh_rm_drv_private.h"

#define HH_RM_MAX_NUM_FRAGMENTS	62

#define GIC_V3_SPI_MAX		1019

#define HH_RM_NO_IRQ_ALLOC	-1

#define HH_RM_MAX_MSG_SIZE_BYTES \
	(HH_MSGQ_MAX_MSG_SIZE_BYTES - sizeof(struct hh_rm_rpc_hdr))

struct hh_rm_connection {
	u32 msg_id;
	u16 seq;
	void *recv_buff;
	u32 reply_err_code;
	size_t recv_buff_size;

	struct completion seq_done;

	u8 num_fragments;
	u8 fragments_received;
	void *current_recv_buff;
};

static struct task_struct *hh_rm_drv_recv_task;
static struct hh_msgq_desc *hh_rm_msgq_desc;

static DEFINE_MUTEX(hh_rm_call_idr_lock);
static DEFINE_IDR(hh_rm_call_idr);
static struct hh_rm_connection *curr_connection;
static DEFINE_MUTEX(hh_rm_send_lock);

static DEFINE_IDA(hh_rm_free_virq_ida);
static struct device_node *hh_rm_intc;
static struct irq_domain *hh_rm_irq_domain;
static u32 hh_rm_base_virq;

SRCU_NOTIFIER_HEAD_STATIC(hh_rm_notifier);

static void hh_rm_get_svm_res_work_fn(struct work_struct *work);
static DECLARE_WORK(hh_rm_get_svm_res_work, hh_rm_get_svm_res_work_fn);

static struct hh_rm_connection *hh_rm_alloc_connection(u32 msg_id)
{
	struct hh_rm_connection *connection;

	connection = kzalloc(sizeof(*connection), GFP_KERNEL);
	if (!connection)
		return ERR_PTR(-ENOMEM);

	init_completion(&connection->seq_done);

	connection->msg_id = msg_id;

	return connection;
}

static int
hh_rm_init_connection_buff(struct hh_rm_connection *connection,
				void *recv_buff, size_t hdr_size,
				size_t payload_size)
{
	struct hh_rm_rpc_hdr *hdr = recv_buff;

	/* Some of the 'reply' types doesn't contain any payload */
	if (!payload_size)
		return 0;

	/* If the data is split into multiple fragments, allocate a large
	 * enough buffer to hold the payloads for all the fragments.
	 */
	connection->recv_buff = connection->current_recv_buff =
		kzalloc((HH_MSGQ_MAX_MSG_SIZE_BYTES - hdr_size) *
			(hdr->fragments + 1),
			GFP_KERNEL);
	if (!connection->recv_buff)
		return -ENOMEM;

	memcpy(connection->recv_buff, recv_buff + hdr_size, payload_size);
	connection->current_recv_buff += payload_size;
	connection->recv_buff_size = payload_size;
	connection->num_fragments = hdr->fragments;

	return 0;
}

int hh_rm_register_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&hh_rm_notifier, nb);
}
EXPORT_SYMBOL(hh_rm_register_notifier);

int hh_rm_unregister_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&hh_rm_notifier, nb);
}
EXPORT_SYMBOL(hh_rm_unregister_notifier);

static struct hh_rm_connection *
hh_rm_wait_for_notif_fragments(void *recv_buff, size_t recv_buff_size)
{
	struct hh_rm_rpc_hdr *hdr = recv_buff;
	struct hh_rm_connection *connection;
	size_t payload_size;
	int ret = 0;

	connection = hh_rm_alloc_connection(hdr->msg_id);
	if (IS_ERR_OR_NULL(connection))
		return connection;

	payload_size = recv_buff_size - sizeof(*hdr);
	curr_connection = connection;

	ret = hh_rm_init_connection_buff(connection, recv_buff,
					sizeof(*hdr), payload_size);
	if (ret < 0)
		goto out;

	if (wait_for_completion_interruptible(&connection->seq_done)) {
		ret = -ERESTARTSYS;
		goto out;
	}

	return connection;

out:
	kfree(connection);
	return ERR_PTR(ret);
}

static int hh_rm_process_notif(void *recv_buff, size_t recv_buff_size)
{
	struct hh_rm_connection *connection = NULL;
	struct hh_rm_rpc_hdr *hdr = recv_buff;
	u32 notification = hdr->msg_id;
	void *payload = NULL;
	int ret = 0;

	pr_debug("Notification received from RM-VM: %x\n", notification);

	if (curr_connection) {
		pr_err("Received new notification from RM-VM before completing last connection\n");
		return -EINVAL;
	}

	if (recv_buff_size > sizeof(*hdr))
		payload = recv_buff + sizeof(*hdr);

	/* If the notification payload is split-up into
	 * fragments, wait until all them arrive.
	 */
	if (hdr->fragments) {
		connection = hh_rm_wait_for_notif_fragments(recv_buff,
								recv_buff_size);
		if (IS_ERR_OR_NULL(connection))
			return PTR_ERR(connection);

		/* In the case of fragments, the complete payload
		 * is contained under connection->recv_buff
		 */
		payload = connection->recv_buff;
		recv_buff_size = connection->recv_buff_size;
	}

	switch (notification) {
	case HH_RM_NOTIF_VM_STATUS:
		if (recv_buff_size != sizeof(*hdr) +
			sizeof(struct hh_rm_notif_vm_status_payload)) {
			pr_err("%s: Invalid size for VM_STATUS notif: %u\n",
				__func__, recv_buff_size - sizeof(*hdr));
			ret = -EINVAL;
			goto err;
		}
		break;
	case HH_RM_NOTIF_VM_IRQ_LENT:
		if (recv_buff_size != sizeof(*hdr) +
			sizeof(struct hh_rm_notif_vm_irq_lent_payload)) {
			pr_err("%s: Invalid size for VM_IRQ_LENT notif: %u\n",
				__func__, recv_buff_size - sizeof(*hdr));
			ret = -EINVAL;
			goto err;
		}
		break;
	case HH_RM_NOTIF_VM_IRQ_RELEASED:
		if (recv_buff_size != sizeof(*hdr) +
			sizeof(struct hh_rm_notif_vm_irq_released_payload)) {
			pr_err("%s: Invalid size for VM_IRQ_REL notif: %u\n",
				__func__, recv_buff_size - sizeof(*hdr));
			ret = -EINVAL;
			goto err;
		}
		break;
	case HH_RM_NOTIF_VM_IRQ_ACCEPTED:
		if (recv_buff_size != sizeof(*hdr) +
			sizeof(struct hh_rm_notif_vm_irq_accepted_payload)) {
			pr_err("%s: Invalid size for VM_IRQ_ACCEPTED notif: %u\n",
				__func__, recv_buff_size - sizeof(*hdr));
			ret = -EINVAL;
			goto err;
		}
		break;
	case HH_RM_NOTIF_MEM_SHARED:
		if (recv_buff_size < sizeof(*hdr) +
			sizeof(struct hh_rm_notif_mem_shared_payload)) {
			pr_err("%s: Invalid size for MEM_SHARED notif: %u\n",
				__func__, recv_buff_size - sizeof(*hdr));
			ret = -EINVAL;
			goto err;
		}
		break;
	case HH_RM_NOTIF_MEM_RELEASED:
		if (recv_buff_size != sizeof(*hdr) +
			sizeof(struct hh_rm_notif_mem_released_payload)) {
			pr_err("%s: Invalid size for MEM_RELEASED notif: %u\n",
				__func__, recv_buff_size - sizeof(*hdr));
			ret = -EINVAL;
			goto err;
		}
		break;
	case HH_RM_NOTIF_MEM_ACCEPTED:
		if (recv_buff_size != sizeof(*hdr) +
			sizeof(struct hh_rm_notif_mem_accepted_payload)) {
			pr_err("%s: Invalid size for MEM_ACCEPTED notif: %u\n",
				__func__, recv_buff_size - sizeof(*hdr));
			ret = -EINVAL;
			goto err;
		}
		break;
	case HH_RM_NOTIF_VM_CONSOLE_CHARS:
		if (recv_buff_size < sizeof(*hdr) +
			sizeof(struct hh_rm_notif_vm_console_chars)) {
			struct hh_rm_notif_vm_console_chars *console_chars;
			u16 num_bytes;

			console_chars = recv_buff + sizeof(*hdr);
			num_bytes = console_chars->num_bytes;

			if (sizeof(*hdr) + sizeof(*console_chars) + num_bytes !=
				recv_buff_size) {
				pr_err("%s: Invalid size for VM_CONSOLE_CHARS notify %u\n",
				       __func__, recv_buff_size - sizeof(*hdr));
				ret = -EINVAL;
				goto err;
			}
		}
		break;
	default:
		pr_err("%s: Unknown notification received: %u\n", __func__,
			notification);
		ret = -EINVAL;
		goto err;
	}

	srcu_notifier_call_chain(&hh_rm_notifier, notification, payload);

err:
	if (connection) {
		kfree(connection->recv_buff);
		kfree(connection);
	}

	return ret;
}

static int hh_rm_process_rply(void *recv_buff, size_t recv_buff_size)
{
	struct hh_rm_rpc_reply_hdr *reply_hdr = recv_buff;
	struct hh_rm_rpc_hdr *hdr = recv_buff;
	struct hh_rm_connection *connection;
	size_t payload_size;
	int ret = 0;

	if (curr_connection) {
		pr_err("Received new reply from RM-VM before completing last connection\n");
		return -EINVAL;
	}

	if (mutex_lock_interruptible(&hh_rm_call_idr_lock)) {
		ret = -ERESTARTSYS;
		goto out;
	}

	connection = idr_find(&hh_rm_call_idr, hdr->seq);
	mutex_unlock(&hh_rm_call_idr_lock);

	if (!connection || connection->seq != hdr->seq ||
	    connection->msg_id != hdr->msg_id) {
		pr_err("%s: Failed to get the connection info for seq: %d\n",
			__func__, hdr->seq);
		ret = -EINVAL;
		goto out;
	}

	payload_size = recv_buff_size - sizeof(*reply_hdr);
	curr_connection = connection;

	ret = hh_rm_init_connection_buff(connection, recv_buff,
					sizeof(*reply_hdr), payload_size);
	if (ret < 0)
		return ret;

	connection->reply_err_code = reply_hdr->err_code;

	/*
	 * If the data is composed of a single message, wakeup the
	 * receiver immediately.
	 *
	 * Else, if the data is split into multiple fragments, fill
	 * this buffer as and when the fragments arrive, and finally
	 * wakeup the receiver upon reception of the last fragment.
	 */
	if (!hdr->fragments) {
		curr_connection = NULL;
		complete(&connection->seq_done);
	}
out:
	return ret;
}

static int hh_rm_process_cont(void *recv_buff, size_t recv_buff_size)
{
	struct hh_rm_rpc_hdr *hdr = recv_buff;
	struct hh_rm_connection *connection = curr_connection;
	size_t payload_size;

	if (!connection) {
		pr_err("%s: not processing a fragmented connection\n",
			__func__);
		return -EINVAL;
	}

	if (connection->msg_id != hdr->msg_id) {
		pr_err("%s: got message id %x when expecting %x\n",
			__func__, hdr->msg_id, connection->msg_id);
	}

	/*
	 * hdr->fragments preserves the value from the first 'reply/notif'
	 * message. For the sake of sanity, check if it's still intact.
	 */
	if (connection->num_fragments != hdr->fragments) {
		pr_err("%s: Number of fragments mismatch for seq: %d\n",
			__func__, hdr->seq);
		return -EINVAL;
	}

	payload_size = recv_buff_size - sizeof(*hdr);

	/* Keep appending the data to the previous fragment's end */
	memcpy(connection->current_recv_buff,
		recv_buff + sizeof(*hdr), payload_size);
	connection->current_recv_buff += payload_size;
	connection->recv_buff_size += payload_size;

	connection->fragments_received++;
	if (connection->fragments_received == connection->num_fragments) {
		curr_connection = NULL;
		complete(&connection->seq_done);
	}

	return 0;
}

struct hh_rm_msgq_data {
	void *recv_buff;
	size_t recv_buff_size;
	struct work_struct recv_work;
};

static void hh_rm_process_recv_work(struct work_struct *work)
{
	struct hh_rm_msgq_data *msgq_data;
	struct hh_rm_rpc_hdr *hdr;
	void *recv_buff;

	msgq_data = container_of(work, struct hh_rm_msgq_data, recv_work);
	recv_buff = hdr = msgq_data->recv_buff;

	switch (hdr->type) {
	case HH_RM_RPC_TYPE_NOTIF:
		hh_rm_process_notif(recv_buff, msgq_data->recv_buff_size);
		break;
	case HH_RM_RPC_TYPE_RPLY:
		hh_rm_process_rply(recv_buff, msgq_data->recv_buff_size);
		break;
	case HH_RM_RPC_TYPE_CONT:
		hh_rm_process_cont(recv_buff, msgq_data->recv_buff_size);
		break;
	default:
		pr_err("%s: Invalid message type (%d) received\n",
			__func__, hdr->type);
	}

	/* All the processing functions would have trimmed-off the header
	 * and copied the data to connection->recv_buff. Hence, it's okay
	 * to release the original packet that arrived and free the msgq_data.
	 */
	kfree(recv_buff);
	kfree(msgq_data);
}

static int hh_rm_recv_task_fn(void *data)
{
	struct hh_rm_msgq_data *msgq_data;
	size_t recv_buff_size;
	void *recv_buff;
	int ret;

	while (!kthread_should_stop()) {
		recv_buff = kzalloc(HH_MSGQ_MAX_MSG_SIZE_BYTES, GFP_KERNEL);
		if (!recv_buff)
			continue;

		/* Block until a new message is received */
		ret = hh_msgq_recv(hh_rm_msgq_desc, recv_buff,
					HH_MSGQ_MAX_MSG_SIZE_BYTES,
					&recv_buff_size, 0);
		if (ret < 0) {
			pr_err("%s: Failed to receive the message: %d\n",
				__func__, ret);
			kfree(recv_buff);
			continue;
		} else if (recv_buff_size <= sizeof(struct hh_rm_rpc_hdr)) {
			pr_err("%s: Invalid message size received\n", __func__);
			kfree(recv_buff);
			continue;
		}

		/* Process this message in the background, while leaving
		 * this thread to receive the next message.
		 */
		msgq_data = kzalloc(sizeof(*msgq_data), GFP_KERNEL);
		if (!msgq_data) {
			kfree(recv_buff);
			continue;
		}

		print_hex_dump_debug("hh_rm_recv: ", DUMP_PREFIX_OFFSET,
				     4, 1, recv_buff, recv_buff_size, false);

		msgq_data->recv_buff = recv_buff;
		msgq_data->recv_buff_size = recv_buff_size;
		INIT_WORK(&msgq_data->recv_work, hh_rm_process_recv_work);

		schedule_work(&msgq_data->recv_work);
	}

	return 0;
}

static int hh_rm_send_request(u32 message_id,
				const void *req_buff, size_t req_buff_size,
				struct hh_rm_connection *connection)
{
	size_t buff_size_remaining = req_buff_size;
	const void *req_buff_curr = req_buff;
	struct hh_rm_rpc_hdr *hdr;
	unsigned long tx_flags;
	u32 num_fragments = 0;
	size_t payload_size;
	void *send_buff;
	int i, ret;

	num_fragments = (req_buff_size + HH_RM_MAX_MSG_SIZE_BYTES - 1) /
			HH_RM_MAX_MSG_SIZE_BYTES;

	/* The above calculation also includes the count
	 * for the 'request' packet. Exclude it as the
	 * header needs to fill the num. of fragments to follow.
	 */
	num_fragments--;

	if (num_fragments > HH_RM_MAX_NUM_FRAGMENTS) {
		pr_err("%s: Limit exceeded for the number of fragments: %u\n",
			__func__, num_fragments);
		return -E2BIG;
	}

	if (mutex_lock_interruptible(&hh_rm_send_lock)) {
		return -ERESTARTSYS;
	}

	/* Consider also the 'request' packet for the loop count */
	for (i = 0; i <= num_fragments; i++) {
		if (buff_size_remaining > HH_RM_MAX_MSG_SIZE_BYTES) {
			payload_size = HH_RM_MAX_MSG_SIZE_BYTES;
			buff_size_remaining -= payload_size;
		} else {
			payload_size = buff_size_remaining;
		}

		send_buff = kzalloc(sizeof(*hdr) + payload_size, GFP_KERNEL);
		if (!send_buff) {
			mutex_unlock(&hh_rm_send_lock);
			return -ENOMEM;
		}

		hdr = send_buff;
		hdr->version = HH_RM_RPC_HDR_VERSION_ONE;
		hdr->hdr_words = HH_RM_RPC_HDR_WORDS;
		hdr->type = i == 0 ? HH_RM_RPC_TYPE_REQ : HH_RM_RPC_TYPE_CONT;
		hdr->fragments = num_fragments;
		hdr->seq = connection->seq;
		hdr->msg_id = message_id;

		memcpy(send_buff + sizeof(*hdr), req_buff_curr, payload_size);
		req_buff_curr += payload_size;

		/* Force the last fragment (or the request type)
		 * to be sent immediately to the receiver
		 */
		tx_flags = (i == num_fragments) ? HH_MSGQ_TX_PUSH : 0;

		/* delay sending console characters to RM */
		if (message_id == HH_RM_RPC_MSG_ID_CALL_VM_CONSOLE_WRITE ||
		    message_id == HH_RM_RPC_MSG_ID_CALL_VM_CONSOLE_FLUSH)
			udelay(800);

		ret = hh_msgq_send(hh_rm_msgq_desc, send_buff,
					sizeof(*hdr) + payload_size, tx_flags);

		/*
		 * In the case of a success, the hypervisor would have consumed
		 * the buffer. While in the case of a failure, we are going to
		 * quit anyways. Hence, free the buffer regardless of the
		 * return value.
		 */
		kfree(send_buff);

		if (ret) {
			mutex_unlock(&hh_rm_send_lock);
			return ret;
		}
	}

	mutex_unlock(&hh_rm_send_lock);
	return 0;
}

/**
 * hh_rm_call: Achieve request-response type communication with RPC
 * @message_id: The RM RPC message-id
 * @req_buff: Request buffer that contains the payload
 * @req_buff_size: Total size of the payload
 * @resp_buff_size: Size of the response buffer
 * @reply_err_code: Returns Haven standard error code for the response
 *
 * Make a request to the RM-VM and expect a reply back. For a successful
 * response, the function returns the payload and its size for the response.
 * Some of the reply types doesn't contain any payload, in which case, the
 * caller would see a NULL returned. Hence, it's recommended that the caller
 * first read the error code and then dereference the returned payload
 * (if applicable). Also, the caller should kfree the returned pointer
 * when done.
 */
void *hh_rm_call(hh_rm_msgid_t message_id,
			void *req_buff, size_t req_buff_size,
			size_t *resp_buff_size, int *reply_err_code)
{
	struct hh_rm_connection *connection;
	int req_ret;
	void *ret;

	if (!message_id || !req_buff || !resp_buff_size || !reply_err_code)
		return ERR_PTR(-EINVAL);

	connection = hh_rm_alloc_connection(message_id);
	if (IS_ERR_OR_NULL(connection))
		return connection;

	/* Allocate a new seq number for this connection */
	if (mutex_lock_interruptible(&hh_rm_call_idr_lock)) {
		kfree(connection);
		return ERR_PTR(-ERESTARTSYS);
	}

	connection->seq = idr_alloc_cyclic(&hh_rm_call_idr, connection,
					0, U16_MAX, GFP_KERNEL);
	mutex_unlock(&hh_rm_call_idr_lock);

	pr_debug("%s TX msg_id: %x\n", __func__, message_id);
	print_hex_dump_debug("hh_rm_call TX: ", DUMP_PREFIX_OFFSET, 4, 1,
			     req_buff, req_buff_size, false);
	/* Send the request to the Resource Manager VM */
	req_ret = hh_rm_send_request(message_id,
					req_buff, req_buff_size,
					connection);
	if (req_ret < 0) {
		ret = ERR_PTR(req_ret);
		goto out;
	}

	/* Wait for response */
	if (wait_for_completion_interruptible(&connection->seq_done)) {
		ret = ERR_PTR(-ERESTARTSYS);
		goto out;
	}

	*reply_err_code = connection->reply_err_code;
	if (connection->reply_err_code) {
		pr_err("%s: Reply for seq:%d failed with RM err: %d\n",
			__func__, connection->seq, connection->reply_err_code);
		ret = ERR_PTR(hh_remap_error(connection->reply_err_code));
		goto out;
	}

	print_hex_dump_debug("hh_rm_call RX: ", DUMP_PREFIX_OFFSET, 4, 1,
			     connection->recv_buff, connection->recv_buff_size,
			     false);

	mutex_lock(&hh_rm_call_idr_lock);
	idr_remove(&hh_rm_call_idr, connection->seq);
	mutex_unlock(&hh_rm_call_idr_lock);

	ret = connection->recv_buff;
	*resp_buff_size = connection->recv_buff_size;

out:
	kfree(connection);
	return ret;
}

/**
 * hh_rm_virq_to_irq: Get a Linux IRQ from a Haven-compatible vIRQ
 * @virq: Haven-compatible vIRQ
 * @type: IRQ trigger type (IRQ_TYPE_EDGE_RISING)
 *
 * Returns the mapped Linux IRQ# at Haven's IRQ domain (i.e. GIC SPI)
 */
int hh_rm_virq_to_irq(u32 virq, u32 type)
{
	struct irq_fwspec fwspec = {};

	if (virq < 32 || virq >= GIC_V3_SPI_MAX) {
		pr_warn("%s: expecting an SPI from RM, but got GIC IRQ %d\n",
			__func__, virq);
	}

	fwspec.fwnode = of_node_to_fwnode(hh_rm_intc);
	fwspec.param_count = 3;
	fwspec.param[0] = GIC_SPI;
	fwspec.param[1] = virq - 32;
	fwspec.param[2] = type;

	return irq_create_fwspec_mapping(&fwspec);
}
EXPORT_SYMBOL(hh_rm_virq_to_irq);

/**
 * hh_rm_irq_to_virq: Get a Haven-compatible vIRQ from a Linux IRQ
 * @irq: Linux-assigned IRQ#
 * @virq: out value where Haven-compatible vIRQ is stored
 *
 * Returns 0 upon success, -EINVAL if the Linux IRQ could not be mapped to
 * a Haven vIRQ (i.e., the IRQ does not correspond to any GIC-level IRQ)
 */
int hh_rm_irq_to_virq(int irq, u32 *virq)
{
	struct irq_data *irq_data;

	irq_data = irq_domain_get_irq_data(hh_rm_irq_domain, irq);
	if (!irq_data)
		return -EINVAL;

	if (virq)
		*virq = irq_data->hwirq;

	return 0;
}
EXPORT_SYMBOL(hh_rm_irq_to_virq);

static int hh_rm_get_irq(struct hh_vm_get_hyp_res_resp_entry *res_entry)
{
	int ret, virq = res_entry->virq;

	/* For resources, such as DBL source, there's no IRQ. The virq_handle
	 * wouldn't be defined for such cases. Hence ignore such cases
	 */
	if (!res_entry->virq_handle && !virq)
		return 0;

	/* Allocate and bind a new IRQ if RM-VM hasn't already done already */
	if (virq == HH_RM_NO_IRQ_ALLOC) {
		/* Get the next free vIRQ.
		 * Subtract 32 from the base virq to get the base SPI.
		 */
		ret = virq = ida_alloc_range(&hh_rm_free_virq_ida,
					hh_rm_base_virq - 32,
					GIC_V3_SPI_MAX, GFP_KERNEL);
		if (ret < 0)
			return ret;

		/* Add 32 offset to make interrupt as hwirq */
		virq += 32;

		/* Bind the vIRQ */
		ret = hh_rm_vm_irq_accept(res_entry->virq_handle, virq);
		if (ret < 0)
			goto err;
	} else if ((virq - 32) < 0) {
		/* Sanity check to make sure hypervisor is passing the correct
		 * interrupt numbers.
		 */
		return -EINVAL;
	}

	return hh_rm_virq_to_irq(virq, IRQ_TYPE_EDGE_RISING);

err:
	ida_free(&hh_rm_free_virq_ida, virq - 32);
	return ret;
}

/**
 * hh_rm_populate_hyp_res: Query Resource Manager VM to get hyp resources.
 * @vmid: The vmid of resources to be queried.
 *
 * The function encodes the error codes via ERR_PTR. Hence, the caller is
 * responsible to check it with IS_ERR_OR_NULL().
 */
int hh_rm_populate_hyp_res(hh_vmid_t vmid)
{
	struct hh_vm_get_hyp_res_resp_entry *res_entries = NULL;
	int linux_irq, ret = 0;
	hh_capid_t cap_id;
	hh_label_t label;
	u32 n_res, i;

	res_entries = hh_rm_vm_get_hyp_res(vmid, &n_res);
	if (IS_ERR_OR_NULL(res_entries))
		return PTR_ERR(res_entries);

	pr_debug("%s: %d Resources are associated with vmid %d\n",
		 __func__, n_res, vmid);

	for (i = 0; i < n_res; i++) {
		pr_debug("%s: idx:%d res_entries.res_type = 0x%x, res_entries.partner_vmid = 0x%x, res_entries.resource_handle = 0x%x, res_entries.resource_label = 0x%x, res_entries.cap_id_low = 0x%x, res_entries.cap_id_high = 0x%x, res_entries.virq_handle = 0x%x, res_entries.virq = 0x%x\n",
			__func__, i,
			res_entries[i].res_type,
			res_entries[i].partner_vmid,
			res_entries[i].resource_handle,
			res_entries[i].resource_label,
			res_entries[i].cap_id_low,
			res_entries[i].cap_id_high,
			res_entries[i].virq_handle,
			res_entries[i].virq);

		ret = linux_irq = hh_rm_get_irq(&res_entries[i]);
		if (ret < 0)
			goto out;

		cap_id = (u64) res_entries[i].cap_id_high << 32 |
				res_entries[i].cap_id_low;
		label = res_entries[i].resource_label;

		/* Populate MessageQ, DBL and vCPUs cap tables */
		do {
			switch (res_entries[i].res_type) {
			case HH_RM_RES_TYPE_MQ_TX:
				ret = hh_msgq_populate_cap_info(label, cap_id,
					HH_MSGQ_DIRECTION_TX, linux_irq);
				break;
			case HH_RM_RES_TYPE_MQ_RX:
				ret = hh_msgq_populate_cap_info(label, cap_id,
					HH_MSGQ_DIRECTION_RX, linux_irq);
				break;
			case HH_RM_RES_TYPE_VCPU:
				ret = hh_vcpu_populate_affinity_info(label,
									cap_id);
				break;
			case HH_RM_RES_TYPE_DB_TX:
				ret = hh_dbl_populate_cap_info(label, cap_id,
					HH_MSGQ_DIRECTION_TX, linux_irq);
				break;
			case HH_RM_RES_TYPE_DB_RX:
				ret = hh_dbl_populate_cap_info(label, cap_id,
					HH_MSGQ_DIRECTION_RX, linux_irq);
				break;
			case HH_RM_RES_TYPE_VPMGRP:
				ret = hh_vpm_grp_populate_info(cap_id,
								linux_irq);
				break;
			default:
				pr_err("%s: Unknown resource type: %u\n",
					__func__, res_entries[i].res_type);
				ret = -EINVAL;
			}
		} while (ret == -EAGAIN);

		if (ret < 0)
			goto out;
	}

out:
	kfree(res_entries);
	return ret;
}
EXPORT_SYMBOL(hh_rm_populate_hyp_res);

static void hh_rm_get_svm_res_work_fn(struct work_struct *work)
{
	hh_vmid_t vmid;
	int ret;

	ret = hh_rm_get_vmid(HH_PRIMARY_VM, &vmid);
	if (ret)
		pr_err("%s: Unable to get VMID for VM label %d\n",
						__func__, HH_PRIMARY_VM);
	else
		hh_rm_populate_hyp_res(vmid);
}

static int hh_vm_probe(struct device *dev, struct device_node *hyp_root)
{
	struct device_node *node;
	struct hh_vm_property temp_property;
	int vmid, owner_vmid, ret;

	node = of_find_compatible_node(hyp_root, NULL, "qcom,haven-vm-id-1.0");
	if (IS_ERR_OR_NULL(node)) {
		dev_err(dev, "Could not find vm-id node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(node, "qcom,vmid", &vmid);
	if (ret) {
		dev_err(dev, "Could not read vmid: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(node, "qcom,owner-vmid", &owner_vmid);
	if (ret) {
		/* We must be HH_PRIMARY_VM */
		temp_property.vmid = vmid;
		hh_update_vm_prop_table(HH_PRIMARY_VM, &temp_property);
	} else {
		/* We must be HH_TRUSTED_VM */
		temp_property.vmid = vmid;
		hh_update_vm_prop_table(HH_TRUSTED_VM, &temp_property);
		temp_property.vmid = owner_vmid;
		hh_update_vm_prop_table(HH_PRIMARY_VM, &temp_property);

		/* Query RM for available resources */
		schedule_work(&hh_rm_get_svm_res_work);
	}

	return 0;
}

static const struct of_device_id hh_rm_drv_of_match[] = {
	{ .compatible = "qcom,resource-manager-1-0" },
	{ }
};

static int hh_rm_drv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	int ret;

	ret = hh_msgq_probe(pdev, HH_MSGQ_LABEL_RM);
	if (ret) {
		dev_err(dev, "Failed to probe message queue: %d\n", ret);
		return ret;
	}

	if (of_property_read_u32(node, "qcom,free-irq-start",
				 &hh_rm_base_virq)) {
		dev_err(dev, "Failed to get the vIRQ base\n");
		return -ENXIO;
	}

	hh_rm_intc = of_irq_find_parent(node);
	if (!hh_rm_intc) {
		dev_err(dev, "Failed to get the IRQ parent node\n");
		return -ENXIO;
	}
	hh_rm_irq_domain = irq_find_host(hh_rm_intc);
	if (!hh_rm_irq_domain) {
		dev_err(dev, "Failed to get IRQ domain associated with RM\n");
		return -ENXIO;
	}

	hh_rm_msgq_desc = hh_msgq_register(HH_MSGQ_LABEL_RM);
	if (IS_ERR_OR_NULL(hh_rm_msgq_desc))
		return PTR_ERR(hh_rm_msgq_desc);

	/* As we don't have a callback for message reception yet,
	 * spawn a kthread and always listen to incoming messages.
	 */
	hh_rm_drv_recv_task = kthread_run(hh_rm_recv_task_fn,
						NULL, "hh_rm_recv_task");
	if (IS_ERR_OR_NULL(hh_rm_drv_recv_task)) {
		ret = PTR_ERR(hh_rm_drv_recv_task);
		goto err_recv_task;
	}

	/* Probe the vmid */
	ret = hh_vm_probe(dev, node->parent);
	if (ret < 0 && ret != -ENODEV)
		goto err_recv_task;

	return 0;

err_recv_task:
	hh_msgq_unregister(hh_rm_msgq_desc);
	return ret;
}

static int hh_rm_drv_remove(struct platform_device *pdev)
{
	kthread_stop(hh_rm_drv_recv_task);
	hh_msgq_unregister(hh_rm_msgq_desc);
	idr_destroy(&hh_rm_call_idr);

	return 0;
}

static struct platform_driver hh_rm_driver = {
	.probe = hh_rm_drv_probe,
	.remove = hh_rm_drv_remove,
	.driver = {
		.name = "hh_rm_driver",
		.of_match_table = hh_rm_drv_of_match,
	},
};

module_platform_driver(hh_rm_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Haven Resource Mgr. Driver");
