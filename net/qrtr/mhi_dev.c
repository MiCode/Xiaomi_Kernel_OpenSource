/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/of.h>
#include <linux/msm_mhi_dev.h>
#include "qrtr.h"

#define QRTR_MAX_PKT_SIZE SZ_32K

/* MHI DEV Enums are defined from Host perspective */
#define QRTR_MHI_DEV_OUT MHI_CLIENT_IPCR_IN
#define QRTR_MHI_DEV_IN MHI_CLIENT_IPCR_OUT

/**
 * struct qrtr_mhi_dev_ep - qrtr mhi device endpoint
 * @ep: endpoint
 * @dev: device from platform bus
 * @out: channel handle from mhi dev
 * @out_tre: complete when channel is ready to send
 * @out_lock: hold when resetting completion variable
 * @in: channel handle from mhi dev
 * @buf_in: buffer to hold incoming data
 * @net_id: subnet id used by qrtr core
 * @rt: realtime option used by qrtr core
 */
struct qrtr_mhi_dev_ep {
	struct qrtr_endpoint ep;
	struct device *dev;
	struct mhi_dev_client *out;
	struct completion out_tre;
	struct mutex out_lock;		/* for out critical sections */
	struct mhi_dev_client *in;
	void *buf_in;

	u32 net_id;
	bool rt;
};

static struct qrtr_mhi_dev_ep *qrtr_mhi_device_endpoint;

static int qrtr_mhi_dev_send(struct qrtr_endpoint *ep, struct sk_buff *skb)
{
	struct qrtr_mhi_dev_ep *qep;
	struct mhi_req req = { 0 };
	int rc;

	qep = container_of(ep, struct qrtr_mhi_dev_ep, ep);
	rc = skb_linearize(skb);
	if (rc) {
		kfree_skb(skb);
		return rc;
	}

	req.chan = QRTR_MHI_DEV_OUT;
	req.client = qep->out;
	req.mode = DMA_SYNC;
	req.buf = skb->data;
	req.len = skb->len;

	do {
		wait_for_completion(&qep->out_tre);

		mutex_lock(&qep->out_lock);
		rc = mhi_dev_write_channel(&req);
		if (rc == 0)
			reinit_completion(&qep->out_tre);
		mutex_unlock(&qep->out_lock);
	} while (!rc);

	if (rc != skb->len) {
		dev_err(qep->dev, "send failed rc:%d len:%d\n", rc, skb->len);
		kfree_skb(skb);
		return rc;
	}

	consume_skb(skb);
	return 0;
}

static void qrtr_mhi_dev_read(struct qrtr_mhi_dev_ep *qep)
{
	struct mhi_req req = { 0 };
	int rc;
	int bytes_read;

	req.chan = QRTR_MHI_DEV_IN;
	req.client = qep->in;
	req.mode = DMA_SYNC;
	req.buf = qep->buf_in;
	req.len = QRTR_MAX_PKT_SIZE;

	do {
		bytes_read = mhi_dev_read_channel(&req);
		if (bytes_read < 0) {
			dev_err(qep->dev, "failed to read channel %d\n",
				bytes_read);
			return;
		}
		if (bytes_read == 0)
			return;

		rc = qrtr_endpoint_post(&qep->ep, req.buf, req.transfer_len);
		if (rc == -EINVAL)
			dev_err(qep->dev, "invalid ipcrouter packet\n");
	} while (bytes_read > 0);
}

static void qrtr_mhi_dev_event_cb(struct mhi_dev_client_cb_reason *reason)
{
	struct qrtr_mhi_dev_ep *qep;

	qep = qrtr_mhi_device_endpoint;
	if (!qep)
		return;

	if (reason->reason == MHI_DEV_TRE_AVAILABLE) {
		pr_debug("TRE available event for chan %d\n", reason->ch_id);
		if (reason->ch_id == QRTR_MHI_DEV_IN) {
			qrtr_mhi_dev_read(qep);
		} else {
			mutex_lock(&qep->out_lock);
			complete_all(&qep->out_tre);
			mutex_unlock(&qep->out_lock);
		}
	}
}

static int qrtr_mhi_dev_open_channels(struct qrtr_mhi_dev_ep *qep)
{
	int rc;

	/* write channel */
	rc = mhi_dev_open_channel(QRTR_MHI_DEV_IN, &qep->in,
				  qrtr_mhi_dev_event_cb);
	if (rc < 0)
		return rc;

	/* read channel */
	rc = mhi_dev_open_channel(QRTR_MHI_DEV_OUT, &qep->out,
				  qrtr_mhi_dev_event_cb);
	if (rc < 0) {
		mhi_dev_close_channel(qep->in);
		return rc;
	}
	return 0;
}

static void qrtr_mhi_dev_close_channels(struct qrtr_mhi_dev_ep *qep)
{

	mhi_dev_close_channel(qep->in);
	mhi_dev_close_channel(qep->out);
}

static void qrtr_mhi_dev_state_cb(struct mhi_dev_client_cb_data *cb_data)
{
	struct qrtr_mhi_dev_ep *qep;
	int rc;

	if (!cb_data || !cb_data->user_data)
		return;
	qep = cb_data->user_data;

	switch (cb_data->ctrl_info) {
	case MHI_STATE_CONNECTED:
		rc = qrtr_mhi_dev_open_channels(qep);
		if (rc) {
			dev_err(qep->dev, "open failed %d", rc);
			return;
		}

		rc = qrtr_endpoint_register(&qep->ep, qep->net_id, qep->rt);
		if (rc) {
			dev_err(qep->dev, "register failed %d", rc);
			qrtr_mhi_dev_close_channels(qep);
		}
		break;
	case MHI_STATE_DISCONNECTED:
		qrtr_endpoint_unregister(&qep->ep);
		qrtr_mhi_dev_close_channels(qep);
		break;
	default:
		break;
	}
}

static int qrtr_mhi_dev_probe(struct platform_device *pdev)
{
	struct qrtr_mhi_dev_ep *qep;
	struct device_node *node;
	int rc;
	struct mhi_dev_client_cb_data cb_data;

	qep = devm_kzalloc(&pdev->dev, sizeof(*qep), GFP_KERNEL);
	if (!qep)
		return -ENOMEM;
	qep->dev = &pdev->dev;

	node = pdev->dev.of_node;
	rc = of_property_read_u32(node, "qcom,net-id", &qep->net_id);
	if (rc < 0)
		qep->net_id = QRTR_EP_NET_ID_AUTO;
	qep->rt = of_property_read_bool(node, "qcom,low-latency");

	qep->buf_in = devm_kzalloc(&pdev->dev, QRTR_MAX_PKT_SIZE, GFP_KERNEL);
	if (!qep->buf_in)
		return -ENOMEM;

	qrtr_mhi_device_endpoint = qep;

	mutex_init(&qep->out_lock);
	init_completion(&qep->out_tre);
	qep->ep.xmit = qrtr_mhi_dev_send;
	/* HOST init TX first followed by RX, so register for endpoint TX
	 * which makes both channel ready by checking one channel state.
	 */
	rc = mhi_register_state_cb(qrtr_mhi_dev_state_cb, qep,
				   QRTR_MHI_DEV_OUT);
	if (rc == -EEXIST) {
		/**
		 * MHI stack will return -EEXIST if mhi channel is already
		 * opend by the host and will not invoke reqistered callback.
		 * But future state change notification will inform through
		 * registered callback.
		 */
		complete_all(&qep->out_tre);
		cb_data.user_data = (void *)qep;
		cb_data.channel = QRTR_MHI_DEV_OUT;
		cb_data.ctrl_info = MHI_STATE_CONNECTED;
		qrtr_mhi_dev_state_cb(&cb_data);
	} else if (rc) {
		return rc;
	}

	return 0;
}

static const struct of_device_id qrtr_mhi_dev_match_table[] = {
	{ .compatible = "qcom,qrtr-mhi-dev"},
	{},
};

static struct platform_driver qrtr_mhi_dev_driver = {
	.probe = qrtr_mhi_dev_probe,
	.driver = {
		.name = "qrtr_mhi_dev",
		.of_match_table = qrtr_mhi_dev_match_table,
	},
};
module_platform_driver(qrtr_mhi_dev_driver);

MODULE_DESCRIPTION("QTI IPC-Router MHI device interface driver");
MODULE_LICENSE("GPL v2");
