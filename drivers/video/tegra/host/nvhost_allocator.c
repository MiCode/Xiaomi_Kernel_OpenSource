/*
 * drivers/video/tegra/host/nvhost_allocator.c
 *
 * nvhost allocator
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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

#include "nvhost_allocator.h"

static inline void link_block_list(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block,
		struct nvhost_alloc_block *prev,
		struct rb_node *rb_parent);
static inline void link_block_rb(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block,
		struct rb_node **rb_link,
		struct rb_node *rb_parent);
static void link_block(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block,
		struct nvhost_alloc_block *prev, struct rb_node **rb_link,
		struct rb_node *rb_parent);
static void insert_block(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block);

static void unlink_block(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block,
		struct nvhost_alloc_block *prev);
static struct nvhost_alloc_block *unlink_blocks(
		struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block,
		struct nvhost_alloc_block *prev, u32 end);

static struct nvhost_alloc_block *find_block(
		struct nvhost_allocator *allocator, u32 addr);
static struct nvhost_alloc_block *find_block_prev(
		struct nvhost_allocator *allocator, u32 addr,
		struct nvhost_alloc_block **pprev);
static struct nvhost_alloc_block *find_block_prepare(
		struct nvhost_allocator *allocator, u32 addr,
		struct nvhost_alloc_block **pprev, struct rb_node ***rb_link,
		struct rb_node **rb_parent);

static u32 check_free_space(u32 addr, u32 limit, u32 len, u32 align);
static void update_free_addr_cache(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block,
		u32 addr, u32 len, bool free);
static int find_free_area(struct nvhost_allocator *allocator,
		u32 *addr, u32 len);
static int find_free_area_nc(struct nvhost_allocator *allocator,
		u32 *addr, u32 *len);

static void adjust_block(struct nvhost_alloc_block *block,
		u32 start, u32 end,
		struct nvhost_alloc_block *insert);
static struct nvhost_alloc_block *merge_block(
		struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block, u32 addr, u32 end);
static int split_block(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block,
		u32 addr, int new_below);

static int block_alloc_single_locked(struct nvhost_allocator *allocator,
		u32 *addr, u32 len);
static int block_alloc_list_locked(struct nvhost_allocator *allocator,
		u32 *addr, u32 len,
		struct nvhost_alloc_block **pblock);
static int block_free_locked(struct nvhost_allocator *allocator,
		u32 addr, u32 len);
static void block_free_list_locked(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *list);

/* link a block into allocator block list */
static inline void link_block_list(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block,
		struct nvhost_alloc_block *prev,
		struct rb_node *rb_parent)
{
	struct nvhost_alloc_block *next;

	block->prev = prev;
	if (prev) {
		next = prev->next;
		prev->next = block;
	} else {
		allocator->block_first = block;
		if (rb_parent)
			next = rb_entry(rb_parent,
					struct nvhost_alloc_block, rb);
		else
			next = NULL;
	}
	block->next = next;
	if (next)
		next->prev = block;
}

/* link a block into allocator rb tree */
static inline void link_block_rb(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block, struct rb_node **rb_link,
		struct rb_node *rb_parent)
{
	rb_link_node(&block->rb, rb_parent, rb_link);
	rb_insert_color(&block->rb, &allocator->rb_root);
}

/* add a block to allocator with known location */
static void link_block(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block,
		struct nvhost_alloc_block *prev, struct rb_node **rb_link,
		struct rb_node *rb_parent)
{
	struct nvhost_alloc_block *next;

	link_block_list(allocator, block, prev, rb_parent);
	link_block_rb(allocator, block, rb_link, rb_parent);
	allocator->block_count++;

	next = block->next;
	allocator_dbg(allocator, "link new block %d:%d between block %d:%d "
		"and block %d:%d",
		block->start, block->end,
		prev ? prev->start : -1, prev ? prev->end : -1,
		next ? next->start : -1, next ? next->end : -1);
}

/* add a block to allocator */
static void insert_block(struct nvhost_allocator *allocator,
			struct nvhost_alloc_block *block)
{
	struct nvhost_alloc_block *prev;
	struct rb_node **rb_link, *rb_parent;

	find_block_prepare(allocator, block->start,
			&prev, &rb_link, &rb_parent);
	link_block(allocator, block, prev, rb_link, rb_parent);
}

/* remove a block from allocator */
static void unlink_block(struct nvhost_allocator *allocator,
			struct nvhost_alloc_block *block,
			struct nvhost_alloc_block *prev)
{
	struct nvhost_alloc_block *next = block->next;

	allocator_dbg(allocator, "unlink block %d:%d between block %d:%d "
		"and block %d:%d",
		block->start, block->end,
		prev ? prev->start : -1, prev ? prev->end : -1,
		next ? next->start : -1, next ? next->end : -1);

	BUG_ON(block->start < allocator->base);
	BUG_ON(block->end > allocator->limit);

	if (prev)
		prev->next = next;
	else
		allocator->block_first = next;

	if (next)
		next->prev = prev;
	rb_erase(&block->rb, &allocator->rb_root);
	if (allocator->block_recent == block)
		allocator->block_recent = prev;

	allocator->block_count--;
}

/* remove a list of blocks from allocator. the list can contain both
   regular blocks and non-contiguous blocks. skip all non-contiguous
   blocks, remove regular blocks into a separate list, return list head */
static struct nvhost_alloc_block *
unlink_blocks(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block,
		struct nvhost_alloc_block *prev,
		u32 end)
{
	struct nvhost_alloc_block **insertion_point;
	struct nvhost_alloc_block *last_unfreed_block = prev;
	struct nvhost_alloc_block *last_freed_block = NULL;
	struct nvhost_alloc_block *first_freed_block = NULL;

	insertion_point = (prev ? &prev->next : &allocator->block_first);
	*insertion_point = NULL;

	do {
		if (!block->nc_block) {
			allocator_dbg(allocator, "unlink block %d:%d",
				block->start, block->end);
			if (last_freed_block)
				last_freed_block->next = block;
			block->prev = last_freed_block;
			rb_erase(&block->rb, &allocator->rb_root);
			last_freed_block = block;
			allocator->block_count--;
			if (!first_freed_block)
				first_freed_block = block;
		} else {
			allocator_dbg(allocator, "skip nc block %d:%d",
				block->start, block->end);
			if (!*insertion_point)
				*insertion_point = block;
			if (last_unfreed_block)
				last_unfreed_block->next = block;
			block->prev = last_unfreed_block;
			last_unfreed_block = block;
		}
		block = block->next;
	} while (block && block->start < end);

	if (!*insertion_point)
		*insertion_point = block;

	if (block)
		block->prev = last_unfreed_block;
	if (last_unfreed_block)
		last_unfreed_block->next = block;
	if (last_freed_block)
		last_freed_block->next = NULL;

	allocator->block_recent = NULL;

	return first_freed_block;
}

/* Look up the first block which satisfies addr < block->end,
   NULL if none */
static struct nvhost_alloc_block *
find_block(struct nvhost_allocator *allocator, u32 addr)
{
	struct nvhost_alloc_block *block = allocator->block_recent;

	if (!(block && block->end > addr && block->start <= addr)) {
		struct rb_node *rb_node;

		rb_node = allocator->rb_root.rb_node;
		block = NULL;

		while (rb_node) {
			struct nvhost_alloc_block *block_tmp;

			block_tmp = rb_entry(rb_node,
					struct nvhost_alloc_block, rb);

			if (block_tmp->end > addr) {
				block = block_tmp;
				if (block_tmp->start <= addr)
					break;
				rb_node = rb_node->rb_left;
			} else
				rb_node = rb_node->rb_right;
			if (block)
				allocator->block_recent = block;
		}
	}
	return block;
}

/* Same as find_block, but also return a pointer to the previous block */
static struct nvhost_alloc_block *
find_block_prev(struct nvhost_allocator *allocator, u32 addr,
		struct nvhost_alloc_block **pprev)
{
	struct nvhost_alloc_block *block = NULL, *prev = NULL;
	struct rb_node *rb_node;
	if (!allocator)
		goto out;

	block = allocator->block_first;

	rb_node = allocator->rb_root.rb_node;

	while (rb_node) {
		struct nvhost_alloc_block *block_tmp;
		block_tmp = rb_entry(rb_node, struct nvhost_alloc_block, rb);

		if (addr < block_tmp->end)
			rb_node = rb_node->rb_left;
		else {
			prev = block_tmp;
			if (!prev->next || addr < prev->next->end)
				break;
			rb_node = rb_node->rb_right;
		}
	}

out:
	*pprev = prev;
	return prev ? prev->next : block;
}

/* Same as find_block, but also return a pointer to the previous block
   and return rb_node to prepare for rbtree insertion */
static struct nvhost_alloc_block *
find_block_prepare(struct nvhost_allocator *allocator, u32 addr,
		struct nvhost_alloc_block **pprev, struct rb_node ***rb_link,
		struct rb_node **rb_parent)
{
	struct nvhost_alloc_block *block;
	struct rb_node **__rb_link, *__rb_parent, *rb_prev;

	__rb_link = &allocator->rb_root.rb_node;
	rb_prev = __rb_parent = NULL;
	block = NULL;

	while (*__rb_link) {
		struct nvhost_alloc_block *block_tmp;

		__rb_parent = *__rb_link;
		block_tmp = rb_entry(__rb_parent,
				struct nvhost_alloc_block, rb);

		if (block_tmp->end > addr) {
			block = block_tmp;
			if (block_tmp->start <= addr)
				break;
			__rb_link = &__rb_parent->rb_left;
		} else {
			rb_prev = __rb_parent;
			__rb_link = &__rb_parent->rb_right;
		}
	}

	*pprev = NULL;
	if (rb_prev)
		*pprev = rb_entry(rb_prev, struct nvhost_alloc_block, rb);
	*rb_link = __rb_link;
	*rb_parent = __rb_parent;
	return block;
}

/* return available space */
static u32 check_free_space(u32 addr, u32 limit, u32 len, u32 align)
{
	if (addr >= limit)
		return 0;
	if (addr + len <= limit)
		return len;
	return (limit - addr) & ~(align - 1);
}

/* update first_free_addr/last_free_addr based on new free addr
   called when free block(s) and allocate block(s) */
static void update_free_addr_cache(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *next,
		u32 addr, u32 len, bool free)
{
	/* update from block free */
	if (free) {
		if (allocator->first_free_addr > addr)
			allocator->first_free_addr = addr;
	} else { /* update from block alloc */
		if (allocator->last_free_addr < addr + len)
			allocator->last_free_addr = addr + len;
		if (allocator->first_free_addr == addr) {
			if (!next || next->start > addr + len)
				allocator->first_free_addr = addr + len;
			else
				allocator->first_free_addr = next->end;
		}
	}

	if (allocator->first_free_addr > allocator->last_free_addr)
		allocator->first_free_addr = allocator->last_free_addr;
}

/* find a free address range for a fixed len */
static int find_free_area(struct nvhost_allocator *allocator,
			u32 *addr, u32 len)
{
	struct nvhost_alloc_block *block;
	u32 start_addr, search_base, search_limit;

	/* fixed addr allocation */
	/* note: constraints for fixed are handled by caller */
	if (*addr) {
		block = find_block(allocator, *addr);
		if (allocator->limit - len >= *addr &&
		    (!block || *addr + len <= block->start)) {
			update_free_addr_cache(allocator, block,
					*addr, len, false);
			return 0;
		} else
			return -ENOMEM;
	}

	if (!allocator->constraint.enable) {
		search_base  = allocator->base;
		search_limit = allocator->limit;
	} else {
		start_addr = *addr = allocator->constraint.base;
		search_base = allocator->constraint.base;
		search_limit = allocator->constraint.limit;
	}

	/* cached_hole_size has max free space up to last_free_addr */
	if (len > allocator->cached_hole_size)
		start_addr = *addr = allocator->last_free_addr;
	else {
		start_addr = *addr = allocator->base;
		allocator->cached_hole_size = 0;
	}

	allocator_dbg(allocator, "start search addr : %d", start_addr);

full_search:
	for (block = find_block(allocator, *addr); ; block = block->next) {
		if (search_limit - len < *addr) {
			/* start a new search in case we missed any hole */
			if (start_addr != search_base) {
				start_addr = *addr = search_base;
				allocator->cached_hole_size = 0;
				allocator_dbg(allocator, "start a new search from base");
				goto full_search;
			}
			return -ENOMEM;
		}
		if (!block || *addr + len <= block->start) {
			update_free_addr_cache(allocator, block,
					*addr, len, false);
			allocator_dbg(allocator, "free space from %d, len %d",
				*addr, len);
			allocator_dbg(allocator, "next free addr: %d",
				allocator->last_free_addr);
			return 0;
		}
		if (*addr + allocator->cached_hole_size < block->start)
			allocator->cached_hole_size = block->start - *addr;
		*addr = block->end;
	}
}

/* find a free address range for as long as it meets alignment or meet len */
static int find_free_area_nc(struct nvhost_allocator *allocator,
			u32 *addr, u32 *len)
{
	struct nvhost_alloc_block *block;
	u32 start_addr;
	u32 avail_len;

	/* fixed addr allocation */
	if (*addr) {
		block = find_block(allocator, *addr);
		if (allocator->limit - *len >= *addr) {
			if (!block)
				return 0;

			avail_len = check_free_space(*addr, block->start,
						*len, allocator->align);
			if (avail_len != 0) {
				update_free_addr_cache(allocator, block,
					*addr, avail_len, false);
				allocator_dbg(allocator,
					"free space between %d, %d, len %d",
					*addr, block->start, avail_len);
				allocator_dbg(allocator, "next free addr: %d",
					allocator->last_free_addr);
				*len = avail_len;
				return 0;
			} else
				return -ENOMEM;
		} else
			return -ENOMEM;
	}

	start_addr = *addr = allocator->first_free_addr;

	allocator_dbg(allocator, "start search addr : %d", start_addr);

	for (block = find_block(allocator, *addr); ; block = block->next) {
		if (allocator->limit - *len < *addr)
			return -ENOMEM;
		if (!block) {
			update_free_addr_cache(allocator, block,
					*addr, *len, false);
			allocator_dbg(allocator, "free space from %d, len %d",
				*addr, *len);
			allocator_dbg(allocator, "next free addr: %d",
				allocator->first_free_addr);
			return 0;
		}

		avail_len = check_free_space(*addr, block->start,
					*len, allocator->align);
		if (avail_len != 0) {
			update_free_addr_cache(allocator, block,
					*addr, avail_len, false);
			allocator_dbg(allocator, "free space between %d, %d, len %d",
				*addr, block->start, avail_len);
			allocator_dbg(allocator, "next free addr: %d",
				allocator->first_free_addr);
			*len = avail_len;
			return 0;
		}
		if (*addr + allocator->cached_hole_size < block->start)
			allocator->cached_hole_size = block->start - *addr;
		*addr = block->end;
	}
}

/* expand/shrink a block with new start and new end
   split_block function provides insert block for shrink */
static void adjust_block(struct nvhost_alloc_block *block,
		u32 start, u32 end, struct nvhost_alloc_block *insert)
{
	struct nvhost_allocator *allocator = block->allocator;

	allocator_dbg(allocator, "curr block %d:%d, new start %d, new end %d",
		block->start, block->end, start, end);

	/* expand */
	if (!insert) {
		if (start == block->end) {
			struct nvhost_alloc_block *next = block->next;

			if (next && end == next->start) {
				/* ....AAAA.... */
				/* PPPP....NNNN */
				/* PPPPPPPPPPPP */
				unlink_block(allocator, next, block);
				block->end = next->end;
				kmem_cache_free(allocator->block_cache, next);
			} else {
				/* ....AAAA.... */
				/* PPPP........ */
				/* PPPPPPPP.... */
				block->end = end;
			}
		}

		if (end == block->start) {
			/* ....AAAA.... */
			/* ........NNNN */
			/* PP..NNNNNNNN        ....NNNNNNNN */
			block->start = start;
		}
	} else { /* shrink */
		/* BBBBBBBB -> BBBBIIII  OR  BBBBBBBB -> IIIIBBBB */
		block->start = start;
		block->end = end;
		insert_block(allocator, insert);
	}
}

/* given a range [addr, end], merge it with blocks before or after or both
   if they can be combined into a contiguous block */
static struct nvhost_alloc_block *
merge_block(struct nvhost_allocator *allocator,
	struct nvhost_alloc_block *prev, u32 addr, u32 end)
{
	struct nvhost_alloc_block *next;

	if (prev)
		next = prev->next;
	else
		next = allocator->block_first;

	allocator_dbg(allocator, "curr block %d:%d", addr, end);
	if (prev)
		allocator_dbg(allocator, "prev block %d:%d", prev->start, prev->end);
	if (next)
		allocator_dbg(allocator, "next block %d:%d", next->start, next->end);

	/* don't merge with non-contiguous allocation block */
	if (prev && prev->end == addr && !prev->nc_block) {
		adjust_block(prev, addr, end, NULL);
		return prev;
	}

	/* don't merge with non-contiguous allocation block */
	if (next && end == next->start && !next->nc_block) {
		adjust_block(next, addr, end, NULL);
		return next;
	}

	return NULL;
}

/* split a block based on addr. addr must be within (start, end).
   if new_below == 1, link new block before adjusted current block */
static int split_block(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block, u32 addr, int new_below)
{
	struct nvhost_alloc_block *new_block;

	allocator_dbg(allocator, "start %d, split %d, end %d, new_below %d",
		block->start, addr, block->end, new_below);

	BUG_ON(!(addr > block->start && addr < block->end));

	new_block = kmem_cache_alloc(allocator->block_cache, GFP_KERNEL);
	if (!new_block)
		return -ENOMEM;

	*new_block = *block;

	if (new_below)
		new_block->end = addr;
	else
		new_block->start = addr;

	if (new_below)
		adjust_block(block, addr, block->end, new_block);
	else
		adjust_block(block, block->start, addr, new_block);

	return 0;
}

/* free a list of blocks */
static void free_blocks(struct nvhost_allocator *allocator,
			struct nvhost_alloc_block *block)
{
	struct nvhost_alloc_block *curr_block;
	while (block) {
		curr_block = block;
		block = block->next;
		kmem_cache_free(allocator->block_cache, curr_block);
	}
}

/* called with rw_sema acquired */
static int block_alloc_single_locked(struct nvhost_allocator *allocator,
				u32 *addr_req, u32 len)
{
	struct nvhost_alloc_block *block, *prev;
	struct rb_node **rb_link, *rb_parent;
	u32 addr = *addr_req;
	int err;

	*addr_req = ~0;

	err = find_free_area(allocator, &addr, len);
	if (err)
		return err;

	find_block_prepare(allocator, addr, &prev, &rb_link, &rb_parent);

	/* merge requested free space with existing block(s)
	   if they can be combined into one contiguous block */
	block = merge_block(allocator, prev, addr, addr + len);
	if (block) {
		*addr_req = addr;
		return 0;
	}

	/* create a new block if cannot merge */
	block = kmem_cache_zalloc(allocator->block_cache, GFP_KERNEL);
	if (!block)
		return -ENOMEM;

	block->allocator = allocator;
	block->start = addr;
	block->end = addr + len;

	link_block(allocator, block, prev, rb_link, rb_parent);

	*addr_req = addr;

	return 0;
}

static int block_alloc_list_locked(struct nvhost_allocator *allocator,
	u32 *addr_req, u32 nc_len, struct nvhost_alloc_block **pblock)
{
	struct nvhost_alloc_block *block;
	struct nvhost_alloc_block *nc_head = NULL, *nc_prev = NULL;
	u32 addr = *addr_req, len = nc_len;
	int err = 0;

	*addr_req = ~0;

	while (nc_len > 0) {
		err = find_free_area_nc(allocator, &addr, &len);
		if (err) {
			allocator_dbg(allocator, "not enough free space");
			goto clean_up;
		}

		/* never merge non-contiguous allocation block,
		   just create a new block */
		block = kmem_cache_zalloc(allocator->block_cache,
					GFP_KERNEL);
		if (!block) {
			err = -ENOMEM;
			goto clean_up;
		}

		block->allocator = allocator;
		block->start = addr;
		block->end = addr + len;

		insert_block(allocator, block);

		block->nc_prev = nc_prev;
		if (nc_prev)
			nc_prev->nc_next = block;
		nc_prev = block;
		block->nc_block = true;

		if (!nc_head)
			nc_head = block;

		if (*addr_req == ~0)
			*addr_req = addr;

		addr = 0;
		nc_len -= len;
		len = nc_len;
		allocator_dbg(allocator, "remaining length %d", nc_len);
	}

clean_up:
	if (err) {
		while (nc_head) {
			unlink_block(allocator, nc_head, nc_head->prev);
			nc_prev = nc_head;
			nc_head = nc_head->nc_next;
			kmem_cache_free(allocator->block_cache, nc_prev);
		}
		*pblock = NULL;
		*addr_req = ~0;
	} else {
		*pblock = nc_head;
	}

	return err;
}

/* called with rw_sema acquired */
static int block_free_locked(struct nvhost_allocator *allocator,
			u32 addr, u32 len)
{
	struct nvhost_alloc_block *block, *prev, *last;
	u32 end;
	int err;

	/* no block has block->end > addr, already free */
	block = find_block_prev(allocator, addr, &prev);
	if (!block)
		return 0;

	allocator_dbg(allocator, "first block in free range %d:%d",
		block->start, block->end);

	end = addr + len;
	/* not in any block, already free */
	if (block->start >= end)
		return 0;

	/* don't touch nc_block in range free */
	if (addr > block->start && !block->nc_block) {
		int err = split_block(allocator, block, addr, 0);
		if (err)
			return err;
		prev = block;
	}

	last = find_block(allocator, end);
	if (last && end > last->start && !last->nc_block) {

		allocator_dbg(allocator, "last block in free range %d:%d",
			last->start, last->end);

		err = split_block(allocator, last, end, 1);
		if (err)
			return err;
	}

	block = prev ? prev->next : allocator->block_first;

	allocator_dbg(allocator, "first block for free %d:%d",
		block->start, block->end);

	/* remove blocks between [addr, addr + len) from rb tree
	   and put them in a list */
	block = unlink_blocks(allocator, block, prev, end);
	free_blocks(allocator, block);

	update_free_addr_cache(allocator, NULL, addr, len, true);

	return 0;
}

/* called with rw_sema acquired */
static void block_free_list_locked(struct nvhost_allocator *allocator,
			struct nvhost_alloc_block *list)
{
	struct nvhost_alloc_block *block;
	u32 len;

	update_free_addr_cache(allocator, NULL,
			list->start, list->end - list->start, true);

	while (list) {
		block = list;
		unlink_block(allocator, block, block->prev);

		len = block->end - block->start;
		if (allocator->cached_hole_size < len)
			allocator->cached_hole_size = len;

		list = block->nc_next;
		kmem_cache_free(allocator->block_cache, block);
	}
}

static int
nvhost_allocator_constrain(struct nvhost_allocator *a,
			   bool enable, u32 base, u32 limit)
{
	if (enable) {
		a->constraint.enable = (base >= a->base &&
					limit <= a->limit);
		if (!a->constraint.enable)
			return -EINVAL;
		a->constraint.base  = base;
		a->constraint.limit = limit;
		a->first_free_addr = a->last_free_addr = base;

	} else {
		a->constraint.enable = false;
		a->first_free_addr = a->last_free_addr = a->base;
	}

	a->cached_hole_size = 0;

	return 0;
}

/* init allocator struct */
int nvhost_allocator_init(struct nvhost_allocator *allocator,
		const char *name, u32 start, u32 len, u32 align)
{
	memset(allocator, 0, sizeof(struct nvhost_allocator));

	strncpy(allocator->name, name, 32);

	allocator->block_cache =
		kmem_cache_create(allocator->name,
			sizeof(struct nvhost_alloc_block), 0,
			SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD, NULL);
	if (!allocator->block_cache)
		return -ENOMEM;

	allocator->rb_root = RB_ROOT;

	allocator->base = start;
	allocator->limit = start + len - 1;
	allocator->align = align;

	allocator_dbg(allocator, "%s : base %d, limit %d, align %d",
		allocator->name, allocator->base,
		allocator->limit, allocator->align);

	allocator->first_free_addr = allocator->last_free_addr = start;
	allocator->cached_hole_size = len;

	init_rwsem(&allocator->rw_sema);

	allocator->alloc = nvhost_block_alloc;
	allocator->alloc_nc = nvhost_block_alloc_nc;
	allocator->free = nvhost_block_free;
	allocator->free_nc = nvhost_block_free_nc;
	allocator->constrain = nvhost_allocator_constrain;

	return 0;
}

/* destroy allocator, free all remaining blocks if any */
void nvhost_allocator_destroy(struct nvhost_allocator *allocator)
{
	struct nvhost_alloc_block *block, *next;
	u32 free_count = 0;

	down_write(&allocator->rw_sema);

	for (block = allocator->block_first; block; ) {
		allocator_dbg(allocator, "free remaining block %d:%d",
			block->start, block->end);
		next = block->next;
		kmem_cache_free(allocator->block_cache, block);
		free_count++;
		block = next;
	}

	up_write(&allocator->rw_sema);

	/* block_count doesn't match real number of blocks */
	BUG_ON(free_count != allocator->block_count);

	kmem_cache_destroy(allocator->block_cache);

	memset(allocator, 0, sizeof(struct nvhost_allocator));
}

/*
 * *addr != ~0 for fixed address allocation. if *addr == 0, base addr is
 * returned to caller in *addr.
 *
 * contiguous allocation, which allocates one block of
 * contiguous address.
*/
int nvhost_block_alloc(struct nvhost_allocator *allocator, u32 *addr, u32 len)
{
	int ret;
#if defined(ALLOCATOR_DEBUG)
	struct nvhost_alloc_block *block;
	bool should_fail = false;
#endif

	allocator_dbg(allocator, "[in] addr %d, len %d", *addr, len);

	if (*addr + len > allocator->limit || /* check addr range */
	    *addr & (allocator->align - 1) || /* check addr alignment */
	     len == 0)			      /* check len */
		return -EINVAL;

	if (allocator->constraint.enable &&
	    (*addr + len > allocator->constraint.limit ||
	     *addr > allocator->constraint.base))
		return -EINVAL;

	len = ALIGN(len, allocator->align);
	if (!len)
		return -ENOMEM;

	down_write(&allocator->rw_sema);

#if defined(ALLOCATOR_DEBUG)
	if (*addr) {
		for (block = allocator->block_first;
		     block; block = block->next) {
			if (block->end > *addr && block->start < *addr + len) {
				should_fail = true;
				break;
			}
		}
	}
#endif

	ret = block_alloc_single_locked(allocator, addr, len);

#if defined(ALLOCATOR_DEBUG)
	if (!ret) {
		bool allocated = false;
		BUG_ON(should_fail);
		BUG_ON(*addr < allocator->base);
		BUG_ON(*addr + len > allocator->limit);
		for (block = allocator->block_first;
		     block; block = block->next) {
			if (!block->nc_block &&
			    block->start <= *addr &&
			    block->end >= *addr + len) {
				allocated = true;
				break;
			}
		}
		BUG_ON(!allocated);
	}
#endif

	up_write(&allocator->rw_sema);

	allocator_dbg(allocator, "[out] addr %d, len %d", *addr, len);

	return ret;
}

/*
 * *addr != ~0 for fixed address allocation. if *addr == 0, base addr is
 * returned to caller in *addr.
 *
 * non-contiguous allocation, which returns a list of blocks with aggregated
 * size == len. Individual block size must meet alignment requirement.
 */
int nvhost_block_alloc_nc(struct nvhost_allocator *allocator, u32 *addr,
			u32 len, struct nvhost_alloc_block **pblock)
{
	int ret;

	allocator_dbg(allocator, "[in] addr %d, len %d", *addr, len);

	BUG_ON(pblock == NULL);
	*pblock = NULL;

	if (*addr + len > allocator->limit || /* check addr range */
	    *addr & (allocator->align - 1) || /* check addr alignment */
	     len == 0)			      /* check len */
		return -EINVAL;

	len = ALIGN(len, allocator->align);
	if (!len)
		return -ENOMEM;

	down_write(&allocator->rw_sema);

	ret = block_alloc_list_locked(allocator, addr, len, pblock);

#if defined(ALLOCATOR_DEBUG)
	if (!ret) {
		struct nvhost_alloc_block *block = *pblock;
		BUG_ON(!block);
		BUG_ON(block->start < allocator->base);
		while (block->nc_next) {
			BUG_ON(block->end > block->nc_next->start);
			block = block->nc_next;
		}
		BUG_ON(block->end > allocator->limit);
	}
#endif

	up_write(&allocator->rw_sema);

	allocator_dbg(allocator, "[out] addr %d, len %d", *addr, len);

	return ret;
}

/* free all blocks between start and end */
int nvhost_block_free(struct nvhost_allocator *allocator, u32 addr, u32 len)
{
	int ret;

	allocator_dbg(allocator, "[in] addr %d, len %d", addr, len);

	if (addr + len > allocator->limit || /* check addr range */
	    addr < allocator->base ||
	    addr & (allocator->align - 1))   /* check addr alignment */
		return -EINVAL;

	len = ALIGN(len, allocator->align);
	if (!len)
		return -EINVAL;

	down_write(&allocator->rw_sema);

	ret = block_free_locked(allocator, addr, len);

#if defined(ALLOCATOR_DEBUG)
	if (!ret) {
		struct nvhost_alloc_block *block;
		for (block = allocator->block_first;
		     block; block = block->next) {
			if (!block->nc_block)
				BUG_ON(block->start >= addr &&
					block->end <= addr + len);
		}
	}
#endif
	up_write(&allocator->rw_sema);

	allocator_dbg(allocator, "[out] addr %d, len %d", addr, len);

	return ret;
}

/* free non-contiguous allocation block list */
void nvhost_block_free_nc(struct nvhost_allocator *allocator,
		struct nvhost_alloc_block *block)
{
	/* nothing to free */
	if (!block)
		return;

	down_write(&allocator->rw_sema);
	block_free_list_locked(allocator, block);
	up_write(&allocator->rw_sema);
}

#if defined(ALLOCATOR_DEBUG)

#include <linux/random.h>

/* test suite */
void nvhost_allocator_test(void)
{
	struct nvhost_allocator allocator;
	struct nvhost_alloc_block *list[5];
	u32 addr, len;
	u32 count;
	int n;

	nvhost_allocator_init(&allocator, "test", 0, 10, 1);

	/* alloc/free a single block in the beginning */
	addr = 0;
	nvhost_block_alloc(&allocator, &addr, 2);
	nvhost_allocator_dump(&allocator);
	nvhost_block_free(&allocator, addr, 2);
	nvhost_allocator_dump(&allocator);
	/* alloc/free a single block in the middle */
	addr = 4;
	nvhost_block_alloc(&allocator, &addr, 2);
	nvhost_allocator_dump(&allocator);
	nvhost_block_free(&allocator, addr, 2);
	nvhost_allocator_dump(&allocator);
	/* alloc/free a single block in the end */
	addr = 8;
	nvhost_block_alloc(&allocator, &addr, 2);
	nvhost_allocator_dump(&allocator);
	nvhost_block_free(&allocator, addr, 2);
	nvhost_allocator_dump(&allocator);

	/* allocate contiguous blocks */
	addr = 0;
	nvhost_block_alloc(&allocator, &addr, 2);
	nvhost_allocator_dump(&allocator);
	addr = 0;
	nvhost_block_alloc(&allocator, &addr, 4);
	nvhost_allocator_dump(&allocator);
	addr = 0;
	nvhost_block_alloc(&allocator, &addr, 4);
	nvhost_allocator_dump(&allocator);

	/* no free space */
	addr = 0;
	nvhost_block_alloc(&allocator, &addr, 2);
	nvhost_allocator_dump(&allocator);

	/* free in the end */
	nvhost_block_free(&allocator, 8, 2);
	nvhost_allocator_dump(&allocator);
	/* free in the beginning */
	nvhost_block_free(&allocator, 0, 2);
	nvhost_allocator_dump(&allocator);
	/* free in the middle */
	nvhost_block_free(&allocator, 4, 2);
	nvhost_allocator_dump(&allocator);

	/* merge case PPPPAAAANNNN */
	addr = 4;
	nvhost_block_alloc(&allocator, &addr, 2);
	nvhost_allocator_dump(&allocator);
	/* merge case ....AAAANNNN */
	addr = 0;
	nvhost_block_alloc(&allocator, &addr, 2);
	nvhost_allocator_dump(&allocator);
	/* merge case PPPPAAAA.... */
	addr = 8;
	nvhost_block_alloc(&allocator, &addr, 2);
	nvhost_allocator_dump(&allocator);

	/* test free across multiple blocks and split */
	nvhost_block_free(&allocator, 2, 2);
	nvhost_allocator_dump(&allocator);
	nvhost_block_free(&allocator, 6, 2);
	nvhost_allocator_dump(&allocator);
	nvhost_block_free(&allocator, 1, 8);
	nvhost_allocator_dump(&allocator);

	/* test non-contiguous allocation */
	addr = 4;
	nvhost_block_alloc(&allocator, &addr, 2);
	nvhost_allocator_dump(&allocator);
	addr = 0;
	nvhost_block_alloc_nc(&allocator, &addr, 5, &list[0]);
	nvhost_allocator_dump(&allocator);
	nvhost_allocator_dump_nc_list(&allocator, list[0]);

	/* test free a range overlaping non-contiguous blocks */
	nvhost_block_free(&allocator, 2, 6);
	nvhost_allocator_dump(&allocator);

	/* test non-contiguous free */
	nvhost_block_free_nc(&allocator, list[0]);
	nvhost_allocator_dump(&allocator);

	nvhost_allocator_destroy(&allocator);

	/* random stress test */
	nvhost_allocator_init(&allocator, "test", 4096, 4096 * 1024, 4096);
	for (;;) {
		printk(KERN_DEBUG "alloc tests...\n");
		for (count = 0; count < 50; count++) {
			addr = 0;
			len = random32() % (4096 * 1024 / 16);
			nvhost_block_alloc(&allocator, &addr, len);
			nvhost_allocator_dump(&allocator);
		}

		printk(KERN_DEBUG "free tests...\n");
		for (count = 0; count < 30; count++) {
			addr = (random32() % (4096 * 1024)) & ~(4096 - 1);
			len = random32() % (4096 * 1024 / 16);
			nvhost_block_free(&allocator, addr, len);
			nvhost_allocator_dump(&allocator);
		}

		printk(KERN_DEBUG "non-contiguous alloc tests...\n");
		for (n = 0; n < 5; n++) {
			addr = 0;
			len = random32() % (4096 * 1024 / 8);
			nvhost_block_alloc_nc(&allocator, &addr, len, &list[n]);
			nvhost_allocator_dump(&allocator);
			nvhost_allocator_dump_nc_list(&allocator, list[n]);
		}

		printk(KERN_DEBUG "free tests...\n");
		for (count = 0; count < 10; count++) {
			addr = (random32() % (4096 * 1024)) & ~(4096 - 1);
			len = random32() % (4096 * 1024 / 16);
			nvhost_block_free(&allocator, addr, len);
			nvhost_allocator_dump(&allocator);
		}

		printk(KERN_DEBUG "non-contiguous free tests...\n");
		for (n = 4; n >= 0; n--) {
			nvhost_allocator_dump_nc_list(&allocator, list[n]);
			nvhost_block_free_nc(&allocator, list[n]);
			nvhost_allocator_dump(&allocator);
		}

		printk(KERN_DEBUG "fixed addr alloc tests...\n");
		for (count = 0; count < 10; count++) {
			addr = (random32() % (4096 * 1024)) & ~(4096 - 1);
			len = random32() % (4096 * 1024 / 32);
			nvhost_block_alloc(&allocator, &addr, len);
			nvhost_allocator_dump(&allocator);
		}

		printk(KERN_DEBUG "free tests...\n");
		for (count = 0; count < 10; count++) {
			addr = (random32() % (4096 * 1024)) & ~(4096 - 1);
			len = random32() % (4096 * 1024 / 16);
			nvhost_block_free(&allocator, addr, len);
			nvhost_allocator_dump(&allocator);
		}
	}
	nvhost_allocator_destroy(&allocator);
}

#endif /* ALLOCATOR_DEBUG */

