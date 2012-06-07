/*
 * Copyright (c) 2010,2011, Dan Magenheimer, Oracle Corp.
 * Copyright (c) 2010,2011, Nitin Gupta
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * Qcache provides an in-kernel "host implementation" for transcendent memory
 * and, thus indirectly, for cleancache and frontswap.  Qcache includes a
 * page-accessible memory [1] interface, utilizing lzo1x compression:
 * 1) "compression buddies" ("zbud") is used for ephemeral pages
 * Zbud allows pairs (and potentially,
 * in the future, more than a pair of) compressed pages to be closely linked
 * so that reclaiming can be done via the kernel's physical-page-oriented
 * "shrinker" interface.
 *
 * [1] For a definition of page-accessible memory (aka PAM), see:
 *   http://marc.info/?l=linux-mm&m=127811271605009
 */

#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/highmem.h>
#include <linux/list.h>
#include <linux/lzo.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/math64.h>
#include <linux/bitmap.h>
#include <linux/fmem.h>
#include "tmem.h"

#if !defined(CONFIG_CLEANCACHE)
#error "qcache is useless without CONFIG_CLEANCACHE"
#endif
#include <linux/cleancache.h>

#define ZCACHE_GFP_MASK \
	(__GFP_FS | __GFP_NORETRY | __GFP_NOWARN | __GFP_NOMEMALLOC)

#define MAX_POOLS_PER_CLIENT 16

#define MAX_CLIENTS 16
#define LOCAL_CLIENT ((uint16_t)-1)

MODULE_LICENSE("GPL");

struct zcache_client {
	struct tmem_pool *tmem_pools[MAX_POOLS_PER_CLIENT];
	struct xv_pool *xvpool;
	bool allocated;
	atomic_t refcount;
};

struct qcache_info {
	void *addr;
	unsigned long *bitmap;
	spinlock_t lock;
	unsigned pages;
};
static struct qcache_info qcache_info;
static unsigned long zcache_qc_allocated;
static unsigned long zcache_qc_freed;
static unsigned long zcache_qc_used;
static unsigned long zcache_qc_max_used;

static struct zcache_client zcache_host;
static struct zcache_client zcache_clients[MAX_CLIENTS];

static inline uint16_t get_client_id_from_client(struct zcache_client *cli)
{
	BUG_ON(cli == NULL);
	if (cli == &zcache_host)
		return LOCAL_CLIENT;
	return cli - &zcache_clients[0];
}

static inline bool is_local_client(struct zcache_client *cli)
{
	return cli == &zcache_host;
}

/**********
 * Compression buddies ("zbud") provides for packing two (or, possibly
 * in the future, more) compressed ephemeral pages into a single "raw"
 * (physical) page and tracking them with data structures so that
 * the raw pages can be easily reclaimed.
 *
 * A zbud page ("zbpg") is an aligned page containing a list_head,
 * a lock, and two "zbud headers".  The remainder of the physical
 * page is divided up into aligned 64-byte "chunks" which contain
 * the compressed data for zero, one, or two zbuds.  Each zbpg
 * resides on: (1) an "unused list" if it has no zbuds; (2) a
 * "buddied" list if it is fully populated  with two zbuds; or
 * (3) one of PAGE_SIZE/64 "unbuddied" lists indexed by how many chunks
 * the one unbuddied zbud uses.  The data inside a zbpg cannot be
 * read or written unless the zbpg's lock is held.
 */

#define ZBH_SENTINEL  0x43214321
#define ZBPG_SENTINEL  0xdeadbeef

#define ZBUD_MAX_BUDS 2

struct zbud_hdr {
	uint16_t client_id;
	uint16_t pool_id;
	struct tmem_oid oid;
	uint32_t index;
	uint16_t size; /* compressed size in bytes, zero means unused */
	DECL_SENTINEL
};

struct zbud_page {
	struct list_head bud_list;
	spinlock_t lock;
	struct zbud_hdr buddy[ZBUD_MAX_BUDS];
	DECL_SENTINEL
	/* followed by NUM_CHUNK aligned CHUNK_SIZE-byte chunks */
};

#define CHUNK_SHIFT	6
#define CHUNK_SIZE	(1 << CHUNK_SHIFT)
#define CHUNK_MASK	(~(CHUNK_SIZE-1))
#define NCHUNKS		(((PAGE_SIZE - sizeof(struct zbud_page)) & \
				CHUNK_MASK) >> CHUNK_SHIFT)
#define MAX_CHUNK	(NCHUNKS-1)

static struct {
	struct list_head list;
	unsigned count;
} zbud_unbuddied[NCHUNKS];
/* list N contains pages with N chunks USED and NCHUNKS-N unused */
/* element 0 is never used but optimizing that isn't worth it */
static unsigned long zbud_cumul_chunk_counts[NCHUNKS];

struct list_head zbud_buddied_list;
static unsigned long zcache_zbud_buddied_count;

/* protects the buddied list and all unbuddied lists */
static DEFINE_SPINLOCK(zbud_budlists_spinlock);

static atomic_t zcache_zbud_curr_raw_pages;
static atomic_t zcache_zbud_curr_zpages;
static unsigned long zcache_zbud_curr_zbytes;
static unsigned long zcache_zbud_cumul_zpages;
static unsigned long zcache_zbud_cumul_zbytes;
static unsigned long zcache_compress_poor;
static unsigned long zcache_mean_compress_poor;

/* forward references */
static void *zcache_get_free_page(void);

static void *qcache_alloc(void)
{
	void *addr;
	unsigned long flags;
	int offset;
	struct qcache_info *qc = &qcache_info;

	spin_lock_irqsave(&qc->lock, flags);
	offset = bitmap_find_free_region(qc->bitmap, qc->pages, 0);

	if (offset < 0) {
		spin_unlock_irqrestore(&qc->lock, flags);
		return NULL;
	}

	zcache_qc_allocated++;
	zcache_qc_used++;
	zcache_qc_max_used = max(zcache_qc_max_used, zcache_qc_used);
	spin_unlock_irqrestore(&qc->lock, flags);

	addr = qc->addr + offset * PAGE_SIZE;

	return addr;
}

static void qcache_free(void *addr)
{
	unsigned long flags;
	int offset;
	struct qcache_info *qc = &qcache_info;

	offset = (addr - qc->addr) / PAGE_SIZE;

	spin_lock_irqsave(&qc->lock, flags);
	bitmap_release_region(qc->bitmap, offset, 0);

	zcache_qc_freed++;
	zcache_qc_used--;
	spin_unlock_irqrestore(&qc->lock, flags);
}

/*
 * zbud helper functions
 */

static inline unsigned zbud_max_buddy_size(void)
{
	return MAX_CHUNK << CHUNK_SHIFT;
}

static inline unsigned zbud_size_to_chunks(unsigned size)
{
	BUG_ON(size == 0 || size > zbud_max_buddy_size());
	return (size + CHUNK_SIZE - 1) >> CHUNK_SHIFT;
}

static inline int zbud_budnum(struct zbud_hdr *zh)
{
	unsigned offset = (unsigned long)zh & (PAGE_SIZE - 1);
	struct zbud_page *zbpg = NULL;
	unsigned budnum = -1U;
	int i;

	for (i = 0; i < ZBUD_MAX_BUDS; i++)
		if (offset == offsetof(typeof(*zbpg), buddy[i])) {
			budnum = i;
			break;
		}
	BUG_ON(budnum == -1U);
	return budnum;
}

static char *zbud_data(struct zbud_hdr *zh, unsigned size)
{
	struct zbud_page *zbpg;
	char *p;
	unsigned budnum;

	ASSERT_SENTINEL(zh, ZBH);
	budnum = zbud_budnum(zh);
	BUG_ON(size == 0 || size > zbud_max_buddy_size());
	zbpg = container_of(zh, struct zbud_page, buddy[budnum]);
	p = (char *)zbpg;
	if (budnum == 0)
		p += ((sizeof(struct zbud_page) + CHUNK_SIZE - 1) &
							CHUNK_MASK);
	else if (budnum == 1)
		p += PAGE_SIZE - ((size + CHUNK_SIZE - 1) & CHUNK_MASK);
	return p;
}

/*
 * zbud raw page management
 */

static struct zbud_page *zbud_alloc_raw_page(void)
{
	struct zbud_page *zbpg = NULL;
	struct zbud_hdr *zh0, *zh1;

	zbpg = zcache_get_free_page();
	if (likely(zbpg != NULL)) {
		INIT_LIST_HEAD(&zbpg->bud_list);
		zh0 = &zbpg->buddy[0]; zh1 = &zbpg->buddy[1];
		spin_lock_init(&zbpg->lock);
		atomic_inc(&zcache_zbud_curr_raw_pages);
		INIT_LIST_HEAD(&zbpg->bud_list);
		SET_SENTINEL(zbpg, ZBPG);
		zh0->size = 0; zh1->size = 0;
		tmem_oid_set_invalid(&zh0->oid);
		tmem_oid_set_invalid(&zh1->oid);
	}
	return zbpg;
}

static void zbud_free_raw_page(struct zbud_page *zbpg)
{
	struct zbud_hdr *zh0 = &zbpg->buddy[0], *zh1 = &zbpg->buddy[1];

	ASSERT_SENTINEL(zbpg, ZBPG);
	BUG_ON(!list_empty(&zbpg->bud_list));
	BUG_ON(zh0->size != 0 || tmem_oid_valid(&zh0->oid));
	BUG_ON(zh1->size != 0 || tmem_oid_valid(&zh1->oid));
	INVERT_SENTINEL(zbpg, ZBPG);
	spin_unlock(&zbpg->lock);
	qcache_free(zbpg);
}

/*
 * core zbud handling routines
 */

static unsigned zbud_free(struct zbud_hdr *zh)
{
	unsigned size;

	ASSERT_SENTINEL(zh, ZBH);
	BUG_ON(!tmem_oid_valid(&zh->oid));
	size = zh->size;
	BUG_ON(zh->size == 0 || zh->size > zbud_max_buddy_size());
	zh->size = 0;
	tmem_oid_set_invalid(&zh->oid);
	INVERT_SENTINEL(zh, ZBH);
	zcache_zbud_curr_zbytes -= size;
	atomic_dec(&zcache_zbud_curr_zpages);
	return size;
}

static void zbud_free_and_delist(struct zbud_hdr *zh)
{
	unsigned chunks;
	struct zbud_hdr *zh_other;
	unsigned budnum = zbud_budnum(zh), size;
	struct zbud_page *zbpg =
		container_of(zh, struct zbud_page, buddy[budnum]);

	spin_lock(&zbpg->lock);
	if (list_empty(&zbpg->bud_list)) {
		spin_unlock(&zbpg->lock);
		return;
	}
	size = zbud_free(zh);
	zh_other = &zbpg->buddy[(budnum == 0) ? 1 : 0];
	if (zh_other->size == 0) { /* was unbuddied: unlist and free */
		chunks = zbud_size_to_chunks(size) ;
		spin_lock(&zbud_budlists_spinlock);
		BUG_ON(list_empty(&zbud_unbuddied[chunks].list));
		list_del_init(&zbpg->bud_list);
		zbud_unbuddied[chunks].count--;
		spin_unlock(&zbud_budlists_spinlock);
		zbud_free_raw_page(zbpg);
	} else { /* was buddied: move remaining buddy to unbuddied list */
		chunks = zbud_size_to_chunks(zh_other->size) ;
		spin_lock(&zbud_budlists_spinlock);
		list_del_init(&zbpg->bud_list);
		zcache_zbud_buddied_count--;
		list_add_tail(&zbpg->bud_list, &zbud_unbuddied[chunks].list);
		zbud_unbuddied[chunks].count++;
		spin_unlock(&zbud_budlists_spinlock);
		spin_unlock(&zbpg->lock);
	}
}

static struct zbud_hdr *zbud_create(uint16_t client_id, uint16_t pool_id,
					struct tmem_oid *oid,
					uint32_t index, struct page *page,
					void *cdata, unsigned size)
{
	struct zbud_hdr *zh0, *zh1, *zh = NULL;
	struct zbud_page *zbpg = NULL, *ztmp;
	unsigned nchunks;
	char *to;
	int i, found_good_buddy = 0;

	nchunks = zbud_size_to_chunks(size) ;
	for (i = MAX_CHUNK - nchunks + 1; i > 0; i--) {
		spin_lock(&zbud_budlists_spinlock);
		if (!list_empty(&zbud_unbuddied[i].list)) {
			list_for_each_entry_safe(zbpg, ztmp,
				    &zbud_unbuddied[i].list, bud_list) {
				if (spin_trylock(&zbpg->lock)) {
					found_good_buddy = i;
					goto found_unbuddied;
				}
			}
		}
		spin_unlock(&zbud_budlists_spinlock);
	}
	/* didn't find a good buddy, try allocating a new page */
	zbpg = zbud_alloc_raw_page();
	if (unlikely(zbpg == NULL))
		goto out;
	/* ok, have a page, now compress the data before taking locks */
	spin_lock(&zbpg->lock);
	spin_lock(&zbud_budlists_spinlock);
	list_add_tail(&zbpg->bud_list, &zbud_unbuddied[nchunks].list);
	zbud_unbuddied[nchunks].count++;
	zh = &zbpg->buddy[0];
	goto init_zh;

found_unbuddied:
	zh0 = &zbpg->buddy[0]; zh1 = &zbpg->buddy[1];
	BUG_ON(!((zh0->size == 0) ^ (zh1->size == 0)));
	if (zh0->size != 0) { /* buddy0 in use, buddy1 is vacant */
		ASSERT_SENTINEL(zh0, ZBH);
		zh = zh1;
	} else if (zh1->size != 0) { /* buddy1 in use, buddy0 is vacant */
		ASSERT_SENTINEL(zh1, ZBH);
		zh = zh0;
	} else
		BUG();
	list_del_init(&zbpg->bud_list);
	zbud_unbuddied[found_good_buddy].count--;
	list_add_tail(&zbpg->bud_list, &zbud_buddied_list);
	zcache_zbud_buddied_count++;

init_zh:
	SET_SENTINEL(zh, ZBH);
	zh->size = size;
	zh->index = index;
	zh->oid = *oid;
	zh->pool_id = pool_id;
	zh->client_id = client_id;
	/* can wait to copy the data until the list locks are dropped */
	spin_unlock(&zbud_budlists_spinlock);

	to = zbud_data(zh, size);
	memcpy(to, cdata, size);
	spin_unlock(&zbpg->lock);
	zbud_cumul_chunk_counts[nchunks]++;
	atomic_inc(&zcache_zbud_curr_zpages);
	zcache_zbud_cumul_zpages++;
	zcache_zbud_curr_zbytes += size;
	zcache_zbud_cumul_zbytes += size;
out:
	return zh;
}

static int zbud_decompress(struct page *page, struct zbud_hdr *zh)
{
	struct zbud_page *zbpg;
	unsigned budnum = zbud_budnum(zh);
	size_t out_len = PAGE_SIZE;
	char *to_va, *from_va;
	unsigned size;
	int ret = 0;

	zbpg = container_of(zh, struct zbud_page, buddy[budnum]);
	spin_lock(&zbpg->lock);
	if (list_empty(&zbpg->bud_list)) {
		ret = -EINVAL;
		goto out;
	}
	ASSERT_SENTINEL(zh, ZBH);
	BUG_ON(zh->size == 0 || zh->size > zbud_max_buddy_size());
	to_va = kmap_atomic(page);
	size = zh->size;
	from_va = zbud_data(zh, size);
	ret = lzo1x_decompress_safe(from_va, size, to_va, &out_len);
	BUG_ON(ret != LZO_E_OK);
	BUG_ON(out_len != PAGE_SIZE);
	kunmap_atomic(to_va);
out:
	spin_unlock(&zbpg->lock);
	return ret;
}

static struct tmem_pool *zcache_get_pool_by_id(uint16_t cli_id,
						uint16_t poolid);
static void zcache_put_pool(struct tmem_pool *pool);

static void zbud_init(void)
{
	int i;

	INIT_LIST_HEAD(&zbud_buddied_list);
	zcache_zbud_buddied_count = 0;
	for (i = 0; i < NCHUNKS; i++) {
		INIT_LIST_HEAD(&zbud_unbuddied[i].list);
		zbud_unbuddied[i].count = 0;
	}
}

#ifdef CONFIG_SYSFS
/*
 * These sysfs routines show a nice distribution of how many zbpg's are
 * currently (and have ever been placed) in each unbuddied list.  It's fun
 * to watch but can probably go away before final merge.
 */
static int zbud_show_unbuddied_list_counts(char *buf)
{
	int i;
	char *p = buf;

	for (i = 0; i < NCHUNKS; i++)
		p += sprintf(p, "%u ", zbud_unbuddied[i].count);
	return p - buf;
}

static int zbud_show_cumul_chunk_counts(char *buf)
{
	unsigned long i, chunks = 0, total_chunks = 0, sum_total_chunks = 0;
	unsigned long total_chunks_lte_21 = 0, total_chunks_lte_32 = 0;
	unsigned long total_chunks_lte_42 = 0;
	char *p = buf;

	for (i = 0; i < NCHUNKS; i++) {
		p += sprintf(p, "%lu ", zbud_cumul_chunk_counts[i]);
		chunks += zbud_cumul_chunk_counts[i];
		total_chunks += zbud_cumul_chunk_counts[i];
		sum_total_chunks += i * zbud_cumul_chunk_counts[i];
		if (i == 21)
			total_chunks_lte_21 = total_chunks;
		if (i == 32)
			total_chunks_lte_32 = total_chunks;
		if (i == 42)
			total_chunks_lte_42 = total_chunks;
	}
	p += sprintf(p, "<=21:%lu <=32:%lu <=42:%lu, mean:%lu\n",
		total_chunks_lte_21, total_chunks_lte_32, total_chunks_lte_42,
		chunks == 0 ? 0 : sum_total_chunks / chunks);
	return p - buf;
}
#endif

/*
 * zcache core code starts here
 */

/* useful stats not collected by cleancache or frontswap */
static unsigned long zcache_flush_total;
static unsigned long zcache_flush_found;
static unsigned long zcache_flobj_total;
static unsigned long zcache_flobj_found;
static unsigned long zcache_failed_eph_puts;

/*
 * Tmem operations assume the poolid implies the invoking client.
 * Zcache only has one client (the kernel itself): LOCAL_CLIENT.
 * RAMster has each client numbered by cluster node, and a KVM version
 * of zcache would have one client per guest and each client might
 * have a poolid==N.
 */
static struct tmem_pool *zcache_get_pool_by_id(uint16_t cli_id, uint16_t poolid)
{
	struct tmem_pool *pool = NULL;
	struct zcache_client *cli = NULL;

	if (cli_id == LOCAL_CLIENT)
		cli = &zcache_host;
	else {
		if (cli_id >= MAX_CLIENTS)
			goto out;
		cli = &zcache_clients[cli_id];
		if (cli == NULL)
			goto out;
		atomic_inc(&cli->refcount);
	}
	if (poolid < MAX_POOLS_PER_CLIENT) {
		pool = cli->tmem_pools[poolid];
		if (pool != NULL)
			atomic_inc(&pool->refcount);
	}
out:
	return pool;
}

static void zcache_put_pool(struct tmem_pool *pool)
{
	struct zcache_client *cli = NULL;

	if (pool == NULL)
		BUG();
	cli = pool->client;
	atomic_dec(&pool->refcount);
	atomic_dec(&cli->refcount);
}

int zcache_new_client(uint16_t cli_id)
{
	struct zcache_client *cli = NULL;
	int ret = -1;

	if (cli_id == LOCAL_CLIENT)
		cli = &zcache_host;
	else if ((unsigned int)cli_id < MAX_CLIENTS)
		cli = &zcache_clients[cli_id];
	if (cli == NULL)
		goto out;
	if (cli->allocated)
		goto out;
	cli->allocated = 1;
	ret = 0;
out:
	return ret;
}

/* counters for debugging */
static unsigned long zcache_failed_get_free_pages;
static unsigned long zcache_failed_alloc;
static unsigned long zcache_put_to_flush;
static unsigned long zcache_aborted_preload;
static unsigned long zcache_aborted_shrink;

/*
 * Ensure that memory allocation requests in zcache don't result
 * in direct reclaim requests via the shrinker, which would cause
 * an infinite loop.  Maybe a GFP flag would be better?
 */
static DEFINE_SPINLOCK(zcache_direct_reclaim_lock);

/*
 * for now, used named slabs so can easily track usage; later can
 * either just use kmalloc, or perhaps add a slab-like allocator
 * to more carefully manage total memory utilization
 */
static struct kmem_cache *zcache_objnode_cache;
static struct kmem_cache *zcache_obj_cache;
static atomic_t zcache_curr_obj_count = ATOMIC_INIT(0);
static unsigned long zcache_curr_obj_count_max;
static atomic_t zcache_curr_objnode_count = ATOMIC_INIT(0);
static unsigned long zcache_curr_objnode_count_max;

/*
 * to avoid memory allocation recursion (e.g. due to direct reclaim), we
 * preload all necessary data structures so the hostops callbacks never
 * actually do a malloc
 */
struct zcache_preload {
	void *page;
	struct tmem_obj *obj;
	int nr;
	struct tmem_objnode *objnodes[OBJNODE_TREE_MAX_PATH];
};
static DEFINE_PER_CPU(struct zcache_preload, zcache_preloads) = { 0, };

static int zcache_do_preload(struct tmem_pool *pool)
{
	struct zcache_preload *kp;
	struct tmem_objnode *objnode;
	struct tmem_obj *obj;
	void *page;
	int ret = -ENOMEM;

	if (unlikely(zcache_objnode_cache == NULL))
		goto out;
	if (unlikely(zcache_obj_cache == NULL))
		goto out;
	if (!spin_trylock(&zcache_direct_reclaim_lock)) {
		zcache_aborted_preload++;
		goto out;
	}
	preempt_disable();
	kp = &__get_cpu_var(zcache_preloads);
	while (kp->nr < ARRAY_SIZE(kp->objnodes)) {
		preempt_enable_no_resched();
		objnode = kmem_cache_alloc(zcache_objnode_cache,
				ZCACHE_GFP_MASK);
		if (unlikely(objnode == NULL)) {
			zcache_failed_alloc++;
			goto unlock_out;
		}
		preempt_disable();
		kp = &__get_cpu_var(zcache_preloads);
		if (kp->nr < ARRAY_SIZE(kp->objnodes))
			kp->objnodes[kp->nr++] = objnode;
		else
			kmem_cache_free(zcache_objnode_cache, objnode);
	}
	preempt_enable_no_resched();
	obj = kmem_cache_alloc(zcache_obj_cache, ZCACHE_GFP_MASK);
	if (unlikely(obj == NULL)) {
		zcache_failed_alloc++;
		goto unlock_out;
	}
	page = qcache_alloc();
	if (unlikely(page == NULL)) {
		zcache_failed_get_free_pages++;
		kmem_cache_free(zcache_obj_cache, obj);
		goto unlock_out;
	}
	preempt_disable();
	kp = &__get_cpu_var(zcache_preloads);
	if (kp->obj == NULL)
		kp->obj = obj;
	else
		kmem_cache_free(zcache_obj_cache, obj);
	if (kp->page == NULL)
		kp->page = page;
	else
		qcache_free(page);
	ret = 0;
unlock_out:
	spin_unlock(&zcache_direct_reclaim_lock);
out:
	return ret;
}

static void *zcache_get_free_page(void)
{
	struct zcache_preload *kp;
	void *page;

	kp = &__get_cpu_var(zcache_preloads);
	page = kp->page;
	BUG_ON(page == NULL);
	kp->page = NULL;
	return page;
}

/*
 * zcache implementation for tmem host ops
 */

static struct tmem_objnode *zcache_objnode_alloc(struct tmem_pool *pool)
{
	struct tmem_objnode *objnode = NULL;
	unsigned long count;
	struct zcache_preload *kp;

	kp = &__get_cpu_var(zcache_preloads);
	if (kp->nr <= 0)
		goto out;
	objnode = kp->objnodes[kp->nr - 1];
	BUG_ON(objnode == NULL);
	kp->objnodes[kp->nr - 1] = NULL;
	kp->nr--;
	count = atomic_inc_return(&zcache_curr_objnode_count);
	if (count > zcache_curr_objnode_count_max)
		zcache_curr_objnode_count_max = count;
out:
	return objnode;
}

static void zcache_objnode_free(struct tmem_objnode *objnode,
					struct tmem_pool *pool)
{
	atomic_dec(&zcache_curr_objnode_count);
	BUG_ON(atomic_read(&zcache_curr_objnode_count) < 0);
	kmem_cache_free(zcache_objnode_cache, objnode);
}

static struct tmem_obj *zcache_obj_alloc(struct tmem_pool *pool)
{
	struct tmem_obj *obj = NULL;
	unsigned long count;
	struct zcache_preload *kp;

	kp = &__get_cpu_var(zcache_preloads);
	obj = kp->obj;
	BUG_ON(obj == NULL);
	kp->obj = NULL;
	count = atomic_inc_return(&zcache_curr_obj_count);
	if (count > zcache_curr_obj_count_max)
		zcache_curr_obj_count_max = count;
	return obj;
}

static void zcache_obj_free(struct tmem_obj *obj, struct tmem_pool *pool)
{
	atomic_dec(&zcache_curr_obj_count);
	BUG_ON(atomic_read(&zcache_curr_obj_count) < 0);
	kmem_cache_free(zcache_obj_cache, obj);
}

static void zcache_flush_all_obj(void)
{
	struct tmem_pool *pool;
	int pool_id;
	struct zcache_preload *kp;

	kp = &__get_cpu_var(zcache_preloads);

	for (pool_id = 0; pool_id < MAX_POOLS_PER_CLIENT; pool_id++) {
		pool = zcache_get_pool_by_id(LOCAL_CLIENT, pool_id);
		tmem_flush_pool(pool);
		if (pool)
			zcache_put_pool(pool);
	}
	if (kp->page) {
		qcache_free(kp->page);
		kp->page = NULL;
	}
	if (zcache_qc_used)
		pr_warn("pages used not 0 after qcache flush all, is %ld\n",
			zcache_qc_used);
}

/*
 * When zcache is disabled ("frozen"), pools can be created and destroyed,
 * but all puts (and thus all other operations that require memory allocation)
 * must fail.  If zcache is unfrozen, accepts puts, then frozen again,
 * data consistency requires all puts while frozen to be converted into
 * flushes.
 */
static bool zcache_freeze;

static void zcache_control(bool freeze)
{
	zcache_freeze = freeze;
}

static struct tmem_hostops zcache_hostops = {
	.obj_alloc = zcache_obj_alloc,
	.obj_free = zcache_obj_free,
	.objnode_alloc = zcache_objnode_alloc,
	.objnode_free = zcache_objnode_free,
	.flush_all_obj = zcache_flush_all_obj,
	.control = zcache_control,
};

/*
 * zcache implementations for PAM page descriptor ops
 */

static atomic_t zcache_curr_eph_pampd_count = ATOMIC_INIT(0);
static unsigned long zcache_curr_eph_pampd_count_max;

/* forward reference */
static int zcache_compress(struct page *from, void **out_va, size_t *out_len);

static void *zcache_pampd_create(char *data, size_t size, bool raw, int eph,
				struct tmem_pool *pool, struct tmem_oid *oid,
				 uint32_t index)
{
	void *pampd = NULL, *cdata;
	size_t clen;
	int ret;
	unsigned long count;
	struct page *page = (struct page *)(data);
	struct zcache_client *cli = pool->client;
	uint16_t client_id = get_client_id_from_client(cli);

	ret = zcache_compress(page, &cdata, &clen);
	if (ret == 0)
		goto out;
	if (clen == 0 || clen > zbud_max_buddy_size()) {
		zcache_compress_poor++;
		goto out;
	}
	pampd = (void *)zbud_create(client_id, pool->pool_id, oid,
					index, page, cdata, clen);
	if (pampd != NULL) {
		count = atomic_inc_return(&zcache_curr_eph_pampd_count);
		if (count > zcache_curr_eph_pampd_count_max)
			zcache_curr_eph_pampd_count_max = count;
	}
out:
	return pampd;
}

/*
 * fill the pageframe corresponding to the struct page with the data
 * from the passed pampd
 */
static int zcache_pampd_get_data(char *data, size_t *bufsize, bool raw,
					void *pampd, struct tmem_pool *pool,
					struct tmem_oid *oid, uint32_t index)
{
	BUG();
	return 0;
}

/*
 * fill the pageframe corresponding to the struct page with the data
 * from the passed pampd
 */
static int zcache_pampd_get_data_and_free(char *data, size_t *bufsize, bool raw,
					void *pampd, struct tmem_pool *pool,
					struct tmem_oid *oid, uint32_t index)
{
	int ret = 0;

	zbud_decompress((struct page *)(data), pampd);
	zbud_free_and_delist((struct zbud_hdr *)pampd);
	atomic_dec(&zcache_curr_eph_pampd_count);
	return ret;
}

/*
 * free the pampd and remove it from any zcache lists
 * pampd must no longer be pointed to from any tmem data structures!
 */
static void zcache_pampd_free(void *pampd, struct tmem_pool *pool,
				struct tmem_oid *oid, uint32_t index)
{
	zbud_free_and_delist((struct zbud_hdr *)pampd);
	atomic_dec(&zcache_curr_eph_pampd_count);
	BUG_ON(atomic_read(&zcache_curr_eph_pampd_count) < 0);
}

static void zcache_pampd_free_obj(struct tmem_pool *pool, struct tmem_obj *obj)
{
}

static void zcache_pampd_new_obj(struct tmem_obj *obj)
{
}

static int zcache_pampd_replace_in_obj(void *pampd, struct tmem_obj *obj)
{
	return -1;
}

static bool zcache_pampd_is_remote(void *pampd)
{
	return 0;
}

static struct tmem_pamops zcache_pamops = {
	.create = zcache_pampd_create,
	.get_data = zcache_pampd_get_data,
	.get_data_and_free = zcache_pampd_get_data_and_free,
	.free = zcache_pampd_free,
	.free_obj = zcache_pampd_free_obj,
	.new_obj = zcache_pampd_new_obj,
	.replace_in_obj = zcache_pampd_replace_in_obj,
	.is_remote = zcache_pampd_is_remote,
};

/*
 * zcache compression/decompression and related per-cpu stuff
 */

#define LZO_WORKMEM_BYTES LZO1X_1_MEM_COMPRESS
#define LZO_DSTMEM_PAGE_ORDER 1
static DEFINE_PER_CPU(unsigned char *, zcache_workmem);
static DEFINE_PER_CPU(unsigned char *, zcache_dstmem);

static int zcache_compress(struct page *from, void **out_va, size_t *out_len)
{
	int ret = 0;
	unsigned char *dmem = __get_cpu_var(zcache_dstmem);
	unsigned char *wmem = __get_cpu_var(zcache_workmem);
	char *from_va;

	BUG_ON(!irqs_disabled());
	if (unlikely(dmem == NULL || wmem == NULL))
		goto out;  /* no buffer, so can't compress */
	from_va = kmap_atomic(from);
	mb();
	ret = lzo1x_1_compress(from_va, PAGE_SIZE, dmem, out_len, wmem);
	BUG_ON(ret != LZO_E_OK);
	*out_va = dmem;
	kunmap_atomic(from_va);
	ret = 1;
out:
	return ret;
}

#ifdef CONFIG_SYSFS
#define ZCACHE_SYSFS_RO(_name) \
	static ssize_t zcache_##_name##_show(struct kobject *kobj, \
				struct kobj_attribute *attr, char *buf) \
	{ \
		return sprintf(buf, "%lu\n", zcache_##_name); \
	} \
	static struct kobj_attribute zcache_##_name##_attr = { \
		.attr = { .name = __stringify(_name), .mode = 0444 }, \
		.show = zcache_##_name##_show, \
	}

#define ZCACHE_SYSFS_RO_ATOMIC(_name) \
	static ssize_t zcache_##_name##_show(struct kobject *kobj, \
				struct kobj_attribute *attr, char *buf) \
	{ \
	    return sprintf(buf, "%d\n", atomic_read(&zcache_##_name)); \
	} \
	static struct kobj_attribute zcache_##_name##_attr = { \
		.attr = { .name = __stringify(_name), .mode = 0444 }, \
		.show = zcache_##_name##_show, \
	}

#define ZCACHE_SYSFS_RO_CUSTOM(_name, _func) \
	static ssize_t zcache_##_name##_show(struct kobject *kobj, \
				struct kobj_attribute *attr, char *buf) \
	{ \
	    return _func(buf); \
	} \
	static struct kobj_attribute zcache_##_name##_attr = { \
		.attr = { .name = __stringify(_name), .mode = 0444 }, \
		.show = zcache_##_name##_show, \
	}

ZCACHE_SYSFS_RO(curr_obj_count_max);
ZCACHE_SYSFS_RO(curr_objnode_count_max);
ZCACHE_SYSFS_RO(flush_total);
ZCACHE_SYSFS_RO(flush_found);
ZCACHE_SYSFS_RO(flobj_total);
ZCACHE_SYSFS_RO(flobj_found);
ZCACHE_SYSFS_RO(failed_eph_puts);
ZCACHE_SYSFS_RO(zbud_curr_zbytes);
ZCACHE_SYSFS_RO(zbud_cumul_zpages);
ZCACHE_SYSFS_RO(zbud_cumul_zbytes);
ZCACHE_SYSFS_RO(zbud_buddied_count);
ZCACHE_SYSFS_RO(failed_get_free_pages);
ZCACHE_SYSFS_RO(failed_alloc);
ZCACHE_SYSFS_RO(put_to_flush);
ZCACHE_SYSFS_RO(aborted_preload);
ZCACHE_SYSFS_RO(aborted_shrink);
ZCACHE_SYSFS_RO(compress_poor);
ZCACHE_SYSFS_RO(mean_compress_poor);
ZCACHE_SYSFS_RO(qc_allocated);
ZCACHE_SYSFS_RO(qc_freed);
ZCACHE_SYSFS_RO(qc_used);
ZCACHE_SYSFS_RO(qc_max_used);
ZCACHE_SYSFS_RO_ATOMIC(zbud_curr_raw_pages);
ZCACHE_SYSFS_RO_ATOMIC(zbud_curr_zpages);
ZCACHE_SYSFS_RO_ATOMIC(curr_obj_count);
ZCACHE_SYSFS_RO_ATOMIC(curr_objnode_count);
ZCACHE_SYSFS_RO_CUSTOM(zbud_unbuddied_list_counts,
			zbud_show_unbuddied_list_counts);
ZCACHE_SYSFS_RO_CUSTOM(zbud_cumul_chunk_counts,
			zbud_show_cumul_chunk_counts);

static struct attribute *qcache_attrs[] = {
	&zcache_curr_obj_count_attr.attr,
	&zcache_curr_obj_count_max_attr.attr,
	&zcache_curr_objnode_count_attr.attr,
	&zcache_curr_objnode_count_max_attr.attr,
	&zcache_flush_total_attr.attr,
	&zcache_flobj_total_attr.attr,
	&zcache_flush_found_attr.attr,
	&zcache_flobj_found_attr.attr,
	&zcache_failed_eph_puts_attr.attr,
	&zcache_compress_poor_attr.attr,
	&zcache_mean_compress_poor_attr.attr,
	&zcache_zbud_curr_raw_pages_attr.attr,
	&zcache_zbud_curr_zpages_attr.attr,
	&zcache_zbud_curr_zbytes_attr.attr,
	&zcache_zbud_cumul_zpages_attr.attr,
	&zcache_zbud_cumul_zbytes_attr.attr,
	&zcache_zbud_buddied_count_attr.attr,
	&zcache_failed_get_free_pages_attr.attr,
	&zcache_failed_alloc_attr.attr,
	&zcache_put_to_flush_attr.attr,
	&zcache_aborted_preload_attr.attr,
	&zcache_aborted_shrink_attr.attr,
	&zcache_zbud_unbuddied_list_counts_attr.attr,
	&zcache_zbud_cumul_chunk_counts_attr.attr,
	&zcache_qc_allocated_attr.attr,
	&zcache_qc_freed_attr.attr,
	&zcache_qc_used_attr.attr,
	&zcache_qc_max_used_attr.attr,
	NULL,
};

static struct attribute_group qcache_attr_group = {
	.attrs = qcache_attrs,
	.name = "qcache",
};

#endif /* CONFIG_SYSFS */

/*
 * zcache shims between cleancache ops and tmem
 */

static int zcache_put_page(int cli_id, int pool_id, struct tmem_oid *oidp,
				uint32_t index, struct page *page)
{
	struct tmem_pool *pool;
	int ret = -1;

	BUG_ON(!irqs_disabled());
	pool = zcache_get_pool_by_id(cli_id, pool_id);
	if (unlikely(pool == NULL))
		goto out;
	if (!zcache_freeze && zcache_do_preload(pool) == 0) {
		/* preload does preempt_disable on success */
		ret = tmem_put(pool, oidp, index, (char *)(page),
				PAGE_SIZE, 0, is_ephemeral(pool));
		if (ret < 0) {
			zcache_failed_eph_puts++;
		}
		zcache_put_pool(pool);
		preempt_enable_no_resched();
	} else {
		zcache_put_to_flush++;
		if (atomic_read(&pool->obj_count) > 0)
			/* the put fails whether the flush succeeds or not */
			(void)tmem_flush_page(pool, oidp, index);
		zcache_put_pool(pool);
	}
out:
	return ret;
}

static int zcache_get_page(int cli_id, int pool_id, struct tmem_oid *oidp,
				uint32_t index, struct page *page)
{
	struct tmem_pool *pool;
	int ret = -1;
	unsigned long flags;
	size_t size = PAGE_SIZE;

	local_irq_save(flags);
	pool = zcache_get_pool_by_id(cli_id, pool_id);
	if (likely(pool != NULL)) {
		if (atomic_read(&pool->obj_count) > 0)
			ret = tmem_get(pool, oidp, index, (char *)(page),
					&size, 0, is_ephemeral(pool));
		zcache_put_pool(pool);
	}
	local_irq_restore(flags);
	return ret;
}

static int zcache_flush_page(int cli_id, int pool_id,
				struct tmem_oid *oidp, uint32_t index)
{
	struct tmem_pool *pool;
	int ret = -1;
	unsigned long flags;

	local_irq_save(flags);
	zcache_flush_total++;
	pool = zcache_get_pool_by_id(cli_id, pool_id);
	if (likely(pool != NULL)) {
		if (atomic_read(&pool->obj_count) > 0)
			ret = tmem_flush_page(pool, oidp, index);
		zcache_put_pool(pool);
	}
	if (ret >= 0)
		zcache_flush_found++;
	local_irq_restore(flags);
	return ret;
}

static int zcache_flush_object(int cli_id, int pool_id,
				struct tmem_oid *oidp)
{
	struct tmem_pool *pool;
	int ret = -1;
	unsigned long flags;

	local_irq_save(flags);
	zcache_flobj_total++;
	pool = zcache_get_pool_by_id(cli_id, pool_id);
	if (likely(pool != NULL)) {
		if (atomic_read(&pool->obj_count) > 0)
			ret = tmem_flush_object(pool, oidp);
		zcache_put_pool(pool);
	}
	if (ret >= 0)
		zcache_flobj_found++;
	local_irq_restore(flags);
	return ret;
}

static int zcache_destroy_pool(int cli_id, int pool_id)
{
	struct tmem_pool *pool = NULL;
	struct zcache_client *cli = NULL;
	int ret = -1;

	if (pool_id < 0)
		goto out;
	if (cli_id == LOCAL_CLIENT)
		cli = &zcache_host;
	else if ((unsigned int)cli_id < MAX_CLIENTS)
		cli = &zcache_clients[cli_id];
	if (cli == NULL)
		goto out;
	atomic_inc(&cli->refcount);
	pool = cli->tmem_pools[pool_id];
	if (pool == NULL)
		goto out;
	cli->tmem_pools[pool_id] = NULL;
	/* wait for pool activity on other cpus to quiesce */
	while (atomic_read(&pool->refcount) != 0)
		;
	atomic_dec(&cli->refcount);
	local_bh_disable();
	ret = tmem_destroy_pool(pool);
	local_bh_enable();
	kfree(pool);
	pr_info("qcache: destroyed pool id=%d, cli_id=%d\n",
			pool_id, cli_id);
out:
	return ret;
}

static int zcache_new_pool(uint16_t cli_id, uint32_t flags)
{
	int poolid = -1;
	struct tmem_pool *pool;
	struct zcache_client *cli = NULL;

	if (cli_id == LOCAL_CLIENT)
		cli = &zcache_host;
	else if ((unsigned int)cli_id < MAX_CLIENTS)
		cli = &zcache_clients[cli_id];
	if (cli == NULL)
		goto out;
	atomic_inc(&cli->refcount);
	pool = kmalloc(sizeof(struct tmem_pool), GFP_KERNEL);
	if (pool == NULL) {
		pr_info("qcache: pool creation failed: out of memory\n");
		goto out;
	}

	for (poolid = 0; poolid < MAX_POOLS_PER_CLIENT; poolid++)
		if (cli->tmem_pools[poolid] == NULL)
			break;
	if (poolid >= MAX_POOLS_PER_CLIENT) {
		pr_info("qcache: pool creation failed: max exceeded\n");
		kfree(pool);
		poolid = -1;
		goto out;
	}
	atomic_set(&pool->refcount, 0);
	pool->client = cli;
	pool->pool_id = poolid;
	tmem_new_pool(pool, flags);
	cli->tmem_pools[poolid] = pool;
	pr_info("qcache: created %s tmem pool, id=%d, client=%d\n",
		flags & TMEM_POOL_PERSIST ? "persistent" : "ephemeral",
		poolid, cli_id);
out:
	if (cli != NULL)
		atomic_dec(&cli->refcount);
	return poolid;
}

/**********
 * Two kernel functionalities currently can be layered on top of tmem.
 * These are "cleancache" which is used as a second-chance cache for clean
 * page cache pages; and "frontswap" which is used for swap pages
 * to avoid writes to disk.  A generic "shim" is provided here for each
 * to translate in-kernel semantics to zcache semantics.
 */

static void zcache_cleancache_put_page(int pool_id,
					struct cleancache_filekey key,
					pgoff_t index, struct page *page)
{
	u32 ind = (u32) index;
	struct tmem_oid oid = *(struct tmem_oid *)&key;

	if (likely(ind == index))
		(void)zcache_put_page(LOCAL_CLIENT, pool_id, &oid, index, page);
}

static int zcache_cleancache_get_page(int pool_id,
					struct cleancache_filekey key,
					pgoff_t index, struct page *page)
{
	u32 ind = (u32) index;
	struct tmem_oid oid = *(struct tmem_oid *)&key;
	int ret = -1;

	if (likely(ind == index))
		ret = zcache_get_page(LOCAL_CLIENT, pool_id, &oid, index, page);
	return ret;
}

static void zcache_cleancache_flush_page(int pool_id,
					struct cleancache_filekey key,
					pgoff_t index)
{
	u32 ind = (u32) index;
	struct tmem_oid oid = *(struct tmem_oid *)&key;

	if (likely(ind == index))
		(void)zcache_flush_page(LOCAL_CLIENT, pool_id, &oid, ind);
}

static void zcache_cleancache_flush_inode(int pool_id,
					struct cleancache_filekey key)
{
	struct tmem_oid oid = *(struct tmem_oid *)&key;

	(void)zcache_flush_object(LOCAL_CLIENT, pool_id, &oid);
}

static void zcache_cleancache_flush_fs(int pool_id)
{
	if (pool_id >= 0)
		(void)zcache_destroy_pool(LOCAL_CLIENT, pool_id);
}

static int zcache_cleancache_init_fs(size_t pagesize)
{
	BUG_ON(sizeof(struct cleancache_filekey) !=
				sizeof(struct tmem_oid));
	BUG_ON(pagesize != PAGE_SIZE);
	return zcache_new_pool(LOCAL_CLIENT, 0);
}

static int zcache_cleancache_init_shared_fs(char *uuid, size_t pagesize)
{
	/* shared pools are unsupported and map to private */
	BUG_ON(sizeof(struct cleancache_filekey) !=
				sizeof(struct tmem_oid));
	BUG_ON(pagesize != PAGE_SIZE);
	return zcache_new_pool(LOCAL_CLIENT, 0);
}

static struct cleancache_ops zcache_cleancache_ops = {
	.put_page = zcache_cleancache_put_page,
	.get_page = zcache_cleancache_get_page,
	.invalidate_page = zcache_cleancache_flush_page,
	.invalidate_inode = zcache_cleancache_flush_inode,
	.invalidate_fs = zcache_cleancache_flush_fs,
	.init_shared_fs = zcache_cleancache_init_shared_fs,
	.init_fs = zcache_cleancache_init_fs
};

struct cleancache_ops zcache_cleancache_register_ops(void)
{
	struct cleancache_ops old_ops =
		cleancache_register_ops(&zcache_cleancache_ops);

	return old_ops;
}

static int __init qcache_init(void)
{
	int ret = 0;
	struct qcache_info *qc = &qcache_info;
	struct fmem_data *fdp;
	int bitmap_size;
	unsigned int cpu;
	struct cleancache_ops old_ops;

#ifdef CONFIG_SYSFS
	ret = sysfs_create_group(mm_kobj, &qcache_attr_group);
	if (ret) {
		pr_err("qcache: can't create sysfs\n");
		goto out;
	}
#endif /* CONFIG_SYSFS */

	fdp = fmem_get_info();
	qc->addr = fdp->virt;
	qc->pages = fdp->size >> PAGE_SHIFT;
	if (!qc->pages)
		goto out;

	tmem_register_hostops(&zcache_hostops);
	tmem_register_pamops(&zcache_pamops);
	for_each_online_cpu(cpu) {
		per_cpu(zcache_dstmem, cpu) = (void *)__get_free_pages(
			GFP_KERNEL | __GFP_REPEAT,
			LZO_DSTMEM_PAGE_ORDER),
		per_cpu(zcache_workmem, cpu) =
			kzalloc(LZO1X_MEM_COMPRESS,
				GFP_KERNEL | __GFP_REPEAT);
	}
	zcache_objnode_cache = kmem_cache_create("zcache_objnode",
				sizeof(struct tmem_objnode), 0, 0, NULL);
	zcache_obj_cache = kmem_cache_create("zcache_obj",
				sizeof(struct tmem_obj), 0, 0, NULL);
	ret = zcache_new_client(LOCAL_CLIENT);
	if (ret) {
		pr_err("qcache: can't create client\n");
		goto out;
	}

	zbud_init();
	old_ops = zcache_cleancache_register_ops();
	pr_info("qcache: cleancache enabled using kernel "
		"transcendent memory and compression buddies\n");
	if (old_ops.init_fs != NULL)
		pr_warning("qcache: cleancache_ops overridden");


	bitmap_size = BITS_TO_LONGS(qc->pages) * sizeof(long);

	qc->bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!qc->bitmap) {
		pr_info("can't allocate qcache bitmap!\n");
		ret = -ENOMEM;
		goto out;
	}
	spin_lock_init(&qc->lock);

	fmem_set_state(FMEM_T_STATE);

out:
	return ret;
}

module_init(qcache_init)
