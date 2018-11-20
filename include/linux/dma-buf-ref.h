/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _DMA_BUF_REF_H
#define _DMA_BUF_REF_H

struct dma_buf;
struct seq_file;

#ifdef CONFIG_DEBUG_DMA_BUF_REF
void dma_buf_ref_init(struct dma_buf *b);
void dma_buf_ref_destroy(struct dma_buf *b);
void dma_buf_ref_mod(struct dma_buf *b, int nr);
int dma_buf_ref_show(struct seq_file *s, struct dma_buf *dmabuf);

#else
static inline void dma_buf_ref_init(struct dma_buf *b) {}
static inline void dma_buf_ref_destroy(struct dma_buf *b) {}
static inline void dma_buf_ref_mod(struct dma_buf *b, int nr) {}
static inline int dma_buf_ref_show(struct seq_file *s, struct dma_buf *dmabuf)
{
	return -ENOMEM;
}
#endif


#endif /* _DMA_BUF_REF_H */
