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
#include "mdss_dp.h"
#include "mdss_dp_util.h"
#include "mdss_hdmi_panel.h"
#include <linux/hdcp_qseecom.h>
#include "mdss_hdcp_1x.h"
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

static int mdss_dp_clk_init(struct mdss_dp_drv_pdata *dp_drv,
				struct device *dev, bool initialize)
{
	struct dss_module_power *core_power_data = NULL;
	struct dss_module_power *ctrl_power_data = NULL;
	int rc = 0;

	if (!dp_drv || !dev) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto exit;
	}

	core_power_data = &dp_drv->power_data[DP_CORE_PM];
	ctrl_power_data = &dp_drv->power_data[DP_CTRL_PM];

	if (!core_power_data || !ctrl_power_data) {
		pr_err("invalid power_data\n");
		rc = -EINVAL;
		goto exit;
	}

	if (initialize) {
		rc = msm_dss_get_clk(dev, core_power_data->clk_config,
			core_power_data->num_clk);
		if (rc) {
			DEV_ERR("Failed to get %s clk. Err=%d\n",
				__mdss_dp_pm_name(DP_CORE_PM), rc);
			goto exit;
		}

		rc = msm_dss_get_clk(dev, ctrl_power_data->clk_config,
			ctrl_power_data->num_clk);
		if (rc) {
			DEV_ERR("Failed to get %s clk. Err=%d\n",
				__mdss_dp_pm_name(DP_CTRL_PM), rc);
			goto ctrl_get_error;
		}

	} else {
		msm_dss_put_clk(ctrl_power_data->clk_config,
					ctrl_power_data->num_clk);
		msm_dss_put_clk(core_power_data->clk_config,
					core_power_data->num_clk);
	}

	return rc;

ctrl_get_error:
	msm_dss_put_clk(core_power_data->clk_config,
				core_power_data->num_clk);

exit:
	return rc;
}

static int mdss_dp_clk_set_rate_enable(
		struct dss_module_power *power_data,
		bool enable)
{
	int ret = 0;

	if (enable) {
		ret = msm_dss_clk_set_rate(
			power_data->clk_config,
			power_data->num_clk);
		if (ret) {
			pr_err("failed to set clks rate.\n");
			goto exit;
		}

		ret = msm_dss_enable_clk(
			power_data->clk_config,
			power_data->num_clk, 1);
		if (ret) {
			pr_err("failed to enable clks\n");
			goto exit;
		}
	} else {
		ret = msm_dss_enable_clk(
			power_data->clk_config,
			power_data->num_clk, 0);
		if (ret) {
			pr_err("failed to disable clks\n");
				goto exit;
		}
	}
exit:
	return ret;
}

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

			ret = mdss_dp_clk_set_rate_enable(
				&dp_drv->power_data[DP_CORE_PM],
				enable);
			if (ret) {
				pr_err("failed to enable clks: %s. err=%d\n",
					__mdss_dp_pm_name(DP_CORE_PM), ret);
				goto error;
			} else {
				dp_drv->core_clks_on = true;
			}
		}
	}

	ret = mdss_dp_clk_set_rate_enable(
		&dp_drv->power_data[pm_type],
		enable);
	if (ret) {
		pr_err("failed to '%s' clks for: %s. err=%d\n",
			enable ? "enable" : "disable",
			__mdss_dp_pm_name(pm_type), ret);
			goto error;
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
	int ret = 0, i = 0, j = 0;

	if (dp_drv->core_power == enable) {
		pr_debug("regulators already %s\n",
			enable ? "enabled" : "disabled");
		return 0;
	}

	for (i = DP_CORE_PM; i < DP_MAX_PM; i++) {
		ret = msm_dss_enable_vreg(
			dp_drv->power_data[i].vreg_config,
			dp_drv->power_data[i].num_vreg, enable);
		if (ret) {
			pr_err("failed to '%s' vregs for %s\n",
					enable ? "enable" : "disable",
					__mdss_dp_pm_name(i));
			if (enable) {
				/* Disabling the enabled vregs */
				for (j = i-1; j >= DP_CORE_PM; j--) {
					msm_dss_enable_vreg(
					dp_drv->power_data[j].vreg_config,
					dp_drv->power_data[j].num_vreg, 0);
				}
			}
			goto error;
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

static int mdss_dp_pinctrl_set_state(
	struct mdss_dp_drv_pdata *dp,
	bool active)
{
	struct pinctrl_state *pin_state;
	int rc = -EFAULT;

	if (IS_ERR_OR_NULL(dp->pin_res.pinctrl))
		return PTR_ERR(dp->pin_res.pinctrl);

	pin_state = active ? dp->pin_res.state_active
				: dp->pin_res.state_suspend;
	if (!IS_ERR_OR_NULL(pin_state)) {
		rc = pinctrl_select_state(dp->pin_res.pinctrl,
				pin_state);
		if (rc)
			pr_err("can not set %s pins\n",
			       active ? "mdss_dp_active"
			       : "mdss_dp_sleep");
	} else {
		pr_err("invalid '%s' pinstate\n",
		       active ? "mdss_dp_active"
		       : "mdss_dp_sleep");
	}
	return rc;
}

static int mdss_dp_pinctrl_init(struct platform_device *pdev,
			struct mdss_dp_drv_pdata *dp)
{
	dp->pin_res.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(dp->pin_res.pinctrl)) {
		pr_err("failed to get pinctrl\n");
		return PTR_ERR(dp->pin_res.pinctrl);
	}

	dp->pin_res.state_active
		= pinctrl_lookup_state(dp->pin_res.pinctrl,
				"mdss_dp_active");
	if (IS_ERR_OR_NULL(dp->pin_res.state_active)) {
		pr_err("can not get dp active pinstate\n");
		return PTR_ERR(dp->pin_res.state_active);
	}

	dp->pin_res.state_suspend
		= pinctrl_lookup_state(dp->pin_res.pinctrl,
				"mdss_dp_sleep");
	if (IS_ERR_OR_NULL(dp->pin_res.state_suspend)) {
		pr_err("can not get dp sleep pinstate\n");
		return PTR_ERR(dp->pin_res.state_suspend);
	}

	return 0;
}

static int mdss_dp_request_gpios(struct mdss_dp_drv_pdata *dp)
{
	int rc = 0;
	struct device *dev = NULL;

	if (!dp) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dev = &dp->pdev->dev;
	if (gpio_is_valid(dp->aux_en_gpio)) {
		rc = devm_gpio_request(dev, dp->aux_en_gpio,
						"aux_enable");
		if (rc) {
			pr_err("request aux_en gpio failed, rc=%d\n",
				       rc);
			goto aux_en_gpio_err;
		}
	}
	if (gpio_is_valid(dp->aux_sel_gpio)) {
		rc = devm_gpio_request(dev, dp->aux_sel_gpio, "aux_sel");
		if (rc) {
			pr_err("request aux_sel gpio failed, rc=%d\n",
				rc);
			goto aux_sel_gpio_err;
		}
	}
	if (gpio_is_valid(dp->usbplug_cc_gpio)) {
		rc = devm_gpio_request(dev, dp->usbplug_cc_gpio,
						"usbplug_cc");
		if (rc) {
			pr_err("request usbplug_cc gpio failed, rc=%d\n",
				rc);
			goto usbplug_cc_gpio_err;
		}
	}
	if (gpio_is_valid(dp->hpd_gpio)) {
		rc = devm_gpio_request(dev, dp->hpd_gpio, "hpd");
		if (rc) {
			pr_err("request hpd gpio failed, rc=%d\n",
				rc);
			goto hpd_gpio_err;
		}
	}
	return rc;

hpd_gpio_err:
	if (gpio_is_valid(dp->usbplug_cc_gpio))
		gpio_free(dp->usbplug_cc_gpio);
usbplug_cc_gpio_err:
	if (gpio_is_valid(dp->aux_sel_gpio))
		gpio_free(dp->aux_sel_gpio);
aux_sel_gpio_err:
	if (gpio_is_valid(dp->aux_en_gpio))
		gpio_free(dp->aux_en_gpio);
aux_en_gpio_err:
	return rc;
}

static int mdss_dp_config_gpios(struct mdss_dp_drv_pdata *dp, bool enable)
{
	int rc = 0;

	if (enable == true) {
		rc = mdss_dp_request_gpios(dp);
		if (rc) {
			pr_err("gpio request failed\n");
			return rc;
		}

		if (gpio_is_valid(dp->aux_en_gpio)) {
			rc = gpio_direction_output(
				dp->aux_en_gpio, 0);
			if (rc)
				pr_err("unable to set dir for aux_en gpio\n");
		}
		if (gpio_is_valid(dp->aux_sel_gpio)) {
			rc = gpio_direction_output(
				dp->aux_sel_gpio, 0);
			if (rc)
				pr_err("unable to set dir for aux_sel gpio\n");
		}
		if (gpio_is_valid(dp->usbplug_cc_gpio)) {
			gpio_set_value(
				dp->usbplug_cc_gpio, 0);
		}
		if (gpio_is_valid(dp->hpd_gpio)) {
			gpio_set_value(
				dp->hpd_gpio, 1);
		}
	} else {
		if (gpio_is_valid(dp->aux_en_gpio)) {
			gpio_set_value((dp->aux_en_gpio), 0);
			gpio_free(dp->aux_en_gpio);
		}
		if (gpio_is_valid(dp->aux_sel_gpio)) {
			gpio_set_value((dp->aux_sel_gpio), 0);
			gpio_free(dp->aux_sel_gpio);
		}
		if (gpio_is_valid(dp->usbplug_cc_gpio)) {
			gpio_set_value((dp->usbplug_cc_gpio), 0);
			gpio_free(dp->usbplug_cc_gpio);
		}
		if (gpio_is_valid(dp->hpd_gpio)) {
			gpio_set_value((dp->hpd_gpio), 0);
			gpio_free(dp->hpd_gpio);
		}
	}
	return 0;
}

static int mdss_dp_parse_gpio_params(struct platform_device *pdev,
	struct mdss_dp_drv_pdata *dp)
{
	dp->aux_en_gpio = of_get_named_gpio(
			pdev->dev.of_node,
			"qcom,aux-en-gpio", 0);

	if (!gpio_is_valid(dp->aux_en_gpio)) {
		pr_err("%d, Aux_en gpio not specified\n",
					__LINE__);
		return -EINVAL;
	}

	dp->aux_sel_gpio = of_get_named_gpio(
			pdev->dev.of_node,
			"qcom,aux-sel-gpio", 0);

	if (!gpio_is_valid(dp->aux_sel_gpio)) {
		pr_err("%d, Aux_sel gpio not specified\n",
					__LINE__);
		return -EINVAL;
	}

	dp->usbplug_cc_gpio = of_get_named_gpio(
			pdev->dev.of_node,
			"qcom,usbplug-cc-gpio", 0);

	if (!gpio_is_valid(dp->usbplug_cc_gpio)) {
		pr_err("%d,usbplug_cc gpio not specified\n",
					__LINE__);
		return -EINVAL;
	}

	dp->hpd_gpio = of_get_named_gpio(
			pdev->dev.of_node,
			"qcom,hpd-gpio", 0);

	if (!gpio_is_valid(dp->hpd_gpio)) {
		pr_info("%d,hpd gpio not specified\n",
					__LINE__);
	}

	return 0;
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

static int dp_init_panel_info(struct mdss_dp_drv_pdata *dp_drv, u32 vic)
{
	struct mdss_panel_info *pinfo;
	struct msm_hdmi_mode_timing_info timing = {0};
	u32 ret;

	if (!dp_drv) {
		DEV_ERR("invalid input\n");
		return -EINVAL;
	}

	dp_drv->ds_data.ds_registered = false;
	ret = hdmi_get_supported_mode(&timing, &dp_drv->ds_data, vic);
	pinfo = &dp_drv->panel_data.panel_info;

	if (ret || !timing.supported || !pinfo) {
		DEV_ERR("%s: invalid timing data\n", __func__);
		return -EINVAL;
	}

	dp_drv->vic = vic;
	pinfo->xres = timing.active_h;
	pinfo->yres = timing.active_v;
	pinfo->clk_rate = timing.pixel_freq * 1000;

	pinfo->lcdc.h_back_porch = timing.back_porch_h;
	pinfo->lcdc.h_front_porch = timing.front_porch_h;
	pinfo->lcdc.h_pulse_width = timing.pulse_width_h;
	pinfo->lcdc.v_back_porch = timing.back_porch_v;
	pinfo->lcdc.v_front_porch = timing.front_porch_v;
	pinfo->lcdc.v_pulse_width = timing.pulse_width_v;

	pinfo->type = DP_PANEL;
	pinfo->pdest = DISPLAY_4;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 1;

	pinfo->lcdc.border_clr = 0; /* blk */
	pinfo->lcdc.underflow_clr = 0xff; /* blue */
	pinfo->lcdc.hsync_skew = 0;
	pinfo->is_pluggable = true;

	pr_debug("update res. vic= %d, pclk_rate = %llu\n",
				dp_drv->vic, pinfo->clk_rate);

	return 0;
} /* dp_init_panel_info */


int mdss_dp_on(struct mdss_panel_data *pdata)
{
	struct mdss_dp_drv_pdata *dp_drv = NULL;
	int ret = 0;
	enum plug_orientation orientation = ORIENTATION_NONE;
	struct lane_mapping ln_map;

	if (!pdata) {
		pr_err("Invalid input data\n");
		return -EINVAL;
	}

	dp_drv = container_of(pdata, struct mdss_dp_drv_pdata,
			panel_data);

	/* wait until link training is completed */
	mutex_lock(&dp_drv->host_mutex);

	pr_debug("Enter++ cont_splash=%d\n", dp_drv->cont_splash);
	/* Default lane mapping */
	ln_map.lane0 = 2;
	ln_map.lane1 = 3;
	ln_map.lane2 = 1;
	ln_map.lane3 = 0;

	if (!dp_drv->cont_splash) { /* vote for clocks */
		ret = mdss_dp_clk_ctrl(dp_drv, DP_CORE_PM, true);
		if (ret) {
			pr_err("Unabled to start core clocks\n");
			return ret;
		}
		mdss_dp_hpd_configure(&dp_drv->ctrl_io, true);

		orientation = usbpd_get_plug_orientation(dp_drv->pd);
		pr_debug("plug Orientation = %d\n", orientation);

		if (orientation == ORIENTATION_CC2) {
			/* update lane mapping */
			ln_map.lane0 = 1;
			ln_map.lane1 = 0;
			ln_map.lane2 = 2;
			ln_map.lane3 = 3;

			if (gpio_is_valid(dp_drv->usbplug_cc_gpio)) {
				gpio_set_value(
					dp_drv->usbplug_cc_gpio, 1);
				pr_debug("Configured cc gpio for new Orientation\n");
			}

		}

		if (dp_drv->new_vic && (dp_drv->new_vic != dp_drv->vic))
			dp_init_panel_info(dp_drv, dp_drv->new_vic);

		dp_drv->link_rate =
			mdss_dp_gen_link_clk(&dp_drv->panel_data.panel_info,
						dp_drv->dpcd.max_lane_count);

		pr_debug("link_rate=0x%x, Max rate supported by sink=0x%x\n",
			       dp_drv->link_rate, dp_drv->dpcd.max_link_rate);
		if (!dp_drv->link_rate) {
			pr_err("Unable to configure required link rate\n");
			return -EINVAL;
		}

		pr_debug("link_rate = 0x%x\n", dp_drv->link_rate);

		dp_drv->power_data[DP_CTRL_PM].clk_config[0].rate =
				dp_drv->link_rate * DP_LINK_RATE_MULTIPLIER;

		dp_drv->pixel_rate = dp_drv->panel_data.panel_info.clk_rate;
		dp_drv->power_data[DP_CTRL_PM].clk_config[3].rate =
							dp_drv->pixel_rate;

		ret = mdss_dp_clk_ctrl(dp_drv, DP_CTRL_PM, true);
		if (ret) {
			mdss_dp_clk_ctrl(dp_drv, DP_CORE_PM, false);
			pr_err("Unabled to start link clocks\n");
			return ret;
		}

		mdss_dp_mainlink_reset(&dp_drv->ctrl_io);

		mdss_dp_ctrl_lane_mapping(&dp_drv->ctrl_io, ln_map);
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

	dp_drv->power_on = true;

	mutex_unlock(&dp_drv->host_mutex);
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

	mdss_dp_config_gpios(dp_drv, false);
	mdss_dp_pinctrl_set_state(dp_drv, false);

	mdss_dp_aux_ctrl(&dp_drv->ctrl_io, false);
	mdss_dp_clk_ctrl(dp_drv, DP_CTRL_PM, false);
	mdss_dp_clk_ctrl(dp_drv, DP_CORE_PM, false);

	mdss_dp_regulator_ctrl(dp_drv, false);

	pr_debug("End--: state_ctrl=%x\n",
				dp_read(dp_drv->base + DP_STATE_CTRL));

	dp_drv->power_on = false;
	mutex_unlock(&dp_drv->train_mutex);
	return 0;
}

static void mdss_dp_send_cable_notification(
	struct mdss_dp_drv_pdata *dp, int val)
{
	int state = 0;

	if (!dp) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}
	state = dp->sdev.state;

	switch_set_state(&dp->sdev, val);

	DEV_INFO("%s: cable state %s %d\n", __func__,
		dp->sdev.state == state ?
			"is same" : "switched to",
		dp->sdev.state);
}

static int mdss_dp_register_switch_event(struct mdss_dp_drv_pdata *dp)
{
	int rc = -EINVAL;

	if (!dp) {
		DEV_ERR("%s: invalid input\n", __func__);
		goto end;
	}

	dp->sdev.name = "hdmi";
	rc = switch_dev_register(&dp->sdev);
	if (rc) {
		DEV_ERR("%s: display switch registration failed\n", __func__);
		goto end;
	}
end:
	return rc;
}

static int mdss_dp_edid_init(struct mdss_panel_data *pdata)
{
	struct mdss_dp_drv_pdata *dp_drv = NULL;
	struct hdmi_edid_init_data edid_init_data = {0};
	void *edid_data;

	if (!pdata) {
		pr_err("Invalid input data\n");
		return -EINVAL;
	}

	dp_drv = container_of(pdata, struct mdss_dp_drv_pdata,
			panel_data);

	dp_drv->max_pclk_khz = DP_MAX_PIXEL_CLK_KHZ;
	edid_init_data.kobj = dp_drv->kobj;
	edid_init_data.ds_data = dp_drv->ds_data;
	edid_init_data.max_pclk_khz = dp_drv->max_pclk_khz;

	edid_data = hdmi_edid_init(&edid_init_data);
	if (!edid_data) {
		DEV_ERR("%s: edid init failed\n", __func__);
		return -ENODEV;
	}

	dp_drv->panel_data.panel_info.edid_data = edid_data;
	/* initialize EDID buffer pointers */
	dp_drv->edid_buf = edid_init_data.buf;
	dp_drv->edid_buf_size = edid_init_data.buf_size;

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

	mdss_dp_pinctrl_set_state(dp_drv, true);
	mdss_dp_config_gpios(dp_drv, true);

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

	pr_debug("Ctrl_hw_rev =0x%x, phy hw_rev =0x%x\n",
	       mdss_dp_get_ctrl_hw_version(&dp_drv->ctrl_io),
	       mdss_dp_get_phy_hw_version(&dp_drv->phy_io));

	mdss_dp_phy_aux_setup(&dp_drv->phy_io);

	mdss_dp_irq_enable(dp_drv);
	pr_debug("irq enabled\n");
	mdss_dp_dpcd_cap_read(dp_drv);

	ret = mdss_dp_edid_read(dp_drv);
	if (ret)
		goto edid_error;

	pr_debug("edid_read success. buf_size=%d\n",
				dp_drv->edid_buf_size);

	ret = hdmi_edid_parser(dp_drv->panel_data.panel_info.edid_data);
	if (ret) {
		DEV_ERR("%s: edid parse failed\n", __func__);
		goto edid_error;
	}

	mdss_dp_send_cable_notification(dp_drv, true);

	return ret;

edid_error:
	mdss_dp_clk_ctrl(dp_drv, DP_CORE_PM, false);
clk_error:
	mdss_dp_regulator_ctrl(dp_drv, false);
vreg_error:
	return ret;
}

static int mdss_dp_check_params(struct mdss_dp_drv_pdata *dp, void *arg)
{
	struct mdss_panel_info *var_pinfo, *pinfo;
	int rc = 0;
	int new_vic = -1;

	if (!dp || !arg)
		return 0;

	pinfo = &dp->panel_data.panel_info;
	var_pinfo = (struct mdss_panel_info *)arg;

	pr_debug("reconfig xres: %d yres: %d, current xres: %d yres: %d\n",
			var_pinfo->xres, var_pinfo->yres,
					pinfo->xres, pinfo->yres);

	new_vic = hdmi_panel_get_vic(var_pinfo, &dp->ds_data);

	if ((new_vic < 0) || (new_vic > HDMI_VFRMT_MAX)) {
		DEV_ERR("%s: invalid or not supported vic\n", __func__);
		goto end;
	}

	/*
	 * return value of 1 lets mdss know that panel
	 * needs a reconfig due to new resolution and
	 * it will issue close and open subsequently.
	 */
	if (new_vic != dp->vic) {
		rc = 1;
		DEV_ERR("%s: res change %d ==> %d\n", __func__,
			dp->vic, new_vic);
	}
	dp->new_vic = new_vic;
end:
	return rc;
}

static void mdss_dp_hdcp_cb(void *ptr, enum hdcp_states status)
{
	struct mdss_dp_drv_pdata *dp = ptr;
	struct hdcp_ops *ops;
	int rc = 0;

	if (!dp) {
		pr_debug("invalid input\n");
		return;
	}

	ops = dp->hdcp_ops;

	mutex_lock(&dp->train_mutex);

	switch (status) {
	case HDCP_STATE_AUTHENTICATED:
		pr_debug("hdcp 1.3 authenticated\n");
		break;
	case HDCP_STATE_AUTH_FAIL:
		if (dp->power_on) {
			pr_debug("Reauthenticating\n");
			if (ops && ops->reauthenticate) {
				rc = ops->reauthenticate(dp->hdcp_data);
				if (rc)
					pr_err("HDCP reauth failed. rc=%d\n",
						rc);
			}
		} else {
			pr_debug("not reauthenticating, cable disconnected\n");
		}

		break;
	default:
		break;
	}

	mutex_unlock(&dp->train_mutex);
}

static int mdss_dp_hdcp_init(struct mdss_panel_data *pdata)
{
	struct hdcp_init_data hdcp_init_data = {0};
	struct mdss_dp_drv_pdata *dp_drv = NULL;
	struct resource *res;
	int rc = 0;

	if (!pdata) {
		pr_err("Invalid input data\n");
		goto error;
	}

	dp_drv = container_of(pdata, struct mdss_dp_drv_pdata,
			panel_data);

	res = platform_get_resource_byname(dp_drv->pdev,
		IORESOURCE_MEM, "dp_ctrl");
	if (!res) {
		pr_err("Error getting dp ctrl resource\n");
		rc = -EINVAL;
		goto error;
	}

	hdcp_init_data.phy_addr      = res->start;
	hdcp_init_data.core_io       = &dp_drv->ctrl_io;
	hdcp_init_data.qfprom_io     = &dp_drv->qfprom_io;
	hdcp_init_data.hdcp_io       = &dp_drv->hdcp_io;
	hdcp_init_data.mutex         = &dp_drv->hdcp_mutex;
	hdcp_init_data.sysfs_kobj    = dp_drv->kobj;
	hdcp_init_data.workq         = dp_drv->workq;
	hdcp_init_data.notify_status = mdss_dp_hdcp_cb;
	hdcp_init_data.cb_data       = (void *)dp_drv;
	hdcp_init_data.sec_access    = true;
	hdcp_init_data.client_id     = HDCP_CLIENT_DP;

	dp_drv->hdcp_data = hdcp_1x_init(&hdcp_init_data);
	if (IS_ERR_OR_NULL(dp_drv->hdcp_data)) {
		pr_err("Error hdcp init\n");
		rc = -EINVAL;
		goto error;
	}

	pr_debug("HDCP 1.3 initialized\n");

	dp_drv->hdcp_ops = hdcp_1x_start(dp_drv->hdcp_data);

	return 0;
error:
	return rc;
}

static int mdss_dp_event_handler(struct mdss_panel_data *pdata,
				  int event, void *arg)
{
	int rc = 0;
	struct fb_info *fbi;
	struct mdss_dp_drv_pdata *dp = NULL;
	struct hdcp_ops *ops;

	if (!pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pr_debug("event=%d\n", event);

	dp = container_of(pdata, struct mdss_dp_drv_pdata,
				panel_data);

	ops = dp->hdcp_ops;

	switch (event) {
	case MDSS_EVENT_UNBLANK:
		rc = mdss_dp_on(pdata);
		break;
	case MDSS_EVENT_PANEL_ON:
		if (hdcp1_check_if_supported_load_app()) {
			if (ops && ops->authenticate)
				rc = ops->authenticate(dp->hdcp_data);
		}
		break;
	case MDSS_EVENT_PANEL_OFF:
		rc = mdss_dp_off(pdata);
		break;
	case MDSS_EVENT_BLANK:
		if (ops && ops->off)
			ops->off(dp->hdcp_data);
		break;
	case MDSS_EVENT_FB_REGISTERED:
		fbi = (struct fb_info *)arg;
		if (!fbi || !fbi->dev)
			break;

		dp->kobj = &fbi->dev->kobj;
		dp->fb_node = fbi->node;
		mdss_dp_edid_init(pdata);
		mdss_dp_hdcp_init(pdata);
		mdss_dp_register_switch_event(dp);
		break;
	case MDSS_EVENT_CHECK_PARAMS:
		rc = mdss_dp_check_params(dp, arg);
		break;
	default:
		pr_debug("unhandled event=%d\n", event);
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

	ret = dp_init_panel_info(dp_drv, DEFAULT_VIDEO_RESOLUTION);
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

	if (msm_dss_ioremap_byname(pdev, &dp_drv->qfprom_io,
					"qfprom_physical"))
		pr_warn("unable to remap dp qfprom resources\n");

	if (msm_dss_ioremap_byname(pdev, &dp_drv->hdcp_io,
					"hdcp_physical"))
		pr_warn("unable to remap dp hdcp resources\n");

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
	u32 todo = 0, dp_config_pkt[2];

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
	case EV_EDID_READ:
		mdss_dp_edid_read(dp);
		break;
	case EV_DPCD_CAP_READ:
		mdss_dp_dpcd_cap_read(dp);
		break;
	case EV_DPCD_STATUS_READ:
		mdss_dp_dpcd_status_read(dp);
		break;
	case EV_LINK_TRAIN:
		mdss_dp_do_link_train(dp);
		break;
	case EV_VIDEO_READY:
		mdss_dp_video_ready(dp);
		break;
	case EV_IDLE_PATTERNS_SENT:
		mdss_dp_idle_patterns_sent(dp);
		break;
	case EV_USBPD_DISCOVER_MODES:
		usbpd_send_svdm(dp->pd, USB_C_DP_SID, USBPD_SVDM_DISCOVER_MODES,
			SVDM_CMD_TYPE_INITIATOR, 0x0, 0x0, 0x0);
		break;
	case EV_USBPD_ENTER_MODE:
		usbpd_send_svdm(dp->pd, USB_C_DP_SID, USBPD_SVDM_ENTER_MODE,
			SVDM_CMD_TYPE_INITIATOR, 0x1, 0x0, 0x0);
		break;
	case EV_USBPD_EXIT_MODE:
		usbpd_send_svdm(dp->pd, USB_C_DP_SID, USBPD_SVDM_EXIT_MODE,
			SVDM_CMD_TYPE_INITIATOR, 0x1, 0x0, 0x0);
		break;
	case EV_USBPD_DP_STATUS:
		usbpd_send_svdm(dp->pd, USB_C_DP_SID, DP_VDM_STATUS,
			SVDM_CMD_TYPE_INITIATOR, 0x1, 0x0, 0x0);
		break;
	case EV_USBPD_DP_CONFIGURE:
		dp_config_pkt[0] = SVDM_HDR(USB_C_DP_SID, VDM_VERSION, 0x1,
			SVDM_CMD_TYPE_INITIATOR, DP_VDM_CONFIGURE);
		dp_config_pkt[1] = mdss_dp_usbpd_gen_config_pkt(dp);
		usbpd_send_svdm(dp->pd, USB_C_DP_SID, DP_VDM_CONFIGURE,
			SVDM_CMD_TYPE_INITIATOR, 0x1, dp_config_pkt, 0x2);
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

	if (dp->hdcp_ops && dp->hdcp_ops->isr) {
		if (dp->hdcp_ops->isr(dp->hdcp_data))
			pr_err("dp_hdcp_isr failed\n");
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

static void usbpd_connect_callback(struct usbpd_svid_handler *hdlr)
{
	struct mdss_dp_drv_pdata *dp_drv;

	dp_drv = container_of(hdlr, struct mdss_dp_drv_pdata, svid_handler);
	if (!dp_drv->pd) {
		pr_err("get_usbpd phandle failed\n");
		return;
	}

	mutex_lock(&dp_drv->pd_msg_mutex);
	dp_drv->cable_connected = true;
	dp_send_events(dp_drv, EV_USBPD_DISCOVER_MODES);
	mutex_unlock(&dp_drv->pd_msg_mutex);
	pr_debug("discover_mode event sent\n");
}

static void usbpd_disconnect_callback(struct usbpd_svid_handler *hdlr)
{
	struct mdss_dp_drv_pdata *dp_drv;

	dp_drv = container_of(hdlr, struct mdss_dp_drv_pdata, svid_handler);
	if (!dp_drv->pd) {
		pr_err("get_usbpd phandle failed\n");
		return;
	}

	pr_debug("cable disconnected\n");
	mutex_lock(&dp_drv->pd_msg_mutex);
	dp_drv->cable_connected = false;
	mutex_unlock(&dp_drv->pd_msg_mutex);
	mdss_dp_send_cable_notification(dp_drv, false);
}

static void usbpd_response_callback(struct usbpd_svid_handler *hdlr, u8 cmd,
				enum usbpd_svdm_cmd_type cmd_type,
				const u32 *vdos, int num_vdos)
{
	struct mdss_dp_drv_pdata *dp_drv;

	dp_drv = container_of(hdlr, struct mdss_dp_drv_pdata, svid_handler);
	if (!dp_drv->pd) {
		pr_err("get_usbpd phandle failed\n");
		return;
	}

	pr_debug("callback -> cmd: 0x%x, *vdos = 0x%x, num_vdos = %d\n",
				cmd, *vdos, num_vdos);

	switch (cmd) {
	case USBPD_SVDM_DISCOVER_MODES:
		if (cmd_type == SVDM_CMD_TYPE_RESP_ACK) {
			dp_drv->alt_mode.dp_cap.response = *vdos;
			mdss_dp_usbpd_ext_capabilities
					(&dp_drv->alt_mode.dp_cap);
			dp_drv->alt_mode.current_state = DISCOVER_MODES_DONE;
			dp_send_events(dp_drv, EV_USBPD_ENTER_MODE);
		} else {
			pr_err("unknown response: %d for Discover_modes\n",
			       cmd_type);
		}
		break;
	case USBPD_SVDM_ENTER_MODE:
		if (cmd_type == SVDM_CMD_TYPE_RESP_ACK) {
			dp_drv->alt_mode.current_state = ENTER_MODE_DONE;
			dp_send_events(dp_drv, EV_USBPD_DP_STATUS);
		} else {
			pr_err("unknown response: %d for Enter_mode\n",
			       cmd_type);
		}
		break;
	case USBPD_SVDM_ATTENTION:
		if (cmd_type == SVDM_CMD_TYPE_INITIATOR) {
			pr_debug("Attention. cmd_type=%d\n",
			       cmd_type);
			if (!dp_drv->alt_mode.current_state
						== ENTER_MODE_DONE) {
				pr_debug("sending discover_mode\n");
				dp_send_events(dp_drv, EV_USBPD_DISCOVER_MODES);
				break;
			}
			if (num_vdos == 1) {
				dp_drv->alt_mode.dp_status.response = *vdos;
				mdss_dp_usbpd_ext_dp_status
						(&dp_drv->alt_mode.dp_status);
				if (dp_drv->alt_mode.dp_status.hpd_high) {
					pr_debug("HPD high\n");
					dp_drv->alt_mode.current_state =
							DP_STATUS_DONE;
					dp_send_events
						(dp_drv, EV_USBPD_DP_CONFIGURE);
				}
			}
		} else {
			pr_debug("unknown response: %d for Attention\n",
			       cmd_type);
		}
		break;
	case DP_VDM_STATUS:
		if (cmd_type == SVDM_CMD_TYPE_RESP_ACK) {
			dp_drv->alt_mode.dp_status.response = *vdos;
			mdss_dp_usbpd_ext_dp_status
					(&dp_drv->alt_mode.dp_status);
			if (dp_drv->alt_mode.dp_status.hpd_high) {
				pr_debug("HDP high\n");
				dp_drv->alt_mode.current_state =
						DP_STATUS_DONE;
				dp_send_events(dp_drv, EV_USBPD_DP_CONFIGURE);
			}
		} else {
			pr_err("unknown response: %d for DP_Status\n",
			       cmd_type);
		}
		break;
	case DP_VDM_CONFIGURE:
		if ((dp_drv->cable_connected == true)
				|| (cmd_type == SVDM_CMD_TYPE_RESP_ACK)) {
			dp_drv->alt_mode.current_state = DP_CONFIGURE_DONE;
			pr_debug("config USBPD to DP done\n");
			mdss_dp_host_init(&dp_drv->panel_data);
		} else {
			pr_err("unknown response: %d for DP_Configure\n",
			       cmd_type);
		}
		break;
	default:
		pr_err("unknown cmd: %d\n", cmd);
		break;
	}
}

static int mdss_dp_usbpd_setup(struct mdss_dp_drv_pdata *dp_drv)
{
	int ret = 0;
	const char *pd_phandle = "qcom,dp-usbpd-detection";

	dp_drv->pd = devm_usbpd_get_by_phandle(&dp_drv->pdev->dev,
						    pd_phandle);

	if (IS_ERR(dp_drv->pd)) {
		pr_err("get_usbpd phandle failed (%ld)\n",
				PTR_ERR(dp_drv->pd));
		return PTR_ERR(dp_drv->pd);
	}

	dp_drv->svid_handler.svid = USB_C_DP_SID;
	dp_drv->svid_handler.vdm_received = NULL;
	dp_drv->svid_handler.connect = &usbpd_connect_callback;
	dp_drv->svid_handler.svdm_received = &usbpd_response_callback;
	dp_drv->svid_handler.disconnect = &usbpd_disconnect_callback;

	ret = usbpd_register_svid(dp_drv->pd, &dp_drv->svid_handler);
	if (ret) {
		pr_err("usbpd registration failed\n");
		return -ENODEV;
	}

	return ret;
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
	mutex_init(&dp_drv->host_mutex);
	mutex_init(&dp_drv->pd_msg_mutex);
	mutex_init(&dp_drv->hdcp_mutex);
	spin_lock_init(&dp_drv->lock);

	if (mdss_dp_usbpd_setup(dp_drv)) {
		pr_err("Error usbpd setup!\n");
		devm_kfree(&pdev->dev, dp_drv);
		dp_drv = NULL;
		return -EPROBE_DEFER;
	}

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

	ret = mdss_dp_clk_init(dp_drv,
				&pdev->dev, true);
	if (ret) {
		DEV_ERR("clk_init failed.ret=%d\n",
				ret);
		goto probe_err;
	}

	ret = mdss_dp_irq_setup(dp_drv);
	if (ret)
		goto probe_err;

	ret = mdss_dp_event_setup(dp_drv);
	if (ret)
		goto probe_err;

	dp_drv->cont_splash = dp_drv->mdss_util->panel_intf_status(DISPLAY_1,
		MDSS_PANEL_INTF_EDP) ? true : false;

	platform_set_drvdata(pdev, dp_drv);

	ret = mdss_dp_pinctrl_init(pdev, dp_drv);
	if (ret) {
		pr_err("pinctrl init failed, ret=%d\n",
						ret);
		goto probe_err;
	}

	ret = mdss_dp_parse_gpio_params(pdev, dp_drv);
	if (ret) {
		pr_err("failed to parse gpio params, ret=%d\n",
						ret);
		goto probe_err;
	}

	mdss_dp_device_register(dp_drv);

	dp_drv->inited = true;

	pr_debug("done\n");

	dp_send_events(dp_drv, EV_USBPD_DISCOVER_MODES);

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
