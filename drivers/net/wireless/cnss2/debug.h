/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CNSS_DEBUG_H
#define _CNSS_DEBUG_H

#include <linux/ipc_logging.h>
#include <linux/printk.h>

#define CNSS_IPC_LOG_PAGES		32

extern void *cnss_ipc_log_context;

#define cnss_ipc_log_string(_x...) do {					\
		if (cnss_ipc_log_context)				\
			ipc_log_string(cnss_ipc_log_context, _x);	\
	} while (0)

#define cnss_pr_err(_fmt, ...) do {					\
		printk("%scnss: " _fmt, KERN_ERR, ##__VA_ARGS__);	\
		cnss_ipc_log_string("%scnss: " _fmt, "", ##__VA_ARGS__);\
	} while (0)

#define cnss_pr_warn(_fmt, ...) do {					\
		printk("%scnss: " _fmt, KERN_WARNING, ##__VA_ARGS__);	\
		cnss_ipc_log_string("%scnss: " _fmt, "", ##__VA_ARGS__);\
	} while (0)

#define cnss_pr_info(_fmt, ...) do {					\
		printk("%scnss: " _fmt, KERN_INFO, ##__VA_ARGS__);	\
		cnss_ipc_log_string("%scnss: " _fmt, "", ##__VA_ARGS__);\
	} while (0)

#define cnss_pr_dbg(_fmt, ...) do {					\
		printk("%scnss: " _fmt, KERN_DEBUG, ##__VA_ARGS__);	\
		cnss_ipc_log_string("%scnss: " _fmt, "", ##__VA_ARGS__);\
	} while (0)

#ifdef CONFIG_CNSS2_DEBUG
#define CNSS_ASSERT(_condition) do {					\
		if (!(_condition)) {					\
			cnss_pr_err("ASSERT at line %d\n",		\
				    __LINE__);				\
			BUG_ON(1);					\
		}							\
	} while (0)
#else
#define CNSS_ASSERT(_condition) do {					\
		if (!(_condition)) {					\
			cnss_pr_err("ASSERT at line %d\n",		\
				    __LINE__);				\
			WARN_ON(1);					\
		}							\
	} while (0)
#endif

int cnss_debug_init(void);
void cnss_debug_deinit(void);
int cnss_debugfs_create(struct cnss_plat_data *plat_priv);
void cnss_debugfs_destroy(struct cnss_plat_data *plat_priv);

#endif /* _CNSS_DEBUG_H */
