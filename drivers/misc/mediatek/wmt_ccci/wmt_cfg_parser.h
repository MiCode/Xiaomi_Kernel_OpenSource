/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifndef _WMT_CFG_PARSER_H_
#define _WMT_CFG_PARSER_H_

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/kthread.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/err.h>

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[WMT-CCCI]"

#define WMT_CCCI_LOG_LOUD                 4
#define WMT_CCCI_LOG_DBG                  3
#define WMT_CCCI_LOG_INFO                 2
#define WMT_CCCI_LOG_WARN                 1
#define WMT_CCCI_LOG_ERR                  0

extern unsigned int wmtCcciLogLvl;

#define WMT_CCCI_LOUD_FUNC(fmt, arg...) \
do { \
	if (wmtCcciLogLvl >= WMT_CCCI_LOG_LOUD) \
		pr_debug(DFT_TAG "[L]%s:"  fmt, __func__ , ##arg); \
} while (0)
#define WMT_CCCI_INFO_FUNC(fmt, arg...) \
do { \
	if (wmtCcciLogLvl >= WMT_CCCI_LOG_INFO) \
		pr_debug(DFT_TAG "[I]%s:"  fmt, __func__ , ##arg); \
} while (0)
#define WMT_CCCI_WARN_FUNC(fmt, arg...) \
do { \
	if (wmtCcciLogLvl >= WMT_CCCI_LOG_WARN) \
		pr_warn(DFT_TAG "[W]%s:"  fmt, __func__ , ##arg); \
} while (0)
#define WMT_CCCI_ERR_FUNC(fmt, arg...) \
do { \
	if (wmtCcciLogLvl >= WMT_CCCI_LOG_ERR) \
		pr_err(DFT_TAG "[E]%s(%d):"  fmt, __func__ , __LINE__, ##arg); \
} while (0)
#define WMT_CCCI_DBG_FUNC(fmt, arg...) \
do { \
	if (wmtCcciLogLvl >= WMT_CCCI_LOG_DBG) \
		pr_debug(DFT_TAG "[D]%s:"  fmt, __func__ , ##arg); \
} while (0)

#define wmt_ccci_assert(condition) \
do { \
	if (!(condition)) \
		pr_err(DFT_TAG "%s, %d, (%s)\n", __FILE__, __LINE__, #condition); \
} while (0)

#ifndef NAME_MAX
#define NAME_MAX 256
#endif

#define WMT_CFG_FILE "WMT_SOC.cfg"
#define WMT_CFG_FILE_PREFIX "/system/etc/firmware/"

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

typedef struct _WMT_CONF_FILE_ {
	unsigned char cfgExist;

	unsigned char coex_wmt_ant_mode;

	/*GPS LNA setting */
	unsigned char wmt_gps_lna_pin;
	unsigned char wmt_gps_lna_enable;

	/*GPS co-clock setting */
	unsigned int co_clock_flag;

} WMT_CONF_FILE, *P_WMT_CONF_FILE;

typedef struct _WMT_PARSER_CONF_FOR_CCCI_ {
	WMT_CONF_FILE rWmtCfgFile;
	unsigned char cWmtCfgName[NAME_MAX + 1];
	const struct firmware *pWmtCfg;

} WMT_PARSER_CONF_FOR_CCCI, *P_WMT_PARSER_CONF_FOR_CCCI;

#endif
