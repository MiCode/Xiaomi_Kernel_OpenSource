/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

