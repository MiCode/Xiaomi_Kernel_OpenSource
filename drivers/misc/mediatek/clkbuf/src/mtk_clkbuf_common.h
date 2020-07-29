/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/**
 * @file    mtk_clk_buf_common.h
 * @brief   Driver for clock buffer control
 *
 */
#ifndef __MTK_CLK_BUF_COMMON_H__
#define __MTK_CLK_BUF_COMMON_H__

#include <linux/sysfs.h>
#include <linux/kobject.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[Power/clkbuf] " fmt

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

#define NOT_VALID		0xffff

/* #define CLKBUF_DEBUG */

extern struct mutex clk_buf_ctrl_lock;

#endif

