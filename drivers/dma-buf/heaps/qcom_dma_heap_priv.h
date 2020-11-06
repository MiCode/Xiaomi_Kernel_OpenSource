/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _QCOM_DMA_HEAP_PRIV_H
#define _QCOM_DMA_HEAP_PRIV_H

void qcom_init_heap_helper_buffer(struct heap_helper_buffer *buffer,
				  void (*free)(struct heap_helper_buffer *));

struct dma_buf *qcom_heap_helper_export_dmabuf(struct heap_helper_buffer *buffer,
					       int fd_flags);

#endif /* _QCOM_DMA_HEAP_PRIV_H */
