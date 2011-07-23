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

#define MSM_FRAME_AXI_MAX_BUF 16
/* This will enqueue ISP events or signal buffer completion */
static int msm_isp_enqueue(struct msm_cam_media_controller *pmctl,
				struct msm_vfe_resp *data,
				enum msm_queue qtype)
{
	struct v4l2_event v4l2_evt;

	struct msm_stats_buf stats;
	struct msm_isp_stats_event_ctrl *isp_event;
	isp_event = (struct msm_isp_stats_event_ctrl *)v4l2_evt.u.data;
	if (!data) {
		pr_err("%s !!!!data = 0x%p\n", __func__, data);
		return -EINVAL;
	}

	D("%s data->type = %d\n", __func__, data->type);

	switch (qtype) {
	case MSM_CAM_Q_VFE_EVT:
	case MSM_CAM_Q_VFE_MSG:
		/* adsp event and message */
		v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
					MSM_CAM_RESP_STAT_EVT_MSG;

		isp_event->resptype = MSM_CAM_RESP_STAT_EVT_MSG;

		/* 0 - msg from aDSP, 1 - event from mARM */
		isp_event->isp_data.isp_msg.type   = data->evt_msg.type;
		isp_event->isp_data.isp_msg.msg_id = data->evt_msg.msg_id;
		isp_event->isp_data.isp_msg.len	= 0;

		D("%s: qtype %d length %d msd_id %d\n", __func__,
					qtype,
					isp_event->isp_data.isp_msg.len,
					isp_event->isp_data.isp_msg.msg_id);

		if ((data->type >= VFE_MSG_STATS_AEC) &&
			(data->type <=  VFE_MSG_STATS_WE)) {

			D("%s data->phy.sbuf_phy = 0x%x\n", __func__,
						data->phy.sbuf_phy);
			stats.buffer = msm_pmem_stats_ptov_lookup(&pmctl->sync,
							data->phy.sbuf_phy,
							&(stats.fd));
			if (!stats.buffer) {
				pr_err("%s: msm_pmem_stats_ptov_lookup error\n",
								__func__);
				isp_event->isp_data.isp_msg.len = 0;
			} else {
				struct msm_stats_buf *stats_buf =
					kmalloc(sizeof(struct msm_stats_buf),
								GFP_ATOMIC);
				if (!stats_buf) {
					pr_err("%s: out of memory.\n",
								__func__);
					return -ENOMEM;
				}

				*stats_buf = stats;
				isp_event->isp_data.isp_msg.len	=
						sizeof(struct msm_stats_buf);
				isp_event->isp_data.isp_msg.data = stats_buf;
			}

		} else if ((data->evt_msg.len > 0) &&
				(data->type == VFE_MSG_GENERAL)) {
			isp_event->isp_data.isp_msg.data =
					kmalloc(data->evt_msg.len, GFP_ATOMIC);
			if (!isp_event->isp_data.isp_msg.data) {
				pr_err("%s: out of memory.\n", __func__);
				return -ENOMEM;
			}
			memcpy(isp_event->isp_data.isp_msg.data,
						data->evt_msg.data,
						data->evt_msg.len);
		} else if (data->type == VFE_MSG_OUTPUT_P ||
			data->type == VFE_MSG_OUTPUT_V ||
			data->type == VFE_MSG_OUTPUT_S ||
			data->type == VFE_MSG_OUTPUT_T) {
			msm_mctl_buf_done(pmctl, data->type,
					(u32)data->phy.y_phy);
		}
		break;
	default:
		break;
	}

	/* now queue the event */
	v4l2_event_queue(pmctl->config_device->config_stat_event_queue.pvdev,
					  &v4l2_evt);
	return 0;
}

/*
 * This function executes in interrupt context.
 */

void *msm_isp_sync_alloc(int size,
	  void *syncdata __attribute__((unused)),
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

/*
 * This function executes in interrupt context.
 */
static int msm_isp_notify(struct v4l2_subdev *sd, void *arg)
{
	int rc = -EINVAL;
	struct msm_queue_cmd *qcmd = NULL;
	struct msm_sync *sync =
		(struct msm_sync *)v4l2_get_subdev_hostdata(sd);
	struct msm_vfe_resp *vdata = (struct msm_vfe_resp *)arg;

	if (!sync) {
		pr_err("%s: no context in dsp callback.\n", __func__);
		return rc;
	}

	qcmd = ((struct msm_queue_cmd *)vdata) - 1;
	qcmd->type = MSM_CAM_Q_VFE_MSG;
	qcmd->command = vdata;

	D("%s: vdata->type %d\n", __func__, vdata->type);
	switch (vdata->type) {
	case VFE_MSG_STATS_AWB:
		D("%s: qtype %d, AWB stats, enqueue event_q.\n",
					__func__, vdata->type);
		break;

	case VFE_MSG_STATS_AEC:
		D("%s: qtype %d, AEC stats, enqueue event_q.\n",
					__func__, vdata->type);
		break;

	case VFE_MSG_STATS_IHIST:
		D("%s: qtype %d, ihist stats, enqueue event_q.\n",
					__func__, vdata->type);
		break;

	case VFE_MSG_STATS_RS:
		D("%s: qtype %d, rs stats, enqueue event_q.\n",
					__func__, vdata->type);
		break;

	case VFE_MSG_STATS_CS:
		D("%s: qtype %d, cs stats, enqueue event_q.\n",
					__func__, vdata->type);
	break;

	case VFE_MSG_GENERAL:
		D("%s: qtype %d, general msg, enqueue event_q.\n",
					__func__, vdata->type);
		break;
	default:
		D("%s: qtype %d not handled\n", __func__, vdata->type);
		/* fall through, send to config. */
	}

	D("%s: msm_enqueue event_q\n", __func__);
	rc = msm_isp_enqueue(&sync->pcam_sync->mctl, vdata, MSM_CAM_Q_VFE_MSG);

	msm_isp_sync_free(vdata);

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

static int msm_frame_axi_cfg(struct v4l2_subdev *sd,
	struct msm_sync *sync, struct msm_vfe_cfg_cmd *cfgcmd)
{
	int rc = -EIO;
	struct axidata axi_data;
	void *data = &axi_data;
	struct msm_pmem_region region[MSM_FRAME_AXI_MAX_BUF];
	int pmem_type;
	int i = 0;
	int idx = 0;
	struct msm_cam_v4l2_device *pcam = sync->pcam_sync;
	struct msm_cam_v4l2_dev_inst *pcam_inst;

	memset(&axi_data, 0, sizeof(axi_data));

	switch (cfgcmd->cmd_type) {

	case CMD_AXI_CFG_PREVIEW:
		pcam_inst =
		pcam->dev_inst_map[MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW];
		if (pcam_inst)
			idx = pcam_inst->my_index;
		else
			return rc;
		pmem_type = MSM_PMEM_PREVIEW;
		axi_data.bufnum2 =
			msm_pmem_region_lookup_3(sync->pcam_sync, idx,
				&region[0], pmem_type);
		if (!axi_data.bufnum2) {
			pr_err("%s %d: pmem region 3 lookup error\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		D("%s __func__ axi_data.bufnum2 = %d\n", __func__,
						axi_data.bufnum2);
		break;

	case CMD_AXI_CFG_VIDEO:
		pcam_inst =
		pcam->dev_inst_map[MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW];
		if (pcam_inst)
			idx = pcam_inst->my_index;
		else
			return rc;
		pmem_type = MSM_PMEM_PREVIEW;
		axi_data.bufnum1 =
			msm_pmem_region_lookup_3(sync->pcam_sync, idx,
				&region[0], pmem_type);
		D("%s bufnum1 = %d\n", __func__, axi_data.bufnum1);
		if (!axi_data.bufnum1) {
			pr_err("%s %d: pmem region lookup error\n",
				__func__, __LINE__);
			return -EINVAL;
		}

		pcam_inst
		= pcam->dev_inst_map[MSM_V4L2_EXT_CAPTURE_MODE_VIDEO];
		if (pcam_inst)
			idx = pcam_inst->my_index;
			pmem_type = MSM_PMEM_VIDEO;
			axi_data.bufnum2 =
			msm_pmem_region_lookup_3(sync->pcam_sync, idx,
				&region[axi_data.bufnum1], pmem_type);
		D("%s bufnum2 = %d\n", __func__, axi_data.bufnum2);
		if (!axi_data.bufnum2) {
			pr_err("%s %d: pmem region lookup error\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		break;


	case CMD_AXI_CFG_SNAP:
		pcam_inst
		= pcam->dev_inst_map[MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL];
		if (pcam_inst)
			idx = pcam_inst->my_index;
		else
			return rc;
		pmem_type = MSM_PMEM_THUMBNAIL;
		axi_data.bufnum1 =
			msm_pmem_region_lookup_3(sync->pcam_sync, idx,
				&region[0], pmem_type);
		if (!axi_data.bufnum1) {
			pr_err("%s %d: pmem region lookup error\n",
				__func__, __LINE__);
			return -EINVAL;
		}

		pcam_inst
		= pcam->dev_inst_map[MSM_V4L2_EXT_CAPTURE_MODE_MAIN];
		if (pcam_inst)
			idx = pcam_inst->my_index;
		else
			return rc;
		pmem_type = MSM_PMEM_MAINIMG;
		axi_data.bufnum2 =
		msm_pmem_region_lookup_3(sync->pcam_sync, idx,
				&region[axi_data.bufnum1], pmem_type);
		if (!axi_data.bufnum2) {
			pr_err("%s %d: pmem region lookup error\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		break;

	case CMD_RAW_PICT_AXI_CFG:
		pcam_inst
		= pcam->dev_inst_map[MSM_V4L2_EXT_CAPTURE_MODE_MAIN];
		if (pcam_inst)
			idx = pcam_inst->my_index;
		else
			return rc;
		pmem_type = MSM_PMEM_RAW_MAINIMG;
		axi_data.bufnum2 =
			msm_pmem_region_lookup_3(sync->pcam_sync, idx,
				&region[0], pmem_type);
		if (!axi_data.bufnum2) {
			pr_err("%s %d: pmem region lookup error\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		break;

	case CMD_GENERAL:
		data = NULL;
		break;

	default:
		pr_err("%s: unknown command type %d\n",
			__func__, cfgcmd->cmd_type);
		return -EINVAL;
	}

	axi_data.region = &region[0];
	D("%s bufnum1 = %d, bufnum2 = %d\n", __func__,
	  axi_data.bufnum1, axi_data.bufnum2);
	for (i = 0; i < MSM_FRAME_AXI_MAX_BUF; i++) {
		D("%s region %d paddr = 0x%p\n", __func__, i,
					(void *)region[i].paddr);
		D("%s region y_off = %d cbcr_off = %d\n", __func__,
			region[i].info.y_off, region[i].info.cbcr_off);
	}
	/* send the AXI configuration command to driver */
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
	case CMD_RAW_PICT_AXI_CFG:
		return msm_frame_axi_cfg(sd, sync, &cfgcmd);
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
		isp_subdev[i].isp_enqueue = msm_isp_enqueue;
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
