/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2022 Unisoc Inc.
 */
#ifndef _UAPI_SPRD_DMABUF_H
#define _UAPI_SPRD_DMABUF_H

#include <linux/dma-heap.h>
#include <linux/sprd_dmabuf.h>


#define DMABUF_IOC_PHY	_IOWR(DMA_HEAP_IOC_MAGIC, 11,\
				      struct dmabuf_phy_data)

#endif /* _UAPI_SPRD_DMABUF_H */
