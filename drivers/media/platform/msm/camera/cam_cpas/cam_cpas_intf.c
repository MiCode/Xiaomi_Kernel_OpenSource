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

#include <linux/of.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/cam_cpas.h>
#include <media/cam_req_mgr.h>

#include "cam_subdev.h"
#include "cam_cpas_hw_intf.h"

#define CAM_CPAS_DEV_NAME    "cam-cpas"
#define CAM_CPAS_INTF_INITIALIZED() (g_cpas_intf && g_cpas_intf->probe_done)

/**
 * struct cam_cpas_intf : CPAS interface
 *
 * @pdev: Platform device
 * @subdev: Subdev info
 * @hw_intf: CPAS HW interface
 * @hw_caps: CPAS HW capabilities
 * @intf_lock: CPAS interface mutex
 * @open_cnt: CPAS subdev open count
 * @probe_done: Whether CPAS prove completed
 *
 */
struct cam_cpas_intf {
	struct platform_device *pdev;
	struct cam_subdev subdev;
	struct cam_hw_intf *hw_intf;
	struct cam_cpas_hw_caps hw_caps;
	struct mutex intf_lock;
	uint32_t open_cnt;
	bool probe_done;
};

static struct cam_cpas_intf *g_cpas_intf;

int cam_cpas_get_cpas_hw_version(uint32_t *hw_version)
{
	struct cam_hw_info *cpas_hw = NULL;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (!hw_version) {
		CAM_ERR(CAM_CPAS, "invalid input %pK", hw_version);
		return -EINVAL;
	}

	cpas_hw = (struct cam_hw_info  *) g_cpas_intf->hw_intf->hw_priv;

	*hw_version = cpas_hw->soc_info.hw_version;

	if (*hw_version == CAM_CPAS_TITAN_NONE) {
		CAM_DBG(CAM_CPAS, "Didn't find a valid HW Version %d",
			*hw_version);
	}

	return 0;
}


int cam_cpas_get_hw_info(uint32_t *camera_family,
	struct cam_hw_version *camera_version,
	struct cam_hw_version *cpas_version,
	uint32_t *cam_caps)
{
	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (!camera_family || !camera_version || !cpas_version || !cam_caps) {
		CAM_ERR(CAM_CPAS, "invalid input %pK %pK %pK %pK",
			camera_family, camera_version, cpas_version, cam_caps);
		return -EINVAL;
	}

	*camera_family = g_cpas_intf->hw_caps.camera_family;
	*camera_version = g_cpas_intf->hw_caps.camera_version;
	*cpas_version = g_cpas_intf->hw_caps.cpas_version;
	*cam_caps = g_cpas_intf->hw_caps.camera_capability;

	return 0;
}
EXPORT_SYMBOL(cam_cpas_get_hw_info);

int cam_cpas_reg_write(uint32_t client_handle,
	enum cam_cpas_reg_base reg_base, uint32_t offset, bool mb,
	uint32_t value)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		struct cam_cpas_hw_cmd_reg_read_write cmd_reg_write;

		cmd_reg_write.client_handle = client_handle;
		cmd_reg_write.reg_base = reg_base;
		cmd_reg_write.offset = offset;
		cmd_reg_write.value = value;
		cmd_reg_write.mb = mb;

		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_REG_WRITE, &cmd_reg_write,
			sizeof(struct cam_cpas_hw_cmd_reg_read_write));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_reg_write);

int cam_cpas_reg_read(uint32_t client_handle,
	enum cam_cpas_reg_base reg_base, uint32_t offset, bool mb,
	uint32_t *value)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (!value) {
		CAM_ERR(CAM_CPAS, "Invalid arg value");
		return -EINVAL;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		struct cam_cpas_hw_cmd_reg_read_write cmd_reg_read;

		cmd_reg_read.client_handle = client_handle;
		cmd_reg_read.reg_base = reg_base;
		cmd_reg_read.offset = offset;
		cmd_reg_read.mb = mb;
		cmd_reg_read.value = 0;

		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_REG_READ, &cmd_reg_read,
			sizeof(struct cam_cpas_hw_cmd_reg_read_write));
		if (rc) {
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
			return rc;
		}

		*value = cmd_reg_read.value;
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_reg_read);

int cam_cpas_update_axi_vote(uint32_t client_handle,
	struct cam_axi_vote *axi_vote)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		struct cam_cpas_hw_cmd_axi_vote cmd_axi_vote;

		cmd_axi_vote.client_handle = client_handle;
		cmd_axi_vote.axi_vote = axi_vote;

		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_AXI_VOTE, &cmd_axi_vote,
			sizeof(struct cam_cpas_hw_cmd_axi_vote));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_update_axi_vote);

int cam_cpas_update_ahb_vote(uint32_t client_handle,
	struct cam_ahb_vote *ahb_vote)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		struct cam_cpas_hw_cmd_ahb_vote cmd_ahb_vote;

		cmd_ahb_vote.client_handle = client_handle;
		cmd_ahb_vote.ahb_vote = ahb_vote;

		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_AHB_VOTE, &cmd_ahb_vote,
			sizeof(struct cam_cpas_hw_cmd_ahb_vote));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_update_ahb_vote);

int cam_cpas_stop(uint32_t client_handle)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (g_cpas_intf->hw_intf->hw_ops.stop) {
		struct cam_cpas_hw_cmd_stop cmd_hw_stop;

		cmd_hw_stop.client_handle = client_handle;

		rc = g_cpas_intf->hw_intf->hw_ops.stop(
			g_cpas_intf->hw_intf->hw_priv, &cmd_hw_stop,
			sizeof(struct cam_cpas_hw_cmd_stop));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in stop, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid stop ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_stop);

int cam_cpas_start(uint32_t client_handle,
	struct cam_ahb_vote *ahb_vote, struct cam_axi_vote *axi_vote)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (g_cpas_intf->hw_intf->hw_ops.start) {
		struct cam_cpas_hw_cmd_start cmd_hw_start;

		cmd_hw_start.client_handle = client_handle;
		cmd_hw_start.ahb_vote = ahb_vote;
		cmd_hw_start.axi_vote = axi_vote;

		rc = g_cpas_intf->hw_intf->hw_ops.start(
			g_cpas_intf->hw_intf->hw_priv, &cmd_hw_start,
			sizeof(struct cam_cpas_hw_cmd_start));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in start, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid start ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_start);

int cam_cpas_unregister_client(uint32_t client_handle)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_UNREGISTER_CLIENT,
			&client_handle, sizeof(uint32_t));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_unregister_client);

int cam_cpas_register_client(
	struct cam_cpas_register_params *register_params)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_REGISTER_CLIENT, register_params,
			sizeof(struct cam_cpas_register_params));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_register_client);

int cam_cpas_subdev_cmd(struct cam_cpas_intf *cpas_intf,
	struct cam_control *cmd)
{
	int rc = 0;

	if (!cmd) {
		CAM_ERR(CAM_CPAS, "Invalid input cmd");
		return -EINVAL;
	}

	switch (cmd->op_code) {
	case CAM_QUERY_CAP: {
		struct cam_cpas_query_cap query;

		rc = copy_from_user(&query, (void __user *) cmd->handle,
			sizeof(query));
		if (rc) {
			CAM_ERR(CAM_CPAS, "Failed in copy from user, rc=%d",
				rc);
			break;
		}

		rc = cam_cpas_get_hw_info(&query.camera_family,
			&query.camera_version, &query.cpas_version,
			&query.reserved);
		if (rc)
			break;

		rc = copy_to_user((void __user *) cmd->handle, &query,
			sizeof(query));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in copy to user, rc=%d", rc);

		break;
	}
	case CAM_SD_SHUTDOWN:
		break;
	default:
		CAM_ERR(CAM_CPAS, "Unknown op code %d for CPAS", cmd->op_code);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int cam_cpas_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_cpas_intf *cpas_intf = v4l2_get_subdevdata(sd);

	if (!cpas_intf || !cpas_intf->probe_done) {
		CAM_ERR(CAM_CPAS, "CPAS not initialized");
		return -ENODEV;
	}

	mutex_lock(&cpas_intf->intf_lock);
	cpas_intf->open_cnt++;
	CAM_DBG(CAM_CPAS, "CPAS Subdev open count %d", cpas_intf->open_cnt);
	mutex_unlock(&cpas_intf->intf_lock);

	return 0;
}

static int cam_cpas_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_cpas_intf *cpas_intf = v4l2_get_subdevdata(sd);

	if (!cpas_intf || !cpas_intf->probe_done) {
		CAM_ERR(CAM_CPAS, "CPAS not initialized");
		return -ENODEV;
	}

	mutex_lock(&cpas_intf->intf_lock);
	cpas_intf->open_cnt--;
	CAM_DBG(CAM_CPAS, "CPAS Subdev close count %d", cpas_intf->open_cnt);
	mutex_unlock(&cpas_intf->intf_lock);

	return 0;
}

static long cam_cpas_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int32_t rc;
	struct cam_cpas_intf *cpas_intf = v4l2_get_subdevdata(sd);

	if (!cpas_intf || !cpas_intf->probe_done) {
		CAM_ERR(CAM_CPAS, "CPAS not initialized");
		return -ENODEV;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_cpas_subdev_cmd(cpas_intf, (struct cam_control *) arg);
		break;
	default:
		CAM_ERR(CAM_CPAS, "Invalid command %d for CPAS!", cmd);
		rc = -EINVAL;
		break;
	}

	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_cpas_subdev_compat_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	struct cam_control cmd_data;
	int32_t rc;
	struct cam_cpas_intf *cpas_intf = v4l2_get_subdevdata(sd);

	if (!cpas_intf || !cpas_intf->probe_done) {
		CAM_ERR(CAM_CPAS, "CPAS not initialized");
		return -ENODEV;
	}

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_CPAS, "Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_cpas_subdev_cmd(cpas_intf, &cmd_data);
		break;
	default:
		CAM_ERR(CAM_CPAS, "Invalid command %d for CPAS!", cmd);
		rc = -EINVAL;
		break;
	}

	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_CPAS,
				"Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}

	return rc;
}
#endif

static struct v4l2_subdev_core_ops cpas_subdev_core_ops = {
	.ioctl = cam_cpas_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_cpas_subdev_compat_ioctl,
#endif
};

static const struct v4l2_subdev_ops cpas_subdev_ops = {
	.core = &cpas_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops cpas_subdev_intern_ops = {
	.open = cam_cpas_subdev_open,
	.close = cam_cpas_subdev_close,
};

static int cam_cpas_subdev_register(struct platform_device *pdev)
{
	int rc;
	struct cam_subdev *subdev;

	if (!g_cpas_intf)
		return -EINVAL;

	subdev = &g_cpas_intf->subdev;

	subdev->name = CAM_CPAS_DEV_NAME;
	subdev->pdev = pdev;
	subdev->ops = &cpas_subdev_ops;
	subdev->internal_ops = &cpas_subdev_intern_ops;
	subdev->token = g_cpas_intf;
	subdev->sd_flags =
		V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	subdev->ent_function = CAM_CPAS_DEVICE_TYPE;

	rc = cam_register_subdev(subdev);
	if (rc) {
		CAM_ERR(CAM_CPAS, "failed register subdev: %s!",
			CAM_CPAS_DEV_NAME);
		return rc;
	}

	platform_set_drvdata(g_cpas_intf->pdev, g_cpas_intf);
	return rc;
}

static int cam_cpas_dev_probe(struct platform_device *pdev)
{
	struct cam_cpas_hw_caps *hw_caps;
	struct cam_hw_intf *hw_intf;
	int rc;

	if (g_cpas_intf) {
		CAM_ERR(CAM_CPAS, "cpas dev proble already done");
		return -EALREADY;
	}

	g_cpas_intf = kzalloc(sizeof(*g_cpas_intf), GFP_KERNEL);
	if (!g_cpas_intf)
		return -ENOMEM;

	mutex_init(&g_cpas_intf->intf_lock);
	g_cpas_intf->pdev = pdev;

	rc = cam_cpas_hw_probe(pdev, &g_cpas_intf->hw_intf);
	if (rc || (g_cpas_intf->hw_intf == NULL)) {
		CAM_ERR(CAM_CPAS, "Failed in hw probe, rc=%d", rc);
		goto error_destroy_mem;
	}

	hw_intf = g_cpas_intf->hw_intf;
	hw_caps = &g_cpas_intf->hw_caps;
	if (hw_intf->hw_ops.get_hw_caps) {
		rc = hw_intf->hw_ops.get_hw_caps(hw_intf->hw_priv,
			hw_caps, sizeof(struct cam_cpas_hw_caps));
		if (rc) {
			CAM_ERR(CAM_CPAS, "Failed in get_hw_caps, rc=%d", rc);
			goto error_hw_remove;
		}
	} else {
		CAM_ERR(CAM_CPAS, "Invalid get_hw_caps ops");
		goto error_hw_remove;
	}

	rc = cam_cpas_subdev_register(pdev);
	if (rc)
		goto error_hw_remove;

	g_cpas_intf->probe_done = true;
	CAM_DBG(CAM_CPAS,
		"CPAS INTF Probe success %d, %d.%d.%d, %d.%d.%d, 0x%x",
		hw_caps->camera_family, hw_caps->camera_version.major,
		hw_caps->camera_version.minor, hw_caps->camera_version.incr,
		hw_caps->cpas_version.major, hw_caps->cpas_version.minor,
		hw_caps->cpas_version.incr, hw_caps->camera_capability);

	return rc;

error_hw_remove:
	cam_cpas_hw_remove(g_cpas_intf->hw_intf);
error_destroy_mem:
	mutex_destroy(&g_cpas_intf->intf_lock);
	kfree(g_cpas_intf);
	g_cpas_intf = NULL;
	CAM_ERR(CAM_CPAS, "CPAS probe failed");
	return rc;
}

static int cam_cpas_dev_remove(struct platform_device *dev)
{
	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	mutex_lock(&g_cpas_intf->intf_lock);
	cam_unregister_subdev(&g_cpas_intf->subdev);
	cam_cpas_hw_remove(g_cpas_intf->hw_intf);
	mutex_unlock(&g_cpas_intf->intf_lock);
	mutex_destroy(&g_cpas_intf->intf_lock);
	kfree(g_cpas_intf);
	g_cpas_intf = NULL;

	return 0;
}

static const struct of_device_id cam_cpas_dt_match[] = {
	{.compatible = "qcom,cam-cpas"},
	{}
};

static struct platform_driver cam_cpas_driver = {
	.probe = cam_cpas_dev_probe,
	.remove = cam_cpas_dev_remove,
	.driver = {
		.name = CAM_CPAS_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cam_cpas_dt_match,
	},
};

static int __init cam_cpas_dev_init_module(void)
{
	return platform_driver_register(&cam_cpas_driver);
}

static void __exit cam_cpas_dev_exit_module(void)
{
	platform_driver_unregister(&cam_cpas_driver);
}

module_init(cam_cpas_dev_init_module);
module_exit(cam_cpas_dev_exit_module);
MODULE_DESCRIPTION("MSM CPAS driver");
MODULE_LICENSE("GPL v2");
