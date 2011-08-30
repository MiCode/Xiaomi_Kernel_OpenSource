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
#include "msm_ispif.h"

#ifdef CONFIG_MSM_CAMERA_DEBUG
#define D(fmt, args...) pr_debug("msm_mctl: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

#define MSM_V4L2_SWFI_LATENCY 3

/* VFE required buffer number for streaming */
static struct msm_isp_color_fmt msm_isp_formats[] = {
	{
	.name	   = "NV12YUV",
	.depth	  = 12,
	.bitsperpxl = 8,
	.fourcc	 = V4L2_PIX_FMT_NV12,
	.pxlcode	= V4L2_MBUS_FMT_YUYV8_2X8, /* YUV sensor */
	.colorspace = V4L2_COLORSPACE_JPEG,
	},
	{
	.name	   = "NV21YUV",
	.depth	  = 12,
	.bitsperpxl = 8,
	.fourcc	 = V4L2_PIX_FMT_NV21,
	.pxlcode	= V4L2_MBUS_FMT_YUYV8_2X8, /* YUV sensor */
	.colorspace = V4L2_COLORSPACE_JPEG,
	},
	{
	.name	   = "NV12BAYER",
	.depth	  = 8,
	.bitsperpxl = 8,
	.fourcc	 = V4L2_PIX_FMT_NV12,
	.pxlcode	= V4L2_MBUS_FMT_SBGGR10_1X10, /* Bayer sensor */
	.colorspace = V4L2_COLORSPACE_JPEG,
	},
	{
	.name	   = "NV21BAYER",
	.depth	  = 8,
	.bitsperpxl = 8,
	.fourcc	 = V4L2_PIX_FMT_NV21,
	.pxlcode	= V4L2_MBUS_FMT_SBGGR10_1X10, /* Bayer sensor */
	.colorspace = V4L2_COLORSPACE_JPEG,
	},
	{
	.name	   = "RAWBAYER",
	.depth	  = 10,
	.bitsperpxl = 10,
	.fourcc	 = V4L2_PIX_FMT_SBGGR10,
	.pxlcode	= V4L2_MBUS_FMT_SBGGR10_1X10, /* Bayer sensor */
	.colorspace = V4L2_COLORSPACE_JPEG,
	},

};

/*
 *  V4l2 subdevice operations
 */
static	int mctl_subdev_log_status(struct v4l2_subdev *sd)
{
	return -EINVAL;
}

static long mctl_subdev_ioctl(struct v4l2_subdev *sd,
				 unsigned int cmd, void *arg)
{
	struct msm_cam_media_controller *pmctl = NULL;
	if (!sd) {
		pr_err("%s: param is NULL", __func__);
		return -EINVAL;
	} else
		pmctl = (struct msm_cam_media_controller *)
		v4l2_get_subdevdata(sd);


	return -EINVAL;
}


static int mctl_subdev_g_mbus_fmt(struct v4l2_subdev *sd,
					 struct v4l2_mbus_framefmt *mf)
{
	return -EINVAL;
}

static struct v4l2_subdev_core_ops mctl_subdev_core_ops = {
	.log_status = mctl_subdev_log_status,
	.ioctl = mctl_subdev_ioctl,
};

static struct v4l2_subdev_video_ops mctl_subdev_video_ops = {
	.g_mbus_fmt = mctl_subdev_g_mbus_fmt,
};

static struct v4l2_subdev_ops mctl_subdev_ops = {
	.core = &mctl_subdev_core_ops,
	.video  = &mctl_subdev_video_ops,
};

static int msm_get_sensor_info(struct msm_sync *sync,
				void __user *arg)
{
	int rc = 0;
	struct msm_camsensor_info info;
	struct msm_camera_sensor_info *sdata;

	if (copy_from_user(&info,
			arg,
			sizeof(struct msm_camsensor_info))) {
		ERR_COPY_FROM_USER();
		return -EFAULT;
	}

	sdata = sync->pdev->dev.platform_data;
	D("%s: sensor_name %s\n", __func__, sdata->sensor_name);

	memcpy(&info.name[0], sdata->sensor_name, MAX_SENSOR_NAME);
	info.flash_enabled = sdata->flash_data->flash_type !=
					MSM_CAMERA_FLASH_NONE;

	/* copy back to user space */
	if (copy_to_user((void *)arg,
				&info,
				sizeof(struct msm_camsensor_info))) {
		ERR_COPY_TO_USER();
		rc = -EFAULT;
	}

	return rc;
}

/* called by other subdev to notify any changes*/

static int msm_mctl_notify(struct msm_cam_media_controller *p_mctl,
			unsigned int notification, void *arg)
{
	int rc = -EINVAL;
	struct msm_camera_sensor_info *sinfo =
			p_mctl->plat_dev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	uint8_t csid_core = camdev->csid_core;
	switch (notification) {
	case NOTIFY_CID_CHANGE:
		/* reconfig the ISPIF*/
		if (p_mctl->ispif_sdev) {
			struct msm_ispif_params_list ispif_params;
			ispif_params.len = 1;
			ispif_params.params[0].intftype = PIX0;
			ispif_params.params[0].cid_mask = 0x0001;
			ispif_params.params[0].csid = csid_core;

			rc = v4l2_subdev_call(p_mctl->ispif_sdev, core, ioctl,
				VIDIOC_MSM_ISPSF_CFG, &ispif_params);
			if (rc < 0)
				return rc;

			rc = v4l2_subdev_call(p_mctl->ispif_sdev, video,
				s_stream, ISPIF_STREAM(PIX0,
					ISPIF_ON_FRAME_BOUNDARY));
			if (rc < 0)
				return rc;
		}
		break;
	case NOTIFY_ISPIF_STREAM:
		/* call ISPIF stream on/off */
		rc = 0;
		break;
	case NOTIFY_ISP_MSG_EVT:
	case NOTIFY_VFE_MSG_OUT:
	case NOTIFY_VFE_MSG_STATS:
	case NOTIFY_VFE_BUF_EVT:
		if (p_mctl->isp_sdev && p_mctl->isp_sdev->isp_notify) {
			rc = p_mctl->isp_sdev->isp_notify(
				&p_mctl->isp_sdev->sd, notification, arg);
		}
		break;
	default:
		break;
	}

	return rc;
}

static int msm_mctl_set_pp_key(struct msm_cam_media_controller *p_mctl,
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

static int msm_mctl_pp_done(struct msm_cam_media_controller *p_mctl,
				void __user *arg)
{
	struct msm_frame frame;
	int msg_type, image_mode, rc = 0;
	int dirty = 0;
	struct msm_free_buf buf;
	unsigned long flags;

	if (copy_from_user(&frame, arg, sizeof(frame)))
		return -EFAULT;
	spin_lock_irqsave(&p_mctl->pp_info.lock, flags);
	switch (frame.path) {
	case OUTPUT_TYPE_P:
		if (!(p_mctl->pp_info.pp_key & PP_PREV)) {
			rc = -EFAULT;
			goto err;
		}
		msg_type = VFE_MSG_OUTPUT_P;
		image_mode = MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW;
		break;
	case OUTPUT_TYPE_S:
		if (!(p_mctl->pp_info.pp_key & PP_SNAP))
			return -EFAULT;
		msg_type = VFE_MSG_OUTPUT_S;
		image_mode = MSM_V4L2_EXT_CAPTURE_MODE_MAIN;
		break;
	case OUTPUT_TYPE_T:
	case OUTPUT_TYPE_V:
	default:
		rc = -EFAULT;
		goto err;
	}
	memcpy(&buf, &p_mctl->pp_info.div_frame[image_mode], sizeof(buf));
	memset(&p_mctl->pp_info.div_frame[image_mode], 0, sizeof(buf));
	if (p_mctl->pp_info.cur_frame_id[image_mode] !=
					frame.frame_id) {
		/* dirty frame. should not pass to app */
		dirty = 1;
	}
	spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);
	/* here buf.addr is phy_addr */
	rc = msm_mctl_buf_done_pp(p_mctl, msg_type, &buf, dirty);
	return rc;
err:
	spin_unlock_irqrestore(&p_mctl->pp_info.lock, flags);
	return rc;
}

/* called by the server or the config nodes to handle user space
	commands*/
static int msm_mctl_cmd(struct msm_cam_media_controller *p_mctl,
			unsigned int cmd, unsigned long arg)
{
	int rc = -EINVAL;
	void __user *argp = (void __user *)arg;
	if (!p_mctl) {
		pr_err("%s: param is NULL", __func__);
		return -EINVAL;
	}
	D("%s start cmd = %d\n", __func__, _IOC_NR(cmd));

	/* ... call sensor, ISPIF or VEF subdev*/
	switch (cmd) {
		/* sensor config*/
	case MSM_CAM_IOCTL_GET_SENSOR_INFO:
			rc = msm_get_sensor_info(&p_mctl->sync, argp);
			break;

	case MSM_CAM_IOCTL_SENSOR_IO_CFG:
			rc = p_mctl->sync.sctrl.s_config(argp);
			break;

	case MSM_CAM_IOCTL_FLASH_CTRL: {
		struct flash_ctrl_data flash_info;
		if (copy_from_user(&flash_info, argp, sizeof(flash_info))) {
			ERR_COPY_FROM_USER();
			rc = -EFAULT;
		} else {
			rc = msm_flash_ctrl(p_mctl->sync.sdata, &flash_info);
		}
		break;
	}
	case MSM_CAM_IOCTL_PICT_PP:
		rc = msm_mctl_set_pp_key(p_mctl, (void __user *)arg);
		break;
	case MSM_CAM_IOCTL_PICT_PP_DONE:
		rc = msm_mctl_pp_done(p_mctl, (void __user *)arg);
		break;
			/* ISFIF config*/
	default:
		/* ISP config*/
		rc = p_mctl->isp_sdev->isp_config(p_mctl, cmd, arg);
		break;
	}
	D("%s: !!! cmd = %d, rc = %d\n", __func__, _IOC_NR(cmd), rc);
	return rc;
}

static int msm_mctl_open(struct msm_cam_media_controller *p_mctl,
				 const char *const apps_id)
{
	int rc = 0;
	struct msm_sync *sync = NULL;
	D("%s\n", __func__);
	if (!p_mctl) {
		pr_err("%s: param is NULL", __func__);
		return -EINVAL;
	}

	/* msm_sync_init() muct be called before*/
	sync = &(p_mctl->sync);

	mutex_lock(&sync->lock);
	/* open sub devices - once only*/
	if (!sync->opencnt) {
		wake_lock(&sync->wake_lock);

		/* turn on clock */
		rc = msm_camio_sensor_clk_on(sync->pdev);
		if (rc < 0) {
			pr_err("%s: msm_camio_sensor_clk_on failed:%d\n",
			 __func__, rc);
			goto msm_open_done;
		}

		/* ISP first*/
		if (p_mctl->isp_sdev && p_mctl->isp_sdev->isp_open)
			rc = p_mctl->isp_sdev->isp_open(
				&p_mctl->isp_sdev->sd, sync);

		if (rc < 0) {
			pr_err("%s: isp init failed: %d\n", __func__, rc);
			goto msm_open_done;
		}

		/*This has to be after isp_open, because isp_open initialize
		 *platform resource. This dependency needs to be removed. */
		rc = msm_ispif_init(&p_mctl->ispif_sdev, sync->pdev);
		if (rc < 0) {
			pr_err("%s: ispif initialization failed %d\n",
				__func__, rc);
			goto msm_open_done;
		}

		/* then sensor - move sub dev later*/
		if (sync->sctrl.s_init)
			rc = sync->sctrl.s_init(sync->sdata);

		if (rc < 0) {
			pr_err("%s: isp init failed: %d\n", __func__, rc);
			goto msm_open_done;
		}

		pm_qos_add_request(&p_mctl->pm_qos_req_list,
					PM_QOS_CPU_DMA_LATENCY,
					PM_QOS_DEFAULT_VALUE);
		pm_qos_update_request(&p_mctl->pm_qos_req_list,
					MSM_V4L2_SWFI_LATENCY);

		sync->apps_id = apps_id;
		sync->opencnt++;
	}

msm_open_done:
	mutex_unlock(&sync->lock);
	return rc;
}

static int msm_mctl_release(struct msm_cam_media_controller *p_mctl)
{
	struct msm_sync *sync = NULL;
	int rc = 0;

	sync = &(p_mctl->sync);

	msm_ispif_release(p_mctl->ispif_sdev);

	if (p_mctl->isp_sdev && p_mctl->isp_sdev->isp_release)
		p_mctl->isp_sdev->isp_release(&p_mctl->sync);

	if (p_mctl->sync.sctrl.s_release)
		p_mctl->sync.sctrl.s_release();

	rc = msm_camio_sensor_clk_off(sync->pdev);
	if (rc < 0)
		pr_err("%s: msm_camio_sensor_clk_off failed:%d\n",
			 __func__, rc);

	pm_qos_update_request(&p_mctl->pm_qos_req_list,
				PM_QOS_DEFAULT_VALUE);
	pm_qos_remove_request(&p_mctl->pm_qos_req_list);

	return rc;
}

int msm_mctl_init_user_formats(struct msm_cam_v4l2_device *pcam)
{
	struct v4l2_subdev *sd = pcam->mctl.sensor_sdev;
	enum v4l2_mbus_pixelcode pxlcode;
	int numfmt_sensor = 0;
	int numfmt = 0;
	int rc = 0;
	int i, j;

	D("%s\n", __func__);
	while (!v4l2_subdev_call(sd, video, enum_mbus_fmt, numfmt_sensor,
								&pxlcode))
		numfmt_sensor++;

	D("%s, numfmt_sensor = %d\n", __func__, numfmt_sensor);
	if (!numfmt_sensor)
		return -ENXIO;

	pcam->usr_fmts = vmalloc(numfmt_sensor * ARRAY_SIZE(msm_isp_formats) *
				sizeof(struct msm_isp_color_fmt));
	if (!pcam->usr_fmts)
		return -ENOMEM;

	/* from sensor to ISP.. fill the data structure */
	for (i = 0; i < numfmt_sensor; i++) {
		rc = v4l2_subdev_call(sd, video, enum_mbus_fmt, i, &pxlcode);
		D("rc is  %d\n", rc);
		if (rc < 0) {
			vfree(pcam->usr_fmts);
			return rc;
		}

		for (j = 0; j < ARRAY_SIZE(msm_isp_formats); j++) {
			/* find the corresponding format */
			if (pxlcode == msm_isp_formats[j].pxlcode) {
				pcam->usr_fmts[numfmt] = msm_isp_formats[j];
				D("pcam->usr_fmts=0x%x\n", (u32)pcam->usr_fmts);
				D("format pxlcode 0x%x (0x%x) found\n",
					  pcam->usr_fmts[numfmt].pxlcode,
					  pcam->usr_fmts[numfmt].fourcc);
				numfmt++;
			}
		}
	}

	pcam->num_fmts = numfmt;

	if (numfmt == 0) {
		pr_err("%s: No supported formats.\n", __func__);
		vfree(pcam->usr_fmts);
		return -EINVAL;
	}

	D("Found %d supported formats.\n", pcam->num_fmts);
	/* set the default pxlcode, in any case, it will be set through
	 * setfmt */
	return 0;
}

/* this function plug in the implementation of a v4l2_subdev */
int msm_mctl_init_module(struct msm_cam_v4l2_device *pcam)
{
	struct msm_cam_media_controller *pmctl = NULL;
	D("%s\n", __func__);
	if (!pcam) {
		pr_err("%s: param is NULL", __func__);
		return -EINVAL;
	} else
		pmctl = &pcam->mctl;

	pmctl->sync.opencnt = 0;

	/* init module operations*/
	pmctl->mctl_open = msm_mctl_open;
	pmctl->mctl_cmd = msm_mctl_cmd;
	pmctl->mctl_notify = msm_mctl_notify;
	pmctl->mctl_release = msm_mctl_release;
	pmctl->plat_dev = pcam->pdev;
	/* init mctl buf */
	msm_mctl_buf_init(pcam);
	memset(&pmctl->pp_info, 0, sizeof(pmctl->pp_info));
	spin_lock_init(&pmctl->pp_info.lock);
	/* init sub device*/
	v4l2_subdev_init(&(pmctl->mctl_sdev), &mctl_subdev_ops);
	v4l2_set_subdevdata(&(pmctl->mctl_sdev), pmctl);

	return 0;
}
