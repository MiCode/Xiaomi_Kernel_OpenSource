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
	case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN:
		return 2;
	case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10:
		return 4;
	case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_STILL_PICTURE:
		return 8;
	default:
		mtk_vcodec_debug(inst, "unsupported profile %d", profile);
		return 0;
	}
}

static unsigned int venc_h265_get_level(struct venc_inst *inst,
	unsigned int level, unsigned int tier)
{
	switch (level) {
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_1:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 0 : 1;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_2:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 2 : 3;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_2_1:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 4 : 5;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_3:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 6 : 7;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_3_1:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 8 : 9;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_4:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 10 : 11;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_4_1:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 12 : 13;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_5:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 14 : 15;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_5_1:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 16 : 17;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_5_2:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 18 : 19;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_6:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 20 : 21;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_6_1:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 22 : 23;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_6_2:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 24 : 25;
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

	mtk_vcodec_debug(inst, "vsi venc_bs_va 0x%llx",
			 inst->vsi->venc.venc_bs_va);

	ret = vcu_enc_encode(&inst->vcu_inst, VENC_BS_MODE_SEQ_HDR, NULL,
						 bs_buf, bs_size);

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
		inst->vsi->venc.roimap = frm_buf->roimap;

		mtk_vcodec_debug(inst, "ROI: 0x%X", inst->vsi->venc.roimap);
		mtk_vcodec_debug(inst, "QP: 0x%X", inst->vsi->venc.qpmap);
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

	mtk_v4l2_debug(0, "check inst->vsi %p +", inst->vsi);
	if (inst == NULL || inst->vsi == NULL)
		return -EINVAL;

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
	if (!inst) {
		*handle = (unsigned long)NULL;
		return -ENOMEM;
	}

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
	case V4L2_PIX_FMT_HEIF: {
		inst->vcu_inst.id = IPI_VENC_HEIF;
		break;
	}

	default: {
		mtk_vcodec_err(inst, "%s fourcc not supported", __func__);
		break;
	}
	}

	inst->hw_base = mtk_vcodec_get_enc_reg_addr(inst->ctx, VENC_SYS);
	inst->vcu_inst.handler = vcu_enc_ipi_handler;

	mtk_vcodec_debug_enter(inst);

	ret = vcu_enc_init(&inst->vcu_inst);

	inst->vsi = (struct venc_vsi *)inst->vcu_inst.vsi;

	mtk_vcodec_debug_leave(inst);

	if (ret) {
		kfree(inst);
		(*handle) = (unsigned long)NULL;
	} else
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

	if (inst == NULL || inst->vsi == NULL)
		return -EINVAL;

	mtk_vcodec_debug(inst, "opt %d ->", opt);

	switch (opt) {
	case VENC_START_OPT_ENCODE_SEQUENCE_HEADER: {
		unsigned int bs_size_hdr = 0;

		ret = venc_encode_header(inst, bs_buf, &bs_size_hdr);
		if (ret)
			goto encode_err;

		result->bs_size = bs_size_hdr;
		result->is_key_frm = false;
		break;
	}

	case VENC_START_OPT_ENCODE_FRAME: {
		/* only run @ worker then send ipi
		 * VPU flush cmd binding ctx & handle
		 * or cause cmd calllback ctx error
		 */
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

	mtk_vcodec_debug(inst, "bsva %lx frva %lx bssize %d iskey %d",
		pResult->bs_va,
		pResult->frm_va,
		pResult->bs_size,
		pResult->is_key_frm);

	list->read_idx = (list->read_idx == VENC_MAX_FB_NUM - 1U) ?
			 0U : list->read_idx + 1U;
	list->count--;
}

static void venc_get_resolution_change(struct venc_inst *inst,
			     struct venc_vcu_config *Config,
			     struct venc_resolution_change *pResChange)
{
	pResChange->width = Config->pic_w;
	pResChange->height = Config->pic_h;
	pResChange->framerate = Config->framerate;
	pResChange->resolutionchange = Config->resolutionChange;

	if (Config->resolutionChange)
		Config->resolutionChange = 0;

	mtk_vcodec_debug(inst, "get reschange %d %d %d %d\n",
		 pResChange->width,
		 pResChange->height,
		 pResChange->framerate,
		 pResChange->resolutionchange);
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
	case VENC_GET_PARAM_CAPABILITY_FRAME_SIZES:
	case VENC_GET_PARAM_CAPABILITY_SUPPORTED_FORMATS:
		vcu_enc_query_cap(&inst->vcu_inst, type, out);
		break;
	case VENC_GET_PARAM_FREE_BUFFERS:
		if (inst->vsi == NULL)
			return -EINVAL;
		venc_get_free_buffers(inst, &inst->vsi->list_free, out);
		break;
	case VENC_GET_PARAM_ROI_RC_QP: {
		if (inst->vsi == NULL || out == NULL)
			return -EINVAL;
		*(int *)out = inst->vsi->config.roi_rc_qp;
		break;
	}
	case VENC_GET_PARAM_RESOLUTION_CHANGE:
		if (inst->vsi == NULL)
			return -EINVAL;
		venc_get_resolution_change(inst, &inst->vsi->config, out);
		break;

	case VENC_GET_PARAM_REFBUF_FRAME_NUM: {
		if (inst->vsi == NULL || out == NULL)
			return -EINVAL;
		*(int *)out = inst->vsi->config.maxrefbufFrameNum;
		break;
	}

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

	if (inst == NULL || inst->vsi == NULL)
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
		inst->vsi->config.roion = enc_prm->roion;
		inst->vsi->config.scenario = enc_prm->scenario;
		inst->vsi->config.prependheader = enc_prm->prependheader;
		inst->vsi->config.heif_grid_size = enc_prm->heif_grid_size;
		inst->vsi->config.max_w = enc_prm->max_w;
		inst->vsi->config.max_h = enc_prm->max_h;
		inst->vsi->config.num_b_frame = enc_prm->num_b_frame;
		inst->vsi->config.slbc_ready = enc_prm->slbc_ready;
		inst->vsi->config.i_qp = enc_prm->i_qp;
		inst->vsi->config.p_qp = enc_prm->p_qp;
		inst->vsi->config.b_qp = enc_prm->b_qp;
		inst->vsi->config.svp_mode = enc_prm->svp_mode;
		inst->vsi->config.tsvc = enc_prm->tsvc;
		inst->vsi->config.max_qp = enc_prm->max_qp;
		inst->vsi->config.min_qp = enc_prm->min_qp;
		inst->vsi->config.i_p_qp_delta = enc_prm->i_p_qp_delta;
		inst->vsi->config.qp_control_mode = enc_prm->qp_control_mode;
		if (enc_prm->qp_control_mode) {
			inst->vsi->config.frame_level_qp =
				enc_prm->frame_level_qp;
		}
		inst->vsi->config.dummynal = enc_prm->dummynal;

		if (enc_prm->color_desc) {
			memcpy(&inst->vsi->config.color_desc,
				enc_prm->color_desc,
				sizeof(struct mtk_color_desc));
		}
		inst->vsi->config.maxrefpnum = enc_prm->maxrefpnum;

		if (inst->vcu_inst.id == IPI_VENC_H264 ||
			inst->vcu_inst.id == IPI_VENC_HYBRID_H264) {
			inst->vsi->config.profile = enc_prm->profile;
			inst->vsi->config.level = enc_prm->level;
		} else if (inst->vcu_inst.id == IPI_VENC_H265 ||
				inst->vcu_inst.id == IPI_VENC_HEIF) {
			inst->vsi->config.profile =
				venc_h265_get_profile(inst, enc_prm->profile);
			inst->vsi->config.level =
				venc_h265_get_level(inst, enc_prm->level,
					enc_prm->tier);
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
		inst->ctx->async_mode = !(inst->vsi->sync_mode);

		break;
	case VENC_SET_PARAM_PREPEND_HEADER:
		inst->prepend_hdr = 1;
		ret = vcu_enc_set_param(&inst->vcu_inst, type, enc_prm);
		inst->ctx->async_mode = !(inst->vsi->sync_mode);
		break;
	case VENC_SET_PARAM_COLOR_DESC:
		memcpy(&inst->vsi->config.color_desc, enc_prm->color_desc,
			sizeof(struct mtk_color_desc));
		ret = vcu_enc_set_param(&inst->vcu_inst, type, enc_prm);
		break;
	case VENC_SET_PARAM_LOG:
		vcu_set_log(enc_prm->log);
	default:
		ret = vcu_enc_set_param(&inst->vcu_inst, type, enc_prm);
		inst->ctx->async_mode = !(inst->vsi->sync_mode);
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
