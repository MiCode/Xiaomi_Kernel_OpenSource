// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "vdec_fmt_dmabuf.h"
#include "vdec_fmt_utils.h"

int fmt_dmabuf_get_iova(struct dma_buf *dbuf, u64 *iova,
	struct device *dev, struct dma_buf_attachment **attach, struct sg_table **sgt,
	bool cache_sync)
{
	*attach = dma_buf_attach(dbuf, dev);
	if (IS_ERR(*attach)) {
		fmt_debug(0, "attach fail, return\n");
		*attach = NULL;
		return -1;
	}

	if (!cache_sync)
		(*attach)->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	*sgt = dma_buf_map_attachment(*attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(*sgt)) {
		fmt_debug(0, "map failed, detach and return\n");
		dma_buf_detach(dbuf, *attach);
		*sgt = NULL;
		return -1;
	}

	*iova = sg_dma_address((*sgt)->sgl);
	fmt_debug(1, "dbuf %p attach %p sgt %p iova 0x%llx\n", dbuf, *attach, *sgt, *iova);
	return 0;
}

void fmt_dmabuf_free_iova(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach, struct sg_table *sgt)
{
	if (attach == NULL || sgt == NULL) {
		fmt_debug(0, "attach or sgt null, not need to free iova");
		return;
	}
	fmt_debug(1, "dbuf %p attach %p sgt %p\n", dbuf, attach, sgt);
	dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
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
	fmt_debug(1, "dbuf %p fd %d\n", dbuf, fd);

	return dbuf;
}

void fmt_dmabuf_put(struct dma_buf *dbuf)
{
	if (!dbuf) {
		fmt_debug(0, "dma_buf null, no need to put.");
		return;
	}
	fmt_debug(1, "dbuf %p\n", dbuf);
	dma_buf_put(dbuf);
}

u64 fmt_translate_fd(u64 fd, u32 offset, struct dmabufmap map[], struct device *dev,
	struct dma_buf **dbuf, struct dma_buf_attachment **attach, struct sg_table **sgt,
	bool cache_sync)
{
	int i, ret;
	u64 iova = 0;

	if (fd == 0)
		return 0;

	for (i = 0; i < FMT_FD_RESERVE; i++) {
		if (fd == map[i].fd) {
			fmt_debug(3, "quick search iova 0x%llx",
				map[i].iova + offset);
			return map[i].iova + offset;
		}
	}

	*dbuf = fmt_dmabuf_get(fd);

	ret = fmt_dmabuf_get_iova(*dbuf, &iova, dev, attach, sgt, cache_sync);

	if (ret != 0) {
		fmt_debug(0, "fd: %lu iova get failed", fd);
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

	fmt_debug(1, "iova 0x%llx", iova);

	return iova;
}

