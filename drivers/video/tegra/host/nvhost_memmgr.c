/*
 * drivers/video/tegra/host/nvhost_memmgr.c
 *
 * Tegra Graphics Host Memory Management Abstraction
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

#include <linux/kernel.h>
#include <linux/err.h>

#include "nvhost_memmgr.h"
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
#include "nvmap.h"
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
#include "dmabuf.h"
#endif
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
		size_t size, size_t align, int flags)
{
	struct mem_handle *h = NULL;
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	h = nvhost_nvmap_alloc(mgr, size, align, flags);
#else
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	h = nvhost_dmabuf_alloc(mgr, size, align, flags);
#endif
#endif

	return h;
}

struct mem_handle *nvhost_memmgr_get(struct mem_mgr *mgr,
		u32 id, struct platform_device *dev)
{
	struct mem_handle *h = NULL;

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
	switch (nvhost_memmgr_type((u32)handle)) {
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
		struct mem_handle *handle)
{
	switch (nvhost_memmgr_type((u32)handle)) {
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	case mem_mgr_type_nvmap:
		return nvhost_nvmap_pin(mgr, handle);
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
		struct mem_handle *handle, struct sg_table *sgt)
{
	switch (nvhost_memmgr_type((u32)handle)) {
#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	case mem_mgr_type_nvmap:
		nvhost_nvmap_unpin(mgr, handle, sgt);
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
	switch (nvhost_memmgr_type((u32)handle)) {
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
	switch (nvhost_memmgr_type((u32)handle)) {
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

void *nvhost_memmgr_kmap(struct mem_handle *handle, unsigned int pagenum)
{
	switch (nvhost_memmgr_type((u32)handle)) {
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
	switch (nvhost_memmgr_type((u32)handle)) {
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

int nvhost_memmgr_pin_array_ids(struct mem_mgr *mgr,
		struct platform_device *dev,
		u32 *ids,
		dma_addr_t *phys_addr,
		u32 count,
		struct nvhost_job_unpin *unpin_data)
{
	int pin_count = 0;

#ifdef CONFIG_TEGRA_GRHOST_USE_NVMAP
	{
		int nvmap_count = 0;
		nvmap_count = nvhost_nvmap_pin_array_ids(mgr,
			ids, MEMMGR_TYPE_MASK,
			mem_mgr_type_nvmap,
			count, unpin_data,
			phys_addr);
		if (nvmap_count < 0)
			return nvmap_count;
		pin_count += nvmap_count;
	}
#endif
#ifdef CONFIG_TEGRA_GRHOST_USE_DMABUF
	{
		int dmabuf_count = 0;
		dmabuf_count = nvhost_dmabuf_pin_array_ids(dev,
			ids, MEMMGR_TYPE_MASK,
			mem_mgr_type_dmabuf,
			count, &unpin_data[pin_count],
			phys_addr);

		if (dmabuf_count < 0) {
			/* clean up previous handles */
			while (pin_count) {
				pin_count--;
				/* unpin, put */
				nvhost_memmgr_unpin(mgr,
						unpin_data[pin_count].h,
						unpin_data[pin_count].mem);
				nvhost_memmgr_put(mgr,
						unpin_data[pin_count].h);
			}
			return dmabuf_count;
		}
		pin_count += dmabuf_count;
	}
#endif
	return pin_count;
}

static const struct nvhost_mem_ops mem_ops = {
	.alloc_mgr = nvhost_memmgr_alloc_mgr,
	.put_mgr = nvhost_memmgr_put_mgr,
	.get_mgr = nvhost_memmgr_get_mgr,
	.get_mgr_file = nvhost_memmgr_get_mgr_file,
	.alloc = nvhost_memmgr_alloc,
	.get = nvhost_memmgr_get,
	.put = nvhost_memmgr_put,
	.pin = nvhost_memmgr_pin,
	.unpin = nvhost_memmgr_unpin,
	.mmap = nvhost_memmgr_mmap,
	.munmap = nvhost_memmgr_munmap,
	.kmap = nvhost_memmgr_kmap,
	.kunmap = nvhost_memmgr_kunmap,
	.pin_array_ids = nvhost_memmgr_pin_array_ids,
};

int nvhost_memmgr_init(struct nvhost_chip_support *chip)
{
	chip->mem = mem_ops;
	return 0;
}

