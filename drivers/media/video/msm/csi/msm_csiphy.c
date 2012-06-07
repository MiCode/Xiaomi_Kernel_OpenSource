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
#include <mach/board.h>
#include <mach/camera.h>
#include <mach/vreg.h>
#include <media/msm_isp.h>
#include "msm_csiphy.h"
#include "msm.h"

#define DBG_CSIPHY 0

#define V4L2_IDENT_CSIPHY                        50003

/*MIPI CSI PHY registers*/
#define MIPI_CSIPHY_LNn_CFG1_ADDR                0x0
#define MIPI_CSIPHY_LNn_CFG2_ADDR                0x4
#define MIPI_CSIPHY_LNn_CFG3_ADDR                0x8
#define MIPI_CSIPHY_LNn_CFG4_ADDR                0xC
#define MIPI_CSIPHY_LNn_CFG5_ADDR                0x10
#define MIPI_CSIPHY_LNCK_CFG1_ADDR               0x100
#define MIPI_CSIPHY_LNCK_CFG2_ADDR               0x104
#define MIPI_CSIPHY_LNCK_CFG3_ADDR               0x108
#define MIPI_CSIPHY_LNCK_CFG4_ADDR               0x10C
#define MIPI_CSIPHY_LNCK_CFG5_ADDR               0x110
#define MIPI_CSIPHY_LNCK_MISC1_ADDR              0x128
#define MIPI_CSIPHY_GLBL_T_INIT_CFG0_ADDR        0x1E0
#define MIPI_CSIPHY_T_WAKEUP_CFG0_ADDR           0x1E8
#define MIPI_CSIPHY_T_WAKEUP_CFG1_ADDR           0x1EC
#define MIPI_CSIPHY_GLBL_RESET_ADDR             0x0140
#define MIPI_CSIPHY_GLBL_PWR_CFG_ADDR           0x0144
#define MIPI_CSIPHY_INTERRUPT_STATUS0_ADDR      0x0180
#define MIPI_CSIPHY_INTERRUPT_STATUS1_ADDR      0x0184
#define MIPI_CSIPHY_INTERRUPT_STATUS2_ADDR      0x0188
#define MIPI_CSIPHY_INTERRUPT_STATUS3_ADDR      0x018C
#define MIPI_CSIPHY_INTERRUPT_STATUS4_ADDR      0x0190
#define MIPI_CSIPHY_INTERRUPT_MASK0_ADDR        0x01A0
#define MIPI_CSIPHY_INTERRUPT_MASK1_ADDR        0x01A4
#define MIPI_CSIPHY_INTERRUPT_MASK2_ADDR        0x01A8
#define MIPI_CSIPHY_INTERRUPT_MASK3_ADDR        0x01AC
#define MIPI_CSIPHY_INTERRUPT_MASK4_ADDR        0x01B0
#define MIPI_CSIPHY_INTERRUPT_CLEAR0_ADDR       0x01C0
#define MIPI_CSIPHY_INTERRUPT_CLEAR1_ADDR       0x01C4
#define MIPI_CSIPHY_INTERRUPT_CLEAR2_ADDR       0x01C8
#define MIPI_CSIPHY_INTERRUPT_CLEAR3_ADDR       0x01CC
#define MIPI_CSIPHY_INTERRUPT_CLEAR4_ADDR       0x01D0

int msm_csiphy_config(struct csiphy_cfg_params *cfg_params)
{
	int rc = 0;
	int j = 0;
	uint32_t val = 0;
	uint8_t lane_cnt = 0, lane_mask = 0;
	struct csiphy_device *csiphy_dev;
	struct msm_camera_csiphy_params *csiphy_params;
	void __iomem *csiphybase;
	csiphy_dev = v4l2_get_subdevdata(cfg_params->subdev);
	csiphybase = csiphy_dev->base;
	if (csiphybase == NULL)
		return -ENOMEM;

	csiphy_params = cfg_params->parms;
	lane_mask = csiphy_params->lane_mask;
	lane_cnt = csiphy_params->lane_cnt;
	if (csiphy_params->lane_cnt < 1 || csiphy_params->lane_cnt > 4) {
		CDBG("%s: unsupported lane cnt %d\n",
			__func__, csiphy_params->lane_cnt);
		return rc;
	}

	val = 0x3;
	msm_camera_io_w((csiphy_params->lane_mask << 2) | val,
			csiphybase + MIPI_CSIPHY_GLBL_PWR_CFG_ADDR);
	msm_camera_io_w(0x1, csiphybase + MIPI_CSIPHY_GLBL_T_INIT_CFG0_ADDR);
	msm_camera_io_w(0x1, csiphybase + MIPI_CSIPHY_T_WAKEUP_CFG0_ADDR);

	while (lane_mask & 0xf) {
		if (!(lane_mask & 0x1)) {
			j++;
			lane_mask >>= 1;
			continue;
		}
		msm_camera_io_w(0x10,
			csiphybase + MIPI_CSIPHY_LNn_CFG2_ADDR + 0x40*j);
		msm_camera_io_w(csiphy_params->settle_cnt,
			csiphybase + MIPI_CSIPHY_LNn_CFG3_ADDR + 0x40*j);
		msm_camera_io_w(0x6F,
			csiphybase + MIPI_CSIPHY_INTERRUPT_MASK0_ADDR +
				0x4*(j+1));
		msm_camera_io_w(0x6F,
			csiphybase + MIPI_CSIPHY_INTERRUPT_CLEAR0_ADDR +
				0x4*(j+1));
		j++;
		lane_mask >>= 1;
	}

	msm_camera_io_w(0x10, csiphybase + MIPI_CSIPHY_LNCK_CFG2_ADDR);
	msm_camera_io_w(csiphy_params->settle_cnt,
			 csiphybase + MIPI_CSIPHY_LNCK_CFG3_ADDR);

	msm_camera_io_w(0x24,
		csiphybase + MIPI_CSIPHY_INTERRUPT_MASK0_ADDR);
	msm_camera_io_w(0x24,
		csiphybase + MIPI_CSIPHY_INTERRUPT_CLEAR0_ADDR);
	return rc;
}

static irqreturn_t msm_csiphy_irq(int irq_num, void *data)
{
	uint32_t irq;
	struct csiphy_device *csiphy_dev = data;

	irq = msm_camera_io_r(
		csiphy_dev->base + MIPI_CSIPHY_INTERRUPT_STATUS0_ADDR);
	msm_camera_io_w(irq,
		csiphy_dev->base + MIPI_CSIPHY_INTERRUPT_CLEAR0_ADDR);
	CDBG("%s MIPI_CSIPHY%d_INTERRUPT_STATUS0 = 0x%x\n",
		 __func__, csiphy_dev->pdev->id, irq);

	irq = msm_camera_io_r(
		csiphy_dev->base + MIPI_CSIPHY_INTERRUPT_STATUS1_ADDR);
	msm_camera_io_w(irq,
		csiphy_dev->base + MIPI_CSIPHY_INTERRUPT_CLEAR1_ADDR);
	CDBG("%s MIPI_CSIPHY%d_INTERRUPT_STATUS1 = 0x%x\n",
		__func__, csiphy_dev->pdev->id, irq);

	irq = msm_camera_io_r(
		csiphy_dev->base + MIPI_CSIPHY_INTERRUPT_STATUS2_ADDR);
	msm_camera_io_w(irq,
		csiphy_dev->base + MIPI_CSIPHY_INTERRUPT_CLEAR2_ADDR);
	CDBG("%s MIPI_CSIPHY%d_INTERRUPT_STATUS2 = 0x%x\n",
		__func__, csiphy_dev->pdev->id, irq);

	irq = msm_camera_io_r(
		csiphy_dev->base + MIPI_CSIPHY_INTERRUPT_STATUS3_ADDR);
	msm_camera_io_w(irq,
		csiphy_dev->base + MIPI_CSIPHY_INTERRUPT_CLEAR3_ADDR);
	CDBG("%s MIPI_CSIPHY%d_INTERRUPT_STATUS3 = 0x%x\n",
		__func__, csiphy_dev->pdev->id, irq);

	irq = msm_camera_io_r(
		csiphy_dev->base + MIPI_CSIPHY_INTERRUPT_STATUS4_ADDR);
	msm_camera_io_w(irq,
		csiphy_dev->base + MIPI_CSIPHY_INTERRUPT_CLEAR4_ADDR);
	CDBG("%s MIPI_CSIPHY%d_INTERRUPT_STATUS4 = 0x%x\n",
		__func__, csiphy_dev->pdev->id, irq);
	msm_camera_io_w(0x1, csiphy_dev->base + 0x164);
	msm_camera_io_w(0x0, csiphy_dev->base + 0x164);
	return IRQ_HANDLED;
}

static void msm_csiphy_reset(struct csiphy_device *csiphy_dev)
{
	msm_camera_io_w(0x1, csiphy_dev->base + MIPI_CSIPHY_GLBL_RESET_ADDR);
	usleep_range(5000, 8000);
	msm_camera_io_w(0x0, csiphy_dev->base + MIPI_CSIPHY_GLBL_RESET_ADDR);
}

static int msm_csiphy_subdev_g_chip_ident(struct v4l2_subdev *sd,
			struct v4l2_dbg_chip_ident *chip)
{
	BUG_ON(!chip);
	chip->ident = V4L2_IDENT_CSIPHY;
	chip->revision = 0;
	return 0;
}

static struct msm_cam_clk_info csiphy_clk_info[] = {
	{"csiphy_timer_src_clk", 177780000},
	{"csiphy_timer_clk", -1},
};

static int msm_csiphy_init(struct v4l2_subdev *sd)
{
	int rc = 0;
	struct csiphy_device *csiphy_dev;
	csiphy_dev = v4l2_get_subdevdata(sd);
	if (csiphy_dev == NULL) {
		rc = -ENOMEM;
		return rc;
	}

	csiphy_dev->base = ioremap(csiphy_dev->mem->start,
		resource_size(csiphy_dev->mem));
	if (!csiphy_dev->base) {
		rc = -ENOMEM;
		return rc;
	}

	rc = msm_cam_clk_enable(&csiphy_dev->pdev->dev, csiphy_clk_info,
			csiphy_dev->csiphy_clk, ARRAY_SIZE(csiphy_clk_info), 1);

	if (rc < 0) {
		iounmap(csiphy_dev->base);
		csiphy_dev->base = NULL;
		return rc;
	}

#if DBG_CSIPHY
	enable_irq(csiphy_dev->irq->start);
#endif
	msm_csiphy_reset(csiphy_dev);

	return 0;
}

static int msm_csiphy_release(struct v4l2_subdev *sd)
{
	struct csiphy_device *csiphy_dev;
	int i;
	csiphy_dev = v4l2_get_subdevdata(sd);
	for (i = 0; i < 4; i++)
		msm_camera_io_w(0x0, csiphy_dev->base +
		MIPI_CSIPHY_LNn_CFG2_ADDR + 0x40*i);

	msm_camera_io_w(0x0, csiphy_dev->base + MIPI_CSIPHY_LNCK_CFG2_ADDR);
	msm_camera_io_w(0x0, csiphy_dev->base + MIPI_CSIPHY_GLBL_PWR_CFG_ADDR);

#if DBG_CSIPHY
	disable_irq(csiphy_dev->irq->start);
#endif
	msm_cam_clk_enable(&csiphy_dev->pdev->dev, csiphy_clk_info,
		csiphy_dev->csiphy_clk, ARRAY_SIZE(csiphy_clk_info), 0);

	iounmap(csiphy_dev->base);
	csiphy_dev->base = NULL;
	return 0;
}

static long msm_csiphy_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc = -ENOIOCTLCMD;
	struct csiphy_cfg_params cfg_params;
	struct csiphy_device *csiphy_dev = v4l2_get_subdevdata(sd);
	mutex_lock(&csiphy_dev->mutex);
	switch (cmd) {
	case VIDIOC_MSM_CSIPHY_CFG:
		cfg_params.subdev = sd;
		cfg_params.parms = arg;
		rc = msm_csiphy_config(
			(struct csiphy_cfg_params *)&cfg_params);
		break;
	case VIDIOC_MSM_CSIPHY_INIT:
		rc = msm_csiphy_init(sd);
		break;
	case VIDIOC_MSM_CSIPHY_RELEASE:
		rc = msm_csiphy_release(sd);
		break;
	default:
		pr_err("%s: command not found\n", __func__);
	}
	mutex_unlock(&csiphy_dev->mutex);
	return rc;
}

static const struct v4l2_subdev_internal_ops msm_csiphy_internal_ops;

static struct v4l2_subdev_core_ops msm_csiphy_subdev_core_ops = {
	.g_chip_ident = &msm_csiphy_subdev_g_chip_ident,
	.ioctl = &msm_csiphy_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_csiphy_subdev_ops = {
	.core = &msm_csiphy_subdev_core_ops,
};

static int __devinit csiphy_probe(struct platform_device *pdev)
{
	struct csiphy_device *new_csiphy_dev;
	int rc = 0;
	CDBG("%s: device id = %d\n", __func__, pdev->id);
	new_csiphy_dev = kzalloc(sizeof(struct csiphy_device), GFP_KERNEL);
	if (!new_csiphy_dev) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}

	v4l2_subdev_init(&new_csiphy_dev->subdev, &msm_csiphy_subdev_ops);
	new_csiphy_dev->subdev.internal_ops = &msm_csiphy_internal_ops;
	new_csiphy_dev->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(new_csiphy_dev->subdev.name,
			ARRAY_SIZE(new_csiphy_dev->subdev.name), "msm_csiphy");
	v4l2_set_subdevdata(&new_csiphy_dev->subdev, new_csiphy_dev);
	platform_set_drvdata(pdev, &new_csiphy_dev->subdev);

	mutex_init(&new_csiphy_dev->mutex);

	new_csiphy_dev->mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "csiphy");
	if (!new_csiphy_dev->mem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto csiphy_no_resource;
	}
	new_csiphy_dev->irq = platform_get_resource_byname(pdev,
					IORESOURCE_IRQ, "csiphy");
	if (!new_csiphy_dev->irq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto csiphy_no_resource;
	}
	new_csiphy_dev->io = request_mem_region(new_csiphy_dev->mem->start,
		resource_size(new_csiphy_dev->mem), pdev->name);
	if (!new_csiphy_dev->io) {
		pr_err("%s: no valid mem region\n", __func__);
		rc = -EBUSY;
		goto csiphy_no_resource;
	}

	rc = request_irq(new_csiphy_dev->irq->start, msm_csiphy_irq,
		IRQF_TRIGGER_RISING, "csiphy", new_csiphy_dev);
	if (rc < 0) {
		release_mem_region(new_csiphy_dev->mem->start,
			resource_size(new_csiphy_dev->mem));
		pr_err("%s: irq request fail\n", __func__);
		rc = -EBUSY;
		goto csiphy_no_resource;
	}
	disable_irq(new_csiphy_dev->irq->start);

	new_csiphy_dev->pdev = pdev;
	msm_cam_register_subdev_node(
		&new_csiphy_dev->subdev, CSIPHY_DEV, pdev->id);
	return 0;

csiphy_no_resource:
	mutex_destroy(&new_csiphy_dev->mutex);
	kfree(new_csiphy_dev);
	return 0;
}

static struct platform_driver csiphy_driver = {
	.probe = csiphy_probe,
	.driver = {
		.name = MSM_CSIPHY_DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init msm_csiphy_init_module(void)
{
	return platform_driver_register(&csiphy_driver);
}

static void __exit msm_csiphy_exit_module(void)
{
	platform_driver_unregister(&csiphy_driver);
}

module_init(msm_csiphy_init_module);
module_exit(msm_csiphy_exit_module);
MODULE_DESCRIPTION("MSM CSIPHY driver");
MODULE_LICENSE("GPL v2");
