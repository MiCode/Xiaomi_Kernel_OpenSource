/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2016-2021, The Linux Foundation. All rights reserved. */
/* Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved. */

#ifndef _CNSS_DEBUG_H
#define _CNSS_DEBUG_H

#include <linux/printk.h>

enum log_level {
	EMERG_LOG = 0,
	ALERT_LOG = 1,
	CRIT_LOG = 2,
	ERR_LOG = 3,
	WARNING_LOG = 4,
	NOTICE_LOG = 5,
	INFO_LOG = 6,
	DEBUG_LOG = 7,
	DEBUG_HI_LOG = 8,
	MAX_LOG = 9,
};

extern enum log_level cnss_kernel_log_level;

#if IS_ENABLED(CONFIG_IPC_LOGGING)
#include <linux/ipc_logging.h>
#include <asm/current.h>

extern void *cnss_ipc_log_context;
extern void *cnss_ipc_log_long_context;
extern enum log_level cnss_ipc_log_level;

#ifdef CONFIG_CNSS2_DEBUG
#define CNSS_IPC_LOG_PAGES              100
#else
#define CNSS_IPC_LOG_PAGES              50
#endif
#define cnss_debug_log_print(_x...) \
		 cnss_debug_ipc_log_print(cnss_ipc_log_context, _x)

#define cnss_debug_log_long_print(_x...) \
		 cnss_debug_ipc_log_print(cnss_ipc_log_long_context, _x)
#else
#define cnss_debug_log_print(_x...) \
		 cnss_debug_ipc_log_print((void *)NULL, _x)
#define cnss_debug_log_long_print(_x...) \
		 cnss_debug_ipc_log_print((void *)NULL, _x)
#endif

#define proc_name (in_irq() ? "irq" : \
		   (in_softirq() ? "soft_irq" : current->comm))

#define cnss_pr_err(_fmt, ...) \
	cnss_debug_log_print(proc_name, __func__, \
			     ERR_LOG, ERR_LOG, _fmt, ##__VA_ARGS__)

#define cnss_pr_warn(_fmt, ...) \
	cnss_debug_log_print(proc_name, __func__, \
			     WARNING_LOG, WARNING_LOG, _fmt, ##__VA_ARGS__)

#define cnss_pr_info(_fmt, ...) \
	cnss_debug_log_print(proc_name, __func__, \
			     INFO_LOG, INFO_LOG, _fmt, ##__VA_ARGS__)

#define cnss_pr_dbg(_fmt, ...) \
	cnss_debug_log_print(proc_name, __func__, \
			     DEBUG_LOG, DEBUG_LOG, _fmt, ##__VA_ARGS__)

#define cnss_pr_vdbg(_fmt, ...) \
	cnss_debug_log_long_print(proc_name, __func__, \
				  DEBUG_LOG, DEBUG_LOG, _fmt, ##__VA_ARGS__)

#define cnss_pr_buf(_fmt, ...) \
	cnss_debug_log_long_print(proc_name, __func__, \
				  DEBUG_HI_LOG, DEBUG_LOG, _fmt, ##__VA_ARGS__)
#define cnss_pr_dbg_buf(_fmt, ...) \
	cnss_debug_log_long_print(proc_name, __func__, \
				  DEBUG_HI_LOG, DEBUG_HI_LOG, _fmt, ##__VA_ARGS__)

#ifdef CONFIG_CNSS2_DEBUG
#define CNSS_ASSERT(_condition) do {					\
		if (!(_condition)) {					\
			cnss_pr_err("ASSERT at line %d\n",		\
				    __LINE__);				\
			BUG();						\
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

#define cnss_fatal_err(_fmt, ...)					\
	cnss_pr_err("fatal: " _fmt, ##__VA_ARGS__)

int cnss_debug_init(void);
void cnss_debug_deinit(void);
int cnss_debugfs_create(struct cnss_plat_data *plat_priv);
void cnss_debugfs_destroy(struct cnss_plat_data *plat_priv);
void cnss_debug_ipc_log_print(void *log_ctx, char *process, const char *fn,
			      enum log_level kern_log_level,
			      enum log_level ipc_log_level, char *fmt, ...);
#endif /* _CNSS_DEBUG_H */
