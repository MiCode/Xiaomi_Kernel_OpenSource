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
#include <linux/of_gpio.h>
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
	struct device_node *node = dwc->dev->parent->of_node;
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

		pm_runtime_get_sync(otg->phy->dev);
		dbg_event(0xFF, "StrtHost gync",
			atomic_read(&otg->phy->dev->power.usage_count));
		dwc3_otg_notify_host_mode(otg, on);
		usb_phy_notify_connect(dotg->dwc->usb2_phy, USB_SPEED_HIGH);
		ret = regulator_enable(dotg->vbus_otg);
		if (ret) {
			dev_err(otg->phy->dev, "unable to enable vbus_otg\n");
			dwc3_otg_notify_host_mode(otg, 0);
			pm_runtime_put_sync(otg->phy->dev);
			dbg_event(0xFF, "vregerr psync",
				atomic_read(&otg->phy->dev->power.usage_count));
			return ret;
		}

		dotg->cpe_gpio = of_get_named_gpio(node, "qcom,cpe-gpio", 0);
		if (dotg->cpe_gpio < 0) {
			dotg->cpe_gpio = 0;
			dev_dbg(otg->phy->dev, "Error getting CPE GPIO");
		} else {
			ret = devm_gpio_request(dwc->dev->parent,
					dotg->cpe_gpio, "cpe-gpio");
			if (ret)
				dev_dbg(otg->phy->dev, "Error requesting CPE GPIO");

			ret = gpio_direction_output(dotg->cpe_gpio, 1);
			if (ret)
				dev_dbg(otg->phy->dev, "Error setting direction for CPE GPIO");
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
			pm_runtime_put_sync(otg->phy->dev);
			dbg_event(0xFF, "pdeverr psync",
				atomic_read(&otg->phy->dev->power.usage_count));
			return ret;
		}

		hcd = platform_get_drvdata(dwc->xhci);
		otg->host = &hcd->self;

		dwc3_gadget_usb3_phy_suspend(dwc, true);

		/* xHCI should have incremented child count as necessary */
		pm_runtime_put_sync(otg->phy->dev);
		dbg_event(0xFF, "StrtHost psync",
			atomic_read(&otg->phy->dev->power.usage_count));
	} else {
		dev_dbg(otg->phy->dev, "%s: turn off host\n", __func__);

		ret = regulator_disable(dotg->vbus_otg);
		if (ret) {
			dev_err(otg->phy->dev, "unable to disable vbus_otg\n");
			return ret;
		}

		if (dotg->cpe_gpio) {
			ret = gpio_direction_output(dotg->cpe_gpio, 0);
			if (ret)
				dev_dbg(otg->phy->dev, "Error setting direction for CPE GPIO");
		}

		pm_runtime_get_sync(dwc->dev);
		dbg_event(0xFF, "StopHost gsync",
			atomic_read(&dwc->dev->power.usage_count));
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
		pm_runtime_put_sync(dwc->dev);
		dbg_event(0xFF, "StopHost psync",
			atomic_read(&dwc->dev->power.usage_count));
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

	pm_runtime_get_sync(otg->phy->dev);
	dbg_event(0xFF, "StrtGdgt gsync",
		atomic_read(&otg->phy->dev->power.usage_count));

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

	pm_runtime_put_sync(otg->phy->dev);
	dbg_event(0xFF, "StopGdgt psync",
		atomic_read(&otg->phy->dev->power.usage_count));

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

	/* ensure OTG work is finished before returning */
	flush_delayed_work(&dotg->sm_work);
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
static void dwc3_ext_event_notify(struct usb_otg *otg)
{
	static bool init;
	struct dwc3_otg *dotg = container_of(otg, struct dwc3_otg, otg);
	struct dwc3_ext_xceiv *ext_xceiv = dotg->ext_xceiv;
	struct usb_phy *phy = dotg->otg.phy;

	/* Flush processing any pending events before handling new ones */
	if (init)
		flush_delayed_work(&dotg->sm_work);

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

	if (ext_xceiv->suspend) {
		dev_dbg(phy->dev, "XCVR: SUSP set\n");
		set_bit(B_SUSPEND, &dotg->inputs);
	} else {
		dev_dbg(phy->dev, "XCVR: SUSP clear\n");
		clear_bit(B_SUSPEND, &dotg->inputs);
	}

	if (!init) {
		init = true;
		if (!work_busy(&dotg->sm_work.work))
			queue_delayed_work(system_nrt_wq, &dotg->sm_work, 0);

		complete(&dotg->dwc3_xcvr_vbus_init);
		dev_dbg(phy->dev, "XCVR: BSV init complete\n");
		return;
	}

	queue_delayed_work(system_nrt_wq, &dotg->sm_work, 0);
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
		} else {
			phy->state = OTG_STATE_B_IDLE;
			if (test_bit(B_SESS_VLD, &dotg->inputs)) {
				dev_dbg(phy->dev, "b_sess_vld\n");
				work = 1;
			}
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
					break;
				case DWC3_CDP_CHARGER:
					dwc3_otg_set_power(phy,
							DWC3_IDEV_CHG_MAX);
					/* fall through */
				case DWC3_SDP_CHARGER:
					/*
					 * Increment pm usage count upon cable
					 * connect. Count is decremented in
					 * OTG_STATE_B_PERIPHERAL state on cable
					 * disconnect or in bus suspend.
					 */
					pm_runtime_get_sync(phy->dev);
					dbg_event(0xFF, "CHG gsync",
					atomic_read(
						&phy->dev->power.usage_count));
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
				 * No charger registered, assuming SDP
				 * and start peripheral
				 */
				phy->state = OTG_STATE_B_PERIPHERAL;
				/*
				 * Increment pm usage count upon cable connect.
				 * Count is decremented in
				 * OTG_STATE_B_PERIPHERAL state on cable
				 * disconnect or in bus suspend.
				 */
				pm_runtime_get_sync(phy->dev);
				dbg_event(0xFF,
					"NoCHG gsync",
					atomic_read(
						&phy->dev->power.usage_count));
				if (dwc3_otg_start_peripheral(&dotg->otg, 1)) {
					pm_runtime_put_sync(phy->dev);
					dbg_event(0xFF,
						"NoChg psync",
						atomic_read(
						&phy->dev->power.usage_count));
					/*
					 * Probably set_peripheral not called
					 * yet. We will re-try as soon as it
					 * will be called
					 */
					dev_err(phy->dev, "unable to start B-device\n");
					phy->state = OTG_STATE_UNDEFINED;
					return;
				}
			}
		} else {
			if (charger)
				charger->start_detection(dotg->charger, false);

			dotg->charger_retry_count = 0;
			dwc3_otg_set_power(phy, 0);
			dev_dbg(phy->dev, "No device, allowing suspend\n");
		}
		break;

	case OTG_STATE_B_PERIPHERAL:
		if (!test_bit(B_SESS_VLD, &dotg->inputs) ||
				!test_bit(ID, &dotg->inputs)) {
			dev_dbg(phy->dev, "!id || !bsv\n");
			phy->state = OTG_STATE_B_IDLE;
			dwc3_otg_start_peripheral(&dotg->otg, 0);
			/*
			 * Decrement pm usage count upon cable disconnect
			 * which was incremented upon cable connect in
			 * OTG_STATE_B_IDLE state
			 */
			pm_runtime_put_sync(phy->dev);
			dbg_event(0xFF, "BPER psync",
				atomic_read(&phy->dev->power.usage_count));
			if (charger)
				charger->chg_type = DWC3_INVALID_CHARGER;
			work = 1;
		} else if (test_bit(B_SUSPEND, &dotg->inputs) &&
			test_bit(B_SESS_VLD, &dotg->inputs)) {
			dev_dbg(phy->dev, "BPER bsv && susp\n");
			phy->state = OTG_STATE_B_SUSPEND;
			/*
			 * Decrement pm usage count upon bus suspend.
			 * Count was incremented either upon cable
			 * connect in OTG_STATE_B_IDLE or host
			 * initiated resume after bus suspend in
			 * OTG_STATE_B_SUSPEND state
			 */
			pm_runtime_mark_last_busy(phy->dev);
			pm_runtime_put_autosuspend(phy->dev);
			dbg_event(0xFF, "SUSP put",
				atomic_read(&phy->dev->power.usage_count));
		}
		break;

	case OTG_STATE_B_SUSPEND:
		if (!test_bit(B_SESS_VLD, &dotg->inputs)) {
			dev_dbg(phy->dev, "BSUSP: !bsv\n");
			phy->state = OTG_STATE_B_IDLE;
			dwc3_otg_start_peripheral(&dotg->otg, 0);
		} else if (!test_bit(B_SUSPEND, &dotg->inputs)) {
			dev_dbg(phy->dev, "BSUSP !susp\n");
			phy->state = OTG_STATE_B_PERIPHERAL;
			/*
			 * Increment pm usage count upon host
			 * initiated resume. Count was decremented
			 * upon bus suspend in
			 * OTG_STATE_B_PERIPHERAL state.
			 */
			pm_runtime_get_sync(phy->dev);
			dbg_event(0xFF, "SUSP gsync",
				atomic_read(&phy->dev->power.usage_count));
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
				dev_err(phy->dev, "unable to start host\n");
				phy->state = OTG_STATE_A_IDLE;
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
			dbg_event(0xFF, "XHCIResume", 0);
			pm_runtime_resume(&dotg->dwc->xhci->dev);
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
	dotg->otg.phy->state = OTG_STATE_UNDEFINED;
	dotg->regs = dwc->regs;

	/* This reference is used by dwc3 modules for checking otg existance */
	dwc->dotg = dotg;
	dotg->dwc = dwc;
	dotg->otg.phy->dev = dwc->dev;

	init_completion(&dotg->dwc3_xcvr_vbus_init);
	INIT_DELAYED_WORK(&dotg->sm_work, dwc3_otg_sm_work);

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
		dwc->dotg = NULL;
	}
}
