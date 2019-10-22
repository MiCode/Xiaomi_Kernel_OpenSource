/*
 * OKL4 Debug panic handler
 *
 * Copyright (c) 2017 Cog Systems Pty Ltd.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This registers a notifier that can trigger a KDB entry on panic, if enabled
 * on the kernel command line and running under a debug OKL4 kernel.
 */

#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/init.h>

#include <microvisor/microvisor.h>

static int panic_kdb_interact(struct notifier_block *this, unsigned long event,
	 void *ptr)
{
	_okl4_sys_kdb_interact();

	return NOTIFY_DONE;
}

static struct notifier_block panic_block = {
	.notifier_call	= panic_kdb_interact,
};

static int __init setup_okl4_panic_kdb(char *buf)
{
	atomic_notifier_chain_register(&panic_notifier_list, &panic_block);
	return 0;
}
early_param("okl4_panic_kdb", setup_okl4_panic_kdb);
