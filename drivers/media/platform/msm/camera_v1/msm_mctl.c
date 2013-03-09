/* Copyright (c) 2011-2012, The Linux Foundataion. All rights reserved.
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
#include <linux/wakelock.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>



#include "msm.h"
#include "msm_cam_server.h"
#include "msm_csid.h"
#include "msm_csic.h"
#include "msm_csiphy.h"
#include "msm_ispif.h"
#include "msm_sensor.h"
#include "msm_actuator.h"
#include "msm_vpe.h"
#include "msm_vfe32.h"
#include "msm_camera_eeprom.h"
#include "msm_csi_register.h"
#include "msm_flash.h"

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
	.name	   = "NV16BAYER",
	.depth	  = 8,
	.bitsperpxl = 8,
	.fourcc	 = V4L2_PIX_FMT_NV16,
	.pxlcode	= V4L2_MBUS_FMT_SBGGR10_1X10, /* Bayer sensor */
	.colorspace = V4L2_COLORSPACE_JPEG,
	},
	{
	.name	   = "NV61BAYER",
	.depth	  = 8,
	.bitsperpxl = 8,
	.fourcc	 = V4L2_PIX_FMT_NV61,
	.pxlcode	= V4L2_MBUS_FMT_SBGGR10_1X10, /* Bayer sensor */
	.colorspace = V4L2_COLORSPACE_JPEG,
	},
	{
	.name	   = "NV21BAYER",
	.depth	  = 8,
	.bitsperpxl = 8,
	.fourcc	 = V4L2_PIX_FMT_NV21,
	.pxlcode	= V4L2_MBUS_FMT_SGRBG10_1X10, /* Bayer sensor */
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
	{
	.name	   = "RAWBAYER",
	.depth	  = 10,
	.bitsperpxl = 10,
	.fourcc	 = V4L2_PIX_FMT_SBGGR10,
	.pxlcode	= V4L2_MBUS_FMT_SGRBG10_1X10, /* Bayer sensor */
	.colorspace = V4L2_COLORSPACE_JPEG,
	},
	{
	.name	   = "YUYV",
	.depth	  = 16,
	.bitsperpxl = 16,
	.fourcc	 = V4L2_PIX_FMT_YUYV,
	.pxlcode	= V4L2_MBUS_FMT_YUYV8_2X8, /* YUV sensor */
	.colorspace = V4L2_COLORSPACE_JPEG,
	},
	{
	.name	   = "SAEC",
	.depth	  = 16,
	.bitsperpxl = 16,
	.fourcc	 = V4L2_PIX_FMT_STATS_AE,
	.pxlcode	= V4L2_MBUS_FMT_SBGGR10_1X10, /* YUV sensor */
	.colorspace = V4L2_COLORSPACE_JPEG,
	},
	{
	.name	   = "SAWB",
	.depth	  = 16,
	.bitsperpxl = 16,
	.fourcc	 = V4L2_PIX_FMT_STATS_AWB,
	.pxlcode	= V4L2_MBUS_FMT_SBGGR10_1X10, /* YUV sensor */
	.colorspace = V4L2_COLORSPACE_JPEG,
	},
	{
	.name	   = "SAFC",
	.depth	  = 16,
	.bitsperpxl = 16,
	.fourcc	 = V4L2_PIX_FMT_STATS_AF,
	.pxlcode	= V4L2_MBUS_FMT_SBGGR10_1X10, /* YUV sensor */
	.colorspace = V4L2_COLORSPACE_JPEG,
	},
	{
	.name      = "SHST",
	.depth    = 16,
	.bitsperpxl = 16,
	.fourcc  = V4L2_PIX_FMT_STATS_IHST,
	.pxlcode        = V4L2_MBUS_FMT_SBGGR10_1X10, /* YUV sensor */
	.colorspace = V4L2_COLORSPACE_JPEG,
	},
};

static int msm_get_sensor_info(
	struct msm_cam_media_controller *mctl,
	void __user *arg)
{
	int rc = 0;
	struct msm_camsensor_info info;
	struct msm_camera_sensor_info *sdata;
	struct msm_cam_v4l2_device *pcam = mctl->pcam_ptr;
	if (copy_from_user(&info,
			arg,
			sizeof(struct msm_camsensor_info))) {
		ERR_COPY_FROM_USER();
		return -EFAULT;
	}

	sdata = mctl->sdata;
	D("%s: sensor_name %s\n", __func__, sdata->sensor_name);

	memcpy(&info.name[0], sdata->sensor_name, MAX_SENSOR_NAME);
	info.flash_enabled = sdata->flash_data->flash_type !=
					MSM_CAMERA_FLASH_NONE;
	info.pxlcode = pcam->usr_fmts[0].pxlcode;
	info.flashtype = sdata->flash_type; /* two flash_types here? */
	info.camera_type = sdata->camera_type;
	/* sensor_type needed to add YUV/SOC in probing */
	info.sensor_type = sdata->sensor_type;
	info.mount_angle = sdata->sensor_platform_info->mount_angle;
	info.actuator_enabled = sdata->actuator_info ? 1 : 0;
	info.strobe_flash_enabled = sdata->strobe_flash_data ? 1 : 0;
	info.ispif_supported = mctl->ispif_sdev ? 1 : 0;

	/* copy back to user space */
	if (copy_to_user((void *)arg,
				&info,
				sizeof(struct msm_camsensor_info))) {
		ERR_COPY_TO_USER();
		rc = -EFAULT;
	}
	return rc;
}

static int msm_mctl_set_vfe_output_mode(struct msm_cam_media_controller
					*p_mctl, void __user *arg)
{
	int rc = 0;
	if (copy_from_user(&p_mctl->vfe_output_mode,
		(void __user *)arg, sizeof(p_mctl->vfe_output_mode))) {
		pr_err("%s Copy from user failed ", __func__);
		rc = -EFAULT;
	} else {
		pr_debug("%s: mctl=0x%p, vfe output mode =0x%x\n",
		  __func__, p_mctl, p_mctl->vfe_output_mode);
	}
	return rc;
}

static uint8_t msm_sensor_state_check(
	struct msm_cam_media_controller *p_mctl)
{
	struct msm_sensor_ctrl_t *s_ctrl = NULL;
	if (!p_mctl)
		return 0;
	if (!p_mctl->sensor_sdev)
		return 0;
	s_ctrl = get_sctrl(p_mctl->sensor_sdev);
	if (s_ctrl->sensor_state == MSM_SENSOR_POWER_UP)
		return 1;
	return 0;
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
	D("%s:%d: cmd %d\n", __func__, __LINE__, cmd);

	/* ... call sensor, ISPIF or VEF subdev*/
	switch (cmd) {
		/* sensor config*/
	case MSM_CAM_IOCTL_GET_SENSOR_INFO:
			rc = msm_get_sensor_info(p_mctl, argp);
			break;

	case MSM_CAM_IOCTL_SENSOR_IO_CFG:
		rc = v4l2_subdev_call(p_mctl->sensor_sdev,
			core, ioctl, VIDIOC_MSM_SENSOR_CFG, argp);
			break;

	case MSM_CAM_IOCTL_OEM:
	 rc = v4l2_subdev_call(p_mctl->sensor_sdev,
		 core, ioctl, VIDIOC_MSM_SENSOR_CFG, argp);
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

	case MSM_CAM_IOCTL_GET_ACTUATOR_INFO: {
		struct msm_actuator_cfg_data cdata;
		if (copy_from_user(&cdata,
			(void *)argp,
			sizeof(struct msm_actuator_cfg_data))) {
			ERR_COPY_FROM_USER();
			return -EFAULT;
		}
		cdata.is_af_supported = 0;
		rc = 0;

		if (p_mctl->act_sdev) {
			struct msm_camera_sensor_info *sdata;

			sdata = p_mctl->sdata;
			CDBG("%s: Act_cam_Name %d\n", __func__,
				sdata->actuator_info->cam_name);

			cdata.is_af_supported = 1;
			cdata.cfg.cam_name =
				(enum af_camera_name)sdata->
				actuator_info->cam_name;

			CDBG("%s: Af Support:%d\n", __func__,
				cdata.is_af_supported);
			CDBG("%s: Act_name:%d\n", __func__, cdata.cfg.cam_name);

		}
		if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct msm_actuator_cfg_data))) {
			ERR_COPY_TO_USER();
			rc = -EFAULT;
		}
		break;
	}

	case MSM_CAM_IOCTL_ACTUATOR_IO_CFG: {
		struct msm_actuator_cfg_data act_data;
		if (p_mctl->act_sdev) {
			rc = v4l2_subdev_call(p_mctl->act_sdev,
				core, ioctl, VIDIOC_MSM_ACTUATOR_CFG, argp);
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

	case MSM_CAM_IOCTL_EEPROM_IO_CFG: {
		struct msm_eeprom_cfg_data eeprom_data;
		if (p_mctl->eeprom_sdev) {
			rc = v4l2_subdev_call(p_mctl->eeprom_sdev,
				core, ioctl, VIDIOC_MSM_EEPROM_CFG, argp);
		} else {
			rc = copy_from_user(
				&eeprom_data,
				(void *)argp,
				sizeof(struct msm_eeprom_cfg_data));
			if (rc != 0) {
				rc = -EFAULT;
				break;
			}
			eeprom_data.is_eeprom_supported = 0;
			rc = copy_to_user((void *)argp,
					 &eeprom_data,
					 sizeof(struct msm_eeprom_cfg_data));
			if (rc != 0) {
				rc = -EFAULT;
				break;
			}
		}
		break;
	}

	case MSM_CAM_IOCTL_GET_KERNEL_SYSTEM_TIME: {
		struct timeval timestamp;
		if (copy_from_user(&timestamp, argp, sizeof(timestamp))) {
			ERR_COPY_FROM_USER();
			rc = -EFAULT;
		} else {
			msm_mctl_gettimeofday(&timestamp);
			rc = copy_to_user((void *)argp,
				 &timestamp, sizeof(timestamp));
		}
		break;
	}

	case MSM_CAM_IOCTL_FLASH_CTRL: {
		if (p_mctl->flash_sdev && msm_sensor_state_check(p_mctl))
			rc = v4l2_subdev_call(p_mctl->flash_sdev,
				core, ioctl, VIDIOC_MSM_FLASH_CFG, argp);
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
	case MSM_CAM_IOCTL_SET_VFE_OUTPUT_TYPE:
		rc = msm_mctl_set_vfe_output_mode(p_mctl,
		  (void __user *)arg);
		break;
	case MSM_CAM_IOCTL_MCTL_DIVERT_DONE:
		rc = msm_mctl_pp_mctl_divert_done(p_mctl,
			(void __user *)arg);
		break;
			/* ISFIF config*/
	case MSM_CAM_IOCTL_AXI_CONFIG:
		if (p_mctl->axi_sdev) {
			v4l2_set_subdev_hostdata(p_mctl->axi_sdev, p_mctl);
			rc = v4l2_subdev_call(p_mctl->axi_sdev, core, ioctl,
				VIDIOC_MSM_AXI_CFG, (void __user *)arg);
		} else
			rc = p_mctl->isp_config(p_mctl, cmd, arg);
		break;
	case MSM_CAM_IOCTL_ISPIF_IO_CFG:
		rc = v4l2_subdev_call(p_mctl->ispif_sdev,
			core, ioctl, VIDIOC_MSM_ISPIF_CFG, argp);
		break;

	case MSM_CAM_IOCTL_CSIPHY_IO_CFG:
		if (p_mctl->csiphy_sdev)
			rc = v4l2_subdev_call(p_mctl->csiphy_sdev,
				core, ioctl, VIDIOC_MSM_CSIPHY_CFG, argp);
		break;

	case MSM_CAM_IOCTL_CSIC_IO_CFG:
		if (p_mctl->csic_sdev)
			rc = v4l2_subdev_call(p_mctl->csic_sdev,
				core, ioctl, VIDIOC_MSM_CSIC_CFG, argp);
		break;

	case MSM_CAM_IOCTL_CSID_IO_CFG:
		if (p_mctl->csid_sdev)
			rc = v4l2_subdev_call(p_mctl->csid_sdev,
				core, ioctl, VIDIOC_MSM_CSID_CFG, argp);
		break;

	case MSM_CAM_IOCTL_AXI_INIT:
		if (p_mctl->axi_sdev) {
			v4l2_set_subdev_hostdata(p_mctl->axi_sdev, p_mctl);
			rc = v4l2_subdev_call(p_mctl->axi_sdev, core, ioctl,
				VIDIOC_MSM_AXI_INIT, (void __user *)arg);
		}
		break;

	case MSM_CAM_IOCTL_AXI_RELEASE:
		if (p_mctl->axi_sdev) {
			v4l2_set_subdev_hostdata(p_mctl->axi_sdev, p_mctl);
			rc = v4l2_subdev_call(p_mctl->axi_sdev, core, ioctl,
				VIDIOC_MSM_AXI_RELEASE, NULL);
		}
		break;

	default:
		/* ISP config*/
		D("%s:%d: go to default. Calling msm_isp_config\n",
			__func__, __LINE__);
		rc = p_mctl->isp_config(p_mctl, cmd, arg);
		break;
	}
	D("%s: !!! cmd = %d, rc = %d\n",
		__func__, _IOC_NR(cmd), rc);
	return rc;
}

static int msm_mctl_open(struct msm_cam_media_controller *p_mctl,
				 const char *const apps_id)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(p_mctl->sensor_sdev);
	struct msm_camera_sensor_info *sinfo =
		(struct msm_camera_sensor_info *) s_ctrl->sensordata;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	struct msm_camera_sensor_flash_data *flash_data = sinfo->flash_data;
	uint8_t csid_core;
	D("%s\n", __func__);
	if (!p_mctl) {
		pr_err("%s: param is NULL", __func__);
		return -EINVAL;
	}

	mutex_lock(&p_mctl->lock);
	/* open sub devices - once only*/
	if (!p_mctl->opencnt) {
		struct msm_sensor_csi_info csi_info;
		wake_lock(&p_mctl->wake_lock);

		csid_core = camdev->csid_core;
		rc = msm_mctl_find_sensor_subdevs(p_mctl, camdev->csiphy_core,
			csid_core);
		if (rc < 0) {
			pr_err("%s: msm_mctl_find_sensor_subdevs failed:%d\n",
				__func__, rc);
			goto register_sdev_failed;
		}

		/* then sensor - move sub dev later */
		rc = v4l2_subdev_call(p_mctl->sensor_sdev, core, s_power, 1);
		if (rc < 0) {
			pr_err("%s: sensor powerup failed: %d\n", __func__, rc);
			goto sensor_sdev_failed;
		}

		if (p_mctl->act_sdev)
			rc = v4l2_subdev_call(p_mctl->act_sdev,
				core, s_power, 1);
		if (rc < 0) {
			pr_err("%s: act power failed:%d\n", __func__, rc);
			goto act_power_up_failed;
		}

		if (p_mctl->csic_sdev)
			csi_info.is_csic = 1;
		else
			csi_info.is_csic = 0;
		rc = v4l2_subdev_call(p_mctl->sensor_sdev, core, ioctl,
				VIDIOC_MSM_SENSOR_CSID_INFO, &csi_info);
		if (rc < 0) {
			pr_err("%s: sensor csi version failed %d\n",
			__func__, rc);
			goto msm_csi_version;
		}

		if (!p_mctl->flash_sdev && flash_data) {
			if ((flash_data->flash_type == MSM_CAMERA_FLASH_LED) &&
				(flash_data->flash_src_index >= 0))
				msm_mctl_find_flash_subdev(p_mctl,
					flash_data->flash_src_index);
		}

		if (p_mctl->flash_sdev && p_mctl->sdata->flash_data &&
			p_mctl->sdata->flash_data->flash_type !=
			MSM_CAMERA_FLASH_NONE) {
			rc = v4l2_subdev_call(p_mctl->flash_sdev, core, ioctl,
					VIDIOC_MSM_FLASH_LED_DATA_CFG,
					p_mctl->sdata->flash_data);
			if (rc < 0) {
				pr_err("%s: set flash led failed %d\n",
				__func__, rc);
			}
		}

		if (p_mctl->flash_sdev && p_mctl->sdata->strobe_flash_data) {
			rc = v4l2_subdev_call(p_mctl->flash_sdev, core, ioctl,
					VIDIOC_MSM_FLASH_STROBE_DATA_CFG,
					p_mctl->sdata->strobe_flash_data);
			if (rc < 0) {
				pr_err("%s: set strobe flash led failed %d\n",
				__func__, rc);
			}
		}

		pm_qos_add_request(&p_mctl->pm_qos_req_list,
			PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
		pm_qos_update_request(&p_mctl->pm_qos_req_list,
			MSM_V4L2_SWFI_LATENCY);

		p_mctl->apps_id = apps_id;
		p_mctl->opencnt++;
	} else {
		D("%s: camera is already open", __func__);
	}
	mutex_unlock(&p_mctl->lock);
	return rc;

msm_csi_version:
act_power_up_failed:
	if (v4l2_subdev_call(p_mctl->sensor_sdev, core, s_power, 0) < 0)
		pr_err("%s: sensor powerdown failed: %d\n", __func__, rc);
sensor_sdev_failed:
register_sdev_failed:
	wake_unlock(&p_mctl->wake_lock);
	mutex_unlock(&p_mctl->lock);
	return rc;
}

static void msm_mctl_release(struct msm_cam_media_controller *p_mctl)
{
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(p_mctl->sensor_sdev);
	struct msm_camera_sensor_info *sinfo =
		(struct msm_camera_sensor_info *) s_ctrl->sensordata;
	v4l2_subdev_call(p_mctl->sensor_sdev, core, ioctl,
		VIDIOC_MSM_SENSOR_RELEASE, NULL);

	if (p_mctl->csic_sdev) {
		v4l2_subdev_call(p_mctl->csic_sdev, core, ioctl,
			VIDIOC_MSM_CSIC_RELEASE, NULL);
	}

	if (p_mctl->vpe_sdev) {
		v4l2_subdev_call(p_mctl->vpe_sdev, core, ioctl,
			VIDIOC_MSM_VPE_RELEASE, NULL);
	}

	if (p_mctl->axi_sdev) {
		v4l2_set_subdev_hostdata(p_mctl->axi_sdev, p_mctl);
		v4l2_subdev_call(p_mctl->axi_sdev, core, ioctl,
			VIDIOC_MSM_AXI_RELEASE, NULL);
	}

	if (p_mctl->csid_sdev) {
		v4l2_subdev_call(p_mctl->csid_sdev, core, ioctl,
			VIDIOC_MSM_CSID_RELEASE, NULL);
	}

	if (p_mctl->csiphy_sdev) {
		v4l2_subdev_call(p_mctl->csiphy_sdev, core, ioctl,
			VIDIOC_MSM_CSIPHY_RELEASE,
			sinfo->sensor_platform_info->csi_lane_params);
	}

	if (p_mctl->act_sdev) {
		v4l2_subdev_call(p_mctl->act_sdev, core, s_power, 0);
		p_mctl->act_sdev = NULL;
	}

	v4l2_subdev_call(p_mctl->sensor_sdev, core, s_power, 0);

	pm_qos_update_request(&p_mctl->pm_qos_req_list,
				PM_QOS_DEFAULT_VALUE);
	pm_qos_remove_request(&p_mctl->pm_qos_req_list);

	wake_unlock(&p_mctl->wake_lock);
}

int msm_mctl_init_user_formats(struct msm_cam_v4l2_device *pcam)
{
	struct v4l2_subdev *sd = pcam->sensor_sdev;
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
int msm_mctl_init(struct msm_cam_v4l2_device *pcam)
{
	struct msm_cam_media_controller *pmctl = NULL;
	D("%s\n", __func__);
	if (!pcam) {
		pr_err("%s: param is NULL", __func__);
		return -EINVAL;
	}
	pcam->mctl_handle = msm_cam_server_get_mctl_handle();
	if (pcam->mctl_handle == 0) {
		pr_err("%s: cannot get mctl handle", __func__);
		return -EINVAL;
	}

	pmctl = msm_cam_server_get_mctl(pcam->mctl_handle);
	if (!pmctl) {
		pr_err("%s: invalid mctl controller", __func__);
		return -EINVAL;
	}

	wake_lock_init(&pmctl->wake_lock, WAKE_LOCK_SUSPEND, "msm_camera");
	mutex_init(&pmctl->lock);
	pmctl->opencnt = 0;

	/* init module operations*/
	pmctl->mctl_open = msm_mctl_open;
	pmctl->mctl_cmd = msm_mctl_cmd;
	pmctl->mctl_release = msm_mctl_release;
	pmctl->isp_config = msm_isp_config;
	pmctl->isp_notify = msm_isp_notify;

	/* init mctl buf */
	msm_mctl_buf_init(pcam);
	memset(&pmctl->pp_info, 0, sizeof(pmctl->pp_info));
	pmctl->vfe_output_mode = 0;
	spin_lock_init(&pmctl->pp_info.lock);

	pmctl->flash_sdev = pcam->flash_sdev;
	pmctl->act_sdev = pcam->act_sdev;
	pmctl->eeprom_sdev = pcam->eeprom_sdev;
	pmctl->sensor_sdev = pcam->sensor_sdev;
	pmctl->sdata = pcam->sdata;
	v4l2_set_subdev_hostdata(pcam->sensor_sdev, pmctl);

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	pmctl->client = msm_ion_client_create(-1, "camera");
	kref_init(&pmctl->refcount);
#endif

	return 0;
}

int msm_mctl_free(struct msm_cam_v4l2_device *pcam)
{
	int rc = 0;
	struct msm_cam_media_controller *pmctl = NULL;
	D("%s\n", __func__);

	pmctl = msm_cam_server_get_mctl(pcam->mctl_handle);
	if (!pmctl) {
		pr_err("%s: invalid mctl controller", __func__);
		return -EINVAL;
	}

	mutex_destroy(&pmctl->lock);
	wake_lock_destroy(&pmctl->wake_lock);
	/*clear out mctl fields*/
	memset(pmctl, 0, sizeof(struct msm_cam_media_controller));
	msm_cam_server_free_mctl(pcam->mctl_handle);
	return rc;
}

/* mctl node v4l2_file_operations */
static int msm_mctl_dev_open(struct file *f)
{
	int rc = -EINVAL, i;
	/* get the video device */
	struct msm_cam_v4l2_device *pcam  = NULL;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct msm_cam_media_controller *pmctl;

	if (f == NULL) {
		pr_err("%s :: cannot open video driver data", __func__);
		return rc;
	}
	pcam = video_drvdata(f);

	if (!pcam) {
		pr_err("%s NULL pointer passed in!\n", __func__);
		return rc;
	}

	D("%s : E use_count %d", __func__, pcam->mctl_node.use_count);
	mutex_lock(&pcam->mctl_node.dev_lock);
	for (i = 0; i < MSM_DEV_INST_MAX; i++) {
		if (pcam->mctl_node.dev_inst[i] == NULL)
			break;
	}
	/* if no instance is available, return error */
	if (i == MSM_DEV_INST_MAX) {
		mutex_unlock(&pcam->mctl_node.dev_lock);
		return rc;
	}
	pcam_inst = kzalloc(sizeof(struct msm_cam_v4l2_dev_inst), GFP_KERNEL);
	if (!pcam_inst) {
		mutex_unlock(&pcam->mctl_node.dev_lock);
		return rc;
	}

	pcam_inst->sensor_pxlcode = pcam->usr_fmts[0].pxlcode;
	pcam_inst->my_index = i;
	pcam_inst->pcam = pcam;
	mutex_init(&pcam_inst->inst_lock);
	pcam->mctl_node.dev_inst[i] = pcam_inst;

	D("%s pcam_inst %p my_index = %d\n", __func__,
		pcam_inst, pcam_inst->my_index);
	rc = msm_cam_server_open_mctl_session(pcam,
		&pcam->mctl_node.active);
	if (rc < 0) {
		pr_err("%s: mctl session open failed %d", __func__, rc);
		mutex_unlock(&pcam->mctl_node.dev_lock);
		return rc;
	}

	pmctl = msm_cam_server_get_mctl(pcam->mctl_handle);
	if (!pmctl) {
		pr_err("%s mctl NULL!\n", __func__);
		return rc;
	}

	D("%s active %d\n", __func__, pcam->mctl_node.active);
	msm_setup_v4l2_event_queue(&pcam_inst->eventHandle,
			pcam->mctl_node.pvdev);

	pcam_inst->vbqueue_initialized = 0;
	kref_get(&pmctl->refcount);
	f->private_data = &pcam_inst->eventHandle;

	D("f->private_data = 0x%x, pcam = 0x%x\n",
		(u32)f->private_data, (u32)pcam_inst);

	pcam->mctl_node.use_count++;
	mutex_unlock(&pcam->mctl_node.dev_lock);
	D("%s : X ", __func__);
	return rc;
}

static unsigned int msm_mctl_dev_poll(struct file *f,
				struct poll_table_struct *wait)
{
	int rc = 0;
	struct msm_cam_v4l2_device *pcam;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
			struct msm_cam_v4l2_dev_inst, eventHandle);
	pcam = pcam_inst->pcam;

	D("%s : E pcam_inst = %p", __func__, pcam_inst);
	if (!pcam) {
		pr_err("%s NULL pointer of camera device!\n", __func__);
		return -EINVAL;
	}

	poll_wait(f, &(pcam_inst->eventHandle.wait), wait);
	if (v4l2_event_pending(&pcam_inst->eventHandle)) {
		rc |= POLLPRI;
		D("%s Event available on mctl node ", __func__);
	}

	D("%s poll on vb2\n", __func__);
	if (!pcam_inst->vid_bufq.streaming) {
		D("%s vid_bufq.streaming is off, inst=0x%x\n",
				__func__, (u32)pcam_inst);
		return rc;
	}
	rc |= vb2_poll(&pcam_inst->vid_bufq, f, wait);

	D("%s : X ", __func__);
	return rc;
}

static int msm_mctl_dev_close(struct file *f)
{
	int rc = 0;
	struct msm_cam_v4l2_device *pcam;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct msm_cam_media_controller *pmctl;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);
	pcam = pcam_inst->pcam;

	D("%s : E ", __func__);
	if (!pcam) {
		pr_err("%s NULL pointer of camera device!\n", __func__);
		return -EINVAL;
	}

	pmctl = msm_cam_server_get_mctl(pcam->mctl_handle);
	mutex_lock(&pcam->mctl_node.dev_lock);
	mutex_lock(&pcam_inst->inst_lock);
	D("%s : active %d ", __func__, pcam->mctl_node.active);
	if (pcam->mctl_node.active == 1) {
		rc = msm_cam_server_close_mctl_session(pcam);
		if (rc < 0) {
			pr_err("%s: mctl session close failed %d",
				__func__, rc);
			mutex_unlock(&pcam->mctl_node.dev_lock);
			return rc;
		}
		pmctl = NULL;
	}
	pcam_inst->streamon = 0;
	pcam->mctl_node.use_count--;
	pcam->mctl_node.dev_inst_map[pcam_inst->image_mode] = NULL;
	if (pcam_inst->vbqueue_initialized)
		vb2_queue_release(&pcam_inst->vid_bufq);
	D("%s Closing down instance %p ", __func__, pcam_inst);
	pcam->mctl_node.dev_inst[pcam_inst->my_index] = NULL;
	msm_destroy_v4l2_event_queue(&pcam_inst->eventHandle);
	CLR_MCTLPP_INST_IDX(pcam_inst->inst_handle);
	CLR_DEVID_MODE(pcam_inst->inst_handle);
	CLR_IMG_MODE(pcam_inst->inst_handle);
	mutex_unlock(&pcam_inst->inst_lock);
	mutex_destroy(&pcam_inst->inst_lock);

	kfree(pcam_inst);
	f->private_data = NULL;
	if (NULL != pmctl) {
		D("%s : release ion client", __func__);
		kref_put(&pmctl->refcount, msm_release_ion_client);
	}
	mutex_unlock(&pcam->mctl_node.dev_lock);
	D("%s : use_count %d X ", __func__, pcam->mctl_node.use_count);
	return rc;
}

static struct v4l2_file_operations g_msm_mctl_fops = {
	.owner   = THIS_MODULE,
	.open	= msm_mctl_dev_open,
	.poll	= msm_mctl_dev_poll,
	.release = msm_mctl_dev_close,
	.unlocked_ioctl = video_ioctl2,
};

/*
 *
 * implementation of mctl node v4l2_ioctl_ops
 *
 */
static int msm_mctl_v4l2_querycap(struct file *f, void *pctx,
				struct v4l2_capability *pcaps)
{
	struct msm_cam_v4l2_device *pcam;

	if (f == NULL) {
		pr_err("%s :: NULL file pointer", __func__);
		return -EINVAL;
	}

	pcam = video_drvdata(f);

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	if (!pcam) {
		pr_err("%s NULL pointer passed in!\n", __func__);
		return -EINVAL;
	}

	strlcpy(pcaps->driver, pcam->media_dev.dev->driver->name,
			sizeof(pcaps->driver));
	pcaps->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	return 0;
}

static int msm_mctl_v4l2_queryctrl(struct file *f, void *pctx,
				struct v4l2_queryctrl *pqctrl)
{
	int rc = 0;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	return rc;
}

static int msm_mctl_v4l2_g_ctrl(struct file *f, void *pctx,
					struct v4l2_control *c)
{
	int rc = 0;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	return rc;
}

static int msm_mctl_v4l2_s_ctrl(struct file *f, void *pctx,
					struct v4l2_control *ctrl)
{
	int rc = 0;
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
			struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s\n", __func__);

	WARN_ON(pctx != f->private_data);
	mutex_lock(&pcam->mctl_node.dev_lock);
	if (ctrl->id == MSM_V4L2_PID_PP_PLANE_INFO) {
		if (copy_from_user(&pcam_inst->plane_info,
					(void *)ctrl->value,
					sizeof(struct img_plane_info))) {
			pr_err("%s inst %p Copying plane_info failed ",
					__func__, pcam_inst);
			rc = -EFAULT;
		}
		D("%s inst %p got plane info: num_planes = %d,"
				"plane size = %ld %ld ", __func__, pcam_inst,
				pcam_inst->plane_info.num_planes,
				pcam_inst->plane_info.plane[0].size,
				pcam_inst->plane_info.plane[1].size);
	} else
		pr_err("%s Unsupported S_CTRL Value ", __func__);

	mutex_unlock(&pcam->mctl_node.dev_lock);

	return rc;
}

static int msm_mctl_v4l2_reqbufs(struct file *f, void *pctx,
				struct v4l2_requestbuffers *pb)
{
	int rc = 0, i, j;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	struct msm_cam_media_controller *pmctl;
	struct msm_cam_v4l2_device *pcam = video_drvdata(f);
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);
	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	mutex_lock(&pcam_inst->inst_lock);
	if (!pcam_inst->vbqueue_initialized && pb->count) {
		pmctl = msm_cam_server_get_mctl(pcam->mctl_handle);
		if (pmctl == NULL) {
			pr_err("%s Invalid mctl ptr", __func__);
			return -EINVAL;
		}
		pmctl->mctl_vbqueue_init(pcam_inst, &pcam_inst->vid_bufq,
			pb->type);
		pcam_inst->vbqueue_initialized = 1;
	}
	rc = vb2_reqbufs(&pcam_inst->vid_bufq, pb);
	if (rc < 0) {
		pr_err("%s reqbufs failed %d ", __func__, rc);
		mutex_unlock(&pcam_inst->inst_lock);
		return rc;
	}
	if (!pb->count) {
		/* Deallocation. free buf_offset array */
		D("%s Inst %p freeing buffer offsets array",
			__func__, pcam_inst);
		for (j = 0 ; j < pcam_inst->buf_count ; j++) {
			kfree(pcam_inst->buf_offset[j]);
			pcam_inst->buf_offset[j] = NULL;
		}
		kfree(pcam_inst->buf_offset);
		pcam_inst->buf_offset = NULL;
		/* If the userspace has deallocated all the
		 * buffers, then release the vb2 queue */
		if (pcam_inst->vbqueue_initialized) {
			vb2_queue_release(&pcam_inst->vid_bufq);
			pcam_inst->vbqueue_initialized = 0;
		}
	} else {
		D("%s Inst %p Allocating buf_offset array",
			__func__, pcam_inst);
		/* Allocation. allocate buf_offset array */
		pcam_inst->buf_offset = (struct msm_cam_buf_offset **)
			kzalloc(pb->count * sizeof(struct msm_cam_buf_offset *),
							GFP_KERNEL);
		if (!pcam_inst->buf_offset) {
			pr_err("%s out of memory ", __func__);
			mutex_unlock(&pcam_inst->inst_lock);
			return -ENOMEM;
		}
		for (i = 0; i < pb->count; i++) {
			pcam_inst->buf_offset[i] =
				kzalloc(sizeof(struct msm_cam_buf_offset) *
				pcam_inst->plane_info.num_planes, GFP_KERNEL);
			if (!pcam_inst->buf_offset[i]) {
				pr_err("%s out of memory ", __func__);
				for (j = i-1 ; j >= 0; j--) {
					kfree(pcam_inst->buf_offset[j]);
					pcam_inst->buf_offset[j] = NULL;
				}
				kfree(pcam_inst->buf_offset);
				pcam_inst->buf_offset = NULL;
				mutex_unlock(&pcam_inst->inst_lock);
				return -ENOMEM;
			}
		}
	}
	pcam_inst->buf_count = pb->count;
	D("%s inst %p, buf count %d ", __func__,
		pcam_inst, pcam_inst->buf_count);
	mutex_unlock(&pcam_inst->inst_lock);
	return rc;
}

static int msm_mctl_v4l2_querybuf(struct file *f, void *pctx,
					struct v4l2_buffer *pb)
{
	/* get the video device */
	int rc = 0;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);
	mutex_lock(&pcam_inst->inst_lock);
	rc = vb2_querybuf(&pcam_inst->vid_bufq, pb);
	mutex_unlock(&pcam_inst->inst_lock);
	return rc;
}

static int msm_mctl_v4l2_qbuf(struct file *f, void *pctx,
					struct v4l2_buffer *pb)
{
	int rc = 0, i = 0;
	/* get the camera device */
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s Inst = %p\n", __func__, pcam_inst);
	WARN_ON(pctx != f->private_data);

	mutex_lock(&pcam_inst->inst_lock);
	if (!pcam_inst->buf_offset) {
		pr_err("%s Buffer is already released. Returning. ", __func__);
		mutex_unlock(&pcam_inst->inst_lock);
		return -EINVAL;
	}

	if (pb->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		/* Reject the buffer if planes array was not allocated */
		if (pb->m.planes == NULL) {
			pr_err("%s Planes array is null ", __func__);
			mutex_unlock(&pcam_inst->inst_lock);
			return -EINVAL;
		}
		for (i = 0; i < pcam_inst->plane_info.num_planes; i++) {
			D("%s stored offsets for plane %d as"
				"addr offset %d, data offset %d",
				__func__, i, pb->m.planes[i].reserved[0],
				pb->m.planes[i].data_offset);
			pcam_inst->buf_offset[pb->index][i].data_offset =
				pb->m.planes[i].data_offset;
			pcam_inst->buf_offset[pb->index][i].addr_offset =
				pb->m.planes[i].reserved[0];
			pcam_inst->plane_info.plane[i].offset = 0;
			D("%s, len %d user[%d] %p buf_len %d\n",
				__func__, pb->length, i,
				(void *)pb->m.planes[i].m.userptr,
				pb->m.planes[i].length);
		}
	} else {
		D("%s stored reserved info %d", __func__, pb->reserved);
		pcam_inst->buf_offset[pb->index][0].addr_offset = pb->reserved;
	}

	rc = vb2_qbuf(&pcam_inst->vid_bufq, pb);
	D("%s, videobuf_qbuf returns %d\n", __func__, rc);

	mutex_unlock(&pcam_inst->inst_lock);
	return rc;
}

static int msm_mctl_v4l2_dqbuf(struct file *f, void *pctx,
					struct v4l2_buffer *pb)
{
	int rc = 0, i;
	/* get the camera device */
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);
	mutex_lock(&pcam_inst->inst_lock);
	if (0 == pcam_inst->streamon) {
		mutex_unlock(&pcam_inst->inst_lock);
		return -EACCES;
	}

	rc = vb2_dqbuf(&pcam_inst->vid_bufq, pb,  f->f_flags & O_NONBLOCK);
	D("%s, videobuf_dqbuf returns %d\n", __func__, rc);

	if (pb->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		/* Reject the buffer if planes array was not allocated */
		if (pb->m.planes == NULL) {
			pr_err("%s Planes array is null\n", __func__);
			mutex_unlock(&pcam_inst->inst_lock);
			return -EINVAL;
		}
		for (i = 0; i < pcam_inst->plane_info.num_planes; i++) {
			pb->m.planes[i].data_offset =
				pcam_inst->buf_offset[pb->index][i].data_offset;
			pb->m.planes[i].reserved[0] =
				pcam_inst->buf_offset[pb->index][i].addr_offset;
			D("%s update offsets for plane %d as A %d D %d\n",
				__func__, i, pb->m.planes[i].reserved[0],
				pb->m.planes[i].data_offset);
		}
	} else {
		pb->reserved = pcam_inst->buf_offset[pb->index][0].addr_offset;
		D("%s stored reserved info %d\n", __func__, pb->reserved);
	}
	mutex_unlock(&pcam_inst->inst_lock);
	return rc;
}

static int msm_mctl_v4l2_streamon(struct file *f, void *pctx,
					enum v4l2_buf_type buf_type)
{
	int rc = 0;
	/* get the camera device */
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s Inst %p\n", __func__, pcam_inst);
	WARN_ON(pctx != f->private_data);

	mutex_lock(&pcam->mctl_node.dev_lock);
	mutex_lock(&pcam_inst->inst_lock);
	if ((buf_type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) &&
		(buf_type != V4L2_BUF_TYPE_VIDEO_CAPTURE)) {
		pr_err("%s Invalid buffer type ", __func__);
		mutex_unlock(&pcam_inst->inst_lock);
		mutex_unlock(&pcam->mctl_node.dev_lock);
		return -EINVAL;
	}

	D("%s Calling videobuf_streamon", __func__);
	/* if HW streaming on is successful, start buffer streaming */
	rc = vb2_streamon(&pcam_inst->vid_bufq, buf_type);
	D("%s, videobuf_streamon returns %d\n", __func__, rc);

	/* turn HW (VFE/sensor) streaming */
	pcam_inst->streamon = 1;
	mutex_unlock(&pcam_inst->inst_lock);
	mutex_unlock(&pcam->mctl_node.dev_lock);
	D("%s rc = %d\n", __func__, rc);
	return rc;
}

static int msm_mctl_v4l2_streamoff(struct file *f, void *pctx,
					enum v4l2_buf_type buf_type)
{
	int rc = 0;
	/* get the camera device */
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s Inst %p\n", __func__, pcam_inst);
	WARN_ON(pctx != f->private_data);

	if ((buf_type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) &&
		(buf_type != V4L2_BUF_TYPE_VIDEO_CAPTURE)) {
		pr_err("%s Invalid buffer type ", __func__);
		return -EINVAL;
	}

	/* first turn of HW (VFE/sensor) streaming so that buffers are
		not in use when we free the buffers */
	mutex_lock(&pcam->mctl_node.dev_lock);
	mutex_lock(&pcam_inst->inst_lock);
	pcam_inst->streamon = 0;
	if (rc < 0)
		pr_err("%s: hw failed to stop streaming\n", __func__);

	/* stop buffer streaming */
	rc = vb2_streamoff(&pcam_inst->vid_bufq, buf_type);
	D("%s, videobuf_streamoff returns %d\n", __func__, rc);
	mutex_unlock(&pcam_inst->inst_lock);
	mutex_unlock(&pcam->mctl_node.dev_lock);
	return rc;
}

static int msm_mctl_v4l2_enum_fmt_cap(struct file *f, void *pctx,
					struct v4l2_fmtdesc *pfmtdesc)
{
	/* get the video device */
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);
	const struct msm_isp_color_fmt *isp_fmt;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);
	if ((pfmtdesc->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) &&
		(pfmtdesc->type != V4L2_BUF_TYPE_VIDEO_CAPTURE))
		return -EINVAL;

	if (pfmtdesc->index >= pcam->num_fmts)
		return -EINVAL;

	isp_fmt = &pcam->usr_fmts[pfmtdesc->index];

	if (isp_fmt->name)
		strlcpy(pfmtdesc->description, isp_fmt->name,
						sizeof(pfmtdesc->description));

	pfmtdesc->pixelformat = isp_fmt->fourcc;

	D("%s: [%d] 0x%x, %s\n", __func__, pfmtdesc->index,
		isp_fmt->fourcc, isp_fmt->name);
	return 0;
}

static int msm_mctl_v4l2_g_fmt_cap(struct file *f,
		void *pctx, struct v4l2_format *pfmt)
{
	int rc = 0;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	if (pfmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	return rc;
}

static int msm_mctl_v4l2_g_fmt_cap_mplane(struct file *f,
		void *pctx, struct v4l2_format *pfmt)
{
	int rc = 0;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	if (pfmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	return rc;
}

/* This function will readjust the format parameters based in HW
  capabilities. Called by s_fmt_cap
*/
static int msm_mctl_v4l2_try_fmt_cap(struct file *f, void *pctx,
					struct v4l2_format *pfmt)
{
	int rc = 0;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	return rc;
}

static int msm_mctl_v4l2_try_fmt_cap_mplane(struct file *f, void *pctx,
					struct v4l2_format *pfmt)
{
	int rc = 0;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	return rc;
}

/* This function will reconfig the v4l2 driver and HW device, it should be
   called after the streaming is stopped.
*/
static int msm_mctl_v4l2_s_fmt_cap(struct file *f, void *pctx,
					struct v4l2_format *pfmt)
{
	int rc = 0;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	return rc;
}

static int msm_mctl_v4l2_s_fmt_cap_mplane(struct file *f, void *pctx,
				struct v4l2_format *pfmt)
{
	int rc = 0, i;
	struct msm_cam_v4l2_device *pcam = video_drvdata(f);
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
			struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s Inst %p\n", __func__, pcam_inst);
	WARN_ON(pctx != f->private_data);

	for (i = 0; i < pcam->num_fmts; i++)
		if (pcam->usr_fmts[i].fourcc == pfmt->fmt.pix_mp.pixelformat)
			break;
	if (i == pcam->num_fmts) {
		pr_err("%s: User requested pixelformat %x not supported\n",
			__func__, pfmt->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}
	pcam_inst->vid_fmt = *pfmt;
	pcam_inst->sensor_pxlcode =
		pcam->usr_fmts[i].pxlcode;
	D("%s: inst=%p, width=%d, heigth=%d\n",
		__func__, pcam_inst,
		pcam_inst->vid_fmt.fmt.pix_mp.width,
		pcam_inst->vid_fmt.fmt.pix_mp.height);
	return rc;
}
static int msm_mctl_v4l2_g_jpegcomp(struct file *f, void *pctx,
				struct v4l2_jpegcompression *pcomp)
{
	int rc = -EINVAL;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	return rc;
}

static int msm_mctl_v4l2_s_jpegcomp(struct file *f, void *pctx,
				struct v4l2_jpegcompression *pcomp)
{
	int rc = -EINVAL;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	return rc;
}


static int msm_mctl_v4l2_g_crop(struct file *f, void *pctx,
					struct v4l2_crop *crop)
{
	int rc = -EINVAL;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	return rc;
}

static int msm_mctl_v4l2_s_crop(struct file *f, void *pctx,
					struct v4l2_crop *a)
{
	int rc = -EINVAL;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	return rc;
}

/* Stream type-dependent parameter ioctls */
static int msm_mctl_v4l2_g_parm(struct file *f, void *pctx,
				struct v4l2_streamparm *a)
{
	int rc = -EINVAL;
	return rc;
}

static int msm_mctl_vidbuf_get_path(u32 extendedmode)
{
	switch (extendedmode) {
	case MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL:
		return OUTPUT_TYPE_T;
	case MSM_V4L2_EXT_CAPTURE_MODE_MAIN:
		return OUTPUT_TYPE_S;
	case MSM_V4L2_EXT_CAPTURE_MODE_VIDEO:
		return OUTPUT_TYPE_V;
	case MSM_V4L2_EXT_CAPTURE_MODE_RDI:
		return OUTPUT_TYPE_R;
	case MSM_V4L2_EXT_CAPTURE_MODE_RDI1:
		return OUTPUT_TYPE_R1;
	case MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT:
	case MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW:
	default:
		return OUTPUT_TYPE_P;
	}
}

static int msm_mctl_v4l2_s_parm(struct file *f, void *pctx,
				struct v4l2_streamparm *a)
{
	int rc = 0;
	int is_bayer_sensor = 0;
	struct msm_cam_media_controller *pmctl = NULL;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);
	pcam_inst->image_mode = (a->parm.capture.extendedmode & 0x7F);

	pmctl = msm_cam_server_get_mctl(pcam_inst->pcam->mctl_handle);
	if (!pmctl) {
		pr_err("%s: invalid mctl controller", __func__);
		return -EINVAL;
	}
	/* save msm_dev node idx for subdev notify lookup */
	SET_DEVID_MODE(pcam_inst->inst_handle, pmctl->pcam_ptr->vnode_id);
	SET_IMG_MODE(pcam_inst->inst_handle, pcam_inst->image_mode);
	SET_MCTLPP_INST_IDX(pcam_inst->inst_handle, pcam_inst->my_index);
	pcam_inst->pcam->mctl_node.dev_inst_map[pcam_inst->image_mode] =
		pcam_inst;
	pcam_inst->path = msm_mctl_vidbuf_get_path(pcam_inst->image_mode);
	if (pcam_inst->pcam->sdata->sensor_type == BAYER_SENSOR)
		is_bayer_sensor = 1;
	rc = msm_cam_server_config_interface_map(pcam_inst->image_mode,
			pcam_inst->pcam->mctl_handle,
			pcam_inst->pcam->vnode_id, is_bayer_sensor);
	D("%s path=%d, image mode = %d rc=%d\n", __func__,
		pcam_inst->path, pcam_inst->image_mode, rc);
	return rc;
}

static int msm_mctl_v4l2_subscribe_event(struct v4l2_fh *fh,
			struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst =
		(struct msm_cam_v4l2_dev_inst *)container_of(fh,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s:fh = 0x%x, type = 0x%x\n", __func__, (u32)fh, sub->type);

	if (sub->type == V4L2_EVENT_ALL)
		sub->type = V4L2_EVENT_PRIVATE_START+MSM_CAM_APP_NOTIFY_EVENT;
	rc = v4l2_event_subscribe(fh, sub, 30);
	if (rc < 0)
		pr_err("%s: failed for evtType = 0x%x, rc = %d\n",
						__func__, sub->type, rc);
	return rc;
}

static int msm_mctl_v4l2_unsubscribe_event(struct v4l2_fh *fh,
			struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst =
		(struct msm_cam_v4l2_dev_inst *)container_of(fh,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s: fh = 0x%x\n", __func__, (u32)fh);

	rc = v4l2_event_unsubscribe(fh, sub);
	D("%s: rc = %d\n", __func__, rc);
	return rc;
}

static int msm_mctl_v4l2_private_g_ctrl(struct file *f, void *pctx,
	struct msm_camera_v4l2_ioctl_t *ioctl_ptr)
{
	int rc = -EINVAL;
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	WARN_ON(pctx != f->private_data);

	mutex_lock(&pcam->mctl_node.dev_lock);
	switch (ioctl_ptr->id) {
	case MSM_V4L2_PID_INST_HANDLE:
		COPY_TO_USER(rc, (void __user *)ioctl_ptr->ioctl_ptr,
			(void *)&pcam_inst->inst_handle, sizeof(uint32_t));
		if (rc)
			ERR_COPY_TO_USER();
		break;
	default:
		pr_err("%s Unsupported ioctl %d ", __func__, ioctl_ptr->id);
		break;
	}
	mutex_unlock(&pcam->mctl_node.dev_lock);
	return rc;
}

static long msm_mctl_v4l2_private_ioctl(struct file *file, void *fh,
	  bool valid_prio, int cmd, void *arg)
{
	int rc = -EINVAL;
	struct msm_camera_v4l2_ioctl_t *ioctl_ptr = arg;
	D("%s: cmd %d\n", __func__, _IOC_NR(cmd));

	switch (cmd) {
	case MSM_CAM_V4L2_IOCTL_PRIVATE_G_CTRL:
		rc = msm_mctl_v4l2_private_g_ctrl(file, fh, ioctl_ptr);
		break;
	default:
		pr_err("%s Unsupported ioctl cmd %d ", __func__, cmd);
		break;
	}
	return rc;
}

/* mctl node v4l2_ioctl_ops */
static const struct v4l2_ioctl_ops g_msm_mctl_ioctl_ops = {
	.vidioc_querycap = msm_mctl_v4l2_querycap,

	.vidioc_s_crop = msm_mctl_v4l2_s_crop,
	.vidioc_g_crop = msm_mctl_v4l2_g_crop,

	.vidioc_queryctrl = msm_mctl_v4l2_queryctrl,
	.vidioc_g_ctrl = msm_mctl_v4l2_g_ctrl,
	.vidioc_s_ctrl = msm_mctl_v4l2_s_ctrl,

	.vidioc_reqbufs = msm_mctl_v4l2_reqbufs,
	.vidioc_querybuf = msm_mctl_v4l2_querybuf,
	.vidioc_qbuf = msm_mctl_v4l2_qbuf,
	.vidioc_dqbuf = msm_mctl_v4l2_dqbuf,

	.vidioc_streamon = msm_mctl_v4l2_streamon,
	.vidioc_streamoff = msm_mctl_v4l2_streamoff,

	/* format ioctls */
	.vidioc_enum_fmt_vid_cap = msm_mctl_v4l2_enum_fmt_cap,
	.vidioc_enum_fmt_vid_cap_mplane = msm_mctl_v4l2_enum_fmt_cap,
	.vidioc_try_fmt_vid_cap = msm_mctl_v4l2_try_fmt_cap,
	.vidioc_try_fmt_vid_cap_mplane = msm_mctl_v4l2_try_fmt_cap_mplane,
	.vidioc_g_fmt_vid_cap = msm_mctl_v4l2_g_fmt_cap,
	.vidioc_g_fmt_vid_cap_mplane = msm_mctl_v4l2_g_fmt_cap_mplane,
	.vidioc_s_fmt_vid_cap = msm_mctl_v4l2_s_fmt_cap,
	.vidioc_s_fmt_vid_cap_mplane = msm_mctl_v4l2_s_fmt_cap_mplane,

	.vidioc_g_jpegcomp = msm_mctl_v4l2_g_jpegcomp,
	.vidioc_s_jpegcomp = msm_mctl_v4l2_s_jpegcomp,

	/* Stream type-dependent parameter ioctls */
	.vidioc_g_parm =  msm_mctl_v4l2_g_parm,
	.vidioc_s_parm =  msm_mctl_v4l2_s_parm,

	/* event subscribe/unsubscribe */
	.vidioc_subscribe_event = msm_mctl_v4l2_subscribe_event,
	.vidioc_unsubscribe_event = msm_mctl_v4l2_unsubscribe_event,
	.vidioc_default = msm_mctl_v4l2_private_ioctl,
};

int msm_setup_mctl_node(struct msm_cam_v4l2_device *pcam)
{
	int rc = -EINVAL;
	struct video_device *pvdev = NULL;
	struct i2c_client *client = NULL;
	struct platform_device *pdev = NULL;
	D("%s\n", __func__);

	/* first register the v4l2 device */
	if (pcam->sensor_sdev->flags & V4L2_SUBDEV_FL_IS_I2C) {
		client = v4l2_get_subdevdata(pcam->sensor_sdev);
		pcam->mctl_node.v4l2_dev.dev = &client->dev;
	} else {
		pdev = v4l2_get_subdevdata(pcam->sensor_sdev);
		pcam->mctl_node.v4l2_dev.dev = &pdev->dev;
	}

	/* first register the v4l2 device */
	rc = v4l2_device_register(pcam->mctl_node.v4l2_dev.dev,
				&pcam->mctl_node.v4l2_dev);
	if (rc < 0)
		return -EINVAL;
	/*	else
			pcam->v4l2_dev.notify = msm_cam_v4l2_subdev_notify; */

	/* now setup video device */
	pvdev = video_device_alloc();
	if (pvdev == NULL) {
		pr_err("%s: video_device_alloc failed\n", __func__);
		return rc;
	}

	/* init video device's driver interface */
	D("sensor name = %s, sizeof(pvdev->name)=%d\n",
			pcam->sensor_sdev->name, sizeof(pvdev->name));

	/* device info - strlcpy is safer than strncpy but
	   only if architecture supports*/
	strlcpy(pvdev->name, pcam->sensor_sdev->name,
			sizeof(pvdev->name));

	pvdev->release   = video_device_release;
	pvdev->fops	  = &g_msm_mctl_fops;
	pvdev->ioctl_ops  = &g_msm_mctl_ioctl_ops;
	pvdev->minor	  = -1;
	pvdev->vfl_type   = 1;

	/* register v4l2 video device to kernel as /dev/videoXX */
	D("%s video_register_device\n", __func__);
	rc = video_register_device(pvdev,
			VFL_TYPE_GRABBER,
			-1);
	if (rc) {
		pr_err("%s: video_register_device failed\n", __func__);
		goto reg_fail;
	}
	D("%s: video device registered as /dev/video%d\n",
			__func__, pvdev->num);

	/* connect pcam and mctl video dev to each other */
	pcam->mctl_node.pvdev	= pvdev;
	video_set_drvdata(pcam->mctl_node.pvdev, pcam);

	return rc ;

reg_fail:
	video_device_release(pvdev);
	v4l2_device_unregister(&pcam->mctl_node.v4l2_dev);
	pcam->mctl_node.v4l2_dev.dev = NULL;
	return rc;
}
