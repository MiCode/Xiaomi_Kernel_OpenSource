/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Jungchang Tsao <jungchang.tsao@mediatek.com>
 *         Daniel Hsiao <daniel.hsiao@mediatek.com>
 *         PoChun Lin <pochun.lin@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_enc.h"
#include "mtk_vcodec_pm.h"
#include "mtk_vpu.h"

#include "venc_h264_if.h"
#include "venc_h264_vpu.h"

#define VENC_PIC_BITSTREAM_BYTE_CNT 0x0098

static inline void h264_write_reg(struct venc_h264_inst *inst, u32 addr,
				  u32 val)
{
	writel(val, inst->hw_base + addr);
}

static inline u32 h264_read_reg(struct venc_h264_inst *inst, u32 addr)
{
	return readl(inst->hw_base + addr);
}

enum venc_h264_irq_status {
	H264_IRQ_STATUS_ENC_SPS_INT = (1 << 0),
	H264_IRQ_STATUS_ENC_PPS_INT = (1 << 1),
	H264_IRQ_STATUS_ENC_FRM_INT = (1 << 2),
};

static int h264_enc_alloc_work_buf(struct venc_h264_inst *inst, void *param)
{
	int i, j;
	int ret = 0;
	struct venc_h264_vpu_buf *wb = inst->vpu_inst.drv->work_bufs;
	struct venc_enc_prm *enc_param = param;

	mtk_vcodec_debug_enter(inst);

	for (i = 0; i < VENC_H264_VPU_WORK_BUF_MAX; i++) {
		/*
		 * This 'wb' structure is set by VPU side and shared to AP for
		 * buffer allocation and IO virtual addr mapping. For most of
		 * the buffers, AP will allocate the buffer according to 'size'
		 * field and store the IO virtual addr in 'iova' field. There
		 * are two exceptions:
		 * (1) RC_CODE buffer, it's pre-allocated in the VPU side, and
		 * save the VPU addr in the 'vpua' field. The AP will translate
		 * the VPU addr to the corresponding IO virtual addr and store
		 * in 'iova' field for reg setting in VPU side.
		 * (2) SKIP_FRAME buffer, it's pre-allocated in the VPU side,
		 * and save the VPU addr in the 'vpua' field. The AP will
		 * translate the VPU addr to the corresponding AP side virtual
		 * address and do some memcpy access to move to bitstream buffer
		 * assigned by v4l2 layer.
		 */
		if (i == VENC_H264_VPU_WORK_BUF_RC_CODE) {
			inst->work_bufs[i].size = wb[i].size;
			inst->work_bufs[i].va = vpu_mapping_dm_addr(
				inst->dev, wb[i].vpua);
			inst->work_bufs[i].dma_addr =
				vpu_mapping_iommu_dm_addr(
				inst->dev, wb[i].vpua);
			wb[i].iova = inst->work_bufs[i].dma_addr;
		} else if (i == VENC_H264_VPU_WORK_BUF_SKIP_FRAME) {
			inst->work_bufs[i].size = wb[i].size;
			inst->work_bufs[i].va = vpu_mapping_dm_addr(
				inst->dev, wb[i].vpua);
			inst->work_bufs[i].dma_addr = 0;
			wb[i].iova = inst->work_bufs[i].dma_addr;
		} else if (i == VENC_H264_VPU_WORK_BUF_SRC_LUMA ||
			   i == VENC_H264_VPU_WORK_BUF_SRC_CHROMA ||
			   i == VENC_H264_VPU_WORK_BUF_SRC_CHROMA_CB ||
			   i == VENC_H264_VPU_WORK_BUF_SRC_CHROMA_CR) {
			inst->work_bufs[i].size = wb[i].size;
			inst->work_bufs[i].dma_addr = 0;
			inst->work_bufs[i].va = NULL;
		} else {
			inst->work_bufs[i].size = wb[i].size;
			if (mtk_vcodec_mem_alloc(inst->ctx,
						 &inst->work_bufs[i])) {
				mtk_vcodec_err(inst,
					       "cannot allocate buf %d", i);
				ret = -ENOMEM;
				goto err_alloc;
			}
			wb[i].iova = inst->work_bufs[i].dma_addr;
		}
		mtk_vcodec_debug(inst, "buf[%d] va=0x%p iova=0x%p size=0x%zx",
				 i, inst->work_bufs[i].va,
				 (void *)inst->work_bufs[i].dma_addr,
				 inst->work_bufs[i].size);
	}

	if (enc_param->input_fourcc == VENC_YUV_FORMAT_NV12 ||
	    enc_param->input_fourcc == VENC_YUV_FORMAT_NV21) {
		enc_param->sizeimage[0] =
			inst->work_bufs[VENC_H264_VPU_WORK_BUF_SRC_LUMA].size;
		enc_param->sizeimage[1] =
			inst->work_bufs[VENC_H264_VPU_WORK_BUF_SRC_CHROMA].size;
		enc_param->sizeimage[2] = 0;
	} else {
		enc_param->sizeimage[0] =
			inst->work_bufs[VENC_H264_VPU_WORK_BUF_SRC_LUMA].size;
		enc_param->sizeimage[1] =
			inst->work_bufs[VENC_H264_VPU_WORK_BUF_SRC_CHROMA_CB].size;
		enc_param->sizeimage[2] =
			inst->work_bufs[VENC_H264_VPU_WORK_BUF_SRC_CHROMA_CR].size;
	}

	/* the pps_buf is used by AP side only */
	inst->pps_buf.size = 128;
	if (mtk_vcodec_mem_alloc(inst->ctx,
				 &inst->pps_buf)) {
		mtk_vcodec_err(inst, "cannot allocate pps_buf");
		ret = -ENOMEM;
		goto err_alloc;
	}
	mtk_vcodec_debug_leave(inst);

	return ret;

err_alloc:
	for (j = 0; j < i; j++) {
		if ((j != VENC_H264_VPU_WORK_BUF_RC_CODE) &&
		    (j != VENC_H264_VPU_WORK_BUF_SKIP_FRAME))
			if (inst->work_bufs[i].va != NULL)
				mtk_vcodec_mem_free(inst->ctx,
						    &inst->work_bufs[j]);
	}

	return ret;
}

static void h264_enc_free_work_buf(struct venc_h264_inst *inst)
{
	int i;

	mtk_vcodec_debug_enter(inst);
	for (i = 0; i < VENC_H264_VPU_WORK_BUF_MAX; i++) {
		if ((i != VENC_H264_VPU_WORK_BUF_RC_CODE) &&
		    (i != VENC_H264_VPU_WORK_BUF_SKIP_FRAME))
			if (inst->work_bufs[i].va != NULL)
				mtk_vcodec_mem_free(inst->ctx,
						    &inst->work_bufs[i]);
	}
	mtk_vcodec_mem_free(inst->ctx, &inst->pps_buf);
	mtk_vcodec_debug_leave(inst);
}

static unsigned int h264_enc_wait_venc_done(struct venc_h264_inst *inst)
{
	unsigned int irq_status = 0;
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)inst->ctx;

	mtk_vcodec_debug_enter(inst);
	mtk_vcodec_wait_for_done_ctx(ctx, MTK_INST_IRQ_RECEIVED,
				     WAIT_INTR_TIMEOUT, true);
	irq_status = ctx->irq_status;
	mtk_vcodec_debug(inst, "irq_status %x <-", irq_status);

	return irq_status;
}

static int h264_encode_sps(struct venc_h264_inst *inst,
			   struct mtk_vcodec_mem *bs_buf,
			   unsigned int *bs_size)
{
	int ret = 0;
	unsigned int irq_status;

	mtk_vcodec_debug_enter(inst);

	ret = h264_enc_vpu_encode(inst, H264_BS_MODE_SPS, NULL,
				  bs_buf, bs_size);
	if (ret)
		return ret;

	irq_status = h264_enc_wait_venc_done(inst);
	if (irq_status != H264_IRQ_STATUS_ENC_SPS_INT) {
		mtk_vcodec_err(inst, "expect irq status %d",
			       H264_IRQ_STATUS_ENC_SPS_INT);
		return -EINVAL;
	}

	*bs_size = h264_read_reg(inst, VENC_PIC_BITSTREAM_BYTE_CNT);
	mtk_vcodec_debug(inst, "bs size %d <-", *bs_size);

	return ret;
}

static int h264_encode_pps(struct venc_h264_inst *inst,
			   struct mtk_vcodec_mem *bs_buf,
			   unsigned int *bs_size)
{
	int ret = 0;
	unsigned int irq_status;

	mtk_vcodec_debug_enter(inst);

	ret = h264_enc_vpu_encode(inst, H264_BS_MODE_PPS, NULL,
				  bs_buf, bs_size);
	if (ret)
		return ret;

	irq_status = h264_enc_wait_venc_done(inst);
	if (irq_status != H264_IRQ_STATUS_ENC_PPS_INT) {
		mtk_vcodec_err(inst, "expect irq status %d",
			       H264_IRQ_STATUS_ENC_PPS_INT);
		return -EINVAL;
	}

	*bs_size = h264_read_reg(inst, VENC_PIC_BITSTREAM_BYTE_CNT);
	mtk_vcodec_debug(inst, "bs size %d <-", *bs_size);

	return ret;
}

static int h264_encode_frame(struct venc_h264_inst *inst,
			     struct venc_frm_buf *frm_buf,
			     struct mtk_vcodec_mem *bs_buf,
			     unsigned int *bs_size)
{
	int ret = 0;
	unsigned int irq_status;

	mtk_vcodec_debug_enter(inst);

	ret = h264_enc_vpu_encode(inst, H264_BS_MODE_FRAME, frm_buf,
				  bs_buf, bs_size);
	if (ret)
		return ret;

	/*
	 * skip frame case: The skip frame buffer is composed by vpu side only,
	 * it does not trigger the hw, so skip the wait interrupt operation.
	 */
	if (!inst->vpu_inst.wait_int) {
		++inst->frm_cnt;
		return ret;
	}

	irq_status = h264_enc_wait_venc_done(inst);
	if (irq_status != H264_IRQ_STATUS_ENC_FRM_INT) {
		mtk_vcodec_err(inst, "irq_status=%d failed", irq_status);
		return -EINVAL;
	}

	*bs_size = h264_read_reg(inst,
				 VENC_PIC_BITSTREAM_BYTE_CNT);
	++inst->frm_cnt;
	mtk_vcodec_debug(inst, "frm %d bs size %d key_frm %d <-",
			 inst->frm_cnt,
			 *bs_size, inst->is_key_frm);

	return ret;
}

static void h264_encode_filler(struct venc_h264_inst *inst, void *buf,
			       int size)
{
	unsigned char *p = buf;

	*p++ = 0x0;
	*p++ = 0x0;
	*p++ = 0x0;
	*p++ = 0x1;
	*p++ = 0xc;
	size -= 5;
	while (size) {
		*p++ = 0xff;
		size -= 1;
	}
}

static int h264_enc_init(struct mtk_vcodec_ctx *ctx, unsigned long *handle)
{
	int ret = 0;
	struct venc_h264_inst *inst;

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return -ENOMEM;

	inst->ctx = ctx;
	inst->dev = mtk_vcodec_get_plat_dev(ctx);
	inst->hw_base = mtk_vcodec_get_reg_addr(inst->ctx, VENC_SYS);

	ret = h264_enc_vpu_init(inst);
	if (ret)
		kfree(inst);
	else
		(*handle) = (unsigned long)inst;

	return ret;
}

static int h264_enc_encode(unsigned long handle,
		    enum venc_start_opt opt,
		    struct venc_frm_buf *frm_buf,
		    struct mtk_vcodec_mem *bs_buf,
		    struct venc_done_result *result)
{
	int ret = 0;
	struct venc_h264_inst *inst = (struct venc_h264_inst *)handle;
	struct mtk_vcodec_ctx *ctx = inst->ctx;

	mtk_vcodec_debug(inst, "opt %d ->", opt);
	enable_irq(ctx->dev->enc_irq);

	switch (opt) {
	case VENC_START_OPT_ENCODE_SEQUENCE_HEADER: {
		unsigned int bs_size_sps;
		unsigned int bs_size_pps;

		memset(bs_buf->va, 0x38, 20);
		ret = h264_encode_sps(inst, bs_buf, &bs_size_sps);
		if (ret)
			goto encode_err;

		memset(inst->pps_buf.va, 0x49, 20);
		ret = h264_encode_pps(inst, &inst->pps_buf, &bs_size_pps);
		if (ret)
			goto encode_err;

		memcpy(bs_buf->va + bs_size_sps,
		       inst->pps_buf.va,
		       bs_size_pps);
		result->bs_size = bs_size_sps + bs_size_pps;
		result->is_key_frm = false;
	}
	break;

	case VENC_START_OPT_ENCODE_FRAME:
		if (inst->prepend_hdr) {
			int hdr_sz;
			int hdr_sz_ext;
			int bs_alignment = 128;
			int filler_sz = 0;
			struct mtk_vcodec_mem tmp_bs_buf;
			unsigned int bs_size_sps;
			unsigned int bs_size_pps;
			unsigned int bs_size_frm;

			mtk_vcodec_debug(inst,
					 "h264_encode_frame prepend SPS/PPS");
			h264_encode_sps(inst, bs_buf, &bs_size_sps);
			if (ret)
				goto encode_err;

			ret = h264_encode_pps(inst, &inst->pps_buf, &bs_size_pps);
			if (ret)
				goto encode_err;

			memcpy(bs_buf->va + bs_size_sps,
			       inst->pps_buf.va,
			       bs_size_pps);

			hdr_sz = bs_size_sps + bs_size_pps;
			hdr_sz_ext = (hdr_sz & (bs_alignment - 1));
			if (hdr_sz_ext) {
				filler_sz = bs_alignment - hdr_sz_ext;
				if (hdr_sz_ext + 5 > bs_alignment)
					filler_sz += bs_alignment;
				h264_encode_filler(
					inst, bs_buf->va + hdr_sz,
					filler_sz);
			}

			tmp_bs_buf.va = bs_buf->va + hdr_sz +
				filler_sz;
			tmp_bs_buf.dma_addr = bs_buf->dma_addr + hdr_sz +
				filler_sz;
			tmp_bs_buf.size = bs_buf->size -
				(hdr_sz + filler_sz);

			ret = h264_encode_frame(inst, frm_buf, &tmp_bs_buf,
						&bs_size_frm);
			if (ret)
				goto encode_err;

			result->bs_size = hdr_sz + filler_sz + bs_size_frm;
			mtk_vcodec_debug(inst,
					 "hdr %d filler %d frame %d bs %d",
					 hdr_sz, filler_sz, bs_size_frm,
					 result->bs_size);

			inst->prepend_hdr = 0;
		} else {
			ret = h264_encode_frame(inst, frm_buf, bs_buf,
						&result->bs_size);
			if (ret)
				goto encode_err;
		}
		result->is_key_frm = inst->is_key_frm;
		break;

	default:
		mtk_vcodec_err(inst, "venc_start_opt %d not supported", opt);
		ret = -EINVAL;
		break;
	}

encode_err:
	if (ret)
		result->msg = VENC_MESSAGE_ERR;
	else
		result->msg = VENC_MESSAGE_OK;

	disable_irq(ctx->dev->enc_irq);
	mtk_vcodec_debug(inst, "opt %d <-", opt);
	return ret;
}

static int h264_enc_set_param(unsigned long handle,
		       enum venc_set_param_type type, void *in)
{
	int ret = 0;
	struct venc_h264_inst *inst = (struct venc_h264_inst *)handle;
	struct venc_enc_prm *enc_prm;

	mtk_vcodec_debug(inst, "->type=%d", type);

	switch (type) {
	case VENC_SET_PARAM_ENC:
		enc_prm = in;
		ret = h264_enc_vpu_set_param(inst, type, enc_prm);
		if (ret)
			break;
		if (inst->work_buf_allocated == 0) {
			ret = h264_enc_alloc_work_buf(inst, enc_prm);
			if (ret)
				break;
			inst->work_buf_allocated = 1;
		}
		break;

	case VENC_SET_PARAM_FORCE_INTRA:
		ret = h264_enc_vpu_set_param(inst, type, 0);
		break;

	case VENC_SET_PARAM_ADJUST_BITRATE:
		enc_prm = in;
		h264_enc_vpu_set_param(inst, type, &enc_prm->bitrate);
		break;

	case VENC_SET_PARAM_ADJUST_FRAMERATE:
		enc_prm = in;
		ret = h264_enc_vpu_set_param(inst, type, &enc_prm->frm_rate);
		break;

	case VENC_SET_PARAM_I_FRAME_INTERVAL:
		ret = h264_enc_vpu_set_param(inst, type, in);
		break;

	case VENC_SET_PARAM_SKIP_FRAME:
		ret = h264_enc_vpu_set_param(inst, type, 0);
		break;

	case VENC_SET_PARAM_PREPEND_HEADER:
		inst->prepend_hdr = 1;
		mtk_vcodec_debug(inst, "set prepend header mode");
		break;

	default:
		mtk_vcodec_err(inst, "type %d not supported", type);
		ret = -EINVAL;
		break;
	}

	mtk_vcodec_debug_leave(inst);
	return ret;
}

static int h264_enc_deinit(unsigned long handle)
{
	int ret = 0;
	struct venc_h264_inst *inst = (struct venc_h264_inst *)handle;

	mtk_vcodec_debug_enter(inst);

	ret = h264_enc_vpu_deinit(inst);

	if (inst->work_buf_allocated)
		h264_enc_free_work_buf(inst);

	mtk_vcodec_debug_leave(inst);
	kfree(inst);

	return ret;
}

static struct venc_common_if venc_h264_if = {
	h264_enc_init,
	h264_enc_encode,
	h264_enc_set_param,
	h264_enc_deinit,
};

struct venc_common_if *get_h264_enc_comm_if(void)
{
	return &venc_h264_if;
}
