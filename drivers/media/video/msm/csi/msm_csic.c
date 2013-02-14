/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <mach/clk.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <media/msm_isp.h>
#include "msm_csic.h"
#include "msm.h"

#define DBG_CSIC 0

#define V4L2_IDENT_CSIC			50004
/* MIPI	CSI controller registers */
#define	MIPI_PHY_CONTROL		0x00000000
#define	MIPI_PROTOCOL_CONTROL		0x00000004
#define	MIPI_INTERRUPT_STATUS		0x00000008
#define	MIPI_INTERRUPT_MASK		0x0000000C
#define	MIPI_CAMERA_CNTL		0x00000024
#define	MIPI_CALIBRATION_CONTROL	0x00000018
#define	MIPI_PHY_D0_CONTROL2		0x00000038
#define	MIPI_PHY_D1_CONTROL2		0x0000003C
#define	MIPI_PHY_D2_CONTROL2		0x00000040
#define	MIPI_PHY_D3_CONTROL2		0x00000044
#define	MIPI_PHY_CL_CONTROL		0x00000048
#define	MIPI_PHY_D0_CONTROL		0x00000034
#define	MIPI_PHY_D1_CONTROL		0x00000020
#define	MIPI_PHY_D2_CONTROL		0x0000002C
#define	MIPI_PHY_D3_CONTROL		0x00000030
#define	MIPI_PWR_CNTL			0x00000054

/*
 * MIPI_PROTOCOL_CONTROL register bits to enable/disable the features of
 * CSI Rx Block
 */

/* DPCM scheme */
#define	MIPI_PROTOCOL_CONTROL_DPCM_SCHEME_SHFT			0x1e
/* SW_RST to issue a SW reset to the CSI core */
#define	MIPI_PROTOCOL_CONTROL_SW_RST_BMSK			0x8000000
/* To Capture Long packet Header Info in MIPI_PROTOCOL_STATUS register */
#define	MIPI_PROTOCOL_CONTROL_LONG_PACKET_HEADER_CAPTURE_BMSK	0x200000
/* Data format for unpacking purpose */
#define	MIPI_PROTOCOL_CONTROL_DATA_FORMAT_SHFT			0x13
/* Enable decoding of payload based on data type filed of packet hdr */
#define	MIPI_PROTOCOL_CONTROL_DECODE_ID_BMSK			0x40000
/* Enable error correction on packet headers */
#define	MIPI_PROTOCOL_CONTROL_ECC_EN_BMSK			0x20000

/*
 * MIPI_CALIBRATION_CONTROL register contains control info for
 * calibration impledence controller
*/

/* Enable bit for calibration pad */
#define	MIPI_CALIBRATION_CONTROL_SWCAL_CAL_EN_SHFT		0x16
/* With SWCAL_STRENGTH_OVERRIDE_EN, SW_CAL_EN and MANUAL_OVERRIDE_EN
 * the hardware calibration circuitry associated with CAL_SW_HW_MODE
 * is bypassed
*/
#define	MIPI_CALIBRATION_CONTROL_SWCAL_STRENGTH_OVERRIDE_EN_SHFT	0x15
/* To indicate the Calibration process is in the control of HW/SW */
#define	MIPI_CALIBRATION_CONTROL_CAL_SW_HW_MODE_SHFT		0x14
/* When this is set the strength value of the data and clk lane impedence
 * termination is updated with MANUAL_STRENGTH settings and calibration
 * sensing logic is idle.
*/
#define	MIPI_CALIBRATION_CONTROL_MANUAL_OVERRIDE_EN_SHFT	0x7

/* Data lane0 control */
/* T-hs Settle count value  for Rx */
#define	MIPI_PHY_D0_CONTROL2_SETTLE_COUNT_SHFT			0x18
/* Rx termination control */
#define	MIPI_PHY_D0_CONTROL2_HS_TERM_IMP_SHFT			0x10
/* LP Rx enable */
#define	MIPI_PHY_D0_CONTROL2_LP_REC_EN_SHFT			0x4
/*
 * Enable for error in sync sequence
 * 1 - one bit error in sync seq
 * 0 - requires all 8 bit correct seq
*/
#define	MIPI_PHY_D0_CONTROL2_ERR_SOT_HS_EN_SHFT			0x3

/* Comments are same as D0 */
#define	MIPI_PHY_D1_CONTROL2_SETTLE_COUNT_SHFT			0x18
#define	MIPI_PHY_D1_CONTROL2_HS_TERM_IMP_SHFT			0x10
#define	MIPI_PHY_D1_CONTROL2_LP_REC_EN_SHFT			0x4
#define	MIPI_PHY_D1_CONTROL2_ERR_SOT_HS_EN_SHFT			0x3

/* Comments are same as D0 */
#define	MIPI_PHY_D2_CONTROL2_SETTLE_COUNT_SHFT			0x18
#define	MIPI_PHY_D2_CONTROL2_HS_TERM_IMP_SHFT			0x10
#define	MIPI_PHY_D2_CONTROL2_LP_REC_EN_SHFT			0x4
#define	MIPI_PHY_D2_CONTROL2_ERR_SOT_HS_EN_SHFT			0x3

/* Comments are same as D0 */
#define	MIPI_PHY_D3_CONTROL2_SETTLE_COUNT_SHFT			0x18
#define	MIPI_PHY_D3_CONTROL2_HS_TERM_IMP_SHFT			0x10
#define	MIPI_PHY_D3_CONTROL2_LP_REC_EN_SHFT			0x4
#define	MIPI_PHY_D3_CONTROL2_ERR_SOT_HS_EN_SHFT			0x3

/* PHY_CL_CTRL programs the parameters of clk lane of CSIRXPHY */
/* HS Rx termination control */
#define	MIPI_PHY_CL_CONTROL_HS_TERM_IMP_SHFT			0x18
/* Start signal for T-hs delay */
#define	MIPI_PHY_CL_CONTROL_LP_REC_EN_SHFT			0x2

/* PHY DATA lane 0 control */
/*
 * HS RX equalizer strength control
 * 00 - 0db 01 - 3db 10 - 5db 11 - 7db
*/
#define	MIPI_PHY_D0_CONTROL_HS_REC_EQ_SHFT			0x1c
/* PHY DATA lane 1 control */
/* Shutdown signal for MIPI clk phy line */
#define	MIPI_PHY_D1_CONTROL_MIPI_CLK_PHY_SHUTDOWNB_SHFT		0x9
/* Shutdown signal for MIPI data phy line */
#define	MIPI_PHY_D1_CONTROL_MIPI_DATA_PHY_SHUTDOWNB_SHFT	0x8

#define MSM_AXI_QOS_PREVIEW 200000
#define MSM_AXI_QOS_SNAPSHOT 200000
#define MSM_AXI_QOS_RECORDING 200000

#define MIPI_PWR_CNTL_EN	0x07
#define MIPI_PWR_CNTL_DIS	0x0

static int msm_csic_config(struct v4l2_subdev *sd,
	struct msm_camera_csi_params *csic_params)
{
	int rc = 0;
	uint32_t val = 0;
	struct csic_device *csic_dev;
	void __iomem *csicbase;
	int i;

	csic_dev = v4l2_get_subdevdata(sd);
	csicbase = csic_dev->base;

	/* Enable error correction for DATA lane. Applies to all data lanes */
	msm_camera_io_w(0x4, csicbase + MIPI_PHY_CONTROL);

	msm_camera_io_w(MIPI_PROTOCOL_CONTROL_SW_RST_BMSK,
		csicbase + MIPI_PROTOCOL_CONTROL);

	val = MIPI_PROTOCOL_CONTROL_LONG_PACKET_HEADER_CAPTURE_BMSK |
		MIPI_PROTOCOL_CONTROL_DECODE_ID_BMSK |
		MIPI_PROTOCOL_CONTROL_ECC_EN_BMSK;
	val |= (uint32_t)(csic_params->data_format) <<
		MIPI_PROTOCOL_CONTROL_DATA_FORMAT_SHFT;
	val |= csic_params->dpcm_scheme <<
		MIPI_PROTOCOL_CONTROL_DPCM_SCHEME_SHFT;
	CDBG("%s MIPI_PROTOCOL_CONTROL val=0x%x\n", __func__, val);
	msm_camera_io_w(val, csicbase + MIPI_PROTOCOL_CONTROL);

	val = (csic_params->settle_cnt <<
		MIPI_PHY_D0_CONTROL2_SETTLE_COUNT_SHFT) |
		(0x0F << MIPI_PHY_D0_CONTROL2_HS_TERM_IMP_SHFT) |
		(0x1 << MIPI_PHY_D0_CONTROL2_LP_REC_EN_SHFT) |
		(0x1 << MIPI_PHY_D0_CONTROL2_ERR_SOT_HS_EN_SHFT);
	CDBG("%s MIPI_PHY_D0_CONTROL2 val=0x%x\n", __func__, val);
	for (i = 0; i < csic_params->lane_cnt; i++)
		msm_camera_io_w(val, csicbase + MIPI_PHY_D0_CONTROL2 + i * 4);


	val = (0x0F << MIPI_PHY_CL_CONTROL_HS_TERM_IMP_SHFT) |
		(0x1 << MIPI_PHY_CL_CONTROL_LP_REC_EN_SHFT);
	CDBG("%s MIPI_PHY_CL_CONTROL val=0x%x\n", __func__, val);
	msm_camera_io_w(val, csicbase + MIPI_PHY_CL_CONTROL);

	val = 0 << MIPI_PHY_D0_CONTROL_HS_REC_EQ_SHFT;
	msm_camera_io_w(val, csicbase + MIPI_PHY_D0_CONTROL);

	val = (0x1 << MIPI_PHY_D1_CONTROL_MIPI_CLK_PHY_SHUTDOWNB_SHFT) |
		(0x1 << MIPI_PHY_D1_CONTROL_MIPI_DATA_PHY_SHUTDOWNB_SHFT);
	CDBG("%s MIPI_PHY_D1_CONTROL val=0x%x\n", __func__, val);
	msm_camera_io_w(val, csicbase + MIPI_PHY_D1_CONTROL);

	msm_camera_io_w(0x00000000, csicbase + MIPI_PHY_D2_CONTROL);
	msm_camera_io_w(0x00000000, csicbase + MIPI_PHY_D3_CONTROL);

	/* program number of lanes and lane mapping */
	switch (csic_params->lane_cnt) {
	case 1:
		msm_camera_io_w(csic_params->lane_assign << 8 | 0x4,
			csicbase + MIPI_CAMERA_CNTL);
		break;
	case 2:
		msm_camera_io_w(csic_params->lane_assign << 8 | 0x5,
			csicbase + MIPI_CAMERA_CNTL);
		break;
	case 3:
		msm_camera_io_w(csic_params->lane_assign << 8 | 0x6,
			csicbase + MIPI_CAMERA_CNTL);
		break;
	case 4:
		msm_camera_io_w(csic_params->lane_assign << 8 | 0x7,
			csicbase + MIPI_CAMERA_CNTL);
		break;
	}

	msm_camera_io_w(0xF077F3C0, csicbase + MIPI_INTERRUPT_MASK);
	/*clear IRQ bits*/
	msm_camera_io_w(0xF077F3C0, csicbase + MIPI_INTERRUPT_STATUS);

	return rc;
}

static irqreturn_t msm_csic_irq(int irq_num, void *data)
{
	uint32_t irq;
	struct csic_device *csic_dev = data;

	pr_info("msm_csic_irq: %x\n", (unsigned int)csic_dev->base);
	irq = msm_camera_io_r(csic_dev->base + MIPI_INTERRUPT_STATUS);
	pr_info("%s MIPI_INTERRUPT_STATUS = 0x%x 0x%x\n",
		__func__, irq,
		msm_camera_io_r(csic_dev->base + MIPI_PROTOCOL_CONTROL));
	msm_camera_io_w(irq, csic_dev->base + MIPI_INTERRUPT_STATUS);

	/* TODO: Needs to send this info to upper layers */
	if ((irq >> 19) & 0x1)
		pr_info("Unsupported packet format is received\n");
	return IRQ_HANDLED;
}

static int msm_csic_subdev_g_chip_ident(struct v4l2_subdev *sd,
			struct v4l2_dbg_chip_ident *chip)
{
	BUG_ON(!chip);
	chip->ident = V4L2_IDENT_CSIC;
	chip->revision = 0;
	return 0;
}

static struct msm_cam_clk_info csic_8x_clk_info[] = {
	{"csi_src_clk", 384000000},
	{"csi_clk", -1},
	{"csi_vfe_clk", -1},
	{"csi_pclk", -1},
};

static struct msm_cam_clk_info csic_7x_clk_info[] = {
	{"csi_clk", 400000000},
	{"csi_vfe_clk", -1},
	{"csi_pclk", -1},
};

static int msm_csic_init(struct v4l2_subdev *sd)
{
	int rc = 0;
	struct csic_device *csic_dev;
	csic_dev = v4l2_get_subdevdata(sd);
	if (csic_dev == NULL) {
		rc = -ENOMEM;
		return rc;
	}

	csic_dev->base = ioremap(csic_dev->mem->start,
		resource_size(csic_dev->mem));
	if (!csic_dev->base) {
		rc = -ENOMEM;
		return rc;
	}

	csic_dev->hw_version = CSIC_8X;
	rc = msm_cam_clk_enable(&csic_dev->pdev->dev, csic_8x_clk_info,
		csic_dev->csic_clk, ARRAY_SIZE(csic_8x_clk_info), 1);
	if (rc < 0) {
		csic_dev->hw_version = CSIC_7X;
		rc = msm_cam_clk_enable(&csic_dev->pdev->dev, csic_7x_clk_info,
			csic_dev->csic_clk, ARRAY_SIZE(csic_7x_clk_info), 1);
		if (rc < 0) {
			csic_dev->hw_version = 0;
			iounmap(csic_dev->base);
			csic_dev->base = NULL;
			return rc;
		}
	}
	if (csic_dev->hw_version == CSIC_7X)
		msm_camio_vfe_blk_reset_3();

#if DBG_CSIC
	enable_irq(csic_dev->irq->start);
#endif

	return 0;
}

static void msm_csic_disable(struct v4l2_subdev *sd)
{
	uint32_t val;
	struct csic_device *csic_dev;
	csic_dev = v4l2_get_subdevdata(sd);

	val = 0x0;
	if (csic_dev->base != NULL) {
		CDBG("%s MIPI_PHY_D0_CONTROL2 val=0x%x\n", __func__, val);
		msm_camera_io_w(val, csic_dev->base + MIPI_PHY_D0_CONTROL2);
		msm_camera_io_w(val, csic_dev->base + MIPI_PHY_D1_CONTROL2);
		msm_camera_io_w(val, csic_dev->base + MIPI_PHY_D2_CONTROL2);
		msm_camera_io_w(val, csic_dev->base + MIPI_PHY_D3_CONTROL2);
		CDBG("%s MIPI_PHY_CL_CONTROL val=0x%x\n", __func__, val);
		msm_camera_io_w(val, csic_dev->base + MIPI_PHY_CL_CONTROL);
		msleep(20);
		val = msm_camera_io_r(csic_dev->base + MIPI_PHY_D1_CONTROL);
		val &=
		~((0x1 << MIPI_PHY_D1_CONTROL_MIPI_CLK_PHY_SHUTDOWNB_SHFT)
		|(0x1 << MIPI_PHY_D1_CONTROL_MIPI_DATA_PHY_SHUTDOWNB_SHFT));
		CDBG("%s MIPI_PHY_D1_CONTROL val=0x%x\n", __func__, val);
		msm_camera_io_w(val, csic_dev->base + MIPI_PHY_D1_CONTROL);
		usleep_range(5000, 6000);
		msm_camera_io_w(0x0, csic_dev->base + MIPI_INTERRUPT_MASK);
		msm_camera_io_w(0x0, csic_dev->base + MIPI_INTERRUPT_STATUS);
		msm_camera_io_w(MIPI_PROTOCOL_CONTROL_SW_RST_BMSK,
			csic_dev->base + MIPI_PROTOCOL_CONTROL);

		msm_camera_io_w(0xE400, csic_dev->base + MIPI_CAMERA_CNTL);
	}
}

static int msm_csic_release(struct v4l2_subdev *sd)
{
	struct csic_device *csic_dev;
	csic_dev = v4l2_get_subdevdata(sd);

	msm_csic_disable(sd);
#if DBG_CSIC
	disable_irq(csic_dev->irq->start);
#endif

	if (csic_dev->hw_version == CSIC_8X) {
		msm_cam_clk_enable(&csic_dev->pdev->dev, csic_8x_clk_info,
			csic_dev->csic_clk, ARRAY_SIZE(csic_8x_clk_info), 0);
	} else if (csic_dev->hw_version == CSIC_7X) {
		msm_cam_clk_enable(&csic_dev->pdev->dev, csic_7x_clk_info,
			csic_dev->csic_clk, ARRAY_SIZE(csic_7x_clk_info), 0);
	}

	iounmap(csic_dev->base);
	csic_dev->base = NULL;
	return 0;
}

static long msm_csic_cmd(struct v4l2_subdev *sd, void *arg)
{
	long rc = 0;
	struct csic_cfg_data cdata;
	struct msm_camera_csi_params csic_params;
	if (copy_from_user(&cdata,
		(void *)arg,
		sizeof(struct csic_cfg_data)))
		return -EFAULT;
	CDBG("%s cfgtype = %d\n", __func__, cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CSIC_INIT:
		rc = msm_csic_init(sd);
		break;
	case CSIC_CFG:
		if (copy_from_user(&csic_params,
			(void *)cdata.csic_params,
			sizeof(struct msm_camera_csi_params)))
			return -EFAULT;
		rc = msm_csic_config(sd, &csic_params);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static long msm_csic_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	switch (cmd) {
	case VIDIOC_MSM_CSIC_CFG:
		return msm_csic_cmd(sd, arg);
	case VIDIOC_MSM_CSIC_RELEASE:
		return msm_csic_release(sd);
	default:
		return -ENOIOCTLCMD;
	}
}

static struct v4l2_subdev_core_ops msm_csic_subdev_core_ops = {
	.g_chip_ident = &msm_csic_subdev_g_chip_ident,
	.ioctl = &msm_csic_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_csic_subdev_ops = {
	.core = &msm_csic_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_csic_internal_ops;

static int __devinit csic_probe(struct platform_device *pdev)
{
	struct csic_device *new_csic_dev;
	int rc = 0;
	struct msm_cam_subdev_info sd_info;

	CDBG("%s: device id = %d\n", __func__, pdev->id);
	new_csic_dev = kzalloc(sizeof(struct csic_device), GFP_KERNEL);
	if (!new_csic_dev) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}

	v4l2_subdev_init(&new_csic_dev->subdev, &msm_csic_subdev_ops);
	new_csic_dev->subdev.internal_ops = &msm_csic_internal_ops;
	new_csic_dev->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(new_csic_dev->subdev.name,
			ARRAY_SIZE(new_csic_dev->subdev.name), "msm_csic");
	v4l2_set_subdevdata(&new_csic_dev->subdev, new_csic_dev);
	platform_set_drvdata(pdev, &new_csic_dev->subdev);
	mutex_init(&new_csic_dev->mutex);

	new_csic_dev->mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "csic");
	if (!new_csic_dev->mem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto csic_no_resource;
	}
	new_csic_dev->irq = platform_get_resource_byname(pdev,
					IORESOURCE_IRQ, "csic");
	if (!new_csic_dev->irq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto csic_no_resource;
	}
	new_csic_dev->io = request_mem_region(new_csic_dev->mem->start,
		resource_size(new_csic_dev->mem), pdev->name);
	if (!new_csic_dev->io) {
		pr_err("%s: no valid mem region\n", __func__);
		rc = -EBUSY;
		goto csic_no_resource;
	}

	rc = request_irq(new_csic_dev->irq->start, msm_csic_irq,
		IRQF_TRIGGER_HIGH, "csic", new_csic_dev);
	if (rc < 0) {
		release_mem_region(new_csic_dev->mem->start,
			resource_size(new_csic_dev->mem));
		pr_err("%s: irq request fail\n", __func__);
		rc = -EBUSY;
		goto csic_no_resource;
	}
	disable_irq(new_csic_dev->irq->start);

	new_csic_dev->pdev = pdev;

	rc = msm_cam_clk_enable(&new_csic_dev->pdev->dev, &csic_7x_clk_info[2],
				new_csic_dev->csic_clk, 1, 1);
	new_csic_dev->base = ioremap(new_csic_dev->mem->start,
		resource_size(new_csic_dev->mem));
	if (!new_csic_dev->base) {
		rc = -ENOMEM;
		return rc;
	}

	msm_camera_io_w(MIPI_PWR_CNTL_DIS, new_csic_dev->base + MIPI_PWR_CNTL);

	rc = msm_cam_clk_enable(&new_csic_dev->pdev->dev, &csic_7x_clk_info[2],
				new_csic_dev->csic_clk, 1, 0);

	iounmap(new_csic_dev->base);
	new_csic_dev->base = NULL;
	sd_info.sdev_type = CSIC_DEV;
	sd_info.sd_index = pdev->id;
	sd_info.irq_num = new_csic_dev->irq->start;
	msm_cam_register_subdev_node(
		&new_csic_dev->subdev, &sd_info);

	media_entity_init(&new_csic_dev->subdev.entity, 0, NULL, 0);
	new_csic_dev->subdev.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	new_csic_dev->subdev.entity.group_id = CSIC_DEV;
	new_csic_dev->subdev.entity.name = pdev->name;
	new_csic_dev->subdev.entity.revision =
		new_csic_dev->subdev.devnode->num;
	return 0;

csic_no_resource:
	mutex_destroy(&new_csic_dev->mutex);
	kfree(new_csic_dev);
	return 0;
}

static struct platform_driver csic_driver = {
	.probe = csic_probe,
	.driver = {
		.name = MSM_CSIC_DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init msm_csic_init_module(void)
{
	return platform_driver_register(&csic_driver);
}

static void __exit msm_csic_exit_module(void)
{
	platform_driver_unregister(&csic_driver);
}

module_init(msm_csic_init_module);
module_exit(msm_csic_exit_module);
MODULE_DESCRIPTION("MSM csic driver");
MODULE_LICENSE("GPL v2");
