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
#include "msm_csid.h"
#include "msm_csiphy.h"
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
	.name	   = "YU12BAYER",
	.depth	  = 8,
	.bitsperpxl = 8,
	.fourcc	 = V4L2_PIX_FMT_YUV420M,
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
				VIDIOC_MSM_ISPIF_CFG, &ispif_params);
			if (rc < 0)
				return rc;
		}
		break;
	case NOTIFY_ISPIF_STREAM:
		/* call ISPIF stream on/off */
		rc = v4l2_subdev_call(p_mctl->ispif_sdev, video,
				s_stream, (int)arg);
		if (rc < 0)
			return rc;

		break;
	case NOTIFY_ISP_MSG_EVT:
	case NOTIFY_VFE_MSG_OUT:
	case NOTIFY_VFE_MSG_STATS:
	case NOTIFY_VFE_BUF_EVT:
		if (p_mctl->isp_sdev && p_mctl->isp_sdev->isp_notify) {
			rc = p_mctl->isp_sdev->isp_notify(
				p_mctl->isp_sdev->sd, notification, arg);
		}
		break;
	case NOTIFY_VPE_MSG_EVT:
		if (p_mctl->isp_sdev && p_mctl->isp_sdev->isp_notify) {
			rc = p_mctl->isp_sdev->isp_notify(
				p_mctl->isp_sdev->sd_vpe, notification, arg);
		}
		break;
	case NOTIFY_PCLK_CHANGE:
		rc = v4l2_subdev_call(p_mctl->isp_sdev->sd, video,
			s_crystal_freq, *(uint32_t *)arg, 0);
		break;
	case NOTIFY_CSIPHY_CFG:
		rc = v4l2_subdev_call(p_mctl->csiphy_sdev,
			core, ioctl, VIDIOC_MSM_CSIPHY_CFG, arg);
		break;
	case NOTIFY_CSID_CFG:
		rc = v4l2_subdev_call(p_mctl->csid_sdev,
			core, ioctl, VIDIOC_MSM_CSID_CFG, arg);
		break;
	default:
		break;
	}

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
	D("%s cmd = %d\n", __func__, _IOC_NR(cmd));

	/* ... call sensor, ISPIF or VEF subdev*/
	switch (cmd) {
		/* sensor config*/
	case MSM_CAM_IOCTL_GET_SENSOR_INFO:
			rc = msm_get_sensor_info(&p_mctl->sync, argp);
			break;

	case MSM_CAM_IOCTL_SENSOR_IO_CFG:
			rc = p_mctl->sync.sctrl.s_config(argp);
			break;

	case MSM_CAM_IOCTL_SENSOR_V4l2_S_CTRL: {
			struct v4l2_control v4l2_ctrl;
			CDBG("subdev call\n");
			if (copy_from_user(&v4l2_ctrl,
				(void *)argp,
				sizeof(struct v4l2_control))) {
				CDBG("copy fail\n");
				return -EFAULT;
			}
			CDBG("subdev call ok\n");
			rc = v4l2_subdev_call(p_mctl->sensor_sdev,
				core, s_ctrl, &v4l2_ctrl);
			break;
	}

	case MSM_CAM_IOCTL_SENSOR_V4l2_QUERY_CTRL: {
			struct v4l2_queryctrl v4l2_qctrl;
			CDBG("query called\n");
			if (copy_from_user(&v4l2_qctrl,
				(void *)argp,
				sizeof(struct v4l2_queryctrl))) {
				CDBG("copy fail\n");
				rc = -EFAULT;
				break;
			}
			rc = v4l2_subdev_call(p_mctl->sensor_sdev,
				core, queryctrl, &v4l2_qctrl);
			if (rc < 0) {
				rc = -EFAULT;
				break;
			}
			if (copy_to_user((void *)argp,
					 &v4l2_qctrl,
					 sizeof(struct v4l2_queryctrl))) {
				rc = -EFAULT;
			}
			break;
	}

	case MSM_CAM_IOCTL_ACTUATOR_IO_CFG: {
		struct msm_actuator_cfg_data act_data;
		if (p_mctl->sync.actctrl.a_config) {
			rc = p_mctl->sync.actctrl.a_config(argp);
		} else {
			rc = copy_from_user(
				&act_data,
				(void *)argp,
				sizeof(struct msm_actuator_cfg_data));
			if (rc != 0) {
				rc = -EFAULT;
				break;
			}
			act_data.is_af_supported = 0;
			rc = copy_to_user((void *)argp,
					 &act_data,
					 sizeof(struct msm_actuator_cfg_data));
			if (rc != 0) {
				rc = -EFAULT;
				break;
			}
		}
		break;
	}

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
	case MSM_CAM_IOCTL_PICT_PP_DIVERT_DONE:
		rc = msm_mctl_pp_divert_done(p_mctl, (void __user *)arg);
		break;
	case MSM_CAM_IOCTL_PICT_PP_DONE:
		rc = msm_mctl_pp_done(p_mctl, (void __user *)arg);
		break;
	case MSM_CAM_IOCTL_MCTL_POST_PROC:
		rc = msm_mctl_pp_ioctl(p_mctl, cmd, arg);
		break;
	case MSM_CAM_IOCTL_RESERVE_FREE_FRAME:
		rc = msm_mctl_pp_reserve_free_frame(p_mctl,
			(void __user *)arg);
		break;
	case MSM_CAM_IOCTL_RELEASE_FREE_FRAME:
		rc = msm_mctl_pp_release_free_frame(p_mctl,
			(void __user *)arg);
		break;
			/* ISFIF config*/
	default:
		/* ISP config*/
		rc = p_mctl->isp_sdev->isp_config(p_mctl, cmd, arg);
		break;
	}
	D("%s: !!! cmd = %d, rc = %d\n",
		__func__, _IOC_NR(cmd), rc);
	return rc;
}

static int msm_mctl_subdev_match_core(struct device *dev, void *data)
{
	int core_index = (int)data;
	struct platform_device *pdev = to_platform_device(dev);

	if (pdev->id == core_index)
		return 1;
	else
		return 0;
}

static int msm_mctl_register_subdevs(struct msm_cam_media_controller *p_mctl,
	int core_index)
{
	struct device_driver *driver;
	struct device *dev;
	int rc = -ENODEV;

	/* register csiphy subdev */
	driver = driver_find(MSM_CSIPHY_DRV_NAME, &platform_bus_type);
	if (!driver)
		goto out;

	dev = driver_find_device(driver, NULL, (void *)core_index,
				msm_mctl_subdev_match_core);
	if (!dev)
		goto out_put_driver;

	p_mctl->csiphy_sdev = dev_get_drvdata(dev);
	put_driver(driver);

	/* register csid subdev */
	driver = driver_find(MSM_CSID_DRV_NAME, &platform_bus_type);
	if (!driver)
		goto out;

	dev = driver_find_device(driver, NULL, (void *)core_index,
				msm_mctl_subdev_match_core);
	if (!dev)
		goto out_put_driver;

	p_mctl->csid_sdev = dev_get_drvdata(dev);
	put_driver(driver);

	/* register ispif subdev */
	driver = driver_find(MSM_ISPIF_DRV_NAME, &platform_bus_type);
	if (!driver)
		goto out;

	dev = driver_find_device(driver, NULL, 0,
				msm_mctl_subdev_match_core);
	if (!dev)
		goto out_put_driver;

	p_mctl->ispif_sdev = dev_get_drvdata(dev);
	put_driver(driver);

	/* register vfe subdev */
	driver = driver_find(MSM_VFE_DRV_NAME, &platform_bus_type);
	if (!driver)
		goto out;

	dev = driver_find_device(driver, NULL, 0,
				msm_mctl_subdev_match_core);
	if (!dev)
		goto out_put_driver;

	p_mctl->isp_sdev->sd = dev_get_drvdata(dev);
	put_driver(driver);

	/* register vfe subdev */
	driver = driver_find(MSM_VPE_DRV_NAME, &platform_bus_type);
	if (!driver)
		goto out;

	dev = driver_find_device(driver, NULL, 0,
				msm_mctl_subdev_match_core);
	if (!dev)
		goto out_put_driver;

	p_mctl->isp_sdev->sd_vpe = dev_get_drvdata(dev);
	put_driver(driver);

	rc = 0;
	return rc;
out_put_driver:
	put_driver(driver);
out:
	return rc;
}

static int msm_mctl_open(struct msm_cam_media_controller *p_mctl,
				 const char *const apps_id)
{
	int rc = 0;
	struct msm_sync *sync = NULL;
	struct msm_camera_sensor_info *sinfo;
	struct msm_camera_device_platform_data *camdev;
	uint8_t csid_core;
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
		uint32_t csid_version;
		wake_lock(&sync->wake_lock);

		sinfo = sync->pdev->dev.platform_data;
		sync->pdev->resource = sinfo->resource;
		sync->pdev->num_resources = sinfo->num_resources;
		camdev = sinfo->pdata;
		csid_core = camdev->csid_core;
		rc = msm_mctl_register_subdevs(p_mctl, csid_core);
		if (rc < 0) {
			pr_err("%s: msm_mctl_register_subdevs failed:%d\n",
				__func__, rc);
			goto msm_open_done;
		}

		/* turn on clock */
		rc = msm_camio_sensor_clk_on(sync->pdev);
		if (rc < 0) {
			pr_err("%s: msm_camio_sensor_clk_on failed:%d\n",
			 __func__, rc);
			goto msm_open_done;
		}

		rc = v4l2_subdev_call(p_mctl->csiphy_sdev, core, ioctl,
			VIDIOC_MSM_CSIPHY_INIT, NULL);
		if (rc < 0) {
			pr_err("%s: csiphy initialization failed %d\n",
				__func__, rc);
			goto msm_open_done;
		}

		rc = v4l2_subdev_call(p_mctl->csid_sdev, core, ioctl,
			VIDIOC_MSM_CSID_INIT, &csid_version);
		if (rc < 0) {
			pr_err("%s: csid initialization failed %d\n",
				__func__, rc);
			goto msm_open_done;
		}

		/* ISP first*/
		if (p_mctl->isp_sdev && p_mctl->isp_sdev->isp_open)
			rc = p_mctl->isp_sdev->isp_open(
				p_mctl->isp_sdev->sd,
				p_mctl->isp_sdev->sd_vpe, sync);
		if (rc < 0) {
			pr_err("%s: isp init failed: %d\n", __func__, rc);
			goto msm_open_done;
		}

		rc = v4l2_subdev_call(p_mctl->ispif_sdev, core, ioctl,
			VIDIOC_MSM_ISPIF_INIT, &csid_version);
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

		if (sync->actctrl.a_power_up)
			rc = sync->actctrl.a_power_up(
				sync->sdata->actuator_info);

		if (rc < 0) {
			pr_err("%s: act power failed:%d\n", __func__, rc);
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
	int rc = 0;
	struct msm_sync *sync = &(p_mctl->sync);

	v4l2_subdev_call(p_mctl->ispif_sdev, core, ioctl,
		VIDIOC_MSM_ISPIF_RELEASE, NULL);

	if (p_mctl->isp_sdev && p_mctl->isp_sdev->isp_release)
		p_mctl->isp_sdev->isp_release(&p_mctl->sync);

	v4l2_subdev_call(p_mctl->csid_sdev, core, ioctl,
		VIDIOC_MSM_CSID_RELEASE, NULL);

	v4l2_subdev_call(p_mctl->csiphy_sdev, core, ioctl,
		VIDIOC_MSM_CSIPHY_RELEASE, NULL);

	if (p_mctl->sync.actctrl.a_power_down)
		p_mctl->sync.actctrl.a_power_down(sync->sdata->actuator_info);

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
