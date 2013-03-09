/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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



#include "msm.h"
#include "msm_cam_server.h"
#include "msm_ispif.h"

#ifdef CONFIG_MSM_CAMERA_DEBUG
#define D(fmt, args...) pr_debug("msm_mctl_buf: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

static int msm_vb2_ops_queue_setup(struct vb2_queue *vq,
				const struct v4l2_format *fmt,
				unsigned int *num_buffers,
				unsigned int *num_planes,
				unsigned int sizes[],
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
		sizes[i] = pcam_inst->plane_info.plane[i].size;
		D("%s Inst %p : Plane %d Offset = %d Size = %ld"
			"Aligned Size = %d", __func__, pcam_inst, i,
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
	struct msm_cam_media_controller *pmctl;
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
	pmctl = msm_cam_server_get_mctl(pcam->mctl_handle);
	if (pmctl == NULL) {
		pr_err("%s No mctl found\n", __func__);
		return -EINVAL;
	}
	for (i = 0; i < vb->num_planes; i++) {
		mem = vb2_plane_cookie(vb, i);
		if (mem == NULL) {
			pr_err("%s Inst %p Buffer %d Plane %d cookie is null",
				__func__, pcam_inst, buf_idx, i);
			return -EINVAL;
		}
		if (buf_type == VIDEOBUF2_MULTIPLE_PLANES)
			offset.data_offset =
				pcam_inst->plane_info.plane[i].offset;

		if (vb->v4l2_buf.memory == V4L2_MEMORY_USERPTR)
			rc = videobuf2_pmem_contig_user_get(mem, &offset,
				buf_type,
				pcam_inst->buf_offset[buf_idx][i].addr_offset,
				pcam_inst->path, pmctl->client,
				pmctl->domain_num);
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
	struct vb2_queue *vq;

	D("%s\n", __func__);
	if (!vb || !vb->vb2_queue) {
		pr_err("%s error : input is NULL\n", __func__);
		return -EINVAL;
	}
	vq = vb->vb2_queue;
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
	D("%s: inst=0x%x, buf=0x%x, idx=%d\n", __func__,
	(uint32_t)pcam_inst, (uint32_t)buf, vb->v4l2_buf.index);
	return 0;
}

static void msm_vb2_ops_buf_cleanup(struct vb2_buffer *vb)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct msm_cam_media_controller *pmctl;
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
	pmctl = msm_cam_server_get_mctl(pcam->mctl_handle);
	if (pmctl == NULL) {
		pr_err("%s No mctl found\n", __func__);
		buf->state = MSM_BUFFER_STATE_UNUSED;
		return;
	}
	for (i = 0; i < vb->num_planes; i++) {
		mem = vb2_plane_cookie(vb, i);
		if (mem) {
			videobuf2_pmem_contig_user_put(mem, pmctl->client,
				pmctl->domain_num);
		} else {
			pr_err("%s Inst %p buffer plane cookie is null",
				__func__, pcam_inst);
			return;
		}
	}
	buf->state = MSM_BUFFER_STATE_UNUSED;
}

static int msm_vb2_ops_start_streaming(struct vb2_queue *q, unsigned int count)
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
	struct vb2_queue *vq;
	struct msm_frame_buffer *buf;
	D("%s\n", __func__);
	if (!vb || !vb->vb2_queue) {
		pr_err("%s error : input is NULL\n", __func__);
		return ;
	}
	vq = vb->vb2_queue;
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
		pmctl->pcam_ptr->mctl_node.dev_inst_map[image_mode])
		return pmctl->pcam_ptr->
				mctl_node.dev_inst_map[image_mode]->my_index;
	else if ((image_mode >= 0) &&
		pmctl->pcam_ptr->dev_inst_map[image_mode])
		return	pmctl->pcam_ptr->
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
	struct msm_free_buf *fbuf)
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
		if (mem == NULL) {
			pr_err("%s Inst %p plane cookie is null",
				__func__, pcam_inst);
			spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
			return NULL;
		}
		if (mem->buffer_type == VIDEOBUF2_MULTIPLE_PLANES)
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
		struct msm_free_buf *fbuf,
		uint32_t *frame_id,
		struct msm_cam_timestamp *cam_ts)
{
	struct msm_frame_buffer *buf = NULL;
	int del_buf = 1;

	buf = msm_mctl_buf_find(pmctl, pcam_inst, del_buf, fbuf);
	if (!buf) {
		pr_err("%s: buf=0x%x not found\n",
			__func__, fbuf->ch_paddr[0]);
		return -EINVAL;
	}
	if (!cam_ts->present) {
		if (frame_id)
			buf->vidbuf.v4l2_buf.sequence = *frame_id;
		msm_mctl_gettimeofday(
			&buf->vidbuf.v4l2_buf.timestamp);
	} else {
		D("%s Copying timestamp as %ld.%ld", __func__,
			cam_ts->timestamp.tv_sec, cam_ts->timestamp.tv_usec);
		buf->vidbuf.v4l2_buf.timestamp = cam_ts->timestamp;
		buf->vidbuf.v4l2_buf.sequence  = cam_ts->frame_id;
	}
	D("%s Notify user about buffer %d image_mode %d frame_id %d", __func__,
		buf->vidbuf.v4l2_buf.index, pcam_inst->image_mode,
		buf->vidbuf.v4l2_buf.sequence);
	vb2_buffer_done(&buf->vidbuf, VB2_BUF_STATE_DONE);
	return 0;
}


int msm_mctl_buf_done(struct msm_cam_media_controller *p_mctl,
	struct msm_cam_buf_handle *buf_handle,
	struct msm_free_buf *fbuf,
	uint32_t frame_id)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	int idx, rc;
	int pp_divert_type = 0, pp_type = 0;
	uint32_t image_mode;
	struct msm_cam_timestamp cam_ts;

	if (!p_mctl || !buf_handle || !fbuf) {
		pr_err("%s Invalid argument. ", __func__);
		return -EINVAL;
	}
	if (buf_handle->buf_lookup_type == BUF_LOOKUP_BY_IMG_MODE)
		image_mode = buf_handle->image_mode;
	else
		image_mode = GET_IMG_MODE(buf_handle->inst_handle);

	if (image_mode > MSM_V4L2_EXT_CAPTURE_MODE_MAX) {
		pr_err("%s Invalid image mode %d ", __func__, image_mode);
		return -EINVAL;
	}

	msm_mctl_check_pp(p_mctl, image_mode, &pp_divert_type, &pp_type);
	D("%s: pp_type=%d, pp_divert_type = %d, frame_id = 0x%x image_mode %d",
		__func__, pp_type, pp_divert_type, frame_id, image_mode);
	if (pp_type || pp_divert_type) {
		rc = msm_mctl_do_pp_divert(p_mctl, buf_handle,
			fbuf, frame_id, pp_type);
	} else {
		/* Find the instance on which vb2_buffer_done() needs to be
		 * called, so that the user can get the buffer.
		 * If the lookup type is
		 * - By instance handle:
		 *    Either mctl_pp inst idx or video inst idx should be set.
		 *    Try to get the MCTL_PP inst idx first, if its not set,
		 *    fall back to video inst idx. Once we get the inst idx,
		 *    get the pcam_inst from the corresponding dev_inst[] map.
		 *    If neither are set, its a serious error, trigger a BUG_ON.
		 * - By image mode:
		 *    Legacy usecase. Use the image mode and get the pcam_inst
		 *    from the video node.
		 */
		if (buf_handle->buf_lookup_type == BUF_LOOKUP_BY_INST_HANDLE) {
			idx = GET_MCTLPP_INST_IDX(buf_handle->inst_handle);
			if (idx > MSM_DEV_INST_MAX) {
				idx = GET_VIDEO_INST_IDX(
					buf_handle->inst_handle);
				BUG_ON(idx > MSM_DEV_INST_MAX);
				pcam_inst = p_mctl->pcam_ptr->dev_inst[idx];
			} else {
				pcam_inst = p_mctl->pcam_ptr->mctl_node.
					dev_inst[idx];
			}
		} else if (buf_handle->buf_lookup_type ==
				BUF_LOOKUP_BY_IMG_MODE) {
			idx = msm_mctl_img_mode_to_inst_index(p_mctl,
				buf_handle->image_mode, 0);
			if (idx < 0) {
				pr_err("%s Invalid idx %d ", __func__, idx);
				return -EINVAL;
			}
			pcam_inst = p_mctl->pcam_ptr->dev_inst[idx];
		} else {
			pr_err("%s Invalid buffer lookup type %d", __func__,
				buf_handle->buf_lookup_type);
			return -EINVAL;
		}
		if (!pcam_inst) {
			pr_err("%s Invalid instance, Dropping buffer. ",
				__func__);
			return -EINVAL;
		}
		memset(&cam_ts, 0, sizeof(cam_ts));
		rc = msm_mctl_buf_done_proc(p_mctl, pcam_inst,
			fbuf, &frame_id, &cam_ts);
	}
	return rc;
}

int msm_mctl_buf_init(struct msm_cam_v4l2_device *pcam)
{
	struct msm_cam_media_controller *pmctl;
	pmctl = msm_cam_server_get_mctl(pcam->mctl_handle);
	if (pmctl == NULL) {
		pr_err("%s No mctl found\n", __func__);
		return -EINVAL;
	}
	pmctl->mctl_vbqueue_init = msm_vbqueue_init;
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

struct msm_cam_v4l2_dev_inst *msm_mctl_get_inst_by_img_mode(
	struct msm_cam_media_controller *pmctl, uint32_t img_mode)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst = NULL;
	struct msm_cam_v4l2_device *pcam = pmctl->pcam_ptr;
	int idx;

		/* Valid image mode. Search the mctl node first.
		 * If mctl node doesnt have the instance, then
		 * search in the user's video node */
		if (pmctl->vfe_output_mode == VFE_OUTPUTS_MAIN_AND_THUMB
		|| pmctl->vfe_output_mode == VFE_OUTPUTS_THUMB_AND_MAIN) {
			if (pcam->mctl_node.dev_inst_map[img_mode]
			&& is_buffer_queued(pcam, img_mode)) {
				idx = pcam->mctl_node.dev_inst_map[img_mode]
							->my_index;
				pcam_inst = pcam->mctl_node.dev_inst[idx];
				D("%s Found instance %p in mctl node device\n",
				  __func__, pcam_inst);
			} else if (pcam->dev_inst_map[img_mode]) {
				idx = pcam->dev_inst_map[img_mode]->my_index;
				pcam_inst = pcam->dev_inst[idx];
				D("%s Found instance %p in video device\n",
				__func__, pcam_inst);
			}
		} else if (img_mode == MSM_V4L2_EXT_CAPTURE_MODE_V2X_LIVESHOT) {
				img_mode = MSM_V4L2_EXT_CAPTURE_MODE_MAIN;
			if (pcam->mctl_node.dev_inst_map[img_mode] &&
					is_buffer_queued(pcam, img_mode)) {
				idx = pcam->mctl_node.dev_inst_map[img_mode]
							->my_index;
				pcam_inst = pcam->mctl_node.dev_inst[idx];
				D("%s Found instance %p in mctl node device\n",
				  __func__, pcam_inst);
			} else if (pcam->dev_inst_map[img_mode]) {
				idx = pcam->dev_inst_map[img_mode]->my_index;
				pcam_inst = pcam->dev_inst[idx];
				D("%s Found instance %p in video device\n",
				__func__, pcam_inst);
			}
		} else {
			if (pcam->mctl_node.dev_inst_map[img_mode]) {
				idx = pcam->mctl_node.dev_inst_map[img_mode]
				->my_index;
				pcam_inst = pcam->mctl_node.dev_inst[idx];
				D("%s Found instance %p in mctl node device\n",
				__func__, pcam_inst);
			} else if (pcam->dev_inst_map[img_mode]) {
				idx = pcam->dev_inst_map[img_mode]->my_index;
				pcam_inst = pcam->dev_inst[idx];
				D("%s Found instance %p in video device\n",
					__func__, pcam_inst);
			}
		}
	return pcam_inst;
}

struct msm_cam_v4l2_dev_inst *msm_mctl_get_pcam_inst(
				struct msm_cam_media_controller *pmctl,
				struct msm_cam_buf_handle *buf_handle)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst = NULL;
	struct msm_cam_v4l2_device *pcam = pmctl->pcam_ptr;
	int idx;

	/* Get the pcam instance on based on the following rules:
	 * If the lookup type is
	 * - By instance handle:
	 *    Either mctl_pp inst idx or video inst idx should be set.
	 *    Try to get the MCTL_PP inst idx first, if its not set,
	 *    fall back to video inst idx. Once we get the inst idx,
	 *    get the pcam_inst from the corresponding dev_inst[] map.
	 *    If neither are set, its a serious error, trigger a BUG_ON.
	 * - By image mode:(Legacy usecase)
	 *    If vfe is in configured in snapshot mode, first check if
	 *    mctl pp node has a instance created for this image mode
	 *    and if there is a buffer queued for that instance.
	 *    If so, return that instance, otherwise get the pcam instance
	 *    for this image_mode from the video instance.
	 *    If the vfe is configured in any other mode, then first check
	 *    if mctl pp node has a instance created for this image mode,
	 *    otherwise get the pcam instance for this image mode from the
	 *    video instance.
	 */
	if (buf_handle->buf_lookup_type == BUF_LOOKUP_BY_INST_HANDLE) {
		idx = GET_MCTLPP_INST_IDX(buf_handle->inst_handle);
		if (idx > MSM_DEV_INST_MAX) {
			idx = GET_VIDEO_INST_IDX(buf_handle->inst_handle);
			BUG_ON(idx > MSM_DEV_INST_MAX);
			pcam_inst = pcam->dev_inst[idx];
		} else {
			pcam_inst = pcam->mctl_node.dev_inst[idx];
		}
	} else if ((buf_handle->buf_lookup_type == BUF_LOOKUP_BY_IMG_MODE)
		&& (buf_handle->image_mode >= 0 &&
		buf_handle->image_mode < MSM_V4L2_EXT_CAPTURE_MODE_MAX)) {
		pcam_inst = msm_mctl_get_inst_by_img_mode(pmctl,
				buf_handle->image_mode);
	} else {
		pr_err("%s Invalid buffer lookup type %d", __func__,
			buf_handle->buf_lookup_type);
	}
	return pcam_inst;
}

int msm_mctl_reserve_free_buf(
	struct msm_cam_media_controller *pmctl,
	struct msm_cam_v4l2_dev_inst *pref_pcam_inst,
	struct msm_cam_buf_handle *buf_handle,
	struct msm_free_buf *free_buf)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst = pref_pcam_inst;
	unsigned long flags = 0;
	struct videobuf2_contig_pmem *mem;
	struct msm_frame_buffer *buf = NULL;
	int rc = -EINVAL, i;
	uint32_t buf_idx, plane_offset = 0;

	if (!free_buf || !pmctl || !buf_handle) {
		pr_err("%s: Invalid argument passed\n", __func__);
		return rc;
	}
	memset(free_buf, 0, sizeof(struct msm_free_buf));

	/* If the caller wants to reserve a buffer from a particular
	 * camera instance, he would send the preferred camera instance.
	 * If the preferred camera instance is NULL, get the
	 * camera instance using the image mode passed */
	if (!pcam_inst)
		pcam_inst = msm_mctl_get_pcam_inst(pmctl, buf_handle);

	if (!pcam_inst || !pcam_inst->streamon) {
		pr_err("%s: stream is turned off\n", __func__);
		return rc;
	}
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	if (pcam_inst->free_vq.next == NULL) {
		pr_err("%s Inst %p Free queue head is null",
			__func__, pcam_inst);
		spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
		return rc;
	}
	list_for_each_entry(buf, &pcam_inst->free_vq, list) {
		if (buf == NULL) {
			pr_err("%s Inst %p Invalid buffer ptr",
				__func__, pcam_inst);
			spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
			return rc;
		}
		if (buf->state != MSM_BUFFER_STATE_QUEUED)
			continue;

		buf_idx = buf->vidbuf.v4l2_buf.index;
		if (pcam_inst->vid_fmt.type ==
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			free_buf->num_planes =
				pcam_inst->plane_info.num_planes;
			for (i = 0; i < free_buf->num_planes; i++) {
				mem = vb2_plane_cookie(&buf->vidbuf, i);
				if (mem == NULL) {
					pr_err("%s Inst %p %d invalid cookie",
						__func__, pcam_inst, buf_idx);
					spin_unlock_irqrestore(
						&pcam_inst->vq_irqlock, flags);
					return rc;
				}
				if (mem->buffer_type ==
						VIDEOBUF2_MULTIPLE_PLANES)
					plane_offset =
					mem->offset.data_offset;
				else
					plane_offset =
					mem->offset.sp_off.cbcr_off;

				D("%s: data off %d plane off %d",
					__func__,
					pcam_inst->buf_offset[buf_idx][i].
					data_offset, plane_offset);
				free_buf->ch_paddr[i] =	(uint32_t)
				videobuf2_to_pmem_contig(&buf->vidbuf, i) +
				pcam_inst->buf_offset[buf_idx][i].data_offset +
				plane_offset;

			}
		} else {
			mem = vb2_plane_cookie(&buf->vidbuf, 0);
			if (mem == NULL) {
				pr_err("%s Inst %p %d invalid cookie",
					__func__, pcam_inst, buf_idx);
				spin_unlock_irqrestore(
					&pcam_inst->vq_irqlock, flags);
				return rc;
			}
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
				struct msm_free_buf *free_buf)
{
	unsigned long flags = 0;
	struct msm_frame_buffer *buf = NULL;
	uint32_t buf_phyaddr = 0;
	int rc = -EINVAL;

	if (!pcam_inst || !free_buf) {
		pr_err("%s Invalid argument, buffer will not be returned\n",
			__func__);
		return rc;
	}

	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	list_for_each_entry(buf, &pcam_inst->free_vq, list) {
		buf_phyaddr =
			(uint32_t) videobuf2_to_pmem_contig(&buf->vidbuf, 0);
		if (free_buf->ch_paddr[0] == buf_phyaddr) {
			D("%s Return buffer %d and mark it as QUEUED\n",
				__func__, buf->vidbuf.v4l2_buf.index);
			buf->state = MSM_BUFFER_STATE_QUEUED;
			rc = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);

	if (rc)
		pr_err("%s Cannot find buffer %x", __func__,
			free_buf->ch_paddr[0]);

	return rc;
}

int msm_mctl_buf_done_pp(struct msm_cam_media_controller *pmctl,
	struct msm_cam_buf_handle *buf_handle,
	struct msm_free_buf *frame,
	struct msm_cam_return_frame_info *ret_frame)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst = NULL;
	int rc = 0, idx;
	struct msm_cam_timestamp cam_ts;

	if (!pmctl || !buf_handle || !ret_frame) {
		pr_err("%s Invalid argument ", __func__);
		return -EINVAL;
	}

	if (buf_handle->buf_lookup_type == BUF_LOOKUP_BY_INST_HANDLE) {
		idx = GET_MCTLPP_INST_IDX(buf_handle->inst_handle);
		if (idx > MSM_DEV_INST_MAX) {
			idx = GET_VIDEO_INST_IDX(buf_handle->inst_handle);
			BUG_ON(idx > MSM_DEV_INST_MAX);
			pcam_inst = pmctl->pcam_ptr->dev_inst[idx];
		} else {
			pcam_inst = pmctl->pcam_ptr->mctl_node.dev_inst[idx];
		}
	} else if (buf_handle->buf_lookup_type == BUF_LOOKUP_BY_IMG_MODE) {
		idx = msm_mctl_img_mode_to_inst_index(pmctl,
			buf_handle->image_mode, ret_frame->node_type);
		if (idx < 0) {
			pr_err("%s Invalid instance, buffer not released\n",
				__func__);
			return idx;
		}
		if (ret_frame->node_type)
			pcam_inst = pmctl->pcam_ptr->mctl_node.dev_inst[idx];
		else
			pcam_inst = pmctl->pcam_ptr->dev_inst[idx];
	}
	if (!pcam_inst) {
		pr_err("%s Invalid instance, cannot send buf to user",
			__func__);
		return -EINVAL;
	}

	D("%s:inst=0x%p, paddr=0x%x, dirty=%d",
		__func__, pcam_inst, frame->ch_paddr[0], ret_frame->dirty);
	cam_ts.present = 1;
	cam_ts.timestamp = ret_frame->timestamp;
	cam_ts.frame_id   = ret_frame->frame_id;
	if (ret_frame->dirty)
		/* the frame is dirty, not going to disptach to app */
		rc = msm_mctl_release_free_buf(pmctl, pcam_inst, frame);
	else
		rc = msm_mctl_buf_done_proc(pmctl, pcam_inst, frame,
			NULL, &cam_ts);
	return rc;
}

int msm_mctl_buf_return_buf(struct msm_cam_media_controller *pmctl,
			int image_mode, struct msm_frame_buffer *rbuf)
{
	int idx = 0;
	struct msm_frame_buffer *buf = NULL;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct msm_cam_v4l2_device *pcam = pmctl->pcam_ptr;
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

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
/* Unmap using ION APIs */
static void __msm_mctl_unmap_user_frame(struct msm_cam_meta_frame *meta_frame,
	struct ion_client *client, int domain_num)
{
	int i = 0;
	for (i = 0; i < meta_frame->frame.num_planes; i++) {
		D("%s Plane %d handle %p", __func__, i,
			meta_frame->map[i].handle);
		ion_unmap_iommu(client, meta_frame->map[i].handle,
					domain_num, 0);
		ion_free(client, meta_frame->map[i].handle);
	}
}

/* Map using ION APIs */
static int __msm_mctl_map_user_frame(struct msm_cam_meta_frame *meta_frame,
	struct ion_client *client, int domain_num)
{
	unsigned long paddr = 0;
	unsigned long len = 0;
	int i = 0, j = 0;

	for (i = 0; i < meta_frame->frame.num_planes; i++) {
		meta_frame->map[i].handle = ion_import_dma_buf(client,
			meta_frame->frame.mp[i].fd);
		if (IS_ERR_OR_NULL(meta_frame->map[i].handle)) {
			pr_err("%s: ion_import failed for plane = %d fd = %d",
				__func__, i, meta_frame->frame.mp[i].fd);
			/* Roll back previous plane mappings, if any */
			for (j = i-1; j >= 0; j--) {
				ion_unmap_iommu(client,
					meta_frame->map[j].handle,
					domain_num, 0);
				ion_free(client, meta_frame->map[j].handle);
			}
			return -EACCES;
		}
		D("%s Mapping fd %d plane %d handle %p", __func__,
			meta_frame->frame.mp[i].fd, i,
			meta_frame->map[i].handle);
		if (ion_map_iommu(client, meta_frame->map[i].handle,
				domain_num, 0, SZ_4K,
				0, &paddr, &len, 0, 0) < 0) {
			pr_err("%s: cannot map address plane %d", __func__, i);
			ion_free(client, meta_frame->map[i].handle);
			/* Roll back previous plane mappings, if any */
			for (j = i-1; j >= 0; j--) {
				if (meta_frame->map[j].handle) {
					ion_unmap_iommu(client,
						meta_frame->map[j].handle,
						domain_num, 0);
					ion_free(client,
						meta_frame->map[j].handle);
				}
			}
			return -EFAULT;
		}

		/* Validate the offsets with the mapped length. */
		if ((meta_frame->frame.mp[i].addr_offset > len) ||
			(meta_frame->frame.mp[i].data_offset +
			meta_frame->frame.mp[i].length > len)) {
			pr_err("%s: Invalid offsets A %d D %d L %d len %ld",
				__func__, meta_frame->frame.mp[i].addr_offset,
				meta_frame->frame.mp[i].data_offset,
				meta_frame->frame.mp[i].length, len);
			/* Roll back previous plane mappings, if any */
			for (j = i; j >= 0; j--) {
				if (meta_frame->map[j].handle) {
					ion_unmap_iommu(client,
						meta_frame->map[j].handle,
						domain_num, 0);
					ion_free(client,
						meta_frame->map[j].handle);
				}
			}
			return -EINVAL;
		}
		meta_frame->map[i].data_offset =
			meta_frame->frame.mp[i].data_offset;
		/* Add the addr_offset to the paddr here itself. The addr_offset
		 * will be non-zero only if the user has allocated a buffer with
		 * a single fd, but logically partitioned it into
		 * multiple planes or buffers.*/
		paddr += meta_frame->frame.mp[i].addr_offset;
		meta_frame->map[i].paddr = paddr;
		meta_frame->map[i].len = len;
		D("%s Plane %d fd %d handle %p paddr %x", __func__,
			i, meta_frame->frame.mp[i].fd,
			meta_frame->map[i].handle,
			(uint32_t)meta_frame->map[i].paddr);
	}
	D("%s Frame mapped successfully ", __func__);
	return 0;
}
#endif

int msm_mctl_map_user_frame(struct msm_cam_meta_frame *meta_frame,
	struct ion_client *client, int domain_num)
{

	if ((NULL == meta_frame) || (NULL == client)) {
		pr_err("%s Invalid input ", __func__);
		return -EINVAL;
	}

	memset(&meta_frame->map[0], 0,
		sizeof(struct msm_cam_buf_map_info) * VIDEO_MAX_PLANES);

	return __msm_mctl_map_user_frame(meta_frame, client, domain_num);
}

int msm_mctl_unmap_user_frame(struct msm_cam_meta_frame *meta_frame,
	struct ion_client *client, int domain_num)
{
	if ((NULL == meta_frame) || (NULL == client)) {
		pr_err("%s Invalid input ", __func__);
		return -EINVAL;
	}
	__msm_mctl_unmap_user_frame(meta_frame, client, domain_num);
	return 0;
}
