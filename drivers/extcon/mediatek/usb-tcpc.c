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
#include "inc/tcpm.h"
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <extcon_usb.h>
#ifdef CONFIG_MTK_USB_TYPEC_U3_MUX
#include "usb_switch.h"
#include "typec.h"
#endif

static struct notifier_block otg_nb;
static bool usbc_otg_attached;
static struct tcpc_device *otg_tcpc_dev;
static struct mutex tcpc_otg_lock;
static bool tcpc_boost_on;

static int tcpc_otg_enable(void)
{
	if (!usbc_otg_attached) {
		mt_usbhost_connect();
		usbc_otg_attached = true;
	}
	return 0;
}

static int tcpc_otg_disable(void)
{
	if (usbc_otg_attached) {
		mt_usbhost_disconnect();
		usbc_otg_attached = false;
	}
	return 0;
}

static void tcpc_power_work_call(bool enable)
{
	if (enable) {
		if (!tcpc_boost_on) {
			mt_vbus_on();
			tcpc_boost_on = true;
		}
	} else {
		if (tcpc_boost_on) {
			mt_vbus_off();
			tcpc_boost_on = false;
		}
	}
}

static int otg_tcp_notifier_call(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	bool otg_power_enable, otg_on;

	mutex_lock(&tcpc_otg_lock);
	otg_on = usbc_otg_attached;
	mutex_unlock(&tcpc_otg_lock);

	switch (event) {
	case TCP_NOTIFY_SOURCE_VBUS:
		pr_info("%s source vbus = %dmv\n",
				__func__, noti->vbus_state.mv);
		otg_power_enable = (noti->vbus_state.mv) ? true : false;
		tcpc_power_work_call(otg_power_enable);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		pr_info("%s, TCP_NOTIFY_TYPEC_STATE, old_state=%d, new_state=%d\n",
				__func__, noti->typec_state.old_state,
				noti->typec_state.new_state);

		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			pr_info("%s OTG Plug in\n", __func__);
			tcpc_otg_enable();
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SNK) &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			if (otg_on) {
				pr_info("%s OTG Plug out\n", __func__);
				tcpc_otg_disable();
			} else {
				pr_info("%s USB Plug out\n", __func__);
				mt_usb_disconnect();
			}
		}
		/* switch U3 mux */
#ifdef CONFIG_MTK_USB_TYPEC_U3_MUX
		if (noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK) {
			usb3_switch_dps_en(false);
			if (noti->typec_state.polarity == 0)
				usb3_switch_ctrl_sel(CC1_SIDE);
			else
				usb3_switch_ctrl_sel(CC2_SIDE);
		} else if (noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			usb3_switch_dps_en(false);
			if (noti->typec_state.polarity == 0)
				usb3_switch_ctrl_sel(CC2_SIDE);
			else
				usb3_switch_ctrl_sel(CC1_SIDE);
		} else if (noti->typec_state.new_state == TYPEC_UNATTACHED) {
			usb3_switch_dps_en(true);
		}
#endif
		break;
	case TCP_NOTIFY_DR_SWAP:
		pr_info("%s TCP_NOTIFY_DR_SWAP, new role=%d\n",
				__func__, noti->swap_state.new_role);
		if (otg_on &&
			noti->swap_state.new_role == PD_ROLE_UFP) {
			pr_info("%s switch role to device\n", __func__);
			tcpc_otg_disable();
			mt_usb_connect();
		} else if (!otg_on &&
			noti->swap_state.new_role == PD_ROLE_DFP) {
			pr_info("%s switch role to host\n", __func__);
			mt_usb_disconnect();
			tcpc_otg_enable();
		}
		break;
	}
	return NOTIFY_OK;
}


static int __init mtk_typec_init(void)
{
	int ret;

	mutex_init(&tcpc_otg_lock);

	otg_tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!otg_tcpc_dev) {
		pr_info("%s get tcpc device type_c_port0 fail\n", __func__);
		return -ENODEV;
	}

	otg_nb.notifier_call = otg_tcp_notifier_call;
	ret = register_tcp_dev_notifier(otg_tcpc_dev, &otg_nb,
		TCP_NOTIFY_TYPE_USB|TCP_NOTIFY_TYPE_VBUS|TCP_NOTIFY_TYPE_MISC);
	if (ret < 0) {
		pr_info("%s register tcpc notifer fail\n", __func__);
		return -EINVAL;
	}

	return 0;
}

late_initcall(mtk_typec_init);

static void __exit mtk_typec_init_cleanup(void)
{
}

module_exit(mtk_typec_init_cleanup);

