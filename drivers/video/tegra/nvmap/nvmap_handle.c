/*
 * drivers/video/tegra/nvmap/nvmap_handle.c
 *
 * Handle allocation and freeing routines for nvmap
 *
 * Copyright (c) 2009-2014, NVIDIA CORPORATION. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/dma-buf.h>
#include <linux/nvmap.h>
#include <linux/tegra-soc.h>

#include <asm/cacheflush.h>
#include <asm/outercache.h>
#include <asm/tlbflush.h>
#include <asm/pgtable.h>

#include <trace/events/nvmap.h>

#include "nvmap_priv.h"
#include "nvmap_ioctl.h"

u32 nvmap_max_handle_count;

#define NVMAP_SECURE_HEAPS	(NVMAP_HEAP_CARVEOUT_IRAM | NVMAP_HEAP_IOVMM | \
				 NVMAP_HEAP_CARVEOUT_VPR)

/* handles may be arbitrarily large (16+MiB), and any handle allocated from
 * the kernel (i.e., not a carveout handle) includes its array of pages. to
 * preserve kmalloc space, if the array of pages exceeds PAGELIST_VMALLOC_MIN,
 * the array is allocated using vmalloc. */
#define PAGELIST_VMALLOC_MIN	(PAGE_SIZE)

static inline void *altalloc(size_t len)
{
	if (len > PAGELIST_VMALLOC_MIN)
		return vmalloc(len);
	else
		return kmalloc(len, GFP_KERNEL);
}

static inline void altfree(void *ptr, size_t len)
{
	if (!ptr)
		return;

	if (len > PAGELIST_VMALLOC_MIN)
		vfree(ptr);
	else
		kfree(ptr);
}

void _nvmap_handle_free(struct nvmap_handle *h)
{
	int err;
	struct nvmap_share *share = nvmap_get_share_from_dev(h->dev);
	unsigned int i, nr_page, page_index = 0;
#ifdef CONFIG_NVMAP_PAGE_POOLS
	struct nvmap_page_pool *pool = NULL;
#endif

	if (h->nvhost_priv)
		h->nvhost_priv_delete(h->nvhost_priv);

	if (nvmap_handle_remove(h->dev, h) != 0)
		return;

	if (!h->alloc)
		goto out;

	if (!h->heap_pgalloc) {
		nvmap_heap_free(h->carveout);
		goto out;
	}

	nr_page = DIV_ROUND_UP(h->size, PAGE_SIZE);

	BUG_ON(h->size & ~PAGE_MASK);
	BUG_ON(!h->pgalloc.pages);

#ifdef CONFIG_NVMAP_PAGE_POOLS
	if (h->flags < NVMAP_NUM_POOLS)
		pool = &share->pools[h->flags];

	while (page_index < nr_page) {
		if (!nvmap_page_pool_release(pool,
		    h->pgalloc.pages[page_index]))
			break;
		page_index++;
	}
#endif

	if (page_index == nr_page)
		goto skip_attr_restore;

	/* Restore page attributes. */
	if (h->flags == NVMAP_HANDLE_WRITE_COMBINE ||
	    h->flags == NVMAP_HANDLE_UNCACHEABLE ||
	    h->flags == NVMAP_HANDLE_INNER_CACHEABLE) {
		/* This op should never fail. */
		err = nvmap_set_pages_array_wb(&h->pgalloc.pages[page_index],
				nr_page - page_index);
		BUG_ON(err);
	}

skip_attr_restore:
	for (i = page_index; i < nr_page; i++)
		__free_page(h->pgalloc.pages[i]);

	altfree(h->pgalloc.pages, nr_page * sizeof(struct page *));

out:
	kfree(h);
}

static struct page *nvmap_alloc_pages_exact(gfp_t gfp, size_t size)
{
	struct page *page, *p, *e;
	unsigned int order;

	size = PAGE_ALIGN(size);
	order = get_order(size);
	page = alloc_pages(gfp, order);

	if (!page)
		return NULL;

	split_page(page, order);
	e = page + (1 << order);
	for (p = page + (size >> PAGE_SHIFT); p < e; p++)
		__free_page(p);

	return page;
}

static int handle_page_alloc(struct nvmap_client *client,
			     struct nvmap_handle *h, bool contiguous)
{
	int err = 0;
	size_t size = PAGE_ALIGN(h->size);
	unsigned int nr_page = size >> PAGE_SHIFT;
	pgprot_t prot;
	unsigned int i = 0, page_index = 0;
	struct page **pages;
#ifdef CONFIG_NVMAP_PAGE_POOLS
	struct nvmap_page_pool *pool = NULL;
	struct nvmap_share *share = nvmap_get_share_from_dev(h->dev);
	phys_addr_t paddr;
#endif
	gfp_t gfp = GFP_NVMAP;
	unsigned long kaddr;
	pte_t **pte = NULL;

	if (h->userflags & NVMAP_HANDLE_ZEROED_PAGES) {
		gfp |= __GFP_ZERO;
		prot = nvmap_pgprot(h, pgprot_kernel);
		pte = nvmap_alloc_pte(nvmap_dev, (void **)&kaddr);
		if (IS_ERR(pte))
			return -ENOMEM;
	}

	pages = altalloc(nr_page * sizeof(*pages));
	if (!pages)
		return -ENOMEM;

	prot = nvmap_pgprot(h, pgprot_kernel);

	if (contiguous) {
		struct page *page;
		page = nvmap_alloc_pages_exact(gfp, size);
		if (!page)
			goto fail;

		for (i = 0; i < nr_page; i++)
			pages[i] = nth_page(page, i);

	} else {
#ifdef CONFIG_NVMAP_PAGE_POOLS
		if (h->flags < NVMAP_NUM_POOLS)
			pool = &share->pools[h->flags];
		else
			BUG();

		for (i = 0; i < nr_page; i++) {
			/* Get pages from pool, if available. */
			pages[i] = nvmap_page_pool_alloc(pool);
			if (!pages[i])
				break;
			if (h->userflags & NVMAP_HANDLE_ZEROED_PAGES) {
				/*
				 * Just memset low mem pages; they will for
				 * sure have a virtual address. Otherwise, build
				 * a mapping for the page in the kernel.
				 */
				if (!PageHighMem(pages[i])) {
					memset(page_address(pages[i]), 0,
					       PAGE_SIZE);
				} else {
					paddr = page_to_phys(pages[i]);
					set_pte_at(&init_mm, kaddr, *pte,
						   pfn_pte(__phys_to_pfn(paddr),
							   prot));
					nvmap_flush_tlb_kernel_page(kaddr);
					memset((char *)kaddr, 0, PAGE_SIZE);
				}
			}
			page_index++;
		}
#endif
		for (; i < nr_page; i++) {
			pages[i] = nvmap_alloc_pages_exact(gfp,	PAGE_SIZE);
			if (!pages[i])
				goto fail;
		}
	}

	if (nr_page == page_index)
		goto skip_attr_change;

	/* Update the pages mapping in kernel page table. */
	if (h->flags == NVMAP_HANDLE_WRITE_COMBINE)
		err = nvmap_set_pages_array_wc(&pages[page_index],
					nr_page - page_index);
	else if (h->flags == NVMAP_HANDLE_UNCACHEABLE)
		err = nvmap_set_pages_array_uc(&pages[page_index],
					nr_page - page_index);
	else if (h->flags == NVMAP_HANDLE_INNER_CACHEABLE)
		err = nvmap_set_pages_array_iwb(&pages[page_index],
					nr_page - page_index);

	if (err)
		goto fail;

skip_attr_change:
	if (h->userflags & NVMAP_HANDLE_ZEROED_PAGES)
		nvmap_free_pte(nvmap_dev, pte);
	h->size = size;
	h->pgalloc.pages = pages;
	h->pgalloc.contig = contiguous;
	return 0;

fail:
	if (h->userflags & NVMAP_HANDLE_ZEROED_PAGES)
		nvmap_free_pte(nvmap_dev, pte);
	if (i) {
		err = nvmap_set_pages_array_wb(pages, i);
		BUG_ON(err);
	}
	while (i--)
		__free_page(pages[i]);
	altfree(pages, nr_page * sizeof(*pages));
	wmb();
	return -ENOMEM;
}

static void alloc_handle(struct nvmap_client *client,
			 struct nvmap_handle *h, unsigned int type)
{
	unsigned int carveout_mask = NVMAP_HEAP_CARVEOUT_MASK;
	unsigned int iovmm_mask = NVMAP_HEAP_IOVMM;

	BUG_ON(type & (type - 1));

#ifdef CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM
	/* Convert generic carveout requests to iovmm requests. */
	carveout_mask &= ~NVMAP_HEAP_CARVEOUT_GENERIC;
	iovmm_mask |= NVMAP_HEAP_CARVEOUT_GENERIC;
#endif

	if (type & carveout_mask) {
		struct nvmap_heap_block *b;

		b = nvmap_carveout_alloc(client, h, type);
		if (b) {
			h->heap_pgalloc = false;
			/* barrier to ensure all handle alloc data
			 * is visible before alloc is seen by other
			 * processors.
			 */
			mb();
			h->alloc = true;
			nvmap_carveout_commit_add(client,
				nvmap_heap_to_arg(nvmap_block_to_heap(b)),
				h->size);
		}
	} else if (type & iovmm_mask) {
		int ret;
		size_t reserved = PAGE_ALIGN(h->size);

		atomic_add(reserved, &client->iovm_commit);
		ret = handle_page_alloc(client, h, false);
		if (ret) {
			atomic_sub(reserved, &client->iovm_commit);
			return;
		}
		h->heap_pgalloc = true;
		mb();
		h->alloc = true;
	}
}

/* small allocations will try to allocate from generic OS memory before
 * any of the limited heaps, to increase the effective memory for graphics
 * allocations, and to reduce fragmentation of the graphics heaps with
 * sub-page splinters */
static const unsigned int heap_policy_small[] = {
	NVMAP_HEAP_CARVEOUT_VPR,
	NVMAP_HEAP_CARVEOUT_IRAM,
	NVMAP_HEAP_CARVEOUT_MASK,
	NVMAP_HEAP_IOVMM,
	0,
};

static const unsigned int heap_policy_large[] = {
	NVMAP_HEAP_CARVEOUT_VPR,
	NVMAP_HEAP_CARVEOUT_IRAM,
	NVMAP_HEAP_IOVMM,
	NVMAP_HEAP_CARVEOUT_MASK,
	0,
};

int nvmap_alloc_handle_id(struct nvmap_client *client,
			  unsigned long id, unsigned int heap_mask,
			  size_t align,
			  u8 kind,
			  unsigned int flags)
{
	struct nvmap_handle *h = NULL;
	const unsigned int *alloc_policy;
	int nr_page;
	int err = -ENOMEM;

	h = nvmap_get_handle_id(client, id);

	if (!h)
		return -EINVAL;

	if (h->alloc) {
		nvmap_handle_put(h);
		return -EEXIST;
	}

	trace_nvmap_alloc_handle_id(client, id, heap_mask, align, flags);
	h->userflags = flags;
	nr_page = ((h->size + PAGE_SIZE - 1) >> PAGE_SHIFT);
	h->secure = !!(flags & NVMAP_HANDLE_SECURE);
	h->flags = (flags & NVMAP_HANDLE_CACHE_FLAG);
	h->align = max_t(size_t, align, L1_CACHE_BYTES);
	h->kind = kind;
	h->map_resources = 0;

#ifndef CONFIG_TEGRA_IOVMM
	/* convert iovmm requests to generic carveout. */
	if (heap_mask & NVMAP_HEAP_IOVMM) {
		heap_mask = (heap_mask & ~NVMAP_HEAP_IOVMM) |
			    NVMAP_HEAP_CARVEOUT_GENERIC;
	}
#endif
	/* secure allocations can only be served from secure heaps */
	if (h->secure)
		heap_mask &= NVMAP_SECURE_HEAPS;

	if (!heap_mask) {
		err = -EINVAL;
		goto out;
	}

	alloc_policy = (nr_page == 1) ? heap_policy_small : heap_policy_large;

	while (!h->alloc && *alloc_policy) {
		unsigned int heap_type;

		heap_type = *alloc_policy++;
		heap_type &= heap_mask;

		if (!heap_type)
			continue;

		heap_mask &= ~heap_type;

		while (heap_type && !h->alloc) {
			unsigned int heap;

			/* iterate possible heaps MSB-to-LSB, since higher-
			 * priority carveouts will have higher usage masks */
			heap = 1 << __fls(heap_type);
			alloc_handle(client, h, heap);
			heap_type &= ~heap;
		}
	}

out:
	err = (h->alloc) ? 0 : err;
	nvmap_handle_put(h);
	return err;
}

void nvmap_free_handle_id(struct nvmap_client *client, unsigned long id)
{
	struct nvmap_handle_ref *ref;
	struct nvmap_handle *h;
	int pins;

	nvmap_ref_lock(client);

	ref = __nvmap_validate_id_locked(client, id);
	if (!ref) {
		nvmap_ref_unlock(client);
		return;
	}

	trace_nvmap_free_handle_id(client, id);
	BUG_ON(!ref->handle);
	h = ref->handle;

	if (atomic_dec_return(&ref->dupes)) {
		nvmap_ref_unlock(client);
		goto out;
	}

	smp_rmb();
	pins = atomic_read(&ref->pin);
	rb_erase(&ref->node, &client->handle_refs);
	client->handle_count--;

	if (h->alloc && h->heap_pgalloc && !h->pgalloc.contig)
		atomic_sub_return(h->size, &client->iovm_commit);

	if (h->alloc && !h->heap_pgalloc) {
		mutex_lock(&h->lock);
		nvmap_carveout_commit_subtract(client,
			nvmap_heap_to_arg(nvmap_block_to_heap(h->carveout)),
			h->size);
		mutex_unlock(&h->lock);
	}

	nvmap_ref_unlock(client);

	if (pins)
		nvmap_debug(client, "%s freeing pinned handle %p\n",
			    current->group_leader->comm, h);

	while (atomic_read(&ref->pin))
		__nvmap_unpin(ref);

	if (h->owner == client) {
		h->owner = NULL;
		h->owner_ref = NULL;
	}

	dma_buf_put(ref->handle->dmabuf);
	kfree(ref);

out:
	BUG_ON(!atomic_read(&h->ref));
	if (nvmap_find_cache_maint_op(h->dev, h))
		nvmap_cache_maint_ops_flush(h->dev, h);
	nvmap_handle_put(h);
}
EXPORT_SYMBOL(nvmap_free_handle_id);

void nvmap_free_handle_user_id(struct nvmap_client *client,
			       unsigned long user_id)
{
	nvmap_free_handle_id(client, unmarshal_user_id(user_id));
}

static void add_handle_ref(struct nvmap_client *client,
			   struct nvmap_handle_ref *ref)
{
	struct rb_node **p, *parent = NULL;

	nvmap_ref_lock(client);
	p = &client->handle_refs.rb_node;
	while (*p) {
		struct nvmap_handle_ref *node;
		parent = *p;
		node = rb_entry(parent, struct nvmap_handle_ref, node);
		if (ref->handle > node->handle)
			p = &parent->rb_right;
		else
			p = &parent->rb_left;
	}
	rb_link_node(&ref->node, parent, p);
	rb_insert_color(&ref->node, &client->handle_refs);
	client->handle_count++;
	if (client->handle_count > nvmap_max_handle_count)
		nvmap_max_handle_count = client->handle_count;
	nvmap_ref_unlock(client);
}

struct nvmap_handle_ref *nvmap_create_handle(struct nvmap_client *client,
					     size_t size)
{
	void *err = ERR_PTR(-ENOMEM);
	struct nvmap_handle *h;
	struct nvmap_handle_ref *ref = NULL;

	if (!client)
		return ERR_PTR(-EINVAL);

	if (!size)
		return ERR_PTR(-EINVAL);

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return ERR_PTR(-ENOMEM);

	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref)
		goto ref_alloc_fail;

	atomic_set(&h->ref, 1);
	atomic_set(&h->pin, 0);
	h->owner = client;
	h->owner_ref = ref;
	h->dev = nvmap_dev;
	BUG_ON(!h->owner);
	h->size = h->orig_size = size;
	h->flags = NVMAP_HANDLE_WRITE_COMBINE;
	mutex_init(&h->lock);

	/*
	 * This takes out 1 ref on the dambuf. This corresponds to the
	 * handle_ref that gets automatically made by nvmap_create_handle().
	 */
	h->dmabuf = __nvmap_make_dmabuf(client, h);
	if (IS_ERR(h->dmabuf)) {
		err = h->dmabuf;
		goto make_dmabuf_fail;
	}

	/*
	 * Pre-attach nvmap to this new dmabuf. This gets unattached during the
	 * dma_buf_release() operation.
	 */
	h->attachment = dma_buf_attach(h->dmabuf, &nvmap_pdev->dev);
	if (IS_ERR(h->attachment)) {
		err = h->attachment;
		goto dma_buf_attach_fail;
	}

	nvmap_handle_add(nvmap_dev, h);

	/*
	 * Major assumption here: the dma_buf object that the handle contains
	 * is created with a ref count of 1.
	 */
	atomic_set(&ref->dupes, 1);
	ref->handle = h;
	atomic_set(&ref->pin, 0);
	add_handle_ref(client, ref);
	trace_nvmap_create_handle(client, client->name, h, size, ref);
	return ref;

dma_buf_attach_fail:
	dma_buf_put(h->dmabuf);
make_dmabuf_fail:
	kfree(ref);
ref_alloc_fail:
	kfree(h);
	return err;
}

struct nvmap_handle_ref *nvmap_duplicate_handle_id(struct nvmap_client *client,
					unsigned long id, bool skip_val)
{
	struct nvmap_handle_ref *ref = NULL;
	struct nvmap_handle *h = NULL;

	BUG_ON(!client);
	/* on success, the reference count for the handle should be
	 * incremented, so the success paths will not call nvmap_handle_put */
	h = nvmap_validate_get(client, id, skip_val);

	if (!h) {
		nvmap_debug(client, "%s duplicate handle failed\n",
			    current->group_leader->comm);
		return ERR_PTR(-EPERM);
	}

	if (!h->alloc) {
		nvmap_err(client, "%s duplicating unallocated handle\n",
			  current->group_leader->comm);
		nvmap_handle_put(h);
		return ERR_PTR(-EINVAL);
	}

	nvmap_ref_lock(client);
	ref = __nvmap_validate_id_locked(client, (unsigned long)h);

	if (ref) {
		/* handle already duplicated in client; just increment
		 * the reference count rather than re-duplicating it */
		atomic_inc(&ref->dupes);
		nvmap_ref_unlock(client);
		return ref;
	}

	nvmap_ref_unlock(client);

	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref) {
		nvmap_handle_put(h);
		return ERR_PTR(-ENOMEM);
	}

	if (!h->heap_pgalloc) {
		mutex_lock(&h->lock);
		nvmap_carveout_commit_add(client,
			nvmap_heap_to_arg(nvmap_block_to_heap(h->carveout)),
			h->size);
		mutex_unlock(&h->lock);
	} else if (!h->pgalloc.contig) {
		atomic_add(h->size, &client->iovm_commit);
	}

	atomic_set(&ref->dupes, 1);
	ref->handle = h;
	atomic_set(&ref->pin, 0);
	add_handle_ref(client, ref);

	/*
	 * Ref counting on the dma_bufs follows the creation and destruction of
	 * nvmap_handle_refs. That is every time a handle_ref is made the
	 * dma_buf ref count goes up and everytime a handle_ref is destroyed
	 * the dma_buf ref count goes down.
	 */
	get_dma_buf(h->dmabuf);

	trace_nvmap_duplicate_handle_id(client, id, ref);
	return ref;
}

struct nvmap_handle_ref *nvmap_create_handle_from_fd(
			struct nvmap_client *client, int fd)
{
	unsigned long id;
	struct nvmap_handle_ref *ref;

	BUG_ON(!client);

	id = nvmap_get_id_from_dmabuf_fd(client, fd);
	if (IS_ERR_VALUE(id))
		return ERR_PTR(id);
	ref = nvmap_duplicate_handle_id(client, id, 1);
	return ref;
}

unsigned long nvmap_duplicate_handle_id_ex(struct nvmap_client *client,
						unsigned long id)
{
	struct nvmap_handle_ref *ref = nvmap_duplicate_handle_id(client, id, 0);

	if (IS_ERR(ref))
		return 0;

	return __nvmap_ref_to_id(ref);
}
EXPORT_SYMBOL(nvmap_duplicate_handle_id_ex);

int nvmap_get_page_list_info(struct nvmap_client *client,
				unsigned long id, u32 *size, u32 *flags,
				u32 *nr_page, bool *contig)
{
	struct nvmap_handle *h;

	BUG_ON(!size || !flags || !nr_page || !contig);
	BUG_ON(!client);

	*size = 0;
	*flags = 0;
	*nr_page = 0;

	h = nvmap_validate_get(client, id, 0);

	if (!h) {
		nvmap_err(client, "%s query invalid handle %p\n",
			  current->group_leader->comm, (void *)id);
		return -EINVAL;
	}

	if (!h->alloc || !h->heap_pgalloc) {
		nvmap_err(client, "%s query unallocated handle %p\n",
			  current->group_leader->comm, (void *)id);
		nvmap_handle_put(h);
		return -EINVAL;
	}

	*flags = h->flags;
	*size = h->orig_size;
	*nr_page = PAGE_ALIGN(h->size) >> PAGE_SHIFT;
	*contig = h->pgalloc.contig;

	nvmap_handle_put(h);
	return 0;
}
EXPORT_SYMBOL(nvmap_get_page_list_info);

int nvmap_acquire_page_list(struct nvmap_client *client,
			unsigned long id, struct page **pages, u32 nr_page)
{
	struct nvmap_handle *h;
	struct nvmap_handle_ref *ref;
	int idx;
	phys_addr_t dummy;

	BUG_ON(!client);

	h = nvmap_validate_get(client, id, 0);

	if (!h) {
		nvmap_err(client, "%s query invalid handle %p\n",
			  current->group_leader->comm, (void *)id);
		return -EINVAL;
	}

	if (!h->alloc || !h->heap_pgalloc) {
		nvmap_err(client, "%s query unallocated handle %p\n",
			  current->group_leader->comm, (void *)id);
		nvmap_handle_put(h);
		return -EINVAL;
	}

	BUG_ON(nr_page != PAGE_ALIGN(h->size) >> PAGE_SHIFT);

	for (idx = 0; idx < nr_page; idx++)
		pages[idx] = h->pgalloc.pages[idx];

	nvmap_ref_lock(client);
	ref = __nvmap_validate_id_locked(client, id);
	if (ref)
		__nvmap_pin(ref, &dummy);
	nvmap_ref_unlock(client);

	return 0;
}
EXPORT_SYMBOL(nvmap_acquire_page_list);

int nvmap_release_page_list(struct nvmap_client *client, unsigned long id)
{
	struct nvmap_handle_ref *ref;
	struct nvmap_handle *h = NULL;

	BUG_ON(!client);

	nvmap_ref_lock(client);

	ref = __nvmap_validate_id_locked(client, id);
	if (ref)
		__nvmap_unpin(ref);

	nvmap_ref_unlock(client);

	if (ref)
		h = ref->handle;
	if (h)
		nvmap_handle_put(h);

	return 0;
}
EXPORT_SYMBOL(nvmap_release_page_list);

int __nvmap_get_handle_param(struct nvmap_client *client,
			     struct nvmap_handle *h, u32 param, u64 *result)
{
	int err = 0;

	if (WARN_ON(!virt_addr_valid(h)))
		return -EINVAL;

	switch (param) {
	case NVMAP_HANDLE_PARAM_SIZE:
		*result = h->orig_size;
		break;
	case NVMAP_HANDLE_PARAM_ALIGNMENT:
		*result = h->align;
		break;
	case NVMAP_HANDLE_PARAM_BASE:
		if (!h->alloc || !atomic_read(&h->pin))
			*result = -EINVAL;
		else if (!h->heap_pgalloc) {
			mutex_lock(&h->lock);
			*result = h->carveout->base;
			mutex_unlock(&h->lock);
		} else if (h->pgalloc.contig)
			*result = page_to_phys(h->pgalloc.pages[0]);
		else if (h->attachment->priv)
			*result = sg_dma_address(
				((struct sg_table *)h->attachment->priv)->sgl);
		else
			*result = -EINVAL;
		break;
	case NVMAP_HANDLE_PARAM_HEAP:
		if (!h->alloc)
			*result = 0;
		else if (!h->heap_pgalloc) {
			mutex_lock(&h->lock);
			*result = nvmap_carveout_usage(client, h->carveout);
			mutex_unlock(&h->lock);
		} else
			*result = NVMAP_HEAP_IOVMM;
		break;
	case NVMAP_HANDLE_PARAM_KIND:
		*result = h->kind;
		break;
	case NVMAP_HANDLE_PARAM_COMPR:
		/* ignored, to be removed */
		break;
	default:
		err = -EINVAL;
		break;
	}
	return err;
}

int nvmap_get_handle_param(struct nvmap_client *client,
			   struct nvmap_handle_ref *ref, u32 param, u64 *result)
{
	if (WARN_ON(!virt_addr_valid(ref)) ||
	    WARN_ON(!virt_addr_valid(client)) ||
	    WARN_ON(!result))
		return -EINVAL;

	return __nvmap_get_handle_param(client, ref->handle, param, result);
}
