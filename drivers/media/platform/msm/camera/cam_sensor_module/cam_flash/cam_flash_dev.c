/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include "cam_flash_dev.h"
#include "cam_flash_soc.h"
#include "cam_flash_core.h"

static int32_t cam_flash_driver_cmd(struct cam_flash_ctrl *fctrl,
		void *arg, struct cam_flash_private_soc *soc_private)
{
	int rc = 0;
	int i = 0;
	struct cam_control *cmd = (struct cam_control *)arg;

	if (!fctrl || !arg) {
		CAM_ERR(CAM_FLASH, "fctrl/arg is NULL with arg:%pK fctrl%pK",
			fctrl, arg);
		return -EINVAL;
	}

	mutex_lock(&(fctrl->flash_mutex));
	switch (cmd->op_code) {
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev flash_acq_dev;
		struct cam_create_dev_hdl bridge_params;

		CAM_DBG(CAM_FLASH, "CAM_ACQUIRE_DEV");

		if (fctrl->flash_state != CAM_FLASH_STATE_INIT) {
			CAM_ERR(CAM_FLASH,
				"Cannot apply Acquire dev: Prev state: %d",
				fctrl->flash_state);
			rc = -EINVAL;
			goto release_mutex;
		}

		if (fctrl->bridge_intf.device_hdl != -1) {
			CAM_ERR(CAM_FLASH, "Device is already acquired");
			rc = -EINVAL;
			goto release_mutex;
		}

		rc = copy_from_user(&flash_acq_dev, (void __user *)cmd->handle,
			sizeof(flash_acq_dev));
		if (rc) {
			CAM_ERR(CAM_FLASH, "Failed Copying from User");
			goto release_mutex;
		}

		bridge_params.session_hdl = flash_acq_dev.session_handle;
		bridge_params.ops = &fctrl->bridge_intf.ops;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = fctrl;

		flash_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		fctrl->bridge_intf.device_hdl =
			flash_acq_dev.device_handle;
		fctrl->bridge_intf.session_hdl =
			flash_acq_dev.session_handle;

		rc = copy_to_user((void __user *) cmd->handle, &flash_acq_dev,
			sizeof(struct cam_sensor_acquire_dev));
		if (rc) {
			CAM_ERR(CAM_FLASH, "Failed Copy to User with rc = %d",
				rc);
			rc = -EFAULT;
			goto release_mutex;
		}
		fctrl->flash_state = CAM_FLASH_STATE_ACQUIRE;
		break;
	}
	case CAM_RELEASE_DEV: {
		CAM_DBG(CAM_FLASH, "CAM_RELEASE_DEV");
		if ((fctrl->flash_state == CAM_FLASH_STATE_INIT) ||
			(fctrl->flash_state == CAM_FLASH_STATE_START)) {
			CAM_WARN(CAM_FLASH,
				"Cannot apply Release dev: Prev state:%d",
				fctrl->flash_state);
		}

		if (fctrl->bridge_intf.device_hdl == -1 &&
			fctrl->flash_state == CAM_FLASH_STATE_ACQUIRE) {
			CAM_ERR(CAM_FLASH,
				"Invalid Handle: Link Hdl: %d device hdl: %d",
				fctrl->bridge_intf.device_hdl,
				fctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_flash_release_dev(fctrl);
		if (rc)
			CAM_ERR(CAM_FLASH,
				"Failed in destroying the device Handle rc= %d",
				rc);
		fctrl->flash_state = CAM_FLASH_STATE_INIT;
		break;
	}
	case CAM_QUERY_CAP: {
		struct cam_flash_query_cap_info flash_cap = {0};

		CAM_DBG(CAM_FLASH, "CAM_QUERY_CAP");
		flash_cap.slot_info = fctrl->soc_info.index;
		for (i = 0; i < fctrl->flash_num_sources; i++) {
			flash_cap.max_current_flash[i] =
				soc_private->flash_max_current[i];
			flash_cap.max_duration_flash[i] =
				soc_private->flash_max_duration[i];
		}

		for (i = 0; i < fctrl->torch_num_sources; i++)
			flash_cap.max_current_torch[i] =
				soc_private->torch_max_current[i];

		if (copy_to_user((void __user *) cmd->handle, &flash_cap,
			sizeof(struct cam_flash_query_cap_info))) {
			CAM_ERR(CAM_FLASH, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
		break;
	}
	case CAM_START_DEV: {
		CAM_DBG(CAM_FLASH, "CAM_START_DEV");
		if ((fctrl->flash_state == CAM_FLASH_STATE_INIT) ||
			(fctrl->flash_state == CAM_FLASH_STATE_START)) {
			CAM_WARN(CAM_FLASH,
				"Cannot apply Start Dev: Prev state: %d",
				fctrl->flash_state);
			rc = -EINVAL;
			goto release_mutex;
		}

		rc = cam_flash_prepare(fctrl, true);
		if (rc) {
			CAM_ERR(CAM_FLASH,
				"Enable Regulator Failed rc = %d", rc);
			goto release_mutex;
		}
		rc = cam_flash_apply_setting(fctrl, 0);
		if (rc) {
			CAM_ERR(CAM_FLASH, "cannot apply settings rc = %d", rc);
			goto release_mutex;
		}
		fctrl->flash_state = CAM_FLASH_STATE_START;
		break;
	}
	case CAM_STOP_DEV: {
		CAM_DBG(CAM_FLASH, "CAM_STOP_DEV ENTER");
		if (fctrl->flash_state != CAM_FLASH_STATE_START) {
			CAM_WARN(CAM_FLASH,
				"Cannot apply Stop dev: Prev state is: %d",
				fctrl->flash_state);
			rc = -EINVAL;
			goto release_mutex;
		}

		rc = cam_flash_stop_dev(fctrl);
		if (rc) {
			CAM_ERR(CAM_FLASH, "Stop Dev Failed rc = %d",
				rc);
			goto release_mutex;
		}
		fctrl->flash_state = CAM_FLASH_STATE_ACQUIRE;
		break;
	}
	case CAM_CONFIG_DEV: {
		CAM_DBG(CAM_FLASH, "CAM_CONFIG_DEV");
		rc = cam_flash_parser(fctrl, arg);
		if (rc) {
			CAM_ERR(CAM_FLASH, "Failed Flash Config: rc=%d\n", rc);
			goto release_mutex;
		}
		break;
	}
	default:
		CAM_ERR(CAM_FLASH, "Invalid Opcode: %d", cmd->op_code);
		rc = -EINVAL;
	}

release_mutex:
	mutex_unlock(&(fctrl->flash_mutex));
	return rc;
}

static const struct of_device_id cam_flash_dt_match[] = {
	{.compatible = "qcom,camera-flash", .data = NULL},
	{}
};

static long cam_flash_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int rc = 0;
	struct cam_flash_ctrl *fctrl = NULL;
	struct cam_flash_private_soc *soc_private = NULL;

	CAM_DBG(CAM_FLASH, "Enter");

	fctrl = v4l2_get_subdevdata(sd);
	soc_private = fctrl->soc_info.soc_private;

	switch (cmd) {
	case VIDIOC_CAM_CONTROL: {
		rc = cam_flash_driver_cmd(fctrl, arg,
			soc_private);
		break;
	}
	default:
		CAM_ERR(CAM_FLASH, "Invalid ioctl cmd type");
		rc = -EINVAL;
		break;
	}

	CAM_DBG(CAM_FLASH, "Exit");
	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_flash_subdev_do_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	struct cam_control cmd_data;
	int32_t rc = 0;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_FLASH,
			"Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL: {
		rc = cam_flash_subdev_ioctl(sd, cmd, &cmd_data);
		if (rc)
			CAM_ERR(CAM_FLASH, "cam_flash_ioctl failed");
		break;
	}
	default:
		CAM_ERR(CAM_FLASH, "Invalid compat ioctl cmd_type:%d",
			cmd);
		rc = -EINVAL;
	}

	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_FLASH,
				"Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}

	return rc;
}
#endif

static int cam_flash_platform_remove(struct platform_device *pdev)
{
	struct cam_flash_ctrl *fctrl;

	fctrl = platform_get_drvdata(pdev);
	if (!fctrl) {
		CAM_ERR(CAM_FLASH, "Flash device is NULL");
		return 0;
	}

	devm_kfree(&pdev->dev, fctrl);

	return 0;
}

static int cam_flash_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_flash_ctrl *flash_ctrl =
		v4l2_get_subdevdata(sd);

	if (!flash_ctrl) {
		CAM_ERR(CAM_FLASH, "Flash ctrl ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&flash_ctrl->flash_mutex);
	cam_flash_shutdown(flash_ctrl);
	mutex_unlock(&flash_ctrl->flash_mutex);

	return 0;
}

static struct v4l2_subdev_core_ops cam_flash_subdev_core_ops = {
	.ioctl = cam_flash_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_flash_subdev_do_ioctl
#endif
};

static struct v4l2_subdev_ops cam_flash_subdev_ops = {
	.core = &cam_flash_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops cam_flash_internal_ops = {
	.close = cam_flash_subdev_close,
};

static int32_t cam_flash_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct cam_flash_ctrl *flash_ctrl = NULL;

	CAM_DBG(CAM_FLASH, "Enter");
	if (!pdev->dev.of_node) {
		CAM_ERR(CAM_FLASH, "of_node NULL");
		return -EINVAL;
	}

	flash_ctrl = kzalloc(sizeof(struct cam_flash_ctrl), GFP_KERNEL);
	if (!flash_ctrl)
		return -ENOMEM;

	flash_ctrl->pdev = pdev;
	flash_ctrl->soc_info.pdev = pdev;
	flash_ctrl->soc_info.dev = &pdev->dev;
	flash_ctrl->soc_info.dev_name = pdev->name;

	rc = cam_flash_get_dt_data(flash_ctrl, &flash_ctrl->soc_info);
	if (rc) {
		CAM_ERR(CAM_FLASH, "cam_flash_get_dt_data failed with %d", rc);
		kfree(flash_ctrl);
		return -EINVAL;
	}

	flash_ctrl->v4l2_dev_str.internal_ops =
		&cam_flash_internal_ops;
	flash_ctrl->v4l2_dev_str.ops = &cam_flash_subdev_ops;
	flash_ctrl->v4l2_dev_str.name = CAMX_FLASH_DEV_NAME;
	flash_ctrl->v4l2_dev_str.sd_flags =
		V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	flash_ctrl->v4l2_dev_str.ent_function = CAM_FLASH_DEVICE_TYPE;
	flash_ctrl->v4l2_dev_str.token = flash_ctrl;

	rc = cam_register_subdev(&(flash_ctrl->v4l2_dev_str));
	if (rc) {
		CAM_ERR(CAM_FLASH, "Fail to create subdev with %d", rc);
		goto free_resource;
	}
	flash_ctrl->bridge_intf.device_hdl = -1;
	flash_ctrl->bridge_intf.ops.get_dev_info = cam_flash_publish_dev_info;
	flash_ctrl->bridge_intf.ops.link_setup = cam_flash_establish_link;
	flash_ctrl->bridge_intf.ops.apply_req = cam_flash_apply_request;
	flash_ctrl->bridge_intf.ops.flush_req = cam_flash_flush_request;

	platform_set_drvdata(pdev, flash_ctrl);
	v4l2_set_subdevdata(&flash_ctrl->v4l2_dev_str.sd, flash_ctrl);

	mutex_init(&(flash_ctrl->flash_mutex));
	mutex_init(&(flash_ctrl->flash_wq_mutex));

	flash_ctrl->flash_state = CAM_FLASH_STATE_INIT;
	CAM_DBG(CAM_FLASH, "Probe success");
	return rc;
free_resource:
	kfree(flash_ctrl);
	return rc;
}

MODULE_DEVICE_TABLE(of, cam_flash_dt_match);

static struct platform_driver cam_flash_platform_driver = {
	.probe = cam_flash_platform_probe,
	.remove = cam_flash_platform_remove,
	.driver = {
		.name = "CAM-FLASH-DRIVER",
		.owner = THIS_MODULE,
		.of_match_table = cam_flash_dt_match,
	},
};

static int __init cam_flash_init_module(void)
{
	int32_t rc = 0;

	rc = platform_driver_register(&cam_flash_platform_driver);
	if (rc)
		CAM_ERR(CAM_FLASH, "platform probe for flash failed");

	return rc;
}

static void __exit cam_flash_exit_module(void)
{
	platform_driver_unregister(&cam_flash_platform_driver);
}

module_init(cam_flash_init_module);
module_exit(cam_flash_exit_module);
MODULE_DESCRIPTION("CAM FLASH");
MODULE_LICENSE("GPL v2");
