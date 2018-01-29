/*
 *  QTI's FM Shared Memory Transport Driver
 *
 *  FM HCI_SMD ( FM HCI Shared Memory Driver) is QTI's Shared memory driver
 *  for the HCI protocol. This file is based on drivers/bluetooth/hci_vhci.c
 *
 *  Copyright (c) 2000-2001, 2011-2012, 2014-2015 The Linux Foundation.
 *  All rights reserved.
 *
 *  Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
 *  Copyright (C) 2004-2006  Marcel Holtmann <marcel@holtmann.org>
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
#include <linux/workqueue.h>
#include <soc/qcom/smd.h>
#include <media/radio-iris.h>
#include <linux/wakelock.h>
#include <linux/uaccess.h>

struct radio_data {
	struct radio_hci_dev *hdev;
	struct tasklet_struct   rx_task;
	struct smd_channel  *fm_channel;
};
struct radio_data hs;
DEFINE_MUTEX(fm_smd_enable);
static int fmsmd_set;
static bool chan_opened;
static int hcismd_fm_set_enable(const char *val, struct kernel_param *kp);
module_param_call(fmsmd_set, hcismd_fm_set_enable, NULL, &fmsmd_set, 0644);
static struct work_struct *reset_worker;
static void radio_hci_smd_deregister(void);
static void radio_hci_smd_exit(void);

static void radio_hci_smd_destruct(struct radio_hci_dev *hdev)
{
	radio_hci_unregister_dev();
}


static void radio_hci_smd_recv_event(unsigned long temp)
{
	int len;
	int rc;
	struct sk_buff *skb;
	unsigned  char *buf;
	struct radio_data *hsmd = &hs;
	FMDBG("");

	len = smd_read_avail(hsmd->fm_channel);

	while (len) {
		skb = alloc_skb(len, GFP_ATOMIC);
		if (!skb) {
			FMDERR("Memory not allocated for the socket");
			return;
		}

		buf = kmalloc(len, GFP_ATOMIC);
		if (!buf) {
			kfree_skb(skb);
			FMDERR("Error in allocating buffer memory");
			return;
		}

		rc = smd_read(hsmd->fm_channel, (void *)buf, len);

		memcpy(skb_put(skb, len), buf, len);

		skb_orphan(skb);
		skb->dev = (struct net_device   *)hs.hdev;

		rc = radio_hci_recv_frame(skb);

		kfree(buf);
		len = smd_read_avail(hsmd->fm_channel);
	}
}

static int radio_hci_smd_send_frame(struct sk_buff *skb)
{
	int len = 0;
	FMDBG("skb %pK", skb);

	len = smd_write(hs.fm_channel, skb->data, skb->len);
	if (len < skb->len) {
		FMDERR("Failed to write Data %d", len);
		kfree_skb(skb);
		return -ENODEV;
	}
	kfree_skb(skb);
	return 0;
}


static void send_disable_event(struct work_struct *worker)
{
	struct sk_buff *skb;
	unsigned char buf[6] = { 0x0f, 0x04, 0x01, 0x02, 0x4c, 0x00 };
	int len = sizeof(buf);

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb) {
		FMDERR("Memory not allocated for the socket");
		kfree(worker);
		return;
	}

	FMDERR("FM INSERT DISABLE Rx Event");

	memcpy(skb_put(skb, len), buf, len);

	skb_orphan(skb);
	skb->dev = (struct net_device   *)hs.hdev;

	radio_hci_recv_frame(skb);
	kfree(worker);
}

static void radio_hci_smd_notify_cmd(void *data, unsigned int event)
{
	struct radio_hci_dev *hdev = (struct radio_hci_dev *)data;
	FMDBG("data %p event %u", data, event);

	if (!hdev) {
		FMDERR("Frame for unknown HCI device (hdev=NULL)");
		return;
	}

	switch (event) {
	case SMD_EVENT_DATA:
		tasklet_schedule(&hs.rx_task);
		break;
	case SMD_EVENT_OPEN:
		break;
	case SMD_EVENT_CLOSE:
		reset_worker = kzalloc(sizeof(*reset_worker), GFP_ATOMIC);
		if (!reset_worker) {
			FMDERR("Out of memory");
			break;
		}
		INIT_WORK(reset_worker, send_disable_event);
		schedule_work(reset_worker);
		break;
	default:
		break;
	}
}

static int radio_hci_smd_register_dev(struct radio_data *hsmd)
{
	struct radio_hci_dev *hdev;
	int rc;
	FMDBG("hsmd: %pK", hsmd);

	if (hsmd == NULL)
		return -ENODEV;

	hdev = kmalloc(sizeof(struct radio_hci_dev), GFP_KERNEL);
	if (hdev == NULL)
		return -ENODEV;

	tasklet_init(&hsmd->rx_task, radio_hci_smd_recv_event,
		(unsigned long) hsmd);
	hdev->send  = radio_hci_smd_send_frame;
	hdev->destruct = radio_hci_smd_destruct;
	hdev->close_smd = radio_hci_smd_exit;

	/* Open the SMD Channel and device and register the callback function */
	rc = smd_named_open_on_edge("APPS_FM", SMD_APPS_WCNSS,
		&hsmd->fm_channel, hdev, radio_hci_smd_notify_cmd);

	if (rc < 0) {
		FMDERR("Cannot open the command channel");
		hsmd->hdev = NULL;
		kfree(hdev);
		return -ENODEV;
	}

	smd_disable_read_intr(hsmd->fm_channel);

	if (radio_hci_register_dev(hdev) < 0) {
		FMDERR("Can't register HCI device");
		smd_close(hsmd->fm_channel);
		hsmd->hdev = NULL;
		kfree(hdev);
		return -ENODEV;
	}

	hsmd->hdev = hdev;
	return 0;
}

static void radio_hci_smd_deregister(void)
{
	FMDBG("");

	radio_hci_unregister_dev();
	kfree(hs.hdev);
	hs.hdev = NULL;

	smd_close(hs.fm_channel);
	hs.fm_channel = 0;
	fmsmd_set = 0;
}

static int radio_hci_smd_init(void)
{
	int ret;

	if (chan_opened) {
		FMDBG("Channel is already opened");
		return 0;
	}

	/* this should be called with fm_smd_enable lock held */
	ret = radio_hci_smd_register_dev(&hs);
	if (ret < 0) {
		FMDERR("Failed to register smd device");
		chan_opened = false;
		return ret;
	}
	chan_opened = true;
	return ret;
}

static void radio_hci_smd_exit(void)
{
	if (!chan_opened) {
		FMDBG("Channel already closed");
		return;
	}

	/* this should be called with fm_smd_enable lock held */
	radio_hci_smd_deregister();
	chan_opened = false;
}

static int hcismd_fm_set_enable(const char *val, struct kernel_param *kp)
{
	int ret = 0;

	mutex_lock(&fm_smd_enable);
	ret = param_set_int(val, kp);
	if (ret)
		goto done;
	switch (fmsmd_set) {

	case 1:
		radio_hci_smd_init();
		break;
	case 0:
		radio_hci_smd_exit();
		break;
	default:
		ret = -EFAULT;
	}
done:
	mutex_unlock(&fm_smd_enable);
	return ret;
}
MODULE_DESCRIPTION("FM SMD driver");
MODULE_LICENSE("GPL v2");
