/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef SDE_DBG_H_
#define SDE_DBG_H_

#include <stdarg.h>
#include <linux/debugfs.h>
#include <linux/list.h>

#define SDE_EVTLOG_DATA_LIMITER	(-1)
#define SDE_EVTLOG_FUNC_ENTRY	0x1111
#define SDE_EVTLOG_FUNC_EXIT	0x2222

#define SDE_DBG_DUMP_DATA_LIMITER (NULL)

enum sde_dbg_evtlog_flag {
	SDE_EVTLOG_DEFAULT = BIT(0),
	SDE_EVTLOG_IRQ = BIT(1),
	SDE_EVTLOG_ALL = BIT(7)
};

/**
 * SDE_EVT32 - Write an list of 32bit values as an event into the event log
 * ... - variable arguments
 */
#define SDE_EVT32(...) sde_evtlog(__func__, __LINE__, SDE_EVTLOG_DEFAULT, \
		##__VA_ARGS__, SDE_EVTLOG_DATA_LIMITER)
#define SDE_EVT32_IRQ(...) sde_evtlog(__func__, __LINE__, SDE_EVTLOG_IRQ, \
		##__VA_ARGS__, SDE_EVTLOG_DATA_LIMITER)

#define SDE_DBG_DUMP(...)	\
	sde_dbg_dump(false, __func__, ##__VA_ARGS__, \
		SDE_DBG_DUMP_DATA_LIMITER)

#define SDE_DBG_DUMP_WQ(...)	\
	sde_dbg_dump(true, __func__, ##__VA_ARGS__, \
		SDE_DBG_DUMP_DATA_LIMITER)

#if defined(CONFIG_DEBUG_FS)

int sde_evtlog_init(struct dentry *debugfs_root);
void sde_evtlog_destroy(void);
void sde_evtlog(const char *name, int line, int flag, ...);
void sde_dbg_dump(bool queue, const char *name, ...);
#else
static inline int sde_evtlog_init(struct dentry *debugfs_root) { return 0; }
static inline void sde_evtlog(const char *name, int line,  flag, ...) {}
static inline void sde_evtlog_destroy(void) { }
static inline void sde_dbg_dump(bool queue, const char *name, ...) {}
#endif

#endif /* SDE_DBG_H_ */
