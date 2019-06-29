/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#include "cam_ir_led_dev.h"
#include "cam_ir_led_soc.h"
#include "cam_ir_led_core.h"

static int32_t cam_ir_led_driver_cmd(struct cam_ir_led_ctrl *ictrl,
		void *arg, struct cam_ir_led_private_soc *soc_private)
{
	int rc = 0;
	struct cam_control *cmd = (struct cam_control *)arg;

	if (!ictrl || !arg) {
		CAM_ERR(CAM_IR_LED, "ictrl/arg is NULL with arg:%pK ictrl%pK",
			ictrl, arg);
		return -EINVAL;
	}

	if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_IR_LED, "Invalid handle type: %d",
			cmd->handle_type);
		return -EINVAL;
	}

	mutex_lock(&(ictrl->ir_led_mutex));
	switch (cmd->op_code) {
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev ir_led_acq_dev;
		struct cam_create_dev_hdl dev_hdl;

		CAM_DBG(CAM_IR_LED, "CAM_ACQUIRE_DEV");

		if (ictrl->ir_led_state != CAM_IR_LED_STATE_INIT) {
			CAM_ERR(CAM_IR_LED,
				" Cannot apply Acquire dev: Prev state: %d",
				ictrl->ir_led_state);
			rc = -EINVAL;
			goto release_mutex;
		}

		rc = copy_from_user(&ir_led_acq_dev,
			u64_to_user_ptr(cmd->handle),
			sizeof(ir_led_acq_dev));
		if (rc) {
			CAM_ERR(CAM_IR_LED, "Failed Copy from User rc=%d", rc);
			goto release_mutex;
		}

		dev_hdl.priv = ictrl;

		ir_led_acq_dev.device_handle =
			cam_create_device_hdl(&dev_hdl);
		ictrl->device_hdl =
			ir_led_acq_dev.device_handle;

		rc = copy_to_user(u64_to_user_ptr(cmd->handle), &ir_led_acq_dev,
			sizeof(struct cam_sensor_acquire_dev));
		if (rc) {
			CAM_ERR(CAM_IR_LED, "Failed Copy to User rc=%d", rc);
			rc = -EFAULT;
			goto release_mutex;
		}
		ictrl->ir_led_state = CAM_IR_LED_STATE_ACQUIRE;
		break;
	}
	case CAM_RELEASE_DEV: {
		CAM_DBG(CAM_IR_LED, "CAM_RELEASE_DEV");
		if ((ictrl->ir_led_state == CAM_IR_LED_STATE_INIT) ||
			(ictrl->ir_led_state == CAM_IR_LED_STATE_START)) {
			CAM_WARN(CAM_IR_LED,
				" Cannot apply Release dev: Prev state:%d",
				ictrl->ir_led_state);
		}

		if (ictrl->device_hdl == -1 &&
			ictrl->ir_led_state == CAM_IR_LED_STATE_ACQUIRE) {
			CAM_ERR(CAM_IR_LED,
				" Invalid Handle: device hdl: %d",
				ictrl->device_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_ir_led_release_dev(ictrl);
		if (rc)
			CAM_ERR(CAM_IR_LED,
				" Failed in destroying the device Handle rc= %d",
				rc);
		ictrl->ir_led_state = CAM_IR_LED_STATE_INIT;
		break;
	}
	case CAM_QUERY_CAP: {
		struct cam_ir_led_query_cap_info ir_led_cap = {0};

		CAM_DBG(CAM_IR_LED, "CAM_QUERY_CAP");
		ir_led_cap.slot_info = ictrl->soc_info.index;

		if (copy_to_user(u64_to_user_ptr(cmd->handle), &ir_led_cap,
			sizeof(struct cam_ir_led_query_cap_info))) {
			CAM_ERR(CAM_IR_LED, " Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
		break;
	}
	case CAM_START_DEV: {
		CAM_DBG(CAM_IR_LED, "CAM_START_DEV");
		if ((ictrl->ir_led_state == CAM_IR_LED_STATE_INIT) ||
			(ictrl->ir_led_state == CAM_IR_LED_STATE_START)) {
			CAM_ERR(CAM_IR_LED,
				"Cannot apply Start Dev: Prev state: %d",
				ictrl->ir_led_state);
			rc = -EINVAL;
			goto release_mutex;
		}
		ictrl->ir_led_state = CAM_IR_LED_STATE_START;

		break;
	}
	case CAM_STOP_DEV: {
		CAM_DBG(CAM_IR_LED, "CAM_STOP_DEV ENTER");
		if (ictrl->ir_led_state != CAM_IR_LED_STATE_START) {
			CAM_WARN(CAM_IR_LED,
				" Cannot apply Stop dev: Prev state is: %d",
				ictrl->ir_led_state);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_ir_led_stop_dev(ictrl);
		if (rc) {
			CAM_ERR(CAM_IR_LED, "Failed STOP_DEV: rc=%d\n", rc);
			goto release_mutex;
		}
		ictrl->ir_led_state = CAM_IR_LED_STATE_ACQUIRE;
		break;
	}
	case CAM_CONFIG_DEV: {
		CAM_DBG(CAM_IR_LED, "CAM_CONFIG_DEV");
		rc = cam_ir_led_parser(ictrl, arg);
		if (rc) {
			CAM_ERR(CAM_IR_LED, "Failed CONFIG_DEV: rc=%d\n", rc);
			goto release_mutex;
		}
		break;
	}
	default:
		CAM_ERR(CAM_IR_LED, "Invalid Opcode: %d", cmd->op_code);
		rc = -EINVAL;
	}

release_mutex:
	mutex_unlock(&(ictrl->ir_led_mutex));
	return rc;
}

static const struct of_device_id cam_ir_led_dt_match[] = {
	{.compatible = "qcom,camera-ir-led", .data = NULL},
	{}
};

static long cam_ir_led_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int rc = 0;
	struct cam_ir_led_ctrl *ictrl = NULL;
	struct cam_ir_led_private_soc *soc_private = NULL;

	CAM_DBG(CAM_IR_LED, "Enter");

	ictrl = v4l2_get_subdevdata(sd);
	soc_private = ictrl->soc_info.soc_private;

	switch (cmd) {
	case VIDIOC_CAM_CONTROL: {
		rc = cam_ir_led_driver_cmd(ictrl, arg,
			soc_private);
		break;
	}
	default:
		CAM_ERR(CAM_IR_LED, " Invalid ioctl cmd type");
		rc = -EINVAL;
		break;
	}

	CAM_DBG(CAM_IR_LED, "Exit");
	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_ir_led_subdev_do_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	struct cam_control cmd_data;
	int32_t rc = 0;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_IR_LED,
			" Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL: {
		rc = cam_ir_led_subdev_ioctl(sd, cmd, &cmd_data);
		if (rc)
			CAM_ERR(CAM_IR_LED, "cam_ir_led_ioctl failed");
		break;
	}
	default:
		CAM_ERR(CAM_IR_LED, " Invalid compat ioctl cmd_type:%d",
			cmd);
		rc = -EINVAL;
	}

	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_IR_LED,
				" Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}

	return rc;
}
#endif

static int cam_ir_led_platform_remove(struct platform_device *pdev)
{
	struct cam_ir_led_ctrl *ictrl;

	ictrl = platform_get_drvdata(pdev);
	if (!ictrl) {
		CAM_ERR(CAM_IR_LED, " Ir_led device is NULL");
		return 0;
	}

	devm_kfree(&pdev->dev, ictrl);

	return 0;
}

static int cam_ir_led_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_ir_led_ctrl *ir_led_ctrl =
		v4l2_get_subdevdata(sd);

	if (!ir_led_ctrl) {
		CAM_ERR(CAM_IR_LED, " Ir_led ctrl ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&ir_led_ctrl->ir_led_mutex);
	cam_ir_led_shutdown(ir_led_ctrl);
	mutex_unlock(&ir_led_ctrl->ir_led_mutex);

	return 0;
}

static struct v4l2_subdev_core_ops cam_ir_led_subdev_core_ops = {
	.ioctl = cam_ir_led_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_ir_led_subdev_do_ioctl
#endif
};

static struct v4l2_subdev_ops cam_ir_led_subdev_ops = {
	.core = &cam_ir_led_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops cam_ir_led_internal_ops = {
	.close = cam_ir_led_subdev_close,
};

static int32_t cam_ir_led_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct cam_ir_led_ctrl *ir_led_ctrl = NULL;

	CAM_ERR(CAM_IR_LED, "DBG:Enter");
	if (!pdev->dev.of_node) {
		CAM_ERR(CAM_IR_LED, "of_node NULL");
		return -EINVAL;
	}

	ir_led_ctrl = kzalloc(sizeof(struct cam_ir_led_ctrl), GFP_KERNEL);
	if (!ir_led_ctrl)
		return -ENOMEM;

	ir_led_ctrl->pdev = pdev;
	ir_led_ctrl->soc_info.pdev = pdev;
	ir_led_ctrl->soc_info.dev = &pdev->dev;
	ir_led_ctrl->soc_info.dev_name = pdev->name;

	rc = cam_ir_led_get_dt_data(ir_led_ctrl, &ir_led_ctrl->soc_info);
	if (rc) {
		CAM_ERR(CAM_IR_LED, "cam_ir_led_get_dt_data failed rc=%d", rc);
		if (ir_led_ctrl->soc_info.soc_private != NULL) {
			kfree(ir_led_ctrl->soc_info.soc_private);
			ir_led_ctrl->soc_info.soc_private = NULL;
		}
		kfree(ir_led_ctrl);
		ir_led_ctrl = NULL;
		return -EINVAL;
	}

	ir_led_ctrl->v4l2_dev_str.internal_ops =
		&cam_ir_led_internal_ops;
	ir_led_ctrl->v4l2_dev_str.ops = &cam_ir_led_subdev_ops;
	ir_led_ctrl->v4l2_dev_str.name = CAMX_IR_LED_DEV_NAME;
	ir_led_ctrl->v4l2_dev_str.sd_flags =
		V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	ir_led_ctrl->v4l2_dev_str.ent_function = CAM_IRLED_DEVICE_TYPE;
	ir_led_ctrl->v4l2_dev_str.token = ir_led_ctrl;

	rc = cam_register_subdev(&(ir_led_ctrl->v4l2_dev_str));
	if (rc) {
		CAM_ERR(CAM_IR_LED, "Fail to create subdev with %d", rc);
		goto free_resource;
	}
	ir_led_ctrl->device_hdl = -1;

	platform_set_drvdata(pdev, ir_led_ctrl);
	v4l2_set_subdevdata(&ir_led_ctrl->v4l2_dev_str.sd, ir_led_ctrl);

	mutex_init(&(ir_led_ctrl->ir_led_mutex));

	ir_led_ctrl->ir_led_state = CAM_IR_LED_STATE_INIT;
	CAM_ERR(CAM_IR_LED, "DBG:Probe success");
	return rc;
free_resource:
	kfree(ir_led_ctrl);
	return rc;
}

MODULE_DEVICE_TABLE(of, cam_ir_led_dt_match);

static struct platform_driver cam_ir_led_platform_driver = {
	.probe = cam_ir_led_platform_probe,
	.remove = cam_ir_led_platform_remove,
	.driver = {
		.name = "CAM-IR-LED-DRIVER",
		.owner = THIS_MODULE,
		.of_match_table = cam_ir_led_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_ir_led_init_module(void)
{
	int32_t rc = 0;

	rc = platform_driver_register(&cam_ir_led_platform_driver);
	if (rc)
		CAM_ERR(CAM_IR_LED, "platform probe for ir_led failed");

	return rc;
}

static void __exit cam_ir_led_exit_module(void)
{
	platform_driver_unregister(&cam_ir_led_platform_driver);
}

module_init(cam_ir_led_init_module);
module_exit(cam_ir_led_exit_module);
MODULE_DESCRIPTION("CAM IR_LED");
MODULE_LICENSE("GPL v2");
