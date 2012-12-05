/*
 *  HCI_SMD (HCI Shared Memory Driver) is Qualcomm's Shared memory driver
 *  for the BT HCI protocol.
 *
 *  Copyright (c) 2000-2001, 2011-2012 The Linux Foundation. All rights reserved.
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
#include <linux/semaphore.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <net/bluetooth/hci.h>
#include <mach/msm_smd.h>

#define EVENT_CHANNEL		"APPS_RIVA_BT_CMD"
#define DATA_CHANNEL		"APPS_RIVA_BT_ACL"
/* release wakelock in 500ms, not immediately, because higher layers
 * don't always take wakelocks when they should
 * This is derived from the implementation for UART transport
 */

#define RX_Q_MONITOR		(500)	/* 500 milli second */
#define HCI_REGISTER_SET	0

/* SSR state machine to take care of back to back SSR requests
 * and handling the incomming BT on/off,Airplane mode toggling and
 * also spuriour SMD open notification while one SSr is in progress
 */
#define STATE_SSR_ON 0x1
#define STATE_SSR_START 0x02
#define STATE_SSR_CHANNEL_OPEN_PENDING 0x04
#define STATE_SSR_PENDING_INIT  0x08
#define STATE_SSR_COMPLETE 0x00
#define STATE_SSR_OFF STATE_SSR_COMPLETE

static int ssr_state = STATE_SSR_OFF;


static int hcismd_set;
static DEFINE_SEMAPHORE(hci_smd_enable);

static int restart_in_progress;

static int hcismd_set_enable(const char *val, struct kernel_param *kp);
module_param_call(hcismd_set, hcismd_set_enable, NULL, &hcismd_set, 0644);

static void hci_dev_smd_open(struct work_struct *worker);
static void hci_dev_restart(struct work_struct *worker);

struct hci_smd_data {
	struct hci_dev *hdev;
	unsigned long flags;
	struct smd_channel *event_channel;
	struct smd_channel *data_channel;
	struct wake_lock wake_lock_tx;
	struct wake_lock wake_lock_rx;
	struct timer_list rx_q_timer;
	struct tasklet_struct rx_task;
};
static struct hci_smd_data hs;

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
	if (NULL != hdev->driver_data)
		kfree(hdev->driver_data);
}

static void hci_smd_recv_data(void)
{
	int len = 0;
	int rc = 0;
	struct sk_buff *skb = NULL;
	struct hci_smd_data *hsmd = &hs;
	wake_lock(&hs.wake_lock_rx);

	len = smd_read_avail(hsmd->data_channel);
	if (len > HCI_MAX_FRAME_SIZE) {
		BT_ERR("Frame larger than the allowed size, flushing frame");
		smd_read(hsmd->data_channel, NULL, len);
		goto out_data;
	}

	if (len <= 0)
		goto out_data;

	skb = bt_skb_alloc(len, GFP_ATOMIC);
	if (!skb) {
		BT_ERR("Error in allocating socket buffer");
		smd_read(hsmd->data_channel, NULL, len);
		goto out_data;
	}

	rc = smd_read(hsmd->data_channel, skb_put(skb, len), len);
	if (rc < len) {
		BT_ERR("Error in reading from the channel");
		goto out_data;
	}

	skb->dev = (void *)hsmd->hdev;
	bt_cb(skb)->pkt_type = HCI_ACLDATA_PKT;
	skb_orphan(skb);

	rc = hci_recv_frame(skb);
	if (rc < 0) {
		BT_ERR("Error in passing the packet to HCI Layer");
		/*
		 * skb is getting freed in hci_recv_frame, making it
		 * to null to avoid multiple access
		 */
		skb = NULL;
		goto out_data;
	}

	/*
	 * Start the timer to monitor whether the Rx queue is
	 * empty for releasing the Rx wake lock
	 */
	BT_DBG("Rx Timer is starting");
	mod_timer(&hsmd->rx_q_timer,
			jiffies + msecs_to_jiffies(RX_Q_MONITOR));

out_data:
	release_lock();
	if (rc)
		kfree_skb(skb);
}

static void hci_smd_recv_event(void)
{
	int len = 0;
	int rc = 0;
	struct sk_buff *skb = NULL;
	struct hci_smd_data *hsmd = &hs;
	wake_lock(&hs.wake_lock_rx);

	len = smd_read_avail(hsmd->event_channel);
	if (len > HCI_MAX_FRAME_SIZE) {
		BT_ERR("Frame larger than the allowed size, flushing frame");
		rc = smd_read(hsmd->event_channel, NULL, len);
		goto out_event;
	}

	while (len > 0) {
		skb = bt_skb_alloc(len, GFP_ATOMIC);
		if (!skb) {
			BT_ERR("Error in allocating socket buffer");
			smd_read(hsmd->event_channel, NULL, len);
			goto out_event;
		}

		rc = smd_read(hsmd->event_channel, skb_put(skb, len), len);
		if (rc < len) {
			BT_ERR("Error in reading from the event channel");
			goto out_event;
		}

		skb->dev = (void *)hsmd->hdev;
		bt_cb(skb)->pkt_type = HCI_EVENT_PKT;

		skb_orphan(skb);

		rc = hci_recv_frame(skb);
		if (rc < 0) {
			BT_ERR("Error in passing the packet to HCI Layer");
			/*
			 * skb is getting freed in hci_recv_frame, making it
			 *  to null to avoid multiple access
			 */
			skb = NULL;
			goto out_event;
		}

		len = smd_read_avail(hsmd->event_channel);
		/*
		 * Start the timer to monitor whether the Rx queue is
		 * empty for releasing the Rx wake lock
		 */
		BT_DBG("Rx Timer is starting");
		mod_timer(&hsmd->rx_q_timer,
				jiffies + msecs_to_jiffies(RX_Q_MONITOR));
	}
out_event:
	release_lock();
	if (rc)
		kfree_skb(skb);
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
		avail = smd_write_avail(hs.data_channel);
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
		BT_ERR("Uknown packet type");
		ret = -ENODEV;
		break;
	}

	kfree_skb(skb);
	wake_unlock(&hs.wake_lock_tx);
	return ret;
}

static void hci_smd_rx(unsigned long arg)
{
	struct hci_smd_data *hsmd = &hs;

	while ((smd_read_avail(hsmd->event_channel) > 0) ||
				(smd_read_avail(hsmd->data_channel) > 0)) {
		hci_smd_recv_event();
		hci_smd_recv_data();
	}
}

static void hci_smd_notify_event(void *data, unsigned int event)
{
	struct hci_dev *hdev = hs.hdev;
	struct hci_smd_data *hsmd = &hs;
	struct work_struct *reset_worker;
	struct work_struct *open_worker;

	int len = 0;

	if (!hdev) {
		BT_ERR("Frame for unknown HCI device (hdev=NULL)");
		return;
	}

	switch (event) {
	case SMD_EVENT_DATA:
		len = smd_read_avail(hsmd->event_channel);
		if (len > 0)
			tasklet_hi_schedule(&hs.rx_task);
		else if (len < 0)
			BT_ERR("Failed to read event from smd %d", len);

		break;
	case SMD_EVENT_OPEN:
		BT_INFO("opening HCI-SMD channel :%s", EVENT_CHANNEL);
		BT_DBG("SSR state is : %x", ssr_state);
		if ((ssr_state == STATE_SSR_OFF) ||
			(ssr_state == STATE_SSR_CHANNEL_OPEN_PENDING)) {

			hci_smd_open(hdev);
			open_worker = kzalloc(sizeof(*open_worker), GFP_ATOMIC);
			if (!open_worker) {
				BT_ERR("Out of memory");
				break;
			}
			if (ssr_state == STATE_SSR_CHANNEL_OPEN_PENDING) {
				ssr_state = STATE_SSR_PENDING_INIT;
				BT_INFO("SSR state is : %x", ssr_state);
			}
			INIT_WORK(open_worker, hci_dev_smd_open);
			schedule_work(open_worker);

		}
		break;
	case SMD_EVENT_CLOSE:
		BT_INFO("Closing HCI-SMD channel :%s", EVENT_CHANNEL);
		BT_DBG("SSR state is : %x", ssr_state);
		if ((ssr_state == STATE_SSR_OFF) ||
			(ssr_state == (STATE_SSR_PENDING_INIT))) {

			hci_smd_close(hdev);
			reset_worker = kzalloc(sizeof(*reset_worker),
							GFP_ATOMIC);
			if (!reset_worker) {
				BT_ERR("Out of memory");
				break;
			}
			ssr_state = STATE_SSR_ON;
			BT_INFO("SSR state is : %x", ssr_state);
			INIT_WORK(reset_worker, hci_dev_restart);
			schedule_work(reset_worker);

		} else if (ssr_state & STATE_SSR_ON) {
				BT_ERR("SSR state is : %x", ssr_state);
		}

		break;
	default:
		break;
	}
}

static void hci_smd_notify_data(void *data, unsigned int event)
{
	struct hci_dev *hdev = hs.hdev;
	struct hci_smd_data *hsmd = &hs;
	int len = 0;

	if (!hdev) {
		BT_ERR("Frame for unknown HCI device (hdev=NULL)");
		return;
	}

	switch (event) {
	case SMD_EVENT_DATA:
		len = smd_read_avail(hsmd->data_channel);
		if (len > 0)
			tasklet_hi_schedule(&hs.rx_task);
		else if (len < 0)
			BT_ERR("Failed to read data from smd %d", len);
		break;
	case SMD_EVENT_OPEN:
		BT_INFO("opening HCI-SMD channel :%s", DATA_CHANNEL);
		hci_smd_open(hdev);
		break;
	case SMD_EVENT_CLOSE:
		BT_INFO("Closing HCI-SMD channel :%s", DATA_CHANNEL);
		hci_smd_close(hdev);
		break;
	default:
		break;
	}

}

static int hci_smd_hci_register_dev(struct hci_smd_data *hsmd)
{
	struct hci_dev *hdev;

	if (hsmd->hdev)
		hdev = hsmd->hdev;
	else {
		BT_ERR("hdev is NULL");
		return 0;
	}
	/* Allow the incomming SSR even the prev one at PENDING INIT STATE
	 * since clenup need to be started again from the beging and ignore
	 *  or bypass the prev one
	 */
	if ((ssr_state == STATE_SSR_OFF) ||
			(ssr_state == STATE_SSR_PENDING_INIT)) {

		if (test_and_set_bit(HCI_REGISTER_SET, &hsmd->flags)) {
			BT_ERR("HCI device registered already");
			return 0;
		} else
			BT_INFO("HCI device registration is starting");
		if (hci_register_dev(hdev) < 0) {
			BT_ERR("Can't register HCI device");
			hci_free_dev(hdev);
			hsmd->hdev = NULL;
			clear_bit(HCI_REGISTER_SET, &hsmd->flags);
			return -ENODEV;
		}
		if (ssr_state == STATE_SSR_PENDING_INIT) {
			ssr_state = STATE_SSR_COMPLETE;
			BT_INFO("SSR state is : %x", ssr_state);
		}
	} else if (ssr_state)
		BT_ERR("Registration called in invalid context");
	return 0;
}

static int hci_smd_register_smd(struct hci_smd_data *hsmd)
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
	hdev->driver_data = NULL;
	hdev->open  = hci_smd_open;
	hdev->close = hci_smd_close;
	hdev->send  = hci_smd_send_frame;
	hdev->destruct = hci_smd_destruct;
	hdev->owner = THIS_MODULE;


	tasklet_init(&hsmd->rx_task,
			hci_smd_rx, (unsigned long) hsmd);
	/*
	 * Setup the timer to monitor whether the Rx queue is empty,
	 * to control the wake lock release
	 */
	setup_timer(&hsmd->rx_q_timer, schedule_timer,
			(unsigned long) hsmd->hdev);
	if (ssr_state == STATE_SSR_START) {
		ssr_state = STATE_SSR_CHANNEL_OPEN_PENDING;
		BT_INFO("SSR state is : %x", ssr_state);
	}
	/* Open the SMD Channel and device and register the callback function */
	rc = smd_named_open_on_edge(EVENT_CHANNEL, SMD_APPS_WCNSS,
			&hsmd->event_channel, hdev, hci_smd_notify_event);
	if (rc < 0) {
		BT_ERR("Cannot open the command channel");
		hci_free_dev(hdev);
		hsmd->hdev = NULL;
		return -ENODEV;
	}

	rc = smd_named_open_on_edge(DATA_CHANNEL, SMD_APPS_WCNSS,
			&hsmd->data_channel, hdev, hci_smd_notify_data);
	if (rc < 0) {
		BT_ERR("Failed to open the Data channel");
		hci_free_dev(hdev);
		hsmd->hdev = NULL;
		return -ENODEV;
	}

	/* Disable the read interrupts on the channel */
	smd_disable_read_intr(hsmd->event_channel);
	smd_disable_read_intr(hsmd->data_channel);
	return 0;
}

static void hci_smd_deregister_dev(struct hci_smd_data *hsmd)
{
	tasklet_kill(&hs.rx_task);
	if (ssr_state)
		BT_DBG("SSR state is : %x", ssr_state);
	/* Though the hci_smd driver is not registered with the hci
	 * need to close the opened channels as a part of cleaup
	 */
	if (!test_and_clear_bit(HCI_REGISTER_SET, &hsmd->flags)) {
		BT_ERR("HCI device un-registered already");
	} else {
		BT_INFO("HCI device un-registration going on");

		if (hsmd->hdev) {
			if (hci_unregister_dev(hsmd->hdev) < 0)
				BT_ERR("Can't unregister HCI device %s",
					hsmd->hdev->name);

			hci_free_dev(hsmd->hdev);
			hsmd->hdev = NULL;
		}
	}
	smd_close(hs.event_channel);
	smd_close(hs.data_channel);

	if (wake_lock_active(&hs.wake_lock_rx))
		wake_unlock(&hs.wake_lock_rx);
	if (wake_lock_active(&hs.wake_lock_tx))
		wake_unlock(&hs.wake_lock_tx);

	/*Destroy the timer used to monitor the Rx queue for emptiness */
	if (hs.rx_q_timer.function) {
		del_timer_sync(&hs.rx_q_timer);
		hs.rx_q_timer.function = NULL;
		hs.rx_q_timer.data = 0;
	}
}

static void hci_dev_restart(struct work_struct *worker)
{
	down(&hci_smd_enable);
	restart_in_progress = 1;
	BT_DBG("SSR state is : %x", ssr_state);

	if (ssr_state == STATE_SSR_ON) {
		ssr_state = STATE_SSR_START;
		BT_INFO("SSR state is : %x", ssr_state);
	} else {
		BT_ERR("restart triggered in wrong context");
		up(&hci_smd_enable);
		kfree(worker);
		return;
	}
	hci_smd_deregister_dev(&hs);
	hci_smd_register_smd(&hs);
	up(&hci_smd_enable);
	kfree(worker);

}

static void hci_dev_smd_open(struct work_struct *worker)
{
	down(&hci_smd_enable);
	if (ssr_state)
		BT_DBG("SSR state is : %x", ssr_state);

	if ((ssr_state != STATE_SSR_OFF) &&
			(ssr_state  !=  (STATE_SSR_PENDING_INIT))) {
		up(&hci_smd_enable);
		kfree(worker);
		return;
	}

	if (restart_in_progress == 1) {
		/* Allow wcnss to initialize */
		restart_in_progress = 0;
		msleep(10000);
	}

	hci_smd_hci_register_dev(&hs);
	up(&hci_smd_enable);
	kfree(worker);

}

static int hcismd_set_enable(const char *val, struct kernel_param *kp)
{
	int ret = 0;

	pr_err("hcismd_set_enable %d", hcismd_set);

	down(&hci_smd_enable);

	ret = param_set_int(val, kp);

	if (ret)
		goto done;

	/* Ignore the all incomming register de-register requests in case of
	 * SSR is in-progress
	 */
	switch (hcismd_set) {

	case 1:
		if ((hs.hdev == NULL) && (ssr_state == STATE_SSR_OFF))
			hci_smd_register_smd(&hs);
		else if (ssr_state)
			BT_ERR("SSR is in progress,state is : %x", ssr_state);

	break;
	case 0:
		if (ssr_state == STATE_SSR_OFF)
			hci_smd_deregister_dev(&hs);
		else if (ssr_state)
			BT_ERR("SSR is in progress,state is : %x", ssr_state);
	break;
	default:
		ret = -EFAULT;
	}

done:
	up(&hci_smd_enable);
	return ret;
}
static int  __init hci_smd_init(void)
{
	wake_lock_init(&hs.wake_lock_rx, WAKE_LOCK_SUSPEND,
			 "msm_smd_Rx");
	wake_lock_init(&hs.wake_lock_tx, WAKE_LOCK_SUSPEND,
			 "msm_smd_Tx");
	restart_in_progress = 0;
	ssr_state = STATE_SSR_OFF;
	hs.hdev = NULL;
	return 0;
}
module_init(hci_smd_init);

static void __exit hci_smd_exit(void)
{
	wake_lock_destroy(&hs.wake_lock_rx);
	wake_lock_destroy(&hs.wake_lock_tx);
}
module_exit(hci_smd_exit);

MODULE_AUTHOR("Ankur Nandwani <ankurn@codeaurora.org>");
MODULE_DESCRIPTION("Bluetooth SMD driver");
MODULE_LICENSE("GPL v2");
