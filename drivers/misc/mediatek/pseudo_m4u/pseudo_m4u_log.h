/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef PSEUDO_M4U_LOG_H
#define PSEUDO_M4U_LOG_H

#define LOG_LEVEL_HIGH  3
#define LOG_LEVEL_MID   2
#define LOG_LEVEL_LOW   1

extern int m4u_log_level;
extern int m4u_log_to_uart;

#define _M4ULOG(level, string, args...) \
do { \
	if (level > m4u_log_level) { \
		if (level > m4u_log_to_uart) \
			pr_info("[PSEUDO][%s #%d]: "string,	 \
				__func__, __LINE__, ##args); \
		else\
			pr_debug("[PSEUDO][%s #%d]: "string,		\
				__func__, __LINE__, ##args); \
	}  \
} while (0)

#define M4ULOG_LOW(string, args...) _M4ULOG(LOG_LEVEL_LOW, string, ##args)
#define M4ULOG_MID(string, args...) _M4ULOG(LOG_LEVEL_MID, string, ##args)
#define M4ULOG_HIGH(string, args...) _M4ULOG(LOG_LEVEL_HIGH, string, ##args)

#define M4U_ERR(string, args...) _M4ULOG(LOG_LEVEL_HIGH, string, ##args)
#define M4U_MSG(string, args...) _M4ULOG(LOG_LEVEL_HIGH, string, ##args)
#define M4U_INFO(string, args...) _M4ULOG(LOG_LEVEL_MID, string, ##args)
#define M4U_DBG(string, args...) _M4ULOG(LOG_LEVEL_LOW, string, ##args)

#define M4UTRACE() \
do { \
	if (!m4u_log_to_uart) \
		pr_info("[PSEUDO] %s, %d\n", __func__, __LINE__); \
} while (0)

#define M4U_PRINT_SEQ(seq_file, fmt, args...) \
	do {\
		if (seq_file)\
			seq_printf(seq_file, fmt, ##args);\
		else\
			pr_notice(fmt, ##args);\
	} while (0)

#endif
