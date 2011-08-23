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
#include <linux/android_pmem.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <media/msm_isp.h>

#include "msm.h"

#ifdef CONFIG_MSM_CAMERA_DEBUG
#define D(fmt, args...) pr_debug("msm_isp: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif
#define ERR_USER_COPY(to) pr_err("%s(%d): copy %s user\n", \
				__func__, __LINE__, ((to) ? "to" : "from"))
#define ERR_COPY_FROM_USER() ERR_USER_COPY(0)
#define ERR_COPY_TO_USER() ERR_USER_COPY(1)

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

static int msm_isp_notify_VFE_BUF_EVT(struct v4l2_subdev *sd, void *arg)
{
	int rc = -EINVAL;
	struct msm_vfe_resp *vdata = (struct msm_vfe_resp *)arg;
	struct msm_free_buf free_buf;
	struct msm_camvfe_params vfe_params;
	struct msm_vfe_cfg_cmd cfgcmd;
	struct msm_sync *sync =
		(struct msm_sync *)v4l2_get_subdev_hostdata(sd);
	struct msm_cam_v4l2_device *pcam = sync->pcam_sync;

	int vfe_id = vdata->evt_msg.msg_id;

	switch (vdata->type) {
	case VFE_MSG_V32_START:
	case VFE_MSG_V32_START_RECORDING:
		D("%s Got V32_START_*: Getting ping addr id = %d",
						__func__, vfe_id);
		msm_mctl_reserve_free_buf(&pcam->mctl, vfe_id, &free_buf);
		cfgcmd.cmd_type = CMD_CONFIG_PING_ADDR;
		cfgcmd.value = &vfe_id;
		vfe_params.vfe_cfg = &cfgcmd;
		vfe_params.data = (void *)&free_buf;
		rc = v4l2_subdev_call(sd, core, ioctl, 0, &vfe_params);
		msm_mctl_reserve_free_buf(&pcam->mctl, vfe_id, &free_buf);
		cfgcmd.cmd_type = CMD_CONFIG_PONG_ADDR;
		cfgcmd.value = &vfe_id;
		vfe_params.vfe_cfg = &cfgcmd;
		vfe_params.data = (void *)&free_buf;
		rc = v4l2_subdev_call(sd, core, ioctl, 0, &vfe_params);
		break;
	case VFE_MSG_V32_CAPTURE:
		pr_err("%s Got V32_CAPTURE: getting buffer for id = %d",
						__func__, vfe_id);
		msm_mctl_reserve_free_buf(&pcam->mctl, vfe_id, &free_buf);
		cfgcmd.cmd_type = CMD_CONFIG_PING_ADDR;
		cfgcmd.value = &vfe_id;
		vfe_params.vfe_cfg = &cfgcmd;
		vfe_params.data = (void *)&free_buf;
		rc = v4l2_subdev_call(sd, core, ioctl, 0, &vfe_params);
		/* Write the same buffer into PONG */
		cfgcmd.cmd_type = CMD_CONFIG_PONG_ADDR;
		cfgcmd.value = &vfe_id;
		vfe_params.vfe_cfg = &cfgcmd;
		vfe_params.data = (void *)&free_buf;
		rc = v4l2_subdev_call(sd, core, ioctl, 0, &vfe_params);
		break;
	case VFE_MSG_OUTPUT_IRQ:
		D("%s Got OUTPUT_IRQ: Getting free buf id = %d",
						__func__, vfe_id);
		msm_mctl_reserve_free_buf(&pcam->mctl, vfe_id, &free_buf);
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
	msm_isp_sync_free(vdata);
	return rc;
}

/*
 * This function executes in interrupt context.
 */
static int msm_isp_notify(struct v4l2_subdev *sd,
	unsigned int notification,  void *arg)
{
	int rc = 0;
	struct v4l2_event v4l2_evt;
	struct msm_isp_stats_event_ctrl *isp_event;
	struct msm_sync *sync =
		(struct msm_sync *)v4l2_get_subdev_hostdata(sd);
	struct msm_cam_media_controller *pmctl = &sync->pcam_sync->mctl;
	struct msm_free_buf buf;

	if (!sync) {
		pr_err("%s: no context in dsp callback.\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	if (notification == NOTIFY_VFE_BUF_EVT)
		return msm_isp_notify_VFE_BUF_EVT(sd, arg);

	v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
					MSM_CAM_RESP_STAT_EVT_MSG;
	isp_event = (struct msm_isp_stats_event_ctrl *)v4l2_evt.u.data;
	isp_event->resptype = MSM_CAM_RESP_STAT_EVT_MSG;
	isp_event->isp_data.isp_msg.type = MSM_CAMERA_MSG;
	isp_event->isp_data.isp_msg.len = 0;

	switch (notification) {
	case NOTIFY_ISP_MSG_EVT:
		isp_event->isp_data.isp_msg.msg_id = (uint32_t)arg;
		break;
	case NOTIFY_VFE_MSG_OUT: {
		uint8_t msgid;
		struct isp_msg_output *isp_output =
				(struct isp_msg_output *)arg;
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
		default:
			pr_err("%s: Invalid VFE output id: %d\n",
				__func__, isp_output->output_id);
			rc = -EINVAL;
			break;
		}

		if (!rc) {
			isp_event->isp_data.isp_msg.msg_id =
				isp_output->output_id;
			buf = isp_output->buf;
			msm_mctl_buf_done(pmctl, msgid,
				&buf, isp_output->frameCounter);
		}
		}
		break;
	case NOTIFY_VFE_MSG_STATS: {
		struct msm_stats_buf stats;
		struct isp_msg_stats *isp_stats = (struct isp_msg_stats *)arg;

		isp_event->isp_data.isp_msg.msg_id = isp_stats->id;
		stats.buffer = msm_pmem_stats_ptov_lookup(&pmctl->sync,
						isp_stats->buffer,
						&(stats.fd));
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
				pr_err("%s: out of memory.\n",
							__func__);
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

/* This function is called by open() function, so we need to init HW*/
static int msm_isp_open(struct v4l2_subdev *sd, struct msm_sync *sync)
{
	/* init vfe and senor, register sync callbacks for init*/
	int rc = 0;
	D("%s\n", __func__);
	if (!sync) {
		pr_err("%s: param is NULL", __func__);
		return -EINVAL;
	}


	rc = msm_vfe_subdev_init(sd, sync, sync->pdev);
	if (rc < 0) {
		pr_err("%s: vfe_init failed at %d\n",
					__func__, rc);
	}

	return rc;
}

static void msm_isp_release(struct msm_sync *psync)
{
	D("%s\n", __func__);
	msm_vfe_subdev_release(psync->pdev);
}

static int msm_config_vfe(struct v4l2_subdev *sd,
		struct msm_sync *sync, void __user *arg)
{
	struct msm_vfe_cfg_cmd cfgcmd;
	struct msm_pmem_region region[8];
	struct axidata axi_data;

	if (copy_from_user(&cfgcmd, arg, sizeof(cfgcmd))) {
		ERR_COPY_FROM_USER();
		return -EFAULT;
	}

	memset(&axi_data, 0, sizeof(axi_data));
	CDBG("%s: cmd_type %d\n", __func__, cfgcmd.cmd_type);
	switch (cfgcmd.cmd_type) {
	case CMD_STATS_AF_ENABLE:
		axi_data.bufnum1 =
			msm_pmem_region_lookup(&sync->pmem_stats,
					MSM_PMEM_AF, &region[0],
					NUM_STAT_OUTPUT_BUFFERS);
		if (!axi_data.bufnum1) {
			pr_err("%s %d: pmem region lookup error\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		axi_data.region = &region[0];
		return msm_isp_subdev_ioctl(sd, &cfgcmd,
							&axi_data);
	case CMD_STATS_AEC_ENABLE:
		axi_data.bufnum1 =
			msm_pmem_region_lookup(&sync->pmem_stats,
			MSM_PMEM_AEC, &region[0],
			NUM_STAT_OUTPUT_BUFFERS);
		if (!axi_data.bufnum1) {
			pr_err("%s %d: pmem region lookup error\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		axi_data.region = &region[0];
		return msm_isp_subdev_ioctl(sd, &cfgcmd,
							&axi_data);
	case CMD_STATS_AWB_ENABLE:
		axi_data.bufnum1 =
			msm_pmem_region_lookup(&sync->pmem_stats,
			MSM_PMEM_AWB, &region[0],
			NUM_STAT_OUTPUT_BUFFERS);
		if (!axi_data.bufnum1) {
			pr_err("%s %d: pmem region lookup error\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		axi_data.region = &region[0];
		return msm_isp_subdev_ioctl(sd, &cfgcmd,
							&axi_data);
	case CMD_STATS_IHIST_ENABLE:
		axi_data.bufnum1 =
			msm_pmem_region_lookup(&sync->pmem_stats,
			MSM_PMEM_IHIST, &region[0],
			NUM_STAT_OUTPUT_BUFFERS);
		if (!axi_data.bufnum1) {
			pr_err("%s %d: pmem region lookup error\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		axi_data.region = &region[0];
		return msm_isp_subdev_ioctl(sd, &cfgcmd,
							&axi_data);
	case CMD_STATS_RS_ENABLE:
		axi_data.bufnum1 =
			msm_pmem_region_lookup(&sync->pmem_stats,
			MSM_PMEM_RS, &region[0],
			NUM_STAT_OUTPUT_BUFFERS);
		if (!axi_data.bufnum1) {
			pr_err("%s %d: pmem region lookup error\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		axi_data.region = &region[0];
		return msm_isp_subdev_ioctl(sd, &cfgcmd,
							&axi_data);
	case CMD_STATS_CS_ENABLE:
		axi_data.bufnum1 =
			msm_pmem_region_lookup(&sync->pmem_stats,
			MSM_PMEM_CS, &region[0],
			NUM_STAT_OUTPUT_BUFFERS);
		if (!axi_data.bufnum1) {
			pr_err("%s %d: pmem region lookup error\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		axi_data.region = &region[0];
		return msm_isp_subdev_ioctl(sd, &cfgcmd,
							&axi_data);
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

static int msm_vpe_frame_cfg(struct msm_sync *sync,
				void *cfgcmdin)
{
	int rc = -EIO;
	struct axidata axi_data;
	void *data = &axi_data;
	struct msm_pmem_region region[8];
	int pmem_type;

	struct msm_vpe_cfg_cmd *cfgcmd;
	cfgcmd = (struct msm_vpe_cfg_cmd *)cfgcmdin;

	memset(&axi_data, 0, sizeof(axi_data));
	CDBG("In vpe_frame_cfg cfgcmd->cmd_type = %d\n",
		cfgcmd->cmd_type);
	switch (cfgcmd->cmd_type) {
	case CMD_AXI_CFG_VPE:
		pmem_type = MSM_PMEM_VIDEO_VPE;
		axi_data.bufnum1 =
			msm_pmem_region_lookup_2(&sync->pmem_frames, pmem_type,
								&region[0], 8);
		CDBG("axi_data.bufnum1 = %d\n", axi_data.bufnum1);
		if (!axi_data.bufnum1) {
			pr_err("%s %d: pmem region lookup error\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		pmem_type = MSM_PMEM_VIDEO;
		break;
	default:
		pr_err("%s: unknown command type %d\n",
			__func__, cfgcmd->cmd_type);
		break;
	}
	axi_data.region = &region[0];
	CDBG("out vpe_frame_cfg cfgcmd->cmd_type = %d\n",
		cfgcmd->cmd_type);
	/* send the AXI configuration command to driver */
	if (sync->vpefn.vpe_config)
		rc = sync->vpefn.vpe_config(cfgcmd, data);
	return rc;
}

static int msm_stats_axi_cfg(struct v4l2_subdev *sd,
		struct msm_sync *sync, struct msm_vfe_cfg_cmd *cfgcmd)
{
	int rc = -EIO;
	struct axidata axi_data;
	void *data = &axi_data;
	struct msm_pmem_region region[3];
	int pmem_type = MSM_PMEM_MAX;

	memset(&axi_data, 0, sizeof(axi_data));

	switch (cfgcmd->cmd_type) {
	case CMD_STATS_AXI_CFG:
		pmem_type = MSM_PMEM_AEC_AWB;
		break;
	case CMD_STATS_AF_AXI_CFG:
		pmem_type = MSM_PMEM_AF;
		break;
	case CMD_GENERAL:
		data = NULL;
		break;
	default:
		pr_err("%s: unknown command type %d\n",
			__func__, cfgcmd->cmd_type);
		return -EINVAL;
	}

	if (cfgcmd->cmd_type != CMD_GENERAL) {
		axi_data.bufnum1 =
			msm_pmem_region_lookup(&sync->pmem_stats, pmem_type,
				&region[0], NUM_STAT_OUTPUT_BUFFERS);
		if (!axi_data.bufnum1) {
			pr_err("%s %d: pmem region lookup error\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		axi_data.region = &region[0];
	}

	/* send the AEC/AWB STATS configuration command to driver */
	rc = msm_isp_subdev_ioctl(sd, cfgcmd, data);
	return rc;
}

static int msm_axi_config(struct v4l2_subdev *sd,
			struct msm_sync *sync, void __user *arg)
{
	struct msm_vfe_cfg_cmd cfgcmd;

	if (copy_from_user(&cfgcmd, arg, sizeof(cfgcmd))) {
		ERR_COPY_FROM_USER();
		return -EFAULT;
	}

	switch (cfgcmd.cmd_type) {
	case CMD_AXI_CFG_VIDEO:
	case CMD_AXI_CFG_PREVIEW:
	case CMD_AXI_CFG_SNAP:
	case CMD_AXI_CFG_ZSL:
	case CMD_RAW_PICT_AXI_CFG:
		/* Dont need to pass buffer information.
		 * subdev will get the buffer from media
		 * controller free queue.
		 */
		return msm_isp_subdev_ioctl(sd, &cfgcmd, NULL);
	case CMD_AXI_CFG_VPE:
		return 0;
		return msm_vpe_frame_cfg(sync, (void *)&cfgcmd);

	case CMD_STATS_AXI_CFG:
	case CMD_STATS_AF_AXI_CFG:
		return msm_stats_axi_cfg(sd, sync, &cfgcmd);

	default:
		pr_err("%s: unknown command type %d\n",
			__func__,
			cfgcmd.cmd_type);
		return -EINVAL;
	}

	return 0;
}

static int msm_set_crop(struct msm_sync *sync, void __user *arg)
{
	struct crop_info crop;

	if (copy_from_user(&crop,
				arg,
				sizeof(struct crop_info))) {
		ERR_COPY_FROM_USER();
		return -EFAULT;
	}

	if (!sync->croplen) {
		sync->cropinfo = kmalloc(crop.len, GFP_KERNEL);
		if (!sync->cropinfo)
			return -ENOMEM;
	} else if (sync->croplen < crop.len)
		return -EINVAL;

	if (copy_from_user(sync->cropinfo,
				crop.info,
				crop.len)) {
		ERR_COPY_FROM_USER();
		kfree(sync->cropinfo);
		return -EFAULT;
	}

	sync->croplen = crop.len;

	return 0;
}

static int msm_put_stats_buffer(struct v4l2_subdev *sd,
			struct msm_sync *sync, void __user *arg)
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
	pphy = msm_pmem_stats_vtop_lookup(sync, buf.buffer, buf.fd);

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

/* config function simliar to origanl msm_ioctl_config*/
static int msm_isp_config(struct msm_cam_media_controller *pmctl,
			 unsigned int cmd, unsigned long arg)
{

	int rc = -EINVAL;
	void __user *argp = (void __user *)arg;
	struct v4l2_subdev *sd = &pmctl->isp_sdev->sd;

	D("%s: cmd %d\n", __func__, _IOC_NR(cmd));
	switch (cmd) {
	case MSM_CAM_IOCTL_PICT_PP_DONE:
		/* Release the preview of snapshot frame
		 * that was grabbed.
		 */
		/*rc = msm_pp_release(pmsm->sync, arg);*/
		break;

	case MSM_CAM_IOCTL_CONFIG_VFE:
		/* Coming from config thread for update */
		rc = msm_config_vfe(sd, &pmctl->sync, argp);
		break;

	case MSM_CAM_IOCTL_CONFIG_VPE:
		/* Coming from config thread for update */
		/*rc = msm_config_vpe(pmsm->sync, argp);*/
		rc = 0;
		break;

	case MSM_CAM_IOCTL_AXI_CONFIG:
	case MSM_CAM_IOCTL_AXI_VPE_CONFIG:
		D("Received MSM_CAM_IOCTL_AXI_CONFIG\n");
		rc = msm_axi_config(sd, &pmctl->sync, argp);
		break;

	case MSM_CAM_IOCTL_SET_CROP:
		rc = msm_set_crop(&pmctl->sync, argp);
		break;

	case MSM_CAM_IOCTL_RELEASE_STATS_BUFFER:
		rc = msm_put_stats_buffer(sd, &pmctl->sync, argp);
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
		isp_subdev[i].isp_open = msm_isp_open;
		isp_subdev[i].isp_config = msm_isp_config;
		isp_subdev[i].isp_release  = msm_isp_release;
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
