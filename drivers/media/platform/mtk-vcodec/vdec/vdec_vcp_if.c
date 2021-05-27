/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2016 MediaTek Inc.
* Author: Chia-Mao Hung<chia-mao.hung@mediatek.com>
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <mt-plat/mtk_tinysys_ipi.h>
#include <uapi/linux/mtk_vcu_controls.h>

#include "vdec_drv_base.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_drv.h"
#include "vdec_drv_if.h"
#include "scp_ipi_pin.h"
#include "scp_mbox_layout.h"  /* for IPI mbox size */
#include "scp_helper.h"
// TODO: need remove ISR ipis
#include "mtk_vcodec_intr.h"

#ifdef CONFIG_MTK_ENG_BUILD
#define IPI_TIMEOUT_MS          16000U
#else
#define IPI_TIMEOUT_MS          5000U
#endif

static void put_fb_to_free(struct vdec_inst *inst, struct vdec_fb *fb)
{
	struct ring_fb_list *list;

	if (fb != NULL) {
		list = &inst->vsi->list_free;
		if (list->count == DEC_MAX_FB_NUM) {
			mtk_vcodec_err(inst, "[FB] put fb free_list full");
			return;
		}

		mtk_vcodec_debug(inst,
						 "[FB] put fb into free_list @(%p, %llx)",
						 fb->fb_base[0].va,
						 (u64)fb->fb_base[1].dma_addr);

		list->fb_list[list->write_idx].vdec_fb_va = (u64)(uintptr_t)fb;
		list->write_idx = (list->write_idx == DEC_MAX_FB_NUM - 1U) ?
						  0U : list->write_idx + 1U;
		list->count++;
	}
}

static void get_pic_info(struct vdec_inst *inst,
						 struct vdec_pic_info *pic)
{
	if (inst == NULL)
		return;
	if (inst->vsi == NULL)
		return;

	memcpy(pic, &inst->vsi->pic, sizeof(struct vdec_pic_info));

	mtk_vcodec_debug(inst, "pic(%d, %d), buf(%d, %d), bitdepth = %d, fourcc = %d\n",
		pic->pic_w, pic->pic_h, pic->buf_w, pic->buf_h,
		pic->bitdepth, pic->fourcc);
	mtk_vcodec_debug(inst, "Y/C(%d, %d)", pic->fb_sz[0], pic->fb_sz[1]);
}

static void get_crop_info(struct vdec_inst *inst, struct v4l2_crop *cr)
{
	if (inst == NULL)
		return;
	if (inst->vsi == NULL)
		return;

	cr->c.left      = inst->vsi->crop.left;
	cr->c.top       = inst->vsi->crop.top;
	cr->c.width     = inst->vsi->crop.width;
	cr->c.height    = inst->vsi->crop.height;
	mtk_vcodec_debug(inst, "l=%d, t=%d, w=%d, h=%d",
		cr->c.left, cr->c.top, cr->c.width, cr->c.height);
}

static void get_dpb_size(struct vdec_inst *inst, unsigned int *dpb_sz)
{
	if (inst == NULL)
		return;
	if (inst->vsi == NULL)
		return;

	*dpb_sz = inst->vsi->dec.dpb_sz + 1U;
	mtk_vcodec_debug(inst, "sz=%d", *dpb_sz);
}


static int vdec_vcp_ipi_send(struct vdec_inst *inst, void *msg, int len, bool is_ack)
{
	int ret, ipi_size;
	unsigned long timeout;
	struct share_obj obj;

	if (!is_scp_ready(SCP_A_ID))
		mtk_vcodec_err(inst, "SCP_A_ID not ready");

	if (len > sizeof(struct share_obj))
		mtk_vcodec_err(inst, "ipi data size wrong %d > %d", len, sizeof(struct share_obj));

	if (!is_ack)
		mutex_lock(&inst->ctx->dev->ipi_mutex);
	memset(&obj, 0, sizeof(obj));
	memcpy(obj.share_buf, msg, len);
	if (*(__u32 *)msg == AP_IPIMSG_DEC_FRAME_BUFFER)
		obj.id = IPI_VDEC_RESOURCE;
	else
		obj.id = inst->vcu.id;
	obj.len = len;
	ipi_size = ((sizeof(u32) * 2) + len + 3) /4;

	mtk_v4l2_debug(2, "id %d len %d msg 0x%x is_ack %d %d", obj.id, obj.len, *(u32 *)msg,
		is_ack, inst->vcu.signaled);
	ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_VDEC_1, IPI_SEND_WAIT, &obj,
		ipi_size, IPI_TIMEOUT_MS);

	if (is_ack)
		return 0;

	if (ret != IPI_ACTION_DONE) {
		mtk_vcodec_err(inst, "mtk_ipi_send fail %d", ret);
		mutex_unlock(&inst->ctx->dev->ipi_mutex);
		inst->vcu.failure = VDEC_IPI_MSG_STATUS_FAIL;
		return -EIO;
	}

	if (!is_ack) {
		/* wait for VCP's ACK */
		timeout = msecs_to_jiffies(IPI_TIMEOUT_MS);
		ret = wait_event_timeout(inst->vcu.wq, inst->vcu.signaled, timeout);
		inst->vcu.signaled = false;

		if (ret == 0) {
			mtk_vcodec_err(inst, "wait vcp ipi %X ack time out !%d", *(u32 *)msg, ret);
			mutex_unlock(&inst->ctx->dev->ipi_mutex);
			inst->vcu.failure = VDEC_IPI_MSG_STATUS_FAIL;
			return -EIO;
		}
	}
	mutex_unlock(&inst->ctx->dev->ipi_mutex);

	return 0;
}

static void handle_init_ack_msg(struct vdec_vcu_ipi_init_ack *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)
		(unsigned long)msg->ap_inst_addr;
	__u64 shmem_pa_start = (__u64)scp_get_reserve_mem_phys(VDEC_MEM_ID);
	__u64 inst_offset = ((msg->vcu_inst_addr & 0x0FFFFFFF) - (shmem_pa_start & 0x0FFFFFFF));

	if (vcu == NULL)
		return;
	mtk_vcodec_debug(vcu, "+ ap_inst_addr = 0x%lx",
		(uintptr_t)msg->ap_inst_addr);

	vcu->vsi = (void *)((__u64)scp_get_reserve_mem_virt(VDEC_MEM_ID) + inst_offset);
	vcu->inst_addr = msg->vcu_inst_addr;
	mtk_vcodec_debug(vcu, "- vcu_inst_addr = 0x%x", vcu->inst_addr);
}

static void handle_query_cap_ack_msg(struct vdec_vcu_ipi_query_cap_ack *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)msg->ap_inst_addr;
	void *data;
	int size = 0;
	__u64 shmem_pa_start = (__u64)scp_get_reserve_mem_phys(VDEC_MEM_ID);
	__u64 data_offset = ((msg->vcu_data_addr & 0x0FFFFFFF) - (shmem_pa_start & 0x0FFFFFFF));

	if (vcu == NULL)
		return;
	mtk_vcodec_debug(vcu, "+ ap_inst_addr = 0x%lx, vcu_data_addr = 0x%x, id = %d",
		(uintptr_t)msg->ap_inst_addr, msg->vcu_data_addr, msg->id);
	/* mapping VCU address to kernel virtual address */
	data =  (void *)((__u64)scp_get_reserve_mem_virt(VDEC_MEM_ID) + data_offset);
	if (data == NULL)
		return;
	switch (msg->id) {
	case GET_PARAM_VDEC_CAP_SUPPORTED_FORMATS:
		size = sizeof(struct mtk_video_fmt);
		memcpy((void *)msg->ap_data_addr, data,
			 size * MTK_MAX_DEC_CODECS_SUPPORT);
		break;
	case GET_PARAM_VDEC_CAP_FRAME_SIZES:
		size = sizeof(struct mtk_codec_framesizes);
		memcpy((void *)msg->ap_data_addr, data,
			size * MTK_MAX_DEC_CODECS_SUPPORT);
		break;
	default:
		break;
	}
	mtk_vcodec_debug(vcu, "- vcu_inst_addr = 0x%x", vcu->inst_addr);
}

static void handle_vdec_mem_alloc(struct vdec_vcu_ipi_mem_op *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)msg->ap_inst_addr;
	struct vdec_inst *inst = NULL;

	if (msg->mem.type == MEM_TYPE_FOR_SHM) {
		msg->status = 0;
		msg->mem.va = (__u64)scp_get_reserve_mem_virt(VDEC_MEM_ID);
		msg->mem.pa = (__u64)scp_get_reserve_mem_phys(VDEC_MEM_ID);
		msg->mem.len = (__u64)scp_get_reserve_mem_size(VDEC_MEM_ID);
		msg->mem.iova = 0;
		mtk_v4l2_debug(4, "va 0x%llx pa 0x%llx iova 0x%llx len %d type %d size of %d %d\n",
			msg->mem.va, msg->mem.pa, msg->mem.iova, msg->mem.len, msg->mem.type, sizeof(msg->mem), sizeof(*msg));
		// TODO: need add iova
	} else {
		if (IS_ERR_OR_NULL(vcu))
			return;

		mtk_vcodec_debug(vcu, "va 0x%llx pa 0x%llx iova 0x%llx len %d type %d\n",
			msg->mem.va, msg->mem.pa, msg->mem.iova, msg->mem.len,	msg->mem.type);
		// TODO: need add list record for exception error handling
		inst = container_of(vcu, struct vdec_inst, vcu);
		msg->status = mtk_vcodec_alloc_mem(&msg->mem, &vcu->ctx->dev->plat_dev->dev, VDEC_WORK_ID, vcu->ctx->dev->dec_mem_slot_stat);
	}
	if (msg->status)
		mtk_vcodec_err(vcu, "fail %d, va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			msg->status, msg->mem.va, msg->mem.pa,
			msg->mem.iova, msg->mem.len,  msg->mem.type);

}

static void handle_vdec_mem_free(struct vdec_vcu_ipi_mem_op *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)msg->ap_inst_addr;
	struct vdec_inst *inst = NULL;

	if (IS_ERR_OR_NULL(vcu))
		return;

	mtk_vcodec_debug(vcu, "va 0x%llx pa 0x%llx iova 0x%llx len %d type %d\n",
		msg->mem.va, msg->mem.pa, msg->mem.iova, msg->mem.len,  msg->mem.type);

	inst = container_of(vcu, struct vdec_inst, vcu);
	msg->status = mtk_vcodec_free_mem(&msg->mem, &vcu->ctx->dev->plat_dev->dev, VDEC_WORK_ID, vcu->ctx->dev->dec_mem_slot_stat);

	if (msg->status)
		mtk_vcodec_err(vcu, "fail %d, va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			msg->status, msg->mem.va, msg->mem.pa,
			msg->mem.iova, msg->mem.len,  msg->mem.type);

}

int vcp_dec_ipi_handler(void *arg)
{
	struct mtk_vcodec_dev *dev = (struct mtk_vcodec_dev *)arg;
	struct vdec_vcu_ipi_ack *msg = NULL;
	struct vdec_vcu_inst *vcu = NULL;
	struct vdec_inst *inst = NULL;
	struct share_obj *obj;
	struct vdec_vsi *vsi;
	int ret = 0;
	struct mtk_vcodec_msg_node *mq_node;
	struct vdec_vcu_ipi_mem_op *shem_msg;
	unsigned long flags;

	mtk_v4l2_debug_enter();
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_cmd) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_init) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_dec_start) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_set_param) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_query_cap) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_vcu_ipi_ack) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_vcu_ipi_init_ack) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(
		sizeof(struct vdec_vcu_ipi_query_cap_ack) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(
		sizeof(struct vdec_vcu_ipi_query_cap_ack) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(
		sizeof(struct vdec_vcu_ipi_mem_op) > SHARE_BUF_SIZE);

	do {
		wait_event(dev->mq.wq, atomic_read(&dev->mq.cnt) > 0);

		spin_lock_irqsave(&dev->mq.lock, flags);
		mq_node = list_entry(dev->mq.head.next, struct mtk_vcodec_msg_node, list);
		list_del(&(mq_node->list));
		atomic_dec(&dev->mq.cnt);
		spin_unlock_irqrestore(&dev->mq.lock, flags);

		obj = &mq_node->ipi_data;
		msg = (struct vdec_vcu_ipi_ack *)obj->share_buf;

		if (msg == NULL || (struct vdec_vcu_inst *)msg->ap_inst_addr == NULL) {
			mtk_v4l2_err(" msg invalid %lx\n", msg);
			kfree(mq_node);
			continue;
		}

		/* handling VSI (shared memory) preparation when VCP init service without inst*/
		if (msg->msg_id == VCU_IPIMSG_DEC_MEM_ALLOC) {
			shem_msg = (struct vdec_vcu_ipi_mem_op *)obj->share_buf;
			if (shem_msg->mem.type == MEM_TYPE_FOR_SHM) {
				handle_vdec_mem_alloc((void *)shem_msg);
				shem_msg->msg_id = AP_IPIMSG_DEC_MEM_ALLOC_DONE;
				ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_VDEC_1, IPI_SEND_WAIT, obj,
					PIN_OUT_SIZE_VDEC, 100);
				if (ret != IPI_ACTION_DONE)
					mtk_v4l2_err("mtk_ipi_send fail %d", ret);
				kfree(mq_node);
				continue;
			}
		}

		vcu = (struct vdec_vcu_inst *)(unsigned long)msg->ap_inst_addr;
		if (vcu->abort) {
			mtk_v4l2_err(" msg vcu abort\n");
			kfree(mq_node);
			continue;
		}
		mtk_v4l2_debug(2, "+ pop msg_id %X ml_cnt %d, vcu %lx, status %d",
			msg->msg_id, atomic_read(&dev->mq.cnt), (unsigned long)vcu, msg->status);

		vsi = (struct vdec_vsi *)vcu->vsi;
		inst = container_of(vcu, struct vdec_inst, vcu);

		if (msg->status == 0) {
			switch (msg->msg_id) {
			case VCU_IPIMSG_DEC_INIT_DONE:
				handle_init_ack_msg((void *)obj->share_buf);
			case VCU_IPIMSG_DEC_START_DONE:
			case VCU_IPIMSG_DEC_DONE:
			case VCU_IPIMSG_DEC_DEINIT_DONE:
			case VCU_IPIMSG_DEC_RESET_DONE:
			case VCU_IPIMSG_DEC_SET_PARAM_DONE:
				vcu->signaled = true;
				wake_up(&vcu->wq);
				break;
			case VCU_IPIMSG_DEC_QUERY_CAP_DONE:
				handle_query_cap_ack_msg((void *)obj->share_buf);
				vcu->signaled = true;
				wake_up(&vcu->wq);
				break;
			case VCU_IPIMSG_DEC_PUT_FRAME_BUFFER:
				mtk_vdec_put_fb(vcu->ctx, PUT_BUFFER_CALLBACK);
				ret = 1;
				msg->msg_id = AP_IPIMSG_DEC_PUT_FRAME_BUFFER_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), 1);
				break;
			case VCU_IPIMSG_DEC_MEM_ALLOC:
				handle_vdec_mem_alloc((void *)obj->share_buf);
				msg->msg_id = AP_IPIMSG_DEC_MEM_ALLOC_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(struct vdec_vcu_ipi_mem_op), 1);
				break;
			case VCU_IPIMSG_DEC_MEM_FREE:
				handle_vdec_mem_free((void *)obj->share_buf);
				msg->msg_id = AP_IPIMSG_DEC_MEM_FREE_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(struct vdec_vcu_ipi_mem_op), 1);
				break;
			// TODO: need remove HW locks /power & ISR ipis
			case VCU_IPIMSG_DEC_LOCK_LAT:
				vdec_decode_prepare(vcu->ctx, MTK_VDEC_LAT);
				ret = 1;
				msg->msg_id = AP_IPIMSG_DEC_LOCK_LAT_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), 1);
				break;
			case VCU_IPIMSG_DEC_UNLOCK_LAT:
				vdec_decode_unprepare(vcu->ctx, MTK_VDEC_LAT);
				ret = 1;
				msg->msg_id = AP_IPIMSG_DEC_UNLOCK_LAT_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), 1);
				break;
			case VCU_IPIMSG_DEC_LOCK_CORE:
				vdec_decode_prepare(vcu->ctx, MTK_VDEC_CORE);
				ret = 1;
				msg->msg_id = AP_IPIMSG_DEC_LOCK_CORE_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), 1);
				break;
			case VCU_IPIMSG_DEC_UNLOCK_CORE:
				vdec_decode_unprepare(vcu->ctx, MTK_VDEC_CORE);
				ret = 1;
				msg->msg_id = AP_IPIMSG_DEC_UNLOCK_CORE_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), 1);
				break;
			case VCU_IPIMSG_DEC_WAITISR:
				vcodec_trace_count("VDEC_HW_CORE", 2);
				/* wait decoder done interrupt */
				ret = mtk_vcodec_wait_for_done_ctx(vcu->ctx,
					msg->status,
					MTK_INST_IRQ_RECEIVED,
					WAIT_INTR_TIMEOUT_MS);
				msg->msg_id = AP_IPIMSG_DEC_WAITISR_DONE;
				vcodec_trace_count("VDEC_HW_CORE", 1);
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), 1);
				break;
			case VCU_IPIMSG_DEC_GET_FRAME_BUFFER:
				mtk_vcodec_err(vcu, "GET_FRAME_BUFFER not support", msg->msg_id);
				ret = 1;
				break;
			default:
				mtk_vcodec_err(vcu, "invalid msg=%X", msg->msg_id);
				ret = 1;
				break;
			}
		} else {
			switch (msg->msg_id) {
			case VCU_IPIMSG_DEC_INIT_DONE:
			case VCU_IPIMSG_DEC_START_DONE:
			case VCU_IPIMSG_DEC_DONE:
			case VCU_IPIMSG_DEC_DEINIT_DONE:
			case VCU_IPIMSG_DEC_RESET_DONE:
			case VCU_IPIMSG_DEC_SET_PARAM_DONE:
			case VCU_IPIMSG_DEC_QUERY_CAP_DONE:
				vcu->signaled = true;
				wake_up(&vcu->wq);
				break;
			case VCU_IPIMSG_DEC_WAITISR:
				vcodec_trace_count("VDEC_HW_LAT", 2);
				/* wait decoder done interrupt */
				ret = mtk_vcodec_wait_for_done_ctx(vcu->ctx,
					msg->status,
					MTK_INST_IRQ_RECEIVED,
					WAIT_INTR_TIMEOUT_MS);
				msg->msg_id = AP_IPIMSG_DEC_WAITISR_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), 1);
				vcodec_trace_count("VDEC_HW_LAT", 1);
				break;
			default:
				mtk_vcodec_err(vcu, "invalid msg=%X", msg->msg_id);
				ret = 1;
				break;
			}
		}
		mtk_vcodec_debug(vcu, "- id=%X", msg->msg_id);
		kfree(mq_node);
	} while (!kthread_should_stop());
	mtk_v4l2_debug_leave();

	return ret;
}

static int vdec_vcp_ipi_isr(unsigned int id, void *prdata, void *data,
	 unsigned int len) {
	struct mtk_vcodec_dev *dev = (struct mtk_vcodec_dev *)prdata;
	struct vdec_vcu_ipi_ack *msg = NULL;
	struct share_obj *obj = (struct share_obj *)data;
	struct mtk_vcodec_msg_node *mq_node;
	unsigned long flags;

	msg = (struct vdec_vcu_ipi_ack *)obj->share_buf;

	// add to ipi msg list
	mq_node = (struct mtk_vcodec_msg_node *)kmalloc(sizeof(struct mtk_vcodec_msg_node),
		GFP_DMA |GFP_ATOMIC);

	if (NULL != mq_node) {
		memcpy(&mq_node->ipi_data, obj, sizeof(struct share_obj));
		spin_lock_irqsave(&dev->mq.lock, flags);
		list_add_tail(&mq_node->list, &dev->mq.head);
		atomic_inc(&dev->mq.cnt);
		spin_unlock_irqrestore(&dev->mq.lock, flags);
		mtk_v4l2_debug(2, "push ipi_id %x msg_id %x, ml_cnt %d",
			obj->id, msg->msg_id, atomic_read(&dev->mq.cnt));
		wake_up(&dev->mq.wq);
	} else {
		mtk_v4l2_err("kmalloc fail\n");
	}

	return 0;
}

void vdec_vcp_probe(struct mtk_vcodec_dev *dev)
{
	int ret;
	void *vcp_ipi_data;

	mtk_v4l2_debug_enter();
	vcp_ipi_data = kmalloc(sizeof(struct share_obj), GFP_KERNEL);

	INIT_LIST_HEAD(&dev->mq.head);
	spin_lock_init(&dev->mq.lock);
	init_waitqueue_head(&dev->mq.wq);
	atomic_set(&dev->mq.cnt, 0);

	ret = mtk_ipi_register(&scp_ipidev, IPI_IN_VDEC_1, vdec_vcp_ipi_isr, dev, vcp_ipi_data);
	if (ret) {
		mtk_v4l2_err(" ipi_register fail, ret %d\n", ret);
	}

	kthread_run(vcp_dec_ipi_handler, dev, "vdec_ipi_recv");

	if (mtk_vcodec_init_reserve_mem_slot(VDEC_WORK_ID, &dev->dec_mem_slot_stat))
		mtk_v4l2_err ("[err] %s init reserve memory slot failed\n", __func__);

	mtk_v4l2_debug_leave();
}
EXPORT_SYMBOL(vdec_vcp_probe);

static int vdec_vcp_init(struct mtk_vcodec_ctx *ctx, unsigned long *h_vdec)
{
	int err = 0;
	struct vdec_ap_ipi_init msg;
	struct vdec_inst *inst = NULL;

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return -ENOMEM;
	if (!ctx) {
		err = -ENOMEM;
		goto error_free_inst;
	}

	inst->ctx = ctx;

	switch (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc) {
	case V4L2_PIX_FMT_H264:
		inst->vcu.id = IPI_VDEC_H264;
		break;
	case V4L2_PIX_FMT_H265:
		inst->vcu.id = IPI_VDEC_H265;
		break;
	case V4L2_PIX_FMT_HEIF:
		inst->vcu.id = IPI_VDEC_HEIF;
		break;
	case V4L2_PIX_FMT_VP8:
		inst->vcu.id = IPI_VDEC_VP8;
		break;
	case V4L2_PIX_FMT_VP9:
		inst->vcu.id = IPI_VDEC_VP9;
		break;
	case V4L2_PIX_FMT_MPEG4:
		inst->vcu.id = IPI_VDEC_MPEG4;
		break;
	case V4L2_PIX_FMT_H263:
		inst->vcu.id = IPI_VDEC_H263;
		break;
	case V4L2_PIX_FMT_S263:
		inst->vcu.id = IPI_VDEC_S263;
		break;
	case V4L2_PIX_FMT_MPEG1:
	case V4L2_PIX_FMT_MPEG2:
		inst->vcu.id = IPI_VDEC_MPEG12;
		break;
	case V4L2_PIX_FMT_WMV1:
	case V4L2_PIX_FMT_WMV2:
	case V4L2_PIX_FMT_WMV3:
	case V4L2_PIX_FMT_WMVA:
	case V4L2_PIX_FMT_WVC1:
		inst->vcu.id = IPI_VDEC_WMV;
		break;
	case V4L2_PIX_FMT_RV30:
		inst->vcu.id = IPI_VDEC_RV30;
		break;
	case V4L2_PIX_FMT_RV40:
		inst->vcu.id = IPI_VDEC_RV40;
		break;
	case V4L2_PIX_FMT_AV1:
		inst->vcu.id = IPI_VDEC_AV1;
		break;
	default:
		mtk_vcodec_err(inst, "%s no fourcc", __func__);
		break;
	}

	inst->vcu.ctx = ctx;
	init_waitqueue_head(&inst->vcu.wq);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_INIT;
	msg.ap_inst_addr = (unsigned long)&inst->vcu;

	if (ctx->dec_params.svp_mode)
		msg.reserved = ctx->dec_params.svp_mode;

	mtk_vcodec_debug(inst, "vdec_inst=%p svp_mode=%d", &inst->vcu, msg.reserved);
	err = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), 0);

	if (err != 0) {
		mtk_vcodec_err(inst, "%s err=%d", __func__, err);
		goto error_free_inst;
	}

	inst->vsi = (struct vdec_vsi *)inst->vcu.vsi;
	inst->vcu.signaled = false;
	ctx->input_driven = inst->vsi->input_driven;
	ctx->ipi_blocked = &inst->vsi->ipi_blocked;
	*(ctx->ipi_blocked) = 0;

	mtk_vcodec_debug(inst, "Decoder Instance >> %p", inst);

	*h_vdec = (unsigned long)inst;
	return 0;

error_free_inst:
	kfree(inst);
	*h_vdec = (unsigned long)NULL;

	return err;
}

static void vdec_vcp_deinit(unsigned long h_vdec)
{
	struct vdec_inst *inst = (struct vdec_inst *)h_vdec;
	struct vdec_ap_ipi_cmd msg;
	int err = 0;

	mtk_vcodec_debug_enter(inst);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_DEINIT;
	msg.vcu_inst_addr = inst->vcu.inst_addr;

	err = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), 0);
	mtk_vcodec_debug(inst, "- ret=%d", err);

	kfree(inst);
}

int vdec_vcp_reset(struct vdec_inst *inst, enum vdec_reset_type drain_type)
{
	struct vdec_ap_ipi_cmd msg;
	int err = 0;
	mtk_vcodec_debug(inst, "drain_type %d +", drain_type);
	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_RESET;
	msg.vcu_inst_addr = inst->vcu.inst_addr;
	msg.reserved = drain_type;

	err = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), 0);
	mtk_vcodec_debug(inst, "- ret=%d", err);

	return err;
}

static int vdec_vcp_decode(unsigned long h_vdec, struct mtk_vcodec_mem *bs,
	struct vdec_fb *fb, unsigned int *src_chg)
{
	int ret = 0;

	struct vdec_inst *inst = (struct vdec_inst *)h_vdec;
	struct vdec_vcu_inst *vcu = &inst->vcu;
	struct vdec_ap_ipi_dec_start msg;
	uint64_t vdec_fb_va;
	uint64_t fb_dma[VIDEO_MAX_PLANES] = { 0 };
	uint32_t num_planes;
	unsigned int i = 0;
	unsigned int bs_fourcc = inst->ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
	unsigned int fm_fourcc = inst->ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc;
	unsigned int *errormap_info = &inst->ctx->errormap_info[0];

	num_planes = fb ? inst->vsi->dec.fb_num_planes : 0U;

	for (i = 0; i < num_planes; i++)
		fb_dma[i] = (u64)fb->fb_base[i].dma_addr;

	vdec_fb_va = (u64)(uintptr_t)fb;

	mtk_vcodec_debug(inst, "+ [%d] FB y_dma=%llx c_dma=%llx va=%p num_planes %d",
		inst->num_nalu, fb_dma[0], fb_dma[1], fb, num_planes);

	/* bs == NULL means reset decoder */
	if (bs == NULL) {
		if (fb == NULL)
			return vdec_vcp_reset(inst, VDEC_FLUSH);   /* flush (0) */
		else if (fb->status == 0)
			return vdec_vcp_reset(inst, VDEC_DRAIN);   /* drain (1) */
		else
			return vdec_vcp_reset(inst, VDEC_DRAIN_EOS);   /* drain and return EOS frame (2) */
	}

	mtk_vcodec_debug(inst, "+ BS dma=0x%llx dmabuf=%p format=%c%c%c%c",
		(uint64_t)bs->dma_addr, bs->dmabuf, bs_fourcc & 0xFF,
		(bs_fourcc >> 8) & 0xFF, (bs_fourcc >> 16) & 0xFF,
		(bs_fourcc >> 24) & 0xFF);

	inst->vsi->dec.vdec_bs_va = (u64)(uintptr_t)bs;
	inst->vsi->dec.bs_dma = (uint64_t)bs->dma_addr;

	for (i = 0; i < num_planes; i++)
		inst->vsi->dec.fb_dma[i] = fb_dma[i];

	if (inst->vsi->input_driven == NON_INPUT_DRIVEN) {
		inst->vsi->dec.vdec_fb_va = (u64)(uintptr_t)NULL;
		inst->vsi->dec.index = 0xFF;
	}
	if (fb != NULL) {
		inst->vsi->dec.vdec_fb_va = vdec_fb_va;
		inst->vsi->dec.index = fb->index;
		if (fb->dma_general_buf != 0) {
			inst->vsi->general_buf_fd = fb->general_buf_fd;
			inst->vsi->general_buf_size = fb->dma_general_buf->size;
			inst->vsi->general_buf_dma = fb->dma_general_addr;
			mtk_vcodec_debug(inst, "dma_general_buf dma_buf=%p fd=%d dma=%llx size=%lu",
			    fb->dma_general_buf, inst->vsi->general_buf_fd,
			    inst->vsi->general_buf_dma,
			    fb->dma_general_buf->size);
		} else {
			fb->general_buf_fd = -1;
			inst->vsi->general_buf_fd = -1;
			inst->vsi->general_buf_size = 0;
			mtk_vcodec_debug(inst, "no general buf dmabuf");
		}
	}

	inst->vsi->dec.queued_frame_buf_count =
		inst->ctx->dec_params.queued_frame_buf_count;
	inst->vsi->dec.timestamp =
		inst->ctx->dec_params.timestamp;

	mtk_vcodec_debug(inst, "+ FB y_fd=%llx c_fd=%llx BS fd=%llx format=%c%c%c%c",
		inst->vsi->dec.fb_fd[0], inst->vsi->dec.fb_fd[1],
		inst->vsi->dec.bs_fd, fm_fourcc & 0xFF,
		(fm_fourcc >> 8) & 0xFF, (fm_fourcc >> 16) & 0xFF,
		(fm_fourcc >> 24) & 0xFF);


	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_START;
	msg.vcu_inst_addr = vcu->inst_addr;
	msg.data[0] = (unsigned int)bs->size;
	msg.data[1] = (unsigned int)bs->length;
	msg.data[2] = (unsigned int)bs->flags;
	ret = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), 0);

	*src_chg = inst->vsi->dec.vdec_changed_info;
	*(errormap_info + bs->index % VB2_MAX_FRAME) =
		inst->vsi->dec.error_map;

	if ((*src_chg & VDEC_NEED_SEQ_HEADER) != 0U)
		mtk_vcodec_err(inst, "- need first seq header -");
	else if ((*src_chg & VDEC_RES_CHANGE) != 0U)
		mtk_vcodec_debug(inst, "- resolution changed -");
	else if ((*src_chg & VDEC_HW_NOT_SUPPORT) != 0U)
		mtk_vcodec_err(inst, "- unsupported -");
	/*ack timeout means vpud has crashed*/
	if (ret != IPI_ACTION_DONE) {
		mtk_vcodec_err(inst, "- IPI msg ack fail %d -", ret);
		*src_chg = *src_chg | VDEC_HW_NOT_SUPPORT;
	}

	if (ret < 0 || ((*src_chg & VDEC_HW_NOT_SUPPORT) != 0U)
		|| ((*src_chg & VDEC_NEED_SEQ_HEADER) != 0U))
		goto err_free_fb_out;

	inst->ctx->input_driven = inst->vsi->input_driven;
	inst->num_nalu++;
	return ret;

err_free_fb_out:
	put_fb_to_free(inst, fb);
	mtk_vcodec_err(inst, "\n - NALU[%d] err=%d -\n", inst->num_nalu, ret);

	return ret;
}

int vdec_vcp_set_frame_buffer(struct vdec_inst *inst, void *fb)
{
	int err = 0;
	struct vdec_ap_ipi_set_param msg;
	struct mtk_video_dec_buf *dst_buf_info = fb;
	struct vdec_ipi_fb ipi_fb;
	struct vdec_fb *pfb = NULL;
	bool dst_not_get = true;

	mtk_vcodec_debug(inst, "+ id=%X", AP_IPIMSG_DEC_FRAME_BUFFER);

	memset(&msg, 0, sizeof(msg));
	memset(&ipi_fb, 0, sizeof(ipi_fb));
	msg.msg_id = AP_IPIMSG_DEC_FRAME_BUFFER;
	msg.id = 0;
	msg.vcu_inst_addr = inst->vcu.inst_addr;

	do {
		if (fb == NULL) {
			mtk_vcodec_debug(inst, "send flush");
		} else {
			pfb = mtk_vcodec_get_fb(inst->ctx);
			if (pfb == &dst_buf_info->frame_buffer) {
				dst_not_get = false;
			}
			ipi_fb.vdec_fb_va = (u64)pfb;
		}

		if (pfb != NULL) {
			ipi_fb.reserved = pfb->index;
			ipi_fb.y_fb_dma = (u64)pfb->fb_base[0].dma_addr;
			if (pfb->num_planes > 1){
				ipi_fb.c_fb_dma = (u64)pfb->fb_base[1].dma_addr;
			}

			if (pfb->dma_general_buf != 0) {
				ipi_fb.dma_general_addr = pfb->dma_general_addr;
				ipi_fb.general_size = pfb->dma_general_buf->size;
				mtk_vcodec_debug(inst, "FB id=%d dma_addr (%llx,%llx) dma_general_buf %p size %lu dma %lu",
					pfb->index, ipi_fb.y_fb_dma, ipi_fb.c_fb_dma,
					pfb->dma_general_buf, pfb->dma_general_buf->size, pfb->dma_general_addr);
			} else {
				ipi_fb.dma_general_addr = -1;
				mtk_vcodec_debug(inst, "FB id=%d dma_addr (%llx,%llx) dma_general_buf %p no general buf dmabuf",
					pfb->index, ipi_fb.y_fb_dma, ipi_fb.c_fb_dma,
					pfb->dma_general_buf);
			}
		}

		if (pfb != NULL || fb == NULL) {
			memcpy(msg.data, &ipi_fb, sizeof(struct vdec_ipi_fb));
			err = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), 0);
		}
	} while (pfb != NULL);

	if (fb != NULL && dst_not_get) {
		mtk_vcodec_debug(inst, "warning: dst_buf_info->frame_buffer id=%d %p %llx not get",
			dst_buf_info->frame_buffer.index,
			&dst_buf_info->frame_buffer,
			(u64)&dst_buf_info->frame_buffer);
	}
	mtk_vcodec_debug(inst, "- id=%X ret=%d", AP_IPIMSG_DEC_FRAME_BUFFER, err);

	return err;
}



static void vdec_get_bs(struct vdec_inst *inst,
						struct ring_bs_list *list,
						struct mtk_vcodec_mem **out_bs)
{
	unsigned long vdec_bs_va;
	struct mtk_vcodec_mem *bs;

	if (list->count == 0) {
		mtk_vcodec_debug(inst, "[BS] there is no bs");
		*out_bs = NULL;
		return;
	}

	vdec_bs_va = (unsigned long)list->vdec_bs_va_list[list->read_idx];
	bs = (struct mtk_vcodec_mem *)vdec_bs_va;

	*out_bs = bs;
	mtk_vcodec_debug(inst, "[BS] get free bs %lx", vdec_bs_va);

	list->read_idx = (list->read_idx == DEC_MAX_BS_NUM - 1) ?
					 0 : list->read_idx + 1;
	list->count--;
}

static void vdec_get_fb(struct vdec_inst *inst,
	struct ring_fb_list *list,
	bool disp_list, struct vdec_fb **out_fb)
{
	unsigned long vdec_fb_va;
	struct vdec_fb *fb;

	if (list->count == 0) {
		mtk_vcodec_debug(inst, "[FB] there is no %s fb",
						 disp_list ? "disp" : "free");
		*out_fb = NULL;
		return;
	}

	vdec_fb_va = (unsigned long)list->fb_list[list->read_idx].vdec_fb_va;
	fb = (struct vdec_fb *)vdec_fb_va;
	if (fb == NULL)
		return;
	fb->timestamp = list->fb_list[list->read_idx].timestamp;

	if (disp_list)
		fb->status |= FB_ST_DISPLAY;
	else {
		fb->status |= FB_ST_FREE;
		if (list->fb_list[list->read_idx].reserved)
			fb->status |= FB_ST_EOS;
	}

	*out_fb = fb;
	mtk_vcodec_debug(inst, "[FB] get %s fb st=%x id=%d ts=%llu %llx gbuf fd %d dma %p",
		disp_list ? "disp" : "free", fb->status, list->read_idx,
		list->fb_list[list->read_idx].timestamp,
		list->fb_list[list->read_idx].vdec_fb_va,
		fb->general_buf_fd, fb->dma_general_buf);

	list->read_idx = (list->read_idx == DEC_MAX_FB_NUM - 1U) ?
					 0U : list->read_idx + 1U;
	list->count--;
}


static int vdec_vcp_set_param(unsigned long h_vdec,
	enum vdec_set_param_type type, void *in)
{
	struct vdec_ap_ipi_set_param msg;
	int ret = 0;
	struct vdec_inst *inst = (struct vdec_inst *)h_vdec;
	uint64_t size;
	unsigned long *param_ptr = (unsigned long *)in;

	if (inst == NULL)
		return -EINVAL;

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_SET_PARAM;
	msg.id = type;
	msg.vcu_inst_addr = inst->vcu.inst_addr;

	switch (type) {
	case SET_PARAM_FRAME_BUFFER:
		vdec_vcp_set_frame_buffer(inst, in);
		break;
	case SET_PARAM_FRAME_SIZE:
	case SET_PARAM_SET_FIXED_MAX_OUTPUT_BUFFER:
		msg.data[0] = (__u32)(*param_ptr);
		msg.data[1] = (__u32)(*param_ptr + 1);
		vdec_vcp_ipi_send(inst, &msg, sizeof(msg), 0);
		break;
	case SET_PARAM_DECODE_MODE:
	case SET_PARAM_NAL_SIZE_LENGTH:
	case SET_PARAM_WAIT_KEY_FRAME:
	case SET_PARAM_OPERATING_RATE:
	case SET_PARAM_TOTAL_FRAME_BUFQ_COUNT:
		msg.data[0] = (__u32)(*param_ptr);
		vdec_vcp_ipi_send(inst, &msg, sizeof(msg), 0);
		break;
	case SET_PARAM_UFO_MODE:
		break;
	case SET_PARAM_CRC_PATH:
		if (inst->vsi == NULL)
			return -EINVAL;
		size = strlen((char *) *(uintptr_t *)in);
		memcpy(inst->vsi->crc_path, (void *) *(uintptr_t *)in, size);
		break;
	case SET_PARAM_GOLDEN_PATH:
		if (inst->vsi == NULL)
			return -EINVAL;
		size = strlen((char *) *(uintptr_t *)in);
		memcpy(inst->vsi->golden_path, (void *) *(uintptr_t *)in, size);
		break;
	case SET_PARAM_FB_NUM_PLANES:
		if (inst->vsi == NULL)
			return -EINVAL;
		inst->vsi->dec.fb_num_planes = *(unsigned int *)in;
		break;
	default:
		mtk_vcodec_err(inst, "invalid set parameter type=%d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}


// TODO: VSI touch common code shared with vcu
static void get_supported_format(struct vdec_inst *inst,
	struct mtk_video_fmt *video_fmt)
{
	unsigned int i = 0;
	struct vdec_ap_ipi_query_cap msg;

	inst->vcu.ctx = inst->ctx;
	inst->vcu.id = (inst->vcu.id == IPI_VCU_INIT) ? IPI_VDEC_COMMON : inst->vcu.id;
	init_waitqueue_head(&inst->vcu.wq);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_QUERY_CAP;
	msg.id = GET_PARAM_VDEC_CAP_SUPPORTED_FORMATS;
	msg.ap_inst_addr = (uintptr_t)&inst->vcu;
	msg.ap_data_addr = (uintptr_t)video_fmt;
	vdec_vcp_ipi_send(inst, &msg, sizeof(msg), 0);

	for (i = 0; i < MTK_MAX_DEC_CODECS_SUPPORT; i++) {
		if (video_fmt[i].fourcc != 0) {
			mtk_vcodec_debug(inst, "video_formats[%d] fourcc %d type %d num_planes %d\n",
				i, video_fmt[i].fourcc, video_fmt[i].type,
				video_fmt[i].num_planes);
		}
	}
}

static void get_frame_sizes(struct vdec_inst *inst,
	struct mtk_codec_framesizes *codec_framesizes)
{
	unsigned int i = 0;
	struct vdec_ap_ipi_query_cap msg;

	inst->vcu.ctx = inst->ctx;
	inst->vcu.id = (inst->vcu.id == IPI_VCU_INIT) ? IPI_VDEC_COMMON : inst->vcu.id;
	init_waitqueue_head(&inst->vcu.wq);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_QUERY_CAP;
	msg.id = GET_PARAM_VDEC_CAP_FRAME_SIZES;
	msg.ap_inst_addr = (uintptr_t)&inst->vcu;
	msg.ap_data_addr = (uintptr_t)codec_framesizes;
	vdec_vcp_ipi_send(inst, &msg, sizeof(msg), 0);

	for (i = 0; i < MTK_MAX_DEC_CODECS_SUPPORT; i++) {
		if (codec_framesizes[i].fourcc != 0) {
			mtk_vcodec_debug(inst,
				"codec_fs[%d] fourcc %d s %d %d %d %d %d %d P %d L %d\n",
				i, codec_framesizes[i].fourcc,
				codec_framesizes[i].stepwise.min_width,
				codec_framesizes[i].stepwise.max_width,
				codec_framesizes[i].stepwise.step_width,
				codec_framesizes[i].stepwise.min_height,
				codec_framesizes[i].stepwise.max_height,
				codec_framesizes[i].stepwise.step_height,
				codec_framesizes[i].profile,
				codec_framesizes[i].level);
		}
	}

}

static void get_color_desc(struct vdec_inst *inst,
	struct mtk_color_desc *color_desc)
{
	inst->vcu.ctx = inst->ctx;
	memcpy(color_desc, &inst->vsi->color_desc, sizeof(*color_desc));
}

static void get_aspect_ratio(struct vdec_inst *inst, unsigned int *aspect_ratio)
{
	if (inst->vsi == NULL)
		return;

	inst->vcu.ctx = inst->ctx;
	*aspect_ratio = inst->vsi->aspect_ratio;
}

static void get_supported_fix_buffers(struct vdec_inst *inst,
					unsigned int *supported)
{
	inst->vcu.ctx = inst->ctx;
	if (inst->vsi != NULL)
		*supported = inst->vsi->fix_buffers;
}

static void get_supported_fix_buffers_svp(struct vdec_inst *inst,
					unsigned int *supported)
{
	inst->vcu.ctx = inst->ctx;
	if (inst->vsi != NULL)
		*supported = inst->vsi->fix_buffers_svp;
}

static void get_interlacing(struct vdec_inst *inst,
			    unsigned int *interlacing)
{
	inst->vcu.ctx = inst->ctx;
	if (inst->vsi != NULL)
		*interlacing = inst->vsi->interlacing;
}

static void get_codec_type(struct vdec_inst *inst,
			   unsigned int *codec_type)
{
	inst->vcu.ctx = inst->ctx;
	if (inst->vsi != NULL)
		*codec_type = inst->vsi->codec_type;
}

static void get_input_driven(struct vdec_inst *inst,
			   unsigned int *input_driven)
{
	inst->vcu.ctx = inst->ctx;
	if (inst->vsi != NULL)
		*input_driven = inst->vsi->input_driven;
}


static int vdec_vcp_get_param(unsigned long h_vdec,
	enum vdec_get_param_type type, void *out)
{
	int ret = 0;
	struct vdec_inst *inst = (struct vdec_inst *)h_vdec;

	if (inst == NULL)
		return -EINVAL;

	switch (type) {
	case GET_PARAM_FREE_BITSTREAM_BUFFER:
		if (inst->vsi == NULL)
			return -EINVAL;
		vdec_get_bs(inst, &inst->vsi->list_free_bs, out);
		break;

	case GET_PARAM_DISP_FRAME_BUFFER:
	{
		struct vdec_fb *pfb;
		if (inst->vsi == NULL)
			return -EINVAL;
		vdec_get_fb(inst, &inst->vsi->list_disp, true, out);

		pfb = *((struct vdec_fb **)out);
		if (pfb != NULL) {
			if (pfb->general_buf_fd >= 0) {
				mtk_vcodec_debug(inst, "free pfb->general_buf_fd:%d pfb->dma_general_buf %p\n",
					pfb->general_buf_fd,
					pfb->dma_general_buf);
				pfb->general_buf_fd = -1;
			}
		}
		break;
	}

	case GET_PARAM_FREE_FRAME_BUFFER:
	{
		struct vdec_fb *pfb;
		int i;

		if (inst->vsi == NULL)
			return -EINVAL;
		vdec_get_fb(inst, &inst->vsi->list_free, false, out);

		pfb = *((struct vdec_fb **)out);
		if (pfb != NULL) {
			for (i = 0; i < pfb->num_planes; i++) {
				if (pfb->fb_base[i].buf_fd >= 0) {
					mtk_vcodec_debug(inst, "free pfb->fb_base[%d].buf_fd:%llx\n",
						i, pfb->fb_base[i].buf_fd);
					pfb->fb_base[i].buf_fd = -1;
				}
			}
			if (pfb->general_buf_fd >= 0) {
				mtk_vcodec_debug(inst, "free pfb->general_buf_fd:%d pfb->dma_general_buf %p\n",
					pfb->general_buf_fd,
					pfb->dma_general_buf);
				pfb->general_buf_fd = -1;
			}
		}
		break;
	}

	case GET_PARAM_PIC_INFO:
		get_pic_info(inst, out);
		break;

	case GET_PARAM_DPB_SIZE:
		get_dpb_size(inst, out);
		break;

	case GET_PARAM_CROP_INFO:
		get_crop_info(inst, out);
		break;

	case GET_PARAM_VDEC_CAP_SUPPORTED_FORMATS:
		get_supported_format(inst, out);
		break;

	case GET_PARAM_VDEC_CAP_FRAME_SIZES:
		get_frame_sizes(inst, out);
		break;

	case GET_PARAM_COLOR_DESC:
		if (inst->vsi == NULL)
			return -EINVAL;
		get_color_desc(inst, out);
		break;

	case GET_PARAM_ASPECT_RATIO:
		get_aspect_ratio(inst, out);
		break;

	case GET_PARAM_PLATFORM_SUPPORTED_FIX_BUFFERS:
		get_supported_fix_buffers(inst, out);
		break;

	case GET_PARAM_PLATFORM_SUPPORTED_FIX_BUFFERS_SVP:
		get_supported_fix_buffers_svp(inst, out);
		break;

	case GET_PARAM_INTERLACING:
		get_interlacing(inst, out);
		break;

	case GET_PARAM_CODEC_TYPE:
		get_codec_type(inst, out);
		break;

	case GET_PARAM_INPUT_DRIVEN:
		get_input_driven(inst, out);
		break;

	default:
		mtk_vcodec_err(inst, "invalid get parameter type=%d", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct vdec_common_if vdec_vcp_if = {
	vdec_vcp_init,
	vdec_vcp_decode,
	vdec_vcp_get_param,
	vdec_vcp_set_param,
	vdec_vcp_deinit,
};

struct vdec_common_if *get_dec_vcp_if(void)
{
	return &vdec_vcp_if;
}
