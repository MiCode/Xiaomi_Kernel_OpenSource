/**
 * dwc3_otg.c - DesignWare USB3 DRD Controller OTG
 *
 * Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include "core.h"
#include "dwc3_otg.h"
#include "io.h"
#include "debug.h"
#include "xhci.h"

#define VBUS_REG_CHECK_DELAY	(msecs_to_jiffies(1000))
#define MAX_INVALID_CHRGR_RETRY 3
static int max_chgr_retry_count = MAX_INVALID_CHRGR_RETRY;
module_param(max_chgr_retry_count, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(max_chgr_retry_count, "Max invalid charger retry count");

static void dwc3_otg_notify_host_mode(struct usb_otg *otg, int host_mode);

/**
 * dwc3_otg_start_host -  helper function for starting/stoping the host controller driver.
 *
 * @otg: Pointer to the otg_transceiver structure.
 * @on: start / stop the host controller driver.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_otg_start_host(struct usb_otg *otg, int on)
{
	struct dwc3_otg *dotg = container_of(otg, struct dwc3_otg, otg);
	struct dwc3_ext_xceiv *ext_xceiv = dotg->ext_xceiv;
	struct dwc3 *dwc = dotg->dwc;
	struct usb_hcd *hcd;
	int ret = 0;

	if (!dwc->xhci)
		return -EINVAL;

	if (!dotg->vbus_otg) {
		dotg->vbus_otg = devm_regulator_get(dwc->dev->parent,
							"vbus_dwc3");
		if (IS_ERR(dotg->vbus_otg)) {
			dev_err(dwc->dev, "Failed to get vbus regulator\n");
			ret = PTR_ERR(dotg->vbus_otg);
			dotg->vbus_otg = 0;
			return ret;
		}
	}

	if (on) {
		dev_dbg(otg->phy->dev, "%s: turn on host\n", __func__);

		dwc3_otg_notify_host_mode(otg, on);
		usb_phy_notify_connect(dotg->dwc->usb2_phy, USB_SPEED_HIGH);
		ret = regulator_enable(dotg->vbus_otg);
		if (ret) {
			dev_err(otg->phy->dev, "unable to enable vbus_otg\n");
			dwc3_otg_notify_host_mode(otg, 0);
			return ret;
		}

		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_HOST);

		/*
		 * FIXME If micro A cable is disconnected during system suspend,
		 * xhci platform device will be removed before runtime pm is
		 * enabled for xhci device. Due to this, disable_depth becomes
		 * greater than one and runtimepm is not enabled for next microA
		 * connect. Fix this by calling pm_runtime_init for xhci device.
		 */
		pm_runtime_init(&dwc->xhci->dev);
		ret = platform_device_add(dwc->xhci);
		if (ret) {
			dev_err(otg->phy->dev,
				"%s: failed to add XHCI pdev ret=%d\n",
				__func__, ret);
			regulator_disable(dotg->vbus_otg);
			dwc3_otg_notify_host_mode(otg, 0);
			return ret;
		}

		/*
		 * WORKAROUND: currently host mode suspend isn't working well.
		 * Disable xHCI's runtime PM for now.
		 */
		pm_runtime_disable(&dwc->xhci->dev);

		hcd = platform_get_drvdata(dwc->xhci);
		otg->host = &hcd->self;

		dwc3_gadget_usb3_phy_suspend(dwc, true);
	} else {
		dev_dbg(otg->phy->dev, "%s: turn off host\n", __func__);

		ret = regulator_disable(dotg->vbus_otg);
		if (ret) {
			dev_err(otg->phy->dev, "unable to disable vbus_otg\n");
			return ret;
		}

		dbg_event(0xFF, "StHost get", 0);
		pm_runtime_get(dwc->dev);
		usb_phy_notify_disconnect(dotg->dwc->usb2_phy, USB_SPEED_HIGH);
		dwc3_otg_notify_host_mode(otg, on);
		otg->host = NULL;
		platform_device_del(dwc->xhci);

		/*
		 * Perform USB hardware RESET (both core reset and DBM reset)
		 * when moving from host to peripheral. This is required for
		 * peripheral mode to work.
		 */
		if (ext_xceiv && ext_xceiv->ext_block_reset)
			ext_xceiv->ext_block_reset(ext_xceiv, true);

		dwc3_gadget_usb3_phy_suspend(dwc, false);
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_DEVICE);

		/* re-init core and OTG registers as block reset clears these */
		dwc3_post_host_reset_core_init(dwc);
		dbg_event(0xFF, "StHost put", 0);
		pm_runtime_put(dwc->dev);
	}

	return 0;
}

/**
 * dwc3_otg_start_peripheral -  bind/unbind the peripheral controller.
 *
 * @otg: Pointer to the otg_transceiver structure.
 * @gadget: pointer to the usb_gadget structure.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_otg_start_peripheral(struct usb_otg *otg, int on)
{
	struct dwc3_otg *dotg = container_of(otg, struct dwc3_otg, otg);
	struct dwc3_ext_xceiv *ext_xceiv = dotg->ext_xceiv;

	if (!otg->gadget)
		return -EINVAL;

	if (on) {
		dev_dbg(otg->phy->dev, "%s: turn on gadget %s\n",
					__func__, otg->gadget->name);

		usb_phy_notify_connect(dotg->dwc->usb2_phy, USB_SPEED_HIGH);
		usb_phy_notify_connect(dotg->dwc->usb3_phy, USB_SPEED_SUPER);

		/* Core reset is not required during start peripheral. Only
		 * DBM reset is required, hence perform only DBM reset here */
		if (ext_xceiv && ext_xceiv->ext_block_reset)
			ext_xceiv->ext_block_reset(ext_xceiv, false);

		dwc3_set_mode(dotg->dwc, DWC3_GCTL_PRTCAP_DEVICE);
		usb_gadget_vbus_connect(otg->gadget);
	} else {
		dev_dbg(otg->phy->dev, "%s: turn off gadget %s\n",
					__func__, otg->gadget->name);
		usb_gadget_vbus_disconnect(otg->gadget);
		usb_phy_notify_disconnect(dotg->dwc->usb2_phy, USB_SPEED_HIGH);
		usb_phy_notify_disconnect(dotg->dwc->usb3_phy, USB_SPEED_SUPER);
		dwc3_gadget_usb3_phy_suspend(dotg->dwc, false);
	}

	return 0;
}

/**
 * dwc3_otg_set_peripheral -  bind/unbind the peripheral controller driver.
 *
 * @otg: Pointer to the otg_transceiver structure.
 * @gadget: pointer to the usb_gadget structure.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_otg_set_peripheral(struct usb_otg *otg,
				struct usb_gadget *gadget)
{
	struct dwc3_otg *dotg = container_of(otg, struct dwc3_otg, otg);

	if (gadget) {
		dev_dbg(otg->phy->dev, "%s: set gadget %s\n",
					__func__, gadget->name);
		otg->gadget = gadget;
		queue_delayed_work(system_nrt_wq, &dotg->sm_work, 0);
	} else {
		if (otg->phy->state == OTG_STATE_B_PERIPHERAL) {
			dwc3_otg_start_peripheral(otg, 0);
			otg->gadget = NULL;
			otg->phy->state = OTG_STATE_UNDEFINED;
			queue_delayed_work(system_nrt_wq, &dotg->sm_work, 0);
		} else {
			otg->gadget = NULL;
		}
	}

	return 0;
}

/**
 * dwc3_otg_set_suspend -  Set or clear OTG suspend bit and schedule OTG state machine
 * work.
 *
 * @phy: Pointer to the phy structure.
 * @suspend: 1 - Ask OTG state machine to issue low power mode entry.
 *                 0 - Cancel low-power mode entry request.
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_otg_set_suspend(struct usb_phy *phy, int suspend)
{
	const unsigned int lpm_after_suspend_delay = 500;

	struct dwc3_otg *dotg = container_of(phy->otg, struct dwc3_otg, otg);

	if (!dotg->dwc->enable_bus_suspend)
		return 0;

	if (suspend) {
		set_bit(DWC3_OTG_SUSPEND, &dotg->inputs);
		queue_delayed_work(system_nrt_wq,
			&dotg->sm_work,
			msecs_to_jiffies(lpm_after_suspend_delay));
	} else {
		clear_bit(DWC3_OTG_SUSPEND, &dotg->inputs);
	}

	return 0;
}

/**
 * dwc3_ext_chg_det_done - callback to handle charger detection completion
 * @otg: Pointer to the otg transceiver structure
 * @charger: Pointer to the external charger structure
 *
 * Returns 0 on success
 */
static void dwc3_ext_chg_det_done(struct usb_otg *otg, struct dwc3_charger *chg)
{
	struct dwc3_otg *dotg = container_of(otg, struct dwc3_otg, otg);

	/*
	 * Ignore chg_detection notification if BSV has gone off by this time.
	 * STOP chg_det as part of !BSV handling would reset the chg_det flags
	 */
	if (test_bit(B_SESS_VLD, &dotg->inputs))
		queue_delayed_work(system_nrt_wq, &dotg->sm_work, 0);
}

/**
 * dwc3_set_charger - bind/unbind external charger driver
 * @otg: Pointer to the otg transceiver structure
 * @charger: Pointer to the external charger structure
 *
 * Returns 0 on success
 */
int dwc3_set_charger(struct usb_otg *otg, struct dwc3_charger *charger)
{
	struct dwc3_otg *dotg = container_of(otg, struct dwc3_otg, otg);

	dotg->charger = charger;
	if (charger)
		charger->notify_detection_complete = dwc3_ext_chg_det_done;

	return 0;
}

/**
 * dwc3_ext_event_notify - callback to handle events from external transceiver
 * @otg: Pointer to the otg transceiver structure
 * @event: Event reported by transceiver
 *
 * Returns 0 on success
 */
static void dwc3_ext_event_notify(struct usb_otg *otg,
					enum dwc3_ext_events event)
{
	static bool init;
	struct dwc3_otg *dotg = container_of(otg, struct dwc3_otg, otg);
	struct dwc3_ext_xceiv *ext_xceiv = dotg->ext_xceiv;
	struct usb_phy *phy = dotg->otg.phy;
	int ret = 0;

	/* Flush processing any pending events before handling new ones */
	if (init)
		flush_delayed_work(&dotg->sm_work);

	if (event == DWC3_EVENT_PHY_RESUME) {
		if (!pm_runtime_status_suspended(phy->dev))
			dev_warn(phy->dev, "PHY_RESUME event out of LPM!!!!\n");

		dev_dbg(phy->dev, "ext PHY_RESUME event received\n");
		/* ext_xceiver would have taken h/w out of LPM by now */
		ret = pm_runtime_get(phy->dev);
		dbg_event(0xFF, "PhyRes get", ret);
		if (ret == -EACCES) {
			/* pm_runtime_get may fail during system
			   resume with -EACCES error */
			pm_runtime_disable(phy->dev);
			pm_runtime_set_active(phy->dev);
			pm_runtime_enable(phy->dev);
		} else if (ret < 0) {
			dev_warn(phy->dev, "pm_runtime_get failed!\n");
		}
	} else if (event == DWC3_EVENT_XCEIV_STATE) {
		if (pm_runtime_status_suspended(phy->dev) ||
			atomic_read(&phy->dev->power.usage_count) == 0) {
			dev_dbg(phy->dev, "ext XCEIV_STATE while runtime_status=%d\n",
				phy->dev->power.runtime_status);
			ret = pm_runtime_get(phy->dev);
			dbg_event(0xFF, "Xceiv get", ret);
			if (ret < 0)
				dev_warn(phy->dev, "pm_runtime_get failed!!\n");
		}
		if (ext_xceiv->id == DWC3_ID_FLOAT) {
			dev_dbg(phy->dev, "XCVR: ID set\n");
			set_bit(ID, &dotg->inputs);
		} else {
			dev_dbg(phy->dev, "XCVR: ID clear\n");
			clear_bit(ID, &dotg->inputs);
		}

		if (ext_xceiv->bsv) {
			dev_dbg(phy->dev, "XCVR: BSV set\n");
			set_bit(B_SESS_VLD, &dotg->inputs);
		} else {
			dev_dbg(phy->dev, "XCVR: BSV clear\n");
			clear_bit(B_SESS_VLD, &dotg->inputs);
		}

		if (!init) {
			init = true;
			if (!work_busy(&dotg->sm_work.work))
				queue_delayed_work(system_nrt_wq,
							&dotg->sm_work, 0);

			complete(&dotg->dwc3_xcvr_vbus_init);
			dev_dbg(phy->dev, "XCVR: BSV init complete\n");
			return;
		}

		queue_delayed_work(system_nrt_wq, &dotg->sm_work, 0);
	}
}

/**
 * dwc3_set_ext_xceiv - bind/unbind external transceiver driver
 * @otg: Pointer to the otg transceiver structure
 * @ext_xceiv: Pointer to the external transceiver struccture
 *
 * Returns 0 on success
 */
int dwc3_set_ext_xceiv(struct usb_otg *otg, struct dwc3_ext_xceiv *ext_xceiv)
{
	struct dwc3_otg *dotg = container_of(otg, struct dwc3_otg, otg);

	dotg->ext_xceiv = ext_xceiv;
	if (ext_xceiv)
		ext_xceiv->notify_ext_events = dwc3_ext_event_notify;

	return 0;
}

static void dwc3_otg_notify_host_mode(struct usb_otg *otg, int host_mode)
{
	struct dwc3_otg *dotg = container_of(otg, struct dwc3_otg, otg);

	if (!dotg->psy) {
		dev_err(otg->phy->dev, "no usb power supply registered\n");
		return;
	}

	if (host_mode)
		power_supply_set_scope(dotg->psy, POWER_SUPPLY_SCOPE_SYSTEM);
	else
		power_supply_set_scope(dotg->psy, POWER_SUPPLY_SCOPE_DEVICE);
}

static int dwc3_otg_set_power(struct usb_phy *phy, unsigned mA)
{
	enum power_supply_property power_supply_type;
	struct dwc3_otg *dotg = container_of(phy->otg, struct dwc3_otg, otg);


	if (!dotg->psy || !dotg->charger) {
		dev_err(phy->dev, "no usb power supply/charger registered\n");
		return 0;
	}

	if (dotg->charger->charging_disabled)
		return 0;

	if (dotg->charger->chg_type != DWC3_INVALID_CHARGER) {
		dev_dbg(phy->dev,
			"SKIP setting power supply type again,chg_type = %d\n",
			dotg->charger->chg_type);
		goto skip_psy_type;
	}

	if (dotg->charger->chg_type == DWC3_SDP_CHARGER)
		power_supply_type = POWER_SUPPLY_TYPE_USB;
	else if (dotg->charger->chg_type == DWC3_CDP_CHARGER)
		power_supply_type = POWER_SUPPLY_TYPE_USB_CDP;
	else if (dotg->charger->chg_type == DWC3_DCP_CHARGER ||
			dotg->charger->chg_type == DWC3_PROPRIETARY_CHARGER)
		power_supply_type = POWER_SUPPLY_TYPE_USB_DCP;
	else
		power_supply_type = POWER_SUPPLY_TYPE_UNKNOWN;

	power_supply_set_supply_type(dotg->psy, power_supply_type);

skip_psy_type:

	if (dotg->charger->chg_type == DWC3_CDP_CHARGER)
		mA = DWC3_IDEV_CHG_MAX;

	if (dotg->charger->max_power == mA)
		return 0;

	dev_info(phy->dev, "Avail curr from USB = %u\n", mA);

	if (dotg->charger->max_power > 0 && (mA == 0 || mA == 2)) {
		/* Disable charging */
		if (power_supply_set_online(dotg->psy, false))
			goto psy_error;
	} else {
		/* Enable charging */
		if (power_supply_set_online(dotg->psy, true))
			goto psy_error;
	}

	/* Set max current limit in uA */
	if (power_supply_set_current_limit(dotg->psy, 1000*mA))
		goto psy_error;

	power_supply_changed(dotg->psy);
	dotg->charger->max_power = mA;
	return 0;

psy_error:
	dev_dbg(phy->dev, "power supply error when setting property\n");
	return -ENXIO;
}

/**
 * dwc3_otg_init_sm - initialize OTG statemachine input
 * @dotg: Pointer to the dwc3_otg structure
 *
 */
void dwc3_otg_init_sm(struct dwc3_otg *dotg)
{
	struct usb_phy *phy = dotg->otg.phy;
	struct dwc3 *dwc = dotg->dwc;
	int ret;

	/*
	 * VBUS initial state is reported after PMIC
	 * driver initialization. Wait for it.
	 */
	ret = wait_for_completion_timeout(&dotg->dwc3_xcvr_vbus_init, HZ * 5);
	if (!ret) {
		dev_err(phy->dev, "%s: completion timeout\n", __func__);
		/* We can safely assume no cable connected */
		set_bit(ID, &dotg->inputs);
	}

	/*
	 * If vbus-present property was set then set BSV to 1.
	 * This is needed for emulation platforms as PMIC ID
	 * interrupt is not available.
	 */
	if (dwc->vbus_active)
		set_bit(B_SESS_VLD, &dotg->inputs);
}

/**
 * dwc3_otg_sm_work - workqueue function.
 *
 * @w: Pointer to the dwc3 otg workqueue
 *
 * NOTE: After any change in phy->state,
 * we must reschdule the state machine.
 */
static void dwc3_otg_sm_work(struct work_struct *w)
{
	struct dwc3_otg *dotg = container_of(w, struct dwc3_otg, sm_work.work);
	struct usb_phy *phy = dotg->otg.phy;
	struct dwc3_charger *charger = dotg->charger;
	bool work = 0;
	int ret = 0;
	unsigned long delay = 0;

	pm_runtime_resume(phy->dev);
	dev_dbg(phy->dev, "%s state\n", usb_otg_state_string(phy->state));

	/* Check OTG state */
	switch (phy->state) {
	case OTG_STATE_UNDEFINED:
		dwc3_otg_init_sm(dotg);
		if (!dotg->psy) {
			dotg->psy = power_supply_get_by_name("usb");

			if (!dotg->psy)
				dev_err(phy->dev,
					 "couldn't get usb power supply\n");
		}

		/* Switch to A or B-Device according to ID / BSV */
		if (!test_bit(ID, &dotg->inputs)) {
			dev_dbg(phy->dev, "!id\n");
			phy->state = OTG_STATE_A_IDLE;
			work = 1;
		} else if (test_bit(B_SESS_VLD, &dotg->inputs)) {
			dev_dbg(phy->dev, "b_sess_vld\n");
			phy->state = OTG_STATE_B_IDLE;
			work = 1;
		} else {
			phy->state = OTG_STATE_B_IDLE;
			dev_dbg(phy->dev, "No device, trying to suspend\n");
			dbg_event(0xFF, "UNDEF put", 0);
			pm_runtime_put_sync(phy->dev);
		}
		break;

	case OTG_STATE_B_IDLE:
		if (!test_bit(ID, &dotg->inputs)) {
			dev_dbg(phy->dev, "!id\n");
			phy->state = OTG_STATE_A_IDLE;
			work = 1;
			dotg->charger_retry_count = 0;
			if (charger) {
				if (charger->chg_type == DWC3_INVALID_CHARGER)
					charger->start_detection(dotg->charger,
									false);
				else
					charger->chg_type =
							DWC3_INVALID_CHARGER;
			}
		} else if (test_bit(B_SESS_VLD, &dotg->inputs)) {
			dev_dbg(phy->dev, "b_sess_vld\n");
			if (charger) {
				/* Has charger been detected? If no detect it */
				switch (charger->chg_type) {
				case DWC3_DCP_CHARGER:
				case DWC3_PROPRIETARY_CHARGER:
					dev_dbg(phy->dev, "lpm, DCP charger\n");
					dwc3_otg_set_power(phy,
						dcp_max_current);
					dbg_event(0xFF, "PROPCHG put", 0);
					pm_runtime_put_sync(phy->dev);
					break;
				case DWC3_CDP_CHARGER:
					dwc3_otg_set_power(phy,
							DWC3_IDEV_CHG_MAX);
					dwc3_otg_start_peripheral(&dotg->otg,
									1);
					phy->state = OTG_STATE_B_PERIPHERAL;
					work = 1;
					break;
				case DWC3_SDP_CHARGER:
					dwc3_otg_start_peripheral(&dotg->otg,
									1);
					phy->state = OTG_STATE_B_PERIPHERAL;
					work = 1;
					break;
				case DWC3_FLOATED_CHARGER:
					if (dotg->charger_retry_count <
							max_chgr_retry_count)
						dotg->charger_retry_count++;
					/*
					 * In case of floating charger, if
					 * retry count equal to max retry count
					 * notify PMIC about floating charger
					 * and put Hw in low power mode. Else
					 * perform charger detection again by
					 * calling start_detection() with false
					 * and then with true argument.
					 */
					if (dotg->charger_retry_count ==
						max_chgr_retry_count) {
						dwc3_otg_set_power(phy, 0);
						dbg_event(0xFF, "FLCHG put", 0);
						pm_runtime_put_sync(phy->dev);
						break;
					}
					charger->start_detection(dotg->charger,
									false);

				default:
					dev_dbg(phy->dev, "chg_det started\n");
					charger->start_detection(charger, true);
					break;
				}
			} else {
				/*
				 * no charger registered, assuming SDP
				 * and start peripheral
				 */
				phy->state = OTG_STATE_B_PERIPHERAL;
				if (dwc3_otg_start_peripheral(&dotg->otg, 1)) {
					/*
					 * Probably set_peripheral not called
					 * yet. We will re-try as soon as it
					 * will be called
					 */
					dev_err(phy->dev, "enter lpm as\n"
						"unable to start B-device\n");
					phy->state = OTG_STATE_UNDEFINED;
					dbg_event(0xFF, "NoCH put", 0);
					pm_runtime_put_sync(phy->dev);
					return;
				}
			}
		} else {
			if (charger)
				charger->start_detection(dotg->charger, false);

			dotg->charger_retry_count = 0;
			dwc3_otg_set_power(phy, 0);
			dev_dbg(phy->dev, "No device, trying to suspend\n");
			dbg_event(0xFF, "NoDev put", 0);
			pm_runtime_put_sync(phy->dev);
		}
		break;

	case OTG_STATE_B_PERIPHERAL:
		if (!test_bit(B_SESS_VLD, &dotg->inputs) ||
				!test_bit(ID, &dotg->inputs)) {
			dev_dbg(phy->dev, "!id || !bsv\n");
			dwc3_otg_start_peripheral(&dotg->otg, 0);
			phy->state = OTG_STATE_B_IDLE;
			if (charger)
				charger->chg_type = DWC3_INVALID_CHARGER;
			work = 1;
		} else if (test_bit(DWC3_OTG_SUSPEND, &dotg->inputs) &&
			test_bit(B_SESS_VLD, &dotg->inputs)) {
			dbg_event(0xFF, "BPER put", 0);
			pm_runtime_put_sync(phy->dev);
		}
		break;

	case OTG_STATE_A_IDLE:
		/* Switch to A-Device*/
		if (test_bit(ID, &dotg->inputs)) {
			dev_dbg(phy->dev, "id\n");
			phy->state = OTG_STATE_B_IDLE;
			dotg->vbus_retry_count = 0;
			work = 1;
		} else {
			phy->state = OTG_STATE_A_HOST;
			ret = dwc3_otg_start_host(&dotg->otg, 1);
			if ((ret == -EPROBE_DEFER) &&
						dotg->vbus_retry_count < 3) {
				/*
				 * Get regulator failed as regulator driver is
				 * not up yet. Will try to start host after 1sec
				 */
				phy->state = OTG_STATE_A_IDLE;
				dev_dbg(phy->dev, "Unable to get vbus regulator. Retrying...\n");
				delay = VBUS_REG_CHECK_DELAY;
				work = 1;
				dotg->vbus_retry_count++;
			} else if (ret) {
				dev_dbg(phy->dev, "enter lpm as\n"
					"unable to start A-device\n");
				phy->state = OTG_STATE_A_IDLE;
				dbg_event(0xFF, "AIDL put", 0);
				pm_runtime_put_sync(phy->dev);
				return;
			} else {
				/*
				 * delay 1s to allow for xHCI to detect
				 * just-attached devices before allowing
				 * runtime suspend
				 */
				dev_dbg(phy->dev, "a_host state entered\n");
				delay = VBUS_REG_CHECK_DELAY;
				work = 1;
			}
		}
		break;

	case OTG_STATE_A_HOST:
		if (test_bit(ID, &dotg->inputs)) {
			dev_dbg(phy->dev, "id\n");
			dwc3_otg_start_host(&dotg->otg, 0);
			phy->state = OTG_STATE_B_IDLE;
			dotg->vbus_retry_count = 0;
			work = 1;
		} else {
			dev_dbg(phy->dev, "still in a_host state. Resuming root hub.\n");
			dbg_event(0xFF, "AHOST put", 0);
			pm_runtime_resume(&dotg->dwc->xhci->dev);
			pm_runtime_put_noidle(phy->dev);
		}
		break;

	default:
		dev_err(phy->dev, "%s: invalid otg-state\n", __func__);

	}

	if (work)
		queue_delayed_work(system_nrt_wq, &dotg->sm_work, delay);
}

/**
 * dwc3_otg_init - Initializes otg related registers
 * @dwc: Pointer to out controller context structure
 *
 * Returns 0 on success otherwise negative errno.
 */
int dwc3_otg_init(struct dwc3 *dwc)
{
	struct dwc3_otg *dotg;

	dev_dbg(dwc->dev, "dwc3_otg_init\n");

	/* Allocate and init otg instance */
	dotg = devm_kzalloc(dwc->dev, sizeof(struct dwc3_otg), GFP_KERNEL);
	if (!dotg) {
		dev_err(dwc->dev, "unable to allocate dwc3_otg\n");
		return -ENOMEM;
	}

	dotg->otg.phy = devm_kzalloc(dwc->dev, sizeof(struct usb_phy),
							GFP_KERNEL);
	if (!dotg->otg.phy) {
		dev_err(dwc->dev, "unable to allocate dwc3_otg.phy\n");
		return -ENOMEM;
	}

	dotg->otg.phy->otg = &dotg->otg;
	dotg->otg.phy->dev = dwc->dev;
	dotg->otg.phy->set_power = dwc3_otg_set_power;
	dotg->otg.set_peripheral = dwc3_otg_set_peripheral;
	dotg->otg.phy->set_suspend = dwc3_otg_set_suspend;
	dotg->otg.phy->state = OTG_STATE_UNDEFINED;
	dotg->regs = dwc->regs;

	/* This reference is used by dwc3 modules for checking otg existance */
	dwc->dotg = dotg;
	dotg->dwc = dwc;
	dotg->otg.phy->dev = dwc->dev;

	init_completion(&dotg->dwc3_xcvr_vbus_init);
	INIT_DELAYED_WORK(&dotg->sm_work, dwc3_otg_sm_work);

	dbg_event(0xFF, "OTGInit get", 0);
	pm_runtime_get(dwc->dev);

	return 0;
}

/**
 * dwc3_otg_exit
 * @dwc: Pointer to out controller context structure
 *
 * Returns 0 on success otherwise negative errno.
 */
void dwc3_otg_exit(struct dwc3 *dwc)
{
	struct dwc3_otg *dotg = dwc->dotg;

	/* dotg is null when GHWPARAMS6[10]=SRPSupport=0, see dwc3_otg_init */
	if (dotg) {
		if (dotg->charger)
			dotg->charger->start_detection(dotg->charger, false);
		cancel_delayed_work_sync(&dotg->sm_work);
		dbg_event(0xFF, "OTGExit put", 0);
		pm_runtime_put(dwc->dev);
		dwc->dotg = NULL;
	}
}
