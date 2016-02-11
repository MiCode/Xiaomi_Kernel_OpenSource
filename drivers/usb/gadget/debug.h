/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DEBUG_H_
#define __DEBUG_H_

#define DBG_MAX_MSG   1024UL
#define DBG_MSG_LEN   80UL
#define TIME_BUF_LEN  17
#define DBG_EVENT_LEN  (DBG_MSG_LEN - TIME_BUF_LEN)

extern unsigned int enable_event_log;
extern void put_timestamp(char *tbuf);
extern void add_event_to_buf(char *tbuf);
extern int debug_debugfs_init(void);
extern void debug_debugfs_exit(void);

#define LOGLEVEL_NONE 8
#define LOGLEVEL_DEBUG 7
#define LOGLEVEL_INFO 6
#define LOGLEVEL_ERR 3

#define log_event(log_level, x...)					\
do {									\
	char buf[DBG_MSG_LEN];						\
	if (log_level == LOGLEVEL_DEBUG)				\
		pr_debug(x);						\
	else if (log_level == LOGLEVEL_ERR)				\
		pr_err(x);						\
	else if (log_level == LOGLEVEL_INFO)				\
		pr_info(x);						\
	if (enable_event_log) {						\
		put_timestamp(buf);					\
		snprintf(&buf[TIME_BUF_LEN - 1], DBG_EVENT_LEN, x);	\
		add_event_to_buf(buf);					\
	}								\
} while (0)

#define log_event_none(x, ...) log_event(LOGLEVEL_NONE, x, ##__VA_ARGS__)
#define log_event_dbg(x, ...) log_event(LOGLEVEL_DEBUG, x, ##__VA_ARGS__)
#define log_event_err(x, ...) log_event(LOGLEVEL_ERR, x, ##__VA_ARGS__)
#define log_event_info(x, ...) log_event(LOGLEVEL_INFO, x, ##__VA_ARGS__)

#endif	/* __DEBUG_H_ */
