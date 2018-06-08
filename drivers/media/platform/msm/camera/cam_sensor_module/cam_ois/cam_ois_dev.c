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
 */

#include "cam_ois_dev.h"
#include "cam_req_mgr_dev.h"
#include "cam_ois_soc.h"
#include "cam_ois_core.h"
#include "cam_debug_util.h"

static long cam_ois_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int                       rc     = 0;
	struct cam_ois_ctrl_t *o_ctrl = v4l2_get_subdevdata(sd);

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_ois_driver_cmd(o_ctrl, arg);
		break;
	default:
		rc = -ENOIOCTLCMD;
		break;
	}

	return rc;
}

static int cam_ois_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_ois_ctrl_t *o_ctrl =
		v4l2_get_subdevdata(sd);

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "o_ctrl ptr is NULL");
			return -EINVAL;
	}

	mutex_lock(&(o_ctrl->ois_mutex));
	cam_ois_shutdown(o_ctrl);
	mutex_unlock(&(o_ctrl->ois_mutex));

	return 0;
}

static int32_t cam_ois_update_i2c_info(struct cam_ois_ctrl_t *o_ctrl,
	struct cam_ois_i2c_info_t *i2c_info)
{
	struct cam_sensor_cci_client        *cci_client = NULL;

	if (o_ctrl->io_master_info.master_type == CCI_MASTER) {
		cci_client = o_ctrl->io_master_info.cci_client;
		if (!cci_client) {
			CAM_ERR(CAM_OIS, "failed: cci_client %pK",
				cci_client);
			return -EINVAL;
		}
		cci_client->cci_i2c_master = o_ctrl->cci_i2c_master;
		cci_client->sid = (i2c_info->slave_addr) >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->i2c_freq_mode = i2c_info->i2c_freq_mode;
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long cam_ois_init_subdev_do_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	struct cam_control cmd_data;
	int32_t rc = 0;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_OIS,
			"Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_ois_subdev_ioctl(sd, cmd, &cmd_data);
		if (rc) {
			CAM_ERR(CAM_OIS,
				"Failed in ois suddev handling rc %d",
				rc);
			return rc;
		}
		break;
	default:
		CAM_ERR(CAM_OIS, "Invalid compat ioctl: %d", cmd);
		rc = -EINVAL;
	}

	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_OIS,
				"Failed to copy from user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}
	return rc;
}
#endif

static const struct v4l2_subdev_internal_ops cam_ois_internal_ops = {
	.close = cam_ois_subdev_close,
};

static struct v4l2_subdev_core_ops cam_ois_subdev_core_ops = {
	.ioctl = cam_ois_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_ois_init_subdev_do_ioctl,
#endif
};

static struct v4l2_subdev_ops cam_ois_subdev_ops = {
	.core = &cam_ois_subdev_core_ops,
};

static int cam_ois_init_subdev_param(struct cam_ois_ctrl_t *o_ctrl)
{
	int rc = 0;

	o_ctrl->v4l2_dev_str.internal_ops = &cam_ois_internal_ops;
	o_ctrl->v4l2_dev_str.ops = &cam_ois_subdev_ops;
	strlcpy(o_ctrl->device_name, CAM_OIS_NAME,
		sizeof(o_ctrl->device_name));
	o_ctrl->v4l2_dev_str.name = o_ctrl->device_name;
	o_ctrl->v4l2_dev_str.sd_flags =
		(V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	o_ctrl->v4l2_dev_str.ent_function = CAM_OIS_DEVICE_TYPE;
	o_ctrl->v4l2_dev_str.token = o_ctrl;

	rc = cam_register_subdev(&(o_ctrl->v4l2_dev_str));
	if (rc)
		CAM_ERR(CAM_OIS, "fail to create subdev");

	return rc;
}

static int cam_ois_i2c_driver_probe(struct i2c_client *client,
	 const struct i2c_device_id *id)
{
	int                          rc = 0;
	struct cam_ois_ctrl_t       *o_ctrl = NULL;
	struct cam_ois_soc_private  *soc_private = NULL;

	if (client == NULL || id == NULL) {
		CAM_ERR(CAM_OIS, "Invalid Args client: %pK id: %pK",
			client, id);
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CAM_ERR(CAM_OIS, "i2c_check_functionality failed");
		goto probe_failure;
	}

	o_ctrl = kzalloc(sizeof(*o_ctrl), GFP_KERNEL);
	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "kzalloc failed");
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, o_ctrl);

	o_ctrl->soc_info.dev = &client->dev;
	o_ctrl->soc_info.dev_name = client->name;
	o_ctrl->ois_device_type = MSM_CAMERA_I2C_DEVICE;
	o_ctrl->io_master_info.master_type = I2C_MASTER;
	o_ctrl->io_master_info.client = client;

	soc_private = kzalloc(sizeof(struct cam_ois_soc_private),
		GFP_KERNEL);
	if (!soc_private) {
		rc = -ENOMEM;
		goto octrl_free;
	}

	o_ctrl->soc_info.soc_private = soc_private;
	rc = cam_ois_driver_soc_init(o_ctrl);
	if (rc) {
		CAM_ERR(CAM_OIS, "failed: cam_sensor_parse_dt rc %d", rc);
		goto soc_free;
	}

	rc = cam_ois_init_subdev_param(o_ctrl);
	if (rc)
		goto soc_free;

	o_ctrl->cam_ois_state = CAM_OIS_INIT;

	return rc;

soc_free:
	kfree(soc_private);
octrl_free:
	kfree(o_ctrl);
probe_failure:
	return rc;
}

static int cam_ois_i2c_driver_remove(struct i2c_client *client)
{
	int                             i;
	struct cam_ois_ctrl_t          *o_ctrl = i2c_get_clientdata(client);
	struct cam_hw_soc_info         *soc_info;
	struct cam_ois_soc_private     *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "ois device is NULL");
		return -EINVAL;
	}

	soc_info = &o_ctrl->soc_info;

	for (i = 0; i < soc_info->num_clk; i++)
		devm_clk_put(soc_info->dev, soc_info->clk[i]);

	soc_private =
		(struct cam_ois_soc_private *)soc_info->soc_private;
	power_info = &soc_private->power_info;

	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	kfree(o_ctrl->soc_info.soc_private);
	kfree(o_ctrl);

	return 0;
}

static int32_t cam_ois_platform_driver_probe(
	struct platform_device *pdev)
{
	int32_t                         rc = 0;
	struct cam_ois_ctrl_t          *o_ctrl = NULL;
	struct cam_ois_soc_private     *soc_private = NULL;

	o_ctrl = kzalloc(sizeof(struct cam_ois_ctrl_t), GFP_KERNEL);
	if (!o_ctrl)
		return -ENOMEM;

	o_ctrl->soc_info.pdev = pdev;
	o_ctrl->pdev = pdev;
	o_ctrl->soc_info.dev = &pdev->dev;
	o_ctrl->soc_info.dev_name = pdev->name;

	o_ctrl->ois_device_type = MSM_CAMERA_PLATFORM_DEVICE;

	o_ctrl->io_master_info.master_type = CCI_MASTER;
	o_ctrl->io_master_info.cci_client = kzalloc(
		sizeof(struct cam_sensor_cci_client), GFP_KERNEL);
	if (!o_ctrl->io_master_info.cci_client)
		goto free_o_ctrl;

	soc_private = kzalloc(sizeof(struct cam_ois_soc_private),
		GFP_KERNEL);
	if (!soc_private) {
		rc = -ENOMEM;
		goto free_cci_client;
	}
	o_ctrl->soc_info.soc_private = soc_private;
	soc_private->power_info.dev  = &pdev->dev;

	INIT_LIST_HEAD(&(o_ctrl->i2c_init_data.list_head));
	INIT_LIST_HEAD(&(o_ctrl->i2c_calib_data.list_head));
	INIT_LIST_HEAD(&(o_ctrl->i2c_mode_data.list_head));
	mutex_init(&(o_ctrl->ois_mutex));
	rc = cam_ois_driver_soc_init(o_ctrl);
	if (rc) {
		CAM_ERR(CAM_OIS, "failed: soc init rc %d", rc);
		goto free_soc;
	}

	rc = cam_ois_init_subdev_param(o_ctrl);
	if (rc)
		goto free_soc;

	rc = cam_ois_update_i2c_info(o_ctrl, &soc_private->i2c_info);
	if (rc) {
		CAM_ERR(CAM_OIS, "failed: to update i2c info rc %d", rc);
		goto unreg_subdev;
	}
	o_ctrl->bridge_intf.device_hdl = -1;

	platform_set_drvdata(pdev, o_ctrl);
	v4l2_set_subdevdata(&o_ctrl->v4l2_dev_str.sd, o_ctrl);

	o_ctrl->cam_ois_state = CAM_OIS_INIT;

	return rc;
unreg_subdev:
	cam_unregister_subdev(&(o_ctrl->v4l2_dev_str));
free_soc:
	kfree(soc_private);
free_cci_client:
	kfree(o_ctrl->io_master_info.cci_client);
free_o_ctrl:
	kfree(o_ctrl);
	return rc;
}

static int cam_ois_platform_driver_remove(struct platform_device *pdev)
{
	int                             i;
	struct cam_ois_ctrl_t          *o_ctrl;
	struct cam_ois_soc_private     *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_hw_soc_info         *soc_info;

	o_ctrl = platform_get_drvdata(pdev);
	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "ois device is NULL");
		return -EINVAL;
	}

	soc_info = &o_ctrl->soc_info;
	for (i = 0; i < soc_info->num_clk; i++)
		devm_clk_put(soc_info->dev, soc_info->clk[i]);

	soc_private =
		(struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	kfree(o_ctrl->soc_info.soc_private);
	kfree(o_ctrl->io_master_info.cci_client);
	kfree(o_ctrl);
	return 0;
}

static const struct of_device_id cam_ois_dt_match[] = {
	{ .compatible = "qcom,ois" },
	{ }
};


MODULE_DEVICE_TABLE(of, cam_ois_dt_match);

static struct platform_driver cam_ois_platform_driver = {
	.driver = {
		.name = "qcom,ois",
		.owner = THIS_MODULE,
		.of_match_table = cam_ois_dt_match,
	},
	.probe = cam_ois_platform_driver_probe,
	.remove = cam_ois_platform_driver_remove,
};
static const struct i2c_device_id cam_ois_i2c_id[] = {
	{ "msm_ois", (kernel_ulong_t)NULL},
	{ }
};

static struct i2c_driver cam_ois_i2c_driver = {
	.id_table = cam_ois_i2c_id,
	.probe  = cam_ois_i2c_driver_probe,
	.remove = cam_ois_i2c_driver_remove,
	.driver = {
		.name = "msm_ois",
	},
};

static struct cam_ois_registered_driver_t registered_driver = {
	0, 0};

static int __init cam_ois_driver_init(void)
{
	int rc = 0;

	rc = platform_driver_register(&cam_ois_platform_driver);
	if (rc) {
		CAM_ERR(CAM_OIS, "platform_driver_register failed rc = %d",
			rc);
		return rc;
	}

	registered_driver.platform_driver = 1;

	rc = i2c_add_driver(&cam_ois_i2c_driver);
	if (rc) {
		CAM_ERR(CAM_OIS, "i2c_add_driver failed rc = %d", rc);
		return rc;
	}

	registered_driver.i2c_driver = 1;
	return rc;
}

static void __exit cam_ois_driver_exit(void)
{
	if (registered_driver.platform_driver)
		platform_driver_unregister(&cam_ois_platform_driver);

	if (registered_driver.i2c_driver)
		i2c_del_driver(&cam_ois_i2c_driver);
}

module_init(cam_ois_driver_init);
module_exit(cam_ois_driver_exit);
MODULE_DESCRIPTION("CAM OIS driver");
MODULE_LICENSE("GPL v2");
