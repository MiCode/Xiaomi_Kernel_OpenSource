// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "vdec_fmt_dmabuf.h"
#include "vdec_fmt_utils.h"

int fmt_dmabuf_get_iova(struct dma_buf *dbuf, u64 *iova,
	struct device *dev, struct dma_buf_attachment **attach, struct sg_table **sgt)
{
	*attach = dma_buf_attach(dbuf, dev);
	if (IS_ERR(*attach)) {
		fmt_debug(0, "attach fail, return\n");
		*attach = NULL;
		return -1;
	}

	*sgt = dma_buf_map_attachment(*attach, DMA_TO_DEVICE);
	if (IS_ERR(*sgt)) {
		fmt_debug(0, "map failed, detach and return\n");
		dma_buf_detach(dbuf, *attach);
		*sgt = NULL;
		return -1;
	}

	*iova = sg_dma_address((*sgt)->sgl);
	return 0;
}

void fmt_dmabuf_free_iova(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach, struct sg_table *sgt)
{
	if (attach == NULL || sgt == NULL) {
		fmt_debug(0, "attach or sgt null, not need to free iova");
		return;
	}
	dma_buf_unmap_attachment(attach, sgt, DMA_TO_DEVICE);
	dma_buf_detach(dbuf, attach);
}

struct dma_buf *fmt_dmabuf_get(int fd)
{
	struct dma_buf *dbuf;

	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		fmt_debug(0, "dma_buf_get fail");
		return NULL;
	}

	return dbuf;
}

void fmt_dmabuf_put(struct dma_buf *dbuf)
{
	if (!dbuf) {
		fmt_debug(0, "dma_buf null, no need to put.");
		return;
	}
	dma_buf_put(dbuf);
}

u64 fmt_translate_fd(u64 fd, u32 offset, struct dmabufmap map[], struct device *dev,
	struct dma_buf **dbuf, struct dma_buf_attachment **attach, struct sg_table **sgt)
{
	int i, ret;
	u64 iova = 0;

	for (i = 0; i < FMT_FD_RESERVE; i++) {
		if (fd == map[i].fd) {
			fmt_debug(1, "quick search iova 0x%x",
				map[i].iova + offset);
			return map[i].iova + offset;
		}
	}

	*dbuf = fmt_dmabuf_get(fd);

	ret = fmt_dmabuf_get_iova(*dbuf, &iova, dev, attach, sgt);

	if (ret != 0) {
		fmt_debug(0, "fd: %d iova get failed", fd);
		return 0;
	}

	for (i = 0; i < FMT_FD_RESERVE; i++) {
		if (map[i].fd == -1) {
			map[i].fd = fd;
			map[i].iova = iova;
			break;
		}
	}

	iova += offset;

	fmt_debug(1, "iova 0x%x", iova);

	return iova;
}

