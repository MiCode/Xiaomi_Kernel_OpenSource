/* Copyright (c) 2019-2020, Xiaomi Mobile Software Comp. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#include <linux/err.h>
#include <linux/iova.h>
#include <linux/debugfs.h>
#include "mi_iommu.h"

#define EXTEN_SIZE  (2)
#define MAX_DEBUG_INDEX (1000)

struct iova_gap {
	struct list_head list;
	struct rb_node *node;
};

static struct dentry *dir;
static struct kmem_cache *iova_gap_cache;

static struct iova_gap *alloc_iova_gap_mem(void)
{
	return kmem_cache_alloc(iova_gap_cache, GFP_ATOMIC);
}

static void free_iova_gap_mem(struct iova_gap *gap)
{
	kmem_cache_free(iova_gap_cache, gap);
}

struct iova_gap *radix_tree_delete_rbnode(struct radix_tree_root *rdroot, unsigned long index, struct rb_node *prev)
{
	struct iova_gap *gap;
	struct list_head *gap_list;
	int found = 0;

	if (!prev || (index == 0))
		return NULL;

	gap_list = radix_tree_lookup(rdroot, index);

	if (gap_list && !list_empty(gap_list)) {
		list_for_each_entry(gap, gap_list, list) {
			if (gap->node == prev) {
				list_del(&gap->list);
				found = 1;
				break;
			}
		}

		if (list_empty(gap_list)) {
			radix_tree_delete(rdroot, index);
			kfree(gap_list);
			gap_list = NULL;
		}
	}

	return (found ? gap : NULL);
}

static int __radix_tree_insert_rbnode(struct radix_tree_root *rdroot, unsigned long index, struct iova_gap *gap)
{
	struct list_head *gap_list = radix_tree_lookup(rdroot, index);

	if (!gap_list) {
		gap_list = kmalloc(sizeof(struct list_head), GFP_ATOMIC);
		if (gap_list == NULL)
		    return -ENOMEM;

		INIT_LIST_HEAD(gap_list);
		radix_tree_insert(rdroot, index, gap_list);
	}

	list_add_tail(&gap->list, gap_list);

	return 0;
}

static struct iova_gap *radix_tree_insert_rbnode(struct radix_tree_root *rdroot, unsigned long index, struct rb_node *prev)
{
	struct iova_gap *gap;

	if (index && prev) {
		gap = alloc_iova_gap_mem();
		if (gap == NULL)
			return NULL;
		gap->node = prev;

	    if (__radix_tree_insert_rbnode(rdroot, index, gap))
			free_iova_gap_mem(gap);
	}

	return NULL;
}

#ifdef CONFIG_ARM64_DMA_IOMMU_ALIGNMENT
#define MAX_ALIGN(shift) (((1 << CONFIG_ARM64_DMA_IOMMU_ALIGNMENT) * PAGE_SIZE)\
			  >> (shift))
#else
#define MAX_ALIGN(shift) ULONG_MAX
#endif

/*
 * Computes the padding size required, to make the start address
 * naturally aligned on the minimum of the power-of-two order of its size and
 * max_align
 */
static unsigned int
iova_get_pad_size(unsigned int size, unsigned int limit_pfn,
		  unsigned int max_align)
{
	unsigned int align = __roundup_pow_of_two(size);

	if (align > max_align)
		align = max_align;

	return (limit_pfn - size) & (align - 1);
}

/**
 * rdxtree_matched_gap - match gap by size
 * @iovad: - iova domain
 * @limit_pfn: - max limit address
 * @size: - size of page frames to allocate
 * @size_aligned: - set if size_aligned address range is required
 */
struct rb_node *rdxtree_matched_gap(struct iova_domain *iovad,
			unsigned long *limit_pfn, unsigned long size, bool size_aligned)
{
	struct radix_tree_root *rdroot;
	struct list_head **ppgap_list;
	struct list_head *gap_list;
	unsigned long limit_size;
	int exten_size  = EXTEN_SIZE;
	int i;

	if (!iovad)
		return NULL;

	if (!iovad->best_fit)
		return NULL;

	rdroot = &iovad->rdroot;

	ppgap_list = kzalloc(exten_size * sizeof(struct list_head), GFP_ATOMIC);
	if (!ppgap_list) {
		return NULL;
	}

	exten_size = radix_tree_gang_lookup(rdroot, (void **)ppgap_list, size, exten_size);

	for (i = 0; i < exten_size; i++) {
		gap_list = ppgap_list[i];
		if (gap_list && !list_empty(gap_list)) {
			struct iova_gap *gap;
			struct rb_node *node;
			struct iova *iova_prev, *iova_next;
			unsigned int pad_size = 0;
			list_for_each_entry(gap, gap_list, list) {
				iova_prev = rb_entry(gap->node, struct iova, node);
				node = rb_next(gap->node);

				if (node) {
					iova_next = rb_entry(node, struct iova, node);
					limit_size = iova_next->pfn_lo;
				} else {
					limit_size = *limit_pfn;
				}

				if (size_aligned)
					pad_size = iova_get_pad_size(size, limit_size, MAX_ALIGN(iova_shift(iovad)));

				/*
				 *  best fit
				 *  eg.    iova_prev      gap       iova_next
				 *         [0    19]                [40   69]
				 */
				if ((iova_prev->pfn_hi + size + pad_size) < limit_size) {
					*limit_pfn = limit_size;
					kfree(ppgap_list);
					return gap->node;
				}
			}
		}
	}

	kfree(ppgap_list);

	return NULL;
}

/**
 * rdxtree_update_gap
 * @iovad: - iova domain
 * @prev: - prev rbnode of new rbnode
 * @next: - next rbnode of new rbnode
 * @new: - allocated rbnode
 * After allocate iova, one gap maybe be split or consumed.
 * so we update the gaps.
 */
int rdxtree_update_gap(struct iova_domain *iovad,
			struct rb_node *prev, struct rb_node *next, struct rb_node *new)
{
	struct radix_tree_root *rdroot;
	struct iova *iova_next;
	struct iova *iova_prev;
	struct iova *iova_new;
	struct iova_gap *gap;
	unsigned long gap_index;

	if (!new || !iovad)
		return -EINVAL;

	if (!iovad->best_fit)
		return -EINVAL;

	rdroot = &iovad->rdroot;
	iova_next = (next == NULL) ? NULL : rb_entry(next, struct iova, node);
	iova_prev = (prev == NULL) ? NULL : rb_entry(prev, struct iova, node);
	iova_new = (new == NULL) ? NULL : rb_entry(new, struct iova, node);

	// the gap is consumed.
	if (iova_prev) {
		if (iova_next) {
			gap_index = iova_next->pfn_lo - iova_prev->pfn_hi - 1;
		} else {
			gap_index = iovad->dma_32bit_pfn - iova_prev->pfn_hi - 1;
		}

		gap = radix_tree_delete_rbnode(rdroot, gap_index, prev);
		if (gap)
			free_iova_gap_mem(gap);
	}

	// caculate prev gap
	if (iova_new && iova_prev) {
		gap_index = iova_new->pfn_lo - iova_prev->pfn_hi - 1;
		if (gap_index) {
			radix_tree_insert_rbnode(rdroot, gap_index, prev);
		}
	}

	// caculate next gap
	if (iova_new) {
		if (iova_next) {
			gap_index = iova_next->pfn_lo - iova_new->pfn_hi - 1;
		} else {
			gap_index = iovad->dma_32bit_pfn - iova_new->pfn_hi - 1;
		}

		if (gap_index) {
			radix_tree_insert_rbnode(rdroot, gap_index, new);
		}
	}

    return 0;
}

/**
 * rdxtree_insert_gap
 * @iovad: - iova domain
 * @free: - to free rbnode
 * When iova freed, we deal with the new gap
 */
int rdxtree_insert_gap(struct iova_domain *iovad, struct rb_node *free)
{
	struct radix_tree_root *rdroot;
	struct iova *iova_next;
	struct iova *iova_prev;
	struct iova *iova_free;
	struct iova_gap *gap;
	struct rb_node *node;
	unsigned long gap_index;
	unsigned long gap_index_new;

	if (!free || !iovad)
		return -EINVAL;

	if (!iovad->best_fit)
		return -EINVAL;

	rdroot = &iovad->rdroot;

	node = rb_next(free);
	iova_next = (node == NULL) ? NULL : rb_entry(node, struct iova, node);

	node = rb_prev(free);
	iova_prev = (node == NULL) ? NULL : rb_entry(node, struct iova, node);

	iova_free = rb_entry(free, struct iova, node);

	gap_index_new = iova_free->pfn_hi - iova_free->pfn_lo + 1;

	// merge prev gap
	if (iova_prev) {
		gap_index = iova_free->pfn_lo - iova_prev->pfn_hi - 1;
		gap = radix_tree_delete_rbnode(rdroot, gap_index, &(iova_prev->node));
		if (gap) {
			gap_index_new += gap_index;
			free_iova_gap_mem(gap);
		}
	}

	// merge next gap
	if (iova_next) {
		gap_index = iova_next->pfn_lo - iova_free->pfn_hi - 1;
	} else {
		gap_index = iovad->dma_32bit_pfn - iova_free->pfn_hi - 1;
	}

	gap = radix_tree_delete_rbnode(rdroot, gap_index, free);
	if (gap) {
		gap_index_new += gap_index;
		free_iova_gap_mem(gap);
	}

	// add new
	if (iova_prev) {
		radix_tree_insert_rbnode(rdroot, gap_index_new, &(iova_prev->node));
	}

	return 0;
}

// for debug
static int radix_tree_find_rbnode(struct radix_tree_root *rdroot, unsigned long index, struct rb_node *prev)
{
	struct iova_gap *gap;
	struct list_head *gap_list;
	int found = 0;

	if (!prev || (index == 0))
		return found;

	gap_list = radix_tree_lookup(rdroot, index);

	if (gap_list && !list_empty(gap_list)) {
		list_for_each_entry(gap, gap_list, list) {
			if (gap->node == prev) {
				found = 1;
				break;
			}
		}
	}

	return found;
}

static int radix_tree_show(struct iova_domain *iovad, unsigned long index, unsigned long size)
{
	struct list_head **ppgap_list;
	struct list_head *gap_list;
	struct iova_gap *gap;
	struct rb_node *node, *next_node;
	struct iova *iova_next, *iova_prev;
	struct radix_tree_root *rdroot;
	int i = 0;

	if (!iovad)
		return -EINVAL;

	rdroot = &iovad->rdroot;

	ppgap_list = kzalloc(size * sizeof(struct list_head), GFP_ATOMIC);
	if (!ppgap_list) {
		return -ENOMEM;
	}

	size = radix_tree_gang_lookup(rdroot, (void **)ppgap_list, index, size);

	for (i = 0; i < size; i++) {
		gap_list = ppgap_list[i];

		if (gap_list && !list_empty(gap_list)) {
			list_for_each_entry(gap, gap_list, list) {
				node = gap->node;
				next_node = rb_next(node);
				iova_prev = rb_entry(node, struct iova, node);
				if (next_node)
					iova_next = rb_entry(next_node, struct iova, node);

				pr_info("node %lx   prev %lx  next %lx\n", node, node, next_node);

				if (next_node) {
					pr_info("range prev: %lx @ %lx  next: %lx @ %lx  gap: %ld\n",
					iova_prev->pfn_lo, iova_prev->pfn_hi,
					iova_next->pfn_lo, iova_next->pfn_hi,
					iova_next->pfn_lo - iova_prev->pfn_hi - 1);
				} else {
					pr_info("range prev: %lx @ %lx  next: %lx @ %lx  gap: %ld\n",
					iova_prev->pfn_lo, iova_prev->pfn_hi,
					0, 0,
					iovad->dma_32bit_pfn - iova_prev->pfn_hi - 1);
				}

			}
		}
	}

	kfree(ppgap_list);

	return size;
}

static int mi_iommu_show(struct seq_file *m, void *v)
{
	struct iova_domain *iovad;
	unsigned long flags;
	struct rb_node *node;
	struct rb_node *next_node;
	struct iova *next_iova = NULL;
	int size = 0;

	iovad = m->private;

	if (!iovad)
		return -EINVAL;

	spin_lock_irqsave(&iovad->iova_rbtree_lock, flags);
	pr_info("mi_iommu dump iovad by rb tree\n");
	for (node = rb_first(&iovad->rbroot); node; node = rb_next(node)) {
		struct iova *iova = rb_entry(node, struct iova, node);
		next_node = rb_next(node);
		if (next_node) {
			next_iova = rb_entry(next_node, struct iova, node);
			size = next_iova->pfn_lo - iova->pfn_hi - 1;
		} else {
			size = iovad->dma_32bit_pfn - iova->pfn_hi - 1;
		}

		pr_info("iova prev %lx  next %lx\n", node, next_node);

		if (next_node) {
			pr_info("range prev: %lx @ %lx  next: %lx @ %lx  gap: %ld\n",
			iova->pfn_lo, iova->pfn_hi, next_iova->pfn_lo, next_iova->pfn_hi, size);
		} else {
			pr_info("range prev: %lx @ %lx  next: %lx @ %lx  gap: %ld\n",
			iova->pfn_lo, iova->pfn_hi, 0, 0, size);
		}

		pr_info("range prev: %lx mi_iommu exist %d\n", node, radix_tree_find_rbnode(&iovad->rdroot, size, node));
	}

	pr_info("mi_iommu dump iovad by rd tree\n");
	radix_tree_show(iovad, 0, MAX_DEBUG_INDEX);

	spin_unlock_irqrestore(&iovad->iova_rbtree_lock, flags);

	return 0;
}

static int mi_iommu_open(struct inode *inode, struct file *file)
{
	return single_open(file, mi_iommu_show, inode->i_private);
}

static int __init mi_iommu_init(void)
{
	if (!debugfs_initialized()) {
		pr_warn("debugfs not available, stat dir not created\n");
		return -ENOENT;
	}

	dir = debugfs_create_dir("mi_iommu", NULL);
	if (!dir) {
		pr_err("debugfs 'mi_iommu' stat dir creation failed\n");
		return -ENOMEM ;
	}

	return 0;
}

static const struct file_operations mi_iommu_fops = {
	.owner		= THIS_MODULE,
	.open		= mi_iommu_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

void mi_iommu_debug_init(struct iova_domain *iovad, const char *name)
{
	if (dir && name && strlen(name) != 0) {
		debugfs_create_file(name,
				S_IFREG | S_IRUGO | S_IWUSR,
				dir,
				(void *)iovad,
				&mi_iommu_fops);
	}
}

static int __init iova_gap_init(void)
{
	iova_gap_cache = kmem_cache_create(
		"mi_iommu_iova_gap", sizeof(struct iova_gap), 0,
		SLAB_HWCACHE_ALIGN, NULL);
	if (!iova_gap_cache) {
		printk(KERN_ERR "Couldn't create iova cache\n");
		return -ENOMEM;
	}

	return 0;
}

static void __exit iova_gap_exit(void)
{
    kmem_cache_destroy(iova_gap_cache);
}

fs_initcall(mi_iommu_init);
module_init(iova_gap_init);
module_exit(iova_gap_exit);

MODULE_AUTHOR("Xiaomi Camera Optimization Team <camera-opt@xiaomi.com>");
MODULE_LICENSE("GPL");
