// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "cam_node.h"
#include "cam_hw_mgr_intf.h"
#include "cam_cre_hw_mgr.h"
#include "cam_cre_dev.h"
#include "cam_debug_util.h"
#include "cam_smmu_api.h"
#include "camera_main.h"

#define CAM_CRE_DEV_NAME "cam-cre"

struct cam_cre_subdev {
	struct cam_subdev sd;
	struct cam_node *node;
	struct cam_context ctx[CRE_CTX_MAX];
	struct cam_cre_context ctx_cre[CRE_CTX_MAX];
	struct mutex cre_lock;
	int32_t open_cnt;
	int32_t reserved;
};
static struct cam_cre_subdev g_cre_dev;

static void cam_cre_dev_iommu_fault_handler(
	struct cam_smmu_pf_info *pf_info)
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

static int cam_cre_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{

	mutex_lock(&g_cre_dev.cre_lock);
	g_cre_dev.open_cnt++;
	mutex_unlock(&g_cre_dev.cre_lock);

	return 0;
}

static int cam_cre_subdev_close_internal(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct cam_node *node = v4l2_get_subdevdata(sd);


	mutex_lock(&g_cre_dev.cre_lock);
	if (g_cre_dev.open_cnt <= 0) {
		CAM_DBG(CAM_CRE, "CRE subdev is already closed");
		rc = -EINVAL;
		goto end;
	}

	g_cre_dev.open_cnt--;

	if (!node) {
		CAM_ERR(CAM_CRE, "Node ptr is NULL");
		rc = -EINVAL;
		goto end;
	}

	if (g_cre_dev.open_cnt == 0)
		cam_node_shutdown(node);

end:
	mutex_unlock(&g_cre_dev.cre_lock);
	return rc;
}

static int cam_cre_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	bool crm_active = cam_req_mgr_is_open();

	if (crm_active) {
		CAM_DBG(CAM_ICP, "CRM is ACTIVE, close should be from CRM");
		return 0;
	}

	return cam_cre_subdev_close_internal(sd, fh);
}

static const struct v4l2_subdev_internal_ops cam_cre_subdev_internal_ops = {
	.close = cam_cre_subdev_close,
	.open = cam_cre_subdev_open,
};

static int cam_cre_subdev_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	int i;
	int rc = 0;
	struct cam_hw_mgr_intf *hw_mgr_intf;
	struct cam_node *node;
	int iommu_hdl = -1;
	struct platform_device *pdev = to_platform_device(dev);

	g_cre_dev.sd.pdev = pdev;
	g_cre_dev.sd.internal_ops = &cam_cre_subdev_internal_ops;
	rc = cam_subdev_probe(&g_cre_dev.sd, pdev, CAM_CRE_DEV_NAME,
		CAM_CRE_DEVICE_TYPE);
	if (rc) {
		CAM_ERR(CAM_CRE, "CRE cam_subdev_probe failed %d", rc);
		goto err;
	}
	node = (struct cam_node *)g_cre_dev.sd.token;

	hw_mgr_intf = kzalloc(sizeof(*hw_mgr_intf), GFP_KERNEL);
	if (!hw_mgr_intf) {
		CAM_ERR(CAM_CRE, "Error allocating memory");
		rc = -ENOMEM;
		goto hw_alloc_fail;
	}

	rc = cam_cre_hw_mgr_init(pdev->dev.of_node, hw_mgr_intf,
		&iommu_hdl);
	if (rc) {
		CAM_ERR(CAM_CRE, "Can not initialize CRE HWmanager %d", rc);
		goto hw_init_fail;
	}

	memset(g_cre_dev.ctx_cre, 0,  sizeof(g_cre_dev.ctx_cre));
	for (i = 0; i < CAM_CRE_CTX_MAX; i++) {
		g_cre_dev.ctx_cre[i].base = &g_cre_dev.ctx[i];
		rc = cam_cre_context_init(&g_cre_dev.ctx_cre[i],
			hw_mgr_intf, i, iommu_hdl);
		if (rc) {
			CAM_ERR(CAM_CRE, "CRE context init failed %d %d",
				i, rc);
			goto ctx_init_fail;
		}
	}

	rc = cam_node_init(node, hw_mgr_intf, g_cre_dev.ctx,
		CAM_CRE_CTX_MAX, CAM_CRE_DEV_NAME);
	if (rc) {
		CAM_ERR(CAM_CRE, "CRE node init failed %d", rc);
		goto ctx_init_fail;
	}

	node->sd_handler = cam_cre_subdev_close_internal;
	cam_smmu_set_client_page_fault_handler(iommu_hdl,
		cam_cre_dev_iommu_fault_handler, node);

	g_cre_dev.open_cnt = 0;
	mutex_init(&g_cre_dev.cre_lock);

	CAM_DBG(CAM_CRE, "Component bound successfully");

	return rc;

ctx_init_fail:
	for (--i; i >= 0; i--)
		if (cam_cre_context_deinit(&g_cre_dev.ctx_cre[i]))
			CAM_ERR(CAM_CRE, "deinit fail %d %d", i, rc);
hw_init_fail:
	kfree(hw_mgr_intf);
hw_alloc_fail:
	if (cam_subdev_remove(&g_cre_dev.sd))
		CAM_ERR(CAM_CRE, "remove fail %d", rc);
err:
	return rc;
}

static void cam_cre_subdev_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	int i;

	for (i = 0; i < CRE_CTX_MAX; i++)
		cam_cre_context_deinit(&g_cre_dev.ctx_cre[i]);

	cam_node_deinit(g_cre_dev.node);
	cam_subdev_remove(&g_cre_dev.sd);
	mutex_destroy(&g_cre_dev.cre_lock);
}

const static struct component_ops cam_cre_subdev_component_ops = {
	.bind = cam_cre_subdev_component_bind,
	.unbind = cam_cre_subdev_component_unbind,
};

static int cam_cre_subdev_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_cre_subdev_component_ops);
	return 0;
}

static int cam_cre_subdev_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_CRE, "Adding CRE sub component");
	rc = component_add(&pdev->dev, &cam_cre_subdev_component_ops);
	if (rc)
		CAM_ERR(CAM_CRE, "failed to add component rc: %d", rc);
	return rc;
}

static const struct of_device_id cam_cre_subdev_dt_match[] = {
	{
		.compatible = "qcom,cam-cre",
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_cre_subdev_dt_match);

struct platform_driver cam_cre_subdev_driver = {
	.probe = cam_cre_subdev_probe,
	.remove = cam_cre_subdev_remove,
	.driver = {
		.name = "cam_cre",
		.of_match_table = cam_cre_subdev_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_cre_subdev_init_module(void)
{
	return platform_driver_register(&cam_cre_subdev_driver);
}

void cam_cre_subdev_exit_module(void)
{
	platform_driver_unregister(&cam_cre_subdev_driver);
}

MODULE_DESCRIPTION("MSM CRE driver");
MODULE_LICENSE("GPL v2");
