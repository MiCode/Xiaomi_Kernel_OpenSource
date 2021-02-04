/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/atomic.h>
#include <xhci-mtk-driver.h>
#include <typec.h>
#ifdef CONFIG_USB_C_SWITCH_U3_MUX
#include "usb_switch.h"
#include "typec.h"
#endif


#ifdef CONFIG_TCPC_CLASS


#include "tcpm.h"
#include "musb_core.h"
#include <linux/workqueue.h>
#include <linux/mutex.h>
static struct notifier_block otg_nb;
static struct tcpc_device *otg_tcpc_dev;
static struct mutex tcpc_otg_lock;
static bool tcpc_otg_attached;

struct tcpc_otg_work {
	struct delayed_work dwork;
	int ops;
};

enum OTG_OPS {
	OTG_OPS_OFF = 0,
	OTG_OPS_ON
};

enum VBUS_OPS {
	VBUS_OPS_OFF = 0,
	VBUS_OPS_ON
};
#endif /* CONFIG_TCPC_CLASS */

#ifdef CONFIG_USB_C_SWITCH
#ifndef CONFIG_TCPC_CLASS
static int typec_otg_enable(void *data)
{
	pr_info("%s\n", __func__);
	return mtk_xhci_driver_load(false);
}

static int typec_otg_disable(void *data)
{
	pr_info("%s\n", __func__);
	mtk_xhci_driver_unload(false);
	return 0;
}

static struct typec_switch_data typec_host_driver = {
	.name = "xhci-mtk",
	.type = HOST_TYPE,
	.enable = typec_otg_enable,
	.disable = typec_otg_disable,
};
#endif /* if not CONFIG_TCPC_CLASS */
#endif

#ifdef CONFIG_TCPC_CLASS
static void do_vbus_work(struct work_struct *data)
{
	struct tcpc_otg_work *work =
		container_of(data, struct tcpc_otg_work, dwork.work);
	bool vbus_on = (work->ops ==
			VBUS_OPS_ON ? true : false);

	pr_info("%s vbus_on=%d\n", __func__, vbus_on);

	if (vbus_on)
		mtk_xhci_enable_vbus();
	else
		mtk_xhci_disable_vbus();

	/* free kfree */
	kfree(work);
}

static void issue_vbus_work(int ops, int delay)
{
	struct tcpc_otg_work *work;
	struct workqueue_struct *st_wq  = mt_usb_get_workqueue();

	if (!st_wq) {
		pr_info("%s st_wq = NULL\n", __func__);
		return;
	}
	/* create and prepare worker */
	work = kzalloc(sizeof(struct tcpc_otg_work), GFP_ATOMIC);

	if (!work)
		return;

	work->ops = ops;
	INIT_DELAYED_WORK(&work->dwork, do_vbus_work);

	/* issue vbus work */
	pr_info("%s issue work, ops<%d>, delay<%d>\n",
			__func__, ops, delay);

	queue_delayed_work(st_wq, &work->dwork,
			msecs_to_jiffies(delay));
}

static void tcpc_vbus_enable(bool enable)
{
	if (enable)
		issue_vbus_work(VBUS_OPS_ON, 0);
	else
		issue_vbus_work(VBUS_OPS_OFF, 0);
}

static void do_otg_work(struct work_struct *data)
{
	struct tcpc_otg_work *work =
		container_of(data, struct tcpc_otg_work, dwork.work);
	bool otg_on = (work->ops ==
			OTG_OPS_ON ? true : false);

	pr_info("%s otg_on=%d\n", __func__, otg_on);

	if (otg_on)
		mtk_xhci_driver_load(false);
	else
		mtk_xhci_driver_unload(false);

	/* free kfree */
	kfree(work);
}

static void issue_otg_work(int ops, int delay)
{
	struct tcpc_otg_work *work;
	struct workqueue_struct *st_wq = mt_usb_get_workqueue();

	if (!st_wq) {
		pr_info("%s st_wq = NULL\n", __func__);
		return;
	}

	/* create and prepare worker */
	work = kzalloc(sizeof(struct tcpc_otg_work), GFP_ATOMIC);

	if (!work)
		return;

	work->ops = ops;
	INIT_DELAYED_WORK(&work->dwork, do_otg_work);

	/* issue connection work */
	pr_info("%s issue work, ops<%d>, delay<%d>\n",
			__func__, ops, delay);

	queue_delayed_work(st_wq, &work->dwork,
			msecs_to_jiffies(delay));
}

static void tcpc_otg_enable(bool enable)
{
	mutex_lock(&tcpc_otg_lock);
	tcpc_otg_attached = (enable ? true : false);
	mutex_unlock(&tcpc_otg_lock);

	if (enable)
		issue_otg_work(OTG_OPS_ON, 0);
	else
		issue_otg_work(OTG_OPS_OFF, 0);
}

static int otg_tcp_notifier_call(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	bool otg_on;

	mutex_lock(&tcpc_otg_lock);
	otg_on = tcpc_otg_attached;
	mutex_unlock(&tcpc_otg_lock);

	switch (event) {
	case TCP_NOTIFY_SOURCE_VBUS:
		pr_info("%s source vbus = %dmv\n",
				__func__, noti->vbus_state.mv);
		if (noti->vbus_state.mv)
			tcpc_vbus_enable(true);
		else
			tcpc_vbus_enable(false);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		pr_info("%s TCP_NOTIFY_TYPEC_STATE, old=%d, new=%d\n",
			__func__, noti->typec_state.old_state,
			noti->typec_state.new_state);
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			pr_info("%s OTG Plug in\n", __func__);
			tcpc_otg_enable(true);
#ifdef CONFIG_USB_C_SWITCH_U3_MUX
			usb3_switch_dps_en(false);
			if (noti->typec_state.polarity == 0)
				usb3_switch_ctrl_sel(CC2_SIDE);
			else
				usb3_switch_ctrl_sel(CC1_SIDE);
#endif
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SNK) &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			if (otg_on) {
				pr_info("%s OTG Plug out\n", __func__);
				tcpc_otg_enable(false);
			} else {
				pr_info("%s USB Plug out\n", __func__);
				mt_usb_disconnect();
			}
#ifdef CONFIG_USB_C_SWITCH_U3_MUX
			usb3_switch_dps_en(true);
#endif
		}
		break;
	case TCP_NOTIFY_DR_SWAP:
		pr_info("%s TCP_NOTIFY_DR_SWAP, new role=%d\n",
				__func__, noti->swap_state.new_role);
		if (otg_on &&
			noti->swap_state.new_role == PD_ROLE_UFP) {
			pr_info("%s switch role to device\n", __func__);
			tcpc_otg_enable(false);
			mt_usb_connect();
		} else if (!otg_on &&
			noti->swap_state.new_role == PD_ROLE_DFP) {
			pr_info("%s switch role to host\n", __func__);
			mt_usb_disconnect();
			mt_usb_dev_off();
			tcpc_otg_enable(true);
		}
		break;
	}
	return NOTIFY_OK;
}
#endif /* CONFIG_TCPC_CLASS */

static int __init rt_typec_init(void)
{
#ifdef CONFIG_TCPC_CLASS
		int ret;
#endif /* CONFIG_TCPC_CLASS */


#ifdef CONFIG_USB_C_SWITCH
#ifndef CONFIG_TCPC_CLASS
		typec_host_driver.priv_data = NULL;
		register_typec_switch_callback(&typec_host_driver);
#endif /* if not CONFIG_TCPC_CLASS */
#endif

#ifdef CONFIG_TCPC_CLASS
	mutex_init(&tcpc_otg_lock);

	otg_tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!otg_tcpc_dev) {
		pr_info("%s get tcpc device type_c_port0 fail\n", __func__);
		return -ENODEV;
	}

	otg_nb.notifier_call = otg_tcp_notifier_call;
	ret = register_tcp_dev_notifier(otg_tcpc_dev, &otg_nb,
		TCP_NOTIFY_TYPE_USB|TCP_NOTIFY_TYPE_VBUS|
		TCP_NOTIFY_TYPE_MISC);
	if (ret < 0) {
		pr_info("%s register tcpc notifer fail\n", __func__);
		return -EINVAL;
	}
#endif /* CONFIG_TCPC_CLASS */
	return 0;
}

late_initcall(rt_typec_init);

static void __exit rt_typec_init_cleanup(void)
{
}

module_exit(rt_typec_init_cleanup);

