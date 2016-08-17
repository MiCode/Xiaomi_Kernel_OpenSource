/*
 * drivers/video/tegra/host/nvmap.c
 *
 * Tegra Graphics Host Nvmap support
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

#include <linux/nvmap.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/err.h>
#include "nvmap.h"
#include "nvhost_job.h"


struct mem_mgr *nvhost_nvmap_alloc_mgr(void)
{
	return (struct mem_mgr *)nvmap_create_client(nvmap_dev, "nvhost");
}

void nvhost_nvmap_put_mgr(struct mem_mgr *mgr)
{
	nvmap_client_put((struct nvmap_client *)mgr);
}

struct mem_mgr *nvhost_nvmap_get_mgr(struct mem_mgr *mgr)
{
	return (struct mem_mgr *)nvmap_client_get((struct nvmap_client *)mgr);
}

struct mem_mgr *nvhost_nvmap_get_mgr_file(int fd)
{
	return (struct mem_mgr *)nvmap_client_get_file(fd);
}

struct mem_handle *nvhost_nvmap_alloc(struct mem_mgr *mgr,
		size_t size, size_t align, int flags)
{
	return (struct mem_handle *)nvmap_alloc((struct nvmap_client *)mgr,
			size, align, flags, 0);
}

void nvhost_nvmap_put(struct mem_mgr *mgr, struct mem_handle *handle)
{
	_nvmap_free((struct nvmap_client *)mgr,
			(struct nvmap_handle_ref *)handle);
}

static struct scatterlist *sg_kmalloc(unsigned int nents, gfp_t gfp_mask)
{
	return (struct scatterlist *)gfp_mask;
}

struct sg_table *nvhost_nvmap_pin(struct mem_mgr *mgr,
		struct mem_handle *handle)
{
	int err = 0;
	dma_addr_t ret = 0;
	struct sg_table *sgt = kmalloc(sizeof(*sgt) + sizeof(*sgt->sgl),
			GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	err = __sg_alloc_table(sgt, 1, 1, (gfp_t)(sgt+1), sg_kmalloc);
	if (err) {
		kfree(sgt);
		return ERR_PTR(err);
	}

	ret = nvmap_pin((struct nvmap_client *)mgr,
			(struct nvmap_handle_ref *)handle);
	if (IS_ERR_VALUE(ret)) {
		kfree(sgt);
		return ERR_PTR(ret);
	}
	sg_dma_address(sgt->sgl) = ret;

	return sgt;
}

void nvhost_nvmap_unpin(struct mem_mgr *mgr,
		struct mem_handle *handle, struct sg_table *sgt)
{
	kfree(sgt);
	return nvmap_unpin((struct nvmap_client *)mgr,
			(struct nvmap_handle_ref *)handle);
}

void *nvhost_nvmap_mmap(struct mem_handle *handle)
{
	return nvmap_mmap((struct nvmap_handle_ref *)handle);
}

void nvhost_nvmap_munmap(struct mem_handle *handle, void *addr)
{
	nvmap_munmap((struct nvmap_handle_ref *)handle, addr);
}

void *nvhost_nvmap_kmap(struct mem_handle *handle, unsigned int pagenum)
{
	return nvmap_kmap((struct nvmap_handle_ref *)handle, pagenum);
}

void nvhost_nvmap_kunmap(struct mem_handle *handle, unsigned int pagenum,
		void *addr)
{
	nvmap_kunmap((struct nvmap_handle_ref *)handle, pagenum, addr);
}

int nvhost_nvmap_pin_array_ids(struct mem_mgr *mgr,
		u32 *ids,
		u32 id_type_mask,
		u32 id_type,
		u32 count,
		struct nvhost_job_unpin *unpin_data,
		dma_addr_t *phys_addr)
{
	int i;
	int result = 0;
	struct nvmap_handle **unique_handles;
	struct nvmap_handle_ref **unique_handle_refs;
	void *ptrs = kmalloc(sizeof(void *) * count * 2,
			GFP_KERNEL);

	if (!ptrs)
		return -ENOMEM;

	unique_handles = (struct nvmap_handle **) ptrs;
	unique_handle_refs = (struct nvmap_handle_ref **)
			&unique_handles[count];

	result = nvmap_pin_array((struct nvmap_client *)mgr,
		    (long unsigned *)ids, id_type_mask, id_type, count,
		    unique_handles,
		    unique_handle_refs);

	if (result < 0)
		goto fail;

	BUG_ON(result > count);

	for (i = 0; i < result; i++)
		unpin_data[i].h = (struct mem_handle *)unique_handle_refs[i];

	for (i = 0; i < count; i++) {
		if ((ids[i] & id_type_mask) == id_type)
			phys_addr[i] = (dma_addr_t)_nvmap_get_addr_from_id(ids[i]);
	}

fail:
	kfree(ptrs);
	return result;
}

struct mem_handle *nvhost_nvmap_get(struct mem_mgr *mgr,
		u32 id, struct platform_device *dev)
{
	return (struct mem_handle *)
		nvmap_duplicate_handle_id((struct nvmap_client *)mgr, id);
}

