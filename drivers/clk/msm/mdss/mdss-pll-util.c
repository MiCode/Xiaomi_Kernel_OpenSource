/* Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/clk/msm-clock-generic.h>
#include <linux/vmalloc.h>
#include <linux/memblock.h>

#include "mdss-pll.h"

int mdss_pll_util_resource_init(struct platform_device *pdev,
					struct mdss_pll_resources *pll_res)
{
	int rc = 0;
	struct dss_module_power *mp = &pll_res->mp;

	rc = msm_dss_config_vreg(&pdev->dev,
				mp->vreg_config, mp->num_vreg, 1);
	if (rc) {
		pr_err("Vreg config failed rc=%d\n", rc);
		goto vreg_err;
	}

	rc = msm_dss_get_clk(&pdev->dev, mp->clk_config, mp->num_clk);
	if (rc) {
		pr_err("Clock get failed rc=%d\n", rc);
		goto clk_err;
	}

	return rc;

clk_err:
	msm_dss_config_vreg(&pdev->dev, mp->vreg_config, mp->num_vreg, 0);
vreg_err:
	return rc;
}

/**
 * mdss_pll_get_mp_by_reg_name() -- Find power module by regulator name
 *@pll_res: Pointer to the PLL resource
 *@name: Regulator name as specified in the pll dtsi
 *
 * This is a helper function to retrieve the regulator information
 * for each pll resource.
 */
struct dss_vreg *mdss_pll_get_mp_by_reg_name(struct mdss_pll_resources *pll_res
		, char *name)
{

	struct dss_vreg *regulator = NULL;
	int i;

	if ((pll_res == NULL) || (pll_res->mp.vreg_config == NULL)) {
		pr_err("%s Invalid PLL resource\n", __func__);
		goto error;
	}

	regulator = pll_res->mp.vreg_config;

	for (i = 0; i < pll_res->mp.num_vreg; i++) {
		if (!strcmp(name, regulator->vreg_name)) {
			pr_debug("Found regulator match for %s\n", name);
			break;
		}
		regulator++;
	}

error:
	return regulator;
}

void mdss_pll_util_resource_deinit(struct platform_device *pdev,
					 struct mdss_pll_resources *pll_res)
{
	struct dss_module_power *mp = &pll_res->mp;

	msm_dss_put_clk(mp->clk_config, mp->num_clk);

	msm_dss_config_vreg(&pdev->dev, mp->vreg_config, mp->num_vreg, 0);
}

void mdss_pll_util_resource_release(struct platform_device *pdev,
					struct mdss_pll_resources *pll_res)
{
	struct dss_module_power *mp = &pll_res->mp;

	devm_kfree(&pdev->dev, mp->clk_config);
	devm_kfree(&pdev->dev, mp->vreg_config);
	mp->num_vreg = 0;
	mp->num_clk = 0;
}

int mdss_pll_util_resource_enable(struct mdss_pll_resources *pll_res,
								bool enable)
{
	int rc = 0;
	struct dss_module_power *mp = &pll_res->mp;

	if (enable) {
		rc = msm_dss_enable_vreg(mp->vreg_config, mp->num_vreg, enable);
		if (rc) {
			pr_err("Failed to enable vregs rc=%d\n", rc);
			goto vreg_err;
		}

		rc = msm_dss_clk_set_rate(mp->clk_config, mp->num_clk);
		if (rc) {
			pr_err("Failed to set clock rate rc=%d\n", rc);
			goto clk_err;
		}

		rc = msm_dss_enable_clk(mp->clk_config, mp->num_clk, enable);
		if (rc) {
			pr_err("clock enable failed rc:%d\n", rc);
			goto clk_err;
		}
	} else {
		msm_dss_enable_clk(mp->clk_config, mp->num_clk, enable);

		msm_dss_enable_vreg(mp->vreg_config, mp->num_vreg, enable);
	}

	return rc;

clk_err:
	msm_dss_enable_vreg(mp->vreg_config, mp->num_vreg, 0);
vreg_err:
	return rc;
}

static int mdss_pll_util_parse_dt_supply(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int i = 0, rc = 0;
	u32 tmp = 0;
	struct device_node *of_node = NULL, *supply_root_node = NULL;
	struct device_node *supply_node = NULL;
	struct dss_module_power *mp = &pll_res->mp;

	of_node = pdev->dev.of_node;

	mp->num_vreg = 0;
	supply_root_node = of_get_child_by_name(of_node,
						"qcom,platform-supply-entries");
	if (!supply_root_node) {
		pr_err("no supply entry present\n");
		return rc;
	}

	for_each_child_of_node(supply_root_node, supply_node) {
		mp->num_vreg++;
	}

	if (mp->num_vreg == 0) {
		pr_debug("no vreg\n");
		return rc;
	}
	pr_debug("vreg found. count=%d\n", mp->num_vreg);

	mp->vreg_config = devm_kzalloc(&pdev->dev, sizeof(struct dss_vreg) *
						mp->num_vreg, GFP_KERNEL);
	if (!mp->vreg_config) {
		rc = -ENOMEM;
		return rc;
	}

	for_each_child_of_node(supply_root_node, supply_node) {

		const char *st = NULL;

		rc = of_property_read_string(supply_node,
						"qcom,supply-name", &st);
		if (rc) {
			pr_err(":error reading name. rc=%d\n", rc);
			goto error;
		}

		strlcpy(mp->vreg_config[i].vreg_name, st,
					sizeof(mp->vreg_config[i].vreg_name));

		rc = of_property_read_u32(supply_node,
					"qcom,supply-min-voltage", &tmp);
		if (rc) {
			pr_err(": error reading min volt. rc=%d\n", rc);
			goto error;
		}
		mp->vreg_config[i].min_voltage = tmp;

		rc = of_property_read_u32(supply_node,
					"qcom,supply-max-voltage", &tmp);
		if (rc) {
			pr_err(": error reading max volt. rc=%d\n", rc);
			goto error;
		}
		mp->vreg_config[i].max_voltage = tmp;

		rc = of_property_read_u32(supply_node,
					"qcom,supply-enable-load", &tmp);
		if (rc) {
			pr_err(": error reading enable load. rc=%d\n", rc);
			goto error;
		}
		mp->vreg_config[i].load[DSS_REG_MODE_ENABLE] = tmp;

		rc = of_property_read_u32(supply_node,
					"qcom,supply-disable-load", &tmp);
		if (rc) {
			pr_err(": error reading disable load. rc=%d\n", rc);
			goto error;
		}
		mp->vreg_config[i].load[DSS_REG_MODE_DISABLE] = tmp;

		rc = of_property_read_u32(supply_node,
					"qcom,supply-ulp-load", &tmp);
		if (rc)
			pr_warn(": error reading ulp load. rc=%d\n", rc);

		mp->vreg_config[i].load[DSS_REG_MODE_ULP] = (!rc ? tmp :
			mp->vreg_config[i].load[DSS_REG_MODE_ENABLE]);

		rc = of_property_read_u32(supply_node,
					"qcom,supply-pre-on-sleep", &tmp);
		if (rc)
			pr_debug("error reading supply pre sleep value. rc=%d\n",
							rc);

		mp->vreg_config[i].pre_on_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
					"qcom,supply-pre-off-sleep", &tmp);
		if (rc)
			pr_debug("error reading supply pre sleep value. rc=%d\n",
							rc);

		mp->vreg_config[i].pre_off_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
					"qcom,supply-post-on-sleep", &tmp);
		if (rc)
			pr_debug("error reading supply post sleep value. rc=%d\n",
							rc);

		mp->vreg_config[i].post_on_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
					"qcom,supply-post-off-sleep", &tmp);
		if (rc)
			pr_debug("error reading supply post sleep value. rc=%d\n",
							rc);

		mp->vreg_config[i].post_off_sleep = (!rc ? tmp : 0);

		pr_debug("%s min=%d, max=%d, enable=%d, disable=%d, ulp=%d, preonsleep=%d, postonsleep=%d, preoffsleep=%d, postoffsleep=%d\n",
			mp->vreg_config[i].vreg_name,
			mp->vreg_config[i].min_voltage,
			mp->vreg_config[i].max_voltage,
			mp->vreg_config[i].load[DSS_REG_MODE_ENABLE],
			mp->vreg_config[i].load[DSS_REG_MODE_DISABLE],
			mp->vreg_config[i].load[DSS_REG_MODE_ULP],
			mp->vreg_config[i].pre_on_sleep,
			mp->vreg_config[i].post_on_sleep,
			mp->vreg_config[i].pre_off_sleep,
			mp->vreg_config[i].post_off_sleep);
		++i;

		rc = 0;
	}

	return rc;

error:
	if (mp->vreg_config) {
		devm_kfree(&pdev->dev, mp->vreg_config);
		mp->vreg_config = NULL;
		mp->num_vreg = 0;
	}

	return rc;
}

static int mdss_pll_util_parse_dt_clock(struct platform_device *pdev,
					struct mdss_pll_resources *pll_res)
{
	u32 i = 0, rc = 0;
	struct dss_module_power *mp = &pll_res->mp;
	const char *clock_name;
	u32 clock_rate;

	mp->num_clk = of_property_count_strings(pdev->dev.of_node,
							"clock-names");
	if (mp->num_clk <= 0) {
		pr_err("clocks are not defined\n");
		goto clk_err;
	}

	mp->clk_config = devm_kzalloc(&pdev->dev,
			sizeof(struct dss_clk) * mp->num_clk, GFP_KERNEL);
	if (!mp->clk_config) {
		rc = -ENOMEM;
		mp->num_clk = 0;
		goto clk_err;
	}

	for (i = 0; i < mp->num_clk; i++) {
		of_property_read_string_index(pdev->dev.of_node, "clock-names",
							i, &clock_name);
		strlcpy(mp->clk_config[i].clk_name, clock_name,
				sizeof(mp->clk_config[i].clk_name));

		of_property_read_u32_index(pdev->dev.of_node, "clock-rate",
							i, &clock_rate);
		mp->clk_config[i].rate = clock_rate;

		if (!clock_rate)
			mp->clk_config[i].type = DSS_CLK_AHB;
		else
			mp->clk_config[i].type = DSS_CLK_PCLK;
	}

clk_err:
	return rc;
}

int mdss_pll_util_resource_parse(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc = 0;
	struct dss_module_power *mp = &pll_res->mp;

	rc = mdss_pll_util_parse_dt_supply(pdev, pll_res);
	if (rc) {
		pr_err("vreg parsing failed rc=%d\n", rc);
		goto end;
	}

	rc = mdss_pll_util_parse_dt_clock(pdev, pll_res);
	if (rc) {
		pr_err("clock name parsing failed rc=%d", rc);
		goto clk_err;
	}

	return rc;

clk_err:
	devm_kfree(&pdev->dev, mp->vreg_config);
	mp->num_vreg = 0;
end:
	return rc;
}
