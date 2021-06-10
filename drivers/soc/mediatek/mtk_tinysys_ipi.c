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
#include <linux/sched/clock.h>
#include <linux/rpmsg.h>
#include <linux/rpmsg/mtk_rpmsg.h>
#include <mt-plat/mtk-mbox.h>
#include <mt-plat/mtk_tinysys_ipi.h>

#define MS_TO_NS(x) ((x)*1000000)
#define ts_before(ts) (cpu_clock(0) < (ts))
#define ipi_delay() udelay(10)

#define ipi_echo(en, fmt, args...) \
	({ if (en) pr_info(fmt, ##args); })


static void ipi_isr_cb(struct mtk_mbox_pin_recv *pin, void *priv);


static void ipi_monitor(struct mtk_ipi_device *ipidev, int id, int stage)
{
	unsigned long flags = 0;
	struct mtk_ipi_chan_table *chan;

	chan = &ipidev->table[id];

	spin_lock_irqsave(&ipidev->lock_monitor, flags);
	switch (stage) {
	case SEND_MSG:
		chan->ipi_seqno++;
		chan->ipi_record[0].idx = 0;
		chan->ipi_record[0].ts = cpu_clock(0);
		chan->ipi_record[1].idx = 4;
		chan->ipi_record[1].ts = 0;
		chan->ipi_record[2].idx = 5;
		chan->ipi_record[2].ts = 0;
		ipi_echo(ipidev->mbdev->log_enable,
			"%s: IPI_%d send msg (#%d)\n",
			ipidev->name, id, chan->ipi_seqno);
		break;
	case ISR_RECV_MSGV:
		chan->ipi_seqno++;
		chan->ipi_record[0].idx = 1;
		chan->ipi_record[0].ts = cpu_clock(0);
		chan->ipi_record[1].idx = 2;
		chan->ipi_record[1].ts = 0;
		chan->ipi_record[2].idx = 3;
		chan->ipi_record[2].ts = 0;
		break;
	case RECV_MSG:
		chan->ipi_record[1].ts = cpu_clock(0);
		ipi_echo(ipidev->mbdev->log_enable,
			"%s: IPI_%d recv msg (#%d)\n",
			ipidev->name, id, chan->ipi_seqno);
		break;
	case RECV_ACK:
		chan->ipi_record[2].ts = cpu_clock(0);
		ipi_echo(ipidev->mbdev->log_enable,
			"%s: IPI_%d recv ack (#%d)\n",
			ipidev->name, id, chan->ipi_seqno);
		break;
	case ISR_RECV_ACK:
		chan->ipi_record[1].ts = cpu_clock(0);
		break;
	case SEND_ACK:
		chan->ipi_record[2].ts = cpu_clock(0);
		ipi_echo(ipidev->mbdev->log_enable,
			"%s: IPI_%d send ack (#%d)\n",
			ipidev->name, id, chan->ipi_seqno);
		break;
	default:
		break;
	}

	chan->ipi_stage = stage;

	spin_unlock_irqrestore(&ipidev->lock_monitor, flags);
}

static void ipi_timeout_dump(struct mtk_ipi_device *ipidev, int ipi_id)
{
	unsigned long flags = 0;
	struct mtk_ipi_chan_table *chan;

	chan = &ipidev->table[ipi_id];

	spin_lock_irqsave(&ipidev->lock_monitor, flags);

	pr_err("Error: %s IPI %d timeout at %lld (last done is IPI %d)\n",
		 ipidev->name, ipi_id, cpu_clock(0), ipidev->ipi_last_done);

	pr_err("IPI %d: seqno=%d, state=%d, t%d=%lld, t%d=%lld, t%d=%lld (trysend %d, polling %d\n",
		ipi_id, chan->ipi_seqno, chan->ipi_stage,
		chan->ipi_record[0].idx, chan->ipi_record[0].ts,
		chan->ipi_record[1].idx, chan->ipi_record[1].ts,
		chan->ipi_record[2].idx, chan->ipi_record[2].ts,
		chan->trysend_count, chan->polling_count);

	spin_unlock_irqrestore(&ipidev->lock_monitor, flags);
}

void ipi_monitor_dump(struct mtk_ipi_device *ipidev)
{
	int i;
	unsigned int ipi_chan_count;
	unsigned long flags = 0;
	struct mtk_ipi_chan_table *chan;

	ipi_chan_count = ipidev->mrpdev->rpdev.src;

	pr_info("%s dump IPIMonitor:\n", ipidev->name);

	spin_lock_irqsave(&ipidev->lock_monitor, flags);

	for (i = 0; i < ipi_chan_count; i++) {
		chan = &ipidev->table[i];
		if (chan->ipi_stage == UNUSED)
			continue;
		pr_info("IPI %d: seqno=%d, state=%d, t%d=%lld, t%d=%lld, t%d=%lld\n",
			i, chan->ipi_seqno, chan->ipi_stage,
			chan->ipi_record[0].idx, chan->ipi_record[0].ts,
			chan->ipi_record[1].idx, chan->ipi_record[1].ts,
			chan->ipi_record[2].idx, chan->ipi_record[2].ts);
	}

	spin_unlock_irqrestore(&ipidev->lock_monitor, flags);
}

int mtk_ipi_device_register(struct mtk_ipi_device *ipidev,
		struct platform_device *pdev, struct mtk_mbox_device *mbox,
		unsigned int ipi_chan_count)
{
	int index;
	char chan_name[RPMSG_NAME_SIZE];
	struct mtk_ipi_chan_table *ipi_chan_table;
	struct mtk_rpmsg_device *mtk_rpdev;
	struct mtk_rpmsg_channel_info *mtk_rpchan;

	if (!mbox || !ipi_chan_count)
		return -EINVAL;

	if (!ipidev->name)
		return -ENXIO;

	ipi_chan_table = kcalloc(ipi_chan_count,
		sizeof(struct mtk_ipi_chan_table), GFP_KERNEL);
	if (!ipi_chan_table)
		return -ENOMEM;

	mtk_rpdev = mtk_rpmsg_create_device(pdev, mbox, ipi_chan_count);
	if (!mtk_rpdev) {
		pr_err("%s create mtk rpmsg device fail.\n", ipidev->name);
		kfree(ipi_chan_table);
		return IPI_RPMSG_ERR;
	}

	for (index = 0; index < ipi_chan_count; index++) {
		snprintf(chan_name, RPMSG_NAME_SIZE, "%s_ipi#%d",
			ipidev->name, index);
		mtk_rpchan = mtk_rpmsg_create_channel(mtk_rpdev, index,
				chan_name);
		ipi_chan_table[index].rpchan = mtk_rpchan;
		ipi_chan_table[index].ept =
			rpmsg_create_ept(&(mtk_rpdev->rpdev),
					NULL, mtk_rpchan, mtk_rpchan->info);
		if (!ipi_chan_table[index].ept)
			return -EINVAL;
		ipi_chan_table[index].ipi_stage = UNUSED;
		ipi_chan_table[index].ipi_seqno = 0;
		atomic_set(&ipi_chan_table[index].holder, 0);
	}

	mutex_init(&ipidev->mutex_ipi_reg);
	spin_lock_init(&ipidev->lock_monitor);
	ipidev->ipi_last_done = -1;

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

	mbox->ipi_cb = ipi_isr_cb;
	mbox->ipi_priv = (void *)ipidev;
	ipidev->mrpdev = mtk_rpdev;
	ipidev->table = ipi_chan_table;
	ipidev->mbdev = mbox;
	ipidev->ipi_inited = 1;

	pr_info("%s (with %d IPI) has registered.\n",
		ipidev->name, ipi_chan_count);
	return IPI_ACTION_DONE;
}

int mtk_ipi_device_reset(struct mtk_ipi_device *ipidev)
{
	int index, chan_count;
	unsigned long flags = 0;
	struct mtk_ipi_chan_table *chan_table;

	if (!ipidev->table)
		return -ENXIO;

	ipidev->ipi_inited = 0;
	chan_count = ipidev->mrpdev->rpdev.src;
	chan_table = ipidev->table;

	spin_lock_irqsave(&ipidev->lock_monitor, flags);

	for (index = 0; index < chan_count; index++) {
		chan_table[index].ipi_stage = UNUSED;
		chan_table[index].ipi_seqno = 0;
	}

	ipidev->ipi_last_done = -1;

	spin_unlock_irqrestore(&ipidev->lock_monitor, flags);

	mtk_mbox_reset_record(ipidev->mbdev);

	ipidev->ipi_inited = 1;

	pr_info("%s (with %d IPI) has reset.\n", ipidev->name, chan_count);

	return IPI_ACTION_DONE;
}

int mtk_ipi_register(struct mtk_ipi_device *ipidev, int ipi_id,
		mbox_pin_cb_t cb, void *prdata, void *msg)
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

int mtk_ipi_unregister(struct mtk_ipi_device *ipidev, int ipi_id)
{
	unsigned long flags = 0;
	struct mtk_mbox_pin_recv *pin_recv;

	if (!ipidev->ipi_inited)
		return IPI_DEV_ILLEGAL;

	pin_recv = ipidev->table[ipi_id].pin_recv;
	if (!pin_recv)
		return IPI_UNAVAILABLE;

	mutex_lock(&ipidev->mutex_ipi_reg);

	/* Drop the ipi and reset the record */
	complete(&pin_recv->notify);

	spin_lock_irqsave(&ipidev->lock_monitor, flags);
	ipidev->table[ipi_id].ipi_stage = UNUSED;
	ipidev->table[ipi_id].ipi_seqno = 0;
	spin_unlock_irqrestore(&ipidev->lock_monitor, flags);

	pin_recv->mbox_pin_cb = NULL;
	pin_recv->pin_buf = NULL;
	pin_recv->prdata = NULL;

	mutex_unlock(&ipidev->mutex_ipi_reg);

	return IPI_ACTION_DONE;
}
EXPORT_SYMBOL(mtk_ipi_unregister);

int mtk_ipi_send(struct mtk_ipi_device *ipidev, int ipi_id,
		int opt, void *data, int len, int timeout)
{
	struct mtk_mbox_pin_send *pin;
	unsigned long flags = 0;
	u64 timeover;
	int ret = 1;

	if (!ipidev->ipi_inited)
		return IPI_DEV_ILLEGAL;

	pin = ipidev->table[ipi_id].pin_send;
	if (!pin)
		return IPI_UNAVAILABLE;

	if (len > pin->msg_size)
		return IPI_NO_MEMORY;
	else if (!len)
		len = pin->msg_size;

	if (ipidev->pre_cb)
		ipidev->pre_cb(ipidev->prdata);

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

	timeover = cpu_clock(0) + MS_TO_NS(timeout);

	ret = rpmsg_trysend(ipidev->table[ipi_id].ept, data, len);
	ipidev->table[ipi_id].trysend_count = 1;

	while (ret && ts_before(timeover)) {
		ipi_delay();
		ret = rpmsg_trysend(ipidev->table[ipi_id].ept, data, len);
		ipidev->table[ipi_id].trysend_count++;
	}

	if (!ret) {
		ipi_monitor(ipidev, ipi_id, SEND_MSG);
		ipidev->ipi_last_done = ipi_id;
	}

	if (opt == IPI_SEND_POLLING)
		spin_unlock_irqrestore(&pin->pin_lock, flags);
	else
		mutex_unlock(&pin->mutex_send);

	if (ipidev->post_cb)
		ipidev->post_cb(ipidev->prdata);

	if (ret == MBOX_PIN_BUSY) {
		ipi_timeout_dump(ipidev, ipi_id);
		return IPI_PIN_BUSY;
	} else if (ret != IPI_ACTION_DONE) {
		pr_warn("%s IPI %d send fail (%d)\n",
			ipidev->name, ipi_id, ret);
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

	ipi_monitor(ipidev, ipi_id, RECV_MSG);
	ipidev->ipi_last_done = ipi_id;

	if (pin->mbox_pin_cb && pin->cb_ctx_opt == MBOX_CB_IN_PROCESS)
		pin->mbox_pin_cb(ipi_id, pin->prdata, pin->pin_buf,
			pin->msg_size);

	return IPI_ACTION_DONE;
}
EXPORT_SYMBOL(mtk_ipi_recv);

int mtk_ipi_send_compl(struct mtk_ipi_device *ipidev, int ipi_id,
		int opt, void *data, int len, unsigned long timeout)
{
	struct mtk_mbox_pin_send *pin_s;
	struct mtk_mbox_pin_recv *pin_r;
	unsigned long flags = 0;
	u64 timeover;
	int ret;

	if (!ipidev->ipi_inited)
		return IPI_DEV_ILLEGAL;

	pin_s = ipidev->table[ipi_id].pin_send;
	pin_r = ipidev->table[ipi_id].pin_recv;
	if (!pin_s || !pin_r)
		return IPI_UNAVAILABLE;

	if (len > pin_s->msg_size)
		return IPI_NO_MEMORY;
	else if (!len)
		len = pin_s->msg_size;

	if (ipidev->pre_cb)
		ipidev->pre_cb(ipidev->prdata);

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

	atomic_inc(&ipidev->table[ipi_id].holder);

	timeover = cpu_clock(0) + MS_TO_NS(timeout);

	ret = rpmsg_trysend(ipidev->table[ipi_id].ept, data, len);
	ipidev->table[ipi_id].trysend_count = 1;
	ipidev->table[ipi_id].polling_count = 0;

	while (ret && ts_before(timeover)) {
		ipi_delay();
		ret = rpmsg_trysend(ipidev->table[ipi_id].ept, data, len);
		ipidev->table[ipi_id].trysend_count++;
	}

	if (ret) {
		if (opt == IPI_SEND_POLLING)
			spin_unlock_irqrestore(&pin_s->pin_lock, flags);
		else
			mutex_unlock(&pin_s->mutex_send);

		atomic_set(&ipidev->table[ipi_id].holder, 0);

		if (ipidev->post_cb)
			ipidev->post_cb(ipidev->prdata);

		pr_warn("%s IPI %d send fail (%d)\n",
			ipidev->name, ipi_id, ret);
		return (ret == MBOX_PIN_BUSY) ? IPI_PIN_BUSY : IPI_RPMSG_ERR;
	}

	ipi_monitor(ipidev, ipi_id, SEND_MSG);

	if (opt == IPI_SEND_POLLING) {
		do {
			ipidev->table[ipi_id].polling_count++;
			if (mtk_mbox_polling(ipidev->mbdev, pin_r->mbox,
				pin_r->pin_buf, pin_r) == MBOX_DONE) {
				ret = 1;
				break;
			}

			if (try_wait_for_completion(&pin_r->notify)) {
				ret = 1;
				break;
			}

			ipi_delay();
		} while (ts_before(timeover));
	} else {
		/* WAIT Mode */
		timeover = ts_before(timeover) ? timeover - cpu_clock(0) : 0;
		ret = wait_for_completion_timeout(&pin_r->notify,
			nsecs_to_jiffies(timeover));
	}

	atomic_set(&ipidev->table[ipi_id].holder, 0);

	if (ret > 0) {
		ipi_monitor(ipidev, ipi_id, RECV_ACK);
		ipidev->ipi_last_done = ipi_id;
		ret = IPI_ACTION_DONE;
	} else {
		mtk_mbox_dump_recv_pin(ipidev->mbdev, pin_r);
		ipi_timeout_dump(ipidev, ipi_id);
		if (ipidev->timeout_handler)
			ipidev->timeout_handler(ipi_id);
		ret = IPI_COMPL_TIMEOUT;
	}

	if (opt == IPI_SEND_POLLING)
		spin_unlock_irqrestore(&pin_s->pin_lock, flags);
	else
		mutex_unlock(&pin_s->mutex_send);

	if (ipidev->post_cb)
		ipidev->post_cb(ipidev->prdata);

	return ret;
}
EXPORT_SYMBOL(mtk_ipi_send_compl);

int mtk_ipi_recv_reply(struct mtk_ipi_device *ipidev, int ipi_id,
		void *reply_data, int len)
{
	struct mtk_mbox_pin_send *pin_s;
	struct mtk_mbox_pin_recv *pin_r;
	unsigned long flags = 0;
	int ret;

	if (!ipidev->ipi_inited)
		return IPI_DEV_ILLEGAL;

	pin_r = ipidev->table[ipi_id].pin_recv;
	pin_s = ipidev->table[ipi_id].pin_send;
	if (!pin_r || !pin_s)
		return IPI_UNAVAILABLE;

	if (len > pin_s->msg_size)
		return IPI_NO_MEMORY;
	else if (len == 0)
		len = pin_s->msg_size;

	/* recvice the IPI message*/
	wait_for_completion(&pin_r->notify);

	ipi_monitor(ipidev, ipi_id, RECV_MSG);

	if (pin_r->mbox_pin_cb && pin_r->cb_ctx_opt == MBOX_CB_IN_PROCESS)
		pin_r->mbox_pin_cb(ipi_id, pin_r->prdata, pin_r->pin_buf,
			pin_r->msg_size);

	/* send the response*/
	if (ipidev->pre_cb)
		ipidev->pre_cb(ipidev->prdata);

	/* lock this pin until send ack*/
	spin_lock_irqsave(&pin_s->pin_lock, flags);

	ret = rpmsg_trysend(ipidev->table[ipi_id].ept, reply_data, len);

	if (ret == IPI_ACTION_DONE) {
		ipi_monitor(ipidev, ipi_id, SEND_ACK);
		ipidev->ipi_last_done = ipi_id;
	}

	spin_unlock_irqrestore(&pin_s->pin_lock, flags);

	if (ipidev->post_cb)
		ipidev->post_cb(ipidev->prdata);

	if (ret == MBOX_PIN_BUSY)
		return IPI_PIN_BUSY;
	else if (ret != IPI_ACTION_DONE) {
		pr_warn("%s IPI %d reply fail (%d)\n",
			ipidev->name, ipi_id, ret);
		return IPI_RPMSG_ERR;
	}

	return ret;
}
EXPORT_SYMBOL(mtk_ipi_recv_reply);

void mtk_ipi_tracking(struct mtk_ipi_device *ipidev, bool en)
{
	ipidev->mbdev->log_enable = en;
	pr_info("%s IPI tracking %s\n", ipidev->name, en ? "on" : "off");
}

static void ipi_isr_cb(struct mtk_mbox_pin_recv *pin, void *priv)
{
	struct mtk_ipi_device *ipidev = priv;
	int ipi_id = pin->chan_id;
	atomic_t holder = ipidev->table[ipi_id].holder;

	if (pin->recv_opt == MBOX_RECV_MESSAGE) {
		ipi_monitor(ipidev, ipi_id, ISR_RECV_MSGV);
		complete(&pin->notify);
	} else if (atomic_read(&holder)) {
		ipi_monitor(ipidev, ipi_id, ISR_RECV_ACK);
		complete(&pin->notify);
	}

}
