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
#define pr_fmt(fmt) "zone_movable_cma: " fmt
#include <linux/notifier.h>

static BLOCKING_NOTIFIER_HEAD(zmc_notifier_list);

/**
 *	zmc_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int zmc_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&zmc_notifier_list, nb);
}

/**
 *	zmc_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int zmc_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&zmc_notifier_list, nb);
}

/**
 * zmc_notifier_call_chain - notify clients
 *
 */
int zmc_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&zmc_notifier_list, val, v);
}
