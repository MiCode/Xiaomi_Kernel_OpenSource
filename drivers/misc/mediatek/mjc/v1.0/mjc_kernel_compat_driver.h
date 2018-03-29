/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MJC_KERNEL_COMPAT_DRIVER_H
#define _MJC_KERNEL_COMPAT_DRIVER_H

long compat_mjc_ioctl(struct file *pfile, unsigned int u4cmd, unsigned long u4arg);

#endif
