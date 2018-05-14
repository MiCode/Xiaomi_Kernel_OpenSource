/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/termios.h>
#include <linux/wait.h>
#include <linux/mhi.h>
#include "mhi_internal.h"

struct __packed dtr_ctrl_msg {
	u32 preamble;
	u32 msg_id;
	u32 dest_id;
	u32 size;
	u32 msg;
};

#define CTRL_MAGIC (0x4C525443)
#define CTRL_MSG_DTR BIT(0)
#define CTRL_MSG_ID (0x10)

static int mhi_dtr_tiocmset(struct mhi_controller *mhi_cntrl,
			    struct mhi_chan *mhi_chan,
			    u32 tiocm)
{
	struct dtr_ctrl_msg *dtr_msg = NULL;
	struct mhi_chan *dtr_chan = mhi_cntrl->dtr_dev->ul_chan;
	int ret = 0;

	tiocm &= TIOCM_DTR;
	if (mhi_chan->tiocm == tiocm)
		return 0;

	mutex_lock(&dtr_chan->mutex);

	dtr_msg = kzalloc(sizeof(*dtr_msg), GFP_KERNEL);
	if (!dtr_msg) {
		ret = -ENOMEM;
		goto tiocm_exit;
	}

	dtr_msg->preamble = CTRL_MAGIC;
	dtr_msg->msg_id = CTRL_MSG_ID;
	dtr_msg->dest_id = mhi_chan->chan;
	dtr_msg->size = sizeof(u32);
	if (tiocm & TIOCM_DTR)
		dtr_msg->msg |= CTRL_MSG_DTR;

	reinit_completion(&dtr_chan->completion);
	ret = mhi_queue_transfer(mhi_cntrl->dtr_dev, DMA_TO_DEVICE, dtr_msg,
				 sizeof(*dtr_msg), MHI_EOT);
	if (ret)
		goto tiocm_exit;

	ret = wait_for_completion_timeout(&dtr_chan->completion,
				msecs_to_jiffies(mhi_cntrl->timeout_ms));

	if (!ret) {
		MHI_ERR("Failed to receive transfer callback\n");
		ret = -EIO;
		goto tiocm_exit;
	}

	ret = 0;
	mhi_chan->tiocm = tiocm;

tiocm_exit:
	kfree(dtr_msg);
	mutex_unlock(&dtr_chan->mutex);

	return ret;
}

long mhi_ioctl(struct mhi_device *mhi_dev, unsigned int cmd, unsigned long arg)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;
	struct mhi_chan *mhi_chan = mhi_dev->ul_chan;
	int ret;

	/* ioctl not supported by this controller */
	if (!mhi_cntrl->dtr_dev)
		return -EIO;

	switch (cmd) {
	case TIOCMGET:
		return mhi_chan->tiocm;
	case TIOCMSET:
	{
		u32 tiocm;

		ret = get_user(tiocm, (u32 *)arg);
		if (ret)
			return ret;

		return mhi_dtr_tiocmset(mhi_cntrl, mhi_chan, tiocm);
	}
	default:
		break;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(mhi_ioctl);

static void mhi_dtr_dl_xfer_cb(struct mhi_device *mhi_dev,
			       struct mhi_result *mhi_result)
{
}

static void mhi_dtr_ul_xfer_cb(struct mhi_device *mhi_dev,
			       struct mhi_result *mhi_result)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;
	struct mhi_chan *dtr_chan = mhi_cntrl->dtr_dev->ul_chan;

	MHI_VERB("Received with status:%d\n", mhi_result->transaction_status);
	if (!mhi_result->transaction_status)
		complete(&dtr_chan->completion);
}

static void mhi_dtr_remove(struct mhi_device *mhi_dev)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;

	mhi_cntrl->dtr_dev = NULL;
}

static int mhi_dtr_probe(struct mhi_device *mhi_dev,
			 const struct mhi_device_id *id)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;
	int ret;

	MHI_LOG("Enter for DTR control channel\n");

	ret = mhi_prepare_for_transfer(mhi_dev);
	if (!ret)
		mhi_cntrl->dtr_dev = mhi_dev;

	MHI_LOG("Exit with ret:%d\n", ret);

	return ret;
}

static const struct mhi_device_id mhi_dtr_table[] = {
	{ .chan = "IP_CTRL" },
	{ NULL },
};

static struct mhi_driver mhi_dtr_driver = {
	.id_table = mhi_dtr_table,
	.remove = mhi_dtr_remove,
	.probe = mhi_dtr_probe,
	.ul_xfer_cb = mhi_dtr_ul_xfer_cb,
	.dl_xfer_cb = mhi_dtr_dl_xfer_cb,
	.driver = {
		.name = "MHI_DTR",
		.owner = THIS_MODULE,
	}
};

int __init mhi_dtr_init(void)
{
	return mhi_driver_register(&mhi_dtr_driver);
}
