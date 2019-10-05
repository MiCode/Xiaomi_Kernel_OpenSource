/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/mtk-rpmsg-mailbox.h>
#include "../misc/mediatek/include/mt-plat/mtk-mbox.h"

static struct mtk_mbox_chan *to_mtk_mbox_chan(struct mbox_chan *chan)
{
	if (!chan || !chan->con_priv)
		return NULL;

	return (struct mtk_mbox_chan *)chan->con_priv;
}

int mtk_mbox_send_ipi(struct mtk_mbox_chan *mchan, void *data)
{
	struct mtk_mbox_device *mbdev;
	struct mtk_ipi_msg *msg;
	unsigned int status;
	int ret;

	if (WARN_ON(!data)) {
		pr_notice("mbox fw:%u warning\n", mchan->mbox);
		return -EINVAL;
	}

	mbdev = mchan->mbdev;
	msg = (struct mtk_ipi_msg *)data;

	status = mtk_mbox_check_send_irq(mbdev, mchan->mbox,
		mchan->send_pin_index);
	if (status != 0) {
		mchan->ipimsg = data;
		return -EBUSY;
	}

	ret = mtk_mbox_write_hd(mbdev, mchan->mbox, mchan->send_slot, msg);
	if (ret != MBOX_DONE)
		return -EIO;
	/*
	 * Ensure that all writes to SRAM are committed before sending the
	 * interrupt to mbox.
	 */
	mb();
	ret = mtk_mbox_trigger_irq(mbdev, mchan->mbox,
		0x1 << mchan->send_pin_index);
	if (ret != MBOX_DONE)
		pr_notice("mbox fw:%u irq fail\n", mchan->mbox);

	return ret;
}

bool mtk_mbox_tx_done(struct mtk_mbox_chan *mchan)
{
	struct mtk_mbox_device *mbdev;

	struct mbox_chan *chan;
	unsigned int status;
	unsigned long flags;

	chan = mchan->chan;
	mbdev = mchan->mbdev;

	spin_lock_irqsave(&chan->lock, flags);
	status = mtk_mbox_check_send_irq(mbdev, mchan->mbox,
		mchan->send_pin_index);
	spin_unlock_irqrestore(&chan->lock, flags);

	return status ? false : true;
}

int mtk_mbox_start(struct mtk_mbox_chan *mchan)
{
	return 0;
}

void mtk_mbox_down(struct mtk_mbox_chan *mchan)
{
}

int mtk_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct mtk_mbox_chan *mchan = to_mtk_mbox_chan(chan);

	return mchan->ops->mtk_send_ipi(mchan, data);
}

bool mtk_mbox_last_tx_done(struct mbox_chan *chan)
{
	struct mtk_mbox_chan *mchan = to_mtk_mbox_chan(chan);

	return mchan->ops->mtk_tx_done(mchan);
}

int mtk_mbox_startup(struct mbox_chan *chan)
{
	struct mtk_mbox_chan *mchan = to_mtk_mbox_chan(chan);

	if (mchan->ops->mtk_startup)
		return mchan->ops->mtk_startup(mchan);

	return 0;
}

void mtk_mbox_shutdown(struct mbox_chan *chan)
{
	struct mtk_mbox_chan *mchan = to_mtk_mbox_chan(chan);

	if (mchan->ops->mtk_shutdown)
		mchan->ops->mtk_shutdown(mchan);
}

static const struct mbox_chan_ops mtk_ipi_mbox_chan_ops = {
	.send_data = mtk_mbox_send_data,
	.last_tx_done = mtk_mbox_last_tx_done,
	.startup = mtk_mbox_startup,
	.shutdown = mtk_mbox_shutdown,
};

static struct mtk_mbox_operations mtk_mbox_ops = {
	.mtk_send_ipi = mtk_mbox_send_ipi,
	.mtk_tx_done = mtk_mbox_tx_done,
	.mtk_startup = mtk_mbox_start,
	.mtk_shutdown = mtk_mbox_down,
};

static struct mbox_chan *mtk_rpmsg_mbox_xlate(struct mbox_controller *mbox,
		const struct of_phandle_args *sp)
{
	int chan_id = sp->args[0];

	if (chan_id >= mbox->num_chans)
		return ERR_PTR(-EINVAL);

	return &mbox->chans[chan_id];
}


int mtk_mbox_chan_create(struct mbox_controller *mboxctrl,
		struct mtk_mbox_device *mbdev, int num)
{
	struct mtk_mbox_chan *mchan;
	struct mtk_mbox_pin_send *msend;
	struct mbox_chan *chan;
	unsigned int i, j, count;

	mchan = kcalloc(num, sizeof(struct mtk_mbox_chan), GFP_KERNEL);
	if (!mchan)
		return -ENOMEM;

	chan = kcalloc(num, sizeof(struct mbox_chan), GFP_KERNEL);
	if (!chan) {
		kfree(mchan);
		return -ENOMEM;
	}

	count = mbdev->send_count;
	for (i = 0; i < num; i++) {
		chan[i].con_priv = &mchan[i];
		mchan[i].chan  = &chan[i];
		mchan[i].mbdev = mbdev;
		mchan[i].ops   = &mtk_mbox_ops;
		for (j = 0; j < count; j++) {
			msend = &(mbdev->pin_send_table[j]);
			if (i == msend->chan_id) {
				mchan[i].mbox = msend->mbox;
				mchan[i].send_slot = msend->offset;
				mchan[i].send_slot_size = msend->msg_size;
				mchan[i].send_pin_index = msend->pin_index;
				mchan[i].send_pin_offset = j;
			}
		}
	}
	mboxctrl->ops = &mtk_ipi_mbox_chan_ops;
	mboxctrl->chans = chan;
	mboxctrl->of_xlate = mtk_rpmsg_mbox_xlate;

	return 0;
}
