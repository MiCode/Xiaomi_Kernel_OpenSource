/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 * MTK heap api can be used by other modules
 */

/**
 * This file is used to export api for mtk dmabufheap users
 * Please don't add dmabufheap private info
 */
#ifndef _MTK_DMABUFHEAP_H
#define _MTK_DMABUFHEAP_H
#include <linux/dma-buf.h>

extern atomic64_t dma_heap_normal_total;

/* return 0 means error */
u64 dmabuf_to_secure_handle(const struct dma_buf *dmabuf);

int is_system_heap_dmabuf(const struct dma_buf *dmabuf);
int is_mtk_mm_heap_dmabuf(const struct dma_buf *dmabuf);
int is_mtk_sec_heap_dmabuf(const struct dma_buf *dmabuf);
int is_support_secure_handle(const struct dma_buf *dmabuf);

long mtk_dma_buf_set_name(struct dma_buf *dmabuf, const char *buf);

#endif /* _MTK_DMABUFHEAP_DEBUG_H */
