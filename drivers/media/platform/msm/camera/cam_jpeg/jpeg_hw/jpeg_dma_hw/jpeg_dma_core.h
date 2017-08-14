/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef CAM_JPEG_DMA_CORE_H
#define CAM_JPEG_DMA_CORE_H

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/dma-buf.h>

#include "cam_jpeg_hw_intf.h"

struct cam_jpeg_dma_device_hw_info {
	uint32_t reserved;
};

enum cam_jpeg_dma_core_state {
	CAM_JPEG_DMA_CORE_NOT_READY,
	CAM_JPEG_DMA_CORE_READY,
	CAM_JPEG_DMA_CORE_RESETTING,
	CAM_JPEG_DMA_CORE_STATE_MAX,
};

struct cam_jpeg_dma_device_core_info {
	enum cam_jpeg_dma_core_state core_state;
	struct cam_jpeg_dma_device_hw_info *jpeg_dma_hw_info;
	uint32_t cpas_handle;
	struct cam_jpeg_set_irq_cb irq_cb;
	int32_t ref_count;
	struct mutex core_mutex;
};

int cam_jpeg_dma_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_jpeg_dma_deinit_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_jpeg_dma_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size);
irqreturn_t cam_jpeg_dma_irq(int irq_num, void *data);

#endif /* CAM_JPEG_DMA_CORE_H */
