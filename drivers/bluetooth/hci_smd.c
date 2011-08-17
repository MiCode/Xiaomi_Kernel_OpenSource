/*
 *  HCI_SMD (HCI Shared Memory Driver) is Qualcomm's Shared memory driver
 *  for the BT HCI protocol.
 *
 *  Copyright (c) 2000-2001, 2011 Code Aurora Forum. All rights reserved.
 *  Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
 *  Copyright (C) 2004-2006  Marcel Holtmann <marcel@holtmann.org>
 *
 *  This file is based on drivers/bluetooth/hci_vhci.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <net/bluetooth/hci.h>
#include <mach/msm_smd.h>

#define EVENT_CHANNEL "APPS_RIVA_BT_CMD"
#define DATA_CHANNEL "APPS_RIVA_BT_ACL"

struct hci_smd_data {
	struct hci_dev *hdev;

	struct smd_channel *event_channel;
	struct smd_channel *data_channel;
};
struct hci_smd_data hs;

static int hci_smd_open(struct hci_dev *hdev)
{
	set_bit(HCI_RUNNING, &hdev->flags);
	return 0;
}


static int hci_smd_close(struct hci_dev *hdev)
{
	if (!test_and_clear_bit(HCI_RUNNING, &hdev->flags))
		return 0;
	else
		return -EPERM;
}


static void hci_smd_destruct(struct hci_dev *hdev)
{
	kfree(hdev->driver_data);
}

static void hci_smd_recv_data(unsigned long arg)
{
	int len;
	int rc;
	struct sk_buff *skb;
	unsigned  char *buf;
	struct hci_smd_data *hsmd = &hs;

	len = smd_read_avail(hsmd->data_channel);

	while (len > 0) {
		skb = bt_skb_alloc(len, GFP_KERNEL);
		if (!skb) {
			BT_ERR("Error in allocating  socket buffer\n");
			return;
		}

		buf = kmalloc(len, GFP_KERNEL);
		if (!buf)  {
			BT_ERR("Error in allocating  buffer\n");
			kfree_skb(skb);
			return;
		}

		rc = smd_read_from_cb(hsmd->data_channel, (void *)buf, len);
		if (rc < len) {
			BT_ERR("Error in reading from the channel");
			return;
		}

		memcpy(skb_put(skb, len), buf, len);
		skb->dev = (void *)hsmd->hdev;
		bt_cb(skb)->pkt_type = HCI_ACLDATA_PKT;

		skb_orphan(skb);

		rc = hci_recv_frame(skb);
		if (rc < 0) {
			BT_ERR("Error in passing the packet to HCI Layer");
			return;
		}

		kfree(buf);
		len = smd_read_avail(hsmd->data_channel);
	}
}

static void hci_smd_recv_event(unsigned long arg)
{
	int len;
	int rc;
	struct sk_buff *skb;
	unsigned  char *buf;
	struct hci_smd_data *hsmd = &hs;

	len = smd_read_avail(hsmd->event_channel);
	if (len > HCI_MAX_FRAME_SIZE) {
		BT_ERR("Frame larger than the allowed size");
		return;
	}

	while (len > 0) {
		skb = bt_skb_alloc(len, GFP_KERNEL);
		if (!skb)
			return;

		buf = kmalloc(len, GFP_KERNEL);
		if (!buf) {
			kfree_skb(skb);
			return;
		}

		rc = smd_read_from_cb(hsmd->event_channel, (void *)buf, len);

		memcpy(skb_put(skb, len), buf, len);
		skb->dev = (void *)hsmd->hdev;
		bt_cb(skb)->pkt_type = HCI_EVENT_PKT;

		skb_orphan(skb);

		rc = hci_recv_frame(skb);
		if (rc < 0) {
			BT_ERR("Error in passing the packet to HCI Layer");
			return;
		}

		kfree(buf);
		len = smd_read_avail(hsmd->event_channel);
	}
}

static int hci_smd_send_frame(struct sk_buff *skb)
{
	int len;

	switch (bt_cb(skb)->pkt_type) {
	case HCI_COMMAND_PKT:
		len = smd_write(hs.event_channel, skb->data, skb->len);
		if (len < skb->len) {
			BT_ERR("Failed to write Command %d", len);
			return -ENODEV;
		}
		break;
	case HCI_ACLDATA_PKT:
	case HCI_SCODATA_PKT:
		len = smd_write(hs.data_channel, skb->data, skb->len);
		if (len < skb->len) {
			BT_ERR("Failed to write Data %d", len);
			return -ENODEV;
		}
		break;
	default:
		BT_ERR("Uknown packet type\n");
		return -ENODEV;
		break;
	}
	return 0;
}


static void hci_smd_notify_event(void *data, unsigned int event)
{
	struct hci_dev *hdev = hs.hdev;

	if (!hdev) {
		BT_ERR("Frame for unknown HCI device (hdev=NULL)");
		return;
	}

	switch (event) {
	case SMD_EVENT_DATA:
		hci_smd_recv_event(event);
		break;
	case SMD_EVENT_OPEN:
		hci_smd_open(hdev);
		break;
	case SMD_EVENT_CLOSE:
		hci_smd_close(hdev);
		break;
	default:
		break;
	}
}

static void hci_smd_notify_data(void *data, unsigned int event)
{
	struct hci_dev *hdev = hs.hdev;
	if (!hdev) {
		BT_ERR("HCI device (hdev=NULL)");
		return;
	}

	switch (event) {
	case SMD_EVENT_DATA:
		hci_smd_recv_data(event);
		break;
	case SMD_EVENT_OPEN:
		hci_smd_open(hdev);
		break;
	case SMD_EVENT_CLOSE:
		hci_smd_close(hdev);
		break;
	default:
		break;
	}

}

static int hci_smd_register_dev(struct hci_smd_data *hsmd)
{
	struct hci_dev *hdev;
	int rc;

	/* Initialize and register HCI device */
	hdev = hci_alloc_dev();
	if (!hdev) {
		BT_ERR("Can't allocate HCI device");
		return -ENOMEM;
	}

	hsmd->hdev = hdev;
	hdev->bus = HCI_SMD;
	hdev->driver_data = hsmd;
	hdev->open  = hci_smd_open;
	hdev->close = hci_smd_close;
	hdev->send  = hci_smd_send_frame;
	hdev->destruct = hci_smd_destruct;
	hdev->owner = THIS_MODULE;

	/* Open the SMD Channel and device and register the callback function */
	rc = smd_named_open_on_edge(EVENT_CHANNEL, SMD_APPS_WCNSS,
			&hsmd->event_channel, hdev, hci_smd_notify_event);
	if (rc < 0) {
		BT_ERR("Cannot open the command channel");
		hci_free_dev(hdev);
		return -ENODEV;
	}

	rc = smd_named_open_on_edge(DATA_CHANNEL, SMD_APPS_WCNSS,
			&hsmd->data_channel, hdev, hci_smd_notify_data);
	if (rc < 0) {
		BT_ERR("Failed to open the Data channel\n");
		hci_free_dev(hdev);
		return -ENODEV;
	}

	/* Disable the read interrupts on the channel */
	smd_disable_read_intr(hsmd->event_channel);
	smd_disable_read_intr(hsmd->data_channel);

	if (hci_register_dev(hdev) < 0) {
		BT_ERR("Can't register HCI device");
		hci_free_dev(hdev);
		return -ENODEV;
	}

	return 0;
}

static void hci_smd_deregister(void)
{
	smd_close(hs.event_channel);
	hs.event_channel = 0;
	smd_close(hs.data_channel);
	hs.data_channel = 0;
}

static int hci_smd_init(void)
{
	return hci_smd_register_dev(&hs);
}
module_init(hci_smd_init);

static void __exit hci_smd_exit(void)
{
	hci_smd_deregister();
}
module_exit(hci_smd_exit);

MODULE_AUTHOR("Ankur Nandwani <ankurn@codeaurora.org>");
MODULE_DESCRIPTION("Bluetooth SMD driver");
MODULE_LICENSE("GPL v2");
