/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, 2021 The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_JPEG_DMA_SOC_H_
#define _CAM_JPEG_DMA_SOC_H_

#include "cam_soc_util.h"
#include "cam_jpeg_hw_intf.h"

/*
 * struct cam_jpeg_dma_soc_private:
 *
 * @Brief:                   Private SOC data specific to JPEG DMA Driver
 *
 * @num_pid                  JPEG number of pids
 * @pid:                     JPEG DMA pid value list
 * @rd_mid:                  JPEG DMA inport read mid value
 * @wr_mid:                  JPEG DMA outport write mid value
 */
struct cam_jpeg_dma_soc_private {
	uint32_t    num_pid;
	uint32_t    pid[CAM_JPEG_HW_MAX_NUM_PID];
	uint32_t    rd_mid;
	uint32_t    wr_mid;
};

int cam_jpeg_dma_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t jpeg_dma_irq_handler, void *irq_data);

int cam_jpeg_dma_enable_soc_resources(struct cam_hw_soc_info *soc_info);

int cam_jpeg_dma_disable_soc_resources(struct cam_hw_soc_info *soc_info);

#endif /* _CAM_JPEG_DMA_SOC_H_*/
