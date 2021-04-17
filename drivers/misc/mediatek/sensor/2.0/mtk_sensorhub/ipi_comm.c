// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "[ipi_comm] " fmt

#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

#include "ipi_comm.h"
#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"

enum scp_ipi_status __attribute__((weak)) scp_ipi_send(enum ipi_id id,
		void *buf, unsigned int  len,
		unsigned int wait, enum scp_core_id scp_id)
{
	return SCP_IPI_ERROR;
}

struct ipi_controller {
	spinlock_t lock;
	bool running;
	struct list_head head;
	struct workqueue_struct *workqueue;
	struct work_struct work;
	void (*notify_callback)(int id, void *data, unsigned int len);
};

struct ipi_hw_transfer {
	struct completion done;
	int count;
	/* data buffers */
	int id;
	const unsigned char *tx;
	unsigned char *rx;
	unsigned int tx_len;
	unsigned int rx_len;
	void *context;
};

static struct ipi_controller controller;
static struct ipi_hw_transfer hw_transfer;
static DEFINE_SPINLOCK(hw_transfer_lock);

static int ipi_transfer_buffer(struct ipi_transfer *t)
{
	int status = 0, retry = 0;
	int timeout;
	unsigned long flags;
	struct ipi_hw_transfer *hw = &hw_transfer;

	spin_lock_irqsave(&hw_transfer_lock, flags);
	hw->id = t->id;
	hw->tx = t->tx_buf;
	hw->rx = t->rx_buf;
	hw->tx_len = t->tx_len;
	hw->rx_len = t->rx_len;

	init_completion(&hw->done);
	hw->context = &hw->done;
	spin_unlock_irqrestore(&hw_transfer_lock, flags);
	do {
		status = scp_ipi_send(hw->id,
			(unsigned char *)hw->tx, hw->tx_len, 0, SCP_A_ID);
		if (status == SCP_IPI_ERROR) {
			pr_err("transfer fail\n");
			return -1;
		}
		if (status == SCP_IPI_BUSY) {
			if (retry++ == 1000) {
				pr_err("retry fail\n");
				return -1;
			}
			if (retry % 100 == 0)
				usleep_range(1000, 2000);
		}
	} while (status == SCP_IPI_BUSY);

	if (retry >= 100)
		pr_debug("retry times: %d\n", retry);

	timeout = wait_for_completion_timeout(&hw->done,
			msecs_to_jiffies(500));
	spin_lock_irqsave(&hw_transfer_lock, flags);
	if (!timeout) {
		pr_err("transfer timeout!");
		hw->count = -1;
	}
	hw->context = NULL;
	spin_unlock_irqrestore(&hw_transfer_lock, flags);
	return hw->count;
}

static void ipi_complete(void *arg)
{
	complete(arg);
}

static void ipi_transfer_messages(void)
{
	struct ipi_message *m;
	struct ipi_transfer *t = NULL;
	int status = 0;
	unsigned long flags;

	spin_lock_irqsave(&controller.lock, flags);
	if (list_empty(&controller.head) || controller.running)
		goto out;
	controller.running = true;
	while (!list_empty(&controller.head)) {
		m = list_first_entry(&controller.head,
			struct ipi_message, list);
		list_del(&m->list);
		spin_unlock_irqrestore(&controller.lock, flags);
		list_for_each_entry(t, &m->transfers, transfer_list) {
			if (!t->tx_buf && t->tx_len) {
				status = -EINVAL;
				pr_err("invalid parameter: %d\n",
					status);
				break;
			}
			if (t->tx_len)
				status = ipi_transfer_buffer(t);
			if (status < 0) {
				status = -EREMOTEIO;
				/* pr_err("transfer err :%d\n", status); */
				break;
			} else if (status != t->rx_len) {
				pr_err("ack err: %d %d\n", status, t->rx_len);
				status = -EREMOTEIO;
				break;
			}
			status = 0;
		}
		m->status = status;
		m->complete(m->context);
		spin_lock_irqsave(&controller.lock, flags);
	}
	controller.running = false;
out:
	spin_unlock_irqrestore(&controller.lock, flags);
}

static void ipi_prefetch_messages(void)
{
	ipi_transfer_messages();
}

static void ipi_work(struct work_struct *work)
{
	ipi_transfer_messages();
}

static int __ipi_transfer(struct ipi_message *m)
{
	unsigned long flags;

	m->status = -EINPROGRESS;

	spin_lock_irqsave(&controller.lock, flags);
	list_add_tail(&m->list, &controller.head);
	queue_work(controller.workqueue, &controller.work);
	spin_unlock_irqrestore(&controller.lock, flags);
	return 0;
}

static int __ipi_xfer(struct ipi_message *message)
{
	DECLARE_COMPLETION_ONSTACK(done);
	int status;

	message->complete = ipi_complete;
	message->context = &done;

	status = __ipi_transfer(message);

	if (status == 0) {
		ipi_prefetch_messages();
		wait_for_completion(&done);
		status = message->status;
	}
	message->context = NULL;
	return status;
}

static int ipi_sync(int id, const unsigned char *txbuf, unsigned int n_tx,
		unsigned char *rxbuf, unsigned int n_rx)
{
	struct ipi_transfer t;
	struct ipi_message m;

	t.id = id;
	t.tx_buf = txbuf;
	t.tx_len = n_tx;
	t.rx_buf = rxbuf;
	t.rx_len = n_rx;

	ipi_message_init(&m);
	ipi_message_add_tail(&t, &m);

	return __ipi_xfer(&m);
}

static int ipi_async(struct ipi_message *m)
{
	return __ipi_transfer(m);
}

int ipi_comm_sync(int id, unsigned char *tx, unsigned int n_tx,
		unsigned char *rx, unsigned int n_rx)
{
	return ipi_sync(id, tx, n_tx, rx, n_rx);
}

int ipi_comm_async(struct ipi_message *m)
{
	return ipi_async(m);
}

static void ipi_comm_complete(unsigned char *buffer, unsigned int len)
{
	struct ipi_hw_transfer *hw = &hw_transfer;

	spin_lock(&hw_transfer_lock);
	if (!hw->context) {
		pr_err("dropped transfer\n");
		goto out;
	}
	/* only copy hw->rx_len bytes to hw->rx to avoid memory corruption */
	memcpy(hw->rx, buffer, hw->rx_len);
	/* hw->count give real len */
	hw->count = len;
	complete(hw->context);
out:
	spin_unlock(&hw_transfer_lock);
}

static void ipi_comm_ctrl_handler(int id, void *data, unsigned int len)
{
	WARN_ON(id != IPI_CHRE);
	ipi_comm_complete(data, len);
}

static void ipi_comm_notify_handler(int id, void *data, unsigned int len)
{
	WARN_ON(id != IPI_CHREX);
	if (controller.notify_callback)
		controller.notify_callback(id, data, len);
}

int get_ctrl_id(void)
{
	return IPI_CHRE;
}

void ipi_comm_notify_handler_register(void (*f)(int, void *, unsigned int))
{
	controller.notify_callback = f;
}

void ipi_comm_notify_handler_unregister(void)
{
	controller.notify_callback = NULL;
}

int ipi_comm_init(void)
{
	INIT_WORK(&controller.work, ipi_work);
	INIT_LIST_HEAD(&controller.head);
	spin_lock_init(&controller.lock);
	controller.workqueue = create_singlethread_workqueue("ipi_comm");
	if (controller.workqueue == NULL) {
		pr_err("create workqueue fail\n");
		return -1;
	}
	scp_ipi_registration(IPI_CHRE,
		ipi_comm_ctrl_handler, "ipi_comm_ctrl");
	scp_ipi_registration(IPI_CHREX,
		ipi_comm_notify_handler, "ipi_comm_notify");
	return 0;
}
