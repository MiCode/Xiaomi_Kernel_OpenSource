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

#include <asm/page.h>
#include <linux/io.h>
#include <linux/memory_alloc.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/log2.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>


#define MAX_MEMPOOLS 8

struct mem_pool mpools[MAX_MEMPOOLS];

/* The tree contains all allocations over all memory pools */
static struct rb_root alloc_root;
static struct mutex alloc_mutex;

static void *s_start(struct seq_file *m, loff_t *pos)
	__acquires(&alloc_mutex)
{
	loff_t n = *pos;
	struct rb_node *r;

	mutex_lock(&alloc_mutex);
	r = rb_first(&alloc_root);

	while (n > 0 && r) {
		n--;
		r = rb_next(r);
	}
	if (!n)
		return r;
	return NULL;
}

static void *s_next(struct seq_file *m, void *p, loff_t *pos)
{
	struct rb_node *r = p;
	++*pos;
	return rb_next(r);
}

static void s_stop(struct seq_file *m, void *p)
	__releases(&alloc_mutex)
{
	mutex_unlock(&alloc_mutex);
}

static int s_show(struct seq_file *m, void *p)
{
	struct rb_node *r = p;
	struct alloc *node = rb_entry(r, struct alloc, rb_node);

	seq_printf(m, "0x%pa 0x%pa %ld %u %pS\n", &node->paddr, &node->vaddr,
		   node->len, node->mpool->id, node->caller);
	return 0;
}

static const struct seq_operations mempool_op = {
	.start = s_start,
	.next = s_next,
	.stop = s_stop,
	.show = s_show,
};

static int mempool_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &mempool_op);
}

static struct alloc *find_alloc(phys_addr_t addr)
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
			WARN(1, "memory at %pa already allocated", &tmp->vaddr);
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

static struct gen_pool *initialize_gpool(phys_addr_t start,
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
	unsigned long align, int cached, void *caller)
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

	/*
	 * Just cast to an unsigned long to avoid warnings about casting from a
	 * pointer to an integer of different size. The pointer is only 32-bits
	 * so we lose no data.
	 */
	node->vaddr = (unsigned long)vaddr;
	node->paddr = paddr;
	node->len = aligned_size;
	node->mpool = mpool;
	node->caller = caller;
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
	struct alloc *node = find_alloc((unsigned long)vaddr);

	if (!node)
		return;

	if (unmap)
		/*
		 * We need the double cast because otherwise gcc complains about
		 * cast to pointer of different size. This is technically a down
		 * cast but if unmap is being called, this had better be an
		 * actual 32-bit pointer anyway.
		 */
		iounmap((void *)(unsigned long)node->vaddr);

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

struct mem_pool *initialize_memory_pool(phys_addr_t start,
	unsigned long size, int mem_type)
{
	int id = mem_type;

	if (id >= MAX_MEMPOOLS || size <= PAGE_SIZE || size % PAGE_SIZE)
		return NULL;

	mutex_lock(&mpools[id].pool_mutex);

	mpools[id].paddr = start;
	mpools[id].size = size;
	mpools[id].free = size;
	mpools[id].id = id;
	mutex_unlock(&mpools[id].pool_mutex);

	pr_info("memory pool %d (start %pa size %lx) initialized\n",
		id, &start, size);
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
	return __alloc(mpool, aligned_size, align, cached,
		__builtin_return_address(0));

}
EXPORT_SYMBOL_GPL(allocate_contiguous_memory);

phys_addr_t _allocate_contiguous_memory_nomap(unsigned long size,
	int mem_type, unsigned long align, void *caller)
{
	phys_addr_t paddr;
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
	node->vaddr = paddr;
	node->len = aligned_size;
	node->mpool = mpool;
	node->caller = caller;
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
EXPORT_SYMBOL_GPL(_allocate_contiguous_memory_nomap);

phys_addr_t allocate_contiguous_memory_nomap(unsigned long size,
	int mem_type, unsigned long align)
{
	return _allocate_contiguous_memory_nomap(size, mem_type, align,
		__builtin_return_address(0));
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

void free_contiguous_memory_by_paddr(phys_addr_t paddr)
{
	if (!paddr)
		return;
	__free((void *)(unsigned long)paddr, false);
	return;
}
EXPORT_SYMBOL_GPL(free_contiguous_memory_by_paddr);

phys_addr_t memory_pool_node_paddr(void *vaddr)
{
	struct alloc *node = find_alloc((unsigned long)vaddr);

	if (!node)
		return -EINVAL;

	return node->paddr;
}
EXPORT_SYMBOL_GPL(memory_pool_node_paddr);

unsigned long memory_pool_node_len(void *vaddr)
{
	struct alloc *node = find_alloc((unsigned long)vaddr);

	if (!node)
		return -EINVAL;

	return node->len;
}
EXPORT_SYMBOL_GPL(memory_pool_node_len);

static const struct file_operations mempool_operations = {
	.owner		= THIS_MODULE,
	.open           = mempool_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = seq_release_private,
};

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

static int __init debugfs_mempool_init(void)
{
	struct dentry *entry, *dir = debugfs_create_dir("mempool", NULL);

	if (!dir) {
		pr_err("Cannot create /sys/kernel/debug/mempool");
		return -EINVAL;
	}

	entry = debugfs_create_file("map", S_IRUSR, dir,
		NULL, &mempool_operations);

	if (!entry)
		pr_err("Cannot create /sys/kernel/debug/mempool/map");

	return entry ? 0 : -EINVAL;
}

module_init(debugfs_mempool_init);
