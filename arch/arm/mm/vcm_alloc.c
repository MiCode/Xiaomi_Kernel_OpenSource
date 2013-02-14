/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/vcm.h>
#include <linux/vcm_alloc.h>
#include <linux/string.h>
#include <asm/sizes.h>

int basicalloc_init;

#define vcm_alloc_err(a, ...)						\
	pr_err("ERROR %s %i " a, __func__, __LINE__, ##__VA_ARGS__)

struct phys_chunk_head {
	struct list_head head;
	int num;
};

struct phys_pool {
	int size;
	int chunk_size;
	struct phys_chunk_head head;
};

static int vcm_num_phys_pools;
static int vcm_num_memtypes;
static struct phys_pool *vcm_phys_pool;
static struct vcm_memtype_map *memtype_map;

static int num_pools(enum memtype_t memtype)
{
	if (memtype >= vcm_num_memtypes) {
		vcm_alloc_err("Bad memtype: %d\n", memtype);
		return -EINVAL;
	}
	return memtype_map[memtype].num_pools;
}

static int pool_chunk_size(enum memtype_t memtype, int prio_idx)
{
	int pool_idx;
	if (memtype >= vcm_num_memtypes) {
		vcm_alloc_err("Bad memtype: %d\n", memtype);
		return -EINVAL;
	}

	if (prio_idx >= num_pools(memtype)) {
		vcm_alloc_err("Bad prio index: %d, max=%d, mt=%d\n", prio_idx,
			      num_pools(memtype), memtype);
		return -EINVAL;
	}

	pool_idx = memtype_map[memtype].pool_id[prio_idx];
	return vcm_phys_pool[pool_idx].chunk_size;
}

int vcm_alloc_pool_idx_to_size(int pool_idx)
{
	if (pool_idx >= vcm_num_phys_pools) {
		vcm_alloc_err("Bad pool index: %d\n, max=%d\n", pool_idx,
			      vcm_num_phys_pools);
		return -EINVAL;
	}
	return vcm_phys_pool[pool_idx].chunk_size;
}

static struct phys_chunk_head *get_chunk_list(enum memtype_t memtype,
					      int prio_idx)
{
	unsigned int pool_idx;

	if (memtype >= vcm_num_memtypes) {
		vcm_alloc_err("Bad memtype: %d\n", memtype);
		return NULL;
	}

	if (prio_idx >= num_pools(memtype)) {
		vcm_alloc_err("bad chunk size: mt=%d, prioidx=%d, np=%d\n",
			      memtype, prio_idx, num_pools(memtype));
		BUG();
		return NULL;
	}

	if (!vcm_phys_pool) {
		vcm_alloc_err("phys_pool is null\n");
		return NULL;
	}

	/* We don't have a "pool count" anywhere but this is coming
	 * strictly from data in a board file
	 */
	pool_idx = memtype_map[memtype].pool_id[prio_idx];

	return &vcm_phys_pool[pool_idx].head;
}

static int is_allocated(struct list_head *allocated)
{
	/* This should not happen under normal conditions */
	if (!allocated) {
		vcm_alloc_err("no allocated\n");
		return 0;
	}

	if (!basicalloc_init) {
		vcm_alloc_err("no basicalloc_init\n");
		return 0;
	}
	return !list_empty(allocated);
}

static int count_allocated_size(enum memtype_t memtype, int idx)
{
	int cnt = 0;
	struct phys_chunk *chunk, *tmp;
	struct phys_chunk_head *pch;

	if (!basicalloc_init) {
		vcm_alloc_err("no basicalloc_init\n");
		return 0;
	}

	pch = get_chunk_list(memtype, idx);
	if (!pch) {
		vcm_alloc_err("null pch\n");
		return -EINVAL;
	}

	list_for_each_entry_safe(chunk, tmp, &pch->head, list) {
		if (is_allocated(&chunk->allocated))
			cnt++;
	}

	return cnt;
}


int vcm_alloc_get_mem_size(void)
{
	if (!vcm_phys_pool) {
		vcm_alloc_err("No physical pool set up!\n");
		return -ENODEV;
	}
	return vcm_phys_pool[0].size;
}
EXPORT_SYMBOL(vcm_alloc_get_mem_size);

void vcm_alloc_print_list(enum memtype_t memtype, int just_allocated)
{
	int i;
	struct phys_chunk *chunk, *tmp;
	struct phys_chunk_head *pch;

	if (!basicalloc_init) {
		vcm_alloc_err("no basicalloc_init\n");
		return;
	}

	for (i = 0; i < num_pools(memtype); ++i) {
		pch = get_chunk_list(memtype, i);

		if (!pch) {
			vcm_alloc_err("pch is null\n");
			return;
		}

		if (list_empty(&pch->head))
			continue;

		list_for_each_entry_safe(chunk, tmp, &pch->head, list) {
			if (just_allocated && !is_allocated(&chunk->allocated))
				continue;

			printk(KERN_INFO "pa = %#x, size = %#x\n",
			chunk->pa, vcm_phys_pool[chunk->pool_idx].chunk_size);
		}
	}
}
EXPORT_SYMBOL(vcm_alloc_print_list);

int vcm_alloc_blocks_avail(enum memtype_t memtype, int idx)
{
	struct phys_chunk_head *pch;
	if (!basicalloc_init) {
		vcm_alloc_err("no basicalloc_init\n");
		return 0;
	}
	pch = get_chunk_list(memtype, idx);

	if (!pch) {
		vcm_alloc_err("pch is null\n");
		return 0;
	}
	return pch->num;
}
EXPORT_SYMBOL(vcm_alloc_blocks_avail);


int vcm_alloc_get_num_chunks(enum memtype_t memtype)
{
	return num_pools(memtype);
}
EXPORT_SYMBOL(vcm_alloc_get_num_chunks);


int vcm_alloc_all_blocks_avail(enum memtarget_t memtype)
{
	int i;
	int cnt = 0;

	if (!basicalloc_init) {
		vcm_alloc_err("no basicalloc_init\n");
		return 0;
	}

	for (i = 0; i < num_pools(memtype); ++i)
		cnt += vcm_alloc_blocks_avail(memtype, i);
	return cnt;
}
EXPORT_SYMBOL(vcm_alloc_all_blocks_avail);


int vcm_alloc_count_allocated(enum memtype_t memtype)
{
	int i;
	int cnt = 0;

	if (!basicalloc_init) {
		vcm_alloc_err("no basicalloc_init\n");
		return 0;
	}

	for (i = 0; i < num_pools(memtype); ++i)
		cnt += count_allocated_size(memtype, i);
	return cnt;
}
EXPORT_SYMBOL(vcm_alloc_count_allocated);

int vcm_alloc_destroy(void)
{
	int i, mt;
	struct phys_chunk *chunk, *tmp;

	if (!basicalloc_init) {
		vcm_alloc_err("no basicalloc_init\n");
		return -ENODEV;
	}

	/* can't destroy a space that has allocations */
	for (mt = 0; mt < vcm_num_memtypes; mt++)
		if (vcm_alloc_count_allocated(mt)) {
			vcm_alloc_err("allocations still present\n");
			return -EBUSY;
		}

	for (i = 0; i < vcm_num_phys_pools; i++) {
		struct phys_chunk_head *pch = &vcm_phys_pool[i].head;

		if (list_empty(&pch->head))
			continue;
		list_for_each_entry_safe(chunk, tmp, &pch->head, list) {
			list_del(&chunk->list);
			memset(chunk, 0, sizeof(*chunk));
			kfree(chunk);
		}
		vcm_phys_pool[i].head.num = 0;
	}

	kfree(vcm_phys_pool);
	kfree(memtype_map);

	vcm_phys_pool = NULL;
	memtype_map = NULL;
	basicalloc_init = 0;
	vcm_num_phys_pools = 0;
	return 0;
}
EXPORT_SYMBOL(vcm_alloc_destroy);


int vcm_alloc_init(struct physmem_region *mem, int n_regions,
		   struct vcm_memtype_map *mt_map, int n_mt)
{
	int i = 0, j = 0, r = 0, num_chunks;
	struct phys_chunk *chunk;
	struct phys_chunk_head *pch = NULL;
	unsigned long pa;

	/* no double inits */
	if (basicalloc_init) {
		vcm_alloc_err("double basicalloc_init\n");
		BUG();
		goto fail;
	}
	memtype_map = kzalloc(sizeof(*mt_map) * n_mt, GFP_KERNEL);
	if (!memtype_map) {
		vcm_alloc_err("Could not copy memtype map\n");
		goto fail;
	}
	memcpy(memtype_map, mt_map, sizeof(*mt_map) * n_mt);

	vcm_phys_pool = kzalloc(sizeof(*vcm_phys_pool) * n_regions, GFP_KERNEL);
	vcm_num_phys_pools = n_regions;
	vcm_num_memtypes = n_mt;

	if (!vcm_phys_pool) {
		vcm_alloc_err("Could not allocate physical pool structure\n");
		goto fail;
	}

	/* separate out to ensure good cleanup */
	for (i = 0; i < n_regions; i++) {
		pch = &vcm_phys_pool[i].head;
		INIT_LIST_HEAD(&pch->head);
		pch->num = 0;
	}

	for (r = 0; r < n_regions; r++) {
		pa = mem[r].addr;
		vcm_phys_pool[r].size = mem[r].size;
		vcm_phys_pool[r].chunk_size = mem[r].chunk_size;
		pch = &vcm_phys_pool[r].head;

		num_chunks = mem[r].size / mem[r].chunk_size;

		printk(KERN_INFO "VCM Init: region %d, chunk size=%d, "
		       "num=%d, pa=%p\n", r, mem[r].chunk_size, num_chunks,
		       (void *)pa);

		for (j = 0; j < num_chunks; ++j) {
			chunk = kzalloc(sizeof(*chunk), GFP_KERNEL);
			if (!chunk) {
				vcm_alloc_err("null chunk\n");
				goto fail;
			}
			chunk->pa = pa;
			chunk->size = mem[r].chunk_size;
			pa += mem[r].chunk_size;
			chunk->pool_idx = r;
			INIT_LIST_HEAD(&chunk->allocated);
			list_add_tail(&chunk->list, &pch->head);
			pch->num++;
		}
	}

	basicalloc_init = 1;
	return 0;
fail:
	vcm_alloc_destroy();
	return -EINVAL;
}
EXPORT_SYMBOL(vcm_alloc_init);


int vcm_alloc_free_blocks(enum memtype_t memtype, struct phys_chunk *alloc_head)
{
	struct phys_chunk *chunk, *tmp;
	struct phys_chunk_head *pch = NULL;

	if (!basicalloc_init) {
		vcm_alloc_err("no basicalloc_init\n");
		goto fail;
	}

	if (!alloc_head) {
		vcm_alloc_err("no alloc_head\n");
		goto fail;
	}

	list_for_each_entry_safe(chunk, tmp, &alloc_head->allocated,
				 allocated) {
		list_del_init(&chunk->allocated);
		pch = &vcm_phys_pool[chunk->pool_idx].head;

		if (!pch) {
			vcm_alloc_err("null pch\n");
			goto fail;
		}
		pch->num++;
	}

	return 0;
fail:
	return -ENODEV;
}
EXPORT_SYMBOL(vcm_alloc_free_blocks);


int vcm_alloc_num_blocks(int num, enum memtype_t memtype, int idx,
			 struct phys_chunk *alloc_head)
{
	struct phys_chunk *chunk;
	struct phys_chunk_head *pch = NULL;
	int num_allocated = 0;

	if (!basicalloc_init) {
		vcm_alloc_err("no basicalloc_init\n");
		goto fail;
	}

	if (!alloc_head) {
		vcm_alloc_err("no alloc_head\n");
		goto fail;
	}

	pch = get_chunk_list(memtype, idx);

	if (!pch) {
		vcm_alloc_err("null pch\n");
		goto fail;
	}
	if (list_empty(&pch->head)) {
		vcm_alloc_err("list is empty\n");
		goto fail;
	}

	if (vcm_alloc_blocks_avail(memtype, idx) < num) {
		vcm_alloc_err("not enough blocks? num=%d\n", num);
		goto fail;
	}

	list_for_each_entry(chunk, &pch->head, list) {
		if (num_allocated == num)
			break;
		if (is_allocated(&chunk->allocated))
			continue;

		list_add_tail(&chunk->allocated, &alloc_head->allocated);
		pch->num--;
		num_allocated++;
	}
	return num_allocated;
fail:
	return 0;
}
EXPORT_SYMBOL(vcm_alloc_num_blocks);


int vcm_alloc_max_munch(int len, enum memtype_t memtype,
			struct phys_chunk *alloc_head)
{
	int i;

	int blocks_req = 0;
	int block_residual = 0;
	int blocks_allocated = 0;
	int cur_chunk_size = 0;
	int ba = 0;

	if (!basicalloc_init) {
		vcm_alloc_err("basicalloc_init is 0\n");
		goto fail;
	}

	if (!alloc_head) {
		vcm_alloc_err("alloc_head is NULL\n");
		goto fail;
	}

	if (num_pools(memtype) <= 0) {
		vcm_alloc_err("Memtype %d has improper mempool configuration\n",
			      memtype);
		goto fail;
	}

	for (i = 0; i < num_pools(memtype); ++i) {
		cur_chunk_size = pool_chunk_size(memtype, i);
		if (cur_chunk_size <= 0) {
			vcm_alloc_err("Bad chunk size: %d\n", cur_chunk_size);
			goto fail;
		}

		blocks_req = len / cur_chunk_size;
		block_residual = len % cur_chunk_size;

		len = block_residual; /* len left */
		if (blocks_req) {
			int blocks_available = 0;
			int blocks_diff = 0;
			int bytes_diff = 0;

			blocks_available = vcm_alloc_blocks_avail(memtype, i);
			if (blocks_available < blocks_req) {
				blocks_diff =
					(blocks_req - blocks_available);
				bytes_diff =
					blocks_diff * cur_chunk_size;

				/* add back in the rest */
				len += bytes_diff;
			} else {
				/* got all the blocks I need */
				blocks_available =
					(blocks_available > blocks_req)
					? blocks_req : blocks_available;
			}

			ba = vcm_alloc_num_blocks(blocks_available, memtype, i,
						  alloc_head);

			if (ba != blocks_available) {
				vcm_alloc_err("blocks allocated (%i) !="
					      " blocks_available (%i):"
					      " chunk size = %#x,"
					      " alloc_head = %p\n",
					      ba, blocks_available,
					      i, (void *) alloc_head);
				goto fail;
			}
			blocks_allocated += blocks_available;
		}
	}

	if (len) {
		int blocks_available = 0;
		int last_sz = num_pools(memtype) - 1;
		blocks_available = vcm_alloc_blocks_avail(memtype, last_sz);

		if (blocks_available > 0) {
			ba = vcm_alloc_num_blocks(1, memtype, last_sz,
						  alloc_head);
			if (ba != 1) {
				vcm_alloc_err("blocks allocated (%i) !="
					      " blocks_available (%i):"
					      " chunk size = %#x,"
					      " alloc_head = %p\n",
					      ba, 1,
					      last_sz,
					      (void *) alloc_head);
				goto fail;
			}
			blocks_allocated += 1;
		} else {
			vcm_alloc_err("blocks_available (%#x) <= 1\n",
				      blocks_available);
			goto fail;
		}
	}

	return blocks_allocated;
fail:
	vcm_alloc_free_blocks(memtype, alloc_head);
	return 0;
}
EXPORT_SYMBOL(vcm_alloc_max_munch);
