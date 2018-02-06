/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "CAM-BUFMGR %s:%d " fmt, __func__, __LINE__

#include <media/ais/msm_ais_mgr.h>
#include "msm_ais_mngr.h"
#include "msm_early_cam.h"
#include "msm_camera_diag_util.h"
#include "msm_diag_cam.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

static struct msm_ais_mngr_device *msm_ais_mngr_dev;

static long msm_ais_hndl_ioctl(struct v4l2_subdev *sd, void *arg)
{
	long rc = 0;
	struct clk_mgr_cfg_data *pcdata = (struct clk_mgr_cfg_data *)arg;
	struct msm_ais_mngr_device *clk_mngr_dev =
		(struct msm_ais_mngr_device *)v4l2_get_subdevdata(sd);

	if (WARN_ON(!clk_mngr_dev) || WARN_ON(!pcdata)) {
		rc = -EINVAL;
		return rc;
	}

	mutex_lock(&clk_mngr_dev->cont_mutex);
	CDBG(pr_fmt("cfg_type = %d\n"), pcdata->cfg_type);
	switch (pcdata->cfg_type) {
	case AIS_CLK_ENABLE:
		rc = msm_ais_enable_clocks();
		break;
	case AIS_CLK_DISABLE:
		rc = msm_ais_disable_clocks();
		break;
	case AIS_CLK_ENABLE_ALLCLK:
		rc = msm_ais_enable_allclocks();
		break;
	case AIS_CLK_DISABLE_ALLCLK:
		rc = msm_ais_disable_allclocks();
		break;
	default:
		pr_err("invalid cfg_type\n");
		rc = -EINVAL;
	}

	if (rc)
		pr_err("msm_ais_hndl_ioctl failed %ld\n", rc);

	mutex_unlock(&clk_mngr_dev->cont_mutex);
	return rc;
}

static long msm_ais_hndl_ext_ioctl(struct v4l2_subdev *sd, void *arg)
{
	long rc = 0;
	struct clk_mgr_cfg_data_ext *pcdata =
				(struct clk_mgr_cfg_data_ext *)arg;
	struct msm_ais_mngr_device *clk_mngr_dev =
		(struct msm_ais_mngr_device *)v4l2_get_subdevdata(sd);

	if (WARN_ON(!clk_mngr_dev) || WARN_ON(!pcdata)) {
		rc = -EINVAL;
		return rc;
	}

	mutex_lock(&clk_mngr_dev->cont_mutex);
	CDBG(pr_fmt("cfg_type = %d\n"), pcdata->cfg_type);
	switch (pcdata->cfg_type) {
	case AIS_DIAG_GET_REGULATOR_INFO_LIST:
		rc = msm_diag_camera_get_vreginfo_list(
				&pcdata->data.vreg_infolist);
		break;
	case AIS_DIAG_GET_BUS_INFO_STATE:
		rc = msm_camera_diag_get_ddrbw(&pcdata->data.bus_info);
		break;
	case AIS_DIAG_GET_CLK_INFO_LIST:
		rc = msm_camera_diag_get_clk_list(&pcdata->data.clk_infolist);
		break;
	case AIS_DIAG_GET_GPIO_LIST:
		rc = msm_camera_diag_get_gpio_list(&pcdata->data.gpio_list);
		break;
	case AIS_DIAG_SET_GPIO_LIST:
		rc = msm_camera_diag_set_gpio_list(&pcdata->data.gpio_list);
		break;
	default:
		pr_err("invalid cfg_type\n");
		rc = -EINVAL;
	}
	mutex_unlock(&clk_mngr_dev->cont_mutex);
	return rc;
}

static long msm_ais_mngr_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int32_t rc = 0;

	CDBG(pr_fmt("Enter\n"));
	switch (cmd) {
	case VIDIOC_MSM_AIS_CLK_CFG:
		rc = msm_ais_hndl_ioctl(sd, arg);
		if (rc)
			pr_err("msm_ais_mngr_subdev_ioctl failed\n");
		break;
	case VIDIOC_MSM_AIS_CLK_CFG_EXT:
		rc = msm_ais_hndl_ext_ioctl(sd, arg);
		if (rc)
			pr_err("msm_ais_hndl_ext_ioctl failed\n");
		break;
	default:
		rc = -ENOIOCTLCMD;
	}
	CDBG(pr_fmt("Exit\n"));
	return rc;
}

static struct v4l2_subdev_core_ops msm_ais_mngr_subdev_core_ops = {
	.ioctl = msm_ais_mngr_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_ais_mngr_subdev_ops = {
	.core = &msm_ais_mngr_subdev_core_ops,
};

static struct v4l2_file_operations msm_ais_v4l2_subdev_fops;

static long msm_clkmgr_subdev_do_ioctl(
		struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);

	return v4l2_subdev_call(sd, core, ioctl, cmd, arg);
}


static long msm_ais_subdev_fops_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_clkmgr_subdev_do_ioctl);
}

static int32_t __init msm_ais_mngr_init(void)
{
	int32_t rc = 0;

	msm_ais_mngr_dev = kzalloc(sizeof(*msm_ais_mngr_dev),
		GFP_KERNEL);
	if (!msm_ais_mngr_dev)
		return -ENOMEM;

	/* Sub-dev */
	v4l2_subdev_init(&msm_ais_mngr_dev->subdev.sd,
		&msm_ais_mngr_subdev_ops);
	msm_cam_copy_v4l2_subdev_fops(&msm_ais_v4l2_subdev_fops);
	msm_ais_v4l2_subdev_fops.unlocked_ioctl = msm_ais_subdev_fops_ioctl;

	snprintf(msm_ais_mngr_dev->subdev.sd.name,
		ARRAY_SIZE(msm_ais_mngr_dev->subdev.sd.name), "msm_ais_mngr");
	msm_ais_mngr_dev->subdev.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	v4l2_set_subdevdata(&msm_ais_mngr_dev->subdev.sd, msm_ais_mngr_dev);

	media_entity_init(&msm_ais_mngr_dev->subdev.sd.entity, 0, NULL, 0);
	msm_ais_mngr_dev->subdev.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	msm_ais_mngr_dev->subdev.sd.entity.group_id =
		MSM_CAMERA_SUBDEV_AIS_MNGR;
	msm_ais_mngr_dev->subdev.close_seq = MSM_SD_CLOSE_4TH_CATEGORY;
	rc = msm_sd_register(&msm_ais_mngr_dev->subdev);
	if (rc != 0) {
		pr_err("msm_sd_register error = %d\n", rc);
		kfree(msm_ais_mngr_dev);
		return rc;
	}

	msm_ais_mngr_dev->subdev.sd.devnode->fops = &msm_ais_v4l2_subdev_fops;
	mutex_init(&msm_ais_mngr_dev->cont_mutex);

	return rc;
}

static void __exit msm_ais_mngr_exit(void)
{

	msm_sd_unregister(&msm_ais_mngr_dev->subdev);
	mutex_destroy(&msm_ais_mngr_dev->cont_mutex);
	kfree(msm_ais_mngr_dev);
}

module_init(msm_ais_mngr_init);
module_exit(msm_ais_mngr_exit);
MODULE_DESCRIPTION("MSM AIS Manager");
MODULE_LICENSE("GPL v2");
