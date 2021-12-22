/* SPDX-License-Identifier: GPL-2.0 */
// Copyright (c) 2019 MediaTek Inc.

#ifndef __MTK_CAM_SENINF_REGS_H__
#define __MTK_CAM_SENINF_REGS_H__



#define SENINF_BITS(base, reg, field, val) do { \
	u32 __iomem *__p = base + reg; \
	u32 __v = readl(__p); \
	__v &= ~field##_MASK; \
	__v |= (((val) << field##_SHIFT) & field##_MASK); \
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

#define SENINF_WRITE_REG(base, reg, val) do { \
	u32 __iomem *__p = base + reg; \
	writel(val, __p); \
} while (0)
#endif
