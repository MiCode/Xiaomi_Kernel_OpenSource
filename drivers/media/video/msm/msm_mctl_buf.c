/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>

#include <linux/android_pmem.h>

#include "msm.h"
#include "msm_ispif.h"

#ifdef CONFIG_MSM_CAMERA_DEBUG
#define D(fmt, args...) pr_debug("msm_mctl_buf: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

#define PAD_TO_WORD(a)	  (((a) + 3) & ~3)
#define CEILING16(a)	(((a) + 15) & ~15)

static int buffer_size(uint32_t *yoffset, uint32_t *cbcroffset,
						int width, int height,
						int pixelformat, int ext_mode)
{
	int size;

	*yoffset = 0;
	switch (pixelformat) {
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12:
		if (ext_mode == MSM_V4L2_EXT_CAPTURE_MODE_VIDEO) {
			*cbcroffset = PAD_TO_2K(width * height, 1);
			size = *cbcroffset + PAD_TO_2K(width * height/2, 1);
		} else if (ext_mode == MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW) {
			*cbcroffset = PAD_TO_WORD(width * height);
			size = *cbcroffset + PAD_TO_WORD(width * height/2);
		} else {
			*cbcroffset = PAD_TO_WORD(width * CEILING16(height));
			size = *cbcroffset
				+ PAD_TO_WORD(width * CEILING16(height)/2);
		}
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		size = PAD_TO_WORD(width * height);
		break;
	default:
		pr_err("%s: pixelformat %d not supported.\n",
			__func__, pixelformat);
		size = -EINVAL;
	}
	size = PAGE_ALIGN(size);
	return size;
}

static int msm_vb2_ops_queue_setup(struct vb2_queue *vq,
					unsigned int *num_buffers,
					unsigned int *num_planes,
					unsigned long sizes[],
					void *alloc_ctxs[])
{
	/* get the video device */
	struct msm_cam_v4l2_dev_inst *pcam_inst = vb2_get_drv_priv(vq);
	struct msm_cam_v4l2_device *pcam = pcam_inst->pcam;
	uint32_t yoffset, cbcroffset;

	D("%s\n", __func__);
	if (!pcam || !(*num_buffers)) {
		pr_err("%s error : invalid input\n", __func__);
		return -EINVAL;
	}
	/* TBD: need to set based on format */
	*num_planes = 1;
	D("%s, inst=0x%x,idx=%d, width = %d\n", __func__,
		(u32)pcam_inst, pcam_inst->my_index,
		pcam_inst->vid_fmt.fmt.pix.width);
	D("%s, inst=0x%x,idx=%d, height = %d\n", __func__,
		(u32)pcam_inst, pcam_inst->my_index,
		pcam_inst->vid_fmt.fmt.pix.height);
	sizes[0] = buffer_size(&yoffset, &cbcroffset,
				pcam_inst->vid_fmt.fmt.pix.width,
				pcam_inst->vid_fmt.fmt.pix.height,
				pcam_inst->vid_fmt.fmt.pix.pixelformat,
				pcam_inst->image_mode);
	return 0;
}

static void msm_vb2_ops_wait_prepare(struct vb2_queue *q)
{
	/* we use polling so do not use this fn now */
}
static void msm_vb2_ops_wait_finish(struct vb2_queue *q)
{
	/* we use polling so do not use this fn now */
}

static int msm_vb2_ops_buf_init(struct vb2_buffer *vb)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct msm_cam_v4l2_device *pcam;
	struct videobuf2_contig_pmem *mem;
	struct vb2_queue	*vq;
	uint32_t yoffset, cbcroffset;
	int size, rc = 0;
	vq = vb->vb2_queue;
	pcam_inst = vb2_get_drv_priv(vq);
	pcam = pcam_inst->pcam;
	D("%s\n", __func__);
	D("%s, inst=0x%x,idx=%d, width = %d\n", __func__,
		(u32)pcam_inst, pcam_inst->my_index,
		pcam_inst->vid_fmt.fmt.pix.width);
	D("%s, inst=0x%x,idx=%d, height = %d\n", __func__,
		(u32)pcam_inst, pcam_inst->my_index,
		pcam_inst->vid_fmt.fmt.pix.height);
	mem = vb2_plane_cookie(vb, 0);
	size = buffer_size(&yoffset, &cbcroffset,
				pcam_inst->vid_fmt.fmt.pix.width,
				pcam_inst->vid_fmt.fmt.pix.height,
				pcam_inst->vid_fmt.fmt.pix.pixelformat,
				pcam_inst->image_mode);
	if (size < 0)
		return size;
	if (vb->v4l2_buf.memory == V4L2_MEMORY_USERPTR)
		rc = videobuf2_pmem_contig_user_get(mem, yoffset, cbcroffset,
			pcam_inst->buf_offset[vb->v4l2_buf.index],
			pcam_inst->path);
	else
		rc = videobuf2_pmem_contig_mmap_get(mem, yoffset, cbcroffset,
			pcam_inst->path);

	return rc;
}

static int msm_vb2_ops_buf_prepare(struct vb2_buffer *vb)
{
	int i, rc = 0;
	uint32_t len;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct msm_cam_v4l2_device *pcam;
	struct msm_frame_buffer *buf;
	struct vb2_queue	*vq = vb->vb2_queue;

	D("%s\n", __func__);
	if (!vb || !vq) {
		pr_err("%s error : input is NULL\n", __func__);
		return -EINVAL;
	}
	pcam_inst = vb2_get_drv_priv(vq);
	pcam = pcam_inst->pcam;
	buf = container_of(vb, struct msm_frame_buffer, vidbuf);

	if (!pcam || !buf) {
		pr_err("%s error : pointer is NULL\n", __func__);
		return -EINVAL;
	}
	/* by this time vid_fmt should be already set.
	 * return error if it is not. */
	if ((pcam_inst->vid_fmt.fmt.pix.width == 0) ||
		(pcam_inst->vid_fmt.fmt.pix.height == 0)) {
		pr_err("%s error : pcam vid_fmt is not set\n", __func__);
		return -EINVAL;
	}
	/* prefill in the byteused field */
	for (i = 0; i < vb->num_planes; i++) {
		len = vb2_plane_size(vb, i);
		vb2_set_plane_payload(vb, i, len);
	}
	buf->inuse = 0;
	return rc;
}

static int msm_vb2_ops_buf_finish(struct vb2_buffer *vb)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct msm_cam_v4l2_device *pcam;
	struct msm_frame_buffer *buf;

	pcam_inst = vb2_get_drv_priv(vb->vb2_queue);
	pcam = pcam_inst->pcam;
	buf = container_of(vb, struct msm_frame_buffer, vidbuf);
	D("%s: inst=0x%x, buf=0x, %x, idx=%d\n", __func__,
	(uint32_t)pcam_inst, (uint32_t)buf, vb->v4l2_buf.index);
	return 0;
}

static void msm_vb2_ops_buf_cleanup(struct vb2_buffer *vb)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct msm_cam_v4l2_device *pcam;
	struct videobuf2_contig_pmem *mem;
	struct msm_frame_buffer *buf;
	uint32_t i, vb_phyaddr = 0, buf_phyaddr = 0;
	unsigned long flags = 0;

	pcam_inst = vb2_get_drv_priv(vb->vb2_queue);
	pcam = pcam_inst->pcam;
	buf = container_of(vb, struct msm_frame_buffer, vidbuf);
	mem = vb2_plane_cookie(vb, 0);
	if (!mem)
		return;
	D("%s: inst=0x%x, buf=0x%x, idx=%d\n", __func__,
	(uint32_t)pcam_inst, (uint32_t)buf, vb->v4l2_buf.index);
	vb_phyaddr = (unsigned long) videobuf2_to_pmem_contig(vb, 0);
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	if (!list_empty(&pcam_inst->free_vq)) {
		list_for_each_entry(buf, &pcam_inst->free_vq, list) {
			buf_phyaddr = (unsigned long)
				videobuf2_to_pmem_contig(&buf->vidbuf, 0);
		  D("%s vb_idx=%d,vb_paddr=0x%x,phyaddr=0x%x\n",
			__func__, buf->vidbuf.v4l2_buf.index,
			buf_phyaddr, vb_phyaddr);
			if (vb_phyaddr == buf_phyaddr) {
				list_del_init(&buf->list);
				break;
			}
		}
	}
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);

	for (i = 0; i < vb->num_planes; i++) {
		mem = vb2_plane_cookie(vb, i);
		if (mem)
			videobuf2_pmem_contig_user_put(mem);
	}
}

static int msm_vb2_ops_start_streaming(struct vb2_queue *q)
{
	return 0;
}

static int msm_vb2_ops_stop_streaming(struct vb2_queue *q)
{
	int rc = 0;
	struct msm_free_buf *free_buf = NULL;
	struct msm_cam_v4l2_dev_inst *pcam_inst = vb2_get_drv_priv(q);
	if (rc != 0)
		msm_mctl_release_free_buf(&pcam_inst->pcam->mctl,
					pcam_inst->path, free_buf);
	return 0;
}

static void msm_vb2_ops_buf_queue(struct vb2_buffer *vb)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst = NULL;
	struct msm_cam_v4l2_device *pcam = NULL;
	unsigned long phyaddr = 0;
	unsigned long flags = 0;
	struct vb2_queue *vq = vb->vb2_queue;
	struct videobuf2_contig_pmem *mem;
	struct msm_frame_buffer *buf;
	D("%s\n", __func__);
	if (!vb || !vq) {
		pr_err("%s error : input is NULL\n", __func__);
		return ;
	}
	pcam_inst = vb2_get_drv_priv(vq);
	pcam = pcam_inst->pcam;
	D("%s pcam_inst=%p,(vb=0x%p),idx=%d,len=%d\n",
		__func__, pcam_inst,
	vb, vb->v4l2_buf.index, vb->v4l2_buf.length);
	buf = container_of(vb, struct msm_frame_buffer, vidbuf);
	/* we are returning a buffer to the queue */
	mem = vb2_plane_cookie(vb, 0);
	/* get the physcial address of the buffer */
	phyaddr = (unsigned long) videobuf2_to_pmem_contig(vb, 0);
	D("%s buffer type is %d\n", __func__, mem->buffer_type);
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	D("Inst %p,  >>>ADD>>> Buf 0x%x index %d into free Q",
		 pcam_inst, (int)phyaddr, vb->v4l2_buf.index);
	list_add_tail(&buf->list, &pcam_inst->free_vq);
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
}

static struct vb2_ops msm_vb2_ops = {
	.queue_setup = msm_vb2_ops_queue_setup,
	.wait_prepare = msm_vb2_ops_wait_prepare,
	.wait_finish = msm_vb2_ops_wait_finish,
	.buf_init = msm_vb2_ops_buf_init,
	.buf_prepare = msm_vb2_ops_buf_prepare,
	.buf_finish = msm_vb2_ops_buf_finish,
	.buf_cleanup = msm_vb2_ops_buf_cleanup,
	.start_streaming = msm_vb2_ops_start_streaming,
	.stop_streaming = msm_vb2_ops_stop_streaming,
	.buf_queue = msm_vb2_ops_buf_queue,
};


/* prepare a video buffer queue for a vl42 device*/
static int msm_vidbuf_init(struct msm_cam_v4l2_dev_inst *pcam_inst,
						struct vb2_queue *q)
{
	int rc = 0;
	struct resource *res;
	struct platform_device *pdev = NULL;
	struct msm_cam_v4l2_device *pcam = NULL;

	D("%s\n", __func__);
	pcam = pcam_inst->pcam;
	if (!pcam || !q) {
		pr_err("%s error : input is NULL\n", __func__);
		return -EINVAL;
	} else
		pdev = pcam->mctl.sync.pdev;

	if (!pdev) {
		pr_err("%s error : pdev is NULL\n", __func__);
		return -EINVAL;
	}
	if (pcam->use_count == 1) {
		/* first check if we have resources */
		res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
		if (res) {
			D("res->start = 0x%x\n", (u32)res->start);
			D("res->size = 0x%x\n", (u32)resource_size(res));
			D("res->end = 0x%x\n", (u32)res->end);
			rc = dma_declare_coherent_memory(&pdev->dev, res->start,
					res->start,
					resource_size(res),
					DMA_MEMORY_MAP |
					DMA_MEMORY_EXCLUSIVE);
			if (!rc) {
				pr_err("%s: Unable to declare coherent memory.\n",
				__func__);
				rc = -ENXIO;
				return rc;
			}
			D("%s: found DMA capable resource\n", __func__);
		} else {
			pr_err("%s: no DMA capable resource\n", __func__);
			return -ENOMEM;
		}
	}
	spin_lock_init(&pcam_inst->vq_irqlock);
	INIT_LIST_HEAD(&pcam_inst->free_vq);
	videobuf2_queue_pmem_contig_init(q, V4L2_BUF_TYPE_VIDEO_CAPTURE,
						&msm_vb2_ops,
						sizeof(struct msm_frame_buffer),
						(void *)pcam_inst);
	return 0;
}

static int msm_mctl_out_type_to_inst_index(struct msm_cam_v4l2_device *pcam,
					int out_type)
{
	switch (out_type) {
	case VFE_MSG_OUTPUT_P:
		return pcam->dev_inst_map
			[MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW]->my_index;
	case VFE_MSG_OUTPUT_V:
		return pcam->dev_inst_map
			[MSM_V4L2_EXT_CAPTURE_MODE_VIDEO]->my_index;
	case VFE_MSG_OUTPUT_S:
		return pcam->dev_inst_map
			[MSM_V4L2_EXT_CAPTURE_MODE_MAIN]->my_index;
	case VFE_MSG_OUTPUT_T:
		return pcam->dev_inst_map
			[MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL]->my_index;
	default:
		return 0;
	}
	return 0;
}

static void msm_mctl_gettimeofday(struct timeval *tv)
{
	struct timespec ts;

	BUG_ON(!tv);

	ktime_get_ts(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec/1000;
}

static struct msm_frame_buffer *msm_mctl_buf_find(
	struct msm_cam_media_controller *pmctl,
	struct msm_cam_v4l2_dev_inst *pcam_inst, int del_buf,
	int msg_type, uint32_t y_phy)
{
	struct msm_frame_buffer *buf = NULL;
	uint32_t buf_phyaddr = 0;
	unsigned long flags = 0;

	/* we actually need a list, not a queue */
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	if (!list_empty(&pcam_inst->free_vq)) {
		list_for_each_entry(buf, &pcam_inst->free_vq, list) {
			buf_phyaddr = (unsigned long)
				videobuf2_to_pmem_contig(&buf->vidbuf, 0);
			D("%s vb_idx=%d,vb_paddr=0x%x,y_phy=0x%x\n",
				__func__, buf->vidbuf.v4l2_buf.index,
				buf_phyaddr, y_phy);
			if (y_phy == buf_phyaddr) {
				if (del_buf)
					list_del_init(&buf->list);
				spin_unlock_irqrestore(&pcam_inst->vq_irqlock,
					flags);
				return buf;
			}
		}
	}
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	return NULL;
}

int msm_mctl_buf_done_proc(
		struct msm_cam_media_controller *pmctl,
		struct msm_cam_v4l2_dev_inst *pcam_inst,
		int msg_type, uint32_t y_phy,
		uint32_t *frame_id, int gen_timestamp)
{
	struct msm_frame_buffer *buf = NULL;
	int del_buf = 1;

	buf = msm_mctl_buf_find(pmctl, pcam_inst, del_buf,
							msg_type, y_phy);
	if (!buf) {
		pr_err("%s: y_phy=0x%x not found\n",
			__func__, y_phy);
		return -EINVAL;
	}
	if (gen_timestamp) {
		if (frame_id)
			buf->vidbuf.v4l2_buf.sequence = *frame_id;
		msm_mctl_gettimeofday(
			&buf->vidbuf.v4l2_buf.timestamp);
	}
	vb2_buffer_done(&buf->vidbuf, VB2_BUF_STATE_DONE);
	return 0;
}

static int msm_mctl_buf_divert(
			struct msm_cam_media_controller *pmctl,
			struct msm_cam_v4l2_dev_inst *pcam_inst,
			struct msm_cam_evt_divert_frame *div)
{
	struct v4l2_event v4l2_evt;
	struct msm_cam_evt_divert_frame *tmp;
	memset(&v4l2_evt, 0, sizeof(v4l2_evt));
	v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			MSM_CAM_RESP_DIV_FRAME_EVT_MSG;
	memcpy(&v4l2_evt.u.data[0], div,
			sizeof(struct msm_cam_evt_divert_frame));
	D("%s inst=%p, img_mode=%d, frame_id=%d,phy=0x%x,len=%d\n",
		__func__, pcam_inst, pcam_inst->image_mode, div->frame_id,
		(uint32_t)div->phy_addr, div->length);
	tmp = (struct msm_cam_evt_divert_frame *)&v4l2_evt.u.data[0];
	v4l2_event_queue(
		pmctl->config_device->config_stat_event_queue.pvdev,
		&v4l2_evt);
	return 0;
}

int msm_mctl_buf_done(struct msm_cam_media_controller *p_mctl,
			int msg_type, uint32_t y_phy, uint32_t frame_id)
{
	uint32_t pp_key = 0;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	int idx;
	int del_buf = 0;
	int image_mode = MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT;
	unsigned long flags;
	int path = 0;


	idx = msm_mctl_out_type_to_inst_index(
					p_mctl->sync.pcam_sync, msg_type);
	pcam_inst = p_mctl->sync.pcam_sync->dev_inst[idx];
	switch (msg_type) {
	case VFE_MSG_OUTPUT_P:
		pp_key = PP_PREV;
		image_mode = MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW;
		path = OUTPUT_TYPE_P;
		break;
	case VFE_MSG_OUTPUT_S:
		pp_key = PP_SNAP;
		image_mode = MSM_V4L2_EXT_CAPTURE_MODE_MAIN;
		path = OUTPUT_TYPE_S;
		break;
	case VFE_MSG_OUTPUT_T:
	default:
	  path = OUTPUT_TYPE_T;
		break;
	}
	spin_lock_irqsave(&p_mctl->pp_info.lock, flags);
	if (p_mctl->pp_info.pp_key & pp_key) {
		int rc = 0;
		struct msm_cam_evt_divert_frame div;
		struct msm_frame_buffer *vb = NULL;
		struct videobuf2_contig_pmem *mem;

		p_mctl->pp_info.cur_frame_id[image_mode]++;
		if (!p_mctl->pp_info.cur_frame_id[image_mode])
			p_mctl->pp_info.cur_frame_id[image_mode] = 1;
		if (!p_mctl->pp_info.div_frame[image_mode].paddr) {
			/* no frame in postproc. good to divert the frame up */
			memset(&div, 0, sizeof(div));
			vb = msm_mctl_buf_find(p_mctl, pcam_inst,
					del_buf, msg_type, y_phy);
			if (!vb) {
				spin_unlock_irqrestore(&p_mctl->pp_info.lock,
					flags);
				return -EINVAL;
			}
			vb->vidbuf.v4l2_buf.sequence = frame_id;
			mem = vb2_plane_cookie(&vb->vidbuf, 0);
			div.image_mode = pcam_inst->image_mode;
			div.op_mode    = pcam_inst->pcam->op_mode;
			div.inst_idx   = pcam_inst->my_index;
			div.node_idx   = pcam_inst->pcam->vnode_id;
			div.phy_addr   =
				videobuf2_to_pmem_contig(&vb->vidbuf, 0);
			div.phy_offset = mem->addr_offset;
			div.y_off      = mem->y_off;
			div.cbcr_off   = mem->cbcr_off;
			div.fd         = (int)mem->vaddr;
			div.frame_id   =
				p_mctl->pp_info.cur_frame_id[image_mode];
			div.path       = path;
			div.length     = mem->size;
			msm_mctl_gettimeofday(&div.timestamp);
			vb->vidbuf.v4l2_buf.timestamp = div.timestamp;
			p_mctl->pp_info.div_frame[image_mode].paddr =
				div.phy_addr;
			p_mctl->pp_info.div_frame[image_mode].y_off =
				div.y_off;
			p_mctl->pp_info.div_frame[image_mode].cbcr_off =
				div.cbcr_off;
			rc = msm_mctl_buf_divert(p_mctl, pcam_inst, &div);
			spin_unlock_irqrestore(&p_mctl->pp_info.lock,
				flags);
			return rc;
		}
	}
	spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);
	return msm_mctl_buf_done_proc(p_mctl, pcam_inst,
							msg_type, y_phy,
							&frame_id, 1);
}

int msm_mctl_buf_init(struct msm_cam_v4l2_device *pcam)
{
	pcam->mctl.mctl_vidbuf_init = msm_vidbuf_init;
	return 0;
}

int msm_mctl_reserve_free_buf(
		struct msm_cam_media_controller *pmctl,
		int msg_type, struct msm_free_buf *free_buf)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct videobuf2_contig_pmem *mem;
	unsigned long flags = 0;
	struct msm_frame_buffer *buf = NULL;
	int rc = -EINVAL, idx;

	if (!free_buf) {
		pr_err("%s: free_buf= null\n", __func__);
		return rc;
	}
	memset(free_buf, 0, sizeof(struct msm_free_buf));
	idx = msm_mctl_out_type_to_inst_index(pmctl->sync.pcam_sync,
		msg_type);
	pcam_inst = pmctl->sync.pcam_sync->dev_inst[idx];
	if (!pcam_inst->streamon) {
		D("%s: stream 0x%p is off\n", __func__, pcam_inst);
		return rc;
	}
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	if (!list_empty(&pcam_inst->free_vq)) {
		list_for_each_entry(buf, &pcam_inst->free_vq, list) {
			if (buf->inuse == 0) {
				mem = vb2_plane_cookie(&buf->vidbuf, 0);
				if (!mem)
					continue;
				free_buf->paddr =
				(uint32_t) videobuf2_to_pmem_contig(
					&buf->vidbuf, 0);
				free_buf->y_off = mem->y_off;
				free_buf->cbcr_off = mem->cbcr_off;
				D("%s idx=%d,inst=0x%p,idx=%d,paddr=0x%x, "
					"y_off=%d,cbcroff=%d\n", __func__, idx,
					pcam_inst, buf->vidbuf.v4l2_buf.index,
					free_buf->paddr, free_buf->y_off,
					free_buf->cbcr_off);
				/* mark it used */
				buf->inuse = 1;
				rc = 0;
				break;
			}
		}
	}
	if (rc != 0)
		D("%s:No free buffer available: inst = 0x%p ",
				__func__, pcam_inst);
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	return rc;
}

int msm_mctl_release_free_buf(struct msm_cam_media_controller *pmctl,
				int msg_type, struct msm_free_buf *free_buf)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	unsigned long flags = 0;
	struct msm_frame_buffer *buf = NULL;
	uint32_t buf_phyaddr = 0;
	int rc = -EINVAL, idx;

	if (!free_buf)
		return rc;

	idx = msm_mctl_out_type_to_inst_index(pmctl->sync.pcam_sync, msg_type);
	pcam_inst = pmctl->sync.pcam_sync->dev_inst[idx];
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	list_for_each_entry(buf, &pcam_inst->free_vq, list) {
		buf_phyaddr =
			(uint32_t) videobuf2_to_pmem_contig(&buf->vidbuf, 0);
		if (free_buf->paddr == buf_phyaddr) {
			D("%s buf = 0x%x ", __func__, free_buf->paddr);
			/* mark it free */
			buf->inuse = 0;
			rc = 0;
			break;
		}
	}

	if (rc != 0)
		pr_err("%s invalid buffer address ", __func__);

	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	return rc;
}

int msm_mctl_buf_done_pp(
		struct msm_cam_media_controller *pmctl,
		int msg_type, struct msm_free_buf *frame, int dirty)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	int idx, rc = 0;

	idx = msm_mctl_out_type_to_inst_index(
		pmctl->sync.pcam_sync, msg_type);
	pcam_inst = pmctl->sync.pcam_sync->dev_inst[idx];
	D("%s:inst=0x%p, paddr=0x%x, dirty=%d",
		__func__, pcam_inst, frame->paddr, dirty);
	if (dirty)
		/* the frame is dirty, not going to disptach to app */
		rc = msm_mctl_release_free_buf(pmctl, msg_type, frame);
	else
		rc = msm_mctl_buf_done_proc(pmctl, pcam_inst,
			msg_type, frame->paddr, NULL, 0);
	return rc;
}

