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

#ifndef __EXTDISP_DRV_LOG_H__
#define __EXTDISP_DRV_LOG_H__
#include "hdmitx.h"

/* /for kernel */
/*#include "extd_drv.h"*/

#define HDMI_ERR(fmt, arg...) \
	do { \
		pr_err("[EXTD]#%d ", __LINE__); \
		pr_err(fmt, ##arg); \
	} while (0)

#define HDMI_LOG(fmt, arg...) \
	do { \
		if (hdmi_log_on) { \
			pr_warn("[EXTD]#%d ", __LINE__); \
			pr_warn(fmt, ##arg); } \
	} while (0)

#define HDMI_FUNC()    \
	do { \
		if (hdmi_log_on) \
			pr_debug("[EXTD] %s\n", __func__); \
	} while (0)

#define HDMI_LINE()    \
	do { \
		if (hdmi_log_on) { \
			pr_debug("[EXTD]%s,%d ", __func__, __LINE__); \
			pr_debug(fmt, ##arg); } \
	} while (0)

#define HDMI_FRC_LOG(fmt, arg...) \
	do { \
		if (hdmi_get_frc_log_level()) { \
			pr_err("[EXTD]"); \
			pr_err(fmt, ##arg); } \
	} while (0)

/*
#define DISP_LOG_PRINT(level, sub_module, fmt, arg...)      \
	do {                                                    \
		xlog_printk(level, "EXTD/"sub_module, fmt, ##arg);  \
	} while (0)

#define LOG_PRINT(level, module, fmt, arg...)               \
	do {                                                    \
		xlog_printk(level, module, fmt, ##arg);             \
	} while (0)
*/
#define DISPMSG(string, args...) pr_warn("[EXTD]"string, ##args)	/* default on, important msg, not err */
#define DISPDBG(string, args...)	/* pr_err("[DISP]"string, ##args)  // default on, important msg, not err */
#define DISPERR(string, args...) pr_err("[EXTD][%s #%d]ERROR:"string, __func__, __LINE__, ##args)
#define DISPFUNC() pr_err("[EXTD]func|%s\n", __func__)	/* default on, err msg */
#define DISPDBGFUNC() DISPDBG("[EXTD]func|%s\n", __func__)	/* default on, err msg */

#define DISPCHECK(string, args...) pr_debug("[EXTD_CHK] #%d "string, __LINE__,  ##args)

#endif				/* __DISP_DRV_LOG_H__ */
