/*
 * drivers/video/tegra/host/nvhost_memmgr.c
 *
 * Tegra Graphics Host Memory Management Abstraction
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

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/bug.h>
#include <linux/platform_device.h>

#include "nvhost_memmgr.h"
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
#include "nvmap.h"
#include <linux/nvmap.h>
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
#include "dmabuf.h"
#endif
#include <linux/sort.h>
#include "chip_support.h"

struct mem_mgr *nvhost_memmgr_alloc_mgr(void)
{
	struct mem_mgr *mgr = NULL;
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	mgr = nvhost_nvmap_alloc_mgr();
#else
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	mgr = (struct mem_mgr)1;
#endif
#endif

	return mgr;
}

void nvhost_memmgr_put_mgr(struct mem_mgr *mgr)
{
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	nvhost_nvmap_put_mgr(mgr);
#else
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	mgr = (struct mem_mgr)1;
#endif
#endif
}

struct mem_mgr *nvhost_memmgr_get_mgr(struct mem_mgr *_mgr)
{
	struct mem_mgr *mgr = NULL;
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	mgr = nvhost_nvmap_get_mgr(_mgr);
#else
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	mgr = (struct mem_mgr)1;
#endif
#endif

	return mgr;
}

struct mem_mgr *nvhost_memmgr_get_mgr_file(int fd)
{
	struct mem_mgr *mgr = NULL;
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	mgr = nvhost_nvmap_get_mgr_file(fd);
#else
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	mgr = (struct mem_mgr)1;
#endif
#endif

	return mgr;
}

struct mem_handle *nvhost_memmgr_alloc(struct mem_mgr *mgr,
       size_t size, size_t align, int flags, unsigned int heap_mask)
{
	struct mem_handle *h = NULL;
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	h = nvhost_nvmap_alloc(mgr, size, align, flags, heap_mask);
#else
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	h = nvhost_dmabuf_alloc(mgr, size, align, flags);
#endif
#endif

	return h;
}

struct mem_handle *nvhost_memmgr_get(struct mem_mgr *mgr,
		ulong id, struct platform_device *dev)
{
	struct mem_handle *h = ERR_PTR(-EINVAL);

	switch (nvhost_memmgr_type(id)) {
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	case mem_mgr_type_nvmap:
		h = (struct mem_handle *) nvhost_nvmap_get(mgr, id, dev);
		break;
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	case mem_mgr_type_dmabuf:
		h = (struct mem_handle *) nvhost_dmabuf_get(id, dev);
		break;
#endif
	default:
		break;
	}

	return h;
}

void nvhost_memmgr_put(struct mem_mgr *mgr, struct mem_handle *handle)
{
	switch (nvhost_memmgr_type((u32)((uintptr_t)handle))) {
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	case mem_mgr_type_nvmap:
		nvhost_nvmap_put(mgr, handle);
		break;
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	case mem_mgr_type_dmabuf:
		nvhost_dmabuf_put(handle);
		break;
#endif
	default:
		break;
	}
}

struct sg_table *nvhost_memmgr_pin(struct mem_mgr *mgr,
		struct mem_handle *handle, struct device *dev, int rw_flag)
{
	switch (nvhost_memmgr_type((u32)((uintptr_t)handle))) {
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	case mem_mgr_type_nvmap:
		return nvhost_nvmap_pin(mgr, handle, dev, rw_flag);
		break;
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	case mem_mgr_type_dmabuf:
		return nvhost_dmabuf_pin(handle);
		break;
#endif
	default:
		return 0;
		break;
	}
}

void nvhost_memmgr_unpin(struct mem_mgr *mgr,
		struct mem_handle *handle, struct device *dev,
		struct sg_table *sgt)
{
	switch (nvhost_memmgr_type((u32)((uintptr_t)handle))) {
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	case mem_mgr_type_nvmap:
		nvhost_nvmap_unpin(mgr, handle, dev, sgt);
		break;
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	case mem_mgr_type_dmabuf:
		nvhost_dmabuf_unpin(handle, sgt);
		break;
#endif
	default:
		break;
	}
}

void *nvhost_memmgr_mmap(struct mem_handle *handle)
{
	switch (nvhost_memmgr_type((u32)((uintptr_t)handle))) {
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	case mem_mgr_type_nvmap:
		return nvhost_nvmap_mmap(handle);
		break;
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	case mem_mgr_type_dmabuf:
		return nvhost_dmabuf_mmap(handle);
		break;
#endif
	default:
		return 0;
		break;
	}
}

void nvhost_memmgr_munmap(struct mem_handle *handle, void *addr)
{
	switch (nvhost_memmgr_type((u32)((uintptr_t)handle))) {
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	case mem_mgr_type_nvmap:
		nvhost_nvmap_munmap(handle, addr);
		break;
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	case mem_mgr_type_dmabuf:
		nvhost_dmabuf_munmap(handle, addr);
		break;
#endif
	default:
		break;
	}
}

int nvhost_memmgr_get_param(struct mem_mgr *mem_mgr,
			    struct mem_handle *mem_handle,
			    u32 param, u64 *result)
{
#ifndef CONFIG_ARM64
	switch (nvhost_memmgr_type((u32)mem_handle)) {
#else
	switch (nvhost_memmgr_type((u32)((uintptr_t)mem_handle))) {
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	case mem_mgr_type_nvmap:
		return nvhost_nvmap_get_param(mem_mgr, mem_handle,
					      param, result);
		break;
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	case mem_mgr_type_dmabuf:
		return nvhost_dmabuf_get_param(mem_mgr, mem_handle,
					       param, result);
		break;
#endif
	default:
		break;
	}
	return -EINVAL;
}

void *nvhost_memmgr_kmap(struct mem_handle *handle, unsigned int pagenum)
{
	switch (nvhost_memmgr_type((u32)((uintptr_t)handle))) {
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	case mem_mgr_type_nvmap:
		return nvhost_nvmap_kmap(handle, pagenum);
		break;
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	case mem_mgr_type_dmabuf:
		return nvhost_dmabuf_kmap(handle, pagenum);
		break;
#endif
	default:
		return 0;
		break;
	}
}

void nvhost_memmgr_kunmap(struct mem_handle *handle, unsigned int pagenum,
		void *addr)
{
	switch (nvhost_memmgr_type((u32)((uintptr_t)handle))) {
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	case mem_mgr_type_nvmap:
		nvhost_nvmap_kunmap(handle, pagenum, addr);
		break;
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	case mem_mgr_type_dmabuf:
		nvhost_dmabuf_kunmap(handle, pagenum, addr);
		break;
#endif
	default:
		break;
	}
}

static int id_cmp(const void *_id1, const void *_id2)
{
	u32 id1 = ((struct nvhost_memmgr_pinid *)_id1)->id;
	u32 id2 = ((struct nvhost_memmgr_pinid *)_id2)->id;

	if (id1 < id2)
		return -1;
	if (id1 > id2)
		return 1;

	return 0;
}

int nvhost_memmgr_pin_array_ids(struct mem_mgr *mgr,
		struct platform_device *dev,
		struct nvhost_memmgr_pinid *ids,
		dma_addr_t *phys_addr,
		u32 count,
		struct nvhost_job_unpin *unpin_data)
{
	int i, pin_count = 0;
	struct sg_table *sgt;
	struct mem_handle *h;
	u32 prev_id = 0;
	dma_addr_t prev_addr = 0;

	for (i = 0; i < count; i++)
		ids[i].index = i;

	sort(ids, count, sizeof(*ids), id_cmp, NULL);

	for (i = 0; i < count; i++) {
		if (ids[i].id == prev_id) {
			phys_addr[ids[i].index] = prev_addr;
			continue;
		}

		h = nvhost_memmgr_get(mgr, ids[i].id, dev);
		if (IS_ERR(h))
			return -EINVAL;

		sgt = nvhost_memmgr_pin(mgr, h, &dev->dev, mem_flag_none);
		if (IS_ERR(sgt))
			return PTR_ERR(sgt);

		phys_addr[ids[i].index] = nvhost_memmgr_dma_addr(sgt);
		unpin_data[pin_count].h = h;
		unpin_data[pin_count++].mem = sgt;

		prev_id = ids[i].id;
		prev_addr = phys_addr[ids[i].index];
	}
	return pin_count;
}

struct sg_table *nvhost_memmgr_sg_table(struct mem_mgr *mgr,
		struct mem_handle *handle)
{
	switch (nvhost_memmgr_type((ulong)handle)) {
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	case mem_mgr_type_nvmap:
		return nvmap_dmabuf_sg_table((struct dma_buf *)handle);
		break;
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	case mem_mgr_type_dmabuf:
		WARN_ON(1);
		break;
#endif
	default:
		break;
	}

	return NULL;

}

void nvhost_memmgr_free_sg_table(struct mem_mgr *mgr,
		struct mem_handle *handle, struct sg_table *sgt)
{
	switch (nvhost_memmgr_type((ulong)handle)) {
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	case mem_mgr_type_nvmap:
		return nvmap_dmabuf_free_sg_table(
					(struct dma_buf *)handle, sgt);
		break;
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	case mem_mgr_type_dmabuf:
		WARN_ON(1);
		break;
#endif
	default:
		break;
	}
	return;
}

void nvhost_memmgr_get_comptags(struct mem_handle *mem,
				struct nvhost_comptags *comptags)
{
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	return nvhost_nvmap_get_comptags(mem, comptags);
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	WARN_ON(1);
#endif
}

int nvhost_memmgr_alloc_comptags(struct mem_handle *mem,
				 struct nvhost_allocator *allocator,
				 int lines)
{
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	return nvhost_nvmap_alloc_comptags(mem, allocator, lines);
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	WARN_ON(1);
#endif
}

int nvhost_memmgr_init(struct nvhost_chip_support *chip)
{
	return 0;
}

