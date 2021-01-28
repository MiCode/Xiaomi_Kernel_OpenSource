/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_VCODEC_MEM_H
#define MTK_VCODEC_MEM_H

#include <media/videobuf2-dma-contig.h>
#include <uapi/linux/mtk_vcu_controls.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>

#ifndef CONFIG_ARM64
#include "mm/dma.h"
#endif

#ifndef dmac_map_area
#define dmac_map_area __dma_map_area
#endif
#ifndef dmac_unmap_area
#define dmac_unmap_area __dma_unmap_area
#endif
#ifndef dmac_flush_range
#define dmac_flush_range __dma_flush_range
#endif

#define CODEC_MAX_BUFFER 512U
#define CODEC_ALLOCATE_MAX_BUFFER_SIZE 0x8000000UL /*128MB*/

/**
 * struct mtk_vcu_mem - memory buffer allocated in kernel
 *
 * @mem_priv:   vcu allocated buffer vb2_dc_buf
 * @size:       buffer size
 * @dbuf:       io buffer dma_buff
 * @iova:       io buffer iova
 */
struct mtk_vcu_mem {
	void *mem_priv;
	size_t size;
	struct dma_buf *dbuf;
	dma_addr_t iova;
};

/**
 * struct mtk_vcu_queue - the allocated buffer queue
 *
 * @vcu:        struct mtk_vcu
 * @mmap_lock:  the lock to protect allocated buffer
 * @dev:        device
 * @num_buffers:        allocated buffer number
 * @mem_ops:    the file operation of memory allocated
 * @bufs:       store the information of allocated buffers
 * @map_buf_pa: store map pa and it's flag
 */
struct mtk_vcu_queue {
	void *vcu;
	struct mutex mmap_lock;
	struct device *dev;
	unsigned int num_buffers;
	const struct vb2_mem_ops *mem_ops;
	struct mtk_vcu_mem bufs[CODEC_MAX_BUFFER];
	uint64_t map_buf_pa;
};

/**
 * mtk_vcu_mem_init - just init vcu_queue
 *
 * @dev:        vcu device.
 *
 * Return:      Return NULL if it is failed.
 * otherwise it is vcu queue to store the allocated buffer
 **/
struct mtk_vcu_queue *mtk_vcu_mem_init(struct device *dev);

/**
 * mtk_vcu_mem_release - just release the vcu_queue
 *
 * @vcu_queue:  the queue to store allocated buffer.
 *
 * Return: void
 **/
void mtk_vcu_mem_release(struct mtk_vcu_queue *vcu_queue);

/**
 * mtk_vcu_set_buffer - set the allocated buffer iova/va
 *
 * @vcu_queue:  the queue to store allocated buffer.
 * @mem_buff_data:      store iova/va.
 * @src_vb/dst_vb:      set io buffer dma to vcu_queue for cache sync.
 *
 * Return: Return real address if it is ok, otherwise failed
 **/
void *mtk_vcu_set_buffer(struct mtk_vcu_queue *vcu_queue,
	struct mem_obj *mem_buff_data, struct vb2_buffer *src_vb,
	struct vb2_buffer *dst_vb);

/**
 * mtk_vcu_get_buffer - get the allocated buffer iova/va
 *
 * @vcu_queue:  the queue to store allocated buffer.
 * @mem_buff_data:      store iova/va.
 *
 * Return: Return real address if it is ok, otherwise failed
 **/
void *mtk_vcu_get_buffer(struct mtk_vcu_queue *vcu_queue,
						 struct mem_obj *mem_buff_data);

/**
 * mtk_vcu_free_buffer - just free unused buffer iova/va
 *
 * @vcu_queue:  the queue to store allocated buffer.
 * @mem_buff_data:      store iova/va to free.
 *
 * Return:      Return 0 if it is ok, otherwise failed
 **/
int mtk_vcu_free_buffer(struct mtk_vcu_queue *vcu_queue,
						struct mem_obj *mem_buff_data);

/**
 * vcu_buffer_flush_all - flush all VCU buffer cache for device
 *
 * @dev:        vcu device.
 * @vcu_queue:  the queue to store allocated buffer.
 *
 * Return:      Return 0 if it is ok, otherwise failed
 **/
int vcu_buffer_flush_all(struct device *dev, struct mtk_vcu_queue *vcu_queue);

/**
 * vcu_buffer_cache_sync - VCU buffer cache sync by dma_addr for device
 *
 * @dev:        vcu device.
 * @vcu_queue:  the queue to store allocated buffer.
 * @dma_addr:   the buffer to be flushed dma addr
 * @dma_addr:   the corresponding flushed size
 * @op:         DMA_TO_DEVICE or DMA_FROM_DEVICE
 *
 * Return:      Return 0 if it is ok, otherwise failed
 **/
int vcu_buffer_cache_sync(struct device *dev, struct mtk_vcu_queue *vcu_queue,
	dma_addr_t dma_addr, size_t size, int op);

#endif

