/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __SSPM_SYSFS_H__
#define __SSPM_SYSFS_H__

#include <linux/sysfs.h>
#include <linux/device.h>

extern void sspm_log_if_wake(void);
extern int __init sspm_sysfs_init(void);
extern int sspm_sysfs_create_file(const struct device_attribute *attr);
extern int sspm_sysfs_create_bin_file(const struct bin_attribute *attr);
#endif
