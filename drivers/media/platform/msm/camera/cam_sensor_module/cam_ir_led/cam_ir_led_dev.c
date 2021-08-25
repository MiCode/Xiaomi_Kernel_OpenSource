/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include <linux/pwm.h>
#include "cam_ir_led_dev.h"
#include "cam_ir_led_soc.h"
#include "cam_ir_led_core.h"

static struct cam_ir_led_table cam_pmic_ir_led_table;

static struct cam_ir_led_table *ir_led_table[] = {
	&cam_pmic_ir_led_table,
};

static int32_t cam_pmic_ir_led_init(
	struct cam_ir_led_ctrl *ictrl)
{
	return ictrl->func_tbl->camera_ir_led_off(ictrl);
}

static int32_t cam_pmic_ir_led_release(
	struct cam_ir_led_ctrl *ictrl)
{
	int32_t rc = 0;

	CAM_DBG(CAM_IR_LED, "Enter");
	rc = ictrl->func_tbl->camera_ir_led_off(ictrl);
	if (rc < 0) {
		CAM_ERR(CAM_IR_LED, "camera_ir_led_off failed (%d)", rc);
		return rc;
	}
	return rc;
}

static int32_t cam_pmic_ir_led_off(struct cam_ir_led_ctrl *ictrl)
{
	int32_t rc = 0;

	CAM_DBG(CAM_IR_LED, "Enter");
	if (ictrl->pwm_dev) {
		pwm_disable(ictrl->pwm_dev);
	} else {
		CAM_ERR(CAM_IR_LED, "pwm device is null");
		return -EINVAL;
	}

	rc = gpio_direction_input(
		ictrl->soc_info.gpio_data->cam_gpio_common_tbl[0].gpio);
	if (rc)
		CAM_ERR(CAM_IR_LED, "gpio operation failed(%d)", rc);

	return rc;
}

static int32_t cam_pmic_ir_led_on(
	struct cam_ir_led_ctrl *ictrl,
	struct cam_ir_led_set_on_off *ir_led_data)
{
	int rc;

	if (ictrl->pwm_dev) {
		rc = pwm_config(ictrl->pwm_dev,
			ir_led_data->pwm_duty_on_ns,
			ir_led_data->pwm_period_ns);
		if (rc) {
			CAM_ERR(CAM_IR_LED, "PWM config failed (%d)", rc);
			return rc;
		}

		rc = pwm_enable(ictrl->pwm_dev);
		CAM_DBG(CAM_IR_LED, "enabled=%d, period=%llu, duty_cycle=%llu",
			ictrl->pwm_dev->state.enabled,
			ictrl->pwm_dev->state.period,
			ictrl->pwm_dev->state.duty_cycle);
		if (rc) {
			CAM_ERR(CAM_IR_LED, "PWM enable failed(%d)", rc);
			return rc;
		}
		rc = gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[0].gpio,
			1);
		if (rc) {
			CAM_ERR(CAM_IR_LED, "gpio operation failed(%d)", rc);
			return rc;
		}
		rc = gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[1].gpio,
			1);
		if (rc) {
			CAM_ERR(CAM_IR_LED, "gpio operation failed(%d)", rc);
			return rc;
		}
	} else {
		CAM_ERR(CAM_IR_LED, "pwm device is null");
	}

	return 0;
}

static int32_t cam_ir_led_handle_init(
	struct cam_ir_led_ctrl *ictrl)
{
	uint32_t i = 0;
	int32_t rc = -EFAULT;
	enum cam_ir_led_driver_type ir_led_driver_type =
					ictrl->ir_led_driver_type;

	CAM_DBG(CAM_IR_LED, "IRLED HW type=%d", ir_led_driver_type);
	for (i = 0; i < ARRAY_SIZE(ir_led_table); i++) {
		if (ir_led_driver_type == ir_led_table[i]->ir_led_driver_type) {
			ictrl->func_tbl = &ir_led_table[i]->func_tbl;
			rc = 0;
			break;
		}
	}

	if (rc < 0) {
		CAM_ERR(CAM_IR_LED, "failed invalid ir_led_driver_type %d",
				ir_led_driver_type);
		return -EINVAL;
	}

	rc = ictrl->func_tbl->camera_ir_led_init(ictrl);
	if (rc < 0)
		CAM_ERR(CAM_IR_LED, "camera_ir_led_init failed (%d)", rc);

	return rc;
}
static int32_t cam_ir_led_config(struct cam_ir_led_ctrl *ictrl,
	void *arg)
{
	int rc = 0;
	uint32_t  *cmd_buf =  NULL;
	uintptr_t generic_ptr;
	uint32_t  *offset = NULL;
	size_t len_of_buffer;
	struct cam_control *ioctl_ctrl = NULL;
	struct cam_packet *csl_packet = NULL;
	struct cam_config_dev_cmd config;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	struct cam_ir_led_set_on_off *cam_ir_led_info = NULL;

	if (!ictrl || !arg) {
		CAM_ERR(CAM_IR_LED, "ictrl/arg is NULL");
		return -EINVAL;
	}
	/* getting CSL Packet */
	ioctl_ctrl = (struct cam_control *)arg;

	if (copy_from_user((&config), u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(config))) {
		CAM_ERR(CAM_IR_LED, "Copy cmd handle from user failed");
		rc = -EFAULT;
		return rc;
	}

	rc = cam_mem_get_cpu_buf(config.packet_handle,
		(uintptr_t *)&generic_ptr, &len_of_buffer);
	if (rc) {
		CAM_ERR(CAM_IR_LED, "Failed in getting the buffer : %d", rc);
		return rc;
	}

	if (config.offset > len_of_buffer) {
		CAM_ERR(CAM_IR_LED,
			"offset is out of bounds: offset: %lld len: %zu",
			config.offset, len_of_buffer);
		return -EINVAL;
	}

	/* Add offset to the ir_led csl header */
	csl_packet = (struct cam_packet *)(uintptr_t)(generic_ptr +
			config.offset);

	offset = (uint32_t *)((uint8_t *)&csl_packet->payload +
		csl_packet->cmd_buf_offset);
	cmd_desc = (struct cam_cmd_buf_desc *)(offset);
	rc = cam_mem_get_cpu_buf(cmd_desc->mem_handle,
		(uintptr_t *)&generic_ptr, &len_of_buffer);
	if (rc < 0) {
		CAM_ERR(CAM_IR_LED, "Failed to get the command Buffer");
		return -EINVAL;
	}

	cmd_buf = (uint32_t *)((uint8_t *)generic_ptr +
		cmd_desc->offset);
	cam_ir_led_info = (struct cam_ir_led_set_on_off *)cmd_buf;

	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_IR_LED_PACKET_OPCODE_ON:
		rc = ictrl->func_tbl->camera_ir_led_on(
				ictrl, cam_ir_led_info);
		if (rc < 0) {
			CAM_ERR(CAM_IR_LED, "Fail to turn irled ON rc=%d", rc);
			return rc;
		}
		ictrl->ir_led_state = CAM_IR_LED_STATE_ON;
		break;
	case CAM_IR_LED_PACKET_OPCODE_OFF:
		if (ictrl->ir_led_state != CAM_IR_LED_STATE_ON) {
			CAM_DBG(CAM_IR_LED,
				"IRLED_OFF NA, Already OFF, state:%d",
				ictrl->ir_led_state);
			return 0;
		}
		rc = ictrl->func_tbl->camera_ir_led_off(ictrl);
		if (rc < 0) {
			CAM_ERR(CAM_IR_LED, "Fail to turn irled OFF rc=%d", rc);
			return rc;
		}
		ictrl->ir_led_state = CAM_IR_LED_STATE_OFF;
		break;
	case CAM_PKT_NOP_OPCODE:
		CAM_DBG(CAM_IR_LED, "CAM_PKT_NOP_OPCODE");
		break;
	default:
		CAM_ERR(CAM_IR_LED, "Invalid Opcode : %d",
			(csl_packet->header.op_code & 0xFFFFFF));
		return -EINVAL;
	}

	return rc;
}

static int32_t cam_ir_led_driver_cmd(struct cam_ir_led_ctrl *ictrl,
		void *arg, struct cam_ir_led_private_soc *soc_private)
{
	int rc = 0;
	struct cam_control *cmd = (struct cam_control *)arg;
	struct cam_sensor_acquire_dev ir_led_acq_dev;
	struct cam_create_dev_hdl dev_hdl;
	struct cam_ir_led_query_cap_info ir_led_cap = {0};

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
	CAM_DBG(CAM_IR_LED, "cmd->op_code %d", cmd->op_code);
	switch (cmd->op_code) {
	case CAM_ACQUIRE_DEV:
		if (ictrl->ir_led_state != CAM_IR_LED_STATE_INIT) {
			CAM_ERR(CAM_IR_LED,
				"Cannot apply Acquire dev: Prev state: %d",
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
		rc = cam_ir_led_handle_init(ictrl);
		ictrl->ir_led_state = CAM_IR_LED_STATE_ACQUIRE;
		break;
	case CAM_RELEASE_DEV:
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
	case CAM_QUERY_CAP:
		ir_led_cap.slot_info = ictrl->soc_info.index;

		if (copy_to_user(u64_to_user_ptr(cmd->handle), &ir_led_cap,
			sizeof(struct cam_ir_led_query_cap_info))) {
			CAM_ERR(CAM_IR_LED, " Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
		break;
	case CAM_START_DEV:
		if (ictrl->ir_led_state != CAM_IR_LED_STATE_ACQUIRE) {
			CAM_ERR(CAM_IR_LED,
				"Cannot apply Start Dev: Prev state: %d",
				ictrl->ir_led_state);
			rc = -EINVAL;
			goto release_mutex;
		}
		ictrl->ir_led_state = CAM_IR_LED_STATE_START;
		break;
	case CAM_STOP_DEV:
		rc = cam_ir_led_stop_dev(ictrl);
		if (rc) {
			CAM_ERR(CAM_IR_LED, "Failed STOP_DEV: rc=%d", rc);
			goto release_mutex;
		}
		ictrl->ir_led_state = CAM_IR_LED_STATE_ACQUIRE;
		break;
	case CAM_CONFIG_DEV:
		if ((ictrl->ir_led_state == CAM_IR_LED_STATE_INIT) ||
			(ictrl->ir_led_state == CAM_IR_LED_STATE_ACQUIRE)) {
			CAM_ERR(CAM_IR_LED,
				"Cannot apply Config Dev: Prev state: %d",
				ictrl->ir_led_state);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_ir_led_config(ictrl, arg);
		if (rc) {
			CAM_ERR(CAM_IR_LED, "Failed CONFIG_DEV: rc=%d", rc);
			goto release_mutex;
		}
		break;
	case CAM_FLUSH_REQ:
		rc = cam_ir_led_stop_dev(ictrl);
		if (rc) {
			CAM_ERR(CAM_IR_LED, "Failed FLUSH_REQ: rc=%d", rc);
			goto release_mutex;
		}
		ictrl->ir_led_state = CAM_IR_LED_STATE_ACQUIRE;
		break;
	default:
		CAM_ERR(CAM_IR_LED, "Invalid Opcode:%d", cmd->op_code);
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
	case VIDIOC_CAM_CONTROL:
		rc = cam_ir_led_driver_cmd(ictrl, arg,
			soc_private);
		break;
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
	case VIDIOC_CAM_CONTROL:
		rc = cam_ir_led_subdev_ioctl(sd, cmd, &cmd_data);
		if (rc)
			CAM_ERR(CAM_IR_LED, "cam_ir_led_ioctl failed");
		break;
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

	kfree(ictrl);

	return 0;
}

static int cam_ir_led_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_ir_led_ctrl *ictrl =
		v4l2_get_subdevdata(sd);

	if (!ictrl) {
		CAM_ERR(CAM_IR_LED, " Ir_led ctrl ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&ictrl->ir_led_mutex);
	cam_ir_led_shutdown(ictrl);
	mutex_unlock(&ictrl->ir_led_mutex);

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
	struct cam_ir_led_ctrl *ictrl = NULL;

	CAM_DBG(CAM_IR_LED, "Enter");
	if (!pdev->dev.of_node) {
		CAM_ERR(CAM_IR_LED, "of_node NULL");
		return -EINVAL;
	}

	ictrl = kzalloc(sizeof(struct cam_ir_led_ctrl), GFP_KERNEL);
	if (!ictrl) {
		CAM_ERR(CAM_IR_LED, "kzalloc failed!!");
		return -ENOMEM;
	}

	ictrl->pdev = pdev;
	ictrl->soc_info.pdev = pdev;
	ictrl->soc_info.dev = &pdev->dev;
	ictrl->soc_info.dev_name = pdev->name;

	rc = cam_ir_led_get_dt_data(ictrl, &ictrl->soc_info);
	if (rc) {
		CAM_ERR(CAM_IR_LED, "cam_ir_led_get_dt_data failed rc=%d", rc);
		if (ictrl->soc_info.soc_private != NULL) {
			kfree(ictrl->soc_info.soc_private);
			ictrl->soc_info.soc_private = NULL;
		}
		kfree(ictrl);
		ictrl = NULL;
		return -EINVAL;
	}

	ictrl->v4l2_dev_str.internal_ops =
		&cam_ir_led_internal_ops;
	ictrl->v4l2_dev_str.ops = &cam_ir_led_subdev_ops;
	ictrl->v4l2_dev_str.name = CAMX_IR_LED_DEV_NAME;
	ictrl->v4l2_dev_str.sd_flags =
		V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	ictrl->v4l2_dev_str.ent_function = CAM_IRLED_DEVICE_TYPE;
	ictrl->v4l2_dev_str.token = ictrl;

	rc = cam_register_subdev(&(ictrl->v4l2_dev_str));
	if (rc) {
		CAM_ERR(CAM_IR_LED, "Fail to create subdev with %d", rc);
		kfree(ictrl);
		return rc;
	}

	ictrl->device_hdl = -1;
	platform_set_drvdata(pdev, ictrl);
	v4l2_set_subdevdata(&ictrl->v4l2_dev_str.sd, ictrl);
	mutex_init(&(ictrl->ir_led_mutex));
	ictrl->ir_led_state = CAM_IR_LED_STATE_INIT;
	return rc;
}

static struct cam_ir_led_table cam_pmic_ir_led_table = {
	.ir_led_driver_type = IR_LED_DRIVER_PMIC,
	.func_tbl = {
		.camera_ir_led_init = &cam_pmic_ir_led_init,
		.camera_ir_led_release = &cam_pmic_ir_led_release,
		.camera_ir_led_off = &cam_pmic_ir_led_off,
		.camera_ir_led_on = &cam_pmic_ir_led_on,
	},
};

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

module_platform_driver(cam_ir_led_platform_driver);

MODULE_DESCRIPTION("CAM IR_LED");
MODULE_LICENSE("GPL v2");
