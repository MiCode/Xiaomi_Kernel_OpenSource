/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/spinlock.h>
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include <soc/qcom/secure_buffer.h>

#include "msm_drv.h"
#include "msm_gem.h"
#include "msm_gpu.h"
#include "msm_mmu.h"

static void msm_gem_mn_free(struct kref *refcount)
{
	struct msm_mmu_notifier *msm_mn = container_of(refcount,
			struct msm_mmu_notifier, refcount);

	mmu_notifier_unregister(&msm_mn->mn, msm_mn->mm);
	hash_del(&msm_mn->node);

	kfree(msm_mn);
}

static int msm_gem_mn_get(struct msm_mmu_notifier *msm_mn)
{
	if (msm_mn)
		return kref_get_unless_zero(&msm_mn->refcount);
	return 0;
}

static void msm_gem_mn_put(struct msm_mmu_notifier *msm_mn)
{
	if (msm_mn) {
		struct msm_drm_private *msm_dev = msm_mn->msm_dev;

		mutex_lock(&msm_dev->mn_lock);
		kref_put(&msm_mn->refcount, msm_gem_mn_free);
		mutex_unlock(&msm_dev->mn_lock);
	}
}

void msm_mn_invalidate_range_start(struct mmu_notifier *mn,
		struct mm_struct *mm, unsigned long start, unsigned long end);

static const struct mmu_notifier_ops msm_mn_ops = {
	.invalidate_range_start = msm_mn_invalidate_range_start,
};

static struct msm_mmu_notifier *
msm_gem_mn_find(struct msm_drm_private *msm_dev, struct mm_struct *mm,
		struct msm_gem_address_space *aspace)
{
	struct msm_mmu_notifier *msm_mn;
	int ret = 0;

	mutex_lock(&msm_dev->mn_lock);
	hash_for_each_possible(msm_dev->mn_hash, msm_mn, node,
			(unsigned long) mm) {
		if (msm_mn->mm == mm) {
			if (!msm_gem_mn_get(msm_mn)) {
				ret = -EINVAL;
				goto fail;
			}
			mutex_unlock(&msm_dev->mn_lock);
			return msm_mn;
		}
	}

	msm_mn = kzalloc(sizeof(*msm_mn), GFP_KERNEL);
	if (!msm_mn) {
		ret = -ENOMEM;
		goto fail;
	}

	msm_mn->mm = current->mm;
	msm_mn->mn.ops = &msm_mn_ops;
	ret = mmu_notifier_register(&msm_mn->mn, msm_mn->mm);
	if (ret) {
		kfree(msm_mn);
		goto fail;
	}

	msm_mn->svm_tree = RB_ROOT;
	spin_lock_init(&msm_mn->svm_tree_lock);
	kref_init(&msm_mn->refcount);
	msm_mn->msm_dev = msm_dev;

	/* Insert the msm_mn into the hash */
	hash_add(msm_dev->mn_hash, &msm_mn->node, (unsigned long) msm_mn->mm);
	mutex_unlock(&msm_dev->mn_lock);

	return msm_mn;

fail:
	mutex_unlock(&msm_dev->mn_lock);
	return ERR_PTR(ret);
}

static int msm_gem_mn_register(struct msm_gem_svm_object *msm_svm_obj,
		struct msm_gem_address_space *aspace)
{
	struct drm_gem_object *obj = &msm_svm_obj->msm_obj_base.base;
	struct msm_drm_private *msm_dev = obj->dev->dev_private;
	struct msm_mmu_notifier *msm_mn;

	msm_svm_obj->mm = current->mm;
	msm_svm_obj->svm_node.start = msm_svm_obj->hostptr;
	msm_svm_obj->svm_node.last = msm_svm_obj->hostptr + obj->size - 1;

	msm_mn = msm_gem_mn_find(msm_dev, msm_svm_obj->mm, aspace);
	if (IS_ERR(msm_mn))
		return PTR_ERR(msm_mn);

	msm_svm_obj->msm_mn = msm_mn;

	spin_lock(&msm_mn->svm_tree_lock);
	interval_tree_insert(&msm_svm_obj->svm_node, &msm_mn->svm_tree);
	spin_unlock(&msm_mn->svm_tree_lock);

	return 0;
}

static void msm_gem_mn_unregister(struct msm_gem_svm_object *msm_svm_obj)
{
	struct msm_mmu_notifier *msm_mn = msm_svm_obj->msm_mn;

	/* invalid: bo already unregistered */
	if (!msm_mn || msm_svm_obj->invalid)
		return;

	spin_lock(&msm_mn->svm_tree_lock);
	interval_tree_remove(&msm_svm_obj->svm_node, &msm_mn->svm_tree);
	spin_unlock(&msm_mn->svm_tree_lock);
}

static int protect_pages(struct msm_gem_object *msm_obj)
{
	int perm = PERM_READ | PERM_WRITE;
	int src = VMID_HLOS;
	int dst = VMID_CP_PIXEL;

	return hyp_assign_table(msm_obj->sgt, &src, 1, &dst, &perm, 1);
}

static int unprotect_pages(struct msm_gem_object *msm_obj)
{
	int perm = PERM_READ | PERM_WRITE | PERM_EXEC;
	int src = VMID_CP_PIXEL;
	int dst = VMID_HLOS;

	return hyp_assign_table(msm_obj->sgt, &src, 1, &dst, &perm, 1);
}

static void *get_dmabuf_ptr(struct drm_gem_object *obj)
{
	return (obj && obj->import_attach) ? obj->import_attach->dmabuf : NULL;
}

static dma_addr_t physaddr(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct msm_drm_private *priv = obj->dev->dev_private;
	return (((dma_addr_t)msm_obj->vram_node->start) << PAGE_SHIFT) +
			priv->vram.paddr;
}

static bool use_pages(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	return !msm_obj->vram_node;
}

/* allocate pages from VRAM carveout, used when no IOMMU: */
static struct page **get_pages_vram(struct drm_gem_object *obj, int npages)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct msm_drm_private *priv = obj->dev->dev_private;
	dma_addr_t paddr;
	struct page **p;
	int ret, i;

	p = drm_malloc_ab(npages, sizeof(struct page *));
	if (!p)
		return ERR_PTR(-ENOMEM);

	spin_lock(&priv->vram.lock);
	ret = drm_mm_insert_node(&priv->vram.mm, msm_obj->vram_node,
			npages, 0, DRM_MM_SEARCH_DEFAULT);
	spin_unlock(&priv->vram.lock);
	if (ret) {
		drm_free_large(p);
		return ERR_PTR(ret);
	}

	paddr = physaddr(obj);
	for (i = 0; i < npages; i++) {
		p[i] = phys_to_page(paddr);
		paddr += PAGE_SIZE;
	}

	return p;
}

static struct page **get_pages(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);

	if (!msm_obj->pages) {
		struct drm_device *dev = obj->dev;
		struct page **p;
		int npages = obj->size >> PAGE_SHIFT;

		if (use_pages(obj))
			p = drm_gem_get_pages(obj);
		else
			p = get_pages_vram(obj, npages);

		if (IS_ERR(p)) {
			dev_err(dev->dev, "could not get pages: %ld\n",
					PTR_ERR(p));
			return p;
		}

		msm_obj->pages = p;

		msm_obj->sgt = drm_prime_pages_to_sg(p, npages);
		if (IS_ERR(msm_obj->sgt)) {
			void *ptr = ERR_CAST(msm_obj->sgt);

			msm_obj->sgt = NULL;
			return ptr;
		}

		/*
		 * Make sure to flush the CPU cache for newly allocated memory
		 * so we don't get ourselves into trouble with a dirty cache
		 */
		if (msm_obj->flags & (MSM_BO_WC|MSM_BO_UNCACHED))
			dma_sync_sg_for_device(dev->dev, msm_obj->sgt->sgl,
				msm_obj->sgt->nents, DMA_BIDIRECTIONAL);

		/* Secure the pages if we need to */
		if (use_pages(obj) && msm_obj->flags & MSM_BO_SECURE) {
			int ret = protect_pages(msm_obj);

			if (ret)
				return ERR_PTR(ret);

			/*
			 * Set a flag to indicate the pages are locked by us and
			 * need to be unlocked when the pages get freed
			 */
			msm_obj->flags |= MSM_BO_LOCKED;
		}
	}

	return msm_obj->pages;
}

static void put_pages_vram(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct msm_drm_private *priv = obj->dev->dev_private;

	spin_lock(&priv->vram.lock);
	drm_mm_remove_node(msm_obj->vram_node);
	spin_unlock(&priv->vram.lock);

	drm_free_large(msm_obj->pages);
}

static void put_pages(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);

	if (msm_obj->pages) {
		if (msm_obj->flags & MSM_BO_LOCKED) {
			unprotect_pages(msm_obj);
			msm_obj->flags &= ~MSM_BO_LOCKED;
		}

		if (msm_obj->sgt)
			sg_free_table(msm_obj->sgt);
		kfree(msm_obj->sgt);

		if (use_pages(obj)) {
			if (msm_obj->flags & MSM_BO_SVM) {
				int npages = obj->size >> PAGE_SHIFT;

				release_pages(msm_obj->pages, npages, 0);
				kfree(msm_obj->pages);
			} else {
				drm_gem_put_pages(obj, msm_obj->pages,
						true, false);
			}
		} else {
			put_pages_vram(obj);
		}

		msm_obj->pages = NULL;
	}
}

struct page **msm_gem_get_pages(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct page **p;

	mutex_lock(&msm_obj->lock);
	p = get_pages(obj);
	mutex_unlock(&msm_obj->lock);
	return p;
}

void msm_gem_put_pages(struct drm_gem_object *obj)
{
	/* when we start tracking the pin count, then do something here */
}

int msm_gem_mmap_obj(struct drm_gem_object *obj,
		struct vm_area_struct *vma)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);

	/* We can't mmap secure objects or SVM objects */
	if (msm_obj->flags & (MSM_BO_SECURE | MSM_BO_SVM)) {
		drm_gem_vm_close(vma);
		return -EACCES;
	}

	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_flags |= VM_MIXEDMAP;

	if (msm_obj->flags & MSM_BO_WC) {
		vma->vm_page_prot = pgprot_writecombine(vm_get_page_prot(vma->vm_flags));
	} else if (msm_obj->flags & MSM_BO_UNCACHED) {
		vma->vm_page_prot = pgprot_noncached(vm_get_page_prot(vma->vm_flags));
	} else {
		/*
		 * Shunt off cached objs to shmem file so they have their own
		 * address_space (so unmap_mapping_range does what we want,
		 * in particular in the case of mmap'd dmabufs)
		 */
		fput(vma->vm_file);
		get_file(obj->filp);
		vma->vm_pgoff = 0;
		vma->vm_file  = obj->filp;

		vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	}

	return 0;
}

int msm_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;

	ret = drm_gem_mmap(filp, vma);
	if (ret) {
		DBG("mmap failed: %d", ret);
		return ret;
	}

	return msm_gem_mmap_obj(vma->vm_private_data, vma);
}

int msm_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct page **pages;
	unsigned long pfn;
	pgoff_t pgoff;
	int ret;

	/*
	 * vm_ops.open and close get and put a reference on obj.
	 * So, we dont need to hold one here.
	 */
	ret = mutex_lock_interruptible(&msm_obj->lock);
	if (ret)
		goto out;

	/* make sure we have pages attached now */
	pages = get_pages(obj);
	if (IS_ERR(pages)) {
		ret = PTR_ERR(pages);
		goto out_unlock;
	}

	/* We don't use vmf->pgoff since that has the fake offset: */
	pgoff = ((unsigned long)vmf->virtual_address -
			vma->vm_start) >> PAGE_SHIFT;

	pfn = page_to_pfn(pages[pgoff]);

	VERB("Inserting %pK pfn %lx, pa %lx", vmf->virtual_address,
			pfn, pfn << PAGE_SHIFT);

	ret = vm_insert_mixed(vma, (unsigned long)vmf->virtual_address, pfn);

out_unlock:
	mutex_unlock(&msm_obj->lock);
out:
	switch (ret) {
	case -EAGAIN:
	case 0:
	case -ERESTARTSYS:
	case -EINTR:
	case -EBUSY:
		/*
		 * EBUSY is ok: this just means that another thread
		 * already did the job.
		 */
		return VM_FAULT_NOPAGE;
	case -ENOMEM:
		return VM_FAULT_OOM;
	default:
		return VM_FAULT_SIGBUS;
	}
}

/** get mmap offset */
static uint64_t mmap_offset(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	int ret;

	WARN_ON(!mutex_is_locked(&msm_obj->lock));

	/* Make it mmapable */
	ret = drm_gem_create_mmap_offset(obj);

	if (ret) {
		dev_err(dev->dev, "could not allocate mmap offset\n");
		return 0;
	}

	return drm_vma_node_offset_addr(&obj->vma_node);
}

uint64_t msm_gem_mmap_offset(struct drm_gem_object *obj)
{
	uint64_t offset;
	struct msm_gem_object *msm_obj = to_msm_bo(obj);

	mutex_lock(&msm_obj->lock);
	offset = mmap_offset(obj);
	mutex_unlock(&msm_obj->lock);
	return offset;
}

static void obj_remove_domain(struct msm_gem_vma *domain)
{
	if (domain) {
		list_del(&domain->list);
		kfree(domain);
	}
}

/* Called with msm_obj->lock locked */
static void
put_iova(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct msm_gem_svm_object *msm_svm_obj;
	struct msm_gem_vma *domain, *tmp;
	bool invalid = false;

	WARN_ON(!mutex_is_locked(&msm_obj->lock));

	if (msm_obj->flags & MSM_BO_SVM) {
		msm_svm_obj = to_msm_svm_obj(msm_obj);
		invalid = msm_svm_obj->invalid;
	}

	list_for_each_entry_safe(domain, tmp, &msm_obj->domains, list) {
		if (iommu_present(&platform_bus_type)) {
			msm_gem_unmap_vma(domain->aspace, domain,
				msm_obj->sgt, get_dmabuf_ptr(obj), invalid);
		}

		obj_remove_domain(domain);
	}
}

static struct msm_gem_vma *obj_add_domain(struct drm_gem_object *obj,
		struct msm_gem_address_space *aspace)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct msm_gem_vma *domain = kzalloc(sizeof(*domain), GFP_KERNEL);

	if (!domain)
		return ERR_PTR(-ENOMEM);

	domain->aspace = aspace;

	list_add_tail(&domain->list, &msm_obj->domains);

	return domain;
}

static struct msm_gem_vma *obj_get_domain(struct drm_gem_object *obj,
		struct msm_gem_address_space *aspace)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct msm_gem_vma *domain;

	list_for_each_entry(domain, &msm_obj->domains, list) {
		if (domain->aspace == aspace)
			return domain;
	}

	return NULL;
}

#ifndef IOMMU_PRIV
#define IOMMU_PRIV 0
#endif

/* A reference to obj must be held before calling this function. */
int msm_gem_get_iova(struct drm_gem_object *obj,
		struct msm_gem_address_space *aspace, uint64_t *iova)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct page **pages;
	struct msm_gem_vma *domain;
	int ret = 0;

	mutex_lock(&msm_obj->lock);

	if (!iommu_present(&platform_bus_type)) {
		pages = get_pages(obj);

		if (IS_ERR(pages)) {
			mutex_unlock(&msm_obj->lock);
			return PTR_ERR(pages);
		}

		*iova = (uint64_t) physaddr(obj);
		mutex_unlock(&msm_obj->lock);
		return 0;
	}

	domain = obj_get_domain(obj, aspace);

	if (!domain) {
		domain = obj_add_domain(obj, aspace);
		if (IS_ERR(domain)) {
			mutex_unlock(&msm_obj->lock);
			return PTR_ERR(domain);
		}

		pages = get_pages(obj);
		if (IS_ERR(pages)) {
			obj_remove_domain(domain);
			mutex_unlock(&msm_obj->lock);
			return PTR_ERR(pages);
		}

		ret = msm_gem_map_vma(aspace, domain, msm_obj->sgt,
			get_dmabuf_ptr(obj), msm_obj->flags);
	}

	if (!ret)
		*iova = domain->iova;
	else
		obj_remove_domain(domain);

	mutex_unlock(&msm_obj->lock);
	return ret;
}

/* get iova without taking a reference, used in places where you have
 * already done a 'msm_gem_get_iova()'.
 */
uint64_t msm_gem_iova(struct drm_gem_object *obj,
		struct msm_gem_address_space *aspace)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct msm_gem_vma *domain;
	uint64_t iova;

	mutex_lock(&msm_obj->lock);
	domain  = obj_get_domain(obj, aspace);
	WARN_ON(!domain);
	iova = domain ? domain->iova : 0;
	mutex_unlock(&msm_obj->lock);

	return iova;
}

void msm_gem_put_iova(struct drm_gem_object *obj,
		struct msm_gem_address_space *aspace)
{
	// XXX TODO ..
	// NOTE: probably don't need a _locked() version.. we wouldn't
	// normally unmap here, but instead just mark that it could be
	// unmapped (if the iova refcnt drops to zero), but then later
	// if another _get_iova_locked() fails we can start unmapping
	// things that are no longer needed..
}

int msm_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
		struct drm_mode_create_dumb *args)
{
	args->pitch = align_pitch(args->width, args->bpp);
	args->size  = PAGE_ALIGN(args->pitch * args->height);
	return msm_gem_new_handle(dev, file, args->size,
			MSM_BO_SCANOUT | MSM_BO_WC, &args->handle);
}

int msm_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
		uint32_t handle, uint64_t *offset)
{
	struct drm_gem_object *obj;
	int ret = 0;

	/* GEM does all our handle to object mapping */
	obj = drm_gem_object_lookup(dev, file, handle);
	if (obj == NULL) {
		ret = -ENOENT;
		goto fail;
	}

	*offset = msm_gem_mmap_offset(obj);

	drm_gem_object_unreference_unlocked(obj);

fail:
	return ret;
}

void *msm_gem_vaddr(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);

	mutex_lock(&msm_obj->lock);

	if (msm_obj->vaddr) {
		mutex_unlock(&msm_obj->lock);
		return msm_obj->vaddr;
	}

	if (obj->import_attach) {
		msm_obj->vaddr = dma_buf_vmap(obj->import_attach->dmabuf);
	} else {
		struct page **pages = get_pages(obj);
		if (IS_ERR(pages)) {
			mutex_unlock(&msm_obj->lock);
			return ERR_CAST(pages);
		}
		msm_obj->vaddr = vmap(pages, obj->size >> PAGE_SHIFT,
				VM_MAP, pgprot_writecombine(PAGE_KERNEL));
	}
	mutex_unlock(&msm_obj->lock);

	return msm_obj->vaddr;
}

/* setup callback for when bo is no longer busy..
 * TODO probably want to differentiate read vs write..
 */
int msm_gem_queue_inactive_cb(struct drm_gem_object *obj,
		struct msm_fence_cb *cb)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	uint32_t fence = msm_gem_fence(msm_obj,
			MSM_PREP_READ | MSM_PREP_WRITE);
	return msm_queue_fence_cb(obj->dev, cb, fence);
}

void msm_gem_move_to_active(struct drm_gem_object *obj,
		struct msm_gpu *gpu, bool write, uint32_t fence)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	msm_obj->gpu = gpu;
	if (write)
		msm_obj->write_fence = fence;
	else
		msm_obj->read_fence = fence;
	list_del_init(&msm_obj->mm_list);
	list_add_tail(&msm_obj->mm_list, &gpu->active_list);
}

void msm_gem_move_to_inactive(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_gem_object *msm_obj = to_msm_bo(obj);

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));

	msm_obj->gpu = NULL;
	msm_obj->read_fence = 0;
	msm_obj->write_fence = 0;
	list_del_init(&msm_obj->mm_list);
	list_add_tail(&msm_obj->mm_list, &priv->inactive_list);
}

int msm_gem_cpu_prep(struct drm_gem_object *obj, uint32_t op, ktime_t *timeout)
{
	struct drm_device *dev = obj->dev;
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	int ret = 0;

	if (is_active(msm_obj)) {
		uint32_t fence = msm_gem_fence(msm_obj, op);

		if (op & MSM_PREP_NOSYNC)
			timeout = NULL;

		ret = msm_wait_fence(dev, fence, timeout, true);
	}

	/* TODO cache maintenance */

	return ret;
}

int msm_gem_cpu_fini(struct drm_gem_object *obj)
{
	/* TODO cache maintenance */
	return 0;
}

void msm_gem_sync(struct drm_gem_object *obj, u32 op)
{
	struct drm_device *dev = obj->dev;
	struct msm_gem_object *msm_obj = to_msm_bo(obj);

	if (msm_obj->flags & (MSM_BO_WC|MSM_BO_UNCACHED))
		return;

	switch (op) {
	case MSM_GEM_SYNC_TO_CPU:
		dma_sync_sg_for_cpu(dev->dev, msm_obj->sgt->sgl,
			msm_obj->sgt->nents, DMA_BIDIRECTIONAL);
		break;
	case MSM_GEM_SYNC_TO_DEV:
		dma_sync_sg_for_device(dev->dev, msm_obj->sgt->sgl,
			msm_obj->sgt->nents, DMA_BIDIRECTIONAL);
		break;
	}
}

#ifdef CONFIG_DEBUG_FS
void msm_gem_describe(struct drm_gem_object *obj, struct seq_file *m)
{
	struct drm_device *dev = obj->dev;
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct msm_gem_vma *domain;
	uint64_t off = drm_vma_node_start(&obj->vma_node);

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));
	seq_printf(m, "%08x: %c(r=%u,w=%u) %2d (%2d) %08llx %pK\t",
			msm_obj->flags, is_active(msm_obj) ? 'A' : 'I',
			msm_obj->read_fence, msm_obj->write_fence,
			obj->name, obj->refcount.refcount.counter,
			off, msm_obj->vaddr);

	/* FIXME: we need to print the address space here too */
	list_for_each_entry(domain, &msm_obj->domains, list)
		seq_printf(m, " %08llx", domain->iova);

	seq_puts(m, "\n");
}

void msm_gem_describe_objects(struct list_head *list, struct seq_file *m)
{
	struct msm_gem_object *msm_obj;
	int count = 0;
	size_t size = 0;

	list_for_each_entry(msm_obj, list, mm_list) {
		struct drm_gem_object *obj = &msm_obj->base;
		seq_printf(m, "   ");
		msm_gem_describe(obj, m);
		count++;
		size += obj->size;
	}

	seq_printf(m, "Total %d objects, %zu bytes\n", count, size);
}
#endif

void msm_gem_free_object(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct msm_gem_svm_object *msm_svm_obj = NULL;

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));

	/* object should not be on active list: */
	WARN_ON(is_active(msm_obj));

	if (msm_obj->flags & MSM_BO_SVM)
		msm_svm_obj = to_msm_svm_obj(msm_obj);

	list_del(&msm_obj->mm_list);

	/* Unregister SVM object from mmu notifications */
	if (msm_obj->flags & MSM_BO_SVM) {
		msm_gem_mn_unregister(msm_svm_obj);
		msm_gem_mn_put(msm_svm_obj->msm_mn);
		msm_svm_obj->msm_mn = NULL;
	}

	mutex_lock(&msm_obj->lock);
	put_iova(obj);

	if (obj->import_attach) {
		if (msm_obj->vaddr)
			dma_buf_vunmap(obj->import_attach->dmabuf,
				msm_obj->vaddr);
		/* Don't drop the pages for imported dmabuf, as they are not
		 * ours, just free the array we allocated:
		 */
		if (msm_obj->pages)
			drm_free_large(msm_obj->pages);

		drm_prime_gem_destroy(obj, msm_obj->sgt);
	} else {
		vunmap(msm_obj->vaddr);
		put_pages(obj);
	}

	if (msm_obj->resv == &msm_obj->_resv)
		reservation_object_fini(msm_obj->resv);

	drm_gem_object_release(obj);
	mutex_unlock(&msm_obj->lock);

	if (msm_obj->flags & MSM_BO_SVM)
		kfree(msm_svm_obj);
	else
		kfree(msm_obj);
}

/* convenience method to construct a GEM buffer object, and userspace handle */
int msm_gem_new_handle(struct drm_device *dev, struct drm_file *file,
		uint32_t size, uint32_t flags, uint32_t *handle)
{
	struct drm_gem_object *obj;
	int ret;

	obj = msm_gem_new(dev, size, flags);

	if (IS_ERR(obj))
		return PTR_ERR(obj);

	ret = drm_gem_handle_create(file, obj, handle);

	/* drop reference from allocate - handle holds it now */
	drm_gem_object_unreference_unlocked(obj);

	return ret;
}

/* convenience method to construct an SVM buffer object, and userspace handle */
int msm_gem_svm_new_handle(struct drm_device *dev, struct drm_file *file,
		uint64_t hostptr, uint64_t size,
		uint32_t flags, uint32_t *handle)
{
	struct drm_gem_object *obj;
	int ret;

	obj = msm_gem_svm_new(dev, file, hostptr, size, flags);

	if (IS_ERR(obj))
		return PTR_ERR(obj);

	ret = drm_gem_handle_create(file, obj, handle);

	/* drop reference from allocate - handle holds it now */
	drm_gem_object_unreference_unlocked(obj);

	return ret;
}

static int msm_gem_obj_init(struct drm_device *dev,
		uint32_t size, uint32_t flags,
		struct msm_gem_object *msm_obj, bool struct_mutex_locked)
{
	struct msm_drm_private *priv = dev->dev_private;
	bool use_vram = false;

	switch (flags & MSM_BO_CACHE_MASK) {
	case MSM_BO_UNCACHED:
	case MSM_BO_CACHED:
	case MSM_BO_WC:
		break;
	default:
		dev_err(dev->dev, "invalid cache flag: %x\n",
				(flags & MSM_BO_CACHE_MASK));
		return -EINVAL;
	}

	if (!iommu_present(&platform_bus_type))
		use_vram = true;
	else if ((flags & MSM_BO_STOLEN) && priv->vram.size)
		use_vram = true;

	if (WARN_ON(use_vram && !priv->vram.size))
		return -EINVAL;

	mutex_init(&msm_obj->lock);

	if (use_vram) {
		struct msm_gem_vma *domain = obj_add_domain(&msm_obj->base, 0);

		if (!IS_ERR(domain))
			msm_obj->vram_node = &domain->node;
	}

	msm_obj->flags = flags;

	msm_obj->resv = &msm_obj->_resv;
	reservation_object_init(msm_obj->resv);

	INIT_LIST_HEAD(&msm_obj->mm_list);
	INIT_LIST_HEAD(&msm_obj->submit_entry);
	INIT_LIST_HEAD(&msm_obj->domains);

	if (struct_mutex_locked) {
		list_add_tail(&msm_obj->mm_list, &priv->inactive_list);
	} else {
		mutex_lock(&dev->struct_mutex);
		list_add_tail(&msm_obj->mm_list, &priv->inactive_list);
		mutex_unlock(&dev->struct_mutex);
	}

	return 0;
}

static struct drm_gem_object *msm_gem_new_impl(struct drm_device *dev,
		uint32_t size, uint32_t flags, bool struct_mutex_locked)
{
	struct msm_gem_object *msm_obj;
	int ret;

	msm_obj = kzalloc(sizeof(*msm_obj), GFP_KERNEL);
	if (!msm_obj)
		return ERR_PTR(-ENOMEM);

	ret = msm_gem_obj_init(dev, size, flags, msm_obj, struct_mutex_locked);
	if (ret) {
		kfree(msm_obj);
		return ERR_PTR(ret);
	}

	return &msm_obj->base;
}

static struct drm_gem_object *_msm_gem_new(struct drm_device *dev,
		uint32_t size, uint32_t flags, bool struct_mutex_locked)
{
	struct drm_gem_object *obj;
	int ret;

	size = PAGE_ALIGN(size);

	/*
	 * Disallow zero sized objects as they make the underlying
	 * infrastructure grumpy
	 */
	if (!size)
		return ERR_PTR(-EINVAL);

	obj = msm_gem_new_impl(dev, size, flags, struct_mutex_locked);
	if (IS_ERR(obj))
		return obj;

	if (use_pages(obj)) {
		ret = drm_gem_object_init(dev, obj, size);
		if (ret)
			goto fail;
	} else {
		drm_gem_private_object_init(dev, obj, size);
	}

	return obj;

fail:
	drm_gem_object_unreference_unlocked(obj);

	return ERR_PTR(ret);
}

struct drm_gem_object *msm_gem_new_locked(struct drm_device *dev,
		uint32_t size, uint32_t flags)
{
	return _msm_gem_new(dev, size, flags, true);
}

struct drm_gem_object *msm_gem_new(struct drm_device *dev,
		uint32_t size, uint32_t flags)
{
	return _msm_gem_new(dev, size, flags, false);
}

static struct drm_gem_object *msm_svm_gem_new_impl(struct drm_device *dev,
		uint32_t size, uint32_t flags)
{
	struct msm_gem_svm_object *msm_svm_obj;
	struct msm_gem_object *msm_obj;
	int ret;

	msm_svm_obj = kzalloc(sizeof(*msm_svm_obj), GFP_KERNEL);
	if (!msm_svm_obj)
		return ERR_PTR(-ENOMEM);

	msm_obj = &msm_svm_obj->msm_obj_base;

	ret = msm_gem_obj_init(dev, size, flags | MSM_BO_SVM, msm_obj, false);
	if (ret) {
		kfree(msm_svm_obj);
		return ERR_PTR(ret);
	}

	return &msm_obj->base;
}

/* convenience method to construct an SVM GEM bo, and userspace handle */
struct drm_gem_object *msm_gem_svm_new(struct drm_device *dev,
		struct drm_file *file, uint64_t hostptr,
		uint64_t size, uint32_t flags)
{
	struct drm_gem_object *obj;
	struct msm_file_private *ctx = file->driver_priv;
	struct msm_gem_address_space *aspace;
	struct msm_gem_object *msm_obj;
	struct msm_gem_svm_object *msm_svm_obj;
	struct msm_gem_vma *domain = NULL;
	struct page **p;
	int npages;
	int num_pinned = 0;
	int write;
	int ret;

	if (!ctx)
		return ERR_PTR(-ENODEV);

	/* if we don't have IOMMU, don't bother pretending we can import: */
	if (!iommu_present(&platform_bus_type)) {
		dev_err_once(dev->dev, "cannot import without IOMMU\n");
		return ERR_PTR(-EINVAL);
	}

	/* hostptr and size must be page-aligned */
	if (offset_in_page(hostptr | size))
		return ERR_PTR(-EINVAL);

	/* Only CPU cached SVM objects are allowed */
	if ((flags & MSM_BO_CACHE_MASK) != MSM_BO_CACHED)
		return ERR_PTR(-EINVAL);

	/* Allocate and initialize a new msm_gem_object */
	obj = msm_svm_gem_new_impl(dev, size, flags);
	if (IS_ERR(obj))
		return obj;

	drm_gem_private_object_init(dev, obj, size);

	msm_obj = to_msm_bo(obj);
	aspace = ctx->aspace;
	domain = obj_add_domain(&msm_obj->base, aspace);
	if (IS_ERR(domain)) {
		drm_gem_object_unreference_unlocked(obj);
		return ERR_CAST(domain);
	}

	/* Reserve iova if not already in use, else fail */
	ret = msm_gem_reserve_iova(aspace, domain, hostptr, size);
	if (ret) {
		obj_remove_domain(domain);
		drm_gem_object_unreference_unlocked(obj);
		return ERR_PTR(ret);
	}

	msm_svm_obj = to_msm_svm_obj(msm_obj);
	msm_svm_obj->hostptr = hostptr;
	msm_svm_obj->invalid = false;

	ret = msm_gem_mn_register(msm_svm_obj, aspace);
	if (ret)
		goto fail;

	/*
	 * Get physical pages and map into smmu in the ioctl itself.
	 * The driver handles iova allocation, physical page allocation and
	 * SMMU map all in one go. If we break this, then we have to maintain
	 * state to tell if physical pages allocation/map needs to happen.
	 * For SVM, iova reservation needs to happen in the ioctl itself,
	 * so do the rest right here as well.
	 */
	npages = size >> PAGE_SHIFT;
	p = kcalloc(npages, sizeof(struct page *), GFP_KERNEL);
	if (!p) {
		ret = -ENOMEM;
		goto fail;
	}

	write = (msm_obj->flags & MSM_BO_GPU_READONLY) ? 0 : 1;
	/* This may hold mm->mmap_sem */
	num_pinned = get_user_pages_fast(hostptr, npages, write, p);
	if (num_pinned != npages) {
		ret = -EINVAL;
		goto free_pages;
	}

	msm_obj->sgt = drm_prime_pages_to_sg(p, npages);
	if (IS_ERR(msm_obj->sgt)) {
		ret = PTR_ERR(msm_obj->sgt);
		goto free_pages;
	}

	msm_obj->pages = p;

	ret = aspace->mmu->funcs->map(aspace->mmu, domain->iova,
			msm_obj->sgt, msm_obj->flags, get_dmabuf_ptr(obj));
	if (ret)
		goto free_pages;

	kref_get(&aspace->kref);

	return obj;

free_pages:
	release_pages(p, num_pinned, 0);
	kfree(p);

fail:
	if (domain)
		msm_gem_release_iova(aspace, domain);

	obj_remove_domain(domain);
	drm_gem_object_unreference_unlocked(obj);

	return ERR_PTR(ret);
}

struct drm_gem_object *msm_gem_import(struct drm_device *dev,
		uint32_t size, struct sg_table *sgt, u32 flags)
{
	struct msm_gem_object *msm_obj;
	struct drm_gem_object *obj;
	int ret, npages;

	/* if we don't have IOMMU, don't bother pretending we can import: */
	if (!iommu_present(&platform_bus_type)) {
		dev_err(dev->dev, "cannot import without IOMMU\n");
		return ERR_PTR(-EINVAL);
	}

	size = PAGE_ALIGN(size);

	obj = msm_gem_new_impl(dev, size, MSM_BO_WC, false);
	if (IS_ERR(obj))
		return obj;

	drm_gem_private_object_init(dev, obj, size);

	npages = size / PAGE_SIZE;

	msm_obj = to_msm_bo(obj);
	mutex_lock(&msm_obj->lock);
	msm_obj->sgt = sgt;
	msm_obj->pages = drm_malloc_ab(npages, sizeof(struct page *));
	if (!msm_obj->pages) {
		mutex_unlock(&msm_obj->lock);
		ret = -ENOMEM;
		goto fail;
	}

	/* OR the passed in flags */
	msm_obj->flags |= flags;

	ret = drm_prime_sg_to_page_addr_arrays(sgt, msm_obj->pages,
			NULL, npages);
	if (ret) {
		mutex_unlock(&msm_obj->lock);
		goto fail;
	}

	mutex_unlock(&msm_obj->lock);

	return obj;

fail:
	drm_gem_object_unreference_unlocked(obj);

	return ERR_PTR(ret);
}

/* Timeout in ms, long enough so we are sure the GPU is hung */
#define SVM_OBJ_WAIT_TIMEOUT 10000
static void invalidate_svm_object(struct msm_gem_svm_object *msm_svm_obj)
{
	struct msm_gem_object *msm_obj = &msm_svm_obj->msm_obj_base;
	struct drm_device *dev = msm_obj->base.dev;
	struct msm_gem_vma *domain, *tmp;
	uint32_t fence;
	int ret;

	if (is_active(msm_obj)) {
		ktime_t timeout = ktime_add_ms(ktime_get(),
				SVM_OBJ_WAIT_TIMEOUT);

		/* Get the most recent fence that touches the object */
		fence = msm_gem_fence(msm_obj, MSM_PREP_READ | MSM_PREP_WRITE);

		/* Wait for the fence to retire */
		ret = msm_wait_fence(dev, fence, &timeout, true);
		if (ret)
			/* The GPU could be hung! Not much we can do */
			dev_err(dev->dev, "drm: Error (%d) waiting for svm object: 0x%llx",
					ret, msm_svm_obj->hostptr);
	}

	/* GPU is done, unmap object from SMMU */
	mutex_lock(&msm_obj->lock);
	list_for_each_entry_safe(domain, tmp, &msm_obj->domains, list) {
		struct msm_gem_address_space *aspace = domain->aspace;

		if (domain->iova)
			aspace->mmu->funcs->unmap(aspace->mmu,
					domain->iova, msm_obj->sgt,
					get_dmabuf_ptr(&msm_obj->base));
	}
	/* Let go of the physical pages */
	put_pages(&msm_obj->base);
	mutex_unlock(&msm_obj->lock);
}

void msm_mn_invalidate_range_start(struct mmu_notifier *mn,
		struct mm_struct *mm, unsigned long start, unsigned long end)
{
	struct msm_mmu_notifier *msm_mn =
		container_of(mn, struct msm_mmu_notifier, mn);
	struct interval_tree_node *itn = NULL;
	struct msm_gem_svm_object *msm_svm_obj;
	struct drm_gem_object *obj;
	LIST_HEAD(inv_list);

	if (!msm_gem_mn_get(msm_mn))
		return;

	spin_lock(&msm_mn->svm_tree_lock);
	itn = interval_tree_iter_first(&msm_mn->svm_tree, start, end - 1);
	while (itn) {
		msm_svm_obj = container_of(itn,
				struct msm_gem_svm_object, svm_node);
		obj = &msm_svm_obj->msm_obj_base.base;

		if (kref_get_unless_zero(&obj->refcount))
			list_add(&msm_svm_obj->lnode, &inv_list);

		itn = interval_tree_iter_next(itn, start, end - 1);
	}
	spin_unlock(&msm_mn->svm_tree_lock);

	list_for_each_entry(msm_svm_obj, &inv_list, lnode) {
		obj = &msm_svm_obj->msm_obj_base.base;
		/* Unregister SVM object from mmu notifications */
		msm_gem_mn_unregister(msm_svm_obj);
		msm_svm_obj->invalid = true;
		invalidate_svm_object(msm_svm_obj);
		drm_gem_object_unreference_unlocked(obj);
	}

	msm_gem_mn_put(msm_mn);
}

/*
 * Helper function to consolidate in-kernel buffer allocations that usually need
 * to allocate a buffer object, iova and a virtual address all in one shot
 */
static void *_msm_gem_kernel_new(struct drm_device *dev, uint32_t size,
		uint32_t flags, struct msm_gem_address_space *aspace,
		struct drm_gem_object **bo, uint64_t *iova, bool locked)
{
	void *vaddr;
	struct drm_gem_object *obj = _msm_gem_new(dev, size, flags, locked);
	int ret;

	if (IS_ERR(obj))
		return ERR_CAST(obj);

	ret = msm_gem_get_iova(obj, aspace, iova);
	if (ret) {
		drm_gem_object_unreference(obj);
		return ERR_PTR(ret);
	}

	vaddr = msm_gem_vaddr(obj);
	if (!vaddr) {
		msm_gem_put_iova(obj, aspace);
		drm_gem_object_unreference(obj);
		return ERR_PTR(-ENOMEM);
	}

	*bo = obj;
	return vaddr;
}

void *msm_gem_kernel_new(struct drm_device *dev, uint32_t size,
		uint32_t flags, struct msm_gem_address_space *aspace,
		struct drm_gem_object **bo, uint64_t *iova)
{
	return _msm_gem_kernel_new(dev, size, flags, aspace, bo, iova,
		false);
}

void *msm_gem_kernel_new_locked(struct drm_device *dev, uint32_t size,
		uint32_t flags, struct msm_gem_address_space *aspace,
		struct drm_gem_object **bo, uint64_t *iova)
{
	return _msm_gem_kernel_new(dev, size, flags, aspace, bo, iova,
		true);
}
