// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <drm/drm_gem.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <drm/mediatek_drm.h>
#include <linux/iommu.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_prime.h>
#include <linux/kmemleak.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>

#include "mtk_drm_drv.h"
#include "mtk_drm_gem.h"
#include "mtk_fence.h"
#include "mtk_drm_session.h"
#include "mtk_drm_mmp.h"
#include <soc/mediatek/smi.h>
#ifdef IF_ZERO
#include "mt_iommu.h"
#include "mtk_iommu_ext.h"
#endif

#include "../mml/mtk-mml.h"
#include "../mml/mtk-mml-drm-adaptor.h"
#include "../mml/mtk-mml-driver.h"
#include <linux/of_platform.h>

static const struct drm_gem_object_funcs mtk_drm_gem_object_funcs = {
	.free = mtk_drm_gem_free_object,
	.get_sg_table = mtk_gem_prime_get_sg_table,
	.vm_ops = &drm_gem_cma_vm_ops,
};

static struct mtk_drm_gem_obj *mtk_drm_gem_init(struct drm_device *dev,
						unsigned long size)
{
	struct mtk_drm_gem_obj *mtk_gem_obj;
	int ret;

	size = round_up(size, PAGE_SIZE);

	mtk_gem_obj = kzalloc(sizeof(*mtk_gem_obj), GFP_KERNEL);
	if (!mtk_gem_obj)
		return ERR_PTR(-ENOMEM);

	mtk_gem_obj->base.funcs = &mtk_drm_gem_object_funcs;

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
					dma_addr_t *fb_pa)
{
	phys_addr_t pa_align;
	//phys_addr_t addr;
	uint size, sz_align, npages, i;
	struct page **pages;
	pgprot_t pgprot;
	void *va_align;
	struct sg_table *sgt = NULL;
	int ret = 0;
	unsigned long attrs;

	size = mtk_gem->size;
	pa_align = round_down(pa, PAGE_SIZE);
	sz_align = ALIGN(pa + size - pa_align, PAGE_SIZE);
	npages = sz_align / PAGE_SIZE;

	pages = kmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		DDPPR_ERR("pages creation failed\n");
		return NULL;
	}

	pgprot = cached ? PAGE_KERNEL : pgprot_writecombine(PAGE_KERNEL);
	for (i = 0; i < npages; i++)
		pages[i] = phys_to_page(pa_align + i * PAGE_SIZE);

	va_align = vmap(pages, npages, VM_MAP, pgprot);

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		kfree(pages);
		vunmap(va_align);
		DDPPR_ERR("sgt creation failed\n");
		return NULL;
	}

	ret = sg_alloc_table_from_pages(sgt, pages, npages, 0, size, GFP_KERNEL);

	if (ret < 0) {
		DDPPR_ERR("sg_alloc_table_from_pages failed with ret %d\n", ret);
		kfree(pages);
		kfree(sgt);
		vunmap(va_align);
		return NULL;
	}
	attrs = DMA_ATTR_SKIP_CPU_SYNC;
	dma_map_sg_attrs(dev, sgt->sgl, sgt->nents, 0, attrs);
	*fb_pa = sg_dma_address(sgt->sgl);

	mtk_gem->cookie = va_align;

	kfree(pages);

	return sgt;
}

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
	dma_addr_t fb_pa = 0;
	struct mtk_drm_private *private = dev->dev_private;

	DDPINFO("%s+\n", __func__);
	mtk_gem = mtk_drm_gem_init(dev, vramsize);
	if (IS_ERR(mtk_gem))
		return ERR_CAST(mtk_gem);

	obj = &mtk_gem->base;

	mtk_gem->size = size;
	mtk_gem->dma_attrs = DMA_ATTR_WRITE_COMBINE;

	sgt = mtk_gem_vmap_pa(mtk_gem, fb_base, 0, dev->dev, &fb_pa);

	mtk_gem->sec = false;

	if (!mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_USE_M4U))
		mtk_gem->dma_addr = fb_base;
	else
		mtk_gem->dma_addr = fb_pa;

	mtk_gem->kvaddr = mtk_gem->cookie;
	mtk_gem->sg = sgt;

	DDPINFO("%s cookie = %p dma_addr = %pad size = %zu\n", __func__,
		mtk_gem->cookie, &mtk_gem->dma_addr, size);

	return mtk_gem;
}

void mtk_drm_fb_gem_release(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct mtk_drm_gem_obj *mtk_gem = priv->fb_info.fb_gem;
	struct sg_table *sg = mtk_gem->sg;

	dma_unmap_sg_attrs(dev->dev, sg->sgl, sg->nents,
			DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
	sg_free_table(sg);
	vunmap(mtk_gem->kvaddr);
	drm_gem_object_release(&mtk_gem->base);

	kfree(mtk_gem->sg);
	kfree(mtk_gem);
	priv->fb_info.fb_gem = NULL;
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

struct mtk_drm_gem_obj *mtk_drm_gem_create_from_heap(struct drm_device *dev,
				       const char *heap, size_t size)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct mtk_drm_gem_obj *mtk_gem;
	struct drm_gem_object *obj;
	struct dma_heap *dma_heap;
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct dma_buf_map map = {};
	int ret;

	mtk_gem = mtk_drm_gem_init(dev, size);
	if (IS_ERR(mtk_gem))
		return ERR_CAST(mtk_gem);

	obj = &mtk_gem->base;

	dma_heap = dma_heap_find(heap);
	if (!dma_heap) {
		DDPPR_ERR("heap find fail\n");
		goto err_gem_free;
	}
	dma_buf = dma_heap_buffer_alloc(dma_heap, size,
			O_RDWR | O_CLOEXEC, DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR(dma_buf)) {
		DDPPR_ERR("buffer alloc fail\n");
		dma_heap_put(dma_heap);
		goto err_gem_free;
	}
	dma_heap_put(dma_heap);

	attach = dma_buf_attach(dma_buf, priv->dma_dev);
	if (IS_ERR(attach)) {
		DDPPR_ERR("attach fail, return\n");
		dma_heap_buffer_free(dma_buf);
		goto err_gem_free;
	}
	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		DDPPR_ERR("map failed, detach and return\n");
		dma_buf_detach(dma_buf, attach);
		dma_heap_buffer_free(dma_buf);
		goto err_gem_free;
	}
	mtk_gem->dma_addr = sg_dma_address(sgt->sgl);
	mtk_gem->sg = sgt;
	mtk_gem->size = dma_buf->size;
	ret = dma_buf_vmap(dma_buf, &map);
	if (ret) {
		DDPPR_ERR("map failed\n");
		/* svp buff can not be mapped */
		//dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
		//dma_buf_detach(dma_buf, attach);
		//dma_heap_buffer_free(dma_buf);
		mtk_gem->kvaddr = NULL;
	} else
		mtk_gem->kvaddr = map.vaddr;

	return mtk_gem;

err_gem_free:
	drm_gem_object_release(obj);
	kfree(mtk_gem);
	return NULL;
}

void mtk_drm_gem_free_object(struct drm_gem_object *obj)
{
	struct mtk_drm_gem_obj *mtk_gem = to_mtk_gem_obj(obj);
	struct mtk_drm_private *priv = obj->dev->dev_private;
	DRM_MMP_MARK(ion_import_free,
		(unsigned long)mtk_gem->dma_addr,
		(unsigned long)((unsigned long long)(mtk_gem->dma_addr >> 4 & 0x030000000ul)
		| mtk_gem->size));

	if (mtk_gem->sg)
		drm_prime_gem_destroy(obj, mtk_gem->sg);
	else if (!mtk_gem->sec)
		mtk_gem_dma_free(priv->dma_dev, obj->size, mtk_gem->cookie,
			       mtk_gem->dma_addr, mtk_gem->dma_attrs,
			       __func__, __LINE__);

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
	drm_gem_object_put(&mtk_gem->base);

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
	drm_gem_object_put(obj);
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

void mtk_drm_gem_ion_free_handle(struct dma_buf *buf_hnd,
				const char *name, int line)
{
	if (!buf_hnd) {
		DDPPR_ERR("invalid buf_hnd handle!\n");

		return;
	}

	dma_buf_put(buf_hnd);

	DDPDBG("free dma_buf handle 0x%p\n", buf_hnd);
}

/**
 * Import ion handle and configure this buffer
 * @client
 * @fd ion shared fd
 * @return ion handle
 */
struct dma_buf *mtk_drm_gem_ion_import_handle(int fd)
{
	struct dma_buf *buf_hnd = NULL;

	buf_hnd = dma_buf_get(fd);

	if (IS_ERR(buf_hnd)) {
		DDPPR_ERR("%s:%d error! hnd:0x%p, fd:%d\n",
					__func__, __LINE__, buf_hnd, fd);
		return NULL;
	}

	DDPDBG("import dma_buf handle fd=%d,hnd=0x%p\n", fd, buf_hnd);

	return buf_hnd;
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
	struct mtk_drm_gem_obj *mtk_gem = NULL;
#ifdef IF_ZERO
	int ret;
	struct scatterlist *s;
	unsigned int i, last_len = 0, error_cnt = 0;
	dma_addr_t expected, last_iova = 0;
#endif

	DDPDBG("%s:%d dev:0x%p, attach:0x%p, sg:0x%p +\n",
			__func__, __LINE__,
			dev,
			attach,
			sg);
	if (attach && attach->dmabuf)
		mtk_gem = mtk_drm_gem_init(dev, attach->dmabuf->size);

	if (IS_ERR(mtk_gem)) {
		DDPPR_ERR("%s:%d mtk_gem error:0x%p\n",
				__func__, __LINE__,
				mtk_gem);
		return ERR_PTR(PTR_ERR(mtk_gem));
	} else if (!mtk_gem) {
		DDPPR_ERR("%s:%d mtk_gem error:0x%p\n",
				__func__, __LINE__,
				mtk_gem);
		return ERR_PTR(-ENOMEM);
	}

#ifdef IF_ZERO
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
#endif
	if (disp_sec_cb.cb != NULL && attach->dmabuf)
		mtk_gem->sec = disp_sec_cb.cb(DISP_SEC_CHECK, NULL, 0, attach->dmabuf);
	else
		mtk_gem->sec = false;
	mtk_gem->dma_addr = sg_dma_address(sg->sgl);
	mtk_gem->size = attach->dmabuf->size;
	mtk_gem->sg = sg;
	DRM_MMP_MARK(ion_import_dma,
		(unsigned long)mtk_gem->dma_addr,
		(unsigned long)((unsigned long long)(mtk_gem->dma_addr >> 4 & 0x030000000ul)
		| mtk_gem->size));

	DDPDBG("%s:%d sec:%d, addr:0x%llx, size:%ld, sg:0x%p -\n",
			__func__, __LINE__,
			mtk_gem->sec,
			mtk_gem->dma_addr,
			mtk_gem->size,
			mtk_gem->sg);

	return &mtk_gem->base;

#ifdef IF_ZERO
err_gem_free:
	kfree(mtk_gem);
	return ERR_PTR(ret);
#endif
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
#ifdef IF_ZERO
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
	drm_gem_object_put(&mtk_gem->base);

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
	output_buf = mtk_fence_prepare_buf(dev, buf, false, NULL);
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
		buf = mtk_fence_prepare_buf(dev, args, false, NULL);
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

void print_mml_frame_buffer(struct mml_frame_buffer frame_buf)
{
	unsigned int i = 0;

	DDPMSG("====  frame buffer s ====\n");
	DDPMSG("src buf\n");
	DDPMSG("[0]:fd:%d, size:%d, [1]:fd:%d, size:%d, [2]:fd:%d, size:%d,",
			frame_buf.src.fd[0], frame_buf.src.size[0],
			frame_buf.src.fd[1], frame_buf.src.size[1],
			frame_buf.src.fd[2], frame_buf.src.size[2]);
	DDPMSG("cnt:%d, fence:%d, flush:%d, invalid:%d\n",
			frame_buf.src.cnt, frame_buf.src.fence,
			frame_buf.src.flush, frame_buf.src.invalid);
	DDPMSG("dest:(cnt:%d)\n", frame_buf.dest_cnt);
	for (; i < MML_MAX_OUTPUTS; ++i) {
		DDPMSG("dest buf[%d]\n", i);
		DDPMSG("[0]:fd:%d, size:%d, [1]:fd:%d, size:%d, [2]:fd:%d, size:%d,",
			frame_buf.dest[i].fd[0], frame_buf.dest[i].size[0],
			frame_buf.dest[i].fd[1], frame_buf.dest[i].size[1],
			frame_buf.dest[i].fd[2], frame_buf.dest[i].size[2]);

		DDPMSG("cnt:%d, fence:%d, flush:%d, invalid:%d\n",
			frame_buf.dest[i].cnt, frame_buf.dest[i].fence,
			frame_buf.dest[i].flush, frame_buf.dest[i].invalid);
	}
	DDPMSG("==== frame buffer e ====\n");
}

//To-Do: need to be remove
void print_mml_frame_info(struct mml_frame_info info)
{
	unsigned int i = 0;

	DDPMSG("====  frame_info s ====\n");
	DDPMSG("src cfg:\n");
	DDPMSG("w:%d, h:%d, y_s:%d, uv_s:%d, vert_s:%d, f:%d, pro:%d,",
		info.src.width, info.src.height,
		info.src.y_stride, info.src.uv_stride,
		info.src.vert_stride, info.src.format, info.src.profile);
	DDPMSG("plane_offset[0]:%d, plane_offset[1]:%d, plane_offset[2]:%d, p_c:%d, sec:%d\n",
		info.src.plane_offset[0], info.src.plane_offset[1], info.src.plane_offset[2],
		info.src.plane_cnt, info.src.secure);
	DDPMSG("dest_cnt:%d\n", info.dest_cnt);
	for (; i < MML_MAX_OUTPUTS; ++i) {
		DDPMSG("dest cfg[%d]:\n", i);
		DDPMSG("w:%d, h:%d, y_s:%d, uv_s:%d, vert_s:%d, f:%d, pro:%d,",
			info.dest[i].data.width, info.dest[i].data.height,
			info.dest[i].data.y_stride, info.dest[i].data.uv_stride,
			info.dest[i].data.vert_stride, info.dest[i].data.format,
			info.dest[i].data.profile);
		DDPMSG("plane_offset[0]:%d, plane_offset[1]:%d, plane_offset[2]:%d,",
			info.dest[i].data.plane_offset[0],
			info.dest[i].data.plane_offset[1],
			info.dest[i].data.plane_offset[2]);
		DDPMSG("p_c:%d, sec:%d\n",
			info.dest[i].data.plane_cnt, info.dest[i].data.secure);
		DDPMSG("x_sub_px:%d, y_sub_px:%d, w_sub_px:%d, h_sub_px:%d\n",
			info.dest[i].crop.x_sub_px, info.dest[i].crop.y_sub_px,
			info.dest[i].crop.w_sub_px, info.dest[i].crop.h_sub_px);
		DDPMSG("l:%d, t:%d, w:%d, h:%d\n",
			info.dest[i].compose.left, info.dest[i].compose.top,
			info.dest[i].compose.width, info.dest[i].compose.height);
		DDPMSG("[MMLPQParamParser] en_sharp[%d], en_ur[%d], en_dc[%d],",
			info.dest[i].pq_config.en_sharp, info.dest[i].pq_config.en_ur,
			info.dest[i].pq_config.en_dc);
		DDPMSG("en_color[%d], en_hdr[%d], en_ccorr[%d], en_dre[%d]",
			info.dest[i].pq_config.en_color, info.dest[i].pq_config.en_hdr,
			info.dest[i].pq_config.en_ccorr, info.dest[i].pq_config.en_dre);
		DDPMSG("rotate:%d, flip:%d, pq_config.en:%d\n",
			info.dest[i].rotate, info.dest[i].flip, info.dest[i].pq_config);
	}
	DDPMSG("mode:%d, layer_id:%d\n", info.mode, info.layer_id);
	DDPMSG("====  frame_info e ====\n");
}

//To-Do: need to be remove
void print_mml_submit(struct mml_submit *args)
{
	int i = 0;

	if (!args)
		return;

	DDPMSG("====  %s s ====\n", __func__);
	if (args->job)
		DDPMSG("jobid:%d, fence:%d\n", args->job->jobid, args->job->fence);

	print_mml_frame_info(args->info);
	print_mml_frame_buffer(args->buffer);

	for (i = 0; i < MML_MAX_OUTPUTS; i++) {
		if (args->pq_param[i]) {
			DDPMSG("pq_param[%d]\n", i);
			DDPMSG("e:%d, t:%d, pq_s:%d, l_id:%d, d_id:%d, s_ga:%d, d_ga:%d,",
				args->pq_param[i]->enable, args->pq_param[i]->time_stamp,
				args->pq_param[i]->scenario, args->pq_param[i]->layer_id,
				args->pq_param[i]->disp_id, args->pq_param[i]->src_gamut,
				args->pq_param[i]->dst_gamut);
			DDPMSG("src_hdr_video_mode:%d, video_id:%d, time_stamp:%d,",
				args->pq_param[i]->src_hdr_video_mode,
				args->pq_param[i]->video_param.video_id,
				args->pq_param[i]->video_param.time_stamp);
			DDPMSG("ishdr2sdr:%d, param_table:%d, pq_user_info:%d\n",
				args->pq_param[i]->video_param.ishdr2sdr,
				args->pq_param[i]->video_param.param_table,
				args->pq_param[i]->user_info);
		}
	}
	DDPMSG("sec:%lld, usec:%lld, update:%d\n",
		args->end.sec, args->end.nsec, args->update);
	DDPMSG("====  %s e ====\n", __func__);
}

int mtk_drm_ioctl_mml_gem_submit(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	int ret = 0;
	int i = 0;
	struct mml_submit *submit_user = (struct mml_submit *)data;
	struct mtk_drm_private *priv = dev->dev_private;
	struct mml_drm_ctx *mml_ctx = NULL;
	struct mml_submit *submit_kernel;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;

	DDPINFO("%s:%d +\n", __func__, __LINE__);

	if (!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_MML_PRIMARY)) {
		DDPINFO("%s:%d MTK_DRM_OPT_MML_PRIMARY is not support\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!submit_user)
		return -EFAULT;

	submit_kernel = kzalloc(sizeof(struct mml_submit), GFP_KERNEL);
	if (!submit_kernel) {
		DDPMSG("[%s][%d][%d] kzalloc fail\n", __func__, __LINE__, ret);
		return -EFAULT;
	}

	memcpy(submit_kernel, submit_user, sizeof(struct mml_submit));
	submit_kernel->job = kzalloc(sizeof(struct mml_job), GFP_KERNEL);
	if (!submit_kernel->job) {
		ret = -EFAULT;
		DDPMSG("[%s][%d][%d] kzalloc fail\n", __func__, __LINE__, ret);
		goto err_handle_create;
	}

	if (submit_user->job) {
		ret = copy_from_user(submit_kernel->job, submit_user->job, sizeof(struct mml_job));
		if (ret) {
			DDPMSG("[%s][%d][%d] copy_from_user fail\n", __func__, __LINE__, ret);
			goto err_handle_create;
		}
	} else
		DDPMSG("[%s] submit_user->job is null\n", __func__);

	for (i = 0; i < MML_MAX_OUTPUTS; i++) {
		if (submit_user->pq_param[i]) {
			submit_kernel->pq_param[i] =
				kzalloc(sizeof(struct mml_pq_param), GFP_KERNEL);
			if (!submit_kernel->pq_param[i]) {
				ret = -EFAULT;
				DDPMSG("[%s][%d][%d] kzalloc fail\n", __func__, __LINE__, ret);
				goto err_handle_create;
			}

			ret = copy_from_user(submit_kernel->pq_param[i], submit_user->pq_param[i],
				sizeof(struct mml_pq_param));
			if (ret) {
				DDPMSG("[%s][%d][%d] copy_from_user fail\n",
				__func__, __LINE__, ret);
				goto err_handle_create;
			}
			//copy_from_user(submit_kernel->pq_param[i]->gralloc_extra_handle,
			//	submit_user->pq_param[i]->gralloc_extra_handle, sizeof(void *));
		} else {
			DDPMSG("%s submit_user->pq_param[i] is null\n", __func__);
		}
	}

	crtc = list_first_entry(&(dev)->mode_config.crtc_list,
		typeof(*crtc), head);
	mtk_crtc = to_mtk_crtc(crtc);
	mml_ctx = mtk_drm_get_mml_drm_ctx(dev, crtc);

	if (mtk_crtc->mml_debug & DISP_MML_DBG_LOG) {
		DDPINFO("%s:%d\n", __func__, __LINE__);
		print_mml_submit(submit_kernel);
		DDPINFO("%s:%d\n", __func__, __LINE__);
	}

	if (mml_ctx > 0) {
		DDPINFO("%s:%d mml_drm_submit +\n", __func__, __LINE__);
		ret = mml_drm_submit(mml_ctx, submit_kernel, NULL);
		DDPINFO("%s:%d mml_drm_submit - ret:%d, job(id,fence):(%d,%d)\n",
			__func__, __LINE__, ret,
			submit_kernel->job->jobid, submit_kernel->job->fence);
		if (ret)
			DDPMSG("submit failed: %d\n", ret);
	}

	if (submit_user->job) {
		ret = copy_to_user(submit_user->job, submit_kernel->job, sizeof(struct mml_job));
		if (ret)
			DDPMSG("[%s][%d][%d] copy_to_user fail\n", __func__, __LINE__, ret);
	}

err_handle_create:
	for (i = 0; i < MML_MAX_OUTPUTS; i++) {
		if (submit_kernel->pq_param[i])
			kfree(submit_kernel->pq_param[i]);
	}
	kfree(submit_kernel->job);
	kfree(submit_kernel);

	DDPINFO("%s:%d -\n", __func__, __LINE__);

	return ret;
}

