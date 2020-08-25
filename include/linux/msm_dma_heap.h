/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */
#ifndef _MSM_DMA_HEAP_H
#define _MSM_DMA_HEAP_H

#include <linux/bits.h>
#include <linux/msm_dma_heap_names.h>

/* Heap flags */
#define MSM_DMA_HEAP_FLAG_CACHED	BIT(1)

#define MSM_DMA_HEAP_FLAG_CP_TRUSTED_VM		BIT(15)
/* Unused */
#define MSM_DMA_HEAP_FLAG_CP_TOUCH		BIT(17)
#define MSM_DMA_HEAP_FLAG_CP_BITSTREAM		BIT(18)
#define MSM_DMA_HEAP_FLAG_CP_PIXEL		BIT(19)
#define MSM_DMA_HEAP_FLAG_CP_NON_PIXEL		BIT(20)
#define MSM_DMA_HEAP_FLAG_CP_CAMERA		BIT(21)
#define MSM_DMA_HEAP_FLAG_CP_HLOS		BIT(22)
#define MSM_DMA_HEAP_FLAG_CP_SPSS_SP		BIT(23)
#define MSM_DMA_HEAP_FLAG_CP_SPSS_SP_SHARED	BIT(24)
#define MSM_DMA_HEAP_FLAG_CP_SEC_DISPLAY	BIT(25)
#define MSM_DMA_HEAP_FLAG_CP_APP		BIT(26)
#define MSM_DMA_HEAP_FLAG_CP_CAMERA_PREVIEW	BIT(27)
/* Unused */
#define MSM_DMA_HEAP_FLAG_CP_CDSP		BIT(29)
#define MSM_DMA_HEAP_FLAG_CP_SPSS_HLOS_SHARED	BIT(30)

#define MSM_DMA_HEAP_FLAGS_CP_MASK	GENMASK(30, 15)

#endif /* _MSM_DMA_HEAP_H */
