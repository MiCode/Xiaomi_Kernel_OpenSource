/*
 * drivers/video/tegra/nvmap/nvmap_heap.h
 *
 * GPU heap allocator.
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __NVMAP_HEAP_H
#define __NVMAP_HEAP_H

struct device;
struct nvmap_heap;
struct attribute_group;

struct nvmap_heap_block {
	phys_addr_t	base;
	unsigned int	type;
	struct nvmap_handle *handle;
};

#define NVMAP_HEAP_MIN_BUDDY_SIZE	8192

struct nvmap_heap *nvmap_heap_create(struct device *parent, const char *name,
				     phys_addr_t base, size_t len,
				     unsigned int buddy_size, void *arg);

void nvmap_heap_destroy(struct nvmap_heap *heap);

void *nvmap_heap_device_to_arg(struct device *dev);

void *nvmap_heap_to_arg(struct nvmap_heap *heap);

struct nvmap_heap_block *nvmap_heap_alloc(struct nvmap_heap *heap,
					  struct nvmap_handle *handle);

struct nvmap_heap *nvmap_block_to_heap(struct nvmap_heap_block *b);

void nvmap_heap_free(struct nvmap_heap_block *block);

int nvmap_heap_create_group(struct nvmap_heap *heap,
			    const struct attribute_group *grp);

void nvmap_heap_remove_group(struct nvmap_heap *heap,
			     const struct attribute_group *grp);

int __init nvmap_heap_init(void);

void nvmap_heap_deinit(void);

int nvmap_flush_heap_block(struct nvmap_client *client,
	struct nvmap_heap_block *block, size_t len, unsigned int prot);

#endif
