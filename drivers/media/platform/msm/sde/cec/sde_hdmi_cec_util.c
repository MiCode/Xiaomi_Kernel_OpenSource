/* Copyright (c) 2012, 2015-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>

#include "sde_hdmi_cec_util.h"

void sde_hdmi_cec_reg_w(struct cec_io_data *io,
	u32 offset, u32 value, bool debug)
{
	u32 in_val;

	if (!io || !io->base) {
		pr_err("invalid input\n");
		return;
	}

	if (offset > io->len) {
		pr_err("offset out of range\n");
		return;
	}

	writel_relaxed(value, io->base + offset);
	if (debug) {
		in_val = readl_relaxed(io->base + offset);
		pr_debug("[%08x] => %08x [%08x]\n",
			(u32)(unsigned long)(io->base + offset),
			value, in_val);
	}
}

u32 sde_hdmi_cec_reg_r(struct cec_io_data *io, u32 offset, bool debug)
{
	u32 value;

	if (!io || !io->base) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	if (offset > io->len) {
		pr_err("offset out of range\n");
		return -EINVAL;
	}

	value = readl_relaxed(io->base + offset);
	if (debug)
		pr_debug("[%08x] <= %08x\n",
			(u32)(unsigned long)(io->base + offset), value);

	return value;
}

void sde_hdmi_cec_reg_dump(void __iomem *base, u32 length, const char *prefix,
	bool debug)
{
	if (debug)
		print_hex_dump(KERN_INFO, prefix, DUMP_PREFIX_OFFSET, 32, 4,
			__io_virt(base), length, false);
}

static int sde_hdmi_cec_config_vreg(struct device *dev,
	struct cec_vreg *in_vreg, int num_vreg, bool config)
{
	int i = 0, rc = 0;
	struct cec_vreg *curr_vreg = NULL;
	enum cec_vreg_type type;

	if (!in_vreg || !num_vreg)
		return rc;

	if (config) {
		for (i = 0; i < num_vreg; i++) {
			curr_vreg = &in_vreg[i];
			curr_vreg->vreg = regulator_get(dev,
				curr_vreg->vreg_name);
			rc = PTR_RET(curr_vreg->vreg);
			if (rc) {
				pr_err("%s get failed. rc=%d\n",
					 curr_vreg->vreg_name, rc);
				curr_vreg->vreg = NULL;
				goto vreg_get_fail;
			}
			type = (regulator_count_voltages(curr_vreg->vreg) > 0)
					? CEC_REG_LDO : CEC_REG_VS;
			if (type == CEC_REG_LDO) {
				rc = regulator_set_voltage(
					curr_vreg->vreg,
					curr_vreg->min_voltage,
					curr_vreg->max_voltage);
				if (rc < 0) {
					pr_err("%s set vltg fail\n",
						curr_vreg->vreg_name);
					goto vreg_set_voltage_fail;
				}
			}
		}
	} else {
		for (i = num_vreg-1; i >= 0; i--) {
			curr_vreg = &in_vreg[i];
			if (curr_vreg->vreg) {
				type = (regulator_count_voltages(
					curr_vreg->vreg) > 0)
					? CEC_REG_LDO : CEC_REG_VS;
				if (type == CEC_REG_LDO) {
					regulator_set_voltage(curr_vreg->vreg,
						0, curr_vreg->max_voltage);
				}
				regulator_put(curr_vreg->vreg);
				curr_vreg->vreg = NULL;
			}
		}
	}
	return 0;

vreg_unconfig:
if (type == CEC_REG_LDO)
	regulator_set_load(curr_vreg->vreg, 0);

vreg_set_voltage_fail:
	regulator_put(curr_vreg->vreg);
	curr_vreg->vreg = NULL;

vreg_get_fail:
	for (i--; i >= 0; i--) {
		curr_vreg = &in_vreg[i];
		type = (regulator_count_voltages(curr_vreg->vreg) > 0)
			? CEC_REG_LDO : CEC_REG_VS;
		goto vreg_unconfig;
	}
	return rc;
}

static int sde_hdmi_cec_enable_vreg(struct cec_hw_resource *hw, int enable)
{
	int i = 0, rc = 0;
	bool need_sleep;
	struct cec_vreg *in_vreg = hw->vreg_config;
	int num_vreg = hw->num_vreg;

	if (enable) {
		for (i = 0; i < num_vreg; i++) {
			rc = PTR_RET(in_vreg[i].vreg);
			if (rc) {
				pr_err("%s regulator error. rc=%d\n",
					in_vreg[i].vreg_name, rc);
				goto vreg_set_opt_mode_fail;
			}
			need_sleep = !regulator_is_enabled(in_vreg[i].vreg);
			if (in_vreg[i].pre_on_sleep && need_sleep)
				usleep_range(in_vreg[i].pre_on_sleep * 1000,
					in_vreg[i].pre_on_sleep * 1000);
			rc = regulator_set_load(in_vreg[i].vreg,
				in_vreg[i].enable_load);
			if (rc < 0) {
				pr_err("%s set opt m fail\n",
					in_vreg[i].vreg_name);
				goto vreg_set_opt_mode_fail;
			}
			rc = regulator_enable(in_vreg[i].vreg);
			if (in_vreg[i].post_on_sleep && need_sleep)
				usleep_range(in_vreg[i].post_on_sleep * 1000,
					in_vreg[i].post_on_sleep * 1000);
			if (rc < 0) {
				pr_err("%s enable failed\n",
					in_vreg[i].vreg_name);
				goto disable_vreg;
			}
		}
	} else {
		for (i = num_vreg-1; i >= 0; i--) {
			if (in_vreg[i].pre_off_sleep)
				usleep_range(in_vreg[i].pre_off_sleep * 1000,
					in_vreg[i].pre_off_sleep * 1000);
			regulator_set_load(in_vreg[i].vreg,
				in_vreg[i].disable_load);
			regulator_disable(in_vreg[i].vreg);
			if (in_vreg[i].post_off_sleep)
				usleep_range(in_vreg[i].post_off_sleep * 1000,
					in_vreg[i].post_off_sleep * 1000);
		}
	}
	return rc;

disable_vreg:
	regulator_set_load(in_vreg[i].vreg, in_vreg[i].disable_load);

vreg_set_opt_mode_fail:
	for (i--; i >= 0; i--) {
		if (in_vreg[i].pre_off_sleep)
			usleep_range(in_vreg[i].pre_off_sleep * 1000,
				in_vreg[i].pre_off_sleep * 1000);
		regulator_set_load(in_vreg[i].vreg,
			in_vreg[i].disable_load);
		regulator_disable(in_vreg[i].vreg);
		if (in_vreg[i].post_off_sleep)
			usleep_range(in_vreg[i].post_off_sleep * 1000,
				in_vreg[i].post_off_sleep * 1000);
	}

	return rc;
}

static void sde_hdmi_cec_put_clk(struct cec_clk *clk_arry, int num_clk)
{
	int i;

	for (i = num_clk - 1; i >= 0; i--) {
		if (clk_arry[i].clk)
			clk_put(clk_arry[i].clk);
		clk_arry[i].clk = NULL;
	}
}

static int sde_hdmi_cec_get_clk(struct device *dev,
	struct cec_clk *clk_arry, int num_clk)
{
	int i, rc = 0;

	for (i = 0; i < num_clk; i++) {
		clk_arry[i].clk = clk_get(dev, clk_arry[i].clk_name);
		rc = PTR_RET(clk_arry[i].clk);
		if (rc) {
			pr_err("'%s' get failed. rc=%d\n",
				clk_arry[i].clk_name, rc);
			goto error;
		}
	}

	return rc;

error:
	sde_hdmi_cec_put_clk(clk_arry, num_clk);

	return rc;
}

static int sde_hdmi_cec_enable_clk(struct cec_hw_resource *hw, int enable)
{
	int i, rc = 0;
	struct cec_clk *clk_arry = hw->clk_config;
	int num_clk = hw->num_clk;

	if (enable) {
		for (i = 0; i < num_clk; i++) {
			pr_debug("enable %s\n", clk_arry[i].clk_name);
			if (clk_arry[i].clk) {
				rc = clk_prepare_enable(clk_arry[i].clk);
				if (rc)
					pr_err("%s enable fail. rc=%d\n",
						clk_arry[i].clk_name, rc);
			} else {
				pr_err("%s is not available\n",
					clk_arry[i].clk_name);
				rc = -EPERM;
			}
		}
	} else {
		for (i = num_clk - 1; i >= 0; i--) {
			pr_debug("disable %s\n", clk_arry[i].clk_name);

			if (clk_arry[i].clk)
				clk_disable_unprepare(clk_arry[i].clk);
			else
				pr_err("%s is not available\n",
					clk_arry[i].clk_name);
		}
	}

	return rc;
}

static int sde_hdmi_cec_pinctrl_enable(struct cec_hw_resource *hw,
	bool enable)
{
	struct pinctrl_state *pin_state = NULL;
	int rc = 0;

	if (!hw) {
		pr_err("invalid input param hw:%pK\n", hw);
		return -EINVAL;
	}

	pr_debug("set cec pinctrl state %d\n", enable);

	pin_state = enable ? hw->pin_res.state_active : hw->pin_res.state_sleep;

	if (!IS_ERR_OR_NULL(hw->pin_res.pinctrl))
		rc = pinctrl_select_state(hw->pin_res.pinctrl,
			pin_state);
	else
		pr_err("pinstate not found\n");

	return rc;
}

static void sde_hdmi_cec_put_dt_clock(struct platform_device *pdev,
					struct cec_hw_resource *hw)
{
	if (!pdev || !hw) {
		pr_err("invalid input param pdev:%pK hw:%pK\n", pdev, hw);
		return;
	}

	if (hw->clk_config) {
		sde_hdmi_cec_put_clk(hw->clk_config, hw->num_clk);
		devm_kfree(&pdev->dev, hw->clk_config);
		hw->clk_config = NULL;
	}
	hw->num_clk = 0;

	pr_debug("put dt clock\n");
}

static int sde_hdmi_cec_get_dt_clock(struct platform_device *pdev,
					struct cec_hw_resource *hw)
{
	int i = 0;
	int num_clk = 0;
	const char *clock_name;
	int rc = 0;

	if (!pdev || !hw) {
		pr_err("invalid input param pdev:%pK hw:%pK\n", pdev, hw);
		return -EINVAL;
	}

	hw->num_clk = 0;
	num_clk = of_property_count_strings(pdev->dev.of_node, "clock-names");
	if (num_clk <= 0) {
		pr_debug("clocks are not defined\n");
		return 0;
	}

	hw->num_clk = num_clk;
	hw->clk_config = devm_kzalloc(&pdev->dev,
			sizeof(struct cec_clk) * num_clk, GFP_KERNEL);
	if (!hw->clk_config) {
		hw->num_clk = 0;
		return -ENOMEM;
	}

	for (i = 0; i < num_clk; i++) {
		of_property_read_string_index(pdev->dev.of_node, "clock-names",
							i, &clock_name);
		strlcpy(hw->clk_config[i].clk_name, clock_name,
				sizeof(hw->clk_config[i].clk_name));
	}

	rc = sde_hdmi_cec_get_clk(&pdev->dev, hw->clk_config, hw->num_clk);
	if (rc) {
		sde_hdmi_cec_put_dt_clock(pdev, hw);
		return rc;
	}

	pr_debug("get dt clock\n");

	return 0;
}

static int sde_hdmi_cec_get_dt_supply(struct platform_device *pdev,
				struct cec_hw_resource *hw)
{
	int i = 0, rc = 0;
	u32 tmp = 0;
	struct device_node *of_node = NULL, *supply_root_node = NULL;
	struct device_node *supply_node = NULL;

	if (!pdev || !hw) {
		pr_err("invalid input param pdev:%pK hw:%pK\n", pdev, hw);
		return -EINVAL;
	}

	of_node = pdev->dev.of_node;

	hw->num_vreg = 0;
	supply_root_node = of_get_child_by_name(of_node,
						"qcom,platform-supply-entries");
	if (!supply_root_node) {
		pr_debug("no supply entry present\n");
		return rc;
	}

	hw->num_vreg = of_get_available_child_count(supply_root_node);
	if (hw->num_vreg == 0) {
		pr_debug("no vreg present\n");
		return rc;
	}

	pr_debug("vreg found. count=%d\n", hw->num_vreg);
	hw->vreg_config = devm_kzalloc(&pdev->dev, sizeof(struct cec_vreg) *
						hw->num_vreg, GFP_KERNEL);
	if (!hw->vreg_config) {
		rc = -ENOMEM;
		return rc;
	}

	for_each_available_child_of_node(supply_root_node, supply_node) {
		const char *st = NULL;

		rc = of_property_read_string(supply_node,
						"qcom,supply-name", &st);
		if (rc) {
			pr_err("error reading name. rc=%d\n", rc);
			goto error;
		}

		strlcpy(hw->vreg_config[i].vreg_name, st,
					sizeof(hw->vreg_config[i].vreg_name));

		rc = of_property_read_u32(supply_node,
					"qcom,supply-min-voltage", &tmp);
		if (rc) {
			pr_err("error reading min volt. rc=%d\n", rc);
			goto error;
		}
		hw->vreg_config[i].min_voltage = tmp;

		rc = of_property_read_u32(supply_node,
					"qcom,supply-max-voltage", &tmp);
		if (rc) {
			pr_err("error reading max volt. rc=%d\n", rc);
			goto error;
		}
		hw->vreg_config[i].max_voltage = tmp;

		rc = of_property_read_u32(supply_node,
					"qcom,supply-enable-load", &tmp);
		if (rc) {
			pr_err("error reading enable load. rc=%d\n", rc);
			goto error;
		}
		hw->vreg_config[i].enable_load = tmp;

		rc = of_property_read_u32(supply_node,
					"qcom,supply-disable-load", &tmp);
		if (rc) {
			pr_err("error reading disable load. rc=%d\n", rc);
			goto error;
		}
		hw->vreg_config[i].disable_load = tmp;

		rc = of_property_read_u32(supply_node,
					"qcom,supply-pre-on-sleep", &tmp);
		if (rc)
			pr_debug("no supply pre sleep value. rc=%d\n", rc);

		hw->vreg_config[i].pre_on_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
					"qcom,supply-pre-off-sleep", &tmp);
		if (rc)
			pr_debug("no supply pre sleep value. rc=%d\n", rc);

		hw->vreg_config[i].pre_off_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
					"qcom,supply-post-on-sleep", &tmp);
		if (rc)
			pr_debug("no supply post sleep value. rc=%d\n", rc);

		hw->vreg_config[i].post_on_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
					"qcom,supply-post-off-sleep", &tmp);
		if (rc)
			pr_debug("no supply post sleep value. rc=%d\n", rc);

		hw->vreg_config[i].post_off_sleep = (!rc ? tmp : 0);

		pr_debug("%s min=%d, max=%d, enable=%d, disable=%d, preonsleep=%d, postonsleep=%d, preoffsleep=%d, postoffsleep=%d\n",
					hw->vreg_config[i].vreg_name,
					hw->vreg_config[i].min_voltage,
					hw->vreg_config[i].max_voltage,
					hw->vreg_config[i].enable_load,
					hw->vreg_config[i].disable_load,
					hw->vreg_config[i].pre_on_sleep,
					hw->vreg_config[i].post_on_sleep,
					hw->vreg_config[i].pre_off_sleep,
					hw->vreg_config[i].post_off_sleep);
		++i;

		rc = 0;
	}

	rc = sde_hdmi_cec_config_vreg(&pdev->dev,
		hw->vreg_config, hw->num_vreg, true);
	if (rc)
		goto error;

	pr_debug("get dt supply\n");

	return rc;

error:
	if (hw->vreg_config) {
		devm_kfree(&pdev->dev, hw->vreg_config);
		hw->vreg_config = NULL;
		hw->num_vreg = 0;
	}

	return rc;
}

static void sde_hdmi_cec_put_dt_supply(struct platform_device *pdev,
				struct cec_hw_resource *hw)
{
	if (!pdev || !hw) {
		pr_err("invalid input param pdev:%pK hw:%pK\n", pdev, hw);
		return;
	}

	sde_hdmi_cec_config_vreg(&pdev->dev,
		hw->vreg_config, hw->num_vreg, false);

	if (hw->vreg_config) {
		devm_kfree(&pdev->dev, hw->vreg_config);
		hw->vreg_config = NULL;
	}
	hw->num_vreg = 0;

	pr_debug("put dt supply\n");
}

static int sde_hdmi_cec_get_dt_pinres(struct platform_device *pdev,
				struct cec_hw_resource *hw)
{
	if (!pdev || !hw) {
		pr_err("invalid input param pdev:%pK hw:%pK\n", pdev, hw);
		return -EINVAL;
	}

	hw->pin_res.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(hw->pin_res.pinctrl)) {
		pr_err("failed to get pinctrl\n");
		return PTR_ERR(hw->pin_res.pinctrl);
	}

	hw->pin_res.state_active =
		pinctrl_lookup_state(hw->pin_res.pinctrl, "cec_active");
	if (IS_ERR_OR_NULL(hw->pin_res.state_active))
		pr_debug("cannot get active pinstate\n");

	hw->pin_res.state_sleep =
		pinctrl_lookup_state(hw->pin_res.pinctrl, "cec_sleep");
	if (IS_ERR_OR_NULL(hw->pin_res.state_sleep))
		pr_debug("cannot get sleep pinstate\n");

	pr_debug("get dt pinres data\n");

	return 0;
}

static void sde_hdmi_cec_put_dt_pinres(struct platform_device *pdev,
				struct cec_hw_resource *hw)
{
	if (!pdev || !hw) {
		pr_err("invalid input param pdev:%pK hw:%pK\n", pdev, hw);
		return;
	}

	if (!IS_ERR_OR_NULL(hw->pin_res.pinctrl))
		devm_pinctrl_put(hw->pin_res.pinctrl);
}

static void sde_hdmi_cec_deinit_power(struct platform_device *pdev,
	struct cec_hw_resource *hw)
{
	if (!pdev || !hw) {
		pr_err("invalid input param pdev:%pK hw:%pK\n", pdev, hw);
		return;
	}

	sde_hdmi_cec_put_dt_supply(pdev, hw);
	sde_hdmi_cec_put_dt_clock(pdev, hw);
	sde_hdmi_cec_put_dt_pinres(pdev, hw);

	pr_debug("put dt power data\n");
}

static int sde_hdmi_cec_init_power(struct platform_device *pdev,
	struct cec_hw_resource *hw)
{
	int rc = 0;

	if (!pdev || !hw) {
		pr_err("invalid input param pdev:%pK hw:%pK\n", pdev, hw);
		return -EINVAL;
	}

	/* VREG */
	rc = sde_hdmi_cec_get_dt_supply(pdev, hw);
	if (rc) {
		pr_err("get_dt_supply failed. rc=%d\n", rc);
		goto error;
	}

	/* Clock */
	rc = sde_hdmi_cec_get_dt_clock(pdev, hw);
	if (rc) {
		pr_err("get_dt_clock failed. rc=%d\n", rc);
		goto error;
	}

	/* Pinctrl */
	rc = sde_hdmi_cec_get_dt_pinres(pdev, hw);
	if (rc) {
		pr_err("get_dt_pinres failed. rc=%d\n", rc);
		goto error;
	}

	pr_debug("get dt power data\n");

	return rc;

error:
	sde_hdmi_cec_deinit_power(pdev, hw);
	return rc;
}

static int sde_hdmi_cec_init_io(struct platform_device *pdev,
	struct cec_hw_resource *hw)
{
	struct resource *res = NULL;
	struct cec_io_data *io_data = NULL;
	const char *reg_name;

	if (!pdev || !hw) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	if (of_property_read_string(pdev->dev.of_node, "reg-names",
			&reg_name)) {
		pr_err("cec reg not defined\n");
		return -ENODEV;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, reg_name);
	if (!res) {
		pr_err("%s get_res_byname failed\n", reg_name);
		return -ENODEV;
	}

	io_data = &hw->io_res;
	io_data->len = (u32)resource_size(res);
	io_data->base = ioremap(res->start, io_data->len);
	if (!io_data->base) {
		pr_err("%s ioremap failed\n", reg_name);
		return -EIO;
	}

	return 0;
}

static void sde_hdmi_cec_deinit_io(struct platform_device *pdev,
	struct cec_hw_resource *hw)
{
	struct cec_io_data *io_data = NULL;

	if (!pdev || !hw) {
		pr_err("invalid input\n");
		return;
	}

	io_data = &hw->io_res;

	if (io_data->base) {
		iounmap(io_data->base);
		io_data->base = NULL;
	}
	io_data->len = 0;
}

int sde_hdmi_cec_init_resource(struct platform_device *pdev,
	struct cec_hw_resource *hw)
{
	int rc = 0;

	/* power */
	rc = sde_hdmi_cec_init_power(pdev, hw);
	if (rc)
		return rc;

	/* io */
	rc = sde_hdmi_cec_init_io(pdev, hw);
	if (rc)
		goto io_error;

	pr_debug("cec init resource\n");

	return rc;

io_error:
	sde_hdmi_cec_deinit_power(pdev, hw);
	return rc;
}

void sde_hdmi_cec_deinit_resource(struct platform_device *pdev,
	struct cec_hw_resource *hw)
{
	sde_hdmi_cec_deinit_power(pdev, hw);
	sde_hdmi_cec_deinit_io(pdev, hw);

	pr_debug("cec deinit resource\n");
}

int sde_hdmi_cec_enable_power(struct cec_hw_resource *hw, bool enable)
{
	int rc = 0;

	rc = sde_hdmi_cec_enable_vreg(hw, enable);
	if (rc)
		return rc;

	rc = sde_hdmi_cec_pinctrl_enable(hw, enable);
	if (rc)
		return rc;

	rc = sde_hdmi_cec_enable_clk(hw, enable);
	if (rc)
		return rc;

	pr_debug("cec power enable = %d\n", enable);

	return rc;
}
