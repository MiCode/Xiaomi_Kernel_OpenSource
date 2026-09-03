/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_DEBUG_H
#define _GSP_DEBUG_H

#include <linux/printk.h>

#define GSP_TAG		"sprd-gsp:"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) GSP_TAG fmt

#define GSP_DEBUG	pr_debug

#define GSP_ERR		pr_err

#define GSP_INFO	pr_info

#define GSP_DUMP	pr_info

#define GSP_WARN	pr_warn

#define GSP_DEV_DEBUG(dev, fmt, ...) \
	dev_dbg(dev, fmt, ##__VA_ARGS__)

#define GSP_DEV_ERR(dev, fmt, ...) \
	dev_err(dev, fmt, ##__VA_ARGS__)

#define GSP_DEV_INFO(dev, fmt, ...) \
	dev_info(dev, fmt, ##__VA_ARGS__)

#endif
