/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/pwm.h>
#include <linux/delay.h>
#include "msm_ir_led.h"
#include "msm_camera_dt_util.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

DEFINE_MSM_MUTEX(msm_ir_led_mutex);

static struct v4l2_file_operations msm_ir_led_v4l2_subdev_fops;

static const struct of_device_id msm_ir_led_dt_match[] = {
	{.compatible = "qcom,ir-led", .data = NULL},
	{}
};

static struct msm_ir_led_table msm_default_ir_led_table;

static struct msm_ir_led_table *ir_led_table[] = {
	&msm_default_ir_led_table,
};

static int32_t msm_ir_led_get_subdev_id(
	struct msm_ir_led_ctrl_t *ir_led_ctrl, void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;

	if (!subdev_id) {
		pr_err("subdevice ID is not valid\n");
		return -EINVAL;
	}
	if (ir_led_ctrl->ir_led_device_type != MSM_CAMERA_PLATFORM_DEVICE) {
		pr_err("device type is not matching\n");
		return -EINVAL;
	}

	*subdev_id = ir_led_ctrl->pdev->id;

	CDBG("subdev_id %d\n", *subdev_id);
	return 0;
}

static int32_t msm_ir_led_init(
	struct msm_ir_led_ctrl_t *ir_led_ctrl,
	struct msm_ir_led_cfg_data_t *ir_led_data)
{
	int32_t rc = 0;

	rc = ir_led_ctrl->func_tbl->camera_ir_led_off(ir_led_ctrl, ir_led_data);

	return rc;
}

static int32_t msm_ir_led_release(
	struct msm_ir_led_ctrl_t *ir_led_ctrl,
		struct msm_ir_led_cfg_data_t *ir_led_data)
{
	int32_t rc = -EFAULT;

	if (ir_led_ctrl->ir_led_state == MSM_CAMERA_IR_LED_RELEASE) {
		pr_err("Invalid ir_led state = %d\n",
			ir_led_ctrl->ir_led_state);
		return rc;
	}

	rc = ir_led_ctrl->func_tbl->camera_ir_led_off(ir_led_ctrl, ir_led_data);
	if (rc < 0) {
		pr_err("camera_ir_led_off failed (%d)\n", rc);
		return rc;
	}
	ir_led_ctrl->ir_led_state = MSM_CAMERA_IR_LED_RELEASE;

	return rc;
}

static int32_t msm_ir_led_off(struct msm_ir_led_ctrl_t *ir_led_ctrl,
	struct msm_ir_led_cfg_data_t *ir_led_data)
{
	int32_t rc = 0;

	CDBG("pwm duty on(ns) %d, pwm period(ns) %d\n",
		ir_led_data->pwm_duty_on_ns, ir_led_data->pwm_period_ns);

	if (ir_led_data->pwm_period_ns <= 0)
		ir_led_data->pwm_period_ns = DEFAULT_PWM_TIME_PERIOD_NS;

	if (ir_led_data->pwm_duty_on_ns != 0)
		ir_led_data->pwm_duty_on_ns = DEFAULT_PWM_DUTY_CYCLE_NS;

	if (ir_led_ctrl->pwm_dev) {
		rc = pwm_config(ir_led_ctrl->pwm_dev,
			ir_led_data->pwm_duty_on_ns,
			ir_led_data->pwm_period_ns);

		if (rc) {
			pr_err("PWM config failed (%d)\n", rc);
			return rc;
		}
		/* workaround to disable pwm_module */
		udelay(50);

		pwm_disable(ir_led_ctrl->pwm_dev);
	} else {
		CDBG("pwm device is null\n");
	}

	return 0;
}

static int32_t msm_ir_led_on(
	struct msm_ir_led_ctrl_t *ir_led_ctrl,
	struct msm_ir_led_cfg_data_t *ir_led_data)
{
	int32_t rc = 0;

	CDBG("pwm duty on(ns) %d, pwm period(ns) %d\n",
		ir_led_data->pwm_duty_on_ns, ir_led_data->pwm_period_ns);

	if (ir_led_ctrl->pwm_dev) {
		rc = pwm_config(ir_led_ctrl->pwm_dev,
			ir_led_data->pwm_duty_on_ns,
			ir_led_data->pwm_period_ns);
		if (rc) {
			pr_err("PWM config failed (%d)\n", rc);
			return rc;
		}

		rc = pwm_enable(ir_led_ctrl->pwm_dev);
		if (rc) {
			pr_err("PWM enable failed(%d)\n", rc);
			return rc;
		}
	} else {
		CDBG("pwm device is null\n");
	}
	return 0;
}

static int32_t msm_ir_led_handle_init(
	struct msm_ir_led_ctrl_t *ir_led_ctrl,
	struct msm_ir_led_cfg_data_t *ir_led_data)
{
	uint32_t i = 0;
	int32_t rc = -EFAULT;
	enum msm_ir_led_driver_type ir_led_driver_type =
		ir_led_ctrl->ir_led_driver_type;

	if (ir_led_ctrl->ir_led_state == MSM_CAMERA_IR_LED_INIT) {
		pr_err("Invalid ir_led state = %d\n",
				ir_led_ctrl->ir_led_state);
		return rc;
	}

	for (i = 0; i < ARRAY_SIZE(ir_led_table); i++) {
		if (ir_led_driver_type == ir_led_table[i]->ir_led_driver_type) {
			ir_led_ctrl->func_tbl = &ir_led_table[i]->func_tbl;
			rc = 0;
			break;
		}
	}

	if (rc < 0) {
		pr_err("failed invalid ir_led_driver_type %d\n",
				ir_led_driver_type);
		return -EINVAL;
	}

	rc = ir_led_ctrl->func_tbl->camera_ir_led_init(
			ir_led_ctrl, ir_led_data);
	if (rc < 0) {
		pr_err("camera_ir_led_init failed (%d)\n", rc);
		return rc;
	}

	ir_led_ctrl->ir_led_state = MSM_CAMERA_IR_LED_INIT;

	CDBG("IR LED STATE intialised Successfully\n");
	return rc;
}

static int32_t msm_ir_led_config(struct msm_ir_led_ctrl_t *ir_led_ctrl,
	void *argp)
{
	int32_t rc = -EINVAL;
	struct msm_ir_led_cfg_data_t *ir_led_data =
		(struct msm_ir_led_cfg_data_t *) argp;

	CDBG("type %d\n", ir_led_data->cfg_type);

	mutex_lock(ir_led_ctrl->ir_led_mutex);

	switch (ir_led_data->cfg_type) {
	case CFG_IR_LED_INIT:
		rc = msm_ir_led_handle_init(ir_led_ctrl, ir_led_data);
		break;
	case CFG_IR_LED_RELEASE:
		if (ir_led_ctrl->ir_led_state == MSM_CAMERA_IR_LED_INIT)
			rc = ir_led_ctrl->func_tbl->camera_ir_led_release(
				ir_led_ctrl, ir_led_data);
		break;
	case CFG_IR_LED_OFF:
		if (ir_led_ctrl->ir_led_state == MSM_CAMERA_IR_LED_INIT)
			rc = ir_led_ctrl->func_tbl->camera_ir_led_off(
				ir_led_ctrl, ir_led_data);
		break;
	case CFG_IR_LED_ON:
		if (ir_led_ctrl->ir_led_state == MSM_CAMERA_IR_LED_INIT)
			rc = ir_led_ctrl->func_tbl->camera_ir_led_on(
				ir_led_ctrl, ir_led_data);
		break;
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(ir_led_ctrl->ir_led_mutex);

	CDBG("Exit (%d): type %d\n", rc, ir_led_data->cfg_type);

	return rc;
}

static long msm_ir_led_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct msm_ir_led_ctrl_t *fctrl = NULL;
	void *argp = arg;
	struct msm_ir_led_cfg_data_t ir_led_data = {0};

	if (!sd) {
		pr_err(" v4l2 ir led subdevice is NULL\n");
		return -EINVAL;
	}
	fctrl = v4l2_get_subdevdata(sd);
	if (!fctrl) {
		pr_err("fctrl NULL\n");
		return -EINVAL;
	}
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_ir_led_get_subdev_id(fctrl, argp);
	case VIDIOC_MSM_IR_LED_CFG:
		return msm_ir_led_config(fctrl, argp);
	case MSM_SD_NOTIFY_FREEZE:
		return 0;
	case MSM_SD_SHUTDOWN:
		if (!fctrl->func_tbl) {
			pr_err("No call back funcions\n");
			return -EINVAL;
		} else {
			return fctrl->func_tbl->camera_ir_led_release(fctrl,
							&ir_led_data);
		}
	default:
		pr_err_ratelimited("invalid cmd %d\n", cmd);
		return -ENOIOCTLCMD;
	}
}

static struct v4l2_subdev_core_ops msm_ir_led_subdev_core_ops = {
	.ioctl = msm_ir_led_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_ir_led_subdev_ops = {
	.core = &msm_ir_led_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_ir_led_internal_ops;

static int32_t msm_ir_led_get_dt_data(struct device_node *of_node,
	struct msm_ir_led_ctrl_t *fctrl)
{
	int32_t rc = 0;

	/* Read the sub device */
	rc = of_property_read_u32(of_node, "cell-index", &fctrl->pdev->id);
	if (rc < 0) {
		pr_err("reading cell-index for ir-led node is failed(rc) %d\n",
				rc);
		return rc;
	}

	fctrl->ir_led_driver_type = IR_LED_DRIVER_DEFAULT;
	return rc;
}

#ifdef CONFIG_COMPAT
static long msm_ir_led_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	int32_t rc = 0;
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	struct msm_ir_led_cfg_data_t32 *u32 =
		(struct msm_ir_led_cfg_data_t32 *)arg;
	struct msm_ir_led_cfg_data_t ir_led_data;

	switch (cmd) {
	case VIDIOC_MSM_IR_LED_CFG32:
		cmd = VIDIOC_MSM_IR_LED_CFG;
		break;
	default:
		return msm_ir_led_subdev_ioctl(sd, cmd, arg);
	}

	ir_led_data.cfg_type = u32->cfg_type;
	ir_led_data.pwm_duty_on_ns = u32->pwm_duty_on_ns;
	ir_led_data.pwm_period_ns = u32->pwm_period_ns;

	rc = msm_ir_led_subdev_ioctl(sd, cmd, &ir_led_data);

	return rc;
}

static long msm_ir_led_subdev_fops_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_ir_led_subdev_do_ioctl);
}
#endif

static int32_t msm_ir_led_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_ir_led_ctrl_t *ir_led_ctrl = NULL;

	if (!pdev->dev.of_node) {
		pr_err("IR LED device node is not present in device tree\n");
		return -EINVAL;
	}

	ir_led_ctrl = devm_kzalloc(&pdev->dev, sizeof(struct msm_ir_led_ctrl_t),
				GFP_KERNEL);
	if (!ir_led_ctrl)
		return -ENOMEM;

	ir_led_ctrl->pdev = pdev;

	/* Reading PWM device node */
	ir_led_ctrl->pwm_dev = of_pwm_get(pdev->dev.of_node, NULL);

	if (PTR_ERR(ir_led_ctrl->pwm_dev) == -EPROBE_DEFER) {
		pr_info("Deferring probe...Cannot get PWM device\n");
		return -EPROBE_DEFER;
	}

	if (IS_ERR(ir_led_ctrl->pwm_dev)) {
		rc = PTR_ERR(ir_led_ctrl->pwm_dev);
		CDBG("Cannot get PWM device (%d)\n", rc);
		ir_led_ctrl->pwm_dev = NULL;
	}

	rc = msm_ir_led_get_dt_data(pdev->dev.of_node, ir_led_ctrl);
	if (rc < 0) {
		pr_err("msm_ir_led_get_dt_data failed\n");
		return -EINVAL;
	}

	ir_led_ctrl->ir_led_state = MSM_CAMERA_IR_LED_RELEASE;
	ir_led_ctrl->power_info.dev = &ir_led_ctrl->pdev->dev;
	ir_led_ctrl->ir_led_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	ir_led_ctrl->ir_led_mutex = &msm_ir_led_mutex;

	/* Initialize sub device */
	v4l2_subdev_init(&ir_led_ctrl->msm_sd.sd, &msm_ir_led_subdev_ops);
	v4l2_set_subdevdata(&ir_led_ctrl->msm_sd.sd, ir_led_ctrl);

	ir_led_ctrl->msm_sd.sd.internal_ops = &msm_ir_led_internal_ops;
	ir_led_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(ir_led_ctrl->msm_sd.sd.name,
		ARRAY_SIZE(ir_led_ctrl->msm_sd.sd.name),
		"msm_camera_ir_led");
	media_entity_init(&ir_led_ctrl->msm_sd.sd.entity, 0, NULL, 0);
	ir_led_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	ir_led_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_IR_LED;
	ir_led_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x1;

	rc = msm_sd_register(&ir_led_ctrl->msm_sd);
	if (rc < 0) {
		pr_err("sub dev register failed for ir_led device\n");
		return rc;
	}

	CDBG("ir_led sd name = %s\n",
		ir_led_ctrl->msm_sd.sd.entity.name);
	msm_ir_led_v4l2_subdev_fops = v4l2_subdev_fops;
#ifdef CONFIG_COMPAT
	msm_ir_led_v4l2_subdev_fops.compat_ioctl32 =
		msm_ir_led_subdev_fops_ioctl;
#endif
	ir_led_ctrl->msm_sd.sd.devnode->fops = &msm_ir_led_v4l2_subdev_fops;

	CDBG("probe success\n");
	return rc;
}

MODULE_DEVICE_TABLE(of, msm_ir_led_dt_match);

static struct platform_driver msm_ir_led_platform_driver = {
	.probe = msm_ir_led_platform_probe,
	.driver = {
		.name = "qcom,ir-led",
		.owner = THIS_MODULE,
		.of_match_table = msm_ir_led_dt_match,
	},
};

static int __init msm_ir_led_init_module(void)
{
	int32_t rc = 0;

	rc = platform_driver_register(&msm_ir_led_platform_driver);
	if (!rc)
		return rc;

	pr_err("ir-led driver register failed (%d)\n", rc);

	return rc;
}

static void __exit msm_ir_led_exit_module(void)
{
	platform_driver_unregister(&msm_ir_led_platform_driver);
}

static struct msm_ir_led_table msm_default_ir_led_table = {
	.ir_led_driver_type = IR_LED_DRIVER_DEFAULT,
	.func_tbl = {
		.camera_ir_led_init = msm_ir_led_init,
		.camera_ir_led_release = msm_ir_led_release,
		.camera_ir_led_off = msm_ir_led_off,
		.camera_ir_led_on = msm_ir_led_on,
	},
};

module_init(msm_ir_led_init_module);
module_exit(msm_ir_led_exit_module);
MODULE_DESCRIPTION("MSM IR LED");
MODULE_LICENSE("GPL v2");
