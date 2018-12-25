/*
 *
 * (C) COPYRIGHT 2012 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



#ifndef _DMA_BUF_LOCK_H
#define _DMA_BUF_LOCK_H

typedef enum dma_buf_lock_exclusive
{
	DMA_BUF_LOCK_NONEXCLUSIVE = 0,
	DMA_BUF_LOCK_EXCLUSIVE = -1
} dma_buf_lock_exclusive;

typedef struct dma_buf_lock_k_request
{
	int count;
	int *list_of_dma_buf_fds;
	int timeout;
	dma_buf_lock_exclusive exclusive;
} dma_buf_lock_k_request;

#define DMA_BUF_LOCK_IOC_MAGIC '~'

#define DMA_BUF_LOCK_FUNC_LOCK_ASYNC       _IOW(DMA_BUF_LOCK_IOC_MAGIC, 11, dma_buf_lock_k_request)

#define DMA_BUF_LOCK_IOC_MINNR 11
#define DMA_BUF_LOCK_IOC_MAXNR 11

#endif /* _DMA_BUF_LOCK_H */
