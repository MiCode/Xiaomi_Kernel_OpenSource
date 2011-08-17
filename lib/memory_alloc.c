/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <asm/page.h>
#include <linux/io.h>
#include <linux/memory_alloc.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/log2.h>

#define MAX_MEMPOOLS 8

struct mem_pool mpools[MAX_MEMPOOLS];

/* The tree contains all allocations over all memory pools */
static struct rb_root alloc_root;
static struct mutex alloc_mutex;

static struct alloc *find_alloc(void *addr)
{
	struct rb_root *root = &alloc_root;
	struct rb_node *p = root->rb_node;

	mutex_lock(&alloc_mutex);

	while (p) {
		struct alloc *node;

		node = rb_entry(p, struct alloc, rb_node);
		if (addr < node->vaddr)
			p = p->rb_left;
		else if (addr > node->vaddr)
			p = p->rb_right;
		else {
			mutex_unlock(&alloc_mutex);
			return node;
		}
	}
	mutex_unlock(&alloc_mutex);
	return NULL;
}

static int add_alloc(struct alloc *node)
{
	struct rb_root *root = &alloc_root;
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;

	mutex_lock(&alloc_mutex);
	while (*p) {
		struct alloc *tmp;
		parent = *p;

		tmp = rb_entry(parent, struct alloc, rb_node);

		if (node->vaddr < tmp->vaddr)
			p = &(*p)->rb_left;
		else if (node->vaddr > tmp->vaddr)
			p = &(*p)->rb_right;
		else {
			WARN(1, "memory at %p already allocated", tmp->vaddr);
			mutex_unlock(&alloc_mutex);
			return -EINVAL;
		}
	}
	rb_link_node(&node->rb_node, parent, p);
	rb_insert_color(&node->rb_node, root);
	mutex_unlock(&alloc_mutex);
	return 0;
}

static int remove_alloc(struct alloc *victim_node)
{
	struct rb_root *root = &alloc_root;
	if (!victim_node)
		return -EINVAL;

	mutex_lock(&alloc_mutex);
	rb_erase(&victim_node->rb_node, root);
	mutex_unlock(&alloc_mutex);
	return 0;
}

static struct gen_pool *initialize_gpool(unsigned long start,
	unsigned long size)
{
	struct gen_pool *gpool;

	gpool = gen_pool_create(PAGE_SHIFT, -1);

	if (!gpool)
		return NULL;
	if (gen_pool_add(gpool, start, size, -1)) {
		gen_pool_destroy(gpool);
		return NULL;
	}

	return gpool;
}

static void *__alloc(struct mem_pool *mpool, unsigned long size,
	unsigned long align, int cached)
{
	unsigned long paddr;
	void __iomem *vaddr;

	unsigned long aligned_size;
	int log_align = ilog2(align);

	struct alloc *node;

	aligned_size = PFN_ALIGN(size);
	paddr = gen_pool_alloc_aligned(mpool->gpool, aligned_size, log_align);
	if (!paddr)
		return NULL;

	node = kmalloc(sizeof(struct alloc), GFP_KERNEL);
	if (!node)
		goto out;

	if (cached)
		vaddr = ioremap_cached(paddr, aligned_size);
	else
		vaddr = ioremap(paddr, aligned_size);

	if (!vaddr)
		goto out_kfree;

	node->vaddr = vaddr;
	node->paddr = paddr;
	node->len = aligned_size;
	node->mpool = mpool;
	if (add_alloc(node))
		goto out_kfree;

	mpool->free -= aligned_size;

	return vaddr;
out_kfree:
	if (vaddr)
		iounmap(vaddr);
	kfree(node);
out:
	gen_pool_free(mpool->gpool, paddr, aligned_size);
	return NULL;
}

static void __free(void *vaddr, bool unmap)
{
	struct alloc *node = find_alloc(vaddr);

	if (!node)
		return;

	if (unmap)
		iounmap(node->vaddr);

	gen_pool_free(node->mpool->gpool, node->paddr, node->len);
	node->mpool->free += node->len;

	remove_alloc(node);
	kfree(node);
}

static struct mem_pool *mem_type_to_memory_pool(int mem_type)
{
	struct mem_pool *mpool = &mpools[mem_type];

	if (!mpool->size)
		return NULL;

	mutex_lock(&mpool->pool_mutex);
	if (!mpool->gpool)
		mpool->gpool = initialize_gpool(mpool->paddr, mpool->size);
	mutex_unlock(&mpool->pool_mutex);
	if (!mpool->gpool)
		return NULL;

	return mpool;
}

struct mem_pool *initialize_memory_pool(unsigned long start,
	unsigned long size, int mem_type)
{
	int id = mem_type;

	if (id >= MAX_MEMPOOLS || size <= PAGE_SIZE || size % PAGE_SIZE)
		return NULL;

	mutex_lock(&mpools[id].pool_mutex);

	mpools[id].paddr = start;
	mpools[id].size = size;
	mpools[id].free = size;
	mutex_unlock(&mpools[id].pool_mutex);

	pr_info("memory pool %d (start %lx size %lx) initialized\n",
		id, start, size);
	return &mpools[id];
}
EXPORT_SYMBOL_GPL(initialize_memory_pool);

void *allocate_contiguous_memory(unsigned long size,
	int mem_type, unsigned long align, int cached)
{
	unsigned long aligned_size = PFN_ALIGN(size);
	struct mem_pool *mpool;

	mpool = mem_type_to_memory_pool(mem_type);
	if (!mpool)
		return NULL;
	return __alloc(mpool, aligned_size, align, cached);

}
EXPORT_SYMBOL_GPL(allocate_contiguous_memory);

unsigned long allocate_contiguous_memory_nomap(unsigned long size,
	int mem_type, unsigned long align)
{
	unsigned long paddr;
	unsigned long aligned_size;

	struct alloc *node;
	struct mem_pool *mpool;
	int log_align = ilog2(align);

	mpool = mem_type_to_memory_pool(mem_type);
	if (!mpool || !mpool->gpool)
		return 0;

	aligned_size = PFN_ALIGN(size);
	paddr = gen_pool_alloc_aligned(mpool->gpool, aligned_size, log_align);
	if (!paddr)
		return 0;

	node = kmalloc(sizeof(struct alloc), GFP_KERNEL);
	if (!node)
		goto out;

	node->paddr = paddr;

	/* We search the tree using node->vaddr, so set
	 * it to something unique even though we don't
	 * use it for physical allocation nodes.
	 * The virtual and physical address ranges
	 * are disjoint, so there won't be any chance of
	 * a duplicate node->vaddr value.
	 */
	node->vaddr = (void *)paddr;
	node->len = aligned_size;
	node->mpool = mpool;
	if (add_alloc(node))
		goto out_kfree;

	mpool->free -= aligned_size;
	return paddr;
out_kfree:
	kfree(node);
out:
	gen_pool_free(mpool->gpool, paddr, aligned_size);
	return 0;
}
EXPORT_SYMBOL_GPL(allocate_contiguous_memory_nomap);

void free_contiguous_memory(void *addr)
{
	if (!addr)
		return;
	__free(addr, true);
	return;
}
EXPORT_SYMBOL_GPL(free_contiguous_memory);

void free_contiguous_memory_by_paddr(unsigned long paddr)
{
	if (!paddr)
		return;
	__free((void *)paddr, false);
	return;
}
EXPORT_SYMBOL_GPL(free_contiguous_memory_by_paddr);

unsigned long memory_pool_node_paddr(void *vaddr)
{
	struct alloc *node = find_alloc(vaddr);

	if (!node)
		return -EINVAL;

	return node->paddr;
}
EXPORT_SYMBOL_GPL(memory_pool_node_paddr);

unsigned long memory_pool_node_len(void *vaddr)
{
	struct alloc *node = find_alloc(vaddr);

	if (!node)
		return -EINVAL;

	return node->len;
}
EXPORT_SYMBOL_GPL(memory_pool_node_len);

int __init memory_pool_init(void)
{
	int i;

	alloc_root = RB_ROOT;
	mutex_init(&alloc_mutex);
	for (i = 0; i < ARRAY_SIZE(mpools); i++) {
		mutex_init(&mpools[i].pool_mutex);
		mpools[i].gpool = NULL;
	}
	return 0;
}
