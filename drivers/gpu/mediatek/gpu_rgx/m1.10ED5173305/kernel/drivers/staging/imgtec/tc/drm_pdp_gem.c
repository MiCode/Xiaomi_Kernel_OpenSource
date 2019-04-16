/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/dma-buf.h>
#include <linux/reservation.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/capability.h>

#include <drm/drm_mm.h>

#if defined(SUPPORT_PLATO_DISPLAY)
#include "plato_drv.h"
#else
#include "tc_drv.h"
#endif

#include "drm_pdp_drv.h"
#include "drm_pdp_gem.h"
#include "pdp_drm.h"
#include "kernel_compatibility.h"

struct pdp_gem_object {
	struct drm_gem_object base;

	/* Non-null if backing originated from this driver */
	struct drm_mm_node *vram;

	/* Non-null if backing was imported */
	struct sg_table *sgt;

	phys_addr_t cpu_addr;
	dma_addr_t dev_addr;

	struct reservation_object _resv;
	struct reservation_object *resv;

	bool cpu_prep;
};

#define to_pdp_obj(obj) container_of(obj, struct pdp_gem_object, base)

#if defined(SUPPORT_PLATO_DISPLAY)
	typedef struct plato_pdp_platform_data pdp_gem_platform_data;
#else
	typedef struct tc_pdp_platform_data pdp_gem_platform_data;
#endif

struct pdp_gem_private {
	struct mutex			vram_lock;
	struct				drm_mm vram;
};

static struct pdp_gem_object *
pdp_gem_private_object_create(struct drm_device *dev,
			      size_t size)
{
	struct pdp_gem_object *pdp_obj;

	WARN_ON(PAGE_ALIGN(size) != size);

	pdp_obj = kzalloc(sizeof(*pdp_obj), GFP_KERNEL);
	if (!pdp_obj)
		return ERR_PTR(-ENOMEM);

	drm_gem_private_object_init(dev, &pdp_obj->base, size);
	reservation_object_init(&pdp_obj->_resv);

	return pdp_obj;
}

static struct drm_gem_object *pdp_gem_object_create(struct drm_device *dev,
					struct pdp_gem_private *gem_priv,
					size_t size,
					u32 flags)
{
	pdp_gem_platform_data *pdata =
		to_platform_device(dev->dev)->dev.platform_data;
	struct pdp_gem_object *pdp_obj;
	struct drm_mm_node *node;
	int err = 0;

	pdp_obj = pdp_gem_private_object_create(dev, size);
	if (!pdp_obj) {
		err = -ENOMEM;
		goto err_exit;
	}

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		err = -ENOMEM;
		goto err_unref;
	}

	mutex_lock(&gem_priv->vram_lock);
	err = drm_mm_insert_node(&gem_priv->vram, node, size);
	mutex_unlock(&gem_priv->vram_lock);
	if (err)
		goto err_free_node;

	pdp_obj->vram = node;
	pdp_obj->dev_addr = pdp_obj->vram->start;
	pdp_obj->cpu_addr = pdata->memory_base + pdp_obj->dev_addr;
	pdp_obj->resv = &pdp_obj->_resv;

	return &pdp_obj->base;

err_free_node:
	kfree(node);
err_unref:
	drm_gem_object_put_unlocked(&pdp_obj->base);
err_exit:
	return ERR_PTR(err);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
int pdp_gem_object_vm_fault(struct vm_fault *vmf)
#else
int pdp_gem_object_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
#endif
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
	struct vm_area_struct *vma = vmf->vma;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
	unsigned long addr = vmf->address;
#else
	unsigned long addr = (unsigned long)vmf->virtual_address;
#endif
	struct drm_gem_object *obj = vma->vm_private_data;
	struct pdp_gem_object *pdp_obj = to_pdp_obj(obj);
	unsigned long off;
	unsigned long pfn;
	int err;

	off = addr - vma->vm_start;
	pfn = (pdp_obj->cpu_addr + off) >> PAGE_SHIFT;

	err = vm_insert_pfn(vma, addr, pfn);
	switch (err) {
	case 0:
	case -EBUSY:
		return VM_FAULT_NOPAGE;
	case -ENOMEM:
		return VM_FAULT_OOM;
	default:
		return VM_FAULT_SIGBUS;
	}
}

void pdp_gem_object_free_priv(struct pdp_gem_private *gem_priv,
			      struct drm_gem_object *obj)
{
	struct pdp_gem_object *pdp_obj = to_pdp_obj(obj);

	drm_gem_free_mmap_offset(obj);

	if (&pdp_obj->_resv == pdp_obj->resv)
		reservation_object_fini(pdp_obj->resv);

	if (pdp_obj->vram) {
		mutex_lock(&gem_priv->vram_lock);
		drm_mm_remove_node(pdp_obj->vram);
		mutex_unlock(&gem_priv->vram_lock);

		kfree(pdp_obj->vram);
	} else if (obj->import_attach) {
		drm_prime_gem_destroy(obj, pdp_obj->sgt);
	}

	drm_gem_object_release(&pdp_obj->base);
	kfree(pdp_obj);
}

static int pdp_gem_prime_attach(struct dma_buf *dma_buf,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
				struct device *dev,
#endif
				struct dma_buf_attachment *attach)
{
	struct drm_gem_object *obj = dma_buf->priv;

	/* Restrict access to Rogue */
	if (WARN_ON(!obj->dev->dev->parent) ||
	    obj->dev->dev->parent != attach->dev->parent)
		return -EPERM;

	return 0;
}

static struct sg_table *
pdp_gem_prime_map_dma_buf(struct dma_buf_attachment *attach,
			  enum dma_data_direction dir)
{
	struct drm_gem_object *obj = attach->dmabuf->priv;
	struct pdp_gem_object *pdp_obj = to_pdp_obj(obj);
	struct sg_table *sgt;

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return NULL;

	if (sg_alloc_table(sgt, 1, GFP_KERNEL))
		goto err_free_sgt;

	sg_dma_address(sgt->sgl) = pdp_obj->dev_addr;
	sg_dma_len(sgt->sgl) = obj->size;

	return sgt;

err_free_sgt:
	kfree(sgt);
	return NULL;
}

static void pdp_gem_prime_unmap_dma_buf(struct dma_buf_attachment *attach,
					struct sg_table *sgt,
					enum dma_data_direction dir)
{
	sg_free_table(sgt);
	kfree(sgt);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
static void *pdp_gem_prime_kmap_atomic(struct dma_buf *dma_buf,
				       unsigned long page_num)
{
	return NULL;
}
#endif

static void *pdp_gem_prime_kmap(struct dma_buf *dma_buf,
				unsigned long page_num)
{
	return NULL;
}

static int pdp_gem_prime_mmap(struct dma_buf *dma_buf,
			      struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = dma_buf->priv;
	int err;

	mutex_lock(&obj->dev->struct_mutex);
	err = drm_gem_mmap_obj(obj, obj->size, vma);
	mutex_unlock(&obj->dev->struct_mutex);

	return err;
}

#if defined(CONFIG_X86)
static void *pdp_gem_prime_vmap(struct dma_buf *dma_buf)
{
	struct drm_gem_object *obj = dma_buf->priv;
	struct pdp_gem_object *pdp_obj = to_pdp_obj(obj);
	void *vaddr;

	mutex_lock(&obj->dev->struct_mutex);

	/*
	 * On x86 platforms, the pointer returned by ioremap can be dereferenced
	 * directly. As such, explicitly cast away the __ioremap qualifier.
	 */
	vaddr = (void __force *)ioremap(pdp_obj->cpu_addr, obj->size);
	if (vaddr == NULL)
		DRM_DEBUG_DRIVER("ioremap failed");

	mutex_unlock(&obj->dev->struct_mutex);

	return vaddr;
}

static void pdp_gem_prime_vunmap(struct dma_buf *dma_buf, void *vaddr)
{
	struct drm_gem_object *obj = dma_buf->priv;

	mutex_lock(&obj->dev->struct_mutex);
	iounmap((void __iomem *)vaddr);
	mutex_unlock(&obj->dev->struct_mutex);
}
#endif

static const struct dma_buf_ops pdp_gem_prime_dmabuf_ops = {
	.attach		= pdp_gem_prime_attach,
	.map_dma_buf	= pdp_gem_prime_map_dma_buf,
	.unmap_dma_buf	= pdp_gem_prime_unmap_dma_buf,
	.release	= drm_gem_dmabuf_release,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
	.map_atomic	= pdp_gem_prime_kmap_atomic,
#endif
	.map		= pdp_gem_prime_kmap,
#else
	.kmap_atomic	= pdp_gem_prime_kmap_atomic,
	.kmap		= pdp_gem_prime_kmap,
#endif
	.mmap		= pdp_gem_prime_mmap,
#if defined(CONFIG_X86)
	.vmap		= pdp_gem_prime_vmap,
	.vunmap		= pdp_gem_prime_vunmap
#endif
};


static int
pdp_gem_lookup_our_object(struct drm_file *file, u32 handle,
			  struct drm_gem_object **objp)

{
	struct drm_gem_object *obj;

	obj = drm_gem_object_lookup(file, handle);
	if (!obj)
		return -ENOENT;

	if (obj->import_attach) {
		/*
		 * The dmabuf associated with the object is not one of
		 * ours. Our own buffers are handled differently on import.
		 */
		drm_gem_object_put_unlocked(obj);
		return -EINVAL;
	}

	*objp = obj;
	return 0;
}

struct dma_buf *pdp_gem_prime_export(struct drm_device *dev,
				     struct drm_gem_object *obj,
				     int flags)
{
	struct pdp_gem_object *pdp_obj = to_pdp_obj(obj);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
	DEFINE_DMA_BUF_EXPORT_INFO(export_info);

	export_info.ops = &pdp_gem_prime_dmabuf_ops;
	export_info.size = obj->size;
	export_info.flags = flags;
	export_info.resv = pdp_obj->resv;
	export_info.priv = obj;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
	return drm_gem_dmabuf_export(dev, &export_info);
#else
	return dma_buf_export(&export_info);
#endif
#else
	return dma_buf_export(obj, &pdp_gem_prime_dmabuf_ops, obj->size,
			      flags, pdp_obj->resv);
#endif
}

struct drm_gem_object *
pdp_gem_prime_import(struct drm_device *dev,
		     struct dma_buf *dma_buf)
{
	struct drm_gem_object *obj = dma_buf->priv;

	if (obj->dev == dev) {
		BUG_ON(dma_buf->ops != &pdp_gem_prime_dmabuf_ops);

		/*
		 * The dmabuf is one of ours, so return the associated
		 * PDP GEM object, rather than create a new one.
		 */
		drm_gem_object_get(obj);

		return obj;
	}

	return drm_gem_prime_import(dev, dma_buf);
}

struct drm_gem_object *
pdp_gem_prime_import_sg_table(struct drm_device *dev,
			      struct dma_buf_attachment *attach,
			      struct sg_table *sgt)
{
	pdp_gem_platform_data *pdata =
		to_platform_device(dev->dev)->dev.platform_data;
	struct pdp_gem_object *pdp_obj;
	int err;

	pdp_obj = pdp_gem_private_object_create(dev, attach->dmabuf->size);
	if (!pdp_obj) {
		err = -ENOMEM;
		goto err_exit;
	}

	pdp_obj->sgt = sgt;

	/* We only expect a single entry for card memory */
	if (pdp_obj->sgt->nents != 1) {
		err = -EINVAL;
		goto err_obj_unref;
	}

	pdp_obj->dev_addr = sg_dma_address(pdp_obj->sgt->sgl);
	pdp_obj->cpu_addr = pdata->memory_base + pdp_obj->dev_addr;
	pdp_obj->resv = attach->dmabuf->resv;

	return &pdp_obj->base;

err_obj_unref:
	drm_gem_object_put_unlocked(&pdp_obj->base);
err_exit:
	return ERR_PTR(err);
}

int pdp_gem_dumb_create_priv(struct drm_file *file,
			     struct drm_device *dev,
			     struct pdp_gem_private *gem_priv,
			     struct drm_mode_create_dumb *args)
{
	struct drm_gem_object *obj;
	u32 handle;
	u32 pitch;
	size_t size;
	int err;

	pitch = args->width * (ALIGN(args->bpp, 8) >> 3);
	size = PAGE_ALIGN(pitch * args->height);

	obj = pdp_gem_object_create(dev, gem_priv, size, 0);
	if (IS_ERR(obj))
		return PTR_ERR(obj);

	err = drm_gem_handle_create(file, obj, &handle);
	if (err)
		goto exit;

	args->handle = handle;
	args->pitch = pitch;
	args->size = size;

exit:
	drm_gem_object_put_unlocked(obj);
	return err;
}

int pdp_gem_dumb_map_offset(struct drm_file *file,
			    struct drm_device *dev,
			    uint32_t handle,
			    uint64_t *offset)
{
	struct drm_gem_object *obj;
	int err;

	mutex_lock(&dev->struct_mutex);

	err = pdp_gem_lookup_our_object(file, handle, &obj);
	if (err)
		goto exit_unlock;

	err = drm_gem_create_mmap_offset(obj);
	if (err)
		goto exit_obj_unref;

	*offset = drm_vma_node_offset_addr(&obj->vma_node);

exit_obj_unref:
	drm_gem_object_put_unlocked(obj);
exit_unlock:
	mutex_unlock(&dev->struct_mutex);
	return err;
}

struct pdp_gem_private *pdp_gem_init(struct drm_device *dev)
{
#if !defined(SUPPORT_ION)
	pdp_gem_platform_data *pdata =
		to_platform_device(dev->dev)->dev.platform_data;
#endif
	struct pdp_gem_private *gem_priv =
					kmalloc(sizeof(*gem_priv), GFP_KERNEL);

	if (!gem_priv)
		return NULL;

	mutex_init(&gem_priv->vram_lock);

	memset(&gem_priv->vram, 0, sizeof(gem_priv->vram));

#if defined(SUPPORT_ION)
	drm_mm_init(&gem_priv->vram, 0, 0);
	DRM_INFO("%s has no directly allocatable memory; the memory is managed by ION\n",
		dev->driver->name);
#else
	drm_mm_init(&gem_priv->vram,
			pdata->pdp_heap_memory_base - pdata->memory_base,
			pdata->pdp_heap_memory_size);

	DRM_INFO("%s has %pa bytes of allocatable memory at 0x%llx = (0x%llx - 0x%llx)\n",
		dev->driver->name, &pdata->pdp_heap_memory_size,
		(u64)(pdata->pdp_heap_memory_base - pdata->memory_base),
		(u64)pdata->pdp_heap_memory_base, (u64)pdata->memory_base);
#endif
	return gem_priv;
}

void pdp_gem_cleanup(struct pdp_gem_private *gem_priv)
{
	drm_mm_takedown(&gem_priv->vram);
	mutex_destroy(&gem_priv->vram_lock);

	kfree(gem_priv);
}

struct reservation_object *pdp_gem_get_resv(struct drm_gem_object *obj)
{
	return (to_pdp_obj(obj)->resv);
}

u64 pdp_gem_get_dev_addr(struct drm_gem_object *obj)
{
	struct pdp_gem_object *pdp_obj = to_pdp_obj(obj);

	return pdp_obj->dev_addr;
}

int pdp_gem_object_create_ioctl_priv(struct drm_device *dev,
				struct pdp_gem_private *gem_priv,
				void *data,
				struct drm_file *file)
{
	struct drm_pdp_gem_create *args = data;
	struct drm_gem_object *obj;
	int err;

	if (args->flags) {
		DRM_ERROR("invalid flags: %#08x\n", args->flags);
		return -EINVAL;
	}

	if (args->handle) {
		DRM_ERROR("invalid handle (this should always be 0)\n");
		return -EINVAL;
	}

	obj = pdp_gem_object_create(dev,
					gem_priv,
					PAGE_ALIGN(args->size),
					args->flags);
	if (IS_ERR(obj))
		return PTR_ERR(obj);

	err = drm_gem_handle_create(file, obj, &args->handle);
	drm_gem_object_put_unlocked(obj);

	return err;

}

int pdp_gem_object_mmap_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file)
{
	struct drm_pdp_gem_mmap *args = (struct drm_pdp_gem_mmap *)data;

	if (args->pad) {
		DRM_ERROR("invalid pad (this should always be 0)\n");
		return -EINVAL;
	}

	if (args->offset) {
		DRM_ERROR("invalid offset (this should always be 0)\n");
		return -EINVAL;
	}

	return pdp_gem_dumb_map_offset(file, dev, args->handle, &args->offset);
}

int pdp_gem_object_cpu_prep_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file)
{
	struct drm_pdp_gem_cpu_prep *args = (struct drm_pdp_gem_cpu_prep *)data;
	struct drm_gem_object *obj;
	struct pdp_gem_object *pdp_obj;
	bool write = !!(args->flags & PDP_GEM_CPU_PREP_WRITE);
	bool wait = !(args->flags & PDP_GEM_CPU_PREP_NOWAIT);
	int err = 0;

	if (args->flags & ~(PDP_GEM_CPU_PREP_READ |
			    PDP_GEM_CPU_PREP_WRITE |
			    PDP_GEM_CPU_PREP_NOWAIT)) {
		DRM_ERROR("invalid flags: %#08x\n", args->flags);
		return -EINVAL;
	}

	mutex_lock(&dev->struct_mutex);

	err = pdp_gem_lookup_our_object(file, args->handle, &obj);
	if (err)
		goto exit_unlock;

	pdp_obj = to_pdp_obj(obj);

	if (pdp_obj->cpu_prep) {
		err = -EBUSY;
		goto exit_unref;
	}

	if (wait) {
		long lerr;

		lerr = reservation_object_wait_timeout_rcu(pdp_obj->resv,
							   write,
							   true,
							   30 * HZ);
		if (!lerr)
			err = -EBUSY;
		else if (lerr < 0)
			err = lerr;
	} else {
		if (!reservation_object_test_signaled_rcu(pdp_obj->resv,
							  write))
			err = -EBUSY;
	}

	if (!err)
		pdp_obj->cpu_prep = true;

exit_unref:
	drm_gem_object_put_unlocked(obj);
exit_unlock:
	mutex_unlock(&dev->struct_mutex);
	return err;
}

int pdp_gem_object_cpu_fini_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file)
{
	struct drm_pdp_gem_cpu_fini *args = (struct drm_pdp_gem_cpu_fini *)data;
	struct drm_gem_object *obj;
	struct pdp_gem_object *pdp_obj;
	int err = 0;

	if (args->pad) {
		DRM_ERROR("invalid pad (this should always be 0)\n");
		return -EINVAL;
	}

	mutex_lock(&dev->struct_mutex);

	err = pdp_gem_lookup_our_object(file, args->handle, &obj);
	if (err)
		goto exit_unlock;

	pdp_obj = to_pdp_obj(obj);

	if (!pdp_obj->cpu_prep) {
		err = -EINVAL;
		goto exit_unref;
	}

	pdp_obj->cpu_prep = false;

exit_unref:
	drm_gem_object_put_unlocked(obj);
exit_unlock:
	mutex_unlock(&dev->struct_mutex);
	return err;
}

