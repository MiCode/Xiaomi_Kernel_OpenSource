/* Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
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
#include <linux/dma-mapping.h>
#include <linux/dma-direction.h>
#include <linux/mhi.h>
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
		.mempool_init = 0,
		.mhi_wq = NULL,
		.mhi_dev = NULL,
		.read_ch = {
			.type = TYPE_MHI_READ_CH,
		},
		.write_ch = {
			.type = TYPE_MHI_WRITE_CH,
		}
	},
	{
		.id = MHI_DCI_1,
		.dev_id = DIAGFWD_MDM_DCI,
		.name = "MDM_DCI",
		.enabled = 0,
		.num_read = 0,
		.mempool = POOL_TYPE_MDM_DCI,
		.mempool_init = 0,
		.mhi_wq = NULL,
		.mhi_dev = NULL,
		.read_ch = {
			.type = TYPE_MHI_READ_CH,
		},
		.write_ch = {
			.type = TYPE_MHI_WRITE_CH,
		}
	}
};

static int mhi_buf_tbl_add(struct diag_mhi_info *mhi_info, int type,
			   void *buf, int len)
{
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

	item = kzalloc(sizeof(*item), GFP_ATOMIC);
	if (!item)
		return -ENOMEM;
	kmemleak_not_leak(item);

	item->buf = buf;
	DIAG_LOG(DIAG_DEBUG_MHI,
		 "buffer %pK added to table of ch: %s\n", buf, mhi_info->name);
	item->len = len;
	list_add_tail(&item->link, &ch->buf_tbl);

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
		if (type == TYPE_MHI_READ_CH) {
			DIAG_LOG(DIAG_DEBUG_MHI,
			"Callback received on buffer:%pK from mhi\n", buf);
			diagmem_free(driver, item->buf, mhi_info->mempool);
		}
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
	struct diag_mhi_buf_tbl_t *tp = NULL, *tp_temp = NULL;
	struct diag_mhi_ch_t *ch = NULL;
	unsigned char *buf;

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
			buf = item->buf;
			list_for_each_entry_safe(tp, tp_temp,
				&mhi_info->read_done_list, link) {
				if (tp->buf == buf) {
					DIAG_LOG(DIAG_DEBUG_MHI,
						"buffer:%pK removed from table for ch:%s\n",
						buf, mhi_info->name);
					list_del(&tp->link);
					kfree(tp);
					tp = NULL;
				}
			}
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

	atomic_set(&(mhi_info->read_ch.opened), 0);
	atomic_set(&(mhi_info->write_ch.opened), 0);

	cancel_work(&mhi_info->read_work);
	cancel_work(&mhi_info->read_done_work);
	flush_workqueue(mhi_info->mhi_wq);

	if (close_flag == CLOSE_CHANNELS) {
		mutex_lock(&mhi_info->ch_mutex);
		mhi_unprepare_from_transfer(mhi_info->mhi_dev);
		mutex_unlock(&mhi_info->ch_mutex);
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

	if (!mhi_info)
		return -EIO;
	if (!mhi_info->enabled)
		return -ENODEV;
	if (open_flag == OPEN_CHANNELS) {
		if ((atomic_read(&(mhi_info->read_ch.opened))) &&
			(atomic_read(&(mhi_info->write_ch.opened))))
			return 0;
		mutex_lock(&mhi_info->ch_mutex);
		err = mhi_prepare_for_transfer(mhi_info->mhi_dev);
		mutex_unlock(&mhi_info->ch_mutex);
		if (err) {
			pr_err("diag: In %s, unable to open ch, err: %d\n",
				__func__, err);
			goto fail;
		}
		atomic_set(&mhi_info->read_ch.opened, 1);
		atomic_set(&mhi_info->write_ch.opened, 1);
		DIAG_LOG(DIAG_DEBUG_BRIDGE,
			 "opened mhi read/write channel, port: %d\n",
			mhi_info->id);
	} else if (open_flag == CHANNELS_OPENED) {
		if (!atomic_read(&(mhi_info->read_ch.opened)) ||
		    !atomic_read(&(mhi_info->write_ch.opened))) {
			return -ENODEV;
		}
	}

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
	int err = 0;
	int len;
	unsigned long flags;
	struct diag_mhi_buf_tbl_t *tp;
	struct diag_mhi_info *mhi_info = container_of(work,
						      struct diag_mhi_info,
						      read_done_work);
	if (!mhi_info)
		return;

	do {
		if (!(atomic_read(&(mhi_info->read_ch.opened))))
			break;
		spin_lock_irqsave(&mhi_info->read_ch.lock, flags);
		if (list_empty(&mhi_info->read_done_list)) {
			spin_unlock_irqrestore(&mhi_info->read_ch.lock, flags);
			break;
		}
		tp = list_first_entry(&mhi_info->read_done_list,
					struct diag_mhi_buf_tbl_t, link);
		list_del(&tp->link);
		buf = tp->buf;
		len = tp->len;
		kfree(tp);
		spin_unlock_irqrestore(&mhi_info->read_ch.lock, flags);
		if (!buf)
			break;
		DIAG_LOG(DIAG_DEBUG_BRIDGE,
			"read from mhi port %d buf %pK len:%d\n",
			mhi_info->id, buf, len);
		/*
		 * The read buffers can come after the MHI channels are closed.
		 * If the channels are closed at the time of read, discard the
		 * buffers here and do not forward them to the mux layer.
		 */
		if ((atomic_read(&(mhi_info->read_ch.opened)))) {
			err = diag_remote_dev_read_done(mhi_info->dev_id, buf,
						  len);
			if (err) {
				DIAG_LOG(DIAG_DEBUG_MHI,
				"diag: remove buf entry %pK for failing flush to sink\n",
				buf);
				mhi_buf_tbl_remove(mhi_info, TYPE_MHI_READ_CH,
					buf, len);
				break;
			}
		} else {
			DIAG_LOG(DIAG_DEBUG_MHI,
			"diag: remove buf entry %pK if channel is closed\n",
				buf);
			mhi_buf_tbl_remove(mhi_info, TYPE_MHI_READ_CH, buf,
					   len);
			break;
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

		spin_lock_irqsave(&read_ch->lock, flags);
		buf = diagmem_alloc(driver, DIAG_MDM_BUF_SIZE,
				    mhi_info->mempool);
		if (!buf) {
			spin_unlock_irqrestore(&read_ch->lock, flags);
			break;
		}
		DIAG_LOG(DIAG_DEBUG_MHI,
			 "Allocated buffer %pK, ch: %s\n", buf, mhi_info->name);

		err = mhi_buf_tbl_add(mhi_info, TYPE_MHI_READ_CH, buf,
				      DIAG_MDM_BUF_SIZE);
		if (err) {
			diagmem_free(driver, buf, mhi_info->mempool);
			spin_unlock_irqrestore(&read_ch->lock, flags);
			goto fail;
		}

		DIAG_LOG(DIAG_DEBUG_BRIDGE,
			 "queueing a read buf %pK, ch: %s\n",
			 buf, mhi_info->name);

		err = mhi_queue_transfer(mhi_info->mhi_dev, DMA_FROM_DEVICE,
					buf, DIAG_MDM_BUF_SIZE, mhi_flags);
		spin_unlock_irqrestore(&read_ch->lock, flags);
		if (err) {
			pr_err_ratelimited("diag: Unable to read from MHI channel %s, err: %d\n",
					   mhi_info->name, err);
			goto fail;
		}
	} while (buf);

	return;
fail:
	DIAG_LOG(DIAG_DEBUG_MHI,
		"diag: remove buf entry %pK for error\n", buf);
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

	spin_lock_irqsave(&ch->lock, flags);
	err = mhi_buf_tbl_add(&diag_mhi[id], TYPE_MHI_WRITE_CH, buf,
			      len);
	if (err)
		goto fail;

	err = mhi_queue_transfer(diag_mhi[id].mhi_dev, DMA_TO_DEVICE, buf,
				len, mhi_flags);
	spin_unlock_irqrestore(&ch->lock, flags);
	if (err) {
		DIAG_LOG(DIAG_DEBUG_MHI,
			"Cannot write to MHI channel %s, len %d, err: %d\n",
			  diag_mhi[id].name, len, err);
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

	DIAG_LOG(DIAG_DEBUG_MHI,
		"Remove buffer from mhi read table after write completion %pK len:%d\n",
		buf, len);
	mhi_buf_tbl_remove(&diag_mhi[id], TYPE_MHI_READ_CH, buf, len);
	queue_work(diag_mhi[id].mhi_wq, &(diag_mhi[id].read_work));
	return 0;
}

static int mhi_remote_proc_check(void)
{
	return diag_mhi[MHI_1].enabled;
}

static struct diag_mhi_info *diag_get_mhi_info(struct mhi_device *mhi_dev)
{
	struct diag_mhi_info *mhi_info = NULL;
	int i;

	for (i = 0; i < NUM_MHI_DEV; i++) {
		mhi_info = &diag_mhi[i];
		if (mhi_info->mhi_dev == mhi_dev)
			return mhi_info;
	}
	return NULL;
}
static void diag_mhi_read_cb(struct mhi_device *mhi_dev,
				struct mhi_result *result)
{
	struct diag_mhi_info *mhi_info = NULL;
	struct diag_mhi_buf_tbl_t *item = NULL;
	struct diag_mhi_buf_tbl_t *tp = NULL, *temp = NULL;
	unsigned long flags;
	void *buf = NULL;
	uint8_t queue_read = 0;

	if (!mhi_dev)
		return;
	mhi_info = diag_get_mhi_info(mhi_dev);
	if (!mhi_info)
		return;
	buf = result->buf_addr;

	if (!buf)
		return;
	if (atomic_read(&mhi_info->read_ch.opened) &&
	    result->transaction_status != -ENOTCONN) {
		spin_lock_irqsave(&mhi_info->read_ch.lock, flags);
		tp = kmalloc(sizeof(*tp), GFP_ATOMIC);
		if (!tp) {
			DIAG_LOG(DIAG_DEBUG_BRIDGE,
			"no mem for list\n");
			spin_unlock_irqrestore(&mhi_info->read_ch.lock, flags);
			return;
		}
		list_for_each_entry_safe(item, temp,
				&mhi_info->read_ch.buf_tbl, link) {
			if (item->buf == buf) {
				DIAG_LOG(DIAG_DEBUG_MHI,
				"Callback received on buffer:%pK from mhi\n",
					buf);
				tp->buf = buf;
				tp->len = result->bytes_xferd;
				list_add_tail(&tp->link,
					&mhi_info->read_done_list);
				queue_read = 1;
				break;
			}
		}
		spin_unlock_irqrestore(&mhi_info->read_ch.lock, flags);
		if (queue_read)
			queue_work(mhi_info->mhi_wq,
			&(mhi_info->read_done_work));
	} else {
		DIAG_LOG(DIAG_DEBUG_MHI,
		"Removing buf entry from read table if ch is not open %pK\n",
		buf);
		mhi_buf_tbl_remove(mhi_info, TYPE_MHI_READ_CH, buf,
					result->bytes_xferd);
	}
}

static void diag_mhi_write_cb(struct mhi_device *mhi_dev,
				struct mhi_result *result)
{
	struct diag_mhi_info *mhi_info = NULL;
	void *buf = NULL;

	if (!mhi_dev)
		return;
	mhi_info = diag_get_mhi_info(mhi_dev);
	if (!mhi_info)
		return;
	buf = result->buf_addr;
	if (!buf) {
		pr_err_ratelimited("diag: In %s, unable to de-serialize the data\n",
					__func__);
		return;
	}
	mhi_buf_tbl_remove(mhi_info, TYPE_MHI_WRITE_CH, buf,
				   result->bytes_xferd);
	diag_remote_dev_write_done(mhi_info->dev_id, buf,
					   result->bytes_xferd,
					   mhi_info->id);
}

static void diag_mhi_remove(struct mhi_device *mhi_dev)
{
	unsigned long flags;
	struct diag_mhi_info *mhi_info = NULL;

	if (!mhi_dev)
		return;
	mhi_info = diag_get_mhi_info(mhi_dev);
	if (!mhi_info)
		return;
	if (!mhi_info->enabled)
		return;
	__mhi_close(mhi_info, CHANNELS_CLOSED);
	spin_lock_irqsave(&mhi_info->lock, flags);
	mhi_info->enabled = 0;
	spin_unlock_irqrestore(&mhi_info->lock, flags);
}

static int diag_mhi_probe(struct mhi_device *mhi_dev,
			const struct mhi_device_id *id)
{
	int index = id->driver_data;
	int ret = 0;
	unsigned long flags;
	struct diag_mhi_info *mhi_info = &diag_mhi[index];

	DIAG_LOG(DIAG_DEBUG_BRIDGE,
		"received probe for %d\n",
		index);
	diag_mhi[index].mhi_dev = mhi_dev;
	DIAG_LOG(DIAG_DEBUG_BRIDGE,
		"diag: mhi device is ready to open\n");
	spin_lock_irqsave(&mhi_info->lock, flags);
	mhi_info->enabled = 1;
	spin_unlock_irqrestore(&mhi_info->lock, flags);
	__mhi_open(&diag_mhi[index], OPEN_CHANNELS);
	queue_work(diag_mhi[index].mhi_wq,
			   &(diag_mhi[index].open_work));
	return ret;
}

static struct diag_remote_dev_ops diag_mhi_fwd_ops = {
	.open = mhi_open,
	.close = mhi_close,
	.queue_read = mhi_queue_read,
	.write = mhi_write,
	.fwd_complete = mhi_fwd_complete,
	.remote_proc_check = mhi_remote_proc_check,
};

static void diag_mhi_dev_exit(int dev)
{
	struct diag_mhi_info *mhi_info = NULL;

	mhi_info = &diag_mhi[dev];
	if (!mhi_info)
		return;
	if (mhi_info->mhi_wq)
		destroy_workqueue(mhi_info->mhi_wq);
	mhi_close(mhi_info->id);
	if (mhi_info->mempool_init)
		diagmem_exit(driver, mhi_info->mempool);
}

int diag_mhi_init(void)
{
	int i;
	int err = 0;
	struct diag_mhi_info *mhi_info = NULL;
	char wq_name[DIAG_MHI_NAME_SZ + DIAG_MHI_STRING_SZ];

	for (i = 0; i < NUM_MHI_DEV; i++) {
		mhi_info = &diag_mhi[i];
		spin_lock_init(&mhi_info->lock);
		spin_lock_init(&mhi_info->read_ch.lock);
		spin_lock_init(&mhi_info->write_ch.lock);
		mutex_init(&mhi_info->ch_mutex);
		INIT_LIST_HEAD(&mhi_info->read_ch.buf_tbl);
		INIT_LIST_HEAD(&mhi_info->write_ch.buf_tbl);
		atomic_set(&(mhi_info->read_ch.opened), 0);
		atomic_set(&(mhi_info->write_ch.opened), 0);
		INIT_WORK(&(mhi_info->read_work), mhi_read_work_fn);
		INIT_LIST_HEAD(&mhi_info->read_done_list);
		INIT_WORK(&(mhi_info->read_done_work), mhi_read_done_work_fn);
		INIT_WORK(&(mhi_info->open_work), mhi_open_work_fn);
		INIT_WORK(&(mhi_info->close_work), mhi_close_work_fn);
		strlcpy(wq_name, "diag_mhi_", sizeof(wq_name));
		strlcat(wq_name, mhi_info->name, sizeof(wq_name));
		diagmem_init(driver, mhi_info->mempool);
		mhi_info->mempool_init = 1;
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
		DIAG_LOG(DIAG_DEBUG_BRIDGE, "mhi port %d is initailzed\n", i);
	}

	return 0;
fail:
	diag_mhi_dev_exit(i);
	return -ENOMEM;
}

void diag_mhi_exit(void)
{
	int i;

	for (i = 0; i < NUM_MHI_DEV; i++) {
		diag_mhi_dev_exit(i);
	}
}

static const struct mhi_device_id diag_mhi_match_table[] = {
	{ .chan = "DIAG", .driver_data = MHI_1 },
	{ .chan = "DCI", .driver_data = MHI_DCI_1 },
	{},
};

static struct mhi_driver diag_mhi_driver = {
	.id_table = diag_mhi_match_table,
	.remove = diag_mhi_remove,
	.probe = diag_mhi_probe,
	.ul_xfer_cb = diag_mhi_write_cb,
	.dl_xfer_cb = diag_mhi_read_cb,
	.driver = {
		.name = "diag_mhi_driver",
		.owner = THIS_MODULE,
	},
};

void diag_register_with_mhi(void)
{
	int ret = 0;

	ret = diag_remote_init();
	if (ret) {
		diag_remote_exit();
		return;
	}

	ret = diagfwd_bridge_init();
	if (ret) {
		diagfwd_bridge_exit();
		diag_remote_exit();
		return;
	}

	mhi_driver_register(&diag_mhi_driver);
}
