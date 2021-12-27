/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_JPEG_DMA_SOC_H_
#define _CAM_JPEG_DMA_SOC_H_

#include "cam_soc_util.h"

int cam_jpeg_dma_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t jpeg_dma_irq_handler, void *irq_data);

int cam_jpeg_dma_enable_soc_resources(struct cam_hw_soc_info *soc_info);

int cam_jpeg_dma_disable_soc_resources(struct cam_hw_soc_info *soc_info);

#endif /* _CAM_JPEG_DMA_SOC_H_*/
