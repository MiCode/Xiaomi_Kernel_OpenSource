/*
 *  linux/drivers/video/fb_notify.c
 *
 *  Copyright (C) 2006 Antonino Daplas <adaplas@pol.net>
 *  Copyright (C) 2016 XiaoMi, Inc.
 *
 *	2001 - Documented with DocBook
 *	- Brad Douglas <brad@neruo.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/export.h>

static BLOCKING_NOTIFIER_HEAD(fb_notifier_list);

/**
 *	fb_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int fb_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&fb_notifier_list, nb);
}
EXPORT_SYMBOL(fb_register_client);

/**
 *	fb_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int fb_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&fb_notifier_list, nb);
}
EXPORT_SYMBOL(fb_unregister_client);

/**
 * fb_notifier_call_chain - notify clients of fb_events
 *
 */
int fb_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&fb_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(fb_notifier_call_chain);

static BLOCKING_NOTIFIER_HEAD(tp_notifier_list);

/**
 *	tp_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int tp_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&tp_notifier_list, nb);
}
EXPORT_SYMBOL(tp_register_client);

/**
 *	tp_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int tp_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&tp_notifier_list, nb);
}
EXPORT_SYMBOL(tp_unregister_client);

/**
 * tp_notifier_call_chain - notify clients of tp_events
 *
 */
int tp_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&tp_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(tp_notifier_call_chain);
