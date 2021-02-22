// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2016-2021, The Linux Foundation. All rights reserved. */

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
