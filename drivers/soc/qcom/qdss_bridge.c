/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#define KMSG_COMPONENT "QDSS diag bridge"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direction.h>
#include <linux/mhi.h>
#include <linux/usb/usb_qdss.h>
#include <linux/of.h>
#include "qdss_bridge.h"

#define MODULE_NAME "qdss_bridge"

#define QDSS_BUF_SIZE		(16*1024)
#define MHI_CLIENT_QDSS_IN	9

/* Max number of objects needed */
static int poolsize = 32;
module_param(poolsize, int, 0644);

/* Size of single buffer */
static int itemsize = QDSS_BUF_SIZE;
module_param(itemsize, int, 0644);

static int qdss_destroy_buf_tbl(struct qdss_bridge_drvdata *drvdata)
{
	struct list_head *start, *temp;
	struct qdss_buf_tbl_lst *entry = NULL;

	list_for_each_safe(start, temp, &drvdata->buf_tbl) {
		entry = list_entry(start, struct qdss_buf_tbl_lst, link);
		list_del(&entry->link);
		kfree(entry->buf);
		kfree(entry->usb_req);
		kfree(entry);
	}

	return 0;
}

static int qdss_create_buf_tbl(struct qdss_bridge_drvdata *drvdata)
{
	struct qdss_buf_tbl_lst *entry;
	void *buf;
	struct qdss_request *usb_req;
	int i;

	for (i = 0; i < poolsize; i++) {
		entry = kzalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry)
			goto err;

		buf = kzalloc(QDSS_BUF_SIZE, GFP_KERNEL);
		usb_req = kzalloc(sizeof(*usb_req), GFP_KERNEL);

		entry->buf = buf;
		entry->usb_req = usb_req;
		atomic_set(&entry->available, 1);
		list_add_tail(&entry->link, &drvdata->buf_tbl);

		if (!buf || !usb_req)
			goto err;
	}

	return 0;
err:
	qdss_destroy_buf_tbl(drvdata);
	return -ENOMEM;
}

struct qdss_buf_tbl_lst *qdss_get_buf_tbl_entry(
					struct qdss_bridge_drvdata *drvdata,
					void *buf)
{
	struct qdss_buf_tbl_lst *entry;

	list_for_each_entry(entry, &drvdata->buf_tbl, link) {
		if (atomic_read(&entry->available))
			continue;
		if (entry->buf == buf)
			return entry;
	}

	return NULL;
}

struct qdss_buf_tbl_lst *qdss_get_entry(struct qdss_bridge_drvdata *drvdata)
{
	struct qdss_buf_tbl_lst *item;

	list_for_each_entry(item, &drvdata->buf_tbl, link)
		if (atomic_cmpxchg(&item->available, 1, 0) == 1)
			return item;

	return NULL;
}

static void qdss_buf_tbl_remove(struct qdss_bridge_drvdata *drvdata,
				void *buf)
{
	struct qdss_buf_tbl_lst *entry = NULL;

	list_for_each_entry(entry, &drvdata->buf_tbl, link) {
		if (entry->buf != buf)
			continue;
		atomic_set(&entry->available, 1);
		return;
	}

	pr_err_ratelimited("Failed to find buffer for removal\n");
}

static void mhi_ch_close(struct qdss_bridge_drvdata *drvdata)
{
	flush_workqueue(drvdata->mhi_wq);
	qdss_destroy_buf_tbl(drvdata);
	mhi_unprepare_from_transfer(drvdata->mhi_dev);
}

static void mhi_read_work_fn(struct work_struct *work)
{
	int err = 0;
	enum MHI_FLAGS mhi_flags = MHI_EOT;
	struct qdss_buf_tbl_lst *entry;

	struct qdss_bridge_drvdata *drvdata =
				container_of(work,
					     struct qdss_bridge_drvdata,
					     read_work);

	do {
		if (!drvdata->opened)
			break;
		entry = qdss_get_entry(drvdata);
		if (!entry)
			break;

		err = mhi_queue_transfer(drvdata->mhi_dev, DMA_FROM_DEVICE,
					entry->buf, QDSS_BUF_SIZE, mhi_flags);
		if (err) {
			pr_err_ratelimited("Unable to read from MHI buffer err:%d",
					   err);
			goto fail;
		}
	} while (entry);

	return;
fail:
	qdss_buf_tbl_remove(drvdata, entry->buf);
	queue_work(drvdata->mhi_wq, &drvdata->read_work);
}

static int mhi_queue_read(struct qdss_bridge_drvdata *drvdata)
{
	queue_work(drvdata->mhi_wq, &(drvdata->read_work));
	return 0;
}

static int usb_write(struct qdss_bridge_drvdata *drvdata,
			     unsigned char *buf, size_t len)
{
	int ret = 0;
	struct qdss_buf_tbl_lst *entry;

	entry = qdss_get_buf_tbl_entry(drvdata, buf);
	if (!entry)
		return -EINVAL;

	entry->usb_req->buf = buf;
	entry->usb_req->length = len;
	ret = usb_qdss_write(drvdata->usb_ch, entry->usb_req);

	return ret;
}

static void mhi_read_done_work_fn(struct work_struct *work)
{
	unsigned char *buf = NULL;
	int err = 0;
	size_t len = 0;
	struct qdss_mhi_buf_tbl_t *tp, *_tp;
	struct qdss_bridge_drvdata *drvdata =
				container_of(work,
					     struct qdss_bridge_drvdata,
					     read_done_work);
	LIST_HEAD(head);

	do {
		if (!(drvdata->opened))
			break;
		spin_lock_bh(&drvdata->lock);
		if (list_empty(&drvdata->read_done_list)) {
			spin_unlock_bh(&drvdata->lock);
			break;
		}
		list_splice_tail_init(&drvdata->read_done_list, &head);
		spin_unlock_bh(&drvdata->lock);

		list_for_each_entry_safe(tp, _tp, &head, link) {
			list_del(&tp->link);
			buf = tp->buf;
			len = tp->len;
			kfree(tp);
			if (!buf)
				break;
			pr_debug("Read from mhi buf %pK len:%zd\n", buf, len);
			/*
			 * The read buffers can come after the MHI channels are
			 * closed. If the channels are closed at the time of
			 * read, discard the buffers here and do not forward
			 * them to the mux layer.
			 */
			if (drvdata->opened) {
				err = usb_write(drvdata, buf, len);
				if (err)
					qdss_buf_tbl_remove(drvdata, buf);
			} else {
				qdss_buf_tbl_remove(drvdata, buf);
			}
		}
		list_del_init(&head);
	} while (buf);
}

static void usb_write_done(struct qdss_bridge_drvdata *drvdata,
				   struct qdss_request *d_req)
{
	if (d_req->status) {
		pr_err_ratelimited("USB write failed err:%d\n", d_req->status);
		mhi_queue_read(drvdata);
		return;
	}
	qdss_buf_tbl_remove(drvdata, d_req->buf);
	mhi_queue_read(drvdata);
}

static void usb_notifier(void *priv, unsigned int event,
			struct qdss_request *d_req, struct usb_qdss_ch *ch)
{
	struct qdss_bridge_drvdata *drvdata = priv;

	if (!drvdata)
		return;

	switch (event) {
	case USB_QDSS_CONNECT:
		usb_qdss_alloc_req(ch, poolsize, 0);
		mhi_queue_read(drvdata);
		break;

	case USB_QDSS_DISCONNECT:
		/* Leave MHI/USB open.Only close on MHI disconnect */
		break;

	case USB_QDSS_DATA_WRITE_DONE:
		usb_write_done(drvdata, d_req);
		break;

	default:
		break;
	}
}

static int mhi_ch_open(struct qdss_bridge_drvdata *drvdata)
{
	int ret;

	if (drvdata->opened)
		return 0;
	ret = mhi_prepare_for_transfer(drvdata->mhi_dev);
	if (ret) {
		pr_err("Unable to open MHI channel\n");
		return ret;
	}

	spin_lock_bh(&drvdata->lock);
	drvdata->opened = 1;
	spin_unlock_bh(&drvdata->lock);
	return 0;
}

static void qdss_bridge_open_work_fn(struct work_struct *work)
{
	struct qdss_bridge_drvdata *drvdata =
				container_of(work,
					     struct qdss_bridge_drvdata,
					     open_work);
	int ret;

	ret = mhi_ch_open(drvdata);
	if (ret)
		goto err_open;

	ret = qdss_create_buf_tbl(drvdata);
	if (ret)
		goto err;

	drvdata->usb_ch = usb_qdss_open("qdss_mdm", drvdata, usb_notifier);
	if (IS_ERR_OR_NULL(drvdata->usb_ch)) {
		ret = PTR_ERR(drvdata->usb_ch);
		goto err;
	}

	return;
err:
	mhi_ch_close(drvdata);
err_open:
	pr_err("Open work failed with err:%d\n", ret);
}

static void qdss_mhi_write_cb(struct mhi_device *mhi_dev,
				struct mhi_result *result)
{
}

static void qdss_mhi_read_cb(struct mhi_device *mhi_dev,
				struct mhi_result *result)
{
	struct qdss_bridge_drvdata *drvdata = NULL;
	struct qdss_mhi_buf_tbl_t *tp;
	void *buf = NULL;

	drvdata = mhi_dev->priv_data;
	if (!drvdata)
		return;
	buf = result->buf_addr;

	if (drvdata->opened &&
	    result->transaction_status != -ENOTCONN) {
		tp = kmalloc(sizeof(*tp), GFP_ATOMIC);
		if (!tp)
			return;
		tp->buf = buf;
		tp->len = result->bytes_xferd;
		spin_lock_bh(&drvdata->lock);
		list_add_tail(&tp->link, &drvdata->read_done_list);
		spin_unlock_bh(&drvdata->lock);
		queue_work(drvdata->mhi_wq, &drvdata->read_done_work);
	} else {
		qdss_buf_tbl_remove(drvdata, buf);
	}
}

static void qdss_mhi_remove(struct mhi_device *mhi_dev)
{
	struct qdss_bridge_drvdata *drvdata = NULL;

	if (!mhi_dev)
		return;
	drvdata = mhi_dev->priv_data;
	if (!drvdata)
		return;
	if (!drvdata->opened)
		return;
	spin_lock_bh(&drvdata->lock);
	drvdata->opened = 0;
	spin_unlock_bh(&drvdata->lock);
	usb_qdss_close(drvdata->usb_ch);
	flush_workqueue(drvdata->mhi_wq);
	qdss_destroy_buf_tbl(drvdata);
}

int qdss_mhi_init(struct qdss_bridge_drvdata *drvdata)
{
	drvdata->mhi_wq = create_singlethread_workqueue(MODULE_NAME);
	if (!drvdata->mhi_wq)
		return -ENOMEM;

	spin_lock_init(&drvdata->lock);
	INIT_WORK(&(drvdata->read_work), mhi_read_work_fn);
	INIT_WORK(&(drvdata->read_done_work), mhi_read_done_work_fn);
	INIT_WORK(&(drvdata->open_work), qdss_bridge_open_work_fn);
	INIT_LIST_HEAD(&drvdata->buf_tbl);
	INIT_LIST_HEAD(&drvdata->read_done_list);
	drvdata->opened = 0;

	return 0;
}

static int qdss_mhi_probe(struct mhi_device *mhi_dev,
				const struct mhi_device_id *id)
{
	int ret;
	struct qdss_bridge_drvdata *drvdata;

	drvdata = devm_kzalloc(&mhi_dev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		ret = -ENOMEM;
		return ret;
	}

	drvdata->mhi_dev = mhi_dev;
	mhi_device_set_devdata(mhi_dev, drvdata);

	ret = qdss_mhi_init(drvdata);
	if (ret)
		goto err;
	queue_work(drvdata->mhi_wq, &drvdata->open_work);
	return 0;
err:
	pr_err("Device probe failed err:%d\n", ret);
	return ret;
}

static const struct mhi_device_id qdss_mhi_match_table[] = {
	{ .chan = "QDSS" },
	{},
};

static struct mhi_driver qdss_mhi_driver = {
	.id_table = qdss_mhi_match_table,
	.probe = qdss_mhi_probe,
	.remove = qdss_mhi_remove,
	.dl_xfer_cb = qdss_mhi_read_cb,
	.ul_xfer_cb = qdss_mhi_write_cb,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
	}
};

static int __init qdss_bridge_init(void)
{
	return mhi_driver_register(&qdss_mhi_driver);
}

static void __exit qdss_bridge_exit(void)
{
	mhi_driver_unregister(&qdss_mhi_driver);
}

module_init(qdss_bridge_init);
module_exit(qdss_bridge_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("QDSS Bridge driver");
