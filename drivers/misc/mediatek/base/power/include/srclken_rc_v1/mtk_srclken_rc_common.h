/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
*/

/**
 * @file    mtk_srclken_rc_common.h
 * @brief   Driver for subys request resource control
 *
 */
#ifndef __MTK_SRCLKEN_RC_COMMON_H__
#define __MTK_SRCLKEN_RC_COMMON_H__

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>

#include <mt-plat/sync_write.h>
#include <mt-plat/upmu_common.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[Power/srclken] " fmt

#define srclken_dbg(fmt, args...)			\
	do {						\
		if (srclken_get_debug_cfg())			\
			pr_info(fmt, ##args);		\
	} while (0)

#define srclken_read(addr)		\
		__raw_readl((void __force __iomem *)(addr))
#define srclken_write(addr, val)	mt_reg_sync_writel(val, addr)

#ifdef CONFIG_PM
#define DEFINE_ATTR_RO(_name)			\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = #_name,			\
		.mode = 0444,			\
	},					\
	.show	= _name##_show,			\
}

#define DEFINE_ATTR_RW(_name)			\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = #_name,			\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

#define __ATTR_OF(_name)	(&_name##_attr.attr)
#endif /* CONFIG_PM */

extern bool is_srclken_initiated;

extern int srclken_fs_init(void);

#endif /*  __MTK_SRCLKEN_RC_COMMON_H__ */

