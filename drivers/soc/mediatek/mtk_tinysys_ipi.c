/*
 * Copyright (C) 2019 MediaTek Inc.
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


#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/rpmsg.h>
#include <linux/rpmsg/mtk_rpmsg.h>
#include <mt-plat/mtk-mbox.h>
#include <mt-plat/mtk_tinysys_ipi.h>

#define IPI_POLLING_INTERVAL_US    10

static void ipi_isr_cb(struct mtk_mbox_pin_recv *pin_recv);

int mtk_ipi_device_register(struct mtk_ipi_device *ipidev,
		struct platform_device *pdev, struct mtk_mbox_device *mbox,
		unsigned int ipi_chan_count)
{
	int index;
	struct mtk_ipi_chan_table *ipi_chan_table;
	struct mtk_rpmsg_device *mtk_rpdev;
	struct mtk_rpmsg_channel_info *mtk_rpchan;

	if (!mbox || !ipi_chan_count)
		return -EINVAL;

	ipi_chan_table = kcalloc(ipi_chan_count,
		sizeof(struct mtk_ipi_chan_table), GFP_KERNEL);
	if (!ipi_chan_table)
		return -ENOMEM;

	mtk_rpdev = mtk_rpmsg_create_device(pdev, mbox, ipi_chan_count);
	if (!mtk_rpdev) {
		pr_err("IPI init fail when create mtk rpmsg device.\n");
		kfree(ipi_chan_table);
		return IPI_RPMSG_ERR;
	}

	for (index = 0; index < ipi_chan_count; index++) {
		// TODO: how to get the IPC name~?
		mtk_rpchan = mtk_rpmsg_create_channel(mtk_rpdev, index, "ipi0");
		ipi_chan_table[index].rpchan = mtk_rpchan;
		ipi_chan_table[index].ept =
			rpmsg_create_ept(&(mtk_rpdev->rpdev),
					NULL, mtk_rpchan, mtk_rpchan->info);
	}

	mutex_init(&ipidev->mutex_ipi_reg);

	for (index = 0; index < mbox->send_count; index++) {
		mutex_init(&mbox->pin_send_table[index].mutex_send);
		init_completion(&mbox->pin_send_table[index].comp_ack);
		spin_lock_init(&mbox->pin_send_table[index].pin_lock);
		ipi_chan_table[mbox->pin_send_table[index].chan_id].pin_send =
						&mbox->pin_send_table[index];
	}

	for (index = 0; index < mbox->recv_count; index++) {
		init_completion(&mbox->pin_recv_table[index].notify);
		spin_lock_init(&mbox->pin_recv_table[index].pin_lock);
		ipi_chan_table[mbox->pin_recv_table[index].chan_id].pin_recv =
						&mbox->pin_recv_table[index];
	}

	// TODO: check_table_tag()

	mbox->ipi_cb = ipi_isr_cb;
	ipidev->mrpdev = mtk_rpdev;
	ipidev->table = ipi_chan_table;
	ipidev->mbdev = mbox;
	ipidev->ipi_inited = 1;

	return IPI_ACTION_DONE;
}


int mtk_ipi_register(struct mtk_ipi_device *ipidev, int ipi_id,
		void *cb, void *prdata, void *msg)
{
	struct mtk_mbox_pin_recv *pin_recv;

	if (!ipidev->ipi_inited)
		return IPI_DEV_ILLEGAL;

	if (!msg)
		return IPI_NO_MSGBUF;

	pin_recv = ipidev->table[ipi_id].pin_recv;
	if (!pin_recv)
		return IPI_UNAVAILABLE;

	mutex_lock(&ipidev->mutex_ipi_reg);

	if (pin_recv->pin_buf != NULL) {
		mutex_unlock(&ipidev->mutex_ipi_reg);
		return IPI_DUPLEX;
	}
	pin_recv->mbox_pin_cb = cb;
	pin_recv->pin_buf = msg;
	pin_recv->prdata = prdata;

	mutex_unlock(&ipidev->mutex_ipi_reg);

	return IPI_ACTION_DONE;
}
EXPORT_SYMBOL(mtk_ipi_register);

int mtk_ipi_send(struct mtk_ipi_device *ipidev, int ipi_id,
		int opt, void *data, int len, int retry_timeout)
{
	struct mtk_mbox_pin_send *pin;
	unsigned long flags = 0;
	int wait_us, ret = 1;

	if (!ipidev->ipi_inited)
		return IPI_DEV_ILLEGAL;

	pin = ipidev->table[ipi_id].pin_send;
	if (!pin)
		return IPI_UNAVAILABLE;

	if (len > pin->msg_size)
		return IPI_NO_MEMORY;
	else if (!len)
		len = pin->msg_size;

	if (opt == IPI_SEND_POLLING) {
		if (mutex_is_locked(&pin->mutex_send)) {
			pr_err("Error: IPI '%s' has been used in WAIT mode\n",
				ipidev->table[ipi_id].rpchan->info.name);
			BUG_ON(1);
		}
		spin_lock_irqsave(&pin->pin_lock, flags);
	} else {
		/* WAIT Mode: NOT be allowed in atomic/interrupt/IRQ disabled */
		if (preempt_count() || in_interrupt() || irqs_disabled()) {
			pr_err("IPI Panic: %s pin# %d, atomic=%d, interrupt=%ld, irq disabled=%d\n",
				ipidev->name, ipi_id, preempt_count(),
				in_interrupt(), irqs_disabled());
			BUG_ON(1);
		}
		mutex_lock(&pin->mutex_send);
	}

	if (ipidev->pre_cb)
		ipidev->pre_cb(ipidev->prdata);

	wait_us = retry_timeout * 1000;
	ret = rpmsg_trysend(ipidev->table[ipi_id].ept, data, len);
	while (wait_us > 0 && ret) {
		udelay(IPI_POLLING_INTERVAL_US);
		wait_us -= IPI_POLLING_INTERVAL_US;
		ret = rpmsg_trysend(ipidev->table[ipi_id].ept, data, len);
	}

	if (ipidev->post_cb)
		ipidev->post_cb(ipidev->prdata);

	if (opt == IPI_SEND_POLLING)
		spin_unlock_irqrestore(&pin->pin_lock, flags);
	else
		mutex_unlock(&pin->mutex_send);

	if (ret == MBOX_PIN_BUSY)
		return IPI_PIN_BUSY;
	else if (ret != IPI_ACTION_DONE) {
		pr_err("IPI send fail (%d)\n", ret);
		return IPI_RPMSG_ERR;
	}

	return IPI_ACTION_DONE;
}
EXPORT_SYMBOL(mtk_ipi_send);

int mtk_ipi_recv(struct mtk_ipi_device *ipidev, int ipi_id)
{
	struct mtk_mbox_pin_recv *pin;

	if (!ipidev->ipi_inited)
		return IPI_DEV_ILLEGAL;

	pin = ipidev->table[ipi_id].pin_recv;
	if (!pin)
		return IPI_UNAVAILABLE;

	/* receive the ipi from ISR */
	wait_for_completion(&pin->notify);

	if (pin->cb_ctx_opt) {
		pin->mbox_pin_cb(ipi_id, pin->prdata, pin->pin_buf,
			pin->msg_size);
	}

	return IPI_ACTION_DONE;
}
EXPORT_SYMBOL(mtk_ipi_recv);

int mtk_ipi_send_compl(struct mtk_ipi_device *ipidev, int ipi_id,
		int opt, void *data, int len, unsigned long timeout)
{
	struct mtk_mbox_pin_send *pin_s;
	struct mtk_mbox_pin_recv *pin_r;
	unsigned long flags = 0;
	int wait_us, ret = 1;

	if (!ipidev->ipi_inited)
		return IPI_DEV_ILLEGAL;

	pin_s = ipidev->table[ipi_id].pin_send;
	if (!pin_s)
		return IPI_UNAVAILABLE;

	if (len > pin_s->msg_size)
		return IPI_NO_MEMORY;
	else if (!len)
		len = pin_s->msg_size;

	if (opt == IPI_SEND_POLLING) {
		if (mutex_is_locked(&pin_s->mutex_send)) {
			pr_err("Error: IPI '%s' has been used in WAIT mode\n",
				ipidev->table[ipi_id].rpchan->info.name);
			BUG_ON(1);
		}
		spin_lock_irqsave(&pin_s->pin_lock, flags);
	} else {
		/* WAIT Mode: NOT be allowed in atomic/interrupt/IRQ disabled */
		if (preempt_count() || in_interrupt() || irqs_disabled()) {
			pr_err("IPI Panic: %s pin# %d, atomic=%d, interrupt=%ld, irq disabled=%d\n",
				ipidev->name, ipi_id, preempt_count(),
				in_interrupt(), irqs_disabled());
			BUG_ON(1);
		}
		mutex_lock(&pin_s->mutex_send);
	}

	if (ipidev->pre_cb)
		ipidev->pre_cb(ipidev->prdata);

	wait_us = timeout * 1000;
	ret = rpmsg_trysend(ipidev->table[ipi_id].ept, data, len);
	while (ret && wait_us > 0) {
		udelay(IPI_POLLING_INTERVAL_US);
		wait_us -= IPI_POLLING_INTERVAL_US;
		ret = rpmsg_trysend(ipidev->table[ipi_id].ept, data, len);
	}

	if (!ret) {
		if (opt == IPI_SEND_POLLING)
			spin_unlock_irqrestore(&pin_s->pin_lock, flags);
		else
			mutex_unlock(&pin_s->mutex_send);

		if (ipidev->post_cb)
			ipidev->post_cb(ipidev->prdata);

		return IPI_PIN_BUSY;
	}

	pin_r = ipidev->table[ipi_id].pin_recv;
	if (!pin_r)
		return IPI_UNAVAILABLE;

	/* Run receive at least once */
	wait_us = (wait_us < 0) ? IPI_POLLING_INTERVAL_US : wait_us;

	ret = IPI_COMPL_TIMEOUT;
	if (opt == IPI_SEND_POLLING) {
		while (wait_us > 0) {
			ret = mtk_mbox_polling(ipidev->mbdev,
					pin_r->mbox, pin_r->pin_buf, pin_r);
			if (ret == MBOX_DONE)
				break;

			if (try_wait_for_completion(&pin_r->notify)) {
				ret = IPI_ACTION_DONE;
				break;
			}
			udelay(IPI_POLLING_INTERVAL_US);
			wait_us -= IPI_POLLING_INTERVAL_US;
		}
		spin_unlock_irqrestore(&pin_s->pin_lock, flags);
	} else {
		/* WAIT Mode */
		ret = wait_for_completion_timeout(&pin_r->notify,
				msecs_to_jiffies(wait_us/100));
		mutex_unlock(&pin_s->mutex_send);

		if (ret > 0)
			ret = IPI_ACTION_DONE;
	}

	if (ipidev->post_cb)
		ipidev->post_cb(ipidev->prdata);

	return ret;
}
EXPORT_SYMBOL(mtk_ipi_send_compl);

int mtk_ipi_recv_reply(struct mtk_ipi_device *ipidev, int ipi_id,
		void *reply_data, int len)
{
	int ret;

	/* recvice the IPI message*/
	ret = mtk_ipi_recv(ipidev, ipi_id);

	if (ret == IPI_ACTION_DONE) {
		/* send the response*/
		ret = mtk_ipi_send(ipidev, ipi_id,
			IPI_SEND_POLLING, reply_data, len, 0);
	}

	return ret;
}
EXPORT_SYMBOL(mtk_ipi_recv_reply);

static void ipi_isr_cb(struct mtk_mbox_pin_recv *pin_recv)
{
	complete(&pin_recv->notify);
}
