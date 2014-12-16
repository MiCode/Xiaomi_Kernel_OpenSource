/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <linux/ratelimit.h>
#include <linux/irqreturn.h>
#include "msm_csid.h"
#include "msm_csid_hwreg.h"
#include "msm_sd.h"
#include "msm_camera_io_util.h"

#define V4L2_IDENT_CSID                            50002
#define CSID_VERSION_V20                      0x02000011
#define CSID_VERSION_V22                      0x02001000
#define CSID_VERSION_V30                      0x30000000
#define MSM_CSID_DRV_NAME                    "msm_csid"

#define DBG_CSID 0

#define TRUE   1
#define FALSE  0

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

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
		if (csid_lut_params->vc_cfg[i]->cid >=
			csid_lut_params->num_cid ||
			csid_lut_params->vc_cfg[i]->cid < 0) {
				pr_err("%s: cid outside range %d\n",
					__func__, csid_lut_params->vc_cfg[i]->cid);
				return -EINVAL;
		}
		CDBG("%s lut params num_cid = %d, cid = %d, dt = %x, df = %d\n",
			__func__,
			csid_lut_params->num_cid,
			csid_lut_params->vc_cfg[i]->cid,
			csid_lut_params->vc_cfg[i]->dt,
			csid_lut_params->vc_cfg[i]->decode_format);
		if (csid_lut_params->vc_cfg[i]->dt < 0x12 ||
			csid_lut_params->vc_cfg[i]->dt > 0x37) {
			pr_err("%s: unsupported data type 0x%x\n",
				 __func__, csid_lut_params->vc_cfg[i]->dt);
			return rc;
		}
		val = msm_camera_io_r(csidbase + CSID_CID_LUT_VC_0_ADDR +
			(csid_lut_params->vc_cfg[i]->cid >> 2) * 4)
			& ~(0xFF << ((csid_lut_params->vc_cfg[i]->cid % 4) *
			8));
		val |= (csid_lut_params->vc_cfg[i]->dt <<
			((csid_lut_params->vc_cfg[i]->cid % 4) * 8));
		msm_camera_io_w(val, csidbase + CSID_CID_LUT_VC_0_ADDR +
			(csid_lut_params->vc_cfg[i]->cid >> 2) * 4);

		val = (csid_lut_params->vc_cfg[i]->decode_format << 4) | 0x3;
		msm_camera_io_w(val, csidbase + CSID_CID_n_CFG_ADDR +
			(csid_lut_params->vc_cfg[i]->cid * 4));
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

static void msm_csid_reset(struct csid_device *csid_dev)
{
	msm_camera_io_w(CSID_RST_STB_ALL, csid_dev->base + CSID_RST_CMD_ADDR);
	wait_for_completion(&csid_dev->reset_complete);
	return;
}

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

	msm_csid_reset(csid_dev);

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
	void __iomem *csidbase;
	csidbase = csid_dev->base;

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

static int msm_csid_irq_routine(struct v4l2_subdev *sd, u32 status,
	bool *handled)
{
	struct csid_device *csid_dev = v4l2_get_subdevdata(sd);
	irqreturn_t ret;
	CDBG("%s E\n", __func__);
	ret = msm_csid_irq(csid_dev->irq->start, csid_dev);
	*handled = TRUE;
	return 0;
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

static struct msm_cam_clk_info csid_8974_clk_info[] = {
	{"camss_top_ahb_clk", -1},
	{"ispif_ahb_clk", -1},
	{"csi_ahb_clk", -1},
	{"csi_src_clk", 200000000},
	{"csi_clk", -1},
	{"csi_phy_clk", -1},
	{"csi_pix_clk", -1},
	{"csi_rdi_clk", -1},
};

static struct msm_cam_clk_info csid_8610_clk_info[] = {
	{"csi_ahb_clk", -1},
	{"csi_src_clk", 200000000},
	{"csi_clk", -1},
	{"csi0phy_mux_clk", -1},
	{"csi1phy_mux_clk", -1},
	{"csi0pix_mux_clk", -1},
	{"csi0rdi_mux_clk", -1},
	{"csi1rdi_mux_clk", -1},
	{"csi2rdi_mux_clk", -1},
};

static struct msm_cam_clk_info csid_8610_clk_src_info[] = {
	{"csi_phy_src_clk", 0},
	{"csi_phy_src_clk", 0},
	{"csi_pix_src_clk", 0},
	{"csi_rdi_src_clk", 0},
	{"csi_rdi_src_clk", 0},
	{"csi_rdi_src_clk", 0},
};

static struct camera_vreg_t csid_8960_vreg_info[] = {
	{"mipi_csi_vdd", REG_LDO, 1200000, 1200000, 20000},
};

static struct camera_vreg_t csid_vreg_info[] = {
	{"qcom,mipi-csi-vdd", REG_LDO, 0, 0, 12000},
};

static int msm_csid_init(struct csid_device *csid_dev, uint32_t *csid_version)
{
	int rc = 0;
	struct camera_vreg_t *cam_vreg;

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

	if (CSID_VERSION == CSID_VERSION_V20)
		cam_vreg = csid_8960_vreg_info;
	else
		cam_vreg = csid_vreg_info;

	if (CSID_VERSION < CSID_VERSION_V30) {
		rc = msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 1);
		if (rc < 0) {
			pr_err("%s: regulator on failed\n", __func__);
			goto vreg_config_failed;
		}
		rc = msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 1);
		if (rc < 0) {
			pr_err("%s: regulator enable failed\n", __func__);
			goto vreg_enable_failed;
		}

		if (CSID_VERSION == CSID_VERSION_V20) {
			rc = msm_cam_clk_enable(&csid_dev->pdev->dev,
				csid_8960_clk_info, csid_dev->csid_clk,
				ARRAY_SIZE(csid_8960_clk_info), 1);
			if (rc < 0) {
				pr_err("%s: 8960: clock enable failed\n",
					 __func__);
				goto clk_enable_failed;
			}
		} else {
			msm_cam_clk_sel_src(&csid_dev->pdev->dev,
				&csid_8610_clk_info[3], csid_8610_clk_src_info,
				ARRAY_SIZE(csid_8610_clk_src_info));
			rc = msm_cam_clk_enable(&csid_dev->pdev->dev,
				csid_8610_clk_info, csid_dev->csid_clk,
				ARRAY_SIZE(csid_8610_clk_info), 1);
			if (rc < 0) {
				pr_err("%s: 8610: clock enable failed\n",
					 __func__);
				goto clk_enable_failed;
			}
		}
	} else if (CSID_VERSION >= CSID_VERSION_V30) {
		rc = msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 1);
		if (rc < 0) {
			pr_err("%s: regulator on failed\n", __func__);
			goto vreg_config_failed;
		}

		rc = msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 1);
		if (rc < 0) {
			pr_err("%s: regulator enable failed\n", __func__);
			goto vreg_enable_failed;
		}

		rc = msm_cam_clk_enable(&csid_dev->pdev->dev,
			csid_8974_clk_info, csid_dev->csid_clk,
			ARRAY_SIZE(csid_8974_clk_info), 1);
		if (rc < 0) {
			pr_err("%s: clock enable failed\n", __func__);
			goto clk_enable_failed;
		}
	}
		CDBG("%s:%d called\n", __func__, __LINE__);
	csid_dev->hw_version =
		msm_camera_io_r(csid_dev->base + CSID_HW_VERSION_ADDR);
	CDBG("%s:%d called csid_dev->hw_version %x\n", __func__, __LINE__,
		csid_dev->hw_version);
	*csid_version = csid_dev->hw_version;

	init_completion(&csid_dev->reset_complete);

	enable_irq(csid_dev->irq->start);

	msm_csid_reset(csid_dev);
	csid_dev->csid_state = CSID_POWER_UP;
	return rc;

clk_enable_failed:
	if (CSID_VERSION < CSID_VERSION_V30) {
		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	} else if (CSID_VERSION >= CSID_VERSION_V30) {
		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	}
vreg_enable_failed:
	if (CSID_VERSION < CSID_VERSION_V30) {
		msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	} else if (CSID_VERSION >= CSID_VERSION_V30) {
		msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
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

	if (csid_dev->csid_state != CSID_POWER_UP) {
		pr_err("%s: csid invalid state %d\n", __func__,
			csid_dev->csid_state);
		return -EINVAL;
	}

	irq = msm_camera_io_r(csid_dev->base + CSID_IRQ_STATUS_ADDR);
	msm_camera_io_w(irq, csid_dev->base + CSID_IRQ_CLEAR_CMD_ADDR);
	msm_camera_io_w(0, csid_dev->base + CSID_IRQ_MASK_ADDR);

	disable_irq(csid_dev->irq->start);

	if (csid_dev->hw_version == CSID_VERSION_V20) {
		msm_cam_clk_enable(&csid_dev->pdev->dev, csid_8960_clk_info,
			csid_dev->csid_clk, ARRAY_SIZE(csid_8960_clk_info), 0);

		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_8960_vreg_info, ARRAY_SIZE(csid_8960_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);

		msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_8960_vreg_info, ARRAY_SIZE(csid_8960_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	} else if (csid_dev->hw_version == CSID_VERSION_V22) {
		msm_cam_clk_enable(&csid_dev->pdev->dev,
			csid_8610_clk_info,
			csid_dev->csid_clk,
			ARRAY_SIZE(csid_8610_clk_info), 0);

		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);

		msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	} else if (csid_dev->hw_version >= CSID_VERSION_V30) {
		msm_cam_clk_enable(&csid_dev->pdev->dev, csid_8974_clk_info,
			csid_dev->csid_clk, ARRAY_SIZE(csid_8974_clk_info), 0);

		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);

		msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
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
	struct csid_cfg_data *cdata = (struct csid_cfg_data *)arg;

	if (!csid_dev || !cdata) {
		pr_err("%s:%d csid_dev %p, cdata %p\n", __func__, __LINE__,
			csid_dev, cdata);
		return -EINVAL;
	}
	CDBG("%s cfgtype = %d\n", __func__, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CSID_INIT:
		rc = msm_csid_init(csid_dev, &cdata->cfg.csid_version);
		CDBG("%s csid version %x\n", __func__,
			cdata->cfg.csid_version);
		break;
	case CSID_CFG: {
		struct msm_camera_csid_params csid_params;
		struct msm_camera_csid_vc_cfg *vc_cfg = NULL;
		int8_t i = 0;
		if (copy_from_user(&csid_params,
			(void *)cdata->cfg.csid_params,
			sizeof(struct msm_camera_csid_params))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		if (csid_params.lut_params.num_cid < 1 ||
			csid_params.lut_params.num_cid > 16) {
			pr_err("%s: %d num_cid outside range\n",
				 __func__, __LINE__);
			rc = -EINVAL;
			break;
		}
		for (i = 0; i < csid_params.lut_params.num_cid; i++) {
			vc_cfg = kzalloc(sizeof(struct msm_camera_csid_vc_cfg),
			    GFP_KERNEL);
			if (!vc_cfg) {
				pr_err("%s: %d failed\n", __func__, __LINE__);
				for (i--; i >= 0; i--)
					kfree(csid_params.lut_params.vc_cfg[i]);
				rc = -ENOMEM;
				break;
			}
			if (copy_from_user(vc_cfg,
				(void *)csid_params.lut_params.vc_cfg[i],
				sizeof(struct msm_camera_csid_vc_cfg))) {
				pr_err("%s: %d failed\n", __func__, __LINE__);
				kfree(vc_cfg);
				for (i--; i >= 0; i--)
					kfree(csid_params.lut_params.vc_cfg[i]);
				rc = -EFAULT;
				break;
			}
			csid_params.lut_params.vc_cfg[i] = vc_cfg;
		}
		rc = msm_csid_config(csid_dev, &csid_params);
		for (i--; i >= 0; i--)
			kfree(csid_params.lut_params.vc_cfg[i]);
		break;
	}
	case CSID_RELEASE:
		rc = msm_csid_release(csid_dev);
		break;
	default:
		pr_err_ratelimited("%s: %d failed\n", __func__, __LINE__);
		rc = -ENOIOCTLCMD;
		break;
	}
	return rc;
}

static int32_t msm_csid_get_subdev_id(struct csid_device *csid_dev, void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	if (!subdev_id) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EINVAL;
	}
	*subdev_id = csid_dev->pdev->id;
	pr_debug("%s:%d subdev_id %d\n", __func__, __LINE__, *subdev_id);
	return 0;
}

static long msm_csid_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc = -ENOIOCTLCMD;
	struct csid_device *csid_dev = v4l2_get_subdevdata(sd);
	mutex_lock(&csid_dev->mutex);
	CDBG("%s:%d id %d\n", __func__, __LINE__, csid_dev->pdev->id);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		rc = msm_csid_get_subdev_id(csid_dev, arg);
		break;
	case VIDIOC_MSM_CSID_IO_CFG:
		rc = msm_csid_cmd(csid_dev, arg);
		break;
	case VIDIOC_MSM_CSID_RELEASE:
	case MSM_SD_SHUTDOWN:
		rc = msm_csid_release(csid_dev);
		break;
	default:
		pr_err("%s: command not found\n", __func__);
	}
	CDBG("%s:%d\n", __func__, __LINE__);
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
	uint32_t csi_vdd_voltage = 0;
	int rc = 0;
	new_csid_dev = kzalloc(sizeof(struct csid_device), GFP_KERNEL);
	if (!new_csid_dev) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}

	v4l2_subdev_init(&new_csid_dev->msm_sd.sd, &msm_csid_subdev_ops);
	v4l2_set_subdevdata(&new_csid_dev->msm_sd.sd, new_csid_dev);
	platform_set_drvdata(pdev, &new_csid_dev->msm_sd.sd);
	mutex_init(&new_csid_dev->mutex);

	if (pdev->dev.of_node) {
		rc = of_property_read_u32((&pdev->dev)->of_node,
			"cell-index", &pdev->id);
		if (rc < 0) {
			pr_err("%s:%d failed to read cell-index\n", __func__,
				__LINE__);
			goto csid_no_resource;
		}
		CDBG("%s device id %d\n", __func__, pdev->id);

		rc = of_property_read_u32((&pdev->dev)->of_node,
			"qcom,csi-vdd-voltage", &csi_vdd_voltage);
		if (rc < 0) {
			pr_err("%s:%d failed to read qcom,csi-vdd-voltage\n",
				__func__, __LINE__);
			goto csid_no_resource;
		}
		CDBG("%s:%d reading mipi_csi_vdd is %d\n", __func__, __LINE__,
			csi_vdd_voltage);

		csid_vreg_info[0].min_voltage = csi_vdd_voltage;
		csid_vreg_info[0].max_voltage = csi_vdd_voltage;
	}

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
	new_csid_dev->msm_sd.sd.internal_ops = &msm_csid_internal_ops;
	new_csid_dev->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(new_csid_dev->msm_sd.sd.name,
			ARRAY_SIZE(new_csid_dev->msm_sd.sd.name), "msm_csid");
	media_entity_init(&new_csid_dev->msm_sd.sd.entity, 0, NULL, 0);
	new_csid_dev->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	new_csid_dev->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_CSID;
	new_csid_dev->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x5;
	msm_sd_register(&new_csid_dev->msm_sd);

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
	if (rc < 0) {
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
