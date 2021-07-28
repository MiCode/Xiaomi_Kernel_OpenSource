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

/* return 0 means error */
u32 dmabuf_to_secure_handle(struct dma_buf *dmabuf);

#endif /* _MTK_DMABUFHEAP_DEBUG_H */
