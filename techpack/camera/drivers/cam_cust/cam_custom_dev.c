// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/ion.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>

#include <media/cam_req_mgr.h>

#include "cam_custom_dev.h"
#include "cam_hw_mgr_intf.h"
#include "cam_custom_hw_mgr_intf.h"
#include "cam_node.h"
#include "cam_debug_util.h"
#include "cam_smmu_api.h"
#include "camera_main.h"

static struct cam_custom_dev g_custom_dev;

static void cam_custom_dev_iommu_fault_handler(
	struct cam_smmu_pf_info *pf_info)
{
	int i = 0;
	struct cam_node *node = NULL;

	if (!pf_info || !pf_info->token) {
		CAM_ERR(CAM_CUSTOM, "invalid token in page handler cb");
		return;
	}

	node = (struct cam_node *)pf_info->token;

	for (i = 0; i < node->ctx_size; i++)
		cam_context_dump_pf_info(&(node->ctx_list[i]), pf_info);
}

static const struct of_device_id cam_custom_dt_match[] = {
	{
		.compatible = "qcom,cam-custom"
	},
	{}
};

static int cam_custom_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	mutex_lock(&g_custom_dev.custom_dev_mutex);
	g_custom_dev.open_cnt++;
	mutex_unlock(&g_custom_dev.custom_dev_mutex);

	return 0;
}

static int cam_custom_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct cam_node *node = v4l2_get_subdevdata(sd);

	mutex_lock(&g_custom_dev.custom_dev_mutex);
	if (g_custom_dev.open_cnt <= 0) {
		CAM_DBG(CAM_CUSTOM, "Custom subdev is already closed");
		rc = -EINVAL;
		goto end;
	}

	g_custom_dev.open_cnt--;
	if (!node) {
		CAM_ERR(CAM_CUSTOM, "Node ptr is NULL");
		rc = -EINVAL;
		goto end;
	}

	if (g_custom_dev.open_cnt == 0)
		cam_node_shutdown(node);

end:
	mutex_unlock(&g_custom_dev.custom_dev_mutex);
	return rc;
}

static const struct v4l2_subdev_internal_ops cam_custom_subdev_internal_ops = {
	.close = cam_custom_subdev_close,
	.open = cam_custom_subdev_open,
};

static int cam_custom_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	int rc = -EINVAL;
	int i;
	struct cam_hw_mgr_intf         hw_mgr_intf;
	struct cam_node                *node;
	int iommu_hdl = -1;
	struct platform_device *pdev = to_platform_device(dev);

	g_custom_dev.sd.internal_ops = &cam_custom_subdev_internal_ops;

	rc = cam_subdev_probe(&g_custom_dev.sd, pdev, CAM_CUSTOM_DEV_NAME,
		CAM_CUSTOM_DEVICE_TYPE);
	if (rc) {
		CAM_ERR(CAM_CUSTOM, "Custom device cam_subdev_probe failed!");
		goto err;
	}
	node = (struct cam_node *) g_custom_dev.sd.token;

	memset(&hw_mgr_intf, 0, sizeof(hw_mgr_intf));
	rc = cam_custom_hw_mgr_init(pdev->dev.of_node,
		&hw_mgr_intf, &iommu_hdl);
	if (rc != 0) {
		CAM_ERR(CAM_CUSTOM, "Can not initialized Custom HW manager!");
		goto unregister;
	}

	for (i = 0; i < CAM_CUSTOM_HW_MAX_INSTANCES; i++) {
		rc = cam_custom_dev_context_init(&g_custom_dev.ctx_custom[i],
			&g_custom_dev.ctx[i],
			&node->crm_node_intf,
			&node->hw_mgr_intf,
			i);
		if (rc) {
			CAM_ERR(CAM_CUSTOM, "Custom context init failed!");
			goto unregister;
		}
	}

	rc = cam_node_init(node, &hw_mgr_intf, g_custom_dev.ctx,
		CAM_CUSTOM_HW_MAX_INSTANCES, CAM_CUSTOM_DEV_NAME);
	if (rc) {
		CAM_ERR(CAM_CUSTOM, "Custom HW node init failed!");
		goto unregister;
	}

	cam_smmu_set_client_page_fault_handler(iommu_hdl,
		cam_custom_dev_iommu_fault_handler, node);

	mutex_init(&g_custom_dev.custom_dev_mutex);

	CAM_DBG(CAM_CUSTOM, "%s component bound successfully", pdev->name);

	return 0;
unregister:
	rc = cam_subdev_remove(&g_custom_dev.sd);
err:
	return rc;
}

static void cam_custom_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	int rc = 0;
	int i;

	/* clean up resources */
	for (i = 0; i < CAM_CUSTOM_HW_MAX_INSTANCES; i++) {
		rc = cam_custom_dev_context_deinit(&g_custom_dev.ctx_custom[i]);
		if (rc)
			CAM_ERR(CAM_CUSTOM,
				"Custom context %d deinit failed", i);
	}

	rc = cam_subdev_remove(&g_custom_dev.sd);
	if (rc)
		CAM_ERR(CAM_CUSTOM, "Unregister failed");

	memset(&g_custom_dev, 0, sizeof(g_custom_dev));
}

const static struct component_ops cam_custom_component_ops = {
	.bind = cam_custom_component_bind,
	.unbind = cam_custom_component_unbind,
};

static int cam_custom_dev_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_custom_component_ops);
	return 0;
}

static int cam_custom_dev_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_CUSTOM, "Adding Custom HW component");
	rc = component_add(&pdev->dev, &cam_custom_component_ops);
	if (rc)
		CAM_ERR(CAM_CUSTOM, "failed to add component rc: %d", rc);

	return rc;
}

struct platform_driver custom_driver = {
	.probe = cam_custom_dev_probe,
	.remove = cam_custom_dev_remove,
	.driver = {
		.name = "cam_custom",
		.of_match_table = cam_custom_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_custom_dev_init_module(void)
{
	return platform_driver_register(&custom_driver);
}

void cam_custom_dev_exit_module(void)
{
	platform_driver_unregister(&custom_driver);
}

MODULE_DESCRIPTION("MSM CUSTOM driver");
MODULE_LICENSE("GPL v2");
