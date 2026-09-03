/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _ION_DEBUG_H
#define _ION_DEBUG_H

#include <linux/printk.h>

#define ION_TAG		"sprd-ion:"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) ION_TAG " %s()-" fmt, __func__

#define ION_DEBUG	pr_debug

#define ION_ERR		pr_err

#define ION_INFO	pr_info

#define ION_DUMP	pr_info

#define ION_WARN	pr_warn

#define ION_DEV_DEBUG(dev, fmt, ...) \
	dev_dbg(dev, "%s()-" fmt, __func__, ##__VA_ARGS__)

#define ION_DEV_ERR(dev, fmt, ...) \
	dev_err(dev, "%s()-" fmt, __func__, ##__VA_ARGS__)

#define ION_DEV_INFO(dev, fmt, ...) \
	dev_info(dev, "%s()-" fmt, __func__, ##__VA_ARGS__)

#endif
