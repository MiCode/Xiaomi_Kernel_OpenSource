/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

static int msm_vb2_ops_queue_setup(struct vb2_queue *vq,
					unsigned int *num_buffers,
					unsigned int *num_planes,
					unsigned long sizes[],
					void *alloc_ctxs[])
{
	/* get the video device */
	struct msm_cam_v4l2_dev_inst *pcam_inst = vb2_get_drv_priv(vq);
	struct msm_cam_v4l2_device *pcam = pcam_inst->pcam;
	int i;

	D("%s\n", __func__);
	if (!pcam || !(*num_buffers)) {
		pr_err("%s error : invalid input\n", __func__);
		return -EINVAL;
	}

	*num_planes = pcam_inst->plane_info.num_planes;
	for (i = 0; i < pcam_inst->vid_fmt.fmt.pix_mp.num_planes; i++) {
		sizes[i] = PAGE_ALIGN(pcam_inst->plane_info.plane[i].size);
		D("%s Inst %p : Plane %d Offset = %d Size = %ld"
			"Aligned Size = %ld", __func__, pcam_inst, i,
			pcam_inst->plane_info.plane[i].offset,
			pcam_inst->plane_info.plane[i].size, sizes[i]);
	}
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
	uint32_t buf_idx;
	struct msm_frame_buffer *buf;
	int rc = 0, i;
	enum videobuf2_buffer_type buf_type;
	struct videobuf2_msm_offset offset;
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

	buf = container_of(vb, struct msm_frame_buffer, vidbuf);
	if (buf->state == MSM_BUFFER_STATE_INITIALIZED)
		return rc;

	if (pcam_inst->plane_info.buffer_type ==
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		buf_type = VIDEOBUF2_MULTIPLE_PLANES;
	else if (pcam_inst->plane_info.buffer_type ==
		V4L2_BUF_TYPE_VIDEO_CAPTURE)
		buf_type = VIDEOBUF2_SINGLE_PLANE;
	else
		return -EINVAL;

	if (buf_type == VIDEOBUF2_SINGLE_PLANE) {
		offset.sp_off.y_off = pcam_inst->plane_info.sp_y_offset;
		offset.sp_off.cbcr_off =
			pcam_inst->plane_info.plane[0].offset;
	}
	buf_idx = vb->v4l2_buf.index;
	for (i = 0; i < vb->num_planes; i++) {
		mem = vb2_plane_cookie(vb, i);
		if (buf_type == VIDEOBUF2_MULTIPLE_PLANES)
			offset.data_offset =
				pcam_inst->plane_info.plane[i].offset;

		if (vb->v4l2_buf.memory == V4L2_MEMORY_USERPTR)
			rc = videobuf2_pmem_contig_user_get(mem, &offset,
				buf_type,
				pcam_inst->buf_offset[buf_idx][i].addr_offset,
				pcam_inst->path, pcam->mctl.client);
		else
			rc = videobuf2_pmem_contig_mmap_get(mem, &offset,
				buf_type, pcam_inst->path);
		if (rc < 0) {
			pr_err("%s error initializing buffer ",
				__func__);
			return rc;
		}
	}
	buf->state = MSM_BUFFER_STATE_INITIALIZED;
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
	buf->state = MSM_BUFFER_STATE_PREPARED;
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
	buf->state = MSM_BUFFER_STATE_DEQUEUED;
	D("%s: inst=0x%x, buf=0x, %x, idx=%d\n", __func__,
	(uint32_t)pcam_inst, (uint32_t)buf, vb->v4l2_buf.index);
	D("%s: inst=%p, buf=%x, idx=%d\n", __func__,
	pcam_inst, (uint32_t)buf, vb->v4l2_buf.index);
	return 0;
}

static void msm_vb2_ops_buf_cleanup(struct vb2_buffer *vb)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct msm_cam_v4l2_device *pcam;
	struct videobuf2_contig_pmem *mem;
	struct msm_frame_buffer *buf, *tmp;
	uint32_t i, vb_phyaddr = 0, buf_phyaddr = 0;
	unsigned long flags = 0;

	pcam_inst = vb2_get_drv_priv(vb->vb2_queue);
	pcam = pcam_inst->pcam;
	buf = container_of(vb, struct msm_frame_buffer, vidbuf);

	if (pcam_inst->vid_fmt.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		for (i = 0; i < vb->num_planes; i++) {
			mem = vb2_plane_cookie(vb, i);
			if (!mem) {
				D("%s Inst %p memory already freed up. return",
					__func__, pcam_inst);
				return;
			}
			D("%s: inst=%p, buf=0x%x, idx=%d plane id = %d\n",
				__func__, pcam_inst,
				(uint32_t)buf, vb->v4l2_buf.index, i);

			spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
			list_for_each_entry_safe(buf, tmp,
					&pcam_inst->free_vq, list) {
				if (&buf->vidbuf == vb) {
					list_del_init(&buf->list);
					break;
				}
			}
			spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
		}
	} else {
		mem = vb2_plane_cookie(vb, 0);
		if (!mem)
			return;
		D("%s: inst=0x%x, buf=0x%x, idx=%d\n", __func__,
		(uint32_t)pcam_inst, (uint32_t)buf, vb->v4l2_buf.index);
		vb_phyaddr = (unsigned long) videobuf2_to_pmem_contig(vb, 0);
		spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
		list_for_each_entry_safe(buf, tmp,
				&pcam_inst->free_vq, list) {
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
		spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	}
	for (i = 0; i < vb->num_planes; i++) {
		mem = vb2_plane_cookie(vb, i);
		videobuf2_pmem_contig_user_put(mem, pcam->mctl.client);
	}
	buf->state = MSM_BUFFER_STATE_UNUSED;
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
	unsigned long flags = 0;
	struct vb2_queue *vq = vb->vb2_queue;
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
	D("%s pcam_inst=%p, idx=%d\n", __func__, pcam_inst,
		vb->v4l2_buf.index);
	buf = container_of(vb, struct msm_frame_buffer, vidbuf);
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	/* we are returning a buffer to the queue */
	list_add_tail(&buf->list, &pcam_inst->free_vq);
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	buf->state = MSM_BUFFER_STATE_QUEUED;
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
static int msm_vbqueue_init(struct msm_cam_v4l2_dev_inst *pcam_inst,
			struct vb2_queue *q, enum v4l2_buf_type type)
{
	if (!q) {
		pr_err("%s error : input is NULL\n", __func__);
		return -EINVAL;
	}

	spin_lock_init(&pcam_inst->vq_irqlock);
	INIT_LIST_HEAD(&pcam_inst->free_vq);
	videobuf2_queue_pmem_contig_init(q, type,
					&msm_vb2_ops,
					sizeof(struct msm_frame_buffer),
					(void *)pcam_inst);
	return 0;
}

int msm_mctl_img_mode_to_inst_index(struct msm_cam_media_controller *pmctl,
					int image_mode, int node_type)
{
	if ((image_mode >= 0) && node_type &&
		pmctl->sync.pcam_sync->mctl_node.dev_inst_map[image_mode])
		return pmctl->sync.pcam_sync->
				mctl_node.dev_inst_map[image_mode]->my_index;
	else if ((image_mode >= 0) &&
		pmctl->sync.pcam_sync->dev_inst_map[image_mode])
		return	pmctl->sync.pcam_sync->
				dev_inst_map[image_mode]->my_index;
	else
		return -EINVAL;
}

void msm_mctl_gettimeofday(struct timeval *tv)
{
	struct timespec ts;

	BUG_ON(!tv);

	ktime_get_ts(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec/1000;
}

struct msm_frame_buffer *msm_mctl_buf_find(
	struct msm_cam_media_controller *pmctl,
	struct msm_cam_v4l2_dev_inst *pcam_inst, int del_buf,
	int image_mode, struct msm_free_buf *fbuf)
{
	struct msm_frame_buffer *buf = NULL, *tmp;
	uint32_t buf_phyaddr = 0;
	unsigned long flags = 0;
	uint32_t buf_idx, offset = 0;
	struct videobuf2_contig_pmem *mem;

	/* we actually need a list, not a queue */
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	list_for_each_entry_safe(buf, tmp,
			&pcam_inst->free_vq, list) {
		buf_idx = buf->vidbuf.v4l2_buf.index;
		mem = vb2_plane_cookie(&buf->vidbuf, 0);
		if (mem->buffer_type ==	VIDEOBUF2_MULTIPLE_PLANES)
			offset = mem->offset.data_offset +
				pcam_inst->buf_offset[buf_idx][0].data_offset;
		else
			offset = mem->offset.sp_off.y_off;
		buf_phyaddr = (unsigned long)
				videobuf2_to_pmem_contig(&buf->vidbuf, 0) +
				offset;
		D("%s vb_idx=%d,vb_paddr=0x%x ch0=0x%x\n",
			__func__, buf->vidbuf.v4l2_buf.index,
			buf_phyaddr, fbuf->ch_paddr[0]);
		if (fbuf->ch_paddr[0] == buf_phyaddr) {
			if (del_buf)
				list_del_init(&buf->list);
			spin_unlock_irqrestore(&pcam_inst->vq_irqlock,
								flags);
			buf->state = MSM_BUFFER_STATE_RESERVED;
			return buf;
		}
	}
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	return NULL;
}

int msm_mctl_buf_done_proc(
		struct msm_cam_media_controller *pmctl,
		struct msm_cam_v4l2_dev_inst *pcam_inst,
		int image_mode, struct msm_free_buf *fbuf,
		uint32_t *frame_id, int gen_timestamp)
{
	struct msm_frame_buffer *buf = NULL;
	int del_buf = 1;

	buf = msm_mctl_buf_find(pmctl, pcam_inst, del_buf,
					image_mode, fbuf);
	if (!buf) {
		pr_err("%s: buf=0x%x not found\n",
			__func__, fbuf->ch_paddr[0]);
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


int msm_mctl_buf_done(struct msm_cam_media_controller *p_mctl,
			int image_mode, struct msm_free_buf *fbuf,
			uint32_t frame_id)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	int idx, rc;
	int pp_divert_type = 0, pp_type = 0;

	msm_mctl_check_pp(p_mctl, image_mode, &pp_divert_type, &pp_type);
	D("%s: pp_type=%d, pp_divert_type = %d, frame_id = 0x%x",
		__func__, pp_type, pp_divert_type, frame_id);
	if (pp_type || pp_divert_type)
		rc = msm_mctl_do_pp_divert(p_mctl,
		image_mode, fbuf, frame_id, pp_type);
	else {
		idx = msm_mctl_img_mode_to_inst_index(
				p_mctl, image_mode, 0);
		if (idx < 0) {
			pr_err("%s Invalid instance, dropping buffer\n",
				__func__);
			return idx;
		}
		pcam_inst = p_mctl->sync.pcam_sync->dev_inst[idx];
		rc = msm_mctl_buf_done_proc(p_mctl, pcam_inst,
				image_mode, fbuf,
				&frame_id, 1);
	}
	return rc;
}

int msm_mctl_buf_init(struct msm_cam_v4l2_device *pcam)
{
	pcam->mctl.mctl_vbqueue_init = msm_vbqueue_init;
	return 0;
}

static int is_buffer_queued(struct msm_cam_v4l2_device *pcam, int image_mode)
{
	int idx;
	int ret = 0;
	struct msm_frame_buffer *buf = NULL;
	struct msm_cam_v4l2_dev_inst *pcam_inst = NULL;
	idx = pcam->mctl_node.dev_inst_map[image_mode]->my_index;
	pcam_inst = pcam->mctl_node.dev_inst[idx];
	list_for_each_entry(buf, &pcam_inst->free_vq, list) {
		if (buf->state != MSM_BUFFER_STATE_QUEUED)
			continue;
		ret = 1;
	}
	return ret;
}

struct msm_cam_v4l2_dev_inst *msm_mctl_get_pcam_inst(
				struct msm_cam_media_controller *pmctl,
				int image_mode)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst = NULL;
	struct msm_cam_v4l2_device *pcam = pmctl->sync.pcam_sync;
	int idx;

	if (image_mode >= 0) {
		/* Valid image mode. Search the mctl node first.
		 * If mctl node doesnt have the instance, then
		 * search in the user's video node */
		if (pmctl->vfe_output_mode == VFE_OUTPUTS_MAIN_AND_THUMB
		|| pmctl->vfe_output_mode == VFE_OUTPUTS_THUMB_AND_MAIN) {
			if (pcam->mctl_node.dev_inst_map[image_mode]
			&& is_buffer_queued(pcam, image_mode)) {
				idx =
				pcam->mctl_node.dev_inst_map[image_mode]
				->my_index;
				pcam_inst = pcam->mctl_node.dev_inst[idx];
				D("%s Found instance %p in mctl node device\n",
				  __func__, pcam_inst);
			} else if (pcam->dev_inst_map[image_mode]) {
				idx = pcam->dev_inst_map[image_mode]->my_index;
				pcam_inst = pcam->dev_inst[idx];
				D("%s Found instance %p in video device",
				__func__, pcam_inst);
			}
		} else {
			if (pcam->mctl_node.dev_inst_map[image_mode]) {
				idx = pcam->mctl_node.dev_inst_map[image_mode]
				->my_index;
				pcam_inst = pcam->mctl_node.dev_inst[idx];
				D("%s Found instance %p in mctl node device\n",
				__func__, pcam_inst);
			} else if (pcam->dev_inst_map[image_mode]) {
				idx = pcam->dev_inst_map[image_mode]->my_index;
				pcam_inst = pcam->dev_inst[idx];
				D("%s Found instance %p in video device",
				__func__, pcam_inst);
			}
		}
	} else
		pr_err("%s Invalid image mode %d. Return NULL\n",
			__func__, image_mode);
	return pcam_inst;
}

int msm_mctl_reserve_free_buf(
		struct msm_cam_media_controller *pmctl,
		struct msm_cam_v4l2_dev_inst *pref_pcam_inst,
		int image_mode, struct msm_free_buf *free_buf)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst = pref_pcam_inst;
	unsigned long flags = 0;
	struct videobuf2_contig_pmem *mem;
	struct msm_frame_buffer *buf = NULL;
	int rc = -EINVAL, i;
	uint32_t buf_idx, plane_offset = 0;

	if (!free_buf || !pmctl) {
		pr_err("%s: free_buf/pmctl is null\n", __func__);
		return rc;
	}
	memset(free_buf, 0, sizeof(struct msm_free_buf));

	/* If the caller wants to reserve a buffer from a particular
	 * camera instance, he would send the preferred camera instance.
	 * If the preferred camera instance is NULL, get the
	 * camera instance using the image mode passed */
	if (!pcam_inst)
		pcam_inst = msm_mctl_get_pcam_inst(pmctl, image_mode);

	if (!pcam_inst || !pcam_inst->streamon) {
		pr_err("%s: stream is turned off\n", __func__);
		return rc;
	}
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	list_for_each_entry(buf, &pcam_inst->free_vq, list) {
		if (buf->state != MSM_BUFFER_STATE_QUEUED)
			continue;

		buf_idx = buf->vidbuf.v4l2_buf.index;
		if (pcam_inst->vid_fmt.type ==
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			free_buf->num_planes =
				pcam_inst->plane_info.num_planes;
			for (i = 0; i < free_buf->num_planes; i++) {
				mem = vb2_plane_cookie(&buf->vidbuf, i);
				if (mem->buffer_type ==
						VIDEOBUF2_MULTIPLE_PLANES)
					plane_offset =
					mem->offset.data_offset;
				else
					plane_offset =
					mem->offset.sp_off.cbcr_off;

				free_buf->ch_paddr[i] =	(uint32_t)
				videobuf2_to_pmem_contig(&buf->vidbuf, i) +
				pcam_inst->buf_offset[buf_idx][i].data_offset +
				plane_offset;

			}
		} else {
			mem = vb2_plane_cookie(&buf->vidbuf, 0);
			free_buf->ch_paddr[0] = (uint32_t)
				videobuf2_to_pmem_contig(&buf->vidbuf, 0) +
				mem->offset.sp_off.y_off;
			free_buf->ch_paddr[1] =	free_buf->ch_paddr[0] +
				mem->offset.sp_off.cbcr_off;
		}
		free_buf->vb = (uint32_t)buf;
		buf->state = MSM_BUFFER_STATE_RESERVED;
		D("%s inst=0x%p, idx=%d, paddr=0x%x, "
			"ch1 addr=0x%x\n", __func__,
			pcam_inst, buf->vidbuf.v4l2_buf.index,
			free_buf->ch_paddr[0], free_buf->ch_paddr[1]);
		rc = 0;
		break;
	}
	if (rc != 0)
		D("%s:No free buffer available: inst = 0x%p ",
				__func__, pcam_inst);
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	return rc;
}

int msm_mctl_release_free_buf(struct msm_cam_media_controller *pmctl,
				struct msm_cam_v4l2_dev_inst *pcam_inst,
				int image_mode, struct msm_free_buf *free_buf)
{
	unsigned long flags = 0;
	struct msm_frame_buffer *buf = NULL;
	uint32_t buf_phyaddr = 0;
	int rc = -EINVAL;

	if (!free_buf)
		return rc;

	if (!pcam_inst) {
		pr_err("%s Invalid instance, buffer not released\n",
			__func__);
		return rc;
	}

	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	list_for_each_entry(buf, &pcam_inst->free_vq, list) {
		buf_phyaddr =
			(uint32_t) videobuf2_to_pmem_contig(&buf->vidbuf, 0);
		if (free_buf->ch_paddr[0] == buf_phyaddr) {
			D("%s buf = 0x%x ", __func__, free_buf->ch_paddr[0]);
			buf->state = MSM_BUFFER_STATE_UNUSED;
			rc = 0;
			break;
		}
	}

	if (rc != 0)
		pr_err("%s invalid buffer address ", __func__);

	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	return rc;
}

int msm_mctl_buf_done_pp(struct msm_cam_media_controller *pmctl,
	int image_mode, struct msm_free_buf *frame, int dirty, int node_type)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	int rc = 0, idx;

	idx = msm_mctl_img_mode_to_inst_index(pmctl, image_mode, node_type);
	if (idx < 0) {
		pr_err("%s Invalid instance, buffer not released\n", __func__);
		return idx;
	}
	if (node_type)
		pcam_inst = pmctl->sync.pcam_sync->mctl_node.dev_inst[idx];
	else
		pcam_inst = pmctl->sync.pcam_sync->dev_inst[idx];
	if (!pcam_inst) {
		pr_err("%s Invalid instance, cannot send buf to user",
			__func__);
		return -EINVAL;
	}

	D("%s:inst=0x%p, paddr=0x%x, dirty=%d",
		__func__, pcam_inst, frame->ch_paddr[0], dirty);
	if (dirty)
		/* the frame is dirty, not going to disptach to app */
		rc = msm_mctl_release_free_buf(pmctl, pcam_inst,
						image_mode, frame);
	else
		rc = msm_mctl_buf_done_proc(pmctl, pcam_inst,
			image_mode, frame, NULL, 0);
	return rc;
}

struct msm_frame_buffer *msm_mctl_get_free_buf(
		struct msm_cam_media_controller *pmctl,
		int image_mode)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	unsigned long flags = 0;
	struct msm_frame_buffer *buf = NULL;
	int rc = -EINVAL, idx;

	idx = msm_mctl_img_mode_to_inst_index(pmctl,
		image_mode, 0);
	if (idx < 0) {
		pr_err("%s Invalid instance, cant get buffer\n", __func__);
		return NULL;
	}
	pcam_inst = pmctl->sync.pcam_sync->dev_inst[idx];
	if (!pcam_inst->streamon) {
		D("%s: stream 0x%p is off\n", __func__, pcam_inst);
		return NULL;
	}
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	if (!list_empty(&pcam_inst->free_vq)) {
		list_for_each_entry(buf, &pcam_inst->free_vq, list) {
			if (buf->state == MSM_BUFFER_STATE_QUEUED) {
				buf->state = MSM_BUFFER_STATE_RESERVED;
				rc = 0;
				break;
			}
		}
	}
	if (rc != 0) {
		D("%s:No free buffer available: inst = 0x%p ",
				__func__, pcam_inst);
		buf = NULL;
	}
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	return buf;
}

int msm_mctl_put_free_buf(
		struct msm_cam_media_controller *pmctl,
		int image_mode, struct msm_frame_buffer *my_buf)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	unsigned long flags = 0;
	int rc = 0, idx;
	struct msm_frame_buffer *buf = NULL;

	idx = msm_mctl_img_mode_to_inst_index(pmctl,
		image_mode, 0);
	if (idx < 0) {
		pr_err("%s Invalid instance, cant put buffer\n", __func__);
		return idx;
	}
	pcam_inst = pmctl->sync.pcam_sync->dev_inst[idx];
	if (!pcam_inst->streamon) {
		D("%s: stream 0x%p is off\n", __func__, pcam_inst);
		return rc;
	}
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	if (!list_empty(&pcam_inst->free_vq)) {
		list_for_each_entry(buf, &pcam_inst->free_vq, list) {
			if (my_buf == buf) {
				buf->state = MSM_BUFFER_STATE_QUEUED;
				spin_unlock_irqrestore(&pcam_inst->vq_irqlock,
					flags);
				return 0;
			}
		}
	}
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	return rc;
}

int msm_mctl_buf_del(struct msm_cam_media_controller *pmctl,
	int image_mode,
	struct msm_frame_buffer *my_buf)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct msm_frame_buffer *buf = NULL;
	unsigned long flags = 0;
	int idx;

	idx = msm_mctl_img_mode_to_inst_index(pmctl,
		image_mode, 0);
	if (idx < 0) {
		pr_err("%s Invalid instance, cant delete buffer\n", __func__);
		return idx;
	}
	pcam_inst = pmctl->sync.pcam_sync->dev_inst[idx];
	D("%s: idx = %d, pinst=0x%p", __func__, idx, pcam_inst);
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	if (!list_empty(&pcam_inst->free_vq)) {
		list_for_each_entry(buf, &pcam_inst->free_vq, list) {
			if (my_buf == buf) {
				list_del_init(&buf->list);
				spin_unlock_irqrestore(&pcam_inst->vq_irqlock,
					flags);
				return 0;
			}
		}
	}
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	pr_err("%s: buf 0x%p not found", __func__, my_buf);
	return -EINVAL;
}

int msm_mctl_buf_return_buf(struct msm_cam_media_controller *pmctl,
			int image_mode, struct msm_frame_buffer *rbuf)
{
	int idx = 0;
	struct msm_frame_buffer *buf = NULL;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct msm_cam_v4l2_device *pcam = pmctl->sync.pcam_sync;
	unsigned long flags = 0;

	if (pcam->mctl_node.dev_inst_map[image_mode]) {
		idx = pcam->mctl_node.dev_inst_map[image_mode]->my_index;
		pcam_inst = pcam->mctl_node.dev_inst[idx];
		D("%s Found instance %p in mctl node device\n",
			__func__, pcam_inst);
	} else {
		pr_err("%s Invalid image mode %d ", __func__, image_mode);
		return -EINVAL;
	}

	if (!pcam_inst) {
		pr_err("%s Invalid instance\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	if (!list_empty(&pcam_inst->free_vq)) {
		list_for_each_entry(buf, &pcam_inst->free_vq, list) {
			if (rbuf == buf) {
				D("%s Return buffer %x in pcam_inst %p ",
				__func__, (int)rbuf, pcam_inst);
				buf->state = MSM_BUFFER_STATE_QUEUED;
				spin_unlock_irqrestore(&pcam_inst->vq_irqlock,
					flags);
				return 0;
			}
		}
	}
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	return -EINVAL;
}
