/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Yunfei Dong <yunfei.dong@mediatek.com>
 */

#ifndef MTK_VCODEC_MEM_H
#define MTK_VCODEC_MEM_H

#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include "val_types_public.h"

#define CODEC_MAX_BUFFER 768U
#define CODEC_ALLOCATE_MAX_BUFFER_SIZE 0x10000000UL /*256MB*/

struct mtk_vcodec_mem {
	void *mem_priv;
	size_t size;
	struct dma_buf *dbuf;
	dma_addr_t iova;
	atomic_t ref_cnt;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	unsigned int useAlloc;
};


struct mtk_vcodec_queue {
	void *vcodec;
	struct mutex mmap_lock;
	struct device *dev;
	struct mutex dev_lock;
	unsigned int num_buffers;
	struct mtk_vcodec_mem bufs[CODEC_MAX_BUFFER];

};


/**
 * mtk_vcodec_mem_init - just init vcodec_queue
 *
 * @dev:        vcodec device.
 *
 * Return:      Return NULL if it is failed.
 * otherwise it is vcodec queue to store the allocated buffer
 **/
struct mtk_vcodec_queue *mtk_vcodec_mem_init(struct device *dev);

/**
 * mtk_vcodec_mem_release - just release the vcodec_queue
 *
 * @vcodec_queue:  the queue to store allocated buffer.
 *
 * Return: void
 **/
void mtk_vcodec_mem_release(struct mtk_vcodec_queue *vcodec_queue);

/**
 * mtk_vcodec_get_buffer/mtk_vcodec_get_page - get the allocated buffer iova/va/pa
 *
 * @vcodec_queue:  the queue to store allocated buffer.
 * @mem_buff_data:      store iova/va.
 *
 * Return: Return real address if it is ok, otherwise failed
 **/
void *mtk_vcodec_get_buffer(struct device *dev, struct mtk_vcodec_queue *vcodec_queue,
				struct VAL_MEM_INFO_T *mem_buff_data);

/**
 * mtk_vcodec_free_buffer/mtk_vcodec_free_page - just free unused buffer iova/va/pa
 *
 * @vcodec_queue:  the queue to store allocated buffer.
 * @mem_buff_data:      store iova/va to free.
 *
 * Return:      Return 0 if it is ok, otherwise failed
 **/
int mtk_vcodec_free_buffer(struct mtk_vcodec_queue *vcodec_queue,
						struct VAL_MEM_INFO_T *mem_buff_data);

#endif
