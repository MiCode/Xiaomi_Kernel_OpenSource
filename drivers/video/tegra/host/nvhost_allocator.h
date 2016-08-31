/*
 * drivers/video/tegra/host/nvhost_allocator.h
 *
 * nvhost allocator
 *
 * Copyright (c) 2011, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef __NVHOST_ALLOCATOR_H__
#define __NVHOST_ALLOCATOR_H__

#include <linux/rbtree.h>
#include <linux/rwsem.h>
#include <linux/slab.h>

/* #define ALLOCATOR_DEBUG */

struct allocator_block;

/* main struct */
struct nvhost_allocator {

	char name[32];			/* name for allocator */
	struct rb_root rb_root;		/* rb tree root for blocks */

	u32 base;			/* min value of this linear space */
	u32 limit;			/* max value = limit - 1 */
	u32 align;			/* alignment size, power of 2 */

	struct nvhost_alloc_block *block_first;	/* first block in list */
	struct nvhost_alloc_block *block_recent; /* last visited block */

	u32 first_free_addr;		/* first free addr,
					   non-contigous allocation preferred start,
					   in order to pick up small holes */
	u32 last_free_addr;		/* last free addr,
					   contiguous allocation preferred start */
	u32 cached_hole_size;		/* max free hole size up to last_free_addr */
	u32 block_count;		/* number of blocks */

	struct rw_semaphore rw_sema;	/* lock */
	struct kmem_cache *block_cache;	/* slab cache */

	/* if enabled, constrain to [base, limit) */
	struct {
		bool enable;
		u32 base;
		u32 limit;
	} constraint;

	int (*alloc)(struct nvhost_allocator *allocator,
		u32 *addr, u32 len);
	int (*alloc_nc)(struct nvhost_allocator *allocator,
		u32 *addr, u32 len,
		struct nvhost_alloc_block **pblock);
	int (*free)(struct nvhost_allocator *allocator,
		u32 addr, u32 len);
	void (*free_nc)(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block);

	int (*constrain)(struct nvhost_allocator *a,
			 bool enable,
			 u32 base, u32 limit);
};

/* a block of linear space range [start, end) */
struct nvhost_alloc_block {
	struct nvhost_allocator *allocator;	/* parent allocator */
	struct rb_node rb;			/* rb tree node */

	u32 start;				/* linear space range [start, end) */
	u32 end;

	void *priv;				/* backing structure for this linear space block
						   page table, comp tag, etc */

	struct nvhost_alloc_block *prev;	/* prev block with lower address */
	struct nvhost_alloc_block *next;	/* next block with higher address */

	bool nc_block;
	struct nvhost_alloc_block *nc_prev;	/* prev block for non-contiguous allocation */
	struct nvhost_alloc_block *nc_next;	/* next block for non-contiguous allocation */
};

int nvhost_allocator_init(struct nvhost_allocator *allocator,
			const char *name, u32 base, u32 size, u32 align);
void nvhost_allocator_destroy(struct nvhost_allocator *allocator);

int nvhost_block_alloc(struct nvhost_allocator *allocator,
			u32 *addr, u32 len);
int nvhost_block_alloc_nc(struct nvhost_allocator *allocator,
			u32 *addr, u32 len,
			struct nvhost_alloc_block **pblock);

int nvhost_block_free(struct nvhost_allocator *allocator,
			u32 addr, u32 len);
void nvhost_block_free_nc(struct nvhost_allocator *allocator,
			struct nvhost_alloc_block *block);

#if defined(ALLOCATOR_DEBUG)

#define allocator_dbg(alloctor, format, arg...)				\
do {								\
	if (1)							\
		printk(KERN_DEBUG "nvhost_allocator (%s) %s: " format "\n", alloctor->name, __func__, ##arg);\
} while (0)

static inline void
nvhost_allocator_dump(struct nvhost_allocator *allocator) {
	struct nvhost_alloc_block *block;
	u32 count = 0;

	down_read(&allocator->rw_sema);
	for (block = allocator->block_first; block; block = block->next) {
		allocator_dbg(allocator, "block %d - %d:%d, nc %d",
			count++, block->start, block->end, block->nc_block);

		if (block->prev)
			BUG_ON(block->prev->end > block->start);
		if (block->next)
			BUG_ON(block->next->start < block->end);
	}
	allocator_dbg(allocator, "tracked count %d, actual count %d",
		allocator->block_count, count);
	allocator_dbg(allocator, "first block %d:%d",
		allocator->block_first ? allocator->block_first->start : -1,
		allocator->block_first ? allocator->block_first->end : -1);
	allocator_dbg(allocator, "first free addr %d", allocator->first_free_addr);
	allocator_dbg(allocator, "last free addr %d", allocator->last_free_addr);
	allocator_dbg(allocator, "cached hole size %d", allocator->cached_hole_size);
	up_read(&allocator->rw_sema);

	BUG_ON(count != allocator->block_count);
}

static inline void
nvhost_allocator_dump_nc_list(
		struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block)
{
	down_read(&allocator->rw_sema);
	while (block) {
		printk(KERN_DEBUG "non-contiguous block %d:%d\n",
			block->start, block->end);
		block = block->nc_next;
	}
	up_read(&allocator->rw_sema);
}

void nvhost_allocator_test(void);

#else /* ALLOCATOR_DEBUG */

#define allocator_dbg(format, arg...)

#endif /* ALLOCATOR_DEBUG */

#endif /*__NVHOST_ALLOCATOR_H__ */
