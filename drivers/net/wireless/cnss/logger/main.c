/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <net/cnss_logger.h>
#include "logger.h"

static struct logger_context *ctx;

struct logger_context *logger_get_ctx(void)
{
	return ctx;
}

static int __init logger_module_init(void)
{
	int ret;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ret = logger_netlink_init(ctx);

	mutex_init(&ctx->con_mutex);
	spin_lock_init(&ctx->data_lock);
	logger_debugfs_init(ctx);

	return ret;
}

static void __exit logger_module_exit(void)
{
	logger_debugfs_remove(ctx);
	logger_netlink_deinit(ctx);

	kfree(ctx);
}

module_init(logger_module_init);
module_exit(logger_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CNSS Logging Service Driver");
