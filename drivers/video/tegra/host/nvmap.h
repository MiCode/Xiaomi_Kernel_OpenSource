/*
 * drivers/video/tegra/host/nvmap.h
 *
 * Tegra Graphics Host nvmap memory manager
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation.
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

#ifndef __NVHOST_NVMAP_H
#define __NVHOST_NVMAP_H

#include "nvhost_memmgr.h"

struct nvhost_chip_support;
struct platform_device;
struct sg_table;

struct mem_mgr *nvhost_nvmap_alloc_mgr(void);
void nvhost_nvmap_put_mgr(struct mem_mgr *mgr);
struct mem_mgr *nvhost_nvmap_get_mgr(struct mem_mgr *mgr);
struct mem_mgr *nvhost_nvmap_get_mgr_file(int fd);
struct mem_handle *nvhost_nvmap_alloc(struct mem_mgr *mgr,
		size_t size, size_t align, int flags);
void nvhost_nvmap_put(struct mem_mgr *mgr, struct mem_handle *handle);
struct sg_table *nvhost_nvmap_pin(struct mem_mgr *mgr,
		struct mem_handle *handle);
void nvhost_nvmap_unpin(struct mem_mgr *mgr,
		struct mem_handle *handle, struct sg_table *sgt);
void *nvhost_nvmap_mmap(struct mem_handle *handle);
void nvhost_nvmap_munmap(struct mem_handle *handle, void *addr);
void *nvhost_nvmap_kmap(struct mem_handle *handle, unsigned int pagenum);
void nvhost_nvmap_kunmap(struct mem_handle *handle, unsigned int pagenum,
		void *addr);
struct mem_handle *nvhost_nvmap_get(struct mem_mgr *mgr,
		u32 id, struct platform_device *dev);

phys_addr_t nvhost_nvmap_get_addr_from_id(u32 id);

int nvhost_nvmap_pin_array_ids(struct mem_mgr *mgr,
		u32 *ids,
		u32 id_type_mask,
		u32 id_type,
		u32 count,
		struct nvhost_job_unpin *unpin_data,
		dma_addr_t *phys_addr);

void nvhost_nvmap_unpin_id(struct mem_mgr *mgr, u32 id);


#endif
