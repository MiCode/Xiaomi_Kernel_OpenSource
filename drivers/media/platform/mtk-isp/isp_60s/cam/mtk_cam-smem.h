/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __MTK_CAM_ISP_SMEM_H
#define __MTK_CAM_ISP_SMEM_H

#include <linux/dma-mapping.h>

struct mtk_cam_device;

#define CQ_MEM_SIZE 0x200000
#define META_INPUT_SIZE 0x800000

int mtk_cam_reserved_memory_init(struct mtk_cam_device *cam);
void mtk_cam_reserved_memory_uninit(struct mtk_cam_device *cam);
dma_addr_t mtk_cam_smem_iova_to_scp_addr(struct device *smem_dev,
					 dma_addr_t iova);

#endif
