/*
 * drivers/video/tegra/nvmap/nvmap_ioctl.c
 *
 * User-space interface to nvmap
 *
 * Copyright (c) 2011-2012, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/nvmap.h>

#include <asm/cacheflush.h>
#include <asm/outercache.h>
#include <asm/tlbflush.h>

#include <mach/iovmm.h>
#include <trace/events/nvmap.h>
#include <linux/vmalloc.h>

#include "nvmap_ioctl.h"
#include "nvmap.h"
#include "nvmap_common.h"

#include <linux/list.h>

#define CACHE_MAINT_IMMEDIATE		0
#define CACHE_MAINT_ALLOW_DEFERRED	1

#define FLUSH_ALL_HANDLES		0

static ssize_t rw_handle(struct nvmap_client *client, struct nvmap_handle *h,
			 int is_read, unsigned long h_offs,
			 unsigned long sys_addr, unsigned long h_stride,
			 unsigned long sys_stride, unsigned long elem_size,
			 unsigned long count);

static int cache_maint(struct nvmap_client *client, struct nvmap_handle *h,
		       unsigned long start, unsigned long end, unsigned int op,
		       unsigned int allow_deferred);

int nvmap_ioctl_pinop(struct file *filp, bool is_pin, void __user *arg)
{
	struct nvmap_pin_handle op;
	struct nvmap_handle *h;
	unsigned long on_stack[16];
	unsigned long *refs;
	unsigned long __user *output;
	unsigned int i;
	int err = 0;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!op.count)
		return -EINVAL;

	if (op.count > 1) {
		size_t bytes = op.count * sizeof(*refs); /* kcalloc below will catch overflow. */

		if (op.count > ARRAY_SIZE(on_stack))
			refs = kcalloc(op.count, sizeof(*refs), GFP_KERNEL);
		else
			refs = on_stack;

		if (!refs)
			return -ENOMEM;

		if (copy_from_user(refs, (void *)op.handles, bytes)) {
			err = -EFAULT;
			goto out;
		}
	} else {
		refs = on_stack;
		on_stack[0] = (unsigned long)op.handles;
	}

	trace_nvmap_ioctl_pinop(filp->private_data, is_pin, op.count, refs);
	if (is_pin)
		err = nvmap_pin_ids(filp->private_data, op.count, refs);
	else
		nvmap_unpin_ids(filp->private_data, op.count, refs);

	/* skip the output stage on unpin */
	if (err || !is_pin)
		goto out;

	/* it is guaranteed that if nvmap_pin_ids returns 0 that
	 * all of the handle_ref objects are valid, so dereferencing
	 * directly here is safe */
	if (op.count > 1)
		output = (unsigned long __user *)op.addr;
	else {
		struct nvmap_pin_handle __user *tmp = arg;
		output = (unsigned long __user *)&(tmp->addr);
	}

	if (!output)
		goto out;

	for (i = 0; i < op.count && !err; i++) {
		unsigned long addr;

		h = (struct nvmap_handle *)refs[i];

		if (h->heap_pgalloc && h->pgalloc.contig)
			addr = page_to_phys(h->pgalloc.pages[0]);
		else if (h->heap_pgalloc)
			addr = h->pgalloc.area->iovm_start;
		else
			addr = h->carveout->base;

		err = put_user(addr, &output[i]);
	}

	if (err)
		nvmap_unpin_ids(filp->private_data, op.count, refs);

out:
	if (refs != on_stack)
		kfree(refs);

	return err;
}

int nvmap_ioctl_getid(struct file *filp, void __user *arg)
{
	struct nvmap_client *client = filp->private_data;
	struct nvmap_create_handle op;
	struct nvmap_handle *h = NULL;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!op.handle)
		return -EINVAL;

	h = nvmap_get_handle_id(client, op.handle);

	if (!h)
		return -EPERM;

	op.id = (__u32)h;
	if (client == h->owner)
		h->global = true;

	nvmap_handle_put(h);

	return copy_to_user(arg, &op, sizeof(op)) ? -EFAULT : 0;
}

int nvmap_ioctl_alloc(struct file *filp, void __user *arg)
{
	struct nvmap_alloc_handle op;
	struct nvmap_client *client = filp->private_data;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!op.handle)
		return -EINVAL;

	if (op.align & (op.align - 1))
		return -EINVAL;

	/* user-space handles are aligned to page boundaries, to prevent
	 * data leakage. */
	op.align = max_t(size_t, op.align, PAGE_SIZE);
#if defined(CONFIG_NVMAP_FORCE_ZEROED_USER_PAGES)
	op.flags |= NVMAP_HANDLE_ZEROED_PAGES;
#endif

	return nvmap_alloc_handle_id(client, op.handle, op.heap_mask,
				     op.align, op.flags);
}

int nvmap_ioctl_create(struct file *filp, unsigned int cmd, void __user *arg)
{
	struct nvmap_create_handle op;
	struct nvmap_handle_ref *ref = NULL;
	struct nvmap_client *client = filp->private_data;
	int err = 0;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!client)
		return -ENODEV;

	if (cmd == NVMAP_IOC_CREATE) {
		ref = nvmap_create_handle(client, PAGE_ALIGN(op.size));
		if (!IS_ERR(ref))
			ref->handle->orig_size = op.size;
	} else if (cmd == NVMAP_IOC_FROM_ID) {
		ref = nvmap_duplicate_handle_id(client, op.id);
	} else {
		return -EINVAL;
	}

	if (IS_ERR(ref))
		return PTR_ERR(ref);

	op.handle = nvmap_ref_to_id(ref);
	if (copy_to_user(arg, &op, sizeof(op))) {
		err = -EFAULT;
		nvmap_free_handle_id(client, op.handle);
	}

	return err;
}

int nvmap_map_into_caller_ptr(struct file *filp, void __user *arg)
{
	struct nvmap_client *client = filp->private_data;
	struct nvmap_map_caller op;
	struct nvmap_vma_priv *vpriv;
	struct vm_area_struct *vma;
	struct nvmap_handle *h = NULL;
	unsigned int cache_flags;
	int err = 0;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!op.handle)
		return -EINVAL;

	h = nvmap_get_handle_id(client, op.handle);

	if (!h)
		return -EPERM;

	if(!h->alloc) {
		nvmap_handle_put(h);
		return -EFAULT;
	}

	trace_nvmap_map_into_caller_ptr(client, h, op.offset,
					op.length, op.flags);
	down_read(&current->mm->mmap_sem);

	vma = find_vma(current->mm, op.addr);
	if (!vma || !vma->vm_private_data) {
		err = -ENOMEM;
		goto out;
	}

	if (op.offset & ~PAGE_MASK) {
		err = -EFAULT;
		goto out;
	}

	if (op.offset > h->size || (op.offset + op.length) > h->size) {
		err = -EADDRNOTAVAIL;
		goto out;
	}

	vpriv = vma->vm_private_data;
	BUG_ON(!vpriv);

	/* the VMA must exactly match the requested mapping operation, and the
	 * VMA that is targetted must have been created by this driver
	 */
	if ((vma->vm_start != op.addr) || !is_nvmap_vma(vma) ||
	    (vma->vm_end-vma->vm_start != op.length)) {
		err = -EPERM;
		goto out;
	}

	/* verify that each mmap() system call creates a unique VMA */

	if (vpriv->handle && (h == vpriv->handle)) {
		goto out;
	} else if (vpriv->handle) {
		err = -EADDRNOTAVAIL;
		goto out;
	}

	nvmap_usecount_inc(h);

	if (!h->heap_pgalloc && (h->carveout->base & ~PAGE_MASK)) {
		nvmap_usecount_dec(h);
		err = -EFAULT;
		goto out;
	}

	vpriv->handle = h;
	vpriv->offs = op.offset;

	cache_flags = op.flags & NVMAP_HANDLE_CACHE_FLAG;
	if ((cache_flags == NVMAP_HANDLE_INNER_CACHEABLE ||
	     cache_flags == NVMAP_HANDLE_CACHEABLE) &&
	    (h->flags == NVMAP_HANDLE_UNCACHEABLE ||
	     h->flags == NVMAP_HANDLE_WRITE_COMBINE)) {
		if (h->size & ~PAGE_MASK) {
			pr_err("\n%s:attempt to convert a buffer from uc/wc to"
				" wb, whose size is not a multiple of page size."
				" request ignored.\n", __func__);
		} else {
			unsigned int nr_page = h->size >> PAGE_SHIFT;
			wmb();
			/* override allocation time cache coherency attributes. */
			h->flags &= ~NVMAP_HANDLE_CACHE_FLAG;
			h->flags |= cache_flags;

			/* Update page attributes, if the memory is allocated
			 *  from system heap pages.
			 */
			if (cache_flags == NVMAP_HANDLE_INNER_CACHEABLE &&
				h->heap_pgalloc)
				set_pages_array_iwb(h->pgalloc.pages, nr_page);
			else if (h->heap_pgalloc)
				set_pages_array_wb(h->pgalloc.pages, nr_page);
		}
	}
	vma->vm_page_prot = nvmap_pgprot(h, vma->vm_page_prot);

out:
	up_read(&current->mm->mmap_sem);

	if (err)
		nvmap_handle_put(h);
	return err;
}

int nvmap_ioctl_get_param(struct file *filp, void __user* arg)
{
	struct nvmap_handle_param op;
	struct nvmap_client *client = filp->private_data;
	struct nvmap_handle *h;
	int err = 0;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	h = nvmap_get_handle_id(client, op.handle);
	if (!h)
		return -EINVAL;

	switch (op.param) {
	case NVMAP_HANDLE_PARAM_SIZE:
		op.result = h->orig_size;
		break;
	case NVMAP_HANDLE_PARAM_ALIGNMENT:
		mutex_lock(&h->lock);
		if (!h->alloc)
			op.result = 0;
		else if (h->heap_pgalloc)
			op.result = PAGE_SIZE;
		else if (h->carveout->base)
			op.result = (h->carveout->base & -h->carveout->base);
		else
			op.result = SZ_4M;
		mutex_unlock(&h->lock);
		break;
	case NVMAP_HANDLE_PARAM_BASE:
		if (WARN_ON(!h->alloc || !atomic_add_return(0, &h->pin)))
			op.result = -1ul;
		else if (!h->heap_pgalloc) {
			mutex_lock(&h->lock);
			op.result = h->carveout->base;
			mutex_unlock(&h->lock);
		} else if (h->pgalloc.contig)
			op.result = page_to_phys(h->pgalloc.pages[0]);
		else if (h->pgalloc.area)
			op.result = h->pgalloc.area->iovm_start;
		else
			op.result = -1ul;
		break;
	case NVMAP_HANDLE_PARAM_HEAP:
		if (!h->alloc)
			op.result = 0;
		else if (!h->heap_pgalloc) {
			mutex_lock(&h->lock);
			op.result = nvmap_carveout_usage(client, h->carveout);
			mutex_unlock(&h->lock);
		} else if (h->pgalloc.contig)
			op.result = NVMAP_HEAP_SYSMEM;
		else
			op.result = NVMAP_HEAP_IOVMM;
		break;
	default:
		err = -EINVAL;
		break;
	}

	if (!err && copy_to_user(arg, &op, sizeof(op)))
		err = -EFAULT;

	nvmap_handle_put(h);
	return err;
}

int nvmap_ioctl_rw_handle(struct file *filp, int is_read, void __user* arg)
{
	struct nvmap_client *client = filp->private_data;
	struct nvmap_rw_handle __user *uarg = arg;
	struct nvmap_rw_handle op;
	struct nvmap_handle *h;
	ssize_t copied;
	int err = 0;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!op.handle || !op.addr || !op.count || !op.elem_size)
		return -EINVAL;

	h = nvmap_get_handle_id(client, op.handle);
	if (!h)
		return -EPERM;

	nvmap_usecount_inc(h);

	trace_nvmap_ioctl_rw_handle(client, h, is_read, op.offset,
				    op.addr, op.hmem_stride,
				    op.user_stride, op.elem_size, op.count);
	copied = rw_handle(client, h, is_read, op.offset,
			   (unsigned long)op.addr, op.hmem_stride,
			   op.user_stride, op.elem_size, op.count);

	if (copied < 0) {
		err = copied;
		copied = 0;
	} else if (copied < (op.count * op.elem_size))
		err = -EINTR;

	__put_user(copied, &uarg->count);

	nvmap_usecount_dec(h);

	nvmap_handle_put(h);

	return err;
}

int nvmap_ioctl_cache_maint(struct file *filp, void __user *arg)
{
	struct nvmap_client *client = filp->private_data;
	struct nvmap_cache_op op;
	struct vm_area_struct *vma;
	struct nvmap_vma_priv *vpriv;
	unsigned long start;
	unsigned long end;
	int err = 0;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!op.handle || !op.addr || op.op < NVMAP_CACHE_OP_WB ||
	    op.op > NVMAP_CACHE_OP_WB_INV)
		return -EINVAL;

	down_read(&current->mm->mmap_sem);

	vma = find_vma(current->active_mm, (unsigned long)op.addr);
	if (!vma || !is_nvmap_vma(vma) ||
	    (unsigned long)op.addr + op.len > vma->vm_end) {
		err = -EADDRNOTAVAIL;
		goto out;
	}

	vpriv = (struct nvmap_vma_priv *)vma->vm_private_data;

	if ((unsigned long)vpriv->handle != op.handle) {
		err = -EFAULT;
		goto out;
	}

	start = (unsigned long)op.addr - vma->vm_start;
	end = start + op.len;

	err = cache_maint(client, vpriv->handle, start, end, op.op,
		CACHE_MAINT_ALLOW_DEFERRED);
out:
	up_read(&current->mm->mmap_sem);
	return err;
}

int nvmap_ioctl_free(struct file *filp, unsigned long arg)
{
	struct nvmap_client *client = filp->private_data;

	if (!arg)
		return 0;

	nvmap_free_handle_id(client, arg);
	return 0;
}

static void inner_cache_maint(unsigned int op, void *vaddr, size_t size)
{
	if (op == NVMAP_CACHE_OP_WB_INV)
		dmac_flush_range(vaddr, vaddr + size);
	else if (op == NVMAP_CACHE_OP_INV)
		dmac_map_area(vaddr, size, DMA_FROM_DEVICE);
	else
		dmac_map_area(vaddr, size, DMA_TO_DEVICE);
}

static void outer_cache_maint(unsigned int op, phys_addr_t paddr, size_t size)
{
	if (op == NVMAP_CACHE_OP_WB_INV)
		outer_flush_range(paddr, paddr + size);
	else if (op == NVMAP_CACHE_OP_INV)
		outer_inv_range(paddr, paddr + size);
	else
		outer_clean_range(paddr, paddr + size);
}

static void heap_page_cache_maint(
	struct nvmap_handle *h, unsigned long start, unsigned long end,
	unsigned int op, bool inner, bool outer, pte_t **pte,
	unsigned long kaddr, pgprot_t prot)
{
	struct page *page;
	phys_addr_t paddr;
	unsigned long next;
	unsigned long off;
	size_t size;

	while (start < end) {
		page = h->pgalloc.pages[start >> PAGE_SHIFT];
		next = min(((start + PAGE_SIZE) & PAGE_MASK), end);
		off = start & ~PAGE_MASK;
		size = next - start;
		paddr = page_to_phys(page) + off;

		if (inner) {
			void *vaddr = (void *)kaddr + off;
			BUG_ON(!pte);
			BUG_ON(!kaddr);
			set_pte_at(&init_mm, kaddr, *pte,
				pfn_pte(__phys_to_pfn(paddr), prot));
			nvmap_flush_tlb_kernel_page(kaddr);
			inner_cache_maint(op, vaddr, size);
		}

		if (outer)
			outer_cache_maint(op, paddr, size);
		start = next;
	}
}

#if defined(CONFIG_NVMAP_OUTER_CACHE_MAINT_BY_SET_WAYS)
static bool fast_cache_maint_outer(unsigned long start,
		unsigned long end, unsigned int op)
{
	bool result = false;
	if (end - start >= cache_maint_outer_threshold) {
		if (op == NVMAP_CACHE_OP_WB_INV) {
			outer_flush_all();
			result = true;
		}
		if (op == NVMAP_CACHE_OP_WB) {
			outer_clean_all();
			result = true;
		}
	}

	return result;
}
#else
static inline bool fast_cache_maint_outer(unsigned long start,
		unsigned long end, unsigned int op)
{
	return false;
}
#endif

#if defined(CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS)
static bool fast_cache_maint(struct nvmap_handle *h,
	unsigned long start,
	unsigned long end, unsigned int op)
{
	if ((op == NVMAP_CACHE_OP_INV) ||
		((end - start) < cache_maint_inner_threshold))
		return false;

	if (op == NVMAP_CACHE_OP_WB_INV)
		inner_flush_cache_all();
	else if (op == NVMAP_CACHE_OP_WB)
		inner_clean_cache_all();

	/* outer maintenance */
	if (h->flags != NVMAP_HANDLE_INNER_CACHEABLE) {
		if(!fast_cache_maint_outer(start, end, op))
		{
			if (h->heap_pgalloc) {
				heap_page_cache_maint(h, start,
					end, op, false, true, NULL, 0, 0);
			} else  {
				phys_addr_t pstart;

				pstart = start + h->carveout->base;
				outer_cache_maint(op, pstart, end - start);
			}
		}
	}
	return true;
}
#else
static inline bool fast_cache_maint(struct nvmap_handle *h,
				    unsigned long start, unsigned long end,
				    unsigned int op)
{
	return false;
}
#endif

static void debug_count_requested_op(struct nvmap_deferred_ops *deferred_ops,
		unsigned long size, unsigned int flags)
{
	unsigned long inner_flush_size = size;
	unsigned long outer_flush_size = size;
	(void) outer_flush_size;

#ifdef CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS
	inner_flush_size = min(size, (unsigned long)
		cache_maint_inner_threshold);
#endif

#if defined(CONFIG_NVMAP_OUTER_CACHE_MAINT_BY_SET_WAYS)
	outer_flush_size = min(size, (unsigned long)
		cache_maint_outer_threshold);
#endif

	if (flags == NVMAP_HANDLE_INNER_CACHEABLE)
		deferred_ops->deferred_maint_inner_requested +=
				inner_flush_size;

	if (flags == NVMAP_HANDLE_CACHEABLE) {
		deferred_ops->deferred_maint_inner_requested +=
				inner_flush_size;
#ifdef CONFIG_OUTER_CACHE
		deferred_ops->deferred_maint_outer_requested +=
				outer_flush_size;
#endif /* CONFIG_OUTER_CACHE */
	}
}

static void debug_count_flushed_op(struct nvmap_deferred_ops *deferred_ops,
		unsigned long size, unsigned int flags)
{
	unsigned long inner_flush_size = size;
	unsigned long outer_flush_size = size;
	(void) outer_flush_size;

#ifdef CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS
	inner_flush_size = min(size, (unsigned long)
		cache_maint_inner_threshold);
#endif

#if defined(CONFIG_NVMAP_OUTER_CACHE_MAINT_BY_SET_WAYS)
	outer_flush_size = min(size, (unsigned long)
		cache_maint_outer_threshold);
#endif

	if (flags == NVMAP_HANDLE_INNER_CACHEABLE)
		deferred_ops->deferred_maint_inner_flushed +=
				inner_flush_size;

	if (flags == NVMAP_HANDLE_CACHEABLE) {
		deferred_ops->deferred_maint_inner_flushed +=
				inner_flush_size;
#ifdef CONFIG_OUTER_CACHE
		deferred_ops->deferred_maint_outer_flushed +=
				outer_flush_size;
#endif /* CONFIG_OUTER_CACHE */
	}
}

struct cache_maint_op {
	struct list_head list_data;
	phys_addr_t start;
	phys_addr_t end;
	unsigned int op;
	struct nvmap_handle *h;
	int error;
	bool inner;
	bool outer;
};

static void cache_maint_work_funct(struct cache_maint_op *cache_work)
{
	pgprot_t prot;
	pte_t **pte = NULL;
	unsigned long kaddr;
	phys_addr_t pstart = cache_work->start;
	phys_addr_t pend = cache_work->end;
	phys_addr_t loop;
	int err = 0;
	struct nvmap_handle *h = cache_work->h;
	struct nvmap_client *client = h->owner;
	unsigned int op = cache_work->op;

	BUG_ON(!h);

	h = nvmap_handle_get(h);
	if (!h) {
		cache_work->error = -EFAULT;
		return;
	}
	if (!h->alloc) {
		cache_work->error = -EFAULT;
		goto out;
	}

	if (client)
		trace_cache_maint(client, h, pstart, pend, op);
	wmb();
	if (h->flags == NVMAP_HANDLE_UNCACHEABLE ||
	    h->flags == NVMAP_HANDLE_WRITE_COMBINE || pstart == pend)
		goto out;

	if (fast_cache_maint(h, pstart, pend, op))
		goto out;

	prot = nvmap_pgprot(h, pgprot_kernel);
	pte = nvmap_alloc_pte(h->dev, (void **)&kaddr);
	if (IS_ERR(pte)) {
		err = PTR_ERR(pte);
		pte = NULL;
		goto out;
	}

	if (h->heap_pgalloc) {
		heap_page_cache_maint(h, pstart, pend, op, true,
			(h->flags == NVMAP_HANDLE_INNER_CACHEABLE) ?
					false : true,
			pte, kaddr, prot);
		goto out;
	}

	if (pstart > h->size || pend > h->size) {
		pr_warn("cache maintenance outside handle\n");
		cache_work->error = -EINVAL;
		goto out;
	}

	/* lock carveout from relocation by mapcount */
	nvmap_usecount_inc(h);

	pstart += h->carveout->base;
	pend += h->carveout->base;

	loop = pstart;

	while (loop < pend) {
		phys_addr_t next = (loop + PAGE_SIZE) & PAGE_MASK;
		void *base = (void *)kaddr + (loop & ~PAGE_MASK);
		next = min(next, pend);

		set_pte_at(&init_mm, kaddr, *pte,
			   pfn_pte(__phys_to_pfn(loop), prot));
		nvmap_flush_tlb_kernel_page(kaddr);

		inner_cache_maint(op, base, next - loop);
		loop = next;
	}

	if (h->flags != NVMAP_HANDLE_INNER_CACHEABLE)
		outer_cache_maint(op, pstart, pend - pstart);

	/* unlock carveout */
	nvmap_usecount_dec(h);

out:
	if (pte)
		nvmap_free_pte(h->dev, pte);
	nvmap_handle_put(h);
	return;
}

int nvmap_find_cache_maint_op(struct nvmap_device *dev,
		struct nvmap_handle *h) {
	struct nvmap_deferred_ops *deferred_ops =
			nvmap_get_deferred_ops_from_dev(dev);
	struct cache_maint_op *cache_op = NULL;
	spin_lock(&deferred_ops->deferred_ops_lock);
	list_for_each_entry(cache_op, &deferred_ops->ops_list, list_data) {
		if (cache_op->h == h) {
			spin_unlock(&deferred_ops->deferred_ops_lock);
			return true;
		}
	}
	spin_unlock(&deferred_ops->deferred_ops_lock);
	return false;
}

void nvmap_cache_maint_ops_flush(struct nvmap_device *dev,
		struct nvmap_handle *h) {

	struct nvmap_deferred_ops *deferred_ops =
		nvmap_get_deferred_ops_from_dev(dev);

	struct cache_maint_op *cache_op = NULL;
	struct cache_maint_op *temp = NULL;

	size_t flush_size_outer_inner = 0;
	size_t flush_size_inner	= 0;
	bool allow_outer_flush_by_ways;

	struct list_head flushed_ops;

	(void) flush_size_outer_inner;
	(void) flush_size_inner;
	INIT_LIST_HEAD(&flushed_ops);

#ifdef CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS
	/* go through deferred ops, check if we can just do full L1/L2 flush
	 we only do list operation inside lock, actual maintenance shouldn't
	 block list operations */
	spin_lock(&deferred_ops->deferred_ops_lock);

#ifdef CONFIG_NVMAP_OUTER_CACHE_MAINT_BY_SET_WAYS
	allow_outer_flush_by_ways =
			cache_maint_outer_threshold >
				cache_maint_inner_threshold;
#else
	allow_outer_flush_by_ways = false;
#endif

	if (list_empty(&deferred_ops->ops_list)) {
		spin_unlock(&deferred_ops->deferred_ops_lock);
		return;
	}

	/* count sum of inner and outer flush ranges */
	list_for_each_entry(cache_op, &deferred_ops->ops_list, list_data) {
		if (cache_op->op == NVMAP_CACHE_OP_WB_INV) {
			unsigned long range =
					cache_op->end - cache_op->start;
			if (allow_outer_flush_by_ways &&
				cache_op->outer && cache_op->inner)
				flush_size_outer_inner += range;
			else
			if (cache_op->inner && !cache_op->outer)
				flush_size_inner += range;
		}
	}
	/* collect all flush operations */
	if (flush_size_outer_inner > cache_maint_outer_threshold) {
		list_for_each_entry_safe(cache_op, temp,
					&deferred_ops->ops_list, list_data) {
			if (cache_op->op == NVMAP_CACHE_OP_WB_INV &&
					(cache_op->outer && cache_op->inner))
				list_move(&cache_op->list_data, &flushed_ops);
		}
		debug_count_flushed_op(deferred_ops,
				cache_maint_outer_threshold,
				NVMAP_HANDLE_CACHEABLE);
		debug_count_flushed_op(deferred_ops,
				cache_maint_inner_threshold,
				NVMAP_HANDLE_INNER_CACHEABLE);
	} else if (flush_size_inner > cache_maint_inner_threshold) {
		list_for_each_entry_safe(cache_op, temp,
				&deferred_ops->ops_list, list_data) {
			if (cache_op->op == NVMAP_CACHE_OP_WB_INV &&
					(cache_op->inner && !cache_op->outer))
				list_move(&cache_op->list_data, &flushed_ops);
		}
		debug_count_flushed_op(deferred_ops,
				cache_maint_inner_threshold,
				NVMAP_HANDLE_INNER_CACHEABLE);
	}
	spin_unlock(&deferred_ops->deferred_ops_lock);

	/* do actual maintenance outside spinlock */
	if (flush_size_outer_inner > cache_maint_outer_threshold) {
		inner_flush_cache_all();
		outer_flush_all();
		/* cleanup finished ops */
		list_for_each_entry_safe(cache_op, temp,
				&flushed_ops, list_data) {
			list_del(&cache_op->list_data);
			nvmap_handle_put(cache_op->h);
			kfree(cache_op);
		}
	} else if (flush_size_inner > cache_maint_inner_threshold) {
		/* Flush only inner-cached entries */
		inner_flush_cache_all();
		/* cleanup finished ops */
		list_for_each_entry_safe(cache_op, temp,
				&flushed_ops, list_data) {
			list_del(&cache_op->list_data);
			nvmap_handle_put(cache_op->h);
			kfree(cache_op);
		}
	}
#endif
	/* Flush other handles (all or only requested) */
	spin_lock(&deferred_ops->deferred_ops_lock);
	list_for_each_entry_safe(cache_op, temp,
			&deferred_ops->ops_list, list_data) {
		if (!h || cache_op->h == h)
			list_move(&cache_op->list_data, &flushed_ops);
	}
	spin_unlock(&deferred_ops->deferred_ops_lock);

	list_for_each_entry_safe(cache_op, temp,
			&flushed_ops, list_data) {

		cache_maint_work_funct(cache_op);

		if (cache_op->op == NVMAP_CACHE_OP_WB_INV)
			debug_count_flushed_op(deferred_ops,
				cache_op->end - cache_op->start,
				cache_op->h->flags);

		list_del(&cache_op->list_data);
		nvmap_handle_put(cache_op->h);
		kfree(cache_op);
	}
}

static int cache_maint(struct nvmap_client *client,
			struct nvmap_handle *h,
			unsigned long start, unsigned long end,
			unsigned int op, unsigned int allow_deferred)
{
	int err = 0;
	struct nvmap_deferred_ops *deferred_ops =
		nvmap_get_deferred_ops_from_dev(client->dev);
	bool inner_maint = false;
	bool outer_maint = false;

	h = nvmap_handle_get(h);
	if (!h)
		return -EFAULT;

	/* count requested flush ops */
	if (op == NVMAP_CACHE_OP_WB_INV) {
		spin_lock(&deferred_ops->deferred_ops_lock);
		debug_count_requested_op(deferred_ops,
				end - start, h->flags);
		spin_unlock(&deferred_ops->deferred_ops_lock);
	}

	inner_maint = h->flags == NVMAP_HANDLE_CACHEABLE ||
			h->flags == NVMAP_HANDLE_INNER_CACHEABLE;

#ifdef CONFIG_OUTER_CACHE
	outer_maint = h->flags == NVMAP_HANDLE_CACHEABLE;
#endif

	/* Finish deferred maintenance for the handle before invalidating */
	if (op == NVMAP_CACHE_OP_INV &&
			nvmap_find_cache_maint_op(h->dev, h)) {
		struct nvmap_share *share = nvmap_get_share_from_dev(h->dev);
		mutex_lock(&share->pin_lock);
		nvmap_cache_maint_ops_flush(h->dev, h);
		mutex_unlock(&share->pin_lock);
	}

	if (op == NVMAP_CACHE_OP_WB_INV &&
			(inner_maint || outer_maint) &&
			allow_deferred == CACHE_MAINT_ALLOW_DEFERRED &&
			atomic_read(&h->pin) == 0 &&
			deferred_ops->enable_deferred_cache_maintenance) {

		struct cache_maint_op *cache_op;

		cache_op = (struct cache_maint_op *)
				kmalloc(sizeof(struct cache_maint_op),
					GFP_KERNEL);
		cache_op->h = h;
		cache_op->start = start;
		cache_op->end = end;
		cache_op->op = op;
		cache_op->inner = inner_maint;
		cache_op->outer = outer_maint;

		spin_lock(&deferred_ops->deferred_ops_lock);
			list_add_tail(&cache_op->list_data,
				&deferred_ops->ops_list);
		spin_unlock(&deferred_ops->deferred_ops_lock);
	} else {
		struct cache_maint_op cache_op;

		cache_op.h = h;
		cache_op.start = start;
		cache_op.end = end;
		cache_op.op = op;
		cache_op.inner = inner_maint;
		cache_op.outer = outer_maint;

		cache_maint_work_funct(&cache_op);

		if (op == NVMAP_CACHE_OP_WB_INV) {
			spin_lock(&deferred_ops->deferred_ops_lock);
			debug_count_flushed_op(deferred_ops,
				end - start, h->flags);
			spin_unlock(&deferred_ops->deferred_ops_lock);
		}

		err = cache_op.error;
		nvmap_handle_put(h);
	}
	return 0;
}

static int rw_handle_page(struct nvmap_handle *h, int is_read,
			  unsigned long start, unsigned long rw_addr,
			  unsigned long bytes, unsigned long kaddr, pte_t *pte)
{
	pgprot_t prot = nvmap_pgprot(h, pgprot_kernel);
	unsigned long end = start + bytes;
	int err = 0;

	while (!err && start < end) {
		struct page *page = NULL;
		phys_addr_t phys;
		size_t count;
		void *src;

		if (!h->heap_pgalloc) {
			phys = h->carveout->base + start;
		} else {
			page = h->pgalloc.pages[start >> PAGE_SHIFT];
			BUG_ON(!page);
			get_page(page);
			phys = page_to_phys(page) + (start & ~PAGE_MASK);
		}

		set_pte_at(&init_mm, kaddr, pte,
			   pfn_pte(__phys_to_pfn(phys), prot));
		nvmap_flush_tlb_kernel_page(kaddr);

		src = (void *)kaddr + (phys & ~PAGE_MASK);
		phys = PAGE_SIZE - (phys & ~PAGE_MASK);
		count = min_t(size_t, end - start, phys);

		if (is_read)
			err = copy_to_user((void *)rw_addr, src, count);
		else
			err = copy_from_user(src, (void *)rw_addr, count);

		if (err)
			err = -EFAULT;

		rw_addr += count;
		start += count;

		if (page)
			put_page(page);
	}

	return err;
}

static ssize_t rw_handle(struct nvmap_client *client, struct nvmap_handle *h,
			 int is_read, unsigned long h_offs,
			 unsigned long sys_addr, unsigned long h_stride,
			 unsigned long sys_stride, unsigned long elem_size,
			 unsigned long count)
{
	ssize_t copied = 0;
	pte_t **pte;
	void *addr;
	int ret = 0;

	if (!elem_size)
		return -EINVAL;

	if (!h->alloc)
		return -EFAULT;

	if (elem_size == h_stride && elem_size == sys_stride) {
		elem_size *= count;
		h_stride = elem_size;
		sys_stride = elem_size;
		count = 1;
	}

	pte = nvmap_alloc_pte(client->dev, &addr);
	if (IS_ERR(pte))
		return PTR_ERR(pte);

	while (count--) {
		if (h_offs + elem_size > h->size) {
			nvmap_warn(client, "read/write outside of handle\n");
			ret = -EFAULT;
			break;
		}
		if (is_read)
			cache_maint(client, h, h_offs,
				h_offs + elem_size, NVMAP_CACHE_OP_INV,
				CACHE_MAINT_IMMEDIATE);

		ret = rw_handle_page(h, is_read, h_offs, sys_addr,
				     elem_size, (unsigned long)addr, *pte);

		if (ret)
			break;

		if (!is_read)
			cache_maint(client, h, h_offs,
				h_offs + elem_size, NVMAP_CACHE_OP_WB_INV,
				CACHE_MAINT_IMMEDIATE);

		copied += elem_size;
		sys_addr += sys_stride;
		h_offs += h_stride;
	}

	nvmap_free_pte(client->dev, pte);
	return ret ?: copied;
}
