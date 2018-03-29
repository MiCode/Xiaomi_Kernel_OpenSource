/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Daniel Hsiao <daniel.hsiao@mediatek.com>
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

#include "mtk_vpu.h"

#include "venc_vp8_if.h"
#include "venc_vp8_vpu.h"
#include "venc_ipi_msg.h"

#define VENC_VP8_WAIT_VPU_TIMEOUT_MS		(2000)

static void handle_vp8_enc_init_msg(struct venc_vp8_inst *inst, void *data)
{
	struct venc_vpu_ipi_msg_init *msg = data;

	inst->vpu_inst.id = msg->inst_id;
	inst->vpu_inst.drv = (struct venc_vp8_vpu_drv *)
		vpu_mapping_dm_addr(inst->dev,
				    msg->inst_id);
}

static void vp8_enc_vpu_ipi_handler(void *data, unsigned int len, void *priv)
{
	struct venc_vpu_ipi_msg_common *msg = data;
	struct venc_vp8_inst *inst = (struct venc_vp8_inst *)(uintptr_t)msg->venc_inst;

	mtk_vcodec_debug(inst, "->msg_id=%x status=%d",
			 msg->msg_id, msg->status);

	switch (msg->msg_id) {
	case VPU_IPIMSG_VP8_ENC_INIT_DONE:
		handle_vp8_enc_init_msg(inst, data);
		break;
	case VPU_IPIMSG_VP8_ENC_SET_PARAM_DONE:
	case VPU_IPIMSG_VP8_ENC_ENCODE_DONE:
	case VPU_IPIMSG_VP8_ENC_DEINIT_DONE:
		break;
	default:
		mtk_vcodec_err(inst, "unknown msg id=%x", msg->msg_id);
		break;
	}

	inst->vpu_inst.signaled = 1;
	inst->vpu_inst.failure = (msg->status != VENC_IPI_MSG_STATUS_OK);

	mtk_vcodec_debug_leave(inst);
}

static int vp8_enc_vpu_send_msg(struct venc_vp8_inst *inst, void *msg,
				int len, int wait_ack)
{
	int status;

	mtk_vcodec_debug_enter(inst);
	status = vpu_ipi_send(inst->dev, IPI_VENC_VP8, (void *)msg, len);
	if (status) {
		mtk_vcodec_err(inst,
			       "vpu_ipi_send msg_id=%x len=%d failed status=%d",
			       *(unsigned int *)msg, len, status);
		return -EINVAL;
	}
	mtk_vcodec_debug_leave(inst);
	return 0;
}

int vp8_enc_vpu_init(struct venc_vp8_inst *inst)
{
	int status;
	struct venc_ap_ipi_msg_init out;

	mtk_vcodec_debug_enter(inst);
	init_waitqueue_head(&inst->vpu_inst.wq_hd);
	inst->vpu_inst.signaled = 0;
	inst->vpu_inst.failure = 0;

	status = vpu_ipi_register(inst->dev, IPI_VENC_VP8,
				  vp8_enc_vpu_ipi_handler,
				  "vp8_enc", NULL);
	if (status) {
		mtk_vcodec_err(inst,
			       "vpu_ipi_register failed status=%d", status);
		return -EINVAL;
	}

	out.msg_id = AP_IPIMSG_VP8_ENC_INIT;
	out.venc_inst = (unsigned long)inst;
	if (vp8_enc_vpu_send_msg(inst, &out, sizeof(out), 1) ||
	    inst->vpu_inst.failure) {
		mtk_vcodec_err(inst, "failed");
		return -EINVAL;
	}

	mtk_vcodec_debug_leave(inst);
	return 0;
}

int vp8_enc_vpu_set_param(struct venc_vp8_inst *inst, unsigned int id,
			  void *param)
{
	struct venc_ap_ipi_msg_set_param out;

	mtk_vcodec_debug_enter(inst);
	out.msg_id = AP_IPIMSG_VP8_ENC_SET_PARAM;
	out.inst_id = inst->vpu_inst.id;
	out.param_id = id;
	switch (id) {
	case VENC_SET_PARAM_ENC: {
		struct venc_enc_prm *enc_param = (struct venc_enc_prm *)param;

		inst->vpu_inst.drv->config.input_fourcc =
			enc_param->input_fourcc;
		inst->vpu_inst.drv->config.bitrate = enc_param->bitrate;
		inst->vpu_inst.drv->config.pic_w = enc_param->width;
		inst->vpu_inst.drv->config.pic_h = enc_param->height;
		inst->vpu_inst.drv->config.buf_w = enc_param->buf_width;
		inst->vpu_inst.drv->config.buf_h = enc_param->buf_height;
		inst->vpu_inst.drv->config.intra_period =
			enc_param->intra_period;
		inst->vpu_inst.drv->config.framerate = enc_param->frm_rate;
		inst->vpu_inst.drv->config.ts_mode = inst->ts_mode;
		out.data_item = 0;
		break;
	}
	case VENC_SET_PARAM_FORCE_INTRA:
		out.data_item = 0;
		break;
	case VENC_SET_PARAM_ADJUST_BITRATE:
		out.data_item = 1;
		out.data[0] = *(unsigned int *)param;
		break;
	case VENC_SET_PARAM_ADJUST_FRAMERATE:
		out.data_item = 1;
		out.data[0] = *(unsigned int *)param;
		break;
	case VENC_SET_PARAM_I_FRAME_INTERVAL:
		out.data_item = 1;
		out.data[0] = *(unsigned int *)param;
		break;
	}
	if (vp8_enc_vpu_send_msg(inst, &out, sizeof(out), 1) ||
	    inst->vpu_inst.failure) {
		mtk_vcodec_err(inst, "failed");
		return -EINVAL;
	}

	mtk_vcodec_debug_leave(inst);
	return 0;
}

int vp8_enc_vpu_encode(struct venc_vp8_inst *inst,
		       struct venc_frm_buf *frm_buf,
		       struct mtk_vcodec_mem *bs_buf)
{
	struct venc_ap_ipi_msg_enc out;

	mtk_vcodec_debug_enter(inst);
	out.msg_id = AP_IPIMSG_VP8_ENC_ENCODE;
	out.inst_id = inst->vpu_inst.id;
	if (frm_buf) {
		if ((frm_buf->fb_addr.dma_addr % 16 == 0) &&
		    (frm_buf->fb_addr1.dma_addr % 16 == 0) &&
		    (frm_buf->fb_addr2.dma_addr % 16 == 0)) {
			out.input_addr[0] = frm_buf->fb_addr.dma_addr;
			out.input_addr[1] = frm_buf->fb_addr1.dma_addr;
			out.input_addr[2] = frm_buf->fb_addr2.dma_addr;
		} else {
			mtk_vcodec_err(inst, "dma_addr not align to 16");
			return -EINVAL;
		}
	} else {
		out.input_addr[0] = 0;
		out.input_addr[1] = 0;
		out.input_addr[2] = 0;
	}
	if (bs_buf) {
		out.bs_addr = bs_buf->dma_addr;
		out.bs_size = bs_buf->size;
	} else {
		out.bs_addr = 0;
		out.bs_size = 0;
	}

	if (vp8_enc_vpu_send_msg(inst, &out, sizeof(out), 1) ||
	    inst->vpu_inst.failure) {
		mtk_vcodec_err(inst, "failed");
		return -EINVAL;
	}

	mtk_vcodec_debug_leave(inst);
	return 0;
}

int vp8_enc_vpu_deinit(struct venc_vp8_inst *inst)
{
	struct venc_ap_ipi_msg_deinit out;

	mtk_vcodec_debug_enter(inst);
	out.msg_id = AP_IPIMSG_VP8_ENC_DEINIT;
	out.inst_id = inst->vpu_inst.id;
	if (vp8_enc_vpu_send_msg(inst, &out, sizeof(out), 1) ||
	    inst->vpu_inst.failure) {
		mtk_vcodec_err(inst, "failed");
		return -EINVAL;
	}

	mtk_vcodec_debug_leave(inst);
	return 0;
}
