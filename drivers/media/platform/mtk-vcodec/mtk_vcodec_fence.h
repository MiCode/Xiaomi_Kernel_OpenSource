/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MTK_VCODEC_FENCE_H
#define _MTK_VCODEC_FENCE_H

#include <linux/dma-fence.h>

struct dma_fence *mtk_vcodec_create_fence(int fence_count);
void mtk_vcodec_fence_signal(struct dma_fence *fence, int index);

#endif
