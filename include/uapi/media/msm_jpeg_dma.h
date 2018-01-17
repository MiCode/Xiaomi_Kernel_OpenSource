/* Copyright (c) 2015-2016, 2018, The Linux Foundation. All rights reserved.
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

#ifndef __UAPI_MSM_JPEG_DMA__
#define __UAPI_MSM_JPEG_DMA__

#include <linux/videodev2.h>

/* msm jpeg dma control ID's */
#define V4L2_CID_JPEG_DMA_SPEED (V4L2_CID_PRIVATE_BASE)
#define V4L2_CID_JPEG_DMA_MAX_DOWN_SCALE (V4L2_CID_PRIVATE_BASE + 1)

/* msm_jpeg_dma_buf */
struct msm_jpeg_dma_buff {
	int32_t fd;
	uint32_t offset;
};

#endif /* __UAPI_MSM_JPEG_DMA__ */
