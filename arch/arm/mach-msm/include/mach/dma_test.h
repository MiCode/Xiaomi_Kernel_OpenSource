/* Copyright (c) 2008-2009, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MSM_DMA_TEST__
#define __MSM_DMA_TEST__

#include <linux/ioctl.h>

#define MSM_DMA_IOC_MAGIC     0x83

/* The testing driver can manage a series of buffers.  These are
 * allocated and freed using these calls. */
struct msm_dma_alloc_req {
	int size;		/* Size of this request, in bytes. */
	int bufnum;		/* OUT: Number of buffer allocated. */
};
#define MSM_DMA_IOALLOC _IOWR(MSM_DMA_IOC_MAGIC, 2, struct msm_dma_alloc_req)

/* Free the specified buffer. */
#define MSM_DMA_IOFREE _IOW(MSM_DMA_IOC_MAGIC, 3, int)

/* Free all used buffers. */
#define MSM_DMA_IOFREEALL  _IO(MSM_DMA_IOC_MAGIC, 7)

/* Read/write data into kernel buffer. */
struct msm_dma_bufxfer {
	void *data;
	int size;
	int bufnum;
};
#define MSM_DMA_IOWBUF _IOW(MSM_DMA_IOC_MAGIC, 4, struct msm_dma_bufxfer)
#define MSM_DMA_IORBUF _IOW(MSM_DMA_IOC_MAGIC, 5, struct msm_dma_bufxfer)

/* Use the data mover to copy from one buffer to another. */
struct msm_dma_scopy {
	int srcbuf;
	int destbuf;
	int size;
};
#define MSM_DMA_IOSCOPY _IOW(MSM_DMA_IOC_MAGIC, 6, struct msm_dma_scopy)

#endif /* __MSM_DMA_TEST__ */
