/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#include <linux/msm_ext_display.h>

#include "mdss.h"
#include "mdss_dp.h"
#include "mdss_dp_util.h"
#include "mdss_hdmi_panel.h"
#include <linux/hdcp_qseecom.h>
#include "mdss_hdcp.h"
#include "mdss_debug.h"

#define RGB_COMPONENTS		3
#define VDDA_MIN_UV			1800000	/* uV units */
#define VDDA_MAX_UV			1800000	/* uV units */
#define VDDA_UA_ON_LOAD		100000	/* uA units */
#define VDDA_UA_OFF_LOAD	100		/* uA units */

#define DP_CRYPTO_CLK_RATE_KHZ 337500

struct mdss_dp_attention_node {
	u32 vdo;
	struct list_head list;
};

#define DEFAULT_VIDEO_RESOLUTION HDMI_VFRMT_640x480p60_4_3

static int mdss_dp_off_irq(struct mdss_dp_drv_pdata *dp_drv);
static void mdss_dp_mainlink_push_idle(struct mdss_panel_data *pdata);
static inline void mdss_dp_link_maintenance(struct mdss_dp_drv_pdata *dp,
		bool lt_needed);
static void mdss_dp_handle_attention(struct mdss_dp_drv_pdata *dp_drv);
static void dp_send_events(struct mdss_dp_drv_pdata *dp, u32 events);
static int mdss_dp_notify_clients(struct mdss_dp_drv_pdata *dp,
	enum notification_status status);
static int mdss_dp_process_phy_test_pattern_request(
		struct mdss_dp_drv_pdata *dp);

static inline void mdss_dp_reset_test_data(struct mdss_dp_drv_pdata *dp)
{
	dp->test_data = (const struct dpcd_test_request){ 0 };
	dp->test_data.test_bit_depth = DP_TEST_BIT_DEPTH_UNKNOWN;
	hdmi_edid_config_override(dp->panel_data.panel_info.edid_data,
				false, 0);
}

static inline bool mdss_dp_is_link_status_updated(struct mdss_dp_drv_pdata *dp)
{
	return dp->link_status.link_status_updated;
}

static inline bool mdss_dp_is_downstream_port_status_changed(
		struct mdss_dp_drv_pdata *dp)
{
	return dp->link_status.downstream_port_status_changed;
}

static inline bool mdss_dp_is_audio_pattern_requested(
		struct mdss_dp_drv_pdata *dp)
{
	return (dp->test_data.test_requested & TEST_AUDIO_PATTERN);
}

static inline bool mdss_dp_is_link_training_requested(
		struct mdss_dp_drv_pdata *dp)
{
	return (dp->test_data.test_requested == TEST_LINK_TRAINING);
}

static inline bool mdss_dp_is_video_pattern_requested(
		struct mdss_dp_drv_pdata *dp)
{
	return (dp->test_data.test_requested & TEST_VIDEO_PATTERN)
		&& !(dp->test_data.test_requested & TEST_AUDIO_DISABLED_VIDEO);
}

static inline bool mdss_dp_is_phy_test_pattern_requested(
		struct mdss_dp_drv_pdata *dp)
{
	return (dp->test_data.test_requested == PHY_TEST_PATTERN);
}

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

static int mdss_dp_parse_prop(struct platform_device *pdev,
			struct mdss_dp_drv_pdata *dp_drv)
{
	int len = 0, i = 0, rc = 0;
	const char *data;

	data = of_get_property(pdev->dev.of_node,
		"qcom,aux-cfg-settings", &len);
	if ((!data) || (len != AUX_CFG_LEN)) {
		pr_err("%s:%d, Unable to read DP AUX CFG settings",
			__func__, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < len; i++)
		dp_drv->aux_cfg[i] = data[i];

	data = of_get_property(pdev->dev.of_node,
		"qcom,logical2physical-lane-map", &len);
	if ((!data) || (len != DP_MAX_PHY_LN)) {
		pr_debug("%s:%d, lane mapping not defined, use default",
			__func__, __LINE__);
		dp_drv->l_map[DP_PHY_LN0] = DP_ML0;
		dp_drv->l_map[DP_PHY_LN1] = DP_ML1;
		dp_drv->l_map[DP_PHY_LN2] = DP_ML2;
		dp_drv->l_map[DP_PHY_LN3] = DP_ML3;
	} else {
		for (i = 0; i < len; i++)
			dp_drv->l_map[i] = data[i];
	}

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,max-pclk-frequency-khz", &dp_drv->max_pclk_khz);
	if (rc)
		dp_drv->max_pclk_khz = DP_MAX_PIXEL_CLK_KHZ;

	return 0;
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
			if (!strcmp(clk_name, "ctrl_link_clk") ||
			    !strcmp(clk_name, "ctrl_pixel_clk") ||
			    !strcmp(clk_name, "ctrl_crypto_clk"))
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

		dp_drv->pixel_clk_rcg = devm_clk_get(dev, "pixel_clk_rcg");
		if (IS_ERR(dp_drv->pixel_clk_rcg)) {
			pr_debug("%s: Unable to get DP pixel clk RCG\n",
				__func__);
			dp_drv->pixel_clk_rcg = NULL;
		}

		dp_drv->pixel_parent = devm_clk_get(dev,
			"pixel_parent");
		if (IS_ERR(dp_drv->pixel_parent)) {
			pr_debug("%s: Unable to get DP pixel RCG parent\n",
				__func__);
			dp_drv->pixel_parent = NULL;
		}
	} else {
		if (dp_drv->pixel_parent)
			devm_clk_put(dev, dp_drv->pixel_parent);
		if (dp_drv->pixel_clk_rcg)
			devm_clk_put(dev, dp_drv->pixel_clk_rcg);
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
				dp->aux_sel_gpio, dp->aux_sel_gpio_output);
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
	struct mdss_panel_info *pinfo;
	u32 data = 0;

	timing = &dp->edid.timing[0];
	pinfo = &dp->panel_data.panel_info;

	cap = &dp->dpcd;

	data |= (2 << 13); /* Default-> LSCLK DIV: 1/4 LCLK  */

	/* Color Format */
	switch (dp->panel_data.panel_info.out_format) {
	case MDP_Y_CBCR_H2V2:
		data |= (1 << 11); /* YUV420 */
		break;
	case MDP_Y_CBCR_H2V1:
		data |= (2 << 11); /* YUV422 */
		break;
	default:
		data |= (0 << 11); /* RGB */
		break;
	}

	/* Scrambler reset enable */
	if (cap->scrambler_reset)
		data |= (1 << 10);

	/* Bits per components */
	data |= (mdss_dp_bpp_to_test_bit_depth(pinfo->bpp) << 8);

	/* Num of Lanes */
	data |= ((dp->lane_cnt - 1) << 4);

	if (cap->enhanced_frame)
		data |= 0x40;

	if (!timing->interlaced)	/* progressive */
		data |= 0x04;

	data |= 0x03;	/* sycn clock & static Mvid */

	mdss_dp_configuration_ctrl(&dp->ctrl_io, data);
}

static int mdss_dp_wait4video_ready(struct mdss_dp_drv_pdata *dp_drv)
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

static void mdss_dp_update_cable_status(struct mdss_dp_drv_pdata *dp,
		bool connected)
{
	mutex_lock(&dp->pd_msg_mutex);
	pr_debug("cable_connected to %d\n", connected);
	if (dp->cable_connected != connected)
		dp->cable_connected = connected;
	else
		pr_debug("no change in cable status\n");
	mutex_unlock(&dp->pd_msg_mutex);
}

static int dp_get_cable_status(struct platform_device *pdev, u32 vote)
{
	struct mdss_dp_drv_pdata *dp_ctrl = platform_get_drvdata(pdev);
	u32 hpd;

	if (!dp_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -ENODEV;
	}

	hpd = dp_ctrl->cable_connected;

	return hpd;
}

static bool mdss_dp_is_dvi_mode(struct mdss_dp_drv_pdata *dp)
{
	return hdmi_edid_is_dvi_mode(dp->panel_data.panel_info.edid_data);
}

static int dp_audio_info_setup(struct platform_device *pdev,
	struct msm_ext_disp_audio_setup_params *params)
{
	int rc = 0;
	struct mdss_dp_drv_pdata *dp_ctrl = platform_get_drvdata(pdev);

	if (!dp_ctrl || !params) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -ENODEV;
	}

	mdss_dp_audio_setup_sdps(&dp_ctrl->ctrl_io, params->num_of_channels);
	mdss_dp_config_audio_acr_ctrl(&dp_ctrl->ctrl_io, dp_ctrl->link_rate);
	mdss_dp_set_safe_to_exit_level(&dp_ctrl->ctrl_io, dp_ctrl->lane_cnt);
	mdss_dp_audio_enable(&dp_ctrl->ctrl_io, true);

	return rc;
} /* dp_audio_info_setup */

static int dp_get_audio_edid_blk(struct platform_device *pdev,
	struct msm_ext_disp_audio_edid_blk *blk)
{
	struct mdss_dp_drv_pdata *dp = platform_get_drvdata(pdev);
	int rc = 0;

	if (!dp) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -ENODEV;
	}

	rc = hdmi_edid_get_audio_blk
			(dp->panel_data.panel_info.edid_data, blk);
	if (rc)
		DEV_ERR("%s:edid_get_audio_blk failed\n", __func__);

	return rc;
} /* dp_get_audio_edid_blk */

static void dp_audio_teardown_done(struct platform_device *pdev)
{
	struct mdss_dp_drv_pdata *dp = platform_get_drvdata(pdev);

	if (!dp) {
		pr_err("invalid input\n");
		return;
	}

	mdss_dp_audio_enable(&dp->ctrl_io, false);
	/* Make sure the DP audio engine is disabled */
	wmb();

	pr_debug("audio engine disabled\n");
} /* dp_audio_teardown_done */

static int mdss_dp_init_ext_disp(struct mdss_dp_drv_pdata *dp)
{
	int ret = 0;
	struct device_node *pd_np;
	const char *phandle = "qcom,msm_ext_disp";

	if (!dp) {
		pr_err("%s: invalid input\n", __func__);
		ret = -ENODEV;
		goto end;
	}

	dp->ext_audio_data.type = EXT_DISPLAY_TYPE_DP;
	dp->ext_audio_data.kobj = dp->kobj;
	dp->ext_audio_data.pdev = dp->pdev;
	dp->ext_audio_data.codec_ops.audio_info_setup =
		dp_audio_info_setup;
	dp->ext_audio_data.codec_ops.get_audio_edid_blk =
		dp_get_audio_edid_blk;
	dp->ext_audio_data.codec_ops.cable_status =
		dp_get_cable_status;
	dp->ext_audio_data.codec_ops.teardown_done =
		dp_audio_teardown_done;

	if (!dp->pdev->dev.of_node) {
		pr_err("%s cannot find dp dev.of_node\n", __func__);
		ret = -ENODEV;
		goto end;
	}

	pd_np = of_parse_phandle(dp->pdev->dev.of_node, phandle, 0);
	if (!pd_np) {
		pr_err("%s cannot find %s dev\n", __func__, phandle);
		ret = -ENODEV;
		goto end;
	}

	dp->ext_pdev = of_find_device_by_node(pd_np);
	if (!dp->ext_pdev) {
		pr_err("%s cannot find %s pdev\n", __func__, phandle);
		ret = -ENODEV;
		goto end;
	}

	ret = msm_ext_disp_register_intf(dp->ext_pdev,
			&dp->ext_audio_data);
	if (ret)
		pr_err("%s: failed to register disp\n", __func__);

end:
	return ret;
}

static u32 mdss_dp_get_bpp(struct mdss_dp_drv_pdata *dp)
{
	u32 bpp;
	u32 bit_depth;

	/*
	 * Set bpp value based on whether a test video pattern is requested.
	 * For test pattern, the test data has the bit depth per color
	 * component. Otherwise, set it based on EDID.
	 */
	if (dp->override_config || mdss_dp_is_video_pattern_requested(dp))
		bit_depth = dp->test_data.test_bit_depth;
	else
		bit_depth = dp->edid.color_depth;

	if (!mdss_dp_is_test_bit_depth_valid(bit_depth)) {
		pr_debug("invalid bit_depth=%d. fall back to default\n",
			bit_depth);
		bit_depth = DP_TEST_BIT_DEPTH_8; /* default to 24bpp */
	}

	bpp = mdss_dp_test_bit_depth_to_bpp(bit_depth);
	return bpp;
}

static u32 mdss_dp_get_colorimetry_config(struct mdss_dp_drv_pdata *dp)
{
	u32 cc;
	enum dynamic_range dr;

	/* unless a video pattern CTS test is ongoing, use CEA_VESA */
	if (mdss_dp_is_video_pattern_requested(dp))
		dr = dp->test_data.test_dyn_range;
	else
		dr = DP_DYNAMIC_RANGE_RGB_VESA;

	/* Only RGB_VESA nd RGB_CEA supported for now */
	switch (dr) {
	case DP_DYNAMIC_RANGE_RGB_CEA:
		cc = BIT(3);
		break;
	case DP_DYNAMIC_RANGE_RGB_VESA:
	default:
		cc = 0;
	}

	return cc;
}

static int dp_init_panel_info(struct mdss_dp_drv_pdata *dp_drv, u32 vic)
{
	struct mdss_panel_info *pinfo;
	struct msm_hdmi_mode_timing_info timing = {0};
	u32 ret;

	if (!dp_drv) {
		DEV_ERR("invalid input\n");
		return -EINVAL;
	}
	pinfo = &dp_drv->panel_data.panel_info;

	if (vic != HDMI_VFRMT_UNKNOWN) {
		ret = hdmi_get_supported_mode(&timing, 0, vic);

		if (ret || !timing.supported || !pinfo) {
			DEV_ERR("%s: invalid timing data\n", __func__);
			return -EINVAL;
		}
	} else {
		pr_debug("reset panel info to zeroes\n");
	}

	dp_drv->vic = vic;
	pinfo->xres = timing.active_h;
	pinfo->yres = timing.active_v;
	pinfo->clk_rate = timing.pixel_freq * 1000;

	pinfo->lcdc.h_back_porch = timing.back_porch_h;
	pinfo->lcdc.h_front_porch = timing.front_porch_h;
	pinfo->lcdc.h_pulse_width = timing.pulse_width_h;
	pinfo->lcdc.h_active_low = timing.active_low_h;
	pinfo->lcdc.v_back_porch = timing.back_porch_v;
	pinfo->lcdc.v_front_porch = timing.front_porch_v;
	pinfo->lcdc.v_pulse_width = timing.pulse_width_v;
	pinfo->lcdc.v_active_low = timing.active_low_v;

	pinfo->type = DP_PANEL;
	pinfo->pdest = DISPLAY_4;
	pinfo->wait_cycle = 0;
	pinfo->fb_num = 1;

	pinfo->lcdc.border_clr = 0; /* blk */
	pinfo->lcdc.underflow_clr = 0xff; /* blue */
	pinfo->lcdc.hsync_skew = 0;
	pinfo->is_pluggable = true;

	pinfo->bpp = mdss_dp_get_bpp(dp_drv);
	pr_debug("bpp=%d\n", pinfo->bpp);
	pr_debug("update res. vic= %d, pclk_rate = %llu\n",
				dp_drv->vic, pinfo->clk_rate);

	return 0;
} /* dp_init_panel_info */

static inline void mdss_dp_ack_state(struct mdss_dp_drv_pdata *dp, int val)
{
	if (dp && dp->ext_audio_data.intf_ops.notify)
		dp->ext_audio_data.intf_ops.notify(dp->ext_pdev, val);
}

/**
 * mdss_dp_get_lane_mapping() - returns lane mapping based on given orientation
 * @orientation: usb plug orientation
 * @lane_map: the configured lane mapping
 *
 * Returns 0 when the lane mapping is successfully determined based on the
 * given usb plug orientation.
 */
static int mdss_dp_get_lane_mapping(struct mdss_dp_drv_pdata *dp,
		enum plug_orientation orientation, char *lane_map)
{
	int ret = 0, i = 0, j = 0;

	pr_debug("enter: orientation = %d\n", orientation);

	if (!lane_map) {
		pr_err("invalid lane map input");
		ret = -EINVAL;
		goto exit;
	}

	/* For flip case, swap phy lanes with ML0 and ML3, ML1 and ML2 */
	if (orientation == ORIENTATION_CC2) {
		for (i = 0; i < DP_MAX_PHY_LN; i++) {
			if (dp->l_map[i] == DP_ML0) {
				for (j = 0; j < DP_MAX_PHY_LN; j++) {
					if (dp->l_map[j] == DP_ML3) {
						lane_map[i] = DP_ML3;
						lane_map[j] = DP_ML0;
						break;
					}
				}
			} else if (dp->l_map[i] == DP_ML1) {
				for (j = 0; j < DP_MAX_PHY_LN; j++) {
					if (dp->l_map[j] == DP_ML2) {
						lane_map[i] = DP_ML2;
						lane_map[j] = DP_ML1;
						break;
					}
				}
			}
		}
	} else {
		/* Normal orientation */
		for (i = 0; i < DP_MAX_PHY_LN; i++)
			lane_map[i] = dp->l_map[i];
	}

	pr_debug("lane0 = %d, lane1 = %d, lane2 =%d, lane3 =%d\n",
		lane_map[0], lane_map[1], lane_map[2], lane_map[3]);

exit:
	return ret;
}

static void mdss_dp_set_clock_rate(struct mdss_dp_drv_pdata *dp,
		char *name, u32 rate)
{
	u32 num = dp->power_data[DP_CTRL_PM].num_clk;
	struct dss_clk *cfg = dp->power_data[DP_CTRL_PM].clk_config;

	while (num && strcmp(cfg->clk_name, name)) {
		num--;
		cfg++;
	}

	if (num)
		cfg->rate = rate;
	else
		pr_err("%s clock could not be set with rate %d\n", name, rate);
}

/**
 * mdss_dp_enable_mainlink_clocks() - enables Display Port main link clocks
 * @dp: Display Port Driver data
 *
 * Returns 0 when the main link clocks are successfully enabled.
 */
static int mdss_dp_enable_mainlink_clocks(struct mdss_dp_drv_pdata *dp)
{
	int ret = 0;

	if (dp->pixel_clk_rcg && dp->pixel_parent)
		clk_set_parent(dp->pixel_clk_rcg, dp->pixel_parent);

	mdss_dp_set_clock_rate(dp, "ctrl_link_clk",
		(dp->link_rate * DP_LINK_RATE_MULTIPLIER) / DP_KHZ_TO_HZ);

	mdss_dp_set_clock_rate(dp, "ctrl_crypto_clk", DP_CRYPTO_CLK_RATE_KHZ);

	dp->pixel_rate = dp->panel_data.panel_info.clk_rate;
	mdss_dp_set_clock_rate(dp, "ctrl_pixel_clk",
		(dp->pixel_rate / DP_KHZ_TO_HZ));

	ret = mdss_dp_clk_ctrl(dp, DP_CTRL_PM, true);
	if (ret) {
		pr_err("Unabled to start link clocks\n");
		ret = -EINVAL;
	}

	return ret;
}

/**
 * mdss_dp_disable_mainlink_clocks() - disables Display Port main link clocks
 * @dp: Display Port Driver data
 */
static void mdss_dp_disable_mainlink_clocks(struct mdss_dp_drv_pdata *dp_drv)
{
	mdss_dp_clk_ctrl(dp_drv, DP_CTRL_PM, false);
}

/**
 * mdss_dp_configure_source_params() - configures DP transmitter source params
 * @dp: Display Port Driver data
 * @lane_map: usb port lane mapping
 *
 * Configures the DP transmitter source params including details such as lane
 * configuration, output format and sink/panel timing information.
 */
static void mdss_dp_configure_source_params(struct mdss_dp_drv_pdata *dp,
		char *lane_map)
{
	mdss_dp_ctrl_lane_mapping(&dp->ctrl_io, lane_map);
	mdss_dp_fill_link_cfg(dp);
	mdss_dp_mainlink_ctrl(&dp->ctrl_io, true);
	mdss_dp_config_ctrl(dp);
	mdss_dp_config_misc(dp,
		mdss_dp_bpp_to_test_bit_depth(mdss_dp_get_bpp(dp)),
		mdss_dp_get_colorimetry_config(dp));
	mdss_dp_sw_config_msa(&dp->ctrl_io, dp->link_rate, &dp->dp_cc_io);
	mdss_dp_timing_cfg(&dp->ctrl_io, &dp->panel_data.panel_info);
}

/**
 * mdss_dp_setup_main_link() - initiates training of DP main link
 * @dp: Display Port Driver data
 * @train: specify if link training should be done or not
 *
 * Initiates training of the DP main link and checks the state of the main
 * link after the training is complete.
 *
 * Return: error code. -EINVAL if any invalid data or -EAGAIN if retraining
 * is required.
 */
static int mdss_dp_setup_main_link(struct mdss_dp_drv_pdata *dp, bool train)
{
	int ret = 0;
	int ready = 0;

	pr_debug("enter\n");
	mdss_dp_mainlink_ctrl(&dp->ctrl_io, true);
	mdss_dp_aux_set_sink_power_state(dp, SINK_POWER_ON);
	reinit_completion(&dp->video_comp);

	if (mdss_dp_is_phy_test_pattern_requested(dp))
		goto end;

	if (!train)
		goto send_video;

	/*
	 * As part of previous calls, DP controller state might have
	 * transitioned to PUSH_IDLE. In order to start transmitting a link
	 * training pattern, we have to first to a DP software reset.
	 */
	mdss_dp_ctrl_reset(&dp->ctrl_io);
	ret = mdss_dp_link_train(dp);
	if (ret)
		goto end;

send_video:
	/*
	 * Set up transfer unit values and set controller state to send
	 * video.
	 */
	mdss_dp_setup_tr_unit(&dp->ctrl_io, dp->link_rate, dp->lane_cnt,
		dp->vic, &dp->panel_data.panel_info);
	mdss_dp_state_ctrl(&dp->ctrl_io, ST_SEND_VIDEO);

	mdss_dp_wait4video_ready(dp);
	ready = mdss_dp_mainlink_ready(dp, BIT(0));
	pr_debug("main link %s\n", ready ? "READY" : "NOT READY");

end:
	return ret;
}

static int mdss_dp_on_irq(struct mdss_dp_drv_pdata *dp_drv, bool lt_needed)
{
	int ret = 0;
	char ln_map[4];

	/* wait until link training is completed */
	pr_debug("enter, lt_needed=%s\n", lt_needed ? "true" : "false");

	do {
		if (ret == -EAGAIN)
			mdss_dp_mainlink_ctrl(&dp_drv->ctrl_io, false);

		mutex_lock(&dp_drv->train_mutex);

		dp_init_panel_info(dp_drv, dp_drv->vic);
		ret = mdss_dp_get_lane_mapping(dp_drv, dp_drv->orientation,
				ln_map);
		if (ret)
			goto exit_loop;

		mdss_dp_phy_share_lane_config(&dp_drv->phy_io,
				dp_drv->orientation,
				dp_drv->dpcd.max_lane_count,
				dp_drv->phy_reg_offset);

		if (lt_needed) {
			/*
			 * Diasable and re-enable the mainlink clock since the
			 * link clock might have been adjusted as part of the
			 * link maintenance.
			 */
			if (!mdss_dp_is_phy_test_pattern_requested(dp_drv))
				mdss_dp_disable_mainlink_clocks(dp_drv);
			ret = mdss_dp_enable_mainlink_clocks(dp_drv);
			if (ret)
				goto exit_loop;
		}

		mdss_dp_configure_source_params(dp_drv, ln_map);

		reinit_completion(&dp_drv->idle_comp);

		dp_drv->power_on = true;

		if (dp_drv->psm_enabled) {
			ret = mdss_dp_aux_send_psm_request(dp_drv, false);
			if (ret) {
				pr_err("Failed to exit low power mode, rc=%d\n",
					ret);
				goto exit_loop;
			}
		}

		ret = mdss_dp_setup_main_link(dp_drv, lt_needed);

exit_loop:
		mutex_unlock(&dp_drv->train_mutex);
	} while (ret == -EAGAIN);

	pr_debug("end\n");

	/* Send a connect notification */
	if (!mdss_dp_is_phy_test_pattern_requested(dp_drv))
		mdss_dp_notify_clients(dp_drv, NOTIFY_CONNECT_IRQ_HPD);

	return ret;
}

int mdss_dp_on_hpd(struct mdss_dp_drv_pdata *dp_drv)
{
	int ret = 0;
	char ln_map[4];

	/* wait until link training is completed */
	mutex_lock(&dp_drv->train_mutex);

	pr_debug("Enter++ cont_splash=%d\n", dp_drv->cont_splash);

	if (dp_drv->cont_splash) {
		mdss_dp_aux_ctrl(&dp_drv->ctrl_io, true);
		goto link_training;
	}

	ret = mdss_dp_clk_ctrl(dp_drv, DP_CORE_PM, true);
	if (ret) {
		pr_err("Unabled to start core clocks\n");
		goto exit;
	}
	mdss_dp_hpd_configure(&dp_drv->ctrl_io, true);

	ret = mdss_dp_get_lane_mapping(dp_drv, dp_drv->orientation, ln_map);
	if (ret)
		goto exit;

	dp_init_panel_info(dp_drv, dp_drv->new_vic);

	dp_drv->link_rate = mdss_dp_gen_link_clk(dp_drv);
	if (!dp_drv->link_rate) {
		pr_err("Unable to configure required link rate\n");
		ret = -EINVAL;
		goto exit;
	}

	mdss_dp_phy_share_lane_config(&dp_drv->phy_io, dp_drv->orientation,
			dp_drv->dpcd.max_lane_count, dp_drv->phy_reg_offset);

	ret = mdss_dp_enable_mainlink_clocks(dp_drv);
	if (ret)
		goto exit;

	reinit_completion(&dp_drv->idle_comp);

	mdss_dp_configure_source_params(dp_drv, ln_map);

	if (dp_drv->psm_enabled) {
		ret = mdss_dp_aux_send_psm_request(dp_drv, false);
		if (ret) {
			pr_err("Failed to exit low power mode, rc=%d\n", ret);
			goto exit;
		}
	}


link_training:
	dp_drv->power_on = true;

	while (-EAGAIN == mdss_dp_setup_main_link(dp_drv, true))
		pr_debug("MAIN LINK TRAINING RETRY\n");

	dp_drv->cont_splash = 0;

	dp_drv->power_on = true;
	pr_debug("End-\n");

exit:
	mutex_unlock(&dp_drv->train_mutex);
	return ret;
}

int mdss_dp_on(struct mdss_panel_data *pdata)
{
	struct mdss_dp_drv_pdata *dp_drv = NULL;

	if (!pdata) {
		pr_err("Invalid input data\n");
		return -EINVAL;
	}

	dp_drv = container_of(pdata, struct mdss_dp_drv_pdata,
			panel_data);

	if (dp_drv->power_on) {
		pr_debug("Link already setup, return\n");
		return 0;
	}

	return mdss_dp_on_hpd(dp_drv);
}

static int mdss_dp_off_irq(struct mdss_dp_drv_pdata *dp_drv)
{
	if (!dp_drv->power_on) {
		pr_debug("panel already powered off\n");
		return 0;
	}

	/* wait until link training is completed */
	mutex_lock(&dp_drv->train_mutex);

	pr_debug("start\n");

	mdss_dp_mainlink_ctrl(&dp_drv->ctrl_io, false);
	mdss_dp_audio_enable(&dp_drv->ctrl_io, false);
	/* Make sure DP mainlink and audio engines are disabled */
	wmb();

	mdss_dp_ack_state(dp_drv, false);
	mutex_unlock(&dp_drv->train_mutex);

	complete_all(&dp_drv->irq_comp);
	pr_debug("end\n");

	return 0;
}

static int mdss_dp_off_hpd(struct mdss_dp_drv_pdata *dp_drv)
{
	if (!dp_drv->power_on) {
		pr_debug("panel already powered off\n");
		return 0;
	}

	/* wait until link training is completed */
	mutex_lock(&dp_drv->train_mutex);

	pr_debug("Entered++, cont_splash=%d\n", dp_drv->cont_splash);

	mdss_dp_mainlink_ctrl(&dp_drv->ctrl_io, false);

	mdss_dp_aux_ctrl(&dp_drv->ctrl_io, false);

	mdss_dp_audio_enable(&dp_drv->ctrl_io, false);

	mdss_dp_irq_disable(dp_drv);

	mdss_dp_config_gpios(dp_drv, false);
	mdss_dp_pinctrl_set_state(dp_drv, false);

	/*
	* The global reset will need DP link ralated clocks to be
	* running. Add the global reset just before disabling the
	* link clocks and core clocks.
	*/
	mdss_dp_ctrl_reset(&dp_drv->ctrl_io);

	/* Make sure DP is disabled before clk disable */
	wmb();
	mdss_dp_disable_mainlink_clocks(dp_drv);
	mdss_dp_clk_ctrl(dp_drv, DP_CORE_PM, false);

	mdss_dp_regulator_ctrl(dp_drv, false);
	dp_drv->dp_initialized = false;

	dp_drv->power_on = false;
	dp_drv->sink_info_read = false;
	dp_init_panel_info(dp_drv, HDMI_VFRMT_UNKNOWN);

	mdss_dp_ack_state(dp_drv, false);
	mdss_dp_reset_test_data(dp_drv);
	mutex_unlock(&dp_drv->train_mutex);
	pr_debug("DP off done\n");

	return 0;
}

int mdss_dp_off(struct mdss_panel_data *pdata)
{
	struct mdss_dp_drv_pdata *dp = NULL;

	dp = container_of(pdata, struct mdss_dp_drv_pdata,
				panel_data);
	if (!dp) {
		pr_err("Invalid input data\n");
		return -EINVAL;
	}

	if (dp->hpd_irq_on)
		return mdss_dp_off_irq(dp);
	else
		return mdss_dp_off_hpd(dp);
}

static int mdss_dp_send_cable_notification(
	struct mdss_dp_drv_pdata *dp, int val)
{
	int ret = 0;
	u32 flags = 0;

	if (!dp) {
		DEV_ERR("%s: invalid input\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	flags |= MSM_EXT_DISP_HPD_VIDEO;

	if (!mdss_dp_is_dvi_mode(dp) || dp->audio_test_req) {
		dp->audio_test_req = false;

		flags |= MSM_EXT_DISP_HPD_AUDIO;
	}

	if (dp->ext_audio_data.intf_ops.hpd)
		ret = dp->ext_audio_data.intf_ops.hpd(dp->ext_pdev,
				dp->ext_audio_data.type, val, flags);

end:
	return ret;
}

static void mdss_dp_set_default_resolution(struct mdss_dp_drv_pdata *dp)
{
	hdmi_edid_set_video_resolution(dp->panel_data.panel_info.edid_data,
			DEFAULT_VIDEO_RESOLUTION, true);
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

	edid_init_data.kobj = dp_drv->kobj;
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

	mdss_dp_set_default_resolution(dp_drv);

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

	if (dp_drv->dp_initialized) {
		pr_debug("%s: host init done already\n", __func__);
		return 0;
	}

	ret = mdss_dp_regulator_ctrl(dp_drv, true);
	if (ret) {
		pr_err("failed to enable regulators\n");
		goto vreg_error;
	}

	dp_drv->orientation = usbpd_get_plug_orientation(dp_drv->pd);

	dp_drv->aux_sel_gpio_output = 0;
	if (dp_drv->orientation == ORIENTATION_CC2)
		dp_drv->aux_sel_gpio_output = 1;

	pr_debug("orientation = %d, aux_sel_gpio_output = %d\n",
			dp_drv->orientation, dp_drv->aux_sel_gpio_output);

	mdss_dp_pinctrl_set_state(dp_drv, true);
	mdss_dp_config_gpios(dp_drv, true);

	ret = mdss_dp_clk_ctrl(dp_drv, DP_CORE_PM, true);
	if (ret) {
		pr_err("Unabled to start core clocks\n");
		goto clk_error;
	}

	mdss_dp_aux_init(dp_drv);

	mdss_dp_phy_initialize(dp_drv);
	mdss_dp_ctrl_reset(&dp_drv->ctrl_io);
	mdss_dp_phy_reset(&dp_drv->ctrl_io);
	mdss_dp_aux_reset(&dp_drv->ctrl_io);
	mdss_dp_aux_ctrl(&dp_drv->ctrl_io, true);

	pr_debug("Ctrl_hw_rev =0x%x, phy hw_rev =0x%x\n",
	       mdss_dp_get_ctrl_hw_version(&dp_drv->ctrl_io),
	       mdss_dp_get_phy_hw_version(&dp_drv->phy_io));

	mdss_dp_phy_aux_setup(&dp_drv->phy_io, dp_drv->aux_cfg,
			dp_drv->phy_reg_offset);

	mdss_dp_irq_enable(dp_drv);
	dp_drv->dp_initialized = true;

	return 0;

clk_error:
	mdss_dp_regulator_ctrl(dp_drv, false);
	mdss_dp_config_gpios(dp_drv, false);
vreg_error:
	return ret;
}

/**
 * mdss_dp_notify_clients() - notifies DP clients of cable connection
 * @dp: Display Port Driver data
 * @status: HPD notification status requested
 *
 * This function will send a notification to display/audio clients of change
 * in DP connection status.
 */
static int mdss_dp_notify_clients(struct mdss_dp_drv_pdata *dp,
	enum notification_status status)
{
	const int irq_comp_timeout = HZ * 2;
	int ret = 0;

	mutex_lock(&dp->pd_msg_mutex);
	if (status == dp->hpd_notification_status) {
		pr_debug("No change in status %s --> %s\n",
			mdss_dp_notification_status_to_string(status),
			mdss_dp_notification_status_to_string(
				dp->hpd_notification_status));
		goto end;
	}

	switch (status) {
	case NOTIFY_CONNECT_IRQ_HPD:
		if (dp->hpd_notification_status != NOTIFY_DISCONNECT_IRQ_HPD)
			goto invalid_request;
		/* Follow the same programming as for NOTIFY_CONNECT */
		mdss_dp_host_init(&dp->panel_data);
		mdss_dp_send_cable_notification(dp, true);
		break;
	case NOTIFY_CONNECT:
		if ((dp->hpd_notification_status == NOTIFY_CONNECT_IRQ_HPD) ||
			(dp->hpd_notification_status ==
			 NOTIFY_DISCONNECT_IRQ_HPD))
			goto invalid_request;
		mdss_dp_host_init(&dp->panel_data);
		mdss_dp_send_cable_notification(dp, true);
		break;
	case NOTIFY_DISCONNECT:
		mdss_dp_send_cable_notification(dp, false);
		break;
	case NOTIFY_DISCONNECT_IRQ_HPD:
		if (dp->hpd_notification_status == NOTIFY_DISCONNECT)
			goto invalid_request;

		mdss_dp_send_cable_notification(dp, false);
		if (!IS_ERR_VALUE(ret) && ret) {
			reinit_completion(&dp->irq_comp);
			ret = wait_for_completion_timeout(&dp->irq_comp,
					irq_comp_timeout);
			if (ret <= 0) {
				pr_warn("irq_comp timed out\n");
				ret = -EINVAL;
			} else {
				ret = 0;
			}
		}
		break;
	default:
		pr_err("Invalid notification status = %d\n", status);
		ret = -EINVAL;
		break;
	}

	goto end;

invalid_request:
	pr_err("Invalid request %s --> %s\n",
		mdss_dp_notification_status_to_string(
			dp->hpd_notification_status),
		mdss_dp_notification_status_to_string(status));
	ret = -EINVAL;

end:
	if (!ret) {
		pr_debug("Successfully sent notification %s --> %s\n",
			mdss_dp_notification_status_to_string(
				dp->hpd_notification_status),
			mdss_dp_notification_status_to_string(status));
		dp->hpd_notification_status = status;
	}

	mutex_unlock(&dp->pd_msg_mutex);
	return ret;
}

static int mdss_dp_process_hpd_high(struct mdss_dp_drv_pdata *dp)
{
	int ret;

	if (dp->sink_info_read)
		return 0;

	pr_debug("start\n");

	mdss_dp_dpcd_cap_read(dp);

	ret = mdss_dp_edid_read(dp);
	if (ret) {
		pr_debug("edid read error, setting default resolution\n");

		mdss_dp_set_default_resolution(dp);
		goto notify;
	}

	ret = hdmi_edid_parser(dp->panel_data.panel_info.edid_data);
	if (ret) {
		pr_err("edid parse failed\n");
		goto notify;
	}

	dp->sink_info_read = true;

notify:
	/* Check if there is a PHY_TEST_PATTERN request when we get HPD high.
	 * Update the DP driver with the test parameters including link rate,
	 * lane count, voltage level, and pre-emphasis level. Do not notify
	 * the userspace of the connection, just power on the DP controller
	 * and mainlink with the new settings.
	 */
	if (mdss_dp_is_phy_test_pattern_requested(dp)) {
		pr_info("PHY_TEST_PATTERN requested by sink\n");
		mdss_dp_process_phy_test_pattern_request(dp);
		pr_info("skip client notification\n");
		goto end;
	}

	mdss_dp_notify_clients(dp, NOTIFY_CONNECT);

end:
	pr_debug("end\n");
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

	new_vic = hdmi_panel_get_vic(var_pinfo, 0);

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

static inline bool dp_is_hdcp_enabled(struct mdss_dp_drv_pdata *dp_drv)
{
	return dp_drv->hdcp.feature_enabled &&
		(dp_drv->hdcp.hdcp1_present || dp_drv->hdcp.hdcp2_present) &&
		dp_drv->hdcp.ops;
}

static void mdss_dp_hdcp_cb_work(struct work_struct *work)
{
	struct mdss_dp_drv_pdata *dp;
	struct delayed_work *dw = to_delayed_work(work);
	struct hdcp_ops *ops;
	unsigned char *base;
	int rc = 0;
	u32 hdcp_auth_state;

	dp = container_of(dw, struct mdss_dp_drv_pdata, hdcp_cb_work);
	base = dp->base;


	if (dp->hdcp_status == HDCP_STATE_AUTHENTICATING &&
	    mdss_dp_is_audio_pattern_requested(dp)) {
		pr_debug("no hdcp for audio tests\n");
		return;
	}

	hdcp_auth_state = (dp_read(base + DP_HDCP_STATUS) >> 20) & 0x3;

	pr_debug("hdcp auth state %d\n", hdcp_auth_state);

	ops = dp->hdcp.ops;

	switch (dp->hdcp_status) {
	case HDCP_STATE_AUTHENTICATING:
		pr_debug("start authenticaton\n");

		dp->dpcd_version = dp->dpcd.minor | (dp->dpcd.major << 8);

		if (dp->hdcp.ops && dp->hdcp.ops->authenticate)
			rc = dp->hdcp.ops->authenticate(dp->hdcp.data);

		break;
	case HDCP_STATE_AUTHENTICATED:
		pr_debug("hdcp authenticated\n");
		dp->hdcp.auth_state = true;
		break;
	case HDCP_STATE_AUTH_FAIL:
		dp->hdcp.auth_state = false;

		if (dp->alt_mode.dp_status.hpd_high && dp->power_on) {
			pr_debug("Reauthenticating\n");
			if (ops && ops->reauthenticate) {
				rc = ops->reauthenticate(dp->hdcp.data);
				if (rc)
					pr_err("reauth failed rc=%d\n", rc);
			}
		} else {
			pr_debug("not reauthenticating, cable disconnected\n");
		}

		break;
	default:
		break;
	}
}

static void mdss_dp_hdcp_cb(void *ptr, enum hdcp_states status)
{
	struct mdss_dp_drv_pdata *dp = ptr;

	if (!dp) {
		pr_err("invalid input\n");
		return;
	}

	dp->hdcp_status = status;

	if (dp->alt_mode.dp_status.hpd_high)
		queue_delayed_work(dp->workq, &dp->hdcp_cb_work, HZ/4);
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
	hdcp_init_data.version       = &dp_drv->dpcd_version;

	dp_drv->hdcp.hdcp1 = hdcp_1x_init(&hdcp_init_data);
	if (IS_ERR_OR_NULL(dp_drv->hdcp.hdcp1)) {
		pr_err("Error hdcp init\n");
		rc = -EINVAL;
		goto error;
	}

	dp_drv->panel_data.panel_info.hdcp_1x_data = dp_drv->hdcp.hdcp1;

	pr_debug("HDCP 1.3 initialized\n");

	dp_drv->hdcp.hdcp2 = dp_hdcp2p2_init(&hdcp_init_data);
	if (!IS_ERR_OR_NULL(dp_drv->hdcp.hdcp2))
		pr_debug("HDCP 2.2 initialized\n");

	dp_drv->hdcp.feature_enabled = true;
	return 0;
error:
	return rc;
}

static struct mdss_dp_drv_pdata *mdss_dp_get_drvdata(struct device *device)
{
	struct msm_fb_data_type *mfd;
	struct mdss_panel_data *pd;
	struct mdss_dp_drv_pdata *dp = NULL;
	struct fb_info *fbi = dev_get_drvdata(device);

	if (fbi) {
		mfd = (struct msm_fb_data_type *)fbi->par;
		pd = dev_get_platdata(&mfd->pdev->dev);

		dp = container_of(pd, struct mdss_dp_drv_pdata, panel_data);
	}

	return dp;
}

static ssize_t mdss_dp_rda_connected(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct mdss_dp_drv_pdata *dp = mdss_dp_get_drvdata(dev);

	if (!dp)
		return -EINVAL;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", dp->cable_connected);
	pr_debug("%d\n", dp->cable_connected);

	return ret;
}

static ssize_t mdss_dp_sysfs_wta_s3d_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, s3d_mode;
	struct mdss_dp_drv_pdata *dp = mdss_dp_get_drvdata(dev);

	if (!dp) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	ret = kstrtoint(buf, 10, &s3d_mode);
	if (ret) {
		DEV_ERR("%s: kstrtoint failed. rc=%d\n", __func__, ret);
		goto end;
	}

	dp->s3d_mode = s3d_mode;
	ret = strnlen(buf, PAGE_SIZE);
	DEV_DBG("%s: %d\n", __func__, dp->s3d_mode);
end:
	return ret;
}

static ssize_t mdss_dp_sysfs_rda_s3d_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct mdss_dp_drv_pdata *dp = mdss_dp_get_drvdata(dev);

	if (!dp) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "%d\n", dp->s3d_mode);
	DEV_DBG("%s: '%d'\n", __func__, dp->s3d_mode);

	return ret;
}

static bool mdss_dp_is_test_ongoing(struct mdss_dp_drv_pdata *dp)
{
	return (dp->hpd_notification_status == NOTIFY_DISCONNECT_IRQ_HPD);
}

/**
 * mdss_dp_psm_config() - Downstream device uPacket RX Power Management
 * @dp: Display Port Driver data
 *
 * Perform required steps to configure the uPacket RX of a downstream
 * connected device in a power-save mode.
 */
static int mdss_dp_psm_config(struct mdss_dp_drv_pdata *dp, bool enable)
{
	int ret = 0;

	if (!dp) {
		pr_err("invalid data\n");
		return -EINVAL;
	}

	if (dp->psm_enabled == enable) {
		pr_debug("No change in psm requested\n");
		goto end;
	}

	pr_debug("Power save mode %s requested\n", enable ? "entry" : "exit");

	if (enable) {
		ret = mdss_dp_aux_send_psm_request(dp, true);
		if (ret)
			goto end;

		/*
		 * If this configuration is requested as part of an
		 * automated test, then HPD notification has already been
		 * sent out. Just disable the main-link and turn off DP Tx.
		 *
		 * Otherwise, trigger a complete shutdown of the pipeline.
		 */
		if (mdss_dp_is_test_ongoing(dp)) {
			mdss_dp_mainlink_push_idle(&dp->panel_data);
			mdss_dp_off_irq(dp);
		} else {
			mdss_dp_notify_clients(dp, NOTIFY_DISCONNECT);
		}
	} else {
		/*
		 * If this configuration is requested as part of an
		 * automated test, then just perform a link retraining.
		 *
		 * Otherwise, re-initialize the host and setup the complete
		 * pipeline from scratch by sending a connection notification
		 * to user modules.
		 */
		if (mdss_dp_is_test_ongoing(dp)) {
			mdss_dp_link_maintenance(dp, true);
		} else {
			mdss_dp_host_init(&dp->panel_data);
			mdss_dp_notify_clients(dp, NOTIFY_CONNECT);
		}
	}

end:
	pr_debug("Power save mode %s %s\n",
		dp->psm_enabled ? "entry" : "exit",
		ret ? "failed" : "successful");

	return ret;
}

static ssize_t mdss_dp_wta_psm(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int psm;
	int rc;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct mdss_dp_drv_pdata *dp = mdss_dp_get_drvdata(dev);

	if (!dp) {
		pr_err("invalid data\n");
		ret = -EINVAL;
		goto end;
	}

	rc = kstrtoint(buf, 10, &psm);
	if (rc) {
		pr_err("kstrtoint failed. ret=%d\n", (int)ret);
		goto end;
	}

	rc = mdss_dp_psm_config(dp, psm ? true : false);
	if (rc) {
		pr_err("failed to config Power Save Mode\n");
		goto end;
	}

end:
	return ret;
}

static ssize_t mdss_dp_rda_psm(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct mdss_dp_drv_pdata *dp = mdss_dp_get_drvdata(dev);

	if (!dp) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "%d\n", dp->psm_enabled ? 1 : 0);
	pr_debug("psm: %s\n", dp->psm_enabled ? "enabled" : "disabled");

	return ret;
}

static ssize_t mdss_dp_wta_hpd(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int hpd, rc;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct mdss_dp_drv_pdata *dp = mdss_dp_get_drvdata(dev);

	if (!dp) {
		pr_err("invalid data\n");
		ret = -EINVAL;
		goto end;
	}

	rc = kstrtoint(buf, 10, &hpd);
	if (rc) {
		pr_err("kstrtoint failed. ret=%d\n", rc);
		ret = rc;
		goto end;
	}

	dp->hpd = !!hpd;
	pr_debug("hpd=%d\n", dp->hpd);

	if (dp->hpd && dp->cable_connected) {
		if (dp->alt_mode.current_state & DP_CONFIGURE_DONE) {
			mdss_dp_host_init(&dp->panel_data);
			mdss_dp_process_hpd_high(dp);
		} else {
			dp_send_events(dp, EV_USBPD_DISCOVER_MODES);
		}
	} else if (!dp->hpd && dp->power_on) {
		mdss_dp_notify_clients(dp, NOTIFY_DISCONNECT);
	}
end:
	return ret;
}

static ssize_t mdss_dp_rda_hpd(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct mdss_dp_drv_pdata *dp = mdss_dp_get_drvdata(dev);

	if (!dp) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "%d\n", dp->hpd);
	pr_debug("hpd: %d\n", dp->hpd);

	return ret;
}

static int mdss_dp_parse_config_value(char const *buf, char const *name,
	u32 *val)
{
	int ret = 0;
	char *buf1;
	char *token;

	buf1 = strnstr(buf, name, PAGE_SIZE);
	if (buf1) {
		buf1 = buf1 + strlen(name);
		token = strsep(&buf1, " ");
		if (!token) {
			pr_err("strsep failed\n");
			ret = -EINVAL;
			goto end;
		}
		ret = kstrtou32(token, 10, val);
		if (ret) {
			pr_err("kstrtoint failed. ret=%d\n", (int)ret);
			goto end;
		}
		pr_debug("parsed %s(%d)\n", name, *val);
	} else {
		ret = -EINVAL;
	}

end:
	return ret;
}

static ssize_t mdss_dp_wta_config(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val;
	u32 bit_depth;
	int ret;
	char const *bpp_key = "bpp=";
	char const *pattern_type_key = "pattern_type=";
	struct mdss_dp_drv_pdata *dp = mdss_dp_get_drvdata(dev);

	if (!dp) {
		pr_err("invalid data\n");
		ret = -EINVAL;
		goto end;
	}

	ret = mdss_dp_parse_config_value(buf, bpp_key, &val);
	if (ret) {
		pr_debug("%s config not found\n", bpp_key);
		goto pattern_type;
	}

	bit_depth = mdss_dp_bpp_to_test_bit_depth(val);
	if (!mdss_dp_is_test_bit_depth_valid(bit_depth)) {
		pr_err("invalid bpp = %d\n", val);
	} else {
		dp->test_data.test_bit_depth = bit_depth;
		if (val != 0)
			dp->override_config = true;
		else
			dp->override_config = false;
		pr_debug("bpp=%d, test_bit_depth=%d\n", val,
			dp->test_data.test_bit_depth);
	}

pattern_type:
	ret = mdss_dp_parse_config_value(buf, pattern_type_key, &val);
	if (ret) {
		pr_debug("%s config not found\n", pattern_type_key);
		goto end;
	}

	if (!mdss_dp_is_test_video_pattern_valid(val)) {
		pr_err("invalid test video pattern = %d\n", val);
	} else {
		dp->test_data.test_video_pattern = val;
		pr_debug("test_video_pattern=%d (%s)\n",
			dp->test_data.test_video_pattern,
			mdss_dp_test_video_pattern_to_string(
				dp->test_data.test_video_pattern));
	}

end:
	return count;
}

static ssize_t mdss_dp_rda_config(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	u32 bpp;
	struct mdss_dp_drv_pdata *dp = mdss_dp_get_drvdata(dev);

	if (!dp) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	bpp = mdss_dp_get_bpp(dp);
	ret = snprintf(buf, PAGE_SIZE, "bpp=%d\npattern_type=%d\n",
		bpp, dp->test_data.test_video_pattern);

	pr_debug("bpp: %d pattern_type=%d (%s)\n",
		bpp, dp->test_data.test_video_pattern,
		mdss_dp_test_video_pattern_to_string(
			dp->test_data.test_video_pattern));

	return ret;
}

static ssize_t mdss_dp_wta_frame_crc(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	u32 val;
	struct mdss_dp_drv_pdata *dp = mdss_dp_get_drvdata(dev);
	char const *ctl_crc_key = "ctl_crc_en=";
	char const *sink_crc_key = "sink_crc_en=";
	bool ctl_crc_en, sink_crc_en;

	if (!dp) {
		pr_err("invalid data\n");
		goto end;
	}

	if (!dp->power_on) {
		pr_err("DP controller not powered on\n");
		goto end;
	}

	ret = mdss_dp_parse_config_value(buf, ctl_crc_key, &val);
	if (ret) {
		pr_debug("%s config not found\n", ctl_crc_key);
		goto sink_crc;
	}
	ctl_crc_en = val ? true : false;
	mdss_dp_config_ctl_frame_crc(dp, ctl_crc_en);

sink_crc:
	ret = mdss_dp_parse_config_value(buf, sink_crc_key, &val);
	if (ret) {
		pr_debug("%s config not found\n", sink_crc_key);
		goto end;
	}
	sink_crc_en = val ? true : false;
	mdss_dp_aux_config_sink_frame_crc(dp, sink_crc_en);

end:
	return count;
}

static ssize_t mdss_dp_print_crc_values(struct mdss_dp_drv_pdata *dp,
	char *buf, ssize_t len)
{
	char line[] = "------------------------------";

	mdss_dp_read_ctl_frame_crc(dp);
	mdss_dp_aux_read_sink_frame_crc(dp);

	return snprintf(buf, PAGE_SIZE,
		"\t\t|R_Cr\t\t|G_y\t\t|B_Cb\n%s%s\nctl(%s)\t|0x%08x\t|0x%08x\t|0x%08x\nsink(%s)\t|0x%08x\t|0x%08x\t|0x%08x\n",
		line, line, dp->ctl_crc.en ? "enabled" : "disabled",
		dp->ctl_crc.r_cr, dp->ctl_crc.g_y, dp->ctl_crc.b_cb,
		dp->sink_crc.en ? "enabled" : "disabled",
		dp->sink_crc.r_cr, dp->sink_crc.g_y, dp->sink_crc.b_cb);
}

static ssize_t mdss_dp_rda_frame_crc(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct mdss_dp_drv_pdata *dp = mdss_dp_get_drvdata(dev);

	if (!dp) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	if (!dp->power_on) {
		pr_err("DP controller not powered on\n");
		return 0;
	}

	ret = mdss_dp_print_crc_values(dp, buf, PAGE_SIZE);

	return ret;
}

static ssize_t mdss_dp_wta_hdcp_feature(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	u32 hdcp;
	int rc;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct mdss_dp_drv_pdata *dp = mdss_dp_get_drvdata(dev);

	if (!dp) {
		pr_err("invalid data\n");
		ret = -EINVAL;
		goto end;
	}

	rc = kstrtoint(buf, 10, &hdcp);
	if (rc) {
		pr_err("kstrtoint failed. ret=%d\n", rc);
		ret = rc;
		goto end;
	}

	dp->hdcp.feature_enabled = !!hdcp;
	pr_debug("hdcp=%d\n", dp->hdcp.feature_enabled);
end:
	return ret;
}

static ssize_t mdss_dp_rda_hdcp_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct mdss_dp_drv_pdata *dp = mdss_dp_get_drvdata(dev);

	if (!dp) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "%d\n", dp->hdcp.feature_enabled);
	pr_debug("hdcp: %d\n", dp->hdcp.feature_enabled);

	return ret;
}

static DEVICE_ATTR(connected, S_IRUGO, mdss_dp_rda_connected, NULL);
static DEVICE_ATTR(s3d_mode, S_IRUGO | S_IWUSR, mdss_dp_sysfs_rda_s3d_mode,
	mdss_dp_sysfs_wta_s3d_mode);
static DEVICE_ATTR(hpd, S_IRUGO | S_IWUSR, mdss_dp_rda_hpd,
	mdss_dp_wta_hpd);
static DEVICE_ATTR(psm, S_IRUGO | S_IWUSR, mdss_dp_rda_psm,
	mdss_dp_wta_psm);
static DEVICE_ATTR(config, S_IRUGO | S_IWUSR, mdss_dp_rda_config,
	mdss_dp_wta_config);
static DEVICE_ATTR(frame_crc, S_IRUGO | S_IWUSR, mdss_dp_rda_frame_crc,
	mdss_dp_wta_frame_crc);
static DEVICE_ATTR(hdcp_feature, S_IRUGO | S_IWUSR, mdss_dp_rda_hdcp_feature,
	mdss_dp_wta_hdcp_feature);

static struct attribute *mdss_dp_fs_attrs[] = {
	&dev_attr_connected.attr,
	&dev_attr_s3d_mode.attr,
	&dev_attr_hpd.attr,
	&dev_attr_psm.attr,
	&dev_attr_config.attr,
	&dev_attr_frame_crc.attr,
	&dev_attr_hdcp_feature.attr,
	NULL,
};

static struct attribute_group mdss_dp_fs_attrs_group = {
	.attrs = mdss_dp_fs_attrs,
};

static int mdss_dp_sysfs_create(struct mdss_dp_drv_pdata *dp,
	struct fb_info *fbi)
{
	int rc;

	if (!dp || !fbi) {
		pr_err("ivalid input\n");
		return -ENODEV;
	}

	rc = sysfs_create_group(&fbi->dev->kobj,
		&mdss_dp_fs_attrs_group);
	if (rc) {
		pr_err("failed, rc=%d\n", rc);
		return rc;
	}

	pr_debug("sysfs ceated\n");

	return 0;
}

static void mdss_dp_mainlink_push_idle(struct mdss_panel_data *pdata)
{
	struct mdss_dp_drv_pdata *dp_drv = NULL;
	const int idle_pattern_completion_timeout_ms = 3 * HZ / 100;

	dp_drv = container_of(pdata, struct mdss_dp_drv_pdata,
				panel_data);
	if (!dp_drv) {
		pr_err("Invalid input data\n");
		return;
	}
	pr_debug("Entered++\n");

	/* wait until link training is completed */
	mutex_lock(&dp_drv->train_mutex);

	mdss_dp_aux_set_sink_power_state(dp_drv, SINK_POWER_OFF);

	reinit_completion(&dp_drv->idle_comp);
	mdss_dp_state_ctrl(&dp_drv->ctrl_io, ST_PUSH_IDLE);
	if (!wait_for_completion_timeout(&dp_drv->idle_comp,
			idle_pattern_completion_timeout_ms))
		pr_warn("PUSH_IDLE pattern timedout\n");

	mutex_unlock(&dp_drv->train_mutex);
	pr_debug("mainlink off done\n");
}

static void mdss_dp_update_hdcp_info(struct mdss_dp_drv_pdata *dp)
{
	void *fd = NULL;
	struct hdcp_ops *ops = NULL;

	if (!dp) {
		pr_err("invalid input\n");
		return;
	}

	if (!dp->hdcp.feature_enabled) {
		pr_debug("feature not enabled\n");
		return;
	}

	/* check first if hdcp2p2 is supported */
	fd = dp->hdcp.hdcp2;
	if (fd)
		ops = dp_hdcp2p2_start(fd);

	if (ops && ops->feature_supported)
		dp->hdcp.hdcp2_present = ops->feature_supported(fd);
	else
		dp->hdcp.hdcp2_present = false;

	if (!dp->hdcp.hdcp2_present) {
		dp->hdcp.hdcp1_present = hdcp1_check_if_supported_load_app();

		if (dp->hdcp.hdcp1_present) {
			fd = dp->hdcp.hdcp1;
			ops = hdcp_1x_start(fd);
		}
	}

	/* update internal data about hdcp */
	if (dp->hdcp.hdcp2_present || dp->hdcp.hdcp1_present) {
		dp->hdcp.data = fd;
		dp->hdcp.ops = ops;
	} else {
		dp->hdcp.data = NULL;
		dp->hdcp.ops = NULL;
	}
}

static int mdss_dp_event_handler(struct mdss_panel_data *pdata,
				  int event, void *arg)
{
	int rc = 0;
	struct fb_info *fbi;
	struct mdss_dp_drv_pdata *dp = NULL;

	if (!pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pr_debug("event=%s\n", mdss_panel_intf_event_to_string(event));

	dp = container_of(pdata, struct mdss_dp_drv_pdata,
				panel_data);

	switch (event) {
	case MDSS_EVENT_UNBLANK:
		rc = mdss_dp_on(pdata);
		break;
	case MDSS_EVENT_PANEL_ON:
		mdss_dp_ack_state(dp, true);

		mdss_dp_update_hdcp_info(dp);

		if (dp_is_hdcp_enabled(dp)) {
			cancel_delayed_work(&dp->hdcp_cb_work);

			dp->hdcp_status = HDCP_STATE_AUTHENTICATING;
			queue_delayed_work(dp->workq,
				&dp->hdcp_cb_work, HZ / 2);
		}
		break;
	case MDSS_EVENT_PANEL_OFF:
		rc = mdss_dp_off(pdata);
		break;
	case MDSS_EVENT_BLANK:
		if (dp_is_hdcp_enabled(dp)) {
			dp->hdcp_status = HDCP_STATE_INACTIVE;

			cancel_delayed_work(&dp->hdcp_cb_work);
			if (dp->hdcp.ops->off)
				dp->hdcp.ops->off(dp->hdcp.data);
		}

		mdss_dp_mainlink_push_idle(pdata);
		break;
	case MDSS_EVENT_FB_REGISTERED:
		fbi = (struct fb_info *)arg;
		if (!fbi || !fbi->dev)
			break;

		dp->kobj = &fbi->dev->kobj;
		dp->fb_node = fbi->node;
		mdss_dp_sysfs_create(dp, fbi);
		mdss_dp_edid_init(pdata);
		mdss_dp_hdcp_init(pdata);

		rc = mdss_dp_init_ext_disp(dp);
		if (rc)
			pr_err("failed to initialize ext disp data, ret=%d\n",
					rc);

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

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,phy-register-offset", &dp_drv->phy_reg_offset);
	if (rc)
		dp_drv->phy_reg_offset = 0;

	rc = msm_dss_ioremap_byname(pdev, &dp_drv->tcsr_reg_io,
					"tcsr_regs");
	if (rc) {
		pr_err("%d unable to remap dp tcsr_reg resources\n",
			       __LINE__);
		return rc;
	}

	if (msm_dss_ioremap_byname(pdev, &dp_drv->dp_cc_io, "dp_mmss_cc")) {
		pr_err("%d unable to remap dp MMSS_CC resources\n",
				__LINE__);
		return rc;
	}

	if (msm_dss_ioremap_byname(pdev, &dp_drv->qfprom_io,
					"qfprom_physical"))
		pr_warn("unable to remap dp qfprom resources\n");

	if (msm_dss_ioremap_byname(pdev, &dp_drv->hdcp_io,
					"hdcp_physical"))
		pr_warn("unable to remap dp hdcp resources\n");

	pr_debug("DP Driver base=%pK size=%x\n",
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

static int mdss_dp_event_thread(void *data)
{
	unsigned long flag;
	u32 todo = 0, config;

	struct mdss_dp_event_data *ev_data;
	struct mdss_dp_event *ev;
	struct mdss_dp_drv_pdata *dp = NULL;

	if (!data)
		return -EINVAL;

	ev_data = (struct mdss_dp_event_data *)data;
	init_waitqueue_head(&ev_data->event_q);
	spin_lock_init(&ev_data->event_lock);

	while (!kthread_should_stop()) {
		wait_event(ev_data->event_q,
			(ev_data->pndx != ev_data->gndx) ||
			kthread_should_stop());
		spin_lock_irqsave(&ev_data->event_lock, flag);
		ev = &(ev_data->event_list[ev_data->gndx++]);
		todo = ev->id;
		dp = ev->dp;
		ev->id = 0;
		ev_data->gndx %= MDSS_DP_EVENT_Q_MAX;
		spin_unlock_irqrestore(&ev_data->event_lock, flag);

		pr_debug("todo=%s\n", mdss_dp_ev_event_to_string(todo));

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
		case EV_USBPD_ATTENTION:
			mdss_dp_handle_attention(dp);
			break;
		case EV_USBPD_DISCOVER_MODES:
			usbpd_send_svdm(dp->pd, USB_C_DP_SID,
				USBPD_SVDM_DISCOVER_MODES,
				SVDM_CMD_TYPE_INITIATOR, 0x0, 0x0, 0x0);
			break;
		case EV_USBPD_ENTER_MODE:
			usbpd_send_svdm(dp->pd, USB_C_DP_SID,
				USBPD_SVDM_ENTER_MODE,
				SVDM_CMD_TYPE_INITIATOR, 0x1, 0x0, 0x0);
			break;
		case EV_USBPD_EXIT_MODE:
			usbpd_send_svdm(dp->pd, USB_C_DP_SID,
				USBPD_SVDM_EXIT_MODE,
				SVDM_CMD_TYPE_INITIATOR, 0x1, 0x0, 0x0);
			break;
		case EV_USBPD_DP_STATUS:
			config = 0x1; /* DFP_D connected */
			usbpd_send_svdm(dp->pd, USB_C_DP_SID, DP_VDM_STATUS,
				SVDM_CMD_TYPE_INITIATOR, 0x1, &config, 0x1);
			break;
		case EV_USBPD_DP_CONFIGURE:
			config = mdss_dp_usbpd_gen_config_pkt(dp);
			usbpd_send_svdm(dp->pd, USB_C_DP_SID, DP_VDM_CONFIGURE,
				SVDM_CMD_TYPE_INITIATOR, 0x1, &config, 0x1);
			break;
		default:
			pr_err("Unknown event:%d\n", todo);
		}
	}

	return 0;
}

static void dp_send_events(struct mdss_dp_drv_pdata *dp, u32 event)
{
	struct mdss_dp_event *ev;
	struct mdss_dp_event_data *ev_data = &dp->dp_event;

	pr_debug("event=%s\n", mdss_dp_ev_event_to_string(event));

	spin_lock(&ev_data->event_lock);
	ev = &ev_data->event_list[ev_data->pndx++];
	ev->id = event;
	ev->dp = dp;
	ev_data->pndx %= MDSS_DP_EVENT_Q_MAX;
	wake_up(&ev_data->event_q);
	spin_unlock(&ev_data->event_lock);
}

irqreturn_t dp_isr(int irq, void *ptr)
{
	struct mdss_dp_drv_pdata *dp = (struct mdss_dp_drv_pdata *)ptr;
	unsigned char *base = dp->base;
	u32 isr1, isr2, mask1;
	u32 ack;

	spin_lock(&dp->lock);
	isr1 = dp_read(base + DP_INTR_STATUS);
	isr2 = dp_read(base + DP_INTR_STATUS2);

	mask1 = isr1 & dp->mask1;

	isr1 &= ~mask1;	/* remove masks bit */

	ack = isr1 & EDP_INTR_STATUS1;
	ack <<= 1;	/* ack bits */
	ack |= mask1;
	dp_write(base + DP_INTR_STATUS, ack);

	ack = isr2 & EDP_INTR_STATUS2;
	ack <<= 1;	/* ack bits */
	ack |= isr2;
	dp_write(base + DP_INTR_STATUS2, ack);
	spin_unlock(&dp->lock);

	if (isr1 & EDP_INTR_HPD) {
		isr1 &= ~EDP_INTR_HPD;	/* clear */
		mdss_dp_host_init(&dp->panel_data);
		dp_send_events(dp, EV_LINK_TRAIN);
	}

	if (isr2 & EDP_INTR_READY_FOR_VIDEO)
		mdss_dp_video_ready(dp);

	if (isr2 & EDP_INTR_IDLE_PATTERNs_SENT)
		mdss_dp_idle_patterns_sent(dp);

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

	if (dp_is_hdcp_enabled(dp) && dp->hdcp.ops->isr) {
		if (dp->hdcp.ops->isr(dp->hdcp.data))
			pr_err("dp_hdcp_isr failed\n");
	}

	return IRQ_HANDLED;
}

static void mdss_dp_event_cleanup(struct mdss_dp_drv_pdata *dp)
{
	destroy_workqueue(dp->workq);

	if (dp->ev_thread == current)
		return;

	kthread_stop(dp->ev_thread);
}

static int mdss_dp_event_setup(struct mdss_dp_drv_pdata *dp)
{

	dp->ev_thread = kthread_run(mdss_dp_event_thread,
		(void *)&dp->dp_event, "mdss_dp_event");
	if (IS_ERR(dp->ev_thread)) {
		pr_err("unable to start event thread\n");
		return PTR_ERR(dp->ev_thread);
	}

	dp->workq = create_workqueue("mdss_dp_hpd");
	if (!dp->workq) {
		pr_err("%s: Error creating workqueue\n", __func__);
		return -EPERM;
	}

	INIT_DELAYED_WORK(&dp->hdcp_cb_work, mdss_dp_hdcp_cb_work);
	INIT_LIST_HEAD(&dp->attention_head);
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

	mdss_dp_update_cable_status(dp_drv, true);

	if (dp_drv->hpd)
		dp_send_events(dp_drv, EV_USBPD_DISCOVER_MODES);
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
	mdss_dp_update_cable_status(dp_drv, false);
	dp_drv->alt_mode.current_state = UNKNOWN_STATE;

	/**
	 * Manually turn off the DP controller if we are in PHY
	 * testing mode.
	 */
	if (mdss_dp_is_phy_test_pattern_requested(dp_drv)) {
		pr_info("turning off DP controller for PHY testing\n");
		mdss_dp_mainlink_push_idle(&dp_drv->panel_data);
		mdss_dp_off_hpd(dp_drv);
	} else {
		mdss_dp_notify_clients(dp_drv, NOTIFY_DISCONNECT);
	}
}

static int mdss_dp_validate_callback(u8 cmd,
	enum usbpd_svdm_cmd_type cmd_type, int num_vdos)
{
	int ret = 0;

	if (cmd_type == SVDM_CMD_TYPE_RESP_NAK) {
		pr_err("error: NACK\n");
		ret = -EINVAL;
		goto end;
	}

	if (cmd_type == SVDM_CMD_TYPE_RESP_BUSY) {
		pr_err("error: BUSY\n");
		ret = -EBUSY;
		goto end;
	}

	if (cmd == USBPD_SVDM_ATTENTION) {
		if (cmd_type != SVDM_CMD_TYPE_INITIATOR) {
			pr_err("error: invalid cmd type for attention\n");
			ret = -EINVAL;
			goto end;
		}

		if (!num_vdos) {
			pr_err("error: no vdo provided\n");
			ret = -EINVAL;
			goto end;
		}
	} else {
		if (cmd_type != SVDM_CMD_TYPE_RESP_ACK) {
			pr_err("error: invalid cmd type\n");
			ret = -EINVAL;
		}
	}
end:
	return ret;
}

/**
 * mdss_dp_send_test_response() - sends the test response to the sink
 * @dp: Display Port Driver data
 *
 * This function will send the test response to the sink but only after
 * any previous link training has been completed.
 */
static inline void mdss_dp_send_test_response(struct mdss_dp_drv_pdata *dp)
{
	mutex_lock(&dp->train_mutex);
	mdss_dp_aux_send_test_response(dp);
	mutex_unlock(&dp->train_mutex);
}

/**
 * mdss_dp_link_maintenance() - initiates link maintenanace
 * @dp: Display Port Driver data
 * @lt_needed: link retraining needed
 *
 * This function will perform link maintenance by first notifying
 * DP clients and triggering DP shutdown, and then enabling DP after
 * notification is done successfully. It will perform link retraining
 * if specified.
 */
static inline void mdss_dp_link_maintenance(struct mdss_dp_drv_pdata *dp,
		bool lt_needed)
{
	if (mdss_dp_notify_clients(dp, NOTIFY_DISCONNECT_IRQ_HPD))
		return;

	mdss_dp_on_irq(dp, lt_needed);
}

/**
 * mdss_dp_process_link_status_update() - processes link status updates
 * @dp: Display Port Driver data
 *
 * This function will check for changes in the link status, e.g. clock
 * recovery done on all lanes, and trigger link training if there is a
 * failure/error on the link.
 *
 * The function will return 0 if the a link status update has been processed,
 * otherwise it will return -EINVAL.
 */
static int mdss_dp_process_link_status_update(struct mdss_dp_drv_pdata *dp)
{
	if (!mdss_dp_is_link_status_updated(dp) ||
			(mdss_dp_aux_channel_eq_done(dp) &&
			mdss_dp_aux_clock_recovery_done(dp)))
		return -EINVAL;

	pr_info("channel_eq_done = %d, clock_recovery_done = %d\n",
			mdss_dp_aux_channel_eq_done(dp),
			mdss_dp_aux_clock_recovery_done(dp));

	mdss_dp_link_maintenance(dp, true);

	return 0;
}

/**
 * mdss_dp_process_link_training_request() - processes new training requests
 * @dp: Display Port Driver data
 *
 * This function will handle new link training requests that are initiated by
 * the sink. In particular, it will update the requested lane count and link
 * link rate, and then trigger the link retraining procedure.
 *
 * The function will return 0 if a link training request has been processed,
 * otherwise it will return -EINVAL.
 */
static int mdss_dp_process_link_training_request(struct mdss_dp_drv_pdata *dp)
{
	if (!mdss_dp_is_link_training_requested(dp))
		return -EINVAL;

	mdss_dp_send_test_response(dp);

	pr_info("%s link rate = 0x%x, lane count = 0x%x\n",
			mdss_dp_get_test_name(TEST_LINK_TRAINING),
			dp->test_data.test_link_rate,
			dp->test_data.test_lane_count);
	dp->dpcd.max_lane_count =
		dp->test_data.test_lane_count;
	dp->link_rate = dp->test_data.test_link_rate;

	mdss_dp_link_maintenance(dp, true);

	return 0;
}

/**
 * mdss_dp_process_phy_test_pattern_request() - process new phy test requests
 * @dp: Display Port Driver data
 *
 * This function will handle new phy test pattern requests that are initiated
 * by the sink. The function will return 0 if a phy test pattern has been
 * processed, otherwise it will return -EINVAL.
 */
static int mdss_dp_process_phy_test_pattern_request(
		struct mdss_dp_drv_pdata *dp)
{
	u32 test_link_rate = 0, test_lane_count = 0;

	if (!mdss_dp_is_phy_test_pattern_requested(dp))
		return -EINVAL;

	test_link_rate = dp->test_data.test_link_rate;
	test_lane_count = dp->test_data.test_lane_count;

	if (!mdss_dp_aux_is_link_rate_valid(test_link_rate) ||
		!mdss_dp_aux_is_lane_count_valid(test_lane_count)) {
		pr_info("Invalid params: link rate = 0x%x, lane count = 0x%x\n",
				test_link_rate, test_lane_count);
		return -EINVAL;
	}

	pr_debug("start\n");

	if (dp->power_on) {
		pr_info("turning off DP controller for PHY testing\n");
		mdss_dp_mainlink_push_idle(&dp->panel_data);
		/*
		 * The global reset will need DP link ralated clocks to be
		 * running. Add the global reset just before disabling the
		 * link clocks and core clocks.
		 */
		mdss_dp_ctrl_reset(&dp->ctrl_io);
		mdss_dp_off_irq(dp);
	}

	/**
	 * Set the timing information to 1920x1080p60. This resolution will be
	 * used when enabling the pixel clock.
	 */
	dp_init_panel_info(dp, HDMI_VFRMT_1920x1080p60_16_9);

	pr_info("Current: link rate = 0x%x, lane count = 0x%x\n",
			dp->dpcd.max_lane_count,
			dp->link_rate);

	pr_info("Requested: link rate = 0x%x, lane count = 0x%x\n",
			dp->test_data.test_link_rate,
			dp->test_data.test_lane_count);

	dp->dpcd.max_lane_count = dp->test_data.test_lane_count;
	dp->link_rate = dp->test_data.test_link_rate;

	mdss_dp_on_irq(dp, true);

	/**
	 * Read the updated values for voltage and pre-emphasis levels and
	 * then program the DP controller PHY accordingly.
	 */
	mdss_dp_aux_parse_vx_px(dp);
	mdss_dp_aux_update_voltage_and_pre_emphasis_lvl(dp);

	mdss_dp_phy_send_test_pattern(dp);

	mdss_dp_send_test_response(dp);

	pr_debug("end\n");

	return 0;
}

/**
 * mdss_dp_process_audio_pattern_request() - process new audio pattern request
 * @dp: Display Port Driver data
 *
 * This function will handle a new audio pattern request that is initiated by
 * the sink. This is acheieved by sending the necessary secondary data packets
 * to the sink. It is expected that any simulatenous requests for video
 * patterns will be handled before the audio pattern is sent to the sink.
 */
static int mdss_dp_process_audio_pattern_request(struct mdss_dp_drv_pdata *dp)
{
	if (!mdss_dp_is_audio_pattern_requested(dp))
		return -EINVAL;

	if (dp_is_hdcp_enabled(dp) && dp->hdcp.ops->off) {
		cancel_delayed_work(&dp->hdcp_cb_work);
		dp->hdcp.ops->off(dp->hdcp.data);
	}

	pr_debug("sampling_rate=%s, channel_count=%d, pattern_type=%s\n",
		mdss_dp_get_audio_sample_rate(
			dp->test_data.test_audio_sampling_rate),
		dp->test_data.test_audio_channel_count,
		mdss_dp_get_audio_test_pattern(
			dp->test_data.test_audio_pattern_type));

	pr_debug("audio_period: ch1=0x%x, ch2=0x%x, ch3=0x%x, ch4=0x%x\n",
		dp->test_data.test_audio_period_ch_1,
		dp->test_data.test_audio_period_ch_2,
		dp->test_data.test_audio_period_ch_3,
		dp->test_data.test_audio_period_ch_4);

	pr_debug("audio_period: ch5=0x%x, ch6=0x%x, ch7=0x%x, ch8=0x%x\n",
		dp->test_data.test_audio_period_ch_5,
		dp->test_data.test_audio_period_ch_6,
		dp->test_data.test_audio_period_ch_7,
		dp->test_data.test_audio_period_ch_8);

	if (dp->ext_audio_data.intf_ops.hpd)
		dp->ext_audio_data.intf_ops.hpd(dp->ext_pdev,
			dp->ext_audio_data.type, 1, MSM_EXT_DISP_HPD_AUDIO);

	dp->audio_test_req = true;

	return 0;
}

/**
 * mdss_dp_process_downstream_port_status_change() - process port status changes
 * @dp: Display Port Driver data
 *
 * This function will handle downstream port updates that are initiated by
 * the sink. If the downstream port status has changed, the EDID is read via
 * AUX.
 *
 * The function will return 0 if a downstream port update has been
 * processed, otherwise it will return -EINVAL.
 */
static int mdss_dp_process_downstream_port_status_change(
		struct mdss_dp_drv_pdata *dp)
{
	if (!mdss_dp_is_downstream_port_status_changed(dp))
		return -EINVAL;

	return mdss_dp_edid_read(dp);
}

static bool mdss_dp_video_pattern_test_lt_needed(struct mdss_dp_drv_pdata *dp)
{
	char new_link_rate;

	/*
	 * Link re-training for video format change is only needed if:
	 *    1. Link rate changes
	 *    2. Lane count changes
	 * For now, assume that lane count is not going to change
	 */
	new_link_rate = mdss_dp_gen_link_clk(dp);
	pr_debug("new link rate = 0x%x, current link rate = 0x%x\n",
		new_link_rate, dp->link_rate);
	if (new_link_rate != dp->link_rate) {
		dp->link_rate = new_link_rate;
		return true;
	}

	return false;
}

/**
 * mdss_dp_process_video_pattern_request() - process new video pattern request
 * @dp: Display Port Driver data
 *
 * This function will handle a new video pattern request that are initiated by
 * the sink. This is acheieved by first sending a disconnect notification to
 * the sink followed by a subsequent connect notification to the user modules,
 * where it is expected that the user modules would draw the required test
 * pattern.
 */
static int mdss_dp_process_video_pattern_request(struct mdss_dp_drv_pdata *dp)
{
	bool lt_needed;
	struct hdmi_edid_override_data ov_data = {0, 0, 1,
		HDMI_VFRMT_640x480p59_4_3};
	bool ov_res = false;

	if (!mdss_dp_is_video_pattern_requested(dp))
		goto end;

	pr_info("%s: bit depth=%d(%d bpp) pattern=%s\n",
		mdss_dp_get_test_name(TEST_VIDEO_PATTERN),
		dp->test_data.test_bit_depth,
		mdss_dp_test_bit_depth_to_bpp(dp->test_data.test_bit_depth),
		mdss_dp_test_video_pattern_to_string(
			dp->test_data.test_video_pattern));

	if (dp->test_data.test_h_width == 640) {
		pr_debug("Set resolution to 640x480p59");
		if (dp->vic != HDMI_VFRMT_640x480p59_4_3) {
			ov_res = true;
			dp->vic = HDMI_VFRMT_640x480p59_4_3;
		}
		hdmi_edid_config_override(dp->panel_data.panel_info.edid_data,
			true, &ov_data);
	}

	dp_init_panel_info(dp, dp->vic);
	lt_needed = ov_res | mdss_dp_video_pattern_test_lt_needed(dp);

	pr_debug("Link training needed: %s", lt_needed ? "yes" : "no");

	mdss_dp_link_maintenance(dp, lt_needed);

	if (mdss_dp_is_audio_pattern_requested(dp))
		goto end;

	mdss_dp_send_test_response(dp);

	return 0;
end:
	return -EINVAL;
}

/**
 * mdss_dp_process_hpd_irq_high() - handle HPD IRQ transition to HIGH
 * @dp: Display Port Driver data
 *
 * This function will handle the HPD IRQ state transitions from LOW to HIGH
 * (including cases when there are back to back HPD IRQ HIGH) indicating
 * the start of a new link training request or sink status update.
 */
static int mdss_dp_process_hpd_irq_high(struct mdss_dp_drv_pdata *dp)
{
	int ret = 0;

	pr_debug("start\n");

	dp->hpd_irq_on = true;

	mdss_dp_reset_test_data(dp);

	mdss_dp_aux_parse_sink_status_field(dp);

	ret = mdss_dp_process_link_training_request(dp);
	if (!ret)
		goto exit;

	ret = mdss_dp_process_phy_test_pattern_request(dp);
	if (!ret)
		goto exit;

	ret = mdss_dp_process_link_status_update(dp);
	if (!ret)
		goto exit;

	ret = mdss_dp_process_downstream_port_status_change(dp);
	if (!ret)
		goto exit;

	ret = mdss_dp_process_video_pattern_request(dp);
	if (!ret)
		goto exit;

	ret = mdss_dp_process_audio_pattern_request(dp);
	if (!ret)
		goto exit;

	pr_debug("done\n");

exit:
	dp->hpd_irq_on = false;
	return ret;
}

static void usbpd_response_callback(struct usbpd_svid_handler *hdlr, u8 cmd,
				enum usbpd_svdm_cmd_type cmd_type,
				const u32 *vdos, int num_vdos)
{
	struct mdss_dp_drv_pdata *dp_drv;
	struct mdss_dp_attention_node *node;

	dp_drv = container_of(hdlr, struct mdss_dp_drv_pdata, svid_handler);
	if (!dp_drv->pd) {
		pr_err("get_usbpd phandle failed\n");
		return;
	}

	pr_debug("callback -> cmd: 0x%x, *vdos = 0x%x, num_vdos = %d\n",
				cmd, *vdos, num_vdos);

	if (mdss_dp_validate_callback(cmd, cmd_type, num_vdos)) {
		pr_debug("invalid callback received\n");
		return;
	}

	switch (cmd) {
	case USBPD_SVDM_DISCOVER_MODES:
		dp_drv->alt_mode.dp_cap.response = *vdos;
		mdss_dp_usbpd_ext_capabilities(&dp_drv->alt_mode.dp_cap);
		dp_drv->alt_mode.current_state |= DISCOVER_MODES_DONE;
		if (dp_drv->alt_mode.dp_cap.s_port & BIT(0))
			dp_send_events(dp_drv, EV_USBPD_ENTER_MODE);
		break;
	case USBPD_SVDM_ENTER_MODE:
		dp_drv->alt_mode.current_state |= ENTER_MODE_DONE;
		dp_send_events(dp_drv, EV_USBPD_DP_STATUS);
		break;
	case USBPD_SVDM_ATTENTION:
		node = kzalloc(sizeof(*node), GFP_KERNEL);
		if (!node)
			return;
		node->vdo = *vdos;

		mutex_lock(&dp_drv->attention_lock);
		list_add_tail(&node->list, &dp_drv->attention_head);
		mutex_unlock(&dp_drv->attention_lock);

		dp_send_events(dp_drv, EV_USBPD_ATTENTION);
		break;
	case DP_VDM_STATUS:
		dp_drv->alt_mode.dp_status.response = *vdos;
		mdss_dp_usbpd_ext_dp_status(&dp_drv->alt_mode.dp_status);

		if (!(dp_drv->alt_mode.current_state & DP_CONFIGURE_DONE)) {
			dp_drv->alt_mode.current_state |= DP_STATUS_DONE;
			if (dp_drv->alt_mode.dp_status.c_port & BIT(1))
				dp_send_events(dp_drv, EV_USBPD_DP_CONFIGURE);
		}
		break;
	case DP_VDM_CONFIGURE:
		dp_drv->alt_mode.current_state |= DP_CONFIGURE_DONE;
		pr_debug("Configure: config USBPD to DP done\n");
		mdss_dp_usbpd_ext_dp_status(&dp_drv->alt_mode.dp_status);

		mdss_dp_host_init(&dp_drv->panel_data);

		if (dp_drv->alt_mode.dp_status.hpd_high)
			mdss_dp_process_hpd_high(dp_drv);
		break;
	default:
		pr_err("unknown cmd: %d\n", cmd);
		break;
	}
}

static void mdss_dp_process_attention(struct mdss_dp_drv_pdata *dp_drv)
{
	if (dp_drv->alt_mode.dp_status.hpd_irq) {
		pr_debug("Attention: hpd_irq high\n");

		if (dp_is_hdcp_enabled(dp_drv) && dp_drv->hdcp.ops->cp_irq) {
			if (!dp_drv->hdcp.ops->cp_irq(dp_drv->hdcp.data))
				return;
		}

		if (!mdss_dp_process_hpd_irq_high(dp_drv))
			return;
	}

	if (!dp_drv->alt_mode.dp_status.hpd_high) {
		pr_debug("Attention: HPD low\n");

		if (dp_is_hdcp_enabled(dp_drv) && dp_drv->hdcp.ops->off) {
			cancel_delayed_work(&dp_drv->hdcp_cb_work);
			dp_drv->hdcp.ops->off(dp_drv->hdcp.data);
		}

		mdss_dp_notify_clients(dp_drv, NOTIFY_DISCONNECT);
		pr_debug("Attention: Notified clients\n");

		/**
		 * Manually turn off the DP controller if we are in PHY
		 * testing mode.
		 */
		if (mdss_dp_is_phy_test_pattern_requested(dp_drv)) {
			pr_info("turning off DP controller for PHY testing\n");
			mdss_dp_mainlink_push_idle(&dp_drv->panel_data);
			mdss_dp_off_hpd(dp_drv);
		}
		return;
	}

	pr_debug("Attention: HPD high\n");

	dp_drv->alt_mode.current_state |= DP_STATUS_DONE;

	if (dp_drv->alt_mode.current_state & DP_CONFIGURE_DONE) {
		mdss_dp_host_init(&dp_drv->panel_data);
		mdss_dp_process_hpd_high(dp_drv);
	} else {
		dp_send_events(dp_drv, EV_USBPD_DP_CONFIGURE);
	}
}

static void mdss_dp_handle_attention(struct mdss_dp_drv_pdata *dp)
{
	int i = 0;
	pr_debug("start\n");

	while (!list_empty_careful(&dp->attention_head)) {
		struct mdss_dp_attention_node *node;
		u32 vdo;

		pr_debug("processing item %d in the list\n", ++i);

		mutex_lock(&dp->attention_lock);
		node = list_first_entry(&dp->attention_head,
				struct mdss_dp_attention_node, list);

		vdo = node->vdo;
		list_del(&node->list);
		mutex_unlock(&dp->attention_lock);

		kzfree(node);

		dp->alt_mode.dp_status.response = vdo;
		mdss_dp_usbpd_ext_dp_status(&dp->alt_mode.dp_status);
		mdss_dp_process_attention(dp);

		pr_debug("done processing item %d in the list\n", i);
	};

	pr_debug("exit\n");
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
	mutex_init(&dp_drv->pd_msg_mutex);
	mutex_init(&dp_drv->attention_lock);
	mutex_init(&dp_drv->hdcp_mutex);
	spin_lock_init(&dp_drv->lock);
	mutex_init(&dp_drv->aux_mutex);
	mutex_init(&dp_drv->train_mutex);
	init_completion(&dp_drv->aux_comp);
	init_completion(&dp_drv->idle_comp);
	init_completion(&dp_drv->video_comp);

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

	ret = mdss_dp_parse_prop(pdev, dp_drv);
	if (ret) {
		DEV_ERR("DP properties parsing failed.ret=%d\n",
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
	dp_drv->hpd_irq_on = false;
	mdss_dp_reset_test_data(dp_drv);
	init_completion(&dp_drv->irq_comp);

	pr_debug("done\n");

	dp_send_events(dp_drv, EV_USBPD_DISCOVER_MODES);

	return 0;

probe_err:
	iounmap(dp_drv->ctrl_io.base);
	iounmap(dp_drv->phy_io.base);
	if (dp_drv) {
		if (dp_drv->pd)
			usbpd_unregister_svid(dp_drv->pd,
					&dp_drv->svid_handler);
		devm_kfree(&pdev->dev, dp_drv);
	}
	return ret;

}

void *mdss_dp_get_hdcp_data(struct device *dev)
{
	struct mdss_dp_drv_pdata *dp;
	struct msm_fb_data_type *mfd;
	struct mdss_panel_data *pd;
	struct fb_info *fbi = dev_get_drvdata(dev);

	if (!fbi) {
		pr_err("invalid fbi\n");
		goto error;
	}

	mfd = (struct msm_fb_data_type *)fbi->par;
	if (!mfd) {
		pr_err("invalid mfd\n");
		goto error;
	}

	pd = dev_get_platdata(&mfd->pdev->dev);
	if (!pd) {
		pr_err("invalid panel_data\n");
		goto error;
	}

	dp = container_of(pd, struct mdss_dp_drv_pdata, panel_data);

	return dp->hdcp.data;
error:
	return NULL;
}

static inline bool dp_is_stream_shareable(struct mdss_dp_drv_pdata *dp_drv)
{
	bool ret = 0;

	switch (dp_drv->hdcp.enc_lvl) {
	case HDCP_STATE_AUTH_ENC_NONE:
		ret = true;
		break;
	case HDCP_STATE_AUTH_ENC_1X:
		ret = dp_is_hdcp_enabled(dp_drv) &&
			dp_drv->hdcp.auth_state;
		break;
	case HDCP_STATE_AUTH_ENC_2P2:
		ret = dp_drv->hdcp.feature_enabled &&
			dp_drv->hdcp.hdcp2_present &&
			dp_drv->hdcp.auth_state;
		break;
	default:
		ret = false;
	}

	return ret;
}

static int mdss_dp_remove(struct platform_device *pdev)
{
	struct mdss_dp_drv_pdata *dp_drv = NULL;

	dp_drv = platform_get_drvdata(pdev);
	dp_hdcp2p2_deinit(dp_drv->hdcp.data);

	mdss_dp_event_cleanup(dp_drv);
	iounmap(dp_drv->ctrl_io.base);
	dp_drv->ctrl_io.base = NULL;
	iounmap(dp_drv->phy_io.base);
	dp_drv->phy_io.base = NULL;

	return 0;
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
