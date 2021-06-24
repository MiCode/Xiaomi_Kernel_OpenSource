/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 * MTK heap api can be used by other modules
 */

#ifndef _MTK_DMABUFHEAP_H
#define _MTK_DMABUFHEAP_H

#include <linux/dma-buf.h>

/* return 0 means error */
u32 dmabuf_to_secure_handle(struct dma_buf *dmabuf);

#endif /* _MTK_DMABUFHEAP_DEBUG_H */
