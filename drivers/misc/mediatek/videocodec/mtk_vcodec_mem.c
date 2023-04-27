// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Yunfei Dong <yunfei.dong@mediatek.com>
 */

#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>
#include "mtk_heap.h"
#include "mtk_vcodec_mem.h"


struct mtk_vcodec_queue *mtk_vcodec_mem_init(struct device *dev)
{
	struct mtk_vcodec_queue *vcodec_queue;

	pr_debug("Allocate new vcodec_queue queue !\n");
	vcodec_queue = vzalloc(sizeof(struct mtk_vcodec_queue));
	if (vcodec_queue == NULL) {
		pr_info("Allocate new vcu queue fail!\n");
		vfree(vcodec_queue);
		return NULL;
	}
	vcodec_queue->dev = dev;
	vcodec_queue->num_buffers = 0;
	mutex_init(&vcodec_queue->mmap_lock);
	mutex_init(&vcodec_queue->dev_lock);

	return vcodec_queue;
}


void mtk_vcodec_mem_release(struct mtk_vcodec_queue *vcodec_queue)
{
	struct mtk_vcodec_mem *vcodec_buffer;
	unsigned int buffer;

	mutex_lock(&vcodec_queue->mmap_lock);
	pr_debug("[%s] Release code queue ,  Num_buffers :%d\n",
		__func__, vcodec_queue->num_buffers);
	if (vcodec_queue->num_buffers != 0) {
		for (buffer = 0; buffer < vcodec_queue->num_buffers; buffer++) {
			vcodec_buffer = &vcodec_queue->bufs[buffer];
			if (vcodec_buffer->mem_priv && atomic_read(&vcodec_buffer->ref_cnt) == 1) {
				if (vcodec_buffer->useAlloc)
					dma_heap_buffer_free(vcodec_buffer->mem_priv);
				else
					dma_buf_put(vcodec_buffer->mem_priv);

				atomic_set(&vcodec_buffer->ref_cnt, 0);
			pr_info("Free %d dbuf = %p size = %d mem_priv = %lx ref_cnt = %d\n",
				 buffer, vcodec_buffer->dbuf,
				 (unsigned int)vcodec_buffer->size,
				 (unsigned long)vcodec_buffer->mem_priv,
				 atomic_read(&vcodec_buffer->ref_cnt));
			}
		}
	}

	mutex_unlock(&vcodec_queue->mmap_lock);
	vfree(vcodec_queue);
	vcodec_queue = NULL;
}


void *mtk_vcodec_get_buffer(struct device *dev, struct mtk_vcodec_queue *vcodec_queue,
	struct VAL_MEM_INFO_T *mem_buff_data)
{
	struct mtk_vcodec_mem *vcodec_buf;
	unsigned int num_vcodec_buf = 0;
	struct dma_heap *dma_heap;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;

	mutex_lock(&vcodec_queue->mmap_lock);
	num_vcodec_buf = vcodec_queue->num_buffers;
	if (mem_buff_data->len == 0U || num_vcodec_buf >= CODEC_MAX_BUFFER) {
		pr_info("Get buffer fail: buffer len = %d num_buffers = %d !!\n",
			(unsigned int)mem_buff_data->len, num_vcodec_buf);
		mutex_unlock(&vcodec_queue->mmap_lock);
		return ERR_PTR(-EINVAL);
	}

	vcodec_buf = &vcodec_queue->bufs[num_vcodec_buf];
	if (mem_buff_data->shared_fd == 0) {
		pr_debug("shared_fd 0 case");
		dma_heap = dma_heap_find("mtk_mm");
		if (!dma_heap) {
			pr_info("[%s] dma heap find fail\n", __func__);
			mutex_unlock(&vcodec_queue->mmap_lock);
			return ERR_PTR(-EINVAL);
		}

		vcodec_buf->dbuf = dma_heap_buffer_alloc(dma_heap, mem_buff_data->len,
			O_RDWR | O_CLOEXEC, DMA_HEAP_VALID_HEAP_FLAGS);

		dma_heap_put(dma_heap);
		if (IS_ERR(vcodec_buf->dbuf)) {
			pr_info("[%s] dma heap buffer alloc fail\n", __func__);
			mutex_unlock(&vcodec_queue->mmap_lock);
			return ERR_PTR(-EINVAL);
		}
		vcodec_buf->useAlloc = 1;
		mem_buff_data->shared_fd = dma_buf_fd(vcodec_buf->dbuf, O_RDWR | O_CLOEXEC);
	} else {
		pr_debug("shared_fd != 0 case ");
		vcodec_buf->dbuf = dma_buf_get(mem_buff_data->shared_fd);
		vcodec_buf->useAlloc = 0;
	}

	attach = dma_buf_attach(vcodec_buf->dbuf, dev);
	if (IS_ERR(attach)) {
		pr_info("[%s] attach fail, return\n", __func__);
		mutex_unlock(&vcodec_queue->mmap_lock);
		return NULL;
	}
	vcodec_buf->attach = attach;

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		pr_info("map failed, detach and return\n");
		dma_buf_detach(vcodec_buf->dbuf, attach);
		mutex_unlock(&vcodec_queue->mmap_lock);
		return NULL;
	}
	vcodec_buf->sgt = sgt;
	mem_buff_data->iova = sg_dma_address(sgt->sgl);
	if (vcodec_buf->dbuf != NULL) {
		if (vcodec_buf->useAlloc)
			get_dma_buf(vcodec_buf->dbuf);
	}
	vcodec_buf->mem_priv = vcodec_buf->dbuf;
	vcodec_buf->size = mem_buff_data->len;
	vcodec_buf->iova = mem_buff_data->iova;

	vcodec_queue->num_buffers++;
	atomic_set(&vcodec_buf->ref_cnt, 1);
	mem_buff_data->cnt = 1;
	mutex_unlock(&vcodec_queue->mmap_lock);

	pr_info("[%s] nob:%d iova: %llx size: %d mem_priv: %lx useAlloc:%d, state: %d ref_cnt %d\n",
		__func__, vcodec_queue->num_buffers, mem_buff_data->iova,
		(unsigned int)vcodec_buf->size,
		(unsigned long)vcodec_buf->mem_priv, vcodec_buf->useAlloc,
		vcodec_buf->dbuf->sysfs_entry->kobj.state_initialized,
		atomic_read(&vcodec_buf->ref_cnt));

	return vcodec_buf->mem_priv;
}
static void vcodec_buf_remove(struct mtk_vcodec_queue *vcodec_queue, unsigned int buffer)
{
	unsigned int last_buffer;

	last_buffer = vcodec_queue->num_buffers - 1U;
	if (last_buffer != buffer)
		vcodec_queue->bufs[buffer] = vcodec_queue->bufs[last_buffer];
	vcodec_queue->bufs[last_buffer].mem_priv = NULL;
	vcodec_queue->bufs[last_buffer].size = 0;
	vcodec_queue->bufs[last_buffer].dbuf = NULL;
	atomic_set(&vcodec_queue->bufs[last_buffer].ref_cnt, 0);
	vcodec_queue->num_buffers--;
}
int mtk_vcodec_free_buffer(struct mtk_vcodec_queue *vcodec_queue,
	struct VAL_MEM_INFO_T *mem_buff_data)
{
	struct mtk_vcodec_mem *vcodec_buf;
	unsigned int i;
	unsigned int num_buf = 0;
	int ret = -EINVAL;

	mutex_lock(&vcodec_queue->mmap_lock);
	num_buf = vcodec_queue->num_buffers;

	pr_info("Free buffer iova = %llx, len %u queue_num = %d\n",
		mem_buff_data->iova, mem_buff_data->len, num_buf);
	if (num_buf != 0U) {
		for (i = 0; i < num_buf; i++) {
			vcodec_buf = &vcodec_queue->bufs[i];
			if (vcodec_buf->mem_priv == NULL || vcodec_buf->size == 0) {
				pr_info("%s invalid queue bufs[%u] in num_buffers %u",
					__func__, i, vcodec_queue->num_buffers);
				pr_info("%s (mem_priv 0x%x size %d ref_cnt %d)\n",
					__func__, vcodec_buf->mem_priv, vcodec_buf->size,
					atomic_read(&vcodec_buf->ref_cnt));
				vcodec_buf_remove(vcodec_queue, i);
				continue;
			}
			if (mem_buff_data->iova == vcodec_buf->iova &&
				mem_buff_data->len == vcodec_buf->size &&
				atomic_read(&vcodec_buf->ref_cnt) == 1) {
				pr_info("Free buffer index = %d iova = %llx, queue_num = %d\n",
					i, mem_buff_data->iova, num_buf);
				pr_info("[%s] iova = %llx size = %d mem_priv = %lx, useAlloc:%d\n",
				__func__, vcodec_buf->iova, (unsigned int)vcodec_buf->size,
				(unsigned long)vcodec_buf->mem_priv, vcodec_buf->useAlloc);

				/* decrease file count */
				//free iova here
				dma_buf_unmap_attachment(vcodec_buf->attach,
					vcodec_buf->sgt,
					DMA_BIDIRECTIONAL);

				dma_buf_detach(vcodec_buf->dbuf, vcodec_buf->attach);
				//free buffer here
				if (vcodec_buf->useAlloc)
					dma_heap_buffer_free(vcodec_buf->dbuf);
				else
					dma_buf_put(vcodec_buf->dbuf);

				atomic_set(&vcodec_buf->ref_cnt, 0);
				vcodec_buf_remove(vcodec_queue, i);
				ret = 0;
				break;
			}
		}
	}
	mutex_unlock(&vcodec_queue->mmap_lock);

	if (ret != 0)
		pr_info("Can not free memory sec_iova %lx len %u!\n",
			mem_buff_data->iova, mem_buff_data->len);

	return ret;
}
