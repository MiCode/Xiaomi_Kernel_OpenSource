/* Copyright (c) 2016, Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/extcon.h>
#include "usbpd.h"

struct usbpd {
	struct device		dev;
	struct workqueue_struct	*wq;
	struct work_struct	sm_work;

	struct extcon_dev	*extcon;
	struct power_supply	*usb_psy;
	struct notifier_block	psy_nb;

	enum power_supply_typec_mode typec_mode;
	enum power_supply_type	psy_type;

	enum data_role		current_dr;
	enum power_role		current_pr;

	struct list_head	instance;
};

static LIST_HEAD(_usbpd);	/* useful for debugging */

static const unsigned int usbpd_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

static const u32 usbpd_extcon_exclusive[] = {0xffffffff, 0};

static void usbpd_sm(struct work_struct *w)
{
	struct usbpd *pd = container_of(w, struct usbpd, sm_work);

	if (pd->current_pr == PR_NONE) {
		if (pd->current_dr == DR_UFP)
			extcon_set_cable_state_(pd->extcon, EXTCON_USB, 0);
		else if (pd->current_dr == DR_DFP)
			extcon_set_cable_state_(pd->extcon, EXTCON_USB_HOST, 0);

		pd->current_dr = DR_NONE;
	} else if (pd->current_pr == PR_SINK && pd->current_dr == DR_NONE) {
		pd->current_dr = DR_UFP;

		if (pd->psy_type == POWER_SUPPLY_TYPE_USB ||
				pd->psy_type == POWER_SUPPLY_TYPE_USB_CDP)
			extcon_set_cable_state_(pd->extcon, EXTCON_USB, 1);
	} else if (pd->current_pr == PR_SRC && pd->current_dr == DR_NONE) {
		pd->current_dr = DR_DFP;
		extcon_set_cable_state_(pd->extcon, EXTCON_USB_HOST, 1);
	}
}

static int psy_changed(struct notifier_block *nb, unsigned long evt, void *ptr)
{
	struct usbpd *pd = container_of(nb, struct usbpd, psy_nb);
	union power_supply_propval val;
	bool pd_allowed;
	enum power_supply_typec_mode typec_mode;
	enum power_supply_type psy_type;
	int ret;

	if (ptr != pd->usb_psy || evt != PSY_EVENT_PROP_CHANGED)
		return 0;

	ret = power_supply_get_property(pd->usb_psy,
			POWER_SUPPLY_PROP_PD_ALLOWED, &val);
	if (ret) {
		dev_err(&pd->dev, "Unable to read USB PROP_PD_ALLOWED: %d\n",
				ret);
		return ret;
	}

	pd_allowed = val.intval;

	ret = power_supply_get_property(pd->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_MODE, &val);
	if (ret) {
		dev_err(&pd->dev, "Unable to read USB TYPEC_MODE: %d\n", ret);
		return ret;
	}

	/* don't proceed if PD_ALLOWED is false */
	if (!pd_allowed && val.intval != POWER_SUPPLY_TYPEC_NONE)
		return 0;

	typec_mode = val.intval;

	ret = power_supply_get_property(pd->usb_psy,
			POWER_SUPPLY_PROP_TYPE, &val);
	if (ret) {
		dev_err(&pd->dev, "Unable to read USB TYPE: %d\n", ret);
		return ret;
	}

	psy_type = val.intval;

	dev_dbg(&pd->dev, "typec mode:%d present:%d type:%d\n", typec_mode,
			pd->vbus_present, psy_type);

	/* any change? */
	if (pd->typec_mode == typec_mode && pd->psy_type == psy_type)
		return 0;

	pd->typec_mode = typec_mode;
	pd->psy_type = psy_type;

	switch (typec_mode) {
	/* Disconnect */
	case POWER_SUPPLY_TYPEC_NONE:
		pd->current_pr = PR_NONE;
		queue_work(pd->wq, &pd->sm_work);
		break;

	/* Sink states */
	case POWER_SUPPLY_TYPEC_SOURCE_DEFAULT:
	case POWER_SUPPLY_TYPEC_SOURCE_MEDIUM:
	case POWER_SUPPLY_TYPEC_SOURCE_HIGH:
		dev_info(&pd->dev, "Type-C Source connected\n");
		pd->current_pr = PR_SINK;
		queue_work(pd->wq, &pd->sm_work);
		break;

	/* Source states */
	case POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE:
	case POWER_SUPPLY_TYPEC_SINK:
		dev_info(&pd->dev, "Type-C Sink connected\n");
		pd->current_pr = PR_SRC;
		queue_work(pd->wq, &pd->sm_work);
		break;

	case POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY:
		dev_info(&pd->dev, "Type-C Debug Accessory connected\n");
		break;
	case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
		dev_info(&pd->dev, "Type-C Analog Audio Adapter connected\n");
		break;
	default:
		dev_warn(&pd->dev, "Unsupported typec mode: %d\n", typec_mode);
		break;
	}

	return 0;
}

static struct class usbpd_class = {
	.name = "usbpd",
	.owner = THIS_MODULE,
};

static int num_pd_instances;

/**
 * usbpd_create - Create a new instance of USB PD protocol/policy engine
 * @parent - parent device to associate with
 *
 * This creates a new usbpd class device which manages the state of a
 * USB PD-capable port. The parent device that is passed in should be
 * associated with the physical device port, e.g. a PD PHY.
 *
 * Return: struct usbpd pointer, or an ERR_PTR value
 */
struct usbpd *usbpd_create(struct device *parent)
{
	int ret;
	struct usbpd *pd;

	pd = kzalloc(sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return ERR_PTR(-ENOMEM);

	device_initialize(&pd->dev);
	pd->dev.class = &usbpd_class;
	pd->dev.parent = parent;
	dev_set_drvdata(&pd->dev, pd);

	ret = dev_set_name(&pd->dev, "usbpd%d", num_pd_instances++);
	if (ret)
		goto free_pd;

	ret = device_add(&pd->dev);
	if (ret)
		goto free_pd;

	pd->wq = alloc_ordered_workqueue("usbpd_wq", WQ_FREEZABLE);
	if (!pd->wq) {
		ret = -ENOMEM;
		goto del_pd;
	}
	INIT_WORK(&pd->sm_work, usbpd_sm);

	pd->usb_psy = power_supply_get_by_name("usb");
	if (!pd->usb_psy) {
		dev_dbg(&pd->dev, "Could not get USB power_supply, deferring probe\n");
		ret = -EPROBE_DEFER;
		goto destroy_wq;
	}

	pd->psy_nb.notifier_call = psy_changed;
	ret = power_supply_reg_notifier(&pd->psy_nb);
	if (ret)
		goto put_psy;

	/*
	 * associate extcon with the parent dev as it could have a DT
	 * node which will be useful for extcon_get_edev_by_phandle()
	 */
	pd->extcon = devm_extcon_dev_allocate(parent, usbpd_extcon_cable);
	if (IS_ERR(pd->extcon)) {
		dev_err(&pd->dev, "failed to allocate extcon device\n");
		ret = PTR_ERR(pd->extcon);
		goto unreg_psy;
	}

	pd->extcon->mutually_exclusive = usbpd_extcon_exclusive;
	ret = devm_extcon_dev_register(parent, pd->extcon);
	if (ret) {
		dev_err(&pd->dev, "failed to register extcon device\n");
		goto unreg_psy;
	}

	pd->current_pr = PR_NONE;
	pd->current_dr = DR_NONE;
	list_add_tail(&pd->instance, &_usbpd);

	/* force read initial power_supply values */
	psy_changed(&pd->psy_nb, PSY_EVENT_PROP_CHANGED, pd->usb_psy);

	return pd;

unreg_psy:
	power_supply_unreg_notifier(&pd->psy_nb);
put_psy:
	power_supply_put(pd->usb_psy);
destroy_wq:
	destroy_workqueue(pd->wq);
del_pd:
	device_del(&pd->dev);
free_pd:
	num_pd_instances--;
	kfree(pd);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(usbpd_create);

/**
 * usbpd_destroy - Removes and frees a usbpd instance
 * @pd: the instance to destroy
 */
void usbpd_destroy(struct usbpd *pd)
{
	if (!pd)
		return;

	list_del(&pd->instance);
	power_supply_unreg_notifier(&pd->psy_nb);
	power_supply_put(pd->usb_psy);
	destroy_workqueue(pd->wq);
	device_del(&pd->dev);
	kfree(pd);
}
EXPORT_SYMBOL(usbpd_destroy);

static int __init usbpd_init(void)
{
	return class_register(&usbpd_class);
}
module_init(usbpd_init);

static void __exit usbpd_exit(void)
{
	class_unregister(&usbpd_class);
}
module_exit(usbpd_exit);

MODULE_DESCRIPTION("USB Power Delivery Policy Engine");
MODULE_LICENSE("GPL v2");
