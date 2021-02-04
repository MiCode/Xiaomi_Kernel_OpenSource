/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __CCCI_DEBUG_H__
#define __CCCI_DEBUG_H__

/* log tag defination */
#define CORE "cor"
#define BM "bfm"
#define FSM "fsm"
#define PORT "pot"
#define NET "net"
#define CHAR "chr"
#define IPC "ipc"
#define RPC "rpc"
#define SYS "sys"
#define SMEM "shm"
#define UDC "udc"

enum {
	CCCI_LOG_ALL_UART = 1,
	CCCI_LOG_ALL_MOBILE,
	CCCI_LOG_CRITICAL_UART,
	CCCI_LOG_CRITICAL_MOBILE,
	CCCI_LOG_ALL_OFF,
};

extern unsigned int ccci_debug_enable; /* Exported by CCCI core */
extern int ccci_log_write(const char *fmt, ...); /* Exported by CCCI Util */

/*****************************************************************************
 ** CCCI dump log define start ****************
 ****************************************************************************/
/*--------------------------------------------------------------------------*/
/* This is used for log to mobile log or uart log */
#define CCCI_LEGACY_DBG_LOG(idx, tag, fmt, args...) \
do { \
	if (ccci_debug_enable == CCCI_LOG_ALL_MOBILE) \
		pr_debug("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == CCCI_LOG_ALL_UART) \
		pr_info("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
} while (0)

#define CCCI_LEGACY_ALWAYS_LOG(idx, tag, fmt, args...) \
do { \
	if (ccci_debug_enable == CCCI_LOG_ALL_MOBILE \
		|| ccci_debug_enable == CCCI_LOG_CRITICAL_MOBILE) \
		pr_debug("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == CCCI_LOG_ALL_UART \
			|| ccci_debug_enable == CCCI_LOG_CRITICAL_UART) \
		pr_info("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
} while (0)

#define CCCI_LEGACY_ERR_LOG(idx, tag, fmt, args...) \
	pr_notice("[ccci%d/" tag "]" fmt, (idx+1), ##args)

/*--------------------------------------------------------------------------*/
/* This log is used for driver init and part of first boot up log */
#define CCCI_INIT_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_INIT, CCCI_DUMP_TIME_FLAG, "[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

/* This log is used for save runtime data */
/* The first line with time stamp */
#define CCCI_BOOTUP_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_BOOTUP, CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_BOOTUP_DUMP_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_BOOTUP, 0, "[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

/* This log is used for modem boot up log and event */
#define CCCI_NORMAL_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_NORMAL, CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_ALWAYS_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_NOTICE_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_NORMAL, CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_ALWAYS_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_ERROR_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_NORMAL, CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_ERR_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_DEBUG_LOG(idx, tag, fmt, args...) \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args)

/* This log is used for periodic log */
#define CCCI_REPEAT_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_REPEAT, CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

/* This log is used for memory dump */
#define CCCI_MEM_LOG_TAG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_MEM_DUMP, CCCI_DUMP_TIME_FLAG|CCCI_DUMP_CURR_FLAG,\
			"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_MEM_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_MEM_DUMP, 0, fmt, ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

/* This log is used for history dump */
#define CCCI_HISTORY_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_HISTORY, 0, fmt, ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)
#define CCCI_HISTORY_TAG_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_HISTORY, \
		CCCI_DUMP_TIME_FLAG, fmt, ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)
#define CCCI_BUF_LOG_TAG(idx, buf_type, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, buf_type, \
		CCCI_DUMP_TIME_FLAG|CCCI_DUMP_CURR_FLAG,\
		"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

/****************************************************************************
 ** CCCI dump log define end ****************
 ****************************************************************************/

/* #define CLDMA_TRACE */
/* #define PORT_NET_TRACE */
#define CCCI_SKB_TRACE
/* #define CCCI_BM_TRACE */

#endif				/* __CCCI_DEBUG_H__ */
