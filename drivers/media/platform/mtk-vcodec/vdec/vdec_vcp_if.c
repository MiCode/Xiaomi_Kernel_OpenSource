/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2016 MediaTek Inc.
* Author: Chia-Mao Hung<chia-mao.hung@mediatek.com>
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/mtk_vcu_controls.h>
#include <linux/delay.h>
#include <soc/mediatek/smi.h>

#include "vdec_drv_base.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_drv.h"
#include "vdec_drv_if.h"
#include "vcp_ipi_pin.h"
#include "vcp_mbox_layout.h"  /* for IPI mbox size */
#include "vcp_helper.h"
// TODO: need remove ISR ipis
#include "mtk_vcodec_intr.h"

#ifdef CONFIG_MTK_ENG_BUILD
#define IPI_TIMEOUT_MS          (2000U)
#else
#define IPI_TIMEOUT_MS          (1000U + ((mtk_vcodec_dbg | mtk_v4l2_dbg_level) ? 1000U : 0U))
#endif

struct vcp_dec_mem_list {
	struct vcodec_mem_obj mem;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct list_head list;
};

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

static void get_crop_info(struct vdec_inst *inst, struct v4l2_rect *r)
{
	if (inst == NULL)
		return;
	if (inst->vsi == NULL)
		return;

	r->left      = inst->vsi->crop.left;
	r->top       = inst->vsi->crop.top;
	r->width     = inst->vsi->crop.width;
	r->height    = inst->vsi->crop.height;
	mtk_vcodec_debug(inst, "l=%d, t=%d, w=%d, h=%d",
		r->left, r->top, r->width, r->height);
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
	unsigned long timeout = 0;
	struct share_obj obj;
	unsigned int suspend_block_cnt = 0;
	struct mutex *msg_mutex;
	unsigned int *msg_signaled;
	wait_queue_head_t *msg_wq;

	if (inst->vcu.abort || inst->vcu.daemon_pid != get_vcp_generation())
		return -EIO;

	while (!is_vcp_ready(VCP_A_ID)) {
		mtk_v4l2_debug((((timeout % 20) == 10) ? 0 : 4), "[VCP] wait ready %d ms", timeout);
		mdelay(1);
		timeout++;
		if (timeout > VCP_SYNC_TIMEOUT_MS) {
			mtk_vcodec_err(inst, "VCP_A_ID not ready");
			mtk_smi_dbg_hang_detect("VDEC VCP");
			break;
		}
	}

	if (len > sizeof(struct share_obj)) {
		mtk_vcodec_err(inst, "ipi data size wrong %d > %d", len, sizeof(struct share_obj));
		inst->vcu.abort = 1;
		return -EIO;
	}

	if (!is_ack) {
		while (inst->ctx->dev->is_codec_suspending == 1) {
			suspend_block_cnt++;
			if (suspend_block_cnt > SUSPEND_TIMEOUT_CNT) {
				mtk_v4l2_debug(4, "VDEC blocked by suspend\n");
				suspend_block_cnt = 0;
			}
			usleep_range(10000, 20000);
		}
	}

	memset(&obj, 0, sizeof(obj));
	memcpy(obj.share_buf, msg, len);

	if (*(__u32 *)msg == AP_IPIMSG_DEC_FRAME_BUFFER) {
		obj.id = IPI_VDEC_RESOURCE;
		msg_mutex = &inst->ctx->dev->ipi_mutex_res;
		msg_signaled = &inst->vcu.signaled_res;
		msg_wq = &inst->vcu.wq_res;
	} else {
		obj.id = inst->vcu.id;
		msg_mutex = &inst->ctx->dev->ipi_mutex;
		msg_signaled = &inst->vcu.signaled;
		msg_wq = &inst->vcu.wq;
	}
	if (!is_ack)
		mutex_lock(msg_mutex);

	obj.len = len;
	ipi_size = ((sizeof(u32) * 2) + len + 3) /4;
	inst->vcu.failure = 0;
	if (!is_ack)
		*msg_signaled = false;

	mtk_v4l2_debug(2, "id %d len %d msg 0x%x is_ack %d %d", obj.id, obj.len, *(u32 *)msg,
		is_ack, *msg_signaled);
	ret = mtk_ipi_send(&vcp_ipidev, IPI_OUT_VDEC_1, IPI_SEND_WAIT, &obj,
		ipi_size, IPI_TIMEOUT_MS);

	if (is_ack)
		return 0;

	if (ret != IPI_ACTION_DONE) {
		mtk_vcodec_err(inst, "mtk_ipi_send %X fail %d", *(u32 *)msg, ret);
		mutex_unlock(msg_mutex);
		inst->vcu.failure = VDEC_IPI_MSG_STATUS_FAIL;
		inst->vcu.abort = 1;
		trigger_vcp_halt(VCP_A_ID);
		return -EIO;
	}

	if (!is_ack) {
wait_ack:
		/* wait for VCP's ACK */
		timeout = msecs_to_jiffies(IPI_TIMEOUT_MS);
		ret = wait_event_timeout(*msg_wq, *msg_signaled, timeout);
		*msg_signaled = false;

		if (ret == 0 || inst->vcu.failure) {
			mtk_vcodec_err(inst, "wait vcp ipi %X ack time out or fail! %d %d",
				*(u32 *)msg, ret, inst->vcu.failure);
			mutex_unlock(msg_mutex);
			inst->vcu.failure = VDEC_IPI_MSG_STATUS_FAIL;
			inst->vcu.abort = 1;
			trigger_vcp_halt(VCP_A_ID);
			return -EIO;
		} else if (-ERESTARTSYS == ret) {
			mtk_vcodec_err(inst, "wait vcp ipi %X ack ret %d RESTARTSYS retry! (%d)",
				*(u32 *)msg, ret, inst->vcu.failure);
			goto wait_ack;
		} else if (ret < 0) {
			mtk_vcodec_err(inst, "wait vcp ipi %X ack fail ret %d! (%d)",
				*(u32 *)msg, ret, inst->vcu.failure);
		}
	}
	mutex_unlock(msg_mutex);

	return 0;
}

static void handle_init_ack_msg(struct vdec_vcu_ipi_init_ack *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)
		(unsigned long)msg->ap_inst_addr;
	__u64 shmem_pa_start = (__u64)vcp_get_reserve_mem_phys(VDEC_MEM_ID);
	__u64 inst_offset = ((msg->vcu_inst_addr & 0x0FFFFFFF) - (shmem_pa_start & 0x0FFFFFFF));

	if (vcu == NULL)
		return;
	mtk_vcodec_debug(vcu, "+ ap_inst_addr = 0x%lx",
		(uintptr_t)msg->ap_inst_addr);

	vcu->vsi = (void *)((__u64)vcp_get_reserve_mem_virt(VDEC_MEM_ID) + inst_offset);
	vcu->inst_addr = msg->vcu_inst_addr;
	mtk_vcodec_debug(vcu, "- vcu_inst_addr = 0x%x", vcu->inst_addr);
}

static void handle_query_cap_ack_msg(struct vdec_vcu_ipi_query_cap_ack *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)msg->ap_inst_addr;
	void *data;
	int size = 0;
	__u64 shmem_pa_start = (__u64)vcp_get_reserve_mem_phys(VDEC_MEM_ID);
	__u64 data_offset = ((msg->vcu_data_addr & 0x0FFFFFFF) - (shmem_pa_start & 0x0FFFFFFF));

	if (vcu == NULL)
		return;
	mtk_vcodec_debug(vcu, "+ ap_inst_addr = 0x%lx, vcu_data_addr = 0x%x, id = %d",
		(uintptr_t)msg->ap_inst_addr, msg->vcu_data_addr, msg->id);
	/* mapping VCU address to kernel virtual address */
	data =  (void *)((__u64)vcp_get_reserve_mem_virt(VDEC_MEM_ID) + data_offset);
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

static struct device *get_dev_by_mem_type(struct vdec_inst *inst, struct vcodec_mem_obj *mem)
{
	if (inst->ctx->dec_params.svp_mode) {
		if (mtk_vdec_sw_mem_sec && mem->type == MEM_TYPE_FOR_SW)
			mem->type = MEM_TYPE_FOR_SEC_SW;
		else if (mem->type == MEM_TYPE_FOR_HW)
			mem->type = MEM_TYPE_FOR_SEC_HW;
		else if (mem->type == MEM_TYPE_FOR_UBE_HW)
			mem->type = MEM_TYPE_FOR_SEC_UBE_HW;
	}

	if (mem->type == MEM_TYPE_FOR_SW || mem->type == MEM_TYPE_FOR_SEC_SW)
		return vcp_get_io_device(VCP_IOMMU_WORK_256MB2);
	else if (mem->type == MEM_TYPE_FOR_HW || mem->type == MEM_TYPE_FOR_SEC_HW)
		return &inst->vcu.ctx->dev->plat_dev->dev;
	else if (mem->type == MEM_TYPE_FOR_UBE_HW || mem->type == MEM_TYPE_FOR_SEC_UBE_HW) {
		if (vcp_get_io_device(VCP_IOMMU_UBE_LAT) != NULL)
			return vcp_get_io_device(VCP_IOMMU_UBE_LAT);
		else if (vcp_get_io_device(VCP_IOMMU_UBE_CORE) != NULL)
			return vcp_get_io_device(VCP_IOMMU_UBE_CORE);
		else
			return &inst->vcu.ctx->dev->plat_dev->dev;
	} else
		return NULL;
}

static void handle_vdec_mem_alloc(struct vdec_vcu_ipi_mem_op *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)msg->ap_inst_addr;
	struct vdec_inst *inst = NULL;
	struct device *dev = NULL;
	struct vcp_dec_mem_list *tmp = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *sgt = NULL;

	if (msg->mem.type == MEM_TYPE_FOR_SHM) {
		msg->status = 0;
		msg->mem.va = (__u64)vcp_get_reserve_mem_virt(VDEC_MEM_ID);
		msg->mem.pa = (__u64)vcp_get_reserve_mem_phys(VDEC_MEM_ID);
		msg->mem.len = (__u64)vcp_get_reserve_mem_size(VDEC_MEM_ID);
		msg->mem.iova = msg->mem.pa;
		mtk_v4l2_debug(4, "va 0x%llx pa 0x%llx iova 0x%llx len %d type %d size of %d %d\n",
			msg->mem.va, msg->mem.pa, msg->mem.iova, msg->mem.len, msg->mem.type, sizeof(msg->mem), sizeof(*msg));
	} else {
		if (IS_ERR_OR_NULL(vcu))
			return;

		inst = container_of(vcu, struct vdec_inst, vcu);
		dev = get_dev_by_mem_type(inst, &msg->mem);
		msg->status = mtk_vcodec_alloc_mem(&msg->mem, dev, &attach, &sgt);

		mtk_vcodec_debug(vcu, "va 0x%llx pa 0x%llx iova 0x%llx len %d type %d\n",
			msg->mem.va, msg->mem.pa, msg->mem.iova, msg->mem.len, msg->mem.type);
	}

	/* check memory bound */
	if (msg->mem.type == MEM_TYPE_FOR_SW || msg->mem.type == MEM_TYPE_FOR_SEC_SW) {
		if ((msg->mem.iova >> 28) != ((msg->mem.iova + msg->mem.len) >> 28)) {
			mtk_vcodec_free_mem(&msg->mem, dev, attach, sgt);
			msg->status = -ENOMEM;
		}
	}

	if (msg->status) {
		mtk_vcodec_err(vcu, "fail %d, va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			msg->status, msg->mem.va, msg->mem.pa,
			msg->mem.iova, msg->mem.len,  msg->mem.type);
	} else if (msg->mem.type != MEM_TYPE_FOR_SHM) {
		tmp = kmalloc(sizeof(struct vcp_dec_mem_list), GFP_KERNEL);
		if (tmp) {
			mutex_lock(vcu->ctx_ipi_lock);
			tmp->attach = attach;
			tmp->sgt = sgt;
			tmp->mem = msg->mem;
			list_add_tail(&tmp->list, &vcu->bufs);
			mutex_unlock(vcu->ctx_ipi_lock);
		}
	}
}

static void handle_vdec_mem_free(struct vdec_vcu_ipi_mem_op *msg)
{
	struct vdec_vcu_inst *vcu = (struct vdec_vcu_inst *)msg->ap_inst_addr;
	struct vdec_inst *inst = NULL;
	struct device *dev = NULL;
	struct vcp_dec_mem_list *tmp = NULL;
	struct list_head *p, *q;
	bool found = 0;

	if (IS_ERR_OR_NULL(vcu))
		return;
	mutex_lock(vcu->ctx_ipi_lock);
	list_for_each_safe(p, q, &vcu->bufs) {
		tmp = list_entry(p, struct vcp_dec_mem_list, list);
		if (!memcmp(&tmp->mem, &msg->mem, sizeof(struct vcodec_mem_obj))) {
			found = 1;
			list_del(p);
			break;
		}
	}
	mutex_unlock(vcu->ctx_ipi_lock);
	if (!found) {
		mtk_vcodec_err(vcu, "not found  %d, va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			msg->status, msg->mem.va, msg->mem.pa,
			msg->mem.iova, msg->mem.len,  msg->mem.type);
		return;
	}

	mtk_vcodec_debug(vcu, "va 0x%llx pa 0x%llx iova 0x%llx len %d type %d\n",
		msg->mem.va, msg->mem.pa, msg->mem.iova, msg->mem.len,  msg->mem.type);

	inst = container_of(vcu, struct vdec_inst, vcu);
	dev = get_dev_by_mem_type(inst, &msg->mem);
	msg->status = mtk_vcodec_free_mem(&msg->mem, dev, tmp->attach, tmp->sgt);
	kfree(tmp);

	if (msg->status)
		mtk_vcodec_err(vcu, "fail %d, va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			msg->status, msg->mem.va, msg->mem.pa,
			msg->mem.iova, msg->mem.len,  msg->mem.type);
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
	unsigned int suspend_block_cnt = 0;
	struct list_head *p, *q;
	struct mtk_vcodec_ctx *temp_ctx;
	int msg_valid = 0;

	mtk_v4l2_debug_enter();
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_cmd) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_init) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_dec_start) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_set_param) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_ap_ipi_query_cap) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_vcu_ipi_ack) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_vcu_ipi_init_ack) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_vcu_ipi_query_cap_ack) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct vdec_vcu_ipi_mem_op) > SHARE_BUF_SIZE);

	do {
		ret = wait_event_interruptible(dev->mq.wq, atomic_read(&dev->mq.cnt) > 0);
		if (ret) {
			while (dev->is_codec_suspending == 1) {
				suspend_block_cnt++;
				if (suspend_block_cnt > SUSPEND_TIMEOUT_CNT) {
					mtk_v4l2_debug(4, "blocked by suspend\n");
					suspend_block_cnt = 0;
				}
				usleep_range(10000, 20000);
			}
			continue;
		}

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
				ret = mtk_ipi_send(&vcp_ipidev, IPI_OUT_VDEC_1, IPI_SEND_WAIT, obj,
					PIN_OUT_SIZE_VDEC, 100);
				if (ret != IPI_ACTION_DONE)
					mtk_v4l2_err("mtk_ipi_send (msg_id %X) fail %d",
						msg->msg_id, ret);
				kfree(mq_node);
				continue;
			}
		}
		vcu = (struct vdec_vcu_inst *)(unsigned long)msg->ap_inst_addr;

		/* Check IPI inst is valid */
		mutex_lock(&dev->ctx_mutex);
		msg_valid = 0;
		list_for_each_safe(p, q, &dev->ctx_list) {
			temp_ctx = list_entry(p, struct mtk_vcodec_ctx, list);
			inst = (struct vdec_inst *)temp_ctx->drv_handle;
			if (inst != NULL && vcu == &inst->vcu) {
				msg_valid = 1;
				break;
			}
		}
		if (!msg_valid) {
			mtk_v4l2_err(" msg msg_id %X vcu not exist %p\n", msg->msg_id, vcu);
			mutex_unlock(&dev->ctx_mutex);
			kfree(mq_node);
			continue;
		}

		if (vcu->abort || vcu->daemon_pid != get_vcp_generation()) {
			mtk_v4l2_err(" [%d] msg msg_id %X vcu abort %d %d\n",
				msg->msg_id, vcu->ctx->id, vcu->daemon_pid, get_vcp_generation());
			mutex_unlock(&dev->ctx_mutex);
			kfree(mq_node);
			continue;
		}
		mtk_v4l2_debug(2, "+ pop msg_id %X ml_cnt %d, vcu %lx, status %d",
			msg->msg_id, atomic_read(&dev->mq.cnt), (unsigned long)vcu, msg->status);

		vsi = (struct vdec_vsi *)vcu->vsi;
		inst = container_of(vcu, struct vdec_inst, vcu);

		if (msg->msg_id == VCU_IPIMSG_DEC_CHECK_CODEC_ID) {

			if (check_codec_id(msg, vcu->ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc,
				vcu->ctx->dec_params.svp_mode) == 0)
				msg->status = 0;
			else
				msg->status = -1;

			msg->msg_id = AP_IPIMSG_DEC_CHECK_CODEC_ID_DONE;
			msg->ctx_id = inst->ctx->id;
			vdec_vcp_ipi_send(inst, msg, sizeof(*msg), 1);
		} else if (msg->status == VDEC_IPI_MSG_STATUS_OK) {
			if ((msg->msg_id & 0xF000) == VCU_IPIMSG_VDEC_SEND_BASE)
				msg->ctx_id = inst->ctx->id;
			switch (msg->msg_id) {
			case VCU_IPIMSG_DEC_DONE:
				vcu->signaled_res = true;
				wake_up(&vcu->wq_res);
				break;
			case VCU_IPIMSG_DEC_INIT_DONE:
				handle_init_ack_msg((void *)obj->share_buf);
			case VCU_IPIMSG_DEC_START_DONE:
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
				mtk_vdec_put_fb(vcu->ctx, PUT_BUFFER_CALLBACK, msg->reserved != 0);
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
				msg->msg_id = AP_IPIMSG_DEC_LOCK_LAT_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), 1);
				break;
			case VCU_IPIMSG_DEC_UNLOCK_LAT:
				vdec_decode_unprepare(vcu->ctx, MTK_VDEC_LAT);
				msg->msg_id = AP_IPIMSG_DEC_UNLOCK_LAT_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), 1);
				break;
			case VCU_IPIMSG_DEC_LOCK_CORE:
				vdec_decode_prepare(vcu->ctx, MTK_VDEC_CORE);
				msg->msg_id = AP_IPIMSG_DEC_LOCK_CORE_DONE;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), 1);
				break;
			case VCU_IPIMSG_DEC_UNLOCK_CORE:
				vdec_decode_unprepare(vcu->ctx, MTK_VDEC_CORE);
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
				msg->status = ret;
				vcodec_trace_count("VDEC_HW_CORE", 1);
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), 1);
				break;
			case VCU_IPIMSG_DEC_GET_FRAME_BUFFER:
				mtk_vcodec_err(vcu, "GET_FRAME_BUFFER not support", msg->msg_id);
				break;
			default:
				mtk_vcodec_err(vcu, "invalid msg=%X", msg->msg_id);
				break;
			}
		} else {
			switch (msg->msg_id) {
			case VCU_IPIMSG_DEC_DONE:
				vcu->signaled_res = true;
				wake_up(&vcu->wq_res);
				break;
			case VCU_IPIMSG_DEC_INIT_DONE:
				vcu->failure = VDEC_IPI_MSG_STATUS_FAIL;
			case VCU_IPIMSG_DEC_START_DONE:
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
				msg->ctx_id = inst->ctx->id;
				msg->status = ret;
				vdec_vcp_ipi_send(inst, msg, sizeof(*msg), 1);
				vcodec_trace_count("VDEC_HW_LAT", 1);
				break;
			default:
				mtk_vcodec_err(vcu, "invalid msg=%X", msg->msg_id);
				break;
			}
		}
		mtk_vcodec_debug(vcu, "- id=%X", msg->msg_id);
		mutex_unlock(&dev->ctx_mutex);
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
		mtk_v4l2_debug(8, "push ipi_id %x msg_id %x, ml_cnt %d",
			obj->id, msg->msg_id, atomic_read(&dev->mq.cnt));
		wake_up(&dev->mq.wq);
	} else {
		mtk_v4l2_err("kmalloc fail\n");
	}

	return 0;
}

static int vcp_vdec_notify_callback(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	struct mtk_vcodec_dev *dev;
	struct list_head *p, *q;
	struct mtk_vcodec_ctx *ctx;

	dev = container_of(this, struct mtk_vcodec_dev, vcp_notify);

	switch (event) {
	case VCP_EVENT_STOP:
		mutex_lock(&dev->ctx_mutex);
		// check release all ctx lock
		list_for_each_safe(p, q, &dev->ctx_list) {
			ctx = list_entry(p, struct mtk_vcodec_ctx, list);
			if (ctx != NULL && ctx->state != MTK_STATE_ABORT) {
				ctx->state = MTK_STATE_ABORT;
				vdec_check_release_lock(ctx);
				mtk_vdec_queue_error_event(ctx);
			}
		}
		mutex_unlock(&dev->ctx_mutex);
	break;
	}
	return NOTIFY_DONE;
}

void vdec_vcp_probe(struct mtk_vcodec_dev *dev)
{
	int ret;

	mtk_v4l2_debug_enter();
	INIT_LIST_HEAD(&dev->mq.head);
	spin_lock_init(&dev->mq.lock);
	init_waitqueue_head(&dev->mq.wq);
	atomic_set(&dev->mq.cnt, 0);

	mtk_vcodec_vcp |= 1 << MTK_INST_DECODER;

	ret = mtk_ipi_register(&vcp_ipidev, IPI_IN_VDEC_1,
		vdec_vcp_ipi_isr, dev, &dev->dec_ipi_data);
	if (ret) {
		mtk_v4l2_debug(0, " ipi_register, ret %d\n", ret);
	}

	kthread_run(vcp_dec_ipi_handler, dev, "vdec_ipi_recv");

	dev->vcp_notify.notifier_call = vcp_vdec_notify_callback;
	vcp_A_register_notify(&dev->vcp_notify);

	mtk_v4l2_debug_leave();
}

static int vdec_vcp_init(struct mtk_vcodec_ctx *ctx, unsigned long *h_vdec)
{
	int err = 0;
	struct vdec_ap_ipi_init msg;
	struct vdec_inst *inst = NULL;
	__u32 fourcc;

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return -ENOMEM;
	if (!ctx) {
		err = -ENOMEM;
		goto error_free_inst;
	}

	mtk_vcodec_add_ctx_list(ctx);

	inst->ctx = ctx;
	fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;

	inst->vcu.id = IPI_VDEC_COMMON;
	inst->vcu.ctx = ctx;
	init_waitqueue_head(&inst->vcu.wq);
	init_waitqueue_head(&inst->vcu.wq_res);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_INIT;
	msg.ctx_id = inst->ctx->id;
	msg.ap_inst_addr = (unsigned long)&inst->vcu;

	inst->vcu.ctx_ipi_lock = kzalloc(sizeof(struct mutex),
		GFP_KERNEL);
	if (!inst->vcu.ctx_ipi_lock)
		goto error_free_inst;
	mutex_init(inst->vcu.ctx_ipi_lock);
	INIT_LIST_HEAD(&inst->vcu.bufs);

	mtk_vcodec_debug(inst, "vdec_inst=%p svp_mode=%d",
		&inst->vcu, ctx->dec_params.svp_mode);
	*h_vdec = (unsigned long)inst;
	inst->vcu.daemon_pid = get_vcp_generation();
	err = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), 0);

	if (err != 0) {
		mtk_vcodec_err(inst, "%s err=%d", __func__, err);
		goto error_free_inst;
	}

	inst->vsi = (struct vdec_vsi *)inst->vcu.vsi;
	inst->vcu.signaled = false;
	inst->vcu.signaled_res = false;
	ctx->input_driven = inst->vsi->input_driven;
	ctx->ipi_blocked = &inst->vsi->ipi_blocked;
	*(ctx->ipi_blocked) = 0;

	mtk_v4l2_debug(0, "[%d] %c%c%c%c(%d) Decoder Instance >> %p, ap_inst_addr %llx",
		ctx->id, fourcc & 0xFF, (fourcc >> 8) & 0xFF,
		(fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF,
		inst->vcu.id, inst, msg.ap_inst_addr);

	return 0;

error_free_inst:
	mtk_vcodec_del_ctx_list(ctx);
	kfree(inst->vcu.ctx_ipi_lock);
	kfree(inst);
	*h_vdec = (unsigned long)NULL;

	return err;
}

static void vdec_vcp_deinit(unsigned long h_vdec)
{
	struct vdec_inst *inst = (struct vdec_inst *)h_vdec;
	struct vdec_ap_ipi_cmd msg;
	struct vcp_dec_mem_list *tmp = NULL;
	struct list_head *p, *q;
	struct device *dev = NULL;
	int err = 0;

	mtk_vcodec_debug_enter(inst);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_DEINIT;
	msg.ctx_id = inst->ctx->id;
	msg.vcu_inst_addr = inst->vcu.inst_addr;

	err = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), 0);
	mtk_vcodec_debug(inst, "- ret=%d", err);

	mtk_vcodec_del_ctx_list(inst->ctx);

	mutex_lock(inst->vcu.ctx_ipi_lock);
	list_for_each_safe(p, q, &inst->vcu.bufs) {
		tmp = list_entry(p, struct vcp_dec_mem_list, list);

		dev = get_dev_by_mem_type(inst, &tmp->mem);
		mtk_vcodec_free_mem(&tmp->mem, dev, tmp->attach, tmp->sgt);
		mtk_v4l2_debug(0, "[%d] leak free va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			inst->ctx->id, tmp->mem.va, tmp->mem.pa,
			tmp->mem.iova, tmp->mem.len,  tmp->mem.type);

		list_del(p);
		kfree(tmp);
	}
	mutex_unlock(inst->vcu.ctx_ipi_lock);
	mutex_destroy(inst->vcu.ctx_ipi_lock);
	kfree(inst->vcu.ctx_ipi_lock);
	kfree(inst);
}

int vdec_vcp_reset(struct vdec_inst *inst, enum vdec_reset_type drain_type)
{
	struct vdec_ap_ipi_cmd msg;
	int err = 0;
	mtk_vcodec_debug(inst, "drain_type %d +", drain_type);
	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_DEC_RESET;
	msg.ctx_id = inst->ctx->id;
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
	msg.ctx_id = inst->ctx->id;
	msg.vcu_inst_addr = vcu->inst_addr;
	msg.data[0] = (unsigned int)bs->size;
	msg.data[1] = (unsigned int)bs->length;
	msg.data[2] = (unsigned int)bs->flags;
	ret = vdec_vcp_ipi_send(inst, &msg, sizeof(msg), 0);

	*src_chg = inst->vsi->dec.vdec_changed_info;
	*(errormap_info + bs->index % VB2_MAX_FRAME) =
		inst->vsi->dec.error_map;

	if ((*src_chg & VDEC_NEED_SEQ_HEADER) != 0U)
		mtk_vcodec_debug(inst, "- need first seq header -");
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
	mtk_vcodec_debug(inst, "\n - NALU[%d] err=%d -\n", inst->num_nalu, ret);

	return ret;
}

void set_vdec_vcp_data(struct vdec_inst *inst, enum vcp_reserve_mem_id_t id, void *string)
{
	struct vdec_ap_ipi_set_param msg;
	void *string_va = (void *)(__u64)vcp_get_reserve_mem_virt(id);
	void *string_pa = (void *)(__u64)vcp_get_reserve_mem_phys(id);
	__u64 mem_size = (__u64)vcp_get_reserve_mem_size(id);
	int string_len = strlen((char *)string);

	mtk_vcodec_debug(inst, "mem_size 0x%llx, string_va 0x%llx, string_pa 0x%llx\n",
		mem_size, string_va, string_pa);
	mtk_vcodec_debug(inst, "string: %s\n", (char *)string);
	mtk_vcodec_debug(inst, "string_len:%d\n", string_len);

	if (string_len <= (mem_size-1))
		memcpy(string_va, (char *)string, string_len + 1);

	inst->vcu.ctx = inst->ctx;
	inst->vcu.id = (inst->vcu.id == IPI_VCU_INIT) ? IPI_VDEC_COMMON : inst->vcu.id;

	memset(&msg, 0, sizeof(msg));

	msg.msg_id = AP_IPIMSG_DEC_SET_PARAM;

	if (id == VDEC_SET_PROP_MEM_ID)
		msg.id = SET_PARAM_VDEC_PROPERTY;
	else if (id == VDEC_VCP_LOG_INFO_ID)
		msg.id = SET_PARAM_VDEC_VCP_LOG_INFO;
	else
		mtk_vcodec_err(inst, "unknown id (%d)", msg.id);

	msg.ctx_id = inst->ctx->id;
	msg.vcu_inst_addr = (uintptr_t)&inst->vcu;
	msg.data[0] = (__u32)((__u64)string_pa & 0xFFFFFFFF);
	msg.data[1] = (__u32)((__u64)string_pa >> 32);
	inst->vcu.daemon_pid = get_vcp_generation();

	mtk_vcodec_debug(inst, "msg.id %d msg.data[0]:0x%08x, msg.data[1]:0x%08x\n",
		msg.id, msg.data[0], msg.data[1]);
	vdec_vcp_ipi_send(inst, &msg, sizeof(msg), 1);
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
	msg.ctx_id = inst->ctx->id;
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
			if (err < 0)
				break;
		}
	} while (pfb != NULL);

	if (fb != NULL && dst_not_get) {
		mtk_vcodec_debug(inst, "warning: dst_buf_info->frame_buffer id=%d %p %llx not get",
			dst_buf_info->frame_buffer.index,
			&dst_buf_info->frame_buffer,
			(u64)&dst_buf_info->frame_buffer);
	}
	if (err < 0)
		mtk_vcodec_err(inst, "- id=%X ret=%d", AP_IPIMSG_DEC_FRAME_BUFFER, err);
	else
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
	msg.ctx_id = inst->ctx->id;
	msg.id = type;
	msg.vcu_inst_addr = inst->vcu.inst_addr;

	switch (type) {
	case SET_PARAM_FRAME_BUFFER:
		ret = vdec_vcp_set_frame_buffer(inst, in);
		break;
	case SET_PARAM_FRAME_SIZE:
	case SET_PARAM_SET_FIXED_MAX_OUTPUT_BUFFER:
		msg.data[0] = (__u32)(*param_ptr);
		msg.data[1] = (__u32)(*(param_ptr + 1));
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
	case SET_PARAM_VDEC_PROPERTY:
		set_vdec_vcp_data(inst, VDEC_SET_PROP_MEM_ID, in);
		break;
	case SET_PARAM_VDEC_VCP_LOG_INFO:
		set_vdec_vcp_data(inst, VDEC_VCP_LOG_INFO_ID, in);
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
	msg.ctx_id = inst->ctx->id;
	msg.ap_inst_addr = (uintptr_t)&inst->vcu;
	msg.ap_data_addr = (uintptr_t)video_fmt;
	inst->vcu.daemon_pid = get_vcp_generation();
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
	msg.ctx_id = inst->ctx->id;
	msg.ap_inst_addr = (uintptr_t)&inst->vcu;
	msg.ap_data_addr = (uintptr_t)codec_framesizes;
	inst->vcu.daemon_pid = get_vcp_generation();
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

		if (inst->vsi == NULL)
			return -EINVAL;
		vdec_get_fb(inst, &inst->vsi->list_free, false, out);

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
