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

#include "msm_drv.h"
#include "msm_gem.h"
#include "msm_gpu.h"
#include "msm_mmu.h"

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
static struct page **get_pages_vram(struct drm_gem_object *obj,
		int npages)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct msm_drm_private *priv = obj->dev->dev_private;
	dma_addr_t paddr;
	struct page **p;
	int ret, i;

	p = drm_malloc_ab(npages, sizeof(struct page *));
	if (!p)
		return ERR_PTR(-ENOMEM);

	ret = drm_mm_insert_node(&priv->vram.mm, msm_obj->vram_node,
			npages, 0, DRM_MM_SEARCH_DEFAULT);
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

static int msm_drm_alloc_buf(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	enum dma_attr attr;
	unsigned int nr_pages;
	struct page **p = NULL;
	struct drm_device *dev = obj->dev;
	struct msm_gem_buf *buf;

	if (msm_obj->buf)
		return 0;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		DRM_ERROR("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	init_dma_attrs(&buf->dma_attrs);

	if (msm_obj->flags & MSM_BO_CONTIGUOUS)
		dma_set_attr(DMA_ATTR_FORCE_CONTIGUOUS, &buf->dma_attrs);

	if (msm_obj->flags & (MSM_BO_UNCACHED | MSM_BO_WC))
		attr = DMA_ATTR_WRITE_COMBINE;
	else
		attr = DMA_ATTR_NON_CONSISTENT;

	dma_set_attr(attr, &buf->dma_attrs);
	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &buf->dma_attrs);

	nr_pages = obj->size >> PAGE_SHIFT;

	p = dma_alloc_attrs(dev->dev, obj->size,
				&buf->dma_addr, GFP_KERNEL, &buf->dma_attrs);
	if (!p) {
		DRM_ERROR("failed to allocate buffer.\n");
		kfree(buf);
		return -ENOMEM;
	}

	msm_obj->buf = buf;
	msm_obj->pages = p;
	return 0;
}

static int msm_drm_free_buf(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct msm_gem_buf *buf = msm_obj->buf;
	struct drm_device *dev = obj->dev;

	if (!buf)
		return 0;

	dma_free_attrs(dev->dev, obj->size, msm_obj->pages,
				(dma_addr_t)buf->dma_addr, &buf->dma_attrs);

	kfree(buf);
	msm_obj->buf = NULL;

	return 0;
}

int msm_drm_gem_mmap_buffer(struct drm_gem_object *obj,
				      struct vm_area_struct *vma)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	struct msm_gem_buf *buf = msm_obj->buf;
	struct drm_device *dev = obj->dev;
	unsigned long vm_size;
	int ret;

	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_pgoff = 0;

	vm_size = vma->vm_end - vma->vm_start;

	/* check if user-requested size is valid. */
	if (vm_size > obj->size)
		return -EINVAL;

	ret = dma_mmap_attrs(dev->dev, vma, msm_obj->pages,
				buf->dma_addr, obj->size,
				&buf->dma_attrs);
	if (ret < 0) {
		DRM_ERROR("failed to mmap.\n");
		return ret;
	}

	return 0;
}

/* called with dev->struct_mutex held */
static struct page **get_pages(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);

	if (!msm_obj->pages) {
		struct page **p;
		int npages = obj->size >> PAGE_SHIFT;

		if (use_pages(obj)) {
			if (!msm_drm_alloc_buf(obj))
				p = msm_obj->pages;
		} else {
			p = get_pages_vram(obj, npages);
		}

		if (IS_ERR(p)) {
			DRM_ERROR("could not get pages: %ld\n", PTR_ERR(p));
			return p;
		}

		msm_obj->pages = p;
	}

	return msm_obj->pages;
}

static void put_pages(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);

	if (msm_obj->pages) {
		if (use_pages(obj))
			msm_drm_free_buf(obj);
		else {
			drm_mm_remove_node(msm_obj->vram_node);
			drm_free_large(msm_obj->pages);
		}

		msm_obj->pages = NULL;
	}
}

struct page **msm_gem_get_pages(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct page **p;
	mutex_lock(&dev->struct_mutex);
	p = get_pages(obj);
	mutex_unlock(&dev->struct_mutex);
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
	struct drm_device *dev = obj->dev;
	struct page **pages;
	unsigned long pfn;
	pgoff_t pgoff;
	int ret;

	/* Make sure we don't parallel update on a fault, nor move or remove
	 * something from beneath our feet
	 */
	ret = mutex_lock_interruptible(&dev->struct_mutex);
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
	mutex_unlock(&dev->struct_mutex);
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
	int ret;

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));

	/* Make it mmapable */
	ret = drm_gem_create_mmap_offset(obj);

	if (ret) {
		DRM_ERROR("could not allocate mmap offset\n");
		return 0;
	}

	return drm_vma_node_offset_addr(&obj->vma_node);
}

uint64_t msm_gem_mmap_offset(struct drm_gem_object *obj)
{
	uint64_t offset;
	mutex_lock(&obj->dev->struct_mutex);
	offset = mmap_offset(obj);
	mutex_unlock(&obj->dev->struct_mutex);
	return offset;
}

/* should be called under struct_mutex.. although it can be called
 * from atomic context without struct_mutex to acquire an extra
 * iova ref if you know one is already held.
 *
 * That means when I do eventually need to add support for unpinning
 * the refcnt counter needs to be atomic_t.
 */
int msm_gem_get_iova_locked(struct drm_gem_object *obj, int id,
		uint32_t *iova)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	int ret = 0;

	if (!msm_obj->domain[id].iova) {
		struct msm_drm_private *priv = obj->dev->dev_private;
		int npages = obj->size >> PAGE_SHIFT;
		struct page **pages = get_pages(obj);

		if (IS_ERR(pages))
			return PTR_ERR(pages);

		if (!msm_obj->domain[id].sgt) {
			msm_obj->domain[id].sgt =
					drm_prime_pages_to_sg(pages, npages);
			if (IS_ERR(msm_obj->domain[id].sgt)) {
				DRM_ERROR("failed to allocate sgt\n");
				return PTR_ERR(msm_obj->domain[id].sgt);
			}
		}

		if (iommu_present(&platform_bus_type)) {
			struct msm_mmu *mmu = priv->mmus[id];

			if (WARN_ON(!mmu))
				return -EINVAL;

			if (obj->import_attach && mmu->funcs->map_dma_buf) {
				ret = mmu->funcs->map_dma_buf(mmu,
						msm_obj->domain[id].sgt,
						obj->import_attach->dmabuf,
						DMA_BIDIRECTIONAL);
				if (ret) {
					DRM_ERROR("Unable to map dma buf\n");
					return ret;
				}
				msm_obj->domain[id].iova =
				sg_dma_address(msm_obj->domain[id].sgt->sgl);
			} else if (!use_pages(obj)) {
				/* use vram */
				dma_addr_t pa = physaddr(obj);

				ret = mmu->funcs->map(mmu, pa,
						msm_obj->domain[id].sgt,
						IOMMU_READ | IOMMU_NOEXEC);
				if (ret) {
					DRM_ERROR("Unable to map phy buf=%pK\n",
						(void *)pa);
					return ret;
				}
				msm_obj->domain[id].iova = pa;
			} else {
				if (msm_obj->flags &
						(MSM_BO_WC|MSM_BO_UNCACHED))
					dma_map_sg(mmu->dev,
						msm_obj->domain[id].sgt->sgl,
						msm_obj->domain[id].sgt->nents,
						DMA_BIDIRECTIONAL);
				msm_obj->domain[id].iova =
				sg_dma_address(msm_obj->domain[id].sgt->sgl);
			}
			DRM_DEBUG("iova=%pK\n",
					(void *)msm_obj->domain[id].iova);
		} else {
			WARN_ONCE(1, "physical address being used\n");
			msm_obj->domain[id].iova = physaddr(obj);
		}
	}

	if (!ret)
		*iova = msm_obj->domain[id].iova;

	return ret;
}

/* get iova, taking a reference.  Should have a matching put */
int msm_gem_get_iova(struct drm_gem_object *obj, int id, uint32_t *iova)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	int ret;

	/* this is safe right now because we don't unmap until the
	 * bo is deleted:
	 */
	if (msm_obj->domain[id].iova) {
		*iova = msm_obj->domain[id].iova;
		return 0;
	}

	mutex_lock(&obj->dev->struct_mutex);
	ret = msm_gem_get_iova_locked(obj, id, iova);
	mutex_unlock(&obj->dev->struct_mutex);
	return ret;
}

/* get iova without taking a reference, used in places where you have
 * already done a 'msm_gem_get_iova()'.
 */
uint32_t msm_gem_iova(struct drm_gem_object *obj, int id)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	WARN_ON(!msm_obj->domain[id].iova);
	return msm_obj->domain[id].iova;
}

void msm_gem_put_iova(struct drm_gem_object *obj, int id)
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

void *msm_gem_vaddr_locked(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	WARN_ON(!mutex_is_locked(&obj->dev->struct_mutex));
	if (!msm_obj->vaddr) {
		struct page **pages = get_pages(obj);
		if (IS_ERR(pages))
			return ERR_CAST(pages);
		msm_obj->vaddr = vmap(pages, obj->size >> PAGE_SHIFT,
				VM_MAP, pgprot_writecombine(PAGE_KERNEL));
	}
	return msm_obj->vaddr;
}

void *msm_gem_vaddr(struct drm_gem_object *obj)
{
	void *ret;
	mutex_lock(&obj->dev->struct_mutex);
	ret = msm_gem_vaddr_locked(obj);
	mutex_unlock(&obj->dev->struct_mutex);
	return ret;
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
	struct drm_device *dev = obj->dev;

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));

	msm_obj->gpu = gpu;
	if (write)
		msm_obj->write_timestamp = fence;
	else
		msm_obj->read_timestamp = fence;
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
	msm_obj->read_timestamp = 0;
	msm_obj->write_timestamp = 0;
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

#ifdef CONFIG_DEBUG_FS
void msm_gem_describe(struct drm_gem_object *obj, struct seq_file *m)
{
	struct drm_device *dev = obj->dev;
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	uint64_t off = drm_vma_node_start(&obj->vma_node);

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));
	seq_printf(m, "%08x: %c(r=%u,w=%u) %2d (%2d) %08llx %pK %zu\n",
			msm_obj->flags, is_active(msm_obj) ? 'A' : 'I',
			msm_obj->read_timestamp, msm_obj->write_timestamp,
			obj->name, obj->refcount.refcount.counter,
			off, msm_obj->vaddr, obj->size);
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
	struct msm_drm_private *priv = obj->dev->dev_private;
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	int id;

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));

	/* object should not be on active list: */
	WARN_ON(is_active(msm_obj));

	list_del(&msm_obj->mm_list);

	for (id = 0; id < ARRAY_SIZE(msm_obj->domain); id++) {
		struct msm_mmu *mmu = priv->mmus[id];
		if (mmu && msm_obj->domain[id].iova) {
			if (obj->import_attach && mmu->funcs->unmap_dma_buf) {
				mmu->funcs->unmap_dma_buf(mmu,
						msm_obj->domain[id].sgt,
						obj->import_attach->dmabuf,
						DMA_BIDIRECTIONAL);
			} else if (!use_pages(obj)) {
				uint32_t offset = msm_obj->domain[id].iova;

				mmu->funcs->unmap(mmu, offset,
					msm_obj->domain[id].sgt);
			} else {
				dma_unmap_sg(mmu->dev,
					msm_obj->domain[id].sgt->sgl,
					msm_obj->domain[id].sgt->nents,
					DMA_BIDIRECTIONAL);
			}
			msm_obj->domain[id].iova = 0;
		}

		if (msm_obj->domain[id].sgt) {
			sg_free_table(msm_obj->domain[id].sgt);
			kfree(msm_obj->domain[id].sgt);
			msm_obj->domain[id].sgt = NULL;
		}
	}

	if (obj->import_attach) {
		if (msm_obj->vaddr)
			dma_buf_vunmap(obj->import_attach->dmabuf,
					msm_obj->vaddr);

		/* Don't drop the pages for imported dmabuf, as they are not
		 * ours, just free the array we allocated:
		 */
		if (msm_obj->pages) {
			drm_free_large(msm_obj->pages);
			msm_obj->pages = NULL;
		}

		drm_prime_gem_destroy(obj, msm_obj->import_sgt);
	} else {
		if (msm_obj->vaddr)
			vunmap(msm_obj->vaddr);
		put_pages(obj);
	}

	if (msm_obj->resv == &msm_obj->_resv)
		reservation_object_fini(msm_obj->resv);

	drm_gem_object_release(obj);

	kfree(msm_obj);
}

/* convenience method to construct a GEM buffer object, and userspace handle */
int msm_gem_new_handle(struct drm_device *dev, struct drm_file *file,
		uint32_t size, uint32_t flags, uint32_t *handle)
{
	struct drm_gem_object *obj;
	int ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	obj = msm_gem_new(dev, size, flags);

	mutex_unlock(&dev->struct_mutex);

	if (IS_ERR(obj))
		return PTR_ERR(obj);

	ret = drm_gem_handle_create(file, obj, handle);

	/* drop reference from allocate - handle holds it now */
	drm_gem_object_unreference_unlocked(obj);

	return ret;
}

static int msm_gem_new_impl(struct drm_device *dev,
		uint32_t size, uint32_t flags,
		struct drm_gem_object **obj)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_gem_object *msm_obj;
	unsigned sz;
	bool use_vram = false;

	switch (flags & MSM_BO_CACHE_MASK) {
	case MSM_BO_UNCACHED:
	case MSM_BO_CACHED:
	case MSM_BO_WC:
		break;
	default:
		DRM_ERROR("invalid cache flag: %x\n",
				(flags & MSM_BO_CACHE_MASK));
		return -EINVAL;
	}

	if (!iommu_present(&platform_bus_type))
		use_vram = true;
	else if ((flags & MSM_BO_STOLEN) && priv->vram.size)
		use_vram = true;

	if (WARN_ON(use_vram && !priv->vram.size))
		return -EINVAL;

	sz = sizeof(*msm_obj);
	if (use_vram)
		sz += sizeof(struct drm_mm_node);

	msm_obj = kzalloc(sz, GFP_KERNEL);
	if (!msm_obj)
		return -ENOMEM;

	if (use_vram)
		msm_obj->vram_node = (void *)&msm_obj[1];

	msm_obj->flags = flags;

	msm_obj->resv = &msm_obj->_resv;
	reservation_object_init(msm_obj->resv);

	if (use_vram) {
		/* Update start page index */
		struct msm_gem_object *pos;
		dma_addr_t start = 0;

		list_for_each_entry(pos, &priv->inactive_list, mm_list) {
			struct drm_gem_object *gem_obj = &pos->base;
			struct drm_mm_node *vram_node = pos->vram_node;

			if (vram_node)
				start += vram_node->start;
			start += (gem_obj->size >> PAGE_SHIFT);
		}
		msm_obj->vram_node->start = start;
	}

	INIT_LIST_HEAD(&msm_obj->submit_entry);
	list_add_tail(&msm_obj->mm_list, &priv->inactive_list);

	*obj = &msm_obj->base;

	return 0;
}

struct drm_gem_object *msm_gem_new(struct drm_device *dev,
		uint32_t size, uint32_t flags)
{
	struct drm_gem_object *obj = NULL;
	int ret;

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));

	size = PAGE_ALIGN(size);

	ret = msm_gem_new_impl(dev, size, flags, &obj);
	if (ret)
		goto fail;

	if (use_pages(obj)) {
		ret = drm_gem_object_init(dev, obj, size);
		if (ret)
			goto fail;
	} else {
		drm_gem_private_object_init(dev, obj, size);
	}

	return obj;

fail:
	if (obj)
		drm_gem_object_unreference(obj);

	return ERR_PTR(ret);
}

struct drm_gem_object *msm_gem_import(struct drm_device *dev,
		uint32_t size, struct sg_table *sgt)
{
	struct msm_gem_object *msm_obj;
	struct drm_gem_object *obj;
	int ret, npages;

	/* if we don't have IOMMU, don't bother pretending we can import: */
	if (!iommu_present(&platform_bus_type)) {
		DRM_ERROR("cannot import without IOMMU\n");
		return ERR_PTR(-EINVAL);
	}

	size = PAGE_ALIGN(size);

	mutex_lock(&dev->struct_mutex);
	ret = msm_gem_new_impl(dev, size, MSM_BO_WC, &obj);
	mutex_unlock(&dev->struct_mutex);
	if (ret)
		goto fail;

	drm_gem_private_object_init(dev, obj, size);

	npages = size / PAGE_SIZE;

	msm_obj = to_msm_bo(obj);
	msm_obj->import_sgt = sgt;
	msm_obj->pages = drm_malloc_ab(npages, sizeof(struct page *));
	if (!msm_obj->pages) {
		ret = -ENOMEM;
		goto fail;
	}

	ret = drm_prime_sg_to_page_addr_arrays(sgt, msm_obj->pages, NULL,
						 npages);
	if (ret)
		goto fail;

	return obj;

fail:
	if (obj)
		drm_gem_object_unreference_unlocked(obj);

	return ERR_PTR(ret);
}
