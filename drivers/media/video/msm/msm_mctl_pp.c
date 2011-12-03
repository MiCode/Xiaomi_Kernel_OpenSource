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
	int msg_type, int *pp_divert_type, int *pp_type)
{
	int rc = 0;
	unsigned long flags;
	uint32_t pp_key = 0;
	int image_mode = MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT;

	*pp_type = 0;
	*pp_divert_type = 0;
	spin_lock_irqsave(&p_mctl->pp_info.lock, flags);
	switch (msg_type) {
	case VFE_MSG_OUTPUT_P:
		pp_key = PP_PREV;
		image_mode = MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW;
		if (p_mctl->pp_info.pp_key & pp_key)
			*pp_divert_type = OUTPUT_TYPE_P;
		if (p_mctl->pp_info.pp_ctrl.pp_msg_type & OUTPUT_TYPE_P)
			*pp_type = OUTPUT_TYPE_P;
		break;
	case VFE_MSG_OUTPUT_S:
		pp_key = PP_SNAP;
		image_mode = MSM_V4L2_EXT_CAPTURE_MODE_MAIN;
		if (p_mctl->pp_info.pp_key & pp_key)
			*pp_divert_type = OUTPUT_TYPE_S;
		if (p_mctl->pp_info.pp_ctrl.pp_msg_type & OUTPUT_TYPE_S)
			*pp_type = OUTPUT_TYPE_P;
		break;
	case VFE_MSG_OUTPUT_V:
		if (p_mctl->pp_info.pp_ctrl.pp_msg_type & OUTPUT_TYPE_V)
			*pp_type = OUTPUT_TYPE_V;
		break;
	case VFE_MSG_OUTPUT_T:
		if (p_mctl->pp_info.pp_ctrl.pp_msg_type & OUTPUT_TYPE_T)
			*pp_type = OUTPUT_TYPE_T;
		break;
	default:
		break;
	}
	if (p_mctl->pp_info.div_frame[image_mode].ch_paddr[0])
		*pp_divert_type = 0;
	spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);
	D("%s: pp_type=%d, pp_divert_type = %d",
		__func__, *pp_type, *pp_divert_type);
	return rc;
}

int msm_mctl_do_pp_divert(
	struct msm_cam_media_controller *p_mctl,
	int msg_type, struct msm_free_buf *fbuf,
	uint32_t frame_id, int pp_type)
{
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	int idx, rc = 0, i, buf_idx;
	int del_buf = 0; /* delete from free queue */
	struct msm_cam_evt_divert_frame div;
	struct msm_frame_buffer *vb = NULL;
	struct videobuf2_contig_pmem *mem;

	idx = msm_mctl_out_type_to_inst_index(
		p_mctl->sync.pcam_sync, msg_type);
	if (idx < 0) {
		pr_err("%s Invalid instance. returning\n", __func__);
		return -EINVAL;
	}
	pcam_inst = p_mctl->sync.pcam_sync->dev_inst[idx];
	vb = msm_mctl_buf_find(p_mctl, pcam_inst,
		  del_buf, msg_type, fbuf);
	if (!vb)
		return -EINVAL;

	vb->vidbuf.v4l2_buf.sequence = frame_id;
	buf_idx = vb->vidbuf.v4l2_buf.index;
	div.image_mode = pcam_inst->image_mode;
	div.op_mode    = pcam_inst->pcam->op_mode;
	div.inst_idx   = pcam_inst->my_index;
	div.node_idx   = pcam_inst->pcam->vnode_id;
	p_mctl->pp_info.cur_frame_id[pcam_inst->image_mode] = frame_id;
	div.frame.frame_id =
		p_mctl->pp_info.cur_frame_id[pcam_inst->image_mode];
	div.frame.handle = (uint32_t)vb;
	msm_mctl_gettimeofday(&div.frame.timestamp);
	vb->vidbuf.v4l2_buf.timestamp = div.frame.timestamp;
	div.do_pp = pp_type;
	/* Get the cookie for 1st plane and store the path.
	 * Also use this to check the number of planes in
	 * this buffer.*/
	mem = vb2_plane_cookie(&vb->vidbuf, 0);
	div.frame.path = mem->path;
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
			div.frame.mp[i].data_offset =
			pcam_inst->buf_offset[buf_idx][i].data_offset;
			div.frame.mp[i].addr_offset =
				mem->addr_offset;
			div.frame.mp[i].fd = (int)mem->vaddr;
			div.frame.mp[i].length = mem->size;
		}
		if (!pp_type)
			p_mctl->pp_info.div_frame[pcam_inst->image_mode].
			ch_paddr[0] = div.frame.mp[0].phy_addr;
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
			D("%s: vpe cmd size mismatch "
				"(id=%d, length = %d, expect size = %d",
				__func__, pp_cmd->id, pp_cmd->length,
				sizeof(struct msm_vpe_clock_rate));
				rc = -EINVAL;
				break;
		}
		if (copy_from_user(&clk_rate, pp_cmd->value,
			sizeof(struct msm_vpe_clock_rate))) {
			D("%s:clk_rate copy failed", __func__);
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

static int msm_mctl_pp_path_to_msg_type(int path)
{
	switch (path) {
	case OUTPUT_TYPE_P:
		return VFE_MSG_OUTPUT_P;
	case OUTPUT_TYPE_V:
		return VFE_MSG_OUTPUT_V;
	case OUTPUT_TYPE_S:
		return VFE_MSG_OUTPUT_S;
	case OUTPUT_TYPE_T:
	default:
		return VFE_MSG_OUTPUT_T;
	}
}

int msm_mctl_pp_proc_cmd(struct msm_cam_media_controller *p_mctl,
			struct msm_mctl_pp_cmd *pp_cmd)
{
	int rc = 0;
	struct msm_mctl_pp_frame_buffer pp_buffer;
	struct msm_frame_buffer *buf = NULL;
	void __user *argp = (void __user *)pp_cmd->value;
	int msg_type = VFE_MSG_OUTPUT_V;
	unsigned long flags;

	switch (pp_cmd->id) {
	case MCTL_CMD_GET_FRAME_BUFFER: {
		if (copy_from_user(&pp_buffer, pp_cmd->value,
				sizeof(pp_buffer)))
			return -EFAULT;
		msg_type = msm_mctl_pp_path_to_msg_type(pp_buffer.path);
		buf = msm_mctl_get_free_buf(p_mctl, msg_type);
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
		msg_type = msm_mctl_pp_path_to_msg_type(pp_buffer.path);
			buf = (struct msm_frame_buffer *)pp_buffer.buf_handle;
		msm_mctl_put_free_buf(p_mctl, msg_type, buf);
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
			int msg_type =
				msm_mctl_pp_path_to_msg_type(
					pp_frame_cmd->path);

			done_frame.ch_paddr[0] =
				pp_frame_info->dest_frame.sp.phy_addr;
			done_frame.vb =
				pp_frame_info->dest_frame.handle;
			msm_mctl_buf_done_pp(
				p_mctl, msg_type, &done_frame, 0);
			pr_info("%s: vpe done to app, vb=0x%x, path=%d, phy=0x%x",
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
	int msg_type, image_mode, rc = 0;
	struct msm_free_buf free_buf;
	memset(&free_buf, 0, sizeof(struct msm_free_buf));
	if (copy_from_user(&frame, arg,
		sizeof(struct msm_cam_evt_divert_frame)))
		return -EFAULT;
	switch (frame.frame.path) {
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
	default:
		rc = -EFAULT;
		return rc;
	}
	rc = msm_mctl_reserve_free_buf(p_mctl, msg_type, &free_buf);
	if (rc == 0) {
		frame.frame.sp.phy_addr = free_buf.ch_paddr[0];
		frame.frame.handle = free_buf.vb;
		if (copy_to_user((void *)arg,
				&frame,
				sizeof(frame))) {
			ERR_COPY_TO_USER();
			rc = -EFAULT;
		}
	}
	D("%s: reserve free buf, rc = %d, phy = 0x%x",
		__func__, rc, free_buf.ch_paddr[0]);
	return rc;
}

int msm_mctl_pp_release_free_frame(
	struct msm_cam_media_controller *p_mctl,
	void __user *arg)
{
	struct msm_cam_evt_divert_frame frame;
	int msg_type, image_mode, rc = 0;
	struct msm_free_buf free_buf;

	if (copy_from_user(&frame, arg,
		sizeof(struct msm_cam_evt_divert_frame)))
		return -EFAULT;
	switch (frame.frame.path) {
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
	default:
		rc = -EFAULT;
		return rc;
	}
	free_buf.ch_paddr[0] = frame.frame.sp.phy_addr;
	rc = msm_mctl_release_free_buf(p_mctl, msg_type, &free_buf);
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
	int msg_type, image_mode, rc = 0;
	int dirty = 0;
	struct msm_free_buf buf;
	unsigned long flags;

	if (copy_from_user(&frame, arg, sizeof(frame)))
		return -EFAULT;

	spin_lock_irqsave(&p_mctl->pp_info.lock, flags);
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
	default:
		rc = -EFAULT;
		goto err;
	}
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
			buf.ch_paddr[0] = frame.mp[0].phy_addr;
		else
			buf.ch_paddr[0] = frame.sp.phy_addr;
	}
	spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);
	/* here buf.addr is phy_addr */
	rc = msm_mctl_buf_done_pp(p_mctl, msg_type, &buf, dirty);
	return rc;
err:
	spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);
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

	if (copy_from_user(&frame, arg, sizeof(frame)))
		return -EFAULT;

	spin_lock_irqsave(&p_mctl->pp_info.lock, flags);
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
	default:
		rc = -EFAULT;
		goto err;
	}
	if (frame.num_planes > 1)
		buf.ch_paddr[0] = frame.mp[0].phy_addr;
	else
		buf.ch_paddr[0] = frame.sp.phy_addr;
	spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);
	/* here buf.addr is phy_addr */
	rc = msm_mctl_buf_done_pp(p_mctl, msg_type, &buf, dirty);
	return rc;
err:
	spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);
	return rc;
}


