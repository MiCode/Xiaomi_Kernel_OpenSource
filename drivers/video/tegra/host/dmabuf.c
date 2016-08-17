/*
 * drivers/video/tegra/host/dmabuf.c
 *
 * Tegra Graphics Host DMA-BUF support
 *
 * Copyright (c) 2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/dma-buf.h>
#include <linux/nvhost.h>
#include "chip_support.h"
#include "nvhost_memmgr.h"

static inline struct dma_buf_attachment *to_dmabuf_att(struct mem_handle *h)
{
	return (struct dma_buf_attachment *)(((u32)h) & ~0x3);
}

static inline struct dma_buf *to_dmabuf(struct mem_handle *h)
{
	return to_dmabuf_att(h)->dmabuf;
}

static inline int to_dmabuf_fd(u32 id)
{
	return nvhost_memmgr_id(id) >> 2;
}
struct mem_handle *nvhost_dmabuf_alloc(size_t size, size_t align, int flags)
{
	/* TODO: Add allocation via DMA Mapping API */
	return NULL;
}

void nvhost_dmabuf_put(struct mem_handle *handle)
{
	struct dma_buf_attachment *attach = to_dmabuf_att(handle);
	struct dma_buf *dmabuf = attach->dmabuf;
	dma_buf_detach(dmabuf, attach);
	dma_buf_put(dmabuf);
}

struct sg_table *nvhost_dmabuf_pin(struct mem_handle *handle)
{
	return dma_buf_map_attachment(to_dmabuf_att(handle),
				DMA_BIDIRECTIONAL);
}

void nvhost_dmabuf_unpin(struct mem_handle *handle, struct sg_table *sgt)
{
	dma_buf_unmap_attachment(to_dmabuf_att(handle), sgt, DMA_BIDIRECTIONAL);
}


void *nvhost_dmabuf_mmap(struct mem_handle *handle)
{
	return dma_buf_vmap(to_dmabuf(handle));
}

void nvhost_dmabuf_munmap(struct mem_handle *handle, void *addr)
{
	dma_buf_vunmap(to_dmabuf(handle), addr);
}

void *nvhost_dmabuf_kmap(struct mem_handle *handle, unsigned int pagenum)
{
	return dma_buf_kmap(to_dmabuf(handle), pagenum);
}

void nvhost_dmabuf_kunmap(struct mem_handle *handle, unsigned int pagenum,
		void *addr)
{
	dma_buf_kunmap(to_dmabuf(handle), pagenum, addr);
}

struct mem_handle *nvhost_dmabuf_get(u32 id, struct platform_device *dev)
{
	struct mem_handle *h;
	struct dma_buf *buf;

	buf = dma_buf_get(to_dmabuf_fd(id));
	if (IS_ERR_OR_NULL(buf))
		return (struct mem_handle *)buf;
	else {
		h = (struct mem_handle *)dma_buf_attach(buf, &dev->dev);
		if (IS_ERR_OR_NULL(h))
			dma_buf_put(buf);
	}

	return (struct mem_handle *) ((u32)h | mem_mgr_type_dmabuf);
}

int nvhost_dmabuf_pin_array_ids(struct platform_device *dev,
		u32 *ids,
		u32 id_type_mask,
		u32 id_type,
		u32 count,
		struct nvhost_job_unpin *unpin_data,
		dma_addr_t *phys_addr) {
	int i;
	int pin_count = 0;
	int err;

	for (i = 0; i < count; i++) {
		struct mem_handle *handle;
		struct sg_table *sgt;

		if ((ids[i] & id_type_mask) != id_type)
			continue;

		handle = nvhost_dmabuf_get(ids[i], dev);

		if (IS_ERR(handle)) {
			err = PTR_ERR(handle);
			goto fail;
		}

		sgt = nvhost_dmabuf_pin(handle);
		if (IS_ERR_OR_NULL(sgt)) {
			nvhost_dmabuf_put(handle);
			err = PTR_ERR(sgt);
			goto fail;
		}

		phys_addr[i] = sg_dma_address(sgt->sgl);

		unpin_data[pin_count].h = handle;
		unpin_data[pin_count].mem = sgt;
		pin_count++;
	}
	return pin_count;
fail:
	while (pin_count) {
		pin_count--;
		nvhost_dmabuf_unpin(unpin_data[pin_count].h,
				unpin_data[pin_count].mem);
		nvhost_dmabuf_put(unpin_data[pin_count].h);
	}
	return err;
}
