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

#include <linux/android_pmem.h>

#include "msm.h"

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
		int image_mode, struct msm_free_buf *fbuf, int *node_type)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst = NULL;
	struct msm_cam_v4l2_device *pcam = pmctl->sync.pcam_sync;
	int idx;

	if (image_mode >= 0) {
		/* Valid image mode. Search the mctl node first.
		 * If mctl node doesnt have the instance, then
		 * search in the user's video node */
		if (pcam->mctl_node.dev_inst_map[image_mode]
		&& is_buf_in_queue(pcam, fbuf, image_mode)) {
			idx =
			pcam->mctl_node.dev_inst_map[image_mode]->my_index;
			pcam_inst = pcam->mctl_node.dev_inst[idx];
			*node_type = MCTL_NODE;
			D("%s Found instance %p in mctl node device\n",
				__func__, pcam_inst);
		} else if (pcam->dev_inst_map[image_mode]) {
			idx = pcam->dev_inst_map[image_mode]->my_index;
			pcam_inst = pcam->dev_inst[idx];
			*node_type = VIDEO_NODE;
			D("%s Found instance %p in video device",
				__func__, pcam_inst);
		} else
			pr_err("%s Invalid image mode %d. Return NULL\n",
				   __func__, image_mode);
	}
		return pcam_inst;
}

int msm_mctl_do_pp_divert(
	struct msm_cam_media_controller *p_mctl,
	int image_mode, struct msm_free_buf *fbuf,
	uint32_t frame_id, int pp_type)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	int rc = 0, i, buf_idx;
	int del_buf = 0; /* delete from free queue */
	struct msm_cam_evt_divert_frame div;
	struct msm_frame_buffer *vb = NULL;
	struct videobuf2_contig_pmem *mem;
	int node;

	pcam_inst = msm_mctl_get_pcam_inst_for_divert
		(p_mctl, image_mode, fbuf, &node);
	if (!pcam_inst) {
		pr_err("%s Invalid instance. Cannot divert frame.\n",
			__func__);
		return -EINVAL;
	}
	vb = msm_mctl_buf_find(p_mctl, pcam_inst,
		  del_buf, image_mode, fbuf);
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
	/* Get the cookie for 1st plane and store the path.
	 * Also use this to check the number of planes in
	 * this buffer.*/
	mem = vb2_plane_cookie(&vb->vidbuf, 0);
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

static int msm_mctl_pp_copy_timestamp_and_frame_id(
	uint32_t src_handle, uint32_t dest_handle)
{
	struct msm_frame_buffer *src_vb;
	struct msm_frame_buffer *dest_vb;

	src_vb = (struct msm_frame_buffer *)src_handle;
	dest_vb = (struct msm_frame_buffer *)dest_handle;
	dest_vb->vidbuf.v4l2_buf.timestamp =
		src_vb->vidbuf.v4l2_buf.timestamp;
	dest_vb->vidbuf.v4l2_buf.sequence =
		src_vb->vidbuf.v4l2_buf.sequence;
	D("%s: timestamp=%ld:%ld,frame_id=0x%x", __func__,
		dest_vb->vidbuf.v4l2_buf.timestamp.tv_sec,
		dest_vb->vidbuf.v4l2_buf.timestamp.tv_usec,
		dest_vb->vidbuf.v4l2_buf.sequence);
	return 0;
}

static int msm_mctl_pp_path_to_inst_index(struct msm_cam_v4l2_device *pcam,
					int out_type)
{
	int image_mode;
	switch (out_type) {
	case OUTPUT_TYPE_P:
		image_mode = MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW;
		break;
	case OUTPUT_TYPE_V:
		image_mode = MSM_V4L2_EXT_CAPTURE_MODE_VIDEO;
		break;
	case OUTPUT_TYPE_S:
		image_mode = MSM_V4L2_EXT_CAPTURE_MODE_MAIN;
		break;
	default:
		image_mode = -1;
		break;
	}
	if ((image_mode >= 0) && pcam->dev_inst_map[image_mode])
		return pcam->dev_inst_map[image_mode]->my_index;
	else
		return -EINVAL;
}

int msm_mctl_pp_proc_vpe_cmd(
	struct msm_cam_media_controller *p_mctl,
	struct msm_mctl_pp_cmd *pp_cmd)
{
	int rc = 0, idx;
	void __user *argp = (void __user *)pp_cmd->value;
	struct msm_cam_v4l2_dev_inst *pcam_inst;

	switch (pp_cmd->id) {
	case VPE_CMD_INIT:
	case VPE_CMD_DEINIT:
		rc = msm_isp_subdev_ioctl_vpe(
			p_mctl->isp_sdev->sd_vpe, pp_cmd, NULL);
		break;
	case VPE_CMD_DISABLE:
	case VPE_CMD_RESET:
		rc = msm_isp_subdev_ioctl_vpe(
			p_mctl->isp_sdev->sd_vpe, pp_cmd, NULL);
		break;
	case VPE_CMD_ENABLE: {
		struct msm_vpe_clock_rate clk_rate;
		if (sizeof(struct msm_vpe_clock_rate) !=
			pp_cmd->length) {
			pr_err("%s: vpe cmd size mismatch "
				"(id=%d, length = %d, expect size = %d",
				__func__, pp_cmd->id, pp_cmd->length,
				sizeof(struct msm_vpe_clock_rate));
				rc = -EINVAL;
				break;
		}
		if (copy_from_user(&clk_rate, pp_cmd->value,
			sizeof(struct msm_vpe_clock_rate))) {
			pr_err("%s:clk_rate copy failed", __func__);
			return -EFAULT;
		}
		pp_cmd->value = (void *)&clk_rate;
		rc = msm_isp_subdev_ioctl_vpe(
			p_mctl->isp_sdev->sd_vpe, pp_cmd, NULL);
		pp_cmd->value = argp;
		break;
	}
	case VPE_CMD_FLUSH: {
		struct msm_vpe_flush_frame_buffer flush_buf;
		if (sizeof(struct msm_vpe_flush_frame_buffer) !=
			pp_cmd->length) {
			D("%s: size mismatch(id=%d, len = %d, expected = %d",
				__func__, pp_cmd->id, pp_cmd->length,
				sizeof(struct msm_vpe_flush_frame_buffer));
				rc = -EINVAL;
				break;
		}
		if (copy_from_user(
			&flush_buf, pp_cmd->value, sizeof(flush_buf)))
			return -EFAULT;
		pp_cmd->value = (void *)&flush_buf;
		rc = msm_isp_subdev_ioctl_vpe(
			p_mctl->isp_sdev->sd_vpe, pp_cmd, NULL);
		if (rc == 0) {
			if (copy_to_user((void *)argp,
						&flush_buf,
						sizeof(flush_buf))) {
				ERR_COPY_TO_USER();
				rc = -EFAULT;
			}
			pp_cmd->value = argp;
		}
	}
	break;
	case VPE_CMD_OPERATION_MODE_CFG: {
		struct msm_vpe_op_mode_cfg op_mode_cfg;
		if (sizeof(struct msm_vpe_op_mode_cfg) !=
		pp_cmd->length) {
			D("%s: size mismatch(id=%d, len = %d, expected = %d",
				__func__, pp_cmd->id, pp_cmd->length,
				sizeof(struct msm_vpe_op_mode_cfg));
				rc = -EINVAL;
				break;
		}
		if (copy_from_user(&op_mode_cfg,
			pp_cmd->value,
			sizeof(op_mode_cfg)))
			return -EFAULT;
		pp_cmd->value = (void *)&op_mode_cfg;
		rc = msm_isp_subdev_ioctl_vpe(
			p_mctl->isp_sdev->sd_vpe, pp_cmd, NULL);
		break;
	}
	case VPE_CMD_INPUT_PLANE_CFG: {
		struct msm_vpe_input_plane_cfg input_cfg;
		if (sizeof(struct msm_vpe_input_plane_cfg) !=
			pp_cmd->length) {
			D("%s: mismatch(id=%d, len = %d, expected = %d",
				__func__, pp_cmd->id, pp_cmd->length,
				sizeof(struct msm_vpe_input_plane_cfg));
				rc = -EINVAL;
				break;
		}
		if (copy_from_user(
			&input_cfg, pp_cmd->value, sizeof(input_cfg)))
			return -EFAULT;
		pp_cmd->value = (void *)&input_cfg;
		rc = msm_isp_subdev_ioctl_vpe(
			p_mctl->isp_sdev->sd_vpe, pp_cmd, NULL);
		break;
	}
	case VPE_CMD_OUTPUT_PLANE_CFG: {
		struct msm_vpe_output_plane_cfg output_cfg;
		if (sizeof(struct msm_vpe_output_plane_cfg) !=
			pp_cmd->length) {
			D("%s: size mismatch(id=%d, len = %d, expected = %d",
				__func__, pp_cmd->id, pp_cmd->length,
				sizeof(struct msm_vpe_output_plane_cfg));
				rc = -EINVAL;
				break;
		}
		if (copy_from_user(&output_cfg, pp_cmd->value,
			sizeof(output_cfg))) {
			D("%s: cannot copy pp_cmd->value, size=%d",
				__func__, pp_cmd->length);
			return -EFAULT;
		}
		pp_cmd->value = (void *)&output_cfg;
		rc = msm_isp_subdev_ioctl_vpe(
			p_mctl->isp_sdev->sd_vpe, pp_cmd, NULL);
		break;
	}
	case VPE_CMD_INPUT_PLANE_UPDATE: {
		struct msm_vpe_input_plane_update_cfg input_update_cfg;
		if (sizeof(struct msm_vpe_input_plane_update_cfg) !=
			pp_cmd->length) {
			D("%s: size mismatch(id=%d, len = %d, expected = %d",
				__func__, pp_cmd->id, pp_cmd->length,
				sizeof(struct msm_vpe_input_plane_update_cfg));
				rc = -EINVAL;
				break;
		}
		if (copy_from_user(&input_update_cfg, pp_cmd->value,
			sizeof(input_update_cfg)))
			return -EFAULT;
		pp_cmd->value = (void *)&input_update_cfg;
		rc = msm_isp_subdev_ioctl_vpe(
			p_mctl->isp_sdev->sd_vpe, pp_cmd, NULL);
		break;
	}
	case VPE_CMD_SCALE_CFG_TYPE: {
		struct msm_vpe_scaler_cfg scaler_cfg;
		if (sizeof(struct msm_vpe_scaler_cfg) !=
			pp_cmd->length) {
			D("%s: size mismatch(id=%d, len = %d, expected = %d",
				__func__, pp_cmd->id, pp_cmd->length,
				sizeof(struct msm_vpe_scaler_cfg));
				rc = -EINVAL;
				break;
		}
		if (copy_from_user(&scaler_cfg, pp_cmd->value,
			sizeof(scaler_cfg)))
			return -EFAULT;
		pp_cmd->value = (void *)&scaler_cfg;
		rc = msm_isp_subdev_ioctl_vpe(
			p_mctl->isp_sdev->sd_vpe, pp_cmd, NULL);
		break;
	}
	case VPE_CMD_ZOOM: {
		struct msm_mctl_pp_frame_info *zoom;
		zoom = kmalloc(sizeof(struct msm_mctl_pp_frame_info),
					GFP_ATOMIC);
		if (!zoom) {
			rc = -ENOMEM;
			break;
		}
		if (sizeof(zoom->pp_frame_cmd) != pp_cmd->length) {
			D("%s: size mismatch(id=%d, len = %d, expected = %d",
				__func__, pp_cmd->id, pp_cmd->length,
				sizeof(zoom->pp_frame_cmd));
				rc = -EINVAL;
				kfree(zoom);
				break;
		}
		if (copy_from_user(&zoom->pp_frame_cmd, pp_cmd->value,
			sizeof(zoom->pp_frame_cmd))) {
			kfree(zoom);
			return -EFAULT;
		}
		D("%s: src=0x%x, dest=0x%x,cookie=0x%x,action=0x%x,path=0x%x",
				__func__, zoom->pp_frame_cmd.src_buf_handle,
				zoom->pp_frame_cmd.dest_buf_handle,
				zoom->pp_frame_cmd.cookie,
				zoom->pp_frame_cmd.vpe_output_action,
				zoom->pp_frame_cmd.path);
		idx = msm_mctl_pp_path_to_inst_index(p_mctl->sync.pcam_sync,
			zoom->pp_frame_cmd.path);
		if (idx < 0) {
			pr_err("%s Invalid path, returning\n", __func__);
			kfree(zoom);
			return idx;
		}
		pcam_inst = p_mctl->sync.pcam_sync->dev_inst[idx];
		if (!pcam_inst) {
			pr_err("%s Invalid instance, returning\n", __func__);
			kfree(zoom);
			return -EINVAL;
		}
		zoom->user_cmd = pp_cmd->id;
		rc = msm_mctl_pp_get_phy_addr(pcam_inst,
			zoom->pp_frame_cmd.src_buf_handle, &zoom->src_frame);
		if (rc) {
			kfree(zoom);
			break;
		}
		rc = msm_mctl_pp_get_phy_addr(pcam_inst,
			zoom->pp_frame_cmd.dest_buf_handle, &zoom->dest_frame);
		if (rc) {
			kfree(zoom);
			break;
		}
		rc = msm_mctl_pp_copy_timestamp_and_frame_id(
			zoom->pp_frame_cmd.src_buf_handle,

			zoom->pp_frame_cmd.dest_buf_handle);
		if (rc) {
			kfree(zoom);
			break;
		}
		rc = msm_isp_subdev_ioctl_vpe(
			p_mctl->isp_sdev->sd_vpe, pp_cmd, (void *)zoom);
		if (rc) {
			kfree(zoom);
			break;
		}
		break;
	}
	default:
		rc = -1;
		break;
	}
	return rc;
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
	default:
		return -EINVAL;
	}
}

int msm_mctl_pp_proc_cmd(struct msm_cam_media_controller *p_mctl,
			struct msm_mctl_pp_cmd *pp_cmd)
{
	int rc = 0;
	struct msm_mctl_pp_frame_buffer pp_buffer;
	struct msm_frame_buffer *buf = NULL;
	void __user *argp = (void __user *)pp_cmd->value;
	int img_mode;
	unsigned long flags;

	switch (pp_cmd->id) {
	case MCTL_CMD_GET_FRAME_BUFFER: {
		if (copy_from_user(&pp_buffer, pp_cmd->value,
				sizeof(pp_buffer)))
			return -EFAULT;
		img_mode = msm_mctl_pp_path_to_img_mode(pp_buffer.path);
		if (img_mode < 0) {
			pr_err("%s Invalid image mode\n", __func__);
			return img_mode;
		}
		buf = msm_mctl_get_free_buf(p_mctl, img_mode);
		pp_buffer.buf_handle = (uint32_t)buf;
		if (copy_to_user((void *)argp,
			&pp_buffer,
			sizeof(struct msm_mctl_pp_frame_buffer))) {
			ERR_COPY_TO_USER();
			rc = -EFAULT;
		}
		break;
	}
	case MCTL_CMD_PUT_FRAME_BUFFER: {
		if (copy_from_user(&pp_buffer, pp_cmd->value,
				sizeof(pp_buffer)))
			return -EFAULT;
		img_mode = msm_mctl_pp_path_to_img_mode(pp_buffer.path);
		if (img_mode < 0) {
			pr_err("%s Invalid image mode\n", __func__);
			return img_mode;
		}
		buf = (struct msm_frame_buffer *)pp_buffer.buf_handle;
		msm_mctl_put_free_buf(p_mctl, img_mode, buf);
		break;
	}
	case MCTL_CMD_DIVERT_FRAME_PP_PATH: {
		struct msm_mctl_pp_divert_pp divert_pp;
		if (copy_from_user(&divert_pp, pp_cmd->value,
				sizeof(divert_pp)))
			return -EFAULT;
		D("%s: PP_PATH, path=%d",
			__func__, divert_pp.path);
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
	case MSM_PP_CMD_TYPE_VPE:
		rc = msm_mctl_pp_proc_vpe_cmd(p_mctl, &pp_cmd.cmd);
		break;
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

int msm_mctl_pp_notify(struct msm_cam_media_controller *p_mctl,
			struct msm_mctl_pp_frame_info *pp_frame_info)
{
		struct msm_mctl_pp_frame_cmd *pp_frame_cmd;
		pp_frame_cmd = &pp_frame_info->pp_frame_cmd;

		D("%s: msm_cam_evt_divert_frame=%d",
			__func__, sizeof(struct msm_mctl_pp_event_info));
		if ((MSM_MCTL_PP_VPE_FRAME_TO_APP &
			pp_frame_cmd->vpe_output_action)) {
			struct msm_free_buf done_frame;
			int img_mode =
				msm_mctl_pp_path_to_img_mode(
					pp_frame_cmd->path);
			if (img_mode < 0) {
				pr_err("%s Invalid image mode\n", __func__);
				return img_mode;
			}
			done_frame.ch_paddr[0] =
				pp_frame_info->dest_frame.sp.phy_addr;
			done_frame.vb =
				pp_frame_info->dest_frame.handle;
			msm_mctl_buf_done_pp(
				p_mctl, img_mode, &done_frame, 0, 0);
			D("%s: vpe done to app, vb=0x%x, path=%d, phy=0x%x",
				__func__, done_frame.vb,
				pp_frame_cmd->path, done_frame.ch_paddr[0]);
		}
		if ((MSM_MCTL_PP_VPE_FRAME_ACK &
			pp_frame_cmd->vpe_output_action)) {
			struct v4l2_event v4l2_evt;
			struct msm_mctl_pp_event_info *pp_event_info;
			struct msm_isp_event_ctrl *isp_event;
			isp_event = kzalloc(sizeof(struct msm_isp_event_ctrl),
								GFP_ATOMIC);
			if (!isp_event) {
				pr_err("%s Insufficient memory.", __func__);
				return -ENOMEM;
			}
			memset(&v4l2_evt, 0, sizeof(v4l2_evt));
			*((uint32_t *)v4l2_evt.u.data) = (uint32_t)isp_event;

			/* Get hold of pp event info struct inside event ctrl.*/
			pp_event_info = &(isp_event->isp_data.pp_event_info);

			pp_event_info->event = MCTL_PP_EVENT_CMD_ACK;
			pp_event_info->ack.cmd = pp_frame_info->user_cmd;
			pp_event_info->ack.status = 0;
			pp_event_info->ack.cookie = pp_frame_cmd->cookie;
			v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
						MSM_CAM_RESP_MCTL_PP_EVENT;

			v4l2_event_queue(
				p_mctl->config_device->
					config_stat_event_queue.pvdev,
				&v4l2_evt);
			D("%s: ack to daemon, cookie=0x%x, event = 0x%x",
				__func__, pp_frame_info->pp_frame_cmd.cookie,
				v4l2_evt.type);
		}
		kfree(pp_frame_info); /* free mem */
		return 0;
}

int msm_mctl_pp_reserve_free_frame(
	struct msm_cam_media_controller *p_mctl,
	void __user *arg)
{
	struct msm_cam_evt_divert_frame frame;
	int image_mode, rc = 0;
	struct msm_free_buf free_buf;
	struct msm_cam_v4l2_dev_inst *pcam_inst;

	memset(&free_buf, 0, sizeof(struct msm_free_buf));
	if (copy_from_user(&frame, arg,
		sizeof(struct msm_cam_evt_divert_frame)))
		return -EFAULT;

	image_mode = frame.image_mode;
	if (image_mode <= 0) {
		pr_err("%s Invalid image mode %d", __func__, image_mode);
		return -EINVAL;
	}
	/* Always reserve the buffer from user's video node */
	pcam_inst = p_mctl->sync.pcam_sync->dev_inst[image_mode];
	if (!pcam_inst) {
		pr_err("%s Instance already closed ", __func__);
		return -EINVAL;
	}
	rc = msm_mctl_reserve_free_buf(p_mctl, pcam_inst,
					image_mode, &free_buf);
	if (rc == 0) {
		msm_mctl_pp_get_phy_addr(pcam_inst, free_buf.vb, &frame.frame);
		if (copy_to_user((void *)arg, &frame, sizeof(frame))) {
			ERR_COPY_TO_USER();
			rc = -EFAULT;
		}
	}
	D("%s: reserve free buf got buffer %d from %p rc = %d, phy = 0x%x",
		__func__, frame.frame.buf_idx,
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

	if (copy_from_user(&div_frame, arg,
		sizeof(struct msm_cam_evt_divert_frame)))
		return -EFAULT;

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

	pcam_inst = msm_mctl_get_pcam_inst(p_mctl, image_mode);
	if (!pcam_inst) {
		pr_err("%s Invalid instance. Cannot release frame.\n",
			__func__);
		return -EINVAL;
	}

	rc = msm_mctl_release_free_buf(p_mctl, pcam_inst,
					image_mode, &free_buf);
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
			arg, sizeof(p_mctl->pp_info.pp_key)))
		rc = -EFAULT;
	else
		D("%s: mctl=0x%p, pp_key_setting=0x%x",
			__func__, p_mctl, p_mctl->pp_info.pp_key);
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

	if (copy_from_user(&frame, arg, sizeof(frame)))
		return -EFAULT;

	spin_lock_irqsave(&p_mctl->pp_info.lock, flags);
	image_mode = msm_mctl_pp_path_to_img_mode(frame.path);
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
	/* here buf.addr is phy_addr */
	rc = msm_mctl_buf_done_pp(p_mctl, image_mode, &buf, dirty, 0);
	return rc;
}

int msm_mctl_pp_divert_done(
	struct msm_cam_media_controller *p_mctl,
	void __user *arg)
{
	struct msm_pp_frame frame;
	int msg_type, image_mode, rc = 0;
	int dirty = 0;
	struct msm_free_buf buf;
	unsigned long flags;
	D("%s enter\n", __func__);

	if (copy_from_user(&frame, arg, sizeof(frame)))
		return -EFAULT;

	spin_lock_irqsave(&p_mctl->pp_info.lock, flags);
	D("%s Frame path: %d\n", __func__, frame.path);
	switch (frame.path) {
	case OUTPUT_TYPE_P:
		msg_type = VFE_MSG_OUTPUT_P;
		image_mode = MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW;
		break;
	case OUTPUT_TYPE_S:
		msg_type = VFE_MSG_OUTPUT_S;
		image_mode = MSM_V4L2_EXT_CAPTURE_MODE_MAIN;
		break;
	case OUTPUT_TYPE_V:
		msg_type = VFE_MSG_OUTPUT_V;
		image_mode = MSM_V4L2_EXT_CAPTURE_MODE_VIDEO;
		break;
	case OUTPUT_TYPE_T:
		msg_type = VFE_MSG_OUTPUT_T;
		image_mode = MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL;
		break;
	default:
		rc = -EFAULT;
		goto err;
	}

	if (frame.num_planes > 1)
		buf.ch_paddr[0] = frame.mp[0].phy_addr +
					frame.mp[0].data_offset;
	else
		buf.ch_paddr[0] = frame.sp.phy_addr + frame.sp.y_off;

	spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);
	D("%s Frame done id: %d\n", __func__, frame.frame_id);
	rc = msm_mctl_buf_done_pp(p_mctl, image_mode,
		&buf, dirty, frame.node_type);
	return rc;
err:
	spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);
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
