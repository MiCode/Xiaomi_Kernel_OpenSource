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
#include <linux/wakelock.h>

#define EVENT_CHANNEL		"APPS_RIVA_BT_CMD"
#define DATA_CHANNEL		"APPS_RIVA_BT_ACL"
#define RX_Q_MONITOR		(1)	/* 1 milli second */

struct hci_smd_data {
	struct hci_dev *hdev;

	struct smd_channel *event_channel;
	struct smd_channel *data_channel;
	struct wake_lock wake_lock_tx;
	struct wake_lock wake_lock_rx;
	struct timer_list rx_q_timer;
};
struct hci_smd_data hs;

/* Rx queue monitor timer function */
static int is_rx_q_empty(unsigned long arg)
{
	struct hci_dev *hdev = (struct hci_dev *) arg;
	struct sk_buff_head *list_ = &hdev->rx_q;
	struct sk_buff *list = ((struct sk_buff *)list_)->next;
	BT_DBG("%s Rx timer triggered", hdev->name);

	if (list == (struct sk_buff *)list_) {
		BT_DBG("%s RX queue empty", hdev->name);
		return 1;
	} else{
		BT_DBG("%s RX queue not empty", hdev->name);
		return 0;
	}
}

static void release_lock(void)
{
	struct hci_smd_data *hsmd = &hs;
	BT_DBG("Releasing Rx Lock");
	if (is_rx_q_empty((unsigned long)hsmd->hdev) &&
		wake_lock_active(&hs.wake_lock_rx))
			wake_unlock(&hs.wake_lock_rx);
}

/* Rx timer callback function */
static void schedule_timer(unsigned long arg)
{
	struct hci_dev *hdev = (struct hci_dev *) arg;
	struct hci_smd_data *hsmd = &hs;
	BT_DBG("%s Schedule Rx timer", hdev->name);

	if (is_rx_q_empty(arg) && wake_lock_active(&hs.wake_lock_rx)) {
		BT_DBG("%s RX queue empty", hdev->name);
		/*
		 * Since the queue is empty, its ideal
		 * to release the wake lock on Rx
		 */
		wake_unlock(&hs.wake_lock_rx);
	} else{
		BT_DBG("%s RX queue not empty", hdev->name);
		/*
		 * Restart the timer to monitor whether the Rx queue is
		 * empty for releasing the Rx wake lock
		 */
		mod_timer(&hsmd->rx_q_timer,
			jiffies + msecs_to_jiffies(RX_Q_MONITOR));
	}
}

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
	int len = 0;
	int rc = 0;
	struct sk_buff *skb = NULL;
	unsigned  char *buf = NULL;
	struct hci_smd_data *hsmd = &hs;
	wake_lock(&hs.wake_lock_rx);

	len = smd_read_avail(hsmd->data_channel);
	if (len <= 0) {
		BT_ERR("Nothing to read from SMD channel\n");
		goto out_data;
	}
	while (len > 0) {
		skb = bt_skb_alloc(len, GFP_ATOMIC);
		if (!skb) {
			BT_ERR("Error in allocating  socket buffer\n");
			goto out_data;
		}

		buf = kmalloc(len, GFP_ATOMIC);
		if (!buf)  {
			BT_ERR("Error in allocating  buffer\n");
			rc = -ENOMEM;
			goto out_data;
		}

		rc = smd_read_from_cb(hsmd->data_channel, (void *)buf, len);
		if (rc < len) {
			BT_ERR("Error in reading from the channel");
			goto out_data;
		}

		memcpy(skb_put(skb, len), buf, len);
		skb->dev = (void *)hsmd->hdev;
		bt_cb(skb)->pkt_type = HCI_ACLDATA_PKT;

		skb_orphan(skb);

		rc = hci_recv_frame(skb);
		if (rc < 0) {
			BT_ERR("Error in passing the packet to HCI Layer");
			goto out_data;
		}

		kfree(buf);
		buf = NULL;
		len = smd_read_avail(hsmd->data_channel);
		/*
		 * Start the timer to monitor whether the Rx queue is
		 * empty for releasing the Rx wake lock
		 */
		BT_DBG("Rx Timer is starting\n");
		mod_timer(&hsmd->rx_q_timer,
				jiffies + msecs_to_jiffies(RX_Q_MONITOR));
	}
out_data:
	release_lock();
	if (rc) {
		if (skb)
			kfree_skb(skb);
		kfree(buf);
	}
}

static void hci_smd_recv_event(unsigned long arg)
{
	int len = 0;
	int rc = 0;
	struct sk_buff *skb = NULL;
	unsigned  char *buf = NULL;
	struct hci_smd_data *hsmd = &hs;
	wake_lock(&hs.wake_lock_rx);

	len = smd_read_avail(hsmd->event_channel);
	if (len > HCI_MAX_FRAME_SIZE) {
		BT_ERR("Frame larger than the allowed size");
		goto out_event;
	} else if (len <= 0) {
		BT_ERR("Nothing to read from SMD channel\n");
		goto out_event;
	}

	while (len > 0) {
		skb = bt_skb_alloc(len, GFP_ATOMIC);
		if (!skb) {
			BT_ERR("Error in allocating  socket buffer\n");
			goto out_event;
		}
		buf = kmalloc(len, GFP_ATOMIC);
		if (!buf) {
			BT_ERR("Error in allocating  buffer\n");
			rc = -ENOMEM;
			goto out_event;
		}
		rc = smd_read_from_cb(hsmd->event_channel, (void *)buf, len);
		if (rc < len) {
			BT_ERR("Error in reading from the event channel");
			goto out_event;
		}

		memcpy(skb_put(skb, len), buf, len);
		skb->dev = (void *)hsmd->hdev;
		bt_cb(skb)->pkt_type = HCI_EVENT_PKT;

		skb_orphan(skb);

		rc = hci_recv_frame(skb);
		if (rc < 0) {
			BT_ERR("Error in passing the packet to HCI Layer");
			goto out_event;
		}

		kfree(buf);
		buf = NULL;
		len = smd_read_avail(hsmd->event_channel);
		/*
		 * Start the timer to monitor whether the Rx queue is
		 * empty for releasing the Rx wake lock
		 */
		BT_DBG("Rx Timer is starting\n");
		mod_timer(&hsmd->rx_q_timer,
				jiffies + msecs_to_jiffies(RX_Q_MONITOR));
	}
out_event:
	release_lock();
	if (rc) {
		if (skb)
			kfree_skb(skb);
		kfree(buf);
	}
}

static int hci_smd_send_frame(struct sk_buff *skb)
{
	int len;
	int avail;
	int ret = 0;
	wake_lock(&hs.wake_lock_tx);

	switch (bt_cb(skb)->pkt_type) {
	case HCI_COMMAND_PKT:
		avail = smd_write_avail(hs.event_channel);
		if (!avail) {
			BT_ERR("No space available for smd frame");
			ret =  -ENOSPC;
		}
		len = smd_write(hs.event_channel, skb->data, skb->len);
		if (len < skb->len) {
			BT_ERR("Failed to write Command %d", len);
			ret = -ENODEV;
		}
		break;
	case HCI_ACLDATA_PKT:
	case HCI_SCODATA_PKT:
		avail = smd_write_avail(hs.event_channel);
		if (!avail) {
			BT_ERR("No space available for smd frame");
			ret = -ENOSPC;
		}
		len = smd_write(hs.data_channel, skb->data, skb->len);
		if (len < skb->len) {
			BT_ERR("Failed to write Data %d", len);
			ret = -ENODEV;
		}
		break;
	default:
		BT_ERR("Uknown packet type\n");
		ret = -ENODEV;
		break;
	}

	wake_unlock(&hs.wake_lock_tx);
	return ret;
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

	wake_lock_init(&hs.wake_lock_rx, WAKE_LOCK_SUSPEND, "msm_smd_Rx");
	wake_lock_init(&hs.wake_lock_tx, WAKE_LOCK_SUSPEND, "msm_smd_Tx");
	/*
	 * Setup the timer to monitor whether the Rx queue is empty,
	 * to control the wake lock release
	 */
	setup_timer(&hsmd->rx_q_timer, schedule_timer,
			(unsigned long) hsmd->hdev);

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
	wake_lock_destroy(&hs.wake_lock_rx);
	wake_lock_destroy(&hs.wake_lock_tx);

	/*Destroy the timer used to monitor the Rx queue for emptiness */
	del_timer_sync(&hs.rx_q_timer);
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
