/*
 * usb_typec_detect.c: usb type-c cable detecton driver
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
 * Author: Albin B <albin.bala.krishnan@intel.com>
 */

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/jiffies.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/wait.h>
#include <linux/usb/phy.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/usb_typec_phy.h>
#include "usb_typec_detect.h"

#define CC_OPEN(x)		(x == USB_TYPEC_CC_VRD_UNKNOWN)
#define CC_RD(x)		(x > USB_TYPEC_CC_VRA)
#define CC_RA(x)		(x == USB_TYPEC_CC_VRA)

#define TYPEC_DRPLOCK_TIMEOUT 130	/* 100 - 150ms */

#define TYPEC_CABLE_USB		"USB"
#define TYPEC_CABLE_USB_HOST	"USB-Host"

static const char *detect_extcon_cable[] = {
	TYPEC_CABLE_USB,
	TYPEC_CABLE_USB_HOST,
	NULL,
};

static LIST_HEAD(typec_detect_list);
static void detect_remove(struct typec_detect *detect);

static int detect_kthread(void *data)
{
	struct typec_detect *detect = (struct typec_detect *)data;
	struct typec_phy *phy;
	int state;

	if (!detect) {
		pr_err("%s: no detect found", __func__);
		return 0;
	}

	phy = detect->phy;

	do {
		detect->timer_evt = TIMER_EVENT_NONE;
		wait_event(detect->wq, detect->timer_evt);
		cancel_work_sync(&detect->dfp_work);

		if (detect->timer_evt == TIMER_EVENT_QUIT)
			break;

		mutex_lock(&detect->lock);
		if (detect->got_vbus) {
			mutex_unlock(&detect->lock);
			continue;
		}
		state = detect->state;
		mutex_unlock(&detect->lock);


		if (state == DETECT_STATE_UNATTACHED_DFP ||
			state == DETECT_STATE_UNATTACHED_DRP) {
			mutex_lock(&detect->lock);
			detect->state = DETECT_STATE_UNATTACHED_UFP;
			typec_switch_mode(phy, TYPEC_MODE_UFP);
			mutex_unlock(&detect->lock);
			/* next state start from VALID VBUS */
		} else if (state == DETECT_STATE_UNATTACHED_UFP) {
			mutex_lock(&detect->lock);
			detect->state = DETECT_STATE_UNATTACHED_DFP;
			typec_set_host_current(phy, TYPEC_CURRENT_USB);
			typec_switch_mode(phy, TYPEC_MODE_DFP);
			mutex_unlock(&detect->lock);
			schedule_work(&detect->dfp_work);
		}
	} while (true);

	return 0;
}

static enum typec_cc_pin get_active_cc(struct typec_cc_psy *cc1,
		struct typec_cc_psy *cc2)
{
	int ret = 0;

	if (CC_RD(cc1->v_rd) && (CC_OPEN(cc2->v_rd) || CC_RA(cc2->v_rd)))
		ret = TYPEC_PIN_CC1;
	else if (CC_RD(cc2->v_rd) && (CC_OPEN(cc1->v_rd) || CC_RA(cc1->v_rd)))
		ret = TYPEC_PIN_CC2;

	return ret;
}

/*
 * return 1 on VBUS presence (UFP detected),
 * 0 on measurement sucess,
 * -ERR on measuremet failure
 */
static int detect_measure_cc(struct typec_detect *detect, enum typec_cc_pin pin,
		struct typec_cc_psy *cc_psy, bool *found)
{
	int ret;

	ret = typec_measure_cc(detect->phy, pin, cc_psy, msecs_to_jiffies(3));
	if (ret >= 0) {
		*found = true;
		ret = 0;
	}
	mutex_lock(&detect->lock);
	if (detect->got_vbus) {
		ret = detect->got_vbus;
		mutex_unlock(&detect->lock);
		dev_err(detect->phy->dev, "%s:exiting got vbus cc%d\n",
				__func__, pin);
		return ret;
	}
	mutex_unlock(&detect->lock);
	return ret;
}

static void detect_dfp_work(struct work_struct *work)
{
	struct typec_detect *detect =
		container_of(work, struct typec_detect, dfp_work);
	bool cc1_found = false;
	bool cc2_found = false;
	int ret;
	enum typec_cc_pin use_cc = 0;
	struct typec_phy *phy = detect->phy;
	struct typec_cc_psy cc1 = {0, 0};
	struct typec_cc_psy cc2 = {0, 0};

	mutex_lock(&detect->lock);
	if (detect->state != DETECT_STATE_UNATTACHED_DFP || detect->got_vbus) {
		mutex_unlock(&detect->lock);
		return;
	}
	mutex_unlock(&detect->lock);

	ret = detect_measure_cc(detect, TYPEC_PIN_CC1, &cc1, &cc1_found);
	/* if vbus is received due to the UFP attachment, then break worker */
	if (ret > 0)
		return;

	ret = detect_measure_cc(detect, TYPEC_PIN_CC2, &cc2, &cc2_found);
	/* if vbus is received due to the UFP attachment, then break worker */
	if (ret > 0)
		return;

	dev_dbg(detect->phy->dev,
		"cc1_found = %d cc2_found = %d unattach dfp cc1 = %d, cc2 = %d",
		cc1_found, cc2_found, cc1.v_rd, cc2.v_rd);

	if (cc1_found && cc2_found) {
		if (((CC_RA(cc1.v_rd) || (CC_OPEN(cc1.v_rd)))
				&& CC_RD(cc2.v_rd)) ||
			(CC_RD(cc1.v_rd) && (CC_RA(cc2.v_rd) ||
					CC_OPEN(cc2.v_rd)))) {
			del_timer(&detect->drp_timer); /* disable timer */
			mutex_lock(&detect->lock);
			detect->state = DETECT_STATE_ATTACH_DFP_DRP_WAIT;
			mutex_unlock(&detect->lock);
			usleep_range(100000, 150000);
			mutex_lock(&detect->lock);
			detect->state = DETECT_STATE_ATTACHED_DFP;
			mutex_unlock(&detect->lock);
			use_cc = get_active_cc(&cc1, &cc2);
			typec_setup_cc(phy, use_cc, TYPEC_STATE_ATTACHED_DFP);

			extcon_set_cable_state(detect->edev, "USB-Host", true);
			atomic_notifier_call_chain(&detect->otg->notifier,
				USB_EVENT_ID, NULL);

			return;
		} else if (CC_RA(cc1.v_rd) && CC_RA(cc2.v_rd)) {
			mutex_lock(&detect->lock);
			detect->state = DETECT_STATE_ATTACHED_DFP;
			mutex_unlock(&detect->lock);
			/* TODO: Need to set the phy state */
			del_timer(&detect->drp_timer); /* disable timer */
			/* Audio Accessory. */
			/* next state Attached UFP based on VBUS */
			dev_info(detect->phy->dev, "Audio Accessory Detected");
			return;
		} else if (CC_RD(cc1.v_rd) && CC_RD(cc2.v_rd)) {
			mutex_lock(&detect->lock);
			detect->state = DETECT_STATE_ATTACHED_DFP;
			mutex_unlock(&detect->lock);
			del_timer(&detect->drp_timer); /* disable timer */
			/* Debug Accessory */
			/* next state Attached UFP based on VBUS */
			dev_info(detect->phy->dev, "Debug Accessory Detected");
			return;
		}
	}
	schedule_work(&detect->dfp_work);
}

static void detect_drp_timer(unsigned long data)
{
	struct typec_detect *detect = (struct typec_detect *)data;
	struct typec_phy *phy;

	phy = detect->phy;
	if (!phy) {
		pr_err("%s: no valid phy registered", __func__);
		return;
	}

	detect->timer_evt = TIMER_EVENT_PROCESS;
	wake_up(&detect->wq);
	mod_timer(&detect->drp_timer, jiffies + msecs_to_jiffies(50));
}

static int get_chrgcur_from_rd(enum  typec_cc_level rd1,
				enum typec_cc_level rd2)
{
	int ma;
	enum typec_cc_level use_rd;

	if (CC_RA(rd1))
		use_rd = rd2;
	else
		use_rd = rd1;

	switch (use_rd) {
	case USB_TYPEC_CC_VRD_USB:
		ma = 0;
		break;
	case USB_TYPEC_CC_VRD_1500:
		ma = 1500;
		break;
	case USB_TYPEC_CC_VRD_3000:
		ma = 3000;
		break;
	default:
		ma = 0;
		break;
	}

	return ma;
}

static void detect_lock_ufp_work(struct work_struct *work)
{
	struct typec_detect *detect = container_of(work, struct typec_detect,
					lock_ufp_work);
	int ret;
	/* tDRPLock - 100 to 150ms */
	unsigned long timeout = msecs_to_jiffies(TYPEC_DRPLOCK_TIMEOUT);

	typec_switch_mode(detect->phy, TYPEC_MODE_UFP);
	ret = wait_for_completion_timeout(&detect->lock_ufp_complete, timeout);
	if (ret == 0) {
		mutex_lock(&detect->lock);
		detect->state = DETECT_STATE_UNATTACHED_DRP;
		mutex_unlock(&detect->lock);
		typec_switch_mode(detect->phy, TYPEC_MODE_DRP);
	}
	/* got vbus, goto attached ufp */

	return;
}

static void update_phy_state(struct work_struct *work)
{
	struct typec_phy *phy;
	struct typec_detect *detect;
	int ret;
	enum typec_cc_pin use_cc = 0;
	struct typec_cc_psy cc1_psy, cc2_psy;
	struct power_supply_cable_props cable_props = {0};
	int state;

	detect = container_of(work, struct typec_detect, phy_ntf_work);
	phy = detect->phy;

	switch (detect->event) {
	case TYPEC_EVENT_VBUS:
		mutex_lock(&detect->lock);
		detect->got_vbus = true;
		state = detect->state;
		if (state == DETECT_STATE_LOCK_UFP)
			complete(&detect->lock_ufp_complete);
		mutex_unlock(&detect->lock);


		cancel_work_sync(&detect->dfp_work);
		del_timer(&detect->drp_timer); /* disable timer */
		if (state == DETECT_STATE_ATTACHED_DFP)
			break;
		else if ((state == DETECT_STATE_UNATTACHED_DFP) ||
				(state == DETECT_STATE_UNATTACHED_DRP)) {
			mutex_lock(&detect->lock);
			typec_switch_mode(phy, TYPEC_MODE_UFP);
			mutex_unlock(&detect->lock);
		}

		mutex_lock(&detect->lock);

		ret = typec_measure_cc(phy, TYPEC_PIN_CC1, &cc1_psy, 0);
		if (ret < 0) {
			dev_err(detect->phy->dev,
					"%s: Error(%d) measuring cc1\n",
					__func__, ret);
			break;
		}

		ret = typec_measure_cc(phy, TYPEC_PIN_CC2, &cc2_psy, 0);
		if (ret < 0) {
			dev_err(detect->phy->dev,
					"%s: Error(%d) measuring cc2\n",
					__func__, ret);
			break;
		}

		mutex_unlock(&detect->lock);
		dev_info(detect->phy->dev, "evt_vbus cc1 = %d, cc2 = %d",
						cc1_psy.v_rd, cc2_psy.v_rd);

		/* try another time? */
		if (CC_OPEN(cc1_psy.v_rd) || CC_RA(cc1_psy.v_rd)) {
			ret = typec_measure_cc(phy, TYPEC_PIN_CC1, &cc1_psy, 0);
			if (ret < 0) {
				dev_err(detect->phy->dev,
					"%s: Error(%d) measuring cc1\n",
					__func__, ret);
				break;
			}
		}
		if (CC_OPEN(cc2_psy.v_rd) || CC_RA(cc2_psy.v_rd)) {
			ret = typec_measure_cc(phy, TYPEC_PIN_CC2, &cc2_psy, 0);
			if (ret < 0) {
				dev_err(detect->phy->dev,
					"%s: Error(%d) measuring cc2\n",
					__func__, ret);
				break;
			}
		}

		use_cc = get_active_cc(&cc1_psy, &cc2_psy);
		if (CC_OPEN(cc1_psy.v_rd) && CC_OPEN(cc2_psy.v_rd)) {
			/* nothing connected */
		} else if (use_cc) {
			/* valid cc found; UFP_ATTACHED */
			mutex_lock(&detect->lock);
			detect->state = DETECT_STATE_ATTACHED_UFP;
			mutex_unlock(&detect->lock);
			typec_setup_cc(phy, use_cc, TYPEC_STATE_ATTACHED_UFP);
			extcon_set_cable_state(detect->edev, "USB", true);

			/* notify power supply */
			cable_props.chrg_evt =
					POWER_SUPPLY_CHARGER_EVENT_CONNECT;
			cable_props.chrg_type =
					POWER_SUPPLY_CHARGER_TYPE_USB_TYPEC;
			cable_props.ma = get_chrgcur_from_rd(cc1_psy.v_rd,
								cc2_psy.v_rd);
			atomic_notifier_call_chain(&power_supply_notifier,
							PSY_CABLE_EVENT,
							&cable_props);
		}
		break;
	case TYPEC_EVENT_NONE:
		mutex_lock(&detect->lock);
		detect->got_vbus = false;
		typec_setup_cc(phy, 0, TYPEC_STATE_UNATTACHED_UFP);
		mutex_unlock(&detect->lock);
		if (detect->state == DETECT_STATE_ATTACHED_UFP) {
			extcon_set_cable_state(detect->edev, "USB", false);
			/* notify power supply */
			cable_props.chrg_evt =
				POWER_SUPPLY_CHARGER_EVENT_DISCONNECT;
			cable_props.chrg_type =
				POWER_SUPPLY_CHARGER_TYPE_USB_TYPEC;
			cable_props.ma = 0;
			atomic_notifier_call_chain(&power_supply_notifier,
							PSY_CABLE_EVENT,
							&cable_props);
		} else {
			/* state = DFP; disable VBUS */
			extcon_set_cable_state(detect->edev,
							"USB-Host", false);

			reinit_completion(&detect->lock_ufp_complete);
			mutex_lock(&detect->lock);
			detect->state = DETECT_STATE_LOCK_UFP;
			mutex_unlock(&detect->lock);
			queue_work(detect->wq_lock_ufp,
					&detect->lock_ufp_work);
			atomic_notifier_call_chain(&detect->otg->notifier,
					USB_EVENT_NONE, NULL);
			break;
		}
		mutex_lock(&detect->lock);
		detect->state = DETECT_STATE_UNATTACHED_DRP;
		mutex_unlock(&detect->lock);
		break;
	default:
		dev_err(detect->phy->dev, "unknown event %d", detect->event);
	}
}

static int typec_handle_phy_ntf(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct typec_phy *phy;
	struct typec_detect *detect =
		container_of(nb, struct typec_detect, nb);
	int handled = NOTIFY_OK;

	phy = detect->phy;
	if (!phy)
		return NOTIFY_BAD;

	detect->event = event;
	switch (event) {
	case TYPEC_EVENT_VBUS:
	case TYPEC_EVENT_NONE:
		schedule_work(&detect->phy_ntf_work);
		break;
	case TYPEC_EVENT_DRP:
		dev_info(detect->phy->dev, "EVNT DRP");
		detect->state = DETECT_STATE_UNATTACHED_DRP;
		/* start the timer now */
		mod_timer(&detect->drp_timer, jiffies +
				msecs_to_jiffies(1));
		break;
	default:
		handled = NOTIFY_DONE;
	}
	return handled;
}

static int detect_otg_notifier(struct notifier_block *nb, unsigned long event,
				void *param)
{
	return NOTIFY_DONE;
}

int typec_bind_detect(struct typec_phy *phy)
{
	struct typec_detect *detect;
	int ret;

	detect = kzalloc(sizeof(struct typec_detect), GFP_KERNEL);

	if (!detect) {
		pr_err("typec fsm: no memory");
		return -ENOMEM;
	}

	if (!phy) {
		pr_err("%s: no valid phy provided", __func__);
		return -EINVAL;
	}

	detect->phy = phy;
	detect->nb.notifier_call = typec_handle_phy_ntf;

	ret = typec_register_notifier(phy, &detect->nb);
	if (ret  < 0) {
		dev_err(phy->dev, "unable to register notifier");
		goto error;
	}

	init_waitqueue_head(&detect->wq);

	INIT_WORK(&detect->phy_ntf_work, update_phy_state);
	INIT_WORK(&detect->dfp_work, detect_dfp_work);

	setup_timer(&detect->drp_timer, detect_drp_timer,
			(unsigned long)detect);

	detect->detect_kthread = kthread_run(detect_kthread, detect, "detect");
	detect->state = DETECT_STATE_UNATTACHED_DRP;

	detect->otg = usb_get_phy(USB_PHY_TYPE_USB2);
	if (IS_ERR_OR_NULL(detect->otg)) {
		detect->otg = NULL;
		ret = -EINVAL;
		goto error;
	} else {
		detect->otg_nb.notifier_call = detect_otg_notifier;
		ret = usb_register_notifier(detect->otg, &detect->otg_nb);
		if (ret < 0)
			goto error;
	}
	mutex_init(&detect->lock);
	detect->wq_lock_ufp = create_singlethread_workqueue("wq_lock_ufp");
	INIT_WORK(&detect->lock_ufp_work, detect_lock_ufp_work);
	init_completion(&detect->lock_ufp_complete);

	detect->edev = devm_kzalloc(phy->dev, sizeof(struct extcon_dev),
					GFP_KERNEL);

	if (!detect->edev) {
		ret = -ENOMEM;
		goto error;
	}
	detect->edev->name = "usb-typec";
	detect->edev->supported_cable = detect_extcon_cable;
	ret = extcon_dev_register(detect->edev);
	if (ret) {
		devm_kfree(phy->dev, detect->edev);
		goto error;
	}

	list_add_tail(&detect->list, &typec_detect_list);
	return 0;

error:
	detect_remove(detect);
	return ret;
}

static void detect_remove(struct typec_detect *detect)
{
	if (detect) {
		cancel_work_sync(&detect->phy_ntf_work);
		cancel_work_sync(&detect->dfp_work);
		del_timer(&detect->drp_timer);
		detect->timer_evt = TIMER_EVENT_QUIT;
		wake_up(&detect->wq);

		if (detect->otg) {
			usb_unregister_notifier(detect->otg, &detect->otg_nb);
			usb_put_phy(detect->otg);
			detect->otg = NULL;
		}
		if (detect->edev)
			extcon_dev_unregister(detect->edev);
		kfree(detect);
	}

	return;
}

int typec_unbind_detect(struct typec_phy *phy)
{
	struct typec_detect *detect;

	list_for_each_entry(detect, &typec_detect_list, list) {
		if (strcmp(detect->phy->label, phy->label)) {
			list_del(&detect->list);
			detect_remove(detect);
		}
	}
	return 0;
}
