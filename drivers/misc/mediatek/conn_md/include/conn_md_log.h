/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __CONN_MD_LOG_H_
#define __CONN_MD_LOG_H_

#include <linux/module.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/time.h>		/* gettimeofday */
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>

#define DBG_LOG_STR_SIZE 256

extern int g_conn_md_dbg_lvl;

extern int __conn_md_get_log_lvl(void);
extern int conn_md_log_set_lvl(int log_lvl);
extern int __conn_md_log_print(const char *str, ...);

#define CONN_MD_LOG_LOUD    4
#define CONN_MD_LOG_DBG     3
#define CONN_MD_LOG_INFO    2
#define CONN_MD_LOG_WARN    1
#define CONN_MD_LOG_ERR     0

#ifndef DFT_TAG
#define DFT_TAG         "[CONN-MD-DFT]"
#endif

#define CONN_MD_LOUD_FUNC(fmt, arg ...) \
do { \
	if (__conn_md_get_log_lvl() >= CONN_MD_LOG_LOUD) \
		__conn_md_log_print(DFT_TAG "[L]%s:"  fmt, \
		__func__, ## arg); \
} while (0)

#define CONN_MD_INFO_FUNC(fmt, arg ...) \
do { \
	if (__conn_md_get_log_lvl() >= CONN_MD_LOG_INFO)\
		__conn_md_log_print(DFT_TAG "[I]%s:"  fmt, \
		__func__, ## arg); \
} while (0)

#define CONN_MD_WARN_FUNC(fmt, arg ...) \
do { \
	if (__conn_md_get_log_lvl() >= CONN_MD_LOG_WARN)\
		__conn_md_log_print(DFT_TAG "[W]%s:"  fmt, \
		__func__, ## arg); \
} while (0)

#define CONN_MD_ERR_FUNC(fmt, arg ...)\
do {\
	if (__conn_md_get_log_lvl() >= CONN_MD_LOG_ERR)\
		__conn_md_log_print(DFT_TAG "[E]%s(%d):"  fmt,\
		__func__, __LINE__, ## arg);\
} while (0)

#define CONN_MD_DBG_FUNC(fmt, arg ...) \
do { \
	if (__conn_md_get_log_lvl() >= CONN_MD_LOG_DBG) \
		__conn_md_log_print(DFT_TAG "[D]%s:"  fmt, \
		__func__, ## arg); \
} while (0)

#define CONN_MD_TRC_FUNC(f) \
do { \
	if (__conn_md_get_log_lvl() >= CONN_MD_LOG_DBG) \
		__conn_md_log_print(DFT_TAG "<%s> <%d>\n", \
		__func__, __LINE__); \
} while (0)

#endif
