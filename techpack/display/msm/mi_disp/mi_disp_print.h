// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_DISP_PRINT_H_
#define _MI_DISP_PRINT_H_

#include <linux/compiler.h>
#include <linux/printk.h>

#include "mi_disp_config.h"
#include "mi_disp_debugfs.h"

#if MI_DISP_PRINT_ENABLE
#define DISP_NAME    "mi_disp"

__printf(2, 3)
void mi_disp_printk(const char *level, const char *format, ...);
__printf(1, 2)
void mi_disp_dbg(const char *format, ...);
__printf(2, 3)
void mi_disp_printk_utc(const char *level, const char *format, ...);
__printf(1, 2)
void mi_disp_dbg_utc(const char *format, ...);

#define DISP_WARN(fmt, ...)     \
		mi_disp_printk(KERN_WARNING, "[warn]" fmt, ##__VA_ARGS__)

#define DISP_INFO(fmt, ...)     \
		mi_disp_printk(KERN_INFO, "[info]" fmt, ##__VA_ARGS__)

#define DISP_ERROR(fmt, ...)    \
		mi_disp_printk(KERN_ERR, "[error]" fmt, ##__VA_ARGS__)

#define DISP_DEBUG(fmt, ...)                               \
	do {                                                   \
		if (is_enable_debug_log())                         \
			mi_disp_dbg("[debug]" fmt, ##__VA_ARGS__);    \
		else                                               \
			pr_debug("[debug]" fmt, ##__VA_ARGS__);       \
	} while (0)

#define DISP_UTC_WARN(fmt, ...)   \
		mi_disp_printk_utc(KERN_WARNING, "[warn]" fmt, ##__VA_ARGS__)
#define DISP_UTC_INFO(fmt, ...)   \
		mi_disp_printk_utc(KERN_INFO, "[info]" fmt, ##__VA_ARGS__)
#define DISP_UTC_ERROR(fmt, ...)  \
		mi_disp_printk_utc(KERN_ERR, "[error]" fmt, ##__VA_ARGS__)
#define DISP_UTC_DEBUG(fmt, ...)                             \
	do {                                                     \
		if (is_enable_debug_log())                           \
			mi_disp_dbg_utc("[debug]" fmt, ##__VA_ARGS__);  \
		else                                                 \
			pr_debug("[debug]" fmt, ##__VA_ARGS__);         \
	} while (0)
#else /* !MI_DISP_PRINT_ENABLE */
#define DISP_WARN(fmt, ...)     \
			printk(KERN_WARNING, fmt, ##__VA_ARGS__)
#define DISP_INFO(fmt, ...)     \
			printk(KERN_INFO, fmt, ##__VA_ARGS__)
#define DISP_ERROR(fmt, ...)    \
			printk(KERN_ERR, fmt, ##__VA_ARGS__)
#define DISP_DEBUG(fmt, ...)    \
			pr_debug(fmt, ##__VA_ARGS__)

#define DISP_UTC_WARN(fmt, ...)   \
			printk(KERN_WARNING, fmt, ##__VA_ARGS__)
#define DISP_UTC_INFO(fmt, ...)   \
			printk(KERN_INFO, fmt, ##__VA_ARGS__)
#define DISP_UTC_ERROR(fmt, ...)  \
			printk(KERN_ERR, fmt, ##__VA_ARGS__)
#define DISP_UTC_DEBUG(fmt, ...)  \
			pr_debug(fmt, ##__VA_ARGS__)
#endif

#endif /* _MI_DISP_PRINT_H_ */
