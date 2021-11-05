/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _ICNSS_DEBUG_H
#define _ICNSS_DEBUG_H

#include <linux/ipc_logging.h>
#include <linux/printk.h>

#define NUM_LOG_PAGES			10
#define NUM_LOG_LONG_PAGES		4

extern void *icnss_ipc_log_context;
extern void *icnss_ipc_log_long_context;
extern void *icnss_ipc_log_smp2p_context;
extern void *icnss_ipc_soc_wake_context;

#if IS_ENABLED(CONFIG_IPC_LOGGING)
#define icnss_ipc_log_string(_x...)                                     \
	ipc_log_string(icnss_ipc_log_context, _x)

#define icnss_ipc_log_long_string(_x...)                                \
	ipc_log_string(icnss_ipc_log_long_context, _x)

#define icnss_ipc_log_smp2p_string(_x...)                                \
	ipc_log_string(icnss_ipc_log_smp2p_context, _x)

#define icnss_ipc_soc_wake_string(_x...)                                \
	ipc_log_string(icnss_ipc_soc_wake_context, _x)
#else
#define icnss_ipc_log_string(_x...)

#define icnss_ipc_log_long_string(_x...)

#define icnss_ipc_log_smp2p_string(_x...)

#define icnss_ipc_soc_wake_string(_x...)
#endif

#define icnss_pr_err(_fmt, ...) do {                                    \
	printk("%s" pr_fmt(_fmt), KERN_ERR, ##__VA_ARGS__);             \
	icnss_ipc_log_string("%s" pr_fmt(_fmt), "",                     \
			     ##__VA_ARGS__);                            \
	} while (0)

#define icnss_pr_warn(_fmt, ...) do {                                   \
	printk("%s" pr_fmt(_fmt), KERN_WARNING, ##__VA_ARGS__);         \
	icnss_ipc_log_string("%s" pr_fmt(_fmt), "",                     \
			     ##__VA_ARGS__);                            \
	} while (0)

#define icnss_pr_info(_fmt, ...) do {                                   \
	printk("%s" pr_fmt(_fmt), KERN_INFO, ##__VA_ARGS__);            \
	icnss_ipc_log_string("%s" pr_fmt(_fmt), "",                     \
			     ##__VA_ARGS__);                            \
	} while (0)

#define icnss_pr_dbg(_fmt, ...) do {                                    \
	pr_debug(_fmt, ##__VA_ARGS__);                                  \
	icnss_ipc_log_string(pr_fmt(_fmt), ##__VA_ARGS__);              \
	} while (0)

#define icnss_pr_vdbg(_fmt, ...) do {                                   \
	pr_debug(_fmt, ##__VA_ARGS__);                                  \
	icnss_ipc_log_long_string(pr_fmt(_fmt), ##__VA_ARGS__);         \
	} while (0)

#define icnss_pr_smp2p(_fmt, ...) do {                                  \
	pr_debug(_fmt, ##__VA_ARGS__);                                  \
	icnss_ipc_log_smp2p_string(pr_fmt(_fmt), ##__VA_ARGS__);        \
	} while (0)

#define icnss_pr_soc_wake(_fmt, ...) do {                               \
	pr_debug(_fmt, ##__VA_ARGS__);                                  \
	icnss_ipc_soc_wake_string(pr_fmt(_fmt), ##__VA_ARGS__);         \
	} while (0)

#ifdef CONFIG_ICNSS2_DEBUG
#define ICNSS_ASSERT(_condition) do {                                   \
		if (!(_condition)) {                                    \
			icnss_pr_err("ASSERT at line %d\n", __LINE__);  \
			BUG();                                          \
		}                                                       \
	} while (0)
#else
#define ICNSS_ASSERT(_condition) do { } while (0)
#endif

#define icnss_fatal_err(_fmt, ...)                                      \
	icnss_pr_err("fatal: "_fmt, ##__VA_ARGS__)

enum icnss_debug_quirks {
	HW_ALWAYS_ON,
	HW_DEBUG_ENABLE,
	SKIP_QMI,
	RECOVERY_DISABLE,
	SSR_ONLY,
	PDR_ONLY,
	ENABLE_DAEMON_SUPPORT,
	FW_REJUVENATE_ENABLE,
};

void icnss_debug_init(void);
void icnss_debug_deinit(void);
int icnss_debugfs_create(struct icnss_priv *priv);
void icnss_debugfs_destroy(struct icnss_priv *priv);
#endif /* _ICNSS_DEBUG_H */
