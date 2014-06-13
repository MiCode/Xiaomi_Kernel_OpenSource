/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/leds-qpnp-wled.h>

#include "mdss.h"
#include "mdss_panel.h"
#include "mdss_dsi.h"
#include "mdss_debug.h"

static int mdss_dsi_pinctrl_set_state(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
					bool active);

static int mdss_dsi_regulator_init(struct platform_device *pdev)
{
	int rc = 0;

	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int i = 0;

	if (!pdev) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = platform_get_drvdata(pdev);
	if (!ctrl_pdata) {
		pr_err("%s: invalid driver data\n", __func__);
		return -EINVAL;
	}

	for (i = 0; !rc && (i < DSI_MAX_PM); i++) {
		rc = msm_dss_config_vreg(&pdev->dev,
			ctrl_pdata->power_data[i].vreg_config,
			ctrl_pdata->power_data[i].num_vreg, 1);
		if (rc)
			pr_err("%s: failed to init vregs for %s\n",
				__func__, __mdss_dsi_pm_name(i));
	}

	return rc;
}

static int mdss_dsi_panel_power_on(struct mdss_panel_data *pdata, int enable)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int i = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		ret = -EINVAL;
		goto error;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	pr_debug("%s: enable=%d\n", __func__, enable);

	/*
	 * If a dynamic mode switch is pending, the regulators should not
	 * be turned off or on.
	 */
	if (pdata->panel_info.dynamic_switch_pending)
		return 0;

	if (enable) {
		for (i = 0; i < DSI_MAX_PM; i++) {
			/*
			 * Core power module will be enabled when the
			 * clocks are enabled
			 */
			if (DSI_CORE_PM == i)
				continue;
			ret = msm_dss_enable_vreg(
				ctrl_pdata->power_data[i].vreg_config,
				ctrl_pdata->power_data[i].num_vreg, 1);
			if (ret) {
				pr_err("%s: failed to enable vregs for %s\n",
					__func__, __mdss_dsi_pm_name(i));
				goto error_enable;
			}
		}
		if (ctrl_pdata->panel_bias_vreg) {
			pr_debug("%s: Enable panel bias vreg. ndx = %d\n",
			       __func__, ctrl_pdata->ndx);
			if (qpnp_ibb_enable(true))
				pr_err("Unable to configure bias vreg\n");
			/*
			 * Add delay recommended by panel specs
			 */
			udelay(2000);
		}

		if (!pdata->panel_info.mipi.lp11_init) {
			if (mdss_dsi_pinctrl_set_state(ctrl_pdata, true))
				pr_debug("reset enable: pinctrl not enabled\n");

			ret = mdss_dsi_panel_reset(pdata, 1);
			if (ret) {
				pr_err("%s: Panel reset failed. rc=%d\n",
						__func__, ret);
				goto error_enable;
			}
		}
	} else {
		ret = mdss_dsi_panel_reset(pdata, 0);
		if (ret) {
			pr_err("%s: Panel reset failed. rc=%d\n",
					__func__, ret);
			goto error;
		}
		if (mdss_dsi_pinctrl_set_state(ctrl_pdata, false))
			pr_debug("reset disable: pinctrl not enabled\n");

		if (ctrl_pdata->panel_bias_vreg) {
			pr_debug("%s: Disabling panel bias vreg. ndx = %d\n",
			       __func__, ctrl_pdata->ndx);
			if (qpnp_ibb_enable(false))
				pr_err("Unable to disable bias vreg\n");
			/*
			 * Add delay recommended by panel specs
			 */
			udelay(2000);
		}
		for (i = DSI_MAX_PM - 1; i >= 0; i--) {
			/*
			 * Core power module will be disabled when the
			 * clocks are disabled
			 */
			if (DSI_CORE_PM == i)
				continue;
			ret = msm_dss_enable_vreg(
				ctrl_pdata->power_data[i].vreg_config,
				ctrl_pdata->power_data[i].num_vreg, 0);
			if (ret)
				pr_err("%s: failed to disable vregs for %s\n",
					__func__, __mdss_dsi_pm_name(i));
		}
	}

error_enable:
	if (ret) {
		for (; i >= 0; i--)
			msm_dss_enable_vreg(
				ctrl_pdata->power_data[i].vreg_config,
				ctrl_pdata->power_data[i].num_vreg, 0);
	}

error:
	return ret;
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
	struct dss_module_power *mp, enum dsi_pm_type module)
{
	int i = 0, rc = 0;
	u32 tmp = 0;
	struct device_node *of_node = NULL, *supply_node = NULL;
	const char *pm_supply_name = NULL;
	struct device_node *supply_root_node = NULL;

	if (!dev || !mp) {
		pr_err("%s: invalid input\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	of_node = dev->of_node;

	mp->num_vreg = 0;
	pm_supply_name = __mdss_dsi_pm_supply_node_name(module);
	supply_root_node = of_get_child_by_name(of_node, pm_supply_name);
	if (!supply_root_node) {
		pr_err("no supply entry present\n");
		goto novreg;
	}

	for_each_child_of_node(supply_root_node, supply_node) {
		mp->num_vreg++;
	}

	if (mp->num_vreg == 0) {
		pr_debug("%s: no vreg\n", __func__);
		goto novreg;
	} else {
		pr_debug("%s: vreg found. count=%d\n", __func__, mp->num_vreg);
	}

	mp->vreg_config = devm_kzalloc(dev, sizeof(struct dss_vreg) *
		mp->num_vreg, GFP_KERNEL);
	if (!mp->vreg_config) {
		pr_err("%s: can't alloc vreg mem\n", __func__);
		rc = -ENOMEM;
		goto error;
	}

	for_each_child_of_node(supply_root_node, supply_node) {
		const char *st = NULL;
		/* vreg-name */
		rc = of_property_read_string(supply_node,
			"qcom,supply-name", &st);
		if (rc) {
			pr_err("%s: error reading name. rc=%d\n",
				__func__, rc);
			goto error;
		}
		snprintf(mp->vreg_config[i].vreg_name,
			ARRAY_SIZE((mp->vreg_config[i].vreg_name)), "%s", st);
		/* vreg-min-voltage */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-min-voltage", &tmp);
		if (rc) {
			pr_err("%s: error reading min volt. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].min_voltage = tmp;

		/* vreg-max-voltage */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-max-voltage", &tmp);
		if (rc) {
			pr_err("%s: error reading max volt. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].max_voltage = tmp;

		/* enable-load */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-enable-load", &tmp);
		if (rc) {
			pr_err("%s: error reading enable load. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].enable_load = tmp;

		/* disable-load */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-disable-load", &tmp);
		if (rc) {
			pr_err("%s: error reading disable load. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].disable_load = tmp;

		/* pre-sleep */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-pre-on-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply pre sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].pre_on_sleep = tmp;
		}

		rc = of_property_read_u32(supply_node,
			"qcom,supply-pre-off-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply pre sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].pre_off_sleep = tmp;
		}

		/* post-sleep */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-post-on-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply post sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].post_on_sleep = tmp;
		}

		rc = of_property_read_u32(supply_node,
			"qcom,supply-post-off-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply post sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].post_off_sleep = tmp;
		}

		pr_debug("%s: %s min=%d, max=%d, enable=%d, disable=%d, preonsleep=%d, postonsleep=%d, preoffsleep=%d, postoffsleep=%d\n",
			__func__,
			mp->vreg_config[i].vreg_name,
			mp->vreg_config[i].min_voltage,
			mp->vreg_config[i].max_voltage,
			mp->vreg_config[i].enable_load,
			mp->vreg_config[i].disable_load,
			mp->vreg_config[i].pre_on_sleep,
			mp->vreg_config[i].post_on_sleep,
			mp->vreg_config[i].pre_off_sleep,
			mp->vreg_config[i].post_off_sleep
			);
		++i;
	}

	return rc;

error:
	if (mp->vreg_config) {
		devm_kfree(dev, mp->vreg_config);
		mp->vreg_config = NULL;
	}
novreg:
	mp->num_vreg = 0;

	return rc;
}

static int mdss_dsi_get_panel_cfg(char *panel_cfg)
{
	int rc;
	struct mdss_panel_cfg *pan_cfg = NULL;

	if (!panel_cfg)
		return MDSS_PANEL_INTF_INVALID;

	pan_cfg = mdss_panel_intf_type(MDSS_PANEL_INTF_DSI);
	if (IS_ERR(pan_cfg)) {
		return PTR_ERR(pan_cfg);
	} else if (!pan_cfg) {
		panel_cfg[0] = 0;
		return 0;
	}

	pr_debug("%s:%d: cfg:[%s]\n", __func__, __LINE__,
		 pan_cfg->arg_cfg);
	rc = strlcpy(panel_cfg, pan_cfg->arg_cfg,
		     sizeof(pan_cfg->arg_cfg));
	return rc;
}

static int mdss_dsi_off(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *panel_info = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (!pdata->panel_info.panel_power_on) {
		pr_warn("%s:%d Panel already off.\n", __func__, __LINE__);
		return 0;
	}

	pdata->panel_info.panel_power_on = 0;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	mutex_lock(&ctrl_pdata->mutex);
	panel_info = &ctrl_pdata->panel_data.panel_info;
	pr_debug("%s+: ctrl=%p ndx=%d\n", __func__,
				ctrl_pdata, ctrl_pdata->ndx);

	if (pdata->panel_info.type == MIPI_CMD_PANEL)
		mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 1);

	/* disable DSI controller */
	mdss_dsi_controller_cfg(0, pdata);

	/* disable DSI phy */
	mdss_dsi_phy_disable(ctrl_pdata);

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 0);

	ret = mdss_dsi_panel_power_on(pdata, 0);
	if (ret) {
		mutex_unlock(&ctrl_pdata->mutex);
		pr_err("%s: Panel power off failed\n", __func__);
		return ret;
	}

	if (panel_info->dynamic_fps
	    && (panel_info->dfps_update == DFPS_SUSPEND_RESUME_MODE)
	    && (panel_info->new_fps != panel_info->mipi.frame_rate))
		panel_info->mipi.frame_rate = panel_info->new_fps;

	mutex_unlock(&ctrl_pdata->mutex);
	pr_debug("%s-:\n", __func__);

	return ret;
}

static void __mdss_dsi_ctrl_setup(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	u32 clk_rate;
	u32 hbp, hfp, vbp, vfp, hspw, vspw, width, height;
	u32 ystride, bpp, data, dst_bpp;
	u32 dummy_xres = 0, dummy_yres = 0;
	u32 hsync_period, vsync_period;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pinfo = &pdata->panel_info;

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

	if (pdata->panel_info.type == MIPI_VIDEO_PANEL) {
		dummy_xres = pdata->panel_info.lcdc.xres_pad;
		dummy_yres = pdata->panel_info.lcdc.yres_pad;
	}

	vsync_period = vspw + vbp + height + dummy_yres + vfp;
	hsync_period = hspw + hbp + width + dummy_xres + hfp;

	mipi = &pdata->panel_info.mipi;
	if (pdata->panel_info.type == MIPI_VIDEO_PANEL) {
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x24,
			((hspw + hbp + width + dummy_xres) << 16 |
			(hspw + hbp)));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x28,
			((vspw + vbp + height + dummy_yres) << 16 |
			(vspw + vbp)));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2C,
				((vsync_period - 1) << 16)
				| (hsync_period - 1));

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
}

int mdss_dsi_ulps_config(struct mdss_dsi_ctrl_pdata *ctrl_pdata, int enable)
{
	int ret = 0;
	struct mdss_panel_data *pdata = NULL;
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	u32 lane_status = 0, regval;
	u32 active_lanes = 0, clamp_reg;
	u32 clamp_reg_off, phyrst_reg_off;

	if (!ctrl_pdata) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (&ctrl_pdata->mmss_misc_io == NULL) {
		pr_err("%s: mmss_misc_io is NULL. ULPS not valid\n", __func__);
		return -EINVAL;
	}

	pdata = &ctrl_pdata->panel_data;
	if (!pdata) {
		pr_err("%s: Invalid panel data\n", __func__);
		return -EINVAL;
	}
	pinfo = &pdata->panel_info;
	mipi = &pinfo->mipi;
	clamp_reg_off = ctrl_pdata->ulps_clamp_ctrl_off;
	phyrst_reg_off = ctrl_pdata->ulps_phyrst_ctrl_off;

	if (!mdss_dsi_ulps_feature_enabled(pdata)) {
		pr_debug("%s: ULPS feature not supported. enable=%d\n",
			__func__, enable);
		return -ENOTSUPP;
	}

	/*
	 * No need to enter ULPS when transitioning from splash screen to
	 * boot animation since it is expected that the clocks would be turned
	 * right back on.
	 */
	if (pinfo->cont_splash_enabled) {
		pr_debug("%s: skip ULPS config with splash screen enabled\n",
			__func__);
		return 0;
	}

	/* clock lane will always be programmed for ulps and will be clamped */
	active_lanes = BIT(4);
	clamp_reg = BIT(8) | BIT(9);
	/*
	 * make a note of all active data lanes for which ulps entry/exit
	 * as well as DSI clamps are needed
	 */
	if (mipi->data_lane0) {
		active_lanes |= BIT(0);
		clamp_reg |= (BIT(0) | BIT(1));
	}
	if (mipi->data_lane1) {
		active_lanes |= BIT(1);
		clamp_reg |= (BIT(2) | BIT(3));
	}
	if (mipi->data_lane2) {
		active_lanes |= BIT(2);
		clamp_reg |= (BIT(4) | BIT(5));
	}
	if (mipi->data_lane3) {
		active_lanes |= BIT(3);
		clamp_reg |= (BIT(6) | BIT(7));
	}

	pr_debug("%s: configuring ulps (%s) for ctrl%d, active lanes=0x%08x\n",
		__func__, (enable ? "on" : "off"), ctrl_pdata->ndx,
		active_lanes);

	if (enable && !ctrl_pdata->ulps) {
		/*
		 * ULPS Entry Request.
		 * Wait for a short duration to ensure that the lanes
		 * enter ULP state.
		 */
		MIPI_OUTP(ctrl_pdata->ctrl_base + 0x0AC, active_lanes);
		usleep(100);

		/* Check to make sure that all active data lanes are in ULPS */
		lane_status = MIPI_INP(ctrl_pdata->ctrl_base + 0xA8);
		if (lane_status & (active_lanes << 8)) {
			pr_err("%s: ULPS entry req failed for ctrl%d. Lane status=0x%08x\n",
				__func__, ctrl_pdata->ndx, lane_status);
			ret = -EINVAL;
			goto error;
		}

		/* Enable MMSS DSI Clamps */
		if (ctrl_pdata->ndx == DSI_CTRL_0) {
			regval = MIPI_INP(ctrl_pdata->mmss_misc_io.base +
				clamp_reg_off);
			MIPI_OUTP(ctrl_pdata->mmss_misc_io.base + clamp_reg_off,
				regval | clamp_reg);
			MIPI_OUTP(ctrl_pdata->mmss_misc_io.base + clamp_reg_off,
				regval | (clamp_reg | BIT(15)));
		} else if (ctrl_pdata->ndx == DSI_CTRL_1) {
			regval = MIPI_INP(ctrl_pdata->mmss_misc_io.base +
				clamp_reg_off);
			MIPI_OUTP(ctrl_pdata->mmss_misc_io.base + clamp_reg_off,
				regval | (clamp_reg << 16));
			MIPI_OUTP(ctrl_pdata->mmss_misc_io.base + clamp_reg_off,
				regval | ((clamp_reg << 16) | BIT(31)));
		}

		wmb();

		/*
		 * This register write ensures that DSI PHY will not be
		 * reset when mdss ahb clock reset is asserted while coming
		 * out of power collapse
		 */
		MIPI_OUTP(ctrl_pdata->mmss_misc_io.base + phyrst_reg_off, 0x1);
		ctrl_pdata->ulps = true;
	} else if (ctrl_pdata->ulps) {
		MIPI_OUTP(ctrl_pdata->mmss_misc_io.base + phyrst_reg_off, 0x0);
		if (ctrl_pdata->ctrl_rev == MDSS_DSI_HW_REV_103)
			mdss_dsi_20nm_phy_init(pdata);
		else
			mdss_dsi_phy_init(pdata);

		__mdss_dsi_ctrl_setup(pdata);
		mdss_dsi_sw_reset(pdata);
		mdss_dsi_host_init(pdata);
		mdss_dsi_op_mode_config(pdata->panel_info.mipi.mode,
			pdata);

		/*
		 * ULPS Entry Request. This is needed because, after power
		 * collapse and reset, the DSI controller resets back to
		 * idle state and not ULPS.
		 * Wait for a short duration to ensure that the lanes
		 * enter ULP state.
		 */
		MIPI_OUTP(ctrl_pdata->ctrl_base + 0x0AC, active_lanes);
		usleep(100);

		/* Disable MMSS DSI Clamps */
		if (ctrl_pdata->ndx == DSI_CTRL_0) {
			regval = MIPI_INP(ctrl_pdata->mmss_misc_io.base +
				clamp_reg_off);
			MIPI_OUTP(ctrl_pdata->mmss_misc_io.base + clamp_reg_off,
				regval & ~(clamp_reg | BIT(15)));
		} else if (ctrl_pdata->ndx == DSI_CTRL_1) {
			regval = MIPI_INP(ctrl_pdata->mmss_misc_io.base +
				clamp_reg_off);
			MIPI_OUTP(ctrl_pdata->mmss_misc_io.base + clamp_reg_off,
				regval & ~((clamp_reg << 16) | BIT(31)));
		}


		/*
		 * ULPS Exit Request
		 * Hardware requirement is to wait for at least 1ms
		 */
		MIPI_OUTP(ctrl_pdata->ctrl_base + 0x0AC, active_lanes << 8);
		usleep(1000);
		MIPI_OUTP(ctrl_pdata->ctrl_base + 0x0AC, 0x0);

		/*
		 * Wait for a short duration before enabling
		 * data transmission
		 */
		usleep(100);

		lane_status = MIPI_INP(ctrl_pdata->ctrl_base + 0xA8);
		ctrl_pdata->ulps = false;
	}

	pr_debug("%s: DSI lane status = 0x%08x. Ulps %s\n", __func__,
		lane_status, enable ? "enabled" : "disabled");

error:
	return ret;
}

static int mdss_dsi_update_panel_config(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
				int mode)
{
	int ret = 0;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (mode == DSI_CMD_MODE) {
		pinfo->mipi.mode = DSI_CMD_MODE;
		pinfo->type = MIPI_CMD_PANEL;
		pinfo->mipi.vsync_enable = 1;
		pinfo->mipi.hw_vsync_mode = 1;
	} else {	/*video mode*/
		pinfo->mipi.mode = DSI_VIDEO_MODE;
		pinfo->type = MIPI_VIDEO_PANEL;
		pinfo->mipi.vsync_enable = 0;
		pinfo->mipi.hw_vsync_mode = 0;
	}

	ctrl_pdata->panel_mode = pinfo->mipi.mode;
	mdss_panel_get_dst_fmt(pinfo->bpp, pinfo->mipi.mode,
			pinfo->mipi.pixel_packing, &(pinfo->mipi.dst_format));
	pinfo->cont_splash_enabled = 0;

	return ret;
}

int mdss_dsi_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
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
	mipi = &pdata->panel_info.mipi;

	ret = mdss_dsi_panel_power_on(pdata, 1);
	if (ret) {
		pr_err("%s:Panel power on failed. rc=%d\n", __func__, ret);
		return ret;
	}

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_BUS_CLKS, 1);
	if (ret) {
		pr_err("%s: failed to enable bus clocks. rc=%d\n", __func__,
			ret);
		ret = mdss_dsi_panel_power_on(pdata, 0);
		if (ret) {
			pr_err("%s: Panel reset failed. rc=%d\n",
					__func__, ret);
			return ret;
		}
		pdata->panel_info.panel_power_on = 0;
		return ret;
	}
	pdata->panel_info.panel_power_on = 1;

	ctrl_pdata->ctrl_rev = MIPI_INP(ctrl_pdata->ctrl_base);

	mdss_dsi_phy_sw_reset((ctrl_pdata->ctrl_base));
	if (ctrl_pdata->ctrl_rev == MDSS_DSI_HW_REV_103)
		mdss_dsi_20nm_phy_init(pdata);
	else
		mdss_dsi_phy_init(pdata);

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_BUS_CLKS, 0);

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 1);

	__mdss_dsi_ctrl_setup(pdata);
	mdss_dsi_sw_reset(pdata);
	mdss_dsi_host_init(pdata);

	/*
	 * Issue hardware reset line after enabling the DSI clocks and data
	 * data lanes for LP11 init
	 */
	if (mipi->lp11_init) {
		if (mdss_dsi_pinctrl_set_state(ctrl_pdata, true))
			pr_debug("reset enable: pinctrl not enabled\n");
		mdss_dsi_panel_reset(pdata, 1);
	}

	if (mipi->init_delay)
		usleep(mipi->init_delay);

	if (mipi->force_clk_lane_hs) {
		u32 tmp;

		tmp = MIPI_INP((ctrl_pdata->ctrl_base) + 0xac);
		tmp |= (1<<28);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0xac, tmp);
		wmb();
	}

	if (pdata->panel_info.type == MIPI_CMD_PANEL)
		mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 0);

	pr_debug("%s-:\n", __func__);
	return 0;
}

static int mdss_dsi_pinctrl_set_state(
	struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	bool active)
{
	struct pinctrl_state *pin_state;
	int rc = -EFAULT;

	if (IS_ERR_OR_NULL(ctrl_pdata->pin_res.pinctrl))
		return PTR_ERR(ctrl_pdata->pin_res.pinctrl);

	pin_state = active ? ctrl_pdata->pin_res.gpio_state_active
				: ctrl_pdata->pin_res.gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pin_state)) {
		rc = pinctrl_select_state(ctrl_pdata->pin_res.pinctrl,
				pin_state);
		if (rc)
			pr_err("%s: can not set %s pins\n", __func__,
			       active ? MDSS_PINCTRL_STATE_DEFAULT
			       : MDSS_PINCTRL_STATE_SLEEP);
	} else {
		pr_err("%s: invalid '%s' pinstate\n", __func__,
		       active ? MDSS_PINCTRL_STATE_DEFAULT
		       : MDSS_PINCTRL_STATE_SLEEP);
	}
	return rc;
}

static int mdss_dsi_pinctrl_init(struct platform_device *pdev)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;

	ctrl_pdata = platform_get_drvdata(pdev);
	ctrl_pdata->pin_res.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(ctrl_pdata->pin_res.pinctrl)) {
		pr_err("%s: failed to get pinctrl\n", __func__);
		return PTR_ERR(ctrl_pdata->pin_res.pinctrl);
	}

	ctrl_pdata->pin_res.gpio_state_active
		= pinctrl_lookup_state(ctrl_pdata->pin_res.pinctrl,
				MDSS_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(ctrl_pdata->pin_res.gpio_state_active))
		pr_warn("%s: can not get default pinstate\n", __func__);

	ctrl_pdata->pin_res.gpio_state_suspend
		= pinctrl_lookup_state(ctrl_pdata->pin_res.pinctrl,
				MDSS_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(ctrl_pdata->pin_res.gpio_state_suspend))
		pr_warn("%s: can not get sleep pinstate\n", __func__);

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

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 1);

	if (!(ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT)) {
		if (!pdata->panel_info.dynamic_switch_pending) {
			ret = ctrl_pdata->on(pdata);
			if (ret) {
				pr_err("%s: unable to initialize the panel\n",
							__func__);
				goto error;
			}
		}
		ctrl_pdata->ctrl_state |= CTRL_STATE_PANEL_INIT;
	}

	if ((pdata->panel_info.type == MIPI_CMD_PANEL) &&
		mipi->vsync_enable && mipi->hw_vsync_mode)
		mdss_dsi_set_tear_on(ctrl_pdata);

error:
	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 0);
	pr_debug("%s-:\n", __func__);

	return ret;
}

static int mdss_dsi_blank(struct mdss_panel_data *pdata)
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
	mipi = &pdata->panel_info.mipi;

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 1);

	if (pdata->panel_info.type == MIPI_VIDEO_PANEL &&
			ctrl_pdata->off_cmds.link_state == DSI_LP_MODE) {
		mdss_dsi_sw_reset(pdata);
		mdss_dsi_host_init(pdata);
	}

	mdss_dsi_op_mode_config(DSI_CMD_MODE, pdata);

	if (pdata->panel_info.dynamic_switch_pending) {
		pr_info("%s: switching to %s mode\n", __func__,
			(pdata->panel_info.mipi.mode ? "video" : "command"));
		if (pdata->panel_info.type == MIPI_CMD_PANEL) {
			ctrl_pdata->switch_mode(pdata, DSI_VIDEO_MODE);
		} else if (pdata->panel_info.type == MIPI_VIDEO_PANEL) {
			ctrl_pdata->switch_mode(pdata, DSI_CMD_MODE);
			mdss_dsi_set_tear_off(ctrl_pdata);
		}
	}

	if ((pdata->panel_info.type == MIPI_CMD_PANEL) &&
		mipi->vsync_enable && mipi->hw_vsync_mode)
		mdss_dsi_set_tear_off(ctrl_pdata);

	if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
		if (!pdata->panel_info.dynamic_switch_pending) {
			ret = ctrl_pdata->off(pdata);
			if (ret) {
				pr_err("%s: Panel OFF failed\n", __func__);
				goto error;
			}
		}
		ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
	}

error:
	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 0);
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
	mdss_dsi_host_init(pdata);
	mdss_dsi_op_mode_config(mipi->mode, pdata);
	pr_debug("%s-:End\n", __func__);
	return ret;
}

static void __mdss_dsi_update_video_mode_total(struct mdss_panel_data *pdata,
		int new_fps)
{
	u32 hsync_period, vsync_period;
	u32 new_dsi_v_total, current_dsi_v_total;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s Invalid pdata\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	if (ctrl_pdata == NULL) {
		pr_err("%s Invalid ctrl_pdata\n", __func__);
		return;
	}

	vsync_period =
		mdss_panel_get_vtotal(&pdata->panel_info);
	hsync_period =
		mdss_panel_get_htotal(&pdata->panel_info, true);
	current_dsi_v_total =
		MIPI_INP((ctrl_pdata->ctrl_base) + 0x2C);
	new_dsi_v_total =
		((vsync_period - 1) << 16) | (hsync_period - 1);

	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2C,
			(current_dsi_v_total | 0x8000000));
	if (new_dsi_v_total & 0x8000000) {
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2C,
				new_dsi_v_total);
	} else {
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2C,
				(new_dsi_v_total | 0x8000000));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2C,
				(new_dsi_v_total & 0x7ffffff));
	}
	ctrl_pdata->panel_data.panel_info.mipi.frame_rate = new_fps;

}

static int __mdss_dsi_dfps_update_clks(struct mdss_panel_data *pdata,
		int new_fps)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	u32 dsi_ctrl;

	if (pdata == NULL) {
		pr_err("%s Invalid pdata\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	if (ctrl_pdata == NULL) {
		pr_err("%s Invalid ctrl_pdata\n", __func__);
		return -EINVAL;
	}

	rc = mdss_dsi_clk_div_config
		(&ctrl_pdata->panel_data.panel_info, new_fps);
	if (rc) {
		pr_err("%s: unable to initialize the clk dividers\n",
				__func__);
		return rc;
	}
	ctrl_pdata->pclk_rate =
		pdata->panel_info.mipi.dsi_pclk_rate;
	ctrl_pdata->byte_clk_rate =
		pdata->panel_info.clk_rate / 8;

	if (pdata->panel_info.dfps_update
			== DFPS_IMMEDIATE_CLK_UPDATE_MODE) {
		dsi_ctrl = MIPI_INP((ctrl_pdata->ctrl_base) +
				0x0004);
		pdata->panel_info.mipi.frame_rate = new_fps;
		dsi_ctrl &= ~0x2;
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0004,
				dsi_ctrl);
		mdss_dsi_controller_cfg(true, pdata);
		mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 0);
		mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 1);
		dsi_ctrl |= 0x2;
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0004,
				dsi_ctrl);
	}

	return rc;
}

static int mdss_dsi_dfps_config(struct mdss_panel_data *pdata, int new_fps)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_dsi_ctrl_pdata *sctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;

	pr_debug("%s+:\n", __func__);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	if (!ctrl_pdata->panel_data.panel_info.dynamic_fps) {
		pr_err("%s: Dynamic fps not enabled for this panel\n",
					__func__);
		return -EINVAL;
	}

	/*
	 * at split display case, DFPS registers were already programmed
	 * while programming the left ctrl(DSI0). Ignore right ctrl (DSI1)
	 * reguest.
	 */
	pinfo = &pdata->panel_info;
	if (pinfo->is_split_display) {
		if (mdss_dsi_is_right_ctrl(ctrl_pdata)) {
			pr_debug("%s DFPS already updated.\n", __func__);
			return rc;
		}
		/* left ctrl to get right ctrl */
		sctrl_pdata = mdss_dsi_get_other_ctrl(ctrl_pdata);
	}

	if (new_fps !=
		ctrl_pdata->panel_data.panel_info.mipi.frame_rate) {
		if (pdata->panel_info.dfps_update
			== DFPS_IMMEDIATE_PORCH_UPDATE_MODE) {

			__mdss_dsi_update_video_mode_total(pdata, new_fps);
			if (sctrl_pdata) {
				pr_debug("%s Updating slave ctrl DFPS\n",
						__func__);
				__mdss_dsi_update_video_mode_total(
						&sctrl_pdata->panel_data,
						new_fps);
			}

		} else {
			rc = __mdss_dsi_dfps_update_clks(pdata, new_fps);
			if (!rc && sctrl_pdata) {
				pr_debug("%s Updating slave ctrl DFPS\n",
						__func__);
				rc = __mdss_dsi_dfps_update_clks(
						&sctrl_pdata->panel_data,
						new_fps);
			}
		}
	} else {
		pr_debug("%s: Panel is already at this FPS\n", __func__);
	}

	return rc;
}

static int mdss_dsi_ctl_partial_roi(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int rc = -EINVAL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (ctrl_pdata->set_col_page_addr)
		rc = ctrl_pdata->set_col_page_addr(pdata);

	return rc;
}

static int mdss_dsi_set_stream_size(struct mdss_panel_data *pdata)
{
	u32 data, idle;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;
	struct mdss_rect *roi;
	struct panel_horizontal_idle *pidle;
	int i;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pinfo = &pdata->panel_info;

	if (!pinfo->partial_update_enabled)
		return -EINVAL;

	roi = &pinfo->roi;

	/* DSI_COMMAND_MODE_MDP_STREAM_CTRL */
	data = (((roi->w * 3) + 1) << 16) |
			(pdata->panel_info.mipi.vc << 8) | DTYPE_DCS_LWRITE;
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x60, data);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x58, data);

	/* DSI_COMMAND_MODE_MDP_STREAM_TOTAL */
	data = roi->h << 16 | roi->w;
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x64, data);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x5C, data);

	/* set idle control -- dsi clk cycle */
	idle = 0;
	pidle = ctrl_pdata->line_idle;
	for (i = 0; i < ctrl_pdata->horizontal_idle_cnt; i++) {
		if (roi->w > pidle->min && roi->w <= pidle->max) {
			idle = pidle->idle;
			pr_debug("%s: ndx=%d w=%d range=%d-%d idle=%d\n",
				__func__, ctrl_pdata->ndx, roi->w,
				pidle->min, pidle->max, pidle->idle);
			break;
		}
		pidle++;
	}

	if (idle)
		idle |= BIT(12);	/* enable */

	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x194, idle);

	return 0;
}

int mdss_dsi_register_recovery_handler(struct mdss_dsi_ctrl_pdata *ctrl,
	struct mdss_panel_recovery *recovery)
{
	mutex_lock(&ctrl->mutex);
	ctrl->recovery = recovery;
	mutex_unlock(&ctrl->mutex);
	return 0;
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

	MDSS_XLOG(event, arg, ctrl_pdata->ndx, 0x3333);

	switch (event) {
	case MDSS_EVENT_UNBLANK:
		rc = mdss_dsi_on(pdata);
		mdss_dsi_op_mode_config(pdata->panel_info.mipi.mode,
							pdata);
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
		if (ctrl_pdata->off_cmds.link_state == DSI_LP_MODE)
			rc = mdss_dsi_blank(pdata);
		ctrl_pdata->ctrl_state &= ~CTRL_STATE_MDP_ACTIVE;
		rc = mdss_dsi_cont_splash_on(pdata);
		break;
	case MDSS_EVENT_PANEL_CLK_CTRL:
		mdss_dsi_clk_req(ctrl_pdata, (int) (unsigned long) arg);
		break;
	case MDSS_EVENT_DSI_CMDLIST_KOFF:
		mdss_dsi_cmdlist_commit(ctrl_pdata, 1);
		break;
	case MDSS_EVENT_PANEL_UPDATE_FPS:
		if (arg != NULL) {
			rc = mdss_dsi_dfps_config(pdata,
					 (int) (unsigned long) arg);
			pr_debug("%s:update fps to = %d\n",
				 __func__, (int) (unsigned long) arg);
		}
		break;
	case MDSS_EVENT_CONT_SPLASH_BEGIN:
		if (ctrl_pdata->off_cmds.link_state == DSI_HS_MODE) {
			/* Panel is Enabled in Bootloader */
			rc = mdss_dsi_blank(pdata);
		}
		break;
	case MDSS_EVENT_ENABLE_PARTIAL_ROI:
		rc = mdss_dsi_ctl_partial_roi(pdata);
		break;
	case MDSS_EVENT_DSI_STREAM_SIZE:
		rc = mdss_dsi_set_stream_size(pdata);
		break;
	case MDSS_EVENT_DSI_DYNAMIC_SWITCH:
		rc = mdss_dsi_update_panel_config(ctrl_pdata,
					(int)(unsigned long) arg);
		break;
	case MDSS_EVENT_REGISTER_RECOVERY_HANDLER:
		rc = mdss_dsi_register_recovery_handler(ctrl_pdata,
			(struct mdss_panel_recovery *)arg);
		break;
	default:
		pr_debug("%s: unhandled event=%d\n", __func__, event);
		break;
	}
	pr_debug("%s-:event=%d, rc=%d\n", __func__, event, rc);
	return rc;
}

static struct device_node *mdss_dsi_pref_prim_panel(
		struct platform_device *pdev)
{
	struct device_node *dsi_pan_node = NULL;

	pr_debug("%s:%d: Select primary panel from dt\n",
					__func__, __LINE__);
	dsi_pan_node = of_parse_phandle(pdev->dev.of_node,
					"qcom,dsi-pref-prim-pan", 0);
	if (!dsi_pan_node)
		pr_err("%s:can't find panel phandle\n", __func__);

	return dsi_pan_node;
}

/**
 * mdss_dsi_find_panel_of_node(): find device node of dsi panel
 * @pdev: platform_device of the dsi ctrl node
 * @panel_cfg: string containing intf specific config data
 *
 * Function finds the panel device node using the interface
 * specific configuration data. This configuration data is
 * could be derived from the result of bootloader's GCDB
 * panel detection mechanism. If such config data doesn't
 * exist then this panel returns the default panel configured
 * in the device tree.
 *
 * returns pointer to panel node on success, NULL on error.
 */
static struct device_node *mdss_dsi_find_panel_of_node(
		struct platform_device *pdev, char *panel_cfg)
{
	int len, i;
	int ctrl_id = pdev->id - 1;
	char panel_name[MDSS_MAX_PANEL_LEN];
	char ctrl_id_stream[3] =  "0:";
	char *stream = NULL, *pan = NULL;
	struct device_node *dsi_pan_node = NULL, *mdss_node = NULL;

	len = strlen(panel_cfg);
	if (!len) {
		/* no panel cfg chg, parse dt */
		pr_debug("%s:%d: no cmd line cfg present\n",
			 __func__, __LINE__);
		goto end;
	} else {
		if (ctrl_id == 1)
			strlcpy(ctrl_id_stream, "1:", 3);

		stream = strnstr(panel_cfg, ctrl_id_stream, len);
		if (!stream) {
			pr_err("controller config is not present\n");
			goto end;
		}
		stream += 2;

		pan = strnchr(stream, strlen(stream), ':');
		if (!pan) {
			strlcpy(panel_name, stream, MDSS_MAX_PANEL_LEN);
		} else {
			for (i = 0; (stream + i) < pan; i++)
				panel_name[i] = *(stream + i);
			panel_name[i] = 0;
		}

		pr_debug("%s:%d:%s:%s\n", __func__, __LINE__,
			 panel_cfg, panel_name);

		mdss_node = of_parse_phandle(pdev->dev.of_node,
					     "qcom,mdss-mdp", 0);

		if (!mdss_node) {
			pr_err("%s: %d: mdss_node null\n",
			       __func__, __LINE__);
			return NULL;
		}
		dsi_pan_node = of_find_node_by_name(mdss_node,
						    panel_name);
		if (!dsi_pan_node) {
			pr_err("%s: invalid pan node, selecting prim panel\n",
			       __func__);
			goto end;
		}
		return dsi_pan_node;
	}
end:
	dsi_pan_node = mdss_dsi_pref_prim_panel(pdev);

	return dsi_pan_node;
}

static int mdss_dsi_ctrl_probe(struct platform_device *pdev)
{
	int rc = 0, i = 0;
	u32 index;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct device_node *dsi_pan_node = NULL;
	char panel_cfg[MDSS_MAX_PANEL_LEN];
	const char *ctrl_name;
	bool cmd_cfg_cont_splash = true;
	struct mdss_panel_cfg *pan_cfg = NULL;

	if (!mdss_is_ready()) {
		pr_err("%s: MDP not probed yet!\n", __func__);
		return -EPROBE_DEFER;
	}

	if (!pdev->dev.of_node) {
		pr_err("DSI driver only supports device tree probe\n");
		return -ENOTSUPP;
	}

	pan_cfg = mdss_panel_intf_type(MDSS_PANEL_INTF_HDMI);
	if (IS_ERR(pan_cfg)) {
		return PTR_ERR(pan_cfg);
	} else if (pan_cfg) {
		pr_debug("%s: HDMI is primary\n", __func__);
		return -ENODEV;
	}

	ctrl_pdata = platform_get_drvdata(pdev);
	if (!ctrl_pdata) {
		ctrl_pdata = devm_kzalloc(&pdev->dev,
					  sizeof(struct mdss_dsi_ctrl_pdata),
					  GFP_KERNEL);
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

	rc = of_platform_populate(pdev->dev.of_node,
				  NULL, NULL, &pdev->dev);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: failed to add child nodes, rc=%d\n",
			__func__, rc);
		goto error_no_mem;
	}

	rc = mdss_dsi_pinctrl_init(pdev);
	if (rc)
		pr_warn("%s: failed to get pin resources\n", __func__);

	/* Parse the regulator information */
	for (i = 0; i < DSI_MAX_PM; i++) {
		rc = mdss_dsi_get_dt_vreg_data(&pdev->dev,
			&ctrl_pdata->power_data[i], i);
		if (rc) {
			DEV_ERR("%s: '%s' get_dt_vreg_data failed.rc=%d\n",
				__func__, __mdss_dsi_pm_name(i), rc);
			goto error_vreg;
		}
	}

	/*
	 * Currently, the Bias vreg is controlled by wled driver.
	 * Once we have support from pmic driver, implement the
	 * bias vreg control using the existing vreg apis.
	 */
	ctrl_pdata->panel_bias_vreg = of_property_read_bool(
			pdev->dev.of_node, "qcom,dsi-panel-bias-vreg");

	/* DSI panels can be different between controllers */
	rc = mdss_dsi_get_panel_cfg(panel_cfg);
	if (!rc)
		/* dsi panel cfg not present */
		pr_warn("%s:%d:dsi specific cfg not present\n",
			__func__, __LINE__);

	/* find panel device node */
	dsi_pan_node = mdss_dsi_find_panel_of_node(pdev, panel_cfg);
	if (!dsi_pan_node) {
		pr_err("%s: can't find panel node %s\n", __func__, panel_cfg);
		goto error_pan_node;
	}

	cmd_cfg_cont_splash = mdss_panel_get_boot_cfg() ? true : false;

	rc = mdss_dsi_panel_init(dsi_pan_node, ctrl_pdata, cmd_cfg_cont_splash);
	if (rc) {
		pr_err("%s: dsi panel init failed\n", __func__);
		goto error_pan_node;
	}

	rc = dsi_panel_device_register(dsi_pan_node, ctrl_pdata);
	if (rc) {
		pr_err("%s: dsi panel dev reg failed\n", __func__);
		goto error_pan_node;
	}

	pr_debug("%s: Dsi Ctrl->%d initialized\n", __func__, index);
	return 0;

error_pan_node:
	of_node_put(dsi_pan_node);
error_vreg:
	for (; i >= 0; i--)
		mdss_dsi_put_dt_vreg_data(&pdev->dev,
			&ctrl_pdata->power_data[i]);
error_no_mem:
	devm_kfree(&pdev->dev, ctrl_pdata);

	return rc;
}

static int mdss_dsi_ctrl_remove(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);
	int i = 0;

	if (!ctrl_pdata) {
		pr_err("%s: no driver data\n", __func__);
		return -ENODEV;
	}

	for (i = DSI_MAX_PM - 1; i >= 0; i--) {
		if (msm_dss_config_vreg(&pdev->dev,
				ctrl_pdata->power_data[i].vreg_config,
				ctrl_pdata->power_data[i].num_vreg, 1) < 0)
			pr_err("%s: failed to de-init vregs for %s\n",
				__func__, __mdss_dsi_pm_name(i));
		mdss_dsi_put_dt_vreg_data(&pdev->dev,
			&ctrl_pdata->power_data[i]);
	}

	mfd = platform_get_drvdata(pdev);
	msm_dss_iounmap(&ctrl_pdata->mmss_misc_io);
	msm_dss_iounmap(&ctrl_pdata->phy_io);
	msm_dss_iounmap(&ctrl_pdata->ctrl_io);
	return 0;
}

struct device dsi_dev;

int mdss_dsi_retrieve_ctrl_resources(struct platform_device *pdev, int mode,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0;
	u32 index;

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

	rc = msm_dss_ioremap_byname(pdev, &ctrl->ctrl_io, "dsi_ctrl");
	if (rc) {
		pr_err("%s:%d unable to remap dsi ctrl resources",
			       __func__, __LINE__);
		return rc;
	}

	ctrl->ctrl_base = ctrl->ctrl_io.base;
	ctrl->reg_size = ctrl->ctrl_io.len;

	rc = msm_dss_ioremap_byname(pdev, &ctrl->phy_io, "dsi_phy");
	if (rc) {
		pr_err("%s:%d unable to remap dsi phy resources",
			       __func__, __LINE__);
		return rc;
	}

	pr_info("%s: ctrl_base=%p ctrl_size=%x phy_base=%p phy_size=%x\n",
		__func__, ctrl->ctrl_base, ctrl->reg_size, ctrl->phy_io.base,
		ctrl->phy_io.len);

	rc = msm_dss_ioremap_byname(pdev, &ctrl->mmss_misc_io,
		"mmss_misc_phys");
	if (rc) {
		pr_debug("%s:%d mmss_misc IO remap failed\n",
			__func__, __LINE__);
	}

	return 0;
}

int dsi_panel_device_register(struct device_node *pan_node,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct mipi_panel_info *mipi;
	int rc, i, len;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);
	struct device_node *dsi_ctrl_np = NULL;
	struct platform_device *ctrl_pdev = NULL;
	bool dynamic_fps;
	const char *data;

	mipi  = &(pinfo->mipi);

	pinfo->type =
		((mipi->mode == DSI_VIDEO_MODE)
			? MIPI_VIDEO_PANEL : MIPI_CMD_PANEL);

	rc = mdss_dsi_clk_div_config(pinfo, mipi->frame_rate);
	if (rc) {
		pr_err("%s: unable to initialize the clk dividers\n", __func__);
		return rc;
	}

	dsi_ctrl_np = of_parse_phandle(pan_node,
				"qcom,mdss-dsi-panel-controller", 0);
	if (!dsi_ctrl_np) {
		pr_err("%s: Dsi controller node not initialized\n", __func__);
		return -EPROBE_DEFER;
	}

	ctrl_pdev = of_find_device_by_node(dsi_ctrl_np);

	rc = mdss_dsi_regulator_init(ctrl_pdev);
	if (rc) {
		pr_err("%s: failed to init regulator, rc=%d\n",
						__func__, rc);
		return rc;
	}

	data = of_get_property(ctrl_pdev->dev.of_node,
		"qcom,platform-strength-ctrl", &len);
	if ((!data) || (len != 2)) {
		pr_err("%s:%d, Unable to read Phy Strength ctrl settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	pinfo->mipi.dsi_phy_db.strength[0] = data[0];
	pinfo->mipi.dsi_phy_db.strength[1] = data[1];

	pinfo->mipi.dsi_phy_db.reg_ldo_mode = of_property_read_bool(
		ctrl_pdev->dev.of_node, "qcom,regulator-ldo-mode");

	data = of_get_property(ctrl_pdev->dev.of_node,
		"qcom,platform-regulator-settings", &len);
	if ((!data) || (len != 7)) {
		pr_err("%s:%d, Unable to read Phy regulator settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++) {
		pinfo->mipi.dsi_phy_db.regulator[i]
			= data[i];
	}

	data = of_get_property(ctrl_pdev->dev.of_node,
		"qcom,platform-bist-ctrl", &len);
	if ((!data) || (len != 6)) {
		pr_err("%s:%d, Unable to read Phy Bist Ctrl settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++) {
		pinfo->mipi.dsi_phy_db.bistctrl[i]
			= data[i];
	}

	data = of_get_property(ctrl_pdev->dev.of_node,
		"qcom,platform-lane-config", &len);
	if ((!data) || (len != 45)) {
		pr_err("%s:%d, Unable to read Phy lane configure settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++) {
		pinfo->mipi.dsi_phy_db.lanecfg[i] =
			data[i];
	}

	rc = of_property_read_u32(ctrl_pdev->dev.of_node,
			"qcom,mmss-ulp-clamp-ctrl-offset",
			&ctrl_pdata->ulps_clamp_ctrl_off);
	if (!rc) {
		rc = of_property_read_u32(ctrl_pdev->dev.of_node,
				"qcom,mmss-phyreset-ctrl-offset",
				&ctrl_pdata->ulps_phyrst_ctrl_off);
	}
	if (rc && pinfo->ulps_feature_enabled) {
		pr_err("%s: dsi ulps clamp register settings missing\n",
				__func__);
		return -EINVAL;
	}

	ctrl_pdata->cmd_sync_wait_broadcast = of_property_read_bool(
		pan_node, "qcom,cmd-sync-wait-broadcast");

	ctrl_pdata->cmd_sync_wait_trigger = of_property_read_bool(
		pan_node, "qcom,cmd-sync-wait-trigger");

	pr_debug("%s: cmd_sync_wait_enable=%d trigger=%d\n", __func__,
				ctrl_pdata->cmd_sync_wait_broadcast,
				ctrl_pdata->cmd_sync_wait_trigger);

	dynamic_fps = of_property_read_bool(pan_node,
					  "qcom,mdss-dsi-pan-enable-dynamic-fps");
	if (dynamic_fps) {
		pinfo->dynamic_fps = true;
		data = of_get_property(pan_node,
					  "qcom,mdss-dsi-pan-fps-update", NULL);
		if (data) {
			if (!strcmp(data, "dfps_suspend_resume_mode")) {
				pinfo->dfps_update =
						DFPS_SUSPEND_RESUME_MODE;
				pr_debug("%s: dfps mode: suspend/resume\n",
								__func__);
			} else if (!strcmp(data,
					    "dfps_immediate_clk_mode")) {
				pinfo->dfps_update =
						DFPS_IMMEDIATE_CLK_UPDATE_MODE;
				pr_debug("%s: dfps mode: Immediate clk\n",
								__func__);
			} else if (!strcmp(data,
					    "dfps_immediate_porch_mode")) {
				pinfo->dfps_update =
					DFPS_IMMEDIATE_PORCH_UPDATE_MODE;
				pr_debug("%s: dfps mode: Immediate porch\n",
								__func__);
			} else {
				pr_debug("%s: dfps to default mode\n",
								__func__);
				pinfo->dfps_update =
						DFPS_SUSPEND_RESUME_MODE;
				pr_debug("%s: dfps mode: suspend/resume\n",
								__func__);
			}
		} else {
			pr_debug("%s: dfps update mode not configured\n",
								__func__);
				pinfo->dynamic_fps = false;
				pr_debug("%s: dynamic FPS disabled\n",
								__func__);
		}
		pinfo->new_fps = pinfo->mipi.frame_rate;
	}

	pinfo->panel_max_fps = mdss_panel_get_framerate(pinfo);
	pinfo->panel_max_vtotal = mdss_panel_get_vtotal(pinfo);
	ctrl_pdata->disp_en_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-enable-gpio", 0);

	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio))
		pr_err("%s:%d, Disp_en gpio not specified\n",
						__func__, __LINE__);

	ctrl_pdata->bklt_en_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-bklight-en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->bklt_en_gpio))
		pr_info("%s: bklt_en gpio not specified\n", __func__);

	ctrl_pdata->rst_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
			 "qcom,platform-reset-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->rst_gpio))
		pr_err("%s:%d, reset gpio not specified\n",
						__func__, __LINE__);

	if (pinfo->mode_gpio_state != MODE_GPIO_NOT_VALID) {

		ctrl_pdata->mode_gpio = of_get_named_gpio(
					ctrl_pdev->dev.of_node,
					"qcom,platform-mode-gpio", 0);
		if (!gpio_is_valid(ctrl_pdata->mode_gpio))
			pr_info("%s:%d, mode gpio not specified\n",
							__func__, __LINE__);
	} else {
		ctrl_pdata->mode_gpio = -EINVAL;
	}

	if (mdss_dsi_clk_init(ctrl_pdev, ctrl_pdata)) {
		pr_err("%s: unable to initialize Dsi ctrl clks\n", __func__);
		return -EPERM;
	}

	if (mdss_dsi_retrieve_ctrl_resources(ctrl_pdev,
					     pinfo->pdest,
					     ctrl_pdata)) {
		pr_err("%s: unable to get Dsi controller res\n", __func__);
		return -EPERM;
	}

	ctrl_pdata->panel_data.event_handler = mdss_dsi_event_handler;

	if (ctrl_pdata->status_mode == ESD_REG)
		ctrl_pdata->check_status = mdss_dsi_reg_status_check;
	else if (ctrl_pdata->status_mode == ESD_BTA)
		ctrl_pdata->check_status = mdss_dsi_bta_status_check;

	if (ctrl_pdata->status_mode == ESD_MAX) {
		pr_err("%s: Using default BTA for ESD check\n", __func__);
		ctrl_pdata->check_status = mdss_dsi_bta_status_check;
	}
	if (ctrl_pdata->bklt_ctrl == BL_PWM)
		mdss_dsi_panel_pwm_cfg(ctrl_pdata);

	mdss_dsi_ctrl_init(&ctrl_pdev->dev, ctrl_pdata);
	/*
	 * register in mdp driver
	 */

	ctrl_pdata->pclk_rate = mipi->dsi_pclk_rate;
	ctrl_pdata->byte_clk_rate = pinfo->clk_rate / 8;
	pr_debug("%s: pclk=%d, bclk=%d\n", __func__,
			ctrl_pdata->pclk_rate, ctrl_pdata->byte_clk_rate);

	ctrl_pdata->ctrl_state = CTRL_STATE_UNKNOWN;

	if (pinfo->cont_splash_enabled) {
		pinfo->panel_power_on = 1;
		rc = mdss_dsi_panel_power_on(&(ctrl_pdata->panel_data), 1);
		if (rc) {
			pr_err("%s: Panel power on failed\n", __func__);
			return rc;
		}

		mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 1);
		ctrl_pdata->ctrl_state |=
			(CTRL_STATE_PANEL_INIT | CTRL_STATE_MDP_ACTIVE);
		ctrl_pdata->ctrl_rev = MIPI_INP(ctrl_pdata->ctrl_base);
	} else {
		pinfo->panel_power_on = 0;
	}

	rc = mdss_register_panel(ctrl_pdev, &(ctrl_pdata->panel_data));
	if (rc) {
		pr_err("%s: unable to register MIPI DSI panel\n", __func__);
		return rc;
	}

	if (pinfo->pdest == DISPLAY_1) {
		mdss_debug_register_base("dsi0",
			ctrl_pdata->ctrl_base, ctrl_pdata->reg_size);
		ctrl_pdata->ndx = 0;
	} else {
		mdss_debug_register_base("dsi1",
			ctrl_pdata->ctrl_base, ctrl_pdata->reg_size);
		ctrl_pdata->ndx = 1;
	}

	pr_debug("%s: Panel data initialized\n", __func__);
	return 0;
}

static const struct of_device_id mdss_dsi_ctrl_dt_match[] = {
	{.compatible = "qcom,mdss-dsi-ctrl"},
	{}
};
MODULE_DEVICE_TABLE(of, mdss_dsi_ctrl_dt_match);

static struct platform_driver mdss_dsi_ctrl_driver = {
	.probe = mdss_dsi_ctrl_probe,
	.remove = mdss_dsi_ctrl_remove,
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
	platform_driver_unregister(&mdss_dsi_ctrl_driver);
}
module_exit(mdss_dsi_driver_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DSI controller driver");
MODULE_AUTHOR("Chandan Uddaraju <chandanu@codeaurora.org>");
