// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2016 MediaTek Inc.
* Author: PC Chen <pc.chen@mediatek.com>
*       Tiffany Lin <tiffany.lin@mediatek.com>
*/

#include <linux/module.h>
#include <linux/module.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <uapi/linux/dma-heap.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"
#include "mtk_vcodec_dec.h"
#include "vdec_drv_if.h"
#include "venc_drv_if.h"


#define LOG_PARAM_INFO_SIZE 64
#define MAX_SUPPORTED_LOG_PARAMS_COUNT 32
char mtk_vdec_tmp_log[LOG_PROPERTY_SIZE];
char mtk_venc_tmp_log[LOG_PROPERTY_SIZE];
char mtk_vdec_tmp_prop[LOG_PROPERTY_SIZE];
char mtk_venc_tmp_prop[LOG_PROPERTY_SIZE];


#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
extern phys_addr_t vcp_get_reserve_mem_phys(enum vcp_reserve_mem_id_t id);
extern phys_addr_t vcp_get_reserve_mem_virt(enum vcp_reserve_mem_id_t id);
extern phys_addr_t vcp_get_reserve_mem_size(enum vcp_reserve_mem_id_t id);
#endif

/* For encoder, this will enable logs in venc/*/
bool mtk_vcodec_dbg;
EXPORT_SYMBOL_GPL(mtk_vcodec_dbg);

/* For vcodec performance measure */
bool mtk_vcodec_perf;
EXPORT_SYMBOL_GPL(mtk_vcodec_perf);

/* The log level of v4l2 encoder or decoder driver.
 * That is, files under mtk-vcodec/.
 */
int mtk_v4l2_dbg_level;
EXPORT_SYMBOL_GPL(mtk_v4l2_dbg_level);

/* For vcodec vcp debug */
int mtk_vcodec_vcp;
EXPORT_SYMBOL_GPL(mtk_vcodec_vcp);

/* For vdec set property */
char *mtk_vdec_property = "";
EXPORT_SYMBOL(mtk_vdec_property);

/* For venc set property */
char *mtk_venc_property = "";
EXPORT_SYMBOL(mtk_venc_property);

/* For vdec vcp log info */
char *mtk_vdec_vcp_log = "";
EXPORT_SYMBOL(mtk_vdec_vcp_log);

/* For venc vcp log info */
char *mtk_venc_vcp_log = "";
EXPORT_SYMBOL(mtk_venc_vcp_log);

/* For vcp vdec sec mem debug */
int mtk_vdec_sw_mem_sec;
EXPORT_SYMBOL_GPL(mtk_vdec_sw_mem_sec);

struct vcu_v4l2_func vcu_func = { NULL };
EXPORT_SYMBOL_GPL(vcu_func);

/* VCODEC FTRACE */
unsigned long vcodec_get_tracing_mark(void)
{
/*
	static unsigned long __read_mostly tracing_mark_write_addr;

	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr =
			kallsyms_lookup_name("tracing_mark_write");

	return tracing_mark_write_addr;
*/
	return 0;
}
EXPORT_SYMBOL(vcodec_get_tracing_mark);

void __iomem *mtk_vcodec_get_dec_reg_addr(struct mtk_vcodec_ctx *data,
	unsigned int reg_idx)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;

	if (!data || reg_idx >= NUM_MAX_VDEC_REG_BASE) {
		mtk_v4l2_err("Invalid arguments, reg_idx=%d", reg_idx);
		return NULL;
	}
	return ctx->dev->dec_reg_base[reg_idx];
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_dec_reg_addr);

void __iomem *mtk_vcodec_get_enc_reg_addr(struct mtk_vcodec_ctx *data,
	unsigned int reg_idx)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;

	if (!data || reg_idx >= NUM_MAX_VENC_REG_BASE) {
		mtk_v4l2_err("Invalid arguments, reg_idx=%d", reg_idx);
		return NULL;
	}
	return ctx->dev->enc_reg_base[reg_idx];
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_enc_reg_addr);


int mtk_vcodec_mem_alloc(struct mtk_vcodec_ctx *data,
						 struct mtk_vcodec_mem *mem)
{
	unsigned long size;
	struct mtk_vcodec_ctx *ctx;
	struct device *dev;

	if (IS_ERR_OR_NULL(data) || IS_ERR_OR_NULL(mem)) {
		mtk_v4l2_err("invalid arguments, data: %p, mem: %p", data, mem);
		return -EINVAL;
	}

	size = mem->size;
	ctx = (struct mtk_vcodec_ctx *)data;
	dev = &ctx->dev->plat_dev->dev;

	mem->va = dma_alloc_coherent(dev, size, &mem->dma_addr, GFP_KERNEL);

	if (!mem->va) {
		mtk_v4l2_err("%s dma_alloc size=%ld failed!", dev_name(dev),
					 size);
		return -ENOMEM;
	}

	memset(mem->va, 0, size);

	mtk_v4l2_debug(4, "[%d]  - va      = %p", ctx->id, mem->va);
	mtk_v4l2_debug(4, "[%d]  - dma     = 0x%lx", ctx->id,
				   (unsigned long)mem->dma_addr);
	mtk_v4l2_debug(4, "[%d]    size = 0x%lx", ctx->id, size);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_mem_alloc);

void mtk_vcodec_mem_free(struct mtk_vcodec_ctx *data,
						 struct mtk_vcodec_mem *mem)
{
	unsigned long size;
	struct mtk_vcodec_ctx *ctx;
	struct device *dev;

	if (IS_ERR_OR_NULL(data) || IS_ERR_OR_NULL(mem)) {
		mtk_v4l2_err("invalid arguments, data: %p, mem: %p", data, mem);
		return;
	}

	size = mem->size;
	ctx = (struct mtk_vcodec_ctx *)data;
	dev = &ctx->dev->plat_dev->dev;

	if (!mem->va) {
		mtk_v4l2_err("%s dma_free size=%ld failed!", dev_name(dev),
					 size);
		return;
	}

	mtk_v4l2_debug(4, "[%d]  - va      = %p", ctx->id, mem->va);
	mtk_v4l2_debug(4, "[%d]  - dma     = 0x%lx", ctx->id,
				   (unsigned long)mem->dma_addr);
	mtk_v4l2_debug(4, "[%d]    size = 0x%lx", ctx->id, size);

	dma_free_coherent(dev, size, mem->va, mem->dma_addr);
	mem->va = NULL;
	mem->dma_addr = 0;
	mem->size = 0;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_mem_free);

void mtk_vcodec_set_curr_ctx(struct mtk_vcodec_dev *dev,
	struct mtk_vcodec_ctx *ctx, unsigned int hw_id)
{
	unsigned long flags;

	if (IS_ERR_OR_NULL(dev)) {
		mtk_v4l2_err("invalid arguments, dev: %p", dev);
		return;
	}

	spin_lock_irqsave(&dev->irqlock, flags);
	dev->curr_dec_ctx[hw_id] = ctx;
	spin_unlock_irqrestore(&dev->irqlock, flags);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_curr_ctx);

struct mtk_vcodec_ctx *mtk_vcodec_get_curr_ctx(struct mtk_vcodec_dev *dev,
	unsigned int hw_id)
{
	unsigned long flags;
	struct mtk_vcodec_ctx *ctx;

	if (IS_ERR_OR_NULL(dev)) {
		mtk_v4l2_err("invalid arguments, dev: %p", dev);
		return NULL;
	}

	spin_lock_irqsave(&dev->irqlock, flags);
	ctx = dev->curr_dec_ctx[hw_id];
	spin_unlock_irqrestore(&dev->irqlock, flags);
	return ctx;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_curr_ctx);

void mtk_vcodec_add_ctx_list(struct mtk_vcodec_ctx *ctx)
{
	if (IS_ERR_OR_NULL(ctx)) {
		mtk_v4l2_err("invalid arguments, ctx: %p", ctx);
		return;
	}
	mutex_lock(&ctx->dev->ctx_mutex);
	list_add(&ctx->list, &ctx->dev->ctx_list);
	mutex_unlock(&ctx->dev->ctx_mutex);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_add_ctx_list);

void mtk_vcodec_del_ctx_list(struct mtk_vcodec_ctx *ctx)
{
	if (IS_ERR_OR_NULL(ctx)) {
		mtk_v4l2_err("invalid arguments, ctx: %p", ctx);
		return;
	}
	mutex_lock(&ctx->dev->ctx_mutex);
	list_del_init(&ctx->list);
	mutex_unlock(&ctx->dev->ctx_mutex);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_del_ctx_list);


struct vdec_fb *mtk_vcodec_get_fb(struct mtk_vcodec_ctx *ctx)
{
	struct vb2_buffer *dst_buf = NULL;
	struct vdec_fb *pfb;
	struct mtk_video_dec_buf *dst_buf_info;
	struct vb2_v4l2_buffer *dst_vb2_v4l2;
	int i;

	if (!ctx) {
		mtk_v4l2_err("Ctx is NULL!");
		return NULL;
	}

	mtk_v4l2_debug_enter();
	dst_vb2_v4l2 = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	if (dst_vb2_v4l2 != NULL)
		dst_buf = &dst_vb2_v4l2->vb2_buf;
	if (dst_buf != NULL) {
		dst_buf_info = container_of(
			dst_vb2_v4l2, struct mtk_video_dec_buf, vb);
		pfb = &dst_buf_info->frame_buffer;
		pfb->num_planes = dst_vb2_v4l2->vb2_buf.num_planes;
		pfb->index = dst_buf->index;

		mutex_lock(&ctx->buf_lock);
		for (i = 0; i < dst_vb2_v4l2->vb2_buf.num_planes; i++) {
			pfb->fb_base[i].va = vb2_plane_vaddr(dst_buf, i);
			pfb->fb_base[i].dma_addr =
				vb2_dma_contig_plane_dma_addr(dst_buf, i);
			pfb->fb_base[i].size = ctx->picinfo.fb_sz[i];
			pfb->fb_base[i].length = dst_buf->planes[i].length;
			pfb->fb_base[i].dmabuf = dst_buf->planes[i].dbuf;
			if (dst_buf_info->used == false) {
				get_file(dst_buf->planes[i].dbuf->file);
				mtk_v4l2_debug(4, "[Ref cnt] id=%d Ref get dma %p", dst_buf->index,
					dst_buf->planes[i].dbuf);
			}
		}
		pfb->status = FB_ST_INIT;
		dst_buf_info->used = true;

		mtk_v4l2_debug(1, "[%d] id=%d pfb=0x%p %llx VA=%p dma_addr[0]=%lx dma_addr[1]=%lx Size=%zx fd:%x, dma_general_buf = %p, general_buf_fd = %d",
				ctx->id, dst_buf->index, pfb, (unsigned long long)pfb,
				pfb->fb_base[0].va,
				(unsigned long)pfb->fb_base[0].dma_addr,
				(unsigned long)pfb->fb_base[1].dma_addr,
				pfb->fb_base[0].size,
				dst_buf->planes[0].m.fd,
				pfb->dma_general_buf,
				pfb->general_buf_fd);

		dst_vb2_v4l2 = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
		if (dst_vb2_v4l2 != NULL)
			dst_buf = &dst_vb2_v4l2->vb2_buf;
			mtk_v4l2_debug(8, "[%d] index=%d, num_rdy_bufs=%d\n",
				ctx->id, dst_buf->index,
				v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx));

		mutex_unlock(&ctx->buf_lock);
	} else {
		mtk_v4l2_debug(8, "[%d] No free framebuffer in v4l2!!\n", ctx->id);
		pfb = NULL;
	}
	mtk_v4l2_debug_leave();

	return pfb;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_fb);


int v4l2_m2m_buf_queue_check(struct v4l2_m2m_ctx *m2m_ctx,
		void *vbuf)
{
	struct v4l2_m2m_buffer *b;

	if (IS_ERR_OR_NULL(vbuf)) {
		mtk_v4l2_err("Invalid arguments, m2m_ctx=0x%p, vbuf=%p", m2m_ctx, vbuf);
		return -1;
	}
	b = container_of(vbuf, struct v4l2_m2m_buffer, vb);
	mtk_v4l2_debug(8, "[Debug] b %p b->list.next %p prev %p %p %p\n",
		b, b->list.next, b->list.prev,
		LIST_POISON1, LIST_POISON2);

	if (WARN_ON(IS_ERR_OR_NULL(m2m_ctx) ||
		(b->list.next != LIST_POISON1 && b->list.next) ||
		(b->list.prev != LIST_POISON2 && b->list.prev))) {
		v4l2_aee_print("b %p next %p prev %p already in rdyq %p %p\n",
			b, b->list.next, b->list.prev,
			LIST_POISON1, LIST_POISON2);
		return -1;
	}
	v4l2_m2m_buf_queue(m2m_ctx, vbuf);
	return 0;
}
EXPORT_SYMBOL_GPL(v4l2_m2m_buf_queue_check);

int mtk_dma_sync_sg_range(const struct sg_table *sgt,
	struct device *dev, unsigned int size,
	enum dma_data_direction direction)
{
	struct sg_table *sgt_tmp;
	struct scatterlist *s_sgl, *d_sgl;
	unsigned int contig_size = 0;
	int ret, i;

	if (sgt == NULL || dev == NULL || size == 0) {
		mtk_v4l2_err("sgt 0x%x dev 0x%x size %d invalid", sgt, dev, size);
		return -1;
	}

	sgt_tmp = kzalloc(sizeof(*sgt_tmp), GFP_KERNEL);
	if (!sgt_tmp)
		return -1;

	ret = sg_alloc_table(sgt_tmp, sgt->orig_nents, GFP_KERNEL);
	if (ret) {
		mtk_v4l2_debug(0, "sg alloc table failed %d.\n", ret);
		kfree(sgt_tmp);
		return -1;
	}
	sgt_tmp->nents = 0;
	d_sgl = sgt_tmp->sgl;

	for_each_sg(sgt->sgl, s_sgl, sgt->orig_nents, i) {
		mtk_v4l2_debug(4, "%d contig_size %d bytesused %d.\n",
			i, contig_size, size);
		if (contig_size >= size)
			break;
		memcpy(d_sgl, s_sgl, sizeof(*s_sgl));
		contig_size += s_sgl->length;
		d_sgl = sg_next(d_sgl);
		sgt_tmp->nents++;
	}
	if (direction == DMA_TO_DEVICE) {
		dma_sync_sg_for_device(dev, sgt_tmp->sgl, sgt_tmp->nents, direction);
	} else if (direction == DMA_FROM_DEVICE) {
		dma_sync_sg_for_cpu(dev, sgt_tmp->sgl, sgt_tmp->nents, direction);
	} else {
		mtk_v4l2_debug(0, "direction %d not correct\n", direction);
		return -1;
	}
	mtk_v4l2_debug(4, "flush nents %d total nents %d\n",
		sgt_tmp->nents, sgt->orig_nents);
	sg_free_table(sgt_tmp);
	kfree(sgt_tmp);

	return 0;
}
EXPORT_SYMBOL(mtk_dma_sync_sg_range);

void v4l_fill_mtk_fmtdesc(struct v4l2_fmtdesc *fmt)
{
	const char *descr = NULL;
	unsigned int sz;

	if (IS_ERR_OR_NULL(fmt)) {
		mtk_v4l2_err("invalid argument, fmt: %p", fmt);
		return;
	}

	sz = sizeof(fmt->description);

	switch (fmt->pixelformat) {
	case V4L2_PIX_FMT_H265:
	    descr = "H.265"; break;
	case V4L2_PIX_FMT_HEIF:
	    descr = "HEIF"; break;
	case V4L2_PIX_FMT_WMV1:
	    descr = "WMV1"; break;
	case V4L2_PIX_FMT_WMV2:
	    descr = "WMV2"; break;
	case V4L2_PIX_FMT_WMV3:
	    descr = "WMV3"; break;
	case V4L2_PIX_FMT_WVC1:
	    descr = "WVC1"; break;
	case V4L2_PIX_FMT_WMVA:
	    descr = "WMVA"; break;
	case V4L2_PIX_FMT_RV30:
	    descr = "RealVideo 8"; break;
	case V4L2_PIX_FMT_RV40:
	    descr = "RealVideo 9/10"; break;
	case V4L2_PIX_FMT_AV1:
	    descr = "AV1"; break;
	case V4L2_PIX_FMT_MT10S:
	    descr = "MTK 10-bit compressed single"; break;
	case V4L2_PIX_FMT_MT10:
	    descr = "MTK 10-bit compressed"; break;
	case V4L2_PIX_FMT_P010S:
	    descr = "10-bit P010 LSB 6-bit single"; break;
	case V4L2_PIX_FMT_P010M:
	    descr = "10-bit P010 LSB 6-bit"; break;
	case V4L2_PIX_FMT_NV12_AFBC:
	    descr = "AFBC NV12"; break;
	case V4L2_PIX_FMT_NV21_AFBC:
	    descr = "AFBC NV21"; break;
	case V4L2_PIX_FMT_NV12_10B_AFBC:
	    descr = "10-bit AFBC NV12"; break;
	case V4L2_PIX_FMT_RGB32_AFBC:
	    descr = "32-bit AFBC A/XRGB 8-8-8-8"; break;
	case V4L2_PIX_FMT_BGR32_AFBC:
	    descr = "32-bit AFBC A/XBGR 8-8-8-8"; break;
	case V4L2_PIX_FMT_RGBA1010102_AFBC:
	    descr = "10-bit AFBC RGB 2-bit for A"; break;
	case V4L2_PIX_FMT_BGRA1010102_AFBC:
	    descr = "10-bit AFBC BGR 2-bit for A"; break;
	case V4L2_PIX_FMT_ARGB1010102:
	case V4L2_PIX_FMT_ABGR1010102:
	case V4L2_PIX_FMT_RGBA1010102:
	case V4L2_PIX_FMT_BGRA1010102:
	    descr = "10-bit for RGB, 2-bit for A"; break;
	case V4L2_PIX_FMT_MT21:
	case V4L2_PIX_FMT_MT2110T:
	case V4L2_PIX_FMT_MT2110R:
	case V4L2_PIX_FMT_MT21C10T:
	case V4L2_PIX_FMT_MT21C10R:
	case V4L2_PIX_FMT_MT21CS:
	case V4L2_PIX_FMT_MT21S:
	case V4L2_PIX_FMT_MT21S10T:
	case V4L2_PIX_FMT_MT21S10R:
	case V4L2_PIX_FMT_MT21CS10T:
	case V4L2_PIX_FMT_MT21CS10R:
	case V4L2_PIX_FMT_MT21CSA:
	case V4L2_PIX_FMT_MT21S10TJ:
	case V4L2_PIX_FMT_MT21S10RJ:
	case V4L2_PIX_FMT_MT21CS10TJ:
	case V4L2_PIX_FMT_MT21CS10RJ:
		descr = "Mediatek Video Block Format"; break;
	default:
		mtk_v4l2_debug(8, "MTK Unknown pixelformat 0x%08x\n", fmt->pixelformat);
		break;
	}

	if (descr)
		WARN_ON(strscpy(fmt->description, descr, sz) < 0);
}
EXPORT_SYMBOL_GPL(v4l_fill_mtk_fmtdesc);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
#define VCP_CACHE_LINE 128
int mtk_vcodec_alloc_mem(struct vcodec_mem_obj *mem, struct device *dev,
	struct dma_buf_attachment **attach, struct sg_table **sgt)
{
	struct dma_heap *dma_heap;
	struct dma_buf *dbuf;
	__u32 alloc_len;

	if (IS_ERR_OR_NULL(mem) || IS_ERR_OR_NULL(dev) ||
			IS_ERR_OR_NULL(attach) || IS_ERR_OR_NULL(sgt)) {
		mtk_v4l2_err("invalid arguments, mem: %p, dev: %p, attach: %p, sgt: %p",
			mem, dev, attach, sgt);
		return -EINVAL;
	}

	alloc_len = mem->len;
	mem->iova = 0;

	if (mem->len > CODEC_ALLOCATE_MAX_BUFFER_SIZE || mem->len == 0U) {
		mtk_v4l2_err("buffer len = %u invalid", mem->len);
		return -EPERM;
	}

	if (mem->type == MEM_TYPE_FOR_SW ||
	    mem->type == MEM_TYPE_FOR_HW ||
	    mem->type == MEM_TYPE_FOR_UBE_HW) {
		dma_heap = dma_heap_find("mtk_mm");
	} else if (mem->type == MEM_TYPE_FOR_SEC_SW ||
		   mem->type == MEM_TYPE_FOR_SEC_HW ||
		   mem->type == MEM_TYPE_FOR_SEC_UBE_HW) {
		dma_heap = dma_heap_find("mtk_svp_page-uncached");
	} else {
		mtk_v4l2_err("wrong type %u\n", mem->type);
		return -EPERM;
	}

	if (!dma_heap) {
		mtk_v4l2_err("heap find fail\n");
		return -EPERM;
	}

	if (mem->type == MEM_TYPE_FOR_SW ||
		mem->type == MEM_TYPE_FOR_SEC_SW) {
		alloc_len = (ROUND_N(mem->len, VCP_CACHE_LINE) + VCP_CACHE_LINE);
	}

	dbuf = dma_heap_buffer_alloc(dma_heap, alloc_len,
		O_RDWR | O_CLOEXEC, DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR_OR_NULL(dbuf)) {
		mtk_v4l2_err("buffer alloc fail\n");
		return PTR_ERR(dbuf);
	}

	*attach = dma_buf_attach(dbuf, dev);
	if (IS_ERR_OR_NULL(*attach)) {
		mtk_v4l2_err("attach fail, return\n");
		dma_heap_buffer_free(dbuf);
		return PTR_ERR(*attach);
	}
	*sgt = dma_buf_map_attachment(*attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(*sgt)) {
		mtk_v4l2_err("map failed, detach and return\n");
		dma_buf_detach(dbuf, *attach);
		dma_heap_buffer_free(dbuf);
		return PTR_ERR(*sgt);
	}
	mem->va = (__u64)dbuf;
	mem->pa = (__u64)sg_dma_address((*sgt)->sgl);
	mem->iova = (__u64)mem->pa;

	if (mem->va == (__u64)NULL || mem->pa == (__u64)NULL) {
		mtk_v4l2_err("alloc failed, va 0x%llx pa 0x%llx iova 0x%llx len %d type %u\n",
		mem->va, mem->pa, mem->iova, mem->len, mem->type);
		return -EPERM;
	}

	mtk_v4l2_debug(8, "va 0x%llx pa 0x%llx iova 0x%llx len %d type %u\n",
		mem->va, mem->pa, mem->iova, mem->len, mem->type);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_alloc_mem);

int mtk_vcodec_free_mem(struct vcodec_mem_obj *mem, struct device *dev,
	struct dma_buf_attachment *attach, struct sg_table *sgt)
{
	if (IS_ERR_OR_NULL(mem) || IS_ERR_OR_NULL(attach) || IS_ERR_OR_NULL(sgt)) {
		mtk_v4l2_err("invalid arguments, mem: %p, attach: %p, sgt: %p", mem, attach, sgt);
		return -EINVAL;
	}
	if (mem->type == MEM_TYPE_FOR_SW ||
		mem->type == MEM_TYPE_FOR_HW ||
		mem->type == MEM_TYPE_FOR_UBE_HW ||
		mem->type == MEM_TYPE_FOR_SEC_SW ||
		mem->type == MEM_TYPE_FOR_SEC_HW ||
		mem->type == MEM_TYPE_FOR_SEC_UBE_HW) {
		dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach((struct dma_buf *)mem->va, attach);
		dma_heap_buffer_free((struct dma_buf *)mem->va);
	} else {
		mtk_v4l2_err("wrong type %d\n", mem->type);
		return -EPERM;
	}

	mtk_v4l2_debug(8, "va 0x%llx pa 0x%llx iova 0x%llx len %d\n",
		mem->va, mem->pa, mem->iova, mem->len);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_free_mem);
#endif

static void mtk_vcodec_sync_log(struct mtk_vcodec_dev *dev,
	const char *param_key, const char *param_val, enum mtk_vcodec_log_index index)
{
	struct mtk_vcodec_log_param *pram, *tmp;
	struct list_head *plist;
	struct mutex *plist_mutex;
	int param_count;

	if (index == MTK_VCODEC_LOG_INDEX_LOG) {
		plist = &dev->log_param_list;
		plist_mutex = &dev->log_param_mutex;
	} else if (index == MTK_VCODEC_LOG_INDEX_PROP) {
		plist = &dev->prop_param_list;
		plist_mutex = &dev->prop_param_mutex;
	} else {
		mtk_v4l2_err("invalid index: %d", index);
		return;
	}

	mutex_lock(plist_mutex);

	list_for_each_entry(pram, plist, list) {
		// find existed param, replace its value
		if (strcmp(pram->param_key, param_key) == 0) {
			mtk_v4l2_debug(8, "replace old key: %s, value: %s -> %s\n",
				pram->param_key, pram->param_val, param_val);
			memset(pram->param_val, 0x00, LOG_PARAM_INFO_SIZE);
			strncpy(pram->param_val, param_val, LOG_PARAM_INFO_SIZE - 1);
			mutex_unlock(plist_mutex);
			return;
		}
	}

	// remove disabled log param from list if value is empty
	param_count = 0;
	list_for_each_entry_safe(pram, tmp, plist, list) {
		if (strlen(pram->param_val) == 0) {
			mtk_v4l2_debug(8, "remove deprecated key: %s, value: %s\n",
				pram->param_key, pram->param_val);
			list_del_init(&pram->list);
			kfree(pram);
		} else {
			param_count++;
		}
	}

	// don't allow add if param count exceeds the limit
	if (param_count >= MAX_SUPPORTED_LOG_PARAMS_COUNT) {
		mtk_v4l2_debug(0, "cannot add due to param count[%d] exceeds the limit[%d]\n",
			param_count, MAX_SUPPORTED_LOG_PARAMS_COUNT);
		mutex_unlock(plist_mutex);
		return;
	}

	// cannot find, add new
	pram = kzalloc(sizeof(*pram), GFP_KERNEL);
	strncpy(pram->param_key, param_key, LOG_PARAM_INFO_SIZE - 1);
	strncpy(pram->param_val, param_val, LOG_PARAM_INFO_SIZE - 1);
	pram->param_val[LOG_PARAM_INFO_SIZE-1] = '\0';
	mtk_v4l2_debug(8, "add new key: %s, value: %s\n",
		pram->param_key, pram->param_val);
	list_add(&pram->list, plist);

	mutex_unlock(plist_mutex);
}

static void mtk_vcodec_build_log_string(struct mtk_vcodec_dev *dev,
	enum mtk_vcodec_log_index log_index)
{
	struct mtk_vcodec_log_param *pram;
	struct list_head *plist;
	struct mutex *plist_mutex;
	char *vdec_temp_str;
	char *venc_temp_str;
	int str_offset = 0;

	if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
		plist = &dev->log_param_list;
		plist_mutex = &dev->log_param_mutex;
		vdec_temp_str = mtk_vdec_tmp_log;
		venc_temp_str = mtk_venc_tmp_log;
	} else if (log_index == MTK_VCODEC_LOG_INDEX_PROP) {
		plist = &dev->prop_param_list;
		plist_mutex = &dev->prop_param_mutex;
		vdec_temp_str = mtk_vdec_tmp_prop;
		venc_temp_str = mtk_venc_tmp_prop;
	} else {
		mtk_v4l2_err("invalid log_index: %d", log_index);
		return;
	}

	mutex_lock(plist_mutex);

	if (dev->vfd_dec) {
		memset(vdec_temp_str, 0x00, LOG_PROPERTY_SIZE);
		list_for_each_entry(pram, plist, list) {
			mtk_v4l2_debug(8, "existed log param %s: %s\n",
					pram->param_key, pram->param_val);

			str_offset += snprintf(vdec_temp_str + str_offset,
				LOG_PROPERTY_SIZE - str_offset, " %s %s",
				pram->param_key, pram->param_val);
		}
		if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
			mtk_vdec_vcp_log = vdec_temp_str;
			mtk_v4l2_debug(8, "build mtk_vdec_vcp_log: %s\n", mtk_vdec_vcp_log);
		} else if (log_index == MTK_VCODEC_LOG_INDEX_PROP) {
			mtk_vdec_property = vdec_temp_str;
			mtk_v4l2_debug(8, "build mtk_vdec_property: %s\n", mtk_vdec_property);
		}
	} else {
		memset(venc_temp_str, 0x00, LOG_PROPERTY_SIZE);
		list_for_each_entry(pram, plist, list) {
			mtk_v4l2_debug(8, "existed log param %s: %s\n",
					pram->param_key, pram->param_val);

			str_offset += snprintf(venc_temp_str + str_offset,
				LOG_PROPERTY_SIZE - str_offset, " %s %s",
				pram->param_key, pram->param_val);
		}
		if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
			mtk_venc_vcp_log = venc_temp_str;
			mtk_v4l2_debug(8, "build mtk_venc_vcp_log: %s\n", mtk_venc_vcp_log);
		} else if (log_index == MTK_VCODEC_LOG_INDEX_PROP) {
			mtk_venc_property = venc_temp_str;
			mtk_v4l2_debug(8, "build mtk_venc_property: %s\n", mtk_venc_property);
		}
	}
	mutex_unlock(plist_mutex);
}

void mtk_vcodec_set_log(struct mtk_vcodec_dev *dev, const char *val,
	enum mtk_vcodec_log_index log_index)
{
	int i, argc = 0;
	char (*argv)[LOG_PARAM_INFO_SIZE] = NULL;
	char *temp = NULL;
	char *token = NULL;
	long temp_val = 0;
	char *log = NULL;

	if (dev == NULL || val == NULL || strlen(val) == 0)
		return;

	mtk_v4l2_debug(0, "val: %s, log_index: %d", val, log_index);

	argv = kzalloc(MAX_SUPPORTED_LOG_PARAMS_COUNT * 2 * LOG_PARAM_INFO_SIZE, GFP_KERNEL);
	if (!argv)
		return;
	log = kzalloc(LOG_PROPERTY_SIZE, GFP_KERNEL);
	if (!log) {
		kfree(argv);
		return;
	}

	strncpy(log, val, LOG_PROPERTY_SIZE - 1);
	temp = log;
	for (token = strsep(&temp, "\n\r ");
	     token != NULL && argc < MAX_SUPPORTED_LOG_PARAMS_COUNT * 2;
	     token = strsep(&temp, "\n\r ")) {
		if (strlen(token) == 0)
			continue;
		strncpy(argv[argc], token, LOG_PARAM_INFO_SIZE);
		argv[argc][LOG_PARAM_INFO_SIZE - 1] = '\0';
		argc++;
	}

	for (i = 0; i < argc-1; i += 2) {
		if (strlen(argv[i]) == 0)
			continue;
		if (strcmp("-mtk_vcodec_dbg", argv[i]) == 0) {
			if (kstrtol(argv[i+1], 0, &temp_val) == 0)
				mtk_vcodec_dbg = temp_val;
		} else if (strcmp("-mtk_vcodec_perf", argv[i]) == 0) {
			if (kstrtol(argv[i+1], 0, &temp_val) == 0)
				mtk_vcodec_perf = temp_val;
		} else if (strcmp("-mtk_v4l2_dbg_level", argv[i]) == 0) {
			if (kstrtol(argv[i+1], 0, &temp_val) == 0)
				mtk_v4l2_dbg_level = temp_val;
		} else {
			mtk_vcodec_sync_log(dev, argv[i], argv[i+1], log_index);
		}
	}

	mtk_vcodec_build_log_string(dev, log_index);

	if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
		pr_info("----------------Debug Config ----------------\n");
		pr_info("mtk_vcodec_dbg: %d\n", mtk_vcodec_dbg);
		pr_info("mtk_vcodec_perf: %d\n", mtk_vcodec_perf);
		pr_info("mtk_v4l2_dbg_level: %d\n", mtk_v4l2_dbg_level);
	}

	kfree(argv);
	kfree(log);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_log);

long long div_64(long long a, long long b)
{
#if IS_ENABLED(CONFIG_64BIT)
	return (a/b);
#else
	uint32_t rem = 0;
	uint64_t dividend, divisor;

	dividend = (a >= 0) ? a : (-a);
	divisor = (b >= 0) ? b : (-b);
	rem = do_div(dividend, divisor);
	a = ((a < 0) ^ (b < 0)) ?
		(0LL - (long long)dividend) :
		(long long)dividend;
	return a;
#endif
}
EXPORT_SYMBOL_GPL(div_64);

MODULE_LICENSE("GPL v2");

