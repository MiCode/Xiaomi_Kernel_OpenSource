/*
 * usb_typec_detect.h: usb type-c cable detection header file
 *
 * Copyright (C) 2014 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. Seee the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Kannappan, R <r.kannappan@intel.com>
 */

#ifndef __USB_TYPEC_DETECT_H__
#define __USB_TYPEC_DETECT_H__

#include <linux/timer.h>
#include <linux/extcon.h>
#include <linux/usb_typec_phy.h>

enum typec_detect_state {
	DETECT_STATE_UNATTACHED_UFP,
	DETECT_STATE_UNATTACHED_DFP,
	DETECT_STATE_UNATTACHED_DRP,
	DETECT_STATE_ATTACHED_DFP,
	DETECT_STATE_ATTACH_DFP_DRP_WAIT,
	DETECT_STATE_LOCK_UFP,
	DETECT_STATE_ATTACHED_UFP,
};

enum {
	TIMER_EVENT_NONE,
	TIMER_EVENT_PROCESS,
	TIMER_EVENT_QUIT
};

struct typec_detect {
	struct typec_phy *phy;
	struct extcon_dev *edev;
	enum typec_detect_state state;
	enum typec_detect_state old_state;
	enum typec_event event;
	struct notifier_block nb;
	struct work_struct phy_ntf_work;
	struct work_struct dfp_work;
	struct timer_list drp_timer;
	struct list_head list;
	struct usb_phy *otg;
	struct notifier_block otg_nb;
	struct task_struct *detect_kthread;
	struct workqueue_struct *wq_lock_ufp;
	struct work_struct lock_ufp_work;
	struct completion lock_ufp_complete;
	int timer_evt;
	bool got_vbus;
	wait_queue_head_t wq;
	struct mutex lock;
};

extern int typec_bind_detect(struct typec_phy *phy);
extern int typec_unbind_detect(struct typec_phy *phy);

#endif /* __USB_TYPEC_DETECT_H__ */
