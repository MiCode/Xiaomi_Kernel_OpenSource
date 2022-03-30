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


int mvpu_sysfs_init(void);
void mvpu_sysfs_exit(void);

#endif

