/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Longfei Wang <longfei.wang@mediatek.com>
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

#include "../mtk_vcodec_drv.h"
#include "../mtk_vcodec_util.h"
#include "../mtk_vcodec_enc.h"
#include "../venc_drv_base.h"
#include "../venc_ipi_msg.h"
#include "../venc_vcu_if.h"
#include "mtk_vcodec_enc_pm.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcu.h"



static unsigned int venc_h265_get_profile(struct venc_inst *inst,
	unsigned int profile)
{
	switch (profile) {
	case V4L2_MPEG_VIDEO_H265_PROFILE_MAIN:
		return 2;
	case V4L2_MPEG_VIDEO_H265_PROFILE_MAIN10:
		return 4;
	case V4L2_MPEG_VIDEO_H265_PROFILE_MAIN_STILL_PIC:
		return 8;
	default:
		mtk_vcodec_debug(inst, "unsupported profile %d", profile);
		return 0;
	}
}

static unsigned int venc_h265_get_level(struct venc_inst *inst,
	unsigned int level)
{
	switch (level) {
	case V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_1:
		return 0;
	case V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_1:
		return 1;
	case V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_2:
		return 2;
	case V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_2:
		return 3;
	case V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_2_1:
		return 4;
	case V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_2_1:
		return 5;
	case V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_3:
		return 6;
	case V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_3:
		return 7;
	case V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_3_1:
		return 8;
	case V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_3_1:
		return 9;
	case V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_4:
		return 10;
	case V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_4:
		return 11;
	case V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_4_1:
		return 12;
	case V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_4_1:
		return 13;
	case V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_5:
		return 14;
	case V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_5:
		return 15;
	case V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_5_1:
		return 16;
	case V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_5_1:
		return 17;
	case V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_5_2:
		return 18;
	case V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_5_2:
		return 19;
	case V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_6:
		return 20;
	case V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_6:
		return 21;
	case V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_6_1:
		return 22;
	case V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_6_1:
		return 23;
	case V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_6_2:
		return 24;
	case V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_6_2:
		return 25;
	default:
		mtk_vcodec_debug(inst, "unsupported level %d", level);
		return 26;
	}
}

static unsigned int venc_mpeg4_get_profile(struct venc_inst *inst,
	unsigned int profile)
{
	switch (profile) {
	case V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE:
		return 0;
	case V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_SIMPLE:
		return 1;
	case V4L2_MPEG_VIDEO_MPEG4_PROFILE_CORE:
		return 2;
	case V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE_SCALABLE:
		return 3;
	case V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_CODING_EFFICIENCY:
		return 4;
	default:
		mtk_vcodec_debug(inst, "unsupported mpeg4 profile %d", profile);
		return 100;
	}
}

static unsigned int venc_mpeg4_get_level(struct venc_inst *inst,
	unsigned int level)
{
	switch (level) {
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_0:
		return 0;
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_0B:
		return 1;
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_1:
		return 2;
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_2:
		return 3;
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_3:
		return 4;
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_3B:
		return 5;
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_4:
		return 6;
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_5:
		return 7;
	default:
		mtk_vcodec_debug(inst, "unsupported mpeg4 level %d", level);
		return 4;
	}
}

static int venc_encode_header(struct venc_inst *inst,
	struct mtk_vcodec_mem *bs_buf,
	unsigned int *bs_size)
{
	int ret = 0;

	mtk_vcodec_debug_enter(inst);
	if (bs_buf == NULL)
		inst->vsi->venc.venc_bs_va = 0;
	else
		inst->vsi->venc.venc_bs_va = (u64)(uintptr_t)bs_buf;

	inst->vsi->venc.venc_fb_va = 0;
	ret = vcu_enc_encode(&inst->vcu_inst, VENC_BS_MODE_SEQ_HDR, NULL,
						 bs_buf, bs_size);
	if (ret)
		return ret;

	mtk_vcodec_debug(inst, "vsi venc_bs_va 0x%llx",
			 inst->vsi->venc.venc_bs_va);

	return ret;
}

static int venc_encode_frame(struct venc_inst *inst,
	struct venc_frm_buf *frm_buf,
	struct mtk_vcodec_mem *bs_buf,
	unsigned int *bs_size)
{
	int ret = 0;
	unsigned int fm_fourcc = inst->ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
	unsigned int bs_fourcc = inst->ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc;

	mtk_vcodec_debug_enter(inst);

	if (bs_buf == NULL)
		inst->vsi->venc.venc_bs_va = 0;
	else
		inst->vsi->venc.venc_bs_va = (u64)(uintptr_t)bs_buf;

	if (frm_buf == NULL)
		inst->vsi->venc.venc_fb_va = 0;
	else {
		inst->vsi->venc.venc_fb_va = (u64)(uintptr_t)frm_buf;
		inst->vsi->venc.timestamp = frm_buf->timestamp;
	}
	ret = vcu_enc_encode(&inst->vcu_inst, VENC_BS_MODE_FRAME, frm_buf,
						 bs_buf, bs_size);
	if (ret)
		return ret;

	++inst->frm_cnt;
	mtk_vcodec_debug(inst,
		 "Format: frame_va %llx (%c%c%c%c) bs_va:%llx (%c%c%c%c)",
		  inst->vsi->venc.venc_fb_va,
		  fm_fourcc & 0xFF, (fm_fourcc >> 8) & 0xFF,
		  (fm_fourcc >> 16) & 0xFF, (fm_fourcc >> 24) & 0xFF,
		  inst->vsi->venc.venc_bs_va,
		  bs_fourcc & 0xFF, (bs_fourcc >> 8) & 0xFF,
		  (bs_fourcc >> 16) & 0xFF, (bs_fourcc >> 24) & 0xFF);

	return ret;
}

static int venc_encode_frame_final(struct venc_inst *inst,
	struct venc_frm_buf *frm_buf,
	struct mtk_vcodec_mem *bs_buf,
	unsigned int *bs_size)
{
	int ret = 0;

	mtk_vcodec_debug_enter(inst);
	if (bs_buf == NULL)
		inst->vsi->venc.venc_bs_va = 0;
	else
		inst->vsi->venc.venc_bs_va = (u64)(uintptr_t)bs_buf;
	if (frm_buf == NULL)
		inst->vsi->venc.venc_fb_va = 0;
	else
		inst->vsi->venc.venc_fb_va = (u64)(uintptr_t)frm_buf;

	ret = vcu_enc_encode(&inst->vcu_inst, VENC_BS_MODE_FRAME_FINAL, frm_buf,
						 bs_buf, bs_size);
	if (ret)
		return ret;

	*bs_size = inst->vcu_inst.bs_size;
	mtk_vcodec_debug(inst, "bs size %d <-", *bs_size);

	return ret;
}


static int venc_init(struct mtk_vcodec_ctx *ctx, unsigned long *handle)
{
	int ret = 0;
	struct venc_inst *inst;
	u32 fourcc = ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc;

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return -ENOMEM;

	inst->ctx = ctx;
	inst->vcu_inst.ctx = ctx;
	inst->vcu_inst.dev = ctx->dev->vcu_plat_dev;

	switch (fourcc) {
	case V4L2_PIX_FMT_H264: {
		if (ctx->oal_vcodec == 1)
			inst->vcu_inst.id = IPI_VENC_HYBRID_H264;
		else
			inst->vcu_inst.id = IPI_VENC_H264;
		break;
	}

	case V4L2_PIX_FMT_VP8: {
		inst->vcu_inst.id = IPI_VENC_VP8;
		break;
	}

	case V4L2_PIX_FMT_MPEG4: {
		inst->vcu_inst.id = IPI_VENC_MPEG4;
		break;
	}

	case V4L2_PIX_FMT_H263: {
		inst->vcu_inst.id = IPI_VENC_H263;
		break;
	}
	case V4L2_PIX_FMT_H265: {
		inst->vcu_inst.id = IPI_VENC_H265;
		break;
	}

	default: {
		mtk_vcodec_err(inst, "%s fourcc not supported", __func__);
		break;
	}
	}

	inst->hw_base = mtk_vcodec_get_enc_reg_addr(inst->ctx, VENC_SYS);

	mtk_vcodec_debug_enter(inst);

	ret = vcu_enc_init(&inst->vcu_inst);

	inst->vsi = (struct venc_vsi *)inst->vcu_inst.vsi;

	mtk_vcodec_debug_leave(inst);

	if (ret)
		kfree(inst);
	else
		(*handle) = (unsigned long)inst;

	return ret;
}

static int venc_encode(unsigned long handle,
					   enum venc_start_opt opt,
					   struct venc_frm_buf *frm_buf,
					   struct mtk_vcodec_mem *bs_buf,
					   struct venc_done_result *result)
{
	int ret = 0;
	struct venc_inst *inst = (struct venc_inst *)handle;
	struct mtk_vcodec_ctx *ctx = inst->ctx;

	mtk_vcodec_debug(inst, "opt %d ->", opt);

	if (ctx->oal_vcodec == 0)
		enable_irq(ctx->dev->enc_irq);

	switch (opt) {
	case VENC_START_OPT_ENCODE_SEQUENCE_HEADER: {
		unsigned int bs_size_hdr;

		ret = venc_encode_header(inst, bs_buf, &bs_size_hdr);
		if (ret)
			goto encode_err;

		result->bs_size = bs_size_hdr;
		result->is_key_frm = false;
		break;
	}

	case VENC_START_OPT_ENCODE_FRAME: {

		ret = venc_encode_frame(inst, frm_buf, bs_buf,
			&result->bs_size);
		if (ret)
			goto encode_err;
		result->is_key_frm = inst->vcu_inst.is_key_frm;
		break;
	}

	case VENC_START_OPT_ENCODE_FRAME_FINAL: {
		ret = venc_encode_frame_final(inst,
			frm_buf, bs_buf, &result->bs_size);
		if (ret)
			goto encode_err;
		result->is_key_frm = inst->vcu_inst.is_key_frm;
		break;
	}

	default:
		mtk_vcodec_err(inst, "venc_start_opt %d not supported", opt);
		ret = -EINVAL;
		break;
	}

encode_err:

	if (ctx->oal_vcodec == 0)
		disable_irq(ctx->dev->enc_irq);
	mtk_vcodec_debug(inst, "opt %d <-", opt);

	return ret;
}

static void venc_get_free_buffers(struct venc_inst *inst,
			     struct ring_input_list *list,
			     struct venc_done_result *pResult)
{
	if (list->count == 0) {
		mtk_vcodec_debug(inst, "[FB] there is no free buffers");
		pResult->bs_va = 0;
		pResult->frm_va = 0;
		pResult->is_key_frm = false;
		pResult->bs_size = 0;
		return;
	}

	pResult->bs_size = list->bs_size[list->read_idx];
	pResult->is_key_frm = list->is_key_frm[list->read_idx];
	pResult->bs_va = list->venc_bs_va_list[list->read_idx];
	pResult->frm_va = list->venc_fb_va_list[list->read_idx];

	mtk_vcodec_debug(inst, "bsva %llx frva %llx bssize %d iskey %d",
		pResult->bs_va,
		pResult->frm_va,
		pResult->bs_size,
		pResult->is_key_frm);

	list->read_idx = (list->read_idx == VENC_MAX_FB_NUM - 1U) ?
			 0U : list->read_idx + 1U;
	list->count--;
}

static int venc_get_param(unsigned long handle,
						  enum venc_get_param_type type,
						  void *out)
{
	int ret = 0;
	struct venc_inst *inst = (struct venc_inst *)handle;

	if (inst == NULL)
		return -EINVAL;

	mtk_vcodec_debug(inst, "%s: %d", __func__, type);
	inst->vcu_inst.ctx = inst->ctx;

	switch (type) {
	case GET_PARAM_CAPABILITY_FRAME_SIZES:
	case GET_PARAM_CAPABILITY_SUPPORTED_FORMATS:
		vcu_enc_query_cap(&inst->vcu_inst, type, out);
		break;
	case GET_PARAM_FREE_BUFFERS:
		if (inst->vsi == NULL)
			return -EINVAL;
		venc_get_free_buffers(inst, &inst->vsi->list_free, out);
		break;
	default:
		mtk_vcodec_err(inst, "invalid get parameter type=%d", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int venc_set_param(unsigned long handle,
	enum venc_set_param_type type,
	struct venc_enc_param *enc_prm)
{
	int i;
	int ret = 0;
	struct venc_inst *inst = (struct venc_inst *)handle;

	if (inst == NULL)
		return -EINVAL;

	mtk_vcodec_debug(inst, "->type=%d, ipi_id=%d", type, inst->vcu_inst.id);

	switch (type) {
	case VENC_SET_PARAM_ENC:
		inst->vsi->config.input_fourcc = enc_prm->input_yuv_fmt;
		inst->vsi->config.bitrate = enc_prm->bitrate;
		inst->vsi->config.pic_w = enc_prm->width;
		inst->vsi->config.pic_h = enc_prm->height;
		inst->vsi->config.buf_w = enc_prm->buf_width;
		inst->vsi->config.buf_h = enc_prm->buf_height;
		inst->vsi->config.gop_size = enc_prm->gop_size;
		inst->vsi->config.framerate = enc_prm->frm_rate;
		inst->vsi->config.intra_period = enc_prm->intra_period;
		inst->vsi->config.operationrate = enc_prm->operationrate;
		inst->vsi->config.bitratemode = enc_prm->bitratemode;
		inst->vsi->config.scenario = enc_prm->scenario;
		inst->vsi->config.prependheader = enc_prm->prependheader;

		if (inst->vcu_inst.id == IPI_VENC_H264 ||
			inst->vcu_inst.id == IPI_VENC_HYBRID_H264) {
			inst->vsi->config.profile = enc_prm->profile;
			inst->vsi->config.level = enc_prm->level;
		} else if (inst->vcu_inst.id == IPI_VENC_H265) {
			inst->vsi->config.profile =
				venc_h265_get_profile(inst, enc_prm->profile);
			inst->vsi->config.level =
				venc_h265_get_level(inst, enc_prm->level);
		} else if (inst->vcu_inst.id == IPI_VENC_MPEG4) {
			inst->vsi->config.profile =
				venc_mpeg4_get_profile(inst, enc_prm->profile);
			inst->vsi->config.level =
				venc_mpeg4_get_level(inst, enc_prm->level);
		}
		inst->vsi->config.wfd = 0;
		ret = vcu_enc_set_param(&inst->vcu_inst, type, enc_prm);
		if (ret)
			break;

		for (i = 0; i < MTK_VCODEC_MAX_PLANES; i++) {
			enc_prm->sizeimage[i] =
				inst->vsi->sizeimage[i];
			mtk_vcodec_debug(inst, "sizeimage[%d] size=0x%x", i,
							 enc_prm->sizeimage[i]);
		}
		if (inst->ctx->slowmotion)
			vcu_enc_set_ctx_for_gce(&inst->vcu_inst);

		break;
	case VENC_SET_PARAM_PREPEND_HEADER:
		inst->prepend_hdr = 1;
		ret = vcu_enc_set_param(&inst->vcu_inst, type, enc_prm);
		break;
	default:
		ret = vcu_enc_set_param(&inst->vcu_inst, type, enc_prm);
		break;
	}

	mtk_vcodec_debug_leave(inst);

	return ret;
}

static int venc_deinit(unsigned long handle)
{
	int ret = 0;
	struct venc_inst *inst = (struct venc_inst *)handle;

	mtk_vcodec_debug_enter(inst);

	ret = vcu_enc_deinit(&inst->vcu_inst);

	mtk_vcodec_debug_leave(inst);
	kfree(inst);

	return ret;
}

static const struct venc_common_if venc_if = {
	.init = venc_init,
	.encode = venc_encode,
	.get_param = venc_get_param,
	.set_param = venc_set_param,
	.deinit = venc_deinit,
};

const struct venc_common_if *get_enc_common_if(void);

const struct venc_common_if *get_enc_common_if(void)
{
	return &venc_if;
}
