/*
 * Copyright (c) 2016 MediaTek Inc.
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

#include <linux/fdtable.h>
#include <linux/interrupt.h>
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "vdec_ipi_msg.h"
#include "vdec_vcu_if.h"

static void handle_init_ack_msg(struct vdec_vcu_ipi_init_ack *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)
					(unsigned long)msg->ap_inst_addr;

	mtk_vcodec_debug(vcu, "+ ap_inst_addr = 0x%llx", msg->ap_inst_addr);

	/* mapping VCU address to kernel virtual address */
	/* the content in vsi is initialized to 0 in VCU */
	vcu->vsi = vcu_mapping_dm_addr(vcu->dev, msg->vcu_inst_addr);
	vcu->inst_addr = msg->vcu_inst_addr;
	mtk_vcodec_debug(vcu, "- vcu_inst_addr = 0x%x", vcu->inst_addr);
}

int get_mapped_fd(struct dma_buf *dmabuf)
{
	int target_fd;
	unsigned long rlim_cur;
	unsigned long irqs;
	struct task_struct *task = NULL;
	struct files_struct *f = NULL;

	if (dmabuf == NULL || dmabuf->file == NULL)
		return 0;

	vcu_get_task(&task, &f);

	if (!lock_task_sighand(task, &irqs))
		return -EMFILE;

	rlim_cur = task_rlimit(task, RLIMIT_NOFILE);
	unlock_task_sighand(task, &irqs);

	target_fd = __alloc_fd(f, 0, rlim_cur, O_CLOEXEC);

	get_file(dmabuf->file);
	__fd_install(f, target_fd, dmabuf->file);

	pr_info("get_mapped_fd: %d", target_fd);
	return target_fd;
}

/*
 * This function runs in interrupt context and it means there's a IPI MSG
 * from VCU.
 */
int vcu_dec_ipi_handler(void *data, unsigned int len, void *priv)
{
	struct vdec_vcu_ipi_ack *msg = data;
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)
		(unsigned long)msg->ap_inst_addr;
	int ret = 0;
	struct timeval t_s, t_e;

	mtk_vcodec_debug(vcu, "+ id=%X status = %d\n", msg->msg_id, msg->status);

	vcu->failure = msg->status;

	if (msg->status == 0) {
		switch (msg->msg_id) {
		case VCU_IPIMSG_DEC_INIT_ACK:
			handle_init_ack_msg(data);
			break;

		case VCU_IPIMSG_DEC_START_ACK:
		case VCU_IPIMSG_DEC_END_ACK:
		case VCU_IPIMSG_DEC_DEINIT_ACK:
		case VCU_IPIMSG_DEC_RESET_ACK:
		case VCU_IPIMSG_DEC_SET_PARAM_ACK:
			break;
		case VCU_IPIMSG_DEC_WAITISR:
			/* wait decoder done interrupt */
			do_gettimeofday(&t_s);
			mtk_vcodec_wait_for_done_ctx(vcu->ctx,
						     MTK_INST_IRQ_RECEIVED,
						     WAIT_INTR_TIMEOUT_MS);
			do_gettimeofday(&t_e);
			mtk_v4l2_debug(5, "IRQtimeuse:%ld\n", (t_e.tv_sec - t_s.tv_sec) * 1000000 +
					(t_e.tv_usec - t_s.tv_usec));
			ret = 1;
			break;
		case VCU_IPIMSG_DEC_CLOCK_ON:
			/* TEST: need to remove v4l2 code to do experiment.
			 * It may be removed later
			 */
			mtk_vcodec_dec_clock_on(&vcu->ctx->dev->pm);
			enable_irq(vcu->ctx->dev->dec_irq);
			ret = 1;
			break;
		case VCU_IPIMSG_DEC_CLOCK_OFF:
			/* TEST: need to remove v4l2 code to do experiment.
			 * It may be removed later
			 */
			disable_irq(vcu->ctx->dev->dec_irq);
			mtk_vcodec_dec_clock_off(&vcu->ctx->dev->pm);
			ret = 1;
			break;
		case VCU_IPIMSG_DEC_GET_FRAME_BUFFER:
			/* TODO: return til it can get available frame buffer */
			ret = 1;
			break;
		default:
			mtk_vcodec_err(vcu, "invalid msg=%X", msg->msg_id);
			ret = 1;
			break;
		}
	}

	mtk_vcodec_debug(vcu, "- id=%X", msg->msg_id);
	vcu->signaled = 1;

	return ret;
}

static int vcodec_vcu_send_msg(struct vdec_vcu_inst *vcu, void *msg, int len)
{
	int err;

	mtk_vcodec_debug(vcu, "id=%X", *(uint32_t *)msg);

	vcu->failure = 0;
	vcu->signaled = 0;

	err = vcu_ipi_send(vcu->dev, vcu->id, msg, len);
	if (err) {
		mtk_vcodec_err(vcu, "send fail vcu_id=%d msg_id=%X status=%d",
			       vcu->id, *(uint32_t *)msg, err);
		return err;
	}

	return vcu->failure;
}

static int vcodec_send_ap_ipi(struct vdec_vcu_inst *vcu, unsigned int msg_id)
{
	struct vdec_ap_ipi_cmd msg;
	int err = 0;

	mtk_vcodec_debug(vcu, "+ id=%X", msg_id);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = msg_id;
	msg.vcu_inst_addr = vcu->inst_addr;

	err = vcodec_vcu_send_msg(vcu, &msg, sizeof(msg));
	mtk_vcodec_debug(vcu, "- id=%X ret=%d", msg_id, err);
	return err;
}

int vcu_dec_init(struct vdec_vcu_inst *vcu)
{
	struct vdec_ap_ipi_init msg;
	int err;

	mtk_vcodec_debug_enter(vcu);

	init_waitqueue_head(&vcu->wq);
	vcu->signaled = 0;
	vcu->failure = 0;

	err = vcu_ipi_register(vcu->dev, vcu->id, vcu->handler, NULL, NULL);
	if (err != 0) {
		mtk_vcodec_err(vcu, "vcu_ipi_register fail status=%d", err);
		return err;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_INIT;
	msg.ap_inst_addr = (unsigned long)vcu;

	mtk_vcodec_debug(vcu, "vdec_inst=%p", vcu);

	err = vcodec_vcu_send_msg(vcu, (void *)&msg, sizeof(msg));
	mtk_vcodec_debug(vcu, "- ret=%d", err);
	return err;
}

int vcu_dec_start(struct vdec_vcu_inst *vcu, unsigned int *data,
		  unsigned int len)
{
	struct vdec_ap_ipi_dec_start msg;
	int i;
	int err = 0;

	mtk_vcodec_debug_enter(vcu);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_START;
	msg.vcu_inst_addr = vcu->inst_addr;

	for (i = 0; i < len; i++)
		msg.data[i] = data[i];

	err = vcodec_vcu_send_msg(vcu, (void *)&msg, sizeof(msg));
	mtk_vcodec_debug(vcu, "- ret=%d", err);
	return err;
}

int vcu_dec_end(struct vdec_vcu_inst *vcu)
{
	return vcodec_send_ap_ipi(vcu, AP_IPIMSG_DEC_END);
}

int vcu_dec_deinit(struct vdec_vcu_inst *vcu)
{
	return vcodec_send_ap_ipi(vcu, AP_IPIMSG_DEC_DEINIT);
}

int vcu_dec_reset(struct vdec_vcu_inst *vcu)
{
	return vcodec_send_ap_ipi(vcu, AP_IPIMSG_DEC_RESET);
}

int vcu_dec_set_param(struct vdec_vcu_inst *vcu, unsigned int id, void *param, unsigned int size)
{
	struct vdec_ap_ipi_set_param msg;
	uint32_t *param_ptr = (uint32_t *)param;
	int err = 0;
	int i = 0;

	mtk_vcodec_debug(vcu, "+ id=%X", AP_IPIMSG_DEC_SET_PARAM);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_SET_PARAM;
	msg.id = id;
	msg.vcu_inst_addr = vcu->inst_addr;
	for (i = 0; i < size; i++)
		msg.data[i] = *(param_ptr + i);

	err = vcodec_vcu_send_msg(vcu, &msg, sizeof(msg));
	mtk_vcodec_debug(vcu, "- id=%X ret=%d", AP_IPIMSG_DEC_SET_PARAM, err);

	return err;
}
