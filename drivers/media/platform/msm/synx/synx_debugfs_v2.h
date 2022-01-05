/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SYNX_DEBUGFS_V2_H__
#define __SYNX_DEBUGFS_V2_H__

#include <linux/debugfs.h>
#include <linux/delay.h>

#include "synx_private_v2.h"

enum synx_debug_level {
	SYNX_ERR  = 0x0001,
	SYNX_WARN = 0x0002,
	SYNX_INFO = 0x0004,
	SYNX_DBG  = 0x0008,
	SYNX_VERB = 0x0010,
	SYNX_IPCL = 0x0020,
	SYNX_GSM  = 0x0040,
	SYNX_MEM  = 0x0080,
	SYNX_ALL  = SYNX_ERR | SYNX_WARN | SYNX_INFO |
				SYNX_DBG | SYNX_IPCL | SYNX_GSM  | SYNX_MEM,
};

enum synx_columns_level {
	NAME_COLUMN     = 0x0001,
	ID_COLUMN       = 0x0002,
	BOUND_COLUMN    = 0x0004,
	STATE_COLUMN    = 0x0008,
	FENCE_COLUMN    = 0x0010,
	COREDATA_COLUMN = 0x0020,
	GLOBAL_COLUMN   = 0x0040,
	ERROR_CODES     = 0x8000,
};

#ifndef SYNX_DBG_LABEL
#define SYNX_DBG_LABEL "synx"
#endif

#define SYNX_DBG_TAG SYNX_DBG_LABEL ": %4s: "

extern int synx_debug;

static inline char *synx_debug_str(int level)
{
	switch (level) {
	case SYNX_ERR:
		return "err";
	case SYNX_WARN:
		return "warn";
	case SYNX_INFO:
		return "info";
	case SYNX_DBG:
		return "dbg";
	case SYNX_VERB:
		return "verb";
	case SYNX_IPCL:
		return "ipcl";
	case SYNX_GSM:
		return "gmem";
	case SYNX_MEM:
		return "mem";
	default:
		return "???";
	}
}

#define dprintk(__level, __fmt, arg...)                 \
	do {                                                \
		if (synx_debug & __level) {                     \
			pr_info(SYNX_DBG_TAG "%s: %d: "  __fmt,     \
				synx_debug_str(__level), __func__,      \
				__LINE__, ## arg);                      \
		}                                               \
	} while (0)

/**
 * synx_init_debugfs_dir - Initializes debugfs
 *
 * @param dev : Pointer to synx device structure
 */
struct dentry *synx_init_debugfs_dir(struct synx_device *dev);

/**
 * synx_remove_debugfs_dir - Removes debugfs
 *
 * @param dev : Pointer to synx device structure
 */
void synx_remove_debugfs_dir(struct synx_device *dev);

#endif /* __SYNX_DEBUGFS_V2_H__ */
