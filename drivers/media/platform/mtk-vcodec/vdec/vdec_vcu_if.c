// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 */

#include <linux/fdtable.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <media/v4l2-mem2mem.h>
#include <linux/mtk_vcu_controls.h>
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "vdec_ipi_msg.h"
#include "vdec_vcu_if.h"
#include "vdec_drv_if.h"
//#include "smi_public.h"
#define VIDEO_USE_IOVA

static void handle_init_ack_msg(struct vdec_vcu_ipi_init_ack *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)
		(unsigned long)msg->ap_inst_addr;

	if (vcu == NULL)
		return;
	mtk_vcodec_debug(vcu, "+ ap_inst_addr = 0x%lx",
		(uintptr_t)msg->ap_inst_addr);

	/* mapping VCU address to kernel virtual address */
	/* the content in vsi is initialized to 0 in VCU */
	vcu->vsi = VCU_FPTR(vcu_mapping_dm_addr)(vcu->dev, msg->vcu_inst_addr);
	vcu->inst_addr = msg->vcu_inst_addr;
	mtk_vcodec_debug(vcu, "- vcu_inst_addr = 0x%x", vcu->inst_addr);
}

static void handle_query_cap_ack_msg(struct vdec_vcu_ipi_query_cap_ack *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)msg->ap_inst_addr;
	void *data;
	int size = 0;

	if (vcu == NULL)
		return;
	mtk_vcodec_debug(vcu, "+ ap_inst_addr = 0x%lx, vcu_data_addr = 0x%x, id = %d",
		(uintptr_t)msg->ap_inst_addr, msg->vcu_data_addr, msg->id);
	/* mapping VCU address to kernel virtual address */
	data = VCU_FPTR(vcu_mapping_dm_addr)(vcu->dev, msg->vcu_data_addr);
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

static int check_codec_id(struct vdec_vcu_ipi_ack *msg, unsigned int fmt, unsigned int svp)
{
	int codec_id = 0, ret = 0;

	switch (fmt) {
	case V4L2_PIX_FMT_H264:
		codec_id = VDEC_H264;
		break;
	case V4L2_PIX_FMT_H265:
		codec_id = VDEC_H265;
		break;
	case V4L2_PIX_FMT_HEIF:
		codec_id = VDEC_HEIF;
		break;
	case V4L2_PIX_FMT_VP8:
		codec_id = VDEC_VP8;
		break;
	case V4L2_PIX_FMT_VP9:
		codec_id = VDEC_VP9;
		break;
	case V4L2_PIX_FMT_MPEG4:
		codec_id = VDEC_MPEG4;
		break;
	case V4L2_PIX_FMT_H263:
		codec_id = VDEC_H263;
		break;
	case V4L2_PIX_FMT_MPEG1:
	case V4L2_PIX_FMT_MPEG2:
		codec_id = VDEC_MPEG12;
		break;
	case V4L2_PIX_FMT_WMV1:
	case V4L2_PIX_FMT_WMV2:
	case V4L2_PIX_FMT_WMV3:
	case V4L2_PIX_FMT_WMVA:
	case V4L2_PIX_FMT_WVC1:
		codec_id = VDEC_WMV;
		break;
	case V4L2_PIX_FMT_RV30:
		codec_id = VDEC_RV30;
		break;
	case V4L2_PIX_FMT_RV40:
		codec_id = VDEC_RV40;
		break;
	case V4L2_PIX_FMT_AV1:
		codec_id = VDEC_AV1;
		break;
	default:
		pr_info("%s no fourcc", __func__);
		break;
	}

	if (codec_id == 0) {
		pr_info("[error] vdec unsupported fourcc\n");
		ret = -1;
	} else if (msg->codec_id == codec_id && msg->status == svp) {
		pr_info("%s ipi id %d svp %d is correct\n", __func__, msg->codec_id, msg->status);
		ret = 0;
	} else {
		mtk_v4l2_debug(2, "[Info] ipi id %d svp %d is incorrect\n",
			msg->codec_id, msg->status);
		ret = -1;
	}

	return ret;
}

/*
 * This function runs in interrupt context and it means there's a IPI MSG
 * from VCU.
 */
int vcu_dec_ipi_handler(void *data, unsigned int len, void *priv)
{
	struct vdec_vcu_ipi_ack *msg = data;
	struct vdec_vcu_inst *vcu = NULL;
	struct vdec_fb *pfb;
	struct timespec64 t_s, t_e;
	struct task_struct *task = NULL;
	struct vdec_vsi *vsi;
	uint64_t vdec_fb_va;
	long timeout_jiff;
	int ret = 0;
	int i = 0;
	struct list_head *p, *q;
	struct mtk_vcodec_ctx *temp_ctx;
	struct mtk_vcodec_dev *dev = (struct mtk_vcodec_dev *)priv;
	struct vdec_inst *inst = NULL;
	int msg_valid = 0;

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

	VCU_FPTR(vcu_get_task)(&task, 0);
	if (msg == NULL || task == NULL ||
	   task->tgid != current->tgid ||
	   (struct vdec_vcu_inst *)msg->ap_inst_addr == NULL) {
		ret = -EINVAL;
		return ret;
	}

	vcu = (struct vdec_vcu_inst *)(unsigned long)msg->ap_inst_addr;
	/* Check IPI inst is valid */
	mutex_lock(&dev->ctx_mutex);
	list_for_each_safe(p, q, &dev->ctx_list) {
		temp_ctx = list_entry(p, struct mtk_vcodec_ctx, list);
		inst = (struct vdec_inst *)temp_ctx->drv_handle;
		if (vcu == &inst->vcu) {
			msg_valid = 1;
			break;
		}
	}
	if (!msg_valid) {
		mtk_v4l2_err(" msg vcu not exist %p\n", vcu);
		mutex_unlock(&dev->ctx_mutex);
		return -EINVAL;
	}
	mutex_unlock(&dev->ctx_mutex);

	if (vcu->daemon_pid != current->tgid) {
		pr_info("%s, vcu->daemon_pid:%d != current %d\n",
			__func__, vcu->daemon_pid, current->tgid);
		return 1;
	}

	vsi = (struct vdec_vsi *)vcu->vsi;
	mtk_vcodec_debug(vcu, "+ id=%X status = %d vcu:%p\n",
		msg->msg_id, msg->status, vcu);

	if (vcu->abort) {
		return -EINVAL;
	}

	if (msg->msg_id == VCU_IPIMSG_DEC_CHECK_CODEC_ID) {

		if (check_codec_id(msg, vcu->ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc,
				vcu->ctx->dec_params.svp_mode) == 0)
			msg->status = 0;
		else
			msg->status = -1;
		ret = 1;
	} else if (msg->msg_id == VCU_IPIMSG_DEC_WAITISR) {
		if (msg->status == MTK_VDEC_CORE)
			vcodec_trace_count("VDEC_HW_CORE", 2);
		else
			vcodec_trace_count("VDEC_HW_LAT", 2);

		/* wait decoder done interrupt */
		mtk_vdec_do_gettimeofday(&t_s);
		ret = mtk_vcodec_wait_for_done_ctx(vcu->ctx,
			msg->status,
			MTK_INST_IRQ_RECEIVED,
			WAIT_INTR_TIMEOUT_MS);
		mtk_vdec_do_gettimeofday(&t_e);

		if (msg->status == MTK_VDEC_CORE)
			vcodec_trace_count("VDEC_HW_CORE", 1);
		else
			vcodec_trace_count("VDEC_HW_LAT", 1);

		mtk_vcodec_perf_log("irq:%ld",
			(t_e.tv_sec - t_s.tv_sec) * 1000000LL +
			(t_e.tv_nsec - t_s.tv_nsec) / 1000);
		if (ret == -1 && msg->status == MTK_VDEC_CORE) {
			/* dump smi when vdec core timeout */
			//smi_debug_bus_hang_detect(0, "VCODEC");
		}
		msg->status = ret;
		ret = 1;
	} else if (msg->status == 0) {
		switch (msg->msg_id) {
		case VCU_IPIMSG_DEC_INIT_DONE:
			handle_init_ack_msg(data);
			break;
		case VCU_IPIMSG_DEC_QUERY_CAP_DONE:
			handle_query_cap_ack_msg(data);
			break;
		case VCU_IPIMSG_DEC_START_DONE:
		case VCU_IPIMSG_DEC_DONE:
		case VCU_IPIMSG_DEC_DEINIT_DONE:
		case VCU_IPIMSG_DEC_RESET_DONE:
		case VCU_IPIMSG_DEC_SET_PARAM_DONE:
			break;
		case VCU_IPIMSG_DEC_LOCK_LAT:
			vdec_decode_prepare(vcu->ctx, MTK_VDEC_LAT);
			ret = 1;
			break;
		case VCU_IPIMSG_DEC_UNLOCK_LAT:
			vdec_decode_unprepare(vcu->ctx, MTK_VDEC_LAT);
			ret = 1;
			break;
		case VCU_IPIMSG_DEC_LOCK_CORE:
			if (vcu->ctx->user_lock_hw)
				vdec_decode_prepare(vcu->ctx, MTK_VDEC_CORE);
			ret = 1;
			break;
		case VCU_IPIMSG_DEC_UNLOCK_CORE:
			if (vcu->ctx->user_lock_hw)
				vdec_decode_unprepare(vcu->ctx, MTK_VDEC_CORE);
			ret = 1;
			break;
		case VCU_IPIMSG_DEC_GET_FRAME_BUFFER:
			if (vsi->input_driven != INPUT_DRIVEN_CB_FRM) {
				mtk_vcodec_err(vcu, "Cannot use IPIMSG_DEC_GET_FRAME_BUFFER %d\n",
					vsi->input_driven);
				break;
			}
			mtk_vcodec_debug(vcu, "+ try get fm form rdy q size=%d\n",
				v4l2_m2m_num_dst_bufs_ready(vcu->ctx->m2m_ctx));

			pfb = mtk_vcodec_get_fb(vcu->ctx);
			timeout_jiff = msecs_to_jiffies(1000);
			/* 1s timeout */
			while (pfb == NULL) {
				ret = wait_event_interruptible_timeout(
					vcu->ctx->fm_wq,
					 v4l2_m2m_num_dst_bufs_ready(
						 vcu->ctx->m2m_ctx) > 0 ||
					 vcu->ctx->state == MTK_STATE_FLUSH,
					 timeout_jiff);
				pfb = mtk_vcodec_get_fb(vcu->ctx);
				if (vcu->ctx->state == MTK_STATE_FLUSH)
					mtk_vcodec_debug(vcu, "get fm fail: state == FLUSH (pfb=0x%p)\n",
						pfb);
				else if (ret == 0)
					mtk_vcodec_debug(vcu, "get fm fail: timeout (pfb=0x%p)\n",
						pfb);
				else if (pfb == NULL)
					mtk_vcodec_debug(vcu, "get fm fail: unknown (ret = %d)\n",
						ret);

				if (vcu->ctx->state == MTK_STATE_FLUSH ||
					ret != 0)
					break;
			}
			mtk_vcodec_debug(vcu, "- wait get fm pfb=0x%p\n", pfb);

			vdec_fb_va = (u64)pfb;
			vsi->dec.vdec_fb_va = vdec_fb_va;
			if (pfb != NULL) {
				vsi->dec.index = pfb->index;
				for (i = 0; i < pfb->num_planes; i++) {
					vsi->dec.fb_dma[i] = (u64)
						pfb->fb_base[i].dma_addr;
					mtk_vcodec_debug(vcu, "+ vsi->dec.fb_fd[%d]:%llx\n",
						i, vsi->dec.fb_fd[i]);
				}
				if (pfb->dma_general_buf != 0) {
					vsi->general_buf_dma =
						pfb->dma_general_addr;
					vsi->general_buf_fd =
						pfb->general_buf_fd;
					vsi->general_buf_size =
						pfb->dma_general_buf->size;
					mtk_vcodec_debug(vcu, "fb->dma_general_buf = %p, mapped fd = %d, size = %lu",
						pfb->dma_general_buf,
						vsi->general_buf_fd,
						pfb->dma_general_buf->size);
				} else {
					pfb->general_buf_fd = -1;
					vsi->general_buf_fd = -1;
					mtk_vcodec_debug(vcu, "no general buf dmabuf");
				}
			} else {
				vsi->dec.index = 0xFF;
				mtk_vcodec_err(vcu, "Cannot get frame buffer from V4l2\n");
			}
			mtk_vcodec_debug(vcu, "+ FB y_fd=%llx c_fd=%llx BS fd=0x%llx index:%d vdec_fb_va:%llx,dma_addr(%llx,%llx)",
				vsi->dec.fb_fd[0], vsi->dec.fb_fd[1],
				vsi->dec.bs_fd, vsi->dec.index,
				vsi->dec.vdec_fb_va,
				vsi->dec.fb_dma[0], vsi->dec.fb_dma[1]);

#if 0
			if (vcu->ctx->dec_params.svp_mode == 0 &&
				vcu->ctx->dev->dec_irq != 0) {
				ret = devm_request_irq(
					&vcu->ctx->dev->plat_dev->dev,
					vcu->ctx->dev->dec_irq,
					mtk_vcodec_dec_irq_handler, 0,
					vcu->ctx->dev->plat_dev->name,
					vcu->ctx->dev);
				vcu->ctx->dev->reqst_irq = true;
				mtk_vcodec_debug(vcu, "Requset irq:%d ok,rqst_irq:%d",
					vcu->ctx->dev->dec_irq,
					vcu->ctx->dev->reqst_irq);
				if (ret) {
					mtk_vcodec_err(vcu, "Failed to install dev->dec_irq %d (%d)",
					vcu->ctx->dev->dec_irq,
					ret);
					return -EINVAL;
				}
			}
#endif
			ret = 1;
			break;
		case VCU_IPIMSG_DEC_PUT_FRAME_BUFFER:
			mtk_vdec_put_fb(vcu->ctx, PUT_BUFFER_CALLBACK, msg->reserved != 0);
			ret = 1;
			break;
		default:
			mtk_vcodec_err(vcu, "invalid msg=%X", msg->msg_id);
			ret = 1;
			break;
		}
	}

	mtk_vcodec_debug(vcu, "- id=%X", msg->msg_id);

	/* deinit ack timeout case handling do not touch vdec_vcu_inst
	 * or memory used after freed
	 */
	if (msg->msg_id != VCU_IPIMSG_DEC_DEINIT_DONE) {
		vcu->signaled = 1;
		vcu->failure = msg->status;
	}

	return ret;
}

static int vcodec_vcu_send_msg(struct vdec_vcu_inst *vcu, void *msg, int len)
{
	int err;
	struct task_struct *task = NULL;
	unsigned int suspend_block_cnt = 0;

	mtk_vcodec_debug(vcu, "id=%X", *(uint32_t *)msg);
	if (vcu->abort)
		return -EIO;

	while (vcu->ctx->dev->is_codec_suspending == 1) {
		suspend_block_cnt++;
		if (suspend_block_cnt > SUSPEND_TIMEOUT_CNT) {
			mtk_v4l2_debug(4, "VDEC blocked by suspend\n");
			suspend_block_cnt = 0;
		}
		usleep_range(10000, 20000);
	}

	VCU_FPTR(vcu_get_task)(&task, 0);
	if (task == NULL ||
		vcu->daemon_pid != task->tgid) {
		if (task)
			mtk_vcodec_err(vcu, "send fail pid: inst %d curr %d",
				vcu->daemon_pid, task->tgid);
		vcu->abort = 1;
		return -EIO;
	}

	vcu->failure = 0;
	vcu->signaled = 0;

	if (*(__u32 *)msg == AP_IPIMSG_DEC_FRAME_BUFFER)
		err = VCU_FPTR(vcu_ipi_send)(vcu->dev, IPI_VDEC_RESOURCE, msg, len, vcu->ctx->dev);
	else
		err = VCU_FPTR(vcu_ipi_send)(vcu->dev, vcu->id, msg, len, vcu->ctx->dev);

	if (err) {
		mtk_vcodec_err(vcu, "send fail vcu_id=%d msg_id=%X status=%d",
					   vcu->id, *(uint32_t *)msg, err);
		if (err == -EIO)
			vcu->abort = 1;
		return err;
	}
	mtk_vcodec_debug(vcu, "- ret=%d", err);
	return vcu->failure;
}

static int vcodec_send_ap_ipi(struct vdec_vcu_inst *vcu, unsigned int msg_id)
{
	struct vdec_ap_ipi_cmd msg;
	int err = 0;

	mtk_vcodec_debug(vcu, "+ id=%X", msg_id);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = msg_id;
	msg.ctx_id = vcu->ctx->id;
	msg.vcu_inst_addr = vcu->inst_addr;

	err = vcodec_vcu_send_msg(vcu, &msg, sizeof(msg));
	mtk_vcodec_debug(vcu, "- id=%X ret=%d", msg_id, err);
	return err;
}

void vcu_dec_set_pid(struct vdec_vcu_inst *vcu)
{
	struct task_struct *task = NULL;

	VCU_FPTR(vcu_get_task)(&task, 0);
	if (task != NULL)
		vcu->daemon_pid = task->tgid;
	else
		vcu->daemon_pid = -1;
}

int vcu_dec_set_ctx(struct vdec_vcu_inst *vcu,
	struct mtk_vcodec_mem *bs,
	struct vdec_fb *fb)
{
	int err = 0;
	struct vb2_buffer *src_vb = NULL;
	struct vb2_buffer *dst_vb = NULL;
	struct mtk_video_dec_buf *temp;

	if (bs != NULL) {
		temp = container_of(bs, struct mtk_video_dec_buf, bs_buffer);
		src_vb = &temp->vb.vb2_buf;
	}
	if (fb != NULL) {
		temp = container_of(fb, struct mtk_video_dec_buf, frame_buffer);
		dst_vb = &temp->vb.vb2_buf;
	}
	VCU_FPTR(vcu_set_codec_ctx)(vcu->dev,
		(void *)vcu->ctx,
		src_vb, dst_vb,
		VCU_VDEC);

	return err;
}

int vcu_dec_clear_ctx(struct vdec_vcu_inst *vcu)
{
	int err = 0;

	VCU_FPTR(vcu_clear_codec_ctx)(vcu->dev,
		(void *)vcu->ctx, VCU_VDEC);

	return err;
}

int vcu_dec_init(struct vdec_vcu_inst *vcu)
{
	struct vdec_ap_ipi_init msg;
	int err;

	mtk_vcodec_debug_enter(vcu);
	vcu->signaled = 0;
	vcu->failure = 0;
	VCU_FPTR(vcu_get_ctx_ipi_binding_lock)(vcu->dev, &vcu->ctx_ipi_lock, VCU_VDEC);

	err = VCU_FPTR(vcu_ipi_register)(vcu->dev, vcu->id, vcu->handler, NULL, vcu->ctx->dev);
	if (err != 0) {
		mtk_vcodec_err(vcu, "vcu_ipi_register %d fail status=%d", vcu->id, err);
		return err;
	}

	err = VCU_FPTR(vcu_ipi_register)(vcu->dev, IPI_VDEC_RESOURCE,
		vcu->handler, NULL, vcu->ctx->dev);
	if (err != 0) {
		mtk_vcodec_err(vcu, "vcu_ipi_register resource fail status=%d", err);
		return err;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_INIT;
	msg.ctx_id = vcu->ctx->id;
	msg.ap_inst_addr = (unsigned long)vcu;

	mtk_vcodec_debug(vcu, "vdec_inst=%p svp_mode=%d", vcu, vcu->ctx->dec_params.svp_mode);

	vcu_dec_set_pid(vcu);

	err = vcodec_vcu_send_msg(vcu, (void *)&msg, sizeof(msg));

	mtk_vcodec_debug(vcu, "- ret=%d", err);
	return err;
}

int vcu_dec_start(struct vdec_vcu_inst *vcu,
	unsigned int *data, unsigned int len,
	struct mtk_vcodec_mem *bs, struct vdec_fb *fb)
{
	struct vdec_ap_ipi_dec_start msg;
	int i;
	int err = 0;

	mtk_vcodec_debug_enter(vcu);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_START;
	msg.ctx_id = vcu->ctx->id;
	msg.vcu_inst_addr = vcu->inst_addr;

	for (i = 0; i < len; i++)
		msg.data[i] = data[i];

	mutex_lock(vcu->ctx_ipi_lock);
	vcu_dec_set_ctx(vcu, bs, fb);
	err = vcodec_vcu_send_msg(vcu, (void *)&msg, sizeof(msg));
	mutex_unlock(vcu->ctx_ipi_lock);

	mtk_vcodec_debug(vcu, "- ret=%d", err);
	return err;
}

int vcu_dec_end(struct vdec_vcu_inst *vcu)
{
	return vcodec_send_ap_ipi(vcu, AP_IPIMSG_DEC_END);
}

int vcu_dec_deinit(struct vdec_vcu_inst *vcu)
{
	int err = 0;

	mutex_lock(vcu->ctx_ipi_lock);
	err = vcodec_send_ap_ipi(vcu, AP_IPIMSG_DEC_DEINIT);
	vcu_dec_clear_ctx(vcu);
	mutex_unlock(vcu->ctx_ipi_lock);

	return err;
}

int vcu_dec_reset(struct vdec_vcu_inst *vcu, enum vdec_reset_type drain_type)
{
	struct vdec_ap_ipi_cmd msg;
	int err = 0;

	mtk_vcodec_debug_enter(vcu);
	mtk_vcodec_debug(vcu, "drain_type %d", drain_type);
	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_RESET;
	msg.ctx_id = vcu->ctx->id;
	msg.vcu_inst_addr = vcu->inst_addr;
	msg.reserved = drain_type;

	err = vcodec_vcu_send_msg(vcu, (void *)&msg, sizeof(msg));
	mtk_vcodec_debug(vcu, "- ret=%d", err);

	return err;
}

int vcu_dec_query_cap(struct vdec_vcu_inst *vcu, unsigned int id, void *out)
{
	struct vdec_ap_ipi_query_cap msg;
	int err = 0;

	mtk_vcodec_debug(vcu, "+ id=%X", AP_IPIMSG_DEC_QUERY_CAP);
	vcu->dev = VCU_FPTR(vcu_get_plat_device)(vcu->ctx->dev->plat_dev);
	if (vcu->dev  == NULL) {
		mtk_vcodec_err(vcu, "vcu device in not ready");
		return -EPROBE_DEFER;
	}

	vcu->id = (vcu->id == IPI_VCU_INIT) ? IPI_VDEC_COMMON : vcu->id;
	vcu->handler = vcu_dec_ipi_handler;

	err = VCU_FPTR(vcu_ipi_register)(vcu->dev, vcu->id, vcu->handler, NULL, vcu->ctx->dev);
	if (err != 0) {
		mtk_vcodec_err(vcu, "vcu_ipi_register fail status=%d", err);
		return err;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_QUERY_CAP;
	msg.id = id;
	msg.ctx_id = vcu->ctx->id;
	msg.ap_inst_addr = (uintptr_t)vcu;
	msg.ap_data_addr = (uintptr_t)out;

	vcu_dec_set_pid(vcu);
	err = vcodec_vcu_send_msg(vcu, &msg, sizeof(msg));
	mtk_vcodec_debug(vcu, "- id=%X ret=%d", msg.msg_id, err);
	return err;
}

int vcu_dec_set_param(struct vdec_vcu_inst *vcu, unsigned int id, void *param,
					  unsigned int size)
{
	struct vdec_ap_ipi_set_param msg;
	unsigned long *param_ptr = (unsigned long *)param;
	int err = 0;
	int i = 0;

	mtk_vcodec_debug(vcu, "+ id=%X", AP_IPIMSG_DEC_SET_PARAM);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_SET_PARAM;
	msg.id = id;
	msg.ctx_id = vcu->ctx->id;
	msg.vcu_inst_addr = vcu->inst_addr;
	for (i = 0; i < size; i++) {
		msg.data[i] = (__u32)(*(param_ptr + i));
		mtk_vcodec_debug(vcu, "msg.id = 0x%X, msg.data[%d]=%d",
			msg.id, i, msg.data[i]);
	}

	err = vcodec_vcu_send_msg(vcu, &msg, sizeof(msg));
	mtk_vcodec_debug(vcu, "- id=%X ret=%d", AP_IPIMSG_DEC_SET_PARAM, err);

	return err;
}

int vcu_dec_set_frame_buffer(struct vdec_vcu_inst *vcu, void *fb)
{
	int err = 0;
	struct vdec_ap_ipi_set_param msg;
	struct mtk_video_dec_buf *dst_buf_info = fb;
	struct vdec_ipi_fb ipi_fb;
	struct vdec_fb *pfb = NULL;
	bool dst_not_get = true;

	mtk_vcodec_debug(vcu, "+ id=%X", AP_IPIMSG_DEC_FRAME_BUFFER);

	memset(&msg, 0, sizeof(msg));
	memset(&ipi_fb, 0, sizeof(ipi_fb));
	msg.msg_id = AP_IPIMSG_DEC_FRAME_BUFFER;
	msg.id = 0;
	msg.ctx_id = vcu->ctx->id;
	msg.vcu_inst_addr = vcu->inst_addr;

	do {
		if (fb == NULL) {
			mtk_vcodec_debug(vcu, "send flush");
		} else {
			pfb = mtk_vcodec_get_fb(vcu->ctx);
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
				mtk_vcodec_debug(vcu, "FB id=%d dma_addr (%llx,%llx) dma_general_buf %p size %lu dma %lu",
					pfb->index, ipi_fb.y_fb_dma, ipi_fb.c_fb_dma,
					pfb->dma_general_buf, pfb->dma_general_buf->size, pfb->dma_general_addr);
			} else {
				ipi_fb.dma_general_addr = -1;
				mtk_vcodec_debug(vcu, "FB id=%d dma_addr (%llx,%llx) dma_general_buf %p no general buf dmabuf",
					pfb->index, ipi_fb.y_fb_dma, ipi_fb.c_fb_dma,
					pfb->dma_general_buf);
			}
		}

		if (pfb != NULL || fb == NULL) {
			memcpy(msg.data, &ipi_fb, sizeof(struct vdec_ipi_fb));
			err = vcodec_vcu_send_msg(vcu, &msg, sizeof(msg));
		}
	} while (pfb != NULL);

	if (fb != NULL && dst_not_get) {
		mtk_vcodec_debug(vcu, "warning: dst_buf_info->frame_buffer id=%d %p %llx not get",
			dst_buf_info->frame_buffer.index,
			&dst_buf_info->frame_buffer,
			(u64)&dst_buf_info->frame_buffer);
	}

	mtk_vcodec_debug(vcu, "- id=%X ret=%d", AP_IPIMSG_DEC_FRAME_BUFFER, err);

	return err;
}


