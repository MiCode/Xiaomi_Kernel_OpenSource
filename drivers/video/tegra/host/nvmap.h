/*
 * drivers/video/tegra/host/nvmap.h
 *
 * Tegra Graphics Host nvmap memory manager
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
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
		size_t size, size_t align, int flags, unsigned int heap_flags);
void nvhost_nvmap_put(struct mem_mgr *mgr, struct mem_handle *handle);
struct sg_table *nvhost_nvmap_pin(struct mem_mgr *mgr,
		struct mem_handle *handle, struct device *dev, int rw_flag);
void nvhost_nvmap_unpin(struct mem_mgr *mgr, struct mem_handle *handle,
		struct device *dev, struct sg_table *sgt);
void *nvhost_nvmap_mmap(struct mem_handle *handle);
void nvhost_nvmap_munmap(struct mem_handle *handle, void *addr);
void *nvhost_nvmap_kmap(struct mem_handle *handle, unsigned int pagenum);
void nvhost_nvmap_kunmap(struct mem_handle *handle, unsigned int pagenum,
		void *addr);
struct mem_handle *nvhost_nvmap_get(struct mem_mgr *mgr,
		ulong id, struct platform_device *dev);
int nvhost_nvmap_get_param(struct mem_mgr *mgr, struct mem_handle *handle,
			   u32 param, u64 *result);
phys_addr_t nvhost_nvmap_get_addr_from_id(ulong id);

void nvhost_nvmap_unpin_id(struct mem_mgr *mgr, ulong id);
void nvhost_nvmap_get_comptags(struct mem_handle *mem,
			       struct nvhost_comptags *comptags);
int nvhost_nvmap_alloc_comptags(struct mem_handle *mem,
				struct nvhost_allocator *allocator,
				int lines);

#endif
