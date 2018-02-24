/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include <linux/of_gpio.h>
#include <linux/of_platform.h>

#include "dp_parser.h"

static void dp_parser_unmap_io_resources(struct dp_parser *parser)
{
	int i = 0;
	struct dp_io *io = &parser->io;

	for (i = 0; i < io->len; i++)
		msm_dss_iounmap(&io->data[i].io);
}

static int dp_parser_reg(struct dp_parser *parser)
{
	int rc = 0, i = 0;
	u32 reg_count;
	struct platform_device *pdev = parser->pdev;
	struct dp_io *io = &parser->io;
	struct device *dev = &pdev->dev;

	reg_count = of_property_count_strings(dev->of_node, "reg-names");
	if (reg_count <= 0) {
		pr_err("no reg defined\n");
		return -EINVAL;
	}

	io->len = reg_count;
	io->data = devm_kzalloc(dev, sizeof(struct dp_io_data) * reg_count,
			GFP_KERNEL);
	if (!io->data)
		return -ENOMEM;

	for (i = 0; i < reg_count; i++) {
		of_property_read_string_index(dev->of_node,
				"reg-names", i,	&io->data[i].name);
		rc = msm_dss_ioremap_byname(pdev, &io->data[i].io,
			io->data[i].name);
		if (rc) {
			pr_err("unable to remap %s resources\n",
				io->data[i].name);
			goto err;
		}
	}

	return 0;
err:
	dp_parser_unmap_io_resources(parser);
	return rc;
}

static const char *dp_get_phy_aux_config_property(u32 cfg_type)
{
	switch (cfg_type) {
	case PHY_AUX_CFG0:
		return "qcom,aux-cfg0-settings";
	case PHY_AUX_CFG1:
		return "qcom,aux-cfg1-settings";
	case PHY_AUX_CFG2:
		return "qcom,aux-cfg2-settings";
	case PHY_AUX_CFG3:
		return "qcom,aux-cfg3-settings";
	case PHY_AUX_CFG4:
		return "qcom,aux-cfg4-settings";
	case PHY_AUX_CFG5:
		return "qcom,aux-cfg5-settings";
	case PHY_AUX_CFG6:
		return "qcom,aux-cfg6-settings";
	case PHY_AUX_CFG7:
		return "qcom,aux-cfg7-settings";
	case PHY_AUX_CFG8:
		return "qcom,aux-cfg8-settings";
	case PHY_AUX_CFG9:
		return "qcom,aux-cfg9-settings";
	default:
		return "unknown";
	}
}

static void dp_parser_phy_aux_cfg_reset(struct dp_parser *parser)
{
	int i = 0;

	for (i = 0; i < PHY_AUX_CFG_MAX; i++)
		parser->aux_cfg[i] = (const struct dp_aux_cfg){ 0 };
}

static int dp_parser_aux(struct dp_parser *parser)
{
	struct device_node *of_node = parser->pdev->dev.of_node;
	int len = 0, i = 0, j = 0, config_count = 0;
	const char *data;
	int const minimum_config_count = 1;

	for (i = 0; i < PHY_AUX_CFG_MAX; i++) {
		const char *property = dp_get_phy_aux_config_property(i);

		data = of_get_property(of_node, property, &len);
		if (!data) {
			pr_err("Unable to read %s\n", property);
			goto error;
		}

		config_count = len - 1;
		if ((config_count < minimum_config_count) ||
			(config_count > DP_AUX_CFG_MAX_VALUE_CNT)) {
			pr_err("Invalid config count (%d) configs for %s\n",
					config_count, property);
			goto error;
		}

		parser->aux_cfg[i].offset = data[0];
		parser->aux_cfg[i].cfg_cnt = config_count;
		pr_debug("%s offset=0x%x, cfg_cnt=%d\n",
				property,
				parser->aux_cfg[i].offset,
				parser->aux_cfg[i].cfg_cnt);
		for (j = 1; j < len; j++) {
			parser->aux_cfg[i].lut[j - 1] = data[j];
			pr_debug("%s lut[%d]=0x%x\n",
					property,
					i,
					parser->aux_cfg[i].lut[j - 1]);
		}
	}
		return 0;

error:
	dp_parser_phy_aux_cfg_reset(parser);
	return -EINVAL;
}

static int dp_parser_misc(struct dp_parser *parser)
{
	int rc = 0;
	struct device_node *of_node = parser->pdev->dev.of_node;

	rc = of_property_read_u32(of_node,
		"qcom,max-pclk-frequency-khz", &parser->max_pclk_khz);
	if (rc)
		parser->max_pclk_khz = DP_MAX_PIXEL_CLK_KHZ;

	return 0;
}

static int dp_parser_msm_hdcp_dev(struct dp_parser *parser)
{
	struct device_node *node;
	struct platform_device *pdev;

	node = of_find_compatible_node(NULL, NULL, "qcom,msm-hdcp");
	if (!node) {
		// This is a non-fatal error, module initialization can proceed
		pr_warn("couldn't find msm-hdcp node\n");
		return 0;
	}

	pdev = of_find_device_by_node(node);
	if (!pdev) {
		// This is a non-fatal error, module initialization can proceed
		pr_warn("couldn't find msm-hdcp pdev\n");
		return 0;
	}

	parser->msm_hdcp_dev = &pdev->dev;

	return 0;
}

static int dp_parser_pinctrl(struct dp_parser *parser)
{
	int rc = 0;
	struct dp_pinctrl *pinctrl = &parser->pinctrl;

	pinctrl->pin = devm_pinctrl_get(&parser->pdev->dev);

	if (IS_ERR_OR_NULL(pinctrl->pin)) {
		rc = PTR_ERR(pinctrl->pin);
		pr_err("failed to get pinctrl, rc=%d\n", rc);
		goto error;
	}

	pinctrl->state_active = pinctrl_lookup_state(pinctrl->pin,
					"mdss_dp_active");
	if (IS_ERR_OR_NULL(pinctrl->state_active)) {
		rc = PTR_ERR(pinctrl->state_active);
		pr_err("failed to get pinctrl active state, rc=%d\n", rc);
		goto error;
	}

	pinctrl->state_suspend = pinctrl_lookup_state(pinctrl->pin,
					"mdss_dp_sleep");
	if (IS_ERR_OR_NULL(pinctrl->state_suspend)) {
		rc = PTR_ERR(pinctrl->state_suspend);
		pr_err("failed to get pinctrl suspend state, rc=%d\n", rc);
		goto error;
	}
error:
	return rc;
}

static int dp_parser_gpio(struct dp_parser *parser)
{
	int i = 0;
	struct device *dev = &parser->pdev->dev;
	struct device_node *of_node = dev->of_node;
	struct dss_module_power *mp = &parser->mp[DP_CORE_PM];
	static const char * const dp_gpios[] = {
		"qcom,aux-en-gpio",
		"qcom,aux-sel-gpio",
		"qcom,usbplug-cc-gpio",
	};

	mp->gpio_config = devm_kzalloc(dev,
		sizeof(struct dss_gpio) * ARRAY_SIZE(dp_gpios), GFP_KERNEL);
	if (!mp->gpio_config)
		return -ENOMEM;

	mp->num_gpio = ARRAY_SIZE(dp_gpios);

	for (i = 0; i < ARRAY_SIZE(dp_gpios); i++) {
		mp->gpio_config[i].gpio = of_get_named_gpio(of_node,
			dp_gpios[i], 0);

		if (!gpio_is_valid(mp->gpio_config[i].gpio)) {
			pr_err("%s gpio not specified\n", dp_gpios[i]);
			return -EINVAL;
		}

		strlcpy(mp->gpio_config[i].gpio_name, dp_gpios[i],
			sizeof(mp->gpio_config[i].gpio_name));

		mp->gpio_config[i].value = 0;
	}

	return 0;
}

static const char *dp_parser_supply_node_name(enum dp_pm_type module)
{
	switch (module) {
	case DP_CORE_PM:	return "qcom,core-supply-entries";
	case DP_CTRL_PM:	return "qcom,ctrl-supply-entries";
	case DP_PHY_PM:		return "qcom,phy-supply-entries";
	default:		return "???";
	}
}

static int dp_parser_get_vreg(struct dp_parser *parser,
		enum dp_pm_type module)
{
	int i = 0, rc = 0;
	u32 tmp = 0;
	const char *pm_supply_name = NULL;
	struct device_node *supply_node = NULL;
	struct device_node *of_node = parser->pdev->dev.of_node;
	struct device_node *supply_root_node = NULL;
	struct dss_module_power *mp = &parser->mp[module];

	mp->num_vreg = 0;
	pm_supply_name = dp_parser_supply_node_name(module);
	supply_root_node = of_get_child_by_name(of_node, pm_supply_name);
	if (!supply_root_node) {
		pr_err("no supply entry present: %s\n", pm_supply_name);
		goto novreg;
	}

	mp->num_vreg = of_get_available_child_count(supply_root_node);

	if (mp->num_vreg == 0) {
		pr_debug("no vreg\n");
		goto novreg;
	} else {
		pr_debug("vreg found. count=%d\n", mp->num_vreg);
	}

	mp->vreg_config = devm_kzalloc(&parser->pdev->dev,
		sizeof(struct dss_vreg) * mp->num_vreg, GFP_KERNEL);
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
		devm_kfree(&parser->pdev->dev, mp->vreg_config);
		mp->vreg_config = NULL;
	}
novreg:
	mp->num_vreg = 0;

	return rc;
}

static void dp_parser_put_vreg_data(struct device *dev,
	struct dss_module_power *mp)
{
	if (!mp) {
		DEV_ERR("invalid input\n");
		return;
	}

	if (mp->vreg_config) {
		devm_kfree(dev, mp->vreg_config);
		mp->vreg_config = NULL;
	}
	mp->num_vreg = 0;
}

static int dp_parser_regulator(struct dp_parser *parser)
{
	int i, rc = 0;
	struct platform_device *pdev = parser->pdev;

	/* Parse the regulator information */
	for (i = DP_CORE_PM; i < DP_MAX_PM; i++) {
		rc = dp_parser_get_vreg(parser, i);
		if (rc) {
			pr_err("get_dt_vreg_data failed for %s. rc=%d\n",
				dp_parser_pm_name(i), rc);
			i--;
			for (; i >= DP_CORE_PM; i--)
				dp_parser_put_vreg_data(&pdev->dev,
					&parser->mp[i]);
			break;
		}
	}

	return rc;
}

static bool dp_parser_check_prefix(const char *clk_prefix, const char *clk_name)
{
	return !!strnstr(clk_name, clk_prefix, strlen(clk_name));
}

static void dp_parser_put_clk_data(struct device *dev,
	struct dss_module_power *mp)
{
	if (!mp) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (mp->clk_config) {
		devm_kfree(dev, mp->clk_config);
		mp->clk_config = NULL;
	}

	mp->num_clk = 0;
}

static void dp_parser_put_gpio_data(struct device *dev,
	struct dss_module_power *mp)
{
	if (!mp) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (mp->gpio_config) {
		devm_kfree(dev, mp->gpio_config);
		mp->gpio_config = NULL;
	}

	mp->num_gpio = 0;
}

static int dp_parser_init_clk_data(struct dp_parser *parser)
{
	int num_clk = 0, i = 0, rc = 0;
	int core_clk_count = 0, ctrl_clk_count = 0;
	const char *core_clk = "core";
	const char *ctrl_clk = "ctrl";
	const char *clk_name;
	struct device *dev = &parser->pdev->dev;
	struct dss_module_power *core_power = &parser->mp[DP_CORE_PM];
	struct dss_module_power *ctrl_power = &parser->mp[DP_CTRL_PM];

	num_clk = of_property_count_strings(dev->of_node, "clock-names");
	if (num_clk <= 0) {
		pr_err("no clocks are defined\n");
		rc = -EINVAL;
		goto exit;
	}

	for (i = 0; i < num_clk; i++) {
		of_property_read_string_index(dev->of_node,
				"clock-names", i, &clk_name);

		if (dp_parser_check_prefix(core_clk, clk_name))
			core_clk_count++;

		if (dp_parser_check_prefix(ctrl_clk, clk_name))
			ctrl_clk_count++;
	}

	/* Initialize the CORE power module */
	if (core_clk_count <= 0) {
		pr_err("no core clocks are defined\n");
		rc = -EINVAL;
		goto exit;
	}

	core_power->num_clk = core_clk_count;
	core_power->clk_config = devm_kzalloc(dev,
			sizeof(struct dss_clk) * core_power->num_clk,
			GFP_KERNEL);
	if (!core_power->clk_config) {
		rc = -EINVAL;
		goto exit;
	}

	/* Initialize the CTRL power module */
	if (ctrl_clk_count <= 0) {
		pr_err("no ctrl clocks are defined\n");
		rc = -EINVAL;
		goto ctrl_clock_error;
	}

	ctrl_power->num_clk = ctrl_clk_count;
	ctrl_power->clk_config = devm_kzalloc(dev,
			sizeof(struct dss_clk) * ctrl_power->num_clk,
			GFP_KERNEL);
	if (!ctrl_power->clk_config) {
		ctrl_power->num_clk = 0;
		rc = -EINVAL;
		goto ctrl_clock_error;
	}

	return rc;

ctrl_clock_error:
	dp_parser_put_clk_data(dev, core_power);
exit:
	return rc;
}

static int dp_parser_clock(struct dp_parser *parser)
{
	int rc = 0, i = 0;
	int num_clk = 0;
	int core_clk_index = 0, ctrl_clk_index = 0;
	int core_clk_count = 0, ctrl_clk_count = 0;
	const char *clk_name;
	const char *core_clk = "core";
	const char *ctrl_clk = "ctrl";
	struct device *dev = &parser->pdev->dev;
	struct dss_module_power *core_power = &parser->mp[DP_CORE_PM];
	struct dss_module_power *ctrl_power = &parser->mp[DP_CTRL_PM];

	core_power = &parser->mp[DP_CORE_PM];
	ctrl_power = &parser->mp[DP_CTRL_PM];

	rc =  dp_parser_init_clk_data(parser);
	if (rc) {
		pr_err("failed to initialize power data\n");
		rc = -EINVAL;
		goto exit;
	}

	core_clk_count = core_power->num_clk;
	ctrl_clk_count = ctrl_power->num_clk;

	num_clk = core_clk_count + ctrl_clk_count;

	for (i = 0; i < num_clk; i++) {
		of_property_read_string_index(dev->of_node, "clock-names",
				i, &clk_name);

		if (dp_parser_check_prefix(core_clk, clk_name) &&
				core_clk_index < core_clk_count) {
			struct dss_clk *clk =
				&core_power->clk_config[core_clk_index];
			strlcpy(clk->clk_name, clk_name, sizeof(clk->clk_name));
			clk->type = DSS_CLK_AHB;
			core_clk_index++;
		} else if (dp_parser_check_prefix(ctrl_clk, clk_name) &&
			   ctrl_clk_index < ctrl_clk_count) {
			struct dss_clk *clk =
				&ctrl_power->clk_config[ctrl_clk_index];
			strlcpy(clk->clk_name, clk_name, sizeof(clk->clk_name));
			ctrl_clk_index++;

			if (!strcmp(clk_name, "ctrl_link_clk") ||
			    !strcmp(clk_name, "ctrl_pixel_clk"))
				clk->type = DSS_CLK_PCLK;
			else
				clk->type = DSS_CLK_AHB;
		}
	}

	pr_debug("clock parsing successful\n");

exit:
	return rc;
}

static int dp_parser_parse(struct dp_parser *parser)
{
	int rc = 0;

	if (!parser) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto err;
	}

	rc = dp_parser_reg(parser);
	if (rc)
		goto err;

	rc = dp_parser_aux(parser);
	if (rc)
		goto err;

	rc = dp_parser_misc(parser);
	if (rc)
		goto err;

	rc = dp_parser_clock(parser);
	if (rc)
		goto err;

	rc = dp_parser_regulator(parser);
	if (rc)
		goto err;

	rc = dp_parser_gpio(parser);
	if (rc)
		goto err;

	rc = dp_parser_pinctrl(parser);
	if (rc)
		goto err;

	rc = dp_parser_msm_hdcp_dev(parser);
err:
	return rc;
}

static struct dp_io_data *dp_parser_get_io(struct dp_parser *dp_parser,
				char *name)
{
	int i = 0;
	struct dp_io *io;

	if (!dp_parser) {
		pr_err("invalid input\n");
		goto err;
	}

	io = &dp_parser->io;

	for (i = 0; i < io->len; i++) {
		struct dp_io_data *data = &io->data[i];

		if (!strcmp(data->name, name))
			return data;
	}
err:
	return NULL;
}

static void dp_parser_get_io_buf(struct dp_parser *dp_parser, char *name)
{
	int i = 0;
	struct dp_io *io;

	if (!dp_parser) {
		pr_err("invalid input\n");
		return;
	}

	io = &dp_parser->io;

	for (i = 0; i < io->len; i++) {
		struct dp_io_data *data = &io->data[i];

		if (!strcmp(data->name, name)) {
			if (!data->buf)
				data->buf = devm_kzalloc(&dp_parser->pdev->dev,
					data->io.len, GFP_KERNEL);
		}
	}
}

static void dp_parser_clear_io_buf(struct dp_parser *dp_parser)
{
	int i = 0;
	struct dp_io *io;

	if (!dp_parser) {
		pr_err("invalid input\n");
		return;
	}

	io = &dp_parser->io;

	for (i = 0; i < io->len; i++) {
		struct dp_io_data *data = &io->data[i];

		if (data->buf)
			devm_kfree(&dp_parser->pdev->dev, data->buf);

		data->buf = NULL;
	}
}

struct dp_parser *dp_parser_get(struct platform_device *pdev)
{
	struct dp_parser *parser;

	parser = devm_kzalloc(&pdev->dev, sizeof(*parser), GFP_KERNEL);
	if (!parser)
		return ERR_PTR(-ENOMEM);

	parser->parse = dp_parser_parse;
	parser->get_io = dp_parser_get_io;
	parser->get_io_buf = dp_parser_get_io_buf;
	parser->clear_io_buf = dp_parser_clear_io_buf;
	parser->pdev = pdev;

	return parser;
}

void dp_parser_put(struct dp_parser *parser)
{
	int i = 0;
	struct dss_module_power *power = NULL;

	if (!parser) {
		pr_err("invalid parser module\n");
		return;
	}

	power = parser->mp;

	for (i = 0; i < DP_MAX_PM; i++) {
		dp_parser_put_clk_data(&parser->pdev->dev, &power[i]);
		dp_parser_put_vreg_data(&parser->pdev->dev, &power[i]);
		dp_parser_put_gpio_data(&parser->pdev->dev, &power[i]);
	}

	dp_parser_clear_io_buf(parser);
	devm_kfree(&parser->pdev->dev, parser->io.data);
	devm_kfree(&parser->pdev->dev, parser);
}
