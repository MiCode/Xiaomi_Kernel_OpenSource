/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>

#include "mdss.h"
#include "mdss_panel.h"
#include "mdss_dsi.h"
#include "mdss_debug.h"

static unsigned char *mdss_dsi_base;

static int mdss_dsi_regulator_init(struct platform_device *pdev)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_drv_cm_data *dsi_drv = NULL;

	if (!pdev) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = platform_get_drvdata(pdev);
	if (!ctrl_pdata) {
		pr_err("%s: invalid driver data\n", __func__);
		return -EINVAL;
	}

	dsi_drv = &(ctrl_pdata->shared_pdata);
	if (ctrl_pdata->power_data.num_vreg > 0) {
		ret = msm_dss_config_vreg(&pdev->dev,
				ctrl_pdata->power_data.vreg_config,
				ctrl_pdata->power_data.num_vreg, 1);
	} else {
		dsi_drv->vdd_vreg = devm_regulator_get(&pdev->dev, "vdd");
		if (IS_ERR(dsi_drv->vdd_vreg)) {
			pr_err("%s: could not get vdda vreg, rc=%ld\n",
				__func__, PTR_ERR(dsi_drv->vdd_vreg));
			return PTR_ERR(dsi_drv->vdd_vreg);
		}

		ret = regulator_set_voltage(dsi_drv->vdd_vreg, 3000000,
				3000000);
		if (ret) {
			pr_err("%s: set voltage failed on vdda vreg, rc=%d\n",
				__func__, ret);
			return ret;
		}

		dsi_drv->vdd_io_vreg = devm_regulator_get(&pdev->dev, "vddio");
		if (IS_ERR(dsi_drv->vdd_io_vreg)) {
			pr_err("%s: could not get vddio reg, rc=%ld\n",
				__func__, PTR_ERR(dsi_drv->vdd_io_vreg));
			return PTR_ERR(dsi_drv->vdd_io_vreg);
		}

		ret = regulator_set_voltage(dsi_drv->vdd_io_vreg, 1800000,
				1800000);
		if (ret) {
			pr_err("%s: set voltage failed on vddio vreg, rc=%d\n",
				__func__, ret);
			return ret;
		}

		dsi_drv->vdda_vreg = devm_regulator_get(&pdev->dev, "vdda");
		if (IS_ERR(dsi_drv->vdda_vreg)) {
			pr_err("%s: could not get vdda vreg, rc=%ld\n",
				__func__, PTR_ERR(dsi_drv->vdda_vreg));
			return PTR_ERR(dsi_drv->vdda_vreg);
		}

		ret = regulator_set_voltage(dsi_drv->vdda_vreg, 1200000,
				1200000);
		if (ret) {
			pr_err("%s: set voltage failed on vdda vreg, rc=%d\n",
				__func__, ret);
			return ret;
		}
	}

	return 0;
}

static int mdss_dsi_panel_power_on(struct mdss_panel_data *pdata, int enable)
{
	int ret;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	pr_debug("%s: enable=%d\n", __func__, enable);

	if (enable) {
		if (ctrl_pdata->power_data.num_vreg > 0) {
			ret = msm_dss_enable_vreg(
				ctrl_pdata->power_data.vreg_config,
				ctrl_pdata->power_data.num_vreg, 1);
			if (ret) {
				pr_err("%s:Failed to enable regulators.rc=%d\n",
					__func__, ret);
				return ret;
			}

			/*
			 * A small delay is needed here after enabling
			 * all regulators and before issuing panel reset
			 */
			msleep(20);
		} else {
			ret = regulator_set_optimum_mode(
				(ctrl_pdata->shared_pdata).vdd_vreg, 100000);
			if (ret < 0) {
				pr_err("%s: vdd_vreg set opt mode failed.\n",
					 __func__);
				return ret;
			}

			ret = regulator_set_optimum_mode(
				(ctrl_pdata->shared_pdata).vdd_io_vreg, 100000);
			if (ret < 0) {
				pr_err("%s: vdd_io_vreg set opt mode failed.\n",
					__func__);
				return ret;
			}

			ret = regulator_set_optimum_mode
			  ((ctrl_pdata->shared_pdata).vdda_vreg, 100000);
			if (ret < 0) {
				pr_err("%s: vdda_vreg set opt mode failed.\n",
					__func__);
				return ret;
			}

			ret = regulator_enable(
				(ctrl_pdata->shared_pdata).vdd_io_vreg);
			if (ret) {
				pr_err("%s: Failed to enable regulator.\n",
					__func__);
				return ret;
			}
			msleep(20);

			ret = regulator_enable(
				(ctrl_pdata->shared_pdata).vdd_vreg);
			if (ret) {
				pr_err("%s: Failed to enable regulator.\n",
					__func__);
				return ret;
			}
			msleep(20);

			ret = regulator_enable(
				(ctrl_pdata->shared_pdata).vdda_vreg);
			if (ret) {
				pr_err("%s: Failed to enable regulator.\n",
					__func__);
				return ret;
			}
		}

		if (pdata->panel_info.panel_power_on == 0)
			mdss_dsi_panel_reset(pdata, 1);

	} else {

		mdss_dsi_panel_reset(pdata, 0);

		if (ctrl_pdata->power_data.num_vreg > 0) {
			ret = msm_dss_enable_vreg(
				ctrl_pdata->power_data.vreg_config,
				ctrl_pdata->power_data.num_vreg, 0);
			if (ret) {
				pr_err("%s: Failed to disable regs.rc=%d\n",
					__func__, ret);
				return ret;
			}
		} else {
			ret = regulator_disable(
				(ctrl_pdata->shared_pdata).vdd_vreg);
			if (ret) {
				pr_err("%s: Failed to disable regulator.\n",
					__func__);
				return ret;
			}

			ret = regulator_disable(
				(ctrl_pdata->shared_pdata).vdda_vreg);
			if (ret) {
				pr_err("%s: Failed to disable regulator.\n",
					__func__);
				return ret;
			}

			ret = regulator_disable(
				(ctrl_pdata->shared_pdata).vdd_io_vreg);
			if (ret) {
				pr_err("%s: Failed to disable regulator.\n",
					__func__);
				return ret;
			}

			ret = regulator_set_optimum_mode(
				(ctrl_pdata->shared_pdata).vdd_vreg, 100);
			if (ret < 0) {
				pr_err("%s: vdd_vreg set opt mode failed.\n",
					 __func__);
				return ret;
			}

			ret = regulator_set_optimum_mode(
				(ctrl_pdata->shared_pdata).vdd_io_vreg, 100);
			if (ret < 0) {
				pr_err("%s: vdd_io_vreg set opt mode failed.\n",
					__func__);
				return ret;
			}
			ret = regulator_set_optimum_mode(
				(ctrl_pdata->shared_pdata).vdda_vreg, 100);
			if (ret < 0) {
				pr_err("%s: vdda_vreg set opt mode failed.\n",
					__func__);
				return ret;
			}
		}
	}
	return 0;
}

static void mdss_dsi_put_dt_vreg_data(struct device *dev,
	struct dss_module_power *module_power)
{
	if (!module_power) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	if (module_power->vreg_config) {
		devm_kfree(dev, module_power->vreg_config);
		module_power->vreg_config = NULL;
	}
	module_power->num_vreg = 0;
}

static int mdss_dsi_get_dt_vreg_data(struct device *dev,
	struct dss_module_power *mp)
{
	int i, rc = 0;
	int dt_vreg_total = 0;
	u32 *val_array = NULL;
	struct device_node *of_node = NULL;

	if (!dev || !mp) {
		pr_err("%s: invalid input\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	of_node = dev->of_node;

	mp->num_vreg = 0;
	dt_vreg_total = of_property_count_strings(of_node, "qcom,supply-names");
	if (dt_vreg_total < 0) {
		pr_debug("%s: vreg not found. rc=%d\n", __func__,
			dt_vreg_total);
		rc = 0;
		goto error;
	} else {
		pr_debug("%s: vreg found. count=%d\n", __func__, dt_vreg_total);
	}

	if (dt_vreg_total > 0) {
		mp->num_vreg = dt_vreg_total;
		mp->vreg_config = devm_kzalloc(dev, sizeof(struct dss_vreg) *
			dt_vreg_total, GFP_KERNEL);
		if (!mp->vreg_config) {
			pr_err("%s: can't alloc vreg mem\n", __func__);
			goto error;
		}
	} else {
		pr_debug("%s: no vreg\n", __func__);
		return 0;
	}

	val_array = devm_kzalloc(dev, sizeof(u32) * dt_vreg_total, GFP_KERNEL);
	if (!val_array) {
		pr_err("%s: can't allocate vreg scratch mem\n", __func__);
		rc = -ENOMEM;
		goto error;
	}

	for (i = 0; i < dt_vreg_total; i++) {
		const char *st = NULL;
		/* vreg-name */
		rc = of_property_read_string_index(of_node, "qcom,supply-names",
			i, &st);
		if (rc) {
			pr_err("%s: error reading name. i=%d, rc=%d\n",
				__func__, i, rc);
			goto error;
		}
		snprintf(mp->vreg_config[i].vreg_name,
			ARRAY_SIZE((mp->vreg_config[i].vreg_name)), "%s", st);

		/* vreg-min-voltage */
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,supply-min-voltage-level", val_array,
			dt_vreg_total);
		if (rc) {
			pr_err("%s: error reading min volt. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].min_voltage = val_array[i];

		/* vreg-max-voltage */
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,supply-max-voltage-level", val_array,
			dt_vreg_total);
		if (rc) {
			pr_err("%s: error reading max volt. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].max_voltage = val_array[i];

		/* vreg-peak-current*/
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,supply-peak-current", val_array,
			dt_vreg_total);
		if (rc) {
			pr_err("%s: error reading peak current. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].peak_current = val_array[i];

		pr_debug("%s: %s min=%d, max=%d, pc=%d\n", __func__,
			mp->vreg_config[i].vreg_name,
			mp->vreg_config[i].min_voltage,
			mp->vreg_config[i].max_voltage,
			mp->vreg_config[i].peak_current);
	}

	devm_kfree(dev, val_array);

	return rc;

error:
	if (mp->vreg_config) {
		devm_kfree(dev, mp->vreg_config);
		mp->vreg_config = NULL;
	}
	mp->num_vreg = 0;

	if (val_array)
		devm_kfree(dev, val_array);
	return rc;
}

static int mdss_dsi_off(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (!pdata->panel_info.panel_power_on) {
		pr_warn("%s:%d Panel already off.\n", __func__, __LINE__);
		return -EPERM;
	}

	pdata->panel_info.panel_power_on = 0;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s+: ctrl=%p ndx=%d\n", __func__,
				ctrl_pdata, ctrl_pdata->ndx);

	if (pdata->panel_info.type == MIPI_CMD_PANEL)
		mdss_dsi_clk_ctrl(ctrl_pdata, 1);

	/* disable DSI controller */
	mdss_dsi_controller_cfg(0, pdata);

	mdss_dsi_clk_ctrl(ctrl_pdata, 0);

	ret = mdss_dsi_enable_bus_clocks(ctrl_pdata);
	if (ret) {
		pr_err("%s: failed to enable bus clocks. rc=%d\n", __func__,
			ret);
		mdss_dsi_panel_power_on(pdata, 0);
		return ret;
	}

	/* disable DSI phy */
	mdss_dsi_phy_enable(ctrl_pdata, 0);

	mdss_dsi_disable_bus_clocks(ctrl_pdata);

	ret = mdss_dsi_panel_power_on(pdata, 0);
	if (ret) {
		pr_err("%s: Panel power off failed\n", __func__);
		return ret;
	}

	pr_debug("%s-:\n", __func__);

	return ret;
}

int mdss_dsi_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	u32 clk_rate;
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	u32 hbp, hfp, vbp, vfp, hspw, vspw, width, height;
	u32 ystride, bpp, data, dst_bpp;
	u32 dummy_xres, dummy_yres;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (pdata->panel_info.panel_power_on) {
		pr_warn("%s:%d Panel already on.\n", __func__, __LINE__);
		return 0;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s+: ctrl=%p ndx=%d\n",
				__func__, ctrl_pdata, ctrl_pdata->ndx);

	pinfo = &pdata->panel_info;

	ret = mdss_dsi_panel_power_on(pdata, 1);
	if (ret) {
		pr_err("%s: Panel power on failed\n", __func__);
		return ret;
	}

	pdata->panel_info.panel_power_on = 1;

	ret = mdss_dsi_enable_bus_clocks(ctrl_pdata);
	if (ret) {
		pr_err("%s: failed to enable bus clocks. rc=%d\n", __func__,
			ret);
		mdss_dsi_panel_power_on(pdata, 0);
		return ret;
	}

	mdss_dsi_phy_sw_reset((ctrl_pdata->ctrl_base));
	mdss_dsi_phy_init(pdata);
	mdss_dsi_disable_bus_clocks(ctrl_pdata);

	mdss_dsi_clk_ctrl(ctrl_pdata, 1);

	clk_rate = pdata->panel_info.clk_rate;
	clk_rate = min(clk_rate, pdata->panel_info.clk_max);

	dst_bpp = pdata->panel_info.fbc.enabled ?
		(pdata->panel_info.fbc.target_bpp) : (pinfo->bpp);

	hbp = mult_frac(pdata->panel_info.lcdc.h_back_porch, dst_bpp,
			pdata->panel_info.bpp);
	hfp = mult_frac(pdata->panel_info.lcdc.h_front_porch, dst_bpp,
			pdata->panel_info.bpp);
	vbp = mult_frac(pdata->panel_info.lcdc.v_back_porch, dst_bpp,
			pdata->panel_info.bpp);
	vfp = mult_frac(pdata->panel_info.lcdc.v_front_porch, dst_bpp,
			pdata->panel_info.bpp);
	hspw = mult_frac(pdata->panel_info.lcdc.h_pulse_width, dst_bpp,
			pdata->panel_info.bpp);
	vspw = pdata->panel_info.lcdc.v_pulse_width;
	width = mult_frac(pdata->panel_info.xres, dst_bpp,
			pdata->panel_info.bpp);
	height = pdata->panel_info.yres;

	mipi  = &pdata->panel_info.mipi;
	if (pdata->panel_info.type == MIPI_VIDEO_PANEL) {
		dummy_xres = pdata->panel_info.lcdc.xres_pad;
		dummy_yres = pdata->panel_info.lcdc.yres_pad;

		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x24,
			((hspw + hbp + width + dummy_xres) << 16 |
			(hspw + hbp)));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x28,
			((vspw + vbp + height + dummy_yres) << 16 |
			(vspw + vbp)));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2C,
			(vspw + vbp + height + dummy_yres +
				vfp - 1) << 16 | (hspw + hbp +
				width + dummy_xres + hfp - 1));

		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x30, (hspw << 16));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x34, 0);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x38, (vspw << 16));

	} else {		/* command mode */
		if (mipi->dst_format == DSI_CMD_DST_FORMAT_RGB888)
			bpp = 3;
		else if (mipi->dst_format == DSI_CMD_DST_FORMAT_RGB666)
			bpp = 3;
		else if (mipi->dst_format == DSI_CMD_DST_FORMAT_RGB565)
			bpp = 2;
		else
			bpp = 3;	/* Default format set to RGB888 */

		ystride = width * bpp + 1;

		/* DSI_COMMAND_MODE_MDP_STREAM_CTRL */
		data = (ystride << 16) | (mipi->vc << 8) | DTYPE_DCS_LWRITE;
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x60, data);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x58, data);

		/* DSI_COMMAND_MODE_MDP_STREAM_TOTAL */
		data = height << 16 | width;
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x64, data);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x5C, data);
	}

	mdss_dsi_sw_reset(pdata);
	mdss_dsi_host_init(mipi, pdata);

	if (mipi->force_clk_lane_hs) {
		u32 tmp;

		tmp = MIPI_INP((ctrl_pdata->ctrl_base) + 0xac);
		tmp |= (1<<28);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0xac, tmp);
		wmb();
	}

	if (pdata->panel_info.type == MIPI_CMD_PANEL)
		mdss_dsi_clk_ctrl(ctrl_pdata, 0);

	pr_debug("%s-:\n", __func__);
	return 0;
}

static int mdss_dsi_unblank(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pr_debug("%s+:\n", __func__);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	mipi  = &pdata->panel_info.mipi;

	if (!(ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT)) {
		ret = ctrl_pdata->on(pdata);
		if (ret) {
			pr_err("%s: unable to initialize the panel\n",
							__func__);
			return ret;
		}
		ctrl_pdata->ctrl_state |= CTRL_STATE_PANEL_INIT;
	}
	mdss_dsi_op_mode_config(mipi->mode, pdata);

	pr_debug("%s-:\n", __func__);

	return ret;
}

static int mdss_dsi_blank(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pr_debug("%s+:\n", __func__);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	mdss_dsi_op_mode_config(DSI_CMD_MODE, pdata);

	if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
		ret = ctrl_pdata->off(pdata);
		if (ret) {
			pr_err("%s: Panel OFF failed\n", __func__);
			return ret;
		}
		ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
	}
	pr_debug("%s-:End\n", __func__);
	return ret;
}

int mdss_dsi_cont_splash_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pr_info("%s:%d DSI on for continuous splash.\n", __func__, __LINE__);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	mipi = &pdata->panel_info.mipi;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s+: ctrl=%p ndx=%d\n", __func__,
				ctrl_pdata, ctrl_pdata->ndx);

	WARN((ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT),
		"Incorrect Ctrl state=0x%x\n", ctrl_pdata->ctrl_state);

	mdss_dsi_sw_reset(pdata);
	mdss_dsi_host_init(mipi, pdata);

	if (ctrl_pdata->on_cmds.link_state == DSI_LP_MODE) {
		mdss_dsi_op_mode_config(DSI_CMD_MODE, pdata);
		ret = mdss_dsi_unblank(pdata);
		if (ret) {
			pr_err("%s: unblank failed\n", __func__);
			return ret;
		}
	}

	pr_debug("%s-:End\n", __func__);
	return ret;
}

static int mdss_dsi_event_handler(struct mdss_panel_data *pdata,
				  int event, void *arg)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	pr_debug("%s+:event=%d\n", __func__, event);

	switch (event) {
	case MDSS_EVENT_UNBLANK:
		rc = mdss_dsi_on(pdata);
		if (ctrl_pdata->on_cmds.link_state == DSI_LP_MODE)
			rc = mdss_dsi_unblank(pdata);
		break;
	case MDSS_EVENT_PANEL_ON:
		ctrl_pdata->ctrl_state |= CTRL_STATE_MDP_ACTIVE;
		if (ctrl_pdata->on_cmds.link_state == DSI_HS_MODE)
			rc = mdss_dsi_unblank(pdata);
		break;
	case MDSS_EVENT_BLANK:
		if (ctrl_pdata->off_cmds.link_state == DSI_HS_MODE)
			rc = mdss_dsi_blank(pdata);
		break;
	case MDSS_EVENT_PANEL_OFF:
		ctrl_pdata->ctrl_state &= ~CTRL_STATE_MDP_ACTIVE;
		if (ctrl_pdata->off_cmds.link_state == DSI_LP_MODE)
			rc = mdss_dsi_blank(pdata);
		rc = mdss_dsi_off(pdata);
		break;
	case MDSS_EVENT_CONT_SPLASH_FINISH:
		ctrl_pdata->ctrl_state &= ~CTRL_STATE_MDP_ACTIVE;
		if (ctrl_pdata->on_cmds.link_state == DSI_LP_MODE) {
			rc = mdss_dsi_cont_splash_on(pdata);
		} else {
			pr_debug("%s:event=%d, Dsi On not called: ctrl_state: %d\n",
				 __func__, event,
				 ctrl_pdata->on_cmds.link_state);
			rc = -EINVAL;
		}
		break;
	case MDSS_EVENT_PANEL_CLK_CTRL:
		mdss_dsi_clk_req(ctrl_pdata, (int)arg);
		break;
	case MDSS_EVENT_DSI_CMDLIST_KOFF:
		mdss_dsi_cmdlist_commit(ctrl_pdata, 1);
		break;
	case MDSS_EVENT_CONT_SPLASH_BEGIN:
		if (ctrl_pdata->off_cmds.link_state == DSI_HS_MODE) {
			/* Panel is Enabled in Bootloader */
			rc = mdss_dsi_blank(pdata);
		}
		break;
	default:
		pr_debug("%s: unhandled event=%d\n", __func__, event);
		break;
	}
	pr_debug("%s-:event=%d, rc=%d\n", __func__, event, rc);
	return rc;
}

static int __devinit mdss_dsi_ctrl_probe(struct platform_device *pdev)
{
	int rc = 0;
	u32 index;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdev->dev.of_node) {
		struct resource *mdss_dsi_mres;
		const char *ctrl_name;

		ctrl_pdata = platform_get_drvdata(pdev);
		if (!ctrl_pdata) {
			ctrl_pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct mdss_dsi_ctrl_pdata), GFP_KERNEL);
			if (!ctrl_pdata) {
				pr_err("%s: FAILED: cannot alloc dsi ctrl\n",
					__func__);
				rc = -ENOMEM;
				goto error_no_mem;
			}
			platform_set_drvdata(pdev, ctrl_pdata);
		}

		ctrl_name = of_get_property(pdev->dev.of_node, "label", NULL);
		if (!ctrl_name)
			pr_info("%s:%d, DSI Ctrl name not specified\n",
						__func__, __LINE__);
		else
			pr_info("%s: DSI Ctrl name = %s\n",
				__func__, ctrl_name);

		rc = of_property_read_u32(pdev->dev.of_node,
					  "cell-index", &index);
		if (rc) {
			dev_err(&pdev->dev,
				"%s: Cell-index not specified, rc=%d\n",
							__func__, rc);
			goto error_no_mem;
		}

		if (index == 0)
			pdev->id = 1;
		else
			pdev->id = 2;

		mdss_dsi_mres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!mdss_dsi_mres) {
			pr_err("%s:%d unable to get the MDSS resources",
				       __func__, __LINE__);
			rc = -ENOMEM;
			goto error_no_mem;
		}
		if (mdss_dsi_mres) {
			mdss_dsi_base = ioremap(mdss_dsi_mres->start,
				resource_size(mdss_dsi_mres));
			if (!mdss_dsi_base) {
				pr_err("%s:%d unable to remap dsi resources",
					       __func__, __LINE__);
				rc = -ENOMEM;
				goto error_no_mem;
			}
		}

		rc = of_platform_populate(pdev->dev.of_node,
					NULL, NULL, &pdev->dev);
		if (rc) {
			dev_err(&pdev->dev,
				"%s: failed to add child nodes, rc=%d\n",
							__func__, rc);
			goto error_ioremap;
		}

		/* Parse the regulator information */
		rc = mdss_dsi_get_dt_vreg_data(&pdev->dev,
			&ctrl_pdata->power_data);
		if (rc) {
			pr_err("%s: failed to get vreg data from dt. rc=%d\n",
				__func__, rc);
			goto error_vreg;
		}

		pr_debug("%s: Dsi Ctrl->%d initialized\n", __func__, index);
	}

	return 0;

error_ioremap:
	iounmap(mdss_dsi_base);
error_no_mem:
	devm_kfree(&pdev->dev, ctrl_pdata);
error_vreg:
	mdss_dsi_put_dt_vreg_data(&pdev->dev, &ctrl_pdata->power_data);

	return rc;
}

static int __devexit mdss_dsi_ctrl_remove(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);

	if (!ctrl_pdata) {
		pr_err("%s: no driver data\n", __func__);
		return -ENODEV;
	}

	if (msm_dss_config_vreg(&pdev->dev,
			ctrl_pdata->power_data.vreg_config,
			ctrl_pdata->power_data.num_vreg, 1) < 0)
		pr_err("%s: failed to de-init vregs\n", __func__);
	mdss_dsi_put_dt_vreg_data(&pdev->dev, &ctrl_pdata->power_data);
	mfd = platform_get_drvdata(pdev);
	iounmap(mdss_dsi_base);
	return 0;
}

struct device dsi_dev;

int mdss_dsi_retrieve_ctrl_resources(struct platform_device *pdev, int mode,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0;
	u32 index;
	struct resource *mdss_dsi_mres;

	rc = of_property_read_u32(pdev->dev.of_node, "cell-index", &index);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: Cell-index not specified, rc=%d\n",
						__func__, rc);
		return rc;
	}

	if (index == 0) {
		if (mode != DISPLAY_1) {
			pr_err("%s:%d Panel->Ctrl mapping is wrong",
				       __func__, __LINE__);
			return -EPERM;
		}
	} else if (index == 1) {
		if (mode != DISPLAY_2) {
			pr_err("%s:%d Panel->Ctrl mapping is wrong",
				       __func__, __LINE__);
			return -EPERM;
		}
	} else {
		pr_err("%s:%d Unknown Ctrl mapped to panel",
			       __func__, __LINE__);
		return -EPERM;
	}

	mdss_dsi_mres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mdss_dsi_mres) {
		pr_err("%s:%d unable to get the DSI ctrl resources",
			       __func__, __LINE__);
		return -ENOMEM;
	}

	ctrl->ctrl_base = ioremap(mdss_dsi_mres->start,
		resource_size(mdss_dsi_mres));
	if (!(ctrl->ctrl_base)) {
		pr_err("%s:%d unable to remap dsi resources",
			       __func__, __LINE__);
		return -ENOMEM;
	}

	ctrl->reg_size = resource_size(mdss_dsi_mres);

	pr_info("%s: dsi base=%x size=%x\n",
		__func__, (int)ctrl->ctrl_base, ctrl->reg_size);

	return 0;
}


int dsi_panel_device_register(struct platform_device *pdev,
			      struct mdss_panel_common_pdata *panel_data)
{
	struct mipi_panel_info *mipi;
	int rc;
	u8 lanes = 0, bpp;
	u32 h_period, v_period, dsi_pclk_rate;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;
	struct device_node *dsi_ctrl_np = NULL;
	struct platform_device *ctrl_pdev = NULL;
	bool broadcast;
	bool cont_splash_enabled = false;

	h_period = ((panel_data->panel_info.lcdc.h_pulse_width)
			+ (panel_data->panel_info.lcdc.h_back_porch)
			+ (panel_data->panel_info.xres)
			+ (panel_data->panel_info.lcdc.h_front_porch));

	v_period = ((panel_data->panel_info.lcdc.v_pulse_width)
			+ (panel_data->panel_info.lcdc.v_back_porch)
			+ (panel_data->panel_info.yres)
			+ (panel_data->panel_info.lcdc.v_front_porch));

	mipi  = &panel_data->panel_info.mipi;

	panel_data->panel_info.type =
		((mipi->mode == DSI_VIDEO_MODE)
			? MIPI_VIDEO_PANEL : MIPI_CMD_PANEL);

	if (mipi->data_lane3)
		lanes += 1;
	if (mipi->data_lane2)
		lanes += 1;
	if (mipi->data_lane1)
		lanes += 1;
	if (mipi->data_lane0)
		lanes += 1;


	if ((mipi->dst_format == DSI_CMD_DST_FORMAT_RGB888)
	    || (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB888)
	    || (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB666_LOOSE))
		bpp = 3;
	else if ((mipi->dst_format == DSI_CMD_DST_FORMAT_RGB565)
		 || (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB565))
		bpp = 2;
	else
		bpp = 3;		/* Default format set to RGB888 */

	if (!panel_data->panel_info.clk_rate) {
		h_period += panel_data->panel_info.lcdc.xres_pad;
		v_period += panel_data->panel_info.lcdc.yres_pad;

		if (lanes > 0) {
			panel_data->panel_info.clk_rate =
			((h_period * v_period * (mipi->frame_rate) * bpp * 8)
			   / lanes);
		} else {
			pr_err("%s: forcing mdss_dsi lanes to 1\n", __func__);
			panel_data->panel_info.clk_rate =
				(h_period * v_period
					 * (mipi->frame_rate) * bpp * 8);
		}
	}
	pll_divider_config.clk_rate = panel_data->panel_info.clk_rate;

	rc = mdss_dsi_clk_div_config(bpp, lanes, &dsi_pclk_rate);
	if (rc) {
		pr_err("%s: unable to initialize the clk dividers\n", __func__);
		return rc;
	}

	if ((dsi_pclk_rate < 3300000) || (dsi_pclk_rate > 250000000))
		dsi_pclk_rate = 35000000;
	mipi->dsi_pclk_rate = dsi_pclk_rate;

	dsi_ctrl_np = of_parse_phandle(pdev->dev.of_node,
				       "qcom,dsi-ctrl-phandle", 0);
	if (!dsi_ctrl_np) {
		pr_err("%s: Dsi controller node not initialized\n", __func__);
		return -EPROBE_DEFER;
	}

	ctrl_pdev = of_find_device_by_node(dsi_ctrl_np);
	ctrl_pdata = platform_get_drvdata(ctrl_pdev);
	if (!ctrl_pdata) {
		pr_err("%s: no dsi ctrl driver data\n", __func__);
		return -EINVAL;
	}

	rc = mdss_dsi_regulator_init(ctrl_pdev);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: failed to init regulator, rc=%d\n",
						__func__, rc);
		return rc;
	}

	broadcast = of_property_read_bool(pdev->dev.of_node,
					  "qcom,mdss-pan-broadcast-mode");
	if (broadcast)
		ctrl_pdata->shared_pdata.broadcast_enable = 1;

	ctrl_pdata->disp_en_gpio = of_get_named_gpio(pdev->dev.of_node,
						     "qcom,enable-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_err("%s:%d, Disp_en gpio not specified\n",
						__func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->disp_en_gpio, "disp_enable");
		if (rc) {
			pr_err("request reset gpio failed, rc=%d\n",
			       rc);
			gpio_free(ctrl_pdata->disp_en_gpio);
			return -ENODEV;
		}
	}

	ctrl_pdata->disp_te_gpio = of_get_named_gpio(pdev->dev.of_node,
						     "qcom,te-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_te_gpio)) {
		pr_err("%s:%d, Disp_te gpio not specified\n",
						__func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->disp_te_gpio, "disp_te");
		if (rc) {
			pr_err("request TE gpio failed, rc=%d\n",
			       rc);
			gpio_free(ctrl_pdata->disp_te_gpio);
			return -ENODEV;
		}
		rc = gpio_tlmm_config(GPIO_CFG(
				ctrl_pdata->disp_te_gpio, 1,
				GPIO_CFG_INPUT,
				GPIO_CFG_PULL_DOWN,
				GPIO_CFG_2MA),
				GPIO_CFG_ENABLE);

		if (rc) {
			pr_err("%s: unable to config tlmm = %d\n",
				__func__, ctrl_pdata->disp_te_gpio);
			gpio_free(ctrl_pdata->disp_te_gpio);
			return -ENODEV;
		}

		rc = gpio_direction_input(ctrl_pdata->disp_te_gpio);
		if (rc) {
			pr_err("set_direction for disp_en gpio failed, rc=%d\n",
			       rc);
			gpio_free(ctrl_pdata->disp_te_gpio);
			return -ENODEV;
		}
		pr_debug("%s: te_gpio=%d\n", __func__,
					ctrl_pdata->disp_te_gpio);
	}


	ctrl_pdata->rst_gpio = of_get_named_gpio(pdev->dev.of_node,
						 "qcom,rst-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_err("%s:%d, reset gpio not specified\n",
						__func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->rst_gpio, "disp_rst_n");
		if (rc) {
			pr_err("request reset gpio failed, rc=%d\n",
				rc);
			gpio_free(ctrl_pdata->rst_gpio);
			if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
				gpio_free(ctrl_pdata->disp_en_gpio);
			return -ENODEV;
		}
	}

	if (mdss_dsi_clk_init(ctrl_pdev, ctrl_pdata)) {
		pr_err("%s: unable to initialize Dsi ctrl clks\n", __func__);
		return -EPERM;
	}

	if (mdss_dsi_retrieve_ctrl_resources(ctrl_pdev,
					     panel_data->panel_info.pdest,
					     ctrl_pdata)) {
		pr_err("%s: unable to get Dsi controller res\n", __func__);
		return -EPERM;
	}

	ctrl_pdata->panel_data.event_handler = mdss_dsi_event_handler;

	ctrl_pdata->on_cmds = panel_data->on_cmds;
	ctrl_pdata->off_cmds = panel_data->off_cmds;

	memcpy(&((ctrl_pdata->panel_data).panel_info),
				&(panel_data->panel_info),
				       sizeof(struct mdss_panel_info));

	ctrl_pdata->panel_data.set_backlight = panel_data->bl_fnc;
	ctrl_pdata->bklt_ctrl = panel_data->panel_info.bklt_ctrl;
	ctrl_pdata->pwm_pmic_gpio = panel_data->panel_info.pwm_pmic_gpio;
	ctrl_pdata->pwm_period = panel_data->panel_info.pwm_period;
	ctrl_pdata->pwm_lpg_chan = panel_data->panel_info.pwm_lpg_chan;
	ctrl_pdata->bklt_max = panel_data->panel_info.bl_max;

	if (ctrl_pdata->bklt_ctrl == BL_PWM)
		mdss_dsi_panel_pwm_cfg(ctrl_pdata);

	mdss_dsi_ctrl_init(ctrl_pdata);
	/*
	 * register in mdp driver
	 */

	ctrl_pdata->pclk_rate = dsi_pclk_rate;
	ctrl_pdata->byte_clk_rate = panel_data->panel_info.clk_rate / 8;
	pr_debug("%s: pclk=%d, bclk=%d\n", __func__,
			ctrl_pdata->pclk_rate, ctrl_pdata->byte_clk_rate);

	ctrl_pdata->ctrl_state = CTRL_STATE_UNKNOWN;
	cont_splash_enabled = of_property_read_bool(pdev->dev.of_node,
			"qcom,cont-splash-enabled");
	if (!cont_splash_enabled) {
		pr_info("%s:%d Continuous splash flag not found.\n",
				__func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.cont_splash_enabled = 0;
		ctrl_pdata->panel_data.panel_info.panel_power_on = 0;
	} else {
		pr_info("%s:%d Continuous splash flag enabled.\n",
				__func__, __LINE__);

		ctrl_pdata->panel_data.panel_info.cont_splash_enabled = 1;
		ctrl_pdata->panel_data.panel_info.panel_power_on = 1;
		rc = mdss_dsi_panel_power_on(&(ctrl_pdata->panel_data), 1);
		if (rc) {
			pr_err("%s: Panel power on failed\n", __func__);
			return rc;
		}

		mdss_dsi_clk_ctrl(ctrl_pdata, 1);
		ctrl_pdata->ctrl_state |=
			(CTRL_STATE_PANEL_INIT | CTRL_STATE_MDP_ACTIVE);
	}

	rc = mdss_register_panel(ctrl_pdev, &(ctrl_pdata->panel_data));
	if (rc) {
		dev_err(&pdev->dev, "unable to register MIPI DSI panel\n");
		if (ctrl_pdata->rst_gpio)
			gpio_free(ctrl_pdata->rst_gpio);
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_free(ctrl_pdata->disp_en_gpio);
		return rc;
	}

	ctrl_pdata->on = panel_data->on;
	ctrl_pdata->off = panel_data->off;

	if (panel_data->panel_info.pdest == DISPLAY_1) {
		mdss_debug_register_base("dsi0",
			ctrl_pdata->ctrl_base, ctrl_pdata->reg_size);
		ctrl_pdata->ndx = 0;
	} else {
		mdss_debug_register_base("dsi1",
			ctrl_pdata->ctrl_base, ctrl_pdata->reg_size);
		ctrl_pdata->ndx = 1;
	}

	pr_debug("%s: Panal data initialized\n", __func__);
	return 0;
}

static const struct of_device_id mdss_dsi_ctrl_dt_match[] = {
	{.compatible = "qcom,mdss-dsi-ctrl"},
	{}
};
MODULE_DEVICE_TABLE(of, mdss_dsi_ctrl_dt_match);

static struct platform_driver mdss_dsi_ctrl_driver = {
	.probe = mdss_dsi_ctrl_probe,
	.remove = __devexit_p(mdss_dsi_ctrl_remove),
	.shutdown = NULL,
	.driver = {
		.name = "mdss_dsi_ctrl",
		.of_match_table = mdss_dsi_ctrl_dt_match,
	},
};

static int mdss_dsi_register_driver(void)
{
	return platform_driver_register(&mdss_dsi_ctrl_driver);
}

static int __init mdss_dsi_driver_init(void)
{
	int ret;

	ret = mdss_dsi_register_driver();
	if (ret) {
		pr_err("mdss_dsi_register_driver() failed!\n");
		return ret;
	}

	return ret;
}
module_init(mdss_dsi_driver_init);

static void __exit mdss_dsi_driver_cleanup(void)
{
	iounmap(mdss_dsi_base);
	platform_driver_unregister(&mdss_dsi_ctrl_driver);
}
module_exit(mdss_dsi_driver_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DSI controller driver");
MODULE_AUTHOR("Chandan Uddaraju <chandanu@codeaurora.org>");
