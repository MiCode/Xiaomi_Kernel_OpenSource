/*
 *
 * (C) COPYRIGHT 2012-2015 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



/**
 * pl111_drm_vma.c
 * Implementation of the VM functions for PL111 DRM
 */
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/version.h>
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include <linux/module.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include "pl111_drm.h"

/* BEGIN drivers/staging/omapdrm/omap_gem_helpers.c */
/**
 * drm_gem_put_pages - helper to free backing pages for a GEM object
 * @obj: obj in question
 * @pages: pages to free
 */
static void _drm_gem_put_pages(struct drm_gem_object *obj, struct page **pages,
				bool dirty, bool accessed)
{
	int i, npages;
	struct pl111_gem_bo *bo;
	npages = obj->size >> PAGE_SHIFT;
	bo = PL111_BO_FROM_GEM(obj);
	for (i = 0; i < npages; i++) {
		if (dirty)
			set_page_dirty(pages[i]);
		if (accessed)
			mark_page_accessed(pages[i]);
		/* Undo the reference we took when populating the table */
		page_cache_release(pages[i]);
	}
	drm_free_large(pages);
}

void put_pages(struct drm_gem_object *obj, struct page **pages)
{
	int i, npages;
	struct pl111_gem_bo *bo;
	npages = obj->size >> PAGE_SHIFT;
	bo = PL111_BO_FROM_GEM(obj);
	_drm_gem_put_pages(obj, pages, true, true);
	if (bo->backing_data.shm.dma_addrs) {
		for (i = 0; i < npages; i++) {
			/* Filter pages unmapped because of CPU accesses */
			if (!bo->backing_data.shm.dma_addrs[i])
				continue;
			if (!dma_mapping_error(obj->dev->dev,
					bo->backing_data.shm.dma_addrs[i])) {
				dma_unmap_page(obj->dev->dev,
					bo->backing_data.shm.dma_addrs[i],
					PAGE_SIZE,
					DMA_BIDIRECTIONAL);
			}
		}
		kfree(bo->backing_data.shm.dma_addrs);
		bo->backing_data.shm.dma_addrs = NULL;
	}
}

/**
 * drm_gem_get_pages - helper to allocate backing pages for a GEM object
 * @obj: obj in question
 * @gfpmask: gfp mask of requested pages
 */
static struct page **_drm_gem_get_pages(struct drm_gem_object *obj,
					gfp_t gfpmask)
{
	struct inode *inode;
	struct address_space *mapping;
	struct page *p, **pages;
	int i, npages;

	/* This is the shared memory object that backs the GEM resource */
	inode = obj->filp->f_path.dentry->d_inode;
	mapping = inode->i_mapping;

	npages = obj->size >> PAGE_SHIFT;

	pages = drm_malloc_ab(npages, sizeof(struct page *));
	if (pages == NULL)
		return ERR_PTR(-ENOMEM);

	gfpmask |= mapping_gfp_mask(mapping);

	for (i = 0; i < npages; i++) {
		p = shmem_read_mapping_page_gfp(mapping, i, gfpmask);
		if (IS_ERR(p))
			goto fail;
		pages[i] = p;

		/*
		 * There is a hypothetical issue w/ drivers that require
		 * buffer memory in the low 4GB.. if the pages are un-
		 * pinned, and swapped out, they can end up swapped back
		 * in above 4GB.  If pages are already in memory, then
		 * shmem_read_mapping_page_gfp will ignore the gfpmask,
		 * even if the already in-memory page disobeys the mask.
		 *
		 * It is only a theoretical issue today, because none of
		 * the devices with this limitation can be populated with
		 * enough memory to trigger the issue.  But this BUG_ON()
		 * is here as a reminder in case the problem with
		 * shmem_read_mapping_page_gfp() isn't solved by the time
		 * it does become a real issue.
		 *
		 * See this thread: http://lkml.org/lkml/2011/7/11/238
		 */
		BUG_ON((gfpmask & __GFP_DMA32) &&
			(page_to_pfn(p) >= 0x00100000UL));
	}

	return pages;

fail:
	while (i--)
		page_cache_release(pages[i]);

	drm_free_large(pages);
	return ERR_PTR(PTR_ERR(p));
}

struct page **get_pages(struct drm_gem_object *obj)
{
	struct pl111_gem_bo *bo;
	bo = PL111_BO_FROM_GEM(obj);

	if (bo->backing_data.shm.pages == NULL) {
		struct page **p;
		int npages = obj->size >> PAGE_SHIFT;
		int i;

		p = _drm_gem_get_pages(obj, GFP_KERNEL);
		if (IS_ERR(p))
			return ERR_PTR(-ENOMEM);

		bo->backing_data.shm.pages = p;

		if (bo->backing_data.shm.dma_addrs == NULL) {
			bo->backing_data.shm.dma_addrs =
				kzalloc(npages * sizeof(dma_addr_t),
					GFP_KERNEL);
			if (bo->backing_data.shm.dma_addrs == NULL)
				goto error_out;
		}

		if (!(bo->type & PL111_BOT_CACHED)) {
			for (i = 0; i < npages; ++i) {
				bo->backing_data.shm.dma_addrs[i] =
					dma_map_page(obj->dev->dev, p[i], 0, PAGE_SIZE,
						DMA_BIDIRECTIONAL);
				if (dma_mapping_error(obj->dev->dev,
						bo->backing_data.shm.dma_addrs[i]))
					goto error_out;
			}
		}
	}

	return bo->backing_data.shm.pages;

error_out:
	put_pages(obj, bo->backing_data.shm.pages);
	bo->backing_data.shm.pages = NULL;
	return ERR_PTR(-ENOMEM);
}

/* END drivers/staging/omapdrm/omap_gem_helpers.c */
int pl111_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page **pages;
	unsigned long pfn;
	struct drm_gem_object *obj = vma->vm_private_data;
	struct pl111_gem_bo *bo = PL111_BO_FROM_GEM(obj);
	struct drm_device *dev = obj->dev;
	int ret;

	mutex_lock(&dev->struct_mutex);

	/*
	 * Our mmap calls setup a valid vma->vm_pgoff
	 * so we can use vmf->pgoff
	 */

	if (bo->type & PL111_BOT_DMA) {
		pfn = (bo->backing_data.dma.fb_dev_addr >> PAGE_SHIFT) +
				vmf->pgoff;
	} else { /* PL111_BOT_SHM */
		pages = get_pages(obj);
		if (IS_ERR(pages)) {
			dev_err(obj->dev->dev,
				"could not get pages: %ld\n", PTR_ERR(pages));
			ret = PTR_ERR(pages);
			goto error;
		}
		pfn = page_to_pfn(pages[vmf->pgoff]);
		pl111_gem_sync_to_cpu(bo, vmf->pgoff);
	}

	DRM_DEBUG("bo=%p physaddr=0x%.8x for offset 0x%x\n",
				bo, PFN_PHYS(pfn), PFN_PHYS(vmf->pgoff));

	ret = vm_insert_mixed(vma, (unsigned long)vmf->virtual_address, pfn);

error:
	mutex_unlock(&dev->struct_mutex);

	switch (ret) {
	case 0:
	case -ERESTARTSYS:
	case -EINTR:
	case -EBUSY:
		return VM_FAULT_NOPAGE;
	case -ENOMEM:
		return VM_FAULT_OOM;
	default:
		return VM_FAULT_SIGBUS;
	}
}

/*
 * The core drm_vm_ functions in kernel 3.4 are not ready
 * to handle dma_buf cases where vma->vm_file->private_data
 * cannot be accessed to get the device.
 * 
 * We use these functions from 3.5 instead where the device
 * pointer is passed explicitly.
 *
 * However they aren't exported from the kernel until 3.10
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
void pl111_drm_vm_open_locked(struct drm_device *dev,
                struct vm_area_struct *vma)
{
	struct drm_vma_entry *vma_entry;

	DRM_DEBUG("0x%08lx,0x%08lx\n",
		vma->vm_start, vma->vm_end - vma->vm_start);
	atomic_inc(&dev->vma_count);

	vma_entry = kmalloc(sizeof(*vma_entry), GFP_KERNEL);
	if (vma_entry) {
		vma_entry->vma = vma;
		vma_entry->pid = current->pid;
		list_add(&vma_entry->head, &dev->vmalist);
	}
}

void pl111_drm_vm_close_locked(struct drm_device *dev,
                struct vm_area_struct *vma)
{
	struct drm_vma_entry *pt, *temp;

	DRM_DEBUG("0x%08lx,0x%08lx\n",
		vma->vm_start, vma->vm_end - vma->vm_start);
	atomic_dec(&dev->vma_count);

	list_for_each_entry_safe(pt, temp, &dev->vmalist, head) {
		if (pt->vma == vma) {
			list_del(&pt->head);
			kfree(pt);
			break;
		}
	}
}

void pl111_gem_vm_open(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;

	drm_gem_object_reference(obj);

	mutex_lock(&obj->dev->struct_mutex);
	pl111_drm_vm_open_locked(obj->dev, vma);
	mutex_unlock(&obj->dev->struct_mutex);
}

void pl111_gem_vm_close(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct drm_device *dev = obj->dev;

	mutex_lock(&dev->struct_mutex);
	pl111_drm_vm_close_locked(obj->dev, vma);
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
}
#endif
