/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/notifier.h>
#include <linux/export.h>
#include "dsi_notify.h"

static BLOCKING_NOTIFIER_HEAD(dsi_panel_notifier_list);

/**
 *	dsi_panel_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int dsi_panel_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&dsi_panel_notifier_list, nb);
}
EXPORT_SYMBOL(dsi_panel_register_client);

/**
 *	dsi_panel_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int dsi_panel_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&dsi_panel_notifier_list, nb);
}
EXPORT_SYMBOL(dsi_panel_unregister_client);

/**
 * dsi_panel_notifier_call_chain - notify clients of fb_events
 *
 */
int dsi_panel_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&dsi_panel_notifier_list, val, v);
}
EXPORT_SYMBOL(dsi_panel_notifier_call_chain);
