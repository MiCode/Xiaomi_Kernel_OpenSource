/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_MEMALLOC_H
#define	_LINUX_MEMALLOC_H

#include <linux/mutex.h>
#include <linux/genalloc.h>
#include <linux/rbtree.h>

struct mem_pool {
	struct mutex pool_mutex;
	struct gen_pool *gpool;
	phys_addr_t paddr;
	unsigned long size;
	unsigned long free;
	unsigned int id;
};

struct alloc {
	struct rb_node rb_node;
	/*
	 * The physical address may be used for lookup in the tree so the
	 * 'virtual address' needs to be able to accomodate larger physical
	 * addresses.
	 */
	phys_addr_t vaddr;
	phys_addr_t paddr;
	struct mem_pool *mpool;
	unsigned long len;
	void *caller;
};

struct mem_pool *initialize_memory_pool(phys_addr_t start,
	unsigned long size, int mem_type);

void *allocate_contiguous_memory(unsigned long size,
	int mem_type, unsigned long align, int cached);

phys_addr_t _allocate_contiguous_memory_nomap(unsigned long size,
	int mem_type, unsigned long align, void *caller);

phys_addr_t allocate_contiguous_memory_nomap(unsigned long size,
	int mem_type, unsigned long align);

void free_contiguous_memory(void *addr);
void free_contiguous_memory_by_paddr(phys_addr_t paddr);

phys_addr_t memory_pool_node_paddr(void *vaddr);

unsigned long memory_pool_node_len(void *vaddr);

int memory_pool_init(void);
#endif	/* _LINUX_MEMALLOC_H */
