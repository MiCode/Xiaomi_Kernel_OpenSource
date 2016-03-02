/* Copyright (c) 2012-2016 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/qpnp/pwm.h>
#include <linux/clk.h>
#include <linux/spinlock_types.h>
#include <linux/kthread.h>

#include "mdss.h"
#include "mdss_edp.h"
#include "mdss_dp_util.h"
#include "mdss_debug.h"

#define RGB_COMPONENTS		3
#define VDDA_MIN_UV			1800000	/* uV units */
#define VDDA_MAX_UV			1800000	/* uV units */
#define VDDA_UA_ON_LOAD		100000	/* uA units */
#define VDDA_UA_OFF_LOAD	100		/* uA units */



static void mdss_dp_put_dt_clk_data(struct device *dev,
	struct dss_module_power *module_power)
{
	if (!module_power) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (module_power->clk_config) {
		devm_kfree(dev, module_power->clk_config);
		module_power->clk_config = NULL;
	}
	module_power->num_clk = 0;
} /* mdss_dp_put_dt_clk_data */

static int mdss_dp_is_clk_prefix(const char *clk_prefix, const char *clk_name)
{
	return !strncmp(clk_name, clk_prefix, strlen(clk_prefix));
}

static int mdss_dp_init_clk_power_data(struct device *dev,
		struct mdss_dp_drv_pdata *pdata)
{
	int num_clk = 0, i = 0, rc = 0;
	int core_clk_count = 0, ctrl_clk_count = 0;
	const char *core_clk = "core";
	const char *ctrl_clk = "ctrl";
	struct dss_module_power *core_power_data = NULL;
	struct dss_module_power *ctrl_power_data = NULL;
	const char *clk_name;

	num_clk = of_property_count_strings(dev->of_node,
			"clock-names");
	if (num_clk <= 0) {
		pr_err("no clocks are defined\n");
		rc = -EINVAL;
		goto exit;
	}

	core_power_data = &pdata->power_data[DP_CORE_PM];
	ctrl_power_data = &pdata->power_data[DP_CTRL_PM];

	for (i = 0; i < num_clk; i++) {
		of_property_read_string_index(dev->of_node, "clock-names",
				i, &clk_name);

		if (mdss_dp_is_clk_prefix(core_clk, clk_name))
			core_clk_count++;
		if (mdss_dp_is_clk_prefix(ctrl_clk, clk_name))
			ctrl_clk_count++;
	}

	/* Initialize the CORE power module */
	if (core_clk_count <= 0) {
		pr_err("no core clocks are defined\n");
		rc = -EINVAL;
		goto exit;
	}

	core_power_data->num_clk = core_clk_count;
	core_power_data->clk_config = devm_kzalloc(dev, sizeof(struct dss_clk) *
			core_power_data->num_clk, GFP_KERNEL);
	if (!core_power_data->clk_config) {
		rc = -EINVAL;
		goto exit;
	}

	/* Initialize the CTRL power module */
	if (ctrl_clk_count <= 0) {
		pr_err("no ctrl clocks are defined\n");
		rc = -EINVAL;
		goto ctrl_clock_error;
	}

	ctrl_power_data->num_clk = ctrl_clk_count;
	ctrl_power_data->clk_config = devm_kzalloc(dev, sizeof(struct dss_clk) *
			ctrl_power_data->num_clk, GFP_KERNEL);
	if (!ctrl_power_data->clk_config) {
		ctrl_power_data->num_clk = 0;
		rc = -EINVAL;
		goto ctrl_clock_error;
	}

	return rc;

ctrl_clock_error:
	mdss_dp_put_dt_clk_data(dev, core_power_data);
exit:
	return rc;
}

static int mdss_dp_get_dt_clk_data(struct device *dev,
		struct mdss_dp_drv_pdata *pdata)
{
	int rc = 0, i = 0;
	const char *clk_name;
	int num_clk = 0;
	int core_clk_index = 0, ctrl_clk_index = 0;
	int core_clk_count = 0, ctrl_clk_count = 0;
	const char *core_clk = "core";
	const char *ctrl_clk = "ctrl";
	struct dss_module_power *core_power_data = NULL;
	struct dss_module_power *ctrl_power_data = NULL;

	if (!dev || !pdata) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto exit;
	}

	rc =  mdss_dp_init_clk_power_data(dev, pdata);
	if (rc) {
		pr_err("failed to initialize power data\n");
		rc = -EINVAL;
		goto exit;
	}

	core_power_data = &pdata->power_data[DP_CORE_PM];
	core_clk_count = core_power_data->num_clk;
	ctrl_power_data = &pdata->power_data[DP_CTRL_PM];
	ctrl_clk_count = ctrl_power_data->num_clk;

	num_clk = core_clk_count + ctrl_clk_count;

	for (i = 0; i < num_clk; i++) {
		of_property_read_string_index(dev->of_node, "clock-names",
				i, &clk_name);

		if (mdss_dp_is_clk_prefix(core_clk, clk_name)
				&& core_clk_index < core_clk_count) {
			struct dss_clk *clk =
				&core_power_data->clk_config[core_clk_index];
			strlcpy(clk->clk_name, clk_name, sizeof(clk->clk_name));
			clk->type = DSS_CLK_AHB;
			core_clk_index++;
		} else if (mdss_dp_is_clk_prefix(ctrl_clk, clk_name)
				&& ctrl_clk_index < ctrl_clk_count) {
			struct dss_clk *clk =
				&ctrl_power_data->clk_config[ctrl_clk_index];
			strlcpy(clk->clk_name, clk_name, sizeof(clk->clk_name));
			ctrl_clk_index++;
			if (!strcmp(clk_name, "ctrl_link_clk"))
				clk->type = DSS_CLK_PCLK;
			else if (!strcmp(clk_name, "ctrl_pixel_clk"))
				clk->type = DSS_CLK_PCLK;
			else
				clk->type = DSS_CLK_AHB;
		}
	}

	pr_debug("Display-port clock parsing successful\n");

exit:
	return rc;
} /* mdss_dp_get_dt_clk_data */

/*
 * This clock control function supports enabling/disabling
 * of core and ctrl power module clocks
 */
static int mdss_dp_clk_ctrl(struct mdss_dp_drv_pdata *dp_drv,
				int pm_type, bool enable)
{
	int ret = 0;

	if ((pm_type != DP_CORE_PM)
			&& (pm_type != DP_CTRL_PM)) {
		pr_err("unsupported power module: %s\n",
				__mdss_dp_pm_name(pm_type));
		return -EINVAL;
	}

	if (enable) {
		if ((pm_type == DP_CORE_PM)
			&& (dp_drv->core_clks_on)) {
			pr_debug("core clks already enabled\n");
			return 0;
		}

		if ((pm_type == DP_CTRL_PM)
			&& (dp_drv->link_clks_on)) {
			pr_debug("links clks already enabled\n");
			return 0;
		}

		if ((pm_type == DP_CTRL_PM)
			&& (!dp_drv->core_clks_on)) {
			pr_debug("Need to enable core clks before link clks\n");

			ret = msm_dss_enable_clk(
				dp_drv->power_data[DP_CORE_PM].clk_config,
				dp_drv->power_data[DP_CORE_PM].num_clk, 1);
			if (ret) {
				pr_err("failed to enable clks for %s\n",
					__mdss_dp_pm_name(pm_type));
				goto error;
			} else {
				dp_drv->core_clks_on = true;
			}
		}

		ret = msm_dss_enable_clk(
			dp_drv->power_data[pm_type].clk_config,
			dp_drv->power_data[pm_type].num_clk, 1);
		if (ret) {
			pr_err("failed to enable clks for %s\n",
				 __mdss_dp_pm_name(pm_type));
				goto error;
		}
	} else {
		ret = msm_dss_enable_clk(
			dp_drv->power_data[pm_type].clk_config,
			dp_drv->power_data[pm_type].num_clk, 0);
		if (ret) {
			pr_err("failed to disable clks for %s\n",
				__mdss_dp_pm_name(pm_type));
				goto error;
		}
	}

	if (pm_type == DP_CORE_PM)
		dp_drv->core_clks_on = enable;
	else
		dp_drv->link_clks_on = enable;

error:
	return ret;
}

static int mdss_dp_regulator_ctrl(struct mdss_dp_drv_pdata *dp_drv,
					bool enable)
{
	int i, ret = 0;

	if (dp_drv->core_power == enable) {
		pr_debug("regulators already %s\n",
			enable ? "enabled" : "disabled");
		return 0;
	}

	if (enable) {
		for (i = DP_CORE_PM; i < DP_MAX_PM; i++) {
			ret = msm_dss_enable_vreg(
				dp_drv->power_data[i].vreg_config,
				dp_drv->power_data[i].num_vreg, 1);
			if (ret) {
				pr_err("failed to enable vregs for %s\n",
					__mdss_dp_pm_name(i));
				goto error;
			}
		}
	} else {
		for (i = DP_CORE_PM; i < DP_MAX_PM; i++) {
			ret = msm_dss_enable_vreg(
				dp_drv->power_data[i].vreg_config,
				dp_drv->power_data[i].num_vreg, 1);
			if (ret) {
				pr_err("failed to disable vregs for %s\n",
					__mdss_dp_pm_name(i));
				goto error;
			}
		}
	}

	dp_drv->core_power = enable;

error:
	return ret;
}

static void mdss_dp_put_dt_vreg_data(struct device *dev,
	struct dss_module_power *module_power)
{
	if (!module_power) {
		DEV_ERR("invalid input\n");
		return;
	}

	if (module_power->vreg_config) {
		devm_kfree(dev, module_power->vreg_config);
		module_power->vreg_config = NULL;
	}
	module_power->num_vreg = 0;
} /* mdss_dp_put_dt_vreg_data */

static int mdss_dp_get_dt_vreg_data(struct device *dev,
	struct device_node *of_node, struct dss_module_power *mp,
	enum dp_pm_type module)
{
	int i = 0, rc = 0;
	u32 tmp = 0;
	struct device_node *supply_node = NULL;
	const char *pm_supply_name = NULL;
	struct device_node *supply_root_node = NULL;

	if (!dev || !mp) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		return rc;
	}

	mp->num_vreg = 0;
	pm_supply_name = __mdss_dp_pm_supply_node_name(module);
	supply_root_node = of_get_child_by_name(of_node, pm_supply_name);
	if (!supply_root_node) {
		pr_err("no supply entry present: %s\n", pm_supply_name);
		goto novreg;
	}

	mp->num_vreg =
		of_get_available_child_count(supply_root_node);

	if (mp->num_vreg == 0) {
		pr_debug("no vreg\n");
		goto novreg;
	} else {
		pr_debug("vreg found. count=%d\n", mp->num_vreg);
	}

	mp->vreg_config = devm_kzalloc(dev, sizeof(struct dss_vreg) *
		mp->num_vreg, GFP_KERNEL);
	if (!mp->vreg_config) {
		rc = -ENOMEM;
		goto error;
	}

	for_each_child_of_node(supply_root_node, supply_node) {
		const char *st = NULL;
		/* vreg-name */
		rc = of_property_read_string(supply_node,
			"qcom,supply-name", &st);
		if (rc) {
			pr_err("error reading name. rc=%d\n",
				 rc);
			goto error;
		}
		snprintf(mp->vreg_config[i].vreg_name,
			ARRAY_SIZE((mp->vreg_config[i].vreg_name)), "%s", st);
		/* vreg-min-voltage */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-min-voltage", &tmp);
		if (rc) {
			pr_err("error reading min volt. rc=%d\n",
				rc);
			goto error;
		}
		mp->vreg_config[i].min_voltage = tmp;

		/* vreg-max-voltage */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-max-voltage", &tmp);
		if (rc) {
			pr_err("error reading max volt. rc=%d\n",
				rc);
			goto error;
		}
		mp->vreg_config[i].max_voltage = tmp;

		/* enable-load */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-enable-load", &tmp);
		if (rc) {
			pr_err("error reading enable load. rc=%d\n",
				rc);
			goto error;
		}
		mp->vreg_config[i].enable_load = tmp;

		/* disable-load */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-disable-load", &tmp);
		if (rc) {
			pr_err("error reading disable load. rc=%d\n",
				rc);
			goto error;
		}
		mp->vreg_config[i].disable_load = tmp;

		pr_debug("%s min=%d, max=%d, enable=%d, disable=%d\n",
			mp->vreg_config[i].vreg_name,
			mp->vreg_config[i].min_voltage,
			mp->vreg_config[i].max_voltage,
			mp->vreg_config[i].enable_load,
			mp->vreg_config[i].disable_load
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
} /* mdss_dp_get_dt_vreg_data */

static int mdss_dp_regulator_init(struct platform_device *pdev,
			struct mdss_dp_drv_pdata *dp_drv)
{
	int rc = 0, i = 0, j = 0;

	if (!pdev || !dp_drv) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	for (i = DP_CORE_PM; !rc && (i < DP_MAX_PM); i++) {
		rc = msm_dss_config_vreg(&pdev->dev,
			dp_drv->power_data[i].vreg_config,
			dp_drv->power_data[i].num_vreg, 1);
		if (rc) {
			pr_err("failed to init vregs for %s\n",
				__mdss_dp_pm_name(i));
			for (j = i-1; j >= DP_CORE_PM; j--) {
				msm_dss_config_vreg(&pdev->dev,
				dp_drv->power_data[j].vreg_config,
				dp_drv->power_data[j].num_vreg, 0);
			}
		}
	}

	return rc;
}

void mdss_dp_phy_initialize(struct mdss_dp_drv_pdata *dp)
{
	/*
	 * To siwtch the usb3_phy to operate in DP mode, the phy and PLL
	 * should have the reset lines asserted
	 */
	mdss_dp_assert_phy_reset(&dp->ctrl_io, true);
	/* Delay to make sure the assert is propagated */
	udelay(2000);
	mdss_dp_switch_usb3_phy_to_dp_mode(&dp->tcsr_reg_io);
	wmb(); /* ensure that the register write is successful */
	mdss_dp_assert_phy_reset(&dp->ctrl_io, false);
}

void mdss_dp_config_ctrl(struct mdss_dp_drv_pdata *dp)
{
	struct dpcd_cap *cap;
	struct display_timing_desc *timing;
	u32 data = 0;

	timing = &dp->edid.timing[0];

	cap = &dp->dpcd;

	data = dp->lane_cnt - 1;
	data <<= 4;

	if (cap->enhanced_frame)
		data |= 0x40;

	if (dp->edid.color_depth == 8) {
		/* 0 == 6 bits, 1 == 8 bits */
		data |= 0x100;	/* bit 8 */
	}

	if (!timing->interlaced)	/* progressive */
		data |= 0x04;

	data |= 0x03;	/* sycn clock & static Mvid */

	mdss_dp_configuration_ctrl(&dp->ctrl_io, data);
}

int mdss_dp_wait4train(struct mdss_dp_drv_pdata *dp_drv)
{
	int ret = 0;

	if (dp_drv->cont_splash)
		return ret;

	ret = wait_for_completion_timeout(&dp_drv->video_comp, 30);
	if (ret <= 0) {
		pr_err("Link Train timedout\n");
		ret = -EINVAL;
	} else {
		ret = 0;
	}

	pr_debug("End--\n");

	return ret;
}

#define DEFAULT_VIDEO_RESOLUTION HDMI_VFRMT_640x480p60_4_3

static int dp_init_panel_info(struct mdss_dp_drv_pdata *dp_drv)
{
	struct mdss_panel_info *pinfo;
	struct msm_hdmi_mode_timing_info timing = {0};
	u32 ret;

	if (!dp_drv) {
		DEV_ERR("invalid input\n");
		return -EINVAL;
	}

	dp_drv->ds_data.ds_registered = false;
	ret = hdmi_get_supported_mode(&timing, &dp_drv->ds_data,
		DEFAULT_VIDEO_RESOLUTION);
	pinfo = &dp_drv->panel_data.panel_info;

	if (ret || !timing.supported || !pinfo) {
		DEV_ERR("%s: invalid timing data\n", __func__);
		return -EINVAL;
	}

	pinfo->xres = timing.active_h;
	pinfo->yres = timing.active_v;
	pinfo->clk_rate = timing.pixel_freq * 1000;

	pinfo->lcdc.h_back_porch = timing.back_porch_h;
	pinfo->lcdc.h_front_porch = timing.front_porch_h;
	pinfo->lcdc.h_pulse_width = timing.pulse_width_h;
	pinfo->lcdc.v_back_porch = timing.back_porch_v;
	pinfo->lcdc.v_front_porch = timing.front_porch_v;
	pinfo->lcdc.v_pulse_width = timing.pulse_width_v;

	pinfo->type = EDP_PANEL;
	pinfo->pdest = DISPLAY_4;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 1;

	pinfo->lcdc.border_clr = 0; /* blk */
	pinfo->lcdc.underflow_clr = 0xff; /* blue */
	pinfo->lcdc.hsync_skew = 0;

	return 0;
} /* dp_init_panel_info */


int mdss_dp_on(struct mdss_panel_data *pdata)
{
	struct mdss_dp_drv_pdata *dp_drv = NULL;
	int ret = 0;

	if (!pdata) {
		pr_err("Invalid input data\n");
		return -EINVAL;
	}

	dp_drv = container_of(pdata, struct mdss_dp_drv_pdata,
			panel_data);

	pr_debug("++ cont_splash=%d\n", dp_drv->cont_splash);

	if (!dp_drv->cont_splash) { /* vote for clocks */
		ret = mdss_dp_clk_ctrl(dp_drv, DP_CORE_PM, true);
		if (ret) {
			pr_err("Unabled to start core clocks\n");
			return ret;
		}
		mdss_dp_phy_reset(&dp_drv->ctrl_io);
		mdss_dp_aux_reset(&dp_drv->ctrl_io);
		mdss_dp_mainlink_reset(&dp_drv->ctrl_io);
		mdss_dp_aux_ctrl(&dp_drv->ctrl_io, true);
		mdss_dp_hpd_configure(&dp_drv->ctrl_io, true);

		mdss_dp_phy_aux_setup(&dp_drv->phy_io);

		mdss_dp_irq_enable(dp_drv);
		pr_debug("irq enabled\n");
		mdss_dp_dpcd_cap_read(dp_drv);
		ret = mdss_dp_clk_ctrl(dp_drv, DP_CTRL_PM, true);
		if (ret) {
			mdss_dp_clk_ctrl(dp_drv, DP_CORE_PM, false);
			pr_err("Unabled to start link clocks\n");
			return ret;
		}

		mdss_dp_mainlink_reset(&dp_drv->ctrl_io);

		reinit_completion(&dp_drv->idle_comp);
		mdss_dp_fill_link_cfg(dp_drv);
		mdss_dp_mainlink_ctrl(&dp_drv->ctrl_io, true);
		mdss_dp_config_ctrl(dp_drv);
		mdss_dp_sw_mvid_nvid(&dp_drv->ctrl_io);
		mdss_dp_timing_cfg(&dp_drv->ctrl_io,
				&dp_drv->panel_data.panel_info);
	} else {
		mdss_dp_aux_ctrl(&dp_drv->ctrl_io, true);
	}

	pr_debug("call link_training\n");
	mdss_dp_link_train(dp_drv);

	mdss_dp_wait4train(dp_drv);

	dp_drv->cont_splash = 0;

	if (mdss_dp_mainlink_ready(dp_drv, BIT(0)))
		pr_debug("mainlink ready\n");

	pr_debug("End-\n");
	return ret;
}

int mdss_dp_off(struct mdss_panel_data *pdata)
{
	struct mdss_dp_drv_pdata *dp_drv = NULL;
	int ret = 0;

	dp_drv = container_of(pdata, struct mdss_dp_drv_pdata,
				panel_data);
	if (!dp_drv) {
		pr_err("Invalid input data\n");
		return -EINVAL;
	}
	pr_debug("Entered++, cont_splash=%d\n", dp_drv->cont_splash);

	/* wait until link training is completed */
	mutex_lock(&dp_drv->train_mutex);

	reinit_completion(&dp_drv->idle_comp);
	mdss_dp_state_ctrl(&dp_drv->ctrl_io, ST_PUSH_IDLE);

	ret = wait_for_completion_timeout(&dp_drv->idle_comp,
						msecs_to_jiffies(100));
	if (ret == 0)
		pr_err("idle pattern timedout\n");

	mdss_dp_state_ctrl(&dp_drv->ctrl_io, 0);

	mdss_dp_irq_disable(dp_drv);

	mdss_dp_mainlink_reset(&dp_drv->ctrl_io);
	mdss_dp_mainlink_ctrl(&dp_drv->ctrl_io, false);

	mdss_dp_aux_ctrl(&dp_drv->ctrl_io, false);
	mdss_dp_clk_ctrl(dp_drv, DP_CTRL_PM, false);
	mdss_dp_clk_ctrl(dp_drv, DP_CORE_PM, false);

	mdss_dp_regulator_ctrl(dp_drv, false);

	pr_debug("End--: state_ctrl=%x\n",
				dp_read(dp_drv->base + DP_STATE_CTRL));

	mutex_unlock(&dp_drv->train_mutex);
	return 0;
}

static int mdss_dp_host_init(struct mdss_panel_data *pdata)
{
	struct mdss_dp_drv_pdata *dp_drv = NULL;
	int ret = 0;

	if (!pdata) {
		pr_err("Invalid input data\n");
		return -EINVAL;
	}

	dp_drv = container_of(pdata, struct mdss_dp_drv_pdata,
			panel_data);

	ret = mdss_dp_regulator_ctrl(dp_drv, true);
	if (ret) {
		pr_err("failed to enable regulators\n");
		goto vreg_error;
	}

	ret = mdss_dp_clk_ctrl(dp_drv, DP_CORE_PM, true);
	if (ret) {
		pr_err("Unabled to start core clocks\n");
		goto clk_error;
	}

	mdss_dp_aux_init(dp_drv);

	mdss_dp_phy_reset(&dp_drv->ctrl_io);
	mdss_dp_aux_reset(&dp_drv->ctrl_io);
	mdss_dp_phy_initialize(dp_drv);
	mdss_dp_aux_ctrl(&dp_drv->ctrl_io, true);

	return ret;

clk_error:
	mdss_dp_regulator_ctrl(dp_drv, false);
vreg_error:
	return ret;
}

static int mdss_dp_event_handler(struct mdss_panel_data *pdata,
				  int event, void *arg)
{
	int rc = 0;

	pr_debug("event=%d\n", event);
	switch (event) {
	case MDSS_EVENT_UNBLANK:
		rc = mdss_dp_on(pdata);
		break;
	case MDSS_EVENT_PANEL_OFF:
		rc = mdss_dp_off(pdata);
		break;
	}
	return rc;
}

static int mdss_dp_remove(struct platform_device *pdev)
{
	struct mdss_dp_drv_pdata *dp_drv = NULL;

	dp_drv = platform_get_drvdata(pdev);

	iounmap(dp_drv->ctrl_io.base);
	dp_drv->ctrl_io.base = NULL;
	iounmap(dp_drv->phy_io.base);
	dp_drv->phy_io.base = NULL;

	return 0;
}

static int mdss_dp_device_register(struct mdss_dp_drv_pdata *dp_drv)
{
	int ret;

	ret = dp_init_panel_info(dp_drv);
	if (ret) {
		DEV_ERR("%s: dp_init_panel_info failed\n", __func__);
		return ret;
	}

	dp_drv->panel_data.event_handler = mdss_dp_event_handler;

	dp_drv->panel_data.panel_info.cont_splash_enabled =
					dp_drv->cont_splash;

	ret = mdss_register_panel(dp_drv->pdev, &dp_drv->panel_data);
	if (ret) {
		dev_err(&(dp_drv->pdev->dev), "unable to register dp\n");
		return ret;
	}

	pr_info("dp initialized\n");

	return 0;
}

/*
 * Retrieve dp Resources
 */
static int mdss_retrieve_dp_ctrl_resources(struct platform_device *pdev,
			struct mdss_dp_drv_pdata *dp_drv)
{
	int rc = 0;
	u32 index;

	rc = of_property_read_u32(pdev->dev.of_node, "cell-index", &index);
	if (rc) {
		dev_err(&pdev->dev,
			"Cell-index not specified, rc=%d\n",
						rc);
		return rc;
	}

	rc = msm_dss_ioremap_byname(pdev, &dp_drv->ctrl_io, "dp_ctrl");
	if (rc) {
		pr_err("%d unable to remap dp ctrl resources\n",
				__LINE__);
		return rc;
	}
	dp_drv->base = dp_drv->ctrl_io.base;
	dp_drv->base_size = dp_drv->ctrl_io.len;

	rc = msm_dss_ioremap_byname(pdev, &dp_drv->phy_io, "dp_phy");
	if (rc) {
		pr_err("%d unable to remap dp PHY resources\n",
				__LINE__);
		return rc;
	}

	rc = msm_dss_ioremap_byname(pdev, &dp_drv->tcsr_reg_io,
					"tcsr_regs");
	if (rc) {
		pr_err("%d unable to remap dp tcsr_reg resources\n",
			       __LINE__);
		return rc;
	}

	pr_debug("DP Driver base=%p size=%x\n",
		dp_drv->base, dp_drv->base_size);

	mdss_debug_register_base("dp",
			dp_drv->base, dp_drv->base_size, NULL);

	return 0;
}

static void mdss_dp_video_ready(struct mdss_dp_drv_pdata *dp)
{
	pr_debug("dp_video_ready\n");
	complete(&dp->video_comp);
}

static void mdss_dp_idle_patterns_sent(struct mdss_dp_drv_pdata *dp)
{
	pr_debug("idle_patterns_sent\n");
	complete(&dp->idle_comp);
}

static void mdss_dp_do_link_train(struct mdss_dp_drv_pdata *dp)
{
	if (dp->cont_splash)
		return;

	mdss_dp_link_train(dp);
}

static void mdss_dp_event_work(struct work_struct *work)
{
	struct mdss_dp_drv_pdata *dp = NULL;
	struct delayed_work *dw = to_delayed_work(work);
	unsigned long flag;
	u32 todo = 0;

	if (!dw) {
		pr_err("invalid work structure\n");
		return;
	}

	dp = container_of(dw, struct mdss_dp_drv_pdata, dwork);

	spin_lock_irqsave(&dp->event_lock, flag);
	todo = dp->current_event;
	dp->current_event = 0;
	spin_unlock_irqrestore(&dp->event_lock, flag);

	pr_debug("todo=%x\n", todo);

	switch (todo) {
	case (EV_EDID_READ):
		mdss_dp_edid_read(dp, 0);
		break;
	case (EV_DPCD_CAP_READ):
		mdss_dp_dpcd_cap_read(dp);
		break;
	case (EV_DPCD_STATUS_READ):
		mdss_dp_dpcd_status_read(dp);
		break;
	case (EV_LINK_TRAIN):
		mdss_dp_do_link_train(dp);
		break;
	case (EV_VIDEO_READY):
		mdss_dp_video_ready(dp);
		break;
	case (EV_IDLE_PATTERNS_SENT):
		mdss_dp_idle_patterns_sent(dp);
		break;
	default:
		pr_err("Unknown event:%d\n", todo);
	}
}

static void dp_send_events(struct mdss_dp_drv_pdata *dp, u32 events)
{
	spin_lock(&dp->event_lock);
	dp->current_event = events;
	queue_delayed_work(dp->workq,
				&dp->dwork, HZ);
	spin_unlock(&dp->event_lock);
}

irqreturn_t dp_isr(int irq, void *ptr)
{
	struct mdss_dp_drv_pdata *dp = (struct mdss_dp_drv_pdata *)ptr;
	unsigned char *base = dp->base;
	u32 isr1, isr2, mask1, mask2;
	u32 ack;

	spin_lock(&dp->lock);
	isr1 = dp_read(base + DP_INTR_STATUS);
	isr2 = dp_read(base + DP_INTR_STATUS2);

	mask1 = isr1 & dp->mask1;
	mask2 = isr2 & dp->mask2;

	isr1 &= ~mask1;	/* remove masks bit */
	isr2 &= ~mask2;

	pr_debug("isr=%x mask=%x isr2=%x mask2=%x\n",
			isr1, mask1, isr2, mask2);

	ack = isr1 & EDP_INTR_STATUS1;
	ack <<= 1;	/* ack bits */
	ack |= mask1;
	dp_write(base + DP_INTR_STATUS, ack);

	ack = isr2 & EDP_INTR_STATUS2;
	ack <<= 1;	/* ack bits */
	ack |= mask2;
	dp_write(base + DP_INTR_STATUS2, ack);
	spin_unlock(&dp->lock);

	if (isr1 & EDP_INTR_HPD) {
		isr1 &= ~EDP_INTR_HPD;	/* clear */
		mdss_dp_host_init(&dp->panel_data);
		dp_send_events(dp, EV_LINK_TRAIN);
	}

	if (isr2 & EDP_INTR_READY_FOR_VIDEO)
		dp_send_events(dp, EV_VIDEO_READY);

	if (isr2 & EDP_INTR_IDLE_PATTERNs_SENT)
		dp_send_events(dp, EV_IDLE_PATTERNS_SENT);

	if (isr1 && dp->aux_cmd_busy) {
		/* clear DP_AUX_TRANS_CTRL */
		dp_write(base + DP_AUX_TRANS_CTRL, 0);
		/* read DP_INTERRUPT_TRANS_NUM */
		dp->aux_trans_num =
			dp_read(base + DP_INTERRUPT_TRANS_NUM);

		if (dp->aux_cmd_i2c)
			dp_aux_i2c_handler(dp, isr1);
		else
			dp_aux_native_handler(dp, isr1);
	}

	return IRQ_HANDLED;
}

static int mdss_dp_event_setup(struct mdss_dp_drv_pdata *dp)
{

	spin_lock_init(&dp->event_lock);
	dp->workq = create_workqueue("mdss_dp_hpd");
	if (!dp->workq) {
		pr_err("%s: Error creating workqueue\n", __func__);
		return -EPERM;
	}

	INIT_DELAYED_WORK(&dp->dwork, mdss_dp_event_work);
	return 0;
}

static int mdss_dp_probe(struct platform_device *pdev)
{
	int ret, i;
	struct mdss_dp_drv_pdata *dp_drv;
	struct mdss_panel_cfg *pan_cfg = NULL;
	struct mdss_util_intf *util;

	util = mdss_get_util_intf();
	if (!util) {
		pr_err("Failed to get mdss utility functions\n");
		return -ENODEV;
	}

	if (!util->mdp_probe_done) {
		pr_err("MDP not probed yet!\n");
		return -EPROBE_DEFER;
	}

	if (!pdev || !pdev->dev.of_node) {
		pr_err("pdev not found for DP controller\n");
		return -ENODEV;
	}

	pan_cfg = mdss_panel_intf_type(MDSS_PANEL_INTF_EDP);
	if (IS_ERR(pan_cfg)) {
		return PTR_ERR(pan_cfg);
	} else if (pan_cfg) {
		pr_debug("DP as prim not supported\n");
		return -ENODEV;
	}

	dp_drv = devm_kzalloc(&pdev->dev, sizeof(*dp_drv), GFP_KERNEL);
	if (dp_drv == NULL)
		return -ENOMEM;

	dp_drv->pdev = pdev;
	dp_drv->pdev->id = 1;
	dp_drv->mdss_util = util;
	dp_drv->clk_on = 0;
	dp_drv->aux_rate = 19200000;
	dp_drv->mask1 = EDP_INTR_MASK1;
	dp_drv->mask2 = EDP_INTR_MASK2;
	mutex_init(&dp_drv->emutex);
	spin_lock_init(&dp_drv->lock);

	ret = mdss_retrieve_dp_ctrl_resources(pdev, dp_drv);
	if (ret)
		goto probe_err;

	/* Parse the regulator information */
	for (i = DP_CORE_PM; i < DP_MAX_PM; i++) {
		ret = mdss_dp_get_dt_vreg_data(&pdev->dev,
			pdev->dev.of_node, &dp_drv->power_data[i], i);
		if (ret) {
			pr_err("get_dt_vreg_data failed for %s. rc=%d\n",
				__mdss_dp_pm_name(i), ret);
			i--;
			for (; i >= DP_CORE_PM; i--)
				mdss_dp_put_dt_vreg_data(&pdev->dev,
					&dp_drv->power_data[i]);
			goto probe_err;
		}
	}

	ret = mdss_dp_get_dt_clk_data(&pdev->dev, dp_drv);
	if (ret) {
		DEV_ERR("get_dt_clk_data failed.ret=%d\n",
				ret);
		goto probe_err;
	}

	ret = mdss_dp_regulator_init(pdev, dp_drv);
	if (ret)
		goto probe_err;

	ret = mdss_dp_irq_setup(dp_drv);
	if (ret)
		goto probe_err;

	ret = mdss_dp_event_setup(dp_drv);
	if (ret)
		goto probe_err;

	ret = mdss_dp_clk_ctrl(dp_drv, DP_CORE_PM, true);
	if (ret) {
		pr_err("Unabled to enable core clocks\n");
		goto probe_err;
	}

	pr_info("ctrl_hw_rev =0x%x, phy hw_rev =0x%x\n",
	       mdss_dp_get_ctrl_hw_version(&dp_drv->ctrl_io),
	       mdss_dp_get_phy_hw_version(&dp_drv->phy_io));

	ret = mdss_dp_clk_ctrl(dp_drv, DP_CORE_PM, false);
	if (ret) {
		pr_err("Unabled to disable core clocks\n");
		goto probe_err;
	}

	dp_drv->cont_splash = dp_drv->mdss_util->panel_intf_status(DISPLAY_1,
		MDSS_PANEL_INTF_EDP) ? true : false;

	platform_set_drvdata(pdev, dp_drv);

	mdss_dp_device_register(dp_drv);

	dp_drv->inited = true;

	pr_debug("done\n");

	return 0;

probe_err:
	iounmap(dp_drv->ctrl_io.base);
	iounmap(dp_drv->phy_io.base);
	if (dp_drv)
		devm_kfree(&pdev->dev, dp_drv);
	return ret;

}

static const struct of_device_id msm_mdss_dp_dt_match[] = {
	{.compatible = "qcom,mdss-dp"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_mdss_dp_dt_match);

static struct platform_driver mdss_dp_driver = {
	.probe = mdss_dp_probe,
	.remove = mdss_dp_remove,
	.shutdown = NULL,
	.driver = {
		.name = "mdss_dp",
		.of_match_table = msm_mdss_dp_dt_match,
	},
};

static int __init mdss_dp_init(void)
{
	int ret;

	ret = platform_driver_register(&mdss_dp_driver);
	if (ret) {
		pr_err("driver register failed");
		return ret;
	}

	return ret;
}
module_init(mdss_dp_init);

static void __exit mdss_dp_driver_cleanup(void)
{
	platform_driver_unregister(&mdss_dp_driver);
}
module_exit(mdss_dp_driver_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DP controller driver");
