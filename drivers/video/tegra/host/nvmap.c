/*
 * drivers/video/tegra/host/nvmap.c
 *
 * Tegra Graphics Host Nvmap support
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/nvmap.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/err.h>
#include <mach/tegra_smmu.h>
#include <trace/events/nvhost.h>
#include "nvmap.h"
#include "nvhost_job.h"
#include "chip_support.h"
#include "nvhost_allocator.h"

#define FLAG_NVHOST_MAPPED 1
#define FLAG_NVMAP_MAPPED 2
#define FLAG_CARVEOUT 3

struct nvhost_nvmap_as_data {
	struct device *dev;
	size_t len;
	struct sg_table *sgt;
	int pin_count;
	int flags;
	struct dma_buf_attachment *attach;
};

struct nvhost_nvmap_data {
	struct mutex lock;
	struct nvhost_nvmap_as_data *as[TEGRA_IOMMU_NUM_ASIDS];
	struct nvhost_comptags comptags;
	struct nvhost_allocator *comptag_allocator;
};

struct mem_mgr *nvhost_nvmap_alloc_mgr(void)
{
	return (struct mem_mgr *)0x1;
}

void nvhost_nvmap_put_mgr(struct mem_mgr *mgr)
{
	if ((ulong)mgr != 0x1)
		nvmap_client_put((struct nvmap_client *)mgr);
}

struct mem_mgr *nvhost_nvmap_get_mgr(struct mem_mgr *mgr)
{
	if ((ulong)mgr == 0x1)
		return (struct mem_mgr *)0x1;
	return (struct mem_mgr *)nvmap_client_get((struct nvmap_client *)mgr);
}

struct mem_mgr *nvhost_nvmap_get_mgr_file(int fd)
{
	return (struct mem_mgr *)nvmap_client_get_file(fd);
}

struct mem_handle *nvhost_nvmap_alloc(struct mem_mgr *mgr,
		size_t size, size_t align, int flags, unsigned int heap_mask)
{
	return (struct mem_handle *)nvmap_alloc_dmabuf(
			size, align, flags, heap_mask);
}

void nvhost_nvmap_put(struct mem_mgr *mgr, struct mem_handle *handle)
{
	if (!handle)
		return;
	dma_buf_put((struct dma_buf *)handle);
}

void delete_priv(void *_priv)
{
	struct nvhost_nvmap_data *priv = _priv;
	int i;
	for (i = 0; i < ARRAY_SIZE(priv->as); i++) {
		struct nvhost_nvmap_as_data *as = priv->as[i];
		if (!as)
			continue;
		if (as->sgt && as->flags & BIT(FLAG_CARVEOUT))
			nvmap_dmabuf_free_sg_table(NULL, as->sgt);
		kfree(as);
	}
	if (priv->comptags.lines) {
		BUG_ON(!priv->comptag_allocator);
		priv->comptag_allocator->free(priv->comptag_allocator,
					      priv->comptags.offset,
					      priv->comptags.lines);
	}
	kfree(priv);
}

struct sg_table *nvhost_nvmap_pin(struct mem_mgr *mgr,
		struct mem_handle *handle,
		struct device *dev,
		int rw_flag)
{
	struct nvhost_nvmap_data *priv;
	struct nvhost_nvmap_as_data *as_priv;
	struct sg_table *sgt;
	int ret;
	struct dma_buf *dmabuf = (struct dma_buf *)handle;
	static DEFINE_MUTEX(priv_lock);

	/* create the nvhost priv if needed */
	priv = nvmap_get_dmabuf_private(dmabuf);
	if (!priv) {
		mutex_lock(&priv_lock);
		priv = nvmap_get_dmabuf_private(dmabuf);
		if (priv)
			goto priv_exist_or_err;
		priv = kzalloc(sizeof(*priv), GFP_KERNEL);
		if (!priv) {
			priv = ERR_PTR(-ENOMEM);
			goto priv_exist_or_err;
		}
		mutex_init(&priv->lock);
		nvmap_set_dmabuf_private(dmabuf, priv, delete_priv);
priv_exist_or_err:
		mutex_unlock(&priv_lock);
	}
	if (IS_ERR(priv))
		return (struct sg_table *)priv;

	mutex_lock(&priv->lock);
	/* create the per-as part of the priv if needed */
	as_priv = priv->as[tegra_smmu_get_asid(dev)];
	if (!as_priv) {
		u64 size = 0, heap = 0;

		ret = nvmap_get_dmabuf_param(dmabuf,
				NVMAP_HANDLE_PARAM_SIZE, &size);
		if (ret) {
			mutex_unlock(&priv->lock);
			return ERR_PTR(ret);
		}

		ret = nvmap_get_dmabuf_param(dmabuf,
				NVMAP_HANDLE_PARAM_HEAP, &heap);
		if (ret) {
			mutex_unlock(&priv->lock);
			return ERR_PTR(ret);
		}

		as_priv = kzalloc(sizeof(*as_priv), GFP_KERNEL);
		if (!as_priv) {
			mutex_unlock(&priv->lock);
			return ERR_PTR(-ENOMEM);
		}
		if (heap & NVMAP_HEAP_CARVEOUT_MASK)
			as_priv->flags |= BIT(FLAG_CARVEOUT);
		as_priv->len = size;
		as_priv->dev = dev;
		priv->as[tegra_smmu_get_asid(dev)] = as_priv;
	}

	if (as_priv->flags & BIT(FLAG_CARVEOUT)) {
		if (!as_priv->sgt) {
			as_priv->sgt = nvmap_dmabuf_sg_table(dmabuf);
			if (IS_ERR(as_priv->sgt)) {
				sgt = as_priv->sgt;
				as_priv->sgt = NULL;
				mutex_unlock(&priv->lock);
				return sgt;
			}
		}
	} else if (as_priv->pin_count == 0) {
		as_priv->attach = dma_buf_attach(dmabuf, dev);
		if (IS_ERR(as_priv->attach)) {
			mutex_unlock(&priv->lock);
			return (struct sg_table *)as_priv->attach;
		}

		as_priv->sgt = dma_buf_map_attachment(as_priv->attach,
						      DMA_BIDIRECTIONAL);
		if (IS_ERR(as_priv->sgt)) {
			dma_buf_detach(dmabuf, as_priv->attach);
			mutex_unlock(&priv->lock);
			return as_priv->sgt;
		}
	}

	trace_nvhost_nvmap_pin(dev_name(dev), dmabuf, as_priv->len,
			       sg_dma_address(as_priv->sgt->sgl));
	sgt = as_priv->sgt;
	as_priv->pin_count++;
	mutex_unlock(&priv->lock);
	return sgt;
}

void nvhost_nvmap_unpin(struct mem_mgr *mgr, struct mem_handle *handle,
		struct device *dev, struct sg_table *sgt)
{
	struct dma_buf *dmabuf = (struct dma_buf *)handle;
	struct nvhost_nvmap_data *priv = nvmap_get_dmabuf_private(dmabuf);
	struct nvhost_nvmap_as_data *as_priv;
	dma_addr_t dma_addr;

	if (IS_ERR(priv) || !priv)
		return;

	mutex_lock(&priv->lock);
	as_priv = priv->as[tegra_smmu_get_asid(dev)];
	if (as_priv) {
		WARN_ON(as_priv->sgt != sgt);
		as_priv->pin_count--;
		WARN_ON(as_priv->pin_count < 0);
		dma_addr = sg_dma_address(as_priv->sgt->sgl);
		if (as_priv->pin_count == 0 &&
		    as_priv->flags & BIT(FLAG_CARVEOUT)) {
			/* do nothing. sgt free is deferred to delete_priv */
		} else if (as_priv->pin_count == 0) {
			dma_buf_unmap_attachment(as_priv->attach,
				as_priv->sgt, DMA_BIDIRECTIONAL);
			dma_buf_detach(dmabuf, as_priv->attach);
		}
		trace_nvhost_nvmap_unpin(dev_name(dev),
			dmabuf, as_priv->len, dma_addr);
	}
	mutex_unlock(&priv->lock);
}

void *nvhost_nvmap_mmap(struct mem_handle *handle)
{
	return dma_buf_vmap((struct dma_buf *)handle);
}

void nvhost_nvmap_munmap(struct mem_handle *handle, void *addr)
{
	dma_buf_vunmap((struct dma_buf *)handle, addr);
}

void *nvhost_nvmap_kmap(struct mem_handle *handle, unsigned int pagenum)
{
	return dma_buf_kmap((struct dma_buf *)handle, pagenum);
}

void nvhost_nvmap_kunmap(struct mem_handle *handle, unsigned int pagenum,
		void *addr)
{
	dma_buf_kunmap((struct dma_buf *)handle, pagenum, addr);
}

struct mem_handle *nvhost_nvmap_get(struct mem_mgr *mgr,
		ulong id, struct platform_device *dev)
{
#ifdef CONFIG_NVMAP_USE_FD_FOR_HANDLE
	return (struct mem_handle *)dma_buf_get(id);
#else
	return (struct mem_handle *)
		nvmap_dmabuf_export((struct nvmap_client *)mgr, id);
#endif
}

int nvhost_nvmap_get_param(struct mem_mgr *mgr, struct mem_handle *handle,
		u32 param, u64 *result)
{
	return nvmap_get_dmabuf_param(
			(struct dma_buf *)handle,
			param, result);
}

void nvhost_nvmap_get_comptags(struct mem_handle *mem,
			       struct nvhost_comptags *comptags)
{
	struct nvhost_nvmap_data *priv;

	priv = nvmap_get_dmabuf_private((struct dma_buf *)mem);

	BUG_ON(!priv || !comptags);

	*comptags = priv->comptags;
}

int nvhost_nvmap_alloc_comptags(struct mem_handle *mem,
				struct nvhost_allocator *allocator,
				int lines)
{
	int err;
	u32 offset = 0;
	struct nvhost_nvmap_data *priv;

	priv = nvmap_get_dmabuf_private((struct dma_buf *)mem);

	BUG_ON(!priv);
	BUG_ON(!lines);

	/* store the allocator so we can use it when we free the ctags */
	priv->comptag_allocator = allocator;
	err = allocator->alloc(allocator, &offset, lines);
	if (!err) {
		priv->comptags.lines = lines;
		priv->comptags.offset = offset;
	}
	return err;
}
