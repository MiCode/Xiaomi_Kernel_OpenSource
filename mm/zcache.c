/*
 * linux/mm/zcache.c
 *
 * A cleancache backend for file pages compression.
 * Concepts based on original zcache by Dan Magenheimer.
 * Copyright (C) 2013  Bob Liu <bob.liu@xxxxxxxxxx>
 *
 * With zcache, active file pages can be compressed in memory during page
 * reclaiming. When their data is needed again the I/O reading operation is
 * avoided. This results in a significant performance gain under memory pressure
 * for systems with many file pages.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/atomic.h>
#include <linux/cleancache.h>
#include <linux/cpu.h>
#include <linux/crypto.h>
#include <linux/page-flags.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/radix-tree.h>
#include <linux/rbtree.h>
#include <linux/types.h>
#include <linux/zbud.h>

/*
 * Enable/disable zcache (disabled by default)
 */
static bool zcache_enabled __read_mostly;
module_param_named(enabled, zcache_enabled, bool, 0);

/*
 * Compressor to be used by zcache
 */
#define ZCACHE_COMPRESSOR_DEFAULT "lzo"
static char *zcache_compressor = ZCACHE_COMPRESSOR_DEFAULT;
module_param_named(compressor, zcache_compressor, charp, 0);

/*
 * The maximum percentage of memory that the compressed pool can occupy.
 */
static unsigned int zcache_max_pool_percent = 10;
module_param_named(max_pool_percent, zcache_max_pool_percent, uint, 0644);

/*
 * zcache statistics
 */
static u64 zcache_pool_limit_hit;
static u64 zcache_dup_entry;
static u64 zcache_zbud_alloc_fail;
static u64 zcache_pool_pages;
static u64 zcache_evict_zpages;
static u64 zcache_evict_filepages;
static u64 zcache_inactive_pages_refused;
static u64 zcache_reclaim_fail;
static atomic_t zcache_stored_pages = ATOMIC_INIT(0);

/*
 * Zcache receives pages for compression through the Cleancache API and is able
 * to evict pages from its own compressed pool on an LRU basis in the case that
 * the compressed pool is full.
 *
 * Zcache makes use of zbud for the managing the compressed memory pool. Each
 * allocation in zbud is not directly accessible by address.  Rather, a handle
 * (zaddr) is return by the allocation routine and that handle(zaddr must be
 * mapped before being accessed. The compressed memory pool grows on demand and
 * shrinks as compressed pages are freed.
 *
 * When a file page is passed from cleancache to zcache, zcache maintains a
 * mapping of the <filesystem_type, inode_number, page_index> to the zbud
 * address that references that compressed file page. This mapping is achieved
 * with a red-black tree per filesystem type, plus a radix tree per red-black
 * node.
 *
 * A zcache pool with pool_id as the index is created when a filesystem mounted
 * Each zcache pool has a red-black tree, the inode number(rb_index) is the
 * search key. Each red-black tree node has a radix tree which use
 * page->index(ra_index) as the index. Each radix tree slot points to the zbud
 * address combining with some extra information(zcache_ra_handle).
 */
#define MAX_ZCACHE_POOLS 32
/*
 * One zcache_pool per (cleancache aware) filesystem mount instance
 */
struct zcache_pool {
	struct rb_root rbtree;
	rwlock_t rb_lock;		/* Protects rbtree */
	struct zbud_pool *pool;         /* Zbud pool used */
};

/*
 * Manage all zcache pools
 */
struct _zcache {
	struct zcache_pool *pools[MAX_ZCACHE_POOLS];
	u32 num_pools;			/* Current no. of zcache pools */
	spinlock_t pool_lock;		/* Protects pools[] and num_pools */
};
struct _zcache zcache;

/*
 * Redblack tree node, each node has a page index radix-tree.
 * Indexed by inode nubmer.
 */
struct zcache_rbnode {
	struct rb_node rb_node;
	int rb_index;
	struct radix_tree_root ratree; /* Page radix tree per inode rbtree */
	spinlock_t ra_lock;		/* Protects radix tree */
	struct kref refcount;
};

/*
 * Radix-tree leaf, indexed by page->index
 */
struct zcache_ra_handle {
	int rb_index;			/* Redblack tree index */
	int ra_index;			/* Radix tree index */
	int zlen;			/* Compressed page size */
	struct zcache_pool *zpool;	/* Finding zcache_pool during evict */
};

static struct kmem_cache *zcache_rbnode_cache;
static int zcache_rbnode_cache_create(void)
{
	zcache_rbnode_cache = KMEM_CACHE(zcache_rbnode, 0);
	return zcache_rbnode_cache == NULL;
}
static void zcache_rbnode_cache_destroy(void)
{
	kmem_cache_destroy(zcache_rbnode_cache);
}

/*
 * Compression functions
 * (Below functions are copyed from zswap!)
 */
static struct crypto_comp * __percpu *zcache_comp_pcpu_tfms;

enum comp_op {
	ZCACHE_COMPOP_COMPRESS,
	ZCACHE_COMPOP_DECOMPRESS
};

static int zcache_comp_op(enum comp_op op, const u8 *src, unsigned int slen,
				u8 *dst, unsigned int *dlen)
{
	struct crypto_comp *tfm;
	int ret;

	tfm = *per_cpu_ptr(zcache_comp_pcpu_tfms, get_cpu());
	switch (op) {
	case ZCACHE_COMPOP_COMPRESS:
		ret = crypto_comp_compress(tfm, src, slen, dst, dlen);
		break;
	case ZCACHE_COMPOP_DECOMPRESS:
		ret = crypto_comp_decompress(tfm, src, slen, dst, dlen);
		break;
	default:
		ret = -EINVAL;
	}

	put_cpu();
	return ret;
}

static int __init zcache_comp_init(void)
{
	if (!crypto_has_comp(zcache_compressor, 0, 0)) {
		pr_info("%s compressor not available\n", zcache_compressor);
		/* fall back to default compressor */
		zcache_compressor = ZCACHE_COMPRESSOR_DEFAULT;
		if (!crypto_has_comp(zcache_compressor, 0, 0))
			/* can't even load the default compressor */
			return -ENODEV;
	}
	pr_info("using %s compressor\n", zcache_compressor);

	/* alloc percpu transforms */
	zcache_comp_pcpu_tfms = alloc_percpu(struct crypto_comp *);
	if (!zcache_comp_pcpu_tfms)
		return -ENOMEM;
	return 0;
}

static void zcache_comp_exit(void)
{
	/* free percpu transforms */
	if (zcache_comp_pcpu_tfms)
		free_percpu(zcache_comp_pcpu_tfms);
}

/*
 * Per-cpu code
 * (Below functions are also copyed from zswap!)
 */
static DEFINE_PER_CPU(u8 *, zcache_dstmem);

static int __zcache_cpu_notifier(unsigned long action, unsigned long cpu)
{
	struct crypto_comp *tfm;
	u8 *dst;

	switch (action) {
	case CPU_UP_PREPARE:
		tfm = crypto_alloc_comp(zcache_compressor, 0, 0);
		if (IS_ERR(tfm)) {
			pr_err("can't allocate compressor transform\n");
			return NOTIFY_BAD;
		}
		*per_cpu_ptr(zcache_comp_pcpu_tfms, cpu) = tfm;
		dst = kmalloc(PAGE_SIZE * 2, GFP_KERNEL);
		if (!dst) {
			pr_err("can't allocate compressor buffer\n");
			crypto_free_comp(tfm);
			*per_cpu_ptr(zcache_comp_pcpu_tfms, cpu) = NULL;
			return NOTIFY_BAD;
		}
		per_cpu(zcache_dstmem, cpu) = dst;
		break;
	case CPU_DEAD:
	case CPU_UP_CANCELED:
		tfm = *per_cpu_ptr(zcache_comp_pcpu_tfms, cpu);
		if (tfm) {
			crypto_free_comp(tfm);
			*per_cpu_ptr(zcache_comp_pcpu_tfms, cpu) = NULL;
		}
		dst = per_cpu(zcache_dstmem, cpu);
		kfree(dst);
		per_cpu(zcache_dstmem, cpu) = NULL;
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static int zcache_cpu_notifier(struct notifier_block *nb,
				unsigned long action, void *pcpu)
{
	unsigned long cpu = (unsigned long)pcpu;

	return __zcache_cpu_notifier(action, cpu);
}

static struct notifier_block zcache_cpu_notifier_block = {
	.notifier_call = zcache_cpu_notifier
};

static int zcache_cpu_init(void)
{
	unsigned long cpu;

	get_online_cpus();
	for_each_online_cpu(cpu)
		if (__zcache_cpu_notifier(CPU_UP_PREPARE, cpu) != NOTIFY_OK)
			goto cleanup;
	register_cpu_notifier(&zcache_cpu_notifier_block);
	put_online_cpus();
	return 0;

cleanup:
	for_each_online_cpu(cpu)
		__zcache_cpu_notifier(CPU_UP_CANCELED, cpu);
	put_online_cpus();
	return -ENOMEM;
}

/*
 * Zcache helpers
 */
static bool zcache_is_full(void)
{
	return totalram_pages * zcache_max_pool_percent / 100 <
			zcache_pool_pages;
}

/*
 * The caller must hold zpool->rb_lock at least
 */
static struct zcache_rbnode *zcache_find_rbnode(struct rb_root *rbtree,
	int index, struct rb_node **rb_parent, struct rb_node ***rb_link)
{
	struct zcache_rbnode *entry;
	struct rb_node **__rb_link, *__rb_parent, *rb_prev;

	__rb_link = &rbtree->rb_node;
	rb_prev = __rb_parent = NULL;

	while (*__rb_link) {
		__rb_parent = *__rb_link;
		entry = rb_entry(__rb_parent, struct zcache_rbnode, rb_node);
		if (entry->rb_index > index)
			__rb_link = &__rb_parent->rb_left;
		else if (entry->rb_index < index) {
			rb_prev = __rb_parent;
			__rb_link = &__rb_parent->rb_right;
		} else
			return entry;
	}

	if (rb_parent)
		*rb_parent = __rb_parent;
	if (rb_link)
		*rb_link = __rb_link;
	return NULL;
}

static struct zcache_rbnode *zcache_find_get_rbnode(struct zcache_pool *zpool,
					int rb_index)
{
	unsigned long flags;
	struct zcache_rbnode *rbnode;

	read_lock_irqsave(&zpool->rb_lock, flags);
	rbnode = zcache_find_rbnode(&zpool->rbtree, rb_index, 0, 0);
	if (rbnode)
		kref_get(&rbnode->refcount);
	read_unlock_irqrestore(&zpool->rb_lock, flags);
	return rbnode;
}

/*
 * kref_put callback for zcache_rbnode.
 *
 * The rbnode must have been isolated from rbtree already.
 */
static void zcache_rbnode_release(struct kref *kref)
{
	struct zcache_rbnode *rbnode;

	rbnode = container_of(kref, struct zcache_rbnode, refcount);
	BUG_ON(rbnode->ratree.rnode);
	kmem_cache_free(zcache_rbnode_cache, rbnode);
}

/*
 * Check whether the radix-tree of this rbnode is empty.
 * If that's true, then we can delete this zcache_rbnode from
 * zcache_pool->rbtree
 *
 * Caller must hold zcache_rbnode->ra_lock
 */
static int zcache_rbnode_empty(struct zcache_rbnode *rbnode)
{
	return rbnode->ratree.rnode == NULL;
}

/*
 * Remove zcache_rbnode from zpool->rbtree
 *
 * holded_rblock - whether the caller has holded zpool->rb_lock
 */
static void zcache_rbnode_isolate(struct zcache_pool *zpool,
		struct zcache_rbnode *rbnode, bool holded_rblock)
{
	unsigned long flags;

	if (!holded_rblock)
		write_lock_irqsave(&zpool->rb_lock, flags);
	/*
	 * Someone can get reference on this rbnode before we could
	 * acquire write lock above.
	 * We want to remove it from zpool->rbtree when only the caller and
	 * corresponding ratree holds a reference to this rbnode.
	 * Below check ensures that a racing zcache put will not end up adding
	 * a page to an isolated node and thereby losing that memory.
	 */
	if (atomic_read(&rbnode->refcount.refcount) == 2) {
		rb_erase(&rbnode->rb_node, &zpool->rbtree);
		RB_CLEAR_NODE(&rbnode->rb_node);
		kref_put(&rbnode->refcount, zcache_rbnode_release);
	}
	if (!holded_rblock)
		write_unlock_irqrestore(&zpool->rb_lock, flags);
}

/*
 * Store zaddr which allocated by zbud_alloc() to the hierarchy rbtree-ratree.
 */
static int zcache_store_zaddr(struct zcache_pool *zpool,
		struct zcache_ra_handle *zhandle, unsigned long zaddr)
{
	unsigned long flags;
	struct zcache_rbnode *rbnode, *tmp;
	struct rb_node **link = NULL, *parent = NULL;
	int ret;
	void *dup_zaddr;

	rbnode = zcache_find_get_rbnode(zpool, zhandle->rb_index);
	if (!rbnode) {
		/* alloc and init a new rbnode */
		rbnode = kmem_cache_alloc(zcache_rbnode_cache, GFP_KERNEL);
		if (!rbnode)
			return -ENOMEM;

		INIT_RADIX_TREE(&rbnode->ratree, GFP_ATOMIC|__GFP_NOWARN);
		spin_lock_init(&rbnode->ra_lock);
		rbnode->rb_index = zhandle->rb_index;
		kref_init(&rbnode->refcount);
		RB_CLEAR_NODE(&rbnode->rb_node);

		/* add that rbnode to rbtree */
		write_lock_irqsave(&zpool->rb_lock, flags);
		tmp = zcache_find_rbnode(&zpool->rbtree, zhandle->rb_index,
				&parent, &link);
		if (tmp) {
			/* somebody else allocated new rbnode */
			kmem_cache_free(zcache_rbnode_cache, rbnode);
			rbnode = tmp;
		} else {
			rb_link_node(&rbnode->rb_node, parent, link);
			rb_insert_color(&rbnode->rb_node, &zpool->rbtree);
		}

		/* Inc the reference of this zcache_rbnode */
		kref_get(&rbnode->refcount);
		write_unlock_irqrestore(&zpool->rb_lock, flags);
	}

	/* Succfully got a zcache_rbnode when arriving here */
	spin_lock_irqsave(&rbnode->ra_lock, flags);
	dup_zaddr = radix_tree_delete(&rbnode->ratree, zhandle->ra_index);
	if (unlikely(dup_zaddr)) {
		WARN_ON("duplicated, will be replaced!\n");
		zbud_free(zpool->pool, (unsigned long)dup_zaddr);
		atomic_dec(&zcache_stored_pages);
		zcache_pool_pages = zbud_get_pool_size(zpool->pool);
		zcache_dup_entry++;
	}

	/* Insert zcache_ra_handle to ratree */
	ret = radix_tree_insert(&rbnode->ratree, zhandle->ra_index,
				(void *)zaddr);
	if (unlikely(ret))
		if (zcache_rbnode_empty(rbnode))
			zcache_rbnode_isolate(zpool, rbnode, 0);
	spin_unlock_irqrestore(&rbnode->ra_lock, flags);

	kref_put(&rbnode->refcount, zcache_rbnode_release);
	return ret;
}

/*
 * Load zaddr and delete it from radix tree.
 * If the radix tree of the corresponding rbnode is empty, delete the rbnode
 * from zpool->rbtree also.
 */
static void *zcache_load_delete_zaddr(struct zcache_pool *zpool,
				int rb_index, int ra_index)
{
	struct zcache_rbnode *rbnode;
	void *zaddr = NULL;
	unsigned long flags;

	rbnode = zcache_find_get_rbnode(zpool, rb_index);
	if (!rbnode)
		goto out;

	BUG_ON(rbnode->rb_index != rb_index);

	spin_lock_irqsave(&rbnode->ra_lock, flags);
	zaddr = radix_tree_delete(&rbnode->ratree, ra_index);
	if (zcache_rbnode_empty(rbnode))
		zcache_rbnode_isolate(zpool, rbnode, 0);
	spin_unlock_irqrestore(&rbnode->ra_lock, flags);

	kref_put(&rbnode->refcount, zcache_rbnode_release);
out:
	return zaddr;
}

static void zcache_store_page(int pool_id, struct cleancache_filekey key,
		pgoff_t index, struct page *page)
{
	struct zcache_ra_handle *zhandle;
	u8 *zpage, *src, *dst;
	unsigned long zaddr; /* Address of zhandle + compressed data(zpage) */
	unsigned int zlen = PAGE_SIZE;
	int ret;

	struct zcache_pool *zpool = zcache.pools[pool_id];

	/*
	 * Zcache will be ineffective if the compressed memory pool is full with
	 * compressed inactive file pages and most of them will never be used
	 * again.
	 * So we refuse to compress pages that are not from active file list.
	 */
	if (!PageWasActive(page)) {
		zcache_inactive_pages_refused++;
		return;
	}

	if (zcache_is_full()) {
		zcache_pool_limit_hit++;
		if (zbud_reclaim_page(zpool->pool, 8)) {
			zcache_reclaim_fail++;
			return;
		}
		/*
		 * Continue if reclaimed a page frame succ.
		 */
		zcache_evict_filepages++;
		zcache_pool_pages = zbud_get_pool_size(zpool->pool);
	}

	/* compress */
	dst = get_cpu_var(zcache_dstmem);
	src = kmap_atomic(page);
	ret = zcache_comp_op(ZCACHE_COMPOP_COMPRESS, src, PAGE_SIZE, dst,
			&zlen);
	kunmap_atomic(src);
	if (ret) {
		pr_err("zcache compress error ret %d\n", ret);
		put_cpu_var(zcache_dstmem);
		return;
	}

	/* store zcache handle together with compressed page data */
	ret = zbud_alloc(zpool->pool, zlen + sizeof(struct zcache_ra_handle),
			__GFP_NORETRY | __GFP_NOWARN, &zaddr);
	if (ret) {
		zcache_zbud_alloc_fail++;
		put_cpu_var(zcache_dstmem);
		return;
	}

	zhandle = (struct zcache_ra_handle *)zbud_map(zpool->pool, zaddr);
	zhandle->ra_index = index;
	zhandle->rb_index = key.u.ino;
	zhandle->zlen = zlen;
	zhandle->zpool = zpool;

	/* Compressed page data stored at the end of zcache_ra_handle */
	zpage = (u8 *)(zhandle + 1);
	memcpy(zpage, dst, zlen);
	zbud_unmap(zpool->pool, zaddr);
	put_cpu_var(zcache_dstmem);

	/* store zcache handle */
	ret = zcache_store_zaddr(zpool, zhandle, zaddr);
	if (ret) {
		pr_err("%s: store handle error %d\n", __func__, ret);
		zbud_free(zpool->pool, zaddr);
	}

	/* update stats */
	atomic_inc(&zcache_stored_pages);
	zcache_pool_pages = zbud_get_pool_size(zpool->pool);
}

static int zcache_load_page(int pool_id, struct cleancache_filekey key,
			pgoff_t index, struct page *page)
{
	int ret;
	u8 *src, *dst;
	void *zaddr;
	unsigned int dlen = PAGE_SIZE;
	struct zcache_ra_handle *zhandle;
	struct zcache_pool *zpool = zcache.pools[pool_id];

	zaddr = zcache_load_delete_zaddr(zpool, key.u.ino, index);
	if (!zaddr)
		return -ENOENT;

	zhandle = (struct zcache_ra_handle *)zbud_map(zpool->pool,
			(unsigned long)zaddr);
	/* Compressed page data stored at the end of zcache_ra_handle */
	src = (u8 *)(zhandle + 1);

	/* decompress */
	dst = kmap_atomic(page);
	ret = zcache_comp_op(ZCACHE_COMPOP_DECOMPRESS, src, zhandle->zlen, dst,
			&dlen);
	kunmap_atomic(dst);
	zbud_unmap(zpool->pool, (unsigned long)zaddr);
	zbud_free(zpool->pool, (unsigned long)zaddr);

	BUG_ON(ret);
	BUG_ON(dlen != PAGE_SIZE);

	/* update stats */
	atomic_dec(&zcache_stored_pages);
	zcache_pool_pages = zbud_get_pool_size(zpool->pool);
	SetPageWasActive(page);
	return ret;
}

static void zcache_flush_page(int pool_id, struct cleancache_filekey key,
			pgoff_t index)
{
	struct zcache_pool *zpool = zcache.pools[pool_id];
	void *zaddr = NULL;

	zaddr = zcache_load_delete_zaddr(zpool, key.u.ino, index);
	if (zaddr) {
		zbud_free(zpool->pool, (unsigned long)zaddr);
		atomic_dec(&zcache_stored_pages);
		zcache_pool_pages = zbud_get_pool_size(zpool->pool);
	}
}

#define FREE_BATCH 16
/*
 * Callers must hold the lock
 */
static void zcache_flush_ratree(struct zcache_pool *zpool,
		struct zcache_rbnode *rbnode)
{
	unsigned long index = 0;
	int count, i;
	struct zcache_ra_handle *zhandle;

	do {
		void *zaddrs[FREE_BATCH];

		count = radix_tree_gang_lookup(&rbnode->ratree, (void **)zaddrs,
				index, FREE_BATCH);

		for (i = 0; i < count; i++) {
			zhandle = (struct zcache_ra_handle *)zbud_map(
					zpool->pool, (unsigned long)zaddrs[i]);
			index = zhandle->ra_index;
			radix_tree_delete(&rbnode->ratree, index);
			zbud_unmap(zpool->pool, (unsigned long)zaddrs[i]);
			zbud_free(zpool->pool, (unsigned long)zaddrs[i]);
			atomic_dec(&zcache_stored_pages);
			zcache_pool_pages = zbud_get_pool_size(zpool->pool);
		}

		index++;
	} while (count == FREE_BATCH);
}

static void zcache_flush_inode(int pool_id, struct cleancache_filekey key)
{
	struct zcache_rbnode *rbnode;
	unsigned long flags1, flags2;
	struct zcache_pool *zpool = zcache.pools[pool_id];

	/*
	 * Refuse new pages added in to the same rbinode, so get rb_lock at
	 * first.
	 */
	write_lock_irqsave(&zpool->rb_lock, flags1);
	rbnode = zcache_find_rbnode(&zpool->rbtree, key.u.ino, 0, 0);
	if (!rbnode) {
		write_unlock_irqrestore(&zpool->rb_lock, flags1);
		return;
	}

	kref_get(&rbnode->refcount);
	spin_lock_irqsave(&rbnode->ra_lock, flags2);

	zcache_flush_ratree(zpool, rbnode);
	if (zcache_rbnode_empty(rbnode))
		/* When arrvied here, we already hold rb_lock */
		zcache_rbnode_isolate(zpool, rbnode, 1);

	spin_unlock_irqrestore(&rbnode->ra_lock, flags2);
	write_unlock_irqrestore(&zpool->rb_lock, flags1);
	kref_put(&rbnode->refcount, zcache_rbnode_release);
}

static void zcache_destroy_pool(struct zcache_pool *zpool);
static void zcache_flush_fs(int pool_id)
{
	struct zcache_rbnode *z_rbnode = NULL;
	struct rb_node *rbnode;
	unsigned long flags1, flags2;
	struct zcache_pool *zpool;

	if (pool_id < 0)
		return;

	zpool = zcache.pools[pool_id];
	if (!zpool)
		return;

	/*
	 * Refuse new pages added in, so get rb_lock at first.
	 */
	write_lock_irqsave(&zpool->rb_lock, flags1);

	rbnode = rb_first(&zpool->rbtree);
	while (rbnode) {
		z_rbnode = rb_entry(rbnode, struct zcache_rbnode, rb_node);
		rbnode = rb_next(rbnode);
		if (z_rbnode) {
			kref_get(&z_rbnode->refcount);
			spin_lock_irqsave(&z_rbnode->ra_lock, flags2);
			zcache_flush_ratree(zpool, z_rbnode);
			if (zcache_rbnode_empty(z_rbnode))
				zcache_rbnode_isolate(zpool, z_rbnode, 1);
			spin_unlock_irqrestore(&z_rbnode->ra_lock, flags2);
			kref_put(&z_rbnode->refcount, zcache_rbnode_release);
		}
	}

	write_unlock_irqrestore(&zpool->rb_lock, flags1);
	zcache_destroy_pool(zpool);
}

/*
 * Evict compressed pages from zcache pool on an LRU basis after the compressed
 * pool is full.
 */
static int zcache_evict_zpage(struct zbud_pool *pool, unsigned long zaddr)
{
	struct zcache_pool *zpool;
	struct zcache_ra_handle *zhandle;
	void *zaddr_intree;

	zhandle = (struct zcache_ra_handle *)zbud_map(pool, zaddr);

	zpool = zhandle->zpool;
	BUG_ON(!zpool);
	BUG_ON(pool != zpool->pool);

	zaddr_intree = zcache_load_delete_zaddr(zpool, zhandle->rb_index,
			zhandle->ra_index);
	if (zaddr_intree) {
		BUG_ON((unsigned long)zaddr_intree != zaddr);
		zbud_unmap(pool, zaddr);
		zbud_free(pool, zaddr);
		atomic_dec(&zcache_stored_pages);
		zcache_pool_pages = zbud_get_pool_size(pool);
		zcache_evict_zpages++;
	}
	return 0;
}

static struct zbud_ops zcache_zbud_ops = {
	.evict = zcache_evict_zpage
};

/* Return pool id */
static int zcache_create_pool(void)
{
	int ret;
	struct zcache_pool *zpool;

	zpool = kzalloc(sizeof(*zpool), GFP_KERNEL);
	if (!zpool) {
		ret = -ENOMEM;
		goto out;
	}

	zpool->pool = zbud_create_pool(GFP_KERNEL, &zcache_zbud_ops);
	if (!zpool->pool) {
		kfree(zpool);
		ret = -ENOMEM;
		goto out;
	}

	spin_lock(&zcache.pool_lock);
	if (zcache.num_pools == MAX_ZCACHE_POOLS) {
		pr_err("Cannot create new pool (limit:%u)\n", MAX_ZCACHE_POOLS);
		zbud_destroy_pool(zpool->pool);
		kfree(zpool);
		ret = -EPERM;
		goto out_unlock;
	}

	rwlock_init(&zpool->rb_lock);
	zpool->rbtree = RB_ROOT;
	/* Add to pool list */
	for (ret = 0; ret < MAX_ZCACHE_POOLS; ret++)
		if (!zcache.pools[ret])
			break;
	zcache.pools[ret] = zpool;
	zcache.num_pools++;
	pr_info("New pool created id:%d\n", ret);

out_unlock:
	spin_unlock(&zcache.pool_lock);
out:
	return ret;
}

static void zcache_destroy_pool(struct zcache_pool *zpool)
{
	int i;

	if (!zpool)
		return;

	spin_lock(&zcache.pool_lock);
	zcache.num_pools--;
	for (i = 0; i < MAX_ZCACHE_POOLS; i++)
		if (zcache.pools[i] == zpool)
			break;
	zcache.pools[i] = NULL;
	spin_unlock(&zcache.pool_lock);

	if (!RB_EMPTY_ROOT(&zpool->rbtree))
		WARN_ON("Memory leak detected. Freeing non-empty pool!\n");

	zbud_destroy_pool(zpool->pool);
	kfree(zpool);
}

static int zcache_init_fs(size_t pagesize)
{
	int ret;

	if (pagesize != PAGE_SIZE) {
		pr_info("Unsupported page size: %zu", pagesize);
		ret = -EINVAL;
		goto out;
	}

	ret = zcache_create_pool();
	if (ret < 0) {
		pr_info("Failed to create new pool\n");
		ret = -ENOMEM;
		goto out;
	}
out:
	return ret;
}

static int zcache_init_shared_fs(char *uuid, size_t pagesize)
{
	/* shared pools are unsupported and map to private */
	return zcache_init_fs(pagesize);
}

static struct cleancache_ops zcache_ops = {
	.put_page = zcache_store_page,
	.get_page = zcache_load_page,
	.invalidate_page = zcache_flush_page,
	.invalidate_inode = zcache_flush_inode,
	.invalidate_fs = zcache_flush_fs,
	.init_shared_fs = zcache_init_shared_fs,
	.init_fs = zcache_init_fs
};

/*
 * Debugfs functions
 */
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
static struct dentry *zcache_debugfs_root;

static int __init zcache_debugfs_init(void)
{
	if (!debugfs_initialized())
		return -ENODEV;

	zcache_debugfs_root = debugfs_create_dir("zcache", NULL);
	if (!zcache_debugfs_root)
		return -ENOMEM;

	debugfs_create_u64("pool_limit_hit", S_IRUGO, zcache_debugfs_root,
			&zcache_pool_limit_hit);
	debugfs_create_u64("reject_alloc_fail", S_IRUGO, zcache_debugfs_root,
			&zcache_zbud_alloc_fail);
	debugfs_create_u64("duplicate_entry", S_IRUGO, zcache_debugfs_root,
			&zcache_dup_entry);
	debugfs_create_u64("pool_pages", S_IRUGO, zcache_debugfs_root,
			&zcache_pool_pages);
	debugfs_create_atomic_t("stored_pages", S_IRUGO, zcache_debugfs_root,
			&zcache_stored_pages);
	debugfs_create_u64("evicted_zpages", S_IRUGO, zcache_debugfs_root,
			&zcache_evict_zpages);
	debugfs_create_u64("evicted_filepages", S_IRUGO, zcache_debugfs_root,
			&zcache_evict_filepages);
	debugfs_create_u64("reclaim_fail", S_IRUGO, zcache_debugfs_root,
			&zcache_reclaim_fail);
	debugfs_create_u64("inactive_pages_refused", S_IRUGO,
			zcache_debugfs_root, &zcache_inactive_pages_refused);
	return 0;
}

static void __exit zcache_debugfs_exit(void)
{
	debugfs_remove_recursive(zcache_debugfs_root);
}
#else
static int __init zcache_debugfs_init(void)
{
	return 0;
}
static void __exit zcache_debugfs_exit(void)
{
}
#endif

/*
 * zcache init and exit
 */
static int __init init_zcache(void)
{
	if (!zcache_enabled)
		return 0;

	pr_info("loading zcache..\n");
	if (zcache_rbnode_cache_create()) {
		pr_err("entry cache creation failed\n");
		goto error;
	}

	if (zcache_comp_init()) {
		pr_err("compressor initialization failed\n");
		goto compfail;
	}
	if (zcache_cpu_init()) {
		pr_err("per-cpu initialization failed\n");
		goto pcpufail;
	}

	spin_lock_init(&zcache.pool_lock);
	cleancache_register_ops(&zcache_ops);

	if (zcache_debugfs_init())
		pr_warn("debugfs initialization failed\n");
	return 0;
pcpufail:
	zcache_comp_exit();
compfail:
	zcache_rbnode_cache_destroy();
error:
	return -ENOMEM;
}

/* must be late so crypto has time to come up */
late_initcall(init_zcache);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bob Liu <bob.liu@xxxxxxxxxx>");
MODULE_DESCRIPTION("Compressed cache for clean file pages");

