// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include "cam_csiphy_dev.h"
#include "cam_req_mgr_dev.h"
#include "cam_csiphy_soc.h"
#include "cam_csiphy_core.h"
#include <media/cam_sensor.h>
#include <dt-bindings/msm/msm-camera.h>
#include "camera_main.h"

static struct dentry *root_dentry;

static void cam_csiphy_subdev_handle_message(
		struct v4l2_subdev *sd,
		enum cam_subdev_message_type_t message_type,
		uint32_t data)
{
	struct csiphy_device *csiphy_dev = v4l2_get_subdevdata(sd);

	switch (message_type) {
	case CAM_SUBDEV_MESSAGE_IRQ_ERR:
		CAM_INFO(CAM_CSIPHY, "subdev index : %d CSIPHY index: %d",
				csiphy_dev->soc_info.index, data);
		if (data == csiphy_dev->soc_info.index)
			cam_csiphy_status_dmp(csiphy_dev);
		break;
	default:
		break;
	}
}

static int cam_csiphy_debug_register(struct csiphy_device *csiphy_dev)
{
	int rc = 0;
	struct dentry *dbgfileptr = NULL;
	char debugfs_name[25];

	if (!csiphy_dev) {
		CAM_ERR(CAM_CSIPHY, "null CSIPHY dev ptr");
		return -EINVAL;
	}

	if (!root_dentry) {
		dbgfileptr = debugfs_create_dir("camera_csiphy", NULL);
		if (!dbgfileptr) {
			CAM_ERR(CAM_CSIPHY,
				"Debugfs could not create directory!");
			rc = -ENOENT;
			goto end;
		}
		/* Store parent inode for cleanup in caller */
		root_dentry = dbgfileptr;
	}

	snprintf(debugfs_name, 25, "%s%d%s", "csiphy",
		csiphy_dev->soc_info.index,
		"_en_irq_dump");
	dbgfileptr = debugfs_create_bool(debugfs_name, 0644,
		root_dentry, &csiphy_dev->enable_irq_dump);

	if (IS_ERR(dbgfileptr)) {
		if (PTR_ERR(dbgfileptr) == -ENODEV)
			CAM_WARN(CAM_CSIPHY, "DebugFS not enabled in kernel!");
		else
			rc = PTR_ERR(dbgfileptr);
	}
end:
	return rc;
}

static void cam_csiphy_debug_unregister(void)
{
	debugfs_remove_recursive(root_dentry);
	root_dentry = NULL;
}

static int cam_csiphy_subdev_close_internal(struct v4l2_subdev *sd,
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

static int cam_csiphy_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	bool crm_active = cam_req_mgr_is_open(CAM_CSIPHY);

	if (crm_active) {
		CAM_DBG(CAM_CSIPHY, "CRM is ACTIVE, close should be from CRM");
		return 0;
	}

	return cam_csiphy_subdev_close_internal(sd, fh);
}

static long cam_csiphy_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct csiphy_device *csiphy_dev = v4l2_get_subdevdata(sd);
	int rc = 0;

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_csiphy_core_cfg(csiphy_dev, arg);
		if (rc)
			CAM_ERR(CAM_CSIPHY,
				"Failed in configuring the device: %d", rc);
		break;
	case CAM_SD_SHUTDOWN:
		if (!cam_req_mgr_is_shutdown()) {
			CAM_ERR(CAM_CORE, "SD shouldn't come from user space");
			return 0;
		}

		rc = cam_csiphy_subdev_close_internal(sd, NULL);
		break;
	default:
		CAM_ERR(CAM_CSIPHY, "Wrong ioctl : %d", cmd);
		rc = -ENOIOCTLCMD;
		break;
	}

	return rc;
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
		if (rc)
			CAM_ERR(CAM_CSIPHY,
				"Failed in subdev_ioctl: %d", rc);
		break;
	default:
		CAM_ERR(CAM_CSIPHY, "Invalid compat ioctl cmd: %d", cmd);
		rc = -ENOIOCTLCMD;
		break;
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

static int cam_csiphy_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_cpas_register_params cpas_parms;
	struct csiphy_device *new_csiphy_dev;
	int32_t               rc = 0;
	struct platform_device *pdev = to_platform_device(dev);
	int i;

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
	/* validate PHY FUSE only for CSIPHY4 */
	if ((new_csiphy_dev->soc_info.index == 4) &&
		!cam_cpas_is_feature_supported(
			CAM_CPAS_CSIPHY_FUSE,
			(1 << new_csiphy_dev->soc_info.index), NULL)) {
		CAM_ERR(CAM_CSIPHY, "PHY%d is not supported: %d",
			new_csiphy_dev->soc_info.index);
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
	new_csiphy_dev->v4l2_dev_str.msg_cb =
		cam_csiphy_subdev_handle_message;
	new_csiphy_dev->v4l2_dev_str.token =
		new_csiphy_dev;
	new_csiphy_dev->v4l2_dev_str.close_seq_prior =
		CAM_SD_CLOSE_MEDIUM_PRIORITY;

	rc = cam_register_subdev(&(new_csiphy_dev->v4l2_dev_str));
	if (rc < 0) {
		CAM_ERR(CAM_CSIPHY, "cam_register_subdev Failed rc: %d", rc);
		goto csiphy_no_resource;
	}

	platform_set_drvdata(pdev, &(new_csiphy_dev->v4l2_dev_str.sd));

	for (i = 0; i < CSIPHY_MAX_INSTANCES_PER_PHY; i++) {
		new_csiphy_dev->csiphy_info[i].hdl_data.device_hdl = -1;
		new_csiphy_dev->csiphy_info[i].hdl_data.session_hdl = -1;
		new_csiphy_dev->csiphy_info[i].csiphy_3phase = -1;
		new_csiphy_dev->csiphy_info[i].data_rate = 0;
		new_csiphy_dev->csiphy_info[i].settle_time = 0;
		new_csiphy_dev->csiphy_info[i].lane_cnt = 0;
		new_csiphy_dev->csiphy_info[i].lane_assign = 0;
		new_csiphy_dev->csiphy_info[i].lane_enable = 0;
	}

	new_csiphy_dev->ops.get_dev_info = NULL;
	new_csiphy_dev->ops.link_setup = NULL;
	new_csiphy_dev->ops.apply_req = NULL;

	new_csiphy_dev->acquire_count = 0;
	new_csiphy_dev->start_dev_count = 0;

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

	cam_csiphy_register_baseaddress(new_csiphy_dev);

	CAM_DBG(CAM_CSIPHY, "%s component bound successfully",
		pdev->name);

	cam_csiphy_debug_register(new_csiphy_dev);

	return rc;

csiphy_unregister_subdev:
	cam_unregister_subdev(&(new_csiphy_dev->v4l2_dev_str));
csiphy_no_resource:
	mutex_destroy(&new_csiphy_dev->mutex);
	kfree(new_csiphy_dev->ctrl_reg);
	devm_kfree(&pdev->dev, new_csiphy_dev);
	return rc;
}

static void cam_csiphy_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);

	struct v4l2_subdev *subdev = platform_get_drvdata(pdev);
	struct csiphy_device *csiphy_dev = v4l2_get_subdevdata(subdev);

	cam_csiphy_debug_unregister();
	CAM_INFO(CAM_CSIPHY, "Unbind CSIPHY component");
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
}

const static struct component_ops cam_csiphy_component_ops = {
	.bind = cam_csiphy_component_bind,
	.unbind = cam_csiphy_component_unbind,
};

static int32_t cam_csiphy_platform_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_CSIPHY, "Adding CSIPHY component");
	rc = component_add(&pdev->dev, &cam_csiphy_component_ops);
	if (rc)
		CAM_ERR(CAM_CSIPHY, "failed to add component rc: %d", rc);

	return rc;
}


static int32_t cam_csiphy_device_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_csiphy_component_ops);
	return 0;
}

static const struct of_device_id cam_csiphy_dt_match[] = {
	{.compatible = "qcom,csiphy"},
	{}
};

MODULE_DEVICE_TABLE(of, cam_csiphy_dt_match);

struct platform_driver csiphy_driver = {
	.probe = cam_csiphy_platform_probe,
	.remove = cam_csiphy_device_remove,
	.driver = {
		.name = CAMX_CSIPHY_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cam_csiphy_dt_match,
		.suppress_bind_attrs = true,
	},
};

int32_t cam_csiphy_init_module(void)
{
	return platform_driver_register(&csiphy_driver);
}

void cam_csiphy_exit_module(void)
{
	platform_driver_unregister(&csiphy_driver);
}

MODULE_DESCRIPTION("CAM CSIPHY driver");
MODULE_LICENSE("GPL v2");
