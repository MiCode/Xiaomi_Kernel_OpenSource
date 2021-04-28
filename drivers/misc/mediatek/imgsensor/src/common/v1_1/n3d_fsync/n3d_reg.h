/*
 * Copyright (C) 2021 MediaTek Inc.
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

#ifndef __N3D__REG_H__
#define __N3D__REG_H__

#include <linux/io.h>

#include "seninf_n3d.h"
#include "seninf_cammux.h"

#define SENINF_BITS(base, reg, field, val) do { \
	u32 __iomem *__p = base + reg; \
	u32 __v = readl(__p); \
	__v &= ~field##_MASK; \
	__v |= ((val) << field##_SHIFT); \
	writel(__v, __p); \
} while (0)

#define SENINF_READ_BITS(base, reg, field) \
	({ \
	 u32 __iomem *__p = base + reg; \
	 u32 __v = readl(__p); \
	 __v &= field##_MASK; \
	 __v >>= field##_SHIFT; \
	 __v; \
	 })

#define SENINF_READ_REG(base, reg) \
	({ \
	 u32 __iomem *__p = base + reg; \
	 u32 __v = readl(__p); \
	 __v; \
	 })

#define SENINF_WRITE_REG(base, reg, val) { \
	u32 __iomem *__p = base + reg; \
	writel(val, __p); \
}
#endif

