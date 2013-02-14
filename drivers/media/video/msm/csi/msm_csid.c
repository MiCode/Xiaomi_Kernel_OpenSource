/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/of.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <media/msm_isp.h>
#include "msm_csid.h"
#include "msm_csid_hwreg.h"
#include "msm.h"
#include "msm_cam_server.h"

#define V4L2_IDENT_CSID                            50002
#define CSID_VERSION_V2                      0x02000011
#define CSID_VERSION_V3                      0x30000000

#define DBG_CSID 0

#define TRUE   1
#define FALSE  0

static int msm_csid_cid_lut(
	struct msm_camera_csid_lut_params *csid_lut_params,
	void __iomem *csidbase)
{
	int rc = 0, i = 0;
	uint32_t val = 0;

	if (!csid_lut_params) {
		pr_err("%s:%d csid_lut_params NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < csid_lut_params->num_cid && i < 16; i++) {
		CDBG("%s lut params num_cid = %d, cid = %d, dt = %x, df = %d\n",
			__func__,
			csid_lut_params->num_cid,
			csid_lut_params->vc_cfg[i].cid,
			csid_lut_params->vc_cfg[i].dt,
			csid_lut_params->vc_cfg[i].decode_format);
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

static int msm_csid_config(struct csid_device *csid_dev,
	struct msm_camera_csid_params *csid_params)
{
	int rc = 0;
	uint32_t val = 0;
	void __iomem *csidbase;
	csidbase = csid_dev->base;
	if (!csidbase || !csid_params) {
		pr_err("%s:%d csidbase %p, csid params %p\n", __func__,
			__LINE__, csidbase, csid_params);
		return -EINVAL;
	}

	CDBG("%s csid_params, lane_cnt = %d, lane_assign = %x, phy sel = %d\n",
		__func__,
		csid_params->lane_cnt,
		csid_params->lane_assign,
		csid_params->phy_sel);
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
	if (!csid_dev) {
		pr_err("%s:%d csid_dev NULL\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}
	irq = msm_camera_io_r(csid_dev->base + CSID_IRQ_STATUS_ADDR);
	CDBG("%s CSID%d_IRQ_STATUS_ADDR = 0x%x\n",
		 __func__, csid_dev->pdev->id, irq);
	if (irq & (0x1 << CSID_RST_DONE_IRQ_BITSHIFT))
			complete(&csid_dev->reset_complete);
	msm_camera_io_w(irq, csid_dev->base + CSID_IRQ_CLEAR_CMD_ADDR);
	return IRQ_HANDLED;
}

int msm_csid_irq_routine(struct v4l2_subdev *sd, u32 status, bool *handled)
{
	struct csid_device *csid_dev = v4l2_get_subdevdata(sd);
	irqreturn_t ret;
	CDBG("%s E\n", __func__);
	ret = msm_csid_irq(csid_dev->irq->start, csid_dev);
	*handled = TRUE;
	return 0;
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

static struct msm_cam_clk_info csid_8960_clk_info[] = {
	{"csi_src_clk", 177780000},
	{"csi_clk", -1},
	{"csi_phy_clk", -1},
	{"csi_pclk", -1},
};

static struct msm_cam_clk_info csid0_8974_clk_info[] = {
	{"csi0_ahb_clk", -1},
	{"csi0_src_clk", 200000000},
	{"csi0_clk", -1},
	{"csi0_phy_clk", -1},
	{"csi0_pix_clk", -1},
	{"csi0_rdi_clk", -1},
};

static struct msm_cam_clk_info csid1_8974_clk_info[] = {
	{"csi1_ahb_clk", -1},
	{"csi1_src_clk", 200000000},
	{"csi1_clk", -1},
	{"csi1_phy_clk", -1},
	{"csi1_pix_clk", -1},
	{"csi1_rdi_clk", -1},
};

static struct msm_cam_clk_info csid2_8974_clk_info[] = {
	{"csi2_ahb_clk", -1},
	{"csi2_src_clk", 200000000},
	{"csi2_clk", -1},
	{"csi2_phy_clk", -1},
	{"csi2_pix_clk", -1},
	{"csi2_rdi_clk", -1},
};

static struct msm_cam_clk_info csid3_8974_clk_info[] = {
	{"csi3_ahb_clk", -1},
	{"csi3_src_clk", 200000000},
	{"csi3_clk", -1},
	{"csi3_phy_clk", -1},
	{"csi3_pix_clk", -1},
	{"csi3_rdi_clk", -1},
};

static struct msm_cam_clk_setting csid_8974_clk_info[] = {
	{&csid0_8974_clk_info[0], ARRAY_SIZE(csid0_8974_clk_info)},
	{&csid1_8974_clk_info[0], ARRAY_SIZE(csid1_8974_clk_info)},
	{&csid2_8974_clk_info[0], ARRAY_SIZE(csid2_8974_clk_info)},
	{&csid3_8974_clk_info[0], ARRAY_SIZE(csid3_8974_clk_info)},
};

static struct camera_vreg_t csid_8960_vreg_info[] = {
	{"mipi_csi_vdd", REG_LDO, 1200000, 1200000, 20000},
};

static struct camera_vreg_t csid_8974_vreg_info[] = {
	{"mipi_csi_vdd", REG_LDO, 1800000, 1800000, 12000},
};

static int msm_csid_init(struct csid_device *csid_dev, uint32_t *csid_version)
{
	int rc = 0;
	uint8_t core_id = 0;

	if (!csid_version) {
		pr_err("%s:%d csid_version NULL\n", __func__, __LINE__);
		rc = -EINVAL;
		return rc;
	}

	if (csid_dev->csid_state == CSID_POWER_UP) {
		pr_err("%s: csid invalid state %d\n", __func__,
			csid_dev->csid_state);
		rc = -EINVAL;
		return rc;
	}

	csid_dev->base = ioremap(csid_dev->mem->start,
		resource_size(csid_dev->mem));
	if (!csid_dev->base) {
		pr_err("%s csid_dev->base NULL\n", __func__);
		rc = -ENOMEM;
		return rc;
	}

	if (CSID_VERSION <= CSID_VERSION_V2) {
		rc = msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_8960_vreg_info, ARRAY_SIZE(csid_8960_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 1);
		if (rc < 0) {
			pr_err("%s: regulator on failed\n", __func__);
			goto vreg_config_failed;
		}

		rc = msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_8960_vreg_info, ARRAY_SIZE(csid_8960_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 1);
		if (rc < 0) {
			pr_err("%s: regulator enable failed\n", __func__);
			goto vreg_enable_failed;
		}

		rc = msm_cam_clk_enable(&csid_dev->pdev->dev,
			csid_8960_clk_info, csid_dev->csid_clk,
			ARRAY_SIZE(csid_8960_clk_info), 1);
		if (rc < 0) {
			pr_err("%s: clock enable failed\n", __func__);
			goto clk_enable_failed;
		}
	} else if (CSID_VERSION == CSID_VERSION_V3) {
		rc = msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_8974_vreg_info, ARRAY_SIZE(csid_8974_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 1);
		if (rc < 0) {
			pr_err("%s: regulator on failed\n", __func__);
			goto vreg_config_failed;
		}

		rc = msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_8974_vreg_info, ARRAY_SIZE(csid_8974_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 1);
		if (rc < 0) {
			pr_err("%s: regulator enable failed\n", __func__);
			goto vreg_enable_failed;
		}

		rc = msm_cam_clk_enable(&csid_dev->pdev->dev,
			csid_8974_clk_info[0].clk_info, csid_dev->csid0_clk,
			csid_8974_clk_info[0].num_clk_info, 1);
		if (rc < 0) {
			pr_err("%s: clock enable failed\n", __func__);
			goto csid0_clk_enable_failed;
		}
		core_id = csid_dev->pdev->id;
		if (core_id) {
			rc = msm_cam_clk_enable(&csid_dev->pdev->dev,
				csid_8974_clk_info[core_id].clk_info,
				csid_dev->csid_clk,
				csid_8974_clk_info[core_id].num_clk_info, 1);
			if (rc < 0) {
				pr_err("%s: clock enable failed\n",
					__func__);
				goto clk_enable_failed;
			}
		}
	}

	csid_dev->hw_version =
		msm_camera_io_r(csid_dev->base + CSID_HW_VERSION_ADDR);
	*csid_version = csid_dev->hw_version;

	init_completion(&csid_dev->reset_complete);

	enable_irq(csid_dev->irq->start);

	msm_csid_reset(csid_dev);
	csid_dev->csid_state = CSID_POWER_UP;
	return rc;

clk_enable_failed:
	if (CSID_VERSION == CSID_VERSION_V3) {
		msm_cam_clk_enable(&csid_dev->pdev->dev,
			csid_8974_clk_info[0].clk_info, csid_dev->csid0_clk,
			csid_8974_clk_info[0].num_clk_info, 0);
	}
csid0_clk_enable_failed:
	if (CSID_VERSION <= CSID_VERSION_V2) {
		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_8960_vreg_info, ARRAY_SIZE(csid_8960_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	} else if (CSID_VERSION == CSID_VERSION_V3) {
		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_8974_vreg_info, ARRAY_SIZE(csid_8974_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	}
vreg_enable_failed:
	if (CSID_VERSION <= CSID_VERSION_V2) {
		msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_8960_vreg_info, ARRAY_SIZE(csid_8960_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	} else if (CSID_VERSION == CSID_VERSION_V3) {
		msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_8974_vreg_info, ARRAY_SIZE(csid_8974_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	}
vreg_config_failed:
	iounmap(csid_dev->base);
	csid_dev->base = NULL;
	return rc;
}

static int msm_csid_release(struct csid_device *csid_dev)
{
	uint32_t irq;
	uint8_t core_id = 0;

	if (csid_dev->csid_state != CSID_POWER_UP) {
		pr_err("%s: csid invalid state %d\n", __func__,
			csid_dev->csid_state);
		return -EINVAL;
	}

	irq = msm_camera_io_r(csid_dev->base + CSID_IRQ_STATUS_ADDR);
	msm_camera_io_w(irq, csid_dev->base + CSID_IRQ_CLEAR_CMD_ADDR);
	msm_camera_io_w(0, csid_dev->base + CSID_IRQ_MASK_ADDR);

	disable_irq(csid_dev->irq->start);

	if (csid_dev->hw_version <= CSID_VERSION_V2) {
		msm_cam_clk_enable(&csid_dev->pdev->dev, csid_8960_clk_info,
			csid_dev->csid_clk, ARRAY_SIZE(csid_8960_clk_info), 0);

		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_8960_vreg_info, ARRAY_SIZE(csid_8960_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);

		msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_8960_vreg_info, ARRAY_SIZE(csid_8960_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	} else if (csid_dev->hw_version == CSID_VERSION_V3) {
		core_id = csid_dev->pdev->id;
		if (core_id)
			msm_cam_clk_enable(&csid_dev->pdev->dev,
				csid_8974_clk_info[core_id].clk_info,
				csid_dev->csid_clk,
				csid_8974_clk_info[core_id].num_clk_info, 0);

		msm_cam_clk_enable(&csid_dev->pdev->dev,
			csid_8974_clk_info[0].clk_info, csid_dev->csid0_clk,
			csid_8974_clk_info[0].num_clk_info, 0);

		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_8974_vreg_info, ARRAY_SIZE(csid_8974_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);

		msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_8974_vreg_info, ARRAY_SIZE(csid_8974_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	}

	iounmap(csid_dev->base);
	csid_dev->base = NULL;
	csid_dev->csid_state = CSID_POWER_DOWN;
	return 0;
}

static long msm_csid_cmd(struct csid_device *csid_dev, void *arg)
{
	int rc = 0;
	struct csid_cfg_data cdata;

	if (!csid_dev) {
		pr_err("%s:%d csid_dev NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (copy_from_user(&cdata,
		(void *)arg,
		sizeof(struct csid_cfg_data))) {
		pr_err("%s: %d failed\n", __func__, __LINE__);
		return -EFAULT;
	}
	CDBG("%s cfgtype = %d\n", __func__, cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CSID_INIT:
		rc = msm_csid_init(csid_dev, &cdata.cfg.csid_version);
		if (copy_to_user((void *)arg,
			&cdata,
			sizeof(struct csid_cfg_data))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -EFAULT;
		}
		break;
	case CSID_CFG: {
		struct msm_camera_csid_params csid_params;
		struct msm_camera_csid_vc_cfg *vc_cfg = NULL;
		if (copy_from_user(&csid_params,
			(void *)cdata.cfg.csid_params,
			sizeof(struct msm_camera_csid_params))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		vc_cfg = kzalloc(csid_params.lut_params.num_cid *
			sizeof(struct msm_camera_csid_vc_cfg),
			GFP_KERNEL);
		if (!vc_cfg) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(vc_cfg,
			(void *)csid_params.lut_params.vc_cfg,
			(csid_params.lut_params.num_cid *
			sizeof(struct msm_camera_csid_vc_cfg)))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			kfree(vc_cfg);
			rc = -EFAULT;
			break;
		}
		csid_params.lut_params.vc_cfg = vc_cfg;
		rc = msm_csid_config(csid_dev, &csid_params);
		kfree(vc_cfg);
		break;
	}
	default:
		pr_err("%s: %d failed\n", __func__, __LINE__);
		rc = -ENOIOCTLCMD;
		break;
	}
	return rc;
}

static long msm_csid_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc = -ENOIOCTLCMD;
	struct csid_device *csid_dev = v4l2_get_subdevdata(sd);
	mutex_lock(&csid_dev->mutex);
	switch (cmd) {
	case VIDIOC_MSM_CSID_CFG:
		rc = msm_csid_cmd(csid_dev, arg);
		break;
	case VIDIOC_MSM_CSID_RELEASE:
		rc = msm_csid_release(csid_dev);
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
	.interrupt_service_routine = msm_csid_irq_routine,
};

static const struct v4l2_subdev_ops msm_csid_subdev_ops = {
	.core = &msm_csid_subdev_core_ops,
};

static int __devinit csid_probe(struct platform_device *pdev)
{
	struct csid_device *new_csid_dev;
	struct msm_cam_subdev_info sd_info;
	struct intr_table_entry irq_req;

	int rc = 0;
	CDBG("%s:%d called\n", __func__, __LINE__);
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

	CDBG("%s device id %d\n", __func__, pdev->id);
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

	/* Request for this device irq from the camera server. If the
	 * IRQ Router is present on this target, the interrupt will be
	 * handled by the camera server and the interrupt service
	 * routine called. If the request_irq call returns ENXIO, then
	 * the IRQ Router hardware is not present on this target. We
	 * have to request for the irq ourselves and register the
	 * appropriate interrupt handler. */
	irq_req.cam_hw_idx       = MSM_CAM_HW_CSI0 + pdev->id;
	irq_req.dev_name         = "csid";
	irq_req.irq_idx          = CAMERA_SS_IRQ_2 + pdev->id;
	irq_req.irq_num          = new_csid_dev->irq->start;
	irq_req.is_composite     = 0;
	irq_req.irq_trigger_type = IRQF_TRIGGER_RISING;
	irq_req.num_hwcore       = 1;
	irq_req.subdev_list[0]   = &new_csid_dev->subdev;
	irq_req.data             = (void *)new_csid_dev;
	rc = msm_cam_server_request_irq(&irq_req);
	if (rc == -ENXIO) {
		/* IRQ Router hardware is not present on this hardware.
		 * Request for the IRQ and register the interrupt handler. */
		rc = request_irq(new_csid_dev->irq->start, msm_csid_irq,
			IRQF_TRIGGER_RISING, "csid", new_csid_dev);
		if (rc < 0) {
			release_mem_region(new_csid_dev->mem->start,
				resource_size(new_csid_dev->mem));
			pr_err("%s: irq request fail\n", __func__);
			rc = -EBUSY;
			goto csid_no_resource;
		}
		disable_irq(new_csid_dev->irq->start);
	} else if (rc < 0) {
		release_mem_region(new_csid_dev->mem->start,
			resource_size(new_csid_dev->mem));
		pr_err("%s Error registering irq ", __func__);
		goto csid_no_resource;
	}

	new_csid_dev->csid_state = CSID_POWER_DOWN;
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
