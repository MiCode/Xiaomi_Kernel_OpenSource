/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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

#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/usb/ipc_bridge.h>

#include "qrtr.h"

#define IPC_DRIVER_NAME "ipc_bridge"

struct qrtr_usb_dev_ep {
	struct qrtr_endpoint ep;
	struct platform_device *pdev;
	struct task_struct *rx_thread;
};

static struct qrtr_usb_dev_ep *qep;

/* from qrtr to usb */
static int qrtr_usb_dev_send(struct qrtr_endpoint *ep, struct sk_buff *skb)
{
	struct qrtr_usb_dev_ep *qep =
		container_of(ep, struct qrtr_usb_dev_ep, ep);
	struct ipc_bridge_platform_data *ipc_bridge;
	int rc;

	ipc_bridge = qep->pdev->dev.platform_data;

	rc = skb_linearize(skb);
	if (rc)
		goto exit_free_skb;

	rc = ipc_bridge->write(qep->pdev, skb->data, skb->len);
	if (rc < 0) {
		dev_err(&qep->pdev->dev, "error writing data %d\n", rc);
	} else if (rc != skb->len) {
		dev_err(&qep->pdev->dev, "wrote partial data, len=%d\n", rc);
		rc = -EIO;
	} else {
		dev_dbg(&qep->pdev->dev, "wrote message with len=%d\n", rc);
		rc = 0;
	}

exit_free_skb:
	if (rc)
		kfree_skb(skb);
	else
		consume_skb(skb);

	return rc;
}

/* from usb to qrtr */
static int qrtr_usb_dev_rx_thread_fn(void *data)
{
	struct qrtr_usb_dev_ep *qep = data;
	struct ipc_bridge_platform_data *ipc_bridge;
	void *buf;
	int bytes_read;
	int rc = 0;

	ipc_bridge = qep->pdev->dev.platform_data;

	buf = kmalloc(ipc_bridge->max_read_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	while (!kthread_should_stop()) {
		bytes_read = ipc_bridge->read(qep->pdev, buf,
					      ipc_bridge->max_read_size);
		if (bytes_read < 0) {
			dev_err(&qep->pdev->dev,
				"error in ipc read operation %d\n", bytes_read);
			continue;
		}

		dev_dbg(&qep->pdev->dev, "received message with len=%d\n",
			bytes_read);

		rc = qrtr_endpoint_post(&qep->ep, buf, bytes_read);
		if (rc == -EINVAL)
			dev_err(&qep->pdev->dev,
				"invalid ipcrouter packet\n");
	}

	kfree(buf);
	dev_dbg(&qep->pdev->dev, "leaving rx_thread\n");

	return rc;
}

static int qrtr_usb_dev_probe(struct platform_device *pdev)
{
	struct ipc_bridge_platform_data *ipc_bridge;
	int rc;

	ipc_bridge = pdev->dev.platform_data;
	if (!ipc_bridge || !ipc_bridge->open || !ipc_bridge->read ||
	    !ipc_bridge->write || !ipc_bridge->close) {
		dev_err(&pdev->dev,
			"ipc_bridge or ipc_bridge->operations is NULL\n");
		rc = -EINVAL;
		goto exit;
	}

	rc = ipc_bridge->open(pdev);
	if (rc) {
		dev_err(&pdev->dev, "channel open failed for %s.%s\n",
			pdev->name, pdev->id);
		goto exit;
	}

	qep = devm_kzalloc(&pdev->dev, sizeof(*qep), GFP_KERNEL);
	if (!qep) {
		rc = -ENOMEM;
		goto exit_close_bridge;
	}

	qep->pdev = pdev;
	qep->ep.xmit = qrtr_usb_dev_send;

	rc = qrtr_endpoint_register(&qep->ep, QRTR_EP_NID_AUTO, false);
	if (rc) {
		dev_err(&pdev->dev, "failed to register qrtr endpoint\n");
		goto exit_close_bridge;
	}

	qep->rx_thread = kthread_run(qrtr_usb_dev_rx_thread_fn, qep,
				     "qrtr-usb-dev-rx");
	if (IS_ERR(qep->rx_thread)) {
		dev_err(&qep->pdev->dev, "could not create rx_thread\n");
		rc = PTR_ERR(qep->rx_thread);
		goto exit_qrtr_unregister;
	}

	dev_dbg(&qep->pdev->dev, "QTI USB-dev QRTR driver probed\n");

	return 0;

exit_qrtr_unregister:
	qrtr_endpoint_unregister(&qep->ep);
exit_close_bridge:
	ipc_bridge->close(pdev);
exit:
	return rc;
}

static int qrtr_usb_dev_remove(struct platform_device *pdev)
{
	struct ipc_bridge_platform_data *ipc_bridge;

	dev_dbg(&pdev->dev, "removing the platform dev\n");

	kthread_stop(qep->rx_thread);
	qrtr_endpoint_unregister(&qep->ep);

	ipc_bridge = pdev->dev.platform_data;
	if (ipc_bridge && ipc_bridge->close)
		ipc_bridge->close(pdev);

	return 0;
}

static struct platform_driver qrtr_usb_dev_driver = {
	.probe = qrtr_usb_dev_probe,
	.remove = qrtr_usb_dev_remove,
	.driver = {
		.name = IPC_DRIVER_NAME,
		.owner = THIS_MODULE,
	 },
};
module_platform_driver(qrtr_usb_dev_driver);

MODULE_DESCRIPTION("QTI IPC-Router USB device interface driver");
MODULE_LICENSE("GPL v2");
