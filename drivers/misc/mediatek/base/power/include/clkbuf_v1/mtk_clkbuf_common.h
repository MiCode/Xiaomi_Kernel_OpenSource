/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

/**
 * @file    mtk_clk_buf_common.h
 * @brief   Driver for clock buffer control
 *
 */
#ifndef __MTK_CLK_BUF_COMMON_H__
#define __MTK_CLK_BUF_COMMON_H__

#include <linux/init.h>
#include <linux/module.h>
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
#define pr_fmt(fmt) "[Power/clkbuf] " fmt

#define clk_buf_pr_dbg(fmt, args...)			\
	do {						\
		if (clkbuf_debug)			\
			pr_info(fmt, ##args);		\
	} while (0)

#define clkbuf_readl(addr)			__raw_readl(addr)
#define clkbuf_writel(addr, val)	mt_reg_sync_writel(val, addr)

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

extern struct mutex clk_buf_ctrl_lock;
extern bool is_clkbuf_initiated;
extern bool is_pmic_clkbuf;
extern bool clkbuf_debug;
extern unsigned int clkbuf_ctrl_stat;

short is_clkbuf_bringup(void);
extern int clk_buf_dts_map(void);
extern void clk_buf_dump_dts_log(void);
extern int clk_buf_fs_init(void);

#endif

