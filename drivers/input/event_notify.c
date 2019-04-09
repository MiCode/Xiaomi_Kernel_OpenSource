// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#include <linux/export.h>
#include <linux/notifier.h>
#include <linux/input/touch_event_notify.h>

static BLOCKING_NOTIFIER_HEAD(touch_notifier_list);

int touch_event_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&touch_notifier_list, nb);
}
EXPORT_SYMBOL(touch_event_register_notifier);

int touch_event_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&touch_notifier_list, nb);
}
EXPORT_SYMBOL(touch_event_unregister_notifier);

void touch_event_call_notifier(unsigned long action, void *data)
{
	blocking_notifier_call_chain(&touch_notifier_list, action, data);
}
EXPORT_SYMBOL(touch_event_call_notifier);

