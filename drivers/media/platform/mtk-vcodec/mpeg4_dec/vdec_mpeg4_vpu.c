/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Rick Chang <rick.chang@mediatek.com>
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

#include <linux/slab.h>

#include "mtk_vpu.h"
#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_util.h"
#include "vdec_mpeg4_vpu.h"
#include "vdec_ipi_msg.h"

#define VDEC_MPEG4_WAIT_VPU_TIMEOUT_MS		(2000)

/**
 * struct vdec_mpeg4_ipi_init - for AP_IPIMSG_DEC_INIT
 * @msg_id      : AP_IPIMSG_DEC_INIT
 * @vdec_inst   : AP vdec_mpeg4_inst address
 * @bs             : Input bit-stream buffer
 */
struct vdec_mpeg4_ipi_init {
	uint32_t msg_id;
	uint64_t vdec_inst;
	struct vdec_mpeg4_bs bs;
};

/**
 * struct vdec_mpeg4_ipi_dec_start - for AP_IPIMSG_DEC_START
 * @msg_id      : AP_IPIMSG_DEC_START
 * @h_drv	  : Handle to VPU driver
 * @bs             : Input bit-stream buffer
 * @fb              : Frame buffer
 */
struct vdec_mpeg4_ipi_dec_start {
	uint32_t msg_id;
	uint32_t h_drv;
	struct vdec_mpeg4_bs bs;
	struct vdec_mpeg4_fb fb;
};

/**
 * struct vdec_mpeg4_ipi_init_ack - for VPU_IPIMSG_DEC_INIT_ACK
 * @ack          : ack information
 * @vdec_inst : AP vdec_mpeg4_inst address
 * @h_drv	: Handle to VPU driver
 * @shmem_addr  : VPU shared memory address (VPU is 32-bit)
 */
struct vdec_mpeg4_ipi_init_ack {
	struct vdec_vpu_ipi_ack ack;
	uint32_t h_drv;
	uint32_t shmem_addr;
};

/**
 * struct vdec_mpeg4_vpu_inst - VPU instance for MPEG4 decode
 * @ctx             : Handle of mtk_vcodec_ctx
 * @dev             : Handle of vpu platform device
 * @h_drv	   : handle to VPU driver
 * @failure        : VPU execution result status
 */
struct vdec_mpeg4_vpu_inst {
	void *ctx;
	struct platform_device *dev;
	void *shmem_addr;
	uint32_t h_drv;
	int failure;
};

struct vdec_mpeg4_vpu_inst *vdec_mpeg4_vpu_alloc(void *ctx)
{
	struct vdec_mpeg4_vpu_inst *vpu;

	vpu = kzalloc(sizeof(*vpu), GFP_KERNEL);
	if (vpu) {
		vpu->ctx = ctx;
		vpu->dev = mtk_vcodec_get_plat_dev(ctx);
	}
	return vpu;
}

void vdec_mpeg4_vpu_free(struct vdec_mpeg4_vpu_inst *vpu)
{
	kfree(vpu);
}

void *vdec_mpeg4_vpu_get_shmem(struct vdec_mpeg4_vpu_inst *vpu)
{
	return vpu->shmem_addr;
}

dma_addr_t vdec_mpeg4_vpu_get_dma(struct vdec_mpeg4_vpu_inst *vpu, uint32_t vpu_addr)
{
	return vpu_mapping_iommu_dm_addr(vpu->dev, vpu_addr);
}

static void handle_init_ack_msg(struct vdec_mpeg4_ipi_init_ack *msg)
{
	struct vdec_mpeg4_vpu_inst *vpu;

	vpu = (struct vdec_mpeg4_vpu_inst *)(uintptr_t)msg->ack.vdec_inst;
	mtk_vcodec_debug(vpu, "+ vdec_inst = %p", vpu);

	/* mapping VPU address to kernel virtual address */
	vpu->shmem_addr = vpu_mapping_dm_addr(vpu->dev, msg->shmem_addr);
	vpu->h_drv = msg->h_drv;
	mtk_vcodec_debug(vpu, "- h_drv = 0x%x", vpu->h_drv);
}

/*
 * This function runs in interrupt context and it means there's a IPI MSG
 * from VPU.
 */
static void mpeg4_dec_vpu_ipi_handler(void *msg, unsigned int len, void *priv)
{
	struct vdec_vpu_ipi_ack *ack;
	struct vdec_mpeg4_vpu_inst *vpu;

	ack = (struct vdec_vpu_ipi_ack *)msg;
	vpu = (struct vdec_mpeg4_vpu_inst *)(uintptr_t)ack->vdec_inst;

	mtk_vcodec_debug(vpu, "+ id=%X", ack->msg_id);

	vpu->failure = ack->status;

	if (ack->status == 0) {
		switch (ack->msg_id) {
		case VPU_IPIMSG_DEC_INIT_ACK:
			handle_init_ack_msg(msg);
			break;

		case VPU_IPIMSG_DEC_START_ACK:
		case VPU_IPIMSG_DEC_END_ACK:
		case VPU_IPIMSG_DEC_DEINIT_ACK:
		case VPU_IPIMSG_DEC_RESET_ACK:
			break;

		default:
			mtk_vcodec_err(vpu, "invalid msg=%X", ack->msg_id);
			break;
		}
	}

	mtk_vcodec_debug(vpu, "- id=%X", ack->msg_id);
}

static int mpeg4_dec_vpu_send_msg(struct vdec_mpeg4_vpu_inst *vpu, void *msg, int len)
{
	int err;

	mtk_vcodec_debug(vpu, "id=%X", *(unsigned int *)msg);

	vpu->failure = 0;

	err = vpu_ipi_send(vpu->dev, IPI_VDEC_MPEG4, msg, len);
	if (err != 0) {
		mtk_vcodec_err(vpu, "vpu_ipi_send fail status=%d", err);
		return err;
	}

	return vpu->failure;
}

int vdec_mpeg4_vpu_init(struct vdec_mpeg4_vpu_inst *vpu, struct vdec_mpeg4_bs *bs)
{
	struct vdec_mpeg4_ipi_init msg;
	int err;

	mtk_vcodec_debug_enter(vpu);

	err = vpu_ipi_register(vpu->dev, IPI_VDEC_MPEG4,
				   mpeg4_dec_vpu_ipi_handler, "vdec_mpeg4", NULL);
	if (err != 0) {
		mtk_vcodec_err(vpu, "vpu_ipi_register fail status=%d", err);
		return err;
	}

	msg.msg_id = AP_IPIMSG_DEC_INIT;
	msg.vdec_inst = (uint64_t)(uintptr_t)vpu;
	memcpy(&msg.bs, bs, sizeof(*bs));
	mtk_vcodec_debug(vpu, "vdec_inst=0x%llx bs_sz=0x%x", msg.vdec_inst, msg.bs.size);

	err = mpeg4_dec_vpu_send_msg(vpu, &msg, sizeof(msg));

	mtk_vcodec_debug(vpu, "- ret=%d", err);
	return err;
}

int vdec_mpeg4_vpu_dec_start(struct vdec_mpeg4_vpu_inst *vpu, struct vdec_mpeg4_bs *bs,
			     struct vdec_mpeg4_fb *fb)
{
	struct vdec_mpeg4_ipi_dec_start msg;
	int err = 0;

	msg.msg_id = AP_IPIMSG_DEC_START;
	msg.h_drv = vpu->h_drv;
	memcpy(&msg.bs, bs, sizeof(*bs));
	memcpy(&msg.fb, fb, sizeof(*fb));

	err = mpeg4_dec_vpu_send_msg(vpu, &msg, sizeof(msg));

	mtk_vcodec_debug(vpu, "- ret=%d", err);
	return err;
}

static int mpeg4_dec_send_ap_ipi(struct vdec_mpeg4_vpu_inst *vpu, unsigned int msg_id)
{
	struct vdec_ap_ipi_cmd msg;
	int err = 0;

	mtk_vcodec_debug(vpu, "+ id=%X", msg_id);

	msg.msg_id = msg_id;
	msg.h_drv = vpu->h_drv;

	err = mpeg4_dec_vpu_send_msg(vpu, &msg, sizeof(msg));

	mtk_vcodec_debug(vpu, "- id=%X ret=%d", msg_id, err);
	return err;
}

int vdec_mpeg4_vpu_dec_end(struct vdec_mpeg4_vpu_inst *vpu)
{
	return mpeg4_dec_send_ap_ipi(vpu, AP_IPIMSG_DEC_END);
}

int vdec_mpeg4_vpu_deinit(struct vdec_mpeg4_vpu_inst *vpu)
{
	return mpeg4_dec_send_ap_ipi(vpu, AP_IPIMSG_DEC_DEINIT);
}

int vdec_mpeg4_vpu_reset(struct vdec_mpeg4_vpu_inst *vpu)
{
	return mpeg4_dec_send_ap_ipi(vpu, AP_IPIMSG_DEC_RESET);
}
