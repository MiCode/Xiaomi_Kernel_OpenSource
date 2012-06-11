/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <media/msm_isp.h>
#include "msm_csid.h"
#include "msm_csid_hwreg.h"
#include "msm.h"

#define V4L2_IDENT_CSID                            50002

#define DBG_CSID 0

static int msm_csid_cid_lut(
	struct msm_camera_csid_lut_params *csid_lut_params,
	void __iomem *csidbase)
{
	int rc = 0, i = 0;
	uint32_t val = 0;

	for (i = 0; i < csid_lut_params->num_cid && i < 16; i++) {
		if (csid_lut_params->vc_cfg[i].dt < 0x12 ||
			csid_lut_params->vc_cfg[i].dt > 0x37) {
			CDBG("%s: unsupported data type 0x%x\n",
				 __func__, csid_lut_params->vc_cfg[i].dt);
			return rc;
		}
		val = msm_camera_io_r(csidbase + CSID_CID_LUT_VC_0_ADDR +
			(csid_lut_params->vc_cfg[i].cid >> 2) * 4)
			& ~(0xFF << ((csid_lut_params->vc_cfg[i].cid % 4) * 8));
		val |= (csid_lut_params->vc_cfg[i].dt <<
			((csid_lut_params->vc_cfg[i].cid % 4) * 8));
		msm_camera_io_w(val, csidbase + CSID_CID_LUT_VC_0_ADDR +
			(csid_lut_params->vc_cfg[i].cid >> 2) * 4);
		val = (csid_lut_params->vc_cfg[i].decode_format << 4) | 0x3;
		msm_camera_io_w(val, csidbase + CSID_CID_n_CFG_ADDR +
			(csid_lut_params->vc_cfg[i].cid * 4));
	}
	return rc;
}

#if DBG_CSID
static void msm_csid_set_debug_reg(void __iomem *csidbase,
	struct msm_camera_csid_params *csid_params)
{
	uint32_t val = 0;
	val = ((1 << csid_params->lane_cnt) - 1) << 20;
	msm_camera_io_w(0x7f010800 | val, csidbase + CSID_IRQ_MASK_ADDR);
	msm_camera_io_w(0x7f010800 | val, csidbase + CSID_IRQ_CLEAR_CMD_ADDR);
}
#else
static void msm_csid_set_debug_reg(void __iomem *csidbase,
	struct msm_camera_csid_params *csid_params) {}
#endif

static int msm_csid_config(struct csid_cfg_params *cfg_params)
{
	int rc = 0;
	uint32_t val = 0;
	struct csid_device *csid_dev;
	struct msm_camera_csid_params *csid_params;
	void __iomem *csidbase;
	csid_dev = v4l2_get_subdevdata(cfg_params->subdev);
	csidbase = csid_dev->base;
	if (csidbase == NULL)
		return -ENOMEM;
	csid_params = cfg_params->parms;

	val = csid_params->lane_cnt - 1;
	val |= csid_params->lane_assign << CSID_DL_INPUT_SEL_SHIFT;
	if (csid_dev->hw_version < 0x30000000) {
		val |= (0xF << 10);
		msm_camera_io_w(val, csidbase + CSID_CORE_CTRL_0_ADDR);
	} else {
		msm_camera_io_w(val, csidbase + CSID_CORE_CTRL_0_ADDR);
		val = csid_params->phy_sel << CSID_PHY_SEL_SHIFT;
		val |= 0xF;
		msm_camera_io_w(val, csidbase + CSID_CORE_CTRL_1_ADDR);
	}

	rc = msm_csid_cid_lut(&csid_params->lut_params, csidbase);
	if (rc < 0)
		return rc;

	msm_csid_set_debug_reg(csidbase, csid_params);

	return rc;
}

static irqreturn_t msm_csid_irq(int irq_num, void *data)
{
	uint32_t irq;
	struct csid_device *csid_dev = data;
	irq = msm_camera_io_r(csid_dev->base + CSID_IRQ_STATUS_ADDR);
	CDBG("%s CSID%d_IRQ_STATUS_ADDR = 0x%x\n",
		 __func__, csid_dev->pdev->id, irq);
	if (irq & (0x1 << CSID_RST_DONE_IRQ_BITSHIFT))
			complete(&csid_dev->reset_complete);
	msm_camera_io_w(irq, csid_dev->base + CSID_IRQ_CLEAR_CMD_ADDR);
	return IRQ_HANDLED;
}

static void msm_csid_reset(struct csid_device *csid_dev)
{
	msm_camera_io_w(CSID_RST_STB_ALL, csid_dev->base + CSID_RST_CMD_ADDR);
	wait_for_completion_interruptible(&csid_dev->reset_complete);
	return;
}

static int msm_csid_subdev_g_chip_ident(struct v4l2_subdev *sd,
			struct v4l2_dbg_chip_ident *chip)
{
	BUG_ON(!chip);
	chip->ident = V4L2_IDENT_CSID;
	chip->revision = 0;
	return 0;
}

static struct msm_cam_clk_info csid_clk_info[] = {
	{"csi_src_clk", 177780000},
	{"csi_clk", -1},
	{"csi_phy_clk", -1},
	{"csi_pclk", -1},
};

static struct camera_vreg_t csid_vreg_info[] = {
	{"mipi_csi_vdd", REG_LDO, 1200000, 1200000, 20000},
};

static int msm_csid_init(struct v4l2_subdev *sd, uint32_t *csid_version)
{
	int rc = 0;
	struct csid_device *csid_dev;
	csid_dev = v4l2_get_subdevdata(sd);
	if (csid_dev == NULL) {
		rc = -ENOMEM;
		return rc;
	}

	csid_dev->base = ioremap(csid_dev->mem->start,
		resource_size(csid_dev->mem));
	if (!csid_dev->base) {
		rc = -ENOMEM;
		return rc;
	}

	rc = msm_camera_config_vreg(&csid_dev->pdev->dev, csid_vreg_info,
		ARRAY_SIZE(csid_vreg_info), &csid_dev->csi_vdd, 1);
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto vreg_config_failed;
	}

	rc = msm_camera_enable_vreg(&csid_dev->pdev->dev, csid_vreg_info,
		ARRAY_SIZE(csid_vreg_info), &csid_dev->csi_vdd, 1);
	if (rc < 0) {
		pr_err("%s: regulator enable failed\n", __func__);
		goto vreg_enable_failed;
	}

	rc = msm_cam_clk_enable(&csid_dev->pdev->dev, csid_clk_info,
		csid_dev->csid_clk, ARRAY_SIZE(csid_clk_info), 1);
	if (rc < 0) {
		pr_err("%s: regulator enable failed\n", __func__);
		goto clk_enable_failed;
	}

	csid_dev->hw_version =
		msm_camera_io_r(csid_dev->base + CSID_HW_VERSION_ADDR);
	*csid_version = csid_dev->hw_version;

	init_completion(&csid_dev->reset_complete);

	rc = request_irq(csid_dev->irq->start, msm_csid_irq,
		IRQF_TRIGGER_RISING, "csid", csid_dev);

	msm_csid_reset(csid_dev);
	return rc;

clk_enable_failed:
	msm_camera_enable_vreg(&csid_dev->pdev->dev, csid_vreg_info,
		ARRAY_SIZE(csid_vreg_info), &csid_dev->csi_vdd, 0);
vreg_enable_failed:
	msm_camera_config_vreg(&csid_dev->pdev->dev, csid_vreg_info,
		ARRAY_SIZE(csid_vreg_info), &csid_dev->csi_vdd, 0);
vreg_config_failed:
	iounmap(csid_dev->base);
	csid_dev->base = NULL;
	return rc;
}

static int msm_csid_release(struct v4l2_subdev *sd)
{
	uint32_t irq;
	struct csid_device *csid_dev;
	csid_dev = v4l2_get_subdevdata(sd);

	irq = msm_camera_io_r(csid_dev->base + CSID_IRQ_STATUS_ADDR);
	msm_camera_io_w(irq, csid_dev->base + CSID_IRQ_CLEAR_CMD_ADDR);
	msm_camera_io_w(0, csid_dev->base + CSID_IRQ_MASK_ADDR);

	free_irq(csid_dev->irq->start, csid_dev);

	msm_cam_clk_enable(&csid_dev->pdev->dev, csid_clk_info,
		csid_dev->csid_clk, ARRAY_SIZE(csid_clk_info), 0);

	msm_camera_enable_vreg(&csid_dev->pdev->dev, csid_vreg_info,
		ARRAY_SIZE(csid_vreg_info), &csid_dev->csi_vdd, 0);

	msm_camera_config_vreg(&csid_dev->pdev->dev, csid_vreg_info,
		ARRAY_SIZE(csid_vreg_info), &csid_dev->csi_vdd, 0);

	iounmap(csid_dev->base);
	csid_dev->base = NULL;
	return 0;
}

static long msm_csid_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc = -ENOIOCTLCMD;
	struct csid_cfg_params cfg_params;
	struct csid_device *csid_dev = v4l2_get_subdevdata(sd);
	mutex_lock(&csid_dev->mutex);
	switch (cmd) {
	case VIDIOC_MSM_CSID_CFG:
		cfg_params.subdev = sd;
		cfg_params.parms = arg;
		rc = msm_csid_config((struct csid_cfg_params *)&cfg_params);
		break;
	case VIDIOC_MSM_CSID_INIT:
		rc = msm_csid_init(sd, (uint32_t *)arg);
		break;
	case VIDIOC_MSM_CSID_RELEASE:
		rc = msm_csid_release(sd);
		break;
	default:
		pr_err("%s: command not found\n", __func__);
	}
	mutex_unlock(&csid_dev->mutex);
	return rc;
}

static const struct v4l2_subdev_internal_ops msm_csid_internal_ops;

static struct v4l2_subdev_core_ops msm_csid_subdev_core_ops = {
	.g_chip_ident = &msm_csid_subdev_g_chip_ident,
	.ioctl = &msm_csid_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_csid_subdev_ops = {
	.core = &msm_csid_subdev_core_ops,
};

static int __devinit csid_probe(struct platform_device *pdev)
{
	struct csid_device *new_csid_dev;
	struct msm_cam_subdev_info sd_info;

	int rc = 0;
	CDBG("%s: device id = %d\n", __func__, pdev->id);
	new_csid_dev = kzalloc(sizeof(struct csid_device), GFP_KERNEL);
	if (!new_csid_dev) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}

	v4l2_subdev_init(&new_csid_dev->subdev, &msm_csid_subdev_ops);
	new_csid_dev->subdev.internal_ops = &msm_csid_internal_ops;
	new_csid_dev->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(new_csid_dev->subdev.name,
			ARRAY_SIZE(new_csid_dev->subdev.name), "msm_csid");
	v4l2_set_subdevdata(&new_csid_dev->subdev, new_csid_dev);
	platform_set_drvdata(pdev, &new_csid_dev->subdev);
	mutex_init(&new_csid_dev->mutex);

	if (pdev->dev.of_node)
		of_property_read_u32((&pdev->dev)->of_node,
			"cell-index", &pdev->id);

	new_csid_dev->mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "csid");
	if (!new_csid_dev->mem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto csid_no_resource;
	}
	new_csid_dev->irq = platform_get_resource_byname(pdev,
					IORESOURCE_IRQ, "csid");
	if (!new_csid_dev->irq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto csid_no_resource;
	}
	new_csid_dev->io = request_mem_region(new_csid_dev->mem->start,
		resource_size(new_csid_dev->mem), pdev->name);
	if (!new_csid_dev->io) {
		pr_err("%s: no valid mem region\n", __func__);
		rc = -EBUSY;
		goto csid_no_resource;
	}

	new_csid_dev->pdev = pdev;
	sd_info.sdev_type = CSID_DEV;
	sd_info.sd_index = pdev->id;
	sd_info.irq_num = new_csid_dev->irq->start;
	msm_cam_register_subdev_node(&new_csid_dev->subdev, &sd_info);

	media_entity_init(&new_csid_dev->subdev.entity, 0, NULL, 0);
	new_csid_dev->subdev.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	new_csid_dev->subdev.entity.group_id = CSID_DEV;
	new_csid_dev->subdev.entity.name = pdev->name;
	new_csid_dev->subdev.entity.revision =
		new_csid_dev->subdev.devnode->num;
	return 0;

csid_no_resource:
	mutex_destroy(&new_csid_dev->mutex);
	kfree(new_csid_dev);
	return 0;
}

static const struct of_device_id msm_csid_dt_match[] = {
	{.compatible = "qcom,csid"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_csid_dt_match);

static struct platform_driver csid_driver = {
	.probe = csid_probe,
	.driver = {
		.name = MSM_CSID_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_csid_dt_match,
	},
};

static int __init msm_csid_init_module(void)
{
	return platform_driver_register(&csid_driver);
}

static void __exit msm_csid_exit_module(void)
{
	platform_driver_unregister(&csid_driver);
}

module_init(msm_csid_init_module);
module_exit(msm_csid_exit_module);
MODULE_DESCRIPTION("MSM CSID driver");
MODULE_LICENSE("GPL v2");
