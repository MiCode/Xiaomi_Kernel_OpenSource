/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/diagchar.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/msm_mhi.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <asm/current.h>
#include <linux/atomic.h>
#include "diagmem.h"
#include "diagfwd_bridge.h"
#include "diagfwd_mhi.h"
#include "diag_ipc_logging.h"

#define SET_CH_CTXT(index, type)	(((index & 0xFF) << 8) | (type & 0xFF))
#define GET_INFO_INDEX(val)		((val & 0xFF00) >> 8)
#define GET_CH_TYPE(val)		((val & 0x00FF))

#define CHANNELS_OPENED			0
#define OPEN_CHANNELS			1
#define CHANNELS_CLOSED			0
#define CLOSE_CHANNELS			1

#define DIAG_MHI_STRING_SZ		11

struct diag_mhi_info diag_mhi[NUM_MHI_DEV] = {
	{
		.id = MHI_1,
		.dev_id = DIAGFWD_MDM,
		.name = "MDM",
		.enabled = 0,
		.num_read = 0,
		.mempool = POOL_TYPE_MDM,
		.mhi_wq = NULL,
		.read_ch = {
			.chan = MHI_CLIENT_DIAG_IN,
			.type = TYPE_MHI_READ_CH,
			.hdl = NULL,
		},
		.write_ch = {
			.chan = MHI_CLIENT_DIAG_OUT,
			.type = TYPE_MHI_WRITE_CH,
			.hdl = NULL,
		}
	},
	{
		.id = MHI_DCI_1,
		.dev_id = DIAGFWD_MDM_DCI,
		.name = "MDM_DCI",
		.enabled = 0,
		.num_read = 0,
		.mempool = POOL_TYPE_MDM_DCI,
		.mhi_wq = NULL,
		.read_ch = {
			.chan = MHI_CLIENT_DCI_IN,
			.type = TYPE_MHI_READ_CH,
			.hdl = NULL,
		},
		.write_ch = {
			.chan = MHI_CLIENT_DCI_OUT,
			.type = TYPE_MHI_WRITE_CH,
			.hdl = NULL,
		}
	}
};

static int mhi_ch_open(struct diag_mhi_ch_t *ch)
{
	int err = 0;

	if (!ch)
		return -EINVAL;

	if (atomic_read(&ch->opened)) {
		pr_debug("diag: In %s, channel is already opened, id: %d\n",
			 __func__, ch->type);
		return 0;
	}
	err = mhi_open_channel(ch->hdl);
	if (err) {
		pr_err("diag: In %s, unable to open ch, type: %d, err: %d\n",
		       __func__, ch->type, err);
		return err;
	}

	atomic_set(&ch->opened, 1);
	INIT_LIST_HEAD(&ch->buf_tbl);
	return 0;
}

static int mhi_buf_tbl_add(struct diag_mhi_info *mhi_info, int type,
			   void *buf, int len)
{
	unsigned long flags;
	struct diag_mhi_buf_tbl_t *item;
	struct diag_mhi_ch_t *ch = NULL;

	if (!mhi_info || !buf || len < 0)
		return -EINVAL;

	switch (type) {
	case TYPE_MHI_READ_CH:
		ch = &mhi_info->read_ch;
		break;
	case TYPE_MHI_WRITE_CH:
		ch = &mhi_info->write_ch;
		break;
	default:
		pr_err_ratelimited("diag: In %s, invalid type: %d\n",
				   __func__, type);
		return -EINVAL;
	}

	item = kzalloc(sizeof(struct diag_mhi_buf_tbl_t), GFP_KERNEL);
	if (!item) {
		return -ENOMEM;
	}
	kmemleak_not_leak(item);

	spin_lock_irqsave(&ch->lock, flags);
	item->buf = buf;
	item->len = len;
	list_add_tail(&item->link, &ch->buf_tbl);
	spin_unlock_irqrestore(&ch->lock, flags);

	return 0;
}

static void mhi_buf_tbl_remove(struct diag_mhi_info *mhi_info, int type,
			       void *buf, int len)
{
	int found = 0;
	unsigned long flags;
	struct list_head *start, *temp;
	struct diag_mhi_buf_tbl_t *item = NULL;
	struct diag_mhi_ch_t *ch = NULL;

	if (!mhi_info || !buf || len < 0)
		return;

	switch (type) {
	case TYPE_MHI_READ_CH:
		ch = &mhi_info->read_ch;
		break;
	case TYPE_MHI_WRITE_CH:
		ch = &mhi_info->write_ch;
		break;
	default:
		pr_err_ratelimited("diag: In %s, invalid type: %d\n",
				   __func__, type);
		return;
	}

	spin_lock_irqsave(&ch->lock, flags);
	list_for_each_safe(start, temp, &ch->buf_tbl) {
		item = list_entry(start, struct diag_mhi_buf_tbl_t, link);
		if (item->buf != buf)
			continue;
		list_del(&item->link);
		if (type == TYPE_MHI_READ_CH)
			diagmem_free(driver, item->buf, mhi_info->mempool);
		kfree(item);
		found = 1;
	}
	spin_unlock_irqrestore(&ch->lock, flags);

	if (!found) {
		pr_err_ratelimited("diag: In %s, unable to find buffer, ch: %pK, type: %d, buf: %pK\n",
				   __func__, ch, ch->type, buf);
	}
}

static void mhi_buf_tbl_clear(struct diag_mhi_info *mhi_info)
{
	unsigned long flags;
	struct list_head *start, *temp;
	struct diag_mhi_buf_tbl_t *item = NULL;
	struct diag_mhi_ch_t *ch = NULL;

	if (!mhi_info || !mhi_info->enabled)
		return;

	/* Clear all the pending reads */
	ch = &mhi_info->read_ch;
	/* At this point, the channel should already by closed */
	if (!(atomic_read(&ch->opened))) {
		spin_lock_irqsave(&ch->lock, flags);
		list_for_each_safe(start, temp, &ch->buf_tbl) {
			item = list_entry(start, struct diag_mhi_buf_tbl_t,
					  link);
			list_del(&item->link);
			diagmem_free(driver, item->buf, mhi_info->mempool);
			kfree(item);

		}
		spin_unlock_irqrestore(&ch->lock, flags);
	}

	/* Clear all the pending writes */
	ch = &mhi_info->write_ch;
	/* At this point, the channel should already by closed */
	if (!(atomic_read(&ch->opened))) {
		spin_lock_irqsave(&ch->lock, flags);
		list_for_each_safe(start, temp, &ch->buf_tbl) {
			item = list_entry(start, struct diag_mhi_buf_tbl_t,
					  link);
			list_del(&item->link);
			diag_remote_dev_write_done(mhi_info->dev_id, item->buf,
						   item->len, mhi_info->id);
			kfree(item);

		}
		spin_unlock_irqrestore(&ch->lock, flags);
	}
}

static int __mhi_close(struct diag_mhi_info *mhi_info, int close_flag)
{
	if (!mhi_info)
		return -EIO;

	if (!mhi_info->enabled)
		return -ENODEV;

	if (close_flag == CLOSE_CHANNELS) {
		atomic_set(&(mhi_info->read_ch.opened), 0);
		atomic_set(&(mhi_info->write_ch.opened), 0);
	}

	if (!(atomic_read(&(mhi_info->read_ch.opened)))) {
		flush_workqueue(mhi_info->mhi_wq);
		mhi_close_channel(mhi_info->read_ch.hdl);
	}

	if (!(atomic_read(&(mhi_info->write_ch.opened)))) {
		flush_workqueue(mhi_info->mhi_wq);
		mhi_close_channel(mhi_info->write_ch.hdl);
	}

	mhi_buf_tbl_clear(mhi_info);
	diag_remote_dev_close(mhi_info->dev_id);
	return 0;
}

static int mhi_close(int id)
{
	if (id < 0 || id >= NUM_MHI_DEV) {
		pr_err("diag: In %s, invalid index %d\n", __func__, id);
		return -EINVAL;
	}

	if (!diag_mhi[id].enabled)
		return -ENODEV;
	/*
	 * This function is called whenever the channel needs to be closed
	 * explicitly by Diag. Close both the read and write channels (denoted
	 * by CLOSE_CHANNELS flag)
	 */
	return __mhi_close(&diag_mhi[id], CLOSE_CHANNELS);
}

static void mhi_close_work_fn(struct work_struct *work)
{
	struct diag_mhi_info *mhi_info = container_of(work,
						      struct diag_mhi_info,
						      close_work);
	/*
	 * This is a part of work function which is queued after the channels
	 * are explicitly closed. Do not close channels again (denoted by
	 * CHANNELS_CLOSED flag)
	 */
	if (mhi_info)
		__mhi_close(mhi_info, CHANNELS_CLOSED);
}

static int __mhi_open(struct diag_mhi_info *mhi_info, int open_flag)
{
	int err = 0;
	unsigned long flags;

	if (!mhi_info)
		return -EIO;

	if (open_flag == OPEN_CHANNELS) {
		if (!atomic_read(&mhi_info->read_ch.opened)) {
			err = mhi_ch_open(&mhi_info->read_ch);
			if (err)
				goto fail;
			DIAG_LOG(DIAG_DEBUG_BRIDGE,
				 "opened mhi read channel, port: %d\n",
				 mhi_info->id);
		}
		if (!atomic_read(&mhi_info->write_ch.opened)) {
			err = mhi_ch_open(&mhi_info->write_ch);
			if (err)
				goto fail;
			DIAG_LOG(DIAG_DEBUG_BRIDGE,
				 "opened mhi write channel, port: %d\n",
				 mhi_info->id);
		}
	} else if (open_flag == CHANNELS_OPENED) {
		if (!atomic_read(&(mhi_info->read_ch.opened)) ||
		    !atomic_read(&(mhi_info->write_ch.opened))) {
			return -ENODEV;
		}
	}

	spin_lock_irqsave(&mhi_info->lock, flags);
	mhi_info->enabled = 1;
	spin_unlock_irqrestore(&mhi_info->lock, flags);
	diag_remote_dev_open(mhi_info->dev_id);
	queue_work(mhi_info->mhi_wq, &(mhi_info->read_work));
	return 0;

fail:
	pr_err("diag: Failed to open mhi channlels, err: %d\n", err);
	mhi_close(mhi_info->id);
	return err;
}

static int mhi_open(int id)
{
	if (id < 0 || id >= NUM_MHI_DEV) {
		pr_err("diag: In %s, invalid index %d\n", __func__, id);
		return -EINVAL;
	}

	if (!diag_mhi[id].enabled)
		return -ENODEV;
	/*
	 * This function is called whenever the channel needs to be opened
	 * explicitly by Diag. Open both the read and write channels (denoted by
	 * OPEN_CHANNELS flag)
	 */
	__mhi_open(&diag_mhi[id], OPEN_CHANNELS);
	diag_remote_dev_open(diag_mhi[id].dev_id);
	queue_work(diag_mhi[id].mhi_wq, &(diag_mhi[id].read_work));

	return 0;
}

static void mhi_open_work_fn(struct work_struct *work)
{
	struct diag_mhi_info *mhi_info = container_of(work,
						      struct diag_mhi_info,
						      open_work);
	/*
	 * This is a part of work function which is queued after the channels
	 * are explicitly opened. Do not open channels again (denoted by
	 * CHANNELS_OPENED flag)
	 */
	if (mhi_info) {
		diag_remote_dev_open(mhi_info->dev_id);
		queue_work(mhi_info->mhi_wq, &(mhi_info->read_work));
	}
}

static void mhi_read_done_work_fn(struct work_struct *work)
{
	unsigned char *buf = NULL;
	struct mhi_result result;
	int err = 0;
	struct diag_mhi_info *mhi_info = container_of(work,
						      struct diag_mhi_info,
						      read_done_work);
	if (!mhi_info)
		return;

	do {
		if (!(atomic_read(&(mhi_info->read_ch.opened))))
			break;
		err = mhi_poll_inbound(mhi_info->read_ch.hdl, &result);
		if (err) {
			pr_debug("diag: In %s, err %d\n", __func__, err);
			break;
		}
		buf = result.buf_addr;
		if (!buf)
			break;
		DIAG_LOG(DIAG_DEBUG_BRIDGE,
			 "read from mhi port %d buf %pK\n",
			 mhi_info->id, buf);
		/*
		 * The read buffers can come after the MHI channels are closed.
		 * If the channels are closed at the time of read, discard the
		 * buffers here and do not forward them to the mux layer.
		 */
		if ((atomic_read(&(mhi_info->read_ch.opened)))) {
			diag_remote_dev_read_done(mhi_info->dev_id, buf,
						  result.bytes_xferd);
		} else {
			mhi_buf_tbl_remove(mhi_info, TYPE_MHI_READ_CH, buf,
					   result.bytes_xferd);
		}
	} while (buf);
}

static void mhi_read_work_fn(struct work_struct *work)
{
	int err = 0;
	unsigned char *buf = NULL;
	enum MHI_FLAGS mhi_flags = MHI_EOT;
	struct diag_mhi_ch_t *read_ch = NULL;
	unsigned long flags;
	struct diag_mhi_info *mhi_info = container_of(work,
						      struct diag_mhi_info,
						      read_work);
	if (!mhi_info)
		return;

	read_ch = &mhi_info->read_ch;
	do {
		if (!(atomic_read(&(read_ch->opened))))
			break;

		buf = diagmem_alloc(driver, DIAG_MDM_BUF_SIZE,
				    mhi_info->mempool);
		if (!buf)
			break;

		err = mhi_buf_tbl_add(mhi_info, TYPE_MHI_READ_CH, buf,
				      DIAG_MDM_BUF_SIZE);
		if (err)
			goto fail;

		DIAG_LOG(DIAG_DEBUG_BRIDGE,
			 "queueing a read buf %pK, ch: %s\n",
			 buf, mhi_info->name);
		spin_lock_irqsave(&read_ch->lock, flags);
		err = mhi_queue_xfer(read_ch->hdl, buf, DIAG_MDM_BUF_SIZE,
				     mhi_flags);
		spin_unlock_irqrestore(&read_ch->lock, flags);
		if (err) {
			pr_err_ratelimited("diag: Unable to read from MHI channel %s, err: %d\n",
					   mhi_info->name, err);
			goto fail;
		}
	} while (buf);

	return;
fail:
	mhi_buf_tbl_remove(mhi_info, TYPE_MHI_READ_CH, buf, DIAG_MDM_BUF_SIZE);
	queue_work(mhi_info->mhi_wq, &mhi_info->read_work);
}

static int mhi_queue_read(int id)
{
	if (id < 0 || id >= NUM_MHI_DEV) {
		pr_err_ratelimited("diag: In %s, invalid index %d\n", __func__,
				   id);
		return -EINVAL;
	}
	queue_work(diag_mhi[id].mhi_wq, &(diag_mhi[id].read_work));
	return 0;
}

static int mhi_write(int id, unsigned char *buf, int len, int ctxt)
{
	int err = 0;
	enum MHI_FLAGS mhi_flags = MHI_EOT;
	unsigned long flags;
	struct diag_mhi_ch_t *ch = NULL;

	if (id < 0 || id >= NUM_MHI_DEV) {
		pr_err_ratelimited("diag: In %s, invalid index %d\n", __func__,
				   id);
		return -EINVAL;
	}

	if (!buf || len <= 0) {
		pr_err("diag: In %s, ch %d, invalid buf %pK len %d\n",
			__func__, id, buf, len);
		return -EINVAL;
	}

	if (!diag_mhi[id].enabled) {
		pr_err_ratelimited("diag: In %s, MHI channel %s is not enabled\n",
				   __func__, diag_mhi[id].name);
		return -EIO;
	}

	ch = &diag_mhi[id].write_ch;
	if (!(atomic_read(&(ch->opened)))) {
		pr_err_ratelimited("diag: In %s, MHI write channel %s is not open\n",
				   __func__, diag_mhi[id].name);
		return -EIO;
	}

	err = mhi_buf_tbl_add(&diag_mhi[id], TYPE_MHI_WRITE_CH, buf,
			      len);
	if (err)
		goto fail;

	spin_lock_irqsave(&ch->lock, flags);
	err = mhi_queue_xfer(ch->hdl, buf, len, mhi_flags);
	spin_unlock_irqrestore(&ch->lock, flags);
	if (err) {
		pr_err_ratelimited("diag: In %s, cannot write to MHI channel %pK, len %d, err: %d\n",
				   __func__, diag_mhi[id].name, len, err);
		mhi_buf_tbl_remove(&diag_mhi[id], TYPE_MHI_WRITE_CH, buf, len);
		goto fail;
	}

	return 0;
fail:
	return err;
}

static int mhi_fwd_complete(int id, unsigned char *buf, int len, int ctxt)
{
	if (id < 0 || id >= NUM_MHI_DEV) {
		pr_err_ratelimited("diag: In %s, invalid index %d\n", __func__,
				   id);
		return -EINVAL;
	}

	if (!buf)
		return -EINVAL;

	mhi_buf_tbl_remove(&diag_mhi[id], TYPE_MHI_READ_CH, buf, len);
	queue_work(diag_mhi[id].mhi_wq, &(diag_mhi[id].read_work));
	return 0;
}

static void mhi_notifier(struct mhi_cb_info *cb_info)
{
	int index;
	int type;
	int err = 0;
	struct mhi_result *result = NULL;
	struct diag_mhi_ch_t *ch = NULL;
	void *buf = NULL;

	if (!cb_info)
		return;

	result = cb_info->result;
	if (!result) {
		pr_err_ratelimited("diag: failed to obtain mhi result from callback\n");
		return;
	}

	index = GET_INFO_INDEX((uintptr_t)cb_info->result->user_data);
	if (index < 0 || index >= NUM_MHI_DEV) {
		pr_err_ratelimited("diag: In %s, invalid MHI index %d\n",
				   __func__, index);
		return;
	}

	type = GET_CH_TYPE((uintptr_t)cb_info->result->user_data);
	switch (type) {
	case TYPE_MHI_READ_CH:
		ch = &diag_mhi[index].read_ch;
		break;
	case TYPE_MHI_WRITE_CH:
		ch = &diag_mhi[index].write_ch;
		break;
	default:
		pr_err_ratelimited("diag: In %s, invalid channel type %d\n",
				   __func__, type);
		return;
	}

	switch (cb_info->cb_reason) {
	case MHI_CB_MHI_ENABLED:
		DIAG_LOG(DIAG_DEBUG_BRIDGE,
			 "received mhi enabled notifiation port: %d ch: %d\n",
			 index, ch->type);
		err = mhi_ch_open(ch);
		if (err)
			break;
		if (ch->type == TYPE_MHI_READ_CH) {
			diag_mhi[index].num_read = mhi_get_free_desc(ch->hdl);
			if (diag_mhi[index].num_read <= 0) {
				pr_err("diag: In %s, invalid number of descriptors %d\n",
				       __func__, diag_mhi[index].num_read);
				break;
			}
		}
		__mhi_open(&diag_mhi[index], CHANNELS_OPENED);
		queue_work(diag_mhi[index].mhi_wq,
			   &(diag_mhi[index].open_work));
		break;
	case MHI_CB_MHI_DISABLED:
		DIAG_LOG(DIAG_DEBUG_BRIDGE,
			 "received mhi disabled notifiation port: %d ch: %d\n",
			 index, ch->type);
		atomic_set(&(ch->opened), 0);
		__mhi_close(&diag_mhi[index], CHANNELS_CLOSED);
		break;
	case MHI_CB_XFER:
		/*
		 * If the channel is a read channel, this is a read
		 * complete notification - write complete if the channel is
		 * a write channel.
		 */
		if (type == TYPE_MHI_READ_CH) {
			if (!atomic_read(&(diag_mhi[index].read_ch.opened)))
				break;

			queue_work(diag_mhi[index].mhi_wq,
				   &(diag_mhi[index].read_done_work));
			break;
		}
		buf = result->buf_addr;
		if (!buf) {
			pr_err_ratelimited("diag: In %s, unable to de-serialize the data\n",
					   __func__);
			break;
		}
		mhi_buf_tbl_remove(&diag_mhi[index], TYPE_MHI_WRITE_CH, buf,
				   result->bytes_xferd);
		diag_remote_dev_write_done(diag_mhi[index].dev_id, buf,
					   result->bytes_xferd,
					   diag_mhi[index].id);
		break;
	default:
		pr_err("diag: In %s, invalid cb reason 0x%x\n", __func__,
		       cb_info->cb_reason);
		break;
	}

	return;
}

static struct diag_remote_dev_ops diag_mhi_fwd_ops = {
	.open = mhi_open,
	.close = mhi_close,
	.queue_read = mhi_queue_read,
	.write = mhi_write,
	.fwd_complete = mhi_fwd_complete,
};

static int diag_mhi_register_ch(int id, struct diag_mhi_ch_t *ch)
{
	int ctxt = 0;
	if (!ch)
		return -EIO;
	if (id < 0 || id >= NUM_MHI_DEV)
		return -EINVAL;
	spin_lock_init(&ch->lock);
	atomic_set(&(ch->opened), 0);
	ctxt = SET_CH_CTXT(id, ch->type);
	ch->client_info.mhi_client_cb = mhi_notifier;
	return mhi_register_channel(&ch->hdl, ch->chan, 0, &ch->client_info,
				    (void *)(uintptr_t)ctxt);
}

int diag_mhi_init()
{
	int i;
	int err = 0;
	struct diag_mhi_info *mhi_info = NULL;
	char wq_name[DIAG_MHI_NAME_SZ + DIAG_MHI_STRING_SZ];

	for (i = 0; i < NUM_MHI_DEV; i++) {
		mhi_info = &diag_mhi[i];
		spin_lock_init(&mhi_info->lock);
		INIT_WORK(&(mhi_info->read_work), mhi_read_work_fn);
		INIT_WORK(&(mhi_info->read_done_work), mhi_read_done_work_fn);
		INIT_WORK(&(mhi_info->open_work), mhi_open_work_fn);
		INIT_WORK(&(mhi_info->close_work), mhi_close_work_fn);
		strlcpy(wq_name, "diag_mhi_", DIAG_MHI_STRING_SZ);
		strlcat(wq_name, mhi_info->name, sizeof(mhi_info->name));
		diagmem_init(driver, mhi_info->mempool);
		mhi_info->mhi_wq = create_singlethread_workqueue(wq_name);
		if (!mhi_info->mhi_wq)
			goto fail;
		err = diagfwd_bridge_register(mhi_info->dev_id, mhi_info->id,
					      &diag_mhi_fwd_ops);
		if (err) {
			pr_err("diag: Unable to register MHI channel %d with bridge, err: %d\n",
			       i, err);
			goto fail;
		}
		err = diag_mhi_register_ch(mhi_info->id, &mhi_info->read_ch);
		if (err) {
			pr_err("diag: Unable to register MHI read channel for %d, err: %d\n",
			       i, err);
			goto fail;
		}
		err = diag_mhi_register_ch(mhi_info->id, &mhi_info->write_ch);
		if (err) {
			pr_err("diag: Unable to register MHI write channel for %d, err: %d\n",
			       i, err);
			goto fail;
		}
		DIAG_LOG(DIAG_DEBUG_BRIDGE, "mhi port %d is initailzed\n", i);
	}

	return 0;
fail:
	diag_mhi_exit();
	return -ENOMEM;
}

void diag_mhi_exit()
{
	int i;
	struct diag_mhi_info *mhi_info = NULL;

	for (i = 0; i < NUM_MHI_DEV; i++) {
		mhi_info = &diag_mhi[i];
		if (mhi_info->mhi_wq)
			destroy_workqueue(mhi_info->mhi_wq);
		mhi_close(mhi_info->id);
		diagmem_exit(driver, mhi_info->mempool);
	}
}

