/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/of.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "dsi_pwr.h"

/*
 * dsi_pwr_parse_supply_node() - parse power supply node from root device node
 */
static int dsi_pwr_parse_supply_node(struct device_node *root,
				     struct dsi_regulator_info *regs)
{
	int rc = 0;
	int i = 0;
	u32 tmp = 0;
	struct device_node *node = NULL;

	for_each_child_of_node(root, node) {
		const char *st = NULL;

		rc = of_property_read_string(node, "qcom,supply-name", &st);
		if (rc) {
			pr_err("failed to read name, rc = %d\n", rc);
			goto error;
		}

		snprintf(regs->vregs[i].vreg_name,
			 ARRAY_SIZE(regs->vregs[i].vreg_name),
			 "%s", st);

		rc = of_property_read_u32(node, "qcom,supply-min-voltage",
					  &tmp);
		if (rc) {
			pr_err("failed to read min voltage, rc = %d\n", rc);
			goto error;
		}
		regs->vregs[i].min_voltage = tmp;

		rc = of_property_read_u32(node, "qcom,supply-max-voltage",
					  &tmp);
		if (rc) {
			pr_err("failed to read max voltage, rc = %d\n", rc);
			goto error;
		}
		regs->vregs[i].max_voltage = tmp;

		rc = of_property_read_u32(node, "qcom,supply-enable-load",
					  &tmp);
		if (rc) {
			pr_err("failed to read enable load, rc = %d\n", rc);
			goto error;
		}
		regs->vregs[i].enable_load = tmp;

		rc = of_property_read_u32(node, "qcom,supply-disable-load",
					  &tmp);
		if (rc) {
			pr_err("failed to read disable load, rc = %d\n", rc);
			goto error;
		}
		regs->vregs[i].disable_load = tmp;

		/* Optional values */
		rc = of_property_read_u32(node, "qcom,supply-pre-on-sleep",
					  &tmp);
		if (rc) {
			pr_debug("pre-on-sleep not specified\n");
			rc = 0;
		} else {
			regs->vregs[i].pre_on_sleep = tmp;
		}

		rc = of_property_read_u32(node, "qcom,supply-pre-off-sleep",
					  &tmp);
		if (rc) {
			pr_debug("pre-off-sleep not specified\n");
			rc = 0;
		} else {
			regs->vregs[i].pre_off_sleep = tmp;
		}

		rc = of_property_read_u32(node, "qcom,supply-post-on-sleep",
					  &tmp);
		if (rc) {
			pr_debug("post-on-sleep not specified\n");
			rc = 0;
		} else {
			regs->vregs[i].post_on_sleep = tmp;
		}

		rc = of_property_read_u32(node, "qcom,supply-post-off-sleep",
					  &tmp);
		if (rc) {
			pr_debug("post-off-sleep not specified\n");
			rc = 0;
		} else {
			regs->vregs[i].post_off_sleep = tmp;
		}

		pr_debug("[%s] minv=%d maxv=%d, en_load=%d, dis_load=%d\n",
			 regs->vregs[i].vreg_name,
			 regs->vregs[i].min_voltage,
			 regs->vregs[i].max_voltage,
			 regs->vregs[i].enable_load,
			 regs->vregs[i].disable_load);
		++i;
	}

error:
	return rc;
}

/**
 * dsi_pwr_enable_vregs() - enable/disable regulators
 */
static int dsi_pwr_enable_vregs(struct dsi_regulator_info *regs, bool enable)
{
	int rc = 0, i = 0;
	struct dsi_vreg *vreg;
	int num_of_v = 0;

	if (enable) {
		for (i = 0; i < regs->count; i++) {
			vreg = &regs->vregs[i];
			if (vreg->pre_on_sleep)
				usleep_range(vreg->pre_on_sleep * 1000,
						vreg->pre_on_sleep * 1000);

			rc = regulator_set_load(vreg->vreg,
						vreg->enable_load);
			if (rc < 0) {
				pr_err("Setting optimum mode failed for %s\n",
				       vreg->vreg_name);
				goto error;
			}
			num_of_v = regulator_count_voltages(vreg->vreg);
			if (num_of_v > 0) {
				rc = regulator_set_voltage(vreg->vreg,
							   vreg->min_voltage,
							   vreg->max_voltage);
				if (rc) {
					pr_err("Set voltage(%s) fail, rc=%d\n",
						 vreg->vreg_name, rc);
					goto error_disable_opt_mode;
				}
			}

			rc = regulator_enable(vreg->vreg);
			if (rc) {
				pr_err("enable failed for %s, rc=%d\n",
				       vreg->vreg_name, rc);
				goto error_disable_voltage;
			}

			if (vreg->post_on_sleep)
				usleep_range(vreg->post_on_sleep * 1000,
						vreg->post_on_sleep * 1000);
		}
	} else {
		for (i = (regs->count - 1); i >= 0; i--) {
			if (regs->vregs[i].pre_off_sleep)
				usleep_range(regs->vregs[i].pre_off_sleep * 1000,
						regs->vregs[i].pre_off_sleep * 1000);

			(void)regulator_set_load(regs->vregs[i].vreg,
						regs->vregs[i].disable_load);
			(void)regulator_disable(regs->vregs[i].vreg);

			if (regs->vregs[i].post_off_sleep)
				usleep_range(regs->vregs[i].post_off_sleep * 1000,
						regs->vregs[i].post_off_sleep * 1000);
		}
	}

	return 0;
error_disable_opt_mode:
	(void)regulator_set_load(regs->vregs[i].vreg,
				 regs->vregs[i].disable_load);

error_disable_voltage:
	if (num_of_v > 0)
		(void)regulator_set_voltage(regs->vregs[i].vreg,
					    0, regs->vregs[i].max_voltage);
error:
	for (i--; i >= 0; i--) {
		if (regs->vregs[i].pre_off_sleep)
			usleep_range(regs->vregs[i].pre_off_sleep * 1000,
						regs->vregs[i].pre_off_sleep * 1000);

		(void)regulator_set_load(regs->vregs[i].vreg,
					 regs->vregs[i].disable_load);

		num_of_v = regulator_count_voltages(regs->vregs[i].vreg);
		if (num_of_v > 0)
			(void)regulator_set_voltage(regs->vregs[i].vreg,
					    0, regs->vregs[i].max_voltage);

		(void)regulator_disable(regs->vregs[i].vreg);

		if (regs->vregs[i].post_off_sleep)
			usleep_range(regs->vregs[i].post_off_sleep * 1000,
						regs->vregs[i].post_off_sleep * 1000);
	}

	return rc;
}

/**
* dsi_pwr_of_get_vreg_data - Parse regulator supply information
* @of_node:        Device of node to parse for supply information.
* @regs:           Pointer where regulator information will be copied to.
* @supply_name:    Name of the supply node.
*
* return: error code in case of failure or 0 for success.
*/
int dsi_pwr_of_get_vreg_data(struct device_node *of_node,
				 struct dsi_regulator_info *regs,
				 char *supply_name)
{
	int rc = 0;
	struct device_node *supply_root_node = NULL;

	if (!of_node || !regs) {
		pr_err("Bad params\n");
		return -EINVAL;
	}

	regs->count = 0;
	supply_root_node = of_get_child_by_name(of_node, supply_name);
	if (!supply_root_node) {
		supply_root_node = of_parse_phandle(of_node, supply_name, 0);
		if (!supply_root_node) {
			pr_debug("No supply entry present for %s\n",
					supply_name);
			return -EINVAL;
		}
	}

	regs->count = of_get_available_child_count(supply_root_node);
	if (regs->count == 0) {
		pr_err("No vregs defined for %s\n", supply_name);
		return -EINVAL;
	}

	regs->vregs = kcalloc(regs->count, sizeof(*regs->vregs), GFP_KERNEL);
	if (!regs->vregs) {
		regs->count = 0;
		return -ENOMEM;
	}

	rc = dsi_pwr_parse_supply_node(supply_root_node, regs);
	if (rc) {
		pr_err("failed to parse supply node for %s, rc = %d\n",
			supply_name, rc);

		kfree(regs->vregs);
		regs->vregs = NULL;
		regs->count = 0;
	}

	return rc;
}

/**
 * dsi_pwr_get_dt_vreg_data - parse regulator supply information
 * @dev:            Device whose of_node needs to be parsed.
 * @regs:           Pointer where regulator information will be copied to.
 * @supply_name:    Name of the supply node.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_pwr_get_dt_vreg_data(struct device *dev,
				 struct dsi_regulator_info *regs,
				 char *supply_name)
{
	int rc = 0;
	struct device_node *of_node = NULL;
	struct device_node *supply_node = NULL;
	struct device_node *supply_root_node = NULL;

	if (!dev || !regs) {
		pr_err("Bad params\n");
		return -EINVAL;
	}

	of_node = dev->of_node;
	regs->count = 0;
	supply_root_node = of_get_child_by_name(of_node, supply_name);
	if (!supply_root_node) {
		supply_root_node = of_parse_phandle(of_node, supply_name, 0);
		if (!supply_root_node) {
			pr_debug("No supply entry present for %s\n",
					supply_name);
			return -EINVAL;
		}
	}

	for_each_child_of_node(supply_root_node, supply_node)
		regs->count++;

	if (regs->count == 0) {
		pr_err("No vregs defined for %s\n", supply_name);
		return -EINVAL;
	}

	regs->vregs = devm_kcalloc(dev, regs->count, sizeof(*regs->vregs),
				   GFP_KERNEL);
	if (!regs->vregs) {
		regs->count = 0;
		return -ENOMEM;
	}

	rc = dsi_pwr_parse_supply_node(supply_root_node, regs);
	if (rc) {
		pr_err("failed to parse supply node for %s, rc = %d\n",
		       supply_name, rc);
		devm_kfree(dev, regs->vregs);
		regs->vregs = NULL;
		regs->count = 0;
	}

	return rc;
}

/**
 * dsi_pwr_enable_regulator() - enable a set of regulators
 * @regs:       Pointer to set of regulators to enable or disable.
 * @enable:     Enable/Disable regulators.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_pwr_enable_regulator(struct dsi_regulator_info *regs, bool enable)
{
	int rc = 0;

	if (!regs->vregs) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (enable) {
		if (regs->refcount == 0) {
			rc = dsi_pwr_enable_vregs(regs, true);
			if (rc)
				pr_err("failed to enable regulators\n");
		}
		regs->refcount++;
	} else {
		if (regs->refcount == 0) {
			pr_err("Unbalanced regulator off:%s\n",
					regs->vregs->vreg_name);
		} else {
			regs->refcount--;
			if (regs->refcount == 0) {
				rc = dsi_pwr_enable_vregs(regs, false);
				if (rc)
					pr_err("failed to disable vregs\n");
			}
		}
	}

	return rc;
}
