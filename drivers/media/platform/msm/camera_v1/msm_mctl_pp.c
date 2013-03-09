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
 *
 */

#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <linux/videodev2.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>



#include "msm.h"
#include "msm_vpe.h"

#ifdef CONFIG_MSM_CAMERA_DEBUG
#define D(fmt, args...) pr_debug("msm_mctl: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

static int msm_mctl_pp_buf_divert(
			struct msm_cam_media_controller *pmctl,
			struct msm_cam_v4l2_dev_inst *pcam_inst,
			struct msm_cam_evt_divert_frame *div)
{
	struct v4l2_event v4l2_evt;
	struct msm_isp_event_ctrl *isp_event;
	isp_event = kzalloc(sizeof(struct msm_isp_event_ctrl),
						GFP_ATOMIC);
	if (!isp_event) {
		pr_err("%s Insufficient memory. return", __func__);
		return -ENOMEM;
	}
	D("%s: msm_cam_evt_divert_frame=%d",
		__func__, sizeof(struct msm_cam_evt_divert_frame));
	memset(&v4l2_evt, 0, sizeof(v4l2_evt));
	v4l2_evt.id = 0;
	v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			MSM_CAM_RESP_DIV_FRAME_EVT_MSG;
	*((uint32_t *)v4l2_evt.u.data) = (uint32_t)isp_event;
	/* Copy the divert frame struct into event ctrl struct. */
	isp_event->isp_data.div_frame = *div;

	D("%s inst=%p, img_mode=%d, frame_id=%d\n", __func__,
		pcam_inst, pcam_inst->image_mode, div->frame.frame_id);
	v4l2_event_queue(
		pmctl->config_device->config_stat_event_queue.pvdev,
		&v4l2_evt);
	return 0;
}

int msm_mctl_check_pp(struct msm_cam_media_controller *p_mctl,
	int image_mode, int *pp_divert_type, int *pp_type)
{
	int rc = 0;
	unsigned long flags;
	uint32_t pp_key = 0;

	*pp_type = 0;
	*pp_divert_type = 0;
	spin_lock_irqsave(&p_mctl->pp_info.lock, flags);
	switch (image_mode) {
	case MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW:
		pp_key = PP_PREV;
		if (p_mctl->pp_info.pp_key & pp_key)
			*pp_divert_type = OUTPUT_TYPE_P;
		if (p_mctl->pp_info.pp_ctrl.pp_msg_type & OUTPUT_TYPE_P)
			*pp_type = OUTPUT_TYPE_P;
		break;
	case MSM_V4L2_EXT_CAPTURE_MODE_MAIN:
		pp_key = PP_SNAP;
		if (p_mctl->pp_info.pp_key & pp_key)
			*pp_divert_type = OUTPUT_TYPE_S;
		if (p_mctl->pp_info.pp_ctrl.pp_msg_type & OUTPUT_TYPE_S)
			*pp_type = OUTPUT_TYPE_P;
		break;
	case MSM_V4L2_EXT_CAPTURE_MODE_VIDEO:
		if (p_mctl->pp_info.pp_ctrl.pp_msg_type == OUTPUT_TYPE_V)
			*pp_type = OUTPUT_TYPE_V;
		break;
	case MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL:
		pp_key = PP_THUMB;
		if (p_mctl->pp_info.pp_key & pp_key)
			*pp_divert_type = OUTPUT_TYPE_T;
		if (p_mctl->pp_info.pp_ctrl.pp_msg_type == OUTPUT_TYPE_T)
			*pp_type = OUTPUT_TYPE_T;
		break;
	case MSM_V4L2_EXT_CAPTURE_MODE_RDI:
		if (p_mctl->pp_info.pp_ctrl.pp_msg_type & OUTPUT_TYPE_R)
			*pp_type = OUTPUT_TYPE_R;
		break;
	default:
		break;
	}
	if (p_mctl->vfe_output_mode != VFE_OUTPUTS_MAIN_AND_THUMB &&
		p_mctl->vfe_output_mode != VFE_OUTPUTS_THUMB_AND_MAIN) {
		if (p_mctl->pp_info.div_frame[image_mode].ch_paddr[0])
			*pp_divert_type = 0;
	}
	spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);
	D("%s: pp_type=%d, pp_divert_type = %d",
	__func__, *pp_type, *pp_divert_type);
	return rc;
}

static int is_buf_in_queue(struct msm_cam_v4l2_device *pcam,
	struct msm_free_buf *fbuf, int image_mode)
{
	struct msm_frame_buffer *buf = NULL, *tmp;
	struct msm_cam_v4l2_dev_inst *pcam_inst = NULL;
	unsigned long flags = 0;
	struct videobuf2_contig_pmem *mem;
	uint32_t buf_idx, offset = 0;
	uint32_t buf_phyaddr = 0;
	int idx;
	idx = pcam->mctl_node.dev_inst_map[image_mode]->my_index;
	pcam_inst = pcam->mctl_node.dev_inst[idx];
	spin_lock_irqsave(&pcam_inst->vq_irqlock, flags);
	list_for_each_entry_safe(buf, tmp,
	&pcam_inst->free_vq, list) {
		buf_idx = buf->vidbuf.v4l2_buf.index;
		mem = vb2_plane_cookie(&buf->vidbuf, 0);
		if (mem == NULL) {
			pr_err("%s Inst %p Buffer %d invalid plane cookie",
				__func__, pcam_inst, buf_idx);
			spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
			return 0;
		}
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
			spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
			return 1;
		}
	}
	spin_unlock_irqrestore(&pcam_inst->vq_irqlock, flags);
	return 0;
}

static struct msm_cam_v4l2_dev_inst *msm_mctl_get_pcam_inst_for_divert(
	struct msm_cam_media_controller *pmctl,
	struct msm_cam_buf_handle *buf_handle,
	struct msm_free_buf *fbuf, int *node_type)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst = NULL;
	struct msm_cam_v4l2_device *pcam = pmctl->pcam_ptr;
	int idx;
	uint32_t img_mode;

	if (buf_handle->buf_lookup_type == BUF_LOOKUP_BY_INST_HANDLE) {
		idx = GET_MCTLPP_INST_IDX(buf_handle->inst_handle);
		if (idx > MSM_DEV_INST_MAX) {
			idx = GET_VIDEO_INST_IDX(buf_handle->inst_handle);
			BUG_ON(idx > MSM_DEV_INST_MAX);
			pcam_inst = pcam->dev_inst[idx];
			*node_type = VIDEO_NODE;
		} else {
			pcam_inst = pcam->mctl_node.dev_inst[idx];
			*node_type = MCTL_NODE;
		}
	} else if (buf_handle->buf_lookup_type == BUF_LOOKUP_BY_IMG_MODE) {
		img_mode = buf_handle->image_mode;
		if (img_mode >= 0 && img_mode < MSM_V4L2_EXT_CAPTURE_MODE_MAX) {
			/* Valid image mode. Search the mctl node first.
			 * If mctl node doesnt have the instance, then
			 * search in the user's video node */
			if (pcam->mctl_node.dev_inst_map[img_mode]
				&& is_buf_in_queue(pcam, fbuf, img_mode)) {
				idx = pcam->mctl_node.
					dev_inst_map[img_mode]->my_index;
				pcam_inst = pcam->mctl_node.dev_inst[idx];
				*node_type = MCTL_NODE;
				D("%s Found instance %p in mctl node device\n",
					__func__, pcam_inst);
			} else if (pcam->dev_inst_map[img_mode]) {
				idx = pcam->dev_inst_map[img_mode]->my_index;
				pcam_inst = pcam->dev_inst[idx];
				*node_type = VIDEO_NODE;
				D("%s Found instance %p in video device",
					__func__, pcam_inst);
			} else {
				pr_err("%s Cannot find instance for %d.\n",
					__func__, img_mode);
			}
		} else {
			pr_err("%s Invalid image mode %d. Return NULL\n",
				__func__, buf_handle->image_mode);
		}
	} else {
		pr_err("%s Invalid buffer lookup type ", __func__);
	}
	return pcam_inst;
}

int msm_mctl_do_pp_divert(
	struct msm_cam_media_controller *p_mctl,
	struct msm_cam_buf_handle *buf_handle,
	struct msm_free_buf *fbuf,
	uint32_t frame_id, int pp_type)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	int rc = 0, i, buf_idx, node;
	int del_buf = 0; /* delete from free queue */
	struct msm_cam_evt_divert_frame div;
	struct msm_frame_buffer *vb = NULL;
	struct videobuf2_contig_pmem *mem;
	uint32_t image_mode;

	if (buf_handle->buf_lookup_type == BUF_LOOKUP_BY_IMG_MODE) {
		image_mode = buf_handle->image_mode;
		div.frame.inst_handle = 0;
	} else if (buf_handle->buf_lookup_type == BUF_LOOKUP_BY_INST_HANDLE) {
		image_mode = GET_IMG_MODE(buf_handle->inst_handle);
		div.frame.inst_handle = buf_handle->inst_handle;
	} else {
		pr_err("%s Invalid buffer lookup type %d ", __func__,
			buf_handle->buf_lookup_type);
		return -EINVAL;
	}

	pcam_inst = msm_mctl_get_pcam_inst_for_divert(p_mctl,
			buf_handle, fbuf, &node);
	if (!pcam_inst) {
		pr_err("%s Invalid instance. Cannot divert frame.\n",
			__func__);
		return -EINVAL;
	}
	vb = msm_mctl_buf_find(p_mctl, pcam_inst, del_buf, fbuf);
	if (!vb)
		return -EINVAL;

	vb->vidbuf.v4l2_buf.sequence = frame_id;
	buf_idx = vb->vidbuf.v4l2_buf.index;
	D("%s Diverting frame %d %x Image mode %d\n", __func__, buf_idx,
		(uint32_t)vb, pcam_inst->image_mode);
	div.image_mode = pcam_inst->image_mode;
	div.op_mode    = pcam_inst->pcam->op_mode;
	div.inst_idx   = pcam_inst->my_index;
	div.node_idx   = pcam_inst->pcam->vnode_id;
	p_mctl->pp_info.cur_frame_id[pcam_inst->image_mode] = frame_id;
	div.frame.frame_id =
		p_mctl->pp_info.cur_frame_id[pcam_inst->image_mode];
	div.frame.buf_idx  = buf_idx;
	div.frame.handle = (uint32_t)vb;
	msm_mctl_gettimeofday(&div.frame.timestamp);
	vb->vidbuf.v4l2_buf.timestamp = div.frame.timestamp;
	div.do_pp = pp_type;
	D("%s Diverting frame %x id %d to userspace ", __func__,
		(int)div.frame.handle, div.frame.frame_id);
	/* Get the cookie for 1st plane and store the path.
	 * Also use this to check the number of planes in
	 * this buffer.*/
	mem = vb2_plane_cookie(&vb->vidbuf, 0);
	if (mem == NULL) {
		pr_err("%s Inst %p Buffer %d, invalid plane cookie ", __func__,
			pcam_inst, buf_idx);
		return -EINVAL;
	}
	div.frame.path = mem->path;
	div.frame.node_type = node;
	if (mem->buffer_type == VIDEOBUF2_SINGLE_PLANE) {
		/* This buffer contains only 1 plane. Use the
		 * single planar structure to store the info.*/
		div.frame.num_planes	= 1;
		div.frame.sp.phy_addr	=
			videobuf2_to_pmem_contig(&vb->vidbuf, 0);
		div.frame.sp.addr_offset = mem->addr_offset;
		div.frame.sp.y_off      = 0;
		div.frame.sp.cbcr_off   = mem->offset.sp_off.cbcr_off;
		div.frame.sp.fd         = (int)mem->vaddr;
		div.frame.sp.length     = mem->size;
		if (!pp_type)
			p_mctl->pp_info.div_frame[pcam_inst->image_mode].
			ch_paddr[0] = div.frame.sp.phy_addr;
	} else {
		/* This buffer contains multiple planes. Use the mutliplanar
		 * structure to store the info. */
		div.frame.num_planes	= pcam_inst->plane_info.num_planes;
		/* Now traverse through all the planes of the buffer to
		 * fill out the plane info. */
		for (i = 0; i < div.frame.num_planes; i++) {
			mem = vb2_plane_cookie(&vb->vidbuf, i);
			if (mem == NULL) {
				pr_err("%s Inst %p %d invalid plane cookie ",
					__func__, pcam_inst, buf_idx);
				return -EINVAL;
			}
			div.frame.mp[i].phy_addr =
				videobuf2_to_pmem_contig(&vb->vidbuf, i);
			if (!pcam_inst->buf_offset)
				div.frame.mp[i].data_offset = 0;
			else
				div.frame.mp[i].data_offset =
				pcam_inst->buf_offset[buf_idx][i].data_offset;
			div.frame.mp[i].addr_offset =
				mem->addr_offset;
			div.frame.mp[i].fd = (int)mem->vaddr;
			div.frame.mp[i].length = mem->size;
		}
		if (!pp_type)
			p_mctl->pp_info.div_frame[pcam_inst->image_mode].
			ch_paddr[0] = div.frame.mp[0].phy_addr +
					div.frame.mp[0].data_offset;
	}
	rc = msm_mctl_pp_buf_divert(p_mctl, pcam_inst, &div);
	return rc;
}

static int msm_mctl_pp_get_phy_addr(
	struct msm_cam_v4l2_dev_inst *pcam_inst,
	uint32_t frame_handle,
	struct msm_pp_frame *pp_frame)
{
	struct msm_frame_buffer *vb = NULL;
	struct videobuf2_contig_pmem *mem;
	int i, buf_idx = 0;

	vb = (struct msm_frame_buffer *)frame_handle;
	buf_idx = vb->vidbuf.v4l2_buf.index;
	memset(pp_frame, 0, sizeof(struct msm_pp_frame));
	pp_frame->handle = (uint32_t)vb;
	pp_frame->frame_id = vb->vidbuf.v4l2_buf.sequence;
	pp_frame->timestamp = vb->vidbuf.v4l2_buf.timestamp;
	pp_frame->buf_idx = buf_idx;
	pp_frame->inst_handle = pcam_inst->inst_handle;
	/* Get the cookie for 1st plane and store the path.
	 * Also use this to check the number of planes in
	 * this buffer.*/
	mem = vb2_plane_cookie(&vb->vidbuf, 0);
	if (mem == NULL) {
		pr_err("%s Inst %p Buffer %d, invalid plane cookie ", __func__,
			pcam_inst, buf_idx);
		return -EINVAL;
	}
	pp_frame->image_type = (unsigned short)mem->path;
	if (mem->buffer_type == VIDEOBUF2_SINGLE_PLANE) {
		pp_frame->num_planes = 1;
		pp_frame->sp.addr_offset = mem->addr_offset;
		pp_frame->sp.phy_addr =
			videobuf2_to_pmem_contig(&vb->vidbuf, 0);
		pp_frame->sp.y_off = 0;
		pp_frame->sp.cbcr_off = mem->offset.sp_off.cbcr_off;
		pp_frame->sp.length = mem->size;
		pp_frame->sp.fd = (int)mem->vaddr;
	} else {
		pp_frame->num_planes = pcam_inst->plane_info.num_planes;
		for (i = 0; i < pp_frame->num_planes; i++) {
			mem = vb2_plane_cookie(&vb->vidbuf, i);
			pp_frame->mp[i].addr_offset = mem->addr_offset;
			pp_frame->mp[i].phy_addr =
				videobuf2_to_pmem_contig(&vb->vidbuf, i);
			pp_frame->mp[i].data_offset =
			pcam_inst->buf_offset[buf_idx][i].data_offset;
			pp_frame->mp[i].fd = (int)mem->vaddr;
			pp_frame->mp[i].length = mem->size;
			D("%s frame id %d buffer %d plane %d phy addr 0x%x"
				" fd %d length %d\n", __func__,
				pp_frame->frame_id, buf_idx, i,
				(uint32_t)pp_frame->mp[i].phy_addr,
				pp_frame->mp[i].fd, pp_frame->mp[i].length);
		}
	}
	return 0;
}

static int msm_mctl_pp_path_to_img_mode(int path)
{
	switch (path) {
	case OUTPUT_TYPE_P:
		return MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW;
	case OUTPUT_TYPE_V:
		return MSM_V4L2_EXT_CAPTURE_MODE_VIDEO;
	case OUTPUT_TYPE_S:
		return MSM_V4L2_EXT_CAPTURE_MODE_MAIN;
	case OUTPUT_TYPE_T:
		return MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL;
	case OUTPUT_TYPE_SAEC:
		return MSM_V4L2_EXT_CAPTURE_MODE_AEC;
	case OUTPUT_TYPE_SAWB:
		return MSM_V4L2_EXT_CAPTURE_MODE_AWB;
	case OUTPUT_TYPE_SAFC:
		return MSM_V4L2_EXT_CAPTURE_MODE_AF;
	case OUTPUT_TYPE_IHST:
		return MSM_V4L2_EXT_CAPTURE_MODE_IHIST;
	case OUTPUT_TYPE_CSTA:
		return MSM_V4L2_EXT_CAPTURE_MODE_CSTA;
	default:
		return -EINVAL;
	}
}

int msm_mctl_pp_proc_cmd(struct msm_cam_media_controller *p_mctl,
			struct msm_mctl_pp_cmd *pp_cmd)
{
	int rc = 0;
	unsigned long flags;

	switch (pp_cmd->id) {
	case MCTL_CMD_DIVERT_FRAME_PP_PATH: {
		struct msm_mctl_pp_divert_pp divert_pp;
		if (copy_from_user(&divert_pp, pp_cmd->value,
				sizeof(divert_pp))) {
			ERR_COPY_FROM_USER();
			return -EFAULT;
		}
		D("%s: Divert Image mode =%d Enable %d",
			__func__, divert_pp.path, divert_pp.enable);
		spin_lock_irqsave(&p_mctl->pp_info.lock, flags);
		if (divert_pp.enable)
			p_mctl->pp_info.pp_ctrl.pp_msg_type |= divert_pp.path;
		else
			p_mctl->pp_info.pp_ctrl.pp_msg_type &= ~divert_pp.path;
		spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);
		D("%s: pp path = 0x%x", __func__,
			p_mctl->pp_info.pp_ctrl.pp_msg_type);
		break;
	}
	default:
		rc = -EPERM;
	break;
	}
	return rc;
}


int msm_mctl_pp_ioctl(struct msm_cam_media_controller *p_mctl,
			unsigned int cmd, unsigned long arg)
{
	int rc = -EINVAL;
	struct msm_mctl_post_proc_cmd pp_cmd;
	void __user *argp = (void __user *)arg;

	if (copy_from_user(&pp_cmd, argp, sizeof(pp_cmd)))
		return -EFAULT;

	switch (pp_cmd.type) {
	case MSM_PP_CMD_TYPE_MCTL:
		rc = msm_mctl_pp_proc_cmd(p_mctl, &pp_cmd.cmd);
		break;
	default:
		rc = -EPERM;
		break;
	}
	if (!rc) {
		/* deep copy back the return value */
		if (copy_to_user((void *)arg,
			&pp_cmd,
			sizeof(struct msm_mctl_post_proc_cmd))) {
			ERR_COPY_TO_USER();
			rc = -EFAULT;
		}
	}
	return rc;
}

int msm_mctl_pp_reserve_free_frame(
	struct msm_cam_media_controller *p_mctl,
	void __user *arg)
{
	struct msm_cam_evt_divert_frame div_frame;
	int image_mode, rc = 0;
	struct msm_free_buf free_buf;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct msm_cam_buf_handle buf_handle;

	memset(&free_buf, 0, sizeof(struct msm_free_buf));
	if (copy_from_user(&div_frame, arg,
		sizeof(struct msm_cam_evt_divert_frame))) {
		ERR_COPY_FROM_USER();
		return -EFAULT;
	}

	image_mode = div_frame.image_mode;
	if (image_mode <= 0) {
		pr_err("%s Invalid image mode %d", __func__, image_mode);
		return -EINVAL;
	}
	/* Always reserve the buffer from user's video node */
	pcam_inst = p_mctl->pcam_ptr->dev_inst_map[image_mode];
	if (!pcam_inst) {
		pr_err("%s Instance already closed ", __func__);
		return -EINVAL;
	}
	D("%s Reserving free frame using %p inst handle %x ", __func__,
		pcam_inst, div_frame.frame.inst_handle);
	if (div_frame.frame.inst_handle) {
		buf_handle.buf_lookup_type = BUF_LOOKUP_BY_INST_HANDLE;
		buf_handle.inst_handle = div_frame.frame.inst_handle;
	} else {
		buf_handle.buf_lookup_type = BUF_LOOKUP_BY_IMG_MODE;
		buf_handle.image_mode = image_mode;
	}
	rc = msm_mctl_reserve_free_buf(p_mctl, pcam_inst,
					&buf_handle, &free_buf);
	if (rc == 0) {
		msm_mctl_pp_get_phy_addr(pcam_inst,
			free_buf.vb, &div_frame.frame);
		if (copy_to_user((void *)arg, &div_frame, sizeof(div_frame))) {
			ERR_COPY_TO_USER();
			rc = -EFAULT;
		}
	}
	D("%s: Got buffer %d from Inst %p rc = %d, phy = 0x%x",
		__func__, div_frame.frame.buf_idx,
		pcam_inst, rc, free_buf.ch_paddr[0]);
	return rc;
}

int msm_mctl_pp_release_free_frame(
	struct msm_cam_media_controller *p_mctl,
	void __user *arg)
{
	struct msm_cam_evt_divert_frame div_frame;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct msm_pp_frame *frame;
	int image_mode, rc = 0;
	struct msm_free_buf free_buf;
	struct msm_cam_buf_handle buf_handle;

	if (copy_from_user(&div_frame, arg,
		sizeof(struct msm_cam_evt_divert_frame))) {
		ERR_COPY_FROM_USER();
		return -EFAULT;
	}

	image_mode = div_frame.image_mode;
	if (image_mode < 0) {
		pr_err("%s Invalid image mode %d\n", __func__, image_mode);
		return -EINVAL;
	}
	frame = &div_frame.frame;
	if (frame->num_planes > 1)
		free_buf.ch_paddr[0] = frame->mp[0].phy_addr;
	else
		free_buf.ch_paddr[0] = frame->sp.phy_addr;

	if (div_frame.frame.inst_handle) {
		buf_handle.buf_lookup_type = BUF_LOOKUP_BY_INST_HANDLE;
		buf_handle.inst_handle = div_frame.frame.inst_handle;
	} else {
		buf_handle.buf_lookup_type = BUF_LOOKUP_BY_IMG_MODE;
		buf_handle.image_mode = image_mode;
	}
	pcam_inst = msm_mctl_get_pcam_inst(p_mctl, &buf_handle);
	if (!pcam_inst) {
		pr_err("%s Invalid instance. Cannot release frame.\n",
			__func__);
		return -EINVAL;
	}

	rc = msm_mctl_release_free_buf(p_mctl, pcam_inst, &free_buf);
	D("%s: release free buf, rc = %d, phy = 0x%x",
		__func__, rc, free_buf.ch_paddr[0]);

	return rc;
}

int msm_mctl_set_pp_key(struct msm_cam_media_controller *p_mctl,
				void __user *arg)
{
	int rc = 0;
	unsigned long flags;
	spin_lock_irqsave(&p_mctl->pp_info.lock, flags);
	if (copy_from_user(&p_mctl->pp_info.pp_key,
			arg, sizeof(p_mctl->pp_info.pp_key))) {
		ERR_COPY_FROM_USER();
		rc = -EFAULT;
	} else {
		D("%s: mctl=0x%p, pp_key_setting=0x%x",
			__func__, p_mctl, p_mctl->pp_info.pp_key);
	}
	spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);
	return rc;
}

int msm_mctl_pp_done(
	struct msm_cam_media_controller *p_mctl,
	void __user *arg)
{
	struct msm_pp_frame frame;
	int image_mode, rc = 0;
	int dirty = 0;
	struct msm_free_buf buf;
	unsigned long flags;
	struct msm_cam_buf_handle buf_handle;
	struct msm_cam_return_frame_info ret_frame;

	if (copy_from_user(&frame, arg, sizeof(frame))) {
		ERR_COPY_FROM_USER();
		return -EFAULT;
	}

	spin_lock_irqsave(&p_mctl->pp_info.lock, flags);
	if (frame.inst_handle) {
		buf_handle.buf_lookup_type = BUF_LOOKUP_BY_INST_HANDLE;
		buf_handle.inst_handle = frame.inst_handle;
		image_mode = GET_IMG_MODE(frame.inst_handle);
	} else {
		buf_handle.buf_lookup_type = BUF_LOOKUP_BY_IMG_MODE;
		buf_handle.image_mode =
			msm_mctl_pp_path_to_img_mode(frame.path);
		image_mode = buf_handle.image_mode;
	}
	if (image_mode < 0) {
		pr_err("%s Invalid image mode\n", __func__);
		return image_mode;
	}
	D("%s Returning frame %x id %d to kernel ", __func__,
		(int)frame.handle, frame.frame_id);
	if (p_mctl->pp_info.div_frame[image_mode].ch_paddr[0]) {
		memcpy(&buf,
			&p_mctl->pp_info.div_frame[image_mode],
			sizeof(buf));
		memset(&p_mctl->pp_info.div_frame[image_mode],
			0, sizeof(buf));
		if (p_mctl->pp_info.cur_frame_id[image_mode] !=
					frame.frame_id) {
			/* dirty frame. should not pass to app */
			dirty = 1;
		}
	} else {
		if (frame.num_planes > 1)
			buf.ch_paddr[0] = frame.mp[0].phy_addr +
						frame.mp[0].data_offset;
		else
			buf.ch_paddr[0] = frame.sp.phy_addr + frame.sp.y_off;
	}
	spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);

	ret_frame.dirty = dirty;
	ret_frame.node_type = 0;
	ret_frame.timestamp = frame.timestamp;
	ret_frame.frame_id   = frame.frame_id;
	D("%s frame_id: %d buffer idx %d\n", __func__,
		frame.frame_id, frame.buf_idx);
	rc = msm_mctl_buf_done_pp(p_mctl, &buf_handle, &buf, &ret_frame);
	return rc;
}

int msm_mctl_pp_divert_done(
	struct msm_cam_media_controller *p_mctl,
	void __user *arg)
{
	struct msm_pp_frame frame;
	int rc = 0;
	struct msm_free_buf buf;
	unsigned long flags;
	struct msm_cam_buf_handle buf_handle;
	struct msm_cam_return_frame_info ret_frame;

	D("%s enter\n", __func__);

	if (copy_from_user(&frame, arg, sizeof(frame))) {
		ERR_COPY_FROM_USER();
		return -EFAULT;
	}

	spin_lock_irqsave(&p_mctl->pp_info.lock, flags);
	if (frame.inst_handle) {
		buf_handle.buf_lookup_type = BUF_LOOKUP_BY_INST_HANDLE;
		buf_handle.inst_handle = frame.inst_handle;
	} else {
		buf_handle.buf_lookup_type = BUF_LOOKUP_BY_IMG_MODE;
		buf_handle.image_mode = frame.image_type;
	}

	if (frame.num_planes > 1)
		buf.ch_paddr[0] = frame.mp[0].phy_addr +
					frame.mp[0].data_offset;
	else
		buf.ch_paddr[0] = frame.sp.phy_addr + frame.sp.y_off;

	spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);

	ret_frame.dirty = 0;
	ret_frame.node_type = frame.node_type;
	ret_frame.timestamp = frame.timestamp;
	ret_frame.frame_id  = frame.frame_id;
	D("%s Frame done id: %d\n", __func__, frame.frame_id);
	rc = msm_mctl_buf_done_pp(p_mctl, &buf_handle, &buf, &ret_frame);
	return rc;
}

int msm_mctl_pp_mctl_divert_done(
	struct msm_cam_media_controller *p_mctl,
	void __user *arg)
{
	struct msm_cam_evt_divert_frame div_frame;
	struct msm_frame_buffer *buf;
	int image_mode, rc = 0;

	if (copy_from_user(&div_frame, arg,
			sizeof(struct msm_cam_evt_divert_frame))) {
		pr_err("%s copy from user failed ", __func__);
		return -EFAULT;
	}

	if (!div_frame.frame.handle) {
		pr_err("%s Invalid buffer handle ", __func__);
		return -EINVAL;
	}
	image_mode = div_frame.image_mode;
	buf = (struct msm_frame_buffer *)div_frame.frame.handle;
	D("%s Returning buffer %x Image mode %d ", __func__,
		(int)buf, image_mode);
	rc = msm_mctl_buf_return_buf(p_mctl, image_mode, buf);
	if (rc < 0)
		pr_err("%s Error returning mctl buffer ", __func__);

	return rc;
}
