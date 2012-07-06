/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <mach/irqs.h>
#include <media/msm_isp.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include "msm.h"
#include "server/msm_cam_server.h"
#include "msm_camirq_router.h"

#ifdef CONFIG_MSM_CAMERA_DEBUG
#define D(fmt, args...) pr_debug("msm: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

static void msm_irqrouter_update_irqmap_entry(
	struct msm_cam_server_irqmap_entry *entry,
	int is_composite, int irq_idx, int cam_hw_idx)
{
	int rc = 0;
	entry->irq_idx = irq_idx;
	entry->is_composite = is_composite;
	entry->cam_hw_idx = cam_hw_idx;
	rc = msm_cam_server_update_irqmap(entry);
	if (rc < 0)
		pr_err("%s Error updating irq %d information ",
			__func__, irq_idx);
}

static void msm_irqrouter_send_default_irqmap(
	struct irqrouter_ctrl_type *irqrouter_ctrl)
{
	struct msm_cam_server_irqmap_entry *irqmap =
		&irqrouter_ctrl->def_hw_irqmap[0];

	msm_irqrouter_update_irqmap_entry(&irqmap[CAMERA_SS_IRQ_0],
		0, CAMERA_SS_IRQ_0, MSM_CAM_HW_MICRO);

	msm_irqrouter_update_irqmap_entry(&irqmap[CAMERA_SS_IRQ_1],
		0, CAMERA_SS_IRQ_1, MSM_CAM_HW_CCI);

	msm_irqrouter_update_irqmap_entry(&irqmap[CAMERA_SS_IRQ_2],
		0, CAMERA_SS_IRQ_2, MSM_CAM_HW_CSI0);

	msm_irqrouter_update_irqmap_entry(&irqmap[CAMERA_SS_IRQ_3],
		0, CAMERA_SS_IRQ_3, MSM_CAM_HW_CSI1);

	msm_irqrouter_update_irqmap_entry(&irqmap[CAMERA_SS_IRQ_4],
		0, CAMERA_SS_IRQ_4, MSM_CAM_HW_CSI2);

	msm_irqrouter_update_irqmap_entry(&irqmap[CAMERA_SS_IRQ_5],
		0, CAMERA_SS_IRQ_5, MSM_CAM_HW_CSI3);

	msm_irqrouter_update_irqmap_entry(&irqmap[CAMERA_SS_IRQ_6],
		0, CAMERA_SS_IRQ_6, MSM_CAM_HW_ISPIF);

	msm_irqrouter_update_irqmap_entry(&irqmap[CAMERA_SS_IRQ_7],
		0, CAMERA_SS_IRQ_7, MSM_CAM_HW_CPP);

	msm_irqrouter_update_irqmap_entry(&irqmap[CAMERA_SS_IRQ_8],
		0, CAMERA_SS_IRQ_8, MSM_CAM_HW_VFE0);

	msm_irqrouter_update_irqmap_entry(&irqmap[CAMERA_SS_IRQ_9],
		0, CAMERA_SS_IRQ_9, MSM_CAM_HW_VFE1);

	msm_irqrouter_update_irqmap_entry(&irqmap[CAMERA_SS_IRQ_10],
		0, CAMERA_SS_IRQ_10, MSM_CAM_HW_JPEG0);

	msm_irqrouter_update_irqmap_entry(&irqmap[CAMERA_SS_IRQ_11],
		0, CAMERA_SS_IRQ_11, MSM_CAM_HW_JPEG1);

	msm_irqrouter_update_irqmap_entry(&irqmap[CAMERA_SS_IRQ_12],
		0, CAMERA_SS_IRQ_12, MSM_CAM_HW_JPEG2);
}

static int msm_irqrouter_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct irqrouter_ctrl_type *irqrouter_ctrl = v4l2_get_subdevdata(sd);
	/* Only one object of IRQ Router allowed. */
	if (atomic_read(&irqrouter_ctrl->active) != 0) {
		pr_err("%s IRQ router is already opened\n", __func__);
		return -EINVAL;
	}

	D("%s E ", __func__);
	atomic_inc(&irqrouter_ctrl->active);

	return 0;
}

static int msm_irqrouter_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct irqrouter_ctrl_type *irqrouter_ctrl = v4l2_get_subdevdata(sd);
	if (atomic_read(&irqrouter_ctrl->active) == 0) {
		pr_err("%s IRQ router is already closed\n", __func__);
		return -EINVAL;
	}
	D("%s E ", __func__);
	atomic_dec(&irqrouter_ctrl->active);
	return 0;
}

static const struct v4l2_subdev_internal_ops msm_irqrouter_internal_ops = {
	.open = msm_irqrouter_open,
	.close = msm_irqrouter_close,
};

long msm_irqrouter_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct irqrouter_ctrl_type *irqrouter_ctrl = v4l2_get_subdevdata(sd);
	struct msm_camera_irq_cfg *irq_cfg;
	struct intr_table_entry irq_req;
	int rc = 0;

	/* Handle all IRQ Router Subdev IOCTLs here.
	 * Userspace sends the composite irq configuration.
	 * IRQ Router subdev then configures the registers to group
	 * together individual core hw irqs into a composite IRQ
	 * to the MSM IRQ controller. It also registers them with
	 * the irq manager in the camera server. */
	switch (cmd) {
	case MSM_IRQROUTER_CFG_COMPIRQ:
		COPY_FROM_USER(rc, &irq_cfg, (void __user *)arg,
			sizeof(struct msm_camera_irq_cfg));
		if (rc) {
			ERR_COPY_FROM_USER();
			break;
		}

		if (!irq_cfg ||
			(irq_cfg->irq_idx < CAMERA_SS_IRQ_0) ||
			(irq_cfg->irq_idx >= CAMERA_SS_IRQ_MAX)) {
			pr_err("%s Invalid input", __func__);
			return -EINVAL;
		} else {
			irq_req.cam_hw_mask      = irq_cfg->cam_hw_mask;
			irq_req.irq_idx          = irq_cfg->irq_idx;
			irq_req.irq_num          =
			irqrouter_ctrl->def_hw_irqmap[irq_cfg->irq_idx].irq_num;
			irq_req.is_composite     = 1;
			irq_req.irq_trigger_type = IRQF_TRIGGER_RISING;
			irq_req.num_hwcore       = irq_cfg->num_hwcore;
			irq_req.data             = NULL;
			rc = msm_cam_server_request_irq(&irq_req);
			if (rc < 0) {
				pr_err("%s Error requesting comp irq %d ",
					__func__, irq_req.irq_idx);
				return rc;
			}
			irqrouter_ctrl->def_hw_irqmap
				[irq_cfg->irq_idx].is_composite = 1;
		}
		break;
	default:
		pr_err("%s Invalid cmd %d ", __func__, cmd);
		break;
	}

	return rc;
}

static const struct v4l2_subdev_core_ops msm_irqrouter_subdev_core_ops = {
	.ioctl = msm_irqrouter_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_irqrouter_subdev_ops = {
	.core = &msm_irqrouter_subdev_core_ops,
};

static int __devinit irqrouter_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct irqrouter_ctrl_type *irqrouter_ctrl;
	struct msm_cam_subdev_info sd_info;

	D("%s: device id = %d\n", __func__, pdev->id);

	irqrouter_ctrl = kzalloc(sizeof(struct irqrouter_ctrl_type),
				GFP_KERNEL);
	if (!irqrouter_ctrl) {
		pr_err("%s: not enough memory\n", __func__);
		return -ENOMEM;
	}

	v4l2_subdev_init(&irqrouter_ctrl->subdev, &msm_irqrouter_subdev_ops);
	irqrouter_ctrl->subdev.internal_ops = &msm_irqrouter_internal_ops;
	irqrouter_ctrl->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(irqrouter_ctrl->subdev.name,
			 sizeof(irqrouter_ctrl->subdev.name), "msm_irqrouter");
	v4l2_set_subdevdata(&irqrouter_ctrl->subdev, irqrouter_ctrl);
	irqrouter_ctrl->pdev = pdev;

	if (pdev->dev.of_node)
		of_property_read_u32((&pdev->dev)->of_node,
			"cell-index", &pdev->id);

	msm_irqrouter_send_default_irqmap(irqrouter_ctrl);

	media_entity_init(&irqrouter_ctrl->subdev.entity, 0, NULL, 0);
	irqrouter_ctrl->subdev.entity.type = MEDIA_ENT_T_DEVNODE_V4L;
	irqrouter_ctrl->subdev.entity.group_id = IRQ_ROUTER_DEV;
	irqrouter_ctrl->subdev.entity.name = pdev->name;

	sd_info.sdev_type = IRQ_ROUTER_DEV;
	sd_info.sd_index = 0;
	sd_info.irq_num = 0;
	/* Now register this subdev with the camera server. */
	rc = msm_cam_register_subdev_node(&irqrouter_ctrl->subdev, &sd_info);
	if (rc < 0) {
		pr_err("%s Error registering irqr subdev %d", __func__, rc);
		goto error;
	}
	irqrouter_ctrl->subdev.entity.revision =
		irqrouter_ctrl->subdev.devnode->num;
	atomic_set(&irqrouter_ctrl->active, 0);

	platform_set_drvdata(pdev, &irqrouter_ctrl->subdev);

	return rc;
error:
	kfree(irqrouter_ctrl);
	return rc;
}

static int __exit irqrouter_exit(struct platform_device *pdev)
{
	struct v4l2_subdev *subdev = dev_get_drvdata(&pdev->dev);
	struct irqrouter_ctrl_type *irqrouter_ctrl =
		v4l2_get_subdevdata(subdev);
	kfree(irqrouter_ctrl);
	return 0;
}

static const struct of_device_id msm_irqrouter_dt_match[] = {
	{.compatible = "qcom,irqrouter"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_irqrouter_dt_match);

static struct platform_driver msm_irqrouter_driver = {
	.probe = irqrouter_probe,
	.remove = irqrouter_exit,
	.driver = {
		.name = MSM_IRQ_ROUTER_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_irqrouter_dt_match,
	},
};

static int __init msm_irqrouter_init_module(void)
{
	return platform_driver_register(&msm_irqrouter_driver);
}

static void __exit msm_irqrouter_exit_module(void)
{
	platform_driver_unregister(&msm_irqrouter_driver);
}

module_init(msm_irqrouter_init_module);
module_exit(msm_irqrouter_exit_module);
MODULE_DESCRIPTION("msm camera irq router");
MODULE_LICENSE("GPL v2");
