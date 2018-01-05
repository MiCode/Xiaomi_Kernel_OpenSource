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
#include <linux/msm_mhi.h>
#include <linux/usb/usb_qdss.h>
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
	mhi_close_channel(drvdata->hdl);
}

static void mhi_close_work_fn(struct work_struct *work)
{
	struct qdss_bridge_drvdata *drvdata =
				container_of(work,
					     struct qdss_bridge_drvdata,
					     close_work);

	usb_qdss_close(drvdata->usb_ch);
	mhi_ch_close(drvdata);
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

		err = mhi_queue_xfer(drvdata->hdl, entry->buf, QDSS_BUF_SIZE,
				      mhi_flags);
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
			     struct mhi_result *result)
{
	int ret = 0;
	struct qdss_buf_tbl_lst *entry;

	entry = qdss_get_buf_tbl_entry(drvdata, result->buf_addr);
	if (!entry)
		return -EINVAL;

	entry->usb_req->buf = result->buf_addr;
	entry->usb_req->length = result->bytes_xferd;
	ret = usb_qdss_data_write(drvdata->usb_ch, entry->usb_req);

	return ret;
}

static void mhi_read_done_work_fn(struct work_struct *work)
{
	unsigned char *buf = NULL;
	struct mhi_result result;
	int err = 0;
	struct qdss_bridge_drvdata *drvdata =
				container_of(work,
					     struct qdss_bridge_drvdata,
					     read_done_work);

	do {
		err = mhi_poll_inbound(drvdata->hdl, &result);
		if (err) {
			pr_debug("MHI poll failed err:%d\n", err);
			break;
		}
		buf = result.buf_addr;
		if (!buf)
			break;
		err = usb_write(drvdata, &result);
		if (err)
			qdss_buf_tbl_remove(drvdata, buf);
	} while (1);
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
		usb_qdss_alloc_req(drvdata->usb_ch, poolsize, 0);
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

	ret = mhi_open_channel(drvdata->hdl);
	if (ret) {
		pr_err("Unable to open MHI channel\n");
		return ret;
	}

	ret = mhi_get_free_desc(drvdata->hdl);
	if (ret <= 0)
		return -EIO;

	drvdata->opened = 1;
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

static void mhi_notifier(struct mhi_cb_info *cb_info)
{
	struct mhi_result *result;
	struct qdss_bridge_drvdata *drvdata;

	if (!cb_info)
		return;

	result = cb_info->result;
	if (!result) {
		pr_err_ratelimited("Failed to obtain MHI result\n");
		return;
	}

	drvdata = (struct qdss_bridge_drvdata *)cb_info->result->user_data;
	if (!drvdata) {
		pr_err_ratelimited("MHI returned invalid drvdata\n");
		return;
	}

	switch (cb_info->cb_reason) {
	case MHI_CB_MHI_ENABLED:
		queue_work(drvdata->mhi_wq, &drvdata->open_work);
		break;

	case MHI_CB_XFER:
		if (!drvdata->opened)
			break;

		queue_work(drvdata->mhi_wq, &drvdata->read_done_work);
		break;

	case MHI_CB_MHI_DISABLED:
		if (!drvdata->opened)
			break;

		drvdata->opened = 0;
		queue_work(drvdata->mhi_wq, &drvdata->close_work);
		break;

	case MHI_CB_SYS_ERROR:
	case MHI_CB_MHI_SHUTDOWN:
		drvdata->opened = 0;

		flush_workqueue(drvdata->mhi_wq);
		qdss_destroy_buf_tbl(drvdata);
		break;

	default:
		pr_err_ratelimited("MHI returned invalid cb reason 0x%x\n",
		       cb_info->cb_reason);
		break;
	}
}

static int qdss_mhi_register_ch(struct qdss_bridge_drvdata *drvdata)
{
	struct mhi_client_info_t *client_info;
	int ret;
	struct mhi_client_info_t *mhi_info;

	client_info = devm_kzalloc(drvdata->dev, sizeof(*client_info),
				   GFP_KERNEL);
	if (!client_info)
		return -ENOMEM;

	client_info->mhi_client_cb = mhi_notifier;
	drvdata->client_info = client_info;

	mhi_info = client_info;
	mhi_info->chan = MHI_CLIENT_QDSS_IN;
	mhi_info->dev = drvdata->dev;
	mhi_info->node_name = "qcom,mhi";
	mhi_info->user_data = drvdata;

	ret = mhi_register_channel(&drvdata->hdl, mhi_info);
	return ret;
}

int qdss_mhi_init(struct qdss_bridge_drvdata *drvdata)
{
	int ret;

	drvdata->mhi_wq = create_singlethread_workqueue(MODULE_NAME);
	if (!drvdata->mhi_wq)
		return -ENOMEM;

	INIT_WORK(&(drvdata->read_work), mhi_read_work_fn);
	INIT_WORK(&(drvdata->read_done_work), mhi_read_done_work_fn);
	INIT_WORK(&(drvdata->open_work), qdss_bridge_open_work_fn);
	INIT_WORK(&(drvdata->close_work), mhi_close_work_fn);
	INIT_LIST_HEAD(&drvdata->buf_tbl);
	drvdata->opened = 0;

	ret = qdss_mhi_register_ch(drvdata);
	if (ret) {
		destroy_workqueue(drvdata->mhi_wq);
		pr_err("Unable to register MHI read channel err:%d\n", ret);
		return ret;
	}

	return 0;
}

static int qdss_mhi_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct qdss_bridge_drvdata *drvdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		ret = -ENOMEM;
		return ret;
	}

	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	ret = qdss_mhi_init(drvdata);
	if (ret)
		goto err;

	return 0;
err:
	pr_err("Device probe failed err:%d\n", ret);
	return ret;
}

static const struct of_device_id qdss_mhi_table[] = {
	{.compatible = "qcom,qdss-mhi"},
	{},
};

static struct platform_driver qdss_mhi_driver = {
	.probe = qdss_mhi_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = qdss_mhi_table,
	},
};

static int __init qdss_bridge_init(void)
{
	return platform_driver_register(&qdss_mhi_driver);
}

static void __exit qdss_bridge_exit(void)
{
	platform_driver_unregister(&qdss_mhi_driver);
}

module_init(qdss_bridge_init);
module_exit(qdss_bridge_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("QDSS Bridge driver");
