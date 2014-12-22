/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#include <linux/irqreturn.h>
#include "msm_csid.h"
#include "msm_sd.h"
#include "msm_camera_io_util.h"
#include "include/msm_csid_2_0_hwreg.h"
#include "include/msm_csid_2_2_hwreg.h"
#include "include/msm_csid_3_0_hwreg.h"
#include "include/msm_csid_3_1_hwreg.h"
#include "include/msm_csid_3_2_hwreg.h"

#define V4L2_IDENT_CSID                            50002
#define CSID_VERSION_V20                      0x02000011
#define CSID_VERSION_V22                      0x02001000
#define CSID_VERSION_V30                      0x30000000
#define CSID_VERSION_V31                      0x30010000
#define CSID_VERSION_V31_1                    0x30010001
#define CSID_VERSION_V31_3                    0x30010003
#define CSID_VERSION_V32                      0x30020000
#define CSID_VERSION_V33                      0x30030000
#define CSID_VERSION_V34                      0x30040000
#define CSID_VERSION_V37                      0x30070000
#define CSID_VERSION_V40                      0x40000000
#define MSM_CSID_DRV_NAME                    "msm_csid"

#define DBG_CSID 0

#define TRUE   1
#define FALSE  0

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

static struct msm_cam_clk_info csid_clk_info[CSID_NUM_CLK_MAX];
static struct msm_cam_clk_info csid_clk_src_info[CSID_NUM_CLK_MAX];

static struct camera_vreg_t csid_vreg_info[] = {
	{"qcom,mipi-csi-vdd", 0, 0, 12000},
};

static struct camera_vreg_t csid_8960_vreg_info[] = {
	{"mipi_csi_vdd", 1200000, 1200000, 20000},
};
#ifdef CONFIG_COMPAT
static struct v4l2_file_operations msm_csid_v4l2_subdev_fops;
#endif

static int msm_csid_cid_lut(
	struct msm_camera_csid_lut_params *csid_lut_params,
	struct csid_device *csid_dev)
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

		CDBG("%s lut params num_cid = %d, cid = %d\n",
			__func__,
			csid_lut_params->num_cid,
			csid_lut_params->vc_cfg[i]->cid);
		CDBG("%s lut params dt = 0x%x, df = %d\n", __func__,
			csid_lut_params->vc_cfg[i]->dt,
			csid_lut_params->vc_cfg[i]->decode_format);
		if (csid_lut_params->vc_cfg[i]->dt < 0x12 ||
			csid_lut_params->vc_cfg[i]->dt > 0x37) {
			pr_err("%s: unsupported data type 0x%x\n",
				 __func__, csid_lut_params->vc_cfg[i]->dt);
			return rc;
		}
		val = msm_camera_io_r(csid_dev->base +
			csid_dev->ctrl_reg->csid_reg.csid_cid_lut_vc_0_addr +
			(csid_lut_params->vc_cfg[i]->cid >> 2) * 4)
			& ~(0xFF << ((csid_lut_params->vc_cfg[i]->cid % 4) *
			8));
		val |= (csid_lut_params->vc_cfg[i]->dt <<
			((csid_lut_params->vc_cfg[i]->cid % 4) * 8));
		msm_camera_io_w(val, csid_dev->base +
			csid_dev->ctrl_reg->csid_reg.csid_cid_lut_vc_0_addr +
			(csid_lut_params->vc_cfg[i]->cid >> 2) * 4);

		val = (csid_lut_params->vc_cfg[i]->decode_format << 4) | 0x3;
		msm_camera_io_w(val, csid_dev->base +
			csid_dev->ctrl_reg->csid_reg.csid_cid_n_cfg_addr +
			(csid_lut_params->vc_cfg[i]->cid * 4));
	}
	return rc;
}

#if DBG_CSID
static void msm_csid_set_debug_reg(struct csid_device *csid_dev,
	struct msm_camera_csid_params *csid_params)
{
	uint32_t val = 0;
	val = ((1 << csid_params->lane_cnt) - 1) << 20;
	msm_camera_io_w(0x7f010800 | val, csid_dev->base +
		csid_dev->ctrl_reg->csid_reg.csid_irq_mask_addr);
	msm_camera_io_w(0x7f010800 | val, csid_dev->base +
		csid_dev->ctrl_reg->csid_reg.csid_irq_clear_cmd_addr);
}
#else
static void msm_csid_set_debug_reg(struct csid_device *csid_dev,
	struct msm_camera_csid_params *csid_params) {}
#endif

static void msm_csid_reset(struct csid_device *csid_dev)
{
	msm_camera_io_w(csid_dev->ctrl_reg->csid_reg.csid_rst_stb_all,
		csid_dev->base +
		csid_dev->ctrl_reg->csid_reg.csid_rst_cmd_addr);
	wait_for_completion(&csid_dev->reset_complete);
	return;
}

static int msm_csid_config(struct csid_device *csid_dev,
	struct msm_camera_csid_params *csid_params)
{
	int rc = 0;
	uint32_t val = 0, clk_rate = 0, round_rate = 0;
	struct clk **csid_clk_ptr;
	void __iomem *csidbase;
	csidbase = csid_dev->base;
	if (!csidbase || !csid_params) {
		pr_err("%s:%d csidbase %p, csid params %p\n", __func__,
			__LINE__, csidbase, csid_params);
		return -EINVAL;
	}

	CDBG("%s csid_params, lane_cnt = %d, lane_assign = 0x%x\n",
		__func__,
		csid_params->lane_cnt,
		csid_params->lane_assign);
	CDBG("%s csid_params phy_sel = %d\n", __func__,
		csid_params->phy_sel);

	csid_dev->csid_lane_cnt = csid_params->lane_cnt;
	msm_csid_reset(csid_dev);

	csid_clk_ptr = csid_dev->csid_clk;
	if (!csid_clk_ptr) {
		pr_err("csi_src_clk get failed\n");
		return -EINVAL;
	}

	clk_rate = (csid_params->csi_clk > 0) ?
				(csid_params->csi_clk) : csid_dev->csid_max_clk;
	round_rate = clk_round_rate(csid_clk_ptr[csid_dev->csid_clk_index],
					clk_rate);
	if (round_rate > csid_dev->csid_max_clk)
		round_rate = csid_dev->csid_max_clk;
	pr_debug("usr set rate csi_clk clk_rate = %u round_rate = %u\n",
					clk_rate, round_rate);
	rc = clk_set_rate(csid_clk_ptr[csid_dev->csid_clk_index],
				round_rate);
	if (rc < 0) {
		pr_err("csi_src_clk set failed\n");
		return rc;
	}

	val = csid_params->lane_cnt - 1;
	val |= csid_params->lane_assign <<
		csid_dev->ctrl_reg->csid_reg.csid_dl_input_sel_shift;
	if (csid_dev->hw_version < 0x30000000) {
		val |= (0xF << 10);
		msm_camera_io_w(val, csidbase +
		csid_dev->ctrl_reg->csid_reg.csid_core_ctrl_0_addr);
	} else {
		msm_camera_io_w(val, csidbase +
		csid_dev->ctrl_reg->csid_reg.csid_core_ctrl_0_addr);
		val = csid_params->phy_sel <<
			csid_dev->ctrl_reg->csid_reg.csid_phy_sel_shift;
		val |= 0xF;
		msm_camera_io_w(val, csidbase +
		csid_dev->ctrl_reg->csid_reg.csid_core_ctrl_1_addr);
	}

	rc = msm_csid_cid_lut(&csid_params->lut_params, csid_dev);
	if (rc < 0)
		return rc;

	msm_csid_set_debug_reg(csid_dev, csid_params);
	return rc;
}

static irqreturn_t msm_csid_irq(int irq_num, void *data)
{
	uint32_t irq;
	uint32_t short_dt = 0;
	struct csid_device *csid_dev = data;

	if (!csid_dev) {
		pr_err("%s:%d csid_dev NULL\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}
	irq = msm_camera_io_r(csid_dev->base +
		csid_dev->ctrl_reg->csid_reg.csid_irq_status_addr);
	if (csid_dev->csid_sof_debug == 1)
		pr_err("%s CSID%d_IRQ_STATUS_ADDR = 0x%x\n",
			 __func__, csid_dev->pdev->id, irq);
	else
		CDBG("%s CSID%d_IRQ_STATUS_ADDR = 0x%x\n",
			 __func__, csid_dev->pdev->id, irq);
	if (irq & (0x1 <<
		csid_dev->ctrl_reg->csid_reg.csid_rst_done_irq_bitshift))
		complete(&csid_dev->reset_complete);
	if (irq & (0x1 << 9)) {
		if (csid_dev->csid_sof_debug == 1) {
			short_dt = msm_camera_io_r(csid_dev->base +
				csid_dev->ctrl_reg->csid_reg.
				csid_captured_short_pkt_addr);
			short_dt = short_dt >> 24;
			pr_err("%s: core %d short dt %x\n", __func__,
				csid_dev->pdev->id, short_dt);
		}
		msm_camera_io_w(0x101, csid_dev->base +
			csid_dev->ctrl_reg->csid_reg.csid_rst_cmd_addr);
	}
	msm_camera_io_w(irq, csid_dev->base +
		csid_dev->ctrl_reg->csid_reg.csid_irq_clear_cmd_addr);
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

static int msm_csid_init(struct csid_device *csid_dev, uint32_t *csid_version)
{
	int rc = 0;

	if (!csid_version) {
		pr_err("%s:%d csid_version NULL\n", __func__, __LINE__);
		rc = -EINVAL;
		return rc;
	}

	csid_dev->reg_ptr = NULL;

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

	pr_info("%s: CSID_VERSION = 0x%x\n", __func__,
		csid_dev->ctrl_reg->csid_reg.csid_version);
	/* power up */
	if (csid_dev->ctrl_reg->csid_reg.csid_version < CSID_VERSION_V22) {
		rc = msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_8960_vreg_info, ARRAY_SIZE(csid_8960_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 1);
	} else {
		rc = msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 1);
	}
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto vreg_config_failed;
	}
	if (csid_dev->ctrl_reg->csid_reg.csid_version < CSID_VERSION_V22) {
		rc = msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_8960_vreg_info, ARRAY_SIZE(csid_8960_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 1);
	} else {
		rc = msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 1);
	}
	if (rc < 0) {
		pr_err("%s: regulator enable failed\n", __func__);
		goto vreg_enable_failed;
	}

	csid_dev->reg_ptr = regulator_get(&(csid_dev->pdev->dev),
					 "qcom,gdscr-vdd");
	if (IS_ERR_OR_NULL(csid_dev->reg_ptr)) {
		pr_err(" %s: Failed in getting TOP gdscr regulator handle",
			__func__);
	} else {
		rc = regulator_enable(csid_dev->reg_ptr);
		if (rc) {
			pr_err(" %s: regulator enable failed for GDSCR\n",
				__func__);
			goto gdscr_regulator_enable_failed;
		}
	}

	if (csid_dev->ctrl_reg->csid_reg.csid_version == CSID_VERSION_V22)
		msm_cam_clk_sel_src(&csid_dev->pdev->dev,
			&csid_clk_info[3], csid_clk_src_info,
			csid_dev->num_clk_src_info);


	rc = msm_cam_clk_enable(&csid_dev->pdev->dev,
			csid_clk_info, csid_dev->csid_clk,
			csid_dev->num_clk, 1);
	if (rc < 0) {
		pr_err("%s:%d clock enable failed\n",
			 __func__, __LINE__);
		goto clk_enable_failed;
	}
	CDBG("%s:%d called\n", __func__, __LINE__);
	csid_dev->hw_version =
		msm_camera_io_r(csid_dev->base +
		csid_dev->ctrl_reg->csid_reg.csid_hw_version_addr);
	CDBG("%s:%d called csid_dev->hw_version %x\n", __func__, __LINE__,
		csid_dev->hw_version);
	*csid_version = csid_dev->hw_version;
	csid_dev->csid_sof_debug = 0;

	init_completion(&csid_dev->reset_complete);

	enable_irq(csid_dev->irq->start);

	msm_csid_reset(csid_dev);
	csid_dev->csid_state = CSID_POWER_UP;
	return rc;

clk_enable_failed:
	if (csid_dev->ctrl_reg->csid_reg.csid_version < CSID_VERSION_V22) {
		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_8960_vreg_info, ARRAY_SIZE(csid_8960_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	} else {
		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	}
gdscr_regulator_enable_failed:
	if (!IS_ERR_OR_NULL(csid_dev->reg_ptr)) {
		regulator_disable(csid_dev->reg_ptr);
		regulator_put(csid_dev->reg_ptr);
		csid_dev->reg_ptr = NULL;
	}

vreg_enable_failed:
	if (csid_dev->ctrl_reg->csid_reg.csid_version < CSID_VERSION_V22) {
		msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_8960_vreg_info, ARRAY_SIZE(csid_8960_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	} else {
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

	CDBG("%s:%d, hw_version = 0x%x\n", __func__, __LINE__,
		csid_dev->hw_version);

	irq = msm_camera_io_r(csid_dev->base +
		csid_dev->ctrl_reg->csid_reg.csid_irq_status_addr);
	msm_camera_io_w(irq, csid_dev->base +
		csid_dev->ctrl_reg->csid_reg.csid_irq_clear_cmd_addr);
	msm_camera_io_w(0, csid_dev->base +
		csid_dev->ctrl_reg->csid_reg.csid_irq_mask_addr);

	disable_irq(csid_dev->irq->start);

	if (csid_dev->hw_version == CSID_VERSION_V20) {
		msm_cam_clk_enable(&csid_dev->pdev->dev, csid_clk_info,
			csid_dev->csid_clk, csid_dev->num_clk, 0);

		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_8960_vreg_info, ARRAY_SIZE(csid_8960_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);

		msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_8960_vreg_info, ARRAY_SIZE(csid_8960_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	} else if (csid_dev->hw_version == CSID_VERSION_V22) {
		msm_cam_clk_enable(&csid_dev->pdev->dev,
			csid_clk_info,
			csid_dev->csid_clk,
			csid_dev->num_clk, 0);

		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);

		msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	} else if ((csid_dev->hw_version >= CSID_VERSION_V30 &&
		csid_dev->hw_version < CSID_VERSION_V31) ||
		(csid_dev->hw_version == CSID_VERSION_V40) ||
		(csid_dev->hw_version == CSID_VERSION_V31_1) ||
		(csid_dev->hw_version == CSID_VERSION_V31_3)) {
		msm_cam_clk_enable(&csid_dev->pdev->dev, csid_clk_info,
			csid_dev->csid_clk, csid_dev->num_clk, 0);
		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
		msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	} else if ((csid_dev->hw_version == CSID_VERSION_V31) ||
		(csid_dev->hw_version == CSID_VERSION_V32) ||
		(csid_dev->hw_version == CSID_VERSION_V33) ||
		(csid_dev->hw_version == CSID_VERSION_V37) ||
		(csid_dev->hw_version == CSID_VERSION_V34)) {
		msm_cam_clk_enable(&csid_dev->pdev->dev, csid_clk_info,
			csid_dev->csid_clk, csid_dev->num_clk, 0);
		msm_camera_enable_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);

		msm_camera_config_vreg(&csid_dev->pdev->dev,
			csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
			NULL, 0, &csid_dev->csi_vdd, 0);
	} else {
		pr_err("%s:%d, invalid hw version : 0x%x", __func__, __LINE__,
			csid_dev->hw_version);
		return -EINVAL;
	}

	if (!IS_ERR_OR_NULL(csid_dev->reg_ptr)) {
		regulator_disable(csid_dev->reg_ptr);
		regulator_put(csid_dev->reg_ptr);
	}

	iounmap(csid_dev->base);
	csid_dev->base = NULL;
	csid_dev->csid_state = CSID_POWER_DOWN;
	return 0;
}

static int32_t msm_csid_cmd(struct csid_device *csid_dev, void __user *arg)
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
		CDBG("%s csid version 0x%x\n", __func__,
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
		csid_dev->csid_sof_debug = 0;
		rc = msm_csid_config(csid_dev, &csid_params);
		for (i--; i >= 0; i--)
			kfree(csid_params.lut_params.vc_cfg[i]);
		break;
	}
	case CSID_RELEASE:
		rc = msm_csid_release(csid_dev);
		break;
	default:
		pr_err("%s: %d failed\n", __func__, __LINE__);
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
	case MSM_SD_NOTIFY_FREEZE: {
		if (csid_dev->csid_state != CSID_POWER_UP)
			break;
		csid_dev->csid_sof_debug = 1;
		break;
	   }
	case VIDIOC_MSM_CSID_RELEASE:
	case MSM_SD_SHUTDOWN:
		rc = msm_csid_release(csid_dev);
		break;
	default:
		pr_err_ratelimited("%s: command not found\n", __func__);
	}
	CDBG("%s:%d\n", __func__, __LINE__);
	mutex_unlock(&csid_dev->mutex);
	return rc;
}


#ifdef CONFIG_COMPAT
static int32_t msm_csid_cmd32(struct csid_device *csid_dev, void __user *arg)
{
	int rc = 0;
	struct csid_cfg_data *cdata;
	struct csid_cfg_data32 *arg32 =  (struct csid_cfg_data32 *) (arg);
	struct csid_cfg_data local_arg;
	local_arg.cfgtype = arg32->cfgtype;
	cdata = &local_arg;

	if (!csid_dev || !cdata) {
		pr_err("%s:%d csid_dev %p, cdata %p\n", __func__, __LINE__,
			csid_dev, cdata);
		return -EINVAL;
	}

	CDBG("%s cfgtype = %d\n", __func__, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CSID_INIT:
		rc = msm_csid_init(csid_dev, &cdata->cfg.csid_version);
		arg32->cfg.csid_version = local_arg.cfg.csid_version;
		CDBG("%s csid version 0x%x\n", __func__,
			cdata->cfg.csid_version);
		break;
	case CSID_CFG: {

		struct msm_camera_csid_params csid_params;
		struct msm_camera_csid_vc_cfg *vc_cfg = NULL;
		int8_t i = 0;
		struct msm_camera_csid_lut_params32 lut_par32;
		struct msm_camera_csid_params32 csid_params32;
		struct msm_camera_csid_vc_cfg vc_cfg32;

		if (copy_from_user(&csid_params32,
			(void *)compat_ptr(arg32->cfg.csid_params),
			sizeof(struct msm_camera_csid_params32))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		csid_params.lane_cnt = csid_params32.lane_cnt;
		csid_params.lane_assign = csid_params32.lane_assign;
		csid_params.phy_sel = csid_params32.phy_sel;
		csid_params.csi_clk = csid_params32.csi_clk;

		lut_par32 = csid_params32.lut_params;
		csid_params.lut_params.num_cid = lut_par32.num_cid;

		if (csid_params.lut_params.num_cid < 1 ||
			csid_params.lut_params.num_cid > 16) {
			pr_err("%s: %d num_cid outside range\n",
				 __func__, __LINE__);
			rc = -EINVAL;
			break;
		}

		for (i = 0; i < lut_par32.num_cid; i++) {
			vc_cfg = kzalloc(sizeof(struct msm_camera_csid_vc_cfg),
				GFP_KERNEL);
			if (!vc_cfg) {
				pr_err("%s: %d failed\n", __func__, __LINE__);
				for (i--; i >= 0; i--)
					kfree(csid_params.lut_params.vc_cfg[i]);
				rc = -ENOMEM;
				break;
			}
			/* msm_camera_csid_vc_cfg size
			 * does not change in COMPAT MODE
			 */
			if (copy_from_user(&vc_cfg32,
				(void *)compat_ptr(lut_par32.vc_cfg[i]),
				sizeof(vc_cfg32))) {
				pr_err("%s: %d failed\n", __func__, __LINE__);
				for (; i >= 0; i--)
					kfree(csid_params.lut_params.vc_cfg[i]);
				rc = -EFAULT;
				break;
			}
			vc_cfg->cid = vc_cfg32.cid;
			vc_cfg->dt = vc_cfg32.dt;
			vc_cfg->decode_format = vc_cfg32.decode_format;
			csid_params.lut_params.vc_cfg[i] = vc_cfg;
		}

		if (rc < 0)
			break;
		rc = msm_csid_config(csid_dev, &csid_params);
		for (i--; i >= 0; i--)
			kfree(csid_params.lut_params.vc_cfg[i]);
		break;
	}
	case CSID_RELEASE:
		rc = msm_csid_release(csid_dev);
		break;
	default:
		pr_err("%s: %d failed\n", __func__, __LINE__);
		rc = -ENOIOCTLCMD;
		break;
	}
	return rc;
}

static long msm_csid_subdev_ioctl32(struct v4l2_subdev *sd,
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
	case VIDIOC_MSM_CSID_IO_CFG32:
		rc = msm_csid_cmd32(csid_dev, arg);
		break;
	case MSM_SD_NOTIFY_FREEZE: {
		if (csid_dev->csid_state != CSID_POWER_UP)
			break;
		csid_dev->csid_sof_debug = 1;
		break;
	   }
	case VIDIOC_MSM_CSID_RELEASE:
	case MSM_SD_SHUTDOWN:
		rc = msm_csid_release(csid_dev);
		break;
	default:
		pr_err_ratelimited("%s: command not found\n", __func__);
	}
	CDBG("%s:%d\n", __func__, __LINE__);
	mutex_unlock(&csid_dev->mutex);
	return rc;
}

static long msm_csid_subdev_do_ioctl32(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);

	return msm_csid_subdev_ioctl32(sd, cmd, arg);
}

static long msm_csid_subdev_fops_ioctl32(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_csid_subdev_do_ioctl32);
}
#endif
static const struct v4l2_subdev_internal_ops msm_csid_internal_ops;

static struct v4l2_subdev_core_ops msm_csid_subdev_core_ops = {
	.g_chip_ident = &msm_csid_subdev_g_chip_ident,
	.ioctl = &msm_csid_subdev_ioctl,
	.interrupt_service_routine = msm_csid_irq_routine,
};

static const struct v4l2_subdev_ops msm_csid_subdev_ops = {
	.core = &msm_csid_subdev_core_ops,
};

static int msm_csid_get_clk_info(struct csid_device *csid_dev,
	struct platform_device *pdev)
{
	uint32_t count;
	uint32_t cnt = 0;
	int i, rc;
	int ii = 0;
	uint32_t rates[CSID_NUM_CLK_MAX];
	const char *clock_name;
	struct device_node *of_node;
	of_node = pdev->dev.of_node;

	count = of_property_count_strings(of_node, "clock-names");
	csid_dev->num_clk = count;

	CDBG("%s: count = %d\n", __func__, count);
	if (count == 0) {
		pr_err("%s: no clocks found in device tree, count=%d",
			__func__, count);
		return -EINVAL;
	}

	if (count > CSID_NUM_CLK_MAX) {
		pr_err("%s: invalid count=%d, max is %d\n", __func__,
			count, CSID_NUM_CLK_MAX);
		return -EINVAL;
	}

	if (csid_dev->hw_dts_version == CSID_VERSION_V22) {
		cnt = count;
		count = 0;
		CDBG("%s: cnt = %d\n", __func__, cnt);
		if (cnt == 0) {
			pr_err("%s: no clocks found in device tree, cnt=%d",
				__func__, cnt);
			return -EINVAL;
		}

		if (cnt > CSID_NUM_CLK_MAX) {
			pr_err("%s: invalid cnt=%d, max is %d\n", __func__,
				cnt, CSID_NUM_CLK_MAX);
			return -EINVAL;
		}

		for (i = 0; i < cnt; i++) {
			count++;
			rc = of_property_read_string_index(of_node,
				"clock-names", i, &clock_name);
			CDBG("%s: clock_names[%d] = %s\n", __func__,
				i, clock_name);
			if (rc < 0) {
				pr_err("%s:%d, failed\n", __func__, __LINE__);
				return rc;
			}
			if (strcmp(clock_name, "csi_phy_src_clk") == 0)
				break;
		}
		csid_dev->num_clk = count;
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node, "clock-names",
				i, &(csid_clk_info[i].clk_name));
		CDBG("%s: clock-names[%d] = %s\n", __func__,
			i, csid_clk_info[i].clk_name);
		if (rc < 0) {
			pr_err("%s:%d, failed\n", __func__, __LINE__);
			return rc;
		}
	}
	rc = of_property_read_u32_array(of_node, "qcom,clock-rates",
		rates, count);
	if (rc < 0) {
		pr_err("%s:%d, failed", __func__, __LINE__);
		return rc;
	}
	for (i = 0; i < count; i++) {
		csid_clk_info[i].clk_rate = (rates[i] == 0) ?
			(long)-1 : rates[i];
		if (!strcmp(csid_clk_info[i].clk_name, "csi_src_clk")) {
			CDBG("%s:%d, copy csi_src_clk",
				__func__, __LINE__);
			csid_dev->csid_max_clk = rates[i];
			csid_dev->csid_clk_index = i;
		}
		CDBG("%s: clk_rate[%d] = %ld\n", __func__, i,
			csid_clk_info[i].clk_rate);
	}

	if (csid_dev->hw_dts_version == CSID_VERSION_V22) {
		csid_dev->num_clk_src_info = cnt - count;
		CDBG("%s: count = %d\n", __func__, (cnt - count));

		for (i = count; i < cnt; i++) {
			ii++;
			rc = of_property_read_string_index(of_node,
				"clock-names", i,
				&(csid_clk_src_info[ii].clk_name));
			CDBG("%s: clock-names[%d] = %s\n", __func__,
				ii, csid_clk_src_info[ii].clk_name);
			if (rc < 0) {
				pr_err("%s:%d, failed\n", __func__, __LINE__);
				return rc;
			}
		}
		ii = 0;
		rc = of_property_read_u32_array(of_node, "qcom,clock-rates",
			rates, cnt);
		if (rc < 0) {
			pr_err("%s:%d, failed", __func__, __LINE__);
			return rc;
		}
		for (i = count; i < cnt; i++) {
			ii++;
			csid_clk_src_info[ii].clk_rate = rates[i];
			CDBG("%s: clk_rate[%d] = %ld\n", __func__, ii,
			csid_clk_src_info[ii].clk_rate);
		}
	}
	return 0;
}

static int csid_probe(struct platform_device *pdev)
{
	struct csid_device *new_csid_dev;
	uint32_t csi_vdd_voltage = 0;
	int rc = 0;
	new_csid_dev = kzalloc(sizeof(struct csid_device), GFP_KERNEL);
	if (!new_csid_dev) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}

	new_csid_dev->ctrl_reg = NULL;
	new_csid_dev->ctrl_reg = kzalloc(sizeof(struct csid_ctrl_t),
		GFP_KERNEL);
	if (!new_csid_dev->ctrl_reg) {
		pr_err("%s:%d kzalloc failed\n", __func__, __LINE__);
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

	rc = msm_csid_get_clk_info(new_csid_dev, pdev);
	if (rc < 0) {
		pr_err("%s: msm_csid_get_clk_info() failed", __func__);
		return -EFAULT;
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

#ifdef CONFIG_COMPAT
	msm_csid_v4l2_subdev_fops = v4l2_subdev_fops;
	msm_csid_v4l2_subdev_fops.compat_ioctl32 = msm_csid_subdev_fops_ioctl32;
	new_csid_dev->msm_sd.sd.devnode->fops = &msm_csid_v4l2_subdev_fops;
#endif

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

	if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v2.0")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v2_0;
		new_csid_dev->hw_dts_version = CSID_VERSION_V20;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v2.2")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v2_2;
		new_csid_dev->hw_dts_version = CSID_VERSION_V22;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v3.0")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_0;
		new_csid_dev->hw_dts_version = CSID_VERSION_V30;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v4.0")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_0;
		new_csid_dev->hw_dts_version = CSID_VERSION_V40;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v3.1")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_1;
		new_csid_dev->hw_dts_version = CSID_VERSION_V31;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v3.2")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_2;
		new_csid_dev->hw_dts_version = CSID_VERSION_V32;
	} else {
		pr_err("%s:%d, invalid hw version : 0x%x", __func__, __LINE__,
		new_csid_dev->hw_dts_version);
		return -EINVAL;
	}

	new_csid_dev->csid_state = CSID_POWER_DOWN;
	return 0;

csid_no_resource:
	mutex_destroy(&new_csid_dev->mutex);
	kfree(new_csid_dev->ctrl_reg);
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
