/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#ifndef _LINUX_REGULATOR_SPM_H
#define _LINUX_REGULATOR_SPM_H

#include <linux/err.h>
#include <linux/init.h>

#ifdef CONFIG_REGULATOR_SPM
int __init spm_regulator_init(void);
#else
static inline int __init spm_regulator_init(void) { return -ENODEV; }
#endif

#endif
