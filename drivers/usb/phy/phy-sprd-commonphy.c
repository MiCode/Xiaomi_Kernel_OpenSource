// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 Spreadtrum Co., Ltd.
 *		http://www.spreadtrum.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/usb/sprd_commonphy.h>

struct sprd_usbphy_event {
	int max_supported;
	struct raw_notifier_head nh[SPRD_USBPHY_EVENT_MAX];
};

static struct sprd_usbphy_event *usbphy_event_sprd;

/* define our own notifier_call_chain */
int call_sprd_usbphy_event_notifiers(unsigned int id, unsigned long val, void *v)
{
	struct sprd_usbphy_event *usbphy_event = usbphy_event_sprd;
	int ret = 0;

	if (!usbphy_event || id >= SPRD_USBPHY_EVENT_MAX)
		return -EINVAL;

	pr_info("[%s]id(%d),val(%ld)\n", __func__, id, val);
	ret = raw_notifier_call_chain(&usbphy_event->nh[id], val, v);

	return ret;
}
EXPORT_SYMBOL(call_sprd_usbphy_event_notifiers);

/* define our own notifier_chain_register func */
int register_sprd_usbphy_notifier(struct notifier_block *nb, unsigned int id)
{
	struct sprd_usbphy_event *usbphy_event = usbphy_event_sprd;

	if (!usbphy_event || !nb || id >= SPRD_USBPHY_EVENT_MAX)
		return -EINVAL;

	return raw_notifier_chain_register(&usbphy_event->nh[id], nb);
}
EXPORT_SYMBOL(register_sprd_usbphy_notifier);

static int __init sprd_usbphy_event_driver_init(void)
{
	int index;
	struct sprd_usbphy_event *usbphy_event;

	usbphy_event = kzalloc(sizeof(*usbphy_event), GFP_KERNEL);
	if (!usbphy_event)
		return -ENOMEM;

	usbphy_event->max_supported = SPRD_USBPHY_EVENT_MAX;

	for (index = 0; index < usbphy_event->max_supported; index++)
		RAW_INIT_NOTIFIER_HEAD(&usbphy_event->nh[index]);
	usbphy_event_sprd = usbphy_event;

	return 0;
}

static void __exit sprd_usbphy_event_driver_exit(void)
{
	struct sprd_usbphy_event *usbphy_event = usbphy_event_sprd;

	kfree(usbphy_event);
	usbphy_event_sprd = NULL;
}

subsys_initcall(sprd_usbphy_event_driver_init);
module_exit(sprd_usbphy_event_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare SPRD USBPHY_EVENT");
