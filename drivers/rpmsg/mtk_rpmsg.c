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


/*
 * The MediaTek Mbox communication solution provides point-to-point
 * channels for clients to send and receive packet based data.
 */
#include <linux/of.h>
#include <linux/slab.h>

#include "rpmsg_internal.h"
#include <mt-plat/mtk-mbox.h>
#include <linux/rpmsg/mtk_rpmsg.h>

#define to_mtk_rpmsg_device(r) container_of(r, struct mtk_rpmsg_device, rpdev)
#define to_mtk_rpmsg_endpoint(r) container_of(r, struct mtk_rpmsg_endpoint, ept)

int mtk_mbox_send(struct mtk_rpmsg_endpoint *mept,
		struct mtk_rpmsg_channel_info *mchan, void *buf,
		unsigned int len, unsigned int wait)
{

	struct mtk_rpmsg_device *mdev;
	struct mtk_mbox_device *mbdev;
	unsigned int status;
	unsigned long flags;
	int ret;

	if (WARN_ON(len > mchan->send_slot_size || WARN_ON(!buf))) {
		pr_err("mbox:%u warning\n", mchan->mbox);
		return -EINVAL;
	}

	mdev = mept->mdev;
	mbdev = mept->mdev->mbdev;

	spin_lock_irqsave(&mchan->channel_lock, flags);
	status = mtk_mbox_check_send_irq(mbdev, mchan->mbox,
			mchan->send_pin_index);
	if (status != 0) {
		spin_unlock_irqrestore(&mchan->channel_lock, flags);
		return MBOX_PIN_BUSY;
	}
	spin_unlock_irqrestore(&mchan->channel_lock, flags);

	ret = mtk_mbox_write(mbdev, mchan->mbox, mchan->send_slot, buf,
			len * MBOX_SLOT_SIZE);
	if (ret != MBOX_DONE)
		return ret;

	/*
	 * Ensure that all writes to SRAM are committed before sending the
	 * interrupt to mbox.
	 */
	mb();

	ret = mtk_mbox_trigger_irq(mbdev, mchan->mbox,
			0x1 << mchan->send_pin_index);
	if (ret != MBOX_DONE)
		return ret;

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mbox_send);

static struct mtk_rpmsg_operations mtk_rpmsg_ops = {
	.mbox_send = mtk_mbox_send,
	// .send_ipi = mtk_mbox_send,
	// .register_ipi = scp_ipi_register,
	// .unregister_ipi = scp_ipi_unregister,
};

static void __ept_release(struct kref *kref)
{
	struct rpmsg_endpoint *ept;

	ept = container_of(kref, struct rpmsg_endpoint, refcount);
	kfree(to_mtk_rpmsg_endpoint(ept));
}

static void mtk_rpmsg_destroy_ept(struct rpmsg_endpoint *ept)
{
	kref_put(&ept->refcount, __ept_release);
}

static int mtk_rpmsg_send(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct mtk_rpmsg_device *mdev;
	struct mtk_rpmsg_channel_info *mchan;

	mdev = to_mtk_rpmsg_endpoint(ept)->mdev;
	mchan = to_mtk_rpmsg_endpoint(ept)->mchan;

	return mdev->ops->mbox_send(to_mtk_rpmsg_endpoint(ept),
		mchan, data, len, 1);
}

static int mtk_rpmsg_trysend(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct mtk_rpmsg_device *mdev;
	struct mtk_rpmsg_channel_info *mchan;

	mdev = to_mtk_rpmsg_endpoint(ept)->mdev;
	mchan = to_mtk_rpmsg_endpoint(ept)->mchan;

	return mdev->ops->mbox_send(to_mtk_rpmsg_endpoint(ept),
		mchan, data, len, 0);
}

static const struct rpmsg_endpoint_ops mtk_rpmsg_endpoint_ops = {
	.destroy_ept = mtk_rpmsg_destroy_ept,
	.send = mtk_rpmsg_send,
	.trysend = mtk_rpmsg_trysend,
};


/*
 * create mtk rpmsg channel
 */
struct mtk_rpmsg_channel_info *
mtk_rpmsg_create_channel(struct mtk_rpmsg_device *mdev, u32 chan_id, char *name)
{
	struct mtk_rpmsg_channel_info *mchan;
	struct mtk_mbox_device *mbdev;
	struct mtk_mbox_pin_send *msend;
	struct mtk_mbox_pin_recv *mrecv;
	unsigned int i, count, found;

	//malloc mtk_rpmsg_channel_info mchan
	mchan = kzalloc(sizeof(*mchan), GFP_KERNEL);
	if (!mchan)
		return NULL;

	mbdev = mdev->mbdev;
	spin_lock_init(&mchan->channel_lock);
	mchan->info.src = chan_id;
	//mchan->info.dst = dst;
	//copy name to rpdev, only dev no need now
	//strncpy(rpdev->id.name, name, RPMSG_NAME_SIZE);
	//copy name to channel info
	strlcpy(mchan->info.name, name, RPMSG_NAME_SIZE);

	count = mbdev->recv_count;
	found = 0;
	for (i = 0; i < count; ++i) {
		mrecv = &(mbdev->pin_recv_table[i]);
		if (chan_id == mrecv->chan_id) {
			mchan->mbox = mrecv->mbox;
			mchan->recv_slot = mrecv->offset;
			mchan->recv_slot_size = mrecv->msg_size;
			mchan->recv_pin_index = mrecv->pin_index;
			mchan->recv_pin_offset = i;
			//TODO if not found
			found = 1;
		}
	}

	count = mbdev->send_count;
	found = 0;
	for (i = 0; i < count; ++i) {
		msend = &(mbdev->pin_send_table[i]);
		if (chan_id == msend->chan_id) {
			mchan->mbox = msend->mbox;
			mchan->send_slot = msend->offset;
			mchan->send_slot_size = msend->msg_size;
			mchan->send_pin_index = msend->pin_index;
			mchan->send_pin_offset = i;
			//TODO if not found
			found = 1;
		}
	}
	return mchan;
}
EXPORT_SYMBOL_GPL(mtk_rpmsg_create_channel);

/*
 * create endpoint
 */
static struct rpmsg_endpoint *
__rpmsg_create_ept(struct rpmsg_device *rpdev, rpmsg_rx_cb_t cb, void *priv,
		struct rpmsg_channel_info chinfo)
{
	struct mtk_rpmsg_endpoint *mept;
	struct rpmsg_endpoint *ept;

	mept = kzalloc(sizeof(*mept), GFP_KERNEL);
	if (!mept)
		return NULL;

	mept->mdev = to_mtk_rpmsg_device(rpdev);
	mept->mchan = (struct mtk_rpmsg_channel_info *)priv;

	ept = &mept->ept;
	kref_init(&ept->refcount);

	ept->rpdev = rpdev;
	ept->cb = cb;
	//ept->priv = priv;
	ept->ops = &mtk_rpmsg_endpoint_ops;
	ept->addr = chinfo.src;

	return ept;
}

static struct rpmsg_endpoint *
mtk_rpmsg_create_ept(struct rpmsg_device *rpdev, rpmsg_rx_cb_t cb, void *priv,
		struct rpmsg_channel_info chinfo)
{
	return __rpmsg_create_ept(rpdev, cb, priv, chinfo);
}

static const struct rpmsg_device_ops mtk_rpmsg_device_ops = {
	.create_ept = mtk_rpmsg_create_ept,
};

static void mtk_rpmsg_release_device(struct device *dev)
{
	struct rpmsg_device *rpdev = to_rpmsg_device(dev);
	struct mtk_rpmsg_device *mdev = to_mtk_rpmsg_device(rpdev);

	kfree(mdev);
}

/*
 * create mtk rpmsg device
 */
struct mtk_rpmsg_device *
mtk_rpmsg_create_device(struct platform_device *pdev,
		struct mtk_mbox_device *mbdev, unsigned int ipc_chan_id)
{

	struct rpmsg_device *rpdev;
	struct mtk_rpmsg_device *mdev;
	int ret;

	mdev = kzalloc(sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return NULL;

	mdev->pdev = pdev;
	mdev->mbdev = mbdev;
	mdev->ops = &mtk_rpmsg_ops;
	rpdev = &mdev->rpdev;
	rpdev->ops = &mtk_rpmsg_device_ops;
	rpdev->src = ipc_chan_id;
	//rpdev->dst = info->dst;

//register RPMSG device

#if 0
	rpdev->dev.of_node =
		mtk_rpmsg_match_device_subnode
		(pdev->dev.of_node, info->name);
#endif
	rpdev->dev.parent = &pdev->dev;
	rpdev->dev.release = mtk_rpmsg_release_device;

	ret = rpmsg_register_device(rpdev);
	if (ret) {
		kfree(mdev);
		return NULL;
	}

	return mdev;
}
EXPORT_SYMBOL_GPL(mtk_rpmsg_create_device);


//MODULE_LICENSE("GPL v2");
//MODULE_DESCRIPTION("MediaTek rpmsg driver");
