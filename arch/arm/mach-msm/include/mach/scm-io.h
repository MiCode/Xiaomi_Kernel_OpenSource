/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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
#ifndef __MACH_SCM_IO_H
#define __MACH_SCM_IO_H

#include <linux/types.h>

#ifdef CONFIG_MSM_SECURE_IO

extern u32 secure_readl(void __iomem *c);
extern void secure_writel(u32 v, void __iomem *c);

#else

#define secure_readl(c) readl(c)
#define secure_writel(v, c) writel(v, c)

#endif

#endif
