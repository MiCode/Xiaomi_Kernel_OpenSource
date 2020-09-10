/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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

#include "cam_csiphy_dev.h"
#include "cam_req_mgr_dev.h"
#include "cam_csiphy_soc.h"
#include "cam_csiphy_core.h"
#include <media/cam_sensor.h>

static long cam_csiphy_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct csiphy_device *csiphy_dev = v4l2_get_subdevdata(sd);
	int rc = 0;

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_csiphy_core_cfg(csiphy_dev, arg);
		if (rc != 0) {
			CAM_ERR(CAM_CSIPHY, "in configuring the device");
			return rc;
		}
		break;
	default:
		CAM_ERR(CAM_CSIPHY, "Wrong ioctl : %d", cmd);
		break;
	}

	return rc;
}

static int cam_csiphy_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct csiphy_device *csiphy_dev =
		v4l2_get_subdevdata(sd);

	if (!csiphy_dev) {
		CAM_ERR(CAM_CSIPHY, "csiphy_dev ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&csiphy_dev->mutex);
	cam_csiphy_shutdown(csiphy_dev);
	mutex_unlock(&csiphy_dev->mutex);

	return 0;
}

#ifdef CONFIG_COMPAT
static long cam_csiphy_subdev_compat_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	int32_t rc = 0;
	struct cam_control cmd_data;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_CSIPHY, "Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	/* All the arguments converted to 64 bit here
	 * Passed to the api in core.c
	 */
	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_csiphy_subdev_ioctl(sd, cmd, &cmd_data);
		break;
	default:
		CAM_ERR(CAM_CSIPHY, "Invalid compat ioctl cmd: %d", cmd);
		rc = -EINVAL;
	}

	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_CSIPHY,
				"Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}

	return rc;
}
#endif

static struct v4l2_subdev_core_ops csiphy_subdev_core_ops = {
	.ioctl = cam_csiphy_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_csiphy_subdev_compat_ioctl,
#endif
};

static const struct v4l2_subdev_ops csiphy_subdev_ops = {
	.core = &csiphy_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops csiphy_subdev_intern_ops = {
	.close = cam_csiphy_subdev_close,
};

static int32_t cam_csiphy_platform_probe(struct platform_device *pdev)
{
	struct cam_cpas_register_params cpas_parms;
	struct csiphy_device *new_csiphy_dev;
	int32_t              rc = 0;

	new_csiphy_dev = devm_kzalloc(&pdev->dev,
		sizeof(struct csiphy_device), GFP_KERNEL);
	if (!new_csiphy_dev)
		return -ENOMEM;

	new_csiphy_dev->ctrl_reg = kzalloc(sizeof(struct csiphy_ctrl_t),
		GFP_KERNEL);
	if (!new_csiphy_dev->ctrl_reg) {
		devm_kfree(&pdev->dev, new_csiphy_dev);
		return -ENOMEM;
	}

	mutex_init(&new_csiphy_dev->mutex);
	new_csiphy_dev->v4l2_dev_str.pdev = pdev;

	new_csiphy_dev->soc_info.pdev = pdev;
	new_csiphy_dev->soc_info.dev = &pdev->dev;
	new_csiphy_dev->soc_info.dev_name = pdev->name;
	new_csiphy_dev->ref_count = 0;

	rc = cam_csiphy_parse_dt_info(pdev, new_csiphy_dev);
	if (rc < 0) {
		CAM_ERR(CAM_CSIPHY, "DT parsing failed: %d", rc);
		goto csiphy_no_resource;
	}

	new_csiphy_dev->v4l2_dev_str.internal_ops =
		&csiphy_subdev_intern_ops;
	new_csiphy_dev->v4l2_dev_str.ops =
		&csiphy_subdev_ops;
	strlcpy(new_csiphy_dev->device_name, CAMX_CSIPHY_DEV_NAME,
		sizeof(new_csiphy_dev->device_name));
	new_csiphy_dev->v4l2_dev_str.name =
		new_csiphy_dev->device_name;
	new_csiphy_dev->v4l2_dev_str.sd_flags =
		(V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	new_csiphy_dev->v4l2_dev_str.ent_function =
		CAM_CSIPHY_DEVICE_TYPE;
	new_csiphy_dev->v4l2_dev_str.token =
		new_csiphy_dev;

	rc = cam_register_subdev(&(new_csiphy_dev->v4l2_dev_str));
	if (rc < 0) {
		CAM_ERR(CAM_CSIPHY, "cam_register_subdev Failed rc: %d", rc);
		goto csiphy_no_resource;
	}

	platform_set_drvdata(pdev, &(new_csiphy_dev->v4l2_dev_str.sd));

	new_csiphy_dev->bridge_intf.device_hdl[0] = -1;
	new_csiphy_dev->bridge_intf.device_hdl[1] = -1;
	new_csiphy_dev->bridge_intf.ops.get_dev_info =
		NULL;
	new_csiphy_dev->bridge_intf.ops.link_setup =
		NULL;
	new_csiphy_dev->bridge_intf.ops.apply_req =
		NULL;

	new_csiphy_dev->acquire_count = 0;
	new_csiphy_dev->start_dev_count = 0;
	new_csiphy_dev->is_acquired_dev_combo_mode = 0;

	cpas_parms.cam_cpas_client_cb = NULL;
	cpas_parms.cell_index = new_csiphy_dev->soc_info.index;
	cpas_parms.dev = &pdev->dev;
	cpas_parms.userdata = new_csiphy_dev;

	strlcpy(cpas_parms.identifier, "csiphy", CAM_HW_IDENTIFIER_LENGTH);
	rc = cam_cpas_register_client(&cpas_parms);
	if (rc) {
		CAM_ERR(CAM_CSIPHY, "CPAS registration failed rc: %d", rc);
		goto csiphy_unregister_subdev;
	}
	CAM_DBG(CAM_CSIPHY, "CPAS registration successful handle=%d",
		cpas_parms.client_handle);
	new_csiphy_dev->cpas_handle = cpas_parms.client_handle;

	return rc;

csiphy_unregister_subdev:
	cam_unregister_subdev(&(new_csiphy_dev->v4l2_dev_str));
csiphy_no_resource:
	mutex_destroy(&new_csiphy_dev->mutex);
	kfree(new_csiphy_dev->ctrl_reg);
	devm_kfree(&pdev->dev, new_csiphy_dev);
	return rc;
}


static int32_t cam_csiphy_device_remove(struct platform_device *pdev)
{
	struct v4l2_subdev *subdev =
		platform_get_drvdata(pdev);
	struct csiphy_device *csiphy_dev =
		v4l2_get_subdevdata(subdev);

	CAM_INFO(CAM_CSIPHY, "device remove invoked");
	cam_cpas_unregister_client(csiphy_dev->cpas_handle);
	cam_csiphy_soc_release(csiphy_dev);
	mutex_lock(&csiphy_dev->mutex);
	cam_csiphy_shutdown(csiphy_dev);
	mutex_unlock(&csiphy_dev->mutex);
	cam_unregister_subdev(&(csiphy_dev->v4l2_dev_str));
	kfree(csiphy_dev->ctrl_reg);
	csiphy_dev->ctrl_reg = NULL;
	platform_set_drvdata(pdev, NULL);
	v4l2_set_subdevdata(&(csiphy_dev->v4l2_dev_str.sd), NULL);
	devm_kfree(&pdev->dev, csiphy_dev);

	return 0;
}

static const struct of_device_id cam_csiphy_dt_match[] = {
	{.compatible = "qcom,csiphy"},
	{}
};

MODULE_DEVICE_TABLE(of, cam_csiphy_dt_match);

static struct platform_driver csiphy_driver = {
	.probe = cam_csiphy_platform_probe,
	.remove = cam_csiphy_device_remove,
	.driver = {
		.name = CAMX_CSIPHY_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cam_csiphy_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int32_t __init cam_csiphy_init_module(void)
{
	return platform_driver_register(&csiphy_driver);
}

static void __exit cam_csiphy_exit_module(void)
{
	platform_driver_unregister(&csiphy_driver);
}

module_init(cam_csiphy_init_module);
module_exit(cam_csiphy_exit_module);
MODULE_DESCRIPTION("CAM CSIPHY driver");
MODULE_LICENSE("GPL v2");
