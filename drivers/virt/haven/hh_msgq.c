// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/ratelimit.h>

#include <linux/haven/hcall.h>
#include <linux/haven/hh_msgq.h>
#include <linux/haven/hh_errno.h>

/* HVC call specific mask: 0 to 31 */
#define HH_MSGQ_HVC_FLAGS_MASK GENMASK_ULL(31, 0)

struct hh_msgq_desc {
	enum hh_msgq_label label;
};

struct hh_msgq_cap_table {
	struct hh_msgq_desc *client_desc;
	spinlock_t cap_entry_lock;

	hh_capid_t tx_cap_id;
	hh_capid_t rx_cap_id;
	int tx_irq;
	int rx_irq;
	const char *tx_irq_name;
	const char *rx_irq_name;
	spinlock_t tx_lock;
	spinlock_t rx_lock;

	bool tx_full;
	bool rx_empty;
	wait_queue_head_t tx_wq;
	wait_queue_head_t rx_wq;
};

static bool hh_msgq_initialized;
static struct hh_msgq_cap_table hh_msgq_cap_table[HH_MSGQ_LABEL_MAX];

static irqreturn_t hh_msgq_rx_isr(int irq, void *dev)
{
	struct hh_msgq_cap_table *cap_table_entry = dev;

	spin_lock(&cap_table_entry->rx_lock);
	cap_table_entry->rx_empty = false;
	spin_unlock(&cap_table_entry->rx_lock);

	wake_up_interruptible(&cap_table_entry->rx_wq);

	return IRQ_HANDLED;
}

static irqreturn_t hh_msgq_tx_isr(int irq, void *dev)
{
	struct hh_msgq_cap_table *cap_table_entry = dev;

	spin_lock(&cap_table_entry->tx_lock);
	cap_table_entry->tx_full = false;
	spin_unlock(&cap_table_entry->tx_lock);

	wake_up_interruptible(&cap_table_entry->tx_wq);

	return IRQ_HANDLED;
}

static int __hh_msgq_recv(struct hh_msgq_cap_table *cap_table_entry,
				void *buff, size_t buff_size,
				size_t *recv_size, u64 rx_flags)
{
	struct hh_hcall_msgq_recv_resp resp = {};
	unsigned long flags;
	int hh_ret;
	int ret = 0;

	/* Discard the driver specific flags, and keep only HVC specifics */
	rx_flags &= HH_MSGQ_HVC_FLAGS_MASK;

	spin_lock_irqsave(&cap_table_entry->rx_lock, flags);
	hh_ret = hh_hcall_msgq_recv(cap_table_entry->rx_cap_id, buff,
					buff_size, &resp);

	switch (hh_ret) {
	case HH_ERROR_OK:
		*recv_size = resp.recv_size;
		cap_table_entry->rx_empty = !resp.not_empty;
		ret = 0;
		break;
	case HH_ERROR_MSGQUEUE_EMPTY:
		cap_table_entry->rx_empty = true;
		ret = -EAGAIN;
		break;
	default:
		ret = hh_remap_error(hh_ret);
	}

	spin_unlock_irqrestore(&cap_table_entry->rx_lock, flags);

	if (ret != 0 && ret != -EAGAIN)
		pr_err("%s: Failed to recv from msgq. Hypercall error: %d\n",
			__func__, hh_ret);

	return ret;
}

/**
 * hh_msgq_recv: Receive a message from the client running on a different VM
 * @client_desc: The client descriptor that was obtained via hh_msgq_register()
 * @buff: Pointer to the buffer where the received data must be placed
 * @buff_size: The size of the buffer space available
 * @recv_size: The actual amount of data that is copied into buff
 * @flags: Optional flags to pass to receive the data. For the list of flags,
 *         see linux/haven/hh_msgq.h
 *
 * The function returns 0 if the data is successfully received and recv_size
 * would contain the actual amount of data copied into buff.
 * It returns -EINVAL if the caller passes invalid arguments, -EAGAIN
 * if the message queue is not yet ready to communicate, and -EPERM if the
 * caller doesn't have permissions to receive the data. In all these failure
 * cases, recv_size is unmodified.
 *
 * Note: this function may sleep and should not be called from interrupt
 *       context
 */
int hh_msgq_recv(void *msgq_client_desc,
			void *buff, size_t buff_size,
			size_t *recv_size, unsigned long flags)
{
	struct hh_msgq_desc *client_desc = msgq_client_desc;
	struct hh_msgq_cap_table *cap_table_entry;
	int ret;

	if (!client_desc || !buff || !buff_size || !recv_size)
		return -EINVAL;

	if (buff_size > HH_MSGQ_MAX_MSG_SIZE_BYTES)
		return -E2BIG;

	cap_table_entry = &hh_msgq_cap_table[client_desc->label];

	spin_lock(&cap_table_entry->cap_entry_lock);

	if (cap_table_entry->client_desc != client_desc) {
		pr_err("%s: Invalid client descriptor\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	if ((cap_table_entry->rx_cap_id == HH_CAPID_INVAL) &&
		(flags & HH_MSGQ_NONBLOCK)) {
		pr_err_ratelimited(
			"%s: Recv info for label %d not yet initialized\n",
			__func__, client_desc->label);
		ret = -EAGAIN;
		goto err;
	}

	spin_unlock(&cap_table_entry->cap_entry_lock);

	if (wait_event_interruptible(cap_table_entry->rx_wq,
				cap_table_entry->rx_cap_id != HH_CAPID_INVAL))
		return -ERESTARTSYS;

	spin_lock(&cap_table_entry->cap_entry_lock);

	if (!cap_table_entry->rx_irq) {
		pr_err_ratelimited("%s: Rx IRQ for label %d not yet setup\n",
			__func__, client_desc->label);
		ret = -EAGAIN;
		goto err;
	}

	spin_unlock(&cap_table_entry->cap_entry_lock);

	do {
		if (cap_table_entry->rx_empty && (flags & HH_MSGQ_NONBLOCK))
			return -EAGAIN;

		if (wait_event_interruptible(cap_table_entry->rx_wq,
					!cap_table_entry->rx_empty))
			return -ERESTARTSYS;

		ret = __hh_msgq_recv(cap_table_entry, buff, buff_size,
					recv_size, flags);
	} while (ret == -EAGAIN);

	if (!ret)
		print_hex_dump_debug("hh_msgq_recv: ", DUMP_PREFIX_OFFSET,
				     4, 1, buff, *recv_size, false);

	return ret;

err:
	spin_unlock(&cap_table_entry->cap_entry_lock);
	return ret;
}
EXPORT_SYMBOL(hh_msgq_recv);

static int __hh_msgq_send(struct hh_msgq_cap_table *cap_table_entry,
				void *buff, size_t size, u64 tx_flags)
{
	struct hh_hcall_msgq_send_resp resp = {};
	unsigned long flags;
	int hh_ret;
	int ret = 0;

	/* Discard the driver specific flags, and keep only HVC specifics */
	tx_flags &= HH_MSGQ_HVC_FLAGS_MASK;

	print_hex_dump_debug("hh_msgq_send: ", DUMP_PREFIX_OFFSET,
			     4, 1, buff, size, false);

	spin_lock_irqsave(&cap_table_entry->tx_lock, flags);
	hh_ret = hh_hcall_msgq_send(cap_table_entry->tx_cap_id,
					size, buff, tx_flags, &resp);

	switch (hh_ret) {
	case HH_ERROR_OK:
		cap_table_entry->tx_full = !resp.not_full;
		ret = 0;
		break;
	case HH_ERROR_MSGQUEUE_FULL:
		cap_table_entry->tx_full = true;
		ret = -EAGAIN;
		break;
	default:
		ret = hh_remap_error(hh_ret);
	}

	spin_unlock_irqrestore(&cap_table_entry->tx_lock, flags);

	if (ret != 0 && ret != -EAGAIN)
		pr_err("%s: Failed to send on msgq. Hypercall error: %d\n",
			__func__, hh_ret);

	return ret;
}

/**
 * hh_msgq_send: Send a message to the client on a different VM
 * @client_desc: The client descriptor that was obtained via hh_msgq_register()
 * @buff: Pointer to the buffer that needs to be sent
 * @size: The size of the buffer
 * @flags: Optional flags to pass to send the data. For the list of flags,
 *         see linux/haven/hh_msgq.h
 *
 * The function returns -EINVAL if the caller passes invalid arguments,
 * -EAGAIN if the message queue is not yet ready to communicate, and -EPERM if
 * the caller doesn't have permissions to send the data.
 *
 */
int hh_msgq_send(void *msgq_client_desc,
			void *buff, size_t size, unsigned long flags)
{
	struct hh_msgq_desc *client_desc = msgq_client_desc;
	struct hh_msgq_cap_table *cap_table_entry;
	int ret;

	if (!client_desc || !buff || !size)
		return -EINVAL;

	if (size > HH_MSGQ_MAX_MSG_SIZE_BYTES)
		return -E2BIG;

	cap_table_entry = &hh_msgq_cap_table[client_desc->label];

	spin_lock(&cap_table_entry->cap_entry_lock);

	if (cap_table_entry->client_desc != client_desc) {
		pr_err("%s: Invalid client descriptor\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	if ((cap_table_entry->tx_cap_id == HH_CAPID_INVAL) &&
		(flags & HH_MSGQ_NONBLOCK)) {
		pr_err_ratelimited(
			"%s: Send info for label %d not yet initialized\n",
			__func__, client_desc->label);
		ret = -EAGAIN;
		goto err;
	}

	spin_unlock(&cap_table_entry->cap_entry_lock);

	if (wait_event_interruptible(cap_table_entry->tx_wq,
				cap_table_entry->tx_cap_id != HH_CAPID_INVAL))
		return -ERESTARTSYS;

	spin_lock(&cap_table_entry->cap_entry_lock);

	if (!cap_table_entry->tx_irq) {
		pr_err_ratelimited("%s: Tx IRQ for label %d not yet setup\n",
			__func__, client_desc->label);
		ret = -EAGAIN;
		goto err;
	}

	spin_unlock(&cap_table_entry->cap_entry_lock);

	do {
		if (cap_table_entry->tx_full && (flags & HH_MSGQ_NONBLOCK))
			return -EAGAIN;

		if (wait_event_interruptible(cap_table_entry->tx_wq,
					!cap_table_entry->tx_full))
			return -ERESTARTSYS;

		ret = __hh_msgq_send(cap_table_entry, buff, size, flags);
	} while (ret == -EAGAIN);

	return ret;
err:
	spin_unlock(&cap_table_entry->cap_entry_lock);
	return ret;
}
EXPORT_SYMBOL(hh_msgq_send);

/**
 * hh_msgq_register: Register as a client to the use the message queue
 * @label: The label associated to the message queue that the client wants
 *         to communicate
 *
 * The function returns a descriptor for the clients to send and receive the
 * messages. Else, returns -EBUSY if some other client is already regitsered
 * to this label, and -EINVAL for invalid arguments. The caller should check
 * the return value using IS_ERR_OR_NULL() and PTR_ERR() to extract the error
 * code.
 */
void *hh_msgq_register(enum hh_msgq_label label)
{
	struct hh_msgq_cap_table *cap_table_entry;
	struct hh_msgq_desc *client_desc;

	if (!hh_msgq_initialized)
		return ERR_PTR(-EPROBE_DEFER);

	if (label < 0 || label >= HH_MSGQ_LABEL_MAX)
		return ERR_PTR(-EINVAL);

	cap_table_entry = &hh_msgq_cap_table[label];

	spin_lock(&cap_table_entry->cap_entry_lock);

	/* Multiple clients cannot register to the same label (msgq) */
	if (cap_table_entry->client_desc) {
		spin_unlock(&cap_table_entry->cap_entry_lock);
		pr_err("%s: Client already exists for label %d\n",
				__func__, label);
		return ERR_PTR(-EBUSY);
	}

	client_desc = kzalloc(sizeof(*client_desc), GFP_ATOMIC);
	if (!client_desc) {
		spin_unlock(&cap_table_entry->cap_entry_lock);
		return ERR_PTR(ENOMEM);
	}

	client_desc->label = label;

	cap_table_entry->client_desc = client_desc;
	spin_unlock(&cap_table_entry->cap_entry_lock);

	pr_info("hh_msgq: Registered client for label: %d\n", label);

	return client_desc;
}
EXPORT_SYMBOL(hh_msgq_register);

/**
 * hh_msgq_unregister: Unregister as a client to the use the message queue
 * @client_desc: The descriptor that was passed via hh_msgq_register()
 *
 * The function returns 0 is the client was unregistered successfully. Else,
 * -EINVAL for invalid arguments.
 */
int hh_msgq_unregister(void *msgq_client_desc)
{
	struct hh_msgq_desc *client_desc = msgq_client_desc;
	struct hh_msgq_cap_table *cap_table_entry;

	if (!client_desc)
		return -EINVAL;

	cap_table_entry = &hh_msgq_cap_table[client_desc->label];

	spin_lock(&cap_table_entry->cap_entry_lock);

	/* Is the client trying to free someone else's msgq? */
	if (cap_table_entry->client_desc != client_desc) {
		pr_err("%s: Trying to free invalid client descriptor!\n",
			__func__);
		spin_unlock(&cap_table_entry->cap_entry_lock);
		return -EINVAL;
	}

	cap_table_entry->client_desc = NULL;

	spin_unlock(&cap_table_entry->cap_entry_lock);

	pr_info("%s: Unregistered client for label: %d\n",
		__func__, client_desc->label);

	kfree(client_desc);

	return 0;
}
EXPORT_SYMBOL(hh_msgq_unregister);

int hh_msgq_populate_cap_info(enum hh_msgq_label label, u64 cap_id,
				int direction, int irq)
{
	struct hh_msgq_cap_table *cap_table_entry;
	int ret;

	if (!hh_msgq_initialized)
		return -EAGAIN;

	if (label < 0 || label >= HH_MSGQ_LABEL_MAX) {
		pr_err("%s: Invalid label passed\n", __func__);
		return -EINVAL;
	}

	if (irq < 0) {
		pr_err("%s: Invalid IRQ number passed\n", __func__);
		return -ENXIO;
	}

	cap_table_entry = &hh_msgq_cap_table[label];

	if (direction == HH_MSGQ_DIRECTION_TX) {
		ret = request_irq(irq, hh_msgq_tx_isr, 0,
				cap_table_entry->tx_irq_name, cap_table_entry);
		if (ret < 0)
			goto err;

		spin_lock(&cap_table_entry->cap_entry_lock);
		cap_table_entry->tx_cap_id = cap_id;
		cap_table_entry->tx_irq = irq;
		spin_unlock(&cap_table_entry->cap_entry_lock);

		wake_up_interruptible(&cap_table_entry->tx_wq);
	} else if (direction == HH_MSGQ_DIRECTION_RX) {
		ret = request_irq(irq, hh_msgq_rx_isr, 0,
				cap_table_entry->rx_irq_name, cap_table_entry);
		if (ret < 0)
			goto err;

		spin_lock(&cap_table_entry->cap_entry_lock);
		cap_table_entry->rx_cap_id = cap_id;
		cap_table_entry->rx_irq = irq;
		spin_unlock(&cap_table_entry->cap_entry_lock);

		wake_up_interruptible(&cap_table_entry->rx_wq);
	} else {
		pr_err("%s: Invalid direction passed\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	irq_set_irq_wake(irq, 1);

	pr_debug(
		"%s: label: %d; cap_id: %llu; dir: %d; irq: %d\n",
		__func__, label, cap_id, direction, irq);

	return 0;

err:
	return ret;
}
EXPORT_SYMBOL(hh_msgq_populate_cap_info);

static int hh_msgq_probe_direction(struct platform_device *pdev,
			enum hh_msgq_label label, int direction, int idx)
{
	int irq, ret;
	u64 capid;

	irq = platform_get_irq(pdev, idx);
	if (irq < 0) {
		dev_err(&pdev->dev, "Failed to get the IRQ%d. ret: %d\n",
			idx, irq);
		return irq;
	}

	ret = of_property_read_u64_index(pdev->dev.of_node, "reg", idx, &capid);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get capid[%d]\n", idx);
		return ret;
	}

	return hh_msgq_populate_cap_info(label, capid, direction, irq);
}

int hh_msgq_probe(struct platform_device *pdev, enum hh_msgq_label label)
{
	int ret, idx = 0;
	struct device_node *node = pdev->dev.of_node;
	bool duplex;

	duplex = of_property_read_bool(node, "qcom,is-full-duplex");

	if (duplex || of_property_read_bool(node, "qcom,is-sender")) {
		ret = hh_msgq_probe_direction(pdev, label, HH_MSGQ_DIRECTION_TX,
					      idx);
		if (ret)
			return ret;
		idx++;
	}

	if (duplex || of_property_read_bool(node, "qcom,is-receiver")) {
		ret = hh_msgq_probe_direction(pdev, label, HH_MSGQ_DIRECTION_RX,
					      idx);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(hh_msgq_probe);

static void hh_msgq_cleanup(int begin_idx)
{
	struct hh_msgq_cap_table *cap_table_entry;
	int i;

	if (begin_idx >= HH_MSGQ_LABEL_MAX)
		begin_idx = HH_MSGQ_LABEL_MAX - 1;

	for (i = begin_idx; i >= 0; i--) {
		cap_table_entry = &hh_msgq_cap_table[i];

		kfree(cap_table_entry->tx_irq_name);
		kfree(cap_table_entry->rx_irq_name);
	}
}

static int __init hh_msgq_init(void)
{
	struct hh_msgq_cap_table *cap_table_entry;
	int ret;
	int i;

	for (i = 0; i < HH_MSGQ_LABEL_MAX; i++) {
		cap_table_entry = &hh_msgq_cap_table[i];

		cap_table_entry->tx_cap_id = HH_CAPID_INVAL;
		cap_table_entry->rx_cap_id = HH_CAPID_INVAL;
		cap_table_entry->tx_full = false;
		cap_table_entry->rx_empty = true;
		init_waitqueue_head(&cap_table_entry->tx_wq);
		init_waitqueue_head(&cap_table_entry->rx_wq);
		spin_lock_init(&cap_table_entry->tx_lock);
		spin_lock_init(&cap_table_entry->rx_lock);
		spin_lock_init(&cap_table_entry->cap_entry_lock);

		cap_table_entry->tx_irq_name = kasprintf(GFP_ATOMIC,
							"hh_msgq_tx_%d", i);
		if (!cap_table_entry->tx_irq_name) {
			ret = -ENOMEM;
			goto err;
		}

		cap_table_entry->rx_irq_name = kasprintf(GFP_ATOMIC,
							"hh_msgq_rx_%d", i);
		if (!cap_table_entry->rx_irq_name) {
			ret = -ENOMEM;
			goto err;
		}
	}

	hh_msgq_initialized = true;
	return 0;

err:
	hh_msgq_cleanup(i);
	return ret;
}
module_init(hh_msgq_init);

static void __exit hh_msgq_exit(void)
{
	hh_msgq_cleanup(HH_MSGQ_LABEL_MAX - 1);
}
module_exit(hh_msgq_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Haven Message Queue Driver");
