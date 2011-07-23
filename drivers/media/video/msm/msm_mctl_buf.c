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
			size = PAD_TO_2K(width * height, 1)
				+ PAD_TO_2K(width * height/2, 1);
			*cbcroffset = PAD_TO_2K(width * height, 1);
		} else {
			size = PAD_TO_WORD(width * height)
				+ PAD_TO_WORD(width * height/2);
			*cbcroffset = PAD_TO_WORD(width * height);
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
	pcam_inst = vb2_get_drv_priv(vb->vb2_queue);
	pcam = pcam_inst->pcam;
	buf = container_of(vb, struct msm_frame_buffer, vidbuf);
	D("%s: inst=0x%x, buf=0x%x, idx=%d\n", __func__,
	(uint32_t)pcam_inst, (uint32_t)buf, vb->v4l2_buf.index);
	vb_phyaddr = (unsigned long) videobuf2_to_pmem_contig(vb, 0);
	list_for_each_entry(buf, &pcam_inst->free_vq, list) {
		buf_phyaddr = (unsigned long)
				videobuf2_to_pmem_contig(&buf->vidbuf, 0);
		D("%s vb_idx=%d,vb_paddr=0x%x,phyaddr=0x%x\n",
			__func__, buf->vidbuf.v4l2_buf.index,
			buf_phyaddr, vb_phyaddr);
		if (vb_phyaddr == buf_phyaddr) {
			list_del(&buf->list);
			break;
		}
	}
	for (i = 0; i < vb->num_planes; i++) {
		mem = vb2_plane_cookie(vb, i);
		videobuf2_pmem_contig_user_put(mem);
	}
}

static int msm_vb2_ops_start_streaming(struct vb2_queue *q)
{
	return 0;
}

static int msm_vb2_ops_stop_streaming(struct vb2_queue *q)
{
	return 0;
}

static void msm_vb2_ops_buf_queue(struct vb2_buffer *vb)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst = NULL;
	struct msm_cam_v4l2_device *pcam = NULL;
	unsigned long phyaddr = 0;
	unsigned long flags = 0;
	int rc;
	struct vb2_queue *vq = vb->vb2_queue;
	struct msm_frame frame;
	struct msm_vfe_cfg_cmd cfgcmd;
	struct videobuf2_contig_pmem *mem;
	struct msm_frame_buffer *buf;
	D("%s\n", __func__);
	if (!vb || !vq) {
		pr_err("%s error : input is NULL\n", __func__);
		return ;
	}
	pcam_inst = vb2_get_drv_priv(vq);
	pcam = pcam_inst->pcam;
	D("%s pcam_inst=%p,(vb=0x%p),idx=%d,len=%d\n", __func__, pcam_inst,
	vb, vb->v4l2_buf.index, vb->v4l2_buf.length);
	buf = container_of(vb, struct msm_frame_buffer, vidbuf);
	/* we are returning a buffer to the queue */
	mem = vb2_plane_cookie(vb, 0);
	/* get the physcial address of the buffer */
	phyaddr = (unsigned long) videobuf2_to_pmem_contig(vb, 0);
	D("%s buffer type is %d\n", __func__, mem->buffer_type);
	frame.path = pcam_inst->path;
	frame.buffer = 0;
	frame.y_off = mem->y_off;
	frame.cbcr_off = mem->cbcr_off;
	/* now release frame to vfe */
	cfgcmd.cmd_type = CMD_FRAME_BUF_RELEASE;
	cfgcmd.value    = (void *)&frame;
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	list_add_tail(&buf->list, &pcam_inst->free_vq);
	/* TBD: need to remove. VFE is going to call
	   msm_mctl_fetch_free_buf to get free buf.
	   Work with Shuzhen to hash out details tomorrow */
	rc = msm_isp_subdev_ioctl(&pcam->mctl.isp_sdev->sd,
			&cfgcmd, &phyaddr);
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

static int msm_mctl_get_pcam_inst_idx(int out_put_type)
{
	switch (out_put_type) {
	case OUTPUT_TYPE_T:
		return MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL;
	case OUTPUT_TYPE_S:
		return MSM_V4L2_EXT_CAPTURE_MODE_MAIN;
	case OUTPUT_TYPE_V:
		return MSM_V4L2_EXT_CAPTURE_MODE_VIDEO;
	case OUTPUT_TYPE_P:
	  return MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW;
	default:
		return 0;
	}
}


int msm_mctl_buf_done(struct msm_cam_media_controller *pmctl,
					int msg_type, uint32_t y_phy)
{
	struct msm_frame_buffer *buf = NULL;
	uint32_t buf_phyaddr = 0;
	int idx, got = 0;
	unsigned long flags = 0;
	struct msm_cam_v4l2_dev_inst *pcam_inst;

	idx = msm_mctl_out_type_to_inst_index(pmctl->sync.pcam_sync, msg_type);
	pcam_inst = pmctl->sync.pcam_sync->dev_inst[idx];
	/* we actually need a list, not a queue */
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	list_for_each_entry(buf, &pcam_inst->free_vq, list) {
		buf_phyaddr = (unsigned long)
				videobuf2_to_pmem_contig(&buf->vidbuf, 0);
		D("%s vb_idx=%d,vb_paddr=0x%x,y_phy=0x%x\n",
			__func__, buf->vidbuf.v4l2_buf.index,
			buf_phyaddr, y_phy);
		if (y_phy == buf_phyaddr) {
			got = 1;
			break;
		}
	}
	if (!got) {
		spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
		pr_err("%s: buf_done addr 0x%x != free buf queue head 0x%x\n",
			__func__, (uint32_t)y_phy, (uint32_t)buf_phyaddr);
		return -EINVAL;
	}
	list_del(&buf->list);
	buf->vidbuf.v4l2_buf.sequence++;
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	do_gettimeofday(&buf->vidbuf.v4l2_buf.timestamp);
	vb2_buffer_done(&buf->vidbuf, VB2_BUF_STATE_DONE);
	return 0;
}

int msm_mctl_buf_init(struct msm_cam_v4l2_device *pcam)
{
	pcam->mctl.mctl_vidbuf_init = msm_vidbuf_init;
	return 0;
}

int msm_mctl_fetch_free_buf(struct msm_cam_media_controller *pmctl,
				int path, struct msm_free_buf *free_buf)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct videobuf2_contig_pmem *mem;
	int idx = msm_mctl_get_pcam_inst_idx(path);
	unsigned long flags = 0;
	struct msm_frame_buffer *buf = NULL;
	int rc = -1;

	pcam_inst = pmctl->sync.pcam_sync->dev_inst[idx];
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	list_for_each_entry(buf, &pcam_inst->free_vq, list) {
		if (buf->inuse == 0) {
			mem = vb2_plane_cookie(&buf->vidbuf, 0);
			free_buf->paddr =
			(uint32_t) videobuf2_to_pmem_contig(&buf->vidbuf, 0);
			free_buf->y_off = mem->y_off;
			free_buf->cbcr_off = mem->cbcr_off;
			D("%s path=%d,inst=0x%p,idx=%d,paddr=0x%x, "
				"y_off=%d,cbcroff=%d\n", __func__, path,
				pcam_inst, buf->vidbuf.v4l2_buf.index,
				free_buf->paddr, free_buf->y_off,
				free_buf->cbcr_off);
			/* mark it used */
			buf->inuse = 1;
			rc = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	return rc;
}
