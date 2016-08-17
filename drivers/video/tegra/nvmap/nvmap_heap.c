/*
 * drivers/video/tegra/nvmap/nvmap_heap.c
 *
 * GPU heap allocator.
 *
 * Copyright (c) 2012, NVIDIA Corporation.
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

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/err.h>

#include <linux/nvmap.h>
#include "nvmap.h"
#include "nvmap_heap.h"
#include "nvmap_common.h"

#include <asm/tlbflush.h>
#include <asm/cacheflush.h>

/*
 * "carveouts" are platform-defined regions of physically contiguous memory
 * which are not managed by the OS. a platform may specify multiple carveouts,
 * for either small special-purpose memory regions (like IRAM on Tegra SoCs)
 * or reserved regions of main system memory.
 *
 * the carveout allocator returns allocations which are physically contiguous.
 * to reduce external fragmentation, the allocation algorithm implemented in
 * this file employs 3 strategies for keeping allocations of similar size
 * grouped together inside the larger heap: the "small", "normal" and "huge"
 * strategies. the size thresholds (in bytes) for determining which strategy
 * to employ should be provided by the platform for each heap. it is possible
 * for a platform to define a heap where only the "normal" strategy is used.
 *
 * o "normal" allocations use an address-order first-fit allocator (called
 *   BOTTOM_UP in the code below). each allocation is rounded up to be
 *   an integer multiple of the "small" allocation size.
 *
 * o "huge" allocations use an address-order last-fit allocator (called
 *   TOP_DOWN in the code below). like "normal" allocations, each allocation
 *   is rounded up to be an integer multiple of the "small" allocation size.
 *
 * o "small" allocations are treated differently: the heap manager maintains
 *   a pool of "small"-sized blocks internally from which allocations less
 *   than 1/2 of the "small" size are buddy-allocated. if a "small" allocation
 *   is requested and none of the buddy sub-heaps is able to service it,
 *   the heap manager will try to allocate a new buddy-heap.
 *
 * this allocator is intended to keep "splinters" colocated in the carveout,
 * and to ensure that the minimum free block size in the carveout (i.e., the
 * "small" threshold) is still a meaningful size.
 *
 */

#define MAX_BUDDY_NR	128	/* maximum buddies in a buddy allocator */

enum direction {
	TOP_DOWN,
	BOTTOM_UP
};

enum block_type {
	BLOCK_FIRST_FIT,	/* block was allocated directly from the heap */
	BLOCK_BUDDY,		/* block was allocated from a buddy sub-heap */
	BLOCK_EMPTY,
};

struct heap_stat {
	size_t free;		/* total free size */
	size_t free_largest;	/* largest free block */
	size_t free_count;	/* number of free blocks */
	size_t total;		/* total size */
	size_t largest;		/* largest unique block */
	size_t count;		/* total number of blocks */
	/* fast compaction attempt counter */
	unsigned int compaction_count_fast;
	/* full compaction attempt counter */
	unsigned int compaction_count_full;
};

struct buddy_heap;

struct buddy_block {
	struct nvmap_heap_block block;
	struct buddy_heap *heap;
};

struct list_block {
	struct nvmap_heap_block block;
	struct list_head all_list;
	unsigned int mem_prot;
	phys_addr_t orig_addr;
	size_t size;
	size_t align;
	struct nvmap_heap *heap;
	struct list_head free_list;
};

struct combo_block {
	union {
		struct list_block lb;
		struct buddy_block bb;
	};
};

struct buddy_bits {
	unsigned int alloc:1;
	unsigned int order:7;	/* log2(MAX_BUDDY_NR); */
};

struct buddy_heap {
	struct list_block *heap_base;
	unsigned int nr_buddies;
	struct list_head buddy_list;
	struct buddy_bits bitmap[MAX_BUDDY_NR];
};

struct nvmap_heap {
	struct list_head all_list;
	struct list_head free_list;
	struct mutex lock;
	struct list_head buddy_list;
	unsigned int min_buddy_shift;
	unsigned int buddy_heap_size;
	unsigned int small_alloc;
	const char *name;
	void *arg;
	struct device dev;
};

static struct kmem_cache *buddy_heap_cache;
static struct kmem_cache *block_cache;

static inline struct nvmap_heap *parent_of(struct buddy_heap *heap)
{
	return heap->heap_base->heap;
}

static inline unsigned int order_of(size_t len, size_t min_shift)
{
	len = 2 * DIV_ROUND_UP(len, (1 << min_shift)) - 1;
	return fls(len)-1;
}

/* returns the free size in bytes of the buddy heap; must be called while
 * holding the parent heap's lock. */
static void buddy_stat(struct buddy_heap *heap, struct heap_stat *stat)
{
	unsigned int index;
	unsigned int shift = parent_of(heap)->min_buddy_shift;

	for (index = 0; index < heap->nr_buddies;
	     index += (1 << heap->bitmap[index].order)) {
		size_t curr = 1 << (heap->bitmap[index].order + shift);

		stat->largest = max(stat->largest, curr);
		stat->total += curr;
		stat->count++;

		if (!heap->bitmap[index].alloc) {
			stat->free += curr;
			stat->free_largest = max(stat->free_largest, curr);
			stat->free_count++;
		}
	}
}

/* returns the free size of the heap (including any free blocks in any
 * buddy-heap suballocators; must be called while holding the parent
 * heap's lock. */
static phys_addr_t heap_stat(struct nvmap_heap *heap, struct heap_stat *stat)
{
	struct buddy_heap *bh;
	struct list_block *l = NULL;
	phys_addr_t base = -1ul;

	memset(stat, 0, sizeof(*stat));
	mutex_lock(&heap->lock);
	list_for_each_entry(l, &heap->all_list, all_list) {
		stat->total += l->size;
		stat->largest = max(l->size, stat->largest);
		stat->count++;
		base = min(base, l->orig_addr);
	}

	list_for_each_entry(bh, &heap->buddy_list, buddy_list) {
		buddy_stat(bh, stat);
		/* the total counts are double-counted for buddy heaps
		 * since the blocks allocated for buddy heaps exist in the
		 * all_list; subtract out the doubly-added stats */
		stat->total -= bh->heap_base->size;
		stat->count--;
	}

	list_for_each_entry(l, &heap->free_list, free_list) {
		stat->free += l->size;
		stat->free_count++;
		stat->free_largest = max(l->size, stat->free_largest);
	}
	mutex_unlock(&heap->lock);

	return base;
}

static ssize_t heap_name_show(struct device *dev,
			      struct device_attribute *attr, char *buf);

static ssize_t heap_stat_show(struct device *dev,
			      struct device_attribute *attr, char *buf);

static struct device_attribute heap_stat_total_max =
	__ATTR(total_max, S_IRUGO, heap_stat_show, NULL);

static struct device_attribute heap_stat_total_count =
	__ATTR(total_count, S_IRUGO, heap_stat_show, NULL);

static struct device_attribute heap_stat_total_size =
	__ATTR(total_size, S_IRUGO, heap_stat_show, NULL);

static struct device_attribute heap_stat_free_max =
	__ATTR(free_max, S_IRUGO, heap_stat_show, NULL);

static struct device_attribute heap_stat_free_count =
	__ATTR(free_count, S_IRUGO, heap_stat_show, NULL);

static struct device_attribute heap_stat_free_size =
	__ATTR(free_size, S_IRUGO, heap_stat_show, NULL);

static struct device_attribute heap_stat_base =
	__ATTR(base, S_IRUGO, heap_stat_show, NULL);

static struct device_attribute heap_attr_name =
	__ATTR(name, S_IRUGO, heap_name_show, NULL);

static struct attribute *heap_stat_attrs[] = {
	&heap_stat_total_max.attr,
	&heap_stat_total_count.attr,
	&heap_stat_total_size.attr,
	&heap_stat_free_max.attr,
	&heap_stat_free_count.attr,
	&heap_stat_free_size.attr,
	&heap_stat_base.attr,
	&heap_attr_name.attr,
	NULL,
};

static struct attribute_group heap_stat_attr_group = {
	.attrs	= heap_stat_attrs,
};

static ssize_t heap_name_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{

	struct nvmap_heap *heap = container_of(dev, struct nvmap_heap, dev);
	return sprintf(buf, "%s\n", heap->name);
}

static ssize_t heap_stat_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct nvmap_heap *heap = container_of(dev, struct nvmap_heap, dev);
	struct heap_stat stat;
	phys_addr_t base;

	base = heap_stat(heap, &stat);

	if (attr == &heap_stat_total_max)
		return sprintf(buf, "%u\n", stat.largest);
	else if (attr == &heap_stat_total_count)
		return sprintf(buf, "%u\n", stat.count);
	else if (attr == &heap_stat_total_size)
		return sprintf(buf, "%u\n", stat.total);
	else if (attr == &heap_stat_free_max)
		return sprintf(buf, "%u\n", stat.free_largest);
	else if (attr == &heap_stat_free_count)
		return sprintf(buf, "%u\n", stat.free_count);
	else if (attr == &heap_stat_free_size)
		return sprintf(buf, "%u\n", stat.free);
	else if (attr == &heap_stat_base)
		return sprintf(buf, "%08llx\n", (unsigned long long)base);
	else
		return -EINVAL;
}
#ifndef CONFIG_NVMAP_CARVEOUT_COMPACTOR
static struct nvmap_heap_block *buddy_alloc(struct buddy_heap *heap,
					    size_t size, size_t align,
					    unsigned int mem_prot)
{
	unsigned int index = 0;
	unsigned int min_shift = parent_of(heap)->min_buddy_shift;
	unsigned int order = order_of(size, min_shift);
	unsigned int align_mask;
	unsigned int best = heap->nr_buddies;
	struct buddy_block *b;

	if (heap->heap_base->mem_prot != mem_prot)
		return NULL;

	align = max(align, (size_t)(1 << min_shift));
	align_mask = (align >> min_shift) - 1;

	for (index = 0; index < heap->nr_buddies;
	     index += (1 << heap->bitmap[index].order)) {

		if (heap->bitmap[index].alloc || (index & align_mask) ||
		    (heap->bitmap[index].order < order))
			continue;

		if (best == heap->nr_buddies ||
		    heap->bitmap[index].order < heap->bitmap[best].order)
			best = index;

		if (heap->bitmap[best].order == order)
			break;
	}

	if (best == heap->nr_buddies)
		return NULL;

	b = kmem_cache_zalloc(block_cache, GFP_KERNEL);
	if (!b)
		return NULL;

	while (heap->bitmap[best].order != order) {
		unsigned int buddy;
		heap->bitmap[best].order--;
		buddy = best ^ (1 << heap->bitmap[best].order);
		heap->bitmap[buddy].order = heap->bitmap[best].order;
		heap->bitmap[buddy].alloc = 0;
	}
	heap->bitmap[best].alloc = 1;
	b->block.base = heap->heap_base->block.base + (best << min_shift);
	b->heap = heap;
	b->block.type = BLOCK_BUDDY;
	return &b->block;
}
#endif

static struct buddy_heap *do_buddy_free(struct nvmap_heap_block *block)
{
	struct buddy_block *b = container_of(block, struct buddy_block, block);
	struct buddy_heap *h = b->heap;
	unsigned int min_shift = parent_of(h)->min_buddy_shift;
	unsigned int index;

	index = (block->base - h->heap_base->block.base) >> min_shift;
	h->bitmap[index].alloc = 0;

	for (;;) {
		unsigned int buddy = index ^ (1 << h->bitmap[index].order);
		if (buddy >= h->nr_buddies || h->bitmap[buddy].alloc ||
		    h->bitmap[buddy].order != h->bitmap[index].order)
			break;

		h->bitmap[buddy].order++;
		h->bitmap[index].order++;
		index = min(buddy, index);
	}

	kmem_cache_free(block_cache, b);
	if ((1 << h->bitmap[0].order) == h->nr_buddies)
		return h;

	return NULL;
}


/*
 * base_max limits position of allocated chunk in memory.
 * if base_max is 0 then there is no such limitation.
 */
static struct nvmap_heap_block *do_heap_alloc(struct nvmap_heap *heap,
					      size_t len, size_t align,
					      unsigned int mem_prot,
					      phys_addr_t base_max)
{
	struct list_block *b = NULL;
	struct list_block *i = NULL;
	struct list_block *rem = NULL;
	phys_addr_t fix_base;
	enum direction dir;

	/* since pages are only mappable with one cache attribute,
	 * and most allocations from carveout heaps are DMA coherent
	 * (i.e., non-cacheable), round cacheable allocations up to
	 * a page boundary to ensure that the physical pages will
	 * only be mapped one way. */
	if (mem_prot == NVMAP_HANDLE_CACHEABLE ||
	    mem_prot == NVMAP_HANDLE_INNER_CACHEABLE) {
		align = max_t(size_t, align, PAGE_SIZE);
		len = PAGE_ALIGN(len);
	}

#ifdef CONFIG_NVMAP_CARVEOUT_COMPACTOR
	dir = BOTTOM_UP;
#else
	dir = (len <= heap->small_alloc) ? BOTTOM_UP : TOP_DOWN;
#endif

	if (dir == BOTTOM_UP) {
		list_for_each_entry(i, &heap->free_list, free_list) {
			size_t fix_size;
			fix_base = ALIGN(i->block.base, align);
			if(!fix_base || fix_base >= i->block.base + i->size)
				continue;

			fix_size = i->size - (fix_base - i->block.base);

			/* needed for compaction. relocated chunk
			 * should never go up */
			if (base_max && fix_base > base_max)
				break;

			if (fix_size >= len) {
				b = i;
				break;
			}
		}
	} else {
		list_for_each_entry_reverse(i, &heap->free_list, free_list) {
			if (i->size >= len) {
				fix_base = i->block.base + i->size - len;
				fix_base &= ~(align-1);
				if (fix_base >= i->block.base) {
					b = i;
					break;
				}
			}
		}
	}

	if (!b)
		return NULL;

	if (dir == BOTTOM_UP)
		b->block.type = BLOCK_FIRST_FIT;

	/* split free block */
	if (b->block.base != fix_base) {
		/* insert a new free block before allocated */
		rem = kmem_cache_zalloc(block_cache, GFP_KERNEL);
		if (!rem) {
			b->orig_addr = b->block.base;
			b->block.base = fix_base;
			b->size -= (b->block.base - b->orig_addr);
			goto out;
		}

		rem->block.type = BLOCK_EMPTY;
		rem->block.base = b->block.base;
		rem->orig_addr = rem->block.base;
		rem->size = fix_base - rem->block.base;
		b->block.base = fix_base;
		b->orig_addr = fix_base;
		b->size -= rem->size;
		list_add_tail(&rem->all_list,  &b->all_list);
		list_add_tail(&rem->free_list, &b->free_list);
	}

	b->orig_addr = b->block.base;

	if (b->size > len) {
		/* insert a new free block after allocated */
		rem = kmem_cache_zalloc(block_cache, GFP_KERNEL);
		if (!rem)
			goto out;

		rem->block.type = BLOCK_EMPTY;
		rem->block.base = b->block.base + len;
		rem->size = b->size - len;
		BUG_ON(rem->size > b->size);
		rem->orig_addr = rem->block.base;
		b->size = len;
		list_add(&rem->all_list,  &b->all_list);
		list_add(&rem->free_list, &b->free_list);
	}

out:
	list_del(&b->free_list);
	b->heap = heap;
	b->mem_prot = mem_prot;
	b->align = align;
	return &b->block;
}

#ifdef DEBUG_FREE_LIST
static void freelist_debug(struct nvmap_heap *heap, const char *title,
			   struct list_block *token)
{
	int i;
	struct list_block *n;

	dev_debug(&heap->dev, "%s\n", title);
	i = 0;
	list_for_each_entry(n, &heap->free_list, free_list) {
		dev_debug(&heap->dev, "\t%d [%p..%p]%s\n", i, (void *)n->orig_addr,
			  (void *)(n->orig_addr + n->size),
			  (n == token) ? "<--" : "");
		i++;
	}
}
#else
#define freelist_debug(_heap, _title, _token)	do { } while (0)
#endif

static struct list_block *do_heap_free(struct nvmap_heap_block *block)
{
	struct list_block *b = container_of(block, struct list_block, block);
	struct list_block *n = NULL;
	struct nvmap_heap *heap = b->heap;

	BUG_ON(b->block.base > b->orig_addr);
	b->size += (b->block.base - b->orig_addr);
	b->block.base = b->orig_addr;

	freelist_debug(heap, "free list before", b);

	/* Find position of first free block to the right of freed one */
	list_for_each_entry(n, &heap->free_list, free_list) {
		if (n->block.base > b->block.base)
			break;
	}

	/* Add freed block before found free one */
	list_add_tail(&b->free_list, &n->free_list);
	BUG_ON(list_empty(&b->all_list));

	freelist_debug(heap, "free list pre-merge", b);

	/* merge freed block with next if they connect
	 * freed block becomes bigger, next one is destroyed */
	if (!list_is_last(&b->free_list, &heap->free_list)) {
		n = list_first_entry(&b->free_list, struct list_block, free_list);
		if (n->block.base == b->block.base + b->size) {
			list_del(&n->all_list);
			list_del(&n->free_list);
			BUG_ON(b->orig_addr >= n->orig_addr);
			b->size += n->size;
			kmem_cache_free(block_cache, n);
		}
	}

	/* merge freed block with prev if they connect
	 * previous free block becomes bigger, freed one is destroyed */
	if (b->free_list.prev != &heap->free_list) {
		n = list_entry(b->free_list.prev, struct list_block, free_list);
		if (n->block.base + n->size == b->block.base) {
			list_del(&b->all_list);
			list_del(&b->free_list);
			BUG_ON(n->orig_addr >= b->orig_addr);
			n->size += b->size;
			kmem_cache_free(block_cache, b);
			b = n;
		}
	}

	freelist_debug(heap, "free list after", b);
	b->block.type = BLOCK_EMPTY;
	return b;
}

#ifndef CONFIG_NVMAP_CARVEOUT_COMPACTOR

static struct nvmap_heap_block *do_buddy_alloc(struct nvmap_heap *h,
					       size_t len, size_t align,
					       unsigned int mem_prot)
{
	struct buddy_heap *bh;
	struct nvmap_heap_block *b = NULL;

	list_for_each_entry(bh, &h->buddy_list, buddy_list) {
		b = buddy_alloc(bh, len, align, mem_prot);
		if (b)
			return b;
	}

	/* no buddy heaps could service this allocation: try to create a new
	 * buddy heap instead */
	bh = kmem_cache_zalloc(buddy_heap_cache, GFP_KERNEL);
	if (!bh)
		return NULL;

	b = do_heap_alloc(h, h->buddy_heap_size,
			h->buddy_heap_size, mem_prot, 0);
	if (!b) {
		kmem_cache_free(buddy_heap_cache, bh);
		return NULL;
	}

	bh->heap_base = container_of(b, struct list_block, block);
	bh->nr_buddies = h->buddy_heap_size >> h->min_buddy_shift;
	bh->bitmap[0].alloc = 0;
	bh->bitmap[0].order = order_of(h->buddy_heap_size, h->min_buddy_shift);
	list_add_tail(&bh->buddy_list, &h->buddy_list);
	return buddy_alloc(bh, len, align, mem_prot);
}

#endif

#ifdef CONFIG_NVMAP_CARVEOUT_COMPACTOR

static int do_heap_copy_listblock(struct nvmap_device *dev,
		 phys_addr_t dst_base, phys_addr_t src_base, size_t len)
{
	pte_t **pte_src = NULL;
	pte_t **pte_dst = NULL;
	void *addr_src = NULL;
	void *addr_dst = NULL;
	unsigned long kaddr_src;
	unsigned long kaddr_dst;
	phys_addr_t phys_src = src_base;
	phys_addr_t phys_dst = dst_base;
	unsigned long pfn_src;
	unsigned long pfn_dst;
	int error = 0;

	pgprot_t prot = pgprot_writecombine(pgprot_kernel);

	int page;

	pte_src = nvmap_alloc_pte(dev, &addr_src);
	if (IS_ERR(pte_src)) {
		pr_err("Error when allocating pte_src\n");
		pte_src = NULL;
		error = -1;
		goto fail;
	}

	pte_dst = nvmap_alloc_pte(dev, &addr_dst);
	if (IS_ERR(pte_dst)) {
		pr_err("Error while allocating pte_dst\n");
		pte_dst = NULL;
		error = -1;
		goto fail;
	}

	kaddr_src = (unsigned long)addr_src;
	kaddr_dst = (unsigned long)addr_dst;

	BUG_ON(phys_dst > phys_src);
	BUG_ON((phys_src & PAGE_MASK) != phys_src);
	BUG_ON((phys_dst & PAGE_MASK) != phys_dst);
	BUG_ON((len & PAGE_MASK) != len);

	for (page = 0; page < (len >> PAGE_SHIFT) ; page++) {

		pfn_src = __phys_to_pfn(phys_src) + page;
		pfn_dst = __phys_to_pfn(phys_dst) + page;

		set_pte_at(&init_mm, kaddr_src, *pte_src,
				pfn_pte(pfn_src, prot));
		nvmap_flush_tlb_kernel_page(kaddr_src);

		set_pte_at(&init_mm, kaddr_dst, *pte_dst,
				pfn_pte(pfn_dst, prot));
		nvmap_flush_tlb_kernel_page(kaddr_dst);

		memcpy(addr_dst, addr_src, PAGE_SIZE);
	}

fail:
	if (pte_src)
		nvmap_free_pte(dev, pte_src);
	if (pte_dst)
		nvmap_free_pte(dev, pte_dst);
	return error;
}


static struct nvmap_heap_block *do_heap_relocate_listblock(
		struct list_block *block, bool fast)
{
	struct nvmap_heap_block *heap_block = &block->block;
	struct nvmap_heap_block *heap_block_new = NULL;
	struct nvmap_heap *heap = block->heap;
	struct nvmap_handle *handle = heap_block->handle;
	phys_addr_t src_base = heap_block->base;
	phys_addr_t dst_base;
	size_t src_size = block->size;
	size_t src_align = block->align;
	unsigned int src_prot = block->mem_prot;
	int error = 0;
	struct nvmap_share *share;

	if (!handle) {
		pr_err("INVALID HANDLE!\n");
		return NULL;
	}

	mutex_lock(&handle->lock);

	share = nvmap_get_share_from_dev(handle->dev);

	/* TODO: It is possible to use only handle lock and no share
	 * pin_lock, but then we'll need to lock every handle during
	 * each pinning operation. Need to estimate performance impact
	 * if we decide to simplify locking this way. */
	mutex_lock(&share->pin_lock);

	/* abort if block is pinned */
	if (atomic_read(&handle->pin))
		goto fail;
	/* abort if block is mapped */
	if (atomic_read(&handle->usecount))
		goto fail;

	if (fast) {
		/* Fast compaction path - first allocate, then free. */
		heap_block_new = do_heap_alloc(heap, src_size, src_align,
				src_prot, src_base);
		if (heap_block_new)
			do_heap_free(heap_block);
		else
			goto fail;
	} else {
		/* Full compaction path, first free, then allocate
		 * It is slower but provide best compaction results */
		do_heap_free(heap_block);
		heap_block_new = do_heap_alloc(heap, src_size, src_align,
				src_prot, src_base);
		/* Allocation should always succeed*/
		BUG_ON(!heap_block_new);
	}

	/* update handle */
	handle->carveout = heap_block_new;
	heap_block_new->handle = handle;

	/* copy source data to new block location */
	dst_base = heap_block_new->base;

	/* new allocation should always go lower addresses, or stay same */
	BUG_ON(dst_base > src_base);

	if (dst_base != src_base) {
		error = do_heap_copy_listblock(handle->dev,
					dst_base, src_base, src_size);
		BUG_ON(error);
	}

fail:
	mutex_unlock(&share->pin_lock);
	mutex_unlock(&handle->lock);
	return heap_block_new;
}

static void nvmap_heap_compact(struct nvmap_heap *heap,
				size_t requested_size, bool fast)
{
	struct list_block *block_current = NULL;
	struct list_block *block_prev = NULL;
	struct list_block *block_next = NULL;

	struct list_head *ptr, *ptr_prev, *ptr_next;
	int relocation_count = 0;

	ptr = heap->all_list.next;

	/* walk through all blocks */
	while (ptr != &heap->all_list) {
		block_current = list_entry(ptr, struct list_block, all_list);

		ptr_prev = ptr->prev;
		ptr_next = ptr->next;

		if (block_current->block.type != BLOCK_EMPTY) {
			ptr = ptr_next;
			continue;
		}

		if (fast && block_current->size >= requested_size)
			break;

		/* relocate prev block */
		if (ptr_prev != &heap->all_list) {

			block_prev = list_entry(ptr_prev,
					struct list_block, all_list);

			BUG_ON(block_prev->block.type != BLOCK_FIRST_FIT);

			if (do_heap_relocate_listblock(block_prev, true)) {
				/* After relocation current free block can be
				 * destroyed when it is merged with previous
				 * free block. Updated pointer to new free
				 * block can be obtained from the next block */
				relocation_count++;
				ptr = ptr_next->prev;
				continue;
			}
		}

		if (ptr_next != &heap->all_list) {
			struct nvmap_heap_block *block_new;
			phys_addr_t old_base;

			block_next = list_entry(ptr_next,
					struct list_block, all_list);

			BUG_ON(block_next->block.type != BLOCK_FIRST_FIT);

			old_base = block_next->block.base;
			block_new = do_heap_relocate_listblock(block_next,
					fast);
			if (block_new && block_new->base == old_base) {
				/* When doing full heap compaction, the block
				 * can end up relocated in the same location.
				 * That means nothing was really accomplished,
				 * but block pointers got invalidated. */
				ptr_next = ptr_prev->next->next;
			} else if (block_new) {
				ptr = ptr_prev->next;
				relocation_count++;
				continue;
			}
		}
		ptr = ptr_next;
	}
	pr_err("Relocated %d chunks\n", relocation_count);
}
#endif

void nvmap_usecount_inc(struct nvmap_handle *h)
{
#ifdef CONFIG_NVMAP_CARVEOUT_COMPACTOR
	if (h && !h->heap_pgalloc) {
		mutex_lock(&h->lock);
		atomic_inc(&h->usecount);
		mutex_unlock(&h->lock);
	}
#endif
}


void nvmap_usecount_dec(struct nvmap_handle *h)
{
#ifdef CONFIG_NVMAP_CARVEOUT_COMPACTOR
	if (h && !h->heap_pgalloc) {
		BUG_ON(atomic_read(&h->usecount) == 0);
		atomic_dec(&h->usecount);
	}
#endif
}

/* nvmap_heap_alloc: allocates a block of memory of len bytes, aligned to
 * align bytes. */
struct nvmap_heap_block *nvmap_heap_alloc(struct nvmap_heap *h,
					  struct nvmap_handle *handle)
{
	struct nvmap_heap_block *b;
	size_t len        = handle->size;
	size_t align      = handle->align;
	unsigned int prot = handle->flags;

	mutex_lock(&h->lock);

#ifdef CONFIG_NVMAP_CARVEOUT_COMPACTOR
	/* Align to page size */
	align = ALIGN(align, PAGE_SIZE);
	len = ALIGN(len, PAGE_SIZE);
	b = do_heap_alloc(h, len, align, prot, 0);
	if (!b) {
		pr_err("Compaction triggered!\n");
		nvmap_heap_compact(h, len, true);
		b = do_heap_alloc(h, len, align, prot, 0);
		if (!b) {
			pr_err("Full compaction triggered!\n");
			nvmap_heap_compact(h, len, false);
			b = do_heap_alloc(h, len, align, prot, 0);
		}
	}
#else
	if (len <= h->buddy_heap_size / 2) {
		b = do_buddy_alloc(h, len, align, prot);
	} else {
		if (h->buddy_heap_size)
			len = ALIGN(len, h->buddy_heap_size);
		align = max(align, (size_t)L1_CACHE_BYTES);
		b = do_heap_alloc(h, len, align, prot, 0);
	}
#endif

	if (b) {
		b->handle = handle;
		handle->carveout = b;
	}
	mutex_unlock(&h->lock);
	return b;
}

struct nvmap_heap *nvmap_block_to_heap(struct nvmap_heap_block *b)
{
	if (b->type == BLOCK_BUDDY) {
		struct buddy_block *bb;
		bb = container_of(b, struct buddy_block, block);
		return parent_of(bb->heap);
	} else {
		struct list_block *lb;
		lb = container_of(b, struct list_block, block);
		return lb->heap;
	}
}

/* nvmap_heap_free: frees block b*/
void nvmap_heap_free(struct nvmap_heap_block *b)
{
	struct buddy_heap *bh = NULL;
	struct nvmap_heap *h = nvmap_block_to_heap(b);
	struct list_block *lb;

	mutex_lock(&h->lock);
	if (b->type == BLOCK_BUDDY)
		bh = do_buddy_free(b);
	else {
		lb = container_of(b, struct list_block, block);
		nvmap_flush_heap_block(NULL, b, lb->size, lb->mem_prot);
		do_heap_free(b);
	}

	if (bh) {
		list_del(&bh->buddy_list);
		mutex_unlock(&h->lock);
		nvmap_heap_free(&bh->heap_base->block);
		kmem_cache_free(buddy_heap_cache, bh);
	} else
		mutex_unlock(&h->lock);
}


static void heap_release(struct device *heap)
{
}

/* nvmap_heap_create: create a heap object of len bytes, starting from
 * address base.
 *
 * if buddy_size is >= NVMAP_HEAP_MIN_BUDDY_SIZE, then allocations <= 1/2
 * of the buddy heap size will use a buddy sub-allocator, where each buddy
 * heap is buddy_size bytes (should be a power of 2). all other allocations
 * will be rounded up to be a multiple of buddy_size bytes.
 */
struct nvmap_heap *nvmap_heap_create(struct device *parent, const char *name,
				     phys_addr_t base, size_t len,
				     size_t buddy_size, void *arg)
{
	struct nvmap_heap *h = NULL;
	struct list_block *l = NULL;

	if (WARN_ON(buddy_size && buddy_size < NVMAP_HEAP_MIN_BUDDY_SIZE)) {
		dev_warn(parent, "%s: buddy_size %u too small\n", __func__,
			buddy_size);
		buddy_size = 0;
	} else if (WARN_ON(buddy_size >= len)) {
		dev_warn(parent, "%s: buddy_size %u too large\n", __func__,
			buddy_size);
		buddy_size = 0;
	} else if (WARN_ON(buddy_size & (buddy_size - 1))) {
		dev_warn(parent, "%s: buddy_size %u not a power of 2\n",
			 __func__, buddy_size);
		buddy_size = 1 << (ilog2(buddy_size) + 1);
	}

	if (WARN_ON(buddy_size && (base & (buddy_size - 1)))) {
		phys_addr_t orig = base;
		dev_warn(parent, "%s: base address %p not aligned to "
			 "buddy_size %u\n", __func__, (void *)base, buddy_size);
		base = ALIGN(base, buddy_size);
		len -= (base - orig);
	}

	if (WARN_ON(buddy_size && (len & (buddy_size - 1)))) {
		dev_warn(parent, "%s: length %u not aligned to "
			 "buddy_size %u\n", __func__, len, buddy_size);
		len &= ~(buddy_size - 1);
	}

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h) {
		dev_err(parent, "%s: out of memory\n", __func__);
		goto fail_alloc;
	}

	l = kmem_cache_zalloc(block_cache, GFP_KERNEL);
	if (!l) {
		dev_err(parent, "%s: out of memory\n", __func__);
		goto fail_alloc;
	}

	dev_set_name(&h->dev, "heap-%s", name);
	h->name = name;
	h->arg = arg;
	h->dev.parent = parent;
	h->dev.driver = NULL;
	h->dev.release = heap_release;
	if (device_register(&h->dev)) {
		dev_err(parent, "%s: failed to register %s\n", __func__,
			dev_name(&h->dev));
		goto fail_alloc;
	}
	if (sysfs_create_group(&h->dev.kobj, &heap_stat_attr_group)) {
		dev_err(&h->dev, "%s: failed to create attributes\n", __func__);
		goto fail_register;
	}
	h->small_alloc = max(2 * buddy_size, len / 256);
	h->buddy_heap_size = buddy_size;
	if (buddy_size)
		h->min_buddy_shift = ilog2(buddy_size / MAX_BUDDY_NR);
	INIT_LIST_HEAD(&h->free_list);
	INIT_LIST_HEAD(&h->buddy_list);
	INIT_LIST_HEAD(&h->all_list);
	mutex_init(&h->lock);
	l->block.base = base;
	l->block.type = BLOCK_EMPTY;
	l->size = len;
	l->orig_addr = base;
	list_add_tail(&l->free_list, &h->free_list);
	list_add_tail(&l->all_list, &h->all_list);

	inner_flush_cache_all();
	outer_flush_range(base, base + len);
	wmb();
	return h;

fail_register:
	device_unregister(&h->dev);
fail_alloc:
	if (l)
		kmem_cache_free(block_cache, l);
	kfree(h);
	return NULL;
}

void *nvmap_heap_device_to_arg(struct device *dev)
{
	struct nvmap_heap *heap = container_of(dev, struct nvmap_heap, dev);
	return heap->arg;
}

void *nvmap_heap_to_arg(struct nvmap_heap *heap)
{
	return heap->arg;
}

/* nvmap_heap_destroy: frees all resources in heap */
void nvmap_heap_destroy(struct nvmap_heap *heap)
{
	WARN_ON(!list_empty(&heap->buddy_list));

	sysfs_remove_group(&heap->dev.kobj, &heap_stat_attr_group);
	device_unregister(&heap->dev);

	while (!list_empty(&heap->buddy_list)) {
		struct buddy_heap *b;
		b = list_first_entry(&heap->buddy_list, struct buddy_heap,
				     buddy_list);
		list_del(&heap->buddy_list);
		nvmap_heap_free(&b->heap_base->block);
		kmem_cache_free(buddy_heap_cache, b);
	}

	WARN_ON(!list_is_singular(&heap->all_list));
	while (!list_empty(&heap->all_list)) {
		struct list_block *l;
		l = list_first_entry(&heap->all_list, struct list_block,
				     all_list);
		list_del(&l->all_list);
		kmem_cache_free(block_cache, l);
	}

	kfree(heap);
}

/* nvmap_heap_create_group: adds the attribute_group grp to the heap kobject */
int nvmap_heap_create_group(struct nvmap_heap *heap,
			    const struct attribute_group *grp)
{
	return sysfs_create_group(&heap->dev.kobj, grp);
}

/* nvmap_heap_remove_group: removes the attribute_group grp  */
void nvmap_heap_remove_group(struct nvmap_heap *heap,
			     const struct attribute_group *grp)
{
	sysfs_remove_group(&heap->dev.kobj, grp);
}

int nvmap_heap_init(void)
{
	BUG_ON(buddy_heap_cache != NULL);
	buddy_heap_cache = KMEM_CACHE(buddy_heap, 0);
	if (!buddy_heap_cache) {
		pr_err("%s: unable to create buddy heap cache\n", __func__);
		return -ENOMEM;
	}

	block_cache = KMEM_CACHE(combo_block, 0);
	if (!block_cache) {
		kmem_cache_destroy(buddy_heap_cache);
		pr_err("%s: unable to create block cache\n", __func__);
		return -ENOMEM;
	}
	return 0;
}

void nvmap_heap_deinit(void)
{
	if (buddy_heap_cache)
		kmem_cache_destroy(buddy_heap_cache);
	if (block_cache)
		kmem_cache_destroy(block_cache);

	block_cache = NULL;
	buddy_heap_cache = NULL;
}
