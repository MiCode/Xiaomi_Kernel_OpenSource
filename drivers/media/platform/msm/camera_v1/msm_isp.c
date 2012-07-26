/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#include <linux/export.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <linux/videodev2.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/android_pmem.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <media/msm_isp.h>
#include <media/msm_gemini.h>

#include "msm.h"

#ifdef CONFIG_MSM_CAMERA_DEBUG
#define D(fmt, args...) pr_debug("msm_isp: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

#define MSM_FRAME_AXI_MAX_BUF 32

/*
 * This function executes in interrupt context.
 */

void *msm_isp_sync_alloc(int size,
	  gfp_t gfp)
{
	struct msm_queue_cmd *qcmd =
		kmalloc(sizeof(struct msm_queue_cmd) + size, gfp);

	if (qcmd) {
		atomic_set(&qcmd->on_heap, 1);
		return qcmd + 1;
	}
	return NULL;
}

void msm_isp_sync_free(void *ptr)
{
	if (ptr) {
		struct msm_queue_cmd *qcmd =
			(struct msm_queue_cmd *)ptr;
		qcmd--;
		if (atomic_read(&qcmd->on_heap))
			kfree(qcmd);
	}
}

static int msm_isp_notify_VFE_SOF_COUNT_EVT(struct v4l2_subdev *sd, void *arg)
{
	struct msm_vfe_cfg_cmd cfgcmd;
	struct msm_camvfe_params vfe_params;
	int rc;

	cfgcmd.cmd_type = CMD_VFE_SOF_COUNT_UPDATE;
	cfgcmd.value = NULL;
	vfe_params.vfe_cfg = &cfgcmd;
	vfe_params.data = arg;
	rc = v4l2_subdev_call(sd, core, ioctl, 0, &vfe_params);
	return 0;
}

int msm_isp_vfe_msg_to_img_mode(struct msm_cam_media_controller *pmctl,
				int vfe_msg)
{
	int image_mode;
	uint32_t vfe_output_mode = pmctl->vfe_output_mode;
	vfe_output_mode &= ~(VFE_OUTPUTS_RDI0|VFE_OUTPUTS_RDI1);
	if (vfe_msg == VFE_MSG_OUTPUT_PRIMARY) {
		switch (vfe_output_mode) {
		case VFE_OUTPUTS_MAIN_AND_PREVIEW:
		case VFE_OUTPUTS_MAIN_AND_VIDEO:
		case VFE_OUTPUTS_MAIN_AND_THUMB:
		case VFE_OUTPUTS_RAW:
		case VFE_OUTPUTS_JPEG_AND_THUMB:
			image_mode = MSM_V4L2_EXT_CAPTURE_MODE_MAIN;
			break;
		case VFE_OUTPUTS_THUMB_AND_MAIN:
			image_mode = MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL;
			break;
		case VFE_OUTPUTS_VIDEO:
		case VFE_OUTPUTS_VIDEO_AND_PREVIEW:
			image_mode = MSM_V4L2_EXT_CAPTURE_MODE_VIDEO;
			break;
		case VFE_OUTPUTS_PREVIEW:
		case VFE_OUTPUTS_PREVIEW_AND_VIDEO:
			image_mode = MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW;
			break;
		default:
			image_mode = -1;
			break;
		}
	} else if (vfe_msg == VFE_MSG_OUTPUT_SECONDARY) {
		switch (vfe_output_mode) {
		case VFE_OUTPUTS_MAIN_AND_PREVIEW:
		case VFE_OUTPUTS_VIDEO_AND_PREVIEW:
			image_mode = MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW;
			break;
		case VFE_OUTPUTS_MAIN_AND_VIDEO:
		case VFE_OUTPUTS_PREVIEW_AND_VIDEO:
			image_mode = MSM_V4L2_EXT_CAPTURE_MODE_VIDEO;
			break;
		case VFE_OUTPUTS_MAIN_AND_THUMB:
			image_mode = MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL;
			break;
		case VFE_OUTPUTS_THUMB_AND_MAIN:
			image_mode = MSM_V4L2_EXT_CAPTURE_MODE_MAIN;
			break;
		case VFE_OUTPUTS_JPEG_AND_THUMB:
			image_mode = MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL;
			break;
		case VFE_OUTPUTS_PREVIEW:
		case VFE_OUTPUTS_VIDEO:
			image_mode = MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW;
			break;
		default:
			image_mode = -1;
			break;
		}
	} else if (vfe_msg == VFE_MSG_OUTPUT_TERTIARY1) {
		if (pmctl->vfe_output_mode & VFE_OUTPUTS_RDI0)
			image_mode = MSM_V4L2_EXT_CAPTURE_MODE_RDI;
		else
			image_mode = -1;
	} else if (vfe_msg == VFE_MSG_OUTPUT_TERTIARY2) {
		if (pmctl->vfe_output_mode & VFE_OUTPUTS_RDI1)
			image_mode = MSM_V4L2_EXT_CAPTURE_MODE_RDI1;
		else
			image_mode = -1;
	} else
		image_mode = -1;

	D("%s Selected image mode %d vfe output mode %d, vfe msg %d\n",
	  __func__, image_mode, pmctl->vfe_output_mode, vfe_msg);
	return image_mode;
}

static int msm_isp_notify_VFE_BUF_EVT(struct v4l2_subdev *sd, void *arg)
{
	int rc = -EINVAL;
	struct msm_vfe_resp *vdata = (struct msm_vfe_resp *)arg;
	struct msm_free_buf free_buf, temp_free_buf;
	struct msm_camvfe_params vfe_params;
	struct msm_vfe_cfg_cmd cfgcmd;
	struct msm_cam_media_controller *pmctl =
		(struct msm_cam_media_controller *)v4l2_get_subdev_hostdata(sd);
	struct msm_cam_v4l2_device *pcam = pmctl->pcam_ptr;
	struct msm_frame_info *frame_info =
		(struct msm_frame_info *)vdata->evt_msg.data;
	uint32_t vfe_id;
	struct msm_cam_buf_handle buf_handle;

	if (!pcam) {
		pr_debug("%s pcam is null. return\n", __func__);
		msm_isp_sync_free(vdata);
		return rc;
	}
	if (frame_info) {
		vfe_id = frame_info->path;
		buf_handle.buf_lookup_type = BUF_LOOKUP_BY_INST_HANDLE;
		buf_handle.inst_handle = frame_info->inst_handle;
	} else {
		vfe_id = vdata->evt_msg.msg_id;
		buf_handle.buf_lookup_type = BUF_LOOKUP_BY_IMG_MODE;
		buf_handle.image_mode =
			msm_isp_vfe_msg_to_img_mode(pmctl, vfe_id);
	}

	switch (vdata->type) {
	case VFE_MSG_START:
	case VFE_MSG_START_RECORDING:
	case VFE_MSG_PREVIEW:
		D("%s Got V32_START_*: Getting ping addr id = %d",
						__func__, vfe_id);
		msm_mctl_reserve_free_buf(pmctl, NULL,
					&buf_handle, &free_buf);
		cfgcmd.cmd_type = CMD_CONFIG_PING_ADDR;
		cfgcmd.value = &vfe_id;
		vfe_params.vfe_cfg = &cfgcmd;
		vfe_params.data = (void *)&free_buf;
		rc = v4l2_subdev_call(sd, core, ioctl, 0, &vfe_params);
		msm_mctl_reserve_free_buf(pmctl, NULL,
					&buf_handle, &free_buf);
		cfgcmd.cmd_type = CMD_CONFIG_PONG_ADDR;
		cfgcmd.value = &vfe_id;
		vfe_params.vfe_cfg = &cfgcmd;
		vfe_params.data = (void *)&free_buf;
		rc = v4l2_subdev_call(sd, core, ioctl, 0, &vfe_params);
		break;
	case VFE_MSG_CAPTURE:
		pr_debug("%s Got V32_CAPTURE: getting buffer for id = %d",
						__func__, vfe_id);
		msm_mctl_reserve_free_buf(pmctl, NULL,
					&buf_handle, &free_buf);
		cfgcmd.cmd_type = CMD_CONFIG_PING_ADDR;
		cfgcmd.value = &vfe_id;
		vfe_params.vfe_cfg = &cfgcmd;
		vfe_params.data = (void *)&free_buf;
		rc = v4l2_subdev_call(sd, core, ioctl, 0, &vfe_params);
		temp_free_buf = free_buf;
		if (msm_mctl_reserve_free_buf(pmctl, NULL,
					&buf_handle, &free_buf)) {
			/* Write the same buffer into PONG */
			free_buf = temp_free_buf;
		}
		cfgcmd.cmd_type = CMD_CONFIG_PONG_ADDR;
		cfgcmd.value = &vfe_id;
		vfe_params.vfe_cfg = &cfgcmd;
		vfe_params.data = (void *)&free_buf;
		rc = v4l2_subdev_call(sd, core, ioctl, 0, &vfe_params);
		break;
	case VFE_MSG_JPEG_CAPTURE:
		D("%s:VFE_MSG_JPEG_CAPTURE vdata->type %d\n", __func__,
			vdata->type);
		free_buf.num_planes = 2;
		free_buf.ch_paddr[0] = pmctl->ping_imem_y;
		free_buf.ch_paddr[1] = pmctl->ping_imem_cbcr;
		cfgcmd.cmd_type = CMD_CONFIG_PING_ADDR;
		cfgcmd.value = &vfe_id;
		vfe_params.vfe_cfg = &cfgcmd;
		vfe_params.data = (void *)&free_buf;
		D("%s:VFE_MSG_JPEG_CAPTURE y_ping=%x cbcr_ping=%x\n",
			__func__, free_buf.ch_paddr[0], free_buf.ch_paddr[1]);
		rc = v4l2_subdev_call(sd, core, ioctl, 0, &vfe_params);
		/* Write the same buffer into PONG */
		free_buf.ch_paddr[0] = pmctl->pong_imem_y;
		free_buf.ch_paddr[1] = pmctl->pong_imem_cbcr;
		cfgcmd.cmd_type = CMD_CONFIG_PONG_ADDR;
		cfgcmd.value = &vfe_id;
		vfe_params.vfe_cfg = &cfgcmd;
		vfe_params.data = (void *)&free_buf;
		D("%s:VFE_MSG_JPEG_CAPTURE y_pong=%x cbcr_pong=%x\n",
			__func__, free_buf.ch_paddr[0], free_buf.ch_paddr[1]);
		rc = v4l2_subdev_call(sd, core, ioctl, 0, &vfe_params);
		break;
	case VFE_MSG_OUTPUT_IRQ:
		D("%s Got OUTPUT_IRQ: Getting free buf id = %d",
						__func__, vfe_id);
		msm_mctl_reserve_free_buf(pmctl, NULL,
					&buf_handle, &free_buf);
		cfgcmd.cmd_type = CMD_CONFIG_FREE_BUF_ADDR;
		cfgcmd.value = &vfe_id;
		vfe_params.vfe_cfg = &cfgcmd;
		vfe_params.data = (void *)&free_buf;
		rc = v4l2_subdev_call(sd, core, ioctl, 0, &vfe_params);
		break;
	default:
		pr_err("%s: Invalid vdata type: %d\n", __func__, vdata->type);
		break;
	}
	return rc;
}

/*
 * This function executes in interrupt context.
 */
static int msm_isp_notify_vfe(struct v4l2_subdev *sd,
	unsigned int notification,  void *arg)
{
	int rc = 0;
	struct v4l2_event v4l2_evt;
	struct msm_isp_event_ctrl *isp_event;
	struct msm_cam_media_controller *pmctl =
		(struct msm_cam_media_controller *)v4l2_get_subdev_hostdata(sd);
	struct msm_free_buf buf;

	if (!pmctl) {
		pr_err("%s: no context in dsp callback.\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	if (notification == NOTIFY_VFE_BUF_EVT)
		return msm_isp_notify_VFE_BUF_EVT(sd, arg);

	if (notification == NOTIFY_VFE_SOF_COUNT)
		return msm_isp_notify_VFE_SOF_COUNT_EVT(sd, arg);

	isp_event = kzalloc(sizeof(struct msm_isp_event_ctrl), GFP_ATOMIC);
	if (!isp_event) {
		pr_err("%s Insufficient memory. return", __func__);
		return -ENOMEM;
	}

	v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
					MSM_CAM_RESP_STAT_EVT_MSG;
	v4l2_evt.id = 0;

	*((uint32_t *)v4l2_evt.u.data) = (uint32_t)isp_event;

	isp_event->resptype = MSM_CAM_RESP_STAT_EVT_MSG;
	isp_event->isp_data.isp_msg.type = MSM_CAMERA_MSG;
	isp_event->isp_data.isp_msg.len = 0;

	switch (notification) {
	case NOTIFY_ISP_MSG_EVT: {
		struct isp_msg_event *isp_msg = (struct isp_msg_event *)arg;

		isp_event->isp_data.isp_msg.msg_id = isp_msg->msg_id;
		isp_event->isp_data.isp_msg.frame_id = isp_msg->sof_count;
		getnstimeofday(&(isp_event->isp_data.isp_msg.timestamp));
		break;
	}
	case NOTIFY_VFE_MSG_OUT: {
		uint8_t msgid;
		struct msm_cam_buf_handle buf_handle;
		struct isp_msg_output *isp_output =
				(struct isp_msg_output *)arg;
		if (!isp_output->buf.inst_handle) {
			switch (isp_output->output_id) {
			case MSG_ID_OUTPUT_P:
				msgid = VFE_MSG_OUTPUT_P;
				break;
			case MSG_ID_OUTPUT_V:
				msgid = VFE_MSG_OUTPUT_V;
				break;
			case MSG_ID_OUTPUT_T:
				msgid = VFE_MSG_OUTPUT_T;
				break;
			case MSG_ID_OUTPUT_S:
				msgid = VFE_MSG_OUTPUT_S;
				break;
			case MSG_ID_OUTPUT_PRIMARY:
				msgid = VFE_MSG_OUTPUT_PRIMARY;
				break;
			case MSG_ID_OUTPUT_SECONDARY:
				msgid = VFE_MSG_OUTPUT_SECONDARY;
				break;
			case MSG_ID_OUTPUT_TERTIARY1:
				msgid = VFE_MSG_OUTPUT_TERTIARY1;
				break;
			case MSG_ID_OUTPUT_TERTIARY2:
				msgid = VFE_MSG_OUTPUT_TERTIARY2;
				break;

			default:
				pr_err("%s: Invalid VFE output id: %d\n",
					   __func__, isp_output->output_id);
				rc = -EINVAL;
				break;
			}
			if (!rc) {
				buf_handle.buf_lookup_type =
					BUF_LOOKUP_BY_IMG_MODE;
				buf_handle.image_mode =
				msm_isp_vfe_msg_to_img_mode(pmctl, msgid);
			}
		} else {
			buf_handle.buf_lookup_type = BUF_LOOKUP_BY_INST_HANDLE;
			buf_handle.inst_handle = isp_output->buf.inst_handle;
		}
		isp_event->isp_data.isp_msg.msg_id =
			isp_output->output_id;
		isp_event->isp_data.isp_msg.frame_id =
			isp_output->frameCounter;
		buf = isp_output->buf;
		msm_mctl_buf_done(pmctl, &buf_handle,
			&buf, isp_output->frameCounter);
		}
		break;
	case NOTIFY_VFE_MSG_COMP_STATS: {
		struct msm_stats_buf *stats = (struct msm_stats_buf *)arg;
		struct msm_stats_buf *stats_buf = NULL;

		isp_event->isp_data.isp_msg.msg_id = MSG_ID_STATS_COMPOSITE;
		stats->aec.buff = msm_pmem_stats_ptov_lookup(pmctl,
					stats->aec.buff, &(stats->aec.fd));
		stats->awb.buff = msm_pmem_stats_ptov_lookup(pmctl,
					stats->awb.buff, &(stats->awb.fd));
		stats->af.buff = msm_pmem_stats_ptov_lookup(pmctl,
					stats->af.buff, &(stats->af.fd));
		stats->ihist.buff = msm_pmem_stats_ptov_lookup(pmctl,
					stats->ihist.buff, &(stats->ihist.fd));
		stats->rs.buff = msm_pmem_stats_ptov_lookup(pmctl,
					stats->rs.buff, &(stats->rs.fd));
		stats->cs.buff = msm_pmem_stats_ptov_lookup(pmctl,
					stats->cs.buff, &(stats->cs.fd));

		stats_buf = kmalloc(sizeof(struct msm_stats_buf), GFP_ATOMIC);
		if (!stats_buf) {
			pr_err("%s: out of memory.\n", __func__);
			rc = -ENOMEM;
		} else {
			*stats_buf = *stats;
			isp_event->isp_data.isp_msg.len	=
				sizeof(struct msm_stats_buf);
			isp_event->isp_data.isp_msg.data = stats_buf;
		}
		}
		break;
	case NOTIFY_VFE_MSG_STATS: {
		struct msm_stats_buf stats;
		struct isp_msg_stats *isp_stats = (struct isp_msg_stats *)arg;

		memset(&stats, 0, sizeof(stats));
		isp_event->isp_data.isp_msg.msg_id = isp_stats->id;
		isp_event->isp_data.isp_msg.frame_id =
			isp_stats->frameCounter;
		stats.buffer = isp_stats->buffer;
		stats.fd = isp_stats->fd;
		/* buf_idx used for O(0) lookup */
		stats.buf_idx = isp_stats->buf_idx;
		switch (isp_stats->id) {
		case MSG_ID_STATS_AEC:
		case MSG_ID_STATS_BG:
			stats.aec.buff = stats.buffer;
			stats.aec.fd = stats.fd;
			break;
		case MSG_ID_STATS_AF:
		case MSG_ID_STATS_BF:
			stats.af.buff = stats.buffer;
			stats.af.fd = stats.fd;
			break;
		case MSG_ID_STATS_AWB:
			stats.awb.buff = stats.buffer;
			stats.awb.fd = stats.fd;
			break;
		case MSG_ID_STATS_IHIST:
			stats.ihist.buff = stats.buffer;
			stats.ihist.fd = stats.fd;
			break;
		case MSG_ID_STATS_RS:
			stats.rs.buff = stats.buffer;
			stats.rs.fd = stats.fd;
			break;
		case MSG_ID_STATS_CS:
			stats.cs.buff = stats.buffer;
			stats.cs.fd = stats.fd;
			break;
		case MSG_ID_STATS_BHIST:
			stats.skin.buff = stats.buffer;
			stats.skin.fd = stats.fd;
			break;
		case MSG_ID_STATS_AWB_AEC:
			break;
		default:
			pr_err("%s: Invalid msg type", __func__);
			break;
		}
		if (!stats.buffer) {
			pr_err("%s: msm_pmem_stats_ptov_lookup error\n",
							__func__);
			isp_event->isp_data.isp_msg.len = 0;
			rc = -EFAULT;
		} else {
			struct msm_stats_buf *stats_buf =
				kmalloc(sizeof(struct msm_stats_buf),
							GFP_ATOMIC);
			if (!stats_buf) {
				pr_err("%s: out of memory. stats_id = %d\n",
					__func__, isp_stats->id);
				rc = -ENOMEM;
			} else {
				*stats_buf = stats;
				isp_event->isp_data.isp_msg.len	=
					sizeof(struct msm_stats_buf);
				isp_event->isp_data.isp_msg.data = stats_buf;
			}
		}
		}
		break;
	default:
		pr_err("%s: Unsupport isp notification %d\n",
			__func__, notification);
		rc = -EINVAL;
		break;
	}

	v4l2_event_queue(pmctl->config_device->config_stat_event_queue.pvdev,
			 &v4l2_evt);

	return rc;
}

static int msm_isp_notify(struct v4l2_subdev *sd,
	unsigned int notification, void *arg)
{
	return msm_isp_notify_vfe(sd, notification, arg);
}

static int msm_config_vfe(struct v4l2_subdev *sd,
	struct msm_cam_media_controller *mctl, void __user *arg)
{
	struct msm_vfe_cfg_cmd cfgcmd;
	struct axidata axi_data;

	if (copy_from_user(&cfgcmd, arg, sizeof(cfgcmd))) {
		ERR_COPY_FROM_USER();
		return -EFAULT;
	}

	memset(&axi_data, 0, sizeof(axi_data));
	CDBG("%s: cmd_type %d\n", __func__, cfgcmd.cmd_type);
	switch (cfgcmd.cmd_type) {
	case CMD_STATS_BG_ENABLE:
	case CMD_STATS_BF_ENABLE:
	case CMD_STATS_BHIST_ENABLE:
	case CMD_STATS_AF_ENABLE:
	case CMD_STATS_AEC_ENABLE:
	case CMD_STATS_AWB_ENABLE:
	case CMD_STATS_AEC_AWB_ENABLE:
	case CMD_STATS_IHIST_ENABLE:
	case CMD_STATS_RS_ENABLE:
	case CMD_STATS_CS_ENABLE:
		return msm_isp_subdev_ioctl(sd, &cfgcmd, NULL);
	case CMD_GENERAL:
	case CMD_STATS_DISABLE:
		return msm_isp_subdev_ioctl(sd, &cfgcmd,
							&axi_data);
	default:
		pr_err("%s: unknown command type %d\n",
			__func__, cfgcmd.cmd_type);
	}

	return -EINVAL;
}

static int msm_axi_config(struct v4l2_subdev *sd,
		struct msm_cam_media_controller *mctl, void __user *arg)
{
	struct msm_vfe_cfg_cmd cfgcmd;

	if (copy_from_user(&cfgcmd, arg, sizeof(cfgcmd))) {
		ERR_COPY_FROM_USER();
		return -EFAULT;
	}

	switch (cfgcmd.cmd_type) {
	case CMD_AXI_CFG_PRIM:
	case CMD_AXI_CFG_SEC:
	case CMD_AXI_CFG_ZSL:
	case CMD_RAW_PICT_AXI_CFG:
	case CMD_AXI_CFG_PRIM_ALL_CHNLS:
	case CMD_AXI_CFG_PRIM|CMD_AXI_CFG_SEC:
	case CMD_AXI_CFG_PRIM|CMD_AXI_CFG_SEC_ALL_CHNLS:
	case CMD_AXI_CFG_PRIM_ALL_CHNLS|CMD_AXI_CFG_SEC:
	case CMD_AXI_START:
	case CMD_AXI_STOP:
	case CMD_AXI_CFG_TERT1:
	case CMD_AXI_CFG_TERT2:
		/* Dont need to pass buffer information.
		 * subdev will get the buffer from media
		 * controller free queue.
		 */
		return msm_isp_subdev_ioctl(sd, &cfgcmd, NULL);

	default:
		pr_err("%s: unknown command type %d\n",
			__func__,
			cfgcmd.cmd_type);
		return -EINVAL;
	}

	return 0;
}

static int msm_put_stats_buffer(struct v4l2_subdev *sd,
			struct msm_cam_media_controller *mctl, void __user *arg)
{
	int rc = -EIO;

	struct msm_stats_buf buf;
	unsigned long pphy;
	struct msm_vfe_cfg_cmd cfgcmd;

	if (copy_from_user(&buf, arg,
				sizeof(struct msm_stats_buf))) {
		ERR_COPY_FROM_USER();
		return -EFAULT;
	}

	CDBG("%s\n", __func__);
	pphy = msm_pmem_stats_vtop_lookup(mctl, buf.buffer, buf.fd);

	if (pphy != 0) {
		if (buf.type == STAT_AF)
			cfgcmd.cmd_type = CMD_STATS_AF_BUF_RELEASE;
		else if (buf.type == STAT_AEC)
			cfgcmd.cmd_type = CMD_STATS_AEC_BUF_RELEASE;
		else if (buf.type == STAT_AWB)
			cfgcmd.cmd_type = CMD_STATS_AWB_BUF_RELEASE;
		else if (buf.type == STAT_IHIST)
			cfgcmd.cmd_type = CMD_STATS_IHIST_BUF_RELEASE;
		else if (buf.type == STAT_RS)
			cfgcmd.cmd_type = CMD_STATS_RS_BUF_RELEASE;
		else if (buf.type == STAT_CS)
			cfgcmd.cmd_type = CMD_STATS_CS_BUF_RELEASE;
		else if (buf.type == STAT_AEAW)
			cfgcmd.cmd_type = CMD_STATS_BUF_RELEASE;
		else if (buf.type == STAT_BG)
			cfgcmd.cmd_type = CMD_STATS_BG_BUF_RELEASE;
		else if (buf.type == STAT_BF)
			cfgcmd.cmd_type = CMD_STATS_BF_BUF_RELEASE;
		else if (buf.type == STAT_BHIST)
			cfgcmd.cmd_type = CMD_STATS_BHIST_BUF_RELEASE;

		else {
			pr_err("%s: invalid buf type %d\n",
				__func__,
				buf.type);
			rc = -EINVAL;
			goto put_done;
		}

		cfgcmd.value = (void *)&buf;

		rc = msm_isp_subdev_ioctl(sd, &cfgcmd, &pphy);
	} else {
		pr_err("%s: NULL physical address\n", __func__);
		rc = -EINVAL;
	}

put_done:
	return rc;
}

static int msm_vfe_stats_buf_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd,
	struct msm_cam_media_controller *mctl,
	void __user *arg)
{
	struct msm_vfe_cfg_cmd cfgcmd;
	int rc = 0;
	switch (cmd) {
	case MSM_CAM_IOCTL_STATS_REQBUF: {
		struct msm_stats_reqbuf reqbuf;
		if (copy_from_user(&reqbuf, arg,
			sizeof(struct msm_stats_reqbuf))) {
			ERR_COPY_FROM_USER();
			return -EFAULT;
		}
	cfgcmd.cmd_type = VFE_CMD_STATS_REQBUF;
	cfgcmd.value = (void *)&reqbuf;
	cfgcmd.length = sizeof(struct msm_stats_reqbuf);
	rc = msm_isp_subdev_ioctl(sd, &cfgcmd, (void *)mctl->client);
	break;
	}
	case MSM_CAM_IOCTL_STATS_ENQUEUEBUF: {
		struct msm_stats_buf_info buf_info;
		if (copy_from_user(&buf_info, arg,
			sizeof(struct msm_stats_buf_info))) {
			ERR_COPY_FROM_USER();
			return -EFAULT;
		}
	cfgcmd.cmd_type = VFE_CMD_STATS_ENQUEUEBUF;
	cfgcmd.value = (void *)&buf_info;
	cfgcmd.length = sizeof(struct msm_stats_buf_info);
	rc = msm_isp_subdev_ioctl(sd, &cfgcmd, NULL);
	break;
	}
	case MSM_CAM_IOCTL_STATS_FLUSH_BUFQ: {
		struct msm_stats_flush_bufq bufq_info;
		if (copy_from_user(&bufq_info, arg,
			sizeof(struct msm_stats_flush_bufq))) {
			ERR_COPY_FROM_USER();
			return -EFAULT;
		}
	cfgcmd.cmd_type = VFE_CMD_STATS_FLUSH_BUFQ;
	cfgcmd.value = (void *)&bufq_info;
	cfgcmd.length = sizeof(struct msm_stats_flush_bufq);
	rc = msm_isp_subdev_ioctl(sd, &cfgcmd, NULL);
	break;
	}
	case MSM_CAM_IOCTL_STATS_UNREG_BUF: {
		struct msm_stats_reqbuf reqbuf;
		if (copy_from_user(&reqbuf, arg,
			sizeof(struct msm_stats_reqbuf))) {
			ERR_COPY_FROM_USER();
			return -EFAULT;
		}
	cfgcmd.cmd_type = VFE_CMD_STATS_UNREGBUF;
	cfgcmd.value = (void *)&reqbuf;
	cfgcmd.length = sizeof(struct msm_stats_reqbuf);
	rc = msm_isp_subdev_ioctl(sd, &cfgcmd, (void *)mctl->client);
	break;
	}
	default:
		rc = -1;
	break;
	}
	CDBG("%s\n", __func__);
	return rc;
}
/* config function simliar to origanl msm_ioctl_config*/
static int msm_isp_config(struct msm_cam_media_controller *pmctl,
			 unsigned int cmd, unsigned long arg)
{

	int rc = -EINVAL;
	void __user *argp = (void __user *)arg;
	struct v4l2_subdev *sd = pmctl->isp_sdev->sd;

	D("%s: cmd %d\n", __func__, _IOC_NR(cmd));
	switch (cmd) {
	case MSM_CAM_IOCTL_CONFIG_VFE:
		/* Coming from config thread for update */
		rc = msm_config_vfe(sd, pmctl, argp);
		break;

	case MSM_CAM_IOCTL_AXI_CONFIG:
		D("Received MSM_CAM_IOCTL_AXI_CONFIG\n");
		rc = msm_axi_config(sd, pmctl, argp);
		break;

	case MSM_CAM_IOCTL_RELEASE_STATS_BUFFER:
		rc = msm_put_stats_buffer(sd, pmctl, argp);
		break;

	case MSM_CAM_IOCTL_STATS_REQBUF:
	case MSM_CAM_IOCTL_STATS_ENQUEUEBUF:
	case MSM_CAM_IOCTL_STATS_FLUSH_BUFQ:
	case MSM_CAM_IOCTL_STATS_UNREG_BUF:
		rc = msm_vfe_stats_buf_ioctl(sd, cmd, pmctl, argp);
		break;

	default:
		break;
	}

	D("%s: cmd %d DONE\n", __func__, _IOC_NR(cmd));

	return rc;
}

static struct msm_isp_ops isp_subdev[MSM_MAX_CAMERA_CONFIGS];

/**/
int msm_isp_init_module(int g_num_config_nodes)
{
	int i = 0;

	for (i = 0; i < g_num_config_nodes; i++) {
		isp_subdev[i].isp_config = msm_isp_config;
		isp_subdev[i].isp_notify = msm_isp_notify;
	}
	return 0;
}
EXPORT_SYMBOL(msm_isp_init_module);

/*
*/
int msm_isp_register(struct msm_cam_server_dev *psvr)
{
	int i = 0;

	D("%s\n", __func__);

	BUG_ON(!psvr);

	/* Initialize notify function for v4l2_dev */
	for (i = 0; i < psvr->config_info.num_config_nodes; i++)
		psvr->isp_subdev[i] = &(isp_subdev[i]);

	return 0;
}
EXPORT_SYMBOL(msm_isp_register);

/**/
void msm_isp_unregister(struct msm_cam_server_dev *psvr)
{
	int i = 0;
	for (i = 0; i < psvr->config_info.num_config_nodes; i++)
		psvr->isp_subdev[i] = NULL;
}

int msm_isp_subdev_ioctl(struct v4l2_subdev *isp_subdev,
	struct msm_vfe_cfg_cmd *cfgcmd, void *data)
{
	struct msm_camvfe_params vfe_params;
	vfe_params.vfe_cfg = cfgcmd;
	vfe_params.data = data;
	return v4l2_subdev_call(isp_subdev, core, ioctl, 0, &vfe_params);
}
