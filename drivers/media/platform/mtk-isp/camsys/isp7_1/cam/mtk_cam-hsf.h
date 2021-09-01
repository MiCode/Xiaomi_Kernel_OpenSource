/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_HSF_H
#define __MTK_CAM_HSF_H

#include "mtk_cam.h"
#include "mtk_cam-hsf-def.h"

int mtk_cam_hsf_init(struct mtk_cam_ctx *ctx);
int mtk_cam_hsf_config(struct mtk_cam_ctx *ctx, unsigned int raw_id);
int mtk_cam_hsf_uninit(struct mtk_cam_ctx *ctx);
void ccu_apply_cq(struct mtk_raw_device *dev, dma_addr_t cq_addr, unsigned int cq_size,
int initial, unsigned int cq_offset,  unsigned int sub_cq_size, unsigned int sub_cq_offset);
void ccu_stream_on(struct mtk_raw_device *dev);

struct dma_buf *mtk_cam_dmabuf_alloc(struct mtk_cam_ctx *ctx, unsigned int size);
int mtk_cam_dmabuf_get_iova(struct mtk_cam_ctx *ctx, struct device *dev,
struct mtk_cam_dma_map *dmap);
void mtk_cam_dmabuf_free_iova(struct mtk_cam_ctx *ctx, struct mtk_cam_dma_map *dmap);

#endif /*__MTK_CAM_RAW_H*/
