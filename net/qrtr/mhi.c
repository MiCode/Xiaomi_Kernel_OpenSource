/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/mod_devicetable.h>
#include <linux/mhi.h>

#include "qrtr.h"

struct qrtr_mhi_dev {
	struct qrtr_endpoint ep;
	struct mhi_device *mhi_dev;
	struct device *dev;
	struct completion ul_done;
};

/* from mhi to qrtr */
static void qcom_mhi_qrtr_dl_callback(struct mhi_device *mhi_dev,
				      struct mhi_result *mhi_res)
{
	struct qrtr_mhi_dev *qdev = dev_get_drvdata(&mhi_dev->dev);
	int rc;

	if (!qdev || mhi_res->transaction_status)
		return;

	rc = qrtr_endpoint_post(&qdev->ep, mhi_res->buf_addr,
				mhi_res->bytes_xferd);
	if (rc == -EINVAL)
		dev_err(qdev->dev, "invalid ipcrouter packet\n");
}

/* from mhi to qrtr */
static void qcom_mhi_qrtr_ul_callback(struct mhi_device *mhi_dev,
				      struct mhi_result *mhi_res)
{
	struct qrtr_mhi_dev *qdev = dev_get_drvdata(&mhi_dev->dev);
	struct sk_buff *skb = mhi_res->buf_addr;

	if (!mhi_res->transaction_status) {
		complete(&qdev->ul_done);
		consume_skb(skb);
	} else {
		kfree_skb(skb);
	}
}

/* from qrtr to mhi */
static int qcom_mhi_qrtr_send(struct qrtr_endpoint *ep, struct sk_buff *skb)
{
	struct qrtr_mhi_dev *qdev = container_of(ep, struct qrtr_mhi_dev, ep);
	int rc;

	rc = skb_linearize(skb);
	if (rc)
		goto out;

	reinit_completion(&qdev->ul_done);
	rc = mhi_queue_transfer(qdev->mhi_dev, DMA_TO_DEVICE, skb, skb->len,
				MHI_EOT);
	if (rc)
		goto out;

	rc = wait_for_completion_interruptible_timeout(&qdev->ul_done, HZ * 5);
	if (rc > 0)
		rc = 0;
	else if (rc == 0)
		rc = -ETIMEDOUT;

out:
	return rc;
}

static int qcom_mhi_qrtr_probe(struct mhi_device *mhi_dev,
			       const struct mhi_device_id *id)
{
	struct qrtr_mhi_dev *qdev;
	int rc;

	qdev = devm_kzalloc(&mhi_dev->dev, sizeof(*qdev), GFP_KERNEL);
	if (!qdev)
		return -ENOMEM;

	qdev->mhi_dev = mhi_dev;
	qdev->dev = &mhi_dev->dev;
	qdev->ep.xmit = qcom_mhi_qrtr_send;

	init_completion(&qdev->ul_done);

	rc = qrtr_endpoint_register(&qdev->ep, QRTR_EP_NID_AUTO);
	if (rc)
		return rc;

	dev_set_drvdata(&mhi_dev->dev, qdev);

	dev_dbg(qdev->dev, "Qualcomm MHI QRTR driver probed\n");

	return 0;
}

static void qcom_mhi_qrtr_remove(struct mhi_device *mhi_dev)
{
	struct qrtr_mhi_dev *qdev = dev_get_drvdata(&mhi_dev->dev);

	qrtr_endpoint_unregister(&qdev->ep);
	complete(&qdev->ul_done);
	dev_set_drvdata(&mhi_dev->dev, NULL);
}

static const struct mhi_device_id qcom_mhi_qrtr_mhi_match[] = {
	{ .chan = "IPCR" },
	{}
};

static struct mhi_driver qcom_mhi_qrtr_driver = {
	.probe = qcom_mhi_qrtr_probe,
	.remove = qcom_mhi_qrtr_remove,
	.dl_xfer_cb = qcom_mhi_qrtr_dl_callback,
	.ul_xfer_cb = qcom_mhi_qrtr_ul_callback,
	.id_table = qcom_mhi_qrtr_mhi_match,
	.driver = {
		.name = "qcom_mhi_qrtr",
		.owner = THIS_MODULE,
	},
};

module_driver(qcom_mhi_qrtr_driver, mhi_driver_register,
	      mhi_driver_unregister);

MODULE_DESCRIPTION("Qualcomm IPC-Router MHI interface driver");
MODULE_LICENSE("GPL v2");
