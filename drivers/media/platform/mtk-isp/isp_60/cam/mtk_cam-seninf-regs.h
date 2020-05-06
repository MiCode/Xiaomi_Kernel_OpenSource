/* SPDX-License-Identifier: GPL-2.0 */
// Copyright (c) 2019 MediaTek Inc.

#ifndef __MTK_CAM_SENINF_REGS_H__
#define __MTK_CAM_SENINF_REGS_H__

#include "mtk_cam-seninf-top-ctrl.h"
#include "mtk_cam-seninf-seninf1-mux.h"
#include "mtk_cam-seninf-seninf1.h"
#include "mtk_cam-seninf-seninf1-csi2.h"
#include "mtk_cam-seninf-tg1.h"
#include "mtk_cam-seninf-cammux.h"

#include "mtk_cam-seninf-mipi-rx-ana-cdphy-csi0a.h"
#include "mtk_cam-seninf-csi0-cphy.h"
#include "mtk_cam-seninf-csi0-dphy.h"

#define SENINF_BITS(base, reg, field, val) do { \
	u32 __iomem *__p = base + reg; \
	u32 __v = *__p; \
	__v &= ~field##_MASK; \
	__v |= ((val) << field##_SHIFT); \
	*__p = __v; \
} while (0)

#define SENINF_READ_BITS(base, reg, field) \
({ \
	u32 __iomem *__p = base + reg; \
	u32 __v = *__p; \
	__v &= field##_MASK; \
	__v >>= field##_SHIFT; \
	__v; \
})

#define SENINF_READ_REG(base, reg) \
({ \
	u32 __iomem *__p = base + reg; \
	u32 __v = *__p; \
	__v; \
})

#define SENINF_WRITE_REG(base, reg, val) { \
	*(u32 __iomem *)(base + reg) = val; \
}

#endif
