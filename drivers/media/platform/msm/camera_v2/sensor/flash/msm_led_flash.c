/* Copyright (c) 2009-2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include "msm_led_flash.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

static struct v4l2_file_operations msm_led_flash_v4l2_subdev_fops;

static long msm_led_flash_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct msm_led_flash_ctrl_t *fctrl = NULL;
	void __user *argp = (void __user *)arg;
	if (!sd) {
		pr_err("sd NULL\n");
		return -EINVAL;
	}
	fctrl = v4l2_get_subdevdata(sd);
	if (!fctrl) {
		pr_err("fctrl NULL\n");
		return -EINVAL;
	}
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return fctrl->func_tbl->flash_get_subdev_id(fctrl, argp);
	case VIDIOC_MSM_FLASH_LED_DATA_CFG:
		return fctrl->func_tbl->flash_led_config(fctrl, argp);
	case MSM_SD_NOTIFY_FREEZE:
		return 0;
	case MSM_SD_SHUTDOWN:
		*(int *)argp = MSM_CAMERA_LED_RELEASE;
		return fctrl->func_tbl->flash_led_config(fctrl, argp);
	default:
		pr_err_ratelimited("invalid cmd %d\n", cmd);
		return -ENOIOCTLCMD;
	}
}

static struct v4l2_subdev_core_ops msm_flash_subdev_core_ops = {
	.ioctl = msm_led_flash_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_flash_subdev_ops = {
	.core = &msm_flash_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_flash_internal_ops;

int32_t msm_led_flash_create_v4lsubdev(struct platform_device *pdev, void *data)
{
	struct msm_led_flash_ctrl_t *fctrl =
		(struct msm_led_flash_ctrl_t *)data;
	CDBG("Enter\n");

	if (!fctrl) {
		pr_err("fctrl NULL\n");
		return -EINVAL;
	}

	/* Initialize sub device */
	v4l2_subdev_init(&fctrl->msm_sd.sd, &msm_flash_subdev_ops);
	v4l2_set_subdevdata(&fctrl->msm_sd.sd, fctrl);

	fctrl->pdev = pdev;
	fctrl->msm_sd.sd.internal_ops = &msm_flash_internal_ops;
	fctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(fctrl->msm_sd.sd.name, ARRAY_SIZE(fctrl->msm_sd.sd.name),
		"msm_flash");
	media_entity_init(&fctrl->msm_sd.sd.entity, 0, NULL, 0);
	fctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	fctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_LED_FLASH;
	fctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x1;
	msm_sd_register(&fctrl->msm_sd);

	msm_led_flash_v4l2_subdev_fops = v4l2_subdev_fops;
#ifdef CONFIG_COMPAT
	msm_led_flash_v4l2_subdev_fops.compat_ioctl32 =
		msm_led_flash_v4l2_subdev_fops.unlocked_ioctl;
#endif
	fctrl->msm_sd.sd.devnode->fops = &msm_led_flash_v4l2_subdev_fops;
	CDBG("probe success\n");
	return 0;
}

int32_t msm_led_i2c_flash_create_v4lsubdev(void *data)
{
	struct msm_led_flash_ctrl_t *fctrl =
		(struct msm_led_flash_ctrl_t *)data;
	CDBG("Enter\n");

	if (!fctrl) {
		pr_err("fctrl NULL\n");
		return -EINVAL;
	}

	/* Initialize sub device */
	v4l2_subdev_init(&fctrl->msm_sd.sd, &msm_flash_subdev_ops);
	v4l2_set_subdevdata(&fctrl->msm_sd.sd, fctrl);

	fctrl->msm_sd.sd.internal_ops = &msm_flash_internal_ops;
	fctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(fctrl->msm_sd.sd.name, ARRAY_SIZE(fctrl->msm_sd.sd.name),
		"msm_flash");
	media_entity_init(&fctrl->msm_sd.sd.entity, 0, NULL, 0);
	fctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	fctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_LED_FLASH;
	msm_sd_register(&fctrl->msm_sd);

	msm_led_flash_v4l2_subdev_fops = v4l2_subdev_fops;
	fctrl->msm_sd.sd.devnode->fops = &msm_led_flash_v4l2_subdev_fops;

	CDBG("probe success\n");
	return 0;
}
