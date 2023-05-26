// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/module.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"
#include "mtk_vcodec_dec.h"
#include "vdec_drv_if.h"
#include "venc_drv_if.h"

#define LOG_INFO_SIZE 64
#define MAX_SUPPORTED_LOG_PARAMS_COUNT 12

/* For encoder, this will enable logs in venc/*/
bool mtk_vcodec_dbg;
EXPORT_SYMBOL(mtk_vcodec_dbg);

/* For vcodec performance measure */
bool mtk_vcodec_perf;
EXPORT_SYMBOL(mtk_vcodec_perf);

/* The log level of v4l2 encoder or decoder driver.
 * That is, files under mtk-vcodec/.
 */
int mtk_v4l2_dbg_level;
EXPORT_SYMBOL(mtk_v4l2_dbg_level);

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
EXPORT_SYMBOL(mtk_vcodec_get_dec_reg_addr);

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
EXPORT_SYMBOL(mtk_vcodec_get_enc_reg_addr);


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
EXPORT_SYMBOL(mtk_vcodec_mem_alloc);

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
EXPORT_SYMBOL(mtk_vcodec_mem_free);

void mtk_vcodec_set_curr_ctx(struct mtk_vcodec_dev *dev,
	struct mtk_vcodec_ctx *ctx, unsigned int hw_id)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->irqlock, flags);
	dev->curr_dec_ctx[hw_id] = ctx;
	spin_unlock_irqrestore(&dev->irqlock, flags);
}
EXPORT_SYMBOL(mtk_vcodec_set_curr_ctx);

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
EXPORT_SYMBOL(mtk_vcodec_get_curr_ctx);

void mtk_vcodec_add_ctx_list(struct mtk_vcodec_ctx *ctx)
{
	mutex_lock(&ctx->dev->ctx_mutex);
	list_add(&ctx->list, &ctx->dev->ctx_list);
	mutex_unlock(&ctx->dev->ctx_mutex);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_add_ctx_list);

void mtk_vcodec_del_ctx_list(struct mtk_vcodec_ctx *ctx)
{
	mutex_lock(&ctx->dev->ctx_mutex);
	list_del_init(&ctx->list);
	mutex_unlock(&ctx->dev->ctx_mutex);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_del_ctx_list);


struct vdec_fb *mtk_vcodec_get_fb(struct mtk_vcodec_ctx *ctx)
{
	struct vb2_buffer *dst_buf, *src_buf;
	struct vdec_fb *pfb;
	struct mtk_video_dec_buf *dst_buf_info;
	struct vb2_v4l2_buffer *dst_vb2_v4l2, *src_vb2_v4l2;
	int i;

	if (!ctx) {
		mtk_v4l2_err("Ctx is NULL!");
		return NULL;
	}

	/* for getting timestamp*/
	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	src_vb2_v4l2 = container_of(src_buf, struct vb2_v4l2_buffer, vb2_buf);

	mtk_v4l2_debug_enter();
	dst_buf = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	if (dst_buf != NULL) {
		dst_vb2_v4l2 = container_of(
			dst_buf, struct vb2_v4l2_buffer, vb2_buf);
		dst_buf_info = container_of(
			dst_vb2_v4l2, struct mtk_video_dec_buf, vb);
		pfb = &dst_buf_info->frame_buffer;
		pfb->num_planes = dst_vb2_v4l2->vb2_buf.num_planes;
		pfb->index = dst_buf->index;

		mutex_lock(&ctx->buf_lock);
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
			if (dst_buf_info->used == false) {
				get_file(dst_buf->planes[i].dbuf->file);
				mtk_v4l2_debug(4, "[Ref cnt] id=%d Ref get dma %p", dst_buf->index,
					dst_buf->planes[i].dbuf);
			}
		}
		pfb->status = 0;
		dst_buf_info->used = true;
		mutex_unlock(&ctx->buf_lock);

		mtk_v4l2_debug(1, "[%d] id=%d pfb=0x%p %llx VA=%p dma_addr[0]=%lx dma_addr[1]=%lx Size=%zx fd:%x, dma_general_buf = %p, general_buf_fd = %d",
				ctx->id, dst_buf->index, pfb, (unsigned long long)pfb,
				pfb->fb_base[0].va,
				(unsigned long)pfb->fb_base[0].dma_addr,
				(unsigned long)pfb->fb_base[1].dma_addr,
				pfb->fb_base[0].size,
				dst_buf->planes[0].m.fd,
				pfb->dma_general_buf,
				pfb->general_buf_fd);

		dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
		if (dst_buf != NULL)
			mtk_v4l2_debug(8, "[%d] index=%d, num_rdy_bufs=%d\n",
				ctx->id, dst_buf->index,
				v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx));
	} else {
		mtk_v4l2_err("[%d] No free framebuffer in v4l2!!\n", ctx->id);
		pfb = NULL;
	}
	mtk_v4l2_debug_leave();

	return pfb;
}
EXPORT_SYMBOL(mtk_vcodec_get_fb);


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
EXPORT_SYMBOL(v4l2_m2m_buf_queue_check);

void mtk_vcodec_set_log(struct mtk_vcodec_ctx *ctx, char *val)
{
	int i, argc = 0, ret = 0, argcMex = 0;
	//char argv[MAX_SUPPORTED_LOG_PARAMS_COUNT * 2][LOG_INFO_SIZE] = {0};
	char *argv[MAX_SUPPORTED_LOG_PARAMS_COUNT * 2];
	char *temp = NULL;
	char *token = NULL;
	int max_cpy_len = 0;
	long temp_val = 0;
	char vcu_log[LOG_INFO_SIZE] = {0};
	struct venc_enc_param enc_prm;

	pr_info("%s: %s", __func__, val);

	if (val == NULL) {
		mtk_v4l2_err("cannot set log due to input is null");
		return;
	}

	for (i = 0; i < MAX_SUPPORTED_LOG_PARAMS_COUNT * 2; i++)
		argv[i] = kzalloc(LOG_INFO_SIZE, GFP_KERNEL);

	temp = val;
	for (token = strsep(&temp, " "); token != NULL; token = strsep(&temp, " ")) {
		max_cpy_len = strnlen(token, LOG_INFO_SIZE - 1);
		if (argc >= 0 && argv[argc]) {
			argcMex = (LOG_INFO_SIZE-1 > max_cpy_len) ?
				max_cpy_len : (LOG_INFO_SIZE-2);
			strncpy(argv[argc], token, argcMex);
			argv[argc][argcMex+1] = '\0';
		}
		argc++;
		if (argc >= MAX_SUPPORTED_LOG_PARAMS_COUNT * 2)
			break;
	}

	argcMex = (argc >= MAX_SUPPORTED_LOG_PARAMS_COUNT * 2
			? MAX_SUPPORTED_LOG_PARAMS_COUNT * 2 : argc);
	for (i = 0; i < argcMex; i++) {
		if (argv[i] != NULL)
			argv[i][LOG_INFO_SIZE-1] = '\0';
		else
			continue;
		if ((i < argcMex-1) && strcmp("-mtk_vcodec_dbg", argv[i]) == 0) {
			if (kstrtol(argv[++i], 0, &temp_val) == 0)
				mtk_vcodec_dbg = temp_val;
		} else if ((i < argcMex-1) && strcmp("-mtk_vcodec_perf", argv[i]) == 0) {
			if (kstrtol(argv[++i], 0, &temp_val) == 0)
				mtk_vcodec_perf = temp_val;
		} else if ((i < argcMex-1) && strcmp("-mtk_v4l2_dbg_level", argv[i]) == 0) {
			if (kstrtol(argv[++i], 0, &temp_val) == 0)
				mtk_v4l2_dbg_level = temp_val;
		} else if (i < argcMex-1) {
			memset(vcu_log, 0x00, LOG_INFO_SIZE);
			if (argv[i+1] != NULL) {
				argv[i+1][LOG_INFO_SIZE-1] = '\0';
				ret = snprintf(vcu_log, LOG_INFO_SIZE, "%s %s", argv[i], argv[i+1]);
			} else {
				pr_info("[MTK_V4L2] vcu_log input arg[%d] error: Null value", i+1);
				break;
			}
			if (ret < 0) {
				pr_info("[MTK_V4L2] vcu_log snprintf error: %d", ret);
				break;
			}
			if (ctx->type == MTK_INST_DECODER) {
				vdec_if_init(ctx, V4L2_CID_MPEG_MTK_LOG);
				vdec_if_set_param(ctx, SET_PARAM_DEC_LOG, vcu_log);
			} else {
				memset(&enc_prm, 0, sizeof(enc_prm));
				enc_prm.log = vcu_log;
				venc_if_init(ctx, V4L2_CID_MPEG_MTK_LOG);
				venc_if_set_param(ctx, VENC_SET_PARAM_LOG, &enc_prm);
			}
			i++;
		}
	}

	for (i = 0; i < MAX_SUPPORTED_LOG_PARAMS_COUNT * 2; i++) {
		if (argv[i] != NULL)
			kfree(argv[i]);
	}

	pr_info("----------------Debug Config ----------------\n");
	pr_info("mtk_vcodec_dbg: %d\n", mtk_vcodec_dbg);
	pr_info("mtk_vcodec_perf: %d\n", mtk_vcodec_perf);
	pr_info("mtk_v4l2_dbg_level: %d\n", mtk_v4l2_dbg_level);
}

