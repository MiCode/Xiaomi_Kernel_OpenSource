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

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"
#include "mtk_vcodec_dec.h"

#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCP)
extern phys_addr_t scp_get_reserve_mem_phys(enum scp_reserve_mem_id_t id);
extern phys_addr_t scp_get_reserve_mem_virt(enum scp_reserve_mem_id_t id);
extern phys_addr_t scp_get_reserve_mem_size(enum scp_reserve_mem_id_t id);
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
	unsigned long size = mem->size;
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;
	struct device *dev = &ctx->dev->plat_dev->dev;

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
	unsigned long size = mem->size;
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;
	struct device *dev = &ctx->dev->plat_dev->dev;

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

	spin_lock_irqsave(&dev->irqlock, flags);
	ctx = dev->curr_dec_ctx[hw_id];
	spin_unlock_irqrestore(&dev->irqlock, flags);
	return ctx;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_curr_ctx);


struct vdec_fb *mtk_vcodec_get_fb(struct mtk_vcodec_ctx *ctx)
{
	struct vb2_buffer *dst_buf = NULL;
	struct vdec_fb *pfb;
	struct mtk_video_dec_buf *dst_buf_info;
	struct vb2_v4l2_buffer *dst_vb2_v4l2;
	int i;
	unsigned int num_rdy_bufs;

	if (!ctx) {
		mtk_v4l2_err("Ctx is NULL!");
		return NULL;
	}

	mtk_v4l2_debug_enter();
	dst_vb2_v4l2 = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	if (dst_vb2_v4l2 != NULL) {
		dst_buf = &dst_vb2_v4l2->vb2_buf;
		if (ctx->dec_eos_vb == (void *)dst_vb2_v4l2) {
			num_rdy_bufs = v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx);
			mtk_v4l2_debug(8, "[%d] Find EOS framebuffer in v4l2 (num_rdy_bufs %d), get next",
				ctx->id, num_rdy_bufs);
			if (num_rdy_bufs > 1) {
				dst_vb2_v4l2 = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
				v4l2_m2m_buf_queue_check(
					ctx->m2m_ctx, dst_vb2_v4l2);
				dst_vb2_v4l2 = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
				dst_buf = &dst_vb2_v4l2->vb2_buf;
			} else
				dst_buf = NULL;
		}
	}
	if (dst_buf != NULL) {
		dst_buf_info = container_of(
			dst_vb2_v4l2, struct mtk_video_dec_buf, vb);
		pfb = &dst_buf_info->frame_buffer;
		pfb->num_planes = dst_vb2_v4l2->vb2_buf.num_planes;
		pfb->index = dst_buf->index;

		for (i = 0; i < dst_vb2_v4l2->vb2_buf.num_planes; i++) {
			pfb->fb_base[i].va = vb2_plane_vaddr(dst_buf, i);
#ifdef CONFIG_VB2_MEDIATEK_DMA_SG
			pfb->fb_base[i].dma_addr =
				mtk_vb2_dma_contig_plane_dma_addr(dst_buf, i);
#else
			pfb->fb_base[i].dma_addr =
				vb2_dma_contig_plane_dma_addr(dst_buf, i);
#endif
			pfb->fb_base[i].size = ctx->picinfo.fb_sz[i];
			pfb->fb_base[i].length = dst_buf->planes[i].length;
			pfb->fb_base[i].dmabuf = dst_buf->planes[i].dbuf;
		}

		pfb->status = 0;
		mtk_v4l2_debug(1, "[%d] id=%d pfb=0x%p %llx VA=%p dma_addr[0]=%p dma_addr[1]=%p Size=%zx fd:%x, dma_general_buf = %p, general_buf_fd = %d",
				ctx->id, dst_buf->index, pfb, (unsigned long long)pfb,
				pfb->fb_base[0].va,
				&pfb->fb_base[0].dma_addr,
				&pfb->fb_base[1].dma_addr,
				pfb->fb_base[0].size,
				dst_buf->planes[0].m.fd,
				pfb->dma_general_buf,
				pfb->general_buf_fd);

		mutex_lock(&ctx->buf_lock);
		dst_buf_info->used = true;
		mutex_unlock(&ctx->buf_lock);
		dst_vb2_v4l2 = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
		if (dst_vb2_v4l2 != NULL)
			dst_buf = &dst_vb2_v4l2->vb2_buf;
			mtk_v4l2_debug(8, "[%d] index=%d, num_rdy_bufs=%d\n",
				ctx->id, dst_buf->index,
				v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx));
	} else {
		mtk_v4l2_debug(8, "[%d] No free framebuffer in v4l2!!\n", ctx->id);
		pfb = NULL;
	}
	mtk_v4l2_debug_leave();

	return pfb;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_fb);


void v4l2_m2m_buf_queue_check(struct v4l2_m2m_ctx *m2m_ctx,
		void *vbuf)
{
	struct v4l2_m2m_buffer *b = container_of(vbuf,
				struct v4l2_m2m_buffer, vb);
	mtk_v4l2_debug(8, "[Debug] b %p b->list.next %p prev %p %p %p\n",
		b, b->list.next, b->list.prev,
		LIST_POISON1, LIST_POISON2);

	if (WARN_ON(IS_ERR_OR_NULL(m2m_ctx) ||
		(b->list.next != LIST_POISON1 && b->list.next) ||
		(b->list.prev != LIST_POISON2 && b->list.prev))) {
		v4l2_aee_print("b %p next %p prev %p already in rdyq %p %p\n",
			b, b->list.next, b->list.prev,
			LIST_POISON1, LIST_POISON2);
		return;
	}
	v4l2_m2m_buf_queue(m2m_ctx, vbuf);
}
EXPORT_SYMBOL_GPL(v4l2_m2m_buf_queue_check);

void v4l_fill_mtk_fmtdesc(struct v4l2_fmtdesc *fmt)
{
	const char *descr = NULL;
	const unsigned int sz = sizeof(fmt->description);

	switch (fmt->pixelformat) {
	case V4L2_PIX_FMT_H265:
	    descr = "H.265"; break;
	case V4L2_PIX_FMT_HEIF:
	    descr = "HEIF"; break;
	case V4L2_PIX_FMT_S263:
	    descr = "S.263"; break;
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
	    mtk_v4l2_debug(0, "MTK Unknown pixelformat 0x%08x\n", fmt->pixelformat);
	}

	if (descr)
		strscpy(fmt->description, descr, sz);

}
EXPORT_SYMBOL_GPL(v4l_fill_mtk_fmtdesc);

#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCP)
int mtk_vcodec_alloc_mem(struct vcodec_mem_obj *mem, struct device *dev, enum scp_reserve_mem_id_t res_mem_id, int *mem_slot_stat)
{
	dma_addr_t dma_addr;

	if (mem->type == MEM_TYPE_FOR_SW) {
		int ret;
		ret = mtk_vcodec_get_reserve_mem_slot(mem, res_mem_id, mem_slot_stat);
		if(ret)
			return ret;
	} else if (mem->type == MEM_TYPE_FOR_HW) {
		mem->va = (__u64)dma_alloc_attrs(dev,
			mem->len, &dma_addr, GFP_KERNEL, 0);
		if (IS_ERR_OR_NULL((void *)mem->va))
			return -ENOMEM;
		else {
			mem->iova = (__u64)dma_addr;
			mem->pa = 0;
		}
	} else if (mem->type == MEM_TYPE_FOR_SEC) {
		// TODO: alloc secure memory
	} else {
		mtk_v4l2_err("wrong type %u\n", mem->type);
		return -EPERM;
	}

	mtk_v4l2_debug(8, "va 0x%llx pa 0x%llx iova 0x%llx len %d\n", mem->va, mem->pa, mem->iova, mem->len);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_alloc_mem);

int mtk_vcodec_free_mem(struct vcodec_mem_obj *mem, struct device *dev, enum scp_reserve_mem_id_t res_mem_id, int *mem_slot_stat)
{
	if (mem->type == MEM_TYPE_FOR_SW){
		int ret;
		ret = mtk_vcodec_put_reserve_mem_slot(mem, res_mem_id, mem_slot_stat);
		if(ret)
			return ret;
	} else if (mem->type == MEM_TYPE_FOR_HW) {
		if (IS_ERR_OR_NULL((void *)mem->va))
			return -EFAULT;
		else
			dma_free_attrs(dev, mem->len, (void *)mem->va, (dma_addr_t)mem->iova, 0);
	} else if (mem->type == MEM_TYPE_FOR_SEC) {
		// TODO: free secure memory
	} else {
		mtk_v4l2_err("wrong type %d\n", mem->type);
		return -EPERM;
	}

	mtk_v4l2_debug(8, "va 0x%llx pa 0x%llx iova 0x%llx len %d\n", mem->va, mem->pa, mem->iova, mem->len);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_free_mem);

int mtk_vcodec_init_reserve_mem_slot(enum scp_reserve_mem_id_t res_mem_id, int **mem_slot_stat)
{
	__u64 total_mem = (__u64)scp_get_reserve_mem_size(res_mem_id);
	__u64 va_start = (__u64)scp_get_reserve_mem_virt(res_mem_id);
	__u64 pa_start = (__u64)scp_get_reserve_mem_phys(res_mem_id);
	__u64 slot_range = mem_slot_range;

	int total_slot_num = (int)(total_mem/slot_range);
	int slot;

	mtk_v4l2_debug(8, "res_mem_id %d, total_mem (%llu), mem_slot_range (%llu), total_slot_num(%d), va_start:0x%llx, pa_start:0x%llx\n",
		res_mem_id, total_mem, slot_range, total_slot_num, va_start, pa_start);

	if (res_mem_id != VDEC_WORK_ID && res_mem_id != VENC_WORK_ID) {
		mtk_v4l2_err("[err] unknown res_mem_id (%d)\n", res_mem_id);
		return -EPERM;
	}

	*mem_slot_stat = kmalloc_array(total_slot_num, sizeof(int), GFP_KERNEL);
	if (*mem_slot_stat == NULL) {
		mtk_v4l2_err("[err] kmalloc_array mem_slot_stat (size %d) failed\n", total_slot_num);
		return -ENOMEM;
	}

	for (slot = 0; slot < total_slot_num; slot++) {
		*(*mem_slot_stat + slot) = 0;
	}
	mtk_v4l2_debug(8, "init mem_slot_stat 0x%08x, total_slot_num %d\n", *mem_slot_stat, total_slot_num);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_init_reserve_mem_slot);

int mtk_vcodec_get_reserve_mem_slot(struct vcodec_mem_obj *mem, enum scp_reserve_mem_id_t res_mem_id, int *mem_slot_stat)
{
	__u64 total_mem = (__u64)scp_get_reserve_mem_size(res_mem_id);
	__u64 va_start = (__u64)scp_get_reserve_mem_virt(res_mem_id);
	__u64 pa_start = (__u64)scp_get_reserve_mem_phys(res_mem_id);
	__u64 slot_range = mem_slot_range;
	int total_slot_num = (int)(total_mem/slot_range), slot;

	if (res_mem_id != VDEC_WORK_ID && res_mem_id != VENC_WORK_ID) {
		mtk_v4l2_err("[err] unknown res_mem_id (%d)\n", res_mem_id);
		return -EPERM;
	}

	if ((mem->len) > slot_range) {
		mtk_v4l2_err("[err] Allocate size exit max memory slot %llu\n", slot_range);
		return -EPERM;
	}

	if (mem_slot_stat == NULL) {
		mtk_v4l2_err("[err] %s mem_slot_stat is null\n", __func__);
		return -EPERM;
	}

	for (slot = 0; slot < total_slot_num; slot++) {
		if(mem_slot_stat[slot] == 0) /* 0: memory slot is available */
			break;
	}

	if (slot == total_slot_num) {
		mtk_v4l2_err("[err] No available reserved memory\n");
		return -ENOMEM;
	}
	else {
		mem_slot_stat[slot] = 1; /* 1: memory slot is using */
		mem->va = va_start + (__u64)slot * slot_range;
		mem->pa = pa_start + (__u64)slot * slot_range;
		mem->iova = 0;
		mtk_v4l2_debug(8, "va 0x%llx pa 0x%llx iova 0x%llx len %d\n", mem->va, mem->pa, mem->iova, mem->len);
		return 0;
	}
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_reserve_mem_slot);

int mtk_vcodec_put_reserve_mem_slot(struct vcodec_mem_obj *mem, enum scp_reserve_mem_id_t res_mem_id, int *mem_slot_stat)
{

	__u64 total_mem = (__u64)scp_get_reserve_mem_size(res_mem_id);
	__u64 va_start = (__u64)scp_get_reserve_mem_virt(res_mem_id);
	__u64 pa_start = (__u64)scp_get_reserve_mem_phys(res_mem_id);
	__u64 slot_range = mem_slot_range;
	int slot;
	__u64 va_free = mem->va, pa_free = mem->pa;

	if (res_mem_id != VDEC_WORK_ID && res_mem_id != VENC_WORK_ID) {
		mtk_v4l2_err("[err] unknown res_mem_id (%d)\n", res_mem_id);
		return -EPERM;
	}

	if ((mem->len) > slot_range) {
		mtk_v4l2_err("[err] Freed size exit max memory slot %llu\n", mem_slot_range);
		return -EPERM;
	}

	if (mem_slot_stat == NULL) {
		mtk_v4l2_err("[err] %s mem_slot_stat is null\n", __func__);
		return -EPERM;
	}

	/* check whether the freed address out of reserved memory range */
	if ((pa_free < pa_start) || (va_free < va_start) || (pa_free > (pa_start + total_mem)) || (va_free > (va_start + total_mem))) {
		mtk_v4l2_err("[err] Freed address is not in the reserved memory pa %llu, va %llu, pa range(%llu,%llu), va range (%llu,%llu)\n"
			, pa_free, va_free, pa_start, pa_start + total_mem, va_start, va_start + total_mem);
		return -EPERM;
	}

	/* check whether the freed address is on the start of one slot */
	if (((pa_free - pa_start) % slot_range != 0) ||  ((va_free - va_start) % slot_range != 0)) {
		mtk_v4l2_err("[err] Freed address is not on the slot pa %llu, va %llu\n", pa_free, va_free);
		return -EPERM;
	}/* check whether the freed address of va and pa are on the same slot */
	else if ((pa_free - pa_start) / slot_range != (va_free - va_start) / slot_range) {
		mtk_v4l2_err("[err] Freed address of pa & va are not in the same slot pa %llu, va %llu\n", pa_free, va_free);
		return -EPERM;
	}
	else {
		slot = (pa_free - pa_start) / slot_range;
		if (mem_slot_stat[slot] == 0) {
			mtk_v4l2_err("[err] Freed slot status %d is wrong, expected is 1\n", slot);
		}

		mem_slot_stat[slot] = 0;
		mem->va = 0;
		mem->pa = 0;
		mem->iova = 0;
		mtk_v4l2_debug(8, "va 0x%llx pa 0x%llx iova 0x%llx len %d\n", mem->va, mem->pa, mem->iova, mem->len);
		return 0;
	}
}
EXPORT_SYMBOL(mtk_vcodec_put_reserve_mem_slot);
#endif

MODULE_LICENSE("GPL v2");

