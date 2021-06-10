/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <drm/mediatek_drm.h>
#include <linux/iommu.h>
#include <linux/kmemleak.h>

#include "mtk_drm_drv.h"
#include "mtk_drm_gem.h"
#include "mtk_fence.h"
#include "mtk_drm_session.h"
#include "mtk_drm_mmp.h"
#include "ion_drv.h"
#include "ion_priv.h"
#include <soc/mediatek/smi.h>
#if defined(CONFIG_MTK_IOMMU_V2)
#include "mt_iommu.h"
#include "mtk_iommu_ext.h"
#include "mtk/mtk_ion.h"
#endif

static struct mtk_drm_gem_obj *mtk_drm_gem_init(struct drm_device *dev,
						unsigned long size)
{
	struct mtk_drm_gem_obj *mtk_gem_obj;
	int ret;

	size = round_up(size, PAGE_SIZE);

	mtk_gem_obj = kzalloc(sizeof(*mtk_gem_obj), GFP_KERNEL);
	if (!mtk_gem_obj)
		return ERR_PTR(-ENOMEM);

	ret = drm_gem_object_init(dev, &mtk_gem_obj->base, size);
	if (ret != 0) {
		DDPPR_ERR("failed to initialize gem object\n");
		kfree(mtk_gem_obj);
		return ERR_PTR(ret);
	}

	return mtk_gem_obj;
}

/* Following function is not used in mtk_drm_fb_gem_insert() so far,*/
/* Need verify before use it. */
static struct sg_table *mtk_gem_vmap_pa(struct mtk_drm_gem_obj *mtk_gem,
					phys_addr_t pa, int cached,
					struct device *dev,
					unsigned long *fb_pa)
{
	phys_addr_t pa_align;
	//phys_addr_t addr;
	uint size, sz_align, npages, i;
	struct page **pages;
	pgprot_t pgprot;
	void *va_align;
	struct sg_table *sgt;
	unsigned long attrs;

	size = mtk_gem->size;
	pa_align = round_down(pa, PAGE_SIZE);
	sz_align = ALIGN(pa + size - pa_align, PAGE_SIZE);
	npages = sz_align / PAGE_SIZE;

	pages = kmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return NULL;

	pgprot = cached ? PAGE_KERNEL : pgprot_writecombine(PAGE_KERNEL);
	for (i = 0; i < npages; i++)
		pages[i] = phys_to_page(pa_align + i * PAGE_SIZE);

	va_align = vmap(pages, npages, VM_MAP, pgprot);

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		DDPPR_ERR("sgt creation failed\n");
		return NULL;
	}

	sg_alloc_table_from_pages(sgt, pages, npages, 0, size, GFP_KERNEL);
	attrs = DMA_ATTR_SKIP_CPU_SYNC;
	dma_map_sg_attrs(dev, sgt->sgl, sgt->nents, 0, attrs);
	*fb_pa = sg_dma_address(sgt->sgl);

	mtk_gem->cookie = va_align;

	kfree(pages);

	return sgt;
}
#if 0
static void mtk_gem_vmap_pa_legacy(phys_addr_t pa, uint size,
				   struct mtk_drm_gem_obj *mtk_gem)
{
#if defined(CONFIG_MTK_IOMMU_V2)
	struct ion_client *client;
	struct ion_handle *handle;
	struct ion_mm_data mm_data;

	mtk_gem->cookie = (void *)ioremap_nocache(pa, size);
	mtk_gem->kvaddr = mtk_gem->cookie;

	client = mtk_drm_gem_ion_create_client("disp_fb0");
	handle =
		ion_alloc(client, size, (size_t)mtk_gem->kvaddr,
			  ION_HEAP_MULTIMEDIA_MAP_MVA_MASK, 0);
	if (IS_ERR(handle)) {
		DDPPR_ERR("ion alloc failed, handle:0x%p\n", handle);
		return;
	}

	/* use get_iova replace config_buffer & get_phys*/
	memset((void *)&mm_data, 0, sizeof(struct ion_mm_data));
	/* should use Your HW port id, please don't use other's port id */
	mm_data.get_phys_param.module_id = 0;
	mm_data.get_phys_param.kernel_handle = handle;
	mm_data.mm_cmd = ION_MM_GET_IOVA;

	if (ion_kernel_ioctl(client, ION_CMD_MULTIMEDIA,
				 (unsigned long)&mm_data) < 0) {
		DDPPR_ERR("ion config failed, handle:0x%p\n", handle);
		mtk_drm_gem_ion_free_handle(client, handle,
				__func__, __LINE__);
		return;
	}
	mtk_gem->sec = false;
	mtk_gem->dma_addr = (unsigned int)mm_data.get_phys_param.phy_addr;
	mtk_gem->size = mm_data.get_phys_param.len;
#endif
}
#endif
static inline void *mtk_gem_dma_alloc(struct device *dev, size_t size,
				       dma_addr_t *dma_handle, gfp_t flag,
				       unsigned long attrs, const char *name,
				       int line)
{
	void *va;

	DDPINFO("D_ALLOC:%s[%d], dma:0x%p, size:%ld\n", name, line,
			dma_handle, size);
	DRM_MMP_EVENT_START(dma_alloc, line, 0);
	va = dma_alloc_attrs(dev, size, dma_handle,
					flag, attrs);

	DRM_MMP_EVENT_END(dma_alloc, (unsigned long)dma_handle,
			(unsigned long)size);
	return va;
}

static inline void mtk_gem_dma_free(struct device *dev, size_t size,
				     void *cpu_addr, dma_addr_t dma_handle,
				     unsigned long attrs, const char *name,
				     int line)
{
	DDPINFO("D_FREE:%s[%d], dma:0x%llx, size:%ld\n", name, line,
			dma_handle, size);
	DRM_MMP_EVENT_START(dma_free, (unsigned long)dma_handle,
			(unsigned long)size);
	dma_free_attrs(dev, size, cpu_addr, dma_handle,
					attrs);

	DRM_MMP_EVENT_END(dma_free, line, 0);
}

struct mtk_drm_gem_obj *mtk_drm_fb_gem_insert(struct drm_device *dev,
					      size_t size, phys_addr_t fb_base,
					      unsigned int vramsize)
{
	struct mtk_drm_gem_obj *mtk_gem;
	struct drm_gem_object *obj;
	struct sg_table *sgt;
	unsigned long fb_pa = 0;

	DDPINFO("%s+\n", __func__);
	mtk_gem = mtk_drm_gem_init(dev, vramsize);
	if (IS_ERR(mtk_gem))
		return ERR_CAST(mtk_gem);

	obj = &mtk_gem->base;

	mtk_gem->size = size;
	mtk_gem->dma_attrs = DMA_ATTR_WRITE_COMBINE;

	sgt = mtk_gem_vmap_pa(mtk_gem, fb_base, 0, dev->dev, &fb_pa);

	mtk_gem->sec = false;
	mtk_gem->dma_addr = (dma_addr_t)fb_pa;
	mtk_gem->kvaddr = mtk_gem->cookie;
	mtk_gem->sg = sgt;

	DDPINFO("%s cookie = %p dma_addr = %pad size = %zu\n", __func__,
		mtk_gem->cookie, &mtk_gem->dma_addr, size);

	return mtk_gem;
}

void mtk_drm_fb_gem_release(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct mtk_drm_gem_obj *mtk_gem = to_mtk_gem_obj(priv->fbdev_bo);

	sg_free_table(mtk_gem->sg);
	drm_gem_object_release(&mtk_gem->base);

	kfree(mtk_gem->sg);
	kfree(mtk_gem);
}

struct mtk_drm_gem_obj *mtk_drm_gem_create(struct drm_device *dev, size_t size,
					   bool alloc_kmap)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct mtk_drm_gem_obj *mtk_gem;
	struct drm_gem_object *obj;
	int ret;

	mtk_gem = mtk_drm_gem_init(dev, size);
	if (IS_ERR(mtk_gem))
		return ERR_CAST(mtk_gem);

	obj = &mtk_gem->base;

	mtk_gem->dma_attrs = DMA_ATTR_WRITE_COMBINE;

	if (!alloc_kmap)
		mtk_gem->dma_attrs |= DMA_ATTR_NO_KERNEL_MAPPING;
	//	mtk_gem->dma_attrs |= DMA_ATTR_NO_WARN;
	mtk_gem->cookie =
		mtk_gem_dma_alloc(priv->dma_dev, obj->size, &mtk_gem->dma_addr,
				GFP_KERNEL, mtk_gem->dma_attrs, __func__,
				__LINE__);
	if (!mtk_gem->cookie) {
		DDPPR_ERR("failed to allocate %zx byte dma buffer", obj->size);
		ret = -ENOMEM;
		goto err_gem_free;
	}
	mtk_gem->size = obj->size;

	if (alloc_kmap)
		mtk_gem->kvaddr = mtk_gem->cookie;

	DRM_DEBUG_DRIVER("cookie = %p dma_addr = %pad size = %zu\n",
			 mtk_gem->cookie, &mtk_gem->dma_addr, mtk_gem->size);

	return mtk_gem;

err_gem_free:
	drm_gem_object_release(obj);
	kfree(mtk_gem);
	return ERR_PTR(ret);
}

void mtk_drm_gem_free_object(struct drm_gem_object *obj)
{
	struct mtk_drm_gem_obj *mtk_gem = to_mtk_gem_obj(obj);
	struct mtk_drm_private *priv = obj->dev->dev_private;

	if (mtk_gem->sg)
		drm_prime_gem_destroy(obj, mtk_gem->sg);
	else if (!mtk_gem->sec)
		mtk_gem_dma_free(priv->dma_dev, obj->size, mtk_gem->cookie,
			       mtk_gem->dma_addr, mtk_gem->dma_attrs,
			       __func__, __LINE__);

#if defined(CONFIG_MTK_IOMMU_V2)
	/* No ion handle in dumb buffer */
	if (mtk_gem->handle && priv->client)
		mtk_drm_gem_ion_free_handle(priv->client, mtk_gem->handle,
				__func__, __LINE__);
	else if (!mtk_gem->is_dumb)
		DDPPR_ERR("invaild ion handle or client\n");
#endif

	/* release file pointer to gem object. */
	drm_gem_object_release(obj);

	kfree(mtk_gem);
}

int mtk_drm_gem_dumb_create(struct drm_file *file_priv, struct drm_device *dev,
			    struct drm_mode_create_dumb *args)
{
	struct mtk_drm_gem_obj *mtk_gem;
	int ret;

	args->pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
	args->size = (__u64)args->pitch * (__u64)args->height;

	mtk_gem = mtk_drm_gem_create(dev, args->size, false);
	if (IS_ERR(mtk_gem))
		return PTR_ERR(mtk_gem);

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv, &mtk_gem->base, &args->handle);
	if (ret)
		goto err_handle_create;

	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_unreference_unlocked(&mtk_gem->base);

	mtk_gem->is_dumb = 1;

	return 0;

err_handle_create:
	mtk_drm_gem_free_object(&mtk_gem->base);
	return ret;
}

int mtk_drm_gem_dumb_map_offset(struct drm_file *file_priv,
				struct drm_device *dev, uint32_t handle,
				uint64_t *offset)
{
	struct drm_gem_object *obj;
	int ret;

	obj = drm_gem_object_lookup(file_priv, handle);
	if (!obj) {
		DDPPR_ERR("failed to lookup gem object.\n");
		return -EINVAL;
	}

	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		goto out;

	*offset = drm_vma_node_offset_addr(&obj->vma_node);
	DRM_DEBUG_KMS("offset = 0x%llx\n", *offset);

out:
	drm_gem_object_unreference_unlocked(obj);
	return ret;
}

static int mtk_drm_gem_object_mmap(struct drm_gem_object *obj,
				   struct vm_area_struct *vma)

{
	int ret;
	struct mtk_drm_gem_obj *mtk_gem = to_mtk_gem_obj(obj);
	struct mtk_drm_private *priv = obj->dev->dev_private;

	/*
	 * dma_alloc_attrs() allocated a struct page table for mtk_gem, so clear
	 * VM_PFNMAP flag that was set by drm_gem_mmap_obj()/drm_gem_mmap().
	 */
	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_pgoff = 0;

	ret = dma_mmap_attrs(priv->dma_dev, vma, mtk_gem->cookie,
			     mtk_gem->dma_addr, obj->size, mtk_gem->dma_attrs);
	if (ret)
		drm_gem_vm_close(vma);

	return ret;
}

int mtk_drm_gem_mmap_buf(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	int ret;

	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	if (ret)
		return ret;

	return mtk_drm_gem_object_mmap(obj, vma);
}

int mtk_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_gem_object *obj;
	int ret;

	ret = drm_gem_mmap(filp, vma);
	if (ret)
		return ret;

	obj = vma->vm_private_data;

	return mtk_drm_gem_object_mmap(obj, vma);
}

#if defined(CONFIG_MTK_IOMMU_V2)
struct ion_handle *mtk_gem_ion_import_dma_buf(struct ion_client *client,
		struct dma_buf *dmabuf, const char *name, int line)
{
	struct ion_handle *handle;

	DDPDBG("%s:%d client:0x%p, dma_buf=0x%p, name:%s, line:%d +\n",
		   __func__, __LINE__,
		   client,
		   dmabuf,
		   name,
		   line);
	DRM_MMP_EVENT_START(ion_import_dma, (unsigned long)client, line);
	handle = ion_import_dma_buf(client, dmabuf);

	DRM_MMP_EVENT_END(ion_import_dma, (unsigned long)handle->buffer,
			(unsigned long)dmabuf);
	DDPDBG("%s:%d handle:0x%p -\n",
		   __func__, __LINE__,
		   handle);

	return handle;
}

struct ion_handle *mtk_gem_ion_import_dma_fd(struct ion_client *client,
		int fd, const char *name, int line)
{
	struct ion_handle *handle;
	struct dma_buf *dmabuf;

	DDPDBG("%s:%d client:0x%p, fd=%d, name:%s, line:%d +\n",
		   __func__, __LINE__,
		   client,
		   fd,
		   name,
		   line);
	DRM_MMP_EVENT_START(ion_import_fd, (unsigned long)client, line);
	handle = ion_import_dma_buf_fd(client, fd);
	if (IS_ERR(handle)) {
		DDPPR_ERR("%s:%d dma_buf_get fail fd=%d ret=0x%p\n",
		       __func__, __LINE__, fd, handle);
		return ERR_CAST(handle);
	}

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf)) {
		DDPPR_ERR("%s:%d dma_buf_get fail fd=%d ret=0x%p\n",
		       __func__, __LINE__, fd, dmabuf);
		return ERR_CAST(dmabuf);
	}
	DRM_MMP_MARK(dma_get, (unsigned long)handle->buffer,
			(unsigned long)dmabuf);

	DRM_MMP_EVENT_END(ion_import_fd, (unsigned long)handle->buffer,
			(unsigned long)dmabuf);

	DRM_MMP_MARK(dma_put, (unsigned long)handle->buffer,
			(unsigned long)dmabuf);
	dma_buf_put(dmabuf);
	DDPDBG("%s:%d -\n",
		   __func__, __LINE__);

	return handle;
}

struct ion_client *mtk_drm_gem_ion_create_client(const char *name)
{
	struct ion_client *disp_ion_client = NULL;

	if (g_ion_device)
		disp_ion_client = ion_client_create(g_ion_device, name);
	else
		DDPPR_ERR("invalid g_ion_device\n");

	if (!disp_ion_client) {
		DDPPR_ERR("create ion client failed!\n");
		return NULL;
	}

	return disp_ion_client;
}

void mtk_drm_gem_ion_destroy_client(struct ion_client *client)
{
	if (!client) {
		DDPPR_ERR("invalid ion client!\n");
		return;
	}

	ion_client_destroy(client);
}

void mtk_drm_gem_ion_free_handle(struct ion_client *client,
	struct ion_handle *handle, const char *name, int line)
{
	DRM_MMP_EVENT_START(ion_import_free,
			    (unsigned long)handle->buffer, line);

	if (!client) {
		DDPPR_ERR("invalid ion client!\n");
		DRM_MMP_MARK(ion_import_free, 0, 1);
		DRM_MMP_EVENT_END(ion_import_free, (unsigned long)client, line);
		return;
	}
	if (!handle) {
		DDPPR_ERR("invalid ion handle!\n");
		DRM_MMP_MARK(ion_import_free, 0, 2);
		DRM_MMP_EVENT_END(ion_import_free, (unsigned long)client, line);
		return;
	}

	ion_free(client, handle);

	DRM_MMP_EVENT_END(ion_import_free, (unsigned long)client, line);
}

/**
 * Import ion handle and configure this buffer
 * @client
 * @fd ion shared fd
 * @return ion handle
 */
struct ion_handle *mtk_drm_gem_ion_import_handle(struct ion_client *client,
	int fd)
{
	struct ion_handle *handle = NULL;

	DDPDBG("%s:%d client:0x%p, fd=%d +\n",
		   __func__, __LINE__,
		   client,
		   fd);
	if (!client) {
		DDPPR_ERR("invalid ion client!\n");
		return handle;
	}
	handle = mtk_gem_ion_import_dma_fd(client, fd,
			__func__, __LINE__);
	if (IS_ERR(handle)) {
		DDPPR_ERR("import ion handle failed! fd:%d\n",
			fd);
		return NULL;
	}
	DDPDBG("%s:%d handle:0x%p -\n",
		   __func__, __LINE__,
		   handle);

	return handle;
}
#endif

/* Generate drm_gem_object from dma_buf which get from prime fd in DRM core
 * check drm_gem_prime_fd_to_handle
 */
struct drm_gem_object *
mtk_gem_prime_import(struct drm_device *dev, struct dma_buf *dma_buf)
{
	struct drm_gem_object *obj;
	struct mtk_drm_gem_obj *mtk_gem;// = to_mtk_gem_obj(obj);
#if defined(CONFIG_MTK_IOMMU_V2)
	struct mtk_drm_private *priv = dev->dev_private;
	struct ion_client *client;
	struct ion_handle *handle;

	DDPDBG("%s:%d dev:0x%p, dma_buf=0x%p +\n",
		   __func__, __LINE__,
		   dev,
		   dma_buf);

	client = priv->client;
	handle = mtk_gem_ion_import_dma_buf(client, dma_buf,
			__func__, __LINE__);
	if (IS_ERR(handle)) {
		DDPPR_ERR("ion import failed, client:0x%p, dmabuf:0x%p\n",
				client, dma_buf);
		return ERR_PTR(-EINVAL);
	}

#endif
	obj = drm_gem_prime_import(dev, dma_buf);
	if (IS_ERR(obj))
		return obj;

	mtk_gem = to_mtk_gem_obj(obj);
#if defined(CONFIG_MTK_IOMMU_V2)
	mtk_gem->handle = handle;

	DDPDBG("%s:%d client:0x%p, handle=0x%p obj:0x%p, gem:0x%p -\n",
		   __func__, __LINE__,
		   client,
		   handle,
		   obj,
		   mtk_gem);
#endif

	return obj;
}

/*
 * Allocate a sg_table for this GEM object.
 * Note: Both the table's contents, and the sg_table itself must be freed by
 *       the caller.
 * Returns a pointer to the newly allocated sg_table, or an ERR_PTR() error.
 */
struct sg_table *mtk_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct mtk_drm_gem_obj *mtk_gem = to_mtk_gem_obj(obj);
	struct mtk_drm_private *priv = obj->dev->dev_private;
	struct sg_table *sgt;
	int ret;

	DDPDBG("%s:%d obj:0x%p +\n",
		   __func__, __LINE__,
		   obj);

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		DDPPR_ERR("%s:%d sgt alloc error\n",
			   __func__, __LINE__);
		return ERR_PTR(-ENOMEM);
	}

	ret = dma_get_sgtable_attrs(priv->dma_dev, sgt, mtk_gem->cookie,
				    mtk_gem->dma_addr, obj->size,
				    mtk_gem->dma_attrs);
	if (ret) {
		DDPPR_ERR("failed to allocate sgt, %d\n", ret);
		kfree(sgt);
		return ERR_PTR(ret);
	}

	DDPDBG("%s:%d sgt:0x%p ret:%d -\n",
		   __func__, __LINE__,
		   sgt,
		   ret);

	return sgt;
}

struct drm_gem_object *
mtk_gem_prime_import_sg_table(struct drm_device *dev,
			      struct dma_buf_attachment *attach,
			      struct sg_table *sg)
{
	struct mtk_drm_gem_obj *mtk_gem;
	int ret;
	struct scatterlist *s;
	unsigned int i, last_len = 0, error_cnt = 0;
	dma_addr_t expected, last_iova = 0;

	DDPDBG("%s:%d dev:0x%p, attach:0x%p, sg:0x%p +\n",
			__func__, __LINE__,
			dev,
			attach,
			sg);
	mtk_gem = mtk_drm_gem_init(dev, attach->dmabuf->size);

	if (IS_ERR(mtk_gem)) {
		DDPPR_ERR("%s:%d mtk_gem error:0x%p\n",
				__func__, __LINE__,
				mtk_gem);
		return ERR_PTR(PTR_ERR(mtk_gem));
	}

	expected = sg_dma_address(sg->sgl);
	for_each_sg(sg->sgl, s, sg->nents, i) {
		if (sg_dma_address(s) != expected) {
			if (!error_cnt)
				DDPPR_ERR("sg_table is not contiguous %u\n",
					  sg->nents);
			DDPPR_ERR("exp:0x%llx,cur:0x%llx/%u,last:0x%llx+0x%x\n",
				  expected, sg_dma_address(s),
				  i, last_iova, last_len);
			if (error_cnt++ > 5)
				break;
		}
		last_iova = sg_dma_address(s);
		last_len = sg_dma_len(s);
		expected = sg_dma_address(s) + sg_dma_len(s);
	}
	if (error_cnt) {
		ret = -EINVAL;
		goto err_gem_free;
	}

	mtk_gem->sec = false;
	mtk_gem->dma_addr = sg_dma_address(sg->sgl);
	mtk_gem->size = attach->dmabuf->size;
	mtk_gem->sg = sg;

	DDPDBG("%s:%d sec:%d, addr:0x%llx, size:%ld, sg:0x%p -\n",
			__func__, __LINE__,
			mtk_gem->sec,
			mtk_gem->dma_addr,
			mtk_gem->size,
			mtk_gem->sg);

	return &mtk_gem->base;

err_gem_free:
	kfree(mtk_gem);
	return ERR_PTR(ret);
}

int mtk_gem_map_offset_ioctl(struct drm_device *drm, void *data,
			     struct drm_file *file_priv)
{
	struct drm_mtk_gem_map_off *args = data;

	return mtk_drm_gem_dumb_map_offset(file_priv, drm, args->handle,
					   &args->offset);
}

int mtk_gem_create_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
//avoid security issue
#if 0
	struct mtk_drm_gem_obj *mtk_gem;
	struct drm_mtk_gem_create *args = data;
	int ret;

	DDPDBG("%s(), args->size %llu\n", __func__, args->size);
	if (args->size == 0) {
		DDPPR_ERR("%s(), invalid args->size %llu\n",
			  __func__, args->size);
		return 0;
	}

	mtk_gem = mtk_drm_gem_create(dev, args->size, false);
	if (IS_ERR(mtk_gem))
		return PTR_ERR(mtk_gem);

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv, &mtk_gem->base, &args->handle);
	if (ret)
		goto err_handle_create;

	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_unreference_unlocked(&mtk_gem->base);

	return 0;

err_handle_create:
	mtk_drm_gem_free_object(&mtk_gem->base);
	return ret;
#endif
	DDPPR_ERR("not supported!\n");
	return 0;
}

static void prepare_output_buffer(struct drm_device *dev,
				  struct drm_mtk_gem_submit *buf,
				  struct mtk_fence_buf_info *output_buf)
{

	if (!(mtk_drm_session_mode_is_decouple_mode(dev) &&
	      mtk_drm_session_mode_is_mirror_mode(dev))) {
		buf->interface_fence_fd = MTK_INVALID_FENCE_FD;
		buf->interface_index = 0;
		return;
	}

	/* create second fence for WDMA when decouple mirror mode */
	buf->layer_id = mtk_fence_get_interface_timeline_id();
	output_buf = mtk_fence_prepare_buf(dev, buf);
	if (output_buf) {
		buf->interface_fence_fd = output_buf->fence;
		buf->interface_index = output_buf->idx;
	} else {
		DDPPR_ERR("P+ FAIL /%s%d/L%d/e%d/fd%d/id%d/ffd%d\n",
			  mtk_fence_session_mode_spy(buf->session_id),
			  MTK_SESSION_DEV(buf->session_id), buf->layer_id,
			  buf->layer_en, buf->index, buf->fb_id, buf->fence_fd);
		buf->interface_fence_fd = MTK_INVALID_FENCE_FD;
		buf->interface_index = 0;
	}
}

int mtk_gem_submit_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_mtk_gem_submit *args = data;
	struct mtk_fence_buf_info *buf, *buf2 = NULL;

	if (args->type == MTK_SUBMIT_OUT_FENCE)
		args->layer_id = mtk_fence_get_output_timeline_id();

	if (args->layer_en) {
		buf = mtk_fence_prepare_buf(dev, args);
		if (buf != NULL) {
			args->fence_fd = buf->fence;
			args->index = buf->idx;
		} else {
			DDPPR_ERR("P+ FAIL /%s%d/L%d/e%d/fd%d/id%d/ffd%d\n",
				  mtk_fence_session_mode_spy(args->session_id),
				  MTK_SESSION_DEV(args->session_id),
				  args->layer_id, args->layer_en, args->fb_id,
				  args->index, args->fence_fd);
			args->fence_fd = MTK_INVALID_FENCE_FD; /* invalid fd */
			args->index = 0;
		}
		if (args->type == MTK_SUBMIT_OUT_FENCE)
			prepare_output_buffer(dev, args, buf2);
	} else {
		DDPPR_ERR("P+ FAIL /%s%d/l%d/e%d/fd%d/id%d/ffd%d\n",
			  mtk_fence_session_mode_spy(args->session_id),
			  MTK_SESSION_DEV(args->session_id), args->layer_id,
			  args->layer_en, args->fb_id, args->index,
			  args->fence_fd);
		args->fence_fd = MTK_INVALID_FENCE_FD; /* invalid fd */
		args->index = 0;
	}

	return ret;
}

int mtk_drm_sec_hnd_to_gem_hnd(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct drm_mtk_sec_gem_hnd *args = data;
	struct mtk_drm_gem_obj *mtk_gem_obj;

	DDPDBG("%s:%d dev:0x%p, data:0x%p, priv:0x%p +\n",
		  __func__, __LINE__,
		  dev,
		  data,
		  file_priv);

	if (!drm_core_check_feature(dev, DRIVER_PRIME))
		return -EINVAL;

	mtk_gem_obj = kzalloc(sizeof(*mtk_gem_obj), GFP_KERNEL);
	if (!mtk_gem_obj)
		return -ENOMEM;

	mtk_gem_obj->sec = true;
	mtk_gem_obj->dma_addr = args->sec_hnd;
	drm_gem_private_object_init(dev, &mtk_gem_obj->base, 0);
	drm_gem_handle_create(file_priv, &mtk_gem_obj->base, &args->gem_hnd);

	DDPDBG("%s:%d obj:0x%p, sec:%d, addr:0x%llx -\n",
		  __func__, __LINE__,
		  mtk_gem_obj,
		  mtk_gem_obj->sec,
		  mtk_gem_obj->dma_addr);

	return 0;
}

