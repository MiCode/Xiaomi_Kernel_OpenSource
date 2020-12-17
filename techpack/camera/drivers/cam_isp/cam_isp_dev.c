// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>

#include <media/cam_req_mgr.h>
#include "cam_isp_dev.h"
#include "cam_hw_mgr_intf.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_node.h"
#include "cam_debug_util.h"
#include "cam_smmu_api.h"

static struct cam_isp_dev g_isp_dev;

static void cam_isp_dev_iommu_fault_handler(
	struct iommu_domain *domain, struct device *dev, unsigned long iova,
	int flags, void *token, uint32_t buf_info)
{
	int i = 0;
	struct cam_node *node = NULL;

	if (!token) {
		CAM_ERR(CAM_ISP, "invalid token in page handler cb");
		return;
	}

	node = (struct cam_node *)token;

	for (i = 0; i < node->ctx_size; i++)
		cam_context_dump_pf_info(&(node->ctx_list[i]), iova,
			buf_info);
}

static const struct of_device_id cam_isp_dt_match[] = {
	{
		.compatible = "qcom,cam-isp"
	},
	{}
};

static int cam_isp_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	mutex_lock(&g_isp_dev.isp_mutex);
	g_isp_dev.open_cnt++;
	mutex_unlock(&g_isp_dev.isp_mutex);

	return 0;
}

static int cam_isp_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct cam_node *node = v4l2_get_subdevdata(sd);

	mutex_lock(&g_isp_dev.isp_mutex);
	if (g_isp_dev.open_cnt <= 0) {
		CAM_DBG(CAM_ISP, "ISP subdev is already closed");
		rc = -EINVAL;
		goto end;
	}

	g_isp_dev.open_cnt--;
	if (!node) {
		CAM_ERR(CAM_ISP, "Node ptr is NULL");
		rc = -EINVAL;
		goto end;
	}

	if (g_isp_dev.open_cnt == 0)
		cam_node_shutdown(node);

end:
	mutex_unlock(&g_isp_dev.isp_mutex);
	return rc;
}

static const struct v4l2_subdev_internal_ops cam_isp_subdev_internal_ops = {
	.close = cam_isp_subdev_close,
	.open = cam_isp_subdev_open,
};

static int cam_isp_dev_remove(struct platform_device *pdev)
{
	int rc = 0;
	int i;

	/* clean up ife/tfe resources */
	for (i = 0; i < g_isp_dev.max_context; i++) {
		rc = cam_isp_context_deinit(&g_isp_dev.ctx_isp[i]);
		if (rc)
			CAM_ERR(CAM_ISP, "ISP context %d deinit failed",
				i);
	}
	kfree(g_isp_dev.ctx);
	g_isp_dev.ctx = NULL;
	kfree(g_isp_dev.ctx_isp);
	g_isp_dev.ctx_isp = NULL;

	rc = cam_subdev_remove(&g_isp_dev.sd);
	if (rc)
		CAM_ERR(CAM_ISP, "Unregister failed");

	memset(&g_isp_dev, 0, sizeof(g_isp_dev));
	return 0;
}

static int cam_isp_dev_probe(struct platform_device *pdev)
{
	int rc = -1;
	int i;
	struct cam_hw_mgr_intf         hw_mgr_intf;
	struct cam_node               *node;
	const char                    *compat_str = NULL;

	int iommu_hdl = -1;

	rc = of_property_read_string_index(pdev->dev.of_node, "arch-compat", 0,
		(const char **)&compat_str);

	g_isp_dev.sd.internal_ops = &cam_isp_subdev_internal_ops;
	/* Initialize the v4l2 subdevice first. (create cam_node) */
	if (strnstr(compat_str, "ife", strlen(compat_str))) {
		rc = cam_subdev_probe(&g_isp_dev.sd, pdev, CAM_ISP_DEV_NAME,
		CAM_IFE_DEVICE_TYPE);
		g_isp_dev.isp_device_type = CAM_IFE_DEVICE_TYPE;
		g_isp_dev.max_context = CAM_IFE_CTX_MAX;
	} else if (strnstr(compat_str, "tfe", strlen(compat_str))) {
		rc = cam_subdev_probe(&g_isp_dev.sd, pdev, CAM_ISP_DEV_NAME,
		CAM_TFE_DEVICE_TYPE);
		g_isp_dev.isp_device_type = CAM_TFE_DEVICE_TYPE;
		g_isp_dev.max_context = CAM_TFE_CTX_MAX;
	} else  {
		CAM_ERR(CAM_ISP, "Invalid ISP hw type %s", compat_str);
		rc = -EINVAL;
		goto err;
	}

	if (rc) {
		CAM_ERR(CAM_ISP, "ISP cam_subdev_probe failed!");
		goto err;
	}
	node = (struct cam_node *) g_isp_dev.sd.token;

	memset(&hw_mgr_intf, 0, sizeof(hw_mgr_intf));
	g_isp_dev.ctx = kcalloc(g_isp_dev.max_context,
		sizeof(struct cam_context),
		GFP_KERNEL);
	if (!g_isp_dev.ctx) {
		CAM_ERR(CAM_ISP,
			"Mem Allocation failed for ISP base context");
		goto unregister;
	}

	g_isp_dev.ctx_isp = kcalloc(g_isp_dev.max_context,
		sizeof(struct cam_isp_context),
		GFP_KERNEL);
	if (!g_isp_dev.ctx_isp) {
		CAM_ERR(CAM_ISP,
			"Mem Allocation failed for Isp private context");
		kfree(g_isp_dev.ctx);
		g_isp_dev.ctx = NULL;
		goto unregister;
	}

	rc = cam_isp_hw_mgr_init(compat_str, &hw_mgr_intf, &iommu_hdl);
	if (rc != 0) {
		CAM_ERR(CAM_ISP, "Can not initialized ISP HW manager!");
		goto kfree;
	}

	for (i = 0; i < g_isp_dev.max_context; i++) {
		rc = cam_isp_context_init(&g_isp_dev.ctx_isp[i],
			&g_isp_dev.ctx[i],
			&node->crm_node_intf,
			&node->hw_mgr_intf,
			i,
			g_isp_dev.isp_device_type);
		if (rc) {
			CAM_ERR(CAM_ISP, "ISP context init failed!");
			goto kfree;
		}
	}
	rc = cam_node_init(node, &hw_mgr_intf, g_isp_dev.ctx,
			g_isp_dev.max_context, CAM_ISP_DEV_NAME);

	if (rc) {
		CAM_ERR(CAM_ISP, "ISP node init failed!");
		goto kfree;
	}

	cam_smmu_set_client_page_fault_handler(iommu_hdl,
		cam_isp_dev_iommu_fault_handler, node);

	mutex_init(&g_isp_dev.isp_mutex);

	CAM_INFO(CAM_ISP, "Camera ISP probe complete");

	return 0;

kfree:
	kfree(g_isp_dev.ctx);
	g_isp_dev.ctx = NULL;
	kfree(g_isp_dev.ctx_isp);
	g_isp_dev.ctx_isp = NULL;

unregister:
	rc = cam_subdev_remove(&g_isp_dev.sd);
err:
	return rc;
}


static struct platform_driver isp_driver = {
	.probe = cam_isp_dev_probe,
	.remove = cam_isp_dev_remove,
	.driver = {
		.name = "cam_isp",
		.owner = THIS_MODULE,
		.of_match_table = cam_isp_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_isp_dev_init_module(void)
{
	return platform_driver_register(&isp_driver);
}

static void __exit cam_isp_dev_exit_module(void)
{
	platform_driver_unregister(&isp_driver);
}

module_init(cam_isp_dev_init_module);
module_exit(cam_isp_dev_exit_module);
MODULE_DESCRIPTION("MSM ISP driver");
MODULE_LICENSE("GPL v2");
