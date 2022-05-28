/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __APS_SYSFS_H__
#define __APS_SYSFS_H__

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/seq_file.h>

int aps_sysfs_init(void);
void aps_sysfs_exit(void);

#endif
