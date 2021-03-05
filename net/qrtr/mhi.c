/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include <net/sock.h>
#include <linux/of.h>

#include "qrtr.h"

struct qrtr_mhi_dev {
	struct qrtr_endpoint ep;
	struct mhi_device *mhi_dev;
	struct device *dev;
	spinlock_t ul_lock;		/* lock to protect ul_pkts */
	struct list_head ul_pkts;
	atomic_t in_reset;
};

struct qrtr_mhi_pkt {
	struct list_head node;
	struct sk_buff *skb;
	struct kref refcount;
	struct completion done;
};

static void qrtr_mhi_pkt_release(struct kref *ref)
{
	struct qrtr_mhi_pkt *pkt = container_of(ref, struct qrtr_mhi_pkt,
						refcount);
	struct sock *sk = pkt->skb->sk;

	consume_skb(pkt->skb);
	if (sk)
		sock_put(sk);
	kfree(pkt);
}

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
	struct qrtr_mhi_pkt *pkt;
	unsigned long flags;

	spin_lock_irqsave(&qdev->ul_lock, flags);
	pkt = list_first_entry(&qdev->ul_pkts, struct qrtr_mhi_pkt, node);
	list_del(&pkt->node);
	complete_all(&pkt->done);

	kref_put(&pkt->refcount, qrtr_mhi_pkt_release);
	spin_unlock_irqrestore(&qdev->ul_lock, flags);
}

/* fatal error */
static void qcom_mhi_qrtr_status_callback(struct mhi_device *mhi_dev,
					  enum MHI_CB mhi_cb)
{
	struct qrtr_mhi_dev *qdev = dev_get_drvdata(&mhi_dev->dev);
	struct qrtr_mhi_pkt *pkt;
	unsigned long flags;

	if (mhi_cb != MHI_CB_FATAL_ERROR)
		return;

	atomic_inc(&qdev->in_reset);
	spin_lock_irqsave(&qdev->ul_lock, flags);
	list_for_each_entry(pkt, &qdev->ul_pkts, node)
		complete_all(&pkt->done);
	spin_unlock_irqrestore(&qdev->ul_lock, flags);
}

/* from qrtr to mhi */
static int qcom_mhi_qrtr_send(struct qrtr_endpoint *ep, struct sk_buff *skb)
{
	struct qrtr_mhi_dev *qdev = container_of(ep, struct qrtr_mhi_dev, ep);
	struct qrtr_mhi_pkt *pkt;
	int rc;

	rc = skb_linearize(skb);
	if (rc) {
		kfree_skb(skb);
		return rc;
	}

	pkt = kzalloc(sizeof(*pkt), GFP_KERNEL);
	if (!pkt) {
		kfree_skb(skb);
		return -ENOMEM;
	}

	init_completion(&pkt->done);
	kref_init(&pkt->refcount);
	kref_get(&pkt->refcount);
	pkt->skb = skb;

	spin_lock_bh(&qdev->ul_lock);
	list_add_tail(&pkt->node, &qdev->ul_pkts);
	rc = mhi_queue_transfer(qdev->mhi_dev, DMA_TO_DEVICE, skb, skb->len,
				MHI_EOT);
	if (rc) {
		list_del(&pkt->node);
		kfree_skb(skb);
		kfree(pkt);
		spin_unlock_bh(&qdev->ul_lock);
		return rc;
	}
	spin_unlock_bh(&qdev->ul_lock);
	if (skb->sk)
		sock_hold(skb->sk);

	rc = wait_for_completion_interruptible_timeout(&pkt->done, HZ * 5);
	if (atomic_read(&qdev->in_reset))
		rc = -ECONNRESET;
	else if (rc == 0)
		rc = -ETIMEDOUT;
	else if (rc > 0)
		rc = 0;

	kref_put(&pkt->refcount, qrtr_mhi_pkt_release);
	return rc;
}

static int qcom_mhi_qrtr_probe(struct mhi_device *mhi_dev,
			       const struct mhi_device_id *id)
{
	struct qrtr_mhi_dev *qdev;
	u32 net_id;
	bool rt;
	int rc;

	qdev = devm_kzalloc(&mhi_dev->dev, sizeof(*qdev), GFP_KERNEL);
	if (!qdev)
		return -ENOMEM;

	qdev->mhi_dev = mhi_dev;
	qdev->dev = &mhi_dev->dev;
	qdev->ep.xmit = qcom_mhi_qrtr_send;
	atomic_set(&qdev->in_reset, 0);

	rc = of_property_read_u32(mhi_dev->dev.of_node, "qcom,net-id", &net_id);
	if (rc < 0)
		net_id = QRTR_EP_NET_ID_AUTO;

	rt = of_property_read_bool(mhi_dev->dev.of_node, "qcom,low-latency");

	INIT_LIST_HEAD(&qdev->ul_pkts);
	spin_lock_init(&qdev->ul_lock);

	rc = qrtr_endpoint_register(&qdev->ep, net_id, rt);
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
	.status_cb = qcom_mhi_qrtr_status_callback,
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
