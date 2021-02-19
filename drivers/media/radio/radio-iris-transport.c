// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (c) 2000-2001, 2011-2012, 2014-2015 The Linux Foundation.
 *  All rights reserved.
 *
 *  Copyright (C) 2000-2001  Qualcomm Incorporated
 *  Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
 *  Copyright (C) 2004-2006  Marcel Holtmann <marcel@holtmann.org>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/rpmsg.h>
#include <linux/workqueue.h>
#include <media/radio-iris.h>
#include <linux/uaccess.h>



struct radio_data {
	struct radio_hci_dev *hdev;
	struct tasklet_struct   rx_task;
	struct rpmsg_endpoint  *fm_channel;
	unsigned  char *data;
	int length;
};
struct radio_data hs;
DEFINE_MUTEX(fm_smd_enable);
static int fmsmd_set;
static bool chan_opened;
static int hcismd_fm_set_enable(const char *val, const struct kernel_param *kp);
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
	FMDBG("smd_recv event: is called\n");
	len = hs.length;
	buf = hs.data;
	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb) {
		FMDERR("Memory not allocated for the socket\n");
		return;
	}

	memcpy(skb_put(skb, len), buf, len);
	skb_orphan(skb);
	skb->dev = (struct net_device   *)hs.hdev;
	rc = radio_hci_recv_frame(skb);
	kfree(buf);
}

static int radio_hci_smd_send_frame(struct sk_buff *skb)
{
	int len = 0;
	FMDBG("hci_send_frame: is called\n");
	FM_INFO("skb %pK\n", skb);

	len = rpmsg_send(hs.fm_channel, skb->data, skb->len);
	if (len < skb->len) {
		FMDERR("Failed to write Data %d\n", len);
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
		FMDERR("Memory not allocated for the socket\n");
		kfree(worker);
		return;
	}

	FMDBG("FM INSERT DISABLE Rx Event\n");

	memcpy(skb_put(skb, len), buf, len);

	skb_orphan(skb);
	skb->dev = (struct net_device   *)hs.hdev;

	radio_hci_recv_frame(skb);
	kfree(worker);
}

static int radio_hci_smd_register_dev(struct radio_data *hsmd)
{
	struct radio_hci_dev *hdev;
	FMDBG("smd_register event: is called\n");
	if (hsmd == NULL)
		return -ENODEV;
	hdev = kmalloc(sizeof(struct radio_hci_dev), GFP_KERNEL);
	if (hdev == NULL)
		return -ENODEV;

	tasklet_init(&hsmd->rx_task, radio_hci_smd_recv_event,
				(unsigned long) hsmd);
	hdev->send = radio_hci_smd_send_frame;
	hdev->destruct = radio_hci_smd_destruct;
	hdev->close_smd = radio_hci_smd_exit;

	if (radio_hci_register_dev(hdev) < 0) {
		FMDERR("Can't register HCI device\n");
		hsmd->hdev = NULL;
		kfree(hdev);
		return -ENODEV;
	}
	hsmd->hdev = hdev;
	return 0;
}

static void radio_hci_smd_deregister(void)
{
	FM_INFO("smd_deregister: is called\n");
	radio_hci_unregister_dev();
	kfree(hs.hdev);
	hs.hdev = NULL;
	hs.fm_channel = 0;
	fmsmd_set = 0;
}


static int qcom_smd_fm_callback(struct rpmsg_device *rpdev,
			void *data, int len, void *priv, u32 addr)
{
	FM_INFO("fm_callback: is called\n");
	if (!len) {
		FMDERR("length received is NULL\n");
		return -EINVAL;
	}

	hs.data = kmemdup((unsigned char *)data, len, GFP_ATOMIC);
	if (!hs.data) {
		FMDERR("Memory not allocated\n");
		return -ENOMEM;
	}

	hs.length = len;
	tasklet_schedule(&hs.rx_task);
	return 0;
}

static int qcom_smd_fm_probe(struct rpmsg_device *rpdev)
{
	int ret;

	FM_INFO("fm_probe: is called\n");
	if (chan_opened) {
		FMDBG("Channel is already opened\n");
		return 0;
	}

	hs.fm_channel = rpdev->ept;
	ret = radio_hci_smd_register_dev(&hs);
	if (ret < 0) {
		FMDERR("Failed to register with rpmsg device\n");
		chan_opened = false;
		return ret;
	}
	FMDBG("probe succeeded\n");
	chan_opened = true;
	return ret;
}

static void qcom_smd_fm_remove(struct rpmsg_device *rpdev)
{
	FM_INFO("fm_remove: is called\n");
	reset_worker = kzalloc(sizeof(*reset_worker), GFP_ATOMIC);
	if (reset_worker) {
		INIT_WORK(reset_worker, send_disable_event);
		schedule_work(reset_worker);
	}
}


static const struct rpmsg_device_id qcom_smd_fm_match[] = {
	{ "APPS_FM" },
	{}
};


static struct rpmsg_driver qcom_smd_fm_driver = {
	.probe = qcom_smd_fm_probe,
	.remove = qcom_smd_fm_remove,
	.callback = qcom_smd_fm_callback,
	.id_table = qcom_smd_fm_match,
	.drv = {
		.name = "qcom_smd_fm",
	},
};

static int radio_hci_smd_init(void)
{
	int ret = 0;

	FM_INFO("smd_init : is called\n");
	if (chan_opened) {
		FMDBG("Channel is already opened\n");
		return 0;
	}

	ret = register_rpmsg_driver(&qcom_smd_fm_driver);
	if (ret < 0) {
		FMDERR("%s: Failed to register with rpmsg\n", __func__);
		return ret;
	}

	chan_opened = true;
	return ret;
}

static void radio_hci_smd_exit(void)
{
	if (!chan_opened) {
		FMDBG("Channel already closed\n");
		return;
	}

	/* this should be called with fm_smd_enable lock held */
	radio_hci_smd_deregister();
	unregister_rpmsg_driver(&qcom_smd_fm_driver);
	chan_opened = false;
}

static int hcismd_fm_set_enable(const char *val, const struct kernel_param *kp)
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

MODULE_ALIAS("rpmsg:APPS_FM");

MODULE_DESCRIPTION("FM SMD driver");
MODULE_LICENSE("GPL v2");
