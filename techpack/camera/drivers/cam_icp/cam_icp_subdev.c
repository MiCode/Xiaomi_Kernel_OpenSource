// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/cam_req_mgr.h>
#include <media/cam_defs.h>
#include <media/cam_icp.h>
#include "cam_req_mgr_dev.h"
#include "cam_subdev.h"
#include "cam_node.h"
#include "cam_context.h"
#include "cam_icp_context.h"
#include "cam_hw_mgr_intf.h"
#include "cam_icp_hw_mgr_intf.h"
#include "cam_debug_util.h"
#include "cam_smmu_api.h"
#include "camera_main.h"

#define CAM_ICP_DEV_NAME        "cam-icp"

struct cam_icp_subdev {
	struct cam_subdev sd;
	struct cam_node *node;
	struct cam_context ctx[CAM_ICP_CTX_MAX];
	struct cam_icp_context ctx_icp[CAM_ICP_CTX_MAX];
	struct mutex icp_lock;
	int32_t open_cnt;
	int32_t reserved;
};

static struct cam_icp_subdev g_icp_dev;

static const struct of_device_id cam_icp_dt_match[] = {
	{.compatible = "qcom,cam-icp"},
	{}
};

static void cam_icp_dev_iommu_fault_handler(struct cam_smmu_pf_info *pf_info)
{
	int i = 0;
	struct cam_node *node = NULL;

	if (!pf_info || !pf_info->token) {
		CAM_ERR(CAM_ISP, "invalid token in page handler cb");
		return;
	}

	node = (struct cam_node *)pf_info->token;

	for (i = 0; i < node->ctx_size; i++)
		cam_context_dump_pf_info(&(node->ctx_list[i]), pf_info);
}

static int cam_icp_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_hw_mgr_intf *hw_mgr_intf = NULL;
	struct cam_node *node = v4l2_get_subdevdata(sd);
	int rc = 0;

	mutex_lock(&g_icp_dev.icp_lock);
	if (g_icp_dev.open_cnt >= 1) {
		CAM_ERR(CAM_ICP, "ICP subdev is already opened");
		rc = -EALREADY;
		goto end;
	}

	if (!node) {
		CAM_ERR(CAM_ICP, "Invalid args");
		rc = -EINVAL;
		goto end;
	}

	hw_mgr_intf = &node->hw_mgr_intf;
	rc = hw_mgr_intf->hw_open(hw_mgr_intf->hw_mgr_priv, NULL);
	if (rc < 0) {
		CAM_ERR(CAM_ICP, "FW download failed");
		goto end;
	}
	g_icp_dev.open_cnt++;
end:
	mutex_unlock(&g_icp_dev.icp_lock);
	return rc;
}

static int cam_icp_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct cam_hw_mgr_intf *hw_mgr_intf = NULL;
	struct cam_node *node = v4l2_get_subdevdata(sd);

	mutex_lock(&g_icp_dev.icp_lock);
	if (g_icp_dev.open_cnt <= 0) {
		CAM_DBG(CAM_ICP, "ICP subdev is already closed");
		rc = -EINVAL;
		goto end;
	}
	g_icp_dev.open_cnt--;
	if (!node) {
		CAM_ERR(CAM_ICP, "Invalid args");
		rc = -EINVAL;
		goto end;
	}

	hw_mgr_intf = &node->hw_mgr_intf;
	if (!hw_mgr_intf) {
		CAM_ERR(CAM_ICP, "hw_mgr_intf is not initialized");
		rc = -EINVAL;
		goto end;
	}

	rc = cam_node_shutdown(node);
	if (rc < 0) {
		CAM_ERR(CAM_ICP, "HW close failed");
		goto end;
	}

end:
	mutex_unlock(&g_icp_dev.icp_lock);
	return 0;
}

const struct v4l2_subdev_internal_ops cam_icp_subdev_internal_ops = {
	.open = cam_icp_subdev_open,
	.close = cam_icp_subdev_close,
};

static int cam_icp_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	int rc = 0, i = 0;
	struct cam_node *node;
	struct cam_hw_mgr_intf *hw_mgr_intf;
	int iommu_hdl = -1;
	struct platform_device *pdev = to_platform_device(dev);

	if (!pdev) {
		CAM_ERR(CAM_ICP, "pdev is NULL");
		return -EINVAL;
	}

	g_icp_dev.sd.pdev = pdev;
	g_icp_dev.sd.internal_ops = &cam_icp_subdev_internal_ops;
	rc = cam_subdev_probe(&g_icp_dev.sd, pdev, CAM_ICP_DEV_NAME,
		CAM_ICP_DEVICE_TYPE);
	if (rc) {
		CAM_ERR(CAM_ICP, "ICP cam_subdev_probe failed");
		goto probe_fail;
	}

	node = (struct cam_node *) g_icp_dev.sd.token;

	hw_mgr_intf = kzalloc(sizeof(*hw_mgr_intf), GFP_KERNEL);
	if (!hw_mgr_intf) {
		rc = -ENOMEM;
		CAM_ERR(CAM_ICP, "Memory allocation fail");
		goto hw_alloc_fail;
	}

	rc = cam_icp_hw_mgr_init(pdev->dev.of_node, (uint64_t *)hw_mgr_intf,
		&iommu_hdl);
	if (rc) {
		CAM_ERR(CAM_ICP, "ICP HW manager init failed: %d", rc);
		goto hw_init_fail;
	}

	for (i = 0; i < CAM_ICP_CTX_MAX; i++) {
		g_icp_dev.ctx_icp[i].base = &g_icp_dev.ctx[i];
		rc = cam_icp_context_init(&g_icp_dev.ctx_icp[i],
					hw_mgr_intf, i);
		if (rc) {
			CAM_ERR(CAM_ICP, "ICP context init failed");
			goto ctx_fail;
		}
	}

	rc = cam_node_init(node, hw_mgr_intf, g_icp_dev.ctx,
				CAM_ICP_CTX_MAX, CAM_ICP_DEV_NAME);
	if (rc) {
		CAM_ERR(CAM_ICP, "ICP node init failed");
		goto ctx_fail;
	}

	cam_smmu_set_client_page_fault_handler(iommu_hdl,
		cam_icp_dev_iommu_fault_handler, node);

	g_icp_dev.open_cnt = 0;
	mutex_init(&g_icp_dev.icp_lock);

	CAM_DBG(CAM_ICP, "Component bound successfully");

	return rc;

ctx_fail:
	for (--i; i >= 0; i--)
		cam_icp_context_deinit(&g_icp_dev.ctx_icp[i]);
hw_init_fail:
	kfree(hw_mgr_intf);
hw_alloc_fail:
	cam_subdev_remove(&g_icp_dev.sd);
probe_fail:
	return rc;
}

static void cam_icp_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	int i;
	struct v4l2_subdev *sd;
	struct platform_device *pdev = to_platform_device(dev);

	if (!pdev) {
		CAM_ERR(CAM_ICP, "pdev is NULL");
		return;
	}

	sd = platform_get_drvdata(pdev);
	if (!sd) {
		CAM_ERR(CAM_ICP, "V4l2 subdev is NULL");
		return;
	}

	for (i = 0; i < CAM_ICP_CTX_MAX; i++)
		cam_icp_context_deinit(&g_icp_dev.ctx_icp[i]);

	cam_icp_hw_mgr_deinit();
	cam_node_deinit(g_icp_dev.node);
	cam_subdev_remove(&g_icp_dev.sd);
	mutex_destroy(&g_icp_dev.icp_lock);
}

const static struct component_ops cam_icp_component_ops = {
	.bind = cam_icp_component_bind,
	.unbind = cam_icp_component_unbind,
};

static int cam_icp_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_ICP, "Adding ICP component");
	rc = component_add(&pdev->dev, &cam_icp_component_ops);
	if (rc)
		CAM_ERR(CAM_ICP, "failed to add component rc: %d", rc);

	return rc;
}

static int cam_icp_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_icp_component_ops);
	return 0;
}

struct platform_driver cam_icp_driver = {
	.probe = cam_icp_probe,
	.remove = cam_icp_remove,
	.driver = {
		.name = "cam_icp",
		.owner = THIS_MODULE,
		.of_match_table = cam_icp_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_icp_init_module(void)
{
	return platform_driver_register(&cam_icp_driver);
}

void cam_icp_exit_module(void)
{
	platform_driver_unregister(&cam_icp_driver);
}

MODULE_DESCRIPTION("MSM ICP driver");
MODULE_LICENSE("GPL v2");
