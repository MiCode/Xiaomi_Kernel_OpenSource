// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved. */

#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/usb.h>

#include "qrtr.h"

#define QRTR_VENDOR_ID 0x05c6
#define MAX_DATA_SIZE 16384

enum qrtr_usb_rx_state {
	QRTR_USB_RX_SUSPEND = 0,
	QRTR_USB_RX_RUN,
	QRTR_USB_RX_STOP
};

struct qrtr_usb_dev {
	struct qrtr_endpoint ep;
	struct usb_device *udev;
	struct usb_interface *iface;
	struct usb_anchor submitted;
	struct completion in_compl;
	struct urb *in_urb;
	struct task_struct *rx_thread;
	enum qrtr_usb_rx_state thread_state;
	wait_queue_head_t qrtr_wq;
	unsigned int in_pipe;
	unsigned int out_pipe;
};

static void qcom_usb_qrtr_txn_cb(struct urb *urb)
{
	struct completion *compl = urb->context;

	complete(compl);
}

/* from usb to qrtr */
static int qcom_usb_qrtr_rx_thread_fn(void *data)
{
	struct qrtr_usb_dev *qdev = data;
	struct urb *in_urb = qdev->in_urb;
	void *buf;
	int rc = 0;

	buf = kmalloc(MAX_DATA_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	usb_anchor_urb(in_urb, &qdev->submitted);

	usb_fill_bulk_urb(in_urb, qdev->udev, qdev->in_pipe,
			  buf, MAX_DATA_SIZE, qcom_usb_qrtr_txn_cb,
			  &qdev->in_compl);

	while (!kthread_should_stop()) {
		if (qdev->thread_state == QRTR_USB_RX_SUSPEND ||
		    qdev->thread_state == QRTR_USB_RX_STOP) {
			dev_dbg(&qdev->udev->dev,
				"pausing or stopping thread, state=%d\n",
				qdev->thread_state);
			wait_event_interruptible(qdev->qrtr_wq,
						 qdev->thread_state ==
						 QRTR_USB_RX_RUN ||
						 kthread_should_stop());
			continue;
		}

		rc = usb_autopm_get_interface(qdev->iface);
		if (rc) {
			dev_err(&qdev->udev->dev,
				"failed to get autopm, rc=%d\n", rc);
			continue;
		}

		reinit_completion(&qdev->in_compl);

		rc = usb_submit_urb(in_urb, GFP_KERNEL);
		if (rc) {
			usb_autopm_put_interface(qdev->iface);
			dev_dbg(&qdev->udev->dev,
				"could not submit in urb, rc=%d\n", rc);
			/* Give up if the device is disconnected */
			if (rc == -ENODEV)
				break;
			continue;
		}

		wait_for_completion(&qdev->in_compl);
		if (in_urb->status) {
			usb_autopm_put_interface(qdev->iface);
			dev_dbg(&qdev->udev->dev, "URB status %d",
				in_urb->status);
			continue;
		}

		usb_autopm_put_interface(qdev->iface);

		dev_dbg(&qdev->udev->dev, "received message with len=%d\n",
			in_urb->actual_length);

		rc = qrtr_endpoint_post(&qdev->ep, buf, in_urb->actual_length);
		if (rc == -EINVAL)
			dev_err(&qdev->udev->dev,
				"invalid ipcrouter packet\n");
	}

	kfree(buf);
	wait_event_interruptible(qdev->qrtr_wq, kthread_should_stop());
	dev_dbg(&qdev->udev->dev, "leaving rx_thread\n");

	return rc;
}

/* from qrtr to usb */
static int qcom_usb_qrtr_send(struct qrtr_endpoint *ep, struct sk_buff *skb)
{
	struct qrtr_usb_dev *qdev = container_of(ep, struct qrtr_usb_dev, ep);
	struct urb *out_urb = NULL;
	struct completion compl;
	int rc;

	rc = skb_linearize(skb);
	if (rc)
		goto exit_free_skb;

	out_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!out_urb) {
		rc = -ENOMEM;
		goto exit_free_skb;
	}

	rc = usb_autopm_get_interface(qdev->iface);
	if (rc)
		goto exit_free_urb;

	init_completion(&compl);
	usb_fill_bulk_urb(out_urb, qdev->udev, qdev->out_pipe,
			  skb->data, skb->len, qcom_usb_qrtr_txn_cb, &compl);

	usb_anchor_urb(out_urb, &qdev->submitted);
	rc = usb_submit_urb(out_urb, GFP_KERNEL);
	if (rc) {
		dev_err(&qdev->udev->dev, "could not submit out urb\n");
		usb_unanchor_urb(out_urb);
		goto exit_autopm_put_intf;
	}

	wait_for_completion(&compl);
	rc = out_urb->status;

	dev_dbg(&qdev->udev->dev, "sent message with len=%d, rc=%d\n",
		skb->len, rc);

exit_autopm_put_intf:
	usb_autopm_put_interface(qdev->iface);
exit_free_urb:
	usb_free_urb(out_urb);
exit_free_skb:
	if (rc)
		kfree_skb(skb);
	else
		consume_skb(skb);

	return rc;
}

static int qcom_usb_qrtr_probe(struct usb_interface *interface,
			       const struct usb_device_id *id)
{
	struct qrtr_usb_dev *qdev;
	struct usb_device *udev;
	struct usb_host_interface *intf_desc;
	struct usb_endpoint_descriptor *ep_desc;
	int rc, i;

	udev = usb_get_dev(interface_to_usbdev(interface));
	if (!udev)
		return -ENODEV;

	qdev = devm_kzalloc(&udev->dev, sizeof(*qdev), GFP_KERNEL);
	if (!qdev)
		return -ENOMEM;

	qdev->udev = udev;
	qdev->iface = interface;
	qdev->ep.xmit = qcom_usb_qrtr_send;

	intf_desc = interface->cur_altsetting;
	for (i = 0; i < intf_desc->desc.bNumEndpoints; i++) {
		ep_desc = &intf_desc->endpoint[i].desc;
		if (!qdev->in_pipe && usb_endpoint_is_bulk_in(ep_desc))
			qdev->in_pipe =
			usb_rcvbulkpipe(qdev->udev, ep_desc->bEndpointAddress);
		if (!qdev->out_pipe && usb_endpoint_is_bulk_out(ep_desc))
			qdev->out_pipe =
			usb_sndbulkpipe(qdev->udev, ep_desc->bEndpointAddress);
	}

	if (!qdev->in_pipe || !qdev->out_pipe) {
		dev_err(&qdev->udev->dev, "could not find endpoints\n");
		return -ENODEV;
	}

	qdev->in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!qdev->in_urb) {
		dev_err(&qdev->udev->dev, "could not allocate in urb\n");
		return -ENOMEM;
	}

	init_usb_anchor(&qdev->submitted);

	rc = qrtr_endpoint_register(&qdev->ep, QRTR_EP_NID_AUTO, false);
	if (rc)
		return rc;

	usb_set_intfdata(interface, qdev);

	init_waitqueue_head(&qdev->qrtr_wq);
	init_completion(&qdev->in_compl);
	qdev->thread_state = QRTR_USB_RX_RUN;
	qdev->rx_thread = kthread_run(qcom_usb_qrtr_rx_thread_fn, qdev,
				      "qrtr-usb-rx");
	if (IS_ERR(qdev->rx_thread)) {
		dev_err(&qdev->udev->dev, "could not create rx_thread\n");
		qrtr_endpoint_unregister(&qdev->ep);
		return PTR_ERR(qdev->rx_thread);
	}

	dev_dbg(&qdev->udev->dev, "QTI USB QRTR driver probed\n");

	return 0;
}

static int qcom_usb_qrtr_suspend(struct usb_interface *intf,
				 pm_message_t message)
{
	struct qrtr_usb_dev *qdev = usb_get_intfdata(intf);

	/* Suspend the thread */
	qdev->thread_state = QRTR_USB_RX_SUSPEND;
	usb_kill_anchored_urbs(&qdev->submitted);

	return 0;
}

static int qcom_usb_qrtr_resume(struct usb_interface *intf)
{
	struct qrtr_usb_dev *qdev = usb_get_intfdata(intf);

	qdev->thread_state = QRTR_USB_RX_RUN;
	wake_up(&qdev->qrtr_wq);

	return 0;
}

static int qcom_usb_qrtr_reset_resume(struct usb_interface *intf)
{
	struct qrtr_usb_dev *qdev = usb_get_intfdata(intf);
	int rc = 0;

	qrtr_endpoint_unregister(&qdev->ep);
	rc = qrtr_endpoint_register(&qdev->ep, QRTR_EP_NID_AUTO, false);
	if (rc)
		return rc;

	qdev->thread_state = QRTR_USB_RX_RUN;
	wake_up(&qdev->qrtr_wq);

	return rc;
}

static void qcom_usb_qrtr_disconnect(struct usb_interface *interface)
{
	struct qrtr_usb_dev *qdev = usb_get_intfdata(interface);

	qdev->thread_state = QRTR_USB_RX_STOP;
	usb_kill_anchored_urbs(&qdev->submitted);
	kthread_stop(qdev->rx_thread);

	usb_free_urb(qdev->in_urb);
	qrtr_endpoint_unregister(&qdev->ep);
	usb_set_intfdata(interface, NULL);
	usb_put_dev(qdev->udev);
}

static const struct usb_device_id qcom_usb_qrtr_ids[] = {
	{ USB_DEVICE_INTERFACE_NUMBER(QRTR_VENDOR_ID, 0x90ef, 3) },
	{ USB_DEVICE_INTERFACE_NUMBER(QRTR_VENDOR_ID, 0x90f0, 3) },
	{ USB_DEVICE_INTERFACE_NUMBER(QRTR_VENDOR_ID, 0x90f3, 2) },
	{ } /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, qcom_usb_qrtr_ids);

static struct usb_driver qcom_usb_qrtr_driver = {
	.name = "qcom_usb_qrtr",
	.probe = qcom_usb_qrtr_probe,
	.disconnect = qcom_usb_qrtr_disconnect,
	.suspend = qcom_usb_qrtr_suspend,
	.resume = qcom_usb_qrtr_resume,
	.reset_resume = qcom_usb_qrtr_reset_resume,
	.id_table = qcom_usb_qrtr_ids,
	.supports_autosuspend = 1,
};

module_usb_driver(qcom_usb_qrtr_driver);

MODULE_DESCRIPTION("QTI IPC-Router USB interface driver");
MODULE_LICENSE("GPL v2");
