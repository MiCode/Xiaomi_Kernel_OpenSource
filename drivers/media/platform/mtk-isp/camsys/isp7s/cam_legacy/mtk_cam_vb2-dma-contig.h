/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_CAM_VB2_DMA_CONTIG_H
#define __MTK_CAM_VB2_DMA_CONTIG_H

#include <linux/dma-buf.h>

extern const struct vb2_mem_ops mtk_cam_dma_contig_memops;

void mtk_cam_vb2_sync_for_device(void *buf_priv);
void mtk_cam_vb2_sync_for_cpu(void *buf_priv);

#endif /*__MTK_CAM_VB2_DMA_CONTIG_H*/
