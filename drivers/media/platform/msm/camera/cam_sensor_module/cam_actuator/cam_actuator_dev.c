/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include "cam_actuator_dev.h"
#include "cam_req_mgr_dev.h"
#include "cam_actuator_soc.h"
#include "cam_actuator_core.h"
#include "cam_trace.h"

static long cam_actuator_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int rc = 0;
	struct cam_actuator_ctrl_t *a_ctrl =
		v4l2_get_subdevdata(sd);

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_actuator_driver_cmd(a_ctrl, arg);
		break;
	default:
		pr_err("%s:%d Invalid ioctl cmd\n",
			__func__, __LINE__);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int32_t cam_actuator_driver_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t rc = 0, i = 0;
	struct cam_actuator_ctrl_t *a_ctrl;

	if (client == NULL || id == NULL) {
		pr_err("%s:%d: :Error: Invalid Args client: %pK id: %pK\n",
			__func__, __LINE__, client, id);
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s %s :Error: i2c_check_functionality failed\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	/* Create sensor control structure */
	a_ctrl = kzalloc(sizeof(*a_ctrl), GFP_KERNEL);
	if (!a_ctrl)
		return -ENOMEM;

	i2c_set_clientdata(client, a_ctrl);

	a_ctrl->i2c_data.per_frame =
		(struct i2c_settings_array *)
		kzalloc(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (a_ctrl->i2c_data.per_frame == NULL) {
		rc = -ENOMEM;
		goto free_ctrl;
	}

	INIT_LIST_HEAD(&(a_ctrl->i2c_data.init_settings.list_head));

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++)
		INIT_LIST_HEAD(&(a_ctrl->i2c_data.per_frame[i].list_head));

	/* Initialize sensor device type */
	a_ctrl->io_master_info.master_type = I2C_MASTER;

	rc = cam_actuator_parse_dt(a_ctrl, &client->dev);
	if (rc < 0) {
		pr_err("failed: cam_sensor_parse_dt rc %d", rc);
		goto free_mem;
	}

	return rc;
free_mem:
	kfree(a_ctrl->i2c_data.per_frame);
free_ctrl:
	kfree(a_ctrl);
	return rc;
}

static int32_t cam_actuator_platform_remove(struct platform_device *pdev)
{
	struct cam_actuator_ctrl_t  *a_ctrl;
	int32_t rc = 0;

	a_ctrl = platform_get_drvdata(pdev);
	if (!a_ctrl) {
		pr_err("%s: Actuator device is NULL\n", __func__);
		return 0;
	}

	kfree(a_ctrl->io_master_info.cci_client);
	a_ctrl->io_master_info.cci_client = NULL;
	kfree(a_ctrl->i2c_data.per_frame);
	a_ctrl->i2c_data.per_frame = NULL;
	devm_kfree(&pdev->dev, a_ctrl);

	return rc;
}

static int32_t cam_actuator_driver_i2c_remove(struct i2c_client *client)
{
	struct cam_actuator_ctrl_t  *a_ctrl = i2c_get_clientdata(client);
	int32_t rc = 0;

	/* Handle I2C Devices */
	if (!a_ctrl) {
		pr_err("%s: Actuator device is NULL\n", __func__);
		return -EINVAL;
	}
	/*Free Allocated Mem */
	kfree(a_ctrl->i2c_data.per_frame);
	a_ctrl->i2c_data.per_frame = NULL;
	kfree(a_ctrl);
	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_actuator_init_subdev_do_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	struct cam_control cmd_data;
	int32_t rc = 0;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		pr_err("Failed to copy from user_ptr=%pK size=%zu\n",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		cmd = VIDIOC_CAM_CONTROL;
		rc = cam_actuator_subdev_ioctl(sd, cmd, &cmd_data);
		if (rc < 0) {
			pr_err("%s:%d Failed in actuator suddev handling",
				__func__, __LINE__);
			return rc;
		}
		break;
	default:
		pr_err("%s:%d Invalid compat ioctl: %d\n",
			__func__, __LINE__, cmd);
		rc = -EINVAL;
	}

	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			pr_err("Failed to copy to user_ptr=%pK size=%zu\n",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}
	return rc;
}

#endif

static struct v4l2_subdev_core_ops cam_actuator_subdev_core_ops = {
	.ioctl = cam_actuator_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_actuator_init_subdev_do_ioctl,
#endif
};

static struct v4l2_subdev_ops cam_actuator_subdev_ops = {
	.core = &cam_actuator_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops cam_actuator_internal_ops;

static const struct of_device_id cam_actuator_driver_dt_match[] = {
	{.compatible = "qcom,actuator"},
	{}
};

static int32_t cam_actuator_driver_platform_probe(
	struct platform_device *pdev)
{
	int32_t rc = 0, i = 0;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;

	/* Create sensor control structure */
	a_ctrl = devm_kzalloc(&pdev->dev,
		sizeof(struct cam_actuator_ctrl_t), GFP_KERNEL);
	if (!a_ctrl)
		return -ENOMEM;

	/*fill in platform device*/
	a_ctrl->v4l2_dev_str.pdev = pdev;
	a_ctrl->soc_info.pdev = pdev;

	a_ctrl->io_master_info.master_type = CCI_MASTER;

	a_ctrl->io_master_info.cci_client = kzalloc(sizeof(
		struct cam_sensor_cci_client), GFP_KERNEL);
	if (!(a_ctrl->io_master_info.cci_client))
		return -ENOMEM;

	a_ctrl->i2c_data.per_frame =
		(struct i2c_settings_array *)
		kzalloc(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (a_ctrl->i2c_data.per_frame == NULL)
		return -ENOMEM;

	INIT_LIST_HEAD(&(a_ctrl->i2c_data.init_settings.list_head));

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++)
		INIT_LIST_HEAD(&(a_ctrl->i2c_data.per_frame[i].list_head));

	rc = cam_actuator_parse_dt(a_ctrl, &(pdev->dev));
	if (rc < 0) {
		pr_err("%s:%d :Error: Paring actuator dt failed rc %d",
			__func__, __LINE__, rc);
		goto free_ctrl;
	}

	/* Fill platform device id*/
	pdev->id = a_ctrl->id;

	a_ctrl->v4l2_dev_str.internal_ops =
		&cam_actuator_internal_ops;
	a_ctrl->v4l2_dev_str.ops =
		&cam_actuator_subdev_ops;
	strlcpy(a_ctrl->device_name, CAMX_ACTUATOR_DEV_NAME,
		sizeof(a_ctrl->device_name));
	a_ctrl->v4l2_dev_str.name =
		a_ctrl->device_name;
	a_ctrl->v4l2_dev_str.sd_flags =
		(V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	a_ctrl->v4l2_dev_str.ent_function =
		CAM_ACTUATOR_DEVICE_TYPE;
	a_ctrl->v4l2_dev_str.token = a_ctrl;

	rc = cam_register_subdev(&(a_ctrl->v4l2_dev_str));
	if (rc < 0) {
		pr_err("%s:%d :ERROR: Fail with cam_register_subdev\n",
			__func__, __LINE__);
		goto free_mem;
	}

	rc = cam_soc_util_request_platform_resource(&a_ctrl->soc_info,
			NULL, NULL);
	if (rc < 0) {
		pr_err("%s:%d :Error: Requesting Platform Resources failed rc %d",
			__func__, __LINE__, rc);
		goto free_ctrl;
	}

	a_ctrl->bridge_intf.device_hdl = -1;
	a_ctrl->bridge_intf.ops.get_dev_info =
		cam_actuator_publish_dev_info;
	a_ctrl->bridge_intf.ops.link_setup =
		cam_actuator_establish_link;
	a_ctrl->bridge_intf.ops.apply_req =
		cam_actuator_apply_request;

	platform_set_drvdata(pdev, a_ctrl);
	v4l2_set_subdevdata(&a_ctrl->v4l2_dev_str.sd, a_ctrl);

	return rc;
free_mem:
	kfree(a_ctrl->i2c_data.per_frame);
free_ctrl:
	devm_kfree(&pdev->dev, a_ctrl);
	return rc;
}

MODULE_DEVICE_TABLE(of, cam_actuator_driver_dt_match);

static struct platform_driver cam_actuator_platform_driver = {
	.probe = cam_actuator_driver_platform_probe,
	.driver = {
		.name = "qcom,actuator",
		.owner = THIS_MODULE,
		.of_match_table = cam_actuator_driver_dt_match,
	},
	.remove = cam_actuator_platform_remove,
};

static const struct i2c_device_id i2c_id[] = {
	{ACTUATOR_DRIVER_I2C, (kernel_ulong_t)NULL},
	{ }
};

static struct i2c_driver cam_actuator_driver_i2c = {
	.id_table = i2c_id,
	.probe  = cam_actuator_driver_i2c_probe,
	.remove = cam_actuator_driver_i2c_remove,
	.driver = {
		.name = ACTUATOR_DRIVER_I2C,
	},
};

static int __init cam_actuator_driver_init(void)
{
	int32_t rc = 0;

	rc = platform_driver_register(&cam_actuator_platform_driver);
	if (rc < 0) {
		pr_err("%s platform_driver_register failed rc = %d",
			__func__, rc);
		return rc;
	}
	rc = i2c_add_driver(&cam_actuator_driver_i2c);
	if (rc)
		pr_err("%s:%d :Error: i2c_add_driver failed rc = %d",
			__func__, __LINE__, rc);

	return rc;
}

static void __exit cam_actuator_driver_exit(void)
{
	platform_driver_unregister(&cam_actuator_platform_driver);
	i2c_del_driver(&cam_actuator_driver_i2c);
}

module_init(cam_actuator_driver_init);
module_exit(cam_actuator_driver_exit);
MODULE_DESCRIPTION("cam_actuator_driver");
MODULE_LICENSE("GPL v2");
