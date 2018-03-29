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

#ifndef __DISP_DRV_LOG_H__
#define __DISP_DRV_LOG_H__
#include "../dispsys/display_recorder.h"
    /* /for kernel */
/*
#define DISP_LOG_PRINT(level, sub_module, fmt, arg...)      \
	do {                                                    \
	    xlog_printk(level, "DISP/"sub_module, fmt, ##arg);  \
	} while (0)

#define LOG_PRINT(level, module, fmt, arg...)               \
	do {                                                    \
	    xlog_printk(level, module, fmt, ##arg);             \
	} while (0)
*/
extern unsigned int dprec_error_log_len;
extern unsigned int dprec_error_log_id;
extern unsigned int dprec_error_log_buflen;
extern char dprec_error_log_buffer[];
extern unsigned int gEnableFenceLog;

#define DEBUG

#define DISPPR(string, args...) \
	do {\
		dprec_error_log_len = scnprintf(dprec_error_log_buffer, dprec_error_log_buflen, string, ##args); \
		dprec_logger_pr(DPREC_LOGGER_HWOP, dprec_error_log_buffer);\
	} while (0)
#define DISPPR_HWOP(string, args...)	dprec_logger_pr(DPREC_LOGGER_HWOP, string, ##args)
#define DISPPR_ERROR(string, args...)  \
	do { \
		dprec_logger_pr(DPREC_LOGGER_ERROR, string, ##args); \
		pr_err("[DISP][%s #%d]ERROR:"string, __func__, __LINE__, ##args); \
	} while (0)
#define DISPPR_FENCE(string, args...) \
	do { \
		dprec_logger_pr(DPREC_LOGGER_FENCE, string, ##args); \
		if (1 == gEnableFenceLog) \
			pr_debug("[DISP][%s #%d]"string, __func__, __LINE__, ##args); \
	} while (0)

#define DISPMSG(string, args...) pr_warn("[DISP]"string, ##args)
#define DISPDBG(string, args...) pr_debug("[DISP]"string, ##args)
#define DISPERR	DISPPR_ERROR

#define DISPFUNC() pr_err("[DISP]func|%s\n", __func__)
#define DISPDBGFUNC() DISPDBG("[DISP]func|%s\n", __func__)
#define DISPCHECK(string, args...) pr_debug("[DISPCHECK]"string, ##args)

#endif				/* __DISP_DRV_LOG_H__ */
