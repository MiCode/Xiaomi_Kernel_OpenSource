// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_DISP_LOG_H_
#define _MI_DISP_LOG_H_

#include "mi_disp_config.h"

#ifdef CONFIG_MI_DISP_LOG
#define DISPLOG_WARN(fmt, ...)  \
			disp_log_printk("[warn]:"fmt, ##__VA_ARGS__)
#define DISPLOG_ERR(fmt, ...)   \
			disp_log_printk("[error]:" fmt, ##__VA_ARGS__)
#define DISPLOG_INFO(fmt, ...)  \
			disp_log_printk("[info]:"fmt, ##__VA_ARGS__)
#define DISPLOG_DEBUG(fmt, ...) \
			disp_log_printk("[debug]:"fmt, ##__VA_ARGS__)

#define DISPLOG_UTC_WARN(fmt, ...)  \
			disp_log_printk_utc("[warn]:"fmt, ##__VA_ARGS__)
#define DISPLOG_UTC_ERR(fmt, ...)   \
			disp_log_printk_utc("[error]:" fmt, ##__VA_ARGS__)
#define DISPLOG_UTC_INFO(fmt, ...)  \
			disp_log_printk_utc("[info]:"fmt, ##__VA_ARGS__)
#define DISPLOG_UTC_DEBUG(fmt, ...) \
			disp_log_printk_utc("[debug]:"fmt, ##__VA_ARGS__)

int mi_disp_log_init(void);
void mi_disp_log_exit(void);
void disp_log_printk(const char *fmt, ...);
void disp_log_printk_utc(const char *format, ...);
#else
static inline int mi_disp_log_init(void) { return 0; }
static inline void mi_disp_log_exit(void) {}
static inline void disp_log_printk(const char *fmt, ...) {}
static inline void disp_log_printk_utc(const char *fmt, ...) {}
#endif

#endif /* _MI_DISPLOG_H_ */

