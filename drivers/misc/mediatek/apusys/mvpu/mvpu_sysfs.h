/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MVPU_DEBUG_H__
#define __MVPU_DEBUG_H__

#define DEBUG 1

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/seq_file.h>

static int mvpu_loglvl_sys;

enum {
	APUSYS_MVPU_LOG_ERR  = 0,
	APUSYS_MVPU_LOG_WRN  = 1,
	APUSYS_MVPU_LOG_INFO = 2,
	APUSYS_MVPU_LOG_DBG  = 3,
	APUSYS_MVPU_LOG_ALL  = 4,
};

int get_mvpu_log_lvl(void);

int mvpu_sysfs_init(void);
void mvpu_sysfs_exit(void);

#endif

