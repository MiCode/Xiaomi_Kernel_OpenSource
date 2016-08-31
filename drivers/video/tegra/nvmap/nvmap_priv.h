/*
 * drivers/video/tegra/nvmap/nvmap.h
 *
 * GPU memory management driver for Tegra
 *
 * Copyright (c) 2009-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *'
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __VIDEO_TEGRA_NVMAP_NVMAP_H
#define __VIDEO_TEGRA_NVMAP_NVMAP_H

#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/dma-buf.h>
#include <linux/syscalls.h>
#include <linux/nvmap.h>

#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direction.h>
#include <linux/platform_device.h>

#include <asm/tlbflush.h>
#include <asm/cacheflush.h>

#include "nvmap_heap.h"

#ifdef CONFIG_NVMAP_HIGHMEM_ONLY
#define __GFP_NVMAP     __GFP_HIGHMEM
#else
#define __GFP_NVMAP     (GFP_KERNEL | __GFP_HIGHMEM)
#endif

#ifdef CONFIG_NVMAP_FORCE_ZEROED_USER_PAGES
#define NVMAP_ZEROED_PAGES     __GFP_ZERO
#else
#define NVMAP_ZEROED_PAGES     0
#endif

#define GFP_NVMAP              (__GFP_NVMAP | __GFP_NOWARN | NVMAP_ZEROED_PAGES)

#define NVMAP_NUM_PTES		64

struct nvmap_share;
struct page;

extern const struct file_operations nvmap_fd_fops;
void _nvmap_handle_free(struct nvmap_handle *h);
extern struct nvmap_share *nvmap_share;
/* holds max number of handles allocted per process at any time */
extern u32 nvmap_max_handle_count;

extern struct platform_device *nvmap_pdev;

#if defined(CONFIG_TEGRA_NVMAP)
#define nvmap_err(_client, _fmt, ...)				\
	dev_err(nvmap_client_to_device(_client),		\
		"%s: "_fmt, __func__, ##__VA_ARGS__)

#define nvmap_warn(_client, _fmt, ...)				\
	dev_warn(nvmap_client_to_device(_client),		\
		 "%s: "_fmt, __func__, ##__VA_ARGS__)

#define nvmap_debug(_client, _fmt, ...)				\
	dev_dbg(nvmap_client_to_device(_client),		\
		"%s: "_fmt, __func__, ##__VA_ARGS__)

#define CACHE_MAINT_IMMEDIATE		0
#define CACHE_MAINT_ALLOW_DEFERRED	1

struct nvmap_deferred_ops {
	struct list_head ops_list;
	spinlock_t deferred_ops_lock;
	bool enable_deferred_cache_maintenance;
	u64 deferred_maint_inner_requested;
	u64 deferred_maint_inner_flushed;
	u64 deferred_maint_outer_requested;
	u64 deferred_maint_outer_flushed;
};

/* handles allocated using shared system memory (either IOVMM- or high-order
 * page allocations */
struct nvmap_pgalloc {
	struct page **pages;
	bool contig;			/* contiguous system memory */
	u32 iovm_addr;	/* is non-zero, if client need specific iova mapping */
};

struct nvmap_handle {
	struct rb_node node;	/* entry on global handle tree */
	atomic_t ref;		/* reference count (i.e., # of duplications) */
	atomic_t pin;		/* pin count */
	atomic_t disable_deferred_cache;
	unsigned long flags;
	size_t size;		/* padded (as-allocated) size */
	size_t orig_size;	/* original (as-requested) size */
	size_t align;
	u8 kind;                /* memory kind (0=pitch, !0 -> blocklinear) */
	void *map_resources;    /* mapping resources associated with the
				   buffer */
	struct nvmap_client *owner;
	struct nvmap_handle_ref *owner_ref; /* use this ref to avoid spending
			time on validation in some cases.
			if handle was duplicated by other client and
			original client destroy ref, this field
			has to be set to zero. In this case ref should be
			obtained through validation */

	/*
	 * dma_buf necessities. An attachment is made on dma_buf allocation to
	 * facilitate the nvmap_pin* APIs.
	 */
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;

	struct nvmap_device *dev;
	union {
		struct nvmap_pgalloc pgalloc;
		struct nvmap_heap_block *carveout;
	};
	bool global;		/* handle may be duplicated by other clients */
	bool secure;		/* zap IOVMM area on unpin */
	bool heap_pgalloc;	/* handle is page allocated (sysmem / iovmm) */
	bool alloc;		/* handle has memory allocated */
	unsigned int userflags;	/* flags passed from userspace */
	struct mutex lock;
	void *nvhost_priv;	/* nvhost private data */
	void (*nvhost_priv_delete)(void *priv);
};

/* handle_ref objects are client-local references to an nvmap_handle;
 * they are distinct objects so that handles can be unpinned and
 * unreferenced the correct number of times when a client abnormally
 * terminates */
struct nvmap_handle_ref {
	struct nvmap_handle *handle;
	struct rb_node	node;
	atomic_t	dupes;	/* number of times to free on file close */
	atomic_t	pin;	/* number of times to unpin on free */
};

#ifdef CONFIG_NVMAP_PAGE_POOLS
#define NVMAP_UC_POOL NVMAP_HANDLE_UNCACHEABLE
#define NVMAP_WC_POOL NVMAP_HANDLE_WRITE_COMBINE
#define NVMAP_IWB_POOL NVMAP_HANDLE_INNER_CACHEABLE
#define NVMAP_WB_POOL NVMAP_HANDLE_CACHEABLE
#define NVMAP_NUM_POOLS (NVMAP_HANDLE_CACHEABLE + 1)

struct nvmap_page_pool {
	struct mutex lock;
	int npages;
	struct page **page_array;
	struct page **shrink_array;
	int max_pages;
	int flags;
};

int nvmap_page_pool_init(struct nvmap_page_pool *pool, int flags);
struct page *nvmap_page_pool_alloc(struct nvmap_page_pool *pool);
bool nvmap_page_pool_release(struct nvmap_page_pool *pool, struct page *page);
#endif

struct nvmap_share {
	struct tegra_iovmm_client *iovmm;
	wait_queue_head_t pin_wait;
	struct mutex pin_lock;
#ifdef CONFIG_NVMAP_PAGE_POOLS
	union {
		struct nvmap_page_pool pools[NVMAP_NUM_POOLS];
		struct {
			struct nvmap_page_pool uc_pool;
			struct nvmap_page_pool wc_pool;
			struct nvmap_page_pool iwb_pool;
			struct nvmap_page_pool wb_pool;
		};
	};
#endif
};

struct nvmap_carveout_commit {
	size_t commit;
	struct list_head list;
};

struct nvmap_client {
	const char			*name;
	struct rb_root			handle_refs;
	atomic_t			iovm_commit;
	struct mutex			ref_lock;
	bool				super;
	bool				kernel_client;
	atomic_t			count;
	struct task_struct		*task;
	struct list_head		list;
	u32				handle_count;
	struct nvmap_carveout_commit	carveout_commit[0];
};

struct nvmap_vma_priv {
	struct nvmap_handle *handle;
	size_t		offs;
	atomic_t	count;	/* number of processes cloning the VMA */
};

#include <linux/mm.h>
#include <linux/miscdevice.h>

struct nvmap_device {
	struct vm_struct *vm_rgn;
	pte_t		*ptes[NVMAP_NUM_PTES];
	unsigned long	ptebits[NVMAP_NUM_PTES / BITS_PER_LONG];
	unsigned int	lastpte;
	spinlock_t	ptelock;

	struct rb_root	handles;
	spinlock_t	handle_lock;
	wait_queue_head_t pte_wait;
	struct miscdevice dev_super;
	struct miscdevice dev_user;
	struct nvmap_carveout_node *heaps;
	int nr_carveouts;
	struct nvmap_share iovmm_master;
	struct list_head clients;
	spinlock_t	clients_lock;
	struct nvmap_deferred_ops deferred_ops;
};

static inline void nvmap_ref_lock(struct nvmap_client *priv)
{
	mutex_lock(&priv->ref_lock);
}

static inline void nvmap_ref_unlock(struct nvmap_client *priv)
{
	mutex_unlock(&priv->ref_lock);
}

/*
 * NOTE: this does not ensure the continued existence of the underlying
 * dma_buf. If you want ensure the existence of the dma_buf you must get an
 * nvmap_handle_ref as that is what tracks the dma_buf refs.
 */
static inline struct nvmap_handle *nvmap_handle_get(struct nvmap_handle *h)
{
	if (unlikely(atomic_inc_return(&h->ref) <= 1)) {
		pr_err("%s: %s attempt to get a freed handle\n",
			__func__, current->group_leader->comm);
		atomic_dec(&h->ref);
		return NULL;
	}
	return h;
}

static inline pgprot_t nvmap_pgprot(struct nvmap_handle *h, pgprot_t prot)
{
	if (h->flags == NVMAP_HANDLE_UNCACHEABLE)
		return pgprot_noncached(prot);
	else if (h->flags == NVMAP_HANDLE_WRITE_COMBINE)
		return pgprot_writecombine(prot);
#ifndef CONFIG_ARM_LPAE /* !!!FIXME!!! BUG 892578 */
	else if (h->flags == NVMAP_HANDLE_INNER_CACHEABLE)
		return pgprot_inner_writeback(prot);
#endif
	return prot;
}

#else /* CONFIG_TEGRA_NVMAP */
struct nvmap_handle *nvmap_handle_get(struct nvmap_handle *h);
void nvmap_handle_put(struct nvmap_handle *h);
pgprot_t nvmap_pgprot(struct nvmap_handle *h, pgprot_t prot);

#endif /* !CONFIG_TEGRA_NVMAP */

struct device *nvmap_client_to_device(struct nvmap_client *client);

pte_t **nvmap_alloc_pte(struct nvmap_device *dev, void **vaddr);

pte_t **nvmap_alloc_pte_irq(struct nvmap_device *dev, void **vaddr);

void nvmap_free_pte(struct nvmap_device *dev, pte_t **pte);

pte_t **nvmap_vaddr_to_pte(struct nvmap_device *dev, unsigned long vaddr);

struct nvmap_heap_block *nvmap_carveout_alloc(struct nvmap_client *dev,
					      struct nvmap_handle *handle,
					      unsigned long type);

unsigned long nvmap_carveout_usage(struct nvmap_client *c,
				   struct nvmap_heap_block *b);

struct nvmap_carveout_node;
void nvmap_carveout_commit_add(struct nvmap_client *client,
			       struct nvmap_carveout_node *node, size_t len);

void nvmap_carveout_commit_subtract(struct nvmap_client *client,
				    struct nvmap_carveout_node *node,
				    size_t len);

struct nvmap_share *nvmap_get_share_from_dev(struct nvmap_device *dev);

void nvmap_cache_maint_ops_flush(struct nvmap_device *dev,
		struct nvmap_handle *h);

struct nvmap_deferred_ops *nvmap_get_deferred_ops_from_dev(
		struct nvmap_device *dev);

int nvmap_find_cache_maint_op(struct nvmap_device *dev,
		struct nvmap_handle *h);

struct nvmap_handle *nvmap_validate_get(struct nvmap_client *client,
					unsigned long handle, bool skip_val);

struct nvmap_handle *nvmap_get_handle_id(struct nvmap_client *client,
					 unsigned long id);

void nvmap_handle_put(struct nvmap_handle *h);

struct nvmap_handle_ref *__nvmap_validate_id_locked(struct nvmap_client *priv,
						   unsigned long id);

struct nvmap_handle_ref *nvmap_create_handle(struct nvmap_client *client,
					     size_t size);

struct nvmap_handle_ref *nvmap_duplicate_handle_id(struct nvmap_client *client,
					unsigned long id, bool skip_val);

struct nvmap_handle_ref *nvmap_create_handle_from_fd(
			struct nvmap_client *client, int fd);

int nvmap_alloc_handle_id(struct nvmap_client *client,
			  unsigned long id, unsigned int heap_mask,
			  size_t align, u8 kind,
			  unsigned int flags);

void nvmap_free_handle_id(struct nvmap_client *c, unsigned long id);

void nvmap_free_handle_user_id(struct nvmap_client *c, unsigned long user_id);

int nvmap_pin_ids(struct nvmap_client *client,
		  unsigned int nr, const unsigned long *ids);

void nvmap_unpin_ids(struct nvmap_client *priv,
		     unsigned int nr, const unsigned long *ids);

int nvmap_handle_remove(struct nvmap_device *dev, struct nvmap_handle *h);

void nvmap_handle_add(struct nvmap_device *dev, struct nvmap_handle *h);

int is_nvmap_vma(struct vm_area_struct *vma);

int nvmap_get_dmabuf_fd(struct nvmap_client *client, ulong id);
ulong nvmap_get_id_from_dmabuf_fd(struct nvmap_client *client, int fd);

int nvmap_get_handle_param(struct nvmap_client *client,
		struct nvmap_handle_ref *ref, u32 param, u64 *result);

#ifdef CONFIG_COMPAT
ulong unmarshal_user_handle(__u32 handle);
__u32 marshal_kernel_handle(ulong handle);
ulong unmarshal_user_id(u32 id);
#else
ulong unmarshal_user_handle(struct nvmap_handle *handle);
struct nvmap_handle *marshal_kernel_handle(ulong handle);
ulong unmarshal_user_id(ulong id);
#endif

static inline void nvmap_flush_tlb_kernel_page(unsigned long kaddr)
{
#ifdef CONFIG_ARM_ERRATA_798181
	flush_tlb_kernel_page_skip_errata_798181(kaddr);
#else
	flush_tlb_kernel_page(kaddr);
#endif
}

/* MM definitions. */
extern size_t cache_maint_outer_threshold;
extern int inner_cache_maint_threshold;

extern void v7_flush_kern_cache_all(void);
extern void v7_clean_kern_cache_all(void *);
extern void __flush_dcache_page(struct address_space *, struct page *);

void inner_flush_cache_all(void);
void inner_clean_cache_all(void);
int nvmap_set_pages_array_uc(struct page **pages, int addrinarray);
int nvmap_set_pages_array_wc(struct page **pages, int addrinarray);
int nvmap_set_pages_array_iwb(struct page **pages, int addrinarray);
int nvmap_set_pages_array_wb(struct page **pages, int addrinarray);

/* Internal API to support dmabuf */
struct dma_buf *__nvmap_dmabuf_export(struct nvmap_client *client,
				 unsigned long id);
struct dma_buf *__nvmap_make_dmabuf(struct nvmap_client *client,
				    struct nvmap_handle *handle);
struct sg_table *__nvmap_sg_table(struct nvmap_client *client,
				  struct nvmap_handle *h);
void __nvmap_free_sg_table(struct nvmap_client *client,
			   struct nvmap_handle *h, struct sg_table *sgt);
void *__nvmap_kmap(struct nvmap_handle *h, unsigned int pagenum);
void __nvmap_kunmap(struct nvmap_handle *h, unsigned int pagenum, void *addr);
void *__nvmap_mmap(struct nvmap_handle *h);
void __nvmap_munmap(struct nvmap_handle *h, void *addr);
int __nvmap_map(struct nvmap_handle *h, struct vm_area_struct *vma);
int __nvmap_get_handle_param(struct nvmap_client *client,
			     struct nvmap_handle *h, u32 param, u64 *result);
int __nvmap_cache_maint(struct nvmap_client *client, struct nvmap_handle *h,
			unsigned long start, unsigned long end,
			unsigned int op, unsigned int allow_deferred);
struct nvmap_client *__nvmap_create_client(struct nvmap_device *dev,
					   const char *name);
struct dma_buf *__nvmap_dmabuf_export_from_ref(struct nvmap_handle_ref *ref);
ulong __nvmap_ref_to_id(struct nvmap_handle_ref *ref);
int __nvmap_pin(struct nvmap_handle_ref *ref, phys_addr_t *phys);
void __nvmap_unpin(struct nvmap_handle_ref *ref);
int __nvmap_dmabuf_fd(struct dma_buf *dmabuf, int flags);

void nvmap_dmabuf_debugfs_init(struct dentry *nvmap_root);
int nvmap_dmabuf_stash_init(void);

#endif /* __VIDEO_TEGRA_NVMAP_NVMAP_H */
