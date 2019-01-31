/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
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
 * pl111_drm_gem.c
 * Implementation of the GEM functions for PL111 DRM
 */
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/version.h>
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <asm/cacheflush.h>
#include <asm/outercache.h>
#include "pl111_drm.h"

void pl111_gem_free_object(struct drm_gem_object *obj)
{
	struct pl111_gem_bo *bo;
	struct drm_device *dev = obj->dev;
	DRM_DEBUG_KMS("DRM %s on drm_gem_object=%p\n", __func__, obj);

	bo = PL111_BO_FROM_GEM(obj);

	if (obj->import_attach)
		drm_prime_gem_destroy(obj, bo->sgt);

	if (obj->map_list.map != NULL)
		drm_gem_free_mmap_offset(obj);

	/*
	 * Only free the backing memory if the object has not been imported.
	 * If it has been imported, the exporter is in charge to free that
	 * once dmabuf's refcount becomes 0.
	 */
	if (obj->import_attach)
		goto imported_out;

	if (bo->type & PL111_BOT_DMA) {
		dma_free_writecombine(dev->dev, obj->size,
				bo->backing_data.dma.fb_cpu_addr,
				bo->backing_data.dma.fb_dev_addr);
	} else if (bo->backing_data.shm.pages != NULL) {
		put_pages(obj, bo->backing_data.shm.pages);
	}

imported_out:
	drm_gem_object_release(obj);

	kfree(bo);

	DRM_DEBUG_KMS("Destroyed dumb_bo handle 0x%p\n", bo);
}

static int pl111_gem_object_create(struct drm_device *dev, u64 size,
				   u32 flags, struct drm_file *file_priv,
				   u32 *handle)
{
	int ret = 0;
	struct pl111_gem_bo *bo = NULL;

	bo = kzalloc(sizeof(*bo), GFP_KERNEL);
	if (bo == NULL) {
		ret = -ENOMEM;
		goto finish;
	}

	bo->type = flags;

#ifndef ARCH_HAS_SG_CHAIN
	/*
	 * If the ARCH can't chain we can't have non-contiguous allocs larger
	 * than a single sg can hold.
	 * In this case we fall back to using contiguous memory
	 */
	if (!(bo->type & PL111_BOT_DMA)) {
		long unsigned int n_pages =
				PAGE_ALIGN(size) >> PAGE_SHIFT;
		if (n_pages > SG_MAX_SINGLE_ALLOC) {
			bo->type |= PL111_BOT_DMA;
			/*
			 * Non-contiguous allocation request changed to
			 * contigous
			 */
			DRM_INFO("non-contig alloc to contig %lu > %lu pages.",
					n_pages, SG_MAX_SINGLE_ALLOC);
		}
	}
#endif
	if (bo->type & PL111_BOT_DMA) {
		/* scanout compatible - use physically contiguous buffer */
		bo->backing_data.dma.fb_cpu_addr =
			dma_alloc_writecombine(dev->dev, size,
					&bo->backing_data.dma.fb_dev_addr,
					GFP_KERNEL);
		if (bo->backing_data.dma.fb_cpu_addr == NULL) {
			DRM_ERROR("dma_alloc_writecombine failed\n");
			ret = -ENOMEM;
			goto free_bo;
		}

		ret = drm_gem_private_object_init(dev, &bo->gem_object,
							size);
		if (ret != 0) {
			DRM_ERROR("DRM could not initialise GEM object\n");
			goto free_dma;
		}
	} else { /* PL111_BOT_SHM */
		/* not scanout compatible - use SHM backed object */
		ret = drm_gem_object_init(dev, &bo->gem_object, size);
		if (ret != 0) {
			DRM_ERROR("DRM could not init SHM backed GEM obj\n");
			ret = -ENOMEM;
			goto free_bo;
		}
		DRM_DEBUG_KMS("Num bytes: %d\n", bo->gem_object.size);
	}

	DRM_DEBUG("s=%llu, flags=0x%x, %s 0x%.8lx, type=%d\n",
		size, flags,
		(bo->type & PL111_BOT_DMA) ? "physaddr" : "shared page array",
		(bo->type & PL111_BOT_DMA) ?
			(unsigned long)bo->backing_data.dma.fb_dev_addr:
			(unsigned long)bo->backing_data.shm.pages,
			bo->type);

	ret = drm_gem_handle_create(file_priv, &bo->gem_object, handle);
	if (ret != 0) {
		DRM_ERROR("DRM failed to create GEM handle\n");
		goto obj_release;
	}

	/* drop reference from allocate - handle holds it now */
	drm_gem_object_unreference_unlocked(&bo->gem_object);
	
	return 0;

obj_release:
	drm_gem_object_release(&bo->gem_object);
free_dma:
	if (bo->type & PL111_BOT_DMA)
		dma_free_writecombine(dev->dev, size,
			bo->backing_data.dma.fb_cpu_addr,
			bo->backing_data.dma.fb_dev_addr);
free_bo:
	kfree(bo);
finish:
	return ret;
}

int pl111_drm_gem_create_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv)
{
	struct drm_pl111_gem_create *args = data;
	uint32_t bytes_pp;

	/* Round bpp up, to allow for case where bpp<8 */
	bytes_pp = args->bpp >> 3;
	if (args->bpp & ((1 << 3) - 1))
		bytes_pp++;

	if (args->flags & ~PL111_BOT_MASK) {
		DRM_ERROR("wrong flags: 0x%x\n", args->flags);
		return -EINVAL;
	}

	args->pitch = ALIGN(args->width * bytes_pp, 64);
	args->size = PAGE_ALIGN(args->pitch * args->height);

	DRM_DEBUG_KMS("gem_create w=%d h=%d p=%d bpp=%d b=%d s=%llu f=0x%x\n",
			args->width, args->height, args->pitch, args->bpp,
			bytes_pp, args->size, args->flags);

	return pl111_gem_object_create(dev, args->size, args->flags, file_priv,
					&args->handle);
}

int pl111_dumb_create(struct drm_file *file_priv,
		struct drm_device *dev, struct drm_mode_create_dumb *args)
{
	uint32_t bytes_pp;

	/* Round bpp up, to allow for case where bpp<8 */
	bytes_pp = args->bpp >> 3;
	if (args->bpp & ((1 << 3) - 1))
		bytes_pp++;

	if (args->flags) {
		DRM_ERROR("flags must be zero: 0x%x\n", args->flags);
		return -EINVAL;
	}

	args->pitch = ALIGN(args->width * bytes_pp, 64);
	args->size = PAGE_ALIGN(args->pitch * args->height);

	DRM_DEBUG_KMS("dumb_create w=%d h=%d p=%d bpp=%d b=%d s=%llu f=0x%x\n",
			args->width, args->height, args->pitch, args->bpp,
			bytes_pp, args->size, args->flags);

	return pl111_gem_object_create(dev, args->size,
				       PL111_BOT_DMA | PL111_BOT_UNCACHED,
				       file_priv, &args->handle);
}

int pl111_dumb_destroy(struct drm_file *file_priv, struct drm_device *dev,
		uint32_t handle)
{
	DRM_DEBUG_KMS("DRM %s on file_priv=%p handle=0x%.8x\n", __func__,
			file_priv, handle);
	return drm_gem_handle_delete(file_priv, handle);
}

int pl111_dumb_map_offset(struct drm_file *file_priv,
			struct drm_device *dev, uint32_t handle,
			uint64_t *offset)
{
	struct drm_gem_object *obj;
	int ret = 0;
	DRM_DEBUG_KMS("DRM %s on file_priv=%p handle=0x%.8x\n", __func__,
			file_priv, handle);

	/* GEM does all our handle to object mapping */
	obj = drm_gem_object_lookup(dev, file_priv, handle);
	if (obj == NULL) {
		ret = -ENOENT;
		goto fail;
	}

	if (obj->map_list.map == NULL) {
		ret = drm_gem_create_mmap_offset(obj);
		if (ret != 0) {
			drm_gem_object_unreference_unlocked(obj);
			goto fail;
		}
	}

	*offset = (uint64_t) obj->map_list.hash.key << PAGE_SHIFT;

	drm_gem_object_unreference_unlocked(obj);
fail:
	return ret;
}

/* sync the buffer for DMA access */
void pl111_gem_sync_to_dma(struct pl111_gem_bo *bo)
{
	struct drm_device *dev = bo->gem_object.dev;

	if (!(bo->type & PL111_BOT_DMA) && (bo->type & PL111_BOT_CACHED)) {
		int i, npages = bo->gem_object.size >> PAGE_SHIFT;
		struct page **pages = bo->backing_data.shm.pages;
		bool dirty = false;

		for (i = 0; i < npages; i++) {
			if (!bo->backing_data.shm.dma_addrs[i]) {
				DRM_DEBUG("%s: dma map page=%d bo=%p\n", __func__, i, bo);
				 bo->backing_data.shm.dma_addrs[i] =
					dma_map_page(dev->dev, pages[i], 0,
					PAGE_SIZE, DMA_BIDIRECTIONAL);
				dirty = true;
			}
		}

		if (dirty) {
			DRM_DEBUG("%s: zap ptes (and flush cache) bo=%p\n", __func__, bo);
			/*
			 * TODO MIDEGL-1813
			 * 
			 * Use flush_cache_page() and outer_flush_range() to
			 * flush only the user space mappings of the dirty pages
			 */
			flush_cache_all();
			outer_flush_all();
			unmap_mapping_range(bo->gem_object.filp->f_mapping, 0,
					bo->gem_object.size, 1);
		}
	}
}

void pl111_gem_sync_to_cpu(struct pl111_gem_bo *bo, int pgoff)
{
	struct drm_device *dev = bo->gem_object.dev;

	/*
	 * TODO MIDEGL-1808
	 * 
	 * The following check was meant to detect if the CPU is trying to access
	 * a buffer that is currently mapped for DMA accesses, which is illegal
	 * as described by the DMA-API.
	 * 
	 * However, some of our tests are trying to do that, which triggers the message
	 * below and avoids dma-unmapping the pages not to annoy the DMA device but that
	 * leads to the test failing because of caches not being properly flushed.
	 */

	/*
	if (bo->sgt) {
		DRM_ERROR("%s: the CPU is trying to access a dma-mapped buffer\n",  __func__);
		return;
	}
	*/

	if (!(bo->type & PL111_BOT_DMA) && (bo->type & PL111_BOT_CACHED) &&
	     bo->backing_data.shm.dma_addrs[pgoff]) {
		DRM_DEBUG("%s: unmap bo=%p (s=%d), paddr=%08x\n",
			  __func__, bo,  bo->gem_object.size,
			  bo->backing_data.shm.dma_addrs[pgoff]);
		dma_unmap_page(dev->dev, bo->backing_data.shm.dma_addrs[pgoff],
				PAGE_SIZE, DMA_BIDIRECTIONAL);
		bo->backing_data.shm.dma_addrs[pgoff] = 0;
	}
}

/* Based on omapdrm driver */
int pl111_bo_mmap(struct drm_gem_object *obj, struct pl111_gem_bo *bo,
		 struct vm_area_struct *vma, size_t size)
{
	DRM_DEBUG("DRM %s on drm_gem_object=%p, pl111_gem_bo=%p type=%08x\n",
			__func__, obj, bo, bo->type);

	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_flags |= VM_MIXEDMAP;

	if (bo->type & PL111_BOT_WC) {
		vma->vm_page_prot =
			pgprot_writecombine(vm_get_page_prot(vma->vm_flags));
	} else if (bo->type & PL111_BOT_CACHED) {
		/*
		 * Objects that do not have a filp (DMA backed) can't be
		 * mapped as cached now. Write-combine should be enough.
		 */
		if (WARN_ON(!obj->filp))
			return -EINVAL;

		/*
		 * As explained in Documentation/dma-buf-sharing.txt
		 * we need this trick so that we can manually zap ptes
		 * in order to fake coherency.
		 */
		fput(vma->vm_file);
		vma->vm_pgoff = 0;
		get_file(obj->filp);
		vma->vm_file = obj->filp;

		vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	} else { /* PL111_BOT_UNCACHED */
		vma->vm_page_prot =
			pgprot_noncached(vm_get_page_prot(vma->vm_flags));
	}
	return 0;
}

int pl111_gem_mmap(struct file *file_priv, struct vm_area_struct *vma)
{
	int ret;
	struct drm_file *priv = file_priv->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_gem_mm *mm = dev->mm_private;
	struct drm_local_map *map = NULL;
	struct drm_hash_item *hash;
	struct drm_gem_object *obj;
	struct pl111_gem_bo *bo;

	DRM_DEBUG_KMS("DRM %s\n", __func__);

	drm_ht_find_item(&mm->offset_hash, vma->vm_pgoff, &hash);
	map = drm_hash_entry(hash, struct drm_map_list, hash)->map;
	obj = map->handle;
	bo = PL111_BO_FROM_GEM(obj);

	DRM_DEBUG_KMS("DRM %s on pl111_gem_bo %p bo->type 0x%08x\n", __func__, bo, bo->type);

	/* for an imported buffer we let the exporter handle the mmap */
	if (obj->import_attach)
		return dma_buf_mmap(obj->import_attach->dmabuf, vma, 0);

	ret = drm_gem_mmap(file_priv, vma);
	if (ret < 0) {
		DRM_ERROR("failed to mmap\n");
		return ret;
	}

	return pl111_bo_mmap(obj, bo, vma, vma->vm_end - vma->vm_start);
}
