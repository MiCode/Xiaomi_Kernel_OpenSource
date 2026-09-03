// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/dma-direction.h>
#include <linux/dma-buf.h>
#include <linux/sprd_iommu.h>
#include <linux/ion.h>
#include <drm/gsp_cfg.h>
#include "gsp_debug.h"
#include "gsp_layer.h"

int gsp_layer_to_type(struct gsp_layer *layer)
{
	return layer->type;
}

u32 gsp_layer_to_uv_offset(struct gsp_layer *layer)
{
	return layer->mem_data.uv_offset;
}

u32 gsp_layer_to_v_offset(struct gsp_layer *layer)
{
	return layer->mem_data.v_offset;
}

int gsp_layer_to_share_fd(struct gsp_layer *layer)
{
	return layer->mem_data.share_fd;
}

struct gsp_buf *gsp_layer_to_buf(struct gsp_layer *layer)
{
	return &layer->mem_data.buf;
}

struct gsp_buf_map *gsp_layer_to_buf_map(struct gsp_layer *layer)
{
	return &layer->mem_data.map;
}

struct gsp_addr_data *gsp_layer_to_addr(struct gsp_layer *layer)
{
	return &layer->src_addr;
}

int gsp_layer_to_wait_fd(struct gsp_layer *layer)
{
	return layer->wait_fd;
}

int gsp_layer_to_sig_fd(struct gsp_layer *layer)
{
	return layer->sig_fd;
}

int gsp_layer_is_enable(struct gsp_layer *layer)
{
	return layer->enable;
}

int gsp_layer_has_share_fd(struct gsp_layer *layer)
{
	int fd = -1;

	fd = gsp_layer_to_share_fd(layer);
	return fd <= 0 ? 0 : 1;
}

int gsp_layer_is_filled(struct gsp_layer *layer)
{
	return layer->filled;
}

void gsp_layer_set_filled(struct gsp_layer *layer)
{
	layer->filled = 1;
}

int gsp_layer_buf_verify(struct gsp_buf *buf)
{
	return (buf->size == 0) || (IS_ERR_OR_NULL(buf->dmabuf)) ? 1 : 0;
}

int gsp_layer_buf_map_verify(struct gsp_buf_map *map)
{
	int ret = -1;

	ret =  (IS_ERR_OR_NULL(map->attachment))
		|| (IS_ERR_OR_NULL(map->table));

	return ret == 0 ? 0 : -1;
}

int gsp_layer_addr_is_unset(struct gsp_layer *layer)
{
	struct gsp_addr_data *addr = NULL;

	addr = gsp_layer_to_addr(layer);

	return addr->addr_y == 0 ? 1 : 0;
}

int gsp_layer_need_iommu(struct gsp_layer *layer)
{
	struct gsp_buf *buf = NULL;

	buf = gsp_layer_to_buf(layer);
	return gsp_layer_is_enable(layer) &&
		gsp_layer_has_share_fd(layer) && buf->is_iova;
}

void gsp_layer_addr_set(struct gsp_layer *layer, u32 iova_addr)
{
	struct gsp_addr_data *addr = NULL;
	u32 offset = 0;

	addr = gsp_layer_to_addr(layer);
	addr->addr_y = iova_addr;
	GSP_DEBUG("map iommu addr_y: %x.\n", iova_addr);

	offset = gsp_layer_to_uv_offset(layer);
	addr->addr_uv = addr->addr_y + offset;
	GSP_DEBUG("map iommu addr_uv: %x.\n", addr->addr_uv);

	offset = gsp_layer_to_v_offset(layer);
	addr->addr_va = addr->addr_y + offset;
	GSP_DEBUG("map iommu addr_va: %x.\n", addr->addr_va);

}

void gsp_layer_addr_put(struct gsp_layer *layer)
{
	struct gsp_addr_data *addr = NULL;

	addr = gsp_layer_to_addr(layer);

	addr->addr_y = 0;
	addr->addr_uv = 0;
	addr->addr_va = 0;
}

int gsp_layer_get_dmabuf(struct gsp_layer *layer)
{
	int fd = -1;
	int ret = -1;
	struct gsp_buf *buf = NULL;
	struct dma_buf *dmabuf = NULL;

	fd = gsp_layer_to_share_fd(layer);
	buf = gsp_layer_to_buf(layer);

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		GSP_ERR("layer[%d] get dma buffer failed, fd: %d\n",
			gsp_layer_to_type(layer), fd);
		goto done;
	}
	buf->dmabuf = dmabuf;
	buf->size = dmabuf->size;
	buf->is_iova = 1;

	GSP_DEBUG("layer[%d] get dmabuf success\n", gsp_layer_to_type(layer));
	ret = 0;
done:
	return ret;
}

void gsp_layer_put_dmabuf(struct gsp_layer *layer)
{
	struct gsp_buf *buf = NULL;

	if (IS_ERR_OR_NULL(layer)) {
		GSP_WARN("no need to put layer dmabuf\n");
		return;
	}

	buf = gsp_layer_to_buf(layer);
	if (buf->dmabuf)
		dma_buf_put(buf->dmabuf);

	/* reset gsp buf status */
	buf->dmabuf = NULL;
	buf->size = 0;

	GSP_DEBUG("gsp layer put dmabuf\n");
}

void gsp_layer_dmabuf_unmap(struct gsp_layer *layer)
{
	struct gsp_buf *buf = NULL;
	struct gsp_buf_map *map = NULL;

	buf = gsp_layer_to_buf(layer);
	map = gsp_layer_to_buf_map(layer);

	if (map->table) {
		dma_buf_unmap_attachment(map->attachment, map->table, map->dir);
		map->table = NULL;
	}

	if (map->attachment) {
		dma_buf_detach(buf->dmabuf, map->attachment);
		map->attachment = NULL;
	}

	GSP_DEBUG("gsp layer dmabuf unmap\n");
}

int gsp_layer_dmabuf_map(struct gsp_layer *layer, struct device *dev)
{
	int ret = -1;
	struct gsp_buf *buf = NULL;
	struct gsp_buf_map *map = NULL;
	struct dma_buf_attachment *attachment = NULL;
	struct sg_table *table = NULL;
	enum dma_data_direction dir;

	buf = gsp_layer_to_buf(layer);
	map = gsp_layer_to_buf_map(layer);

	attachment = dma_buf_attach(buf->dmabuf, dev);
	if (IS_ERR_OR_NULL(attachment)) {
		GSP_ERR("dma buf attach failed\n");
		goto done;
	}
	map->attachment = attachment;

	if (layer->type == GSP_DES_LAYER)
		dir = DMA_FROM_DEVICE;
	else
		dir = DMA_TO_DEVICE;

	map->dir = dir;

	table = dma_buf_map_attachment(attachment, dir);
	if (IS_ERR_OR_NULL(table)) {
		GSP_ERR("dma buffer map attachment failed\n");
		goto done;
	}
	map->table = table;

	GSP_DEBUG("gsp layer dmabuf map success\n");
	ret = 0;
done:
	if (ret < 0)
		gsp_layer_dmabuf_unmap(layer);

	return ret;
}

int gsp_layer_iommu_map(struct gsp_layer *layer, struct device *dev)
{
	int ret = -1;
	struct sprd_iommu_map_data iommu_data;
	struct gsp_buf *buf = NULL;

	/* this buf has been assigned when kcfg get dmabuf
	 * so we need to check whether it is validate
	 */
	buf = gsp_layer_to_buf(layer);
	ret = gsp_layer_buf_verify(buf);
	if (ret) {
		GSP_ERR("layer[%d] buf is invalidate\n",
			gsp_layer_to_type(layer));
		goto done;
	}

	iommu_data.buf = buf->dmabuf->priv;
	iommu_data.iova_size = buf->size;
	iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;
	ret = sprd_iommu_map(dev, &iommu_data);
	if (ret) {
		GSP_ERR("get dma buffer address failed\n");
		goto done;
	}

	if (iommu_data.iova_addr == 0) {
		GSP_ERR("iommu allocate iova addr error\n");
		ret = -1;
	} else {
		gsp_layer_addr_set(layer, iommu_data.iova_addr);
	}
done:
	if (ret < 0)
		GSP_ERR("gsp layer[%d] buf map failed\n",
			gsp_layer_to_type(layer));
	else
		GSP_DEBUG("gsp layer[%d] buf map success\n",
			gsp_layer_to_type(layer));

	return ret;
}

void gsp_layer_iommu_unmap(struct gsp_layer *layer, struct device *dev)
{
	struct gsp_buf *buf = NULL;
	struct gsp_addr_data *addr = NULL;
	struct sprd_iommu_unmap_data iommu_data;

	iommu_data.buf = NULL;

	buf = gsp_layer_to_buf(layer);
	addr = gsp_layer_to_addr(layer);
	if (buf->size && addr->addr_y) {
		/* fill iommu data with gsp buf information */
		iommu_data.iova_size = buf->size;
		iommu_data.iova_addr = addr->addr_y;
		iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;
		if (sprd_iommu_unmap(dev, &iommu_data))
			GSP_ERR("layer[%d] free iommu kaddr failed\n",
				gsp_layer_to_type(layer));
	}

	gsp_layer_addr_put(layer);
}

void gsp_layer_common_print(struct gsp_layer *layer)
{
	struct gsp_addr_data *addr = NULL;

	addr = gsp_layer_to_addr(layer);
	GSP_DEBUG("layer type[%d]\n", gsp_layer_to_type(layer));
	GSP_DEBUG("layer enable: %d\n", gsp_layer_is_enable(layer));
	GSP_DEBUG("layer share_fd: %d\n", gsp_layer_to_share_fd(layer));
	GSP_DEBUG("layer wait_fd: %d\n", gsp_layer_to_wait_fd(layer));
	GSP_DEBUG("layer sig_fd: %d\n", gsp_layer_to_sig_fd(layer));
	GSP_DEBUG("layer src addr y: %x\n", addr->addr_y);
	GSP_DEBUG("layer src addr u: %x\n", addr->addr_uv);
	GSP_DEBUG("layer src addr v: %x\n", addr->addr_va);
}
