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

#define pr_fmt(fmt) "[mtk_nanohub_ipi] " fmt

#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

#include "mtk_nanohub_ipi.h"
#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"

enum scp_ipi_status __attribute__((weak)) scp_ipi_send(enum ipi_id id,
		void *buf, unsigned int  len,
		unsigned int wait, enum scp_core_id scp_id)
{
	return SCP_IPI_ERROR;
}

struct ipi_hw_master {
	spinlock_t lock;
	bool running;
	struct list_head head;
	struct workqueue_struct *workqueue;
	struct work_struct work;
};

struct ipi_hw_transfer {
	struct completion done;
	int count;
	/* data buffers */
	const unsigned char *tx;
	unsigned char *rx;
	unsigned int tx_len;
	unsigned int rx_len;
	void *context;
};

static struct ipi_hw_master hw_master;
static struct ipi_hw_transfer hw_transfer;
static DEFINE_SPINLOCK(hw_transfer_lock);

static int ipi_txrx_bufs(struct ipi_transfer *t)
{
	int status = 0, retry = 0;
	int timeout;
	unsigned long flags;
	struct ipi_hw_transfer *hw = &hw_transfer;

	spin_lock_irqsave(&hw_transfer_lock, flags);
	hw->tx = t->tx_buf;
	hw->rx = t->rx_buf;
	hw->tx_len = t->tx_len;
	hw->rx_len = t->rx_len;

	init_completion(&hw->done);
	hw->context = &hw->done;
	spin_unlock_irqrestore(&hw_transfer_lock, flags);
	do {
		status = scp_ipi_send(IPI_SENSOR,
			(unsigned char *)hw->tx, hw->tx_len, 0, SCP_A_ID);
		if (status == SCP_IPI_ERROR) {
			pr_err("scp_ipi_send fail\n");
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
		pr_debug("retry time:%d\n", retry);

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

	spin_lock_irqsave(&hw_master.lock, flags);
	if (list_empty(&hw_master.head) || hw_master.running)
		goto out;
	hw_master.running = true;
	while (!list_empty(&hw_master.head)) {
		m = list_first_entry(&hw_master.head,
			struct ipi_message, list);
		list_del(&m->list);
		spin_unlock_irqrestore(&hw_master.lock, flags);
		list_for_each_entry(t, &m->transfers, transfer_list) {
			if (!t->tx_buf && t->tx_len) {
				status = -EINVAL;
				pr_err("transfer param wrong :%d\n",
					status);
				break;
			}
			if (t->tx_len)
				status = ipi_txrx_bufs(t);
			if (status < 0) {
				status = -EREMOTEIO;
				/* pr_err("transfer err :%d\n", status); */
				break;
			} else if (status != t->rx_len) {
				pr_err("ack err :%d %d\n", status, t->rx_len);
				status = -EREMOTEIO;
				break;
			}
			status = 0;
		}
		m->status = status;
		m->complete(m->context);
		spin_lock_irqsave(&hw_master.lock, flags);
	}
	hw_master.running = false;
out:
	spin_unlock_irqrestore(&hw_master.lock, flags);
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

	spin_lock_irqsave(&hw_master.lock, flags);
	list_add_tail(&m->list, &hw_master.head);
	queue_work(hw_master.workqueue, &hw_master.work);
	spin_unlock_irqrestore(&hw_master.lock, flags);
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

static int ipi_sync(const unsigned char *txbuf, unsigned int n_tx,
		unsigned char *rxbuf, unsigned int n_rx)
{
	struct ipi_transfer t;
	struct ipi_message m;

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

int mtk_nanohub_ipi_sync(unsigned char *buffer, unsigned int len)
{
	return ipi_sync(buffer, len, buffer, len);
}

int mtk_nanohub_ipi_async(struct ipi_message *m)
{
	return ipi_async(m);
}

void mtk_nanohub_ipi_complete(unsigned char *buffer, unsigned int len)
{
	struct ipi_hw_transfer *hw = &hw_transfer;

	spin_lock(&hw_transfer_lock);
	if (!hw->context) {
		pr_err("after ipi timeout ack occur then dropped this\n");
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

int mtk_nanohub_ipi_init(void)
{
	INIT_WORK(&hw_master.work, ipi_work);
	INIT_LIST_HEAD(&hw_master.head);
	spin_lock_init(&hw_master.lock);
	hw_master.workqueue = create_singlethread_workqueue("ipi_master");
	if (hw_master.workqueue == NULL) {
		pr_err("workqueue fail\n");
		return -1;
	}

	return 0;
}

MODULE_AUTHOR("Mediatek");
MODULE_DESCRIPTION("mtk_nanohub_ipi driver");
MODULE_LICENSE("GPL");
