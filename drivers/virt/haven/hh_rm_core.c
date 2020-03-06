// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
#include <linux/completion.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <dt-bindings/interrupt-controller/arm-gic.h>

#include <linux/haven/hh_msgq.h>
#include <linux/haven/hh_errno.h>
#include <linux/haven/hh_common.h>

#include "hh_rm_drv_private.h"

#define HH_RM_MAX_NUM_FRAGMENTS	62

#define GIC_V3_SPI_MAX		1019

#define HH_RM_NO_IRQ_ALLOC	-1

#define HH_RM_MAX_MSG_SIZE_BYTES \
	(HH_MSGQ_MAX_MSG_SIZE_BYTES - sizeof(struct hh_rm_rpc_hdr))

struct hh_rm_connection {
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

static DEFINE_IDA(hh_rm_free_virq_ida);
static struct device_node *hh_rm_intc;
static u32 hh_rm_base_virq;

SRCU_NOTIFIER_HEAD_STATIC(hh_rm_notifier);

static void hh_rm_get_svm_res_work_fn(struct work_struct *work);
static DECLARE_WORK(hh_rm_get_svm_res_work, hh_rm_get_svm_res_work_fn);

static int hh_rm_populate_hyp_res(void);

static struct hh_rm_connection *hh_rm_alloc_connection(int seq)
{
	struct hh_rm_connection *connection;

	connection = kzalloc(sizeof(*connection), GFP_KERNEL);
	if (!connection)
		return ERR_PTR(-ENOMEM);

	init_completion(&connection->seq_done);

	if (seq > 0) {
		connection->seq = seq;
		return connection;
	}

	/* Allocate a new seq number for this connection */
	if (mutex_lock_interruptible(&hh_rm_call_idr_lock)) {
		kfree(connection);
		return ERR_PTR(-ERESTARTSYS);
	}

	connection->seq = idr_alloc_cyclic(&hh_rm_call_idr, connection,
					0, U16_MAX, GFP_KERNEL);
	mutex_unlock(&hh_rm_call_idr_lock);

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

static int hh_rm_process_notif_vm_status(void *recv_buff, size_t recv_buff_size)
{
	struct hh_rm_notif_vm_status_payload *vm_status_payload;

	vm_status_payload = recv_buff + sizeof(struct hh_rm_rpc_hdr);

	/* The VM is now booting. Collect it's info and
	 * populate to other entities such as MessageQ and DBL
	 */
	if (vm_status_payload->vm_status == HH_RM_OS_STATUS_BOOT)
		return hh_rm_populate_hyp_res();

	return 0;
}

static struct hh_rm_connection *
hh_rm_wait_for_notif_fragments(void *recv_buff, size_t recv_buff_size)
{
	struct hh_rm_rpc_hdr *hdr = recv_buff;
	struct hh_rm_connection *connection;
	size_t payload_size;
	int ret = 0;

	connection = hh_rm_alloc_connection(hdr->seq);
	if (IS_ERR_OR_NULL(connection))
		return connection;

	payload_size = recv_buff_size - sizeof(*hdr);

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

		ret = hh_rm_process_notif_vm_status(recv_buff, recv_buff_size);
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

	if (mutex_lock_interruptible(&hh_rm_call_idr_lock)) {
		ret = -ERESTARTSYS;
		goto out;
	}

	connection = idr_find(&hh_rm_call_idr, hdr->seq);
	mutex_unlock(&hh_rm_call_idr_lock);

	if (!connection || connection->seq != hdr->seq) {
		pr_err("%s: Failed to get the connection info for seq: %d\n",
			__func__, hdr->seq);
		ret = -EINVAL;
		goto out;
	}

	payload_size = recv_buff_size - sizeof(*reply_hdr);

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
	if (!hdr->fragments)
		complete(&connection->seq_done);
out:
	return ret;
}

static int hh_rm_process_cont(void *recv_buff, size_t recv_buff_size)
{
	struct hh_rm_rpc_hdr *hdr = recv_buff;
	struct hh_rm_connection *connection;
	size_t payload_size;

	if (mutex_lock_interruptible(&hh_rm_call_idr_lock))
		return -ERESTARTSYS;

	connection = idr_find(&hh_rm_call_idr, hdr->seq);
	mutex_unlock(&hh_rm_call_idr_lock);

	/* The seq number should be the same for all the fragments */
	if (!connection || connection->seq != hdr->seq) {
		pr_err("%s: Failed to get the connection info for seq: %d\n",
			__func__, hdr->seq);
		return -EINVAL;
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
	if (connection->fragments_received == connection->num_fragments)
		complete(&connection->seq_done);

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
	 * to release the original packet that arrived.
	 */
	kfree(recv_buff);
}

static int hh_rm_recv_task_fn(void *data)
{
	struct hh_rm_msgq_data *msgq_data;
	size_t recv_buff_size;
	void *recv_buff;
	int ret;

	while (!kthread_should_stop()) {
		/* Block until a new message is received */
		ret = hh_msgq_recv(hh_rm_msgq_desc, &recv_buff,
					&recv_buff_size, 0);
		if (ret < 0) {
			pr_err("%s: Failed to receive the message: %d\n",
				__func__, ret);
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
	u8 num_fragments = 0;
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

	/* Consider also the 'request' packet for the loop count */
	for (i = 0; i <= num_fragments; i++) {
		if (buff_size_remaining > HH_RM_MAX_MSG_SIZE_BYTES) {
			payload_size = HH_RM_MAX_MSG_SIZE_BYTES;
			buff_size_remaining -= payload_size;
		} else {
			payload_size = buff_size_remaining;
		}

		send_buff = kzalloc(sizeof(*hdr) + payload_size, GFP_KERNEL);
		if (!send_buff)
			return -ENOMEM;

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

		ret = hh_msgq_send(hh_rm_msgq_desc, send_buff,
					sizeof(*hdr) + payload_size, tx_flags);
		if (ret) {
			kfree(send_buff);
			return ret;
		}
	}

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

	connection = hh_rm_alloc_connection(-1);
	if (IS_ERR_OR_NULL(connection))
		return connection;

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
		pr_err("%s: Reply for seq:%d failed with err: %d\n",
			__func__, connection->seq, connection->reply_err_code);
		ret = ERR_PTR(hh_remap_error(connection->reply_err_code));
		goto out;
	}

	ret = connection->recv_buff;
	*resp_buff_size = connection->recv_buff_size;

out:
	kfree(connection);
	return ret;
}

static int hh_rm_virq_to_linux_irq(u32 virq, u32 type, u32 trigger)
{
	struct irq_fwspec fwspec = {};

	fwspec.fwnode = of_node_to_fwnode(hh_rm_intc);
	fwspec.param_count = 3;
	fwspec.param[0] = type;
	fwspec.param[1] = virq;
	fwspec.param[2] = trigger;

	return irq_create_fwspec_mapping(&fwspec);
}

static int hh_rm_get_irq(struct hh_vm_get_hyp_res_resp_entry *res_entry)
{
	int ret, virq = res_entry->virq;

	/* For resources, such as DBL source, there's no IRQ. The virq_handle
	 * wouldn't be defined for such cases. Hence ignore such cases
	 */
	if (!res_entry->virq_handle)
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

		/* Bind the vIRQ */
		ret = hh_rm_vm_irq_accept(res_entry->virq_handle, virq);
		if (ret < 0)
			goto err;
	}

	return hh_rm_virq_to_linux_irq(virq, GIC_SPI, IRQ_TYPE_LEVEL_HIGH);

err:
	ida_free(&hh_rm_free_virq_ida, virq);
	return ret;
}

static int hh_rm_populate_hyp_res(void)
{
	struct hh_vm_get_hyp_res_resp_entry *res_entries = NULL;
	int linux_irq, ret = 0;
	hh_capid_t cap_id;
	hh_label_t label;
	u32 n_res, i;

	res_entries = hh_rm_vm_get_hyp_res(0, &n_res);
	if (IS_ERR_OR_NULL(res_entries))
		return PTR_ERR(res_entries);

	for (i = 0; i < n_res; i++) {
		ret = linux_irq = hh_rm_get_irq(&res_entries[i]);
		if (ret < 0)
			goto out;

		cap_id = (u64) res_entries[i].cap_id_high << 32 |
				res_entries[i].cap_id_low;
		label = res_entries[i].resource_label;

		/* Populate MessageQ & DBL's cap tables */
		/* TODO: Handle DBL */
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
			ret = hh_vcpu_populate_affinity_info(label, cap_id);
			break;
		case HH_RM_RES_TYPE_DB_TX:
			break;
		case HH_RM_RES_TYPE_DB_RX:
			break;
		default:
			pr_err("%s: Unknown resource type: %u\n",
				__func__, res_entries[i].res_type);
			ret = -EINVAL;
		}

		if (ret < 0)
			goto out;
	}

out:
	kfree(res_entries);
	return ret;
}

static void hh_rm_get_svm_res_work_fn(struct work_struct *work)
{
	hh_rm_populate_hyp_res();
}

static const struct of_device_id hh_rm_drv_of_match[] = {
	{ .compatible = "qcom,haven-resource-manager-1-0" },
	{ }
};

static int hh_rm_drv_probe(struct platform_device *pdev)
{
	struct resource *rm_tx_res, *rm_rx_res;
	int tx_irq, rx_irq;
	u32 owner_vmid;
	int ret;

	rm_tx_res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"rm-tx-cap-id");
	if (!rm_tx_res) {
		dev_err(&pdev->dev, "Failed to get the Tx cap-id\n");
		return -EINVAL;
	}

	rm_rx_res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"rm-rx-cap-id");
	if (!rm_rx_res) {
		dev_err(&pdev->dev, "Failed to get the Rx cap-id\n");
		return -EINVAL;
	}

	tx_irq = platform_get_irq_byname(pdev, "rm-tx-irq");
	if (tx_irq < 0) {
		dev_err(&pdev->dev, "Failed to get the Tx IRQ. ret: %d\n",
			tx_irq);
		return tx_irq;
	}

	rx_irq = platform_get_irq_byname(pdev, "rm-rx-irq");
	if (rx_irq < 0) {
		dev_err(&pdev->dev, "Failed to get the Rx IRQ. ret: %d\n",
			rx_irq);
		return rx_irq;
	}

	if (of_property_read_u32(pdev->dev.of_node, "qcom,virq-base",
				&hh_rm_base_virq)) {
		dev_err(&pdev->dev, "Failed to get the vIRQ base\n");
		return -ENXIO;
	}

	hh_rm_intc = of_irq_find_parent(pdev->dev.of_node);
	if (!hh_rm_intc) {
		dev_err(&pdev->dev, "Failed to get the IRQ parent node\n");
		return -ENXIO;
	}

	ret = hh_msgq_populate_cap_info(HH_MSGQ_LABEL_RM, rm_tx_res->start,
					HH_MSGQ_DIRECTION_TX, tx_irq);
	if (ret)
		return ret;

	ret = hh_msgq_populate_cap_info(HH_MSGQ_LABEL_RM, rm_rx_res->start,
					HH_MSGQ_DIRECTION_RX, rx_irq);
	if (ret)
		return ret;

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

	/* If the node has a "qcom,owner-vmid" property, it means that it's
	 * a secondary-VM. Gain the info about it's resources here as there
	 * won't be any explicit notification from the primary-VM.
	 */
	if (!of_property_read_u32(pdev->dev.of_node, "qcom,owner-vmid",
				&owner_vmid)) {
		schedule_work(&hh_rm_get_svm_res_work);
	}

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
