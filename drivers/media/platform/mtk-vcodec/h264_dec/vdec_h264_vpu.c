/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
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

#include "mtk_vpu.h"

#include "vdec_h264_if.h"
#include "vdec_h264_vpu.h"

#include "vdec_ipi_msg.h"

#define VDEC_H264_WAIT_VPU_TIMEOUT_MS		(2000)

/**
 * enum vdec_h264_shmem - share memory between Host and VPU.
 * @VDEC_SHMEM_HDR_BS :	Bit-stream buffer for header NAL
 * @VDEC_SHMEM_VSI    :	Shared decoding information (struct vdec_h264_vsi)
 */
enum vdec_h264_shmem {
	VDEC_SHMEM_HDR_BS		= 0,
	VDEC_SHMEM_VSI			= 1,
	VDEC_SHMEM_MAX			= 2
};

/**
 * struct vdec_h264_ipi_init - for AP_IPIMSG_DEC_INIT
 * @msg_id   : AP_IPIMSG_DEC_INIT
 * @ctx_id   : V4L2 ctx id
 * @vdec_inst: AP vdec_h264_inst address
 * @bs_dma   : Input bit-stream buffer dma address
 * @bs_sz    : Bit-stream buffer size
 */
struct vdec_h264_ipi_init {
	uint32_t msg_id;
	uint32_t ctx_id;
	uint64_t vdec_inst;
	uint64_t bs_dma;
	uint32_t bs_sz;
};

/**
 * struct vdec_h264_ipi_init_ack - for VPU_IPIMSG_DEC_INIT_ACK
 * @msg_id      : VPU_IPIMSG_DEC_INIT_ACK
 * @status      : VPU exeuction result
 * @vdec_inst   : AP vdec_h264_inst address
 * @h_drv	: Handle to VPU driver
 * @shmem_addr  : VPU shared memory address (VPU is 32-bit)
 */
struct vdec_h264_ipi_init_ack {
	uint32_t msg_id;
	int32_t status;
	uint64_t vdec_inst;
	uint32_t h_drv;
	uint32_t shmem_addr[VDEC_SHMEM_MAX];
};

/**
 * struct vdec_h264_ipi_dec_start - for AP_IPIMSG_DEC_START
 * @msg_id       : AP_IPIMSG_DEC_START
 * @h_drv	 : Handle to VPU driver
 * @bs_sz        : Bit-stream buffer size
 * @nal_start    : NALU first byte
 * @bs_dma       : Input bit-stream buffer dma address
 * @y_fb_dma     : Y frame buffer dma address
 * @c_fb_dma     : C frame buffer dma address
 * @vdec_fb_va   : VDEC frame buffer struct virtual address
 */
struct vdec_h264_ipi_dec_start {
	uint32_t msg_id;
	uint32_t h_drv;
	uint32_t bs_sz;
	uint32_t nal_start;
	uint64_t bs_dma;
	uint64_t y_fb_dma;
	uint64_t c_fb_dma;
	uint64_t vdec_fb_va;
};

static void handle_init_ack_msg(struct vdec_h264_ipi_init_ack *msg)
{
	struct vdec_h264_inst *inst = (struct vdec_h264_inst *)(uintptr_t)msg->vdec_inst;
	u32 vpu_addr;
	void *shmem_va[VDEC_SHMEM_MAX];
	int i;

	mtk_vcodec_debug(inst, "+ vdec_inst = 0x%llx", msg->vdec_inst);

	/* mapping VPU address to kernel virtual address */
	for (i = 0; i < VDEC_SHMEM_MAX; i++) {
		vpu_addr = (unsigned long)msg->shmem_addr[i];
		shmem_va[i] = vpu_mapping_dm_addr(inst->dev, vpu_addr);
		mtk_vcodec_debug(inst, "shmem[%d] va=%p\n", i, shmem_va[i]);
	}

	inst->vsi = (struct vdec_h264_vsi *)shmem_va[VDEC_SHMEM_VSI];
	inst->vpu.hdr_bs_buf = (unsigned char *)shmem_va[VDEC_SHMEM_HDR_BS];
	inst->vpu.h_drv = msg->h_drv;

	mtk_vcodec_debug(inst, "- h_drv = 0x%x", inst->vpu.h_drv);
}

/*
 * This function runs in interrupt context and it means there's a IPI MSG
 * from VPU.
 */
static void h264_dec_vpu_ipi_handler(void *data, unsigned int len, void *priv)
{
	struct vdec_vpu_ipi_ack *msg;
	struct vdec_h264_inst *inst;

	msg = (struct vdec_vpu_ipi_ack *)data;
	inst = (struct vdec_h264_inst *)(uintptr_t)msg->vdec_inst;

	mtk_vcodec_debug(inst, "+ id=%X", msg->msg_id);

	inst->vpu.failure = msg->status;

	if (msg->status == 0) {
		switch (msg->msg_id) {
		case VPU_IPIMSG_DEC_INIT_ACK:
			handle_init_ack_msg(data);
			break;

		case VPU_IPIMSG_DEC_START_ACK:
		case VPU_IPIMSG_DEC_END_ACK:
		case VPU_IPIMSG_DEC_DEINIT_ACK:
		case VPU_IPIMSG_DEC_RESET_ACK:
			break;

		default:
			mtk_vcodec_err(inst, "invalid msg=%X", msg->msg_id);
			break;
		}
	}

	mtk_vcodec_debug(inst, "- id=%X", msg->msg_id);
	inst->vpu.signaled = 1;
}

static int h264_dec_vpu_send_msg(struct vdec_h264_inst *inst,
				 void *msg, int len)
{
	int err;
	struct vdec_h264_vpu_inst *vpu = &inst->vpu;

	mtk_vcodec_debug(inst, "id=%X", *(unsigned int *)msg);

	vpu->failure = 0;
	vpu->signaled = 0;

	err = vpu_ipi_send(inst->dev, IPI_VDEC_H264, msg, len);
	if (err) {
		mtk_vcodec_err(inst, "vpu_ipi_send fail status=%d", err);
		return err;
	}

	return err;
}

int vdec_h264_vpu_init(struct vdec_h264_inst *inst, uint64_t bs_dma,
		       uint32_t bs_sz)
{
	struct vdec_h264_ipi_init msg;
	int err;

	mtk_vcodec_debug_enter(inst);

	init_waitqueue_head(&inst->vpu.wq);
	inst->vpu.signaled = 0;
	inst->vpu.failure = 0;

	err = vpu_ipi_register(inst->dev, IPI_VDEC_H264,
			       h264_dec_vpu_ipi_handler, "vdec_h264",
			       NULL);
	if (err != 0) {
		mtk_vcodec_err(inst, "vpu_ipi_register fail status=%d", err);
		return err;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_id	= AP_IPIMSG_DEC_INIT;
	msg.ctx_id	= inst->ctx_id;
	msg.vdec_inst   = (unsigned long)inst;
	msg.bs_dma	= bs_dma;
	msg.bs_sz       = bs_sz;

	mtk_vcodec_debug(inst, "vdec_inst=%p bs_sz=0x%x", inst, bs_sz);

	err = h264_dec_vpu_send_msg(inst, (void *)&msg, sizeof(msg));

	if (!err && inst->vpu.failure != 0)
		err = inst->vpu.failure;

	mtk_vcodec_debug(inst, "- ret=%d", err);
	return err;
}

int vdec_h264_vpu_dec_start(struct vdec_h264_inst *inst, uint32_t bs_sz,
			    uint32_t nal_start, uint64_t bs_dma,
			    uint64_t y_fb_dma, uint64_t c_fb_dma,
			    uint64_t vdec_fb_va)
{
	struct vdec_h264_ipi_dec_start msg;
	int err = 0;

	mtk_vcodec_debug(inst, "+ type=%d sz=%d", NAL_TYPE(nal_start), bs_sz);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id	= AP_IPIMSG_DEC_START;
	msg.h_drv	= inst->vpu.h_drv;
	msg.bs_sz	= bs_sz;
	msg.nal_start   = nal_start;
	msg.bs_dma	= bs_dma;
	msg.y_fb_dma	= y_fb_dma;
	msg.c_fb_dma	= c_fb_dma;
	msg.vdec_fb_va  = vdec_fb_va;

	err = h264_dec_vpu_send_msg(inst, (void *)&msg, sizeof(msg));
	if (!err && inst->vpu.failure != 0)
		err = inst->vpu.failure;

	mtk_vcodec_debug(inst, "- ret=%d", err);
	return err;
}

static int h264_dec_send_ap_ipi(struct vdec_h264_inst *inst,
				unsigned int msg_id)
{
	struct vdec_ap_ipi_cmd msg;
	int err = 0;

	mtk_vcodec_debug(inst, "+ id=%X", msg_id);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = msg_id;
	msg.h_drv = inst->vpu.h_drv;

	err = h264_dec_vpu_send_msg(inst, &msg, sizeof(msg));
	if (!err && inst->vpu.failure != 0)
		err = inst->vpu.failure;

	mtk_vcodec_debug(inst, "- id=%X ret=%d", msg_id, err);
	return err;
}

int vdec_h264_vpu_dec_end(struct vdec_h264_inst *inst)
{
	return h264_dec_send_ap_ipi(inst, AP_IPIMSG_DEC_END);
}

int vdec_h264_vpu_deinit(struct vdec_h264_inst *inst)
{
	return h264_dec_send_ap_ipi(inst, AP_IPIMSG_DEC_DEINIT);
}

int vdec_h264_vpu_reset(struct vdec_h264_inst *inst)
{
	return h264_dec_send_ap_ipi(inst, AP_IPIMSG_DEC_RESET);
}
